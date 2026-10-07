// Side panel with the history of a single entity.
import { api } from "./api.js";
import { lineChart, timeline } from "./charts.js";
import { ago, dateTime, entityValue, esc, num, decimalsFor } from "./format.js";
import { store } from "./store.js";
import { widget, bindControls } from "./controls.js";

const RANGES = [["6h", 6 * 3600], ["24h", 86400], ["7d", 7 * 86400], ["30d", 30 * 86400], ["1y", 365 * 86400]];

const drawer = document.getElementById("drawer");
const body = document.getElementById("drawer-body");
let chart = null;
let currentId = null;

drawer.addEventListener("click", (ev) => { if (ev.target.closest("[data-close]")) close(); });
document.addEventListener("keydown", (ev) => { if (ev.key === "Escape" && !drawer.hidden) close(); });
bindControls(body, () => { const e = store.entities.get(currentId); if (e) renderWidget(e); });

export function close() {
  drawer.hidden = true;
  chart?.destroy();
  chart = null;
  currentId = null;
}

function renderWidget(e) {
  const slot = body.querySelector("#drawer-widget");
  if (slot) slot.innerHTML = e.writable ? widget(e) : "";
}

export function refreshDrawer(e) {
  if (e.id !== currentId) return;
  body.querySelector("#drawer-now").innerHTML = `${entityValue(e)} <small>${esc(e.unit || "")}</small>`;
  document.getElementById("drawer-sub").textContent = `Updated ${ago(e.updated)}`;
  if (!body.querySelector(".stepper.pending")) renderWidget(e);
}

export function openEntity(id, range = 86400) {
  const e = store.entities.get(id);
  if (!e) return;
  currentId = id;
  drawer.hidden = false;
  document.getElementById("drawer-title").textContent = e.name;
  document.getElementById("drawer-sub").textContent = `Updated ${ago(e.updated)}`;
  body.innerHTML = `
    <div style="display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap">
      <div class="big-number" id="drawer-now">${entityValue(e)} <small>${esc(e.unit || "")}</small></div>
      <div id="drawer-widget"></div>
    </div>
    <div class="pills" style="margin:12px 0 16px">
      <span class="pill">${esc(e.platform)}</span>
      ${e.device_class ? `<span class="pill">${esc(e.device_class)}</span>` : ""}
      ${e.min != null ? `<span class="pill">range <b>${e.min} – ${e.max}</b></span>` : ""}
      <span class="pill faint">${esc(e.id)}</span>
    </div>
    <div class="segmented" id="drawer-ranges">${RANGES.map(([l, s]) => `<button data-s="${s}" class="${s === range ? "active" : ""}">${l}</button>`).join("")}</div>
    <div class="stat-row" id="drawer-stats"></div>
    <div id="drawer-chart"></div>`;
  renderWidget(e);
  body.querySelector("#drawer-ranges").addEventListener("click", (ev) => {
    const b = ev.target.closest("button[data-s]");
    if (b) openEntity(id, +b.dataset.s);
  });
  load(e, range);
}

async function load(e, range) {
  const end = Date.now() / 1000, start = end - range;
  const el = body.querySelector("#drawer-chart");
  const stats = body.querySelector("#drawer-stats");
  el.innerHTML = `<div class="skeleton" style="height:280px"></div>`;
  const res = await api.history([e.id], start, end, 800);
  if (e.id !== currentId) return;
  const s = res.series[e.id];
  chart?.destroy();
  chart = null;

  if (e.kind === "numeric") {
    const values = s.v.filter((v) => v != null);
    const d = decimalsFor(e.unit, values[0]);
    const avg = values.length ? values.reduce((a, b) => a + b, 0) / values.length : null;
    const mins = s.min.filter((v) => v != null), maxs = s.max.filter((v) => v != null);
    const isTotal = e.state_class === "total_increasing";
    stats.innerHTML = isTotal
      ? stat("Change", values.length ? num(values[values.length - 1] - values[0], d) : "–", e.unit)
        + stat("Start", num(values[0], d), e.unit) + stat("End", num(values[values.length - 1], d), e.unit) + stat("Samples", num(s.t.length, 0))
      : stat("Min", mins.length ? num(Math.min(...mins), d) : "–", e.unit) + stat("Avg", num(avg, d), e.unit)
        + stat("Max", maxs.length ? num(Math.max(...maxs), d) : "–", e.unit) + stat("Points", num(s.t.length, 0));
    chart = lineChart(el, { series: [{ label: e.name, color: "--s1", unit: e.unit, decimals: d }], height: 300, unit: e.unit });
    chart.setData([[s.t, s.v]]);
  } else {
    // share of time per state
    const totals = new Map();
    for (let i = 0; i < s.t.length; i++) {
      const a = s.t[i], b = i + 1 < s.t.length ? s.t[i + 1] : end;
      const k = s.text[i] ?? "–";
      totals.set(k, (totals.get(k) || 0) + (b - a));
    }
    const all = [...totals.values()].reduce((a, b) => a + b, 0) || 1;
    stats.innerHTML = [...totals.entries()].sort((a, b) => b[1] - a[1]).slice(0, 4)
      .map(([k, v]) => stat(k, num((v / all) * 100, 0), "%")).join("");
    const order = e.platform === "switch" || e.platform === "binary_sensor" ? ["ON", "OFF"] : e.options;
    timeline(el, [{ label: e.name, data: s, order }], start, end);
    const changes = [];
    for (let i = s.t.length - 1; i >= 0 && changes.length < 12; i--) {
      if (i === 0 || s.text[i] !== s.text[i - 1]) changes.push(`<tr><td>${dateTime(s.t[i])}</td><td>${esc(s.text[i])}</td></tr>`);
    }
    el.insertAdjacentHTML("beforeend", `<h3 style="margin:20px 0 6px">Recent changes</h3><div class="table-wrap"><table><tbody>${changes.join("") || `<tr><td class="faint">No changes</td></tr>`}</tbody></table></div>`);
  }
}

function stat(k, v, unit = "") {
  return `<div><div class="k">${esc(k)}</div><div class="v">${v}${unit ? ` <small class="muted" style="font-size:12px;font-weight:500">${esc(unit)}</small>` : ""}</div></div>`;
}
