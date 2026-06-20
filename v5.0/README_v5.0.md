# myclang-c++ v5.0 — 项目介绍文档

> **版本**: 5.0
> **日期**: 2026-06-20
> **适用**: `v5.0/` 目录下的所有文件
> **前序**: [v4.2](../v4.2/README_v4.2.md)
> **主题**: 代码规范检查完备化 + CFG 可达性分析

---

## 1. 版本概述

v5.0 是第 5 代大版本，标志着从纯 AST 模式向 **AST + CFG 双引擎** 的升级完成。代码规范检查从 7 项扩展到 **8 项**（+圈复杂度上限 + 无法执行 return），CFG 不仅用于 CCN 计算，也用于可达性分析——检测真正的死代码。

### 1.1 核心变更

| 变更 | v4.2 | v5.0 |
|------|------|------|
| 代码规范检查项 | 7 项 | **8 项** |
| 圈复杂度上限 | ❌ | ✅ MAX_CCN=10 告警 |
| 死代码检测 | ❌ | ✅ CFG 可达性：无法执行的 return |
| CFG 用途 | CCN 计算 | CCN + 可达性分析 |
| 新依赖 | clangAnalysis | +`CFGReachabilityAnalysis.h` (header-only) |
| FunctionStats 字段 | 23 个 | **25 个** (+isHighCCN, +hasDeadReturn, +deadReturnCount) |
| AnalysisStats 字段 | 31 个 | **33 个** (+highCCNFunctions, +deadReturns) |

---

## 2. CFG 双引擎架构

```
TraverseFunctionDecl
  │
  ├─ 前序: 函数元数据 (行数/参数/空函数/命名/冗余return)
  │
  ├─ 递归遍历: 17 个 Visit* 方法 (计数各 AST 节点)
  │
  └─ 后序 (CFG 双引擎):
       ├─ CFG::buildCFG()  ← 构建一次
       ├─ 引擎 A: M = E − N + 2  →  CCN
       ├─ 引擎 B: CFGReverseBlockReachabilityAnalysis  →  dead return
       └─ 保存统计
```

一张 CFG 图同时驱动两个分析维度，无需重复构建。

---

## 3. 新增检查项

### 3.1 圈复杂度上限

| 阈值 | 规则 | 配置 |
|------|------|------|
| 10 | CCN > MAX_CCN → 超标 | `static const int MAX_CCN = 10;` |

报告格式：
```
  圈复杂度上限(10): ⚠ 发现 1 个超标
    - high_ccn_func(): CCN=17 (超标 7)
```

### 3.2 无法执行的 return

利用 `CFGReverseBlockReachabilityAnalysis` 做入口→所有块的可达性查询，标记含 `ReturnStmt` 的不可达块：

```
  无法执行return: ⚠ 发现 3 处
    - after_if_else(): 1 个return在不可达路径上
    - after_return(): 1 个return在不可达路径上
    - after_switch(): 1 个return在不可达路径上
```

检测场景：

```c
// ✓ 检测: if-else 两分支都 return 后的 return
int f(int x) { if(x) return 1; else return 2; return 3; }

// ✓ 检测: 无条件 return 后的 return
void g() { return; return; }

// ✗ 不误报: 可达的 return
int h(int x) { if(x) return 1; return 0; }
```

---

## 4. 代码规范检查全景（8 项）

```
【代码规范检查】
  函数行数上限(50行):    ✓ / ⚠        ← v3.0
  goto 语句:            ✓ / ⚠        ← v3.0
  命名规范:             ✓ / ⚠        ← v3.0
  全局变量命名:         ✓ / ⚠        ← v3.1
  单行长度(100字符):    ✓ / ⚠        ← v3.1
  冗余 return:          ✓ / ⚠        ← v3.2
  空代码块:             ✓ / ⚠        ← v3.2
  圈复杂度上限(10):     ✓ / ⚠        ← v5.0 ★
  无法执行return:       ✓ / ⚠        ← v5.0 ★
```

---

## 5. 指标清单

### 5.1 全局汇总（33 项）

| 分类 | 指标 | 版本 |
|------|------|:--:|
| 基础 | 文件数、函数数 | v1.x |
| 参数 | 无参函数、有参函数 | v2.1 |
| 空函数 | 空函数数 | v2.1 |
| 行数 | 总行数、代码行、空行 | v4.0 |
| 行数 | 单行注释 / 多行注释 | v4.0 |
| 行数 | 预处理指令行 | v4.0 |
| 规范 | 行数超标函数 | v3.0 |
| 规范 | 命名不规范函数 | v3.0 |
| 规范 | 全局变量缺 g_ 前缀 | v3.1 |
| 规范 | 单行超长 | v3.1 |
| 规范 | 冗余 return | v3.2 |
| ★规范 | 圈复杂度过高函数 | v5.0 |
| ★规范 | 无法执行的 return | v5.0 |
| 规范 | 空代码块 | v3.2 |
| 变量 | 全局变量、局部变量 | v1.x |
| 控制流 | if/for/while/do-while/switch/case/&&\|\|/?:/break/continue/return/goto | v1.x~v4.1 |
| 预处理 | #include | v1.4 |
| 调用 | 调用次数、调用分布 | v1.x |

### 5.2 逐函数明细（25 项）

```
name | lines | localVars | paramCount |
ccn (CFG 计算) | caseCount | logicalAndOrCount | conditionalOpCount |
isEmptyOrReturnOnly | isOverlong | isBadName |
hasRedundantReturn | hasDeadReturn | deadReturnCount |
isHighCCN | emptyBlocks |
ifCount | forCount | whileCount | doWhileCount |
switchCount | breakCount | continueCount |
returnCount | gotoCount | callCount | callTargets
```

---

## 6. 报告全景（6 段）

```
========== C 源文件分析报告 ==========

【基本信息】              ← v1.x
【代码行数统计】          ← v4.0
【代码规范检查】          ← v3.x + v5.0 (8项)
【逐函数统计】            ← v2.x + CCN + 死代码标签
【控制流语句统计（全局汇总）】  ← v1.x + v4.1 扩展
【函数调用统计（全局汇总）】   ← v1.x
```

---

## 7. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 983 | 多文件分析 + AST+CFG 双引擎 + 8 项规范检查 + 33 项全局 + 25 项逐函数 |
| `IRMake.cpp` | 107 | LLVM IR 手工构建（不变，自 v1.0） |
| `Makefile` | 116 | 双项目构建 + test 目标（不变，自 v1.1） |

---

## 8. 版本演进总览

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
v5.0  规范完备化   — CCN 上限告警 + CFG 可达性死代码检测
```

---

*本文档版本 5.0，归档于 2026-06-20。*
