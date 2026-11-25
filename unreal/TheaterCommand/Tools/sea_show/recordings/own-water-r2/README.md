# Own-water recordings (test only)

The naval rules' fleet actions, recorded with the "Friendly torpedoes" switch on (campaign flag `ownWater`). The
computer fights both sides. The naval thread delivered them on 25 November 2026 from
`naval-battles/own-water-r2/recordings/`, and they were copied here unchanged.

They are test recordings for the sea show's cloud tests (`Tools/sea_show/run.sh`). The torpedo words test
(`sea_words_test.cpp`) pins its lines to them. They do not replace `Content/Data/SeaBattles`, which belongs to the
rules' joins.

| File | md5 | Seed | Views | What the views set |
|---|---|---|---|---|
| `light-ships.json` | `ea539b93ba9b436a4179d8420528fe17` | 63591 | 289 | 35 piece-views with `heldFor` (in 23 views) and 40 with `clearFrom` (in 26 views), all on Force H (the attacker, the views' `me`) |
| `two-lines.json` | `e1f5c25b293f65fc0abe650bd0dae14b` | 23996 | 305 | 5 piece-views with `heldFor`, all on the Battle Fleet's Destroyers 3 (the defender), 372 to 380 s |
| `two-lines-sailors.json` | `319f427c9ddf912537e53d436cd1ca0f` | 71510 | 315 | the same 5, with the sailors' rescue in the after card |
| `samar.json` | `fe43a293cbf589d8000d9d9ca32698e2` | 16077 | 220 | none, so the scene shows nothing |
| `samar-sailors.json` | `49c6608422760f6dc24c518b44432a35` | 16077 | 220 | none, so the scene shows nothing (with the sailors' after card) |

- The `-sailors` files are the same actions recorded with the sailors' rules too: their `after` card carries
  `sailors`, `rescue` and `abandoned`.
- `light-ships`, `two-lines` and `samar` have the same keys as the shipped recordings in `Content/Data/SeaBattles`, but
  they are different fights. The shipped ones were recorded before the switch, so they have other seeds (except samar),
  other lengths and no torpedo fields.
- Every view's `me` is `att`. Neither side is human, so `ordered` never appears. The naval thread's
  `tests/own-water-orders-r2.test.ts` covers the player's order on the rules side. `sea_words_test.cpp` covers it in the
  scene with a view written by hand.
- The rules give the names the way the log writes them mid-sentence, with a count in figures: "4 destroyers of Force H".
