from pathlib import Path
import sys
core,out=map(Path,sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
def block(text,marker):
 start=text.index(marker);end=text.index('{',start)+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
trade=Path(sys.argv[3]).read_text() if len(sys.argv)>3 and sys.argv[3] else (core/'modules/mod-playerbots/src/playerbot/strategy/actions/TradeAction.cpp').read_text()
(out/'native-trade.inc').write_text(block(trade,'bool TradeAction::Execute(')+'\n'+block(trade,'bool TradeAction::TradeItem('))
native=(core/'src/game/Handlers/TradeHandler.cpp').read_text()
handlers=['HandleBeginTradeOpcode','HandleSetTradeItemOpcode','HandleClearTradeItemOpcode','HandleSetTradeGoldOpcode']
(out/'native-handlers.inc').write_text('\n'.join(block(native,'void WorldSession::'+name+'(') for name in handlers))
(out/'native-set-item.inc').write_text(block((core/'src/game/Objects/Player.cpp').read_text(),'void TradeData::SetItem('))
chat=(core/'modules/mod-playerbots/src/playerbot/strategy/generic/ChatCommandHandlerStrategy.cpp').read_text()
assert '"nt",\n        NextAction::array(0, new NextAction("trade", relevance)' in chat
