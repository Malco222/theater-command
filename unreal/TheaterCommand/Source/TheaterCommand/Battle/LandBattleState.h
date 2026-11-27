#pragma once

// The ground battle's plain C++ state, kept apart from the actor's header so that the cores' standard-library types
// never pass through Unreal's header tool. Only LandBattle.cpp and LandBattleScreen.cpp include it.

#include "Battle/LandAir.h"
#include "Battle/LandBail.h"
#include "Battle/LandBurn.h"
#include "Battle/LandCrater.h"
#include "Battle/LandDesk.h"
#include "Battle/LandEye.h"
#include "Battle/LandFeed.h"
#include "Battle/LandField.h"
#include "Battle/LandGeneral.h"
#include "Battle/LandGeneralEye.h"
#include "Battle/LandKit.h"
#include "Battle/LandMen.h"
#include "Battle/LandNight.h"
#include "Battle/LandPlay.h"
#include "Battle/LandSight.h"
#include "Battle/LandSound.h"
#include "Battle/LandTeamGun.h"
#include "Battle/LandWalkPan.h"
#include "Battle/Fates.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

struct FLandBattleState
{
	/** The recording, read once (LandFeed), and its text (the men's and the tanks' cores read it for themselves). */
	LandFeed::Recording Rec;
	std::string RecText;

	// The cores: the fire, the sound, the camera, the words; the ground, the men, the tanks.
	LandPlay::Replay Play;
	LandSound::Mixer Mix;
	LandEye::Director Eye;
	/** (Camera specials, release 2) -TheaterLandWhistle: the attack's officer blows his whistle as he steps off, the
	 * film opening on him (LandEye WhistleAt, WhistleMan); and whether it has sounded. */
	bool bWhistleOn = false, bWhistleBlown = false;
	/** The earned names by block id, for the film's card (-TheaterLandNickCard): empty strings where none. */
	TMap<int32, FString> NickOfBlock;
	/** The walk pan's plan once chosen (-TheaterLandWalkPan, the Mac recipe's look 7; it asks EyeWorld below). */
	LandWalkPan::Plan Pan;
	LandDesk::Desk Desk;
	std::unique_ptr<LandField::Ground> Field;
	/** What stands where, for the camera (LandSight): the field's trees, shrubs and houses laid once, the men and the
	 * vehicles placed again each frame before the camera asks. */
	LandSight::Room Sight;
	/** The soldier files' head and chest bones (-1: none), by file, for placing the men in Sight and following a fall. */
	std::vector<std::pair<const LandMen::Soldier*, std::pair<int, int>>> SightBones;
	/** The shell holes made so far (LandCrater), kept the whole battle. */
	LandCrater::Field Holes;
	/** The tanks' and lorries' track marks so far, and the corn their tracks laid flat (LandCrater), kept the whole battle. */
	LandCrater::Ruts Ruts;
	/** The ground drawn, cut over the dug holes' bowls (a tile's near ground split into parts once dug in). */
	LandCrater::Dug Dug;
	std::unique_ptr<LandMen::Crowd> Men;
	std::unique_ptr<LandKit::Battle> Armour;
	/** Effects (R2): the wrecks' film fire, -TheaterLandFilmFire (LandBurn). */
	LandBurn::Director Burns;
	/** Effects (R2): the crews getting out of knocked-out tanks, -TheaterLandBailOut (LandBail). */
	LandBail::Book Bails;
	/** Effects (R2): the fighters on call over the field, -TheaterLandAirSupport (LandAir; the scene's FlyOver,
	 * Battle/LandBattleAir.cpp): the plan, and what the effects kit has been told of it. */
	struct FAirSupport
	{
		bool bAsked = false, bOn = false, bMade = false;
		LandAir::Sky Sky;
		size_t Built = 0;                // the recording's air notes the plan was made from
		double Told = -1.0;              // the battle time the kit was last told up to (under 0: not yet)
		std::vector<LandAir::Plane> Planes;
		std::vector<LandAir::Gunner> Guns;
		std::vector<LandAir::Happen> Happens;
		std::set<int> Firing, Gunning;   // the kit's guns firing now: the planes' (GunIdOf) and the ground's (GunnerIdOf)
		std::set<int> Falling, Hit, Struck; // planes by key: going down with a trail, hit, into the ground
		/** Effects (R2): the downed pilots (-TheaterLandChutes): each one now, by his plane's key the shape his mesh was
		 * last built to (his canopy's open share in the air; 2 and its slump once down), and once down his man in the
		 * crowd and the clips he is blended between. */
		bool bChutes = false;
		std::vector<LandAir::Chute> ChutesNow;
		struct FChuteMan
		{
			double Shape = -1.0;
			int Man = -1;
			std::string File, Clip, WasClip;
			double Switched = -1.0, WasTime = 0.0, NowTime = 0.0;
		};
		std::map<int, FChuteMan> ChuteMen;
	};
	FAirSupport AirSupport;
	Fates::Table FatesTable;

	/** Each recording track's man in the crowd and vehicle in the armour (-1: none), and back. */
	std::vector<int> ManOf, VehicleOf;
	std::vector<int> TrackOfMan, TrackOfVehicle;

