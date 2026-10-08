// Interactive widgets for writable entities (switch / select / number).
import { api } from "./api.js";
import { esc, num, toast, decimalsFor } from "./format.js";
import { store } from "./store.js";

const pendingNumbers = new Map(); // entity id -> { value, timer }

function disabledAttr(e) {
  return !e.writable || store.status.read_only ? "disabled" : "";
}

export function widget(e) {
  if (!e) return "";
  const dis = disabledAttr(e);
  if (e.kind === "binary") {
    return `<label class="switch"><input type="checkbox" data-ctl="${esc(e.id)}" ${e.value ? "checked" : ""} ${dis} aria-label="${esc(e.name)}"><span></span></label>`;
  }
  if (e.kind === "enum") {
    return `<div class="segmented" role="radiogroup" aria-label="${esc(e.name)}">${e.options.map((o) =>
      `<button data-ctl="${esc(e.id)}" data-opt="${esc(o)}" class="${e.text === o ? "active" : ""}" ${dis}>${esc(o)}</button>`).join("")}</div>`;
  }
  const pending = pendingNumbers.get(e.id);
  const v = pending ? pending.value : e.value;
  const step = e.step || 1;
  const decimals = Math.max(decimalsFor(e.unit, v), step % 1 ? 1 : 0);
  return `<div class="stepper ${pending ? "pending" : ""}" data-stepper="${esc(e.id)}">
    <button data-ctl="${esc(e.id)}" data-step="-1" ${dis || (e.min != null && v <= e.min ? "disabled" : "")} aria-label="Decrease">−</button>
    <output>${num(v, decimals)}<small>${esc(e.unit || "")}</small></output>
    <button data-ctl="${esc(e.id)}" data-step="1" ${dis || (e.max != null && v >= e.max ? "disabled" : "")} aria-label="Increase">+</button>
  </div>`;
}

async function send(e, value) {
  try {
    await api.command(e.id, value);
    toast(`${e.name} → ${typeof value === "number" ? num(value, e.step % 1 ? 1 : 0) + (e.unit ? " " + e.unit : "") : value}`, "ok");
  } catch (err) {
    toast(`${e.name}: ${err.message}`, "err");
    return false;
  }
  return true;
}

/** Attach delegated listeners to a container; `rerender(entity)` refreshes a single widget. */
export function bindControls(root, rerender) {
  root.addEventListener("change", async (ev) => {
    const input = ev.target.closest("input[data-ctl]");
    if (!input) return;
    const e = store.entities.get(input.dataset.ctl);
    if (!e) return;
    const ok = await send(e, input.checked ? "ON" : "OFF");
    if (!ok) input.checked = !input.checked;
  });

  root.addEventListener("click", async (ev) => {
    const btn = ev.target.closest("button[data-ctl]");
    if (!btn || btn.disabled) return;
    const e = store.entities.get(btn.dataset.ctl);
    if (!e) return;

    if (btn.dataset.opt != null) {
      btn.parentElement.querySelectorAll("button").forEach((b) => b.classList.toggle("active", b === btn));
      if (!(await send(e, btn.dataset.opt))) rerender(e);
      return;
    }

    // number stepper: debounce so several clicks send a single command
    const step = (e.step || 1) * +btn.dataset.step;
    const current = pendingNumbers.get(e.id)?.value ?? e.value ?? e.min ?? 0;
    let next = Math.round((current + step) * 100) / 100;
    if (e.min != null) next = Math.max(e.min, next);
    if (e.max != null) next = Math.min(e.max, next);
    const prev = pendingNumbers.get(e.id);
    if (prev) clearTimeout(prev.timer);
    const timer = setTimeout(async () => {
      await send(e, next);
      // keep showing the pending value until the bridge confirms it (or 15 s pass)
      setTimeout(() => { if (pendingNumbers.get(e.id)?.timer === timer) { pendingNumbers.delete(e.id); rerender(e); } }, 15000);
    }, 700);
    pendingNumbers.set(e.id, { value: next, timer });
    rerender(e);
  });
}

/** Called on state updates: drop the pending marker once the value arrived. */
export function confirmState(e) {
  const p = pendingNumbers.get(e.id);
  if (p && e.value != null && Math.abs(e.value - p.value) < 1e-6) pendingNumbers.delete(e.id);
}
