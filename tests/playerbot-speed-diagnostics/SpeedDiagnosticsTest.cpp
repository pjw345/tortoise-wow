// Execute the real recalculation, setters and log bridge with deterministic
// aura, transport and file stand-ins. No server or production data required.
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
using MovementChangeType = int;
class Unit;
struct PlayerbotAI {};
struct MovementGenerator { int type; int GetMovementGeneratorType() const { return type; } };
struct Spline { bool Finalized() const { return false; } };
struct Creature;
class Unit
{
public:
    bool bot=true, mounted=false, pending=false, controlled=false, inWorld=true;
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
    bool HasPendingMovementChange(int) const { return pending; }
    bool IsMovedByPlayer() const { return controlled; }
    bool IsInWorld() const { return inWorld; }
    bool HasAuraState(int) const { return false; }
    void PropagateSpeedChange() { ++propagations; }
    template<class F> void CallForAllControlledUnits(F, int) {}
    void UpdateSpeed(UnitMoveType, bool, float=1, const char* ="unspecified");
    void SetSpeedRate(UnitMoveType, float);
    void SetSpeedRateReal(UnitMoveType, float, const char* ="direct");
};
struct Player : Unit { bool InBattleGround() const { return false; } };
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
int GetChangeTypeByMoveType(UnitMoveType type) { return int(type); }
void AddSpeedChangeToController(Unit* unit, UnitMoveType, float) { unit->pending=true; ++unit->packets; }
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
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool Contains(const std::string& text) { for (const auto& line : records) if (line.find(text)!=std::string::npos) return true; return false; }
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
        Check(unit.pending && unit.GetSpeedRate(MOVE_RUN)==1 && unit.packets==1,"controlled speed waits for native ACK");
        unit.SetSpeedRateReal(MOVE_RUN,.5f,"ResolvePendingMovementChange");
        Check(unit.GetSpeedRate(MOVE_RUN)==.5f && Contains("SPEED_APPLIED reason=ResolvePendingMovementChange"),"ACK application observable");
        unit.pending=false;
        int packets=unit.packets;
        unit.SetSpeedRate(MOVE_RUN,.5f);
        Check(unit.packets==packets,"unchanged request remains no-op");
        unit.pending=true;
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
        std::cout << "Speed diagnostics checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
