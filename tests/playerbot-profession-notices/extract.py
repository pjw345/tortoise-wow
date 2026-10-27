from pathlib import Path
import re,sys
core,out=map(Path,sys.argv[1:3]);out.mkdir(exist_ok=True,parents=True)
def block(text,marker):
 masked=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),text)
 start=text.index(marker);end=masked.index('{',start)+1;depth=1
 while depth:
  depth+=(masked[end]=='{')-(masked[end]=='}');end+=1
 return text[start:end]
player=(core/'src/game/Objects/Player.cpp').read_text()
for function in ['UpdateCraftSkill','UpdateGatherSkill','UpdateFishingSkill']:
 assert 'UpdateSkillPro(' in block(player,'bool Player::'+function+'(')
if len(sys.argv)>3 and sys.argv[3]:player=Path(sys.argv[3]).read_text()
(out/'native-update.inc').write_text(block(player,'bool Player::UpdateSkillPro('))
header=(core/'src/game/ScriptObjects.h').read_text()
enum=header[header.index('    PLAYERHOOK_ON_PLAYER_JUST_DIED,'):header.index('    PLAYERHOOK_END')]+ '    PLAYERHOOK_END\n'
(out/'native-hook.inc').write_text('enum PlayerHook {\n'+enum+'};\nstruct PlayerScript {\n'+block(header,'virtual void OnProfessionSkillGain(')+'\n};\n')
source=(core/'modules/mod-playerbots/src/playerbot/PlayerbotScripts.cpp').read_text()
(out/'native-bot-hook.inc').write_text(block(source,'void OnProfessionSkillGain('))
spells=(core/'src/game/Spells/SpellMgr.h').read_text()
(out/'native-professions.inc').write_text(block(spells,'inline bool IsPrimaryProfessionSkill(')+'\n'+block(spells,'inline bool IsProfessionSkill('))
defs=(core/'src/game/SharedDefines.h').read_text()
names=['SKILL_CATEGORY_PROFESSION','SKILL_FISHING','SKILL_COOKING','SKILL_FIRST_AID','SKILL_SURVIVAL2']
(out/'native-enums.inc').write_text('\n'.join('constexpr uint32 '+name+'='+re.search(r'^\s*'+name+r'\s*=\s*(\d+)',defs,re.M)[1]+';' for name in names)+'\n')

(out/'native-enable-hooks.inc').write_text(block(header,'inline void TortoiseEnableAllHooksIfEmpty('))
registry=(core/'src/game/ScriptMgr.h').read_text()
(out/'native-dispatch.inc').write_text('template<class Callback>\n'+block(registry,'static void ForEachEnabledHook('))
