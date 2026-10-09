import { api } from "../api.js";
import { lineChart, sparkline, timeline } from "../charts.js";
import { bindControls, confirmState, widget } from "../controls.js";
import { openEntity } from "../drawer.js";
import { ago, energy, entityValue, esc, num, power } from "../format.js";
import { N, ent, heatOutput, liveCop, outletTarget, store, text, val } from "../store.js";

const ICONS = {
  temp: `<svg viewBox="0 0 24 24"><path d="M14 14.8V5a2 2 0 1 0-4 0v9.8a4 4 0 1 0 4 0z"/></svg>`,
  drop: `<svg viewBox="0 0 24 24"><path d="M12 2s7 7.6 7 13a7 7 0 0 1-14 0c0-5.4 7-13 7-13z"/></svg>`,
  bolt: `<svg viewBox="0 0 24 24"><path d="M13 2L4 14h7l-1 8 9-12h-7z"/></svg>`,
  home: `<svg viewBox="0 0 24 24"><path d="M3 11l9-8 9 8v10h-6v-6H9v6H3z"/></svg>`,
  flame: `<svg viewBox="0 0 24 24"><path d="M12 2c1 4 6 6 6 12a6 6 0 0 1-12 0c0-3 2-5 3-6 0 2 1 3 2 3 0-4-1-6 1-9z"/></svg>`,
  leaf: `<svg viewBox="0 0 24 24"><path d="M20 4C9 4 4 9 4 16c0 1 0 2 .3 3C6 13 10 10 15 9c-4 2-7 5-9 11h2c3-8 12-6 12-16z"/></svg>`,
};

const KPIS = [
  { key: N.outdoor, label: "Outdoor", icon: "temp", cls: "cool", color: "--s1" },
  { key: N.waterOut, label: "Flow temp", icon: "flame", cls: "heat", color: "--s2" },
  { key: N.tank, label: "Hot water", icon: "drop", cls: "heat", color: "--s5" },
  { key: N.outlet1, label: "Outlet zone 1", icon: "home", cls: "accent", color: "--s7" },
  { key: N.outlet2, label: "Outlet zone 2", icon: "home", cls: "accent", color: "--s4" },
  { key: N.power, label: "Power", icon: "bolt", cls: "good", color: "--s3" },
];

let root, chart, sparkData = {}, timers = [], raf = 0;

export default {
  title: "Overview",
  subtitle: () => "Live view of your heat pump",

  mount(el, { topRight }) {
    root = el;
    root.innerHTML = `
      <div class="grid overview-grid">
        <section class="span-12 kpis" id="ov-kpis"></section>
        <div class="col span-8">
          <section class="card flow-card">
            <div class="card-head">
              <div><h2>System</h2><div class="sub" id="ov-flow-sub"></div></div>
              <div class="pills" id="ov-pills"></div>
            </div>
            <div class="flow-wrap">${flowSvg()}</div>
            <div class="flow-stats" id="ov-flow-stats"></div>
          </section>
          <section class="card chart-card">
            <div class="card-head"><div><h2>Temperatures</h2><div class="sub">Last 24 hours</div></div>
              <a class="btn" href="#history">Open history
                <svg viewBox="0 0 24 24"><path d="M5 12h14M13 6l6 6-6 6"/></svg></a></div>
            <div id="ov-chart"></div>
          </section>
          <section class="card">
            <div class="card-head"><div><h2>Activity</h2><div class="sub">Last 24 hours</div></div></div>
            <div id="ov-activity"></div>
          </section>
        </div>
        <div class="col span-4">
          <section class="card" id="ov-today"></section>
          <section class="card">
            <div class="card-head"><h2>Controls</h2><span class="sub" id="ov-ro"></span></div>
            <div class="controls" id="ov-controls"></div>
          </section>
        </div>
      </div>`;
    topRight.innerHTML = "";

    root.querySelector("#ov-kpis").addEventListener("click", (ev) => {
      const tile = ev.target.closest("[data-entity]");
      if (tile) openEntity(tile.dataset.entity);
    });
    root.querySelector(".flow-svg").addEventListener("click", (ev) => {
      const t = ev.target.closest("[data-entity]");
      if (t) openEntity(t.dataset.entity);
    });
    bindControls(root.querySelector("#ov-controls"), () => renderControls());

    chart = lineChart(root.querySelector("#ov-chart"), {
      series: [
        { label: "Outdoor", color: "--s1", unit: "°C" },
        { label: "Flow out", color: "--s2", unit: "°C" },
        { label: "Return", color: "--s3", unit: "°C" },
        { label: "DHW tank", color: "--s5", unit: "°C" },
        { label: "Outlet zone 1", color: "--s7", unit: "°C" },
        { label: "Outlet zone 2", color: "--s4", unit: "°C" },
      ],
      unit: "°C",
      height: 250,
    });

    renderAll();
    loadHistory();
    loadToday();
    timers = [setInterval(loadHistory, 5 * 60 * 1000), setInterval(loadToday, 60 * 1000), setInterval(renderFlowSub, 5000)];
  },

  update(ev) {
    if (ev.type === "state") confirmState(ev.entity);
    cancelAnimationFrame(raf);
    raf = requestAnimationFrame(renderAll);
  },

  unmount() {
    timers.forEach(clearInterval);
    chart?.destroy();
    chart = null;
    cancelAnimationFrame(raf);
  },
};