	/**
	 * The support teams' weapons (release 2, on trial "teamguns"; Battle/LandTeamGun): a machine gun on its mount or a
	 * mortar at each team's key man (role hmg or mortar), stepped each frame from where the crowd draws him
	 * (ALandBattle::MakeTeamGuns). The weapon files read so far; each weapon, with its copies in its file's three instanced
	 * meshes (her mount, what turns, and a belt-fed gun's box that swings with her; -1: none yet); each track's weapon (its row in TeamGuns, -1 none), which the fire reads for her muzzle
	 * (PlayWorld.Muzzle) and her burst (PlayWorld.GunLook); and the files asked for and not read, each said once.
	 */
	struct FTeamGun
	{
		int Track = -1, Kind = -1;
		LandTeamGun::Gun Gun;
		int32 MountCopy = -1, TurnedCopy = -1, SwungCopy = -1;
		bool bMountShown = false, bTurnedShown = false, bSwungShown = false;
		/** Her gunner has been told he serves her (LandMen::Crowd::Serve). */
		bool bServed = false;
	};
	bool bTeamGuns = false;
	std::vector<LandTeamGun::Kind> TeamKinds;
	std::vector<FTeamGun> TeamGuns;
	std::vector<int> TeamGunOf;
	std::vector<std::string> TeamFilesBad;

	/**
	 * The tank commanders up in their hatches (LandMen riders, the soldiers thread's tank crews): one an American or
	 * German tank whose model the crew file's clips have a place on (MakeRiders), seated on her commander socket each
	 * frame and posed by her hatch, her roof gun's bursts and his end (RideTanks).
	 */
	struct FRider
	{
		int Vehicle = -1, Man = -1;
		/** His clips: in the hatch with his glasses, at the roof gun, firing it, and his two ends. */
		enum Kind { Hatch, Roof, Burst, Slump, Drop, Kinds };
		int Clips[Kinds] = { -1, -1, -1, -1, -1 };
		/** Each clip's root on this model: the socket moved forward, right and up of its facing (the clip's places). */
		LandMen::Vec3 Offsets[Kinds];
		float Long[Kinds] = { 1.f, 1.f, 1.f, 1.f, 1.f };
		float DropHide = 0.85f;         // die_hatch_drop's hide: his head under the rim (life seconds into it)
		float Phase = 0.f;              // his own offset into the loops (life seconds), so no two look about as one
		/** Her roof gun's bursts (battle time, in order): he takes the gun before them and fires each one. */
		std::vector<double> Bursts;
		/** When he is killed up in the hatch (a round that struck him there, or her end while he was up), -1 never; and
		 * the clip he dies by (Slump or Drop), or Kinds: not seen to die (thrown with her turret: gone at once). */
		double Killed = -1.0;
		int Dies = Kinds;
		/** What he plays now, and what he is leaving (blended out over a third of a second). */
		int Now = -1, Was = -1;
		float NowTime = 0.f;            // how far into Now (life seconds), as last posed
		double SwitchedAt = -1e9;       // battle time the clip changed
		float WasTime = 0.f;            // the left clip's time when it changed (life seconds)
		float Rise = 0.f;               // how far up out of the hatch he stood when last alive (0 down inside, 1 up)
		/** His field glasses' copy (the scene's GlassCopies; -1 none made yet), and whether it is in his hands now. */
		int Glass = -1;
		bool bGlass = false;
	};
	std::vector<FRider> Riders;
	/** Each vehicle's roof gun manned now (the German MG 34 is drawn only while it is: the tank modeller's rule); true
	 * where she has no commander drawn. */
	std::vector<uint8_t> RoofManned;

	/**
	 * The men who ride the light cars and motorcycles (release 2: the art director's rule, none moves empty): her driver,
	 * and the seats her kind fills (a car's front passenger, a jeep's gunner at her gun, a sidecar's gunner or rider, a
	 * pillion where the machine has no sidecar seat), each from his army's infantry file's seat clips (the soldiers
	 * thread's ride_clips: a clip whose places name her model and that socket). He sits his seat's loop and slumps by its
	 * death clip from the moment she is knocked out (SeatLight, RideLight).
	 */
	struct FSeated
	{
		int Vehicle = -1, Man = -1;
		std::string Socket;
		/** His seat's loop and his death in it (-1: none), and each one's root offset from the socket. */
		int Clip = -1, Death = -1;
		LandMen::Vec3 Offset, DeathOffset;
		float Phase = 0.f;              // his own offset into the loop (life seconds)
	};
	std::vector<FSeated> Seated;
	/** Each vehicle looked at for seats yet (her men seated, or none to seat). */
	std::vector<uint8_t> SeatedDone;

	/**
	 * The carriers' benches as they stand this frame (the lorries and half-tracks whose models have bench seats,
	 * rider.1 to rider.10: LandKit::BenchesNow), for the lorried men aboard (LandMen's SeatFn, LandKit::BenchFor; Brian,
	 * 23 November 14:10Z: "troops are stuck under the truck"). Made before the men are posed (SeatCarriers); read by the
	 * men's posing threads at once, never written then.
	 */
	std::vector<LandKit::Benches> Carriers;

