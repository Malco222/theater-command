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
| `AirGround.h/.cpp` | The stage's ground as painted, plain C++ (the cloud test paints the same): the grass varied and patched with red earth, the sand kept to the water's edge, the shallows darkening into the sea, the runways lifted, the ground split finer near the battle, the yard's paving as worn slabs. |
| `../../../Tools/air_show/` | The cloud test. It plays each recording through the core at 60 fps and checks the flight: g, turn radius, slip, roll, a fall for every loss, two gunners never at one window, the frame time. It also plays the screen's clock and words. `run.sh` builds and runs it; `air_show_frames.py` draws pictures of the flight. |
| `Content/Data/AirBattles/*.json` | The recorded battles, `oahu` and `midway-fleet` (ruling 238), and `bremen-b17` (a B-17 day raid of December 1943 with its Mustang escort, met by Fw 190s, over the board's own land with no stage; Brian's ask of 20 November to compare with the B-17 clip), written by `tools/air-record` on the rules branch `claude/project-thread-2bdb2m`. |
| `Content/Data/Sound/mix.json` | The sound director's mix, read by the scene (it never copies the mix's numbers). |
| `Sounds/Real/*.wav` (17 new) | The air recordings of the approved battle pack: engines by model, guns by calibre, flak, and the falls. Credits are in `credits.json` and `CREDITS.md`. |

## Running it