function renderAll() {
  renderFlow();
  renderPills();
  renderKpis();
  renderControls();
  renderFlowSub();
}

function renderFlowSub() {
  const el = root?.querySelector("#ov-flow-sub");
  if (el) el.textContent = `Updated ${ago(store.status.last_message)}`;
}

/* --------------------------------------------------------------- flow svg */

function flowSvg() {
  const id = (n) => esc(ent(n)?.id || "");
  return `
  <svg class="flow-svg" viewBox="0 0 900 320" role="img" aria-label="Heat pump schematic">
    <defs>
      <linearGradient id="tankGrad" x1="0" y1="1" x2="0" y2="0">
        <stop offset="0" stop-color="var(--heat)"/><stop offset="1" stop-color="var(--heat-2)"/>
      </linearGradient>
      <clipPath id="tankClip"><rect x="690" y="182" width="100" height="120" rx="22"/></clipPath>
    </defs>

    <!-- pipes background -->
    <path class="pipe-bg" d="M276,110 H450"/>
    <path class="pipe-bg" d="M490,110 H630"/>
    <path class="pipe-bg" d="M470,130 V222 H690"/>
    <path class="pipe-bg" d="M630,140 H560 V250"/>
    <path class="pipe-bg" d="M690,280 H560 V250"/>
    <path class="pipe-bg" d="M560,250 H276"/>

    <!-- animated pipes -->
    <path class="pipe supply" id="p-main-s" d="M276,110 H450"/>
    <path class="pipe supply" id="p-heat-s" d="M490,110 H630"/>
    <path class="pipe supply" id="p-tank-s" d="M470,130 V222 H690"/>
    <path class="pipe ret" id="p-heat-r" d="M630,140 H560 V250"/>
    <path class="pipe ret" id="p-tank-r" d="M690,280 H560 V250"/>
    <path class="pipe ret" id="p-main-r" d="M560,250 H276"/>

    <!-- outdoor unit -->
    <g data-entity="${id(N.freq)}" style="cursor:pointer">
      <rect class="unit-box" x="26" y="40" width="250" height="240" rx="20"/>
      <g opacity=".5">${[0, 1, 2, 3, 4, 5].map((i) => `<rect x="184" y="${70 + i * 10}" width="72" height="3" rx="1.5" fill="var(--border-strong)"/>`).join("")}</g>
      <circle cx="100" cy="150" r="58" fill="var(--surface)" stroke="var(--border-strong)"/>
      <g class="fan">
        ${[0, 72, 144, 216, 288].map((a) => `<path transform="rotate(${a} 100 150)" d="M100,150 C92,128 92,104 104,96 C114,104 112,128 100,150Z" fill="var(--cool)" opacity=".85"/>`).join("")}
        <circle cx="100" cy="150" r="9" fill="var(--surface-3)" stroke="var(--border-strong)"/>
      </g>
      <text class="lbl" x="44" y="236">Compressor</text>
      <text class="val" x="44" y="262" id="f-freq">–</text>
      <text class="lbl" x="184" y="236">Fan</text>
      <text class="val-sm" x="184" y="260" id="f-fan">–</text>
    </g>
    <g data-entity="${id(N.outdoor)}" style="cursor:pointer">
      <text class="lbl" x="26" y="18">Outside</text>
      <text class="val" x="92" y="20" id="f-outdoor" style="font-size:16px">–</text>
    </g>

    <!-- labels on main pipes -->
    <g data-entity="${id(N.waterOut)}" style="cursor:pointer">
      <text class="lbl" x="300" y="72">Flow out</text>
      <text class="val" x="300" y="96" id="f-out">–</text>
    </g>
    <g data-entity="${id(N.waterIn)}" style="cursor:pointer">
      <text class="lbl" x="300" y="276">Return</text>
      <text class="val" x="300" y="300" id="f-in">–</text>
    </g>
    <g data-entity="${id(N.flow)}" style="cursor:pointer">
      <text class="lbl" x="300" y="172">Flow rate</text>
      <text class="val-sm" x="300" y="192" id="f-flow">–</text>
    </g>

    <!-- 3 way valve -->
    <g data-entity="${id(N.valve)}" style="cursor:pointer">
      <circle cx="470" cy="110" r="20" fill="var(--surface-2)" stroke="var(--border-strong)"/>
      <g class="valve-arm" id="f-valve-arm"><path d="M470,110 L486,110" stroke="var(--heat)" stroke-width="4" stroke-linecap="round"/></g>
      <circle cx="470" cy="110" r="4" fill="var(--text)"/>
      <text class="lbl" x="470" y="74" text-anchor="middle">3-way valve</text>
      <text class="val-sm" x="470" y="160" text-anchor="middle" id="f-valve" style="display:none">–</text>
    </g>

    <!-- house -->
    <g class="target" id="f-house">
      <path d="M630,70 L720,16 L810,70 V160 H630Z" fill="var(--surface-2)" stroke="var(--border-strong)" stroke-linejoin="round"/>
      <text class="lbl" x="720" y="62" text-anchor="middle" id="f-heat-lbl">Heating</text>
      ${[1, 2].map((z) => `
      <g id="f-z${z}" data-entity="${id(z === 1 ? N.outlet1 : N.outlet2)}" style="cursor:pointer">
        <text class="lbl" x="644" y="${z === 1 ? 104 : 140}">Zone ${z}</text>
        <text class="val" x="748" y="${z === 1 ? 105 : 141}" text-anchor="end" style="font-size:17px" id="f-z${z}-v">–</text>
        <text class="val-sm" x="798" y="${z === 1 ? 105 : 141}" text-anchor="end" style="font-size:11px" id="f-z${z}-t"></text>
      </g>`).join("")}
    </g>

    <!-- tank -->
    <g class="target" id="f-tank" data-entity="${id(N.tank)}" style="cursor:pointer">
      <rect x="690" y="182" width="100" height="120" rx="22" fill="var(--surface-2)" stroke="var(--border-strong)"/>
      <g clip-path="url(#tankClip)"><rect class="tank-fill" id="f-tank-fill" x="690" y="302" width="100" height="0" fill="url(#tankGrad)" opacity=".45"/></g>
      <text class="lbl" x="740" y="204" text-anchor="middle" style="fill:var(--text-2)">Hot water</text>
      <text class="val" x="740" y="248" text-anchor="middle" id="f-tank-v">–</text>
      <text class="val-sm" x="740" y="270" text-anchor="middle" id="f-tank-t" style="fill:var(--text)"></text>
    </g>
  </svg>`;
}

