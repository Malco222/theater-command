#pragma once

#include "Battle/AirLand.h"

#include <string>

/**
 * (Release 2, the air battle's hitch: the Mac's timing legs of 26 November, build 172f19ae0.) The ground's pictures
 * under a raid with no stage (Battle/AirLand) take the pool 13.6 to 18.2 s to paint, and one draw-side freeze of about a
 * second fell inside that paint in every leg. What the scene needs to know about a paint's inputs, in plain C++ so that
 * the native test checks exactly what the game does:
 * - SamePlace: whether two asks of the painter read the same place, field by field (the row flyPaintEarly: a paint begun
 *   as the replay is queued is used only when the battle, once open, asks for the very same ground; else it is painted
 *   again, so the pictures are always the bytes today's paint makes).
 */
namespace AirGroundKeep
{
	/** Every field of the place the painter reads, compared exactly. */
	inline bool SamePlace(const AirLand::Place& A, const AirLand::Place& B)
	{
		return A.AimX == B.AimX && A.AimY == B.AimY && A.NorthX == B.NorthX && A.NorthY == B.NorthY && A.Terrain == B.Terrain && A.Name == B.Name
			&& A.Month == B.Month && A.Seed == B.Seed && A.SunX == B.SunX && A.SunY == B.SunY && A.SunZ == B.SunZ && A.Land == B.Land && A.Rivers == B.Rivers;
	}
}
