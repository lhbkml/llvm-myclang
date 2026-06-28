#include "visitor.h"

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Analysis/Analyses/CFGReachabilityAnalysis.h"
#include "clang/Analysis/CFG.h"

#include <set>

using namespace llvm;
using namespace clang;

// ====================== DiagCollector ======================

DiagCollector::DiagCollector(AnalysisResult &R) : Result(R) {}

void DiagCollector::HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                                     const clang::Diagnostic &Info) {
    if (DiagLevel == DiagnosticsEngine::Ignored ||
        DiagLevel == DiagnosticsEngine::Note)
        return;
    llvm::SmallString<256> Buf;
    Info.FormatDiagnostic(Buf);
    if (DiagLevel == DiagnosticsEngine::Error ||
        DiagLevel == DiagnosticsEngine::Fatal)
        Result.errors.push_back(std::string(Buf.str()));
    else
        Result.warnings.push_back(std::string(Buf.str()));
}

// ====================== MyASTVisitor ======================

MyASTVisitor::MyASTVisitor(AnalysisStats &S, const Thresholds &T) : Stats(S), Thresh(T) {}

bool MyASTVisitor::TraverseIfStmt(IfStmt *IS) {
    currentDepth++;
    if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
        CurrentFunc->maxNesting = currentDepth;
    bool r = RecursiveASTVisitor::TraverseIfStmt(IS);
    currentDepth--;
    return r;
}

bool MyASTVisitor::TraverseForStmt(ForStmt *FS) {
    currentDepth++;
    if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
        CurrentFunc->maxNesting = currentDepth;
    bool r = RecursiveASTVisitor::TraverseForStmt(FS);
    currentDepth--;
    return r;
}

bool MyASTVisitor::TraverseWhileStmt(WhileStmt *WS) {
    currentDepth++;
    if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
        CurrentFunc->maxNesting = currentDepth;
    bool r = RecursiveASTVisitor::TraverseWhileStmt(WS);
    currentDepth--;
    return r;
}

bool MyASTVisitor::TraverseDoStmt(DoStmt *DS) {
    currentDepth++;
    if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
        CurrentFunc->maxNesting = currentDepth;
    bool r = RecursiveASTVisitor::TraverseDoStmt(DS);
    currentDepth--;
    return r;
}

bool MyASTVisitor::TraverseSwitchStmt(SwitchStmt *SS) {
    currentDepth++;
    if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
        CurrentFunc->maxNesting = currentDepth;
    bool r = RecursiveASTVisitor::TraverseSwitchStmt(SS);
    currentDepth--;
    return r;
}

bool MyASTVisitor::isFromMainFile(Decl *D) {
    if (!SM) return true;
    return SM->isInMainFile(D->getLocation());
}

bool MyASTVisitor::isStmtFromMainFile(Stmt *S) {
    if (!SM) return true;
    return SM->isInMainFile(S->getBeginLoc());
}

void MyASTVisitor::checkEmptyBlock(Stmt *Body, const std::string &Type) {
    if (auto *CS = dyn_cast<CompoundStmt>(Body)) {
        if (CS->body_empty()) {
            Stats.emptyBlockCount++;
            Stats.emptyBlocks.push_back({
                CurrentFunc ? CurrentFunc->name : "<toplevel>",
                Type
            });
            if (CurrentFunc)
                CurrentFunc->emptyBlocks++;
        }
    }
}

