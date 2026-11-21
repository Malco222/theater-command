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
		/** Its stick has begun to leave the bay: the load the model racks there is gone (a bomber shot down before it
		 * dropped takes hers with it). */
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
		enum class Phase { None, Setup, Run, Break, Climb, Turn, Peel, Plunge, Pull, Egress };
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
			double Weave = 0.0;      // its own phase in an escort's weave
			bool bDropped = false;   // its bombs gone
			/** Its stick: bombs still in the bay to go, when the next leaves, and whether it is a torpedo. Its bay doors:
			 * 0 shut to 1 open, as they stand; when the last bomb of the stick left (-1: none yet). */
			int StickLeft = 0, StickSize = 0;
			double StickNext = 0.0;
			bool bStickTorpedo = false, bStickBay = false; // (bStickBay: a level bomber's stick, let go through her doors)
			double Bay = 0.0;
			double StickDone = -1.0;
			Vec LastP;
			bool bHasLast = false;
		};

		struct Band
		{
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
		int PickQuarry(const Group& G);
		void PilotFight(Craft& C, const Group& G, double Dt);
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
