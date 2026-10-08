import { api } from "../api.js";
import { SLOTS, lineChart, timeline } from "../charts.js";
import { decimalsFor, esc } from "../format.js";
import { N, ent, store } from "../store.js";

const RANGES = [["1h", 3600], ["6h", 6 * 3600], ["24h", 86400], ["7d", 7 * 86400], ["30d", 30 * 86400], ["90d", 90 * 86400], ["1y", 365 * 86400]];

const PANELS = [
  { id: "temps", title: "Temperatures", unit: "°C", span: 12, height: 300,
    series: [[N.outdoor, "Outdoor"], [N.waterOut, "Flow out"], [N.waterIn, "Return"], [N.tank, "DHW tank"], [N.room1, "Room zone 1"], [N.room2, "Room zone 2"], [N.lawTarget, "Water law target"]] },
  { id: "power", title: "Electrical power", unit: "W", span: 12, height: 220, series: [[N.power, "Power"]] },
  { id: "freq", title: "Compressor frequency", unit: "Hz", span: 6, height: 200, series: [[N.freq, "Frequency"]] },
  { id: "flow", title: "Water flow", unit: "L/min", span: 6, height: 200, series: [[N.flow, "Flow"]] },
];

const STATES = [[N.valve, "3-way valve"], [N.opMode, "Operation"], [N.dhw, "DHW"], [N.zone1, "Zone 1"], [N.zone2, "Zone 2"], [N.defrost, "Defrost"], [N.backup, "Backup heater"]];

let root, charts = [], span = 86400, end = null, timer = null, custom = [];
try { custom = JSON.parse(localStorage.getItem("history.custom") || "[]"); } catch (e) { custom = []; }

export default {
  title: "History",
  subtitle: () => "Drag across a chart to zoom in",

  mount(el, { topRight }) {
    root = el;
    topRight.innerHTML = `
      <div class="rangebar">
        <button class="icon-btn" id="h-prev" title="Earlier" aria-label="Earlier"><svg viewBox="0 0 24 24"><path d="M15 6l-6 6 6 6" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg></button>
        <span class="span-label" id="h-label"></span>
        <button class="icon-btn" id="h-next" title="Later" aria-label="Later"><svg viewBox="0 0 24 24"><path d="M9 6l6 6-6 6" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg></button>
        <div class="segmented" id="h-ranges">${RANGES.map(([l, s]) => `<button data-s="${s}">${l}</button>`).join("")}</div>
      </div>`;
    topRight.querySelector("#h-ranges").addEventListener("click", (ev) => {
      const b = ev.target.closest("button[data-s]");
      if (!b) return;
      span = +b.dataset.s;
      end = null;
      load();
    });
    topRight.querySelector("#h-prev").addEventListener("click", () => { end = (end ?? now()) - span; load(); });
    topRight.querySelector("#h-next").addEventListener("click", () => {
      end = (end ?? now()) + span;
      if (end >= now() - 60) end = null;
      load();
    });

    const panels = PANELS.filter((p) => p.series.some(([n]) => ent(n)));
    root.innerHTML = `<div class="grid overview-grid">
      ${panels.map((p) => `<section class="card chart-card span-${p.span}">
        <div class="card-head"><h2>${esc(p.title)}</h2><span class="sub">${esc(p.unit)}</span></div>
        <div data-panel="${p.id}"></div></section>`).join("")}
      <section class="card span-12"><div class="card-head"><h2>Operating states</h2></div><div id="h-states"></div></section>
      <section class="card span-12">
        <div class="card-head"><div><h2>Custom chart</h2><div class="sub">Pick any sensors — series are grouped by unit</div></div>
          <select class="input" id="h-pick"><option value="">+ Add sensor…</option></select></div>
        <div class="chips" id="h-chips" style="margin-bottom:12px"></div>
        <div id="h-custom"></div>
      </section>
    </div>`;

    charts = panels.map((p) => {
      const series = p.series.filter(([n]) => ent(n)).map(([n, label], i) => ({ name: n, label, color: SLOTS[i], unit: p.unit, decimals: decimalsFor(p.unit) }));
      return { panel: p, series, chart: lineChart(root.querySelector(`[data-panel="${p.id}"]`), { series, unit: p.unit, height: p.height, onZoom: zoom }) };
    });

    root.querySelector("#h-pick").addEventListener("change", (ev) => {
      const id = ev.target.value;
      if (id && !custom.includes(id)) custom.push(id);
      ev.target.value = "";
      saveCustom();
      loadCustom();
    });
    root.querySelector("#h-chips").addEventListener("click", (ev) => {
      const c = ev.target.closest("[data-rm]");
      if (!c) return;
      custom = custom.filter((id) => id !== c.dataset.rm);
      saveCustom();
      loadCustom();
    });

    load();
    timer = setInterval(() => { if (end == null) load(true); }, 30000);
  },

  unmount() {
    clearInterval(timer);
    charts.forEach((c) => c.chart.destroy());
    customCharts.forEach((c) => c.destroy());
    charts = [];
    customCharts = [];
  },
};

