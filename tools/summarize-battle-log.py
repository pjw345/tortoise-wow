#!/usr/bin/env python3
"""Summarize unsampled party battle JSONL. Does not contact or change a server."""
import argparse
import csv
from collections import Counter, defaultdict
from datetime import datetime, timezone
import json
from pathlib import Path

CLASSES = {1: "Warrior", 2: "Paladin", 3: "Hunter", 4: "Rogue", 5: "Priest", 7: "Shaman", 8: "Mage", 9: "Warlock", 11: "Druid"}
POWER = {-2: "health", 0: "mana", 1: "rage_raw", 2: "focus", 3: "energy", 4: "happiness"}
CODES = json.loads(Path(__file__).with_name("battle-log-result-codes.json").read_text(encoding="utf-8"))


def read_capture(path):
    if path.stat().st_size > 64 * 1024 * 1024:
        raise ValueError("Capture exceeds the supported 64 MiB limit")
    events, errors, expected = [], [], 1
    with path.open(encoding="utf-8", errors="strict") as stream:
        for line_number, line in enumerate(stream, 1):
            try:
                if len(line) > 1024 * 1024:
                    raise ValueError("Oversized event")
                def invalid(value):
                    raise ValueError("Non-finite JSON number: " + value)
                event = json.loads(line, parse_constant=invalid)
                if not isinstance(event, dict) or event.get("schema") != 1:
                    raise ValueError("Unsupported event/schema")
                if event.get("seq") != expected:
                    errors.append(f"Sequence gap at line {line_number}")
                expected = event.get("seq", expected) + 1
                if not isinstance(event.get("elapsed_ms"), (int, float)) or event["elapsed_ms"] < 0:
                    raise ValueError("Invalid elapsed time")
                events.append(event)
            except (json.JSONDecodeError, ValueError) as error:
                errors.append(f"Line {line_number}: {error}")
    return events, errors


