# Alin combat safety follow-up

The b74fc343 playtest capture 20 (14:17:32-14:18:06 UTC) recorded Mercy's death
with 1,015/1,492 mana still available. Three melee attackers dealt 807 effective
damage plus 236 absorbed damage. No health-restoring heal landed on Mercy.

Cria's Heal on Alin was cancelled at +25.639s by SelectNewTargetAction. Mercy's
self-Heal began at +27.041s and remained PREPARING until death at +32.805s;
repeated rescue casts failed SPELL_FAILED_SPELL_IN_PROGRESS. Alon's decisions
still counted pending=430 on Mercy at 237, 205, 131 and 90 HP. Mercy was roughly
33.4 yards from Cria, beyond AI HealDistance=30 despite native learned heals
having greater range. At +8.562s Alon requested pursuit of a full-health,
out-of-combat Champion about 35 yards away. Mercy pursued it too and encountered
the three attackers. This does not establish that Alon caused every add.

Capture 22 shows Alon's Hammer of Justice at +6.459s on a runner with 111 HP.
No new bot casts started against that target during the stun; existing periodic
damage and Alin's attacks killed it at +9.121s. Target-selection retries match
the native IsStunned crowd-control exclusion in the master-focus path.

Changes retain native spell preparation, cost/LOS/range/immunity checks,
healing completion and movement. Enemy retargeting yields to useful friendly
heals; timely direct-heal coverage uses current remaining cast time and native
melee swing timers. Untimely pending/available primary healing cannot suppress
emergency backup. This conservative estimate uses unmitigated mean melee damage;
it does not predict caster bursts, DoTs, crits, absorbs or future movement.

Party patient selection reaches the maximum learned healing spell range,
bounded by sight distance. Friendly cast checks still decide actual feasibility.
Hostile pursuit requires combat contact and stays within configured follow range
of the human leader, including after their death. Stationary local defense and
existing ranged casts remain available outside that movement boundary. There
is no general proof that every path inside the boundary avoids extra aggro.

A stun alone does not exclude an engaged kill target from focus; breakable CC,
fear, roots, tapping and immunity retain native checks. Melee can hit an adjacent
stunned runner but cannot move after it. Stun expiry re-enables the melee runner
restriction; ranged finishers retain their existing no-pursuit lock.

Validation: focused source-extracted Windows tests with fake geometry and native
spell dependencies. A full server/image build, activation and gameplay acceptance
are deliberately deferred until the user logs off and stops the servers.
