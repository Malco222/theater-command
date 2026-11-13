#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campaign/CampaignMapData.h"
#include "Campaign/CampaignGround.h"
#include "Campaign/CampaignTerrain.h"
#include "Campaign/CampaignView.h"
#include "CampaignMapActor.generated.h"

class UProceduralMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UTextRenderComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;
class AArmyPiece;
class UTexture2D;
struct FCampaignMeshBuffers;

/**
 * The campaign table: Europe as a relief model set into a table, with the
 * nations painted on it, borders inked in, towns and seas lettered, and the
 * armies standing on it as pieces.
 *
 * Everything is built in code from the campaign data when the game starts, so
 * there is nothing here an editor has to place by hand and nothing large to
 * keep in the repository.
 */
UCLASS()
class THEATERCOMMAND_API ACampaignMapActor : public AActor
{
	GENERATED_BODY()

public:
	ACampaignMapActor();

	/** Reads the campaign data and builds the whole table. Returns false if the data could not be read. */
	bool BuildMap();

	const FCampaignMapData& GetData() const { return Data; }
	const FCampaignTerrain& GetTerrain() const { return Terrain; }

	/** Map units to world space. Height is in map units above the sea. */
	FVector MapToWorld(double X, double Y, double Height = 0.0) const;
	FVector2D WorldToMap(const FVector& World) const;

	/** A point on the surface of the table: on the ground over land, on the water over sea. */
	FVector SurfacePoint(double X, double Y, double Lift = 0.0) const;

	/** The same, but clear of every bump within a short reach: for lines drawn over rough ground, which a ridge would otherwise cut. */
	FVector SurfacePointOver(double X, double Y, double Lift) const;

	int32 RegionAtWorld(const FVector& World) const;

	void SetSelectedRegion(int32 RegionId);
	int32 GetSelectedRegion() const { return SelectedRegion; }
	void SetHoveredRegion(int32 RegionId);

	/** Says who holds each region now, and repaints the table if ground has changed hands. */
	void SetHolders(const TArray<FString>& Holders, const TArray<FString>& League = TArray<FString>());

	/**
	 * Stands the armies on the table as the rules say they are. Marches are corps that have just moved:
	 * their pieces are picked up and carried along the road they took.
	 */
	void ShowArmies(const FCampaignView& View, const TArray<FCorpsMove>& Marches, const FVector* CarriedFrom = nullptr);

	/** The piece a line from Origin along Direction passes through, nearest first. */
	AArmyPiece* PieceAtRay(const FVector& Origin, const FVector& Direction) const;

	/** The piece that holds this corps, if it is on the table. */
	AArmyPiece* FindPieceOfCorps(int32 CorpsId) const;

	/** The given nation's piece in a region, if it has one there. */
	AArmyPiece* FindPiece(int32 RegionId, const FString& Nation) const;

	/** Tints the regions a picked-up army could go to: its own colour for a march, red for an attack. An empty list clears it. */
	void SetReach(const TArray<FReachRegion>& Reach, bool bByRail = false);
	/** The time of year, from the month of the war (1 is September 1939): snow lies over the north and the high ground in winter. */
	void SetSeason(int32 Turn);
	/** The chief town of each province with what it pays, from the rules: the labels are written again to say so. */
	void SetTowns(const TArray<FTownInfo>& InTowns);
	bool HasTowns() const { return Towns.Num() > 0; }
	/** The wealth view: every province tinted gold by what it pays, on the table and on the small map. */
	void ShowWealth(bool bOn);
	bool IsWealthShown() const { return bWealth; }
	/** The friend-and-foe view: the player's own ground left as it is, his allies' washed blue, his enemies' red, on the table and on the small map. */
	void ShowSides(bool bOn, const TArray<FString>& Standing);
	bool AreSidesShown() const { return bSides; }
	/** Rings in gold the capitals of fallen countries that the player would set free by taking them. */
	void ShowFreeCapitals(const TArray<int32>& Capitals);
	/** Rings the seas a fleet in hand can sail to, the one the pointer is over more brightly, and the fleet itself. None clears them. */
	void ShowSeaReach(const TArray<int32>& Zones, int32 Hover, int32 FleetId);

	/** Draws the road an army would take, from where it stands through the regions given. An empty list clears it. */
	void SetRoute(const FVector& FromWorld, const TArray<int32>& Regions);

	/** Called as the camera moves: decides which lettering is worth showing and turns the standards to face the viewer. */
	void UpdateForCamera(float CameraDistance, float CameraYaw, const FVector& CameraFocus);

