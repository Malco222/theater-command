#pragma once

#include "Battle/LandFeed.h"

#include <functional>
#include <string>
#include <vector>

/**
 * The ground battle's replay of its fire: every round the rules fired, once, as the effects kit draws it and the sound
 * hears it (the bridge's "The men": rounds { n, t, l, by, k, j, w, on, ok, oj, x, y, r, fc, u, h }, j and oj the man of
 * the section who fired and who was struck), and the battle's own blasts (the 'shell' events). Plain C++: the cloud
 * test plays exactly what the game plays.
 *
 * The rule it keeps (the sound director, approved 21 November; the realism rule): every shot fired is one flash and one
 * sound, from the shooter's own muzzle, and lands where the rules say with what the rules say. Most miss. Nothing is
 * made up: no shot is added for show, none is left out. A 'shell' event and its kill round are ONE blast.
 *
 * Nothing here draws or plays: it says what is due in a span of battle time, and the scene hands it on.
 */
namespace LandPlay
{
	struct Vec
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		Vec() = default;
		Vec(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
		Vec operator+(const Vec& O) const { return Vec(X + O.X, Y + O.Y, Z + O.Z); }
		Vec operator-(const Vec& O) const { return Vec(X - O.X, Y - O.Y, Z - O.Z); }
		Vec operator*(double S) const { return Vec(X * S, Y * S, Z * S); }
		double Dot(const Vec& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		double Length() const;
		Vec Unit() const;
	};

	/** Effects (R2), small arms off armour: a plate a round struck on a vehicle, as the scene has her posed now (World::
	 * Plate; field metres, x east, y north, z up). At the strike and Normal its outward way; RoomMid, RoomA and RoomB how
	 * far the plate runs flat about it (two ways along it, square to each other, each as long as it runs from RoomMid
	 * that way and as far the other way; RoomA level where the plate stands up); Floor her own height under it where a
	 * spark thrown down would land on her (her deck under her turret), under -1e29 none. */
	struct PlateSpot
	{
		Vec At, Normal, RoomMid, RoomA, RoomB;
		double Floor = -1.0e30;
	};

	/** What the scene knows of its world, asked by the replay without engine types. Each may be left empty: the
	 * replay then uses the rules' own field and plain figures (the cloud test without the other cores). */
	struct World
	{
		/** The ground's height at a field point, metres. */
		std::function<double(double X, double Y)> Height;
		/** The effects kit's word for the ground at a point: earth, sand, snow, mud, town, woods, water, rock. */
		std::function<std::string(double X, double Y)> GroundWord;
		/** A shooter's muzzle (a man's from his skinned rifle, a tank's or a gun's from its model's socket) at a battle
		 * time: where and which way, for track index Track. Socket names a vehicle's other gun (TankMg: hull_mg, coax,
		 * roof_mg), empty for its main gun. False: not known (the replay places it plainly). */
		std::function<bool(int Track, double T, const std::string& Socket, Vec& OutAt, Vec& OutFacing)> Muzzle;
		/** Where round Round struck a vehicle's skin (the rules' face: front, left, right, rear, top; U across, -1..1;
		 * H up, 0..1) at battle time T, and the skin's outward way there. False: not known. */
		std::function<bool(int Track, int Round, double T, const std::string& Face, double U, double H, Vec& OutAt, Vec& OutNormal)> Skin;
		/** Whether a tank (track index Track) carries a roof machine gun as drawn (her model's roof_mg swap this month,
		 * and for her nation: no .50 on a British Sherman, no MG 34 on an early Tiger's roof). False: TankMg never gives
		 * her roof gun, so her commander never fires one she has not got. Empty: every American and German tank has one. */
		std::function<bool(int Track)> RoofGun;
		/** Effects (R2), small arms off armour (Brian, 25 November: "bullets should be bouncing off it"; off unless
		 * -TheaterLandRicochet (release 2, on trial)): a rifle's or a machine gun's round that strikes an armoured vehicle
		 * (ArmouredVehicle: a tank, an assault gun, a self-propelled gun, an armoured car, a half-track), whatever the rules
		 * made of it, is its burst's Shots with bArmour (the plate's point and its outward Normal: the kit's ricochets,
		 * never dirt) and a Glance noise carrying the burst's rounds (a rattle of steel), and no Glance effect of its own.
		 * Off (as left): as before. */
		bool bRicochet = false;
		/** bRicochet: life seconds a battle second (the game's -TheaterLandLife, 0.5 to 4): the kit lays a burst's
		 * strikes on life's clock, so below 1 her path through it is worked out further on in battle time. */
		double Life = 1.0;
		/** Effects (R2), small arms off armour: the plate round Round struck on vehicle Track, as the scene has her posed
		 * now (the tank core's cast on her model, nearer wheels and tracks included, and how far it runs flat about the
		 * strike: LandKit::Vehicle::PlateNow). False: not known (her Skin, every round at its point; else her plain box). */
		std::function<bool(int Track, int Round, PlateSpot& Out)> Plate;
		/** Effects (R2), -TheaterLandGunFlash (Brian, 23 November: "can we make the mussel flash realistic?"): the look of
		 * the shooter's own gun (track Track; Weapon the round's: rifle, mg, tank, at, gun; Army and Role his), the
		 * kit's look named after the weapon's row as "mg:star"; for a man whose one round is a burst (a squad leader's
		 * sub-machine gun) its rounds and their rate a life second, else left at 0 for the round's own. Empty, or this
		 * unset: the kit's row by the weapon alone, as before. */
		std::function<std::string(int Track, const std::string& Weapon, const std::string& Army, const std::string& Role, int& OutRounds, double& OutPerSecond)> GunLook;
	};

	/** Whether a vehicle's plate turns a rifle or machine-gun round (World::bRicochet): her own model where the
	 * recording names one (Track::Model), else her block's (Block::Model, may be null). */
	bool ArmouredVehicle(const LandFeed::Track& Tr, const LandFeed::Block* B);
	/** Effects (R2), small arms off armour: her plain box (a tank about 6 m long, 3.2 wide and 2.6 high; a half-track or
	 * an armoured car 5.8, 2.2, 2.2, by her model or her role) standing at S on ground Ground: the face the rules name, the
	 * point U across it and H up it, its outward way, and the whole face as its room. The replay's plate where the scene
	 * knows no better, and land_play's check of it. */
	PlateSpot PlainPlate(const LandFeed::Track& Tr, const LandFeed::Block* B, const LandFeed::Spot& S, double Ground, const std::string& Face, double U, double H);

	/** One thing for the effects kit (BattleEffects), at battle time T. */
	enum class FxKind : unsigned char
	{
		Muzzle,     // Name: rifle, mg, tank_gun, at_gun, howitzer, mortar; At the muzzle, Way its facing
		Shots,      // small arms: Rounds from At toward To at PerSecond (the kit lays tracers and the dust spurts; bArmour: ricochets off the plate at To, Normal its outward way)
		Shell,      // a shell by the field's calibre (Name: light, mortar, field, howitzer, heavy) at At on Ground
		Size,       // a burst by the kit's size row (Name: grenade, gun75, how105...) at At on Ground
		Skip,       // an armour-piercing round that missed: a low spurt and a skip along Way
		Glance,     // a shot glancing off armour at At: Way its incoming way, Normal the plate's, CalibreMm
		Plate,      // a round on armour that went in or killed: the kit's on_armour sparks (Size with bArmour)
		Screen      // a smoke round landing: its screen (Name: wp, a phosphorus round's burst; canister, a burning bomb's), CalibreMm the bomb's
	};

	struct Effect
	{
		double T = 0.0;
		FxKind Kind = FxKind::Muzzle;
		Vec At, Way, To, Normal;
		std::string Name, Ground;
		bool bArmour = false;
		/** For Shots: the gun's own number (the kit keeps each gun's tracer count), rounds and their rate. */
		int Gun = 0;
		int Rounds = 1;
		double PerSecond = 1.0;
		double CalibreMm = 75.0;
		/** The round's number in the recording (0 for a blast with no round). */
		int Round = 0;
		/** For Screen: how long the rules hold its screen, battle seconds (the round's Hang); -1 the calibre's own. */
		double Hang = -1.0;
		/** For Shots with bArmour (Effects (R2), small arms off armour): her plate at the battle time played to (To and
		 * Normal), how far it runs flat (PlateSpot's RoomMid, RoomA, RoomB) and her floor under it (under -1e29 none), as
		 * she is posed then; her path through the burst (Path: her middle on the ground and how far her heading has
		 * turned, + clockwise seen from above, at moments from then (Dt 0, her middle Pivot) to past its last strike), so
		 * the kit lays each strike where her plate is when it strikes, as she slows, stops or turns (PathMoved); and her
		 * steady velocity (Carry, field metres a battle second) and turn (Turn, radians a battle second) through it. */
		struct PathPoint
		{
			double Dt = 0.0;
			Vec At;
			double Turn = 0.0;
		};
		Vec RoomMid, RoomA, RoomB, Carry, Pivot;
		double Floor = -1.0e30, Turn = 0.0;
		std::vector<PathPoint> Path;
	};
	/** Effects (R2), small arms off armour: where a point of her plate as handed over (an Effect's To, RoomMid) is Dt
	 * battle seconds later, carried along her Path (straight between its moments, held before the first and after the
	 * last), and how far she has turned by then; the kit (ShotsOnArmour) carries it so. No path: her Carry and Turn. */
	Vec PathMoved(const Effect& E, const Vec& P, double Dt, double* OutTurn = nullptr);

	/** One thing for the sound (LandSound), at battle time T. */
	enum class NoiseKind : unsigned char
	{
		Shot,       // Weapon: rifle, mg, at, tank, gun, mortar; Army the shooter's; Rounds a burst's; From/To its line
		Landing,    // Size: grenade, mortar, field_gun, howitzer_105, howitzer_150, tank_he; or ap (an AP miss)
		Glance,     // a round off armour (Weapon the round's; Rounds and PerSecond a small arm's burst, World::bRicochet)
		Holed,      // a round that went into a vehicle short of a kill
		Fall,       // a man down
		KnockedOut, // a tank or gun knocked out
		Contact,    // a block comes into the fight: the whistle and an order (Army the block's)
		Broken      // an army breaks: Side the broken one
	};

	struct Noise
	{
		double T = 0.0;
		NoiseKind Kind = NoiseKind::Shot;
		Vec At;
		/** A shot's line, for the crack of rounds passing near the ear. */
		Vec From, To;
		std::string Weapon, Size, Army;
		/** A shot's shooter's role (rifle, officer, leader, mg ...), empty when unknown. */
		std::string Role;
		/** A tank's machine-gun burst: which of her guns fired it ("coax", "hull_mg" or "roof_mg", as TankMg), for its
		 * sound (the Sherman's roof gun is a .50, her others .30s); empty for any other shot. */
		std::string Mount;
		int Side = -1;
		int Rounds = 1;
		double PerSecond = 1.0;
		/** The shooter's track (or the fallen man's, the knocked-out vehicle's), -1 none. */
		int Track = -1;
		bool bArmour = false;
		/** A shot's round missed (it strikes the ground at To), and the battle time it lands (-1 unknown): for the
		 * sound of a round striking near the ear and of a shell heard coming in. */
		bool bMiss = false;
		double LandsAt = -1.0;
	};

	/** A man falling or a vehicle knocked out, for the scene's other cores and the camera. */
	struct Loss
	{
		double T = 0.0;
		int Track = -1;
		Vec At;
		bool bVehicle = false;
		/** The round that did it (its weapon), "" when the recording names none. */
		std::string By;
	};

	/** Muzzle rates and burst lengths: an MG 42 (1,200 a minute, bursts of 7 to 10), the Americans' M1919 and BAR
	 * (about 8 a second, bursts of 5 to 6). */
	struct Burst
	{
		int Rounds = 1;
		double PerSecond = 1.0;
	};
	Burst BurstOf(const std::string& Weapon, const std::string& Army, const std::string& Role);

	/** The kit's ground word for a rules ground kind (open, woods, buildings...). */
	std::string GroundWordOf(const std::string& Ground);

	class Replay
	{
	public:
		bool Begin(const LandFeed::Recording& InRecording, const World& InWorld);
		/**
		 * A live battle (release 2): indexes what one delta added to the recording (LandFeed::AppendDelta's Grown) as
		 * Begin indexes a whole one: the new rounds' shooters and targets, their landings in order, the 'shell' events
		 * paired with their kill rounds (a round and its event may come in different deltas), the new falls. The rules
		 * run ahead of the screen, so all of it is later than what has been played. A reset begins again.
		 */
		void Extend(const LandFeed::Grown& G);
		/** Everything due in (From, To] of battle time, in order: the effects, the sounds' sources, the losses. A step
		 * backwards (From > To) hands nothing: a replay only goes forward. */
		void Step(double From, double To, std::vector<Effect>& OutFx, std::vector<Noise>& OutNoise, std::vector<Loss>& OutLoss);
		/** Back to the start (a replay played again). */
		void Rewind();

		// For the test and the log.
		int RoundsPlayed() const { return Played; }
		int BlastsPaired() const { return Paired; }
		int BlastsAlone() const { return Alone; }
		/** Kill rounds of a gun, tank or mortar (or a grenade) on a follower that no 'shell' event draws: each its own
		 * burst. */
		int FollowerBurstCount() const { return FollowerBursts; }
		const LandFeed::Recording* Source() const { return Rec; }

	private:
		const LandFeed::Recording* Rec = nullptr;
		World Here;
		/** Each round's shooter track (-1 none), and whether a 'shell' event draws it (then the round only fires). */
		std::vector<int> Shooter;
		std::vector<int> TargetTrack;
		std::vector<int> EventOf;
		/** For each 'shell' event, the round it is paired with (-1: a blast alone). */
		std::vector<int> RoundOfEvent;
		/** Each track's fall listed (a live battle's falls come in as they happen). */
		std::vector<uint8_t> FallListed;
		/** The battle time played to (Step's To), -1 none yet. */
		double PlayedTo = -1.0;
		/** Pairs 'shell' events [From, To) with the kill rounds they draw (Begin's rule: the nearest unpaired kill round
		 * landing within 0.35 s and 1.5 m, a key man's first), the rounds looked for among those landing near each. */
		void PairBlasts(size_t From, size_t To);
		/** Where the replay has got to in the time-ordered lists: rounds fired, rounds landed (by landing time), events,
		 * falls. */
		size_t NextFire = 0, NextLand = 0, NextEvent = 0, NextFall = 0;
		std::vector<int> ByLanding;
		struct FallRow { double T; int Track; bool bVehicle; };
		std::vector<FallRow> Falls;
		int Played = 0, Paired = 0, Alone = 0, FollowerBursts = 0;

		double HeightAt(double X, double Y) const;
		std::string GroundAt(double X, double Y) const;
		Vec MuzzleOf(int Track, double T, const std::string& Weapon, Vec& OutFacing, const Vec& Toward, int RoundN = -1, const std::string& Gun = std::string()) const;
	public:
		/**
		 * A tank's machine gun for one of her bursts: the rules' own where the round names it (Gun: coax, hull, roof;
		 * a roof round from a tank the scene says carries none, World::RoofGun, comes from her coaxial).
		 * Where it does not (a recording from before they did), the scene picks: the commander's on her roof when he is
		 * up in his hatch (not buttoned) and his army mounted one there (the Americans' .50, the Germans' MG 34 on the
		 * cupola's ring), a burst in three; else the hull gunner's ball-mounted gun where the foot are within 25 degrees
		 * of her bow, half its bursts; else the coaxial beside her main gun. The same burst picks the same gun (by its
		 * round's number). Its socket's name: roof_mg, hull_mg or coax.
		 */
		std::string TankMg(int Track, double T, int RoundN, const Vec& Toward, const std::string& Gun = std::string()) const;
	private:
		/** Where a round lands: on the ground at x, y; on a vehicle's skin where the rules name the face. */
		Vec LandingOf(const LandFeed::Round& R, int Target, Vec& OutNormal) const;
		/** World::bRicochet: a rifle's or machine gun's round, not a miss, on an armoured vehicle. */
		bool Ricochets(const LandFeed::Round& R, int Target) const;
		/** Where such a round's burst strikes her plate, for its Shots (To, Normal, the room, her floor and her velocity
		 * through the burst; its Rounds and PerSecond already set): the scene's cast (World::Plate) where it is known, her
		 * skin (World::Skin) at its point, else her plain box's face that the rules name, never her middle. */
		void PlateOf(const LandFeed::Round& R, int Target, Effect& Sh) const;
		void Fire(const LandFeed::Round& R, int Index, std::vector<Effect>& OutFx, std::vector<Noise>& OutNoise);
		void Land(const LandFeed::Round& R, int Index, std::vector<Effect>& OutFx, std::vector<Noise>& OutNoise);
		void Blast(const LandFeed::Event& E, int Round, std::vector<Effect>& OutFx, std::vector<Noise>& OutNoise);
	};
}
