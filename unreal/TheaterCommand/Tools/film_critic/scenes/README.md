# Film critic scenes

What the night films, in what order, and why. `night.sh` plays `scenes.txt` from the top, one game run per scene,
and stops when the night runs out or Brian comes back, so the scenes are in order of value: the tail is lost first.

| File | What it is |
|---|---|
| `scenes.txt` | the run order: `name \| minutes (cap) \| flags`, with `{OUT}` and `{KIT}` |
| `<name>.steps.txt` | the step files (TAB between verb, target and expect). Each step's comment says, by number, what its picture `step-NNN.png` should show |
| `shots.yaml` | per scene: kind, theater (T1..T9), year and month, nations, and every expected picture with its tags and one line on what it should show |
| `check_steps.cjs` | re-checks every step file against the game's rules bundle in node (run it when the game's head moves) |

All facts below were read on the release 2 head **2fdb13e5** (this worktree). The Play folder may be a little newer.

## The night, in order

| # | Scene | Cap | About | What it films | Theater |
|---|---|---|---|---|---|
| 1 | `air-raid-rhineland` | 15 | 8 min | **The bombing film, flown live.** France, Jan 1940, DAY BOMBING: RHINELAND, FLY IT YOURSELF: the opening flyover (4 stills), the formations meeting, 24 s of the fight, the chase view and the next aircraft, the run-in, the bombs at x1, the after card, the map after | T1 |
| 2 | `map-fleets-europe` | 10 | 4 min | **Fleets on the map**, Britain Sep 1939: Home Fleet north, Western Approaches Fleet south, Mediterranean Fleet west and then east over the same water. Each is pictured mid-sail, just arrived (about 5 s after the order) and at rest a minute later. Dover Fleet, which never moves, is the control | T1 |
| 3 | `map-fleets-europe-heading` | 10 | 4 min | **The art director's second witness for her fleet-heading fix**: the same step file, with `-TheaterFleetHeading` added. Only the fleets' sail, arrival and rest pictures | T1 |
| 4 | `photo-fleets-yaw` | 6 | 3 min | The photographer (no step file). The first fleet laid (Home Fleet) at rest, close, from camera headings 0/90/180/270. The English Channel (Dover Fleet) at 0 and 135. A running train. Southern Command marching from Hampshire to Lothian | T1 |
| 5 | `opening-screen` | 4 | 2 min | The opening screen. Five country cards pressed in turn, filmed at 60 frames a second for the click flicker | ui |
| 6 | `replay-air-bremen-b17` | 10 | 6 min | The recorded B-17 raid on Bremen (the bombing film as a replay): 18 named stills, with strips after each | T1 |
| 7 | `land-desert-tobruk` | 12 | 5 min | UK 7th Armoured Division on Tobruk, Jan 1941, desert, fought live. See "Each live land battle" below | T5 |
| 8 | `land-winter-orel` | 12 | 4.5 min | SOV 9th Army on Orel, Jan 1942, farmland+snow | T3 |
| 9 | `land-med-salonika` | 12 | 5 min | ITA Albania Command on Salonika, Jan 1941, med | T6 |
| 10 | `far-pacific` | 16 | 8 min | The whole-world map (`-TheaterWorld -TheaterFar`), Japan Sep 1939. Fleets sail north (2nd Fleet to the Yellow Sea), south (Southern Expeditionary Fleet to the Natuna Sea), east and, after a restore, west (4th Fleet to Marshall Waters, then Leyte Gulf), each pictured mid-sail, arrived and at rest. Then 21st Army (Canton) on Kukong, fought live in the jungle look (southern China). Then Washington and Hawaii by day and by night | T7 (battle in T8 China) |
| 11 | `map-raid-hanover` | 9 | 4.5 min | A raid flown on the map: UK DAY BOMBING: HANOVER with three groups, one of them from the carrier. Hanover close before the raid, four stills of the 11-s flight, Hanover just after. Hanover and Kent by night (T key), Kent by day. Then end-month, close, and Hanover a month later | T1 |
| 12 | `land-poland-1939` | 16 | 7.5 min | GER Sep 1939: Berlin close (spies), XIX Corps on Pomorze (farmland), then I Corps on Warsaw (a city field), then East Prussia by day and night | T2 (+ the city field) |
| 13 | `land-west-saar` | 12 | 4 min | FRA 3rd Army on the Saar, Sep 1939, farmland with bunkers | T1 |
| 14 | `replay-land-normandy-1944` | 12 | 8.5 min | The recorded battle in the Normandy hedgerows (bocage), 11 stills | T1 |
| 15 | `replay-land-prokhorovka-1943` | 12 | 8.5 min | The recorded tank battle at Prokhorovka, 11 stills | T2 |
| 16 | `replay-sea-samar` | 10 | 5 min | The recorded action off Samar, Oct 1944 | T7 |
| 17 | `replay-air-oahu` | 10 | 6 min | The recorded strike on Pearl Harbor, Dec 1941 | T7 |
| 18 | `replay-air-midway-fleet` | 10 | 6 min | The recorded strike on the fleet at Midway, Jun 1942 | T7 |
| 19 | `replay-sea-two-lines` | 10 | 6 min | The practice battle of two battle lines, UK against GER | T1 |
| 20 | `replay-sea-light-ships` | 10 | 6 min | The practice battle of light ships against battleships, UK against ITA | T6 |

