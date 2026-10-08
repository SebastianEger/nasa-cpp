"""FastAPI application: REST + WebSocket API and the static frontend."""

import asyncio
import contextlib
import logging
import time
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException, Query, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from .config import settings
from .entities import Registry, entity_from_discovery
from .mqtt_bridge import MqttBridge
from .storage import Storage

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
log = logging.getLogger("nasa_web")

STATIC_DIR = Path(__file__).parent / "static"
MAX_IDS = 32


class Hub:
    """Fans out MQTT events (from the paho thread) to all WebSocket clients."""

    def __init__(self) -> None:
        self.loop: asyncio.AbstractEventLoop | None = None
        self.queues: set[asyncio.Queue] = set()

    def publish(self, event: dict[str, Any]) -> None:
        if self.loop is None:
            return
        self.loop.call_soon_threadsafe(self._fan_out, event)

    def _fan_out(self, event: dict[str, Any]) -> None:
        for queue in list(self.queues):
            if queue.qsize() < 1000:
                queue.put_nowait(event)


registry = Registry()
storage = Storage(settings.db_path, settings.heartbeat_seconds, settings.min_interval_seconds)
bridge = MqttBridge(settings, registry, storage)
hub = Hub()
bridge.add_listener(hub.publish)


def _restore_entities() -> None:
    for entity_id, platform, cfg, value, text, updated in storage.load_entities():
        try:
            entity = registry.upsert(entity_from_discovery(entity_id, platform, cfg))
            entity.value, entity.text, entity.updated = value, text, updated
        except Exception:  # noqa: BLE001
            log.exception("could not restore entity %s", entity_id)
    log.info("Restored %d entities from %s", len(registry.all()), settings.db_path)


async def _flush_loop() -> None:
    while True:
        await asyncio.sleep(2)
        await asyncio.to_thread(storage.flush)


async def _purge_loop() -> None:
    while True:
        removed = await asyncio.to_thread(storage.purge, settings.retention_days)
        if removed:
            log.info("Purged %d samples older than %d days", removed, settings.retention_days)
        await asyncio.sleep(6 * 3600)


@contextlib.asynccontextmanager
async def lifespan(_app: FastAPI):
    hub.loop = asyncio.get_running_loop()
    _restore_entities()
    bridge.start()
    tasks = [asyncio.create_task(_flush_loop()), asyncio.create_task(_purge_loop())]
    try:
        yield
    finally:
        for task in tasks:
            task.cancel()
        bridge.stop()
        storage.close()


app = FastAPI(title="NASA EHS Web", lifespan=lifespan)


def _status() -> dict[str, Any]:
    return {
        "mqtt": bridge.connected,
        "broker": f"{settings.mqtt_host}:{settings.mqtt_port}",
        "last_message": bridge.last_message,
        "read_only": settings.read_only,
        "retention_days": settings.retention_days,
        "entities": len(registry.all()),
        "server_time": time.time(),
    }


def _split_ids(ids: str) -> list[str]:
    out = [i for i in ids.split(",") if i][:MAX_IDS]
    if not out:
        raise HTTPException(400, "no entity ids given")
    return out


@app.get("/api/status")
async def api_status() -> dict[str, Any]:
    status = _status()
    status["storage"] = await asyncio.to_thread(storage.stats)
    return status


@app.get("/api/entities")
async def api_entities() -> list[dict[str, Any]]:
    return [e.to_dict() for e in registry.all()]


@app.get("/api/history")
async def api_history(
    ids: str,
    start: int | None = None,
    end: int | None = None,
    points: int = Query(600, ge=10, le=5000),
) -> dict[str, Any]:
    end = end or int(time.time())
    start = start or end - 86400
    if start >= end:
        raise HTTPException(400, "start must be before end")
    result: dict[str, Any] = {}
    for entity_id in _split_ids(ids):
        entity = registry.get(entity_id)
        numeric = entity is None or entity.kind == "numeric"
        result[entity_id] = await asyncio.to_thread(storage.history, entity_id, start, end, points, numeric)
    return {"start": start, "end": end, "series": result}


@app.get("/api/values_at")
async def api_values_at(ids: str, ts: str) -> dict[str, Any]:
    try:
        timestamps = [int(t) for t in ts.split(",") if t][:500]
    except ValueError as exc:
        raise HTTPException(400, "invalid timestamps") from exc
    result = {}
    for entity_id in _split_ids(ids):
        result[entity_id] = await asyncio.to_thread(storage.values_at, entity_id, timestamps)
    return {"ts": timestamps, "values": result}


class Command(BaseModel):
    value: Any


@app.post("/api/entities/{entity_id}/command")
async def api_command(entity_id: str, command: Command) -> dict[str, Any]:
    if settings.read_only:
        raise HTTPException(403, "web UI is in read-only mode")
    entity = registry.get(entity_id)
    if entity is None:
        raise HTTPException(404, "unknown entity")
    if not bridge.connected:
        raise HTTPException(503, "not connected to MQTT broker")
    try:
        payload = bridge.send_command(entity, command.value)
    except ValueError as exc:
        raise HTTPException(400, str(exc)) from exc
    except ConnectionError as exc:
        raise HTTPException(503, str(exc)) from exc
    return {"ok": True, "payload": payload}


@app.websocket("/ws")
async def websocket(ws: WebSocket) -> None:
    await ws.accept()
    queue: asyncio.Queue = asyncio.Queue()
    hub.queues.add(queue)
    try:
        await ws.send_json({"type": "snapshot", "status": _status(), "entities": [e.to_dict() for e in registry.all()]})
        while True:
            event = await queue.get()
            if event["type"] == "entities":
                # coalesce bursts of discovery messages into one snapshot
                await asyncio.sleep(0.5)
                while not queue.empty():
                    queue.get_nowait()
                event = {"type": "snapshot", "status": _status(), "entities": [e.to_dict() for e in registry.all()]}
            elif event["type"] == "status":
                event = {"type": "status", "status": _status()}
            await ws.send_json(event)
    except (WebSocketDisconnect, RuntimeError):
        pass
    finally:
        hub.queues.discard(queue)


@app.get("/")
async def index() -> FileResponse:
    return FileResponse(STATIC_DIR / "index.html")


app.mount("/", StaticFiles(directory=STATIC_DIR), name="static")
