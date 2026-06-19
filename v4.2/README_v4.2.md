# myclang-c++ v4.2 — 项目介绍文档

> **版本**: 4.2
> **日期**: 2026-06-17
> **适用**: `v4.2/` 目录下的所有文件
> **前序**: [v4.1](../v4.1/README_v4.1.md)
> **主题**: 圈复杂度改用 CFG 控制流图计算

---

## 1. 版本概述

v4.2 将圈复杂度从 AST 节点人工枚举计数改为基于 Clang 内置 CFG (Control Flow Graph) 的图论公式计算，消除决策点定义的人为分歧。

### 1.1 核心变更

| 变更 | v4.1 | v4.2 |
|------|------|------|
| CCN 计算方式 | 枚举节点累加 | **CFG 图论公式** |
| 公式 | `1 + if + for + ... + ?:` | **`M = E − N + 2`** |
| CFG 依赖 | ❌ | ✅ `clang/Analysis/CFG.h` |
| ASTContext | 不需要 | ✅ `Ctx` 指针传入 Visitor |
| switch 多穿透 case | 每个标签 +1（可能高估） | 按出边数减 1（图论准确） |
| 含代码的同行 `/* */` 闭合判断 | 有代码 → 代码行 | 同 v4.1 |
| FunctionStats 字段 | 23 个 | 23 个（不变） |
| AnalysisStats 字段 | 31 个 | 31 个（不变） |
| 访客方法 | 17 个 Visit | 17 个 Visit（不变） |

---

## 2. CFG 图论公式

### 2.1 McCabe 原理

```
控制流图中: M = E − N + 2P
  E = 边数（每个基本块的后继数之和）
  N = 节点数（基本块总数）
  P = 连通分量数（单函数 = 1）
```

Clang 的 CFG 自动将 `&&` / `||` 短路求值、`?:` 三元、`switch` 多路分发等隐式分支展开为显式的基本块和边，无需手工枚举。

### 2.2 实现

```cpp
bool TraverseFunctionDecl(FunctionDecl *FD) {
    // ... 递归遍历，Visit 方法累加各节点计数（保留用于展示）...

    // 基于 CFG 计算 CCN
    CFG::BuildOptions opts;
    opts.AddEHEdges = false;
    opts.AddImplicitDtors = false;
    opts.AddStaticInitBranches = false;
    opts.PruneTriviallyFalseEdges = false;
    std::unique_ptr<CFG> cfg = CFG::buildCFG(FD, FD->getBody(), Ctx, opts);
    if (cfg) {
        int nodes = 0, edges = 0;
        for (auto it = cfg->begin(); it != cfg->end(); ++it) {
            nodes++;
            edges += (*it)->succ_size();
        }
        FS.ccn = edges - nodes + 2;
        if (FS.ccn < 1) FS.ccn = 1;
    }
}
```

### 2.3 与计数法对比

| 场景 | 计数法 (v4.1) | CFG 法 (v4.2) | 说明 |
|------|:--:|:--:|------|
| `if (a && b)` | +2 (if + &&) | +2 | 一致：短路产生两条边 |
| `a ? b : c` | +1 (?:) | +1 | 一致 |
| switch 5 case（含穿透） | +5 | +4 | CFG 更准：5 条出边 = 4 个独立路径 |
| 简单 `return` | 1 | 1 | 一致 |

---

## 3. 架构变化

```
Visitor                           v4.1        v4.2
  ├─ SourceManager *SM            ✓           ✓
  ├─ ASTContext *Ctx              —           ✓ (新增)
  ├─ 17 个 Visit 方法             ✓           ✓ (展示用，不参与 CCN)
  ├─ CCN = 1 + if + for + ...     ✓           —
  └─ CCN = E − N + 2              —           ✓
```

决策点计数（`caseCount` / `logicalAndOrCount` / `conditionalOpCount`）仍保留，继续在报告的"分支/循环"行展示，但不再参与 CCN 计算——二者解耦。

---

## 4. 报告输出（不变）

```
【逐函数统计】
  ─── switch_func() — 23 行, 1 参数, CCN=7 ───
    局部变量: 1
    分支/循环: if:2 switch:1 case:5 break:4 return:1
    函数调用: 0 次

  平均圈复杂度: 3.7  最高: switch_func() = 7
```

---

## 5. 指标清单

### 5.1 全局汇总（31 项，同 v4.1）

| 分类 | 指标 | 版本 |
|------|------|:--:|
| 基础 | 文件数、函数数 | v1.x |
| 参数 | 无参函数、有参函数 | v2.1 |
| 空函数 | 空函数数 | v2.1 |
| 行数 | 总行数、代码行、空行 | v4.0 |
| 行数 | 单行注释 / 多行注释 | v4.0 |
| 行数 | 预处理指令行 | v4.0 |
| 规范 | 行数超标、命名、g_ 前缀、长行、冗余 return、空代码块 | v3.x |
| 变量 | 全局变量、局部变量 | v1.x |
| 控制流 | if/for/while/do-while/switch/case/&&\|\|/?: | v1.x / v4.1 |
| 控制流 | break/continue/return/goto | v1.x |
| 预处理 | #include | v1.4 |
| 调用 | 调用次数、调用分布 | v1.x |

### 5.2 逐函数明细（23 项，同 v4.1）

```
name | lines | localVars | paramCount |
ccn (CFG 计算) | caseCount | logicalAndOrCount | conditionalOpCount |
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
| `frontendAction.cpp` | 922 | 多文件分析 + 31 项全局 + 23 项逐函数 + CCN (CFG) |
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
```

---

*本文档版本 4.2，归档于 2026-06-17。*
