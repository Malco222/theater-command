#pragma once

#include "Effects/TheaterFxCore.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <utility>
#include <set>
#include <string>
#include <vector>

/**
 * The air battle acted out, in plain C++: how every aircraft of a formation flies, in formation and in the fight, and
 * what it does that the effects kit and the sound show (its guns, a hit, a fall, a straggler's smoke, flak round a box,
 * a stick of bombs). The same code runs in the game (AAirBattle hands it the battle's views and draws what it says)
 * and in the cloud, where tools/air-show plays a recorded battle through it and checks the flight (ruling 238, the air
 * battles thread's 3D handoff, sections 2, 9 and 10).
 *
 * The battle (src/battle/air) decides everything that counts: where each group is, its height, who it fights, who is
 * lost and how. Nothing here changes that. What is decided here is only what the battle leaves open: which model of a
 * formation falls when the group loses an aircraft, the line each fighter flies on its pass, the second its guns fire.
 *
 * Every aircraft flies as an aircraft does (Brian, 19 November: "the german plane outside not moving normal"; the
 * handoff's section 10): its nose along its path, no sliding sideways; banked into every turn as the turn needs
 * (bank = atan(v squared / (g r))); no turn tighter than 210 m; no more than 6 g, nor 160 degrees a second of roll;
 * speed lost in a climb and gained in a dive; never hovering, an escort at its bombers' speed weaving beside them.
 * Its passes are the historian's: the pass the contact names, three breaks in four a half-roll and dive beneath the box,
 * the climb back ahead for another pass, bursts of one to two seconds.
 *
 * Places are in board metres (X east, Y north, Z up: the battle's own board, the sea at 0). Headings are radians
 * clockwise from north, as the battle's own. Time: the battle's clock for the views and their events; the air clock
 * (the battle's times its scale, SKY.scale) for flight, guns and smoke, as the effects kit wants it.
 */
namespace AirShow
{
	using Vec = TheaterFx::Vec;
	using Dice = TheaterFx::Dice;

	enum class Act { Idle, March, Fight, Rout, Down };

	/** One gun of a loadout (src/battle/air/guns.ts): its kit row, mount, sockets and arc. */
	struct Gun
	{
		std::string Kind = "rifle";   // the effects kit's air_guns row: rifle, hmg, cannon20
		std::string Mount = "fixed";  // fixed, turret, hand
		std::vector<std::string> At;  // the model's sockets ("gun.nose", "gun.1", "turret.3")
		int N = 1;
		double Rate = 0.0;            // rounds a minute, each gun
		double Mv = 800.0;            // metres a second
		bool bArc = false;            // a gunner's arc: yaw [from, to] clockwise from the nose, degrees; pitch [low, high]
		double Yaw0 = 0.0, Yaw1 = 360.0, Pitch0 = -90.0, Pitch1 = 90.0;
	};

	struct Loadout
	{
		std::vector<Gun> Guns;
		double Conv = 0.0;                // where the wing guns cross, metres (0: none)
		std::vector<std::string> Silent;  // muzzles dark that month
		double ReachRifle = 370.0, ReachHeavy = 550.0; // how far a gunner fires
	};

	/** A bomber fallen out of its box (extra.strag). */
	struct Strag
	{
		int Id = 0;
		std::vector<int> Eng;  // its dead engines, from 1
		bool bFeather = true;  // its dead airscrew feathered, or windmilling
		bool bFire = false;    // its dead engine burning
		int Cover = -1;        // the escort group over it
		double Back = 0.0, Down = 0.0; // metres behind and below its box, along the box's heading
	};

	/** Whom a fighter group is firing at this tick, and how hard (extra.fire). */
	struct Fire
	{
		int On = -1;
		double Rate = 0.0;
		std::string Pass;
		/** The beat of the passes on a box (the rules' beat; the defenders' fighters): going in, or standing off to form
		 * up again, and the air seconds of it left. -1: not said (an older recording, or no beat on this enemy). */
		double Go = -1.0, Wait = -1.0;
	};

	/** One air group as a view gives it. */
	struct Group
	{
		int Id = 0;
		int Side = 0; // 0 the raider, 1 the defender
		bool bFighter = true;
		std::string Name, Model, Nation, State, Form, Style, Box, Role, Strike, Job;
		std::string Cool;     // its engines: liquid or radial (the rules' extra.cool; empty in an older recording)
		/** The view named no model: the place's main one of its kind stands in until a view names it. */
		bool bModelGuessed = false;
		double X = 0.0, Y = 0.0, Alt = 0.0, Face = 0.0, W = 400.0, D = 300.0, Speed = 0.0;
		double Ammo = 1.0, Run = 0.0;
		/** The pilots' brief (shown, never counted; -1 where an older recording does not say): the enemy group a fighter
		 * group is making for, the bomber group an escort guards, and its pilots' skill, 0 to 1. */
		int Quarry = -1, With = -1;
		double Skill = -1.0;
		int Figures = 0, Per = 1, N = 0;
		Act Doing = Act::March;
		int Engaged = -1;
		std::vector<Fire> Firing;
		std::vector<Strag> Strags;
		std::vector<std::string> Flak;
	};