function setText(sel, value) {
  const el = root.querySelector(sel);
  if (el && el.textContent !== value) el.textContent = value;
}

function renderFlow() {
  const svg = root.querySelector(".flow-svg");
  if (!svg) return;
  const flow = val(N.flow) ?? 0;
  const freq = val(N.freq) ?? 0;
  const rpm = val(N.fan) ?? 0;
  const valve = text(N.valve);
  const toTank = valve === "TANK";
  const running = flow > 0.5;

  svg.classList.toggle("running", running);
  svg.classList.toggle("fan-on", rpm > 0);
  svg.style.setProperty("--fan-speed", `${Math.max(0.25, 320 / Math.max(rpm, 1))}s`);
  svg.querySelectorAll(".pipe").forEach((p) => {
    p.style.animationDuration = `${Math.max(0.35, Math.min(2.5, 14 / Math.max(flow, 1)))}s`;
  });
  const branch = { "p-heat-s": !toTank, "p-heat-r": !toTank, "p-tank-s": toTank, "p-tank-r": toTank, "p-main-s": true, "p-main-r": true };
  for (const [pid, on] of Object.entries(branch)) svg.querySelector(`#${pid}`).classList.toggle("idle", !on || !running);
  svg.querySelector("#f-valve-arm").style.transform = toTank ? "rotate(90deg)" : "rotate(0deg)";
  svg.querySelector("#f-house").classList.toggle("active", !toTank);
  svg.querySelector("#f-tank").classList.toggle("active", toTank);

  const deg = (v) => (v == null ? "–" : `${num(v, 1)}°`);
  setText("#f-outdoor", val(N.outdoor) == null ? "–" : `${num(val(N.outdoor), 1)} °C`);
  setText("#f-freq", `${num(freq, 0)} Hz`);
  setText("#f-fan", `${num(rpm, 0)} rpm`);
  setText("#f-out", deg(val(N.waterOut)));
  setText("#f-in", deg(val(N.waterIn)));
  setText("#f-flow", `${num(flow, 1)} L/min`);
  const auto = text(N.mode) === "AUTO";
  setText("#f-heat-lbl", auto ? "Heating · water law" : "Heating");
  for (const [z, room, sw] of [[1, N.outlet1, N.zone1], [2, N.outlet2, N.zone2]]) {
    const on = text(sw) !== "OFF";
    const t = outletTarget(z);
    setText(`#f-z${z}-v`, deg(val(room)));
    setText(`#f-z${z}-t`, !on ? "off" : t == null ? "" : `→ ${num(t, 1)}°`);
    svg.querySelector(`#f-z${z}`).style.opacity = !ent(room) ? "0" : on ? "1" : "0.45";
  }
  const tank = val(N.tank), tankT = val(N.dhwTarget);
  setText("#f-tank-v", deg(tank));
  setText("#f-tank-t", tankT == null ? "" : `target ${num(tankT, 0)}°`);
  const level = tank == null ? 0 : Math.max(0.05, Math.min(1, (tank - 20) / 45));
  const fill = svg.querySelector("#f-tank-fill");
  fill.setAttribute("y", String(302 - 120 * level));
  fill.setAttribute("height", String(120 * level));

  // stats strip
  const heat = heatOutput(), pw = val(N.power), cop = liveCop();
  const dt = val(N.waterOut) != null && val(N.waterIn) != null ? val(N.waterOut) - val(N.waterIn) : null;
  const p = power(pw), hq = power(heat);
  root.querySelector("#ov-flow-stats").innerHTML = `
    <div><div class="k">Heat output</div><div class="v">${hq.v}<small>${hq.u}</small></div></div>
    <div><div class="k">Power input</div><div class="v">${p.v}<small>${p.u}</small></div></div>
    <div><div class="k">Live COP</div><div class="v">${cop == null ? "–" : num(cop, 2)}</div></div>
    <div><div class="k">ΔT flow / return</div><div class="v">${dt == null ? "–" : num(dt, 1)}<small>K</small></div></div>`;
}