const now = () => Date.now() / 1000;

function zoom(a, b) {
  span = Math.max(300, b - a);
  end = b;
  load();
}

function saveCustom() {
  try { localStorage.setItem("history.custom", JSON.stringify(custom)); } catch (e) { /* ignore */ }
}

function range() {
  const e = end ?? now();
  return [e - span, e];
}

function renderRangeBar() {
  const [s, e] = range();
  const opts = span >= 2 * 86400 ? { month: "short", day: "numeric" } : { month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" };
  document.getElementById("h-label").textContent =
    `${new Date(s * 1000).toLocaleString(undefined, opts)} – ${end == null ? "now" : new Date(e * 1000).toLocaleString(undefined, opts)}`;
  document.querySelectorAll("#h-ranges button").forEach((b) => b.classList.toggle("active", +b.dataset.s === span));
  document.getElementById("h-next").disabled = end == null;
}

async function load(silent = false) {
  renderRangeBar();
  const [s, e] = range();
  const ids = new Set();
  charts.forEach((c) => c.series.forEach((x) => ids.add(ent(x.name).id)));
  STATES.forEach(([n]) => ent(n) && ids.add(ent(n).id));
  const width = root.clientWidth || 1200;
  try {
    const res = await api.history([...ids], s, e, Math.min(1500, Math.round(width / 1.5)));
    for (const c of charts) {
      c.chart.setData(c.series.map((x) => {
        const d = res.series[ent(x.name).id];
        return d ? [d.t, d.v] : [[], []];
      }));
    }
    const rows = STATES.filter(([n]) => ent(n)).map(([n, label]) => {
      const en = ent(n);
      return { label, data: res.series[en.id], order: en.kind === "binary" ? ["ON", "OFF"] : en.options, hide: [] };
    });
    timeline(root.querySelector("#h-states"), rows, s, e);
  } catch (err) {
    console.error(err);
  }
  if (!silent || custom.length) loadCustom();
}

let customCharts = [];

async function loadCustom() {
  const numeric = [...store.entities.values()].filter((x) => x.kind === "numeric" && x.category !== "fsv").sort((a, b) => a.name.localeCompare(b.name));
  const pick = root.querySelector("#h-pick");
  pick.innerHTML = `<option value="">+ Add sensor…</option>` + numeric.filter((x) => !custom.includes(x.id))
    .map((x) => `<option value="${esc(x.id)}">${esc(x.name)}${x.unit ? ` (${esc(x.unit)})` : ""}</option>`).join("");
  const chosen = custom.map((id) => store.entities.get(id)).filter(Boolean);
  root.querySelector("#h-chips").innerHTML = chosen.map((x) => `<button class="chip" data-rm="${esc(x.id)}" title="Remove">${esc(x.name)} ✕</button>`).join("");

  customCharts.forEach((c) => c.destroy());
  customCharts = [];
  const host = root.querySelector("#h-custom");
  host.innerHTML = chosen.length ? "" : `<div class="empty-state">Add sensors to build your own chart</div>`;
  if (!chosen.length) return;

  // one chart per unit, never two y-axes
  const groups = new Map();
  chosen.forEach((x) => {
    const u = x.unit || "value";
    if (!groups.has(u)) groups.set(u, []);
    groups.get(u).push(x);
  });
  const [s, e] = range();
  const res = await api.history(chosen.map((x) => x.id), s, e, 800);
  for (const [unit, list] of groups) {
    const box = document.createElement("div");
    box.style.marginBottom = "18px";
    host.append(box);
    const series = list.map((x, i) => ({ label: x.name, color: SLOTS[i % SLOTS.length], unit: x.unit, decimals: decimalsFor(x.unit) }));
    const c = lineChart(box, { series, unit: unit === "value" ? "" : unit, height: 220, onZoom: zoom, legend: true });
    c.setData(list.map((x) => { const d = res.series[x.id]; return d ? [d.t, d.v] : [[], []]; }));
    customCharts.push(c);
  }
}
