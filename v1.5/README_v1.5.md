# myclang-c++ v1.5 — 项目介绍文档

> **版本**: 1.5
> **日期**: 2026-06-15
> **适用**: `v1.5/` 目录下的所有文件
> **前序**: [v1.4](../v1.4/README_v1.4.md)
> **主题**: 多文件分析 + Stmt 级主文件过滤 + 变量拆分统计

---

## 1. 版本概述

v1.5 是项目从"单文件工具"升级为"多文件批量分析工具"的关键版本。同时修复了 v1.4 的控制流语句泄漏问题（头文件中的 return/if/... 也被计入），并将全局变量和局部变量拆分为独立指标。

### 1.1 核心变更

| 变更 | v1.4 | v1.5 |
|------|------|------|
| 命令行输入 | `cl::opt<string>` 单文件 | `cl::list<string>` 多文件 |
| 代码架构 | 全部在 `main()` 中 | `analyzeFile()` 函数提取 |
| 统计聚合 | 无 | `operator+=` 跨文件累加 |
| 相对 `#include` | ❌ 不支持 | ✅ 自动添加父目录到搜索路径 |
| Stmt 过滤 | ❌ 头文件语句被计入 | ✅ `isStmtFromMainFile()` 全部过滤 |
| 变量统计 | 仅全局变量 | 全局变量 + 局部变量（不含参数） |
| 指标总数 | 13 | **14** |

---

## 2. 指标清单（14 项）

### 2.1 基本信息（4 项）

| 指标 | 数据来源 |
|------|---------|
| 函数数量 | `VisitFunctionDecl` + `isFromMainFile` |
| 全局变量数量 | `VisitVarDecl` + `isTranslationUnit` + `isFromMainFile` |
| 局部变量数量 | `VisitVarDecl` + `!isTranslationUnit` + `!isa<ParmVarDecl>` |
| `#include` 数量 | `PPCallbacks::InclusionDirective` |

### 2.2 控制流语句（9 项）— 全部有 Stmt 级过滤

| 指标 | Visit 方法 | 过滤 |
|------|-----------|:--:|
| if | `VisitIfStmt` | `isStmtFromMainFile` |
| for | `VisitForStmt` | ✅ |
| while | `VisitWhileStmt` | ✅ |
| do-while | `VisitDoStmt` | ✅ |
| switch | `VisitSwitchStmt` | ✅ |
| break | `VisitBreakStmt` | ✅ |
| continue | `VisitContinueStmt` | ✅ |
| return | `VisitReturnStmt` | ✅ |
| goto | `VisitGotoStmt` | ✅ |

### 2.3 表达式（1 项）

| 指标 | Visit 方法 | 过滤 |
|------|-----------|:--:|
| 函数调用 | `VisitCallExpr` | `isStmtFromMainFile` |

---

## 3. 架构演进：单文件 → 多文件

### 3.1 v1.4 架构

```
main()
├── CreateFromArgs (1次)
├── CompilerInstance 初始化
└── ParseAST (1个文件)
```

### 3.2 v1.5 架构

```
main()
├── CreateFromArgs (1次，共享系统路径)
├── for each file:
│   └── analyzeFile(file, SharedInvocation) → fileStats
│       ├── 新建 CompilerInstance
│       ├── 继承系统路径
│       ├── + 添加文件父目录（支持相对 #include "..."）
│       ├── + 注册 IncludeCounter (PPCallbacks)
│       └── ParseAST → 返回 stats
│
├── totalStats += fileStats (operator+= 累加)
└── printReport(totalStats, fileList)
```

### 3.3 `analyzeFile()` 函数

```cpp
static bool analyzeFile(const std::string &FilePath,
                        const CompilerInvocation &SharedInvocation,
                        AnalysisStats &Stats) {
    // 1. 独立 CompilerInstance（互不污染）
    // 2. 继承共享的 HeaderSearchOpts（系统路径只探测一次）
    // 3. 额外添加文件父目录到搜索路径
    // 4. 注册 IncludeCounter → Stats.includeCount
    // 5. ParseAST → Stats 被 MyASTVisitor 填充
}
```

### 3.4 双轨制过滤体系

```
Decl* (声明)               Stmt* (语句/表达式)
─────────────               ──────────────────
VisitFunctionDecl           VisitIfStmt
VisitVarDecl                VisitForStmt / WhileStmt / DoStmt
    │                       VisitSwitchStmt
    ▼                       VisitBreakStmt / ContinueStmt
isFromMainFile(D)           VisitReturnStmt / GotoStmt
    │                       VisitCallExpr
    ▼                           │
过滤头文件中的函数声明           ▼
过滤头文件中的全局变量        isStmtFromMainFile(S)
                                │
                                ▼
                            过滤头文件中的所有语句和表达式
```

---

## 4. 多文件测试套件

```
output/
├── mylib.h             ← 共享头文件（2函数 + 1全局变量）
├── file1_loops.c       ← for/while/do-while/continue/break/goto
├── file2_switch.c      ← switch/case/break/if-else/函数调用
└── file3_main.c        ← #include <stdio.h> + "mylib.h" 主入口
```

### 4.1 单文件运行

```bash
$ ./frontendAction output/file3_main.c
函数: 1 (main)    全局: 1 (app_name)    局部: 3 (a,b,total)
#include: 2        if:1  return:1       调用: loop_test/printf/switch_test
```

### 4.2 多文件运行

```bash
$ ./frontendAction output/file1_loops.c output/file2_switch.c output/file3_main.c
函数: 3 | 全局: 1 | 局部: 8 | #include: 4
for:1 while:1 do-while:1 switch:1 break:5 continue:1 goto:1 return:4 if:5
```

---

## 5. v1.4 → v1.5 关键修复

### 修复：Stmt 泄漏

v1.4 中 `VisitReturnStmt` 等 10 个方法没有 `isFromMainFile` 过滤，导致头文件（stdio.h、mylib.h）中的语句也被计入。

```
v1.4: ./frontendAction file3_main.c → return: 3   (main=1 + mylib.h=2)
v1.5: ./frontendAction file3_main.c → return: 1   (只计 main)
```

### 新增：局部变量统计

```
VisitVarDecl:
  if (!isFromMainFile)  return;        // 过滤头文件
  if (isa<ParmVarDecl>) return;        // 过滤函数参数
  if (isTranslationUnit) globalVars++; // 全局变量
  else                   localVars++;  // 局部变量（含 static local）
```

---

## 6. 文件清单

| 文件 | 说明 |
|------|------|
| `Makefile` | 双项目构建 + test 目标 |
| `frontendAction.cpp` | 多文件分析 + 14 项指标 |
| `IRMake.cpp` | LLVM IR 手工构建（不变） |

---

## 7. 版本演进总览

```
v1.0  原型验证     — 5种AST节点逐行打印
v1.1  实用化       — 统计-报告架构，6种节点，双项目构建
v1.2  突破(#include) — 方案A：手动HeaderSearchOpts
v1.3  成熟化       — 方案B：CreateFromArgs自动发现
v1.4  扩展         — 13项指标 + PPCallbacks
v1.5  ★ 多文件     — 批量分析 + Stmt过滤 + 变量拆分
```

---

*本文档版本 1.5，归档于 2026-06-15。*
