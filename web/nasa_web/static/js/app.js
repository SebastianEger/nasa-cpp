import { connect, store, subscribe } from "./store.js";
import { ago } from "./format.js";
import { close as closeDrawer, refreshDrawer } from "./drawer.js";
import overview from "./views/overview.js";
import history from "./views/history.js";
import energy from "./views/energy.js";
import settings from "./views/settings.js";
import entities from "./views/entities.js";

const views = { overview, history, energy, settings, entities };
const viewEl = document.getElementById("view");
const titleEl = document.getElementById("view-title");
const subEl = document.getElementById("view-sub");
const topRight = document.getElementById("topbar-right");
let current = null;
let currentName = null;

function route() {
  const name = (location.hash.slice(1) || "overview").split("?")[0];
  const view = views[name] || overview;
  if (!store.ready) return;
  if (current && currentName === name) return;
  current?.unmount?.();
  currentName = name;
  current = view;
  document.querySelectorAll(".tabs a").forEach((a) => a.classList.toggle("active", a.dataset.view === name));
  titleEl.textContent = view.title;
  subEl.textContent = view.subtitle?.() ?? "";
  topRight.innerHTML = "";
  // fresh host element per view so listeners never leak between views
  const host = document.createElement("div");
  host.className = "fade-in";
  viewEl.replaceChildren(host);
  view.mount(host, { topRight });
}

function renderConnection() {
  const el = document.getElementById("conn");
  const label = el.querySelector(".label");
  el.classList.remove("ok", "warn", "bad");
  const fresh = store.status.last_message && Date.now() / 1000 - store.status.last_message < 120;
  if (!store.wsConnected) {
    el.classList.add("bad");
    label.textContent = "Server offline";
  } else if (!store.status.mqtt) {
    el.classList.add("bad");
    label.textContent = "MQTT disconnected";
  } else if (!fresh) {
    el.classList.add("warn");
    label.textContent = `No data · ${ago(store.status.last_message)}`;
  } else {
    el.classList.add("ok");
    label.textContent = "Live";
  }
  el.title = `Broker ${store.status.broker || "?"} · last update ${ago(store.status.last_message)}`;
}

subscribe((ev) => {
  if (ev.type === "connection") renderConnection();
  if (ev.type === "snapshot") {
    renderConnection();
    if (!current) route();
    else current.update?.(ev);
  }
  if (ev.type === "state") {
    current?.update?.(ev);
    refreshDrawer(ev.entity);
  }
});

setInterval(renderConnection, 5000);
window.addEventListener("hashchange", () => { closeDrawer(); route(); });

document.getElementById("theme-toggle").addEventListener("click", () => {
  const root = document.documentElement;
  const dark = root.dataset.theme ? root.dataset.theme === "dark" : matchMedia("(prefers-color-scheme: dark)").matches;
  root.dataset.theme = dark ? "light" : "dark";
  try { localStorage.setItem("theme", root.dataset.theme); } catch (e) { /* private mode */ }
  window.dispatchEvent(new Event("themechange"));
});

viewEl.innerHTML = `<div class="grid overview-grid">${'<div class="card span-4 skeleton" style="height:120px"></div>'.repeat(6)}</div>`;
connect();
