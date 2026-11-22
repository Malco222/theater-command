#pragma once

// The ground battle's plain C++ state, kept apart from the actor's header so that the cores' standard-library types
// never pass through Unreal's header tool. Only LandBattle.cpp and LandBattleScreen.cpp include it.

#include "Battle/LandCrater.h"
#include "Battle/LandDesk.h"
#include "Battle/LandEye.h"
#include "Battle/LandFeed.h"
#include "Battle/LandField.h"
#include "Battle/LandGeneral.h"
#include "Battle/LandGeneralEye.h"
#include "Battle/LandKit.h"
#include "Battle/LandMen.h"
#include "Battle/LandPlay.h"
#include "Battle/LandSight.h"
#include "Battle/LandSound.h"
#include "Battle/Fates.h"

#include <memory>
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
	Fates::Table FatesTable;

	/** Each recording track's man in the crowd and vehicle in the armour (-1: none), and back. */
	std::vector<int> ManOf, VehicleOf;
	std::vector<int> TrackOfMan, TrackOfVehicle;

	/** A live battle's general: his clipboard and what his screen draws this frame (LandBattleCommand.cpp). */
	FLandGeneralShown General;
	/** The camera's world as the film has it (Open), kept for the general's view. */
	LandEye::World EyeWorld;
	/** The general's view (LandGeneralEye, the camera thread's): his own camera over the field while the film is not
	 * asked for; what he does to it this frame (the screen fills it: keys held, slides, wheel, pinch, a double click),
	 * the turn keys held (-1..1), whether it is begun, and the film and his view switched this frame (the picture cuts). */
	LandGeneralEye::Eye GeneralEye;
	LandGeneralEye::Input GeneralIn;
	double GeneralTurnHeld = 0.0;
	bool bGeneralEye = false, bEyeSwitched = false;

	/** The camera's pose this frame and the words on the screen. */
	LandEye::Pose Pose;
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

	/** The battle's end: the last line shown, then the after card a few seconds on. */
	bool bOver = false, bCard = false;
	double OverReal = 0.0;
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
};
