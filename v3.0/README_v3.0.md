# myclang-c++ v3.0 — 项目介绍文档

> **版本**: 3.0
> **日期**: 2026-06-16
> **适用**: `v3.0/` 目录下的所有文件
> **前序**: [v2.1](../v2.1/README_v2.1.md)
> **主题**: 代码规范 & 风格检查

---

## 1. 版本概述

v3.0 开启全新的「代码规范 & 风格检查」主题。在 v2.1 统计分析能力的基础上，新增报告中的 `【代码规范检查】` 独立段落，逐项给出 ✓/⚠ 判定。

### 1.1 核心变更

| 变更 | v2.1 | v3.0 |
|------|------|------|
| 报告结构 | 基本信息 → 逐函数 → 全局汇总 | 基本信息 → **代码规范检查** → 逐函数 → 全局汇总 |
| 函数行数 | 仅展示 | **超标警告**（阈值 50 行） |
| goto 语句 | 仅统计 | **禁用项告警** |
| 命名规范 | ❌ | **强制 snake_case 校验** |
| FunctionStats 字段 | 16 个 | 18 个 (+isOverlong, +isBadName) |
| AnalysisStats 字段 | 18 个 | 20 个 (+overlongFunctions, +badNamedFunctions) |

---

## 2. 代码规范检查项

### 2.1 函数行数上限

| 阈值 | 规则 | 配置 |
|------|------|------|
| 50 行 | `lines > MAX_FUNCTION_LINES` → 超标 | `static const int MAX_FUNCTION_LINES = 50;` |

超标函数在报告中单独列出，显示行数和超标幅度：

```
  函数行数上限: 50 行
  ⚠ 超标函数: 1 个
    - big_function(): 59 行 (超标 9 行)
```

### 2.2 goto 语句禁用

根据 `Stats.gotoCount` 和 `FunctionStats::gotoCount`（v1.4 开始已追踪），零开销判定：

```
  goto 语句:   ⚠ 发现 1 处
    - loop_test(): 1 处
```

### 2.3 命名规范 — 强制小写下划线风格

校验函数 `isValidSnakeCase()`：

```cpp
static bool isValidSnakeCase(const std::string &name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_')
            return false;
    }
    return name[0] >= 'a' && name[0] <= 'z';  // 首字符必须小写字母
}
```

| 函数名 | 判定 | 原因 |
|--------|------|------|
| `loop_test` | ✓ | 全小写 + 下划线 |
| `has123digits` | ✓ | 数字合法 |
| `BadName` | ✗ | 大写字母 B |
| `badCamelCase` | ✗ | 大写字母 C |
| `UPPER_CASE` | ✗ | 大写字母 |
| `_leading` | ✗ | 下划线开头 |

---

## 3. 报告结构

```
========== C 源文件分析报告 ==========

【基本信息】              ← 文件数、函数数、参数分类、空函数、变量、#include
【代码规范检查】          ← ★ v3.0 新增：行数 / goto / 命名
【逐函数统计】            ← 每函数独立明细（含 ⚠ 标记）
【控制流语句统计（全局汇总）】
【函数调用统计（全局汇总）】
```

### 3.1 逐函数 ⚠ 标记

每个函数的标题行可携带 1-3 个标记：

```
  ─── big_function() — 59 行, 1 参数 ⚠ 行数超标 ⚠ 命名不规范 ───
  ─── loop_test() — 29 行, 1 参数 ───
```

---

## 4. 测试文件

| 文件 | 测试目标 |
|------|---------|
| `output/test_overlong.c` | 行数超标（59 行函数） |
| `output/test_bad_names.c` | 命名规范（6 函数，4 不规范 + 2 合规） |

```bash
$ ./frontendAction output/test_bad_names.c
  命名规范:   ⚠ 发现 4 个不规范
    - BadName(): 应使用小写下划线风格
    - badCamelCase(): 应使用小写下划线风格
    - UPPER_CASE(): 应使用小写下划线风格
    - _leading_underscore(): 应使用小写下划线风格
```

---

## 5. 指标清单（20 项全局 + 18 项/函数）

### 5.1 全局汇总

| 分类 | 指标 | v3.0 新增 |
|------|------|:--:|
| 基础 | 文件数、函数数 | |
| 参数 | 无参函数、有参函数 | |
| 空函数 | 空函数数 | |
| ★规范 | 行数超标函数 | ✅ |
| ★规范 | 命名不规范函数 | ✅ |
| 变量 | 全局变量、局部变量 | |
| 控制流 | if/for/while/do-while/switch/break/continue/return/goto | |
| 预处理 | #include | |
| 调用 | 调用次数、调用分布 | |

### 5.2 逐函数明细

```
name | lines | localVars | paramCount |
isEmptyOrReturnOnly | isOverlong | isBadName |
ifCount | forCount | whileCount | doWhileCount |
switchCount | breakCount | continueCount |
returnCount | gotoCount | callCount | callTargets
```

---

## 6. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 595 | 多文件分析 + 20 项全局指标 + 18 项逐函数明细 + 3 项规范检查 |
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
v3.0  ★ 代码规范   — 行数上限 + goto禁用 + snake_case命名
```

---

*本文档版本 3.0，归档于 2026-06-16。*
