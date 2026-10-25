from pathlib import Path
import sys,re
r=Path(__file__).resolve().parents[2]; out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
def block(s,marker):
 a=s.index(marker);masked=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),s);b=masked.index('{',a);i=b+1;d=1
 while d:d+=(masked[i]=='{')-(masked[i]=='}');i+=1
 return s[a:i]
loot=(r/'src/game/LootMgr.cpp').read_text(); header=(r/'src/game/LootMgr.h').read_text()
parts=[block(loot,x) for x in ['bool Loot::HasQuestLootForPlayer(','QuestItemList* Loot::FillQuestLoot(','LootItem* Loot::LootItemInSlot(','uint32 Loot::GetMaxSlotInLootFor(']]
parts.append(block(header,'    bool isLooted() const'))
# The actual inline global exhaustion predicate belongs inside the fake class.
(out/'Exhaustion.inc').write_text(parts.pop())
stack=(r/'modules/mod-playerbots/src/playerbot/LootObjectStack.cpp').read_text()
parts += [block(stack,'bool ai::CanOpenActivatedQuestChest('),block(stack,'    Player* GetPersonalQuestChestTurn(')]
release=(r/'src/game/Handlers/LootHandler.cpp').read_text();release=release[release.index('void WorldSession::DoLootRelease('):]
go=block(release,'        case HIGHGUID_GAMEOBJECT:');go=go[go.index('{'):]
parts.append('void WorldSession::Release(){Player* player=_player;Loot* loot=nullptr;int lguid=1;switch(1){case 1:'+go+'}}')
take=release # storage is in a different method; keep the exact successful-take flags/count fragment.
handler=(r/'src/game/Handlers/LootHandler.cpp').read_text();a=handler.index('        if (qitem)\n');b=handler.index('        --loot->unlootedCount;',a)+len('        --loot->unlootedCount;')
parts.append('void Take(Loot* loot,Player* player,uint32 lootSlot){QuestItem *qitem=nullptr,*ffaitem=nullptr,*conditem=nullptr;LootItem* item=loot->LootItemInSlot(lootSlot,player->GetGUIDLow(),&qitem,&ffaitem,&conditem);if(!item||!item->AllowedForPlayer(player,loot->GetLootTarget()))throw std::runtime_error("invalid take");'+handler[a:b]+'}')
game=(r/'src/game/Objects/GameObject.cpp').read_text();a=game.index('        case GO_JUST_DEACTIVATED:'); section=game[a:]
restock=block(section,'                case GAMEOBJECT_TYPE_CHEST:');restock=restock[restock.index('{'):].replace('                    break;','')
a=section.index('            loot.clear();');b=section.index('            JustDespawnedWaitingRespawn();',a)+len('            JustDespawnedWaitingRespawn();')
parts.append('void GameObject::TickDeactivated(){if(GetGoType()==GAMEOBJECT_TYPE_CHEST)'+restock+section[a:b]+'}')
(out/'Native.inc').write_text('\n'.join(parts))
# Old adapter must fail the shared-empty-crate regression with unchanged fixtures.
old='\n'.join(parts).replace(' &&\n        go->loot.HasQuestLootForPlayer(bot)','').replace(' ||\n                (go->getLootState() == GO_ACTIVATED && !go->loot.HasQuestLootForPlayer(groupMember))','')
(out/'Before.inc').write_text(old)