The "About" column is my estimate. It assumes about 1.5 min to start the game, and gives about **2 h** for the whole
night. The caps add up to 228 min. A cap only stops a run that hangs.

**Each live land battle** runs the same steps:

1. The army is picked up and the attack card opens.
2. FIGHT IT YOURSELF is pressed.
3. 8 s in the placing view, then Enter.
4. x4 to contact: Tab takes a unit and V rides with it for a close view, which stays on through contact.
5. The close fight is watched at x1.
6. C, then Q and W held: his high view, turned and moved.
7. C again: the film cuts on its own for 16 s.
8. Esc, then x4 to the after card, CONTINUE, and the map after the battle.

No order key is ever pressed. A step run's battle had shown only the high view before (the critic's note). The
V, C, Q and W steps are what give close views.

## Brian's finds, and the scenes that film them

| Find | Scenes, pictures |
|---|---|
| "the bombing video camera is super choppy ... it flickers and isn't smooth" (26 Nov) | `air-raid-rhineland`: every wait is filmed (12 fps, up to 24 s each): the flyover, the fight, the chase view, the bombs. Also `replay-air-bremen-b17`, `-oahu` and `-midway-fleet`: strips after every still. The facts file has every frame's camera in every scene |
| Resting fleets' wakes all point screen-left (26 Nov) | `map-fleets-europe` (rest stills 002, 006, 010, 014, 018, 019-022), `photo-fleets-yaw` (the same fleet from four headings: a wake that stays on one screen side is the bug), `far-pacific` |
| Ships jump on the map | the strips of the order-sea steps in `map-fleets-europe` and `far-pacific` (the whole sail) |
| The raid's opening shows ground and cloud, never the interceptors | `air-raid-rhineland` 004-006, and the `00-flyover-*` stills of the air replays |
| The map raid hitches | `map-raid-hanover` 004, with a 12-s strip of the whole flight |
| A bombed city should smoke black through the next turn | `map-raid-hanover` 009 (just after) and 016 (a month later) |
| The map at night read as fog ("America looks fogged") | `map-raid-hanover` 010-013, `land-poland-1939` 049-051, `far-pacific` (Washington and Hawaii) |
| "i didnt see spies" | `land-poland-1939` 002 (Berlin close, the first month) |
| The click on a country name flickers | `opening-screen` (60 fps strips after each press) |
| Markings and paint in every year | every battle scene, dates from 1939 to 1944 |

## Theaters

- T1 W Europe: the air raid, the map scenes, Saar, Normandy, Bremen, two-lines.
- T2 East summer: Pomorze, Prokhorovka.
- T3 Winter: Orel.
- T4 Ruined city: not built. There is no city look in LandScape, and the Look Bible marks it missing. The Warsaw battle
  in `land-poland-1939` is fought on a city field (4430 building cells, suburbs, industry, a river and bridges), the
  nearest the game comes today.
