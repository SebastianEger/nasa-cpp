#!/usr/bin/env python3
"""Fake tcp_hass_bridge: publishes the same discovery/state topics with simulated heat pump data.

Useful to develop or demo the web UI without a heat pump:

    python tools/simulate.py --broker localhost                          # live data via MQTT
    python tools/simulate.py --backfill-days 14 --db data/nasa_web.sqlite3   # seed history, then live
"""

import argparse
import json
import math
import random
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from nasa_web.entities import Transform  # noqa: E402

DEVICE_NAME = "Samsung EHS"
DEVICE = {"name": DEVICE_NAME, "identifiers": "samsung_nasa_cpp", "manufacturer": "Samsung", "model": "Mono HQ Quiet"}
DEFAULT_MSG_DIR = ROOT.parent / "msg" / "AE08BXYDGG-MIM03EN"


def object_id(name: str) -> str:
    return f"{DEVICE_NAME} {name}".lower().replace(" ", "_")


def load_messages(msg_dir: Path) -> list[dict]:
    """Build the discovery payloads exactly like NasaHassBridge::addMsg."""
    out = []
    for file in ("sensor_msg_list.json", "control_msg_list.json", "fsv_msg_list.json"):
        path = msg_dir / file
        if not path.exists():
            continue
        for msg in json.loads(path.read_text()):
            hass = dict(msg["hass"])
            platform = hass.pop("type")
            oid = object_id(msg["name"])
            prefix = f"homeassistant/{platform}/{oid}"
            if isinstance(hass.get("options"), dict):
                hass["options"] = list(hass["options"].keys())
            payload = dict(hass, device=DEVICE, unique_id=f"samsung_nasa_cpp_{oid}", name=msg["name"],
                           state_topic=f"{prefix}/state")
            if platform in ("switch", "select", "number"):
                payload["command_topic"] = f"{prefix}/set"
            out.append({"name": msg["name"], "platform": platform, "id": oid,
                        "config_topic": f"{prefix}/config", "payload": payload})
    return out