	/** One event of a view, with what the show needs of its extra. */
	struct Event
	{
		double T = 0.0;
		std::string What, Cue, Text, ChuteCue;
		int Unit = -1, Side = -1;
		double X = 0.0, Y = 0.0;
		bool bAt = false;
		std::string Kind;     // air:lost: explode, burn, spin
		int Chutes = 0;
		double Alt = -1.0;
		int By = -1;          // the group that brought it down; -1 flak or none
		bool bByFlak = false;
		int Strag = -1;       // a straggler's id (air:lost, air:straggle, air:limped)
		std::vector<int> Eng;
		int With = -1;        // contact: the other group
		std::string Pass;     // contact: the pass flown
		int Ship = -1;        // air:ship-hit, air:ship-sunk
		std::string How, Strike;   // air:lost: the way down (explode ... collide); air:ship-sunk: how she goes
		bool bBig = false;
		std::string Name;
		// air:lost's way down (the air rules decide it, Brian 11-20 14:23Z: "many different ways a plan can go down"):
		int Fid = -1;              // this loss's own number; With, for struck and collide, the other's
		std::string Cause;         // struck, collide: debris, falling or ram
		int Hand = 0;              // the wing, tail or spin's side: -1 port, +1 starboard, 0 not said
		int Roll = 0;              // +1 or -1: which way it rolls or spins
		int Look = 0;              // 0 to 3: which of its way's looks
		double EngineAt = -1.0;    // which engine, 0 (port) to 1 (starboard)
		double FireAt = -1.0, Boom = -1.0, Dur = -1.0; // air seconds from the hit: the fire shows, it blows apart, it strikes
		bool bDitch = false;       // a glide onto the sea under control
		int Out = 0;               // crew seen climbing out after a ditching or crash-landing
		std::vector<double> ChuteAt; // air seconds from the hit when each parachute opens
		// air:hit (a hit survived): what shows, for how long, on which model of the formation
		std::string Mark;          // holes, smoke, pieces or fireout
		double For = 0.0;
		int Slot = -1;
		bool bFire = false;        // air:straggle: it falls out burning
		bool bFeather = false;     // air:straggle, air:fireout, air:regain, air:hit fireout: its dead airscrew feathered
		int GunBox = -1;           // air:lost, a fighter: the box whose gunners brought it down (-1: not gunners)
	};

	/** A ship under attack, as the view's ships give it. */
	struct Ship
	{
		int Id = 0;
		Vec At;
		double Face = 0.0;
		std::string Class;
		bool bSunk = false;
	};

	/** What the show asks of the effects kit, the stage and the sound this frame (all on the air clock). */
	enum class FxKind
	{
		Gun,        // a gun firing this frame: Key, At (muzzle), Aim, Vel (the shooter's), What (kind), Text (mount), Rate, Mv
		GunStop,    // Key
		Hit,        // rounds striking an aircraft: At, Normal, Vel (the aircraft's), What (the gun's kind)
		TrailStart, // Key, At, Vel, What: smoke, fire, glycol; Size the burning aircraft's span (or the piece's size);
		            //   Text "piece" for a piece of a blown-apart aircraft, else the aircraft's own trail
		TrailMove,  // Key, At, Vel
		TrailStop,  // Key
		Flak,       // At, What: light, medium, heavy
		Blast,      // an aircraft blowing apart: Key (the aircraft), At, Vel, Size (its span)
		Fall,       // an aircraft lost, for its debris (the effects kit's AirFall): Key (the aircraft), At, Vel,
		            //   What (explode, burn, spin), Size (its span), Deck (metres: where its pieces leave sight)
		FallMove,   // a burning or spinning fall, every frame: Key, At, Vel; Deck -1 once, where it passes under the
		            //   deck through a hole, its debris row having been told the deck (the row stops shedding there)
		FallStop,   // the fall over, the model gone: Key
		Splash,     // something into the sea: At, Size (metres across)
		Crash,      // something into the land: At, Size
		BombHit,    // a bomb landing: At, What ("water" or "land"), Size (kg class: 250 or 500), Text "stick" for the
		            //   first of a stick of two or more (heard as the stick), "one" for a bomb alone, else empty
		Sound,      // a one-shot: What (the sound director's name), At
		Dispatch,   // an event's words, when it is shown: What (the event), Text, Side, Key (its unit)
		ShipHit,    // the stage's Hit: Key (ship), What (strike)
		ShipSunk,   // the stage's Sink: Key (ship), What (how), Size (1 the biggest blast), Text (the line)
		Smoke,      // a bombed target's column (air:rail, air:ruin): At
	};

	struct Fx
	{
		FxKind Kind = FxKind::Sound;
		int Key = 0;
		Vec At, Aim, Vel, Normal;
		std::string What, Text;
		double Rate = 0.0, Mv = 0.0, Size = 0.0;
		double Deck = 0.0;
		int Side = -1;
	};

	/**
	 * The effects kit's room for falling debris (its AirFall, kit head 44af184). A fall's pieces hold places among
	 * the kit's puffs until they sink into the deck, up to 240 s for the light plates; a bomber blown apart holds about
	 * 100, a fighter about 50, a burning aircraft up to 10 over its fall, a spinning one up to 5. A fall is given its
	 * debris only while the pieces already falling leave room under Most, so the trails and the flak always keep
	 * theirs (the kit drops the farthest puffs first when it is full; a trail holds about 240). A burning or spinning
	 * fall leaves Keep of it for the next aircraft blown apart, whose debris row is the one the eye reads. Past that a
	 * blown-apart aircraft keeps the older torn flame and its few big pieces, and a burning or spinning one its trail.
	 * Times are the core's AirNow.
	 */
	struct DebrisRoom
	{
		int Most = 700, Keep = 100;
		int Peak = 0, Given = 0, Refused = 0, RefusedBlasts = 0;
		/** Puffs the falls given so far still hold at T. */
		int Holding(double T);
		/** A Fall effect: true if it is given its debris (and its puffs are counted from T). */
		bool Take(double T, const Fx& Fall);

	private:
		struct Lot
		{
			double Until = 0.0;
			int Puffs = 0;
		};
		std::vector<Lot> Lots;
	};