	/** One shared material per colour, so a hundred pieces of one nation cost one material. */
	UMaterialInstanceDynamic* GetSolidMaterial(const FLinearColor& Color, float Roughness, float Metallic, float Glow = 0.0f);

	/** A nation's flag of the period, as a picture. Painted the first time it is asked for. */
	UTexture2D* GetFlagTexture(const FString& Tag);

	/** The cloth a nation's standards are cut from: its flag, readable from either side. Null if the flag material is missing. */
	UMaterialInterface* GetFlagMaterial(const FString& Tag);

	/** The paint the pieces are moulded in: each point of a piece carries its own colour. */
	UMaterialInterface* GetPieceMaterial() const { return PieceMaterial; }

	/** Half the size of the map in world units, for keeping the camera over the table. */
	FVector2D GetWorldHalfExtent() const;

	/** World centimetres per map unit. Ten makes an army piece about the height of a person, which is the scale the engine's lighting is tuned for. */
	UPROPERTY(EditAnywhere, Category = "Campaign")
	float UnitScale = 10.0f;

	/** Extra vertical exaggeration of the relief. */
	UPROPERTY(EditAnywhere, Category = "Campaign")
	float VerticalScale = 1.0f;

	const TArray<TObjectPtr<AArmyPiece>>& GetPieces() const { return Pieces; }

	/** The models set out for inspection ("-TheaterParade=Kiev"): whether there are any, and where one stands on the map (or the middle of them all). */
	bool HasParade() const { return ParadeSpots.Num() > 0; }
	FVector2D ParadeSpot(int32 Index) const;
	float ParadeHeight() const { return ParadeLevel; }

	/** Lays the rivers and railways the rules know on the table (once, when a campaign begins). */
	void SetLand(const FLandFeatures& InLand);
	bool HasLand() const { return bLandLaid; }

	/**
	 * Stands the works of each province on the table as models: factories built since the war began, shipyards,
	 * anti-aircraft guns and airfields. Does nothing if nothing has changed.
	 */
	void ShowBuildings(const TArray<FIntVector4>& Works);

	/** Where a province's airfield lies on the map (or where it would), and the province whose airfield is at a point, if one is. */
	FVector2D AirfieldSpot(int32 RegionId) const;
	int32 AirfieldNear(const FVector2D& MapPoint) const;

	/** Draws the shipping lanes over the sea: pale where the cargoes get through, amber where raiders are at them, red where the lane is cut. */
	void ShowLanes(const TArray<FViewLane>& Lanes);

	/**
	 * A picture of one of the game's own models, for a unit's card: taken once for each model and kept. The name is
	 * a vehicle's, an aircraft's or a ship's ("PanzerIV", "B17", "Battleship:UK"), or a figure's ("Soldier:GER",
	 * "Rider:POL", "Pioneer:GER").
	 */
	UTextureRenderTarget2D* GetPortrait(const FString& Model);

	/** The model that stands for a kind of corps of a nation, as it is this year: its tank, its gun, its soldier. */
	static FString ModelFor(const FString& Nation, const FString& Kind);

	/** The model shown large and slowly turning beside the cards; empty for none. */
	void SetShowpiece(const FString& Model);
	UTextureRenderTarget2D* GetShowpiece() const { return ShowTarget; }
	const FString& GetShowpieceModel() const { return ShowModel; }

	/** Aircraft take off from their airfield, fly to the target and come home, over some seconds. */
	void FlySortie(const FSortie& Sortie);

	/** Smoke and gun flashes over a region where a battle has just been fought, for a few seconds. */
	void AddBattleSmoke(int32 RegionId);

	/** Shows which bridges are down and which railways are cut. Does nothing if nothing has changed. */
	void ShowWorks(const TArray<int32>& BridgesDown, const TArray<int32>& RailsCut);

	/** Where the fleet of this number lies, on the map (for the photographer: "@fleet2"). The middle of the map if there is none. */
	FVector2D FleetSpot(int32 Index) const;
	/** The whole map small, each country's ground in its colour, for the corner of the screen. Painted afresh when ground changes hands. */
	UTexture2D* GetMinimap();
	/** Where a fleet lies on the map, by its id. */
	FVector2D FleetPlace(int32 FleetId) const;

