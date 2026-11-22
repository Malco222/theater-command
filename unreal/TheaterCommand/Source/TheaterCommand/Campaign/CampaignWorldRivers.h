#pragma once

#include "CoreMinimal.h"

/**
 * The rivers of the world beyond the map of Europe (the whole-world map).
 *
 * Europe's rivers are the rules' own, laid as ribbons of water. Beyond Europe there are no provinces yet and so
 * no rivers the rules know; but a jungle with no river in it, or a savanna with no dry bed, is not the country
 * the art director's world look describes. These courses were worked out from NOAA's heights alone by
 * Tools/make_rivers_world.mjs (water runs downhill; a desert sheds next to nothing) and are read here once.
 * They are paint: the land's own picture carries them, and they change no rule.
 */
class FCampaignWorldRivers
{
public:
	/** The courses, read once from Content/Data/Campaign/rivers-world.bin. Call it first from the game's own thread. */
	static const FCampaignWorldRivers& Get();

	bool IsLoaded() const { return Courses.Num() > 0; }

	struct FCourse
	{
		// In the world map's own frame: x east from the seam at 30.45 W (a course that crosses the seam keeps
		// counting past it), y south from 71.4 N, in map units. From its source to where it ends.
		TArray<FVector2f> Points;
		TArray<float> Water; // how much water passes each point (rain-weighted square kilometres of the country above it)
		float MinX = 0.0f, MaxX = 0.0f, MinY = 0.0f, MaxY = 0.0f;
		int32 Joins = INDEX_NONE; // the river it joins, as its place among the courses: none where it reaches the sea or ends in a basin
		float JoinsWater = 0.0f; // how much water that river carries where this one joins it
		float HalfCap = 1.6f; // the most half its breadth may be: a tributary is never more than half as broad as the river it joins
		bool bGreat = false; // it reaches the sea (or its basin) as a great river: it keeps a great river's breadth through any country

		/** Half its breadth at one of its points, in map units, and the water it counts as carrying there. */
		float BreadthAt(int32 Point, float& OutWater) const;
		/** Half its breadth where it carries this much water (away from its source). */
		float BreadthFor(float WaterThere) const;
	};
	TArray<FCourse> Courses;

	/** Half the breadth a river is drawn with for the water it carries, in map units: a hairline of 0.12 for the lesser courses, about 0.6 for a main tributary, about 1.5 for the Amazon. */
	static float HalfBreadth(float Water);

private:
	void Load();
};
