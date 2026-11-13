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
 *   Ground  the colour of the bare country as nature made it, with how wooded
 *           it is in the alpha
 *   Tint    the colour the country takes where its nation's stain lies on it,
 *           with how near the national frontier is in the alpha: the stain
 *           is a wash everywhere and a band along the frontier
 *   Relief  which way the ground leans (red, green), how the lamp and the
 *           lie of the land shade it (blue, half scale), and in the alpha how
 *           much more wash a nation of little colour needs to be read
 *   Masks   how much bare rock (red), snow (green), shore sand (blue) and dry
 *           country (alpha)
 * and a picture, Water, of the colour of the sea: pale over the shelves, dark
 * over the deeps, turquoise in the shallows, with how far offshore a point is
 * in its alpha (0 at the shore, 1 at six map units and beyond), from which the
 * sea's material draws the line of surf.
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
	double WaterStep = 1.5;
	int32 WaterNX = 0;
	int32 WaterNY = 0;
	TArray<FColor> Water;

	// The colours the material lays over the paint where the masks say.
	static FLinearColor RockColour();
	static FLinearColor SnowColour();
	static FLinearColor SandColour();

	/** How much more wash than the rest a nation needs for its ground to read as its own: more for a grey one. */
	static float WashBoost(const FColor& NationColour);
	/** How far inside a national frontier its band of stronger stain reaches, in map units. */
	static constexpr double BandReach = 8.0;

private:
	// The colour of the bare ground at every point, as it is seen (0 to 1, not linear light). Kept for repainting.
	TArray<float> GroundR, GroundG, GroundB;
	float PlainsLightness = 1.0f;
	// For points just off the shore: the region of the nearest land, so the paint runs right to the water's edge.
	TArray<int16> ShoreRegion;
};
