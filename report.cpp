#include "report.h"

#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <sstream>

using namespace llvm;

// ====================== JSON 序列化 ======================

json::Value &jsonSet(json::Object &O, const char *Key) {
    const json::ObjectKey k(Key);
    return O[k];
}

json::Value toJSON(const LongLineInfo &L) {
    return json::Object{{"file", L.file}, {"line", L.lineNum}, {"length", L.length}};
}

json::Value toJSON(const EmptyBlockInfo &B) {
    return json::Object{{"function", B.funcName}, {"type", B.type}};
}

json::Value toJSON(const FallThroughInfo &F) {
    return json::Object{{"function", F.funcName},
                         {"fromCase", F.fromCase},
                         {"toCase", F.toCase}};
}

json::Object toJSON(const std::map<std::string, int> &M) {
    json::Object O;
    for (const auto &P : M)
        O[P.first] = P.second;
    return O;
}

json::Value toJSON(const FunctionStats &F) {
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

json::Value toJSON(const AnalysisStats &Stats, const Thresholds &Thresh) {
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

    auto violationList = [&](const auto &Items) {
        return json::Object{{"count", (int)Items.size()}, {"items", json::Array(Items)}};
    };

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
        {"thresholds",
         json::Object{
             {"maxFunctionLines", Thresh.maxFunctionLines},
             {"maxLineLength", Thresh.maxLineLength},
             {"maxCCN", Thresh.maxCCN},
             {"maxParams", Thresh.maxParams},
             {"maxNesting", Thresh.maxNesting},
         }},
    };

    return Root;
}

json::Value toJSON(const AnalysisResult &R, const Thresholds &Thresh) {
    json::Value json = toJSON(R.stats, Thresh);
    if (auto *O = json.getAsObject()) {
        if (!R.errors.empty())
            jsonSet(*O, "errors") = json::Array(R.errors);
        if (!R.warnings.empty())
            jsonSet(*O, "warnings") = json::Array(R.warnings);
        jsonSet(*O, "success") = R.success;
    }
    return json;
}

// ====================== 文本报告 ======================

