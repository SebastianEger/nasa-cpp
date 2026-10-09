// Live entity state, kept in sync over a WebSocket.

const listeners = new Set();

export const store = {
  entities: new Map(), // id -> entity
  byName: new Map(),   // lower-case name -> entity
  status: { mqtt: false },
  wsConnected: false,
  ready: false,
};

/** Well-known entity names from msg/*_msg_list.json */
export const N = {
  outdoor: "Temp Outer",
  waterOut: "Temp Water Out",
  waterIn: "Temp Water In",
  lawTarget: "Temp Water Law Target",
  tank: "Temp DHW Tank",
  outlet1: "Temp Outlet Zone1",
  outlet2: "Temp Outlet Zone2",
  flow: "Flow Sensor",
  power: "System Power",
  freq: "Current Frequency 1",
  fan: "Outdoor Fan1 RPM",
  pwm: "PWM",
  valve: "3 Way Valve",
  opMode: "Operating Mode",
  defrost: "Defrost Step",
  backup: "Backup Heater Mode",
  booster: "Booster Heater Mode",
  error: "Error Code",
  energyIn: "Total Input Energy",
  energyOut: "Total Output Energy",
  minutesActive: "Minutes Active",
  dhw: "DHW",
  dhwMode: "DHW Mode",
  dhwTarget: "Temp DHW Target",
  mode: "Mode",
  zone1: "Zone1",
  zone1Target: "Zone 1 Target",
  zone2: "Zone2",
  zone2Target: "Zone 2 Target",
  outlet1Target: "Zone 1 Water Outlet Target",
  outlet2Target: "Zone 2 Water Outlet Target",
  lawOffset: "Temp Water Law Target F",
};

export function ent(name) {
  return store.byName.get(name.toLowerCase());
}

export function val(name) {
  const e = ent(name);
  return e ? e.value : null;
}

export function text(name) {
  const e = ent(name);
  return e ? (e.text ?? (e.value != null ? String(e.value) : null)) : null;
}

/** Value of a field setting by its code, e.g. fsv(2011). */
export function fsv(code) {
  const prefix = `fsv ${code} `;
  for (const [name, e] of store.byName) if (name.startsWith(prefix)) return e.value;
  return null;
}

/**
 * Water outlet target from the heating water law (used when Mode is AUTO).
 * Zone 1 uses WL1 (FSV 2021/2022), zone 2 uses WL2 (FSV 2031/2032). The target is
 * interpolated linearly between the outdoor temperatures FSV 2011 (max point, cold)
 * and FSV 2012 (min point, warm) and held constant outside that range. The water law
 * offset ("Temp Water Law Target F") is added to the result.
 */
export function waterLawTarget(zone) {
  const outdoor = val(N.outdoor);
  const tCold = fsv(2011), tWarm = fsv(2012);
  const wCold = fsv(zone === 1 ? 2021 : 2031), wWarm = fsv(zone === 1 ? 2022 : 2032);
  if ([outdoor, tCold, tWarm, wCold, wWarm].some((v) => v == null)) return null;
  let target;
  if (outdoor <= tCold || tWarm <= tCold) target = wCold;
  else if (outdoor >= tWarm) target = wWarm;
  else target = wCold + ((outdoor - tCold) / (tWarm - tCold)) * (wWarm - wCold);
  return target + (val(N.lawOffset) ?? 0);
}

/** Effective water outlet target of a zone: water law in AUTO mode, else the set target. */
export function outletTarget(zone) {
  if (text(N.mode) === "AUTO") return waterLawTarget(zone);
  return val(zone === 1 ? N.outlet1Target : N.outlet2Target);
}

/** Thermal output in W from flow (L/min) and delta T. */
export function heatOutput() {
  const flow = val(N.flow), out = val(N.waterOut), inn = val(N.waterIn), freq = val(N.freq);
  if (flow == null || out == null || inn == null) return null;
  if (freq === 0) return 0; // compressor off: residual delta T is not useful heat
  return Math.max(0, flow / 60 * 4186 * (out - inn));
}

export function liveCop() {
  const heat = heatOutput(), power = val(N.power), freq = val(N.freq);
  if (heat == null || power == null || power < 150 || !freq) return null;
  return heat / power;
}

export function subscribe(fn) {
  listeners.add(fn);
  return () => listeners.delete(fn);
}

function emit(event) {
  for (const fn of listeners) {
    try { fn(event); } catch (err) { console.error(err); }
  }
}

function setEntities(list) {
  store.entities.clear();
  store.byName.clear();
  for (const e of list) {
    store.entities.set(e.id, e);
    store.byName.set(e.name.toLowerCase(), e);
  }
}

export function connect() {
  const proto = location.protocol === "https:" ? "wss" : "ws";
  const base = location.pathname.replace(/[^/]*$/, "");
  let retry = 1000;

  const open = () => {
    const ws = new WebSocket(`${proto}://${location.host}${base}ws`);
    ws.onopen = () => {
      retry = 1000;
      store.wsConnected = true;
      emit({ type: "connection" });
    };
    ws.onmessage = (msg) => {
      const ev = JSON.parse(msg.data);
      if (ev.type === "snapshot") {
        setEntities(ev.entities);
        store.status = ev.status;
        store.ready = true;
        emit({ type: "snapshot" });
      } else if (ev.type === "state") {
        const e = store.entities.get(ev.id);
        if (e) {
          e.value = ev.value;
          e.text = ev.text;
          e.updated = ev.ts;
          store.status.last_message = ev.ts;
          emit({ type: "state", entity: e });
        }
      } else if (ev.type === "status") {
        store.status = ev.status;
        emit({ type: "connection" });
      }
    };
    ws.onclose = () => {
      store.wsConnected = false;
      emit({ type: "connection" });
      setTimeout(open, retry);
      retry = Math.min(retry * 2, 15000);
    };
    ws.onerror = () => ws.close();
  };
  open();
}
