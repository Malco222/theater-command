#pragma once

#include "Battle/ShipGuns.h"
#include "Effects/TheaterFxCore.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

/**
 * The fleet action acted out, in plain C++: every ship of every piece as it steams, turns, fires, is hit, burns and goes
 * down, and what the effects kit and the sound show of it (salvos, columns of water round the target, hits, torpedo
 * tracks, the carriers' strikes, smoke screens, oil burning where a ship went down). The same code runs in the game
 * (ASeaBattle hands it the battle's views and draws what it says) and in the cloud, where Tools/sea_show plays a
 * recorded action through it and checks the seamanship (the ship behaviour plan's B0 and its look items L1 to L7).
 *
 * The battle (src/battle/sea) decides everything that counts: where each piece is, whom it fires on, what it loses and
 * when, which side breaks off. Nothing here changes that, and nothing here draws from the battle's luck. What is
 * decided here is only what the battle leaves open, with its own dice from the battle's seed:
 *  - L1: a piece of several ships steams as a column in line ahead, each ship at her own helm in her leader's wake,
 *    turning where he turned; every ship's turn and rudder no more than her class's at her speed; her bow wave and wake
 *    by her speed, her heel in a turn and on the swell, her turrets trained on the ship she fires at; salvos on her
 *    class's cadence, their columns falling round the target after the shells' flight, a hit for each share of hull the
 *    battle took off the target. Each column keeps its own water: where the battle lays two pieces on one spot, their
 *    columns take stations abreast, and every ship afloat, leader or follower, keeps clear of every other: the lighter
 *    of the heavier, astern in a line of those ahead, between ships alike by the rule of the road; she alters course or
 *    eases her way as she must, looking 45 s ahead, and comes back to her place once clear. A ship sunk or out of the
 *    fight sheers out of her line.
 *
 * The ships are steamed in fixed steps (Fixed, a twentieth of a second) whatever the frame rate, so the same battle is
 * the same show on any machine and in the cloud test.
 *  - L2: a carrier's strike flies off her deck, forms up, flies out, dives on the ship the battle names, and those left
 *    fly home and land on.
 *  - L3: a torpedo salvo's tracks fan out from each ship of the piece toward the battle's aim point; one ends against the
 *    ship the battle says was hit, the rest run on and fade. No track runs through a ship of its own side (Brian, 25
 *    November 12:52Z: "the friendly ships would pass though right in front of torpedoes"): each of a ship's tubes is
 *    held until its way to the aim is clear of every ship of her side (her column's ships ahead of her among them, and
 *    each as she steams on over the whole of the track's run to the aim, turning as she is or easing her helm, her way a
 *    little faster or slower than now as it may be), each tube looked at every quarter second and fired from the first
 *    place it is clear from: her own place while she is in her line, until a third of the run is gone; from then (or
 *    once she is out of her line) instead the salvo's firing point, the place the battle told the salvo from, where the
 *    rules' water starts, and before then too for a tube whose track from her own place would not come into that water
 *    within LookAhead (a track from the firing point starts in that water; one from her own place, at her side, comes
 *    into it within LookAhead). A boat fires from her bow only. It runs at the speed that brings it to the aim when the
 *    battle has the tracks arrive, never stopped short of the aim. A tube still held at two thirds of the run (too late
 *    for a track to run the third of it a hit's track has) is never drawn, and is judged over its hold: for the rules'
 *    side where at any judging (once a second) other pieces' ships in the salvo's water ahead of its heads were all that
 *    stood in its way (their own-water switch off lets them lie there); else as it is given up, for her own piece's ships
 *    alone or the scene's own otherwise. A track stops at the aim where an own ship lies in its run on, or as one comes across it
 *    there. A hit ends the track of the launching piece nearest her that has run a third of the run with its way on to
 *    her clear of its own side (TrackForHit). Where and when a track is drawn from only: the room, the ship a hit
 *    strikes and the dice are kept as the tracks fired at the battle's moment would have them (their shadows).
 *  - L4: a ship out of the fight burning leaves her line and limps away from the enemy, on fire.
 *  - L5: smoke screens: destroyers turning away from their run, light ships breaking off, the screen of a carrier under
 *    gunfire. Drawn only: guns still fire through it, as the battle has them.
 *  - L6: one wind a battle (from its seed: the recording's field.wind, or a hash): smoke leans with it, the sea's state.
 *  - L7: before the guns meet each side zig-zags together, legs of about a minute either side of its course; from
 *    contact it steadies on the battle's course.
 *  - L8 (ships under air attack, release 2: the historian's naval-battles/ship-air-defence.md, B4 and B5.4-5): the big
 *    ship a dive is aimed at puts her helm hard over about half a minute before the bombs fall, to the side it comes
 *    from where she has the room; every ship combs a torpedo plane's drop, bow or stern on; a column's leader leaves a
 *    ghost on its course for his line to keep and comes back to it, the ghost kept until she is on it. The hard over is
 *    held no more than 50 degrees off her course, the comb is the one nearer her course (not her bow), a follower cuts
 *    back to his track at 35 degrees at most, and one who comes back astern of the next in her line falls in astern of
 *    her. Each dive bomber's hit comes with its two wing bombs as near misses close aboard; a dive bomb's miss falls
 *    close. The battle's hits and misses stand as it booked them: this is how she fights for her life as they fall.
 *    Since: a leader fallen back level with her next astern hands her the lead and falls in where she can reach (a new
 *    leader still dodging leads from her place on the track, not from where her dodge took her); a comb is given up
 *    once her course has turned 110 degrees from it, and one off her place across her course combs back toward it
 *    where the two lie near abeam; a dodge is judged only if her helm bit, and a judging is put off only while she
 *    gains on her place, five minutes at most.
 *  - L9 (ships fire back, release 2: Brian 21 November 18:06Z, the shared Battle/ShipGuns): every ship's guns of her
 *    navy and month at the other side's aircraft in reach, laid twenty times a second: light guns' streams of tracer,
 *    heavy guns' rounds bursting about the aircraft or thrown into the sea ahead of a torpedo plane, rounds striking
 *    home now and then. Looks only: the battle decides every loss.
 *
 * Places are in board metres (X east, Y north, Z up from the sea), the battle's own board. Headings are radians
 * clockwise from north, as the battle's own. Time is the battle's clock.
 */
namespace SeaShow
{
	using Vec = TheaterFx::Vec;
	using Dice = TheaterFx::Dice;

	/** A class's size and handling, as the battle's numbers give them (data/battle/sea.json classes). */
	struct ClassSize
	{
		double Long = 180.0, Beam = 20.0;
		double Speed = 13.0;  // metres a second at cruise
		double Turn = 0.08;   // radians a second at full helm, the battle's
		double Reach = 5000.0;
	};

	/** What arrives once, at the start (TC.seaStart's field, and the recording's own additions). */
	struct Field
	{
		double Size = 20000.0, Edge = 5000.0, Limit = 1500.0;
		uint64_t Seed = 239;
		std::string ZoneName, Title, Date;
		std::string AttNat, DefNat, AttAdj, DefAdj;
		int Year = 1941, Month = 4; // Month from 0 (January)
		bool bWhen = false;         // whether the battle gave its year and month (field.when): else the date is not known
		double Lat = 45.0, Lon = 0.0;
		/** The look's wind (L6): where it blows from, radians clockwise from north, and metres a second (SeaSim.wind and
		 * windSpeed). A speed of -1: none given, the show takes both from the seed; -2: the bearing alone was given. */
		double WindFrom = 0.0, WindSpeed = -1.0;
		double TorpTravel = 75.0, TorpRadius = 800.0, StrikeWindup = 90.0, StrikeRadius = 1500.0;
		std::map<std::string, ClassSize> Classes; // "bb", "cv", "ca", "dd"; a convoy's own also "ss", "mv"
		/** A convoy's night (ruling 241, its header light{sun, moon, vis}): whether it was given; the sun's height and the
		 * moon's (degrees, below 0 under the horizon), the moon's bearing (degrees from north) and the share of her lit;
		 * how far a ship is seen (metres, 0 not said). Look only, apart from vis, which the battle drew. */
		bool bLight = false;
		double SunUp = 90.0, MoonUp = -90.0, MoonFrom = 0.0, MoonLit = 0.0, Seeing = 0.0;
		/** Boats (the header's subs{scope, deep, crush, dived}): the keel's depth at periscope depth, running deep, and where
		 * her hull fails (metres below the surface); her best speed dived (metres a second: a Type VIIC's 7.6 knots). */
		double ScopeKeel = 13.0, DeepKeel = 60.0, CrushKeel = 250.0, DivedTop = 3.9;
		/** Depth charges (dc{sink, lethal}): how fast one sinks (metres a second) and how near it kills (metres). */
		double ChargeSink = 3.0, ChargeLethal = 6.0;
		/** The men in the water: the most the show keeps at once (each one man while there is room: Brian's standing rule,
		 * 22 November 04:14Z, the real numbers; past it each ship's men are thinned, and the share is told). */
		int MenMost = 2400;
		/** Whether the battle was fought with the naval rules' "Friendly torpedoes" switch on (field.ownWater, where the field
		 * says it: TC.seaStart's, or a stand-in's; the rules' recorder writes no mark of it). The torpedo words' gate
		 * (SeaFeed::FriendlyTorpedoes); the show itself never reads it. */
		bool bOwnWater = false;
	};

	/** One piece as a view gives it (battle-bridge.ts unitOut for a sea piece). */
	struct Piece
	{
		int Id = 0;
		int Side = 0; // 0 the attacker, 1 the defender
		std::string Class, Nation, Name, Fleet, State, Act, SpeedOrder;
		double X = 0.0, Y = 0.0, Face = 0.0, Str = 1.0, Mor = 1.0, Speed = 0.0, Depth = 0.0;
		int Ships0 = 1, Ships = 1, Sunk = 0, Burnt = 0;
		int Engaged = -1;  // the enemy piece its guns are on, -1 none
		int Run = -1;      // the enemy piece it is running in on with torpedoes, -1 none
		bool bScreen = false;
		double Planes = 1.0;
		bool bStrikeWinding = false;
		/** The battle's own look fields (the ship plan's B1, sim.ts, from claude/project-thread-ty2kvs's look head): set
		 * when the view carries them, and then read in place of the show's own guesses. */
		bool bLooks = false;
		int Foe = -1;           // the enemy piece whose bearing set its course (-1: an order, its line or the edge did)
		double Helm = 0.0;      // -1 to 1, plus to starboard
		std::string RunPhase;   // "in", "away" on a torpedo run, else empty
		double RunUntil = -1.0;
		bool bSmoke = false;    // laying smoke
		int LineAt = -1;        // its place in its side's battle line, 0 the guide (the van), -1 in none (the ship plan's R1)
		std::string Flight;     // carriers: "deck", "out", "back"
		double Cover = -1.0;    // carriers: 0 to 1, the share of its fighters held over the fleet still up
		/** A boat (class "ss", ruling 241): "surfaced", "periscope" or "deep" (empty: a ship), her keel's depth (metres,
		 * below 0 not said: as her state has it), her periscope up. A merchant ("mv"): her column in the convoy (-1 none). */
		std::string Dive;
		double Keel = -1.0;
		/** A boat's keel as the battle tells it changing, view by view (the convoy field's own rise, 2 m/s blown, and her
		 * going down): metres a second to the next view, plus down, set by the show between two views that both tell a keel
		 * and lie no faster than a boat goes (Show::Interpolate); 0 for a steady depth, or a jump (the stand-ins' told step
		 * from deep to the surface), which she makes at her own rates. Never read from a view. */
		double KeelRate = 0.0;
		/** With a KeelRate, the next view's keel (metres): where the change told so far may end, the show holding one view in
		 * hand and no more (below 0 none). She keeps where she can stop there (Show::Trim). Never read from a view. */
		double KeelNext = -1.0;
		bool bScope = false;
		int ConvoyColumn = -1;
		/** The torpedo words' fields (the naval rules behind their "Friendly torpedoes" switch, 25 November; Battle/SeaWordsCore
		 * words them): the own piece in its line of fire it holds its salvo for (heldFor, -1 none) and that piece as the rules
		 * name it; the own piece whose running salvo it turns clear of (clearFrom, -1 none), the battle second its heads
		 * pass her and the launcher as the rules name it; under an order of the player's own (ordered). As each view says (none
		 * without the switch), but for the held ones once the piece's salvo is told after its view (the rules have let it
		 * fire: SalvoToldAt); the show itself never reads them. */
		int HeldFor = -1, ClearFrom = -1;
		std::string HeldForName, ClearFromName;
		double ClearUntil = -1.0;
		bool bOrdered = false;
		/** The view reached holds her (heldFor), but her salvo has been told since, so her held fields are taken off (above):
		 * the torpedo words' alone (Battle/SeaWordsCore: until the view that ends it, the view's hold still wins over its own
		 * turn clear while the scene holds some of her tubes); never read from a view, and the show itself never reads it. */
		bool bHeldLetGo = false;
		/** Whether the player's side has her (Show::SetMine), where the view says it. A boat: the escorts of the other side
		 * running in on her now (heard, their pieces; bHeard once a view has said it, and then it stands as last told), and
		 * the enemy piece her deck gun is on while she is up (gun, -1 none: as each view says). An escort: the piece its echo
		 * is on and how ("asdic", "radar", "sight", "bearing"; echo {on, kind}, -1 and empty without one: as each view says).
		 * U1 hook (under water): the view under the water never reads heard itself; it asks the show (ShipOut::bShown). */
		std::vector<int> Heard;
		bool bHeard = false;
		int GunOn = -1;
		int EchoOn = -1;
		std::string EchoKind;
		/** A boat's deck gun or an armed merchant's gun (the convoy battle's "gun", above): its rounds are fired only while a
		 * view names whom it is on, never on "engaged", which is a warship's line's. Its bore where the battle names it
		 * (gunMm: a Type IX's 105 mm against a Type VII's 88; as last told), else 0 and the show's table by navy and year. */
		double GunMm = 0.0;
	};

