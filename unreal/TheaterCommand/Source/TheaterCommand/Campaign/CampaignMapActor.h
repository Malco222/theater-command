#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include <atomic>
#include "GameFramework/Actor.h"
#include "Campaign/CampaignMapData.h"
#include "Campaign/CampaignGround.h"
#include "Campaign/CampaignTerrain.h"
#include "Campaign/CampaignView.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignTextures.h"
#include "Campaign/PortLife.h"
#include "Campaign/StreetLife.h"
#include "Effects/BattleEffects.h"
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
struct FSeasonTurnTable; // (release 2, the season turns: Campaign/CampaignSeasonTurn.cpp)
struct FFrontChurnTable; // (release 2, churned ground between dug-in lines: Campaign/CampaignFrontChurn.cpp)
struct FFrontChurnCell;
struct FFrontChurnTile;
struct FFrontChurnVeilWork;
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

	/** Waits for any sheet of ground still being built on another thread: it reads this actor's map. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
		int32 Sea = INDEX_NONE; // (which sea, for a sea's name)
		bool bRegion = false; // a province's own name, small, written whole where the province's middle is
		bool bOverRoofs = false; // a province's name with no open ground or water of its own to lie on: it lies over its town's roofs, a little smaller, with a soft dark shadow
	};
	const TArray<FMapWord>& GetMapWords() const { return MapWords; }
	FVector2D WorldToMap(const FVector& World) const;

	/** A point on the surface of the table: on the ground over land, on the water over sea. */
	FVector SurfacePoint(double X, double Y, double Lift = 0.0) const;

	/** The same, but clear of every bump within a short reach: for lines drawn over rough ground, which a ridge would otherwise cut. */
	FVector SurfacePointOver(double X, double Y, double Lift) const;

	/**
	 * (Speed: tc.Speed.KeptHeights.) SurfacePointOver, kept: for the lines laid again and again on the same ground (the
	 * frontiers, the front line, the enemy's reach), each point is worked out once and kept, the very same answer,
	 * until a fine sheet of the world stands up or is taken away over it (the only thing that changes the ground's height).
	 */
	FVector SurfacePointKept(double X, double Y, double Lift) const;

	int32 RegionAtWorld(const FVector& World) const;

	void SetSelectedRegion(int32 RegionId);
	int32 GetSelectedRegion() const { return SelectedRegion; }
	void SetHoveredRegion(int32 RegionId);

	/** Says who holds each region now, and repaints the table if ground has changed hands. */
	/**
	 * The whole-world map: is fine ground the view wants still being modelled? It is built on another thread, two
	 * sheets at a time, and until it stands the coarse sheet shows in its place, with no lesser river, lake or
	 * clearing on it. (A picture of the world waits for it, and for the haze beyond the map while it is being laid
	 * on another thread, tc.Speed.FogAside.)
	 */
	bool AreFineSheetsComing() const { return bFineSheetsComing || bLandTilesComing || bSquaresComing || FogAsideBuilding.IsValid(); }
	/** Whether the next change of holders is one month following another (ink spreads, a siege's colour moves) or the table laid out afresh (it does not). */
	void SetInkAllowed(bool bAllowed) { bInkAllowed = bAllowed; }
	// ---- (release 2, the front you can read) ----
	/** The player has cut the month's replay short: ink still running is finished at once, and ground that changes hands in this same frame takes its new colour at once, with no ink. */
	void SkipInk();
	/** The month's replay: ink begun in this frame is run in no longer than the step is shown (Seconds), so that the next step's ground never cuts it short (TheaterFront::InkFitted). */
	void FitInk(double Seconds);
	/** While an order's result card is up over the table, the ink it began stands at its start; put away, the ink's second begins (the art director's ruling of 22 November, after run 4). */
	void HoldInk(bool bHold);
	// ---- (end release 2, the front you can read) ----
	void SetHolders(const TArray<FString>& Holders, const TArray<FString>& League, const TArray<FString>& Owners = TArray<FString>(), const TArray<FCampaignView::FSiegePaint>& Sieges = TArray<FCampaignView::FSiegePaint>());

	/**
	 * Stands the armies on the table as the rules say they are. Marches are corps that have just moved:
	 * their pieces are picked up and carried along the road they took.
	 */
	void ShowArmies(const FCampaignView& View, const TArray<FCorpsMove>& Marches, const FVector* CarriedFrom = nullptr);
	/** (tc.Speed.ModelWarm) Queues the model files this view's nations could ask for, to be read on a thread of their own. */
	void WarmModels(const FCampaignView& View);

	/** The piece a line from Origin along Direction passes through, nearest first. */
	AArmyPiece* PieceAtRay(const FVector& Origin, const FVector& Direction, double* OutDistance = nullptr, const AArmyPiece* PassOver = nullptr) const;

	// ---- (Ruling 127; the art director's weather-sky brief.) The weather's sky.
	/** A weather zone and its sky this month: 0 clear, 1 in cloud, 2 closed. */
	struct FSkyZone
	{
		FString Name;
		int32 Sky = 0;
		TArray<int32> Regions;
	};
	/**
	 * Lays the month's skies on the table. Under cloud, soft shadows drift over the ground; under a closed sky the
	 * zone sits under a dim lid with mist and rain. With no zones, no weather is known, and the drifting shadows of
	 * old stay as they were.
	 */
	void ShowSky(const TArray<FSkyZone>& Zones);
	/** The sky view: on with an air group in hand, the planner open or a raid card up, and off with them. It comes and goes over a third of a second. */
	void SetSkyView(bool bOn) { bSkyViewWanted = bOn; }
	const TArray<FSkyZone>& GetSkyZones() const { return SkyZones; }
	float GetSkyView() const { return bSkyKnown ? SkyViewNow : 0.0f; }
	/** A zone's plate, for the sky view: its name, its sky, and places on its ground (the plate stands among those of them that are in sight). */
	struct FSkyPlate
	{
		FString Name;
		int32 Sky = 0;
		TArray<FVector> Grounds;
	};
	const TArray<FSkyPlate>& GetSkyPlates() const { return SkyPlates; }

	// ---- (Ruling 153.) The player's agents, standing on the map as pieces that he alone sees.
	/** Stands his agents in their provinces, and a mark where one was caught this month. Remade only when they change. Call it after ShowArmies: an agent stands clear of the armies' bases. */
	void ShowAgents(const FCampaignView& View);
	/** The agent whose piece (from high up, whose sign) a line from the eye passes through: his id, and how far along the line. INDEX_NONE if none. */
	int32 AgentAtRay(const FVector& Origin, const FVector& Direction, bool* bOutOnBody = nullptr) const;
	/** The agent in hand, by his id (INDEX_NONE: none): his piece is lifted a little off the table and a brass ring is lit on its base. */
	void SetAgentInHand(int32 AgentId);
	/** The agent the pointer rests on: the ring on his base lights, to say he can be picked up. */
	void SetAgentHovered(int32 AgentId) { AgentHovered = AgentId; }
	/**
	 * (Release 2, the spies thread of 24 November 2026.) An enemy agent his hunters uncovered stands with his own, and
	 * AgentAtRay gives its piece a number of its own, below any agent's: FoeStand(Id) for the enemy agent Id, which
	 * IsFoeStand tells apart and FoeOfStand turns back. It is never picked up: a press on it opens the secret service.
	 */
	static int32 FoeStand(int32 FoeId) { return -1000 - FoeId; }
	static bool IsFoeStand(int32 StandId) { return StandId <= -1000; }
	static int32 FoeOfStand(int32 StandId) { return -1000 - StandId; }
	/** Carries the agent in hand over this point of the table; and sets him back where he stands. */
	void CarryAgent(const FVector& GroundPoint);
	void ReturnAgent();
	/** (Ruling 163.) With an agent in hand: the province under the pointer and its one ring are edged as the ground he would watch from there. INDEX_NONE takes the edge away. */
	void SetWatchEdge(int32 RegionId);
	/** Where the Nth of the agents' pieces stands on the table, in the map's own measure: for the photographer's eye. */
	bool GetAgentSpot(int32 Index, FVector2D& OutSpot) const;
	/** The agent in hand: where the foot of his base is just now, and how tall he stands. False with none in hand, or from so high that he is a sign. */
	bool GetAgentInHandStand(FVector& OutFoot, double& OutHigh) const;

	/** The piece that holds this corps, if it is on the table. */
	AArmyPiece* FindPieceOfCorps(int32 CorpsId) const;

	/** The given nation's piece in a region, if it has one there. */
	AArmyPiece* FindPiece(int32 RegionId, const FString& Nation) const;
	/** (Rules 84.3.3) An air group's piece, by the group's id; none where it is not on the table. */
	AArmyPiece* FindAirPiece(int32 WingId) const;

	/**
	 * (Rules 84.3.3, FIRST COMMAND; the art director's first-hour brief, section 3.) The staff note's pointer on the map:
	 * four brass corner clamps drawn on the ground, under every piece, flag and plate, so that where an arm would cross
	 * one it passes under it. Given as quads already laid on the ground (four corners each, in order round the quad),
	 * with an ink for each, the dark case first and the brass over it. Empty takes it away.
	 */
	void ShowPointer(const TArray<FVector>& Corners, const TArray<FColor>& Inks);

	/**
	 * Tints the regions a picked-up army could go to: its own colour for a march, red for an attack. An empty list clears it.
	 * The enemy's reach goes with it, unless bKeepEnemyReach: then the enemy's wash and its dashed line stand until the
	 * ShowEnemyReach that follows replaces them (the controller's orders laid out over frames).
	 */
	void SetReach(const TArray<FReachRegion>& Reach, bool bByRail = false, bool bKeepEnemyReach = false);
	/**
	 * (release 1) With an army in hand: the regions the enemy the player can see could strike next month. They take a
	 * faint wash, and a dashed red line is drawn where that ground ends inside the player's own country (the front
	 * itself is the front line's to draw). SetReach clears it: call this after it.
	 */
	void ShowEnemyReach(const TArray<int32>& Danger);
	void FindSharedEdges();
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
	/** (tc.Speed.SidesKept) Lays the sides view out of sight as the war is laid out, so that the first press shows it at once. */
	void WarmSides(const TArray<FString>& Standing);
	bool AreSidesShown() const { return bSides; }
	/** How far into the night the room is, 0 to 1: the windows of the towns are lit as it comes on. */
	void SetNight(float Night, bool bLamplight = false);
	/** The mud of the eastern spring and autumn: the ground it lies on is washed brown. None clears it. */
	void ShowMud(const TArray<int32>& Regions);
	/** The fog of war: ground the player has no eyes on lies under a shadow, on the table and on the small map. */
	void ShowShroud(const TArray<int32>& Unseen, const TArray<int32>& Glimpsed = TArray<int32>());
	/** Rings in gold the capitals of fallen countries that the player would set free by taking them. */
	void ShowFreeCapitals(const TArray<int32>& Capitals);
	/** Rings the seas a fleet in hand can sail to, the one the pointer is over more brightly, and the fleet itself. None clears them. */
	void ShowSeaReach(const TArray<int32>& Zones, int32 Hover, int32 FleetId);

	/** Draws the road an army would take, from where it stands through the regions given. An empty list clears it. */
	/** RouteEnds: 0 a march, 1 a battle at the end of it, 2 a frontier that means war. */
	void SetRoute(const FVector& FromWorld, const TArray<int32>& Regions, int32 RouteEnds = 0);
	// ---- (release 2, the enemy's month near you; Brian, 24 November 2026)
	/** Lays the arrows of one step of the others' month, each grown to Grow (0 to 1), as Campaign/EnemyMoves draws them. An empty list clears them. */
	void ShowStepArrows(const TArray<FStepArrow>& Arrows, float Grow);
	/** Where to look to see these arrows whole: the middle of their ends, and how far the farthest lies from it (map units). False where there are none. */
	bool FrameStepArrows(const TArray<FStepArrow>& Arrows, FVector2D& OutMiddle, double& OutReach) const;
	// ---- (end release 2)

	/** Called as the camera moves: decides which lettering is worth showing and turns the standards to face the viewer. */
	void UpdateForCamera(float CameraDistance, float CameraYaw, const FVector& CameraFocus);

	/** One shared material per colour, so a hundred pieces of one nation cost one material. */
	UMaterialInstanceDynamic* GetSolidMaterial(const FLinearColor& Color, float Roughness, float Metallic, float Glow = 0.0f);

	/** A nation's flag of the period, as a picture. Painted the first time it is asked for. */
	UTexture2D* GetFlagTexture(const FString& Tag);

	/** The cloth a nation's standards are cut from: its flag, readable from either side. Null if the flag material is missing. */
	/** A nation's flag for the cloth of a standard; Wear 1 is the faded flag of a battered army, 2 of one nearly spent. */
	UMaterialInterface* GetFlagMaterial(const FString& Tag, int32 Wear = 0, bool bGlimpsed = false);

	/** The paint the pieces are moulded in: each point of a piece carries its own colour. */
	UMaterialInterface* GetPieceMaterial() const { return PieceMaterial; }
	/** The foam of a ship's wake: see-through, breaking into streaks that drift astern, and dimmed with the sea by night. */
	UMaterialInterface* GetWakeMaterial() const { return WakeInk; }

	/** Half the size of the map in world units, for keeping the camera over the table. */
	FVector2D GetWorldHalfExtent() const;

	/** World centimetres per map unit. Ten makes an army piece about the height of a person, which is the scale the engine's lighting is tuned for. */
	UPROPERTY(EditAnywhere, Category = "Campaign")
	float UnitScale = 10.0f;

	/** Extra vertical exaggeration of the relief. */
	UPROPERTY(EditAnywhere, Category = "Campaign")
	float VerticalScale = 1.0f;

	const TArray<TObjectPtr<AArmyPiece>>& GetPieces() const { return Pieces; }
	const TMap<int32, TObjectPtr<AArmyPiece>>& GetFleetPieces() const { return FleetPieces; }
	/**
	 * (Release 2; Brian 24 November 2026 18:13Z: "when i move a navy I want to see the boats sail to it as well".) Whether
	 * a fleet that has changed its sea, or come in or gone out of harbour, sails there along a way by water (OPTIONS,
	 * "Ships sail on the map"); off, it is simply found where it now is. ForgetFleetPlaces drops where each fleet was,
	 * so that the fleets of a war just opened are laid out, not sailed from the last war's places.
	 */
	void SetShipsSail(bool bSail) { bShipsSail = bSail; }
	bool GetShipsSail() const { return bShipsSail; }
	void ForgetFleetPlaces() { FleetPlaces.Reset(); FleetSpots.Reset(); ForgetWakes(); }
	/**
	 * (Release 2: fading wakes; Brian 24 November 2026 18:24Z, "i like these".) An enemy fleet seen to move leaves a
	 * faint trail of foam on the sea, the way it went by water, for a month: brightest where it is now and fading back
	 * to where it came from, fainter again the month after, then gone. OPTIONS "Fading wakes" (the options key "wakes").
	 * Off, none is laid. A fleet only reported, not seen, leaves none; nor do the player's own fleets or his allies'.
	 */
	void SetWakes(bool bOn);
	bool GetWakes() const { return bWakes; }
	void ForgetWakes();
	/**
	 * A way by water from one point of the map to another, as points from the one to the other: round the land, a
	 * little off the coasts, and straight where the water is open. Empty where none is found in the box round the
	 * two (a strait thinner than the search's squares, say): the fleet is then simply found at its new place, as it
	 * was before ships sailed, and never sails over the land. (Release 2, the far side: the short way round the
	 * world. The way is given in the turn it leaves from, so over the chart's seam its last point lies a whole world
	 * from To: a sailing piece comes round by itself, see RoundPiece, and a wake is laid in both turns.)
	 */
	TArray<FVector2D> WaterWay(const FVector2D& From, const FVector2D& To) const;
	/**
	 * (Release 2, the far side.) The whole world's chart goes round, and what is laid once on it (a town, a plate, a
	 * piece) is laid in the chart's own turn of the world, from its seam at 141 W. LaidX brings a place from west to
	 * east into that turn; NearX brings it into the turn nearest another place, which is the short way round between
	 * the two (San Diego to Pearl Harbor is 1,066 units west across the seam, not 8,190 east across two oceans).
	 * Both give the place back as it was on a table with edges. RoundPiece tells a piece its turn (AArmyPiece::KeepRound).
	 */
	double LaidX(double X) const;
	double NearX(double X, double BesideX) const;
	void RoundPiece(AArmyPiece* Piece) const;
	/**
	 * (Seam-4.) Where a thing that moves or changes each frame is shown: a place in the world's own measure, brought
	 * into the turn of the world nearest what the eye looks at while both sides of the chart's seam are drawn
	 * (OverSeam), so that a ship going down a little way over the seam is seen from this side of it. Otherwise the
	 * place as it was given. (The same rule, each in its own measure: a raid, a train and a field's parked aircraft
	 * count their place by NearX from the eye's; smoke and fire are brought round at the top of UpdateSmoke. What
	 * stands still has its twin, LayOverSeam; the lights of the night have KeepLightsTwin; a piece has LookFrom.)
	 */
	FVector BesideEye(const FVector& Place) const;
	/**
	 * (Release 2, the far side: seam-2.) Whether what lies within sight of the chart's seam is shown from both sides of
	 * it: the far side on, and tc.Far.OverSeam not 0. Then the screen counts its names, plates and badges in the turn of
	 * the world nearest the eye (TheaterSpeed::RoundWorld).
	 */
	bool OverSeam() const;
	/**
	 * (Seam-2.) The eye has come in again at the other end of the world and now looks at Focus: the pieces and the
	 * screen's names are counted from there at once (the camera's CameRound), not from the frame after.
	 */
	void EyeCameRound(const FVector& Focus);
	/**
	 * (Seam-2.) A builder says that it has laid this model of the table again. The twins' keeper looks at a model by
	 * its counts and a few of its points, and a model may change and keep all of those (a gun pit moved a few degrees,
	 * an airfield drawn smaller): this is what tells it. FCampaignMeshBuffers::Commit says it for every model it
	 * fills; a builder that fills a model with CreateMeshSection itself says it after. It costs a look through the
	 * twins' short list, and does nothing for a model that has no twin.
	 */
	void SeamLaidAgain(const UProceduralMeshComponent* Of);

