# myclang-c++ v5.1 — 项目介绍文档

> **版本**: 5.1
> **日期**: 2026-06-20
> **适用**: `v5.1/` 目录下的所有文件
> **前序**: [v5.0](../v5.0/README_v5.0.md)
> **主题**: 死代码检测从 return 扩展为通用语句

---

## 1. 版本概述

v5.1 将 v5.0 中仅检测不可达 return 的 CFG 可达性分析扩展为检测不可达块中的**所有语句**——赋值、函数调用、自增/减、break/continue 等任何类型的 CFGStmt，而非仅限于 `ReturnStmt`。

### 1.1 核心变更

| 变更 | v5.0 | v5.1 |
|------|------|------|
| 死代码检测范围 | 仅 `ReturnStmt` | **所有 CFGStmt** |
| 过滤条件 | `isa<ReturnStmt>` | 无过滤（`getAs<CFGStmt>` 直接计数） |
| FunctionStats 字段 | `hasDeadReturn` / `deadReturnCount` | `hasDeadCode` / `deadStmtCount` |
| AnalysisStats 字段 | `deadReturns` | `deadStmts` |
| 报告标签 | `⚠ 含无法执行return` | `⚠ 含死代码` |
| 报告检查项 | `无法执行return: X 个return在不可达路径上` | `死代码: X 条不可达语句` |

---

## 2. 死代码检测：return → 通用

### 2.1 原理

CFG 构建后，以入口块为起点做反向可达性查询——某块中所有 `CFGStmt` 类型的元素如果在从入口不可达的块中，都计为死代码：

```cpp
// v5.0: 仅 ReturnStmt
if (isa<ReturnStmt>(S->getStmt()))
    FS.deadReturnCount++;

// v5.1: 所有语句
if (Elem.getAs<CFGStmt>())
    FS.deadStmtCount++;
```

### 2.2 检测案例

```c
int f(int x) {
    if (x) return 1;
    else   return 2;
    x = 42;    // ← 死代码：赋值
    x++;       // ← 死代码：自增
    return x;  // ← 死代码：return
}
// 报告: f(): 4 条语句在不可达路径上
```

所有类型语句均被覆盖：赋值、函数调用、循环、break/continue、goto、return 等。

---

## 3. 字段对照

### FunctionStats（25 项 → 25 项，重命名 3 项）

| v5.0 | v5.1 |
|------|------|
| `hasDeadReturn` (bool) | `hasDeadCode` (bool) |
| `deadReturnCount` (int) | `deadStmtCount` (int) |

### AnalysisStats（33 项 → 33 项，重命名 1 项）

| v5.0 | v5.1 |
|------|------|
| `deadReturns` (int) | `deadStmts` (int) |

---

## 4. 报告变更

### v5.0
```
  无法执行return: ⚠ 发现 3 处
    - after_if_else(): 1 个return在不可达路径上
```

### v5.1
```
  死代码:     ⚠ 发现 4 条不可达语句
    - after_if_else(): 2 条语句在不可达路径上
    - dead_assignments(): 4 条语句在不可达路径上
```

---

## 5. 指标清单

同 v5.0（33 项全局 + 25 项逐函数），字段名更新见第 3 节。

---

## 6. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 981 | 多文件分析 + AST+CFG 双引擎 + 8 项规范检查 + 通用死代码检测 |
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
v4.0  行数统计     — 代码行/空行/注释行分类 + // vs /**/ 拆分 + 预处理指令
v4.1  圈复杂度     — 逐函数 CCN + case/&&/||/?: 决策点计数 (枚举法)
v4.2  CCN → CFG   — 改用 Clang CFG 图论公式 (M = E − N + 2)
v5.0  规范完备化   — CCN 上限告警 + CFG 可达性死 return 检测
v5.1  死代码通用化 — 死代码检测从 ReturnStmt 扩展为所有语句类型
```

---

*本文档版本 5.1，归档于 2026-06-20。*
