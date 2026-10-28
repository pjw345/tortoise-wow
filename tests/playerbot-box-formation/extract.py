from pathlib import Path
import sys
core,out=map(Path,sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
source=(core/'modules/mod-playerbots/src/playerbot/strategy/values/Formations.cpp').read_text()
loader=Path(sys.argv[3]).read_text() if len(sys.argv)>3 and sys.argv[3] else source
if len(sys.argv)>4 and sys.argv[4]:source=Path(sys.argv[4]).read_text()
def block(text,marker):
 start=text.index(marker);end=text.index('{',start)+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
(out/'native-base.inc').write_text('\n'.join(block(source,m) for m in ['bool IsSameLocation(', 'bool Formation::IsNullLocation(', 'float Formation::GetAngle(', 'float Formation::GetOffset(', 'float Formation::GetFollowAngle(']))
(out/'native-formations.inc').write_text(block(source,'    class CircleFormation :')+';\n'+block(source,'    class BoxFormation :')+';\n')
(out/'native-loader.inc').write_text('\n'.join(block(loader,m) for m in ['FormationValue::FormationValue(', 'void FormationValue::Reset(', 'std::string FormationValue::Save(', 'bool FormationValue::Load(', 'bool SetFormationAction::Execute(']))
group=(core/'src/game/Group/Group.h').read_text()
size=next(x for x in group.splitlines() if x.startswith('#define MAX_GROUP_SIZE '))
(out/'native-group-size.inc').write_text(size+'\n')
assert 'supported.push_back("formation")' in (core/'modules/mod-playerbots/src/playerbot/strategy/generic/ChatCommandHandlerStrategy.cpp').read_text()

roles=(core/'modules/mod-playerbots/src/playerbot/PlayerbotAI.cpp').read_text()
strategy=(core/'modules/mod-playerbots/src/playerbot/strategy/Strategy.h').read_text()
assist=(core/'modules/mod-playerbots/src/playerbot/strategy/generic/TankAssistStrategy.h').read_text()
(out/'native-strategy-types.inc').write_text(block(strategy,'enum StrategyType')+';\n')
(out/'native-roles.inc').write_text('\n'.join(block(roles,m) for m in ['bool PlayerbotAI::IsRanged(', 'bool PlayerbotAI::IsTank(', 'bool PlayerbotAI::IsHeal('])+'\nstruct NativeTankAssistType : StrategyTypeProbe { '+block(assist,'int GetType()')+' };\n')
