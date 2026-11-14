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
| `v64.tsv`, `v64e.tsv` to `v64j.tsv` | The queues of the Version 64 test for `jobs.sh`: the Version 63 queue again (`v64`, 325 jobs); Berlin on sixty seeds for the plain and the digging German player, the new orders one at a time on twelve seeds, the Soviet seat (`v64e`, 240); the wars of 1941 with and without a probe and the bombers (`v64f`, 36); what the card reads after softening (`v64g`, `v64h`); the wider relief rule on both versions (`v64i`, not played: see the traps); the relief test (`v64j`, 16) | `./jobs.sh v64e.tsv out/v64e 3` |
| `lib.ts` | New plan options. `"fold":true` folds two worn corps of a kind standing together every month (`"fold":"raise"` also raises every cadre again first). `"probe":true` probes a dug-in line whose attack reads 0.7 of the bar or better (a number sets the share) and is not a sure thing; `"bombLine":true` sends the bombers at the same line first. `"rotate":0.7` makes a dug-in line corps under that strength change places with a stronger corps of its kind next door | `'{"minOdds":1.4,...,"probe":true,"bombLine":true}'` |
| `probe28.ts` | Its `TOTAL` line now ends with the towns taken and when, the attacks by year (fought, won, on a prepared line, men lost and inflicted), and Version 64's counters: folds, cadres raised, reliefs and the months of works handed over, probes, line raids, lines softened, how many attacks that lifted over the bar, and how often the card's chance read better or worse afterwards | `node out64n/probe28.mjs GER 1 7 76 '<plan>'` |
| `probe41.ts` | The Soviet seat's `TOTAL` line also counts probes, line raids, softened lines and folds | `node out64n/probe41.mjs SOV 1 7 76 '<plan>'` |
| `probe42.ts` | Writes saved games for the page as the player's month opens, the first month each new card is on offer: `-soften` (a dug-in line a corps can probe and the bombers can reach), `-fold` (two worn corps together), `-left` (works standing empty), each with a `.txt` saying what to click | `node out64n/probe42.mjs GER 1 7 30 '<plan>' out/v64saves/GER-7` |
| `probe43.ts` | The relief test: on the turns given, every dug-in line corps changes places with a corps of its kind next door on a copy of the game, and the months of works on the two provinces are added up before and after | `node out64q/probe43.mjs GER 1 7 48 '<plan>' 31,35,41,47` |
| `habits.py` | The new orders by habit from `probe28` outputs: Berlin, towns taken, attacks by year, and the Version 64 counters; a habit may be read from another folder as `DIR/HABIT` | `python3 habits.py out/v64e asis probebomb --seeds 1-24` |
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
