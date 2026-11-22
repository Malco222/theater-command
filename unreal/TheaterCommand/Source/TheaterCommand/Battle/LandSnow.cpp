#include "Battle/LandScape.h"

// Snow over any ground: until its own file is built, the plain snow of LandScape (a white wash by cover, the clutter
// thinned, the roofs whitened).
namespace LandScape
{
	std::unique_ptr<Snow> MakeSnowCover(double Cover, Look L, uint64_t S)
	{
		return std::make_unique<Snow>(Cover, L, S);
	}
}
