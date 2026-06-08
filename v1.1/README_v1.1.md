# myclang-c++ v1.1 — 项目介绍文档

> **版本**: 1.1
> **日期**: 2026-06-07
> **适用范围**: `v1.1/` 目录下的所有文件
> **前序版本**: [v1.0](../v1.0/README_v1.0.md)

---

## 1. 版本概述

v1.1 是项目从"教学 Demo"向"实用工具"演进的重要版本。相比 v1.0 的原型验证，v1.1 在前端分析工具的功能完整性、构建系统的多目标支持、以及项目的工程化结构上都有了实质性改进。

### 1.1 版本变更快览

| 维度 | v1.0 | v1.1 | 变化性质 |
|------|------|------|----------|
| frontendAction | 即时逐条打印 AST 节点 | 收集-统计-报告 模式 | **重构** |
| 识别的 AST 节点 | 5 种 | 6 种（+WhileStmt, +CallExpr, -ReturnStmt） | **调整** |
| 输出格式 | 逐行 `llvm::outs()` | 结构化中文报告 `printReport()` | **新增** |
| Makefile | 单项目（仅 frontendAction） | 双项目（+IRMake） + test 目标 | **扩展** |
| IRMake | 手工构建 IR | 完全不变 | 维持 |
| 工程化 | 无 | `.gitignore` + `v1.0/` 归档 | **新增** |
| 已知局限 | — | `#include` 不支持（见第9章） | **遗留** |

---

## 2. 文件清单

### 2.1 核心源码

| 文件 | 大小 | 行数 | 说明 |
|------|------|------|------|
| `Makefile` | 2867B | 116 | 双项目构建系统 + test 目标 |
| `frontendAction.cpp` | 5672B | 183 | Clang C 源文件分析工具 |
| `IRMake.cpp` | 5087B | 107 | LLVM IR 手工构建工具（同 v1.0） |

### 2.2 测试/输入文件

| 文件 | 大小 | 说明 |
|------|------|------|
| `sum.c` | 79B | `sum()` + `add()` 两函数 |
| `test.c` | 84B | `write()` 系统调用的 `main()` |
| `analyze_test.c` | 1014B | 综合测试：7 函数，覆盖 if/for/while/递归/全局变量 |

### 2.3 工程文件

| 文件 | 说明 |
|------|------|
| `.gitignore` | 排除可执行文件、`.o`、`.bc`、`.ll` 构建产物 |

---

## 3. Makefile — 双项目构建系统

### 3.1 与 v1.0 的差异

```
v1.0:                     v1.1:
PROJECT = frontendAction  PROJECTS = frontendAction IRMake
default: $(PROJECT)       default: all
                          all: $(PROJECTS)
```

### 3.2 两条独立的链接规则

```makefile
# frontendAction 需要约 50 个 Clang 库
frontendAction: frontendAction.o
    $(CXX) -o $@ $^ $(LDFLAGS) $(CLANGLIBS) $(LLVMLIBS) $(SYSTEMLIBS)

# IRMake 仅需 LLVM 库，不链接 Clang
IRMake: IRMake.o
    $(CXX) -o $@ $^ $(LDFLAGS) $(LLVMLIBS) $(SYSTEMLIBS)
```

### 3.3 新增 test 目标

```makefile
test: frontendAction
    @echo "=== 测试 test.c ==="
    ./frontendAction test.c
    @echo ""
    @echo "=== 测试 sum.c ==="
    ./frontendAction sum.c
```

---

## 4. frontendAction.cpp — 核心变革

### 4.1 架构变化：即时打印 → 收集-报告

```
v1.0 模式:
  VisitXxx() → 直接 llvm::outs() 输出
  main()     → PrintStats() 输出内部统计

v1.1 模式:
  VisitXxx() → 写入 AnalysisStats 结构体
  main()     → printReport(Stats) 输出格式化中文报告
```

### 4.2 新增数据结构 `AnalysisStats`

```cpp
struct AnalysisStats {
    int totalFunctions = 0;
    int globalVars = 0;
    int ifCount = 0;
    int forCount = 0;
    int whileCount = 0;
    int callCount = 0;
    std::map<std::string, int> callTargets;
    std::vector<std::pair<std::string, int>> funcLines;
};
```

### 4.3 AST 访问者能力对比

| AST 节点 | v1.0 | v1.1 | v1.1 行为 |
|----------|:----:|:----:|-----------|
| FunctionDecl | ✅ | ✅ | 计数 + 计算函数体行数（SourceManager） |
| VarDecl | ✅ | ✅ | 仅统计**全局**变量（过滤局部变量噪音） |
| IfStmt | ✅ | ✅ | 计数（无声） |
| ForStmt | ✅ | ✅ | 计数（无声） |
| ReturnStmt | ✅ | ❌ | **删除** — 统计价值低 |
| WhileStmt | ❌ | ✅ | **新增** |
| CallExpr | ❌ | ✅ | **新增** — 含被调用函数名追踪 |

### 4.4 报告输出格式

```
========== C 源文件分析报告 ==========

【基本信息】
  输入文件: analyze_test.c
  总函数数量: 7
  全局变量数量: 3

【每个函数的代码行数】
  max(): 5 行
  min(): 4 行
  ...

【控制流语句统计】
  if 语句:    8
  for 语句:   1
  while 语句: 1
  合计:       10

【函数调用统计】
  总调用次数: 10
  调用分布:
    max(): 1 次
    min(): 1 次
    ...
========================================
```

