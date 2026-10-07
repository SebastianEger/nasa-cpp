from nasa_web.storage import Storage


def test_record_heartbeat_and_interval():
    s = Storage(":memory:", heartbeat_seconds=300, min_interval_seconds=30)
    assert s.record("a", 1.0, None, 1000)
    assert not s.record("a", 1.0, None, 1100)   # unchanged, within heartbeat
    assert not s.record("a", 2.0, None, 1010)   # changed, but within min interval
    assert s.record("a", 2.0, None, 1040)
    assert s.record("a", 2.0, None, 1400)       # heartbeat
    assert s.record("e", 0.0, "ROOM", 1001) and s.record("e", 1.0, "TANK", 1002)  # state changes always stored
    s.flush()
    h = s.history("a", 0, 2000)
    assert h["t"] == [1000, 1040, 1400]


def test_history_aggregates_and_values_at():
    s = Storage(":memory:", heartbeat_seconds=0)
    for i in range(1000):
        s.record("t", float(i), None, i * 10)
    s.flush()
    h = s.history("t", 0, 9990, max_points=100)
    assert 90 <= len(h["t"]) <= 101
    assert h["min"][0] <= h["v"][0] <= h["max"][0]
    raw = s.history("t", 0, 9990, max_points=100, aggregate=False)
    assert len(raw["t"]) == 1000
    assert s.values_at("t", [-5, 55, 100000]) == [0.0, 5.0, 999.0]


def test_history_includes_value_before_window():
    s = Storage(":memory:")
    s.record("v", 0.0, "ROOM", 100)
    s.flush()
    h = s.history("v", 200, 300, aggregate=False)
    assert h["t"] == [200] and h["text"] == ["ROOM"]
