# nasa-web — standalone dashboard

A self-hosted web dashboard for `tcp_hass_bridge`. No Home Assistant needed. It listens to the
MQTT topics the bridge already publishes, stores every value in SQLite, and serves a live UI with history.

![Web dashboard](../res/web-dashboard.png)

## Features

- **Overview**: animated system schematic (compressor, fan, flow/return pipes, 3-way valve, house and
  DHW tank), live heat output, power and COP, KPI tiles with 24 h sparklines, today's energy and COP, a 24 h
  activity timeline, and every control (switches, modes, set-points).
- **History**: temperatures, power, compressor frequency and flow, plus operating-state timelines. Ranges
  run from 1 h to 1 y; drag across a chart to zoom, and use the arrows to page back in time. A custom chart
  plots any sensor, with series grouped by unit.
- **Energy**: daily or monthly electricity vs. heat, COP per period, and ambient energy gained.
- **Settings**: all FSV field settings, grouped and searchable, with an optional edit mode.
- **Entities**: every discovered entity. Click one to open its history.
- Live updates over WebSocket, light and dark themes, and a mobile layout.

## How it works

```
heat pump ⇄ RS485/TCP ⇄ tcp_hass_bridge ⇄ MQTT broker ⇄ nasa-web (FastAPI + SQLite) ⇄ browser
```

`nasa-web` subscribes to `homeassistant/+/+/config`, the discovery messages the bridge publishes
(retained). From those it learns every entity, its state/command topics, units, options, limits and the
value/command scaling templates. It then subscribes to all state topics and records the values.
Commands from the UI are published to the entity's `command_topic` in the same format Home Assistant
would use, so the bridge handles them as usual.

History is stored on change. Unchanged values are written at least every `HEARTBEAT_SECONDS`, and
changing numeric values at most every `MIN_INTERVAL_SECONDS`. Long time ranges are downsampled in SQL,
so even a year of data stays fast.

## Run with Docker

Add it next to the bridge with the repository's root `compose.yml`. It uses host networking and the
same broker as the bridge:

```bash
docker compose up -d nasa-web      # → http://<host>:8080
```

Or run the standalone stack in `web/compose.yml`, which includes its own Mosquitto broker:

```bash
cd web
docker compose up -d                    # broker + dashboard
docker compose --profile demo up -d     # … plus a simulated heat pump, no hardware needed
```

If you use the standalone broker, point the bridge at it (`mqtt.address` in `cfg/tcp_hass_bridge.json`).

## Run locally

```bash
cd web
pip install -r requirements.txt
MQTT_HOST=localhost python -m nasa_web           # http://localhost:8080
```

To try it without a heat pump, run a broker on `localhost:1883`, then:

```bash
python tools/simulate.py --backfill-days 30 --no-live   # seed 30 days of history into ./data
python tools/simulate.py                                # publish live data like tcp_hass_bridge
```

Tests: `cd web && python -m pytest tests`

## Configuration

All settings are environment variables.

| Variable | Default | Description |
|---|---|---|
| `MQTT_HOST` / `MQTT_PORT` | `localhost` / `1883` | MQTT broker |
| `MQTT_USERNAME` / `MQTT_PASSWORD` | – | Broker credentials |
| `MQTT_CLIENT_ID` | `nasa_web_<random>` | MQTT client id |
| `DISCOVERY_PREFIX` | `homeassistant` | Discovery prefix used by the bridge |
| `DEVICE_ID` | `samsung_nasa_cpp` | Only entities of this device are used (empty = all) |
| `HTTP_HOST` / `HTTP_PORT` | `0.0.0.0` / `8080` | Web server |
| `DB_PATH` | `./data/nasa_web.sqlite3` (`/data/…` in Docker) | SQLite database |
| `RETENTION_DAYS` | `365` | Samples older than this are deleted |
| `HEARTBEAT_SECONDS` | `300` | Store unchanged values at least this often |
| `MIN_INTERVAL_SECONDS` | `30` | Store changing numeric values at most this often |
| `READ_ONLY` | `false` | Disable all controls in the UI/API |

At the defaults, expect roughly 50 MB of database per month for the standard sensor and control lists.

> [!WARNING]
> The UI has no authentication. Keep it inside your LAN or put it behind a reverse proxy with auth.
> Set `READ_ONLY=true` if it should only display data. FSV writes only reach the heat pump when the
> bridge runs with `"disable_write_send": false`.

## API

| Endpoint | Description |
|---|---|
| `GET /api/status` | Broker connection, last message, storage stats |
| `GET /api/entities` | All entities with current value |
| `GET /api/history?ids=a,b&start=&end=&points=600` | History (unix seconds), downsampled to ~`points` |
| `GET /api/values_at?ids=a,b&ts=t1,t2` | Last known value at each timestamp (used for energy deltas) |
| `POST /api/entities/{id}/command` `{"value": …}` | Send a command |
| `WS /ws` | Snapshot, then live `state` / `status` events |
