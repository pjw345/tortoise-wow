Extracts native UpdateSkillPro, its profession predicate, the new virtual/enum,
enabled-hook expansion and actual registry iteration, plus the bot notification
handler. Controlled RNG/player/chat/DBC services exercise real committed gains,
failed rolls (which can return true), zero-step/caps, primary/custom and secondary
professions, non-profession skills, recipients, silent policy and unchanged Eluna
callbacks in module on/off and Eluna on/off configurations.
An observer module remains present when bots are absent to check generic dispatch.
OLD_PLAYER_SOURCE replaces only the native updater; old primary-gain tests fail.
Fixtures do not execute full WorldSession delivery, training/login lifecycles or
recipe/loot generation. Actual crafting/gathering/fishing call paths are checked
during extraction; full native translation units complement these fixtures.
