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
| `probe24.ts` | Government from the player's seat: every directive with what stood beside its target when it was set (my provinces and corps beside it, land hops for the nearest corps, defenders), how it ended, the cabinet's trust every half year with what moved it, the months the cabinet fell, capital, income, and at the end `TRUST` and `CAPITAL` lines with one value a month. A sixth argument picks a cabinet: `none`, `money`, `men`, `calm`, `most`, or `aside` (set every directive aside at once). The plan may be `{"idle":true}` | `node out/probe24.mjs GER 1 7 76 '<plan>' money` |
| `gov.sh` | The 83 government wars in streams. Part A: scripted players of eleven countries who leave the cabinet alone. B: idle players of nine. C: three countries with four cabinets each. D: three who set everything aside. E: fifteen who go for the town each directive names. `PARTS=E` runs one part. Writes one file a war, `totals.txt` and `errors.txt` | `BIN=out57 ./gov.sh out/v57gov 4` |
| `govread.py` | One line for each directive in a folder of `probe24` output (kind, town, size, fortress, owner, what stood beside it, months, how it ended), or the totals | `python3 govread.py out/v57gov directives > d.txt` |
| `govtab.py` | Tables from those lines: by kind, by defenders against the player's troops beside the town, by fortress level, the first directive of each war, by country, the holds | `python3 govtab.py d.txt` |
| `probe25.ts` | Plays a war and writes saves for the page at government moments: `-start`, `-directive`, `-late`, `-doubt`, `-crisis`, `-rich` | `node out/probe25.mjs GER 1 7 40 '<plan>' out/v57saves/ger` |
| `probe26.ts` | What each minister and decree is worth in money at one month of one war, lever by lever from the ledger. It gives the player the political capital for each lever, on that run only | `node out/probe26.mjs GER 1 7 30 '<plan>'` |
| `probe27.ts` | Each "bring the army up to strength" directive beside the army the player sees: divisions on the map and overseas, the directive's own count, the price of an infantry corps in months of income | `node out/probe27.mjs SWE 1 7 76 '<plan>'` |
| `ui16.cjs` | The Cabinet screen from a save: the button's flag, the alerts, the card's text and buttons, then clicks on Set it aside, an appointment, a decree and Show it on the map, with pictures | `node ui16.cjs out/v57saves/ger-directive.json 80-ger aside,appoint,decree,show` |
| `ui17.cjs` | The countries the start card offers from a start year | `node ui17.cjs` |

`lib.ts`: an idle plan now calls `each` too, so a probe can watch a player who only ends the month. A plan may set `"chase": true` (and `chaseOdds`, 1.1 unless set): the scripted player then masses on the town the directive names, attacks it at shorter odds, storms it at a 40% chance, and is let off `homeOnly` for that one town. Off by default, so every other run plays as before.

Traps:
- The cabinet's minutes (`governmentOf(g).log`) keep the last twelve lines only. A probe that reads them by position loses endings and falls in a long war: tell lines apart by month and text.
- The probe prints a town's owner at the end of the player's month. "Take X [of ME; defenders 0]" means the town fell in the month the order came, not that the cabinet named a town the player already held.
- "Land hops -1" is a sea crossing only when no corps of the player's stands beside the town. Corps landed on ground they do not control give the same -1.
- The United States is not offered from a 1939 start: a job for it plays no months.
- The scripted player never courts a neutral and never signs a trade agreement, so its political capital sits at the cap more than a person's would.
- The German cabinet wars with the money or the men cabinet end early (month 56 or 57): compare levers at one month with `probe26`, not whole wars.

## Version 58 (prepared defences), and a first look at Version 59 (manpower)

Build against the version under test, for example `./build.sh probe28 /home/user/tc58 out58`. The scripted player in `lib.ts` has new plan options, all off unless named: `"minChance":0.6` (attack only where the card gives that chance), `"guns":1.5` (keep that many artillery corps for every ten field corps), `"useGuns":true` (bring up to two artillery corps into an attack in place of the weakest corps from the same province, where the card then reads better), and the hooks `onLook` (every attack on offer, before the player chooses) and `onAttack`.

