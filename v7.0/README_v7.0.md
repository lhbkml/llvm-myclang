# myclang-c++ v7.0 — 项目介绍文档

> **版本**: 7.0
> **日期**: 2026-06-28
> **前序**: [v6.5](../v6.5/README_v6.5.md)
> **主题**: 项目级目录扫描 — `--project` 参数

---

## 1. 版本概述

v6.5 支持单文件分析 + Web 前端，v7.0 新增 **项目级目录扫描**：

```
v6.5（单文件）
  frontendAction a.c b.c c.c          ← 手动列举每个文件

v7.0（项目级）
  frontendAction --project ./src      ← 自动递归发现所有 .c 文件
```

---

## 2. 新增功能：`--project <目录>`

### 2.1 核心逻辑

```
--project ./myproject
    │
    └─► collectCFiles("./myproject")    ← 递归扫描
            │
            ├── src/main.c              ← 自动发现
            ├── src/utils.c
            ├── src/io/file_io.c
            └── tests/test_utils.c
                    │
                    ▼
            逐个分析，聚合结果
            ├── [FAIL] 标红失败文件
            ├── 文本报告聚合所有成功文件
            └── JSON 输出增加 "project" 字段
```

### 2.2 新增 CLI 参数

```
--project=<string>   递归扫描目录下所有 .c 文件并分析
```

与 `--stdin` 互斥，可与 `--json`、阈值参数组合使用。

### 2.3 修改文件

仅 `frontendAction.cpp` 一个文件：

| 改动 | 说明 |
|------|------|
| `#include "llvm/Support/FileSystem.h"` | 目录遍历 API |
| `#include "llvm/Support/Path.h"` | 路径操作 |
| `cl::opt<std::string> ProjectDir("project", ...)` | 新增 CLI 参数 |
| `static void collectCFiles(...)` | 递归扫描 .c 文件 |
| main() 项目模式分支 | 扫描 → 逐个分析 → 聚合 |

### 2.4 递归扫描实现

```cpp
static void collectCFiles(const std::string &Dir, std::vector<std::string> &Files) {
    std::error_code EC;
    for (llvm::sys::fs::directory_iterator It(Dir, EC), End;
         It != End && !EC; It.increment(EC)) {
        auto rawPath = It->path();
        std::string Path(rawPath.data(), rawPath.size());
        llvm::sys::fs::file_status St;
        if (llvm::sys::fs::status(Path, St)) continue;
        if (St.type() == llvm::sys::fs::file_type::regular_file) {
            if (StringRef(Path).endswith(".c"))
                Files.push_back(Path);
        } else if (St.type() == llvm::sys::fs::file_type::directory_file) {
            StringRef DirName = llvm::sys::path::filename(Path);
            if (DirName != "." && DirName != "..")
                collectCFiles(Path, Files);
        }
    }
}
```

---

## 3. 使用示例

```bash
# Docker：分析内置测试目录
docker run --rm myclang-cc --project /app/output

# Docker + JSON + 自定义阈值
docker run --rm myclang-cc --project /app/output --json --max-ccn 5 2>/dev/null

# Docker：挂载自己的 C 项目
docker run --rm -v $(pwd):/project myclang-cc --project /project

# 命令行构建
make frontendAction
./frontendAction --project ./my-c-project
```

---

## 4. 遇到的问题

### 问题：LLVM `directory_entry::path()` 返回类型不一致

**现象**：Ubuntu 24.04 LLVM 18 中 `path()` 返回 `std::string`，但 apt.llvm.org 版本返回 `StringRef`。直接调用 `.str()` 在 Ubuntu 版本上编译失败。

**错误信息**：
```
error: 'const std::string' has no member named 'str'
```

**解决**：使用 `rawPath.data()` 和 `rawPath.size()` 统一构造 `std::string`，两者都支持：

```cpp
auto rawPath = It->path();
std::string Path(rawPath.data(), rawPath.size());
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
| `frontendAction.cpp` | 253 | **CLI 入口 + --project + 目录扫描** |
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
v7.0   ✅ 项目级目录扫描 — --project 参数
```

---

*本文档版本 7.0，归档于 2026-06-28。*