	/** A live battle's general: his clipboard and what his screen draws this frame (LandBattleCommand.cpp). */
	FLandGeneralShown General;
	/** The camera's world as the film has it (Open), kept for the general's view. */
	LandEye::World EyeWorld;
	/** The general's view (LandGeneralEye, the camera thread's): his own camera over the field while the film is not
	 * asked for; what he does to it this frame (the screen fills it: keys held, slides, wheel, pinch, a double click),
	 * the turn keys and the tilt keys held (-1..1), whether it is begun, and the film and his view switched this frame
	 * (the picture cuts). */
	LandGeneralEye::Eye GeneralEye;
	LandGeneralEye::Input GeneralIn;
	double GeneralTurnHeld = 0.0, GeneralTiltHeld = 0.0;
	bool bGeneralEye = false, bEyeSwitched = false;
	/** The free view's row (`freeview`, release 2) as it stood when his view began: the hill view, the low look by
	 * his tilt keys and the lock-on are his for this battle. */
	bool bFreeView = false;

	/** The camera's pose this frame and the words on the screen. */
	LandEye::Pose Pose;
	/** The film director's own pose this frame, kept while the general's view has the camera (the film in a corner,
	 * release 2). */
	LandEye::Pose FilmPose;
	LandDesk::Words Words;

	/** Battle seconds shown, and real seconds since the battle opened. */
	double Battle = 0.0, Real = 0.0;
	/**
	 * Life seconds a battle second: the rules' clock runs about twice as fast as life (a man rushing at 6 m/s, a tank
	 * across a field at 10, every arm alike: the drill thread, 22 November), so the fight is played at half the battle's
	 * clock and tanks crawl at 5 m/s, men rush at 3. The men's clips and the tanks' are timed in life's seconds by it.
	 * "-TheaterLandLife=1" plays it as before (the battle's clock taken for life's).
	 */
	double Life = 2.0;
	/** Life seconds played (real seconds times the speed asked for): the effects kit's clock, so a shell's burst, its smoke
	 * and a burst's tracers run at life's pace while the battle's clock runs at half of it. */
	double Lived = 0.0;
	/** The speed asked for, against life (0: held; 1, 2, 4), and whether the scene still sets it itself (four times life
	 * until the armies are about to meet, then life). */
	double Speed = 1.0;
	bool bOwnSpeed = true;
	/** The speed this frame ran at, against life (the battle's clock ran at SpeedNow / Life). */
	double SpeedNow = 1.0;

	/** The men's draw (the battle-scale plan's GS3): a man the lens draws under ~2 px tall (CullPx) is not drawn, past
	 * MenCullFrom metres of seen distance (the usual 50 deg lens's), except in the wide mass shot, where the far men stay
	 * as dots; and at most MenRebuildsMax men's meshes made again for a new level of detail a frame, the rest kept at
	 * their old level a frame or two longer (a remade mesh rebuilds its whole draw). */
	double MenCullFrom = 1e12;
	int MenRebuildsMax = 50;
	/** The picture's lines a man 1.8 m tall stands over at 1 m, at the usual 50 deg lens and 665 lines (GS3's measure:
	 * Lines / D at D metres), for the mass shot's least size. */
	double MenLines = 1283.5;

	/** The night before the attack (LandNight): its shots, and this frame's (while NightNow.bNight its shots play and the
	 * battle's clock waits at its start; Dark and Black the night's light and the dip to black, over the dawn too). */
	LandNight::Prelude Night;
	LandNight::Frame NightNow;

	/** The battle's end: the last line shown, then the after card a few seconds on. */
	bool bOver = false, bCard = false;
	double OverReal = 0.0;
	/**
	 * After the break (release 2, the battle rule `pursue`): the phase (Rec.After, 1 asked or 2 pursuing) in which he
	 * gave his answer or called his pursuit off, 0 with none sent. While it is the recording's phase still, his press
	 * is on its way to the rules: the buttons are put away and nothing is sent twice. Cleared when the phase moves on
	 * or the rules refuse it.
	 */
	int AfterSent = 0;
	/** A short note in the middle of the screen (the camera held, a cut asked for), and how long it has shown. */
	std::string Note;
	double NoteAge = -1.0;

	// What each frame hands on, kept to save the allocations.
	std::vector<LandPlay::Effect> Fx;
	std::vector<LandPlay::Noise> Noises;
	std::vector<LandPlay::Loss> Losses;
	std::vector<LandSound::Take> Takes;
	/** Takes held for later (flash to bang, the same take held 0.15 s): the real second each is due. */
	std::vector<std::pair<double, LandSound::Take>> Later;
	std::vector<LandSound::Bed> Beds;
	/** The one-shot takes' voices (land.voices: ALandBattle::VoicePlayers holds a player for each), and the cuts due
	 * this frame. */
	LandSound::Voices Voices;
	std::vector<int> VoiceFades;
	/** Each vehicle's engine loop's name, worked out when her cue changes (ALandBattle::EngineLoopNames). */
	LandSound::EngineNames EngineNames;
};