private:
	/**
	 * (Seam-2, third part.) What is laid once on the table in one great model (the borders, the railways, the rivers,
	 * the works, the washes of an army in hand) ends at the chart's seam, and from the other side of the seam it was
	 * not there. LayOverSeam lays the share of a model that lies within the high view's sight of the seam a second
	 * time, a whole world away, on a twin of the model that is kept beside it: so the west end's things stand beyond
	 * the east end too, and the east end's beyond the west. KeepSeamTwins, each frame, looks at each such model by a
	 * few of its points, and by the count of times its builder has laid it again (SeamLaidAgain), and lays its twin
	 * again when the model has changed, and keeps the twin shown, hidden and placed as its model is.
	 */
	void LayOverSeam(UProceduralMeshComponent* Of);
	void KeepSeamTwins();
	/**
	 * (And what is laid in squares of country, the woods and the towns, is small enough to be moved whole: each square
	 * is shown in the turn of the world nearest the eye, as a piece is.)
	 */
	void SquaresRound(const FVector& Focus);
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> SeamLaid; // the models looked at, as they were first seen
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> SeamTwins; // each one's twin, once it has anything to lay (else none)
	TArray<uint32> SeamStamps; // and what each looked like when its twin was last laid
	TArray<uint32> SeamCounts; // and how many times each has been laid again since it was first looked at (SeamLaidAgain)

