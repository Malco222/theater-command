#pragma once

#include "Battle/LandFeed.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * Ground-attack fighters over the land battle, drawn (Effects, release 2, behind -TheaterLandAirSupport; Brian's ask
 * through the coordinator, 23 November: the fighters on call "fired on" from the ground, "rarely shot down", and they
 * "crash"). Plain C++, no engine: the game draws what it says (ALandBattle, its Effects (R2) hook), and the cloud plays
 * it through Tools/land_air.
 *
 * The rules decide everything (src/battle/land/sim.ts, air support; LandFeed::AirNote): which side calls a strike and
 * when, each leg's run down a strip of the field (land:air-in, then land:strafe: from, to, its width, how long in battle
 * seconds), its bombs (land:bomb-drop; each burst a shell of calibre bomb, which the scene draws as ever), and the
 * planes it cost (air:lost: explode, burn or spin), as the run ends. Nothing here changes any of it. What is decided
 * here is only what the rules leave open: how a leg's planes fly their run, which of them is the one lost, where the
 * ground's guns stand and where their streams go, and how a plane falls and where it strikes.
 *
 * A leg's planes come in one behind another down the line of its run, high, push over into a dive aimed at the middle
 * of their lane, their guns opening at the machine's range from the lane's start and walking it to its end (the art
 * director's notes of 23 November, 19:08Z: a fighter from 600 to 1000 m in a 20 to 30 degree dive, guns from about
 * 900 m to 250 m, out at the bottom 30 to 60 m up; a Stuka from high in a 75 degree dive, its two guns a short burst,
 * out at about 300 m; an Il-2 low and shallow, out at 15 m), pull out, climb away and break left and right in turn,
 * each 6 to 12 s behind the one before, so that each dive is its own beat. A lane starts 150 m past its own side's
 * front at the least. They fly at a real machine's speed along their way (105 to 140 m/s), not the rules' picture's (120
 * board metres a battle second): the rules' run is seconds of the battle, its planes' pass seconds of life. The ground's
 * guns are the blocks of the other side nearest the run, two guns a block, in ragged bursts, their streams aimed short
 * of the lead, so that the tracers fall behind. A plane lost is the last of its leg not yet lost, hit in its pass or as
 * it climbs away; it blows up (its pieces and its fuel come down burning), or goes down on fire in a long shallow dive,
 * or spins in (about 3 s from 150 m), and burns where it struck.
 *
 * Places are board metres (x east, y north, z up), headings degrees clockwise from north, as the feed's. Times: battle
 * seconds outside; a plane flies on the real seconds since its leg came over (the battle's times Life, the scene's own).
 */
