/**
 * myclang-cc Web 前端
 * 处理代码分析请求 + 结果渲染
 */

const codeInput = document.getElementById("codeInput");
const analyzeBtn = document.getElementById("analyzeBtn");
const resultArea = document.getElementById("resultArea");
const sampleBtn = document.getElementById("sampleBtn");

// 阈值输入映射
const thresholdMap = {
    maxLines: "max-lines",
    maxLineLength: "max-line-length",
    maxCcn: "max-ccn",
    maxParams: "max-params",
    maxNesting: "max-nesting",
};

// ====================== 样例代码 ======================
const SAMPLE_CODE = `#include <stdio.h>

int g_counter = 0;   // 全局变量缺少 g_ 前缀

// 命名不规范 (驼峰)
void BadNaming(int A, int B, int C, int D, int E, int F) {
    if (A > 0) {
        if (B > 0) {
            if (C > 0) {
                if (D > 0) {
                    if (E > 0) {
                        printf("嵌套太深了\\n");
                    }
                }
            }
        }
    }
    return;  // void 函数末尾冗余 return
}

int main() {
    printf("Hello\\n");
    return 0;   // main 末尾冗余 return 0
}`;

sampleBtn.addEventListener("click", () => {
    codeInput.value = SAMPLE_CODE;
});

// ====================== 分析请求 ======================
analyzeBtn.addEventListener("click", async () => {
    const code = codeInput.value.trim();
    if (!code) return;

    analyzeBtn.disabled = true;
    analyzeBtn.textContent = "分析中...";
    resultArea.innerHTML = `<div class="placeholder"><span class="icon">&#x23F3;</span><p>正在分析...</p></div>`;

    try {
        const payload = { code };
        for (const [id, key] of Object.entries(thresholdMap)) {
            const el = document.getElementById(id);
            if (el && el.value) payload[id] = parseInt(el.value);
        }

        const resp = await fetch("/api/analyze", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload),
        });

        const data = await resp.json();
        renderResult(data);
    } catch (err) {
        resultArea.innerHTML = `<div class="error-box">请求失败: ${err.message}</div>`;
    } finally {
        analyzeBtn.disabled = false;
        analyzeBtn.textContent = "开始分析";
    }
});

