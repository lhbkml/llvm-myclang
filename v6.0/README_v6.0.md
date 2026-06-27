# myclang-c++ v6.0 — 项目介绍文档

> **版本**: 6.0
> **日期**: 2026-06-20
> **适用**: `v6.0/` 目录下的所有文件
> **前序**: [v5.4](../v5.4/README_v5.4.md)
> **主题**: JSON 结构化输出 — 为 Web 后端铺路

---

## 1. 版本概述

v6.0 是 6.x 系列的开篇。核心目标：让项目具备"后端"能力。第一步解决输出格式问题——当前 `printReport()` 输出的是人可读文本（中文标签、emoji、分隔线），Web 前端无法解析。v6.0 新增 `--json` 模式，输出结构化 JSON，同时保留原有文本输出不变。

代码规范检查 12 项不变，数据结构字段数不变——这是纯增量改动。

### 1.1 核心变更

| 变更 | v5.4 | v6.0 |
|------|------|------|
| 输出格式 | 仅文本 | **文本 + JSON** |
| `--json` 标志 | — | ✅ |
| 新增 include | — | `llvm/Support/JSON.h` |
| 新增函数 | — | **5 个 toJSON()** |
| frontendAction.cpp 行数 | 1152 | **1352** |

### 1.2 6.x 路线图

```
v6.0  结构化输出   — JSON 序列化 ★
v6.x  内存输入     — 接受源码字符串（非磁盘文件）
v6.x  错误标准化   — 结构化错误返回（非 exit+stderr）
v6.x  Web 层集成   — HTTP API + 上传页面
```

---

## 2. JSON 输出设计

### 2.1 技术选型

使用 **LLVM 内置 JSON 库** (`llvm/Support/JSON.h`)：

- 零新增依赖——已随 `-lclangBasic` 链接
- `json::Value` / `json::Object` / `json::Array` 类型安全
- ADL 约定的 `toJSON()` 自由函数
- `raw_ostream << formatv("{0:2}", value)` 美化输出

### 2.2 序列化架构

```
AnalysisStats
├── toJSON(LongLineInfo)        → {"file", "line", "length"}
├── toJSON(EmptyBlockInfo)      → {"function", "type"}
├── toJSON(FallThroughInfo)     → {"function", "fromCase", "toCase"}
├── toJSON(FunctionStats)       → 逐函数完整 JSON + violations 数组
└── toJSON(AnalysisStats)       → 根 JSON 对象（summary/lines/controls/calls/violations/functions）
```

6 层结构互不耦合，`printReport()` 一行不改。

### 2.3 JSON Schema

```json
{
  "files": ["test.c"],
  "summary": {
    "totalFunctions": 4,
    "emptyFunctions": 0,
    "paramlessFunctions": 2,
    "paramFunctions": 2,
    "globalVars": 2,
    "localVars": 10,
    "includeCount": 3,
    "avgCCN": 3.2,
    "maxCCN": {"function": "high_ccn_func", "value": 17}
  },
  "lines": {
    "total": 200, "code": 150, "blank": 30,
    "singleComment": 10, "multiComment": 5, "preprocessor": 5
  },
  "controls": {
    "if": 10, "for": 5, "while": 3, "doWhile": 1,
    "switch": 2, "case": 8,
    "logicalAndOr": 3, "conditionalOp": 2,
    "break": 5, "continue": 2, "return": 20, "goto": 0
  },
  "calls": {
    "total": 15,
    "targets": {"printf": 5, "malloc": 3}
  },
  "violations": {
    "overlongFunctions": {"count": 1, "items": [{"name":"foo","lines":55}]},
    "tooManyParams":    {"count": 0, "items": []},
    "deepNesting":      {"count": 1, "items": [{"name":"deep","depth":6}]},
    "badNames":         {"count": 0, "items": []},
    "badGlobalVarNames": {"count": 0, "items": []},
    "longLines":        {"count": 2, "items": [...]},
    "redundantReturns": {"count": 2, "items": [...]},
    "deadStmts":        {"count": 0, "items": []},
    "fallThroughs":     {"count": 4, "items": [...]},
    "highCCN":          {"count": 1, "items": [{"name":"complex","ccn":17}]},
    "emptyBlocks":      {"count": 0, "items": []},
    "gotoCount": 0
  },
  "functions": [
    {
      "name": "high_ccn_func",
      "lines": 30,
      "params": 1,
      "ccn": 17,
      "localVars": 5,
      "maxNesting": 4,
      "emptyBlocks": 0,
      "controls": {"if": 6, "for": 2, "return": 5, ...},
      "calls": {"total": 3, "targets": {"log": 2}},
      "violations": ["fallThrough", "highCCN"]
    }
  ]
}
```

每个违规项统一为 `{"count": N, "items": [...]}` 结构，items 中包含足够前端展示详情。

### 2.4 使用

```bash
# 文本输出（默认，行为不变）
./frontendAction test.c

# JSON 输出（给 Web 前端 / API 调用）
./frontendAction test.c --json
./frontendAction test.c --json | python3 -m json.tool   # 美化验证
```

---

## 3. 代码规范检查全景（12 项，不变）

```
  函数行数上限(50行)    ← v3.0
  goto 语句             ← v3.0
  函数参数上限(5个)     ← v5.4
  嵌套深度上限(4)       ← v5.4
  命名规范              ← v3.0
  全局变量命名          ← v3.1
  单行长度(100字符)     ← v3.1
  冗余 return           ← v3.2 (v5.2 CFG 修复)
  switch穿透            ← v5.3
  死代码                ← v5.0/v5.1
  空代码块              ← v3.2
  圈复杂度上限(10)      ← v5.0
```

---

## 4. 指标清单

### 4.1 全局汇总（37 项）

同 v5.4，不变。

### 4.2 逐函数明细（30 项）

同 v5.4，不变。JSON 模式下增加 `violations` 数组和结构化嵌套，但原始字段不变。

---

## 5. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 1352 | AST+CFG 双引擎 + 12 项规范检查 + 文本/JSON 双输出 |
| `IRMake.cpp` | 107 | LLVM IR 手工构建（不变，自 v1.0） |
| `Makefile` | 116 | 双项目构建 + test 目标（不变，自 v1.1） |

---

## 6. 版本演进总览

```
v1.0  原型验证     — 5种AST节点逐行打印
v1.1  实用化       — 统计-报告架构
v1.2  突破(#include) — HeaderSearchOpts 手动配置
v1.3  成熟化       — CreateFromArgs 自动发现
v1.4  扩展         — 13项指标 + PPCallbacks
v1.5  多文件       — 批量分析 + Stmt过滤
v2.0  逐函数       — TraverseFunctionDecl 上下文
v2.1  空函数&参数  — 空函数检测 + 有参/无参
v3.0  代码规范     — 行数上限 + goto + snake_case
v3.1  规范扩展     — g_ 前缀 + 单行长度
v3.2  完备检查     — 冗余 return + 空代码块
v4.0  行数统计     — 代码行/空行/注释/预处理
v4.1  圈复杂度     — 枚举法 CCN
v4.2  CCN → CFG   — 图论公式 M = E − N + 2
v5.0  规范完备化   — CCN 上限 + 死 return 检测
v5.1  死代码通用化 — ReturnStmt → 所有语句
v5.2  冗余检查修复 — CFG 可达性 + return 三层分类
v5.3  穿透检测     — switch case fall-through via CFG
v5.4  参数+嵌套    — 参数个数上限 + 嵌套深度检测
v6.0  JSON 输出    — --json 模式 + toJSON 序列化 ★
```

---

*本文档版本 6.0，归档于 2026-06-20。*
