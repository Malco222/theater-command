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

	/** A town's name plate: where it stands on the table, what it reads, and from how far off it is shown. */
	struct FPlateSpot
	{
		int32 Region = INDEX_NONE;
		FVector World = FVector::ZeroVector; // the town's place on the table
		float Reach = 0.0f; // how far toward the eye, in the world's units, the plate hangs from it
		FString Name;
		FString Line; // what the place is worth, shown close up
		int32 Tier = 0; // 0 a capital, shown from any distance; 1 a town, from the middle distance; 2 a place worth having, close up
		int32 Rank = 0; // the more a place matters, the sooner its plate wins a crowded spot
	};
	const TArray<FPlateSpot>& GetPlateSpots() const { return PlateSpots; }

	/**
	 * A name lettered across the map as an atlas letters it: a country's along the long axis of the ground it
	 * holds, with a slight arc; a sea's across its middle. The letters are spaced out from From to To.
	 */
	struct FMapWord
	{
		FString Text;
		FVector2D From = FVector2D::ZeroVector, To = FVector2D::ZeroVector; // where the first and the last letter stand
		double Bulge = 0.0; // how far the middle of the word stands off the straight line, toward the letters' tops
		double Height = 6.0; // the height of a capital, in map units
		bool bSea = false;
		bool bRegion = false; // a province's own name, small, written whole where the province's middle is
	};
	const TArray<FMapWord>& GetMapWords() const { return MapWords; }
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
	/** What the rules say of a province's chief town, or null before they have said. */
	const FTownInfo* TownOf(int32 RegionId) const { return Towns.IsValidIndex(RegionId) && Towns[RegionId].Region == RegionId ? &Towns[RegionId] : nullptr; }
	/** The wealth view: every province tinted gold by what it pays, on the table and on the small map. */
	void ShowWealth(bool bOn);
	bool IsWealthShown() const { return bWealth; }
	/** The friend-and-foe view: the player's own ground left as it is, his allies' washed blue, his enemies' red, on the table and on the small map. */
	void ShowSides(bool bOn, const TArray<FString>& Standing);
	bool AreSidesShown() const { return bSides; }
	/** How far into the night the room is, 0 to 1: the windows of the towns are lit as it comes on. */
	void SetNight(float Night, bool bLamplight = false);
	/** The mud of the eastern spring and autumn: the ground it lies on is washed brown. None clears it. */
	void ShowMud(const TArray<int32>& Regions);
	/** The fog of war: ground the player has no eyes on lies under a shadow, on the table and on the small map. */
	void ShowShroud(const TArray<int32>& Unseen);
	/** Rings in gold the capitals of fallen countries that the player would set free by taking them. */
	void ShowFreeCapitals(const TArray<int32>& Capitals);
	/** Rings the seas a fleet in hand can sail to, the one the pointer is over more brightly, and the fleet itself. None clears them. */
	void ShowSeaReach(const TArray<int32>& Zones, int32 Hover, int32 FleetId);

	/** Draws the road an army would take, from where it stands through the regions given. An empty list clears it. */
	/** RouteEnds: 0 a march, 1 a battle at the end of it, 2 a frontier that means war. */
	void SetRoute(const FVector& FromWorld, const TArray<int32>& Regions, int32 RouteEnds = 0);

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
	/** The same over a sea where fleets have fought. */
	void AddSeaBattle(int32 Zone);

	/** Shows which bridges are down and which railways are cut. Does nothing if nothing has changed. */
	void ShowWorks(const TArray<int32>& BridgesDown, const TArray<int32>& RailsCut);
	/**
	 * Stands the towns' own buildings on the table, each at its town's edge: four numbers for each, the region,
	 * the line (0 market, 1 farm, 2 school, 3 camp, 4 hospital, 5 hall), the level that stands, and 1 while the
	 * next is going up.
	 */
	void ShowCivic(const TArray<int32>& Civic);
	/** The countries in flat colour, as a map view: the stain laid on whole instead of as a wash and a band. */
	void SetFlatColours(bool bFlat);
	/** While a key is held: every province line at full strength. */
	void SetShowProvinces(bool bShow) { bShowProvinces = bShow; }
	bool AreColoursFlat() const { return bFlatColours; }
	/** How strongly the countries are coloured: 0 light, 1 as the art is meant, 2 strong. */
	void SetColourStrength(int32 Level);
	int32 GetColourStrength() const { return ColourStrength; }
	/**
	 * Leaves the scars of battle on the ground where fighting lately was: craters, burnt-out wrecks, and smoke
	 * while they are fresh. Three numbers for each place: the region, the months since, and how big it was (1 to 3).
	 */
	void ShowScars(const TArray<int32>& Scars);
	/** Smoke hangs over the towns that are besieged: three numbers each, the region, 1 if the ring is closed, months of stores left. */
	void ShowSieges(const TArray<int32>& Sieges);
	/**
	 * The airfields that have air groups based on them (Brian's ask of 2026-11-13: no great aircraft piece standing
	 * on the map; the airfield itself says it is in use): a small aircraft parked on the field for each group, in
	 * its nation's colours. The groups themselves are seen by clicking the airfield.
	 */
	void ShowAirfieldSigns(const TArray<FViewAirfield>& Fields);
	/** Where an airfield stands in the world, for its badge on the screen; false if the province has none on the table. */
	bool GetAirfieldWorld(int32 RegionId, FVector& OutWorld) const;
	/**
	 * How much larger a province's town is built than the size towns first had (art fix 8: one scale for everything
	 * on the table, a capital the largest thing on it and a town about as wide as an army's base): 1 where there is
	 * no town. Whatever stands round a town stands that much further out from its middle.
	 */
	double TownSpread(int32 RegionId) const;
	/** How far the outermost roofs of a province's town stand from its middle, in map units (about 3 where there is no town). */
	double TownRadius(int32 RegionId) const;
	/** True for a city or capital in a province too small for it: built tight and tall. */
	bool IsTownPacked(int32 RegionId) const;
	/** How far south of a town's middle the buildings its owner gives it begin, and how far that whole quarter is turned (degrees). */
	bool CivicInside(int32 RegionId) const; // true where that quarter is kept inside the town's own streets
	double CivicNear(int32 RegionId) const;
	double CivicTurn(int32 RegionId) const;
	/** Whether towns keep ground open inside them for such buildings: only where the rules have them (the grand campaign). The towns are built again if it changes. */
	void SetTownPlots(bool bPlots);
	/** Prizes outside the towns (Version 56): a fire where one is wrecked, a pennant in the colour of the army that holds one outside a town it has not taken. */
	void ShowPrizes(const TArray<FPrizeMark>& Marks);
	/** The night's war: the nations whose towns are blacked out, and the fronts (pairs of regions) where the guns flicker. */
	void SetWarAtNight(const TArray<FString>& Blackout, const TArray<int32>& Fronts);
	/** How far the table is into a moonlit night, 0 to 1 (nothing by day, and nothing by lamplight). */
	float GetMoonlight() const { return bLamplightNow ? 0.0f : NightNow; }
	/** Where a flight takes off from: its airfield, or for a carrier's aircraft the ships themselves. */
	FVector2D SortieStart(const FSortie& Sortie) const;

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
	/**
	 * Where the army in a province's given slot stands: beside the town and never on its roofs. Count is how many
	 * armies stand in the province; where there is not room for them all at full size they are drawn at four
	 * fifths and fanned round the town, and bOutSmall says so.
	 */
	FVector ArmySpot(int32 RegionId, int32 Slot, int32 Count = 1, bool* bOutSmall = nullptr) const;

	/** Spacing is extra room between letters, as a fraction of their height. */
	UTextRenderComponent* AddLabel(const FString& Text, const FVector2D& MapPos, float SizeUnits, const FColor& Color, int32 Tier, float Spacing = 0.0f, int32 OfRegion = INDEX_NONE);
	/** The ink a region's name is written in: dark on a pale country, pale on a dark one. */
	FColor RegionInk(int32 RegionId) const;
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
	/** Ground that has just changed hands flashes once: the province, and when. */
	TArray<TPair<int32, double>> Flashes;
	double FlashesDrawn = 0.0;
	void UpdateFlashes();
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
	TArray<FVector> TownWindows; // the windows of the towns' houses, each as its place and the way it faces
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> LightsMesh; // those windows lit, at night
	TArray<FVector> LightPoints;
	TArray<FColor> LightColours;
	float ShownNight = -1.0f;
	float NightNow = 0.0f; // how far into the night the room is, 0 to 1
	bool bLamplightNow = false; // the night is the old lamplight, not moonlight
	TArray<int32> PaneRegion; // the region each lit pane belongs to: a window of its town
	// The lights of villages and farms: each a small round glow lying on the land, soft at its edge.
	TArray<FVector> VillagePoints;
	TArray<FColor> VillageColours;
	TArray<int32> VillageRegion;
	static constexpr int32 VillageRim = 6; // points round a village light
	// The glow of each town's lamps as it is seen from far off: a soft disc of light over the town.
	TArray<FVector> GlowPoints;
	TMap<int32, TArray<FVector2D>> TownQuarters; // by province: its town's middle and the middles of its lit quarters, on the map
	bool bTownPlots = false;
	TArray<FVector> GlowShown; // the same, with every dark glow gathered into its middle so that it costs nothing to draw
	TArray<FColor> GlowColours;
	TArray<int32> GlowRegion;
	TArray<uint8> GlowKind; // 0 the lamps of a town, 1 a quarter of a great city beside them, 2 the faint warm wash a lit town throws on the country round it
	static constexpr int32 GlowRim = 12; // points round each ring of a glow
	static constexpr int32 GlowRings = 3; // rings to a glow: a hot heart, its fall, and the edge where it is gone
	TArray<FString> BlackoutTags;
	TArray<int32> FrontPairs;
	/** A fire on a front by night: where, how big (most are small, one in five large), and how hot (below 1 it is only an ember). */
	struct FFrontFire
	{
		FVector At = FVector::ZeroVector;
		float Size = 1.0f;
		float Heat = 1.0f;
	};
	TArray<FFrontFire> FrontFires; // along the frontier the armies face each other across: thick where they face, thin elsewhere
	TArray<FVector> FrontGlow; // and evenly along the same frontier, for one faint line of glow under the fires
	// The night's grading of the water (the picture run can try others: "-TheaterNightSea=grey,surf,r,g,b,liftr,liftg,liftb").
	FVector2f SeaNight = FVector2f(0.7f, 0.3f); // how far the water is greyed by moonlight, and the strength of the surf line
	FLinearColor SeaNightTint = FLinearColor(0.42f, 0.38f, 0.33f);
	FLinearColor SeaNightLift = FLinearColor(0.06f, 0.06f, 0.065f);
	FLinearColor RiverNightTint = FLinearColor(7.0f, 3.0f, 1.45f); // rivers by moonlight are steel, not blue
	float TownWash = 0.2f; // the strength of the faint warm wash under a lit town
	float FrontLineGlow = 0.085f; // the strength of the line of glow under a front's fires
	TArray<FVector> WorksChimneys; // and of the factories built since the war began
	TArray<FBattleSmoke> Battles;
	TArray<FVector> SmokeVertices;
	TArray<FColor> SmokeColours;
	bool bSmokeMade = false;
	void UpdateSmoke(float CameraDistance, const FVector& CameraFocus);
	UPROPERTY()
	TObjectPtr<UTexture2D> MiniTexture;
	TArray<int16> MiniRegion; // the province under each point of the small map, or -1 for the sea
	TArray<uint8> MiniShade; // how the ground there takes the light (160 is level ground), or for the sea how deep it is
	int32 MiniWide = 0, MiniHigh = 0;
	bool bMiniStale = true;
	TArray<FIntVector4> ShownBuildings;
	TArray<int32> ShownFree;
	TArray<int32> ShownMud;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> CloudMesh;
	TArray<FVector> CloudVertices;
	TArray<FColor> CloudColours;
	void UpdateClouds();
	TArray<int32> ShownUnseen;
	TSet<int32> UnseenSet;
	TArray<FTownInfo> Towns; // by province id, once the rules have said
	bool bWealth = false;
	bool bSides = false;
	TArray<FString> SideOf; // by province: own, ally, enemy or neutral, as the player stands to its holder
	int32 ShownMonth = -1;
	// The year turning: the look the trees wear (0 summer, 1 September, 2 October, 3 November, 4 winter, 5 March,
	// 6 April), how hard the winter lies (0 to 1), and what the rivers were last drawn for.
	int32 TreeLook = 0;
	float WinterHard = 0.0f;
	float RiversDrawnFor = -1.0f;
	/** How deep the snow lies at a place on the map (0 to 1) when the winter is this hard. */
	static float SnowLying(const FVector2D& Place, float Hard);
	TMap<int32, FVector2D> AirfieldSpots; // by province: where its airfield stands
	TMap<int32, FVector2D> AirfieldLie; // and how it lies: X the way it is turned (degrees), Y its size (1 to 4)
	FString ShownAirSigns; // what the aircraft parked on the airfields were last built from
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> AirSignMesh; // aircraft parked on the airfields that have air groups based there
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

	// The quiet lines of the map, each faded by how high the eye is: province lines, railways, and their sleepers.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ProvinceInk;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ProvincePaleInk;
	float ProvincePaleShown = -1.0f;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ShorePicture;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RailInk;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SleeperInk;
	float ProvinceShown = -1.0f, RailShown = -1.0f, SleeperShown = -1.0f;
	bool bShowProvinces = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PieceMaterial;

	/** Matt paint for the scenery: houses and trees. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SceneryMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WoodsMaterial; // the trees' own, so that they can be cooled by moonlight

	/** The towns, all in one mesh. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> TownMesh;
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> CivicMesh;
	TArray<int32> ShownCivic;
	bool bFlatColours = false;
	int32 ColourStrength = 1;
	float WashShown = -1.0f;
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> ScarsMesh;
	TArray<int32> ShownScars;
	TArray<FVector> Smoulders; // wrecks still burning, for the smoke that rises from them
	TArray<int32> ShownSieges;
	FString ShownPrizes; // what the prize marks were last built from
	TArray<FVector> PrizeFires; // wrecked prizes, for the flame and the smoke over them
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> PrizesMesh; // the pennants over prizes held outside a town
	TArray<FVector> SiegeFires; // besieged towns, for the smoke that hangs over them
	// Pieces on the move, to know when one is set down; and where pieces were lately set down, for the dust.
	TSet<TWeakObjectPtr<AArmyPiece>> OnTheMarch;
	TArray<TPair<FVector, double>> SetDowns;

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
	TObjectPtr<UTexture2D> CoverPicture; // farmland, the size of its fields, the rims of the woods, and where they are fir
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FieldsPicture; // the tones of the fields, one pixel a field
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LiePicture; // how the fields lie in each province: the turn of its grid and the size of its blocks

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
	TArray<int32> LabelRegions; // the region a label names, where its ink should suit that region's paint; or none
	TArray<FPlateSpot> PlateSpots;
	TArray<FMapWord> MapWords;
	void BuildWords();

	FCampaignMapData Data;
	FCampaignTerrain Terrain;
	float WoodsShown = -1.0f; // how thickly the woods are laid on at the moment
	float FieldsShown = -1.0f, HedgesShown = -1.0f; // how strongly the quilt of fields and its hedges show at the moment
	float FieldsNear = 1.0f, FieldsWorking = 0.8f, HedgesNear = 0.25f; // ("-TheaterFields=near,working,hedges" tries others)
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
	float StandardGrowth = 1.0f, StandardLean = 0.0f; // how large the pieces' standards are drawn just now, and how far they lean back toward the eye
	mutable TArray<float> TownSpreads; // TownSpread for every province, worked out once
	mutable TArray<bool> TownPacked; // true for a city or capital in a province too small for it: it is built tight and tall
	TMap<int32, FVector> TownBeacons; // by province: the lantern of its port's lighthouse, on the table
	TSet<int32> AirfieldsUsed; // the provinces whose airfields have air groups based on them
	bool bAirfieldsKnown = false; // the rules say which airfields are in use (the grand campaign): an empty one is then built smaller
	TArray<FIntVector4> LastWorks; // what the provinces' works were last built from
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LanesInk; // the shipping lanes and trade routes: dimmed with the land by night
	bool bFacingSet = false;
};
