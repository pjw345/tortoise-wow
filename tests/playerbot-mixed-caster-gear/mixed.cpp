#define MANGOSBOT_ZERO
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
using uint8=uint8_t;using uint32=uint32_t;using int32=int32_t;using int64=int64_t;
constexpr int CLASS_WARRIOR=1,CLASS_ROGUE=4,CLASS_HUNTER=3,CLASS_MAGE=8,CLASS_PRIEST=5,CLASS_DEATH_KNIGHT=6;
constexpr int MAX_ITEM_PROTO_STATS=10,MAX_ITEM_PROTO_DAMAGES=5,ITEM_CLASS_ARMOR=4,ITEM_CLASS_WEAPON=2,ITEM_SPEC_NONE=0;
using ItemSpecType=int;using Classes=int;
struct ItemPrototype{int Class=ITEM_CLASS_ARMOR,Armor=0,Block=0,Quality=0,ItemLevel=0,Delay=0;struct{uint32 ItemStatType=0;int32 ItemStatValue=0;}ItemStat[MAX_ITEM_PROTO_STATS];struct{float DamageMin=0,DamageMax=0;}Damage[MAX_ITEM_PROTO_DAMAGES];};
struct WeightScaleStat{std::string stat;int weight;};
struct WeightScale{std::vector<WeightScaleStat>stats;struct{std::string name;}info;};
struct Logger{template<class...T>void outDetail(T...){}}sLog;
struct RandomItemMgr{std::map<uint8,WeightScale>m_weightScales;std::map<std::string,uint32>weightStatLink{{"int",5},{"agi",3},{"str",4},{"spi",6},{"sta",7}};uint32 CalculateSingleStatWeight(uint8,uint8,std::string,int32);uint32 StatOnlyWeight(uint8,uint8,const ItemPrototype*);};
#include "native.inc"
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
ItemPrototype boots(){ItemPrototype p;p.Quality=2;p.ItemLevel=18;p.Armor=21;p.ItemStat[0]={5,2};p.ItemStat[1]={3,2};return p;}
ItemPrototype gray(){ItemPrototype p;p.ItemLevel=23;p.Armor=23;return p;}
int main(int argc,char**argv){try{std::string c=argc>1?argv[1]:"caster_boots";RandomItemMgr mgr;for(int i:{23,24,25})mgr.m_weightScales[i].stats={{"int",1},{"splpwr",7},{"spellhitrtng",100}};mgr.m_weightScales[18].stats={{"int",1},{"spi",2},{"splheal",7}};mgr.m_weightScales[1].stats={{"agi",2},{"str",3},{"mledps",5}};
 if(c=="caster_boots"){for(int i:{23,24,25}){auto a=boots(),b=gray();auto old=mgr.StatOnlyWeight(CLASS_MAGE,i,&a),fresh=mgr.StatOnlyWeight(CLASS_MAGE,i,&b);check(old==2,"useful Intellect must survive mixed physical-stat classification");check(fresh==0,"statless gray shoes have no mage weight");check(!IsBetterEquipment(fresh,old,&b,&a),"gray armor cannot displace useful caster stats");check(IsBetterEquipment(old,fresh,&a,&b),"green caster boots recover from gray shoes");}}
 else if(c=="plain_int"){auto p=boots();p.ItemStat[1]={};check(mgr.StatOnlyWeight(CLASS_MAGE,24,&p)==2,"plain Intellect unchanged");}
 else if(c=="mixed_strength"){auto p=boots();p.ItemStat[1]={4,2};check(mgr.StatOnlyWeight(CLASS_MAGE,24,&p)==2,"unused Strength does not erase Intellect");}
 else if(c=="healer"){auto p=boots();p.ItemStat[2]={6,2};check(mgr.StatOnlyWeight(CLASS_PRIEST,18,&p)==6,"healer weighted Intellect/Spirit retained");}
 else if(c=="pure_physical"){auto p=boots();p.ItemStat[0]={};check(mgr.StatOnlyWeight(CLASS_MAGE,24,&p)==0,"pure Agility remains irrelevant to mage");}
 else if(c=="no_mana"){auto p=boots();check(mgr.StatOnlyWeight(CLASS_WARRIOR,1,&p)==0,"native no-mana Intellect exclusion retained");}
 else if(c=="attacker"){auto p=boots();p.ItemStat[0]={};check(mgr.StatOnlyWeight(CLASS_WARRIOR,1,&p)==4,"physical-role Agility score unchanged");}
 else if(c=="statless"){auto a=gray(),b=gray();a.Armor=10;check(IsBetterEquipment(0,0,&b,&a),"statless armor improvement remains allowed");check(!IsBetterEquipment(0,0,&a,&b),"weaker statless armor remains rejected");}
 else throw std::runtime_error("unknown case");std::cout<<"PASS "<<c<<'\n';}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
