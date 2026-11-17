# The far-side seats rig (TRIAL)

A trial merge of the final world chart, and a harness that proves a campaign can be played from a seat
(ruling 209(e): "it starts, an order is given, a year passes and nothing breaks"). It serves both seat builders:
the globe's eight seats and the Pacific's six.

**This branch is never merged into `main`.** The real merge of the chart is the rules builder's, at step 13. What
is here is scratch for testing (ruling 209, rider point 5: "what can be made ready ahead is made ready on a
branch") and a list of findings for that merge. Every change to the game's own code is marked `// TRIAL`:
`git grep -n TRIAL -- src vitest.config.ts`.

**Rows that are for the rig only and are NOT to be lifted into the game:**

- `tools/trial/placeholder-seats.json`: stand-in rows so that every owner code on the chart is a country.
- Any rows a seats file carries for a country that is **not a seat** (Japan's provisional nation row and
  first-day corps in the Pacific builder's file). They exist so that the seats beside them have something to
  stand against; Japan's real rows are the planner's and the historian's.
- `data/campaign/trial-far.json` (`held`, `capitals`, `drop`, `wars`): the rig's own first-day table. It shows
  the rules builder what the merge must carry; it is not a data file of the game.

## Run it (copy and paste)

```
git clone -b claude/project-thread-0h6wag https://github.com/Malco222/theater-command.git   # or: git fetch origin claude/project-thread-0h6wag && git checkout FETCH_HEAD
cd theater-command && npm ci

node tools/trial/build.mjs                       # reads every *seats.json under /mnt/project-files/far-side-seats/*/
node dist/trial/seat-play.mjs list               # the seats this build knows
node dist/trial/seat-play.mjs SAF 7 12           # one seat: SEAT [SEED=7] [MONTHS=12] [LEVEL=colonel]
tools/trial/run-all.sh                           # every seat, four at a time; printouts in dist/trial/out/
tools/trial/run-all.sh dist/trial/out 7 12 "SAF IRN"   # OUT_DIR [SEED] [MONTHS] ["SEATS"]
node dist/trial/seat-play.mjs grow SAF 7 12 army # a determined player: buys all he can of one arm (army, air, navy, works) for a year
node dist/trial/seat-play.mjs watch 7 24         # nobody playing: what the computer does with each country
node dist/trial/seat-play.mjs watch 7 24 CHN,AUS # the same, for the countries named
```

Other ways to build:

```
node tools/trial/build.mjs A-seats.json B-seats.json    # these files only, laid down in this order
node tools/trial/build.mjs --placeholders-only          # no seats file at all
node tools/trial/build.mjs --check                      # read and check the seats files, build nothing
node tools/trial/build.mjs --force                      # build although the check found errors
```

**Rebuild after every change to a seats file**: the files are laid over the data inside the bundle, so a build
made before the change plays the old rows. Nothing in `data/` is ever written by the build.

Timing (cloud container, four cores): a year from one seat takes 28 seconds alone and 36 to 52 when four run at
once, about 2 seconds a month in 1939 and 5 in 1940 (the second half of a run plays two games side by side, see
"save and load" below). All fourteen seats for a year, four at a time: about 3 minutes. Iran for 40 months: 132
seconds. A 24-month watch: 41 seconds.

The printouts of the runs made on 17 November 2026 (seed 7, Colonel, the three seats files then in
`/mnt/project-files/far-side-seats/`) are in `tools/trial/results/`: one file a seat, the 24-month watch, Iran
for 40 months, the eight globe seats' verdicts on placeholder rows alone, and the two test files' output.

The globe's eight printouts there were made again at 15:50Z on the chart as the map builder reissued it under
ruling 210 (South Africa 3 factories, Iran 1) and on `globe-seats.json` as brought to the door's sheet (parts of the
world, 45-word texts, rows of words). The Pacific's six are the first run's.

**The globe's own test file** runs its second half only where the seats are in the data. On this branch:

```
TC_SEATS=/mnt/project-files/far-side-seats/globe/globe-seats.json npx vitest run tests/far-seats-globe.test.ts --fsModuleCache=false   # 13 pass
```

Pass `--fsModuleCache=false` whenever a seats file has changed: the test runner keeps the transformed data files in
`node_modules/.vitest-cache`, its key does not know the overlay, and a `TC_SEATS` run otherwise still sees the
earlier merge (found by the Pacific seat builder, 17 November).

On `main` as it stands (the two new files alone: `data/campaign/far-seats-globe.json`, `tests/far-seats-globe.test.ts`)
9 pass and 4 are skipped until the rows are lifted into `world.json`. Those two files are the only ones on this
branch meant for the rules builder's merge: `git checkout origin/claude/project-thread-0h6wag -- data/campaign/far-seats-globe.json tests/far-seats-globe.test.ts`.

The month is advanced by the Playtester's own harness (`/mnt/project-files/playtest/harness/lib.ts`: `start`,
`advance`, `check`), so a seat is opened and played as every seat has been since Version 53. Set `PT_HARNESS`
to another folder holding `lib.ts` if that one is out of reach.

