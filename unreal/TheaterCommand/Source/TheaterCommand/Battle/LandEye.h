#pragma once

#include "Battle/LandFeed.h"
#include "Battle/LandPlay.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * The ground battle's camera director (Brian, 21 November 16:22Z: "the same intense feeling as the air battle as if
 * i'm watching" a war film; a feeling to aim for, nothing copied). Plain C++, so the cloud test can run the whole
 * battle through it and check that the eye never goes under the ground, never cuts too often, and is where the
 * fighting is when men fall.
 *
 * How a war film is shot, which this follows: mostly from among the men, at their height, hand-held, with something
 * near in the frame; the lens is often long and close on a few men; the wide shots are few and short, there to say
 * where everyone is; the camera is there before the shell lands, not after. The recording is known from the start, so
 * the director looks a few seconds ahead (as a film's director knows the script) and frames the men who are about to
 * be under fire, never inventing anything.
 *
 * The shots:
 *  - Shoulder: hand-held at a man's shoulder (1.5 m up, a metre behind him), looking where he looks.
 *  - Low: lying in the grass beside a man who is down in cover (0.4 m up), rounds striking ahead.
 *  - Tank: beside a tank's track, 1.8 m up, looking along her gun.
 *  - Watch: from a hedge or a tree 6 to 12 m up, 70 to 140 m off, a long lens on a few men under fire.
 *  - Wide: 40 to 70 m up behind a side's line, across the field: where everyone is.
 *  - Open: the opening's slow look over the field from high behind the attack, coming down.
 */
namespace LandEye
{
	using LandPlay::Vec;

	enum class Kind : unsigned char { Open, Wide, Watch, Shoulder, Low, Tank };
	const char* NameOf(Kind K);

	/** Where the eye is, what it looks at, the lens (vertical field of view, degrees) and the roll (degrees). */
	struct Pose
	{
		Vec Eye, Look;
		double Fov = 55.0;
		double Roll = 0.0;
		Kind Shot = Kind::Open;
		int Subject = -1;
		/** True on the frame of a cut (the scene snaps, the sky and effects may re-sort). */
		bool bCut = false;
	};

	struct World
	{
		std::function<double(double X, double Y)> Height;
		/** Whether the eye can stand at a point (not inside a bank, a hedge's shrubs, a house or a tree); unset: yes. */
		std::function<bool(const Vec& At)> Free;
		/** Whether the line between two points is open (no bank, house or hedge across it); unset: yes. */
		std::function<bool(const Vec& From, const Vec& To)> Open;
		/** Where a track is now and which way it faces (the cores' own: a man's head, a tank's turret). Unset: the
		 * recording's track. */
		std::function<bool(int Track, double T, Vec& OutAt, double& OutFacing)> Where;
	};

	struct Settings
	{
		/** The side whose men the camera stays with most (0 the attack, the player's in this dry run). */
		int Own = 0;
		double OwnShare = 0.7;
		/** How long a shot holds, real seconds (a war film cuts every 3 to 8 s in a fight). */
		double ShotMin = 4.5, ShotMax = 9.0;
		/** The opening's length. */
		double Opening = 9.0;
		/** The hand-held wobble's strength (0 off, 1 as made). */
		double Hand = 1.0;
		uint64_t Seed = 1;
	};

	class Director
	{
	public:
		bool Begin(const LandFeed::Recording& InRecording, const World& InWorld, const Settings& InSettings);
		/** The pose for battle time Battle, real seconds Real since the scene opened (the clock may run faster than
		 * life, or be held: the hand-held wobble and the cut timing go by the real clock). */
		Pose Update(double Battle, double Real, double RealDt);
		/** Forces a cut on the next update (a key), or holds the shot it has (Hold true) until let go. */
		void CutNow() { bCutAsked = true; }
		void Hold(bool bHold) { bHeld = bHold; }
		/** A shot of a kind on a subject asked for (the picture run). Subject -1: the director's choice. */
		void Ask(Kind K, int Subject = -1);

		// For the test.
		int Cuts() const { return CutCount; }
		const std::vector<std::pair<double, Kind>>& Log() const { return CutLog; }

	private:
		const LandFeed::Recording* Rec = nullptr;
		World Here;
		Settings Set;
		Pose Now;
		Kind Shot = Kind::Open;
		int Subject = -1;
		double ShotStarted = 0.0, ShotLength = 6.0;
		double BattleAtCut = 0.0;
		bool bCutAsked = false, bHeld = false;
		bool bAsked = false;
		Kind AskedKind = Kind::Wide;
		int AskedSubject = -1;
		/** Where the eye stands relative to its subject for this shot (metres: behind, beside, up) and its own place
		 * for a still shot (Watch, Wide). */
		double Back = 1.0, Side = 0.5, Up = 1.5;
		Vec Stand, Target;
		Vec EyeEased, LookEased;
		double FacingEased = 0.0;
		/** The subject fell during this shot: hold on him a while, then cut. */
		double FellAt = -1.0;
		uint64_t Dice = 0;
		double Phase[6] = { 0, 0, 0, 0, 0, 0 };
		int CutCount = 0;
		std::vector<std::pair<double, Kind>> CutLog;
		std::vector<Kind> Recent;
		/** The falls the director has already decided on (shown or passed over), so each is weighed once. */
		std::vector<int> Appointed;

		double Roll();
		double HeightAt(double X, double Y) const;
		bool WhereIs(int Track, double T, Vec& OutAt, double& OutFacing) const;
		/** How much is about to happen round a point over the next seconds (rounds landing, men falling, tanks
		 * firing), from the recording. */
		double HeatAt(double X, double Y, double From, double To, double Radius) const;
		void Choose(double Battle, double Real);
		bool Place(Kind K, int Track, double Battle);
		Vec KeepAbove(Vec At, double Least) const;
	};
}
