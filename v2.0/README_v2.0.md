# myclang-c++ v2.0 — 项目介绍文档

> **版本**: 2.0
> **日期**: 2026-06-16
> **适用**: `v2.0/` 目录下的所有文件
> **前序**: [v1.5](../v1.5/README_v1.5.md)
> **主题**: 逐函数粒度精细化统计

---

## 1. 版本概述

v2.0 是项目从"全局汇总统计"升级为"逐函数精细化统计"的关键版本。在保留全局汇总的前提下，新增对每个函数内部的独立分析：局部变量数、各类分支/循环语句数、函数调用明细。

### 1.1 核心变更

| 变更 | v1.5 | v2.0 |
|------|------|------|
| 统计粒度 | 仅全局汇总 | 全局汇总 + **逐函数明细** |
| 数据结构 | `AnalysisStats`（14 个 int + funcLines） | 新增 `FunctionStats` 结构体 |
| 上下文追踪 | 无函数上下文 | `TraverseFunctionDecl` push/pop 机制 |
| 变量统计 | 全局/局部总数 | 总数 + **每个函数的局部变量数** |
| 控制流统计 | 全局总数 | 总数 + **每个函数的分支/循环数** |
| 调用统计 | 全局汇总 | 全局汇总 + **每个函数的调用明细** |
| 指标总数 | 14 项全局 | 14 项全局 + 每函数 13 项明细 |

---

## 2. 新增数据结构

### 2.1 `FunctionStats` — 逐函数统计

```cpp
struct FunctionStats {
    std::string name;           // 函数名
    int lines = 0;              // 函数体行数
    int localVars = 0;          // 局部变量数（不含参数）
    int ifCount, forCount, whileCount, doWhileCount;   // 分支
    int switchCount, breakCount, continueCount;         // 跳转
    int returnCount, gotoCount;                         // 返回/跳转
    int callCount = 0;          // 函数调用次数
    std::map<std::string, int> callTargets;  // 调用分布
};
```

### 2.2 `AnalysisStats` 变化

```cpp
// v1.5
std::vector<std::pair<std::string, int>> funcLines;  // 仅函数名+行数

// v2.0
std::vector<FunctionStats> functions;                 // 完整逐函数明细
```

---

## 3. 架构演进：全局统计 → 逐函数统计

### 3.1 v1.5 架构

```
VisitFunctionDecl → totalFunctions++
  VisitVarDecl    → globalVars++ / localVars++
  VisitIfStmt     → ifCount++
  VisitForStmt    → forCount++
  ...所有 Visit 只累加全局计数器...
```

### 3.2 v2.0 架构

```
TraverseFunctionDecl(FD)              ← ★ 新增：管理函数上下文
├── CurrentFunc = &FS                 ← 子节点遍历期间可见
├── RecursiveASTVisitor::TraverseFunctionDecl(FD)
│   ├── VisitFunctionDecl  → totalFunctions++
│   ├── VisitVarDecl       → localVars++  + CurrentFunc->localVars++
│   ├── VisitIfStmt        → ifCount++    + CurrentFunc->ifCount++
│   ├── VisitForStmt       → forCount++   + CurrentFunc->forCount++
│   ├── VisitWhileStmt / DoStmt / SwitchStmt / ...
│   ├── VisitBreakStmt / ContinueStmt / ReturnStmt / GotoStmt
│   └── VisitCallExpr      → callCount++  + CurrentFunc->callCount++
└── Stats.functions.push_back(FS)      ← 弹出，保存逐函数明细
```

### 3.3 为什么用 `TraverseFunctionDecl` 而非 `VisitFunctionDecl`？

`VisitFunctionDecl` 只在进入函数时调用一次，没有"退出"钩子。`TraverseFunctionDecl` 在递归前后都能执行代码，天然适合 push/pop 模式：

```cpp
bool TraverseFunctionDecl(FunctionDecl *FD) {
    FunctionStats *Saved = CurrentFunc;   // 保存上级上下文
    FunctionStats FS;                     // 为本函数新建统计
    CurrentFunc = &FS;                    // ★ push：子节点遍历期间可见

    bool result = RecursiveASTVisitor::TraverseFunctionDecl(FD);
    //              ↑ 期间所有 Visit 方法都能通过 CurrentFunc
    //                累加本函数的计数器

    Stats.functions.push_back(FS);        // ★ pop：保存结果
    CurrentFunc = Saved;                  // 恢复上级上下文
    return result;
}
```

### 3.4 双轨制计数

每个 `Visit*` 方法同时累加两级计数器：

```cpp
bool VisitIfStmt(IfStmt *IS) {
    if (isStmtFromMainFile(IS)) {
        Stats.ifCount++;              // 全局汇总（和 v1.5 一样）
        if (CurrentFunc)
            CurrentFunc->ifCount++;   // ★ 逐函数明细（v2.0 新增）
    }
    return true;
}
```

`CurrentFunc` 为 `nullptr` 的情况：函数声明（无函数体）、头文件中的函数（被 `isFromMainFile` 过滤）。此时只累加全局计数器，不影响逐函数统计。

---

## 4. 报告输出格式

### 4.1 新版报告结构

```
========== C 源文件分析报告 ==========

【基本信息】                  ← 文件数、函数数、变量数、#include
【逐函数统计】                ← ★ 每个函数的独立明细
【控制流语句统计（全局汇总）】  ← 和 v1.5 一样
【函数调用统计（全局汇总）】    ← 和 v1.5 一样
```

### 4.2 逐函数统计示例

```
【逐函数统计】
  ─── loop_test() — 29 行 ───
    局部变量: 4
    分支/循环: if:3 for:1 while:1 do-while:1 break:1 continue:1 return:2 goto:1
    函数调用: 0 次

  ─── switch_test() — 24 行 ───
    局部变量: 1
    分支/循环: if:1 switch:1 break:4 return:1
    函数调用: 2 次 (helper_add:1, helper_mul:1)

  ─── main() — 11 行 ───
    局部变量: 3
    分支/循环: if:1 return:1
    函数调用: 3 次 (loop_test:1, printf:1, switch_test:1)
```

零值不显示，避免噪音。例如 `main()` 没有 `for/while/switch` 就完全不显示这些项。

---

## 5. 测试验证

### 5.1 多文件分析

```bash
$ ./frontendAction output/file1_loops.c output/file2_switch.c output/file3_main.c
```

| 函数 | 行数 | 局部变量 | if | for | while | do-while | switch | break | continue | return | goto | 调用 |
|------|------|----------|----|-----|-------|----------|--------|-------|----------|--------|------|------|
| loop_test() | 29 | 4 | 3 | 1 | 1 | 1 | 0 | 1 | 1 | 2 | 1 | 0 |
| switch_test() | 24 | 1 | 1 | 0 | 0 | 0 | 1 | 4 | 0 | 1 | 0 | 2 |
| main() | 11 | 3 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 3 |

### 5.2 全局汇总（与逐函数之和一致）

```
全局: if:5  for:1  while:1  do-while:1  switch:1  break:5  continue:1  return:4  goto:1
```

---

## 6. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 481 | 多文件分析 + 14 项全局指标 + 逐函数明细 |
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
v2.0  ★ 逐函数     — 函数粒度精细化统计 + TraverseFunctionDecl 上下文
```

---

*本文档版本 2.0，归档于 2026-06-16。*
