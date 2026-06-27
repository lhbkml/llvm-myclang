#ifndef PIPELINE_H
#define PIPELINE_H

#include "core_types.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "llvm/Support/VirtualFileSystem.h"

// Clang 编译管线：装配 CompilerInstance → 解析 → AST 遍历
AnalysisResult runClangPipeline(
    clang::CompilerInstance &CI,
    const clang::CompilerInvocation &SharedInvocation,
    const std::string &FileName,
    const std::string &IncludeParentDir,
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> VFS = nullptr);

// 分析内存源码字符串（程序化入口）
AnalysisResult analyzeSourceCode(
    const std::string &Code,
    const std::string &FileName,
    const clang::CompilerInvocation &SharedInvocation);

// 分析磁盘文件
AnalysisResult analyzeFile(
    const std::string &FilePath,
    const clang::CompilerInvocation &SharedInvocation);

#endif
