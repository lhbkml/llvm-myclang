# myclang-c++ v3.1 — 项目介绍文档

> **版本**: 3.1
> **日期**: 2026-06-16
> **适用**: `v3.1/` 目录下的所有文件
> **前序**: [v3.0](../v3.0/README_v3.0.md)
> **主题**: 全局变量命名规范 + 单行长度检查

---

## 1. 版本概述

v3.1 在 v3.0 三项规范检查的基础上，新增全局变量 `g_` 前缀强制校验和单行代码长度检测。同时统一了所有检查项的输出格式：`项: ⚠ 发现 N 个 xxx`。

### 1.1 核心变更

| 变更 | v3.0 | v3.1 |
|------|------|------|
| 代码规范检查项 | 3 项 | **5 项** |
| 全局变量命名 | ❌ | ✅ 强制 `g_` 前缀 |
| 单行长度 | ❌ | ✅ 阈值 100 字符 |
| 输出格式 | 不统一 | ✅ 统一 `⚠ 发现 N 个` 模式 |
| FunctionStats 字段 | 18 个 | 18 个 |
| AnalysisStats 字段 | 20 个 | 23 个 (+badGlobalVarNames, +badGlobalVars, +longLineCount, +longLines) |
| 新增结构体 | — | `LongLineInfo` |

---

## 2. 新增检查项

### 2.1 全局变量命名 — 强制 `g_` 前缀

校验函数 `isValidGlobalVarName()`：

```cpp
static bool isValidGlobalVarName(const std::string &name) {
    if (name.size() < 3) return false;
    if (name[0] != 'g' || name[1] != '_') return false;
    return isValidSnakeCase(name.substr(2));
}
```

| 变量名 | 判定 | 原因 |
|--------|------|------|
| `g_count` | ✓ | `g_` + snake_case |
| `g_buffer_size` | ✓ | `g_` + snake_case |
| `app_name` | ✗ | 缺 `g_` 前缀 |
| `GLOBAL` | ✗ | 大写且缺 `g_` |
| `_private` | ✗ | 下划线开头且缺 `g_` |

检测在 `VisitVarDecl` 的全局变量分支中完成，仅针对主文件的全局变量（头文件中的全局变量已被 `isFromMainFile` 过滤）。

### 2.2 单行代码长度 — 阈值 100 字符

```cpp
static const int MAX_LINE_LENGTH = 100;

struct LongLineInfo {
    std::string file;
    int lineNum;
    int length;
};
```

检测逻辑在 `analyzeFile()` 中，AST 分析完成后用 `std::ifstream` 逐行读取源文件，比对字符数：

```cpp
std::ifstream src(FilePath);
while (std::getline(src, line)) {
    if (line.length() > MAX_LINE_LENGTH) {
        Stats.longLineCount++;
        Stats.longLines.push_back({FilePath, lineNum, len});
    }
}
```

**注意**：这是纯文本级检查，不依赖 AST，因此也能检测注释行和预处理指令。

---

## 3. 统一输出格式

v3.0 中格式不一致，v3.1 统一为 `项: ⚠ 发现 N 个 xxx` + 逐条缩进详情：

```
【代码规范检查】
  函数行数上限(50行): ✓ 全部在限制内

  goto 语句:   ⚠ 发现 1 处
    - loop_test(): 1 处
  命名规范:   ⚠ 发现 4 个不规范
    - BadName(): 应使用小写下划线风格
    - badCamelCase(): 应使用小写下划线风格
    - UPPER_CASE(): 应使用小写下划线风格
    - _leading_underscore(): 应使用小写下划线风格
  全局变量命名: ⚠ 发现 3 个缺少 g_ 前缀
    - bad_var → 应改为 g_bad_var
    - GLOBAL → 应改为 g_GLOBAL
    - _private → 应改为 g__private
  单行长度(100字符): ⚠ 发现 3 行超标
    - output/test_long_lines.c:8 (118 字符)
    - output/test_long_lines.c:9 (135 字符)
    - output/test_long_lines.c:10 (117 字符)
```

---

## 4. 测试文件

| 文件 | 测试目标 |
|------|---------|
| `output/test_overlong.c` | 函数行数超标（59 行） |
| `output/test_bad_names.c` | 函数命名不规范（4/6） |
| `output/test_global_vars.c` | 全局变量缺少 g_ 前缀（3/5） |
| `output/test_long_lines.c` | 单行超 100 字符（3 行） |

---

## 5. 指标清单

### 5.1 全局汇总（23 项）

| 分类 | 指标 | 版本 |
|------|------|:--:|
| 基础 | 文件数、函数数 | v1.x |
| 参数 | 无参函数、有参函数 | v2.1 |
| 空函数 | 空函数数 | v2.1 |
| ★规范 | 行数超标函数 | v3.0 |
| ★规范 | 命名不规范函数 | v3.0 |
| ★规范 | 全局变量缺 g_ 前缀 | v3.1 |
| ★规范 | 单行超长 | v3.1 |
| 变量 | 全局变量、局部变量 | v1.x |
| 控制流 | if/for/while/do-while/switch/break/continue/return/goto | v1.x |
| 预处理 | #include | v1.4 |
| 调用 | 调用次数、调用分布 | v1.x |

### 5.2 逐函数明细（18 项）

```
name | lines | localVars | paramCount |
isEmptyOrReturnOnly | isOverlong | isBadName |
ifCount | forCount | whileCount | doWhileCount |
switchCount | breakCount | continueCount |
returnCount | gotoCount | callCount | callTargets
```

---

## 6. 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `frontendAction.cpp` | 665 | 多文件分析 + 23 项全局指标 + 18 项逐函数明细 + 5 项规范检查 |
| `IRMake.cpp` | 107 | LLVM IR 手工构建（不变，自 v1.0） |
| `Makefile` | 116 | 双项目构建 + test 目标（不变，自 v1.1） |

---

## 7. 版本演进总览

```
v1.0  原型验证     — 5种AST节点逐行打印
v1.1  实用化       — 统计-报告架构，6种节点，双项目构建
v1.2  突破(#include) — 方案A：手动HeaderSearchOpts
v1.3  成熟化       — 方案B：CreateFromArgs自动发现
v1.4  扩展         — 13项指标 + PPCallbacks
v1.5  多文件       — 批量分析 + Stmt过滤 + 变量拆分
v2.0  逐函数       — TraverseFunctionDecl 上下文 + 逐函数明细
v2.1  空函数&参数  — 空函数检测 + 有参/无参分类
v3.0  代码规范     — 行数上限 + goto禁用 + snake_case命名
v3.1  规范扩展     — 全局变量 g_ 前缀 + 单行长度
```

---

*本文档版本 3.1，归档于 2026-06-16。*
