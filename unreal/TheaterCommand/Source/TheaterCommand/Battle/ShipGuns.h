#pragma once

#include "Effects/TheaterFxCore.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

/**
 * A ship's anti-aircraft guns as her navy had them that month (Brian, 21 November 18:06Z: "for the ships being attacked
 * by planes, they can fire machine gun fire and flak guns at them just like real life"). Plain C++: the air scene's
 * flight core fires them (AirShow, Show::ShipFire), the cloud test reads them, and the sea battle scene may fire the
 * same guns at the strikes it shows.
 *
 * Two families, as the navies fought them. The heavy guns (the American 5-inch, the Japanese 12.7 cm, the British
 * 4-inch, the German 10.5 cm and their like) fire time-fuzed shells laid by a director: a black burst where the plane
 * was going to be, mostly too far behind or off to the side; against a torpedo plane down on the water they fire into
 * the sea ahead of it for the splashes. The light guns (the .50 calibre, the 20 mm, the Japanese 25 mm, the 1.1-inch,
 * the 40 mm Bofors and the pom-pom) are laid by eye along their tracers: streams that hose about the plane, most rounds
 * passing behind it, now and then walking across it.
 *
 * Counts are mounts as fitted at the time, from the class's usual fit (a single ship's own fit where the historian has
 * it later). The battle decides everything that counts; these guns only show the fire that the battle's flak losses
 * and misses came from (the 21 November gunfire rule: draw the volume, land only the counted kills).
 *
 * Shared with the sea battle scene: a scene hands Gunnery its ships (Hull) and the planes in the air (Plane) each
 * frame, and plays the Shots that come back with its effects kit and sound (the air scene: AirShow::Show::ShipFire
 * turns them into its Fx, and AAirBattle plays those). Gunnery picks which plane each gun is on, unless the scene holds
 * a ship's or a plane's fire (bHold: the flak hold while that side's own fighters are going in on it, or the scene's
 * own choice). Board metres (X east, Y north, Z up, the sea at 0), headings radians clockwise from north.
 */
namespace ShipGuns
{
	using Vec = TheaterFx::Vec;
	using Dice = TheaterFx::Dice;

	/** A light gun's stream's key (an aircraft's guns are its key * 64 + the gun, under these): each ship her own 64,
	 * a mount's streams 0 to 59. Owner: the number the key's / 64 gives for her, as an aircraft's key gives its own. */
	constexpr int StreamBase = 1 << 27;
	inline int StreamKey(int ShipId, int Stream) { return StreamBase + ShipId * 64 + Stream; }
	inline int OwnerOf(int ShipId) { return (StreamBase >> 6) + ShipId; }
	inline bool IsStream(int Key) { return Key >= StreamBase && Key < (1 << 29); }
	/** One key for each ship's light guns of one kind (0 hmg, 1 aa25, 2 aa40), for the sound: heard as one battery. */
	inline int HeardKey(int Key, int KindSlot) { return (Key & ~63) | (60 + (KindSlot & 3)); }
	int KindSlot(const std::string& Kind);

	/** One kind of gun. */
	struct Piece
	{
		const char* Name = "";  // as the historian names it: "5-inch/25", "25 mm Type 96 twin"
		bool bHeavy = false;    // a high-angle gun firing shells that burst (flak); else a light gun firing a stream
		const char* Kind = "";  // a light gun's row in the effects kit's air_guns: hmg, aa25, aa40
		int Barrels = 1;        // on one mount
		double Rate = 0.0;      // rounds a minute from each barrel (a heavy gun's as a crew keeps it up)
		double Mv = 800.0;      // muzzle velocity, metres a second
		double Reach = 1000.0;  // the slant range it is fired at, metres (its useful range, not its longest)
		double Ceiling = 0.0;   // a heavy gun's: the highest it bursts a shell, metres
		double MostUp = 85.0;   // its highest elevation, degrees
		int Load = 0;           // a light gun's rounds in a drum, box or clip before its crew must change it (0: fed as it fires)
		double Change = 0.0;    // seconds to change it
		double Mm = 0.0;        // calibre, millimetres
		/** A low-angle gun (a destroyer's or a battleship's main battery): fired only at a torpedo plane low over the
		 * sea, inside its Reach, its shells thrown into the water ahead of it for the splashes; never at a plane overhead. */
		bool bLow = false;
	};