	/** One event of a view, with what the show needs of its extra. */
	struct Event
	{
		double T = 0.0;
		std::string What, Cue, Text;
		int Unit = -1, Side = -1;
		double X = 0.0, Y = 0.0;
		bool bAt = false;
		std::string Key;      // ability: "strike"
		int Target = -1;      // ability (strike), sea:torpedo: the enemy piece
		int Count = 0;        // ability (strike): bombers flown off
		int From = -1;        // sea:torpedoHit: the launching piece
		double AimX = 0.0, AimY = 0.0; // sea:torpedo: where the salvo is aimed
		bool bAim = false;
		double Running = 0.0; // sea:torpedo: seconds it runs
		double Flown = -1.0, Landed = -1.0; // sea:strike: the weight flown and landed
		double Denied = 0.0, Share = 0.0;   // sea:cap (share turned back), sea:flak (weight denied)
		std::string Cls;      // sea:sunk, sea:burning: the class lost
		// The battle's look extras (B1), where the view carries them:
		/** sea:line: why the line formed or turned together ("form", "edge", "crossed"), its hand (+1 starboard, -1 port
		 * of the bearing to the enemy) and its new course (radians, a field bearing). */
		std::string Why;
		int Hand = 0;
		double Course = 0.0;
		/** sea:sunk, sea:burning: her own hull as she left the fight (0 to 1; the ship plan's R2 takes a ship out at half),
		 * -1 when the battle does not say. */
		double Hull = -1.0;
		bool bLimp = false;   // sea:burning: her course home (radians) and speed (m/s)
		double LimpCourse = 0.0, LimpSpeed = 0.0;
		bool bShare = false;  // sea:torpedo: how full the bank was, 0 to 1 (Share): that share of the tracks, none at 0
		std::vector<int> Ships; // sea:flak: the gun ships firing
		bool bDitch = false;  // sea:recover: the carrier has gone down, her aircraft come down in the sea
		// A convoy's night (ruling 241: form.md §5, field-side-read.md §3), where the event gives them:
		std::string Dive;     // sea:dive: the state she goes to ("surfaced", "periscope", "deep"); Why holds why
		double Keel = -1.0;   // sea:dive, sea:depthChargeHit: her keel's depth (metres), below 0 not said
		std::string Kind;     // sea:depthCharge: "pattern" or "ahead"; sea:sonar: "asdic", "radar", "sight", "bearing"
		int On = -1;          // sea:depthCharge, sea:sonar: the piece it is on; sea:rescue: the piece lost; sea:report: the piece reported
		int N = 0;            // sea:depthCharge: charges in the pattern
		double Set = -1.0;    // sea:depthCharge: the depth they are set to burst at (metres), below 0 not said
		double Sinking = -1.0; // sea:depthCharge: seconds from the drop to the burst, below 0 not said
		double Spread = 0.0;  // sea:depthCharge: the pattern's spread across (metres)
		double Harm = 0.0;    // sea:depthChargeHit: the share of her the blow took (1: the kill)
		std::string How;      // sea:sunk: how she went, where the battle says ("crushed", "scuttled", "rammed", "shelled", ...)
		std::string Lost;     // sea:sonar: why a contact was lost ("close", "deep", "wake", "faded"); empty: one gained
		int Fish = 0;         // sea:torpedo from a boat: the torpedoes of her salvo (0: not said)
		double Fan = 0.0;     // sea:torpedo from a boat: the salvo's spread, degrees across
		bool bWakeless = false; // sea:torpedo: electric torpedoes, leaving almost no track
		/** sea:surrender (the naval rules, behind their subSurrender switch): the piece she strikes to, an escort or the
		 * aircraft that forced her up (-1 none said). Her How is "scuttled" (her crew send her down; "scuttle" read the same)
		 * or "captured" (a boarding party takes her). Her own piece is Unit, so this is read by name, never through it. */
		int By = -1;
		// The men in the water (the naval rules' sailor events, 22 November: sea:men, sea:oil, sea:abandon, sea:rescue), where
		// the event gives them. A wreck is keyed {piece, ship}: the piece's first ship lost is 1, the next 2.
		int Ship = 0;         // sea:men, sea:oil, sea:abandon: which of the piece's ships lost (0 not said)
		int Men = -1;         // sea:men: alive in the water; sea:rescue: picked up so far (below 0 not said)
		int Dead = -1;        // sea:men: killed going in, their bodies afloat (below 0 not said)
		int Guests = 0;       // sea:men: men she had picked up, back in the water with her own (the naval rules, 18:42Z)
		double OilR = -1.0, LitR = -1.0, OilUntil = -1.0; // sea:oil: the slick's spread and its burning part's (metres), and when it burns out
		int WreckOf = -1, WreckShip = 0; // sea:rescue: the wreck she is picking up from ({piece, ship}), -1 not said
		bool bDone = false;   // sea:rescue: she has finished (left, the wreck empty, or the fight over)
	};

	/** A smoke zone (a ship lost: 'sunk', where she went down; 'burning', where she left the line) or a torpedo salvo's
	 * aim ('torpedo'). */
	struct Zone
	{
		int Id = 0;
		std::string Kind;
		double X = 0.0, Y = 0.0, R = 0.0, Until = 0.0, At = 0.0;
		int Side = -1, Unit = -1;
	};

	enum class Fate { Line, Limping, Sinking, Gone };

	/** One ship as she is to be drawn. */
	struct ShipOut
	{
		int Key = 0;         // her own number: the piece's id times 64 plus her place in its line at the start
		int Piece = 0, Side = 0, Slot = 0;
		std::string Class, Nation;
		Vec At;              // her waterline amidships (Z 0 afloat)
		/** Degrees: heading clockwise from north; heel (plus: starboard side down), pitch (plus: bow up). */
		double Heading = 0.0, Heel = 0.0, Pitch = 0.0;
		/** Degrees, plus bow up: a boat's angle diving or surfacing, her pitch less the sea's (0 for a ship). */
		double DiveAngle = 0.0;
		double Heave = 0.0;  // metres: how far the swell lifts her (plus) or lets her fall from her waterline
		double Speed = 0.0;  // metres a second through the water (below 0 going astern: a boat deep the battle turns round)
		double Wake = 0.0;   // 0 to 1: her bow wave and wake, by her speed against her flank speed
		double Funnel = 0.0; // 0 to 1: how hard her funnels smoke (black at flank, thin at rest)
		double Long = 180.0, Beam = 20.0;
		Fate Doing = Fate::Line;
		std::string How;     // sinking: settle, sink, capsize, explode, burn, break; a boat also crushed, scuttled, rammed, shelled
		double Down = 0.0;   // sinking: 0 as she starts to go, 1 under
		double DownFor = 0.0; // sinking: seconds from the first to the last
		/** Her main turrets: their bearing off her bow (degrees, clockwise) and elevation (degrees); the ship they are
		 * laid on (its Key, -1 none) and whether a salvo went this frame. */
		double Train = 0.0, Elevation = 0.0;
		int Aim = -1;
		bool bFiring = false;
		/** A boat's deck gun or a merchant's gun (the convoy battle's "gun": one gun, Train and Elevation above its own):
		 * whether it is her after one (an American fleet boat's, aft of her tower; a merchant's stern gun, which takes any
		 * target it bears on), whether it is laid (her gun told on a ship she can fire at while she is up, her casing clear:
		 * its crew at it), when a shell last struck her or fell close aboard her, and when her guns last fired (battle time;
		 * below 0, never: the scene runs a gun back as it fires). */
		bool bAftGun = false, bGunLaid = false;
		double ShakenAt = -1.0, FiredAt = -1.0;
		bool bBurning = false;
		bool bScreen = false; // laying smoke
		int LineAt = -1;      // in her column, her piece's place in its side's battle line (0 the guide), -1 in none
		bool bShown = true;
		/** A boat (class "ss"): the state the battle has her in ("surfaced", "periscope", "deep"; empty for a ship), her
		 * keel's depth below the surface as drawn now (metres: eased, so a crash dive takes 25 to 30 s), her periscope up.
		 * Her heave and pitch are her ride (the swell fading as she goes down) and her angle diving or surfacing. */
		std::string Dive;
		double Keel = 0.0;
		bool bScope = false;
		double Dived = 0.0;  // metres her hull lies below her surfaced trim (her keel less her draught): 0 surfaced
		/** A boat that struck (sea:surrender): seconds since her crew began coming up and her white flag went up (below 0:
		 * she has not); seconds since it came down (below 0: it has not; a prize's as her boarders take her, a scuttled
		 * boat's as she goes); a prize, taken (she stays afloat as she was, her Down held). The convoy looks draw the flag. */
		double Struck = -1.0, Lowered = -1.0;
		bool bPrize = false;
	};

	/** One aircraft of a carrier's strike or a raid from the coast, as it is to be drawn. */
	struct PlaneOut
	{
		int Key = 0;
		int Side = 0;
		std::string Model, Nation, Role; // Role: "dive", "torpedo", "level"
		Vec At;
		double Heading = 0.0, Pitch = 0.0, Bank = 0.0; // degrees
		double Speed = 0.0;
		bool bFalling = false;
		bool bShown = true;
	};

	/** A torpedo running: where its track's head is now. */
	struct TorpOut
	{
		int Key = 0;
		int Side = 0;
		Vec From, At;
		double Heading = 0.0; // radians
		double Speed = 0.0;
		/** The piece that fired it, the ship whose tubes it left (her key; -1: fired from its salvo's firing point, she
		 * not lying at it), where its run ends as it was fired, whether its head is past the battle's aim and its moment
		 * (running on, the scene's own water: the rules keep own ships out of a salvo's water only up to its aim), and
		 * whether it was fired from its salvo's firing point (the place the battle told the salvo from, where the rules'
		 * water starts: From lies in that water). */
		int Piece = -1, Ship = -1;
		Vec End;
		bool bRunOn = false, bFromPoint = false;
	};

	/**
	 * What the scene shows of a piece's torpedoes now, for the torpedo words (Battle/SeaWordsCore; Show::TubesNow): read
	 * only, the show itself never reads it. Its latest salvo told (its number in the order told, 0 none) and the tracks
	 * drawn of it so far (each in the water from its launch); its tubes the scene still holds, of any of its salvos (the
	 * scene's own holds, after the rules let a salvo go: an own ship in the way from where she fires), and the tracks drawn
	 * of the salvos it holds them of; and the own piece whose ship stands across its held tubes' way, as each of its ships
	 * last looked (the first own ship found across each held tube's way, at the first place a ship holds it: the piece
	 * standing across the most of them, another piece before its own, then the lowest; -1 none found).
	 */
	struct TubesOut
	{
		int Salvo = 0, Fired = 0;
		int Held = 0, HeldFired = 0;
		int Across = -1;
	};

	/** Words the battle has said of a piece (the torpedo words' phrases; the show itself never reads them): the battle
	 * second they were said and the words. */
	struct Phrase
	{
		double T = -1.0;
		std::string Text;
	};

	/**
	 * Where a ship went down (Brian, 22 November 05:32Z to 05:33Z: "sailors should jump in the water if the boat is going
	 * down", "have them use the models we have for the soldiers for the sailors", "there should be debris floating in
	 * the water. maybe oil pools?"). Her crew, her boats and rafts, her wreckage and her oil: the show's looks only. The
	 * battle books every loss and every life; nothing here is counted.
	 */
	struct FloatOut
	{
		int Key = 0;          // its own
		int Ship = 0;         // the ship it came from
		/** "plank", "spar", "crate", "drum", "hatch", "ring" (a life ring), "raft" (a Carley float or life raft), "boat" (a
		 * whaleboat or lifeboat) */
		std::string Kind;
		Vec At;               // its middle, on the sea (Z 0)
		double Heading = 0.0; // radians, its long way
		double Long = 1.0, Beam = 0.5;
		int Aboard = 0, Holding = 0; // men in it, men holding on to it
		double Way = 0.0;     // a boat: her own way through the water under oars, metres a second (0: adrift)
	};

	/**
	 * A pulling boat's lines, as the scene draws her (SeaBattle's DrawBoat): her half breadth at her gunwale and her
	 * gunwale's height over her waterline at her stations, U from -1 (her stern) to 1 (her bow), BoatStations of them,
	 * double-ended, her sheer rising toward both ends, flat between stations; her side running in to 0.84 of the
	 * gunwale's breadth 0.12 m under the water.
	 */
	constexpr int BoatStations = 8;
	inline double BoatHalf(double U, double Beam)
	{
		const double A = U < 0.0 ? -U : U;
		const double In = 1.0 - std::pow(A, 2.4);
		const double H = 0.5 * Beam * std::sqrt(In > 0.0 ? In : 0.0);
		return H > 0.03 ? H : 0.03;
	}
	inline double BoatSheer(double U)
	{
		return 0.5 + 0.2 * U * U;
	}
	/** Her side's half breadth and her gunwale's height at U as drawn: straight between her stations. */
	inline void BoatDrawn(double U, double Beam, double& OutHalf, double& OutSheer)
	{
		const double Where = (std::clamp(U, -1.0, 1.0) + 1.0) * 0.5 * BoatStations;
		const int K = std::min(BoatStations - 1, static_cast<int>(Where));
		const double T = Where - K, U0 = -1.0 + 2.0 * K / BoatStations, U1 = -1.0 + 2.0 * (K + 1) / BoatStations;
		OutHalf = BoatHalf(U0, Beam) + (BoatHalf(U1, Beam) - BoatHalf(U0, Beam)) * T;
		OutSheer = BoatSheer(U0) + (BoatSheer(U1) - BoatSheer(U0)) * T;
	}