/* ------------------------------------------------------------------ pills */

function renderPills() {
  const pills = [];
  const err = val(N.error);
  // the heat pump reports -1 or 0 when there is no error, any positive code is an error
  if (ent(N.error) && err != null) {
    pills.push(err > 0 ? `<span class="pill bad"><span class="dot"></span>Error <b>E${num(err, 0)}</b></span>`
      : `<span class="pill good"><span class="dot"></span>No errors</span>`);
  }
  const op = text(N.opMode);
  const OP = { OP_STOP: "Standby", OP_NORMAL: "Running", OP_SAFETY: "Safety", OP_BALANCE: "Balancing", OP_RECOVERY: "Recovery" };
  if (op) pills.push(`<span class="pill ${op === "OP_NORMAL" ? "heat" : ""}"><span class="dot"></span>${esc(OP[op] || op)}</span>`);
  const mode = text(N.mode);
  if (mode) pills.push(`<span class="pill">Mode <b>${esc(mode)}</b></span>`);
  const defrost = text(N.defrost);
  if (defrost && !/NO_DEF/.test(defrost)) pills.push(`<span class="pill cool"><span class="dot"></span>Defrost <b>${esc(defrost.replace("DEFROST_", "").replace("_", " ").toLowerCase())}</b></span>`);
  const backup = text(N.backup);
  if (backup && backup !== "OFF") pills.push(`<span class="pill warn"><span class="dot"></span>Backup heater <b>${esc(backup)}</b></span>`);
  const booster = text(N.booster);
  if (booster && booster !== "OFF") pills.push(`<span class="pill warn"><span class="dot"></span>Booster heater</span>`);
  root.querySelector("#ov-pills").innerHTML = pills.join("");
}

