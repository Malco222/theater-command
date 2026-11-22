#include "Battle/LandScape.h"

// (not built yet: until it is, the ground falls back to the bocage, as before)
namespace LandScape
{
	std::unique_ptr<Region> MakeMed(const LandField::Field& F, uint64_t S)
	{
		(void)F;
		(void)S;
		return nullptr;
	}
}