class HeatPump:
    """Very rough heat pump model, good enough to make the charts look alive."""

    def __init__(self, t0: float) -> None:
        self.t = t0
        self.energy_in = 1_843_000.0  # Wh
        self.energy_out = 6_912_000.0
        self.minutes_total = 412_000.0
        self.minutes = 0.0
        self.tank = 46.0
        self.water_out = 30.0
        self.room = 21.0
        self.compressor = False
        self.cycle_left = 40 * 60.0
        self.freq = 0.0
        self.controls = {
            "DHW": "ON", "DHW Mode": "STANDARD", "Temp DHW Target": 48.0, "Mode": "HEAT", "Zone1": "ON",
            "Zone 1 Target": 21.5, "Zone 1 Water Outlet Target": 35.0, "Zone2": "OFF", "Zone 2 Target": 20.0,
            "Zone 2 Water Outlet Target": 30.0, "Silence Mode": "OFF", "Outing Mode": "OFF",
            "Temp Water Law Target F": 0.0,
        }

    def outdoor(self) -> float:
        day = (self.t % 86400) / 86400
        season = math.sin(self.t / 86400 / 9) * 3
        return 4 + season + 5 * math.sin(2 * math.pi * (day - 0.33)) + random.gauss(0, 0.15)

    def dhw_active(self) -> bool:
        hour = (self.t % 86400) / 3600
        return self.controls["DHW"] == "ON" and (13.0 <= hour < 13.75 or self.tank < self.controls["Temp DHW Target"] - 6)

    def step(self, dt: float) -> dict[str, float | str]:
        self.t += dt
        out_t = self.outdoor()
        dhw = self.dhw_active()
        heating = self.controls["Zone1"] == "ON" or self.controls["Zone2"] == "ON"

        self.cycle_left -= dt
        if self.cycle_left <= 0:
            self.compressor = not self.compressor
            self.cycle_left = random.uniform(35, 70) * 60 if self.compressor else random.uniform(8, 20) * 60
        running = dhw or (heating and self.compressor)

        target_freq = 0.0
        if running:
            target_freq = (78 if dhw else 30 + max(0.0, 12 - out_t) * 3.2)
            if self.controls["Silence Mode"] == "ON":
                target_freq *= 0.8
        self.freq += (target_freq - self.freq) * min(1.0, dt / 90)

        law_target = 35 - 0.55 * (out_t - 0) + self.controls["Temp Water Law Target F"]
        if dhw:
            goal = min(self.tank + 7, 58)
        elif running:
            goal = law_target
        else:
            goal = 24
        self.water_out += (goal - self.water_out) * min(1.0, dt / 300)
        delta_t = (5.0 if running else 0.6) + random.gauss(0, 0.1)
        water_in = self.water_out - delta_t
        flow = (17.5 + random.gauss(0, 0.2)) if (running or heating) else 0.0

        power = (self.freq * 21 + 70) if self.freq > 1 else 9
        cop = max(1.6, (3.4 + 0.09 * out_t) - (0.9 if dhw else 0))
        heat = power * cop if self.freq > 1 else 0
        self.energy_in += power * dt / 3600
        self.energy_out += heat * dt / 3600
        if self.freq > 1:
            self.minutes += dt / 60
            self.minutes_total += dt / 60

        if dhw:
            self.tank += dt * 0.0035
        else:
            self.tank -= dt * 0.00012
        self.room += ((self.controls["Zone 1 Target"] - 0.3 + (0.4 if running else -0.4)) - self.room) * min(1.0, dt / 5400)

        defrost = "NO_DEFROST_OPERATION"
        if running and out_t < 3 and (self.t % 5400) < 300:
            defrost = "DEFROST_STAGE_3"

        return {
            "Error Code": 0,
            "Temp Outer": out_t,
            "Temp Water Law Target": law_target,
            "Temp Water In": water_in,
            "Temp Water Out": self.water_out,
            "Current Frequency 1": round(self.freq),
            "Outdoor Fan1 RPM": round(self.freq * 9.5) if self.freq > 1 else 0,
            "PWM": (85 if running else 40) if flow else 0,
            "3 Way Valve": "TANK" if dhw else "ROOM",
            "Temp DHW Tank": self.tank,
            "Total Output Energy": round(self.energy_out),
            "System Power": round(power),
            "Total Input Energy": round(self.energy_in),
            "Temp Zone 1": self.room,
            "Temp Outlet Zone1": self.water_out - 0.4,
            "Temp Zone 2": self.room - 1.2,
            "Temp Outlet Zone2": self.water_out - 3,
            "Operating Mode": "OP_NORMAL" if running else "OP_STOP",
            "Backup Heater Mode": "OFF",
            "Booster Heater Mode": "OFF",
            "Flow Sensor": flow,
            "Defrost Step": defrost,
            "Minutes Active": round(self.minutes),
            "Minutes Active Since Installation": round(self.minutes_total),
            **self.controls,
        }


def to_payload(msg: dict, value) -> str | None:
    hass = msg["payload"]
    if msg["platform"] in ("switch", "binary_sensor") or "options" in hass:
        return str(value)
    tf = Transform.parse(hass.get("value_template"))
    if tf.factor == 0:
        return None
    return str(int(round(float(value) / tf.factor)))


def backfill(messages: list[dict], db_path: str, days: float, pump: HeatPump) -> None:
    from nasa_web.entities import entity_from_discovery
    from nasa_web.storage import Storage

    storage = Storage(db_path, heartbeat_seconds=300, min_interval_seconds=15)
    entities = {}
    for msg in messages:
        entity = entity_from_discovery(msg["id"], msg["platform"], msg["payload"])
        entities[msg["name"]] = entity
        storage.save_entity(msg["id"], msg["platform"], msg["payload"])

    by_name = {m["name"]: m for m in messages}
    dt = 30.0
    steps = int(days * 86400 / dt)
    for i in range(steps):
        values = pump.step(dt)
        for name, value in values.items():
            entity = entities.get(name)
            if entity is None:
                continue
            v, text = entity.parse_state(to_payload(by_name[name], value) or "")
            storage.record(entity.id, v, text, pump.t)
        if i % 2000 == 0:
            storage.flush()
            print(f"\rbackfill {100 * i / steps:5.1f}%", end="", flush=True)
    storage.close()
    print("\rbackfill done        ")