	/**
	 * Her grab line (the art director, 22 November 13:47Z): a dark rope along each side in festoons, made fast GrabFast
	 * under her gunwale and dipping between to GrabLow over her waterline, about a metre a loop (GrabLoops a side), from
	 * GrabFrom of her length aft of her middle to as far forward. GrabThick: the rope's half thickness.
	 *
	 * A man in the water holding on to her (GrabHold) holds a loop's lowest point. The sailor files' cling clip has his
	 * forearms out in front of him, his fingers GrabHandAhead ahead, GrabHandSide to either side and GrabHandUp over his
	 * root, and his head leaning out past them (0.39 to 0.49 m ahead): square to her side with his hands on the rope his
	 * face went 12 to 15 cm into her planking (the review of 22 November). So he lies GrabTurn off square, turned toward
	 * her bow, his inboard hand on the rope and his head and his other arm clear of her side, his root GrabOut out from
	 * the rope's dip (every point of every file's clip 2 cm or more clear of her planking, bench/side probe); and the loop
	 * he holds is pulled out from her side and lifted to his hand (GrabAt's Held): GrabPull out and GrabLift up at its dip,
	 * falling away to nothing GrabPullHalf either side of it, so that the rope runs back to her side under his head and
	 * along under his forearms as it lay (pulled out along the whole loop it ran through his head and his inboard forearm:
	 * the review of 22 November; narrowed, the rope's centre is 3 cm or more from his head and 4.5 cm from his arms in
	 * every file's clip, and his inboard fingers within 7 mm of the dip: ropefix probe). DrawBoat samples a held loop at
	 * GrabPullAt's points too, so that the pull is drawn as GrabAt has it.
	 */
	constexpr double GrabLoop = 1.1, GrabLow = 0.08, GrabFrom = 0.38, GrabFast = 0.06, GrabThick = 0.012;
	constexpr double GrabHandAhead = 0.34, GrabHandSide = 0.17, GrabHandUp = 0.11;
	constexpr double GrabTurn = 35.0 * 3.14159265358979323846 / 180.0, GrabOut = 0.49;
	/** His inboard hand from his root, in her frame (sin and cos of GrabTurn written out): along her toward her bow, and in
	 * toward her side. */
	constexpr double GrabHandAlong = GrabHandAhead * 0.573576436351046 - GrabHandSide * 0.819152044288992;
	constexpr double GrabHandIn = GrabHandAhead * 0.819152044288992 + GrabHandSide * 0.573576436351046;
	constexpr double GrabPull = GrabOut - GrabHandIn, GrabLift = GrabHandUp - GrabLow, GrabPullHalf = 0.06;
	/** The points a held loop is drawn at besides its own (GrabAt's pull), as shares of GrabPullHalf from its dip. */
	constexpr double GrabPullAt[] = { -1.0, -0.75, -0.5, -0.25, 0.25, 0.5, 0.75, 1.0 };
	inline int GrabLoops(double Long)
	{
		const int N = static_cast<int>(2.0 * GrabFrom * Long / GrabLoop + 0.5);
		return N < 1 ? 1 : N;
	}
	inline double GrabDip(double Long, int I)
	{
		return GrabFrom * Long * (-1.0 + 2.0 * (I + 0.5) / GrabLoops(Long));
	}
	/** The loop of her grab line at Along her (metres from her middle), from 0 aft. */
	inline int GrabLoopAt(double Long, double Along)
	{
		const int N = GrabLoops(Long);
		const double From = -GrabFrom * Long, Span = 2.0 * GrabFrom * Long / N;
		const int K = static_cast<int>(std::floor((std::clamp(Along, From, -From) - From) / Span));
		return std::clamp(K, 0, N - 1);
	}
	/** A point of her grab line on a side (Hand 1 starboard, -1 port), Along her from her middle (metres): X along, Y
	 * across, Z over her waterline, in her own frame; a hanging rope's curve from where it is made fast down to each
	 * loop's middle, lying against her side as drawn. Held: the loops of that side a man holds (bit K for loop K), each
	 * pulled out to his hand. */
	inline Vec GrabAt(double Long, double Beam, double Hand, double Along, uint32_t Held = 0u)
	{
		const int N = GrabLoops(Long);
		const double From = -GrabFrom * Long, Span = 2.0 * GrabFrom * Long / N;
		const double At = std::clamp(Along, From, -From);
		const double Whole = (At - From) / Span;
		const int Index = std::clamp(static_cast<int>(std::floor(Whole)), 0, N - 1);
		const double Loop = Whole - Index;
		const double U = At / (0.5 * Long);
		double Half = 0.0, Sheer = 0.0;
		BoatDrawn(U, Beam, Half, Sheer);
		const double Top = Sheer - GrabFast;
		const double Mid = 2.0 * Loop - 1.0;
		const double Z = GrabLow + (Top - GrabLow) * Mid * Mid;
		const double T = std::clamp((Z + 0.12) / (Sheer + 0.12), 0.0, 1.0);
		const double Off = std::fabs(Mid) * 0.5 * Span;
		const double Pulled = (Index < 32 && ((Held >> Index) & 1u) != 0u && Off < GrabPullHalf) ? (1.0 - Off / GrabPullHalf) * (1.0 - Off / GrabPullHalf) : 0.0;
		return Vec(At, Hand * (Half * (0.84 + 0.16 * T) + GrabThick + GrabPull * Pulled), Z + GrabLift * Pulled);
	}
	/** Where her holder Slot of Hold (from 0) is, in her own frame (X along, Y across, Z 0): his inboard hand on a loop's
	 * lowest point, as pulled out to him, the holders spread along her, starboard and port by turns. He faces GrabTurn
	 * off square to her side, toward her bow (the show's heading for him). */
	inline Vec GrabHold(double Long, double Beam, int Slot, int Hold)
	{
		const double Hand = Slot % 2 == 0 ? 1.0 : -1.0;
		const int PerSide = std::max(1, (Hold + 1) / 2), Loops = GrabLoops(Long);
		const int Loop = std::clamp(static_cast<int>(std::lround((Slot / 2 + 0.5) * Loops / PerSide - 0.5)), 0, Loops - 1);
		const Vec Dip = GrabAt(Long, Beam, Hand, GrabDip(Long, Loop));
		return Vec(Dip.X - GrabHandAlong, Dip.Y + Hand * GrabOut, 0.0);
	}
	/** The loop a holder at (X, Y) in her own frame holds (his inboard hand's), and her side (Hand 1 starboard, -1 port). */
	inline int GrabHeld(double Long, double X, double Y, double& OutHand)
	{
		OutHand = Y >= 0.0 ? 1.0 : -1.0;
		return GrabLoopAt(Long, X + GrabHandAlong);
	}

	/** Her oil on the sea: an oval drawn out down the wind, spreading and thinning as it ages, alight in part for a while. */
	struct SlickOut
	{
		int Key = 0, Ship = 0;
		Vec At;               // its middle
		double Heading = 0.0; // radians: its long way (down the wind)
		double Long = 0.0, Wide = 0.0; // metres from edge to edge, along and across
		double Dark = 0.0;    // 0 to 1: how thick it shows
		double Burning = 0.0; // 0 to 1: the share of it alight (0: none)
		double Age = 0.0;     // seconds since it began
	};

	/** What a man in the water is doing (his clip: the soldier files' sailors, the soldier modeller's, 22 November). */
	enum class Swim : uint8_t
	{
		Jump,  // over her side, falling: At.Z his feet's height above the sea, Drop the height he went from
		Swim,  // swimming (swim_breast)
		Tread, // treading water (tread; wave now and then)
		Cling, // holding on to wreckage or a raft's lines (cling)
		Raft,  // in a raft (sit_raft)
		Boat,  // in a boat (row)
		Climb, // up a rescuer's side (climb_net): At.Z his feet's height
		Dead,  // face down (float_dead)
		// On a struck boat's casing (sea:surrender): Show::Hands() only, never Men(); At.Z his feet's height over the sea
		Hatch,   // coming up out of a hatch, or down the side of her conning tower from her bridge (climb_net)
		Stand,   // standing at his place on her casing (idle)
		Walk,    // walking along her casing to his place (walk: a stride of 1.42 m each 1.133 s, so 1.25 m/s)
		HandsUp, // standing with his hands up, a ship of the other side within a kilometre of her (idle until his own clip)
		Run,     // his last steps out to the edge of her casing before he goes over (run)
	};

	/**
	 * A boat's casing where her crew stand when she strikes (sea:surrender), the stage's model of her navy in its own frame
	 * (metres: along her forward from amidships, across to starboard, up from her waterline), as ConvoyLook::SculptedOf
	 * rows it: her bounds, her conning tower's fore and aft ends and its half-width at her casing, her bridge's floor (its
	 * rim less a 1.2 m bulwark), her fore and after hatches, the stretches of her casing fore and aft of her tower clear of
	 * her guns, how far out on it a man can stand before her saddle tanks fall away, and its height: Deck amidships,
	 * rising Rise a metre forward of RiseFrom, falling Fall times the square of the way across; and her deck gun's length
	 * along her, its mount and its barrel stowed, which her men keep clear of; and the model's top over her waterline (her
	 * periscopes', the stage's High for her, which its sinking takes her down by). Tools/convoy_looks checks these against
	 * the model files and the looks' own rows (ConvoyLook::SculptedOf), so her men stand on her deck as it is drawn.
	 */
	struct BoatDeck
	{
		double Long = 67.0, Wide = 6.79;
		double TowerFore = 3.5, TowerAft = -6.5, TowerHalf = 1.3, Bridge = 4.1;
		double ForeHatch = 8.0, AftHatch = -12.2;
		double AftFrom = -20.0, AftTo = -7.5, ForeFrom = 4.5, ForeTo = 10.0;
		double Edge = 1.2;
		double Deck = 1.58, RiseFrom = 3.0, Rise = 0.0087, Fall = 0.051;
		double GunFrom = 10.85, GunTo = 15.55;
		double Top = 10.49;
	};
	/** The row for a navy (GER, USA, any other or none: the stage's three boats, AirStage's ShipFileOf). */
	const BoatDeck& DeckOf(const std::string& Nation);
	/** Her casing's height over her waterline at a place in the row's frame (along, across: metres). */
	double CasingAt(const BoatDeck& D, double Along, double Across);
	/**
	 * The stage's way "sink" (AirStage::MoveShip, at its look's usual pace), T seconds after the stage was told it: how far
	 * down her waterline is (metres) and her trim (degrees, plus bow up) as she goes by the stern (bByStern; a boat her crew
	 * scuttled after she struck, the stage's end "stern"), or none where her end is the stage's own. High: the model's top
	 * over her waterline (BoatDeck::Top). A struck boat's crew still on her ride her down on it (Show's HandAt).
	 * Tools/convoy_looks holds this to the stage's own lines, read from its source.
	 */
	void StageSink(double T, double High, bool bByStern, double& OutDepth, double& OutPitch);
	/** The stage's end for a ship going down ("stern", or empty for her look's own): a boat that struck (her ShipOut's
	 * Struck or Lowered, 0 or more) and that the battle sends down scuttled goes by the stern, as StageSink has her. */
	std::string StageEndOf(const std::string& How, double Struck, double Lowered);

	struct ManOut
	{
		int Key = 0;          // his own
		int Ship = 0, Side = 0;
		std::string Nation;
		Swim Doing = Swim::Tread;
		Vec At;               // where he is: on the sea's surface in the water (Z 0), his feet's height jumping or climbing
		double Heading = 0.0; // radians, the way he faces
		double Since = 0.0;   // seconds he has been doing it (his clip's phase)
		double Drop = 0.0;    // a jump: the height he went from (metres)
		int On = -1;          // the float he holds or sits in (FloatOut::Key), or swims to; -1 none
		uint32_t Look = 0;    // his own hash (face, dress, an officer or a gun crew's helmet)
		bool bWave = false;   // treading, an arm up to a ship coming
	};

