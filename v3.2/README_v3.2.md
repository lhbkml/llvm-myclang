# myclang-c++ v3.2 — 项目介绍文档

> **版本**: 3.2
> **日期**: 2026-06-17
> **适用**: `v3.2/` 目录下的所有文件
> **前序**: [v3.1](../v3.1/README_v3.1.md)
> **主题**: 冗余 return 检测 + 空代码块检测

---

## 1. 版本概述

v3.2 在 v3.1 五项规范检查的基础上，新增冗余 return 语句检测和空代码块检测。代码规范检查项达到 **7 项**。

### 1.1 核心变更

| 变更 | v3.1 | v3.2 |
|------|------|------|
| 代码规范检查项 | 5 项 | **7 项** |
| 冗余 return | ❌ | ✅ void 末尾 `return;` + main 末尾 `return 0;` |
| 空代码块 | ❌ | ✅ if/for/while/do-while/else 后 `{ }` |
| FunctionStats 字段 | 18 个 | 19 个 (+emptyBlocks) |
| AnalysisStats 字段 | 23 个 | 25 个 (+redundantReturns, +emptyBlockCount, +emptyBlocks) |
| 新增结构体 | LongLineInfo | +EmptyBlockInfo |

---

## 2. 新增检查项

### 2.1 冗余 return 语句

在 `TraverseFunctionDecl` 中，取函数体 `CompoundStmt` 末语句判断：

| 情况 | 检测条件 | 示例 |
|------|---------|------|
| void 末尾 | `isVoidType()` + 末语句是无返回值的 ReturnStmt | `void f() { ... return; }` |
| main 末尾 | 函数名 `main` + `isIntegerType()` + 末语句是 `IntegerLiteral(0)` | `int main() { return 0; }` |

```cpp
if (auto *RS = dyn_cast<ReturnStmt>(lastStmt)) {
    if (FD->getReturnType()->isVoidType() && !RS->getRetValue())
        FS.hasRedundantReturn = true;
    else if (FS.name == "main" && FD->getReturnType()->isIntegerType()) {
        if (auto *IL = dyn_cast<IntegerLiteral>(RS->getRetValue()))
            if (IL->getValue() == 0) FS.hasRedundantReturn = true;
    }
}
```

报告区分两类消息：
- `xxx(): void 函数末尾 return; 是多余的`
- `main(): 末尾 return 0; 是多余的 (C99+ 隐式返回0)`

### 2.2 空代码块

在 `VisitIfStmt` / `VisitForStmt` / `VisitWhileStmt` / `VisitDoStmt` 中，通过 `checkEmptyBlock()` 辅助函数检查：

```cpp
void checkEmptyBlock(Stmt *Body, const std::string &Type) {
    if (auto *CS = dyn_cast<CompoundStmt>(Body)) {
        if (CS->body_empty()) {
            Stats.emptyBlockCount++;
            Stats.emptyBlocks.push_back({
                CurrentFunc ? CurrentFunc->name : "<toplevel>", Type
            });
            if (CurrentFunc) CurrentFunc->emptyBlocks++;
        }
    }
}
```

覆盖 5 种块类型：`if`、`else`、`for`、`while`、`do-while`。IfStmt 同时检查 then 和 else 分支。

---

## 3. 代码规范检查全景（7 项）

```
【代码规范检查】
  函数行数上限(50行): ⚠ 发现 1 个超标          ← v3.0
  goto 语句:   ⚠ 发现 1 处                     ← v3.0
  命名规范:   ⚠ 发现 4 个不规范                ← v3.0
  全局变量命名: ⚠ 发现 3 个缺少 g_ 前缀        ← v3.1
  单行长度(100字符): ⚠ 发现 3 行超标           ← v3.1
  冗余 return: ⚠ 发现 3 处                     ← v3.2
  空代码块:   ⚠ 发现 4 处                      ← v3.2
```

---

## 4. 测试文件

| 文件 | 测试目标 | 版本 |
|------|---------|:--:|
| `output/test_overlong.c` | 函数行数超标 | v3.0 |
| `output/test_bad_names.c` | 函数命名不规范 | v3.0 |
| `output/test_global_vars.c` | 全局变量缺 g_ 前缀 | v3.1 |
| `output/test_long_lines.c` | 单行超长 | v3.1 |
| `output/test_redundant_return.c` | 冗余 return | v3.2 |
| `output/test_main_return.c` | main 冗余 return 0 | v3.2 |
| `output/test_empty_blocks.c` | 空代码块 | v3.2 |

---

## 5. 指标清单

### 5.1 全局汇总（25 项）

| 分类 | 指标 | 版本 |
|------|------|:--:|
| 基础 | 文件数、函数数 | v1.x |
| 参数 | 无参函数、有参函数 | v2.1 |
| 空函数 | 空函数数 | v2.1 |
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
| `frontendAction.cpp` | 754 | 多文件分析 + 25 项全局 + 19 项逐函数 + 7 项规范检查 |
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
v3.0  代码规范     — 行数上限 + goto禁用 + snake_case命名
v3.1  规范扩展     — 全局变量 g_ 前缀 + 单行长度
v3.2  完备检查     — 冗余 return + 空代码块
```

---

*本文档版本 3.2，归档于 2026-06-17。*