def resume_counters(db_path: str, pump: HeatPump) -> None:
    """Continue the energy counters from an existing database so they keep increasing."""
    from nasa_web.storage import Storage

    storage = Storage(db_path)
    now = int(time.time())
    for name, attr in (("Total Input Energy", "energy_in"), ("Total Output Energy", "energy_out"),
                       ("Minutes Active Since Installation", "minutes_total")):
        last = storage.values_at(object_id(name), [now])[0]
        if last is not None:
            setattr(pump, attr, last)
    storage.close()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--broker", default="localhost")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--msg-dir", type=Path, default=DEFAULT_MSG_DIR)
    parser.add_argument("--interval", type=float, default=3.0, help="seconds between live updates")
    parser.add_argument("--speed", type=float, default=1.0, help="simulated seconds per real second")
    parser.add_argument("--backfill-days", type=float, default=0, help="write N days of history into --db first")
    parser.add_argument("--db", default=str(ROOT / "data" / "nasa_web.sqlite3"))
    parser.add_argument("--no-live", action="store_true", help="only backfill, do not publish via MQTT")
    args = parser.parse_args()

    messages = load_messages(args.msg_dir)
    by_name = {m["name"]: m for m in messages}
    pump = HeatPump(time.time() - args.backfill_days * 86400)

    if args.backfill_days:
        backfill(messages, args.db, args.backfill_days, pump)
    elif Path(args.db).exists():
        resume_counters(args.db, pump)
    if args.no_live:
        return

    import paho.mqtt.client as mqtt

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"nasa_simulator_{random.randrange(1 << 32):08x}")
    set_topics = {m["payload"]["command_topic"]: m for m in messages if "command_topic" in m["payload"]}

    def on_message(_c, _u, msg):
        target = set_topics.get(msg.topic)
        if target is None:
            return
        payload = msg.payload.decode()
        if target["name"] in pump.controls:
            if target["platform"] == "number":
                pump.controls[target["name"]] = int(payload) * Transform.parse(target["payload"].get("value_template")).factor
            else:
                pump.controls[target["name"]] = payload
        print(f"command {target['name']} <- {payload}")

    def on_connect(c, _u, _f, _rc, _p=None):
        for m in messages:
            c.publish(m["config_topic"], json.dumps(m["payload"]), qos=1, retain=True)
        for topic in set_topics:
            c.subscribe(topic, qos=1)
        print(f"published {len(messages)} discovery messages")

    client.on_message = on_message
    client.on_connect = on_connect
    client.connect(args.broker, args.port)
    client.loop_start()

    fsv_values = {m["name"]: (m["payload"].get("min", 0) + m["payload"].get("max", 0)) / 2 if m["platform"] == "number"
                  else (m["payload"].get("options") or ["OFF"])[0] if m["platform"] == "select" else "OFF"
                  for m in messages if m["name"].startswith("FSV")}
    # realistic heating water law (outdoor max/min point, WL1 and WL2 water temperatures)
    for name, value in fsv_values.items():
        code = name.split()[1]
        fsv_values[name] = {"2011": -10, "2012": 15, "2021": 35, "2022": 25, "2031": 50, "2032": 35}.get(code, value)
    pump.t = time.time()
    try:
        while True:
            values = pump.step(args.interval * args.speed)
            values.update(fsv_values)
            for name, value in values.items():
                msg = by_name.get(name)
                if msg is None:
                    continue
                payload = to_payload(msg, value)
                if payload is not None:
                    retain = msg["platform"] in ("switch", "select", "number")
                    client.publish(msg["payload"]["state_topic"], payload, qos=0, retain=retain)
            time.sleep(args.interval)
    except KeyboardInterrupt:
        pass
    finally:
        client.loop_stop()


if __name__ == "__main__":
    main()
