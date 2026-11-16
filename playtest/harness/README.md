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

## Version 75: America ships its army overseas

Version 75 (7198cd6) has a computer's America raise corps and ship them itself, lets an American player declare war on Germany and Italy from September 1939, and gives the American seat a par line (the design director's ruling 29). Build with `./build70.sh NAME /home/user/tc75 out75`. Each queue is short enough for the two-hour limit of a background command; play them one after another.

| Script | What it does | Example |
| --- | --- | --- |
| `mk75.py` | Writes the four Version 75 queues: `v75a.tsv` (the war with nobody playing, seeds 17 to 40; the American seat on seeds 25 to 36 staged by hand, at Field command, idle, and declaring on Germany in September 1940 `seatearly-` or in September 1939 `seatfirst-`; Britain by hand and at Field command on seeds 25 to 48), `v75b.tsv` (par from Germany, the Soviet Union, France and Italy), `v75c.tsv` (the domination set with every landing followed; the American hand player is staged), `v75d.tsv` (the idle sets, Canada, Mexico) | `python3 mk75.py; ./jobs.sh v75a.tsv out/v75 4` |
| `probe58.ts` | The American par line: American corps ashore in the west to stay before June 1944 and Germany beaten before May 1945 (the old line: ashore to stay only). A `GOALS` line after the `SEAT` line says how the goals card ended (short and long: won, lost or open) | `node out75/probe58.mjs USA 1 25 76 '{"minOdds":1.5,"land":true,"stage":true}'` |
| `probe67.ts` | Watches another nation month by month while the scripted player plays (where its corps stand, at home and overseas) | `node out75/probe67.mjs UK 1 28 76 '{"minOdds":1.5,"land":true}' USA` |
| `probe68.ts`, `v75e.tsv` | The computer's American army in a war with nobody playing, one `ARMY` line a war: when its corps cross the ocean, how many wait in Britain, where it first lands, whether it would beat the American seat's own par, and where its corps stood the month Germany fell | `node out75/probe68.mjs 17,18,19 80` |
| `probe69.ts` | The same reading beside a player (`armyuk-USA-`: a British player by hand; `armyukidle-`: an idle one), with the player's own corps ashore in the west beside it | `node out75/probe69.mjs UK 1 28 76 '{"minOdds":1.5,"land":true}' USA` |
| `armytab.py` | The table of those `ARMY` lines | `python3 armytab.py 'out/v75/army-USA-*.txt'` |
| `pairfall.py` | Pairs two sets of seat wars by seed: when Germany fell (mean and middle difference, earlier, later, the same) and par gained and lost | `python3 pairfall.py out/v74 seat-UK out/v75 seat-UK --seeds 25-48` |
| `probe70.ts` | The American goals card against a player who declares on Germany in September 1939 and ships one corps to a French port: the panel's checks and its state month by month (on Version 75 the short goal is won in the first month) | `node out75/probe70.mjs 25 4 Bordeaux` |
| `ui44.cjs` | An American player's first hour on the page: the line under the years, Declare war on Germany and its card, the Build tab's word on war work, "Ship by sea" and its list of ports by country, every corps in a home port shipped to Britain month after month, the navy desk's crossings with the first named as the plan, the staff's word on it, the goals panel | `node ui44.cjs 1942 9 99-v75-hour42 wait` |
| `probe71.ts` | Why the computer's Americans wait in Britain beside a British player, one `WAIT` line a month: the American corps in Britain fit to sail and on a quay, the lift left, the second front their staff sees (`frontAshore`) and its harbours, the ports in the west in the player's and the Allies' hands, and how dangerous the staff holds the way from a British quay to each harbour against its limit (`overseas.USA.risk`). It only reads | `node out75b/probe71.mjs UK 1 36 76 '{"minOdds":1.5,"land":true,"buy":["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]}' 50 USA` |
| `v75d0.tsv` | The twelve American wars at Field command on seeds 37 to 48 (the top of `v75d.tsv`), played beside the domination queue so that the scoreboard has 24 wars a row | `./jobs.sh v75d0.tsv out/v75 2` |
| `probe72.ts` | Where the computer's American air groups stand each June and the month Germany falls, in a war with nobody playing (on Version 75: two groups, always at home) | `node out75b/probe72.mjs 17,24,36 80` |
| `probe73.ts` | Why the computer's America lands in Norway before France, in a war with nobody playing. Each month a copy of the war is asked (so the war is not nudged) what three American corps on a British quay would meet on every enemy harbour in reach (`WATCH`: too dangerous for the staff, warned, guarded at poor odds, an army too strong within a march, clear) and what the staff itself holds for its two forces (`STAFF`: the enemy, the quay and who stands in it, the corps ready there, the men ashore, the beach it has its eye on); `LANDED` for every landing it makes (`landingWatch`). On Version 75 the quay moves to a harbour full of British corps | `node out75b/probe73.mjs 18 70 USA` |

## Version 76: Latin America on the map

Version 76 (b62e4cc) makes Brazil, Argentina, Chile, Peru, Colombia and Venezuela playable from September 1939 beside Mexico, and keeps twelve small republics for the computer. Build with `./build70.sh NAME /home/user/tc76 out76`. Build anything new into a second folder (`out76b`) while a queue is using the first. Each queue fits the two-hour limit of a background command.

| Script | What it does | Example |
| --- | --- | --- |
| `mk76.py` | Writes the Version 76 queues: `v76a.tsv` (the war with nobody playing on seeds 17 to 40, the builder's hemisphere set on six seeds, the American and British seats on seeds 25 to 48), `v76b.tsv` (par from Germany, the Soviet Union, France and Italy), `v76c.tsv` (world domination and the landing forecast, 124 months), `v76d.tsv` (the idle player from six seats, Canada), `v76l.tsv` (the Latin set: `year-MODE-NAT-SEED` with `probe75` and `latin-MODE-NAT-SEED` with `probe74`) | `python3 mk76.py && ./jobs.sh v76l.tsv out/v76 3` |
| `v76e.tsv` | The computer's American army on Version 76 (`v75e.tsv` with the new bundle) | `./jobs.sh v76e.tsv out/v76 2` |
| `lib.ts` | The scripted player's `path` now takes `'league'`: join a League if one will have the country, else found one with the first willing partner. (`'league'` is not a path of the rules, and an invitation's answer is `ok`, so the old code never fired.) | `{"path":"league"}` |
| `hemi76.ts`, `hemi76.py` | The rules builder's hemisphere set with nobody playing: income at peace and at war for every country of the Americas, who trades with both sides, which corps leave home. This copy of `hemi76.py` takes globs | `python3 hemi76.py 'out/v76/hemi-*.txt'` |
| `probe74.ts` | The hemisphere set from a player's seat, one `H` line each January, an `ACT` line and a `SUM` line. Modes: `idle`; `merchant` (at peace: a market hall in the capital and every harbour town, the best trade offer signed, the poorest deal swapped for a richer one); `war` (the Allies' path, war on Germany in month T, default 36); `warearly` (the same in month 2); `league`; `axis` (war on the United States and Britain in month T) | `node out76b/probe74.mjs BRA 25 76 merchant` |
| `latintab.py` | The table of those wars by seat and way of playing: income first, mean and most, the share from trade, months at war, attacked and attacking, provinces won and lost | `python3 latintab.py out/v76` |
| `probe75.ts` | A year in a small country's seat. Modes `look`, `allies` (declares on Germany as soon as the rules offer it), `axis` (declares on the first of the United States, Britain and France the rules allow from month T, default 13), `league` (founds, then invites every willing country). A `Y` line in the first month and each January (the doors open today and why not, the League's willing founders, whom it may declare war on, trade, the arsenal, corps), a `YEAR` line a calendar year (months with a battle, a purchase, a move, no order at all, the news that named the country) | `node out76b/probe75.mjs ARG 25 76 axis` |
| `yeartab.py` | One line a war from those files | `python3 yeartab.py out/v76` |
| `probe76.ts` | What a small Axis country's early declaration of war on the United States does to America: a `U` line each January (America's bloc, wars, war work, corps at home, overseas and on the player's soil) and an `END` line. T of 0 is the base line with no declaration | `node out76b/probe76.mjs BRA 25 76 13` |
| `probe77.ts` | Does the computer's Britain ever take back ground a small Axis country took from it (the Falklands)? An `F` line each January (who holds it, whether Germany stands, where Britain's corps are) and an `END` line (when it was taken, when taken back or never, the months after Germany fell with it still lost and the war on) | `node out76b/probe77.mjs ARG 25 124 13` |
| `peek76.ts`, `peek77.ts` | What a Latin player's desks offer in a given month (arsenal, trade book, towns), and where a Latin player at war can ship a corps | `node out76b/peek77.mjs BRA 25 3 allies` |
| `ui44.cjs` | Now also presses the navy desk's "more shores" button (`data-act="crossall"`) and lists every shore | `TC_PAGE=file:///home/user/tc76/dist/play.html node ui44.cjs 1942 9 99-v76-hour42` |
| `ui45.cjs` | A Latin player's first hour on the page: the picker, the start card, the map, the path card and each path's page, a path taken, Declare war and its card, the Build, Powers, Navy and Staff tabs, then some months | `node ui45.cjs BRA 4 99-v76-bra allies` |
| `ui46.cjs`, `ui47.cjs` | The start card's own words for each seat; a zoomed-out picture of a seat's first screen | `node ui47.cjs ARG 99-v76-map 3` |
| `datestab.py` | History's dates in the wars with nobody playing, for the staff report card (ruling 56): the year France falls, the Soviet Union is attacked, America goes to war, Italy is out, France is freed, the Americans are ashore in the west to stay, Germany falls | `python3 datestab.py out/v76 out/v75` |

