# myclang-c++

基于 Clang/LLVM 的 **C 代码静态分析工具**。

**单文件** → 粘贴代码即时分析 | **项目级** → 多文件上传 + 编译数据库 + 跨文件调用图 | **Web 前端** → 浏览器操作

---

## 功能概览

### 12 项规范检查（单文件）

| 检查项 | 默认阈值 | 说明 |
|--------|---------|------|
| 函数行数 | 50 行 | 超长函数 |
| 单行长度 | 100 字符 | 长行检测 |
| 圈复杂度 (CCN) | 10 | McCabe 圈复杂度 |
| 参数个数 | 5 个 | 参数过多 |
| 嵌套深度 | 4 层 | 深层嵌套 |
| 命名规范 | — | snake_case 函数名 / `g_` 前缀全局变量 |
| goto 语句 | — | goto 使用检测 |
| 冗余 return | — | void 函数末尾 `return;` / `main()` 末尾 `return 0;` |
| switch 穿透 | — | CFG 可达性分析检测 case 穿透 |
| 死代码 | — | CFG 不可达路径检测 |
| 空代码块 | — | `if {}` / `for {}` 等空块检测 |
| 代码行分类 | — | 代码行 / 注释行 / 空行 / 预处理行 |

### 阈值运行时配置

```bash
frontendAction input.c --max-lines 80 --max-ccn 15 --max-params 6
```

### 项目级分析（v7.x）

| 功能 | 说明 |
|------|------|
| `--project <dir>` | 递归扫描目录下所有 `.c` 文件 |
| `--cdb <compile_commands.json>` | 逐文件编译参数（`-I` 路径、`-D` 宏） |
| 跨文件调用图 | 追踪跨文件的函数调用关系 |
| 未定义引用 | 调用了但未在项目中定义的函数 |
| 未调用函数 | 定义了但从未被调用的函数 |
| 多文件重复定义 | 同一函数在多个文件中重复定义 |

---

## 快速开始

### Docker（推荐）

```bash
# 构建
docker build -t myclang-cc .

# 单文件分析
docker run --rm myclang-cc /app/output/test.c

# JSON 输出 + 自定义阈值
docker run --rm myclang-cc /app/output/test.c --json --max-ccn 5

# 管道输入
echo 'int main(){return 0;}' | docker run --rm -i myclang-cc --stdin --json

# 项目分析（挂载自己的 C 项目）
docker run --rm -v $(pwd):/project myclang-cc --project /project

# 项目 + 编译数据库
docker run --rm -v $(pwd):/project myclang-cc --project /project --cdb /project/compile_commands.json
```

### Web 前端

```bash
# 安装
apt install python3.12-venv -y
python3 -m venv .venv
.venv/bin/pip install flask

# 启动
.venv/bin/python3 web/app.py

# 浏览器访问 http://<IP>:5000
```

Web 支持两种模式：

| 模式 | 操作 |
|------|------|
| **单文件分析** | 粘贴 C 代码 → 点击分析 |
| **项目分析** | 上传 `.c` + `.h` 文件 + 可选 `compile_commands.json` → 查看跨文件报告 |

> `Ctrl+Enter` 快捷键触发分析。

### 从源码构建

```bash
apt install g++ make llvm-18-dev libclang-18-dev
make clean && make frontendAction
./frontendAction output/test.c
```

---

## CLI 完整参数

```
用法: frontendAction [选项] 源文件...

分析模式:
  --stdin              从标准输入读取源代码（单文件）
  --project=<dir>      递归扫描目录下所有 .c 文件（项目模式）
  --cdb=<path>         compile_commands.json 路径（配合 --project 使用）

输出:
  --json               输出 JSON（默认文本报告）

阈值（均可运行时覆盖）:
  --max-lines=N        函数行数上限（默认 50）
  --max-line-length=N  单行最大字符数（默认 100）
  --max-ccn=N          圈复杂度上限（默认 10）
  --max-params=N       参数个数上限（默认 5）
  --max-nesting=N      嵌套深度上限（默认 4）
```

---

## JSON 输出示例

