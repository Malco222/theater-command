# The air battle's 3D scene (ruling 238: Pearl Harbor and Midway first)

The air battles thread's 3D handoff (`/mnt/project-files/air-battles/3d-handoff.md`, sections 1 to 10) built for the
3D game. The battle is the rules' (`src/battle/air` on the browser branches). This scene draws it, and so does the
battle screen over it.

## The files

| File | What it is |
|---|---|
| `AirSky.h/.cpp` | The day's sky, plain C++: the clouds from `sky.place.cloud` (scattered cumulus, or a deck), laid on a square that repeats round the eye, and the painter that turns their puffs to the eye. |
| `AirShow.h/.cpp` | The flight core, plain C++. Every aircraft of every formation flies as an aircraft flies: banked turns, no turn tighter than 210 m, at most 6 g, no sliding, speed lost in a climb. It also handles the passes and breaks, the guns, the hits, the falls, the parachutes, the flak round the boxes, the bombs and torpedoes, and the ship news when the bombs arrive. |
| `AirFeed.h/.cpp` | Reads the bridge's JSON (sky, views, after card, recordings) into the flight core's terms. Plain C++. |
| `AirDesk.h/.cpp` | The screen's words and clock: the top bar, the aim slip, the near line, the dispatches, the tags, the after card, the orders as `TC.battleOrder` takes them, and the close camera's subject. Plain C++. |
| `AirBattleState.h` | The plain state above, kept out of the actor's header. |
| `AirBattle.h/.cpp` | `AAirBattle`, the scene. It feeds the core and draws what the core says: the planes, the small things, the effects kit, the stage and its ships, the cameras and the sound. |
| `AirBattleScreen.h/.cpp` | `SAirBattleScreen`, the screen. It draws the desk's words and takes the mouse and keys. |
| `AirStage.h/.cpp` | The Oahu and Midway stages (an earlier commit on this branch). |
| `AirGround.h/.cpp` | The stage's ground as painted, plain C++ (the cloud test paints the same): the grass varied and patched with red earth, the sand kept to the water's edge, the shallows darkening into the sea, the runways lifted. |
| `../../../Tools/air_show/` | The cloud test. It plays each recording through the core at 60 fps and checks the flight: g, turn radius, slip, roll, a fall for every loss, two gunners never at one window, the frame time. It also plays the screen's clock and words. `run.sh` builds and runs it; `air_show_frames.py` draws pictures of the flight. |
| `Content/Data/AirBattles/*.json` | The two recorded battles, `oahu` and `midway-fleet`, written by `tools/air-record` on the rules branch `claude/project-thread-2bdb2m`. |
| `Content/Data/Sound/mix.json` | The sound director's mix, read by the scene (it never copies the mix's numbers). |
| `Sounds/Real/*.wav` (17 new) | The air recordings of the approved battle pack: engines by model, guns by calibre, flak, and the falls. Credits are in `credits.json` and `CREDITS.md`. |

## Running it

1. `Tools/build_sounds.sh` brings the 17 new recordings into `/Game/Campaign/Sounds/Real`. Without it the battle plays silent and nothing fails.
2. `-TheaterAirBattle=oahu` or `-TheaterAirBattle=midway-fleet` on the command line plays the recorded battle about two seconds after the table is up. The table is hidden while it plays.
3. Adding `-TheaterAirBattleShots=<folder>` takes the art director's pictures and then quits:
   - the flyover: the raid, the target, and the guard when there is one (`00-flyover-*`)
   - the approach
   - the meeting
   - riding a fighter
   - the thick of it
   - riding again
   - the ships (the overview cut to the first ship that went down)
   - the fight going on
   - the after card

Mouse and keys:
- During the flyover, any click or key takes command: its shots end and a short glide brings the battle view.
- Left click takes his group in hand. Shift adds another group.
- Right click on an enemy tag attacks it. Right click on the sky patrols there.
- Drag turns the camera. The wheel draws it in and out.
- H holds, R breaks off, and C switches the close view.
- Space holds the clock or lets it run. Keys 1 to 4 set the speed.
- Esc answers KEEP FIGHTING. Enter presses CONTINUE.

A recorded battle shows the orders strip greyed out: it plays as it was fought.

**The clock (a reversible call, flagged to the design director).** In the 3D scene, ×1 is real time, so the aircraft fly at their true speeds (Brian: "not moving normal"). A battle second is `SKY.scale` (2) real seconds. The run-up and the run home go at ×10, and the clock is held while a question stands.