def analyze(events, errors=None):
    errors = list(errors or [])
    start = events[0] if events and events[0].get("kind") == "battle_start" else {}
    end = events[-1] if events and events[-1].get("kind") == "battle_end" else {}
    if not start:
        errors.append("Missing battle_start")
    if not end:
        errors.append("Missing battle_end; capture is incomplete")
    roster = set(start.get("roster", []))
    duration = max((e.get("elapsed_ms", 0) for e in events), default=0) / 1000
    stats, actors = {}, {}
    skills = defaultdict(lambda: {"damage": 0, "healing": 0, "overheal": 0, "launches": 0, "mana": 0, "rage_raw": 0})
    checks = Counter()
    decisions, misses, cast_failures = Counter(), Counter(), Counter()
    item_names = {}
    contexts = {}
    contacts, overlaps, chases, target_differences, toward_runners, aggro = {}, [], [], [], [], []
    active_heals, samples, states = {}, {}, {}
    assistance = []

    def credited(actor):
        return actor.get("guid") if actor.get("guid") in roster else actor.get("owner") if actor.get("owner") in roster else None

    def account(actor):
        key = credited(actor)
        if not key:
            return None
        if key not in stats:
            stats[key] = {"guid": key, "damage": 0, "damage_taken": 0, "dot_damage": 0, "wand_damage": 0,
                          "pet_damage": 0, "healing": 0, "healing_others": 0, "self_healing": 0,
                          "healing_outside_party": 0, "raw_healing": 0, "overheal": 0, "self_or_friendly_damage_taken": 0,
                          "mana_spent": 0, "rage_raw_spent": 0, "other_resources": {}, "resource_gains": {}, "pool_gains": {}, "pool_losses": {},
                          "dps_spell_launches": 0, "heal_spell_launches": 0, "wand_launches": 0,
                          "interrupted_or_failed_finishes": 0, "items_cast": {}, "items_spent": {},
                          "threat_delta": 0, "deaths": 0, "mana_min_pct": None, "mana_max_pct": None,
                          "mana_start_pct": None, "mana_end_pct": None, "sampled_wand_seconds": 0,
                          "sampled_wand_above_50_seconds": 0}
        if actor.get("guid") == key:
            actors[key] = actor
            maximum = actor.get("max_mana", 0)
            if maximum:
                mana = 100 * actor.get("mana", 0) / maximum
                row = stats[key]
                row["mana_start_pct"] = mana if row["mana_start_pct"] is None else row["mana_start_pct"]
                row["mana_end_pct"] = mana
                row["mana_min_pct"] = min(row["mana_min_pct"] if row["mana_min_pct"] is not None else mana, mana)
                row["mana_max_pct"] = max(row["mana_max_pct"] if row["mana_max_pct"] is not None else mana, mana)
        return stats[key]

    for actor in start.get("party", []):
        account(actor)
    for e in events:
        kind, time = e.get("kind"), e["elapsed_ms"]
        if kind == "scope_gap":
            errors.append("Cross-map state was not read; native ownership preserved")
            continue
        if e.get("item") and e.get("item_name"):
            item_names[str(e["item"])] = e["item_name"]
        a, b = e.get("actor") or {}, e.get("target") or {}
        own, target = credited(a), credited(b)
        row, receiver = account(a), account(b)
        amount, effective = max(0, e.get("amount", 0)), max(0, e.get("effective", 0))
        spell_key = (own, e.get("spell", 0), e.get("spell_name", ""))
        # A new contact is an observation. It does not establish who pulled it.
        if kind in {"attack_start", "combat_enter", "damage", "threat", "spell_miss"}:
            enemy, party_actor = (b, a) if own and b.get("guid") not in roster else (a, b) if target and a.get("guid") not in roster else ({}, {})
            if enemy.get("guid") not in (None, "0") and not credited(enemy) and enemy.get("guid") not in contacts:
                contacts[enemy["guid"]] = {"guid": enemy["guid"], "name": enemy.get("name", "?"), "entry": enemy.get("entry", 0),
                                           "time_seconds": time / 1000, "party_actor": party_actor.get("name", "?"),
                                           "party_actor_guid": party_actor.get("guid"), "event": kind, "enemy_fleeing": bool(enemy.get("fleeing") or enemy.get("assistance_running"))}
        if kind == "damage":
            if row and not target:
                row["damage"] += effective
                row["dot_damage"] += effective if e.get("periodic") else 0
                row["wand_damage"] += effective if e.get("wand") else 0
                row["pet_damage"] += effective if a.get("owner") in roster else 0
                skills[spell_key]["damage"] += effective
                leader = e.get("leader") or {}
                selected = leader.get("selected")
                if leader.get("alive") and selected not in (None, "0") and selected not in roster and e.get("leader_sample_age_ms", 99999) <= 1000 and b.get("guid") != selected:
                    target_differences.append({"time_seconds": time / 1000, "actor": a.get("name"), "target": b.get("name"),
                                               "leader_selected": selected, "damage": effective, "periodic": bool(e.get("periodic")),
                                               "runner_exception": bool(e.get("target_was_fleeing", b.get("fleeing")))})
            if receiver:
                receiver["damage_taken"] += effective
                if own: receiver["self_or_friendly_damage_taken"] += effective
        elif kind == "heal" and row:
            overheal = max(0, amount - effective)
            if target:
                row["raw_healing"] += amount; row["healing"] += effective; row["overheal"] += overheal
                row["self_healing" if own == target else "healing_others"] += effective
            else:
                row["healing_outside_party"] += effective
            skills[spell_key]["healing"] += effective; skills[spell_key]["overheal"] += overheal
        elif kind == "resource_spent" and row:
            resource = POWER.get(e.get("power_type"), "power_" + str(e.get("power_type")))
            if resource == "mana":
                row["mana_spent"] += amount; skills[spell_key]["mana"] += amount
            elif resource == "rage_raw":
                row["rage_raw_spent"] += amount; skills[spell_key]["rage_raw"] += amount
            else:
                row["other_resources"][resource] = row["other_resources"].get(resource, 0) + amount
        elif kind == "resource_gain" and receiver:
            resource = POWER.get(e.get("power_type"), "power_" + str(e.get("power_type")))
            receiver["resource_gains"][resource] = receiver["resource_gains"].get(resource, 0) + amount
        elif kind == "resource_change" and row:
            resource = POWER.get(e.get("power_type"), "power_" + str(e.get("power_type")))
            delta = e.get("after", 0) - e.get("before", 0)
            field = "pool_gains" if delta >= 0 else "pool_losses"
            row[field][resource] = row[field].get(resource, 0) + abs(delta)
        elif kind == "bot_context" and row:
            contexts[own] = e.get("detail", "")
        elif kind == "cast_launch" and row:
            skills[spell_key]["launches"] += 1
            if e.get("healing") and not e.get("triggered"): row["heal_spell_launches"] += 1
            if e.get("wand"): row["wand_launches"] += 1
            elif e.get("hostile") and not e.get("triggered"): row["dps_spell_launches"] += 1
            if e.get("item") and not e.get("triggered"):
                item = str(e["item"]); row["items_cast"][item] = row["items_cast"].get(item, 0) + 1
            active_heals.pop(e.get("cast"), None)
        elif kind == "item_spent" and row:
            item = str(e.get("item")); row["items_spent"][item] = row["items_spent"].get(item, 0) + amount
        elif kind == "threat" and row:
            row["threat_delta"] += e.get("threat") or 0
        elif kind == "cast_check" and row and e.get("result") != 255:
            code = CODES["SpellCastResult"].get(str(e.get("result")), "CODE_" + str(e.get("result")))
            checks[(own, e.get("spell", 0), e.get("spell_name", ""), code)] += 1
        elif kind == "cast_result" and row and e.get("result") != 255:
            code = CODES["SpellCastResult"].get(str(e.get("result")), "CODE_" + str(e.get("result")))
            cast_failures[(own, e.get("spell", 0), e.get("spell_name", ""), code)] += 1
        elif kind == "spell_miss" and row:
            code = CODES["SpellMissInfo"].get(str(e.get("result")), "CODE_" + str(e.get("result")))
            misses[(own, e.get("spell", 0), e.get("spell_name", ""), code)] += 1
        elif kind == "cast_start" and row and e.get("healing") and not e.get("triggered"):
            for other in active_heals.values():
                if other["target"] == b.get("guid") and other["actor"] != own:
                    overlaps.append({"time_seconds": time / 1000, "target": b.get("name"), "first_healer": other["name"],
                                     "second_healer": a.get("name"), "spell": e.get("spell_name")})
            active_heals[e.get("cast")] = {"target": b.get("guid"), "actor": own, "name": a.get("name")}
        elif kind == "cast_finish":
            active_heals.pop(e.get("cast"), None)
            if row and e.get("result") != 0: row["interrupted_or_failed_finishes"] += 1
        elif kind == "decision" and row:
            detail = e.get("detail", "").rsplit(" | ", 1)
            if len(detail) == 2:
                decisions[(own, detail[0], detail[1])] += 1
                if detail[0] == "death": row["deaths"] += 1
        elif kind == "decision" and target and e.get("detail", "").startswith("threat_victim |"):
            aggro.append({"time_seconds": time / 1000, "enemy": a.get("name"), "enemy_guid": a.get("guid"),
                          "victim": b.get("name"), "victim_guid": target})
        elif kind == "decision" and target and e.get("detail", "").split(" | ", 1)[0] in {"flee_for_assistance", "call_assistance", "call_for_help"}:
            assistance.append({"time_seconds": time / 1000, "enemy": a.get("name"), "enemy_guid": a.get("guid"), "victim": b.get("name"), "detail": e.get("detail")})
        elif kind == "movement" and row and (b.get("fleeing") or b.get("assistance_running")):
            chases.append({"time_seconds": time / 1000, "actor": a.get("name"), "target": b.get("name"), "request": e.get("detail"),
                           "actor_guid": a.get("guid"), "blocked": e.get("detail", "").startswith("chase_blocked"),
                           "x": a.get("x"), "y": a.get("y"), "z": a.get("z")})
        if kind == "state" and row:
            if own in samples:
                previous_time, previous = samples[own]
                gap = time - previous_time
                if 0 < gap <= 2000 and previous.get("target_fleeing") and previous.get("target") == a.get("target"):
                    coords = [previous.get(k) for k in ("x", "y", "z", "target_x", "target_y", "target_z")] + [a.get(k) for k in ("x", "y", "z")]
                    if all(isinstance(v, (int, float)) for v in coords):
                        step = [a[k] - previous[k] for k in ("x", "y", "z")]
                        toward = [previous["target_" + k] - previous[k] for k in ("x", "y", "z")]
                        if sum(v*v for v in step) >= 4 and sum(v*w for v,w in zip(step,toward)) > 0:
                            toward_runners.append({"actor": a.get("name"), "actor_guid": own, "time_seconds": time / 1000,
                                                   "target_guid": a.get("target"), "moved_yards": sum(v*v for v in step)**.5})
                if 0 <= gap <= 2000 and previous.get("shooting") and previous.get("class") in {5, 8, 9}:
                    row["sampled_wand_seconds"] += gap / 1000
                    if previous.get("max_mana") and previous.get("mana", 0) / previous["max_mana"] > .5:
                        row["sampled_wand_above_50_seconds"] += gap / 1000
            samples[own] = time, a
            states[own] = a

    rows = []
    for guid, row in stats.items():
        meta = actors.get(guid, {})
        row.update(name=meta.get("name", guid), character_class=CLASSES.get(meta.get("class"), "Unknown"))
        row["dps"] = row["damage"] / duration if duration else None
        row["rage_spent"] = row["rage_raw_spent"] / 10
        row["overheal_pct"] = 100 * row["overheal"] / row["raw_healing"] if row["raw_healing"] else 0
        row["damage_per_mana"] = row["damage"] / row["mana_spent"] if row["mana_spent"] else None
        rows.append(row)
    names = {row["guid"]: row["name"] for row in rows}
    casts = [dict(actor=names.get(key[0], key[0]), spell=key[1], spell_name=key[2], **value) for key, value in skills.items() if key[0]]
    failed_checks = [dict(actor=names.get(key[0], key[0]), spell=key[1], spell_name=key[2], result=key[3], count=count) for key, count in checks.items()]
    decision_rows = [dict(actor=names.get(key[0], key[0]), action=key[1], outcome=key[2], count=count) for key, count in decisions.items()]
    miss_rows = [dict(actor=names.get(key[0], key[0]), spell=key[1], spell_name=key[2], result=key[3], count=count) for key, count in misses.items()]
    failed_results = [dict(actor=names.get(key[0], key[0]), spell=key[1], spell_name=key[2], result=key[3], count=count) for key, count in cast_failures.items()]
    return {"schema": 1, "revision": start.get("revision"), "wall_time_us": start.get("wall_time_us"), "leader": start.get("leader", {}).get("name"),
            "complete": bool(start and end.get("complete") and not errors), "end_reason": end.get("reason", "missing_end"),
            "duration_seconds": duration, "events": len(events), "errors": errors, "characters": rows, "spells": casts,
            "failed_cast_checks": failed_checks, "decisions": decision_rows, "spell_misses": miss_rows, "item_names": item_names,
            "failed_cast_results": failed_results,
            "bot_context": {names.get(k, k): v for k,v in contexts.items()},
            "enemy_first_contacts": list(contacts.values()), "overlapping_heal_starts": overlaps,
            "fleeing_movement_requests": chases, "damage_to_other_selected_targets": target_differences,
            "sampled_motion_toward_runners": toward_runners,
            "native_threat_victim_changes": aggro, "native_assistance_signals": assistance,
            "limits": ["Threat/contact/movement chronology does not prove which actor caused an add.",
                       "Overlapping heals can be appropriate; assess them with effective healing and overhealing.",
                       "Wand time is estimated from state samples; gaps above two seconds are excluded.",
                       "Damage per mana is a whole-battle ratio including free attacks, not per-spell causal attribution.",
                       "Periodic damage is not charged mana again. Native rage values are divided by ten for display.",
                       "Item casts and actual stack consumption are separate. SetPower changes include costs, gains, refunds and drains without guessing their cause; direct raw-field writes can bypass this hook.",
                       "Individual periodic/central healing events may lack a native cast pointer; no original cast ID is invented for them.",
                       "Only the requested window beginning with your attack is captured. Earlier aggro history is not reconstructed."]}


