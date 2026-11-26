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
	FString Lawful; // its lawful owner where another holds it (occupied ground: the rules' owner against its holder); empty where its holder is its owner
	FString Home; // whose it was when the map was drawn: a capital held by another flies the victor's flag
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

/** A great lake cut out of the land (the wider chart): its name, the height of its water in metres, where its name is written, and its shore. */
struct FCampaignLake
{
	FString Name;
	double Level = 0.0;
	FVector2D Label = FVector2D::ZeroVector;
	TArray<FVector2D> Outline;
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
	/**
	 * (Rules version 74) The wider chart: the same sheet carried on westward to the Pacific coast of North America,
	 * read with -TheaterWorld. Its provinces follow Europe's in number (FirstBeyond is the first of them) and keep
	 * the map's own coordinates, so North America lies at x below nought; its coast is drawn for every land in its
	 * frame, with the Great Lakes cut out. Charted is the box in which the coast is drawn by the files (Europe's
	 * own box without the wider chart): beyond it the land is wherever NOAA's heights stand above the sea.
	 */
	FBox2D Charted = FBox2D(ForceInit);
	int32 FirstBeyond = INDEX_NONE;
	TArray<TArray<FVector2D>> LandHoles; // waters cut out of the land: the Great Lakes
	TArray<FCampaignLake> Lakes;
	TArray<TArray<FVector2D>> RuledRivers; // the courses of the rules' rivers of the wider chart, which are laid as ribbons: the painted courses stand aside for them
	TArray<double> RuledBroad; // and half the breadth each ribbon comes to at its mouth, in map units
	/** Half the breadth a river's ribbon comes to at its mouth, by its name: the greatest of all, the great, and the rest. */
	static double RiverBroad(const FString& Name);
	/** Is a place inside the box the files draw the coast in, whichever turn of the world it is counted in? */
	bool IsCharted(double X, double Y) const;
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

	/**
	 * (Speed: tc.Speed.RegionGrid.) The map ruled into squares, each listing the provinces whose bounds reach into it,
	 * in the provinces' own order: FindRegionAt looks only at those, and finds the same province the whole list would.
	 */
	void RuleRegionGrid();
	FVector2D GridMin = FVector2D::ZeroVector, GridMax = FVector2D::ZeroVector;
	double GridCell = 0.0;
	int32 GridNX = 0, GridNY = 0;
	TArray<int32> GridStart; // (where each square's list begins in GridRegions, square by square, and one more for the end)
	TArray<int32> GridRegions; // (indices into Regions)

	bool bLoaded = false;
};