/* ------------------------------------------------------------------- kpis */

function renderKpis() {
  const el = root.querySelector("#ov-kpis");
  const cop = liveCop();
  const tiles = KPIS.map((k) => {
    const e = ent(k.key);
    if (!e) return "";
    let value = entityValue(e), unit = e.unit || "";
    if (e.unit === "W") { const p = power(e.value); value = p.v; unit = p.u; }
    let meta = "";
    const series = sparkData[e.id];
    if (series && series.length) {
      const vals = series.filter((v) => v != null);
      if (vals.length) meta = `24h ${num(Math.min(...vals), e.unit === "W" ? 0 : 1)} – ${num(Math.max(...vals), e.unit === "W" ? 0 : 1)} ${esc(e.unit || "")}`;
    }
    if (k.key === N.tank && val(N.dhwTarget) != null) meta = `Target ${num(val(N.dhwTarget), 0)} °C`;
    return `<div class="card kpi ${k.cls}" data-entity="${esc(e.id)}">
      <div class="label"><i>${ICONS[k.icon]}</i>${esc(k.label)}</div>
      <div class="value">${value}<small>${esc(unit)}</small></div>
      <div class="meta">${meta}</div>
      <div class="spark">${series ? sparkline(series, { color: k.color }) : ""}</div>
    </div>`;
  });
  tiles.push(`<div class="card kpi good" style="cursor:default">
      <div class="label"><i>${ICONS.leaf}</i>Live COP</div>
      <div class="value">${cop == null ? "–" : num(cop, 2)}</div>
      <div class="meta">${cop == null ? "Compressor idle" : `${power(heatOutput()).v} ${power(heatOutput()).u} heat from ${power(val(N.power)).v} ${power(val(N.power)).u}`}</div>
    </div>`);
  el.innerHTML = tiles.join("");
}

/* --------------------------------------------------------------- controls */

const CONTROL_ORDER = [N.mode, N.zone1, N.zone1Target, N.zone2, N.zone2Target, N.dhw, N.dhwMode, N.dhwTarget];

function renderControls() {
  const el = root.querySelector("#ov-controls");
  const controls = [...store.entities.values()].filter((e) => e.category === "control");
  controls.sort((a, b) => {
    const ia = CONTROL_ORDER.indexOf(a.name), ib = CONTROL_ORDER.indexOf(b.name);
    return (ia < 0 ? 99 : ia) - (ib < 0 ? 99 : ib) || a.name.localeCompare(b.name);
  });
  root.querySelector("#ov-ro").textContent = store.status.read_only ? "read-only" : "";
  if (!controls.length) {
    el.innerHTML = `<div class="empty-state">No controls discovered</div>`;
    el.dataset.key = "";
    return;
  }
  const key = controls.map((e) => e.id).join(",") + store.status.read_only;
  if (el.dataset.key !== key) {
    // entity set changed: full rebuild
    el.dataset.key = key;
    el.innerHTML = controls.map((e) => `<div class="control" data-row="${esc(e.id)}">
      <div><div class="name">${esc(e.name)}</div><div class="hint">${e.kind === "numeric" && e.min != null ? `${e.min}–${e.max} ${esc(e.unit || "")}` : ""}</div></div>
      <div class="w"></div></div>`).join("");
  }
  // patch only widgets whose markup changed, so clicks and focus survive live updates
  for (const e of controls) {
    const slot = el.querySelector(`[data-row="${CSS.escape(e.id)}"] .w`);
    const html = widget(e);
    if (slot && slot._html !== html) {
      slot.innerHTML = html;
      slot._html = html;
    }
  }
}

/* ----------------------------------------------------------------- history */