namespace LandAir
{
	struct V3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};

	using GroundFn = std::function<double(double X, double Y)>;

	/** An air force's machine for the ground attack in a year, as the miniature kit names it, and how it flies it. */
	struct Machine
	{
		std::string Model;              // the kit's model ("Mustang", "Stuka"; "Fighter" a plain one in the nation's paint)
		double Span = 11.0;             // metres
		double Speed = 130.0;           // metres a second over the ground in the attack
		double High = 800.0, Low = 45.0; // its height before the dive and at the bottom of its pull-out, metres
		double Dive = 25.0;             // the dive's angle, degrees
		double Climb = 25.0;            // the climb away, degrees
		double Open = 900.0, Shut = 250.0; // the slant ranges its guns open at (to its lane's start) and shut at (to its end)
		double PushG = 1.5, PullG = 4.0; // into the dive and out of it: the push-over's and the pull-out's g
		std::string Gun = "hmg";        // its guns, as the kit's air_guns rows name them
		double Rate = 20.0;             // rounds a real second drawn from each wing
	};
	Machine MachineOf(const std::string& Nat, int Year);

	enum class Fall : uint8_t { None, Explode, Burn, Spin };

	/** One plane as it is drawn now. */
	struct Plane
	{
		int Key = 0;                    // its leg's number times 8 and its place in the leg: its guns, trail and fall go by it
		int Leg = -1, Slot = 0, Side = -1;
		std::string Nat, Model, Gun;
		double Span = 11.0;
		V3 At, Vel;                     // board metres; metres a real second
		double Heading = 0.0, Pitch = 0.0, Bank = 0.0; // degrees: clockwise from north, nose up, right wing down
		bool bFiring = false;
		V3 Muzzle[2], Aim;              // its wings' guns, and the ground they rake
		Fall How = Fall::None;          // going down, how; None while it flies
		bool bWreck = false;            // down, burning where it struck (drawn nose in, the rest of the battle)
	};

	/** A gun on the ground firing up at a strike now: where it stands and where its stream goes. */
	struct Gunner
	{
		int Key = 0;
		V3 From, To;
	};

	/** What happens to a plane between two frames: hit (its fall begins there), or into the ground. */
	struct Happen
	{
		enum class What : uint8_t { Hit, Crash } Kind = What::Hit;
		int Key = 0;
		double T = 0.0;                 // battle seconds
		Fall How = Fall::None;
		V3 At, Vel;
		double Span = 11.0;
	};

	/** The kit's numbers for a plane's guns (Shots), a ground gun's, and a plane's trail, fall and fire (one each). */
	inline int GunIdOf(int Key, int Wing) { return 90000 + (Key % 2400) * 2 + (Wing & 1); }
	inline int GunnerIdOf(int Key) { return 95000 + Key % 4800; }
	inline int FireIdOf(int Key) { return (1 << 25) + Key % 1000000; }

	class Sky
	{
	public:
		/** Plans every leg the notes name: its planes' ways, the ground's guns, its losses' falls. Life: real seconds a
		 * battle second (the scene's). The guns stand where the other side's blocks stand at the leg's coming (the
		 * recording's views). */
		void Build(const LandFeed::Recording& Rec, const GroundFn& Ground, double Life);
		/** The notes the plan was made from (a live battle's grow: Build again when more come). */
		size_t Notes() const { return NotesN; }
		size_t Legs() const { return Plans.size(); }
		/** Every plane drawn at battle time T (none before its leg comes or after it has gone, but its wreck), and the
		 * ground's guns firing then. */
		void At(double T, std::vector<Plane>& OutPlanes, std::vector<Gunner>& OutGuns) const;
		/** What happened in (From, To], in time's order. */
		void Between(double From, double To, std::vector<Happen>& Out) const;
		/** Rounds a gun firing Rate a real second fires in (From, To] (by the battle's clock: the same however the
		 * frames fall). */
		int Rounds(double From, double To, double Rate) const;

	private:
		struct Sample
		{
			double T = 0.0;             // real seconds since its leg came
			V3 At;
			double Heading = 0.0, Pitch = 0.0, Bank = 0.0;
		};
		struct Down
		{
			Fall How = Fall::None;
			double Hit = 0.0, Crash = 0.0; // real seconds since its leg came
			std::vector<Sample> Path;   // hit to crash (none for an explosion: its pieces fall in the kit)
			V3 HitAt, HitVel, CrashAt, CrashVel;
		};
		struct Burst
		{
			double On = 0.0, Off = 0.0; // real seconds since its leg came
			double Across = 0.0, Up = 0.0, Lead = 0.5;
		};
		struct Post
		{
			int Key = 0;
			V3 At;
			std::vector<Burst> Bursts;
		};
		struct Plan
		{
			int Flight = -1, Side = -1, Models = 4, Year = 1944;
			std::string Nat;
			Machine M;
			double Tin = 0.0;           // battle seconds: over the start of its run
			double Run[4] = { 0.0, 0.0, 0.0, 0.0 };
			double Len = 1.0, Fx = 0.0, Fy = 1.0, Head = 0.0; // the run's length, way (unit) and heading (degrees)
			double Width = 60.0, Base = 0.0;
			double Gap = 6.0, Pass = 4.0; // real seconds between planes, and each plane's pass (guns open to guns shut)
			// Its way in (metres along the run from its start, heights over Base): level at High from Ua, the push-over
			// from Uq into the dive, the dive down its line (aimed at its lane's middle), the pull-out to the bottom at Uc.
			double Ua = 0.0, Uq = 0.0, Uc = 0.0;
			double Us = 0.0;            // where its guns' lane starts (at least 150 m past its own side's front)
			double Gam = 0.4, Rq = 1000.0, Rp = 600.0; // the dive's angle (radians), the push-over's and the pull-out's radii
			double Seg[4] = { 0.0, 0.0, 0.0, 0.0 }; // the level, the push-over, the dive and the pull-out, metres of way
			double Sf0 = 0.0, Bottom = 0.0; // metres along its way in where its guns open; seconds from then to the bottom
			std::vector<Down> Falls;    // per slot (How None: not lost)
			std::vector<Post> Posts;
		};

		/** Its way in, S metres along it: where it is along the run (U), its height over Base (H) and its pitch (radians,
		 * nose up). Before its start it is level; past the bottom, at the bottom. */
		static void WayIn(const Plan& P, double S, double& U, double& H, double& Pitch);
		/** A plane of a leg in flight at Tau (real seconds since its leg came): false where it is not drawn. */
		bool Fly(const Plan& P, int Slot, double Tau, Sample& Out) const;
		void Flown(const Plan& P, int Slot, double Tau, Plane& Out) const;
		void PlanFall(Plan& P, int Slot, Fall How, double Tau);

		std::vector<Plan> Plans;
		GroundFn GroundAt;
		double Life = 2.0;
		size_t NotesN = 0;
	};
}
