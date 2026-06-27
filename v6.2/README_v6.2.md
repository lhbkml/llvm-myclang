# myclang-c++ v6.2 — 项目介绍文档

> **版本**: 6.2
> **日期**: 2026-06-21
> **适用**: `v6.2/` 目录下的所有文件
> **前序**: [v6.1](../v6.1/README_v6.1.md)
> **主题**: 结构化错误返回 — 取代 exit+stderr

---

## 1. 版本概述

v6.1 解决了"怎么分析"（入口函数化），但"分析失败了怎么办"仍然是 `std::cerr` + `return 1`。Web 后端拿到一个 exit code，不知道发生了什么。

v6.2 将所有错误处理改为结构化返回——`AnalysisResult {success, stats, errors[], warnings[]}`。同时替换 `IgnoringDiagConsumer` 为自定义 `DiagCollector`，收集 Clang 编译诊断（语法错误等），不再丢弃。

### 1.1 核心变更

| 变更 | v6.1 | v6.2 |
|------|------|------|
| 错误返回 | `bool` / exit+stderr | **`AnalysisResult {success, errors[], warnings[]}`** |
| Clang 诊断 | 丢弃（IgnoringDiagConsumer） | **收集（DiagCollector）** |
| 新增结构体 | — | **AnalysisResult / DiagCollector** |
| 函数返回值 | 混用 bool/AnalysisStats | **统一 AnalysisResult** |
| frontendAction.cpp 行数 | 1425 | **1497** |

---

## 2. 结构化错误设计

### 2.1 AnalysisResult

```cpp
struct AnalysisResult {
    bool success = false;           // 分析是否成功
    AnalysisStats stats;            // 统计数据（即使失败也有部分结果）
    std::vector<std::string> errors;   // 流程错误 + Clang Error 诊断
    std::vector<std::string> warnings; // Clang Warning 诊断
};
```

### 2.2 错误来源

```
AnalysisResult.errors
├── 流程错误：File not found、No input files 等
└── Clang Error：语法错误、类型错误、未声明变量等

AnalysisResult.warnings
└── Clang Warning：隐式类型转换、未使用变量等
```

### 2.3 返回值统一

| 函数 | 旧返回 | 新返回 |
|------|--------|--------|
| `runClangPipeline()` | `bool` | `AnalysisResult` |
| `analyzeSourceCode()` | `AnalysisStats` | `AnalysisResult` |
| `analyzeFile()` | `bool` | `AnalysisResult` |

### 2.4 CLI 兼容

`main()` 中检查 `result.success`，失败时遍历 `errors` 输出到 stderr + return 1，行为和以前完全一致：

```cpp
AnalysisResult result = analyzeFile(f, *Invocation);
if (!result.success) {
    for (const auto &e : result.errors)
        std::cerr << "Error: " << e << "\n";
    return 1;
}
```

---

## 3. DiagCollector — 收集 Clang 诊断

v6.1 及之前使用 `IgnoringDiagConsumer`，所有 Clang 编译警告/错误被直接丢弃。v6.2 替换为自定义收集器：

```cpp
class DiagCollector : public clang::DiagnosticConsumer {
    AnalysisResult &Result;
public:
    void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                          const clang::Diagnostic &Info) override {
        if (DiagLevel == DiagnosticsEngine::Ignored ||
            DiagLevel == DiagnosticsEngine::Note)
            return;
        llvm::SmallString<256> Buf;
        Info.FormatDiagnostic(Buf);
        if (DiagLevel == DiagnosticsEngine::Error ||
            DiagLevel == DiagnosticsEngine::Fatal)
            Result.errors.push_back(std::string(Buf.str()));
        else
            Result.warnings.push_back(std::string(Buf.str()));
    }
};
```

---

## 4. 三种使用场景的输出

### 4.1 正常代码

```bash
$ ./frontendAction test.c --json | python3 -c "import json,sys; d=json.load(sys.stdin); print(d['success'])"
True
```

JSON 无 `errors`/`warnings` 键。

### 4.2 语法错误的代码

```bash
$ echo 'int main() { return }' | ./frontendAction --stdin --json
```

```json
{
  "success": false,
  "errors": ["expected expression"],
  "warnings": [],
  "files": ["<stdin>"],
  ...
}
```

### 4.3 文件不存在

```bash
$ ./frontendAction nonexistent.c
Error: File not found: nonexistent.c
Failed to analyze any file
# exit: 1
```

---

## 5. 代码规范检查全景（12 项，不变）

```
  函数行数上限(50行)
  goto 语句
  函数参数上限(5个)
  嵌套深度上限(4)
  命名规范
  全局变量命名
  单行长度(100字符)
  冗余 return
  switch穿透
  死代码
  空代码块
  圈复杂度上限(10)
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
| `frontendAction.cpp` | 1497 | AST+CFG 双引擎 + 12 项规范检查 + JSON/文本双输出 + 内存/磁盘双入口 + 结构化错误 |
| `IRMake.cpp` | 107 | LLVM IR 手工构建（不变，自 v1.0） |
| `Makefile` | 116 | 双项目构建 + test 目标（不变，自 v1.1） |
| `test_params_nesting.c` | 61 | v5.4 补充测试：参数个数 + 嵌套深度 |

---

## 8. 6.x 路线图进度

```
v6.0  ✅ JSON 输出     — toJSON 序列化 + --json 标志
v6.1  ✅ 入口函数化    — analyzeSourceCode + InMemoryFileSystem
v6.2  ✅ 错误标准化    — AnalysisResult + DiagCollector
v6.x  ⬜ Docker 部署    — 可移植 Makefile + Dockerfile
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
v6.2  错误标准化   — AnalysisResult + DiagCollector
```

---

*本文档版本 6.2，归档于 2026-06-21。*
