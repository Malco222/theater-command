#pragma once

#include "Battle/LandPlay.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

/**
 * The ground battle's sound, decided in plain C++ from the sound director's mix (Content/Data/Sound/mix.json, read as
 * written: no number of it is copied here) so that the cloud test hears what the game hears and can count it.
 *
 * The director's rules for the field (21 November, by the land thread): drive the shots from the rounds; a shot on
 * the screen is always heard (the limits drop only what is off the screen or far); the same take never starts twice
 * within 0.15 s; a hit on a man is silent (his fall and, sometimes, his cry carry it); no incoming whistle; a moving
 * shooter's take moves with him (the scene's part); the distance low-pass's first point is near; log the shots heard
 * against the takes started.
 */
namespace LandSound
{
	using LandPlay::Vec;

	/** Where the ear is: the ground point the camera looks at, raised by the mix's heightFactor times the camera's
	 * height over it (the scene works it out), the camera's yaw, and the camera's height over the ground. */
	struct Listener
	{
		Vec At;
		double Yaw = 0.0;
		double CameraHeight = 2.0;
	};

	/** A one-shot to start: Delay real seconds from now, at At (or about the ear when not bSpatial), in the mix's
	 * distance family, on its bus, at Db over the file as made and Pitch; From seconds into the take, cut at Cut
	 * seconds (0: to its end) with CutFade. */
	struct Take
	{
		double Delay = 0.0;
		std::string Name;
		Vec At;
		std::string Family, Bus = "close";
		double Db = 0.0, Pitch = 1.0;
		double From = 0.0, Cut = 0.0, CutFade = 0.0, FadeIn = 0.0;
		bool bSpatial = true;
		bool bBig = false;
		/** The shooter's track whose take moves with him (-1 none). */
		int Track = -1;
	};

	/** A loop under the battle and its level now (a bed or the place's air), dB; Silent when it should not play. */
	struct Bed
	{
		std::string Name;
		double Db = 0.0;
		bool bSilent = true;
	};

	/** A distance family of the mix. */
	struct Family
	{
		double Full = 25.0, Silent = 700.0, FarFrom = 1e9;
	};

	class Mixer
	{
	public:
		/** Reads the mix; false (and why) if it cannot. Seed makes the picks repeat run to run. */
		bool Begin(const std::string& MixText, uint64_t Seed, std::string& OutError);
		/** How long a take plays, for the limits (the scene gives the files' own; unset, about 1.5 s). */
		std::function<double(const std::string& Name)> Length;
		/** Whether a world point is on the screen (the scene's camera); unset, all is on it. */
		std::function<bool(const Vec& At)> OnScreen;
		/** The falls' thuds come from the soldiers' own clips (the body_fall event as the body lands): then a Fall noise
		 * gives only the cry. Off (the cloud test without the soldiers' core), the thud follows the fall by 0.7 s. */
		bool bThudFromClips = false;

		/** A source heard: what to start. Now is the real clock (seconds), Speed the battle clock's against it, Battle
		 * the battle time now (a noise's own time may lie a little behind it within the frame). */
		void Hear(const LandPlay::Noise& N, double Now, double Battle, double Speed, const Listener& Ear, std::vector<Take>& Out);
		/** A soldier's clip event heard (a footstep, his knee down, his kit, his bolt, a body landing): Cue the take's
		 * stem as the clip names it (footstep, knee_down, kit_rattle, bolt, body_fall, grenade_throw...). */
		void HearBody(const std::string& Cue, const std::string& Army, const Vec& At, double Now, const Listener& Ear, std::vector<Take>& Out);
		/** Each frame: the meter and the beds. Fighting is how many blocks are in act 'fight' within the mix's
		 * blockRadius of the ear, FightingShare their share of all blocks (for a high camera); bContact once the armies
		 * have met; Cover the ground round the ear (open, woods, buildings, water). */
		void Tick(double Now, double RealDt, const Listener& Ear, int Fighting, double FightingShare, bool bContact, const std::string& Cover, std::vector<Bed>& OutBeds);
		double Meter() const { return MeterNow; }
		/** The meter's raw heat this frame, before it is scaled by fullAt and clamped (for setting fullAt). */
		double Heat() const { return HeatNow; }
		/** The mix's listener.heightFactor: the ear stands over the looked-at ground by this times the camera's
		 * height over it. */
		double EarHeightFactor() const { return HeightFactor; }
		/** A blast close enough to dull the ears (the mix's shellShock), taken once: its low-pass, and how long down,
		 * held and back (seconds). False when none is due. */
		bool TakeShellShock(double& OutLpfHz, double& OutDown, double& OutHold, double& OutBack, double& OutDb);