- T5 Desert: Tobruk.
- T6 Italy/Med: Salonika, light-ships.
- T7 Pacific/jungle: the far side's seas, Kukong's jungle look, Samar, Oahu, Midway.
- T8 China: Kukong, in southern China.
- T9 Americas: map stills only. Washington and Hawaii are on the world map, by day and night. No land battle in the
  Americas can be reached.

## How it was checked (on 2fdb13e5, without the Mac)

- **The rules, in node** (`check_steps.cjs` plays every step file on `Content/Rules/campaign-rules.js` with the
  release 2 war switches; all 272 steps pass). It checks:
  - Each `start` nation is on the list for its date. Japan is on it only on the world map, in Sep 1939. 1941-01
    has no France.
  - Every province, sea, fleet and army is found by `TC.find`. The British Mediterranean Fleet is fleet 4, not the
    French fleet of that name.
  - Every fleet order is accepted, in sequence, with `TC.forceOrder("fleet:<id>:<sea>:patrol")`, the string
    `OrderFleetTo` sends. The Mediterranean Fleet's second order in the same month passes too, because reach is
    counted from where a fleet began the month.
  - Each raid card's title and button come from `TC.page`. "FLY IT YOURSELF" is the HUD's word
    (CampaignHUD.cpp:15189) beside the card's fly offer, and `TC.airLook` gives "The air battle over Rhineland".
    FLY THE RAID BY DAY on Hanover is flown by the rules, with 3 sorties.
  - Every attack has a battle ticket from `TC.battleLook`.
  - Every key and wait is one the step player takes. No land order key is pressed.
- **Battle timings** come from the rules themselves:
  - The air raid (`TC.airStart`): contact about battle s 181, bombs about 393, over about 453.
  - Each land battle in `land-rules.js` from its ticket, as LandLive does:

    | Battle | Contact (battle s) | Over (battle s) |
    |---|---|---|
    | Tobruk | 59 | 215 |
    | Orel | 97 | 178 |
    | Salonika | 102 | 227 |
    | Pomorze | 60 | 139 |
    | Warsaw | 114 | 305 |
    | Saar | 118 | 178 |
    | Kukong | 99 | 220 |

  - Looks come from a port of LandScape::Pick's boxes. The waits are on the battle's own clock, so the pictures land
    at the right battle second however fast the Mac runs.
- **The source** (file:line on this head):
  - The step player's verbs, waits, settles and timeouts (CampaignStepPlayer.cpp: Tick 1468-1720, ActAir 918-1083,
    ActLand 1084-1340). A check-province during a live battle is a silent wait of about 66 s with one still and no
    strip.
  - The air clock (AirDesk.cpp:44-90: x1 is half a battle second a second, x10 until the formations meet and once
    the fight winds down).
  - The land keys (LandBattleScreen.cpp:2120-2240; LandBattleCommand.cpp: ride, film and map). V is refused safely
    with nothing in hand.
  - Fleet sailing by water, and the turn of the bow at 84% (CampaignMapActor.cpp:11140-11220).
  - The photographer's `-TheaterLook` parts (CampaignPhotographer.cpp:723-770) and picture names.
  - Replay still names (LandBattle.cpp:2303-2367, AirBattle.cpp:1111-1124 and 8393, SeaBattle.cpp:5085-5150 and 5470).
  - Picture runs hold the day (CampaignEnvironment.cpp:123-140), so `key t` gives night, then running, then day.
  - The month's submarine scene stays off in picture runs (MonthSeaScene.cpp:24-41).
- **Film strips**: the strip code in this worktree is another helper's work in progress (FilmCritic.cpp, not
  committed). As written there:
  - A strip starts at each air-wait, land-wait, press and order-sea step.
  - It lasts at most the `-FilmCriticStrip` seconds, waits included.
  - A new strip replaces a running one.
  - A run holds at most 1500 frames.

  Each scene's strip setting is chosen to fit that.

## What only the first night on the Mac will tell