	/**
	 * The scene's room for smoke trails (the pre-check, 06:08Z). A trail lit holds about 240 of the effects kit's
	 * puffs; the kit from 1295edc keeps every flame on a full screen and sheds the farthest smoke first, so the scene
	 * lights up to 24 at once on that kit, 10 on an older one. A burning aircraft's own trail comes first: with the
	 * room full it takes the place of the oldest piece's trail (that piece's smoke stops and hangs), so no aircraft
	 * the dispatch says goes down burning falls without its flame. Plain C++, played by the cloud test too.
	 */
	struct TrailRoom
	{
		int Most = 10;
		int Peak = 0, Refused = 0, RefusedOwn = 0, PutOut = 0;
		/** A TrailStart. True if it is lit (or was already); OutPutOut: the key of a piece's trail put out to make room
		 * for it (to stop in the kit), else 0. bPiece: a trail that gives way to an aircraft's own when the room is
		 * full (a piece's, or a dead engine's thin "streak", which never clears); a lit trail's follows its newest kind. */
		bool Start(int Key, bool bPiece, int& OutPutOut);
		bool On(int Key) const;
		/** A TrailStop: true if it was lit. */
		bool Stop(int Key);
		int Count() const { return static_cast<int>(Lit.size()); }

	private:
		struct One
		{
			int Key = 0;
			bool bPiece = false;
		};
		std::vector<One> Lit; // in the order they were lit
	};

	/** One aircraft as it is to be drawn. */
	struct PlaneOut
	{
		int Key = 0;
		int Group = 0;
		int Side = 0;
		bool bFighter = true;
		std::string Model, Nation;
		Vec At;
		/** Degrees: heading clockwise from north, pitch nose up, bank right wing down. */
		double Heading = 0.0, Pitch = 0.0, Bank = 0.0;
		double Span = 12.0;
		bool bShown = true;
		bool bOnGround = false;
		bool bFalling = false;
		bool bFiring = false;  // a fighter: its guns firing this frame
		int Target = -1;       // a fighter in the fight: the plane it is after
		/** A fighter: what its pilot is about, for the ride cameras to pick the moment by. "" in formation with nothing
		 * in hand; "closing" on the enemy it is making for; "escorting" by its bombers; in a fight "attacking" (a pass
		 * or a tail chase), "breaking" (away after a pass), "evading" (an enemy on its tail), "covering" (after the enemy
		 * on its partner's tail, or an escort chasing an attacker off its box), "positioning" (climbing back, or waiting
		 * its turn to go in), "going home" (hit, or its guns empty). */
		std::string Task;
		/** A fighter flying another's wing: his leader's key (-1: he leads, or flies alone). */
		int Leader = -1;
		double Speed = 0.0;    // metres a second along its path
		/** Its dead engines (from 1, as the model's engine.N), their airscrews feathered (stopped) or windmilling, and
		 * how far the soot behind them has built, 0 to 1 over about 30 s from when the engine died (the art director:
		 * it builds over 20 to 40 s and stays, after a fire is out and back in the box too). */
		std::vector<int> DeadEngines;
		bool bFeathered = false;
		double Soot = 0.0;
		/** A bomber's bay doors, 0 shut to 1 open (the model's bay.port and bay.starboard turned that share of their
		 * travel): opened on the last seconds of the bomb run, held open while the stick falls away, then shut. */
		double Bay = 0.0;
		/** Its stick's last bomb has left the bay: the load the model racks there is gone (kept in place until then, so
		 * that each bomb seems to come out of it; a bomber shot down before it dropped takes hers with it). */
		bool bBombsGone = false;
	};

	/** The small things drawn in one mesh: a parachute, a piece of a wreck, a bomb or a torpedo. */
	enum class ThingKind { Chute, Piece, Bomb };
	struct Thing
	{
		ThingKind Kind = ThingKind::Piece;
		int Key = 0; // its trail, for a piece
		Vec At, Vel;
		double Spin = 0.0, Size = 1.0, Age = 0.0, Heading = 0.0;
		bool bOpen = false;
		bool bTorpedo = false;
		/** A bomb: on the first of its stick, the stick's size (0 on the rest: the first's landing is heard as the
		 * stick's); and whether its fall's last moment has been weighed for its rush (mix impacts.incoming.bomb). */
		int Lead = 0;
		bool bRushed = false;
		/** A piece that flutters down slowly, as a torn wing or panel does, at about this many metres a second (0: it
		 * falls as a stone does); and how long it lives, air seconds. */
		double Flutter = 0.0, Life = 400.0;
		bool bTrail = false; // it trails fire or smoke (its Key the trail's)
		/** 1 while it may still pass into the deck, and passes out of sight in any drawn tile it falls into (under one
		 * tile's foot it may yet fall into a lower tile beside it); 0 once it is under the deck's laid base, or there is
		 * no deck, and it falls on to the ground or the sea. */
		double Deck = 0.0;
	};

	/** An aircraft's size and where its engines and gun stations are, in its own frame (X forward, Y right, Z up). */
	struct Shape
	{
		double Span = 11.0, Long = 9.0;
		std::vector<Vec> Engines;                // engine.1 (port outer) onward
		std::map<std::string, Vec> Stations;     // nose, chin, top, ball, waistL, waistR, tail, radio
		bool bDive = false;                      // a dive bomber: brakes, a 5 g pull-out
	};
	/** The built-in shape of a model, by name, until the model files carry their sockets. */
	Shape ShapeOf(const std::string& Model, bool bFighter);
	/** Where a slot of a group's formation lies (the flyover frames its lead three by it). */
	Vec SlotOf(const Group& G, int Slot);

