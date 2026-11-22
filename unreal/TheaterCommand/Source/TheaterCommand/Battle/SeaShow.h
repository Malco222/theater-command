#pragma once

#include "Battle/ShipGuns.h"
#include "Effects/TheaterFxCore.h"

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
 *    ship the battle says was hit, the rest run on and fade.
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
		int Year = 1941, Month = 4;
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
		bool bScope = false;
		int ConvoyColumn = -1;
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
		double Heave = 0.0;  // metres: how far the swell lifts her (plus) or lets her fall from her waterline
		double Speed = 0.0;  // metres a second through the water
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
	};

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
	};

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
		Salvo,       // a ship's guns firing: Key (ship), At (her guns), Aim (where the shells will fall), What (class), Size (calibre mm)
		Splash,      // a shell into the sea: At, What ("heavy", "medium", "light"), Size (calibre mm)
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
		TrackStart,  // a torpedo's track: Key (torpedo), At, Vel
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
		             //   or "ahead"), Size (1: the blow the battle booked on the boat, else 0), Side
		Dive,        // a boat going down or coming up: Key (the boat), At, What ("dive" or "surface"), Text (why), Side
		Starshell,   // a star shell bursting: Key (its own), At (where it bursts, Z its height), Until (battle time it burns out), Side
		Rescue,      // a ship picking up survivors: Key (the rescuer), At (her), Aim (where they are), Until (battle time it ends), Side
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
		double GaveWay = 0.0;       // ship-seconds followers eased their way for a ship of their own line crossing ahead
		double KeptClear = 0.0;     // ship-seconds any ship held an alteration of course or way to keep clear of another
		// Where ships went down: her crew in the water (the real number, and those the show keeps), the most at once, those
		// picked up, those pushed aside by a ship's bow; her floats and her oil.
		int Wrecks = 0, WrecksManned = 0, WrecksDue = 0; // ships gone down; with men in the water; that should have them
		int MenReal = 0, MenKept = 0, MostMen = 0, MenPicked = 0, MenSwept = 0, MenDead = 0;
		int Floats = 0, Rafts = 0, Boats = 0, Slicks = 0, SlicksAlight = 0;
		int MenUnder = 0;           // dead men let go under to make room for the living of a later ship
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

		void SetField(const Field& InField);
		const Field& Board() const { return F; }

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
		/** A ship as shown now, by her key. */
		const ShipOut* Ship(int Key) const;
		/** The pieces as the battle has them now (interpolated), for labels and the camera. */
		const std::map<int, Piece>& Pieces() const { return Now; }
		double ContactAt() const { return ContactT; }
		const Checks& Checked() const { return Check; }
		/** One line a ship and a strike: her piece, her place, what she is doing (a debug overlay; the cloud test's trace). */
		std::string Describe() const;

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
			double Back = 0.0;      // a follower: metres of track behind her leader, as she has closed up so far
			double SOn = 0.0;       // a follower: where she is along his track (how far he had steamed when he passed there)
			double Roll0 = 0.0, RollT = 10.0, PitchT = 7.0; // her own phase on the swell, and her roll and pitch periods
			double Train = 0.0, Elev = 0.0;
			int AimKey = -1;
			double NextSalvo = -1.0; // battle time of her next salvo (below 0: not yet laid)
			int Engaged = -1;
			bool bFiring = false;
			double FiredAt = -1.0;
			// out of the line: limping away burning, or going down
			double LeftAt = -1.0, Away = 0.0, NextFire = 0.0, LimpSpd = 5.0;
			std::string How;
			double SinkAt = -1.0, SinkFor = 120.0;
			// smoke laid
			double ScreenUntil = -1.0;
			bool bScreen = false;
			std::string LastBlow;  // the weapon of the last hit drawn on her (for the way she goes down)
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
			bool bScope = false;
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
		};

		struct Torp
		{
			int Key = 0, Side = 0, Piece = 0;
			Vec P, Way, From;
			double Speed = 20.0, Ends = 0.0, Launched = 0.0;
			Vec Aim;
			bool bRunning = true;
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
			Vec Off;
		};

		Field F;
		Dice Roll;
		std::deque<View> Views;
		std::deque<Event> Queue;
		std::map<int, Piece> Now;
		std::map<int, Column> Lines;
		std::map<int, Hull> Hulls;
		std::vector<Shells> Flying;
		std::vector<Torp> Torps;
		std::map<int, Plane> Air;
		std::vector<Strike> Strikes;
		std::map<int, Owed> Debt;         // by piece: hits the battle has taken off its hull not yet drawn
		std::map<int, double> LastStr;     // by piece: its hull as the view before had it
		std::map<int, double> AirBlowAt;   // by piece: battle time of the last strike, raid or torpedo hit on it
		std::vector<Zone> Zones;
		std::vector<Charge> Charges;   // depth charges sinking, not yet burst
		std::map<int, double> AheadAt; // by escort piece: battle time of its last ahead-thrown salvo (a hit bursts them)
		ShipGuns::Gunnery Gunners;   // L9: the ships' anti-aircraft guns
		bool bGunsUp = false;        // aircraft were up for them last frame
		double GunDue = 0.0;         // battle seconds since they were last laid
		std::set<int> ZonesLit;
		std::map<int, int> ZoneShip;       // by zone id: the ship it was laid for (sunk, burning)
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
			int Men = 0, Out = 0, Dead = 0;               // men the show keeps, gone over so far, of them dead
			int Rafts = 0, RaftsOut = 0, Boats = 0, BoatsOut = 0, Bits = 0, BitsOut = 0;
			Vec Oil0;                                     // where her oil began
			int Slick = -1;                               // its key
			double Burn = 0.0, BurnUntil = -1.0;          // the share of it alight, and until when
			double Lee = 1.0;                             // her windward side (+1 starboard, -1 port)
			int Rescuer = -1;                             // the ship picking her men up (key), -1 none
			double RescueUntil = -1.0;
			uint64_t Seed = 0;                            // her own hash's start
			int Real = 0;                                 // men of hers who went into the water in truth
			double DeadShare = 0.0;                       // of them, the share dead in it
			Vec OilEnd, Drift;                            // where her oil ended (where she went under), and how far it has drifted
			double TrackHdg = 0.0, TrackLen = 0.0;        // her oil's way along her last track
			bool bGone = false, bLit = false, bDue = false;
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
		};
		std::vector<Wreck> Wrecked;
		std::vector<Floater> Afloat;
		std::vector<Man> Swimmers;
		std::vector<SlickOut> Oil;
		int NextFloat = 1, NextMan = 1;
		bool bWrecksMoved = false; // a step has moved them since Output last laid them out
		/** The ships' clock (stepped by Fixed) and the moment drawn, a little after it. */
		double ShownT = 0.0, DrawT = 0.0, ContactT = -1.0;
		static constexpr double Fixed = 0.05;
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
		void FlyStrikes(double Dt);
		void LayScreens();
		void CoverFleet(double Dt);
		/** Ruling 241: each boat's keel eased to the depth the battle has her at; the depth charges burst at their moment. */
		/** (returns how much of the swell still moves her, 1 surfaced; OutSink: metres a second her keel is going down) */
		double Trim(Hull& H, const Piece* P, double Dt, double& OutSink);
		void BurstCharges();
		/** Where she goes down: her crew, her rafts and boats, her wreckage and her oil, set going (Sink). */
		void Abandon(const Hull& H);
		/** Her men, rafts, wreckage and oil over a step: over the side, swimming, holding on, drifting, picked up. */
		void RunWrecks(double Dt);
		Floater* FloatByKey(int Key);
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

		/** The ship of a piece nearest a board point, afloat in its line (null: none). */
		Hull* NearestOf(int PieceId, double X, double Y);
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
