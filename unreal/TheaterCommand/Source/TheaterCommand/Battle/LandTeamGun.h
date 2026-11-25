#pragma once

#include "Battle/LandKit.h"
#include "Battle/LandMen.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * The support teams' weapons on the field (release 2, the trial word "teamguns"; Brian, 25 November 2026: "Where are
 * the machine gun teams and mortar teams", "There should be machine gun teams that lay down suppressing fire, such as
 * the German MG 42 and the US machine gun").
 *
 * The rules already field them (a team's block, kind art, team 'mg' or 'mor': three key men of role hmg or mortar,
 * each key man's place his weapon's place and his facing its facing; the drill's poses: `deployed` and `fire` set up,
 * `wait` set down a moment, `walk`, `run` and `flee` carried by hand, `cower` a routed crew). Until now each was drawn
 * as a rifleman. Here is the weapon itself: which file an army's team serves (FileFor), what that file tells the game
 * (Kind), and where the weapon stands from frame to frame (Gun): set up at its gunner, its gun swung and laid on what
 * he fires at, shaking as he fires; on his shoulder when the team moves (a Maxim rolled behind him on her wheels); and
 * left where it stood when he falls.
 *
 * Plain C++: no engine. The weapon's own axes are a soldier's (x forward, y right, z up), so a frame of hers is a
 * LandMen::Xform as a man's WorldFrame is, and the scene places it with the same code (LandParts::BankedAt).
 * Looks only: nothing here is read by the rules.
 */
namespace LandTeamGun
{
	/** The file (Content/Models/Battle/<name>.json) of the weapon a team's key man serves, by his army (the block's
	 * Army: "GER", "UK@desert", "SOV_winter" ...), his role (hmg, mortar) and the battle's month. Empty for any other
	 * role. An army with no weapon of its own drawn yet takes the nearest shape (FileFor's table says which). */
	std::string FileFor(const std::string& Army, const std::string& Role, int Year, int Month);

	/** A machine gun's burst as her team fires it from her mount: longer than a section's gun's, for she is fed by a belt
	 * or a strip and held by her tripod (the MG 42's 18 rounds in nine tenths of a second, where a section's fires 8;
	 * no longer, for the rules fire her again within the second). Rounds and rounds a second, by her file. False for a
	 * mortar or any other file. */
	bool BurstOf(const std::string& File, int& OutRounds, double& OutPerSecond);

	/** What a weapon's model tells the game. */
	struct Kind
	{
		/** Her file's name (FileFor's; whoever loads her sets it) and what her model calls itself ("MG 42 on the Lafette, set
		 * low"). */
		std::string File, Title;
		/** A mortar (its `tube` the part carried), else a machine gun (its `cradle` and `gun`). */
		bool bMortar = false;
		/** On wheels with a trail to pull her by (the Maxim's `pull` socket): rolled behind her gunner, never lifted. */
		bool bWheeled = false;
		/** Her gunner kneels behind her until his own clips come (a gun whose grips are over 0.52 m up, or on wheels
		 * behind a shield); else he lies. A mortar's gunner kneels. */
		bool bKneels = false;
		/** The side her belt or strip comes in from: -1 the left, +1 the right (0 a mortar). */
		int FeedSide = 0;
		/** Her model's bodies: what stands on the ground, and what the game turns and a man carries. */
		std::vector<int> Mount, Turned;
		/** Model frame (x forward, y right, z up; metres): what Turned swings and tips about; the key man's own place
		 * on the ground; the muzzle at rest and the way a round leaves it; the middle of what is carried; the trail's
		 * handle (bWheeled). */
		LandKit::V3 Pivot, Seat, Muzzle, Bore = LandKit::V3(1.0, 0.0, 0.0), Balance, Pull;
		/** Degrees: how far she swings to the left and to the right, and is laid down and up. */
		double Left = 25.0, Right = 25.0, Low = -8.0, High = 12.0;

		/** Reads them from her model as loaded. False (and why) where the file is not a support weapon's. */
		bool Read(const LandKit::Model& M, std::string& OutWhy);
	};

	/** What a weapon's key man is doing this frame. */
	struct Crew
	{
		/** Field metres; his facing and the way he goes, radians clockwise from north (the track's own). */
		double X = 0.0, Y = 0.0, Facing = 0.0, Way = 0.0;
		std::string Pose;
		bool bAim = false;
		double AimX = 0.0, AimY = 0.0;
		/** He has fallen. */
		bool bDown = false;
		/** He is on the field and drawn (not yet come, gone, or hidden in ambush: false). */
		bool bShown = true;
		/** His own frame on the field this frame (the crowd's WorldFrame), null where it has none: what a carried
		 * weapon rides on. */
		const LandMen::Xform* Frame = nullptr;
	};

	/** One weapon on the field, from frame to frame. */
	struct Gun
	{
		enum class State : uint8_t
		{
			Away,    // not drawn
			SetUp,   // on the ground at her gunner, served
			Carried, // what is carried on his shoulder; her mount put away
			Rolled,  // a wheeled one pulled behind him by her trail
			Left     // where she stood when he fell
		};
		State Is = State::Away;
		/** Her mount's origin on the field and its facing (radians clockwise from north). */
		double X = 0.0, Y = 0.0, Z = 0.0, Facing = 0.0;
		/** Degrees to the right, degrees up, metres back along the bore. */
		double Traverse = 0.0, Elevation = 0.0, Recoil = 0.0;
		/** She fires this frame (set up, her gunner in `fire` with an aim; or, told of her bursts, within one). */
		bool bFiring = false;
		/** Where in her burst-and-pause she is, 0 to 1 (the scene gives each weapon its own, so a team's three do not
		 * shake in step). */
		double Phase = 0.0;
		/** The scene tells her of each burst (Fired) and she shakes through it and no longer; false, she shakes by her
		 * own clock while her gunner's pose is `fire` with an aim (Phase). */
		bool bByRounds = false;
		double FiredAt = -1e9, FiredFor = 0.0, FiredRate = 12.0;
		/** A burst of hers begins at battle time T: Seconds long, PerSecond rounds a second (one told again, or an
		 * earlier one told late, changes nothing). */
		void Fired(double T, double Seconds, double PerSecond);
		/** Carried: where what he carries is this frame. */
		LandMen::Xform Shoulder;

		/** A frame on: Now the battle's seconds, Dt the frame's, Height the drawn ground under a place. */
		void Step(const Kind& K, const Crew& C, double Now, double Dt, const std::function<double(double, double)>& Height);

		bool MountShown() const { return Is == State::SetUp || Is == State::Rolled || Is == State::Left; }
		bool TurnedShown() const { return Is != State::Away; }
		/** Her mount's frame on the field, and that of what turns (field = R * own + T, as a man's WorldFrame). */
		LandMen::Xform MountFrame() const;
		LandMen::Xform TurnedFrame(const Kind& K) const;
		/** Her muzzle on the field and the way a round leaves it, while she stands set up. */
		bool MuzzleAt(const Kind& K, LandMen::Vec3& OutAt, LandMen::Vec3& OutWay) const;
	};
}
