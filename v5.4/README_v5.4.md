# myclang-c++ v5.4 — 项目介绍文档

> **版本**: 5.4
> **日期**: 2026-06-20
> **适用**: `v5.4/` 目录下的所有文件
> **前序**: [v5.3](../v5.3/README_v5.3.md)
> **主题**: 参数个数检测 + 嵌套深度检测

---

## 1. 版本概述

v5.4 新增两项代码规范检查：函数参数个数上限检测和嵌套深度检测。参数过多降低可读性和可维护性；深层嵌套增加认知负担和 bug 风险。两项检查均为 AST 级——参数计数在遍历前序完成，嵌套深度通过覆写 5 个 `Traverse` 方法以 push/pop 模式跟踪。

代码规范检查从 10 项扩展到 **12 项**。

### 1.1 核心变更

| 变更 | v5.3 | v5.4 |
|------|------|------|
| 代码规范检查项 | 10 项 | **12 项** |
| 参数个数检测 | ❌ | ✅ MAX_PARAMS=5 |
| 嵌套深度检测 | ❌ | ✅ MAX_NESTING=4 |
| FunctionStats 字段 | 27 个 | **30 个** (+hasTooManyParams, +maxNesting, +hasDeepNesting) |
| AnalysisStats 字段 | 35 个 | **37 个** (+tooManyParamsFunctions, +deepNestingFunctions) |
| MyASTVisitor 字段 | — | **+currentDepth** |
| Traverse 覆写 | 0 个 | **5 个** (If/For/While/Do/Switch) |

---

## 2. 参数个数检测

### 2.1 原理

在 `TraverseFunctionDecl` 前序阶段直接从 `FunctionDecl::getNumParams()` 获取参数个数，与阈值 `MAX_PARAMS=5` 比较：

```cpp
FS.paramCount = FD->getNumParams();
if (FS.paramCount > MAX_PARAMS)
    FS.hasTooManyParams = true;
```

`VisitFunctionDecl` 中将标记同步到全局统计：

```cpp
if (CurrentFunc && CurrentFunc->hasTooManyParams)
    Stats.tooManyParamsFunctions++;
```

### 2.2 检测案例

```c
// ✗ 检测：7 个参数，超标 2 个
int bad(int a, int b, int c, int d, int e, int f, int g) {
    return a + b + c + d + e + f + g;
}

// ✓ 合规：5 个参数，刚好在限制内
int ok(int a, int b, int c, int d, int e) {
    return a + b + c + d + e;
}

// ✓ 合规：无参
int fine(void) { return 0; }
```

---

## 3. 嵌套深度检测

### 3.1 原理

五种控制流结构各自覆写 `Traverse` 方法，以 push/pop 模式维护 `currentDepth` 计数器。遍历进入时 `depth++` 并更新 `maxNesting`，递归返回后 `depth--`：

```cpp
int currentDepth = 0;

bool TraverseIfStmt(IfStmt *IS) {
    currentDepth++;
    if (CurrentFunc && currentDepth > CurrentFunc->maxNesting)
        CurrentFunc->maxNesting = currentDepth;
    bool r = RecursiveASTVisitor::TraverseIfStmt(IS);
    currentDepth--;
    return r;
}
// TraverseForStmt / TraverseWhileStmt / TraverseDoStmt / TraverseSwitchStmt 同理
```

遍历完成后，在 `TraverseFunctionDecl` 后序对比阈值：

```cpp
if (FS.maxNesting > MAX_NESTING) {
    FS.hasDeepNesting = true;
    Stats.deepNestingFunctions++;
}
```

### 3.2 深度计算示意

```c
int shallow(void) {          // depth=0
    if (x)                    // depth=1
        return 1;
    return 0;
}
// maxNesting = 1  ✓ 不报

int deep(void) {             // depth=0
    if (a) {                 // depth=1
        if (b) {             // depth=2
            if (c) {         // depth=3
                if (d) {     // depth=4
                    if (e) { // depth=5
                        if (f) { // depth=6 ← 超标
                            return 1;
                        }
                    }
                }
            }
        }
    }
    return 0;
}
// maxNesting = 6  ✗ 超标 2 层
```

嵌套深度覆盖 5 种结构，混合嵌套同样生效：

```c
int mixed(void) {
    while (cond) {      // depth=1
        if (x) {        // depth=2
            for (...) { // depth=3
                switch (y) {  // depth=4
                    case 1: break;
                }
            }
        }
    }
}
// maxNesting = 4（刚好在阈值的边界）
```

### 3.3 `main()` 的特殊情况

`main()` 没有控制流结构时深度为 0，不会误报。`TraverseFunctionDecl` 入口处 `currentDepth` 始终从 0 开始，每个函数的深度独立计算。

---

## 4. 报告输出

```
【代码规范检查】
  函数行数上限(50行):  ✓ 全部在限制内
  goto 语句:           ✓ 未使用

  函数参数上限(5个):   ⚠ 发现 1 个超标          ← 新增
    - bad(): 7 个参数 (超标 2 个)

  嵌套深度上限(4):     ⚠ 发现 1 个超标          ← 新增
    - deep(): 最大嵌套 6 层 (超标 2 层)

  命名规范:            ...
  全局变量命名:        ...
  单行长度(100字符):   ...
  冗余 return:         ...
  switch穿透:          ...
  死代码:              ...
  空代码块:            ...
  圈复杂度上限(10):    ...

逐函数:
  ─── deep() — 15 行, 3 参数, CCN=7 ⚠ 嵌套过深 ───   ← 新增标签
    局部变量: 0  嵌套深度: 6                            ← 新增字段
    分支/循环: if:5 while:1 return:2
```

---

## 5. 代码规范检查全景（12 项）

```
  函数行数上限(50行)    ← v3.0
  goto 语句             ← v3.0
  函数参数上限(5个)     ← v5.4 ★
  嵌套深度上限(4)       ← v5.4 ★
  命名规范              ← v3.0
  全局变量命名          ← v3.1
  单行长度(100字符)     ← v3.1
  冗余 return           ← v3.2 (v5.2 CFG 修复)
  switch穿透            ← v5.3
  死代码                ← v5.0/v5.1
  空代码块              ← v3.2
  圈复杂度上限(10)      ← v5.0
```

---

## 6. 指标清单

### 6.1 全局汇总（37 项）

同 v5.3 + `tooManyParamsFunctions` + `deepNestingFunctions`。

### 6.2 逐函数明细（30 项）

同 v5.3 + `hasTooManyParams` + `maxNesting` + `hasDeepNesting`。

---

## 7. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 1152 | AST+CFG 双引擎 + 12 项规范检查 + 参数/嵌套检测 |
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
v5.4  参数+嵌套    — 参数个数上限 + 嵌套深度检测
```

---

*本文档版本 5.4，归档于 2026-06-20。*