	/** The flight's own checks, kept as the show runs (the cloud test reads them). */
	struct Checks
	{
		double MostSlip = 0.0;        // degrees between the nose and the path, outside a spin (by construction 0)
		double LeastRadius = 1e9;     // metres, of any turn, outside a fall
		double MostG = 0.0;           // outside a fall
		double MostRoll = 0.0;        // degrees a second
		double LeastFightSpeed = 1e9, MostFightSpeed = 0.0; // a fighter on a pass or in a turning fight, m/s
		double MostJump = 0.0;        // metres an aircraft moved in a frame beyond its speed (a pop)
		int Falls = 0, Chutes = 0, GunFrames = 0, Hits = 0, FlakBursts = 0, Bombs = 0, Passes = 0, UnderBreaks = 0, OverBreaks = 0;
		int MostGunsAtOnce = 0, SharedWindows = 0;
		/** Sticks let go and their bombs; bombs that left a bay whose doors were not yet open (a model with no bay
		 * draws none, so it counts only the core's own doors); bays still open 8 s after their stick was gone. */
		int Sticks = 0, StickBombs = 0, BombsThroughShutDoors = 0, BaysLeftOpen = 0;
		int Rushes = 0; // falling bombs' rushes given (bomb_release, ending at the landing, within 400 m of the eye)
		std::map<std::string, int> FallsBy;  // falls begun, by the way down they play
		int FallsOnTime = 0, FallsOffTime = 0; // falls that struck the sea or ground within a quarter of the battle's dur, or not
		double MostOffTime = 0.0;            // air seconds, the worst of those
		int ChutesOffTime = 0;               // parachutes not opened when the battle said
		int HitsShown = 0, FiresOut = 0, Regained = 0, Ditched = 0;
		// The fighter pilots (the pilots' section of AirShow.cpp):
		int Fights = 0;                    // fighter groups that came to blows
		int Waves = 0;                     // passes the battle's beat opened on a box
		double BeatIn = 0.0, BeatOut = 0.0; // air seconds of fighters' fire on a box with a beat: in the pass (or within 3 s after it), standing off
		int Threats = 0, Evasions = 0, SplitS = 0, Reversals = 0, Covers = 0, YoYos = 0; // an enemy seen on a tail; what was done about it
		int Scissors = 0, ScissorTurns = 0, ScissorWins = 0; // scissors begun; the times a pilot in one rolled back into the other; those won (one came out behind the other)
		double LongestScissors = 0.0;      // air seconds, the longest of them
		int GuardChases = 0, GuardReturns = 0; // an escort kept by its box going after an attacker, and coming back
		int Hurt = 0, Dry = 0;             // fighters that left the fight hit, or with their guns empty
		int FallsByFighters = 0, FallsUnderFire = 0; // aircraft a fighter group brought down; those with its guns on them as they fell (or within 25 s before)
		int FallsHeld = 0;                 // of those, held a few seconds for the guns to bear
		double WingSum = 0.0;              // metres from wingman to leader, summed over the steps a wingman flew his wing
		long WingSamples = 0, WingFar = 0; // those steps, and those of them over 500 m from his leader
		double LongestWait = 0.0;          // air seconds the longest any leader went between passes on a box
		double LowestFight = 1e9;          // metres over the ground or the sea, the lowest a fighter flew in a fight (not a strafing run)
	};

	/** The cloud deck as drawn over a board point (Show::DeckAt): its tile's foot and the height where it hides what
	 * falls into it, metres; Hide 0 where no tile of it is drawn. */
	struct DeckSpan
	{
		double Foot = 0.0, Hide = 0.0;
	};

	class Show
	{
	public:
		explicit Show(uint64_t Seed = 238);

		/** The board's target (board metres), the flak ring, and air seconds to a battle second (SKY.scale). */
		void SetBoard(double InTargetX, double InTargetY, double InFlakR, double InScale);
		void SetLoadout(const std::string& Nation, bool bFighter, const Loadout& Guns);
		/** The ground's height under a board point, metres (the sea 0). Left unset: the sea everywhere. */
		std::function<double(double, double)> Ground;
		/** The top of the cloud deck below the bombers as laid, metres (0: none). */
		double CloudTop = 0.0;
		/** The deck as drawn over a board point: the foot of the tile drawn over it and the height where it hides what
		 * falls into it (its top puff's middle), metres; Hide 0 where no tile of it is drawn (a hole, the ships'
		 * clearing). Something falling into a drawn tile ends 150 m under where it hides (or halfway to its foot, in
		 * a thin tile), so nothing pops from view; through a hole, or under the foot of the tile over it (begun under
		 * the deck, as a torpedo bomber or a low fight, or come down a hole), it goes on down to the sea or the
		 * ground, unless it falls into another tile on the way.
		 * Unset: the deck is everywhere, from 500 m under CloudTop to CloudTop. */
		std::function<DeckSpan(double, double)> DeckAt;
		/** How far from the eye the deck is drawn (metres; 0: everywhere): past it, as in a hole. */
		double DeckReach = 0.0;
		/** The deck's laid base (metres; 0: unknown): no tile's foot is lower (a tile is only ever lifted), so
		 * something under it is under the deck for good. */
		double DeckBase = 0.0;
		/** Where the camera is (board metres): the guns nearest it are the ones fired when there are too many. */
		Vec Eye;
		/** The most guns fired at once (the effects kit's budget: about 17 puffs a machine gun, 3,000 in all). */
		int MostGuns = 150;
		/** A bomber's bay doors open this many air seconds before its group's run is done (BOOK run: 45 s), and its
		 * stick's bombs leave the bay this far apart. */
		double BayLead = 10.0, StickGap = 0.15;
		/** Air seconds the doors take to open and to shut, eased at both ends (the art director, 06:48Z: motors or
		 * hydraulics, never a snap); and how high a bomber lifts as her load goes (metres, over about a second, trimmed
		 * out over the next six; the battle models' bombs_away clip). */
		double BayOpenS = 2.5, BayShutS = 2.5, StickLift = 0.4;
		/**
		 * The bomb run held steady (the coordinator on the Mac film of release 1's bays: the B-17 banked 50 to 60 degrees
		 * as her first bomb left, so most of the stick fell from a plane on her side; the art director's figures, 14:01Z).
		 * A level bomber flies wings level (within RunBankMost degrees) from her doors opening, holds her heading from her
		 * drop until StickSteady air seconds after her last bomb, and her doors start to shut as she then rolls into her
		 * box's rally turn. A bomber of two engines or more (or 20 m of span) banks no more than RallyBankMost degrees in
		 * her place in a box, rolling no faster than HeavyRollMost degrees a second, and no more than HeavyBankMost out of
		 * it (straggling, covering, going home); falling, she goes as she falls.
		 */
		double StickSteady = 1.5, RunBankMost = 2.0, RallyBankMost = 20.0, HeavyBankMost = 30.0, HeavyRollMost = 5.0;
		/** The target ring's radius (BOOK target.r): a land raid's view carries no run, so a box's run is timed from
		 * when it comes inside the ring (the rules' run is held there 45 air s). */
		double TargetR = 6000.0;
		/** How many bombs a bomber carries, by its model (a usual war load; unknown models by span). */
		static int BombsOf(const std::string& Model, double Span, bool bFighter);

