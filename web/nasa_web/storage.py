"""SQLite persistence for entity metadata and the state history."""

import json
import math
import os
import sqlite3
import threading
import time
from typing import Any, Iterable

SCHEMA = """
CREATE TABLE IF NOT EXISTS entities (
    id          TEXT PRIMARY KEY,
    platform    TEXT NOT NULL,
    config      TEXT NOT NULL,
    value       REAL,
    text        TEXT,
    updated     REAL
);
CREATE TABLE IF NOT EXISTS samples (
    entity_id   TEXT    NOT NULL,
    ts          INTEGER NOT NULL,
    value       REAL,
    text        TEXT,
    PRIMARY KEY (entity_id, ts)
) WITHOUT ROWID;
"""


class Storage:
    def __init__(self, path: str, heartbeat_seconds: int = 300, min_interval_seconds: int = 0) -> None:
        if path != ":memory:":
            os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        self._db = sqlite3.connect(path, check_same_thread=False)
        self._db.execute("PRAGMA journal_mode=WAL")
        self._db.execute("PRAGMA synchronous=NORMAL")
        self._db.executescript(SCHEMA)
        self._lock = threading.Lock()
        self._heartbeat = heartbeat_seconds
        self._min_interval = min_interval_seconds
        self._pending: list[tuple[str, int, float | None, str | None]] = []
        self._last: dict[str, tuple[int, float | None, str | None]] = {}
        for entity_id, ts, value, text in self._db.execute(
            "SELECT entity_id, MAX(ts), value, text FROM samples GROUP BY entity_id"
        ):
            self._last[entity_id] = (ts, value, text)

    # ------------------------------------------------------------------ writes

    def save_entity(self, entity_id: str, platform: str, config: dict[str, Any]) -> None:
        with self._lock:
            self._db.execute(
                "INSERT INTO entities (id, platform, config) VALUES (?, ?, ?) "
                "ON CONFLICT(id) DO UPDATE SET platform=excluded.platform, config=excluded.config",
                (entity_id, platform, json.dumps(config)),
            )
            self._db.commit()

    def delete_entity(self, entity_id: str) -> None:
        with self._lock:
            self._db.execute("DELETE FROM entities WHERE id=?", (entity_id,))
            self._db.commit()

    def load_entities(self) -> list[tuple[str, str, dict[str, Any], float | None, str | None, float | None]]:
        with self._lock:
            rows = self._db.execute("SELECT id, platform, config, value, text, updated FROM entities").fetchall()
        return [(r[0], r[1], json.loads(r[2]), r[3], r[4], r[5]) for r in rows]

    def record(self, entity_id: str, value: float | None, text: str | None, ts: float | None = None) -> bool:
        """Queue a sample.

        Unchanged values are only stored once per heartbeat interval; changing numeric values at most
        once per min_interval (state changes of binary/enum entities are always stored).
        """
        now = int(ts if ts is not None else time.time())
        with self._lock:
            last = self._last.get(entity_id)
            if last is not None:
                last_ts, last_value, last_text = last
                unchanged = last_value == value and last_text == text
                if unchanged and now - last_ts < self._heartbeat:
                    return False
                if text is None and now - last_ts < self._min_interval:
                    return False
            self._last[entity_id] = (now, value, text)
            self._pending.append((entity_id, now, value, text))
            return True

    def flush(self) -> int:
        with self._lock:
            pending, self._pending = self._pending, []
            if not pending:
                return 0
            self._db.executemany(
                "INSERT OR REPLACE INTO samples (entity_id, ts, value, text) VALUES (?, ?, ?, ?)", pending
            )
            latest: dict[str, tuple[int, float | None, str | None]] = {}
            for entity_id, ts, value, text in pending:
                latest[entity_id] = (ts, value, text)
            self._db.executemany(
                "UPDATE entities SET value=?, text=?, updated=? WHERE id=?",
                [(v, t, ts, eid) for eid, (ts, v, t) in latest.items()],
            )
            self._db.commit()
            return len(pending)

    def purge(self, retention_days: int) -> int:
        cutoff = int(time.time()) - retention_days * 86400
        with self._lock:
            cur = self._db.execute("DELETE FROM samples WHERE ts < ?", (cutoff,))
            self._db.commit()
            return cur.rowcount

    # ------------------------------------------------------------------- reads

    def history(self, entity_id: str, start: int, end: int, max_points: int = 600, aggregate: bool = True) -> dict[str, list]:
        """Return samples in [start, end]. Numeric series are bucketed to roughly max_points."""
        with self._lock:
            # value right before the window, so step charts start at the correct level
            before = self._db.execute(
                "SELECT ts, value, text FROM samples WHERE entity_id=? AND ts<? ORDER BY ts DESC LIMIT 1",
                (entity_id, start),
            ).fetchone()
            count = self._db.execute(
                "SELECT COUNT(*) FROM samples WHERE entity_id=? AND ts BETWEEN ? AND ?", (entity_id, start, end)
            ).fetchone()[0]

            out: dict[str, list] = {"t": [], "v": [], "min": [], "max": [], "text": []}
            if before is not None:
                out["t"].append(start)
                out["v"].append(before[1])
                out["min"].append(before[1])
                out["max"].append(before[1])
                out["text"].append(before[2])

            if aggregate and count > max_points:
                bucket = max(1, math.ceil((end - start) / max_points))
                rows = self._db.execute(
                    "SELECT (ts / ?) * ? AS b, AVG(value), MIN(value), MAX(value), MAX(text) "
                    "FROM samples WHERE entity_id=? AND ts BETWEEN ? AND ? GROUP BY b ORDER BY b",
                    (bucket, bucket, entity_id, start, end),
                )
                for b, avg, lo, hi, text in rows:
                    out["t"].append(max(int(b), start))
                    out["v"].append(None if avg is None else round(avg, 4))
                    out["min"].append(lo)
                    out["max"].append(hi)
                    out["text"].append(text)
            else:
                rows = self._db.execute(
                    "SELECT ts, value, text FROM samples WHERE entity_id=? AND ts BETWEEN ? AND ? ORDER BY ts",
                    (entity_id, start, end),
                )
                for ts, value, text in rows:
                    out["t"].append(ts)
                    out["v"].append(value)
                    out["min"].append(value)
                    out["max"].append(value)
                    out["text"].append(text)
        return out

    def values_at(self, entity_id: str, timestamps: Iterable[int]) -> list[float | None]:
        """Last known value at each timestamp (first value after it if there is none before)."""
        result: list[float | None] = []
        with self._lock:
            for ts in timestamps:
                row = self._db.execute(
                    "SELECT value FROM samples WHERE entity_id=? AND ts<=? AND value IS NOT NULL "
                    "ORDER BY ts DESC LIMIT 1",
                    (entity_id, ts),
                ).fetchone()
                if row is None:
                    row = self._db.execute(
                        "SELECT value FROM samples WHERE entity_id=? AND ts>? AND value IS NOT NULL "
                        "ORDER BY ts ASC LIMIT 1",
                        (entity_id, ts),
                    ).fetchone()
                result.append(row[0] if row else None)
        return result

    def stats(self) -> dict[str, Any]:
        with self._lock:
            count, oldest = self._db.execute("SELECT COUNT(*), MIN(ts) FROM samples").fetchone()
            page_count = self._db.execute("PRAGMA page_count").fetchone()[0]
            page_size = self._db.execute("PRAGMA page_size").fetchone()[0]
        return {"samples": count, "oldest": oldest, "size_bytes": page_count * page_size}

    def close(self) -> None:
        self.flush()
        with self._lock:
            self._db.close()
