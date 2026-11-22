#pragma once

// The ground battle's plain C++ state, kept apart from the actor's header so that the cores' standard-library types
// never pass through Unreal's header tool. Only LandBattle.cpp and LandBattleScreen.cpp include it.

#include "Battle/LandCrater.h"
#include "Battle/LandDesk.h"
#include "Battle/LandEye.h"
#include "Battle/LandFeed.h"
#include "Battle/LandField.h"
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
	std::unique_ptr<LandMen::Crowd> Men;
	std::unique_ptr<LandKit::Battle> Armour;
	Fates::Table FatesTable;

	/** Each recording track's man in the crowd and vehicle in the armour (-1: none), and back. */
	std::vector<int> ManOf, VehicleOf;
	std::vector<int> TrackOfMan, TrackOfVehicle;

	/** The camera's pose this frame and the words on the screen. */
	LandEye::Pose Pose;
	LandDesk::Words Words;

	/** Battle seconds shown, and real seconds since the battle opened. */
	double Battle = 0.0, Real = 0.0;
	/** The speed asked for (0: held; 0.5, 1, 2, 4), and whether the scene still sets it itself (twice life until the
	 * armies are about to meet, then life). */
	double Speed = 1.0;
	bool bOwnSpeed = true;
	/** The speed this frame ran at. */
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
