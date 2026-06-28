# myclang-c++ v7.1 — 项目介绍文档

> **版本**: 7.1
> **日期**: 2026-06-28
> **前序**: [v7.0](../v7.0/README_v7.0.md)
> **主题**: compile_commands.json 集成 — 逐文件编译参数

---

## 1. 版本概述

v7.0 实现了 `--project` 目录递归扫描，但所有文件共用一个编译参数（FakeArgs）。真实 C 项目的每个文件可能有不同的 `-I` 路径、`-D` 宏定义。

v7.1 新增 `--cdb <compile_commands.json>` 参数，读取 CMake/Bear 生成的编译数据库，为每个文件使用**正确的编译参数**。

```
v7.0（共享参数）
  --project ./src
    ├── main.c    ← 用相同的 FakeArgs
    ├── utils.c   ← 用相同的 FakeArgs
    └── io/file.c ← 用相同的 FakeArgs

v7.1（逐文件参数）
  --project ./src --cdb compile_commands.json
    ├── main.c    ← -Iinclude -DDEBUG -std=c11
    ├── utils.c   ← -Iinclude -Ithirdparty -std=c99
    └── io/file.c ← -Iinclude -D_FILE_OFFSET_BITS=64
```

---

## 2. 新增功能：`--cdb <路径>`

### 2.1 CLI 参数

```
--cdb=<string>    compile_commands.json 文件路径
```

与 `--project` 配合使用，不能用 `--stdin`。

### 2.2 工作流程

```
compile_commands.json
    │
    ├─► parseCompileCommands()        JSON 解析
    │     ├── 支持 "command"（字符串格式）
    │     └── 支持 "arguments"（数组格式）
    │
    ├─► tokenizeCommand()             命令分词（处理引号转义）
    │
    ├─► 参数过滤与转换
    │     ├── 跳过：编译器名、-c、-o
    │     ├── 替换：-fsyntax-only
    │     ├── 解析：相对 -I 路径 → 绝对路径
    │     └── 保留：-D、-std=、-Wall 等
    │
    ├─► 按文件名建立查找表
    │     main.c → [-Iinclude, -DDEBUG, ...]
    │     utils.c → [-Iinclude, -std=c99, ...]
    │
    └─► 分析循环
          对每个 .c 文件，查表 → 有则用自己的参数，无则 fallback 共享参数
```

### 2.3 compile_commands.json 格式

```json
[
  {
    "directory": "/project/build",
    "command": "gcc -I../include -DDEBUG -std=c11 -c src/main.c",
    "file": "src/main.c"
  },
  {
    "directory": "/project/build",
    "arguments": ["gcc", "-I../include", "-c", "src/utils.c"],
    "file": "src/utils.c"
  }
]
```

两种格式都支持。生成方式：

```bash
# CMake
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
ln -s build/compile_commands.json .

# Make + Bear
bear -- make
```

### 2.4 修改文件

仅 `frontendAction.cpp`（253→441 行）：

| 新增 | 说明 |
|------|------|
| `#include <map>` | 文件→编译参数查找表 |
| `#include "llvm/Support/MemoryBuffer.h"` | JSON 文件读取 |
| `cl::opt<std::string> CDBPath(...)` | CLI 参数 |
| `static tokenizeCommand()` | Shell 命令分词（处理引号、转义） |
| `struct CompileCommand` | 编译条目结构体 |
| `static parseCompileCommands()` | JSON 解析 + 参数转换 |
| main() 项目模式改造 | 逐文件查表 → per-file Invocation |

---

## 3. 遇到的问题

### 问题 1：LLVM 18 `StringRef::endswith` → `ends_with`

**错误**：`'llvm::StringRef' has no member named 'endswith'`

**原因**：Ubuntu 24.04 LLVM 18 中方法名改为 `ends_with`。

**解决**：全局替换 `endswith` → `ends_with`。

### 问题 2：LLVM 18 JSON API 无 `getArray()` 方法

**错误**：`no member named 'getArray'`

**原因**：Ubuntu LLVM 18 的 JSON Object 类中 `getArray()` 不可用。

**解决**：
```cpp
// 旧：auto *ArgsArr = Obj->getArray("arguments");
// 新：
auto *ArgsVal = Obj->get("arguments");
auto *ArgsArr = ArgsVal->getAsArray();
```

### 问题 3：`llvm::sys::path::filename()` 返回 `StringRef`

**错误**：无法隐式转换为 `std::string`

**解决**：显式调用 `.str()`。

---

## 4. 使用示例

```bash
# 基本用法
docker run --rm -v $(pwd):/project myclang-cc \
    --project /project --cdb /project/compile_commands.json

# 带自定义阈值
docker run --rm -v $(pwd):/project myclang-cc \
    --project /project --cdb /project/compile_commands.json --max-ccn 5

# JSON 输出
docker run --rm -v $(pwd):/project myclang-cc \
    --project /project --cdb /project/compile_commands.json --json 2>/dev/null
```

---

## 5. 文件清单

| 文件 | 行数 | 职责 |
|------|------|------|
| `core_types.h` | 214 | 数据结构 + Thresholds |
| `visitor.h` | 75 | AST 访问器声明 |
| `visitor.cpp` | 449 | AST 遍历 + CFG 分析 |
| `report.h` | 33 | JSON/文本报告声明 |
| `report.cpp` | 565 | JSON + 文本报告 + 行分类 |
| `pipeline.h` | 30 | Clang 管线声明 |
| `pipeline.cpp` | 118 | Clang 编译管线 |
| `frontendAction.cpp` | **441** | CLI 入口 + project + cdb + 命令分词 |
| `IRMake.cpp` | 108 | LLVM IR 手工构建 |
| `Makefile` | 135 | 双模式构建 |
| `Dockerfile` | 33 | 多阶段 Docker |
| `web/` (5 文件) | 518 | Web 前端 |
| `README.md` | 166 | 项目主文档 |

---

## 6. 版本演进

```
v5.x   AST+CFG 双引擎，12 项检查
v6.0   JSON 输出
v6.1   内存源码入口
v6.2   结构化错误返回
v6.3   Docker 容器化
v6.4   多文件模块化拆分
v6.5   运行时阈值配置 + Web 前端
v7.0   项目级目录扫描 — --project
v7.1   ✅ compile_commands.json 集成 — --cdb
```

---

*本文档版本 7.1，归档于 2026-06-28。*