		/** A view: its battle time, its groups, its events and its ships. Kept for the picture between views. */
		void Take(double T, const std::vector<Group>& InGroups, const std::vector<Event>& InEvents, const std::vector<Ship>& InShips);
		/** The battle time shown now (never past the latest view), and the air clock with it. */
		void Advance(double T);

		double Shown() const { return ShownT; }
		double AirNow() const { return AirT; }
		double Latest() const { return Views.empty() ? 0.0 : Views.back().T; }
		const std::vector<PlaneOut>& Planes() const { return Out; }
		const std::vector<Thing>& Things() const { return Small; }
		/** What has happened since it was last emptied: the caller plays it and clears it. */
		std::vector<Fx> Effects;
		/** A group's middle as shown now, at its height; false where there is no such group. */
		bool Middle(int GroupId, Vec& OutAt, double& OutFace) const;
		/** The groups as shown now (interpolated), for labels and the camera. */
		const std::map<int, Group>& Groups() const { return Now; }
		const Checks& Checked() const { return Check; }
		/** One line an aircraft: its role, its phase, its quarry (a debug overlay; the cloud test's trace). */
		std::string Describe() const;

	private:
		struct View
		{
			double T = 0.0;
			std::map<int, Group> Groups;
		};
		enum class Role { Ground, Slot, Fight, Straggle, Cover, Fall, Home, Dive, Gone };
		enum class Phase { None, Setup, Run, Break, Climb, Turn, Peel, Plunge, Pull, Egress, Hold, Zoom, Evade, Station, Chase, Away, Wing, Scissor };
		enum class FallHow { None, Explode, Burn, Spin, Flamer, Wing, Tail, Pilot, Glide, Breakup, Struck };

		/** What a pilot asks of his aircraft: a heading, a climb, a speed; or a bank of his own (a half-roll). */
		struct Cmd
		{
			double Hdg = 0.0, Gam = 0.0, Spd = 100.0;
			bool bBank = false;
			double Bank = 0.0;
		};