1. `Tools/build_sounds.sh` brings the 17 new recordings into `/Game/Campaign/Sounds/Real`. Without it the battle plays silent and nothing fails.
2. `-TheaterAirBattle=oahu`, `-TheaterAirBattle=midway-fleet` or `-TheaterAirBattle=bremen-b17` on the command line plays the recorded battle about two seconds after the table is up. The table is hidden while it plays.
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

   Each ride leaves its fighter half a second after its picture. For a film of the run, `-TheaterAirBattleRideHold=40`
   stays on the ride 40 s after it (0.5 to 300), so the film follows the fighter through the fight; a picture due
   meanwhile is taken from the ride. So a film run's stills are not a picture run's: they do not go to the art
   director as one.

   In harbour "the ships" is seen from upwind, 55 degrees off the wind, at least 700 m clear of every smoke column over
   a ship struck, sunk or moored within 800 m of her (or 0.35 of the eye's way to her, if more: at 300 m on build 1778 a
   near column out of focus filled 45 per cent of the picture, and the art director's mark is 60 per cent clear, no
   column in the near eye's fade). Where none is clear at 1.6 km the eye tries 35 and 75 degrees off the wind as well,
   then backs off and rises in steps, the nearest clear eye of any of them (the log line "the ships picture in harbour"
   gives where it stood, its heading and whether every column was kept clear).

What else the scene draws for the battle:
- Each sculpted aircraft near the eye turns its airscrews: the blades the model was built with (Battle/AirScrew finds
  them by their paint) are taken out of its near shape and a see-through blur disc turns at each hub, 1.9 turns a
  second as a film's camera sees it (`-TheaterScrewTurns=` tries another), slower on an aircraft falling. Far shapes
  keep their blades, too small to see turn. A blade is every paint whose name in the file's `paint_names` begins
  "blade" (the newer Kate, Val and Dauntless paint each blade in three), paint 9 in a file without names; a model's
  own painted blur disc (`blur`, `blur_tip`) is taken out with its blades, the turning disc standing in for both.
- Each aircraft goes down the way the battle names it (the air rules' `how`, 11 ways): blown apart, burning, a flamer,
  a wing or the tail torn off and fluttering down, the pilot hit (a dive, no smoke), spinning, a glide on to the sea or
  a belly landing (floating or lying a while), broken up, struck by another's wreck. The battle also says when it
  catches fire, blows apart, strikes and opens each parachute, and the scene keeps to it. A pilot hit and a glide shed
  no debris; a ditching throws a spray, not a bomb's column, and a belly landing dust with no fire.
- Hits survived show on the model in the formation's slot the battle names: sparkles, panels torn away, a puff and a
  trail, or an engine fire in the box that its crew put out.
- A dead engine smokes for as long as it is out (Brian, 20 November 15:29Z: "a lot more dark smoke strains from that
  engine and directly behind it on the tail"): each dead engine its own ribbon from its own nacelle, kinds "fire"
  while it burns, "oil" while it windmills (a liquid-cooled type white "glycol", then after 45 s the "streak", as G1
  has it), "doused" for 2 s when its
  crew put the fire out, then "streak", which never clears, after a fire is out and back in the box too. A kind
  change is a TrailStart on the same key, so the ribbon never breaks. Until the effects thread's rows land the kit
  trails the new kinds as smoke. A streak gives way, as a piece's trail does, when the room is full. Each aircraft
  out carries its dead engines, its airscrews feathered or windmilling, and its soot (0 to 1 over 30 s) for the
  modeller's soot parts when they land.
- Tracers are passed their aircraft's nation for the art director's colours (G2) once the kit takes it, named as
  the kit's rows are (us, gb, de, it, jp, su) from the recordings' USA, UK, GER, ITA, JAP and SOV; any other nation
  is drawn in the kit's default.
- Each bomber gunner lays his own gun (the art director's D6, on the Bremen film: "one gun is one stream", the box's
  streams "a web converging on the fighter", never a starburst). He leads the fighter by 55 to 105 per cent of the
  full lead, slowly changing, and wanders about it, about 0.6 degrees in a power turret and 1.8 with a hand-held gun.
  His gun follows that point about 0.2 s behind and swings no faster than 40 degrees a second in a turret, 50 by
  hand, and he holds his fire while he swings onto a new fighter more than 15 degrees off. The test counts it: no
  more than 1 in 20 pairs of gunners on one fighter lay on one spot, and 99 in 100 of the guns' frames swing at
  under 55 degrees a second (before: 1 in 15, and 145).
- Looking up, the sky is lifted toward a deep blue (the art director: never near black, about 90 to 110 luma graded;
  the Bremen film's zenith read 29, and 43 to 50 with the first lift of 0.3). Light is added on a bowl 9 km round the
  eye, none 12 degrees up and most at the zenith, tinted blue, and its strength grows with the eye's height above the
  low haze (none below 1,000 m, all of it at 7,500 m). "-TheaterZenith=1.3,12" tries another strength and lowest
  angle; 0 turns it off.
- From high up the sea's flat sheet ends short of the round world's horizon, and its far edge showed as a hard straight
  line across the sky's foot, the hazed sea under it lighter than the thin-aired sky over it (the art director on build
  1778's Bremen 01: the haze should swallow the horizon). A haze band stands round the eye at 0.6 of the way to the
  edge, in the haze's own colour with the height fog over it as over the sea behind: none where it meets the sea, all of
  it from just under the edge to just over it, thinning to none 7 degrees above. It is the pale band low on the
  horizon of the trade-cumulus brief (item 3), at every height, now that the haze lies over the sky at a third
  (BattleLook::AirSkyBegin's HazeOver, 0.35; it was all of it, and the whole upper frame read as white fog, 217 to 226,
  no blue): the air above the inversion clean and the sky overhead blue. "-TheaterHazeBand=1,0,1" tries another
  strength (its brightness against the haze's; 0 turns it off) and the eye's heights from none to all of it;
  "-TheaterAirSky=,,,1" lays the haze wholly over the sky again.
- A picture run's second ride (05-ride-again, and a film's second ride) rides the other side's aircraft: on Bremen a
  German fighter.
- Past the nearest 16 parachutes, those over 600 m from the eye are left out, the farthest first.
- A ship blown up at sea takes her magazine fire down with her (about 30 s); her oil burns on where she went down.
- A cloud's shadow lies over the highest ground under it by half a metre and 4 m for each metre the ground under its
  points differs, at most 25 m (the pre-check on Bremen and Oahu): on flat land it passes over a flat field's waiting
  aircraft, not above them, and among hills it clears the ground between its points without lifting off a slope.
- A gun's burst sound stops with the gun: a take still playing is faded out when its gunner stops (a crew baled out,
  a target gone), not left to run to its end.
- The stage's ships are each fitted to her real length and beam (AirFeed::HullOf), the kit having one model a class.
- With no stage, the board's land rings are the ground (AirGround::BoardLand, 2 m up): bombs on them are land bombs,
  and aircraft crash on them rather than splash.

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
- **the raid**, 5 s: the raider's biggest bomber formation, from above and just behind its trailing element (its
  plane furthest back as the shot begins and the two nearest it), so the whole raid stretches ahead of them toward
  the horizon in its vics (the art director on build 1760: the lead three alone did not read as a raid), looking down
  21 degrees at them (13 until the art director on build 1756 asked for 8 more, so the cloud field fills the lower
  half) so the sea or the cloud deck is behind them, near enough that each is about 125 px wide in the 2881 px picture
  (her "about 120"), looking square across at the raid's middle and halfway ahead to it (`RaidLookOn`; the middle
  kept in the group's frame as the shot begins, so the look never hops as a plane drops out). The cloud test holds
  the nearest three to 100 to 160 px and three quarters of the raid in the picture (Midway 9 of 11, Oahu 10 of 12;
  a formation of vics is wider than the lens at that range, so a wingman or two at its edges falls outside). The look
  is drawn back toward the trailing three before any of them would leave the picture: none more than 11 degrees under
  its middle (the bar lies 12.7 under) or 20 to its side (`RaidLow`, `RaidAside`; the pre-check's cure on round 13,
  for raids whose rearmost vic flies on a flank, where the look had turned 30 to 49 degrees off the raid's heading).
  Then the picture holds the three and about half such a raid. The eye pushes
  in from 1.2 to 0.9 of that distance and swings in behind them from out on the side away from the target (the art
  director on build 1750: the planes had been 10 to 15 px specks in a white void). The side is taken on the shot's
  first frame and kept: Bremen's raid flies straight at its target, and its heading wavering by a hundredth of a
  degree had swung the eye 310 m across the raid in one frame;
- **what it comes for**, 4.5 s: from beyond the target, the far side from the raid, pushing in from 2.6 km to 1.9 km
  and down from 650 m to 520 m, the look lifting from the target toward where the raid comes from; over a harbour
  (`place.kind` "harbour") it lifts no further than holds the target 9.5 degrees under the picture's middle, in its
  lower third (the art director on build 1750's Oahu frame, where the hills had pushed Ford Island and the ships off
  the picture's foot: some 10 degrees down at the picture's moment);
- **the guard**, 3.5 s, when the defender has fighters up: the raid's shot, on the guard's lead three (its plane
  furthest ahead and the two nearest it), each about 150 px wide, from 13 degrees above.
Then the eye glides into the strategy view over 2.5 s (0.8 s after a click or key). The screen stands in a film's
frame while it plays, black above and below, with the raid's aim as its title and the line "SURVEYING THE FIELD · CLICK
OR PRESS ANY KEY TO TAKE COMMAND" (13 point, 1.3 times its first size: the art director on build 1750); the frame draws back over the glide and the battle's own screen returns. Each eye is
kept 60 m over the stage's ground. Pearl Harbor (no American fighters up at the start) runs 9.5 s of shots, Midway
(the carriers' fighter patrols up) 13 s. `-TheaterNoFlyover` goes straight to the battle. The cloud test checks the
shots on each recording: the raid and the target shown, 14 s or less; at the raid's and the guard's pictures their
three biggest planes in the picture 120 to 180 px wide, and never fewer than three of the formation in the picture after
each shot's first second; a harbour target in the lower third at its picture; and the eye never jumping within a shot,
never under 60 m, never within 40 m of an aircraft (the formations' trailing planes pass some 85 m from it). The
pictures' moments: the raid 3 s in, the target 2.5 s into its shot, the guard 2 s into its shot.

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
  fills about a third of the picture's width: a 35-degree lens 9 degrees above her, so her smoke stands up over her
  against the sky (`AAirStage::ShipLie` gives her length, bow and harbour), from upwind, looking 55 degrees off the
  wind's way, so her column streams away and across the picture off her (the art director on build 1760: from
  downwind it leaned at the eye, its nearest puffs huge and soft); of the two such eyes the one nearer her beam, and
  on a near tie her sunlit side.
  Depth of field is off in the whole game (the campaign's light trims), so it was not what made that picture soft:
  the scene is drawn at the campaign map's pixel budget (about 1077 by 668 on Brian's 2881 by 1786 window) and filled
  out by Unreal's upscaler. `-TheaterAirSize=85` draws the battle at 85 hundredths of the window's points each way, for
  timing a sharper scene; without it nothing changes. On build 1760 the column was as soft at 85 as at the map's
  budget, for 1.9 ms more of the graphics chip a frame, so the battle keeps the map's budget; the softness was the
  column leaning at the eye, hence the upwind eye.
- **The close view** rides one aircraft: of the group in hand, else one of his own side's fighters closing on what it
  fights within 2.5 km of it, the nearer and the more nearly pointed at it the better, firing first, any after a bomber
  before any after a fighter, and one coming at a bomber head-on most of all, so what it fights is in the picture with
  it or soon will be (`AirDesk::ChooseRide`; Brian on the Bremen film, 20 November: "where are the german fighters?",
  both rides had followed a Mustang and the German fighters showed as specks). The cloud test asks it each second a
  side has a fighter closing on a bomber: its pick on a bomber three times in four or more, none over 2.5 km off (on
  round 18b every time: 125 of 125 seconds at Bremen, 86 of 86 at Midway, 27 of 27 over Oahu); with none such, the busiest of his fighters near the middle of the picture, else the other side's. After 10 seconds on one it moves to another of his, but stays if that
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
bands. Its colour is the deep blue below (1F577A), with a tighter sun path (roughness 0.2): looking down from 5 km the
first run's sea measured 100 to 118 of brightness, and the art director's mark is 80 or under before the grade.
A stage's land more than 25 km from the battle's aim (`AirStage::LandReach`) is not built: a fight at sea keeps
its own sea. Midway's fleet battle is fought 37.6 km from the atoll (33 km from its nearest reef, the stage's log
line on build 1760), which had stood on its horizon as a bright
lens through the haze (the art director on build 1756's Midway 05); the raid on the atoll keeps it. The
stage's log line says when its land was left out, and how far.

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

**The sea and the clouds' brightness (the art director on build 1732, 16:21Z, and on build 1778).** The stage's sea
paint (#1F577A, a deep blue: 1.7 times the light of build 1778's #1C4453, whose harbour read 34 of brightness and
grey-teal looking down on the ships at Pearl Harbor against the brief's 50 to 70) stands as it is under a sun up to 20
degrees high and darkens as the sun climbs, to 0.55 of it at 45 degrees and over, its hue kept (`SeaPaintUnder` in
AirStage.cpp): Pearl Harbor's morning sun at 18 degrees leaves it as it is, and Midway's (45 degrees until round 18c,
now 68 up in the east-south-east, bearing 105, for the dive bombers over the carriers at 10:20: the art director's
item 5) keeps the open sea about where build 1770 measured it (47 to 58, near her 55 to 60), a little lighter. The battle's grade takes nearly
half the colour out of the darks, so the paint carries more blue than the picture shows. The clouds' ink is 1.3 times their
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
- the runways stand 0.8 m clear of the land (Ford Island's strip lay at the island's own height);
- within 4 km of the battle's aim (`Focus`) the land is split until no edge is longer than 80 m and the shallows 30 m,
  each new point painted for its own place, so the mottle and the shallows' fade no longer spread across big triangles
  as stars and wedges (the art director on the first picture, 17:45Z). About 62,000 triangles more, 215,000 in all;
- the shallows fade by how far a point lies between the coast and the band's open-sea edge;
- a shore on sheltered water (the open sea's edge within 130 m: the harbour's natural shores) is the grass to the water,
  not a thin line of sand that read as a drawn outline; quays and seawalls keep their grey;
- the piers' concrete is worn (`Worn`: patches of 6 per cent at 40 m, stains to 7 per cent darker at 16 m), and there
  is no reef inside the lochs (today's mapped shoals there read as mint patches);
- by the quays, piers and seawalls (within 10 m of the works, coming back over 40 m) the shallows take the deep sea's
  paint, since a berth was dredged to the wall; along the natural shores the band's fade wanders in width (its curve's
  power 0.55 to 1.55 on 260 m noise), so it no longer reads as a glow drawn round the land (the art director on the
  second picture, 18:47Z). A sand point back from the coast, by height or past 8 m, fades to the grass as painted there.
Oahu's palette is also darker and less mint (`tools/stages/mesh.py`'s `OAHU_PAINT`: cane #5D7934, pineapple #7D5A3C,
forest #35512A, sand #CDBD95, shallows #3A6E68, reef #6C9E8E, concrete #8F8B83). The mottle finer than the points are apart is the
material's own grain (M_MapSurface's two noise samples at one tile to 1,800 m and one to 260 m, strength 0.28;
`-TheaterGroundGrain=` tries another, 0 for none).

The stage file's `yard` (from `tools/stages/yard.py`, the historian's table of 19 November) draws the 1941 Navy Yard
and Ford Island's seaplane base as grey blocks in BuildWorks:
- Dry Dock No. 1, drained and dark inside its coping, its caisson the steel end on the water. It runs 211° from the
  mouth (the historian's correction), and Pennsylvania, Cassin and Downes lie in it along that line;
- YFD-2 round Shaw, off the beach west of the dry dock where Nevada beached (the historian from three 1941 photo
  captions), Shaw bow west;
- eight shops on their concrete pads by the 1010 Dock;
- the seaplane apron at Ford Island's south end with Hangars 79, 37 and 54. The three stand-in hangars by Ford
  Island's runway are gone;
- Maryland's two 1941 mooring quays at F-5 (kind 4), where today's F-5 pier stands in the map: that pier, its two
  S378 pieces and S382 at F-12 are cut from the works (`yard.py`'s `CUT`), as later than 1941 (Maryland's and
  Raleigh's hulls lay on them).
A block's ground is the highest land under it (a shop's with its pad), and its sides go 4.5 m down (`YardSkirt`), so no
land shows through a pad and none hangs under it (the land falls 3.4 m at most under one). The pads and the apron are
paved as slabs some 10 m square, each a shade of the worn concrete (`AirGround::Paving`, about 140 luma). A ship in a
dock stands on its floor (`DockFloor`: the dry dock's ground and 0.2 m, YFD-2's pontoon at 1.5 m) and goes no lower
however she is hit. The shallows and the reef take an ink without the ground's grain (`WaterInk`).
The cloud test checks that each docked ship lies inside its dock and along it; that no hull (its outline every 5 m)
is on a block, on the land or on a pier's top, the docked ships aside; that the land under each block lies between its
skirt and its top; and that the paving's luma is 134 to 146 with a spread of 3 to 9. Its picture, with the paving:
`AIR_GROUND_DUMP=1 Tools/air_show/run.sh`, then
`STAGE=Content/Data/Stages/oahu.json python3 Tools/air_show/ground_picture.py <out>/ground-oahu.bin harbour.png -1700 -1900 900 200 1.25`.

**The battle look (the 3D builder's, `Campaign/CampaignBattleLook.h/.cpp`).** The scene calls it in five places: the
art director's grade on the scene's own camera (`Grade`, when the eye is made); the air haze, which starts 15 km out
at twice the scene's density (`AirHaze`, after the scene sets its haze, whose numbers it scales); a sky behind the
battle lit by the battle's sun, with the table's other lamps kept out of it (`AirSkyBegin`, once the key lamp faces
the sun); and `AirSkyEnd` on CONTINUE, after the hazes are put back. The two files are the builder's, byte for byte as
built in its 1732. This branch carries a copy only so that it builds alone; on any difference, the builder's own copy
wins. Its switches (`-TheaterBattleGrade=`, `-TheaterAirHaze=`, `-TheaterAirSky=`, and `-TheaterNoBattleGrade` and
`-TheaterNoAirSky` to turn them off) are described in the header.

**The sun** (the art director on build 1750's Midway 05: a hard small disc, its core 25 px or less, the glow back to the
sky's colour within about 150 px, the game-wide curve and bloom unchanged). The table's key lamp is a sun 1.5 degrees
wide, kept soft for the table's shadows, and the sky put all of its light into that disc, some two thousand times the
picture's white, which the bloom spread into a glow 700 px wide. While the battle stands (`AAirBattle::TheSun`, put
back with the table) the disc is the true sun's 0.53 degrees and holds 0.006 of the lamp's light: still near a hundred
times the white, so a hard white disc, its bloom under a hundredth of the white 150 px out. The battle's sky throws the
sun's light forward at 0.75 (Mie anisotropy; the engine's is 0.8). At 0.5, from build 1750 to 1760, the glow spread
so broad that with the sun just out of the picture it washed half the sky white (the art director on 1760's Midway
05: real haze is 0.75 to 0.8, the bright patch kept round the sun and the rest of the sky its blue-grey).
`-TheaterSun=0.53,0.006,0.75` (the disc's width in degrees, its share of the light, the haze's forward throw); the
log says "Air battle: the sun a disc ...".

**The sky.** Each battle carries its day (`sky.place.cloud`, the air thread's `sky.ts`): a cloud deck or scattered
cumulus, with one cloud base and one top. Pearl Harbor and Midway are cumulus days, as history had them: Oahu's
base 940 m and tops 2,060 m, the Mobile Force's 510 m and 1,820 m. A ticket from a rules bundle without the sky draws
cumulus at the cloud table's middle heights. `AirSky` lays the clouds once, on a 44 km square that repeats without a
seam, and draws each cloud where it lies nearest the eye. So there are clouds wherever the fight goes (at Pearl Harbor
most of the fight and the falls are 20 to 32 km short of the harbour). The scene draws them as see-through meshes of
soft puffs, in the effects kit's shape and with the same `M_Highlight`, in two rooms (`AirSky::Rooms`):

- **The near room** (`NearClouds`, 800 puffs) holds the clouds whose near edge is within 4.5 km of the eye across the
  ground, fewer when they would not fit nine tenths of it. They are turned to the eye when it has moved about a quarter
  of a degree at the nearest puff, as soon as it moves after a paint that drew nothing, and, while a drawn cloud's lift
  over high ground is still easing, for each metre the wind carries the clouds. A room with nothing in it waits.
- **The far room** (`Clouds`, 3,200 puffs: a fifth under the painter's 4,000, since the two battles draw about 2,800)
  holds the rest, and the shadows go with it. It is turned only when the eye
  has moved far enough for a far puff's turn to shift its rim by about a pixel (half a thousandth of a radian), or by a
  twenty-fifth of the nearest far puff's distance, whichever comes first; while a far cloud's lift eases, every 3 m of
  the wind's carry. Then the line between the rooms is drawn again from the eye, and both rooms are sent.
- **Why.** On the builder's Mac run of 19 November, Midway stuttered: 9 to 17 frames over 33 ms in five seconds. The
  whole field, 4,000 puffs and 132,000 points, was painted and sent 9 to 16 times a second, on the strategy camera and on
  the chase. Now the far room turns about once or twice a second and the near room, 26,400 points, as often as before.
  The cloud test flies three paths with both and checks that the far room turns a quarter as often as the whole sky did,
  or less, and that under half the points are sent. It also checks, from every test eye, that the two rooms hold
  exactly the puffs, the hearts and the shadows of the whole sky.
- **Order.** The near room is drawn just after the far one (its sort priority 5 over theirs). Every near cloud is nearer
  the eye than every far one, except for a little at the line.
- **Painted aside** (round 11, after the builder's clean Midway run on build 1758: the slowest frame's sky still 24 to
  35 ms, the far turn's paint and send of 132,000 points). The game thread asks the rooms which turn is due and hands
  the painting, and the change into the meshes' points and colours, to a pool thread, on the sky's own copy of the field
  (`SkyAside`, whose ground is a plain copy of the stage's grid, touching nothing of the engine's). The frame it is
  ready, the game thread sends it; no other turn starts while one is painting, so a turn is a frame or a few late and
  both rooms of a far turn go out together. When the eye jumps over 40 m in a frame, and over four times its last
  frame's move (a cut), the turn in hand is finished and sent and the new one painted at once, so a cut never shows
  the last shot's clouds. A glide's eased sweep is not a cut (on build 1760 it was: the flyover's glide into the
  strategy view, up to 100 m a frame, painted 170 to 230 frames at once, each battle's slowest five seconds). A step
  over 3 km is a cut whatever came before (a cut straight after a cut). The game's own glides run far faster than
  that note's 100 m (the strategy view's middle can be 70 km from the flyover's last eye: 700 m a frame), so a turn
  painted aside and shown a frame or three late was painted for an eye kilometres back, and a cloud the eye had
  since flown into could stand across the lens at full strength. So a turn painted aside is given a sweep (the
  pre-check's cure on round 12, `Painter::SetSweep`): eight frames of the eye's step along its way, and each puff
  fades by the lens (1.2 radii to 0.6) by the nearest the eye comes to it along that way. None at a cut. The
  pre-check's glides through cloud went from up to 134 frames with a disc across the lens to none. The battle's end and
  a new sky wait for the turn in hand. `-TheaterSkyAtOnce` paints every turn on the game thread, as before, to
  compare.
- **The log.** Every 5 s, `Air battle sky` gives the near and far turns, the milliseconds of a turn's painting (aside,
  or at once) and of sending on the game thread (the render thread's share of a send is not in it), the most the sky
  took of the game thread in one frame, how many turns were painted at once, how many frames found a turn still
  painting, and the line.

- **How far.** Clouds are drawn out to 20 km from the eye and thin into the haze over the last third of that. A cloud
  within 5 km is drawn with all its puffs, out to 10 km with its bigger ones, and past that with two: its veil and one
  soft lit body, which fades in as the rest fade out, so the clouds run on small toward the horizon with no empty band.
  The painter's room is 4,000 puffs (`MostPuffs`); the three battles draw about 3,100 to 3,300 at most. With no room, the farthest
  puffs are left out.
- **Cumulus** cover about three eighths of the sky: a trade-wind sky (the art director's trade-cumulus brief, 20
  November), sizes on a steep power law, about six in ten 300 to 600 m across as laid, a third 600 m to 1.5 km and one
  in seventeen 1.5 to 3.5 km (even on a log scale within each), so most of the cover comes from the few big ones; in loose streets
  along the wind about 4 km apart, narrower than the clear lanes between them, the bigger ones keeping to the streets
  and the small ones scattered in the lanes a little more. Each is a flat grey base, a lit, lumpy dome 1 to 2.5 times
  as wide as it is tall (a small one flatter all through, a rag and not a ball), a few turrets and a faint veil that
  holds it together (within the body, so it lays no pale disc on the sea; no deeper than 120 m under the base at its
  faded rim, so a flat base stays flat). Tops stop at 0.8 to 0.9 of the day's depth, one broken level under the
  inversion; one big cloud in about twenty is a tower to the day's top. Each leans 5 to 15 degrees downwind as it
  rises; its lee side is softer, thinner and smaller (ragged), its upwind side firm, and a big one has one to three
  grey fractus bits (30 to 75 m, alpha 0.3 to 0.6) at its base level downwind. They drift with the battle's wind.
  Over the target there is a clearing that stays put as they drift: a cloud carried into it thins away, and one
  carried out of it fills in, so the ships or the town stay in sight. Only small clouds stay in it (a few under 600 m,
  none of 1.5 km or more).
- **Light and shade** come from each cloud's shape: every point of a puff is lit as the point of the cloud's rounded
  body it stands for, three parts in four (a quarter from the puff's own lump), and darkened by as much of its own
  cloud as lies between it and the sun (each puff's `Shadowed`, the other lumps' depth along the way to the sun, laid
  with the cloud; up to 0.55 off): one sun on each cloud, a bright sun side and a grey-blue shade side (the brief's
  item 1). The shade side is painted bluer (150, 164, 192) and the base a little darker (122, 130, 148): through the
  grade the old 162, 170, 184 came out a neutral grey. Sunlit tops come out about 242 of brightness, down-sun sides about 165, the base from under it
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
- **Shapes** (round 18c; the art director's item 4, torn fragments in place of soft discs). Where `M_Cloud` is built
  (`Tools/build_content.py`, `build_cloud`: translucent, unlit, two-sided; run `Tools/build_content.sh` once, or
  `TC_REBUILD=M_Cloud`), each puff is one shape from a sheet of 16 the scene paints when a battle opens
  (`AirSky::MakeFragments`, 1,024 px square, about 190 ms on the pool from `Open`, waited for in `LaySky`): 10 cauliflower
  tops with torn lower edges, 4 domes (a far cloud's whole body on a flat underside) and 2 feet (a cloud's one base: a
  slab of lumps, ragged at its ends, never a pill's rounded end, fading upward into the body over it, no seam). The
  sheet holds plain numbers, the shape's light three
  ways (red: the sun to one side; green: behind the eye; blue: ahead, its thin edges bright) and its alpha; the points'
  second texture coordinate says how far the sun is behind the eye or ahead, and their colour is the cloud's light where
  they lie, from the cloud's form, none from the lump. A top is turned so its crisp side faces the sun as the sun stands
  in the picture (give or take a fifth of a right angle); a base stands upright with its underside level, all of a
  cloud's on the cloud's foot, and from under the foot each base's underside is put where the foot is in the picture
  (at most a third of the shape's height off), one line under the cloud, not a stack of saucers. A base under a taller
  body melts up into it (its dome's top and, less, its sides fade), so only its underside is crisp; a base looked down
  on fades as the eye rises over it, and a far body is a base's shape only while the eye is under the cloud's top.
  Every point is lit as the body's near face where the eye's line through it meets the body: white all over with the
  sun behind the eye, dark with the sun ahead, where the shape's bright edges show only on the cloud's own outline
  (inside it a lump's edge is a fold of the dark body); what of the cloud lies sunward darkens the faces seen less as
  the sun comes round behind the eye. A puff's rings are lit at every other point, each one between taking the mean
  of its neighbours (the face's light cost a third of a paint; a paint is back to about 3.5 ms here, and the cloud test
  times the shapes as well as the discs, both under 5 ms). Drawn from shapes a cloud has no veil (it hung by the cloud as a grey ghost seen
  from above). The item 4 mends (the art director on the round 18c sheet): a heap (a cloud whose first puff is its
  foot) draws one foot, centred under it, 0.8 of its width, and its other low puffs as round body lumps lit by their
  height, the cloud's lower third greying down to the foot; a far body drawn as a dome keeps the cloud's own height
  over width (it had been squashed into a lens, and the lenses stacked into shelves); a small cloud stands no taller
  than 0.4 of its width (its lumps floated over its base); its fractus sits on the foot, half of each bit hanging
  under it, to lee, anywhere from near the middle out. From under a cloud, or with the sun ahead, the shape's blue light is the lumps' thickness: dark at a lump's middle, lighter at its rim and in the
  seams, so the underside reads lumpy, never a flat grey cushion; with the sun ahead a big cloud's body is darker
  through its middle with its outline lit through, and a small one (a few hundred metres thick) lit through nearly all
  over. The test picture's sky is brighter toward the sun, as the game's is. A deck's tiles are drawn in shape mode too
  (only a heap's veil is left out; 18c had dropped the deck's tops). Without `M_Cloud` (or the sheet), the soft discs on `M_Highlight`, as before, with a warning in the log;
  the shadows on the sea are soft discs on `M_Highlight` always. `AIR_SKY_PICTURES=1 Tools/air_show/run.sh` draws the
  sheet (`cloud-sheet.ppm`) and eight eyes of each battle's sky, two of them low with the sun straight behind and straight
  ahead; `AIR_SKY_SUN=125,18` gives the stage's sun (Oahu 125/18, Midway 105/68), `AIR_SKY_DISCS=1` the soft discs.
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

## Bomb bays and sticks (release 1)

Brian, 21 November 05:44Z: "add working doors in release 1", "have a realistic number of bombs drop from them as well
which you see and hear drop to the ground".

- **The load** (`Show::BombsOf`): B-17 12, Lancaster 14, He 111 8, B-29 20, Betty 4, SM.79 5, Avenger 4, Il-2 4, Nell 2;
  a Kate, a dive bomber and a fighter 1. Any other model by its span: over 25 m 10, over 18 m 6, else 1.
- **The doors** (`Show::Bays`, `PlaneOut::Bay`, 0 shut to 1 open). A level bomber with a stick of two or more opens
  its doors over 2.5 s (`BayOpenS`, eased at both ends: the art director, 06:48Z, motors, never a snap) on the last
  10 air seconds of its run (`BayLead`): a sea raid's view carries the run, and a land
  raid's box is timed from when it comes inside the target ring (`TargetR`, the rules' run is 45 air s there). The
  bombs leave the bay one after another, 0.15 s apart (`StickGap`), from the hinge line, moving as she moves. Doors
  held open 3 s after the last bomb, then shut over 2.5 s (`BayShutS`), eased. A jettisoned stick opens the doors at once and lets go as
  they come open. A dive bomber, a torpedo and a fighter's bomb go at once, with no doors.
- **The model** (the modeller's batch, `battle-models/game-bay`): B17, Lancaster, He111 and Bomber carry `near.part`
  and `parts` (bay.port, bay.starboard: kind turn, pivot, axis, limits 0 to 85; bombs: the racked load). `TakeBay`
  lifts the doors and the load out of the near shape (sections 2, 3 and 4 of the aircraft's mesh); `Draw` turns each
  door about its hinge by `Bay` x 80 degrees when it has moved, and hides the racked load as the stick's last bomb
  leaves, so that each bomb seems to come out of it. The far shape has no bay: a bomber on its near shape stays on it
  while its doors are open. As her load goes she lifts 0.4 m over about a second (`StickLift`) and is trimmed down
  over the next six. A model without parts is drawn as before.
- **The sound.** The rules' cue for a drop (bomb_stick_01 at the moment it is let go) is no longer played there.
  Each stick's first bomb, as it lands, plays bomb_stick_01 from 0.85 s into the take (its first blast), so the take's
  blasts walk on as the stick lands; one stick take at a time, 1.5 s apart, unless a stick lands at half the distance.
  A bomb alone (a dive bomber's, a Kate's), and any bomb within 150 m, a crump of its own (mix impacts near within
  60 m, middle within 150 m, far past). All after their flash by distance / 343 s (none within 150 m, at most 3 s).
  A falling bomb whose landing is within 400 m of the eye gives its rush, bomb_release (1.76 s, ending at the
  landing; at most one each 0.7 air s), only at the battle's own x1 pace.

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
   time, `Placing::MostPuffs` sets their room and `Painter(12)` the sides of each puff. Midway should no longer
   stutter: the `Air battle sky` log line every 5 s gives each room's turns (near 8 to 15 a second, far about 1 to 2)
   and the game thread's milliseconds. If the far clouds seem to swim or pop as the eye moves, the far room's measure
   is `Rooms`' third and fourth numbers (0.0005 and 0.04): smaller turns it more often. The sky's painting is now off
   the game thread; `-TheaterSkyAtOnce` puts it back, to compare the log's slowest frame.
5. **Pearl Harbor's ground.** The grass mottled with patches of red earth, the beach a band along the water, the
   shallows darkening into the sea, and the yard's grey blocks with Pennsylvania standing on her dry dock's floor and
   Shaw in YFD-2 off the beach. No pad's edge floating over the land or sunk under it; the paving in slabs, not a flat
   card. If the grain shows as a pattern, `-TheaterGroundGrain=0.15` softens it.
6. **The sound.** Each formation's drone, the bursts by calibre, the flak bursts heard after their flash, and a fighter group's pass within 300 m. The loudness and distances come from mix.json's families.
7. **The bomb bays.** Bremen, the camera near a B-17 box on its run: about 10 s before the bombs, both doors swing
   slowly down (2.5 s, eased) from the belly's keel to hang nearly straight down (80 degrees); twelve bombs leave
   each bomber one after another, each leaving with her speed, dropping away beneath her and nosing down over its
   first 2 to 3 s; the racked load inside gone with the last; she lifts a little; the doors shut slowly about 3 s
   after the last. If a door turns up into the
   fuselage instead of down, the sign of the turn is wrong: negate the angle in `AirParts::TurnDoor`'s call in `Draw`.
   If a door swings about the wrong line, the pivot's mapping in `TakeBay` (the file's x forward, y right; the
   shape's nose to -X, right hand at -Y) is off. Then the stick landing: one rolling stick of blasts as it walks
   across the ground, after its flash by the distance.
