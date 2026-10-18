import importlib.util
import json
from pathlib import Path
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("battle_report", root / "tools/summarize-battle-log.py")
reporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reporter)
capture = max(Path(sys.argv[1]).iterdir(), key=lambda p: p.name)

for path in capture.rglob("battle-*.jsonl"):
    if path.name == "battle-user-notes.jsonl":
        continue
    events, errors = reporter.read_capture(path)
    assert not errors, (path, errors)
    assert events[0]["kind"] == "battle_start" and events[-1]["kind"] == "battle_end"
    report = reporter.analyze(events)
    assert report["complete"] == bool(events[-1]["complete"])
    json.dumps(report, allow_nan=False)
    reporter.markdown(report, path)
    if path.parent.name == "complete":
        chars = {r["guid"]: r for r in report["characters"]}
        assert chars["3"]["damage"] == 140, "Sampled/double-counted damage"
        assert chars["3"]["mana_spent"] == 100, "Estimated or repeated cost"
        assert chars["2"]["healing"] == 40 and chars["2"]["overheal"] == 60
        assert chars["3"]["items_spent"] == {"2455": 1}
        assert events[0]["leader"]["name"] == 'Hero "\\\n'
    if path.parent.name in {"limit", "logout", "shutdown", "roster", "instant", "duration", "scope", "bytes"}:
        assert not report["complete"], path


def actor(guid, name, cls):
    return {"guid": str(guid), "owner": "0", "name": name, "class": cls, "alive": True,
            "mana": 700, "max_mana": 1000, "health": 50, "max_health": 100}


a, b, mage, enemy = actor(1, "Leader", 1), actor(2, "Priest A", 5), actor(3, "Mage", 8), actor(99, "Enemy", 0)
second = actor(4, "Priest B", 5)
events = [{"kind": "battle_start", "leader": a, "roster": ["1", "2", "3", "4"], "party": [a, b, mage, second], "elapsed_ms": 0}]
events += [
    {"kind": "cast_start", "actor": b, "target": a, "healing": True, "cast": "10", "elapsed_ms": 10},
    {"kind": "cast_start", "actor": second, "target": a, "healing": True, "cast": "11", "elapsed_ms": 20},
    {"kind": "cast_launch", "actor": b, "target": a, "healing": True, "cast": "10", "elapsed_ms": 500},
    {"kind": "cast_finish", "actor": second, "target": a, "cast": "11", "result": 1, "elapsed_ms": 600},
    {"kind": "cast_check", "actor": mage, "target": enemy, "result": 255, "elapsed_ms": 700},
    {"kind": "cast_check", "actor": mage, "target": enemy, "result": next(int(k) for k,v in reporter.CODES["SpellCastResult"].items() if v == "SPELL_FAILED_NOT_READY"), "elapsed_ms": 800},
    {"kind": "resource_spent", "actor": a, "amount": 100, "power_type": 1, "elapsed_ms": 900},
    {"kind": "decision", "actor": mage, "detail": "fireball | impossible", "elapsed_ms": 950},
    {"kind": "damage", "actor": mage, "target": enemy, "spell": 133, "amount": 10, "effective": 10, "periodic": True, "elapsed_ms": 1000},
    {"kind": "battle_end", "complete": True, "reason": "party_out_of_combat", "elapsed_ms": 2000},
]
report = reporter.analyze(events)
assert len(report["overlapping_heal_starts"]) == 1
assert len(report["failed_cast_checks"]) == 1 and report["failed_cast_checks"][0]["result"] == "SPELL_FAILED_NOT_READY"
chars = {r["guid"]: r for r in report["characters"]}
assert chars["1"]["rage_spent"] == 10
assert chars["3"]["dot_damage"] == 10 and chars["3"]["mana_spent"] == 0
assert report["decisions"][0]["action"] == "fireball"
assert not reporter.analyze(events[:-1])["complete"], "Missing trailer accepted"
assert not reporter.analyze(events, ["Sequence gap"])["complete"], "Corruption accepted"
with tempfile.TemporaryDirectory() as directory:
    p = Path(directory) / "bad.jsonl"
    p.write_text('{"schema":1,"seq":1,"elapsed_ms":0,"kind":"battle_start"}\n{"unfinished"', encoding="utf-8")
    ev, err = reporter.read_capture(p)
    assert err and not reporter.analyze(ev, err)["complete"]
print("Recorder output and battle report validation passed")