- **Framing.** The step player cannot turn the camera, and `look-at` frames a province's middle. Fleets sail by
  water (WaterWay), not in straight lines, and a sea's middle can lie far from any province: the Norwegian Sea is
  about 200 units from Trondheim, so that still is taken "far". A mid-sail fleet can sit off the middle of the
  picture or outside it. The strips (the whole sail) are the backup.
- **Mid-sail timing.** The order-sea still comes about 1.1 s after the order. By the straight line, that is 44-56%
  of the way: well before the bow turns at 84%. Each sail lasts 2.1 to 2.8 s. Routes by water are longer, which only
  helps.
- **`-TheaterFleetHeading`** is not in the 2fdb13e5 source (grep). Until the art director's fix is in the Play build,
  Unreal ignores the switch, and the `-heading` scene plays exactly like the plain one. That makes it a control.
- **The far side.** `-TheaterWorld -TheaterFar` works in a development build only. The Play folder runs the editor
  binary with `-game`, which should be one. How long the world map takes to load is unknown; the cap is 16 min.
- **The live battles.** The game's AI and pace can differ from the node fight: contact can come early or late, and
  a battle can end sooner. Every wait is capped at 15 min.
  - Whether the Warsaw ticket still stands after Pomorze is fought live was not simulated. It is another army in
    another province.
  - At x4 the Mac may lag. The waits are on the battle's clock, so lag only makes a scene slower.
- **The opening screen.** The country cards are expected on its first page. If they are not, the presses say so in
  steps.jsonl and the strips show the screen.
- **The photographer.** `@fleet0` is the first fleet piece the map laid (expected: Home Fleet). `@train` needs a
  train running at that moment. The march still catches the army on the road only if the march lasts longer than the
  picture's delay.
- **Brian's options.** Picture runs keep their own options file (`options-pictures.json`). `-TheaterAirCampaignLive`
  turns on "Fly raids yourself" for the air scene, and `-TheaterFightEveryLook` turns on FIGHT IT YOURSELF
  everywhere.
- **Facts and strips.** Facts and strips depend on the game side (`-FilmCritic`, `-FilmCriticStrip`) being in the
  Play build. Without it, the scenes still give their stills.

## Not made, and why

- **T4 ruined city**: not built in the game (see Theaters).
- **A live battle in the Americas (T9)**: none can be reached from these starts. The Americas get map stills only.
- **The month's submarine scene**: it is off in picture runs. `-TheaterSubScene=` would force it on, but how to wait
  for it and watch it is not verified.
- **Trains vanishing at the end of the line**: the critic wanted a 10-s strip over a railhead. No railhead is known,
  and the photographer (which finds a train) makes no strips. There is only a still (`photo-fleets-yaw` look7).
- **Fog's ruled lines**: there is no scene of its own. The night and day pairs show fog beside own ground.
- **A sea fight on the map, and shelling**: left out for time.
- **Step pictures from other camera headings**: there is no verb to turn the camera; the photographer covers fleets.

## Disk: over the 250 MB target

A night as written makes about **1-1.5 GB**:

- about 430 stills at 1600x1000 PNG, 1.5-2.5 MB each: 0.65-1.1 GB;
- about 4500 strip frames at 960 px JPEG: 0.3-0.45 GB.

`night.sh` keeps each scene's folder **and** its zip, and PNG and JPEG do not compress. So the disk holds 2-3 GB per
night, and 4-6 GB with two nights kept. The Mac had 6.7 GB free with a 3 GB floor, so the second night would stop on
the floor.

To cut it, I left strips out of most land scenes and out of the land and sea replays. The facts file still gives
every frame's camera there.

`night.sh` now does the rest: each still goes from PNG to JPEG (sips, quality 88) before its scene is zipped, the
scene's folder is deleted once its zip is written (only result.json stays), and a night the morning check has
fetched (PULLED) is deleted at the next run. A night is about 0.5 GB on the Mac, and at most two are kept.

## Changing the scenes

Keep step comments and `shots.yaml` in step with the step numbers: inserting a step renumbers every picture after
it. After any change, or when the game's head moves, run `node Tools/film_critic/scenes/check_steps.cjs`.
