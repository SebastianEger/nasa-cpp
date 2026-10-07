import { api } from "../api.js";
import { barChart } from "../charts.js";
import { energy, esc, num } from "../format.js";
import { N, ent } from "../store.js";

const MODES = [["7 days", "d7"], ["30 days", "d30"], ["12 months", "m12"]];
let root, mode = "d30", timer;

export default {
  title: "Energy",
  subtitle: () => "Electricity used, heat produced and efficiency",

  mount(el, { topRight }) {
    root = el;
    topRight.innerHTML = `<div class="segmented" id="e-modes">${MODES.map(([l, m]) => `<button data-m="${m}" class="${m === mode ? "active" : ""}">${l}</button>`).join("")}</div>`;
    topRight.querySelector("#e-modes").addEventListener("click", (ev) => {
      const b = ev.target.closest("button[data-m]");
      if (!b) return;
      mode = b.dataset.m;
      topRight.querySelectorAll("button").forEach((x) => x.classList.toggle("active", x === b));
      load();
    });
    if (!ent(N.energyIn) || !ent(N.energyOut)) {
      root.innerHTML = `<div class="card empty-state"><strong>No energy counters</strong>“${N.energyIn}” and “${N.energyOut}” were not discovered.</div>`;
      return;
    }
    root.innerHTML = `<div class="grid overview-grid">
      <section class="span-12 kpis" id="e-kpis"></section>
      <section class="card span-12"><div class="card-head"><h2>Electricity vs. heat</h2><span class="sub">kWh</span></div><div id="e-bars"></div></section>
      <section class="card span-6"><div class="card-head"><h2>Coefficient of performance</h2><span class="sub">heat ÷ electricity</span></div><div id="e-cop"></div></section>
      <section class="card span-6"><div class="card-head"><h2>Details</h2></div><div class="table-wrap" id="e-table" style="max-height:300px;overflow-y:auto"></div></section>
    </div>`;
    load();
    timer = setInterval(load, 5 * 60 * 1000);
  },

  unmount() { clearInterval(timer); },
};

function boundaries() {
  const out = [];
  const d = new Date();
  if (mode === "m12") {
    d.setHours(0, 0, 0, 0); d.setDate(1);
    d.setMonth(d.getMonth() - 11);
    for (let i = 0; i < 12; i++) { out.push(new Date(d)); d.setMonth(d.getMonth() + 1); }
  } else {
    const days = mode === "d7" ? 7 : 30;
    d.setHours(0, 0, 0, 0);
    d.setDate(d.getDate() - (days - 1));
    for (let i = 0; i < days; i++) { out.push(new Date(d)); d.setDate(d.getDate() + 1); }
  }
  return out;
}

async function load() {
  const starts = boundaries();
  const ts = [...starts.map((d) => d / 1000), Date.now() / 1000];
  const eIn = ent(N.energyIn), eOut = ent(N.energyOut);
  const res = await api.valuesAt([eIn.id, eOut.id], ts);
  const vi = res.values[eIn.id], vo = res.values[eOut.id];
  const delta = (arr, i) => (arr[i] != null && arr[i + 1] != null ? Math.max(0, arr[i + 1] - arr[i]) : null);

  const monthly = mode === "m12";
  const rows = starts.map((d, i) => {
    const used = delta(vi, i), made = delta(vo, i);
    return {
      label: monthly ? d.toLocaleDateString(undefined, { month: "short" }) : d.toLocaleDateString(undefined, { day: "numeric", month: "numeric" }),
      title: monthly ? d.toLocaleDateString(undefined, { month: "long", year: "numeric" }) : d.toLocaleDateString(undefined, { weekday: "long", month: "long", day: "numeric" }),
      used, made, cop: used ? made / used : null,
    };
  });

  const totalUsed = rows.reduce((a, r) => a + (r.used || 0), 0);
  const totalMade = rows.reduce((a, r) => a + (r.made || 0), 0);
  const cop = totalUsed ? totalMade / totalUsed : null;
  const withData = Math.max(1, rows.filter((r) => r.used != null).length);
  const best = rows.filter((r) => r.cop != null && r.used > 100).sort((a, b) => b.cop - a.cop)[0];
  const tile = (label, v, cls, meta = "") => `<div class="card kpi ${cls}" style="cursor:default"><div class="label">${label}</div><div class="value">${v}</div><div class="meta">${meta}</div></div>`;
  const u = energy(totalUsed), m = energy(totalMade), free = energy(Math.max(0, totalMade - totalUsed));
  root.querySelector("#e-kpis").innerHTML =
    tile("Electricity used", `${u.v}<small>${u.u}</small>`, "", `Ø ${energy(totalUsed / withData).v} ${energy(totalUsed / withData).u} per ${monthly ? "month" : "day"}`) +
    tile("Heat produced", `${m.v}<small>${m.u}</small>`, "heat", `Ø ${energy(totalMade / withData).v} ${energy(totalMade / withData).u} per ${monthly ? "month" : "day"}`) +
    tile("Average COP", cop == null ? "–" : num(cop, 2), "good", best ? `Best: ${num(best.cop, 2)} on ${esc(best.title)}` : "") +
    tile("Ambient energy gained", `${free.v}<small>${free.u}</small>`, "cool", "Heat drawn from outside air");

  barChart(root.querySelector("#e-bars"), {
    groups: rows.map((r) => ({ label: r.label, title: r.title, values: [r.used == null ? null : r.used / 1000, r.made == null ? null : r.made / 1000],
      extra: r.cop != null ? `<div class="r"><span>COP</span><b>${num(r.cop, 2)}</b></div>` : "" })),
    series: [{ label: "Electricity used", color: "--s1" }, { label: "Heat produced", color: "--s2" }],
    height: 280,
    format: (v) => num(v, v < 10 ? 1 : 0),
    unitLabel: "kWh",
  });
  barChart(root.querySelector("#e-cop"), {
    groups: rows.map((r) => ({ label: r.label, title: r.title, values: [r.cop] })),
    series: [{ label: "COP", color: "--s3" }],
    height: 240,
    format: (v) => num(v, 1),
  });
  root.querySelector("#e-table").innerHTML = `<table><thead><tr><th>${monthly ? "Month" : "Day"}</th><th class="num">Electricity</th><th class="num">Heat</th><th class="num">COP</th></tr></thead><tbody>
    ${[...rows].reverse().map((r) => `<tr><td>${esc(r.title)}</td><td class="num">${r.used == null ? "–" : num(r.used / 1000, 2)}</td><td class="num">${r.made == null ? "–" : num(r.made / 1000, 2)}</td><td class="num">${r.cop == null ? "–" : num(r.cop, 2)}</td></tr>`).join("")}
  </tbody></table>`;
}