| Script | What it does | Example |
| --- | --- | --- |
| `probe28.ts` | Every field battle of the player's, split by who attacked, whether the line was dug in and fed, and the card's odds; dug-in corps each January; who holds Berlin in December 1944; the longest spell with no battle. With `ROWS=1` every battle is printed as a row | `ROWS=1 node out58/probe28.mjs GER 1 7 76 '<plan>'` |
| `def.sh` | The fifteen defence wars (Germany, Soviet Union, Britain, France, Italy on seeds 7, 11, 23) in streams | `BIN=out58 ./def.sh out/v58def 4` |
| `deftab.py`, `rows.py` | Totals of `probe28`'s BATTLES lines; from the rows, how often a line holds by the card's odds and by how ready the works are, and attacks with guns | `python3 rows.py out/v58rows` |
| `probe29.ts`, `card.sh` | Is the attack card honest: the chance the card showed against what happened, for a German and a Soviet player on three seeds | `BIN=out58 ./card.sh out/v58card` |
| `probe30.ts` | Why a front stands still: each month every attack on offer, how many reach the player's bar, how many are on a dug-in line, the best odds, what the player did. A detail month lists every option with the troops on both sides (strength, organisation, experience, national quality) and the card's lines | `node out58/probe30.mjs GER 1 7 76 '<plan>' 37 49` |
| `guns.sh`, `guns2.sh`, `gunstab.py` | German, Soviet and Soviet-first players on three seeds with four habits (no guns, guns kept in the line, card reader, both), then with the guns brought into attacks; one line a war | `BIN=out58 ./guns2.sh out/v58guns2 4; python3 gunstab.py out/v58guns2` |
| `scorch58.sh` | The scorched earth on and off test on Version 58 (output in `out/v58x/`) | `./scorch58.sh` |
| `probe31.ts` | Writes a save for the page at the first month with an attack on a finished dug-in line at 1.15 to 1.7 to 1 | `node out58/probe31.mjs GER 1 7 76 '<plan>' out/v58saves/GER-7 30` |
| `ui18.cjs`, `uix.cjs` | The new screens from that save: earth banks, "Dug in" tags, the attack card as it opens and with every corps in, the result, wrecking asked twice, the folded policy; `uix.cjs` lists what is on the page | `TC_PAGE=file:///home/user/tc58/dist/play.html node ui18.cjs SAVE.json SAVE.txt shots/84-v58-ger` |
| `probe32.ts` | Version 59 on: the player's reserve of men, the month's class, corps, divisions, how full they are, beside the main enemy's, every third month | `node out59/probe32.mjs SOV 1 11 76 '{"minOdds":1.5}'` |

Traps met on Version 58:

- A battle report's `odds` is the odds after the day's luck, so every win reads 1.2 or better. The odds a player sees on the card are `aPower / dPower`. Band battles by the second (a first table banded by the first showed lines that never held).
- `limitAttackers` takes the strongest corps that fit the roads, so artillery corps are left out. `"guns"` alone buys guns that sit in the line; `"useGuns"` is needed to see guns in an attack.
- In the page the province panel is `#pane`, the army tray is `#tray`, and `#veil` with `#card` inside is the modal (the attack card). The attack card opens with the selected army only: click every `[data-act="atkpick"].out` to add the rest.
- `probe32` builds only against Version 59 or later.

## Versions 59 and 60 (manpower, the cabinet fixes, the cabinet that talks back)

Build against the version under test, for example `./build.sh probe24 /home/user/tc60 out60`. New plan options in `lib.ts`, all off unless named: `"vow":"safe|fair|bold|daring|any"` (promise a town of that band whenever the cabinet will hear one, longest time on offer, then go for it), `"head":"sack|stand"` (answer the cabinet when it asks for a general), `"noCabinet":true` (switch the cabinet off at the start), `"menKeep":0.5` (raise no new corps while the reserve is under that share), and a hook `first(g, run)` called at the start of the player's month, before any order.

