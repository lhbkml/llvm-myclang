"""
myclang-cc Web 前端
方案 B+2: Flask API + 独立前端页面，ECS 直接运行

启动: pip install flask && python app.py
访问: http://<ECS_IP>:5000
"""

import subprocess
import json
import os
from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

DOCKER_IMAGE = os.environ.get("MYCLANG_IMAGE", "myclang-cc")
DOCKER_CMD = ["docker", "run", "--rm", "-i", DOCKER_IMAGE, "--stdin", "--json"]


def run_analysis(code: str, thresholds: dict) -> dict:
    """调用 Docker 执行代码分析，返回 JSON 结果"""
    cmd = list(DOCKER_CMD)
    for key, val in thresholds.items():
        if val is not None and val != "":
            cmd.extend([f"--{key}", str(val)])

    try:
        result = subprocess.run(
            cmd,
            input=code,
            capture_output=True,
            text=True,
            timeout=30,
        )
        # 即使 returncode != 0，工具仍会输出 JSON（success=false + errors 字段）
        try:
            data = json.loads(result.stdout)
            return data
        except json.JSONDecodeError:
            if result.returncode != 0:
                return {"success": False, "error": result.stderr or result.stdout[:200] or "分析失败"}
            return {"success": False, "error": f"解析结果失败: {result.stdout[:200]}"}
    except subprocess.TimeoutExpired:
        return {"success": False, "error": "分析超时（30秒）"}
    except Exception as e:
        return {"success": False, "error": str(e)}


@app.route("/")
def index():
    """首页"""
    return render_template("index.html")


@app.route("/api/analyze", methods=["POST"])
def analyze():
    """代码分析 API"""
    data = request.get_json()
    if not data or "code" not in data:
        return jsonify({"success": False, "error": "缺少 code 字段"}), 400

    code = data["code"]
    if not code.strip():
        return jsonify({"success": False, "error": "代码为空"}), 400

    thresholds = {
        "max-lines": data.get("maxLines"),
        "max-line-length": data.get("maxLineLength"),
        "max-ccn": data.get("maxCcn"),
        "max-params": data.get("maxParams"),
        "max-nesting": data.get("maxNesting"),
    }

    result = run_analysis(code, thresholds)
    return jsonify(result)


if __name__ == "__main__":
    print(f"Docker 镜像: {DOCKER_IMAGE}")
    print("启动 Web 服务: http://0.0.0.0:5000")
    app.run(host="0.0.0.0", port=5000, debug=False)
