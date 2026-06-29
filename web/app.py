"""
myclang-cc Web 前端 v7.2
支持单文件分析 + 项目模式（多文件上传 + compile_commands.json）

启动: .venv/bin/pip install flask && .venv/bin/python3 app.py
访问: http://<ECS_IP>:5000
"""

import subprocess
import json
import os
import tempfile
import shutil
from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

DOCKER_IMAGE = os.environ.get("MYCLANG_IMAGE", "myclang-cc")
DOCKER_CMD = ["docker", "run", "--rm", "-i", DOCKER_IMAGE, "--stdin", "--json"]

THRESHOLD_KEYS = ["max-lines", "max-line-length", "max-ccn", "max-params", "max-nesting"]


def _build_threshold_args(data: dict) -> list:
    """从请求数据中提取阈值参数"""
    args = []
    mapping = {
        "maxLines": "max-lines", "maxLineLength": "max-line-length",
        "maxCcn": "max-ccn", "maxParams": "max-params", "maxNesting": "max-nesting",
    }
    for jsonKey, cliKey in mapping.items():
        val = data.get(jsonKey)
        if val is not None and val != "":
            args.extend([f"--{cliKey}", str(int(val))])
    return args


def _run_docker(cmd: list, stdin_text: str = None) -> dict:
    """运行 Docker 命令，解析 JSON 输出"""
    try:
        result = subprocess.run(cmd, input=stdin_text, capture_output=True,
                                text=True, timeout=60)
        try:
            return json.loads(result.stdout)
        except json.JSONDecodeError:
            if result.returncode != 0:
                return {"success": False,
                        "error": result.stderr or result.stdout[:200] or "分析失败"}
            return {"success": False, "error": f"解析结果失败: {result.stdout[:200]}"}
    except subprocess.TimeoutExpired:
        return {"success": False, "error": "分析超时（60秒）"}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ====================== 路由 ======================

@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/analyze", methods=["POST"])
def analyze():
    """单文件分析（粘贴代码）"""
    data = request.get_json()
    if not data or "code" not in data:
        return jsonify({"success": False, "error": "缺少 code 字段"}), 400

    code = data["code"].strip()
    if not code:
        return jsonify({"success": False, "error": "代码为空"}), 400

    cmd = list(DOCKER_CMD) + _build_threshold_args(data)
    return jsonify(_run_docker(cmd, stdin_text=code))


@app.route("/api/analyze-project", methods=["POST"])
def analyze_project():
    """项目模式：上传多个 .c 文件 + 可选 compile_commands.json"""
    files = request.files.getlist("files")
    if not files:
        return jsonify({"success": False, "error": "未上传文件"}), 400

    # 过滤 .c/.h 文件
    srcFiles = [f for f in files if f.filename.endswith(".c") or f.filename.endswith(".h")]
    if not any(f.filename.endswith(".c") for f in srcFiles):
        return jsonify({"success": False, "error": "未找到 .c 文件"}), 400

    # 创建临时目录
    tmpDir = tempfile.mkdtemp(prefix="myclang_")
    try:
        for f in srcFiles:
            # 保留前端传来的相对路径（支持子目录），同时防止路径穿越
            relPath = f.filename.replace("\\", "/")
            # 去掉开头的 / 和 .. 防止逃逸
            while relPath.startswith("/"):
                relPath = relPath[1:]
            parts = [p for p in relPath.split("/") if p and p != ".."]
            if not parts:
                continue
            safeRelPath = os.path.join(*parts)
            destPath = os.path.join(tmpDir, safeRelPath)
            os.makedirs(os.path.dirname(destPath), exist_ok=True)
            f.save(destPath)

        # compile_commands.json
        cdbFile = request.files.get("cdb")
        hasCdb = False
        if cdbFile and cdbFile.filename.endswith(".json"):
            cdbFile.save(os.path.join(tmpDir, "compile_commands.json"))
            hasCdb = True

        # Docker 参数
        dockerArgs = ["docker", "run", "--rm", "-v", f"{tmpDir}:/project",
                      DOCKER_IMAGE, "--project", "/project", "--json"]
        dockerArgs += _build_threshold_args(request.form)
        if hasCdb:
            dockerArgs += ["--cdb", "/project/compile_commands.json"]

        result = _run_docker(dockerArgs)
        return jsonify(result)
    finally:
        shutil.rmtree(tmpDir, ignore_errors=True)


if __name__ == "__main__":
    print(f"Docker 镜像: {DOCKER_IMAGE}")
    print("启动 Web 服务: http://0.0.0.0:5000")
    app.run(host="0.0.0.0", port=5000, debug=False)