		struct Craft
		{
			int Key = 0, Group = 0, Side = 0;
			bool bFighter = true;
			std::string Model, Nation;
			Shape Form;
			Vec P;
			double Spd = 100.0, Hdg = 0.0, Gam = 0.0, Bank = 0.0;
			double VMin = 80.0, VMax = 153.0, VTop = 200.0, RMin = 210.0, GMax = 6.0, RollMax = 2.79;
			Role Doing = Role::Slot;
			Phase Step = Phase::None;
			double StepT = 0.0;      // air seconds into the phase
			int Slot = 0;
			int Target = -1;         // a plane's key (a fight) or a ship's id (a dive)
			int TargetGroup = -1;
			double Repick = 0.0;     // the air time after which it may pick its quarry again
			std::string Pass;
			bool bUnder = true;      // this break: under the box (three in four) or over it
			double Hand = 1.0;       // which side its passes come from
			double Burst = 0.0;      // air seconds of trigger left
			double Rest = 0.0;       // air seconds before the next burst may start
			bool bFiring = false;
			int StragId = -1;
			double StragAt = -1.0;   // the air time it fell out of its box
			FallHow Falling = FallHow::None;
			double FallT = 0.0;
			double FallDeck = 0.0;   // 1 while its fall may still pass into the deck (as Thing::Deck), else 0
			bool bDebrisToDeck = false; // its debris row was told the deck (it began over a drawn tile's foot)
			double DebrisDeck = 0.0; // the height its debris row was told, where its pieces leave sight
			int ChutesLeft = 0;
			double ChuteAt = 0.0;
			std::string ChuteCue;
			int TrailKey = 0;        // its running trail, 0 for none
			// A dead engine's trail (Brian 15:29Z: "if a engine is out there should be a lot more dark smoke strains
			// from that engine"): where on it the trail runs from, a second dead engine's own trail, what both lay now.
			Vec TrailAt;             // its own frame; engine 1's place unless a dead or burning engine's
			int TrailKey2 = 0;
			Vec TrailAt2;
			std::string TrailKind;   // fire, oil, streak, glycol or smoke, as last laid by LayTrail
			bool bFireOut = false;   // its dead engine's fire put out (the streak stays from then)
			bool bLiquid = false;    // liquid-cooled engines: white glycol first
			std::vector<int> DeadEngines;
			bool bFeathered = false;
			double SootAt = -1.0;    // the air time its dead engine's soot began to build
			// The way it falls, as the battle named it (AirShow.cpp, StartFall):
			std::string FallName;    // the event's how (struck and collide play as another way, kept here as named)
			int Look = 0;
			double Turn = 1.0;       // +1 or -1: the way it rolls, spins or banks away
			double FireAt = -1.0, BoomAt = -1.0, Dur = 60.0; // fall seconds: its fire shows, it blows apart, it strikes
			bool bDitch = false;
			bool bLit = false;       // its fire showing
			bool bHeavy = false;     // a bomber: slower rolls and spins, long curves
			Vec FallV;               // a tumbling or spinning fall's own velocity (its nose no longer its way)
			Vec Axis;                // a spin's middle line, where it is now
			double Spin = 0.0;       // a spin's or a tumble's angle (radians)
			double Ended = -1.0;     // fall seconds when it struck the sea or ground (a ditching floats a while)
			std::vector<double> ChuteTimes; // fall seconds when each parachute opens, as the battle said
			int ChuteNext = 0;
			std::vector<std::pair<double, std::string>> TrailPlan; // air times its trail changes kind ("" stops it)
			bool bShown = true;
			bool bJoining = false;   // a replacement still closing up to its slot
			// The fighter pilots (AirShow.cpp, the pilots' section):
			int Lead = 0;            // the leader he flies on; his own key: he leads his pair (or his vic). 0: not yet paired
			int Wing = 0;            // a leader's (first) wingman, 0: none
			int Boss = 0;            // the leader of a four's second pair: the four's leader, 0: none
			int Flight = 0;          // his four (or vic), as first formed
			int Seat = -1;           // in a four: 0 its leader, 1 his wingman, 2 the second pair's leader, 3 his wingman (-1: not yet paired)
			double Skill = 0.5;      // 0 to 1: how fast he sees a threat, how close he presses, how tight he holds a wing
			double Thrift = 0.1;     // the share of the group's ammunition left at which his own guns are empty
			int Threat = -1;         // the enemy fighter on his tail
			int Foe = -1;            // the one he is breaking away from
			double ThreatSeen = 0.0; // the air time he will have seen it and may act
			double Scan = 0.0;       // the air time he next looks behind him
			double ShotAt = -1e9;    // the last air time an enemy fighter's guns bore on him
			int ShotBy = -1;         // that fighter's group
			bool bHurt = false;      // hit and out of the fight
			bool bDry = false;       // a leader whose pair's guns are empty: out of the fight
			bool bGuard = false;     // an escort leader kept by his box
			bool bCover = false;     // his chase is after the enemy on his partner's tail
			bool bAnswer = false;    // his chase is after one of a group the battle has counted a loss on, yet to be shown
			bool bScored = false;    // this burst has brought one down already: one burst, one aircraft
			bool bRejoin = false;    // a wingman parted from his leader, coming back to him flat out
			int OwnMark = -1;        // a wingman going in on a box with his leader: the bomber beside his leader's that he fires on
			int OwnMarkBy = -1;      // (and the leader's bomber it was picked beside)
			bool bReversed = false;  // his chase is after the enemy that slid past him as he broke
			double ScissAxis = 0.0;  // in a scissors: the line of it (the attacker's heading as it began)
			double ScissUntil = 0.0; // the air time it is given up, nobody having won it
			double ScissNext = 0.0;  // the air time he may next be drawn into one
			double ScissTurn = 0.0;  // the way he is turning in it, +1 right of its line (he rolls back when his enemy has crossed him)
			int ScissLegs = 0;       // the times he has rolled back so far
			bool bScissWin = false;  // the one of the two who comes out behind the other
			bool bScissIn = false;   // the attacker, still coming through fast: the overshoot that begins it
			bool bStrafing = false;  // his pass is at an aircraft on the ground
			bool bFleeing = false;   // half-rolled and diving away from an enemy on his tail
			bool bOpened = false;    // has made his first pass of this fight
			bool bAnywhere = false;  // waiting to go in from where he is (the fight's opening), not from his perch
			bool bLow = false;       // far below the box after a dive: his next pass comes up at it from low astern
			bool bBlind = false;     // has not seen the enemy on his tail, and will not: a loss the battle has counted and the scene has yet to show
			int WingOf = 0;          // a wingman: the leader whose wing he is on now, and the air time he took it
			double WingAt = 0.0;
			double Since = 0.0;      // the air time his present chase began (an escort's leash)
			double PassAt = -1.0;    // the air time his last pass on a box began
			double YoYo = 0.0;       // air seconds left of a pull up and over, to stay behind a turning enemy
			double YoYoNext = 0.0;
			const char* Task = "";
			double Weave = 0.0;      // its own phase in an escort's weave
			bool bDropped = false;   // its bombs gone
			/** Its stick: bombs still in the bay to go, when the next leaves, and whether it is a torpedo. Its bay doors:
			 * 0 shut to 1 open, as they stand; when the last bomb of the stick left (-1: none yet). */
			int StickLeft = 0, StickSize = 0;
			double StickNext = 0.0;
			bool bStickTorpedo = false, bStickBay = false; // (bStickBay: a level bomber's stick, let go through her doors)
			double Bay = 0.0;        // (the doors' travel, even in time: the picture's opening is this eased)
			double StickDone = -1.0;
			double StickBegan = -1.0; // (when the first bomb of a bay's stick left: her lift)
			double RunHdg = 0.0;      // (her heading as she dropped: held, wings level, until her stick has gone)
			Vec LastP;
			bool bHasLast = false;
		};

		/** The beat of a fighter group's passes on one box, as the battle gives it (Fire::Go, Fire::Wait). */
		struct Wave
		{
			bool bKnown = false, bGo = true;
			double Left = 0.0;       // air seconds of the pass, or of the standing off, left
			double Ended = -1e9;     // the air time the last pass ended
			double Last = -1e9;      // the air time a four of the group last went in
		};

