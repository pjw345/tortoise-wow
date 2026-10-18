# Automatic gathering review

Active source: 47cd6625. Alon 9009's session opened 15:30:03 UTC. The saved gather
strategy remains enabled, and his active action log repeatedly executes
`add gathering loot` with FAILED outcomes. Manual group `u go` is followed by
Mining(2575) on Copper Vein 1731 / spawn 34829 at 15:34:45, 15:35:20 and 15:35:25,
with successful store-loot action outcomes. Saved Mining is 28/75. The user
confirmed standing right beside the node; do not attribute that incident to
being outside the five-yard active-master gathering limit.

Confirmed source discrepancy: automatic grouped gathering independently scans
30 yards for hostiles before its parent validates configured LootHostileDistance
(20 on this VM). The native query does not require LOS to the hostile. The node
has nearby Defias spawn locations, including a higher-elevation Footpad roughly
28 yards away in 3D. Static spawn data is not evidence it was alive/present when
the automatic attempt failed. The current action log lacks the rejection reason;
the specific incident remains unproven.

The patch delegates follower safety to the existing shared configured check and
adds opt-in rejection reasons. It preserves profession/tool checks, gathering
range, bag/loot policy, native opening and solo/group-leader strong-hostile rules.
It does not bypass enemies within configured safety distance or bypass manual interaction requirements.

Focused source-extracted MSVC regression passes. A full native/image build and
live acceptance are pending. No services, configuration or gameplay state were
changed. After the next stopped-server build, test an eligible Copper Vein right
beside the human, with debug loot enabled on Alon if a skip recurs. Correlate its
GUID and reason; do not label the specific incident resolved without gameplay.

Explicit `u go` feedback: bots lacking Mining/Herbalism remain silent. A trained
gatherer below the native node requirement replies with the formatted node,
profession, required skill and its current skill, before selecting/opening loot.
This also explains too-high herbs. Automatic scans do not produce this reply.
The existing recovery/loot extracted regression covers missing, insufficient,
exact, above-required and zero skill requirements plus normal open failures.
