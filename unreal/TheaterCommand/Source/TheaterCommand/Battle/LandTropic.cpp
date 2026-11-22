#include "Battle/LandScape.h"

// (not built yet: until it is, the ground falls back to the bocage, as before)
namespace LandScape
{
	std::unique_ptr<Region> MakeTropic(const LandField::Field& F, uint64_t S, bool bIsland)
	{
		(void)F;
		(void)S;
		(void)bIsland;
		return nullptr;
	}
}
