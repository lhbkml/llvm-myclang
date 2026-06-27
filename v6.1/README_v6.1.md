# myclang-c++ v6.1 — 项目介绍文档

> **版本**: 6.1
> **日期**: 2026-06-20
> **适用**: `v6.1/` 目录下的所有文件
> **前序**: [v6.0](../v6.0/README_v6.0.md)
> **主题**: 入口函数化 — 从 CLI-only 到可编程调用

---

## 1. 版本概述

v6.0 解决了输出格式问题（JSON），但入口仍然是 `main(argc, argv)` ——只能从命令行文件路径调用。v6.1 将分析内核抽取为独立函数 `analyzeSourceCode(code, name)`，接受内存中的源码字符串。Web 后端现在可以直接调用它，无需写临时文件。

同时新增 `--stdin` 模式，从标准输入读取源码，方便流水线集成。

### 1.1 核心变更

| 变更 | v6.0 | v6.1 |
|------|------|------|
| 入口方式 | 仅 CLI 文件路径 | **CLI + stdin + 函数调用** |
| `--stdin` 标志 | — | ✅ |
| 新增 include | — | `<sstream>` |
| 新增函数 | — | **classifyLines / runClangPipeline / analyzeSourceCode** |
| analyzeFile | 单体 180 行 | **重构为共享核心调用** |
| frontendAction.cpp 行数 | 1352 | **1425** |

---

## 2. 架构重构

### 2.1 重构前（v6.0）

```
analyzeFile() {
    装配 CI → VFS → FileManager → SourceManager
    → HeaderSearch → Preprocessor → Sema → ParseAST
    → AST 遍历
    → ifstream 文本行分析
}
```

### 2.2 重构后（v6.1）

```
analyzeFile(path)          analyzeSourceCode(code, name)
      │                            │
  真实 VFS                   InMemoryFileSystem
      │                            │
      └──────────┬─────────────────┘
                 ▼
         runClangPipeline()     ← 共享 Clang 管线
         (CI 装配 → Preprocessor → Sema → ParseAST → AST 遍历)
                 │
                 ▼
         classifyLines()        ← 共享文本行分析
         (行数分类 + 长行检测)
                 │
                 ▼
            AnalysisStats
```

### 2.3 三个新函数

| 函数 | 职责 |
|------|------|
| `classifyLines(text, name, stats)` | 文本行分类（代码/空行/注释/预处理），disk/memory 共用 |
| `runClangPipeline(CI, invoc, ...)` | CompilerInstance 装配 + AST 分析，VFS 由调用者提供 |
| `analyzeSourceCode(code, name, invoc)` | 程序化入口：InMemoryFileSystem → pipeline → 行分析 → Stats |

---

## 3. InMemoryFileSystem — 内存输入原理

Clang 通过 VFS（Virtual File System）抽象所有文件 I/O。`analyzeSourceCode()` 利用 LLVM 内置的 `vfs::InMemoryFileSystem` 在内存中创建一个虚拟文件：

```cpp
auto MemFS = llvm::makeIntrusiveRefCnt<vfs::InMemoryFileSystem>();
MemFS->addFile(FileName, 0,
               llvm::MemoryBuffer::getMemBuffer(Code, FileName));

CompilerInstance CI;
CI.setVirtualFileSystem(MemFS);   // ← 后续全流程无感知
```

FileManager、SourceManager、Preprocessor 通过 VFS 接口读写，不关心底层是磁盘还是内存。

---

## 4. 使用方式

### 4.1 文件模式（不变）

```bash
./frontendAction test.c
./frontendAction test.c --json
./frontendAction a.c b.c       # 多文件
```

### 4.2 stdin 模式（新增）

```bash
cat test.c | ./frontendAction --stdin
cat test.c | ./frontendAction --stdin --json | python3 -m json.tool
```

### 4.3 程序化调用

```cpp
// Web 后端直接调用
#include "frontendAction.cpp"  // 或编译为静态库

AnalysisStats stats = analyzeSourceCode(sourceCode, "uploaded.c", invocation);
json::Value result = toJSON(stats);
// → 返回给前端
```

---

## 5. 代码规范检查全景（12 项，不变）

```
  函数行数上限(50行)    ← v3.0
  goto 语句             ← v3.0
  函数参数上限(5个)     ← v5.4
  嵌套深度上限(4)       ← v5.4
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

同 v5.4，不变。

### 6.2 逐函数明细（30 项）

同 v5.4，不变。

---

## 7. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 1425 | AST+CFG 双引擎 + 12 项规范检查 + 文本/JSON 双输出 + 内存/磁盘双入口 |
| `IRMake.cpp` | 107 | LLVM IR 手工构建（不变，自 v1.0） |
| `Makefile` | 116 | 双项目构建 + test 目标（不变，自 v1.1） |

---

## 8. 6.x 路线图进度

```
v6.0  ✅ JSON 输出    — toJSON 序列化 + --json 标志
v6.1  ✅ 入口函数化   — analyzeSourceCode + --stdin + InMemoryFileSystem
v6.x  ⬜ 错误标准化   — 结构化错误返回（非 exit+stderr）
v6.x  ⬜ Web 层集成   — HTTP API + 上传页面
```

---

## 9. 版本演进总览

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
v6.0  JSON 输出    — --json 模式 + toJSON 序列化
v6.1  入口函数化   — analyzeSourceCode + InMemoryFileSystem
```

---

*本文档版本 6.1，归档于 2026-06-20。*
