# Playtest harness

Scripts the Playtester thread uses to play whole grand campaigns against the rules in `src/campaign` and to click through the built page. Nothing here changes game code.

Setup: the repo at `/home/user/theater-command` (or pass its path to `build.sh`), `npm install`, and `npm run build` for the browser scripts.

| Script | What it does | Example |
| --- | --- | --- |
| `play.ts` | A scripted average player with a named plan (GER, GER_WEST, GER_SEALION, GER_STARVE which never storms a town, UK, FRA, FRA_ATTACK, SOV, SOV_FIRST, ITA, ITA_BALKAN, POL, BEL, FIN, TUR, ROM, SWE, ESP) | `./build.sh play && node out/play.mjs GER,UK 7,11 1 76 colonel` (add `-v` for every month) |
| `idle.ts` | A player who only ends the month | `node out/idle.mjs GER 7 1 90` |
| `watch.ts` | Computer-only wars: fall dates, battles a year, biggest stacks, repeated headlines | `node out/watch.mjs 1,2,3,4,5,6,7,8,9,10,11,12 88` |
| `probe8.ts` | Traces one province month by month | `node out/probe8.mjs SOV 29 7 "El Alamein" 40` |
| `probe9.ts` | A country's surrender score month by month (`WHO=ITA` to pick it) | `node out/probe9.mjs UK 7 cadet 37` |
| `probe10.ts` | An idle player: who fell to whom | `node out/probe10.mjs ITA 7 88` |
| `probe11.ts` | Computer-only wars: every surrender and whether the victor makes sense, and assaults against field battles | `node out/probe11.mjs 1,2,3,4,5,6,7,8,9,10,11,12 88` |
| `probe12.ts` | One town's siege month by month: ring, stores, who is inside | `node out/probe12.mjs UK 1 7 Moscow 22 76 '{"minOdds":1.5,"land":true}'` |
| `probe13.ts` | A frozen siege: strengths inside and outside, corps next door, battles between the two sides | `node out/probe13.mjs UK 1 7 Moscow 24 76 6 '<plan json>'` |
| `probe14.ts` | Where a save differs after save, load, save again | `node out/probe14.mjs UK 5 7 colonel 40 '<plan json>'` |
| `siegesave.ts` | Writes `out/siege.json`, a browser save with Poznan under siege (Germany, seed 7, September 1939) | `node out/siegesave.mjs` |
| `fallsave.ts` | Writes a browser save of Italy the month before it falls | `node out/fallsave.mjs 69 ita-fall.json` |
| `monkey.cjs` | Random clicks, keys and drags in `dist/play.html`; reports page errors and bad text | `node monkey.cjs GER 2 2500 1 colonel` |
| `ui1.cjs` to `ui7.cjs` | Single screens: start card, first month, declaring war, a country's fall, the attack card and tabs | `node ui6.cjs` |
| `ui8.cjs`, `ui9.cjs` | An attack on a guarded town; the siege panel, assault card and result loaded from `out/siege.json` | `node ui9.cjs` |
| `compare.py`, `quiet.py` | One version's results beside another's, war by war; the share of quiet months by country | `python3 compare.py ../results-v53 ../results-v54` |

Arguments for `play`: plans, seeds, first turn, last turn, level. Turn 1 is September 1939; 5, 17 and 29 are January 1940, 1941 and 1942; 76 is December 1945. Levels: cadet, captain, colonel, general, marshal.

`lib.ts` holds the scripted player and the checks run after every nation's move: corps and province lists consistent, no two nations at war sharing a province, no troops in a country they are neither allied to nor at war with, no ground held with only the enemy on it, no bad numbers or text, and save, load, save again gives the same save.

The `.cjs` scripts write screenshots to `shots/` beside them and expect Playwright to be installed globally.

## Version 54 (cities and sieges)

