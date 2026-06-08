# myclang-c++ v1.2 — 项目介绍文档

> **版本**: 1.2
> **日期**: 2026-06-07
> **适用范围**: `v1.2/` 目录下的所有文件
> **前序版本**: [v1.1](../v1.1/README_v1.1.md)
> **主题**: `#include` 支持（方案A：手动 HeaderSearchOpts）

---

## 1. 版本概述

v1.2 攻克了项目自 v1.0 以来最关键的遗留问题——**前端分析工具终于能解析带 `#include` 的 C 文件了**。

同时，这是一个"方案选型"的关键节点：v1.2 采用了**方案 A（手动配置 `HeaderSearchOpts`）**，它的后续版本 v1.3 将升级为**方案 B（`CompilerInvocation::CreateFromArgs` 自动发现）**。

### 1.1 版本变更快览

| 维度 | v1.1 | v1.2 |
|------|------|------|
| `#include` 支持 | ❌ 不支持 | ✅ 支持（方案 A — 手动 AddPath） |
| 头文件符号过滤 | ❌ 头文件函数也被统计 | ✅ `isFromMainFile()` 过滤 |
| 诊断处理 | 默认 `createDiagnostics()` | `IgnoringDiagConsumer` 静默 |
| 头文件搜索路径 | 0 条（导致 `#include` 失败） | 4 条硬编码路径 |
| 新测试文件 | — | `analyze_with_include.c` |
| 目录结构 | 文件散落根目录 | `output/` 统一收纳 |
| IRMake | 不变 | 不变 |
| Makefile | 不变（仅 test 路径调整） | 不变 |

---

## 2. 文件清单

### 2.1 核心源码

| 文件 | 大小 | 行数 | 说明 |
|------|------|------|------|
| `Makefile` | ~2870B | 116 | 双项目构建 + test 目标 |
| `frontendAction.cpp` | ~7KB | 223 | **核心** — 方案 A 实现的 `#include` 支持 |
| `IRMake.cpp` | 5087B | 107 | 不变 |

### 2.2 测试文件（output/）

| 文件 | 说明 |
|------|------|
| `sum.c` | 最简求和函数 |
| `test.c` | `write()` 系统调用 |
| `analyze_test.c` | 综合测试（无 `#include`） |
| `analyze_with_include.c` | **★ 新增** — 带 `<stdio.h>` `<stdlib.h>` 的测试 |

### 2.3 构建产物（output/）

| 文件 | 说明 |
|------|------|
| `sum.bc`, `sum.ll` | Clang -O0 编译 `sum.c` |
| `sum-O1.bc`, `sum-O1.ll` | Clang -O1 编译 `sum.c` |
| `sum-O2.ll` | Clang -O2 编译 `sum.c` |
| `sum-fn.bc`, `sum-fn.ll`, `sum-fn.o` | IRMake 手工生成 |
| `sum-tmp.bc` | 临时输出 |

---

## 3. 核心变革：方案 A 实现

### 3.1 问题回顾

v1.1 的 `CompilerInstance` 手动初始化了所有组件，唯独遗漏了 `HeaderSearchOpts` 配置。预处理器没有搜索路径，遇到 `#include <stdio.h>` 直接失败。

### 3.2 修复架构

```
main()
├── CompilerInstance 初始化（同 v1.1）
│   ├── createDiagnostics() → IgnoringDiagConsumer   ★ 新增
│   ├── Target, VFS, FileManager, SourceManager
│   ├── setMainFileID
│   │
│   ├── ★★★ HeaderSearchOpts 配置 ★★★               ← v1.2 核心变更
│   │   ├── Clang 内置头文件路径
│   │   ├── 系统 C 头文件路径
│   │   ├── GCC 工具链头文件路径
│   │   └── /usr/local/include
│   │
│   ├── createPreprocessor()                          ← 现在能解析 #include
│   └── Sema + ParseAST
│
└── printReport()
    └── isFromMainFile() 过滤头文件符号               ★ 新增
```

### 3.3 三处代码修改

#### 变更 ①：诊断静默化

```cpp
// v1.1:
CI.createDiagnostics();

// v1.2:
CI.createDiagnostics();
CI.getDiagnostics().setClient(
    new clang::IgnoringDiagConsumer(), true);
```

**为什么需要？** 加了头文件路径后，预处理器解析系统头文件时可能产生诊断（warnings 等）。默认的 `TextDiagnosticPrinter` 在处理非主文件诊断时会触发 assertion 崩溃。用 `IgnoringDiagConsumer` 吞掉所有诊断，因为分析工具不需要关心编译警告。

#### 变更 ②：头文件搜索路径

```cpp
auto &HSOpts = CI.getHeaderSearchOpts();

// ① Clang 内置头文件
HSOpts.AddPath("/home/li/llvm-project/build/lib/clang/23/include",
               clang::frontend::System, false, false);

// ② 系统 C 头文件
HSOpts.AddPath("/usr/include",
               clang::frontend::System, false, false);

// ③ GCC 工具链头文件
HSOpts.AddPath("/usr/lib/gcc/x86_64-linux-gnu/13/include",
               clang::frontend::System, false, false);

// ④ 用户安装的第三方库
HSOpts.AddPath("/usr/local/include",
               clang::frontend::System, false, false);
```

