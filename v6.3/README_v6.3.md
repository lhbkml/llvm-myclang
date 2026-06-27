# myclang-c++ v6.3 — 项目介绍文档

> **版本**: 6.3
> **日期**: 2026-06-21
> **适用**: `v6.3/` 目录下的所有文件
> **前序**: [v6.2](../v6.2/README_v6.2.md)
> **主题**: Docker 容器化部署 — 解决 LLVM 动态库依赖

---

## 1. 版本概述

v6.2 完成了结构化错误返回，但工具只能在本地运行——依赖自编译 LLVM 23 的动态库。换成其他机器或服务器，缺 `.so` 直接报错。

v6.3 通过 **Docker 容器化** 解决部署问题。一条 `docker run` 即可在任何支持 Docker 的机器上运行，无需安装 LLVM。

### 1.1 核心变更

| 变更 | v6.2 | v6.3 |
|------|------|------|
| 部署方式 | 本地编译运行 | **Docker 容器** |
| LLVM 版本 | 自编译 23-dev | **Ubuntu 24.04 自带 LLVM 18** |
| Makefile | 硬编码路径 | **可移植：自编译/系统安装 双模式** |
| 新增文件 | — | **Dockerfile**（33 行） |
| frontendAction.cpp | 1497 行 | **1480 行**（适配 LLVM 18 API） |
| IRMake.cpp | 107 行 | **108 行**（补 Triple 头文件） |

---

## 2. 难题与解决方案

### 2.1 网络问题链：Docker Desktop 翻墙

Docker Desktop 跑在独立虚拟机里，不走宿主 VPN。在 WSL2 + Clash Verge 环境下拉镜像遇到一系列问题：

| 步骤 | 问题 | 解决 |
|------|------|------|
| 1 | Docker Hub 连不上 | Docker Desktop Settings → Proxies 填代理地址 |
| 2 | `host.docker.internal:7897` 不通 | Clash 监听 `127.0.0.1`，外部不可达 |
| 3 | 开 Allow LAN 后还不行 | Windows 防火墙拦截 → `netsh advfirewall` 加规则 |
| 4 | DNS 解析到错误 IP | `host.docker.internal` 不准确 → 换 WSL2 网关 `172.20.112.1:7897` |
| 5 | 拉镜像偶发超时 | Clash 节点不稳 → 先 `docker pull` 缓存基础镜像 |

### 2.2 ABI 不兼容：apt.llvm.org 预编译包

最初尝试从 `apt.llvm.org` 装 LLVM 19/22，编译通过但链接阶段数百个 `undefined reference`：

```
undefined reference to `llvm::json::Object::operator[](llvm::json::ObjectKey&&)'
```

**根因**：apt.llvm.org 的 LLVM 是**另一个 GCC 版本**编译的，跟 Ubuntu 24.04 的 GCC 14 **ABI 不兼容**。

```
nm 确认符号在库中 (T _ZN4llvm4json6ObjectixEONS0_9ObjectKeyE)
但链接器打死不认 → 符号修饰名（mangling）对不上
```

**解决**：放弃 apt.llvm.org，改用 **Ubuntu 24.04 原生 LLVM 18**。同一个 GCC 编译，零 ABI 问题，走阿里云镜像无需翻墙下载。

### 2.3 共享库符号导出不完整

Ubuntu 的 LLVM 18 共享库 `libLLVM-18.so`、`libclang-cpp.so.18.0` 使用了 `-fvisibility=hidden` 编译，大量内部符号不导出：

```
libLLVM.so.18.0 缺少:
  - llvm::json::Object::operator[]()    (JSON 库)
  - llvm::outs()                        (基础输出)
  - llvm::raw_ostream 的 vtable         (流基类)

libclang-cpp.so 缺少:
  - clang::RISCV::RVVIntrinsic::*       (RISCV 内建函数)
  - clang::PPConditionalDirectiveRecord （预处理条件指令）
  - clang::driver::getDriverOptTable()  (Driver 选项表)
