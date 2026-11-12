#pragma once

#include "CoreMinimal.h"

class FCampaignMapData;

/**
 * The relief of the campaign map: a grid of heights laid over the whole map,
 * worked out from the coastlines and from what kind of country each region is.
 *
 * It is the shape only. Colour, borders and everything drawn on top are the
 * map actor's business. Heights are in map units with the sea at zero.
 *
 * The relief is invented from each region's terrain for now (mountains where
 * the data says mountains). The same grid can be filled from real elevation
 * data later without anything else changing.
 */
class FCampaignTerrain
{
public:
	void Build(const FCampaignMapData& Data);

	int32 Index(int32 I, int32 J) const { return J * NX + I; }

	/** Height at any point of the map, in map units. */
	float HeightAt(double X, double Y) const { return Sample(Height, X, Y); }

	/** How much of the ground here is land: 0 open sea, 0.5 the shoreline, 1 well inland. */
	float CoverageAt(double X, double Y) const { return Sample(Coverage, X, Y); }

	/** Distance to the shoreline in map units: positive on land, negative at sea. */
	float ShoreAt(double X, double Y) const { return Sample(ShoreDistance, X, Y); }

	/** Slope of the ground at a grid point, as (dh/dx, dh/dy) in map units. */
	FVector2f GradientAt(int32 I, int32 J) const;

	static constexpr int16 Sea = -1;
	static constexpr int16 UnclaimedLand = -2;

	int32 NX = 0;
	int32 NY = 0;
	double Cell = 1.5;

	TArray<float> Height;
	TArray<float> Coverage;
	TArray<float> ShoreDistance; // map units to the shoreline: positive on land, negative at sea
	TArray<float> Roughness; // how mountainous: the relief amplitude at each point, in map units
	TArray<int16> Region; // region id at each grid point, or Sea / UnclaimedLand
	TArray<float> BorderDistance; // grid cells to the nearest border between two nations, capped

	/** Box blur with clamped edges, run Passes times. */
	static void Smooth(TArray<float>& Field, int32 Columns, int32 Rows, int32 Radius, int32 Passes);

private:
	/** Turns a field holding 0 at the starting points and a large number elsewhere into distances, in grid cells. */
	void SpreadDistance(TArray<float>& Field) const;

	float Sample(const TArray<float>& Field, double X, double Y) const;
};
