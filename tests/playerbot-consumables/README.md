# Inventory consumables regression fixture

Compiles production item-selection, buff eligibility, explosive safety, mana
selection and action classes. The warning fixture extracts the actual potion
attempt prefix and represents the unchanged chat path with a counter; source
checks retain the original warning throttle and strategy registrations.
Adapters represent native inventory, cooldown, stronger-buff and target checks;
they do not simulate the real spell/item executor or replace integration tests.
Cases include actual local item/spell shapes for Rough Dynamite, Minor Mana and
Weak Troll Blood, inventory exhaustion/trade/level/skill/cooldown/skip filtering,
free-bot policy, buff replacement/role relevance/recovery states and hostile
AOE/LOS/CC/master-target restrictions. No live data is changed by these tests.
