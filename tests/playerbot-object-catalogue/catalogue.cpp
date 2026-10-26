#include <cstdint>
#include <map>
#include <list>
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>
#include <iostream>
using uint32=uint32_t;using int32=int32_t;
struct GameObjectInfo{};
struct Manager {
 std::map<uint32,GameObjectInfo> m_GameObjectInfoMap;
 GameObjectInfo const* GetGameObjectInfo(uint32 id){auto p=m_GameObjectInfoMap.find(id);return p==m_GameObjectInfoMap.end()?nullptr:&p->second;}
 auto const& GetGameObjectInfoMap()const{return m_GameObjectInfoMap;}
};
struct LootStoreItem{uint32 itemid;};
struct LootLootGroupAccess{std::vector<LootStoreItem> ExplicitlyChanced,EqualChanced;};
struct LootTemplateAccess{std::vector<LootStoreItem> Entries;std::vector<LootLootGroupAccess> Groups;};
using DropMap=std::unordered_multimap<uint32,int32>;
constexpr int HIGHGUID_UNIT=1,HIGHGUID_GAMEOBJECT=2,LOOT_CORPSE=0;
struct ObjectGuid{int high;uint32 entry;ObjectGuid(int h,uint32 e,uint32):high(h),entry(e){}};
struct CreatureStore{uint32 GetMaxEntry(){return 0;}}sCreatureStorage;
DropMap itemDrops;DropMap* catalogue=nullptr;
DropMap* value(std::string const& name){return name=="drop map"?catalogue:&itemDrops;}
#define GAI_VALUE(type,name) value(name)
struct DropMapValue {
 std::map<uint32,LootTemplateAccess> templates;
 LootTemplateAccess const* GetLootTemplate(ObjectGuid guid,int){auto f=templates.find(guid.entry);return guid.high==HIGHGUID_GAMEOBJECT&&f!=templates.end()?&f->second:nullptr;}
 DropMap* Calculate();
};
struct EntryLootListValue {std::string qualifier;std::string getQualifier(){return qualifier;}std::list<uint32> Calculate();};
#include "native-catalogue.inc"
void check(bool condition,const char*msg){if(!condition)throw std::runtime_error(msg);}
int main(){
 sObjectMgr.m_GameObjectInfoMap={{42,{}},{300203,{}},{1000380,{}}};
 check(sGOStorage.LookupEntry<>(300203)&&!sGOStorage.LookupEntry<>(300204),"sparse lookup preserved");
 DropMapValue builder;builder.templates[42].Entries={{1205}};
 builder.templates[300203].Entries={{20516}};builder.templates[1000380].Entries={{51751}};
 catalogue=builder.Calculate();EntryLootListValue value;
 value.qualifier="-300203";check(value.Calculate()==std::list<uint32>{20516},"Apple Bob reaches real source catalogue");
 value.qualifier="-1000380";check(value.Calculate()==std::list<uint32>{51751},"highest template reaches source catalogue");
 value.qualifier="-42";check(value.Calculate()==std::list<uint32>{1205},"ordinary low entry preserved");
 value.qualifier="-300204";check(value.Calculate().empty(),"unknown sparse source stays unknown");
 check(sGOStorage.GetMaxEntry()==1000381,"exclusive native maximum includes custom templates");
 delete catalogue;
 sObjectMgr.m_GameObjectInfoMap[7000035]={};check(sGOStorage.GetMaxEntry()==7000036,"actual high custom range has no fixed cap");
 sObjectMgr.m_GameObjectInfoMap.clear();check(sGOStorage.GetMaxEntry()==0,"empty native store");
 std::cout<<"PASS: native maximum, sparse/custom entries, DropMap and EntryLootList pipeline\n";
}
