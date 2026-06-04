# myclang-c++ v1.0 — 项目介绍文档

> **版本**: 1.0  
> **日期**: 2026-05-29  
> **适用范围**: `Makefile_v1.0`, `frontendAction_v1.0.cpp`, `IRMake_v1.0.cpp` 及相关支持文件

---

## 1. 项目概述

这是一个基于 **LLVM/Clang 23.0.0git** 的 C++ 学习/实验项目，包含两个独立程序：

| 程序 | 定位 | 输入 | 输出 |
|------|------|------|------|
| **frontendAction** | Clang 前端工具，AST 遍历分析 | C 源文件 | 终端打印 AST 节点信息 |
| **IRMake** | LLVM IR 手工构建工具 | 无（硬编码） | `.bc` bitcode 文件 |

项目的底层 LLVM 源码位于 `/home/li/llvm-project/`，构建目录为 `/home/li/llvm-project/build/`。

---

## 2. 项目文件清单

### 2.1 核心源码（v1.0）

| 文件 | 大小 | 说明 |
|------|------|------|
| [`Makefile_v1.0`](#3-makefile_v10) | 4223B | 构建系统，基于 llvm-config |
| [`frontendAction_v1.0.cpp`](#4-frontendaction_v10cpp) | 3226B | Clang AST 遍历工具 |
| [`IRMake_v1.0.cpp`](#5-irmake_v10cpp) | 5087B | LLVM IR 手工构建工具 |

### 2.2 测试/输入文件

| 文件 | 说明 |
|------|------|
| `sum.c` (79B) | 最简输入源，含 `sum()` 和 `add()` 两函数 |
| `test.c` (84B) | 带 `write()` 系统调用的 `main()`，测试完整程序编译 |
| `analyze_test.c` (1013B) | **AST 分析测试用例**，含 7 种函数和多种控制流结构 |

### 2.3 构建产物

| 文件 | 说明 |
|------|------|
| `frontendAction` (1.2GB) | frontendAction 可执行文件（Debug 静态链接） |
| `frontendAction.o` (9.2MB) | frontendAction 目标文件 |
| `IRMake` (248MB) | IRMake 可执行文件 |
| `IRMake.o` (123KB) | IRMake 目标文件 |
| `sum.ll` / `sum.bc` | Clang -O0 编译 `sum.c` 的 IR |
| `sum-O1.ll` / `sum-O1.bc` | Clang -O1 编译 `sum.c` 的 IR |
| `sum-O2.ll` | Clang -O2 编译 `sum.c` 的 IR |
| `sum-fn.ll` / `sum-fn.bc` / `sum-fn.o` | IRMake 生成或不同配置下的产物 |
| `sum-tmp.bc` | 临时 bitcode 输出 |

---

## 3. Makefile_v1.0

### 3.1 设计思路

通过 `llvm-config` 工具自动获取 LLVM/Clang 的编译和链接参数，避免手写复杂的 flags。

### 3.2 核心变量

```makefile
LLVM_CONFIG = /home/li/llvm-project/build/bin/llvm-config

CXXFLAGS += $(shell $(LLVM_CONFIG) --cxxflags)   # 编译选项
CXXFLAGS += $(shell $(LLVM_CONFIG) --cppflags)    # 预处理器选项
CXXFLAGS += -fno-rtti                              # 禁用 RTTI（LLVM 要求）

LDFLAGS  += $(shell $(LLVM_CONFIG) --ldflags)     # 链接选项

LLVMLIBS   = $(shell $(LLVM_CONFIG) --libs all)   # 所有 LLVM 库
SYSTEMLIBS = $(shell $(LLVM_CONFIG) --system-libs) # 系统库
```

### 3.3 头文件搜索路径

```makefile
CPPFLAGS += -I/home/li/llvm-project/clang/include
CPPFLAGS += -I/home/li/llvm-project/build/tools/clang/include
CPPFLAGS += -I/home/li/llvm-project/build/include
CPPFLAGS += -I$(SRC_DIR)
```

### 3.4 链接库顺序（关键设计）

Clang 库之间有复杂的依赖关系。v1.0 版本采用了**手动调序**策略：

- **所有约 50 个 Clang 库按从高层到低层的顺序排列**
- `-lclangBasic` 放在列表**最末尾**（第83行），因为它被所有其他 Clang 库依赖
- 注释掉的代码（107-169行）展示了另一种未采用的方案：用 `-Wl,--start-group` / `-Wl,--end-group` 让链接器多次扫描解决循环依赖

### 3.5 构建规则

```makefile
PROJECT = frontendAction              # 只构建 frontendAction
PROJECT_OBJECTS = frontendAction.o

default: $(PROJECT)

%.o: $(SRC_DIR)/%.cpp                 # 模式规则：编译 .cpp → .o
$(PROJECT): $(PROJECT_OBJECTS)        # 链接成可执行文件
clean:                                # 清理目标文件和可执行文件
```

---

## 4. frontendAction_v1.0.cpp

### 4.1 功能说明

一个 Clang 前端工具，解析用户指定的 C 源文件，用 **RecursiveASTVisitor** 遍历其 AST，并将遇到的语法节点信息打印到终端。

### 4.2 头文件依赖

| 头文件 | 用途 |
|--------|------|
| `llvm/Support/CommandLine.h` | 命令行参数解析 |
| `llvm/TargetParser/Host.h` | 获取默认目标三元组 |
| `clang/Frontend/CompilerInstance.h` | 编译器实例（核心调度类） |
| `clang/Frontend/ASTConsumers.h` | AST 消费者基类 |
| `clang/Parse/ParseAST.h` | 解析 AST |
| `llvm/Support/VirtualFileSystem.h` | 虚拟文件系统 |
| `clang/AST/ASTConsumer.h` | 自定义 AST Consumer 需要 |
| `clang/AST/RecursiveASTVisitor.h` | 递归 AST 访问者 |

### 4.3 程序结构

```
main()
├── cl::ParseCommandLineOptions     → 解析命令行，获取输入文件名
├── CompilerInstance 初始化
│   ├── createDiagnostics()         → 诊断引擎
│   ├── TargetInfo 设置             → x86_64-unknown-linux-gnu
│   ├── setVirtualFileSystem()      → 真实文件系统
│   ├── createFileManager()         → 文件管理器
│   ├── createSourceManager()       → 源码管理器
│   │   └── 通过 getFileRef 打开输入文件
│   ├── createPreprocessor()        → 预处理器
│   ├── setASTConsumer()            → 注入自定义 Consumer
│   ├── createASTContext()          → AST 上下文
│   └── createSema()                → 语义分析
├── ParseAST(CI.getSema())          → 启动解析
└── PrintStats()                    → 输出统计信息
```

### 4.4 自定义 AST 访问者（MyASTVisitor）

继承 `RecursiveASTVisitor<MyASTVisitor>`，识别以下 5 种 AST 节点：

| Visit 方法 | AST 节点 | 终端输出 |
|-----------|----------|----------|
| `VisitFunctionDecl` | 函数定义 | `【函数】: <函数名>` |
| `VisitVarDecl` | 变量声明 | `【变量】: <变量名>  【类型】: <类型>` |
| `VisitIfStmt` | if 语句 | `【if 语句】` |
| `VisitForStmt` | for 循环 | `【for 循环】` |
| `VisitReturnStmt` | return 语句 | `【return 语句】` |

### 4.5 设计方案

- **ASTConsumer 模式**：`MyASTConsumer::HandleTranslationUnit()` 在翻译单元解析完成后被回调，启动遍历
- **手写 CompilerInstance 初始化**：不使用 `createInvocationFromCommandLine()` 等高层 API，而是手动创建每个组件，便于理解 Clang 内部结构

---

## 5. IRMake_v1.0.cpp

### 5.1 功能说明

完全不依赖 Clang，用 **LLVM C++ API** 手工构建 LLVM IR 模块，并输出为 `.bc` bitcode 文件。

### 5.2 头文件依赖

| 头文件 | 用途 |
|--------|------|
| `llvm/ADT/SmallVector.h` | 高效小向量容器 |
| `llvm/IR/Verifier.h` | 验证 IR 模块正确性 |
| `llvm/IR/BasicBlock.h` | 基本块类 |
| `llvm/IR/CallingConv.h` | 调用约定定义 |
| `llvm/IR/Function.h` | 函数类 |
| `llvm/IR/Instructions.h` | 所有 IR 指令类 |
| `llvm/IR/LLVMContext.h` | LLVM 全局上下文（线程安全） |
| `llvm/IR/Module.h` | IR 模块（顶层容器） |
| `llvm/Bitcode/BitcodeWriter.h` | Bitcode 写出 |
| `llvm/Support/ToolOutputFile.h` | 输出文件工具类 |
| `llvm/Support/FileSystem.h` | 文件系统交互 |

### 5.3 生成的 IR 结构

```
Module: "sum.ll"
├── DataLayout:   "e-m:e-i64:64-f80:128-n8:16:32:64-S128"  (小端 x86-64)
├── TargetTriple: "x86_64-unknown-linux-gnu"
└── Function:     i32 @add(i32 %a, i32 %b)       (cdecl 调用约定)
    └── BasicBlock: entry
        ├── alloca i32 → %a.addr                  ; 栈分配参数 a
        ├── alloca i32 → %b.addr                  ; 栈分配参数 b
        ├── store i32 %a, ptr %a.addr             ; 存储参数 a
        ├── store i32 %b, ptr %b.addr             ; 存储参数 b
        ├── load i32, ptr %a.addr                 ; 加载 a
        ├── load i32, ptr %b.addr                 ; 加载 b
        ├── add i32 %loadA, %loadB → %result     ; 加法运算
        └── ret i32 %result                       ; 返回结果
```

### 5.4 代码组织

| 部分 | 说明 |
|------|------|
| 模块级 `LLVMContext Context` | 全局变量，整个程序共享 |
| `makeLLVMModules()` | 核心函数：手工构建完整的 IR Module |
| `main()` | 调用构建函数 → 验证模块 → 写出 `sum.bc` → 清理 |

### 5.5 对比：手工构建 vs Clang 编译

| 方面 | IRMake 手工构建 | Clang -O0 编译 | Clang -O1 编译 |
|------|----------------|---------------|---------------|
| 指令数（add函数） | 8 条 | 8 条 | 2 条 |
| 模式 | alloca→store→load→add→ret | alloca→store→load→add→ret | add→ret |
| alloca/store/load | ✅ 有 | ✅ 有 | ❌ 无（mem2reg 优化） |
| IR 结构 | 基本等价 | 基本等价 | 已被优化消除冗余 |

### 5.6 注释掉的备选代码（第15-51行）

文件中有一段被注释掉的 `main()` 函数，它用更简洁的方式构建了完全相同的 IR，区别是：
- 不封装 `makeLLVMModules()` 函数
- 将 IR 打印到标准输出而不是写文件
- 使用了 C++17 的初始化列表语法

这说明作者在开发过程中做过结构上的探索和重构。

---

## 6. 测试文件说明

### 6.1 `sum.c` — 最简测试输入

```c
int sum(int a,int b){ return a+b; }
int add(int a,int b){ return a+b; }
```

最简单的两个求和函数，用于验证 IRMake 和 Clang 编译的基本功能。

### 6.2 `test.c` — 完整程序测试

```c
int main(){
    char * msg="Hello, World!\n";
    write(1, msg, 14);
    return 0;
}
```

直接使用 Linux `write` 系统调用输出字符串，不依赖 C 标准库。

### 6.3 `analyze_test.c` — AST 分析综合测试

精心设计的测试文件，覆盖所有 MyASTVisitor 能识别的节点类型：

| 函数 | 覆盖的 AST 节点 |
|------|----------------|
| `max()` | `FunctionDecl`, `IfStmt`, `ReturnStmt` × 2 |
| `min()` | `FunctionDecl`, `IfStmt`, `ReturnStmt` × 2 |
| `print_loop()` | `FunctionDecl`, `VarDecl`, `ForStmt` |
| `sum_range()` | `FunctionDecl`, `VarDecl` × 2, While 循环, `ReturnStmt` |
| `check_numbers()` | `FunctionDecl`, 嵌套 `IfStmt` × 3 |
| `factorial()` | `FunctionDecl`, `IfStmt`, 递归 `ReturnStmt` |
| `main()` | `FunctionDecl`, 5× `VarDecl`, 5× 函数调用, `ReturnStmt` |

另外包含 3 个全局变量（`global_count`, `global_ratio`, `global_label`），覆盖不同类型。

---

## 7. LLVM IR 文件对比

| 文件 | 优化级别 | DataLayout | 函数 | 指令数/函数 |
|------|----------|------------|------|------------|
| `sum.ll` | -O0 | e-m:e-... | sum, add | 8 |
| `sum-fn.ll` | -O0 | **E**-m:m-...（大端） | sum | 8 |
| `sum-O1.ll` | -O1 | e-m:e-... | sum, add | **2** |
| `sum-O2.ll` | -O2 | e-m:e-... | sum, add | 2 |

> **注意 `sum-fn.ll` 的异常**：其 DataLayout 以 `E` 开头（大端序），与 x86-64 实际的小端序不符。推测是 IRMake 在某次实验时使用了不同的 DataLayout 配置。

---

## 8. 项目演进：v1.0 → 当前版本

> **v1.0 文件**: `Makefile_v1.0`, `frontendAction_v1.0.cpp`, `IRMake_v1.0.cpp`  
> **当前版本文件**: `Makefile`, `frontendAction.cpp`, `IRMake.cpp`

本章详细记录项目从 v1.0 版到当前版的每一步变化，是理解作者学习轨迹的核心章节。

---

### 8.1 Makefile 演进：从单项目到多项目构建

#### 8.1.1 变更概览

```
Makefile_v1.0 (4223B)  ──同步更新──▶  Makefile (2867B)
```

虽然文件体积变小（移除了大段注释掉的 `--start-group` 块），但功能显著增强。

#### 8.1.2 逐项对比

| 方面 | v1.0 | 当前版 | 变化说明 |
|------|------|--------|----------|
| 编译/链接 flags | 相同 | 相同 | `CXXFLAGS`, `CPPFLAGS`, `LDFLAGS` 完全不变 |
| Clang 库列表 | 相同 | 相同 | `CLANGLIBS` 约50个库，顺序完全不变 |
| 项目变量 | `PROJECT = frontendAction` | `PROJECTS = frontendAction IRMake` | **单数→复数**，新增 IRMake |
| 目标文件 | `PROJECT_OBJECTS = frontendAction.o` | `PROJECT_OBJECTS = frontendAction.o IRMake.o` | 新增 `IRMake.o` |
| 默认目标 | `default: $(PROJECT)` | `default: all` | 引入 `all` 伪目标 |
| 构建规则 | `all: $(PROJECTS)` | 新增，支持多目标并行 |
| 链接规则 | 1个通用规则 | 2个独立规则 | **关键差异见下方** |
| 测试目标 | 无 | `test:` | 新增自动化测试 |
| 注释掉的代码 | `--start-group` 方案(62行) | 已删除 | 清理旧方案 |

#### 8.1.3 链接规则差异（核心变化）

**v1.0 — 单一通用链接（仅 frontendAction）：**
```makefile
$(PROJECT): $(PROJECT_OBJECTS)
    $(CXX) -o $@ $^ $(LDFLAGS) $(CLANGLIBS) $(LLVMLIBS) $(SYSTEMLIBS)
```

**当前版 — 两个独立链接规则：**
```makefile
frontendAction: frontendAction.o
    $(CXX) -o $@ $^ $(LDFLAGS) $(CLANGLIBS) $(LLVMLIBS) $(SYSTEMLIBS)
    # ↑ 需要链接约50个 Clang 库

IRMake: IRMake.o
    $(CXX) -o $@ $^ $(LDFLAGS) $(LLVMLIBS) $(SYSTEMLIBS)
    # ↑ 不需要 Clang 库，仅链接 LLVM 库
```

**设计意图**：IRMake 是纯 LLVM IR API 调用，不依赖 Clang 的任何库，所以链接时不应引入 `CLANGLIBS`。这是正确的——分开链接规则既减少了 IRMake 的链接时间，也避免了不必要的依赖。

#### 8.1.4 新增 test 目标

```makefile
test: frontendAction
    @echo "=== 测试 test.c ==="
    ./frontendAction test.c
    @echo ""
    @echo "=== 测试 sum.c ==="
    ./frontendAction sum.c
```

自动化了回归测试流程，用两个最小测试文件验证 frontendAction 是否正常工作。

---

### 8.2 frontendAction.cpp 演进（变化最丰富）

#### 8.2.1 变更概览

```
frontendAction_v1.0.cpp (3226B, 111行)  ──大幅重写──▶  frontendAction.cpp (5672B, 183行)
                                                          体积 +76%, 行数 +65%
```

#### 8.2.2 头文件变化

| 头文件 | v1.0 | 当前版 | 说明 |
|--------|:----:|:------:|------|
| `clang/Basic/SourceManager.h` | ❌ | ✅ 新增 | 计算源码行号需要 |
| `<map>` | ❌ | ✅ 新增 | `callTargets` 统计需要 |
| `<string>` | ❌ | ✅ 新增 | `std::string` 使用 |
| `clang/Basic/VirtualFileSystem.h` | 注释 | 已删除 | 清理无效引用 |

#### 8.2.3 架构变化：从"即时打印"到"先收集-后报告"

**v1.0 模式（边遍历边打印）：**
```
VisitXxx() → 直接 llvm::outs() 输出
main()     → PrintStats() 输出原始统计
```

**当前版模式（先收集数据-后格式化报告）：**
```
VisitXxx() → 写入 AnalysisStats 结构体
main()     → printReport(Stats) 输出格式化报告
```

**新增的数据结构 `AnalysisStats`：**
```cpp
struct AnalysisStats {
    int totalFunctions = 0;
    int globalVars = 0;
    int ifCount = 0;
    int forCount = 0;
    int whileCount = 0;
    int callCount = 0;
    std::map<std::string, int> callTargets;              // 被调用函数→调用次数
    std::vector<std::pair<std::string, int>> funcLines;  // 函数名→代码行数
};
```

#### 8.2.4 AST 访问者逐方法对比

| AST 节点 | v1.0 行为 | 当前版行为 | 变更性质 |
|----------|-----------|------------|----------|
| **FunctionDecl** | 打印 `【函数】: <名>` | 不打印；计数 + 计算函数体行数（通过 SourceManager）+ 检查 `isThisDeclarationADefinition()` | **完全重写**：从展示性→统计性 |
| **VarDecl** | 打印 `【变量】: <名> 【类型】: <类型>`（包括局部变量） | 仅计数全局变量 `isTranslationUnit()` | **语义变更**：过滤噪声，聚焦全局 |
| **IfStmt** | 打印 `【if 语句】` | 计数器 `Stats.ifCount++` | 从展示→统计 |
| **ForStmt** | 打印 `【for 循环】` | 计数器 `Stats.forCount++` | 从展示→统计 |
| **ReturnStmt** | 打印 `【return 语句】` | **已删除** | 作者认为 return 统计价值低 |
| **WhileStmt** | ❌ 未覆盖 | ✅ 计数器 `Stats.whileCount++` | **新增识别** |
| **CallExpr** | ❌ 未覆盖 | ✅ 计数 + 被调用函数名收集 | **新增识别**（高价值） |

#### 8.2.5 MyASTConsumer 变化

| 方面 | v1.0 | 当前版 |
|------|------|--------|
| 构造函数 | 无参 | `explicit MyASTConsumer(AnalysisStats &S)` |
| 向 Visitor 传递数据 | 无状态 | 传递 `Stats` 引用 + `SourceManager*` |

#### 8.2.6 main() 输出变化

| 方面 | v1.0 | 当前版 |
|------|------|--------|
| 输出方式 | `CI.getASTContext().PrintStats()` | `printReport(Stats)` |
| 输出内容 | LLVM 内部统计（AST 节点数等） | 自定义格式化中文报告 |

**当前版 `printReport()` 输出示例格式：**
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

#### 8.2.7 代码清理

v1.0 `main()` 中存在大量注释掉的尝试性代码，当前版全部移除：

| 清理内容 | v1.0 位置 | 说明 |
|----------|-----------|------|
| `// CI.createVFS();` | 第86行 | 已删除 — v1.0 注释说明了不可用 |
| `// CI.setFileManager(new FileManager(...));` | 第88-91行 | 已删除 — v1.0 注释说明了另一种尝试 |
| `// #include "clang/Basic/VirtualFileSystem.h"` | 第7行 | 已删除 — 清理无效头文件引用 |

这些清理说明作者在 v1.0 阶段经过了试错调试（尝试过不通的 FileSystem 设置方式），到当前版已经稳定。

---

### 8.3 IRMake.cpp 演进：零变化

#### 8.3.1 变更概览

```
IRMake_v1.0.cpp (5087B, 107行)  ──完全不变──▶  IRMake.cpp (5087B, 107行)
```

**逐行对比结果：两个文件完全相同**（`diff` 无任何输出）。

#### 8.3.2 分析

IRMake 自 v1.0 之后没有任何改动，反映：

| 观察 | 推断 |
|------|------|
| 程序一次性写对 | 单个函数 `makeLLVMModules()` + 简单 `main()` 结构稳定 |
| 功能已满足需求 | 输出 `sum.bc` 并验证通过，无需扩展 |
| 作者精力集中在 frontendAction | frontendAction 变化最大（+76%），是主要学习对象 |
| 注释保留 | 15-51行的备选实现仍然注释保留，说明作者认可当前结构 |

---

### 8.4 演进总结表

| 维度 | Makefile | frontendAction | IRMake |
|------|----------|----------------|--------|
| 变化程度 | **中等** | **大幅** | **零** |
| 代码变化 | 增加 IRMake 构建 + test 目标 | +72行，+76% | 0行，0% |
| 架构变化 | 单项目→多项目 | 即时打印→统计-报告模式 | 无 |
| 清理工作 | 删除 `--start-group` 注释块 | 删除 VFS 相关注释代码 | 无 |
| 新增能力 | test 自动化 | while/call 识别，行数统计，格式化报告 | 无 |
| 删除能力 | — | return 语句统计，逐个打印 | — |
| 设计意图 | 支持第二个程序的构建 | 从教学级 Demo → 实用分析工具 | 已完成，无需迭代 |

---

### 8.5 演进路径推测

基于以上变化，作者的学习/开发路径大致为：

```
阶段1 (v1.0)
├── 学习 Clang CompilerInstance 手动初始化流程
├── 用 5 种 AST Visitor 验证可行性 → frontendAction_v1.0.cpp
├── 学习 LLVM IR API 手工构建 → IRMake_v1.0.cpp
├── 解决 Clang 库链接顺序问题 → Makefile_v1.0
└── 生成测试 IR 产物（sum.ll, sum-O1.ll, sum-O2.ll）

阶段2 (当前版)
├── frontendAction 定位升级：
│   ├── 从"能跑"到"有用"→ 结构化报告
│   ├── 增加 WhileStmt 和 CallExpr 覆盖
│   └── SourceManager 引入，计算函数体行数
├── IRMake 已稳定，加入 Makefile 统一构建
├── 增加 make test 实现快速回归
└── 清理 v1.0 阶段的试错代码
```

---

## 9. 技术要点总结

1. **构建系统**采用 `llvm-config` 驱动的 Makefile，可自动适配 LLVM 安装路径
2. **链接策略**是项目关键难点——约 50 个 Clang 库需要按依赖关系排序，`clangBasic` 必须垫底
3. **frontendAction** 采用 `CompilerInstance` 手动初始化的低级 API，而非 `ClangTool` 等高级封装，适合学习 Clang 内部机制
4. **IRMake** 手工生成的 IR 与 Clang -O0 输出等价，验证了对 LLVM IR 指令语义的理解
5. **优化对比**：`mem2reg` pass 能将 8 条 alloca/store/load 指令优化为直接寄存器操作（2 条），直观展示了 LLVM 优化的效果
6. **可执行文件巨大**（frontendAction 1.2GB）：Debug 模式下静态链接所有 Clang/LLVM 库的典型结果

---

## 9. 依赖环境

| 组件 | 路径/版本 |
|------|----------|
| LLVM/Clang 源码 | `/home/li/llvm-project/` |
| LLVM/Clang 构建 | `/home/li/llvm-project/build/` |
| Clang 版本 | 23.0.0git (4284bd02...) |
| 目标平台 | x86_64-unknown-linux-gnu |
| 编译器 | 支持 C++17 的 Clang/GCC |
| 操作系统 | WSL2 Linux (6.6.87.2-microsoft-standard-WSL2) |

---

*本文档版本 1.0，归档于 2026-05-29。*