void printReport(const AnalysisStats &Stats,
                 const std::vector<std::string> &Files,
                 const Thresholds &Thresh) {
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

    // 代码行数统计
    outs() << "【代码行数统计】\n";
    outs() << "  总行数:     " << Stats.totalLines << "\n";
    outs() << "  代码行:     " << Stats.codeLines << "\n";
    outs() << "  空行:       " << Stats.blankLines << "\n";
    outs() << "  预处理指令: " << Stats.preprocessorLines << "\n";
    int totalComment = Stats.singleCommentLines + Stats.multiCommentLines;
    outs() << "  注释行:     " << totalComment << "\n";
    outs() << "    - 单行(//): " << Stats.singleCommentLines << "\n";
    outs() << "    - 多行(/**/): " << Stats.multiCommentLines << "\n\n";

    // 代码规范检查
    outs() << "【代码规范检查】\n";
    outs() << "  函数行数上限(" << Thresh.maxFunctionLines << "行): ";
    if (Stats.overlongFunctions == 0) {
        outs() << "= 全部在限制内\n";
    } else {
        outs() << "= 发现 " << Stats.overlongFunctions << " 个超标\n";
        for (const auto &F : Stats.functions) {
            if (F.isOverlong)
                outs() << "    - " << F.name << "(): " << F.lines << " 行 (超标 "
                        << (F.lines - Thresh.maxFunctionLines) << " 行)\n";
        }
    }
    outs() << "\n";

    outs() << "  goto 语句:   ";
    if (Stats.gotoCount == 0) {
        outs() << "= 未使用\n";
    } else {
        outs() << "= 发现 " << Stats.gotoCount << " 处\n";
        for (const auto &F : Stats.functions) {
            if (F.gotoCount > 0)
                outs() << "    - " << F.name << "(): " << F.gotoCount << " 处\n";
        }
    }

    outs() << "  函数参数上限(" << Thresh.maxParams << "个): ";
    if (Stats.tooManyParamsFunctions == 0) {
        outs() << "= 全部在限制内\n";
    } else {
        outs() << "= 发现 " << Stats.tooManyParamsFunctions << " 个超标\n";
        for (const auto &F : Stats.functions) {
            if (F.hasTooManyParams)
                outs() << "    - " << F.name << "(): " << F.paramCount
                       << " 个参数 (超标 " << (F.paramCount - Thresh.maxParams) << " 个)\n";
        }
    }

    outs() << "  嵌套深度上限(" << Thresh.maxNesting << "): ";
    if (Stats.deepNestingFunctions == 0) {
        outs() << "= 全部在限制内\n";
    } else {
        outs() << "= 发现 " << Stats.deepNestingFunctions << " 个超标\n";
        for (const auto &F : Stats.functions) {
            if (F.hasDeepNesting)
                outs() << "    - " << F.name << "(): 最大嵌套 " << F.maxNesting
                       << " 层 (超标 " << (F.maxNesting - Thresh.maxNesting) << " 层)\n";
        }
    }

    outs() << "  命名规范:   ";
    if (Stats.badNamedFunctions == 0) {
        outs() << "= 全部符合 snake_case\n";
    } else {
        outs() << "= 发现 " << Stats.badNamedFunctions << " 个不规范\n";
        for (const auto &F : Stats.functions) {
            if (F.isBadName)
                outs() << "    - " << F.name << "(): 应使用小写下划线风格\n";
        }
    }

    outs() << "  全局变量命名: ";
    if (Stats.badGlobalVarNames == 0) {
        outs() << "= 全部有 g_ 前缀\n";
    } else {
        outs() << "= 发现 " << Stats.badGlobalVarNames << " 个缺少 g_ 前缀\n";
        for (const auto &name : Stats.badGlobalVars)
            outs() << "    - " << name << " -> 应改为 g_" << name << "\n";
    }

    outs() << "  单行长度(" << Thresh.maxLineLength << "字符): ";
    if (Stats.longLineCount == 0) {
        outs() << "= 全部在限制内\n";
    } else {
        outs() << "= 发现 " << Stats.longLineCount << " 行超标\n";
        for (const auto &L : Stats.longLines)
            outs() << "    - " << L.file << ":" << L.lineNum
                   << " (" << L.length << " 字符)\n";
    }

    outs() << "  冗余 return: ";
    if (Stats.redundantReturns == 0) {
        outs() << "= 未发现\n";
    } else {
        outs() << "= 发现 " << Stats.redundantReturns << " 处\n";
        for (const auto &F : Stats.functions) {
            if (F.hasRedundantReturn) {
                if (F.name == "main")
                    outs() << "    - main(): 末尾 return 0; 是多余的 (C99+ 隐式返回0)\n";
                else
                    outs() << "    - " << F.name << "(): void 函数末尾 return; 是多余的\n";
            }
        }
    }

    outs() << "  switch穿透: ";
    if (Stats.fallThroughCount == 0) {
        outs() << "= 未发现\n";
    } else {
        outs() << "= 发现 " << Stats.fallThroughCount << " 处\n";
        for (const auto &F : Stats.functions) {
            if (F.hasFallThrough)
                outs() << "    - " << F.name << "(): " << F.fallThroughCount
                       << " 处穿透\n";
        }
    }

    outs() << "  死代码:     ";
    if (Stats.deadStmts == 0) {
        outs() << "= 未发现\n";
    } else {
        outs() << "= 发现 " << Stats.deadStmts << " 条不可达语句\n";
        for (const auto &F : Stats.functions) {
            if (F.hasDeadCode)
                outs() << "    - " << F.name << "(): " << F.deadStmtCount
                       << " 条语句在不可达路径上\n";
        }
    }

    outs() << "  空代码块:   ";
    if (Stats.emptyBlockCount == 0) {
        outs() << "= 未发现\n";
    } else {
        outs() << "= 发现 " << Stats.emptyBlockCount << " 处\n";
        for (const auto &B : Stats.emptyBlocks)
            outs() << "    - " << B.funcName << "(): " << B.type << " 空块\n";
    }

    outs() << "  圈复杂度上限(" << Thresh.maxCCN << "): ";
    if (Stats.highCCNFunctions == 0) {
        outs() << "= 全部在限制内\n";
    } else {
        outs() << "= 发现 " << Stats.highCCNFunctions << " 个超标\n";
        for (const auto &F : Stats.functions) {
            if (F.isHighCCN)
                outs() << "    - " << F.name << "(): CCN=" << F.ccn
                       << " (超标 " << (F.ccn - Thresh.maxCCN) << ")\n";
        }
    }

    outs() << "\n";

    // 逐函数统计
    outs() << "【逐函数统计】\n";
    if (Stats.functions.empty()) {
        outs() << "  (无)\n\n";
    } else {
        for (const auto &F : Stats.functions) {
            outs() << "  --- " << F.name << "() -- " << F.lines << " 行"
                   << ", " << (F.paramCount > 0
                                ? std::to_string(F.paramCount) + " 参数"
                                : "无参")
                   << ", CCN=" << F.ccn
                   << (F.isEmptyOrReturnOnly ? " [空/仅return]" : "")
                   << (F.isOverlong ? " = 行数超标" : "")
                   << (F.isHighCCN ? " = 圈复杂度过高" : "")
                   << (F.isBadName ? " = 命名不规范" : "")
                   << (F.hasTooManyParams ? " = 参数过多" : "")
                   << (F.hasDeepNesting ? " = 嵌套过深" : "")
                   << (F.hasDeadCode ? " = 含死代码" : "")
                   << (F.hasFallThrough ? " = switch穿透" : "")
                   << " ---\n";
            outs() << "    局部变量: " << F.localVars
                   << "  嵌套深度: " << F.maxNesting << "\n";

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
            if (F.ifCount == 0 && F.forCount == 0 && F.whileCount == 0 &&
                F.doWhileCount == 0 && F.switchCount == 0 && F.caseCount == 0 &&
                F.logicalAndOrCount == 0 && F.conditionalOpCount == 0 &&
                F.breakCount == 0 && F.continueCount == 0 &&
                F.returnCount == 0 && F.gotoCount == 0)
                outs() << " (无)";
            outs() << "\n";

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

    // 全局汇总
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

// ====================== 行分类 ======================

void classifyLines(const std::string &SourceText,
                   const std::string &DisplayName,
                   AnalysisStats &Stats,
                   const Thresholds &Thresh) {
    std::istringstream src(SourceText);
    std::string line;
    int lineNum = 0;
    bool inBlockComment = false;
    while (std::getline(src, line)) {
        lineNum++;
        int len = line.length();
        Stats.totalLines++;

        if (len > Thresh.maxLineLength) {
            Stats.longLineCount++;
            Stats.longLines.push_back({DisplayName, lineNum, len});
        }

        size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) {
            Stats.blankLines++;
            continue;
        }

        if (inBlockComment) {
            Stats.multiCommentLines++;
            if (line.find("*/") != std::string::npos)
                inBlockComment = false;
            continue;
        }

        std::string trimmed = line.substr(first);
        if (trimmed.compare(0, 2, "//") == 0) {
            Stats.singleCommentLines++;
            continue;
        }

        if (trimmed[0] == '#') {
            Stats.preprocessorLines++;
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

// ====================== 项目级跨文件分析 ======================

ProjectReport buildProjectReport(
    const std::vector<std::string> &Files,
    const std::vector<AnalysisStats> &FileStats) {

    ProjectReport PR;

    // 1. 收集所有函数定义（函数名 → 文件）
    for (size_t i = 0; i < Files.size() && i < FileStats.size(); ++i) {
        for (const auto &F : FileStats[i].functions) {
            if (!F.name.empty()) {
                PR.allDefs[F.name].push_back(Files[i]);
            }
        }
    }

    // 单文件定义映射
    for (const auto &P : PR.allDefs) {
        if (P.second.size() == 1)
            PR.funcDefs[P.first] = P.second[0];
        else
            PR.duplicateFuncs.push_back(P.first);
    }

    // 2. 收集所有函数调用，构建跨文件调用图
    for (size_t i = 0; i < Files.size() && i < FileStats.size(); ++i) {
        for (const auto &F : FileStats[i].functions) {
            if (F.name.empty()) continue;
            for (const auto &C : F.callTargets) {
                if (C.first == "<indirect>") continue;
                PR.callGraph[F.name].push_back(C.first);
                // 跨文件调用
                auto defIt = PR.funcDefs.find(C.first);
                if (defIt != PR.funcDefs.end() && defIt->second != Files[i]) {
                    std::string edge = F.name + "@" + Files[i] +
                                       " → " + C.first + "@" + defIt->second;
                    PR.crossFileCalls.push_back(edge);
                }
            }
        }
    }

    // 3. 检测问题
    std::set<std::string> calledSet;
    for (const auto &P : PR.callGraph)
        for (const auto &C : P.second)
            calledSet.insert(C);

    // 未定义引用：被调用但不在任何一个文件中定义
    for (const auto &callee : calledSet) {
        if (PR.allDefs.find(callee) == PR.allDefs.end())
            PR.undefinedRefs.push_back(callee);
    }

    // 未被调用的函数：定义了但从未被调用
    for (const auto &P : PR.allDefs) {
        if (calledSet.find(P.first) == calledSet.end())
            PR.uncalledFuncs.push_back(P.first);
    }

    return PR;
}

void printProjectReport(const ProjectReport &PR) {
    outs() << "\n========== 跨文件分析报告 ==========\n\n";

    outs() << "【函数定义统计】\n";
    outs() << "  总函数定义数: " << PR.allDefs.size() << "\n";

    // 跨文件调用
    outs() << "\n【跨文件调用】\n";
    if (PR.crossFileCalls.empty()) {
        outs() << "  未发现跨文件调用\n";
    } else {
        outs() << "  共 " << PR.crossFileCalls.size() << " 处:\n";
        for (const auto &s : PR.crossFileCalls)
            outs() << "    " << s << "\n";
    }

    // 未定义引用
    outs() << "\n【未定义引用（外部/库函数）】\n";
    if (PR.undefinedRefs.empty()) {
        outs() << "  未发现\n";
    } else {
        outs() << "  共 " << PR.undefinedRefs.size() << " 个:\n";
        for (const auto &f : PR.undefinedRefs)
            outs() << "    " << f << "()\n";
    }

    // 未被调用的函数
    outs() << "\n【未被调用的函数】\n";
    if (PR.uncalledFuncs.empty()) {
        outs() << "  全部函数都有调用关系\n";
    } else {
        outs() << "  共 " << PR.uncalledFuncs.size() << " 个:\n";
        for (const auto &f : PR.uncalledFuncs) {
            auto it = PR.allDefs.find(f);
            std::string loc = (it != PR.allDefs.end() && !it->second.empty())
                              ? " (" + it->second[0] + ")" : "";
            outs() << "    " << f << "()" << loc << "\n";
        }
    }

    // 重复定义
    outs() << "\n【多文件重复定义】\n";
    if (PR.duplicateFuncs.empty()) {
        outs() << "  未发现\n";
    } else {
        outs() << "  共 " << PR.duplicateFuncs.size() << " 个:\n";
        for (const auto &f : PR.duplicateFuncs) {
            auto it = PR.allDefs.find(f);
            if (it != PR.allDefs.end()) {
                outs() << "    " << f << "(): ";
                for (size_t i = 0; i < it->second.size(); ++i) {
                    if (i > 0) outs() << ", ";
                    outs() << it->second[i];
                }
                outs() << "\n";
            }
        }
    }

    outs() << "\n========================================\n";
}

json::Value toJSON(const ProjectReport &PR) {
    json::Object O;

    O["totalFunctions"] = (int)PR.allDefs.size();
    O["crossFileCalls"] = json::Array(PR.crossFileCalls);
    O["undefinedRefs"] = json::Array(PR.undefinedRefs);

    // uncalled functions with file info
    std::vector<json::Value> uncalledItems;
    for (const auto &f : PR.uncalledFuncs) {
        auto it = PR.allDefs.find(f);
        std::string file = (it != PR.allDefs.end() && !it->second.empty())
                           ? it->second[0] : "";
        uncalledItems.push_back(json::Object{{"name", f}, {"file", file}});
    }
    O["uncalledFuncs"] = json::Object{
        {"count", (int)PR.uncalledFuncs.size()},
        {"items", json::Array(uncalledItems)},
    };

    // duplicate definitions
    std::vector<json::Value> dupItems;
    for (const auto &f : PR.duplicateFuncs) {
        auto it = PR.allDefs.find(f);
        std::vector<std::string> locs;
        if (it != PR.allDefs.end()) locs = it->second;
        dupItems.push_back(json::Object{
            {"name", f}, {"files", json::Array(locs)},
        });
    }
    O["duplicateFuncs"] = json::Object{
        {"count", (int)PR.duplicateFuncs.size()},
        {"items", json::Array(dupItems)},
    };

    // call graph (key function → callees)
    json::Object graphObj;
    for (const auto &P : PR.callGraph)
        graphObj[P.first] = json::Array(P.second);
    O["callGraph"] = json::Value(std::move(graphObj));

    return O;
}
