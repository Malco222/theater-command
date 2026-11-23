#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Fates
{
	struct Way;
}

/**
 * A knocked-out vehicle's crew getting out (release 2, behind -TheaterLandBailOut; Brian, 23 November 2026 14:53Z: "if a
 * tank gets hit people try and get out before it explosdes"). Plain C++, so a cloud test plays it
 * (Tools/land_bail/run.sh).
 *
 * Before it, nobody left a knocked-out tank: her commander, if he was up, died in his hatch, and the rest of her crew
 * were never seen. The fates table already says who gets out of each kind of end (its crew_out steps: how many, when,
 * running or crawling, to her rear, to cover or to a crater; crew_in for a crew that bails out and climbs back in), and
 * that a brew-up's ammunition goes 5 to 10 seconds after the hit; the scene played none of it. Now each man climbs out
 * of his hatch (the commander's, the driver's, the loader's beside the commander's, the hull gunner's beside the
 * driver's), steps to the hull's edge on the side away from the enemy, drops to the ground and goes: running bent
 * double for her rear, crawling to a crater, crouching in her lee. Some do not make it: a man still climbing out when
 * her ammunition goes is thrown down dead beside her; a man in the open may be cut down, or hit and left crawling; and
 * now and then the first man out of a burning tank is on fire himself, runs a few steps and falls, burning on.
 *
 * The art director's notes of 23 November: tankers get out (the scene draws them in their tankers' files, no
 * rifle; an army with no tankers' file, for now, in its infantry figure, no rifle); a man first runs or crawls straight back in her lee, 8 to
 * 12 m (a crawler 3 to 5), keeping her hull between him and the enemy, then breaks for his cover; her ammunition
 * going throws a man on her deck 2 to 5 m, and anyone on the ground within 15 m of her flinches flat for a second or
 * two, then goes on; his odds in the open are by the enemy who can see him (Wreck::Threat: 1 in 6 running with the
 * enemy close, much less with none in sight); a man on fire comes out of at most about one brew-up in eight, his
 * flames in patches with smoke trailing from him, and about one in three of them rolls it out and lives, lying
 * wounded and smoking.
 *
 * The rules decide who lives and dies in the battle's numbers; none of this changes them (the fates table's own rule:
 * looks only). Its dice are its own, by the wreck's seed, so the same battle bails out the same way each time.
 */
namespace LandBail
{
	struct V3
	{
		double X = 0.0, Y = 0.0, Z = 0.0; // field metres: x east, y north, z up
	};

	/** How it goes for him. */
	enum class End : uint8_t
	{
		Safe,     // he gets where he was going and goes to ground there
		Shot,     // cut down in the open: he falls dead
		Wounded,  // hit in the open: he falls and crawls on a few metres, then lies writhing
		Burning,  // out of a burning tank on fire: he runs a few steps, falls, and burns on where he fell
		Caught,   // still climbing out when her ammunition went: thrown down dead beside her
	};

	struct Man
	{
		int Exit = 0;             // which of the wreck's hatches (Wreck::Exits)
		double Out = 0.0;         // life's seconds after her kill he starts up out of it
		std::string Go = "run";   // run, crawl, crouch
		std::string To = "rear";  // rear, cover, crater, beside
		End How = End::Safe;
		double FallAt = -1.0;     // life's seconds after the kill he is hit or thrown, -1 never
		double FireOut = -1.0;    // a burning man: life's seconds after the kill his flames go out
		double BackAt = -1.0;     // life's seconds after the kill he goes back to climb in again (crew_in), -1 never
		int Side = 1;             // the side of her hull he drops from: +1 her right, -1 her left
		V3 Hatch, Deck, Down, Goal; // his hatch; the hull's edge he jumps from; where he lands; where he makes for
		V3 Lee;                   // the end of his first leg, straight back in her lee (Down for a man staying beside her)
		double Thrown = 0.0;      // caught by her blast: how far it throws him, metres
		double FlinchAt = -1.0;   // life's seconds after the kill he flinches flat at her blast, -1 never
		double FlinchS = 0.0;     // and for how long, before he goes on
		bool bLives = false;      // on fire: he rolls it out and lives, lying wounded and smoking
		double Speed = 4.5;       // metres a second on the ground
		double Crawl = 0.0;       // a wounded man's crawl after he falls, metres
		int Fall = 0;             // which fall: 0 forward, 1 back
		/** The soldiers' bail clips (Wreck::bClips): out of the hatch in bail_climb_out, off her hull in bail_jump_down,
		 * a man on fire in burning_run and burning_fall_roll; his way out the clips' length (OnGroundOf). */
		bool bClips = false;
		double Face = 0.0;        // with the clips: his facing as he climbs out (his right toward the side he drops from)
	};

