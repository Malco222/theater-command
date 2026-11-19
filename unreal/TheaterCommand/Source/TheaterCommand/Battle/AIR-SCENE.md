# The air battle's 3D scene (ruling 238: Pearl Harbor and Midway first)

The air battles thread's 3D handoff (`/mnt/project-files/air-battles/3d-handoff.md`, sections 1 to 10) built for the
3D game. The battle is the rules' (`src/battle/air` on the browser branches). This scene draws it, and so does the
battle screen over it.

## The files

| File | What it is |
|---|---|
| `AirSky.h/.cpp` | The day's sky, plain C++: the clouds laid round the target from `sky.place.cloud` (scattered cumulus, or a deck), and the painter that turns their puffs to the eye. |
| `AirShow.h/.cpp` | The flight core, plain C++. Every aircraft of every formation flies as an aircraft flies: banked turns, no turn tighter than 210 m, at most 6 g, no sliding, speed lost in a climb. It also handles the passes and breaks, the guns, the hits, the falls, the parachutes, the flak round the boxes, the bombs and torpedoes, and the ship news when the bombs arrive. |
| `AirFeed.h/.cpp` | Reads the bridge's JSON (sky, views, after card, recordings) into the flight core's terms. Plain C++. |
| `AirDesk.h/.cpp` | The screen's words and clock: the top bar, the aim slip, the near line, the dispatches, the tags, the after card, the orders as `TC.battleOrder` takes them, and the close camera's subject. Plain C++. |
| `AirBattleState.h` | The plain state above, kept out of the actor's header. |
| `AirBattle.h/.cpp` | `AAirBattle`, the scene. It feeds the core and draws what the core says: the planes, the small things, the effects kit, the stage and its ships, the cameras and the sound. |
| `AirBattleScreen.h/.cpp` | `SAirBattleScreen`, the screen. It draws the desk's words and takes the mouse and keys. |
| `AirStage.h/.cpp` | The Oahu and Midway stages (an earlier commit on this branch). |
| `../../../Tools/air_show/` | The cloud test. It plays each recording through the core at 60 fps and checks the flight: g, turn radius, slip, roll, a fall for every loss, two gunners never at one window, the frame time. It also plays the screen's clock and words. `run.sh` builds and runs it; `air_show_frames.py` draws pictures of the flight. |
| `Content/Data/AirBattles/*.json` | The two recorded battles, `oahu` and `midway-fleet`, written by `tools/air-record` on the rules branch `claude/project-thread-2bdb2m`. |
| `Content/Data/Sound/mix.json` | The sound director's mix, read by the scene (it never copies the mix's numbers). |
| `Sounds/Real/*.wav` (17 new) | The air recordings of the approved battle pack: engines by model, guns by calibre, flak, and the falls. Credits are in `credits.json` and `CREDITS.md`. |

## Running it

1. `Tools/build_sounds.sh` brings the 17 new recordings into `/Game/Campaign/Sounds/Real`. Without it the battle plays silent and nothing fails.
2. `-TheaterAirBattle=oahu` or `-TheaterAirBattle=midway-fleet` on the command line plays the recorded battle about two seconds after the table is up. The table is hidden while it plays.
3. Adding `-TheaterAirBattleShots=<folder>` takes the art director's eight pictures and then quits:
   - the approach
   - the meeting
   - riding a fighter
   - the thick of it
   - riding again
   - the ships
   - the fight going on
   - the after card

Mouse and keys:
- Left click takes his group in hand. Shift adds another group.
- Right click on an enemy tag attacks it. Right click on the sky patrols there.
- Drag turns the camera. The wheel draws it in and out.
- H holds, R breaks off, and C switches the close view.
- Space holds the clock or lets it run. Keys 1 to 4 set the speed.
- Esc answers KEEP FIGHTING. Enter presses CONTINUE.

A recorded battle shows the orders strip greyed out: it plays as it was fought.

**The clock (a reversible call, flagged to the design director).** In the 3D scene, ×1 is real time, so the aircraft fly at their true speeds (Brian: "not moving normal"). A battle second is `SKY.scale` (2) real seconds. The run-up and the run home go at ×10, and the clock is held while a question stands.