`lib.ts` now needs `src/campaign/siege.ts` (the Version 53 copy is `old/lib-v53.ts`). The scripted player puts infantry into a threatened city or capital, storms a town when `stormOutlook` gives odds of 1.3 (1.1 after two months round it), and sorties at 1.4. A field win counts as won when the report says `taken` or `siege`. The checks accept corps in `r.garr`, and flag a garrison over the town's room, a dead or misplaced garrison corps, and a garrison not allied to the holder. The summary of each war has a siege block: sieges ended, how long, the player's assaults, the rules' own `sgn:` tallies, and sieges still running at the end. A save that loads back with the same contents but a table's keys in another order is reported as such, not as a broken save.

The British plan used in the probes: `'{"minOdds":1.5,"land":true,"buy":["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]}'`.

## Version 55 (the war keeps moving) and later

**Rebuild every program after any change to `lib.ts`.** Each `out/*.mjs` bundles its own copy of the scripted player, so a probe built before a change plays a different war from `play.mjs` and the two will not agree.

The scripted player now judges an assault by the card's chance, as a person would: it storms at 60% or better, at 50% once it has stood round the town two months (`stormAt` in a plan overrides both), and otherwise waits. The siege block of each summary adds "held back N times" and a line "assault card against result" that sets the chance the card showed beside what happened, in bands of ten points.

`battery.sh OUT_DIR` runs the whole battery in four streams: 51 plan wars, the starve-only plan, three late starts, two levels, the small-country and Poland sweeps, the idle runs, twelve computer-only wars twice (`watch`, `probe11`) and eight random-click sessions. Build `play idle watch probe10 probe11` first. It takes about ten minutes on four cores.

To run a second version beside the first without losing the first build:

```
git -C /home/user/theater-command worktree add --detach /home/user/tc56 origin/main
ln -s /home/user/theater-command/node_modules /home/user/tc56/node_modules
(cd /home/user/tc56 && npm run build)
for n in play idle watch probe10 probe11; do ./build.sh $n /home/user/tc56 out56; done
BIN=out56 TC_PAGE=file:///home/user/tc56/dist/play.html ./battery.sh results-v56
```

| Script | What it does | Example |
| --- | --- | --- |
| `probe15.ts` | Computer-only wars stepped one nation's move at a time: for each great power that gives in, how its capital stood just before the fatal move and what was fought there in that move. Use this, not `probe11`, to judge "gave in with the capital untouched": `probe11` looks at the start of the month and misses a capital ringed or stormed inside it. `probe15b.ts` is the same for builds before Version 55 | `node out/probe15.mjs 1,2,3,4,5,6,7,8,9,10,11,12 88` |
| `probe16.ts`, `probe17.ts` | Does a save in the middle change the war; which rules calls change the war. Both answered no on Version 55 | `node out/probe16.mjs` |
| `probe18.ts` | A long siege of a port: ring, sea open or closed, months shut, the besieger's odds for an assault, the holder's surrender score | `node out/probe18.mjs UK 1 11 Athens 14 76 6 '<plan json>'` |
| `probe19.ts` | How an army melts: a nation's corps and divisions each month, where the corps that vanished were last seen, battles lost defending, prisoners | `node out/probe19.mjs GER 1 7 GER 1 76 '<plan json>'` |
| `ui10.cjs` | Is the Begin button in view at three window sizes; how estimates of hidden armies read on the map | `node ui10.cjs shots` |

## Version 56 (prizes outside the town, scorched earth)

The rules are `src/campaign/prizes.ts`; the numbers are `data/campaign/prizes.json`. A computer nation's scorched earth policy is `PRIZES.ai.policy[nation]`, and a probe can overwrite it before the war starts, which is how the same war is played with the burning on and off. The burning has dice of its own (`prize:<seed>:<turn>:<region>:<kind>`), so nothing else in the war shifts until its effects spread.

