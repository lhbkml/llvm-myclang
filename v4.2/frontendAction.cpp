#include "llvm/Support/CommandLine.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Parse/ParseAST.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Format.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CFG.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace llvm;
using namespace clang;

static cl::list<std::string>
InputFiles(cl::Positional, cl::desc("Input files"), cl::OneOrMore);

// 代码规范检查阈值
static const int MAX_FUNCTION_LINES = 50;
static const int MAX_LINE_LENGTH = 100;

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
    bool hasRedundantReturn = false;                    // 冗余 return; (void函数末尾)
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
    int badGlobalVarNames = 0;                          // 全局变量缺 g_ 前缀
    std::vector<std::string> badGlobalVars;             // 问题全局变量名列表
    int longLineCount = 0;                              // 过长单行(>100字符)
    std::vector<LongLineInfo> longLines;                // 过长单行明细
    int redundantReturns = 0;                           // 冗余 return; 语句
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
        badGlobalVarNames  += Other.badGlobalVarNames;
        badGlobalVars.insert(badGlobalVars.end(),
                             Other.badGlobalVars.begin(),
                             Other.badGlobalVars.end());
        longLineCount += Other.longLineCount;
        longLines.insert(longLines.end(),
                         Other.longLines.begin(), Other.longLines.end());
        redundantReturns  += Other.redundantReturns;
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

// ====================== 自定义预处理回调（计数 #include）======================
class IncludeCounter : public PPCallbacks {
public:
    int &Count;
    explicit IncludeCounter(int &C) : Count(C) {}

    void InclusionDirective(SourceLocation HashLoc,
                            const Token &IncludeTok,
                            StringRef FileName,
                            bool IsAngled,
                            CharSourceRange FilenameRange,
                            OptionalFileEntryRef File,
                            StringRef SearchPath,
                            StringRef RelativePath,
                            const Module *SuggestedModule,
                            bool ModuleImported,
                            SrcMgr::CharacteristicKind FileType) override {
        Count++;
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

    explicit MyASTVisitor(AnalysisStats &S) : Stats(S) {}

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

                // 检测冗余 return（函数末尾不必要的 return 语句）
                if (auto *CS = dyn_cast<CompoundStmt>(Body)) {
                    if (!CS->body_empty()) {
                        auto it = CS->body_end();
                        --it;
                        if (auto *RS = dyn_cast<ReturnStmt>(*it)) {
                            // 情况1：void 函数末尾的 return;
                            if (FD->getReturnType()->isVoidType() &&
                                !RS->getRetValue()) {
                                FS.hasRedundantReturn = true;
                            }
                            // 情况2：main() 末尾的 return 0; (C99+ 隐式返回0)
                            else if (FS.name == "main" &&
                                     FD->getReturnType()->isIntegerType()) {
                                if (auto *IL = dyn_cast<IntegerLiteral>(
                                        RS->getRetValue())) {
                                    if (IL->getValue() == 0)
                                        FS.hasRedundantReturn = true;
                                }
                            }
                        }
                    }
                }
            }

