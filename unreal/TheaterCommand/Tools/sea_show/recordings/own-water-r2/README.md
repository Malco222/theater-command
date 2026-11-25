# Own-water recordings (test only)

The naval rules' fleet actions, recorded with the "Friendly torpedoes" switch on (campaign flag `ownWater`). The
computer fights both sides. The naval thread delivered them on 25 November 2026 from
`naval-battles/own-water-r2/recordings/`, and they were copied here unchanged. `light-ships`, `two-lines` and
`two-lines-sailors` were delivered again the same day (17:52Z) after two rules changes, and replaced here unchanged (see
"Changes" below); `samar` and `samar-sailors` did not change.

They are test recordings for the sea show's cloud tests (`Tools/sea_show/run.sh`). The torpedo words test
(`sea_words_test.cpp`) pins its lines to them. They do not replace `Content/Data/SeaBattles`, which belongs to the
rules' joins.

| File | md5 | Seed | Views | What the views set |
|---|---|---|---|---|
| `light-ships.json` | `dd31a5163d350bf4d68471d6a880c593` | 63591 | 289 | 35 piece-views with `heldFor` (in 23 views) and 40 with `clearFrom` (in 26 views), all on Force H (the attacker, the views' `me`); 11 of the 40 while routing (Destroyers 4, 426 to 442 s; Destroyers 5, 426 and 428 s) |
| `two-lines.json` | `02dd7c93115c91177cb96db3e82bdcf7` | 23996 | 305 | 5 piece-views with `heldFor`, all on the Battle Fleet's Destroyers 3 (the defender), 372 to 380 s |
| `two-lines-sailors.json` | `f16738923cc6c286fd4004af2831265c` | 71510 | 315 | the same 5, with the sailors' rescue in the after card |
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
- The rules give the names with a count of one to ten in words and figures from 11: "four destroyers of Force H". The
  log keeps figures.
- `clearUntil` is the second the torpedo heads pass the ship where she lies, when the rules let her go. It is never later
  than the salvo's landing.

## Changes

25 November 2026, 17:47 to 17:52Z: `light-ships`, `two-lines` and `two-lines-sailors` were replaced. Their seeds and
view counts are unchanged. Compared with the first delivery (light-ships `ea539b93ba9b436a4179d8420528fe17`, two-lines
`e1f5c25b293f65fc0abe650bd0dae14b`, two-lines-sailors `319f427c9ddf912537e53d436cd1ca0f`), only these fields differ,
checked field by field:

- **`clearUntil`** (light-ships, all 40 piece-views): it was the salvo's landing, 464.1 s. The ship took up her orders
  20 to 56 s before the count reached 0: Destroyers 3's field ended at 408 s when the line still read 57 seconds,
  Destroyers 5's at 430 s (35 seconds) and Destroyers 4's at 444 s (21 seconds). It is now the second the heads pass her,
  and each turn clear ends within one view (2 s) of its count's 0: Destroyers 3 at 406.1 s, Destroyers 5 at 430.8 s and
  Destroyers 4 at 443.2 s.
- **`heldForName` and `clearFromName`** (light-ships 35 and 40 piece-views, two-lines and two-lines-sailors 5 each):
  "4 destroyers of Force H" is now "four destroyers of Force H", "2 destroyers" is now "two destroyers", and two-lines'
  "4 destroyers of the Battle Fleet" is now "four destroyers of the Battle Fleet".

At the same time the UI director worded a turn clear while routing, which the scene does by itself from the piece's own
state: "Breaking off, and turning clear of torpedoes fired by four destroyers of Force H.", with no count and no orders
to take up again. The tag stays "Turning clear of torpedoes". `sea_words_test.cpp` pins these lines.
