#pragma once

#include "CoreMinimal.h"

/**
 * The looks the ground of the world can take (the art director's world look, 14 November 2026). Europe keeps
 * its own recipe; beyond it every point of the ground picture takes its look from a table of boxes, read in
 * order, a later box painting over an earlier one, with the edges blurred and warped so that no look ends on
 * a ruled line. The look is paint only and changes no rule.
 */
namespace WorldLook
{
	enum Type : int32
	{
		Temperate,
		DrySouth,
		Prairie,
		Steppe,
		Desert,
		Savanna,
		Jungle,
		Monsoon,
		DryPlain,
		Taiga,
		Tundra,
		Ice,
		Alpine,
		Count
	};
}

class FCampaignClimate
{
public:
	/** The table, read once from Content/Data/Campaign/world-climate-boxes.json. */
	static const FCampaignClimate& Get();

	bool IsLoaded() const { return bLoaded; }

	/** How much of each look the ground at a place takes: thirteen shares that add up to one. All temperate where there is no table. */
	void SharesAt(double Latitude, double Longitude, float OutShares[WorldLook::Count]) const;

private:
	void Load();

	// One reading for every half degree, from 58 S to 74 N and right round the world from 180 W.
	static constexpr int32 PerDegree = 2;
	static constexpr int32 Wide = 360 * PerDegree;
	static constexpr int32 High = 132 * PerDegree;
	static constexpr double South = -58.0;

	bool bLoaded = false;
	TArray<float> Share[WorldLook::Count];
};