	struct Mount
	{
		const Piece* Gun = nullptr;
		int Count = 0;          // mounts of it aboard
	};

	struct Battery
	{
		std::vector<Mount> Mounts;
		/** All the light guns' mounts, and all the heavy guns' barrels. */
		int LightMounts() const;
		int HeavyBarrels() const;
		/** The farthest any of its light guns, or its heavy guns, reach (0: none). */
		double LightReach() const;
		double HeavyReach() const;
	};

	/**
	 * Her guns: Nation as a recording names it (USA, JAP, UK, GER, ITA, SOV, FRA; any other a small navy's usual fit),
	 * Class as its ships are classed (bb, cv, ca, dd, ss, ag), the battle's year and month (the guns change with the
	 * war: the .50 calibre giving way to the 20 mm through 1942, the 40 mm from late 1942, the Japanese 25 mm doubled
	 * and doubled again from mid 1943), and her name where the historian has her own fit (the Midway carriers). The
	 * historian's stages and reaches: /mnt/project-files/naval-battles/ship-air-defence.md, parts A5, B1 and B2.
	 * Commonwealth navies fire Britain's guns.
	 */
	Battery BatteryOf(const std::string& Nation, const std::string& Class, int Year, int Month, const std::string& Name = std::string());

	/** How high over the sea her anti-aircraft guns stand, metres (her decks, galleries and platforms). */
	double DeckOf(const std::string& Class);

	/** One line: "8 x 5-inch/25, 8 x .50 cal M2" (for the cloud test and the doc). */
	std::string Describe(const Battery& B);

	/** A ship as the scene has her this frame. */
	struct Hull
	{
		int Id = 0;
		Vec At;                 // amidships, at the waterline
		double Face = 0.0;      // her heading
		Vec Vel;                // metres a second (her rounds leave with it)
		std::string Nation, Class, Name;
		int Side = 1;           // whose: the planes of the other side are her enemies
		double Long = 180.0, Beam = 20.0;
		double Manned = 1.0;    // the share of her guns manned (Pearl's crews reach theirs as the attack goes on)
		bool bSunk = false;     // sunk or past fighting: her guns silent
		bool bHold = false;     // the scene holds her fire
		/** Her side holds Proximity shells (the rules' proxfuze research): her heavy bursts hug the plane instead of
		 * sitting in a height band (the historian's B2). Absent until the rules give it. */
		bool bProxFuze = false;
	};

	/** A plane in the air as the scene has it this frame. */
	struct Plane
	{
		int Key = 0;
		Vec P, V;
		double Hdg = 0.0, Gam = 0.0, Bank = 0.0;
		double Span = 12.0, Long = 10.0;
		int Side = 0;
		bool bFighter = false;
		bool bFighting = false; // a fighter on a pass or in a fight (its own side's guns hold on what it is after)
		bool bFalling = false;  // going down: no gun is laid on it, unless a ship's guns are seen bringing it down (Kill)
		bool bHold = false;     // the scene holds every ship's fire on it
	};

	/** What the guns do, for the scene to play. */
	enum class ShotKind
	{
		Stream,     // a light gun firing this frame (call the kit's AirGun every frame it comes): Key, At (muzzle), Aim
		            //   (where it is laid), Vel (the ship's), Kit (the effects kit's air_guns row), What (the gun), Rate
		            //   (rounds a minute), Mv (metres a second), Ship
		StreamStop, // its trigger let go: Key
		Round,      // a heavy gun fired: At (its muzzle), Aim, What (the gun), Size (its calibre, mm), Ship; its shell's
		            //   Burst or Splash follows when it gets there
		Burst,      // a heavy shell bursting in the air (the kit's AirFlak, "heavy"): At, Size (calibre), Ship
		Splash,     // a shell into the sea ahead of a low torpedo plane: At, Size (calibre), Ship
		Hit,        // rounds striking a plane (the kit's AirHit): Key (the plane), At, Normal, Vel (the plane's), Kit; What
		            //   "kill" for the rounds that bring down one the battle says the ships' guns got (as its fall begins)
	};