		// The log (the director's: shots heard against takes started).
		int ShotsHeard = 0, ShotTakes = 0, ShotsOutOfHearing = 0, ShotsDropped = 0, ShotsOnScreenDropped = 0, Cracks = 0, Landings = 0, Falls = 0, Delayed = 0;
		std::map<std::string, int> TakesByName;
		/** The heat each weight key has added over the run (weight x closeness), for the log. */
		std::map<std::string, double> HeatBy;

	private:
		std::map<std::string, Family> Families;
		std::map<std::string, int> Limits;
		int WorldTotal = 40;
		double FtbFrom = 150.0, FtbSpeed = 343.0, FtbMost = 3.0;
		double HeightFactor = 0.6;
		// The meter.
		double Window = 6.0, Radius = 800.0, BlockRadius = 600.0, PerBlock = 3.0, FullAt = 60.0, Rise = 1.5, Fall = 10.0;
		std::map<std::string, double> Weights;
		double MeterNow = 0.0, HeatNow = 0.0;
		struct Heard { double At; double Weight; };
		std::vector<Heard> HeardList;
		// The beds.
		struct BedRow { std::string Name; double In, Full, Hold, Out, Db; };
		std::vector<BedRow> Beds;
		std::string PlaceOpen = "wind_loop", PlaceWoods = "woods_loop";
		double PlaceDb = 0.0, PlaceFullDb = -10.0, PlaceTownDb = -6.0;
		double RumbleDb = -6.0, RiflesFarBeforeDb = -14.0;
		// Takes.
		std::map<std::string, std::vector<std::string>> Lists;
		std::map<std::string, double> SizeDb, SizeBig;
		double NearWithin = 60.0, MiddleWithin = 150.0;
		double DebrisFrom = 2.3, DebrisTo = 4.9, DebrisDb = 12.0, DebrisFadeIn = 0.15, Debris0 = 0.6, Debris1 = 0.9;
		double NearTwoCut = 2.3, NearTwoCutFade = 0.3;
		double CryChance = 0.25, CryBelow = 60.0;
		double BoltWithin = 30.0, BoltChance = 0.3;
		double CaseWithin = 40.0;
		double LevelDb = 2.0;
		std::map<std::string, double> PitchPercent;
		double ShockWithin = 25.0, ShockBelow = 80.0, ShockLpf = 600.0, ShockDown = 0.1, ShockHold = 1.2, ShockBack = 2.5, ShockDb = -6.0, ShockEvery = 90.0;
		double ShockAt = -1e9;
		bool bShockDue = false;
		// What is playing, for the limits: by family, each take's end (real seconds) and distance.
		struct Playing { double Ends; double Far; };
		std::map<std::string, std::vector<Playing>> PlayingNow;
		std::map<std::string, double> TakeStarted;
		std::map<std::string, std::string> LastOfList;
		std::map<int, int> RifleCount;
		double CrackAt = -1e9;
		/** The last Start found its take past its family's silent distance. */
		bool bLastTooFar = false;
		uint64_t Dice = 0x9E3779B97F4A7C15ull;

		double Roll();
		const std::string& Pick(const std::string& ListName);
		std::vector<std::string> ListOf(const std::string& Name) const;
		const Family& FamilyOf(const std::string& Name) const;
		/** Starts a take if the limits allow (always when on the screen): false if it was dropped. */
		bool Start(Take T, double Now, double Far, bool bOnScreen, std::vector<Take>& Out);
		double FlashToBang(double Far, double Speed) const;
		void Count(double Now, const std::string& WeightKey, double Far);
		void Crack(const LandPlay::Noise& N, double Now, const Listener& Ear, std::vector<Take>& Out);
	};
}