| Script | What it does | Example |
| --- | --- | --- |
| `jobs.sh`, `v60jobs.sh` | A queue that survives a restart: `v60jobs.sh` writes one `NAME<TAB>COMMAND` line a job (252 for the whole test), `jobs.sh` plays them in streams and leaves `DIR/.done/NAME` for each finished one, so the same command plays only what is missing | `./v60jobs.sh > v60.tsv; ./jobs.sh v60.tsv out/v60 4` |
| `probe24.ts` | The government probe, with more on each line: `when asked` (who held the town, the troops in it and the cabinet's own odds before the player moved), the attack card's odds before the player moved in the month asked and at their best while the directive ran, `HEAD asked` with the general's record and his last battles as they stood on the day, the wing on top, promises. Cabinets `hawks` and `doves` | `node out60/probe24.mjs GER 1 7 76 '<plan with "chase":true>' hawks` |
| `vowtab.py` | Promises by the player's habit (made, kept, kept in the month made, broken) and heads asked for (what the general had lost and at what odds, his record, what came of standing by him) | `python3 vowtab.py out/v60 gov-vow` |
| `wingtab.py` | What a hawk or a dove table asked for and what came of it, by how the war was going | `python3 wingtab.py out/v60b wing- -v` |
| `cardtab.py`, `oddstab.py` | Towns the cabinet asked for: its own odds against the attack card's, and how often each band was taken | `python3 cardtab.py out/v60c/card-*.txt -v` |
| `probe32.ts` | Men: the reserve, the class, corps, divisions and how full they are, every third month | `node out60/probe32.mjs SOV 1 23 76 '{"minOdds":1.5,"menKeep":0.5}'` |
| `probe33.ts` | A campaign with the cabinet off (and on again at a month): leaks, the switch across a save, income | `node out60/probe33.mjs GER 1 7 76 '<plan>' 31` |
| `probe34.ts` | Writes saves for the page at the first month of each new thing (a promise on offer, a head asked for, hawks on top, the reserve thinning, short and spent, a doubting cabinet) | `node out60/probe34.mjs SOV 1 11 76 '<plan>' out/v60saves/SOVd-11 doves` |
| `probe35.ts` | Stand back, then promise: lists the promises on offer, marches every corps within two provinces of a town away, lists them again, gives the promise and plays it out | `node out60/probe35.mjs GER 1 7 24 '<plan>' back 2 Kiev` |
| `probe36.ts` | What switching the cabinet off and on again in one month clears, on saves | `node out60/probe36.mjs out/v60saves/GERh-7-directive.json` |
| `probe37.ts` | What an appointment costs and the capital a government opens with, by country | `node out60/probe37.mjs GER BEL SWE` |
| `ui19.cjs`, `ui20.cjs`, `ui21.cjs` | The screens: the men gauge, ledger and cabinet switch; wings, promises and both answers on a general; the start card's picker and the promise button on a province | `TC_PAGE=file:///home/user/tc60/dist/play.html node ui20.cjs SAVE.json 86-v60-sov-head promise,head:stand` |

Traps met on Versions 59 and 60:

- `each` runs at the end of the player's month. A town printed there as the player's own was taken that month, and a general's "last battles" include the month's own fights. Read what the cabinet saw in `first`, before any order (this trap produced a wrong finding twice).
- The cabinet's odds (`directiveOdds`: troops within two provinces over defenders and walls) are not the attack card's (`attackOutlook` on the corps that fit). Compare them before the player moves; at the end of the month the corps that attacked have no moves left and the card reads low.
- The Soviet call-up fills its corps from the reserve of men, so `"menKeep"` cannot stop the Soviet reserve falling.
- The machine can restart in the middle of a long run and kill background work; files survive. Use `jobs.sh`.
- Swap a rebuilt bundle in with `mv` (build to another folder first), never in place, while a queue is running.
- `ui20.cjs` puts `shots/` in front of the shot prefix itself; `ui21.cjs` takes the path as given.
