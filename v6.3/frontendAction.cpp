#include "llvm/Support/CommandLine.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Parse/ParseAST.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/JSON.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/Analyses/CFGReachabilityAnalysis.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace llvm;
using namespace clang;

static cl::list<std::string>
InputFiles(cl::Positional, cl::desc("Input files"), cl::ZeroOrMore);

static cl::opt<bool>
JsonOutput("json", cl::desc("Output JSON instead of text report"));

static cl::opt<bool>
ReadFromStdin("stdin", cl::desc("Read source code from standard input"));

// 代码规范检查阈值
static const int MAX_FUNCTION_LINES = 50;
static const int MAX_LINE_LENGTH = 100;
static const int MAX_CCN = 10;                           // 圈复杂度上限
static const int MAX_PARAMS = 5;                          // 参数个数上限
static const int MAX_NESTING = 4;                          // 嵌套深度上限

// 过长单行信息
struct LongLineInfo {
    std::string file;
    int lineNum;
    int length;
};

// 空代码块信息
struct EmptyBlockInfo {
    std::string funcName;
    std::string type;   // "if", "for", "while", "do-while", "else"
};

// switch case 穿透信息
struct FallThroughInfo {
    std::string funcName;
    std::string fromCase;   // 源 case 标签
    std::string toCase;     // 穿透到的 case 标签
};

// 检查函数名是否符合小写下划线风格 (snake_case)
static bool isValidSnakeCase(const std::string &name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_')
            return false;
    }
    // 首字符必须是字母（不允许数字和下划线开头）
    return name[0] >= 'a' && name[0] <= 'z';
}

// 检查全局变量名是否有 g_ 前缀（其余部分需符合 snake_case）
static bool isValidGlobalVarName(const std::string &name) {
    if (name.size() < 3) return false;  // 至少 "g_x"
    if (name[0] != 'g' || name[1] != '_') return false;
    return isValidSnakeCase(name.substr(2));
}

// ====================== 逐函数统计结构 ======================
struct FunctionStats {
    std::string name;
    int lines = 0;
    int localVars = 0;
    int ifCount = 0;
    int forCount = 0;
    int whileCount = 0;
    int doWhileCount = 0;
    int switchCount = 0;
    int caseCount = 0;                                  // case/default 标签数
    int breakCount = 0;
    int continueCount = 0;
    int returnCount = 0;
    int gotoCount = 0;
    int callCount = 0;
    int paramCount = 0;
    int ccn = 1;                                        // 圈复杂度 (Cyclomatic Complexity Number)
    int logicalAndOrCount = 0;                          // && 和 || 运算符数
    int conditionalOpCount = 0;                         // ?: 三元运算符数
    bool isEmptyOrReturnOnly = false;
    bool isOverlong = false;                            // 行数超标 (>50行)
    bool isBadName = false;                             // 命名不规范（非snake_case）
    bool hasTooManyParams = false;                      // 参数过多 (>MAX_PARAMS)
    int maxNesting = 0;                                 // 最大嵌套深度
    bool hasDeepNesting = false;                        // 嵌套过深 (>MAX_NESTING)
    bool hasRedundantReturn = false;                    // 冗余 return; (void函数末尾)
    bool hasDeadCode = false;                           // 含不可达路径上的死代码
    int deadStmtCount = 0;                              // 不可达路径上的语句数
    bool hasFallThrough = false;                        // 含 switch 穿透
    int fallThroughCount = 0;                           // switch 穿透数
    bool isHighCCN = false;                             // 圈复杂度过高 (>MAX_CCN)
    int emptyBlocks = 0;                                // 空代码块数
    std::map<std::string, int> callTargets;
};

// ====================== 全局统计数据结构 ======================
struct AnalysisStats {
    int totalFunctions = 0;
    int globalVars = 0;
    int localVars = 0;
    int ifCount = 0;
    int forCount = 0;
    int whileCount = 0;
    int doWhileCount = 0;
    int switchCount = 0;
    int caseCount = 0;                                  // case/default 标签数
    int logicalAndOrCount = 0;                          // && 和 || 运算符数
    int conditionalOpCount = 0;                         // ?: 三元运算符数
    int breakCount = 0;
    int continueCount = 0;
    int returnCount = 0;
    int gotoCount = 0;
    int callCount = 0;
    int includeCount = 0;
    int emptyFunctions = 0;                             // 空函数（无函数体/仅return）
    int paramlessFunctions = 0;                         // 无参函数
    int paramFunctions = 0;                             // 有参函数
    int overlongFunctions = 0;                          // 行数超标函数 (>50行)
    int badNamedFunctions = 0;                          // 命名不规范函数
    int tooManyParamsFunctions = 0;                     // 参数过多函数 (>MAX_PARAMS)
    int deepNestingFunctions = 0;                       // 嵌套过深函数 (>MAX_NESTING)
    int badGlobalVarNames = 0;                          // 全局变量缺 g_ 前缀
    std::vector<std::string> badGlobalVars;             // 问题全局变量名列表
    int longLineCount = 0;                              // 过长单行(>100字符)
    std::vector<LongLineInfo> longLines;                // 过长单行明细
    int redundantReturns = 0;                           // 冗余 return; 语句
    int deadStmts = 0;                                  // 不可达路径上的死代码语句
    int fallThroughCount = 0;                          // switch 穿透数
    std::vector<FallThroughInfo> fallThroughs;          // 穿透明细
    int highCCNFunctions = 0;                          // 圈复杂度过高函数 (>MAX_CCN)
    int emptyBlockCount = 0;                            // 空代码块数
    std::vector<EmptyBlockInfo> emptyBlocks;            // 空代码块明细
    int totalLines = 0;                                // 总行数
    int codeLines = 0;                                 // 纯代码行
    int blankLines = 0;                                // 空行
    int singleCommentLines = 0;                        // 单行注释 (//)
    int multiCommentLines = 0;                         // 多行注释 (/* */)
    int preprocessorLines = 0;                         // 预处理指令行 (#)
    std::map<std::string, int> callTargets;             // 被调用函数名 -> 调用次数
    std::vector<FunctionStats> functions;               // 逐函数明细