	/** What the show asks of the effects kit, the scene and the sound this frame. */
	enum class FxKind
	{
		Salvo,       // a ship's guns firing: Key (ship), At (her guns), Aim (where the shells will fall), What (class), Size (calibre mm);
		             //   What "ss": a boat's deck gun, "mv": a merchant's gun (one gun, one round: the scene lights it at her
		             //   model's muzzle, her after gun's where her ShipOut's bAftGun, laid as its Train and Elevation; Until,
		             //   when its round falls)
		Splash,      // a shell into the sea: At, What ("heavy", "medium", "light"; "small": a boat's or a merchant's gun), Size (calibre mm)
		ShellHit,    // a shell striking a ship: Key (ship), At, What ("shell_heavy", "shell_medium", "shell_light"), Size (calibre mm)
		TorpedoHit,  // a torpedo striking a ship: Key (ship), At (at her side)
		BombHit,     // a bomb striking a ship: Key (ship), At, What ("dive", "level", "torpedo" for an aerial torpedo)
		NearMiss,    // a bomb into the sea beside a ship: At, What ("bomb250", "bomb500")
		Sink,        // a ship going down: Key (ship), What (how), Size (1 the battle's biggest blast, else 0), Text (the line)
		LeaveLine,   // a ship out of the fight burning, leaving her line: Key (ship)
		Fire,        // flames on a ship that moves: Key (ship), At, Size (metres across), Until (seconds it burns)
		OilFire,     // oil burning on the sea: Key (the zone's id), At, Size (its reach, metres), What ("sunk", "burning"), Until (battle time), Since (seconds already burning)
		ScreenStart, // a ship starts laying smoke: Key (ship), At (her funnels), Vel
		ScreenMove,  // Key, At, Vel, every frame while she lays it
		ScreenStop,  // Key
		TrackStart,  // a torpedo's track: Key (torpedo), At, Vel; Aim (where its run ends, as fired), Until (the battle time it
		             //   ends), Size (the ship whose tubes it left, her key; -1: fired from its salvo's firing point)
		TrackMove,   // Key, At, Vel
		TrackStop,   // Key
		Flak,        // an anti-aircraft shell bursting: At, What ("heavy", "light")
		Launch,      // an aircraft leaving a carrier's deck: Key (carrier ship), At
		LandOn,      // an aircraft landing on: Key (carrier ship), At
		PlaneDown,   // an aircraft shot down falling: Key (plane), At, Vel
		Ditch,       // an aircraft into the sea: At
		Line,        // a side's battle line forms or turns together (sea:line): Key (its guide's leading ship, -1 none), Side, What (why: "form", "edge", "crossed"), Size (its hand: +1 starboard, -1 port), Vel (the new course's way, of length 1)
		Sound,       // a one-shot: What (the sound director's name), At
		Dispatch,    // an event's words, when it is shown: What (the event), Text, Side, Key (its piece)
		// A ship's anti-aircraft guns (L9: Battle/ShipGuns, shared with the air scene):
		Gun,         // a light gun's stream firing (the kit's AirGun, every frame it comes): Key (the stream), At (its muzzle),
		             //   Aim, Vel (the ship's), What (the kit's air_guns row), Text (her navy), Rate, Mv
		GunStop,     // that stream's trigger let go: Key
		ShipGun,     // a heavy gun fired: Key (ship), At (its muzzle), Aim, What (the gun), Size (calibre mm); its burst follows
		PlaneHit,    // rounds striking an aircraft (the kit's AirHit): Key (plane), At, Normal, Vel, What (the kit's row), Text
		             //   ("kill" for those that bring down one the battle lost to the guns)
		// A convoy's night (ruling 241; the submarine scene's looks are the production lead's helper's, at the scene's
		// marked hooks):
		ChargeDrop,  // an escort's depth charges going in: Key (the escort), At (her stern), Aim (the pattern's middle), What
		             //   ("pattern" or "ahead"), Size (charges), Spread (metres across), Sinking (seconds to the burst), Side
		ChargeBurst, // one charge bursting, at its moment: Key (the escort), At (Z: minus the depth it burst at), What ("pattern"
		             //   or "ahead"), Size (1: the blow the battle booked on the boat, else 0), Side; Aim and Until, where and when
		             //   its pattern set it to burst as it went in (the blow's too, wherever the battle's moment moved it; Until -1
		             //   for one no pattern laid: the view under the water judges a pattern by where it was laid, SeaUnder's Director)
		Dive,        // a boat going down or coming up: Key (the boat), At, What ("dive" or "surface"), Text (why), Side
		Starshell,   // a star shell bursting: Key (its own), At (where it bursts, Z its height), Until (battle time it burns out), Side
		Rescue,      // a ship picking up survivors: Key (the rescuer), At (her), Aim (where they are), Until (battle time it ends), Side
		Surrender,   // a boat striking (sea:surrender): Key (the boat), At, What ("scuttled" or "captured"), Size (the piece she
		             //   strikes to, -1 none), Aim (that piece's ship nearest her, where there is one), Text (the line), Side
	};

	struct Fx
	{
		FxKind Kind = FxKind::Sound;
		int Key = 0;
		Vec At, Aim, Vel;
		std::string What, Text;
		double Size = 0.0, Until = 0.0, Since = 0.0;
		int Side = -1;
		double Rate = 0.0, Mv = 0.0; // a stream's rounds a minute and their speed (Gun)
		Vec Normal;                  // PlaneHit
		double Spread = 0.0, Sinking = 0.0; // ChargeDrop
	};

	/**
	 * The scene's room for columns of water and other big bursts (the effects kit's: a 500 lb bomb's column holds 300 to
	 * 450 of the kit's 3,000 puffs for about 9 s; the effects thread, 20 November). A column is given room while fewer
	 * than Most are standing; those far from the eye only while fewer than FarMost are, so the near fight keeps its
	 * columns. A column refused is not drawn (the kit would thin it to nothing at that range anyway).
	 */
	struct ColumnRoom
	{
		int Most = 14, FarMost = 6;
		double Life = 9.0, Far = 3500.0;
		int Peak = 0, Given = 0, Refused = 0;
		bool Take(double T, double FromEye);
		int Standing(double T) const;

	private:
		struct Lot
		{
			double Until = 0.0;
			bool bFar = false;
		};
		std::vector<Lot> Lots;
	};

	/**
	 * A depth-charge blow told on a boat after the pattern laid on her burst all at once at the rules' moment (sim.ts's
	 * burstCharges books the blow at that very step): the most seconds after that burst it may come and still be that
	 * pattern's, laid by the same escort on the same boat, drawn as one burst at her (Checks::ChargeHitLate counts them).
	 * 15 s because the hand-made stand-in nights tell their blows 5-8 s after the rules' moment (wolf-pack's Boat A at 165 s,
	 * 5.0 s after its pattern's burst at 160 s; sub-air's boats at 141.6 s and 212.5 s, 7.4 s after theirs at 134.2 s and
	 * 205.1 s), with room over the 8 s, as the 15 s a blow is matched to a charge still in the water in. No recording of the
	 * battle tells one: its blows come at the rules' moment itself, and the cloud test holds every recording that is not a
	 * stand-in to none.
	 */
	constexpr double LateHitWithin = 15.0;

	/** The seamanship's own checks, kept as the show runs (the cloud test reads them). */
	struct Checks
	{
		double MostSlip = 0.0;      // degrees between a ship's bow and her way through the water, in the line
		double MostTurn = 0.0;      // how fast a ship of the line turned, against her class's limit at her speed (a share: 1 at the limit)
		double MostRudder = 0.0;    // how fast her rate of turn changed, against the rudder's limit at her speed (a share)
		double MostLead = 0.0;      // metres between a piece's leader and where he is steering for (the battle's place, the zig-zag and his station), two minutes after he last kept clear of another ship
		double MostLeadClearing = 0.0; // the same at any time, keeping clear or not
		double MostKeep = 0.0;      // metres a leader's station lay off the battle's place to keep clear of other ships
		double MostJump = 0.0;      // metres a ship moved in a frame beyond her speed (a pop)
		double MostGap = 0.0;       // metres a follower lay off her place in the line (once closed up)
		double MostHeel = 0.0;      // degrees
		double MostZigAfter = 0.0;  // metres of zig-zag still in the course a minute after contact
		int Salvos = 0, Splashes = 0, ShellHits = 0, TorpedoHits = 0, BombHits = 0, NearMisses = 0;
		int Sinks = 0, Leaves = 0, TracksRun = 0, Launches = 0, Landings = 0, PlanesDown = 0, FlakBursts = 0, Screens = 0;
		int SunkEvents = 0, BurningEvents = 0, TorpedoEvents = 0, TorpedoHitEvents = 0, StrikeEvents = 0, StrikeLandEvents = 0, AirEvents = 0, RecoverEvents = 0;
		int CoverUp = 0;            // fighters of the cover seen over the carriers at the most
		// A convoy's night (ruling 241): the events told, and what the show made of them.
		int DiveEvents = 0, SonarEvents = 0, ChargeEvents = 0, ChargeHitEvents = 0, ReportEvents = 0, StarshellEvents = 0, RescueEvents = 0;
		int ChargeDrops = 0, ChargeBursts = 0, ChargeBurstsBooked = 0, ChargeHitUnmatched = 0, Dives = 0, Starshells = 0, Rescues = 0;
		int ChargeHitLate = 0; // blows told on her after the pattern laid on her burst, within LateHitWithin: matched to that pattern
		int AirOnBoats = 0, AirBlowsOnBoats = 0; // aircraft told on a boat (sea:air), and their blows let go on her while she was afloat
		// A boat's deck gun and a merchant's gun (the convoy battle's "gun"): their actions told (sea:gun), their hits told
		// (sea:gunHit: the events, and the hits they name), and the rounds the show fired from each; of those rounds, the ones
		// that carried a hit told (fired its flight before it, whatever the cadence), and of those the ones whose gun could
		// not be laid on her in time (told too late for it, or her in its blind arc: fired as it lay, so the hit has its round)
		int GunEvents = 0, GunHitEvents = 0, GunHitsTold = 0, DeckRounds = 0, MerchantRounds = 0, GunHitRounds = 0, GunHitRoundsHasty = 0;
		// Enemy boats the player has not found (SetMine): contacts an escort of the other side gained on one and lost, a loss
		// told for a contact no escort held, and radio bearings (a line on the plot, not her place: they find nothing).
		int Contacts = 0, ContactsLost = 0, ContactsOdd = 0, Bearings = 0;
		double MostKeel = 0.0;      // metres: the deepest a boat was drawn
		// Ships under air attack (B4, B5.4 of naval-battles/ship-air-defence.md): the big ships put hard over against a
		// dive, every ship's turn to comb a torpedo plane's drop, those back in their place after, the worst still off it
		// (metres from where she would have been, as the strike's last aircraft is long gone), and the near misses thrown
		// beside a dive bomber's hit (its wing bombs).
		int Dodges = 0, Combs = 0, DodgesBack = 0, WingBombs = 0;
		double MostDodgeOff = 0.0;
		double MostStray = 0.0;     // the farthest any ship was from her place while she dodged or came back to it (metres)
		int DodgesOpen = 0;         // dodges whose ship was not yet judged as the battle ended (still under attack)
		int DodgesClosing = 0;      // judgings put off 30 s for a ship on her track still closing her place at her best speed
		int DodgesClearing = 0;     // judgings put off 30 s for a ship keeping clear of another on her way back
		int FellAstern = 0;         // ships come back from a comb astern of the next in their line, who took her place astern
		int HandedOver = 0;         // leaders fallen back level with their next astern dodging, who handed her the lead
		int DodgesUnturned = 0;     // dodges ended before her helm bit (another ship too near): none, so not judged
		int SunkUnmatched = 0, BurningUnmatched = 0, TorpedoHitUnmatched = 0;
		// Torpedoes and the ships of their own side (Brian, 25 November 12:52Z): the tracks a ship held in her tubes for an own
		// ship in their way and fired once it was clear, and those fired from the salvo's firing point (from a third of the run,
		// or before it where her own place's would not come into the rules' water within LookAhead); the tracks stopped at the
		// aim for an own ship in their run on as they were fired, and those stopped running on as one came across them; the
		// longest a ship held her tubes (seconds), and the least any track's line cleared an own ship's hull by as it was fired
		// (metres beyond her half beam; the show's own measure).
		int TracksHeld = 0, TracksMoved = 0, TracksCut = 0, TracksStopped = 0;
		// Those never drawn (no way clear by two thirds of the run, when a track fired would no longer be one a hit may end),
		// each judged over its hold: those that at some judging (once a second) had only ships of other pieces lying in her
		// salvo's water ahead of its heads in their way (the rules' side: their switch keeps own ships out of it); of the rest,
		// as each was given up, those her own piece's ships alone kept foul at every place she might fire from (the scene's
		// own), and the rest the scene's own too (a ship behind the heads or out of the water, the room).
		int TracksDropped = 0, TracksDroppedOwn = 0, TracksDroppedWater = 0;
		int TubesAtHit = 0;         // tubes fired at a hit with none of its piece's tracks running, to end there (a last resort: none)
		double MostHold = 0.0, LeastTrackGap = 1e9;
		// By salvo, in the order told (each sea:torpedo with ships to fire it): the tubes the battle fired and the tracks drawn of
		// them. By torpedo hit drawn, in order: how far from her side the head of the track as fired at the battle's moment lay
		// (the hit's shadow: the jump the track a hit ended made before any ship held hers; -1 none).
		std::vector<int> SalvoTubes, SalvoTracks;
		// (and by salvo, the same order: the piece that fired it, and its tubes never drawn (DropTube), so a salvo's tubes still
		// held are its tubes less those drawn and those dropped: the torpedo words' test holds Show::TubesNow to them)
		std::vector<int> SalvoPiece, SalvoDropped;
		std::vector<double> HitShadowGap;
		double GaveWay = 0.0;       // ship-seconds followers eased their way for a ship of their own line crossing ahead
		double KeptClear = 0.0;     // ship-seconds any ship held an alteration of course or way to keep clear of another
		// Where ships went down: her crew in the water (the real number, and those the show keeps), the most at once, those
		// picked up, those pushed aside by a ship's bow; her floats and her oil.
		int Wrecks = 0, WrecksManned = 0, WrecksDue = 0; // ships gone down; with men in the water; that should have them
		int MenReal = 0, MenKept = 0, MostMen = 0, MenPicked = 0, MenSwept = 0, MenDead = 0;
		int Floats = 0, Rafts = 0, Boats = 0, Slicks = 0, SlicksAlight = 0;
		int MenUnder = 0;           // dead men let go under to make room for the living of a later ship
		// The naval rules' sailor events (sea:men, sea:oil, sea:abandon): told, and found their wreck (or ship); the wrecks
		// whose men the battle counted, the real number the show took from it against the battle's own (men and dead), and
		// men drawn taken aboard a rescuer beyond the share of the battle's count of those picked up (none allowed).
		int MenEvents = 0, MenMatched = 0, OilEvents = 0, OilMatched = 0, AbandonEvents = 0, Abandons = 0;
		// Ships the battle's own count took out of their line with no word of how (a view's ships fallen below those in her
		// line, no sea:sunk, burning, abandon or surrender told for her by then): given up as one abandoned (Show::Untold)
		int LeftUntold = 0;
		int MenCounted = 0, MenCountOff = 0, PickedOver = 0;
		// A boat that strikes (sea:surrender): the events read; the boats struck (her piece's boat found) and not; her men
		// come out on her casing; of her men in the water, those who went over from it and all of hers; men drawn aboard a
		// ship of the other side (prisoners); prizes taken, and their boarders gone below
		int SurrenderEvents = 0, Struck = 0, StruckUnmatched = 0, HandsOut = 0, OverFromDeck = 0, MenOfStruck = 0;
		int Prisoners = 0, Prizes = 0, BoardersBelow = 0;
		int ClearShips = 0;         // ships that altered so, once or more
		double OwedLeft = 0.0;      // hits the battle took off a hull that no salvo had drawn by the end
		double OwedAll = 0.0;       // hits the battle took off hulls by gunfire, in all
		std::map<std::string, int> SinksBy; // by how
		std::map<int, double> OwedBy;       // by piece: hits owed and not drawn at the end
		int MostScreens = 0, MostPlanes = 0, MostTracks = 0;
		/** Where each worst one was (the ship's key, the time, her speed): for the cloud test's report. */
		std::string TurnWorst, RudderWorst, HeelWorst, LeadWorst, GapWorst;
	};