		struct Band
		{
			std::map<int, Wave> Waves;         // a fighter group: the beat of its passes on each box
			double FightAt = -1.0;             // a fighter group: the air time its fight began (-1: in none)
			double ShareAt = -1e9;             // a fighter group: the air time its pairs may next be shared out again
			int Pending = 0;                   // losses of the group held for the guns to bear (Held::bForShot)
			std::map<int, std::string> Pass;   // the pass its contact named against each group
			std::map<std::string, double> FlakNext;
			std::deque<double> Owed;           // air times of models fallen before the group's figures dropped
			int LastFigures = -1;
			bool bSeen = false;
			bool bDropped = false;
		};

		struct Held
		{
			Event E;
			double At = 0.0;
			/** A loss to fighters, held until the guns of the group that brought it down bear on one of its group (at
			 * most until At), so that what falls is what was shot at. */
			bool bForShot = false;
			/** Held a little longer once, a fighter of that group being about to fire. */
			bool bMore = false;
		};

		Dice Roll;
		double Scale = 2.0, TargetX = 50000.0, TargetY = 70000.0, FlakR = 12000.0;
		std::map<std::string, Loadout> Loadouts; // "<nation>:ftr" / "<nation>:bmr"
		std::deque<View> Views;
		std::deque<Event> Queue;             // events not yet shown
		std::vector<Held> Later;             // ship news held until the bombs reach the ships
		std::vector<Ship> ShipsNow;
		std::map<int, Group> Now;            // the groups as shown
		std::map<int, Band> Bands;
		std::map<int, Craft> Crafts;
		std::vector<Thing> Small;
		std::vector<PlaneOut> Out;
		std::set<int> Firing;
		/** Each bomber gunner's gun as he has it laid (its way in the world, and when), by his mount's key. */
		struct Lay
		{
			Vec Way;
			double T = -1.0;
		};
		std::map<int, Lay> Lays;
		double ShownT = 0.0, AirT = 0.0;
		double ViewT = 0.0;                  // the battle time of the view the groups shown now are read from
		bool bEnded = false;                 // the battle's end has been told: nothing is held back any longer
		/** The fighter pilots' picture, made once a frame (Brief): the fighters after each group, and after each
		 * aircraft; how many of each group are in the air, and how many it still has. */
		std::map<int, std::vector<int>> Hunters, Chasers;
		/** Losses the battle has counted and the scene has yet to show, by (the group that lost one, the group whose
		 * guns brought it down): the pilots of that group answer them (the nearest goes in now; see Brief). */
		std::map<std::pair<int, int>, int> Due;
		std::set<std::pair<int, int>> DueStrag; // (and the stragglers among them: the group, the straggler's id)
		std::map<int, int> Aloft, Alive;
		bool bStarted = false;
		int NextKey = 1;
		Checks Check;