At ×1 the Pearl Harbor raid lasts about 7 real minutes, with contact about 20 s in. Midway lasts about 5 minutes, with contact about 30 s in.

**The flyover** (Brian's, approved 19 November 2026: before an air battle starts, a short cinematic flyover of both
formations, like generals surveying the field). When the battle's first groups stand, the scene plays up to three shots
on the real clock, cut one to the next, while the battle runs at ×1 under them (`AirDesk::Flyover`, in the cloud test):
- **the raid**, 5 s: the raider's biggest bomber formation coming on, seen from ahead and out to the side away from the
  target, from 60 m under it to 10 m over it as the eye closes from 1.7 to 0.5 of its size ahead;
- **what it comes for**, 4.5 s: from beyond the target, the far side from the raid, pushing in from 2.6 km to 1.9 km
  and down from 650 m to 520 m, the look lifting from the target toward where the raid comes from;
- **the guard**, 3.5 s, when the defender has fighters up: from behind them and a little over, looking out toward the
  raid.
Then the eye glides into the strategy view over 2.5 s (0.8 s after a click or key). The screen stands in a film's
frame while it plays, black above and below, with the raid's aim as its title and the line "SURVEYING THE FIELD · CLICK
OR PRESS ANY KEY TO TAKE COMMAND"; the frame draws back over the glide and the battle's own screen returns. Each eye is
kept 60 m over the stage's ground. Pearl Harbor (no American fighters up at the start) runs 9.5 s of shots, Midway
(the carriers' fighter patrols up) 13 s. `-TheaterNoFlyover` goes straight to the battle. The cloud test checks the
shots on each recording: the raid and the target shown, at least 60 per cent of the raid's aircraft in view through its
shot, 14 s or less, and the eye never jumping within a shot, never under 60 m, never within 40 m of an aircraft.

**The size of the words** (the art director on build 1734: the tags and the running words were small at Brian's
window). Unreal draws its game screen at the window's height over 1080; the battle's screen is drawn 1.25 times that
(an `SDPIScaler` round it, in `ShowScreen`), so it keeps the same size against the window at every size and everything
on it grows together, the dark plates with their words. `-TheaterAirTextScale=` tunes it (0.75 to 2); the UI director
has the last word on the type.

**The cameras** (the art director's notes on the first Mac run, build 1720). Both fill the whole window, whatever its
shape (a camera actor keeps 16:9 and bars the rest; both have `bConstrainAspectRatio` off).
- **The overview** starts 1.1 km behind his own raid and 14 degrees above it, looking along its way (from 2.6 km the
  aircraft were specks). It follows the group in hand, else the fight, else his raid; drag turns it and the wheel draws
  it in and out (120 m to 20 km). For "the ships" picture it cuts to the first ship that went down, 1.6 km off and 24
  degrees above her in harbour, and back to the fight after it. At sea (the art director on build 1734, Midway 06) she
  fills about a third of the picture's width: a 35-degree lens from her sunlit side, 35 degrees forward of her beam,
  13 degrees above her, so her smoke stands up over her (`AAirStage::ShipLie` gives her length, bow and harbour).
  Depth of field is off in the whole game (the campaign's light trims), so it was not what made that picture soft:
  the scene is drawn at the campaign map's pixel budget (about 1077 by 668 on Brian's 2881 by 1786 window) and filled
  out by Unreal's upscaler. `-TheaterAirSize=85` draws the battle at 85 hundredths of the window's points each way, for
  timing a sharper scene; without it nothing changes.
- **The close view** rides one aircraft: of the group in hand, else the busiest of his own side's fighters near the
  middle of the picture, else the other side's. After 10 seconds on one it moves to another of his, but stays if that
  one is the only aircraft of his still up. A fighter is seen from 22 m behind along its path and 6 m above it,
  looking 80 m past it, so its back, its fin and the tops of its wings face the eye. A bomber is seen from 1.8 spans
  behind it, 0.55 out on the side the attack comes from and 0.45 above, looking 1.2 spans past its nose: the whole
  bomber in the lower middle of the picture, both wingtips inside it. The eye is set from the
  aircraft every frame (never eased toward it, so it cannot fall behind or swing past a turning one); only the way it
  trails is eased, over a third of a second. The horizon rolls a quarter of the bank, never past 15 degrees. A little
  motion blur (0.12). A fall, a straggle or a limp cuts to the next aircraft.

**The aircraft.** A sculpted model (`Content/Models`) has two shapes: its far one (a few thousand points) for every
aircraft, and its near one (tens of thousands) made only for an aircraft within 300 m of the eye, or the one ridden,
and shown until it is 450 m off. The stand-ins have both shapes too (the plain Fighter and Bomber models carry far
parts). The eye is set before the aircraft are drawn, so a cut to a new aircraft shows it at its near shape on the
very frame of the cut. A stand-in (the nation's plain fighter or bomber, sized to the battle's span) wears
its navy's paint where the country's map colour would be: the Japanese navy's grey (A9B19F), and the American navy's
blue-grey (566A78) on the Wildcat, Dauntless, Devastator and Avenger (by either name the recording uses: "F4F" or
"Wildcat", and so on). The army's machines keep theirs. The battle
models' Kate, Val and Dauntless (`battle-models/air/*.json`) are in the same form as `Content/Models`: put there, they
replace the stand-ins with no change to the scene.

**The sea** (the stage). Round the battle, 48 km a side, the sea is a grid of 250 m squares whose colour wanders in
patches drawn out along the trade wind (from about 070, turned with the stage, so off Midway too), 3 km by 1 km and
1.5 km by 600 m (none smaller than two squares across, or the grid would draw it as speckle), and whose surface tilts
up to some 5 degrees, so the sun's
path on the water breaks into glitter; the patches fade out over the grid's last 4 km. Past it, a plain sheet in four
bands. Its colour is darker than before (1C4453), with a tighter sun path (roughness 0.2): looking down from 5 km the
first run's sea measured 100 to 118 of brightness, and the art director's mark is 80 or under before the grade.

**Falling aircraft.** Every loss hands the effects kit its fall (`AirFall`, kit head 44af184). An aircraft blown apart
throws its debris row at once round the kit's torn fireball. One going down burning or spinning sheds pieces as it
falls (`AirFallMove` every frame, `AirFallStop` when the model is gone). The pieces fade into the ground under the
fight, or in the cloud deck for a fall begun over a drawn tile of it. A blow-up keeps only its two biggest pieces trailing fire and
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

**The sea and the clouds' brightness (the art director on build 1732, 16:21Z).** The stage's sea paint (#1C4453)
stands as it is under a sun up to 20 degrees high and darkens as the sun climbs, to 0.65 of it at 45 degrees and over,
its hue kept (`SeaPaintUnder` in AirStage.cpp): Pearl Harbor's morning sun at 18 degrees leaves it as it was, and
Midway's at 45 takes the open sea from 78 to 90 of brightness toward her 55 to 60. The clouds' ink is 1.3 times their
paint (`CloudBright` in LaySky): through the grade the sunlit tops read 226 to 231, her mark is 238 to 245 with no more
than about one per cent of the picture at 250 or over, and the paint's top is already 252. The painter paints the
shaded sides, the bases and the shadows that much darker (`Painter::SetInk`), so only the sunlit tops rise: the bases
already pass at 140 to 155 and the deck's troughs at 12 to 18 per cent darker (her 16:31Z). `-TheaterCloudBright=`
tries another.

**The harbour's ground and yard (the art director on build 1734, 16:55Z; the historian's blocks).** Pearl Harbor's
land read as flat blocks in three plain colours with hard polygon bands, and the shallows as a mint band. The stage's
meshes now go through `AirGround::Paint` before they are built (BuildGround):
- the grass is varied in broad patches at two sizes (about 380 m and 140 m), and here and there turned toward the red
  earth of the pineapple fields' paint in patches a few hundred metres across. Over the 6 km round the harbour the
  grass's brightness spreads 5.6 luma, by the art director's measure;
- the sand keeps to the water's edge: a sand point back from it takes the grass behind it, long shore edges get a point
  10 m in, and a large all-sand triangle gets a greener middle by its distance from the coast;
- the shallows' open-sea edge takes the sea's own paint (`SeaPaintUnder` at the stage's sun), so the band darkens from
  the coast into the deep water. Midway, with its lagoon, keeps the band's edge;
- the runways stand 0.8 m clear of the land (Ford Island's strip lay at the island's own height).
Oahu's palette is also darker and less mint (`tools/stages/mesh.py`'s `OAHU_PAINT`: cane #5D7934, pineapple #7D5A3C,
forest #35512A, sand #CDBD95, shallows #3A6E68, reef #6C9E8E). The mottle finer than the points are apart is the
material's own grain (M_MapSurface's two noise samples at one tile to 1,800 m and one to 260 m, strength 0.28;
`-TheaterGroundGrain=` tries another, 0 for none).

The stage file's `yard` (from `tools/stages/yard.py`, the historian's table of 19 November) draws the 1941 Navy Yard
and Ford Island's seaplane base as grey blocks in BuildWorks:
- Dry Dock No. 1, drained and dark inside its coping, its caisson the steel end on the water. It runs 211° from the
  mouth (the historian's correction), and Pennsylvania, Cassin and Downes lie in it along that line;
- YFD-2 round Shaw;
- eight shops on their concrete pads by the 1010 Dock;
- the seaplane apron at Ford Island's south end with Hangars 79, 37 and 54. The three stand-in hangars by Ford
  Island's runway are gone.
The cloud test checks that each docked ship lies inside its dock and along it, and that no ship's middle is under a
block. Its picture: `AIR_GROUND_DUMP=1 Tools/air_show/run.sh`, then
`STAGE=Content/Data/Stages/oahu.json python3 Tools/air_show/ground_picture.py <out>/ground-oahu.bin harbour.png -1700 -1900 900 200 1.25`.

**The battle look (the 3D builder's, `Campaign/CampaignBattleLook.h/.cpp`).** The scene calls it in five places: the
art director's grade on the scene's own camera (`Grade`, when the eye is made); the air haze, which starts 15 km out
at twice the scene's density (`AirHaze`, after the scene sets its haze, whose numbers it scales); a sky behind the
battle lit by the battle's sun, with the table's other lamps kept out of it (`AirSkyBegin`, once the key lamp faces
the sun); and `AirSkyEnd` on CONTINUE, after the hazes are put back. The two files are the builder's, byte for byte as
built in its 1732. This branch carries a copy only so that it builds alone; on any difference, the builder's own copy
wins. Its switches (`-TheaterBattleGrade=`, `-TheaterAirHaze=`, `-TheaterAirSky=`, and `-TheaterNoBattleGrade` and
`-TheaterNoAirSky` to turn them off) are described in the header.

**The sky.** Each battle carries its day (`sky.place.cloud`, the air thread's `sky.ts`): a cloud deck or scattered
cumulus, with one cloud base and one top. Pearl Harbor and Midway are cumulus days, as history had them: Oahu's
base 940 m and tops 2,060 m, the Mobile Force's 510 m and 1,820 m. A ticket from a rules bundle without the sky draws
cumulus at the cloud table's middle heights. `AirSky` lays the clouds once, on a 44 km square that repeats without a
seam, and draws each cloud where it lies nearest the eye. So there are clouds wherever the fight goes (at Pearl Harbor
most of the fight and the falls are 20 to 32 km short of the harbour). The scene draws them as one see-through mesh of
soft puffs, in the effects kit's shape and with the same `M_Highlight`. They are turned to the eye only when it has
moved about a quarter of a degree at the nearest puff, as soon as the eye moves after a paint that drew nothing, and,
while a drawn cloud's lift over high ground is still easing, for each metre the wind carries the clouds.

- **How far.** Clouds are drawn out to 20 km from the eye and thin into the haze over the last third of that. A cloud
  within 5 km is drawn with all its puffs, out to 10 km with its bigger ones, and past that with two: its veil and one
  soft lit body, which fades in as the rest fade out, so the clouds run on small toward the horizon with no empty band.
  The painter's room is 4,000 puffs (`MostPuffs`); the two battles draw about 2,800 at most. With no room, the farthest
  puffs are left out.
- **Cumulus** cover about three eighths of the sky: a trade-wind sky (the art director), many small ones (300 to 600 m),
  some middling and a few big (1.5 to 2.5 km), in loose clusters with gaps and in rows along the wind about 4 km
  apart. Each is a flat grey base, a lit, lumpy dome wider than it is tall, a few turrets and a faint veil that holds
  it together (within the body, so it lays no pale disc on the sea; no deeper than 120 m under the base at its faded
  rim, so a flat base stays flat). Most are shallow, and a few reach the day's tops. They drift with the battle's wind.
  Over the target there is a clearing that stays put as they drift: a cloud carried into it thins away, and one
  carried out of it fills in, so the ships or the town stay in sight.
- **Light and shade** come from each cloud's shape: every point of a puff is lit as the point of the cloud's rounded
  body it stands for. Sunlit tops come out about 242 of brightness, down-sun sides about 165, the base from under it
  150 to 155 with a straight, crisp underside (from above, the dome hides it). The sunlit tops and sides are crisp
  over their outer fifth; the base and the far body stay soft. (The art director, 15:17Z.) A third of each point's
  light is from its own round lump as well, so a small cloud's sunlit cap grades rather than reading as a white
  pearl, and the sun ramp ends at 0.9 rather than 0.85. A lump's edges away from the sun are soft (up to 0.7 of its
  radius), those toward it crisp, and no crisp edge is narrower than about six thousandths of a radian (some six
  pixels). Each puff's ring of 16 is turned by its own part of a side, so no two puffs' corners line up, and its
  lumps are three and two to a turn (five to a turn drew a zigzag of corners). Tops in the previews 230 to 245
  (99.5th percentile), brightest 240 to 249. On a clear day each cloud of 600 m or more lays its
  shadow within 10 km of the eye, down-sun, about 27 per cent darker in the picture than what it falls on (half the
  light off at its middle: the blend is in light, and 27 per cent of light was only 13 of brightness; on the open sea
  of the low picture, 42 to 44 against 57 to 59): on the sea, and over land too (the art director, on build 1722),
  draped over the ground as a soft patch, each point of it laid 25 m over the highest ground under it and its
  neighbours so the hills do not cut through. The shadows are a mesh of their own, drawn before every other
  see-through thing whichever way the eye is (sort -40), so the ground's fires and smoke and the air fight stand in
  front of them (the review: in the clouds' mesh they were laid over the harbour's fires). The haze is Unreal's
  height fog at density 0.00045 and falloff 0.006 from 2 km (the picture tool follows the same formula).
- **A deck** covers most of the sky, with a few holes. It sits 1.5 to 2.5 km under the lowest bombers above 2,000 m
  (the art director). It is moved up or down to that and keeps its thickness. Over the lowest band (1,800 m) it lies
  above the bombers instead, its base 600 m over them. It reads as a floor of cells: each tile darker at its rim where
  it meets the next, with lumps of mixed size over it. A strike at ships keeps a ragged hole over the ships: its edge
  3.6 km out, give or take three tenths, in lobes and longer along the wind, the tiles thinning and breaking up over a
  kilometre across the edge, and a few small scraps of cloud in it, none within 2.4 km of the ships (falls pass
  through the scraps). A deck does not drift (from bomber height its drift could not be seen), so the hole stays over
  the ships.
- Cloud never sits lower than 300 m over the ground under it, so a cloud over a mountain is lifted with it where it
  is drawn: 300 m over the ground under its puffs' hearts, 200 m under their faded rims. The ground is looked at on a
  lattice 120 m apart (finer than the stage's 150 m cells) out past the rims, so no cell under a cloud is missed. As a
  cloud drifts over hills, the lift drawn eases toward the higher of the ground under it and 250 m downwind, up one and
  a half metres a metre of drift and down half as fast, so it rises before the high ground reaches it and never hops. A deck tile lifted so high that it would come within 1.5 km of the bombers is thinned, or left
  out where the mountain stands through.
- **Falls and the deck.** Something falling into a drawn tile ends 150 m under its top puff's middle (halfway, in a
  thin tile), so it is hidden before it ends and nothing pops from view: aircraft, pieces and parachutes alike
  (`DeckAt` in the core gives the tile drawn over a point, its foot and that hiding height, out to the scene's reach;
  `DeckZ` for `AirFall` is that height, or the ground). Through a hole, or under the foot of the tile over it (a fall
  begun under the deck, a torpedo bomber, a low fight, or one come down a hole), it goes on down to the sea or the
  ground, unless it falls into another tile on the way: over high ground, tiles side by side are lifted unlike, and
  each counts by its own foot. Under the deck's laid base no tile reaches, so it is under it for good. A fall's debris
  row sheds only while the fall is over a drawn tile above the height its pieces leave sight; once not, the row is
  let go, so no piece fades in clear air. A fall ends after 400 s at most, a parachute after 1,500.
- **Two effects kits.** The fight in the air uses the effects kit as before. A second kit (`GroundFx`) draws what
  happens under the clouds' base: bomb bursts, crashes, splashes, fires and the stage's oil smoke, and the fight's own
  tracer, hits, flak, trails, debris and torn flames while they are under it (each by where it is, with 50 m either side
  of the base before it changes kits, so nothing flickers between them; a trail carries on in the other kit at its
  full look, as of when its kind began, and does not light again). See-through things are drawn one after another,
  not by depth, so the order follows the eye. With the eye over the clouds' base under it, the ground's kit is drawn
  first, then the clouds, then the air: every cloud between the eye and something under the base hides it. The
  clouds' shadows come before all of them. Under the
  base, the air's kit comes first (what it draws there is above the base, behind the clouds), then the clouds (they
  are the sky behind), then the ground's kit. The order turns 50 m either side of the base.
- The cloud test checks the sky too: the day as read, the heights, the cover (0.31 to 0.45), the clearing kept as the
  clouds drift (when the bombs fall, at the end and two hours on), cloud round the falls far from the target, the room,
  the same sky every time, the far-to-near order and the painter's time (under 5 ms for the worst eye, the least of three middles of 15 paints so that one slow stretch
  of a shared machine does not fail it; 2.9 to 3.7 with the look round's lumps and the shadows over land, about 1.8
  times what it was when the mark was 4 ms). It checks that the shadows are laid apart
  from the puffs (none dark among them; some on a cumulus day). It checks the clearing
  on what the painter draws as well. On a cumulus day no fall or parachute ends in the air. It lays a deck under a
  4,500 m band, a deck raised to the band, a deck over the lowest band, a strike's hole, three kinds of high ground (a
  broad 2,000 m mountain, a steep ridge as steep as the Koolau, a 3,800 m peak) and a ticket with no sky. Over each
  hill, and over Oahu's own ground (the stage's grid, at Kaala and the Koolau's highest), it paints the clouds as the
  wind carries them for 20 minutes: every puff's heart 150 m over the ground under it, its rim 30 m, each cloud's foot
  300 m, and no cloud's lift changing faster than 8 m a second. An eye flying round the harbour for a minute asks for
  no more paints over the island than over the sea. It flies the battle again twice: under a test deck over the sea,
  its hole laid over the middle of where the falls begin (so some falls from above go down through it and the rest end
  in the tiles round it, whatever the day's tiles happen to leave open), and under the lowest band's deck lifted over high ground beside where the falls are thickest, so that tiles side by
  side lifted unlike are where falls are. Every end is judged against the drawn tiles found one by one: in a tile,
  under its hiding height; or on the sea or the ground; never in clear air, nothing left falling in cloud, and no
  debris row shedding in clear air. A third flight puts a sharp step in the deck where falls begin (a tile higher than
  any fall on one side, a thick low one on the other): falls that drift across must end in the low tile, and no plane,
  piece or parachute is left falling inside it. `AIR_SKY_PICTURES=1
  Tools/air_show/run.sh` also draws each battle's clouds from four eyes (one where the battle opens, far from the
  target), steeply down from 5 km, and a deck day, with a small software painter (`air_sky_picture.cpp`). Unreal's light and haze are only
  stood in for there.

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
2. **Nose direction and size.** Each model is built nose toward -X and turned half round before its heading. (On build 1720 the close view's aircraft were far and nearly level with the eye, and read as seen from ahead; the stars on the P-40's wings show it was from behind. The close view now looks down on its back from 22 m.) The carrier aircraft (Kate, Val, Dauntless, Devastator, Wildcat) use the sculpted models where `Content/Models` has them. Otherwise each is the nation's plain fighter or bomber, scaled to the battle's span, in its navy's paint.
3. **The tags.** They sit over their formations, his own in cream and the enemy's in muted paper. Tags that would touch join one block: it shows two (the groups in hand, then his own, then the largest) and a "+N more groups" line, never a column. A block moved off its formation keeps a thin line to it. The near line and "x10 UNTIL THE FORMATIONS MEET" are amber on a dark plate; the aim slip and the after card wrap their lines.
4. **The clouds.** They should be white on their sunlit tops, grey under their flat bases, and soft at the edges,
   with no ring of polygon corners. The fight's smoke and tracer over the clouds should lie on them. If the clouds glare
   or vanish against the sky, their brightness is the three colours at the top of `AirSky.cpp`. If they cost too much
   time, `Placing::MostPuffs` sets their room and `Painter(12)` the sides of each puff.
5. **Pearl Harbor's ground.** The grass mottled with patches of red earth, the beach a band along the water, the
   shallows darkening into the sea, and the yard's grey blocks with Pennsylvania in her dry dock. If the grain shows as
   a pattern, `-TheaterGroundGrain=0.15` softens it.
6. **The sound.** Each formation's drone, the bursts by calibre, the flak bursts heard after their flash, and a fighter group's pass within 300 m. The loudness and distances come from mix.json's families.