	class Show
	{
	public:
		explicit Show(uint64_t Seed = 239);

		/** The most torpedo tracks running at once (each salvo shares what is left of it among its ships). */
		static constexpr int TrackRoom = 60;
		/** The least a torpedo's track clears a ship of its own side by as it is fired (metres beyond her half beam, all along
		 * her length): its line from where it starts to where its run ends. */
		static constexpr double TrackClear = 20.0;
		/** No track fired from a ship's own place runs longer than this outside its salvo's water as the rules have it
		 * (seconds; one from the salvo's firing point starts in it). */
		static constexpr double LookAhead = 15.0;
		/** How much faster or slower than now a ship of the side may steam over a track's run as its head is looked at
		 * against her (a share of the time ahead; and a second either way at the least): a ship easing her way to keep clear
		 * of another, or working up again, is still clear of the head. */
		static constexpr double WayDoubt = 0.1;

		void SetField(const Field& InField);
		const Field& Board() const { return F; }
		/** The player's side (0 the attacker, 1 the defender: the views' "me", SeaFeed::SideOf), told before the first Take. An
		 * enemy boat (class "ss" of the other side) is drawn only once she is found and while she stays found: ShipOut::bShown
		 * false until then, with no wake and no smoke, her own cues unheard and her guns silent (her torpedo tracks still run),
		 * and the scene hides her hull and what she throws on the sea. Found: an escort of the other side has her by asdic,
		 * radar or sight (the boat's heard where her view gives it, else sea:sonar, each contact until its loss is told, its
		 * escort is gone or she is, or FoundFor after the last attack of one that has made an ahead-thrown run on her, which
		 * sends no loss; a radio bearing finds nothing), and FoundFor after the last of them loses her; an escort's depth
		 * charges on her or a blow on her; guns laid on her (an escort's or an armed merchant's) or hers firing; aircraft
		 * running in on her; forced up; out of the fight. Up in plain sight within the night's seeing of a ship of the
		 * player's, she stays found until she is under. -1, as made: every ship drawn, as before (the art director's
		 * -TheaterSeaShowAll). */
		void SetMine(int Side) { MySide = Side == 0 || Side == 1 ? Side : -1; }
		int MineSide() const { return MySide; }
		/** Seconds an enemy boat stays in sight after the last escort holding her loses her, or after a blow on her under: her
		 * hunters' run in on where she was lost and their charges sinking to her (form.md section 5: asdic lost close at 150
		 * to 300 m, charges 20 to 25 s down). */
		static constexpr double FoundFor = 90.0;
		/** A boat's deck gun and a merchant's guns (the convoy battle's "gun", one gun firing at a time): how far either side
		 * of where it lies stowed each trains, as the battle modeller's models' mounts turn (their turret.A and turret.Y
		 * limits, which Tools/convoy_looks reads): a boat's 145 degrees, so it never fires through her own tower (an American
		 * fleet boat's gun stowed facing aft, any other's forward); a merchant's 150, so her stern gun, her main one, bears
		 * from 30 degrees off her bow round her stern and takes any target it bears on, and her bow gun only one finer on
		 * her bow than that. */
		static constexpr double DeckGunArc = 145.0, MerchantGunArc = 150.0;

		/** Where the camera is (board metres): columns and screens near it come first when the room is short. */
		Vec Eye;

		/** A view: its battle time, its pieces and its events; its zones where the view carried them (null: as before);
		 * the battle time of first contact (below 0: none yet). Kept for the picture between views. */
		void Take(double T, const std::vector<Piece>& InPieces, const std::vector<Event>& InEvents, const std::vector<Zone>* InZones, double Contact);
		/** The battle time shown now (never past the latest view). */
		void Advance(double T);

		double Shown() const { return DrawT; }
		double Latest() const { return Views.empty() ? 0.0 : Views.back().T; }
		const std::vector<ShipOut>& Ships() const { return OutShips; }
		const std::vector<PlaneOut>& Planes() const { return OutPlanes; }
		/** The ships' anti-aircraft guns' own count (ShipGuns: the cloud test reads it). */
		const ShipGuns::Tally& Guns() const { return Gunners.Tallied(); }
		/** Ships whose dodge is not yet judged (the cloud test: still under attack as the battle ends). */
		int OpenDodges() const;
		const std::vector<TorpOut>& Torpedoes() const { return OutTorps; }
		/** Where ships went down: her wreckage, boats and rafts; her oil; her men in the water. */
		const std::vector<FloatOut>& Floats() const { return OutFloats; }
		const std::vector<SlickOut>& Slicks() const { return OutSlicks; }
		const std::vector<ManOut>& Men() const { return OutMen; }
		/** A struck boat's crew on her casing and her boarding party (sea:surrender), drawn as men are; kept apart from Men(),
		 * whose counts are the men in the water. A captor's whaler and its oarsmen (Swim::Boat, On its key) are here too. */
		const std::vector<ManOut>& Hands() const { return OutHands; }
		const std::vector<FloatOut>& Whalers() const { return OutWhalers; }
		/** The share of the men in the water the show keeps (1: every man; less where there were more than the room). */
		double MenShare() const { return Check.MenReal > 0 ? static_cast<double>(Check.MenKept) / Check.MenReal : 1.0; }
		/** What has happened since it was last emptied: the caller plays it and clears it. */
		std::vector<Fx> Effects;
		ColumnRoom Columns;

		/** The wind (metres a second, the way it blows toward) and the sea's state on the Beaufort scale. */
		Vec Wind() const { return WindV; }
		double SeaState() const { return Beaufort; }
		/** The wind's speed at 10 m (metres a second), and where it blows from (radians, the board's bearing). */
		double WindSpeed() const { return WindU; }
		double WindFrom() const { return WindFromR; }
		/** A piece's column as shown now: its middle and its leader's heading; false where there is none afloat. */
		bool PieceMiddle(int PieceId, Vec& OutAt, double& OutFace) const;
		/** A piece's leading ship as shown now (the head of its column, where the torpedo words' tag stands): her key, -1 where
		 * it has none in its line. */
		int LeaderOf(int PieceId) const
		{
			const auto It = Lines.find(PieceId);
			return It == Lines.end() || It->second.Line.empty() ? -1 : It->second.Line.front();
		}
		/** A ship as shown now, by her key. */
		const ShipOut* Ship(int Key) const;
		/** The pieces as the battle has them now (interpolated), for labels and the camera. */
		const std::map<int, Piece>& Pieces() const { return Now; }
		double ContactAt() const { return ContactT; }
		const Checks& Checked() const { return Check; }
		/** One line a ship and a strike: her piece, her place, what she is doing (a debug overlay; the cloud test's trace). */
		std::string Describe() const;
		/** U1 hook (under water): a depth charge on its way down, as the view under the water draws it (Battle/SeaUnder): its
		 * escort's ship key, the boat's piece, where it bursts (Z minus the depth; a charge told on her bursts Off from her,
		 * where she is then), when it went in and when it bursts, its kind. Whether it is the battle's blow is left out: the
		 * view draws every charge alike. Read only. */
		struct ChargeDown { int Escort = -1, Boat = -1; Vec At, Off; double Bursts = 0.0, Dropped = 0.0; std::string Kind; bool bOnBoat = false; };
		void ChargesDown(std::vector<ChargeDown>& Out) const;
		/** The torpedo words' picture (Battle/SeaWordsCore), read only: what the scene shows of each piece's torpedoes now
		 * (TubesOut), by piece, for each piece that has told a salvo or holds tubes (Out emptied first). */
		void TubesNow(std::map<int, TubesOut>& Out) const;
		/** The torpedo words' phrases, by piece, read only: its name as the rules last gave it in a view the picture has
		 * reached (another piece's heldForName or clearFromName, which name the piece meant), and the log's line of its latest
		 * salvo (its sea:torpedo's text), each as last told. */
		const std::map<int, Phrase>& NamesGiven() const { return RulesNamed; }
		const std::map<int, Phrase>& SalvoLines() const { return SalvoSaid; }

	private:
		struct View
		{
			double T = 0.0;
			std::map<int, Piece> Pieces;
		};

		/** A point of a leader's track: how far he had steamed when he passed it, his heading there and how sharply he
		 * was turning (radians a metre), so those astern pass it as he did. */
		struct Wake
		{
			Vec P;
			double S = 0.0;
			double H = 0.0, K = 0.0;
		};

		struct Hull
		{
			int Key = 0, Piece = 0, Side = 0, Slot = 0;
			std::string Class, Nation;
			Fate Doing = Fate::Line;
			Vec P;
			double Hdg = 0.0, Spd = 0.0, Rate = 0.0; // radians, m/s, radians a second
			double Heel = 0.0, Pitch = 0.0, Heave = 0.0;
			double DiveAngle = 0.0; // a boat's angle diving or surfacing (degrees, plus bow up), in her Pitch
			double Back = 0.0;      // a follower: metres of track behind her leader, as she has closed up so far
			double SOn = 0.0;       // a follower: where she is along his track (how far he had steamed when he passed there)
			double Roll0 = 0.0, RollT = 10.0, PitchT = 7.0; // her own phase on the swell, and her roll and pitch periods
			double Train = 0.0, Elev = 0.0;
			// (a boat's or a merchant's one gun: her after one, laid, and when a shell last shook her: ShipOut's)
			bool bAftGun = false, bGunLaid = false;
			double ShakenAt = -1.0;
			int AimKey = -1;
			double NextSalvo = -1.0; // battle time of her next salvo (below 0: not yet laid)
			int Engaged = -1;
			bool bFiring = false;
			double FiredAt = -1.0;
			// out of the line: limping away burning, or going down
			double LeftAt = -1.0, Away = 0.0, NextFire = 0.0, LimpSpd = 5.0;
			std::string How;
			double SinkAt = -1.0, SinkFor = 120.0;
			// abandoned (sea:abandon): her men leaving her, she settling and losing way, until the battle says she goes
			bool bAbandoned = false;
			// sent to pick men out of the water (sea:rescue, a follower of her line): she lies off her wreck (Wrecked's place)
			// out of the wake, and comes back astern of the ship ahead of her when done
			bool bLyingOff = false;
			int LieFor = -1;
			// smoke laid
			double ScreenUntil = -1.0;
			bool bScreen = false;
			std::string LastBlow;  // the weapon of the last hit drawn on her (for the way she goes down)
			double BlowAt = -1.0;  // when it was drawn (battle time; below 0, never): the ship of her piece a blow struck last
			Vec LastP;
			bool bHasLast = false;
			double LastRate = 0.0;
			// Under air attack (B4): her helm hard over to DodgeWay's side (+1 to starboard) until DodgeUntil, or (bComb)
			// round to CombHdg, bow or stern on to a torpedo plane's drop; then back to her place.
			double DodgeUntil = -1.0, DodgeWay = 0.0, CombHdg = 0.0;
			bool bComb = false;
			bool bDodgeShown = false; // (this dodge's helm put over, the cloud test's count: one ended before it bit is none)
			double DodgeJudge = -1.0; // battle time she is to be back in her place by (the cloud test's count)
			double CourseHdg = 0.0;   // the course she would be steaming: her leader's (his ghost's), on his track at her place
			double OffTrack = 0.0;    // a follower: metres off his track (plus: to her port), last step; a leader: off her ghost
			bool bDodgeTurned = false; // her helm has bitten in a dodge since she was last judged (judged only then)
			double PutOffGap = -1.0;  // metres astern of her place when her judging was last put off as she closed it
			// A boat (ruling 241): her state, her keel as drawn, where it is going and how fast (metres a second, plus down),
			// her periscope.
			std::string Dive;
			double Keel = -1.0, KeelTo = 0.0, KeelRate = 0.0;
			// (following a depth the battle tells as it changes, Trim: on it, and until she has eased to a stop at its end, or at
			// the depth she was given when she left her line in the middle of one; never stopped dead from her rate)
			bool bOnRamp = false;
			// (a boat deep at a crawl whom the battle turns round where an escort has her: she goes astern on her motors, her
			// stern on the battle's course, rather than come round in her circle, Show::SteamColumn; her Spd then below 0.
			// HeldAt: when an escort's echo was last on her piece, battle time. bRounding: a boat deep coming round for her
			// station where the battle has turned her round, until it is off her bow. bCatching: going astern no longer, the
			// battle's course swung across her beam, steering straight for her station until she is near it on its course)
			bool bAstern = false;
			double HeldAt = -1.0;
			bool bRounding = false;
			bool bCatching = false;
			bool bScope = false;
			// Struck (sea:surrender): her flag up and her crew coming out on her casing once she is up (UpAt); how the battle
			// says she ends ("scuttled", "captured"), the piece she struck to, when her flag came down; the battle's order to
			// abandon her told (once); a prize, taken: held afloat as she was (her SinkAt carried on, so her Down stands)
			bool bStruck = false, bAbandonTold = false, bPrize = false;
			// (a boat abandoned, sea:abandon, while still coming up: her men, her raft and her boats wait for her casing)
			bool bLeftUnder = false;
			// (given up by the battle's own count with no word of how, Untold: numbered among her piece's lost as the next, and
			// not yet by the battle's own number, which a sea:abandon told after gives her)
			bool bGivenUp = false;
			double StruckAt = -1.0, UpAt = -1.0, FlagDownAt = -1.0;
			// (struck and sent down after, sea:sunk: when the stage was told her end, StageSink's clock for her crew still on her)
			double SentDownAt = -1.0;
			std::string StruckHow;
			int StruckTo = -1;
			// Keeping clear (Brian, 22 November 05:31Z: "the ships are turning into each other and going over each other"):
			// the alteration she holds off the course she wishes (radians, plus to starboard) and the share of her wished way
			// she keeps, when she chose it, and when she last held one.
			bool bClearing = false, bEverCleared = false;
			double ClearOff = 0.0, ClearKeep = 1.0, ClearAt = -1e9, ClearLast = -1e9;
			double ClearSince = -1e9; // when she took the alteration she holds
			double ClearAbs = 0.0;    // the course she holds to keep clear (radians), where she alters course
		};