---

## 5. IRMake.cpp — 零变化

与 v1.0 字节级完全相同。手工构建 `i32 @add(i32 %a, i32 %b)` 的 IR 模块，输出 `sum.bc`。

结构稳定，无需迭代。

---

## 6. 测试文件设计

### 6.1 设计原则

所有测试文件**刻意不使用 `#include`**，这是为了绕过 v1.1 的一个已知局限（见第9章）。

### 6.2 `analyze_test.c` 覆盖矩阵

| 函数 | FunctionDecl | IfStmt | ForStmt | WhileStmt | VarDecl(全局) | CallExpr | ReturnStmt |
|------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| `max()` | 1 | 1 | — | — | — | — | 2 |
| `min()` | 1 | 1 | — | — | — | — | 2 |
| `print_loop()` | 1 | — | 1 | — | — | — | — |
| `sum_range()` | 1 | — | — | 1 | — | — | 1 |
| `check_numbers()` | 1 | 3 | — | — | — | — | — |
| `factorial()` | 1 | 1 | — | — | — | 1 | 2 |
| `main()` | 1 | — | — | — | — | 5 | 1 |
| 全局变量 | — | — | — | — | 3 | — | — |
| **合计** | **7** | **6** | **1** | **1** | **3** | **6** | **8** |

---

## 7. 项目文件结构

```
myclang-c++/
├── v1.0/                              ← 第一版归档
│   ├── Makefile_v1.0                  (单项目构建)
│   ├── frontendAction_v1.0.cpp        (即时打印，5种节点)
│   ├── IRMake_v1.0.cpp                (手工IR构建)
│   └── README_v1.0.md                 (v1.0 完整文档)
│
├── v1.1/                              ← 当前版归档 ★
│   ├── Makefile                       (双项目构建 + test)
│   ├── frontendAction.cpp             (统计-报告模式，6种节点)
│   ├── IRMake.cpp                     (完全不变)
│   ├── sum.c | test.c | analyze_test.c
│   ├── .gitignore
│   └── README_v1.1.md                 (本文档)
│
├── Makefile                           ← 主构建文件（当前活跃版本）
├── frontendAction.cpp                 ← 主分析工具（当前活跃版本）
├── IRMake.cpp                         ← 主IR构建（当前活跃版本）
├── sum.c | test.c | analyze_test.c    ← 测试输入（活跃）
├── .gitignore
│
├── frontendAction | IRMake            ← 构建产物（gitignore）
├── *.o *.bc *.ll                      ← 构建产物（gitignore）
│
└── README_v1.0.md                     ← 根目录文档
```

---

## 8. 从 v1.0 到 v1.1 的演进路径

```
v1.0 (原型验证)
  目标: 让 Clang 和 LLVM API "跑起来"
  ├── 5 种 AST 节点逐行打印 → 验证 RecursiveASTVisitor 可用
  ├── 手工构建 1 个 IR 函数    → 验证 LLVM IR API 可用
  └── Makefile 解决链接问题    → 验证构建系统可行

        │
        │  发现问题:
        │  · 逐行打印不适合分析大型文件
        │  · 缺少 while 循环和函数调用识别
        │  · 局部变量信息噪音大
        │  · IRMake 未纳入统一构建
        ▼

v1.1 (实用化)
  目标: 从 Demo 升级为可用工具
  ├── 统计-报告架构 → 先收集数据，再格式化输出
  ├── 新增 WhileStmt + CallExpr 覆盖
  ├── 删除 ReturnStmt（价值低），VarDecl 仅计全局变量
  ├── Makefile 双项目 + test 目标
  ├── 新增 analyze_test.c 综合测试
  ├── 工程化: .gitignore + v1.0/归档 + v1.1/归档
  └── 已知局限: #include 不支持（继承自 v1.0）
```

---

## 9. 已知局限

### 9.1 `#include` 无法解析

`frontendAction.cpp` 手动初始化了 `CompilerInstance` 的所有组件，但**未配置 `HeaderSearchOpts`**，导致预处理器不知道去哪里搜索 `#include` 引用的头文件。

**影响**：只能分析"裸 C"文件（不使用任何库函数或系统头文件）。

**临时规避**：所有测试文件刻意不写 `#include`——`test.c` 用 Linux 直接系统调用替代标准库。

**修复方案**：在 `CI.createPreprocessor()` 之前添加头文件搜索路径配置。详见项目根目录的 `frontendAction.cpp`（v1.1+ 修复版）。

### 9.2 `analyze_test.c` 不在 test 目标中

`make test` 只测了 `test.c` 和 `sum.c`，综合测试文件 `analyze_test.c` 需要手动执行：
```bash
./frontendAction analyze_test.c
```

---

## 10. 依赖环境

| 组件 | 路径/版本 |
|------|----------|
| LLVM/Clang 源码 | `/home/li/llvm-project/` |
| LLVM/Clang 构建 | `/home/li/llvm-project/build/` |
| Clang 版本 | 23.0.0git (4284bd02...) |
| 目标平台 | x86_64-unknown-linux-gnu |
| 编译器 | 支持 C++17 |
| 操作系统 | WSL2 Linux (6.6.87.2-microsoft-standard-WSL2) |

---

*本文档版本 1.1，归档于 2026-06-07。*
