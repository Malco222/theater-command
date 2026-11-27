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
| `AirBattleMarks.cpp` | The strikes on the aircraft (Brian, 22 November): each of the core's hits kept on the aircraft it struck, in its own frame, and laid on its skin as a decal (`M_HitMark`) while it is near the eye. |
| `AirMarks.h/.cpp` | What stands on the ground under a raid on troops, plain C++ (release 2, item 51): the column (`place.marks`), each piece's footprint, a bomb's blast, where a counted hit's bomb comes down on its piece and every other bomb clear of them all, what a hit makes of a piece, a strafing lane and the pass flown down it, the column's light flak's bursts. |
| `AirShowTroops.cpp` | The flight core's half of a raid on troops (item 51): each `air:mark-hit` held until a drawn bomb or a strafing pass's rounds reach its piece and shown then (`MarkHit`), the bombs steered onto or off the pieces, the strafing pass flown (`air:strafe`), the column's flak (`MarkGun`). Plain C++. |
| `AirBattleTroops.cpp` | The scene's half (item 51): the column drawn on the land as copies of its army's models, its fires, wrecks and scattered men, its flak in the effects kit; `AAirBattle::CampaignLiveOn(Kind)`. |
| `AirBattle.h/.cpp` | `AAirBattle`, the scene. It feeds the core and draws what the core says: the planes, the small things, the effects kit, the stage and its ships, the cameras and the sound. |
| `AirBattleScreen.h/.cpp` | `SAirBattleScreen`, the screen. It draws the desk's words and takes the mouse and keys. |
| `AirStage.h/.cpp` | The Oahu and Midway stages (an earlier commit on this branch). |
| `SeaFace.h/.cpp` | The sea's face, plain C++ (the cloud test makes the same): the shore picture (where the water lies shut in, the trade wind's fetch, how open it lies, how far from the edge) and the whitecaps' picture the moving sea reads. |
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
   - the chase view (`05c-chase-view`): his first fighter squadron ridden as a double click takes it, then one step
     along it, the plate reading its second aircraft
   - the ships (the overview cut to the first ship that went down)
   - the guns fire back (`06b-guns-fire-back`, release 2's item 14): 2.5 s after a ship's light guns are first laid
     on an attacker within 1.6 km of her (a battleship or a carrier where the battle has one; at Pearl Harbor the
     first such run fell on a destroyer in a nest), the camera on her, side-on to the line from her to him, the look at
     two fifths of the way from her to him and 4 degrees down: she low in the frame, he high against the sky, her
     streams and the heavy bursts between. The log says "Air battle: guns fire back picture on <ship>". The cloud test
     finds the same moment (Pearl Harbor: Nevada under a Val's dive at 7:03 of the battle; Midway: Kaga under a
     Dauntless's at 6:59). Every other picture due meanwhile waits until it is taken, as one waits for the ships.
   - the fight going on
   - the strike marks (`09-strike-marks`, item 19): 105 s after the meeting, riding the aircraft flying that has been
     struck the most (a cannon shell weighed as three rounds, a heavy shell as six). The log says "Air battle: strike
     marks picture on <model> (key N): ... strikes on it"
   - an ace's marks, when the battle has an ace or `-TheaterAceTest=` gives one (item 44; the art director's two frames):
     `12-ace-close`, 30 s after the meeting, the eye 8 m off his record (his name and first row of marks; a German's
     name and rudder bars), square to it, two metres above and a metre behind; and `12b-ace-chase`, 36 s after, the chase view behind him. The log says "Air battle: ace picture
     ... on key N"; with no ace flying, neither is taken and the log says so
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
  keep their blades standing still (with the Battle rules card's Turning airscrews on, item 61, they do not). A blade is every paint whose name in the file's `paint_names` begins
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
  low haze (none below 1,000 m, all of it at 7,500 m). Round 18d (the art director, 23:06Z, on build 1786's 1.3: the
  zenith still read 41, 56, 93 against her 50, 100, 150): strength 6, a greener blue (12, 110, 255), a third weaker on
  the sun's side of the sky. "-TheaterZenith=6,12" tries another strength and lowest angle; 0 turns it off. The
  picture run tries 4, 6 and 9.
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
- A double click on any squadron's tag, his or the enemy's, rides behind its leader (the chase view). In the close
  view a click on a tag rides that squadron; left and right arrows step from aircraft to aircraft along it, up and
  down from squadron to squadron (the plate's buttons do the same); Esc goes back to the strategy view.
- Space holds the clock or lets it run. Keys 1 to 4 set the speed.
- Esc answers KEEP FIGHTING (with no question standing, it leaves the close view). Enter presses CONTINUE.

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
- **The chase view** (release 2; Brian, 21 November 05:52Z: "click on the squadron and then it goes to a view of one
  of the planes from behind so you can watch it. you can then move from one plane to the next if you want to see other
  planes"). A double click on a squadron's tag rides behind its leader, in the close view's camera above; the
  squadron is then his choice, kept to until he steps (no ten-second move to the fight). Its line is its aircraft
  still flying and shown, leader first and on down the formation (`AirDesk::RideLine`); left and right step along it
  and round the end (`RideStep`), up and down step to the last or next squadron's leader, his own squadrons first and
  then the enemy's (`RideSquadrons`, `RideSquadronStep`). When the aircraft ridden goes down the view cuts to the next
  of its squadron still flying, never back to the leader; with none left the view picks the fight again. A plate at
  the top right says RIDING WITH, the squadron's name (the enemy's with its country word), "Aircraft 3 of 12" and
  what it is about (wavering, heading home), with four buttons (‹ AIRCRAFT, AIRCRAFT ›, ‹ SQUADRON, SQUADRON ›);
  tags keep clear of it. While the view picks its own, the plate says THE VIEW FOLLOWS THE FIGHT. It is a view, not
  an order, so it works in a recorded battle too. The cloud test steps every squadron's line round both ways each
  second and checks a fall steps on (on Oahu, Midway and Bremen: 6,546 lines, 71,155 aircraft, 3,622 falls, 0 wrong).
- **The tug-of-war bar** (release 2; Brian, 21 November 05:55Z: "have each sides flag on either end of the bar and
  then have it be like a tug of war in real time so you can see which side is winning the raid/engagement", and
  14:58Z: "Can we put the country flags instead of country names"; the air thread's rule and the UI director's
  ruling of 14:47Z, `/mnt/project-files/ui-director/tug-of-war-bar.md`: "the battle bar is the release 1 card's bar,
  moving"). It takes the place of the strength fill and its start mark at the top middle; HOW NEAR TO BREAKING OFF
  stays under it. His flag at the left end, the enemy's at the right (the table's own, `CampaignFlags`, as the
  release 1 card draws them: 27 by 18, 8 clear of the bar; a plain chip where no table is up). His colour, a brass
  gold (C9A24A), runs from his flag and the enemy's slate (6F7B88) from theirs; they meet at the knot with the
  card's 3-pixel gap, so the side that is winning holds more of the bar, like a front line pushed toward the loser's
  flag. The colours never swap and neither is red. A pale mark at the middle; no ticks, no handle, no words.
  The knot (`AirDesk::KnotOf`) is the running form of the result the battle is judged by: x is the view's
  `sky.score.ratio` (aircraft the defender lost + what the bombs were worth + 10, over aircraft the raider lost + 10;
  1 or more a win), the knot x / (1 + x) from the raider's flag, and a defending player's the mirror (1 / x). Level
  until the first loss; it follows the views as the battle shown reaches them, eased with about 1.5 s of lag on the
  screen's clock (`KnotEase`); once the battle is over it lies at least 0.05 past the middle on the side the stamp
  names (a side that held the field won whatever the exchange said). On the recordings it ends at 0.45 on Bremen (a
  close defeat), 0.91 at Midway and 0.90 at Pearl Harbor. Until the flight core counts the losses the picture has
  shown (the air thread, after the gunnery), it can move up to a fall or two before the fall is seen. The cloud test
  checks it level before the first loss, eased, the other side's the exact mirror, and on the winner's side at the end.

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

**Each day's weather (round 18d; Brian, 21:53Z: "make sure the weather is different and not always cloudy").** The
recording carries each battle's sky from the rules' weather (kind, base, top and cover in eighths), and the scene lays
what it says:
- **Fair, scattered and heavy cumulus differ in character, not only in amount** (the art director, 22:37Z). A fair
  day (cover about 0.2) is small, flat, even clouds, low and few in streets; a scattered day (0.39) is as before,
  drawn from the same numbers so it does not change; a heavy day (0.55 and over) is bigger towers in clusters with wide
  gaps between, darker bases. A clear day has no low cloud.
- **Cirrus and rain** (`LayCirrus`, `LayRainShafts`). A clear day has thin cirrus 8 to 10 km up: bundles of soft
  threads along the wind aloft, fanning out and hooking at their downwind ends, round the target out to 45 km (a fair
  day a little of it, none from 0.32 of cover up). A heavy day (cover 0.45 to 0.55 and over) has up to three grey rain
  shafts under its biggest clouds, 7 km and more from the target and 5 km apart: four crossed soft sheets from inside
  the base to a mist at the ground, lagging upwind as they fall, drifting with their clouds. Neither is drawn in the
  software picture. `-TheaterCirrus=1`, `-TheaterRain=1` ask for them whatever the day; `-TheaterNoCirrus`,
  `-TheaterNoRain` turn them off.
- **The winter deck** (Bremen; the art director, 21:48Z): one lid about 1.8 to 2 km up, its tiles sunk under the top
  so nothing pokes through but the odd cell 100 to 200 m proud; bumps five times wider than tall; finer toward the
  horizon. The lid's underside is unchanged, so its clearance over high ground holds; a base puff hangs at most 170 m
  under its tile's foot.
- **The deck's shade on the ground and sea.** `AirSky::ShadeOf` works out, over the laid square, how much sun each
  point under the deck gets (a 256-square picture: about 0.16 under a tile, full sun through a hole, blurred at the
  edges), read along the sun's ray. The key light carries it as a light function (`M_DeckShade`, from
  `build_content.py`'s `build_deck_shade`; run `TC_REBUILD=M_DeckShade Tools/build_content.sh` once), so the shadow
  falls about 5 km down-sun at Bremen's 13-degree sun, and fades out over the deck's top so aircraft over it stay in
  full sun. The picture repeats past the square (the deck's tiles do too). `-TheaterNoDeckShade` turns it off. Without
  `M_DeckShade` the key light is as before, with a warning in the log.
- **The far deck.** Past 14 km a flat sheet at the deck's middle, out to 600 km, carries the deck to the horizon as a
  smooth pale sheet (from below, its base grey; from over it, its sunlit top). `-TheaterDeckSheet=` sets how solid it
  is (0.4 to 1 by cover); `-TheaterNoDeckSheet` turns it off.
- **Light bounced off the deck.** Over a deck in sun, the aircraft's bellies take light from below off its sunlit top
  (the sky light's lower half, recaptured only when it changes); under it, almost none. `-TheaterBounce=1` scales it (release 2: a sunlit deck under a winter sun throws three times what it did).
- **The sun through the air by height** (release 2; the olive drab read mustard, about 20 degrees of hue warm, on the
  Mac's check). The engine lights every surface with the sun as it reaches the sea, through the whole air: at Bremen's
  13 degrees that is about 1.45 red over green and 0.6 blue over green (the air's scatter and its ozone), which turns a
  green-brown drab mustard; the white star clips, so it hides the cast. The key light now works out its light through
  the air above each point (`bPerPixelAtmosphereTransmittance`), so a bomber at 7.5 km takes about 1.25 and 0.9, and the
  ground keeps its own. `-TheaterAirByHeight=0` lights everything as at sea level, as before. The log line after "the
  sun a disc" says which.
- **The haze over the sea** (release 2; the Mac's check: a white wall, the sea unreadable). Over a stage at sea the
  haze is pale blue (`-TheaterSeaHazeInk=0.44,0.55,0.74`, blue over red by about +15 on the frame) and starts 30 km
  out at 1.2 times the scene's density, so it whitens only at the horizon; over land (Bremen) it stays as it was
  (7.5 km, 2.6 times, its own ink).
- **Land raids' light** (the art director, 23:31Z, on build 1786's tan ground): over land with no stage (Bremen), the
  sun is 6,500 K, not 4,900, and the grade's warm gain is taken off (red and blue gain 1, in sun and under a deck), so
  the sunlit ground reads grey-green (her target: saturation 0.10 to 0.18, blue under red by 5 to 12) and the town
  grey-brown, never orange. `-TheaterKeyWarmth=4900` and `-TheaterLandGrade=red,blue` try others. Pearl Harbor and
  Midway keep their 4,900 K.

## The ships fire back (release 2)

Brian, 21 November 18:06Z: "for the ships being attacked by planes, they can fire machine gun fire and flak guns at
them just like real life". The historian's research is `/mnt/project-files/naval-battles/ship-air-defence.md` (part B
is for the scenes). The battle still decides every loss; the guns only show the fire those losses and misses came from.

- **`ShipGuns.h/.cpp`** (plain C++, shared with the sea battle scene). It holds each ship's anti-aircraft guns as her
  navy fitted them that month (`BatteryOf`: USA, Japan, Britain and the Commonwealth, Germany, Italy, the USSR,
  France, and any other navy's usual small fit, by class bb, cv, ca, dd, ss, ag). The guns change with the war: the
  American .50 calibre gives way to the 20 mm through 1942, the 40 mm comes from late 1942, and the Japanese 25 mm
  doubles and doubles again from mid 1943. The Midway carriers have their own fits. `Gunnery::Fire` lays each crew on a
  plane every frame. The heavy guns (5-inch, 12.7 cm, 4-inch, 10.5 cm and their like) are directed: time-fuzed
  bursts where the plane was going to be, mostly behind it or off to the side, at most four rounds shown a second
  (the nearest ships first). Against a torpedo plane low over the sea, they fire into the water ahead of it, and so
  do the destroyers' and battleships' low-angle main guns. The light guns (.50 calibre, 20 and 25 mm, 1.1-inch,
  40 mm, pom-pom) fire tracer streams from mounts along her decks. The streams come in bursts with their drums and
  clips changed, hose about the plane and walk across it now and then (about 1 frame in 100 on it). A few rounds are
  seen striking. The ships hold fire on a plane their own fighters are going in on.
- **The kills.** A plane the battle says the ships' guns brought down falls under the nearest ship's light guns
  (their streams go on it and a few rounds strike home as its fall begins), or, past them, to one burst of her
  heavy guns. With no ship near, it keeps the one burst it had.
- **The scene.** Streams are the kit's `AirGun` (new rows `air_guns.aa25` and `aa40`: a tracer in three for the
  20 to 25 mm; every round a big tracer, a heavy flash and a puff of smoke for the 40 mm, at about 120 a minute). Heavy
  rounds are `FxKind::ShipGun`: a flash and smoke at her deck (new muzzle rows `ship_aa` and `ship_main`, with no
  dust ring over the sea), the flak firing take after its flash by distance, then the shell's burst (`AirFlak`,
  heavy) or a splash. A ship's light guns of one kind are heard as one battery (the cannon take for 20 to 40 mm, the
  hand-held heavy take for the machine guns). Tracers are in her navy's colours.
- **Proximity fuzes.** `Hull.bProxFuze` hugs the bursts to the plane (part B2). It stays off until the rules give a
  side the `proxfuze` research.
- The cloud test checks every stream is from her own decks and in reach, at most 1 frame in 5 on the plane, some
  hits and some bursts, and nothing with no ships (Bremen). It prints each ship's guns and the guns that fired.

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
  held open 1.5 s after the last bomb (`StickSteady`), then shut over 2.5 s (`BayShutS`), eased, as she rolls into
  the rally turn. A jettisoned stick opens the doors at once and lets go as they come open. A dive bomber, a torpedo
  and a fighter's bomb go at once, with no doors.
- **The run held steady** (release 2; the coordinator, 21 November 13:57Z, from the films of the cut: the bomber
  banked 50 to 60 degrees as the first bomb left; the art director's figures, 14:01Z). A level bomber with a stick
  holds her heading at the drop (`RunHdg`), wings level, until 1.5 s after her last bomb: no bank past 2 degrees
  (`RunBankMost`) from her doors opening to then, in her slot or straggling. Then the box's rally turn: a
  four-engined bomber (two engines or more, or a span of 20 m or more) in her slot banks at most 20 degrees
  (`RallyBankMost`), rolling in no faster than 5 degrees a second (`HeavyRollMost`); out of her slot, 30 at most
  (`HeavyBankMost`). A falling bomber is not held. The flight core's check: "a stick let go wings level, a bomber's
  turns gentle" (at most 3 degrees on the run, 30 ever).
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

## The moving sea (release 2)

Brian, 21 November 05:53Z: "make the water more detailed in the pearl harbor air battle scene".

- **M_StageSea** (`Tools/build_content.py`, `build_stage_sea`; it is new, so `Tools/build_content.sh` makes it on its
  next run). The stage's sea, and the painted shallows and reef over it, keep their colours (the deep paint, its long
  patches drawn out along the trade wind, the shallows' paint); over them the water's surface moves. The ripples
  picture the campaign map's water uses (`CampaignTextures::MakeWaterBumps`, its slopes at four sizes) is read six
  times: a swell (320 and 410 m pictures, slopes from about 50 m down), a chop (64 and 80 m, from about 10 m) and
  ripples (13 and 17 m, from about 2 m), each pair at two headings off the wind (0 and 14, 25 and -18, -40 and 55
  degrees), all travelling downwind at the speed of their waves (8 to 9, about 4, about 2 m/s). The slopes are summed
  in the world's frame over the mesh's own (the patches' tilt stays). Gloss 0.07 near, 0.22 past about 6 km, where the
  waves are finer than a pixel and fold into a broader sun path (`FarDepth`, `FarLean`).
- **Gusts** (cat's-paws and slicks; the art director's notes on v2, 07:20Z): the waves picture the campaign map's sea
  uses (`CampaignTextures::MakeWaves`, its slow unevenness, alpha) laid 2.4 km to a tile across the wind and four
  times that along it (patches drawn out 3 to 5 times along the wind), riding it at 7.5 m/s. In a gust the chop and
  ripples lean up to 1.4 times as much, the water is up to 1.35 times as rough, its paint 0.88 and its specular 0.4;
  in a slick they lean 0.45 times, the water is glassy (0.3 times as rough), its paint 1.12 and specular 0.7. Off the
  sun's path a slick is lighter than a gust by about 6 levels at 4 km up (the stand-in); on it the roughness breaks
  the path into brighter and darker patches (`SlickRough`, `GustRough`, `SlickLight`, `GustDark`, `SlickSpecular`,
  `GustSpecular`). Their edges are broken into fingers some 50 to 200 m across (`GustFingers`, the same unevenness
  600 m to a tile, riding with them; the art director on v3, 08:07Z).
- **The shore picture** (`SeaFace.h/.cpp`, plain C++; `Tools/air_show/sea_face_test.cpp` checks it): worked out when
  the stage is raised (about 0.5 s on Oahu, 0.8 s on Midway) from the stage's land, reef and works, 1024 pixels a
  side over their bounds and 10 km of sea round them. Red how far the water lies from their edge; green how much of
  the whitecaps the trade wind's fetch over open water raises there (under a lee shore few for 2 to 3 km, all past
  about 6 to 10 km); blue how open the water lies to the ocean; alpha how closely land and reef shut it in (Pearl
  Harbor's lochs, Midway's lagoon). A battle fought past the land's reach (Midway's carriers) has the open sea.
- **Whitecaps** (`SeaFace::MakeFoam`, 512 pixels over 160 m, up to 300 whitecaps in rows across the wind about 20 m
  apart, never overlapping, their edges feathered over half a metre): each a short streak across the wind (most
  under 2 m, the largest 3 to 5 m, 2 to 4 times as long as it is deep, `HeadColour` 0.79/0.81/0.82) whose crest runs
  down its length over 1.8 s, only the crest's own line solid white and the foam behind it lacy, with holes
  (`HeadHold` 0.1 s, `HeadLace` 0.08 s, fading over `HeadFade` 1.2 s), and behind that a see-through, mottled,
  faintly aqua tail 2 to 4 times as long (`TailColour` 0.50/0.62/0.62, half see-through at first) that is gone 3 to
  6 s after it. (Two whitecaps that touched in the picture lit a thin white seam where a read between pixels mixed
  their two times, the art director on v3; so they are kept two pixels apart, and each one's time is carried a metre
  out round it.) The tail lies upwind of the head: the crest outruns the foam it leaves (the art director agreed). Read twice (160 and 190 m to a tile, 13 and 17 s cycles), put out of step from place to
  place (`FoamShift`) so the tiles' repeats do not break together. More of them in the gusts, none in a slick, none
  where the water is shut in, few in a lee. From 400 m out they become specks (`FoamNear`, `FoamSpan`): only a few
  break (`FarCount`), each as bright as the share of its pixel it covers (`FarShare`), so they are bright by size;
  none past 4 to 10 km (`FoamFadeFrom`, `FoamFadeSpan`). In the stand-in: low down, 2.7% of the sea more than 10
  levels lighter for the whitecaps and 1.1% more than 30 (v3 as passed: 2.8% and 1.1%); 27 to 40 specks to 10,000
  pixels at 1,500 m (v2's 25 to 31), most about 10 levels over the sea and the biggest 60 to 80.
- **Calm and colour by the shore**: inside a harbour, a loch or the lagoon no swell, the chop at 0.3 (`CalmChop`)
  and no whitecaps, the cat's-paws and ripples stay; its water greener and greyer away from its edge
  (`HarbourColour` 0.06/0.11/0.075, `HarbourShare` 0.85). Under a lee shore the swell falls to 0.3 (`LeeSwell`) with
  the fetch. On a reef flat, within about 300 m of its edge, the chop keeps more than half (`ReefChop`), and a reef
  flat or lagoon is a clearer, deeper turquoise-green over the stage's shallows paint (`ReefColour`
  0.021/0.30/0.26, `ReefShare` 0.6) with darker coral heads some tens of metres across (`CoralRead`, the waves
  picture's slow unevenness 900 m to a tile, still; `CoralDark` 0.45). (The art director on v3: Ewa's reef flat
  read as pale ice.)
- **The surf**: on a reef or shore open to the ocean, a band about 90 m wide over its edge (more on the reef than off
  it), broken into sections some hundreds of metres long that come and go (`SurfRead`: the waves picture's slow
  unevenness and swell, 520 m to a tile, coming in at 2 m/s), white as the whitecaps' heads; inside the break its
  whitewater, lacy, fading over the next 60 m toward the shore (`Wash` 0.55), so the line reads from 1,500 m.
- `AAirStage::MovingSea` sets it up; without the material the sea is drawn still as before, with a warning in the log.
  `-TheaterSeaWaves=` scales the waves' lean, the gusts and the whitecaps (0 still, 1 as made, 1.5 rougher);
  `-TheaterFlatSea` uses the old ink.
- The software stand-in of it (not the engine): `/mnt/project-files/air-battles/scene/round-water/sea-preview.png`
  and `sea-shore.png` (the lochs, the south shore's reef, Midway, whitecaps close).

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

Under ruling 217 there is no campaign-screen button until the playtester passes the battle. Release 1.6 adds that button, switched off (item 48).

## Packaging

Stage `Content/Data` (AirBattles, Stages and Sound) with the game, as `Content/Effects` is staged. Then `Sounds/Real/credits.json` for the credits page.

## On the first build, look at

1. **Bank direction.** A plane turning right should have its right wing down. If every bank reads backwards, negate `P.Bank` in `AAirBattle::Draw` and in the close camera.
2. **Nose direction and size.** Each model is built nose toward -X and turned half round before its heading. (On build 1720 the close view's aircraft were far and nearly level with the eye, and read as seen from ahead; the stars on the P-40's wings show it was from behind. The close view now looks down on its back from 22 m.) The carrier aircraft (Kate, Val, Dauntless, Devastator, Wildcat) use the sculpted models where `Content/Models` has them; so do the Hellcat, the Me 262 and the C.202 wherever a battle names them (by those names, or "F6F", "Me 262", "C.202", "MC.202"). The Me 262 has no airscrew (the airscrew test passes a machine whose every engine is a jet when none is found). Otherwise each is the nation's plain fighter or bomber, scaled to the battle's span, in its navy's paint.
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
7. **Round 18d's weather.** Bremen: the deck's shadow on the town and fields, about 5 km down-sun of the tiles, its
   holes' sunlit patches; the deck melting into a pale sheet at the horizon, no bright band under its far edge; the
   bombers' bellies lit a little from the deck below. The `Air battle` log says if `M_DeckShade` is missing. A clear
   day (`-TheaterWeather=0.05`): faint cirrus streaks high up, no low cloud. A heavy day (`-TheaterWeather=0.55`):
   towers in clusters, and two or three grey rain shafts off to the sides.
8. **The bomb bays.** Bremen, the camera near a B-17 box on its run: about 10 s before the bombs, both doors swing
   slowly down (2.5 s, eased) from the belly's keel to hang nearly straight down (80 degrees); twelve bombs leave
   each bomber one after another, each leaving with her speed, dropping away beneath her and nosing down over its
   first 2 to 3 s; the racked load inside gone with the last; she lifts a little; her wings stay level from the
   doors opening until about 1.5 s after the last bomb, then the doors start to shut slowly as she rolls gently into
   the rally turn (no more than about 20 degrees, rolling in over 4 s or so; never past 30). Each bomb leaves from
   inside the bay, between the doors (release 2's Mac check: they fell from the wing), and no bomber banks or twitches
   from the doors opening to the last bomb gone (the cloud test's "a stick let go wings level"). If a door turns up into
   the fuselage instead of down, the sign of the turn is wrong: negate the angle in `AirParts::TurnDoor`'s call in
   `Draw`. If a door swings about the wrong line, the pivot's mapping in
   `TakeBay` (the file's x forward, y right; the shape's nose to -X, right hand at -Y) is off. Then the stick landing:
   one rolling stick of blasts as it walks across the ground, after its flash by the distance.
9. **The moving sea** (release 2). Pearl Harbor from the close view and from the flyover: the harbour and the sea
   outside it should glitter where the sun's path lies on them, the glints moving downwind, small waves over larger
   ones, nothing seen to slide or to repeat in a grid; far off a soft broad sun path, no shimmer. Toward the sun from
   the flyover, the path broken into brighter and darker patches drawn out along the wind that drift downwind (the
   gusts); off it, the slicks a little lighter than the gusts. Low down, on the open sea, short white streaks across
   the wind breaking for a second or two with faint mottled foam left behind them, more in the gusts; from 1,500 m a
   sparse scatter of specks, none pure white; none in the lochs or Midway's lagoon, few within 2 to 3 km of Oahu's
   south shore; a broken white surf line along the reef off Ewa and round Midway's rim, whitewater inside it, the
   reef flats turquoise-green with darker coral heads. Close (40 m): the foam's edges soft, no thin white rims, the
   heads a narrow bright line with lacy foam behind (the art director's checks on v3). The `Stage:` log line gives
   the shore picture's time (about 0.5 s on Oahu); if whitecaps show in the lochs or the surf lies off the reef, the
   shore picture's placing is off: it is now carried on the sea mesh's first texture coordinate (`AAirStage::ShoreAt`,
   worked out on the CPU in doubles), as the world position on the GPU misplaced it on release 2's Mac check (whitecaps
   inside Pearl Harbor, the harbour no greener). Far off, a whitecap flashes white only at its head, never as a slab. If the waves read as too rough or too calm,
   `-TheaterSeaWaves=0.6` or `1.4` (it scales the whitecaps, the surf and the gusts too; 0 is a still sheet). If the
   glitter crawls the wrong way, the reads' heading in `MovingSea` is turned (the waves should run 070 to 250, as the
   patches do). The `Stage:` log line says if `M_StageSea` is missing.
10. **The chase view** (release 2). In any battle, double click a tag: the picture cuts (no swoop) to behind that
   squadron's leader, the plate at the top right with its name and "Aircraft 1 of N". Right arrow: a cut to the next
   aircraft, "Aircraft 2 of N"; left: back. Down and up: the next squadron's leader, his own first, then the enemy's.
   Hold right: the steps go round the squadron and back to the leader. Let one ridden go down: the next of its
   squadron, never back to the leader. Esc: the strategy view. The ‹ and › signs on the buttons come from Courier
   Prime; if they show as boxes the screen's font lacks them. If arrows turn the camera instead, something before the
   screen took the keys (the screen must have the keyboard focus, as for H and C). The picture run's
   `05c-chase-view` shows the plate; tags must not sit on it.
11. **The undersides** (release 2; the art director, 21 November 14:01Z). Bremen, a B-17 seen from below against the
   sky: her belly should read 60 to 90 (of 255) on the frame, a neutral grey with a little sky blue in it, not near
   black (the release 1 cut measures 9 to 19). Round 18d lifts the bellies with the light from the deck below (item
   7); if they still read under 60 on this base, the belly's fill from the sky is short, not the paint. Release 2's
   Mac check read 38 to 41: the sea or deck's bounce is now three times as strong, more with the sun higher
   (`WeatherAt`'s bounce; `-TheaterBounce=` scales it again).
12. **The olive drab** (release 2; the art director, 14:01Z). The same B-17's sunlit upper side should read as a
   green-brown drab, not mustard: saturation 0.15 to 0.25, red over blue by 8 to 20 (the cut measures about 110, 100,
   60, saturation 0.46). If it reads mustard, test in turn: the paint's base colour (the model's drab swatch), the
   sun's warmth on this day's weather, and the grade (the battle's grade may be missing on this base). The game-wide
   curve and bloom stay as they are. Release 2's Mac check found the warmth the sun's, through the whole air: the key
   light now takes the air above each point (`-TheaterAirByHeight=1`, "The sun through the air by height" above); if
   it still reads warm with that on, the model's drab swatch goes to the planes thread (about #4F583E).
13. **The tug-of-war bar** (release 2). Any battle, the top middle: his flag at the left end and the enemy's at the
   right, each about a line of text high, the same flags as on the battle card; gold from his flag, slate from
   theirs, meeting in the middle at the start with a small dark gap and a pale mark at the middle. As the first
   aircraft go down the meeting point slides, smoothly, never a jump; on Bremen it drifts toward his flag (the
   Germans winning) and at Midway and Pearl Harbor it swings well toward the enemy's flag once the bombs land; when
   the stamp comes it sits on the winner's side. No old strength fill, no start mark, no words beside the bar. If a
   flag shows as a dark chip, the table was not found (`AAirBattle::FlagPicture`); if the flags read squashed, the
   brush's size (96 by 64) is off for the picture the table paints.
14. **The ships fire back** (release 2). Pearl Harbor, the camera on Battleship Row once the dive bombers come down:
   short red-orange streams of .50 calibre and the 1.1-inch climbing from the ships' decks (never from the water or
   the land beside them), in bursts with gaps, hosing about the planes and nearly all passing behind them; black
   bursts of the 5-inch higher up, mostly behind and off to the side of the formations; a flash and a puff of smoke
   at a ship's side for each heavy round, the boom after it by distance. Midway, near Akagi or Hiryu as the
   Dauntlesses dive: the Japanese 25 mm in short bursts, its tracer pale yellow-white (Japan's colours in the kit), the
   12.7 cm bursts among the dive bombers. Now and then a few sparks on a plane; a plane the ships bring down starts
   its fall under their streams. If a stream starts off a ship's deck, its mounts are laid by `Hull.Long` and `Beam`
   (the stage's hull sizes, `AirFeed::ReadView`). The cloud test's `ships' guns` lines list what each ship carries.
   The picture run's `06b-guns-fire-back` is this item's frame (release 2's r2d run had none: its ships picture came
   after the raid, or on a carrier already ablaze).
15. **The screen in the close view** (release 2's Mac check: the dispatches covered the ridden aircraft in Bremen,
   Midway and Oahu 03 and 05, a tag sat on it in Oahu 05). Riding any aircraft, no dispatch line and no tag over it:
   the dispatches broken to the room left of it, else laid at the bottom right, else raised above it; the tags stepped
   off it. Their plates darker (the oldest line still reads over white cloud), the tags' and the chase plate's count
   lines in paper, no tags under the after card, and no line said twice while it is on the screen.
16. **The quays** (release 2's Mac check: Pearl Harbor's quays and walls flat tan cards). From 1 to 2 km over the navy
   yard and Ford Island, the quays' and piers' tops paved in slabs about 10 m square, each a little lighter or
   darker, with the yard's patching and stains, the slabs running along each quay (the cloud test's ground line
   counts them, and checks no crack opens between them).
17. **The cuts** (the camera and film director's note, 22 November). At each cut (the flyover's shots, a picture, a
   ride from one aircraft to the next, the jump of the strategy camera, and back to the table) cloud, flak smoke and
   the light start clean on the new shot: no smear or pumping for a frame or a few. The film opens on the flyover,
   with no frame drawn from sea level at the stage's middle first. Each cut calls SetGameCameraCutThisFrame (AirParts::
   TellCut: DrawSky's cut, the close view's cut, the view taken and given back); the eye is aimed once before it
   becomes the view.
18. **Smoke goes into the air** (Brian, 21 November 18:00Z: smoke that fades gradually; the effects kit's 2a200875).
   A flak burst stays dark to about 8 s, then greys, widens and thins, gone by 25 s; an aircraft's explosion cloud is
   gone by 30 s, a hit's puff by 6 s. As it thins it pales into the sky behind it, not into a mid grey: the log says
   "Air battle: the air's smoke pales into the sky behind it, on screen 202, 209, 221" (over the sea; 219, 224, 230
   over Bremen; greyer under a deck). If old smoke reads as a darker or lighter blot against the sky, measure the sky
   behind it and try "-TheaterSmokeSky=r,g,b" (0 to 255) with that measure (AirParts::SkyBehind).
19. **Bullet holes and damage** (Brian, 22 November 04:12Z: "if a plane gets hit make sure the bullet holes and damage
   shows on the wing or fuselage"). Rebuild the decal first: `TC_REBUILD=M_HitMark Tools/build_content.sh`. Every hit
   the flight core shows is kept on the aircraft it struck for the rest of its flight: a machine-gun strike as a stitch
   of three small holes within about a metre, a 20 or 25 mm shell as a plate-sized tear with a hole beside it, a 37 or
   40 mm as a tear a head could go through. Each hole is black, ringed by a narrow lip of torn bare metal that catches
   the light, with soot blown back over the paint round it, its edge ragged. They are drawn on the ridden aircraft and
   any within 400 m (at most 24 an aircraft, 240 in all), and stay put on the skin as it banks and rolls. Look at
   `09-strike-marks`: the marks on the wings and fuselage, none floating off the skin or lying on the airscrew disc,
   none showing through on the far side of the fuselage. A shell's tear may show on both faces of a thin wing (it went
   through). The log's first lines: "Air battle: strike marks: the first kept on ..." and "... the first laid on the
   skin of ...". If the marks are too faint at the ride's distance, say so; their sizes are in `MarkLooks`
   (AirBattleMarks.cpp). The cloud check fitted every hit in all three battles to the aircraft it struck (365 at
   Bremen, 352 at Pearl Harbor, 595 at Midway; ships' hits to the aircraft they name).
20. **Dead engines' airscrews** (the realism audit of 22 November: every airscrew turned on, dead or not). A B-17 that
   has lost an engine, seen near: that engine's airscrew either windmills at about half the others' turn, or, once
   feathered, stands still, its blades edge on to the wind (dark strips from the boss to the tips, the turning disc
   gone). The right engine: engine 1 is the port outer, as the crews counted, and the flight core's trail of smoke
   comes from the same nacelle.
21. **Every parachute** (the audit: only the nearest 16 were drawn). When three B-17s go down together some twenty to
   thirty canopies hang in the sky at once, all drawn: the near ones with their rigging, past 600 m with fewer sides,
   past 2.5 km as a plain canopy and its man. Only past 240 are the farthest left out.
22. **Clouds: a few big ones and many small** (the art director on 2b177386's frames, 22 November 04:16Z: "popcorn:
   one size, even spacing"). On Pearl Harbor and Midway (three eighths of cumulus) the big ones, 1.5 to 3.5 km across,
   are laid first and take half the cover, about a hundred of them in the 44 km square; a few hundred middling ones,
   600 m to 1.5 km, a quarter; and some 1,900 small ones, 200 to 600 m, the rest, most of them gathered round the
   bigger and a few out in the open sky. A small one is drawn out to 34 times its width (a 200 m one to about 7 km),
   so the sky draws 5 to 27 per cent fewer puffs than 2b177386's did. Look at Pearl 01, 02 and 07 and the Midway
   flyover: big heaps with small rags round them and open sky between, not a field of one size. The cloud check's
   pictures are in /mnt/project-files/air-battles/scene/round-r2e/clouds/. Build-ups over Oahu's mountains are not
   done yet.
23. **The paint flat, not shiny.** The aircraft are now painted 0.78 rough with 0.3 specular (was 0.5 and 0.45): the
   P-40's olive drab on Pearl 05 read sand, (198,180,161), where the low sun ahead glanced off its wing. Look at
   Pearl 05 and Bremen 02: olive drab reads olive drab near the art director's matte #555A47, no sand or pale sheen.
   The log says "Air battle: the aircraft's paint 0.78 rough, 0.30 specular." "-TheaterPlaneInk=0.7,0.35" tries
   others (roughness, then specular). The canopies are in the same paint, so they are duller too; say if they read
   as painted over.
24. **Haze over the sea bluer.** Over the sea (Pearl Harbor and Midway) the haze band at the horizon now takes the
   sea's haze, (0.40,0.53,0.76), not the land's, and the air's white dust is 0.6 of the battle look's. The log says
   "... the air's dust 0.60 of the battle look's over the sea". Measure the horizon on Midway 01 and Pearl 02: pale
   blue near (196,212,230), fading over less than a third of the frame, not (215,218,225). If still milky try
   "-TheaterSeaDust=0.4" and "-TheaterSeaHazeInk=0.36,0.5,0.76"; if the sea's far edge shows a line, say so. Bremen,
   over land, is unchanged.
25. **Quays grey; Battleship Row in Measure 1.** Oahu's concrete is a cool grey, 8A8D90 (was the warm 8F8B83 that
   read tan under the morning sun); the pads and aprons too, on both stages. The American ships on the stage are in
   Measure 1: Dark Gray 5-D for the kit's navy grey, and their wooden decks stained Deck Blue 20-B. The log says
   "Stage: Battleship:USA in Measure 1 ...". Look at Pearl 06: the battleships cool dark grey from above, not tan.
   The map's own miniatures keep the kit's paint.
26. **The strategy view on the aircraft.** It now follows the crowd it frames as the crowd moves and eases only a
   jump to another crowd: before, at the clock's x10 it trailed the raid by a kilometre and more, and Pearl 01,
   Midway 01 and Bremen 01 showed empty sky. It also stands at 800 m and 12 degrees down (was 1,100 m and 14). Look
   at 01, 04 and 07 in every battle: aircraft in the frame, a B-17 about 70 pixels across at 1920. The guns fire
   back picture, 06b, frames ship and aircraft with an eighth to spare (was a quarter), 350 m at the nearest.
27. **The ride's log.** While riding, the dispatches show only the last four, at least 34 characters wide beside
   the aircraft ridden. Look at 03 and 05.
28. **The strike at the player's pace** (the realism audit of 22 November, 4.2; the flight core air-r2-core). While
   bombs or torpedoes are in the air, a bomber's doors are open on its run or a dive is going down, the clock holds
   his own pace even before the formations meet and on the way home: the tag reads "THE STRIKE · RUNNING ×1". Five
   seconds after the last of it the clock eases into ×10 over three seconds and the tag goes back to "×10 UNTIL ...".
   Bremen and Midway: the bomb's rush (bomb_release) is heard as a stick falls near the eye, and not while the clock
   runs fast. The rally: the lead drops first, the rest of the box on him, then it turns home on him (the flight
   core's own rally, which replaces the scene's of e01b4c03).
29. **Akagi's column and far bomb carpets** (the effects kit's 01f2dfc7; the art director on Midway 06 and 06b). Midway
   06: a carrier lost to fire stands under a column, black and dense at her deck, leaning downwind and paling browner
   grey as it climbs to some 600 m, never a soft black ball; it is already grown when she is lost. 06b and Pearl 06:
   the ships' 25 mm and 40 mm read as soft amber hoses with a hot head every third round, thinning past 300 m, out by
   2 km and 3.5 km, no near-white. Bremen 01 and 07 and the strategy views: bombs landing far off still show (the kit's
   far form), each where the flight core lands it; near ones as before. Log: none new.
30. **No aircraft shadows past a kilometre** (AS1 of the production lead's battle-scale plan, 22 November). An aircraft
   in the air casts its shadow only within 1 km of the eye (1.1 km before one casting stops); one on the ground keeps
   it to 3 km. Look at the strategy views (01, 04, 07) and the chase: no aircraft should change its shading as it
   crosses a kilometre; if one does, "-TheaterPlaneShadowM=1500" tries a longer reach (the log then says "Air battle:
   aircraft cast shadows within ..."). Read stat unit's graphics time at Bremen 07 with and without
   "-TheaterPlaneShadowM=100000" (every aircraft casting, as before) and give both.
31. **Small clouds from above: no cotton balls** (the art director on a1acb552's frames, 22 November 08:13Z, her first
   mend). Each small cloud (100 to 600 m) is now drawn one and a half to twice as long as it is wide (one in seven to
   2.5; her trim on the r2g sheets, 11:22Z: past 2.5 they read as rice grains), its top two or three bumps, laid along
   the wind with a ragged edge, and far off its one far shape is long the same way, never a round dot. They gather: after one,
   one to five more follow downwind in a short street, with open sea between the clumps and streets; the share of the
   sky they cover is unchanged. Each casts a soft oval shadow on the sea down-sun (out to 10 km, fading over the last
   quarter). Look at Pearl 01, 04, 05c, 07 and 09 and Midway 01, 02, 04, 05c and 07: clumps and streets with open sea
   between, the shadows soft and about 0.75 of the sea's value, no hard edge and no stripes; say if any cloud still
   reads as a round dot. The cloud check's before and after sheets are in the round's `clouds/` folder.
32. **The paint flatter again** (her third mend: the P-40 read sand from above, the TBD neutral). The aircraft's paint
   is now 0.9 rough with 0.15 specular (was 0.78 and 0.3), so the sky's sheen at a grazing look no longer washes it.
   The log says "Air battle: the aircraft's paint 0.90 rough, 0.15 specular." Pearl 05: the P-40's upper wing olive
   drab about 95 to 120, red no more than green, blue about 0.65 of green; Midway's TBD blue-grey, blue 10 to 20 over
   red. If still pale, "-TheaterPlaneInk=0.95,0.08".
33. **Undersides lit by what is below** (her fourth mend: Bremen 09's B-17 wing 20 to 26 over a deck of 227). The
   aircraft alone now take a light shining straight up, the colour of what lies under them: over a sunlit cloud deck
   a near white at 0.67 (the sun is about 7); over the open sea a blue-grey at 0.23; over land a grey-green; none at
   night. It lights only faces turned down and throws no shadow; the sea, ground, ships and smoke keep their own light.
   The log says "Air battle: the light from below 0.67, (0.95,0.97,1.00), over ... (the view down 0.77 bright)."
   Bremen 09: the bellies and wing undersides 80 to 110; Pearl and Midway over the sea 40 to 60, never black.
   "-TheaterUnderLight=1.5" makes it stronger, 0 takes it away.
34. **The sea's haze bluer still** (her sixth mend: the horizon at 193,201,212). The haze over the sea is now
   (0.42, 0.60, 0.88), was (0.40, 0.53, 0.76): blue up some 18 levels at the horizon, green some 10. Midway 01 and
   Pearl 02: the horizon about 196,212,230; the fade still about 29% of the frame. "-TheaterSeaHazeInk=r,g,b" tries
   others.
35. **The strategy view framed by type** (her fifth mend: Bremen's B-17s 145 to 290 pixels, Pearl 01 and Midway 04
   10 to 25 pixel specks). The look settles on the middle of the aircraft near the crowd it follows (those within
   350 m), not on empty sky between groups, and stands off so the middle aircraft is about 70 pixels across at 1920 for
   bombers of 25 m span or more and about 45 for fighters, 250 to 1,500 m away. Each strategy picture writes a line:
   "Air battle: strategy picture <name>: N groups framed, X m off for a middle span of Y m (about Z pixels across at
   1920); of N aircraft shown the nearest ... m from the look, ... within 600 m of it; the eye ... m up." On the
   2881-wide frames a B-17 about 105 pixels, a fighter 60 or more. Send these lines with the frames.
36. **Bullet holes in walking lines** (her seventh mend; Brian, 04:12Z). A burst's first strike on an aircraft lays a
   line of holes walking across the skin: machine-gun rounds 6 to 14, 0.3 to 0.8 m apart, each a dark dot about 6 cm
   wide with a bright bare-metal rim; the same burst's later strikes add 2 or 3. A 20 mm hit lays 2, 0.5 to 1.2 m
   apart, each a 15 to 25 cm star-shaped tear. Flak lays one torn hole with splinter holes round it out to 1.5 m. The
   ride for 09 looks at the side with the most marks, from below if most are underneath. Rebuild the decal first
   (`TC_REBUILD=M_HitMark Tools/build_content.sh`: new Star and MetalColour). Look at 09: lines of small holes, not
   one big tear; none floating or on the airscrew disc.
37. **Two new pictures: a dead engine and the parachutes** (her eighth mend). `<key>-10-dead-engine.png` rides an
   aircraft flying on with an engine out (a bomber before a fighter), the eye on that engine's side, 85 s after the
   formations meet: its airscrew windmilling slowly or stopped edge-on. Log: "Air battle: dead engine picture on ...:
   engine N of M out, its airscrew windmilling (or stopped); the eye on its right (or left)", or "... no aircraft flew
   on with an engine out, so none taken." `<key>-11-parachutes.png` is taken 3 s after three or more open canopies
   first hang within 1.5 km of each other, the eye some 300 m off them: every canopy drawn. Log: "Air battle:
   parachutes picture: N open canopies within 1.5 km of the look, the eye ... m from their middle."
38. **Fires on a ship burn as a ship's** (the effects kit's Burn bOnShip, 08a18f21). Every fire the stage and the ship
   looks light on a ship (a bomb on her deck, her magazine, her end, a look's fire, its beaten-out fire) is told the
   kit as afloat, so however small or low it is it never takes a burning tank's look. Look at Pearl's and Midway's
   ships picture (06): deck fires with a ship's column, no squat tank-fire shape on a deck.
39. **Fuel balls** (the effects kit's FuelBall, the art director's head D). An aircraft flown into the ground (not a
   belly landing, not a piece of one) throws a rolling ball of burning fuel 15 to 25 m across by its size (a
   fighter's 15, a heavy's 25), then burns on as before. A bomb that bursts on an oil tank of the stage, or within 6 m of
   its wall, sends that tank up once: a ball off its roof (24 m) and a fire that burns on 30 minutes. Log: "Air
   battle: an oil tank goes up (N of M alight), a fuel ball 24 m across, D m from the bomb." The stage test
   (`-TheaterStage=midway`) throws a ball off each of Midway's six tanks.
40. **The camera keeps off a fresh fuel ball** (the camera rules' 13, the art director 22 November 08:08Z). No eye
   stays within twice a ball's width (30 to 50 m) in its first 2 s on the air clock. The close view is cut on the
   flash to another aircraft whose eye stands clear (a fighter's eye 23 m from it, a heavy's two spans); with none
   clear, the eye stands off the ball at its reach until the 2 s are out. The overview is drawn back along its own
   line until clear, the player's zoom with it. Logs: "Air battle: the close view cut on a fuel ball's flash ...
   (rule 13, cut N)", "Air battle: the overview drawn back off a fuel ball's flash, A m off to B (rule 13, cut N)".
41. **The effects drawn alongside the sky** (the production lead's FX2, the kit's TickBegin and TickEnd). Each kit's
   own frame (the air's and the ground's) is drawn on a worker while the scene paints its sky, begun once the
   frame's shells, fires and trails are given and the eye is set, ended after the sky. The sky behind the smoke
   (SetSky), asked between, comes a frame later. `-TheaterFxOneTick` puts both back on the game thread after the sky,
   as before, to compare. In a trace: AirBattle_EffectsBegin, then AirBattle_DrawSky, then AirBattle_EffectsTick (the
   wait for the workers, short when they finished under the sky).
42. **Aircraft past 300 m drawn as copies, and a light shape past 1.5 km** (AS1 of the production lead's battle-scale
   plan, 22 November: "B-17 middle shape (~1,500 triangles); instanced middle and far tiers"). Each aircraft was a
   mesh of its own, handed to the drawing thread afresh every frame, for the picture, its depth and its shadow: at 150
   aircraft, 150 of each. Now an aircraft past its near shape (300 m, 450 m going out; never the ridden one, nor one
   with its bay open) is a copy in one instanced mesh for its model and paint, sent once a frame with where each copy
   is and where it was the frame before (so the anti-aliasing and the blur see it move). Out to 1.5 km a copy is its
   far shape, as before; past 1.5 km (the battle's 55 degree lens; a narrower lens moves it out as far as it enlarges)
   its light shape: its far shape taken down to 500 to 1,500 triangles (Battle/AirLight; a B-17 7,122 to 1,499, a Kate
   11,002 to 1,500, a fighter some 1,900 to 500), within half a pixel there of the far shape (a pixel is 1.45 m at
   1.5 km), its paint lines kept. Shadows as item 30: within a kilometre, three on the ground. The near shape, the
   airscrews, the bay and the strike marks are its own mesh's, as before. Look at the strategy views (01, 04, 07) and
   the chase: no aircraft should change its paint or its outline as it crosses 300 m or 1.5 km, and none should jump
   or smear as it does. If one does, `-TheaterPlaneCopies=0` draws every aircraft on its own mesh as before, and
   `-TheaterPlaneLightM=3000` moves the light shape out. Every 10 s the log says "Air battle: N aircraft drawn as
   copies, K on their own meshes, and B bombs falling as copies, in M meshes"; the light shapes are logged as they are made ("Air battle: B17:USA's
   light shape ..."). Read Bremen's game thread and draw thread times with and without `-TheaterPlaneCopies=0`.
43. **Bombs and torpedoes falling as copies** (AS2 of the battle-scale plan). Each was a six-sided rod laid afresh in
   the small things' mesh every frame; a raid's carpets were hundreds of them rebuilt and sent each frame. Now each is
   a copy of one bomb mesh (or one torpedo mesh), the same rod in the same casing, turned along its fall and sent with
   where it was a frame before (from its speed), with the aircraft's copies. Nothing should look different: Bremen's
   sticks and Midway's torpedoes as before, no smear on a falling bomb. `-TheaterPlaneCopies=0` lays them as rods again.
   The 10 s line counts them ("and N bombs falling as copies").
44. **A named ace's victories on his aircraft** (Brian's named aces, release 2; the air thread's extra.ace, read into
   AirShow::Group as AceName, AceKills and AceBy). His group's lead aircraft (its first) wears one mark a victory in
   the victims' symbols (a cross for a German, a roundel for a Briton, a tricolour for an Italian, a red star or a cherry
   blossom for every kill where his own nation paints its own), laid on its model's kill slots under the cockpit by the
   modeller's rules (Battle/AirTally: each nation a row of its own, the commonest first, at most thirty; the oldest of a
   row a little paler), from the modeller's tally sheet as the art director passed it (Content/Data/Tally; M_TallyMark,
   built by Tools/build_content.sh when missing), the dark or light set by the paint under them. Upright on his side,
   its right toward the tail. Seen on its own mesh, fading into the paint as a mark falls under 7 pixels across and gone
   under 4 (the art director, 18:31Z: "rather than shimmer or turn into a grey block"; some 35 to 60 m off on the Mac's
   screen); soot and holes land on them as on his skin. A model with
   no kill slots flies unpainted (the log says so): today the Mustang, Hellcat, Wildcat, C.202, Devastator, Dauntless,
   Kate and Val have them; the modeller's battle-standard fighters bring them to the rest. A German pilot wears his
   kills as white bars on the rudder instead, and his name and theirs are painted as item 46 says.
   `-TheaterAces=0` paints none; `-TheaterAceTest=att:6:GER=6` gives the raid's first fighter group a test ace (def for
   the defence; more nations after a comma, `GER=14,ITA=6,UK=3`), for frames where the recording has none. Look for: the
   marks square to his side, not mirrored (the Italian tricolour green toward the nose), not floating off the skin or
   sunk into it, no shimmer as he moves off; and only on his aircraft, never moving to another when he falls.
   The picture run takes `12-ace-close` and `12b-ace-chase` of him (the art director checks only those two): the marks
   as matte as his paint and never brighter, taking its light, shade and grime; flags and roundels not sticker-bright;
   no row running on to the wing-root fillet, and no code letter or name panel over the aft end of a full row.
   His name (item 46) is painted with them.
45. **Far trails and far guns** (the battle-scale plan's AS2; the effects kit's far forms, game.air_far in
   Content/Effects/battle-fx.json, passed by the art director 19:25Z 22 November and turned on here): past 1.2 km a
   trail is laid as some 40 longer pieces, not about 128, going over by degrees across the last 15% of the range so no
   length of it changes as the eye closes in; past 1.5 km a gun lays one dash a burst along its aim instead of flying
   its rounds. With them on the scene lights up to 60 trails at once, not 24 (`-TheaterTrailRoom=` tries another, 10 to
   120); the log says "Air battle: room for 60 trails at once (the kit's far trails on, far guns on)". Look for: a far
   trail as dark and lumpy as a near one, never pale at its head, and no jump as a fighter trailing smoke closes from
   2 km to 1 km; a far gun's dashes reading as fire, not as a line drawn on the screen. To turn either off, set its
   `trails_on` or `guns_on` to 0 in battle-fx.json (and tools/battle-fx/fx.json, its source).
46. **A named ace's name under his cockpit, and a German's bars on his rudder** (Brian's named aces, release 2; the
   modeller's tally.name on every fighter, battle-models/air-next/ACES.md, after the art director's pass of 19:47Z 22
   November). Over his kill rows, on the line the modeller laid under the port cockpit sill: his rank short as his nation
   wrote it, his initials and surname, in plain block capitals (`F/LT T. HARLAND`, `OBLT. K. VOSSLER`, `ST. LT. I.
   VERESOV`; a carrier group's ace his navy's: `ENS.`, `LT.(JG)`, `LCDR.`, the Fleet Air Arm's `S/LT`, `LT CDR`, `CDR`;
   Japan's and the nations ACES.md gives no short ranks for, initial and surname: `T. KAWANO`). At the
   file's `cap` (5 cm; 4 on the Bf 109 and Me 262, 7 on the Hurricane, Warhawk and the plain fighters), smaller down to
   `cap_min` to fit the line, then shortened: rank and surname, then the surname alone (Battle/AirTally NameTries and
   LayName, the rules of acepanel.py). One small square a letter at its place along the line, upright, facing her skin
   there, so the name follows her curve; drawn with M_TallyMark from the letters' sheet (Content/Data/Tally/name-letters,
   Tools/name_letters.py: the modeller's hand, DejaVu Sans Bold narrowed to 0.72, 100 capitals with the accented ones the
   rules' Polish, Hungarian, Finnish and Romanian names use). One ink for the paint under the middle of the line: yellow
   D9B43A on an American machine's olive drab (dark, red and green close, both 10 or more and 1.15 times over blue, so
   the game Mustang's matte 555A47 is one), else black 1A1A1A on paint of L* 50 or more and off-white F2F0E8 under it.
   The paint under a mark, a bar or the name is the colour with the most of her skin within 2 cm about it, by area,
   not the nearest point (a rivet's or a seam's under a third of the slots). A German pilot (the painter's nation decides, as the modeller's marks.record does) wears no victims' marks on his
   side: his kills are white bars (DAD8CE) on his rudder's port face, one a kill, from the modeller's tally.rudder; the
   scene's rudders do not turn. Each part fades by its own size (a capital, a mark, a bar) from 7 px to 4. A fighter's
   marks to a row are her file's (`per_row`: the Spitfire's nine, her number covering a tenth column, 27 at most). The
   test ace without a title takes his nation's sample (`Flight Lieutenant Tom Harland`). The log says "Air battle:
   aces: "F/LT T. HARLAND" painted under Spitfire's cockpit (key N), capitals 5.0 cm, off-white ink" and "... painted as
   N bars on Bf109's rudder". The game's fighters are the modeller's battle models with the line (Brian's yes, 20:12Z
   22 November): the Spitfire, Hurricane, Bf 109, Fw 190, Zero, Yak, Warhawk and the plain fighter replace the old
   Blender sculpts, which stay in Content/Models/Old (`-TheaterOldVehicles` draws them instead, to compare and time
   only: the battle's airscrews and marks still read the new files; Tools/build_plane.py now writes there); the Mustang takes the modeller's shape and keeps the game's own paint
   and paint by date; the Hellcat, Wildcat, C.202 and Me 262 their new files; the carrier fighter the unit table names
   is new. The two what-if fighters wait for something to name them. A model without the line flies with marks only.
   Look for: the letters upright and reading from the nose aft (not mirrored), on her skin, not over the sill, the glass
   or the number; even spacing; the ink as the rule says; no shimmer as he moves off.
47. **Japan's land bombers, and a torpedo only on a torpedo run** (Brian's yes, 06:14Z 23 November: every nation's own
   aircraft). The modeller's Nell and Betty are in Content/Models byte for byte from battle-models/air; on the campaign
   map Japan's bombers are the Nell, and the Betty from 1941 (the rules' years), with rows in the map's aircraft table
   (far shape aloft, a bomber's framing, their titles). In the battle the record's G3M and G4M find them; their engines
   (AirShow ShapeOf) are where their files' sockets say fire and smoke start, about the box's middle the scene draws
   them by (the G4M's were 0.6 m outboard and 1.3 m high of the Betty's nacelles). A model whose file carries both
   loads (the Kate, the Devastator, the Nell, the Betty) is drawn with its torpedo in a torpedo group and with its
   bombs everywhere else, the map included (FMiniatureMesh::bTorpedoLoad; the loader leaves the other load's triangles
   out, every point kept for the bay, airscrews and marks); before, both hung under her at once. The SM.79 has her
   shape (three engines, the nose her second), her name SM.79 and her engine sound, but her file waits for the
   modeller's rebuild (bay, damage layers, a lighter far shape): until then Italy flies the plain bomber in her paint.
   The airscrew test takes as many screws as a file says she has engines (the SM.79's three). The twins' drone
   (EngineLoop) now also takes the Nell, the SM.79, the Wellington, the Pe-2, the LeO 451 and the plain bomber in any
   paint ("Bomber:UK"), which flew with a fighter's engine before. `-TheaterOldVehicles`
   also stands the plain machine for the C.202, Nell and Betty, so that the map's timing runs differ only by the new.
   Look for: a level Kate at Pearl with her bomb and no torpedo, a torpedo Kate with her torpedo and no bomb; a Nell
   with her bombs under her and no torpedo; fire from a Betty's nacelle, not from her wing root.
48. **FLY IT YOURSELF on the raid card** (release 1.6; Brian, 23 November 06:52Z: the campaign's air fights open as 3D
   air battles). On the raid card of a bombing raid by day (`bomb`) or a fight for the sky (`superiority`), FLY IT
   YOURSELF stands beside FLY THE RAID, as FIGHT IT YOURSELF stands beside SEND THEM IN. It is shown only when the Battle
   rules card's **Fly raids yourself** is on (`AAirBattle::CampaignLiveOn`, the rule `flyLive` in FLandLive's table,
   never sent to the land rules) and TC.airLook has a ticket for the raid as picked (weighed once a pick). The switch
   is OFF until the playtester passes it (ruling 217) and the Mac holds 45 fps; `-TheaterAirCampaignLive` turns it on
   for one run, `-TheaterAirNoCampaignLive` off, `-TheaterRulesFlip=flyLive` clicks the card's switch. Pressed, it opens
   `BeginLive(Session, Kind, Target, Ids)` with the card's own fly act (`raid:kind:target:id.id`) and puts the card
   away. At CONTINUE on the after card, `Finish` shows the table and runs `HandBack` with TC.battleEnd's answer, which
   is what TC.act answers for the raid (the rules booked it there, with the battle's losses); the controller reads it
   as `Act` does (`FCampaignSession::SettleAirLive`: ok, note, flies, view), brings the map up to date, gives the note
   and takes the eye to the target. Needs the rules bundle with TC.airLook, TC.airStart and TC.battleEnd's raid
   (battle-bridge.ts on the map branch); without them the button is simply never offered.
   Look for: the button on a day raid and on a fight for the sky, never on a night raid or a strike on shipping; the
   battle opens on the raid's own groups; after CONTINUE the map's month shows the raid flown (the groups away, the
   target's damage) and the note says what came of it; the month cannot be ended while the battle is up.

49. **The modeller's planes of 23 November in the game: the Wellington, the Pe-2, the LeO 451, the D.520, the rebuilt
   SM.79 and the rebuilt Betty** (Brian's yes, 06:14Z 23 November: every nation's own aircraft; item 48 is FLY IT
   YOURSELF, on release 1.6's line). Their files are in Content/Models byte for byte from battle-models/air, each passed
   by the art director. On the campaign map, as the rules' fighterOf and bomberOf (doctrine.ts; the air rules thread's
   4e164552 and 78e334bf): the Commonwealth's bombers are the Wellington until the Lancaster of 1942; the Soviet
   bombers the Pe-2 from 1941 (a light bomber: it waits on no heavy bomber research; the SB, the plain bomber, before);
   France's fighters the D.520 and her bombers the LeO 451 from 1940 (the MS.406 and the Amiot and Bloch, the plain
   machines, before); Italy's bombers the SM.79 from the first. Each has a row in the map's aircraft table (far shape
   aloft, its framing, its title). In the battle the rules name them by their files; the record's marks Pe-2, LeO 451
   and D.520 find them too (ModelFor). Their engines (AirShow ShapeOf) are where their files' sockets say fire and
   smoke start, about the box's middle the scene draws them by (the SM.79's three moved to her new file's: the wings'
   1.3 m inboard). The twins' drone already took them (item 47).
   - **Airscrews:** the finder (AirScrew::Find) now also takes a piece painted blade that is no blade to read on its
     own, made apart from the blade it wraps, with the screw whose disc holds all of it (a quarter of the radius either
     side of the disc): the Wellington's painted tip bands, 300 of her 876 blade triangles, were left standing still in
     the turning disc. Every other model's screws are as before.
   - **The Betty's torpedo run:** her rebuilt file hangs her torpedo half out of her belly with her bay's doors off.
     The loader (FMiniatureMesh::Sculpted) now also leaves out whatever her file's own load clip hides (load.torpedo
     or load.bombs, each track's part scaled to nothing): on a torpedo run her bombs and both doors, on a bombing run
     her torpedo. The Kate, the Devastator and the Nell leave out exactly what they did (their clips hide only the
     other load).
   - **The Pe-2's bombs and her bank:** a span under 18 m gave her a stick of one (Show::BombsOf), and a stick of one
     has no bay: her doors never opened and her one bomb left through them. Her load is now four from her fuselage bay
     (her nacelles' two are not drawn), the Wellington's nine 500 lb bombs (ten by her span before), the LeO 451's six
     by her span. And a bomber now keeps her wings level on her run and her rally turn gentle by her engines, two or
     more, not only by a span of 20 m: the Pe-2 (17.2 m) banked 21 degrees with her doors open, and so would the plain
     bomber (19.8 m). Every other bomber flies as before.
   - **Light shapes the maker cannot make:** the SM.79's first far shape had its cabin's glass and skin doubled over
     the same points (699 edges shared by more than two faces), and the light-shape maker (item 42) stopped short of its
     1,500 triangles (1,524) with the shape torn. The modeller rebuilt her far shape (09:41Z; her near shape, parts,
     sockets and clips unchanged): it now comes to 1,499 of 1,500, as clean as the others. The maker now also makes none
     for any shape it cannot take down to what was asked: that aircraft's copies past 1.5 km keep their far shape, and
     the log says so ("Air battle: <shape>'s light shape left unmade ...").
   - `-TheaterOldVehicles` stands the plain machine for all of them on the map, as for the C.202, Nell and Betty.
   - The Wellington's near shape is the heaviest in the game (113k triangles: her fabric's ridges are real shape; the
     B-17's is 23k, the Betty's 85k); it is drawn only within 300 m of the eye, as every near shape is.
   Look for: an RAF raid of 1940 or 1941 in Wellingtons, not the plain twin, her airscrews' yellow tips turning with
   them; fire from a nacelle, not a wing root; her bay doors opening and her stick falling from the bay; a Pe-2 box
   opening its doors before its stick of four, wings level; a French escort of D.520s over LeO 451s; Italy's SM.79s
   with three turning airscrews; a Betty on a torpedo run with no doors and her torpedo half out; the frame rate with a
   box of Wellingtons close to the eye.
   Not yet: a torpedo aircraft keeps her torpedo drawn after she drops it (the Kate, Devastator, Nell and Betty, as
   before this item); the racked bombs are taken away when they fall, the slung torpedo is not. And the air test, run
   on the raids recorded for this item (none is among its own), fails a few checks set for the set pieces: the rally
   check on the
   Pe-2s over Tula in June 1943 (a box 91 m across at its first bomb, 203 at most after, against 1.5 times and 30 m;
   1941's boxes pass with the same spread after) and on the SM.79s (the LeO 451's raid renamed: 151 m, 382 at most;
   the LeO 451's own, 234 and 333, passes); the cloud checks on the clearer skies of the Wellington's and 1943's
   raids; the deck checks over high ground; and, in the D.520's fight for the sky, the checks that look for a raid.

50. **A raid he flies himself always has a way out** (release 1.5, which takes FLY IT YOURSELF; the release lead's bug
   hunt, 23 November, the air twin of the ground's 039ab38c). Before, a TC.battleTick that failed was only logged: rules
   that failed on every tick never brought the battle to its end, so no after card and no CONTINUE came, and the
   campaign behind it (LiveRaid still set) took no order, key or month's end: the game had to be quit. Now Feed counts
   the ticks the rules fail, or answer with no view the scene can read, and any good one resets the count. Three or more
   in a row over 2 s (the wall clock's seconds) stop the raid where it stands, with the held note "THE RAID STOPPED HERE:
   ITS RULES FAILED. NOTHING IS BOOKED; BACK TO THE MAP IN A MOMENT"; 4 s on, WayOut closes the scene as CONTINUE does
   (Finish), with an empty result. The campaign (AirHandBack) books nothing, brings the raid card back as he left it,
   with FLY THE RAID on it and FLY IT YOURSELF not offered again for that pick, and says so. A raid whose end has come
   keeps its after card. And an end the rules fail (TC.battleEnd throws, or answers null) still gives an after card with
   CONTINUE, now one that says so ("The raid's end could not be booked"), where it was a blank card before; handed back,
   it books nothing, the same way. To see both on the Mac: `-TheaterAirRulesFailAt=40` fails every tick from battle
   second 40; `-TheaterAirRulesFailEnd` fails the end.
   Look for: the note after about 2 s of a frozen raid, the map 4 s later with the raid card up and a note under it;
   the month can be ended and the raid sent with FLY THE RAID.

51. **A raid on troops, flown in 3D** (release 2; Brian, 24 November 2026: "i want every air battle playable" and "if a
   bomber group is bombing troops instead of a factory or town and they get intercepted than i want that playable"; the
   every-kind contract, `/mnt/project-files/air-battles/every-kind/CONTRACT.md`, sections 1 to 4). Where the sky has
   marks (`place.kind` "ground", `place.marks`: the army's column the rules placed, at most 64 pieces), the scene stands
   it on the land under the planes, faced along its road: each tank, lorry, half-track and towed gun as its army's model
   (the rules' own name where the game has it, else the nation's line tank, lorry, carrier or field gun, in its paint;
   a light flak gun stands as the field gun until one is modelled), each block of men as a few of its riflemen, one
   for each `per` men left in it, two files along the road. One instanced mesh a model, shadows on for the vehicles,
   none past 12 km from the eye (`AirBattleTroops.cpp`). The flight core (`AirShowTroops.cpp`) holds each counted hit
   (`air:mark-hit`) until a drawn bomb of its group can be steered onto its piece in the air it has left (inside it for
   a wreck, a burn or a blast; beside it, within its blast, for a near miss or men scattered), or a strafing pass's
   rounds reach it, and shows it then; every other bomb of the stick comes down clear of every piece by its blast ("most
   shots miss, but every hit shows on what it struck"). An `air:strafe` sends up to four of the group down the lane:
   run in, a plunge with the guns on from 900 m to 250 m slant and the aim walking down the road, a 4 g pull-out and a
   climb away, within the flight's limits; their rounds miss every piece but the one they struck. As each hit shows, a
   vehicle set burning or wrecked goes up (FuelBall) and burns on (Burn, 600 s), a wreck wears its burnt shape, a
   damaged one smokes, a strafing pass's burn or wreck leaves a scorch (a bomb's crater is laid where it fell), a block
   of men thins and scatters; the views' words (`view.marks`) have the last say, but a wreck never changes again. The
   column's light flak (`aa` pieces, the share `view.marks.fire` mans) lays short streams on the nearest raider in its
   reach, through the effects kit as every gun, heard with the mix's cannon take. The screen speaks of the column: the
   title and the after card ("The raid on the British I Corps near Hampshire"; a card line's "the target" becomes "the
   column"), the top bar ("their column's organisation cut by 1.2% so far", from `view.marks.cut`), the aim slip ("The
   British I Corps on the march near Hampshire", "On the column so far: 1 vehicle or gun wrecked, 2 burning, 3 damaged,
   22 men lost", as the views count them); and with his fighter groups in hand over the enemy's column (never his own,
   where he defends it), STRAFE THE COLUMN in the orders strip sends `{ do: 'air:strafe', units: [...] }`. Nothing is
   told before it is drawn (the reviewer, 25 November): a hit's dispatch line and its sound come in the frame its bomb or
   rounds land, the aim slip counts a piece struck only from then (`AirDesk::Desk::Struck`, from the core's column), and
   the top bar's cut waits until every hit told up to its view has landed (`Show::MarksCut`). A near miss leaves a piece
   as it was until the views' words say it was damaged. Drawn only: the rules decide every hit, state and loss.
   The switch: the Battle rules card's **Fly raids on troops yourself** (FLandLive's rule `flyTroops`, OFF; never sent
   to the land rules: `FLandLive::Start` skips every rule whose key starts "fly"), read by
   `AAirBattle::CampaignLiveOn(Kind)` ("bomb" and "superiority" read `flyLive`, "ground" `flyTroops`).
   `-TheaterAirRuleTroops` turns it on for one run, `-TheaterAirNoRuleTroops` off. With it on, the rules are told before
   each TC.airLook and as the battle opens, `TC.airKinds(["ground"])` (the air rules thread's switch, default none; once
   more with `[]` after the row goes off), and FLY IT YOURSELF is offered on a raid on troops only where they could be
   told and TC.airLook gives a ticket (this base's rules have neither TC.airKinds nor `ground` in `FOUGHT_KINDS`, so it is
   not offered yet: the log says "the rules could not be told"); with it off nothing changes and nothing is sent. A raid with no marks never reaches any of this. The cloud test
   (`Tools/air_show/air_marks_test`, run by `run.sh` on every recording) plays a marked recording and checks every hit,
   every bomb, the pass and the words; `Tools/air_show/make_marks_recording.py` made the test's own recording from the
   day raid on Hampshire (`troops-test.json.gz` in the every-kind folder's `troops/`, with its pictures).
   Look for (the recording: gunzip it into `Content/Data/AirBattles/troops-test.json`, then `-TheaterAirBattle=troops-test`):
   the log's "a raid on troops: the column under it, 44 pieces (34 vehicles and guns, 10 blocks of men, 0 drawn as
   nothing)"; the column on its road through the aim, Matildas, lorries, 25-pounders and men, standing on the land (not
   sunk, not floating) and driving nose first along the road (if every one faces backwards, the half turn in
   `DrawTroops` is the mend); the two boxes' sticks from 394 s mostly clear of the column, and the ten hits each where
   its bomb lands: gun 30 wrecked (burnt shape, fire), lorry 28 going up and burning, near misses smoking (lorries 18 and
   27, flak gun 29), men blocks 15, 34 and 35 thinned and scattered; the dispatch "A gun of the column is wrecked on the
   road." in the frame gun 30 goes up, not before, and the aim slip and the top bar's cut rising only as the hits land;
   at 408 s four Bf 109s down the road, guns firing on the way down, pulling out some 70 m up, lorry 25 set burning by
   them (a fuel ball half as wide again: a big hit), men 14 scattered, the clock at x1 while they go in; the four light
   flak guns' short streams at the nearest raider, heard; gun 30 still wrecked after the stale word at 420 s; the aim
   slip, the top bar and the after card as above;
   45 fps or better with the whole column in view and burning; the Battle rules card's new row, OFF, its tip in full;
   FLY IT YOURSELF still on a day raid with the row off.
52. **The scramble offer: he meets an enemy raid himself** (release 2; Brian, 24 November 2026 22:21Z: "i want every air
   battle playable"; the contract, air-battles/every-kind/CONTRACT.md section 7). In the enemy's month the campaign stops
   where an enemy raid on his ground meets his fighters: TC.endMonth answers `scramble: { id, kind, target, name, nat,
   adj, words, bombers?, escorts?, fighters?, groups?, demo? }` in place of the month's card. The controller's one
   handler of the month's answer (`MonthAnswered`) takes the eye to the target and puts up a card low on the screen (AN
   ENEMY RAID, "A German raid on Kent", the rules' words): SCRAMBLE (brass), LEAVE IT TO THE STAFF, and the tick "Settle
   the rest of this month's raids". SCRAMBLE opens `BeginLiveCall(Session, "scrambleStart", ["id"])` with `HandBack` set,
   his side the defence; what TC.battleEnd answers at CONTINUE is the month's next answer and goes back through the same
   handler (another scramble, or the month's watch and card). LEAVE calls `TC.scrambleLeave(id, { rest })`, its answer
   through the same handler. An answer that is not a month's (no `steps`, no `scramble` with a string `id`: the
   stand-in's `{ ok, note }`) shows the month's answer kept aside, and the log says so. The answer's `note` is said under
   the map and again after the month's own first line ("February 1940. Your orders. That was a demonstration ..."), which
   would otherwise take its place at once (the reviewer, 25 November). The way out (an empty hand-back) calls TC.scrambleLeave(id);
   if that fails too, the month in hand is shown and logged. While the card or its battle is up: no order, no key, no
   END THE MONTH. At most three cards a month (then the rest are left with `rest: true`); at most eight stops at all.
   SCRAMBLE with a battle already up (a set piece's, a land battle's) leaves the raid to the staff, as FlyLive refuses.
   - **The switch:** the Battle rules card's **Scramble your fighters** (`flyDefend`, `AAirBattle::DefendOn`), OFF;
     `-TheaterAirRuleDefend` on for a run, `-TheaterAirNoRuleDefend` off. FLandLive::Start skips every `fly` key. With
     it on, before every TC.endMonth the rules are told `TC.scrambleMode('every'|'big'|'never')`: OPTIONS' **Scramble
     offers** (EVERY RAID / BIG RAIDS ONLY / NEVER, shown with the switch on, kept with the options once changed). With
     it off nothing new is sent: the rules start at `never` at every load, and `never` is sent once only where this load
     of them was told something else before (the reviewer, 25 November: switch off, the same calls as before). Rules
     without the call are asked once and logged. A scramble that still comes with the switch off, or with Scramble
     offers NEVER, is answered at once with `scrambleLeave(id, { rest: true })`.
   - **The defender's read** (with the switch on, any battle he fights as `def`): the top bar's "now about" and HAND IT TO
     THE STAFF's chance are his (the view's `sky.chance` is the raider's), the aim slip speaks of the enemy's raid, the
     question asks to hand over "the fight", and STAY CLOSE and RANGE AHEAD are not offered (he has no bombers to
     escort). A scramble whose rules fail says his staff meets the raid, not that FLY THE RAID still sends his crews.
   - **The demonstration:** `-TheaterAirScrambleDemo` (with the switch on) asks the bridge's stand-in, TC.scrambleDemo
     (Rules/scramble-demo.ts), once after TC.endMonth's answer, and treats its offer as if the month had stopped there;
     it books nothing, and when it ends the month's real answer (kept aside) is shown. Step file:
     air-battles/every-kind/scramble/mac-steps/scramble-demo.steps.txt; the switch off, scramble-off.steps.txt beside it
     (launched with `-TheaterAirNoRuleDefend`).
   Look for: with the switch off, nothing changes (no card, no OPTIONS row); with it on and the demonstration, END THE
   MONTH stops on the card over Kent; SCRAMBLE opens the battle over Kent with the RAF the side marked YOURS and "now
   about" high; after CONTINUE the month's watch and card come as ever, and nothing of the battle is booked.
53. **Bomber nose art** (release 2; Brian, 25 November 2026 01:30Z: "american only"). Each American bomber wears one of
   the modeller's 200 paintings on her nose (battle-models/air/noseart, the air thread's list names.json), packed for the
   game by Tools/pack_nose_art.py into Content/Data/NoseArt (each a quarter of the modeller's size, 352 by 216, lossless,
   never otherwise altered; 11 MB in all). Only the B-17 has a panel for it (panels.json: the B-24, B-25 and B-29 are not
   modelled; the Navy's Dauntless and Devastator and the plain twin stand-in have none). The panel is laid on the game's
   own B-17, her near shape: the painting's 2.2 m from 7.2 m to 5.0 m forward of her middle, below the windscreen and
   above her grey belly, all of it on her olive with some 4 cm to spare, 2 to 5 mm off her skin; port always, and
   starboard too for one painting in eight (the brief's "a few aircraft carry it on both sides"), mirrored so that it
   reads from left to right on both sides. Drawn with M_TallyMark over the paint under it (AirBattleNoseArt.cpp), one
   small mesh of her own while she is within 300 m of the eye on her near shape, a painting read at most once a frame.
   Which painting (Battle/AirNoseArt): where a view names its group's corps (extra.corps) and the airStart board's
   place.bombers gives that corps' seats (the rules' campaign/bombers.ts; never the raid card's airLook, which strips
   them), its models wear them, the model in slot k seat k times per (the first of the seats it stands for, as the
   show's gunners name a bomber's slot), old hands first, a seat with art 0 bare. The rules do not name a box's corps
   yet, and nothing else says which group is which (the battle numbers its groups in the campaign's own order of its
   air groups, where a newer one can stand ahead of older ones, and place.bombers is keyed by corps), so its groups are
   never paired by their order; until the rules send it, and where the battle says nothing (the Bremen film, the test
   battles): each group's paintings shuffled from the battle's seed and its id and given in turn, none shared in the
   battle while any is unused, about one in five a bare nose. An aircraft keeps hers for the whole battle, but for one
   the battle names as she falls or falls out of her box (the loss's extra.bomber, the dispatch's "That was <name>"):
   whichever model the show takes for it wears that bomber's painting from then, and the one of her group that wore it
   takes hers. The show's choice of model is its own, the same with the nose art on or off. Shown, never counted.
   Off until the Mac and the art director pass it: the Battle rules card's **Bomber nose art** (FLandLive's rule
   `noseArt`, never sent to the land rules); `-TheaterAirNoseArt` on for a run, `-TheaterAirNoNoseArt` off,
   `-TheaterRulesFlip=noseArt` clicks the card's switch. Off, DrawNoseArt returns at the switch and the scene's calls
   are as before. The cloud test (Tools/air_show/air_nose_art_test.cpp, in run.sh) checks the pack, the panel against
   her near shape on both sides, and who wears what on the recordings and on two raids the rules made
   (nose_art_raid.json): one played through its first named losses, each falling model wearing the painting its loss
   names, and one whose ticket's order is not by corps.
   Look for (`-TheaterAirNoseArt`, the Bremen film, the close view on a B-17 of the 303rd): the log's "Air battle: nose
   art:" lines naming each painting and its key (and "now wears" for a bomber named as she falls); on the B-17s near
   the eye a flat sign-painter's picture on the port side of the nose under the windscreen, lettering reading left to
   right, upright, not floating off her skin nor flickering into it, none on the Mustangs or Fw 190s, a few bare noses;
   the frame rate as before.
54. **Each aircraft type's own figures** (release 2; Brian, 24 November 2026: "i want the flying to be smooth and very
   realistic" and "i want the planes flying with realistic capabilities and tactics"). The historian's table,
   `Content/Data/Air/performance.json`, holds each type's real figures (size, weight, power, speeds and heights, climb,
   turn, roll, g, dive, load and range), every figure with its source and a mark (measured, manufacturer, derived or
   uncertain). `Battle/AirPerf` reads it, finds a type by the names the scene and the rules give it, and works out the
   limits the flight core uses, each formula stated in `AirPerf.h`. Plain C++, no engine; the cloud test
   (`Tools/air_show/air_perf_test`, run by `run.sh`) checks every figure's source and mark, that every model named is
   found, and each type's limits inside their bounds. Not yet wired into the flight core (`AirShow`): until it is, every
   battle flies as before (wave 1's join, 25 November; numbered 54 by the wave-2 plan).

59. **Earned colours** (release 2; Brian, 25 November 2026 03:27Z: "unique colorings or markings like the tuskegee
   airmen"). A group the rules say has earned colours flies its nation's scheme of the art director's palette
   (Content/Data/Air/unit-colours.json, copied unchanged in substance from art-director/air-colours, fifth issue of
   25 November 12:34Z, the final one; the historian checked it twice) as far as it has earned it. The board's place.corpsMarks (airLook,
   airStart and the replays: corps, nat, scheme 0 to 15, tier 1 to 3, name) is read by AirFeed's one line
   (AirUnitColours::ReadMarks, the one small function to change if it is renamed); each view's extra.corps is already
   each group's Corps (item 51). Step 1 the spinner (the Me 262: its nose cap); a bomber's spinners and the top sixth of
   its fin. Step 2 adds a narrow band right behind the spinner (a radial's cowl front lip; a bomber's every cowl), never
   wider than the spinner is long, never the whole nose. Step 3 adds the tail in the scheme's pattern on the tail's own
   height and chord: Britain the rudder, France the fin, the rest fin and rudder; twin fins both, both faces. By night
   (place.night, or a machine with black undersides: the Lancaster, the Wellington) every colour x0.55, the cream the
   palette's grey, the pattern on the tail's top half; a winter month (the painter's, December to February) at full
   strength. Glass, crews, tyres, blades, rivets and seams, lamps, nose art and tallies are never recoloured; the
   markings stay on top, over a plain patch of the first colour on a checks or stripes tail: shaped as the marking and a
   little larger, on its own face, and only for a marking on a part its nation colours (Britain's fin flash and France's
   rudder stripes need none; Savoy's cross has one under its arms and shield, the field behind it in the pattern); by
   night only where the patch reaches the pattern's half, and then whole. The nickname is never painted.
   How: Battle/AirUnitColours (plain) finds each model's airscrews, cowls and fins once from its near shape (a newer
   file by its parts, an older one from its geometry) and lays the coat on the near and far shapes, parting triangles
   at the colours' edges. AirBattleUnitColours.cpp and .inl (Unreal): PlaneMesh's one line copies the aircraft's shape
   once per look ("Bf109:GER|colours GER 0 t3 day"), coloured, kept beside the plain one with its far shape, so every
   aircraft of a group (and any group with the same look on the same model) draws and is copied as one; its light shape
   is made from the coloured far one as any other's. A shape is checked against its model's file before it is coloured
   (one that is not its file's flies plain, and the log says so); where the near shape takes the look and the far one
   does not, the aircraft flies plain (the near shape alone has no airscrews at a distance). The coloured shapes of an earlier battle are let go at
   the next battle's first aircraft with colours on. Shown, never counted.
   Off until the art director has scored the Mac's stills: the Battle rules card's
   **Earned colours** (FLandLive's rule `unitColours`, never sent to the land rules); `-TheaterAirUnitColours` on for a
   run, `-TheaterAirNoUnitColours` off, `-TheaterRulesFlip=unitColours` clicks the card's switch. Off, UnitColoursShape
   returns at the switch and the aircraft is made exactly as before; on, a group with no row flies plain.
   The cloud test (Tools/air_show/air_unit_colours_test.cpp, in run.sh): the palette, the five patterns, the board's
   rows, all 31 aircraft models at 7 nations' schemes by day and night, steps 0 to 3, near and far (where each region
   lies, by the regions and by the file's own part names; nothing else recoloured; each patch only round its marking on
   its own face, measured from the marking as the miniatures draw it; each step adding its own; the parted triangles
   covering what they were cut from), and a hand-made raid's board. Cloud previews, the hand-made raid (unit_colours_raid.json.gz, never committed) and the
   Mac's steps: air-battles/unit-colours/r2/ (NOTES.md).
   Look for (`-TheaterAirBattle=unit_colours_raid -TheaterAirUnitColours`, the raid copied in locally as NOTES.md says):
   the log's "Air battle: earned colours:" lines (on, then each group and model with its step and scheme); the 354th's
   Mustangs with a purple spinner and a narrow orange band behind it, the 303rd's B-17s with bottle green spinners and
   fin tip, Jagdgeschwader 1's Fw 190s with an orange spinner, a purple band at the cowl's lip and the tail halved orange
   over cream; nothing else recoloured, near and far alike; on a checks or stripes tail with a marking (the Yak's star,
   the MC.202's or SM.79's cross), the marking on its own plain patch and the pattern round it; a short hitch the first time a coloured model appears (its
   file read once a battle); the frame rate as before.
   (Joined onto wave 1 on 25 November; numbered 59, after the wave-2 plan's 55 to 58.)
60. **The finer B-17** (release 2; Brian, 25 November 2026: "why don't we have the new b-17 models in the game?" and
   "it is still the old one"). The modeller's newer B-17 of 22 November (Content/Models/B17Fine.json, the file handed
   over byte for byte, sha256 5e1f3900...d0d79b: 142,896 triangles close up against the older file's 23,386, 20,514 far
   against 7,122, her own nose-art panel and kill tally) drawn in the air battles in place of the older one
   (Content/Models/B17.json, unchanged). Looks only: the rules never read either file.
   How: AAirBattle::B17FineRead reads the Battle rules card's **Finer B-17** (FLandLive's rule `b17Fine`, never sent to
   the land rules) once as a battle begins (BeginReplay, BeginLiveCall). On, ModelFor names the B-17 "B17Fine:USA", so
   every reader of the file by the shape's name (the near and far shapes, airscrews, tally, bay, earned colours) and every
   shape kept for the run is the newer one's under its own name; AirShow::SetFineB17 gives the flight core her engines
   where her file's sockets put them (fire and smoke start there); the nose art reads her panel from
   Content/Data/NoseArt/panels-fine.json (0.45 of the older panel's size each way, below her navigator's windows, on her
   coat and its rivet and seam lines: Tools/pack_nose_art.py --fine says why); her rivet and seam lines (44% of her
   close-up points) take the earned colour of the coat beside them, a shade of it as her file has them. Off (the
   default), the name, the files read and every number are what they were. The campaign map keeps the older B-17.
   Her far shape has no spinners and its cowling lips sit a ring behind the near shape's: there the nacelles' grey within
   each spinner's place stands for the spinner, and the band is laid behind the far shape's own lip (AirUnitColours::Find,
   OnShape; hers alone), so steps 1 and 2 show past 300 m as close up.
   The drawn battle with the row on is not the row-off one: her engine places also shape the flight core's airframe
   (the fighters' passes, the gunners' clear lines) and where strikes land, so in the Bremen raid the fighters make 68
   passes against 77. Counted losses and parachutes are the rules' and the same (30 and 44). Keeping the flown battle
   bit for bit would mean splitting drawn and flown engine places through the strikes, the trails and the airframe:
   tried here, it moved the drawn battle further (58 passes) and failed two show checks, so it was left for a change
   made with a compiler.
   The Mac's guard: each finer B-17 that once comes near keeps her close-up copy for the battle (some 31 to 37 MB each,
   arithmetic from the vertex layout, not measured). `-TheaterAirB17FineNear=N` (8 unless asked, 0 to 1000): at most N
   are made for bombers not ridden; each bomber ridden (the close view's, the timing eye's box rider) gets hers too and
   keeps it, so a battle holds at most N plus the bombers ridden in it. Any other finer B-17 that comes near is drawn
   with her far shape as a copy with the rest: standing blades, a painted blur disc, a closed bay, no nose art or tally,
   however near. (The first N to come near keep theirs, not the N nearest: taking a made copy down again was not written
   without a compiler.) Every bomber also keeps her own far section (about 3.5 MB each for her, 1.2 MB for the older
   B-17, arithmetic), which the guard does not limit.
   `-TheaterAirB17Fine` on for a run, `-TheaterAirNoB17Fine` off, `-TheaterRulesFlip=b17Fine` clicks the card's switch.
   The cloud test: Tools/air_show/air_b17_fine_test.cpp in run.sh (row off reads the base's bytes and numbers, row on
   reads her everywhere), and the airscrews', light shapes', nose art's, figures' and earned colours' tests take
   B17Fine.json from Content/Models with the rest; the Bremen raid is flown through the show with her engines
   (AIR_SHOW_B17_FINE=1). Notes and results: air-battles/b17-fine/NOTES.md.
   **The timing eye** (development only, for this item's run; no row, no option): `-TheaterAirTimeEye=box` rides a
   bomber in the middle of the largest bomber group from behind and above, as the close view rides a bomber (she counts
   as ridden, so she always has her close-up shape), her neighbours' close-up shapes filling the picture;
   `-TheaterAirTimeEye=far` holds the eye 1 km from that group's middle, level with it and abeam, looking at it;
   `-TheaterAirTimeEye=both` is box until two minutes after the eye takes hold, then far. Each holds from 2 s after the
   opening until the battle is over (the flyover, the picture run's rides and a click move the eye for nothing), then
   the run quits by itself 3 s later. The eye turns the speed log on (tc.Speed.Log 1, as -TheaterSpeedLog) and writes one
   line every 5 s: "Air battle: time eye box: F frames a second, worst frame W ms (the speed log's last 5.0 s); near
   shapes drawn N of M on their own meshes; close-up copies made C, of them the finer B-17's B (the most 8,
   -TheaterAirB17FineNear=N); far and light copies K; group G, Q bombers up." ("far" in place of "box" once it stands off.)
   The Mac's run (one sitting, the Mac on mains power with the window in front; 45 frames a second is the mark). The
   Bremen raid has 12 B-17s (the 303rd), not 50: what a 50-bomber raid costs stays arithmetic until such a recording
   is made (tools/air-record). Each launch runs the raid to its end, about 5 minutes:
   1. `-TheaterAirBattle=bremen-b17 -TheaterAirTimeEye=both -TheaterAirNoB17Fine`
   2. `-TheaterAirBattle=bremen-b17 -TheaterAirTimeEye=both -TheaterAirB17Fine`
   3. `-TheaterAirBattle=bremen-b17 -TheaterAirTimeEye=both -TheaterAirB17Fine -TheaterAirB17FineNear=1000` (no guard)
   4. stills, row on, by hand: `-TheaterAirBattle=bremen-b17 -TheaterAirB17Fine -TheaterAirNoseArt -TheaterAirNoseArtEye`
   5. earned colours still: `-TheaterAirBattle=unit_colours_raid -TheaterAirB17Fine -TheaterAirUnitColours` (the raid copied
      in locally as air-battles/unit-colours/r2/NOTES.md says; its 303rd flies B-17s).
   Pass: in runs 2 and 3, every "time eye" line's frames a second at 45 or more where run 1's is, box and far, and no
   worst frame over 100 ms after the opening that run 1 does not have. Send back from each log: every "Air battle: time
   eye" line (leave out the one that straddles the turn from box to far), the "Speed:" lines (game, draw and graphics
   chip ms, memory), "Air battle: finer B-17: on" and, if it comes, "...close-up shapes made, the most a battle keeps",
   and the warm-up's lines ("the raid's N aircraft shapes made ready ... in T ms" and "the opening held S s"): her file
   is read whole about four times, 10.3 MB, so the warm-up grows.
   By eye, row on (a still each, for the art director): a burning engine (fire and smoke on the engine that burns, not
   off the wing); the nose art close (run 4: her panel below the navigator's windows, smaller than the older one's,
   fading out as she draws off, not shimmering); the earned colours (run 5: her rivet and seam lines in the earned
   colour, not olive; past 300 m her spinners and nose band still there); the bomb bay opening and the bombs leaving it;
   the kill tally under her port window (new on screen: the older file had none); her airscrews each turning on its own
   engine; past 1.5 km her light shape (2,000 triangles); and, in run 2 if it comes, a finer B-17 the guard holds back
   within 300 m (standing blades, a closed bay) so that N can be judged. Off: the picture exactly as before.
61. **Turning airscrews** (release 2; Brian, 26 November 2026 04:00Z: "the b-17 propellers didnt look like they were
   spinning"; on 20 November "the japanese zero's propeller didnt spin"). Past 300 m from the eye (450 going out), the
   whole of the Overview (434 to 832 m from its crowd), every aircraft is drawn from its far shape, as a copy, and that
   shape kept every blade of every airscrew standing still; on the finer B-17 and on every newer model (the Bf 109 and
   Fw 190 among them) its modeller's see-through blur disc was drawn solid too (its file's "clear" is not read, and the
   copies' paint is opaque), dark, with the still blades through it. Only the near shape turned its discs. Looks only:
   nothing the rules count reads it.
   How: AAirBattle::PropSpinOn reads the Battle rules card's **Turning airscrews** (FLandLive's rule `flyPropSpin`; its
   key begins "fly" only so that FLandLive::Start passes it over; never sent to the land rules) once a battle, as its
   first shape is made. On, every shape is kept under a key of its own (`|screws`: the shapes are kept for the whole run,
   so a battle with the row off draws what it drew before). Its far shape loses its blades and its blur disc
   (AirScrew::StripScrews finds the far mesh's screws as the near mesh's are found: the same hubs on every aircraft,
   Tools/air_show/air_prop_spin_test.cpp), and keeps everything else (boss, hub, cowl, spinner); its light shape is made
   from that, with no haze (an airscrew is 2 or 3 pixels there). The copies draw each airscrew as an even, faint haze
   (AirScrew::MakeDisc, 24 steps round, 288 triangles, colour 222224, alpha 0/70/60/0 out from the boss) on
   M_ScrewHaze, M_Highlight's look flagged for instanced meshes (Tools/build_content.py builds it when it is missing; it
   must be built on the Mac and its .uasset committed: without it the log says so once and the far shapes keep their
   blades standing, as with the row off, so that no aircraft is ever drawn with no airscrew at all). Close to, the
   turning disc keeps its 576 triangles and trails a stronger smear behind each blade (darkest at the blade, a sharp
   edge ahead, a faint haze between), near black (8, 8, 8): both inks take a point's colour bytes as linear light, with
   no Power 2.2 as M_MapSurface has, so the old disc's 30 was a mid grey. An engine dead and feathered
   still shows its blades standing still: close to as before (BladesOf), and far off its bomber leaves the copies for
   its own mesh, its far shape with the feathered screw's own blades (kept apart as they were taken out, section 6) and
   the running ones' haze (section 5). The discs and the haze sort against the clouds as the air fight's effects do (0
   seen from over the clouds' base, -30 from under it); before, a disc's 0 drew it over every cloud, the ones hiding its
   own aircraft too.
   Cost (counted, not timed): no draw call more per aircraft; one see-through draw more per copies' mesh that has haze
   (its far shape with and without shadow: at most 2 a model, about 6 in the Bremen raid; with the row unitColours on,
   each colour look is a shape of its own, so 2 a look); the light copies none. A
   B-17's far copy 5,970 opaque triangles and 1,152 of haze, against 7,122; the finer B-17's 18,202 and 1,152, against
   20,514; a Bf 109's 4,454 and 288 against 4,770. Each far shape's file is read once more as it is made (the finer
   B-17's 10.3 MB), so the warm-up grows. A bomber feathered past 300 m: its own mesh, two or three draws, while it is
   in view, and its whole far shape even past 1,500 m (not the light one).
   What it promises, and what it does not: past 300 m, all of the Overview, a B-17's airscrew is 4 to 8 pixels across
   in the picture the game draws, and its haze does not turn: no frozen blades and no black discs, a faint even blur
   (12.6 and 11.6 levels darker than the sky at 434 and 832 m in the test's painter; against dark ground, about sRGB
   80, it comes out a few levels lighter than the ground, arithmetic). Whether that reads as a turning airscrew is the
   art director's call on the Mac. Only close to (rides, under 300 m) is a turn drawn.
   `-TheaterAirPropSpin` on for a run, `-TheaterAirNoPropSpin` off, `-TheaterRulesFlip=flyPropSpin` clicks the card's
   switch. The cloud test: air_prop_spin_test in run.sh (every aircraft in Content/Models and Old: one haze a screw, no
   blade or blur triangle left far off and nothing else taken, each screw's blades kept apart; the near disc changes the
   picture 64, 51 and 36 levels of 255 a frame at 60 fps at 50, 150 and 280 m against the old disc's 13, 12 and 11,
   which the test checks fall short of 30; the haze stands 12.6 and 11.6 levels off the sky at 434 and 832 m; the row
   off read from the source: those checks count lines of text, they show the guards are there, not how they behave, so
   the Mac's run 1 against a build of the branch's base is the real proof that off is byte for byte as before) and
   air_light_test built with -DAIR_LIGHT_SCREWS (every light shape, Content/Models and Old, held within half a pixel
   of the far shape without its airscrews).
   The pictures are the test's own software painter at the scene's real scale (about 1,050 pixels across, 1,008 a
   radian), not the Mac's: TSR may even out the near smear (see-through draws write no motion), which only the Mac shows.
   The Mac's run (build_content.sh first, for M_ScrewHaze): the Bremen raid, the Overview and a ride, at 1x and at 1/2
   speed (keys 2 and 1), each launch to its end:
   1. `-TheaterAirBattle=bremen-b17 -TheaterAirTimeEye=both -TheaterAirNoPropSpin`
   2. `-TheaterAirBattle=bremen-b17 -TheaterAirTimeEye=both -TheaterAirPropSpin`
   3. `-TheaterAirBattle=bremen-b17 -TheaterAirTimeEye=both -TheaterAirPropSpin -TheaterAirB17Fine`
   4. stills by hand: `-TheaterAirBattle=bremen-b17 -TheaterAirB17Fine -TheaterAirPropSpin`, and the same without
      `-TheaterAirB17Fine`.
   Pass: run 2's "time eye" lines' frames a second within 1 of run 1's, box and far, and no worst frame over 100 ms that
   run 1 does not have; the log says "turning airscrews: on" with M_ScrewHaze, and no "are not its near ones". Send
   back the "time eye" and "Speed:" lines (draw ms against run 1), the "turning airscrews" lines and the warm-up's lines.
   By eye (a still each, for the art director): at the Overview no still Y of blades and no black disc with a yellow rim
   on any B-17, Bf 109 or Fw 190, a faint grey haze at each engine; in a ride the near discs' smear seen to turn, not
   shimmering; a bomber crossing 300 m with no jump from a turning disc to still blades; a feathered engine's blades
   standing still, near and far; from under the clouds' base, the haze and discs behind a cloud, not over it (and, a
   guess: ground smoke or rain behind a bomber may draw over its discs there, as over the air fight's tracers today);
   no ghost trail behind the haze when the camera pans with the formation (TSR, a guess); no grey checkered discs
   anywhere (M_ScrewHaze missing on a copy). Off: the picture exactly as before, compared with the branch's base.

65. **Bomber markings** (release 2; Brian, 25 November 2026, of the approved studio pictures of the newer B-17: "Build it
   into the game"; 26 November: "why is the video showing the old b-17s again", "i want the new ones in the game"). The
   game held her shape only: plain olive and the star and bar. Now, with the Battle rules card's **Bomber markings** on
   (FLandLive's rule `flyBomberMarks`, off by default; its key begins "fly" only so that FLandLive::Start passes it over),
   every finer B-17 (the row b17Fine on) wears her group's white triangle with its black letter on her fin, both sides,
   and on her upper starboard wing (apex forward, the letter reading the right way round seen from above with her nose
   up), ridden, near and far: the 303rd Bomb Group's C, every other American B-17 group its own letter. Never by date:
   the star and bar is hers in every month (the art director, 26 November 21:42Z), and nothing here reads the month.
   Looks only: nothing the rules count reads it, and no random draw of the battle is made or moved.
   How: each mark is a small vertex-painted mesh in her model file's frame (Content/Data/AirMarks/b17-fine-marks.json,
   6.27 MB at bundle 2, made by `python3 -I Tools/air_marks/make_b17_marks.py --livery <B17Livery.json>` from the model thread's
   livery file, sha256 2ae760cc..., whose marks were laid on her newer shape 12 to 16 mm off her skin, near and far;
   the wing's triangle and letter are the tool's own, 1.5 m across at x 0.9, y 11.6 (the fin's 1.74 m would hang over
   her trailing edge there), the far glyph laid flat and draped 12 and 16 mm over a smooth fit of her wing; it refuses a
   model other than B17Fine.json sha256 5e1f39002423). Inks are the livery's own, the approved picture's: white DCD9CC,
   black 17171A; on PlaneInk as painted metal (alpha 128, as her star), matt as her paint: no new material, texture or
   content build. Each mark mesh ("unit.C": fin.T, fin.T.C, wing.T, wing.T.C) is made once (AAirBattle::MarkStill, by
   her file and frame, behind the opening's curtain in WarmMarks) and drawn as an instanced copy with her own place and
   her place a frame before (CopiesFor "marks|...", sent by SendCopies): the near level while she is on her near shape,
   the far one on her far shape and as a copy (her light shape past 1.5 km wears none). A mark is drawn while it is
   2.7 px or more on the screen (letters and codes 4.0; 0.9 of that while it was drawn, so that it does not flicker;
   `-TheaterAirMarksMinPx=` and `-TheaterAirMarksLetterMinPx=`), always on the ridden one. About 280 to 580 triangles a
   bomber at the far level (390 on average over the 24 letters), 1,400 to 3,000 at the near.
   Who wears which letter is Battle/AirLetters' book (std only): every American B-17 group lettered at the battle's start
   from the show's groups (history's first: the 303rd C and its squadrons VK BN PU GN; the rest by an FNV hash of the
   group's name, the next free of the 24 letters A to Z without I and O on a clash), each bomber given hers by her key's
   order in her group when the show first gives her, never changed after. From the show's aircraft alone, never the
   scene's (whose order of making can differ between two runs of one recording): the same battle wears the same letters
   on every run.
   `-TheaterAirBomberMarks` on for a run, `-TheaterAirNoBomberMarks` off, `-TheaterRulesFlip=flyBomberMarks` clicks the
   card's switch. Off (or the finer B-17 off): no file read, nothing made, placed or said; the card gains its line.
   On with the file missing or bad: one warning, and she flies with the star alone.
   The cloud test: air_letters_test in run.sh (under g++ and clang++): the file read as the game reads it, whole, made on
   her file (its sha256), wound as her file's, every fifth point 3 to 40 mm off its level's skin, by the signed distance to her nearest face (5.0 to 25.8 measured),
   her far side's letters and codes straight in from her side never under her, 2 to 125 mm off, over 40 mm one point in
   a hundred at most (3.7 to 116.5 measured: a tall letter's tail or top reaching round her curve),
   the letter F reading the right way round on both faces of her fin and on her wing from above, near and far, the wing's
   triangle apex forward on her right wing, nothing over her star and bar; each recording played through the show with the
   book taking every step, every third, every seventh from the fifth, and all hidden: the same wears; every American B-17
   given hers; one letter a group, none shared, no code in two groups; the 303rd's first bomber C VK A; the same wears with
   the date moved to 1943-03 and 1944-06; the show's own record the same with the book and without; thirty made-up groups
   numbered two ways; no month or year in the book; and the Unreal side's guards from its source (lines of text: the
   proof is the Mac's run with the row off against the base).
   Bundle 2 (the file's "bundle": 2, 194 parts, 6.27 MB; the bundle-1 parts in it byte for byte as before): her
   squadron's two letters and her own letter on both sides of her fuselage as the livery lays them, her own letter forward of
   the star and bar and the squadron's two aft of it (the 303rd's first bomber reads "A, star, VK" from her port side,
   each letter the right way round from either side), in the livery's code grey B9BAB4 (light grey, the livery's value "code"); her own letter again in
   the livery's yellow C99A35 on her fin under the triangle. Squadron and letter from the book: her n-th bomber of the
   group (n from 0, by key order) flies with squadron (n / 6) mod 4 and letter (n mod 6) + 6 x ((n / 24) mod 4) of the
   24; the codes of a group not known to history from an FNV hash, none in two groups. Her "#" number (CampaignMiniatures'
   paint) is not painted on a finer B-17 shape made with the row on at bundle 2: FMiniatureMesh::bNoPlaneNumber, set
   only in ShapeFor beside bTorpedoLoad, and her shape key ends "|letters" (before "|screws"), her paint key "|nonumber";
   off, every key and every paint is as before. The side letters J and Q (whose tails reached her wing root) are raised
   to the foot of A and laid again 12 mm off her skin, near and far. At the far level the side letters and codes are laid
   again as decals seen straight from her side (the tool's decal_side): her far side is a loft of a few flat faces, and
   the livery's far glyphs, whose long triangles crossed its ridges, let her skin stand through them between their
   corners (olive bites in the letters, seen in the check pictures); each is clipped to a grid whose lines stand at her
   own faces' corners, set 12 mm off the outermost of her faces straight in, a cell raised where her skin stands out
   over it (her wing fairing's edge), and her wing's root in front of the A's and J's feet left to hide them. About
   1,100 to 3,700 triangles a bomber at the far level (2,000 on average, her fin's and wing's triangles counted), 4,200
   to 7,000 at the near. Bundle 3 (the yellow serial, close up) is not built.
   The Mac's run (no build_content.sh: no new content): the Bremen raid with the finer B-17.
   1. `-TheaterAirBattle=bremen-b17 -TheaterAirB17Fine -TheaterAirNoBomberMarks -TheaterAirTimeEye=both`
   2. `-TheaterAirBattle=bremen-b17 -TheaterAirB17Fine -TheaterAirBomberMarks -TheaterAirTimeEye=both`
   3. the leg-144 raid with b17Fine, noseArt and flyBomberMarks on, and the same with flyBomberMarks off.
   4. stills by hand with the row on: the 303rd from the riding eye at about 40, 60 and 150 m, and one Overview still.
   Pass: run 1's log line for line as the base's (no "bomber markings" line); run 2 says "bomber markings: on ... bundle 2,
   194 parts read in N ms" and "N bombers lettered, M mark meshes made ready"; on the 144-aircraft raid the worst 5 s no worse
   than the row-off run's, and the draw thread at most 0.5 ms more in the steady stretch ("Speed:" lines). By eye: a white
   wedge with a dark C on her right wing seen from behind and above, a white triangle with C on her fin both sides,
   reading the right way round; nothing floating off her skin or sunk into it (a white sliver or a flicker of olive
   through the white would say her frame is wrong); no letters in the Overview, a small white fleck on each fin and wing.
