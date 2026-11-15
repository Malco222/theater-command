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

## Version 61 (works that wear, fronts that move again)

Build against the version under test, for example `./build.sh probe38 /home/user/tc61 out61`. New plan options in `lib.ts`, all off unless named: `"grind":0.9` (lean on a line: attack troops with half a month of works or more from these odds up), `"byChance":0.6` (a player who reads the card: order any attack the card gives this chance or better, whatever its odds), `"waves":1.05` (the two-wave player: attack a dug-in line from these odds up only where a second wave stands by, and strike a line already struck that month again from the same odds).

| Script | What it does | Example |
| --- | --- | --- |
| `v61.tsv`, `v61b.tsv` to `v61f.tsv` | The queues of the Version 61 test for `jobs.sh`: the whole battery with the defence wars, card wars, 24 Berlin wars and gun habits (141 jobs); players who lean or read the card (27); two waves (12); the card wars recounted (6); three kinds of attacking German on twelve seeds (36); the Soviet first strike on twelve seeds (24) | `./jobs.sh v61.tsv out/v61 4` |
| `probe28.ts` | The defence probe. Its `TOTAL` line now also gives attacks thrown back that wore works (the player's and those on the player), attacks ordered under the player's bar, second blows on a line struck that month and how many won, the player's field attacks by year and the provinces held at the end | `ROWS=1 node out61/probe28.mjs GER 1 7 76 '<plan with "waves":1.05>'` |
| `probe29.ts` | The card against its battles. Counts what the page prints: one chance where every defender is identified, a band where not. Prints `SURE THROWN BACK` (a flat 100% that was lost) with the card's odds, preparedness, works and dice beside the battle's, `OUTSIDE THE BAND` and a `FOG` line | `node out61/probe29.mjs SOV 1 7 76 '{"minOdds":1.5}'` |
| `probe38.ts` | The jab and the hammer. From a month on, prints every dug-in line beside the player (works, the card's odds, the even mark, the chance), then: `jab` (the weakest corps alone at each line under the bar), `jab2`, `soften` (up to three spare corps, then the blow if it reads 60% or the bar), `hammer` and `hammer3` (the full attack again and again in one month on the best one or three lines, each blow meeting the works the last one left) | `node out61/probe38.mjs GER 1 7 37 '<plan>' hammer 8` |
| `v61tab.py`, `rows2.py` | One line a defence war from the `TOTAL` lines; the battle rows of the same wars in two folders, like for like (lines held by readiness and odds, guns, battles by year) | `python3 rows2.py out/v59def out/v61` |
| `probe39.ts` | The undecided computer-only war. Each January from 1943 it prints the great powers' provinces; at the end, every province the loser still holds beside an enemy: its troops and works, the enemies round it, the odds a full attack by the strongest would get, and when it was last attacked. With `GER dump` it also prints the siege of any ringed town (who is inside, why it may not be stormed, how it is fed) | `node out61/probe39.mjs 11,16 89 GER dump` |
| `gunstab.py` | One line a gun-habit war: attacks on prepared lines, how many had artillery and how many of those won | `python3 gunstab.py out/v61` |
| `ui22.cjs` | The attack card on a dug-in line on a desk and a phone: the Troops and Dug in lines, "Send everyone who can reach", the card's length, the battle report. With `JAB=1` it orders the attack as the card opens, reads the thrown-back report and looks again | `JAB=1 node ui22.cjs saves61/GER-7-line-prepared.json saves61/GER-7-line-prepared.txt 87-v61-jab` |

Traps met on Version 61:

- `T.attackOutlook` returns the Outlook (`exact, odds, lo, hi, chance, chanceLo, chanceHi, even, lines`). Works, wear, preparedness, the dice and the two strengths are on the Assessment from `T.previewAttack`. A plan option that reads `out.works` needs `withSum` in `lib.ts` (the first `grind` wars never leaned for this reason).
- Where a defender is not identified the page prints a band, and `chance` is only the middle of it. A "sure" attack is `exact ? chance : chanceLo` at 0.9995 or more. Counting `chance` alone produced "sure attacks thrown back" that the page never promised (Versions 58 and 61).
- Wars differ by seed drift far more than by one plan option: the same German plan falls between April 1944 and after December 1945 on 24 seeds. Compare kinds of player on twelve seeds or more, never on three.
- A whole war now plays in about a minute on four streams, so wide seed sets are cheap.
- A town's garrison is on the region's `garr` list, not among its `units`: a ringed town that looks empty is not. Use `siegeView(g, id).lines` to read how it is fed.
- `T.candidates` returns nothing in a computer-only war (`g.human` is null). Pick the corps yourself and pass them through `limitAttackers` to `T.previewAttack`.

Results of the Version 61 test: `/mnt/project-files/playtest/results-v61/` (the 141-job queue at the top; `lean-3-seeds`, `two-waves-3-seeds`, `card-recount`, `lean-12-seeds`, `first-strike-12-seeds`, `jab-and-hammer`, `why`, `undecided`, `saves`).

## Version 62: a fair cabinet, a say over the men, a secure office

| Script | What it does | Example |
| --- | --- | --- |
| `v62.tsv`, `v62b.tsv`, `v62c.tsv`, `v62d.tsv` | The queues of the Version 62 test for `jobs.sh`: the Version 60 queue again (252 jobs: government, promises, heads, men, cabinet off, battery, scorch, 24 Berlin wars, computer wars); call-up habits on twelve seeds as the Soviet Union, the Soviet first strike and Germany (132); Berlin on seeds 25 to 60 with the cabinet and 1 to 60 with it off (96); an idle player with a secure office and every ordinary war again with the small jobs done (35) | `./jobs.sh v62b.tsv out/v62b 4` |
| `lib.ts` | New plan options: `"earlyClass":"half"`, `"quarter"`, `"war"` or a turn (call the next class early when the reserve is under that share, as soon as at war, or in that month); `"holdCallUp":[from,to]`, `"first:6"` or `"short:0.85"` (hold the standing call-up between two turns, for its first six months, or while under 85% of field divisions are filled); `"secure":true` (the office is secure); `"jobs":true` (carry out the cabinet's build and agent directives) | `node out62n/probe32.mjs SOV 1 11 76 '{"minOdds":1.5,"holdCallUp":"first:6"}'` |
| `probe32.ts` | The men probe. Its `TOTAL` line now also gives the classes called early with their months, the months the call-up was held, the strongest army, who holds Berlin in December 1944, the provinces at the end, and each January's corps, divisions filled of full, and provinces | as above |
| `probe40.ts` | The readings the cabinet has noted for a town as each of the last months opened, beside the promise on offer today (why standing back still moves one town) | `node out62n/probe40.mjs GER 1 7 24 '<plan>' Riga` |
| `mentab.py` | One block a call-up habit: wars lost, against the same seeds as the game stands, Berlin, the reserve, each January | `python3 mentab.py out/v62b -v` |
| `headtab.py` | Heads asked for, read from the cabinet's own words, beside the same wars on an older version | `python3 headtab.py out/v62 out/v60` |
| `kindtab.py` | The kinds of directive issued and how they ended, new beside old, by set | `python3 kindtab.py out/v62 out/v60 gov-play` |
| `settletab.py` | Whether a kept promise clears the count of ignored directives | `python3 settletab.py out/v62` |
| `bandsum.py` | Battles by odds band summed over `probe28` outputs | `python3 bandsum.py out/v62/berlin-GER-*.txt` |
| `berlintab.py` | Berlin held to the end of 1944 with the cabinet and with it off, by seed set and paired seed by seed | `python3 berlintab.py out/v62 out/v62c` |
| `ui23.cjs`, `ui23-all.sh` | The Cabinet card page by page with its height in screens, the head block, the men block with both call-up orders, the office picker and the bill for switching the cabinet off, on a desk and a phone, for a save from `probe34` | `node ui23.cjs out/v62saves/SOVd-11-thinning.json 88-v62-sov-men early,hold` |
| `ui24.cjs` | The arsenal's tooltips and the office picker on the card that starts a campaign | `node ui24.cjs out/v62saves/GERh-7-short.json 88-v62-ger` |

Traps met on Version 62:

- The small arsenal cards with a tooltip are `.rcard` in the province panel (Region tab). The Build tab's cards are `#pane [data-act="arsenal"]` and carry their text in the card, with no tooltip.
- `wingtab.py` cannot read how a take directive ended on files with the card bracket: count those with `cardtab.py` or `kindtab.py`.
- The scripted player does not do the cabinet's small jobs unless the plan says `"jobs":true`. Without it every build and agent directive is missed, so its cabinet falls far more often than a player's would.
- Twenty-four Berlin wars are too few: seeds 1 to 24 read 22 held on Version 62 where 13 held on Version 61. Use sixty or more each way.
- `"holdCallUp":"short:0.85"` never lets the call-up go in a losing war. That is the finding, not a fault of the habit.

Results of the Version 62 test: `/mnt/project-files/playtest/results-v62/` (the 252-job queue at the top; `call-up`, `secure-and-jobs`, `berlin`, `stand-back`, `screens`, `saves`, `off-and-on.txt`).

## Version 63: wars that end, fronts that fight

| Script | What it does | Example |
| --- | --- | --- |
| `v63.tsv`, `v63b.tsv` to `v63j.tsv` | The queues of the Version 63 test for `jobs.sh`: the Version 62 queue and the Version 61 follow-ups again (325 jobs); call-up habits on twelve seeds (`v63b`, 132); sixty Berlin wars for another computer (`v63c`); secure office and small jobs (`v63d`, 35); Berlin on sixty seeds for four kinds of German player, and two of them on Version 62 (`v63e`, 288); leaning, two waves, reading the chance and the Soviet first strike on twelve seeds (`v63f`, 60); the longest silence from January 1942 on Versions 61, 62 and 63 and for the digging player (`v63h`, 96); the end of Germany from the Soviet seat on twelve seeds (`v63i`, 48); the jab and the hammer (`v63j`, 12); the guns-first German on Version 62 for comparison (`v63k`, 24, into the folder of `v63e`) | `./jobs.sh v63e.tsv out/v63e 4` |
| `lib.ts` | New plan option `"dig":2`: a corps that has works and stands beside the enemy stays in them. It joins an attack only where the card reads these odds or better with everyone in, it is not railed away to mass elsewhere, and it does not march. A plan without `dig` plays the identical war | `'{"minOdds":1.4,...,"dig":2}'`, with guns `"guns":1.5` |
| `probe28.ts` | Its `TOTAL` line now also gives the defences fought, lost, on a prepared line, met tired and against guns; the rings closed on the player and the men who surrendered; the longest silence from January 1942; and the line corps standing in works each January and July | `node out63n/probe28.mjs GER 1 7 76 '<plan>'` |
| `probe41.ts` | The end of an enemy great power from the player's seat: the months its ground fell to ten, three and one provinces, when it gave in, the longest run without a battle after ten, the pocket headlines, and what it still holds | `node out63n/probe41.mjs SOV 1 7 76 '<plan>' GER` |
| `ladder.py` | Berlin by habit from `probe28` outputs named `HABIT-GER-SEED.txt`: one line a habit and every pair of habits seed by seed | `python3 ladder.py out/v63e asis dig diggun gun` |
| `endtab.py` | The end of the enemy power from `probe41` outputs: wars in which it gave in, the months from its last ten provinces to the end, wars never finished | `python3 endtab.py out/v63ij end63-SOVF_base end63-SOV end61-SOVF_chance` |
| `silence.py` | The longest run of months without a battle from January 1942, by habit: median, wars with six and with twelve months or more | `python3 silence.py out/v63h silent61 silent62 silent63 silentdig63` |
| `ui25.cjs` | The attack card's new parts on a desk and a phone: the verdict, the bill, the note on the force the staff brought, the fog note, the ticket lines in view and the fold button; with `mine` it goes back to the picked corps, attacks and reads the report | `node ui25.cjs SAVE.json SAVE.txt 89-v63-card mine` |

The queues name their bundles: `out63n` is every program built against Version 63 with this `lib.ts`; `out62d`, `out62e` and `out61d` are `probe28` (and `probe41` in `out61d`) built against Versions 62 and 61 (`./build.sh probe28 /home/user/tc62 out62d`). Building this `lib.ts` against Version 61 warns that `orderOffice` is undefined: harmless.

Traps met on Version 63:

- Twelve seeds misled again. Seeds 1 to 12 read Berlin held in 4 for the plain German player; seeds 1 to 60 read 28. Sixty wars each way, paired by seed.
- A battle report's `works` is set only when an attack is thrown back and wore the works. It does not say whether the defender stood in works: read `prepared`, or the corps' `dug` before the battle. Counting `works > 0` reads "none on works" for a line that is three-quarters dug in.
- A job that is killed still leaves its `.done` marker. Delete `OUT/.done/NAME` and the partial file before the queue runs again.
- Two `jobs.sh` calls chained with `;` play the second only after the first, at the first one's stream count. To queue behind a running queue, wait on `pgrep -f "jobs.sh FIRST.tsv"` in a background shell.
- `ui25.cjs`: the fold button is in the page on a desk but hidden there. Ask `isVisible()` before clicking.
- `"dig"` changes who is attacked as well as who attacks: the Soviet staff leaves a dug-in line alone for most of a year. Read the silence beside the Berlin number.

Results of the Version 63 test: `/mnt/project-files/playtest/results-v63/` (the 325-job queue at the top; `berlin-ladder`, `call-up`, `secure-and-jobs`, `lean-and-strike`, `silence`, `endgame-and-jab`, `undecided`, `stand-back`, `screens`, `saves`).

## Version 64: the line and the men (fold, hand over the line, probe, bomb the line)

| Script | What it does | Example |
| --- | --- | --- |
| `v64.tsv`, `v64e.tsv` to `v64k.tsv` | The queues of the Version 64 test for `jobs.sh`: the Version 63 queue again (`v64`, 325 jobs); Berlin on sixty seeds for the plain and the digging German player, the new orders one at a time on twelve seeds, the Soviet seat (`v64e`, 240); the wars of 1941 with and without a probe and the bombers (`v64f`, 36); what the card reads after softening (`v64g`, `v64h`); the wider relief rule on both versions (`v64i`, not played: see the traps); the relief test (`v64j`, 16); Version 63 again with the newer `TOTAL` line (`v64k`, 24) | `./jobs.sh v64e.tsv out/v64e 3` |
| `lib.ts` | New plan options. `"fold":true` folds two worn corps of a kind standing together every month (`"fold":"raise"` also raises every cadre again first). `"probe":true` probes a dug-in line whose attack reads 0.7 of the bar or better (a number sets the share) and is not a sure thing; `"bombLine":true` sends the bombers at the same line first. `"rotate":0.7` makes a dug-in line corps under that strength change places with a stronger corps of its kind next door | `'{"minOdds":1.4,...,"probe":true,"bombLine":true}'` |
| `probe28.ts` | Its `TOTAL` line now ends with the towns taken and when, the attacks by year (fought, won, on a prepared line, men lost and inflicted), and Version 64's counters: folds, cadres raised, reliefs and the months of works handed over, probes, line raids, lines softened, how many attacks that lifted over the bar, and how often the card's chance read better or worse afterwards | `node out64n/probe28.mjs GER 1 7 76 '<plan>'` |
| `probe41.ts` | The Soviet seat's `TOTAL` line also counts probes, line raids, softened lines and folds | `node out64n/probe41.mjs SOV 1 7 76 '<plan>'` |
| `probe42.ts` | Writes saved games for the page as the player's month opens, the first month each new card is on offer: `-soften` (a dug-in line a corps can probe and the bombers can reach), `-fold` (two worn corps together), `-left` (works standing empty), each with a `.txt` saying what to click | `node out64n/probe42.mjs GER 1 7 30 '<plan>' out/v64saves/GER-7` |
| `probe43.ts` | The relief test: on the turns given, every dug-in line corps changes places with a corps of its kind next door on a copy of the game, and the months of works on the two provinces are added up before and after | `node out64q/probe43.mjs GER 1 7 48 '<plan>' 31,35,41,47` |
| `habits.py` | The new orders by habit from `probe28` outputs: Berlin, towns taken, attacks by year, and the Version 64 counters; a habit may be read from another folder as `DIR/HABIT` | `python3 habits.py out/v64e asis probebomb --seeds 1-24` |
| `watchtab.py` | How the computer-only wars of `watch.mjs` end, a folder a line: who falls and when, and the wars in which neither Germany nor the Soviet Union has fallen by the last month | `python3 watchtab.py out/v63 out/v64` |
| `ui26.cjs` | The new cards on a desk and a phone from `probe42` saves: `soften` (the row on the attack card, the probe, the raid planner and the raid), `fold` (the tray's note, the fold card, the fold, the cadre in the arsenal), `left` (the note on works standing empty and the banks on the map) | `node ui26.cjs soften SAVE.json SAVE.txt 90-v64-soften` |

Bundles: `out64` is the Version 63 queue's programs against Version 64; `out64n` is `probe28`, `probe41` and `probe42` with the new plan options; `out64p` adds the better-or-worse count; `out64q` and `out63q` are `probe28` and `probe43` with the wider relief rule against Versions 64 and 63. Building this `lib.ts` against Version 63 warns that `foldChoicesAt`, `orderFold`, `softenOptions` and `orderProbe` are undefined: harmless, the options do nothing there.

Traps met on Version 64:

- The scripted player does not rotate. `"rotate"` found a relief about once in ten wars, because its whole army stands in the line with nothing of the same kind behind it, so the `rotate` and `rotate63` sets read exactly like the plain player. Use `probe43` for what a relief costs, and the share of the line in works (`ladder.py`) for what the hand-over does in ordinary play.
- A relief between two full provinces cannot be ordered: neither corps can step into a province that already holds five. In `probe43` two tries in three end as "no road, a full province". The worn corps has to step out first.
- Softening everything wastes men: without a rule the player probed lines it already beat at 6 to 1. A sure thing (chance 0.9 or better and over the bar) is left alone.
- The card's odds are not its chance: after a probe the odds number often falls (the corps that probed is out of the attack and the fog has lifted) while the chance rises (the works are a month weaker). Count better and worse by the chance.
- `.leftworks` is also the class of the banks drawn on the map: read `.note.leftworks` for the province panel.
- `ladder.py` read the line-in-works table to the end of the `TOTAL` line; it now stops at `; took`.
- Do not rebuild a bundle folder a running queue is using: build changed programs into a new folder (`out64p`, `out64q`).

Results of the Version 64 test: `/mnt/project-files/playtest/results-v64/` (the 325-job queue at the top; `berlin-and-new-orders`, `wars-of-1941`, `relief-test`, `undecided`, `screens`, `saves`).

## Version 65: rival staffs (characters, main efforts, staffs that probe, bomb and fold)

| Script | What it does | Example |
| --- | --- | --- |
| `v65.tsv`, `v65e.tsv`, `v65f.tsv`, `v65j.tsv`, `v65s2.tsv`, `v65s3.tsv` | The queues of the Version 65 test for `jobs.sh`: the Version 63 queue again with the computer wars, the page sessions and the `play` wars first (`v65`, 325 jobs); Berlin on sixty seeds for the plain and the digging German player (`v65e`, 120); the new orders one at a time on twelve seeds and the Soviet seat (`v65f`, 108); the relief test on eight seeds (`v65j`); why a front slept, on thirty wars (`v65s2`, and `v65s3` with what set the staff's clock back) | `./jobs.sh v65e.tsv out/v65e 4` |
| `lib.ts` | `"rotate"` now changes a worn corps and its relief in one order where the rules have `orderSwap` (Version 65), and falls back to the two marches on older versions | |
| `probe28.ts` | Follows every main effort aimed where the player stands: an `EFFORT` line each (named when and where, places aimed at, battles, how many the player lost, on a prepared line, card odds, men lost each side, provinces taken, town assaults, ground given up, the first word in the news and the first blow), a `C a main effort hits the player` row in the battle table, and a `STAFFS` line a war (each staff's character, efforts by year, months under an effort and under a general offensive, enemy probes and raids on the player's works, headlines) | `node out65/probe28.mjs GER 1 7 76 '<plan>'` |
| `staffs.py` | Adds up the `STAFFS`, `EFFORT`, battle-table and `TOTAL` lines of a folder by habit, and splits Berlin and the silence by the Soviet staff's character | `python3 staffs.py out/v65e asis dig` |
| `probe43.ts` | The relief test with the one-order change of places; prints a `SWAPS` line with every refusal by its reason (`whyNoSwap`) | `node out65/probe43.mjs GER 1 7 48 '<plan>' 31,35,41,47` |
| `probe44.ts` | Computer-only wars that never end: from the month Germany or the Soviet Union falls, each January and at the end it prints who is at war, where the great powers' ground touches, each staff's line, the armies and where they stand, and a tally of the last twelve months | `node out65/probe44.mjs 4 89` |
| `probe45.ts` | Why a front sleeps: as each of the player's months opens from January 1942 it notes the enemy staff's flags, the corps each side of the line, the best attack as it stands and massed, the staff's own count of the front (`fronts().ratio`) and of the quiet (`quietWith`); it prints the longest silence month by month, a `QUIET` line and a `WHY` line with the reason nothing was named, in the staff's own order, and what last set its clock | `node out65c/probe45.mjs GER 1 33 64 '<plan>'` |
| `probe46.ts` | Follows a landing through one month, nation by nation | `node out65/probe46.mjs 28 63 66 UK` |
| `probe47.ts` | Loads a save from the page, ends the month and prints the works a corps holds and the works standing empty | `node out65/probe47.mjs SAVE.json 137` |
| `probe48.ts` | What a probe by a worn corps does: the probe row of a `-soften` save with the player's army at full, a half, a quarter and a tenth of its men | `node out65c/probe48.mjs GER-7-soften.json GER-7-soften.txt` |
| `v65x.tsv` | A what-if, not a test of the build: `probe28` for the digging German on the 26 seeds with a long silence, built against two scratch copies of the rules with `data/campaign/rivals.json` changed (`x1`: every character's `strength` 0, so a main effort is judged by its sector alone; `x2`: the same and the cautious staff's `quiet` 3). The copies are not in the repository: copy `src` and `data`, change the file, and build with `./build.sh probe28 COPY out65x1` | `./jobs.sh v65x.tsv out/v65x 3` |
| `probe49.ts` | Every staff's character on a run of seeds | `node out65c/probe49.mjs 1-12 SOV` |
| `ui27.cjs` | Page checks on Version 65: `phone` (six screens: how much of the map a tap reaches before and after an army is picked, how many lit provinces a finger can find), `swap` (the "Change places" card on a desk and by press and hold on a phone), `powers` (the staff lines and the character card), `cadre` (a card greyed for want of money) | `node ui27.cjs phone SAVE.json 91-v65-phone` |

Bundles: `out65` is every program against Version 65; `out65b` and `out65c` hold programs changed while a queue was using `out65`.

Traps met on Version 65:

- The silence in the `TOTAL` line counts battles of the player's own corps only. It leaves out town assaults and ground given up without a fight, and the staff's quiet clock (`quietWith`) counts both, so a month this harness calls silent may still set the staff's clock back. `probe45` says which.
- A staff's count of a front (`fronts().ratio`) weighs the whole enemy army in depth, the reserve far behind the line too. It reads far lower than the corps count on the line (0.6 to 1 where the line reads 1.5 to 1), and the main effort is gated on it.
- An effort is dated by the month it was named (`effAt`). A first blow in the same month fell in the same move, before the player had a turn.
- An order on the page needs a right-click on a desk or a press and hold on a touch screen; `onRegion` only picks. On a phone the province must first be dragged out from under the tray.
- The treasury on the page is `pp` times about a hundred: set `pp = 1.36` for a player 136 short, not 136.
- The `rotate` habit's "months handed over" read 66% on Version 65 because it counted months stood beyond three; works are finished at three months and `lib.ts` now caps the count there. No works were lost (`probe43`: 536 of 540 pairs, all kept).
- The table scripts read the last line that starts with `TOTAL`. New summary lines take other first words (`STAFFS`, `EFFORT`, `SWAPS`, `QUIET`, `WHY`).

Results of the Version 65 test: `/mnt/project-files/playtest/results-v65/`.

## Version 66: how deep you play (three depths, six desks the staff can run, an allowance)

| Script | What it does | Example |
| --- | --- | --- |
| `v66e.tsv`, `v66f.tsv`, `v66g.tsv`, `v66h.tsv` | The queues of the Version 66 test for `jobs.sh`: five replay wars, then seeds 1 to 60 for the guns-first shopper, the infantry-only shopper, Field command and Theatre command (`v66e`, 245 jobs); Field command with research kept by hand and with a full allowance on sixty seeds, a quarter allowance on twelve (`v66f`, 132); the builder's seeds 25 to 36 with the six desks, or spies, air and navy, staffed and nothing else changed (`v66g`, 24); the building desk alone and the cabinet off alone on seeds 25 to 32 (`v66h`, 16) | `./jobs.sh v66e.tsv out/v66e 4` |
| `lib.ts` | Plan options `"depth":"field"\|"theatre"\|"supreme"`, `"desks":[...]` (hand these desks to the staff), `"byHand":[...]` (take these back), `"cap":25\|50\|75\|100` (the staff's allowance). The depth is set before the eve of war, as the page sets it. The test player leaves a staffed desk alone: no research of its own, no trade, no arsenal orders, none of the agent or building small jobs | `'{"depth":"field","byHand":["research"]}'` |
| `probe28.ts` | Two more lines a war: `DEPTH` (the depth and the desks staffed, the treasury and the men left each January, the highest treasury, what the staff spent on building, how many months the Staff tab said there were no men to spare and from when) and `LINES` (the staff's news lines by desk, how many wordings, how many word for word the same as that desk's line the month before, the most repeated) | as before |
| `depthtab.py` | Adds up the `DEPTH`, `LINES` and `TOTAL` lines of a folder by set: Berlin, wars still undecided at the end, the middle war's treasury and men each January, the staff's lines | `python3 depthtab.py out/v66e art inf field theatre` |
| `bycmp.py` | My wars against the rules builder's on the seeds both played (`/mnt/project-files/theater-command/results/v66/`): Berlin each way and the months between the two falls, seed by seed | `python3 bycmp.py out/v66e field d66f2` |
| `probe50.ts` | One war month by month as the player's month opens: the depth, the treasury, the staff's spending and what is left of its allowance, the men, the corps, the Staff tab's state for each staffed desk when it changes, and every staff line; then a table by year and each desk's wordings and repeats | `node out66/probe50.mjs SOV 1 7 76 '{"minOdds":1.5,"depth":"field"}'` |
| `probe51.ts` | Writes a save for the page at the turns asked for, as the player's month opens | `node out66b/probe51.mjs GER 1 25 56 '<plan>' out/v66saves/GER-25-field 41,53` |
| `ui28.cjs` | Page checks on a desk and two phones: `start` (the new-campaign card: the depth picker, how long the card is, where Begin sits, a campaign begun) and `staff` (a campaign at each depth four months in: the tabs, the Staff tab's length and smallest buttons, the allowance, "Look in" and the notice on a staffed panel, "Take it back", the settings card, a change of depth and the question it asks) | `TC_PAGE=file:///home/user/tc66/dist/play.html node ui28.cjs staff shots/92-v66` |
| `ui29.cjs` | What a player who hands desks over has to read after four months: unread counts, the News tab's lines by kind, the Battles tab, and where a Supreme command player finds the way to hand a desk over | `node ui29.cjs shots/92-v66` |
| `ui30.cjs` | A late-war Field command save on a desk and a phone: the top bar, the Staff tab's building row, the Build panel once looked in on | `node ui30.cjs out/v66saves/GER-25-field-53.json shots/92-v66-1944` |
| `monkey.cjs` | A sixth argument sets the depth | `node monkey.cjs GER 21 600 1 colonel field` |

Bundles: `out66` is every program against Version 66 (e928f7b); `out66b` holds `probe51`, built while a queue was using `out66`.

Traps met on Version 66:

- Set the depth before the eve of war, as the page does (`newGame` calls `setDepth` before the first month opens). The builder's first Field command number (42 of 48) came from a harness that set the depth during the first month, so September 1939 was played by hand and the test player's own research (winter equipment first) carried the war. Set as the page sets it, Field command reads the same as playing by hand. The builder replayed both and agreed.
- Our two test players are one program: on the builder's seeds the wars match to the month Germany falls whenever no desk is staffed. A difference by seed means a difference in set-up, not luck. `bycmp.py` finds it in a minute.
- `window.theater.newGame(...)` leaves the new-campaign card open over the game: close it with `window.theater.openModal(null)` before clicking a tab.
- The News tab is `data-tab="reel"`. At Supreme command there is no Staff tab.
- A shopping list of corps only (`"buy":["corps:art","corps:inf"]`) lets the treasury pile up once the men run out. That is the list, not the game: only the Field command pile is a finding.
- The air force is refilled to its establishment every month, so the air staff's losses (about 1,600 aircraft in a German war) do not shrink it.
- `T.commandOf` can be read in a hook without changing the war (checked against `probe41`, which never calls it).
- Side tests on the same four cores slow the main queue: the 245 wars took 50 minutes with page checks and three side tests running beside them.

Results of the Version 66 test: `/mnt/project-files/playtest/results-v66/`.

## Version 67: critical fixes (the phone tray, warnings a month ahead, sleeping fronts, Field command)

| Script | What it does | Example |
| --- | --- | --- |
| `v67.tsv`, `v67e.tsv`, `v67f.tsv`, `v67s.tsv` | The queues of the Version 67 test for `jobs.sh`: the older checks (`v67`, 325 jobs); Berlin on seeds 1 to 60 by hand, digging, at Theatre and Field command, guns first and infantry only, with seeds 61 to 72 for the builder's 25 to 72 (`v67e`, 480); the orders one at a time and the Soviet seat (`v67f`, 108); a war from every major seat by hand and at Field command on seeds 1 to 24 (`v67s`, 240) | `./jobs.sh v67e.tsv out/v67e 4` |
| `probe28.ts` | One more line a war, `EAST`: the months the eastern war lasted, the months with a battle in it, and the longest silence while it lasted (a silence after the Soviet Union falls is a different fault) | as before |
| `easttab.py` | Adds up the `EAST` lines of a folder by set: wars with six months and with a year of silence in the east, when the Soviet Union fell, and the silence after it fell | `python3 easttab.py out/v67e dig field` |
| `probe52.ts` | Why a front slept, month by month: each side's best odds on every sector, the enemy staff's bar and patience, and the best massed attack it could have made | `node out67/probe52.mjs GER 1 7 76 '<plan>'` |
| `probe53.ts` | A war from any seat, with a `SEAT` line: whether the player's country stands, how the war went for its side, which great powers fell and when, the months at war, the months with a battle of the player's, the longest silence, and the provinces held at the start, the most and the end | `node out67/probe53.mjs UK 1 7 76 '{"minOdds":1.5,"land":true}'` |
| `efftab.py` | The main efforts of a folder by nation: named, fought, the month of the first blow against the month of the first word in the news (a month ahead, the same month, after, never) | `python3 efftab.py out/v67e dig` |
| `ui31.cjs` | The Build tab's rows on a desk and a phone: which greyed rows print a reason, which only carry a tooltip | `TC_PAGE=file:///home/user/tc67/dist/play.html node ui31.cjs SAVE.json 93-v67-build` |

Traps met on Version 67:

- `ui27.cjs` puts `shots/` in front of the prefix itself. Pass `93-v67-phone`, not `shots/93-v67-phone`.
- On a phone held sideways the tap that should pick an army opens an alert's card, because the alerts lie over the map. A count of provinces reached is then a count with a card open. Read the screenshot before quoting the number.
- Count silence only while the eastern war lasts. At Field command most of the long quiet comes after the Soviet Union has fallen.
- The scripted by-hand player spends four marks in ten on fighters. When the staff beats it, say so: it is a poor shopper, not a careful player.

Results of the Version 67 test: `/mnt/project-files/playtest/results-v67/`.

## Version 68: the war retuned, and a fair shot from every seat

Brian's yardstick (2026-11-14): at Colonel a careful player beats their country's real history in about half of their wars. Par: Germany holds Berlin at the end of 1944; the Soviet Union beats Germany before May 1945 and never loses Moscow; Britain is ashore in the west to stay before June 1944 and is never invaded; France holds Paris at the end of 1940; Italy holds Rome at the end of 1943.

| Script | What it does | Example |
| --- | --- | --- |
| `v68s.tsv` (`v68sa.tsv`, `v68sb.tsv` are its two halves), `v68paris.tsv`, `v68e.tsv`, `v68.tsv`, `v68f.tsv` | The queues of the Version 68 test: every seat by hand and at Field command on seeds 1 to 48, France digging on 1 to 24, and a player who never gives an order on 1 to 12 (`v68s`, 588 jobs); France again with the month the capital fell on the line (`v68paris`, 96); Berlin for the six German players on seeds 1 to 60 and 61 to 72 (`v68e`, 408); the older checks (`v68`, 325); the orders and the Soviet seat (`v68f`, 108) | `./jobs.sh v68s.tsv out/v68s 4` |
| `probe54.ts` | `probe53` with the par line: `par BEATEN` or `missed` and why, the home provinces in enemy hands at the worst, the months with an enemy on home soil, the month the capital was first lost, Soviet levies, the month Britain was ashore in the west to stay, and the overseas provinces held each December | `node out68/probe54.mjs FRA 1 30 76 '{"minOdds":1.6,"homeOnly":true}'` |
| `seattab.py` | Adds up the `SEAT` lines by seat and set: stands, won, lost, par beaten, the fall of the player's country and of its chief enemy, the share of months with a battle, the silence. `--rows` prints every war | `python3 seattab.py out/v68s seat seatfield seatidle --seeds 1-48` |
| `seatvs.py` | What a player in the seat changes: the month Germany falls with a player against the builder's computer-only wars on the same seeds (`fin68L-*.txt`) | `python3 seatvs.py out/v68s seat seatfield` |
| `peek.ts` | Prints a country's provinces (home and overseas) and its collapse numbers | `node out68/peek.mjs FRA` |
| `probe55.ts` | Writes a save for the page after the player's orders of a month and before End month, at the turns asked for | `node out68b/probe55.mjs SOV 1 11 33 '<plan>' out/v68saves/SOV-11-soft 31` |
| `ui32.cjs` | Loads such a save, presses End month three times and reports the page's errors and the date on the top bar each time: what a player sees when the rules stop on an error | `TC_PAGE=file:///home/user/tc68/dist/play.html node ui32.cjs SAVE.json 95-v68-crash` |

The plans of the five seats (the same player by hand; add `"depth":"field"` for the staff, and `{"idle":true}` for the player who never gives an order):

- Germany `{"minOdds":1.4,"declare":[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":true}`
- Britain `{"minOdds":1.5,"land":true,"buy":["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]}` (no shopping list at Field command)
- Soviet Union `{"minOdds":1.5}`
- Italy `{"minOdds":1.4,"declare":[[10,"FRA"],[10,"UK"],[14,"GRE"]],"land":true}`
- France `{"minOdds":1.6,"homeOnly":true}`, and with `"dig":2` for the digging player

Bundles: `out68` is every program against Version 68 (56f2523); `out68b` holds the newer `probe54`, built while a queue was using `out68`.

Traps met on Version 68:

- Always play the seat with a player who never gives an order as well. If doing nothing beats par (Germany 12 of 12, Italy 12 of 12), the line is not measuring play.
- My British script lands only when the odds allow; the builder's lands on a date and counts an early German fall as par beaten. The two read 0 of 48 and 14 of 24 by hand on the same rules. Say which player and which reading a number comes from.
- A par line read at the end of a month and one read as the player's next month opens differ by the enemy's move: France on seed 30 holds Paris at the end of December 1940 one way and not the other.
- France is out the month Paris falls, with a third of the home country lost and every overseas province still French. Counting provinces at the fall shows how early the rule ends the war.
- A note to another thread or to the coordinator is refused over 2,000 characters. Draft to 1,900 and count with `wc -c`.
- `xargs -I{}` breaks the quotes of a JSON plan. Write a queue file and use `jobs.sh`.
- A run that prints `ERROR ... computer month` is a finding, not a harness fault: `jobs.sh` leaves it in the folder and the queue's last line counts them (`grep -l "^ERROR"`). One in about 2,200 runs on Versions 67 and 68 (`end65-SOV_soft-11`: `pocketMonth` reads the stores of an ally's town it never marked). The error comes in the computer's months after the player's End month, so save in `each` at the turn before the month the error names (`probe55`), and press End month on the page (`ui32.cjs`).

Results of the Version 68 test: `/mnt/project-files/playtest/results-v68/`.

## Version 69 and the world domination test

Version 69 (46126a8) gives every par line a second leg, so that a player who never gives an order cannot meet it:
Germany beats France and holds Berlin at the end of 1944; the Soviet Union beats Germany within 47 months of the
invasion and never loses Moscow; Britain has corps ashore in the west to stay before June 1944 and before Germany falls,
and is never invaded; France holds Paris as October 1940 opens; Italy is at war with Britain by the end of 1940 and holds
Rome at the end of 1943. Brian, 2026-11-15: "I also want each faction to have an actual chance to global domination"
(first target: about one war in ten at Colonel), measured from Version 70.

| Script | What it does | Example |
| --- | --- | --- |
| `probe57.ts` | `probe54` with the Version 69 par lines; the lines as first told are still counted (`old line beaten`). Britain is ashore when British corps stand in the west, not when Britain holds provinces there: a liberated France takes its provinces back | `node out69/probe57.mjs UK 1 30 76 '<plan>'` |
| `probe56.ts` | World Domination from any seat: the seat's player orders it the month the rules offer it and plays to the turn given (124 is December 1949); an undecided war is given up at turn 88 (`"giveUp"`). One `DOM` line a war: decided when and for whom, the last act begun, WON or how many nations were left, the share of the map's worth at the end and at the most. `"landLog":true` says why no landing was made; `"dominate":false` takes the victory instead | `node out69/probe56.mjs SOV 1 7 124 '{"minOdds":1.5,"land":true}'` |
| `domtab.py` | Adds up the `DOM` lines by seat and set; `--rows` lists every war | `python3 domtab.py out/v69d dom domfield` |
| `v68d.tsv`, `v69d.tsv` | The domination baseline: five seats by hand and at Field command on seeds 1 to 24 (240 jobs) | `./jobs.sh v69d.tsv out/v69d 4` |
| `v69s.tsv` (`v69sa`, `v69sb`), `v69e.tsv`, `v69.tsv`, `v69f.tsv` | The Version 68 queues again against Version 69 (`out69`, `probe57` for the seats) | `./jobs.sh v69sa.tsv out/v69s 4` |

Bundles: `out69` is every program against Version 69.

## Version 70: the history dial

Version 70 (f7aafe0) adds a five-stop history dial to the grand campaign: 1 By the book, 2 Close to history, 3 History's
shape (the game as it was, and what a campaign with no stop plays), 4 Loose (neutrals with claims), 5 Anything goes.
With no stop the wars are Version 69's, seed for seed. Par is measured at stops 2 and 3, the domination column at stop 3.

Setting a stop: a plan's `"stop": n`, or `TC_STOP=n` in the environment for programs that take no plan (`watch`). The
program must be built with `build70.sh`, which puts `stop70.ts` in front of it; `lib.ts` calls it straight after
`startOpen`, as the page does (for a later start year, after the computer has played history up to the handover).

| Script | What it does | Example |
| --- | --- | --- |
| `build70.sh` | As `build.sh`, for Version 70 or later (default repo `/home/user/tc70`, out `out70`) | `./build70.sh probe58` |
| `probe58.ts` | `probe57` (the `SEAT` line) and one `DIAL` line a war: when the west, Norway, Italy's war, Greece, Yugoslavia and the east opened, when the war was decided, where the book was closed and why, every march and how many months after its warning it came, treaties and joiners, and for each claim on the player's ground the month it could first be read, was pressed and was marched for | `node out70/probe58.mjs ITA 1 3 76 '{"minOdds":1.4,"declare":[[10,"FRA"],[10,"UK"],[14,"GRE"]],"land":true,"stop":5}'` |
| `probe59.ts` | Nobody at the controls at one stop, to December 1949: one `WATCH` line a war (decided when, or who still fights whom) | `node out70/probe59.mjs 5 17,18,19 124` |
| `mk70x.py`, `v70x.tsv` | The Version 70 queue (656 jobs): stop 1 as Germany (by the book, my usual plan, never striking west, idle) and from the other seats; stops 4 and 5 as Italy and from the other seats; par at stop 2 on seeds 25 to 48; start years 1940 to 1942 at stops 1, 2, 4, 5; wars watched to 1949 | `python3 mk70x.py && ./jobs.sh v70x.tsv out/v70x 4` |
| `ui33.cjs` | The dial on the page: `card W H PREFIX` (the new-campaign card at a screen size, then the first month and the Powers tab), `claims SAVE PREFIX` (three months round a claim: news, cards, the Powers tab), `book PREFIX` (an idle Germany by the book: when the book closes and how the player is told), `settings SAVE PREFIX` (the dial turned mid-war and kept through a reload) | `node ui33.cjs claims out/v70ui/ita5-10.json 97-v70-claims` |
| `ui34.cjs` | The month a neighbour marches on the player: the notices as the month opens and where the lines stand in the newsreel | `node ui34.cjs out/v70ui/ita5-12.json 97-v70-march` |

Saves for the page come from `probe55` built with `build70.sh` (a plan with `"stop"`).

## Version 71: the rule for winning the world, a German staff that will not be ringed

Version 71 (dfa1cef) shipped while the Version 70 sets ran, so the Version 70 queue was stopped after stop 1, stops 4 and
5 as Italy and par at stop 2 for Germany and the Soviet Union (244 wars); everything else is played on Version 71.
The world is now won by the last great power standing once it holds three quarters of the map's worth. `probe56`
prints the two tests at the end of its `DOM` line (rival great powers left, the player's share) and `domtab.py` adds
them up. Build with `./build70.sh NAME /home/user/tc71 out71`.

| Script | What it does | Example |
| --- | --- | --- |
| `mk71.py`, `v71.tsv` | The Version 71 queue (1,244 jobs), most wanted first: par at the middle stop on seeds 25 to 48; the domination set (five seats, seeds 1 to 8, by hand, at Field command, idle); par at stop 2; start years 1940 to 1942 at stops 1, 2, 4, 5; stops 4 and 5 from every seat; wars watched to 1949 at every stop; par on seeds 1 to 24, the idle player, the French player who digs in | `python3 mk71.py && ./jobs.sh v71.tsv out/v71 4` |
| `dialtab.py` | Adds up the `DIAL` lines by set and seat | `python3 dialtab.py out/v71 seat2 seat2field` |
| `probe60.ts` | The British player by hand on one seed, a line a month from January 1942: British corps in the west, German corps in France and the Low Countries and in the whole west, the landings ordered and how they went, the landing log by reason | `node out71/probe60.mjs 44` |
| `probe61.ts`, `westtab.py` | One `WEST` line a war from the British seat by hand: at January 1942, January 1943, June 1943, December 1943 and May 1944 the German corps in France and the Low Countries and in all, who holds those provinces, where the British corps stand, and every landing from 1942 on; `westtab.py DIR [--rows]` adds up `w70-SEED.txt` and `w71-SEED.txt` (how the west looked when the player landed, Version 70 beside 71) | `node out71/probe61.mjs 44 > out/v71w/w71-44.txt; python3 westtab.py out/v71w` |
| `ui36.cjs` | The new win rule on the page: the offer card when the war is decided and the two tests on the Powers tab | `node ui36.cjs out/v71ui/sov3-59.json 98-v71-lastact` |
| `probe62.ts` | Who sets foot on the player's home soil, where and when: one `HOME` line for every month a home province is in other hands | `node out71/probe62.mjs UK 39 48 '{"minOdds":1.5,"land":true,"depth":"field"}'` |
| `ui37.cjs` | The month as the player meets it: presses the End month BUTTON and reads every card that opens by itself (the Dispatches card's event cards and plain lines, then whatever waits behind it). Use this, not `ui33 claims`/`ui34`, for anything about what the player is told: those two step the month with `window.theater.nextMonth()`, the watcher's step, which opens no Dispatches card, and they made a wrong finding on Version 70 | `TC_PAGE=file:///home/user/tc72/dist/play.html node ui37.cjs out/v70ui/ita5-10.json 99-v72-end 3` |
| `v72.tsv` | The Version 72 queue: `v71.tsv` without the domination jobs, against `out72` (the domination column on Version 72 is read with the builder's crossing routine instead) | `./jobs.sh v72.tsv out/v72 4` |
| `lib.ts` plan options `cross`, `crossFoe`, `crossFirst`, `engs` (Version 72) | `cross` puts the rules builder's own crossing routine (`prepareCrossing`, copied word for word from `/mnt/project-files/theater-command/results/v72/lib.ts`) in the scripted player's hands: it names the way that can be made safest as the plan, hands the air and navy desks to the staff, gathers up to four corps on the quay and lands at a danger the player bears (`landRisk`). `crossFoe` names the one enemy to cross to. `crossFirst` calls it before the army's moves (the builder's harness calls it after them, when a marching army has no moves left). `engs` keeps that many engineer corps so that cut railways are mended | `'{"minOdds":1.5,"land":true,"cross":true,"crossFoe":"UK","landRisk":0.4}'` |
| `probe56.ts` (Version 72), `crosstab.py` | With `cross` in the plan the domination probe also prints one `CROSS` line a war: for how many months the way was open at a danger the player bears, for how many a force stood ready on the quay, both at once, every landing and what came of it. `"crossLog":true` prints the routine's month by month log. `crosstab.py DIR PREFIX... [--rows]` adds the lines up | `python3 crosstab.py out/v72d dom domfirst domfirsteng domfield` |
| `probe63.ts` | Why nobody stands on the quay: in the months asked for, the quay the routine chose, who is there, how many corps could reach it by rail or road, how many of the player's provinces have the line cut, the engineers, and what became of the men ashore (strength, supply, who faces them, whether the seas behind are shut) | `node out72f/probe63.mjs SOV 4 96 PLAN 85 96` |
| `ui38.cjs` | The sea crossing as the player meets it on the page: where the navy is at this depth, the list "Crossing the sea" with each row's loss, bar and reckoning, the sea lift, and the landing card (or the refusal) for the force on the quay. Needs a save with a crossing named as the plan (`probe55` with `"cross":true`) | `node ui38.cjs out/v72x/gercross-17.json 97-v72-cross` |
| `v72d.tsv` | The Version 72 domination set: five seats, seeds 1 to 8, to December 1949, with the crossing routine by hand (`dom-`) and at Field command (`domfield-`), idle (`domidle-`), and two more Soviet players (`domfirst-`, `domfirsteng-`), against `out72f` | `./jobs.sh v72d.tsv out/v72d 4` |

## Version 73: the sea behind the beach, a German staff that rests

Build against the version under test, for example `./build70.sh probe56 /home/user/tc73 out73e`. Version 73 changes only a computer Germany's play, so Germany's own seat by hand is the same war as on Version 72: play two seeds each way, compare the files with `cmp`, copy the rest and leave a note file in the folder.

| Script | What it does | Example |
| --- | --- | --- |
| `lib.ts` (`holdSeen`, `holdNote`) | Logging only, play unchanged: each landing that gets ashore is kept with what the card said of the sea behind the beach (`previewLanding(...).hold`: the sea, open or closed, enemy, own, need, harbour), and the routine's `LANDED` line says the same | |
| `probe56.ts` (Version 73), `holdtab.py` | The domination probe follows every landing for three months: one `HOLD` line a war (how often the card warned) and one `HELD` line a landing (what the card said, whether the sea was closed a month later and in any of the three months after, whether the force was fed, corps on that ground after 1, 2 and 3 months). `holdtab.py DIR PREFIX... [--rows]` sets the forecast against what happened. The corps count is the whole army on a mainland where the army also stands: read "still there" only for islands and Britain | `python3 holdtab.py out/v73h dom domfirsteng --rows` |
| `probe55.ts` | Writes a save at the month asked for with a crossing named as the plan. It now passes `{ until }` (it used to play the whole war). A save is made after the player's orders, so with `"landRisk":0` the routine prepares and never lands, and the landing card can be read for an open crossing | `node out73p/probe55.mjs GER 1 1 22 '<plan with "cross":true,"landRisk":0>' out/v73x/gerwait 17,18,19,20,21,22` |
| `ui39.cjs` | The crossing on the Version 73 page: the Staff tab's pointer and its button, where "Crossing the sea" stands in the Navy pane, the red line under a crossing, the ticket "Once ashore", the landing card's ticket and its foot note, and what scrolls on the card (`#card`) | `node ui39.cjs out/v73x/gerwait-18.json 98-v73-cross18` |
| `v73.tsv` | The Version 73 par queue (588 jobs): the middle stop on seeds 25 to 48 by hand and at Field command, Britain first; the idle player; Close to history; the French player who digs in | `./jobs.sh v73.tsv out/v73 4` |
| `v73x.tsv`, `v73h.tsv`, `v73d.tsv` | The domination set with the follow-up lines: the two crossing seats first (Soviet `domfirsteng-`, German `dom-`, seeds 1 to 8), then every seat by hand, at Field command and idle | `./jobs.sh v73d.tsv out/v73d 4` |
| `v73c.tsv` | The control: the same eight German wars by hand without the crossing (`domnocross-GER`), to see what the crossing costs a German player | `./jobs.sh v73c.tsv out/v73d 2` |
| `v73w.tsv`, `v72w.tsv` | The war with nobody playing, to December 1949 at the middle stop: 24 wars on Version 73 (seeds 17 to 40), 12 more on Version 72 (seeds 29 to 40), three wars a file | `./jobs.sh v73w.tsv out/v73 2` |
| `watchend.py` | The war with nobody playing: when Germany fell, by seed, from the `WATCH` lines; the middle war and how many end within three months of May 1945 | `python3 watchend.py out/v73 out/v72 --seeds 17-40` |
| `holdall.py` | The same forecast against outcome for every seat together: by seat and how played, then totals for Britain (the stronger navy) and for the four seats with the weaker navy. A landing in the player's last month has no outcome and is left out | `python3 holdall.py 'out/v73d/dom-[FIU]*.txt' 'out/v73d/domfield-*.txt' 'out/v73h/*.txt'` |

## Version 74: the wider world, North America

Version 74 opens the grand campaign on a wider map (479 provinces, 44 seas) with the United States, Canada and Mexico on the start card. `lib.ts` starts every campaign through `openIt`: the wider map by default, and the map of Europe with `TC_EUROPE=1` (the rules' `startOpen(g, false)`), which plays Version 73's wars exactly. Check that first: play `seat-UK-25` and `seat-GER-25` with `TC_EUROPE=1` and compare them with Version 73's files with `cmp`. A 76-month war takes about 57 seconds on the wider map with four streams (48 on Europe). Build with `./build70.sh NAME /home/user/tc74 out74`.

| Script | What it does | Example |
| --- | --- | --- |
| `mk74.py`, `v74.tsv` | The Version 74 queue (578 jobs), most wanted first: the war with nobody playing (seeds 17 to 40, to 1949); the domination set with the follow-up lines for every landing (seeds 1 to 8, the United States added); the new seats (the United States, Canada, Mexico) by hand, at Field command and idle; par on seeds 25 to 48; the idle sets | `python3 mk74.py > v74.tsv; ./jobs.sh v74.tsv out/v74 4` |
| `v74b.tsv` | Mexico with a path: joins the Allies in the first month and declares on Germany in May 1942 (`seatpath-MEX`), joins the Axis and attacks the United States in September 1940 (`seatpathaxis-MEX`). A neutral Mexico or United States is refused a declaration on Germany (it is not `declarable`), so `seatally-MEX` and `seatearly-USA` in `v74.tsv` show the refusal | `./jobs.sh v74b.tsv out/v74 4` |
| `ui40.cjs` | The landing card on the page: the headline, its class and colour, the line under it, "The fight for the beach" and the ticket, read before the card is scrolled; a picture of the top and of the foot | `node ui40.cjs out/v73x/gerwait-18.json 99-v74-cross18` |
| `ui41.cjs` | A new seat as a player meets it: the new-campaign card (who can be picked, which years), the campaign begun from the card, where the map opens, the chart zoomed out, what Declare war offers, the Powers, Build and Staff tabs, then End month a few times. `TC_YEAR=1942` picks a start year | `TC_YEAR=1942 node ui41.cjs start USA 1440 900 99-v74-usa42 3` |
| `ui42.cjs` | A small country's "Your path" card from a new seat: the card, the choice taken, what Declare war offers after it, a few months ended | `node ui42.cjs path MEX 1440 900 99-v74-mex-path allies 4` |
| `peek74.cjs` | Where the player's army and fleets stand in a save, read through the page | `node peek74.cjs out/v74x/usa-40.json` |
| `probe64.ts` | From a save: for each port where the player's corps stand, the friendly ports they can be shipped to, the enemy coasts they can land on, three landing cards, and the crossings on the navy desk. A save made after the player's orders shows no moves left | `node out74/probe64.mjs out/v74x/usa-29.json` |
| `probe65.ts` | "A war saved in mid-course loads back identical": a war with nobody playing is saved at the months asked for, loaded, and the loaded copy is played beside the war that never left memory, compared month by month | `node out74/probe65.mjs 25 6,16,30 4` |
| `lib.ts` (`stage`) | A plan option for a seat an ocean away (the United States, Canada): corps with no front in reach sail to an ally's port that faces an enemy coast (the one where most of the player's corps already stand), and `land` then lands from an ally's port too. Without it the troopship routine sends an American army to whatever port lies beside a land front (Norway) and it never lands in France | `'{"minOdds":1.5,"land":true,"stage":true,...}'` |
| `v74s.tsv`, `v74b2.tsv` | The staged American and Canadian player, seeds 25 to 36 (built into `out74b` so the main queue's bundle is left alone); Mexico's path wars from the same bundle | `./jobs.sh v74s.tsv out/v74s 2` |
| `newseats.py` | The new seats, one row a set: wars, when Germany fell (the middle war), months at war, months with a battle of the player's own, the longest quiet run, provinces at the start, at the most and at the end; `--rows` lists the wars | `python3 newseats.py out/v74 seat-USA seatfield-USA seatidle-USA --seeds 25-36` |
| `probe66.ts` | A seat's diary: one line for every month in which the scripted player did something and the run of quiet months between | `node out74b/probe66.mjs USA 1 25 76 '{"minOdds":1.5,"land":true,"stage":true}'` |
| `holdband.py` | The forecast against the outcome split by how the two fleets compared on the card: no enemy fleet, own fleets level or better, own 50 to 99% of the enemy's, under half | `python3 holdband.py 'out/v74/dom*.txt'` |
| `domtab.py`, `probe56.ts` | The United States counts as a rival great power on the wider map. Wars played before `probe56` listed it are mended in the table from the list of who was left at the end | `python3 domtab.py out/v74 dom domfirsteng domfield` |
| `ui43.cjs` | An American player's first job on the page: pick up the corps in New York in January 1942, press "Ship by sea", read what the page says and picture what it marks at the opening zoom and with the whole chart in view | `node ui43.cjs ship USA 1942 1440 900 99-v74-ship` |
