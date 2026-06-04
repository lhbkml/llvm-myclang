#include "llvm/Support/CommandLine.h"
#include "llvm/TargetParser/Host.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Parse/ParseAST.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include <iostream>
#include <map>
#include <string>

using namespace llvm;
using namespace clang;

static cl::opt<std::string>
FileName(cl::Positional, cl::desc("Input file"), cl::Required);

// ====================== 统计数据结构 ======================
struct AnalysisStats {
    int totalFunctions = 0;
    int globalVars = 0;
    int ifCount = 0;
    int forCount = 0;
    int whileCount = 0;
    int callCount = 0;
    std::map<std::string, int> callTargets;        // 被调用函数名 -> 调用次数
    std::vector<std::pair<std::string, int>> funcLines; // 函数名, 行数
};

// ====================== 自定义 AST 访问者 ======================
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
    AnalysisStats &Stats;
    SourceManager *SM = nullptr;

    explicit MyASTVisitor(AnalysisStats &S) : Stats(S) {}

    // 访问函数定义
    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (!FD->isThisDeclarationADefinition())
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
        // 全局变量：声明上下文是 translation unit
        if (VD->getDeclContext()->isTranslationUnit())
            Stats.globalVars++;
        return true;
    }

    // if 语句
    bool VisitIfStmt(IfStmt *IS) {
        Stats.ifCount++;
        return true;
    }

    // for 语句
    bool VisitForStmt(ForStmt *FS) {
        Stats.forCount++;
        return true;
    }

    // while 语句
    bool VisitWhileStmt(WhileStmt *WS) {
        Stats.whileCount++;
        return true;
    }

    // 函数调用
    bool VisitCallExpr(CallExpr *CE) {
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
static void printReport(const AnalysisStats &Stats) {
    outs() << "\n";
    outs() << "========== C 源文件分析报告 ==========\n\n";

    outs() << "【基本信息】\n";
    outs() << "  输入文件: " << FileName << "\n";
    outs() << "  总函数数量: " << Stats.totalFunctions << "\n";
    outs() << "  全局变量数量: " << Stats.globalVars << "\n\n";

    outs() << "【每个函数的代码行数】\n";
    if (Stats.funcLines.empty()) {
        outs() << "  (无)\n";
    } else {
        for (const auto &p : Stats.funcLines)
            outs() << "  " << p.first << "(): " << p.second << " 行\n";
    }
    outs() << "\n";

    outs() << "【控制流语句统计】\n";
    outs() << "  if 语句:    " << Stats.ifCount << "\n";
    outs() << "  for 语句:   " << Stats.forCount << "\n";
    outs() << "  while 语句: " << Stats.whileCount << "\n";
    outs() << "  合计:       " << (Stats.ifCount + Stats.forCount + Stats.whileCount) << "\n\n";

    outs() << "【函数调用统计】\n";
    outs() << "  总调用次数: " << Stats.callCount << "\n";
    if (!Stats.callTargets.empty()) {
        outs() << "  调用分布:\n";
        for (const auto &p : Stats.callTargets)
            outs() << "    " << p.first << "(): " << p.second << " 次\n";
    }
    outs() << "\n========================================\n";
}

// ====================== main ======================
int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv, "Clang C File Analyzer\n");

    CompilerInstance CI;
    CI.createDiagnostics();

    clang::TargetOptions TO;
    TO.Triple = sys::getDefaultTargetTriple();

    clang::TargetInfo* TI = clang::TargetInfo::CreateTargetInfo(CI.getDiagnostics(), TO);
    CI.setTarget(TI);

    IntrusiveRefCntPtr<vfs::FileSystem> VFS = vfs::getRealFileSystem();
    CI.setVirtualFileSystem(VFS);
    CI.createFileManager();
    CI.createSourceManager();

    auto File = CI.getFileManager().getFileRef(FileName);
    if (!File) {
        std::cerr << "File not found\n";
        return 1;
    }

    CI.getSourceManager().setMainFileID(
        CI.getSourceManager().getOrCreateFileID(*File, SrcMgr::C_User)
    );

    CI.createPreprocessor(TU_Complete);

    AnalysisStats Stats;
    CI.setASTConsumer(std::make_unique<MyASTConsumer>(Stats));
    CI.createASTContext();
    CI.createSema(TU_Complete, nullptr);

    ParseAST(CI.getSema());

    printReport(Stats);
    return 0;
}
