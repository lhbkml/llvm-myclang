# myclang-c++ v4.1 — 项目介绍文档

> **版本**: 4.1
> **日期**: 2026-06-17
> **适用**: `v4.1/` 目录下的所有文件
> **前序**: [v4.0](../v4.0/README_v4.0.md)
> **主题**: 圈复杂度（CCN）计算

---

## 1. 版本概述

v4.1 在 v4.0 代码行数统计的基础上，新增逐函数圈复杂度（Cyclomatic Complexity Number, McCabe）计算。

### 1.1 核心变更

| 变更 | v4.0 | v4.1 |
|------|------|------|
| 逐函数 CCN | ❌ | ✅ McCabe 公式 |
| case/default 计数 | ❌ | ✅ 全局 + 逐函数 |
| && / \|\| 计数 | ❌ | ✅ 全局 + 逐函数 |
| ?: 三元运算计数 | ❌ | ✅ 全局 + 逐函数 |
| 报告 - 逐函数标题 | 无 CCN | ✅ `CCN=N` |
| 报告 - CCN 汇总 | ❌ | ✅ 平均 + 最高 |
| 全局控制流统计 | 9 项 | **12 项** (+case, +&&/\|\|, +?:) |
| FunctionStats 字段 | 19 个 | **23 个** (+ccn, +caseCount, +logicalAndOrCount, +conditionalOpCount) |
| AnalysisStats 字段 | 28 个 | **31 个** (+caseCount, +logicalAndOrCount, +conditionalOpCount) |
| 新增 AST 节点 | — | SwitchCase, BinaryOperator(&&/\|\|), ConditionalOperator(?:) |

---

## 2. 圈复杂度公式

```
CCN = 1 + 决策点数

决策点:
  if 语句           +1
  for 语句          +1
  while 语句        +1
  do-while 语句     +1
  case / default    +1 (每个标签)
  && 逻辑与         +1
  || 逻辑或         +1
  ?: 三元运算符     +1
```

### 2.1 实现位置

在 `TraverseFunctionDecl` 中，递归遍历完成后、保存统计前计算：

```cpp
FS.ccn = 1
    + FS.ifCount
    + FS.forCount
    + FS.whileCount
    + FS.doWhileCount
    + FS.caseCount
    + FS.logicalAndOrCount
    + FS.conditionalOpCount;
```

### 2.2 新增 AST 访问

```cpp
// case / default 标签
bool VisitSwitchCase(SwitchCase *SC);

// && 和 ||
bool VisitBinaryOperator(BinaryOperator *BO)
    → 仅统计 BO_LAnd / BO_LOr

// ?: 三元
bool VisitConditionalOperator(ConditionalOperator *CO);
```

---

## 3. CCN 解读

| CCN 范围 | 风险等级 | 可测试性 |
|----------|:--------:|----------|
| 1 – 5 | 简单 | 低风险，易于测试 |
| 6 – 10 | 适中 | 中等风险 |
| 11 – 20 | 较高 | 较高风险 |
| 21 – 50 | 高 | 高风险，难以充分测试 |
| 50+ | 极高 | 不可测，强烈建议重构 |

---

## 4. 报告新增内容

### 4.1 逐函数标题行

```
  ─── switch_func() — 23 行, 1 参数, CCN=8 ───
  ─── loop_func() — 15 行, 1 参数, CCN=5 ───
  ─── simple_func() — 3 行, 1 参数, CCN=1 ───
```

### 4.2 逐函数详情（新增字段）

```
    分支/循环: if:2 switch:1 case:5 break:4 return:1
    分支/循环: if:1 &&/||:2 return:3
    分支/循环: ?::2 return:1
```

### 4.3 CCN 汇总

```
  平均圈复杂度: 3.9  最高: switch_func() = 8
```

### 4.4 全局控制流（新增 3 项）

```
  case 标签:     5
  && / || 运算:  2
  ?: 三元运算:   2
```

---

## 5. 报告全景（6 段）

```
========== C 源文件分析报告 ==========

【基本信息】              ← v1.x
【代码行数统计】          ← v4.0 (代码行/空行/注释行/预处理)
【代码规范检查】          ← v3.x (7项)
【逐函数统计】            ← v2.x + CCN展示 (v4.1)
    ...
  平均圈复杂度: X.X  最高: xxx() = N
【控制流语句统计（全局汇总）】
【函数调用统计（全局汇总）】
```

---

## 6. 指标清单

### 6.1 全局汇总（31 项）

| 分类 | 指标 | 版本 |
|------|------|:--:|
| 基础 | 文件数、函数数 | v1.x |
| 参数 | 无参函数、有参函数 | v2.1 |
| 空函数 | 空函数数 | v2.1 |
| 行数 | 总行数、代码行、空行 | v4.0 |
| 行数 | 单行注释(`//`)、多行注释(`/* */`) | v4.0 |
| 行数 | 预处理指令行 | v4.0 |
| 规范 | 行数超标函数 | v3.0 |
| 规范 | 命名不规范函数 | v3.0 |
| 规范 | 全局变量缺 g_ 前缀 | v3.1 |
| 规范 | 单行超长 | v3.1 |
| 规范 | 冗余 return | v3.2 |
| 规范 | 空代码块 | v3.2 |
| 变量 | 全局变量、局部变量 | v1.x |
| 控制流 | if/for/while/do-while/switch | v1.x |
| ★控制流 | case 标签数 | v4.1 |
| ★控制流 | && / \|\| 运算符数 | v4.1 |
| ★控制流 | ?: 三元运算符数 | v4.1 |
| 控制流 | break/continue/return/goto | v1.x |
| 预处理 | #include | v1.4 |
| 调用 | 调用次数、调用分布 | v1.x |

### 6.2 逐函数明细（23 项）

```
name | lines | localVars | paramCount |
ccn | caseCount | logicalAndOrCount | conditionalOpCount |
isEmptyOrReturnOnly | isOverlong | isBadName |
hasRedundantReturn | emptyBlocks |
ifCount | forCount | whileCount | doWhileCount |
switchCount | breakCount | continueCount |
returnCount | gotoCount | callCount | callTargets
```

---

## 7. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 913 | 多文件分析 + 31 项全局 + 23 项逐函数 + 7 项规范检查 + CCN |
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
v4.1  圈复杂度     — 逐函数 CCN + case/&&/||/?: 决策点计数
```

---

*本文档版本 4.1，归档于 2026-06-17。*
