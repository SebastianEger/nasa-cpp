import pytest

from nasa_web.entities import Transform, entity_from_discovery


def test_transform_templates():
    assert Transform.parse("{{ (value | float) / 10 }}").apply(215) == 21.5
    assert Transform.parse("{{ (value | float) * 10 }}").apply(3) == 30
    assert Transform.parse("{{ value * 10 }}").apply(21.5) == 215
    assert Transform.parse("{{ (value / 10) | int }}").apply(57) == 5
    assert Transform.parse(None).apply(7) == 7


def _number():
    return entity_from_discovery("samsung_ehs_zone_1_target", "number", {
        "name": "Zone 1 Target", "state_topic": "x/state", "command_topic": "x/set", "unit_of_measurement": "°C",
        "value_template": "{{ (value | float) / 10 }}", "command_template": "{{ value * 10 }}",
        "min": 16, "max": 28, "step": 0.5,
    })


def test_number_state_and_command():
    e = _number()
    assert e.kind == "numeric" and e.category == "control"
    assert e.parse_state("215") == (21.5, None)
    assert e.parse_state("") == (None, None)
    assert e.build_command(22.5) == "225"
    with pytest.raises(ValueError):
        e.build_command(40)


def test_enum_and_switch():
    sel = entity_from_discovery("m", "select", {"name": "Mode", "state_topic": "m/state", "command_topic": "m/set",
                                                "options": ["AUTO", "COOL", "HEAT"]})
    assert sel.parse_state("HEAT") == (2.0, "HEAT")
    assert sel.build_command("COOL") == "COOL"
    with pytest.raises(ValueError):
        sel.build_command("DRY")

    sw = entity_from_discovery("d", "switch", {"name": "DHW", "state_topic": "d/state", "command_topic": "d/set"})
    assert sw.parse_state("ON") == (1.0, "ON")
    assert sw.build_command(False) == "OFF"


def test_fsv_category():
    e = entity_from_discovery("f", "number", {"name": "FSV 1011 Water Out", "state_topic": "f/state"})
    assert e.category == "fsv" and not e.to_dict()["writable"]
