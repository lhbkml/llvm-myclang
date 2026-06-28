/**
 * myclang-cc Web 前端 v7.2
 * 单文件分析 + 项目模式 + 跨文件报告
 */

// ====================== DOM 元素 ======================
const codeInput = document.getElementById("codeInput");
const analyzeBtn = document.getElementById("analyzeBtn");
const resultArea = document.getElementById("resultArea");
const sampleBtn = document.getElementById("sampleBtn");
const placeholderText = document.getElementById("placeholderText");

// 模式切换
const modeBtns = document.querySelectorAll(".mode-btn");
const singleMode = document.getElementById("singleMode");
const projectMode = document.getElementById("projectMode");
let currentMode = "single";

// 项目模式元素
const uploadZone = document.getElementById("uploadZone");
const fileInput = document.getElementById("fileInput");
const selectFilesBtn = document.getElementById("selectFilesBtn");
const fileList = document.getElementById("fileList");
const cdbInput = document.getElementById("cdbInput");
let selectedFiles = [];

// 阈值
const thresholdMap = {
    maxLines: "max-lines", maxLineLength: "max-line-length",
    maxCcn: "max-ccn", maxParams: "max-params", maxNesting: "max-nesting",
};

// ====================== 样例代码 ======================
const SAMPLE_CODE = `#include <stdio.h>

int g_counter = 0;

void BadNaming(int A, int B, int C, int D, int E, int F) {
    if (A > 0) { if (B > 0) { if (C > 0) {
        if (D > 0) { if (E > 0) { printf("deep\\n"); } }
    }}}
    return;
}

int main() {
    printf("Hello\\n");
    return 0;
}`;

sampleBtn.addEventListener("click", () => { codeInput.value = SAMPLE_CODE; });

// ====================== 模式切换 ======================
modeBtns.forEach(btn => {
    btn.addEventListener("click", () => {
        modeBtns.forEach(b => b.classList.remove("active"));
        btn.classList.add("active");
        currentMode = btn.dataset.mode;
        singleMode.classList.toggle("hidden", currentMode !== "single");
        projectMode.classList.toggle("hidden", currentMode !== "project");
        analyzeBtn.textContent = currentMode === "project" ? "上传并分析" : "开始分析";
        placeholderText.textContent = currentMode === "project"
            ? "上传 .c 文件，点击「上传并分析」" : "在左侧输入代码，点击「开始分析」";
        resultArea.innerHTML = `<div class="placeholder"><span class="icon">&#x1F50D;</span><p>${placeholderText.textContent}</p></div>`;
    });
});

// ====================== 文件上传 ======================
selectFilesBtn.addEventListener("click", () => fileInput.click());

fileInput.addEventListener("change", () => {
    selectedFiles = Array.from(fileInput.files).filter(f => f.name.endsWith(".c"));
    renderFileList();
});

uploadZone.addEventListener("dragover", e => { e.preventDefault(); uploadZone.classList.add("drag-over"); });
uploadZone.addEventListener("dragleave", () => uploadZone.classList.remove("drag-over"));
uploadZone.addEventListener("drop", e => {
    e.preventDefault();
    uploadZone.classList.remove("drag-over");
    const dropped = Array.from(e.dataTransfer.files).filter(f => f.name.endsWith(".c"));
    if (dropped.length > 0) {
        selectedFiles = [...selectedFiles, ...dropped];
        renderFileList();
    }
});

function renderFileList() {
    if (selectedFiles.length === 0) { fileList.innerHTML = ""; return; }
    fileList.innerHTML = selectedFiles.map((f, i) =>
        `<span class="file-tag">${f.name} <button data-idx="${i}" class="rm-file">&times;</button></span>`
    ).join("");
    fileList.querySelectorAll(".rm-file").forEach(btn => {
        btn.addEventListener("click", () => {
            selectedFiles.splice(parseInt(btn.dataset.idx), 1);
            renderFileList();
        });
    });
}

// ====================== 分析请求 ======================
analyzeBtn.addEventListener("click", async () => {
    if (currentMode === "single") {
        const code = codeInput.value.trim();
        if (!code) return;
        await runSingleAnalysis(code);
    } else {
        if (selectedFiles.length === 0) return;
        await runProjectAnalysis();
    }
});

async function runSingleAnalysis(code) {
    setLoading();
    try {
        const payload = { code };
        addThresholds(payload);
        const resp = await fetch("/api/analyze", {
            method: "POST", headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload),
        });
        renderResult(await resp.json());
    } catch (err) {
        resultArea.innerHTML = `<div class="error-box">请求失败: ${err.message}</div>`;
    } finally { resetBtn(); }
}

