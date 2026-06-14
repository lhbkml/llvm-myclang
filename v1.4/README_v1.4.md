# myclang-c++ v1.4 — 项目介绍文档

> **版本**: 1.4
> **日期**: 2026-06-08
> **适用范围**: `v1.4/` 目录下的所有文件
> **前序版本**: [v1.3](../v1.3/README_v1.3.md)
> **主题**: 扩展 AST 节点覆盖 + 新增 `#include` 预处理统计

---

## 1. 版本概述

v1.4 将分析指标从 v1.3 的 10 项扩展到 **13 项**，新增了 6 种 AST 节点和 1 种预处理事件。核心亮点是引入了 `PPCallbacks` 机制——这是与 `RecursiveASTVisitor` 完全不同的事件监听系统。

### 1.1 新增能力

| # | 统计项 | Clang 机制 | 类型 |
|---|--------|-----------|------|
| 4 | do-while 语句 | `VisitDoStmt` | AST Visitor |
| 5 | switch 语句 | `VisitSwitchStmt` | AST Visitor |
| 6 | break 语句 | `VisitBreakStmt` | AST Visitor |
| 7 | continue 语句 | `VisitContinueStmt` | AST Visitor |
| 8 | return 语句 | `VisitReturnStmt` | AST Visitor |
| 9 | goto 语句 | `VisitGotoStmt` | AST Visitor |
| — | **#include 头文件引入** | `PPCallbacks::InclusionDirective` | **预处理器回调** |

### 1.2 双轨制统计架构

```
                          输入文件
                             │
                  ┌──────────┴──────────┐
                  │                     │
            预处理阶段               语法分析阶段
          (PPCallbacks)          (RecursiveASTVisitor)
                  │                     │
          InclusionDirective     VisitFunctionDecl
                  │              VisitVarDecl
                  │              VisitIfStmt / ForStmt / ...
                  │              VisitCallExpr
                  │                     │
                  ▼                     ▼
             #include 数量         9种控制流 + 2种声明 + 1种表达式
                  │                     │
                  └──────────┬──────────┘
                             ▼
                    AnalysisStats 统一收集
                             │
                             ▼
                      printReport()
```

---

## 2. 完整指标清单（13 项）

### 2.1 基本信息（3 项）

| 指标 | 数据来源 |
|------|---------|
| 函数数量 | `VisitFunctionDecl` + `isFromMainFile` |
| 全局变量数量 | `VisitVarDecl` + `isTranslationUnit` + `isFromMainFile` |
| `#include` 数量 | `PPCallbacks::InclusionDirective` |

### 2.2 控制流语句（9 项）

| 指标 | Visit 方法 |
|------|-----------|
| if 语句 | `VisitIfStmt` |
| for 语句 | `VisitForStmt` |
| while 语句 | `VisitWhileStmt` |
| do-while 语句 | `VisitDoStmt` |
| switch 语句 | `VisitSwitchStmt` |
| break 语句 | `VisitBreakStmt` |
| continue 语句 | `VisitContinueStmt` |
| return 语句 | `VisitReturnStmt` |
| goto 语句 | `VisitGotoStmt` |

### 2.3 表达式（1 项）

| 指标 | Visit 方法 |
|------|-----------|
| 函数调用 | `VisitCallExpr`（含被调用函数名追踪） |

---

## 3. PPCallbacks：预处理事件拦截

### 3.1 与 RecursiveASTVisitor 的本质区别

| 维度 | RecursiveASTVisitor | PPCallbacks |
|------|-------------------|-------------|
| 处理对象 | AST 语法树节点 | 预处理器的词法/预处理事件 |
| 触发时机 | 解析完成后，遍历 AST | 预处理过程中，实时回调 |
| 能捕获的事件 | 函数、变量、语句、表达式 | `#include`、`#define`、`#if`、宏展开、pragma |
| 注册方式 | `CI.setASTConsumer()` | `CI.getPreprocessor().addPPCallbacks()` |

### 3.2 实现

```cpp
class IncludeCounter : public PPCallbacks {
public:
    int &Count;
    explicit IncludeCounter(int &C) : Count(C) {}

    void InclusionDirective(SourceLocation HashLoc,
                            const Token &IncludeTok,
                            StringRef FileName,
                            bool IsAngled,
                            CharSourceRange FilenameRange,
                            OptionalFileEntryRef File,
                            StringRef SearchPath,
                            StringRef RelativePath,
                            const Module *SuggestedModule,
                            bool ModuleImported,
                            SrcMgr::CharacteristicKind FileType) override {
        Count++;
    }
};

// 注册时机：createPreprocessor() 之后，ParseAST() 之前
CI.getPreprocessor().addPPCallbacks(
    std::make_unique<IncludeCounter>(Stats.includeCount));
```

### 3.3 `InclusionDirective` 回调参数

| 参数 | 说明 | 示例 |
|------|------|------|
| `HashLoc` | `#` 号的源码位置 | — |
| `FileName` | 被包含的文件名 | `"stdio.h"` |
| `IsAngled` | `true` = `<>`, `false` = `""` | `true` |
| `SearchPath` | 实际找到的路径 | `"/usr/include/stdio.h"` |
| `File` | 被包含文件的 FileEntryRef | — |

---

## 4. 报告输出示例

```
========== C 源文件分析报告 ==========

【基本信息】
  输入文件: output/analyze_with_include.c
  总函数数量: 3
  全局变量数量: 1
  #include 数量: 2

【每个函数的代码行数】
  factorial(): 5 行
  print_hello(): 3 行
  main(): 25 行

【控制流语句统计】
  if 语句:       2
  for 语句:      1
  while 语句:    1
  do-while 语句: 0
  switch 语句:   0
  break 语句:    0
  continue 语句: 0
  return 语句:   3
  goto 语句:     0
  合计:          7

【函数调用统计】
  总调用次数: 4
  调用分布:
    factorial(): 2 次
    print_hello(): 1 次
    printf(): 1 次
========================================
```

---

## 5. 文件清单

| 文件 | 说明 |
|------|------|
| `Makefile` | 双项目构建 + test 目标 |
| `frontendAction.cpp` | 方案 B + 13 项指标分析 |
| `IRMake.cpp` | LLVM IR 手工构建（不变） |

---

## 6. 版本演进总览

```
v1.0  原型验证
  └── 5 种 AST 节点逐行打印
      └── v1.1  实用化
          └── 统计-报告架构，6 种 AST 节点
              └── v1.2  突破
                  └── 方案 A：手动 HeaderSearchOpts，支持 #include
                      └── v1.3  成熟化
                          └── 方案 B：CreateFromArgs 自动发现
                              └── v1.4  ★ 扩展
                                  └── +6 种 AST 节点 + PPCallbacks
                                      共 13 项指标，覆盖 C 语言主要语法结构
```

---

*本文档版本 1.4，归档于 2026-06-08。*