In the page, start or continue a game by clicking the card (`[data-act="start"]`, `[data-act="continue"]`): calling `newGame` or `continueGame` through `window.theater` leaves the start card over the map. `window.theater.game` is a function.

| Script | What it does | Example |
| --- | --- | --- |
| `probe20.ts` | Scorched earth from the player's seat: provinces, divisions and income every quarter, wrecked places on the player's ground, scorched earth headlines by month and by place. A sixth argument forces a computer nation's policy, a seventh sets the player's. `DUMP=1` prints every prize headline | `node out56/probe20.mjs GER 1 7 64 '<plan>' SOV=never` and `node out56/probe20.mjs SOV 1 7 64 '<plan>' - everywhere` |
| `scorch.sh` | The twelve wars of the on and off test in four streams (output in `out/v56x/`) | `./scorch.sh` |
| `probe21.ts` | A browser save read from the rules: which places a province has and who holds them | `node out56/probe21.mjs out/siege.json Poznan` |
| `probe22.ts` | Plays until the player holds places outside a town not yet taken and writes a save of that month for the page | `node out56/probe22.mjs GER 1 7 40 '<plan>' out/v56x/besieger.json 1` |
| `probe23.ts` | What the places outside untaken towns pay the player: the ledger's prize line each month against the whole income | `node out56/probe23.mjs GER 1 7 64 '<plan>'` |
| `ui11.cjs`, `ui12.cjs` | The "Places of their own" block of the province panel; a click on a wreck button and on the policy | `TC_PAGE=file:///home/user/tc56/dist/play.html node ui11.cjs` |
| `ui13.cjs` | Height of the province panel in two builds (pass the two page addresses) | `node ui13.cjs file:///home/user/theater-command/dist/play.html file:///home/user/tc56/dist/play.html` |
| `ui14.cjs`, `ui15.cjs` | A save from an older version loaded in the new page; the besieger's view of the places block from a `probe22` save | `node ui15.cjs out/v56x/besieger-1.json Warsaw` |

Traps: Poznan has no places at all (no airfield, no port, fewer than five rail lines), so a siege there shows no block and that is not a fault. Count "airfield alone" headlines with `wreck the airfield at`. The scripted player gives no air orders, so a burned airfield costs it nothing.

## Version 57 (government and directives) and running on another computer

Since Version 57 the harness also lives in the repository, on the branch `claude/project-thread-t35t0x` under `playtest/harness/`, so that a long run can go to another computer. That branch is `main` plus this folder: one checkout holds the rules and the harness. It changes no game code and is not meant to be merged.

```
git fetch origin claude/project-thread-t35t0x
git worktree add ../tc-playtest origin/claude/project-thread-t35t0x     # a folder of its own: never switch branches in a checkout somebody else is working in
cd ../tc-playtest && npm ci
cd playtest/harness && ./build.sh probe24 && ./gov.sh ../results/v57-gov 4
```

`build.sh name` with no repo bundles against the checkout the folder sits in. Run from the scratchpad in the cloud, pass the repo as before.

| Script | What it does | Example |
| --- | --- | --- |
| `probe24.ts` | Government from the player's seat: every directive with what stood beside its target when it was set (my provinces and corps beside it, land hops for the nearest corps, defenders), how it ended, the cabinet's trust every half year with what moved it, the months the cabinet fell, capital, income. A sixth argument picks a cabinet: `none`, `money`, `men`, `calm`, `most`, or `aside` (set every directive aside at once). The plan may be `{"idle":true}` | `node out/probe24.mjs GER 1 7 76 '<plan>' money` |
| `gov.sh` | About 70 government wars in streams: scripted players of eleven countries, idle players of ten, three countries with four cabinets each, three who set everything aside. Writes one file a war, `totals.txt` and `errors.txt` | `BIN=out57 ./gov.sh out/v57gov 4` |

`lib.ts`: an idle plan now calls `each` too, so a probe can watch a player who only ends the month. Nothing else in the scripted player changed.
