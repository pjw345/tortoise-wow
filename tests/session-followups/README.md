# Session followups

Prepared 23 October 2026; not compiled during the live session, at the user's request. On 24 October, after authorization, the focused suite compiled and passed under GCC 11.4. The existing native loot-followup suite also compiled and passed. Full image build and live acceptance remain separate.

Future validation:

```
cmake -S tests/session-followups -B build/session-followups
cmake --build build/session-followups
ctest --test-dir build/session-followups --output-on-failure
```

Uses the production enchanting helper and extracted production buyback method and gathering predicate. Native API fakes test adapter feedback, not real packet storage, client casting or navigation. Run full native builds and live acceptance separately. Rod fixtures cover every supported higher/lower combination, missing carried tools and unrelated tools. Buyback cases cover merged counts, failed native storage, insufficient money, no vendor, repeated requests and multiple stacks.