def markdown(report, source):
    def text(value): return str(value).replace("|", "\\|").replace("\n", " ").replace("\r", " ")
    def num(value): return "—" if value is None else f"{value:.1f}"
    lines = [f"# Battle: {text(report['leader'])}", "", f"Source: `{source}`", f"Revision: `{report['revision']}`", "",
             f"Capture: **{'complete' if report['complete'] else 'INCOMPLETE'}** ({report['end_reason']}); observed duration {report['duration_seconds']:.1f}s; {report['events']} events."]
    if report.get("wall_time_us"):
        lines += ["Started: " + datetime.fromtimestamp(report["wall_time_us"] / 1e6, timezone.utc).isoformat() + " (UTC)."]
    lines += ["", "## Characters", "", "| Character | Class | Damage | DPS | Taken | Effective healing | Overheal % | Mana spent | Rage spent | DPS spell launches | Sampled wand seconds (>50% mana) |", "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for row in report["characters"]:
        lines.append(f"| {text(row['name'])} | {row['character_class']} | {row['damage']} | {num(row['dps'])} | {row['damage_taken']} | {row['healing']} | {num(row['overheal_pct'])} | {row['mana_spent']} | {num(row['rage_spent'])} | {row['dps_spell_launches']} | {num(row['sampled_wand_seconds'])} ({num(row['sampled_wand_above_50_seconds'])}) |")
    paladin = sum(r["healing"] for r in report["characters"] if r["character_class"] == "Paladin")
    priest = sum(r["healing"] for r in report["characters"] if r["character_class"] == "Priest")
    paladin_others = sum(r["healing_others"] for r in report["characters"] if r["character_class"] == "Paladin")
    priest_others = sum(r["healing_others"] for r in report["characters"] if r["character_class"] == "Priest")
    lines += ["", f"Effective party healing: Paladin(s) {paladin} ({paladin_others} to other members); Priest(s) {priest} ({priest_others} to other members). This comparison does not infer assigned roles.", "", "## Resource pools and items", ""]
    for row in report["characters"]:
        def items(values):
            return ", ".join(f"{text(report['item_names'].get(k, 'item'))} ({k}) × {v}" for k,v in values.items()) or "none captured"
        lines.append(f"- {text(row['name'])}: mana {num(row['mana_start_pct'])}% → {num(row['mana_end_pct'])}%; observed range {num(row['mana_min_pct'])}–{num(row['mana_max_pct'])}%; item casts: {items(row['items_cast'])}; consumed: {items(row['items_spent'])}; explicit gains {row['resource_gains']}; deaths {row['deaths']}; healing outside party {row['healing_outside_party']}.")
        lines.append(f"  Recorded threat delta {num(row['threat_delta'])}; DoT damage {row['dot_damage']}; wand damage {row['wand_damage']}; whole-battle damage/mana {num(row['damage_per_mana'])}; self/friendly damage taken {row['self_or_friendly_damage_taken']}.")
        lines.append(f"  Actual pool gains {row['pool_gains']}; losses {row['pool_losses']}. These include non-cast changes and must not be added to spell costs again.")
    lines += ["", "## Observed bot roles and strategies", ""]
    lines += [f"- {text(name)}: {text(detail)}" for name,detail in report["bot_context"].items()]
    lines += ["", "## Spells and attacks", "", "| Character | Spell | Launches | Damage | Effective healing | Overheal | Mana | Rage (raw) |", "|---|---|---:|---:|---:|---:|---:|---:|---:|"]
    for row in report["spells"]:
        lines.append(f"| {text(row['actor'])} | {text(row['spell_name'] or 'Melee/other')} ({row['spell']}) | {row['launches']} | {row['damage']} | {row['healing']} | {row['overheal']} | {row['mana']} | {row['rage_raw']} |")
    lines += ["", "## Failed eligibility checks", ""]
    lines += [f"- {text(r['actor'])}: {text(r['spell_name'])} ({r['spell']}), {r['result']}, {r['count']} checks." for r in report["failed_cast_checks"]] or ["None captured."]
    lines += ["", "## Actual cast errors", ""]
    lines += [f"- {text(r['actor'])}: {text(r['spell_name'])} — {r['result']} × {r['count']}." for r in report["failed_cast_results"]]
    lines += ["", "## Bot action outcomes", ""]
    lines += [f"- {text(r['actor'])}: {text(r['action'])} — {r['outcome']} × {r['count']}." for r in report["decisions"]]
    lines += ["", "## Spell misses", ""]
    lines += [f"- {text(r['actor'])}: {text(r['spell_name'])} — {r['result']} × {r['count']}." for r in report["spell_misses"]]
    lines += ["", "## Coordination and additional enemies", "", f"Overlapping heal starts: {len(report['overlapping_heal_starts'])}. These are opportunities to inspect, not automatically wasted heals."]
    lines += [f"- {r['time_seconds']:.2f}s: {text(r['first_healer'])} and {text(r['second_healer'])} healing {text(r['target'])}." for r in report["overlapping_heal_starts"]]
    for r in report["enemy_first_contacts"]:
        lines.append(f"- First observed contact at {r['time_seconds']:.2f}s: {text(r['name'])} ({r['entry']}, GUID {r['guid']}), involving {text(r['party_actor'])}, event {r['event']}.")
    for r in report["native_threat_victim_changes"]:
        lines.append(f"- {r['time_seconds']:.2f}s: {text(r['enemy'])}'s native threat victim changed to {text(r['victim'])}.")
    for r in report["native_assistance_signals"]:
        lines.append(f"- {r['time_seconds']:.2f}s: {text(r['enemy'])}: {text(r['detail'])}; victim {text(r['victim'])}.")
    for r in report["fleeing_movement_requests"]:
        lines.append(f"- {r['time_seconds']:.2f}s: {text(r['actor'])} {'was blocked from moving toward' if r['blocked'] else 'requested movement involving'} fleeing {text(r['target'])}: {text(r['request'])}.")
    for r in report["sampled_motion_toward_runners"]:
        lines.append(f"- {r['time_seconds']:.2f}s: {text(r['actor'])} moved {r['moved_yards']:.1f} yards toward its previously sampled fleeing victim. This is observed direction, not proof of a chase or pull.")
    different = report["damage_to_other_selected_targets"]
    lines += [f"", f"Damage events differing from a recent, living leader's selected target: {len(different)} ({sum(bool(r['runner_exception']) for r in different)} involved fleeing targets). Raw events include the sampled leader target and sample age. Existing DoTs and launched projectiles can differ legitimately; this is not a count of targeting violations.", "", "## Interpretation limits", ""]
    lines += ["- " + line for line in report["limits"] + report["errors"]]
    return "\n".join(lines) + "\n"


def write_timeline(events, path):
    fields = ["elapsed_seconds", "kind", "actor", "actor_guid", "target", "target_guid", "spell", "spell_name", "cast_id",
              "damage_effective", "heal_effective", "heal_raw", "resource", "resource_spent_raw", "rage_spent",
              "resource_before_pct", "resource_after_pct", "actor_mana_pct_at_event", "actor_rage_pct_at_event",
              "item", "item_name", "item_consumed", "result", "detail"]
    def safe(value):
        return "'" + value if isinstance(value, str) and value.startswith(("=", "+", "-", "@")) else value
    def guid(value):
        return hex(int(value or 0))
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for e in events:
            a, b = e.get("actor") or {}, e.get("target") or {}
            kind, resource = e.get("kind"), POWER.get(e.get("power_type"), e.get("power_type"))
            result = e.get("result", "")
            if kind in {"cast_result", "cast_check"}: result = CODES["SpellCastResult"].get(str(result), result)
            elif kind == "spell_miss": result = CODES["SpellMissInfo"].get(str(result), result)
            row = {"elapsed_seconds": e["elapsed_ms"] / 1000, "kind": kind, "actor": a.get("name", ""), "actor_guid": guid(a.get("guid")),
                   "target": b.get("name", ""), "target_guid": guid(b.get("guid")), "spell": e.get("spell", ""), "spell_name": e.get("spell_name", ""), "cast_id": e.get("cast", ""),
                   "damage_effective": e.get("effective") if kind == "damage" else "", "heal_effective": e.get("effective") if kind == "heal" else "", "heal_raw": e.get("amount") if kind == "heal" else "",
                   "resource": resource if kind in {"resource_spent", "resource_gain"} else "", "resource_spent_raw": e.get("amount") if kind == "resource_spent" else "",
                   "rage_spent": e.get("amount", 0) / 10 if kind == "resource_spent" and e.get("power_type") == 1 else "",
                   "resource_before_pct": e.get("pool_before_pct"), "resource_after_pct": e.get("pool_after_pct"),
                   "actor_mana_pct_at_event": a.get("mana_pct"), "actor_rage_pct_at_event": a.get("rage_pct"),
                   "item": e.get("item", ""), "item_name": e.get("item_name", ""), "item_consumed": e.get("amount") if kind == "item_spent" else "", "result": result, "detail": e.get("detail", "")}
            writer.writerow({key: safe(value) for key,value in row.items()})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", type=Path, nargs="+", help="Battle JSONL files or directories")
    parser.add_argument("--output", type=Path, help="Output directory (default: summaries beside each input)")
    args = parser.parse_args()
    inputs = []
    for path in args.captures:
        inputs.extend(sorted(path.glob("battle-*.jsonl")) if path.is_dir() else [path])
    if len(inputs) > 1000:
        parser.error("At most 1000 captures per invocation")
    failed = False
    for path in inputs:
        try:
            events, errors = read_capture(path)
            report = analyze(events, errors)
            directory = args.output or path.parent / "summaries"
            directory.mkdir(parents=True, exist_ok=True)
            directory.joinpath(path.stem + ".summary.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            target = directory / (path.stem + ".summary.md")
            target.write_text(markdown(report, path.resolve()), encoding="utf-8")
            write_timeline(events, directory / (path.stem + ".events.csv"))
            print(target)
        except (OSError, ValueError, UnicodeError, TypeError, KeyError) as error:
            print(f"{path}: {error}"); failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