## What a run does

`seat-play.mjs SEAT SEED MONTHS`:

1. Opens the grand campaign on 1 September 1939 on the whole chart (1,758 provinces, 145 seas) with SEAT as the
   player, at Colonel.
2. Prints the first day: capital, provinces held against owned, seas, neighbours by land, each corps with its
   strength, fleets and their admirals, the books (income, upkeep, net), wars, the country it follows, friends,
   abilities (`ability: none` where the record gives none), the paths open to it, the countries it could found
   the League with (and whether any lies in another part of the world), and every arsenal offer with why it can
   or cannot be ordered (the shipyard and the troopships again with its first home port picked). On the day the
   player is first up it prints the ways out of the country: how far each corps can march this month by the
   interface's own `reach`, how many ports it could be shipped to (`shipTargets`), and how far its own and its
   allies' ground runs from the capital in the end (for a dominion, its mother country's ground alone as well).
3. Gives one real order through the function the interface calls (`orderArsenal` in `turn.ts`): the cheapest
   corps it can afford, else the cheapest of anything. Where it can afford nothing it says so, says how many
   months away the cheapest offer is, and gives the order in the first month it can.
4. Plays MONTHS months with the player passing and every other country run by the computer, one line a month.
5. Half-way (never in September 1939: finding 16), saves and loads; from then on the loaded game is played
   beside the first, and the two must give the same save every month to the end.
6. After every nation's move runs the Playtester's invariants (corps and province lists consistent, no two
   nations at war sharing a province, no bad numbers or text in the books, the news or the battle tickets).
7. Prints one line, `VERDICT SEAT seed N: PASS|FAIL: ...`, and exits 0 on PASS, 1 on FAIL. PASS needs: no
   exception, no broken invariant, an order given, the save playing on the same, the months played, the country
   still standing. (So a run shorter than the months a poor country needs to afford its first order fails on
   "no order could be given": Nepal, Yemen and Liberia need four or five months.)

`grow SEAT SEED MONTHS AIM` is the year-one measure the design director asked for (her ruling of 16:10Z on the
globe's paper, question 6): the most a determined player can field by the end of year one. Every month he is up
he buys all he can of one arm (`army`, `air`, `navy`, or `works` for factories) that will be delivered by the end, through `orderArsenal`,
best bargain first (most divisions, aircraft or ships for the money); he saves for the best bargain while his income
can still reach it before its last order date, and takes the next best when it cannot. A shipyard or an airfield is
ordered only when nothing of the arm itself is on offer; the airman of a country with no aircraft industry builds
a factory first, in the home city with the most industry. He researches nothing and moves nothing. It prints the
orders, what was never on offer and why (no technology, no men, no yard), what was on offer too late to arrive, and one line:
`GROWN SEAT AIM seed N to September 1940: divisions a -> b on paper (c -> d at strength); aircraft; ships; ...`.
The printouts for the globe's eight are in `results/grow/` (a year four ways for each seat, and South Africa and
Iran run on to 18 and 24 months); `results/grow/three-factories/` is South Africa on the chart before its factories
went from 3 to 2. The report is section 7 of the globe's paper.

