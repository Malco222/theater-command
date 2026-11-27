# Tests ahead of the rules

Tests written before the rules they check, from the design director's rulings, so that each version lands proven.
Every block waits, skipped, until the export it needs exists in the rules, and then runs by itself. Each reads that
export through a loose handle, so `tsc` passes on a tree where it is not built yet. Nothing here changes a rule.

| File | Ruling, version | Switches on when | Tests |
|---|---|---|---|
| `alarm-8426.test.ts` | 211(c), the alarm, 84.2.6 | `danger.ts` exports `homeAlarms(g, nat)` | 17 |
| `britain-save-8425.test.ts` | 211(b) at 60/40 (Brian's change), 84.2.5, on his own save | `game.ts` exports `dominionAnswers` | 3 |
| `march-attack-845.test.ts` | ruling of 15:50Z (a) and sheet 34, march up and attack, 84.5 | `turn.ts` exports `approaches(g, corps, target)` | 7 |
| `works-213.test.ts` | 213(d), aircraft works, the cities version | `arsenal.ts` exports `AIRCRAFT_WORKS` (one constant at the top if named otherwise) | 12 |
| `land-pay-214.test.ts` | 214, land pays by its worth, the cities version | `economy.ts` exports `landFactor` (`(r)` or `(g, r)`) | 8 |

Each `describe` begins with its version or ruling, so the test runner's line ("ahead: N passed, M waiting") can say
which ruling is proven.

## The case

`saves/brian-usa-1940-12.json` is Brian's own save (the United States, seed 89882, December 1940, his phase), the one
the Playtester traced for ruling 211 (`/mnt/project-files/playtest/feel/brian-pittsburgh-trace.md`). It is read with
Vite's `?raw` import (`raw.d.ts` declares it for `tsc`).

## Readings settled before writing (the design director, 2026-11-17 17:22Z)

1. After 84.2.5 the guns at Buffalo can march onto none of his ground, so the row leaves out its second sentence: "A
   Canadian corps of guns stands at Buffalo, on your home ground. Nearest help: the army round Vancouver, 1 month by rail."
2. The row names the enemy exactly as the map's hover box does for that province that month. In his save the guns
   were never identified, so under the fog the row names no kind; with the fog lifted it says "corps of guns".
3. "Has taken" is only for a town that fell since he last ended a month, counted by the order of play, not by the
   calendar month (18:14Z, log b340): a town a country that moves after him takes reads "has taken" in his next month.
   Buffalo fell in Canada's phase of November, after his, so December reads "A Canadian corps of guns has taken
   Buffalo."; from January on it "stands at".
4. Nearest help is the time to reach the province where the enemy stands.

## Left open for the rules builder

- 84.2.6 as built (display-8426, 894006a) passes 13 of the 17 alarm tests. The 4 that fail are "has taken" by the order
  of play: the rule reads `r.since === g.turn`. Against a stand-in that records the phase of a capture, keeps it in
  saves and falls back to the captor's place in the order for old saves, all 17 pass.

- The help's months in his save: model 1 says 1 month by rail; `planMarch` to the town next door counts 2 with this
  month. The test takes either, and checks the row says the reading's own figure.
- `land-pay-214`'s `basePay()` is today's land and harbour pay with no factor. If the cities version changes how a city
  and its parent share land pay (ruling 210(e)), mend that one helper.
- `works-213` counts aircraft from abroad in `airFleetsOf()`. If dated rows land elsewhere, mend `aircraft()`.
- Not covered, for want of a name: his own guns left alone round a town (the tray's red row of 84.2.6), and the
  first-day totals of ruling 214, which were measured on the far-side rig, not on main.

## Proven before handing over

Against rough stand-in rules on a scratch tree (never committed): 37 of 41 pass on main and the alarm's and the
approaches' 19 of 20 pass on `rules-8425`; the rest wait on parts the stand-ins did not build (the warning's last
line, the works' news line, dated rows; Britain's roll had no stand-in). On main b8889c9 and `rules-8425` 6489955 as they are, `tsc`
passes and all 44 are skipped (48 since the alarm gained 4 at 18:15Z, all skipped on main).
