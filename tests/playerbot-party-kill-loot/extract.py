from pathlib import Path
import re,sys
core,out=map(Path,sys.argv[1:3]);out.mkdir(exist_ok=True,parents=True)
root=core/'modules/mod-playerbots/src/playerbot'
def block(text,marker):
 masked=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),text)
 start=text.index(marker);end=masked.index('{',start)+1;depth=1
 while depth:depth+=(masked[end]=='{')-(masked[end]=='}');end+=1
 return text[start:end]
source=(root/'strategy/actions/AddLootAction.cpp').read_text()
(out/'native-action.inc').write_text(block(source,'bool PartyKillLootAction::Execute('))
header=(root/'strategy/actions/AddLootAction.h').read_text()
(out/'native-action.h').write_text(block(header,'class PartyKillLootAction')+';\n')
source=(root/'PlayerbotAI.cpp').read_text() if len(sys.argv)<4 or not sys.argv[3] else Path(sys.argv[3]).read_text()
line=next(x for x in source.splitlines() if 'AddHandler(SMSG_LOG_XPGAIN,' in x)
party=re.search(r'#ifdef MANGOSBOT_ZERO\n[^\n]*\n\s*botOutgoingPacketHandlers.AddHandler\(SMSG_PARTYKILLLOG, "party kill", true\);\n#endif',source)
(out/'native-binding.inc').write_text(line+'\n'+(party[0] if party else '')+'\n')
for file,needle in [
 ('strategy/actions/ActionContext.h','creators["party kill"] = [](PlayerbotAI* ai) { return new PartyKillLootAction(ai); };'),
 ('strategy/triggers/WorldPacketTriggerContext.h','creators["party kill"] = [](PlayerbotAI* ai) { return new WorldPacketTrigger(ai, "party kill"); };'),
 ('strategy/generic/WorldPacketHandlerStrategy.cpp','supported.push_back("party kill");')]:
 text=(root/file).read_text();pos=text.index(needle)
 assert '#ifdef MANGOSBOT_ZERO' in text[max(0,pos-90):pos]
 assert '#endif' in text[pos:pos+len(needle)+12]
native=(core/'src/game/Objects/Unit.cpp').read_text();start=native.index('WorldPacket data(SMSG_PARTYKILLLOG')
wire=native[start:start+300];assert 'data << pPlayerTap->GetObjectGuid()' in wire and 'data << pVictim->GetObjectGuid()' in wire