	/** Life's seconds from his start out of the hatch to his feet on the ground (2.05 s; 3.53 s with the bail clips). */
	double OnGroundOf(const Man& M);

	struct Wreck
	{
		std::string Fate;                // the fates table's key
		const Fates::Way* Way = nullptr; // its row (dials and beats); none: nobody gets out
		std::vector<V3> Exits;           // her hatches' field points as she lies (the commander's first); none: nobody
		V3 Middle;                       // the ground under her middle
		double Heading = 0.0;            // radians, 0 north, clockwise
		double Wide = 3.0;               // her width, metres
		double Long = 6.0;               // her length, metres
		double Away = 0.0;               // radians, 0 north, clockwise: the way away from her enemy
		double Blast = -1.0;             // life's seconds after her kill her ammunition goes (-1 never)
		bool bBurns = false;             // she burns (a man may come out on fire)
		bool bUnderFire = true;          // the fight goes on round her (men in the open may be hit)
		/** The soldiers' bail clips are in her crew's file (bail_climb_out, bail_jump_down, burning_run,
		 * burning_fall_roll; labtrr 42bb946f): played in place of the infantry's stand_up, run and falls. False: as
		 * before, the same crew to the last number. */
		bool bClips = false;
		/** 0 to 1: the enemy who can see her men in the open, 1 close and in sight (the scene's nearest living enemy),
		 * toward 0 with none in sight. A runner's odds of being hit are 1 in 6 times it; a crawler's half that. */
		double Threat = 1.0;
		int MostMen = 5;                 // her crew
		uint64_t Seed = 1;
		std::function<double(double, double)> Ground; // the ground's height at a field point (none: Middle's)
	};

	struct Crew
	{
		std::vector<Man> Men;
		std::string Story; // for the log
	};

	/** Her crew's getting out. The same Wreck gives the same crew. */
	Crew Make(const Wreck& W);

	/** His pose at a time: the clip by its name in a soldier file, how far into it (life's seconds), where his root is
	 * and which way he faces. Not shown while he is inside her. */
	struct Pose
	{
		bool bShown = false;
		std::string Clip;
		double Time = 0.0;
		V3 At;
		double Facing = 0.0; // radians, 0 north, clockwise
		bool bOnFire = false;
		bool bMoving = false;
		/** Where his body is: At moved as the clip's own root moves (bail_climb_out, bail_jump_down, burning_fall_roll),
		 * else At. For what goes with him: a burning man's flames, the camera. */
		V3 Root;
	};
	Pose PoseAt(const Man& M, double Since, const std::function<double(double, double)>& Ground);

	/** The scene's book of the battle's bail-outs: each wreck's crew, made at her kill, and the men drawn for it. */
	struct Book
	{
		/** -TheaterLandBailOut (release 2, off by default until the art director and a 45 fps reading pass). */
		bool bOn = false;
		struct Him
		{
			int Vehicle = -1;
			size_t K = 0;           // his place in her crew
			int Man = -1;           // the crowd's man
			std::string File;       // his soldier file (USA_tanker, GER_panzer, the army's own)
			std::string Clip, WasClip;
			double Switched = -1.0, WasTime = 0.0, NowTime = 0.0;
			bool bLit = false, bOut = false;
		};
		std::vector<Crew> Crews;    // by vehicle (empty: nobody, or not made yet)
		std::vector<uint8_t> Made;  // by vehicle
		std::vector<Him> Men;
		/** The kit's number for a burning man's flames: above the film fire's (1 << 26), below the kit's columns'. */
		static int FireIdOf(size_t Him) { return (1 << 27) + static_cast<int>(Him % 1000000); }
	};
}
