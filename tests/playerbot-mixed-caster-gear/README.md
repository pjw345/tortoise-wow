Executes native CalculateSingleStatWeight, the basic item-stat accumulation,
armor/block accumulation, final class/spec qualification and return portion of
CalculateStatWeight, and native IsBetterEquipment. The stat-only adapter supplies
zero spell/weapon/socket contributions, matching the observed unenchanted cloth
boots. Scales/stat links use the captured mage Intellect weight and controlled
healer/attacker data. It does not execute spell-effect, random-property, socket,
whole-world native initialization or inventory mutation paths.

The observed boot pair is an input example, not an item-ID rule. Covers all three
mage scales, mixed Agility/Strength, plain Intellect, healer stats, physical-only,
no-mana/attacker exclusions and statless armor upgrades. Passing the deployed
RandomItemMgr.cpp through WEIGHT_SOURCE must fail mixed_caster_boots.