At ×1 the Pearl Harbor raid lasts about 7 real minutes, with contact about 20 s in. Midway lasts about 5 minutes, with contact about 30 s in.

**Falling aircraft.** Every loss hands the effects kit its fall (`AirFall`, kit head 44af184). An aircraft blown apart
throws its debris row at once round the kit's torn fireball. One going down burning or spinning sheds pieces as it
falls (`AirFallMove` every frame, `AirFallStop` when the model is gone). The pieces fade into the ground under the
fight, or the cloud deck's top for a fall begun above a deck. A blow-up keeps only its two biggest pieces trailing fire and
smoke, because a trail holds about 240 of the kit's 3,000 puffs.

The debris has a room of 700 puffs (`AirShow::DebrisRoom`). A burning or spinning fall leaves 100 of it for the next
blow-up. Past that, a blow-up gets the older torn flame and a burning fall keeps only its trail. In the cloud test,
Midway gives all 32 falls their debris. Pearl Harbor gives 37 of 52 (10 of its 13 blow-ups), at most 685 puffs at
once. On an older kit without `AirFall`, every blow-up gets the older flame and the scene still builds.

**Smoke trails.** The scene lights up to 24 trails at once on the newer effects kit (the one whose `TrailStart`
takes a span), and 10 on an older one. A burning aircraft's own trail comes first: when the room is full, it takes the
place of the oldest piece's trail. In the cloud test at 24, no burning aircraft loses its trail in either battle
(`AirShow::TrailRoom`).

**The table while a battle is up.** The scene keeps the table's lamps and haze as they were, gives the battle the
day's light, and puts all of it back on CONTINUE. The campaign's controller still ticks under the battle. When the
pointer's province changes, the map unfolds its crowds and would stand its pieces again, at the world's origin where
the battle is. So every frame the scene puts away any piece that came back. On CONTINUE the map stands its pieces
again itself, with the crowds folded.

**The sky.** Each battle carries its day (`sky.place.cloud`, the air thread's `sky.ts`): a cloud deck or scattered
cumulus, with one cloud base and one top. Pearl Harbor and Midway are cumulus days, as history had them: Oahu's
base 940 m and tops 2,060 m, the Mobile Force's 510 m and 1,820 m. A ticket from a rules bundle without the sky draws
cumulus at the cloud table's middle heights. `AirSky` lays the clouds and the scene draws them as one see-through mesh
of soft puffs, in the effects kit's shape and with the same `M_Highlight`. They are turned to the eye only when it has
moved about a quarter of a degree at the nearest puff, and the whole mesh drifts with the battle's wind.

- **Cumulus** cover about three eighths of the sky, fewer over the target so the ships stay in sight. Each is a flat
  grey base, a lit, lumpy dome wider than it is tall, a few turrets and a faint veil that holds it together. Most are
  shallow, and a few reach the day's tops.
- **A deck** covers most of the sky, with a few holes. It sits at least 1.5 km under the raid's bombers (the art
  director's 1.5 to 2.5 km). Where the drawn top comes nearer, the deck is lowered and keeps its thickness. Over the
  lowest band (1,800 m) the deck lies above the bombers instead. A strike at ships keeps a hole over the ships. The
  deck reaches out about 32 km in coarser tiles, so its edge is lost in the haze.
- Cloud never sits lower than 300 m over the ground under it, so a cloud over a mountain is lifted with it.
- **Falls and the deck.** The deck's top as drawn is where falls end (`DeckZ` for `AirFall`, `CloudTop` in the core).
  A fall begun above the deck ends inside it. A fall begun under it (a torpedo bomber, a low fight) goes on down to the
  sea or the ground, with its pieces and parachutes.
- The cloud test checks the sky too. It checks the day as read, the heights, the cover, the room (at most 3,200 puffs;
  the two battles lay about 2,200), the same sky every time, the far-to-near order and the painter's time. It lays a
  deck under a 4,500 m band, a deck over the lowest band, a strike's hole, a 2,000 m mountain and a ticket with no sky,
  and flies the battle again under a test deck. `AIR_SKY_PICTURES=1 Tools/air_show/run.sh` also draws each battle's
  clouds from three eyes, plus a deck day, with a small software painter (`air_sky_picture.cpp`). Unreal's light and
  haze are only stood in for there.

## Hook lines for the 3D builder

The scene builds and plays without any of these lines. It finds each one by itself (a `requires` check).

**CampaignHUD**, so that the campaign's frame is hidden while a battle is up. Until this line is added, the frame
stays under the battle's screen.

```cpp
// CampaignHUD.h, public:
void ShowFrame(bool bShow) { if (Overlay.IsValid()) { Overlay->SetVisibility(bShow ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed); } }
```

(Use the visibility the overlay has now in place of `SelfHitTestInvisible` if it is a different one.)

**CampaignMapActor and CampaignPlayerController**, so that the table stays quiet under a battle. With these lines, its
pieces stay put away whatever the pointer does, and its hover, its tips and its right button stand idle. Until they
are added, the table's hover keeps running under the battle (each province change rebuilds its highlight and gun
lines), and the scene keeps the pieces hidden itself.

