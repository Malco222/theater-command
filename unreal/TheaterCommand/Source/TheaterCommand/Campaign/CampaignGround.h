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

	/**
	 * A fine sheet let into a coarse one (the whole-world map): well offshore its sea takes the coarse sheet's own
	 * colour, so that in open water nobody can find the fine sheet's edge. Set before Build; both must outlive it.
	 */
	const FCampaignGround* CoarseSea = nullptr;
	const FCampaignTerrain* CoarseShape = nullptr;

	/** Repaints the nations' colours over a box of grid points, after ground has changed hands. */
	void PaintTint(const FCampaignMapData& Data, const FCampaignTerrain& Terrain, int32 I0, int32 J0, int32 I1, int32 J1);

	/**
	 * The ownership paint (the art director's brief of 15 November 2026). Three things, all of them in the stain:
	 * nothing here is a line, a mark or a glow, and nothing is brighter than the stain it sits in.
	 *
	 * Occupied: point for point with the ground, the colour a stripe takes on occupied ground (the holder's stain
	 * mixed toward the lawful owner's, each made the usual way), and in its alpha how far the ground there is
	 * occupied: nothing near a national frontier, nothing under a besieger's colour, nothing on free ground. The
	 * land's paint rules the stripes themselves.
	 *
	 * A siege: the besieger's plain stain lies over the field from the sides his side holds, as deep as the siege
	 * has gone; the town keeps an island of its holder's colour to the end. Set before PaintTint.
	 */
	TArray<FColor> Occupied;
	struct FSiege
	{
		int32 Region = INDEX_NONE;
		FString By;
		float Depth = 0.35f; // how far across the province the colour has come: 0.35 when the siege is laid, 1 when the town is about to fall
		bool bEverySide = false;
		TArray<int32> From;
		FVector2D Town = FVector2D::ZeroVector;
		float Island = 8.0f; // the radius of the ground round the town that never turns, in map units
	};
	TArray<FSiege> Sieges;
	/**
	 * By province: how far the snow lying on it takes the colours of its ground toward white, as they are seen
	 * (nothing in summer, about 0.3 in deep winter in the east). A stripe of occupied ground is laid stronger
	 * under snow, so that it shows as well in January as in July. Set before PaintTint.
	 */
	TArray<float> RegionSnow;

	/**
	 * How far each point of a province lies from the frontier a colour comes in by, as a share of the way to its far
	 * side: for the besieger's colour, and for ink that spreads. From the sides named; from every side; or, with no
	 * side named, from the coast, or else from the town.
	 */
	struct FFront
	{
		int32 I0 = 0, J0 = 0, NI = 0, NJ = 0;
		float Reach = 1.0f; // how far the far side lies, in map units
		TArray<float> Far; // for each point of the box: how far in it lies, in map units; less than nothing outside the province
	};
	const FFront& FrontOf(const FCampaignMapData& Data, const FCampaignTerrain& Terrain, int32 Region, const TArray<int32>& From, bool bEverySide);
	/** How far a front that has come this share of the way covers a point: 1 behind it, 0 ahead, soft and wandering between. */
	static float FrontCover(const FFront& Front, const FCampaignTerrain& Terrain, int32 I, int32 J, float Share);
	/** The same as a place along the front's way, in map units behind the front (less than nothing ahead of it), for the wet rim. */
	static float BehindFront(const FFront& Front, const FCampaignTerrain& Terrain, int32 I, int32 J, float Share);
	void ForgetFronts() { Fronts.Reset(); }

private:
	// (Each front is kept in a place of its own that never moves. Kept in the map itself, a front already handed
	// out was moved when the map grew to take another: with many provinces changing hands at once, as when a
	// campaign of 1941 is laid over the map of 1939, the ink read a front that was no longer there.)
	TMap<uint32, TSharedPtr<FFront>> Fronts;

public:

	int32 NX = 0;
	int32 NY = 0;
	TArray<FColor> Ground;
	TArray<FColor> Tint;
	TArray<FColor> Relief;
	TArray<FColor> Masks;
	/**
	 * What covers the ground besides the bare paint (art fix 7). Red: how much of it is farmland, for the quilt of
	 * fields. Green: how large the fields are there (small, hedged ones in the west; great open ones on the steppe).
	 * Blue: the rim of the woods, 0.5 where there is none: lighter on a wood's sunward edge, darker on the ground
	 * in its lee. Alpha: how far the woods there are fir and pine.
	 */
	TArray<FColor> Cover;
	/**
	 * How the fields lie, province by province, so that all Europe is not ruled on one grid: red and green are the
	 * cosine and sine of the angle the province's field system is turned by (0.5 is nought), blue how large its
	 * blocks are beside the common size (0.5 is the same). Read without smoothing: a province's fields break off at
	 * its edge, as field systems do.
	 */
	TArray<FColor> Lie;

	/**
	 * The waters of the world beyond Europe (the whole-world map): its rivers, worked out from the heights
	 * (CampaignWorldRivers.h), and the small lakes of the northern forest and the pools of the tundra. Empty where a
	 * sheet has none. Point for point with the ground:
	 *   Wet     red and green, which way the nearest water's own line lies from the point and how far, east and
	 *           south: a half is nought, nought and one are WetReach map units either way. (Read between the
	 *           picture's points, the two together come to nothing exactly on the line, and beside one straight
	 *           stretch they are the true distance from it: which is what draws a river narrower than the picture's
	 *           own points with a clean bank.) Blue, how far that line is, with no direction to it: where it is much
	 *           more than the first two say, the points round about look to different waters and no line is there.
	 *           Alpha, half the water's breadth there, 2.5 map units at one.
	 *   WetInk  the colour of the water there as it is to be seen, and in its alpha how much of its breadth, from the
	 *           banks inward, is dry bed: under a half, twice the share that is bars which come and go along it; from
	 *           a half up, how far above it is twice the share that is dry all along (a dry river's sand, a lake's shore).
	 */
	TArray<FColor> Wet, WetInk;
	float WetReach = 4.0f;
	/**
	 * The picture of the waters may be ruled finer than the ground: so many of its points to each of the ground's,
	 * each way (two on a fine sheet, one on the coarse sheet of the world). Where hairlines met inside one of the
	 * ground's cells, the picture's points round the meeting looked to different waters and none was drawn there:
	 * a creek stopped a cell short of the stream it joins. The picture's point K lies (K + 0.5) / WetScale - 0.5
	 * of the ground's cells from the ground's first point, so that the same place on the sheet reads both pictures.
	 */
	int32 WetScale = 1, WetNX = 0, WetNY = 0;

	/** The sea's colour, one pixel every WaterStep map units. */
	double WaterStep = 1.5;
	int32 WaterNX = 0;
	int32 WaterNY = 0;
	TArray<FColor> Water;
	/** With the sea's picture, point for point: in its red, how strong the surf is on that stretch of shore. */
	TArray<FColor> Surf;

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

public:
	/** The region a point of the grid is painted as: its own, or for a point just off the shore the nearest land's. Negative for the open sea. */
	int32 PaintedRegion(int32 Index, const FCampaignTerrain& Terrain) const;
};
