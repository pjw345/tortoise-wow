from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
def block(text,marker):
 start=text.index(marker);end=text.index('{',start)+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
commands=Path(sys.argv[2]).read_text() if len(sys.argv)>2 and sys.argv[2] else (root/'src/game/Commands/Commands.cpp').read_text()
(out/'native.inc').write_text(block(commands,'bool ChatHandler::HandleTeleDelCommand(')+'\n'+block((root/'src/game/ObjectMgr.cpp').read_text(),'bool ObjectMgr::DeleteGameTele('))
chat=(root/'src/game/Chat/Chat.cpp').read_text();table=chat[chat.index('static ChatCommand teleCommandTable'):chat.index('static ChatCommand triggerCommandTable')]
assert '"del",            SEC_DEVELOPER,     true' in table