    // 多文件聚合
    AnalysisStats &operator+=(const AnalysisStats &Other) {
        totalFunctions += Other.totalFunctions;
        globalVars     += Other.globalVars;
        localVars      += Other.localVars;
        ifCount        += Other.ifCount;
        forCount       += Other.forCount;
        whileCount     += Other.whileCount;
        doWhileCount   += Other.doWhileCount;
        switchCount    += Other.switchCount;
        caseCount      += Other.caseCount;
        logicalAndOrCount  += Other.logicalAndOrCount;
        conditionalOpCount += Other.conditionalOpCount;
        breakCount     += Other.breakCount;
        continueCount  += Other.continueCount;
        returnCount    += Other.returnCount;
        gotoCount      += Other.gotoCount;
        callCount      += Other.callCount;
        includeCount      += Other.includeCount;
        emptyFunctions     += Other.emptyFunctions;
        paramlessFunctions += Other.paramlessFunctions;
        paramFunctions     += Other.paramFunctions;
        overlongFunctions  += Other.overlongFunctions;
        badNamedFunctions  += Other.badNamedFunctions;
        tooManyParamsFunctions += Other.tooManyParamsFunctions;
        deepNestingFunctions += Other.deepNestingFunctions;
        badGlobalVarNames  += Other.badGlobalVarNames;
        badGlobalVars.insert(badGlobalVars.end(),
                             Other.badGlobalVars.begin(),
                             Other.badGlobalVars.end());
        longLineCount += Other.longLineCount;
        longLines.insert(longLines.end(),
                         Other.longLines.begin(), Other.longLines.end());
        redundantReturns  += Other.redundantReturns;
        deadStmts        += Other.deadStmts;
        fallThroughCount  += Other.fallThroughCount;
        fallThroughs.insert(fallThroughs.end(),
                            Other.fallThroughs.begin(), Other.fallThroughs.end());
        highCCNFunctions  += Other.highCCNFunctions;
        emptyBlockCount   += Other.emptyBlockCount;
        emptyBlocks.insert(emptyBlocks.end(),
                          Other.emptyBlocks.begin(), Other.emptyBlocks.end());
        totalLines         += Other.totalLines;
        codeLines          += Other.codeLines;
        blankLines         += Other.blankLines;
        singleCommentLines += Other.singleCommentLines;
        multiCommentLines  += Other.multiCommentLines;
        preprocessorLines  += Other.preprocessorLines;
        for (const auto &p : Other.callTargets)
            callTargets[p.first] += p.second;
        functions.insert(functions.end(),
                         Other.functions.begin(), Other.functions.end());
        return *this;
    }
};

// ====================== 分析结果包装（v6.2 — 结构化错误返回）======================
struct AnalysisResult {
    bool success = false;
    AnalysisStats stats;
    std::vector<std::string> errors;    // 流程错误 + Clang Error 诊断
    std::vector<std::string> warnings;  // Clang Warning 诊断
};

// ====================== Clang 诊断收集器（替代 IgnoringDiagConsumer）=============
class DiagCollector : public clang::DiagnosticConsumer {
    AnalysisResult &Result;
public:
    explicit DiagCollector(AnalysisResult &R) : Result(R) {}
    void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                          const clang::Diagnostic &Info) override {
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
};


