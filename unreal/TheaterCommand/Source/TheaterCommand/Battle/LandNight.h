#pragma once

#include "Battle/LandFeed.h"
#include "Battle/LandPlay.h"
#include "Battle/LandEye.h"

#include <functional>
#include <string>
#include <vector>

/**
 * Release 2's night before the attack (Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it
 * off"; the tactics thread's patrol, behind the player's patrol switch). A recording that carries the patrol opens on the
 * night before, as a war film would open on it: a few shots of the patrol's men (LandFeed::AddPatrolMen's) going out in
 * file under the moon, lying close to the post they found, flat under the enemy's flares; then the picture dips to black
 * and the light comes up over the battle's own opening. No words and no marks on the field while the night plays.
 *
 * Plain C++ (no engine): the scene asks it each frame for the battle time to show, the eye, how dark the light is and
 * the flares; the play test (Tools/land_play, LAND_PLAY_PATROL) plays it through and checks the men are in the shots.
 */
namespace LandNight
{
	using LandPlay::Vec;

	/** A flare over the field: where it hangs (field metres; Z as the ground's heights), how bright (0 out, 1 full) and
	 * its number among the patrol's (the scene's lamp for it). Rising: still climbing from the gun, a dull spark. */
	struct Flare
	{
		Vec At;
		double Light = 0.0;
		int Id = 0;
		bool bRising = false;
	};

	enum class Beat : unsigned char { Out, Near, Flare, Dawn };
	const char* NameOf(Beat B);

	/** One shot of the night: its kind, the battle time it opens on and how long it runs (real seconds). */
	struct Shot
	{
		Beat Kind = Beat::Out;
		double From = 0.0, Real = 0.0;
	};

	struct Settings
	{
		/** Real seconds each night shot runs; the dip to black between the night and the battle (half down, half up);
		 * the light's rise from the night's to the day's over the battle's opening. */
		double OutFor = 7.0, NearFor = 7.0, FlareFor = 9.0, DipFor = 1.0, DawnFor = 4.5;
		/** Battle seconds a real second while the night plays (the battle's own pace near contact: speed 1 at life 2). */
		double Rate = 0.5;
		/** A flare: how high it bursts over the ground, how fast it falls under its parachute (metres a battle second),
		 * how long it climbs first (battle seconds), how far it swings (metres) and how long a swing takes (battle
		 * seconds). It burns LandFeed::PatrolFlareBurns. */
		double FlareHigh = 160.0, FlareFall = 4.0, FlareRise = 1.0, FlareSwing = 1.5, FlareSwingFor = 2.2;
	};

	/** What the scene shows this frame. */
	struct Frame
	{
		/** The night's own shots play: its clock and its eye (false: the battle's own, though the light may still be
		 * rising over it). */
		bool bNight = false;
		double Battle = 0.0;
		LandEye::Pose Pose;
		/** 1 the night's light, 0 the day's; the picture's dip to black (0 none, 1 black). */
		double Dark = 0.0, Black = 0.0;
		std::vector<Flare> Flares;
		Beat Kind = Beat::Dawn;
	};

	class Prelude
	{
	public:
		/**
		 * Plans the night for a recording with a patrol whose men AddPatrolMen added (false, and nothing to play, where it
		 * has none). Height: the ground's height at a field point. Free: whether the eye can stand at a point (unset:
		 * anywhere). Start: the battle time the battle opens on.
		 */
		bool Begin(const LandFeed::Recording& Rec, std::function<double(double X, double Y)> Height, std::function<bool(const Vec& At)> Free,
			double Start, const Settings& InSettings = Settings());
		/** The frame after RealDt more real seconds. */
		Frame Update(double RealDt);
		/** Straight to the dip and the dawn (a key or a click: the player has seen it). */
		void Skip();
		/** Its night shots are still on (the battle's clock waits). */
		bool Playing() const { return bBegun && Real < NightEnds + 0.5 * Set.DipFor; }
		/** The light has risen: nothing more to do. */
		bool Done() const { return !bBegun || Real >= NightEnds + 0.5 * Set.DipFor + Set.DawnFor; }
		const std::vector<Shot>& Shots() const { return Plan; }
		/** The patrol's men (track indexes), the leader first. */
		const std::vector<int>& Men() const { return Patrol; }

	private:
		const LandFeed::Recording* Rec = nullptr;
		std::function<double(double, double)> HeightAt;
		std::function<bool(const Vec&)> FreeAt;
		Settings Set;
		std::vector<Shot> Plan;
		std::vector<int> Patrol;
		double Start = 0.0, Real = 0.0, NightEnds = 0.0;
		int ShotNow = -1;
		bool bBegun = false;
		Vec EyeAt, LookAt;
		double NearFov = 38.0;
		bool bEyeSet = false;

		Vec MenMiddle(double T, Vec* OutLead = nullptr, Vec* OutLast = nullptr) const;
		Vec Stand(const Vec& Want, double Over) const;
		LandEye::Pose PoseFor(const Shot& S, double T, double RealDt, bool bFirst);
	};

	/** Each flare's place and light at battle time T. */
	void FlaresAt(const LandFeed::Patrol& P, double T, const std::function<double(double, double)>& Height, const Settings& S, std::vector<Flare>& Out);

	/**
	 * A stand-in patrol for a recording without one (the scene's -TheaterLandPatrol, the play test's LAND_PLAY_PATROL),
	 * as the tactics thread's rules would send it: from the attacker's most forward foot block toward the defender's
	 * nearest, 4 men, out the night before (-2400 to -600 s), halting 120 m short of the post (found: its place given), and
	 * home again; Caught: a flare over them as they near it, and the defence alerted. The patrol root's value as JSON
	 * ("" where either side has no foot block).
	 */
	std::string StandInPatrol(const LandFeed::Recording& Rec, bool bFound, bool bCaught);

	/** The stand-in patrol put into a recording's text that has none (its men too: AddPatrolMen), and the recording read
	 * again. The men added (0: it had one already, or none could be made). */
	int AddStandInPatrol(std::string& Text, LandFeed::Recording& Rec, bool bFound, bool bCaught, std::string& OutError);
}
