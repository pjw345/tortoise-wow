// Execute the real recalculation, setters and log bridge with deterministic
// aura, transport and file stand-ins. No server or production data required.
#include <algorithm>
#include <deque>
#include <map>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using uint32 = uint32_t;
using int32 = int32_t;
#include "SpeedDeclarations.inc"
enum UnitMoveType { MOVE_WALK, MOVE_RUN, MOVE_RUN_BACK, MOVE_SWIM, MOVE_SWIM_BACK, MOVE_TURN_RATE, MAX_MOVE_TYPE };
enum { SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED, SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS,
    SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK, SPELL_AURA_MOD_INCREASE_SPEED,
    SPELL_AURA_MOD_SPEED_ALWAYS, SPELL_AURA_MOD_SPEED_NOT_STACK,
    SPELL_AURA_MOD_INCREASE_SWIM_SPEED, SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED,
    SPELL_AURA_MOD_DECREASE_SPEED, UNIT_STAT_FOLLOW, CORPSE,
    CONFIG_FLOAT_GHOST_RUN_SPEED_BG, CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD,
    AURA_STATE_HEALTHLESS_5_PERCENT, AURA_STATE_HEALTHLESS_10_PERCENT,
    AURA_STATE_HEALTHLESS_15_PERCENT, CONTROLLED_PET=1, CONTROLLED_GUARDIANS=2,
    CONTROLLED_CHARM=4, CONTROLLED_MINIPET=8 };
constexpr float SPEED_REDUCTION_HP_5=.5f, SPEED_REDUCTION_HP_10=.6f, SPEED_REDUCTION_HP_15=.7f;
float baseMoveSpeed[MAX_MOVE_TYPE] = {2.5f,7,4.5f,4.7f,2.5f,3.14f};
enum MovementChangeType { INVALID, ROOT, WATER_WALK, SET_HOVER, FEATHER_FALL,
    SPEED_CHANGE_WALK, SPEED_CHANGE_RUN, SPEED_CHANGE_RUN_BACK, SPEED_CHANGE_SWIM,
    SPEED_CHANGE_SWIM_BACK, RATE_CHANGE_TURN, TELEPORT, KNOCK_BACK };
