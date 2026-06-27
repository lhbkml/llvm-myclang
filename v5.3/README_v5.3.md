# myclang-c++ v5.3 — 项目介绍文档

> **版本**: 5.3
> **日期**: 2026-06-20
> **适用**: `v5.3/` 目录下的所有文件
> **前序**: [v5.2](../v5.2/README_v5.2.md)
> **主题**: switch case 穿透检测

---

## 1. 版本概述

v5.3 新增 switch case 穿透（fall-through）检测。利用 CFG 块后继关系识别 case 之间缺少 break/return/goto 的穿透路径——这是 C 代码中常见的 bug 来源。

代码规范检查从 8 项扩展到 **9 项**。

### 1.1 核心变更

| 变更 | v5.2 | v5.3 |
|------|------|------|
| 代码规范检查项 | 8 项 | **9 项** |
| switch 穿透检测 | ❌ | ✅ CFG 块后继分析 |
| FallThroughInfo 结构体 | — | ✅ `fromCase` / `toCase` |
| FunctionStats 字段 | 25 个 | **27 个** (+hasFallThrough, +fallThroughCount) |
| AnalysisStats 字段 | 33 个 | **35 个** (+fallThroughCount, +fallThroughs) |
| 新增依赖 | — | `<set>` |

---

## 2. 穿透检测原理

### 2.1 CFG 模型

```
switch (x) {
    case 1:         // B1 (case 块)
        r = 10;     // B2 (body 块)
    case 2:         // B3 (case 块)
        r = 20;
        break;      // → exit
}
```

CFG：`B1 → B2 → B3 → break → exit`

B2（body 块）的后继是 B3（case 块）→ **穿透**。

```
switch (x) {
    case 1: return 10;   // B1 → exit (无穿透)
    case 2: return 20;   // B3 → exit
}
```

B1 的后继不是 case 块 → **无穿透**。

### 2.2 算法

```cpp
// 1. 收集所有 case 块
caseBlocks = { B | B.label is SwitchCase }

// 2. 标记 dispatch 块（switch 条件块，有 ≥2 个 case 后继）
dispatchBlocks = { B | B 有 ≥2 个后继属于 caseBlocks }

// 3. 遍历所有非 dispatch 块
for each B (not in dispatchBlocks):
    if B has successor in caseBlocks:
        → fall-through!
```

只跳过 dispatch 块——case 块本身也参与检查（case→case 直接穿透）。

---

## 3. 检测案例

```c
// ✓ 检测: case 无 break
int f(int x) {
    switch (x) {
        case 1: r = 10;    // 穿透 → case 2
        case 2: r = 20; break;
    }
}

// ✓ 检测: 多重穿透
int g(int x) {
    switch (x) {
        case 1:
        case 2: r = 20; break;   // case 1→2 穿透
    }
}

// ✗ 不误报: 全部有 break/return
int h(int x) {
    switch (x) {
        case 1: return 10;
        case 2: return 20;
    }
}
```

---

## 4. 报告输出

```
  死代码:     ⚠ 发现 X 条不可达语句
  switch穿透: ⚠ 发现 3 处            ← 新增
    - missing_break(): 1 处穿透
    - multi_fallthrough(): 2 处穿透
  空代码块:   ✓ 未发现

逐函数:
  ─── missing_break() — 14 行, 1 参数, CCN=3 ⚠ switch穿透 ───  ← 新增标签
```

---

## 5. 代码规范检查全景（9 项）

```
  函数行数上限(50行)    ← v3.0
  goto 语句             ← v3.0
  命名规范              ← v3.0
  全局变量命名          ← v3.1
  单行长度(100字符)     ← v3.1
  冗余 return           ← v3.2 (v5.2 CFG 修复)
  死代码                ← v5.0/v5.1
  switch穿透            ← v5.3 ★
  空代码块              ← v3.2
  圈复杂度上限(10)      ← v5.0
```

---

## 6. 指标清单

### 6.1 全局汇总（35 项）

同 v5.2 + `fallThroughCount` + `fallThroughs`。

### 6.2 逐函数明细（27 项）

同 v5.2 + `hasFallThrough` + `fallThroughCount`。

---

## 7. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 1055 | AST+CFG 双引擎 + 9 项规范检查 + switch 穿透检测 |
| `IRMake.cpp` | 107 | LLVM IR 手工构建（不变，自 v1.0） |
| `Makefile` | 116 | 双项目构建 + test 目标（不变，自 v1.1） |

---

## 8. 版本演进总览

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
```

---

*本文档版本 5.3，归档于 2026-06-20。*
