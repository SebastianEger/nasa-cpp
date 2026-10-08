"""Runtime configuration, read from environment variables."""

import os
import uuid
from dataclasses import dataclass, field


def _env_bool(name: str, default: bool) -> bool:
    value = os.environ.get(name)
    if value is None:
        return default
    return value.strip().lower() in ("1", "true", "yes", "on")


@dataclass
class Settings:
    mqtt_host: str = field(default_factory=lambda: os.environ.get("MQTT_HOST", "localhost"))
    mqtt_port: int = field(default_factory=lambda: int(os.environ.get("MQTT_PORT", "1883")))
    mqtt_username: str | None = field(default_factory=lambda: os.environ.get("MQTT_USERNAME") or None)
    mqtt_password: str | None = field(default_factory=lambda: os.environ.get("MQTT_PASSWORD") or None)
    # unique by default: two clients with the same id keep kicking each other off the broker
    mqtt_client_id: str = field(default_factory=lambda: os.environ.get("MQTT_CLIENT_ID") or f"nasa_web_{uuid.uuid4().hex[:8]}")
    # Home Assistant discovery prefix used by tcp_hass_bridge
    discovery_prefix: str = field(default_factory=lambda: os.environ.get("DISCOVERY_PREFIX", "homeassistant"))
    # device identifier set by tcp_hass_bridge; empty accepts every discovered device
    device_id: str = field(default_factory=lambda: os.environ.get("DEVICE_ID", "samsung_nasa_cpp"))

    db_path: str = field(default_factory=lambda: os.environ.get("DB_PATH", "./data/nasa_web.sqlite3"))
    retention_days: int = field(default_factory=lambda: int(os.environ.get("RETENTION_DAYS", "365")))
    # store a sample at least every N seconds even if the value did not change
    heartbeat_seconds: int = field(default_factory=lambda: int(os.environ.get("HEARTBEAT_SECONDS", "300")))
    # store changing numeric values at most every N seconds per entity
    min_interval_seconds: int = field(default_factory=lambda: int(os.environ.get("MIN_INTERVAL_SECONDS", "30")))

    # disables all control endpoints of the web UI
    read_only: bool = field(default_factory=lambda: _env_bool("READ_ONLY", False))


settings = Settings()