```cpp
// CampaignMapActor.h, public:
/** (air battle) While a battle's scene is up, every piece stays off the table whatever the hover asks; cleared, the pieces stand again and the crowds fold up. */
void SetPutAway(bool bAway) { if (bAwayForBattle == bAway) { return; } bAwayForBattle = bAway; if (!bAway) { const bool bWere = bPiecesShown; bPiecesShown = false; SetPiecesShown(bWere); } }
bool IsPutAway() const { return bAwayForBattle; }
// CampaignMapActor.h, private, beside bPiecesShown (a name of its own: ApplyCrowds has a local bPutAway):
bool bAwayForBattle = false;

// CampaignMapActor.cpp, the first line of ApplyCrowds():
if (bAwayForBattle) { return; }
// CampaignMapActor.cpp, the first line of SetPiecesShown(bool bShown):
if (bAwayForBattle && bShown) { return; }

// CampaignPlayerController.cpp, IsTableBusy(): add to what it returns
|| (Map.IsValid() && Map->IsPutAway())
```

The lines already made on this branch:
- `CampaignGameMode.cpp`: the `-TheaterAirBattle` hook, beside the stage's.
- `CampaignSession.h/.cpp`: the public `CallBattle(Function, ArgumentsJson, OutJson, OutError)` for the live battle.

**The live battle.** For a raid from the campaign, call `GetWorld()->SpawnActor<AAirBattle>()->BeginLive(Session, Kind, Target, Ids)`. Kind is "bomb", "superiority" or "ship". This needs land's `battle-bridge.ts` and the air diff (`/mnt/project-files/air-battles/battle-bridge-air.diff`) in the rules bundle.

Under ruling 217 there is no campaign-screen button until the playtester passes the battle, so nothing calls this yet.

## Packaging

Stage `Content/Data` (AirBattles, Stages and Sound) with the game, as `Content/Effects` is staged. Then `Sounds/Real/credits.json` for the credits page.

## On the first build, look at

1. **Bank direction.** A plane turning right should have its right wing down. If every bank reads backwards, negate `P.Bank` in `AAirBattle::Draw` and in the close camera.
2. **Nose direction and size.** Each model is built nose toward -X and turned half round before its heading. The carrier aircraft (Kate, Val, Dauntless, Devastator, Wildcat) use the sculpted models where `Content/Models` has them. Otherwise each is the nation's plain fighter or bomber, scaled to the battle's span.
3. **The tags.** They should sit over their formations and fan out instead of stacking, his own in cream and the enemy's in muted paper. The near line should be amber.
4. **The clouds.** They should be white on their sunlit tops, grey under their flat bases, and soft at the edges,
   with no ring of polygon corners. The fight's smoke and tracer over the clouds should lie on them. If the clouds glare
   or vanish against the sky, their brightness is the three colours at the top of `AirSky.cpp`. If they cost too much
   time, `Placing::MostPuffs` sets their room and `Painter(12)` the sides of each puff.
5. **The sound.** Each formation's drone, the bursts by calibre, the flak bursts heard after their flash, and a fighter group's pass within 300 m. The loudness and distances come from mix.json's families.
