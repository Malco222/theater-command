#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campaign/CampaignMapData.h"
#include "Campaign/CampaignGround.h"
#include "Campaign/CampaignTerrain.h"
#include "Campaign/CampaignView.h"
#include "CampaignMapActor.generated.h"

class UProceduralMeshComponent;
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
	void SetHolders(const TArray<FString>& Holders);

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
	void BuildTable();
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