public:
	/**
	 * A province crowded with the flags of one side (two armies or more) shows one flag, the player's own if it is there, with a count
	 * of the others beside it; pointed at or picked, it fans them all out. This gives, for each province that is
	 * folded up, the piece that is shown and how many are put away behind it.
	 */
	void GetCrowds(TArray<TPair<AArmyPiece*, int32>>& Out) const;

	/**
	 * (Release 2: camps you can see. Brian, 22 November 2026; the art director's paintover.) Lays the camp of every army
	 * that stays put on the ground round its base, by the stage its fieldwork months have reached (see
	 * AArmyPiece::GetCampStage): tents and a vehicle park on trodden ground; then part of a trench toward the enemy;
	 * then the whole trench, a belt of wire before it, gun pits behind it and the vehicles under nets. Each part keeps
	 * to open ground of the army's own side: off water, towns, airfields, rivers, railways, woods and other armies'
	 * bases and guns. A camp is laid again only when what it is laid from has changed (CampaignCamps.cpp).
	 */
	void BuildCamps();

	/** The models set out for inspection ("-TheaterParade=Kiev"): whether there are any, and where one stands on the map (or the middle of them all). */
	bool HasParade() const { return ParadeSpots.Num() > 0; }
	FVector2D ParadeSpot(int32 Index) const;
	/**
	 * (The fog of war as a haze.) Softens a picture of unseen ground at its edge: the haze starts a little outside an
	 * unseen province's border, stands at about a third on it, and is whole some three and a half map units inside.
	 */
	static void SoftenFog(TArray<FColor>& Pixels, int32 NX, int32 NY, double Cell);
	/** (For the photographer.) Where a train now runs in the open, a little ahead of it; one made up under guns of at least that level. */
	bool TrainSpot(int32 MinFlak, FVector2D& Out) const;
	/** (For the photographer.) The next look at a train chooses its train afresh. */
	void ForgetTrain() { WatchedTrain.Reset(); }
	mutable TWeakObjectPtr<UProceduralMeshComponent> WatchedTrain;
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
	/**
	 * (Brian, 24 November 2026: "its hard to click on an airfield"; the art director's spec, item 1.) Every airfield
	 * within Reach map units of a point, with its spot: for the pointer, which then measures each on the screen.
	 */
	void AirfieldsWithin(const FVector2D& MapPoint, double Reach, TArray<TPair<int32, FVector2D>>& Out) const;
	/**
	 * (The same spec, item 2.) The four corners on the table of the ground an airfield covers, its grass and a little
	 * round it, in order round the field: for the brass outline the pointer lays round it. False where it has none.
	 */
	bool GetAirfieldOutline(int32 RegionId, TArray<FVector>& OutCorners) const;

	/** Draws the shipping lanes over the sea: pale where the cargoes get through, amber where raiders are at them, red where the lane is cut. */
	void ShowLanes(const TArray<FViewLane>& Lanes);

	/**
	 * (Release 2, columns.ts) The month's supply columns: the road each took from the railhead to the armies beyond it,
	 * a few convoys of lorries crawling along it, and a burnt-out lorry with its fire wherever enemy aircraft or
	 * partisans caught it. Nothing while the columns are off. (CampaignColumns.cpp) (Release 2, logistics.ts) Where
	 * the wagons are on, horse-drawn wagons among the convoys; where the dumps are, their stacks of crates.
	 */
	void ShowColumns(const TArray<FViewColumn>& Columns, const TArray<FViewDump>& Dumps);

	/**
	 * A picture of one of the game's own models, for a unit's card: taken once for each model and kept. The name is
	 * a vehicle's, an aircraft's or a ship's ("PanzerIV", "B17", "Battleship:UK"), or a figure's ("Soldier:GER",
	 * "Rider:POL", "Pioneer:GER").
	 */
	UTextureRenderTarget2D* GetPortrait(const FString& Model, bool bLarge = false); // (the large one is for the bigger unit card)
	/** (Release 2, tc.Speed.PicturesAside) Whether GetPortrait has been asked for this one already (it answers at once). */
	bool HasPortrait(const FString& Model, bool bLarge = false) const { return (bLarge ? LargePortraits : Portraits).Contains(Model); }

	/** The model that stands for a kind of corps of a nation, as it is this year: its tank, its gun, its soldier. */
	static FString ModelFor(const FString& Nation, const FString& Kind);

	/** The model shown large and slowly turning beside the cards; empty for none. */
	void SetShowpiece(const FString& Model);
	UTextureRenderTarget2D* GetShowpiece() const { return ShowTarget; }
	const FString& GetShowpieceModel() const { return ShowModel; }

	/** Aircraft take off from their airfield, fly to the target and come home, over some seconds. */
	void FlySortie(const FSortie& Sortie);
	/** For a picture: every raid in the air is held at this share of its flight (0 to 1). Less than nothing lets them fly on. */
	void HoldRaids(double Share) { RaidHeldAt = Share; }
	/** Where the first raid in the air is, on the map, and how high above the table. False when none is flying. */
	bool FirstFlight(FVector2D& OutSpot, float& OutHeight) const;
	/** Where the first raid in the air is bound, and how high it flies: for a picture framed on the place, not on the bombers. */
	bool FirstRaidTarget(FVector2D& OutSpot, float& OutHeight) const;

	/** Smoke and gun flashes over a region where a battle has just been fought, for a few seconds. */
	void AddBattleSmoke(int32 RegionId);
	/** The same over a sea where fleets have fought. */
	void AddSeaBattle(int32 Zone);

	/** Shows which bridges are down and which railways are cut. Does nothing if nothing has changed. */
	void ShowWorks(const TArray<int32>& BridgesDown, const TArray<int32>& RailsCut);
	/**
	 * (Rulings 112 to 114; the art director's railway brief, section 7.) The lamps of the cuts the player knows of: a
	 * red disc with a broken rail across it where he sees the cut, the same sign hollow where he knows it from a
	 * report or as it was when last seen.
	 */
	void ShowCuts(const TArray<FViewCut>& Cuts);
	/** (The art director's railway brief, sections 3 to 6.) The lit railway of the army in hand; an empty one puts it out. */
	void SetRailLight(const FRailLight& Light);
	/** The province under the pointer that the army could ride to: its route turns white. INDEX_NONE for none. */
	void SetRailRoute(int32 ToRegion);
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
	/**
	 * (release 1) The admiral's chart: with it on, a sea the player's side has not searched lies under a veil, one
	 * thinly searched under a lighter one, and a navigator's mark stands where enemy ships were last seen, fainter
	 * for each month since. Levels are by sea: 0 not searched, 1 thinly, 2 well, 3 in full view.
	 */
	void ShowChart(bool bOn, const TArray<int32>& Levels, const TArray<FOldSighting>& Old);
	bool IsChartShown() const { return bChartOn; }
	/** How well a sea is searched as the chart shows it now (0 not, 1 thinly, 2 well, 3 in full view): 3 for every sea while the chart is off. */
	int32 ChartLevelOf(int32 Sea) const { return bChartOn && ChartLevels.IsValidIndex(Sea) ? ChartLevels[Sea] : 3; }
	/**
	 * (release 1) A ship goes down where fleets fought: a model of her class in her navy's paint heels over, settles
	 * by the stern and slides under, in about six seconds. Nth sets her a little apart from the others of the fight.
	 */
	void AddSinking(int32 Zone, const FString& Nation, const FString& Class, int32 Nth);
	/** (release 1) The wrecks on the chart: a small chart-maker's wreck sign in each sea where a big ship went down. */
	void ShowWrecks(const TArray<FWreckMark>& Wrecks, int32 Turn);
	/** (release 1) The bare ground: lifts every piece off the table (armies, fleets, air groups) while it is not shown, and sets them back. */
	void SetPiecesShown(bool bShown);
	/**
	 * (air battle) While a battle's scene is up, every piece stays off the table whatever the hover asks; cleared, the
	 * pieces stand again and the crowds fold up.
	 */
	void SetPutAway(bool bAway)
	{
		if (bAwayForBattle == bAway)
		{
			return;
		}
		bAwayForBattle = bAway;
		if (!bAway)
		{
			const bool bWere = bPiecesShown;
			bPiecesShown = false;
			SetPiecesShown(bWere);
		}
	}
	bool IsPutAway() const { return bAwayForBattle; }
	/**
	 * (release 2, the land battle's gate) While the land battle's scene is up the hidden table's per-frame update
	 * (UpdateForCamera: the lettering, the standards, the smoke) is held: 2.3 ms on every long frame and up to 15 ms
	 * (the production lead's lit base, 22 Nov). Released, it runs at once with the last view asked, so the table comes
	 * back as it would have been.
	 */
	void HoldUpdates(bool bHold)
	{
		if (bUpdatesHeld == bHold)
		{
			return;
		}
		bUpdatesHeld = bHold;
		if (!bHold && bViewAsked)
		{
			UpdateForCamera(LastAskedDistance, LastAskedYaw, LastAskedFocus);
		}
	}
	/** (release 1) A warm lamp over the army or fleet in hand: it follows the piece, and goes out when the piece is put down. */
	void SetHandLamp(AArmyPiece* Piece);
	UMaterialInterface* WashInk();
	AArmyPiece* FindFleetPiece(int32 FleetId) const;
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
	 * (Brian's ask of 16 November 2026; the art director's brief.) Black smoke stands over every place a raid has
	 * damaged this month or last: pairs, the region and what the raids on it were worth (three columns for a place
	 * hit hard, one otherwise).
	 */
	void ShowBombed(const TArray<int32>& Pairs);
	/** (Version 64) Fieldworks standing empty: three numbers each, the region, tenths of a month of work, room for how many corps. A small broken bank where an army would stand. */
	void ShowWorksLeft(const TArray<int32>& FromRules);
	/** Factories that are not working (bombed, sabotaged, burnt): pairs, the region and how many. Their works stand as wrecks and their chimneys are cold. */
	void ShowWrecked(const TArray<int32>& FromRules);
	/** Provinces with factories and nothing on order: their chimneys stand cold, the town's own and those of works built since. */
	void ShowIdle(const TArray<int32>& FromRules);
	/** Works going up from the arsenal: pairs, the region and what. A builder's yard in scaffolding stands there meanwhile. Call it before ShowBuildings. */
	void ShowWorksGoing(const TArray<int32>& FromRules);
	/**
	 * The drawn front line (release 1): the wars as pairs of tags, and whom the player leads. A crayon line is laid
	 * along every frontier between the holdings of two countries at war: bold, with barbs toward the enemy, where
	 * it is the player's war; thin and plain where others fight. It is redrawn whenever ground changes hands.
	 */
	void ShowFrontLine(const TArray<FString>& Wars, const FString& Player);
	void BuildFrontLine();
	/** Flies the holder's flag from the great building of every capital that is not in its own country's hands. */
	void ShowVictorFlags();
	void DrawVictorFlags();
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
	/**
	 * (Release 2, fronts that feel alive, piece 1: churned ground between dug-in lines; tc.Map.FrontChurn, off as it
	 * comes.) The fronts the view shows and the corps on them: which pairs of provinces have both sides dug in and for
	 * how long, and which heal. Nothing is laid here: the frames after lay what has changed. Nothing at all while off.
	 */
	void ShowFrontChurn(const FCampaignView& View);
	/** How far the table is into a moonlit night, 0 to 1 (nothing by day, and nothing by lamplight). */
	float GetMoonlight() const { return bLamplightNow ? 0.0f : NightNow; }
	/** How far the table has gone over to the staff's paper map: nothing until the eye is high, all of it at the top of the zoom (art fix 11). */
	float GetPaper() const { return PaperShown < 0.0f ? 0.0f : PaperShown; }
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
	/** One sheet of land: a grid of heights with its own paint, drawn as tiles that are finer near the eye. */
	struct FLandSheet
	{
		const FCampaignTerrain* Shape = nullptr; // its heights
		UMaterialInterface* Material = nullptr; // its paint
		TMap<uint64, UProceduralMeshComponent*> Tiles; // the tiles standing on the table now
		TArray<TArray<float>> Heights; // the heights at each level: level 1 has every second point of level 0, and so on
		TArray<TArray<float>> Cover; // the most land anywhere near each of those points, for telling which tiles are all sea
		TArray<FIntPoint> Sizes; // how many points each level has, across and down
		int32 TopLevel = 0;
		TArray<FBox2D> Holes; // ground that a finer sheet draws is left out of this one: these boxes of it, in map units
		double Sink = 0.0; // how far it lies below its true height round those boxes, in map units, so that it never fights the finer sheet where they meet
		bool bKeep = false; // a sheet built with an apron round it draws only...
		FBox2D Keep = FBox2D(ForceInit); // ...this box of its ground
		bool bHidden = false; // its tiles are stood up out of sight, until all of them are there
		// In a world that runs right round, ground is drawn a turn of the world east or west of where it is kept,
		// whichever is nearest the eye: each tile by itself (1), or the whole sheet together by Turn (2). 0: it stays put.
		int32 WrapMode = 0;
		int32 Turn = 0;
		/** How far inside a hole a point lies, in map units from the hole's nearest edge: nought outside every hole. */
		double DeepInHole(double X, double Y) const
		{
			double Deep = 0.0;
			for (const FBox2D& Hole : Holes)
			{
				Deep = FMath::Max(Deep, FMath::Min(FMath::Min(X - Hole.Min.X, Hole.Max.X - X), FMath::Min(Y - Hole.Min.Y, Hole.Max.Y - Y)));
			}
			return Deep;
		}
		/** True where a point lies well inside one of the holes: Margin map units in from its edge. */
		bool InHole(double X, double Y, double Margin) const
		{
			for (const FBox2D& Hole : Holes)
			{
				if (X > Hole.Min.X + Margin && Y > Hole.Min.Y + Margin && X < Hole.Max.X - Margin && Y < Hole.Max.Y - Margin)
				{
					return true;
				}
			}
			return false;
		}
	};
	/**
	 * A fine sheet of the world's ground beyond the map (the whole-world map, steps two and three): a box of it
	 * modelled and painted as finely as Europe is, built on another thread when the eye comes near and low, stood
	 * up in place of that box of the coarse world sheet, and taken down again when the eye has gone.
	 */
	struct FFineSheet
	{
		FIntPoint Key = FIntPoint::ZeroValue;
		FBox2D Box = FBox2D(ForceInit);
		FCampaignTerrain Shape;
		FCampaignGround Paint;
		TFuture<void> Building;
		UTexture2D* TintPicture = nullptr; // (these three are kept for the engine with its other pictures)
		UTexture2D* ReliefPicture = nullptr;
		UTexture2D* FogPicture = nullptr;
		UTexture2D* OccupiedPicture = nullptr;
		std::atomic<bool> bBuilt { false };
		bool bDressed = false, bStanding = false;
		double LastWanted = 0.0;
		FLandSheet Land;
		UMaterialInstanceDynamic* LandPaint = nullptr;
		UMaterialInstanceDynamic* SeaPaint = nullptr;
		TArray<UObject*> Kept; // its pictures and its paints, which FineKeep holds for the engine
		UProceduralMeshComponent* Sea = nullptr;
		TMap<FName, CampaignTextures::FLevels> Ahead; // (speed) its pictures with their smaller copies, made on the thread that modelled it
		TSet<int32> FogFor; // (speed) the haze its fog picture among them was made for
		int32 DressStage = 0; // (speed: tc.Speed.DressSteps) how far DressFineSheet has gone with it
		FBox2D HoldersChanged = FBox2D(ForceInit); // (speed: tc.Speed.HoldersLock) ground that changed hands while it was being modelled, painted again before it is dressed
		// (Speed: tc.Speed.SheetShelf.) Stood down, and kept: its tiles and its water are taken off the table and the
		// coarse sheet lies there again, as when a sheet is dropped, but its heights, its paint, its pictures and its
		// place among the sheets are kept (so the haze, the seasons and ground changing hands still reach it), and it
		// is stood up from them when the eye wants it again. Always dressed, never standing.
		bool bShelved = false;
		double ModelSeconds = 0.0; // (speed log) how long its modelling took, written by the thread that did it before bBuilt
		bool bModelNoted = false; // (speed log) ...and that this has been said
	};
	TMap<FIntPoint, TSharedPtr<FFineSheet, ESPMode::ThreadSafe>> FineSheets;
	/**
	 * (Speed: tc.Speed.FogAside.) The staff's paper over ground beyond the map, laid on another thread when the haze
	 * moves: the world's picture and each dressed fine sheet's, for the haze as it was asked for, then put on the table
	 * one picture a frame.
	 */
	struct FFogAside
	{
		int32 For = 0; // (FogAsked when it was begun)
		TArray<bool> Hidden;
		bool bAnyHidden = false;
		TArray<FColor> World;
		TArray<TPair<TSharedPtr<FFineSheet, ESPMode::ThreadSafe>, TArray<FColor>>> Sheets;
	};
	TSharedPtr<FFogAside, ESPMode::ThreadSafe> FogAside;
	TFuture<void> FogAsideBuilding;
	int32 FogAsked = 0; // (how often the haze beyond has moved)
	int32 FogLaying = INDEX_NONE; // (the next of its pictures to put on the table: 0 the world's, then its sheets; none while it is being laid)
	void LaunchFogAside();
	void UpdateFogAside();
	/** Which provinces beyond the map are under the haze, as HazeSet stands now. */
	void HiddenBeyond(TArray<bool>& OutHidden, bool& bOutAny) const;
	/** One picture of the staff's paper beyond the map (on any thread). */
	static void LayFog(const TArray<bool>& Hidden, bool bAnyHidden, const FCampaignGround& Painted, const FCampaignTerrain& Shape, TArray<FColor>& OutPixels);
	TArray<double> FineXs, FineYs; // the lines the fine sheets are ruled by, west to east and north to south
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> FineKeep;
	void UpdateFineSheets(float CameraDistance, const FVector& CameraFocus);
	bool bFineSheetsComing = false; // the view wants fine ground that does not stand yet
	bool bLandTilesComing = false; // (speed: tc.Speed.TileTime) ...or tiles of the map's own ground or the world's coarse sheet that are not stood up yet
	bool bSquaresComing = false; // (speed: tc.Speed.ShowSpread) ...or squares of towns or trees that are not shown in their new form yet
	bool bRiversBeyondShown = true; // the ribbons of the wider chart's rivers: put away when the eye is high
	bool bInkAllowed = false;
	void DressFineSheet(FFineSheet& Sheet);
	/** (Speed: tc.Speed.FineAside.) The pictures DressFineSheet makes, with their smaller copies, made ahead on the thread that models the sheet. */
	static void PrepareFineSheet(FFineSheet& Sheet, const TSet<int32>& Haze);
	void StandFineSheet(FFineSheet& Sheet);
	void DropFineSheet(FFineSheet& Sheet);
	/** (Speed: tc.Speed.SheetShelf.) Stands a sheet down onto the shelf: off the table as DropFineSheet takes it, with what was modelled for it kept. */
	void ShelveFineSheet(FFineSheet& Sheet);
	/** (Speed: tc.Speed.SheetShelf.) Drops the sheets on the shelf that are over its number, the longest unwanted first; all of them with KeepAtMost 0. */
	void TrimSheetShelf(int32 KeepAtMost);
	/** Takes a sheet's tiles and its water off the table, and gives the ground back to the coarse sheet if it stood. */
	void TakeSheetOffTable(FFineSheet& Sheet);
	/** Stands the coarse world sheet's tiles over a box up afresh, after a hole in it has been made or closed. */
	void RetileWorld(const FBox2D& Changed);
	const FFineSheet* FineSheetAt(double X, double Y) const;
	/** A place from west to east as the world's sheet keeps it, whichever turn of the world it was given in. */
	double WrapX(double X) const;
	/** How many turns of the world east (or, negative, west) of where it is kept a thing at X is drawn, to be nearest the eye. */
	int32 TurnFor(double X, double EyeX) const;
	float SurfNow = 0.45f; // how strong the line at the water's edge is at this hour
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> WorldSeaTwin; // the same sea again, a turn of the world to the side the eye is on
	void PrepareLand();
	void LevelLand(FLandSheet& Sheet);
	/** Returns true while tiles the view wants are still to be stood up. */
	bool UpdateLand(FLandSheet& Sheet, float CameraDistance, const FVector& CameraFocus, bool bAllAtOnce);
	// (Speed: tc.Speed.TileTime.) The tiles stood up this frame over every sheet together, and what they have taken.
	uint64 TileTimeFrame = 0;
	double TileTimeSpent = 0.0; // (seconds)
	UProceduralMeshComponent* BuildLandTile(FLandSheet& Sheet, int32 Level, int32 TileX, int32 TileY);
	bool LandTileHasLand(const FLandSheet& Sheet, int32 Level, int32 TileX, int32 TileY) const;
	/** The whole-world map, step one: the land and sea beyond today's map, on one coarse sheet (only with -TheaterWorld). */
	void BuildWorld();
	/** The height of the ground anywhere in the world, in map units: the map's own where it reaches, the world's sheet beyond. */
	float GroundAt(double X, double Y) const;
	/** How much of the ground at a place is land, anywhere in the world: 0 open sea, 0.5 the shoreline, 1 well inland. */
	float CoverAt(double X, double Y) const;
	/** Ground beyond the map's own box that has changed hands is painted again on the world's sheets (or on Only, alone). */
	void RepaintBeyond(const FBox2D& Changed, FFineSheet* Only = nullptr);
	/** The staff's paper over ground beyond the map's own box that nobody of the player's can see. */
	void FogBeyond();
	// A setting of the land's or the sea's paint goes to every sheet's copy of it.
	void LandScalar(FName Name, float Value);
	void LandVector(FName Name, const FLinearColor& Value);
	void SeaScalar(FName Name, float Value);
	/**
	 * (Brian, 16 November 2026: the sea moves.) Sets the moving sea for the hour and the eye's height: half as strong
	 * by night with no flicker in the shallows and few, cool glints; the chop and then the swell fade as the eye
	 * rises, before their crests come so close that they crawl; and the glints' bright band keeps to the lamp's
	 * side of what the eye looks at.
	 */
	void UpdateSeaMotion(float CameraDistance, const FVector& CameraFocus);
	void SeaVector(FName Name, const FLinearColor& Value);
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
	/** (Release 2, grander great cities.) True while "tc.Map.GrandCities" is on, or the game was started with -TheaterGrandCities (Campaign/CampaignGrandCities.cpp). */
	static bool GrandCitiesWanted();
	/** (And the far side's kits.) True while "tc.Map.GrandCitiesFar" is on, or the game was started with -TheaterGrandCitiesFar. */
	static bool GrandCitiesFarWanted();
	/** What the builder of a town is told of the place: its size, its works, its kit, its quay. (The table's towns and the studio's picture of one are built from the same.) */
	void FillTownSite(const FCampaignRegion& Region, FMiniatureMesh::FTownSite& Site);

	/** For looking closely at the models ("-TheaterParade=Ardennes"): sets them out in a row, large, on a board over the region named. */
	void BuildParade();
	TArray<FVector2D> ParadeSpots;
	float ParadeLevel = 0.0f;
	void BuildTrees(const TSet<FIntPoint>* Only = nullptr);
	/** (The art director's round 38.) Where the gun lines' trodden earth lies: no tree stands within a few units of any of these. */
	TArray<FVector2D> GunClearings;
	TArray<FIntPoint> TreeSquareOf; // which square of country each of the woods' meshes holds
	// (tc.Speed.TreesSpread) the squares still to be planted again for the month's turn of the woods, by their place in
	// TreeSquareOf, and what one square has been costing (ms), so that a frame takes about the budget
	TArray<int32> TreesDue;
	float TreeSquareMs = 0.5f;
	TArray<TArray<FVector2D>> TreeClearingsKept; // where each province's pieces stand, kept clear of trees: laid at a whole planting
	void PlantTreesDue(const FVector& CameraFocus);
	void RebuildHighlight();
	void AddRegionOverlay(int32 RegionId, const FLinearColor& Fill, const FLinearColor& Outline, int32 FirstSection, double HalfLine = 0.42, const FLinearColor& Under = FLinearColor::Transparent, double UnderHalf = 0.0);
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
	TMap<FString, TObjectPtr<UTextureRenderTarget2D>> LargePortraits;
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
		double NextFlak = 0.0; // when the guns under it fire next (nothing until it comes under fire)
		// The guns of a raid that is met: the two fighters' two guns each (0 to 3) and a gunner in each bomber (4 to
		// 6). When each fires its next round, and how many it has fired: every fifth round is a tracer.
		double NextRound[7] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		int32 RoundsFired[7] = { 0, 0, 0, 0, 0, 0, 0 };
		// A burst of flak close by jolts the formation: when, and by how many degrees it rolls and pitches. It settles in half a second.
		double JoltAt = -10.0;
		double JoltRoll = 0.0, JoltPitch = 0.0;
		// (where the two sides were a frame ago, and when: for the speed a round leaves its gun with)
		FVector SeenAt = FVector::ZeroVector, SeenRivalAt = FVector::ZeroVector;
		double SeenWhen = 0.0;
		// (Ruling 107.) By night: each bomber again as a shell of light, shown while a searchlight holds it.
		int32 LitMachines = 0;
		int32 LitShown = INDEX_NONE;
		TObjectPtr<UMaterialInstanceDynamic> LitInk;
	};
	TArray<FFlight> Flights;
	/**
	 * (Brian, 18 November 2026: "i want tracer fire shown as well. 1 out of every 5 bullets with yellow".) A tracer
	 * round in flight over a raid: where it left its gun and when, and how it flies. It flies straight and burns out.
	 */
	struct FTracerRound
	{
		FVector From = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		double Born = 0.0;
	};
	TArray<FTracerRound> TracerRounds;
	void UpdateSorties();
	float SortieSeenFrom = 6000.0f; // (how high the eye stands: the raid's pencilled course is thinned close up)
	/**
	 * (Brian, 16 November 2026; rulings 108 and 109; the art director's brief.) The gun lines: for every army that
	 * holds artillery, its guns on the ground behind its base, on the side away from the enemy, barrels over the base.
	 * Made again whenever the armies or the fronts change, and when an army with guns is picked up or put down.
	 */
	void BuildGunLines();
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> GunLineMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GunEarthInk; // the trodden earth under the guns: let down by night with the ground
	/** Where each line stands, for the pointer: a click on a gun picks up its army. */
	struct FGunLine
	{
		TWeakObjectPtr<AArmyPiece> Piece;
		FVector Middle = FVector::ZeroVector;
		double Radius = 0.0;
	};
	TArray<FGunLine> GunLines;
	TArray<TWeakObjectPtr<AArmyPiece>> GunsAway; // armies with guns that are in the hand or on the road: their lines wait for them
	FString GunLinesBuilt = TEXT("-"); // (what the lines were last made from)
	TArray<FString> GunStanding; // how each province stands to the player, as the armies were last laid out
	TSet<FString> GunFoes; // the countries at war with the player
	/** (Release 2: camps you can see.) The way from a base toward the enemy of the nation it belongs to, on the map: see BuildCamps. */
	FVector2D CampFacing(int32 HomeRegion, const FString& Nation, const FVector2D& FromSpot) const;
	TArray<FString> CampHolders; // who holds each province, as the armies were last laid out
	TArray<FString> CampWars; // every pair of nations at war, two by two, as then
	TArray<FBox2D> CampRiverBoxes; // the box each river runs in, so that a camp looks only at rivers near it
	/**
	 * (Brian, 16 November 2026: "if the air raid is at night have search lights shown from the cities"; ruling 107
	 * (f); the art director's brief.) The searchlights of every place under raid by night: lamps on the ground round
	 * it, by its guns, and their beams, light added over the scene. Some search and some hold a bomber.
	 */
	void UpdateSearchlights(float CameraDistance);
	void UpdateCutLamps(float CameraDistance);

	// (Ruling 127.) The weather's sky.
	TArray<FSkyZone> SkyZones;
	bool bSkyKnown = false, bSkyEverKnown = false, bSkyViewWanted = false, bSkyViewHeld = false, bSkySnowHeld = false, bSkySnowShown = false;
	float SkyViewNow = 0.0f;
	int32 SkyNX = 0, SkyNY = 0;
	FLinearColor SkyFrame = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> SkyPicture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WeatherPicture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WeatherSnowPicture;
	TSet<TWeakObjectPtr<UMaterialInstanceDynamic>> SkyDressed; // the materials that have been handed the sky's pictures
	TArray<TPair<FName, float>> TuneScalars; // ("-TheaterTune=") numbers tried on the table's materials, for a picture
	TArray<TPair<FName, FLinearColor>> TuneVectors;
	void UpdateSky(float CameraDistance, const FVector& CameraFocus);
	TArray<FSkyPlate> SkyPlates;
	TMap<int32, int32> SkyOfRegion; // each province's sky, where it is not clear
	/** A stretch of border where one sky ends: the worse sky's own side of it, and the better sky beyond. */
	struct FSkyRun
	{
		TArray<FVector2D> Points;
		int32 Worse = 0, Better = 0;
		double Side = 1.0; // which hand the better sky lies on, going along the points
		FBox2D Bounds = FBox2D(ForceInit);
	};
	TArray<FSkyRun> SkyRuns;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SkyEdgeMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SkyEdgeInk; // a sign, not light on the ground: its own ink
	float SkyEdgeDrawnAt = -1.0f, SkyEdgeDrawnView = -1.0f;
	FVector SkyEdgeDrawnFocus = FVector::ZeroVector;
	void DrawSkyEdge(float CameraDistance, const FVector& CameraFocus);

	/** (Ruling 153; release 2.) One agent's piece on the table, or the mark left for a month where one was caught: the empty base, with his hat and his folded newspaper on it. */
	struct FAgentStand
	{
		int32 Id = INDEX_NONE, Region = INDEX_NONE;
		bool bMark = false;
		bool bFoe = false; // (release 2) an enemy agent his hunters uncovered: its rim in its country's colour, lit day and night, and no pool
		FLinearColor FoeInk = FLinearColor::White; // (its country's colour)
		FVector Rest = FVector::ZeroVector; // where it stands on the table
		FVector CarriedAt = FVector::ZeroVector; // in the hand and carried: the point of the table it hangs over
		bool bCarried = false, bRing = false, bRimLit = false; // (bRimLit: his base's brass rim, lit by night)
		float Lift = 0.0f; // how far it has risen into the hand, 0 to 1
		FVector SignAt = FVector::ZeroVector; // from high up: the middle of its sign on the table, and the sign's radius there
		double SignRadius = 0.0;
		USceneComponent* Holder = nullptr;
		UProceduralMeshComponent* Body = nullptr;
		UProceduralMeshComponent* Pool = nullptr; // the warm pool on the ground at his feet, laid on the table itself
		UMaterialInstanceDynamic* PoolInk = nullptr;
		float PoolYaw = 0.0f; // which way the eye looked when the pool was last laid: it lies across the eye's view
	};
	TArray<FAgentStand> AgentStands;
	/** What the stands above are made of, kept here from the collector. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> AgentParts;
	FString AgentSignature;
	int32 AgentInHand = INDEX_NONE, AgentHovered = INDEX_NONE;
	bool bAgentFigures = true; // figures from where he plays; from high up a sign for each
	FVector2D AgentSpot(int32 RegionId, int32 Slot, int32 Count) const;
	void UpdateAgents(float CameraDistance, float CameraYaw);
	/** (Ruling 163.) The edge round the ground an agent in hand would watch. A sign, not a light: its own ink, which the night does not touch. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> WatchMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WatchInk;
	int32 WatchRegion = INDEX_NONE;
	float WatchDrawnAt = -1.0f; // the eye's distance the edge was last drawn for: its line is two pixels wide from any height
	void DrawWatchEdge(float CameraDistance);
	float EdgeWidthAt = -1.0f; // the eye's distance the picked and pointed-at provinces' edges were last drawn for: two pixels from any height
	void KeepEdgeWidth(float CameraDistance);
	/**
	 * (The art director's round 38.) Where a cut province's lamp, board and post stand: on the longest stretch of its
	 * own track, halfway between the town's last house and the border, clear of where its army stands.
	 */
	FVector2D CutSpot(int32 RegionId) const;
	void UpdateRailLight(float CameraDistance);
	FRailLight RailLight;
	int32 RailRouteTo = INDEX_NONE;
	TSet<FIntPoint> RailRoute; // the tracks of the route to the province under the pointer
	TMap<int32, int32> RailBack; // by province on a lit line: the next province toward the army in hand
	TMap<int32, double> DotOut; // by province: how far out along that track its station dot now stands (nothing: over its town)
	bool bRailLightDirty = false;
	float RailLightBuiltFor = -1.0f; // (the eye's height the lines were last drawn for: they keep their width on the screen)
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> RailLightMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RailLightInk; // the lit line: light of its own, by day and by night
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RailTintInk; // the faint cool tint of the provinces it can ride to: gone by night
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> CutLampMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CutLampInk; // a sign, not a light: its own ink, which the night does not touch
	/** (Ruling 107(f).) The red glow of a burning town by night: a soft light on the ground, the roofs and the air downwind. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> FireGlowMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FireGlowInk;
	bool bFireGlowAdds = false; // its light is added to what is there (M_Glow); without that material it is laid over, more weakly
	TArray<TPair<int32, bool>> CutLamps; // the province, and whether the sign is the hollow one
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> BeamMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BeamMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FlakLitInk;
	/** A beam as it stands this moment, for whatever it lights: its lamp, its way, how long it is, how far along it reaches the bombers' height, and how strong it is. */
	struct FLitBeam
	{
		FVector Lamp = FVector::ZeroVector;
		FVector Way = FVector::UpVector;
		double Long = 0.0, ToHeight = 0.0, Wings = 0.0;
		float Strength = 0.0f;
	};
	TArray<FLitBeam> LitBeams;
	bool bRaidNightAsked = false;
	TArray<FVector> FlakLitVertices;
	TArray<FVector2D> FlakLitPlaces;
	TArray<FColor> FlakLitColours;
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
		// (Brian, 16 November 2026: the trains turn round.) While it stands at a station being turned: when the turn
		// began, the headings it turns between, the point it turns about where that is its own middle, and how far
		// along its new line it then stands (a train turned about its middle stands a little way out of the town).
		double TurnBegan = -1.0;
		float TurnFrom = 0.0f, TurnTo = 0.0f;
		bool bPivotMiddle = false;
		FVector Pivot = FVector::ZeroVector;
		double StartAlong = 0.0;
		int32 FlakLevel = -1; // (ruling 125) the guns that guard the province it last set out from: its flak wagons are made up there
		TObjectPtr<UProceduralMeshComponent> Mesh;
	};
	TArray<FTrain> Trains;
	double NextTrain = 0.0;
	bool bTrainShadowsCast = true; // ("tc.Speed.TrainShadows", for measuring only)
	void UpdateTrains(const FVector& CameraFocus, float CameraDistance);
	/** (Ruling 125.) Makes a train up for the province it sets out from: a flak wagon at its tail where guns guard the line, a second behind the tender at level 3. */
	void MakeUpTrain(FTrain& Train, int32 FromRegion) const;
	/**
	 * (Release 2, the campaign map's upgrade 4, second half: busy ports. Campaign/PortLife.h has the reckoning and
	 * Campaign/CampaignPortLife.cpp the rest; tc.Map.PortLife, off as it comes.) A small merchant ship or two works
	 * each port near the eye: out along its fairway, round in the offing, back, and lying off the quay between rounds.
	 * Decoration only: no rule reads it and nothing of it is saved.
	 */
	struct FPortRoads
	{
		int32 Region = INDEX_NONE; // the port's province
		int32 Ships = 1; // how many work it: two at a city's or a capital's port
		bool bManned = false; // its ships are on the water now
		bool bFouled = false; // a fleet lies on its fairway: its ships are put away until she has gone
		float Shown = 0.0f; // how much of its ships is on the water, 0 to 1: they grow onto it and off it
		TheaterPortLife::FRoads Laid; // its fairway, as it was found, and the times of once round it
	};
	/** One of the little ships: which port's she is, which of its ships, and her models (the hull, and the foam of her wake). */
	struct FPortCraft
	{
		int32 Roads = INDEX_NONE;
		int32 Place = 0;
		bool bWakeShown = false;
		TObjectPtr<UProceduralMeshComponent> Hull;
		TObjectPtr<UProceduralMeshComponent> Foam;
	};
	TArray<FPortRoads> PortRoads;
	TArray<FPortCraft> PortCraft;
	bool bPortRoadsStale = true; // (the towns were built again: the ports' fairways are found again when next wanted)
	TMap<int32, FVector2D> PortQuaysFound; // the quays the fairways were found from: while the towns' quays stand where these do, the fairways stand
	TArray<int32> PortsToFind; // the ports whose fairways are looked for, the greater first, one a frame
	int32 PortsNext = 0, PortsDry = 0, PortsCrowded = 0; // how far down that list the looking has got; how many had no water, and how many gave way to a greater port
	double PortFleetsLooked = -1.0e9; // when the fleets were last looked for on the fairways
	/** Looks for the next port's fairway (all of them afresh after the towns were built). */
	void FindPortRoads();
	/** Takes a port's ships off the water (every port's, for INDEX_NONE). */
	void ParkPortCraft(int32 Roads);
	void UpdatePortLife(const FVector& CameraFocus, float CameraDistance);
	/**
	 * (Release 2, the campaign map's upgrade 4, second half: street cars. Campaign/StreetLife.h has the reckoning and
	 * Campaign/CampaignStreetLife.cpp the rest; tc.Map.StreetLife, off as it comes.) A street car works each arm of a
	 * city's or a capital's two avenues near the eye: from her stop by the square out to the arm's end and back.
	 * Decoration only: no rule reads it and nothing of it is saved.
	 */
	struct FStreetLine
	{
		int32 Region = INDEX_NONE; // the city's province
		bool bManned = false; // its car is on the street now
		float Shown = 0.0f; // how much of her is there, 0 to 1: she grows onto it and off it
		TheaterStreetLife::FLine Laid; // the line, and its times
		TObjectPtr<UProceduralMeshComponent> Car;
	};
	TArray<FStreetLine> StreetLines;
	TMap<int32, TArray<FVector2D>> TownAvenues; // for each city, two points of the map for each avenue arm a car may work, as its town was last built
	TMap<int32, TArray<FVector2D>> StreetAvenuesLaid; // the arms the lines were laid from: while the towns' arms are these, the lines and their cars stand
	bool bStreetLinesStale = true; // (the towns were built again)
	int32 StreetCarsOut = 0; // how many cars are on the streets now
	/** Takes every street car off the streets. */
	void ParkStreetCars();
	void UpdateStreetLife(const FVector& CameraFocus, float CameraDistance);
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> LanesMesh;
	FString ShownLanes; // what is drawn now, to tell when it has changed
	/** (Release 2, columns.ts) A supply column's road as drawn: a smooth curve on the map, how far along it each point lies, and its convoys. */
	struct FColumnRoad
	{
		FString Nation;
		TArray<FVector2D> Curve;
		TArray<double> Run; // the distance along the road to each point of the curve
		double Length = 0.0;
		int32 Convoys = 1; // how many convoys are seen on it: one for every hundred and fifty lorries or so, four at most
		int32 Wagons = 0; // how many of those are horse-drawn wagons (release 2, logistics.ts), at the back
		bool bManned = false; // its convoys are on the table (only near the eye)
	};
	/** A convoy on a column's road: three lorries nose to tail, built once and driven as one. */
	struct FColumnConvoy
	{
		int32 Road = INDEX_NONE;
		int32 Place = 0; // which of the road's convoys it is
		bool bWagon = false; // horse-drawn, and slower
		TObjectPtr<UProceduralMeshComponent> Mesh;
	};
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> ColumnsMesh; // the roads (section 0, in ink) and the burnt-out lorries (section 1)
	FString ShownColumns; // what is drawn now, to tell when it has changed
	TArray<FColumnRoad> ColumnRoads;
	TArray<FColumnConvoy> ColumnConvoys;
	TArray<FVector4> ColumnFires; // where columns were caught: the place in the world, and how big the fire is (w)
	void UpdateColumns(const FVector& CameraFocus, float CameraDistance);
	void ParkColumnConvoys(int32 Road); // takes a road's convoys off the table
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
	TArray<int32> TownChimneyRegion; // (and the region each of them stands in)
	TSet<int32> IdleIn; // provinces with nothing on order: no smoke from their stacks
	TArray<FVector> TownWindows; // the windows of the towns' houses, each as its place and the way it faces
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> LightsMesh; // those windows lit, at night
	/**
	 * (Seam-4.) The same lights again, a whole world to one side: the windows, the villages' lamps and the towns' glow
	 * of the far end of the chart, seen by night from over the seam. The lights are one model of the whole world
	 * whose light changes with the hour, so it is not laid in shares as the lines and the works are (a share would be
	 * laid again at every change of the hour): its twin is the whole of it, filled from the model itself and moved
	 * whole to the side the eye is on. It is made the first night the eye comes within sight of the seam, and is
	 * hidden, and not filled, whenever the eye is not.
	 */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> LightsTwin;
	bool bLightsTwinStale = true; // (the lights have changed, or been built again, since their twin was filled)
	void KeepLightsTwin(const FVector& Focus);
	void FillLightsTwin();
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
	TArray<FIntPoint> WorksChimneyRegion; // (for each of those: its region, and which of the region's stacks it is)
	TMap<int32, int32> WreckedIn; // by region: how many of its factories are not working
	TMap<int32, int32> GoingIn; // by region: what is being built there (bits: see ShowWorksGoing)
	TMap<int32, FVector> TownLandmarks; // by region: the highest point of a capital's great building, on the table
	struct FSharedEdge
	{
		FVector2D A = FVector2D::ZeroVector, B = FVector2D::ZeroVector;
		FVector2D Side = FVector2D::ZeroVector; // across the edge, out of Here and into There
		int32 Here = INDEX_NONE, There = INDEX_NONE;
	};
	TArray<FSharedEdge> SharedEdges; // every stretch of line two provinces share, found once (see BuildFrontLine)
	bool bSharedEdgesKnown = false;
	TArray<FString> WarPairs; // the wars, as the rules last gave them
	FString WarPlayer;
	TSet<FString> WarSet; // the same, as "AAA|BBB" both ways round
	TSet<FString> WarFoes; // whom the player is at war with
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> FrontLineMesh;
	int32 FrontLineFar = -1; // which weight of the line is shown: 0 the working one, 1 the staff map's (none yet: -1)
	/** (Speed: tc.Speed.FrontWeights.) Lays one weight of the front line (0 or 1) or both (INDEX_NONE); false if there is no front. */
	bool LayFrontLine(int32 Only, bool bOwed);
	// ---- (release 2, the front you can read: Campaign/FrontLine.h) ----
	/** LayFrontLine's line as a map-maker's pen draws it: chained, rounded, dark ink with teeth toward the player's enemy in sections 0 and 1 (the two weights), its soft shadow in 2 and 3. */
	bool LayPenFront(int32 Only, bool bOwed);
	TSharedPtr<struct FPenFront> PenFront; // (what the pen keeps from one laying to the next: the border's pieces, the lines, its work arrays)
	// ---- (end release 2, the front you can read) ----
	int32 FrontLineOwed = INDEX_NONE; // the weight still to be laid, a frame after the other
	uint64 FrontLineOwedFrame = 0;
	float FrontLineSeenFrom = 0.0f; // (the eye's distance at the last look, for which weight is laid first)
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> EnemyReachMesh; // the dashed line of the enemy's reach, in the front line's own ink and its two weights
	bool bPiecesShown = true;
	bool bAwayForBattle = false; // (a name of its own: ApplyCrowds has a local bPutAway)
	bool bUpdatesHeld = false, bViewAsked = false; // (HoldUpdates, and the last view UpdateForCamera was asked)
	float LastAskedDistance = 0.0f, LastAskedYaw = 0.0f;
	FVector LastAskedFocus = FVector::ZeroVector;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> OrdersInk; // the washes laid for an army in hand: dimmed by night, in the chart veil's step
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SeaReachInk; // the brass rings of the seas in a fleet's reach: let down by night, or they shone on the dark sea
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WakeInk; // the foam of every ship's wake
	FVector WakeLook = FVector(1.0, 0.13, 1.0); // its brightness by day, what it is multiplied by under the moon, and its strength ("-TheaterWake=" for a trial)
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SeaFillInk; // the tinted water inside such a ring: one thin wash, with a night of its own
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SightingInk; // the red crayon of a place where the enemy was last seen
	// (the art director's numbers of round 19, each open to "-TheaterSeaRings=" for a trial)
	FVector2D SeaFill = FVector2D(0.05, 0.085); // how strong the wash inside a sea's ring is by day: any sea in reach, and the one under the pointer
	FLinearColor SeaLineNight = FLinearColor(0.15f, 0.15f, 0.155f, 0.85f); // what a ring's line is multiplied by under the moon, and its strength
	FLinearColor SeaFillNight = FLinearColor(0.45f, 0.45f, 0.47f, 1.0f); // and the wash inside it
	FLinearColor SightingNight = FLinearColor(0.5f, 0.5f, 0.52f, 1.0f); // and a last-seen ring
	FLinearColor SnowWoodFloor = FLinearColor(0.5f, 0.54f, 0.6f); // what a wood's floor is multiplied by under the snow: a cool grey, about a tenth below the snow round it
	FLinearColor SnowOnTrees = FLinearColor(0.205f, 0.25f, 0.235f); // what snow lying on a tree takes its colour toward: a cool grey-green
	void TakeStudioPicture(USceneCaptureComponent2D* Camera, bool bAircraft = false);
	FVector StagedExtent = FVector(100.0); // half the size of the model last stood on a stage, each way
	TArray<FVector> StagedPoints; // the points of that model, from its middle: an aircraft's picture is fitted to them
	/** Stands the studio's camera where the model last staged fills this share of the picture, its middle in the middle. */
	void FitStudioCamera(USceneCaptureComponent2D* Camera, const FVector& Middle, double HighOverWide, double Fill, const FVector& From);
	bool bShowAircraft = false; // the model turning in the tray is an aircraft
	double RaidHeldAt = -1.0;
	FLinearColor WashNight = FLinearColor(0.36f, 0.38f, 0.44f); // what the washes are multiplied by under the moon
	float RingNightGlow = 0.05f; // and how much light of its own the ring round the piece in hand keeps
	struct FSinkingShip
	{
		TObjectPtr<UProceduralMeshComponent> Mesh;
		TObjectPtr<UProceduralMeshComponent> Water; // the patch of white water where she goes under
		TObjectPtr<UMaterialInstanceDynamic> WaterInk;
		double Began = 0.0;
		FVector At = FVector::ZeroVector;
		double Yaw = 0.0, High = 10.0, Long = 10.0;
		double T = 0.0; // how far through it she is: 0 afloat, 1 gone; the white water lingers to 1.6
	};
	TArray<FSinkingShip> SinkingShips;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> SinkingMeshes; // (kept from the collector while they go down)
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SinkingInks;
	void UpdateSinkings();
	TArray<FWreckMark> WrecksShown;
	int32 WrecksTurn = -1;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> WrecksMesh;
	bool bChartOn = false;
	TArray<int32> ChartLevels;
	TArray<FOldSighting> ChartOld;
	TArray<FOldSighting> ChartAsked; // (as the rules last gave them, before any stood for a picture)
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ChartMesh;
	struct FChartPoint
	{
		FVector At = FVector::ZeroVector;
		int32 Sea = INDEX_NONE, Next = INDEX_NONE; // the sea it lies in, and the nearest other
		float Own = 1.0f; // how wholly it is the first sea's: 1 well inside, a half on the line between the two
		bool bWater = false;
	};
	TArray<FChartPoint> ChartPoints; // a grid over the water, laid once
	int32 ChartAcross = 0, ChartDown = 0;
	TWeakObjectPtr<AArmyPiece> HandLampPiece;
	UPROPERTY()
	TObjectPtr<class USpotLightComponent> HandLamp;
	bool bHandLampAtSea = false; // the piece under the lamp is a fleet: by day the lamp burns low over water
	float HandLampGlow = 0.0f; // 0 out, 1 full: it comes up and goes down over a third of a second
	FVector HandLampAt = FVector::ZeroVector;
	struct FVictorFlag
	{
		FString Tag; // whose flag flies there (none: it is on its way down for good)
		FString Was; // whose flew there before the change, while that one is hauled down
		double Since = -1000.0; // when the change was made, by the world's clock
	};
	TMap<int32, FVictorFlag> VictorFlags; // by region: the flag over a captured capital
	bool bVictorsStale = false;
	bool bVictorsSeen = false; // the towns have stood once: a change from here on is acted out
	bool bVictorsMoving = false; // a flag is on its way up or down
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> VictorFlagMesh;
	TArray<TArray<FVector2D>> RiverPaths; // every river's course as it is drawn, wander and all
	TArray<double> RiverBroad; // and how broad each is
	TArray<TArray<double>> RiverHalf; // and its ribbon's half breadth at each point as it is laid (release 2: the gun pits keep off it)
	TArray<FBattleSmoke> Battles;
	TArray<FVector> SmokeVertices;
	TArray<FColor> SmokeColours;
	bool bSmokeMade = false;
	int32 SmokeRoom = 0; // (Speed: "tc.Speed.SmokeTrim") how many puffs the mesh on the graphics chip has room for
	int32 SmokeRoomSpare = 0; // frames running that less room would have done
	TArray<FVector> SmokeSentVertices; // (the puffs in use, and the two corners that keep the bounds: see UpdateSmoke)
	TArray<FColor> SmokeSentColours;
	// Flak (Brian picked "hanging smoke" on 16 November 2026, after the solid bursts: "its solid", "and looks like a
	// ball"). A burst is soft see-through smoke drawn with the smoke's own puffs, never a mesh or a ball. It is born
	// at a place and stays there, drifting slowly downwind, while the bombers fly on and leave a greying wake.
	struct FFlakBurst
	{
		FVector At = FVector::ZeroVector; // where it burst
		double Born = 0.0;
		int32 Seed = 0; // no two alike
		double Wings = 0.0; // a wingspan of the bombers it was fired at: every size of it is in these
		int32 Flashed = 0; // how many frames its flash has been drawn: one or two, and no more
		bool bLight = false; // (release 2, the flak brief, 2c) light flak: pale, half the size, lower, and soon gone
		// (The far side, seam-4, the mend of 25 November 2026.) The raid that fired it, by the province bombed and the
		// province flown from: over the chart's seam a burst is brought round to ITS OWN raid's turn of the world, never
		// to the eye's, so that it is never left hanging over a twin town with no bombers above it.
		int32 Over = INDEX_NONE;
		int32 From = INDEX_NONE;
	};
	TArray<FFlakBurst> FlakBursts;
	int32 FlakFired = 0;
	// (Release 2, the art director's flak brief of 24 November 2026; "tc.Look.FlakRing", off as it comes.) The heavy
	// guns of a town as they stand on the table: where each barrel ends and which way it points, by region. With the
	// ring on, each is seen to fire from there while a raid is over the town.
	struct FFlakGun
	{
		FVector Muzzle = FVector::ZeroVector;
		FVector Bore = FVector::UpVector;
	};
	TMap<int32, TArray<FFlakGun>> FlakGuns;
	bool bFlakRingShown = false; // (the buildings as they stand were laid with the ring on)
	// (The art director's round 36: "the lobes are round".) The smoke of a burst is one or two of her own painted
	// shapes, torn at the edge and never round: twelve on one sheet, four shapes at each of three ages. Each is a
	// flat picture turned to the wind and to the eye.
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> FlakSmokeMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FlakSmokeInk;
	UPROPERTY()
	TObjectPtr<UTexture2D> FlakSheet;
	TArray<FVector> FlakVertices;
	TArray<FVector2D> FlakPlaces; // where on the sheet each corner is
	TArray<FColor> FlakColours;
	bool bFlakSmokeMade = false;
	void UpdateSmoke(float CameraDistance, const FVector& CameraFocus);
	/**
	 * (The effects thread's battle effects kit, 18 November 2026; Brian wants explosions "very dramatic and life like".)
	 * On the campaign table it is made only for its own test barrage, "-TheaterBlastTest", which plays round whatever the
	 * eye looks at: Brian sees a clip of it before any of it is built into play. "-TheaterBlastScale=0.34" says how many
	 * of the map's units a metre of it is drawn as (the raid's bombers are drawn at 0.34).
	 */
	TUniquePtr<FBattleEffects> Blasts;
	FVector BlastTestAt = FVector(1.0e12, 0.0, 0.0);
	double BlastUnitsAMetre = 3.4; // (world units to the metre the kit was begun with)
	UPROPERTY()
	TObjectPtr<UTexture2D> MiniTexture;
	TArray<int16> MiniRegion; // the province under each point of the small map, -1 for the sea, or MiniUnclaimed for land beyond the provinces
	static constexpr int32 MiniUnclaimed = -2;
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
	TArray<int32> ShownUnseen, ShownGlimpsed;
	/** Where the ground nobody of the player's can see lies, a pixel to a point of the terrain: there the land is the staff's paper map, and what stands on it is drawn, not coloured. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FogPicture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> NoWatersPicture; // "no river and no lake anywhere": the picture of the waters for ground that has none painted
	// The ownership paint (the art director's brief of 15 November 2026): occupied stripes, the siege colour, ink that spreads.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> OccupiedPicture; // where the ground is occupied and the colour its stripes take, a pixel to a point of the terrain
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> NoStripesPicture; // "occupied nowhere": for ground beyond the map
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WorldTintPicture; // the nations' paint, the wash each needs, and the staff's paper, on the world's coarse sheet
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WorldReliefPicture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WorldFogPicture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WorldOccupiedPicture;
	/** A siege as it stands on the table: its colour has come this far across the field and is on its way to that far. */
	struct FSiegeShown
	{
		FString By;
		TArray<int32> From;
		bool bEverySide = false;
		float Depth = 0.0f, Target = 0.35f, Speed = 1.0f;
		bool bLeaving = false; // lifted: the colour drains back to the frontier and is gone
	};
	TMap<int32, FSiegeShown> SiegesShown;
	bool bSiegesMoving = false;
	void LaySieges();
	/** Ink that spreads: a province that has just changed hands, the sides its new colour comes in by, and how far a siege's colour had come already. */
	struct FInkRun
	{
		int32 Region = INDEX_NONE;
		TArray<int32> From;
		float Start = 0.0f;
	};
	TArray<FInkRun> InkRuns;
	// (speed: tc.Speed.KeptHeights) the points SurfacePointKept has worked out, by squares of the world as it is kept
	mutable TMap<FIntPoint, TMap<FVector, FVector>> KeptPoints;
	mutable int32 KeptPointsFor = -1; // (the provinces the map had when they were kept: another map, and they are dropped)
	mutable float KeptPointsScale = 0.0f;
	/** A fine sheet has stood up or been taken away over Box: the points kept on and around it are worked out again. */
	void ForgetKeptPoints(const FBox2D& Box);
	float HighlightMoon = 0.0f; // the moonlight the picked and the pointed province's edges were last drawn for
	uint64 FrontLineDue = 0, BordersDue = 0; // (speed) the frame (GFrameCounter) in which the front line and the frontier lines, put off by a press, are drawn; 0 none
	TArray<int32> BorderAcross; // (for each edge of each outline, in the order the frontier lines are built: the province across it)
	int32 BorderAcrossFor = 0; // (how many provinces the map had when that was found: another map, and it is found again)
	/**
	 * (Speed, tc.Speed.BordersKept.) One edge of a province's outline that a line is drawn along, learned once: the
	 * outlines never change, only who holds the ground either side of them, and with it which of the three lines the
	 * edge is (the frontier, the province line, the pale province line on dark ground) and how wide.
	 */
	struct FBorderEdge
	{
		int32 Region = INDEX_NONE; // (where in the map's provinces the one whose outline it is stands: of the two, the one with the lower number)
		int32 Other = INDEX_NONE; // (the province across it)
		FVector2D A = FVector2D::ZeroVector;
		FVector2D Dir = FVector2D::ZeroVector;
		FVector2D Side = FVector2D::ZeroVector;
		double Length = 0.0;
		int32 Steps = 1;
		FBox2D Bounds = FBox2D(ForceInit); // (where its ribbon's points lie, and a little more)
		int32 Line = INDEX_NONE; // (the section it was last laid in: 0 the frontier, 1 the province line, 2 the pale one)
		TArray<FVector> Laid[2]; // (its ribbon's points as laid, at a province line's width [0] and a frontier's [1]: made when first wanted, and again when the ground under them changes)
	};
	TArray<FBorderEdge> BorderEdges;
	int32 BorderEdgesFor = -1; // (how many provinces the map had when they were learned)
	float BorderEdgesScale = 0.0f; // (the height the ground was laid at then: another, and every ribbon is laid again)
	bool bBorderLineStale[3] = { true, true, true }; // (a section to be handed to the graphics chip again)
	void LearnBorderEdges();
	void BuildBordersKept();
	/** A fine sheet has stood up or been taken away over Box: the frontier lines over and around it are laid again next time. */
	void ForgetBorderEdges(const FBox2D& Box);
	double InkBegan = 0.0, PaintStepped = 0.0;
	// ---- (release 2, the front you can read) ----
	float InkSeconds = 1.0f; // how long ink takes to cross a province: release 1's second, the art director's ruling of 22 November (run 3); "-TheaterInkSeconds=1.5" for the plan's second and a half
	uint64 InkSkipFrame = 0; // (the frame in which the player cut the month's replay short: see SkipInk)
	uint64 InkBeganFrame = 0; // (the frame in which the last run of ink began: see FitInk)
	bool bInkHeldByCard = false; // (an order's result card is up: the ink stands where it was, see HoldInk)
	double InkHeldRun = 0.0; // (how far into its second the ink stood when the card came up: see HoldInk)
	// ---- (end release 2, the front you can read) ----
	bool bInkBorders = false; // the frontier's lines are still to be drawn in their new place
	FIntRect InkBox = FIntRect(0, 0, 0, 0);
	TArray<FColor> TintBefore, OccupiedBefore, TintShown, OccupiedShown;
	void StepInk(double Second);
	void EndInk();
	void UpdateOwnershipPaint();
	float PaperShown = -1.0f;
	double MarginLine = 11.0; // (round a world) how wide the chart's ruled border is drawn, in the world's units
	float SquareWorkShown = -1.0f; // (round a world) how much of the prairie's roads and furrows is shown at this height
	float FarFieldsShown = -1.0f; // (round a world) how strong the quilt of fields is on the world's coarse sheet
	float FarFlatShown = -1.0f; // how far the world's far sheet has its fields drawn to their mean
	FLinearColor QuiltMean = FLinearColor::White; // the mean of the quilt's tones, as the land's paint reads them
	float WoodsLiftShown = -1.0f; // (round a world) how much more is asked of a wood on the coarse sheet before it is drawn
	/** The paper's own colours by the hour: parchment and a pale sea by day, dim paper by night. */
	void ApplyPaperLight();
	float PaperLand[7] = { 0.6f, 0.52f, 0.37f, 0.6f, 0.92f, 0.5f, 0.62f }; // parchment (r, g, b), its shade and sun, the holder's wash and band
	float PaperSeaTone[3] = { 0.221f, 0.302f, 0.305f };
	float PaperNight[3] = { 0.42f, 0.3f, 0.35f }; // by night: what the land, the sea and the wash are multiplied by
	/** A margin of paper round the sheet, with a ruled border, and its soft shadow on the table: seen from the height where the table is the staff's map. */
	void BuildSheetMargin();
	/** The settings of the land's paint that belong to the land beyond Europe's own sheet alone (the world's coarse sheet and each fine one). */
	void DressBeyond(UMaterialInstanceDynamic* Sheet) const;
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> MarginMesh;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MarginPaper;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MarginShade;
	TSet<int32> UnseenSet;
	TSet<int32> GlimpsedSet; // (ruling 154) ground he only glimpses
	TSet<int32> HazeSet; // the two together: where the haze lies
	// (The fog's look brief, section 4.3.) The haze thins over about 0.6 seconds where ground comes into view, and
	// returns over about a second where it goes out of view. What the fog's picture shows now, and what it is on its way to.
	TArray<FColor> FogShownPixels, FogWantedPixels;
	FIntRect FogFadeBox;
	bool bFogFading = false, bFogLaidOnce = false;
	void UpdateFogFade(float DeltaSeconds);
	TArray<FTownInfo> Towns; // by province id, once the rules have said
	bool bWealth = false;
	bool bSides = false;
	TArray<FString> SideOf; // by province: own, ally, enemy or neutral, as the player stands to its holder
	// (Release 2, map run 7: tc.Speed.SidesKept) the sides view laid once and kept: its mesh, each point's province, and
	// the sides it was last coloured for
	void LaySidesKept(bool bWarmOnly);
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SidesMesh;
	TArray<int32> SidesPointRegion;
	TArray<FString> SidesColouredFor;
	// (tc.Speed.SidesFade) the kept sides view's own ink, and how far it is faded in (0 put away, 1 full); each frame
	// moves it toward the view's state, and at nought the mesh is hidden
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SidesInk;
	float SidesShown = 0.0f;
	void UpdateSidesFade(float DeltaSeconds);
	int32 ShownMonth = -1;
	// The year turning: the look the trees wear (0 summer, 1 September, 2 October, 3 November, 4 winter, 5 March,
	// 6 April), how hard the winter lies (0 to 1), and what the rivers were last drawn for.
	int32 TreeLook = 0;
	float WinterHard = 0.0f;
	float RiversDrawnFor = -1.0f;
	float MasksSentFor = -1.0f; // (tc.Speed.MonthSpread) the winter's hardness the masks picture was last sent with
	// ---- (release 2, the season turns: Campaign/SeasonTurn.h has the reckoning, Campaign/CampaignSeasonTurn.cpp the
	// rest; tc.Map.SeasonTurn, off as it comes) ----
	/** The snow the month lays on the masks picture (its green), for the winter's hardness Hard: the season's own paint, as SetSeason sends it. */
	void MakeSnowMasks(float Hard, TArray<FColor>& Pixels) const;
	/** SetSeason tells the turn the month it shows, how hard the winter lies now and the month before, and whether the month has just stepped on by one in play. */
	void NoteSeason(int32 Month, float Hard, float HardBefore, bool bStepped);
	/** With tc.Map.SeasonTurn: the month's new masks are taken by the turn (true), which spreads their snow over the coming frames; otherwise (false) they are to be sent now, as before. */
	bool TakeSeasonSnow(TArray<FColor>& Pixels);
	/** With tc.Map.SeasonTurn: the mud of the rules' ground is painted into the ground picture (true); otherwise (false) it is the flat wash, as before. */
	bool AskSeasonMud(const TArray<int32>& Regions);
	/** A frame of the turn: the mud's field is worked out on another thread, and a spreading turn paints its next step. Nothing at all while no turn is in hand. */
	void UpdateSeasonTurn();
	TSharedPtr<FSeasonTurnTable, ESPMode::ThreadSafe> SeasonTurn;
	/** The ground picture as the table holds it now, under any churned ground: the season's own copy while its mud lies or soaks in, else the table's paint. */
	const TArray<FColor>& SeasonGroundShown() const;
	/** The same for the masks: the season's own while its snow spreads, else the month's snow as it was sent. */
	void SeasonMasksShown(TArray<FColor>& Out) const;
	// ---- (end release 2, the season turns) ----
	// ---- (release 2, fronts that feel alive, piece 1: churned ground between dug-in lines. Campaign/FrontChurn.h has the
	// arithmetic, Campaign/CampaignFrontChurn.cpp the rest; tc.Map.FrontChurn, off as it comes) ----
	/** A frame of it: the lines laid a frame after a view that changed them, the pictures' squares sent two a frame, the fine veil and the craters near the eye. Nothing at all while off. */
	void UpdateFrontChurn(const FVector& CameraFocus, float CameraDistance);
	void LayFrontChurn();
	/** One point of the pictures, churned as far as its square shows it, into the pictures as this table keeps them. */
	void LayChurnCell(FFrontChurnCell& Cell);
	void PutChurnTileAway(FFrontChurnTile& Tile);
	void BuildChurnVeil(FFrontChurnTile& Tile, const FFrontChurnVeilWork& Work);
	int32 BuildChurnCraters(FFrontChurnTile& Tile, int32 Most);
	/**
	 * Every send of the ground picture and the masks goes through these, whoever sends them: with tc.Map.FrontChurn on,
	 * the churned ground is laid over the pixels as they are sent, and the arrays the table and the season keep never
	 * hold it. Off, each is the plain send it was.
	 */
	void SendGround(const TArray<FColor>& Pixels);
	void SendGroundBox(const TArray<FColor>& Pixels, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 MaxLevel = MAX_int32);
	void SendMasks(const TArray<FColor>& Pixels);
	void SendMasksBox(const TArray<FColor>& Pixels, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 MaxLevel = MAX_int32);
	/** The woods at a point of the ground picture for the trees standing close up: thinned in the core of churned ground. */
	float ChurnWoods(int32 Index, float Wooded) const;
	TSharedPtr<FFrontChurnTable, ESPMode::ThreadSafe> FrontChurn;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> ChurnMeshes; // the veil's and the craters' squares, made as they are wanted and kept to be used again
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ChurnInks; // each one's ink, faded by itself
	// ---- (end release 2, churned ground) ----
	TSet<int32> RiversHazedFor; // the provinces under the haze when the rivers were last drawn: a river under it is darker
	bool bRiversNightFor = false; // and whether it was night then: by night it is darker still
	/**
	 * (Speed: tc.Speed.RiverInks.) The rivers' inks as they were laid before the haze, point for point in each of the
	 * river mesh's two sections, and the province each point asks the haze of (none for sand and islands); with these
	 * RecolourRivers darkens them where the haze lies now without laying the rivers again. False if it cannot.
	 */
	TArray<FColor> RiverPlainInks[2];
	TArray<int32> RiverInkRegions[2];
	bool bRiverInksKept[2] = { false, false };
	bool bRiverInksFor = false;
	bool RecolourRivers();
	/** How deep the snow lies at a place on the map (0 to 1) when the winter is this hard. */
	static float SnowLying(const FVector2D& Place, float Hard);
	TMap<int32, FVector2D> AirfieldSpots; // by province: where its airfield stands
	TMap<int32, FVector2D> AirfieldLie; // and how it lies: X the way it is turned (degrees), Y its size (1 to 4)
	FString ShownAirSigns; // what the aircraft parked on the airfields were last built from
	/** An airfield in use, for the aircraft parked on it close to: where it is, whose, how it lies, how many stand there, and its mesh once made. */
	struct FParkedField
	{
		FVector2D Spot = FVector2D::ZeroVector;
		FString Nation;
		FString YourNation; // (the player's, where his own groups stand on a field another country holds)
		double Turn = 0.0, Wide = 1.5;
		int32 Parked = 0, Mesh = INDEX_NONE;
		int32 Yours = 0; // how many of the row are his, at its head
		bool bShown = false;
		float Up = 0.0f; // how far its aircraft have come up out of the ground, 0 to 1
		double Aside = 0.0; // (seam-4) how far east of where they were built they are shown, in the world's measure: a whole number of worlds
	};
	TArray<FParkedField> ParkedFields;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> ParkedMeshes;
	bool bParkedClose = false;
	float BoxesDown = 0.0f; // how far the silver box aircraft have gone down into the ground, 0 to 1
	void ShowParkedPlanes(float CameraDistance, const FVector& CameraFocus);
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> AirSignMesh; // aircraft parked on the airfields that have air groups based there
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> AirPennantMesh; // the player's pennants on fields other countries hold: never sunk with the box aircraft
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

	/** (Rules 84.3.3) The staff note's pointer on the ground: a mesh of its own, small, laid again as the eye moves. */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<UProceduralMeshComponent> PointerMesh;

	/**
	 * (Release 2, the enemy's month near you.) The arrows of the watch near him: a mesh of their own, small, laid again each
	 * frame while they grow (the 3D pre-check, 24 Nov 18:14Z: laid in a section of the orders' mesh they made the
	 * whole of that mesh's render copy again each frame).
	 */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<UProceduralMeshComponent> MovesMesh;

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
	/** The same towns as they are drawn from high up: one piece to a block, and no shadows thrown. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> TownFarMesh;
	int32 TownsFarShown = -1; // 1 while the far towns are the ones drawn, 0 the near, -1 not yet said
	TArray<int8> TownSquareForm; // (speed: tc.Speed.ShowSpread) the form each square of towns is shown in now: 1 far, 0 near
	/**
	 * (Speed, 17 November 2026; "tc.Speed.TownSquares".) The same towns cut into squares of country, near form and far
	 * form, a mesh to each square. One mesh the size of the world is drawn whole every frame wherever the eye is, and
	 * drawn whole again into every part of the shadows that changes; a square out of view is not drawn at all. While
	 * the towns stand in squares, TownMesh and TownFarMesh are left empty.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> TownSquares;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> TownFarSquares;
	int32 TownsCut = -1; // 1 while the towns stand in squares, 0 in one piece, -1 not built yet
	int32 TownsGrand = -1; // (release 2, "tc.Map.GrandCities" and "tc.Map.GrandCitiesFar") 1 while the great cities stand in their grander form, plus 2 while the far side's do, 0 as they were, -1 not built yet
	int32 TownShadowsCast = -1; // ("tc.Speed.TownShadows", for measuring only) 1 while the near towns throw shadows
	/** Shows the near towns (and their windows' panes) or the far ones, in whichever pieces they stand. */
	void ShowTownForm(bool bFar);
	/** (for measuring only) Whether the near towns throw shadows. */
	void SetTownShadows(bool bCast);
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
	TArray<int32> ShownBombed;
	struct FBombSmoke
	{
		int32 Region = INDEX_NONE;
		FVector Root = FVector::ZeroVector; // where the column stands
		double Tall = 0.0; // how high it rises, at working height
		double Began = 0.0; // when it began to rise
		double Ends = -1.0; // when it began to thin away (less than nothing while it stands)
		int32 Seed = 0;
		bool bFirst = false; // the one column of its place that is still drawn from the top
	};
	TArray<FBombSmoke> BombSmokes; // bombed places, for the black smoke that stands over them
	TArray<int32> ShownWorksLeft;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> WorksLeftMesh;
	FString ShownPrizes; // what the prize marks were last built from
	TArray<FVector> PrizeFires; // wrecked prizes, for the flame and the smoke over them
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> PrizesMesh; // the pennants over prizes held outside a town
	TArray<FVector> SiegeFires; // besieged towns, for the smoke that hangs over them
	// Pieces on the move, to know when one is set down; and where pieces were lately set down, for the dust.
	TSet<TWeakObjectPtr<AArmyPiece>> OnTheMarch;
	TArray<TPair<FVector, double>> SetDowns;
	/**
	 * (Release 2, armies with weight: Campaign/ArmyCarry.h.) Where a piece was lately set down with weight, for the dust
	 * it raises: a low ring that runs out along the table and a cloud that rises behind it, more of both for a bigger
	 * army, white on snow and pale on sand. Filled in place of SetDowns while tc.Map.ArmyWeight is on.
	 */
	struct FThud
	{
		FVector Where = FVector::ZeroVector;
		double Began = 0.0;
		float Much = 1.0f; // how much of an army landed (TheaterArmyCarry::Heft)
		int32 Ground = 0; // 0 earth, 1 snow, 2 sand
	};
	TArray<FThud> Thuds;
	/**
	 * A piece sent to join an army already standing (it is taken off as it lands), the army it joins, and where it set
	 * out from. Also where that army stood and when the piece is due: an army laid somewhere else since, or a piece
	 * taken off before its time, is no landing, and nothing rocks.
	 */
	struct FJoining
	{
		TWeakObjectPtr<AArmyPiece> Sent;
		TWeakObjectPtr<AArmyPiece> Joined;
		FVector From = FVector::ZeroVector;
		FVector Stood = FVector::ZeroVector;
		double Due = 0.0;
	};
	TArray<FJoining> Joining;

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
	TObjectPtr<UTexture2D> WavesPicture; // the moving sea's swell, chop, flecks and slow unevenness
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> HazePicture; // the wisps of the fog of war's haze

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
	TMap<FIntPoint, FString> CrowdLead; // for each province crowded with flags of one side (the province, and 0 for the player's side or 1 for the rest): whose piece stands for them all while it is folded up
	TSet<FString> CrowdFriends; // the player's country and those that fight beside it
	/** Folds up the crowded provinces, and fans out the one that is pointed at or picked. */
	void ApplyCrowds();

	/** The fleets and the air fleets, each as a piece, by the number the rules know it by. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AArmyPiece>> FleetPieces;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AArmyPiece>> AirPieces;
	// (Release 2: ships that sail.) Where each fleet lay when the fleets were last laid out: its sea and its harbour
	// (INDEX_NONE at sea), and the point on the map.
	bool bShipsSail = true; // (the 1.5 patch: on unless switched off in OPTIONS)
	TMap<int32, FIntPoint> FleetPlaces;
	TMap<int32, FVector2D> FleetSpots;
	// (Release 2: fading wakes.) Each wake laid: whose fleet, the month it was seen to move, and its way on the map.
	struct FWakeTrail
	{
		int32 Fleet = INDEX_NONE;
		int32 Turn = 0;
		TArray<FVector2D> Points;
	};
	bool bWakes = false;
	TArray<FWakeTrail> Wakes;
	int32 WakesLaidFor = INDEX_NONE; // the month the foam was last laid for
	bool bWakesDirty = false;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> WakeMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WakeTrailInk;
	/** Lays the foam of the wakes still to be seen in this month, fading with their age and back along their way. */
	void LayWakes(int32 Turn);

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
	FLandSheet HomeLand; // the map's own land
	FLandSheet WorldLand; // the land beyond it (the whole-world map)
	FCampaignTerrain WorldTerrain;
	FCampaignGround WorldPaint;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WorldLandMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WorldSeaMaterial;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> WorldPictures;
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> WorldSeaMesh;
	bool bLandShadows = false;

	/** The tiles and the spare meshes, kept where the engine can see them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> LandMeshes;
	TArray<UProceduralMeshComponent*> SpareLandMeshes;

	int32 SelectedRegion = INDEX_NONE;
	int32 HoveredRegion = INDEX_NONE;
	bool bReachShown = false; // an army is in hand and its reach is on the table
	int32 LabelState = -1;
	float LastPieceYaw = 0.0f;
	float StandardGrowth = 1.0f, StandardLean = 0.0f; // how large the pieces' standards are drawn just now, and how far they lean back toward the eye
	mutable TArray<float> TownSpreads; // TownSpread for every province, worked out once
	mutable TArray<bool> TownPacked; // true for a city or capital in a province too small for it: it is built tight and tall
	mutable TArray<bool> TownHumbled; // true for a city that gave way to a greater town beside it: it is built as a town, a church and its streets
	mutable TMap<int32, TArray<FVector>> TownClear; // for a greater town, the ground it leaves green beside each lesser town that gave way to it: the middle (X, Y) and how far round it (Z), in map units
	/** For each of two towns that both kept their size beside one another: the other's middle, how far its roofs may run, its province, and how far off that ground this town keeps. */
	struct FTownKeepOff
	{
		FVector2D Middle = FVector2D::ZeroVector;
		double Reach = 0.0;
		int32 Province = INDEX_NONE;
		double Off = 0.0;
	};
	mutable TMap<int32, TArray<FTownKeepOff>> TownKeepOff;
	TMap<int32, FVector2D> TownQuays; // where each port's quay stands, as its town was last built: the names of the provinces keep off them
	TMap<int32, FVector> TownBeacons; // by province: the lantern of its port's lighthouse, on the table
	TSet<int32> AirfieldsUsed; // the provinces whose airfields have air groups based on them
	bool bAirfieldsKnown = false; // the rules say which airfields are in use (the grand campaign): an empty one is then built smaller
	TArray<FIntVector4> LastWorks; // what the provinces' works were last built from
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LanesInk; // the shipping lanes and trade routes: dimmed with the land by night
	bool bFacingSet = false;
};