async function loadHistory() {
  const keys = [N.outdoor, N.waterOut, N.waterIn, N.tank, N.outlet1, N.outlet2];
  const states = [[N.valve, "3-way valve"], [N.opMode, "Compressor"], [N.zone1, "Zone 1"], [N.zone2, "Zone 2"], [N.defrost, "Defrost"]];
  const ids = [...new Set([...keys, N.power, ...states.map((x) => x[0])].map((k) => ent(k)?.id).filter(Boolean))];
  if (!ids.length) return;
  const end = Date.now() / 1000, start = end - 86400;
  try {
    const res = await api.history(ids, start, end, 300);
    if (!chart) return;
    chart.setData(keys.map((k) => {
      const s = res.series[ent(k)?.id];
      return s ? [s.t, s.v] : [[], []];
    }));
    const rows = states.filter(([n]) => res.series[ent(n)?.id]).map(([n, label]) => ({ label, data: res.series[ent(n).id], order: ent(n).kind === "binary" ? ["ON", "OFF"] : ent(n).options }));
    timeline(root.querySelector("#ov-activity"), rows, start, end);
    sparkData = {};
    for (const k of KPIS) {
      const s = res.series[ent(k.key)?.id];
      if (s) sparkData[ent(k.key).id] = resample(s, start, end, 60);
    }
    renderKpis();
  } catch (err) {
    console.error(err);
  }
}

function resample(s, start, end, n) {
  const out = [];
  let j = 0;
  for (let i = 0; i < n; i++) {
    const t = start + ((end - start) * i) / (n - 1);
    while (j + 1 < s.t.length && s.t[j + 1] <= t) j++;
    out.push(s.t[j] <= t ? s.v[j] : null);
  }
  return out;
}

/* -------------------------------------------------------------------- today */

async function loadToday() {
  const el = root?.querySelector("#ov-today");
  if (!el) return;
  const eIn = ent(N.energyIn), eOut = ent(N.energyOut);
  if (!eIn || !eOut) {
    el.innerHTML = `<div class="card-head"><h2>Today</h2></div><div class="empty-state">No energy counters discovered</div>`;
    return;
  }
  const midnight = new Date(); midnight.setHours(0, 0, 0, 0);
  const now = Date.now() / 1000;
  const res = await api.valuesAt([eIn.id, eOut.id], [midnight / 1000, now]);
  const [i0, i1] = res.values[eIn.id], [o0, o1] = res.values[eOut.id];
  const used = i0 != null && i1 != null ? Math.max(0, i1 - i0) : null;
  const made = o0 != null && o1 != null ? Math.max(0, o1 - o0) : null;
  const cop = used && made != null ? made / used : null;
  const u = energy(used), m = energy(made);
  const pct = cop == null ? 0 : Math.min(1, cop / 6);
  const r = 46, c = 2 * Math.PI * r;
  el.innerHTML = `
    <div class="card-head"><div><h2>Today</h2><div class="sub">Since midnight</div></div>
      <a class="btn" href="#energy">Energy<svg viewBox="0 0 24 24"><path d="M5 12h14M13 6l6 6-6 6"/></svg></a></div>
    <div style="display:flex;align-items:center;gap:20px;flex-wrap:wrap">
      <svg class="cop-ring" viewBox="0 0 120 120" role="img" aria-label="COP today">
        <circle cx="60" cy="60" r="${r}" fill="none" stroke="var(--surface-3)" stroke-width="10"/>
        <circle cx="60" cy="60" r="${r}" fill="none" stroke="var(--good)" stroke-width="10" stroke-linecap="round"
          stroke-dasharray="${c * pct} ${c}" transform="rotate(-90 60 60)" style="transition:stroke-dasharray .8s"/>
        <text x="60" y="62" text-anchor="middle" font-size="26" font-weight="700" fill="var(--text)">${cop == null ? "–" : num(cop, 1)}</text>
        <text x="60" y="82" text-anchor="middle" font-size="10" font-weight="600" fill="var(--text-3)" letter-spacing=".08em">COP</text>
      </svg>
      <div style="display:flex;flex-direction:column;gap:14px;flex:1;min-width:140px">
        <div><div class="faint" style="font-size:12px;font-weight:600">HEAT PRODUCED</div>
          <div class="big-number" style="font-size:26px;color:var(--text)">${m.v}<small>${m.u}</small></div></div>
        <div><div class="faint" style="font-size:12px;font-weight:600">ELECTRICITY USED</div>
          <div class="big-number" style="font-size:26px">${u.v}<small>${u.u}</small></div></div>
      </div>
    </div>
    <p class="faint" style="font-size:12px;margin-top:14px">${made != null && used != null ? `Free ambient energy today: <b style="color:var(--text)">${energy(Math.max(0, made - used)).v} ${energy(Math.max(0, made - used)).u}</b>` : ""}</p>`;
}
