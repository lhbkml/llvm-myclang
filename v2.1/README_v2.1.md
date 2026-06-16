# myclang-c++ v2.1 — 项目介绍文档

> **版本**: 2.1
> **日期**: 2026-06-16
> **适用**: `v2.1/` 目录下的所有文件
> **前序**: [v2.0](../v2.0/README_v2.0.md)
> **主题**: 空函数检测 + 有参/无参函数分类

---

## 1. 版本概述

v2.1 在 v2.0 逐函数统计的基础上，新增两项函数级分类统计：空函数（无体/仅 return）检测，和有参/无参函数分类。这些指标同时计入全局汇总和逐函数明细。

### 1.1 核心变更

| 变更 | v2.0 | v2.1 |
|------|------|------|
| 空函数检测 | ❌ | ✅ 检测 `{}` 和 `{ return; }` |
| 参数分类 | ❌ | ✅ 无参/有参 |
| FunctionStats 字段 | 14 个 | 16 个 (+paramCount, +isEmptyOrReturnOnly) |
| AnalysisStats 字段 | 15 个 | 18 个 (+emptyFunctions, +paramlessFunctions, +paramFunctions) |

---

## 2. 空函数检测逻辑

### 2.1 判定条件

一个函数被标记为"空/仅return"，当且仅当其函数体满足以下条件之一：

| 条件 | 例子 | AST 特征 |
|------|------|----------|
| 空函数体 | `void f() { }` | `CompoundStmt::body_empty() == true` |
| 仅含一条 return | `int g() { return 0; }` | `CS->size() == 1 && isa<ReturnStmt>(CS->body_begin())` |

任何包含 return *之外*语句的函数（如含变量声明 + return，或含 if/for + return）都不会被标记。

### 2.2 实现位置

检测在 `TraverseFunctionDecl` 的 pre 阶段完成，早于 `VisitFunctionDecl` 调用：

```cpp
// TraverseFunctionDecl 中，行数计算之后：
if (auto *CS = dyn_cast<CompoundStmt>(Body)) {
    if (CS->body_empty()) {
        FS.isEmptyOrReturnOnly = true;
    } else if (CS->size() == 1 && isa<ReturnStmt>(*CS->body_begin())) {
        FS.isEmptyOrReturnOnly = true;
    }
}
```

全局计数在 `VisitFunctionDecl` 中累加：

```cpp
if (CurrentFunc && CurrentFunc->isEmptyOrReturnOnly)
    Stats.emptyFunctions++;
```

---

## 3. 参数分类逻辑

```cpp
// TraverseFunctionDecl pre 阶段：
FS.paramCount = FD->getNumParams();

// VisitFunctionDecl：
if (FD->getNumParams() == 0)
    Stats.paramlessFunctions++;
else
    Stats.paramFunctions++;
```

---

## 4. 报告输出示例

```
【基本信息】
  总函数数量: 3
  无参函数:     1           ← main()
  有参函数:     2           ← loop_test(), switch_test()
  空函数(无体/仅return): 0

【逐函数统计】
  ─── loop_test() — 29 行, 1 参数 ───
    局部变量: 4
    分支/循环: if:3 for:1 while:1 do-while:1 break:1 continue:1 return:2 goto:1
    函数调用: 0 次

  ─── main() — 11 行, 无参 ───
    局部变量: 3
    分支/循环: if:1 return:1
    函数调用: 3 次 (loop_test:1, printf:1, switch_test:1)

  ─── sum() — 3 行, 2 参数 [空/仅return] ───    ← 仅含 return a+b
    局部变量: 0
    分支/循环: return:1
    函数调用: 0 次
```

---

## 5. 指标清单（16 项全局 + 16 项/函数）

### 5.1 全局汇总（18 项）

| 分类 | 指标 | 说明 |
|------|------|------|
| 基础 | 文件数、函数数 | 同 v2.0 |
| ★参数 | 无参函数 | `paramlessFunctions` |
| ★参数 | 有参函数 | `paramFunctions` |
| ★空函数 | 空函数数 | `emptyFunctions` |
| 变量 | 全局变量、局部变量 | 同 v2.0 |
| 控制流 | if/for/while/do-while/switch/break/continue/return/goto | 同 v2.0 |
| 预处理 | #include | 同 v2.0 |
| 调用 | 调用次数、调用分布 | 同 v2.0 |

### 5.2 逐函数明细（16 项）

```
name | lines | localVars | paramCount | isEmptyOrReturnOnly |
ifCount | forCount | whileCount | doWhileCount |
switchCount | breakCount | continueCount |
returnCount | gotoCount | callCount | callTargets
```

---

## 6. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 518 | 多文件分析 + 18 项全局指标 + 逐函数 16 项明细 |
| `IRMake.cpp` | 107 | LLVM IR 手工构建（不变） |
| `Makefile` | 116 | 双项目构建 + test 目标（不变） |

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
```

---

*本文档版本 2.1，归档于 2026-06-16。*
