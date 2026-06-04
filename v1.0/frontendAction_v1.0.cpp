#include "llvm/Support/CommandLine.h"
#include "llvm/TargetParser/Host.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Parse/ParseAST.h"
#include "llvm/Support/VirtualFileSystem.h"
// #include "clang/Basic/VirtualFileSystem.h"
#include "clang/AST/ASTConsumer.h"     
#include "clang/AST/RecursiveASTVisitor.h" 
#include <iostream>

using namespace llvm;
using namespace clang;

static cl::opt<std::string>
FileName(cl::Positional, cl::desc("Input file"), cl::Required);


// ====================== 自定义 AST 访问者（想打印什么就打印什么）======================
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
    // 访问函数定义
    bool VisitFunctionDecl(FunctionDecl *FD) {
        llvm::outs() << "【函数】: " << FD->getNameAsString() << "\n";
        return true;
    }

    // 访问变量声明
    bool VisitVarDecl(VarDecl *VD) {
        llvm::outs() << "  【变量】: " << VD->getNameAsString() 
                     << "  【类型】: " << VD->getType().getAsString() << "\n";
        return true;
    }

    // 访问 if 语句
    bool VisitIfStmt(IfStmt *IS) {
        llvm::outs() << "  【if 语句】\n";
        return true;
    }

    // 访问 for 语句
    bool VisitForStmt(ForStmt *FS) {
        
        llvm::outs() << "  【for 循环】\n";
        return true;
    }

    // 访问 return 语句
    bool VisitReturnStmt(ReturnStmt *RS) {
        llvm::outs() << "  【return 语句】\n";
        return true;
    }
};

// ====================== 自定义 AST Consumer ======================
class MyASTConsumer : public ASTConsumer {
public:
    void HandleTranslationUnit(ASTContext &Context) override {
        MyASTVisitor Visitor;
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
};



int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv, "My Clang Tool\n");

    CompilerInstance CI;
    CI.createDiagnostics();

    // 直接用栈对象，不传指针！
    clang::TargetOptions TO;
    TO.Triple = sys::getDefaultTargetTriple();

    // 直接传引用！
    clang::TargetInfo* TI = clang::TargetInfo::CreateTargetInfo(CI.getDiagnostics(), TO);
    CI.setTarget(TI);


    // 1. 获取真实文件系统（来自你给的 VirtualFileSystem.h）
    IntrusiveRefCntPtr<vfs::FileSystem> VFS = vfs::getRealFileSystem();
    
    // 2. 设置给 CompilerInstance
    CI.setVirtualFileSystem(VFS);
    // CI.createVFS();  
    CI.createFileManager();
    // CI.setFileManager(new FileManager(
    //     CI.getDiagnostics().getFileSystemOpts(),
    //     &vfs::getRealFileSystem()
    // ));
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
    CI.setASTConsumer(std::make_unique<MyASTConsumer>());
    CI.createASTContext();
    CI.createSema(TU_Complete, nullptr);

    ParseAST(CI.getSema());
    CI.getASTContext().PrintStats();
    return 0;
}