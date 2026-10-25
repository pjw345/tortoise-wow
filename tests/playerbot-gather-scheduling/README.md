The CMake/CTest fixture extracts production strategy handlers, native action
priorities, NextAction array construction, Queue push/pop/peek and the gathering
usefulness guard. World/recovery services are faked. This reproduces the old
follow-over-scan ordering, checks fallback ordering and verifies delegated native
recovery guards. Pair with playerbot-gathering-safety/loot-followups tests for
actual admission and repeat behavior. No full Engine integration or live latency
claim is made by this queue-level fixture. Action waits/casts can still add delay.
