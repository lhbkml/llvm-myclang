# myclang-c++ v1.3 — 项目介绍文档

> **版本**: 1.3  
> **日期**: 2026-06-07  
> **适用范围**: `v1.3/` 目录下的所有文件  
> **前序版本**: [v1.2](../v1.2/README_v1.2.md)  
> **主题**: `#include` 支持（方案B：CompilerInvocation::CreateFromArgs 自动发现）

---

## 1. 版本概述

v1.3 用**方案 B** 重写了 `frontendAction.cpp` 的头文件搜索路径配置逻辑，从"手动硬编码 4 条路径"升级为"让 Clang 自己通过工具链探测自动发现所有路径"。

这是方案选型的最终版本——方案 A（v1.2，手动）验证了可行性，方案 B（v1.3）提供了可移植性。

### 1.1 方案 B vs 方案 A

| 维度 | 方案 A (v1.2) | 方案 B (v1.3) |
|------|:---:|:---:|
| 头文件路径配置 | 4 条手动 `AddPath()` | 1 行：`CI.getHeaderSearchOpts() = Invocation->getHeaderSearchOpts()` |
| 路径来源 | 硬编码字符串 | `CreateFromArgs` 自动工具链探测 |
| Target 设置 | 手动 `getDefaultTargetTriple()` | 从 `Invocation->getTargetOpts()` 读取 |
| 发现路径数 | 4 条（漏了架构特定路径） | 4+ 条（含 `x86_64-linux-gnu` 等） |
| 可移植性 | ❌ 换机器需改代码 | ✅ 自动适配 |
| 扩展性 | ❌ 用户无法加 `-I`/`-D` | ✅ 支持自定义参数 |

### 1.2 核心 API

```cpp
// CompilerInvocation::CreateFromArgs — Clang 驱动的参数解析入口
//
// 自动完成：
//   ① 目标平台探测     → TargetOpts (triple, CPU, features)
//   ② 资源目录定位     → Clang 内置头文件路径
//   ③ GCC 工具链探测   → 系统头文件路径
//   ④ 语言选项设置     → LangOpts (C99/C11/...)
//   ⑤ 预处理器选项     → HeaderSearchOpts
//   ⑥ 代码生成选项     → CodeGenOpts
//
bool CompilerInvocation::CreateFromArgs(
    CompilerInvocation &Res,           // [out] 被填充的完整配置
    ArrayRef<const char *> Args,       // [in]  命令行参数
    DiagnosticsEngine &Diags);         // [in]  诊断引擎
```

### 1.3 自动发现的路径（对比方案A）

```
方案A 手动4条:                       方案B 自动发现:
  clang/23/include        ✅          clang/23/include        ✅ (自动)
  /usr/include            ✅          /usr/include            ✅ (自动)
  gcc/.../13/include      ✅          /usr/local/include      ✅ (自动)
  /usr/local/include      ✅          /usr/include/x86_64...  ✅ (★新增)
```

方案 B 多发现了 `/usr/include/x86_64-linux-gnu`（架构特定头文件），这是方案 A 遗漏的。

---

## 2. 文件清单

| 文件 | 说明 |
|------|------|
| `Makefile` | 双项目构建系统 + test 目标 |
| `frontendAction.cpp` | **方案 B 实现** — CreateFromArgs 自动发现 |
| `IRMake.cpp` | LLVM IR 手工构建（不变） |

### 2.1 头文件依赖变化

```diff
v1.2:                                     v1.3:
  llvm/TargetParser/Host.h                 CompilerInvocation.h  ← 新增
  Lex/HeaderSearchOptions.h                (其余相同)
  llvm/Support/FileSystem.h
```

---

## 3. 关键代码

### 3.1 自动发现 + 注入

```cpp
// 1. 构造虚拟命令行
std::vector<const char *> FakeArgs = {
    "frontendAction",
    "-fsyntax-only",
    FileName.c_str(),
};

// 2. 让 Clang 自己解析，自动发现所有配置
CompilerInstance TmpCI;
TmpCI.createDiagnostics();
auto Invocation = std::make_shared<CompilerInvocation>();
CompilerInvocation::CreateFromArgs(*Invocation, FakeArgs,
                                    TmpCI.getDiagnostics());

// 3. 从 Invocation 读取自动探测的结果
auto &TargetOpts = Invocation->getTargetOpts();         // 目标平台
CI.getHeaderSearchOpts() = Invocation->getHeaderSearchOpts(); // ★ 头文件路径
```

### 3.2 其余部分与 v1.2 完全相同

- `AnalysisStats` 数据结构
- `MyASTVisitor`（6 种 AST 节点 + `isFromMainFile` 过滤）
- `MyASTConsumer`
- `printReport()` 格式化报告
- `main()` 中 `ParseAST()` 调用
- `IgnoringDiagConsumer` 诊断静默

---

## 4. 版本演进总览

```
v1.0  原型验证
  └── Clang AST 遍历 + LLVM IR 手工构建
      └── v1.1  实用化
          └── 统计-报告架构，6种AST节点，双项目构建
              └── v1.2  突破局限
                  └── 方案A：手动 HeaderSearchOpts，支持 #include
                      └── v1.3  成熟化 ★
                          └── 方案B：CreateFromArgs 自动发现，可移植
```

---

*本文档版本 1.3，归档于 2026-06-07。*
