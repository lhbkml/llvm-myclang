# myclang-c++ v6.5 — 项目介绍文档

> **版本**: 6.5
> **日期**: 2026-06-28
> **前序**: [v6.4](../v6.4/README_v6.4.md)
> **主题**: 运行时阈值配置 + Web 前端

---

## 1. 版本概述

v6.4 完成了多文件模块化拆分（1→10 文件），v6.5 新增两大功能：

1. **运行时阈值配置**：原来 5 个检查阈值（函数行数、单行长度、圈复杂度、参数个数、嵌套深度）硬编码在 `core_types.h` 中，现在支持 CLI 参数 `--max-lines` `--max-line-length` `--max-ccn` `--max-params` `--max-nesting` 运行时覆盖
2. **Web 前端**：Flask API + HTML/CSS/JS 前后端分离架构，浏览器粘贴代码即可分析

---

## 2. 新增功能

### 2.1 运行时阈值配置

#### 新增结构体 `Thresholds`（core_types.h）

```cpp
struct Thresholds {
    int maxFunctionLines = 50;
    int maxLineLength    = 100;
    int maxCCN           = 10;
    int maxParams        = 5;
    int maxNesting       = 4;
};
```

旧 `MAX_*` 静态常量保留为 deprecated alias，向后兼容。

#### 新增 CLI 参数（frontendAction.cpp）

```
--max-lines=N        函数行数上限（默认 50）
--max-line-length=N  单行最大字符数（默认 100）
--max-ccn=N          圈复杂度上限（默认 10）
--max-params=N       参数个数上限（默认 5）
--max-nesting=N      嵌套深度上限（默认 4）
```

#### 参数传递链

```
main() 构造 Thresholds
  → analyzeFile/analyzeSourceCode(pipeline)
    → runClangPipeline → MyASTConsumer → MyASTVisitor (visitor)
    → classifyLines (report)
  → printReport / toJSON (report)
```

JSON 输出新增 `"thresholds"` 键，方便前端消费。

#### 修改文件

| 文件 | 变更 |
|------|------|
| `core_types.h` | 新增 `Thresholds` struct + `kDefaultThresholds` |
| `visitor.h/.cpp` | 新增 `const Thresholds &Thresh` 成员，4 处常量→Thresh.* |
| `report.h/.cpp` | 10 处常量→Thresh.*，JSON 新增 `"thresholds"` 键 |
| `pipeline.h/.cpp` | 3 个函数新增 `const Thresholds &` 参数 |
| `frontendAction.cpp` | 5 个 `cl::opt<int>` CLI 标志 + 构造 Thresholds |

### 2.2 Web 前端

#### 架构

```
浏览器（HTML/CSS/JS）
  ↕ HTTP
Flask API（web/app.py）
  ↕ subprocess
Docker myclang-cc --stdin --json
```

#### 文件结构

```
web/
├── app.py              # Flask API: GET / + POST /api/analyze
├── requirements.txt    # flask>=3.0
├── templates/
│   └── index.html      # 左右分栏布局
└── static/
    ├── style.css       # 暗色主题
    └── script.js       # 前端逻辑 + 结果渲染
```

#### 使用方式

```bash
# ECS 上直接运行（需先有 myclang-cc Docker 镜像）
apt install python3.12-venv -y
python3 -m venv .venv
.venv/bin/pip install flask
nohup .venv/bin/python3 web/app.py > /tmp/web.log 2>&1 &

# 浏览器访问 http://<ECS公网IP>:5000
```

页面功能：
- 左侧代码编辑器 + 可折叠阈值面板
- 右侧结果面板：统计卡片 + 逐项检查 + 逐函数表格
- `Ctrl+Enter` 快捷键触发分析
- 「加载示例」按钮提供演示代码

---

## 3. 遇到的问题与解决

### 问题 1：ECS 虚拟环境创建失败

```
The virtual environment was not created successfully because ensurepip is not available.
```

**原因**：Alibaba Cloud Ubuntu 24.04 LTS 精简镜像未安装 `python3-venv`。

**解决**：
```bash
apt install python3.12-venv -y
```

### 问题 2：Flask 安装被 PEP 668 阻止

```
error: externally-managed-environment
```

**原因**：Ubuntu 24.04 默认禁止系统级 pip 安装。

**解决**：使用 venv 虚拟环境隔离安装。

### 问题 3：Web 端始终显示"分析失败"

**现象**：Docker CLI 正常输出 JSON，但网页始终报"分析失败"。

**根因**：`app.py` 中当 Docker 返回非零 exit code 时直接丢弃 stdout（含 JSON），返回通用错误。但工具遇到 C 语法错误时会返回 exit code 1 同时输出 JSON（success=false + errors 字段）。

**解决**：修改 `run_analysis()` 先尝试解析 JSON，失败时才回退到错误信息。

### 问题 4：ECS Docker 镜像未更新

**现象**：Web 前端点击分析后报错 `Unknown command line argument '--max-lines'`。

**原因**：GitHub 推送了 Web 代码但未推送阈值功能代码（8 个文件只在本地 WSL2，未 commit）。

**解决**：补推送阈值功能代码，ECS 上 `git pull + docker build` 重建。

### 问题 5：GitHub push 认证过期

**现象**：`fatal: Authentication failed for 'https://github.com/...'`。

**原因**：GitHub Personal Access Token 过期。

**解决**：用户侧更新 Token 后重新推送。

---

## 4. 文件清单

| 文件 | 行数 | 职责 |
|------|------|------|
| `core_types.h` | 214 | 数据结构 + Thresholds 结构体 |
| `visitor.h` | 75 | AST 访问器声明 |
| `visitor.cpp` | 449 | AST 遍历 + CFG 分析引擎 |
| `report.h` | 33 | JSON/文本报告声明 |
| `report.cpp` | 565 | JSON 序列化 + 文本报告 + 行分类 |
| `pipeline.h` | 30 | Clang 管线声明 |
| `pipeline.cpp` | 118 | Clang 编译管线装配 |
| `frontendAction.cpp` | 152 | CLI 入口 + main |
| `IRMake.cpp` | 108 | LLVM IR 手工构建（实验性） |
| `Makefile` | 135 | 双模式构建 |
| `Dockerfile` | 33 | 多阶段 Docker 构建 |
| `web/app.py` | 84 | Flask API 后端 |
| `web/templates/index.html` | 43 | Web 前端页面 |
| `web/static/style.css` | 175 | 暗色主题样式 |
| `web/static/script.js` | 173 | 前端逻辑 + 结果渲染 |
| `README.md` | 166 | 项目主文档 |

---

## 5. 版本演进

```
v5.x   AST+CFG 双引擎，12 项检查
v6.0   JSON 输出
v6.1   内存源码入口（InMemoryFileSystem）
v6.2   结构化错误返回（AnalysisResult + DiagCollector）
v6.3   Docker 容器化部署
v6.4   多文件模块化拆分（1→10 文件）
v6.5   ✅ 运行时阈值配置 + Web 前端
```

---

*本文档版本 6.5，归档于 2026-06-28。*