Traps met on Version 76:

- `seattab.py` takes the prefix without the nation (`seat`, `seatfield`, `seatstage`, `seatidle`); the nation comes from the file name.
- A scripted Latin player's buy list has no mountain corps or engineers. Do not read "the arsenal sells nothing" or a falling war income from it: `peek76` shows what is on offer, and saboteurs wreck factories the script cannot repair.
- The `attacked` months of a Latin player at war are corps the script shipped overseas. Read `provinces lost` and "an enemy beside a corps" for the home country.
- `army-USA-*` and `watch49s3-*` files hold three wars each.

## Version 77 (the staffs cross where history planned it) and Version 78 (America early, not never)

Version 77 (a991695) gives each staff a list of history's shores and has America wait for nine corps across the ocean before a mainland landing. Version 78 (924c249) mends ruling 58(a) and (b): a computer-led America at war with any Axis member joins the Allies that month, and a League asks only its own part of the world. Build with `./build70.sh NAME /home/user/tc77 out77` (or `tc78`, `out78`); build anything new into a second folder (`out77b`) while a queue is using the first.

| Script | What it does | Example |
| --- | --- | --- |
| `mk77.py` | Writes the Version 77 queues: `v77w.tsv` (nobody playing on seeds 17 to 40: the armies and the watch, three wars a file), `v77a.tsv` (the hemisphere set, the American and British seats on seeds 25 to 48, the Americans beside a British player), `v77b.tsv` (par from Germany, the Soviet Union, France and Italy), `v77c.tsv` (world domination), `v77d.tsv` (the idle player, Canada by hand), `v77l.tsv` (Brazil and Argentina by hand) | `python3 mk77.py && ./jobs.sh v77w.tsv out/v77 4` |
| `probe78.ts` | Every staff landing in a war nobody plays, through the rules' own hook (`landingWatch.tell` in `src/campaign/ai.ts`): a `LAND` line a landing (who, month, place, owner, corps, first or later wave, warned, thrown back, and six months on: dead of the corps that landed, who holds the beach) and a `FIRST` line a war. Queues `v77n.tsv` and `v76n.tsv` | `node out77/probe78.mjs 17,18,19 80` |
| `landtab.py` | The table of those lines: first landing by country, ground and year, corps dead within six months, beaches lost | `python3 landtab.py 'out/v77/land-*.txt' --nats=UK,USA,CAN,BRA` |
| `mk78.py` | Writes `v78a.tsv`: the small Axis player's early declaration (probe76), Germany or Italy at war with America from September 1939 by hand and with nobody playing, a League's partners (probe75) | `python3 mk78.py && ./jobs.sh v78a.tsv out/v78 3` |
| `probe79.ts` | A great Axis power led by hand at war with the United States from month T: a `U` line each January and an `END` line (America among the Allies, at war with Germany and Italy, the December 1941 event, first corps across, ashore in the west to stay, the small Allies, Germany's fall). The plan's `"force":true` declares by the rules' own call | `node out78/probe79.mjs GER 25 76 1 '{"minOdds":1.4,"land":true,"force":true}'` |
| `probe80.ts` | The same with nobody playing: modes `ger39`, `ita39`, `bra40`, `base`; one `EARLY` line a war | `node out78/probe80.mjs 17,18,19 80 ger39` |
| `peek78.ts` | In which months the page offers a player war on a given country | `node out78/peek78.mjs GER 25 46 USA '{"minOdds":1.4,"land":true}'` |

Traps met on Versions 77 and 78:

- `probe68 SEEDS UNTIL NAT` counts Britain "on the continent" from September 1939, because of its army in France. Read Britain's landings from `probe78`.
- The rules builder's two: the memorial wall is `g.stories.fallen`, and a `Fallen.id` is the wall's own count, not the corps id; "ashore to stay" counted by who controls French ground breaks the month France is freed (count by owner, as `probe78` and `probe79` do).
- A German or Italian player is never offered war on the United States (`declarable` lists only countries in reach of a frontier or a landing), so "Germany declares in September 1939" can only be played forced (`declareWar`).
- The British and American landings in France in 1943 on Version 77 total the same (18 landings, 22 of 54 dead) by chance: they are two sets.

## Version 79 (history's dates for the western Allies' own landings)

Version 79 is on main with Version 80's display changes (af9bafa; no war differs). Build with `./build70.sh NAME /home/user/tc79 out79` and anything new into `out79b` while a queue runs.

| Script | What it does | Example |
| --- | --- | --- |
| `v79s3.tsv`, `v79s4.tsv` | The war nobody plays on seeds 17 to 40 at the dial's stop 3 and stop 4: every landing, the endings, the armies of America, Britain and Canada | `./jobs.sh v79s3.tsv out/v79 4` |
| `probe78.ts`, `probe68.ts` | Now take the dial's stop from `TC_DIAL` (default 3). `probe78`'s `LAND` line ends with who stood on Germany's home ground that month (the rules' test for the lapse of France's date) | `TC_DIAL=4 node out79/probe78.mjs 17,18,19 80` |
| `landyear.py` | `LAND` lines by flag, ground and year; `--list` lists every landing in France before May 1944 with the lapse | `python3 landyear.py 'out/v79/landb-*.txt' --list` |
| `probe82.ts`, `v79e.tsv` | The month Germany's home ground is first entered in each war, by whom, and the months it is entered before May 1944 | `node out79b/probe82.mjs 17,18,19 70` |
| `probe58.ts`, `v79g.tsv` | The seat probe takes the difficulty from `TC_LEVEL` (cadet, captain, colonel, general, marshal); Germany by hand at Captain and Cadet | `./jobs.sh v79g.tsv out/v79 4`, then `python3 seattab.py out/v79 seatcaptain seatcadet --seeds 25-36` |
| `v79a.tsv` to `v79d.tsv`, `v79l.tsv` | The seat, idle, Latin and domination queues of `mk77.py` with the Version 79 bundle | `./jobs.sh v79a.tsv out/v79 4` |

Stop 4 reads: the table scripts look for `watch49s3-*`, `land-*`, `army-*` in one folder, so copy the stop 4 files (`watch49s4-*`, `land4-*`, `army4-*`) into a folder of their own under those names (and `sed 's/stop 4/stop 3/'` in the watch copies) before `watchend.py`, `datestab.py`, `armytab.py`; `landtab.py` and `landyear.py` take a glob. `probe69.mjs` must be built into `out79` before `v79a.tsv` (the Americans beside a British player).

## Version 81 (America fights an enemy on its own side of the ocean)

Version 81 is main 4814ebf (`/home/user/tc81`, bundles in `out81`). The 24 no-player wars at stop 3 are Version 79's.

| Script | What it does | Example |
| --- | --- | --- |
| `probe83.ts` | Nobody playing; a computer-led FOE (COL, VEN, BRA, MEX, PER, PAN, CUB, ARG, or `none`) turns to the Axis and declares war on the United States in month T. One `FOE` line a war: the first American corps on its soil, when it falls, Americans across the ocean and ashore in the west to stay (frozen the month Germany falls), Germany's fall, American corps at home then and at the end | `node out81/probe83.mjs 17,18,19 80 COL 13` |
| `foetab.py` | The table of `FOE` lines by enemy: when it falls, Americans ashore in the west to stay by year, Germany's fall against the same seeds with no such enemy (`foe-none-*`) | `python3 foetab.py 'out/v81/foe-*.txt'` |
| `v81.tsv` | The measure of rulings 75, 76 and 80(b): `probe83` on Version 81 and, for Colombia, Venezuela and Brazil, on Version 79 (`out79b`); `probe76` by hand; `peek79 17`; two watch jobs; the hemisphere set; Brazil and Argentina by hand | `./jobs.sh v81.tsv out/v81 3` |

## The two counts of rulings 92 and 94 (Version 79)

| Script | What it does | Example |
| --- | --- | --- |
| `probe84.ts`, `v79x.tsv` | Nobody playing: every British, American and Canadian corps that comes to the continent by ship (a landing told by the rules' hook, a ferry into ground its side held, or a ferry that ends the month on enemy ground), dead or alive six months on, where it died against its arrival and whether cut off. One `ITALY` line a war: when Italy signed, who held its mainland that month, the first western corps on Germany's home ground and how they had come | `TC_DIAL=4 node out79b/probe84.mjs 17,18,19 80` |
| `probe85.ts`, `v79i.tsv` | An Italian player by hand and the armistice of 1943 (Version 79 on): one `ARM` line a war: when each of the six grounds the events read first went to an Ally, when Mussolini fell, when and how Italy left the war (terms or beaten), what Italy still had the month before, and the news lines beforehand. `node out79b/probe85.mjs SEED [UNTIL] [PLAN-JSON]`; `{"secure":true}` sets "Your office is secure". `TC_ME=GER` with Germany's seat plan lists every news line about Italy that player gets from January 1943 (ruling 99). |
| `probe86.ts` | An American player with a computer-led enemy next door (Version 81 on; default a hostile Panama): one `FOEP` line a war: when it declared, who held the Canal Zone, the first American corps on its soil, when it fell, Germany's fall. `node out81b/probe86.mjs SEED [UNTIL] [FOE=PAN] [T=13] [PLAN-JSON]`; `{"depth":"field"}` for Field command. |
| `probe87.ts`, `alliestab.py`, `v82a.tsv` | Where the Allied armies stand once Germany has fallen and a player's Italy still stands (Version 82 on; ruling 111): `node out82/probe87.mjs SEED [UNTIL=76] [PLAN-JSON]`, an Italian player by hand. Three `STAND` blocks a war (the month after Germany falls, twelve months later, the last month): for Britain, America, the Soviet Union and the other Allies whether each is at war with Italy, its living enemies, its field corps and where they stand; who holds each province beside Italy's home ground; where an enemy of Italy holds ground next to Italian-held ground; Vichy's standing. `alliestab.py DIR allies` makes one row a war. |
| `ui49.cjs`, `v82i.tsv` | The Version 82 page and seat set. `TC_PAGE=file:///home/user/tc82/dist/play.html node ui49.cjs NAT MONTHS PREFIX` drives the browser page with Playwright: the NEXT ARMY pill and Tab round, STAND THIS MONTH, a kept march by right-click, the gate marks, the question before END MONTH and the month card (it must click END MONTH: `theater.nextMonth()` is the watch mode and shows no card). `v82i.tsv`: Italy's seat by hand and at Field, office secure, Germany's seat for "Rome wavers", one no-player spot check. |
| `ui50.cjs`, `ui51.cjs`, `ui52.cjs` | The Version 82.1 page. `[TC_DEPTH=supreme] [TC_SEED=n] node ui50.cjs NAT MONTHS PREFIX [raid\|noraid]`: the raid card's two columns BY DAY and BY NIGHT (the air desk is the player's own only at Supreme command), an escort ticked, night and back to day, the raid flown; then the month card each month (tiles, lead story, rows, the three boxes, any line cut to "No"). A country struck as the war opens places its army first (FINISH DEPLOYMENT). `node ui51.cjs SEED [MONTHS] [PREFIX] [NAT=POL]`: what a player is shown in the month his country falls, on a fixed seed (`theater.newGame(nat, seed, 1, 'grand', 'colonel', true, false, 'theatre', 3)`). `node ui52.cjs FROM [UNTIL] [PREFIX]` (`list` prints the seats): a history campaign taken from the menu as Italy (13, 14, 33, 39, 47), no orders given, one line a month with the two Italian events and the six grounds (ruling 103(c)); `TC_CARD=1` prints each month card. |
| `ui53.cjs` | The Version 82.2 page: `node ui53.cjs [NAT=GER] [SEED=7] [PREFIX]` takes a rear army in hand and reads the two reaches (march `.mk.move`, rail `.mk.railfar`), the hint above the map, the railway lines on the panel and the pointer's "by rail" line on a blue province, then right-clicks it. `theater.newGame` must be given `'grand'` as its fourth argument, or it is not a grand campaign. |
| `ui54.cjs` | Poland's first card (Version 82.3, find 2): `TC_PAGE=… node ui54.cjs SEED` counts Poland's provinces at deployment, at the first move and on the September card, and prints the PROVINCES tile beside the rows. |
| `ui55.cjs` | The rail reach under the tray (Version 82.3): `node ui55.cjs [NAT=GER] [SEED=7] [PREFIX]` takes a rear army in hand, says for each blue place (`.mk.railfar`) whether the map or the tray is on top there, reads the lit railway (`.raillit`), zooms out twice, then clicks other own provinces and reads the pointing box after each click (it must not name the army held before). |
| `ui56.cjs` | The hollow lamp (Version 82.3): a fixture, not play. `node ui56.cjs [NAT=GER] [SEED=7] [PREFIX]` gives the player's memory of cut lines (`g.intel.cuts`, which no rule reads) entries for enemy provinces out of sight, redraws, and counts `.raillamp.old` and `.railstop.old`, then reads the pointer and the panel at one. |
| `probe88.ts`, `v823h.tsv`, `v823s.tsv`, `v823o.tsv` | One full grand campaign by hand that uses Version 82.3's features as a player would: `node out82b/probe88.mjs SEED [UNTIL=76] [PLAN-JSON] [ME=GER]`. Bombing raids picked from the raid card by day or by night (`R` and `FORECAST` lines set the card's chance beside what happened), anti-aircraft guns ordered (`GUNS`), long rail moves and trains refused (`TRAINS`), the line torn up when falling back and every cut with its cause and the months to relay it (`CUTS`), artillery (`ARTILLERY`), the enemy's raids (`FOERAIDS`, `BOMBED`), air forces and armies each January (`AIRFORCE`, `ARMS`), quiet runs (`DULL`), months three corps or more were lost (`LOST`). Plan `opt`: `raids`, `tear`, `aa` (each can be switched off), `human` (buys like a person and saves for artillery; without it the lib's buyer orders 90 to 160 fighter wings a war and drains the oil), `aaMost`. `TC_SAVE=dir TC_SAVE_AT=7,9` writes saves. The queues: one seed several ways, seeds 25 to 36 four ways (base, hand, raids, tear), the other seats. | `./jobs.sh v823s.tsv out/v823 4` |
| `ui57.cjs`, `ui58.cjs` | A saved game on the page. `node ui57.cjs SAVE.json PREFIX ACTION...` (`tabs`, `raid:Lille`, `army:Riviera`, `lamps`, `build:Ruhr`, `card`, `news:regex`) reads and pictures what a player sees there. `node ui58.cjs SAVE.json PREFIX [MONTHS=2]` ends months from the save and prints each month card's head, red bands and words: is the player warned before his country falls, or before he is attacked? | `TC_PAGE=file:///home/user/tc83/dist/play.html node ui58.cjs saves/save-t61.json 107-end 2` |
| `probe89.ts`, `eastab.py`, `v83e.tsv` | The film of the eastern front (Version 83 on): `node out83/probe89.mjs SEEDS [UNTIL=80] [ME=none\|GER\|SOV] [PLAN-JSON]`. Nobody playing, or a German or Soviet player by hand. One `F` line a month of the war (Soviet corps and those at the front, the enemy's at the front, home provinces lost, provinces beyond the opening line, attacks each way with wins and odds, corps dead), `CITY` (when each town fell and came back), `ARMY` each February, `RING` (months a side lost three corps or more, with the news), `PRE`, `EAST` (the war in one line), `YEARS`. `eastab.py 'GLOB' [--months] [--label TEXT]` sets the middle war beside history's dates. | `./jobs.sh v83e.tsv out/v83e 4 && python3 eastab.py 'out/v83e/east-*.txt' --months` |
| `probe89.ts` (second version), `v83f.tsv` | The same film with the railways and the reserve of men: each `F` line ends `rail: cut C on Soviet-held ground, relaid R, engineers E (W on a line), unsupplied U`; `RAIL` (by year: lines relaid by engineers, cut at the worst, engineer corps, corps out of supply; new cuts by cause; how long a relay took) and `MEN` (for each great power, the months at war with the reserve of men at nothing, and the first). `eastab.py` prints both as tables; `--events` lists the dates behind a chart. | `./jobs.sh v83f.tsv out/v83f 4 && python3 eastab.py 'out/v83f/sov-*.txt' --months` |
| `probe90.ts`, `lentab.py`, `v83l.tsv` | How a town falls (Leningrad unless a fourth argument names another): `node out83d/probe90.mjs SEEDS [UNTIL=80] [TOWN]`, a war nobody plays for each seed. `L` lines (each month the enemy stands in or beside the province: garrison, town guard, corps in the field, the army outside, stores, the ring, fed by sea, the month's fights there), `FELL` (the month it changes hands: STORMED, STARVED, GATES, WALKED IN or EMPTY, and what held it the month before), `BACK`, `SIEGE` (the whole story in a line). `lentab.py 'GLOB'` makes the table. | `./jobs.sh v83l.tsv out/v83l 4 && python3 lentab.py 'out/v83l/*.txt'` |
| `v83s3.tsv`, `v83s4.tsv`, `v83x.tsv`, `v83u.tsv`, `v83i.tsv` | Version 83's landing measure (the test of strength before a staff puts corps across the sea): the ferried and the landings at stops 3 and 4, the ending, the British seat with the Americans beside it, and Italy's seat. One queue to a background command; stop 4 needs a folder of its own. | `./jobs.sh v83s3.tsv out/v83 4` |
| `probe91.ts`, `dangertab.py`, `v831d.tsv`, `v831e.tsv`, `v831f.tsv` | The red band before the player's own country falls (rulings 117(d) and 133(a), Version 83.1 on): `node out831b/probe91.mjs ME 1 SEED UNTIL PLAN-JSON`, a seat played by the lib's plan. Each month, as it opens, the month card is made as the page makes it and its warning is read (a FALL band, a REACH band said once, or the LAST MONTH lead). One `DANGER` line a war: what the last card before the fall said, the months of unbroken notice, the last eight cards, when REACH was said; a seat that stands lists any fall band it was shown. `TC_DETAIL=N` adds the reading itself for the last N months (`READ` lines) and how it fell. `dangertab.py` makes the table (a later glob replaces an earlier file's seat and seed). The queues: 66 seats that can lose, 8 replays, 4 with detail, and `v831g.tsv`: 34 with the reading (what privation at home adds to the count) | `python3 dangertab.py 'out/v831d/*.txt' 'out/v831e/*.txt'` |
| `ui59.cjs`, `ui60.cjs` | The front door by hand. `node ui59.cjs PREFIX [SAVE.json]`: the start screen as the United States year by year (the line under the years), the question before a new campaign replaces a saved one, and "Keep it". `node ui60.cjs PREFIX NAT FROM [MONTHS]`: a turning point started from the menu, its first card and the cards after; `TC_DUMP=file` keeps the page's own save for `ui57.cjs`; `TC_OFFERS=1` lists every start offered. `ui57.cjs` has new actions: `quays`, `land:QUAY>BEACH[>N]` (the landing card), `lands:QUAY[>N]` (every beach with its card's head), `guard:REGION` or `guard:auto` (the railway guard signs with an army in hand), `zero` | `TC_PAGE=file:///home/user/tc831/dist/play.html node ui60.cjs x ITA 47 2` |
| `shiptab.py` | The tables of both counts from `SHIP` and `ITALY` lines; `--list N` lists the groups that lost N corps or more | `python3 shiptab.py 'out/v79x/ship3-*.txt' --list 2` |