	struct Shot
	{
		ShotKind Kind = ShotKind::Stream;
		int Key = 0, Ship = 0;
		Vec At, Aim, Vel, Normal;
		std::string What, Kit;
		double Rate = 0.0, Mv = 0.0, Size = 0.0;
	};

	/** The guns' own count, as they fire (the cloud test reads it). */
	struct Tally
	{
		long StreamFrames = 0;    // a light gun's stream fired for a frame
		long StreamOn = 0;        // of those, frames it passed within half a span of the plane it was on
		int Hits = 0;             // rounds seen striking a plane
		int Rounds = 0, Bursts = 0, Splashes = 0; // heavy rounds fired; their bursts; shells into the sea ahead of a low plane
		int Kills = 0, KillsHeavy = 0; // planes the battle's flak brought down shown under a ship's light guns; under her heavy guns
		int OutOfReach = 0;       // a gun laid on a plane past its reach, a friend, or one held (by construction 0)
		int Held = 0;             // frames a gun held on a plane its own side's fighters were after
		int MostStreams = 0;
		std::set<std::string> Fired;              // each gun that fired, by name
		std::map<std::string, std::string> Kits;  // each ship's guns as drawn (her name or id: Describe)
	};

	/** The ships' gun crews and their directors, from frame to frame. */
	class Gunnery
	{
	public:
		explicit Gunnery(uint64_t Seed = 239);
		/** The battle's month (each ship's guns: BatteryOf). */
		int Year = 1942, Month = 6;
		/** The light guns' streams drawn at once (nearest the eye first: the effects kit's budget), and the heavy rounds
		 * shown in a second of the guns' clock (each burst about a dozen of the kit's puffs, hanging 9 s), the nearest
		 * ships' first. */
		int MostStreams = 64;
		double RoundsMost = 4.0;
		/** Where the eye is. */
		Vec Eye;

		/** This frame (Now and Dt on the guns' clock: the air clock in the air scene): each crew on its plane, firing in
		 * its bursts, laid with its own error; the heavy guns' rounds; shells and rounds arriving. Dt 0: every stream
		 * stopped. */
		void Fire(double Now, double Dt, const std::vector<Hull>& Ships, const std::vector<Plane>& Planes, std::vector<Shot>& Out);
		/**
		 * A plane the battle says the ships' guns brought down, as it starts to fall. Under the nearest ship's light or
		 * medium guns (inside their reach), their streams go on it and a few rounds are seen striking home: true. Past
		 * them, inside her heavy guns' reach, a burst is laid on it now (At): true. Neither: false (the scene shows it
		 * its own way).
		 */
		bool Kill(double Now, const Plane& P, const std::vector<Hull>& Ships, std::vector<Shot>& Out);
		/** Every stream stopped (a scene paused, or ending). */
		void StopAll(std::vector<Shot>& Out);
		const Tally& Tallied() const { return Count; }
		const Battery& BatteryFor(const Hull& H);

	private:
		struct Crew
		{
			int Target = -1;
			double Pick = 0.0;
			double Burst = 0.0, Rest = 0.0, Change = 0.0, Left = 0.0;
			double Phase = 0.0, W1 = 0.5, W2 = 0.6, Lead = 0.75;
			bool bMade = false;
		};
		struct Director
		{
			int Target = -1;
			double Pick = 0.0, Owed = 0.0, Lead = 0.8;
			int Next = 0;
		};
		struct Shell
		{
			double At = 0.0;
			Shot S;
		};
		struct Lock
		{
			int Plane = -1;
			double Until = 0.0;
		};
		Dice Roll;
		Tally Count;
		std::map<int, Battery> Batteries;   // by ship id
		std::map<int, Crew> Crews;          // by stream key (a mount's first)
		std::map<int, Director> Directors;  // by ship id
		std::map<int, Lock> Locks;          // a ship's light guns on a plane the battle's flak brought down
		std::vector<Shell> Shells;          // shells, and rounds striking, on their way
		std::set<int> Firing;
		double Budget = 0.0;                // heavy rounds that may be shown now (RoundsMost a second, banked up to it)
	};
}
