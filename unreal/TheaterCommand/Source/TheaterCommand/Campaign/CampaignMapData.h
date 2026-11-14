#pragma once

#include "CoreMinimal.h"

/**
 * The campaign map as plain data, read from the same JSON files the browser
 * prototype uses (the repository's data/ and src/generated/ folders). Nothing
 * here knows about rendering: it is the "what is where" that every view of the
 * campaign shares.
 *
 * Map units are the prototype's: x runs east, y runs south, and the whole map
 * is Width x Height units (about 1491 x 1712).
 */

struct FCampaignNation
{
	FString Tag;
	FString Name;
	FString Adjective;
	FColor Color = FColor(128, 128, 128); // sRGB, as written in nations.json
	FString Alignment;
};

struct FCampaignRegion
{
	int32 Id = INDEX_NONE;
	FString Name;
	FString Owner; // nation tag
	FString Terrain; // plains, hills, forest, mountain, urban, desert, marsh
	int32 VictoryPoints = 0;
	int32 Factories = 0;
	bool bCapital = false;
	bool bPort = false;
	bool bOil = false;
	bool bOre = false;
	bool bDepot = false;
	bool bColony = false;
	int32 FortLevel = 0;
	FVector2D Center = FVector2D::ZeroVector; // the region's seed point (its chief town)
	FVector2D Centroid = FVector2D::ZeroVector; // middle of its largest piece of land
	TArray<TArray<FVector2D>> Polygons; // outer rings only; the map has no holes
	TArray<int32> Neighbors;
	FBox2D Bounds = FBox2D(ForceInit);
	double Area = 0.0;
};

struct FCampaignCorps
{
	FString Nation;
	FString Name;
	FString Kind; // inf, mot, arm, cav, mtn, fort
	int32 Divisions = 0;
	int32 RegionId = INDEX_NONE;
	FString General;
};

struct FCampaignSeaZone
{
	FString Name;
	FVector2D Center = FVector2D::ZeroVector;
};

class FCampaignMapData
{
public:
	/** Reads the map, the nations and the September 1939 armies. Returns false and says why if a file is missing or malformed. */
	bool Load(FString& OutError);

	bool IsLoaded() const { return bLoaded; }

	const FCampaignNation* FindNation(const FString& Tag) const { return Nations.Find(Tag); }

	/** The region whose land contains this point, or INDEX_NONE for sea and unclaimed land. */
	int32 FindRegionAt(double X, double Y) const;

	static bool PointInRing(const TArray<FVector2D>& Ring, double X, double Y);

	/**
	 * Where a campaign data file is. The data lives in one place, the repository's own data/ and
	 * src/generated/ folders, which the browser prototype and its tests also read. A packaged game
	 * cannot reach outside its own folder, so a copy in Content/Data/Campaign wins when it exists.
	 */
	static FString ResolveDataFile(const FString& FileName, const FString& RepoRelativeDir);

	static FString TerrainLabel(const FString& Terrain);
	static FString CorpsKindLabel(const FString& Kind);

	double Width = 0.0;
	double Height = 0.0;
	/**
	 * The world the map lies in (the whole-world map, step one). For today's map of Europe alone the world is the
	 * map itself. Run with -TheaterWorld, Europe is set in the frame the rules have fixed for the world map: 9,256.14
	 * units round (the seam at 30.45 W) and 5,096 from 71.4 N to 56 S, with Europe where it will be in the world's
	 * file, 500 units east of the seam. Until that file exists every coordinate stays Europe's own, so the world's
	 * west edge is at x = -500 here; ShiftX is what to add to one of these x's to have the world file's.
	 */
	bool bWorld = false;
	double WorldMinX = 0.0, WorldMinY = 0.0, WorldWidth = 0.0, WorldHeight = 0.0, ShiftX = 0.0;
	/** The longitude of x = 0 and the latitude of y = 0, in degrees: the map is 25.7115 units to a degree east and 40 to a degree south. */
	double Lon0 = -11.0, Lat0 = 71.4;
	double LongitudeAt(double X) const { return Lon0 + X / 25.7115; }
	double LatitudeAt(double Y) const { return Lat0 - Y / 40.0; }
	TArray<TArray<FVector2D>> Land; // every landmass, including land no region claims
	TArray<FCampaignRegion> Regions;
	TMap<FString, FCampaignNation> Nations;
	TSet<FString> League; // the members of the League, the third alliance: their ground is painted in its colour
	TArray<FCampaignCorps> Corps;
	TArray<FCampaignSeaZone> Seas;
	TMap<FString, int32> RegionByName;

	/** Where the files were read from, for the log. */
	FString SourceDescription;

private:
	bool LoadMap(const FString& Path, FString& OutError);
	bool LoadNations(const FString& Path, FString& OutError);
	bool LoadSetup(const FString& Path, FString& OutError);

	bool bLoaded = false;
};
