import pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
file='modules/mod-playerbots/src/playerbot/strategy/actions/UseItemAction.cpp'
player=(root/'src/game/Objects/Player.cpp').read_text();start=player.index('    // remove start item if not need');end=player.index('    GiveQuestSourceItemIfNeed',start);native=player[start:end]
def function(s):
 start=s.index('bool UseAction::UseQuestGiverItem(');brace=s.index('{',start);depth=1;i=brace+1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
fixture=r'''
#include <map>
#include <string>
#include <stdexcept>
#include <iostream>
using uint32=unsigned; enum class PlayerbotSecurityLevel{PLAYERBOT_SECURITY_ALLOW_ALL};
const int TYPEMASK_ITEM=1;
struct Player;
struct ItemPrototype{uint32 StartQuest=373,ItemId=2874;};
struct Object{virtual ~Object()=default;virtual bool isType(int)const{return true;}virtual uint32 GetEntry()const{return 2874;}};
struct Item:Object{ItemPrototype proto;bool present=true;unsigned owner=1;const ItemPrototype* GetProto(){return &proto;}unsigned GetOwnerGuid(){return owner;}unsigned GetObjectGuid(){return 9;}unsigned GetBagSlot(){return 0;}unsigned GetSlot(){return 23;}};
struct Quest{uint32 ReqItemId[4]={2874,0,0,0};unsigned source=0;unsigned GetSrcItemId()const{return source;}unsigned GetQuestId()const{return 373;}};
struct Manager{Quest q;const Quest* GetQuestTemplate(unsigned){return &q;}}sObjectMgr;
struct Player{Item item;bool take=true,add=true,accepted=false,complete=false;Object* giver=nullptr;int destroys=0;bool CanTakeQuest(const Quest*,bool){return take&&!accepted;}bool CanAddQuest(const Quest*,bool){return add;}unsigned GetObjectGuid(){return 1;}bool HasItemCount(unsigned,unsigned){return item.present;}Item* GetItemByGuid(unsigned){return item.present?&item:nullptr;}void DestroyItem(unsigned,unsigned,bool){item.present=false;++destroys;}void DestroyItemCount(unsigned,unsigned,bool){item.present=false;++destroys;}void AddQuest(const Quest*pQuest,Object*questGiver){accepted=true;giver=questGiver;
NATIVE
}bool CanCompleteQuest(unsigned){return accepted&&(!sObjectMgr.q.ReqItemId[0]||item.present);}void CompleteQuest(unsigned){complete=true;}};
struct Chat{std::string formatQuest(const Quest*){return {};}};
struct AI{template<class...T>void TellPlayerNoFacing(T...) {}};
#define BOT_TEXT2(a,b) 0
struct UseAction{Player*bot;bool verbose=false;Chat*chat=nullptr;AI*ai=nullptr;bool UseQuestGiverItem(Player*,Item*);};
FUNCTION
void require(bool b){if(!b)throw std::runtime_error("regression assertion failed");}
int main(){
for(int mode=0;mode<3;++mode){sObjectMgr.q=Quest{};if(mode){sObjectMgr.q.ReqItemId[0]=0;if(mode==1)sObjectMgr.q.source=2874;}Player p;UseAction a{&p};require(a.UseQuestGiverItem(nullptr,&p.item));require(p.giver==&p.item);require(p.item.present==(mode!=2));require(p.destroys==(mode==2?1:0));require(p.complete);require(!a.UseQuestGiverItem(nullptr,&p.item));require(p.destroys==(mode==2?1:0));}
for(int mode=0;mode<5;++mode){sObjectMgr.q=Quest{};Player p;UseAction a{&p};if(mode==0)p.take=false;if(mode==1)p.add=false;if(mode==2)p.item.owner=2;if(mode==3)p.item.present=false;bool ok=a.UseQuestGiverItem(nullptr,mode==4?nullptr:&p.item);require(!ok&&!p.accepted&&!p.destroys);}
std::cout<<"PASS required starter, source starter, consumable starter, repeat, eligibility, ownership and missing-item cases\n";
}
'''
source=(root/file).read_text()
before=source.replace('bot->AddQuest(quest, item);','bot->AddQuest(quest, nullptr);').replace('    return true;\n}', '    bot->DestroyItemCount(proto->ItemId, 1, true);\n    return true;\n}',1)
for label,s in [('before',before),('after',source)]:
 cpp=fixture.replace('NATIVE',native).replace('FUNCTION',function(s));p=out/(label+'.cpp');p.write_text(cpp);exe=out/label
 subprocess.run(['c++','-std=c++17',str(p),'-o',str(exe)],check=True)
 r=subprocess.run([str(exe)],text=True,capture_output=True);print(label,'exit',r.returncode,r.stdout.strip(),r.stderr.strip())
 assert (r.returncode!=0) if label=='before' else (r.returncode==0)
