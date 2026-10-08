import { bindControls, confirmState, widget } from "../controls.js";
import { openEntity } from "../drawer.js";
import { entityValue, esc } from "../format.js";
import { store } from "../store.js";

// FSV code groups as documented in the Samsung installation manual
const GROUPS = [
  ["10", "Temperature limits"],
  ["20", "Water law & heating"],
  ["30", "Domestic hot water"],
  ["40", "Heating system"],
  ["50", "Other functions"],
  ["60", "Miscellaneous"],
];

let root, query = "", editing = false;

export default {
  title: "Settings",
  subtitle: () => "Field setting values (FSV) reported by the heat pump",

  mount(el, { topRight }) {
    root = el;
    topRight.innerHTML = `
      <label class="btn" style="cursor:pointer"><input type="checkbox" id="s-edit" ${editing ? "checked" : ""} style="margin:0"> Edit mode</label>
      <input class="search" id="s-search" placeholder="Search FSV…" value="${esc(query)}">`;
    topRight.querySelector("#s-search").addEventListener("input", (ev) => { query = ev.target.value.toLowerCase(); render(); });
    topRight.querySelector("#s-edit").addEventListener("change", (ev) => { editing = ev.target.checked; render(); });
    root.addEventListener("click", (ev) => {
      if (ev.target.closest("button, input, label")) return;
      const item = ev.target.closest("[data-entity]");
      if (item) openEntity(item.dataset.entity, 30 * 86400);
    });
    bindControls(root, () => render());
    render();
  },

  update(ev) {
    if (ev.type === "state") {
      confirmState(ev.entity);
      if (ev.entity.category !== "fsv") return;
      const slot = root.querySelector(`[data-entity="${CSS.escape(ev.entity.id)}"] .val`);
      if (slot && !slot.querySelector(".pending")) slot.innerHTML = valueHtml(ev.entity);
    } else {
      render();
    }
  },
};

function valueHtml(e) {
  if (editing && e.writable && !store.status.read_only) return widget(e);
  return `${entityValue(e)}${e.unit && e.kind === "numeric" ? ` <span class="faint">${esc(e.unit)}</span>` : ""}`;
}

function render() {
  const fsv = [...store.entities.values()].filter((e) => e.category === "fsv")
    .filter((e) => !query || e.name.toLowerCase().includes(query))
    .sort((a, b) => a.name.localeCompare(b.name, undefined, { numeric: true }));

  const notice = editing
    ? `<div class="notice"><svg viewBox="0 0 24 24"><path d="M12 2L1 21h22L12 2zm0 6l7.5 13h-15L12 8zm-1 3v4h2v-4h-2zm0 6v2h2v-2h-2z"/></svg>
        <div><b style="color:var(--text)">Changing field settings can damage your installation.</b> Only change values you fully understand.
        Writes are only forwarded to the heat pump when <code>disable_write_send</code> is <code>false</code> in the bridge configuration.</div></div>`
    : "";

  if (!fsv.length) {
    root.innerHTML = notice + `<div class="card empty-state"><strong>No field settings</strong>${query ? "Nothing matches your search." : "No FSV entities were discovered."}</div>`;
    return;
  }

  const groups = new Map(GROUPS.map(([k, label]) => [k, { label, items: [] }]));
  for (const e of fsv) {
    const code = (e.name.match(/FSV\s*(\d+)/) || [])[1] || "";
    const g = groups.get(code.slice(0, 2)) || groups.get("60");
    g.items.push({ e, code });
  }

  root.innerHTML = notice + `<div class="fsv-groups">${[...groups.values()].filter((g) => g.items.length).map((g) => `
    <section class="card">
      <div class="card-head"><h2>${esc(g.label)}</h2><span class="sub">${g.items.length}</span></div>
      ${g.items.map(({ e, code }) => `
        <div class="fsv-item" data-entity="${esc(e.id)}" style="cursor:pointer">
          <div style="min-width:0">
            <div><span class="code">${esc(code)}</span> <span class="nm">${esc(e.name.replace(/^FSV\s*\d+\s*/, ""))}</span></div>
            ${e.min != null ? `<div class="rng">Range ${e.min} – ${e.max}${e.unit ? " " + esc(e.unit) : ""}${e.step ? ` · step ${e.step}` : ""}</div>`
              : e.options.length ? `<div class="rng">${e.options.map(esc).join(" · ")}</div>` : ""}
          </div>
          <div class="val">${valueHtml(e)}</div>
        </div>`).join("")}
    </section>`).join("")}</div>`;
}