**API 签名（Clang 23）：**
```cpp
void HeaderSearchOptions::AddPath(
    StringRef Path,                  // 目录路径
    frontend::IncludeDirGroup Group, // System / Angled / CSystem / ...
    bool IsFramework,                // macOS Framework? Linux 一律 false
    bool IgnoreSysRoot               // 是否忽略 --sysroot
);
```

| `IncludeDirGroup` | `#include ""` | `#include <>` |
|-------------------|:---:|:---:|
| `frontend::System` | ✅ | ✅ |
| `frontend::Angled` | | ✅ |
| `frontend::CSystem` | | ✅ (抑制警告) |

#### 变更 ③：主文件过滤

```cpp
// 辅助函数
bool isFromMainFile(Decl *D) {
    if (!SM) return true;
    return SM->isInMainFile(D->getLocation());
}

// VisitFunctionDecl 中：
if (!isFromMainFile(FD)) return true;   // 跳过头文件中的函数

// VisitVarDecl 中：
if (...&& isFromMainFile(VD))           // 全局变量也过滤
    Stats.globalVars++;
```

**为什么需要？** 加了 `#include <stdio.h>` 之后，`ParseAST` 会解析整个翻译单元——包括 stdio.h 里声明的所有函数和变量。不加过滤的话，统计结果会包含头文件中的 `__bswap_16()`, `__uint16_identity()` 等数百个内部函数。

`SourceManager::isInMainFile()` 检查某个声明的源码位置是否在主文件中（即用户写的 `.c` 文件，而非 `#include` 进来的头文件）。

### 3.4 验证结果

**测试 `analyze_with_include.c`：**

```
$ ./frontendAction output/analyze_with_include.c

========== C 源文件分析报告 ==========

【基本信息】
  输入文件: analyze_with_include.c
  总函数数量: 3           ← 正确：只统计 factorial, print_hello, main
  全局变量数量: 1          ← 正确：global_counter

【每个函数的代码行数】
  factorial(): 5 行
  print_hello(): 3 行
  main(): 25 行

【控制流语句统计】
  if 语句:    2
  for 语句:   1
  while 语句: 1
  合计:       4

【函数调用统计】
  总调用次数: 4
  调用分布:
    factorial(): 2 次
    print_hello(): 1 次
    printf(): 1 次       ← 能识别头文件中声明的函数调用！

========================================
```

---

## 4. 方案 A 的局限

| 局限 | 后果 |
|------|------|
| 路径硬编码 | 换机器/换 LLVM 版本/换 GCC 版本后代码需要修改 |
| 路径不完整 | 只加了 4 条，漏了 `/usr/include/x86_64-linux-gnu` 等架构特定路径 |
| 无法扩展 | 用户不能通过 `-I` `-D` `-std=` 等参数自定义 |
| 依赖外部构建环境 | 依赖 LLVM build 目录的位置不变 |

这些局限正是**方案 B（`CompilerInvocation::CreateFromArgs`）**要解决的问题。

---

## 5. 头文件搜索路径的层次结构

```
用户写: #include <stdio.h>
         │
         ▼
预处理器按 HeaderSearchOpts 中的顺序搜索:
         │
  ① ─── /home/li/llvm-project/build/lib/clang/23/include/
         ├── stddef.h        ← stdio.h 内部需要
         ├── stdarg.h        ← stdio.h 内部需要
         ├── stdint.h
         └── limits.h
         │
  ② ─── /usr/include/
         ├── stdio.h         ← ★ 找到了！
         ├── stdlib.h
         └── string.h
         │
  ③ ─── /usr/lib/gcc/x86_64-linux-gnu/13/include/
         └── stddef.h 等 GCC 特有定义
         │
  ④ ─── /usr/local/include/
         └── 用户安装的第三方库
```

> **关键依赖链**：`stdio.h` 内部 `#include <stddef.h>`，`stddef.h` 由 Clang 内置提供（不是系统 GCC 的那个）。所以路径 ① 是**绝对不可缺少的**——没有它，即使找到了 `stdio.h`，预处理器也会在解析 `stdio.h` 的内部 `#include` 时报错。

---

## 6. 项目文件结构

```
myclang-c++/
├── v1.0/                              ← 第一版
├── v1.1/                              ← 统计-报告模式
├── v1.2/                              ← ★ 方案 A: #include 支持
│   ├── Makefile
│   ├── frontendAction.cpp             (HeaderSearchOpts 手动配置)
│   ├── IRMake.cpp
│   ├── .gitignore
│   └── output/                        (测试文件 + 构建产物)
│
├── Makefile                           ← 当前活跃版本
├── frontendAction.cpp                 ← 当前活跃版本
├── IRMake.cpp
└── output/                            ← 活跃测试文件
```

---

## 7. 版本演进路径

```
v1.0  原型验证
  └── 让 Clang AST 和 LLVM IR "跑起来"
      └── v1.1  实用化重构
          └── 统计-报告架构，6 种 AST 节点
              └── v1.2  ★ 当前
                  └── 方案 A：手动配置 HeaderSearchOpts
                      └── v1.3  (下一版)
                          └── 方案 B：CreateFromArgs 自动发现
```

---

*本文档版本 1.2，归档于 2026-06-07。*