```json
{
  "summary": { "totalFunctions": 7, "avgCCN": 2.3, "maxCCN": {"function": "main", "value": 5} },
  "lines": { "total": 150, "code": 120, "blank": 20, "singleComment": 8, "multiComment": 0, "preprocessor": 2 },
  "violations": {
    "overlongFunctions": {"count": 1, "items": [{"name": "foo", "lines": 72}]},
    "highCCN": {"count": 1, "items": [{"name": "main", "ccn": 5}]}
  },
  "thresholds": { "maxFunctionLines": 50, "maxLineLength": 100, "maxCCN": 10, "maxParams": 5, "maxNesting": 4 },
  "projectReport": {
    "crossFileCalls": ["main@main.c → snake_init@snake.c"],
    "undefinedRefs": ["printf", "rand"],
    "uncalledFuncs": {"count": 1, "items": [{"name": "main", "file": "main.c"}]},
    "duplicateFuncs": {"count": 0, "items": []},
    "callGraph": {"main": ["snake_init", "snake_move", "printf"]}
  },
  "functions": [{"name": "foo", "lines": 72, "params": 3, "ccn": 4, "violations": ["overlong"]}]
}
```

---

## 项目结构

```
myclang-c++
├── core_types.h          数据结构 + Thresholds（零 LLVM 依赖）
├── visitor.h / .cpp      AST 访问器 + CFG 分析引擎
├── report.h / .cpp       JSON/文本报告 + 行分类 + 项目级报告
├── pipeline.h / .cpp     Clang 编译管线装配
├── frontendAction.cpp    CLI 入口 + 参数解析 + compile_commands.json
├── IRMake.cpp            LLVM IR 手工构建（实验性）
├── Makefile              双模式构建
├── Dockerfile            多阶段 Docker
├── web/                  Web 前端（Flask + HTML/CSS/JS）
│   ├── app.py            API 后端（单文件 + 项目上传）
│   ├── templates/        页面
│   └── static/           CSS / JS
├── output/               测试用例 + 蛇游戏示例项目
│   ├── *.c               15 个单文件测试
│   └── snake/            贪吃蛇 C 项目（5 源文件 + cdb）
└── v1.0/ ~ v7.1/         历史版本归档
```

### 架构

```
                  core_types.h
                 /     |      \
         visitor    report    pipeline
        (AST+CFG)  (输出)    (Clang管线)
                 \     |      /
              frontendAction (CLI)
                      |
                   web/app.py (Flask API)
                      |
                 浏览器前端
```

---

## 蛇游戏示例

`output/snake/` 是一个终端贪吃蛇游戏，用于演示项目级分析：

```
snake/
├── main.c             游戏循环入口
├── snake.h / snake.c  蛇移动、碰撞
├── board.h / board.c  终端渲染
├── input.h / input.c  键盘输入 (WASD)
├── food.h  / food.c   食物生成
└── compile_commands.json
```

### 编译运行

```bash
gcc -o snake output/snake/*.c -Ioutput/snake
./snake
```

### 分析

```bash
docker run --rm -v $(pwd)/output/snake:/project myclang-cc --project /project
```

输出包含：7 个跨文件调用、11 个未定义引用、逐函数统计。

---

## 技术实现

- **CFG 控制流图**：基于 Clang Analysis CFG 引擎实现死代码、switch 穿透、冗余 return 检测
- **跨文件分析**：后聚合方式，所有文件分析完成后构建项目级调用图
- **编译数据库**：支持 `command`（字符串）和 `arguments`（数组）两种格式，`-I` 路径自动解析为绝对路径
- **LLVM 18 兼容**：处理了 VFS、SourceManager、TargetInfo 等多个 API breaking changes
- **全静态链接**：`--start-group` 解决 LLVM 静态库循环依赖

---

## 版本历史

| 版本 | 主要内容 |
|------|---------|
| v5.x | AST+CFG 双引擎，12 项检查 |
| v6.0 | JSON 输出 |
| v6.1 | 内存源码入口（InMemoryFileSystem） |
| v6.2 | 结构化错误返回（AnalysisResult） |
| v6.3 | Docker 容器化 |
| v6.4 | 多文件模块化拆分（1→10 文件） |
| v6.5 | 运行时阈值配置 + Web 前端（单文件） |
| v7.0 | 项目级目录扫描 — `--project` |
| v7.1 | compile_commands.json — `--cdb` |
| v7.2 | 跨文件调用图 + 项目级报告 + Web 前端（项目上传） |

## License

MIT
