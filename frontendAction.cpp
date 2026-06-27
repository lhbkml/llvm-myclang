#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/JSON.h"
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

// ====================== main ======================
int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv, "Clang C File Analyzer\n");

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