**The year run again under rulings 213 and 214** (section 8 of the paper): `node tools/trial/bake-213.mjs`, build, and the same
grow runs. Printouts in `results/grow-214/` (38 runs), the eight seats' year test with the levers on in
`results/year-214/`, and Iran with its oil paid at a fifth and at nothing in `results/grow-214/iran-oil/`
(`bake-213.mjs --oil IRN=0.2`: a question for the director's watch, not a ruling; bake again without it after).
`node tools/trial/build-books.mjs && node dist/trial/books.mjs SAF IRN UK USA` prints the first day's books line by line.

**The last run, on the historian's pool rows for Iran, Iraq, Saudi Arabia and Nepal** (`rows_17_15`, accepted 17:21Z;
section 9 of the paper): `results/grow-216/` (aim `none` is a player who passes), the four seats' year test in
`results/year-216/`, and South Africa with the factory bound to September 1941 and 1942 in `results/grow-215/`.
The refusal for want of men rounds the monthly class to thousands, so a class of 0.017 reads "0 come of age each month".

**On the map builder's finer chart** (acc796a: 1,686 provinces beyond Europe, China cut to 133; South Africa 28, Iran 28,
Afghanistan 12, Saudi Arabia 12). Built with `globe-seats.json` and the Pacific seat builder's files for this chart
(`pacific/next/`, copied over the live ones at the move). `src/campaign/save.ts` carries the Pacific seat builder's
TRIAL mend `garrison-order-save.patch` (a garrison's order is saved; without it every seat fails "the save plays on
the same" from the spring of 1940 on this chart). All eight globe seats pass the year; printouts and the soldier's
runs in `results/finer-chart/`.

`watch` plays with nobody at the controls and prints, for each country, the months in which something changed
(wars, alignment, provinces, corps, news that names it): the computer's path when nobody leads it.

## A seats file

Keyed by the data file each block belongs in, with or without `.json`, each block in that file's own row forms:

```
{ "_about": ...,
  "grand":      { "nations": [ { "id": "SAF", "small": true, "years": { "1939": "text over 40 letters" } } ] },
  "world":      { "nations": { "SAF": { name, adj, color, quality, air, resilience, align, ... } },
                  "order": ["SAF"], "follow": { "SAF": "UK" }, "idle": { "SAF": 3 }, "pairs": [["a", "b"]],
                  "parts": { "names": { "africa": "Africa" }, "of": { "SAF": "africa" } },
                  "corps": [ ["SAF", "name", "inf", 1, "Pretoria", { "xp": 0.05, "str": 0.4, "hold": 1 }] ],
                  "fleets": [ { nat, name, base, port, home, zone, duty, ships } ] },
  "traits":     { "nations": { "SAF": { "traits": [...], "unit": {...} } } },
  "eve":        { "SAF": { title, advice, place } },
  "relations":  { "groups": [...], "pairs": [...] },
  "diplomacy":  { "home": {}, "bandwagon": {}, "resist": {}, "events": [] },
  "government": { "nations": { "SAF": "democracy" } },
  "commanders": { "names": { "SAF": { "first": [...], "last": [...] } } },
  "open":       { "temper": { "SAF": {...} } },
  "setup":      { "capitals": { "IRN": "Tehran" }, "drop": [["IRN", "Iranian Army"]],
                  "wars": [["CHN", "JAP"]], "held": { "Province": "JAP" } } }
```

- Any top-level key that names a file in `data/campaign/` is accepted (`"navy"`, `"nations"` and so on); one that
  names no file is ignored with a warning.
- Objects are merged by key and lists appended. A list row with an `id` replaces the row of the same id (a door
  entry, an event, a group). A value under a nation's code replaces what was there whole.
- `setup` is the rig's own: `capitals` moves a capital flag once the wider chart is attached; `drop` takes away
  a first-day corps row of `setup-1939.json` or `world.json` (by nation and name); `wars` are wars standing on
  the first day, set once the armies are placed; `held` overrides the baked first-day holders.
- A file may carry rows for a country that is not a seat: a nation with no door entry is simply run by the
  computer.
- The placeholder is laid down first. A seats file that brings a code (a nation row, a door entry or a corps
  row for it) takes all the placeholder's rows for that code away.

The build stops, naming the row, on: a corps row whose province is not on the chart or is not held by that
country on the first day (the game would leave the corps out in silence); a fleet whose base or sea is not on
the chart; a door text of 40 letters or fewer, or a year other than 1939; a seat with no first-day corps; a
seat with no capital in its own hands; a nation row that is not in `order`; an owner code on the chart with no
country; a form of government, a part of the world or a friend that does not exist. It warns on: a seat with no
part of the world, a fleet based off its home sea, a `drop` that matches nothing, two first-day corps of one
name. A seat with no abilities row, or a row of one ability, is taken without complaint (the record decides);
the printout says `ability: none`.

## What the trial changed

| Where | What |
|---|---|
| `src/campaign/world.ts` | Reads `world-cities.json`. At attach: the chart's `joins` are added to `nb` both ways (on copies of the lists); each new province's holder is set from `trial-far.json` `held`; a capital named in `capitals` takes the flag and its country takes it up; first-day rows named in `drop` are removed or skipped; a first-day corps may stand on ground held by the country it follows or that follows it (`standsOn`); `wars` are set after the armies are placed. `WORLD_STAGES` is `[104, 201, 513, 880]`, read from the chart file's `stages` |
| `src/campaign/navy.ts` | Reads `world-cities.json`; the sea lookup `wide` is an `Int16Array` (145 seas do not fit a signed byte) |
| `src/campaign/towns.ts`, `rivers.ts`, `rails.ts` | Read `world-cities.json`, `world-cities-rivers.json`, `world-cities-rails.json` |
| `data/campaign/trial-far.json` | New: the first-day table. `held` is baked from the clerk's `held_1939` column by `node tools/trial/bake-held.mjs` (32 provinces, all China's held by Japan) |
| `src/campaign/ai.ts` | `wantsOut` (the Pacific seat builder's finding and patch, 16:12Z): on the wider world a corps on ground its nation holds and whose owner it is at war with is no guest of an ally; it leaves only if cut off or broken. Without it Japan's whole army in China sails home in September 1939, China walks into the empty ground and Manchukuo falls in December |
| `vitest.config.ts` | A loader that lays the placeholder rows (and `TC_SEATS=file,file`) over the data for the tests |
| `data/campaign/trial-213.json` | New: the rows of rulings 213 and 214, copied unchanged from the clerk's and the historian's tables by `node tools/trial/bake-213.mjs` (`--off` writes it empty: the game as it stood before the rulings). Build again after it |
| `src/campaign/economy.ts` | `provinceYield`: the land and harbour pay of a province is multiplied by `landFactor` of the country that owns the ground (ruling 214; sixteen countries, everyone else 1) |
| `src/campaign/manpower.ts` | The historian's `pool` and `class` rows for Australia, New Zealand, South Africa and Manchukuo are laid over `manpower.json` (ruling 213(b)) |
| `src/campaign/trade.ts` | `wealthOf` reads a province's pay before the land factor (`provinceYield(..., raw)`), so the factor does not reach trade and no great power's first day moves (the director, 17:14Z) |
| `src/campaign/arsenal.ts` | For a smaller power the historian's list of aircraft works stands in place of the two- and three-factory test (ruling 213(d)): on the list and inside its months it builds that kind at home, otherwise never. The great powers keep the factory test. The dated deliveries of that file are not played. Ruling 213(e), in force 17:14Z: a smaller power is refused a factory past half again its first-day count, rounded up (`factoryBound`; the first-day count is read off the home ground it still holds, less what it has built) |
| `tools/trial/` | `seats.mjs` (the overlay and its checks), `build.mjs`, `seat-play.ts`, `run-all.sh`, `bake-held.mjs`, `placeholder-seats.json`, `results/`; the Pacific seat builder's `front.ts` and `build-front.mjs` (one war read month by month with nobody playing: `node tools/trial/build-front.mjs && node dist/trial/front.mjs 7 24 CHN JAP`) |

Nothing else in `src/` was changed, and no crash had to be fixed: the game attached the 1,758-province chart and
played on it at the first try once every owner code was a country.

## Findings for the rules builder (what the real merge will need)

Each was met while building the trial. Lines are those of this branch.

1. **The chart file to read.** Five imports name `world-map.json` and its rivers and railways:
   `src/campaign/world.ts:19`, `src/campaign/navy.ts:8`, `src/campaign/towns.ts:14`, `src/campaign/rivers.ts:15`,
   `src/campaign/rails.ts:19`. The river and railway files of the cities chart do NOT keep today's lists as a
   prefix (crossings and links change where a city was cut out of a bank or a line); the provinces' ids do.
2. **`joins`.** The chart file lists 57 land borders between an older province and a new one (`[old id, new
   id]`, Europe's among them: Gorky and Kazan, "Iran" and Tehran). Nothing reads them today. A province's `nb`
   is the map file's own array (`regionOf`, `src/campaign/game.ts:81`), shared by every game in the process, so
   it must be copied before it is added to (`src/campaign/world.ts:114-119`).
3. **Signed bytes.** `wide` in `src/campaign/navy.ts:121` and `:128` was an `Int8Array`; sea 128 and up read as
   negative. (The Europe-only `lookup`, `navy.ts:119`, can stay.) `near` in `src/campaign/staff.ts:414` is an
   `Int8Array` indexed by province but holds only a count of steps from 0 to 2 (`staff.ts:417`): sound.
4. **Who holds a province on the first day.** The chart carries only `owner`, and `regionOf` sets `ctrl` from it
   (`game.ts:76`). Japan holds 32 of China's provinces on 1 September 1939 (the clerk's `held_1939`). Without it
   every Japanese corps in China is left out in silence by `attachWorld` (the test at `world.ts:190`; the trial
   sets the holders at `world.ts:120-125`). The later start years need `held_1940` to `held_1942` the same way; the rig bakes 1939 only.
5. **A first-day corps on a tied country's ground.** Japan's army in Manchukuo stands on ground Manchukuo holds:
   `attachWorld` drops it. The trial lets a row stand on ground held by the country it follows or that follows
   it (`standsOn`, `world.ts:208`, used at `:190`). `settleNewGround` (`world.ts:216`, the test at `:227`) has the
   same test and was left as it is.
6. **Capitals that move.** Iran's flag stands on Europe's province "Iran" (290) and Iraq's on "Iraq" (289); the
   chart has no flag on Tehran. The flag must move only when the wider world is attached, or a campaign on the
   map of Europe loses its capital (`world.ts:126-133`, `:152-156`).
7. **A country of the old chart cannot be changed from `world.json`.** `attachWorld` skips a nation row whose
   code exists already (`world.ts:145`: `g.nations[id]`), so a `world.json` row for IRN or IRQ is not read; the
   build warns. Their figures stay those of `nations.json` unless that file is changed.
8. **`parts.new`.** A new country not named under `parts.of` counts as `americas` (`src/campaign/league.ts:35`).
   Every far-side country needs its own `of` entry, and so do IRN and IRQ if they are to leave `europe`.
9. **The wrap is not handled, and nothing crashed without it.** Eight pairs of seas are neighbours across the
   seam (California Coast and Hawaiian Approaches lie 8,617 units apart on a sheet 9,256 wide). Sea moves go by the
   lists of neighbours and are sound. Straight-line distances are not modular: `km` in
   `src/campaign/air.ts:169` (a raid's reach), `air.ts:688`, `navy.ts:1290`, `ai.ts:1292`, `stories.ts:680`,
   `trade.ts:422`, `gates.ts:125-147`. `round.ts:156` is modular already. No land link crosses the seam.
10. **Saves.** `WORLD_STAGES` must name every earlier stage (`world.ts:85`; `save.ts:37`). A Version 74 or 75
    save still loads (the Latin test reaches its `order` line). A save of today's chart (201 provinces beyond
    Europe) fits by count, but on the cities chart 182 of those 201 provinces changed shape, neighbours, coast or
    worth: NOT TRIED.
11. **A fallen country's colonies.** With the globe on, `capitulate` puts every colony of France beyond the old
    chart into British keeping in one headline that names them all (`src/campaign/game.ts:2304`): France owns
    90 provinces out there. Ruling 199 (Indochina to Vichy) and the chart's `vichy` flag (41 provinces) are not
    read by that passage.
12. **A war nobody fights ends.** In the watch run China and Japan sign an armistice in August 1940: "Neither
    could come at the other, and their armies had not met for a year" (`stalemates`, called from
    `src/campaign/turn.ts:96`, written at `src/campaign/staff.ts:1959`). In the same run China's count of provinces goes from 66 to 97 in two months:
    occupied ground with no Japanese corps on it changes hands. Both are the Pacific builder's to look at.
