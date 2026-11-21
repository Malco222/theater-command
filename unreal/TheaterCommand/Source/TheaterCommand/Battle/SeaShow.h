#pragma once

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
 *    columns take stations abreast, and a leader keeps clear of other ships by the rule of the road. A ship sunk or out
 *    of the fight sheers out of her line.
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
		std::map<std::string, ClassSize> Classes; // "bb", "cv", "ca", "dd"
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
		std::string Flight;     // carriers: "deck", "out", "back"
		double Cover = -1.0;    // carriers: 0 to 1, the share of its fighters held over the fleet still up
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
		bool bLimp = false;   // sea:burning: her course home (radians) and speed (m/s)
		double LimpCourse = 0.0, LimpSpeed = 0.0;
		bool bShare = false;  // sea:torpedo: how full the bank was, 0 to 1 (Share): that share of the tracks, none at 0
		std::vector<int> Ships; // sea:flak: the gun ships firing
		bool bDitch = false;  // sea:recover: the carrier has gone down, her aircraft come down in the sea
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
		double Speed = 0.0;  // metres a second through the water
		double Wake = 0.0;   // 0 to 1: her bow wave and wake, by her speed against her flank speed
		double Funnel = 0.0; // 0 to 1: how hard her funnels smoke (black at flank, thin at rest)
		double Long = 180.0, Beam = 20.0;
		Fate Doing = Fate::Line;
		std::string How;     // sinking: settle, sink, capsize, explode, burn, break
		double Down = 0.0;   // sinking: 0 as she starts to go, 1 under
		double DownFor = 0.0; // sinking: seconds from the first to the last
		/** Her main turrets: their bearing off her bow (degrees, clockwise) and elevation (degrees); the ship they are
		 * laid on (its Key, -1 none) and whether a salvo went this frame. */
		double Train = 0.0, Elevation = 0.0;
		int Aim = -1;
		bool bFiring = false;
		bool bBurning = false;
		bool bScreen = false; // laying smoke
		bool bShown = true;
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
		Sound,       // a one-shot: What (the sound director's name), At
		Dispatch,    // an event's words, when it is shown: What (the event), Text, Side, Key (its piece)
	};

	struct Fx
	{
		FxKind Kind = FxKind::Sound;
		int Key = 0;
		Vec At, Aim, Vel;
		std::string What, Text;
		double Size = 0.0, Until = 0.0, Since = 0.0;
		int Side = -1;
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
		double MostLead = 0.0;      // metres between a piece's leader and where he is steering for (the battle's place, the zig-zag and his station)
		double MostKeep = 0.0;      // metres a leader's station lay off the battle's place to keep clear of other ships
		double MostJump = 0.0;      // metres a ship moved in a frame beyond her speed (a pop)
		double MostGap = 0.0;       // metres a follower lay off her place in the line (once closed up)
		double MostHeel = 0.0;      // degrees
		double MostZigAfter = 0.0;  // metres of zig-zag still in the course a minute after contact
		int Salvos = 0, Splashes = 0, ShellHits = 0, TorpedoHits = 0, BombHits = 0, NearMisses = 0;
		int Sinks = 0, Leaves = 0, TracksRun = 0, Launches = 0, Landings = 0, PlanesDown = 0, FlakBursts = 0, Screens = 0;
		int SunkEvents = 0, BurningEvents = 0, TorpedoEvents = 0, TorpedoHitEvents = 0, StrikeEvents = 0, StrikeLandEvents = 0, AirEvents = 0, RecoverEvents = 0;
		int CoverUp = 0;            // fighters of the cover seen over the carriers at the most
		int SunkUnmatched = 0, BurningUnmatched = 0, TorpedoHitUnmatched = 0;
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
		const std::vector<TorpOut>& Torpedoes() const { return OutTorps; }
		/** What has happened since it was last emptied: the caller plays it and clears it. */
		std::vector<Fx> Effects;
		ColumnRoom Columns;

		/** The wind (metres a second, the way it blows toward) and the sea's state on the Beaufort scale. */
		Vec Wind() const { return WindV; }
		double SeaState() const { return Beaufort; }
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
			double Heel = 0.0, Pitch = 0.0;
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
		};

		struct Owed
		{
			double Hits = 0.0;
			std::string Weapon = "shell_medium";
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
		std::set<int> ZonesLit;
		std::map<int, int> ZoneShip;       // by zone id: the ship it was laid for (sunk, burning)
		std::vector<ShipOut> OutShips;
		std::vector<PlaneOut> OutPlanes;
		std::vector<TorpOut> OutTorps;
		/** The ships' clock (stepped by Fixed) and the moment drawn, a little after it. */
		double ShownT = 0.0, DrawT = 0.0, ContactT = -1.0;
		static constexpr double Fixed = 0.05;
		bool bStarted = false;
		bool bBigBlast = false;
		int NextKey = 1;
		Vec WindV;
		double Beaufort = 3.0;
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
		/** Each leader's station (bSettle: at the start, each column laid at it at once). */
		void KeepClear(double Dt, bool bSettle = false);
		void Debts(const View& Before, const View& After);
		void Output();

		/** The ship of a piece nearest a board point, afloat in its line (null: none). */
		Hull* NearestOf(int PieceId, double X, double Y);
		Hull* FindHull(int Key);
		/** A hull afloat (not Except's) that a point of the sea lies within Margin metres of. */
		bool OnHull(const Vec& At, double Margin, int Except) const;
		/** How fast a ship of the class may turn at a speed (radians a second): her class's rate at her cruising speed,
		 * in proportion to her speed about it (her turning circle the same at any speed). */
		double MostRateAt(const ClassSize& Sz, double Spd) const;
		const Hull* FindHull(int Key) const;
		/** A ship of a piece by the battle's place along its line (a zone laid for a ship lost: sim.ts smoke). */
		Hull* ByZone(int PieceId, const Zone* Z);
		void TakeOut(Hull& H);
		void Sink(Hull& H, const std::string& Words);
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
