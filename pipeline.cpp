#include "pipeline.h"
#include "report.h"
#include "visitor.h"

#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/Path.h"

#include <fstream>

using namespace llvm;
using namespace clang;

// ====================== 共享 Clang 管线 ======================
// 装配 CompilerInstance → Preprocessor → Sema → ParseAST → AST 遍历
AnalysisResult runClangPipeline(CompilerInstance &CI,
                                const CompilerInvocation &SharedInvocation,
                                const std::string &FileName,
                                const std::string &IncludeParentDir,
                                const Thresholds &Thresh,
                                IntrusiveRefCntPtr<vfs::FileSystem> VFS) {
    AnalysisResult result;

    CI.createDiagnostics();

    // 收集 Clang 诊断
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
    CI.setASTConsumer(std::make_unique<MyASTConsumer>(result.stats, Thresh));
    CI.createASTContext();
    CI.createSema(TU_Complete, nullptr);

    // 记录 ParseAST 前的错误数
    size_t preErrorCount = result.errors.size();
    ParseAST(CI.getSema());

    // Clang 报 Error 才标记失败
    result.success = (result.errors.size() == preErrorCount);

    return result;
}

// ====================== 分析源码字符串 ======================
AnalysisResult analyzeSourceCode(const std::string &Code,
                                 const std::string &FileName,
                                 const CompilerInvocation &SharedInvocation,
                                 const Thresholds &Thresh) {
    auto MemFS = llvm::makeIntrusiveRefCnt<vfs::InMemoryFileSystem>();
    MemFS->addFile(FileName, 0,
                   llvm::MemoryBuffer::getMemBuffer(Code, FileName));

    CompilerInstance CI;

    AnalysisResult result = runClangPipeline(CI, SharedInvocation, FileName, "", Thresh, MemFS);

    classifyLines(Code, FileName, result.stats, Thresh);

    return result;
}

// ====================== 分析磁盘文件 ======================
AnalysisResult analyzeFile(const std::string &FilePath,
                           const CompilerInvocation &SharedInvocation,
                           const Thresholds &Thresh) {
    CompilerInstance CI;

    IntrusiveRefCntPtr<vfs::FileSystem> RealFS = vfs::getRealFileSystem();

    std::string ParentDir = llvm::sys::path::parent_path(FilePath).str();

    AnalysisResult result = runClangPipeline(CI, SharedInvocation, FilePath, ParentDir, Thresh, RealFS);
    if (!result.success && result.errors.size() == 1 &&
        result.errors[0].find("File not found:") == 0)
        return result;

    std::ifstream src(FilePath);
    if (src.is_open()) {
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        classifyLines(code, FilePath, result.stats, Thresh);
    }

    return result;
}
