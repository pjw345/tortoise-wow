#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
using uint32=uint32_t;
#include "NativeMovementFlags.inc"
#include "NativeUnitState.inc"
constexpr int SPELL_ATTR_EX3_HIDE_CHANNEL_BAR=1,EFFECT_INDEX_0=0,SPELL_EFFECT_STUCK=84;
constexpr int SPELL_INTERRUPT_FLAG_MOVEMENT=1,AURA_INTERRUPT_FLAG_MOVE=2;
struct MovementInfo{uint32 flags=0;bool HasMovementFlag(uint32 mask)const{return (flags&mask)!=0;}};
struct Unit
{
    MovementInfo m_movementInfo;uint32 state=0;bool falling=false,jumping=false;
    bool HasUnitState(uint32 mask)const{return (state&mask)!=0;}
    bool IsFalling(){return falling;}
    bool IsJumping(){return jumping;}
    bool IsPlayer(){return true;}
    #include "NativeStopped.inc"
    #include "NativeObjectMoving.inc"
};
using Player=Unit;
struct ServerFacade{static constexpr uint32 movementFlagsMask=MOVEFLAG_MASK_MOVING;bool isMoving(Unit*);};
#include "NativeMoving.inc"
ServerFacade sServerFacade;
void Check(bool value,char const* why){if(!value)throw std::runtime_error(why);}
struct SpellInfo
{
    int castTime=5000,duration=0;bool channel=false;int Effect[1]={0};
    bool HasAttribute(int){return false;}
    bool HasSpellInterruptFlag(int flag){return flag==SPELL_INTERRUPT_FLAG_MOVEMENT;}
    bool HasAuraInterruptFlag(int){return false;}
    bool HasChannelInterruptFlag(int){return false;}
};
struct Spell{bool cancelled=false;void cancel(){cancelled=true;}};
int GetSpellCastTime(SpellInfo const* info,Unit*,Spell*){return info->castTime;}
bool IsChanneledSpell(SpellInfo const* info){return info->channel;}
int GetSpellDuration(SpellInfo const* info){return info->duration;}
struct{uint32 globalCoolDown=1500;}sPlayerbotAIConfig;
struct CastPreparation
{
    Unit* bot;bool playerMaster=true,waitForSpell=true;int stops=0,delay=0;
    bool IsJumping(){return bot->IsJumping();}
    bool HasActivePlayerMaster(){return playerMaster;}
    void StopMoving(){++stops;bot->m_movementInfo.flags=0;bot->state=0;}
    void SetAIInternalUpdateDelay(uint32 value){delay=value;}
    bool Prepare(SpellInfo const* pSpellInfo,Spell* spell)
    {
        #include "NativeCastStop.inc"
        return true;
    }
};
bool MovementInterruptApplies(Unit* m_caster,SpellInfo* m_spellInfo,float moved)
{
    int m_timer=1;
    float m_castPositionX=0,m_castPositionY=0,m_castPositionZ=0;
    float nowPosX=moved,nowPosY=0,nowPosZ=0;
    return
        #include "NativeInterruptCondition.inc"
    ;
}
int main(int argc,char** argv)
{
    try
    {
        Unit moving;moving.m_movementInfo.flags=0x400001;moving.state=0;
#ifdef MANGOSBOT_ZERO
        if(argc>1&&std::string(argv[1])=="--probe")
        {
            bool facade=sServerFacade.isMoving(&moving);
            SpellInfo herb;Spell spell;CastPreparation cast{&moving};cast.Prepare(&herb,&spell);
            std::cout<<"{\"native_moving\":true,\"legacy_stopped\":true,\"facade_moving\":"<<(facade?"true":"false")
                <<",\"stop_calls\":"<<cast.stops<<",\"native_movement_interrupt_applies\":"
                <<(MovementInterruptApplies(&moving,&herb,moving.IsMoving()?0.7f:0.f)?"true":"false")<<"}\n";
            return 0;
        }
#endif
        Check(moving.IsMoving()&&moving.IsStopped(),"captured native flags/state do not disagree");
#if defined(MANGOSBOT_ZERO) || defined(MANGOS)
        Check(sServerFacade.isMoving(&moving),"live forward spline must count as moving");
#else
        Check(!sServerFacade.isMoving(&moving),"other expansion predicate changed");
#endif
        Unit idle;Check(!sServerFacade.isMoving(&idle),"stationary player marked moving");
        idle.state=UNIT_STAT_MOVING;
#if defined(MANGOSBOT_ZERO) || defined(MANGOS)
        Check(!sServerFacade.isMoving(&idle),"stale generator state overrides native stop");
#else
        Check(sServerFacade.isMoving(&idle),"legacy moving state changed outside Turtle");
#endif
        idle.state=0;idle.falling=true;
#ifdef CMANGOS
        Check(sServerFacade.isMoving(&idle),"falling behavior lost");
#else
        Check(!sServerFacade.isMoving(&idle),"MaNGOS predicate changed");
#endif
        idle.falling=false;idle.jumping=true;
#ifdef MANGOSBOT_ONE
        Check(sServerFacade.isMoving(&idle),"TBC jumping behavior lost");
#else
        Check(!sServerFacade.isMoving(&idle),"jump predicate changed outside TBC");
#endif
#ifdef MANGOSBOT_ZERO
        SpellInfo herb;Spell spell;CastPreparation cast{&moving};
        Check(cast.Prepare(&herb,&spell),"eligible cast preparation rejected");
        Check(cast.stops==1&&!moving.IsMoving(),"native cast preparation skipped stopping active spline");
        Check(MovementInterruptApplies(&moving,&herb,0.7f),"native movement-cancel threshold not exercised");
        Check(!MovementInterruptApplies(&moving,&herb,0),"stopped cast still movement-cancelled");
        // Jump/fall and unattended-bot behavior come from the real cast guard.
        moving.m_movementInfo.flags=1;moving.jumping=true;Spell jump;CastPreparation jumping{&moving};
        Check(!jumping.Prepare(&herb,&jump)&&jump.cancelled&&jumping.stops==0,"jumping cast behavior changed");
        moving.jumping=false;moving.falling=true;Spell fall;CastPreparation falling{&moving};
        Check(!falling.Prepare(&herb,&fall)&&fall.cancelled,"falling cast was admitted");
        moving.falling=false;moving.m_movementInfo.flags=1;Spell unattended;CastPreparation noMaster{&moving};noMaster.playerMaster=false;
        Check(!noMaster.Prepare(&herb,&unattended)&&unattended.cancelled&&noMaster.delay==1500,"unattended recovery changed");
        moving.m_movementInfo.flags=1;SpellInfo instant;instant.castTime=0;Spell instantSpell;CastPreparation fast{&moving};
        Check(fast.Prepare(&instant,&instantSpell)&&fast.stops==0,"instant cast now stops movement");
        instant.channel=true;instant.duration=2000;Spell channel;CastPreparation channeled{&moving};
        Check(channeled.Prepare(&instant,&channel)&&channeled.stops==1,"channeled cast skips native stop");
#endif
        std::cout<<"PASS: captured forward spline, native stop/cancel guards, idle/falling/jump and retained expansion predicate\n";
    }
    catch(std::exception const& error){std::cerr<<error.what()<<'\n';return 1;}
}