	/** Lays the fleets on their seas (or by their harbours) and the air fleets over their airfields. */
	void ShowFleets(const FCampaignView& View);

private:
	void LoadMaterials();
	void PaintGround();
	void PrepareLand();
	void UpdateLand(float CameraDistance, const FVector& CameraFocus, bool bAllAtOnce);
	UProceduralMeshComponent* BuildLandTile(int32 Level, int32 TileX, int32 TileY);
	bool LandTileHasLand(int32 Level, int32 TileX, int32 TileY) const;
	void BuildSeaMesh();
	void BuildBorders();
	void BuildWaterways();
	void BuildRailways();
	void BuildWorks();
	void BuildTable();
	/** The clutter of a war room on the table round the map: telephones, mugs, papers, pencils, spare counters, a pusher's rake. */
	void BuildWarRoom();
	void BuildLabels();
	void BuildTowns();

	/** For looking closely at the models ("-TheaterParade=Ardennes"): sets them out in a row, large, on a board over the region named. */
	void BuildParade();
	TArray<FVector2D> ParadeSpots;
	float ParadeLevel = 0.0f;
	void BuildTrees();
	void RebuildHighlight();
	void AddRegionOverlay(int32 RegionId, const FLinearColor& Fill, const FLinearColor& Outline, int32 FirstSection);
	void AppendRegionFill(FCampaignMeshBuffers& Buffers, int32 RegionId, const FLinearColor& Fill) const;
	void AppendRibbon(FCampaignMeshBuffers& Buffers, const TArray<FVector2D>& Points, double HalfWidth, double Lift, const FColor& Ink, bool bClosed) const;

	/** Where a nation's piece stands in a region: beside the town, not on its name; a second nation's piece stands further along. */
	FVector ArmySpot(int32 RegionId, int32 Slot) const;

	/** Spacing is extra room between letters, as a fraction of their height. */
	UTextRenderComponent* AddLabel(const FString& Text, const FVector2D& MapPos, float SizeUnits, const FColor& Color, int32 Tier, float Spacing = 0.0f);
	UStaticMeshComponent* AddBlock(const FVector& CenterWorld, const FVector& SizeWorld, UMaterialInterface* Material);

	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<UProceduralMeshComponent> SeaMesh;

	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<UProceduralMeshComponent> BorderMesh;

	/** Rivers; railways; and the bridges and breaks in the line, which change as the war goes. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> RiverMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> RailMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> WorksMesh;
	// The studio: a place far above the table where a model is set alone before a camera, for its picture.
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> PortraitMesh;
	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> PortraitCamera;
	UPROPERTY()
	TMap<FString, TObjectPtr<UTextureRenderTarget2D>> Portraits;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ShowMesh;
	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> ShowCamera;
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> ShowTarget;
	FString ShowModel;
	double ShowTurn = 0.0;
	/** Sets a model alone on a mesh, its middle at the mesh's own middle; says how big it is. False if there is none of that name. */
	bool StageModel(const FString& Model, UProceduralMeshComponent* Stage, double& OutRadius);
	void AimStudioCamera(USceneCaptureComponent2D* Camera, const FVector& Middle, double Radius);
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SortieMesh;
	struct FFlight
	{
		FSortie Sortie;
		double Began = 0.0;
		bool bStruck = false; // the bombs have been seen to fall
		TObjectPtr<UProceduralMeshComponent> Formation; // the aircraft themselves, built once and flown as one
		TObjectPtr<UProceduralMeshComponent> Rivals; // the defender's fighters that rise to meet them
		FVector At = FVector::ZeroVector; // where the formation is now, and which way it is heading
		FVector Dir = FVector::ForwardVector;
		FVector RivalAt = FVector::ZeroVector;
		double Share = 0.0; // how far through its flight it is, 0 to 1
		bool bEngaged = false; // the defender's fighters are among the bombers
	};
	TArray<FFlight> Flights;
	void UpdateSorties();
	/** A little train on the railway: which link it is on, when it set out, and which way it is going. */
	struct FTrain
	{
		int32 Rail = INDEX_NONE;
		bool bBackward = false; // from the link's second province to its first
		double Began = 0.0;
		int32 Links = 0; // how many links it has run so far
		TObjectPtr<UProceduralMeshComponent> Mesh;
	};
	TArray<FTrain> Trains;
	double NextTrain = 0.0;
	void UpdateTrains(const FVector& CameraFocus, float CameraDistance);
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> LanesMesh;
	FString ShownLanes; // what is drawn now, to tell when it has changed
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> RoomMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> BuildingsMesh;

