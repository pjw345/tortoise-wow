-- Lua 5.0, matching the actual client language/runtime.
local root=arg[1] or "."
local rods={6218,6339,11130,11145,16207}
local names={"Runed Copper Rod","Runed Silver Rod","Runed Golden Rod","Runed Truesilver Rod","Runed Arcanite Rod"}
local bagItem,bankItem,selection,originalFocus,focusHasTool
local calls,errors,lastIndex,lastCount=0,0
WarbandEnchantingRodsData={version=1,rods=rods,rodNames=names,recipes={},byName={}}
for i=1,5 do
  WarbandEnchantingRodsData.recipes[i]={i}
  WarbandEnchantingRodsData.byName["Enchant "..i.."\031"] = i
end
GetCraftItemLink=function(index)return "|Henchant:"..index.."|h[Test]|h" end
GetCraftInfo=function(index)return "Enchant "..index,"","optimal" end
GetCraftSelectionIndex=function()return selection end
GetItemInfo=function(item)for i=1,5 do if rods[i]==item then return names[i] end end end
GetContainerNumSlots=function(bag)return 1 end
GetContainerItemLink=function(bag,slot)
  local item=bag==0 and bagItem or bag==5 and bankItem
  return item and "|Hitem:"..item..":0:0:0|h[Rod]|h"
end
GetCraftSpellFocus=function(index)
  if originalFocus then return originalFocus,focusHasTool end
  return
end
DoCraft=function(index,count)calls=calls+1;lastIndex=index;lastCount=count;return "native",17 end
UIErrorsFrame={AddMessage=function()errors=errors+1 end}
DEFAULT_CHAT_FRAME={AddMessage=function(self,text)self.text=text end}
CreateFrame=function()return {RegisterEvent=function()end,SetScript=function()end} end
SlashCmdList={}
dofile(root.."/WarbandEnchantingRods/WarbandEnchantingRods.lua")
local function check(value,why)if not value then error(why) end end
for required=1,5 do
 for carried=1,5 do
  bagItem=rods[carried];local before=calls
  local a,b=DoCraft(required,2)
  check(calls==before+(carried>=required and 1 or 0),"tier direction")
  if carried>=required then check(a=="native" and b==17 and lastIndex==required and lastCount==2,"native forwarding") end
 end
end
bagItem=nil;bankItem=16207;local before=calls;DoCraft(1);check(calls==before,"bank-only rod")
bankItem=nil;bagItem=2901;DoCraft(1);check(calls==before,"unrelated tool")
bagItem=6339;local name,has=GetCraftSpellFocus(1);check(name==names[1].." (or higher)" and has==1,"higher rod display")
bagItem=nil;name,has=GetCraftSpellFocus(1);check(name==names[1].." (or higher)" and has==nil,"missing rod display")
originalFocus="Anvil";focusHasTool=1;bagItem=6339
local anvil,owned,rod,present=GetCraftSpellFocus(1)
check(anvil=="Anvil" and owned==1 and rod==names[1].." (or higher)" and present==1,"retain other tool requirements")
-- Addon without matching MPQ must leave original client behavior/display intact.
originalFocus=names[1];focusHasTool=nil;bagItem=nil;before=calls
name,has=GetCraftSpellFocus(1);check(name==names[1] and has==nil,"unpatched display unchanged")
DoCraft(1);check(calls==before+1,"unpatched craft delegates native checks")
originalFocus="Anvil";focusHasTool=nil;before=calls;DoCraft(999,3)
check(calls==before+1 and lastIndex==999 and lastCount==3,"unrelated craft forwarding")
-- Exercise the name/rank lookup when the optional link API is unavailable.
GetCraftItemLink=nil;originalFocus=nil;bagItem=6339;before=calls;DoCraft(1)
check(calls==before+1,"native-name fallback")
selection=1;SlashCmdList.WARBANDENCHANTINGRODS();check(string.find(DEFAULT_CHAT_FRAME.text,"active",1,true),"status")
print("PASS: 25 rod combinations, missing/bank/unrelated rods, display, original-client fallback, native arguments and returns")
if arg[2] then
  dofile(arg[2])
  check(WarbandEnchantingRodsData.recipes[7788][1]==1,"actual recipe tier")
  check(WarbandEnchantingRodsData.byName["Enchant Weapon - Minor Striking\031"]==7788,"actual recipe name")
  WarbandEnchantingRods=nil
  GetCraftInfo=function(index)return "Enchant Weapon - Minor Striking","","optimal" end
  GetCraftSpellFocus=function()return end
  DoCraft=function(index,count)calls=calls+1;lastIndex=index;lastCount=count end
  dofile(root.."/WarbandEnchantingRods/WarbandEnchantingRods.lua")
  bagItem=6339;before=calls;DoCraft(42,1)
  check(calls==before+1 and lastIndex==42 and lastCount==1,"actual Minor Striking with Silver Rod")
  bagItem=nil;before=calls;DoCraft(42,1)
  check(calls==before,"actual Minor Striking missing rod")
  print("PASS: generated 172-recipe data parses under Lua 5.0; actual Minor Striking alias dispatch/rod checks")
end
