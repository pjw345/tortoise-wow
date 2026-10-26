# Companion profession skill-up notices

A grouped bot whispers its active human master after a real primary or secondary
profession point gain, for example: Mercy: [Skinning] increased to 29/75.
Names/links use existing ChatHelper localisation. Native IsProfessionSkill also
includes Turtle Survival; no separate profession list is maintained.

The appended PlayerScript hook is emitted inside successful UpdateSkillPro
progression only when the stored value rises. Crafting, gathering and fishing
already converge there. An attempted update can return true without winning its
skill roll, so callers/loot responses are not used as evidence of a gain.
Training, login population and direct skill-setting do not emit this event.
Native updates/caps/Eluna callbacks retain their behavior; this only observes.

The module requires a machine-controlled bot, active human master, master in
world and shared group. A non-leader master is supported. The existing private
TellPlayerNoFacing path supplies normal silent/security/chat handling. No-repeat
suppression is disabled for this one event so consecutive real gains are not
lost; there is no scan-generated repeated message and no public broadcast.

Focused tests extract the actual native update, hook declaration, profession
predicate and bot handler with controlled player/roll/recipient/chat services.
Both module present/absent and Eluna on/off are exercised. Old native update is
a negative control. Native translation-unit checks complement those fixtures;
full image build and live crafting/gathering/fishing acceptance remain pending.