	// Smoke: puffs that rise from the chimneys of works, and for a few seconds from a battlefield. Each puff is a
	// small fan of triangles turned to face the eye, dense in the middle and clear at the rim.
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SmokeMesh;
	struct FBattleSmoke
	{
		FVector Where = FVector::ZeroVector;
		double Began = 0.0;
	};
	TArray<FVector> TownChimneys; // the tops of the chimneys of the towns' own works
	TArray<FVector> WorksChimneys; // and of the factories built since the war began
	TArray<FBattleSmoke> Battles;
	TArray<FVector> SmokeVertices;
	TArray<FColor> SmokeColours;
	bool bSmokeMade = false;
	void UpdateSmoke(float CameraDistance, const FVector& CameraFocus);
	UPROPERTY()
	TObjectPtr<UTexture2D> MiniTexture;
	TArray<int16> MiniRegion; // the province under each point of the small map, or -1 for the sea
	int32 MiniWide = 0, MiniHigh = 0;
	bool bMiniStale = true;
	TArray<FIntVector4> ShownBuildings;
	TArray<int32> ShownFree;
	TArray<FTownInfo> Towns; // by province id, once the rules have said
	bool bWealth = false;
	bool bSides = false;
	TArray<FString> SideOf; // by province: own, ally, enemy or neutral, as the player stands to its holder
	int32 ShownMonth = -1;
	TMap<int32, FVector2D> AirfieldSpots; // by province: where its airfield stands
	FLandFeatures Land;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WaterMaterial;
	TArray<int32> ShownDown, ShownCut;
	bool bWorksShown = false;
	bool bLandLaid = false;

	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<UProceduralMeshComponent> HighlightMesh;

	/** What is drawn for an army that has been picked up: where it can go, and the road to where the pointer is. */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<UProceduralMeshComponent> OrdersMesh;

	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<UInstancedStaticMeshComponent> TownMarkers;

	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<UInstancedStaticMeshComponent> CapitalMarkers;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SolidMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HighlightMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LandMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SeaMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> InkMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PieceMaterial;

	/** Matt paint for the scenery: houses and trees. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SceneryMaterial;

	/** The towns, all in one mesh. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> TownMesh;

	/** The trees, in squares of country so that only those near the eye are drawn. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> TreeMeshes;
	TArray<FVector> TreeCentres; // the middle of each square, in the world
	TArray<bool> TreesShown;
	double TreeSquare = 0.0; // the side of a square, in the world

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> SolidMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FlagMaterial;

	// The paint of the table as pictures (see CampaignGround.h), and the fine detail laid over it.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GroundPicture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TintPicture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ReliefPicture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MasksPicture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WaterPicture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DetailPicture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BumpsPicture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RipplesPicture;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> FlagTextures;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> FlagMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> Labels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Blocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AArmyPiece>> Pieces;

	/** The fleets and the air fleets, each as a piece, by the number the rules know it by. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AArmyPiece>> FleetPieces;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AArmyPiece>> AirPieces;

	/** The same pieces by "region|nation". */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AArmyPiece>> PieceByKey;

	/** Which lettering tier each label belongs to: 0 always, 1 only from far off, 2 only close up. */
	TArray<int32> LabelTiers;

	FCampaignMapData Data;
	FCampaignTerrain Terrain;
	float WoodsShown = -1.0f; // how thickly the woods are laid on at the moment
	FCampaignGround Paint; // the table's colours, as pictures
	// The land's mesh is cut into square tiles, each the same number of quads across. Near the eye the tiles are
	// small and their quads fine; far off one tile covers the ground of four, sixteen or more, with quads to
	// match. The colour comes from pictures and is as fine as ever at any distance; only the shape coarsens.
	// A tile's key packs its level (0 the finest) and its place among the tiles of that level.
	static uint64 LandKey(int32 Level, int32 TileX, int32 TileY) { return (static_cast<uint64>(Level) << 48) | (static_cast<uint64>(TileX) << 24) | static_cast<uint64>(TileY); }
	TMap<uint64, TObjectPtr<UProceduralMeshComponent>> LandTiles; // the tiles standing on the table now
	TArray<TArray<float>> LandHeights; // the heights at each level: level 1 has every second point of level 0, and so on
	TArray<TArray<float>> LandCover; // the most land anywhere near each of those points, for telling which tiles are all sea
	TArray<FIntPoint> LandSizes; // how many points each level has, across and down
	int32 LandTopLevel = 0;
	bool bLandShadows = false;

	/** The tiles and the spare meshes, kept where the engine can see them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> LandMeshes;
	TArray<UProceduralMeshComponent*> SpareLandMeshes;

	int32 SelectedRegion = INDEX_NONE;
	int32 HoveredRegion = INDEX_NONE;
	int32 LabelState = -1;
	float LastPieceYaw = 0.0f;
	bool bFacingSet = false;
};
