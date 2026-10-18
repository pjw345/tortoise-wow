#include "BattleLog.h"
#include "BattleSessionRecorder.h"
#include "Objects/Player.h"
#include "Objects/Item.h"
#include "Group/Group.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Spells/Spell.h"
#include "Spells/SpellMgr.h"
#include "ScriptMgr.h"
#include "Config/Config.h"
#include "revision.h"
#include "Log.h"
#include "Movement/MotionMaster.h"
#include <atomic>
#include <fstream>

namespace BattleLog
{
namespace
{
std::atomic<bool> enabled{false};
std::atomic<Guid> nextCast{0};
std::atomic<uint32> playerFilter{0};
Recorder recorder;
std::string Clip(std::string const& value,size_t maximum)
{
    size_t end=std::min(value.size(),maximum);
    if(end<value.size()) while(end && (static_cast<unsigned char>(value[end])&0xC0)==0x80) --end;
    return value.substr(0,end);
}
Actor Snapshot(Unit* u)
{
    Actor a; if(!u) return a;
    a.guid=u->GetObjectGuid().GetRawValue(); a.name=u->GetName(); a.entry=u->GetEntry();
    a.player=u->IsPlayer(); a.characterClass=u->GetClass(); a.map=u->GetMapId(); a.instance=u->GetInstanceId();
    a.health=u->GetHealth(); a.maxHealth=u->GetMaxHealth();
    a.powerType=u->GetPowerType(); a.power=u->GetPower(u->GetPowerType()); a.maxPower=u->GetMaxPower(u->GetPowerType());
    a.mana=u->GetPower(POWER_MANA); a.maxMana=u->GetMaxPower(POWER_MANA);
    a.rage=u->GetPower(POWER_RAGE); a.maxRage=u->GetMaxPower(POWER_RAGE);
    a.alive=u->IsAlive(); a.combat=u->IsInCombat(); a.fleeing=u->HasUnitState(UNIT_STAT_FLEEING);
    auto motion=u->GetMotionMaster()->GetCurrentMovementGeneratorType();
    a.assistanceRunning=motion==ASSISTANCE_MOTION_TYPE||motion==ASSISTANCE_DISTRACT_MOTION_TYPE;
    a.unitState=u->GetUnitState(); a.unitFlags=u->GetUInt32Value(UNIT_FIELD_FLAGS);
    if(Spell* cast=u->GetCurrentSpell(CURRENT_GENERIC_SPELL)) a.casting=cast->m_spellInfo->Id;
    else if(Spell* channel=u->GetCurrentSpell(CURRENT_CHANNELED_SPELL)) a.casting=channel->m_spellInfo->Id;
    if(Spell* cast=u->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)) a.shooting=cast->m_spellInfo->Id;
    a.x=u->GetPositionX(); a.y=u->GetPositionY(); a.z=u->GetPositionZ();
    if(Unit* target=u->GetVictim())
    {
        a.target=target->GetObjectGuid().GetRawValue();
        if(target->FindMap()==u->FindMap() && target->IsInWorld())
        {
            a.targetSampleKnown=true; a.attacking=target->IsAlive();
            a.targetFleeing=IsRunner(target);
            a.targetX=target->GetPositionX();a.targetY=target->GetPositionY();a.targetZ=target->GetPositionZ();
        }
    }
    if(Player* p=u->ToPlayer()) { a.machine=Script_IsMachineDriven(p); a.selected=p->GetSelectionGuid().GetRawValue(); }
    else if(Player* owner=u->GetCharmerOrOwnerPlayerOrPlayerItself()) a.owner=owner->GetObjectGuid().GetRawValue();
    return a;
}
bool Relevant(Unit* u)
{
    if(!u) return false;
    if(recorder.Contains(u->GetObjectGuid().GetRawValue())) return true;
    Player* owner=u->GetCharmerOrOwnerPlayerOrPlayerItself();
    return owner && recorder.Contains(owner->GetObjectGuid().GetRawValue());
}
std::set<Guid> Roster(Player* p)
{
    std::set<Guid> result;
    if(Group* g=p->GetGroup()) for(auto const& member:g->GetMemberSlots()) result.insert(member.guid.GetRawValue());
    result.insert(p->GetObjectGuid().GetRawValue()); return result;
}
bool Open(Unit* actor, Unit* target, Event const& opening, bool hostileCast=false)
{
    Player* p=actor?actor->ToPlayer():nullptr;
    if(!p || (!hostileCast&&(!target||target==p||p->IsFriendlyTo(target))) || Script_IsMachineDriven(p) ||
        (playerFilter.load() && playerFilter.load()!=p->GetGUIDLow()) ) return false;
    std::set<Guid> roster=Roster(p); std::vector<Actor> initial;
    // No dereference of another map owner's player state, and no retained pointers.
    for(Guid id:roster)
    {
        Player* member=sObjectAccessor.FindPlayerNotInWorld(ObjectGuid(uint64(id)));
        if(!member) { Actor offline; offline.guid=id; offline.player=true; offline.name="offline member"; initial.push_back(offline); }
        else if(member->FindMap()==p->FindMap() && member->IsInWorld()) initial.push_back(Snapshot(member));
    }
    std::string revision=REVISION_HASH;
    std::ifstream source("/opt/turtle/SOURCE_COMMIT"); std::string full;
    if(source && std::getline(source,full) && full.size()==40) revision=full;
    if(!recorder.Start(Snapshot(p),std::move(roster),initial,opening,revision))
    {
        static std::atomic<unsigned long long> lastError{0};
        auto now=Recorder::Now(), previous=lastError.load();
        if(now-previous>60000 && lastError.compare_exchange_strong(previous,now))
            sLog.outError("Battle log could not open a session (I/O, retention or session limit). No gameplay change was made.");
        return false;
    }
    return true;
}
Event Make(char const* kind, Unit* actor, Unit* target, SpellEntry const* info=nullptr)
{
    Event e; e.kind=kind;
    if(actor&&target&&actor->FindMap()!=target->FindMap())
    {
        e.kind="scope_gap"; e.actor.guid=actor->GetObjectGuid().GetRawValue(); e.target.guid=target->GetObjectGuid().GetRawValue();
        e.detail="Cross-map unit state not read"; return e;
    }
    e.actor=Snapshot(actor); e.target=Snapshot(target);
    if(info) { e.spell=info->Id; e.spellName=info->SpellName[0]; e.wand=info->Category==SPELLCATEGORY_RANGED_WEAPON; }
    return e;
}
Event SpellEvent(Spell* s, char const* kind)
{
    Event e=Make(kind,s->GetCaster()->ToUnit(),s->m_targets.getUnitTarget(),s->m_spellInfo);
    e.cast=s->m_battleLogCastId; e.castMs=std::max(0,s->GetCastTime()); e.triggered=s->IsTriggered();
    e.item=s->m_battleLogItemId;
    e.hostile=e.kind!="scope_gap"&&!s->m_spellInfo->IsPositiveSpell(s->GetCaster()->ToUnit(),s->m_targets.getUnitTarget());
    if(e.item) if(ItemPrototype const* item=sObjectMgr.GetItemPrototype(e.item)) e.itemName=item->Name1;
    e.amount=s->GetPowerCost(); e.powerType=s->m_spellInfo->powerType;
    e.healing=s->m_spellInfo->IsHealSpell();
    return e;
}
}
bool Enabled() { return enabled.load(std::memory_order_relaxed); }
bool IsRunner(Unit* actor)
{
    if(!actor) return false;
    auto motion=actor->GetMotionMaster()->GetCurrentMovementGeneratorType();
    return actor->HasUnitState(UNIT_STAT_FLEEING)||motion==ASSISTANCE_MOTION_TYPE||motion==ASSISTANCE_DISTRACT_MOTION_TYPE;
}
void Configure()
{
    enabled.store(false);
    Settings s;
    std::string logs=sConfig.GetStringDefault("LogsDir","../logs");
    s.directory=std::filesystem::path(logs.empty()?"../logs":logs)/"battles";
    s.maxBytes=std::clamp(sConfig.GetIntDefault("BattleLog.MaxFileMiB",8),1,64)*1024u*1024u;
    s.maxEvents=std::clamp(sConfig.GetIntDefault("BattleLog.MaxEvents",50000),100,500000);
    s.maxFiles=std::clamp(sConfig.GetIntDefault("BattleLog.MaxFiles",100),1,1000);
    s.maxSessions=std::clamp(sConfig.GetIntDefault("BattleLog.MaxSessions",8),1,64);
    s.maxDurationMs=std::clamp(sConfig.GetIntDefault("BattleLog.MaxMinutes",30),1,240)*60000ull;
    s.enabled=sConfig.GetBoolDefault("BattleLog.Enabled",false);
    s.allowedLeaderLowGuid=sConfig.GetIntDefault("BattleLog.PlayerGuid",0);
    recorder.Configure(s);
    playerFilter.store(s.allowedLeaderLowGuid);
    enabled.store(s.enabled);
    if(Enabled()) sLog.outString("PARTY_BATTLE_LOG enabled=1 unsampled=1 directory=%s",s.directory.string().c_str());
}
void Shutdown() { enabled.store(false); recorder.CloseAll("server_shutdown"); }
void Attack(Unit* actor, Unit* target, bool humanIntent)
{
    if(!Enabled()) return;
    if(humanIntent && Open(actor,target,Make("attack_start",actor,target))) return;
    if(Relevant(actor)||Relevant(target)) recorder.Emit(Make("attack_start",actor,target));
}
void Combat(Unit* actor, Unit* target, bool entered)
{
    if(!Enabled() || (!Relevant(actor)&&!Relevant(target))) return;
    recorder.Emit(Make(entered?"combat_enter":"combat_exit",actor,target));
}
void Observe(Player* p)
{
    if(!Enabled()||!recorder.Contains(p->GetObjectGuid().GetRawValue())) return;
    if(!p->IsInWorld()||p->IsBeingTeleported()) { recorder.Depart(p->GetObjectGuid().GetRawValue(),"map_transition"); return; }
    recorder.Observe(Snapshot(p),Roster(p));
}
void Depart(Player* p,char const* reason)
{ if(Enabled()) recorder.Depart(p->GetObjectGuid().GetRawValue(),reason); }
void Cast(Spell* s,char const* phase,int result)
{
    if(!Enabled()||!s->GetCaster()) return;
    Unit* actor=s->GetCaster()->ToUnit(); Unit* target=s->m_targets.getUnitTarget();
    if(std::string(phase)=="cast_start")
    {
        if(!s->IsTriggered()&&s->m_isClientStarted&&actor&&actor->IsPlayer()&&
            !Script_IsMachineDriven(actor->ToPlayer())&&!s->m_spellInfo->IsPositiveSpell(actor,target))
        {
            if(!s->m_battleLogCastId) s->m_battleLogCastId=++nextCast;
            if(Item* item=s->GetCastItem()) s->m_battleLogItemId=item->GetEntry();
            Event opening=SpellEvent(s,phase);
            if(Open(actor,target,opening,true)) return;
        }
    }
    if(std::string(phase)=="cast_start"||std::string(phase)=="cast_attempt")
    {
        if(!Relevant(actor)&&!Relevant(target)) return;
        if(!s->m_battleLogCastId) s->m_battleLogCastId=++nextCast;
        if(Item* item=s->GetCastItem()) s->m_battleLogItemId=item->GetEntry();
    }
    if(!s->m_battleLogCastId) return;
    Event e=SpellEvent(s,phase); e.result=result; recorder.Emit(e);
}
void Resource(Spell* s,int type,uint32 before,uint32 after,uint32 maximum)
{
    if(!Enabled()||!s->m_battleLogCastId) return;
    Event e=SpellEvent(s,"resource_spent"); e.powerType=type;
    e.before=before; e.after=after; e.maximum=maximum; e.amount=static_cast<long long>(before)-after;
    recorder.Emit(e);
}
void ItemSpent(Spell* s,uint32 item,uint32 before,uint32 after)
{
    if(!Enabled()||!s->m_battleLogCastId) return;
    Event e=SpellEvent(s,"item_spent"); e.item=item; e.before=before; e.after=after; e.amount=before-after; recorder.Emit(e);
}
void Damage(Unit* actor,Unit* target,uint32 requested,uint32 effective,int type,SpellEntry const* info,CleanDamage const* clean,Spell* spell,bool targetWasFleeing)
{
    if(!Enabled()||(!Relevant(actor)&&!Relevant(target))) return;
    Event e=Make("damage",actor,target,info); e.amount=requested; e.effective=effective; e.periodic=type==DOT;
    e.targetWasFleeing=targetWasFleeing; e.damageType=type;
    if(clean)
    {
        e.attackType=int(clean->attackType);
        if(!info) e.spellName=clean->attackType==BASE_ATTACK?"Main-hand attack":clean->attackType==OFF_ATTACK?"Off-hand attack":"Ranged weapon attack";
    }
    if(clean) { e.absorbed=clean->absorb; e.resisted=clean->resist; e.outcome=clean->hitOutCome; e.critical=clean->hitOutCome==MELEE_HIT_CRIT; }
    if(spell) e.cast=spell->m_battleLogCastId;
    recorder.Emit(e);
}
void Heal(WorldObject* actor,Unit* target,uint32 amount,int32 effective,SpellEntry const* info,bool critical,bool periodic)
{
    Unit* unit=actor?actor->ToUnit():nullptr;
    if(!Enabled()||(!Relevant(unit)&&!Relevant(target))) return;
    Event e=Make("heal",unit,target,info); e.amount=amount; e.effective=std::max(0,effective); e.periodic=periodic; e.critical=critical; recorder.Emit(e);
}
void Threat(Unit* enemy,Unit* actor,float delta,float total)
{
    if(!Enabled()||!Relevant(actor)) return;
    Event e=Make("threat",actor,enemy); e.threat=delta; e.threatTotal=total; recorder.Emit(e);
}
void Movement(Unit* actor,Unit* target,float x,float y,float z,char const* reason)
{
    if(!Enabled()||!Relevant(actor)) return;
    Event e=Make("movement",actor,target); std::ostringstream out; out << reason << " destination=" << x << ',' << y << ',' << z; e.detail=out.str(); recorder.Emit(e);
}
void Decision(Unit* actor,Unit* target,std::string const& action,char const* result)
{
    if(!Enabled()||(!Relevant(actor)&&!Relevant(target))) return;
    Event e=Make("decision",actor,target); e.detail=Clip(action,256)+" | "+result; recorder.Emit(e);
}
void Check(Unit* actor,Unit* target,uint32 spell,int result)
{
    if(!Enabled()||(!Relevant(actor)&&!Relevant(target))) return;
    Event e=Make("cast_check",actor,target,sSpellMgr.GetSpellEntry(spell)); e.result=result; recorder.Emit(e);
}
void Miss(WorldObject const* actor,Unit* target,uint32 spell,int result)
{
    if(!Enabled()) return;
    Unit* unit=actor?const_cast<WorldObject*>(actor)->ToUnit():nullptr;
    if(!Relevant(unit)&&!Relevant(target)) return;
    Event e=Make("spell_miss",unit,target,sSpellMgr.GetSpellEntry(spell));e.result=result;recorder.Emit(e);
}
void PowerGain(WorldObject* actor,Unit* target,uint32 spell,int type,uint32 before,uint32 after,uint32 maximum)
{
    if(!Enabled()) return;
    Unit* unit=actor?actor->ToUnit():nullptr;
    if(!Relevant(unit)&&!Relevant(target)) return;
    Event e=Make("resource_gain",unit,target,sSpellMgr.GetSpellEntry(spell)); e.powerType=type;
    e.before=before;e.after=after;e.maximum=maximum;e.amount=static_cast<long long>(after)-before;recorder.Emit(e);
}
void Aura(Unit* target,uint32 spell,uint64 caster,bool apply,int32 duration)
{
    if(!Enabled()||!target) return;
    Unit* source=ObjectAccessor::GetUnit(*target,ObjectGuid(caster));
    if(!Relevant(source)&&!Relevant(target)) return;
    Event e=Make(apply?"aura_apply":"aura_remove",source,target,sSpellMgr.GetSpellEntry(spell));
    e.amount=duration;e.detail="caster_guid="+std::to_string(caster);recorder.Emit(e);
}
bool NeedsContext(Unit* actor)
{ return Enabled()&&actor&&recorder.NeedsContext(actor->GetObjectGuid().GetRawValue()); }
void Context(Unit* actor,std::string const& detail)
{
    if(!Enabled()||!Relevant(actor)) return;
    Event e=Make("bot_context",actor,nullptr);e.detail=Clip(detail,4096);recorder.Emit(e);
}
void PoolChange(Unit* actor,int type,uint32 before,uint32 after,uint32 maximum)
{
    if(!Enabled()||before==after||!Relevant(actor)) return;
    Event e=Make("resource_change",actor,nullptr);e.powerType=type;
    e.before=before;e.after=after;e.maximum=maximum;e.amount=static_cast<long long>(after)-before;recorder.Emit(e);
}
void AuraCost(Unit* actor,Unit* target,SpellEntry const* info,int type,uint32 before,uint32 after,uint32 maximum)
{
    if(!Enabled()||!Relevant(actor)) return;
    Event e=Make("resource_spent",actor,target,info);e.powerType=type;e.periodic=true;e.healing=info&&info->IsHealSpell();
    e.before=before;e.after=after;e.maximum=maximum;e.amount=static_cast<long long>(before)-after;recorder.Emit(e);
}
}
