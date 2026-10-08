// Chart helpers: uPlot time-series charts, SVG bar charts and sparklines.
import { esc, num } from "./format.js";

export const SLOTS = ["--s1", "--s2", "--s3", "--s4", "--s5", "--s6", "--s7", "--s8"];

export function css(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

const charts = new Set();
window.addEventListener("themechange", () => charts.forEach((c) => c.rebuild()));

/** Insert nulls into gaps so lines don't bridge periods without data. */
function withGaps(t, v, minGap) {
  if (t.length < 3) return [t, v];
  const steps = [];
  for (let i = 1; i < t.length; i++) steps.push(t[i] - t[i - 1]);
  const sorted = [...steps].sort((a, b) => a - b);
  const median = sorted[Math.floor(sorted.length / 2)] || 60;
  const limit = Math.max(minGap, median * 4);
  const ot = [t[0]], ov = [v[0]];
  for (let i = 1; i < t.length; i++) {
    if (t[i] - t[i - 1] > limit) {
      ot.push(t[i - 1] + 1);
      ov.push(null);
    }
    ot.push(t[i]);
    ov.push(v[i]);
  }
  return [ot, ov];
}

function tooltipPlugin(getSeries, fmtTime) {
  let tip;
  return {
    hooks: {
      init: (u) => {
        tip = document.createElement("div");
        tip.className = "tooltip";
        tip.hidden = true;
        u.over.appendChild(tip);
        u.over.addEventListener("mouseleave", () => { tip.hidden = true; });
      },
      setCursor: (u) => {
        const { left, top, idx } = u.cursor;
        if (idx == null || left < 0) { tip.hidden = true; return; }
        const series = getSeries();
        let rows = "";
        series.forEach((s, i) => {
          if (!u.series[i + 1].show) return;
          const v = u.data[i + 1][idx];
          const shown = v == null ? "–" : s.format ? s.format(v) : `${num(v, s.decimals ?? 1)} ${esc(s.unit || "")}`;
          rows += `<div class="r"><span><i style="background:${css(s.color)}"></i>${esc(s.label)}</span><b>${shown}</b></div>`;
        });
        tip.innerHTML = `<div class="t">${fmtTime(u.data[0][idx])}</div>${rows}`;
        tip.hidden = false;
        const flip = left > u.over.clientWidth * 0.62;
        tip.style.left = `${left}px`;
        tip.style.top = `${Math.max(30, Math.min(top, u.over.clientHeight - 30))}px`;
        tip.style.transform = flip ? "translate(calc(-100% - 12px), -50%)" : "translate(12px, -50%)";
      },
    },
  };
}

/**
 * Time series line chart.
 * series: [{label, color: '--s1', unit, decimals, stepped, format}]
 * One unit per chart (never a second y-axis).
 */
export function lineChart(el, { series, height = 240, unit = "", fill = series.length === 1, yRange, legend = series.length > 1, onZoom } = {}) {
  el.innerHTML = "";
  el.classList.add("chart");
  const legendEl = document.createElement("div");
  legendEl.className = "legend";
  const plotEl = document.createElement("div");
  if (legend) el.append(legendEl);
  el.append(plotEl);

  let u = null;
  let data = null;
  const hidden = new Set();

  const fmtTime = (ts) => new Date(ts * 1000).toLocaleString(undefined, { weekday: "short", month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" });

  function renderLegend() {
    legendEl.innerHTML = series.map((s, i) =>
      `<span data-i="${i}" class="${hidden.has(i) ? "off" : ""}"><i style="background:${css(s.color)}"></i>${esc(s.label)}</span>`).join("");
  }
  legendEl.addEventListener("click", (ev) => {
    const span = ev.target.closest("span[data-i]");
    if (!span) return;
    const i = +span.dataset.i;
    hidden.has(i) ? hidden.delete(i) : hidden.add(i);
    u?.setSeries(i + 1, { show: !hidden.has(i) });
    renderLegend();
  });

  function build() {
    u?.destroy();
    plotEl.innerHTML = "";
    if (!data || data[0].length === 0) {
      plotEl.innerHTML = `<div class="empty" style="position:relative;height:${height}px">No data in this time range</div>`;
      u = null;
      return;
    }
    const axis = { stroke: css("--axis"), grid: { stroke: css("--grid"), width: 1 }, ticks: { show: false }, font: `11px ${css("--font")}` };
    const opts = {
      width: Math.max(200, plotEl.clientWidth || el.clientWidth),
      height,
      padding: [8, 4, 0, 0],
      cursor: { drag: { x: !!onZoom, y: false }, points: { size: 8, width: 2, stroke: css("--surface") } },
      select: { show: !!onZoom },
      legend: { show: false },
      scales: { x: { time: true }, y: yRange ? { range: yRange } : { auto: true } },
      axes: [
        { ...axis, space: 70 },
        { ...axis, size: 54, values: (_u, vals) => {
          const stepSize = vals.length > 1 ? Math.abs(vals[1] - vals[0]) : 1;
          const d = stepSize >= 1 ? 0 : stepSize >= 0.1 ? 1 : 2;
          return vals.map((v) => `${num(v, d)}${unit && unit.length <= 3 ? " " + unit : ""}`);
        } },
      ],
      series: [
        {},
        ...series.map((s, i) => ({
          label: s.label,
          stroke: css(s.color),
          width: 2,
          show: !hidden.has(i),
          spanGaps: false,
          fillTo: (self) => self.scales.y.min,
          points: { show: false },
          paths: s.stepped ? uPlot.paths.stepped({ align: 1 }) : undefined,
          fill: fill ? (self) => {
            const ctx = self.ctx, { top, height: h } = self.bbox;
            const g = ctx.createLinearGradient(0, top, 0, top + h);
            g.addColorStop(0, css(s.color) + "40");
            g.addColorStop(1, css(s.color) + "00");
            return g;
          } : undefined,
        })),
      ],
      hooks: onZoom ? {
        setSelect: [(self) => {
          if (self.select.width < 5) return;
          const a = self.posToVal(self.select.left, "x");
          const b = self.posToVal(self.select.left + self.select.width, "x");
          self.setSelect({ width: 0, height: 0 }, false);
          onZoom(a, b);
        }],
      } : {},
      plugins: [tooltipPlugin(() => series, fmtTime)],
    };
    u = new uPlot(opts, data, plotEl);
  }

  const ro = new ResizeObserver(() => {
    if (u) u.setSize({ width: plotEl.clientWidth, height });
  });
  ro.observe(plotEl);

  const chart = {
    /** tables: [[t[], v[]], ...] one per series */
    setData(tables) {
      const prepared = tables.map(([t, v]) => withGaps(t, v, 20 * 60));
      const nonEmpty = prepared.some(([t]) => t.length);
      data = nonEmpty ? uPlot.join(prepared, prepared.map(() => [1])) : null;
      if (legend) renderLegend();
      build();
    },
    rebuild() {
      if (legend) renderLegend();
      build();
    },
    destroy() {
      ro.disconnect();
      u?.destroy();
      charts.delete(chart);
    },
  };
  charts.add(chart);
  return chart;
}

/**
 * Grouped SVG bar chart.
 * groups: [{label, sub?, values: [..per series]}], series: [{label, color, unit}]
 */
export function barChart(el, { groups, series, height = 240, format = (v) => num(v, 1), unitLabel = "" }) {
  el.innerHTML = "";
  el.classList.add("chart");
  const legend = series.length > 1
    ? `<div class="legend">${series.map((s) => `<span><i style="background:var(${s.color});height:8px;width:8px;border-radius:2px"></i>${esc(s.label)}</span>`).join("")}</div>`
    : "";
  const width = Math.max(300, el.clientWidth || 600);
  const pad = { l: 52, r: 4, t: 8, b: 26 };
  const innerW = width - pad.l - pad.r, innerH = height - pad.t - pad.b;
  const max = Math.max(0.001, ...groups.flatMap((g) => g.values.map((v) => v ?? 0)));
  const nice = niceMax(max);
  const ticks = [0, 0.25, 0.5, 0.75, 1].map((f) => f * nice);
  const gw = innerW / Math.max(1, groups.length);
  const bw = Math.max(3, Math.min(28, (gw * 0.72 - (series.length - 1) * 2) / series.length));
  const y = (v) => pad.t + innerH - (v / nice) * innerH;
  const labelEvery = Math.ceil(groups.length / Math.max(1, Math.floor(innerW / 54)));

  let svg = `<svg width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" role="img" aria-label="bar chart">`;
  ticks.forEach((t) => {
    svg += `<line x1="${pad.l}" x2="${width - pad.r}" y1="${y(t)}" y2="${y(t)}" stroke="var(--grid)"/>`;
    svg += `<text x="${pad.l - 8}" y="${y(t) + 4}" text-anchor="end" font-size="11" fill="var(--axis)">${format(t)}${unitLabel ? " " + unitLabel : ""}</text>`;
  });
  groups.forEach((g, gi) => {
    const gx = pad.l + gi * gw + (gw - (bw * series.length + 2 * (series.length - 1))) / 2;
    svg += `<g class="bar-group" data-i="${gi}"><rect x="${pad.l + gi * gw}" y="${pad.t}" width="${gw}" height="${innerH}" fill="transparent"/>`;
    g.values.forEach((v, si) => {
      if (v == null || v <= 0) return;
      const x = gx + si * (bw + 2), top = y(v), hgt = Math.max(1, pad.t + innerH - top);
      const r = Math.min(4, bw / 2, hgt);
      svg += `<path d="M${x},${top + hgt} V${top + r} q0,-${r} ${r},-${r} H${x + bw - r} q${r},0 ${r},${r} V${top + hgt} Z" fill="var(${series[si].color})"/>`;
    });
    if (gi % labelEvery === 0) {
      svg += `<text x="${pad.l + gi * gw + gw / 2}" y="${height - 8}" text-anchor="middle" font-size="11" fill="var(--axis)">${esc(g.label)}</text>`;
    }
    svg += `</g>`;
  });
  svg += `</svg>`;
  el.innerHTML = `${legend}<div style="position:relative">${svg}<div class="tooltip" hidden></div></div>`;

  const tip = el.querySelector(".tooltip");
  const wrap = tip.parentElement;
  el.querySelectorAll(".bar-group").forEach((node) => {
    node.addEventListener("mousemove", (ev) => {
      const g = groups[+node.dataset.i];
      const rows = series.map((s, si) =>
        `<div class="r"><span><i style="background:var(${s.color})"></i>${esc(s.label)}</span><b>${g.values[si] == null ? "–" : format(g.values[si])} ${esc(unitLabel)}</b></div>`).join("");
      tip.innerHTML = `<div class="t">${esc(g.title || g.label)}</div>${rows}${g.extra || ""}`;
      tip.hidden = false;
      const rect = wrap.getBoundingClientRect();
      const x = ev.clientX - rect.left;
      tip.style.left = `${x}px`;
      tip.style.top = `${ev.clientY - rect.top}px`;
      tip.style.transform = x > rect.width * 0.6 ? "translate(calc(-100% - 12px), -50%)" : "translate(12px, -50%)";
    });
    node.addEventListener("mouseleave", () => { tip.hidden = true; });
  });
}

function niceMax(v) {
  const exp = Math.pow(10, Math.floor(Math.log10(v)));
  const f = v / exp;
  const nf = f <= 1 ? 1 : f <= 2 ? 2 : f <= 2.5 ? 2.5 : f <= 5 ? 5 : 10;
  return nf * exp;
}

/** Tiny sparkline as an SVG string. */
export function sparkline(values, { width = 200, height = 42, color = "--s1", area = true } = {}) {
  const pts = values.filter((v) => v != null);
  if (pts.length < 2) return "";
  let min = Math.min(...pts), max = Math.max(...pts);
  if (max - min < 1e-9) { max += 1; min -= 1; }
  const step = width / (values.length - 1);
  let d = "", started = false;
  values.forEach((v, i) => {
    if (v == null) { started = false; return; }
    const x = i * step, yv = height - 3 - ((v - min) / (max - min)) * (height - 8);
    d += `${started ? "L" : "M"}${x.toFixed(1)},${yv.toFixed(1)}`;
    started = true;
  });
  const id = `sg${Math.random().toString(36).slice(2, 8)}`;
  return `<svg viewBox="0 0 ${width} ${height}" preserveAspectRatio="none" width="100%" height="100%">
    <defs><linearGradient id="${id}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="var(${color})" stop-opacity=".28"/><stop offset="1" stop-color="var(${color})" stop-opacity="0"/></linearGradient></defs>
    ${area ? `<path d="${d}L${width},${height}L0,${height}Z" fill="url(#${id})"/>` : ""}
    <path d="${d}" fill="none" stroke="var(${color})" stroke-width="1.6" vector-effect="non-scaling-stroke"/>
  </svg>`;
}

/** Horizontal state timeline rows for binary / enum entities. */
export function timeline(el, rows, start, end) {
  const palette = SLOTS;
  const span = end - start;
  let html = `<div class="timeline">`;
  for (const row of rows) {
    const { t, text } = row.data;
    const colors = new Map();
    (row.order || []).forEach((s, i) => colors.set(s, palette[i % palette.length]));
    // merge consecutive samples with the same state into one segment
    const segs = [];
    for (let i = 0; i < t.length; i++) {
      const state = text[i] ?? "–";
      const a = Math.max(start, t[i]);
      const b = Math.min(end, i + 1 < t.length ? t[i + 1] : Math.min(end, Date.now() / 1000));
      if (b <= a) continue;
      const last = segs[segs.length - 1];
      if (last && last.state === state && Math.abs(last.b - a) < 1) last.b = b;
      else segs.push({ state, a, b });
    }
    const seen = new Set(segs.map((x) => x.state));
    for (const st of seen) if (!colors.has(st)) colors.set(st, palette[colors.size % palette.length]);
    let bar = "";
    for (const { state, a, b } of segs) {
      if (row.hide && row.hide.includes(state)) continue;
      const left = ((a - start) / span) * 100, width = ((b - a) / span) * 100;
      const title = `${state} · ${new Date(a * 1000).toLocaleString()} → ${new Date(b * 1000).toLocaleString()}`;
      bar += `<div class="tl-seg" title="${esc(title)}" style="left:${left}%;width:${width}%;background:var(${colors.get(state)})"></div>`;
    }
    const legend = [...colors.entries()].filter(([s]) => seen.has(s) && !(row.hide || []).includes(s))
      .map(([s, c]) => `<span><i style="background:var(${c})"></i>${esc(s)}</span>`).join("");
    html += `<div class="tl-row"><div class="tl-name" title="${esc(row.label)}">${esc(row.label)}</div><div><div class="tl-bar">${bar}</div><div class="tl-legend">${legend}</div></div></div>`;
  }
  const fmt = (ts) => new Date(ts * 1000).toLocaleString(undefined, span > 2 * 86400 ? { month: "short", day: "numeric" } : { hour: "2-digit", minute: "2-digit" });
  html += `<div class="tl-axis"><span></span><div>${[0, 0.25, 0.5, 0.75, 1].map((f) => `<span>${fmt(start + f * span)}</span>`).join("")}</div></div></div>`;
  el.innerHTML = html;
}