13. **Tests pinned to today's chart.** `tests/world-74.test.ts` and `tests/latin-76.test.ts` on this branch: 30
    pass, 7 fail, none by a crash. Pinned to the old chart: `WORLD_STAGES` equals `[104]` (`latin-76:146`);
    `WORLD.order` (`latin-76:207`, `:361`); the list of river names (`world-74:246`); "No land joins the new world to the old" (`world-74:92`, met first
    at Kazan and Gorky, one of the 57 joins). Moved by the cities: the Amazon
    crossing Para to Amapa is now Belem to Macapa (`latin-76:188`); the colonies headline (`latin-76:293`,
    finding 11).
14. **The page is not built by the rig.** `npm run build` bundles the data without the overlay; the browser
    game on this branch has provinces owned by countries that do not exist. The rig is headless only.
15. **`npx tsc --noEmit` passes** on this branch.
16. **A save made in the first month does not play on the same: on today's `main`, without the trial.** With
    the Playtester's `lib.ts` alone on `main` at 0bd738e (576 provinces): Switzerland, seed 7, saved in September
    1939, loaded, and both games played one month. They differ in `generals`, `stories`, `headlines` and
    `reports` (the first game: "Stanislaw Maczek is taken prisoner with Army Prusy reserve at Warsaw."; the
    loaded one: "Juliusz Rommel is taken prisoner with Army Lodz at Warsaw."). Seed 11 differs in `stories` and
    `reports`. Saved in October 1939 (seed 7) the two are the same a month on, and in the rig every seat's save
    of February 1940 plays on the same to September 1940. The cause was not looked for. The rig never saves in
    September 1939 (`tools/trial/seat-play.ts`, "Half-way").
