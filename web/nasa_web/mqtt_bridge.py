"""MQTT client that discovers tcp_hass_bridge entities and records their states."""

import json
import logging
import time
from typing import Any, Callable

import paho.mqtt.client as mqtt

from .config import Settings
from .entities import Entity, Registry, entity_from_discovery
from .storage import Storage

log = logging.getLogger("nasa_web.mqtt")

Listener = Callable[[dict[str, Any]], None]


class MqttBridge:
    def __init__(self, settings: Settings, registry: Registry, storage: Storage) -> None:
        self.settings = settings
        self.registry = registry
        self.storage = storage
        self.connected = False
        self.last_message: float | None = None
        self._listeners: list[Listener] = []

        self._client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=settings.mqtt_client_id)
        if settings.mqtt_username:
            self._client.username_pw_set(settings.mqtt_username, settings.mqtt_password)
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message
        self._client.reconnect_delay_set(min_delay=1, max_delay=30)

    # --------------------------------------------------------------- lifecycle

    def start(self) -> None:
        log.info("Connecting to MQTT broker %s:%s", self.settings.mqtt_host, self.settings.mqtt_port)
        self._client.connect_async(self.settings.mqtt_host, self.settings.mqtt_port, keepalive=30)
        self._client.loop_start()

    def stop(self) -> None:
        self._client.disconnect()
        self._client.loop_stop()

    def add_listener(self, listener: Listener) -> None:
        self._listeners.append(listener)

    def _emit(self, event: dict[str, Any]) -> None:
        for listener in self._listeners:
            try:
                listener(event)
            except Exception:  # noqa: BLE001 - never let a listener kill the MQTT thread
                log.exception("listener failed")

    # ----------------------------------------------------------------- publish

    def send_command(self, entity: Entity, value: Any) -> str:
        if entity.command_topic is None:
            raise ValueError("entity is read-only")
        payload = entity.build_command(value)
        info = self._client.publish(entity.command_topic, payload, qos=1)
        if info.rc != mqtt.MQTT_ERR_SUCCESS:
            raise ConnectionError(mqtt.error_string(info.rc))
        log.info("Command %s <- %s", entity.command_topic, payload)
        return payload

    # --------------------------------------------------------------- callbacks

    def _on_connect(self, client: mqtt.Client, _userdata, _flags, reason_code, _props=None) -> None:
        if reason_code.is_failure:
            log.error("MQTT connect failed: %s", reason_code)
            return
        log.info("MQTT connected")
        self.connected = True
        client.subscribe(f"{self.settings.discovery_prefix}/+/+/config", qos=1)
        for entity in self.registry.all():
            client.subscribe(entity.state_topic, qos=1)
        self._emit({"type": "status", "mqtt": True})

    def _on_disconnect(self, _client, _userdata, _flags, reason_code, _props=None) -> None:
        log.warning("MQTT disconnected: %s", reason_code)
        self.connected = False
        self._emit({"type": "status", "mqtt": False})

    def _on_message(self, client: mqtt.Client, _userdata, msg: mqtt.MQTTMessage) -> None:
        try:
            payload = msg.payload.decode("utf-8", errors="replace")
            if msg.topic.endswith("/config") and msg.topic.startswith(self.settings.discovery_prefix + "/"):
                self._handle_discovery(client, msg.topic, payload)
                return
            entity = self.registry.by_topic(msg.topic)
            if entity is not None:
                self._handle_state(entity, payload)
        except Exception:  # noqa: BLE001
            log.exception("failed to handle message on %s", msg.topic)

    def _handle_discovery(self, client: mqtt.Client, topic: str, payload: str) -> None:
        parts = topic.split("/")
        if len(parts) != 4:
            return
        _, platform, object_id, _ = parts

        if not payload.strip():
            # empty retained config removes the entity
            if self.registry.get(object_id):
                self.registry.remove(object_id)
                self.storage.delete_entity(object_id)
                self._emit({"type": "entities"})
            return

        cfg = json.loads(payload)
        identifiers = (cfg.get("device") or {}).get("identifiers")
        if isinstance(identifiers, str):
            identifiers = [identifiers]
        if self.settings.device_id and self.settings.device_id not in (identifiers or []):
            return
        if "state_topic" not in cfg:
            return

        entity = self.registry.upsert(entity_from_discovery(object_id, platform, cfg))
        self.storage.save_entity(object_id, platform, cfg)
        client.subscribe(entity.state_topic, qos=1)
        log.debug("Discovered %s (%s)", entity.name, platform)
        self._emit({"type": "entities"})

    def _handle_state(self, entity: Entity, payload: str) -> None:
        value, text = entity.parse_state(payload)
        if value is None and text is None:
            return
        now = time.time()
        self.last_message = now
        self.registry.update_state(entity, value, text)
        self.storage.record(entity.id, value, text, now)
        self._emit({"type": "state", "id": entity.id, "value": value, "text": text, "ts": now})
