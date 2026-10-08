export const h = (strings, ...values) =>
  strings.reduce((out, s, i) => out + s + (i < values.length ? values[i] : ""), "");

export function esc(s) {
  return String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]);
}

export function decimalsFor(unit, value) {
  if (unit === "°C" || unit === "L/min") return 1;
  if (unit === "W" || unit === "Wh" || unit === "rpm" || unit === "Hz" || unit === "%" || unit === "min") return 0;
  if (value != null && Math.abs(value) < 10 && !Number.isInteger(value)) return 2;
  return Number.isInteger(value) ? 0 : 1;
}

export function num(value, decimals = 1) {
  if (value == null || Number.isNaN(value)) return "–";
  return value.toLocaleString(undefined, { minimumFractionDigits: decimals, maximumFractionDigits: decimals });
}

/** Format an entity's current value (without unit). */
export function entityValue(e, value = e?.value, textValue = e?.text) {
  if (!e) return "–";
  if (e.kind !== "numeric") return esc(textValue ?? "–");
  return num(value, decimalsFor(e.unit, value));
}

export function power(w) {
  if (w == null) return { v: "–", u: "W" };
  if (Math.abs(w) >= 1000) return { v: num(w / 1000, 2), u: "kW" };
  return { v: num(w, 0), u: "W" };
}

export function energy(wh) {
  if (wh == null) return { v: "–", u: "kWh" };
  if (Math.abs(wh) >= 1e6) return { v: num(wh / 1e6, 2), u: "MWh" };
  return { v: num(wh / 1000, wh < 10000 ? 2 : 1), u: "kWh" };
}

export function ago(ts) {
  if (!ts) return "never";
  const s = Math.max(0, Date.now() / 1000 - ts);
  if (s < 10) return "just now";
  if (s < 60) return `${Math.floor(s)}s ago`;
  if (s < 3600) return `${Math.floor(s / 60)} min ago`;
  if (s < 86400) return `${Math.floor(s / 3600)} h ago`;
  return `${Math.floor(s / 86400)} d ago`;
}

export function dateTime(ts, withSeconds = false) {
  return new Date(ts * 1000).toLocaleString(undefined, {
    month: "short", day: "numeric", hour: "2-digit", minute: "2-digit", ...(withSeconds ? { second: "2-digit" } : {}),
  });
}

export function day(ts) {
  return new Date(ts * 1000).toLocaleDateString(undefined, { weekday: "short", month: "short", day: "numeric" });
}

export function prettyName(name) {
  return name.replace(/^FSV \d+\s*/, "");
}

export function toast(message, kind = "") {
  const el = document.createElement("div");
  el.className = `toast ${kind}`;
  el.textContent = message;
  document.getElementById("toasts").append(el);
  setTimeout(() => el.remove(), 3500);
}
