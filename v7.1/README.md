# myclang-c++

基于 Clang/LLVM 的 **C 代码静态分析工具**，12 项代码规范检查 + 运行时可配置阈值 + Web 前端。

## 功能

### 12 项规范检查

| 检查项 | 默认阈值 | 说明 |
|--------|---------|------|
| 函数行数 | 50 行 | 超长函数拆分 |
| 单行长度 | 100 字符 | 避免横向滚动 |
| 圈复杂度 (CCN) | 10 | McCabe 圈复杂度 |
| 参数个数 | 5 个 | 减少函数参数 |
| 嵌套深度 | 4 层 | 避免深层嵌套 |
| 命名规范 | — | snake_case 函数名 / g_ 前缀全局变量 |
| goto 语句 | — | 检测 goto 使用 |
| 冗余 return | — | void 函数末尾 return; / main 末尾 return 0; |
| switch 穿透 | — | CFG 可达性分析检测 case 穿透 |
| 死代码 | — | CFG 不可达路径检测 |
| 空代码块 | — | if/for/while 等空块检测 |
| 代码行分类 | — | 代码行 / 注释行 / 空行 / 预处理行统计 |

### 运行时阈值配置

```bash
# 5 个阈值均可通过 CLI 参数覆盖
myclang-cc input.c --max-lines 80 --max-ccn 15 --max-params 6 --max-nesting 5 --max-line-length 120
```

## 快速开始

### Docker（推荐）

```bash
# 构建
docker build -t myclang-cc .

# 分析文件（文本报告）
docker run --rm myclang-cc /app/output/test.c

# JSON 输出 + 自定义阈值
docker run --rm myclang-cc /app/output/test.c --json --max-ccn 5

# 管道输入
echo 'int main(){return 0;}' | docker run --rm -i myclang-cc --stdin --json

# 分析自己的代码（挂载目录）
docker run --rm -v $(pwd):/code myclang-cc /code/your_file.c
```

### Web 前端

```bash
# 安装依赖
apt install python3.12-venv -y
python3 -m venv .venv
.venv/bin/pip install flask

# 启动
.venv/bin/python3 web/app.py

# 浏览器打开 http://<IP>:5000
```

> 页面截图：左侧粘贴代码 + 调整阈值，右侧查看分析结果。支持 `Ctrl+Enter` 快捷键。

### 从源码构建

```bash
# 依赖：LLVM/Clang 18 开发库
apt install g++ make llvm-18-dev libclang-18-dev

# 构建
make clean && make frontendAction

# 运行
./frontendAction output/test.c
```

## 项目结构

```
myclang-c++
├── core_types.h          # 数据结构 + Thresholds 结构体（零 LLVM 依赖）
├── visitor.h / .cpp      # AST 访问器 + CFG 分析引擎
├── report.h / .cpp       # JSON 序列化 + 文本报告 + 行分类
├── pipeline.h / .cpp     # Clang 编译管线装配
├── frontendAction.cpp    # CLI 入口（main + 参数解析）
├── IRMake.cpp            # LLVM IR 手工构建（实验性）
├── Makefile              # 双模式构建（系统 LLVM / 自编译 LLVM）
├── Dockerfile            # 多阶段 Docker 构建
├── web/                  # Web 前端
│   ├── app.py            # Flask API 后端
│   ├── templates/index.html
│   └── static/           # CSS + JS
├── output/               # 测试用 C 源文件（15 个）
├── v1.0/ ~ v6.4/         # 历史版本归档
└── README.md
```

### 架构图

```
                  core_types.h (纯 std::)
                 /        |        \
         visitor.h/.cpp  report.h/.cpp  pipeline.h/.cpp
        (AST遍历+CFG)    (JSON+文本)     (Clang管线)
                 \        |        /
              frontendAction.cpp (main + CLI)
                      |
                   web/app.py (Flask API)
```

## CLI 用法

```bash
用法: frontendAction [选项] 源文件...

选项:
  --json               输出 JSON（默认文本报告）
  --stdin              从标准输入读取源代码
  --max-lines=N        函数行数上限（默认 50）
  --max-line-length=N  单行最大字符数（默认 100）
  --max-ccn=N          圈复杂度上限（默认 10）
  --max-params=N       参数个数上限（默认 5）
  --max-nesting=N      嵌套深度上限（默认 4）
```

## JSON 输出示例

```json
{
  "summary": { "totalFunctions": 3, "avgCCN": 2.3, "maxCCN": {"function": "main", "value": 5} },
  "lines": { "total": 45, "code": 30, "blank": 5, "singleComment": 8, "multiComment": 0, "preprocessor": 2 },
  "violations": {
    "overlongFunctions": {"count": 1, "items": [{"name": "foo", "lines": 72}]},
    "highCCN": {"count": 1, "items": [{"name": "main", "ccn": 5}]}
  },
  "thresholds": { "maxFunctionLines": 50, "maxLineLength": 100, "maxCCN": 10, "maxParams": 5, "maxNesting": 4 },
  "functions": [{"name": "foo", "lines": 72, "params": 3, "ccn": 4, "violations": ["overlong"]}]
}
```

## 技术要点

- **CFG 控制流图**：基于 Clang CFG 引擎实现死代码检测、switch 穿透检测、冗余 return 检测
- **LLVM 18 兼容**：处理了 LLVM 18 的多个 API breaking changes（VFS、SourceManager、TargetInfo）
- **全静态链接**：Docker 镜像使用 `--start-group` 解决 LLVM 静态库循环依赖
- **零外部依赖容器**：运行时镜像仅需 `libclang-cpp18` + `libllvm18`

## 版本历史

| 版本 | 主要内容 |
|------|---------|
| v5.x | AST+CFG 双引擎，12 项检查 |
| v6.0 | JSON 输出 |
| v6.1 | 内存源码入口（InMemoryFileSystem） |
| v6.2 | 结构化错误返回（AnalysisResult） |
| v6.3 | Docker 容器化 |
| v6.4 | 多文件模块化拆分（1→10 文件） |
| v6.5 | 运行时阈值配置 + Web 前端 |

## License

MIT
