#ifndef REPORT_H
#define REPORT_H

#include "core_types.h"

#include "llvm/Support/JSON.h"

#include <map>
#include <string>
#include <vector>

// JSON 序列化（LLVM 18 兼容：强制走 const ObjectKey& 重载）
llvm::json::Value &jsonSet(llvm::json::Object &O, const char *Key);

llvm::json::Value toJSON(const LongLineInfo &L);
llvm::json::Value toJSON(const EmptyBlockInfo &B);
llvm::json::Value toJSON(const FallThroughInfo &F);
llvm::json::Object toJSON(const std::map<std::string, int> &M);
llvm::json::Value toJSON(const FunctionStats &F);
llvm::json::Value toJSON(const AnalysisStats &Stats);
llvm::json::Value toJSON(const AnalysisResult &R);

// 文本报告
void printReport(const AnalysisStats &Stats,
                 const std::vector<std::string> &Files);

// 行分类（代码/空行/注释/预处理）
void classifyLines(const std::string &SourceText,
                   const std::string &DisplayName,
                   AnalysisStats &Stats);

#endif
