#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campaign/CampaignMapData.h"
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
	void UpdateForCamera(float CameraDistance, float CameraYaw);

	/** One shared material per colour, so a hundred pieces of one nation cost one material. */
	UMaterialInstanceDynamic* GetSolidMaterial(const FLinearColor& Color, float Roughness, float Metallic, float Glow = 0.0f);

	/** Half the size of the map in world units, for keeping the camera over the table. */
	FVector2D GetWorldHalfExtent() const;

	/** World centimetres per map unit. Ten makes an army piece about the height of a person, which is the scale the engine's lighting is tuned for. */
	UPROPERTY(EditAnywhere, Category = "Campaign")
	float UnitScale = 10.0f;

	/** Extra vertical exaggeration of the relief. */
	UPROPERTY(EditAnywhere, Category = "Campaign")
	float VerticalScale = 1.0f;

	const TArray<TObjectPtr<AArmyPiece>>& GetPieces() const { return Pieces; }

private:
	void LoadMaterials();
	void PrepareGround();
	void PaintVertices(int32 I0, int32 J0, int32 I1, int32 J1);
	void BuildTerrainMesh();
	void BuildTerrainChunk(int32 ChunkX, int32 ChunkY);
	void BuildSeaMesh();
	void BuildBorders();
	void BuildTable();
	void BuildLabels();
	void BuildTownMarkers();
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
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> SolidMaterials;

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

	/** The same pieces by "region|nation". */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AArmyPiece>> PieceByKey;

	/** Which lettering tier each label belongs to: 0 always, 1 only from far off, 2 only close up. */
	TArray<int32> LabelTiers;

	FCampaignMapData Data;
	FCampaignTerrain Terrain;
	TArray<FColor> VertexColors;

	// What the paint is mixed from that never changes: the colour of the bare ground at every point, and how
	// each point stands against the ground around it. Kept so that repainting after a conquest is quick.
	TArray<float> GroundR, GroundG, GroundB, Surround;
	float PlainsLightness = 1.0f;
	int32 ChunksX = 0;
	int32 ChunksY = 0;

	int32 SelectedRegion = INDEX_NONE;
	int32 HoveredRegion = INDEX_NONE;
	int32 LabelState = -1;
	float LastPieceYaw = 0.0f;
	bool bFacingSet = false;
};
