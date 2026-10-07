import { openEntity } from "../drawer.js";
import { ago, entityValue, esc } from "../format.js";
import { store } from "../store.js";

const FILTERS = [["all", "All"], ["sensor", "Sensors"], ["control", "Controls"], ["fsv", "FSV"]];
let root, filter = "all", query = "", raf = 0;

export default {
  title: "Entities",
  subtitle: () => "Everything discovered from the MQTT bridge",

  mount(el, { topRight }) {
    root = el;
    topRight.innerHTML = `<input class="search" id="en-search" placeholder="Search entities…" value="${esc(query)}">`;
    topRight.querySelector("#en-search").addEventListener("input", (ev) => { query = ev.target.value.toLowerCase(); render(); });
    root.innerHTML = `<div class="chips" id="en-filters" style="margin-bottom:16px"></div><section class="card" style="padding:6px 8px"><div class="table-wrap" id="en-table"></div></section>`;
    root.querySelector("#en-filters").addEventListener("click", (ev) => {
      const c = ev.target.closest("[data-f]");
      if (c) { filter = c.dataset.f; render(); }
    });
    root.querySelector("#en-table").addEventListener("click", (ev) => {
      const tr = ev.target.closest("tr[data-entity]");
      if (tr) openEntity(tr.dataset.entity);
    });
    render();
  },

  update() {
    cancelAnimationFrame(raf);
    raf = requestAnimationFrame(render);
  },

  unmount() { cancelAnimationFrame(raf); },
};

function render() {
  const all = [...store.entities.values()];
  const counts = { all: all.length };
  all.forEach((e) => { counts[e.category] = (counts[e.category] || 0) + 1; });
  root.querySelector("#en-filters").innerHTML = FILTERS.map(([k, l]) =>
    `<button class="chip ${filter === k ? "active" : ""}" data-f="${k}">${l}<span class="count">${counts[k] || 0}</span></button>`).join("");

  const list = all.filter((e) => (filter === "all" || e.category === filter) && (!query || e.name.toLowerCase().includes(query) || e.id.includes(query)))
    .sort((a, b) => a.category.localeCompare(b.category) || a.name.localeCompare(b.name, undefined, { numeric: true }));

  root.querySelector("#en-table").innerHTML = list.length ? `<table>
    <thead><tr><th>Name</th><th class="hide-sm">Type</th><th class="num">Value</th><th class="hide-sm">Updated</th></tr></thead>
    <tbody>${list.map((e) => `<tr class="clickable" data-entity="${esc(e.id)}">
      <td><div style="font-weight:550">${esc(e.name)}</div><div class="mono">${esc(e.id)}</div></td>
      <td class="hide-sm"><span class="badge ${e.category}">${esc(e.platform)}</span></td>
      <td class="num"><b>${entityValue(e)}</b> <span class="faint">${e.kind === "numeric" ? esc(e.unit || "") : ""}</span></td>
      <td class="hide-sm faint">${ago(e.updated)}</td></tr>`).join("")}</tbody></table>`
    : `<div class="empty-state"><strong>Nothing found</strong>Try a different search or filter.</div>`;
}
