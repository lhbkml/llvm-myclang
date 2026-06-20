# myclang-c++ v5.2 — 项目介绍文档

> **版本**: 5.2
> **日期**: 2026-06-20
> **适用**: `v5.2/` 目录下的所有文件
> **前序**: [v5.1](../v5.1/README_v5.1.md)
> **主题**: 冗余 return 检测修复——CFG 可达性过滤死 return

---

## 1. 版本概述

v5.2 修复了冗余 return 检测与死代码检测的重复标记问题。v5.1 中，同一个 `return;` 可能同时被"冗余 return"和"死代码"两个检查标记——因为它既是 void 函数的末语句、又在不可达路径上。v5.2 将冗余检测从 AST 前序阶段移到 CFG 后序阶段，以可达性为前置条件，使三种 return 检查各司其职。

### 1.1 核心变更

| 变更 | v5.1 | v5.2 |
|------|------|------|
| 冗余 return 检测时机 | 前序（AST 末语句） | **后序（CFG 可达性过滤）** |
| 死 return 是否被双重标记 | ✅ 会（冗余 + 死代码） | **❌ 不会（仅死代码）** |
| `Stats.redundantReturns` 递增位置 | `VisitFunctionDecl` | **`TraverseFunctionDecl` 后序** |
| function 字段 | 25 项 | 25 项（不变） |
| 全局字段 | 33 项 | 33 项（不变） |

---

## 2. return 三层分类

```
return 检测
├─ 死代码 ──── CFG 不可达 → "X 条语句在不可达路径上"
├─ void 冗余 ─ 可达 + void 末尾 return; → "void 函数末尾 return; 是多余的"
└─ main 冗余 ─ 可达 + main 末尾 return 0; → "末尾 return 0; 是多余的"
```

三层互斥——一个 return 只归属一层。

### 2.1 检测逻辑

```cpp
// 冗余检测：在 CFG 中定位末语句 return，验证可达性
if (auto *RS = dyn_cast<ReturnStmt>(*CS->body_end()--)) {
    bool retIsReachable = false;
    for (auto *B : *cfg) {
        for (const CFGElement &Elem : *B) {
            if (auto S = Elem.getAs<CFGStmt>()) {
                if (S->getStmt() == RS) {
                    retIsReachable = reach.isReachable(entryBlock, B);
                    goto ret_found;
                }
            }
        }
    }
    ret_found:
    if (retIsReachable) {
        // void: return;   main: return 0;
    }
    // 不可达 → 跳过，归死代码检查
}
```

### 2.2 案例

```c
void test() {
    return;   // ① 可达，不是末语句 → 不报
    return;   // ② 不可达 → 死代码
    return;   // ③ 不可达 + 末语句 → 死代码（冗余跳过）
}
```

| return | 可达 | 末语句 | v5.1 | v5.2 |
|--------|:--:|:--:|------|------|
| ① | ✅ | — | — | — |
| ② | ❌ | — | 死代码 | 死代码 |
| ③ | ❌ | ✅ | 冗余 **+** 死代码 | **仅死代码** |

---

## 3. 报告不变

外部输出格式与 v5.1 相同，仅内部检测逻辑变化。

---

## 4. 指标清单（同 v5.1）

- 全局汇总：33 项
- 逐函数明细：25 项

---

## 5. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 995 | 多文件分析 + AST+CFG 双引擎 + 8 项规范检查 + return 三层分类 |
| `IRMake.cpp` | 107 | LLVM IR 手工构建（不变，自 v1.0） |
| `Makefile` | 116 | 双项目构建 + test 目标（不变，自 v1.1） |

---

## 6. 版本演进总览

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
v5.1  死代码通用化 — 死代码从 ReturnStmt 扩展为所有语句类型
v5.2  冗余检查修复 — 冗余 return 加 CFG 可达性过滤，与死代码解耦
```

---

*本文档版本 5.2，归档于 2026-06-20。*
