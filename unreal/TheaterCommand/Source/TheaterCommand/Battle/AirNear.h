#pragma once

// Which finer B-17s may have their close-up shape made this frame (std-only, so the cloud test runs it).
//
// The Mac's timing of 27 November showed that holding 20 or more close-ups costs nothing the 60 fps cap shows, but that
// MAKING them is paid in the frame they are made: 4 or 5 in one frame gave a 49-64 ms frame, 8 gave 145 ms, 10 or 11
// gave 158 ms and 20 gave 201 ms. So a close-up is made for at most a few bombers a frame, the nearest first, and the
// rest wait a frame or two on their far shape. Off (PerFrame 0) the game keeps its old order and count.

#include <algorithm>
#include <vector>

namespace AirNear
{
/** A finer B-17 in the picture whose close-up shape is not made yet. */
struct FSeen
{
	int Key = 0;
	/** Metres from the eye. */
	double Away = 0.0;
	/** The close view or the timing eye rides her: she is made whatever this says, so she is never chosen here. */
	bool bRidden = false;
};

/**
 * The keys that may have their close-up made this frame: of those not ridden and nearer than NearM, the nearest first
 * (ties by the smaller key, so the order of Unmade never matters), while Made (the close-ups made already, ridden or
 * not, as the game's guard has always counted them) and those taken stay under Most, and no more than PerFrame of them.
 * PerFrame 0 or less takes none: the law is off and the game asks its old guard.
 */
inline std::vector<int> MayMake(std::vector<FSeen> Unmade, int Made, int Most, int PerFrame, double NearM)
{
	std::vector<int> Taken;
	if (PerFrame <= 0)
	{
		return Taken;
	}
	std::sort(Unmade.begin(), Unmade.end(), [](const FSeen& A, const FSeen& B)
	{
		return A.Away != B.Away ? A.Away < B.Away : A.Key < B.Key;
	});
	for (const FSeen& Each : Unmade)
	{
		if (static_cast<int>(Taken.size()) >= PerFrame || Made + static_cast<int>(Taken.size()) >= Most)
		{
			break;
		}
		if (!Each.bRidden && Each.Away < NearM)
		{
			Taken.push_back(Each.Key);
		}
	}
	return Taken;
}
}