		void Interpolate();
		void Play(const Event& E);
		void Release(const Event& E);
		void Reconcile(const Group& G, Band& B);
		Craft& Make(const Group& G, const Vec& At, double Hdg, double Spd, Role Doing);
		Vec SlotPlace(const Group& G, int Slot) const;
		Vec GroupVelocity(int GroupId) const;
		Cmd Steer(const Craft& C, const Vec& To, const Vec& ToVel, double Gain, double SpeedLow, double SpeedHigh) const;
		Cmd Toward(const Craft& C, const Vec& To, double Speed) const;
		double ClimbMost(const Craft& C) const;
		void Fly(Craft& C, const Cmd& K, double Dt, bool bCheck);
		void Pilot(Craft& C, double Dt);
		bool Fights(const Group& G, int Id) const;
		// The fighter pilots (AirShow.cpp, the pilots' section).
		/** Once a frame, before the pilots fly: pairs made up, the beat read, each pair given its enemy group, the
		 * escorts' guards and the pairs with empty guns named. */
		void Brief();
		/** A group's pairs (or vics) as they stand: who leads, who flies whose wing, which pair leads its four. */
		void PairUp(const Group& G);
		/** Each pair its enemy group, by how hard the group fires on each. */
		void ShareOut(const Group& G, Band& B);
		/** An escort's pairs kept by its box. */
		void SetGuards(const Group& G);
		/** The pairs whose guns are empty. */
		void SetSpent(const Group& G);
		void StandDown(Craft& C);
		/** A fighter in his group's fight, this step. False: he has nobody to fight here, and flies his place in the
		 * formation. */
		bool PilotFight(Craft& C, const Group& G, double Dt);
		void LookBehind(Craft& C);
		bool Evading(Craft& C, const Group& G, double Dt);
		/** In a scissors with the enemy who overshot him (or whom he overshot), this step. */
		bool Scissoring(Craft& C, const Group& G, double Dt);
		/** Breaking from an enemy or in a scissors with one: not to be taken off it, and no threat to anyone else. */
		static bool Dodging(const Craft& C) { return C.Step == Phase::Evade || C.Step == Phase::Scissor; }
		bool Covering(Craft& C, const Group& G, Craft* Mate, double Dt);
		bool Answering(Craft& C, const Group& G, double Dt);
		void FlyAway(Craft& C, const Group& G, Craft* Follow, double Dt);
		void FlyWing(Craft& C, Craft& L, const Group& G, double Dt);
		void WingOn(Craft& C, const Craft& L, double Back, double Wide, double Dt);
		bool FlyGuard(Craft& C, const Group& G, double Dt);
		void FlyPass(Craft& C, const Group& G, const Group& E, double Dt);
		bool FlyDuel(Craft& C, const Group& G, const Group& E, double Dt);
		void TailChase(Craft& C, Craft& T, const Group& G, double Dt);
		void BoomZoom(Craft& C, Craft& T, const Group& G, double Dt);
		void StartRun(Craft& C, const Group& G, const Group& E, const std::string& Pass);
		bool MayGo(const Craft& C, const Wave& W, const Craft* FourLead, double Eta) const;
		int MarkBomber(const Craft& C, const Group& E, const std::string& Pass);
		Craft* PickFoe(Craft& C, const Group& G, const Group& E, const Vec& Middle);
		std::string PassOn(const Group& G, int Box);
		Vec PerchOf(const Craft& C, const std::string& Pass, const Vec& Box, const Vec& Fb, const Vec& Rb) const;
		double EtaTo(const Craft& C, const Vec& To, const Vec& ToVel, double Range) const;
		/** His guns: a burst when his nose is on the lead inside Range, within Most of it. */
		void Squeeze(Craft& C, Craft& T, const Group& G, double Range, double Most, double Lo, double Hi, double Dt);
		/** Not into the ground: a command eased level, or up, by how low he is and how fast he is coming down. */
		void MindGround(const Craft& C, Cmd& K, double Least = 220.0) const;
		void Ease(const Craft& C, Cmd& K, double Most) const;
		void FightSpeed(const Craft& C);
		/** A loss to a fighter group: the key of the fighter of that group whose guns bear now on one of the lost
		 * aircraft's group (not one whose burst has brought one down already); 0 when none bears now but one of them was
		 * shot at by that group within the last few seconds; -1 when neither. */
		int UnderGuns(const Event& E) const;
		/** A fighter of that group is on his run at one of them and about to fire. */
		bool Inbound(const Event& E) const;
		/** That fighter's burst has brought one down: his next kill needs another burst. */
		void Scored(int Shooter, const Event& E);
		int DueOn(int Victim, int By) const;
		int RunsOn(int Box, int By) const;
		void PilotDive(Craft& C, const Group& G, double Dt);
		void PilotFall(Craft& C, double Dt);
		void PilotGround(Craft& C, const Group& G, double Dt);
		void FireGuns(double Dt);
		void FlakRound(double Dt);
		void SmallThings(double Dt);
		void StartFall(Craft& C, const Event& E);
		/** A falling aircraft blown apart: the flash, the pieces, the model gone (explode, or a burning one's boom). */
		void BlowApart(Craft& C, bool bFireball);
		/** A piece of a lost aircraft, falling or fluttering on its own, its trail if it has one. */
		void Piece(const Craft& C, int Slot, const Vec& At, const Vec& Vel, double Size, double Flutter, const char* Trail, double Life = 400.0);
		/** A hit survived (air:hit), and a straggler's fire put out (air:fireout). */
		void HitSurvived(const Event& E);
		/** Its own trail (and a second dead engine's) laid as Kind from now: TrailStart again on the same key, so the
		 * kit ramps the new kind into the same ribbon with no break in the smoke. */
		void LayTrail(Craft& C, const std::string& Kind);
		/** A dead engine's trail as its state has it: burning, put out (the thin dark streak that stays), or out and
		 * running oil smoke (white glycol first on a liquid-cooled type; a feathered one thinning to the streak). */
		void DeadEngineTrail(Craft& C, bool bFire);
		void StopTrails(Craft& C);
		/** Engine N's place (from 1, as the model's engine.N) in its own frame. */
		static Vec EngineOf(const Craft& C, int N);
		/** A bomber's gunner at Station (its own frame) laying his gun on Foe, Near metres off, this frame: his own
		 * lead, short of the full one, and his own slow wander about it, wider for a hand-held gun than a power
		 * turret; the gun follows that a little behind and swings no faster than a man or a turret lays one. Mount
		 * names the gunner (his aircraft's key * 64 + the mount's first gun), so a twin's guns lie together. Aim is
		 * where the gun points; true when he is on it (within 15 degrees) and can fire. */
		bool LayGun(const Craft& C, const Vec& Station, const Craft& Foe, double Near, const Gun& Row, int Mount, Vec& Aim);
		/** The model, its shape and its limits, from its group (as made, or when a view first names its model). */
		void Fit(Craft& C, const Group& G);
		int PickToFall(const Event& E);
		void Drop(Craft& C, bool bTorpedo);
		/** The bays and the sticks this frame: doors toward open or shut, and each bomb due out of its bay let go. */
		double RushAt = -1e9; // air seconds: the last falling bomb's rush given
		std::map<int, double> RingT; // air seconds each raiding bomber group has held inside the target ring
		void Bays(double Dt);
		void Leave(Craft& C);
		Vec Local(const Craft& C, const Vec& L) const;
		Vec Velocity(const Craft& C) const;
		double GroundAt(double X, double Y) const;
		Fx& Emit(FxKind Kind, const Vec& At, const std::string& What = std::string(), int Key = 0);
		/** Whether something coming out at a point may pass into the deck (1) or is under it for good, under the
		 * deck's laid base (0; none: 0). */
		double DeckFor(const Vec& At) const;
		/** The deck as drawn over a board point (Hide 0: none drawn there, or past its reach from the eye). */
		DeckSpan DeckHere(double X, double Y) const;
		/** For something that may pass into the deck (Deck 1): true once it is inside a drawn tile, 150 m under where
		 * it hides (halfway to its foot in a thin tile). In a hole or under the foot of the tile over it, it goes on
		 * down, still asked each time (a lower tile beside it may take it); under the deck's laid base, for good
		 * (Deck to 0). */
		bool IntoDeck(double& Deck, const Vec& At) const;
		Craft* Find(int Key);
		const Craft* Find(int Key) const;
		bool Live(const Craft& C) const;
		std::vector<int> LiveOf(int GroupId) const;
	};
}