17. **A seat with no abilities, or with one.** The record gives IRN, IRQ, SAU and LBR no ability and SAF, AFG,
    YEM and NEP one each, with no formation. `tests/traits.test.ts:34-35` asks of every door country a row, and
    two abilities or one and a formation: it fails on all eight (run: see `results/`). The rules take a missing
    row everywhere they read one (`src/campaign/traits.ts:87`, `:90`, `:131`, `:180`), and the interface asks
    `hasTraits` first (`src/campaign/ui/main.ts:6152`; the door `:6405`, the two buttons `:1775` and `:3469`, the
    page `:6172`: "This country has no abilities of its own."). The interface was read, not run.
18. **Tests that pin the door's list.** `tests/league.test.ts:54-55` counts 24 small door countries and names the
    last eight; `:62` has every small country but Canada begin in no alliance (South Africa and Nepal are
    dominions, in the war beside Britain from the first day).
19. **Iraq's capital is on the old chart.** Baghdad is province 366; Tehran is 960. The world's rules for a
    computer-run country ask `beyondEurope(n.capital)` (`armyIdle`, `src/campaign/world.ts:364`; `overseasOf`,
    `:391`; `shipsItsOwn`, `:398-400`), so `world.json` `idle.IRQ` is never read, while Iran's is.
