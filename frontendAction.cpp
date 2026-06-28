#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"

#include "core_types.h"
#include "pipeline.h"
#include "report.h"

#include <iostream>
#include <string>
#include <vector>

using namespace llvm;
using namespace clang;

// ====================== CLI 选项 ======================
static cl::list<std::string>
InputFiles(cl::Positional, cl::desc("Input files"), cl::ZeroOrMore);

static cl::opt<bool>
JsonOutput("json", cl::desc("Output JSON instead of text report"));

static cl::opt<bool>
ReadFromStdin("stdin", cl::desc("Read source code from standard input"));

static cl::opt<int>
    MaxLines("max-lines", cl::desc("Maximum function lines (default 50)"),
             cl::init(kDefaultThresholds.maxFunctionLines));

static cl::opt<int>
    MaxLineLength("max-line-length",
                  cl::desc("Maximum line length in characters (default 100)"),
                  cl::init(kDefaultThresholds.maxLineLength));

static cl::opt<int>
    MaxCCN("max-ccn", cl::desc("Maximum cyclomatic complexity (default 10)"),
           cl::init(kDefaultThresholds.maxCCN));

static cl::opt<int>
    MaxParams("max-params", cl::desc("Maximum function parameters (default 5)"),
              cl::init(kDefaultThresholds.maxParams));

static cl::opt<int>
    MaxNesting("max-nesting", cl::desc("Maximum nesting depth (default 4)"),
               cl::init(kDefaultThresholds.maxNesting));

static cl::opt<std::string>
    ProjectDir("project", cl::desc("Analyze all .c files in a directory tree"));

// ====================== 目录扫描 ======================
static void collectCFiles(const std::string &Dir, std::vector<std::string> &Files) {
    std::error_code EC;
    for (llvm::sys::fs::directory_iterator It(Dir, EC), End;
         It != End && !EC; It.increment(EC)) {
        if (EC) break;
        llvm::sys::fs::file_status St;
        if (llvm::sys::fs::status(It->path(), St)) continue;
        if (St.type() == llvm::sys::fs::file_type::regular_file) {
            if (StringRef(It->path()).endswith(".c"))
                Files.push_back(It->path().str());
        } else if (St.type() == llvm::sys::fs::file_type::directory_file) {
            StringRef DirName = llvm::sys::path::filename(It->path());
            if (DirName != "." && DirName != "..")
                collectCFiles(It->path().str(), Files);
        }
    }
}

// ====================== main ======================
int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv, "Clang C File Analyzer\n");

    Thresholds thresh;
    thresh.maxFunctionLines = MaxLines;
    thresh.maxLineLength    = MaxLineLength;
    thresh.maxCCN           = MaxCCN;
    thresh.maxParams        = MaxParams;
    thresh.maxNesting       = MaxNesting;

    // ===== 项目模式：递归扫描目录下所有 .c 文件 =====
    if (!ProjectDir.empty()) {
        if (ReadFromStdin) {
            std::cerr << "--project and --stdin are mutually exclusive\n";
            return 1;
        }

        std::vector<std::string> projectFiles;
        collectCFiles(ProjectDir, projectFiles);

        if (projectFiles.empty()) {
            std::cerr << "No .c files found in " << ProjectDir << "\n";
            return 1;
        }

        std::cerr << "Found " << projectFiles.size() << " .c file(s) in "
                  << ProjectDir << "\n";

        std::vector<const char *> FakeArgs = {
            "frontendAction",
            "-fsyntax-only",
            projectFiles[0].c_str(),
        };

        CompilerInstance TmpCI;
        TmpCI.createDiagnostics();

        auto Invocation = std::make_shared<CompilerInvocation>();
        if (!CompilerInvocation::CreateFromArgs(*Invocation, FakeArgs,
                                                 TmpCI.getDiagnostics())) {
            std::cerr << "Failed to parse compiler arguments\n";
            return 1;
        }

        AnalysisStats totalStats;
        std::vector<std::string> succeeded;
        std::vector<std::string> allErrors;

        for (const auto &f : projectFiles) {
            AnalysisResult result = analyzeFile(f, *Invocation, thresh);
            if (result.success) {
                totalStats += result.stats;
                succeeded.push_back(f);
            } else {
                std::cerr << "[FAIL] " << f << "\n";
                for (const auto &e : result.errors) {
                    allErrors.push_back(f + ": " + e);
                }
            }
        }

        std::cerr << "Analyzed " << succeeded.size() << "/" << projectFiles.size()
                  << " file(s) successfully\n";

        if (succeeded.empty()) {
            if (JsonOutput) {
                json::Value json = toJSON(totalStats, thresh);
                if (auto *O = json.getAsObject()) {
                    jsonSet(*O, "files") = json::Array(succeeded);
                    jsonSet(*O, "errors") = json::Array(allErrors);
                    jsonSet(*O, "project") = ProjectDir;
                }
                outs() << formatv("{0:2}", json) << "\n";
            } else {
                std::cerr << "Failed to analyze any file\n";
            }
            return 1;
        }

        if (JsonOutput) {
            json::Value json = toJSON(totalStats, thresh);
            if (auto *O = json.getAsObject()) {
                jsonSet(*O, "files") = json::Array(succeeded);
                jsonSet(*O, "project") = ProjectDir;
                if (!allErrors.empty())
                    jsonSet(*O, "errors") = json::Array(allErrors);
            }
            outs() << formatv("{0:2}", json) << "\n";
        } else {
            outs() << "Project: " << ProjectDir << "\n";
            printReport(totalStats, succeeded, thresh);
        }

        return 0;
    }

    // ===== 单文件模式 =====
    if (!ReadFromStdin && InputFiles.empty()) {
        std::cerr << "No input files\n";
        return 1;
    }

    // 一次 CreateFromArgs：共享系统头文件路径
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
        AnalysisResult result = analyzeSourceCode(code, "<stdin>", *Invocation, thresh);
        if (JsonOutput) {
            json::Value json = toJSON(result, thresh);
            if (auto *O = json.getAsObject())
                jsonSet(*O, "files") = json::Array({"<stdin>"});
            outs() << formatv("{0:2}", json) << "\n";
        } else {
            if (!result.success) {
                for (const auto &e : result.errors)
                    std::cerr << "Error: " << e << "\n";
            }
            printReport(result.stats, {"<stdin>"}, thresh);
        }
        return result.success ? 0 : 1;
    }

    // ===== 文件模式 =====
    AnalysisStats totalStats;
    std::vector<std::string> succeeded;
    std::vector<std::string> allErrors;
    for (const auto &f : InputFiles) {
        AnalysisResult result = analyzeFile(f, *Invocation, thresh);
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
        if (JsonOutput) {
            json::Value json = toJSON(totalStats, thresh);
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
        json::Value json = toJSON(totalStats, thresh);
        if (auto *O = json.getAsObject()) {
            jsonSet(*O, "files") = json::Array(succeeded);
            if (!allErrors.empty())
                jsonSet(*O, "errors") = json::Array(allErrors);
        }
        outs() << formatv("{0:2}", json) << "\n";
    } else {
        printReport(totalStats, succeeded, thresh);
    }

    return 0;
}
