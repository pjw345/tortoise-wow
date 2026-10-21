# Playerbot loot follow-ups

This fixture extracts the production ShouldLootObject, StoreLootAction default
exclusion/policy and next-mining-admission methods. It exercises empty generated
loot on a READY vein, repeated native uses versus depletion/despawn/in-use state,
combat/death/disabled gathering, normal admission rejection, herbs/containers,
configured novelty sources, mixed/unknown loot, quest needs and existing manual
loot-list precedence. The extractor verifies that retry admission occurs after
native release and only after a successful item transfer.

The existing gathering-safety fixture executes the actual admission method for
range, LOS, safety, bags and eligibility. Native release probability/depletion,
map geometry and the full asynchronous session lifecycle still require full
build and gameplay validation; fake state transitions do not establish those.