20. **The League's parts of the world.** A country of the old chart is `europe` unless `parts.of` names it
    (`src/campaign/league.ts:35`), and only a neighbour is on the founders' list (`league.ts:221`). So Turkey is
    not on Iran's, Iraq's or Afghanistan's list, though the four are friends by the pact of Saadabad; and
    Africa holds two countries, Liberia and South Africa (an ally, who founds nothing), so a led Liberia has
    nobody to found the League with, ever: "No other country is ready to found it with you yet."
21. **`seaSupply` without `sealift` (South Africa).** `seaSupply` feeds an army abroad through a friendly port on
    an open sea (`src/campaign/game.ts:425-428`), lifts a beaten corps off a foreign port instead of losing it
    (`game.ts:1573`, `:2341`) and brings the army home when its host falls (`game.ts:2315`). It carries nobody
    out: `whyNotShip` answers "South Africa has no shipping to move an army by sea." (`src/campaign/navy.ts:393`)
    and `shipTargets` is empty (`src/campaign/turn.ts:1273`). The lift is bought: a shipyard in a home port (2000,
    8 months), then troopships (1400, 6 months, one corps a month each). By land the army leaves at once: both
    corps reach British Bechuanaland in the first month, and British ground alone runs from Pretoria to the
    Italian frontier at Kismayu (11 provinces off), to Egypt (Faiyum, 19 off) and to Palestine (23 off): see
    `results/SAF-seed7.txt`, "ways out". A computer-run South Africa has no row under
    `world.json` `overseas`, so nothing of its is ever shipped (`overseasOf`, `world.ts:388-394`).

