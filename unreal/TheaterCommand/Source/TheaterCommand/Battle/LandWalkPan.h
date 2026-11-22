// The walk pan: a photographer's shot of a walking platoon for the Mac builder's look test (no engine).
#pragma once

#include "Battle/LandEye.h"
#include "Battle/LandFeed.h"
#include "Battle/LandPlay.h"

#include <vector>

/**
 * The Mac recipe's look 7 (the crowd's walk motion vectors, 3073236c): a walking platoon at mid range, 25 to 140 m,
 * panned across slowly, its men's walk to be as clean as the skinned men's (no shimmer, no trail behind them, no smear
 * on a man who stops). "-TheaterLandWalkPan=<battle s>[,<seconds>]" asks it: from that battle second the eye stands
 * still off the flank of the block walking most steadily, a man's height up (or on a bank's height where the hedge is
 * in the way), and turns with the block's middle as it goes by. The same block and place for the same recording,
 * so the base and -TheaterLandNoVat show the same men.
 */
namespace LandWalkPan
{
	struct Plan
	{
		/** The block chosen, -1 none walking. */
		int Block = -1;
		/** Its men's tracks (alive and on the field throughout), the key men the recording carries. */
		std::vector<int> Men;
		/** Its middle's speed over the pan, metres a second. */
		double Speed = 0.0;
		/** Where the eye stands, and how far off the block's middle it is at the pan's start, middle and end. */
		LandPlay::Vec Eye;
		double FromStart = 0.0, FromMiddle = 0.0, FromEnd = 0.0;
		/** True if the sight lines to the block's middle at the start, middle and end were all open. */
		bool bOpen = false;
	};

	/** The block walking most steadily from T0 for For seconds (0.5 to 2.6 m/s, 6 men or more), and the eye off its
	 * flank: 45 to 100 m from the middle of its walk, a man's height up, or 3.5 m where that is screened. */
	Plan Choose(const LandFeed::Recording& Rec, const LandEye::World& Here, double T0, double For);

	/** The tank pan (the far tanks' motion vectors: LandCrowd's places a frame before; -TheaterLandTankPan): the tank
	 * driving most steadily from T0 for For seconds (2 to 9 m/s, alive and on the field throughout) that is seen in the
	 * open (the best eight tried in turn: a wood's or a town's tank gives way), and the eye off her flank, 170 to 240 m
	 * from the middle of her drive, so she stays past the 140 m hand-over (drawn by her model's far pieces) the whole
	 * pan: 2.5 m up, or 6 m, or 10 m where that is screened. Men holds her one track; Block is her block. */
	Plan ChooseTank(const LandFeed::Recording& Rec, const LandEye::World& Here, double T0, double For);

	/** The block's middle at battle time T, a metre up off the ground; false if none of its men is on the field. */
	bool Middle(const LandFeed::Recording& Rec, const LandEye::World& Here, const Plan& P, double T, LandPlay::Vec& Out);
}
