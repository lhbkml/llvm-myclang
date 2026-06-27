#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <map>
#include <string>
#include <vector>

// ====================== 代码规范检查阈值 ======================
static const int MAX_FUNCTION_LINES = 50;
static const int MAX_LINE_LENGTH = 100;
static const int MAX_CCN = 10;                           // 圈复杂度上限
static const int MAX_PARAMS = 5;                          // 参数个数上限
static const int MAX_NESTING = 4;                          // 嵌套深度上限

// ====================== 记录型小结构 ======================

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

// switch case 穿透信息
struct FallThroughInfo {
    std::string funcName;
    std::string fromCase;   // 源 case 标签
    std::string toCase;     // 穿透到的 case 标签
};

// ====================== 命名检查辅助函数 ======================

// 检查函数名是否符合小写下划线风格 (snake_case)
inline bool isValidSnakeCase(const std::string &name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_')
            return false;
    }
    // 首字符必须是字母（不允许数字和下划线开头）
    return name[0] >= 'a' && name[0] <= 'z';
}

// 检查全局变量名是否有 g_ 前缀（其余部分需符合 snake_case）
inline bool isValidGlobalVarName(const std::string &name) {
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
    bool hasTooManyParams = false;                      // 参数过多 (>MAX_PARAMS)
    int maxNesting = 0;                                 // 最大嵌套深度
    bool hasDeepNesting = false;                        // 嵌套过深 (>MAX_NESTING)
    bool hasRedundantReturn = false;                    // 冗余 return; (void函数末尾)
    bool hasDeadCode = false;                           // 含不可达路径上的死代码
    int deadStmtCount = 0;                              // 不可达路径上的语句数
    bool hasFallThrough = false;                        // 含 switch 穿透
    int fallThroughCount = 0;                           // switch 穿透数
    bool isHighCCN = false;                             // 圈复杂度过高 (>MAX_CCN)
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
    int tooManyParamsFunctions = 0;                     // 参数过多函数 (>MAX_PARAMS)
    int deepNestingFunctions = 0;                       // 嵌套过深函数 (>MAX_NESTING)
    int badGlobalVarNames = 0;                          // 全局变量缺 g_ 前缀
    std::vector<std::string> badGlobalVars;             // 问题全局变量名列表
    int longLineCount = 0;                              // 过长单行(>100字符)
    std::vector<LongLineInfo> longLines;                // 过长单行明细
    int redundantReturns = 0;                           // 冗余 return; 语句
    int deadStmts = 0;                                  // 不可达路径上的死代码语句
    int fallThroughCount = 0;                          // switch 穿透数
    std::vector<FallThroughInfo> fallThroughs;          // 穿透明细
    int highCCNFunctions = 0;                          // 圈复杂度过高函数 (>MAX_CCN)
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
        tooManyParamsFunctions += Other.tooManyParamsFunctions;
        deepNestingFunctions += Other.deepNestingFunctions;
        badGlobalVarNames  += Other.badGlobalVarNames;
        badGlobalVars.insert(badGlobalVars.end(),
                             Other.badGlobalVars.begin(),
                             Other.badGlobalVars.end());
        longLineCount += Other.longLineCount;
        longLines.insert(longLines.end(),
                         Other.longLines.begin(), Other.longLines.end());
        redundantReturns  += Other.redundantReturns;
        deadStmts        += Other.deadStmts;
        fallThroughCount  += Other.fallThroughCount;
        fallThroughs.insert(fallThroughs.end(),
                            Other.fallThroughs.begin(), Other.fallThroughs.end());
        highCCNFunctions  += Other.highCCNFunctions;
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

// ====================== 分析结果包装 ======================
struct AnalysisResult {
    bool success = false;
    AnalysisStats stats;
    std::vector<std::string> errors;    // 流程错误 + Clang Error 诊断
    std::vector<std::string> warnings;  // Clang Warning 诊断
};

#endif