```

**解决**：放弃共享库，改用 **全静态链接**。直接枚举安装目录下所有 `.a` 文件：

```makefile
LLVM_LIBDIR := $(shell $(LLVM_CONFIG) --ldflags | ... | sed 's/^-L//')
LLVMLIBS   := $(shell ls $(LLVM_LIBDIR)/libLLVM*.a  | grep -viE 'polly|lto' | tr '\n' ' ')
CLANGLIBS  := $(shell ls $(LLVM_LIBDIR)/libclang*.a | grep -vi clangd | tr '\n' ' ')
```

### 2.4 静态链接的循环依赖

一百多个 `.a` 文件之间存在双向依赖：

```
libclangFrontend.a → libclangDriver.a → libclangFrontend.a (循环!)
libclangSema.a     → libclangAST.a     → libclangSema.a
libLLVMSupport.a   → libLLVMDemangle.a → libLLVMCore.a → libLLVMSupport.a
```

**解决**：`-Wl,--start-group ... -Wl,--end-group` 包裹所有库，让链接器多次扫描直到符号全部解析。

### 2.5 缺失的系统依赖

LLVM/Clang 静态库依赖 zstd、zlib、tinfo，但 `-lzstd`、`-lz` 找不到 dev symlink（`libzstd-dev` 未安装）：

```
/usr/bin/ld: cannot find -lzstd: No such file or directory
/usr/bin/ld: cannot find -lz: No such file or directory
```

**解决**：用精确文件名绕过 symlink：

```makefile
SYSTEMLIBS = $(shell $(LLVM_CONFIG) --system-libs) -l:libzstd.so.1 -l:libz.so.1 -ltinfo
```

### 2.6 LLVM 18 API 差异

自编译 LLVM 23 → Ubuntu LLVM 18 之间的 API 变化：

| API | LLVM 23 | LLVM 18 | 修改 |
|-----|---------|---------|------|
| `setVirtualFileSystem()` | 无（参数化） | 移除 | 改为 `createFileManager(VFS)` |
| `createSourceManager()` | 无参 | 需 `FileManager&` | 传 `CI.getFileManager()` |
| `CreateTargetInfo()` | `TargetOptions&` | `shared_ptr<TargetOptions>` | `std::make_shared` 包装 |
| `InclusionDirective` | `OptionalFileEntryRef` | `const FileEntry*`（不同 `Optional`） | 删除 IncludeCounter 类 |
| `Module` 类型 | `llvm::Module` | 同时可见 `llvm::Module` 和 `clang::Module` | 显式 `const clang::Module*` |
| `Triple` 头文件 | 隐式包含 | 需显式 include | `#include <llvm/TargetParser/Triple.h>` |

### 2.7 Polly / LTO 符号缺失

`llvm-config --link-static --libs all` 输出的库列表包含 `libLLVMPolly.a` 和 `libLLVMLTO.a`，但 Ubuntu 没装 Polly，导致：

```
undefined reference to `getPollyPluginInfo()'
```

**解决**：`grep -viE 'polly|lto'` 过滤掉这些库。

### 2.8 clangd / protobuf 过度包含

使用 `--whole-archive` 把所有 clang 静态库全拉进来时，`libclangDaemon.a`、`libclangd*` 等引用了 Google protobuf，但未安装：

```
undefined reference to `google::protobuf::internal::fixed_address_empty_string'
```

**解决**：弃用 `--whole-archive`，改用 `--start-group`（按需解析），同时 `grep -vi clangd` 过滤 clangd 相关库。

---

## 3. 最终架构

```
┌─────────────────────────────────────────────────┐
│                   Docker 镜像                    │
│                                                 │
│  ┌──────────────┐    ┌──────────────────────┐   │
│  │ builder 阶段  │    │     runtime 阶段      │   │
│  │              │    │                      │   │
│  │ llvm-18-dev  │    │   libclang-cpp18     │   │
│  │ libclang-18  │    │   libllvm18          │   │
│  │ g++ make     │    │                      │   │
│  │              │    │   /app/frontendAction │   │
│  │ 静态编译 →   │───▶│   /app/output/       │   │
│  │ 全.a 链接    │    │                      │   │
│  └──────────────┘    └──────────────────────┘   │
│                                                 │
│  最终镜像: 583MB                                 │
│  无需任何 LLVM 运行时依赖                         │
└─────────────────────────────────────────────────┘
```

**链接策略**：
```
-Wl,--start-group
  /usr/lib/llvm-18/lib/libclang*.a     (除 clangd)
  /usr/lib/llvm-18/lib/libLLVM*.a      (除 Polly/LTO)
-Wl,--end-group
-l:libzstd.so.1  -l:libz.so.1  -ltinfo   (系统库补洞)
```

---

## 4. 使用方式

```bash
# 构建
docker build -t myclang-cc .

# 分析容器内测试文件
docker run --rm myclang-cc /app/output/test_params_nesting.c --json

# 挂载本地目录
docker run --rm -v $(pwd):/data myclang-cc /data/test.c --json

# 标准输入
cat test.c | docker run --rm -i myclang-cc --stdin --json
```

---

## 5. Makefile 双模式

| 场景 | 用法 | LLVM 来源 |
|------|------|----------|
| **Docker / 系统安装** | `make` | `llvm-config-18` 自动探测，全静态 `.a` |
| **自编译** | `make LLVM_CONFIG=... LLVM_SRC=...` | 自编译 LLVM，`--libs all` |

---

## 6. 12 项代码规范检查（不变）

```
函数行数上限(50)  │  goto 语句  │  参数个数(5)  │  嵌套深度(4)
命名规范          │  全局变量   │  单行长度(100) │  冗余 return
switch 穿透       │  死代码    │  空代码块       │  圈复杂度(10)
```

---

## 7. 版本演进

```
v5.x   AST+CFG 双引擎，12 项检查
v6.0   JSON 输出
v6.1   内存源码入口（InMemoryFileSystem）
v6.2   结构化错误返回（AnalysisResult + DiagCollector）
v6.3   ✅ Docker 容器化部署
```

---

## 8. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 1480 | 核心分析工具（LLVM 18 API 适配版） |
| `IRMake.cpp` | 108 | LLVM IR 手工构建（加 Triple 头文件） |
| `Makefile` | 135 | 可移植双模式构建 |
| `Dockerfile` | 33 | 多阶段 Docker 构建 |

---

*本文档版本 6.3，归档于 2026-06-21。*