enum { MOVEFLAG_MASK_MOVING=1, MOVEFLAG_ROOT=2, MOVEFLAG_WATERWALKING=4, MOVEFLAG_HOVER=8, MOVEFLAG_SAFE_FALL=16 };
struct PlayerMovementPendingChange { MovementChangeType movementChangeType=INVALID; uint32 movementCounter=0; float newValue=0; bool apply=false; };
struct Player;
class Unit;
struct PlayerbotAI {};
struct MovementGenerator { int type; int GetMovementGeneratorType() const { return type; } };
struct Spline { bool Finalized() const { return false; } };
struct Creature;
class Unit
{
public:
    bool bot=true, mounted=false, controlled=false, inWorld=true;
    std::deque<PlayerMovementPendingChange> m_pendingMovementChanges;
    std::map<MovementChangeType,uint32> m_lastMovementChangeCounterPerType;
    uint32 m_movementCounter=0;
    uint32 GetMovementCounterAndInc() { return m_movementCounter++; }
    uint32 GetLastCounterForMovementChangeType(MovementChangeType type) { return m_lastMovementChangeCounterPerType[type]; }
    void PushPendingMovementChange(PlayerMovementPendingChange);
    bool HasPendingMovementChange(MovementChangeType) const;
    bool IsMovementChangeSuperseded(MovementChangeType,uint32);
    void ResolvePendingMovementChanges(bool,bool);
    void ResolvePendingMovementChange(const PlayerMovementPendingChange&,bool);
    bool FindPendingMovementSpeedChange(float,uint32,UnitMoveType);
    Player* ToPlayer();
    void RemoveUnitMovementFlag(int mask) { flags &= ~mask; }
    void SetRootedReal(bool) {}
    void SetWaterWalkingReal(bool) {}
    void SetHoverReal(bool) {}
    void SetFeatherFallReal(bool) {}
    void SendHeartBeat(bool) {}
    int slowAmount=0, normal=0, increase=0, mountedIncrease=0;
    uint32 flags=0x100, state=0x200;
    float m_speed_rate[MAX_MOVE_TYPE]={1,1,1,1,1,1};
    float persistence[MAX_MOVE_TYPE]={1,1,1,1,1,1};
    int propagations=0, packets=0, broadcasts=0, aiLookups=0, reads=0;
    MovementGenerator idle{0}, follow{14};
    std::vector<MovementGenerator*> motion{&idle,&follow};
    Spline spline;
    Spline* movespline=&spline;
    bool IsPlayer() const { return true; }
    bool IsCreature() const { return false; }
    Creature* ToCreature() { return nullptr; }
    bool HasUnitState(int) const { return false; }
    bool IsInCombat() const { return false; }
    bool IsMounted() const { return mounted; }
    Unit* GetCharmerOrOwner() { return nullptr; }
    int GetDeathState() const { return 0; }
    int GetMaxPositiveAuraModifier(int aura) { ++reads; return aura==SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED ? normal : aura==SPELL_AURA_MOD_INCREASE_SPEED ? increase : aura==SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED ? mountedIncrease : 0; }
    int GetMaxNegativeAuraModifier(int) { ++reads; return slowAmount; }
    int GetTotalAuraModifier(int) { return 0; }
    float GetTotalAuraMultiplier(int) { return 1; }
    float GetSpeedRate(UnitMoveType type) const { return m_speed_rate[type]; }
    float GetSpeedRatePersistance(UnitMoveType type) { return persistence[type]; }
    uint32 GetUnitMovementFlags() const { return flags; }
    uint32 GetUnitState() const { return state; }
    std::vector<MovementGenerator*>* GetMotionMaster() { return &motion; }
    bool IsMovedByPlayer() const { return controlled; }
    bool IsInWorld() const { return inWorld; }
    bool HasAuraState(int) const { return false; }
    void PropagateSpeedChange() { ++propagations; }
    template<class F> void CallForAllControlledUnits(F, int) {}
    void UpdateSpeed(UnitMoveType, bool, float=1, const char* ="unspecified");
    void SetSpeedRate(UnitMoveType, float);
    void SetSpeedRateReal(UnitMoveType, float, const char* ="direct");
};
struct Player : Unit {
    bool InBattleGround() const { return false; }
    bool IsBeingTeleportedNear() const { return false; }
    void ExecuteTeleportNear() {}
};
Player* Unit::ToPlayer() { return static_cast<Player*>(this); }
struct Creature : Unit
{
    struct Info { float speed_run=1, speed_walk=1; } info;
    struct Guid { bool IsPlayer() const { return false; } };
    bool IsPet() const { return false; }
    bool IsCharmed() const { return false; }
    bool IsWorldBoss() const { return false; }
    Info* GetCreatureInfo() { return &info; }
    Guid GetOwnerGuid() const { return {}; }
};
struct World { float getConfig(int) { return 1.5f; } } sWorld;
struct Log { template<class... T> void outError(const char*, T...) {} } sLog;
namespace MovementPacketSender {
MovementChangeType GetChangeTypeByMoveType(UnitMoveType type) { return static_cast<MovementChangeType>(SPEED_CHANGE_WALK+int(type)); }
void AddSpeedChangeToController(Unit* unit, UnitMoveType type, float rate) {
    PlayerMovementPendingChange change;
    change.movementChangeType=GetChangeTypeByMoveType(type);
    change.movementCounter=unit->GetMovementCounterAndInc();
    change.newValue=rate*baseMoveSpeed[type];
    unit->PushPendingMovementChange(change); ++unit->packets;
}
void SendMovementFlagChangeToAll(Unit*,int,bool) {}
void SendSpeedChangeToAll(Unit* unit, UnitMoveType, float) { ++unit->broadcasts; }
}
namespace ai { namespace botdiag {
bool enabled=false;
bool IsActionLogEnabled() { return enabled; }
}}
std::vector<std::string> records;
bool fileAvailable=true;
struct BotActionLog
{
    static FILE* GetHandle(PlayerbotAI*) { return fileAvailable ? stdout : nullptr; }
    static void Write(PlayerbotAI*, const char* tag, const char* fmt, ...)
    {
        if (!fileAvailable) return;
        char line[2048]; va_list args; va_start(args,fmt);
        vsnprintf(line,sizeof(line),fmt,args); va_end(args);
        records.push_back(std::string(tag)+" "+line);
    }
};
PlayerbotAI botAI;
PlayerbotAI* AiFor(Unit* unit) { if (!unit) return nullptr; ++unit->aiLookups; return unit->bot ? &botAI : nullptr; }
#include "SpeedLog.inc"
#include "SpeedRecalc.inc"
#include "SpeedSetters.inc"
#include "PendingSpeed.inc"
#include "SpeedMatch.inc"
void ApplyMatchedAck(Unit* pMover, UnitMoveType move_type, uint32 movementCounter, float speed) {
    if (!pMover->FindPendingMovementSpeedChange(speed, movementCounter, move_type)) return;
#include "SpeedAckGuard.inc"
    pMover->SetSpeedRateReal(move_type, speed/baseMoveSpeed[move_type]);
}
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool Contains(const std::string& text) { for (const auto& line : records) if (line.find(text)!=std::string::npos) return true; return false; }
void RunPendingSpeedRegressions();
int main()
{
    try {
        Player unit;
        BotActionLog_LogSpeed(&unit,"TEST",MOVE_RUN,1,.5f,false,1,"disabled");
        Check(records.empty() && unit.aiLookups==0 && unit.reads==0,"disabled path reads no bot state");
        ai::botdiag::enabled=true;
        unit.bot=false;
        BotActionLog_LogSpeed(&unit,"TEST",MOVE_RUN,1,.5f,false,1,"human");
        Check(records.empty() && unit.reads==0,"human must not log or scan auras");
        unit.bot=true;
        unit.slowAmount=-50;
        unit.UpdateSpeed(MOVE_RUN,true,10,"speed cheat");
        Check(std::fabs(unit.GetSpeedRate(MOVE_RUN)-5)<.001f,"slow multiplied once by cheat ratio");
        Check(Contains("SPEED_RECALC reason=speed cheat") && Contains("SPEED_RESULT reason=speed cheat") && Contains("slow=-50"),"recalculation context");
        Check(Contains("generators=[0,14]") && Contains("flags=0x100 state=0x200"),"full motion stack and flags");
        unit.motion.clear();
        BotActionLog_LogSpeed(&unit,"EMPTY",MOVE_RUN,5,5,false,1,"empty stack");
        Check(Contains("generators=[]"),"empty motion stack is safe");
        unit.motion={&unit.idle,&unit.follow};
        unit.UpdateSpeed(static_cast<UnitMoveType>(99), true, 1, "unsupported");
        Check(Contains("type=99"),"unsupported type is safely traced before native rejection");
        unit.slowAmount=0;
        unit.UpdateSpeed(MOVE_RUN,true,1,"aura removal");
        Check(unit.GetSpeedRate(MOVE_RUN)==1,"slow removal restores native rate");
        unit.controlled=true;
        unit.SetSpeedRate(MOVE_RUN,.5f);
        Check(unit.HasPendingMovementChange(SPEED_CHANGE_RUN) && unit.GetSpeedRate(MOVE_RUN)==1 && unit.packets==1,"controlled speed waits for native ACK");
        unit.SetSpeedRateReal(MOVE_RUN,.5f,"ResolvePendingMovementChange");
        Check(unit.GetSpeedRate(MOVE_RUN)==.5f && Contains("SPEED_APPLIED reason=ResolvePendingMovementChange"),"ACK application observable");
        unit.m_pendingMovementChanges.clear();
        int packets=unit.packets;
        unit.SetSpeedRate(MOVE_RUN,.5f);
        Check(unit.packets==packets,"unchanged request remains no-op");
        MovementPacketSender::AddSpeedChangeToController(&unit,MOVE_RUN,.5f);
        --unit.packets;
        unit.SetSpeedRate(MOVE_RUN,.5f);
        Check(unit.packets==packets+1,"pending request retains native resend");
        unit.mounted=true; unit.mountedIncrease=100; unit.slowAmount=-50;
        unit.UpdateSpeed(MOVE_RUN,true,1,"mounted");
        Check(unit.GetSpeedRate(MOVE_RUN)==1,"mounted bonus and slow retain native calculation");
        unit.mounted=false; unit.slowAmount=-100;
        unit.UpdateSpeed(MOVE_SWIM,true,1,"full snare");
        Check(std::fabs(unit.GetSpeedRate(MOVE_SWIM)-.01f)<.0001f,"native full-snare floor preserved");
        BotActionLog_LogSpeedAura(&unit,123,SPELL_AURA_MOD_DECREASE_SPEED,-50,false,"remove");
        Check(Contains("SPEED_AURA reason=remove spell=123") && Contains("amount=-50 apply=0"),"aura removal identity");
        auto count=records.size(); fileAvailable=false;
        unit.UpdateSpeed(MOVE_RUN,true,1,"file unavailable");
        Check(records.size()==count,"file failure does not emit records");
        Check(unit.flags==0x100 && unit.state==0x200 && unit.motion.size()==2,"diagnostics preserve movement state");
        RunPendingSpeedRegressions();
        std::cout << "Speed diagnostics and stale-speed regression checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

void RunPendingSpeedRegressions()
{
    ai::botdiag::enabled=false;
    Player snared;
    snared.controlled=true; snared.slowAmount=-50;
    snared.UpdateSpeed(MOVE_RUN,true,1,"Frostbolt apply");
    snared.controlled=false; snared.slowAmount=0;
    snared.UpdateSpeed(MOVE_RUN,true,1,"Frostbolt expiry during spline");
    snared.ResolvePendingMovementChanges(true,true);
    Check(snared.GetSpeedRate(MOVE_RUN)==1,"aura expiry stays restored after bulk pending resolution with logging disabled");
    // Alon's captured sequence: queued Frostbolt, direct aura-expiry
    // restoration during a spline, then native timeout or late ACK.
    for (int i=0; i<MAX_MOVE_TYPE; ++i) {
        Player victim; victim.bot=(i%2==0); auto type=static_cast<UnitMoveType>(i);
        victim.controlled=true; victim.SetSpeedRate(type,.5f);
        auto stale=victim.m_pendingMovementChanges.front();
        victim.controlled=false; victim.SetSpeedRateReal(type,1);
        auto counter=victim.m_movementCounter;
        victim.SetSpeedRateReal(type,1);
        Check(victim.m_movementCounter==counter,"repeated forced updates do not allocate counters");
        int broadcasts=victim.broadcasts;
        victim.ResolvePendingMovementChange(stale,true);
        Check(victim.GetSpeedRate(type)==1 && victim.broadcasts==broadcasts,"stale timeout cannot restore old speed or broadcast it");
        Check(!victim.FindPendingMovementSpeedChange(stale.newValue+1,stale.movementCounter,type),"wrong speed ACK remains rejected");
        Check(victim.m_pendingMovementChanges.size()==1,"mismatched ACK preserves pending record");
        ApplyMatchedAck(&victim,type,stale.movementCounter,stale.newValue);
        Check(victim.GetSpeedRate(type)==1 && victim.m_pendingMovementChanges.empty(),"late stale ACK consumed without changing restored speed");
        victim.controlled=true; victim.SetSpeedRate(type,.7f);
        auto current=victim.m_pendingMovementChanges.front();
        ApplyMatchedAck(&victim,type,current.movementCounter,current.newValue);
        Check(std::fabs(victim.GetSpeedRate(type)-.7f)<.0001f,"new legitimate ACK remains effective");
    }
    Player order; order.controlled=true;
    order.SetSpeedRate(MOVE_RUN,.5f); auto older=order.m_pendingMovementChanges.back();
    order.SetSpeedRate(MOVE_RUN,.8f); auto newer=order.m_pendingMovementChanges.back();
    ApplyMatchedAck(&order,MOVE_RUN,older.movementCounter,older.newValue);
    Check(order.GetSpeedRate(MOVE_RUN)==1,"out-of-order older ACK cannot apply");
    ApplyMatchedAck(&order,MOVE_RUN,newer.movementCounter,newer.newValue);
    Check(std::fabs(order.GetSpeedRate(MOVE_RUN)-.8f)<.0001f,"latest ordered ACK applies");
    Player independent; independent.controlled=true;
    independent.SetSpeedRate(MOVE_RUN,.5f);
    independent.SetSpeedRate(MOVE_SWIM,.6f);
    independent.SetSpeedRateReal(MOVE_RUN,1);
    independent.ResolvePendingMovementChanges(true,true);
    Check(independent.GetSpeedRate(MOVE_RUN)==1 && std::fabs(independent.GetSpeedRate(MOVE_SWIM)-.6f)<.0001f,"bulk resolution retains unrelated movement type");
    Player resend; resend.controlled=true; resend.SetSpeedRate(MOVE_RUN,.5f);
    resend.m_pendingMovementChanges.front().movementCounter=resend.GetMovementCounterAndInc();
    auto resent=resend.m_pendingMovementChanges.front();
    resend.ResolvePendingMovementChange(resent,false);
    Check(resend.GetSpeedRate(MOVE_RUN)==.5f,"native resent counter remains applicable");
}