bool MyASTVisitor::TraverseFunctionDecl(FunctionDecl *FD) {
    FunctionStats *Saved = CurrentFunc;
    FunctionStats FS;

    if (FD->isThisDeclarationADefinition() && isFromMainFile(FD)) {
        FS.name = FD->getNameAsString();

        // 计算函数体行数
        if (SM && FD->hasBody()) {
            Stmt *Body = FD->getBody();
            unsigned startLine = SM->getExpansionLineNumber(FD->getBeginLoc());
            unsigned endLine   = SM->getExpansionLineNumber(Body->getEndLoc());
            FS.lines = (endLine >= startLine) ? (endLine - startLine + 1) : 0;

            // 检测空函数：函数体为空 {} 或仅含一条 return
            if (auto *CS = dyn_cast<CompoundStmt>(Body)) {
                if (CS->body_empty()) {
                    FS.isEmptyOrReturnOnly = true;
                } else if (CS->size() == 1 &&
                           isa<ReturnStmt>(*CS->body_begin())) {
                    FS.isEmptyOrReturnOnly = true;
                }
            }
        }

        FS.paramCount = FD->getNumParams();

        if (FS.paramCount > Thresh.maxParams)
            FS.hasTooManyParams = true;

        if (FS.lines > Thresh.maxFunctionLines)
            FS.isOverlong = true;

        if (!isValidSnakeCase(FS.name))
            FS.isBadName = true;

        CurrentFunc = &FS;
    }

    bool result = RecursiveASTVisitor::TraverseFunctionDecl(FD);

    // 计算圈复杂度（基于控制流图）
    if (CurrentFunc == &FS && Ctx && FD->hasBody()) {
        CFG::BuildOptions opts;
        opts.AddEHEdges = false;
        opts.AddImplicitDtors = false;
        opts.AddStaticInitBranches = false;
        opts.PruneTriviallyFalseEdges = false;
        std::unique_ptr<CFG> cfg = CFG::buildCFG(FD, FD->getBody(), Ctx, opts);
        if (cfg) {
            int nodes = 0, edges = 0;
            for (auto it = cfg->begin(); it != cfg->end(); ++it) {
                nodes++;
                edges += (*it)->succ_size();
            }
            FS.ccn = edges - nodes + 2;
            if (FS.ccn < 1) FS.ccn = 1;
            if (FS.ccn > Thresh.maxCCN) {
                FS.isHighCCN = true;
                Stats.highCCNFunctions++;
            }

            // 检测死代码
            CFGReverseBlockReachabilityAnalysis reach(*cfg);
            const CFGBlock *entryBlock = &cfg->getEntry();
            for (auto it = cfg->begin(); it != cfg->end(); ++it) {
                const CFGBlock *B = *it;
                if (B == entryBlock) continue;
                if (!reach.isReachable(entryBlock, B)) {
                    for (const CFGElement &Elem : *B) {
                        if (Elem.getAs<CFGStmt>())
                            FS.deadStmtCount++;
                    }
                }
            }
            if (FS.deadStmtCount > 0) {
                FS.hasDeadCode = true;
                Stats.deadStmts++;
            }

            // 检测 switch 穿透
            {
                std::set<const CFGBlock*> caseBlocks, dispatchBlocks;
                for (auto ci = cfg->begin(); ci != cfg->end(); ++ci) {
                    if ((*ci)->getLabel() && isa<SwitchCase>((*ci)->getLabel()))
                        caseBlocks.insert(*ci);
                }
                for (auto ci = cfg->begin(); ci != cfg->end(); ++ci) {
                    int n = 0;
                    for (auto si = (*ci)->succ_begin(); si != (*ci)->succ_end(); ++si)
                        if (si->isReachable() && caseBlocks.count(si->getReachableBlock()))
                            n++;
                    if (n >= 2) dispatchBlocks.insert(*ci);
                }
                for (auto ci = cfg->begin(); ci != cfg->end(); ++ci) {
                    const CFGBlock *B = *ci;
                    if (dispatchBlocks.count(B)) continue;
                    for (auto si = B->succ_begin(); si != B->succ_end(); ++si) {
                        if (si->isReachable() && caseBlocks.count(si->getReachableBlock())) {
                            FS.fallThroughCount++;
                            break;
                        }
                    }
                }
                if (FS.fallThroughCount > 0) {
                    FS.hasFallThrough = true;
                    Stats.fallThroughCount += FS.fallThroughCount;
                }
            }

            // 检测冗余 return
            if (FD->hasBody()) {
                if (auto *CS = dyn_cast<CompoundStmt>(FD->getBody())) {
                    if (!CS->body_empty()) {
                        auto it = CS->body_end();
                        --it;
                        if (auto *RS = dyn_cast<ReturnStmt>(*it)) {
                            bool retIsReachable = true;
                            for (auto ci = cfg->begin(); ci != cfg->end(); ++ci) {
                                for (const CFGElement &Elem : **ci) {
                                    if (auto S = Elem.getAs<CFGStmt>()) {
                                        if (S->getStmt() == RS) {
                                            retIsReachable = reach.isReachable(entryBlock, *ci);
                                            goto ret_found;
                                        }
                                    }
                                }
                            }
                            ret_found:
                            if (retIsReachable) {
                                if (FD->getReturnType()->isVoidType() &&
                                    !RS->getRetValue()) {
                                    FS.hasRedundantReturn = true;
                                    Stats.redundantReturns++;
                                } else if (FS.name == "main" &&
                                           FD->getReturnType()->isIntegerType()) {
                                    if (auto *IL = dyn_cast<IntegerLiteral>(
                                            RS->getRetValue())) {
                                        if (IL->getValue() == 0) {
                                            FS.hasRedundantReturn = true;
                                            Stats.redundantReturns++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (CurrentFunc == &FS && FS.maxNesting > Thresh.maxNesting) {
        FS.hasDeepNesting = true;
        Stats.deepNestingFunctions++;
    }

    if (CurrentFunc == &FS) {
        Stats.functions.push_back(std::move(FS));
        CurrentFunc = Saved;
    }

    return result;
}

bool MyASTVisitor::VisitFunctionDecl(FunctionDecl *FD) {
    if (!FD->isThisDeclarationADefinition())
        return true;
    if (!isFromMainFile(FD))
        return true;

    Stats.totalFunctions++;

    if (FD->getNumParams() == 0)
        Stats.paramlessFunctions++;
    else
        Stats.paramFunctions++;

    if (CurrentFunc && CurrentFunc->isEmptyOrReturnOnly)
        Stats.emptyFunctions++;

    if (CurrentFunc && CurrentFunc->isOverlong)
        Stats.overlongFunctions++;

    if (CurrentFunc && CurrentFunc->isBadName)
        Stats.badNamedFunctions++;

    if (CurrentFunc && CurrentFunc->hasTooManyParams)
        Stats.tooManyParamsFunctions++;

    return true;
}

bool MyASTVisitor::VisitVarDecl(VarDecl *VD) {
    if (!isFromMainFile(VD))
        return true;
    if (isa<ParmVarDecl>(VD))
        return true;

    if (VD->getDeclContext()->isTranslationUnit()) {
        Stats.globalVars++;
        std::string name = VD->getNameAsString();
        if (!isValidGlobalVarName(name)) {
            Stats.badGlobalVarNames++;
            Stats.badGlobalVars.push_back(name);
        }
    } else {
        Stats.localVars++;
        if (CurrentFunc)
            CurrentFunc->localVars++;
    }
    return true;
}

bool MyASTVisitor::VisitIfStmt(IfStmt *IS) {
    if (isStmtFromMainFile(IS)) {
        Stats.ifCount++;
        if (CurrentFunc) CurrentFunc->ifCount++;
        checkEmptyBlock(IS->getThen(), "if");
        if (IS->getElse())
            checkEmptyBlock(IS->getElse(), "else");
    }
    return true;
}

bool MyASTVisitor::VisitForStmt(ForStmt *FS) {
    if (isStmtFromMainFile(FS)) {
        Stats.forCount++;
        if (CurrentFunc) CurrentFunc->forCount++;
        checkEmptyBlock(FS->getBody(), "for");
    }
    return true;
}

bool MyASTVisitor::VisitWhileStmt(WhileStmt *WS) {
    if (isStmtFromMainFile(WS)) {
        Stats.whileCount++;
        if (CurrentFunc) CurrentFunc->whileCount++;
        checkEmptyBlock(WS->getBody(), "while");
    }
    return true;
}

bool MyASTVisitor::VisitDoStmt(DoStmt *DS) {
    if (isStmtFromMainFile(DS)) {
        Stats.doWhileCount++;
        if (CurrentFunc) CurrentFunc->doWhileCount++;
        checkEmptyBlock(DS->getBody(), "do-while");
    }
    return true;
}

bool MyASTVisitor::VisitSwitchStmt(SwitchStmt *SS) {
    if (isStmtFromMainFile(SS)) {
        Stats.switchCount++;
        if (CurrentFunc) CurrentFunc->switchCount++;
    }
    return true;
}

bool MyASTVisitor::VisitSwitchCase(SwitchCase *SC) {
    if (isStmtFromMainFile(SC)) {
        Stats.caseCount++;
        if (CurrentFunc) CurrentFunc->caseCount++;
    }
    return true;
}

bool MyASTVisitor::VisitBreakStmt(BreakStmt *BS) {
    if (isStmtFromMainFile(BS)) {
        Stats.breakCount++;
        if (CurrentFunc) CurrentFunc->breakCount++;
    }
    return true;
}

bool MyASTVisitor::VisitContinueStmt(ContinueStmt *CS) {
    if (isStmtFromMainFile(CS)) {
        Stats.continueCount++;
        if (CurrentFunc) CurrentFunc->continueCount++;
    }
    return true;
}

bool MyASTVisitor::VisitReturnStmt(ReturnStmt *RS) {
    if (isStmtFromMainFile(RS)) {
        Stats.returnCount++;
        if (CurrentFunc) CurrentFunc->returnCount++;
    }
    return true;
}

bool MyASTVisitor::VisitGotoStmt(GotoStmt *GS) {
    if (isStmtFromMainFile(GS)) {
        Stats.gotoCount++;
        if (CurrentFunc) CurrentFunc->gotoCount++;
    }
    return true;
}

bool MyASTVisitor::VisitBinaryOperator(BinaryOperator *BO) {
    if (isStmtFromMainFile(BO)) {
        if (BO->getOpcode() == BO_LAnd || BO->getOpcode() == BO_LOr) {
            Stats.logicalAndOrCount++;
            if (CurrentFunc) CurrentFunc->logicalAndOrCount++;
        }
    }
    return true;
}

bool MyASTVisitor::VisitConditionalOperator(ConditionalOperator *CO) {
    if (isStmtFromMainFile(CO)) {
        Stats.conditionalOpCount++;
        if (CurrentFunc) CurrentFunc->conditionalOpCount++;
    }
    return true;
}

bool MyASTVisitor::VisitCallExpr(CallExpr *CE) {
    if (!isStmtFromMainFile(CE)) return true;

    Stats.callCount++;
    if (FunctionDecl *Callee = CE->getDirectCallee()) {
        Stats.callTargets[Callee->getNameAsString()]++;
        if (CurrentFunc)
            CurrentFunc->callTargets[Callee->getNameAsString()]++;
    } else {
        Stats.callTargets["<indirect>"]++;
        if (CurrentFunc)
            CurrentFunc->callTargets["<indirect>"]++;
    }

    if (CurrentFunc) CurrentFunc->callCount++;
    return true;
}

// ====================== MyASTConsumer ======================

MyASTConsumer::MyASTConsumer(AnalysisStats &S, const Thresholds &T) : Stats(S), Thresh(T) {}

void MyASTConsumer::HandleTranslationUnit(ASTContext &Context) {
    MyASTVisitor Visitor(Stats, Thresh);
    Visitor.SM = &Context.getSourceManager();
    Visitor.Ctx = &Context;
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
}