// ====================== 结果渲染 ======================
function renderResult(data) {
    if (data.error) {
        resultArea.innerHTML = `<div class="error-box">${escapeHtml(data.error)}</div>`;
        return;
    }

    const stats = data;
    const summary = stats.summary || {};
    const lines = stats.lines || {};
    const violations = stats.violations || {};
    const functions = stats.functions || [];
    const thresholds = stats.thresholds || {};

    let html = "";

    // --- 摘要卡片 ---
    html += `<div class="result-summary">`;
    html += statCard("函数总数", summary.totalFunctions || 0);
    html += statCard("平均 CCN", (summary.avgCCN || 0).toFixed(1));
    html += statCard("最高 CCN", summary.maxCCN?.value || 0, summary.maxCCN?.function || "");
    html += statCard("代码行数", lines.code || 0);
    html += statCard("注释行", (lines.singleComment || 0) + (lines.multiComment || 0));
    html += statCard("全局变量", summary.globalVars || 0);
    html += `</div>`;

    // --- 违规检查 ---
    html += `<h2 style="margin-top:8px;">检查结果</h2>`;

    const checks = [
        { key: "overlongFunctions", label: "函数行数", thresh: `上限 ${thresholds.maxFunctionLines || 50} 行`, unit: "个函数" },
        { key: "tooManyParams", label: "参数个数", thresh: `上限 ${thresholds.maxParams || 5} 个`, unit: "个函数" },
        { key: "deepNesting", label: "嵌套深度", thresh: `上限 ${thresholds.maxNesting || 4} 层`, unit: "个函数" },
        { key: "highCCN", label: "圈复杂度", thresh: `上限 ${thresholds.maxCCN || 10}`, unit: "个函数" },
        { key: "badNames", label: "命名规范", thresh: "", unit: "个不规范" },
        { key: "badGlobalVarNames", label: "全局变量命名", thresh: "", unit: "个缺 g_ 前缀" },
        { key: "longLines", label: "单行长度", thresh: `上限 ${thresholds.maxLineLength || 100} 字符`, unit: "行超标" },
        { key: "redundantReturns", label: "冗余 return", thresh: "", unit: "处" },
        { key: "deadStmts", label: "死代码", thresh: "", unit: "条不可达语句" },
        { key: "fallThroughs", label: "switch 穿透", thresh: "", unit: "处穿透" },
        { key: "emptyBlocks", label: "空代码块", thresh: "", unit: "处" },
    ];

    for (const check of checks) {
        const v = violations[check.key];
        const count = typeof v === "object" ? (v.count || 0) : (v || 0);
        html += violationRow(check.label, check.thresh, count, check.unit, v?.items || []);
    }

    // --- goto ---
    const gotoCount = violations.gotoCount || 0;
    html += violationRow("goto 语句", "", gotoCount, "处", []);

    // --- 逐函数表 ---
    if (functions.length > 0) {
        html += `<h2 style="margin-top:16px;">逐函数统计</h2>`;
        html += `<div class="func-table-wrap"><table class="func-table">`;
        html += `<tr><th>函数名</th><th>行数</th><th>参数</th><th>CCN</th><th>嵌套</th><th>局部变量</th><th>标记</th></tr>`;
        for (const f of functions) {
            const flags = [];
            if (f.violations?.includes("overlong")) flags.push({ t: "行数超标", c: "flag-err" });
            if (f.violations?.includes("highCCN")) flags.push({ t: "圈复杂度过高", c: "flag-err" });
            if (f.violations?.includes("tooManyParams")) flags.push({ t: "参数过多", c: "flag-warn" });
            if (f.violations?.includes("deepNesting")) flags.push({ t: "嵌套过深", c: "flag-warn" });
            if (f.violations?.includes("badName")) flags.push({ t: "命名不规范", c: "flag-warn" });
            if (f.violations?.includes("redundantReturn")) flags.push({ t: "冗余 return", c: "flag-warn" });
            if (f.violations?.includes("deadCode")) flags.push({ t: "死代码", c: "flag-err" });
            if (f.violations?.includes("fallThrough")) flags.push({ t: "switch穿透", c: "flag-warn" });
            if (f.violations?.includes("emptyOrReturnOnly")) flags.push({ t: "空/仅return", c: "flag-warn" });

            html += `<tr>
                <td><strong>${escapeHtml(f.name)}</strong></td>
                <td>${f.lines}</td>
                <td>${f.params}</td>
                <td>${f.ccn}</td>
                <td>${f.maxNesting}</td>
                <td>${f.localVars}</td>
                <td>${flags.map(fl => `<span class="flag ${fl.c}">${fl.t}</span>`).join(" ")}</td>
            </tr>`;
        }
        html += `</table></div>`;
    }

    resultArea.innerHTML = html;
}

// ====================== 辅助函数 ======================
function statCard(label, value, sub = "") {
    const val = typeof value === "number" ? value : value;
    return `<div class="stat-card">
        <div class="num">${val}</div>
        <div class="label">${label}${sub ? ` (${sub})` : ""}</div>
    </div>`;
}

function violationRow(label, thresh, count, unit, items) {
    const ok = count === 0;
    const cls = ok ? "pass" : "fail";
    const status = ok ? "✓ 全部在限制内" : `✗ 发现 ${count} ${unit}`;
    const title = thresh ? `${label}（${thresh}）` : label;

    let html = `<div class="violation-group"><h3 class="${cls}">${title}: ${status}</h3>`;
    if (!ok && items.length > 0) {
        html += `<ul>`;
        for (const item of items) {
            const name = item.name || item.function || item.file || "";
            const detail = item.lines ? `${item.lines} 行` :
                           item.params ? `${item.params} 个参数` :
                           item.depth ? `深度 ${item.depth}` :
                           item.ccn ? `CCN=${item.ccn}` :
                           item.length ? `${item.length} 字符` :
                           item.type ? `${item.type}` : "";
            const loc = item.line ? `:${item.line}` : "";
            html += `<li>${escapeHtml(name)}${loc} ${detail}</li>`;
        }
        html += `</ul>`;
    }
    html += `</div>`;
    return html;
}

function escapeHtml(str) {
    const div = document.createElement("div");
    div.textContent = str;
    return div.innerHTML;
}

// ====================== 快捷键 ======================
document.addEventListener("keydown", (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === "Enter") {
        e.preventDefault();
        analyzeBtn.click();
    }
});
