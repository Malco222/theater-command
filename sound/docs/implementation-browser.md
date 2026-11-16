# Sound in the browser game: the smaller set and how to wire it

Sound director, 16 November 2026. For the rules builder. Read with `sound-brief.md` (why) and `ui-hooks.md` (the interface's hooks).

**Brian's words** (project chat, 2026-11-16 18:42:45Z): "do all of those. we definitely need great sound effects".

## What you are given

| Thing | Where | What it is |
|---|---|---|
| 80 sounds | `sound/browser-set/*.mp3` | Mono MP3s, 1.6 MB in all |
| The list | `sound/browser-set/browser-sounds.json` | For each sound: its bus, its gain, its length, its loop points, the UI hook it answers (`hook`) and the moment it is for (`on`) |
| The player | `sound/browser/sound.ts` | One module with no dependencies. It type-checks under `--strict` and was run in a headless browser against the whole set |
| The choice | `sound/browser/set.mjs` | Which sounds are in the set and why. Change this and run the packer to change the set |

All of it is on the branch `claude/project-thread-qjxyv5`, and a copy is in the project files at `/mnt/project-files/sound/browser-set/`.

**Why a smaller set.** The browser game is one page with a flat map. It has no camera to bring down and nothing that carries a sound about, so the engine loops, the placed guns and the outdoor ambience of the 3D game have no use here. It needs the desk, the month, the band's calls, one short sound for each kind of order or report, and one quiet bed for the war room. The full set is 203 sounds and 12 MB; this is 80 and 1.6 MB.

**What it costs.** The page is 3.6 MB today. With the sound written in it is 5.8 MB (measured on a scratch build). The limit for a published page is 16 MB.

**No music in the browser game.** None exists for it and none is added. Gate 7 asks for separate volumes for effects and music: in the browser game that is one Effects volume and an on/off switch.

## Step 1. Write the sounds into the page

In `build.mjs`, before `const page = ...`, and with `existsSync` added to the import from `node:fs`:

```js
// Sound: the browser set, written into the page. `node build.mjs --no-sound` leaves it out.
const SOUND = 'sound/browser-set';
let sound = '';
if (!process.argv.includes('--no-sound') && existsSync(`${SOUND}/browser-sounds.json`)) {
  const rows = JSON.parse(readFileSync(`${SOUND}/browser-sounds.json`, 'utf8'));
  const data = Object.fromEntries(rows.map((r) => [r.name, `data:audio/mpeg;base64,${readFileSync(`${SOUND}/${r.file}`).toString('base64')}`]));
  const list = rows.map(({ name, bus, gain, seconds, loop }) => ({ name, bus, gain, seconds, ...(loop ? { loop } : {}) }));
  sound = `<script>window.__TC_SOUNDS=${JSON.stringify(data)};window.__TC_SOUND_LIST=${JSON.stringify(list)};</script>\n`;
}
```

Then put `${sound}` in the page just before the game's own `<script>`. This exact snippet was tried on a scratch copy of `build.mjs`: the page opened with no errors and both globals held every entry.

Move `sound/browser/sound.ts` to `src/campaign/ui/sound.ts` and import from it in `main.ts`. If the globals are missing (a `--no-sound` build, or the rules under test with no page), every call does nothing.

## Step 2. One sound to a click

The UI director's rule is one sound to an act. `openModal` is called from inside most clicks, so a click that opens a card would make two sounds unless one place decides. Do it with a single pending name:

```ts
import { unlock, play, startLoop, stopLoop } from './sound';

let inClick = false, pending = '';
/** Names the sound for what is happening. Inside a click the last name wins and plays once; outside one it plays now. */
function sfx(name: string): void { if (inClick) pending = name; else play(name); }

function onClick(e: MouseEvent): void {
  unlock();                                  // safe every time; the first click also starts the room
  startLoop('room_day_loop', -4, 2);
  const t = ...closest('[data-act]');
  ...
  inClick = true; pending = ACT_SOUND[act] ?? 'click';
  try { switch (act) { ... } } finally { inClick = false; }
  if (pending) play(pending);
}
```

- `ACT_SOUND` is the table in step 3.
- This is also how the UI director's rule is kept that a moment with a sound of its own takes the place of the general one: END THE MONTH starts as a click, and the question (`month_ask`) or the month ending (`month_end`) replaces it; the close button starts as a click, and the card closing (`card_close`, `screen_close`) replaces it.
- A branch that **refuses** the order (the order function returns false, or a reason why not) calls `sfx('ui_error')`, and the reason is shown in words as it is today. Never the act's own sound for an order that did not happen.
- A branch whose sound depends on what happened calls `sfx(...)` with the right name (a switch turned on or off, a move by road or by rail).
- `openModal` calls `sfx(...)` for the card it opens (step 4), so the card's sound takes the place of the click's.
- Also call `unlock()` from `onKey`.
- Nothing plays when the pointer merely passes over a thing. The player itself refuses to start the same sound more than eight times a second.

**The band and the world may answer the desk.** One click makes one desk sound. Where the order is also an event (war declared, a column marching off), one call or one world sound follows it a quarter of a second later: `play('war_declared', { delay: 0.25 })`. Never more than one.

## Step 3. The buttons

Every `data-act` name below is in `onClick` today. Any act not listed plays `click`.

| Sound | Acts |
|---|---|
| `ui_primary` | The one brass button of a screen: `dodeclare`, `doraid`, `doair`, `dosplit`, `govappoint`, an attack confirmed. Not `end`: END THE MONTH plays `month_ask` or `month_end` (step 5) |
| `ui_back` | `unpick`, `wreckno`, `govkeep`, any Cancel, and a step back to the card before (`ui.modal.back`). Not `close`, the close stud or Esc: those play `card_close` or `screen_close` |
| `ui_tab` | `tab`, `lens`, `menu`, `tradeall`, `ledgerall`, `monthall`; any choice among three or more |
| `ui_dial` | `gchist`, `gclevel`, `gcdepth`: a dial moved one stop |
| `switch_on`, `switch_off` | `fog`, `setask`, `asktick`, `watchpref`, `auto`, `gccabinet`, `gcsecure`: by the state it ends in |
| `pickup` | `pick`, `pickall`, `fleet`, `airfleet`, `wing` |
| `putdown` | `standmonth`, `joinarmy`, `joinall`, `gotmarch` |
| `order_attack` | `attack`, `storm`, `sally`, `probe` |
| `order_cancel` | `stopmarch`, `stopjob`, `cancelbuild`, `sendback` |
| `stamp_light` | Something bought or ordered in the arsenal or a city: an arsenal order, `job`. Not a move on the map |
| `stamp` | An order that cannot be undone: `doshell`, `govdecree` |
| `coins` | `cash`, `drafts` |
| `pen_sign` | `tradesign`, `govpromise`; a technology put into work |
| `radio` | `spygo`, `spymission` |
| `ship_horn` | `shipto`, `sail`, `toport`, `fleetgo` |
| `dig_in` | `fortify`, `garrison` |
| `bridge_blown` | `blow`, `wreck` |
| `rail_cut` | `cutrail` |
| `fire_start` | `burnorder`, `scorch` |
| `medal` | `appoint`, `promote` |

Three orders are not buttons:

| Moment | Sound |
|---|---|
| `selectRegion()` from a click on the map | `select_region` |
| `march()`, a move by road | `order_march`, then `play('marching', { delay: 0.3 })` |
| A move by rail (`ui.rail` is on when the move is made) | `rail_move` and nothing else. This is one half of gate 7's test: a march and a rail move must not sound alike |

After the brass button, the event's own sound follows where there is one: `dodeclare` then `war_declared`; `doraid` then nothing (the raid card brings its sting); `doair` for a patrol, an escort or a scouting flight then `fighter_pass`; `doland` then `landing_craft`; `doshell` then `shore_bombardment`; `bombline` and `dosiege` then `artillery_salvo_01`; `start` (a new campaign) then `campaign_start`.

## Step 4. The cards

`openModal` is also called to repaint the card that is already open (a dial moved, a box ticked). **Play a card's sound only when the card's kind changes**, never on a repaint: keep the kind that is showing and compare.

| Card (`m.kind`) | Sound |
|---|---|
| `askend` | `month_ask` |
| `news` with a month card | `month_card_open`, then 0.6 s later `month_card_good`, `month_card_bad` or `month_card_quiet` (step 5) |
| `attack` | `drums` |
| `report` (a battle) | `battle`, then 1.2 s later `victory` or `defeat` by who won |
| `raid`, `airreport` | One of four stings, named by the hour and then the bomber: `raid_night_` when the mission is `night`, `raid_day_` for every other raid; then `heavy` if the country that flew it has a four-engined bomber, `medium` if a twin (see below). The other half of gate 7's test |
| `seareport` | `ship_guns`; then `torpedo_hit` if a ship was torpedoed, or `ship_sinking` if a capital ship went down (one of them, 1 s later) |
| `shorereport` | `shore_bombardment` |
| `siege` | `artillery_salvo_01` |
| `landing`, `invade` | Against the player: `alert`. The player's own landing: `landing_craft` |
| `award` | `medal` |
| `fallen` | `unit_lost` |
| `worldwon` | `world_won`; the campaign lost: `campaign_lost` |
| The pages: `ledger`, `cabinet`, `trade`, `tech`, `research`, `level`, `staffs`, `generals`, `honours`, `league`, `stores`, `traits` | `screen_open`; closing one, `screen_close` |
| Every other kind | `card_open`; closing one (the close button, Esc, a click outside it), `card_close` |
| One report to the next, a page of the ledger or the guide | `page_turn` |
| `start` (the opening screen) | Nothing: the browser will not play before the first click |

**Which bomber.** The bomber's sound follows the aircraft, not the hour (design director, ruling 142): the 3D game draws each country's bomber as its own, two engines or four, and the ear must agree. The rules hold one kind of bomber a country. Today the four-engined ones are the American bomber and the British one from 1942; every other country's bomber, and Britain's before 1942, is a twin. Keep that in one small function beside the rules' bomber data, so that it changes in one place if a country's bomber does: `const raidSting = (night: boolean, nation: string, year: number) => `raid_${night ? 'night' : 'day'}_${fourEngined(nation, year) ? 'heavy' : 'medium'}`;`

## Step 5. The month

This is the sequence gate 7 means by "every month that ends has a sound". The target to compare against is `scenes/scene_month_end.mp3`.

| Moment | Call |
|---|---|
| The end-of-month question rises (`openModal({ kind: 'askend' })`) | `month_ask` |
| The month really ends (`nextMonth()`) | `month_end` |
| The others begin to move (`runOthers()`, the watched pass) | `startLoop('month_passing_loop')` |
| `arrive()`: the month is the player's again | `stopLoop('month_passing_loop', 0.3)` and `month_yours`, at the same moment: the loop stops as the call begins |
| Anywhere the pass is ended early (`endPassing()`) | `stopLoop('month_passing_loop', 0.3)` |
| The month card is laid down | `month_card_open`, then by its rows 0.6 s later: more good than bad `month_card_good`; more bad than good `month_card_bad`; even, or two rows or fewer, `month_card_quiet` |
| The END THE MONTH button becomes ready (hook `ui.month.ready`) | `play('switch_on', { gain: -6 })` |

Inside the month card, at most **one** of these, the first that applies, 1.5 s after the card's own sound: a country eliminated `nation_falls`; a country liberated `nation_freed`; war declared on or by anyone `war_declared`; a capital or prize city taken or lost by the player `city_taken`, `city_lost`; a treaty `treaty_signed`; research finished `research_done`; works finished `build_works`; rails mended `rail_mended`. The band never plays two calls at once (the player cuts the earlier one), so one is all that would be heard.

Other moments of the desk: a new alert in the strip `alert_slip` (once for a batch), but when the batch holds a `raided` alert (enemy aircraft raided the player's provinces) play `siren` in its place; something that wants the player now and is not an alert (an offer, a question from the cabinet) `ui_notify`; `followAlert` for an urgent alert `alert`; the treasury figure counting `coin_run`; a red band with no call of its own `drum_low`; the WON and LOST stamps `stamp` and `stamp_low`, played as the stamp lands (220 ms after it starts to fall: `play('stamp', { delay: 0.22 })` from the moment the fall starts).

**First Command** (ruling 129): `note_arrives` when a staff note appears, `note_done` as the DONE stamp lands (220 ms after it starts to fall), `note_closing` with the closing note.

## Step 6. The volume

In the settings card (`level`), add one row: a switch "Sound" and a slider "Effects volume".

```ts
import { soundOn, setSoundOn, effectsVolume, setEffectsVolume } from './sound';
```

The player remembers both between visits (`localStorage`, key `tc-sound`) and works without storage. The switch itself answers with `switch_on` or `switch_off`.

## How to know it works

Nobody building this can hear it. Check by log, and leave the ear to Brian.

1. Before the game starts, set `window.__TC_SOUND_LOG = []`. The player then writes every sound asked for to it (`"click"`, `"loop:month_passing_loop"`, `"stop:month_passing_loop"`), whether or not the browser plays it. The Playtester's harness can read it.
2. A month played through should log, in order: `month_ask` (if any army is idle), `month_end`, `loop:month_passing_loop`, `stop:month_passing_loop`, `month_yours`, `month_card_open`, and one of the three card sounds. No `click` and no `ui_primary` among them.
3. Every act in `onClick` should log exactly one desk sound. An order by road logs `order_march` and `marching`; by rail, `rail_move` alone. A day raid logs `raid_day_heavy` or `raid_day_medium`; a night raid, `raid_night_heavy` or `raid_night_medium`; a British raid of 1943 is `heavy` at either hour and a German one `medium`.
4. `npm run typecheck`, `npm test` and the exit tests must pass as before: the module does nothing without a page.
5. Tell the coordinator which steps are in, by number, so `gate-7-sound-list.md` can be marked WIRED line by line.

## Changing the set

Edit `sound/browser/set.mjs`, then:

```
node sound/make_sounds.mjs <sounds-folder>
node sound/make_scenes.mjs <sounds-folder> <scenes-folder>
node sound/tools/make_browser_set.mjs <sounds-folder> <scenes-folder> sound/browser-set
```

It needs Node.js and `ffmpeg`. Making all the sounds takes about two minutes.
