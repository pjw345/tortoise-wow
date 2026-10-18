from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
base=root/'modules/mod-playerbots/src/playerbot'
def block(text, marker):
    start=text.index(marker);opening=text.index('{',start);end=opening+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
ai=(base/'PlayerbotAI.cpp').read_text(encoding='utf-8')
methods=[block(ai,'bool PlayerbotAI::UsesCoordinatedPartyHealing(')]
methods += [block(ai,signature) for signature in ['uint32 PlayerbotAI::GetPartyHealingPressure(',
    'uint32 PlayerbotAI::GetPendingPartyHealing(', 'bool PlayerbotAI::IsPartyHealCovered(',
    'bool PlayerbotAI::IsBackupPartyHealAllowed(']]
triggers=(base/'strategy/triggers/HealthTriggers.cpp').read_text(encoding='utf-8')
methods += [block(triggers,'bool HealTargetFullHealthTrigger::IsActive(')]
actions=(base/'strategy/actions/GenericSpellActions.cpp').read_text(encoding='utf-8')
methods += [block(actions,marker) for marker in ['bool CastHealingSpellAction::isUseful(',
    'bool CastHealingSpellAction::Execute(', 'bool InterruptCurrentSpellAction::isUseful(',
    'bool InterruptCurrentSpellAction::Execute(']]
selector=(base/'strategy/values/PartyMemberToHeal.cpp').read_text(encoding='utf-8')
selection=block(selector,'    if (coordinated)')
methods += ['Unit* Select(PlayerbotAI* ai, Player* bot, std::vector<Unit*> needHeals) { bool coordinated=true;\n'+selection.replace('[this]','[&]')+'\nreturn nullptr;\n}']
# These are integration boundaries the isolated fake world does not execute.
reset=block(ai,'void PlayerbotAI::ResetStrategies(')
assert reset.index('sPlayerbotDbStore.Load(this)') < reset.index('reactionEngine->addStrategy("heal interrupt")')
wire=block(reset,'    if (HasActivePlayerMaster() && !IsRealPlayer() && !IsInPvp() &&')
for name in ['HasActivePlayerMaster','IsRealPlayer','IsInPvp','HasStrategy']:
    wire=wire.replace(name+'(', 'ai->'+name+'(')
methods += ['struct ReactionSetup { int added=0; void addStrategy(char const*) { ++added; } }; void WireInterrupt(PlayerbotAI* ai, ReactionSetup* reactionEngine) { '+wire+' }']

assert 'interrupt current spell::heal' in (base/'strategy/generic/CombatStrategy.cpp').read_text(encoding='utf-8')
assert 'coordinated || !IsTargetOfSpellCast(player, predicate)' in selector
assert 'if (!spell || spell->HasModifierApplied(mod))' in (root/'src/game/Objects/Player.cpp').read_text(encoding='utf-8')
assert 'SPELL_STATE_PREPARING' in triggers
assert 'Spell::CalculatePowerCost(info, member)' in ai
Path(sys.argv[1]).write_text('\n\n'.join(methods)+'\n',encoding='utf-8',newline='\n')