// ====================== 自定义 AST 访问者 ======================
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
    AnalysisStats &Stats;
    SourceManager *SM = nullptr;
    ASTContext *Ctx = nullptr;

    // 当前正在遍历的函数（用于逐函数统计）
    FunctionStats *CurrentFunc = nullptr;

    // 当前嵌套深度（if/for/while/do-while/switch）
    int currentDepth = 0;

    explicit MyASTVisitor(AnalysisStats &S) : Stats(S) {}

    // ===== Traverse 覆写：跟踪嵌套深度 =====
    bool TraverseIfStmt(IfStmt *IS) {
        currentDepth++;
        if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
            CurrentFunc->maxNesting = currentDepth;
        bool r = RecursiveASTVisitor::TraverseIfStmt(IS);
        currentDepth--;
        return r;
    }
    bool TraverseForStmt(ForStmt *FS) {
        currentDepth++;
        if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
            CurrentFunc->maxNesting = currentDepth;
        bool r = RecursiveASTVisitor::TraverseForStmt(FS);
        currentDepth--;
        return r;
    }
    bool TraverseWhileStmt(WhileStmt *WS) {
        currentDepth++;
        if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
            CurrentFunc->maxNesting = currentDepth;
        bool r = RecursiveASTVisitor::TraverseWhileStmt(WS);
        currentDepth--;
        return r;
    }
    bool TraverseDoStmt(DoStmt *DS) {
        currentDepth++;
        if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
            CurrentFunc->maxNesting = currentDepth;
        bool r = RecursiveASTVisitor::TraverseDoStmt(DS);
        currentDepth--;
        return r;
    }
    bool TraverseSwitchStmt(SwitchStmt *SS) {
        currentDepth++;
        if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
            CurrentFunc->maxNesting = currentDepth;
        bool r = RecursiveASTVisitor::TraverseSwitchStmt(SS);
        currentDepth--;
        return r;
    }

    // 辅助：检查声明是否来自主文件（排除头文件中的声明）
    bool isFromMainFile(Decl *D) {
        if (!SM) return true;
        return SM->isInMainFile(D->getLocation());
    }

    // 辅助：检查语句是否来自主文件（排除头文件中的语句）
    bool isStmtFromMainFile(Stmt *S) {
        if (!SM) return true;
        return SM->isInMainFile(S->getBeginLoc());
    }

    // 辅助：检测空代码块（if/for/while/do-while 后空大括号）
    void checkEmptyBlock(Stmt *Body, const std::string &Type) {
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

    // ===== TraverseFunctionDecl：管理逐函数统计的 push/pop =====
    bool TraverseFunctionDecl(FunctionDecl *FD) {
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

            // 检测参数过多
            if (FS.paramCount > MAX_PARAMS)
                FS.hasTooManyParams = true;

            // 检测行数超标
            if (FS.lines > MAX_FUNCTION_LINES)
                FS.isOverlong = true;

            // 检测命名规范：必须小写下划线风格
            if (!isValidSnakeCase(FS.name))
                FS.isBadName = true;

            CurrentFunc = &FS;
        }

        // 递归遍历函数体（期间所有 Visit 方法都能看到 CurrentFunc）
        bool result = RecursiveASTVisitor::TraverseFunctionDecl(FD);

        // 计算圈复杂度（基于控制流图: M = E - N + 2P）
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
                if (FS.ccn > MAX_CCN) {
                    FS.isHighCCN = true;
                    Stats.highCCNFunctions++;
                }

                // 检测死代码：CFG 中从入口不可达的块中的所有语句
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
                                // case→case 或 body→case 均视为穿透
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

                // 检测冗余 return（仅在可达时才标记，死 return 归死代码检查）
                if (FD->hasBody()) {
                    if (auto *CS = dyn_cast<CompoundStmt>(FD->getBody())) {
                        if (!CS->body_empty()) {
                            auto it = CS->body_end();
                            --it;
                            if (auto *RS = dyn_cast<ReturnStmt>(*it)) {
                                // 在 CFG 中定位该 return，检查其所在块是否可达
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

        // 检测嵌套深度
        if (CurrentFunc == &FS && FS.maxNesting > MAX_NESTING) {
            FS.hasDeepNesting = true;
            Stats.deepNestingFunctions++;
        }

        // 弹出当前函数，保存统计
        if (CurrentFunc == &FS) {
            Stats.functions.push_back(std::move(FS));
            CurrentFunc = Saved;
        }

        return result;
    }

    // 访问函数定义
    bool VisitFunctionDecl(FunctionDecl *FD) {
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

    // 访问变量声明
    bool VisitVarDecl(VarDecl *VD) {
        if (!isFromMainFile(VD))        // 只统计主文件
            return true;
        if (isa<ParmVarDecl>(VD))       // 跳过函数参数
            return true;

        if (VD->getDeclContext()->isTranslationUnit()) {
            Stats.globalVars++;         // 全局变量
            // 检测全局变量 g_ 前缀
            std::string name = VD->getNameAsString();
            if (!isValidGlobalVarName(name)) {
                Stats.badGlobalVarNames++;
                Stats.badGlobalVars.push_back(name);
            }
        } else {
            Stats.localVars++;          // 全局汇总：局部变量
            if (CurrentFunc)
                CurrentFunc->localVars++; // 逐函数：当前函数的局部变量
        }
        return true;
    }

    // if 语句
    bool VisitIfStmt(IfStmt *IS) {
        if (isStmtFromMainFile(IS)) {
            Stats.ifCount++;
            if (CurrentFunc) CurrentFunc->ifCount++;
            checkEmptyBlock(IS->getThen(), "if");
            if (IS->getElse())
                checkEmptyBlock(IS->getElse(), "else");
        }
        return true;
    }

    // for 语句
    bool VisitForStmt(ForStmt *FS) {
        if (isStmtFromMainFile(FS)) {
            Stats.forCount++;
            if (CurrentFunc) CurrentFunc->forCount++;
            checkEmptyBlock(FS->getBody(), "for");
        }
        return true;
    }

    // while 语句
    bool VisitWhileStmt(WhileStmt *WS) {
        if (isStmtFromMainFile(WS)) {
            Stats.whileCount++;
            if (CurrentFunc) CurrentFunc->whileCount++;
            checkEmptyBlock(WS->getBody(), "while");
        }
        return true;
    }

    // do-while 语句
    bool VisitDoStmt(DoStmt *DS) {
        if (isStmtFromMainFile(DS)) {
            Stats.doWhileCount++;
            if (CurrentFunc) CurrentFunc->doWhileCount++;
            checkEmptyBlock(DS->getBody(), "do-while");
        }
        return true;
    }

    // switch 语句
    bool VisitSwitchStmt(SwitchStmt *SS) {
        if (isStmtFromMainFile(SS)) {
            Stats.switchCount++;
            if (CurrentFunc) CurrentFunc->switchCount++;
        }
        return true;
    }

    // case / default 标签（圈复杂度决策点）
    bool VisitSwitchCase(SwitchCase *SC) {
        if (isStmtFromMainFile(SC)) {
            Stats.caseCount++;
            if (CurrentFunc) CurrentFunc->caseCount++;
        }
        return true;
    }

    // break 语句
    bool VisitBreakStmt(BreakStmt *BS) {
        if (isStmtFromMainFile(BS)) {
            Stats.breakCount++;
            if (CurrentFunc) CurrentFunc->breakCount++;
        }
        return true;
    }

    // continue 语句
    bool VisitContinueStmt(ContinueStmt *CS) {
        if (isStmtFromMainFile(CS)) {
            Stats.continueCount++;
            if (CurrentFunc) CurrentFunc->continueCount++;
        }
        return true;
    }

    // return 语句
    bool VisitReturnStmt(ReturnStmt *RS) {
        if (isStmtFromMainFile(RS)) {
            Stats.returnCount++;
            if (CurrentFunc) CurrentFunc->returnCount++;
        }
        return true;
    }

    // goto 语句
    bool VisitGotoStmt(GotoStmt *GS) {
        if (isStmtFromMainFile(GS)) {
            Stats.gotoCount++;
            if (CurrentFunc) CurrentFunc->gotoCount++;
        }
        return true;
    }

    // 二元运算符：逻辑 && 和 ||（圈复杂度决策点）
    bool VisitBinaryOperator(BinaryOperator *BO) {
        if (isStmtFromMainFile(BO)) {
            if (BO->getOpcode() == BO_LAnd || BO->getOpcode() == BO_LOr) {
                Stats.logicalAndOrCount++;
                if (CurrentFunc) CurrentFunc->logicalAndOrCount++;
            }
        }
        return true;
    }

    // 三元运算符 ?:（圈复杂度决策点）
    bool VisitConditionalOperator(ConditionalOperator *CO) {
        if (isStmtFromMainFile(CO)) {
            Stats.conditionalOpCount++;
            if (CurrentFunc) CurrentFunc->conditionalOpCount++;
        }
        return true;
    }

    // 函数调用
    bool VisitCallExpr(CallExpr *CE) {
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
};

// ====================== 自定义 AST Consumer ======================
class MyASTConsumer : public ASTConsumer {
    AnalysisStats &Stats;
public:
    explicit MyASTConsumer(AnalysisStats &S) : Stats(S) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        MyASTVisitor Visitor(Stats);
        Visitor.SM = &Context.getSourceManager();
        Visitor.Ctx = &Context;
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
};

// ====================== JSON 序列化 (v6.0) ======================

// 强制走 const ObjectKey& 重载（兼容 LLVM 18 共享库缺少 rvalue 版本）
static json::Value &jsonSet(json::Object &O, const char *Key) {
    const json::ObjectKey k(Key);
    return O[k];
}

static json::Value toJSON(const LongLineInfo &L) {
    return json::Object{{"file", L.file}, {"line", L.lineNum}, {"length", L.length}};
}

static json::Value toJSON(const EmptyBlockInfo &B) {
    return json::Object{{"function", B.funcName}, {"type", B.type}};
}

static json::Value toJSON(const FallThroughInfo &F) {
    return json::Object{{"function", F.funcName},
                         {"fromCase", F.fromCase},
                         {"toCase", F.toCase}};
}

// 将一个 string→int map 转为 JSON Object
static json::Object toJSON(const std::map<std::string, int> &M) {
    json::Object O;
    for (const auto &P : M)
        O[P.first] = P.second;
    return O;
}

static json::Value toJSON(const FunctionStats &F) {
    // 违规列表
    std::vector<std::string> violations;
    if (F.isOverlong)          violations.push_back("overlong");
    if (F.hasTooManyParams)    violations.push_back("tooManyParams");
    if (F.hasDeepNesting)      violations.push_back("deepNesting");
    if (F.isBadName)           violations.push_back("badName");
    if (F.hasRedundantReturn)  violations.push_back("redundantReturn");
    if (F.hasDeadCode)         violations.push_back("deadCode");
    if (F.hasFallThrough)      violations.push_back("fallThrough");
    if (F.isHighCCN)           violations.push_back("highCCN");
    if (F.isEmptyOrReturnOnly) violations.push_back("emptyOrReturnOnly");

    json::Object O{
        {"name", F.name},
        {"lines", F.lines},
        {"params", F.paramCount},
        {"ccn", F.ccn},
        {"localVars", F.localVars},
        {"maxNesting", F.maxNesting},
        {"emptyBlocks", F.emptyBlocks},
        {"controls",
         json::Object{{"if", F.ifCount},
                       {"for", F.forCount},
                       {"while", F.whileCount},
                       {"doWhile", F.doWhileCount},
                       {"switch", F.switchCount},
                       {"case", F.caseCount},
                       {"logicalAndOr", F.logicalAndOrCount},
                       {"conditionalOp", F.conditionalOpCount},
                       {"break", F.breakCount},
                       {"continue", F.continueCount},
                       {"return", F.returnCount},
                       {"goto", F.gotoCount}}},
        {"calls",
         json::Object{{"total", F.callCount}, {"targets", toJSON(F.callTargets)}}},
        {"violations", json::Array(violations)},
    };
    return O;
}

static json::Value toJSON(const AnalysisStats &Stats) {
    // 平均 CCN + 最高 CCN
    double avgCCN = 0;
    int maxCCN = 0;
    std::string maxCCNFunc;
    if (!Stats.functions.empty()) {
        int total = 0;
        for (const auto &F : Stats.functions) {
            total += F.ccn;
            if (F.ccn > maxCCN) {
                maxCCN = F.ccn;
                maxCCNFunc = F.name;
            }
        }
        avgCCN = (double)total / Stats.functions.size();
    }

    // 违规模板函数：生成 {count, items} 结构
    auto violationList = [&](const auto &Items) {
        return json::Object{{"count", (int)Items.size()}, {"items", json::Array(Items)}};
    };

    // 收集各类违规的逐函数详情
    std::vector<json::Value> overlongItems, tooManyParamsItems, deepNestingItems,
        badNameItems, redundantReturnItems, deadCodeItems, fallThroughItems,
        highCCNItems, emptyBlockItems, longLineItems;
    for (const auto &F : Stats.functions) {
        if (F.isOverlong)
            overlongItems.push_back(
                json::Object{{"name", F.name}, {"lines", F.lines}});
        if (F.hasTooManyParams)
            tooManyParamsItems.push_back(
                json::Object{{"name", F.name}, {"params", F.paramCount}});
        if (F.hasDeepNesting)
            deepNestingItems.push_back(
                json::Object{{"name", F.name}, {"depth", F.maxNesting}});
        if (F.isBadName)
            badNameItems.push_back(json::Object{{"name", F.name}});
        if (F.hasRedundantReturn)
            redundantReturnItems.push_back(json::Object{{"name", F.name}});
        if (F.hasDeadCode)
            deadCodeItems.push_back(
                json::Object{{"name", F.name}, {"count", F.deadStmtCount}});
        if (F.hasFallThrough)
            fallThroughItems.push_back(
                json::Object{{"name", F.name}, {"count", F.fallThroughCount}});
        if (F.isHighCCN)
            highCCNItems.push_back(
                json::Object{{"name", F.name}, {"ccn", F.ccn}});
    }
    for (const auto &B : Stats.emptyBlocks)
        emptyBlockItems.push_back(
            json::Object{{"function", B.funcName}, {"type", B.type}});
    for (const auto &L : Stats.longLines)
        longLineItems.push_back(toJSON(L));

    // 组装完整 JSON 树
    json::Object Root{
        {"summary",
         json::Object{
             {"totalFunctions", Stats.totalFunctions},
             {"emptyFunctions", Stats.emptyFunctions},
             {"paramlessFunctions", Stats.paramlessFunctions},
             {"paramFunctions", Stats.paramFunctions},
             {"globalVars", Stats.globalVars},
             {"localVars", Stats.localVars},
             {"includeCount", Stats.includeCount},
             {"avgCCN", avgCCN},
             {"maxCCN",
              json::Object{{"function", maxCCNFunc}, {"value", maxCCN}}},
         }},
        {"lines",
         json::Object{
             {"total", Stats.totalLines},
             {"code", Stats.codeLines},
             {"blank", Stats.blankLines},
             {"singleComment", Stats.singleCommentLines},
             {"multiComment", Stats.multiCommentLines},
             {"preprocessor", Stats.preprocessorLines},
         }},
        {"controls",
         json::Object{
             {"if", Stats.ifCount},
             {"for", Stats.forCount},
             {"while", Stats.whileCount},
             {"doWhile", Stats.doWhileCount},
             {"switch", Stats.switchCount},
             {"case", Stats.caseCount},
             {"logicalAndOr", Stats.logicalAndOrCount},
             {"conditionalOp", Stats.conditionalOpCount},
             {"break", Stats.breakCount},
             {"continue", Stats.continueCount},
             {"return", Stats.returnCount},
             {"goto", Stats.gotoCount},
         }},
        {"calls",
         json::Object{
             {"total", Stats.callCount},
             {"targets", toJSON(Stats.callTargets)},
         }},
        {"violations",
         json::Object{
             {"overlongFunctions", violationList(overlongItems)},
             {"tooManyParams", violationList(tooManyParamsItems)},
             {"deepNesting", violationList(deepNestingItems)},
             {"badNames", violationList(badNameItems)},
             {"badGlobalVarNames",
              json::Object{
                  {"count", Stats.badGlobalVarNames},
                  {"items", json::Array(Stats.badGlobalVars)},
              }},
             {"longLines", violationList(longLineItems)},
             {"redundantReturns", violationList(redundantReturnItems)},
             {"deadStmts", violationList(deadCodeItems)},
             {"fallThroughs", violationList(fallThroughItems)},
             {"highCCN", violationList(highCCNItems)},
             {"emptyBlocks", violationList(emptyBlockItems)},
             {"gotoCount", Stats.gotoCount},
         }},
        {"functions", json::Array(Stats.functions)},
    };

    return Root;
}

// v6.2: 包装 AnalysisStats + errors/warnings
static json::Value toJSON(const AnalysisResult &R) {
    json::Value json = toJSON(R.stats);
    if (auto *O = json.getAsObject()) {
        if (!R.errors.empty())
            jsonSet(*O, "errors") = json::Array(R.errors);
        if (!R.warnings.empty())
            jsonSet(*O, "warnings") = json::Array(R.warnings);
        jsonSet(*O, "success") = R.success;
    }
    return json;
}

// ====================== 输出报告 ======================
static void printReport(const AnalysisStats &Stats,
                        const std::vector<std::string> &Files) {
    outs() << "\n";
    outs() << "========== C 源文件分析报告 ==========\n\n";

    outs() << "【基本信息】\n";
    outs() << "  分析文件数: " << Files.size() << "\n";
    for (const auto &f : Files)
        outs() << "    - " << f << "\n";
    outs() << "  总函数数量: " << Stats.totalFunctions << "\n";
    outs() << "  无参函数:     " << Stats.paramlessFunctions << "\n";
    outs() << "  有参函数:     " << Stats.paramFunctions << "\n";
    outs() << "  空函数(无体/仅return): " << Stats.emptyFunctions << "\n";
    outs() << "  全局变量数量: " << Stats.globalVars << "\n";
    outs() << "  局部变量数量: " << Stats.localVars << "\n";
    outs() << "  #include 数量: " << Stats.includeCount << "\n\n";

    // ===== 代码行数统计（v4.0 新增）=====
    outs() << "【代码行数统计】\n";
    outs() << "  总行数:     " << Stats.totalLines << "\n";
    outs() << "  代码行:     " << Stats.codeLines << "\n";
    outs() << "  空行:       " << Stats.blankLines << "\n";
    outs() << "  预处理指令: " << Stats.preprocessorLines << "\n";
    int totalComment = Stats.singleCommentLines + Stats.multiCommentLines;
    outs() << "  注释行:     " << totalComment << "\n";
    outs() << "    - 单行(//): " << Stats.singleCommentLines << "\n";
    outs() << "    - 多行(/**/): " << Stats.multiCommentLines << "\n\n";

    // ===== 代码规范检查（v3.0 新增）=====
    outs() << "【代码规范检查】\n";
    outs() << "  函数行数上限(" << MAX_FUNCTION_LINES << "行): ";
    if (Stats.overlongFunctions == 0) {
        outs() << "✓ 全部在限制内\n";
    } else {
        outs() << "⚠ 发现 " << Stats.overlongFunctions << " 个超标\n";
        for (const auto &F : Stats.functions) {
            if (F.isOverlong)
                outs() << "    - " << F.name << "(): " << F.lines << " 行 (超标 "
                        << (F.lines - MAX_FUNCTION_LINES) << " 行)\n";
        }
    }
    outs() << "\n";

    // goto 语句检查（代码规范禁用项）
    outs() << "  goto 语句:   ";
    if (Stats.gotoCount == 0) {
        outs() << "✓ 未使用\n";
    } else {
        outs() << "⚠ 发现 " << Stats.gotoCount << " 处\n";
        for (const auto &F : Stats.functions) {
            if (F.gotoCount > 0)
                outs() << "    - " << F.name << "(): " << F.gotoCount << " 处\n";
        }
    }

    // 参数过多检查
    outs() << "  函数参数上限(" << MAX_PARAMS << "个): ";
    if (Stats.tooManyParamsFunctions == 0) {
        outs() << "✓ 全部在限制内\n";
    } else {
        outs() << "⚠ 发现 " << Stats.tooManyParamsFunctions << " 个超标\n";
        for (const auto &F : Stats.functions) {
            if (F.hasTooManyParams)
                outs() << "    - " << F.name << "(): " << F.paramCount
                       << " 个参数 (超标 " << (F.paramCount - MAX_PARAMS) << " 个)\n";
        }
    }

    // 嵌套深度检查
    outs() << "  嵌套深度上限(" << MAX_NESTING << "): ";
    if (Stats.deepNestingFunctions == 0) {
        outs() << "✓ 全部在限制内\n";
    } else {
        outs() << "⚠ 发现 " << Stats.deepNestingFunctions << " 个超标\n";
        for (const auto &F : Stats.functions) {
            if (F.hasDeepNesting)
                outs() << "    - " << F.name << "(): 最大嵌套 " << F.maxNesting
                       << " 层 (超标 " << (F.maxNesting - MAX_NESTING) << " 层)\n";
        }
    }

    // 命名规范检查（强制小写下划线风格）
    outs() << "  命名规范:   ";
    if (Stats.badNamedFunctions == 0) {
        outs() << "✓ 全部符合 snake_case\n";
    } else {
        outs() << "⚠ 发现 " << Stats.badNamedFunctions << " 个不规范\n";
        for (const auto &F : Stats.functions) {
            if (F.isBadName)
                outs() << "    - " << F.name << "(): 应使用小写下划线风格\n";
        }
    }

    // 全局变量命名规范检查（强制 g_ 前缀）
    outs() << "  全局变量命名: ";
    if (Stats.badGlobalVarNames == 0) {
        outs() << "✓ 全部有 g_ 前缀\n";
    } else {
        outs() << "⚠ 发现 " << Stats.badGlobalVarNames << " 个缺少 g_ 前缀\n";
        for (const auto &name : Stats.badGlobalVars)
            outs() << "    - " << name << " → 应改为 g_" << name << "\n";
    }

    // 过长单行代码检查 (>MAX_LINE_LENGTH 字符)
    outs() << "  单行长度(" << MAX_LINE_LENGTH << "字符): ";
    if (Stats.longLineCount == 0) {
        outs() << "✓ 全部在限制内\n";
    } else {
        outs() << "⚠ 发现 " << Stats.longLineCount << " 行超标\n";
        for (const auto &L : Stats.longLines)
            outs() << "    - " << L.file << ":" << L.lineNum
                   << " (" << L.length << " 字符)\n";
    }

    // 冗余 return 语句检查（void 函数末尾的 return;）
    outs() << "  冗余 return: ";
    if (Stats.redundantReturns == 0) {
        outs() << "✓ 未发现\n";
    } else {
        outs() << "⚠ 发现 " << Stats.redundantReturns << " 处\n";
        for (const auto &F : Stats.functions) {
            if (F.hasRedundantReturn) {
                if (F.name == "main")
                    outs() << "    - main(): 末尾 return 0; 是多余的 (C99+ 隐式返回0)\n";
                else
                    outs() << "    - " << F.name << "(): void 函数末尾 return; 是多余的\n";
            }
        }
    }

    // switch 穿透检查
    outs() << "  switch穿透: ";
    if (Stats.fallThroughCount == 0) {
        outs() << "✓ 未发现\n";
    } else {
        outs() << "⚠ 发现 " << Stats.fallThroughCount << " 处\n";
        for (const auto &F : Stats.functions) {
            if (F.hasFallThrough)
                outs() << "    - " << F.name << "(): " << F.fallThroughCount
                       << " 处穿透\n";
        }
    }

    // 死代码检查（CFG 可达性分析：不可达块中的所有语句）
    outs() << "  死代码:     ";
    if (Stats.deadStmts == 0) {
        outs() << "✓ 未发现\n";
    } else {
        outs() << "⚠ 发现 " << Stats.deadStmts << " 条不可达语句\n";
        for (const auto &F : Stats.functions) {
            if (F.hasDeadCode)
                outs() << "    - " << F.name << "(): " << F.deadStmtCount
                       << " 条语句在不可达路径上\n";
        }
    }

    // 空代码块检查（if/for/while/do-while 后空大括号）
    outs() << "  空代码块:   ";
    if (Stats.emptyBlockCount == 0) {
        outs() << "✓ 未发现\n";
    } else {
        outs() << "⚠ 发现 " << Stats.emptyBlockCount << " 处\n";
        for (const auto &B : Stats.emptyBlocks)
            outs() << "    - " << B.funcName << "(): " << B.type << " 空块\n";
    }

    // 圈复杂度检查
    outs() << "  圈复杂度上限(" << MAX_CCN << "): ";
    if (Stats.highCCNFunctions == 0) {
        outs() << "✓ 全部在限制内\n";
    } else {
        outs() << "⚠ 发现 " << Stats.highCCNFunctions << " 个超标\n";
        for (const auto &F : Stats.functions) {
            if (F.isHighCCN)
                outs() << "    - " << F.name << "(): CCN=" << F.ccn
                       << " (超标 " << (F.ccn - MAX_CCN) << ")\n";
        }
    }

    outs() << "\n";

    // ===== 逐函数统计（v2.0 新增）=====
    outs() << "【逐函数统计】\n";
    if (Stats.functions.empty()) {
        outs() << "  (无)\n\n";
    } else {
        for (const auto &F : Stats.functions) {
            outs() << "  ─── " << F.name << "() — " << F.lines << " 行"
                   << ", " << (F.paramCount > 0
                                ? std::to_string(F.paramCount) + " 参数"
                                : "无参")
                   << ", CCN=" << F.ccn
                   << (F.isEmptyOrReturnOnly ? " [空/仅return]" : "")
                   << (F.isOverlong ? " ⚠ 行数超标" : "")
                   << (F.isHighCCN ? " ⚠ 圈复杂度过高" : "")
                   << (F.isBadName ? " ⚠ 命名不规范" : "")
                   << (F.hasTooManyParams ? " ⚠ 参数过多" : "")
                   << (F.hasDeepNesting ? " ⚠ 嵌套过深" : "")
                   << (F.hasDeadCode ? " ⚠ 含死代码" : "")
                   << (F.hasFallThrough ? " ⚠ switch穿透" : "")
                   << " ───\n";
            outs() << "    局部变量: " << F.localVars
                   << "  嵌套深度: " << F.maxNesting << "\n";

            // 分支/循环语句
            outs() << "    分支/循环:";
            if (F.ifCount)      outs() << " if:"      << F.ifCount;
            if (F.forCount)     outs() << " for:"     << F.forCount;
            if (F.whileCount)   outs() << " while:"   << F.whileCount;
            if (F.doWhileCount) outs() << " do-while:"<< F.doWhileCount;
            if (F.switchCount)  outs() << " switch:"  << F.switchCount;
            if (F.caseCount)    outs() << " case:"    << F.caseCount;
            if (F.logicalAndOrCount) outs() << " &&/||:" << F.logicalAndOrCount;
            if (F.conditionalOpCount) outs() << " ?::" << F.conditionalOpCount;
            if (F.breakCount)   outs() << " break:"   << F.breakCount;
            if (F.continueCount)outs() << " continue:"<< F.continueCount;
            if (F.returnCount)  outs() << " return:"  << F.returnCount;
            if (F.gotoCount)    outs() << " goto:"    << F.gotoCount;
            // 如果全部为 0
            if (F.ifCount == 0 && F.forCount == 0 && F.whileCount == 0 &&
                F.doWhileCount == 0 && F.switchCount == 0 && F.caseCount == 0 &&
                F.logicalAndOrCount == 0 && F.conditionalOpCount == 0 &&
                F.breakCount == 0 && F.continueCount == 0 &&
                F.returnCount == 0 && F.gotoCount == 0)
                outs() << " (无)";
            outs() << "\n";

            // 函数调用
            outs() << "    函数调用: " << F.callCount << " 次";
            if (!F.callTargets.empty()) {
                bool first = true;
                for (const auto &p : F.callTargets) {
                    outs() << (first ? " (" : ", ") << p.first << ":" << p.second;
                    first = false;
                }
                outs() << ")";
            }
            outs() << "\n\n";
        }

        // 圈复杂度汇总
        if (!Stats.functions.empty()) {
            int totalCCN = 0;
            int maxCCN = 0;
            std::string maxCCNFunc;
            for (const auto &F : Stats.functions) {
                totalCCN += F.ccn;
                if (F.ccn > maxCCN) {
                    maxCCN = F.ccn;
                    maxCCNFunc = F.name;
                }
            }
            double avgCCN = (double)totalCCN / Stats.functions.size();
            outs() << "  平均圈复杂度: " << format("%.1f", avgCCN);
            outs() << "  最高: " << maxCCNFunc << "() = " << maxCCN << "\n";
        }
    }

    // ===== 全局汇总：控制流 =====
    outs() << "【控制流语句统计（全局汇总）】\n";
    outs() << "  if 语句:       " << Stats.ifCount << "\n";
    outs() << "  for 语句:      " << Stats.forCount << "\n";
    outs() << "  while 语句:    " << Stats.whileCount << "\n";
    outs() << "  do-while 语句: " << Stats.doWhileCount << "\n";
    outs() << "  switch 语句:   " << Stats.switchCount << "\n";
    outs() << "  case 标签:     " << Stats.caseCount << "\n";
    outs() << "  && / || 运算:  " << Stats.logicalAndOrCount << "\n";
    outs() << "  ?: 三元运算:   " << Stats.conditionalOpCount << "\n";
    outs() << "  break 语句:    " << Stats.breakCount << "\n";
    outs() << "  continue 语句: " << Stats.continueCount << "\n";
    outs() << "  return 语句:   " << Stats.returnCount << "\n";
    outs() << "  goto 语句:     " << Stats.gotoCount << "\n";
    int total = Stats.ifCount + Stats.forCount + Stats.whileCount
              + Stats.doWhileCount + Stats.switchCount
              + Stats.caseCount + Stats.logicalAndOrCount
              + Stats.conditionalOpCount
              + Stats.breakCount + Stats.continueCount
              + Stats.returnCount + Stats.gotoCount;
    outs() << "  合计:          " << total << "\n\n";

    outs() << "【函数调用统计（全局汇总）】\n";
    outs() << "  总调用次数: " << Stats.callCount << "\n";
    if (!Stats.callTargets.empty()) {
        outs() << "  调用分布:\n";
        for (const auto &p : Stats.callTargets)
            outs() << "    " << p.first << "(): " << p.second << " 次\n";
    }
    outs() << "\n========================================\n";
}

// ====================== 文本行分析（共用，disk/memory 统一） ======================
static void classifyLines(const std::string &SourceText,
                          const std::string &DisplayName,
                          AnalysisStats &Stats) {
    std::istringstream src(SourceText);
    std::string line;
    int lineNum = 0;
    bool inBlockComment = false;
    while (std::getline(src, line)) {
        lineNum++;
        int len = line.length();
        Stats.totalLines++;

        // 过长单行检测
        if (len > MAX_LINE_LENGTH) {
            Stats.longLineCount++;
            Stats.longLines.push_back({DisplayName, lineNum, len});
        }

        // 行数分类
        size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) {
            Stats.blankLines++;            // 纯空行
            continue;
        }

        if (inBlockComment) {
            Stats.multiCommentLines++;     // 块注释内部
            if (line.find("*/") != std::string::npos)
                inBlockComment = false;
            continue;
        }

        std::string trimmed = line.substr(first);
        if (trimmed.compare(0, 2, "//") == 0) {
            Stats.singleCommentLines++;    // // 单行注释
            continue;
        }

        if (trimmed[0] == '#') {
            Stats.preprocessorLines++;     // #预处理指令
            continue;
        }

        bool isComment = false;
        size_t bs = line.find("/*");
        if (bs != std::string::npos) {
            size_t be = line.find("*/", bs + 2);
            std::string before = line.substr(0, bs);
            bool hasCodeBefore = (before.find_first_not_of(" \t\r") != std::string::npos);
            if (be != std::string::npos) {
                std::string after = line.substr(be + 2);
                bool hasCodeAfter = (after.find_first_not_of(" \t\r") != std::string::npos);
                if (!hasCodeBefore && !hasCodeAfter) {
                    Stats.multiCommentLines++;
                    isComment = true;
                }
            } else {
                if (!hasCodeBefore) {
                    Stats.multiCommentLines++;
                    isComment = true;
                }
                inBlockComment = true;
            }
        }
        if (!isComment)
            Stats.codeLines++;
    }
}

// ====================== 共享 Clang 管线 ======================
// 装配 CompilerInstance → Preprocessor → Sema → ParseAST → AST 遍历
// VFS 必须在调用前设置好，HeaderSearch parent dir 用于 #include "" 解析
static AnalysisResult runClangPipeline(CompilerInstance &CI,
                                        const CompilerInvocation &SharedInvocation,
                                        const std::string &FileName,
                                        const std::string &IncludeParentDir,
                                        IntrusiveRefCntPtr<vfs::FileSystem> VFS = nullptr) {
    AnalysisResult result;

    CI.createDiagnostics();

    // 收集 Clang 诊断（不再丢弃！）
    DiagCollector *diagCollector = new DiagCollector(result);
    CI.getDiagnostics().setClient(diagCollector, /*ShouldOwnClient=*/true);

    // Target
    auto TargetOpts = std::make_shared<clang::TargetOptions>(
        SharedInvocation.getTargetOpts());
    clang::TargetInfo *TI =
        clang::TargetInfo::CreateTargetInfo(CI.getDiagnostics(), TargetOpts);
    CI.setTarget(TI);

    CI.createFileManager(VFS);
    CI.createSourceManager(CI.getFileManager());

    auto File = CI.getFileManager().getFileRef(FileName);
    if (!File) {
        llvm::consumeError(File.takeError());
        result.errors.push_back("File not found: " + FileName);
        return result;
    }

    CI.getSourceManager().setMainFileID(
        CI.getSourceManager().getOrCreateFileID(*File, SrcMgr::C_User));

    // 头文件搜索路径
    CI.getHeaderSearchOpts() = SharedInvocation.getHeaderSearchOpts();
    if (!IncludeParentDir.empty())
        CI.getHeaderSearchOpts().AddPath(IncludeParentDir,
            clang::frontend::System, false, false);

    CI.createPreprocessor(TU_Complete);

    // AST 分析
    CI.setASTConsumer(std::make_unique<MyASTConsumer>(result.stats));
    CI.createASTContext();
    CI.createSema(TU_Complete, nullptr);

    // 记录 ParseAST 前的错误数（排除"File not found"等前置错误）
    size_t preErrorCount = result.errors.size();
    ParseAST(CI.getSema());

    // Clang 报 Error 才标记失败（新增错误 = Clang 诊断错误）
    result.success = (result.errors.size() == preErrorCount);

    return result;
}

// ====================== 分析源码字符串（v6.1 — 程序化入口）======================
static AnalysisResult analyzeSourceCode(const std::string &Code,
                                        const std::string &FileName,
                                        const CompilerInvocation &SharedInvocation) {
    // 内存文件系统
    auto MemFS = llvm::makeIntrusiveRefCnt<vfs::InMemoryFileSystem>();
    MemFS->addFile(FileName, 0,
                   llvm::MemoryBuffer::getMemBuffer(Code, FileName));

    CompilerInstance CI;

    // Clang 管线
    AnalysisResult result = runClangPipeline(CI, SharedInvocation, FileName, "", MemFS);

    // 文本行分析（即使 Clang 报错也做——部分分析仍有价值）
    classifyLines(Code, FileName, result.stats);

    return result;
}

// ====================== 分析磁盘文件 ======================
static AnalysisResult analyzeFile(const std::string &FilePath,
                                  const CompilerInvocation &SharedInvocation) {
    CompilerInstance CI;

    // 真实文件系统
    IntrusiveRefCntPtr<vfs::FileSystem> RealFS = vfs::getRealFileSystem();

    // 父目录用于 #include "" 解析
    std::string ParentDir = llvm::sys::path::parent_path(FilePath).str();

    AnalysisResult result = runClangPipeline(CI, SharedInvocation, FilePath, ParentDir, RealFS);
    if (!result.success && result.errors.size() == 1 &&
        result.errors[0].find("File not found:") == 0)
        return result;  // 文件不存在——直接返回，不做行分析

    // 读文件内容用于文本行分析（即使有编译错误也做）
    std::ifstream src(FilePath);
    if (src.is_open()) {
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        classifyLines(code, FilePath, result.stats);
    }

    return result;
}

// ====================== main ======================
int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv, "Clang C File Analyzer\n");

    if (!ReadFromStdin && InputFiles.empty()) {
        std::cerr << "No input files\n";
        return 1;
    }

    // ===== 一次 CreateFromArgs：共享系统头文件路径 =====
    std::vector<const char *> FakeArgs = {
        "frontendAction",
        "-fsyntax-only",
        InputFiles.empty() ? "stdin.c" : InputFiles[0].c_str(),
    };

    CompilerInstance TmpCI;
    TmpCI.createDiagnostics();

    auto Invocation = std::make_shared<CompilerInvocation>();
    if (!CompilerInvocation::CreateFromArgs(*Invocation, FakeArgs,
                                             TmpCI.getDiagnostics())) {
        std::cerr << "Failed to parse compiler arguments\n";
        return 1;
    }

    // ===== stdin 模式 =====
    if (ReadFromStdin) {
        std::string code((std::istreambuf_iterator<char>(std::cin)),
                          std::istreambuf_iterator<char>());
        if (code.empty()) {
            std::cerr << "No input from stdin\n";
            return 1;
        }
        AnalysisResult result = analyzeSourceCode(code, "<stdin>", *Invocation);
        if (JsonOutput) {
            json::Value json = toJSON(result);
            if (auto *O = json.getAsObject())
                jsonSet(*O, "files") = json::Array({"<stdin>"});
            outs() << formatv("{0:2}", json) << "\n";
        } else {
            if (!result.success) {
                for (const auto &e : result.errors)
                    std::cerr << "Error: " << e << "\n";
            }
            printReport(result.stats, {"<stdin>"});
        }
        return result.success ? 0 : 1;
    }

    // ===== 文件模式 =====
    AnalysisStats totalStats;
    std::vector<std::string> succeeded;
    std::vector<std::string> allErrors;
    for (const auto &f : InputFiles) {
        AnalysisResult result = analyzeFile(f, *Invocation);
        if (result.success) {
            totalStats += result.stats;
            succeeded.push_back(f);
        } else {
            for (const auto &e : result.errors) {
                std::cerr << "Error: " << e << "\n";
                allErrors.push_back(e);
            }
        }
    }

    if (succeeded.empty()) {
        // 如果有部分文件失败但错误已被收集，仍输出空报告
        if (JsonOutput) {
            json::Value json = toJSON(totalStats);
            if (auto *O = json.getAsObject()) {
                jsonSet(*O, "files") = json::Array(succeeded);
                jsonSet(*O, "errors") = json::Array(allErrors);
            }
            outs() << formatv("{0:2}", json) << "\n";
        } else {
            std::cerr << "Failed to analyze any file\n";
        }
        return 1;
    }

    if (JsonOutput) {
        json::Value json = toJSON(totalStats);
        if (auto *O = json.getAsObject()) {
            jsonSet(*O, "files") = json::Array(succeeded);
            if (!allErrors.empty())
                jsonSet(*O, "errors") = json::Array(allErrors);
        }
        outs() << formatv("{0:2}", json) << "\n";
    } else {
        printReport(totalStats, succeeded);
    }
    return 0;
}
