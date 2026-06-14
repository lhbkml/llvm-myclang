#include "llvm/Support/CommandLine.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Parse/ParseAST.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/Path.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace llvm;
using namespace clang;

static cl::list<std::string>
InputFiles(cl::Positional, cl::desc("Input files"), cl::OneOrMore);

// ====================== 统计数据结构 ======================
struct AnalysisStats {
    int totalFunctions = 0;
    int globalVars = 0;
    int localVars = 0;
    int ifCount = 0;
    int forCount = 0;
    int whileCount = 0;
    int doWhileCount = 0;
    int switchCount = 0;
    int breakCount = 0;
    int continueCount = 0;
    int returnCount = 0;
    int gotoCount = 0;
    int callCount = 0;
    int includeCount = 0;
    std::map<std::string, int> callTargets;        // 被调用函数名 -> 调用次数
    std::vector<std::pair<std::string, int>> funcLines; // 函数名, 行数

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
        breakCount     += Other.breakCount;
        continueCount  += Other.continueCount;
        returnCount    += Other.returnCount;
        gotoCount      += Other.gotoCount;
        callCount      += Other.callCount;
        includeCount   += Other.includeCount;
        for (const auto &p : Other.callTargets)
            callTargets[p.first] += p.second;
        funcLines.insert(funcLines.end(),
                         Other.funcLines.begin(), Other.funcLines.end());
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

    // 访问函数定义
    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (!FD->isThisDeclarationADefinition())
            return true;
        if (!isFromMainFile(FD))   // 跳过 #include 头文件中的函数
            return true;

        Stats.totalFunctions++;

        // 计算函数体行数
        if (SM && FD->hasBody()) {
            Stmt *Body = FD->getBody();
            unsigned startLine = SM->getExpansionLineNumber(FD->getBeginLoc());
            unsigned endLine   = SM->getExpansionLineNumber(Body->getEndLoc());
            int lines = (endLine >= startLine) ? (endLine - startLine + 1) : 0;
            Stats.funcLines.push_back({FD->getNameAsString(), lines});
        }

        return true;
    }

    // 访问变量声明
    bool VisitVarDecl(VarDecl *VD) {
        if (!isFromMainFile(VD))        // 只统计主文件
            return true;
        if (isa<ParmVarDecl>(VD))       // 跳过函数参数
            return true;

        if (VD->getDeclContext()->isTranslationUnit())
            Stats.globalVars++;         // 全局变量
        else
            Stats.localVars++;          // 局部变量（含静态局部变量）
        return true;
    }

    // if 语句
    bool VisitIfStmt(IfStmt *IS) {
        if (isStmtFromMainFile(IS)) Stats.ifCount++;
        return true;
    }

    // for 语句
    bool VisitForStmt(ForStmt *FS) {
        if (isStmtFromMainFile(FS)) Stats.forCount++;
        return true;
    }

    // while 语句
    bool VisitWhileStmt(WhileStmt *WS) {
        if (isStmtFromMainFile(WS)) Stats.whileCount++;
        return true;
    }

    // do-while 语句
    bool VisitDoStmt(DoStmt *DS) {
        if (isStmtFromMainFile(DS)) Stats.doWhileCount++;
        return true;
    }

    // switch 语句
    bool VisitSwitchStmt(SwitchStmt *SS) {
        if (isStmtFromMainFile(SS)) Stats.switchCount++;
        return true;
    }

    // break 语句
    bool VisitBreakStmt(BreakStmt *BS) {
        if (isStmtFromMainFile(BS)) Stats.breakCount++;
        return true;
    }

    // continue 语句
    bool VisitContinueStmt(ContinueStmt *CS) {
        if (isStmtFromMainFile(CS)) Stats.continueCount++;
        return true;
    }

    // return 语句
    bool VisitReturnStmt(ReturnStmt *RS) {
        if (isStmtFromMainFile(RS)) Stats.returnCount++;
        return true;
    }

    // goto 语句
    bool VisitGotoStmt(GotoStmt *GS) {
        if (isStmtFromMainFile(GS)) Stats.gotoCount++;
        return true;
    }

    // 函数调用
    bool VisitCallExpr(CallExpr *CE) {
        if (!isStmtFromMainFile(CE)) return true;
        Stats.callCount++;
        if (FunctionDecl *Callee = CE->getDirectCallee())
            Stats.callTargets[Callee->getNameAsString()]++;
        else
            Stats.callTargets["<indirect>"]++;
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
    outs() << "  全局变量数量: " << Stats.globalVars << "\n";
    outs() << "  局部变量数量: " << Stats.localVars << "\n";
    outs() << "  #include 数量: " << Stats.includeCount << "\n\n";

    outs() << "【每个函数的代码行数】\n";
    if (Stats.funcLines.empty()) {
        outs() << "  (无)\n";
    } else {
        for (const auto &p : Stats.funcLines)
            outs() << "  " << p.first << "(): " << p.second << " 行\n";
    }
    outs() << "\n";

    outs() << "【控制流语句统计】\n";
    outs() << "  if 语句:       " << Stats.ifCount << "\n";
    outs() << "  for 语句:      " << Stats.forCount << "\n";
    outs() << "  while 语句:    " << Stats.whileCount << "\n";
    outs() << "  do-while 语句: " << Stats.doWhileCount << "\n";
    outs() << "  switch 语句:   " << Stats.switchCount << "\n";
    outs() << "  break 语句:    " << Stats.breakCount << "\n";
    outs() << "  continue 语句: " << Stats.continueCount << "\n";
    outs() << "  return 语句:   " << Stats.returnCount << "\n";
    outs() << "  goto 语句:     " << Stats.gotoCount << "\n";
    int total = Stats.ifCount + Stats.forCount + Stats.whileCount
              + Stats.doWhileCount + Stats.switchCount
              + Stats.breakCount + Stats.continueCount
              + Stats.returnCount + Stats.gotoCount;
    outs() << "  合计:          " << total << "\n\n";

    outs() << "【函数调用统计】\n";
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
