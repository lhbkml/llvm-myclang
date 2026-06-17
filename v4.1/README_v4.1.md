# myclang-c++ v4.1 — 项目介绍文档

> **版本**: 4.1
> **日期**: 2026-06-17
> **适用**: `v4.1/` 目录下的所有文件
> **前序**: [v3.2](../v3.2/README_v3.2.md)
> **主题**: 代码行数分类统计（代码行/空行/注释行）

---

## 1. 版本概述

v4.1 开启全新的「代码行数统计」主题。在 AST 分析之外，对源文件进行文本级扫描，按代码行、空行、注释行分类计数，并区分单行注释 (`//`) 和多行注释 (`/* */`)。

### 1.1 核心变更

| 变更 | v3.2 | v4.1 |
|------|------|------|
| 报告结构 | 5 段 | **6 段** (+代码行数统计) |
| 行数统计 | ❌ | ✅ 代码行/空行/注释行 |
| 注释拆分 | ❌ | ✅ 单行 `//` vs 多行 `/* */` |
| AnalysisStats 字段 | 25 个 | 28 个 (+totalLines, +codeLines, +blankLines, +singleCommentLines, +multiCommentLines) |

---

## 2. 行数分类算法

在 `analyzeFile()` 中，AST 分析完成后逐行读取源文件，一次遍历同时完成：总行计数、长行检测（v3.1）、行数分类（v4.1）。

### 2.1 分类规则

| 行内容 | 分类 |
|--------|------|
| 纯空白 / 空行 | `blankLines` |
| `// ...`（trim 后以 `//` 开头） | `singleCommentLines` |
| `/* ... */`（同行闭合，前后无代码） | `multiCommentLines` |
| `/*` 开头 → `*/` 结束前的所有行 | `multiCommentLines` |
| `code; // trailing` | `codeLines`（含代码优先） |
| `code; /* comment */` | `codeLines` |
| `/* comment */ code;` | `codeLines` |
| 其余 | `codeLines` |

### 2.2 状态机

```
                    ┌──────────────────┐
     /* 开头        │  inBlockComment  │
   ──────────────→  │  (多行注释内部)   │
                    └────────┬─────────┘
                             │  */ 闭合
                             ▼
                        inBlockComment = false
```

### 2.3 实现位置

```cpp
// analyzeFile() 中，ParseAST 之后：
std::ifstream src(FilePath);
bool inBlockComment = false;
while (std::getline(src, line)) {
    // 1. totalLines++
    // 2. long line check (v3.1)
    // 3. blank → blankLines
    // 4. inBlockComment → multiCommentLines
    // 5. // → singleCommentLines
    // 6. /* ... */ → multiCommentLines
    // 7. else → codeLines
}
```

---

## 3. 报告输出

```
【代码行数统计】
  总行数:     15
  代码行:     4
  空行:       4
  注释行:     7
    - 单行(//): 3
    - 多行(/**/): 4
```

---

## 4. 报告全景（6 段）

```
========== C 源文件分析报告 ==========

【基本信息】              ← v1.x
【代码行数统计】          ← v4.1 新增
【代码规范检查】          ← v3.x (7项)
【逐函数统计】            ← v2.x
【控制流语句统计（全局汇总）】
【函数调用统计（全局汇总）】
```

---

## 5. 指标清单

### 5.1 全局汇总（28 项）

| 分类 | 指标 | 版本 |
|------|------|:--:|
| 基础 | 文件数、函数数 | v1.x |
| 参数 | 无参函数、有参函数 | v2.1 |
| 空函数 | 空函数数 | v2.1 |
| ★行数 | 总行数、代码行、空行 | v4.1 |
| ★行数 | 单行注释(`//`)、多行注释(`/* */`) | v4.1 |
| 规范 | 行数超标函数 | v3.0 |
| 规范 | 命名不规范函数 | v3.0 |
| 规范 | 全局变量缺 g_ 前缀 | v3.1 |
| 规范 | 单行超长 | v3.1 |
| 规范 | 冗余 return | v3.2 |
| 规范 | 空代码块 | v3.2 |
| 变量 | 全局变量、局部变量 | v1.x |
| 控制流 | if/for/while/do-while/switch/break/continue/return/goto | v1.x |
| 预处理 | #include | v1.4 |
| 调用 | 调用次数、调用分布 | v1.x |

### 5.2 逐函数明细（19 项）

```
name | lines | localVars | paramCount |
isEmptyOrReturnOnly | isOverlong | isBadName |
hasRedundantReturn | emptyBlocks |
ifCount | forCount | whileCount | doWhileCount |
switchCount | breakCount | continueCount |
returnCount | gotoCount | callCount | callTargets
```

---

## 6. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 824 | 多文件分析 + 28 项全局 + 19 项逐函数 + 7 项规范检查 + 行数统计 |
| `IRMake.cpp` | 107 | LLVM IR 手工构建（不变，自 v1.0） |
| `Makefile` | 116 | 双项目构建 + test 目标（不变，自 v1.1） |

---

## 7. 版本演进总览

```
v1.0  原型验证     — 5种AST节点逐行打印
v1.1  实用化       — 统计-报告架构，6种节点，双项目构建
v1.2  突破(#include) — 方案A：手动HeaderSearchOpts
v1.3  成熟化       — 方案B：CreateFromArgs自动发现
v1.4  扩展         — 13项指标 + PPCallbacks
v1.5  多文件       — 批量分析 + Stmt过滤 + 变量拆分
v2.0  逐函数       — TraverseFunctionDecl 上下文 + 逐函数明细
v2.1  空函数&参数  — 空函数检测 + 有参/无参分类
v3.0  代码规范     — 行数上限 + goto禁用 + snake_case命名
v3.1  规范扩展     — 全局变量 g_ 前缀 + 单行长度
v3.2  完备检查     — 冗余 return + 空代码块
v4.1  行数统计     — 代码行/空行/注释行分类 + // vs /**/ 拆分
```

---

*本文档版本 4.1，归档于 2026-06-17。*
