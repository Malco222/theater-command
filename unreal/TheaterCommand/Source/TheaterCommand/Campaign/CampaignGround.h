#pragma once

#include "CoreMinimal.h"

class FCampaignMapData;
class FCampaignTerrain;

/**
 * The paint of the campaign table, as pictures.
 *
 * The land used to be coloured point by point on its mesh, which tied how fine
 * the colour could be to how fine the mesh was. Here the same colours are worked
 * out for the same grid of points but kept as pictures laid over the land, so the
 * mesh can be coarse far from the eye without the colour going coarse with it,
 * the ground can be given real texture close up, and ground that changes hands
 * is repainted by changing a few pixels.
 *
 * There is one pixel for every point of the terrain's grid:
 *   Ground  the colour of the bare country, with how wooded it is in the alpha
 *   Tint    the same country as its nation has painted it
 *   Relief  which way the ground leans (red, green) and how the lamp and the
 *           lie of the land shade it (blue, half scale)
 *   Masks   how much bare rock (red), snow (green), shore sand (blue) and dry
 *           country (alpha)
 * and a coarser picture, Water, of the colour of the sea.
 */
class FCampaignGround
{
public:
	/** Paints every picture from the terrain and the map. */
	void Build(const FCampaignMapData& Data, const FCampaignTerrain& Terrain, float VerticalScale);

	/** Repaints the nations' colours over a box of grid points, after ground has changed hands. */
	void PaintTint(const FCampaignMapData& Data, const FCampaignTerrain& Terrain, int32 I0, int32 J0, int32 I1, int32 J1);

	int32 NX = 0;
	int32 NY = 0;
	TArray<FColor> Ground;
	TArray<FColor> Tint;
	TArray<FColor> Relief;
	TArray<FColor> Masks;

	/** The sea's colour, one pixel every WaterStep map units. */
	double WaterStep = 3.0;
	int32 WaterNX = 0;
	int32 WaterNY = 0;
	TArray<FColor> Water;

	// The colours the material lays over the paint where the masks say.
	static FLinearColor RockColour();
	static FLinearColor SnowColour();
	static FLinearColor SandColour();

private:
	// The colour of the bare ground at every point, softened across region boundaries. Kept for repainting.
	TArray<float> GroundR, GroundG, GroundB;
	float PlainsLightness = 1.0f;
};