async function runProjectAnalysis() {
    setLoading("正在上传并分析...");
    try {
        const formData = new FormData();
        selectedFiles.forEach(f => formData.append("files", f));
        if (cdbInput.files.length > 0) formData.append("cdb", cdbInput.files[0]);
        addThresholdsToForm(formData);

        const resp = await fetch("/api/analyze-project", { method: "POST", body: formData });
        renderResult(await resp.json());
    } catch (err) {
        resultArea.innerHTML = `<div class="error-box">请求失败: ${err.message}</div>`;
    } finally { resetBtn(); }
}

function setLoading(msg = "分析中...") {
    analyzeBtn.disabled = true;
    analyzeBtn.textContent = msg;
    resultArea.innerHTML = `<div class="placeholder"><span class="icon">&#x23F3;</span><p>${msg}</p></div>`;
}

function resetBtn() {
    analyzeBtn.disabled = false;
    analyzeBtn.textContent = currentMode === "project" ? "上传并分析" : "开始分析";
}

function addThresholds(obj) {
    for (const id of Object.keys(thresholdMap)) {
        const el = document.getElementById(id);
        if (el && el.value) obj[id] = parseInt(el.value);
    }
}

function addThresholdsToForm(formData) {
    for (const id of Object.keys(thresholdMap)) {
        const el = document.getElementById(id);
        if (el && el.value) formData.append(id, el.value);
    }
}

// ====================== 结果渲染 ======================
function renderResult(data) {
    if (data.error && !data.summary) {
        resultArea.innerHTML = `<div class="error-box">${escapeHtml(data.error)}</div>`;
        return;
    }

    let html = renderSummary(data);
    html += renderViolations(data);
    html += renderFunctions(data);

    // 项目报告
    if (data.projectReport) {
        html += renderProjectReport(data.projectReport);
    }

    resultArea.innerHTML = html;
}

function renderSummary(data) {
    const s = data.summary || {};
    const l = data.lines || {};
    const t = data.thresholds || {};
    let h = `<div class="result-summary">`;
    h += statCard("函数", s.totalFunctions || 0);
    h += statCard("平均 CCN", (s.avgCCN || 0).toFixed(1));
    h += statCard("最高 CCN", s.maxCCN?.value || 0, s.maxCCN?.function || "");
    h += statCard("代码行", l.code || 0);
    h += statCard("注释行", (l.singleComment || 0) + (l.multiComment || 0));
    h += statCard("文件", (data.files || []).length);
    h += `</div>`;
    return h;
}

function renderViolations(data) {
    const v = data.violations || {};
    const t = data.thresholds || {};
    let h = `<h2>检查结果</h2>`;

    const checks = [
        { key: "overlongFunctions", label: "函数行数", thresh: `上限 ${t.maxFunctionLines || 50} 行` },
        { key: "tooManyParams", label: "参数个数", thresh: `上限 ${t.maxParams || 5} 个` },
        { key: "deepNesting", label: "嵌套深度", thresh: `上限 ${t.maxNesting || 4} 层` },
        { key: "highCCN", label: "圈复杂度", thresh: `上限 ${t.maxCCN || 10}` },
        { key: "badNames", label: "命名规范", thresh: "" },
        { key: "badGlobalVarNames", label: "全局变量命名", thresh: "" },
        { key: "longLines", label: "单行长度", thresh: `上限 ${t.maxLineLength || 100} 字符` },
        { key: "redundantReturns", label: "冗余 return", thresh: "" },
        { key: "deadStmts", label: "死代码", thresh: "" },
        { key: "fallThroughs", label: "switch 穿透", thresh: "" },
        { key: "emptyBlocks", label: "空代码块", thresh: "" },
    ];

    for (const c of checks) {
        const item = v[c.key];
        const count = typeof item === "object" ? (item.count || 0) : (item || 0);
        const items = (item && item.items) ? item.items : [];
        const ok = count === 0;
        h += `<div class="violation-group"><h3 class="${ok ? 'pass' : 'fail'}">${c.label}${c.thresh ? '（'+c.thresh+'）' : ''}: ${ok ? '✓' : '✗ '+count}</h3>`;
        if (!ok && items.length > 0) {
            h += `<ul>${items.map(i => {
                const n = i.name || i.function || "";
                const d = i.lines ? `${i.lines}行` : i.params ? `${i.params}参数` : i.depth ? `深度${i.depth}` : i.ccn ? `CCN=${i.ccn}` : i.length ? `${i.length}字符` : i.type || "";
                return `<li>${escapeHtml(n)} ${d}</li>`;
            }).join("")}</ul>`;
        }
        h += `</div>`;
    }
    return h;
}