		/** A piece's column: its leader's track and its ships. */
		struct Column
		{
			int Id = 0;
			int Side = 0;
			std::string Class, Nation;
			ClassSize Size;
			double Spacing = 300.0;
			std::deque<Wake> Track;
			double S = 0.0;          // how far its leader has steamed
			std::vector<int> Line;   // its ships in the line, leader first (their keys)
			bool bStarted = false;
			// L7: its side's zig-zag offset applied to the battle's place (metres to the right of its course)
			double Zig = 0.0;
			// Its station: where its leader keeps off the battle's place to keep clear of other ships (the battle's pieces are
			// points, and it may lay two of them on one spot: drawn, each column keeps its own water). Fixed on the sea, not
			// on the course, so when the fleet turns its columns turn together, as a fleet's did on the signal.
			Vec Keep;
			// Its leader dodging an air attack (B4): where he would steam had he not, from the moment he put his helm over
			// until he is back on it. His track is laid from it meanwhile, so his line keeps its course, not his dodge.
			bool bGhost = false;
			double GhostSince = 0.0;
			Hull Ghost;
		};

		/** A salvo in the air. */
		struct Shells
		{
			int From = 0, Piece = -1;
			int OnPiece = -1;
			double Lands = 0.0;
			double Calibre = 152.0;
			int Count = 2;
			std::string Class;
			Vec Fired;
			// (a boat's or a merchant's round that carries a hit the battle told, sea:gunHit: the hit's moment, when it lands,
			// and the ship it was laid on; Play draws the hit through it as it is told. Below 0, none: a round like any other)
			double ToldAt = -1.0;
			int OnKey = -1;
			// (laid on a ship of that piece wherever she lies afloat, in her line or out of it, as a boat's or a merchant's gun
			// is, or a warship's gun told on her piece: where it falls, NearestAfloatOf, not her line's nearest)
			bool bLaidAfloat = false;
		};

		struct Torp
		{
			int Key = 0, Side = 0, Piece = 0;
			Vec P, Way, From;
			double Speed = 20.0, Ends = 0.0, Launched = 0.0;
			Vec Aim;
			bool bRunning = true;
			// (the ship whose tubes it left, -1 none: from its salvo's firing point, she not lying at it; whether from that
			// point; where its run ends as fired; when its head is level with the aim, and the battle's moment for it: past both
			// it runs on, and stops for an own ship across it; its salvo and that salvo's run, for the hit that ends it)
			int Ship = -1, Salvo = 0;
			bool bFromPoint = false;
			Vec End;
			double AimAt = 0.0, Arrive = 0.0, Running = 75.0;
		};
		/** A track as it would have been fired at the battle's moment from each ship's place, never drawn (a shadow): the
		 * shadows keep the room's count and pick the ship a hit strikes as the tracks did before any ship held hers, so the
		 * dice and every ship are the same with the holds or without. */
		struct Shadow
		{
			int Piece = 0;
			Vec P, Way;
			double Speed = 20.0, Ends = 0.0;
		};
		/** A ship's tubes not yet fired (Brian, 25 November 12:52Z): each held while a ship of her side lies in its way to the
		 * aim. Her salvo (its count), her piece and side; her class and navy; whether a boat's, its tracks wakeless;
		 * the aim, when the battle told it, when its tracks arrive, the run's seconds; the fan; each tube's place in it, its own
		 * scatter (the battle's dice, thrown as the salvo was told) and when its shadow ends; the salvo's firing point (the
		 * piece's place as the battle told the salvo, its leader's there as the rules lay him: where the rules' water starts);
		 * the rules' water (the naval rules' own-water switch: the fan from that point, the leader's half beam wide there, to
		 * the run's radius either side of the aim), and whether she has fired any. */
		struct Held
		{
			int Salvo = 0, Ship = -1, Piece = -1, Side = 0;
			std::string Class, Nation;
			bool bBoat = false, bWakeless = false;
			Vec Aim;
			double Told = 0.0, Arrive = 0.0, Running = 75.0, Fan = 3.5;
			std::vector<double> Ks, Scatter, Ends;
			std::vector<char> RulesOnly; // (per tube: at some judging of its hold the rules' ships alone were in its way, WhyHeld)
			// (per tube still held, as she last looked (FireTubes): the piece of the first own ship found across its way, at the
			// first place a ship holds it, -1 none; the torpedo words' alone, TubesNow: never read in deciding)
			std::vector<int> Across;
			Vec WaterP;
			double WaterHalf = 5.0, Radius = 800.0;
			bool bFiredAny = false;
			double NextLook = 0.0; // (when she looks again: every quarter second, her salvo's ships a step apart)
			double NextJudge = 0.0; // (when she next judges why a tube is held: once a second, from her first look)
		};
		/** Where a held ship's tubes may be fired from: a place, her heading there (the way to the aim at the salvo's firing
		 * point), the ship left out of the tracks' way (hers, they starting at her side; -1 none), whether it is the salvo's
		 * firing point, and whether only for a tube whose track from her own place would not come into the rules' water
		 * within the look ahead (the point before a third of the run). */
		struct FirePlace
		{
			Vec P;
			double Hdg = 0.0;
			int Except = -1;
			bool bPoint = false, bIfOut = false;
		};
		/** One tube's track as it would be fired from a place now: where it starts, its way, where its run ends, its speed,
		 * when it ends, when its head is level with the aim; whether its run on was cut for an own ship in it, and whether its
		 * line is clear; the own ships across it (their keys), and whether it would start or run outside the rules' water
		 * (from the firing point, at its start; from her own place, past the look ahead). */
		struct TubeWay
		{
			Vec P, Dir, End;
			double Speed = 0.0, Ends = 0.0, AimAt = 0.0;
			bool bClear = false, bCut = false, bOutside = false;
			std::vector<int> Foul;
			// (not every ship asked: the first own ship found across it, -1 none; for the torpedo words, Held::Across)
			int FirstFoul = -1;
		};

		enum class Leg { Deck, Form, Out, Attack, Home, Gone, Cover };
		struct Plane
		{
			int Key = 0, Side = 0, Strike = 0;
			std::string Model, Nation, Role;
			Vec P;
			double Hdg = 0.0, Pitch = 0.0, Bank = 0.0, Spd = 0.0, Climb = 0.0;
			Leg Doing = Leg::Deck;
			double LegT = 0.0;
			double Go = 0.0;      // battle time it leaves the deck
			int Carrier = -1;     // the ship it flew from (its key)
			bool bFalling = false;
			double Lost = -1.0;   // battle time it is hit (falling from then)
			Vec Fall;
			bool bShown = false;
			double Hand = 1.0;
			int Slot = 0;
			int Over = -1;        // a fighter of the cover: the carrier piece it circles over (L2)
			bool bHit = false;    // it is one of the strike's hits (the battle's share, decided as the strike landed)
			bool bFinal = false;  // on its final approach to her deck
		};
		struct Strike
		{
			int Id = 0, Side = 0, Carrier = -1, Target = -1;
			double Launched = 0.0, Lands = 0.0;
			int Count = 0;
			bool bLanded = false;
			bool bFromCoast = false;
			std::vector<int> Planes;
			int OnKey = -1;        // the ship of the target piece its aircraft go for (kept, so a ship that turns away stays theirs)
			bool bDodged = false;  // she has put her helm over against its dive (B4)
		};

		struct Owed
		{
			double Hits = 0.0;
			std::string Weapon = "shell_medium";
		};

		/** A depth charge on its way down (ruling 241): the escort's ship and piece, the boat's piece, where and when it
		 * bursts, and whether it is the blow the battle booked. */
		struct Charge
		{
			int Escort = -1, Piece = -1, Boat = -1, Side = -1;
			Vec At;
			double Bursts = 0.0, Dropped = 0.0; // (battle times: it bursts; its pattern went in)
			std::string Kind;
			bool bHit = false;
			bool bOnBoat = false; // (the battle's blow told before it bursts: it bursts where she is then, Off from her)
			bool bAtOnce = false; // (of a pattern laid all at once over the spot the battle tells, bursting at the rules' moment)
			Vec Off;
			// (U1 hook (under water): where and when its pattern set it to burst as it went in, kept where the battle's blow
			// moves it; Due -1 for one no pattern laid, put by her for a blow told with none of hers to take)
			Vec Laid;
			double Due = -1.0;
		};

