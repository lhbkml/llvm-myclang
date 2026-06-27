# myclang-c++ v6.4 — 项目介绍文档

> **版本**: 6.4
> **日期**: 2026-06-21
> **前序**: [v6.3](../v6.3/README_v6.3.md)
> **主题**: 多文件拆分 — 从单文件 1480 行到 10 文件模块化

---

## 1. 版本概述

v6.3 完成了 Docker 容器化部署，但 `frontendAction.cpp` 膨胀到了 1480 行——数据定义、AST 遍历、JSON 序列化、文本报告、Clang 管线全挤在一起。改一个检查规则要在 1480 行里翻代码。

v6.4 将 `frontendAction.cpp` 拆分为 **10 个文件**，形成三层无环依赖结构。每个文件职责单一，最大文件不超过 555 行。

### 1.1 文件结构

```
             core_types.h (纯 std::，零 LLVM/Clang 依赖)
            /              \               \
    visitor.h/.cpp     report.h/.cpp    pipeline.h/.cpp
   (AST 遍历+CFG)     (JSON+文本输出)   (Clang 管线)
            \              /               /
         frontendAction.cpp (main + CLI flags)
```

### 1.2 文件清单

| 文件 | 行数 | 职责 | 来源 |
|------|------|------|------|
| `core_types.h` | 203 | 常量、struct、辅助函数 | 新文件（header-only） |
| `visitor.h` | 74 | DiagCollector、MyASTVisitor 声明 | 新文件 |
| `visitor.cpp` | 449 | AST 遍历 + CFG 分析 | 原 lines 230-708 |
| `report.h` | 32 | toJSON、printReport 声明 | 新文件 |
| `report.cpp` | 555 | JSON 序列化 + 文本报告 + 行分类 | 原 lines 710-1278 |
| `pipeline.h` | 29 | runClangPipeline/analyze 声明 | 新文件 |
| `pipeline.cpp` | 115 | Clang 编译管线 | 原 lines 1280-1384 |
| `frontendAction.cpp` | **124** | CLI 选项 + main() | 原 main 段 |
| `IRMake.cpp` | 108 | LLVM IR 手工构建（不变） | — |
| `Makefile` | 135 | 多目标构建 | 改链接依赖 |
| `Dockerfile` | 33 | 多阶段 Docker 构建（不变） | — |

### 1.3 核心变更

| 变更 | v6.3 | v6.4 |
|------|------|------|
| `frontendAction.cpp` | 1480 行 | **124 行**（-92%） |
| 源文件数 | 2 | **8** |
| 头文件数 | 0 | **4** |
| `static` 函数 | 全部 | 改为普通函数（.h 声明 + .cpp 定义） |

---

## 2. 设计决策

### 2.1 `core_types.h` 零 Clang 依赖

只 include `<string>` `<vector>` `<map>` `<set>`，不碰 LLVM/Clang 头文件。DiagCollector 需要 `clang::DiagnosticConsumer`，归入 `visitor.h`，保持 `core_types.h` 作为最轻量的依赖根。

### 2.2 函数去 `static`

原来所有自由函数都是 `static`（单文件内链接）。拆分后：
- 声明在 `.h`（供跨文件调用）
- 定义在 `.cpp`（去掉了 `static`）
- `isValidSnakeCase`/`isValidGlobalVarName` 留在 `core_types.h` 里用 `inline`

### 2.3 Makefile 改动最小

只改了链接依赖：

```makefile
# v6.3
frontendAction: frontendAction.o

# v6.4
frontendAction: frontendAction.o visitor.o report.o pipeline.o
```

已有 `%.o: $(SRC_DIR)/%.cpp` 模式规则自动编译所有新 `.cpp`。Dockerfile 完全不用改——`COPY . .` 自动包含新文件。

---

## 3. 开发指南

| 要做什么 | 改哪个文件 |
|---------|-----------|
| 新增检查规则（如禁止递归） | `visitor.cpp` — 添加新的 Traverse/Visit 方法 |
| 新增统计字段 | `core_types.h` — FunctionStats/AnalysisStats 加字段 |
| 改 JSON 输出格式 | `report.cpp` — 对应的 toJSON 函数 |
| 改文本报告样式 | `report.cpp` — printReport |
| 改行分类逻辑 | `report.cpp` — classifyLines |
| 改 Clang 管线 | `pipeline.cpp` — runClangPipeline |
| 改 CLI 参数 | `frontendAction.cpp` — 加 cl::opt/cl::list |

---

## 4. 12 项代码规范检查（不变）

```
函数行数上限(50)  │  goto 语句  │  参数个数(5)  │  嵌套深度(4)
命名规范          │  全局变量   │  单行长度(100) │  冗余 return
switch 穿透       │  死代码    │  空代码块       │  圈复杂度(10)
```

---

## 5. 使用方式（不变）

```bash
# 构建
docker build -t myclang-cc .

# 分析
docker run --rm myclang-cc /app/output/test.c --json
echo 'int main(){}' | docker run --rm -i myclang-cc --stdin --json
```

---

## 6. 版本演进

```
v5.x   AST+CFG 双引擎，12 项检查
v6.0   JSON 输出
v6.1   内存源码入口（InMemoryFileSystem）
v6.2   结构化错误返回（AnalysisResult + DiagCollector）
v6.3   Docker 容器化部署
v6.4   ✅ 多文件模块化拆分
```

---

*本文档版本 6.4，归档于 2026-06-21。*