22. **No row says which countries have an aircraft industry.** A fighter wing asks only for a home province with
    two factories and a bomber wing for one with three (`arsenal.json` `air`, `industry()` in `arsenal.ts`), and
    any country may build factories (3,000 and 500 more for each built there, eight months, three new in a city).
    South Africa at two factories (Johannesburg 1, Pretoria 1, as ruled) orders a factory in December 1939, has an
    aircraft industry in August 1940, 500 fighters in March 1941 and 1,200 in September 1941
    (`results/grow/SAF-air-24m-seed7.txt`). Iran has two factories at Tehran in December 1940. Filed with the design
    director in the globe's year-one report; a row or a rule here is every smaller power's.
23. **Thin first-day corps fill up.** A corps below strength takes replacements each month by the one rule for every
    country (`game.ts:2822`), paid from the treasury and drawn from the pool. With nothing bought, each of the eight
    is about half again as strong after a year (Iran 4.6 divisions to 6.8, and 9.0 after two), where the record says
    six of them did not grow. The data's lever is a `pool` and `class` row in `manpower.json`, which `manpower.ts`
    reads for any country (lines 56 and 67), not only the great powers. Filed with the same report.

24. **The land factor moves trade, and two great powers' first day with it.** An agreement's worth follows what each
    partner's provinces pay (`wealthOf`, `trade.ts:36`), so the factor of ruling 214 makes the sixteen countries
    poorer partners. On the first day Britain goes from 7,301 to 7,287 and the United States from 1,465 to 1,412,
    through trade alone (Germany, Italy, France and the Soviet Union do not move); Nepal gains an agreement (206 to
    225). Ruling 213(a) says no great power's total moves: `wealthOf` read before the factor would hold it. Ruled so at
    17:14Z, and the rig does it: Britain 7,301 and the United States 1,465 again.

## Not verified

- The full test suite was not run (over ten minutes; only the two files above).
- One seed (7) and one level (Colonel). No seat was played by a scripted player who fights: the player passes.
- The browser page, the 3D game, the month card and every other screen.
- Loading a save made on today's 576-province chart.
- Later start years (a small country is offered from 1939 only).
- Whether any figure in a seats file is the record's: the rig plays whatever rows it is given.