		Field F;
		/** F.Classes as a list, each with its name's first two letters as a number (-1 for a name not two letters long),
		 * for SizeOf: asked many times a step for every ship, a short scan of numbers beats the map (finding 9). */
		struct ClassRow
		{
			int Code = -1;
			std::string Name;
			ClassSize Size;
		};
		std::vector<ClassRow> ClassList;
		Dice Roll;
		std::deque<View> Views;
		std::deque<Event> Queue;
		std::map<int, Piece> Now;
		std::map<int, Column> Lines;
		std::map<int, Hull> Hulls;
		std::vector<Shells> Flying;
		std::vector<Torp> Torps;
		std::vector<Shadow> Shadows;
		std::vector<Held> Holding;
		int TorpSalvos = 0;
		/** By piece: the battle second its last salvo was told (sea:torpedo); and the second of the view the pieces are taken
		 * from now (Interpolate). A piece whose salvo is told after that view holds it no longer (the rules have let it fire,
		 * as the next view says): its held fields are taken off as the salvo is told, so the torpedo words' tag goes off as
		 * its first track leaves, not at the next view. */
		std::map<int, double> SalvoToldAt;
		double NowFrom = -1.0;
		/** The torpedo words' alone (TubesNow, NamesGiven, SalvoLines; never read in deciding): by piece, its latest salvo's
		 * number; the rules' name of it as a view reached last gave it (Interpolate); and its latest salvo's line in the log. */
		std::map<int, int> LastSalvo;
		std::map<int, Phrase> RulesNamed, SalvoSaid;
		std::map<int, Plane> Air;
		std::vector<Strike> Strikes;
		std::map<int, Owed> Debt;         // by piece: hits the battle has taken off its hull not yet drawn
		std::map<int, double> LastStr;     // by piece: its hull as the view before had it
		std::map<int, double> AirBlowAt;   // by piece: battle time of the last strike, raid or torpedo hit on it
		std::vector<Zone> Zones;
		std::vector<Charge> Charges;   // depth charges sinking, not yet burst
		std::map<int, double> AheadAt; // by escort piece: battle time of its last ahead-thrown salvo (a hit bursts them)
		// (by escort piece: the battle time its last pattern laid over the spot burst, all at once at the rules' moment, and
		// the boat's piece it was laid on: a blow on her from it told after that moment is that pattern's)
		std::map<int, std::pair<double, int>> PatternBurstAt;
		ShipGuns::Gunnery Gunners;   // L9: the ships' anti-aircraft guns
		bool bGunsUp = false;        // aircraft were up for them last frame
		double GunDue = 0.0;         // battle seconds since they were last laid
		std::set<int> ZonesLit;
		std::map<int, int> ZoneShip;       // by zone id: the ship it was laid for (sunk, burning)
		/** The ships each piece has lost, in order ({piece, 1} its first): her key, for the sailor events that name a wreck
		 * so; and how many each piece has lost. */
		std::map<std::pair<int, int>, int> LostShips;
		std::map<int, int> LostCount;
		/** An enemy boat as the player's side has found her (SetMine): the escorts holding her by their sea:sonar events (a
		 * recording whose boats carry no heard), each until it tells its loss, is gone or she is (the battle closes every
		 * contact with a loss or her end: a contact it still holds keeps her in sight however long it goes without an
		 * attack), save one that has made an ahead-thrown run on her, which sends no loss: let go FoundFor after its last
		 * attack; in sight until when (FoundFor after the last of them lost her, or after a blow on her under), up in sight (hit
		 * or forced up: until she is under again, then FoundFor), out of the fight for good (her crew giving her up), held by
		 * the escorts her view says are running in on her, kept by a lookout's eye (up within the night's seeing), and in
		 * sight as of the last sweep. */
		struct Hunted
		{
			/** An escort holding her by its events: when it last had word of her (its contact gained, or its charges on her),
			 * and whether it has made an ahead-thrown run on her since it gained her (no loss is told after one: its hold
			 * runs out FoundFor after that last word). */
			struct Hunter
			{
				double Word = 0.0;
				bool bAhead = false;
			};
			std::map<int, Hunter> HeldBy;
			std::set<int> Lapsed; // escorts whose hold ran out after an ahead-thrown run: a loss told for one after is no odd one
			double Until = -1.0;
			bool bUp = false, bForGood = false, bHeld = false, bLookout = false, bSeen = false;
		};
		std::map<int, Hunted> Hunts; // by hull key: only boats of the other side, once anything has touched them
		int MySide = -1;
		std::vector<ShipOut> OutShips;
		std::vector<PlaneOut> OutPlanes;
		std::vector<TorpOut> OutTorps;
		std::vector<FloatOut> OutFloats;
		std::vector<SlickOut> OutSlicks;
		std::vector<ManOut> OutMen;
		/** Where a ship went down: when her crew go over the side and how many, her rafts and wreckage still to come, her
		 * oil, and the ship picking her men up. */
		struct Wreck
		{
			int Ship = 0, Side = 0;
			std::string Class, Nation, How;
			double Long = 100.0, Beam = 10.0, Free = 5.0; // her length, beam and height of deck over the sea
			double T0 = 0.0, For = 100.0;                 // she began to go, and took this long
			double Start = 0.0, End = 0.0;                // her men go over the side between these
			double BoatsFrom = -1.0;                      // her boats lowered from (below 0: from Start); one left, once she has nearly lost her way
			int Men = 0, Out = 0, Dead = 0;               // men the show keeps, gone over so far, of them dead
			int Rafts = 0, RaftsOut = 0, Boats = 0, BoatsOut = 0, Bits = 0, BitsOut = 0;
			Vec Oil0;                                     // where her oil began
			int Slick = -1;                               // its key
			double Burn = 0.0, BurnUntil = -1.0;          // the share of it alight, and until when
			double Lee = 1.0;                             // her windward side (+1 starboard, -1 port)
			int Rescuer = -1;                             // the ship picking her men up (key), -1 none
			double RescueUntil = -1.0;
			int Picked = 0, PickCap = -1;                 // her men drawn taken aboard, and the most the battle's count allows (-1 no count)
			double OilR = -1.0, LitR = -1.0;              // the battle's spread of her oil and of its burning part, across (metres; below 0 not said)
			uint64_t Seed = 0;                            // her own hash's start
			int Real = 0;                                 // men of hers who went into the water in truth
			double DeadShare = 0.0;                       // of them, the share dead in it
			Vec OilEnd, Drift;                            // where her oil ended (where she went under), and how far it has drifted
			double TrackHdg = 0.0, TrackLen = 0.0;        // her oil's way along her last track
			bool bGone = false, bLit = false, bDue = false;
			bool bLeft = false;                           // her men left her (sea:abandon): over one side from her waist and stern, as she loses her way
			bool bStruck = false;                         // a boat that struck: her men come over from her casing (Muster's plan, StepHands)
			// (the ships near her men and her wreckage, looked for each second, and found again each step)
			std::vector<int> Near;
			std::vector<const Hull*> NearNow;
			std::vector<double> NearHalf, NearR;          // (theirs: half their length less their beam, half their beam)
			double NearAt = -1e9, Nearest = 1e9;          // when; how far the nearest ship afloat is from her middle
			Vec Middle;                                   // the middle of her men and wreckage
			double Spread = 0.0;                          // how far from it the farthest is
			std::vector<int> Floats;                      // her rafts, boats and wreckage (their keys)
		};
		struct Floater
		{
			FloatOut O;
			Vec V;                // its own way through the water (spreading), easing off
			double Leeway = 0.03; // a share of the wind it drifts at
			double Turn = 0.0;    // radians a second
			int Room = 0, Hold = 0; // men it takes aboard, men it lets hold on
			uint32_t InMask = 0, HoldMask = 0; // the places aboard and round it taken (or promised to a man swimming to it)
			int W = 0;            // its wreck (Wrecked's place)
			int Group = 0;        // men aboard it, holding on to it or gathered round it (counted each step)
		};
		struct Man
		{
			ManOut O;
			Vec V;                // his way over the jump
			Vec Goal;             // where he is swimming to
			double Pace = 0.7;    // metres a second he swims
			double Next = 0.0;    // battle time he next looks about him
			Vec Off;              // on a float or a rescuer's side: where on it (its own frame: x along, y across)
			int W = 0;            // his wreck (Wrecked's place)
			int Slot = -1;        // his place on his float (On), -1 none
			bool bIn = false;     // that place aboard it (else holding on round it)
			bool bGoing = false;  // dead, going under (to make room for the living)
			bool bSwept = false;  // pushed aside by a ship's bow, once or more
			double Until = 0.0;   // climbing: when he is aboard; going under: when he is gone
			int To = -1;          // climbing: the rescuer (her key)
			double ToSince = 0.0; // swimming for a rescuer's side: battle time he set off for it (her net goes down as he comes)
			int Near = -1;        // the float he keeps by, treading water in its group with no place on it (its key), -1 none
			Vec NearOff;          // his place by it (its own frame)
		};
		/** One of a struck boat's crew on her casing (Muster's plan), in her own frame as the show has her (Along from
		 * amidships, Across plus to starboard, Z his feet over her casing there, below 0 still in her hatch): up out of his
		 * hatch or down from her bridge, along her casing to his place, standing there (his hands up where a ship of the
		 * other side is close), and at his turn out to the edge of her casing, where RunWrecks takes him into the water as
		 * the man he is (his key and his look kept). He is stepped, never placed, so he never jumps. */
		struct DeckHand
		{
			ManOut O;
			int W = -1, N = 0;                    // her wreck (Wrecked's place); his place in her men's order (her W.Out as he goes)
			int Via = 0;                          // her bridge (down her tower's after face), her fore hatch, her after hatch
			double Along = 0.0, Across = 0.0, Z = 0.0;
			double PlaceAlong = 0.0, PlaceAcross = 0.0; // his place on her casing (his slot: Muster's)
			int Rank = 0;                         // his place's rank, 0 at her casing's edge, inboard from it
			double Side = 1.0;                    // the side he goes over (+1 starboard)
			double OutAt = 0.0, TurnAt = 0.0;     // he comes up; his turn to go over
			int Leg = 0;                          // her crew: out of his hatch, to his place, out to her edge; a boarder: up her side, along to her after hatch, in to it, down it (and, sent down, back: BackLeg)
			bool bOut = false, bAtEdge = false, bUp = false, bBelow = false;
			bool bParty = false;                  // her captor's boarding party (SendBoarders): never her crew, never counted in her men
			bool bHurried = false;                // the battle has sent her down: up his hatch and over at a run (Hurry)
			Vec At;                               // his feet in the world as she rides now (StepHands), where he goes over from
		};
		/** A boat taken (sea:surrender how "captured"): her captor's whaler lowered from his davits and pulled across to her
		 * (round her bow or stern to her lee side where he lies to windward), and its party up her side and down her after
		 * hatch to shut her vents; three below and she is a prize. Sent down after all, her party back into the whaler, which
		 * pulls back to him and is hoisted in. */
		struct Boarding
		{
			int Boat = 0, By = -1;                // her key; her captor's (the ship of the piece she struck to nearest her)
			FloatOut Whaler;                      // (Ship: her captor's key, so it is drawn in his navy's colours)
			std::vector<ManOut> Oars;             // its crew, pulling, its coxswain at its stern, and its party until each goes up her side (and once back)
			std::vector<int> Seated;              // each one's seat (StepHands' SeatOf): 0 to 5 the oars, 6 the coxswain, 7 to 11 its party
			Vec Pull;                             // its way this step (metres a second), carried on to the moment drawn
			double Hand = 1.0, Davit = 0.0;       // the side of her captor its davits are on (+1 starboard); their height over the sea
			double Inboard = 1.0;                 // its own side that lies toward her alongside (+1 its starboard), where its party sits
			double LoweredAt = -1.0, AlongsideAt = -1.0, PrizeAt = -1.0, HomeAt = -1.0, HoistAt = -1.0;
			int Party = 0, Below = 0;
			/** Its leg (StepHands): 0 shoving off his side, 1 across to off her side, 2 in alongside her; going back, 3 shoving
			 * off her side, 4 across to off his davits, 5 in to them. */
			int Leg = 0;
			bool bStowed = false;                 // hoisted in: no longer drawn
		};
		std::vector<Wreck> Wrecked;
		std::vector<Floater> Afloat;
		std::vector<Man> Swimmers;
		std::vector<DeckHand> OnCasing;
		std::vector<Boarding> Boardings;
		std::vector<ManOut> OutHands;
		std::vector<FloatOut> OutWhalers;
		std::vector<SlickOut> Oil;
		int NextFloat = 1, NextMan = 1;
		bool bWrecksMoved = false; // a step has moved them since Output last laid them out
		/** The ships' clock (stepped by Fixed) and the moment drawn, a little after it. The steps are summed from the first
		 * view's moment (ClockFrom; Elapsed the sum), not onto it: a convoy night's window opens hours into the battle's own
		 * clock, where a sum of twentieths onto that falls off the views' moments by more than the 1e-9 s they are matched
		 * to within a few minutes (1.5e-8 s by 1000 s from 11340 s), and a view's events and its counts came a step from
		 * their moment; summed from the window's start it stays as close as a battle's from 0 (3.6e-10 s by 1000 s), and a
		 * battle from 0 is stepped as it always was. */
		double ShownT = 0.0, DrawT = 0.0, ContactT = -1.0;
		double ClockFrom = 0.0, Elapsed = 0.0;
		static constexpr double Fixed = 0.05;
		/** An abandoned ship (sea:abandon): her going down drawn over this long until the battle says she goes (she settles
		 * a sixtieth of the way a minute while her men leave her), and the way she loses a second, her engines stopped. */
		static constexpr double AbandonedFor = 3600.0, AbandonedSlowing = 0.3;
		/** The share of the wind's speed an abandoned ship drifts at, empty and stopped: far more than her men in the water (a
		 * sixtieth) or her rafts (a thirtieth), so that she drifts away from them (the art director, 22 November 18:54Z). */
		static constexpr double AbandonedLeeway = 0.05;
		/** The most a pattern's charge lies off its place in the pattern (metres: a few as they fell, about 3 either way). */
		static constexpr double ChargeScatterMost = 7.0;
		/** A man come to a rescuer's side: the least time from his setting off for it to his going up it (seconds), for a net
		 * to be down there before he is on it (Tools/convoy_looks: ConvoyLook's NetDrop, two seconds, from when he is first
		 * seen swimming for her, and a frame's grace). One brought near her on a float waits at its foot for the rest. */
		static constexpr double NetDownFor = 2.5;
		/** A boat's keel told as it changes (Piece::KeelRate): the fastest change between two views read as a ramp to follow
		 * (metres a second: just over the convoy field's own, 2 m/s up and 1 m/s down; a faster one is a told jump), and how
		 * fast she takes up such a ramp's rate (metres a second, each second) and gains on it where she is behind it (each
		 * metre behind, a second, at most KeelGainMost m/s past its rate, and in all at most KeelRampOver times it). She
		 * brakes at KeelBrake of KeelTakeUp, so that her rate can always follow her braking, and stops KeelShort (metres)
		 * of where she could, her own easing taking her the rest of the way. */
		static constexpr double KeelRampMost = 2.5, KeelTakeUp = 0.5, KeelGain = 0.5, KeelGainMost = 1.0, KeelRampOver = 1.5, KeelBrake = 0.9;
		static constexpr double KeelShort = 0.04;
		/** A boat blown up for the surface (Show::Trim): the sea stops her as her casing breaks it, KeelTopBrake m/s each
		 * second, while her angle is at its most (12 degrees: stopped, above a third of a metre a second up), until she
		 * rises at KeelTopEase times the rise that gives it; from there her own braking, KeelBrake of KeelTakeUp, so that her
		 * angle comes off smoothly. */
		static constexpr double KeelTopBrake = 2.0, KeelTopEase = 1.25;
		/** A boat deep catching up her station once she goes astern no longer (Hull::bCatching): near it again within CatchNear
		 * metres, the battle's course within 60 degrees of her heading, she keeps it as before; till then she steers for where
		 * it will be when she has closed it at her way (CatchLead seconds on at the most), her way the battle's and CatchGain
		 * m/s more a metre it lies ahead of her (less where it lies abaft her beam, to six tenths of the battle's). */
		static constexpr double CatchNear = 8.0, CatchGain = 0.2, CatchLead = 30.0;
		bool bStarted = false;
		bool bBigBlast = false;
		int NextKey = 1;
		Vec WindV;
		double Beaufort = 3.0;
		double WindU = 0.0, WindFromR = 0.0;
		Checks Check;

