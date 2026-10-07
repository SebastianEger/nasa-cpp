"""Entity registry built from the Home Assistant discovery messages of tcp_hass_bridge."""

import math
import re
import threading
import time
from dataclasses import dataclass, field
from typing import Any

# matches the simple arithmetic templates used in msg/*/..._msg_list.json, e.g.
#   {{ (value | float) / 10 }}   {{ value * 10 }}   {{ (value / 10) | int }}
_TEMPLATE_RE = re.compile(r"value\s*(?:\|\s*float\s*)?\)?\s*([*/])\s*([0-9.]+)")


@dataclass(frozen=True)
class Transform:
    """A linear scale factor parsed from a Jinja template."""

    factor: float = 1.0
    to_int: bool = False

    @classmethod
    def parse(cls, template: str | None) -> "Transform":
        if not template:
            return cls()
        match = _TEMPLATE_RE.search(template)
        if not match:
            return cls()
        op, num = match.group(1), float(match.group(2))
        factor = num if op == "*" else 1.0 / num
        return cls(factor=factor, to_int=bool(re.search(r"\|\s*int", template)))

    def apply(self, value: float) -> float:
        out = value * self.factor
        if self.to_int:
            return float(math.trunc(out))
        # avoid float noise like 21.400000000000002
        return round(out, 6)


@dataclass
class Entity:
    id: str
    name: str
    platform: str  # sensor | binary_sensor | switch | select | number
    category: str  # sensor | control | fsv
    state_topic: str
    command_topic: str | None = None
    device_class: str | None = None
    unit: str | None = None
    state_class: str | None = None
    options: list[str] = field(default_factory=list)
    min: float | None = None
    max: float | None = None
    step: float | None = None
    value_tf: Transform = field(default_factory=Transform)
    command_tf: Transform = field(default_factory=Transform)

    # live state
    value: float | None = None
    text: str | None = None
    updated: float | None = None

    @property
    def kind(self) -> str:
        """How the value is represented: numeric, binary or enum."""
        if self.platform in ("switch", "binary_sensor"):
            return "binary"
        if self.platform == "select" or self.device_class == "enum":
            return "enum"
        return "numeric"

    def parse_state(self, payload: str) -> tuple[float | None, str | None]:
        """Convert a raw MQTT state payload into (numeric value, text)."""
        payload = payload.strip()
        if payload == "":
            return None, None
        kind = self.kind
        if kind == "binary":
            on = payload.upper() == "ON"
            return (1.0 if on else 0.0), ("ON" if on else "OFF")
        if kind == "enum":
            index = self.options.index(payload) if payload in self.options else None
            return (float(index) if index is not None else None), payload
        try:
            return self.value_tf.apply(float(payload)), None
        except ValueError:
            return None, payload

    def build_command(self, value: Any) -> str:
        """Convert a UI value into the MQTT command payload expected by the bridge."""
        kind = self.kind
        if kind == "binary":
            if isinstance(value, str):
                return "ON" if value.upper() in ("ON", "1", "TRUE") else "OFF"
            return "ON" if value else "OFF"
        if kind == "enum":
            if str(value) not in self.options:
                raise ValueError(f"invalid option {value!r}")
            return str(value)
        number = float(value)
        if self.min is not None and number < self.min:
            raise ValueError(f"value below minimum {self.min}")
        if self.max is not None and number > self.max:
            raise ValueError(f"value above maximum {self.max}")
        raw = self.command_tf.apply(number)
        # the bridge parses commands with std::stoi
        return str(int(round(raw)))

    def to_dict(self) -> dict[str, Any]:
        return {
            "id": self.id,
            "name": self.name,
            "platform": self.platform,
            "category": self.category,
            "kind": self.kind,
            "device_class": self.device_class,
            "unit": self.unit,
            "state_class": self.state_class,
            "options": self.options,
            "min": self.min,
            "max": self.max,
            "step": self.step,
            "writable": self.command_topic is not None,
            "value": self.value,
            "text": self.text,
            "updated": self.updated,
        }


def entity_from_discovery(object_id: str, platform: str, cfg: dict[str, Any]) -> Entity:
    name = str(cfg.get("name", object_id))
    if platform in ("sensor", "binary_sensor"):
        category = "sensor"
    elif name.upper().startswith("FSV"):
        category = "fsv"
    else:
        category = "control"

    def _num(key: str) -> float | None:
        try:
            return float(cfg[key]) if key in cfg else None
        except (TypeError, ValueError):
            return None

    options = cfg.get("options") or []
    if isinstance(options, dict):
        options = list(options.keys())

    return Entity(
        id=object_id,
        name=name,
        platform=platform,
        category=category,
        state_topic=cfg["state_topic"],
        command_topic=cfg.get("command_topic"),
        device_class=cfg.get("device_class"),
        unit=cfg.get("unit_of_measurement"),
        state_class=cfg.get("state_class"),
        options=[str(o) for o in options],
        min=_num("min"),
        max=_num("max"),
        step=_num("step"),
        value_tf=Transform.parse(cfg.get("value_template")),
        command_tf=Transform.parse(cfg.get("command_template")),
    )


class Registry:
    """Thread-safe map of discovered entities."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._by_id: dict[str, Entity] = {}
        self._by_topic: dict[str, Entity] = {}

    def upsert(self, entity: Entity) -> Entity:
        with self._lock:
            old = self._by_id.get(entity.id)
            if old is not None:
                # keep live state across re-discovery
                entity.value, entity.text, entity.updated = old.value, old.text, old.updated
                self._by_topic.pop(old.state_topic, None)
            self._by_id[entity.id] = entity
            self._by_topic[entity.state_topic] = entity
            return entity

    def remove(self, entity_id: str) -> None:
        with self._lock:
            old = self._by_id.pop(entity_id, None)
            if old is not None:
                self._by_topic.pop(old.state_topic, None)

    def by_topic(self, topic: str) -> Entity | None:
        with self._lock:
            return self._by_topic.get(topic)

    def get(self, entity_id: str) -> Entity | None:
        with self._lock:
            return self._by_id.get(entity_id)

    def all(self) -> list[Entity]:
        with self._lock:
            return list(self._by_id.values())

    def update_state(self, entity: Entity, value: float | None, text: str | None) -> None:
        with self._lock:
            entity.value, entity.text, entity.updated = value, text, time.time()