function renderFunctions(data) {
    const funcs = data.functions || [];
    if (funcs.length === 0) return "";
    let h = `<h2>逐函数统计</h2><div class="func-table-wrap"><table class="func-table">`;
    h += `<tr><th>函数</th><th>行</th><th>参数</th><th>CCN</th><th>嵌套</th><th>局部变量</th><th>标记</th></tr>`;
    for (const f of funcs) {
        const flags = [];
        if (f.violations?.includes("overlong")) flags.push({ t: "行数超标", c: "flag-err" });
        if (f.violations?.includes("highCCN")) flags.push({ t: "高CCN", c: "flag-err" });
        if (f.violations?.includes("tooManyParams")) flags.push({ t: "参数多", c: "flag-warn" });
        if (f.violations?.includes("deepNesting")) flags.push({ t: "嵌套深", c: "flag-warn" });
        if (f.violations?.includes("badName")) flags.push({ t: "命名", c: "flag-warn" });
        if (f.violations?.includes("deadCode")) flags.push({ t: "死代码", c: "flag-err" });
        if (f.violations?.includes("fallThrough")) flags.push({ t: "穿透", c: "flag-warn" });
        h += `<tr><td><strong>${escapeHtml(f.name)}</strong></td><td>${f.lines}</td><td>${f.params}</td><td>${f.ccn}</td><td>${f.maxNesting}</td><td>${f.localVars}</td><td>${flags.map(fl => `<span class="flag ${fl.c}">${fl.t}</span>`).join(" ")}</td></tr>`;
    }
    h += `</table></div>`;
    return h;
}

function renderProjectReport(pr) {
    let h = `<h2 style="margin-top:16px;color:#e94560;">跨文件分析报告</h2>`;
    h += `<div class="result-summary">`;
    h += statCard("总函数", pr.totalFunctions || 0);
    h += statCard("跨文件调用", (pr.crossFileCalls || []).length);
    h += statCard("未定义引用", (pr.undefinedRefs || []).length);
    h += statCard("未调用函数", pr.uncalledFuncs?.count || 0);
    h += statCard("重复定义", pr.duplicateFuncs?.count || 0);
    h += `</div>`;

    // 跨文件调用
    const cross = pr.crossFileCalls || [];
    h += `<div class="violation-group"><h3 class="${cross.length === 0 ? 'pass' : 'fail'}">跨文件调用: ${cross.length} 处</h3>`;
    if (cross.length > 0) h += `<ul>${cross.map(c => `<li>${escapeHtml(c)}</li>`).join("")}</ul>`;
    h += `</div>`;

    // 未定义引用
    const undefs = pr.undefinedRefs || [];
    h += `<div class="violation-group"><h3 class="${undefs.length === 0 ? 'pass' : 'fail'}">未定义引用: ${undefs.length} 个</h3>`;
    if (undefs.length > 0) h += `<ul>${undefs.map(f => `<li>${escapeHtml(f)}()</li>`).join("")}</ul>`;
    h += `</div>`;

    // 未调用函数
    const uncalled = pr.uncalledFuncs || {};
    h += `<div class="violation-group"><h3 class="${uncalled.count === 0 ? 'pass' : 'fail'}">未被调用的函数: ${uncalled.count || 0} 个</h3>`;
    if (uncalled.items && uncalled.items.length > 0) {
        h += `<ul>${uncalled.items.map(i => `<li>${escapeHtml(i.name)}() (${escapeHtml(i.file)})</li>`).join("")}</ul>`;
    }
    h += `</div>`;

    // 重复定义
    const dups = pr.duplicateFuncs || {};
    h += `<div class="violation-group"><h3 class="${dups.count === 0 ? 'pass' : 'fail'}">多文件重复定义: ${dups.count || 0} 个</h3>`;
    if (dups.items && dups.items.length > 0) {
        h += `<ul>${dups.items.map(i => `<li>${escapeHtml(i.name)}(): ${(i.files||[]).join(', ')}</li>`).join("")}</ul>`;
    }
    h += `</div>`;

    return h;
}

// ====================== 辅助 ======================
function statCard(label, value, sub = "") {
    return `<div class="stat-card"><div class="num">${value}</div><div class="label">${label}${sub ? ' ('+sub+')' : ''}</div></div>`;
}

function escapeHtml(str) {
    const div = document.createElement("div");
    div.textContent = String(str);
    return div.innerHTML;
}

document.addEventListener("keydown", e => {
    if ((e.ctrlKey || e.metaKey) && e.key === "Enter") {
        e.preventDefault();
        analyzeBtn.click();
    }
});