            FS.paramCount = FD->getNumParams();

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
            }
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

        if (CurrentFunc && CurrentFunc->hasRedundantReturn)
            Stats.redundantReturns++;

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

    // 空代码块检查（if/for/while/do-while 后空大括号）
    outs() << "  空代码块:   ";
    if (Stats.emptyBlockCount == 0) {
        outs() << "✓ 未发现\n";
    } else {
        outs() << "⚠ 发现 " << Stats.emptyBlockCount << " 处\n";
        for (const auto &B : Stats.emptyBlocks)
            outs() << "    - " << B.funcName << "(): " << B.type << " 空块\n";
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
                   << (F.isBadName ? " ⚠ 命名不规范" : "")
                   << " ───\n";
            outs() << "    局部变量: " << F.localVars << "\n";

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

// ====================== 分析单个文件 ======================
static bool analyzeFile(const std::string &FilePath,
                        const CompilerInvocation &SharedInvocation,
                        AnalysisStats &Stats) {
    CompilerInstance CI;
    CI.createDiagnostics();
    CI.getDiagnostics().setClient(
        new clang::IgnoringDiagConsumer(), /*ShouldOwnClient=*/true);

    // Target
    auto TargetOpts = SharedInvocation.getTargetOpts();
    clang::TargetInfo *TI =
        clang::TargetInfo::CreateTargetInfo(CI.getDiagnostics(), TargetOpts);
    CI.setTarget(TI);

    // 文件系统
    IntrusiveRefCntPtr<vfs::FileSystem> VFS = vfs::getRealFileSystem();
    CI.setVirtualFileSystem(VFS);
    CI.createFileManager();
    CI.createSourceManager();

    auto File = CI.getFileManager().getFileRef(FilePath);
    if (!File) {
        std::cerr << "File not found: " << FilePath << "\n";
        return false;
    }

    CI.getSourceManager().setMainFileID(
        CI.getSourceManager().getOrCreateFileID(*File, SrcMgr::C_User));

    // 头文件路径：共享的系统路径 + 当前文件的父目录（支持相对 #include ""）
    CI.getHeaderSearchOpts() = SharedInvocation.getHeaderSearchOpts();
    StringRef ParentDir = llvm::sys::path::parent_path(FilePath);
    if (!ParentDir.empty())
        CI.getHeaderSearchOpts().AddPath(ParentDir,
            clang::frontend::System, false, false);

    CI.createPreprocessor(TU_Complete);

    // 注册预处理回调
    CI.getPreprocessor().addPPCallbacks(
        std::make_unique<IncludeCounter>(Stats.includeCount));

    // AST 分析
    CI.setASTConsumer(std::make_unique<MyASTConsumer>(Stats));
    CI.createASTContext();
    CI.createSema(TU_Complete, nullptr);

    ParseAST(CI.getSema());

    // 文本级分析：行数分类 + 长行检测
    std::ifstream src(FilePath);
    if (src.is_open()) {
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
                Stats.longLines.push_back({FilePath, lineNum, len});
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
                    // 同行闭合：/* ... */ 有前后代码 → 代码行
                    std::string after = line.substr(be + 2);
                    bool hasCodeAfter = (after.find_first_not_of(" \t\r") != std::string::npos);
                    if (!hasCodeBefore && !hasCodeAfter) {
                        Stats.multiCommentLines++;
                        isComment = true;
                    }
                } else {
                    // 多行块注释开始
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

    return true;
}

// ====================== main ======================
int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv, "Clang C File Analyzer\n");

    if (InputFiles.empty()) {
        std::cerr << "No input files\n";
        return 1;
    }

    // ===== 一次 CreateFromArgs：共享所有文件的系统头文件路径 =====
    std::vector<const char *> FakeArgs = {
        "frontendAction",
        "-fsyntax-only",
        InputFiles[0].c_str(),   // 用第一个文件做探测
    };

    CompilerInstance TmpCI;
    TmpCI.createDiagnostics();

    auto Invocation = std::make_shared<CompilerInvocation>();
    if (!CompilerInvocation::CreateFromArgs(*Invocation, FakeArgs,
                                             TmpCI.getDiagnostics())) {
        std::cerr << "Failed to parse compiler arguments\n";
        return 1;
    }

    // ===== 逐个分析文件，累积统计 =====
    AnalysisStats totalStats;
    std::vector<std::string> succeeded;
    for (const auto &f : InputFiles) {
        AnalysisStats fileStats;
        if (analyzeFile(f, *Invocation, fileStats)) {
            totalStats += fileStats;
            succeeded.push_back(f);
        }
    }

    if (succeeded.empty()) {
        std::cerr << "Failed to analyze any file\n";
        return 1;
    }

    printReport(totalStats, succeeded);
    return 0;
}
