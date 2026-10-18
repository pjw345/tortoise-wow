#pragma once
#include "Common.h"
class Unit;
class Player;
class WorldObject;
class Spell;
struct SpellEntry;
struct CleanDamage;
namespace BattleLog
{
bool Enabled();
bool IsRunner(Unit* actor);
void Configure();
void Shutdown();
void Attack(Unit* actor, Unit* target, bool humanIntent=false);
void Combat(Unit* actor, Unit* target, bool entered);
void Observe(Player* player);
void Depart(Player* player, char const* reason);
void Cast(Spell* spell, char const* phase, int result=-1);
void Resource(Spell* spell, int type, uint32 before, uint32 after, uint32 maximum);
void ItemSpent(Spell* spell, uint32 item, uint32 before, uint32 after);
void Damage(Unit* actor, Unit* target, uint32 requested, uint32 effective, int type,
    SpellEntry const* info, CleanDamage const* clean, Spell* spell, bool targetWasFleeing);
void Heal(WorldObject* actor, Unit* target, uint32 amount, int32 effective,
    SpellEntry const* info, bool critical, bool periodic=false);
void Threat(Unit* enemy, Unit* actor, float delta, float total);
void Movement(Unit* actor, Unit* target, float x, float y, float z, char const* reason);
void Decision(Unit* actor, Unit* target, std::string const& action, char const* result);
void Check(Unit* actor, Unit* target, uint32 spell, int result);
void Miss(WorldObject const* actor, Unit* target, uint32 spell, int result);
void PowerGain(WorldObject* actor, Unit* target, uint32 spell, int type, uint32 before, uint32 after, uint32 maximum);
void Aura(Unit* target, uint32 spell, uint64 caster, bool apply, int32 duration);
bool NeedsContext(Unit* actor);
void Context(Unit* actor, std::string const& detail);
void PoolChange(Unit* actor, int type, uint32 before, uint32 after, uint32 maximum);
void AuraCost(Unit* actor, Unit* target, SpellEntry const* info, int type, uint32 before, uint32 after, uint32 maximum);
}