		void Interpolate();
		void Play(const Event& E);
		void StartColumns();
		void Steam(double Dt);
		void SteamColumn(Column& C, const Piece& P, double Dt);
		/** A point of a column's track by how far its leader had steamed (S), and his heading there. */
		static void Along(const Column& C, double S, Vec& OutP, double& OutHdg, double* OutK = nullptr);
		void Gunnery(double Dt);
		void LandShells();
		void RunTorpedoes(double Dt);
		/** Brian, 25 November 12:52Z: each of a held ship's tubes fired where its way is clear now (from her own place; from a
		 * third of the run, or once she is out of her line, the salvo's firing point too): whether any are still held. */
		bool FireTubes(Held& R);
		/** The places a held ship may fire from now. */
		void PlacesOf(const Held& R, std::vector<FirePlace>& Out) const;
		/** A held tube's track as it would be fired from a place now (OnlyPiece: only that piece's ships in its way, -1 every
		 * ship of her side): whether its way is clear (bAll: every ship across it found, else it stops at the first). */
		bool PlanTube(const Held& R, size_t Tube, const FirePlace& From, int OnlyPiece, TubeWay& W, bool bAll = false) const;
		/** Each ship's ways as HeadsClear foresees them (her helm held or eased: one for a follower on her leader's track, or a
		 * ship not turning), a quarter second apart, Upto seconds on at the least; kept for the step they were foreseen on
		 * (every held tube looked at in a step asks of the same ships), longer as they are asked for. */
		const std::vector<std::vector<std::pair<Vec, double>>>& Foreseen(const Hull& O, double Upto) const;
		mutable std::map<int, std::vector<std::vector<std::pair<Vec, double>>>> ForeseenWays;
		mutable double ForeseenAt = -1.0;
		/** A held tube fired as planned (its entry taken out of hers). */
		void LaunchTube(Held& R, size_t Tube, const FirePlace& From, const TubeWay& W);
		/** Why a held tube's way is not clear now, at the places it may be fired from: 0 her own piece's ships alone, 1 other
		 * pieces' ships in her salvo's water ahead of its heads all that stand in it at one of them (the rules' side), 2 the
		 * scene's own otherwise. */
		int WhyHeld(const Held& R, size_t Tube, const std::vector<FirePlace>& From) const;
		/** A held tube never drawn (bRoom: for the room), judged over its hold (TracksDroppedOwn, TracksDroppedWater; its entry
		 * taken out). */
		void DropTube(Held& R, size_t Tube, bool bRoom = false);
		/** Whether a point lies in a salvo's water as the rules have it (Pad: metres round it; S, where given, how far along
		 * from the piece's place toward the aim). */
		bool InWater(const Held& R, const Vec& Q, double Pad, double* S = nullptr) const;
		/** Whether an own ship lies in a salvo's water ahead of its heads as the battle's moment has them now (bow, middle or
		 * stern, her half beam round each): where the rules' switch keeps her out. */
		bool InWaterAhead(const Held& R, const Hull& O) const;
		/** Whether a track's head, from Start along Dir at Speed over its run's first Length metres, stays clear of every ship
		 * of a side as each steams on, turning as she is or easing her helm, her way as much as WayDoubt faster or slower than
		 * now (Her, the ship it leaves, by 3 m; the rest by 5 m; Skip left out; only OnlyPiece's, where 0 or more); Foul,
		 * where given, every ship that would come across it (each once), else it stops at the first. */
		bool HeadsClear(const Vec& Start, const Vec& Dir, double Speed, double Length, int Side, int Her, int Skip, int OnlyPiece, std::vector<int>* Foul = nullptr,
		                int* First = nullptr) const;
		/** Whether a track's line from A to B clears every ship of a side (but Except; only OnlyPiece's, where 0 or more) by
		 * TrackClear beyond her half beam; Least, where given, the least it clears any by; Foul, where given, every ship it
		 * does not clear. */
		bool WayClear(const Vec& A, const Vec& B, int Side, int Except, double* Least, int OnlyPiece, std::vector<int>* Foul = nullptr, int* First = nullptr) const;
		/** The track of a piece a torpedo hit on a ship ends: of those running, the nearest her that has run at least a third
		 * of its salvo's run with its way to her clear of its own side, else the nearest that has run so long, else the
		 * nearest; none running, a held tube of the piece fired at her now from a clear stretch short of her side, or one
		 * from the battle's moment's (a last resort, TubesAtHit). */
		Torp* TrackForHit(int Piece, const Hull& Struck);
		void FlyStrikes(double Dt);
		void LayScreens();
		void CoverFleet(double Dt);
		/** Ruling 241: each boat's keel eased to the depth the battle has her at; the depth charges burst at their moment. */
		/** (returns how much of the swell still moves her, 1 surfaced; OutSink: metres a second her keel is going down) */
		double Trim(Hull& H, const Piece* P, double Dt, double& OutSink);
		void BurstCharges();
		/** Where she goes down: her crew, her rafts and boats, her wreckage and her oil, set going (Sink). */
		void Abandon(const Hull& H);
		/** Her crew ordered off her (sea:abandon): out of her line, losing way and settling, her men and boats going over
		 * the side, until the battle says she goes (sea:sunk). */
		void AbandonShip(Hull& H, const std::string& Words);
		/** A boat whose crew leave her (sea:abandon, or given up untold): they leave her only on the surface, so she comes the
		 * rest of the way up as one that struck does (her tanks blown), and her men, her raft and her boats wait for her
		 * casing to clear (StepHands lets them go). */
		void LeaveUp(Hull& H);
		/** The battle's own count (a view's ships for each piece) against her line, once each step's events are played: a ship
		 * it took out of the fight with no word of how (none of sea:sunk, burning, abandon or surrender told for her by
		 * then; a merchant gone at her torpedo's hit, her sinking told minutes later or after the night's last view) is
		 * given up as one abandoned, stopped and settling, her men over the side, until the battle sends her down. The ship
		 * of her piece a blow struck last, else the last of her line. Upto: the moment the picture has reached. */
		void Untold(double Upto);
		/** A lost ship's own number in her piece ({piece, n}), counted as each goes (n, where the battle gives it, else the
		 * next); and her wreck by the battle's {piece, ship} (nullptr where none). */
		void NoteLost(const Hull& H, int N);
		Wreck* WreckOf(int Piece, int Ship);
		/** Her men, rafts, wreckage and oil over a step: over the side, swimming, holding on, drifting, picked up. */
		void RunWrecks(double Dt);
		Floater* FloatByKey(int Key);
		/** A boat beaten on the surface strikes (sea:surrender): her flag up, out of the fight as one abandoned, her men to
		 * come out on her casing once she is up (Muster), and, taken, a boarding party sent across (SendBoarders). */
		void StrikeColours(Hull& H, const Event& E);
		/** Her men's plan on her casing from her N'th man on (all of them as she comes up; those the battle adds later). */
		void Muster(Wreck& W, const Hull& H, int From);
		/** The battle sends her down: every man still to come up or go over hurried, the last over within HurryFor of it. */
		void Hurry(Wreck& W);
		static constexpr double HurryFor = 5.0;
		/** Hurried, her men climb at HurriedClimb (metres a second) and follow one another up each opening HurriedApart
		 * (seconds) behind: three files of them, some fifty men, up and over within HurryFor. */
		static constexpr double HurriedClimb = 1.8, HurriedApart = 0.2;
		void SendBoarders(const Hull& H, int ByPiece);
		/** Where a boat pulling from From to To steers now: straight on where its way clears hull H (her beam out from her
		 * middle line, and Clear more), else round her nearer end by a point off her shoulder, or off that end, first. */
		Vec RoundHull(const Vec& From, const Vec& To, const Hull& H, double Clear) const;
		/** Her crew and any boarding party a step on (Dt), at a man's pace; the whalers pulled across. */
		void StepHands(double Dt);
		/** Where a hand's feet are in the world as she rides now (On: seconds on from ShownT, Output's drawn moment). */
		Vec HandAt(const DeckHand& D, const Hull& H, double On) const;
		/** A boat's keel surfaced (metres: her draught). */
		double SurfacedKeel(const std::string& Class) const;
		/** Each leader's station (bSettle: at the start, each column laid at it at once). */
		void KeepClear(double Dt, bool bSettle = false);
		/** L9: the ships' anti-aircraft guns at the aircraft up, this frame (Dt: since the last), played as the show's Fx. */
		void FireGuns(double Dt);
		std::vector<ShipGuns::Hull> GunHulls() const;
		ShipGuns::Plane GunPlane(const Plane& A) const;
		void GunShots(const std::vector<ShipGuns::Shot>& Shots);
		/** B4: a ship's helm against an air attack: hard over against a dive (the big ships), or round to comb a torpedo
		 * plane's drop (every ship); a column's leader leaves a ghost on its course to come back to. */
		void Dodge(Hull& H, const Plane& A, bool bTorpedo, double Until);
		/** Her rate of turn wished while dodging (false: not dodging, or no longer, as she would close another ship). */
		bool DodgeRate(Hull& H, double Most, double& OutWish);
		/** B4: a dodging leader fallen back level with her next astern hands her the lead and falls in where she can. */
		void HandOver(Column& C, Hull& Old, Hull& New);
		void Debts(const View& Before, const View& After, const std::vector<Event>& Between);
		void Output();
		/** Her hunt (SetMine): nullptr unless the player's side is set and she is a boat of the other side. */
		Hunted* Quarry(const Hull& Boat);
		/** A boat under (her casing below the sea, and not on her way up): out of a lookout's sight. */
		bool BoatUnder(const Hull& Boat) const;
		/** A boat the battle has below periscope depth, and under as drawn: she keeps no water at the surface (KeepClear), and
		 * holds the battle's place itself, where its hunters drop on her (SteamColumn's helm). */
		bool HeldDeep(const Hull& Boat) const;
		/** She is in sight until at least For seconds from now. */
		void Sight(const Hull& Boat, double For = FoundFor);
		/** A blow on her: up, in sight until she is under again; and FoundFor from now either way. */
		void SightHit(const Hull& Boat);
		/** What an event finds (before its cue: one that finds her is heard): an escort's contact gained or lost, a boat
		 * forced up or given up, a gun's hit. */
		void HuntEvent(const Event& E, const Piece* P, const Vec& At);
		/** Each step, after everything that finds her: the escorts her view says hold her, a holder of the events' gone, sunk
		 * or FoundFor past its last attack after an ahead-thrown run letting her go, guns laid on her or hers while she is
		 * up, a boat up once under again, a lookout's eye on her; then whether she is in sight. */
		void Hunt();
		/** Whether the player sees her: every ship but an enemy boat in the fight not found. */
		bool InSight(const Hull& H) const;

		/** The ship of a piece nearest a board point, afloat in its line (null: none). */
		Hull* NearestOf(int PieceId, double X, double Y);
		/** The same, out of its line too while she is still afloat (StillAfloat): a straggler fallen out burning or limping,
		 * a ship her men have left settling until the battle sends her down. A boat's deck gun's and a merchant's gun's
		 * target: a straggler or a ship abandoned is what a deck gun most often finished off. */
		Hull* NearestAfloatOf(int PieceId, double X, double Y);
		/** In her line, out of it limping, or left by her men and not yet sent down (sea:abandon until sea:sunk). */
		static bool StillAfloat(const Hull& H);
		/** The first hit told of each piece's gun still to come (sea:gunHit, of the views in hand) and not yet carried by a
		 * round in the air, by the piece that fired it. */
		std::map<int, const Event*> GunHitsDue() const;
		Hull* FindHull(int Key);
		/** A hull afloat (not Except's) that a point of the sea lies within Margin metres of. */
		bool OnHull(const Vec& At, double Margin, int Except) const;
		/** Keeping clear: the course and way a ship takes for those she wishes (Hdg, Spd, changed in place) to pass clear of
		 * every other ship afloat, looking 45 s ahead along her helm and way (Acc: her way gained or lost a second). */
		void ClearWay(Hull& H, const ClassSize& Sz, double Acc, double Dt, double& Hdg, double& Spd);
		/** Whether H keeps clear of O (the other stands on, and acts only at the last). */
		bool GivesWay(const Hull& H, const Hull& O) const;
		struct NearShip
		{
			const Hull* O = nullptr;
			double Half = 0.0, R = 0.0, Horizon = 0.0, Margin = 0.0;
			Vec V, Fore; // (her way, and her middle to the end of her straight side ahead: the same at every look)
		};
		std::vector<NearShip> Nears; // (ClearWay's own, kept to save making it each step)
		int Rechosen = 0;            // ships that chose again how to keep clear this step
		/** How fast a ship of the class may turn at a speed (radians a second): her class's rate at her cruising speed,
		 * in proportion to her speed about it (her turning circle the same at any speed). */
		double MostRateAt(const ClassSize& Sz, double Spd) const;
		const Hull* FindHull(int Key) const;
		/** A ship of a piece by the battle's place along its line (a zone laid for a ship lost: sim.ts smoke). */
		Hull* ByZone(int PieceId, const Zone* Z);
		void TakeOut(Hull& H);
		void Sink(Hull& H, const std::string& Words, const std::string& How = std::string());
		void LeaveLine(Hull& H, const std::string& Words);
		void LightZone(const Zone& Z, const Hull* H);
		std::string WayDown(const Hull& H);
		Vec Bow(const Hull& H) const;
		Vec PlaceOn(const Hull& H, double Along, double Side, double Up) const;
		double Calibre(const std::string& Class, const std::string& Nation) const;
		double Cadence(const std::string& Class);
		const ClassSize& SizeOf(const std::string& Class) const;
		int Launch(Strike& S, const Vec& From, double Hdg, int Slot);
		Fx& Emit(FxKind Kind, const Vec& At, const std::string& What = std::string(), int Key = 0);
		const Piece* PieceNow(int Id) const;
	};
}
