#include "Campaign/CampaignMapActor.h"
#include "Algo/Reverse.h"
#include "Campaign/TheaterSpeed.h"
#include "Campaign/TheaterStillMesh.h"
#include "ImageUtils.h"
#include "CoreGlobals.h" // (GFrameCounter)
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "EngineUtils.h"
#include "Campaign/CampaignClimate.h"
#include "Campaign/CampaignEnvironment.h"

// (the land's own sun and shade at the evening: kept with the rest of the evening's look, in the room)
FLinearColor ACampaignEnvironment_DuskSun();
FLinearColor ACampaignEnvironment_DuskShade();

#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignFlags.h"
#include "Campaign/CampaignUnitTable.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignTextures.h"
#include "Campaign/FrontLine.h" // (release 2, the front you can read)
#include "Campaign/EnemyMoves.h" // (release 2, the enemy's month near you)
#include "Campaign/FleetHeading.h" // (release 2, fleets that keep their heading)
#include "Campaign/ShipWay.h" // (release 2, a fleet under way)
#include "Campaign/ShipSmoke.h" // (release 2, funnel smoke)
#include "Campaign/ArmyCarry.h" // (release 2, armies with weight)
#include "TheaterCommand.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"

/** Geometry on its way into one section of a procedural mesh. */
struct FCampaignMeshBuffers
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	int32 AddVertex(const FVector& Position, const FVector& Normal, const FColor& Color, const FVector2D& UV = FVector2D::ZeroVector, const FVector& Tangent = FVector(0.0, 1.0, 0.0))
	{
		Vertices.Add(Position);
		Normals.Add(Normal);
		Colors.Add(Color);
		UVs.Add(UV);
		Tangents.Emplace(Tangent, false);
		return Vertices.Num() - 1;
	}

	/** Adds a triangle wound so that its face looks up, whichever order the corners came in. */
	void AddTriangleFacingUp(int32 A, int32 B, int32 C)
	{
		const FVector FaceNormal = (Vertices[B] - Vertices[C]) ^ (Vertices[A] - Vertices[C]);
		Triangles.Add(A);
		if (FaceNormal.Z >= 0.0)
		{
			Triangles.Add(B);
			Triangles.Add(C);
		}
		else
		{
			Triangles.Add(C);
			Triangles.Add(B);
		}
	}

	bool Commit(UProceduralMeshComponent* Mesh, int32 Section, UMaterialInterface* Material) const
	{
		if (Triangles.Num() == 0)
		{
			return false;
		}
		Mesh->CreateMeshSection(Section, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
		Mesh->SetMaterial(Section, Material);
		// (seam-2: a model of the table that has a twin over the chart's seam has been laid again, and its twin is
		// to be; the keeper cannot always see it by looking)
		if (ACampaignMapActor* Table = Cast<ACampaignMapActor>(Mesh->GetOwner()))
		{
			Table->SeamLaidAgain(Mesh);
		}
		return true;
	}
};

namespace
{
	FLinearColor FromHex(const TCHAR* Hex)
	{
		return FLinearColor(FColor::FromHex(Hex)); // sRGB in, linear out
	}

	// (Release 2, the art director's flak brief of 24 November 2026: Brian, "i also don't see any anti aircraft guns
	// around the cities for flak".) Off as it comes. On, a town's heavy guns stand in a ring round its roofs, three
	// pits a level, and are seen to fire while a raid is over them; light flak shows its pale puffs where no guns
	// are built; and by night a burst is a short flash and a smoke that is barely there.
	TAutoConsoleVariable<int32> CVarFlakRing(
		TEXT("tc.Look.FlakRing"), 0,
		TEXT("1 stands a town's anti-aircraft guns in a ring round it, three pits a level, and shows them firing in a raid (as -TheaterFlakRing). Release 2; off as it comes."));
	bool FlakRingAsked()
	{
		static const bool bOnTheLine = FParse::Param(FCommandLine::Get(), TEXT("TheaterFlakRing"));
		return bOnTheLine || CVarFlakRing.GetValueOnGameThread() != 0;
	}

	// (Release 2, 25 November 2026.) Ruling 107's night look of a raid: the hour falls to night while a raid flown by
	// night is watched, and the place raided shows its searchlights. It was built for release 1 and never seen in
	// play, because no flight the rules sent ever said night; the air rules thread's bridge now says it. A look that
	// comes on in play has its switch (Brian, 25 November 01:10Z: every release 2 feature keeps one in OPTIONS): off
	// as it comes, a night raid is watched by day as in every release before, until the Mac and the art director
	// pass it. "-TheaterNightRaid" still flies every raid by night for a picture, switch or no switch.
	TAutoConsoleVariable<int32> CVarNightRaid(
		TEXT("tc.Look.NightRaid"), 0,
		TEXT("1 watches a raid flown by night in the dark, the place raided showing its searchlights (ruling 107); 0 watches it by day, as before. Release 2; off as it comes."));

	// (Release 2, the far side: the art director's seam-2, 25 November 2026.) With the whole world on the table, what
	// lies within sight of the chart's seam is shown from both sides of it: the screen's names and plates, the lines and
	// towns laid on the table, and the pieces. On as it comes, and it does nothing without the far side. 0 is the table
	// as the third far bundle left it, everything laid once, for the Mac to set the one against the other.
	TAutoConsoleVariable<int32> CVarOverSeam(
		TEXT("tc.Far.OverSeam"), 1,
		TEXT("With the far side on, 1 shows what lies within sight of the chart's seam (141 W) from both sides of it; 0 lays everything once, as before. Release 2."));

	// (Release 2, the campaign map. Brian, 26 November 2026: "the wake of the boats when moving don't follow the direction
	// they came from".) Every piece on the table is turned to face the eye, and so was every fleet: her wake, which lies
	// astern of her, streamed off to the left of the screen whichever way she had come. With this a fleet at sea lies on
	// the heading she came in on, her wake astern of her along it, and only her standard turns to the eye
	// (Campaign/FleetHeading.h). Off as it comes, until the Mac has shown the one beside the other;
	// "-TheaterFleetHeading" turns it on, switch or no switch. It is read when the fleets are next laid.
	TAutoConsoleVariable<int32> CVarFleetHeading(
		TEXT("tc.Map.FleetHeading"), 0,
		TEXT("1: a fleet at sea keeps the heading she arrived on, her wake trailing back the way she came, and only her flag turns to face the eye; 0: every fleet is turned to face the eye, as before. Release 2; off as it comes."));

	// (Release 2, a fleet under way: the art director's campaign map upgrade 1, 25 November 2026, on Brian's "I want to
	// keep upleveling the graphics and smoothness of how they move and act to make it feel as real as possible" and
	// "can we do that for the campaign map too?".) With ships that sail on, a fleet sent to another sea takes a ship's
	// course (Campaign/ShipWay.h) and not the way's own corners. Off as it comes, until Brian has seen the Mac's film of
	// the one beside the other; "-TheaterShipWay" turns it on for that film, switch or no switch. The rest are its
	// measures, for the Mac to try without a build.
	TAutoConsoleVariable<int32> CVarShipWay(
		TEXT("tc.Map.ShipWay"), 0,
		TEXT("1: a fleet that sails takes a ship's course (out on her turning circle, wide round the corners of her way, in on a circle, slow off the mark and round a tight turn, her wake along the water she has crossed); 0: corner to corner, as before. Release 2; off as it comes."));
	TAutoConsoleVariable<float> CVarShipTurn(
		TEXT("tc.Map.ShipTurn"), 13.0f,
		TEXT("With tc.Map.ShipWay: the radius of a fleet's turning circle as she leaves and comes in, in map units."));
	TAutoConsoleVariable<float> CVarShipSweep(
		TEXT("tc.Map.ShipSweep"), 45.0f,
		TEXT("With tc.Map.ShipWay: the radius she rounds a corner of her way on, out at sea, in map units."));
	TAutoConsoleVariable<float> CVarShipBerth(
		TEXT("tc.Map.ShipBerth"), 4.0f,
		TEXT("With tc.Map.ShipWay: how far outside a corner of her way she passes, in map units."));
	TAutoConsoleVariable<float> CVarShipHelm(
		TEXT("tc.Map.ShipHelm"), 2.4f,
		TEXT("With tc.Map.ShipWay: the fastest a fleet's head swings, in radians a second (less: she creeps round her turns and takes longer)."));
	TAutoConsoleVariable<float> CVarShipPace(
		TEXT("tc.Map.ShipPace"), 1.0f,
		TEXT("With tc.Map.ShipWay: her time for a voyage, as a multiple of the 2.8 to 6.5 seconds its length asks."));

	// (Release 2, funnel smoke: the art director's campaign map upgrade 3, 25 November 2026, on Brian's "I want this to
	// visually wow people on how smooth and realistic and detailed grand it looks".) The ships of a fleet at sea smoke
	// (Campaign/ShipSmoke.h): a pale wisp over each lying still, and under way a plume that streams astern along the
	// water she has crossed, black while she works up to speed. Off as it comes, until Brian has seen the Mac's film of
	// it; "-TheaterShipSmoke" turns it on for that film, switch or no switch. Read every frame: nothing is remade.
	TAutoConsoleVariable<int32> CVarShipSmoke(
		TEXT("tc.Map.ShipSmoke"), 0,
		TEXT("1: the ships of a fleet at sea smoke from their funnels (a wisp lying still; under way a plume astern along her course, black as she works up to speed; none at anchor, in the hand, or for submarines); 0: no funnel smoke, as before."));
	TAutoConsoleVariable<float> CVarShipSmokeMuch(
		TEXT("tc.Map.ShipSmokeMuch"), 1.0f,
		TEXT("With tc.Map.ShipSmoke: how thick the funnel smoke is, 0 (none) to 2."));
	TAutoConsoleVariable<float> CVarShipSmokeLong(
		TEXT("tc.Map.ShipSmokeLong"), 1.0f,
		TEXT("With tc.Map.ShipSmoke: how far astern a ship's smoke streams under way, 0 (it stands over her) to 2 (twice as far)."));

	bool ShipSmokeWanted()
	{
		static const bool bShipSmokeAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterShipSmoke"));
		return bShipSmokeAsked || CVarShipSmoke.GetValueOnGameThread() != 0;
	}

	// (Release 2, surf on the coasts: the art director's campaign map upgrade 4, 25 November 2026, on Brian's "I want this to
	// visually wow people on how smooth and realistic and detailed grand it looks".) Breakers roll in on an open coast:
	// white arcs that rise some fifty units out, come in over five seconds and die on the beach, in three sets, bowed
	// and broken along their length, lively where the ocean runs in and all but still in a bay's lee. They are a second
	// sea material (M_SeaSurf: Tools/build_content.py, build_sea with SEA_BREAKERS; its cloud test Tools/sea_surf), so
	// that the sea without them pays nothing for them. Off as it comes, until Brian has seen the Mac's film of it;
	// "-TheaterSurf" turns it on for that film, switch or no switch. The switch is read as the table is laid (every
	// sea paint is made from one parent then): changed in play, it shows at the next campaign. Its measure is seen at
	// once. Without the asset (Tools/build_content.sh makes it) the sea is M_Sea as before, and the log says so.
	TAutoConsoleVariable<int32> CVarSurf(
		TEXT("tc.Map.Surf"), 0,
		TEXT("1: breakers roll in on the map's open coasts (the sea is drawn with M_SeaSurf; read as the table is laid, so a change shows at the next campaign; without that asset the sea is as before); 0: the sea as before."));
	TAutoConsoleVariable<float> CVarSurfMuch(
		TEXT("tc.Map.SurfMuch"), 1.0f,
		TEXT("With tc.Map.Surf: how strong the breakers are, 0 (none: the sea as before) to 2. Seen at once."));

	bool SurfWanted()
	{
		static const bool bSurfAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterSurf"));
		return bSurfAsked || CVarSurf.GetValueOnGameThread() != 0;
	}

	const TCHAR* SurfaceMaterialPath = TEXT("/Game/Campaign/Materials/M_MapSurface.M_MapSurface");
	const TCHAR* SolidMaterialPath = TEXT("/Game/Campaign/Materials/M_Solid.M_Solid");
	const TCHAR* HighlightMaterialPath = TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight");

	// Lettering tiers.
	constexpr int32 TierAlways = 0;
	constexpr int32 TierFar = 1;
	constexpr int32 TierNear = 2;
	constexpr int32 TierMiddle = 3; // from the middle distance in: the places worth having, and what each is worth

	/**
	 * (The art director, 19 November 2026.) A rifleman seen with the sun behind him stood darker on his shaded side
	 * (about 23 of 255) than the shadow he threw on the ground (30 to 40). The pieces' paint now gives back a little
	 * of a figure's own colour on the side turned from the sun, and a thin rim on the edge turned to it when the sun
	 * stands behind: cloth and flesh only, never painted metal, and no light is added to the scene. Both go out with
	 * the day. "-TheaterFigureLight=0.3,0.6" tries another fill and rim.
	 */
	void LightFigures(UMaterialInstanceDynamic* Paint, float Moon)
	{
		if (!Paint)
		{
			return;
		}
		// (Measured from the south at the effects test's gun, 19 November 2026: a rifleman's shaded back read 18 to 22
		// of 255 with no fill, 39 to 42 at 0.3 and 60 to 62 at 0.6; the shadow he throws on the ground reads 44.)
		static float Fill = 0.45f, Rim = 0.6f;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			FString Asked;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFigureLight="), Asked, false))
			{
				TArray<FString> Parts;
				Asked.ParseIntoArray(Parts, TEXT(","));
				Fill = Parts.IsValidIndex(0) ? FCString::Atof(*Parts[0]) : Fill;
				Rim = Parts.IsValidIndex(1) ? FCString::Atof(*Parts[1]) : Rim;
			}
			UE_LOG(LogTheater, Display, TEXT("Figures: the side turned from the sun gives back %.2f of its colour, the rim %.2f"), Fill, Rim);
		}
		const FVector TowardSun = -FRotator(-42.0, 69.0, 0.0).Vector(); // (the table's lamp, as the environment hangs it)
		const float Day = 1.0f - FMath::Clamp(Moon, 0.0f, 1.0f);
		Paint->SetScalarParameterValue(TEXT("FigureFill"), Fill * Day);
		Paint->SetVectorParameterValue(TEXT("FigureSun"), FLinearColor(static_cast<float>(TowardSun.X), static_cast<float>(TowardSun.Y), static_cast<float>(TowardSun.Z), Rim * Day));
	}
}

ACampaignMapActor::ACampaignMapActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakeMesh = [this](const TCHAR* Name, bool bShadows)
	{
		UProceduralMeshComponent* Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		Mesh->SetupAttachment(Root);
		Mesh->bUseAsyncCooking = false;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(bShadows);
		return Mesh;
	};
	// (Speed: those made once and then left alone are still meshes, drawn from Unreal's kept drawing orders. A still mesh
	// must never be given SetMeshSectionVisible or UpdateMeshSection: see TheaterStillMesh.h.)
	auto MakeStill = [this](const TCHAR* Name, bool bShadows)
	{
		UProceduralMeshComponent* Mesh = CreateDefaultSubobject<UTheaterStillMesh>(Name);
		Mesh->SetupAttachment(Root);
		Mesh->bUseAsyncCooking = false;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(bShadows);
		return Mesh;
	};
	TerrainMesh = MakeStill(TEXT("Terrain"), true);
	SeaMesh = MakeStill(TEXT("Sea"), false);
	BorderMesh = MakeStill(TEXT("Borders"), false);
	RiverMesh = MakeMesh(TEXT("Rivers"), false);
	RailMesh = MakeStill(TEXT("Railways"), false);
	WorksMesh = MakeStill(TEXT("Works"), false);
	BuildingsMesh = MakeStill(TEXT("Buildings"), false);
	RoomMesh = MakeStill(TEXT("WarRoom"), false);
	CloudMesh = MakeMesh(TEXT("Clouds"), false);
	SmokeMesh = MakeMesh(TEXT("Smoke"), false);
	FlakSmokeMesh = MakeMesh(TEXT("FlakSmoke"), false);
	BeamMesh = MakeMesh(TEXT("Searchlights"), false);
	FireGlowMesh = MakeMesh(TEXT("FireGlow"), false);
	GunLineMesh = MakeMesh(TEXT("GunLines"), true);
	CutLampMesh = MakeMesh(TEXT("CutLamps"), false);
	SkyEdgeMesh = MakeMesh(TEXT("SkyEdge"), false);
	WatchMesh = MakeMesh(TEXT("WatchEdge"), false);
	RailLightMesh = MakeMesh(TEXT("RailLight"), false);
	LanesMesh = MakeMesh(TEXT("Lanes"), false);
	SortieMesh = MakeMesh(TEXT("Sorties"), true);

	// The studio, far above the table and seen by nothing but its own cameras.
	auto MakeStage = [this, &MakeMesh](const TCHAR* Name, const FVector& Where)
	{
		UProceduralMeshComponent* Stage = MakeMesh(Name, false);
		Stage->SetRelativeLocation(Where);
		Stage->SetVisibleInSceneCaptureOnly(true);
		return Stage;
	};
	auto MakeStudioCamera = [this](const TCHAR* Name, UProceduralMeshComponent* Stage, bool bEveryFrame)
	{
		USceneCaptureComponent2D* Camera = CreateDefaultSubobject<USceneCaptureComponent2D>(Name);
		Camera->SetupAttachment(Root);
		Camera->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Camera->ShowOnlyComponents.Add(Stage);
		Camera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Camera->bCaptureEveryFrame = false;
		Camera->bCaptureOnMovement = false;
		// (Kept from one picture to the next for both cameras: a camera that keeps nothing takes every picture at one
		// fixed exposure whatever it is told, and an aircraft's picture needs its own.)
		Camera->bAlwaysPersistRenderingState = true;
		Camera->FOVAngle = 20.0f;
		// A plain, steady picture: the lamp and the light of the room, and nothing that needs several frames to settle.
		Camera->ShowFlags.SetTemporalAA(false);
		Camera->ShowFlags.SetMotionBlur(false);
		Camera->ShowFlags.SetBloom(false);
		Camera->ShowFlags.SetFog(false);
		Camera->ShowFlags.SetAtmosphere(false);
		Camera->ShowFlags.SetGlobalIllumination(false);
		Camera->ShowFlags.SetLumenReflections(false);
		Camera->ShowFlags.SetScreenSpaceReflections(false);
		Camera->ShowFlags.SetGrain(false);
		Camera->ShowFlags.SetVignette(false);
		Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
		Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
		Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
		Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
		Camera->PostProcessSettings.AutoExposureBias = -0.9f;
		Camera->PostProcessBlendWeight = 1.0f;
		return Camera;
	};
	PortraitMesh = MakeStage(TEXT("PortraitStage"), FVector(0.0, 0.0, 60000.0));
	PortraitCamera = MakeStudioCamera(TEXT("PortraitCamera"), PortraitMesh, false);
	ShowMesh = MakeStage(TEXT("ShowStage"), FVector(6000.0, 0.0, 60000.0));
	ShowCamera = MakeStudioCamera(TEXT("ShowCamera"), ShowMesh, true);
	HighlightMesh = MakeMesh(TEXT("Highlight"), false);
	OrdersMesh = MakeMesh(TEXT("Orders"), false);
	PointerMesh = MakeMesh(TEXT("Pointer"), false);
	PointerMesh->SetTranslucentSortPriority(1); // (over the ground's washes and the reach; under the table's signs)
	MovesMesh = MakeMesh(TEXT("EnemyMoves"), false); // (release 2, the enemy's month near you)
	MovesMesh->SetTranslucentSortPriority(1);

	auto MakeMarkers = [this](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Markers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Markers->SetupAttachment(Root);
		Markers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Markers->SetCastShadow(true);
		return Markers;
	};
	TownMarkers = MakeMarkers(TEXT("TownMarkers"));
	CapitalMarkers = MakeMarkers(TEXT("CapitalMarkers"));
}

bool ACampaignMapActor::BuildMap()
{
	TheaterSpeed::ApplyCommandLineSets(); // ("-TheaterSet=": before anything of the table reads a switch)
	const double StartTime = FPlatformTime::Seconds();

	FString Error;
	if (!Data.Load(Error))
	{
		UE_LOG(LogTheater, Error, TEXT("The campaign map could not be built: %s"), *Error);
		return false;
	}

	// The land does not cast shadows of its own: its relief is already shaded into its paint, and having
	// a model this large cast shadows every frame costs about a seventh of the frame for no visible gain.
	// The pieces and town markers still cast theirs onto it. "-TheaterLandShadows" turns it back on to compare.
	bLandShadows = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandShadows"));

	LoadMaterials();
	Terrain.Build(Data);
	PaintGround();
	PrepareLand();
	BuildSeaMesh();
	BuildWorld();
	BuildBorders();
	// (Round a world the table is a chart table and the chart is on rollers: the wood, the brass lip and the sheet's
	// margin lie to north and south of it only, and nothing ever ends it to east or west. The things left lying on
	// the table are made for a map with four edges, and are not laid out round a world.)
	BuildTable();
	if (!Data.bWorld)
	{
		BuildWarRoom();
	}
	BuildSheetMargin();
	BuildTowns();
	BuildParade();
	BuildTrees();
	BuildLabels();
	if (FBattleEffects::WantsTest() && !Blasts.IsValid())
	{
		// (the effects kit's test barrage: see Blasts)
		double MapUnitsAMetre = 0.34;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastScale="), MapUnitsAMetre);
		Blasts = MakeUnique<FBattleEffects>();
		BlastUnitsAMetre = FMath::Max(MapUnitsAMetre, 0.01) * UnitScale;
		// ("-TheaterBlastOff=0,60,60" has a third number: everything is drawn that many metres lower, so that what the
		// kit hangs 60 metres up, its flak burst, stands at the ground where the campaign's eye can come close to it.)
		double Lowered = 0.0;
		{
			FString Asked;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastOff="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 3)
			{
				Lowered = FCString::Atod(*Parts[2]) * BlastUnitsAMetre;
			}
		}
		const bool bMade = Blasts->Begin(Root, BlastUnitsAMetre, [this, Lowered](const FVector& Over)
		{
			const FVector2D Under = WorldToMap(Over);
			return SurfacePoint(Under.X, Under.Y, 0.0) - FVector(0.0, 0.0, Lowered);
		});
		// (The kit is told the sun and the wind: smoke is lit on the sun's side, glows at its thin edges against a low
		// sun, and drifts with the wind. The campaign's sun is the room's key light, which does not move, and its wind
		// is the map's own, from the west. "-TheaterBlastSun=8,250" hands the kit another sun for a picture: so many
		// degrees up, and where on the compass it stands, here low in the west.)
		// (EVERY SCENE THAT MAKES A KIT MUST MAKE THESE TWO CALLS, the effects artist, 00:19Z on 19 November 2026: without
		// SetSun the smoke's lit side and its copper fringe follow the kit's own sun, 55 degrees up toward 218, whatever
		// the scene's is. The way toward the sun is minus the sun light's forward vector. Today the test barrage's is
		// the only kit the campaign makes: the map's own smoke and flak do not go through it.)
		FVector TowardSun = -FRotator(-42.0, 69.0, 0.0).Vector();
		{
			FString Asked;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastSun="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 2)
			{
				// (north is +X and east +Y: a compass bearing is a yaw from north round by east)
				TowardSun = FRotator(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), 0.0).Vector();
			}
		}
		if (bMade)
		{
			Blasts->SetSun(TowardSun);
			Blasts->SetWind(FVector(0.25, 0.9, 0.0).GetSafeNormal() * 3.0);
			// (The campaign lays see-through washes over its ground. A see-through mesh with no place in the drawing order
			// is sorted among them by distance, and the kit's has none: the first ten metres of its tracer, half a
			// metre over the ground, were drawn under a wash and did not show, over rails and over bare ground
			// alike. The kit's mesh is put over the washes, as the map's own smoke is. "-TheaterBlastOrder=0" leaves
			// it where it was, to show the difference.)
			int32 Order = 1;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastOrder="), Order);
			TArray<UProceduralMeshComponent*> Meshes;
			GetComponents<UProceduralMeshComponent>(Meshes);
			int32 Placed = 0;
			for (UProceduralMeshComponent* One : Meshes)
			{
				if (One && One->GetName().StartsWith(TEXT("BattleEffects")))
				{
					One->SetTranslucentSortPriority(Order);
					++Placed;
				}
			}
			UE_LOG(LogTheater, Display, TEXT("Effects: the kit's mesh is given place %d in the drawing order of see-through things (%d mesh)"), Order, Placed);
		}
		UE_LOG(LogTheater, Display, TEXT("Effects: the test barrage is %s, a metre drawn as %.2f of a map unit; the sun stands %.0f degrees up"), bMade ? TEXT("made ready") : TEXT("NOT made (its recipe or its material is missing)"),
			MapUnitsAMetre, FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(TowardSun.GetSafeNormal().Z, -1.0, 1.0))));
	}

	UE_LOG(LogTheater, Log, TEXT("Campaign table built in %.2f s: %d labels, %d army pieces"),
		FPlatformTime::Seconds() - StartTime, Labels.Num(), Pieces.Num());
	return true;
}

FVector ACampaignMapActor::MapToWorld(double X, double Y, double Height) const
{
	// North is +X and east is +Y, so the map reads the right way up to a camera looking along +X.
	return FVector((Data.Height * 0.5 - Y) * UnitScale, (X - Data.Width * 0.5) * UnitScale, Height * UnitScale * VerticalScale);
}

FVector2D ACampaignMapActor::WorldToMap(const FVector& World) const
{
	return FVector2D(World.Y / UnitScale + Data.Width * 0.5, Data.Height * 0.5 - World.X / UnitScale);
}

double ACampaignMapActor::WrapX(double X) const
{
	if (!Data.bWorld || Data.WorldWidth <= 0.0)
	{
		return X;
	}
	return X - FMath::FloorToDouble((X - Data.WorldMinX) / Data.WorldWidth) * Data.WorldWidth;
}

int32 ACampaignMapActor::TurnFor(double X, double EyeX) const
{
	return Data.bWorld && Data.WorldWidth > 0.0 ? FMath::RoundToInt32((EyeX - X) / Data.WorldWidth) : 0;
}

double ACampaignMapActor::LaidX(double X) const
{
	if (!Data.bFar || !Data.Charted.bIsValid || Data.WorldWidth <= 0.0)
	{
		return X;
	}
	return X - FMath::FloorToDouble((X - Data.Charted.Min.X) / Data.WorldWidth) * Data.WorldWidth;
}

bool ACampaignMapActor::OverSeam() const
{
	return Data.bFar && Data.Charted.bIsValid && Data.WorldWidth > 0.0 && CVarOverSeam.GetValueOnGameThread() != 0;
}

void ACampaignMapActor::EyeCameRound(const FVector& Focus)
{
	if (!Data.bFar)
	{
		return;
	}
	const bool bOverSeam = OverSeam();
	LastAskedFocus = Focus;
	TheaterSpeed::RoundWorld(Focus.Y, bOverSeam ? Data.WorldWidth * UnitScale : 0.0);
	for (AArmyPiece* Piece : Pieces)
	{
		if (Piece)
		{
			Piece->LookFrom(Focus.Y, bOverSeam);
		}
	}
	for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : FleetPieces)
	{
		if (Pair.Value)
		{
			Pair.Value->LookFrom(Focus.Y, bOverSeam);
		}
	}
	for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : AirPieces)
	{
		if (Pair.Value)
		{
			Pair.Value->LookFrom(Focus.Y, bOverSeam);
		}
	}
	SquaresRound(Focus); // (and the squares of woods and towns)
}

namespace
{
	/**
	 * (Release 2, the far side: seam-2.) How far from the chart's seam the highest view sees, in the map's units: the
	 * eye stands 3,000 units off at the top of its climb, and its picture is about that wide. What is laid nearer the
	 * seam than this is laid again a world away (LayOverSeam).
	 */
	const double SeamSight = 1700.0;
}

void ACampaignMapActor::LayOverSeam(UProceduralMeshComponent* Of)
{
	if (!Of)
	{
		return;
	}
	int32 Slot = SeamLaid.IndexOfByPredicate([Of](const TObjectPtr<UProceduralMeshComponent>& Each) { return Each.Get() == Of; });
	if (Slot == INDEX_NONE)
	{
		Slot = SeamLaid.Add(Of);
		SeamTwins.Add(nullptr);
		SeamStamps.Add(0);
		SeamCounts.Add(0);
	}
	UProceduralMeshComponent* Twin = SeamTwins[Slot].Get();
	// (Only a model that lies on the table as it was built: one that has been turned or grown is a thing that moves,
	// and is not laid twice.)
	const FTransform& Stands = Of->GetComponentTransform();
	if (!OverSeam() || !Stands.GetRotation().IsIdentity(1.0e-4) || !Stands.GetScale3D().Equals(FVector::OneVector, 1.0e-4))
	{
		if (Twin)
		{
			Twin->ClearAllMeshSections();
		}
		return;
	}
	// The seam at the chart's west end, the same seam a world on at its east end, and the sight of each: in the
	// model's own measure along the table's east.
	const double Span = Data.WorldWidth * UnitScale;
	const double West = MapToWorld(Data.Charted.Min.X, 0.0).Y - Stands.GetLocation().Y;
	const double WestSight = West + SeamSight * UnitScale, EastSight = West + Span - SeamSight * UnitScale;
	const int32 Sections = Of->GetNumSections();
	TArray<FVector> Points, Normals;
	TArray<int32> Corners, WestNumber, EastNumber;
	TArray<FVector2D> UV0, UV1, UV2, UV3;
	TArray<FColor> Inks;
	TArray<FProcMeshTangent> Tangents;
	for (int32 Section = 0; Section < Sections; ++Section)
	{
		const FProcMeshSection* Laid = Of->GetProcMeshSection(Section);
		Points.Reset();
		Normals.Reset();
		Corners.Reset();
		UV0.Reset();
		UV1.Reset();
		UV2.Reset();
		UV3.Reset();
		Inks.Reset();
		Tangents.Reset();
		if (Laid && Laid->ProcIndexBuffer.Num() >= 3 && Laid->ProcVertexBuffer.Num() > 0)
		{
			// (each point's number in the share: laid a world east, for the west end's; a world west, for the east end's)
			const int32 Count = Laid->ProcVertexBuffer.Num();
			WestNumber.Init(INDEX_NONE, Count);
			EastNumber.Init(INDEX_NONE, Count);
			auto Take = [&](TArray<int32>& Numbers, uint32 Point, double Shift) -> int32
			{
				int32& Number = Numbers[Point];
				if (Number == INDEX_NONE)
				{
					const FProcMeshVertex& From = Laid->ProcVertexBuffer[Point];
					Number = Points.Add(FVector(From.Position.X, From.Position.Y + Shift, From.Position.Z));
					Normals.Add(FVector(From.Normal));
					UV0.Add(FVector2D(From.UV0));
					UV1.Add(FVector2D(From.UV1));
					UV2.Add(FVector2D(From.UV2));
					UV3.Add(FVector2D(From.UV3));
					Inks.Add(From.Color);
					Tangents.Add(From.Tangent);
				}
				return Number;
			};
			for (int32 First = 0; First + 2 < Laid->ProcIndexBuffer.Num(); First += 3)
			{
				const uint32 A = Laid->ProcIndexBuffer[First], B = Laid->ProcIndexBuffer[First + 1], C = Laid->ProcIndexBuffer[First + 2];
				if (A >= static_cast<uint32>(Count) || B >= static_cast<uint32>(Count) || C >= static_cast<uint32>(Count))
				{
					continue;
				}
				// (a face is laid whole, by where its first corner lies)
				const double Along = Laid->ProcVertexBuffer[A].Position.Y;
				if (Along < WestSight)
				{
					Corners.Add(Take(WestNumber, A, Span));
					Corners.Add(Take(WestNumber, B, Span));
					Corners.Add(Take(WestNumber, C, Span));
				}
				else if (Along > EastSight)
				{
					Corners.Add(Take(EastNumber, A, -Span));
					Corners.Add(Take(EastNumber, B, -Span));
					Corners.Add(Take(EastNumber, C, -Span));
				}
			}
		}
		if (Corners.Num() == 0)
		{
			if (Twin && Section < Twin->GetNumSections())
			{
				Twin->ClearMeshSection(Section);
			}
			continue;
		}
		if (!Twin)
		{
			// (a still model's twin is a still model, and a plain one's plain: each is drawn as its model is)
			Twin = Of->IsA<UTheaterStillMesh>() ? static_cast<UProceduralMeshComponent*>(UTheaterStillMesh::Make(this, TEXT("OverSeam"))) : NewObject<UProceduralMeshComponent>(this);
			Twin->SetupAttachment(Root);
			Twin->bUseAsyncCooking = false;
			Twin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Twin->SetCastShadow(Of->CastShadow);
			Twin->SetTranslucentSortPriority(Of->TranslucencySortPriority);
			Twin->SetRelativeTransform(Of->GetRelativeTransform());
			Twin->RegisterComponent();
			Twin->SetVisibility(Of->IsVisible());
			SeamTwins[Slot] = Twin;
		}
		Twin->CreateMeshSection(Section, Points, Corners, Normals, UV0, UV1, UV2, UV3, Inks, Tangents, false);
		Twin->SetMaterial(Section, Of->GetMaterial(Section));
	}
	// (sections the model no longer has)
	for (int32 Section = Sections; Twin && Section < Twin->GetNumSections(); ++Section)
	{
		Twin->ClearMeshSection(Section);
	}
}

void ACampaignMapActor::SeamLaidAgain(const UProceduralMeshComponent* Of)
{
	const int32 Slot = SeamLaid.IndexOfByPredicate([Of](const TObjectPtr<UProceduralMeshComponent>& Each) { return Each.Get() == Of; });
	if (SeamCounts.IsValidIndex(Slot))
	{
		++SeamCounts[Slot];
	}
}

void ACampaignMapActor::KeepSeamTwins()
{
	// The models laid once that are not things that move: the lines and works of the table, and the washes and signs
	// laid for the player's hand. (Not the ground, the sea or the sky, which go round by themselves. Not what changes
	// each frame or moves, which is seen over the seam by other means (seam-4): smoke, fires, raids, searchlights,
	// trains, parked aircraft and sinking ships are shown in the turn of the world nearest the eye, where each is
	// drawn; the lights of the night have a whole twin, KeepLightsTwin. The woods and the towns are moved whole:
	// SquaresRound. Still laid once: the shadows of the clouds, which lie over the home map alone.)
	UProceduralMeshComponent* const Kept[] = {
		BorderMesh.Get(), RailMesh.Get(), RiverMesh.Get(), WorksMesh.Get(), WorksLeftMesh.Get(), CivicMesh.Get(), PrizesMesh.Get(), ScarsMesh.Get(),
		BuildingsMesh.Get(), TownMesh.Get(), TownFarMesh.Get(), AirSignMesh.Get(), AirPennantMesh.Get(), GunLineMesh.Get(), ColumnsMesh.Get(),
		LanesMesh.Get(), ChartMesh.Get(), WrecksMesh.Get(), WatchMesh.Get(), FrontLineMesh.Get(), EnemyReachMesh.Get(), CutLampMesh.Get(),
		RailLightMesh.Get(), OrdersMesh.Get(), HighlightMesh.Get(), MovesMesh.Get(), PointerMesh.Get(), WakeMesh.Get() };
	const bool bOverSeam = OverSeam();
	for (int32 Gone = 0; Gone < SeamLaid.Num(); ++Gone)
	{
		// (a model that is no more leaves no twin standing)
		if (!SeamLaid[Gone] && SeamTwins[Gone] && SeamTwins[Gone]->GetNumSections() > 0)
		{
			SeamTwins[Gone]->ClearAllMeshSections();
		}
	}
	for (UProceduralMeshComponent* Of : Kept)
	{
		if (!Of)
		{
			continue;
		}
		// What it looks like now: how many points and corners each section has, its ink, and a few of its points.
		uint32 Stamp = bOverSeam ? 2u : 1u;
		const int32 Sections = Of->GetNumSections();
		for (int32 Section = 0; Section < Sections; ++Section)
		{
			const FProcMeshSection* Laid = Of->GetProcMeshSection(Section);
			const int32 Count = Laid ? Laid->ProcVertexBuffer.Num() : 0;
			Stamp = HashCombine(Stamp, GetTypeHash(Count));
			Stamp = HashCombine(Stamp, GetTypeHash(Laid ? Laid->ProcIndexBuffer.Num() : 0));
			Stamp = HashCombine(Stamp, PointerHash(Of->GetMaterial(Section)));
			for (int32 Nth = 0; Nth < 8 && Count > 0; ++Nth)
			{
				const FProcMeshVertex& Sample = Laid->ProcVertexBuffer[static_cast<int32>(static_cast<int64>(Nth) * (Count - 1) / 7)];
				Stamp = HashCombine(Stamp, GetTypeHash(Sample.Position));
				Stamp = HashCombine(Stamp, GetTypeHash(Sample.Color));
			}
		}
		int32 Slot = SeamLaid.IndexOfByPredicate([Of](const TObjectPtr<UProceduralMeshComponent>& Each) { return Each.Get() == Of; });
		// (And how many times its builder has said it was laid again. A model may change and keep every count and
		// every point looked at: a gun pit that made way by a few degrees, an airfield drawn smaller when nobody
		// flies from it. A model with no twin yet counts as none, which is what its new twin starts from.)
		Stamp = HashCombine(Stamp, SeamCounts.IsValidIndex(Slot) ? SeamCounts[Slot] : 0u);
		if (Slot == INDEX_NONE || SeamStamps[Slot] != Stamp)
		{
			if (Slot == INDEX_NONE && !bOverSeam)
			{
				continue; // (never laid over the seam, and not asked to be)
			}
			LayOverSeam(Of);
			Slot = SeamLaid.IndexOfByPredicate([Of](const TObjectPtr<UProceduralMeshComponent>& Each) { return Each.Get() == Of; });
			if (Slot == INDEX_NONE)
			{
				continue;
			}
			SeamStamps[Slot] = Stamp;
		}
		// Its twin is shown, hidden and placed as it is.
		if (UProceduralMeshComponent* Twin = SeamTwins[Slot].Get())
		{
			if (Twin->IsVisible() != Of->IsVisible())
			{
				Twin->SetVisibility(Of->IsVisible());
			}
			if (!Twin->GetRelativeTransform().Equals(Of->GetRelativeTransform(), 0.01))
			{
				Twin->SetRelativeTransform(Of->GetRelativeTransform());
			}
			if (!Twin->IsA<UTheaterStillMesh>())
			{
				// (a plain model's sections may be shown and hidden one by one; a still model's never are)
				for (int32 Section = 0; Section < Sections && Section < Twin->GetNumSections(); ++Section)
				{
					const bool bShown = Of->IsMeshSectionVisible(Section);
					if (Twin->IsMeshSectionVisible(Section) != bShown)
					{
						Twin->SetMeshSectionVisible(Section, bShown);
					}
				}
			}
		}
	}
}

void ACampaignMapActor::FillLightsTwin()
{
	bLightsTwinStale = false;
	if (!LightsTwin || !LightsMesh)
	{
		return;
	}
	// Each section of the lights as it is lit now: its points and their light, and (when it is laid for the first
	// time, or has changed its size) its corners.
	const int32 Sections = LightsMesh->GetNumSections();
	TArray<FVector> Points;
	TArray<FColor> Inks;
	for (int32 Section = 0; Section < Sections; ++Section)
	{
		const FProcMeshSection* Lit = LightsMesh->GetProcMeshSection(Section);
		const FProcMeshSection* Mine = Section < LightsTwin->GetNumSections() ? LightsTwin->GetProcMeshSection(Section) : nullptr;
		const int32 Count = Lit ? Lit->ProcVertexBuffer.Num() : 0;
		if (Count == 0 || Lit->ProcIndexBuffer.Num() < 3)
		{
			if (Mine && Mine->ProcVertexBuffer.Num() > 0)
			{
				LightsTwin->ClearMeshSection(Section);
			}
			continue;
		}
		Points.SetNumUninitialized(Count);
		Inks.SetNumUninitialized(Count);
		for (int32 Point = 0; Point < Count; ++Point)
		{
			Points[Point] = FVector(Lit->ProcVertexBuffer[Point].Position);
			Inks[Point] = Lit->ProcVertexBuffer[Point].Color;
		}
		if (Mine && Mine->ProcVertexBuffer.Num() == Count && Mine->ProcIndexBuffer.Num() == Lit->ProcIndexBuffer.Num())
		{
			// (the same lamps at another hour: their light alone is laid again, as the model's own is)
			LightsTwin->UpdateMeshSection(Section, Points, TArray<FVector>(), TArray<FVector2D>(), Inks, TArray<FProcMeshTangent>());
			continue;
		}
		TArray<int32> Corners;
		Corners.Reserve(Lit->ProcIndexBuffer.Num());
		for (const uint32 Corner : Lit->ProcIndexBuffer)
		{
			Corners.Add(static_cast<int32>(Corner));
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Count);
		LightsTwin->CreateMeshSection(Section, Points, Corners, Normals, TArray<FVector2D>(), Inks, TArray<FProcMeshTangent>(), false);
		LightsTwin->SetMaterial(Section, LightsMesh->GetMaterial(Section));
	}
	// (sections the lights no longer have)
	for (int32 Section = Sections; Section < LightsTwin->GetNumSections(); ++Section)
	{
		LightsTwin->ClearMeshSection(Section);
	}
}

void ACampaignMapActor::KeepLightsTwin(const FVector& Focus)
{
	// Which side the twin stands on. The eye near the chart's west end sees, over the seam, what lies at the chart's
	// east end: a world to the west of where that was built. Near the east end it sees the west end's, a world to the
	// east. (The eye is kept within one turn of the chart by its own camera.) By day, and away from the seam, there
	// is nothing of the twin to see, and nothing of it is drawn or filled.
	double Aside = 0.0;
	if (OverSeam() && LightsMesh && LightsMesh->IsVisible() && LightsMesh->GetNumSections() > 0)
	{
		const double Span = Data.WorldWidth * UnitScale;
		const double FromWest = Focus.Y - MapToWorld(Data.Charted.Min.X, 0.0).Y;
		Aside = FromWest < SeamSight * UnitScale ? -Span : FromWest > Span - SeamSight * UnitScale ? Span : 0.0;
	}
	if (Aside == 0.0)
	{
		if (LightsTwin && LightsTwin->IsVisible())
		{
			LightsTwin->SetVisibility(false);
		}
		return;
	}
	if (!LightsTwin)
	{
		LightsTwin = NewObject<UProceduralMeshComponent>(this);
		LightsTwin->SetupAttachment(Root);
		LightsTwin->bUseAsyncCooking = false;
		LightsTwin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LightsTwin->SetCastShadow(false);
		LightsTwin->SetTranslucentSortPriority(LightsMesh->TranslucencySortPriority);
		LightsTwin->RegisterComponent();
		LightsTwin->SetVisibility(false);
		bLightsTwinStale = true;
	}
	if (bLightsTwinStale)
	{
		FillLightsTwin();
	}
	// It stands as the lights do, a world aside; each of its sections is shown as theirs is (the windows are put out
	// from high up, where the towns are drawn in their far form); and it is shown.
	const FVector Stands = LightsMesh->GetRelativeLocation() + FVector(0.0, Aside, 0.0);
	if (!LightsTwin->GetRelativeLocation().Equals(Stands, 0.5))
	{
		LightsTwin->SetRelativeLocation(Stands);
	}
	for (int32 Section = 0; Section < LightsMesh->GetNumSections() && Section < LightsTwin->GetNumSections(); ++Section)
	{
		const bool bShown = LightsMesh->IsMeshSectionVisible(Section);
		if (LightsTwin->IsMeshSectionVisible(Section) != bShown)
		{
			LightsTwin->SetMeshSectionVisible(Section, bShown);
		}
	}
	if (!LightsTwin->IsVisible())
	{
		LightsTwin->SetVisibility(true);
	}
}

void ACampaignMapActor::SquaresRound(const FVector& Focus)
{
	const double Span = OverSeam() ? Data.WorldWidth * UnitScale : 0.0;
	auto Beside = [&](UProceduralMeshComponent* Square)
	{
		if (!Square)
		{
			return;
		}
		// (its own middle as it was built: where it is shown now, less what it has been moved by)
		const FVector Moved = Square->GetRelativeLocation();
		const double Middle = Square->Bounds.Origin.Y - Moved.Y;
		const double Round = Span > 0.0 ? FMath::RoundToDouble((Focus.Y - Middle) / Span) * Span : 0.0;
		if (FMath::Abs(Moved.Y - Round) > 0.5)
		{
			Square->SetRelativeLocation(FVector(Moved.X, Round, Moved.Z));
		}
	};
	for (const TObjectPtr<UProceduralMeshComponent>& Square : TreeMeshes)
	{
		Beside(Square.Get());
	}
	for (const TObjectPtr<UProceduralMeshComponent>& Square : TownSquares)
	{
		Beside(Square.Get());
	}
	for (const TObjectPtr<UProceduralMeshComponent>& Square : TownFarSquares)
	{
		Beside(Square.Get());
	}
}

double ACampaignMapActor::NearX(double X, double BesideX) const
{
	return Data.bFar && Data.WorldWidth > 0.0 ? X + FMath::RoundToDouble((BesideX - X) / Data.WorldWidth) * Data.WorldWidth : X;
}

FVector ACampaignMapActor::BesideEye(const FVector& Place) const
{
	if (!OverSeam())
	{
		return Place;
	}
	const double Span = Data.WorldWidth * UnitScale;
	return FVector(Place.X, Place.Y + FMath::RoundToDouble((LastAskedFocus.Y - Place.Y) / Span) * Span, Place.Z);
}

void ACampaignMapActor::RoundPiece(AArmyPiece* Piece) const
{
	if (Piece && Data.bFar && Data.Charted.bIsValid && Data.WorldWidth > 0.0)
	{
		Piece->KeepRound(MapToWorld(Data.Charted.Min.X, 0.0).Y, Data.WorldWidth * UnitScale);
		Piece->LookFrom(LastAskedFocus.Y, OverSeam()); // (seam-2: shown on the eye's side from its first frame; UpdateForCamera says so each frame after)
	}
}

float ACampaignMapActor::GroundAt(double InX, double Y) const
{
	// The map's own ground where it reaches; beyond it a fine sheet's where one stands, and the world's coarse sheet's elsewhere.
	const double X = WrapX(InX);
	if (Data.bWorld && (X < 0.0 || Y < 0.0 || X > Data.Width || Y > Data.Height))
	{
		if (const FFineSheet* Fine = FineSheetAt(X, Y))
		{
			return Fine->Shape.HeightAt(X, Y);
		}
		// (Where no fine sheet stands, the height one would give: what is built once, a town, a river, a railway,
		// then sits on the ground the eye sees from near, whether the fine sheet stood when it was built or not.
		// The coarse sheet's own height stands for a place by the mean of the ground round it, and in the hills a
		// town built on that was buried or left in the air when the fine ground came up. Within a few units of a
		// shore the coarse sheet's height is kept: the fine one cannot be had there without the fine sheet's coast.)
		const float Coarse = WorldTerrain.HeightAt(X, Y);
		const float Inland = WorldTerrain.NX > 0 ? FMath::SmoothStep(6.0f, 12.0f, WorldTerrain.ShoreAt(X, Y)) : 0.0f;
		return Inland > 0.0f ? FMath::Lerp(Coarse, WorldTerrain.FineLandHeightAt(X, Y), Inland) : Coarse;
	}
	return Terrain.HeightAt(X, Y);
}

float ACampaignMapActor::CoverAt(double InX, double Y) const
{
	const double X = WrapX(InX);
	if (Data.bWorld && (X < 0.0 || Y < 0.0 || X > Data.Width || Y > Data.Height))
	{
		if (const FFineSheet* Fine = FineSheetAt(X, Y))
		{
			return Fine->Shape.CoverageAt(X, Y);
		}
		return WorldTerrain.CoverageAt(X, Y);
	}
	return Terrain.CoverageAt(X, Y);
}

void ACampaignMapActor::ShowWorksGoing(const TArray<int32>& FromRules)
{
	THEATER_SPEED_SCOPE(ShowWorksGoing);
	// Works on order in a province: pairs, the province and what. A builder's yard stands there while they go up.
	TMap<int32, int32> Now;
	for (int32 K = 0; K + 1 < FromRules.Num(); K += 2)
	{
		if (Data.Regions.IsValidIndex(FromRules[K]) && FromRules[K + 1] != 0)
		{
			Now.Add(FromRules[K], FromRules[K + 1]);
		}
	}
	// ("-TheaterGoing=Berlin,Hamburg" stands a builder's yard there for a picture)
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterGoing="), Asked, false))
	{
		TArray<FString> Places;
		Asked.ParseIntoArray(Places, TEXT(","));
		for (const FString& Place : Places)
		{
			if (const int32* Id = Data.RegionByName.Find(Place))
			{
				Now.Add(*Id, 1);
			}
		}
	}
	if (Now.OrderIndependentCompareEqual(GoingIn))
	{
		return;
	}
	GoingIn = MoveTemp(Now);
	ShownBuildings.Reset(); // (the provinces' works are stood again when next they are shown, which is at once)
	if (LastWorks.Num() > 0)
	{
		const TArray<FIntVector4> Again = LastWorks;
		ShowBuildings(Again);
	}
}

void ACampaignMapActor::ShowVictorFlags()
{
	THEATER_SPEED_SCOPE(ShowVictorFlags);
	// The victor's flag over a captured capital: where a capital is held by another than the country whose it was
	// when the map was drawn, the holder's flag flies from the top of its great building. A small thing: a pole
	// and a cloth under two houses long, read from working height as a patch of the victor's colours over the roofs.
	TMap<int32, FString> Now;
	for (const TPair<int32, FVector>& Top : TownLandmarks)
	{
		if (Data.Regions.IsValidIndex(Top.Key))
		{
			const FCampaignRegion& Region = Data.Regions[Top.Key];
			if (Region.bCapital && !Region.Home.IsEmpty() && !Region.Owner.IsEmpty() && Region.Owner != Region.Home)
			{
				Now.Add(Top.Key, Region.Owner);
			}
		}
	}
	// ("-TheaterVictor=Berlin:SOV,Paris:GER" flies one for a picture)
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterVictor="), Asked, false))
	{
		TArray<FString> Places;
		Asked.ParseIntoArray(Places, TEXT(","));
		for (const FString& Place : Places)
		{
			FString Name, Tag;
			if (Place.Split(TEXT(":"), &Name, &Tag))
			{
				const int32* Id = Data.RegionByName.Find(Name);
				if (Id && TownLandmarks.Contains(*Id))
				{
					Now.Add(*Id, Tag);
				}
			}
		}
	}
	// The flag changes where it can be seen to: a capital that falls has the victor's flag run up over it, one that
	// passes from one victor to another has the first hauled down and then the second run up, and one that comes
	// home has the victor's hauled down and the pole taken away. (What stands when the table is first laid stands
	// at once: only a change made while the table is being watched is acted out.)
	const double Clock = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bool bChanged = bVictorsStale || !VictorFlagMesh;
	for (const TPair<int32, FString>& Flown : Now)
	{
		FVictorFlag* Had = VictorFlags.Find(Flown.Key);
		if (!Had)
		{
			FVictorFlag Fresh;
			Fresh.Tag = Flown.Value;
			Fresh.Since = bVictorsSeen ? Clock : -1000.0;
			VictorFlags.Add(Flown.Key, Fresh);
			bChanged = true;
		}
		else if (Had->Tag != Flown.Value)
		{
			Had->Was = bVictorsSeen ? Had->Tag : FString();
			Had->Tag = Flown.Value;
			Had->Since = bVictorsSeen ? Clock : -1000.0;
			bChanged = true;
		}
	}
	for (auto It = VictorFlags.CreateIterator(); It; ++It)
	{
		if (!Now.Contains(It.Key()) && !It.Value().Tag.IsEmpty())
		{
			if (bVictorsSeen && TownLandmarks.Contains(It.Key()))
			{
				It.Value().Was = It.Value().Tag;
				It.Value().Tag.Reset();
				It.Value().Since = Clock;
			}
			else
			{
				It.RemoveCurrent();
			}
			bChanged = true;
		}
	}
	bVictorsSeen |= TownLandmarks.Num() > 0;
	if (!bChanged)
	{
		return;
	}
	bVictorsStale = false;
	DrawVictorFlags();
}

void ACampaignMapActor::DrawVictorFlags()
{
	THEATER_SPEED_SCOPE(DrawVictorFlags);
	if (!VictorFlagMesh)
	{
		VictorFlagMesh = NewObject<UProceduralMeshComponent>(this);
		VictorFlagMesh->SetupAttachment(Root);
		VictorFlagMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		VictorFlagMesh->RegisterComponent();
		VictorFlagMesh->SetCastShadow(false);
	}
	VictorFlagMesh->ClearAllMeshSections();
	bVictorsMoving = false;
	const double Clock = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const double Unit = UnitScale * 2.1; // the size the towns are built to
	const double PoleHigh = 1.25 * Unit, Wide = 1.15 * Unit, High = 0.72 * Unit;
	const double ComesDown = 1.2, GoesUp = 2.0; // seconds
	FMiniatureMesh Poles;
	int32 Section = 1;
	for (auto It = VictorFlags.CreateIterator(); It; ++It)
	{
		const FVictorFlag& Flag = It.Value();
		const FVector* Stands = TownLandmarks.Find(It.Key());
		if (!Stands)
		{
			It.RemoveCurrent();
			continue;
		}
		const double Passed = Clock - Flag.Since, Lowering = Flag.Was.IsEmpty() ? 0.0 : ComesDown;
		FString Shown;
		double Share = 1.0;
		if (Passed < Lowering)
		{
			Shown = Flag.Was;
			Share = 1.0 - Passed / Lowering;
			bVictorsMoving = true;
		}
		else if (!Flag.Tag.IsEmpty())
		{
			Shown = Flag.Tag;
			Share = FMath::Clamp((Passed - Lowering) / GoesUp, 0.0, 1.0);
			bVictorsMoving |= Share < 1.0;
		}
		else
		{
			It.RemoveCurrent(); // (hauled down, and the capital its own country's again)
			continue;
		}
		// ("-TheaterVictorAt=0.5" holds every such flag half way up its pole, for a picture)
		float Held = 1.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterVictorAt="), Held))
		{
			Share = FMath::Clamp(static_cast<double>(Held), 0.0, 1.0);
		}
		const FVector Foot = *Stands;
		Poles.Pillar(Foot - FVector(0.0, 0.0, 0.1 * Unit), 0.03 * Unit, 0.022 * Unit, PoleHigh + 0.1 * Unit, FLinearColor(FColor::FromHex(TEXT("2A2724"))), 5);
		Poles.Ball(Foot + FVector(0.0, 0.0, PoleHigh + 0.04 * Unit), FVector(0.1 * Unit), FLinearColor(FColor::FromHex(TEXT("B08D57"))), 4, 6);
		UMaterialInterface* Colours = GetFlagMaterial(Shown);
		if (!Colours)
		{
			continue;
		}
		// (the cloth, as an army's standard hangs: read from the south, the pole on the reader's left, a slow ripple
		// in it that grows toward the fly; on its way up or down the pole it is so far up it)
		const double Top = FMath::Lerp(High + 0.06 * Unit, PoleHigh, FMath::SmoothStep(0.0, 1.0, Share));
		const int32 Across = 10, Down = 4;
		const double Phase = (It.Key() * 37 % 100) * 0.0628;
		TArray<FVector> Vertices, Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Triangles;
		for (int32 J = 0; J <= Down; ++J)
		{
			for (int32 I = 0; I <= Across; ++I)
			{
				const double S = static_cast<double>(I) / Across, T = static_cast<double>(J) / Down;
				const double Angle = UE_DOUBLE_TWO_PI * 1.2 * S + Phase + 0.8 * T;
				Vertices.Add(Foot + FVector(0.05 * Wide * S * FMath::Sin(Angle), 0.03 * Unit + S * Wide, Top - T * High - 0.03 * High * S * S));
				Normals.Add(FVector(-1.0, 0.0, 0.0));
				Tangents.Add(FProcMeshTangent(FVector(0.0, 1.0, 0.0), false));
				UVs.Add(FVector2D(S, T));
			}
		}
		for (int32 J = 0; J < Down; ++J)
		{
			for (int32 I = 0; I < Across; ++I)
			{
				const int32 A = J * (Across + 1) + I, B = A + 1, C = A + Across + 1, D = C + 1;
				Triangles.Append({ A, C, B, B, C, D });
			}
		}
		VictorFlagMesh->CreateMeshSection_LinearColor(Section, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), Tangents, false);
		VictorFlagMesh->SetMaterial(Section, Colours);
		++Section;
	}
	if (Poles.Vertices.Num() > 0)
	{
		VictorFlagMesh->CreateMeshSection(0, Poles.Vertices, Poles.Triangles, Poles.Normals, TArray<FVector2D>(), Poles.Colors, TArray<FProcMeshTangent>(), false);
		VictorFlagMesh->SetMaterial(0, SceneryMaterial);
	}
}

void ACampaignMapActor::ShowFrontLine(const TArray<FString>& Wars, const FString& Player)
{
	THEATER_SPEED_SCOPE(ShowFrontLine);
	if (Wars == WarPairs && Player == WarPlayer && FrontLineMesh)
	{
		return;
	}
	WarPairs = Wars;
	WarPlayer = Player;
	WarSet.Reset();
	WarFoes.Reset();
	for (int32 K = 0; K + 1 < Wars.Num(); K += 2)
	{
		WarSet.Add(Wars[K] + TEXT("|") + Wars[K + 1]);
		WarSet.Add(Wars[K + 1] + TEXT("|") + Wars[K]);
		if (Wars[K] == Player)
		{
			WarFoes.Add(Wars[K + 1]);
		}
		else if (Wars[K + 1] == Player)
		{
			WarFoes.Add(Wars[K]);
		}
	}
	BuildFrontLine();
}

void ACampaignMapActor::FindSharedEdges()
{
	if (bSharedEdgesKnown)
	{
		return;
	}
	// Every stretch of line two provinces share, found once: what lies just across each edge of each ring.
	bSharedEdgesKnown = true;
	SharedEdges.Reset();
	for (const FCampaignRegion& Region : Data.Regions)
	{
		for (const TArray<FVector2D>& Ring : Region.Polygons)
		{
			double SignedArea = 0.0;
			for (int32 i = 0, n = Ring.Num(); i < n; ++i)
			{
				SignedArea += Ring[i].X * Ring[(i + 1) % n].Y - Ring[(i + 1) % n].X * Ring[i].Y;
			}
			const double Outward = SignedArea >= 0.0 ? 1.0 : -1.0;
			for (int32 i = 0, n = Ring.Num(); i < n; ++i)
			{
				const FVector2D A = Ring[i], B = Ring[(i + 1) % n];
				const double Length = FVector2D::Distance(A, B);
				if (Length < 0.05)
				{
					continue;
				}
				const FVector2D Dir = (B - A) / Length, Side = FVector2D(Dir.Y, -Dir.X) * Outward;
				const FVector2D Probe = (A + B) * 0.5 + Side * 0.35;
				const int32 Other = Data.FindRegionAt(Probe.X, Probe.Y);
				if (Other == INDEX_NONE || Other == Region.Id || Other < Region.Id)
				{
					continue; // coast, or a line the neighbour has already given
				}
				FSharedEdge& Edge = SharedEdges.AddDefaulted_GetRef();
				Edge.A = A;
				Edge.B = B;
				Edge.Side = Side;
				Edge.Here = Region.Id;
				Edge.There = Other;
			}
		}
	}
}

void ACampaignMapActor::BuildFrontLine()
{
	FrontLineDue = 0; // (a line put off by a press is this one: where a war begins in the same answer it is laid at once, and not again)
	THEATER_SPEED_SCOPE(BuildFrontLine);
	// (Speed, 18 November 2026: tc.Speed.FrontWeights. The line is laid in two weights, and only one is ever shown:
	// 47 ms for both on the take of Buffalo, in the frame of the take, most of it the barbs, which all move along the
	// front when any stretch of it does. The weight the eye is at is laid now and the other a frame after, or at once
	// if the eye goes to it first; the same line.)
	FrontLineOwed = INDEX_NONE;
	if (!TheaterSpeed::FrontWeights())
	{
		LayFrontLine(INDEX_NONE, false);
		return;
	}
	const int32 Wanted = FrontLineSeenFrom > 9000.0f ? 1 : 0; // (as UpdateForCamera will choose once it is laid, from the working weight)
	if (LayFrontLine(Wanted, false))
	{
		FrontLineOwed = 1 - Wanted;
		FrontLineOwedFrame = GFrameCounter;
	}
}

bool ACampaignMapActor::LayFrontLine(int32 Only, bool bOwed)
{
	// The front, drawn as a staff officer draws it: a crayon line along every frontier between the holdings of two
	// countries at war. Where the war is the player's (the country beyond is one he is at war with, and the country
	// on this side is not) the line is vermilion over a dark edge, with small barbs along it that point at the
	// enemy. Where two others fight, it is a thinner buff line with no barbs. It lies on the ground with the other
	// inked lines, under every piece and flag, and it is laid in two weights: one for working height, and a
	// heavier one that can be read from the staff map's height (see UpdateForCamera).
	// (Only: one weight alone, 0 or 1, or INDEX_NONE for both. bOwed: the weight left for a frame after, laid into its
	// own section beside the other, which stands.)
	if (!Data.IsLoaded())
	{
		return false;
	}
	if (!FrontLineMesh)
	{
		FrontLineMesh = NewObject<UProceduralMeshComponent>(this);
		FrontLineMesh->SetupAttachment(Root);
		FrontLineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FrontLineMesh->RegisterComponent();
		FrontLineMesh->SetCastShadow(false);
	}
	if (bOwed)
	{
		FrontLineMesh->ClearMeshSection(Only);
	}
	else
	{
		FrontLineMesh->ClearAllMeshSections();
		FrontLineFar = -1;
	}
	if (WarSet.Num() == 0 || FParse::Param(FCommandLine::Get(), TEXT("TheaterNoFrontLine")))
	{
		return false;
	}
	FindSharedEdges();
	// ---- (release 2, the front you can read) The line chained, rounded and drawn as a map-maker's pen draws it: see
	// LayPenFront. "-TheaterFrontCrayon" lays release 1's crayon line below instead, to set the two side by side.
	static const bool bCrayon = FParse::Param(FCommandLine::Get(), TEXT("TheaterFrontCrayon"));
	if (!bCrayon)
	{
		return LayPenFront(Only, bOwed);
	}
	// ---- (end release 2, the front you can read)
	const FColor Vermilion = FromHex(TEXT("A63A2C")).ToFColor(true), Edging = FromHex(TEXT("2A1410")).ToFColor(true);
	const FColor Buff = FromHex(TEXT("8A8172")).ToFColor(true), BuffEdging = FromHex(TEXT("2A2420")).ToFColor(true);
	FCampaignMeshBuffers Weights[2];
	static const double Heavier[2] = { 1.0, 2.7 };
	int32 Mine = 0, Theirs = 0;
	double ToBarb = 1.1;
	// (Speed, 17 November 2026: the holder of each province as a number, and the pairs at war as numbers. Every edge
	// of the world was having a name made of its two holders to look the war up, each time a province changed hands.)
	TMap<FString, int32> Numbers;
	TArray<int32> Holder;
	Holder.SetNumUninitialized(Data.Regions.Num());
	for (int32 Each = 0; Each < Data.Regions.Num(); ++Each)
	{
		const int32* Known = Numbers.Find(Data.Regions[Each].Owner);
		Holder[Each] = Known ? *Known : Numbers.Add(Data.Regions[Each].Owner, Numbers.Num());
	}
	TSet<uint32> PairsAtWar;
	for (const FString& Pair : WarSet)
	{
		FString One, Other;
		if (Pair.Split(TEXT("|"), &One, &Other))
		{
			const int32* A = Numbers.Find(One);
			const int32* B = Numbers.Find(Other);
			if (A && B)
			{
				PairsAtWar.Add((static_cast<uint32>(*A) << 16) | static_cast<uint32>(*B));
			}
		}
	}
	TArray<bool> IsFoe;
	IsFoe.Init(false, Numbers.Num());
	for (const TPair<FString, int32>& Each : Numbers)
	{
		IsFoe[Each.Value] = WarFoes.Contains(Each.Key);
	}
	for (const FSharedEdge& Edge : SharedEdges)
	{
		const int32 Here = Holder[Edge.Here], There = Holder[Edge.There];
		if (Here == There || !PairsAtWar.Contains((static_cast<uint32>(Here) << 16) | static_cast<uint32>(There)))
		{
			continue;
		}
		const bool bHereFoe = IsFoe[Here], bThereFoe = IsFoe[There];
		const bool bMine = bHereFoe != bThereFoe;
		(bMine ? Mine : Theirs)++;
		const FVector2D Toward = bThereFoe ? Edge.Side : -Edge.Side; // (toward the player's enemy)
		const double Length = FVector2D::Distance(Edge.A, Edge.B);
		const FVector2D Dir = (Edge.B - Edge.A) / Length;
		// (where the barbs stand along this edge: evenly along the whole front, whatever the lengths of its edges)
		TArray<double, TInlineAllocator<8>> Barbs;
		if (bMine)
		{
			for (double Along = ToBarb; Along < Length; Along += 2.2)
			{
				Barbs.Add(Along);
			}
			ToBarb = Barbs.Num() > 0 ? Barbs.Last() + 2.2 - Length : ToBarb - Length;
		}
		for (int32 Weight = 0; Weight < 2; ++Weight)
		{
			if (Only != INDEX_NONE && Weight != Only)
			{
				continue;
			}
			FCampaignMeshBuffers& Into = Weights[Weight];
			const double W = Heavier[Weight] * (bMine ? 1.0 : 0.62);
			auto Ribbon = [&](double HalfWidth, double Lift, const FColor& Ink)
			{
				const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Length / 1.4));
				int32 PrevLeft = INDEX_NONE, PrevRight = INDEX_NONE;
				for (int32 k = 0; k <= Steps; ++k)
				{
					const double T = k == 0 ? -HalfWidth * 0.6 : (k == Steps ? Length + HalfWidth * 0.6 : Length * k / Steps);
					const FVector2D P = Edge.A + Dir * T;
					const FVector2D L = P + Edge.Side * HalfWidth, R = P - Edge.Side * HalfWidth;
					const int32 Left = Into.AddVertex(SurfacePointKept(L.X, L.Y, Lift), FVector::UpVector, Ink);
					const int32 Right = Into.AddVertex(SurfacePointKept(R.X, R.Y, Lift), FVector::UpVector, Ink);
					if (PrevLeft != INDEX_NONE)
					{
						Into.AddTriangleFacingUp(PrevLeft, PrevRight, Left);
						Into.AddTriangleFacingUp(PrevRight, Left, Right);
					}
					PrevLeft = Left;
					PrevRight = Right;
				}
			};
			Ribbon(0.36 * W, 0.16, bMine ? Edging : BuffEdging);
			Ribbon(0.22 * W, 0.2, bMine ? Vermilion : Buff);
			for (const double Along : Barbs)
			{
				// (a barb: a small dark-edged tooth of the same crayon, standing off the line toward the enemy)
				const FVector2D Foot = Edge.A + Dir * Along;
				auto Tooth = [&](double Base, double Reach, double Lift, const FColor& Ink)
				{
					const FVector2D P0 = Foot - Dir * Base, P1 = Foot + Dir * Base, P2 = Foot + Toward * Reach;
					const int32 I0 = Into.AddVertex(SurfacePointKept(P0.X, P0.Y, Lift), FVector::UpVector, Ink);
					const int32 I1 = Into.AddVertex(SurfacePointKept(P1.X, P1.Y, Lift), FVector::UpVector, Ink);
					const int32 I2 = Into.AddVertex(SurfacePointKept(P2.X, P2.Y, Lift), FVector::UpVector, Ink);
					Into.AddTriangleFacingUp(I0, I1, I2);
				};
				Tooth(0.5 * W, 0.98 * W, 0.17, Edging);
				Tooth(0.34 * W, 0.74 * W, 0.21, Vermilion);
			}
		}
	}
	{
		THEATER_SPEED_SCOPE(FrontLineCommit);
		for (int32 Weight = 0; Weight < 2; ++Weight)
		{
			if (Only == INDEX_NONE || Weight == Only)
			{
				Weights[Weight].Commit(FrontLineMesh, Weight, InkMaterial);
			}
		}
	}
	if (bOwed)
	{
		FrontLineMesh->SetMeshSectionVisible(Only, FrontLineFar == Only); // (shown if the eye is at it)
		return true;
	}
	FrontLineMesh->SetMeshSectionVisible(1, false);
	FrontLineFar = 0;
	UE_LOG(LogTheater, Log, TEXT("The front line: %d stretches of the player's own fronts, %d of others' (%d wars)"), Mine, Theirs, WarPairs.Num() / 2);
	return true;
}

// ---- (release 2, the front you can read) ----
bool ACampaignMapActor::LayPenFront(int32 Only, bool bOwed)
{
	// (Item 4 of the campaign map plan. The same front as release 1's crayon, between the holdings of two countries
	// at war, now found to be whole lines (Campaign/FrontLine.h: chained through the meetings of three borders and
	// round an enclave, thinned, its corners rounded within a unit of the border) and drawn once as a map-maker's pen
	// draws it: dark ink that swells and thins a little, a soft shadow under it, and on the player's own fronts small
	// teeth toward his enemy. Laid only when ground changes hands or a war begins or ends, never in a frame for its
	// own sake; the weight the eye is not at is laid a frame later, as before (tc.Speed.FrontWeights). The ink lies
	// in sections 0 and 1 (the working weight, the staff map's) and its shadow in 2 and 3, under every piece and name.)
	THEATER_SPEED_SCOPE(LayPenFront);
	if (!PenFront.IsValid())
	{
		PenFront = MakeShared<FPenFront>();
	}
	FPenFront& Pen = *PenFront;
	if (bOwed)
	{
		FrontLineMesh->ClearMeshSection(Only + 2);
	}
	else
	{
		// The stretches of border, once: the table's outlines never change, only who holds the ground either side.
		if (!Pen.bPiecesKnown)
		{
			Pen.bPiecesKnown = true;
			Pen.Pieces.clear();
			Pen.Pieces.reserve(static_cast<size_t>(SharedEdges.Num()));
			for (const FSharedEdge& Shared : SharedEdges)
			{
				TheaterFront::FBorderPiece& Made = Pen.Pieces.emplace_back();
				Made.A = TheaterFront::FPoint{ Shared.A.X, Shared.A.Y };
				Made.B = TheaterFront::FPoint{ Shared.B.X, Shared.B.Y };
				Made.Side = TheaterFront::FPoint{ Shared.Side.X, Shared.Side.Y };
				Made.Here = Shared.Here;
				Made.There = Shared.There;
			}
		}
		// Who holds each province, as a number; the wars; whom the player is at war with.
		TMap<FString, int32> HolderNumbers;
		for (const FCampaignRegion& Region : Data.Regions)
		{
			if (!HolderNumbers.Contains(Region.Owner))
			{
				HolderNumbers.Add(Region.Owner, HolderNumbers.Num());
			}
		}
		Pen.Sides.Reset(Data.Regions.Num(), HolderNumbers.Num());
		for (int32 Each = 0; Each < Data.Regions.Num(); ++Each)
		{
			Pen.Sides.HolderOf[static_cast<size_t>(Each)] = HolderNumbers.FindChecked(Data.Regions[Each].Owner);
		}
		for (int32 K = 0; K + 1 < WarPairs.Num(); K += 2)
		{
			const int32* One = HolderNumbers.Find(WarPairs[K]);
			const int32* Other = HolderNumbers.Find(WarPairs[K + 1]);
			if (One && Other)
			{
				Pen.Sides.SetWar(*One, *Other);
			}
		}
		for (const TPair<FString, int32>& Holder : HolderNumbers)
		{
			Pen.Sides.Foe[static_cast<size_t>(Holder.Value)] = WarFoes.Contains(Holder.Key) ? 1 : 0;
		}
		TheaterFront::DrawFront(Pen.Pieces, Pen.Sides, Pen.Rules, Pen.Work, Pen.Front, Pen.Chains);
		Pen.Mine = Pen.Theirs = Pen.Closed = 0;
		for (const TheaterFront::FChain& Line : Pen.Chains)
		{
			(Line.Kind == TheaterFront::EKind::Mine ? Pen.Mine : Pen.Theirs)++;
			Pen.Closed += Line.bClosed ? 1 : 0;
		}
	}
	if (Pen.Chains.empty())
	{
		// (Wars, but nowhere on the table do two sides at war meet: a war across the sea alone. Nothing is laid, but as
		// in release 1 the line counts as laid at the working weight, so that UpdateForCamera still takes the enemy's
		// reach to the staff map's weight from high up: with no weight known (-1) it never would.)
		if (!bOwed)
		{
			FrontLineFar = 0;
			UE_LOG(LogTheater, Log, TEXT("The front line, drawn by pen: no two sides at war meet on the table (%d wars)"), WarPairs.Num() / 2);
		}
		return true;
	}
	TheaterFront::FPenLook Look;
	{
		// (the shadow falls a little away from the table's lamp, as the environment hangs it: see LightFigures. The map's
		// x runs east, the world's +Y; the map's y runs south, the world's -X.)
		const FVector Lamp = FRotator(-42.0, 69.0, 0.0).Vector();
		const FVector2D Away = FVector2D(Lamp.Y, -Lamp.X).GetSafeNormal();
		Look.ShadowX = Away.X * 0.12;
		Look.ShadowY = Away.Y * 0.12;
		// ("-TheaterFrontPen=half,others' half,tooth,teeth apart,shadow,shadow's reach" tries another pen, for the art
		// director: the player's line's half width, others', how far a tooth stands out, about how far apart they stand,
		// the shadow's strength (0 to 1) and how many half widths out it fades. Map units; working weight.)
		static TArray<double> PenAsked;
		static bool bPenRead = false;
		if (!bPenRead)
		{
			bPenRead = true;
			FString Asked;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFrontPen="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) > 0)
			{
				for (const FString& Part : Parts)
				{
					PenAsked.Add(FCString::Atod(*Part));
				}
			}
		}
		Look.Half[1] = PenAsked.IsValidIndex(0) ? PenAsked[0] : Look.Half[1];
		Look.Half[0] = PenAsked.IsValidIndex(1) ? PenAsked[1] : Look.Half[0];
		Look.TickReach = PenAsked.IsValidIndex(2) ? PenAsked[2] : Look.TickReach;
		Look.TickEvery = PenAsked.IsValidIndex(3) && PenAsked[3] > 0.2 ? PenAsked[3] : Look.TickEvery;
		Look.HaloAlpha[1] = PenAsked.IsValidIndex(4) ? static_cast<uint8>(FMath::Clamp(PenAsked[4], 0.0, 1.0) * 255.0) : Look.HaloAlpha[1];
		Look.HaloOuter = PenAsked.IsValidIndex(5) && PenAsked[5] > Look.HaloInner ? PenAsked[5] : Look.HaloOuter;
	}
	// The pen's lines handed to the graphics chip as the table's other inked lines are: each point lifted clear of
	// the ground's bumps by SurfacePointKept (kept, so a line laid again where it lay before costs no new heights).
	auto HandOver = [this](const TheaterFront::FInkMesh& From, FCampaignMeshBuffers& Into)
	{
		const int32 PointCount = static_cast<int32>(From.Vertices.size());
		Into.Vertices.Reserve(PointCount);
		Into.Normals.Reserve(PointCount);
		Into.Colors.Reserve(PointCount);
		Into.UVs.Reserve(PointCount);
		Into.Tangents.Reserve(PointCount);
		Into.Triangles.Reserve(static_cast<int32>(From.Triangles.size()));
		for (const TheaterFront::FInkVertex& Laid : From.Vertices)
		{
			Into.AddVertex(SurfacePointKept(Laid.X, Laid.Y, Laid.Lift), FVector::UpVector, FColor(Laid.R, Laid.G, Laid.B, Laid.A));
		}
		for (size_t Corner = 0; Corner + 2 < From.Triangles.size(); Corner += 3)
		{
			Into.AddTriangleFacingUp(From.Triangles[Corner], From.Triangles[Corner + 1], From.Triangles[Corner + 2]);
		}
	};
	for (int32 Weight = 0; Weight < 2; ++Weight)
	{
		if (Only != INDEX_NONE && Weight != Only)
		{
			continue;
		}
		TheaterFront::LayPen(Pen.Chains, Look, Weight, Pen.Ink, Pen.Halo);
		FCampaignMeshBuffers InkLaid, ShadowLaid;
		HandOver(Pen.Ink, InkLaid);
		HandOver(Pen.Halo, ShadowLaid);
		THEATER_SPEED_SCOPE(PenFrontCommit);
		InkLaid.Commit(FrontLineMesh, Weight, InkMaterial);
		ShadowLaid.Commit(FrontLineMesh, Weight + 2, WashInk()); // (see-through, by each point's own strength: the washes' ink, which the night takes down with the ground)
	}
	if (bOwed)
	{
		FrontLineMesh->SetMeshSectionVisible(Only, FrontLineFar == Only); // (shown if the eye is at it)
		FrontLineMesh->SetMeshSectionVisible(Only + 2, FrontLineFar == Only);
		return true;
	}
	FrontLineMesh->SetMeshSectionVisible(1, false);
	FrontLineMesh->SetMeshSectionVisible(3, false);
	FrontLineFar = 0;
	UE_LOG(LogTheater, Log, TEXT("The front line, drawn by pen: %d lines of the player's own fronts and %d of others' (%d closed on themselves), from %d stretches of border (%d wars)"),
		Pen.Mine, Pen.Theirs, Pen.Closed, static_cast<int32>(Pen.Front.size()), WarPairs.Num() / 2);
	return true;
}
// ---- (end release 2, the front you can read) ----

void ACampaignMapActor::ShowWrecked(const TArray<int32>& FromRules)
{
	THEATER_SPEED_SCOPE(ShowWrecked);
	// Factories that are not working: the province and how many. The towns are built again only when that changes
	// (a raid that told, a saboteur, a works relaid): their wrecked works stand roofless, with cold stacks.
	TMap<int32, int32> Now;
	for (int32 K = 0; K + 1 < FromRules.Num(); K += 2)
	{
		if (Data.Regions.IsValidIndex(FromRules[K]) && FromRules[K + 1] > 0)
		{
			Now.Add(FromRules[K], FromRules[K + 1]);
		}
	}
	// ("-TheaterWrecked=Berlin:2,Hamburg:1" wrecks works for a picture)
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterWrecked="), Asked, false))
	{
		TArray<FString> Places;
		Asked.ParseIntoArray(Places, TEXT(","));
		for (const FString& Place : Places)
		{
			FString Name, Count;
			if (Place.Split(TEXT(":"), &Name, &Count))
			{
				if (const int32* Id = Data.RegionByName.Find(Name))
				{
					Now.Add(*Id, FCString::Atoi(*Count));
				}
			}
		}
	}
	if (Now.OrderIndependentCompareEqual(WreckedIn))
	{
		return;
	}
	const bool bTowns = TownMesh != nullptr;
	WreckedIn = MoveTemp(Now);
	if (bTowns)
	{
		BuildTowns();
	}
}

void ACampaignMapActor::AddSinking(int32 Zone, const FString& Nation, const FString& Class, int32 Nth)
{
	if (!Data.Seas.IsValidIndex(Zone) || !GetWorld() || SinkingShips.Num() >= 8)
	{
		return;
	}
	const TCHAR* Kind = Class == TEXT("bb") ? TEXT("Battleship:") : Class == TEXT("cv") ? TEXT("Carrier:") : Class == TEXT("ca") ? TEXT("Cruiser:") : Class == TEXT("dd") ? TEXT("Destroyer:") : TEXT("Submarine:");
	// (to the scale of the fleets' own ships, a little over it so that she reads as the thing to look at)
	const double Metre = 0.066 * UnitScale * 1.4 * 1.5;
	FMiniatureMesh Model;
	{
		TGuardValue<uint8> Painted(Model.Finish, 128); // (ships are painted metal)
		if (!Model.Vehicle(FString(Kind) + Nation, FVector::ZeroVector, Metre, 0.0) || Model.Vertices.Num() == 0)
		{
			return;
		}
	}
	FBox Bounds(ForceInit);
	for (const FVector& Point : Model.Vertices)
	{
		Bounds += Point;
	}
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->RegisterComponent();
	Mesh->SetCastShadow(false);
	Mesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
	Mesh->SetMaterial(0, PieceMaterial);
	// Beside where the guns flash, each ship of the fight in a place of her own.
	// (well clear of the fleets that lie at the middle of the sea, and on open water)
	const double Angle = 2.4 * Nth + 0.7 * Zone;
	FVector2D Spot = Data.Seas[Zone].Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 9.0;
	for (const double Far : { 30.0, 24.0, 18.0, 13.0 })
	{
		const FVector2D Try = Data.Seas[Zone].Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Far + 4.0 * (Nth % 2));
		if (GroundAt(Try.X, Try.Y) <= 0.0f && Data.FindRegionAt(Try.X, Try.Y) == INDEX_NONE)
		{
			Spot = Try;
			break;
		}
	}
	FSinkingShip& Ship = SinkingShips.AddDefaulted_GetRef();
	Ship.Mesh = Mesh;
	Ship.Began = GetWorld()->GetTimeSeconds() + 0.45 * Nth;
	Ship.At = MapToWorld(Spot.X, Spot.Y, 0.0) + FVector(0.0, 0.0, 0.02 * UnitScale);
	Ship.Yaw = FMath::Fmod(Angle * 97.0, 360.0);
	Ship.High = FMath::Max(Bounds.GetSize().Z, 1.0);
	Ship.Long = FMath::Max(Bounds.GetSize().X, Bounds.GetSize().Y);
	SinkingMeshes.Add(Mesh);
	Mesh->SetWorldLocationAndRotation(Ship.At, FRotator(0.0, Ship.Yaw, 0.0));
	// The white water where she goes under (the art director's ruling: from working height the sea stayed glassy and
	// a lost ship just vanished): a patch of the wake's white, a ship's length across, ragged at its edge, that
	// boils up as she goes and fades over a few seconds after. It is paint on the water: dimmed by night with the sea.
	if (HighlightMaterial)
	{
		UProceduralMeshComponent* Water = NewObject<UProceduralMeshComponent>(this);
		Water->SetupAttachment(Root);
		Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Water->RegisterComponent();
		Water->SetCastShadow(false);
		FCampaignMeshBuffers Patch;
		// (the wake's white as it comes out on the screen by day, about (176, 185, 183); soft from its middle to its edge)
		// (The art director's note of round 19: it was a soft lump with corners, wider than the ship, and read as a
		// cloud lying on the water. It is an oval along her line now, her length long and a third as wide, whitest in
		// its middle at about 0.85, with a smooth edge.)
		const FLinearColor White(0.6f, 0.71f, 0.74f); // (a shade to the cold side: on the sea's strong blue-green a neutral white reads as sand)
		const int32 Sides = 36;
		static const double Rings[3] = { 0.4, 0.72, 1.0 };
		static const float Thick[3] = { 0.62f, 0.28f, 0.0f };
		const int32 Hub = Patch.AddVertex(FVector::ZeroVector, FVector::UpVector, FLinearColor(White.R, White.G, White.B, 0.8f).ToFColor(false));
		TArray<int32> Ring[3];
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const double Round = 2.0 * PI * Side / Sides;
			const double Ragged = 1.0 + 0.04 * FMath::Sin(Round * 2.0 + Nth * 1.9) + 0.025 * FMath::Sin(Round * 5.0 + Zone);
			const FVector Out(FMath::Cos(Round), FMath::Sin(Round) * 0.34, 0.0);
			for (int32 K = 0; K < 3; ++K)
			{
				Ring[K].Add(Patch.AddVertex(Out * (Rings[K] * Ragged), FVector::UpVector, FLinearColor(White.R, White.G, White.B, Thick[K]).ToFColor(false)));
			}
		}
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const int32 Next = (Side + 1) % Sides;
			Patch.AddTriangleFacingUp(Hub, Ring[0][Side], Ring[0][Next]);
			for (int32 K = 0; K + 1 < 3; ++K)
			{
				Patch.AddTriangleFacingUp(Ring[K][Side], Ring[K + 1][Side], Ring[K][Next]);
				Patch.AddTriangleFacingUp(Ring[K][Next], Ring[K + 1][Side], Ring[K + 1][Next]);
			}
		}
		UMaterialInstanceDynamic* Wash = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		Wash->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
		Wash->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
		Patch.Commit(Water, 0, Wash);
		Water->SetWorldLocationAndRotation(Ship.At + FVector(0.0, 0.0, 0.03 * UnitScale), FRotator(0.0, Ship.Yaw, 0.0));
		Water->SetVisibility(false);
		Ship.Water = Water;
		Ship.WaterInk = Wash;
		SinkingMeshes.Add(Water);
		SinkingInks.Add(Wash);
	}
}

void ACampaignMapActor::UpdateSinkings()
{
	THEATER_SPEED_SCOPE(UpdateSinkings);
	if (SinkingShips.Num() == 0 || !GetWorld())
	{
		return;
	}
	// She heels over, settles by the stern and slides under: about six seconds from the first list to the last of her.
	// ("-TheaterSinkAt=0.45" holds every sinking ship at that point of it, for a picture.)
	static float Held = -1.0f;
	static const bool bHeld = FParse::Value(FCommandLine::Get(), TEXT("TheaterSinkAt="), Held);
	const double Now = GetWorld()->GetTimeSeconds(), Lasts = 6.0;
	const float Moon = bLamplightNow ? 0.0f : FMath::Clamp(NightNow, 0.0f, 1.0f);
	for (int32 Index = SinkingShips.Num() - 1; Index >= 0; --Index)
	{
		FSinkingShip& Ship = SinkingShips[Index];
		const double T = bHeld ? Held : (Now - Ship.Began) / Lasts;
		Ship.T = T;
		if (T >= 1.6 || !Ship.Mesh)
		{
			for (UProceduralMeshComponent* Part : { Ship.Mesh.Get(), Ship.Water.Get() })
			{
				if (Part)
				{
					SinkingMeshes.Remove(Part);
					Part->DestroyComponent();
				}
			}
			SinkingInks.Remove(Ship.WaterInk);
			SinkingShips.RemoveAt(Index);
			continue;
		}
		{
			// (seam-4: she goes down on the eye's side of the chart's seam, her white water with her, as a fleet is shown)
			const FVector Beside = BesideEye(Ship.At);
			if (Beside.Y != Ship.At.Y)
			{
				if (Ship.Water)
				{
					Ship.Water->AddWorldOffset(FVector(0.0, Beside.Y - Ship.At.Y, 0.0));
				}
				Ship.At = Beside;
			}
		}
		// (she lists for the first half of it, and only then begins to go)
		const double Heel = 56.0 * FMath::SmoothStep(0.0, 0.7, T);
		const double Stern = 14.0 * FMath::SmoothStep(0.3, 1.0, T);
		const double Down = Ship.High * 1.4 * FMath::SmoothStep(0.5, 1.0, T);
		Ship.Mesh->SetVisibility(T < 1.0);
		Ship.Mesh->SetWorldLocationAndRotation(Ship.At - FVector(0.0, 0.0, Down), FRotator(Stern, Ship.Yaw, Heel));
		if (Ship.Water && Ship.WaterInk)
		{
			// The white water: it comes as she begins to go, is at its whitest as the last of her slides under, and
			// is gone a few seconds after; it spreads all the while.
			const float White = static_cast<float>(FMath::SmoothStep(0.45, 0.9, T) * (1.0 - FMath::SmoothStep(1.0, 1.6, T)));
			Ship.Water->SetVisibility(White > 0.01f);
			const double Wide = Ship.Long * (0.36 + 0.16 * FMath::SmoothStep(0.45, 1.6, T)); // (from about three quarters of her length long to a little over her length)
			Ship.Water->SetWorldScale3D(FVector(Wide, Wide, 1.0));
			Ship.WaterInk->SetScalarParameterValue(TEXT("Opacity"), White);
			Ship.WaterInk->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(0.3f, 0.34f, 0.42f), Moon));
		}
	}
}

void ACampaignMapActor::ShowWrecks(const TArray<FWreckMark>& Wrecks, int32 Turn)
{
	THEATER_SPEED_SCOPE(ShowWrecks);
	TArray<FWreckMark> Now = Wrecks;
	{
		// ("-TheaterWrecks=6:GER:bb,6:UK:ca" stands wrecks on the chart for a picture: the sea, whose, what class)
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterWrecks="), Asked, false))
		{
			TArray<FString> Each;
			Asked.ParseIntoArray(Each, TEXT(","));
			for (const FString& One : Each)
			{
				TArray<FString> Parts;
				if (One.ParseIntoArray(Parts, TEXT(":")) == 3)
				{
					FWreckMark& Mark = Now.AddDefaulted_GetRef();
					Mark.Zone = FCString::Atoi(*Parts[0]);
					Mark.Nation = Parts[1];
					Mark.Class = Parts[2];
					Mark.Turn = Turn;
					Mark.Seed = Now.Num() * 7 + Mark.Zone;
				}
			}
		}
	}
	if (Now == WrecksShown && Turn == WrecksTurn)
	{
		return;
	}
	WrecksShown = Now;
	WrecksTurn = Turn;
	if (!WrecksMesh)
	{
		if (Now.Num() == 0)
		{
			return;
		}
		WrecksMesh = UTheaterStillMesh::Make(this, TEXT("Wrecks"));
		WrecksMesh->SetupAttachment(Root);
		WrecksMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WrecksMesh->RegisterComponent();
		WrecksMesh->SetCastShadow(false);
	}
	WrecksMesh->ClearAllMeshSections();
	if (Now.Num() == 0 || !Data.IsLoaded())
	{
		return;
	}
	// A wreck as a chart-maker draws one: a short line for the hull with three strokes across it, in dark ink, on a
	// faint slick. Small, to one side of the sea's middle, and paler once it is more than a year old.
	FCampaignMeshBuffers Buffers;
	for (const FWreckMark& Mark : Now)
	{
		if (!Data.Seas.IsValidIndex(Mark.Zone))
		{
			continue;
		}
		const FVector2D Sea = Data.Seas[Mark.Zone].Center;
		FVector2D At = Sea + FVector2D(14.0, -12.0);
		for (int32 Try = 0; Try < 8; ++Try)
		{
			const double Angle = 2.39996 * (Mark.Seed + Try * 3), Far = 15.0 + 2.2 * ((Mark.Seed + Try) % 6);
			const FVector2D Spot = Sea + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Far;
			if (GroundAt(Spot.X, Spot.Y) <= 0.0f && Data.FindRegionAt(Spot.X, Spot.Y) == INDEX_NONE)
			{
				At = Spot;
				break;
			}
		}
		const float Old = Turn - Mark.Turn > 12 ? 0.55f : 1.0f;
		const double Size = Mark.Class == TEXT("ca") ? 1.5 : 1.9;
		const double Lie = FMath::DegreesToRadians(static_cast<double>((Mark.Seed * 53) % 50) - 25.0);
		const FVector2D Along(FMath::Cos(Lie), FMath::Sin(Lie)), Across(-Along.Y, Along.X);
		// (the slick)
		{
			const FColor Slick = FLinearColor(0.02f, 0.025f, 0.035f, 0.2f * Old).ToFColor(false), Edge = FLinearColor(0.02f, 0.025f, 0.035f, 0.0f).ToFColor(false);
			const int32 Middle = Buffers.AddVertex(SurfacePoint(At.X, At.Y, 0.2), FVector::UpVector, Slick);
			int32 First = INDEX_NONE, Last = INDEX_NONE;
			for (int32 Side = 0; Side < 14; ++Side)
			{
				const double Angle = 2.0 * PI * Side / 14.0, Wobble = 1.0 + 0.18 * FMath::Sin(Angle * 3.0 + Mark.Seed);
				const FVector2D Rim = At + Along * (FMath::Cos(Angle) * Size * 2.1 * Wobble) + Across * (FMath::Sin(Angle) * Size * 1.25 * Wobble);
				const int32 Vertex = Buffers.AddVertex(SurfacePoint(Rim.X, Rim.Y, 0.2), FVector::UpVector, Edge);
				if (Last != INDEX_NONE)
				{
					Buffers.AddTriangleFacingUp(Middle, Last, Vertex);
				}
				else
				{
					First = Vertex;
				}
				Last = Vertex;
			}
			Buffers.AddTriangleFacingUp(Middle, Last, First);
		}
		const FColor Ink = FLinearColor(0.07f, 0.05f, 0.04f, 0.85f * Old).ToFColor(false);
		AppendRibbon(Buffers, { At - Along * Size, At + Along * Size }, 0.16, 0.26, Ink, false);
		for (const double Place : { -0.55, 0.0, 0.55 })
		{
			AppendRibbon(Buffers, { At + Along * (Size * Place) - Across * (Size * 0.42), At + Along * (Size * Place) + Across * (Size * 0.42) }, 0.14, 0.26, Ink, false);
		}
	}
	if (!LanesInk && HighlightMaterial)
	{
		LanesInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		LanesInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
		ShownNight = -1.0f;
	}
	Buffers.Commit(WrecksMesh, 0, LanesInk ? static_cast<UMaterialInterface*>(LanesInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
}

UMaterialInterface* ACampaignMapActor::WashInk()
{
	// The washes laid for an army in hand (where it can go, what it can attack, how far the enemy reaches) have an
	// ink of their own, so that the night can take them down with the ground: see SetNight.
	if (!OrdersInk && HighlightMaterial)
	{
		OrdersInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		ShownNight = -1.0f; // (so that the hour's light is laid on it at once)
		SetNight(NightNow, bLamplightNow);
	}
	return OrdersInk ? static_cast<UMaterialInterface*>(OrdersInk) : static_cast<UMaterialInterface*>(HighlightMaterial);
}

void ACampaignMapActor::SetHandLamp(AArmyPiece* Piece)
{
	HandLampPiece = Piece;
}

AArmyPiece* ACampaignMapActor::FindFleetPiece(int32 FleetId) const
{
	const TObjectPtr<AArmyPiece>* Piece = FleetPieces.Find(FleetId);
	return Piece ? Piece->Get() : nullptr;
}

AArmyPiece* ACampaignMapActor::FindAirPiece(int32 WingId) const
{
	const TObjectPtr<AArmyPiece>* Piece = AirPieces.Find(WingId);
	return Piece ? Piece->Get() : nullptr;
}

void ACampaignMapActor::ShowPointer(const TArray<FVector>& Corners, const TArray<FColor>& Inks)
{
	if (!PointerMesh)
	{
		return;
	}
	if (Corners.Num() == 0 || Corners.Num() != Inks.Num() * 4 || !HighlightMaterial)
	{
		PointerMesh->ClearAllMeshSections();
		return;
	}
	FCampaignMeshBuffers Buffers;
	for (int32 Quad = 0; Quad < Inks.Num(); ++Quad)
	{
		int32 At[4];
		for (int32 Each = 0; Each < 4; ++Each)
		{
			At[Each] = Buffers.AddVertex(Corners[Quad * 4 + Each], FVector::UpVector, Inks[Quad]);
		}
		Buffers.AddTriangleFacingUp(At[0], At[1], At[2]);
		Buffers.AddTriangleFacingUp(At[0], At[2], At[3]);
	}
	Buffers.Commit(PointerMesh, 0, HighlightMaterial);
}

void ACampaignMapActor::ShowChart(bool bOn, const TArray<int32>& Levels, const TArray<FOldSighting>& Old)
{
	if (bOn == bChartOn && (!bOn || (Levels == ChartLevels && Old == ChartAsked)))
	{
		return;
	}
	ChartAsked = Old;
	bChartOn = bOn;
	ChartLevels = Levels;
	ChartOld = Old;
	{
		// ("-TheaterSightings=10:GER:1,3:ITA:3" stands old sightings on the chart for a picture: the sea, whose, how many months ago)
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSightings="), Asked, false))
		{
			TArray<FString> Each;
			Asked.ParseIntoArray(Each, TEXT(","));
			for (const FString& One : Each)
			{
				TArray<FString> Parts;
				if (One.ParseIntoArray(Parts, TEXT(":")) == 3)
				{
					FOldSighting& Seen = ChartOld.AddDefaulted_GetRef();
					Seen.Zone = FCString::Atoi(*Parts[0]);
					Seen.Nation = Parts[1];
					Seen.Months = FCString::Atoi(*Parts[2]);
				}
			}
		}
	}
	if (!ChartMesh)
	{
		if (!bOn)
		{
			return;
		}
		ChartMesh = NewObject<UProceduralMeshComponent>(this);
		ChartMesh->SetupAttachment(Root);
		ChartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ChartMesh->RegisterComponent();
		ChartMesh->SetCastShadow(false);
	}
	ChartMesh->ClearAllMeshSections();
	if (!bOn || !Data.IsLoaded() || Data.Seas.Num() == 0)
	{
		return;
	}
	// The grid over the water, laid once: every point knows the sea it lies in (the nearest sea's, as the rules
	// reckon which sea a coast is on) and the nearest other, so that one sea's veil runs softly into the next.
	const double Step = 6.0;
	if (ChartPoints.Num() == 0)
	{
		ChartAcross = FMath::CeilToInt32(Terrain.NX * Terrain.Cell / Step) + 1;
		ChartDown = FMath::CeilToInt32(Terrain.NY * Terrain.Cell / Step) + 1;
		ChartPoints.SetNum(ChartAcross * ChartDown);
		for (int32 J = 0; J < ChartDown; ++J)
		{
			for (int32 I = 0; I < ChartAcross; ++I)
			{
				FChartPoint& Point = ChartPoints[J * ChartAcross + I];
				const double X = I * Step, Y = J * Step;
				Point.bWater = GroundAt(X, Y) <= 0.0f && Data.FindRegionAt(X, Y) == INDEX_NONE;
				Point.At = SurfacePoint(X, Y, 0.22);
				double Best = TNumericLimits<double>::Max(), Second = TNumericLimits<double>::Max();
				for (int32 Sea = 0; Sea < Data.Seas.Num(); ++Sea)
				{
					const double Far = FVector2D::Distance(Data.Seas[Sea].Center, FVector2D(X, Y));
					if (Far < Best)
					{
						Second = Best;
						Point.Next = Point.Sea;
						Best = Far;
						Point.Sea = Sea;
					}
					else if (Far < Second)
					{
						Second = Far;
						Point.Next = Sea;
					}
				}
				Point.Own = Point.Next == INDEX_NONE ? 1.0f : static_cast<float>(0.5 + 0.5 * FMath::SmoothStep(0.0, 26.0, Second - Best));
			}
		}
	}
	// The veil: none over a sea well searched or in full view, a light one over a sea thinly searched, a deeper
	// one over a sea not searched at all. It is the fog of war as the land wears it: a wash the grey of blank chart
	// paper, and over a sea not searched at all the same slanting hatch.
	auto VeilOf = [this](int32 Sea) -> float
	{
		const int32 Level = ChartLevels.IsValidIndex(Sea) ? ChartLevels[Sea] : 3;
		return Level <= 0 ? 0.5f : Level == 1 ? 0.24f : 0.0f;
	};
	FCampaignMeshBuffers Buffers;
	TArray<int32> Index;
	Index.Init(INDEX_NONE, ChartPoints.Num());
	auto VeilAt = [&](const FChartPoint& Point) -> float
	{
		return !Point.bWater ? 0.0f : FMath::Lerp(VeilOf(Point.Next == INDEX_NONE ? Point.Sea : Point.Next), VeilOf(Point.Sea), Point.Own);
	};
	auto VertexOf = [&](int32 I, int32 J) -> int32
	{
		const int32 K = J * ChartAcross + I;
		if (Index[K] == INDEX_NONE)
		{
			const FChartPoint& Point = ChartPoints[K];
			Index[K] = Buffers.AddVertex(Point.At, FVector::UpVector, FLinearColor(0.56f, 0.55f, 0.5f, VeilAt(Point)).ToFColor(false));
		}
		return Index[K];
	};
	const FVector HatchLift(0.0, 0.0, 0.05 * UnitScale);
	const FLinearColor HatchInk(0.13f, 0.17f, 0.26f, 1.0f);
	for (int32 J = 0; J + 1 < ChartDown; ++J)
	{
		for (int32 I = 0; I + 1 < ChartAcross; ++I)
		{
			const FChartPoint& A = ChartPoints[J * ChartAcross + I];
			const FChartPoint& B = ChartPoints[J * ChartAcross + I + 1];
			const FChartPoint& C = ChartPoints[(J + 1) * ChartAcross + I];
			const FChartPoint& D = ChartPoints[(J + 1) * ChartAcross + I + 1];
			if (!(A.bWater || B.bWater || C.bWater || D.bWater))
			{
				continue; // dry land
			}
			const float VB_ = VeilAt(B), VC_ = VeilAt(C);
			if (VeilAt(A) + VB_ + VC_ + VeilAt(D) <= 0.0f)
			{
				continue; // open water, well searched: nothing to lay over it
			}
			const int32 VA = VertexOf(I, J), VB = VertexOf(I + 1, J), VC = VertexOf(I, J + 1), VD = VertexOf(I + 1, J + 1);
			Buffers.AddTriangleFacingUp(VA, VB, VC);
			Buffers.AddTriangleFacingUp(VB, VC, VD);
			// The hatch: one slanting stroke across every other square, so that the strokes join into lines from
			// corner to corner of the sea; inked only as far as the sea is not searched at all.
			// (they slant the way the land's hatch does, down to the east)
			if (((I - J) % 2 + 2) % 2 == 0 && A.bWater && D.bWater)
			{
				const float InkB = FMath::Clamp((VeilAt(A) - 0.3f) * 2.2f, 0.0f, 0.42f), InkC = FMath::Clamp((VeilAt(D) - 0.3f) * 2.2f, 0.0f, 0.42f);
				if (InkB + InkC > 0.0f)
				{
					const FVector Along = (D.At - A.At).GetSafeNormal2D();
					const FVector Side = FVector(-Along.Y, Along.X, 0.0) * (0.11 * UnitScale);
					const int32 H0 = Buffers.AddVertex(A.At + HatchLift + Side, FVector::UpVector, FLinearColor(HatchInk.R, HatchInk.G, HatchInk.B, InkB).ToFColor(false));
					const int32 H1 = Buffers.AddVertex(A.At + HatchLift - Side, FVector::UpVector, FLinearColor(HatchInk.R, HatchInk.G, HatchInk.B, InkB).ToFColor(false));
					const int32 H2 = Buffers.AddVertex(D.At + HatchLift + Side, FVector::UpVector, FLinearColor(HatchInk.R, HatchInk.G, HatchInk.B, InkC).ToFColor(false));
					const int32 H3 = Buffers.AddVertex(D.At + HatchLift - Side, FVector::UpVector, FLinearColor(HatchInk.R, HatchInk.G, HatchInk.B, InkC).ToFColor(false));
					Buffers.AddTriangleFacingUp(H0, H1, H2);
					Buffers.AddTriangleFacingUp(H1, H2, H3);
				}
			}
		}
	}
	// Old sightings: a navigator's fix where enemy ships were last seen, a ring with a cross in it, in red crayon,
	// fainter for each month since. It stands a little off the middle of the sea, clear of a fleet that lies there.
	TMap<int32, int32> InSea;
	FCampaignMeshBuffers Fixes;
	for (const FOldSighting& Seen : ChartOld)
	{
		if (!Data.Seas.IsValidIndex(Seen.Zone))
		{
			continue;
		}
		const int32 Nth = InSea.FindOrAdd(Seen.Zone)++;
		// (clear of the fleets that lie at the middle of the sea: to one side of it, wherever there is water)
		FVector2D Middle = Data.Seas[Seen.Zone].Center + FVector2D(-11.0, 8.0);
		for (const FVector2D& Off : { FVector2D(-22.0, 20.0), FVector2D(22.0, 20.0), FVector2D(-22.0, -20.0), FVector2D(22.0, -20.0), FVector2D(0.0, 27.0), FVector2D(0.0, -27.0), FVector2D(-12.0, 12.0) })
		{
			const FVector2D Try = Data.Seas[Seen.Zone].Center + Off + FVector2D(9.0 * Nth, 0.0);
			if (GroundAt(Try.X, Try.Y) <= 0.0f && Data.FindRegionAt(Try.X, Try.Y) == INDEX_NONE)
			{
				Middle = Try;
				break;
			}
		}
		const float Strength = Seen.Months <= 1 ? 0.85f : Seen.Months == 2 ? 0.55f : 0.32f;
		UE_LOG(LogTheater, Verbose, TEXT("A sighting in the %s"), *Data.Seas[Seen.Zone].Name);
		const FColor Ink = FLinearColor(0.78f, 0.12f, 0.08f, Strength).ToFColor(false);
		TArray<FVector2D> Round;
		for (int32 Side = 0; Side < 28; ++Side)
		{
			const double Angle = 2.0 * PI * Side / 28.0;
			Round.Add(Middle + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 3.4);
		}
		AppendRibbon(Fixes, Round, 0.32, 0.3, Ink, true);
		AppendRibbon(Fixes, { Middle - FVector2D(1.7, 1.7), Middle + FVector2D(1.7, 1.7) }, 0.28, 0.3, Ink, false);
		AppendRibbon(Fixes, { Middle - FVector2D(1.7, -1.7), Middle + FVector2D(1.7, -1.7) }, 0.28, 0.3, Ink, false);
	}
	// (the sightings in an ink of their own: the lanes' ink goes down so far by night that the ring was lost)
	if (!SightingInk && HighlightMaterial)
	{
		SightingInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		ShownNight = -1.0f;
		SetNight(NightNow, bLamplightNow);
	}
	Fixes.Commit(ChartMesh, 1, SightingInk ? static_cast<UMaterialInterface*>(SightingInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
	// (in the lanes' own ink, which is dimmed with the land by night)
	if (!LanesInk && HighlightMaterial)
	{
		LanesInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		LanesInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
		ShownNight = -1.0f;
	}
	Buffers.Commit(ChartMesh, 0, LanesInk ? static_cast<UMaterialInterface*>(LanesInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
	UE_LOG(LogTheater, Log, TEXT("The admiral's chart: %d seas, %d old sightings, %d points of veil"), ChartLevels.Num(), ChartOld.Num(), Buffers.Vertices.Num());
	{
		FString Names;
		for (int32 Sea = 0; Sea < Data.Seas.Num(); ++Sea)
		{
			Names += FString::Printf(TEXT("%d %s (%d); "), Sea, *Data.Seas[Sea].Name, ChartLevels.IsValidIndex(Sea) ? ChartLevels[Sea] : -1);
		}
		UE_LOG(LogTheater, Log, TEXT("Seas by number, with how well each is searched: %s"), *Names);
	}
}

void ACampaignMapActor::SetPiecesShown(bool bShown)
{
	// (air battle: nothing stands on the table again while a battle's scene is up)
	if (bAwayForBattle && bShown)
	{
		return;
	}
	// (while they are off the table it is said again every frame: a piece stood meanwhile is lifted off with the rest)
	if (bShown && bPiecesShown)
	{
		return;
	}
	bPiecesShown = bShown;
	auto Set = [bShown](AArmyPiece* Piece)
	{
		if (Piece)
		{
			Piece->SetActorHiddenInGame(!bShown);
		}
	};
	for (AArmyPiece* Piece : Pieces)
	{
		Set(Piece);
	}
	for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : FleetPieces)
	{
		Set(Pair.Value);
	}
	for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : AirPieces)
	{
		Set(Pair.Value);
	}
	if (bShown)
	{
		ApplyCrowds(); // (an army that was put away behind another's flag stays put away)
		BuildGunLines();
	}
}

void ACampaignMapActor::ShowIdle(const TArray<int32>& FromRules)
{
	THEATER_SPEED_SCOPE(ShowIdle);
	// Only the smoke changes: the stacks stand as they are, and the plumes are drawn afresh every frame.
	IdleIn.Reset();
	IdleIn.Append(FromRules);
	// ("-TheaterIdle=Berlin,Hamburg" stands a city idle for a picture)
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterIdle="), Asked, false))
	{
		TArray<FString> Places;
		Asked.ParseIntoArray(Places, TEXT(","));
		for (const FString& Place : Places)
		{
			if (const int32* Id = Data.RegionByName.Find(Place))
			{
				IdleIn.Add(*Id);
			}
		}
	}
}

void ACampaignMapActor::DressBeyond(UMaterialInstanceDynamic* Sheet) const
{
	if (!Sheet)
	{
		return;
	}
	// The land beyond Europe's own sheet: its woods are edged with a fringe of thin single trees, round every wood
	// and every opening in one, as the northern forest thins out toward the tundra. The painter lays the edge of a
	// wood there as a slow thinning-out, and here each tree-top is let stand or fall by itself across it. (Europe's
	// woods keep their firm edge.)
	Sheet->SetScalarParameterValue(TEXT("WoodsFringe"), 1.2f);
	// (each whole plot beyond Europe has a thin light edge; "-TheaterOldFields" lays the fields as they were)
	// (and a plot there is under three times as long as it is wide: Europe's strips, at 2.2 rows to the field's
	// length, came out six to one once two or three of a tone lay end to end. "-TheaterPlots=strip,edge" tries others.)
	static const bool bPlotEdges = !FParse::Param(FCommandLine::Get(), TEXT("TheaterOldFields"));
	static float PlotStrip = 1.4f, PlotEdge = 0.45f; // (the art director's choice of 18 November: two to one, about thirty-seven dots by eighteen from playing height)
	static bool bPlotsRead = false;
	if (!bPlotsRead)
	{
		bPlotsRead = true;
		FString Asked;
		TArray<FString> Parts;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterPlots="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) >= 2)
		{
			PlotStrip = FCString::Atof(*Parts[0]);
			PlotEdge = FCString::Atof(*Parts[1]);
		}
	}
	Sheet->SetScalarParameterValue(TEXT("PlotEdge"), bPlotEdges ? PlotEdge : 0.0f);
	if (bPlotEdges)
	{
		Sheet->SetScalarParameterValue(TEXT("FieldStrip"), PlotStrip);
	}
	if (Sheet == WorldLandMaterial)
	{
		// (the coarse sheet is what is seen from high up: there a wood's edge is feathered over a few points of the
		// screen, where a firm one read as a blot of tar)
		Sheet->SetScalarParameterValue(TEXT("WoodsSharp"), 2.2f);
	}
}

void ACampaignMapActor::LandScalar(FName Name, float Value)
{
	if (LandMaterial) { LandMaterial->SetScalarParameterValue(Name, Value); }
	if (WorldLandMaterial) { WorldLandMaterial->SetScalarParameterValue(Name, Value); }
	for (const auto& Pair : FineSheets) { if (Pair.Value->LandPaint) { Pair.Value->LandPaint->SetScalarParameterValue(Name, Value); } }
}

void ACampaignMapActor::LandVector(FName Name, const FLinearColor& Value)
{
	if (LandMaterial) { LandMaterial->SetVectorParameterValue(Name, Value); }
	if (WorldLandMaterial) { WorldLandMaterial->SetVectorParameterValue(Name, Value); }
	for (const auto& Pair : FineSheets) { if (Pair.Value->LandPaint) { Pair.Value->LandPaint->SetVectorParameterValue(Name, Value); } }
}

void ACampaignMapActor::SeaScalar(FName Name, float Value)
{
	if (SeaMaterial) { SeaMaterial->SetScalarParameterValue(Name, Value); }
	if (Name == TEXT("Surf"))
	{
		// (the water's-edge line: the coarse world sheet has none, and a fine sheet's fades with the eye's height)
		SurfNow = Value;
		return;
	}
	if (WorldSeaMaterial) { WorldSeaMaterial->SetScalarParameterValue(Name, Value); }
	for (const auto& Pair : FineSheets) { if (Pair.Value->SeaPaint) { Pair.Value->SeaPaint->SetScalarParameterValue(Name, Value); } }
}

void ACampaignMapActor::UpdateSeaMotion(float CameraDistance, const FVector& CameraFocus)
{
	THEATER_SPEED_SCOPE(UpdateSeaMotion);
	if (!SeaMaterial)
	{
		return;
	}
	// ("-TheaterSeaMoves=0" stills the sea, for the frame rate before and after; another number tries another strength)
	static float Moves = -1.0f;
	if (Moves < 0.0f)
	{
		Moves = 1.0f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaMoves="), Moves);
		Moves = FMath::Clamp(Moves, 0.0f, 3.0f);
	}
	const float Moon = GetMoonlight();
	// (The art director's review of build 1432: by night the swell measured about 3.5% either way and she asked for
	// about twice that. The night's own light already halves what a crest adds, so the sea keeps its day's strength.)
	const float Hour = 1.0f;
	// (From the top, waves so small crawl like static: the chop goes as its crests come under about six pixels
	// apart, and the swell after it. On the paper map there is nothing: the material sees to that.)
	const float Chopping = 1.0f - FMath::SmoothStep(6500.0f, 9500.0f, CameraDistance);
	const float Swelling = 1.0f - FMath::SmoothStep(13000.0f, 20000.0f, CameraDistance);
	SeaScalar(TEXT("Swell"), 1.3f * Hour * Swelling * Moves);
	SeaScalar(TEXT("Chop"), 0.55f * Hour * Chopping * Moves);
	SeaScalar(TEXT("Shallows"), (1.0f - Moon) * Chopping * Moves); // (no flicker in the shallows by night)
	// (Release 2, tc.Map.Surf: the breakers' measure. They go with the chop as the eye rises, for from the top lines
	// so thin crawl like static. M_Sea has no such number, and to it this is nothing. Switched off in play, they are
	// put out once, and nothing more is set.)
	static bool bBreakersShown = false;
	if (SurfWanted())
	{
		SeaScalar(TEXT("Breakers"), FMath::Clamp(CVarSurfMuch.GetValueOnGameThread(), 0.0f, 2.0f) * Chopping * FMath::Min(Moves, 1.0f));
		bBreakersShown = true;
	}
	else if (bBreakersShown)
	{
		SeaScalar(TEXT("Breakers"), 0.0f);
		bBreakersShown = false;
	}
	LandScalar(TEXT("WatersMove"), Hour * Chopping * FMath::Min(Moves, 1.5f)); // (the land's painted lakes and rivers: the sea's recipe, weaker)
	SeaScalar(TEXT("Glints"), FMath::Lerp(1.0f, 0.7f, Moon) * FMath::Min(Moves, 1.0f));
	SeaScalar(TEXT("GlintElsewhere"), FMath::Lerp(0.1f, 0.0f, Moon)); // (by night one band and none outside it)
	SeaVector(TEXT("GlintColour"), FMath::Lerp(FLinearColor(FColor(215, 235, 250)), FLinearColor(FColor(131, 156, 197)), Moon));
	// The bright band: a soft patch on the lamp's side of what the eye looks at (the lamp stands to the west and a
	// little south, and high), which goes with the eye as a light's own reflection does.
	const double BandReach = FMath::Max(0.13 * CameraDistance, 190.0);
	const FVector Heart = CameraFocus + FVector(-0.25, -0.55, 0.0) * (0.14 * CameraDistance);
	SeaVector(TEXT("GlintAt"), FLinearColor(static_cast<float>(Heart.X), static_cast<float>(Heart.Y), static_cast<float>(1.0 / (BandReach * BandReach)), 0.0f));

	// THE FOG OF WAR AS A HAZE: its wisps by the hour and by the eye's height. By day they are a pale slate; by night
	// they are paler than the ground under them, like mist under a moon. From high up they fade into a flat veil.
	// ("-TheaterHaze=0.67,0.5,0.38" tries another darkness, drain and thickness of wisp; "-TheaterHazeInk=r,g,b,r,g,b"
	// other colours for the wisps by day and by night, as the land's paint takes them.)
	// (Tuned by measure on Paris from playing height, 16 November 2026: France in view stood at 121 of brightness; the
	// brief's two thirds is about 81, with the pockets near 55 and the wisps near 101. The paint's 0.4 comes out as
	// two thirds on the screen, since the lens lifts dark paint.)
	// (The art director's review of the haze as built, 16 November 2026. Mend 1: the hazed ground was ash, (88, 91, 91);
	// it takes a cold slate cast, laid on the paint stronger than it is seen, since the lamp is warm. Mend 2: a floor
	// of haze over the pockets, and gentler wisps. Mend 5: by night the haze added blue where it should take it away:
	// there the ground is drained further, its cast is turned warm against the moon's blue, and the wisps are grey.
	// "-TheaterHazeCast=r,g,b,r,g,b" tries other casts by day and by night; "-TheaterHazeFloor=0.4,0.5" another floor
	// and another drain by night.)
	// (Measured on France from Orleans at playing height, as she measures it, 17 November: the hazed ground's mean
	// went from (77, 78, 78) with a colour of 3 to a cold slate with a colour near 12; the pockets from 0.58 of the
	// middle to 0.69, the quarters from 0.82 and 1.15 to 0.88 and 1.11, the palest wisps from 1.33 to 1.22. By night
	// unseen ground went from (16, 32, 63), bluer than the ground in view, to (27, 39, 52).)
	static float HazeDark = 0.33f, HazeDrain = 0.5f, HazeWisps = 0.34f, HazeFloor = 0.55f, HazeDrainNight = 0.75f;
	static FLinearColor HazeDay(0.13f, 0.165f, 0.21f), HazeNight(0.24f, 0.22f, 0.17f);
	static FLinearColor HazeCastDay(0.79f, 1.0f, 1.22f), HazeCastNight(0.68f, 0.95f, 0.80f); // (ruling 205(a): the moon's light is less blue now, so the haze's own cast by night gives up its red to keep the fog the colour it was built)
	static bool bHazeRead = false;
	if (!bHazeRead)
	{
		bHazeRead = true;
		FString Asked;
		TArray<FString> Bits;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterHazeCast="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(",")) == 6)
		{
			HazeCastDay = FLinearColor(FCString::Atof(*Bits[0]), FCString::Atof(*Bits[1]), FCString::Atof(*Bits[2]));
			HazeCastNight = FLinearColor(FCString::Atof(*Bits[3]), FCString::Atof(*Bits[4]), FCString::Atof(*Bits[5]));
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterHazeFloor="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(",")) == 2)
		{
			HazeFloor = FCString::Atof(*Bits[0]);
			HazeDrainNight = FCString::Atof(*Bits[1]);
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterHaze="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(",")) == 3)
		{
			HazeDark = FCString::Atof(*Bits[0]);
			HazeDrain = FCString::Atof(*Bits[1]);
			HazeWisps = FCString::Atof(*Bits[2]);
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterHazeInk="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(",")) == 6)
		{
			HazeDay = FLinearColor(FCString::Atof(*Bits[0]), FCString::Atof(*Bits[1]), FCString::Atof(*Bits[2]));
			HazeNight = FLinearColor(FCString::Atof(*Bits[3]), FCString::Atof(*Bits[4]), FCString::Atof(*Bits[5]));
		}
	}
	// (by night the same paint came out at 57% of ground in view, where two thirds is asked: it is lifted a little there)
	LandScalar(TEXT("HazeDark"), FMath::Lerp(HazeDark, HazeDark * ACampaignEnvironment::MoonLook().Haze, Moon)); // (ruling 205(a): the fog by night stays as built while the ground in view is lifted)
	LandScalar(TEXT("HazeDrain"), FMath::Lerp(HazeDrain, HazeDrainNight, Moon));
	LandScalar(TEXT("HazeFloor"), HazeFloor * FMath::Lerp(1.0f, ACampaignEnvironment::MoonLook().HazeFloor, Moon)); // (ruling 205(a): by night the floor comes down with the share, so the fog stays as dark as it was built)
	{
		// How far the haze has travelled, in the table's units: the game's clock times its pace, 7.3 units a second,
		// about five pixels a second from where he plays (the art director: never over eight). It goes about twenty
		// degrees north of east, off the bands' own long axis, so that their edges creep across the province lines.
		// Its picture turns once in 2,920 units, each way.
		// ("-TheaterHazeDrift=7.3" tries another pace, in units a second; "-TheaterHazeTravel=30" holds the haze where
		// it stands after that many units, to measure a picture against another.)
		static float HazeDrift = -1.0f, HazeHeld = -1.0f;
		if (HazeDrift < 0.0f)
		{
			HazeDrift = 7.3f;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterHazeDrift="), HazeDrift);
			FParse::Value(FCommandLine::Get(), TEXT("TheaterHazeTravel="), HazeHeld);
		}
		const double Clock = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		const double Gone = HazeHeld >= 0.0f ? HazeHeld : Clock * HazeDrift;
		LandVector(TEXT("HazeGone"), FLinearColor(static_cast<float>(FMath::Fmod(Gone * 0.9397, 2920.0)), static_cast<float>(FMath::Fmod(Gone * 0.342, 2920.0)), 0.0f, 0.0f));
	}
	LandVector(TEXT("HazeCast"), FMath::Lerp(HazeCastDay, HazeCastNight, Moon));
	LandScalar(TEXT("HazeWisps"), HazeWisps);
	LandVector(TEXT("HazeColour"), FMath::Lerp(HazeDay, HazeNight, Moon));
	LandScalar(TEXT("HazeWispy"), 1.0f - FMath::SmoothStep(7000.0f, 13000.0f, CameraDistance));
}

void ACampaignMapActor::SeaVector(FName Name, const FLinearColor& Value)
{
	if (SeaMaterial) { SeaMaterial->SetVectorParameterValue(Name, Value); }
	if (WorldSeaMaterial) { WorldSeaMaterial->SetVectorParameterValue(Name, Value); }
	for (const auto& Pair : FineSheets) { if (Pair.Value->SeaPaint) { Pair.Value->SeaPaint->SetVectorParameterValue(Name, Value); } }
}

void ACampaignMapActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Blasts.IsValid())
	{
		Blasts->End();
		Blasts.Reset();
	}
	if (FogAsideBuilding.IsValid())
	{
		FogAsideBuilding.Wait(); // (it reads the world's sheet, which goes with the map)
	}
	FMiniatureMesh::StopWarm(); // (speed: the model file in flight is read by a thread that must not outlive the table)
	for (const auto& Pair : FineSheets)
	{
		if (Pair.Value->Building.IsValid())
		{
			Pair.Value->Building.Wait();
		}
	}
	Super::EndPlay(EndPlayReason);
}

const ACampaignMapActor::FFineSheet* ACampaignMapActor::FineSheetAt(double InX, double Y) const
{
	const double X = WrapX(InX);
	for (const auto& Pair : FineSheets)
	{
		const FFineSheet& Sheet = *Pair.Value;
		if (Sheet.bStanding && X >= Sheet.Box.Min.X && Y >= Sheet.Box.Min.Y && X <= Sheet.Box.Max.X && Y <= Sheet.Box.Max.Y)
		{
			return &Sheet;
		}
	}
	return nullptr;
}

FVector ACampaignMapActor::SurfacePoint(double X, double Y, double Lift) const
{
	const double Ground = FMath::Max(static_cast<double>(GroundAt(X, Y)), 0.0);
	return MapToWorld(X, Y, Ground + Lift);
}

FVector ACampaignMapActor::SurfacePointOver(double X, double Y, double Lift) const
{
	const double Reach = 0.8;
	double Ground = FMath::Max(static_cast<double>(GroundAt(X, Y)), 0.0);
	for (const FVector2D& Step : { FVector2D(Reach, 0.0), FVector2D(-Reach, 0.0), FVector2D(0.0, Reach), FVector2D(0.0, -Reach) })
	{
		Ground = FMath::Max(Ground, static_cast<double>(GroundAt(X + Step.X, Y + Step.Y)));
	}
	return MapToWorld(X, Y, Ground + Lift);
}

namespace
{
	const double KeptSquare = 4.0; // (map units: the squares SurfacePointKept keeps its points by)
}

FVector ACampaignMapActor::SurfacePointKept(double X, double Y, double Lift) const
{
	if (!TheaterSpeed::KeptHeights())
	{
		return SurfacePointOver(X, Y, Lift);
	}
	if (KeptPointsFor != Data.Regions.Num() || KeptPointsScale != VerticalScale * UnitScale)
	{
		KeptPoints.Reset();
		KeptPointsFor = Data.Regions.Num();
		KeptPointsScale = VerticalScale * UnitScale;
	}
	// (by where the ground under it is read: GroundAt takes each place round the world to the turn it is kept in)
	const double Kept = WrapX(X);
	TMap<FVector, FVector>& Near = KeptPoints.FindOrAdd(FIntPoint(FMath::FloorToInt32(Kept / KeptSquare), FMath::FloorToInt32(Y / KeptSquare)));
	const FVector Key(X, Y, Lift);
	if (const FVector* Known = Near.Find(Key))
	{
		return *Known;
	}
	const FVector Made = SurfacePointOver(X, Y, Lift);
	Near.Add(Key, Made);
	return Made;
}

void ACampaignMapActor::ForgetKeptPoints(const FBox2D& Box)
{
	ForgetBorderEdges(Box); // (the frontier lines kept on those points: tc.Speed.BordersKept)
	if (KeptPoints.Num() == 0 || !Box.bIsValid)
	{
		return;
	}
	// (a point kept reads the ground up to 0.8 units round it: everything kept that near the sheet goes)
	const double Margin = 1.0;
	const int32 J0 = FMath::FloorToInt32((Box.Min.Y - Margin) / KeptSquare), J1 = FMath::FloorToInt32((Box.Max.Y + Margin) / KeptSquare);
	auto Drop = [&](double X0, double X1)
	{
		if (X1 < X0)
		{
			return;
		}
		const int32 I0 = FMath::FloorToInt32(X0 / KeptSquare), I1 = FMath::FloorToInt32(X1 / KeptSquare);
		for (int32 I = I0; I <= I1; ++I)
		{
			for (int32 J = J0; J <= J1; ++J)
			{
				KeptPoints.Remove(FIntPoint(I, J));
			}
		}
	};
	double X0 = Box.Min.X - Margin, X1 = Box.Max.X + Margin;
	if (!Data.bWorld || Data.WorldWidth <= 0.0)
	{
		Drop(X0, X1);
		return;
	}
	// (in the turn of the world the sheets are kept in; what reaches over its eastern or western end, on the other side)
	const double Shift = WrapX(Box.Min.X) - Box.Min.X;
	X0 += Shift;
	X1 += Shift;
	const double Start = Data.WorldMinX, End = Data.WorldMinX + Data.WorldWidth;
	Drop(FMath::Max(X0, Start), FMath::Min(X1, End));
	if (X0 < Start)
	{
		Drop(X0 + Data.WorldWidth, End);
	}
	if (X1 > End)
	{
		Drop(Start, X1 - Data.WorldWidth);
	}
}

int32 ACampaignMapActor::RegionAtWorld(const FVector& World) const
{
	if (!Data.IsLoaded())
	{
		return INDEX_NONE;
	}
	const FVector2D P = WorldToMap(World);
	const int32 Exact = Data.FindRegionAt(P.X, P.Y);
	if (Exact != INDEX_NONE)
	{
		return Exact;
	}
	// The mounds raised for very small islands are wider than the islands themselves.
	if (P.X >= 0.0 && P.Y >= 0.0 && P.X <= Data.Width && P.Y <= Data.Height && Terrain.CoverageAt(P.X, P.Y) >= 0.5f)
	{
		const int32 I = FMath::Clamp(FMath::RoundToInt32(P.X / Terrain.Cell), 0, Terrain.NX - 1);
		const int32 J = FMath::Clamp(FMath::RoundToInt32(P.Y / Terrain.Cell), 0, Terrain.NY - 1);
		const int16 Id = Terrain.Region[Terrain.Index(I, J)];
		if (Id >= 0)
		{
			return Id;
		}
	}
	// (Release 2, the whole world.) So is the mound the world's own sheet raises for one of the Pacific's islets or any
	// other island too small for it (Oahu, Okinawa, Samoa), which is all he sees of it from the high view: a click
	// anywhere on the speck is a click on the island. Where a fine sheet stands the island is drawn at its own size,
	// and only its own ground is the island.
	if (Data.bWorld && WorldTerrain.NX > 0)
	{
		const double X = WrapX(P.X);
		if ((X < 0.0 || P.Y < 0.0 || X > Data.Width || P.Y > Data.Height) && !FineSheetAt(X, P.Y) && WorldTerrain.CoverageAt(X, P.Y) >= 0.5f)
		{
			const int32 I = FMath::Clamp(FMath::RoundToInt32((X - WorldTerrain.OriginX) / WorldTerrain.Cell), 0, WorldTerrain.NX - 1);
			const int32 J = FMath::Clamp(FMath::RoundToInt32((P.Y - WorldTerrain.OriginY) / WorldTerrain.Cell), 0, WorldTerrain.NY - 1);
			const int16 Id = WorldTerrain.Region[WorldTerrain.Index(I, J)];
			if (Id >= 0 && Data.Regions.IsValidIndex(Id) && WorldTerrain.Raised.Contains(Id))
			{
				return Id;
			}
		}
	}
	return INDEX_NONE;
}

FVector2D ACampaignMapActor::GetWorldHalfExtent() const
{
	return FVector2D(Data.Height * 0.5 * UnitScale, Data.Width * 0.5 * UnitScale);
}

void ACampaignMapActor::LoadMaterials()
{
	UMaterialInterface* Fallback = UMaterial::GetDefaultMaterial(MD_Surface);
	auto Load = [Fallback](const TCHAR* Path) -> UMaterialInterface*
	{
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path);
		if (!Material)
		{
			UE_LOG(LogTheater, Warning, TEXT("Material %s is missing. Run Tools/build_content.py to create it."), Path);
			return Fallback;
		}
		return Material;
	};
	SurfaceMaterial = Load(SurfaceMaterialPath);
	SolidMaterial = Load(SolidMaterialPath);
	HighlightMaterial = Load(HighlightMaterialPath);
	BeamMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Beam.M_Beam"));
	// (the foam of a ship's wake: without its material the fleets simply have none)
	if (UMaterialInterface* Foam = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Wake.M_Wake")))
	{
		WakeInk = UMaterialInstanceDynamic::Create(Foam, this);
		FString Asked;
		TArray<FString> Parts;
		// ("-TheaterWake=brightness,night,strength[,sharp,drift[,centre,contrast]]")
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterWake="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) >= 3)
		{
			WakeLook = FVector(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), FCString::Atod(*Parts[2]));
			if (Parts.Num() >= 5)
			{
				WakeInk->SetScalarParameterValue(TEXT("Sharp"), FCString::Atof(*Parts[3]));
				WakeInk->SetScalarParameterValue(TEXT("Drift"), FCString::Atof(*Parts[4]));
			}
			if (Parts.Num() >= 7)
			{
				WakeInk->SetScalarParameterValue(TEXT("Centre"), FCString::Atof(*Parts[5]));
				WakeInk->SetScalarParameterValue(TEXT("Contrast"), FCString::Atof(*Parts[6]));
			}
		}
		// (no wake beyond the west edge of the map, where the table's frame begins: it thins out over the last three map units)
		WakeInk->SetScalarParameterValue(TEXT("WestEdge"), static_cast<float>(MapToWorld(1.0, 0.0).Y));
		WakeInk->SetScalarParameterValue(TEXT("EdgeSoft"), static_cast<float>(1.0 / (3.0 * UnitScale)));
		WakeInk->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(WakeLook.X));
		WakeInk->SetScalarParameterValue(TEXT("Opacity"), static_cast<float>(WakeLook.Z));
	}
	// Without the flag material the standards fall back to plain colours, so it is not an error for it to be missing.
	FlagMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Flag.M_Flag"));
	if (!FlagMaterial)
	{
		UE_LOG(LogTheater, Warning, TEXT("The flag material is missing, so standards are plain. Run Tools/build_content.sh to create it."));
	}

	auto MakeSurface = [this](float Roughness, float Specular, float Grain, float Tooth)
	{
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(SurfaceMaterial, this);
		Instance->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		Instance->SetScalarParameterValue(TEXT("Specular"), Specular);
		Instance->SetScalarParameterValue(TEXT("Grain"), Grain);
		Instance->SetScalarParameterValue(TEXT("Tooth"), Tooth);
		return Instance;
	};
	InkMaterial = MakeSurface(0.8f, 0.2f, 0.0f, 0.0f);
	// The quiet lines: drawn in ink that can be thinned as the eye rises.
	auto MakeInk = [this]()
	{
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		Instance->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
		return Instance;
	};
	ProvinceInk = MakeInk();
	ProvincePaleInk = MakeInk();
	RailInk = MakeInk();
	SleeperInk = MakeInk();
	GunEarthInk = MakeInk(); // (the trodden earth under an army's guns)
	CutLampInk = MakeInk(); // (the lamp of a cut railway)
	// (The red glow of a burning town by night is added light: M_Glow. Without that material it is laid over as a
	// wash, which is weaker and flatter, and the log says so.)
	if (UMaterialInterface* GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Glow.M_Glow")))
	{
		FireGlowInk = UMaterialInstanceDynamic::Create(GlowMaterial, this);
		bFireGlowAdds = true;
	}
	else
	{
		FireGlowInk = MakeInk();
		UE_LOG(LogTheater, Warning, TEXT("The firelight's material is missing (M_Glow: run Tools/build_content.sh); a burning town's glow is laid over as a wash"));
	}
	RailLightInk = MakeInk(); // (the lit railway of the army in hand)
	RailLightInk->SetScalarParameterValue(TEXT("Brightness"), 1.2f);
	RailTintInk = MakeInk();
	// The pieces: painted metal and cloth, not bare plastic. A fine grain in the paint and a slight tooth to the
	// surface catch the lamp, at a scale that suits things the size of a model.
	FMiniatureMesh::bTrialTank = FParse::Param(FCommandLine::Get(), TEXT("TheaterTrialTank")); // (the sculpted trial tank on every armoured base, to be judged)
	FMiniatureMesh::bCodeBuilt = FParse::Param(FCommandLine::Get(), TEXT("TheaterCodeBuilt")); // (the vehicles built in code, where sculpted ones would stand)
	PieceMaterial = MakeSurface(0.56f, 0.4f, 0.1f, 0.3f);
	PieceMaterial->SetScalarParameterValue(TEXT("GrainTilingCoarse"), 0.012f);
	PieceMaterial->SetScalarParameterValue(TEXT("GrainTilingFine"), 0.06f);
	PieceMaterial->SetScalarParameterValue(TEXT("ToothTiling"), 0.09f);
	{
		// The paint job (art fix 12): the pieces' paint is weathered, and on their metal it is chipped to the bare
		// metal in patches. ("-TheaterWear=wear,chip tiling,blot tiling,chip at,chip sharp" tries others; 0 for none.)
		float Worn[7] = { 1.0f, 0.016f, 0.005f, -0.56f, 10.0f, 0.5f, 0.26f };
		FString Asked;
		TArray<FString> Bits;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterWear="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(",")) > 0)
		{
			for (int32 K = 0; K < Bits.Num() && K < 7; ++K) { Worn[K] = FCString::Atof(*Bits[K]); }
		}
		FMiniatureMesh::bOldPieces = FParse::Param(FCommandLine::Get(), TEXT("TheaterOldPieces"));
		PieceMaterial->SetScalarParameterValue(TEXT("Wear"), FMiniatureMesh::bOldPieces ? 0.0f : Worn[0]);
		PieceMaterial->SetScalarParameterValue(TEXT("ChipTiling"), Worn[1]);
		PieceMaterial->SetScalarParameterValue(TEXT("BlotTiling"), Worn[2]);
		PieceMaterial->SetScalarParameterValue(TEXT("ChipAt"), Worn[3]);
		PieceMaterial->SetScalarParameterValue(TEXT("ChipSharp"), Worn[4]);
		PieceMaterial->SetScalarParameterValue(TEXT("ChipGather"), Worn[5]); // (how much more readily the paint gives way on a nose, a track guard or a hatch)
		PieceMaterial->SetScalarParameterValue(TEXT("TopLight"), Worn[6]); // (how much paler what faces the sky is than what faces the side)
		PieceMaterial->SetVectorParameterValue(TEXT("ChipColour"), FLinearColor(0.3f, 0.285f, 0.26f)); // (bare steel, a little rusted)
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")))
	{
		PieceMaterial = MakeSurface(0.5f, 0.38f, 0.0f, 0.0f); // as it was: smooth and a little shiny, like moulded plastic
	}
	LightFigures(PieceMaterial, 0.0f);
	SceneryMaterial = MakeSurface(0.85f, 0.2f, 0.0f, 0.0f);
	WoodsMaterial = MakeSurface(0.85f, 0.2f, 0.0f, 0.0f);

	// The land and the sea take their colour from pictures painted in PaintGround.
	LandMaterial = UMaterialInstanceDynamic::Create(Load(TEXT("/Game/Campaign/Materials/M_Land.M_Land")), this);
	UMaterialInterface* SeaParent = Load(TEXT("/Game/Campaign/Materials/M_Sea.M_Sea"));
	if (SurfWanted())
	{
		// (Release 2, tc.Map.Surf: the sea with breakers. The world's sea and every fine sheet's take this one's parent.)
		if (UMaterialInterface* WithBreakers = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_SeaSurf.M_SeaSurf")))
		{
			SeaParent = WithBreakers;
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Surf: tc.Map.Surf is on, but M_SeaSurf is missing (run Tools/build_content.sh to make it). The sea is drawn as before."));
		}
	}
	SeaMaterial = UMaterialInstanceDynamic::Create(SeaParent, this);
	{
		// "-TheaterNightSea=grey,surf,r,g,b,liftr,liftg,liftb" and "-TheaterNightRiver=r,g,b" try another grading of
		// the water by moonlight; "-TheaterNightGlow=wash,line" other strengths for the wash under a lit town and the
		// line of glow under a front.
		FString Asked;
		TArray<FString> Parts;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterNightSea="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 8)
		{
			SeaNight = FVector2f(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
			SeaNightTint = FLinearColor(FCString::Atof(*Parts[2]), FCString::Atof(*Parts[3]), FCString::Atof(*Parts[4]));
			SeaNightLift = FLinearColor(FCString::Atof(*Parts[5]), FCString::Atof(*Parts[6]), FCString::Atof(*Parts[7]));
		}
		// ("-TheaterSeaRings=fill,fill under the pointer,line's night,its strength,fill's night,its strength,sighting's night,trees' snow r,g,b,wood floor r,g,b")
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaRings="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) >= 7)
		{
			auto Number = [&Parts](int32 Index) { return FCString::Atof(*Parts[Index]); };
			SeaFill = FVector2D(Number(0), Number(1));
			SeaLineNight = FLinearColor(Number(2), Number(2), Number(2) * 1.05f, Number(3));
			SeaFillNight = FLinearColor(Number(4), Number(4), Number(4) * 1.05f, Number(5));
			SightingNight = FLinearColor(Number(6), Number(6), Number(6) * 1.04f, 1.0f);
			if (Parts.Num() >= 10)
			{
				SnowOnTrees = FLinearColor(Number(7), Number(8), Number(9));
			}
			if (Parts.Num() >= 13)
			{
				SnowWoodFloor = FLinearColor(Number(10), Number(11), Number(12));
			}
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterNightRiver="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 3)
		{
			RiverNightTint = FLinearColor(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterNightGlow="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 2)
		{
			TownWash = FCString::Atof(*Parts[0]);
			FrontLineGlow = FCString::Atof(*Parts[1]);
		}
	}

	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
}

UMaterialInstanceDynamic* ACampaignMapActor::GetSolidMaterial(const FLinearColor& Color, float Roughness, float Metallic, float Glow)
{
	const FString Key = FString::Printf(TEXT("%s|%.2f|%.2f|%.2f"), *Color.ToFColor(true).ToHex(), Roughness, Metallic, Glow);
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = SolidMaterials.Find(Key))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(SolidMaterial, this);
	Instance->SetVectorParameterValue(TEXT("Color"), Color);
	Instance->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	Instance->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	Instance->SetScalarParameterValue(TEXT("Glow"), Glow);
	SolidMaterials.Add(Key, Instance);
	return Instance;
}

UTexture2D* ACampaignMapActor::GetFlagTexture(const FString& Tag)
{
	if (TObjectPtr<UTexture2D>* Found = FlagTextures.Find(Tag))
	{
		return *Found;
	}
	const FCampaignNation* Nation = Data.FindNation(Tag);
	TArray<FLinearColor> Pixels;
	if (!CampaignFlags::Paint(Tag, Nation ? FLinearColor(Nation->Color) : FLinearColor(0.4f, 0.4f, 0.4f), Pixels))
	{
		UE_LOG(LogTheater, Display, TEXT("No flag is drawn yet for %s; its standards carry its colour."), *Tag);
	}
	UTexture2D* Texture = CampaignFlags::MakeTexture(Pixels, MakeUniqueObjectName(this, UTexture2D::StaticClass(), *FString::Printf(TEXT("Flag_%s"), *Tag)));
	FlagTextures.Add(Tag, Texture);
	return Texture;
}

UMaterialInterface* ACampaignMapActor::GetFlagMaterial(const FString& Tag, int32 Wear, bool bGlimpsed)
{
	if (!FlagMaterial)
	{
		return nullptr;
	}
	const FString Key = (Wear > 0 ? FString::Printf(TEXT("%s|w%d"), *Tag, Wear) : Tag) + (bGlimpsed ? TEXT("|glimpsed") : TEXT(""));
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = FlagMaterials.Find(Key))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(FlagMaterial, this);
	Instance->SetTextureParameterValue(TEXT("Flag"), GetFlagTexture(Tag));
	Instance->SetScalarParameterValue(TEXT("Glow"), 0.5f * (1.0f - 0.4f * GetMoonlight()));
	// (a battered army's flag is dirtied a very little, and a spent one's a little more: never bleached)
	Instance->SetScalarParameterValue(TEXT("Fade"), Wear >= 2 ? 0.2f : Wear == 1 ? 0.1f : 0.0f);
	// (ruling 154: at a glimpse the cloth is drained a quarter, with a tenth of the haze's colour over it)
	Instance->SetScalarParameterValue(TEXT("Drain"), bGlimpsed ? 0.25f : 0.0f);
	Instance->SetScalarParameterValue(TEXT("Veiled"), bGlimpsed ? 0.1f : 0.0f);
	FlagMaterials.Add(Key, Instance);
	return Instance;
}

namespace
{
	// Each tile of the land is this many quads across, whatever its level.
	constexpr int32 TileQuads = 64;
}

void ACampaignMapActor::PaintGround()
{
	const double StartTime = FPlatformTime::Seconds();
	Paint.Build(Data, Terrain, VerticalScale);

	// The pictures the land's colour comes from: one pixel for every point of the terrain.
	GroundPicture = CampaignTextures::Create(Paint.NX, Paint.NY, Paint.Ground, true, false, TEXT("TableGround"));
	TintPicture = CampaignTextures::Create(Paint.NX, Paint.NY, Paint.Tint, true, false, TEXT("TableTint"));
	ReliefPicture = CampaignTextures::Create(Paint.NX, Paint.NY, Paint.Relief, false, false, TEXT("TableRelief"));
	MasksPicture = CampaignTextures::Create(Paint.NX, Paint.NY, Paint.Masks, false, false, TEXT("TableMasks"));
	MasksSentFor = -1.0f; // (made without snow: the season lays its own the first time it is told the month)
	CoverPicture = CampaignTextures::Create(Paint.NX, Paint.NY, Paint.Cover, false, false, TEXT("TableCover"));
	// (an angle half way between two provinces' angles is neither's: no smoothing)
	// (Speed, tc.Speed.PictureOnce: made unsmoothed from the first, not made and then made again, which waits for the
	// drawing thread. The same picture either way.)
	const bool bPictureOnce = TheaterSpeed::PictureOnce();
	LiePicture = CampaignTextures::Create(Paint.NX, Paint.NY, Paint.Lie, false, false, TEXT("TableLie"), bPictureOnce);
	if (LiePicture && !bPictureOnce)
	{
		LiePicture->Filter = TF_Nearest;
		LiePicture->UpdateResource();
	}
	{
		// Where nobody of the player's can see: nowhere, until the war says otherwise.
		TArray<FColor> Clear;
		Clear.Init(FColor(0, 0, 0, 255), Paint.NX * Paint.NY);
		FogPicture = CampaignTextures::Create(Paint.NX, Paint.NY, Clear, false, false, TEXT("TableFog"));
	}
	WaterPicture = CampaignTextures::Create(Paint.WaterNX, Paint.WaterNY, Paint.Water, true, false, TEXT("TableWater"));
	ShorePicture = CampaignTextures::Create(Paint.WaterNX, Paint.WaterNY, Paint.Surf, false, false, TEXT("TableShore"));

	// The fine detail that repeats over it: grass, tree-tops, rock and sand, and the ripples on the water.
	TArray<FColor> Pixels;
	CampaignTextures::MakeGroundDetail(Pixels);
	DetailPicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableDetail"));
	CampaignTextures::MakeGroundBumps(Pixels);
	BumpsPicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableBumps"));
	// The fields of the quilt: one pixel a field, read without smoothing so that each keeps its own tone and its edge.
	CampaignTextures::MakeFields(Pixels);
	{
		// (the mean of the quilt's own tones, as the land's paint reads them: what the strips fade toward from high up)
		double Sum[3] = { 0.0, 0.0, 0.0 };
		for (const FColor& Pixel : Pixels)
		{
			Sum[0] += Pixel.R;
			Sum[1] += Pixel.G;
			Sum[2] += Pixel.B;
		}
		const double Share = Pixels.Num() > 0 ? 2.0 / (255.0 * Pixels.Num()) : 0.0;
		QuiltMean = Pixels.Num() > 0 ? FLinearColor(Sum[0] * Share, Sum[1] * Share, Sum[2] * Share) : FLinearColor::White;
	}
	FieldsPicture = CampaignTextures::Create(CampaignTextures::FieldsSize, CampaignTextures::FieldsSize, Pixels, false, true, TEXT("TableFields"), bPictureOnce);
	if (FieldsPicture && !bPictureOnce)
	{
		FieldsPicture->Filter = TF_Nearest;
		FieldsPicture->UpdateResource();
	}
	CampaignTextures::MakeWaterBumps(Pixels);
	RipplesPicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableRipples"));
	CampaignTextures::MakeWaves(Pixels);
	WavesPicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableWaves"));
	CampaignTextures::MakeHaze(Pixels);
	HazePicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableHaze"));

	LandMaterial->SetTextureParameterValue(TEXT("Ground"), GroundPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Tint"), TintPicture);
	{
		// (the ownership paint: where the ground is occupied, and the colour its stripes take)
		if (Paint.Occupied.Num() != Paint.NX * Paint.NY)
		{
			Paint.Occupied.Init(FColor(0, 0, 0, 0), Paint.NX * Paint.NY);
		}
		OccupiedPicture = CampaignTextures::Create(Paint.NX, Paint.NY, Paint.Occupied, true, false, TEXT("TableOccupied"));
		LandMaterial->SetTextureParameterValue(TEXT("Occupied"), OccupiedPicture);
		// ("-TheaterLandScalar=StripesThinSnow:0.4,Band:0.3" tries other numbers in the land's paint without a rebuild)
		FString ScalarsAsked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandScalar="), ScalarsAsked, false))
		{
			TArray<FString> Each;
			ScalarsAsked.ParseIntoArray(Each, TEXT(","));
			for (const FString& One : Each)
			{
				FString ScalarName, ScalarValue;
				if (One.Split(TEXT(":"), &ScalarName, &ScalarValue))
				{
					LandMaterial->SetScalarParameterValue(FName(*ScalarName), FCString::Atof(*ScalarValue));
				}
			}
		}
		// ("-TheaterLandVector=FieldMean:0.85/0.94/0.87" does the same for a colour of the land's paint)
		FString VectorsAsked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandVector="), VectorsAsked, false))
		{
			TArray<FString> Each;
			VectorsAsked.ParseIntoArray(Each, TEXT(","));
			for (const FString& One : Each)
			{
				FString VectorName, VectorValue;
				TArray<FString> Parts;
				if (One.Split(TEXT(":"), &VectorName, &VectorValue) && VectorValue.ParseIntoArray(Parts, TEXT("/")) == 3)
				{
					LandMaterial->SetVectorParameterValue(FName(*VectorName), FLinearColor(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]), 1.0f));
				}
			}
		}
		// ("-TheaterStripes=0" takes the stripes of occupied ground away, to measure them against the plain stain)
		float StripesAsked = 1.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterStripes="), StripesAsked))
		{
			LandMaterial->SetScalarParameterValue(TEXT("Stripes"), StripesAsked);
		}
		TArray<FColor> Free;
		Free.Init(FColor(0, 0, 0, 0), 16);
		NoStripesPicture = CampaignTextures::Create(4, 4, Free, true, false, TEXT("TableNoStripes"));
	}
	LandMaterial->SetTextureParameterValue(TEXT("Relief"), ReliefPicture);
	LandScalar(TEXT("Band"), 0.4f);
	LandMaterial->SetTextureParameterValue(TEXT("Masks"), MasksPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Cover"), CoverPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Fields"), FieldsPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Lie"), LiePicture);
	LandMaterial->SetTextureParameterValue(TEXT("Fog"), FogPicture);
	{
		// (Europe's own ground has the rules' rivers, laid as ribbons, and none painted: its picture of the waters says "none" everywhere)
		TArray<FColor> Dry;
		Dry.Init(FColor(255, 255, 0, 0), 16);
		NoWatersPicture = CampaignTextures::Create(4, 4, Dry, false, false, TEXT("TableNoWaters"));
		LandMaterial->SetTextureParameterValue(TEXT("Wet"), NoWatersPicture);
		LandMaterial->SetTextureParameterValue(TEXT("WetInk"), NoWatersPicture);
	}
	// (what stands on unseen ground is drawn, not coloured: the towns and the woods read the same picture by where they stand)
	for (UMaterialInstanceDynamic* Stands : { SceneryMaterial.Get(), WoodsMaterial.Get() })
	{
		if (Stands && Data.Width > 0.0 && Data.Height > 0.0)
		{
			Stands->SetTextureParameterValue(TEXT("Fog"), FogPicture);
			Stands->SetVectorParameterValue(TEXT("FogFrame"), FLinearColor(1.0f / (UnitScale * Data.Width), 0.5f, -1.0f / (UnitScale * Data.Height), 0.5f));
			Stands->SetScalarParameterValue(TEXT("Drawn"), 1.0f);
			Stands->SetScalarParameterValue(TEXT("DrawnDark"), 0.45f); // (a town or a wood under the haze: as dark as the ground it stands on)
		}
	}
	// (the nation's stain lies over the canopy at a little under half its strength: a wood is green first, and its owner's second)
	LandScalar(TEXT("WoodsStain"), 0.42f);
	LandVector(TEXT("FirColour"), FLinearColor(0.015f, 0.034f, 0.028f));
	{
		// (the whole-world map) The canopy of the tropics: the art director's jungle, seen at about (54, 87, 56).
		// ("-TheaterJungle=r,g,b" tries another.)
		FLinearColor Jungle(0.043f, 0.088f, 0.066f); // (an eighth down on what it was, and a little less red) // (cooler and a step darker than it first was: lit canopy stays under 0.36, its blue within 32 of its green)
		FString Asked;
		TArray<FString> Parts;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterJungle="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 3)
		{
			Jungle = FLinearColor(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
		}
		LandVector(TEXT("TropicColour"), Jungle);
		// The canopy of the northern forest, seen at about (59, 79, 67). ("-TheaterTaiga=r,g,b" tries another.)
		FLinearColor Taiga(0.046f, 0.086f, 0.097f);
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterTaiga="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 3)
		{
			Taiga = FLinearColor(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
		}
		LandVector(TEXT("NorthColour"), Taiga);
		// The broadleaf woods of the temperate lands beyond Europe: the art director's (50, 64, 46), its blue within
		// 16 of its green. ("-TheaterFarWoods=r,g,b" tries another.) (The paint is far bluer than what is seen: the
		// lamp is warm, and the ground's own colour shows through the canopy as a yellow-green.)
		FLinearColor FarWoods(0.011f, 0.058f, 0.042f); // (greener than it first was: the art director's first figure had been read off woods under Germany's grey stain)
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFarWoods="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 3)
		{
			FarWoods = FLinearColor(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
		}
		LandVector(TEXT("FarWoodsColour"), FarWoods);
		// The New World's square fields: fallow, wheat and green, as what each does to the ground under it.
		// (The art director's swatches are (156, 130, 94), (204, 180, 106) and (146, 154, 94) on prairie ground.)
		const FLinearColor Fallow(1.065f, 0.86f, 0.91f), Wheat(1.93f, 1.47f, 0.79f), Green(0.88f, 1.33f, 0.95f); // (fallow a good deal lighter than it first was, wheat gold and not cream)
		LandVector(TEXT("FieldFallow"), Fallow);
		LandVector(TEXT("FieldWheat"), Wheat);
		LandVector(TEXT("FieldGreen"), Green);
		// (a part in five is green and a part in five fallow, so that the plain reads gold; and what the crops fade toward from high up)
		LandVector(TEXT("FieldMean"), Wheat * 0.6f + Fallow * 0.2f + Green * 0.2f);
		LandVector(TEXT("QuiltMean"), QuiltMean);
	}
	{
		// Woods on a warm-stained nation (Poland's red, Holland's orange) went chocolate: a very dark green takes
		// on the least red that shows through it. There the canopy takes almost none of the stain, is laid on a
		// little thicker, and is itself olive. ("-TheaterWarmWoods=less,more,r,g,b" tries others.)
		float Less = 1.0f, More = 0.13f;
		FLinearColor Olive(2.05f, 1.8f, 1.55f);
		FString Asked;
		TArray<FString> Parts;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterWarmWoods="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 5)
		{
			Less = FCString::Atof(*Parts[0]);
			More = FCString::Atof(*Parts[1]);
			Olive = FLinearColor(FCString::Atof(*Parts[2]), FCString::Atof(*Parts[3]), FCString::Atof(*Parts[4]));
		}
		LandScalar(TEXT("WoodsWarmLess"), -Less);
		LandScalar(TEXT("WoodsWarmMore"), More);
		LandVector(TEXT("WoodsWarmTint"), Olive);
	}
	{
		// "-TheaterFields=0.55,0.4,0.34" tries other strengths for the quilt of fields (close to, at working height) and its hedges.
		FString Asked;
		TArray<FString> Parts;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFields="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 3)
		{
			FieldsNear = FCString::Atof(*Parts[0]);
			FieldsWorking = FCString::Atof(*Parts[1]);
			HedgesNear = FCString::Atof(*Parts[2]);
		}
	}
	LandMaterial->SetTextureParameterValue(TEXT("Detail"), DetailPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Bumps"), BumpsPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Waves"), WavesPicture); // (its painted lakes and rivers move with the sea)
	LandMaterial->SetTextureParameterValue(TEXT("Haze"), HazePicture); // (the wisps of the fog of war's haze)
	{
		// "-TheaterSheen=0.1" tries another strength for the sheen of the painted land.
		float Sheen = 0.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSheen="), Sheen))
		{
			LandScalar(TEXT("Specular"), Sheen);
		}
	}
	LandVector(TEXT("RockColour"), FCampaignGround::RockColour());
	LandVector(TEXT("SnowColour"), FCampaignGround::SnowColour() * 0.86f); // (the baked sun lifts it to white where it is lit)
	LandVector(TEXT("SandColour"), FCampaignGround::SandColour());
	{
		// The staff map at the top of the zoom (art fix 11): parchment, sepia shading, a hand's tint.
		// ("-TheaterPaperLand=r,g,b,shade,sun,wash,band", "-TheaterPaperSea=r,g,b" and "-TheaterPaperNight=land,sea,wash"
		// try others without a rebuild)
		FString Asked;
		TArray<FString> Bits;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterPaperLand="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(",")) > 0)
		{
			for (int32 K = 0; K < Bits.Num() && K < 7; ++K) { PaperLand[K] = FCString::Atof(*Bits[K]); }
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterPaperSea="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(",")) > 0)
		{
			for (int32 K = 0; K < Bits.Num() && K < 3; ++K) { PaperSeaTone[K] = FCString::Atof(*Bits[K]); }
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterPaperNight="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(",")) > 0)
		{
			for (int32 K = 0; K < Bits.Num() && K < 3; ++K) { PaperNight[K] = FCString::Atof(*Bits[K]); }
		}
		ApplyPaperLight();
	}
	SeaMaterial->SetTextureParameterValue(TEXT("Water"), WaterPicture);
	SeaMaterial->SetTextureParameterValue(TEXT("Shore"), ShorePicture);
	SeaMaterial->SetTextureParameterValue(TEXT("Ripples"), RipplesPicture);
	SeaMaterial->SetTextureParameterValue(TEXT("Waves"), WavesPicture);

	UE_LOG(LogTheater, Log, TEXT("Ground: %d x %d pixels of paint and three detail pictures made in %.2f s"), Paint.NX, Paint.NY, FPlatformTime::Seconds() - StartTime);
}

void ACampaignMapActor::PrepareLand()
{
	TerrainMesh->ClearAllMeshSections();
	for (UProceduralMeshComponent* Mesh : LandMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	LandMeshes.Reset();
	SpareLandMeshes.Reset();
	HomeLand = FLandSheet();
	HomeLand.Shape = &Terrain;
	HomeLand.Material = LandMaterial;
	WorldLand = FLandSheet();
	LevelLand(HomeLand);
	UE_LOG(LogTheater, Log, TEXT("Land: %d levels of tiles, the finest %.0f map units across and the coarsest %.0f"),
		HomeLand.TopLevel + 1, TileQuads * Terrain.Cell, TileQuads * Terrain.Cell * (1 << HomeLand.TopLevel));

	// Stand the table's first tiles up at once, for the view the game opens on.
	UpdateLand(HomeLand, 12000.0f, FVector::ZeroVector, true);
}

void ACampaignMapActor::LevelLand(FLandSheet& Sheet)
{
	const FCampaignTerrain& Shape = *Sheet.Shape;
	// The heights at every level. Each coarser level keeps every second point of the one below, each the
	// average of itself and its neighbours there, so that far-off ground is smooth and not a jagged sample.
	// Alongside, the most land near each point, so that a tile that is all open sea can be left out.
	Sheet.Tiles.Reset();
	Sheet.Heights.Reset();
	Sheet.Cover.Reset();
	Sheet.Sizes.Reset();
	Sheet.Heights.Add(Shape.Height);
	Sheet.Cover.Add(Shape.Coverage);
	Sheet.Sizes.Add(FIntPoint(Shape.NX, Shape.NY));
	const double Longest = FMath::Max(Shape.ExtentX, Shape.ExtentY);
	Sheet.TopLevel = 0;
	while (TileQuads * Shape.Cell * (1 << Sheet.TopLevel) < Longest * 0.5 && Sheet.TopLevel < 12)
	{
		++Sheet.TopLevel;
	}
	for (int32 Level = 1; Level <= Sheet.TopLevel; ++Level)
	{
		const FIntPoint Fine = Sheet.Sizes[Level - 1];
		const FIntPoint Coarse((Fine.X - 1) / 2 + 1, (Fine.Y - 1) / 2 + 1);
		const TArray<float>& FineHeights = Sheet.Heights[Level - 1];
		const TArray<float>& FineCover = Sheet.Cover[Level - 1];
		TArray<float> Heights, Cover;
		Heights.SetNumUninitialized(Coarse.X * Coarse.Y);
		Cover.SetNumUninitialized(Coarse.X * Coarse.Y);
		for (int32 J = 0; J < Coarse.Y; ++J)
		{
			for (int32 I = 0; I < Coarse.X; ++I)
			{
				float Sum = 0.0f, Weight = 0.0f, Most = 0.0f;
				for (int32 DJ = -1; DJ <= 1; ++DJ)
				{
					for (int32 DI = -1; DI <= 1; ++DI)
					{
						const int32 FI = FMath::Clamp(I * 2 + DI, 0, Fine.X - 1), FJ = FMath::Clamp(J * 2 + DJ, 0, Fine.Y - 1);
						const float W = (DI == 0 ? 2.0f : 1.0f) * (DJ == 0 ? 2.0f : 1.0f);
						Sum += FineHeights[FJ * Fine.X + FI] * W;
						Weight += W;
						Most = FMath::Max(Most, FineCover[FJ * Fine.X + FI]);
					}
				}
				Heights[J * Coarse.X + I] = Sum / Weight;
				Cover[J * Coarse.X + I] = Most;
			}
		}
		Sheet.Heights.Add(MoveTemp(Heights));
		Sheet.Cover.Add(MoveTemp(Cover));
		Sheet.Sizes.Add(Coarse);
	}
}

bool ACampaignMapActor::LandTileHasLand(const FLandSheet& Sheet, int32 Level, int32 TileX, int32 TileY) const
{
	const FIntPoint Size = Sheet.Sizes[Level];
	const TArray<float>& Cover = Sheet.Cover[Level];
	const int32 I0 = TileX * TileQuads, J0 = TileY * TileQuads;
	if (I0 >= Size.X - 1 || J0 >= Size.Y - 1)
	{
		return false; // off the edge of the map
	}
	const int32 I1 = FMath::Min(I0 + TileQuads, Size.X - 1), J1 = FMath::Min(J0 + TileQuads, Size.Y - 1);
	if (Sheet.Holes.Num() > 0 || Sheet.bKeep)
	{
		// A tile that lies wholly on ground a finer sheet draws is that sheet's business; and a sheet built with an
		// apron round it has no tiles on the apron.
		const double Span = (1 << Level) * Sheet.Shape->Cell;
		const FBox2D Box(FVector2D(Sheet.Shape->OriginX + I0 * Span, Sheet.Shape->OriginY + J0 * Span), FVector2D(Sheet.Shape->OriginX + I1 * Span, Sheet.Shape->OriginY + J1 * Span));
		for (const FBox2D& Hole : Sheet.Holes)
		{
			if (Box.Min.X >= Hole.Min.X && Box.Min.Y >= Hole.Min.Y && Box.Max.X <= Hole.Max.X && Box.Max.Y <= Hole.Max.Y)
			{
				return false;
			}
		}
		if (Sheet.bKeep && (Box.Max.X <= Sheet.Keep.Min.X || Box.Max.Y <= Sheet.Keep.Min.Y || Box.Min.X >= Sheet.Keep.Max.X || Box.Min.Y >= Sheet.Keep.Max.Y))
		{
			return false;
		}
	}
	for (int32 J = J0; J <= J1; ++J)
	{
		for (int32 I = I0; I <= I1; ++I)
		{
			if (Cover[J * Size.X + I] >= 0.3f)
			{
				return true;
			}
		}
	}
	return false;
}

UProceduralMeshComponent* ACampaignMapActor::BuildLandTile(FLandSheet& Sheet, int32 Level, int32 TileX, int32 TileY)
{
	THEATER_SPEED_SCOPE(BuildLandTile);
	const FCampaignTerrain& Shape = *Sheet.Shape;
	const FIntPoint Size = Sheet.Sizes[Level];
	const TArray<float>& Heights = Sheet.Heights[Level];
	const TArray<float>& Cover = Sheet.Cover[Level];
	const int32 Step = 1 << Level; // how many of the finest grid's points one quad of this tile spans
	const int32 I0 = TileX * TileQuads, J0 = TileY * TileQuads;
	const int32 I1 = FMath::Min(I0 + TileQuads, Size.X - 1), J1 = FMath::Min(J0 + TileQuads, Size.Y - 1);
	const int32 Stride = I1 - I0 + 1;

	FCampaignMeshBuffers Buffers;
	TArray<int32> Remap;
	Remap.Init(INDEX_NONE, Stride * (J1 - J0 + 1));
	auto PointAt = [&](int32 I, int32 J, double Drop)
	{
		const double X = Shape.OriginX + FMath::Min(I * Step * Shape.Cell, Shape.ExtentX), Y = Shape.OriginY + FMath::Min(J * Step * Shape.Cell, Shape.ExtentY);
		// The paint has one pixel for every point of the finest grid: this point's is at the middle of its pixel.
		const FVector2D UV((FMath::Min(I * Step, Shape.NX - 1) + 0.5) / Shape.NX, (FMath::Min(J * Step, Shape.NY - 1) + 0.5) / Shape.NY);
		// (a sheet with a finer one let into it lies a hair low round that one, and only there, so that where the two
		// overlap the finer is always the one seen; elsewhere its land stands at its true height above the sea)
		const bool bByTheHole = Sheet.Holes.Num() > 0 && Sheet.InHole(X, Y, -3.0 * Step * Shape.Cell);
		// (And the strip of it that runs on under the finer sheet's edge falls away as it goes in: a third of a unit
		// for every unit. Lying a hair low the whole way, it came up through the finer ground wherever that dipped
		// below it: in a hollow on the Kazakh steppe, near where two fine sheets meet, two pale hard-edged chips of
		// the coarse sheet showed, which the art director found in rounds 27 and 28 and I twice misnamed.)
		const double Under = bByTheHole && Sheet.Sink > 0.0 ? 0.35 * Sheet.DeepInHole(X, Y) : 0.0;
		return Buffers.AddVertex(MapToWorld(X, Y, Heights[J * Size.X + I] - Drop - (bByTheHole ? Sheet.Sink : 0.0) - Under), FVector::UpVector, FColor::White, UV);
	};
	auto VertexAt = [&](int32 I, int32 J)
	{
		int32& Slot = Remap[(J - J0) * Stride + (I - I0)];
		if (Slot == INDEX_NONE)
		{
			Slot = PointAt(I, J, 0.0);
		}
		return Slot;
	};
	auto HasQuad = [&](int32 I, int32 J)
	{
		if (I < I0 || J < J0 || I >= I1 || J >= J1)
		{
			return false;
		}
		if (Sheet.Holes.Num() > 0 || Sheet.bKeep)
		{
			// (ground a finer sheet draws; this one runs a little way under its edge, so that no gap shows between them)
			const double Span = Step * Shape.Cell;
			const double X = Shape.OriginX + (I + 0.5) * Span, Y = Shape.OriginY + (J + 0.5) * Span;
			if (Sheet.InHole(X, Y, Span))
			{
				return false;
			}
			// (and a sheet with an apron draws nothing on it)
			if (Sheet.bKeep && (X < Sheet.Keep.Min.X || Y < Sheet.Keep.Min.Y || X > Sheet.Keep.Max.X || Y > Sheet.Keep.Max.Y))
			{
				return false;
			}
		}
		// Sea bed well away from any shore is never seen: leave it out.
		return FMath::Max(FMath::Max(Cover[J * Size.X + I], Cover[J * Size.X + I + 1]), FMath::Max(Cover[(J + 1) * Size.X + I], Cover[(J + 1) * Size.X + I + 1])) >= 0.3f;
	};
	// A skirt hangs from the tile's edge, so that where a fine tile meets a coarse one no daylight shows between them.
	const double SkirtDrop = 0.9 * Step * Shape.Cell;
	auto Skirt = [&](int32 IA, int32 JA, int32 IB, int32 JB)
	{
		const int32 A = VertexAt(IA, JA), B = VertexAt(IB, JB);
		const int32 LowA = PointAt(IA, JA, SkirtDrop), LowB = PointAt(IB, JB, SkirtDrop);
		Buffers.Triangles.Append({ A, LowA, B, B, LowA, LowB, A, B, LowA, B, LowB, LowA }); // both ways round: it is seen from either side
	};
	for (int32 J = J0; J < J1; ++J)
	{
		for (int32 I = I0; I < I1; ++I)
		{
			if (!HasQuad(I, J))
			{
				continue;
			}
			const int32 A = VertexAt(I, J), B = VertexAt(I + 1, J), C = VertexAt(I, J + 1), D = VertexAt(I + 1, J + 1);
			Buffers.Triangles.Append({ A, C, B, B, C, D });
			if (J == J0) Skirt(I, J, I + 1, J);
			if (J == J1 - 1) Skirt(I, J + 1, I + 1, J + 1);
			if (I == I0) Skirt(I, J, I, J + 1);
			if (I == I1 - 1) Skirt(I + 1, J, I + 1, J + 1);
		}
	}
	if (Buffers.Triangles.Num() == 0)
	{
		return nullptr;
	}

	UProceduralMeshComponent* Mesh = SpareLandMeshes.Num() > 0 ? SpareLandMeshes.Pop() : nullptr;
	if (!Mesh)
	{
		Mesh = UTheaterStillMesh::Make(this, TEXT("LandTile"));
		Mesh->SetupAttachment(Root);
		Mesh->bUseAsyncCooking = false;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(bLandShadows);
		Mesh->RegisterComponent();
		LandMeshes.Add(Mesh);
	}
	Buffers.Commit(Mesh, 0, Sheet.Material);
	Mesh->SetRelativeLocation(FVector(0.0, Sheet.WrapMode == 2 ? Sheet.Turn * Data.WorldWidth * UnitScale : 0.0, 0.0));
	Mesh->SetVisibility(!Sheet.bHidden);
	TheaterSpeed::NoteMade(TheaterSpeed::EMade::Tiles);
	return Mesh;
}

bool ACampaignMapActor::UpdateLand(FLandSheet& Sheet, float CameraDistance, const FVector& CameraFocus, bool bAllAtOnce)
{
	THEATER_SPEED_SCOPE(UpdateLand);
	if (Sheet.Heights.Num() == 0 || !Sheet.Shape)
	{
		return false;
	}
	const FCampaignTerrain& Shape = *Sheet.Shape;
	// Which tiles the view wants: starting from the coarsest, a tile is cut into its four quarters for as long
	// as it is large for how far off it is. "How far off" is the eye's distance from what it looks at, and the
	// tile's distance from that point along the table.
	const double TileUnits = TileQuads * Shape.Cell * UnitScale; // the side of a finest tile, in the world
	const FVector2D Focus(CameraFocus.X, CameraFocus.Y);
	const double EyeX = WorldToMap(CameraFocus).X, WorldSpan = Data.WorldWidth * UnitScale;
	TMap<uint64, int32> Turns; // how many turns of the world east each wanted tile is drawn
	TArray<uint64> Wanted;
	TArray<FIntVector> Stack;
	const int32 TopSide = 1 << Sheet.TopLevel;
	const int32 RootsX = FMath::DivideAndRoundUp(FMath::DivideAndRoundUp(Shape.NX - 1, TileQuads), TopSide);
	const int32 RootsY = FMath::DivideAndRoundUp(FMath::DivideAndRoundUp(Shape.NY - 1, TileQuads), TopSide);
	for (int32 Y = 0; Y < RootsY; ++Y)
	{
		for (int32 X = 0; X < RootsX; ++X)
		{
			Stack.Add(FIntVector(X, Y, Sheet.TopLevel));
		}
	}
	while (Stack.Num() > 0)
	{
		const FIntVector Tile = Stack.Pop();
		const int32 Level = Tile.Z;
		if (!LandTileHasLand(Sheet, Level, Tile.X, Tile.Y))
		{
			continue;
		}
		const double Side = TileUnits * (1 << Level);
		// The tile's box on the table, in the world: north is +X and east is +Y, and the sheet's corner is its north-west.
		// (in a world that runs right round, the tile stands a turn of the world east or west if that is nearer the eye)
		const int32 Turn = Sheet.WrapMode == 1 ? TurnFor(Shape.OriginX + (Tile.X + 0.5) * Side / UnitScale, EyeX) : Sheet.WrapMode == 2 ? Sheet.Turn : 0;
		const FVector NorthWest = MapToWorld(Shape.OriginX + Tile.X * Side / UnitScale, Shape.OriginY + Tile.Y * Side / UnitScale) + FVector(0.0, Turn * WorldSpan, 0.0);
		const FBox2D Box(FVector2D(NorthWest.X - Side, NorthWest.Y), FVector2D(NorthWest.X, NorthWest.Y + Side));
		const double Away = FMath::Sqrt(Box.ComputeSquaredDistanceToPoint(Focus));
		const double Distance = CameraDistance * 0.6 + Away;
		if (Level > 0 && Side > 0.28 * Distance)
		{
			for (int32 Quarter = 0; Quarter < 4; ++Quarter)
			{
				Stack.Add(FIntVector(Tile.X * 2 + (Quarter & 1), Tile.Y * 2 + (Quarter >> 1), Level - 1));
			}
		}
		else
		{
			Wanted.Add(LandKey(Level, Tile.X, Tile.Y));
			if (Sheet.WrapMode != 0)
			{
				Turns.Add(LandKey(Level, Tile.X, Tile.Y), Turn);
			}
		}
	}

	// Stand up the tiles that are wanted and not there yet: all of them when asked, otherwise a few each frame,
	// so that sweeping across the table never stalls.
	int32 Budget = bAllAtOnce ? MAX_int32 : 5;
	// (Speed, 18 November 2026: tc.Speed.TileTime. "A few" was five for every sheet, whatever they took: the world's
	// sheet, the map's own and each fine sheet that stands or is standing up, so thirty or more in a frame of a zoom,
	// and 30 ms of one frame in the builder's sweep of build 1634. Now each sheet's first tile of the frame is always
	// stood up, so every sheet moves on every frame, and its others only while all the frame's tiles together have
	// taken less than the switch's ms. The rest come next frame; what they replace stays until they do, as ever.)
	const double TileLimit = bAllAtOnce ? 0.0 : TheaterSpeed::TileTime() * 0.001;
	if (TileTimeFrame != GFrameCounter)
	{
		TileTimeFrame = GFrameCounter;
		TileTimeSpent = 0.0;
	}
	bool bFirstTile = true;
	TSet<uint64> Missing;
	for (const uint64 Key : Wanted)
	{
		if (Sheet.Tiles.Contains(Key))
		{
			continue;
		}
		if (Budget > 0 && TileLimit > 0.0 && !bFirstTile && TileTimeSpent >= TileLimit)
		{
			Budget = 0; // (the frame's time for tiles is spent)
		}
		if (Budget > 0)
		{
			--Budget;
			bFirstTile = false;
			const double TileBegan = FPlatformTime::Seconds();
			const int32 Level = static_cast<int32>(Key >> 48), TileX = static_cast<int32>((Key >> 24) & 0xFFFFFF), TileY = static_cast<int32>(Key & 0xFFFFFF);
			Sheet.Tiles.Add(Key, BuildLandTile(Sheet, Level, TileX, TileY)); // null for a tile with nothing in it: remembered, so it is not tried again
			TileTimeSpent += FPlatformTime::Seconds() - TileBegan;
		}
		else
		{
			Missing.Add(Key);
		}
	}

	if (Sheet.WrapMode != 0)
	{
		// Each wanted tile is set where it is nearest the eye.
		for (const auto& Pair : Turns)
		{
			UProceduralMeshComponent* const* Standing = Sheet.Tiles.Find(Pair.Key);
			if (Standing && *Standing)
			{
				const FVector Place(0.0, Pair.Value * WorldSpan, 0.0);
				if (!(*Standing)->GetRelativeLocation().Equals(Place, 0.5))
				{
					(*Standing)->SetRelativeLocation(Place);
				}
			}
		}
	}

	// Take away the tiles that are no longer wanted, but only once whatever replaces them is standing: a tile
	// stays for as long as any wanted tile over the same ground (finer or coarser) is still to come.
	const TSet<uint64> WantedSet(Wanted);
	for (auto It = Sheet.Tiles.CreateIterator(); It; ++It)
	{
		if (WantedSet.Contains(It.Key()))
		{
			continue;
		}
		const int32 Level = static_cast<int32>(It.Key() >> 48), TileX = static_cast<int32>((It.Key() >> 24) & 0xFFFFFF), TileY = static_cast<int32>(It.Key() & 0xFFFFFF);
		bool bStillNeeded = false;
		for (const uint64 Other : Missing)
		{
			const int32 OtherLevel = static_cast<int32>(Other >> 48), OtherX = static_cast<int32>((Other >> 24) & 0xFFFFFF), OtherY = static_cast<int32>(Other & 0xFFFFFF);
			const int32 Shift = FMath::Abs(Level - OtherLevel);
			const bool bOver = Level >= OtherLevel
				? (OtherX >> Shift) == TileX && (OtherY >> Shift) == TileY
				: (TileX >> Shift) == OtherX && (TileY >> Shift) == OtherY;
			if (bOver)
			{
				bStillNeeded = true;
				break;
			}
		}
		if (!bStillNeeded)
		{
			if (UProceduralMeshComponent* Mesh = It.Value())
			{
				Mesh->SetVisibility(false);
				Mesh->ClearAllMeshSections();
				SpareLandMeshes.Add(Mesh);
			}
			It.RemoveCurrent();
		}
	}
	return Missing.Num() > 0;
}

namespace
{
	/**
	 * A picture or a field that runs right round the world is made to meet itself at the seam: along each row the
	 * two ends are brought to the value half way between them, and the correction dies away over Reach points in
	 * from each end. What is laid by noise (the swell, the grit, the tooth of the slopes) does not otherwise come
	 * round to where it started, and would show as a line down the middle of the Atlantic.
	 */
	void MeetAtSeam(TArray<FColor>& Pixels, int32 Wide, int32 High, int32 Reach, bool bAlphaToo)
	{
		if (Wide < Reach * 2 + 2 || Pixels.Num() < Wide * High)
		{
			return;
		}
		for (int32 J = 0; J < High; ++J)
		{
			FColor* Row = Pixels.GetData() + J * Wide;
			const FColor West = Row[0], East = Row[Wide - 1];
			const float Half[4] = { (East.R - West.R) * 0.5f, (East.G - West.G) * 0.5f, (East.B - West.B) * 0.5f, (East.A - West.A) * 0.5f };
			for (int32 K = 0; K < Reach; ++K)
			{
				const float Much = FMath::Square(1.0f - static_cast<float>(K) / Reach);
				auto Shift = [Much](uint8 Was, float By) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Was + By * Much), 0, 255)); };
				FColor& A = Row[K];
				FColor& B = Row[Wide - 1 - K];
				A = FColor(Shift(A.R, Half[0]), Shift(A.G, Half[1]), Shift(A.B, Half[2]), bAlphaToo ? Shift(A.A, Half[3]) : A.A);
				B = FColor(Shift(B.R, -Half[0]), Shift(B.G, -Half[1]), Shift(B.B, -Half[2]), bAlphaToo ? Shift(B.A, -Half[3]) : B.A);
			}
		}
	}
}

void ACampaignMapActor::BuildWorld()
{
	if (FogAsideBuilding.IsValid())
	{
		FogAsideBuilding.Wait(); // (speed, tc.Speed.FogAside: it reads the world's sheet, which is made again here)
	}
	FogAside.Reset();
	FogAsideBuilding = TFuture<void>();
	FogLaying = INDEX_NONE;
	if (!Data.bWorld)
	{
		return;
	}
	const double StartTime = FPlatformTime::Seconds();
	{
		// The fine patterns that are laid by where a point is on the table (grass, crowns, fields, hatching, ripples)
		// repeat a whole number of times round the world, so that they are the same pattern when the eye comes
		// round: each tiling is taken to the nearest that does. (A change of less than a thousandth.)
		const double Round = Data.WorldWidth * UnitScale;
		auto Whole = [Round](double Tiling) { return static_cast<float>(FMath::RoundToDouble(Tiling * Round) / Round); };
		LandScalar(TEXT("DetailTiling"), Whole(0.0052));
		LandScalar(TEXT("DetailTilingCoarse"), Whole(0.00137));
		LandScalar(TEXT("FieldTiling"), Whole(0.03));
		LandScalar(TEXT("HatchTiling"), Whole(0.006));
		LandScalar(TEXT("StripeTiling"), Whole(0.0158)); // (the stripes of occupied ground: 0.38 of the ruling's spacing)
		SeaScalar(TEXT("RippleTiling"), Whole(0.0095));
		SeaScalar(TEXT("RippleTilingSecond"), Whole(0.0061));
	}
	// The land beyond today's map, on one coarse sheet that covers the whole world (the whole-world map, step one).
	WorldTerrain.BuildOver(Data, Data.WorldMinX, Data.WorldMinY, Data.WorldWidth, Data.WorldHeight, 6.0, true);
	WorldPaint.Build(Data, WorldTerrain, VerticalScale);
	// (the world runs right round: its pictures are made to meet themselves at the seam)
	MeetAtSeam(WorldPaint.Ground, WorldPaint.NX, WorldPaint.NY, 16, true);
	MeetAtSeam(WorldPaint.Tint, WorldPaint.NX, WorldPaint.NY, 16, true);
	MeetAtSeam(WorldPaint.Relief, WorldPaint.NX, WorldPaint.NY, 16, true);
	MeetAtSeam(WorldPaint.Masks, WorldPaint.NX, WorldPaint.NY, 16, true);
	MeetAtSeam(WorldPaint.Cover, WorldPaint.NX, WorldPaint.NY, 16, true);
	MeetAtSeam(WorldPaint.Water, WorldPaint.WaterNX, WorldPaint.WaterNY, 16, true);
	MeetAtSeam(WorldPaint.Surf, WorldPaint.WaterNX, WorldPaint.WaterNY, 16, true);
	if (Paint.WaterNX > 1 && Paint.WaterNY > 1)
	{
		// Along the map's own box the world's sea takes the colour of the map's sea at that edge, dying away over
		// some forty units, so that no line shows in the water where the map's sheet ends.
		const double Reach = 42.0;
		ParallelFor(WorldPaint.WaterNY, [&](int32 J)
		{
			for (int32 I = 0; I < WorldPaint.WaterNX; ++I)
			{
				const double X = WorldTerrain.OriginX + I * WorldPaint.WaterStep, Y = WorldTerrain.OriginY + J * WorldPaint.WaterStep;
				const double EdgeX = FMath::Clamp(X, 0.0, Data.Width), EdgeY = FMath::Clamp(Y, 0.0, Data.Height);
				const double Away = FVector2D::Distance(FVector2D(X, Y), FVector2D(EdgeX, EdgeY));
				if (Away <= 0.0 || Away >= Reach)
				{
					continue; // inside the box (the map's own sea lies there) or too far off to matter
				}
				const int32 EI = FMath::Clamp(FMath::RoundToInt32(EdgeX / Paint.WaterStep), 0, Paint.WaterNX - 1), EJ = FMath::Clamp(FMath::RoundToInt32(EdgeY / Paint.WaterStep), 0, Paint.WaterNY - 1);
				const FColor& Edge = Paint.Water[EJ * Paint.WaterNX + EI];
				FColor& Here = WorldPaint.Water[J * WorldPaint.WaterNX + I];
				const float Much = 1.0f - FMath::SmoothStep(0.0f, static_cast<float>(Reach), static_cast<float>(Away));
				Here = FColor(static_cast<uint8>(FMath::Lerp(static_cast<float>(Here.R), static_cast<float>(Edge.R), Much)),
					static_cast<uint8>(FMath::Lerp(static_cast<float>(Here.G), static_cast<float>(Edge.G), Much)),
					static_cast<uint8>(FMath::Lerp(static_cast<float>(Here.B), static_cast<float>(Edge.B), Much)), Here.A);
			}
		});
	}
	WorldPictures.Reset();
	auto Picture = [this](const TArray<FColor>& Pixels, int32 Wide, int32 High, bool bColour, const TCHAR* Name, bool bSharp = false)
	{
		UTexture2D* Made = CampaignTextures::Create(Wide, High, Pixels, bColour, false, Name, bSharp);
		WorldPictures.Add(Made);
		return Made;
	};
	const bool bPictureOnce = TheaterSpeed::PictureOnce(); // (speed: see PaintGround)
	UTexture2D* Lie = Picture(WorldPaint.Lie, WorldPaint.NX, WorldPaint.NY, false, TEXT("WorldLie"), bPictureOnce);
	if (Lie && !bPictureOnce)
	{
		Lie->Filter = TF_Nearest;
		Lie->UpdateResource();
	}
	TArray<FColor> Clear;
	Clear.Init(FColor(0, 0, 0, 255), 16);

	// The same paint as the map's own, with this sheet's pictures in place of the map's.
	WorldLandMaterial = UMaterialInstanceDynamic::Create(LandMaterial->Parent, this);
	WorldLandMaterial->CopyParameterOverrides(LandMaterial);
	DressBeyond(WorldLandMaterial);
	WorldLandMaterial->SetVectorParameterValue(TEXT("ReliefJitter"), FLinearColor(1.0f / FMath::Max(WorldPaint.NX, 1), 1.0f / FMath::Max(WorldPaint.NY, 1), 3.0f * WorldPaint.NX, 3.0f * WorldPaint.NY));
	WorldLandMaterial->SetTextureParameterValue(TEXT("Ground"), Picture(WorldPaint.Ground, WorldPaint.NX, WorldPaint.NY, true, TEXT("WorldGround")));
	WorldTintPicture = Picture(WorldPaint.Tint, WorldPaint.NX, WorldPaint.NY, true, TEXT("WorldTint"));
	WorldReliefPicture = Picture(WorldPaint.Relief, WorldPaint.NX, WorldPaint.NY, false, TEXT("WorldRelief"));
	WorldFogPicture = nullptr; // (made when there is first something to hide)
	WorldOccupiedPicture = WorldPaint.Occupied.Num() == WorldPaint.NX * WorldPaint.NY ? Picture(WorldPaint.Occupied, WorldPaint.NX, WorldPaint.NY, true, TEXT("WorldOccupied")) : nullptr;
	WorldLandMaterial->SetTextureParameterValue(TEXT("Tint"), WorldTintPicture);
	WorldLandMaterial->SetTextureParameterValue(TEXT("Relief"), WorldReliefPicture);
	WorldLandMaterial->SetTextureParameterValue(TEXT("Masks"), Picture(WorldPaint.Masks, WorldPaint.NX, WorldPaint.NY, false, TEXT("WorldMasks")));
	WorldLandMaterial->SetTextureParameterValue(TEXT("Cover"), Picture(WorldPaint.Cover, WorldPaint.NX, WorldPaint.NY, false, TEXT("WorldCover")));
	WorldLandMaterial->SetTextureParameterValue(TEXT("Lie"), Lie);
	WorldLandMaterial->SetTextureParameterValue(TEXT("Fog"), Picture(Clear, 4, 4, false, TEXT("WorldFog"))); // (nothing beyond the map is hidden: nobody is there yet)
	WorldLandMaterial->SetTextureParameterValue(TEXT("Occupied"), WorldOccupiedPicture ? WorldOccupiedPicture.Get() : NoStripesPicture.Get()); // (occupied ground beyond Europe keeps its old owner's stripes: from this height, their flat average)
	if (WorldPaint.Wet.Num() == WorldPaint.NX * WorldPaint.NY && WorldPaint.Wet.Num() > 0)
	{
		// (the rivers of the world beyond Europe, painted into the land: see CampaignGround.h)
		WorldLandMaterial->SetTextureParameterValue(TEXT("Wet"), Picture(WorldPaint.Wet, WorldPaint.NX, WorldPaint.NY, false, TEXT("WorldWet")));
		WorldLandMaterial->SetTextureParameterValue(TEXT("WetInk"), Picture(WorldPaint.WetInk, WorldPaint.NX, WorldPaint.NY, true, TEXT("WorldWetInk")));
		WorldLandMaterial->SetScalarParameterValue(TEXT("WetReach"), WorldPaint.WetReach);
		WorldLandMaterial->SetScalarParameterValue(TEXT("WetCell"), static_cast<float>(WorldTerrain.Cell));
	}
	WorldSeaMaterial = UMaterialInstanceDynamic::Create(SeaMaterial->Parent, this);
	WorldSeaMaterial->CopyParameterOverrides(SeaMaterial);
	WorldSeaMaterial->SetTextureParameterValue(TEXT("Water"), Picture(WorldPaint.Water, WorldPaint.WaterNX, WorldPaint.WaterNY, true, TEXT("WorldWater")));
	WorldSeaMaterial->SetTextureParameterValue(TEXT("Shore"), Picture(WorldPaint.Surf, WorldPaint.WaterNX, WorldPaint.WaterNY, false, TEXT("WorldShore")));
	// (The pale line at the water's edge belongs to the fine sheets, at working height. The coarse sheet's sea has
	// none: from high up it would stand as a wire outline, and round islands too small for the coarse land to draw.)
	WorldSeaMaterial->SetScalarParameterValue(TEXT("Surf"), 0.0f);

	// Its land, in tiles like the map's own, with the map's own ground left out of it and the whole of it a
	// hair lower, so that where the two meet the finer one is always the one seen.
	WorldLand = FLandSheet();
	WorldLand.Shape = &WorldTerrain;
	WorldLand.Material = WorldLandMaterial;
	WorldLand.Holes.Add(FBox2D(FVector2D(0.0, 0.0), FVector2D(Data.Width, Data.Height)));
	WorldLand.Sink = 0.06;
	WorldLand.WrapMode = 1;
	// The lines the fine sheets are ruled by: the map's own box is two of their rows high and one column wide, and
	// the rest of the world is ruled from its edges in squares of 768 map units.
	FineXs.Reset();
	FineYs.Reset();
	const double FineSide = 768.0;
	for (double X = 0.0; X > Data.WorldMinX; X -= FineSide) { FineXs.Add(X); }
	FineXs.Add(Data.WorldMinX);
	for (double X = Data.Width; X < Data.WorldMinX + Data.WorldWidth; X += FineSide) { FineXs.Add(X); }
	FineXs.Add(Data.WorldMinX + Data.WorldWidth);
	FineYs.Append({ Data.WorldMinY, Data.Height * 0.5 });
	for (double Y = Data.Height; Y < Data.WorldMinY + Data.WorldHeight; Y += FineSide) { FineYs.Add(Y); }
	FineYs.Add(Data.WorldMinY + Data.WorldHeight);
	FineXs.Sort();
	FineYs.Sort();
	LevelLand(WorldLand);
	UpdateLand(WorldLand, 12000.0f, FVector::ZeroVector, true);

	// Its sea: one flat sheet at the level of the map's own, with the map's own box left out of it.
	if (!WorldSeaMesh)
	{
		WorldSeaMesh = UTheaterStillMesh::Make(this, TEXT("WorldSea"));
		WorldSeaMesh->SetupAttachment(Root);
		WorldSeaMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WorldSeaMesh->SetCastShadow(false);
		WorldSeaMesh->RegisterComponent();
	}
	WorldSeaMesh->ClearAllMeshSections();
	{
		// (its lines run along the edges of the map's box, so that the box can be left out cleanly)
		TArray<double> Xs, Ys;
		for (int32 I = 0; I <= 36; ++I) { Xs.Add(Data.WorldMinX + Data.WorldWidth * I / 36.0); }
		for (int32 J = 0; J <= 20; ++J) { Ys.Add(Data.WorldMinY + Data.WorldHeight * J / 20.0); }
		Xs.Append({ 0.0, Data.Width });
		Ys.Append({ 0.0, Data.Height });
		Xs.Sort();
		Ys.Sort();
		const int32 Across = Xs.Num() - 1, Down = Ys.Num() - 1;
		FCampaignMeshBuffers Buffers;
		for (int32 J = 0; J <= Down; ++J)
		{
			for (int32 I = 0; I <= Across; ++I)
			{
				const double X = Xs[I] - Data.WorldMinX, Y = Ys[J] - Data.WorldMinY;
				const FVector2D UV((X / WorldPaint.WaterStep + 0.5) / WorldPaint.WaterNX, (Y / WorldPaint.WaterStep + 0.5) / WorldPaint.WaterNY);
				Buffers.AddVertex(MapToWorld(Xs[I], Ys[J], 0.0), FVector::UpVector, FColor::White, UV);
			}
		}
		for (int32 J = 0; J < Down; ++J)
		{
			for (int32 I = 0; I < Across; ++I)
			{
				const double MidX = (Xs[I] + Xs[I + 1]) * 0.5, MidY = (Ys[J] + Ys[J + 1]) * 0.5;
				if (MidX > 0.0 && MidX < Data.Width && MidY > 0.0 && MidY < Data.Height)
				{
					continue; // the map's own sea lies here
				}
				const int32 A = J * (Across + 1) + I, B = A + 1, C = A + Across + 1, D = C + 1;
				Buffers.Triangles.Append({ A, C, B, B, C, D });
			}
		}
		Buffers.Commit(WorldSeaMesh, 0, WorldSeaMaterial);
		// (the same sea again, to stand a turn of the world to whichever side the eye is on)
		if (!WorldSeaTwin)
		{
			WorldSeaTwin = UTheaterStillMesh::Make(this, TEXT("WorldSeaTwin"));
			WorldSeaTwin->SetupAttachment(Root);
			WorldSeaTwin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WorldSeaTwin->SetCastShadow(false);
			WorldSeaTwin->RegisterComponent();
		}
		WorldSeaTwin->ClearAllMeshSections();
		Buffers.Commit(WorldSeaTwin, 0, WorldSeaMaterial);
		WorldSeaTwin->SetRelativeLocation(FVector(0.0, -Data.WorldWidth * UnitScale, 0.0));
	}
	UE_LOG(LogTheater, Log, TEXT("World: %.0f x %.0f map units round the map, on a sheet of %d x %d points (%.0f units a cell), %d levels of tiles, built in %.2f s"),
		Data.WorldWidth, Data.WorldHeight, WorldTerrain.NX, WorldTerrain.NY, WorldTerrain.Cell, WorldLand.TopLevel + 1, FPlatformTime::Seconds() - StartTime);
}

void ACampaignMapActor::UpdateFineSheets(float CameraDistance, const FVector& CameraFocus)
{
	THEATER_SPEED_SCOPE(UpdateFineSheets);
	if (!Data.bWorld || FineXs.Num() < 2 || FineYs.Num() < 2 || !WorldLandMaterial)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	// Which sheets the view wants: those within reach of what the eye looks at, while it is low enough for the
	// coarse sheet's cells to show. (The map's own box needs none: it is fine already.)
	const FVector2D Focus = WorldToMap(CameraFocus);
	if (WorldSeaTwin)
	{
		// (the sea's twin stands to the west while the eye is in the west half of the world as it is kept, and to the east otherwise)
		const double Side = Focus.X < Data.WorldMinX + Data.WorldWidth * 0.5 ? -1.0 : 1.0;
		const FVector Place(0.0, Side * Data.WorldWidth * UnitScale, 0.0);
		if (!WorldSeaTwin->GetRelativeLocation().Equals(Place, 0.5))
		{
			WorldSeaTwin->SetRelativeLocation(Place);
		}
	}
	const bool bLow = CameraDistance < 11000.0f;
	if (bLow != bRiversBeyondShown && RiverMesh)
	{
		// (the ribbons of the wider chart's rivers are seen only from where its fine ground is)
		bRiversBeyondShown = bLow;
		if (RiverMesh->GetNumSections() > 1)
		{
			RiverMesh->SetMeshSectionVisible(1, bLow);
		}
	}
	const double Reach = 260.0 + CameraDistance / UnitScale * 0.55;
	auto Wanted = [&](const FBox2D& Box)
	{
		// (the short way round the world)
		const double Turned = TurnFor(Box.GetCenter().X, Focus.X) * Data.WorldWidth;
		return bLow && Box.ShiftBy(FVector2D(Turned, 0.0)).ComputeSquaredDistanceToPoint(Focus) < Reach * Reach;
	};
	int32 BeingBuilt = 0;
	bool bStillComing = false;
	for (const auto& Pair : FineSheets)
	{
		BeingBuilt += Pair.Value->bDressed ? 0 : 1;
	}
	for (int32 Row = 0; Row + 1 < FineYs.Num(); ++Row)
	{
		for (int32 Column = 0; Column + 1 < FineXs.Num(); ++Column)
		{
			const FBox2D Box(FVector2D(FineXs[Column], FineYs[Row]), FVector2D(FineXs[Column + 1], FineYs[Row + 1]));
			if (Box.GetSize().X < 1.0 || Box.GetSize().Y < 1.0 || !Wanted(Box))
			{
				continue;
			}
			const FVector2D Middle = Box.GetCenter();
			if (Middle.X > 0.0 && Middle.X < Data.Width && Middle.Y > 0.0 && Middle.Y < Data.Height)
			{
				continue; // the map's own ground
			}
			const FIntPoint Key(Column, Row);
			if (const TSharedPtr<FFineSheet, ESPMode::ThreadSafe>* Standing = FineSheets.Find(Key))
			{
				(*Standing)->LastWanted = Now;
				if ((*Standing)->bShelved)
				{
					// (Speed: tc.Speed.SheetShelf.) Wanted again: off the shelf. Its tiles are stood up out of sight a few
					// each frame by the lines below, as a new sheet's are once it is dressed, and then it stands.
					(*Standing)->bShelved = false;
					TheaterSpeed::NoteMade(TheaterSpeed::EMade::SheetsFromShelf);
				}
				bStillComing |= !(*Standing)->bStanding;
				continue;
			}
			if (BeingBuilt >= 2)
			{
				bStillComing = true;
				continue; // two at a time: the rest when these are up
			}
			// Is there any land in it? A box of open sea keeps the coarse sheet's water.
			bool bAnyLand = false;
			for (int32 Try = 0; Try < 81 && !bAnyLand; ++Try)
			{
				bAnyLand = WorldTerrain.CoverageAt(Box.Min.X + Box.GetSize().X * (Try % 9) / 8.0, Box.Min.Y + Box.GetSize().Y * (Try / 9) / 8.0) > 0.25f;
			}
			if (!bAnyLand)
			{
				continue;
			}
			TSharedPtr<FFineSheet, ESPMode::ThreadSafe> Sheet = MakeShared<FFineSheet, ESPMode::ThreadSafe>();
			Sheet->Key = Key;
			Sheet->Box = Box;
			Sheet->LastWanted = Now;
			FineSheets.Add(Key, Sheet);
			++BeingBuilt;
			bStillComing = true;
			// It is modelled and painted on another thread, with an apron round it so that what is smoothed or
			// shaded across its edge comes out as it would on one great sheet.
			const FCampaignMapData* Map = &Data;
			const float Scale = VerticalScale;
			// (well offshore its sea is the coarse sheet's own, so that its edge cannot be found in open water)
			Sheet->Paint.CoarseSea = &WorldPaint;
			Sheet->Paint.CoarseShape = &WorldTerrain;
			// (Speed: tc.Speed.FineAside.) Its pictures and their smaller copies are made there too, for the haze as it
			// lies now; the frame that dresses it takes them where they are still what it would make itself.
			const bool bAside = TheaterSpeed::FineAside();
			TSet<int32> Haze = bAside ? HazeSet : TSet<int32>();
			// (Speed: tc.Speed.QuietModel, for measuring. How its loops run: spread over the cores as before, on this one
			// thread alone, or spread at the background's priority. And where it runs: on the engine's pool as before, or
			// on a thread of its own at the lowest priority. See TheaterSpeed::ModelAside.)
			const int32 Quiet = TheaterSpeed::QuietModel();
			Sheet->Building = TheaterSpeed::ModelAside(Quiet, [Sheet, Map, Box, Scale, bAside, Quiet, HazeThen = MoveTemp(Haze)]()
			{
				const double Began = FPlatformTime::Seconds();
				const TheaterSpeed::FQuietModelling QuietHere(Quiet);
				const double Apron = 36.0;
				Sheet->Shape.BuildOver(*Map, Box.Min.X - Apron, Box.Min.Y - Apron, Box.GetSize().X + 2.0 * Apron, Box.GetSize().Y + 2.0 * Apron, 1.5, true);
				Sheet->Paint.Build(*Map, Sheet->Shape, Scale);
				if (bAside)
				{
					PrepareFineSheet(*Sheet, HazeThen);
				}
				Sheet->ModelSeconds = FPlatformTime::Seconds() - Began; // (for the speed log: read only once bBuilt is seen)
				Sheet->bBuilt = true;
			});
		}
	}

	bFineSheetsComing = bStillComing;

	// Bring on the ones that are built, and take down the ones the eye left a while ago.
	TArray<FIntPoint> Gone;
	bool bDressing = false;
	bool bCaughtUp = false; // (speed: one sheet off the shelf has had its changed ground painted this frame)
	bool bStoodOne = false; // (speed: tc.Speed.StandOne) one sheet has been stood this frame
	for (const auto& Pair : FineSheets)
	{
		FFineSheet& Sheet = *Pair.Value;
		if (!Sheet.bBuilt)
		{
			continue;
		}
		if (!Sheet.bModelNoted)
		{
			Sheet.bModelNoted = true;
			TheaterSpeed::NoteMade(TheaterSpeed::EMade::SheetsModelled);
			if (TheaterSpeed::IsLogging())
			{
				UE_LOG(LogTheater, Display, TEXT("Speed: fine sheet %d,%d was modelled in %.2f s on other threads (%d by %d points)"), Sheet.Key.X, Sheet.Key.Y, Sheet.ModelSeconds, Sheet.Shape.NX, Sheet.Shape.NY);
			}
		}
		if (Sheet.bShelved)
		{
			continue; // (speed: tc.Speed.SheetShelf) on the shelf, with nothing of it on the table
		}
		// (each stands a turn of the world east or west of where it is kept, whichever is nearest the eye)
		Sheet.Land.Turn = TurnFor(Sheet.Box.GetCenter().X, Focus.X);
		if (Sheet.SeaPaint)
		{
			// (its water's-edge line fades out as the eye rises toward where the coarse sheet takes over)
			Sheet.SeaPaint->SetScalarParameterValue(TEXT("Surf"), SurfNow * (1.0f - FMath::SmoothStep(6500.0f, 10000.0f, CameraDistance)));
		}
		if (Sheet.Sea)
		{
			const FVector Place(0.0, Sheet.Land.Turn * Data.WorldWidth * UnitScale, 0.0);
			if (!Sheet.Sea->GetRelativeLocation().Equals(Place, 0.5))
			{
				Sheet.Sea->SetRelativeLocation(Place);
			}
		}
		if (!Sheet.bDressed)
		{
			if (bDressing && TheaterSpeed::DressSteps())
			{
				continue; // (speed: one sheet dressed a frame)
			}
			bDressing = true;
			if (Sheet.HoldersChanged.bIsValid)
			{
				// (Speed: tc.Speed.HoldersLock. Ground changed hands while it was being modelled, which did not wait for
				// it: that ground is painted again first, as it would have been had the sheet been up already.)
				const FBox2D Changed = Sheet.HoldersChanged;
				Sheet.HoldersChanged = FBox2D(ForceInit);
				RepaintBeyond(Changed, &Sheet);
			}
			DressFineSheet(Sheet);
		}
		else if (!Sheet.bStanding)
		{
			if (Sheet.HoldersChanged.bIsValid)
			{
				// (Speed: tc.Speed.SheetShelf. Ground changed hands while it lay on the shelf, which did not paint it
				// then: that ground is painted again now, before any of it is seen, as RepaintBeyond would have.)
				// (One sheet a frame. After a load every province has changed hands, and six sheets came off the shelf
				// together and were each painted whole in the one frame: 108 ms of a 153 ms frame, in the builder's
				// capture of 18 November (Poland, build 1680). The others wait their turn out of sight, as they wait
				// for their tiles: none stands before it is painted.)
				if (bCaughtUp)
				{
					continue;
				}
				bCaughtUp = true;
				const FBox2D Changed = Sheet.HoldersChanged;
				Sheet.HoldersChanged = FBox2D(ForceInit);
				RepaintBeyond(Changed, &Sheet);
			}
			// Its tiles are stood up a few each frame, out of sight; when the view has all it wants of them, the
			// sheet is shown and that box of the coarse sheet is taken away, in the same frame.
			if (!UpdateLand(Sheet.Land, CameraDistance, CameraFocus, false))
			{
				// (Speed: tc.Speed.StandOne. One sheet is stood a frame: standing one stands the coarse sheet's tiles
				// round it up afresh and hands them and its own to the draw thread, 25 to 30 ms of the game thread in
				// the same capture, and two off the shelf stood in one frame made a 45 ms one. The next is ready and
				// out of sight until the next frame, with the coarse sheet still lying there, as while its tiles came.)
				if (bStoodOne && TheaterSpeed::StandOne())
				{
					continue;
				}
				bStoodOne = true;
				StandFineSheet(Sheet);
			}
		}
		else
		{
			UpdateLand(Sheet.Land, CameraDistance, CameraFocus, false);
			// (Kept a while once the eye has moved on, in case it comes back. But once the eye has climbed past the
			// height the fine sheets are for, they are put away at once: left standing they showed from high up as
			// a crisp square beside the coarse ground, and then went all together.)
			if (Now - Sheet.LastWanted > (bLow ? 20.0 : 1.0))
			{
				Gone.Add(Pair.Key);
			}
		}
	}
	// (Speed, 18 November 2026: tc.Speed.SheetShelf. A sheet the eye had left was dropped whole here, and modelled again
	// from nothing when the eye came back: some seconds on every spare core each time, and every sheet in view each
	// time the eye went up past the height they are for and came down again. It is now stood down just the same (the
	// coarse sheet lies there again at once, as before) with its modelling kept, up to the shelf's number of sheets.)
	const int32 Shelf = TheaterSpeed::SheetShelf();
	for (const FIntPoint& Key : Gone)
	{
		if (Shelf > 0)
		{
			ShelveFineSheet(*FineSheets[Key]);
		}
		else
		{
			DropFineSheet(*FineSheets[Key]);
			FineSheets.Remove(Key);
			TheaterSpeed::NoteMade(TheaterSpeed::EMade::SheetsDropped);
		}
	}
	TrimSheetShelf(Shelf); // (and all of the shelf, if the switch has been turned off since)
	if (TheaterSpeed::IsLogging())
	{
		int32 Modelling = 0, Standing = 0, Shelved = 0;
		for (const auto& Pair : FineSheets)
		{
			Modelling += Pair.Value->bBuilt ? 0 : 1;
			Standing += Pair.Value->bStanding ? 1 : 0;
			Shelved += Pair.Value->bShelved ? 1 : 0;
		}
		TheaterSpeed::NoteSheets(Modelling, Standing, Shelved);
	}
}

void ACampaignMapActor::PrepareFineSheet(FFineSheet& Sheet, const TSet<int32>& Haze)
{
	// (On the thread that modelled the sheet.) Every picture DressFineSheet makes, from the same pixels, in the same
	// size and kind: keep the two lists the same. One that does not match is made in the frame, as before.
	const FCampaignGround& Painted = Sheet.Paint;
	auto Ahead = [&Sheet](const TCHAR* Name, const TArray<FColor>& Pixels, int32 Wide, int32 High, bool bColour)
	{
		CampaignTextures::MakeLevels(Wide, High, Pixels, bColour, Sheet.Ahead.FindOrAdd(FName(Name)));
	};
	Ahead(TEXT("Lie"), Painted.Lie, Painted.NX, Painted.NY, false);
	Ahead(TEXT("Ground"), Painted.Ground, Painted.NX, Painted.NY, true);
	Ahead(TEXT("Tint"), Painted.Tint, Painted.NX, Painted.NY, true);
	Ahead(TEXT("Relief"), Painted.Relief, Painted.NX, Painted.NY, false);
	Ahead(TEXT("Masks"), Painted.Masks, Painted.NX, Painted.NY, false);
	Ahead(TEXT("Cover"), Painted.Cover, Painted.NX, Painted.NY, false);
	{
		// (the staff's paper over what nobody of the player's can see there: as DressFineSheet lays it, for this haze)
		TArray<FColor> Fogged;
		Fogged.Init(FColor(0, 0, 0, 255), Painted.NX * Painted.NY);
		if (Haze.Num() > 0 && Sheet.Shape.Region.Num() == Fogged.Num())
		{
			for (int32 Index = 0; Index < Fogged.Num(); ++Index)
			{
				const int32 Id = Painted.PaintedRegion(Index, Sheet.Shape);
				if (Id >= 0 && Haze.Contains(Id))
				{
					Fogged[Index].R = 255;
				}
			}
			SoftenFog(Fogged, Painted.NX, Painted.NY, Sheet.Shape.Cell);
		}
		Ahead(TEXT("Fog"), Fogged, Painted.NX, Painted.NY, false);
		Sheet.FogFor = Haze;
	}
	if (Painted.Occupied.Num() == Painted.NX * Painted.NY)
	{
		Ahead(TEXT("Occupied"), Painted.Occupied, Painted.NX, Painted.NY, true);
	}
	if (Painted.Wet.Num() == Painted.WetNX * Painted.WetNY && Painted.Wet.Num() > 0)
	{
		Ahead(TEXT("Wet"), Painted.Wet, Painted.WetNX, Painted.WetNY, false);
		Ahead(TEXT("WetInk"), Painted.WetInk, Painted.NX, Painted.NY, true);
	}
	Ahead(TEXT("Water"), Painted.Water, Painted.WaterNX, Painted.WaterNY, true);
	Ahead(TEXT("Shore"), Painted.Surf, Painted.WaterNX, Painted.WaterNY, false);
}

void ACampaignMapActor::DressFineSheet(FFineSheet& Sheet)
{
	THEATER_SPEED_SCOPE(DressFineSheet);
	auto Picture = [this, &Sheet](const TArray<FColor>& Pixels, int32 Wide, int32 High, bool bColour, const TCHAR* Name, bool bSharp = false)
	{
		// (Speed.) Made with the sheet where that is still this very picture: the same size and kind, and the same pixels
		// at full size, which the repaint of ground that changed hands may have changed since. Otherwise made here.
		const FName Called(*FString::Printf(TEXT("Fine%s_%d_%d"), Name, Sheet.Key.X, Sheet.Key.Y));
		const CampaignTextures::FLevels* Ready = Sheet.Ahead.Find(FName(Name));
		UTexture2D* Made = Ready && Ready->Fits(Wide, High, bColour, Pixels)
			? CampaignTextures::CreateFromLevels(*Ready, false, Called, bSharp)
			: CampaignTextures::Create(Wide, High, Pixels, bColour, false, Called, bSharp);
		Sheet.Kept.Add(Made);
		FineKeep.Add(Made);
		return Made;
	};
	const FCampaignGround& Painted = Sheet.Paint;
	// (Speed, 17 November 2026: tc.Speed.DressSteps. A sheet was dressed whole in one frame, 50 to 170 ms in the
	// builder's logs, two sheets together at the worst. It is now dressed a step at a time, while about 8 ms of the
	// frame lasts, and taken up again the next frame where it was left: nothing of it is seen until its tiles are
	// stood up, after the last step, as before. With the switch at 0 every step is taken at once.)
	const bool bSteps = TheaterSpeed::DressSteps();
	const double Began = FPlatformTime::Seconds();
	auto Enough = [bSteps, Began]() { return bSteps && FPlatformTime::Seconds() - Began > 0.008; };
	if (Sheet.DressStage == 0)
	{
		THEATER_SPEED_SCOPE(DressPaint);
		// (Speed, tc.Speed.PictureOnce: made unsmoothed from the first. Made smoothed and then again unsmoothed, the
		// second making waited for the drawing thread: about 32 ms of every sheet in the builder's logs, 110 at worst.)
		const bool bPictureOnce = TheaterSpeed::PictureOnce();
		UTexture2D* Lie = nullptr;
		{
			THEATER_SPEED_SCOPE(DressLie);
			Lie = Picture(Painted.Lie, Painted.NX, Painted.NY, false, TEXT("Lie"), bPictureOnce);
			if (Lie && !bPictureOnce)
			{
				Lie->Filter = TF_Nearest;
				Lie->UpdateResource();
			}
		}
		Sheet.LandPaint = UMaterialInstanceDynamic::Create(LandMaterial->Parent, this);
		Sheet.LandPaint->CopyParameterOverrides(LandMaterial);
		// (kept for the engine at once, as its pictures are: it lives through the frames between the steps)
		Sheet.Kept.Add(Sheet.LandPaint);
		FineKeep.Add(Sheet.LandPaint);
		DressBeyond(Sheet.LandPaint);
		// (the place its shading is read at is shaken by a little over half a cell: see the land's paint)
		// (and the grid of thirds of a cell its shake is made on: see the land's paint)
		Sheet.LandPaint->SetVectorParameterValue(TEXT("ReliefJitter"), FLinearColor(1.2f / FMath::Max(Sheet.Paint.NX, 1), 1.2f / FMath::Max(Sheet.Paint.NY, 1), 3.0f * Sheet.Paint.NX, 3.0f * Sheet.Paint.NY));
		Sheet.LandPaint->SetTextureParameterValue(TEXT("Lie"), Lie);
		++Sheet.DressStage;
		if (Enough())
		{
			return;
		}
	}
	if (Sheet.DressStage == 1)
	{
		THEATER_SPEED_SCOPE(DressPictures);
		Sheet.LandPaint->SetTextureParameterValue(TEXT("Ground"), Picture(Painted.Ground, Painted.NX, Painted.NY, true, TEXT("Ground")));
		++Sheet.DressStage;
		if (Enough())
		{
			return;
		}
	}
	if (Sheet.DressStage == 2)
	{
		THEATER_SPEED_SCOPE(DressPictures);
		Sheet.TintPicture = Picture(Painted.Tint, Painted.NX, Painted.NY, true, TEXT("Tint"));
		Sheet.ReliefPicture = Picture(Painted.Relief, Painted.NX, Painted.NY, false, TEXT("Relief"));
		Sheet.LandPaint->SetTextureParameterValue(TEXT("Tint"), Sheet.TintPicture);
		Sheet.LandPaint->SetTextureParameterValue(TEXT("Relief"), Sheet.ReliefPicture);
		++Sheet.DressStage;
		if (Enough())
		{
			return;
		}
	}
	if (Sheet.DressStage == 3)
	{
		THEATER_SPEED_SCOPE(DressPictures);
		Sheet.LandPaint->SetTextureParameterValue(TEXT("Masks"), Picture(Painted.Masks, Painted.NX, Painted.NY, false, TEXT("Masks")));
		Sheet.LandPaint->SetTextureParameterValue(TEXT("Cover"), Picture(Painted.Cover, Painted.NX, Painted.NY, false, TEXT("Cover")));
		++Sheet.DressStage;
		if (Enough())
		{
			return;
		}
	}
	if (Sheet.DressStage == 4)
	{
		THEATER_SPEED_SCOPE(DressFog);
		const CampaignTextures::FLevels* FogAhead = Sheet.Ahead.Find(FName(TEXT("Fog")));
		if (FogAhead && FogAhead->Each.Num() > 0 && Sheet.FogFor.Num() == HazeSet.Num() && Sheet.FogFor.Includes(HazeSet))
		{
			// (Speed.) Laid with the sheet, for the haze as it lies now.
			Sheet.FogPicture = Picture(FogAhead->Each[0], Painted.NX, Painted.NY, false, TEXT("Fog"));
			Sheet.LandPaint->SetTextureParameterValue(TEXT("Fog"), Sheet.FogPicture);
		}
		else
		{
			// (the staff's paper over what nobody of the player's can see there, as it stands now)
			TArray<FColor> Fogged;
			Fogged.Init(FColor(0, 0, 0, 255), Painted.NX * Painted.NY);
			if (HazeSet.Num() > 0 && Sheet.Shape.Region.Num() == Fogged.Num())
			{
				for (int32 Index = 0; Index < Fogged.Num(); ++Index)
				{
					const int32 Id = Painted.PaintedRegion(Index, Sheet.Shape);
					if (Id >= 0 && HazeSet.Contains(Id))
					{
						Fogged[Index].R = 255;
					}
				}
				SoftenFog(Fogged, Painted.NX, Painted.NY, Sheet.Shape.Cell);
			}
			Sheet.FogPicture = Picture(Fogged, Painted.NX, Painted.NY, false, TEXT("Fog"));
			Sheet.LandPaint->SetTextureParameterValue(TEXT("Fog"), Sheet.FogPicture);
		}
		++Sheet.DressStage;
		if (Enough())
		{
			return;
		}
	}
	if (Sheet.DressStage == 5)
	{
		THEATER_SPEED_SCOPE(DressPictures);
		// (occupied ground beyond Europe keeps its old owner's stripes too)
		Sheet.OccupiedPicture = Painted.Occupied.Num() == Painted.NX * Painted.NY ? Picture(Painted.Occupied, Painted.NX, Painted.NY, true, TEXT("Occupied")) : nullptr;
		Sheet.LandPaint->SetTextureParameterValue(TEXT("Occupied"), Sheet.OccupiedPicture ? Sheet.OccupiedPicture : NoStripesPicture.Get());
		if (Painted.Wet.Num() == Painted.WetNX * Painted.WetNY && Painted.Wet.Num() > 0)
		{
			Sheet.LandPaint->SetTextureParameterValue(TEXT("Wet"), Picture(Painted.Wet, Painted.WetNX, Painted.WetNY, false, TEXT("Wet")));
			Sheet.LandPaint->SetTextureParameterValue(TEXT("WetInk"), Picture(Painted.WetInk, Painted.NX, Painted.NY, true, TEXT("WetInk")));
			Sheet.LandPaint->SetScalarParameterValue(TEXT("WetReach"), Painted.WetReach);
			Sheet.LandPaint->SetScalarParameterValue(TEXT("WetCell"), static_cast<float>(Sheet.Shape.Cell / FMath::Max(Painted.WetScale, 1))); // (the waters' picture is ruled finer than the ground)
		}
		++Sheet.DressStage;
		if (Enough())
		{
			return;
		}
	}
	if (Sheet.DressStage == 6)
	{
		THEATER_SPEED_SCOPE(DressPictures);
		Sheet.SeaPaint = UMaterialInstanceDynamic::Create(SeaMaterial->Parent, this);
		Sheet.SeaPaint->CopyParameterOverrides(SeaMaterial);
		Sheet.SeaPaint->SetTextureParameterValue(TEXT("Water"), Picture(Painted.Water, Painted.WaterNX, Painted.WaterNY, true, TEXT("Water")));
		Sheet.SeaPaint->SetTextureParameterValue(TEXT("Shore"), Picture(Painted.Surf, Painted.WaterNX, Painted.WaterNY, false, TEXT("Shore")));
		Sheet.Kept.Add(Sheet.SeaPaint);
		FineKeep.Add(Sheet.SeaPaint);
		++Sheet.DressStage;
		if (Enough())
		{
			return;
		}
	}
	{
		THEATER_SPEED_SCOPE(DressLand);
		Sheet.Land = FLandSheet();
		Sheet.Land.Shape = &Sheet.Shape;
		Sheet.Land.Material = Sheet.LandPaint;
		Sheet.Land.bKeep = true;
		Sheet.Land.Keep = Sheet.Box;
		Sheet.Land.bHidden = true;
		Sheet.Land.WrapMode = 2;
		// (where it meets the map's own ground, the map's is the one seen)
		Sheet.Land.Holes.Add(FBox2D(FVector2D(0.0, 0.0), FVector2D(Data.Width, Data.Height)));
		Sheet.Land.Sink = 0.03;
		LevelLand(Sheet.Land);
	}
	Sheet.bDressed = true;
	Sheet.Ahead.Empty();
	Sheet.FogFor.Empty();
}

void ACampaignMapActor::StandFineSheet(FFineSheet& Sheet)
{
	THEATER_SPEED_SCOPE(StandFineSheet);
	Sheet.Land.bHidden = false;
	for (const auto& Tile : Sheet.Land.Tiles)
	{
		if (Tile.Value)
		{
			Tile.Value->SetVisibility(true);
		}
	}
	// Its own water, a hair above the coarse sheet's: the shore and the shallows drawn as finely as its land.
	if (!Sheet.Sea)
	{
		Sheet.Sea = UTheaterStillMesh::Make(this, TEXT("SheetSea"));
		Sheet.Sea->SetupAttachment(Root);
		Sheet.Sea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sheet.Sea->SetCastShadow(false);
		Sheet.Sea->RegisterComponent();
	}
	{
		const FCampaignGround& Painted = Sheet.Paint;
		FCampaignMeshBuffers Buffers;
		const int32 Across = 4, Down = 4;
		for (int32 J = 0; J <= Down; ++J)
		{
			for (int32 I = 0; I <= Across; ++I)
			{
				const double X = Sheet.Box.Min.X + Sheet.Box.GetSize().X * I / Across, Y = Sheet.Box.Min.Y + Sheet.Box.GetSize().Y * J / Down;
				const FVector2D UV(((X - Sheet.Shape.OriginX) / Painted.WaterStep + 0.5) / Painted.WaterNX, ((Y - Sheet.Shape.OriginY) / Painted.WaterStep + 0.5) / Painted.WaterNY);
				Buffers.AddVertex(MapToWorld(X, Y, 0.03), FVector::UpVector, FColor::White, UV);
			}
		}
		for (int32 J = 0; J < Down; ++J)
		{
			for (int32 I = 0; I < Across; ++I)
			{
				const int32 A = J * (Across + 1) + I, B = A + 1, C = A + Across + 1, D = C + 1;
				Buffers.Triangles.Append({ A, C, B, B, C, D });
			}
		}
		Buffers.Commit(Sheet.Sea, 0, Sheet.SeaPaint);
		Sheet.Sea->SetVisibility(true);
	}
	Sheet.bStanding = true;
	ForgetKeptPoints(Sheet.Box); // (speed: the ground here is now the fine sheet's)
	WorldLand.Holes.Add(Sheet.Box);
	RetileWorld(Sheet.Box);
}

void ACampaignMapActor::DropFineSheet(FFineSheet& Sheet)
{
	if (Sheet.Building.IsValid())
	{
		Sheet.Building.Wait();
	}
	TakeSheetOffTable(Sheet);
	for (UObject* Kept : Sheet.Kept)
	{
		FineKeep.Remove(Kept);
	}
	Sheet.Kept.Reset();
	Sheet.LandPaint = nullptr;
	Sheet.SeaPaint = nullptr;
	Sheet.bShelved = false;
}

void ACampaignMapActor::TakeSheetOffTable(FFineSheet& Sheet)
{
	for (const auto& Tile : Sheet.Land.Tiles)
	{
		if (UProceduralMeshComponent* Mesh = Tile.Value)
		{
			Mesh->SetVisibility(false);
			Mesh->ClearAllMeshSections();
			SpareLandMeshes.Add(Mesh);
		}
	}
	Sheet.Land.Tiles.Reset();
	if (Sheet.Sea)
	{
		Sheet.Sea->DestroyComponent();
		Sheet.Sea = nullptr;
	}
	if (Sheet.bStanding)
	{
		Sheet.bStanding = false;
		ForgetKeptPoints(Sheet.Box); // (speed: the ground here is the coarse sheet's again)
		WorldLand.Holes.RemoveAll([&Sheet](const FBox2D& Hole) { return Hole.Min.Equals(Sheet.Box.Min, 0.01) && Hole.Max.Equals(Sheet.Box.Max, 0.01); });
		RetileWorld(Sheet.Box);
	}
}

void ACampaignMapActor::ShelveFineSheet(FFineSheet& Sheet)
{
	THEATER_SPEED_SCOPE(ShelveFineSheet);
	// (Speed: tc.Speed.SheetShelf.) Off the table exactly as a dropped sheet goes: its tiles to the spares, its water
	// away, its box of the coarse sheet closed and stood up again in the same frame. What is kept is what took the
	// seconds to make: its heights (Shape), its paint (Paint), its pictures and paints (Kept, which FineKeep holds), its
	// levels of heights (Land) and its place in FineSheets, where the haze (FogBeyond), the seasons (LandScalar and the
	// rest) and the sky (UpdateSky) still find it. Ground that changes hands is painted when it comes off the shelf.
	TakeSheetOffTable(Sheet);
	Sheet.Land.bHidden = true; // (its tiles are stood up out of sight again when it is wanted, and then it stands)
	Sheet.bShelved = true;
	TheaterSpeed::NoteMade(TheaterSpeed::EMade::SheetsShelved);
}

void ACampaignMapActor::TrimSheetShelf(int32 KeepAtMost)
{
	for (;;)
	{
		int32 Shelved = 0;
		FIntPoint Oldest = FIntPoint::ZeroValue;
		double OldestWanted = TNumericLimits<double>::Max();
		for (const auto& Pair : FineSheets)
		{
			if (Pair.Value->bShelved)
			{
				++Shelved;
				if (Pair.Value->LastWanted < OldestWanted)
				{
					OldestWanted = Pair.Value->LastWanted;
					Oldest = Pair.Key;
				}
			}
		}
		if (Shelved <= FMath::Max(KeepAtMost, 0))
		{
			return;
		}
		DropFineSheet(*FineSheets[Oldest]);
		FineSheets.Remove(Oldest);
		TheaterSpeed::NoteMade(TheaterSpeed::EMade::SheetsDropped);
	}
}

void ACampaignMapActor::RetileWorld(const FBox2D& Changed)
{
	THEATER_SPEED_SCOPE(RetileWorld);
	// Every tile of the coarse sheet that touches the box is stood up afresh, at once: with the hole, or without it.
	const FCampaignTerrain& Shape = WorldTerrain;
	TArray<uint64> Again;
	for (const auto& Tile : WorldLand.Tiles)
	{
		const int32 Level = static_cast<int32>(Tile.Key >> 48), TileX = static_cast<int32>((Tile.Key >> 24) & 0xFFFFFF), TileY = static_cast<int32>(Tile.Key & 0xFFFFFF);
		const double Side = TileQuads * Shape.Cell * (1 << Level), Margin = 4.0 * Shape.Cell * (1 << Level);
		const FBox2D Box(FVector2D(Shape.OriginX + TileX * Side - Margin, Shape.OriginY + TileY * Side - Margin), FVector2D(Shape.OriginX + (TileX + 1) * Side + Margin, Shape.OriginY + (TileY + 1) * Side + Margin));
		if (Box.Intersect(Changed))
		{
			Again.Add(Tile.Key);
		}
	}
	for (const uint64 Key : Again)
	{
		if (UProceduralMeshComponent* Mesh = WorldLand.Tiles[Key])
		{
			Mesh->SetVisibility(false);
			Mesh->ClearAllMeshSections();
			SpareLandMeshes.Add(Mesh);
		}
		const int32 Level = static_cast<int32>(Key >> 48), TileX = static_cast<int32>((Key >> 24) & 0xFFFFFF), TileY = static_cast<int32>(Key & 0xFFFFFF);
		WorldLand.Tiles[Key] = LandTileHasLand(WorldLand, Level, TileX, TileY) ? BuildLandTile(WorldLand, Level, TileX, TileY) : nullptr;
	}
}

void ACampaignMapActor::BuildSeaMesh()
{
	SeaMesh->ClearAllMeshSections();

	// One flat sheet. Its colour is a picture (pale over the shoals, dark over the deeps), so it needs no
	// more points than its corners; a few more are given so that it is lit evenly from corner to corner.
	const int32 Across = 12, Down = 12;
	FCampaignMeshBuffers Buffers;
	for (int32 J = 0; J <= Down; ++J)
	{
		for (int32 I = 0; I <= Across; ++I)
		{
			const double X = Data.Width * I / Across, Y = Data.Height * J / Down;
			// The water's picture has a pixel every WaterStep map units, the first at the map's corner.
			const FVector2D UV((X / Paint.WaterStep + 0.5) / Paint.WaterNX, (Y / Paint.WaterStep + 0.5) / Paint.WaterNY);
			Buffers.AddVertex(MapToWorld(X, Y, 0.0), FVector::UpVector, FColor::White, UV);
		}
	}
	for (int32 J = 0; J < Down; ++J)
	{
		for (int32 I = 0; I < Across; ++I)
		{
			const int32 A = J * (Across + 1) + I, B = A + 1, C = A + Across + 1, D = C + 1;
			Buffers.Triangles.Append({ A, C, B, B, C, D });
		}
	}
	Buffers.Commit(SeaMesh, 0, SeaMaterial);
}

void ACampaignMapActor::BuildBorders()
{
	THEATER_SPEED_SCOPE(BuildBorders);
	// (Speed, ruling b414, build 1596: this took 22 to 100 ms each time the ink arrived, laying every line of the world
	// again for the few edges whose kind had changed. tc.Speed.BordersKept keeps the lines' points and hands over only
	// the kinds of line that changed: BuildBordersKept. The same lines either way. It reads the points SurfacePointKept
	// keeps, so with tc.Speed.KeptHeights off the lines are laid here, as before.)
	if (TheaterSpeed::BordersKept() && TheaterSpeed::KeptHeights())
	{
		BuildBordersKept();
		return;
	}
	for (bool& bStale : bBorderLineStale)
	{
		bStale = true; // (laid here whole: the kept way hands every line over again next time)
	}
	BorderMesh->ClearAllMeshSections();

	// All the map's lines are dark brown ink, never pure black, and each kind has one weight. A frontier is inked
	// firmly; a province line is faint (how faint depends on how high the eye is: see UpdateForCamera).
	const FColor NationalInk = FromHex(TEXT("1E1612")).ToFColor(true);
	const FColor RegionInk = FromHex(TEXT("1E1814")).ToFColor(false);
	// (on a dark country the same faint line is drawn in the lettering's cream instead, so that a province edge
	// can be found at working height without holding a key)
	const FColor RegionInkPale = FromHex(TEXT("F4E8CC")).ToFColor(false);
	const double Lift = 0.13;

	FCampaignMeshBuffers Buffers, Faint, FaintPale;
	int32 NationalCount = 0, RegionCount = 0;
	// (Speed, 17 November 2026.) What lies across each edge of each outline is found once and kept: the outlines never
	// change, only who holds them. Finding it again for every edge of the world, each time one province changed
	// hands, was most of the 190 ms this took.
	const bool bKnown = BorderAcrossFor == Data.Regions.Num() && BorderAcross.Num() > 0;
	if (!bKnown)
	{
		BorderAcross.Reset();
	}
	int32 EdgeAt = 0;

	for (const FCampaignRegion& Region : Data.Regions)
	{
		for (const TArray<FVector2D>& Outline : Region.Polygons)
		{
			double SignedArea = 0.0;
			for (int32 i = 0, n = Outline.Num(); i < n; ++i)
			{
				SignedArea += Outline[i].X * Outline[(i + 1) % n].Y - Outline[(i + 1) % n].X * Outline[i].Y;
			}
			const double Outward = SignedArea >= 0.0 ? 1.0 : -1.0;

			// (The art director's round 34: where the frontier meets the Amazon at Leticia the outline doubles back on
			// itself three times in a few units, and the inked line was a saw. A point where two short edges
			// turn back by more than about 115 degrees is left out of the line that is drawn; the ground's own paint
			// keeps the outline as it is.)
			TArray<FVector2D> Ring = Outline;
			for (int32 Pass = 0; Pass < 4; ++Pass)
			{
				bool bCut = false;
				for (int32 i = 0; i < Ring.Num() && Ring.Num() > 4; ++i)
				{
					const int32 Count = Ring.Num();
					const FVector2D Into = Ring[i] - Ring[(i + Count - 1) % Count], Onward = Ring[(i + 1) % Count] - Ring[i];
					const double LongIn = Into.Size(), LongOn = Onward.Size();
					if (LongIn > 1e-6 && LongOn > 1e-6 && LongIn < 2.6 && LongOn < 2.6 && FVector2D::DotProduct(Into, Onward) / (LongIn * LongOn) < -0.4)
					{
						Ring.RemoveAt(i);
						--i;
						bCut = true;
					}
				}
				if (!bCut)
				{
					break;
				}
			}
			const bool bTrimmed = Ring.Num() != Outline.Num();

			for (int32 i = 0, n = Ring.Num(); i < n; ++i)
			{
				const FVector2D A = Ring[i];
				const FVector2D B = Ring[(i + 1) % n];
				const double Length = FVector2D::Distance(A, B);
				if (Length < 0.05)
				{
					continue;
				}
				const FVector2D Dir = (B - A) / Length;
				const FVector2D Side = FVector2D(Dir.Y, -Dir.X) * Outward;

				// What lies just across this edge decides what kind of line it is.
				const FVector2D Probe = (A + B) * 0.5 + Side * 0.35;
				int32 Other = INDEX_NONE;
				if (bKnown && BorderAcross.IsValidIndex(EdgeAt))
				{
					Other = BorderAcross[EdgeAt];
				}
				else
				{
					Other = Data.FindRegionAt(Probe.X, Probe.Y);
					// (an edge laid across a tooth that was left out may lie a little inside its own province: look further)
					for (const double Further : { 0.8, 1.3 })
					{
						if (bTrimmed && Other == Region.Id)
						{
							Other = Data.FindRegionAt(Probe.X + Side.X * (Further - 0.35), Probe.Y + Side.Y * (Further - 0.35));
						}
					}
					if (!bKnown)
					{
						BorderAcross.Add(Other);
					}
				}
				++EdgeAt;
				if (Other == INDEX_NONE || Other == Region.Id || Other < Region.Id)
				{
					continue; // coast, or a line the neighbour has already drawn
				}
				const bool bNational = Data.Regions[Other].Owner != Region.Owner;
				const double HalfWidth = bNational ? 0.2 : 0.12; // (a frontier is a fine inked line: the band of stain beside it says whose side is whose)
				const FCampaignNation* Whose = Data.FindNation(Region.Owner);
				const bool bDarkGround = Whose && (0.30f * Whose->Color.R + 0.59f * Whose->Color.G + 0.11f * Whose->Color.B) / 255.0f < 0.42f;
				const FColor Ink = bNational ? NationalInk : bDarkGround ? RegionInkPale : RegionInk;
				FCampaignMeshBuffers& Into = bNational ? Buffers : bDarkGround ? FaintPale : Faint;
				(bNational ? NationalCount : RegionCount)++;

				// Lay the line over the relief as a ribbon, a short step at a time.
				const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Length / 1.4));
				int32 PrevLeft = INDEX_NONE, PrevRight = INDEX_NONE;
				for (int32 k = 0; k <= Steps; ++k)
				{
					// The ends run on a little so that consecutive ribbons meet at corners.
					const double T = k == 0 ? -HalfWidth * 0.6 : (k == Steps ? Length + HalfWidth * 0.6 : Length * k / Steps);
					const FVector2D P = A + Dir * T;
					const FVector2D L = P + Side * HalfWidth;
					const FVector2D R = P - Side * HalfWidth;
					const int32 Left = Into.AddVertex(SurfacePointKept(L.X, L.Y, Lift), FVector::UpVector, Ink);
					const int32 Right = Into.AddVertex(SurfacePointKept(R.X, R.Y, Lift), FVector::UpVector, Ink);
					if (PrevLeft != INDEX_NONE)
					{
						Into.AddTriangleFacingUp(PrevLeft, PrevRight, Left);
						Into.AddTriangleFacingUp(PrevRight, Left, Right);
					}
					PrevLeft = Left;
					PrevRight = Right;
				}
			}
		}
	}
	if (!bKnown)
	{
		BorderAcrossFor = Data.Regions.Num();
	}
	{
		THEATER_SPEED_SCOPE(BordersCommit); // (how much of the lines' cost is handing them to the graphics chip)
		Buffers.Commit(BorderMesh, 0, InkMaterial);
		Faint.Commit(BorderMesh, 1, ProvinceInk);
		FaintPale.Commit(BorderMesh, 2, ProvincePaleInk);
	}
	UE_LOG(LogTheater, Log, TEXT("Borders: %d frontier edges, %d region edges"), NationalCount, RegionCount);
}

void ACampaignMapActor::LearnBorderEdges()
{
	THEATER_SPEED_SCOPE(LearnBorderEdges);
	// (The edges BuildBorders draws, found the way it finds them, in the order it draws them: the same outlines, the same
	// teeth left out, the same province across each edge. Only the edges a line is drawn along are kept.)
	BorderEdges.Reset();
	for (int32 RegionIndex = 0; RegionIndex < Data.Regions.Num(); ++RegionIndex)
	{
		const FCampaignRegion& Region = Data.Regions[RegionIndex];
		for (const TArray<FVector2D>& Outline : Region.Polygons)
		{
			double SignedArea = 0.0;
			for (int32 i = 0, n = Outline.Num(); i < n; ++i)
			{
				SignedArea += Outline[i].X * Outline[(i + 1) % n].Y - Outline[(i + 1) % n].X * Outline[i].Y;
			}
			const double Outward = SignedArea >= 0.0 ? 1.0 : -1.0;

			// (the teeth left out of the line: see BuildBorders)
			TArray<FVector2D> Ring = Outline;
			for (int32 Pass = 0; Pass < 4; ++Pass)
			{
				bool bCut = false;
				for (int32 i = 0; i < Ring.Num() && Ring.Num() > 4; ++i)
				{
					const int32 Count = Ring.Num();
					const FVector2D Into = Ring[i] - Ring[(i + Count - 1) % Count], Onward = Ring[(i + 1) % Count] - Ring[i];
					const double LongIn = Into.Size(), LongOn = Onward.Size();
					if (LongIn > 1e-6 && LongOn > 1e-6 && LongIn < 2.6 && LongOn < 2.6 && FVector2D::DotProduct(Into, Onward) / (LongIn * LongOn) < -0.4)
					{
						Ring.RemoveAt(i);
						--i;
						bCut = true;
					}
				}
				if (!bCut)
				{
					break;
				}
			}
			const bool bTrimmed = Ring.Num() != Outline.Num();

			for (int32 i = 0, n = Ring.Num(); i < n; ++i)
			{
				const FVector2D A = Ring[i];
				const FVector2D B = Ring[(i + 1) % n];
				const double Length = FVector2D::Distance(A, B);
				if (Length < 0.05)
				{
					continue;
				}
				const FVector2D Dir = (B - A) / Length;
				const FVector2D Side = FVector2D(Dir.Y, -Dir.X) * Outward;
				const FVector2D Probe = (A + B) * 0.5 + Side * 0.35;
				int32 Other = Data.FindRegionAt(Probe.X, Probe.Y);
				for (const double Further : { 0.8, 1.3 })
				{
					if (bTrimmed && Other == Region.Id)
					{
						Other = Data.FindRegionAt(Probe.X + Side.X * (Further - 0.35), Probe.Y + Side.Y * (Further - 0.35));
					}
				}
				if (Other == INDEX_NONE || Other == Region.Id || Other < Region.Id)
				{
					continue; // coast, or a line the neighbour draws
				}
				FBorderEdge& Edge = BorderEdges.AddDefaulted_GetRef();
				Edge.Region = RegionIndex;
				Edge.Other = Other;
				Edge.A = A;
				Edge.Dir = Dir;
				Edge.Side = Side;
				Edge.Length = Length;
				Edge.Steps = FMath::Max(1, FMath::CeilToInt32(Length / 1.4));
				Edge.Bounds = FBox2D(ForceInit);
				Edge.Bounds += A;
				Edge.Bounds += B;
				Edge.Bounds = Edge.Bounds.ExpandBy(0.5); // (the widest ribbon is 0.2 either side, and runs on 0.12 at each end)
			}
		}
	}
	BorderEdgesFor = Data.Regions.Num();
	for (bool& bStale : bBorderLineStale)
	{
		bStale = true;
	}
}

void ACampaignMapActor::BuildBordersKept()
{
	// (The same inks, widths and steps as BuildBorders; see there.)
	const FColor NationalInk = FromHex(TEXT("1E1612")).ToFColor(true);
	const FColor RegionInk = FromHex(TEXT("1E1814")).ToFColor(false);
	const FColor RegionInkPale = FromHex(TEXT("F4E8CC")).ToFColor(false);
	const double Lift = 0.13;

	if (BorderEdgesFor != Data.Regions.Num())
	{
		LearnBorderEdges();
	}
	const float GroundScale = VerticalScale * UnitScale;
	if (BorderEdgesScale != GroundScale)
	{
		// (the ground is laid at another height: SurfacePointKept forgets every point it kept, and every ribbon goes too)
		for (FBorderEdge& Edge : BorderEdges)
		{
			Edge.Laid[0].Empty();
			Edge.Laid[1].Empty();
		}
		for (bool& bStale : bBorderLineStale)
		{
			bStale = true;
		}
		BorderEdgesScale = GroundScale;
	}

	// Which line each edge is now. An edge that has become another kind of line takes both its old section and its new
	// one to be handed over again.
	int32 NationalCount = 0, RegionCount = 0;
	TArray<int8> DarkOf;
	DarkOf.Init(-1, Data.Regions.Num());
	for (FBorderEdge& Edge : BorderEdges)
	{
		const FCampaignRegion& Region = Data.Regions[Edge.Region];
		const bool bNational = Data.Regions[Edge.Other].Owner != Region.Owner;
		int32 Line = 0;
		if (!bNational)
		{
			int8& Dark = DarkOf[Edge.Region];
			if (Dark < 0)
			{
				const FCampaignNation* Whose = Data.FindNation(Region.Owner);
				Dark = Whose && (0.30f * Whose->Color.R + 0.59f * Whose->Color.G + 0.11f * Whose->Color.B) / 255.0f < 0.42f ? 1 : 0;
			}
			Line = Dark ? 2 : 1;
		}
		(bNational ? NationalCount : RegionCount)++;
		if (Edge.Line != Line)
		{
			if (Edge.Line != INDEX_NONE)
			{
				bBorderLineStale[Edge.Line] = true;
			}
			bBorderLineStale[Line] = true;
			Edge.Line = Line;
		}
	}

	for (int32 Line = 0; Line < 3; ++Line)
	{
		if (!bBorderLineStale[Line])
		{
			continue; // (no edge of this kind has changed, nor the ground under one: it stands as it was handed over)
		}
		bBorderLineStale[Line] = false;
		const bool bNational = Line == 0;
		const double HalfWidth = bNational ? 0.2 : 0.12;
		const FColor Ink = bNational ? NationalInk : Line == 2 ? RegionInkPale : RegionInk;
		FCampaignMeshBuffers Into;
		{
			int32 Points = 0, Faces = 0;
			for (const FBorderEdge& Edge : BorderEdges)
			{
				if (Edge.Line == Line)
				{
					Points += 2 * (Edge.Steps + 1);
					Faces += 2 * Edge.Steps;
				}
			}
			Into.Vertices.Reserve(Points);
			Into.Normals.Reserve(Points);
			Into.Colors.Reserve(Points);
			Into.UVs.Reserve(Points);
			Into.Tangents.Reserve(Points);
			Into.Triangles.Reserve(Faces * 3);
		}
		for (FBorderEdge& Edge : BorderEdges)
		{
			if (Edge.Line != Line)
			{
				continue;
			}
			TArray<FVector>& Laid = Edge.Laid[bNational ? 1 : 0];
			if (Laid.Num() != 2 * (Edge.Steps + 1))
			{
				// Lay the line over the relief as a ribbon, a short step at a time (as BuildBorders does, point for point).
				Laid.SetNumUninitialized(2 * (Edge.Steps + 1));
				for (int32 k = 0; k <= Edge.Steps; ++k)
				{
					const double T = k == 0 ? -HalfWidth * 0.6 : (k == Edge.Steps ? Edge.Length + HalfWidth * 0.6 : Edge.Length * k / Edge.Steps);
					const FVector2D P = Edge.A + Edge.Dir * T;
					const FVector2D L = P + Edge.Side * HalfWidth;
					const FVector2D R = P - Edge.Side * HalfWidth;
					Laid[2 * k] = SurfacePointKept(L.X, L.Y, Lift);
					Laid[2 * k + 1] = SurfacePointKept(R.X, R.Y, Lift);
				}
			}
			int32 PrevLeft = INDEX_NONE, PrevRight = INDEX_NONE;
			for (int32 k = 0; k <= Edge.Steps; ++k)
			{
				const int32 Left = Into.AddVertex(Laid[2 * k], FVector::UpVector, Ink);
				const int32 Right = Into.AddVertex(Laid[2 * k + 1], FVector::UpVector, Ink);
				if (PrevLeft != INDEX_NONE)
				{
					Into.AddTriangleFacingUp(PrevLeft, PrevRight, Left);
					Into.AddTriangleFacingUp(PrevRight, Left, Right);
				}
				PrevLeft = Left;
				PrevRight = Right;
			}
		}
		THEATER_SPEED_SCOPE(BordersCommit);
		if (!Into.Commit(BorderMesh, Line, bNational ? InkMaterial : Line == 2 ? ProvincePaleInk : ProvinceInk))
		{
			BorderMesh->ClearMeshSection(Line); // (no edge of this kind is left)
		}
	}
	UE_LOG(LogTheater, Log, TEXT("Borders: %d frontier edges, %d region edges"), NationalCount, RegionCount);
}

void ACampaignMapActor::ForgetBorderEdges(const FBox2D& Box)
{
	if (BorderEdges.Num() == 0 || !Box.bIsValid)
	{
		return;
	}
	// (SurfacePointKept forgets the squares of KeptSquare units that come within a unit of Box, in the turn of the world
	// they are kept in. Every ribbon with a point that may be among them is laid again; one laid again where its points
	// are all still kept is laid the same.)
	const double Slack = KeptSquare + 2.0;
	const bool bRound = Data.bWorld && Data.WorldWidth > 0.0;
	for (FBorderEdge& Edge : BorderEdges)
	{
		if ((Edge.Laid[0].Num() == 0 && Edge.Laid[1].Num() == 0)
			|| Edge.Bounds.Max.Y + Slack < Box.Min.Y || Edge.Bounds.Min.Y - Slack > Box.Max.Y)
		{
			continue;
		}
		// (round a world, Box a whole number of turns east or west of the edge is over it too: is there such a number?)
		const bool bOver = bRound
			? FMath::CeilToDouble((Edge.Bounds.Min.X - Slack - Box.Max.X) / Data.WorldWidth) <= FMath::FloorToDouble((Edge.Bounds.Max.X + Slack - Box.Min.X) / Data.WorldWidth)
			: Edge.Bounds.Max.X + Slack >= Box.Min.X && Edge.Bounds.Min.X - Slack <= Box.Max.X;
		if (bOver)
		{
			Edge.Laid[0].Empty();
			Edge.Laid[1].Empty();
			if (Edge.Line != INDEX_NONE)
			{
				bBorderLineStale[Edge.Line] = true;
			}
		}
	}
}

void ACampaignMapActor::SetLand(const FLandFeatures& InLand)
{
	THEATER_SPEED_SCOPE(SetLand);
	Land = InLand;
	// (The art director, 18 November 2026, at Quebec: the rules' bridges over the St Lawrence stood on dry land beside a
	// pond. The chart's line for that river ends where its estuary begins, a channel too narrow for a sheet of ground
	// to show as sea, and three of its crossings lie beyond that end. A river is drawn on through its own crossings
	// that lie beyond its mouth, the nearest first, so that every bridge has water under it.)
	for (int32 Index = 0; Index < Land.Rivers.Num(); ++Index)
	{
		TArray<FVector2D>& Course = Land.Rivers[Index].Course;
		if (Course.Num() < 2)
		{
			continue;
		}
		TArray<FVector2D> Beyond;
		for (const FLandCrossing& Crossing : Land.Crossings)
		{
			if (Crossing.River != Index)
			{
				continue;
			}
			double Off = 1.0e9;
			for (int32 K = 0; K + 1 < Course.Num(); ++K)
			{
				Off = FMath::Min(Off, FMath::PointDistToSegment(FVector(Crossing.At, 0.0), FVector(Course[K], 0.0), FVector(Course[K + 1], 0.0)));
			}
			if (Off > 3.0 && FVector2D::Distance(Crossing.At, Course.Last()) < FVector2D::Distance(Crossing.At, Course[0]))
			{
				Beyond.Add(Crossing.At);
			}
		}
		const int32 Added = Beyond.Num();
		while (Beyond.Num() > 0)
		{
			int32 Next = 0;
			for (int32 K = 1; K < Beyond.Num(); ++K)
			{
				if (FVector2D::DistSquared(Beyond[K], Course.Last()) < FVector2D::DistSquared(Beyond[Next], Course.Last()))
				{
					Next = K;
				}
			}
			if (FVector2D::Distance(Beyond[Next], Course.Last()) > 40.0)
			{
				break; // (too far to be this river's own water)
			}
			Course.Add(Beyond[Next]);
			Beyond.RemoveAtSwap(Next);
		}
		if (Added > Beyond.Num())
		{
			UE_LOG(LogTheater, Display, TEXT("Rivers: the %s is drawn on through %d of its crossings that lie beyond its mouth"), *Land.Rivers[Index].Name, Added - Beyond.Num());
		}
	}
	bLandLaid = true;
	bWorksShown = false;
	BuildWaterways();
	BuildRailways();
	BuildWorks();
}

void ACampaignMapActor::ShowBuildings(const TArray<FIntVector4>& Works)
{
	THEATER_SPEED_SCOPE(ShowBuildings);
	// (release 2, the flak brief: the guns are stood again where the switch is turned)
	const bool bFlakRing = FlakRingAsked();
	if (!BuildingsMesh || !Data.IsLoaded() || (Works == ShownBuildings && bFlakRing == bFlakRingShown))
	{
		return;
	}
	ShownBuildings = Works;
	bFlakRingShown = bFlakRing;
	FlakGuns.Reset();
	LastWorks = Works;
	BuildingsMesh->ClearAllMeshSections();
	WorksChimneys.Reset();
	WorksChimneyRegion.Reset();
	AirfieldSpots.Reset();
	AirfieldLie.Reset();
	ShownAirSigns.Reset(); // (the aircraft parked on the airfields are stood again where the airfields now are)

	const FLinearColor Brick(FColor::FromHex(TEXT("7A5547"))), Soot(FColor::FromHex(TEXT("3B3532"))), Pane(FColor::FromHex(TEXT("8FA6A8")));
	const FLinearColor Apron(FColor::FromHex(TEXT("A9A69C"))), Girder(FColor::FromHex(TEXT("4A4F55"))), RedLead(FColor::FromHex(TEXT("8A3B2A")));
	const FLinearColor Sandbag(FColor::FromHex(TEXT("B9A77C"))), GunPaint(FColor::FromHex(TEXT("3E4640"))), Tin(FColor::FromHex(TEXT("7E8A84")));
	const FLinearColor Turf(FColor::FromHex(TEXT("6E7F4E"))), Orange(FColor::FromHex(TEXT("D9772B"))), Timber(FColor::FromHex(TEXT("6B5236")));

	FMiniatureMesh Model;
	const double Unit = UnitScale * 2.1; // the size the towns are built to
	int32 Count = 0;
	int32 RingTowns = 0, RingAsked = 0, RingSeated = 0, RingRows = 0, RingTwos = 0, RingNone = 0, RingPlots = 0; // (release 2, the flak brief: what the ring came to, for the log)
	// (The flak ring's second mend.) Which of a town's six public plots are built on or going up, a bit a line, as
	// ShowCivic was last given them: a pit makes way for those.
	TMap<int32, int32> PlotsTaken;
	if (bFlakRing)
	{
		for (int32 Entry = 0; Entry + 3 < ShownCivic.Num(); Entry += 4)
		{
			if (ShownCivic[Entry + 1] >= 0 && ShownCivic[Entry + 1] <= 5)
			{
				PlotsTaken.FindOrAdd(ShownCivic[Entry]) |= 1 << ShownCivic[Entry + 1];
			}
		}
	}
	for (int32 Id = 0; Id < Works.Num() && Id < Data.Regions.Num(); ++Id)
	{
		const FIntVector4& Built = Works[Id];
		const int32 GoingUp = GoingIn.FindRef(Id);
		if (Built.X <= 0 && Built.Y <= 0 && Built.Z <= 0 && Built.W <= 0 && GoingUp == 0)
		{
			continue;
		}
		const FCampaignRegion& Region = Data.Regions[Id];
		const FVector2D Middle = Region.Center;
		// Where things already stand round a town: the army's flag to the north-east (or north-west), the air
		// fleets to the south-west and south-east. The works take the gaps: north, south, west and east.
		// (The town is larger than it was, by its tier: its works stand outside its last roofs, as far beyond them as
		// they used to stand beyond those of a town of the first size.)
		const double Grown = TownSpread(Id), Roofs = TownRadius(Id);
		auto Place = [this, &Middle, Id, Roofs](const FVector2D& Offset, FVector2D& OutSpot) -> bool
		{
			const double Beyond = FMath::Max(Offset.Size() - 6.4, 2.5);
			// (its own side of the town first; then, for a town that stands by the sea or a frontier, round to another)
			for (const double Round : { 0.0, 40.0, -40.0, 80.0, -80.0, 125.0, -125.0, 180.0 })
			{
				const FVector2D Way = Offset.GetSafeNormal().GetRotated(static_cast<float>(Round));
				for (const double Share : { 1.0, 0.65, 0.35 })
				{
					OutSpot = Middle + Way * (Roofs + Beyond * Share);
					if (Data.FindRegionAt(OutSpot.X, OutSpot.Y) == Id && GroundAt(OutSpot.X, OutSpot.Y) > 0.0f)
					{
						return true;
					}
				}
			}
			return false; // no room in this province: leave it off the table
		};
		// A shipyard wants the water in front of it: the nearest stretch of the province's own coast that is clear of the pieces.
		auto PlaceOnCoast = [this, &Middle, Id, Grown, Roofs](FVector2D& OutSpot) -> bool
		{
			static const FVector2D Taken[] = { FVector2D(9.0, -7.0), FVector2D(-9.0, -7.0), FVector2D(-10.0, 8.0), FVector2D(10.0, 9.0), FVector2D(0.0, -9.0), FVector2D(-10.0, 13.0), FVector2D(-9.5, 0.5) };
			for (double Reach = Roofs + 1.5; Reach <= Roofs + 34.0; Reach += 1.5)
			{
				for (int32 Step = 0; Step < 24; ++Step)
				{
					const FVector2D Way(FMath::Cos(Step * PI / 12.0), FMath::Sin(Step * PI / 12.0));
					const FVector2D Offset = Way * Reach;
					const FVector2D Spot = Middle + Offset, Beyond = Middle + Way * (Reach + 3.5);
					if (Data.FindRegionAt(Spot.X, Spot.Y) != Id || GroundAt(Spot.X, Spot.Y) <= 0.0f
						|| Data.FindRegionAt(Beyond.X, Beyond.Y) != INDEX_NONE || GroundAt(Beyond.X, Beyond.Y) > 0.0f)
					{
						continue;
					}
					bool bClear = true;
					for (const FVector2D& Other : Taken)
					{
						bClear &= FVector2D::Distance(Offset, Other * Grown) > 6.0;
					}
					if (bClear)
					{
						// (down to the water's edge: a slipway runs into the sea, it does not stand in a field)
						FVector2D Shore = Spot;
						for (double Further = 0.25; Further <= 3.75; Further += 0.25)
						{
							const FVector2D Next = Spot + Way * Further;
							if (GroundAt(Next.X, Next.Y) <= 0.03f || Data.FindRegionAt(Next.X, Next.Y) != Id)
							{
								break;
							}
							Shore = Next;
						}
						OutSpot = Shore - Way * 1.1;
						return true;
					}
				}
			}
			return false;
		};
		// Which way a bearing on the map points on the table, in degrees, for turning a model to face it.
		auto Facing = [this, &Middle](const FVector2D& Spot) -> double
		{
			const FVector Way = MapToWorld(Spot.X, Spot.Y, 0.0) - MapToWorld(Middle.X, Middle.Y, 0.0);
			return FMath::RadiansToDegrees(FMath::Atan2(Way.Y, Way.X));
		};
		auto Ahead = [](double Turn, double Along, double Across) -> FVector
		{
			const double R = FMath::DegreesToRadians(Turn);
			return FVector(FMath::Cos(R) * Along - FMath::Sin(R) * Across, FMath::Sin(R) * Along + FMath::Cos(R) * Across, 0.0);
		};

		FVector2D Spot;
		// Something going up (a factory, a shipyard, guns, an airfield or fortifications on order): one builder's
		// yard for the province, whatever and however much is on order. A wall half built inside a cage of scaffold
		// poles, a derrick over it, and timber and brick stacked beside.
		if (GoingUp != 0 && Place(FVector2D(1.5, 10.0), Spot))
		{
			const FVector At = SurfacePoint(Spot.X, Spot.Y, 0.02);
			const double Turn = Facing(Spot) + 90.0;
			const FLinearColor Trodden(FColor::FromHex(TEXT("7A6B52"))), NewBrick(FColor::FromHex(TEXT("9A5E48"))), Pole(FColor::FromHex(TEXT("A58A5E")));
			auto On = [&](double Along, double Across, double Up) { return At + Ahead(Turn, Along * Unit, Across * Unit) + FVector(0.0, 0.0, Up * Unit); };
			Model.Box(On(0.0, 0.0, 0.012), FVector(1.9, 1.5, 0.024) * Unit, Trodden, Turn);
			// (the walls, as far as they have got: one long, one short, one only begun)
			Model.Box(On(-0.2, -0.42, 0.24), FVector(1.1, 0.07, 0.48) * Unit, NewBrick, Turn);
			Model.Box(On(-0.72, -0.05, 0.16), FVector(0.07, 0.8, 0.32) * Unit, NewBrick, Turn);
			Model.Box(On(0.1, 0.33, 0.07), FVector(0.7, 0.07, 0.14) * Unit, NewBrick, Turn);
			// (the scaffold: four poles a side, with two rows of ledgers and a few boards)
			for (int32 K = 0; K < 4; ++K)
			{
				const double Along = -0.75 + K * 0.37;
				Model.Rod(On(Along, -0.55, 0.0), On(Along, -0.55, 0.86), 0.02 * Unit, Pole, 4);
				Model.Rod(On(Along, -0.31, 0.0), On(Along, -0.31, 0.86), 0.02 * Unit, Pole, 4);
				for (const double High : { 0.34, 0.66 })
				{
					Model.Rod(On(Along, -0.55, High), On(Along, -0.31, High), 0.014 * Unit, Pole, 4);
				}
			}
			for (const double High : { 0.34, 0.66 })
			{
				Model.Rod(On(-0.75, -0.55, High), On(0.36, -0.55, High), 0.014 * Unit, Pole, 4);
				Model.Box(On(-0.2, -0.43, High + 0.012), FVector(1.05, 0.2, 0.016) * Unit, Timber, Turn);
			}
			// (the derrick: a mast, a jib and its fall)
			Model.Rod(On(0.45, 0.05, 0.0), On(0.45, 0.05, 1.15), 0.028 * Unit, Timber, 5);
			Model.Rod(On(0.45, 0.05, 0.95), On(-0.25, -0.3, 1.3), 0.02 * Unit, Timber, 5);
			Model.Rod(On(-0.25, -0.3, 1.3), On(-0.25, -0.3, 0.72), 0.007 * Unit, Girder, 4);
			Model.Box(On(-0.25, -0.3, 0.68), FVector(0.14, 0.14, 0.08) * Unit, NewBrick, Turn);
			// (stacked timber, a pile of brick, a heap of sand)
			for (int32 K = 0; K < 3; ++K)
			{
				Model.Box(On(0.55, -0.5 + 0.03 * K, 0.03 + 0.05 * K), FVector(0.6, 0.2 - 0.04 * K, 0.05) * Unit, K % 2 == 0 ? Pole : Timber, Turn + 4.0 * K);
			}
			Model.Box(On(0.62, 0.5, 0.07), FVector(0.3, 0.24, 0.14) * Unit, NewBrick, Turn + 12.0);
			Model.Pillar(On(0.05, 0.6, 0.0), 0.2 * Unit, 0.03 * Unit, 0.13 * Unit, Sandbag, 8);
			++Count;
		}
		// Factories built since the war began: a works with a saw-tooth roof and tall chimneys, one bay for each.
		if (Built.X > 0 && Place(FVector2D(0.0, -9.0), Spot))
		{
			const FVector At = SurfacePoint(Spot.X, Spot.Y, 0.02);
			const double Turn = Facing(Spot) + 90.0;
			const int32 Bays = FMath::Clamp(Built.X, 1, 4);
			for (int32 Bay = 0; Bay < Bays; ++Bay)
			{
				const FVector Foot = At + Ahead(Turn, 0.0, (Bay - (Bays - 1) * 0.5) * 0.72 * Unit);
				Model.Box(Foot + FVector(0.0, 0.0, 0.26 * Unit), FVector(1.9, 0.7, 0.52) * Unit, Brick, Turn);
				// The saw-tooth: a row of north lights along the roof.
				for (int32 Tooth = 0; Tooth < 4; ++Tooth)
				{
					const FVector Ridge = Foot + Ahead(Turn, (Tooth - 1.5) * 0.46 * Unit, 0.0) + FVector(0.0, 0.0, 0.6 * Unit);
					Model.Box(Ridge, FVector(0.34, 0.7, 0.05) * Unit, Soot, Turn, 28.0);
					Model.Box(Ridge + Ahead(Turn, 0.17 * Unit, 0.0) + FVector(0.0, 0.0, 0.0), FVector(0.03, 0.66, 0.2) * Unit, Pane, Turn);
				}
			}
			for (int32 Stack = 0; Stack < FMath::Min(Bays + 1, 3); ++Stack)
			{
				const FVector Foot = At + Ahead(Turn, -1.15 * Unit, (Stack - 1) * 0.5 * Unit);
				Model.Pillar(Foot, 0.12 * Unit, 0.08 * Unit, 1.7 * Unit, Brick, 8);
				Model.Pillar(Foot + FVector(0.0, 0.0, 1.7 * Unit), 0.09 * Unit, 0.09 * Unit, 0.06 * Unit, Soot, 8);
				WorksChimneys.Add(Foot + FVector(0.0, 0.0, 1.76 * Unit));
				WorksChimneyRegion.Add(FIntPoint(Id, Stack));
			}
			++Count;
		}
		// A shipyard: slipways running down to the water, a hull on each, and a gantry crane over them.
		if (Built.Y > 0 && (PlaceOnCoast(Spot) || Place(FVector2D(10.0, 1.0), Spot)))
		{
			const FVector At = SurfacePoint(Spot.X, Spot.Y, 0.02);
			const double Turn = Facing(Spot);
			const int32 Slips = FMath::Clamp(Built.Y, 1, 3);
			for (int32 Slip = 0; Slip < Slips; ++Slip)
			{
				const FVector Foot = At + Ahead(Turn, 0.0, (Slip - (Slips - 1) * 0.5) * 0.62 * Unit);
				Model.Box(Foot + FVector(0.0, 0.0, 0.05 * Unit), FVector(2.4, 0.46, 0.1) * Unit, Apron, Turn);
				// The hull on the slip, in red lead, stern to the water.
				Model.Box(Foot + FVector(0.0, 0.0, 0.25 * Unit), FVector(1.7, 0.3, 0.3) * Unit, RedLead, Turn);
				Model.Box(Foot + Ahead(Turn, -1.0 * Unit, 0.0) + FVector(0.0, 0.0, 0.25 * Unit), FVector(0.4, 0.2, 0.3) * Unit, RedLead, Turn + 0.0);
				Model.Box(Foot + Ahead(Turn, 0.2 * Unit, 0.0) + FVector(0.0, 0.0, 0.47 * Unit), FVector(0.5, 0.22, 0.14) * Unit, Girder, Turn);
			}
			// The gantry: two legs either side, a beam across, and a jib.
			const double Half = (Slips * 0.62 + 0.3) * 0.5 * Unit;
			for (const double Side : { -1.0, 1.0 })
			{
				Model.Box(At + Ahead(Turn, -0.2 * Unit, Side * Half) + FVector(0.0, 0.0, 0.6 * Unit), FVector(0.1, 0.1, 1.2) * Unit, Girder, Turn);
			}
			Model.Box(At + Ahead(Turn, -0.2 * Unit, 0.0) + FVector(0.0, 0.0, 1.22 * Unit), FVector(0.14, 2.0 * Half / Unit + 0.2, 0.1) * Unit, Girder, Turn);
			Model.Rod(At + Ahead(Turn, -0.2 * Unit, 0.0) + FVector(0.0, 0.0, 1.27 * Unit), At + Ahead(Turn, 0.9 * Unit, 0.0) + FVector(0.0, 0.0, 1.7 * Unit), 0.035 * Unit, Girder);
			Model.House(At + Ahead(Turn, -1.7 * Unit, 0.0), 1.2 * Unit, 0.6 * Unit, 0.36 * Unit, 0.16 * Unit, Brick, Soot, Turn + 90.0);
			++Count;
		}
		// Anti-aircraft guns: a sandbagged pit for each level, its barrel up at the sky.
		if (Built.Z > 0)
		{
			// (Ruling 125; the art director's guard-the-line brief, section 4.) THE GUNS BY THE JUNCTION: the heavy
			// anti-aircraft gun, a long barrel raised 70 degrees on a cross-shaped platform in a low ring of sandbags.
			// One a level, up to three, in a row beside the track where it enters the town, on open ground clear of the
			// houses. The ring is as wide as a wagon is long (a wagon is a unit and a half) and the barrel stands as tall
			// as the town's spire. A province with guns and no railway keeps them at the town's edge.
			const int32 Guns = FMath::Clamp(Built.Z, 1, 3);
			const double Ring = 0.75; // (half a wagon, in the map's units)
			TArray<FVector> Feet;
			double Turn = 0.0;
			{
				FVector2D FieldAt;
				const bool bField = Built.W > 0 && Place(FVector2D(-10.0, 13.0), FieldAt);
				const FVector2D Base = WorldToMap(ArmySpot(Id, 0));
				TArray<FVector2D> Row;
				double RowScore = -1.0e9;
				FVector2D RowWay = FVector2D::ZeroVector;
				for (const FLandRail& Rail : Land.Rails)
				{
					if (Rail.A != Id && Rail.B != Id)
					{
						continue;
					}
					const double Length = FVector2D::Distance(Middle, Rail.At);
					const double Open = Length - Roofs - 0.9 - 2.0 * Ring - 0.6; // (the track the row may run along)
					if (Open < 0.0)
					{
						continue;
					}
					// (a ring apart where the track is long enough; closer on a short one, but never touching)
					const double Apart = FMath::Clamp(Open / FMath::Max(Guns - 1, 1), 2.3 * Ring, 4.0 * Ring);
					if ((Guns - 1) * Apart > Open + 0.01)
					{
						continue;
					}
					const FVector2D Way = (Rail.At - Middle) / Length, Side(Way.Y, -Way.X);
					for (const double Hand : { 1.0, -1.0 })
					{
						TArray<FVector2D> Tried;
						double Room = 1.0e9;
						for (int32 Pit = 0; Pit < Guns; ++Pit)
						{
							const FVector2D Stand = Middle + Way * (Roofs + 0.9 + Ring + Pit * Apart) + Side * (Hand * (Ring + 0.8));
							if (Data.FindRegionAt(Stand.X, Stand.Y) != Id || GroundAt(Stand.X, Stand.Y) <= 0.0f)
							{
								break;
							}
							Room = FMath::Min(Room, FVector2D::Distance(Stand, Base) - 9.0);
							if (bField)
							{
								Room = FMath::Min(Room, FVector2D::Distance(Stand, FieldAt) - 6.5);
							}
							Tried.Add(Stand);
						}
						// (a row clear of the army's base and the airfield before one that is not; then the longer line)
						const double Score = (Room >= 0.0 ? 1000.0 : 10.0 * Room) + Length;
						if (Tried.Num() == Guns && Score > RowScore)
						{
							Row = Tried;
							RowScore = Score;
							RowWay = Way;
						}
					}
				}
				for (const FVector2D& Stand : Row)
				{
					Feet.Add(SurfacePoint(Stand.X, Stand.Y, 0.02));
				}
				if (Row.Num() > 0)
				{
					const FVector Along = MapToWorld(Middle.X + RowWay.X, Middle.Y + RowWay.Y, 0.0) - MapToWorld(Middle.X, Middle.Y, 0.0);
					Turn = FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X));
				}
				else if (Place(FVector2D(-9.5, 0.5), Spot))
				{
					// (no railway, or no room beside it: in a row at the town's edge, running out from the town)
					const FVector At = SurfacePoint(Spot.X, Spot.Y, 0.02);
					Turn = Facing(Spot) + 90.0;
					for (int32 Pit = 0; Pit < Guns; ++Pit)
					{
						Feet.Add(At + Ahead(Turn, 0.0, (Pit - (Guns - 1) * 0.5) * 0.86 * Unit));
					}
				}
				// (Release 2, the art director's flak brief of 24 November 2026, section 2a; tc.Look.FlakRing.) A RING,
				// NOT A ROW. Three pits a level on a ring round the roofs, so that a level is read as how ringed the town
				// is: 3, 6, 9. The first pit keeps today's place by the track; the rest stand at even bearings from it,
				// at the radius today's row starts at (the roofs, plus 0.9, plus a ring). A stand is refused by today's
				// own tests (off the province, in water, on the army's base, on the airfield) and where it would lie on
				// a railway, on the road down to a river crossing, on a river, on another pit or on the town's other
				// works; a refused stand slides a little either way and is then dropped. The pit itself is as it was.
				// (The art director's rulings of 25 November 2026, 03:06Z and 03:21Z, on a town that cannot seat its
				// ring. Brian saw no guns round his cities, and one pit by the track is the picture he did not see: so
				// a town falls back to today's row only when it has nowhere else to stand a gun. See THE LIGHTER RING
				// below. And a pit makes way for a public building: see THE GUNS MAKE WAY.)
				if (bFlakRing && Feet.Num() > 0)
				{
					const FVector2D FirstStand = Row.Num() > 0 ? Row[0] : WorldToMap(Feet[0]);
					const double RingOut = Roofs + 0.9 + Ring;
					const double FirstBearing = FMath::Atan2(FirstStand.Y - Middle.Y, FirstStand.X - Middle.X);
					const int32 Asked = 3 * Guns;
					// (what a pit keeps off: a line and how far from it, a place and how far from it)
					struct FKeptLine
					{
						FVector2D From, To;
						double Off;
					};
					TArray<FKeptLine, TInlineAllocator<24>> KeptLines;
					TArray<TPair<FVector2D, double>, TInlineAllocator<8>> KeptPlaces;
					for (const FLandRail& Track : Land.Rails)
					{
						if (Track.A == Id || Track.B == Id)
						{
							KeptLines.Add(FKeptLine{ Middle, Track.At, Ring + 0.75 }); // (today's row stands a ring and 0.8 off the rails)
						}
					}
					for (const FLandCrossing& Bridge : Land.Crossings)
					{
						if ((Bridge.A == Id || Bridge.B == Id) && FVector2D::Distance(Bridge.At, Middle) > 0.01)
						{
							KeptLines.Add(FKeptLine{ Middle, Bridge.At, Ring + 0.5 });
						}
					}
					if (RiverPaths.Num() > 0)
					{
						// (The mend of 25 November 2026.) The rivers AS THEY ARE DRAWN: the smooth curve the ribbon is laid
						// along, which leaves the straight line between two points of a course by a unit or more at a bend,
						// so that a pit kept off the straight line could still stand in the water. A pit keeps off the
						// curve by its own ring, the ribbon's half breadth THERE as BuildWaterways laid it (a river of
						// the wider chart wanders in breadth, flares where it joins another and widens to twice and
						// more where it meets the sea) and the bank beside it.
						for (int32 RiverNo = 0; RiverNo < RiverPaths.Num(); ++RiverNo)
						{
							const TArray<FVector2D>& Drawn = RiverPaths[RiverNo];
							const TArray<double>* DrawnHalf = RiverHalf.IsValidIndex(RiverNo) ? &RiverHalf[RiverNo] : nullptr;
							const double Broadest = 1.4 * (RiverBroad.IsValidIndex(RiverNo) ? RiverBroad[RiverNo] : 0.425);
							for (int32 Bend = 0; Bend + 1 < Drawn.Num(); ++Bend)
							{
								// (a step of the curve is two units or so, and the broadest water is the Amazon's mouth, under
								// fifteen from its middle: a step nowhere near the town is passed by cheaply)
								if (FVector2D::DistSquared(Drawn[Bend], Middle) >= FMath::Square(RingOut + 24.0))
								{
									continue;
								}
								const double HalfHere = DrawnHalf && DrawnHalf->IsValidIndex(Bend + 1) ? FMath::Max((*DrawnHalf)[Bend], (*DrawnHalf)[Bend + 1]) : Broadest;
								const double Banks = Ring + HalfHere + 0.3;
								if (FMath::PointDistToSegment(FVector(Middle, 0.0), FVector(Drawn[Bend], 0.0), FVector(Drawn[Bend + 1], 0.0)) < RingOut + Banks)
								{
									KeptLines.Add(FKeptLine{ Drawn[Bend], Drawn[Bend + 1], Banks });
								}
							}
						}
					}
					else
					{
						for (const FLandRiver& River : Land.Rivers)
						{
							for (int32 Bend = 0; Bend + 1 < River.Course.Num(); ++Bend)
							{
								// (no ribbons laid yet: a river is a map unit and a quarter across at its broadest, and it wanders about its course)
								if (FMath::PointDistToSegment(FVector(Middle, 0.0), FVector(River.Course[Bend], 0.0), FVector(River.Course[Bend + 1], 0.0)) < RingOut + Ring + 1.5)
								{
									KeptLines.Add(FKeptLine{ River.Course[Bend], River.Course[Bend + 1], Ring + 1.5 });
								}
							}
						}
					}
					KeptPlaces.Emplace(Base, 9.0);
					if (bField)
					{
						KeptPlaces.Emplace(FieldAt, 6.5);
					}
					{
						// (the town's other works, where they stand: the builder's yard, the factory, the shipyard)
						FVector2D WorksAt;
						if (GoingUp != 0 && Place(FVector2D(1.5, 10.0), WorksAt))
						{
							KeptPlaces.Emplace(WorksAt, 2.6 + Ring);
						}
						if (Built.X > 0 && Place(FVector2D(0.0, -9.0), WorksAt))
						{
							KeptPlaces.Emplace(WorksAt, 3.0 + Ring);
						}
						// (the mend: a shipyard with no coast of its own to stand on is built east of the town, and is kept off there too)
						if (Built.Y > 0 && (PlaceOnCoast(WorksAt) || Place(FVector2D(10.0, 1.0), WorksAt)))
						{
							KeptPlaces.Emplace(WorksAt, 4.0 + Ring);
						}
					}
					// (The mend.) THE TOWN'S PUBLIC BUILDINGS: the market, the hall, the school and the rest stand on six
					// plots of a quarter of their own (ShowCivic's, the same numbers), the nearest of them against the
					// town's last roofs, where the ring runs. A pit keeps off each plot that is built on or going up.
					// Not off an empty one: no town has a public building when the war of 1939 opens, and the six
					// together close sixty to a hundred degrees of a ring. A plot's paving reaches about a size and a
					// half from its middle at its corners, and the bank and the domed exchange of a market raised to its
					// second and third levels stand beyond the paving, to almost two sizes: a pit keeps two sizes off,
					// whatever the level, so that it need not move again when the building is raised.
					TArray<TPair<FVector2D, double>, TInlineAllocator<6>> PlotPlaces;
					const int32 PlotBits = PlotsTaken.FindRef(Id);
					if (PlotBits != 0)
					{
						static const FVector2D QuarterPlots[] = { FVector2D(1.0, 0.0), FVector2D(16.0, 7.2), FVector2D(16.0, 0.0), FVector2D(8.5, 7.2), FVector2D(1.0, 7.2), FVector2D(8.5, 0.0) };
						const double QuarterNear = CivicNear(Id), QuarterTurn = FMath::DegreesToRadians(CivicTurn(Id));
						const double PlotSize = Region.bCapital ? 2.9 : (Region.Terrain == TEXT("urban") ? 2.7 : 2.4);
						for (int32 PlotLine = 0; PlotLine < 6; ++PlotLine)
						{
							if ((PlotBits & (1 << PlotLine)) != 0)
							{
								const FVector2D PlotOut(QuarterPlots[PlotLine].X, QuarterNear + QuarterPlots[PlotLine].Y);
								PlotPlaces.Emplace(Middle + FVector2D(PlotOut.X * FMath::Cos(QuarterTurn) - PlotOut.Y * FMath::Sin(QuarterTurn), PlotOut.X * FMath::Sin(QuarterTurn) + PlotOut.Y * FMath::Cos(QuarterTurn)), 2.0 * PlotSize + Ring);
							}
						}
					}
					auto OffThePlots = [&](const FVector2D& Asking) -> bool
					{
						for (const TPair<FVector2D, double>& Paved : PlotPlaces)
						{
							if (FVector2D::Distance(Asking, Paved.Key) < Paved.Value)
							{
								return false;
							}
						}
						return true;
					};
					// (whether a pit may stand at a place on the ring, by everything but the other pits and the public plots)
					auto StandOpen = [&](const FVector2D& Asking) -> bool
					{
						bool bFree = Data.FindRegionAt(Asking.X, Asking.Y) == Id && GroundAt(Asking.X, Asking.Y) > 0.0f;
						for (int32 KeptNo = 0; bFree && KeptNo < KeptPlaces.Num(); ++KeptNo)
						{
							bFree = FVector2D::Distance(Asking, KeptPlaces[KeptNo].Key) >= KeptPlaces[KeptNo].Value;
						}
						for (int32 KeptNo = 0; bFree && KeptNo < KeptLines.Num(); ++KeptNo)
						{
							bFree = FMath::PointDistToSegment(FVector(Asking, 0.0), FVector(KeptLines[KeptNo].From, 0.0), FVector(KeptLines[KeptNo].To, 0.0)) >= KeptLines[KeptNo].Off;
						}
						return bFree;
					};
					TArray<FVector2D, TInlineAllocator<9>> Seated;
					// (the bearing each was seated at, from the town's middle, and the bearing its stand has before any slide)
					TArray<double, TInlineAllocator<9>> SeatedBears, SeatedHomes;
					Seated.Add(FirstStand);
					SeatedBears.Add(FirstBearing);
					SeatedHomes.Add(FirstBearing);
					// (a slide either way of a bearing, the nearest first: 0, 5, -5, 10, -10 degrees and so on)
					auto SlideOf = [](int32 Fifth) -> double
					{
						return FMath::DegreesToRadians(((Fifth + 1) / 2) * 5.0 * (Fifth % 2 == 1 ? 1.0 : -1.0));
					};
					// (whether a pit may stand at a bearing by the other pits: never touching, today's least, and no
					// nearer in bearing to one, the short way round, than is asked; the pit asking is left out)
					auto ClearOfPits = [&](double Bears, double LeastApart, int32 Asker) -> bool
					{
						const FVector2D Try = Middle + FVector2D(FMath::Cos(Bears), FMath::Sin(Bears)) * RingOut;
						for (int32 K = 0; K < Seated.Num(); ++K)
						{
							const double Between = Bears - SeatedBears[K];
							if (K != Asker && (FVector2D::Distance(Try, Seated[K]) < 2.3 * Ring
								|| FMath::Abs(FMath::Atan2(FMath::Sin(Between), FMath::Cos(Between))) < LeastApart - 1.0e-6))
							{
								return false;
							}
						}
						return true;
					};
					// (seats a stand: at its own bearing if it may stand there, or the nearest slide from it that may)
					auto SeatStand = [&](double Home, int32 Slides, double LeastApart) -> bool
					{
						for (int32 Slid = 0; Slid <= Slides; ++Slid)
						{
							const double Bears = Home + SlideOf(Slid);
							const FVector2D Try = Middle + FVector2D(FMath::Cos(Bears), FMath::Sin(Bears)) * RingOut;
							if (StandOpen(Try) && ClearOfPits(Bears, LeastApart, INDEX_NONE))
							{
								Seated.Add(Try);
								SeatedBears.Add(Bears);
								SeatedHomes.Add(Home);
								return true;
							}
						}
						return false;
					};
					// (how far a stand of this ring may slide, in steps of five degrees, and how near in bearing two of its
					// pits may end: kept for a pit that later makes way)
					int32 RingSlides = 8;
					double RingApart = 0.0;
					bool bLighter = Guns == 1;
					if (!bLighter)
					{
						// (levels 2 and 3 as they were built: six or nine stands, each sliding up to twenty degrees; their
						// pits are sixty and forty degrees apart, and a wider slide would run them into their neighbours)
						for (int32 Next = 1; Next < Asked; ++Next)
						{
							SeatStand(FirstBearing + 2.0 * PI * Next / Asked, RingSlides, RingApart);
						}
						bLighter = Seated.Num() < 3; // (it cannot seat three: it follows level 1's rule for three)
					}
					if (bLighter)
					{
						// THE LIGHTER RING (the ruling of 03:06Z). Three stands, 120 degrees apart. A stand may slide up to
						// forty degrees either way, the nearest bearing first, but must end at least sixty degrees of bearing
						// from every other seated pit: three pits may sit unevenly, they may never bunch on one side as a
						// battery.
						// (Each stand takes its nearest open bearing as it comes, and the first to be seated can take the
						// one place that leaves the other none. So where three are not seated the pass is made once more
						// with the far stand seated first, before the town is cut to two.)
						RingSlides = 16;
						RingApart = FMath::DegreesToRadians(60.0);
						for (int32 Pass = 0; Pass < 2 && Seated.Num() < 3; ++Pass)
						{
							Seated.SetNum(1);
							SeatedBears.SetNum(1);
							SeatedHomes.SetNum(1);
							for (int32 Next = 1; Next < 3; ++Next)
							{
								SeatStand(FirstBearing + 2.0 * PI * (Pass == 0 ? Next : 3 - Next) / 3.0, RingSlides, RingApart);
							}
						}
						if (Seated.Num() < 3 && Guns < 3)
						{
							// If one of the three cannot be seated, TWO: the first pit by the track and one more, as nearly
							// opposite it as there is room and never under ninety degrees from it. Guns on two sides of a
							// town still read as a defence round it. (Not at level 3, by her ruling of 03:21Z: a town
							// never shows fewer pits than its row would, and two there would read as guns lost.)
							Seated.SetNum(1);
							SeatedBears.SetNum(1);
							SeatedHomes.SetNum(1);
							RingSlides = 36;
							RingApart = FMath::DegreesToRadians(90.0);
							SeatStand(FirstBearing + PI, RingSlides, RingApart);
						}
					}
					// THE GUNS MAKE WAY (her ruling of 03:21Z). The ring above is seated as if the town had no public
					// buildings, so that it is the same ring whatever has been built since. Then a pit that stands on a
					// plot that is built on or going up makes way: it alone moves, to the nearest slide of its own stand
					// that is off the plots and clear of the other pits, which keep their places; with nowhere to go it
					// is taken away. The first pit by the track never moves (ruling 125): there the plot loses, as today.
					// (Every pit in the way is lifted before any is set down again, so that a pit is tried against the
					// others where they will stand, and never against a place another pit is about to leave.)
					// (Whether the ground alone seated a ring, before any pit made way: the art director, 03:39Z, has a
					// town that loses its ring to a public building counted apart from one whose ground never seated it.)
					const bool bRingedBefore = Seated.Num() >= 3 || (Guns < 3 && Seated.Num() == 2);
					TArray<double, TInlineAllocator<9>> Displaced; // (the home bearings of the pits lifted, the last seated first)
					for (int32 InTheWay = Seated.Num() - 1; InTheWay >= 1; --InTheWay)
					{
						if (!OffThePlots(Seated[InTheWay]))
						{
							Displaced.Add(SeatedHomes[InTheWay]);
							Seated.RemoveAt(InTheWay);
							SeatedBears.RemoveAt(InTheWay);
							SeatedHomes.RemoveAt(InTheWay);
						}
					}
					for (int32 SetDown = Displaced.Num() - 1; SetDown >= 0; --SetDown)
					{
						// (in the order they were seated; a pit with no place left is not set down)
						bool bMadeWay = false;
						for (int32 Slid = 0; Slid <= RingSlides && !bMadeWay; ++Slid)
						{
							const double Bears = Displaced[SetDown] + SlideOf(Slid);
							const FVector2D Try = Middle + FVector2D(FMath::Cos(Bears), FMath::Sin(Bears)) * RingOut;
							if (StandOpen(Try) && OffThePlots(Try) && ClearOfPits(Bears, RingApart, INDEX_NONE))
							{
								Seated.Add(Try);
								SeatedBears.Add(Bears);
								SeatedHomes.Add(Displaced[SetDown]);
								bMadeWay = true;
							}
						}
					}
					// (whether the pits are two on two sides of the town: the second never under ninety degrees of
					// bearing from the first)
					auto OnTwoSides = [&]() -> bool
					{
						return Seated.Num() == 2
							&& FMath::Abs(FMath::Atan2(FMath::Sin(SeatedBears[1] - SeatedBears[0]), FMath::Cos(SeatedBears[1] - SeatedBears[0]))) >= FMath::DegreesToRadians(90.0) - 1.0e-6;
					};
					// TWO, BEFORE THE ROW (her ruling of 03:53Z, in place of 03:39Z's). Making way can leave a level 1
					// or 2 town with its first pit alone, or with two under ninety degrees apart. The row would move or
					// take away pits that were not in the way too, and show fewer guns: so the town first tries the
					// two-pit stand, the first pit where it is and one more as nearly opposite it as is open and off
					// the plots, sliding up to ninety degrees either way. Only if that fails does it fall back.
					if (Guns < 3 && Displaced.Num() > 0 && Seated.Num() < 3 && !OnTwoSides())
					{
						Seated.SetNum(1);
						SeatedBears.SetNum(1);
						SeatedHomes.SetNum(1);
						RingSlides = 36;
						RingApart = FMath::DegreesToRadians(90.0);
						const double Opposite = FirstBearing + PI;
						for (int32 Slid = 0; Slid <= RingSlides && Seated.Num() < 2; ++Slid)
						{
							const double Bears = Opposite + SlideOf(Slid);
							const FVector2D Try = Middle + FVector2D(FMath::Cos(Bears), FMath::Sin(Bears)) * RingOut;
							if (StandOpen(Try) && OffThePlots(Try) && ClearOfPits(Bears, RingApart, INDEX_NONE))
							{
								Seated.Add(Try);
								SeatedBears.Add(Bears);
								SeatedHomes.Add(Opposite);
							}
						}
					}
					// (a ring of three or more; at levels 1 and 2 two pits on two sides of the town, however the town
					// came to have two; else today's row: a town never shows fewer pits than its row would, and never
					// two bunched on one side)
					const bool bRinged = Seated.Num() >= 3 || (Guns < 3 && OnTwoSides());
					++RingTowns;
					RingAsked += Asked;
					if (Seated.Num() < 3 || 3 * Seated.Num() < 2 * Asked)
					{
						// (the art director, 25 November 2026: the log names any town that seats under two thirds of its pits,
						// one that seats only two, and one that falls back, in those words, so that they can be counted)
						UE_LOG(LogTheater, Display, TEXT("Flak ring: %s (level %d) seats %d of the %d pits it asks%s"), *Region.Name, Guns, Seated.Num(), Asked,
							!bRinged ? (bRingedBefore ? TEXT(" and falls back to the row, its pits having made way for a public building") : TEXT(" and falls back to the row"))
								: Seated.Num() == 2 ? TEXT(", on two sides of the town") : TEXT(""));
					}
					if (bRinged)
					{
						RingSeated += Seated.Num();
						RingTwos += Seated.Num() == 2 ? 1 : 0;
						Feet.Reset();
						for (const FVector2D& Seat : Seated)
						{
							Feet.Add(SurfacePoint(Seat.X, Seat.Y, 0.02));
						}
					}
					else
					{
						++RingRows; // (it keeps today's row)
						RingPlots += bRingedBefore ? 1 : 0; // (and of those, the towns whose ground did seat a ring)
					}
				}
				else if (bFlakRing)
				{
					// (the mend: a town with guns built and no ground for today's row shows none, and the log says so)
					++RingTowns;
					RingAsked += 3 * Guns;
					++RingNone;
					UE_LOG(LogTheater, Display, TEXT("Flak ring: %s (level %d) has no ground for its row, and no gun is drawn"), *Region.Name, Guns);
				}
			}
			const double Rim = Ring * UnitScale; // (the ring's radius on the table)
			for (int32 Pit = 0; Pit < Feet.Num(); ++Pit)
			{
				const FVector& Foot = Feet[Pit];
				Model.Pillar(Foot, Rim, 0.92 * Rim, 0.11 * Unit, Sandbag, 12);
				Model.Pillar(Foot + FVector(0.0, 0.0, 0.11 * Unit), 0.76 * Rim, 0.76 * Rim, 0.008 * Unit, Soot, 12);
				// the cross-shaped platform, its arms across the pit
				Model.Box(Foot + FVector(0.0, 0.0, 0.135 * Unit), FVector(1.3 * Rim, 0.26 * Rim, 0.035 * Unit), GunPaint, Turn + 45.0);
				Model.Box(Foot + FVector(0.0, 0.0, 0.135 * Unit), FVector(0.26 * Rim, 1.3 * Rim, 0.035 * Unit), GunPaint, Turn + 45.0);
				Model.Pillar(Foot + FVector(0.0, 0.0, 0.12 * Unit), 0.075 * Unit, 0.055 * Unit, 0.2 * Unit, GunPaint, 8);
				const FVector Breech = Foot + FVector(0.0, 0.0, 0.33 * Unit);
				const double Bearing = Turn + Pit * 37.0, Raised = FMath::DegreesToRadians(70.0);
				const FVector Bore = Ahead(Bearing, FMath::Cos(Raised), 0.0) + FVector(0.0, 0.0, FMath::Sin(Raised));
				Model.Rod(Breech - Bore * 0.1 * Unit, Breech + Bore * 0.82 * Unit, 0.036 * Unit, GunPaint);
				Model.Box(Breech, FVector(0.2, 0.13, 0.12) * Unit, GunPaint, Bearing);
				// (release 2, the flak brief, 2b: where its barrel ends, for its flash and its puff of smoke)
				FFlakGun& Gun = FlakGuns.FindOrAdd(Id).AddDefaulted_GetRef();
				Gun.Muzzle = Breech + Bore * 0.82 * Unit;
				Gun.Bore = Bore;
			}
			Count += Feet.Num() > 0 ? 1 : 0;
		}
		// An airfield, by its size: a landing ground is a mown strip with a hut and a windsock; an airfield has a
		// hard runway and hangars; an air base has crossing runways, a row of hangars and a control tower; and
		// the greatest has three runways in a triangle.
		if (Built.W > 0 && Place(FVector2D(-10.0, 13.0), Spot))
		{
			AirfieldSpots.Add(Id, Spot);
			// (an airfield is a big place, and the player has to find it; but one with nobody flying from it is
			// built smaller, so that it is not a greater thing than the town beside it)
			const double Field = Unit * 1.3 * (bAirfieldsKnown && !AirfieldsUsed.Contains(Id) ? 0.62 : 1.0);
			const int32 Size = FMath::Clamp(Built.W, 1, 4);
			const FVector At = SurfacePoint(Spot.X, Spot.Y, 0.02);
			const double Turn = Facing(Spot) + 90.0;
			const double Long = 2.9 + 0.45 * Size, Wide = 1.5 + 0.3 * Size;
			AirfieldLie.Add(Id, FVector2D(Turn, Size));
			// The field is a slab thick enough to stand clear of the roll of the ground under it. (Art fix 8: it is
			// mown grass, a little lighter than the fields round it and no lime sticker: darker than it was, with
			// the nation's stain let through, and with a rounded, uneven edge in place of a ruled square. Its
			// runways are pale concrete, not white.)
			FLinearColor Grass = Turf * 0.6f;
			if (const FCampaignNation* Holder = Data.FindNation(Region.Owner))
			{
				Grass = FMath::Lerp(Grass, FLinearColor(Holder->Color) * 0.42f, 0.3f);
			}
			{
				TArray<FVector2D> Edge;
				FRandomStream Ragged(Id * 131 + 7);
				for (int32 K = 0; K < 18; ++K)
				{
					const double Angle = K * UE_DOUBLE_TWO_PI / 18.0, C = FMath::Cos(Angle), S = FMath::Sin(Angle);
					const double Uneven = 1.0 + Ragged.FRandRange(-0.07f, 0.07f);
					Edge.Add(FVector2D(FMath::Sign(C) * FMath::Pow(FMath::Abs(C), 0.55) * Long * 0.53, FMath::Sign(S) * FMath::Pow(FMath::Abs(S), 0.55) * Wide * 0.55) * Uneven);
				}
				Model.Slab(At, Field, Turn, Edge, -0.08, 0.08, Grass);
			}
			const FLinearColor Runway(FColor::FromHex(TEXT("7E7C74")));
			const FLinearColor Strip = Size >= 2 ? Runway : FLinearColor(FColor::FromHex(TEXT("6C7650")));
			Model.Box(At + FVector(0.0, 0.0, 0.085 * Field), FVector(Long - 0.25, 0.3, 0.02) * Field, Strip, Turn);
			if (Size >= 3)
			{
				Model.Box(At + FVector(0.0, 0.0, 0.088 * Field), FVector(Wide * 1.5, 0.26, 0.02) * Field, Runway, Turn + 58.0);
			}
			if (Size >= 4)
			{
				Model.Box(At + FVector(0.0, 0.0, 0.091 * Field), FVector(Wide * 1.5, 0.26, 0.02) * Field, Runway, Turn - 58.0);
			}
			const int32 Hangars = Size;
			for (int32 Hangar = 0; Hangar < Hangars; ++Hangar)
			{
				const FVector Foot = At + Ahead(Turn, (Hangar - (Hangars - 1) * 0.5) * 0.85 * Field, (Wide * 0.5 - 0.3) * Field) + FVector(0.0, 0.0, 0.08 * Field);
				if (Size == 1)
				{
					Model.House(Foot, 0.42 * Field, 0.36 * Field, 0.16 * Field, 0.1 * Field, Timber, Soot, Turn + 90.0);
				}
				else
				{
					Model.House(Foot, 0.7 * Field, 0.55 * Field, 0.22 * Field, 0.14 * Field, Tin, Girder, Turn + 90.0);
				}
			}
			if (Size >= 3)
			{
				// The control tower: a square tower with a glazed watch room and a mast.
				const FVector Tower = At + Ahead(Turn, -(Long * 0.5 - 0.35) * Field, (Wide * 0.5 - 0.3) * Field) + FVector(0.0, 0.0, 0.08 * Field);
				Model.Box(Tower + FVector(0.0, 0.0, 0.22 * Field), FVector(0.3, 0.3, 0.44) * Field, Brick, Turn);
				Model.Box(Tower + FVector(0.0, 0.0, 0.5 * Field), FVector(0.38, 0.38, 0.14) * Field, Pane, Turn);
				Model.Box(Tower + FVector(0.0, 0.0, 0.585 * Field), FVector(0.42, 0.42, 0.03) * Field, Soot, Turn);
				Model.Pillar(Tower + FVector(0.0, 0.0, 0.6 * Field), 0.015 * Field, 0.01 * Field, 0.3 * Field, Girder, 5);
			}
			const FVector Pole = At + Ahead(Turn, (Long * 0.5 - 0.25) * Field, -(Wide * 0.5 - 0.2) * Field) + FVector(0.0, 0.0, 0.08 * Field);
			Model.Pillar(Pole, 0.02 * Field, 0.02 * Field, 0.5 * Field, Timber, 5);
			Model.Rod(Pole + FVector(0.0, 0.0, 0.5 * Field), Pole + Ahead(Turn, 0.26 * Field, 0.06 * Field) + FVector(0.0, 0.0, 0.46 * Field), 0.035 * Field, Orange);
			++Count;
		}
	}
	if (Model.Vertices.Num() > 0)
	{
		BuildingsMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		BuildingsMesh->SetMaterial(0, SceneryMaterial);
	}
	if (OverSeam())
	{
		// (The far side, seam-2, where it meets the flak ring.) The keeper of the twins over the chart's seam knows a
		// model by its counts and a few of its points. A gun pit that makes way for a public building moves a few
		// degrees and keeps every count (so does an airfield that shrinks when nobody flies from it): the keeper would
		// not see it, and the twin would show the pit still standing on the new market. So this model's twin is laid
		// again with the model, as RecolourRivers lays the rivers'.
		LayOverSeam(BuildingsMesh);
	}
	UE_LOG(LogTheater, Log, TEXT("Works on the table: %d, in %d points"), Count, Model.Vertices.Num());
	if (bFlakRing)
	{
		// (for the reader of a picture run: how many towns have guns, how many pits their levels ask and how many found a seat)
		// (the mend of 25 November 2026: and how many seat only two, fall back to the row, or have no ground for a gun at all)
		// (her ask of 03:39Z: and, of the towns that fall back, how many do so because pits made way for a public building)
		UE_LOG(LogTheater, Display, TEXT("Flak ring (tc.Look.FlakRing): %d towns with guns, %d pits asked, %d seated in rings, %d towns seat only two, %d towns fall back to the row (%d of them for a public building), %d towns have nowhere to stand a gun"),
			RingTowns, RingAsked, RingSeated, RingTwos, RingRows, RingPlots, RingNone);
	}
}

void ACampaignMapActor::SetRailLight(const FRailLight& Light)
{
	RailLight = Light;
	RailRouteTo = INDEX_NONE;
	RailRoute.Reset();
	bRailLightDirty = true;
	// The way back to the army from every province on a lit line, by the fewest lines: a station dot that must move
	// moves that way.
	RailBack.Reset();
	DotOut.Reset();
	if (RailLight.From != INDEX_NONE && RailLight.Tracks.Num() > 0)
	{
		TArray<int32> Queue;
		Queue.Add(RailLight.From);
		RailBack.Add(RailLight.From, INDEX_NONE);
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 Here = Queue[Head];
			for (const FIntPoint& Track : RailLight.Tracks)
			{
				const int32 Next = Track.X == Here ? Track.Y : Track.Y == Here ? Track.X : INDEX_NONE;
				if (Next != INDEX_NONE && !RailBack.Contains(Next))
				{
					RailBack.Add(Next, Here);
					Queue.Add(Next);
				}
			}
		}
	}
	if (RailLight.Dots.Num() > 0)
	{
		FString Named;
		for (const int32 RegionId : RailLight.Dots)
		{
			Named += (Named.IsEmpty() ? TEXT("") : TEXT(", ")) + (Data.Regions.IsValidIndex(RegionId) ? Data.Regions[RegionId].Name : FString(TEXT("?")));
		}
		UE_LOG(LogTheater, Log, TEXT("Rail light: %d tracks lit and %d station dots: %s"), RailLight.Tracks.Num(), RailLight.Dots.Num(), *Named);
	}
}

void ACampaignMapActor::SetRailRoute(int32 ToRegion)
{
	if (ToRegion == RailRouteTo)
	{
		return;
	}
	RailRouteTo = ToRegion;
	RailRoute.Reset();
	// The way a train would take: the fewest lines from where the army stands, over the lit tracks.
	if (ToRegion != INDEX_NONE && RailLight.From != INDEX_NONE && RailLight.Tracks.Num() > 0)
	{
		TMap<int32, int32> Came;
		TArray<int32> Queue;
		Queue.Add(RailLight.From);
		Came.Add(RailLight.From, INDEX_NONE);
		for (int32 Head = 0; Head < Queue.Num() && !Came.Contains(ToRegion); ++Head)
		{
			const int32 Here = Queue[Head];
			for (const FIntPoint& Track : RailLight.Tracks)
			{
				const int32 Next = Track.X == Here ? Track.Y : Track.Y == Here ? Track.X : INDEX_NONE;
				if (Next != INDEX_NONE && !Came.Contains(Next))
				{
					Came.Add(Next, Here);
					Queue.Add(Next);
				}
			}
		}
		for (int32 At = ToRegion; Came.Contains(At) && Came[At] != INDEX_NONE; At = Came[At])
		{
			RailRoute.Add(FIntPoint(FMath::Min(At, Came[At]), FMath::Max(At, Came[At])));
		}
	}
	bRailLightDirty = true;
}

void ACampaignMapActor::UpdateRailLight(float CameraDistance)
{
	THEATER_SPEED_SCOPE(UpdateRailLight);
	if (!RailLightMesh)
	{
		return;
	}
	if (RailLight.Tracks.Num() == 0 && RailLight.Tint.Num() == 0 && RailLight.Bars.Num() == 0)
	{
		if (RailLightMesh->GetNumSections() > 0)
		{
			RailLightMesh->ClearAllMeshSections();
		}
		RailLightBuiltFor = -1.0f;
		bRailLightDirty = false;
		return;
	}
	if (RailTintInk)
	{
		// (the tint is a help and never the signal: by night it does not show at all)
		RailTintInk->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(1.0f - 1.6f * GetMoonlight(), 0.0f, 1.0f));
	}
	// (The lines keep their width on the screen as the eye rises; close up they keep the width they have from the
	// lowest playing height, and lie along the model's own rails.)
	const float Seen = FMath::Clamp(CameraDistance, 2600.0f, 20000.0f);
	if (!bRailLightDirty && RailLightBuiltFor > 0.0f && FMath::Abs(Seen - RailLightBuiltFor) < 0.04f * RailLightBuiltFor)
	{
		return;
	}
	bRailLightDirty = false;
	RailLightBuiltFor = Seen;
	const double Pixel = 0.00037 * Seen / UnitScale; // (a pixel of the 1600 view, in the map's units)
	const bool bRoute = RailRoute.Num() > 0;
	auto Inked = [](const FLinearColor& Colour, float Alpha)
	{
		FLinearColor Out = Colour;
		Out.A = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return Out.ToFColor(false);
	};
	const FLinearColor Lit(FColor(198, 228, 255)), RouteLit(FColor(255, 253, 244)), Casing(FColor(18, 20, 24)), BarRed(FColor(226, 62, 44));
	const FLinearColor RouteRail(FColor(76, 96, 110)); // (laid at nine tenths on the route's white bed it reads about (92,110,122): the lit line's navy)
	FCampaignMeshBuffers Tints, Lines, Stops;
	// The faint cool tint, on the fields as the march wash is laid; the province under the pointer brightens.
	for (const int32 RegionId : RailLight.Tint)
	{
		FLinearColor Fill(FColor(120, 170, 235));
		Fill.A = 0.16f;
		AppendRegionFill(Tints, RegionId, Fill);
		if (RegionId == RailRouteTo)
		{
			FLinearColor Bright(FColor(190, 214, 240));
			Bright.A = 0.26f;
			AppendRegionFill(Tints, RegionId, Bright);
		}
	}
	// The lines, in three passes so that one line's glow never lies over another's rail: the glow, the dark casing,
	// then the rail and its sleepers.
	auto RailOf = [this](const FIntPoint& Track) -> const FLandRail*
	{
		for (const FLandRail& Rail : Land.Rails)
		{
			if ((Rail.A == Track.X && Rail.B == Track.Y) || (Rail.A == Track.Y && Rail.B == Track.X))
			{
				return Data.Regions.IsValidIndex(Rail.A) && Data.Regions.IsValidIndex(Rail.B) ? &Rail : nullptr;
			}
		}
		return nullptr;
	};
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		for (const FIntPoint& Track : RailLight.Tracks)
		{
			const FLandRail* Rail = RailOf(Track);
			if (!Rail)
			{
				continue;
			}
			const bool bOnRoute = RailRoute.Contains(Track);
			// (with a route shown the other lines fall to about seven tenths of their strength)
			const float Strength = bRoute && !bOnRoute ? 0.7f : 1.0f;
			const FLinearColor& Ink = bOnRoute ? RouteLit : Lit;
			const double Core = (bOnRoute ? 1.5 : 1.0) * Pixel, TickLong = (bOnRoute ? 4.0 : 3.2) * Pixel, GlowOut = (bOnRoute ? 14.0 : 10.0) * Pixel;
			const float Glow = (bOnRoute ? 0.55f : 0.30f) * Strength;
			const FVector2D Stops3[3] = { Data.Regions[Rail->A].Center, Rail->At, Data.Regions[Rail->B].Center };
			for (int32 Leg = 0; Leg < 2; ++Leg)
			{
				const FVector2D A = Stops3[Leg], B = Stops3[Leg + 1];
				const double Length = FVector2D::Distance(A, B);
				if (Length < 0.5)
				{
					continue;
				}
				const TArray<FVector2D> Way = { A, B };
				if (Pass == 0)
				{
					AppendRibbon(Lines, Way, Core + GlowOut, 0.2, Inked(Ink, 0.4f * Glow), false);
					AppendRibbon(Lines, Way, Core + 0.45 * GlowOut, 0.21, Inked(Ink, 0.6f * Glow), false);
				}
				else if (Pass == 1)
				{
					// (The art director's review of 18:06, 16 November: on the route under the pointer the two rails stay
					// the lit line's dark navy on a bed that is wider and whiter. They had faded to a pale grey there, and
					// the route read as a road beside the railway.)
					if (bOnRoute)
					{
						AppendRibbon(Lines, Way, 4.0 * Pixel, 0.22, Inked(RouteRail, 0.9f), false);
					}
					else
					{
						AppendRibbon(Lines, Way, 3.2 * Pixel, 0.22, Inked(Casing, 0.66f * Strength), false);
					}
				}
				else
				{
					AppendRibbon(Lines, Way, Core, 0.24, Inked(Ink, Strength), false);
					// (sleepers across it every eight pixels: without them and the casing it reads as a province's border)
					const FVector2D Dir = (B - A) / Length, Side(Dir.Y, -Dir.X);
					const int32 Ticks = FMath::Clamp(FMath::FloorToInt32(Length / (8.0 * Pixel)), 1, 400);
					for (int32 K = 0; K < Ticks; ++K)
					{
						const FVector2D At = A + Dir * (Length * (K + 0.5) / Ticks);
						const TArray<FVector2D> Sleeper = { At - Side * TickLong, At + Side * TickLong };
						AppendRibbon(Lines, Sleeper, 0.75 * Pixel, 0.25, Inked(Ink, Strength), false);
					}
				}
			}
		}
	}
	// Where a lit line reaches a cut: a red buffer bar across the track at that province's border.
	for (const FIntPoint& Bar : RailLight.Bars)
	{
		const FLandRail* Rail = RailOf(Bar);
		if (!Rail)
		{
			continue;
		}
		const FVector2D From = Data.Regions[Bar.X].Center;
		const FVector2D Dir = (Rail->At - From).GetSafeNormal(), Side(Dir.Y, -Dir.X);
		// (a step short of the border, on the side the train comes from)
		const FVector2D At = Rail->At - Dir * (2.0 * Pixel);
		const TArray<FVector2D> Across = { At - Side * (5.0 * Pixel), At + Side * (5.0 * Pixel) };
		AppendRibbon(Stops, Across, 2.9 * Pixel, 0.27, Inked(Casing, 0.8f), false);
		AppendRibbon(Stops, Across, 1.7 * Pixel, 0.28, Inked(BarRed, 1.0f), false);
	}
	RailLightMesh->ClearAllMeshSections();
	Tints.Commit(RailLightMesh, 0, RailTintInk ? static_cast<UMaterialInterface*>(RailTintInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
	Lines.Commit(RailLightMesh, 1, RailLightInk ? static_cast<UMaterialInterface*>(RailLightInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
	Stops.Commit(RailLightMesh, 2, RailLightInk ? static_cast<UMaterialInterface*>(RailLightInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
	RailLightMesh->SetCastShadow(false);
	RailLightMesh->SetTranslucentSortPriority(1); // (over every wash; under the cut lamps and the searchlights)
}

FVector2D ACampaignMapActor::CutSpot(int32 RegionId) const
{
	const FCampaignRegion& Region = Data.Regions[RegionId];
	const double Town = TownRadius(RegionId);
	FVector2D Best = Region.Center + FVector2D(-3.0, 3.0);
	double BestScore = -1.0e9;
	for (const FLandRail& Rail : Land.Rails)
	{
		if (Rail.A != RegionId && Rail.B != RegionId)
		{
			continue;
		}
		const double Length = FVector2D::Distance(Region.Center, Rail.At);
		if (Length - Town < 3.0)
		{
			continue; // (no open track on this line at all)
		}
		// (a stretch of under six units is taken only where no longer one is clear)
		const double Short = Length - Town < 6.0 ? -200.0 : 0.0;
		// What the sign must stand clear of: the armies' bases on both sides of the border this line runs to (a base
		// is some fourteen units across), and both provinces' airfields.
		const int32 Other = Rail.A == RegionId ? Rail.B : Rail.A;
		TArray<TPair<FVector2D, double>, TInlineAllocator<4>> Keep;
		for (const int32 Whose : { RegionId, Other })
		{
			if (Data.Regions.IsValidIndex(Whose))
			{
				Keep.Emplace(WorldToMap(ArmySpot(Whose, 0)), 11.0);
				if (const FVector2D* Field = AirfieldSpots.Find(Whose))
				{
					Keep.Emplace(*Field, 7.0);
				}
			}
		}
		// Halfway between the last house and the border first; then nearer the town, then nearer the border.
		for (const double Part : { 0.5, 0.36, 0.66, 0.24, 0.8 })
		{
			const FVector2D Spot = Region.Center + (Rail.At - Region.Center) / Length * (Town + Part * (Length - Town));
			double Room = 1.0e9;
			for (const TPair<FVector2D, double>& Thing : Keep)
			{
				Room = FMath::Min(Room, FVector2D::Distance(Spot, Thing.Key) - Thing.Value);
			}
			// (a clear place before any that is not; then the longer line; then the nearer to halfway)
			const double Score = (Room >= 0.0 ? 1000.0 : 6.0 * Room) + Short + Length - 24.0 * FMath::Abs(Part - 0.5);
			if (Score > BestScore)
			{
				Best = Spot;
				BestScore = Score;
			}
		}
	}
	return Best;
}

void ACampaignMapActor::ShowCuts(const TArray<FViewCut>& Cuts)
{
	THEATER_SPEED_SCOPE(ShowCuts);
	CutLamps.Reset();
	for (const FViewCut& Cut : Cuts)
	{
		if (Data.Regions.IsValidIndex(Cut.Region))
		{
			CutLamps.Emplace(Cut.Region, Cut.How != TEXT("now"));
		}
	}
}

void ACampaignMapActor::UpdateCutLamps(float CameraDistance)
{
	THEATER_SPEED_SCOPE(UpdateCutLamps);
	UWorld* World = GetWorld();
	if (!CutLampMesh || !World)
	{
		return;
	}
	const bool bAgentSigns = AgentStands.Num() > 0 && !bAgentFigures && bPiecesShown; // (from high up each agent is a sign)
	if ((CutLamps.Num() == 0 && RailLight.Dots.Num() == 0 && RailLight.Tracks.Num() == 0 && HoveredRegion == INDEX_NONE && !bAgentSigns) || !CutLampInk)
	{
		if (CutLampMesh->GetNumSections() > 0)
		{
			CutLampMesh->ClearAllMeshSections();
		}
		return;
	}
	FVector Right = FVector::RightVector, Up = FVector::UpVector, EyeAt = FVector::ZeroVector;
	bool bEye = false;
	if (const APlayerController* Player = World->GetFirstPlayerController())
	{
		if (Player->PlayerCameraManager)
		{
			const FRotationMatrix Looking(Player->PlayerCameraManager->GetCameraRotation());
			Right = Looking.GetUnitAxis(EAxis::Y);
			Up = Looking.GetUnitAxis(EAxis::Z);
			EyeAt = Player->PlayerCameraManager->GetCameraLocation();
			bEye = true;
		}
	}
	const FVector Near = FVector::CrossProduct(Up, Right); // (toward the eye, along its axis)
	// Each layer of a sign lies a hair nearer the eye than the last. The step is taken toward the eye itself, not
	// along its axis: off the middle of the view a step along the axis slides the layers apart on the screen, and a
	// station dot's dark rim showed thick on one side and thin on the other.
	const double LayerStep = 0.004 * CameraDistance;
	auto Nearer = [&](const FVector& Point, double Layers)
	{
		return Point + (bEye ? (EyeAt - Point).GetSafeNormal() : Near) * (Layers * LayerStep);
	};
	const double U = UnitScale;
	// (A pixel of the 1600 view in the table's measure. The lamp is 8.5 px in radius from where he plays and nearer,
	// and is never under 6 px from any height.)
	const double Pixel = 0.00037 * CameraDistance;
	const double Radius = FMath::Lerp(8.5, 6.0, FMath::SmoothStep(4800.0, 9500.0, static_cast<double>(CameraDistance))) * Pixel;
	const FLinearColor Red(FColor(226, 62, 44)), Dark(FColor(18, 20, 24)), CreamRail(FColor(240, 228, 196));
	FCampaignMeshBuffers Sign;
	auto Inked = [](const FLinearColor& Colour, float Alpha)
	{
		FLinearColor Out = Colour;
		Out.A = Alpha;
		return Out.ToFColor(false);
	};
	// A disc, or a ring, or one dash of a ring, facing the eye: from one radius to another, between two angles.
	auto Arc = [&](const FVector& Middle, double Inner, double Outer, double From, double To, const FColor& InnerInk, const FColor& OuterInk)
	{
		const int32 Steps = FMath::Max(2, FMath::CeilToInt32((To - From) / (PI / 12.0)));
		int32 BeforeIn = INDEX_NONE, BeforeOut = INDEX_NONE;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const double Angle = FMath::Lerp(From, To, static_cast<double>(Step) / Steps);
			const FVector Out = Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
			const int32 In = Sign.AddVertex(Middle + Out * Inner, FVector::UpVector, InnerInk);
			const int32 Rim = Sign.AddVertex(Middle + Out * Outer, FVector::UpVector, OuterInk);
			if (Step > 0)
			{
				Sign.Triangles.Append({ BeforeIn, BeforeOut, Rim, BeforeIn, Rim, In });
			}
			BeforeIn = In;
			BeforeOut = Rim;
		}
	};
	// A bar facing the eye, from one point to another.
	auto Bar = [&](const FVector& From, const FVector& To, double HalfWide, const FColor& Ink)
	{
		const FVector Along = (To - From).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Near, Along).GetSafeNormal() * HalfWide;
		const int32 A0 = Sign.AddVertex(From - Side, FVector::UpVector, Ink), A1 = Sign.AddVertex(From + Side, FVector::UpVector, Ink);
		const int32 B0 = Sign.AddVertex(To - Side, FVector::UpVector, Ink), B1 = Sign.AddVertex(To + Side, FVector::UpVector, Ink);
		Sign.Triangles.Append({ A0, A1, B1, A0, B1, B0 });
	};
	for (const TPair<int32, bool>& Lamp : CutLamps)
	{
		const FCampaignRegion& Region = Data.Regions[Lamp.Key];
		// (On a short post over the crossed board, which stands a little to one side of the town. The art director's
		// section 7: a lamp sits inside its province by at least its own width; where the post would stand too near the
		// border the lamp stands over the town, and failing that at the middle of the province.)
		const double Clear = 2.0 * Radius / U;
		auto Inside = [&](const FVector2D& Spot)
		{
			for (const FVector2D& Step : { FVector2D(Clear, 0.0), FVector2D(-Clear, 0.0), FVector2D(0.0, Clear), FVector2D(0.0, -Clear) })
			{
				if (Data.FindRegionAt(Spot.X + Step.X, Spot.Y + Step.Y) != Lamp.Key)
				{
					return false;
				}
			}
			return true;
		};
		FVector2D Stands = CutSpot(Lamp.Key);
		if (!Inside(Stands))
		{
			Stands = Inside(Region.Center) ? Region.Center : Region.Centroid;
		}
		const FVector Foot = SurfacePointOver(Stands.X, Stands.Y, 0.1);
		const FVector Middle = Foot + FVector(0.0, 0.0, 2.4 * U) + Up * (Radius * 1.25);
		if (!Lamp.Value)
		{
			// Seen: a faint red glow, a dark rim, the red disc, and the broken rail in cream.
			Arc(Middle, Radius * 1.2, Radius * 2.0, 0.0, 2.0 * PI, Inked(Red, 0.22f), Inked(Red, 0.0f));
			Arc(Nearer(Middle, 1.0), 0.0, Radius * 1.26, 0.0, 2.0 * PI, Inked(Dark, 0.95f), Inked(Dark, 0.95f));
			Arc(Nearer(Middle, 2.0), 0.0, Radius, 0.0, 2.0 * PI, Inked(Red, 1.0f), Inked(Red, 1.0f));
		}
		else
		{
			// Known from a report, or as it was when last seen: the same size, hollow, a dashed red ring. (The art
			// director's round 38: on a dark see-through backing, (24,18,16) at about two thirds strength and 2.4 px wider
			// than the ring, so that on every ground it reads as a lamp gone out.)
			const FLinearColor Backing(FColor(24, 18, 16));
			Arc(Nearer(Middle, 1.0), 0.0, Radius + 2.4 * Pixel, 0.0, 2.0 * PI, Inked(Backing, 0.66f), Inked(Backing, 0.66f));
			for (int32 Dash = 0; Dash < 8; ++Dash)
			{
				const double From = (Dash + 0.14) * 2.0 * PI / 8.0, To = (Dash + 0.74) * 2.0 * PI / 8.0;
				Arc(Nearer(Middle, 2.0), Radius * 0.8, Radius, From, To, Inked(Red, 1.0f), Inked(Red, 1.0f));
			}
		}
		if (RailLight.Ringed.Contains(Lamp.Key))
		{
			// (Ruling 118: engineers in hand can be carried up to this break and mend it. A white ring two pixels wide,
			// just outside the lamp's dark rim.)
			const FLinearColor White(FColor(236, 246, 255));
			Arc(Nearer(Middle, 2.0), Radius * 1.26 + 0.4 * Pixel, Radius * 1.26 + 2.4 * Pixel, 0.0, 2.0 * PI, Inked(White, 1.0f), Inked(White, 1.0f));
		}
		// The broken rail: one half level, the other dropped.
		const FColor BrokenInk = Inked(Lamp.Value ? Red : CreamRail, 1.0f);
		const FVector Rail = Nearer(Middle, 3.0);
		const double Thick = Radius * 0.13;
		Bar(Rail - Right * (0.62 * Radius), Rail - Right * (0.07 * Radius), Thick, BrokenInk);
		Bar(Rail + Right * (0.1 * Radius) - Up * (0.06 * Radius), Rail + Right * (0.58 * Radius) - Up * (0.36 * Radius), Thick, BrokenInk);
	}
	// The station dots (the art director's railway brief, section 4): one in each province beyond the march that a
	// train would take the army to, over its town. White, 5.2 px in radius with a dark rim and a soft glow; never
	// under 4 px; the one at the end of the route under the pointer grows to 7.4.
	// (What may cover a station dot or a guard tab is gathered once, as boxes on the screen.)
	const APlayerController* Viewer = World->GetFirstPlayerController();
	const TheaterSpeed::FEye OneView(Viewer); // (the view worked out once for all these points: see TheaterSpeed)
	int32 ViewWide = 0, ViewHigh = 0;
	if (Viewer)
	{
		Viewer->GetViewportSize(ViewWide, ViewHigh);
	}
	const double PerPixel = ViewWide > 0 ? ViewWide / 1600.0 : 1.0; // (the screen's own points to a pixel of the 1600 view)
	// What may cover a dot, as boxes on the screen: every army's piece with its flag, and every town's plate.
	TArray<FBox2D> Covers;
	if (Viewer && Viewer->PlayerCameraManager && (RailLight.Dots.Num() > 0 || RailLight.Tracks.Num() > 0 || (!bReachShown && HoveredRegion != INDEX_NONE) || bAgentSigns))
	{
		for (const TObjectPtr<AArmyPiece>& Piece : Pieces)
		{
			if (!Piece || Piece->IsHidden())
			{
				continue;
			}
			FVector Origin, Extent;
			Piece->GetActorBounds(false, Origin, Extent);
			FBox2D Box(ForceInit);
			bool bAll = true;
			for (int32 Corner = 0; Corner < 8 && bAll; ++Corner)
			{
				FVector2D Screen;
				bAll = OneView.Project(Origin + Extent * FVector(Corner & 1 ? 1.0 : -1.0, Corner & 2 ? 1.0 : -1.0, Corner & 4 ? 1.0 : -1.0), Screen);
				Box += Screen;
			}
			if (bAll)
			{
				Covers.Add(Box);
			}
		}
		FVector Toward = -Viewer->PlayerCameraManager->GetCameraRotation().Vector();
		Toward.Z = 0.0;
		Toward = Toward.GetSafeNormal();
		for (const FPlateSpot& Plate : PlateSpots)
		{
			FVector2D Screen;
			if (OneView.Project(Plate.World + Toward * Plate.Reach, Screen))
			{
				const double HalfWide = (Plate.Name.Len() * 4.0 + 16.0) * PerPixel;
				Covers.Add(FBox2D(FVector2D(Screen.X - HalfWide, Screen.Y - 2.0 * PerPixel), FVector2D(Screen.X + HalfWide, Screen.Y + 26.0 * PerPixel)));
			}
		}
	}
	{
		const FLinearColor White(FColor(236, 246, 255));
		// (The art director's review of 18:06, 16 November.) The white is 10.4 px across at every height, in a 2.6 px
		// rim with a soft glow: smaller, it is lost on a town's roofs. And a dot never hides. It stands over its town
		// unless an army's piece, a flag or a plate would cover it there; then it moves out along the lit track toward
		// the army in hand until it is clear of them by its own width.
		for (const int32 RegionId : RailLight.Dots)
		{
			if (!Data.Regions.IsValidIndex(RegionId))
			{
				continue;
			}
			const double Dot = (RegionId == RailRouteTo ? 7.4 : 5.2) * Pixel;
			const FCampaignRegion& Region = Data.Regions[RegionId];
			// The lit track toward the army, and how much of it lies in this province.
			FVector2D Outward = FVector2D::ZeroVector;
			double Track = 0.0;
			if (const int32* Back = RailBack.Find(RegionId))
			{
				for (const FLandRail& Rail : Land.Rails)
				{
					if ((Rail.A == RegionId && Rail.B == *Back) || (Rail.B == RegionId && Rail.A == *Back))
					{
						Track = FVector2D::Distance(Region.Center, Rail.At);
						Outward = Track > 0.01 ? (Rail.At - Region.Center) / Track : FVector2D::ZeroVector;
						break;
					}
				}
			}
			auto Stands = [&](double Along)
			{
				const FVector2D Spot = Region.Center + Outward * Along;
				return SurfacePointOver(Spot.X, Spot.Y, 0.1) + FVector(0.0, 0.0, 2.8 * U) + Up * Dot;
			};
			auto Covered = [&](double Along, double Margin)
			{
				FVector2D Screen;
				if (!Viewer || !OneView.Project(Stands(Along), Screen))
				{
					return false;
				}
				for (const FBox2D& Box : Covers)
				{
					if (Screen.X > Box.Min.X - Margin && Screen.X < Box.Max.X + Margin && Screen.Y > Box.Min.Y - Margin && Screen.Y < Box.Max.Y + Margin)
					{
						return true;
					}
				}
				return false;
			};
			// (Its own width, rim and all, when it must find a place; it keeps the place it has while that stays clear by
			// half as much, so that it does not hop about as the eye moves.)
			const double Width = 15.6 * PerPixel;
			double& Moved = DotOut.FindOrAdd(RegionId, 0.0);
			if (Covered(Moved, 0.5 * Width))
			{
				for (double Try = 0.0; Try <= Track - 1.0; Try += 1.25)
				{
					if (!Covered(Try, Width))
					{
						Moved = Try;
						break;
					}
				}
			}
			else if (Moved > 0.0 && !Covered(0.0, Width))
			{
				Moved = 0.0; // (back over its town as soon as that is clear again)
			}
			const FVector Middle = Stands(Moved);
			Arc(Middle, Dot * 1.2, Dot * 2.6, 0.0, 2.0 * PI, Inked(White, 0.26f), Inked(White, 0.0f));
			Arc(Nearer(Middle, 1.0), 0.0, Dot + 2.6 * Pixel, 0.0, 2.0 * PI, Inked(Dark, 0.9f), Inked(Dark, 0.9f));
			Arc(Nearer(Middle, 2.0), 0.0, Dot, 0.0, 2.0 * PI, Inked(White, 1.0f), Inked(White, 1.0f));
		}
	}
	// THE GUARD TAB (ruling 125; the art director's guard-the-line brief, section 2). Anti-aircraft guns built in a
	// province take 15% a level off the chance that a raid cuts its railway, and this is where he sees it on the
	// table: a small dark tab beside the track with the guns' sign and three pips, brass where guns guard the line and
	// hollow where none do. It is never there at rest. With an army in hand every guarded province on a lit line
	// wears one; with the pointer on a place it can ride to, every province along that route wears one, guarded or
	// not, so that the gaps in the one route are drawn where he is looking; with no army in hand the province under
	// the pointer wears its own. It keeps its size, 41 by 15 px, and from far off it goes rather than shrinks.
	if (CameraDistance < 7000.0f && ShownBuildings.Num() > 0)
	{
		auto GunsOf = [this](int32 RegionId)
		{
			return ShownBuildings.IsValidIndex(RegionId) ? FMath::Clamp(static_cast<int32>(ShownBuildings[RegionId].Z), 0, 3) : 0;
		};
		// Who wears one, and the track it should stand by (none named: whichever of its tracks has the room).
		TMap<int32, FIntPoint> Wear;
		if (RailLight.From != INDEX_NONE && RailLight.Tracks.Num() > 0)
		{
			for (const FIntPoint& Lit : RailLight.Tracks)
			{
				for (const int32 RegionId : { Lit.X, Lit.Y })
				{
					if (GunsOf(RegionId) > 0 && !Wear.Contains(RegionId))
					{
						Wear.Add(RegionId, Lit);
					}
				}
			}
			for (const FIntPoint& OnRoute : RailRoute)
			{
				for (const int32 RegionId : { OnRoute.X, OnRoute.Y })
				{
					Wear.Add(RegionId, OnRoute);
				}
			}
		}
		else if (!bReachShown && Data.Regions.IsValidIndex(HoveredRegion))
		{
			Wear.Add(HoveredRegion, FIntPoint(INDEX_NONE, INDEX_NONE));
		}
		const double HalfWide = 20.5, HalfHigh = 7.5; // (in pixels of the 1600 view)
		const FVector Fore = FVector(Up.X, Up.Y, 0.0).GetSafeNormal(); // (away from the eye, on the table)
		const FLinearColor PlateInk(FColor(26, 21, 17)), EdgeInk(FColor(104, 88, 58)), Brass(FColor(226, 180, 92)), Dim(FColor(156, 142, 116));
		// A plate with rounded ends, facing the eye.
		auto Slab = [&](const FVector& Centre, double HalfW, double HalfH, double Round, const FColor& Ink)
		{
			Bar(Centre - Right * (HalfW - Round), Centre + Right * (HalfW - Round), HalfH, Ink);
			for (const double End : { -1.0, 1.0 })
			{
				Bar(Centre + Right * (End * (HalfW - Round)), Centre + Right * (End * HalfW), HalfH - Round, Ink);
				for (const double Top : { -1.0, 1.0 })
				{
					const FVector Corner = Centre + Right * (End * (HalfW - Round)) + Up * (Top * (HalfH - Round));
					const double Begins = End > 0.0 ? (Top > 0.0 ? 0.0 : 1.5 * PI) : (Top > 0.0 ? 0.5 * PI : PI);
					Arc(Corner, 0.0, Round, Begins, Begins + 0.5 * PI, Ink, Ink);
				}
			}
		};
		for (const TPair<int32, FIntPoint>& Each : Wear)
		{
			const int32 RegionId = Each.Key;
			if (!Data.Regions.IsValidIndex(RegionId))
			{
				continue;
			}
			const FCampaignRegion& Region = Data.Regions[RegionId];
			const double Town = TownRadius(RegionId);
			const FVector2D Base = WorldToMap(ArmySpot(RegionId, 0));
			const bool bLamp = CutLamps.ContainsByPredicate([RegionId](const TPair<int32, bool>& Lamp) { return Lamp.Key == RegionId; });
			const FVector2D LampAt = bLamp ? CutSpot(RegionId) : FVector2D::ZeroVector;
			// Beside the track where it meets the town, 12 px off it, on the side with the more open ground: clear of
			// the houses, the army's base, a cut's lamp and the border; and by a track that does not run down the
			// screen from the town, where the town's plate hangs.
			double BestScore = -1.0e9;
			FVector BestAt = FVector::ZeroVector;
			for (const FLandRail& Rail : Land.Rails)
			{
				if (Rail.A != RegionId && Rail.B != RegionId)
				{
					continue;
				}
				const double Length = FVector2D::Distance(Region.Center, Rail.At);
				if (Length < 1.0)
				{
					continue;
				}
				const bool bWanted = FMath::Min(Rail.A, Rail.B) == Each.Value.X && FMath::Max(Rail.A, Rail.B) == Each.Value.Y;
				const FVector2D Way = (Rail.At - Region.Center) / Length;
				const FVector2D OnTrack = Region.Center + Way * FMath::Min(Town + 1.6, 0.8 * Length);
				const FVector AlongTable = (MapToWorld(OnTrack.X + Way.X, OnTrack.Y + Way.Y, 0.0) - MapToWorld(OnTrack.X, OnTrack.Y, 0.0)).GetSafeNormal2D();
				const FVector AcrossTable(-AlongTable.Y, AlongTable.X, 0.0);
				// (across the track as the eye sees it: the tab's nearer edge is 12 px from the rails)
				const FVector2D Seen(FVector::DotProduct(AcrossTable, Right), FVector::DotProduct(AcrossTable, Up));
				const double Shown = Seen.Size();
				if (Shown < 0.05)
				{
					continue;
				}
				const double Off = (12.0 + (HalfWide * FMath::Abs(Seen.X) + HalfHigh * FMath::Abs(Seen.Y)) / Shown) * Pixel / Shown;
				for (const double Hand : { 1.0, -1.0 })
				{
					const FVector At = SurfacePointOver(OnTrack.X, OnTrack.Y, 0.1) + AcrossTable * (Hand * Off) + FVector(0.0, 0.0, 0.3 * U);
					const FVector2D OnMap = WorldToMap(At);
					if (Data.FindRegionAt(OnMap.X, OnMap.Y) != RegionId || GroundAt(OnMap.X, OnMap.Y) <= 0.0f)
					{
						continue; // (never over the border or on the water)
					}
					double Score = (bWanted ? 300.0 : 0.0) + 0.2 * Length;
					// (never under a town's plate, nor over an army's piece or its flag: the next track, or the other side)
					FVector2D OnView;
					if (Viewer && OneView.Project(At, OnView))
					{
						const double ClearWide = (HalfWide + 3.0) * PerPixel, ClearHigh = (HalfHigh + 3.0) * PerPixel;
						for (const FBox2D& Box : Covers)
						{
							if (OnView.X > Box.Min.X - ClearWide && OnView.X < Box.Max.X + ClearWide && OnView.Y > Box.Min.Y - ClearHigh && OnView.Y < Box.Max.Y + ClearHigh)
							{
								Score -= 400.0;
								break;
							}
						}
					}
					Score += 12.0 * FMath::Min(FVector2D::Distance(OnMap, Base) - 9.5, 0.0);
					Score += 12.0 * FMath::Min(FVector2D::Distance(OnMap, Region.Center) - Town - 0.4, 0.0);
					if (bLamp)
					{
						Score += 12.0 * FMath::Min(FVector2D::Distance(OnMap, LampAt) - 3.5, 0.0);
					}
					Score -= 60.0 * FMath::Max(0.0, -FVector::DotProduct(AlongTable, Fore));
					if (Score > BestScore)
					{
						BestScore = Score;
						BestAt = At;
					}
				}
			}
			if (BestScore < -1.0e8)
			{
				continue; // (no railway here, or nowhere of its own to stand)
			}
			const int32 Level = GunsOf(RegionId);
			const float Strength = Level > 0 ? 1.0f : 0.85f;
			Slab(BestAt, HalfWide * Pixel, HalfHigh * Pixel, 4.0 * Pixel, Inked(EdgeInk, 0.92f * Strength));
			Slab(Nearer(BestAt, 1.0), (HalfWide - 1.0) * Pixel, (HalfHigh - 1.0) * Pixel, 3.0 * Pixel, Inked(PlateInk, 0.92f * Strength));
			// The sign, 16 by 10.6 px: a gun on its mount, the barrel raised.
			const FColor SignInk = Inked(Level > 0 ? Brass : Dim, Strength);
			const FVector Glyph = Nearer(BestAt, 2.0) - Right * (9.0 * Pixel);
			auto Stroke = [&](double X0, double Y0, double X1, double Y1, double HalfThick)
			{
				Bar(Glyph + Right * (X0 * Pixel) + Up * (Y0 * Pixel), Glyph + Right * (X1 * Pixel) + Up * (Y1 * Pixel), HalfThick * Pixel, SignInk);
			};
			Stroke(-7.0, -4.4, 2.0, -4.4, 0.9); // the platform
			Stroke(-2.5, -4.4, -2.5, -0.8, 1.3); // the pedestal
			Stroke(-4.6, -2.9, -1.2, 0.3, 2.0); // the breech
			Stroke(-2.4, -0.8, 6.6, 5.0, 1.05); // the barrel
			// The three pips: filled up to the level, rings after it.
			for (int32 Pip = 0; Pip < 3; ++Pip)
			{
				const FVector PipAt = Nearer(BestAt, 2.0) + Right * ((4.2 + 6.0 * Pip) * Pixel);
				if (Pip < Level)
				{
					Arc(PipAt, 0.0, 2.3 * Pixel, 0.0, 2.0 * PI, Inked(Brass, Strength), Inked(Brass, Strength));
				}
				else
				{
					Arc(PipAt, 1.05 * Pixel, 2.3 * Pixel, 0.0, 2.0 * PI, Inked(Dim, Strength), Inked(Dim, Strength));
				}
			}
		}
	}
	// (Ruling 153; the art director's agent-piece brief, section 5.) From high up an agent is a sign and never a figure:
	// a disc 18 px across on the desk's dark ground with a brass rim, and in it, in the kit's hand, a brimmed hat over a
	// pair of shoulders. The mark of one caught: a dashed faint rim, no ground, the sign faint. It is never drawn over
	// a flag or a plate: it moves aside.
	if (!bAgentFigures && bPiecesShown)
	{
		const FLinearColor DeskInk(FColor(31, 26, 20)), BrassInk(FColor(224, 184, 114));
		for (FAgentStand& Stand : AgentStands)
		{
			const double Disc = 9.0 * Pixel;
			FVector Middle = Stand.Rest + FVector(0.0, 0.0, 1.2 * U) + Up * Disc;
			auto Hidden = [&](const FVector& Point)
			{
				FVector2D Screen;
				if (!Viewer || !OneView.Project(Point, Screen))
				{
					return false;
				}
				const double Margin = 10.0 * PerPixel;
				for (const FBox2D& Box : Covers)
				{
					if (Screen.X > Box.Min.X - Margin && Screen.X < Box.Max.X + Margin && Screen.Y > Box.Min.Y - Margin && Screen.Y < Box.Max.Y + Margin)
					{
						return true;
					}
				}
				return false;
			};
			if (Hidden(Middle))
			{
				for (const FVector& Aside : { Right * 26.0, Right * -26.0, Up * -26.0, Up * 26.0, Right * 52.0, Right * -52.0 })
				{
					if (!Hidden(Middle + Aside * Pixel))
					{
						Middle += Aside * Pixel;
						break;
					}
				}
			}
			Stand.SignAt = Middle;
			Stand.SignRadius = Disc;
			const float Strength = Stand.bMark ? 0.5f : 1.0f;
			if (Stand.bMark)
			{
				for (int32 Dash = 0; Dash < 8; ++Dash)
				{
					const double From = (Dash + 0.14) * 2.0 * PI / 8.0, To = (Dash + 0.74) * 2.0 * PI / 8.0;
					Arc(Nearer(Middle, 1.0), Disc - 1.5 * Pixel, Disc, From, To, Inked(BrassInk, Strength), Inked(BrassInk, Strength));
				}
			}
			else
			{
				// (Release 2: an uncovered enemy agent's rim is its country's colour.)
				const FLinearColor RimInk = Stand.bFoe ? Stand.FoeInk : BrassInk;
				Arc(Middle, 0.0, Disc, 0.0, 2.0 * PI, Inked(RimInk, 1.0f), Inked(RimInk, 1.0f));
				Arc(Nearer(Middle, 1.0), 0.0, Disc - 1.5 * Pixel, 0.0, 2.0 * PI, Inked(DeskInk, 0.97f), Inked(DeskInk, 0.97f));
			}
			// The sign, 13 px, from the kit's own drawing on a square of 24: the hat's crown, its brim, the shoulders.
			const FColor HatInk = Inked(BrassInk, Strength);
			const FVector Sheet = Nearer(Middle, 2.0);
			const double Step = 13.0 / 24.0 * Pixel;
			auto On = [&](double X, double Y) { return Sheet + Right * ((X - 12.0) * Step) + Up * ((12.5 - Y) * Step); };
			auto Stroke = [&](const FVector& From, const FVector& To) { Bar(From, To, 0.7 * Pixel, HatInk); };
			Stroke(On(8.0, 9.5), On(9.0, 4.5));
			Stroke(On(9.0, 4.5), On(15.0, 4.5));
			Stroke(On(15.0, 4.5), On(16.0, 9.5));
			Stroke(On(3.5, 9.8), On(20.5, 9.8));
			FVector Last = On(5.0, 20.5);
			for (int32 Bend = 1; Bend <= 12; ++Bend)
			{
				// (two soft curves: up from the left shoulder to the neck, and down to the right)
				const double T = (Bend <= 6 ? Bend : Bend - 6) / 6.0;
				const FVector2D P0 = Bend <= 6 ? FVector2D(5.0, 20.5) : FVector2D(12.0, 14.0), C = Bend <= 6 ? FVector2D(5.0, 14.0) : FVector2D(19.0, 14.0), P1 = Bend <= 6 ? FVector2D(12.0, 14.0) : FVector2D(19.0, 20.5);
				const FVector2D At = P0 * FMath::Square(1.0 - T) + C * (2.0 * T * (1.0 - T)) + P1 * FMath::Square(T);
				const FVector Next = On(At.X, At.Y);
				Stroke(Last, Next);
				Last = Next;
			}
		}
	}
	if (!Sign.Commit(CutLampMesh, 0, CutLampInk))
	{
		CutLampMesh->ClearAllMeshSections();
	}
	else
	{
		CutLampMesh->SetCastShadow(false);
		CutLampMesh->SetTranslucentSortPriority(2); // (over the washes and the smoke; under the searchlights)
	}
}

void ACampaignMapActor::ShowWorks(const TArray<int32>& BridgesDown, const TArray<int32>& RailsCut)
{
	THEATER_SPEED_SCOPE(ShowWorks);
	if (bWorksShown && BridgesDown == ShownDown && RailsCut == ShownCut)
	{
		return;
	}
	ShownDown = BridgesDown;
	ShownCut = RailsCut;
	BuildWorks();
}

void ACampaignMapActor::BuildWaterways()
{
	RiverMesh->ClearAllMeshSections();
	RiverPaths.Reset();
	RiverBroad.Reset();
	RiverHalf.Reset();
	// A river is a ribbon of water set into the land along its course: narrow at the source, broader at the
	// mouth (a great river about a map unit and a quarter across, the rest under one), its course rounded and
	// given a gentle wander so that it winds, blue-green, with a dark bank on either side.
	// (the paint is laid on darker than it is meant to look, to suit the lamp, as the land's is)
	const FLinearColor MidstreamRuns = FLinearColor(FColor(72, 104, 125)), BankRuns = FLinearColor(FColor(60, 88, 106));
	const FColor Earth = FColor(26, 38, 36);
	// ("-TheaterJungleRiver=102,100,88" tries another paint for a ribbon in the jungle. It is laid cooler than it is
	// seen: under the lamp (98, 95, 79) measured (122, 104, 67), and the painted jungle water is (128, 110, 76).)
	// (The art director's round 33, ruling C: she withdrew the brown. Every other river on the map is blue, so a brown
	// ribbon reads as a road whatever its shape. A great river of the jungle is a silted blue, to be seen at about
	// (98, 146, 160); its mud is shown by its sand bars and its islands.)
	// (laid (66, 112, 142) it was seen at about (60, 138, 163), too bright a sky blue; laid (103, 118, 139), at about
	// (136, 144, 158), a pale lavender: between the two)
	const FColor SiltedBlue(85, 120, 140);
	FLinearColor JungleMidstream = FLinearColor(SiltedBlue);
	{
		FString Asked;
		TArray<FString> Parts;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterJungleRiver="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 3)
		{
			JungleMidstream = FLinearColor(FColor(FCString::Atoi(*Parts[0]), FCString::Atoi(*Parts[1]), FCString::Atoi(*Parts[2])));
		}
	}
	// (The art director's round 31: a brown river is lighter in mid channel and darker toward its banks, with a darker
	// wet margin inside its edge and no dark outline round it.)
	// (and it is flat: no tube shading. The river is a cut in the canopy: a soft shade inside its north-west bank,
	// where the trees stand between it and the sun, and a pale lit line on the far bank.)
	const FLinearColor JungleBank = JungleMidstream, JungleShade = JungleMidstream * 0.7f, JungleLit = JungleMidstream * 1.22f;
	// (in the depth of winter the rivers freeze where the snow lies: ice, pale and dull, with a darker lead midstream)
	const FLinearColor MidstreamIce = FLinearColor(FColor(140, 162, 172)), BankIce = FLinearColor(FColor(186, 196, 199));
	RiversDrawnFor = WinterHard;
	// (The art director's review of the haze: a river under the haze stayed lit, and by night it was the brightest
	// thing on the hazed map. Under the haze the water and its banks are darker, still blue: by day to about
	// (48, 74, 96), by night to about (40, 58, 84).)
	RiversHazedFor = HazeSet;
	bRiversNightFor = NightNow > 0.5f;
	const FVector RiverHaze = bRiversNightFor ? FVector(0.47, 0.54, 0.63) : FVector(0.7, 0.66, 0.72);
	const double Lift = 0.1;
	// The course of each river as it is laid, before any is drawn: so that one that runs into another can be ended
	// at the other's bank.
	TArray<TArray<FVector2D>> RiverLaid;
	RiverLaid.SetNum(Land.Rivers.Num());
	for (const FLandRiver& River : Land.Rivers)
	{
		const int32 Count = River.Course.Num();
		if (Count < 2)
		{
			continue;
		}
		// A smooth curve through the points of the course, a short step at a time.
		TArray<FVector2D> Path;
		for (int32 K = 0; K + 1 < Count; ++K)
		{
			const FVector2D P0 = River.Course[FMath::Max(K - 1, 0)], P1 = River.Course[K], P2 = River.Course[K + 1], P3 = River.Course[FMath::Min(K + 2, Count - 1)];
			const int32 Steps = FMath::Max(2, FMath::CeilToInt32(FVector2D::Distance(P1, P2) / 2.0));
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const double T = static_cast<double>(Step) / Steps;
				if (Data.bWorld && (River.Course[0].X < 0.0 || River.Course[0].Y < 0.0 || River.Course[0].X > Data.Width || River.Course[0].Y > Data.Height))
				{
					// (The wider chart's rivers are given by a score of points a thousand units apart in all, with here and
					// there two close together where another river joins. The plain curve, which leaves each point by
					// half the way between its neighbours, overshot such a short stretch and kinked: a notch in the
					// Mississippi where the Missouri comes in. There the curve leaves a point by no more than the
					// stretch is long. Europe's rivers are left as they were laid.)
					const double Long = FVector2D::Distance(P1, P2);
					FVector2D Leave = (P2 - P0) * 0.5, Arrive = (P3 - P1) * 0.5;
					Leave *= FMath::Min(1.0, Long / FMath::Max(Leave.Size(), 1.0e-6));
					Arrive *= FMath::Min(1.0, Long / FMath::Max(Arrive.Size(), 1.0e-6));
					const double T2 = T * T, T3 = T2 * T;
					Path.Add(P1 * (2.0 * T3 - 3.0 * T2 + 1.0) + Leave * (T3 - 2.0 * T2 + T) + P2 * (-2.0 * T3 + 3.0 * T2) + Arrive * (T3 - T2));
					continue;
				}
				Path.Add(0.5 * ((2.0 * P1) + (-P0 + P2) * T + (2.0 * P0 - 5.0 * P1 + 4.0 * P2 - P3) * T * T + (-P0 + 3.0 * P1 - 3.0 * P2 + P3) * T * T * T));
			}
		}
		Path.Add(River.Course.Last());
		if (Data.bWorld && (River.Course[0].X < 0.0 || River.Course[0].Y < 0.0 || River.Course[0].X > Data.Width || River.Course[0].Y > Data.Height))
		{
			// (The art director's round 30: the Mississippi turned through three hard corners where the Missouri comes
			// in. No ribbon turns more than about thirty degrees at a point: beyond Europe each corner is rounded
			// over the points either side of it.)
			for (int32 Pass = 0; Pass < 5; ++Pass)
			{
				const TArray<FVector2D> Sharp = Path;
				for (int32 K = 1; K + 1 < Path.Num(); ++K)
				{
					Path[K] = Sharp[K - 1] * 0.25 + Sharp[K] * 0.5 + Sharp[K + 1] * 0.25;
				}
			}
		}
		// A little wander, so that a river does not run like a ruled line: nothing at its two ends, most in the
		// middle, and never so much that it leaves its bridges.
		{
			const int32 Seed = GetTypeHash(River.Name) % 9973;
			const TArray<FVector2D> Straight = Path;
			for (int32 K = 1; K + 1 < Path.Num(); ++K)
			{
				const double T = static_cast<double>(K) / (Path.Num() - 1);
				const FVector2D Along = (Straight[K + 1] - Straight[K - 1]).GetSafeNormal();
				const double Wobble = 0.5 * FMath::Sin(T * 7.0 * UE_DOUBLE_TWO_PI + Seed) + 0.28 * FMath::Sin(T * 17.0 * UE_DOUBLE_TWO_PI + Seed * 1.7) + 0.16 * FMath::Sin(T * 31.0 * UE_DOUBLE_TWO_PI + Seed * 2.3);
				Path[K] = Straight[K] + FVector2D(Along.Y, -Along.X) * Wobble * FMath::Sin(T * UE_DOUBLE_PI);
			}
		}
		RiverLaid[static_cast<int32>(&River - Land.Rivers.GetData())] = MoveTemp(Path);
	}
	// Open water beyond Europe, as a fine sheet of ground shows it: the chart draws no land there (or, outside the
	// chart's frame, the world's real heights lie at or below the sea), there and two units round.
	// (Seen at Quebec in the first picture of the rule below: asked only of the one place, a ribbon broke off on dry
	// land short of its first bridge. The chart draws the estuary's head as a channel a unit or two across, which no
	// sheet of ground shows as water: the very channel the ribbon is there to stand for.)
	auto SeaAt = [this](const FVector2D& At)
	{
		if (Data.FindRegionAt(At.X, At.Y) != INDEX_NONE)
		{
			return false;
		}
		if (Data.IsCharted(At.X, At.Y))
		{
			for (const TArray<FVector2D>& Ring : Data.Land)
			{
				if (FCampaignMapData::PointInRing(Ring, At.X, At.Y))
				{
					return false; // (land no province claims, or a lake cut out of it: the lakes have their own rule)
				}
			}
			return true;
		}
		return WorldTerrain.HasElevation() && WorldTerrain.MetresAt(WrapX(At.X), At.Y) <= 0.0f;
	};
	auto OpenSeaAt = [&SeaAt](const FVector2D& At)
	{
		return SeaAt(At) && SeaAt(At + FVector2D(2.0, 0.0)) && SeaAt(At - FVector2D(2.0, 0.0)) && SeaAt(At + FVector2D(0.0, 2.0)) && SeaAt(At - FVector2D(0.0, 2.0));
	};
	TArray<int32> RiverUnderFrom; // where a river's ribbon goes under the ribbon of the river it joins
	RiverUnderFrom.Init(MAX_int32, Land.Rivers.Num());
	if (Data.bWorld)
	{
		// (The wider chart's rivers, by the art director's round 30. A tributary's ribbon runs on under the ribbon of
		// the river it joins, to that river's middle, so that no end shows: cut off at the bank it showed a square
		// end and a gap. Its last point is brought to the other's line, and from where it comes within the other's
		// banks it lies a hair lower. Europe's rivers are left as they were laid.)
		auto BreadthOf = [&](int32 Index, int32 At)
		{
			return FCampaignMapData::RiverBroad(Land.Rivers[Index].Name) * FMath::Lerp(0.5, 1.0, FMath::Sqrt(static_cast<double>(At) / FMath::Max(RiverLaid[Index].Num() - 1, 1)));
		};
		for (int32 Index = 0; Index < RiverLaid.Num(); ++Index)
		{
			TArray<FVector2D>& Mine = RiverLaid[Index];
			if (Mine.Num() < 6 || (Mine[0].X >= 0.0 && Mine[0].Y >= 0.0 && Mine[0].X <= Data.Width && Mine[0].Y <= Data.Height))
			{
				continue;
			}
			if (OpenSeaAt(Mine.Last()))
			{
				continue; // (a river that ends in open water ends there: it is no tributary)
			}
			// (the river it ends on, if it ends on one: the one whose line lies nearest its last point. The rules give a
			// great river a score of points, and the curve drawn through them lies a good way off the straight line
			// between two of them, on which a tributary's last point was set: the Madeira stopped fifteen units short
			// of the Amazon. Within thirty units it is one, and the tributary is carried on to it.)
			int32 Joined = INDEX_NONE;
			double Nearest = 30.0;
			// (The art director, 18 November 2026, at Quebec, the cause found: the St Lawrence's line stops in its estuary,
			// and the St John's, thirty units off over the hills, was the nearest: the great river was carried on to it
			// across the far shore as its tributary. The rules' file says what each river of the wider chart runs
			// into. Where it says, a river is carried on only to the river it names, and one that runs into a sea
			// or a lake to none.)
			const FString& Into = Land.Rivers[Index].Into;
			for (int32 Other = 0; Other < RiverLaid.Num(); ++Other)
			{
				const TArray<FVector2D>& Theirs = RiverLaid[Other];
				if (Other == Index || Theirs.Num() < 6 || (!Into.IsEmpty() && Land.Rivers[Other].Name != Into))
				{
					continue;
				}
				for (int32 K = 0; K + 1 < Theirs.Num(); ++K)
				{
					const double Far = FMath::PointDistToSegment(FVector(Mine.Last(), 0.0), FVector(Theirs[K], 0.0), FVector(Theirs[K + 1], 0.0));
					if (Far < Nearest)
					{
						Nearest = Far;
						Joined = Other;
					}
				}
			}
			if (Joined != INDEX_NONE)
			{
				const int32 Other = Joined;
				const TArray<FVector2D>& Theirs = RiverLaid[Other];
				// (how far a place lies from the other river's line, the place on that line nearest it, and how broad that river is there)
				auto Off = [&](const FVector2D& P, double& OutHalf, FVector2D& OutNearest)
				{
					double Best = 1.0e9;
					for (int32 K = 0; K + 1 < Theirs.Num(); ++K)
					{
						const FVector Close = FMath::ClosestPointOnSegment(FVector(P, 0.0), FVector(Theirs[K], 0.0), FVector(Theirs[K + 1], 0.0));
						const double Far = FVector2D::Distance(P, FVector2D(Close.X, Close.Y));
						if (Far < Best)
						{
							Best = Far;
							OutHalf = BreadthOf(Other, K);
							OutNearest = FVector2D(Close.X, Close.Y);
						}
					}
					return Best;
				};
				double Half = 0.0;
				FVector2D OnTheirs;
				const double Short = Off(Mine.Last(), Half, OnTheirs);
				if (Short > 1.0)
				{
					const FVector2D Was = Mine.Last();
					const int32 Steps = FMath::CeilToInt32(Short / 2.0);
					for (int32 Step = 1; Step < Steps; ++Step)
					{
						Mine.Add(FMath::Lerp(Was, OnTheirs, static_cast<double>(Step) / Steps));
					}
					Mine.Add(OnTheirs);
					// (and the turn where the carried-on stretch begins is rounded, like every other)
					const int32 Bend = Mine.Num() - Steps - 1;
					for (int32 Pass = 0; Pass < 5; ++Pass)
					{
						const TArray<FVector2D> Sharp = Mine;
						for (int32 K = FMath::Max(Bend - 5, 1); K < FMath::Min(Bend + 6, Mine.Num() - 1); ++K)
						{
							Mine[K] = Sharp[K - 1] * 0.25 + Sharp[K] * 0.5 + Sharp[K + 1] * 0.25;
						}
					}
				}
				else
				{
					Mine.Last() = OnTheirs;
				}
				int32 From = Mine.Num() - 1;
				while (From > 1 && Off(Mine[From - 1], Half, OnTheirs) < Half + 0.3)
				{
					--From;
				}
				RiverUnderFrom[Index] = From;
			}
		}
	}
	FCampaignMeshBuffers Buffers, BuffersBeyond;
	// (Speed, 18 November 2026: tc.Speed.RiverInks. Each point's ink as it is before the haze, and the province the
	// haze is asked of for it, none for a point it never darkens: so that where the haze moves the inks are changed
	// in place, the same inks, and the rivers are not laid again, a tenth of a second on the wider chart.)
	const bool bKeepInks = TheaterSpeed::RiverInks();
	TArray<FColor> PlainInks[2];
	TArray<int32> InkRegions[2];
	for (const FLandRiver& River : Land.Rivers)
	{
		const int32 Count = River.Course.Num();
		if (Count < 2)
		{
			continue;
		}
		// (a river of the wider chart goes in a section of its own, which is put away when the eye is high: from
		// there the coarse sheet's painted great rivers stand for them, and a ribbon laid at the fine ground's
		// height would show through the coarse ground only in scraps)
		const bool bRiverBeyond = Data.bWorld && (River.Course[0].X < 0.0 || River.Course[0].Y < 0.0 || River.Course[0].X > Data.Width || River.Course[0].Y > Data.Height);
		FCampaignMeshBuffers& Into = bRiverBeyond ? BuffersBeyond : Buffers;
		const int32 Kind = bRiverBeyond ? 1 : 0;
		const int32 RiverIndex = static_cast<int32>(&River - Land.Rivers.GetData());
		TArray<FVector2D> Path = RiverLaid[RiverIndex];
		const double Broad = FCampaignMapData::RiverBroad(River.Name);
		RiverPaths.Add(Path); // (kept, with how broad it is, for the towns' own bridges)
		RiverBroad.Add(Broad);
		// (release 2, the flak ring's second mend: and its ribbon's half breadth at each point as it is laid below, wander,
		// flare and widening and all, for the gun pits that keep off the water; none past where the ribbon stops)
		TArray<double>& Halves = RiverHalf.AddDefaulted_GetRef();
		int32 Previous[5] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
		// (The art director, 18 November 2026, at Quebec on build 1668: past the town the St Lawrence's ribbon carried on
		// across the estuary's open water, a river drawn on top of the sea. The coarse sheet of ground, which is what
		// "the land stops" is asked of below, is too coarse to show that estuary; a fine sheet shows it. Beyond
		// Europe a ribbon is not drawn where it lies wholly over open water as the chart draws it (or, outside the
		// chart's frame, as the world's real heights have it): it ends where it meets the estuary, widening into it,
		// and is taken up again only where its course crosses land the sheets show, under a bridge of the rules.)
		TArray<bool> OverWater;
		int32 MeetsWater = MAX_int32;
		if (bRiverBeyond)
		{
			OverWater.Init(false, Path.Num());
			for (int32 K = Path.Num() / 2 + 1; K < Path.Num(); ++K)
			{
				OverWater[K] = OpenSeaAt(Path[K]);
				if (OverWater[K] && MeetsWater == MAX_int32)
				{
					MeetsWater = K;
				}
			}
		}
		// (how far along the river each point lies, how broad the river is there and how brown: for its sand bars)
		TArray<double> RunAt, HalfAt;
		TArray<float> BrownAt;
		double Run = 0.0;
		for (int32 K = 0; K < Path.Num(); ++K)
		{
			const FVector2D Along = (Path[FMath::Min(K + 1, Path.Num() - 1)] - Path[FMath::Max(K - 1, 0)]).GetSafeNormal();
			const FVector2D Side(Along.Y, -Along.X);
			Run += K > 0 ? FVector2D::Distance(Path[K], Path[K - 1]) : 0.0;
			// (a brown river of the jungle wanders in width by about a fifth along its length: an even width is a road's)
			float JungleHere = 0.0f;
			if (bRiverBeyond)
			{
				float LooksHere[WorldLook::Count];
				FCampaignClimate::Get().SharesAt(Data.LatitudeAt(Path[K].Y), Data.LongitudeAt(Path[K].X), LooksHere);
				JungleHere = LooksHere[WorldLook::Jungle];
			}
			// (round 33: between about 0.7 and 1.4 of its mean; and a tributary's mouth flares where it joins its river)
			const double Wander = FMath::Clamp(1.7 * FMath::PerlinNoise1D(static_cast<float>(Run * 0.031 + RiverIndex * 13.7)), -1.0, 1.0);
			const double Flare = RiverUnderFrom[RiverIndex] < Path.Num() ? 1.0 + 0.6 * FMath::SmoothStep(static_cast<double>(RiverUnderFrom[RiverIndex] - 5), static_cast<double>(RiverUnderFrom[RiverIndex]), static_cast<double>(K)) : 1.0;
			const double Wanders = (1.0 + FMath::SmoothStep(0.08f, 0.34f, JungleHere) * (Wander > 0.0 ? 0.4 * Wander : 0.3 * Wander)) * (bRiverBeyond ? Flare : 1.0);
			// (it widens into the open water it meets: to a little over twice its breadth, over its last seven points)
			const double Widens = MeetsWater < Path.Num() && K <= MeetsWater ? 1.0 + 1.2 * FMath::SmoothStep(static_cast<double>(MeetsWater - 7), static_cast<double>(MeetsWater), static_cast<double>(K)) : 1.0;
			const double Half = Broad * FMath::Lerp(0.5, 1.0, FMath::Sqrt(static_cast<double>(K) / FMath::Max(Path.Num() - 1, 1))) * Wanders * Widens;
			Halves.Add(Half);
			// A river does not run over the sea: stop where the land does.
			if (GroundAt(Path[K].X, Path[K].Y) <= 0.0f && K > Path.Num() / 2)
			{
				break;
			}
			if (OverWater.IsValidIndex(K) && OverWater[K])
			{
				// (open water: the ribbon is broken off, and begins afresh where its course next crosses land)
				for (int32 P = 0; P < 5; ++P)
				{
					Previous[P] = INDEX_NONE;
				}
				continue;
			}
			const float Frozen = WinterHard >= 0.8f ? FMath::SmoothStep(0.45f, 0.8f, SnowLying(Path[K], WinterHard)) : 0.0f;
			FLinearColor MidstreamHere = MidstreamRuns, BankHere = BankRuns;
			float Brown = 0.0f; // how wholly it is a brown river of the jungle here
			if (bRiverBeyond)
			{
				// (The art director's condition of round 25: a ribbon takes the water colour of the country it crosses,
				// so that it and the painted tributaries that run into it are one water. Brown in the jungle.)
				float Shares[WorldLook::Count];
				FCampaignClimate::Get().SharesAt(Data.LatitudeAt(Path[K].Y), Data.LongitudeAt(Path[K].X), Shares);
				// (Blue mixed straight into brown passes through grey, and where the climate is half jungle, as on the
				// upper Amazon under the Andes, the ribbon read as a tarred road. It turns by way of a murky river
				// green, and wholly by the time the country is three fifths jungle.)
				// (The art director's round 32: the trunk stays muddy brown all the way to the foothills; the common
				// river colour is for the mountain streams above. It has turned wholly by a third jungle.)
				const float Turned = FMath::SmoothStep(0.08f, 0.34f, Shares[WorldLook::Jungle]);
				MidstreamHere = FMath::Lerp(MidstreamHere, JungleMidstream, Turned);
				BankHere = FMath::Lerp(BankHere, JungleBank, Turned);
				Brown = FMath::Clamp(Turned * 2.0f - 1.0f, 0.0f, 1.0f);
				// (long faint streaks along the current: the mid channel and the banks each go a little lighter and
				// darker along the river, out of step with one another)
				MidstreamHere *= 1.0f + 0.05f * Brown * FMath::PerlinNoise1D(static_cast<float>(Run * 0.06 + 7.7 + RiverIndex));
				BankHere *= 1.0f + 0.05f * Brown * FMath::PerlinNoise1D(static_cast<float>(Run * 0.045 + 21.3 + RiverIndex));
			}
			RunAt.Add(Run);
			HalfAt.Add(Half);
			BrownAt.Add(Brown);
			const FColor Midstream = FMath::Lerp(MidstreamHere, MidstreamIce, Frozen).ToFColor(true), Bank = FMath::Lerp(BankHere, BankIce, Frozen).ToFColor(true);
			// (a tributary's ribbon lies a hair lower where it runs under the ribbon of the river it joins)
			const double Under = K >= RiverUnderFrom[RiverIndex] ? 0.045 : 0.0;
			// (A common river has the dark of its bank beside the water. A brown river has no outline: its outer strip
			// lies inside its edge, a darker wet margin about two points of the screen wide, and its edge wanders a
			// little each side so that it is not a ruled line.)
			const double EdgeLeft = 0.14 * Brown * FMath::PerlinNoise1D(static_cast<float>(Run * 0.41 + 3.1 + RiverIndex * 5.3));
			const double EdgeRight = 0.14 * Brown * FMath::PerlinNoise1D(static_cast<float>(Run * 0.37 + 47.9 + RiverIndex * 5.3));
			const double Outer = Half + FMath::Lerp(0.2, 0.04, static_cast<double>(Brown)); // (a common river's is the dark of the bank beside the water)
			const double Inner = Half - static_cast<double>(Brown) * FMath::Min(0.5, Half * 0.34);
			const FVector2D Across[5] = { Path[K] + Side * (Outer + EdgeLeft), Path[K] + Side * (Inner + EdgeLeft), Path[K], Path[K] - Side * (Inner + EdgeRight), Path[K] - Side * (Outer + EdgeRight) };
			// (which bank the sun's trees shade: the one whose own side looks to the north-west)
			const double ToNorthWest = FVector2D::DotProduct(Side, FVector2D(-0.7071, -0.7071));
			const FLinearColor EdgeLeft_ = FMath::Lerp(JungleLit, JungleShade, static_cast<float>(0.5 + 0.5 * ToNorthWest)), EdgeRight_ = FMath::Lerp(JungleLit, JungleShade, static_cast<float>(0.5 - 0.5 * ToNorthWest));
			const FColor MarginLeft = FMath::Lerp(FLinearColor(Earth), FMath::Lerp(EdgeLeft_, BankIce, Frozen), Brown).ToFColor(true);
			const FColor MarginRight = FMath::Lerp(FLinearColor(Earth), FMath::Lerp(EdgeRight_, BankIce, Frozen), Brown).ToFColor(true);
			const int32 RegionHere = HazeSet.Num() > 0 || bKeepInks ? Data.FindRegionAt(Path[K].X, Path[K].Y) : INDEX_NONE;
			const bool bUnderHaze = HazeSet.Num() > 0 && HazeSet.Contains(RegionHere);
			auto Hazed = [bUnderHaze, &RiverHaze](const FColor& Paint)
			{
				return bUnderHaze ? FColor(static_cast<uint8>(Paint.R * RiverHaze.X), static_cast<uint8>(Paint.G * RiverHaze.Y), static_cast<uint8>(Paint.B * RiverHaze.Z), Paint.A) : Paint;
			};
			const FColor Plain[5] = { MarginLeft, Bank, Midstream, Bank, MarginRight };
			const FColor Inks[5] = { Hazed(MarginLeft), Hazed(Bank), Hazed(Midstream), Hazed(Bank), Hazed(MarginRight) };
			int32 Here[5];
			for (int32 P = 0; P < 5; ++P)
			{
				// (the banks a hair lower than the water between them, so the water is never hidden by its bank)
				Here[P] = Into.AddVertex(SurfacePointOver(Across[P].X, Across[P].Y, (P == 0 || P == 4 ? Lift - 0.03 : Lift) - Under), FVector::UpVector, Inks[P]);
				if (bKeepInks)
				{
					PlainInks[Kind].Add(Plain[P]);
					InkRegions[Kind].Add(RegionHere);
				}
			}
			if (Previous[0] != INDEX_NONE)
			{
				for (int32 Strip = 0; Strip < 4; ++Strip)
				{
					Into.AddTriangleFacingUp(Previous[Strip], Previous[Strip + 1], Here[Strip]);
					Into.AddTriangleFacingUp(Previous[Strip + 1], Here[Strip], Here[Strip + 1]);
				}
			}
			for (int32 P = 0; P < 5; ++P)
			{
				Previous[P] = Here[P];
			}
		}
		// (The art director's round 31.) The sand bars of a great brown river: long lenses of pale sand, four to eight
		// times as long as they are wide, hugging the inside of a bend, at uneven gaps of six to twenty river widths
		// and never on the centre line; now and then an island with a tuft of green.
		if (bRiverBeyond && Broad >= 0.8 && RunAt.Num() > 8)
		{
			FRandomStream Chance(RiverIndex * 7919 + 17);
			// (laid well under the pale sand they are to look, about (205, 190, 150): the water's sheen lifts them, and laid at
			// two thirds of that they came out chalk white)
			const FColor Sand(132, 124, 98), WetSand(104, 100, 84), Tuft(46, 74, 44);
			// (a place a given way along the river, and the way across it there)
			auto Place = [&](double Far, FVector2D& OutAt, FVector2D& OutSide)
			{
				int32 K = 1;
				while (K + 1 < RunAt.Num() && RunAt[K] < Far)
				{
					++K;
				}
				const double Span = FMath::Max(RunAt[K] - RunAt[K - 1], 1.0e-6);
				OutAt = FMath::Lerp(Path[K - 1], Path[K], FMath::Clamp((Far - RunAt[K - 1]) / Span, 0.0, 1.0));
				const FVector2D Along = (Path[FMath::Min(K + 1, RunAt.Num() - 1)] - Path[FMath::Max(K - 2, 0)]).GetSafeNormal();
				OutSide = FVector2D(Along.Y, -Along.X);
			};
			auto Lens = [&](double From, double Long, double Wide, double Aside, const FColor& Middle, const FColor& Rim, double Raised)
			{
				const int32 Parts = 12;
				int32 Last[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
				for (int32 Part = 0; Part <= Parts; ++Part)
				{
					const double T = static_cast<double>(Part) / Parts;
					FVector2D At, Side;
					Place(From + Long * T, At, Side);
					const double HalfWide = 0.5 * Wide * FMath::Pow(FMath::Sin(UE_DOUBLE_PI * T), 0.75);
					const FVector2D Centre = At + Side * Aside;
					const FVector2D Points[3] = { Centre + Side * HalfWide, Centre, Centre - Side * HalfWide };
					int32 Now[3];
					for (int32 P = 0; P < 3; ++P)
					{
						Now[P] = Into.AddVertex(SurfacePointOver(Points[P].X, Points[P].Y, Lift + Raised), FVector::UpVector, P == 1 ? Middle : Rim);
						if (bKeepInks)
						{
							PlainInks[Kind].Add(P == 1 ? Middle : Rim);
							InkRegions[Kind].Add(INDEX_NONE); // (sand and islands are not darkened under the haze)
						}
					}
					if (Last[0] != INDEX_NONE)
					{
						for (int32 Strip = 0; Strip < 2; ++Strip)
						{
							Into.AddTriangleFacingUp(Last[Strip], Last[Strip + 1], Now[Strip]);
							Into.AddTriangleFacingUp(Last[Strip + 1], Now[Strip], Now[Strip + 1]);
						}
					}
					for (int32 P = 0; P < 3; ++P)
					{
						Last[P] = Now[P];
					}
				}
			};
			double Next = RunAt[0] + 10.0 + 40.0 * Chance.FRand();
			int32 Bars = 0;
			for (int32 K = 3; K + 3 < RunAt.Num(); ++K)
			{
				if (RunAt[K] < Next)
				{
					continue;
				}
				const double RiverWide = 2.0 * HalfAt[K];
				const double Wide = RiverWide * Chance.FRandRange(0.16f, 0.25f), Long = Wide * Chance.FRandRange(4.0f, 8.0f);
				Next = RunAt[K] + Long + RiverWide * Chance.FRandRange(6.0f, 20.0f);
				if (BrownAt[K] < 0.9f || K >= RiverUnderFrom[RiverIndex] || RunAt[K] + Long > RunAt.Last() - 2.0)
				{
					continue;
				}
				// (the inside of the bend here; where the river runs straight, either bank)
				const FVector2D Before = (Path[K] - Path[K - 3]).GetSafeNormal(), After = (Path[K + 3] - Path[K]).GetSafeNormal();
				const double Turn = Before.X * After.Y - Before.Y * After.X;
				const double Inside = FMath::Abs(Turn) > 0.03 ? (Turn > 0.0 ? -1.0 : 1.0) : (Chance.FRand() < 0.5f ? -1.0 : 1.0);
				const double Aside = Inside * (HalfAt[K] - 0.5 * Wide - 0.07 * RiverWide);
				Lens(RunAt[K], Long, Wide, Aside, Sand, WetSand, 0.03);
				if (++Bars % 3 == 0)
				{
					Lens(RunAt[K] + Long * 0.22, Long * 0.5, Wide * 0.5, Aside, Tuft, Tuft, 0.05);
				}
			}
		}
	}
	if (!WaterMaterial)
	{
		WaterMaterial = UMaterialInstanceDynamic::Create(SurfaceMaterial, this);
		WaterMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.18f);
		WaterMaterial->SetScalarParameterValue(TEXT("Specular"), 0.6f);
		WaterMaterial->SetScalarParameterValue(TEXT("Grain"), 0.0f);
		WaterMaterial->SetScalarParameterValue(TEXT("Tooth"), 0.0f);
	}
	bRiverInksKept[0] = Buffers.Commit(RiverMesh, 0, WaterMaterial);
	bRiverInksKept[1] = false;
	if (BuffersBeyond.Vertices.Num() > 0)
	{
		bRiverInksKept[1] = BuffersBeyond.Commit(RiverMesh, 1, WaterMaterial);
		RiverMesh->SetMeshSectionVisible(1, bRiversBeyondShown);
	}
	const FCampaignMeshBuffers* Laid[2] = { &Buffers, &BuffersBeyond };
	for (int32 Kind = 0; Kind < 2; ++Kind)
	{
		// (kept only where they are this section's points, one for one)
		bRiverInksKept[Kind] = bRiverInksKept[Kind] && bKeepInks && PlainInks[Kind].Num() == Laid[Kind]->Vertices.Num() && InkRegions[Kind].Num() == Laid[Kind]->Vertices.Num();
		RiverPlainInks[Kind] = bRiverInksKept[Kind] ? MoveTemp(PlainInks[Kind]) : TArray<FColor>();
		RiverInkRegions[Kind] = bRiverInksKept[Kind] ? MoveTemp(InkRegions[Kind]) : TArray<int32>();
	}
	bRiverInksFor = bKeepInks;
	UE_LOG(LogTheater, Log, TEXT("Rivers: %d, in %d points (and %d beyond Europe)"), Land.Rivers.Num(), Buffers.Vertices.Num(), BuffersBeyond.Vertices.Num());
}

bool ACampaignMapActor::RecolourRivers()
{
	THEATER_SPEED_SCOPE(RecolourRivers);
	if (!RiverMesh || !bRiverInksFor)
	{
		return false;
	}
	for (int32 Section = 0; Section < 2; ++Section)
	{
		// (the inks kept are for the points the section has: if anything else has laid it since, it is laid again whole)
		const FProcMeshSection* Laid = Section < RiverMesh->GetNumSections() ? RiverMesh->GetProcMeshSection(Section) : nullptr;
		const int32 Points = Laid ? Laid->ProcVertexBuffer.Num() : 0;
		if (bRiverInksKept[Section] ? Points != RiverPlainInks[Section].Num() : Points > 0)
		{
			return false;
		}
	}
	// (the same darkening BuildWaterways gives a point under the haze, by day or by night as it is now)
	bRiversNightFor = NightNow > 0.5f;
	const FVector RiverHaze = bRiversNightFor ? FVector(0.47, 0.54, 0.63) : FVector(0.7, 0.66, 0.72);
	TArray<FColor> Inks;
	TArray<FVector> Positions;
	for (int32 Section = 0; Section < 2; ++Section)
	{
		if (!bRiverInksKept[Section])
		{
			continue;
		}
		// (5.8 changes a section only when it is given all its points, the same number: they are given again as they
		// are, and so its box is the same box)
		const FProcMeshSection* Section_ = RiverMesh->GetProcMeshSection(Section);
		Positions.SetNumUninitialized(Section_->ProcVertexBuffer.Num());
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			Positions[Index] = FVector(Section_->ProcVertexBuffer[Index].Position);
		}
		const TArray<FColor>& Plain = RiverPlainInks[Section];
		const TArray<int32>& Where = RiverInkRegions[Section];
		Inks.SetNumUninitialized(Plain.Num());
		for (int32 Index = 0; Index < Plain.Num(); ++Index)
		{
			const FColor& Laid = Plain[Index];
			const bool bUnderHaze = Where[Index] != INDEX_NONE && HazeSet.Num() > 0 && HazeSet.Contains(Where[Index]);
			Inks[Index] = bUnderHaze ? FColor(static_cast<uint8>(Laid.R * RiverHaze.X), static_cast<uint8>(Laid.G * RiverHaze.Y), static_cast<uint8>(Laid.B * RiverHaze.Z), Laid.A) : Laid;
		}
		// (the inks changed; the points, and so the bounds, as they were; normals and the rest left as they are)
		RiverMesh->UpdateMeshSection(Section, Positions, TArray<FVector>(), TArray<FVector2D>(), Inks, TArray<FProcMeshTangent>());
	}
	RiversHazedFor = HazeSet;
	if (OverSeam())
	{
		LayOverSeam(RiverMesh); // (seam-2: the rivers' twin over the seam takes the new inks; its keeper looks at a few points only)
	}
	return true;
}

void ACampaignMapActor::MakeUpTrain(FTrain& Train, int32 FromRegion) const
{
	const int32 Level = ShownBuildings.IsValidIndex(FromRegion) ? FMath::Clamp(static_cast<int32>(ShownBuildings[FromRegion].Z), 0, 3) : 0;
	if (Level == Train.FlakLevel || !Train.Mesh || Train.Mesh->GetNumSections() < 4)
	{
		return;
	}
	Train.FlakLevel = Level;
	Train.Mesh->SetMeshSectionVisible(1, Level < 3); // (the box wagon behind the tender gives its place to the second flak wagon)
	Train.Mesh->SetMeshSectionVisible(2, Level >= 1);
	Train.Mesh->SetMeshSectionVisible(3, Level >= 3);
}

void ACampaignMapActor::UpdateTrains(const FVector& CameraFocus, float CameraDistance)
{
	THEATER_SPEED_SCOPE(UpdateTrains);
	UWorld* World = GetWorld();
	if (!World || Land.Rails.Num() == 0)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const double U = UnitScale;
	const double LinkLasts = 11.0; // seconds from one town to the next
	const FVector2D Looking = WorldToMap(CameraFocus);
	// (seam-4: a line a little way over the chart's seam is near the eye too. Its place is counted in the turn of the
	// world nearest the eye, and its train is shown there, on the railway's twin. Only while both sides are drawn.)
	const bool bBothSides = OverSeam();
	auto SeenFrom = [this, bBothSides, &Looking](const FVector2D& OnLine)
	{
		return FVector2D(bBothSides ? NearX(OnLine.X, Looking.X) : OnLine.X, OnLine.Y);
	};
	// From far off the trains are too small to see, and are put away.
	const bool bNear = CameraDistance < 9000.0f;
	// (Speed: "tc.Speed.TrainShadows", for measuring only. A thing that throws a shadow and moves has the shadows round
	// it drawn again every frame, with everything else that throws one there.)
	const bool bTrainShadows = TheaterSpeed::TrainShadows();
	if (bTrainShadows != bTrainShadowsCast)
	{
		bTrainShadowsCast = bTrainShadows;
		for (FTrain& Each : Trains)
		{
			if (Each.Mesh)
			{
				Each.Mesh->SetCastShadow(bTrainShadows);
			}
		}
	}

	// The trains running: each along its link, and on to another when it arrives.
	for (int32 Index = Trains.Num() - 1; Index >= 0; --Index)
	{
		FTrain& Train = Trains[Index];
		double Share = (Now - Train.Began) / LinkLasts;
		const bool bGone = !bNear || !Land.Rails.IsValidIndex(Train.Rail) || ShownCut.Contains(Land.Rails[Train.Rail].A) || ShownCut.Contains(Land.Rails[Train.Rail].B)
			|| HazeSet.Contains(Land.Rails[Train.Rail].A) || HazeSet.Contains(Land.Rails[Train.Rail].B) // (nothing moves under the haze)
			|| FVector2D::DistSquared(SeenFrom(Land.Rails[Train.Rail].At), Looking) > 260.0 * 260.0;
		if (!bGone && Share >= 1.0 && Data.Regions.IsValidIndex(Land.Rails[Train.Rail].A) && Data.Regions.IsValidIndex(Land.Rails[Train.Rail].B))
		{
			// Arrived. (Brian, 16 November 2026: "can the trains turn on the tracks? when they get to the end of the line
			// they just vanish".) It goes on by another line out of this town where there is one, swinging onto it as it
			// stands at the station. At the end of the line, and now and then anywhere, it is turned round where it
			// stands, about its own middle, and goes back the way it came. It never vanishes at a station.
			const FLandRail& Came = Land.Rails[Train.Rail];
			const int32 Here = Train.bBackward ? Came.A : Came.B;
			const FVector2D Station = Data.Regions[Here].Center;
			const FVector WayIn = (MapToWorld(Station.X, Station.Y) - MapToWorld(Came.At.X, Came.At.Y)).GetSafeNormal2D();
			TArray<int32> Onward;
			for (int32 Other = 0; Other < Land.Rails.Num(); ++Other)
			{
				if (Other != Train.Rail && (Land.Rails[Other].A == Here || Land.Rails[Other].B == Here) && !ShownCut.Contains(Land.Rails[Other].A) && !ShownCut.Contains(Land.Rails[Other].B)
					&& !HazeSet.Contains(Land.Rails[Other].A) && !HazeSet.Contains(Land.Rails[Other].B)
					&& Data.Regions.IsValidIndex(Land.Rails[Other].A) && Data.Regions.IsValidIndex(Land.Rails[Other].B))
				{
					Onward.Add(Other);
				}
			}
			++Train.Links;
			Train.TurnBegan = Now;
			Train.TurnFrom = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(WayIn.Y, WayIn.X)));
			if (Onward.Num() > 0 && FMath::FRand() > 0.18f)
			{
				Train.Rail = Onward[FMath::RandHelper(Onward.Num())];
				Train.bBackward = Land.Rails[Train.Rail].B == Here;
				const FLandRail& Next = Land.Rails[Train.Rail];
				const FVector WayOut = (MapToWorld(Next.At.X, Next.At.Y) - MapToWorld(Station.X, Station.Y)).GetSafeNormal2D();
				Train.TurnTo = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(WayOut.Y, WayOut.X)));
				Train.bPivotMiddle = false;
				Train.StartAlong = 0.0;
				// (it stands a moment and swings onto its new line: longer the sharper the turn)
				Train.Began = Now + 0.5 + 0.9 * FMath::Abs(FMath::FindDeltaAngleDegrees(Train.TurnFrom, Train.TurnTo)) / 180.0;
			}
			else
			{
				// Turned round where it stands: about its own middle, which is some two and a half of the map's units
				// behind its engine. It then stands twice that far out of the town on the line it came by, facing back.
				const double MiddleBack = 2.6;
				Train.bBackward = !Train.bBackward;
				Train.TurnTo = Train.TurnFrom + 180.0f;
				Train.bPivotMiddle = true;
				const FVector2D Middle = Station - (Station - Came.At).GetSafeNormal() * MiddleBack;
				Train.Pivot = SurfacePointOver(Middle.X, Middle.Y, 0.2);
				Train.StartAlong = 2.0 * MiddleBack;
				Train.Began = Now + 2.4;
			}
			MakeUpTrain(Train, Here);
			{
				// (for whoever checks the trains from the log: how many have gone on, and how many were turned round)
				static int32 WentOn = 0, TurnedRound = 0;
				(Train.bPivotMiddle ? TurnedRound : WentOn) += 1;
				if ((WentOn + TurnedRound) % 12 == 1)
				{
					UE_LOG(LogTheater, Log, TEXT("Trains: %d have gone on by another line and %d have been turned round and sent back, the last at %s; none vanishes at a station"), WentOn, TurnedRound, *Data.Regions[Here].Name);
				}
			}
			Share = 0.0;
		}
		if (bGone)
		{
			if (Train.Mesh)
			{
				Train.Mesh->DestroyComponent();
			}
			Trains.RemoveAt(Index);
			continue;
		}
		const FLandRail& Rail = Land.Rails[Train.Rail];
		if (!Data.Regions.IsValidIndex(Rail.A) || !Data.Regions.IsValidIndex(Rail.B))
		{
			continue;
		}
		const FVector2D From = Data.Regions[Train.bBackward ? Rail.B : Rail.A].Center, To = Data.Regions[Train.bBackward ? Rail.A : Rail.B].Center;
		const double First = FVector2D::Distance(From, Rail.At), Second = FVector2D::Distance(Rail.At, To);
		// It draws out of the station and slows into the next. (A train just turned about its middle sets out from
		// where the turn left it, a little way out of the town.)
		const double Started = FMath::Min(Train.StartAlong, 0.4 * (First + Second));
		const double Along = Started + FMath::SmoothStep(0.0, 1.0, FMath::Clamp(Share, 0.0, 1.0)) * (First + Second - Started);
		const FVector2D A = Along < First ? From : Rail.At, B = Along < First ? Rail.At : To;
		const double Leg = Along < First ? First : Second, Done = Along < First ? Along : Along - First;
		const FVector2D At = FMath::Lerp(A, B, Leg > 0.01 ? Done / Leg : 0.0);
		const FVector Here = SurfacePointOver(At.X, At.Y, 0.2);
		const FVector Ahead = (MapToWorld(B.X, B.Y) - MapToWorld(A.X, A.Y)).GetSafeNormal2D();
		if (Train.Mesh)
		{
			FVector Stands = Here;
			double Heading = FMath::RadiansToDegrees(FMath::Atan2(Ahead.Y, Ahead.X));
			if (Train.TurnBegan >= 0.0 && Now < Train.Began)
			{
				// (standing at the station, being turned: it eases from the heading it came in on to the one it leaves by)
				const double Turned = FMath::SmoothStep(0.0, 1.0, (Now - Train.TurnBegan) / FMath::Max(Train.Began - Train.TurnBegan - 0.3, 0.1));
				Heading = Train.TurnFrom + FMath::FindDeltaAngleDegrees(Train.TurnFrom, Train.TurnTo) * Turned;
				if (Train.bPivotMiddle)
				{
					// (about its own middle, which stays where it stood)
					Heading = Train.TurnFrom + 180.0 * Turned;
					Stands = Train.Pivot + FRotator(0.0, Heading, 0.0).RotateVector(FVector(2.6 * U, 0.0, 0.0));
				}
			}
			Stands.Y += (SeenFrom(Rail.At).X - Rail.At.X) * U; // (seam-4: a whole number of worlds east or west, or nothing)
			Train.Mesh->SetRelativeLocationAndRotation(Stands, FRotator(0.0, Heading, 0.0));
		}
	}

	// A new train now and then, on a line near what the player is looking at.
	if (!bNear || Trains.Num() >= 7 || Now < NextTrain)
	{
		return;
	}
	NextTrain = Now + FMath::FRandRange(0.8, 2.5);
	// (on the lines that are on the screen, or nearly: the closer the eye, the nearer the trains are started)
	const double Reach = FMath::Clamp(0.42 * CameraDistance / U, 30.0, 170.0);
	TArray<int32> Near;
	for (int32 Index = 0; Index < Land.Rails.Num(); ++Index)
	{
		const FLandRail& Rail = Land.Rails[Index];
		if (Data.Regions.IsValidIndex(Rail.A) && Data.Regions.IsValidIndex(Rail.B) && FVector2D::DistSquared(SeenFrom(Rail.At), Looking) < Reach * Reach && !ShownCut.Contains(Rail.A) && !ShownCut.Contains(Rail.B)
			&& !HazeSet.Contains(Rail.A) && !HazeSet.Contains(Rail.B)
			&& !Trains.ContainsByPredicate([Index](const FTrain& Each) { return Each.Rail == Index; }))
		{
			Near.Add(Index);
		}
	}
	if (Near.Num() == 0)
	{
		return;
	}
	// The train: a black engine with its funnel and red wheels, a tender, and three wagons, its front toward +X.
	FMiniatureMesh Model;
	const FLinearColor Black(FColor::FromHex(TEXT("1B1B1D"))), Red(FColor::FromHex(TEXT("8E2A22"))), Brass(FColor::FromHex(TEXT("B08D57")));
	const FLinearColor Wagons[] = { FLinearColor(FColor::FromHex(TEXT("5B4634"))), FLinearColor(FColor::FromHex(TEXT("4E5A4A"))), FLinearColor(FColor::FromHex(TEXT("6A3C2E"))), FLinearColor(FColor::FromHex(TEXT("55595E"))) };
	const double W = 0.62 * U;
	Model.Rod(FVector(0.2 * U, 0.0, 0.62 * U), FVector(1.75 * U, 0.0, 0.62 * U), 0.3 * U, Black, 10);
	Model.Box(FVector(-0.2 * U, 0.0, 0.66 * U), FVector(0.75 * U, W, 0.92 * U), Black);
	Model.Box(FVector(-0.2 * U, 0.0, 1.15 * U), FVector(0.9 * U, W * 1.1, 0.08 * U), Black * 1.6f);
	Model.Pillar(FVector(1.45 * U, 0.0, 0.9 * U), 0.1 * U, 0.14 * U, 0.4 * U, Black, 8);
	Model.Pillar(FVector(0.95 * U, 0.0, 0.9 * U), 0.12 * U, 0.1 * U, 0.16 * U, Brass, 8);
	Model.Box(FVector(0.75 * U, 0.0, 0.22 * U), FVector(2.3 * U, W * 0.95, 0.3 * U), Red);
	Model.Box(FVector(-1.25 * U, 0.0, 0.5 * U), FVector(1.0 * U, W, 0.66 * U), Black);
	const int32 Sort = FMath::RandHelper(4);
	// (the wagon behind the tender is a piece of its own: at level 3 a flak wagon rides in its place)
	FMiniatureMesh FirstWagon, TailFlak, SecondFlak;
	for (int32 Wagon = 0; Wagon < 3; ++Wagon)
	{
		const double X = (-2.75 - Wagon * 1.7) * U;
		FMiniatureMesh& Into = Wagon == 0 ? FirstWagon : Model;
		Into.Box(FVector(X, 0.0, 0.52 * U), FVector(1.5 * U, W, 0.7 * U), Wagons[(Sort + Wagon) % 4]);
		Into.Box(FVector(X, 0.0, 0.12 * U), FVector(1.3 * U, W * 0.8, 0.14 * U), Black);
	}
	const int32 StartRail = Near[FMath::RandHelper(Near.Num())];
	const bool bStartBackward = FMath::RandBool();
	const int32 StartRegion = bStartBackward ? Land.Rails[StartRail].B : Land.Rails[StartRail].A;
	{
		// THE FLAK WAGON (the art director's guard-the-line brief, section 4; our own design): a flat wagon as long as a
		// box wagon, its deck at three tenths of a box wagon's height, low plank sides and a locker at each end;
		// amidships a gun on a pedestal, one barrel 0.62 of a wagon long raised 64 degrees and leaning back along the
		// train, a shield on the muzzle side only. The wagon in the train's own paint, the gun in the nation's vehicle
		// paint. No emblem. It never fires and shows no light.
		const FLinearColor Planks = Wagons[Sort % 4];
		const FLinearColor GunPaint = FMiniatureMesh::VehicleColour(Data.Regions[StartRegion].Owner) * 1.3f;
		auto FlakWagon = [&](FMiniatureMesh& Into, double X)
		{
			Into.Box(FVector(X, 0.0, 0.12 * U), FVector(1.3 * U, W * 0.8, 0.14 * U), Black);
			Into.Box(FVector(X, 0.0, 0.22 * U), FVector(1.5 * U, W, 0.06 * U), Planks * 0.8f);
			for (const double Edge : { -0.47, 0.47 })
			{
				Into.Box(FVector(X, Edge * W, 0.31 * U), FVector(1.5 * U, 0.05 * U, 0.14 * U), Planks);
			}
			for (const double End : { -0.61, 0.61 })
			{
				Into.Box(FVector(X + End * U, 0.0, 0.34 * U), FVector(0.26 * U, W * 0.82, 0.2 * U), Planks * 0.9f);
			}
			Into.Pillar(FVector(X, 0.0, 0.25 * U), 0.15 * U, 0.1 * U, 0.3 * U, GunPaint, 8);
			const FVector Breech(X + 0.08 * U, 0.0, 0.58 * U);
			const double Raised = FMath::DegreesToRadians(64.0);
			Into.Rod(Breech, Breech + FVector(-FMath::Cos(Raised), 0.0, FMath::Sin(Raised)) * (0.93 * U), 0.075 * U, GunPaint, 6);
			Into.Box(Breech, FVector(0.28 * U, 0.3 * U, 0.2 * U), GunPaint);
			Into.Box(FVector(X - 0.14 * U, 0.0, 0.66 * U), FVector(0.05 * U, 0.52 * U, 0.36 * U), GunPaint * 0.92f);
		};
		FlakWagon(TailFlak, (-2.75 - 3 * 1.7) * U);
		FlakWagon(SecondFlak, -2.75 * U);
	}
	FTrain& Train = Trains.AddDefaulted_GetRef();
	Train.Rail = StartRail;
	Train.bBackward = bStartBackward;
	Train.Began = Now;
	Train.Mesh = NewObject<UProceduralMeshComponent>(this);
	Train.Mesh->SetupAttachment(Root);
	Train.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Train.Mesh->RegisterComponent();
	Train.Mesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
	Train.Mesh->SetMaterial(0, SceneryMaterial);
	{
		int32 Section = 1;
		for (const FMiniatureMesh* Part : { &FirstWagon, &TailFlak, &SecondFlak })
		{
			Train.Mesh->CreateMeshSection(Section, Part->Vertices, Part->Triangles, Part->Normals, TArray<FVector2D>(), Part->Colors, TArray<FProcMeshTangent>(), false);
			Train.Mesh->SetMaterial(Section, SceneryMaterial);
			++Section;
		}
		Train.Mesh->SetMeshSectionVisible(2, false);
		Train.Mesh->SetMeshSectionVisible(3, false);
		Train.FlakLevel = 0;
		MakeUpTrain(Train, StartRegion);
	}
	Train.Mesh->SetCastShadow(TheaterSpeed::TrainShadows()); // ("tc.Speed.TrainShadows 0" is for measuring only)
	Train.Mesh->SetVisibility(false); // until its first place on the line is set, at the next frame
	Train.Mesh->SetVisibility(true);
	Train.Mesh->SetRelativeLocation(SurfacePointOver(Data.Regions[Train.bBackward ? Land.Rails[Train.Rail].B : Land.Rails[Train.Rail].A].Center.X, Data.Regions[Train.bBackward ? Land.Rails[Train.Rail].B : Land.Rails[Train.Rail].A].Center.Y, 0.2));
}

void ACampaignMapActor::BuildRailways()
{
	RailMesh->ClearAllMeshSections();
	// A railway runs from the middle of one region to the middle of the next through the point where it
	// crosses their border: a dark line with sleepers across it, as railways are drawn on maps.
	// (A fine brown line, hidden from high up; its sleepers are seen only close to. See UpdateForCamera.)
	const FColor Iron = FromHex(TEXT("34261E")).ToFColor(false);
	const double Lift = 0.16, Half = 0.14;
	FCampaignMeshBuffers Buffers, Sleepers;
	for (const FLandRail& Rail : Land.Rails)
	{
		if (!Data.Regions.IsValidIndex(Rail.A) || !Data.Regions.IsValidIndex(Rail.B))
		{
			continue;
		}
		const FVector2D Stops[3] = { Data.Regions[Rail.A].Center, Rail.At, Data.Regions[Rail.B].Center };
		for (int32 Leg = 0; Leg < 2; ++Leg)
		{
			const FVector2D A = Stops[Leg], B = Stops[Leg + 1];
			const double Length = FVector2D::Distance(A, B);
			if (Length < 0.5)
			{
				continue;
			}
			const FVector2D Dir = (B - A) / Length, Side(Dir.Y, -Dir.X);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Length / 1.6));
			int32 PrevLeft = INDEX_NONE, PrevRight = INDEX_NONE;
			for (int32 K = 0; K <= Steps; ++K)
			{
				const FVector2D P = A + Dir * (Length * K / Steps);
				const int32 Left = Buffers.AddVertex(SurfacePointOver(P.X + Side.X * Half, P.Y + Side.Y * Half, Lift), FVector::UpVector, Iron);
				const int32 Right = Buffers.AddVertex(SurfacePointOver(P.X - Side.X * Half, P.Y - Side.Y * Half, Lift), FVector::UpVector, Iron);
				if (PrevLeft != INDEX_NONE)
				{
					Buffers.AddTriangleFacingUp(PrevLeft, PrevRight, Left);
					Buffers.AddTriangleFacingUp(PrevRight, Left, Right);
				}
				PrevLeft = Left;
				PrevRight = Right;
				// A sleeper across the line at every step.
				const FVector2D S0 = P + Side * 0.55, S1 = P - Side * 0.55, Back = Dir * 0.13;
				const int32 T0 = Sleepers.AddVertex(SurfacePointOver(S0.X - Back.X, S0.Y - Back.Y, Lift), FVector::UpVector, Iron);
				const int32 T1 = Sleepers.AddVertex(SurfacePointOver(S1.X - Back.X, S1.Y - Back.Y, Lift), FVector::UpVector, Iron);
				const int32 T2 = Sleepers.AddVertex(SurfacePointOver(S0.X + Back.X, S0.Y + Back.Y, Lift), FVector::UpVector, Iron);
				const int32 T3 = Sleepers.AddVertex(SurfacePointOver(S1.X + Back.X, S1.Y + Back.Y, Lift), FVector::UpVector, Iron);
				Sleepers.AddTriangleFacingUp(T0, T1, T2);
				Sleepers.AddTriangleFacingUp(T1, T2, T3);
			}
		}
	}
	Buffers.Commit(RailMesh, 0, RailInk);
	Sleepers.Commit(RailMesh, 1, SleeperInk);
	UE_LOG(LogTheater, Log, TEXT("Railways: %d links, in %d points"), Land.Rails.Num(), Buffers.Vertices.Num());
}

void ACampaignMapActor::BuildWorks()
{
	WorksMesh->ClearAllMeshSections();
	bWorksShown = true;
	FMiniatureMesh Model;
	const double U = UnitScale;
	const FLinearColor Stone(FColor::FromHex(TEXT("8E887C"))), Shadow(FColor::FromHex(TEXT("5E5A52"))), Scorch(FColor::FromHex(TEXT("2A2623"))), Rust(FColor::FromHex(TEXT("8A3A2A")));
	// A bridge at every crossing, lying along the road from one region to the other: three arches of stone.
	// One that has been blown has lost its middle span.
	for (int32 K = 0; K < Land.Crossings.Num(); ++K)
	{
		const FLandCrossing& Crossing = Land.Crossings[K];
		if (!Data.Regions.IsValidIndex(Crossing.A) || !Data.Regions.IsValidIndex(Crossing.B))
		{
			continue;
		}
		const FVector2D Way = (Data.Regions[Crossing.B].Center - Data.Regions[Crossing.A].Center).GetSafeNormal();
		const FVector A = MapToWorld(0.0, 0.0), B = MapToWorld(Way.X, Way.Y);
		const double Turn = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
		const FVector Middle = SurfacePointOver(Crossing.At.X, Crossing.At.Y, 0.12);
		const FQuat Facing(FRotator(0.0, Turn, 0.0));
		const bool bDown = ShownDown.Contains(K);
		{
			// (The art director's round 33: there were no bridges over the Amazon. On a river as broad as that the
			// crossing is a ferry: a flat boat lying across the stream, and a landing stage on either bank.)
			double Nearest = 12.0, BroadThere = 0.0;
			for (int32 R = 0; R < RiverPaths.Num(); ++R)
			{
				for (const FVector2D& Point : RiverPaths[R])
				{
					const double Far = FVector2D::Distance(Point, Crossing.At);
					if (Far < Nearest)
					{
						Nearest = Far;
						BroadThere = RiverBroad.IsValidIndex(R) ? RiverBroad[R] : 0.0;
					}
				}
			}
			// (round 34: the lower Madeira's plank bridge takes the ferry too. A river a unit broad or more is a great
			// one of the jungle: the others that broad are none of them bridged by the rules.)
			if (BroadThere >= 1.0)
			{
				const FLinearColor Hull(FColor::FromHex(TEXT("4A3A2A"))), Deck(FColor::FromHex(TEXT("D8CFB4")));
				Model.Box(Middle + FVector(0.0, 0.0, 0.14 * U), FVector(2.6, 0.95, 0.28) * U, Hull, Turn);
				Model.Box(Middle + FVector(0.0, 0.0, 0.34 * U), FVector(2.1, 0.72, 0.12) * U, Deck, Turn);
				Model.Box(Middle + Facing.RotateVector(FVector(-0.55 * U, 0.0, 0.6 * U)), FVector(0.6, 0.52, 0.42) * U, Deck, Turn);
				for (const double End : { -1.0, 1.0 })
				{
					Model.Box(Middle + Facing.RotateVector(FVector(End * (BroadThere + 0.9) * U, 0.0, 0.06 * U)), FVector(0.9, 0.5, 0.12) * U, Stone, Turn);
				}
				continue;
			}
		}
		for (int32 Span = -1; Span <= 1; ++Span)
		{
			const FVector At = Middle + Facing.RotateVector(FVector(Span * 1.5 * U, 0.0, 0.0));
			if (bDown && Span == 0)
			{
				// The fallen span: a broken slab in the water, scorched.
				Model.Box(At + FVector(0.0, 0.0, 0.05 * U), FVector(1.2, 0.9, 0.12) * U, Scorch, Turn, 18.0);
				continue;
			}
			Model.Box(At + FVector(0.0, 0.0, 0.42 * U), FVector(1.5, 1.0, 0.16) * U, Stone, Turn);
			Model.Box(At + FVector(0.0, 0.0, 0.2 * U), FVector(0.34, 1.0, 0.4) * U, Shadow, Turn);
			for (const double Edge : { -1.0, 1.0 })
			{
				Model.Box(At + Facing.RotateVector(FVector(0.0, Edge * 0.47 * U, 0.56 * U)), FVector(1.5, 0.06, 0.12) * U, Shadow, Turn);
			}
		}
	}
	// Where a railway is cut: a red bar across the line at the middle of the region.
	for (const int32 RegionId : ShownCut)
	{
		if (Data.Regions.IsValidIndex(RegionId))
		{
			// (The art director's round 38: in a town the board read as a red cross on a building. It stands with its
			// lamp on open track, halfway between the town's last house and the border.)
			const FVector2D Stands = CutSpot(RegionId);
			const FVector Foot = SurfacePointOver(Stands.X, Stands.Y, 0.1);
			const FVector At = Foot + FVector(0.0, 0.0, 1.9 * U);
			Model.Pillar(Foot, 0.12 * U, 0.12 * U, 1.9 * U, Shadow, 6);
			Model.Box(At, FVector(0.3, 2.6, 0.45) * U, Rust, 0.0, 0.0);
			Model.Box(At + FVector(0.0, 0.0, 0.0), FVector(0.32, 0.45, 2.6) * U, Rust, 0.0, 0.0);
			// and the rails themselves lifted and bent beside it
			Model.Box(Foot + FVector(0.0, 1.6 * U, 0.2 * U), FVector(2.4, 0.12, 0.12) * U, Scorch, 30.0, 14.0);
			Model.Box(Foot + FVector(0.0, 2.0 * U, 0.2 * U), FVector(2.4, 0.12, 0.12) * U, Scorch, 12.0, -10.0);
		}
	}
	// Country too steep for tanks is marked with a pair of snow-capped peaks, so it can be told at a glance.
	{
		const FLinearColor Rock(FColor::FromHex(TEXT("6F6A62"))), Snow(FColor::FromHex(TEXT("ECEEF0")));
		for (const int32 RegionId : Land.Steep)
		{
			if (!Data.Regions.IsValidIndex(RegionId))
			{
				continue;
			}
			const FVector2D Spot = Data.Regions[RegionId].Center + FVector2D(4.0, 5.0) * TownSpread(RegionId);
			const FVector Foot = SurfacePoint(Spot.X, Spot.Y, 0.02);
			const FVector Peaks[2] = { Foot, Foot + FVector(0.9 * U, 1.5 * U, 0.0) };
			const double Heights[2] = { 3.0 * U, 2.2 * U };
			for (int32 Peak = 0; Peak < 2; ++Peak)
			{
				const double Height = Heights[Peak], Radius = Height * 0.48;
				Model.Pillar(Peaks[Peak], Radius, 0.0, Height, Rock, 7);
				Model.Pillar(Peaks[Peak] + FVector(0.0, 0.0, Height * 0.62), Radius * 0.385, 0.0, Height * 0.385, Snow, 7);
			}
		}
	}
	// A city's own bridge: where a river passes a city or a capital, a small bridge of pale dressed stone on two
	// low arches, humped in the middle, with a lamp at each end. It is part of the town and nothing in the rules:
	// it is half the size of a crossing's bridge, the colour of the town's own stone where a crossing's is grey,
	// and it stands only where no crossing is near.
	int32 TownBridges = 0;
	FString TownBridgeNames;
	{
		// (The art director's ruling: the stone read as fresh plaster. It is a warm grey-buff now, its parapets a step
		// darker, and the openings under the arches are near-black and stand a little proud of the roadway's sides,
		// so that the arches read.)
		const FLinearColor Ashlar(FColor::FromHex(TEXT("968B75"))), AshlarDark(FColor::FromHex(TEXT("7C715E"))), Under(FColor::FromHex(TEXT("141210"))), Lamp(FColor::FromHex(TEXT("2E2B28")));
		for (const FCampaignRegion& Region : Data.Regions)
		{
			if (!(Region.bCapital || Region.Terrain == TEXT("urban") || Region.VictoryPoints >= 2 || Region.Factories >= 2))
			{
				continue;
			}
			// (Where the river runs under the town's own roofs a bridge at its nearest point is buried in the houses:
			// it stands instead where the river comes out past the last of them, or, for a river that only passes
			// the town, at its nearest.)
			const double Reach = TownRadius(Region.Id) + 1.4, Rim = TownRadius(Region.Id) + 0.7;
			double Nearest = Reach, Fit = Reach;
			FVector2D At = FVector2D::ZeroVector, Across = FVector2D(1.0, 0.0);
			double Broad = 0.425;
			for (int32 R = 0; R < RiverPaths.Num(); ++R)
			{
				const TArray<FVector2D>& Path = RiverPaths[R];
				for (int32 K = 0; K + 1 < Path.Num(); ++K)
				{
					for (int32 Step = 0; Step < 6; ++Step)
					{
						const FVector2D Point = FMath::Lerp(Path[K], Path[K + 1], Step / 6.0);
						const double Far = FVector2D::Distance(Point, Region.Center);
						if (Far < Reach && FMath::Abs(Far - Rim) < Fit && GroundAt(Point.X, Point.Y) > 0.0f)
						{
							Fit = FMath::Abs(Far - Rim);
							Nearest = Far;
							At = Point;
							const FVector2D Along = (Path[K + 1] - Path[K]).GetSafeNormal();
							Across = FVector2D(Along.Y, -Along.X);
							Broad = RiverBroad.IsValidIndex(R) ? RiverBroad[R] : 0.425;
						}
					}
				}
			}
			if (Nearest >= Reach)
			{
				continue; // no river passes it
			}
			bool bCrossingNear = false;
			for (const FLandCrossing& Crossing : Land.Crossings)
			{
				bCrossingNear |= FVector2D::Distance(Crossing.At, At) < 4.0;
			}
			if (bCrossingNear)
			{
				continue; // the road's own bridge is there
			}
			const FVector A = MapToWorld(0.0, 0.0), B = MapToWorld(Across.X, Across.Y);
			const double Turn = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
			const FQuat Facing(FRotator(0.0, Turn, 0.0));
			const FVector Middle = SurfacePointOver(At.X, At.Y, 0.06);
			const double Long = (2.0 * Broad + 1.3) * U, Wide = 0.5 * U;
			auto On = [&](double Along, double Side, double Up) { return Middle + Facing.RotateVector(FVector(Along * Long, Side * Wide, 0.0)) + FVector(0.0, 0.0, Up * U); };
			// (the roadway in five lengths, rising to the middle; the piers under it; the parapets; the lamps)
			for (int32 Piece = -2; Piece <= 2; ++Piece)
			{
				const double Rise = 0.3 - 0.045 * Piece * Piece;
				Model.Box(On(Piece * 0.2, 0.0, Rise), FVector(Long * 0.21, Wide, 0.1 * U), Ashlar, Turn, -Piece * 7.0);
				for (const double Edge : { -1.0, 1.0 })
				{
					Model.Box(On(Piece * 0.2, Edge * 0.46, Rise + 0.09), FVector(Long * 0.21, Wide * 0.1, 0.09 * U), AshlarDark, Turn, -Piece * 7.0);
				}
			}
			for (const double Pier : { -0.5, 0.0, 0.5 })
			{
				Model.Box(On(Pier, 0.0, 0.1), FVector(Long * 0.09, Wide * 1.08, 0.3 * U), Pier == 0.0 ? AshlarDark : Ashlar, Turn);
			}
			for (const double Arch : { -0.25, 0.25 })
			{
				Model.Box(On(Arch, 0.0, 0.09), FVector(Long * 0.32, Wide * 1.1, 0.16 * U), Under, Turn);
			}
			for (const double End : { -1.0, 1.0 })
			{
				for (const double Edge : { -1.0, 1.0 })
				{
					Model.Rod(On(End * 0.5, Edge * 0.46, 0.12), On(End * 0.5, Edge * 0.46, 0.52), 0.016 * U, Lamp, 4);
					Model.Box(On(End * 0.5, Edge * 0.46, 0.55), FVector(0.05, 0.05, 0.06) * U, FLinearColor(FColor::FromHex(TEXT("E8D9A0"))), Turn);
				}
			}
			++TownBridges;
			UE_LOG(LogTheater, Verbose, TEXT("A town bridge at %s"), *Region.Name);
			TownBridgeNames += (TownBridgeNames.IsEmpty() ? TEXT("") : TEXT(", ")) + FString::Printf(TEXT("%s (%.0f,%.0f)"), *Region.Name, At.X, At.Y);
		}
	}
	UE_LOG(LogTheater, Log, TEXT("Towns with a bridge of their own: %s"), *TownBridgeNames);
	if (Model.Vertices.Num() > 0)
	{
		WorksMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		WorksMesh->SetMaterial(0, SceneryMaterial);
		SeamLaidAgain(WorksMesh); // (seam-2: its twin over the chart's seam is laid again)
	}
	UE_LOG(LogTheater, Log, TEXT("Works: %d crossings (%d bridges down), %d railways cut, %d towns' own bridges"), Land.Crossings.Num(), ShownDown.Num(), ShownCut.Num(), TownBridges);
}

UStaticMeshComponent* ACampaignMapActor::AddBlock(const FVector& CenterWorld, const FVector& SizeWorld, UMaterialInterface* Material)
{
	UStaticMeshComponent* Block = NewObject<UStaticMeshComponent>(this);
	Block->SetupAttachment(Root);
	Block->SetStaticMesh(CubeMesh);
	Block->SetMaterial(0, Material);
	Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Block->SetRelativeLocation(CenterWorld);
	Block->SetRelativeScale3D(SizeWorld / 100.0); // the engine's cube is one metre on a side
	Block->RegisterComponent();
	Blocks.Add(Block);
	return Block;
}

void ACampaignMapActor::BuildTable()
{
	for (UStaticMeshComponent* Block : Blocks)
	{
		if (Block)
		{
			Block->DestroyComponent();
		}
	}
	Blocks.Reset();

	// The map sits in a shallow tray let into a table of dark wood, with a brass strip round the lip.
	UMaterialInstanceDynamic* Wood = GetSolidMaterial(FromHex(TEXT("2B1B11")), 0.42f, 0.0f);
	UMaterialInstanceDynamic* Brass = GetSolidMaterial(FromHex(TEXT("B08D57")), 0.32f, 1.0f);

	const FVector2D Half = GetWorldHalfExtent(); // X: north-south, Y: east-west
	const double U = UnitScale;
	const double Top = 2.4 * U; // the table stands this far above the water
	const double Thickness = 40.0 * U;
	const double Reach = 3000.0 * U; // far enough that its edge is never seen
	const double CenterZ = Top - Thickness * 0.5;

	const double Lip = 1.3 * U, LipHeight = 0.35 * U;
	const double LipZ = Top + LipHeight * 0.5;
	if (Data.bWorld)
	{
		// (seen from the chart's north edge the lamp's own sheen on polished wood turned the table grey: this wood is dull)
		Wood = GetSolidMaterial(FromHex(TEXT("2B1B11")), 0.92f, 0.0f);
		// The chart table: wood and a brass lip along the chart's north edge and its south, running on past
		// anything the eye can reach (it is kept within half a world of the middle, and the chart is laid a turn
		// of the world to either side of it), and the floor of the tray under the whole.
		const double North = MapToWorld(0.0, Data.WorldMinY).X, South = MapToWorld(0.0, Data.WorldMinY + Data.WorldHeight).X;
		const double Long = 2.0 * Data.WorldWidth * U;
		AddBlock(FVector(North + Reach * 0.5, 0.0, CenterZ), FVector(Reach, Long, Thickness), Wood);
		AddBlock(FVector(South - Reach * 0.5, 0.0, CenterZ), FVector(Reach, Long, Thickness), Wood);
		AddBlock(FVector(North + Lip * 0.5, 0.0, LipZ), FVector(Lip, Long, LipHeight), Brass);
		AddBlock(FVector(South - Lip * 0.5, 0.0, LipZ), FVector(Lip, Long, LipHeight), Brass);
		AddBlock(FVector((North + South) * 0.5, 0.0, -3.0 * U - 5.0 * U), FVector(North - South, Long, 10.0 * U), Wood);
		return;
	}

	// Four slabs round the tray.
	AddBlock(FVector(Half.X + Reach * 0.5, 0.0, CenterZ), FVector(Reach, 2.0 * (Half.Y + Reach), Thickness), Wood);
	AddBlock(FVector(-Half.X - Reach * 0.5, 0.0, CenterZ), FVector(Reach, 2.0 * (Half.Y + Reach), Thickness), Wood);
	AddBlock(FVector(0.0, Half.Y + Reach * 0.5, CenterZ), FVector(2.0 * Half.X, Reach, Thickness), Wood);
	AddBlock(FVector(0.0, -Half.Y - Reach * 0.5, CenterZ), FVector(2.0 * Half.X, Reach, Thickness), Wood);

	// The brass lip, standing a little proud of the wood.
	AddBlock(FVector(Half.X + Lip * 0.5, 0.0, LipZ), FVector(Lip, 2.0 * (Half.Y + Lip), LipHeight), Brass);
	AddBlock(FVector(-Half.X - Lip * 0.5, 0.0, LipZ), FVector(Lip, 2.0 * (Half.Y + Lip), LipHeight), Brass);
	AddBlock(FVector(0.0, Half.Y + Lip * 0.5, LipZ), FVector(2.0 * Half.X, Lip, LipHeight), Brass);
	AddBlock(FVector(0.0, -Half.Y - Lip * 0.5, LipZ), FVector(2.0 * Half.X, Lip, LipHeight), Brass);

	// The floor of the tray, under the sea bed, so nothing shows through at the edges.
	AddBlock(FVector(0.0, 0.0, -3.0 * U - 5.0 * U), FVector(2.0 * Half.X, 2.0 * Half.Y, 10.0 * U), Wood);
}

void ACampaignMapActor::BuildWarRoom()
{
	if (!RoomMesh)
	{
		return;
	}
	RoomMesh->ClearAllMeshSections();
	// The things of a map room, lying on the wood round the map where the people who work the table have left
	// them. They are life-size beside the pieces, which are miniatures: a mug stands as tall as a flag's pole.
	const FVector2D Half = GetWorldHalfExtent(); // X: north-south, Y: east-west
	const double U = UnitScale, Top = 2.4 * U;
	FMiniatureMesh M;
	const FLinearColor Enamel(FColor::FromHex(TEXT("E4DFCF"))), Tea(FColor::FromHex(TEXT("5A3A22"))), Paper(FColor::FromHex(TEXT("E9E1C8"))), Ink(FColor::FromHex(TEXT("3A3A44")));
	const FLinearColor Bakelite(FColor::FromHex(TEXT("141416"))), Dial(FColor::FromHex(TEXT("CFC8B4"))), Pencil(FColor::FromHex(TEXT("C9962B"))), Lead(FColor::FromHex(TEXT("2A2520")));
	const FLinearColor Boxwood(FColor::FromHex(TEXT("C8A66B"))), Glass(FColor::FromHex(TEXT("2F4A3C"))), Ash(FColor::FromHex(TEXT("8D8A84"))), Steel(FColor::FromHex(TEXT("8A9098")));
	const FLinearColor BrassPaint(FColor::FromHex(TEXT("B08D57"))), Shade(FColor::FromHex(TEXT("1F4D3A"))), Cloth(FColor::FromHex(TEXT("6E1F1F"))), Label(FColor::FromHex(TEXT("D8C9A0")));
	const FLinearColor Counters[] = { FLinearColor(FColor::FromHex(TEXT("5C6068"))), FLinearColor(FColor::FromHex(TEXT("B33A32"))), FLinearColor(FColor::FromHex(TEXT("2F5D9A"))), FLinearColor(FColor::FromHex(TEXT("5E7A4A"))), FLinearColor(FColor::FromHex(TEXT("C9A227"))) };

	// A place outside one edge of the map: 0 south, 1 west, 2 north, 3 east; Along from -1 to 1 down the edge; Out from the brass lip.
	auto Spot = [&Half, Top](int32 Edge, double Along, double Out) -> FVector
	{
		switch (Edge)
		{
		case 0: return FVector(-Half.X - Out, Along * Half.Y, Top);
		case 1: return FVector(Along * Half.X, -Half.Y - Out, Top);
		case 2: return FVector(Half.X + Out, Along * Half.Y, Top);
		default: return FVector(Along * Half.X, Half.Y + Out, Top);
		}
	};
	auto Ahead = [](double Turn, double Forward, double Side) -> FVector
	{
		const double R = FMath::DegreesToRadians(Turn);
		return FVector(FMath::Cos(R) * Forward - FMath::Sin(R) * Side, FMath::Sin(R) * Forward + FMath::Cos(R) * Side, 0.0);
	};
	const FVector Up(0.0, 0.0, 1.0);

	auto Mug = [&](const FVector& At, double Turn)
	{
		M.Pillar(At, 4.6 * U, 5.0 * U, 10.5 * U, Enamel, 18);
		M.Pillar(At + Up * 10.0 * U, 4.2 * U, 4.2 * U, 0.55 * U, Tea, 18);
		// The handle: a loop of three short rods.
		const FVector Side = At + Ahead(Turn, 5.0 * U, 0.0);
		M.Rod(Side + Up * 8.0 * U, Side + Ahead(Turn, 3.0 * U, 0.0) + Up * 7.0 * U, 0.6 * U, Enamel);
		M.Rod(Side + Ahead(Turn, 3.0 * U, 0.0) + Up * 7.0 * U, Side + Ahead(Turn, 3.0 * U, 0.0) + Up * 3.5 * U, 0.6 * U, Enamel);
		M.Rod(Side + Ahead(Turn, 3.0 * U, 0.0) + Up * 3.5 * U, Side + Up * 2.5 * U, 0.6 * U, Enamel);
	};
	auto Papers = [&](const FVector& At, double Turn)
	{
		M.Box(At + Up * 0.12 * U, FVector(26.0, 36.0, 0.24) * U, Paper, Turn + 9.0);
		M.Box(At + Ahead(Turn, 3.0 * U, -4.0 * U) + Up * 0.36 * U, FVector(26.0, 36.0, 0.24) * U, Paper * 1.04f, Turn - 6.0);
		// What is typed on the top sheet: a heading and lines.
		M.Box(At + Ahead(Turn - 6.0, 13.0 * U, -5.0 * U) + Ahead(Turn, 3.0 * U, -4.0 * U) + Up * 0.5 * U, FVector(1.0, 14.0, 0.05) * U, Ink, Turn - 6.0);
		for (int32 Line = 0; Line < 7; ++Line)
		{
			M.Box(At + Ahead(Turn - 6.0, (8.5 - Line * 2.6) * U, (Line % 3 - 1) * 0.8 * U) + Ahead(Turn, 3.0 * U, -4.0 * U) + Up * 0.5 * U, FVector(0.35, 20.0 - (Line % 3) * 2.5, 0.05) * U, Ink * 1.3f, Turn - 6.0);
		}
	};
	auto PencilAt = [&](const FVector& At, double Turn)
	{
		const FVector A = At + Up * 0.5 * U, B = A + Ahead(Turn, 19.0 * U, 0.0);
		M.Rod(A, B, 0.48 * U, Pencil, 6);
		M.Limb(B, B + Ahead(Turn, 1.6 * U, 0.0), 0.48 * U, 0.05 * U, Boxwood, 6);
		M.Limb(B + Ahead(Turn, 1.1 * U, 0.0), B + Ahead(Turn, 1.7 * U, 0.0), 0.18 * U, 0.02 * U, Lead, 6);
	};
	auto Ruler = [&](const FVector& At, double Turn)
	{
		M.Box(At + Up * 0.18 * U, FVector(36.0, 3.6, 0.36) * U, Boxwood, Turn);
		for (int32 Tick = 0; Tick <= 12; ++Tick)
		{
			M.Box(At + Ahead(Turn, (Tick - 6) * 2.9 * U, 1.1 * U) + Up * 0.38 * U, FVector(0.12, Tick % 6 == 0 ? 1.6 : 0.9, 0.04) * U, Lead, Turn);
		}
	};
	auto Telephone = [&](const FVector& At, double Turn)
	{
		M.Box(At + Up * 2.6 * U, FVector(16.0, 21.0, 5.2) * U, Bakelite, Turn);
		M.Box(At + Up * 6.2 * U, FVector(9.0, 13.0, 2.0) * U, Bakelite, Turn);
		// The dial on the front slope, and the cradle with the handset across it.
		M.Pillar(At + Ahead(Turn, 5.4 * U, 0.0) + Up * 5.2 * U, 4.2 * U, 4.2 * U, 0.5 * U, Dial, 16);
		M.Pillar(At + Ahead(Turn, 5.4 * U, 0.0) + Up * 5.7 * U, 1.3 * U, 1.3 * U, 0.3 * U, Bakelite, 10);
		for (const double Side : { -5.0, 5.0 })
		{
			M.Box(At + Ahead(Turn, -1.0 * U, Side * U) + Up * 8.3 * U, FVector(3.0, 1.4, 2.6) * U, Bakelite, Turn);
		}
		const FVector Ear = At + Ahead(Turn, -1.0 * U, -10.5 * U) + Up * 10.2 * U, Mouth = At + Ahead(Turn, -1.0 * U, 10.5 * U) + Up * 10.2 * U;
		M.Rod(Ear, Mouth, 1.35 * U, Bakelite, 8);
		M.Ball(Ear + Up * -0.8 * U, FVector(4.4, 4.4, 3.4) * U, Bakelite);
		M.Ball(Mouth + Up * -0.8 * U, FVector(4.4, 4.4, 3.4) * U, Bakelite);
		// Its cord, away across the table.
		M.Rod(At + Ahead(Turn, -8.0 * U, 0.0) + Up * 0.5 * U, At + Ahead(Turn, -30.0 * U, 9.0 * U) + Up * 0.4 * U, 0.4 * U, Bakelite, 5);
	};
	auto Ashtray = [&](const FVector& At, double Turn)
	{
		M.Pillar(At, 6.5 * U, 7.6 * U, 2.2 * U, Glass, 18);
		M.Pillar(At + Up * 1.9 * U, 5.6 * U, 5.6 * U, 0.4 * U, Ash, 18);
		M.Rod(At + Ahead(Turn, 2.0 * U, 0.0) + Up * 2.5 * U, At + Ahead(Turn, 9.0 * U, 1.0 * U) + Up * 3.1 * U, 0.42 * U, Enamel, 6);
		M.Rod(At + Ahead(Turn, 1.2 * U, -0.1 * U) + Up * 2.45 * U, At + Ahead(Turn, 2.0 * U, 0.0) + Up * 2.5 * U, 0.44 * U, FLinearColor(FColor::FromHex(TEXT("C46A2A"))), 6);
	};
	auto Stacks = [&](const FVector& At, double Turn, int32 Seed)
	{
		// Spare counters, stacked by colour, waiting to go on the map.
		for (int32 Stack = 0; Stack < 5; ++Stack)
		{
			const int32 Tall = 2 + (Seed * 7 + Stack * 5) % 5;
			const FVector Foot = At + Ahead(Turn, (Stack % 3) * 10.0 * U, (Stack / 3) * 10.0 * U + (Stack % 2) * 1.5 * U);
			for (int32 Disc = 0; Disc < Tall; ++Disc)
			{
				M.Pillar(Foot + Up * Disc * 0.62 * U, 4.3 * U, 4.2 * U, 0.52 * U, Disc % 2 ? Counters[(Stack + Seed) % 5] * 0.8f : Counters[(Stack + Seed) % 5], 20);
			}
		}
	};
	auto Rake = [&](const FVector& At, double Turn)
	{
		// The long stick the pieces are pushed with, its head toward the map.
		const FVector A = At + Up * 0.9 * U, B = A + Ahead(Turn, 92.0 * U, 0.0);
		M.Rod(A, B, 0.75 * U, Boxwood, 8);
		M.Box(B + Up * 1.2 * U, FVector(2.6, 15.0, 4.2) * U, Boxwood * 0.85f, Turn);
	};
	auto Lamp = [&](const FVector& At, double Turn)
	{
		// A desk lamp of brass with a green shade, standing back from the map.
		M.Pillar(At, 8.0 * U, 6.5 * U, 1.6 * U, BrassPaint, 18);
		M.Rod(At + Up * 1.6 * U, At + Up * 30.0 * U, 0.8 * U, BrassPaint, 8);
		M.Rod(At + Up * 30.0 * U, At + Ahead(Turn, 10.0 * U, 0.0) + Up * 33.0 * U, 0.8 * U, BrassPaint, 8);
		M.Box(At + Ahead(Turn, 12.0 * U, 0.0) + Up * 32.0 * U, FVector(9.0, 24.0, 5.0) * U, Shade, Turn);
		M.Box(At + Ahead(Turn, 12.0 * U, 0.0) + Up * 29.3 * U, FVector(7.6, 22.0, 0.5) * U, FLinearColor(FColor::FromHex(TEXT("F2E6B8"))), Turn);
	};
	auto Book = [&](const FVector& At, double Turn)
	{
		M.Box(At + Up * 1.6 * U, FVector(18.0, 25.0, 3.2) * U, Cloth, Turn);
		M.Box(At + Up * 1.6 * U + Ahead(Turn, 0.6 * U, 0.0), FVector(17.6, 24.2, 2.6) * U, Paper, Turn);
		M.Box(At + Up * 3.25 * U, FVector(18.0, 25.0, 0.3) * U, Cloth, Turn);
		M.Box(At + Ahead(Turn, 3.0 * U, 0.0) + Up * 3.45 * U, FVector(5.0, 14.0, 0.06) * U, Label, Turn);
	};
	auto Dividers = [&](const FVector& At, double Turn)
	{
		const FVector Hinge = At + Up * 0.9 * U;
		M.Rod(Hinge, Hinge + Ahead(Turn + 9.0, 17.0 * U, 0.0) + Up * -0.5 * U, 0.38 * U, Steel, 6);
		M.Rod(Hinge, Hinge + Ahead(Turn - 9.0, 17.0 * U, 0.0) + Up * -0.5 * U, 0.38 * U, Steel, 6);
		M.Pillar(Hinge + Up * -0.5 * U, 1.0 * U, 1.0 * U, 1.0 * U, BrassPaint, 10);
	};

	// Along each edge, facing the map: Turn is the way a thing's front points, toward the map.
	const double Toward[4] = { 0.0, 90.0, 180.0, 270.0 };
	for (int32 Edge = 0; Edge < 4; ++Edge)
	{
		const double T = Toward[Edge];
		const int32 S = Edge * 3 + 1;
		Telephone(Spot(Edge, -0.72 + 0.11 * Edge, 34.0 * U), T + 12.0 - 9.0 * Edge);
		Mug(Spot(Edge, -0.52 + 0.07 * Edge, 22.0 * U), T + 70.0);
		Papers(Spot(Edge, -0.3 + 0.05 * Edge, 36.0 * U), T + 4.0 * Edge);
		PencilAt(Spot(Edge, -0.21 + 0.05 * Edge, 16.0 * U), T + 68.0 + 20.0 * Edge);
		Stacks(Spot(Edge, -0.05, 14.0 * U), T + 90.0, S);
		Ruler(Spot(Edge, 0.16 - 0.04 * Edge, 12.0 * U), T + 84.0 + 5.0 * Edge);
		Dividers(Spot(Edge, 0.23, 24.0 * U), T + 40.0 * Edge);
		Ashtray(Spot(Edge, 0.36 + 0.05 * Edge, 26.0 * U), T + 200.0);
		Book(Spot(Edge, 0.5 - 0.03 * Edge, 40.0 * U), T - 14.0 + 8.0 * Edge);
		Stacks(Spot(Edge, 0.66, 18.0 * U), T + 90.0, S + 2);
		Mug(Spot(Edge, 0.8, 30.0 * U), T - 40.0);
		Rake(Spot(Edge, 0.55 - 0.9 * (Edge % 2), 100.0 * U), T + 6.0 - 12.0 * (Edge % 2));
		Lamp(Spot(Edge, 0.93 - 1.86 * (Edge % 2), 60.0 * U), T + 150.0 - 300.0 * (Edge % 2));
	}
	if (M.Vertices.Num() > 0)
	{
		RoomMesh->CreateMeshSection(0, M.Vertices, M.Triangles, M.Normals, TArray<FVector2D>(), M.Colors, TArray<FProcMeshTangent>(), false);
		RoomMesh->SetMaterial(0, SceneryMaterial);
		RoomMesh->SetCastShadow(true);
	}
}

void ACampaignMapActor::ShowCivic(const TArray<int32>& Civic)
{
	THEATER_SPEED_SCOPE(ShowCivic);
	if (!Data.IsLoaded() || (CivicMesh && Civic == ShownCivic))
	{
		return;
	}
	// (Release 2, the flak ring's second mend; tc.Look.FlakRing.) A gun pit makes way for a plot that is built on or
	// going up. Where a plot is newly taken, or given up, in a town with guns, the works are stood again below, so
	// that no pit is left under a market: the pit in the way moves a few degrees, once, as the scaffold appears.
	bool bPlotsMoved = false;
	if (FlakRingAsked())
	{
		auto PlotsOf = [](const TArray<int32>& Listed)
		{
			TSet<int32> Stood;
			for (int32 Entry = 0; Entry + 3 < Listed.Num(); Entry += 4)
			{
				if (Listed[Entry + 1] >= 0 && Listed[Entry + 1] <= 5)
				{
					Stood.Add(Listed[Entry] * 8 + Listed[Entry + 1]);
				}
			}
			return Stood;
		};
		const TSet<int32> StoodBefore = PlotsOf(ShownCivic), StoodAfter = PlotsOf(Civic);
		for (const int32 PlotPair : StoodBefore)
		{
			bPlotsMoved |= !StoodAfter.Contains(PlotPair) && ShownBuildings.IsValidIndex(PlotPair / 8) && ShownBuildings[PlotPair / 8].Z > 0;
		}
		for (const int32 PlotPair : StoodAfter)
		{
			bPlotsMoved |= !StoodBefore.Contains(PlotPair) && ShownBuildings.IsValidIndex(PlotPair / 8) && ShownBuildings[PlotPair / 8].Z > 0;
		}
	}
	ShownCivic = Civic;
	if (!CivicMesh)
	{
		CivicMesh = UTheaterStillMesh::Make(this, TEXT("Civic"));
		CivicMesh->SetupAttachment(Root);
		CivicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CivicMesh->RegisterComponent();
	}
	CivicMesh->ClearAllMeshSections();
	FMiniatureMesh Model;
	const double U = UnitScale;
	// The buildings stand together as a new quarter on the south side of the town, before it as the table is
	// usually looked at, where no flag or aeroplane hides them: the market, the hall and the school in the near
	// row, the hospital, the drill ground and the farms beyond. (An army stands north-east of a town, its air
	// wings and airfield south-west, its factories north and its guns west.) East and south of the town's middle:
	static const FVector2D Plots[] = { FVector2D(1.0, 0.0), FVector2D(16.0, 7.2), FVector2D(16.0, 0.0), FVector2D(8.5, 7.2), FVector2D(1.0, 7.2), FVector2D(8.5, 0.0) };
	for (int32 K = 0; K + 3 < Civic.Num(); K += 4)
	{
		const int32 RegionId = Civic[K], Line = Civic[K + 1], Level = Civic[K + 2];
		if (!Data.Regions.IsValidIndex(RegionId) || Line < 0 || Line > 5)
		{
			continue;
		}
		const FCampaignRegion& Region = Data.Regions[RegionId];
		const bool bCity = Region.Terrain == TEXT("urban");
		// (Art fix 9: the buildings a town is given are a quarter of the town itself. In a city and a capital the
		// ground for them is kept open inside the streets; beside a smaller place they stand against its last
		// houses. The whole quarter is turned one way, so the town's builder knows where to leave room.)
		const double Clear = CivicNear(RegionId);
		const double Size = Region.bCapital ? 2.9 : (bCity ? 2.7 : 2.4);
		const FVector2D Offset(Plots[Line].X, Clear + Plots[Line].Y);
		const double Angle = FMath::DegreesToRadians(CivicTurn(RegionId));
		const FVector2D At = Region.Center + FVector2D(Offset.X * FMath::Cos(Angle) - Offset.Y * FMath::Sin(Angle), Offset.X * FMath::Sin(Angle) + Offset.Y * FMath::Cos(Angle));
		if (GroundAt(At.X, At.Y) <= 0.0f)
		{
			continue; // (that plot is in the water)
		}
		const FCampaignNation* Holder = Data.FindNation(Region.Owner);
		Model.Civic(SurfacePoint(At.X, At.Y, 0.02), U * Size, Line, Level, Civic[K + 3] != 0, Holder ? FLinearColor(Holder->Color) : FLinearColor::Gray);
	}
	if (Model.Vertices.Num() > 0)
	{
		CivicMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		CivicMesh->SetMaterial(0, SceneryMaterial);
		SeamLaidAgain(CivicMesh); // (seam-2: its twin over the chart's seam is laid again)
	}
	if (bPlotsMoved && LastWorks.Num() > 0)
	{
		const TArray<FIntVector4> Again = LastWorks;
		ShownBuildings.Reset();
		ShowBuildings(Again);
	}
}

void ACampaignMapActor::SetFlatColours(bool bFlat)
{
	bFlatColours = bFlat;
	if (LandMaterial)
	{
		LandScalar(TEXT("Flat"), bFlat ? 1.0f : 0.0f);
	}
}

void ACampaignMapActor::SetColourStrength(int32 Level)
{
	ColourStrength = FMath::Clamp(Level, 0, 2);
	WashShown = -1.0f; // (set again at the next look)
	if (LandMaterial)
	{
		static const float Bands[3] = { 0.32f, 0.4f, 0.48f };
		LandScalar(TEXT("Band"), Bands[ColourStrength]);
	}
}

void ACampaignMapActor::ShowSieges(const TArray<int32>& Sieges)
{
	THEATER_SPEED_SCOPE(ShowSieges);
	if (Sieges == ShownSieges)
	{
		return;
	}
	ShownSieges = Sieges;
	SiegeFires.Reset();
	for (int32 K = 0; K + 2 < Sieges.Num(); K += 3)
	{
		if (!Data.Regions.IsValidIndex(Sieges[K]))
		{
			continue;
		}
		// Fires in the town itself: more of them once the ring is closed.
		const FCampaignRegion& Region = Data.Regions[Sieges[K]];
		const int32 Fires = Sieges[K + 1] != 0 ? 4 : 2;
		for (int32 F = 0; F < Fires; ++F)
		{
			const double Angle = F * 2.4 + Region.Id;
			SiegeFires.Add(SurfacePoint(Region.Center.X + FMath::Cos(Angle) * 2.2 * TownSpread(Region.Id), Region.Center.Y + FMath::Sin(Angle) * 2.2 * TownSpread(Region.Id), 1.6 * TownSpread(Region.Id)));
		}
	}
}

void ACampaignMapActor::ShowBombed(const TArray<int32>& FromRules)
{
	THEATER_SPEED_SCOPE(ShowBombed);
	TArray<int32> Pairs = FromRules;
	{
		// (for pictures: "-TheaterBombedAt=Katowice:hard;Krakow" stands a raid's smoke over those places as well)
		FString Asked;
		if (Data.IsLoaded() && FParse::Value(FCommandLine::Get(), TEXT("TheaterBombedAt="), Asked, false))
		{
			TArray<FString> Names;
			Asked.ParseIntoArray(Names, TEXT(";"));
			for (const FString& Each : Names)
			{
				FString Name = Each, How;
				Each.Split(TEXT(":"), &Name, &How);
				if (const int32* Found = Data.RegionByName.Find(Name.Replace(TEXT("_"), TEXT(" "))))
				{
					Pairs.Append({ *Found, How == TEXT("hard") ? 400 : 100 });
				}
			}
		}
	}
	if (Pairs == ShownBombed)
	{
		return;
	}
	ShownBombed = Pairs;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// How many columns each place is to have: three where it was hit hard, one otherwise.
	TMap<int32, int32> Wanted;
	for (int32 K = 0; K + 1 < Pairs.Num(); K += 2)
	{
		if (Data.Regions.IsValidIndex(Pairs[K]))
		{
			Wanted.Add(Pairs[K], Pairs[K + 1] >= 300 ? 3 : 1);
		}
	}
	// A column that stands and is still wanted stands on; one no longer wanted thins away over two seconds.
	TMap<int32, int32> Standing;
	for (FBombSmoke& Smoke : BombSmokes)
	{
		int32& Have = Standing.FindOrAdd(Smoke.Region);
		if (Smoke.Ends < 0.0 && Have >= Wanted.FindRef(Smoke.Region))
		{
			Smoke.Ends = Now;
		}
		else if (Smoke.Ends < 0.0)
		{
			++Have;
		}
	}
	BombSmokes.RemoveAll([Now](const FBombSmoke& Smoke) { return Smoke.Ends >= 0.0 && Now - Smoke.Ends > 2.5; });
	for (const TPair<int32, int32>& Place : Wanted)
	{
		const FCampaignRegion& Region = Data.Regions[Place.Key];
		// (about one and a fifth times as tall as the town's model is wide; a place with no town gets a modest one)
		const double Tall = FMath::Clamp(2.4 * TownRadius(Region.Id), 9.0, 30.0) * UnitScale;
		for (int32 Column = Standing.FindRef(Place.Key); Column < Place.Value; ++Column)
		{
			FBombSmoke& Smoke = BombSmokes.AddDefaulted_GetRef();
			Smoke.Region = Region.Id;
			// (round 34: the second and third columns of a place hit hard stand at least two and a half units out, so that
			// at working height the three are a broad mass with three roots and not one column)
			const double Angle = Column * 2.1 + Region.Id * 0.7, Out = Column == 0 ? 0.35 * TownSpread(Region.Id) : FMath::Max(1.1 * TownSpread(Region.Id), 2.6);
			Smoke.Root = SurfacePoint(Region.Center.X + FMath::Cos(Angle) * Out, Region.Center.Y + FMath::Sin(Angle) * Out, 0.5);
			Smoke.Tall = Tall * (Column == 0 ? 1.0 : 0.8);
			Smoke.Began = Now;
			Smoke.Seed = Region.Id * 7 + Column * 31;
			Smoke.bFirst = Column == 0;
		}
	}
}

void ACampaignMapActor::ShowWorksLeft(const TArray<int32>& FromRules)
{
	THEATER_SPEED_SCOPE(ShowWorksLeft);
	TArray<int32> Left = FromRules;
	{
		// (for pictures: -TheaterWorksLeft=Province,Province stands empty fieldworks in those provinces as well)
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterWorksLeft="), Asked, false))
		{
			TArray<FString> Names;
			Asked.ParseIntoArray(Names, TEXT(","));
			for (int32 K = 0; K < Names.Num(); ++K)
			{
				if (const int32* Found = Data.RegionByName.Find(Names[K].Replace(TEXT("_"), TEXT(" "))))
				{
					Left.Append({ *Found, K % 2 == 0 ? 20 : 10, 1 });
				}
			}
		}
	}
	if (!Data.IsLoaded() || Left == ShownWorksLeft)
	{
		return;
	}
	ShownWorksLeft = Left;
	if (!WorksLeftMesh)
	{
		WorksLeftMesh = UTheaterStillMesh::Make(this, TEXT("WorksLeft"));
		WorksLeftMesh->SetupAttachment(Root);
		WorksLeftMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WorksLeftMesh->RegisterComponent();
		WorksLeftMesh->SetCastShadow(true); // (a thing that throws no shadow is lit as if nothing stood round it: its shaded faces go black)
	}
	WorksLeftMesh->ClearAllMeshSections();
	// A bank of sandbags where an army would stand, broken in four so that it is read as standing empty, and smaller
	// than a piece's base so that it does not crowd the province. Two courses once there is two months' work in it.
	FMiniatureMesh Model;
	const double U = UnitScale * 1.4;
	// (the art director's notes: the bags a step quieter than a flag, the floor trodden earth in the bases' own tone)
	const FLinearColor Bag(FColor::FromHex(TEXT("84795A"))), BagDark(FColor::FromHex(TEXT("70674D"))), Spoil(FColor::FromHex(TEXT("655840"))), SpoilPale(FColor::FromHex(TEXT("6F644B")));
	FRandomStream Ragged(1939);
	for (int32 K = 0; K + 2 < Left.Num(); K += 3)
	{
		if (!Data.Regions.IsValidIndex(Left[K]))
		{
			continue;
		}
		const FVector Middle = ArmySpot(Left[K], 0, 1);
		const int32 Courses = Left[K + 1] >= 20 ? 2 : 1;
		const double Rim = 2.5 * U;
		for (int32 Arc = 0; Arc < 4; ++Arc)
		{
			for (int32 Course = 0; Course < Courses; ++Course)
			{
				const int32 Bags = 3 - Course;
				for (int32 B = 0; B < Bags; ++B)
				{
					const double Angle = FMath::DegreesToRadians(Arc * 90.0 + 21.0 + (B + 0.5 * Course) * 24.0);
					const FVector At = Middle + FVector(FMath::Cos(Angle) * Rim, FMath::Sin(Angle) * Rim, (0.2 + 0.36 * Course) * U);
					Model.Box(At, FVector(0.5, 1.05, 0.38) * U, (B + Course + Arc) % 2 == 0 ? Bag : BagDark, FMath::RadiansToDegrees(Angle));
				}
			}
		}
		// The trodden earth inside it, so the bank has something to stand round: not a plate but a patch, made of
		// many small worn places that thin out toward the bank, so that its edge is ragged and gives way to the field.
		for (int32 Patch = 0; Patch < 46; ++Patch)
		{
			// (thick in the middle, where they run together into one worn place, and small and thin toward the bank)
			const double Angle = Ragged.FRandRange(0.0f, 6.283f), Out = Rim * 0.74 * FMath::Pow(Ragged.FRand(), 0.8);
			const double Size = Rim * FMath::Lerp(0.34, 0.07, Out / (Rim * 0.74)) * Ragged.FRandRange(0.85f, 1.2f);
			Model.Pillar(Middle + FVector(FMath::Cos(Angle) * Out, FMath::Sin(Angle) * Out, (0.016 + 0.0004 * Patch) * U), Size, Size * 0.86, 0.014 * U,
				Out > Rim * 0.5 ? SpoilPale : Spoil, 7);
		}
	}
	if (Model.Vertices.Num() > 0)
	{
		WorksLeftMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		WorksLeftMesh->SetMaterial(0, SceneryMaterial);
		SeamLaidAgain(WorksLeftMesh); // (seam-2: its twin over the chart's seam is laid again)
	}
}

void ACampaignMapActor::ShowPrizes(const TArray<FPrizeMark>& Marks)
{
	THEATER_SPEED_SCOPE(ShowPrizes);
	FString Signature;
	for (const FPrizeMark& Mark : Marks)
	{
		Signature += FString::Printf(TEXT("%d:%s:%d;"), Mark.Region, *Mark.By, Mark.bBurning ? 1 : 0);
	}
	if (!Data.IsLoaded() || Signature == ShownPrizes)
	{
		return;
	}
	ShownPrizes = Signature;
	PrizeFires.Reset();
	if (!PrizesMesh)
	{
		PrizesMesh = UTheaterStillMesh::Make(this, TEXT("Prizes"));
		PrizesMesh->SetupAttachment(Root);
		PrizesMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrizesMesh->RegisterComponent();
		PrizesMesh->SetCastShadow(false);
	}
	PrizesMesh->ClearAllMeshSections();
	FMiniatureMesh Model;
	const double U = UnitScale;
	const FLinearColor Pole(FColor::FromHex(TEXT("3A2E24")));
	for (const FPrizeMark& Mark : Marks)
	{
		if (!Data.Regions.IsValidIndex(Mark.Region))
		{
			continue;
		}
		const FCampaignRegion& Region = Data.Regions[Mark.Region];
		if (Mark.bBurning)
		{
			// What was wrecked lies outside the town: the fire is beside it, not in its streets.
			PrizeFires.Add(SurfacePoint(Region.Center.X - 0.84 * (TownRadius(Region.Id) + 1.6), Region.Center.Y - 0.54 * (TownRadius(Region.Id) + 1.6), 0.5));
		}
		if (!Mark.By.IsEmpty())
		{
			// A pennant in the colour of the army that holds what lies outside the walls, though the town holds:
			// two cloths crossed on one pole, so that it is read from any side of the table.
			const FCampaignNation* Whose = Data.FindNation(Mark.By);
			const FLinearColor Cloth = Whose ? FLinearColor(Whose->Color) : FLinearColor(0.7f, 0.7f, 0.7f);
			// (a pale band along its top edge, so that a dark colour is still seen to be a flag)
			const FLinearColor Band(FColor::FromHex(TEXT("EFE6CC")));
			const FVector Foot = SurfacePoint(Region.Center.X + 0.81 * (TownRadius(Region.Id) + 1.6), Region.Center.Y + 0.59 * (TownRadius(Region.Id) + 1.6), 0.02);
			Model.Pillar(Foot, 0.2 * U, 0.14 * U, 11.0 * U, Pole, 6);
			for (int32 Cross = 0; Cross < 2; ++Cross)
			{
				Model.Box(Foot + FVector(0.0, 0.0, 9.0 * U), FVector(5.6, 0.16, 2.3) * U, Cloth, Cross * 90.0);
				Model.Box(Foot + FVector(0.0, 0.0, 10.4 * U), FVector(5.6, 0.18, 0.5) * U, Band, Cross * 90.0);
			}
			Model.Pillar(Foot + FVector(0.0, 0.0, 11.0 * U), 0.34 * U, 0.05 * U, 0.8 * U, Pole, 6);
		}
	}
	if (Model.Vertices.Num() > 0)
	{
		PrizesMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		PrizesMesh->SetMaterial(0, SceneryMaterial);
		SeamLaidAgain(PrizesMesh); // (seam-2: its twin over the chart's seam is laid again)
	}
}

namespace
{
	/** How far a town's outermost roofs stand from its middle at the size towns were first built, in map units, by its tier: capital, city, large town, small place. */
	double FirstTownRadius(const FCampaignRegion& Region)
	{
		const bool bCity = Region.Terrain == TEXT("urban");
		const bool bBig = Region.bCapital || bCity || Region.VictoryPoints >= 2 || Region.Factories >= 2;
		const bool bTown = bBig || Region.VictoryPoints > 0 || Region.Factories > 0 || Region.bOil || Region.bPort;
		return Region.bCapital ? 10.1 : bCity ? 8.5 : bBig ? 6.4 : bTown ? 4.4 : 0.0;
	}
}

double ACampaignMapActor::TownSpread(int32 RegionId) const
{
	if (!Data.Regions.IsValidIndex(RegionId))
	{
		return 1.0;
	}
	if (TownSpreads.Num() != Data.Regions.Num())
	{
		// The ladder (art fix 8): an army's base is about 14 map units across; a capital is built to about two and
		// a half of them, a city to about one and two thirds, a large town to a little over one, a small place to
		// three quarters. A town is never built past the edge of its own province, though, nor smaller than it was.
		TownSpreads.SetNum(Data.Regions.Num());
		TownPacked.Init(false, Data.Regions.Num());
		for (const FCampaignRegion& Region : Data.Regions)
		{
			const bool bCity = Region.Terrain == TEXT("urban");
			const double First = FirstTownRadius(Region);
			double Wanted = Region.bCapital ? 1.75 : bCity ? 1.4 : First >= 6.0 ? 1.25 : First > 0.0 ? 1.2 : 1.0;
			bool bTight = false;
			if (First > 0.0)
			{
				double Room = 1.0e9;
				for (const TArray<FVector2D>& Ring : Region.Polygons)
				{
					for (int32 i = 0, n = Ring.Num(); i < n; ++i)
					{
						Room = FMath::Min(Room, FMath::PointDistToSegment(FVector(Region.Center, 0.0), FVector(Ring[i], 0.0), FVector(Ring[(i + 1) % n], 0.0)));
					}
				}
				if (Room * 0.95 < First * Wanted)
				{
					// It does not fit as a circle. Where that is the doing of one near edge (a coast, a frontier) and
					// most of the ground round it is still the province's own, the town is built to its full size and
					// grows lopsided, away from that edge: its builder leaves out what would stand in the sea or over
					// the frontier. Where the province really is small, the town is kept to the room there is, and
					// a city or capital is then packed tight and tall.
					int32 Inside = 0;
					const int32 Looks = 24;
					for (int32 K = 0; K < Looks; ++K)
					{
						const double Angle = K * UE_DOUBLE_TWO_PI / Looks;
						const FVector2D At = Region.Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (First * Wanted * 0.75);
						Inside += GroundAt(At.X, At.Y) > 0.0f && Data.FindRegionAt(At.X, At.Y) == Region.Id ? 1 : 0;
					}
					if (Inside * 100 < Looks * 58)
					{
						const double Fits = FMath::Clamp(Room * 0.95 / First, 1.0, Wanted);
						bTight = (Region.bCapital || bCity) && Fits < Wanted * 0.82;
						Wanted = Fits;
					}
				}
			}
			TownSpreads[Region.Id] = static_cast<float>(Wanted);
			TownPacked[Region.Id] = bTight;
		}
		// (The art director's rule for close towns, 18 November 2026, with her mends of that morning; the cities of the
		// Americas stood Baltimore almost on top of Washington. Towns never move. The rule is for towns whose roofs
		// would really touch across the border between them, not for towns that are merely near: Anzio stands clear
		// of Rome and is left alone. Where two would touch, the higher-ranked keeps its size, a capital first and then
		// by what the rules hold the place worth, and the other gives way: but it is never less than the next kit
		// up from a hamlet, a town of a church and its streets with its quay, eight to a dozen roofs. Green field
		// shows between the two: see below for which of them leaves it.
		// "-TheaterTownsAsBuilt" leaves every town its first size.)
		TownHumbled.Init(false, Data.Regions.Num());
		TownClear.Reset();
		TownKeepOff.Reset();
		if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterTownsAsBuilt")))
		{
			auto RankOf = [](const FCampaignRegion& Region)
			{
				return (Region.bCapital ? 1000.0 : 0.0) + Region.VictoryPoints * 10.0 + Region.Factories + (Region.Terrain == TEXT("urban") ? 0.5 : 0.0);
			};
			// How far a town's own ground runs from its middle toward another's: it is built on land, and on its own province's.
			auto Runs = [this](const FCampaignRegion& From, const FCampaignRegion& Toward, double Far)
			{
				const double Apart = FVector2D::Distance(From.Center, Toward.Center);
				const FVector2D Way = (Toward.Center - From.Center) / FMath::Max(Apart, 0.01);
				double Last = 0.0;
				for (double Out = 0.25; Out <= FMath::Min(Far, Apart); Out += 0.25)
				{
					const FVector2D At = From.Center + Way * Out;
					if (GroundAt(At.X, At.Y) <= 0.0f || Data.FindRegionAt(At.X, At.Y) != From.Id)
					{
						break;
					}
					Last = Out;
				}
				return Last;
			};
			TArray<int32> Order;
			for (const FCampaignRegion& Region : Data.Regions)
			{
				if (FirstTownRadius(Region) > 0.0)
				{
					Order.Add(Region.Id);
				}
			}
			Order.Sort([this, &RankOf](int32 A, int32 B)
			{
				const double RankA = RankOf(Data.Regions[A]), RankB = RankOf(Data.Regions[B]);
				return RankA != RankB ? RankA > RankB : A < B;
			});
			const double Touching = 1.0; // (under half a house of green between their roofs: they touch)
			const double CityNeeds = 7.0; // (a city's blocks and square want this much; below it the town's kit is built)
			const double Row = 4.2; // (a row of a city's blocks and its street)
			int32 Gave = 0;
			FString Said;
			for (int32 A = 1; A < Order.Num(); ++A)
			{
				const FCampaignRegion& Lesser = Data.Regions[Order[A]];
				const bool bCity = Lesser.Terrain == TEXT("urban");
				const double Own = FirstTownRadius(Lesser), Was = Own * TownSpreads[Lesser.Id];
				// (the least it is built: a large town's first size for a city, a small place's for the rest; a capital its own)
				const double Least = FMath::Min(Was, Lesser.bCapital ? 10.1 : bCity ? 6.4 : 4.4);
				double Radius = Was;
				bool bUnder = false;
				TArray<int32, TInlineAllocator<4>> Beside;
				FString Names;
				for (int32 B = 0; B < A; ++B)
				{
					const FCampaignRegion& Greater = Data.Regions[Order[B]];
					const double Apart = FVector2D::Distance(Lesser.Center, Greater.Center);
					const double Theirs = FirstTownRadius(Greater) * TownSpreads[Greater.Id];
					if (Was + Theirs > Apart && Apart - Runs(Lesser, Greater, Was) - Runs(Greater, Lesser, Theirs) < Touching)
					{
						// (The art director, 18 November, on build 1648: "the largest kit that fits while keeping the
						// field gap". Where the greater town at its full size, with a row of green, would cover the
						// lesser town's very middle, the lesser stands under it: Baltimore under Washington. It is then
						// built small, and the greater gives the green. Where the lesser's middle stands clear, it
						// keeps its own kit and its size and builds away from the greater, leaving its side toward
						// it unbuilt: Rotterdam, a city on its river mouth, beside Amsterdam.)
						if (Theirs + Row + 0.8 >= Apart)
						{
							bUnder = true;
							Radius = FMath::Min(Radius, FMath::Max((Apart - Theirs) / 1.5, Least));
						}
						// (it keeps its size as well as its kit: seen at a city's first size, with its side toward
						// Amsterdam left unbuilt, Rotterdam was a square, a church and a few sheds, no city)
						Beside.Add(Greater.Id);
						Names += (Names.IsEmpty() ? TEXT("") : TEXT(" and ")) + Greater.Name;
					}
				}
				if (Beside.Num() == 0)
				{
					continue;
				}
				TownSpreads[Lesser.Id] = static_cast<float>(Radius / Own);
				TownHumbled[Lesser.Id] = bUnder && bCity && !Lesser.bCapital && Radius < CityNeeds;
				// The green between: about a row of a city's blocks, and more beside a larger town.
				const double Green = FMath::Clamp(0.5 * Radius + 3.0, Row, 7.0);
				for (const int32 Id : Beside)
				{
					const FCampaignRegion& Greater = Data.Regions[Id];
					const double Apart = FVector2D::Distance(Lesser.Center, Greater.Center);
					const double Theirs = FirstTownRadius(Greater) * TownSpreads[Id];
					const double TheirRun = Runs(Greater, Lesser, Theirs);
					if (Theirs + Row + 0.8 >= Apart)
					{
						// (the greater town leaves its rows unbuilt beside the lesser, but keeps three fifths of its reach on that side)
						const double Keep = FMath::Min(Radius + Green, Apart - 0.6 * TheirRun);
						if (Keep > 0.0)
						{
							TownClear.FindOrAdd(Id).Add(FVector(Lesser.Center.X, Lesser.Center.Y, Keep));
						}
					}
					else
					{
						// (the lesser town builds no nearer the greater than a row of green beyond the greater's last roofs)
						TownClear.FindOrAdd(Lesser.Id).Add(FVector(Greater.Center.X, Greater.Center.Y, TheirRun + Row));
						// (The art director, 18 November 2026, on build 1668: Amsterdam's and Rotterdam's blocks still met in
						// a thin grey band, so that the two read as one long city. The ring above keeps a row clear on
						// the line between the two middles and less and less to either side of it, and it is kept from
						// the middles of the blocks, whose edges then stand two or three units apart; the greater's
						// last row stood on the border itself. So each also keeps off the other's ground, wherever
						// the border between them runs: the lesser a row and half a block off any ground of the
						// greater's province its roofs may reach, and the greater half a block off the lesser's. A row
						// of open ground then lies between the edges of their blocks. The lesser's own middle always
						// stays clear, as it did.)
						const double HalfBlock = 2.0;
						FTownKeepOff& OfLesser = TownKeepOff.FindOrAdd(Lesser.Id).AddDefaulted_GetRef();
						OfLesser.Middle = Greater.Center;
						OfLesser.Reach = FMath::Min(1.22 * Theirs, Apart - Row - HalfBlock - 0.8); // (the lesser's own middle always stays clear)
						OfLesser.Province = Id;
						OfLesser.Off = Row + HalfBlock;
						FTownKeepOff& OfGreater = TownKeepOff.FindOrAdd(Id).AddDefaulted_GetRef();
						OfGreater.Middle = Lesser.Center;
						OfGreater.Reach = 1.22 * Was;
						OfGreater.Province = Lesser.Id;
						OfGreater.Off = HalfBlock;
					}
				}
				++Gave;
				if (Gave <= 8)
				{
					Said += FString::Printf(TEXT("%s%s beside %s (%.1f to %.1f%s)"), Said.IsEmpty() ? TEXT("") : TEXT("; "), *Lesser.Name, *Names, Was, Radius,
						TownHumbled[Lesser.Id] ? TEXT(", as a town") : bUnder ? TEXT("") : TEXT(": its size kept, building away from it"));
				}
			}
			if (Gave > 0)
			{
				UE_LOG(LogTheater, Display, TEXT("Towns: %d would touch a greater town beside them; each is built smaller and the greater leaves green between: %s%s"), Gave, *Said, Gave > 8 ? TEXT("; and more") : TEXT(""));
			}
		}
	}
	return TownSpreads[RegionId];
}

void ACampaignMapActor::ShowTownForm(bool bFar)
{
	if (TownMesh)
	{
		TownMesh->SetVisibility(!bFar);
	}
	if (TownFarMesh)
	{
		TownFarMesh->SetVisibility(bFar);
	}
	for (UProceduralMeshComponent* Square : TownSquares)
	{
		if (Square)
		{
			Square->SetVisibility(!bFar);
		}
	}
	for (UProceduralMeshComponent* Square : TownFarSquares)
	{
		if (Square)
		{
			Square->SetVisibility(bFar);
		}
	}
	if (LightsMesh && LightsMesh->GetNumSections() > 0)
	{
		LightsMesh->SetMeshSectionVisible(0, !bFar);
	}
	TownSquareForm.Init(static_cast<int8>(bFar ? 1 : 0), FMath::Max(TownSquares.Num(), TownFarSquares.Num())); // (speed: tc.Speed.ShowSpread)
}

void ACampaignMapActor::SetTownShadows(bool bCast)
{
	TownShadowsCast = bCast ? 1 : 0;
	if (TownMesh)
	{
		TownMesh->SetCastShadow(bCast);
	}
	for (UProceduralMeshComponent* Square : TownSquares)
	{
		if (Square)
		{
			Square->SetCastShadow(bCast);
		}
	}
}

void ACampaignMapActor::SetTownPlots(bool bPlots)
{
	THEATER_SPEED_SCOPE(SetTownPlots);
	if (bPlots != bTownPlots)
	{
		bTownPlots = bPlots;
		if (TownMesh && Data.IsLoaded())
		{
			BuildTowns(); // (the cities leave room for the quarter, or close it up)
			ShownNight = -1.0f;
			SetNight(NightNow, bLamplightNow);
		}
	}
}

bool ACampaignMapActor::CivicInside(int32 RegionId) const
{
	if (!Data.Regions.IsValidIndex(RegionId))
	{
		return false;
	}
	const FCampaignRegion& Region = Data.Regions[RegionId];
	return (Region.bCapital || Region.Terrain == TEXT("urban")) && TownRadius(RegionId) >= 14.0;
}

double ACampaignMapActor::CivicNear(int32 RegionId) const
{
	if (!Data.Regions.IsValidIndex(RegionId))
	{
		return 8.0;
	}
	const FCampaignRegion& Region = Data.Regions[RegionId];
	// (inside a great city, some way south of its square; against the last houses of a smaller place, or of a
	// city too small to spare the ground: the quarter is as large as a small capital)
	return CivicInside(RegionId) ? FMath::Max(TownRadius(RegionId) * 0.4, 6.5) : TownRadius(RegionId) + 2.4;
}

double ACampaignMapActor::CivicTurn(int32 RegionId) const
{
	if (!Data.Regions.IsValidIndex(RegionId))
	{
		return 0.0;
	}
	// The quarter lies south and east of the town's middle; if the middle of it would be in the sea or over the
	// frontier it is turned to another side, the same for every building in it.
	const FCampaignRegion& Region = Data.Regions[RegionId];
	const FVector2D Offset(9.0, CivicNear(RegionId) + 3.6);
	double Second = 0.0;
	bool bSecond = false;
	for (const double Shift : { 0.0, -55.0, 55.0, -110.0, 110.0, 180.0 })
	{
		const double Angle = FMath::DegreesToRadians(Shift);
		const FVector2D Try = Region.Center + FVector2D(Offset.X * FMath::Cos(Angle) - Offset.Y * FMath::Sin(Angle), Offset.X * FMath::Sin(Angle) + Offset.Y * FMath::Cos(Angle));
		if (GroundAt(Try.X, Try.Y) <= 0.0f)
		{
			continue;
		}
		if (Data.FindRegionAt(Try.X, Try.Y) == RegionId)
		{
			return Shift;
		}
		if (!bSecond)
		{
			bSecond = true;
			Second = Shift;
		}
	}
	return Second;
}

bool ACampaignMapActor::IsTownPacked(int32 RegionId) const
{
	TownSpread(RegionId); // (worked out with the sizes)
	return TownPacked.IsValidIndex(RegionId) && TownPacked[RegionId];
}

double ACampaignMapActor::TownRadius(int32 RegionId) const
{
	if (!Data.Regions.IsValidIndex(RegionId))
	{
		return 3.0;
	}
	const double First = FirstTownRadius(Data.Regions[RegionId]);
	return First > 0.0 ? First * TownSpread(RegionId) : 3.0;
}

bool ACampaignMapActor::GetAirfieldWorld(int32 RegionId, FVector& OutWorld) const
{
	if (const FVector2D* Spot = AirfieldSpots.Find(RegionId))
	{
		OutWorld = SurfacePoint(Spot->X, Spot->Y, 0.6);
		return true;
	}
	return false;
}

void ACampaignMapActor::ShowAirfieldSigns(const TArray<FViewAirfield>& Fields)
{
	THEATER_SPEED_SCOPE(ShowAirfieldSigns);
	{
		// Which airfields are in use: if that has changed, the airfields themselves are built again, the empty ones smaller.
		TSet<int32> Used;
		for (const FViewAirfield& Field : Fields)
		{
			if (Field.Groups > 0)
			{
				Used.Add(Field.Region);
			}
		}
		const bool bKnown = Fields.Num() > 0;
		if (bKnown != bAirfieldsKnown || Used.Num() != AirfieldsUsed.Num() || !Used.Includes(AirfieldsUsed))
		{
			bAirfieldsKnown = bKnown;
			AirfieldsUsed = MoveTemp(Used);
			if (LastWorks.Num() > 0)
			{
				const TArray<FIntVector4> Again = LastWorks;
				ShownBuildings.Reset();
				ShowBuildings(Again);
			}
		}
	}
	FString Signature = FString::Printf(TEXT("%d|"), AirfieldSpots.Num());
	for (const FViewAirfield& Field : Fields)
	{
		if (Field.Groups > 0)
		{
			// (where the player shares the field, the row's share is drawn from the two counts of aircraft: both are in the
			// signature, so that his share does not go stale as the groups grow and shrink. They were in it by
			// twenty-fives, and the row is cut by a rounded share: 26 or 49 of a hundred were one signature and two rows.
			// The 3D pre-check's read, 24 November 2026. Few fields are shared, so the signs are seldom laid again for it.)
			Signature += Field.YourGroups > 0
				? FString::Printf(TEXT("%d:%s:%d:%d:%d:%d;"), Field.Region, *Field.Nation, Field.Groups, Field.YourGroups, FMath::Max(Field.Aircraft, 0), FMath::Max(Field.YourAircraft, 0))
				: FString::Printf(TEXT("%d:%s:%d;"), Field.Region, *Field.Nation, Field.Groups);
		}
	}
	if (!Data.IsLoaded() || Signature == ShownAirSigns)
	{
		return;
	}
	ShownAirSigns = Signature;
	if (!AirSignMesh)
	{
		AirSignMesh = UTheaterStillMesh::Make(this, TEXT("AirSigns"));
		AirSignMesh->SetupAttachment(Root);
		AirSignMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AirSignMesh->RegisterComponent();
		AirSignMesh->SetCastShadow(false);
	}
	AirSignMesh->ClearAllMeshSections();
	if (!AirPennantMesh)
	{
		// (The pennants stand in a mesh of their own: ShowParkedPlanes sinks and hides the box aircraft close to, and a
		// pennant that went with them would be gone just where the player comes to look for his aircraft.)
		AirPennantMesh = UTheaterStillMesh::Make(this, TEXT("AirPennants"));
		AirPennantMesh->SetupAttachment(Root);
		AirPennantMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AirPennantMesh->RegisterComponent();
		AirPennantMesh->SetCastShadow(false);
	}
	AirPennantMesh->ClearAllMeshSections();
	FMiniatureMesh Pennants;
	for (UProceduralMeshComponent* Old : ParkedMeshes)
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	ParkedMeshes.Reset();
	ParkedFields.Reset();
	FMiniatureMesh Model;
	const double Field = UnitScale * 2.1 * 1.3; // (the size the airfields in use are built to)
	auto Ahead = [](double Turn, double Along, double Across) -> FVector
	{
		const double R = FMath::DegreesToRadians(Turn);
		return FVector(FMath::Cos(R) * Along - FMath::Sin(R) * Across, FMath::Sin(R) * Along + FMath::Cos(R) * Across, 0.0);
	};
	const FLinearColor Dural(FColor::FromHex(TEXT("B9BDB6"))), Tyre(FColor::FromHex(TEXT("2E2C2A")));
	for (const FViewAirfield& Each : Fields)
	{
		const FVector2D* Spot = AirfieldSpots.Find(Each.Region);
		const FVector2D* Lie = AirfieldLie.Find(Each.Region);
		if (Each.Groups <= 0 || !Spot || !Lie)
		{
			continue;
		}
		const FCampaignNation* Whose = Data.FindNation(Each.Nation);
		const FLinearColor HolderLivery = Whose ? FLinearColor(Whose->Color) : FLinearColor(0.5f, 0.5f, 0.5f);
		const double Turn = Lie->X, Wide = 1.5 + 0.3 * Lie->Y;
		const FVector At = SurfacePoint(Spot->X, Spot->Y, 0.02);
		// One small aircraft for each group, drawn up in a row on the grass on the side away from the hangars,
		// their noses to the runway: silver, with wings and fin tipped in their country's colour.
		// (Brian, 17 November 2026: "it says i have planes at an airfield but none are there". One aircraft stood for a
		// whole group of eighty and could not be found: now one for every twenty-five or so, never fewer than two.)
		const int32 Parked = Each.Aircraft > 0 ? FMath::Clamp(FMath::CeilToInt32(Each.Aircraft / 25.0f), FMath::Max(Each.Groups, 2), 6) : FMath::Min(Each.Groups, 6);
		// (These silver ones are what the table shows from working height and above. Close to, each field's aircraft
		// are its country's own fighters: see ShowParkedPlanes.)
		// (Brian, 24 November 2026 21:53Z: "when i have a unit on an allied airfield there is no way to know its there".) On a
		// field another country holds, the player's own groups have their share of the row, at its head, in his own colours,
		// and a pennant of his colour stands at that end of the row: his aircraft are there, and the eye finds them.
		const FCampaignNation* Mine = Each.YourGroups > 0 ? Data.FindNation(Each.YourNation) : nullptr;
		const int32 Yours = !Mine ? 0 : Each.YourGroups >= Each.Groups ? Parked
			: FMath::Clamp(Each.Aircraft > 0 ? FMath::RoundToInt32(Parked * float(Each.YourAircraft) / float(Each.Aircraft)) : Each.YourGroups, 1, Parked - 1);
		FParkedField& Kept = ParkedFields.AddDefaulted_GetRef();
		Kept.Spot = *Spot;
		Kept.Nation = Each.Nation;
		Kept.Turn = Turn;
		Kept.Wide = Wide;
		Kept.Parked = Parked;
		Kept.YourNation = Mine ? Each.YourNation : FString();
		Kept.Yours = Yours;
		if (Mine)
		{
			const FLinearColor His(Mine->Color);
			// (the mast is rooted below the grass, as the field's own slab is, where a full row puts it past the slab's end)
			const FVector Mast = At + Ahead(Turn, (-1.0 - (Parked - 1) * 0.5) * 0.62 * Field, -(Wide * 0.5 - 0.42) * Field);
			Pennants.Pillar(Mast - FVector(0.0, 0.0, 0.08 * Field), 0.025 * Field, 0.018 * Field, 1.03 * Field, Dural * 0.7f, 6);
			Pennants.Box(Mast + Ahead(Turn, 0.2 * Field, 0.0) + FVector(0.0, 0.0, 0.8 * Field), FVector(0.4, 0.025, 0.26) * Field, His, Turn);
		}
		for (int32 K = 0; K < Parked; ++K)
		{
			const FLinearColor Livery = (K < Yours && Mine) ? FLinearColor(Mine->Color) : HolderLivery;
			const FVector Foot = At + Ahead(Turn, (K - (Parked - 1) * 0.5) * 0.62 * Field, -(Wide * 0.5 - 0.42) * Field) + FVector(0.0, 0.0, 0.085 * Field);
			const double Nose = Turn + 90.0; // across the field, toward the runway
			Model.Box(Foot + FVector(0.0, 0.0, 0.07 * Field), FVector(0.5, 0.075, 0.075) * Field, Dural, Nose);
			Model.Box(Foot + Ahead(Nose, 0.06 * Field, 0.0) + FVector(0.0, 0.0, 0.075 * Field), FVector(0.11, 0.52, 0.022) * Field, Dural, Nose);
			for (const double Side : { -1.0, 1.0 })
			{
				Model.Box(Foot + Ahead(Nose, 0.06 * Field, Side * 0.22 * Field) + FVector(0.0, 0.0, 0.078 * Field), FVector(0.112, 0.1, 0.024) * Field, Livery, Nose);
			}
			Model.Box(Foot + Ahead(Nose, -0.21 * Field, 0.0) + FVector(0.0, 0.0, 0.08 * Field), FVector(0.07, 0.2, 0.02) * Field, Dural, Nose);
			Model.Box(Foot + Ahead(Nose, -0.22 * Field, 0.0) + FVector(0.0, 0.0, 0.14 * Field), FVector(0.09, 0.02, 0.1) * Field, Livery, Nose);
			Model.Box(Foot + Ahead(Nose, 0.255 * Field, 0.0) + FVector(0.0, 0.0, 0.07 * Field), FVector(0.02, 0.05, 0.05) * Field, Tyre, Nose);
		}
	}
	if (Model.Vertices.Num() > 0)
	{
		AirSignMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		AirSignMesh->SetMaterial(0, SceneryMaterial);
		SeamLaidAgain(AirSignMesh); // (seam-2: its twin over the chart's seam is laid again)
	}
	if (Pennants.Vertices.Num() > 0)
	{
		AirPennantMesh->CreateMeshSection(0, Pennants.Vertices, Pennants.Triangles, Pennants.Normals, TArray<FVector2D>(), Pennants.Colors, TArray<FProcMeshTangent>(), false);
		AirPennantMesh->SetMaterial(0, SceneryMaterial);
		SeamLaidAgain(AirPennantMesh); // (seam-2: its twin over the chart's seam is laid again)
	}
}

void ACampaignMapActor::ShowParkedPlanes(float CameraDistance, const FVector& CameraFocus)
{
	THEATER_SPEED_SCOPE(ShowParkedPlanes);
	// (The art director's ruling of round 21: a parked aircraft must be recognisably the one on its card, by its
	// outline, its hood, its blades, its marking and its paint. Close to, each airfield in use shows its country's
	// own fighter, its plainer model, standing on its wheels, in place of the silver box aircraft. Only close to,
	// and only the fields near what the eye looks at: every field's aircraft drawn always took a fifth off the
	// frame rate for something eight pixels long from working height. A field's aircraft are made the first time
	// they are wanted.)
	if (!AirSignMesh || ParkedFields.Num() == 0)
	{
		return;
	}
	const bool bClose = CameraDistance < (bParkedClose ? 3500.0f : 3200.0f);
	bParkedClose = bClose;
	const FVector2D Eye = WorldToMap(CameraFocus);
	const double Field = UnitScale * 2.1 * 1.3, Metre = 0.5 * Field / 9.0;
	// (The art director's note of round 23: the change from box to aircraft must not show as a pop. Neither is
	// swapped in a single frame: the boxes go down into the ground and the aircraft come up out of it, over about
	// a third of a second, as the eye comes close or a field comes near it, and the other way as it leaves.)
	const float Step = (GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f) / 0.32f;
	const double Sink = 0.4 * Field;
	const float BoxesWere = BoxesDown;
	BoxesDown = FMath::Clamp(BoxesDown + (bClose ? Step : -Step), 0.0f, 1.0f);
	if (BoxesDown != BoxesWere)
	{
		AirSignMesh->SetVisibility(BoxesDown < 1.0f);
		AirSignMesh->SetRelativeLocation(FVector(0.0, 0.0, -Sink * FMath::SmoothStep(0.0f, 1.0f, BoxesDown)));
	}
	const FLinearColor Dural(FColor::FromHex(TEXT("B9BDB6"))), Tyre(FColor::FromHex(TEXT("2E2C2A")));
	const bool bBothSides = OverSeam();
	for (FParkedField& Each : ParkedFields)
	{
		// (seam-4: a field a little way over the chart's seam is near the eye too, and its aircraft are then shown a
		// whole world to the eye's side, where the field's twin stands. They are built where the field was built.)
		const double SeenX = bBothSides ? NearX(Each.Spot.X, Eye.X) : Each.Spot.X;
		const double AsideNow = (SeenX - Each.Spot.X) * UnitScale;
		const bool bShow = bClose && FVector2D::DistSquared(Eye, FVector2D(SeenX, Each.Spot.Y)) < 240.0 * 240.0;
		const float UpWas = Each.Up;
		Each.Up = FMath::Clamp(Each.Up + (bShow ? Step : -Step), 0.0f, 1.0f);
		if (bShow == Each.bShown && Each.Up == UpWas && AsideNow == Each.Aside)
		{
			continue;
		}
		Each.bShown = bShow;
		Each.Aside = AsideNow;
		if (bShow && Each.Mesh == INDEX_NONE)
		{
			FMiniatureMesh Planes;
			const FVector At = SurfacePoint(Each.Spot.X, Each.Spot.Y, 0.02);
			auto Ahead = [](double Turn, double Along, double Across) -> FVector
			{
				const double R = FMath::DegreesToRadians(Turn);
				return FVector(FMath::Cos(R) * Along - FMath::Sin(R) * Across, FMath::Sin(R) * Along + FMath::Cos(R) * Across, 0.0);
			};
			const FString Fighter = FMiniatureMesh::MainFighterOf(Each.Nation);
			// (the player's own share of an ally's field: his own fighters, at the head of the row; see ShowAirfieldSigns)
			const FString HisFighter = Each.Yours > 0 ? FMiniatureMesh::MainFighterOf(Each.YourNation) : Fighter;
			const double Nose = Each.Turn + 90.0; // across the field, toward the runway
			for (int32 K = 0; K < Each.Parked; ++K)
			{
				const FVector Foot = At + Ahead(Each.Turn, (K - (Each.Parked - 1) * 0.5) * 0.62 * Field, -(Each.Wide * 0.5 - 0.42) * Field) + FVector(0.0, 0.0, 0.085 * Field);
				if (Planes.Vehicle(K < Each.Yours ? HisFighter : Fighter, Foot + FVector(0.0, 0.0, 1.35 * Metre), Metre, Nose + 180.0, false))
				{
					for (const double Side : { -1.0, 1.0 })
					{
						// (its main wheels, down on the grass under the wings, and their legs)
						const FVector Wheel = Foot + Ahead(Nose, 0.7 * Metre, Side * 1.25 * Metre) + FVector(0.0, 0.0, 0.3 * Metre);
						Planes.Box(Wheel, FVector(0.62, 0.2, 0.62) * Metre, Tyre, Nose);
						Planes.Box(Wheel + FVector(0.0, 0.0, 0.45 * Metre), FVector(0.12, 0.12, 0.6) * Metre, Dural * 0.6f, Nose);
					}
				}
			}
			UProceduralMeshComponent* Made = UTheaterStillMesh::Make(this, TEXT("ParkedPlanes"));
			Made->SetupAttachment(Root);
			Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Made->RegisterComponent();
			Made->SetCastShadow(false);
			if (Planes.Vertices.Num() > 0)
			{
				Made->CreateMeshSection(0, Planes.Vertices, Planes.Triangles, Planes.Normals, TArray<FVector2D>(), Planes.Colors, TArray<FProcMeshTangent>(), false);
				Made->SetMaterial(0, SceneryMaterial);
			}
			Each.Mesh = ParkedMeshes.Add(Made);
		}
		if (ParkedMeshes.IsValidIndex(Each.Mesh) && ParkedMeshes[Each.Mesh])
		{
			ParkedMeshes[Each.Mesh]->SetVisibility(Each.Up > 0.0f);
			ParkedMeshes[Each.Mesh]->SetRelativeLocation(FVector(0.0, Each.Aside, -Sink * (1.0f - FMath::SmoothStep(0.0f, 1.0f, Each.Up))));
		}
	}
}

void ACampaignMapActor::ShowScars(const TArray<int32>& Scars)
{
	THEATER_SPEED_SCOPE(ShowScars);
	if (!Data.IsLoaded() || (ScarsMesh && Scars == ShownScars))
	{
		return;
	}
	ShownScars = Scars;
	if (!ScarsMesh)
	{
		ScarsMesh = UTheaterStillMesh::Make(this, TEXT("Scars"));
		ScarsMesh->SetupAttachment(Root);
		ScarsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ScarsMesh->RegisterComponent();
	}
	ScarsMesh->ClearAllMeshSections();
	Smoulders.Reset();
	FMiniatureMesh Model;
	const double U = UnitScale;
	const FLinearColor Rim(FColor::FromHex(TEXT("6A553D"))), Pit(FColor::FromHex(TEXT("2B231B"))), Burnt(FColor::FromHex(TEXT("2E2724"))), Rust(FColor::FromHex(TEXT("5A3A2A"))), Char(FColor::FromHex(TEXT("191614"))), Timber(FColor::FromHex(TEXT("4A3A2A")));
	for (int32 K = 0; K + 2 < Scars.Num(); K += 3)
	{
		const int32 RegionId = Scars[K], Age = Scars[K + 1], Big = FMath::Clamp(Scars[K + 2], 1, 3);
		if (!Data.Regions.IsValidIndex(RegionId) || Age > 3)
		{
			continue;
		}
		const FCampaignRegion& Region = Data.Regions[RegionId];
		FRandomStream Random(RegionId * 3571 + 17);
		// The field: in the open country of the province, away from its town, its pieces and the quarter south of it.
		FVector2D Field = Region.Centroid;
		if (FVector2D::Distance(Field, Region.Center) < TownRadius(RegionId) + 9.6)
		{
			bool bFound = false;
			const double Turn = Random.FRandRange(0.0f, 360.0f);
			for (int32 Try = 0; Try < 8 && !bFound; ++Try)
			{
				// (west, north-west and north first: the east is the army's, the south the town's new quarter)
				const double Angle = FMath::DegreesToRadians(200.0 + Try * 45.0 + Turn * 0.1);
				const FVector2D At = Region.Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (TownRadius(RegionId) + 12.6);
				if (GroundAt(At.X, At.Y) > 0.0f && Data.FindRegionAt(At.X, At.Y) == RegionId)
				{
					Field = At;
					bFound = true;
				}
			}
		}
		auto Spot = [&](double Reach, FVector2D& Out) -> bool
		{
			for (int32 Try = 0; Try < 6; ++Try)
			{
				const double Angle = Random.FRandRange(0.0f, 6.2832f), Far = Reach * FMath::Sqrt(Random.FRandRange(0.04f, 1.0f));
				Out = Field + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Far;
				if (GroundAt(Out.X, Out.Y) > 0.0f)
				{
					return true;
				}
			}
			return false;
		};
		// Craters: fewer as the months go by and the ground heals.
		const int32 Craters = FMath::Max(1, (2 + Big * 3) * (4 - Age) / 4);
		for (int32 C = 0; C < Craters; ++C)
		{
			FVector2D At;
			if (!Spot(4.5 + Big * 1.4, At))
			{
				continue;
			}
			const double Wide = Random.FRandRange(0.75f, 1.5f) * U;
			const FVector Foot = SurfacePoint(At.X, At.Y, 0.01);
			Model.Pillar(Foot, Wide * 1.3, Wide * 0.92, 0.13 * U, Rim, 9);
			Model.Pillar(Foot + FVector(0.0, 0.0, 0.11 * U), Wide * 0.84, Wide * 0.7, 0.03 * U, Pit, 9);
		}
		// Wrecks: burnt-out tanks and lorries, cleared away after a couple of months.
		const int32 Wrecks = Age >= 3 ? 0 : FMath::Max(1, (Big + 1) * (3 - Age) / 3 + (Age == 0 ? 1 : 0));
		for (int32 W = 0; W < Wrecks; ++W)
		{
			FVector2D At;
			if (!Spot(4.0 + Big * 1.2, At))
			{
				continue;
			}
			const FVector Foot = SurfacePoint(At.X, At.Y, 0.02);
			const double Turn = Random.FRandRange(0.0f, 360.0f), S = 1.4 * U;
			const double Cos = FMath::Cos(FMath::DegreesToRadians(Turn)), Sin = FMath::Sin(FMath::DegreesToRadians(Turn));
			auto At3 = [&](double Along, double Across, double Up) { return Foot + FVector(Along * Cos - Across * Sin, Along * Sin + Across * Cos, Up) * S; };
			if (W % 3 != 2)
			{
				// A tank: its hull, the turret blown askew, the gun drooping, a track thrown off beside it.
				Model.Box(At3(0.0, 0.0, 0.28), FVector(2.1, 1.15, 0.5) * S, W % 2 == 0 ? Burnt : Rust, Turn);
				Model.Box(At3(0.2, 0.25, 0.68), FVector(0.95, 0.85, 0.36) * S, Char, Turn + 24.0, 12.0);
				Model.Rod(At3(0.6, 0.45, 0.7), At3(1.7, 0.95, 0.42), 0.07 * S, Char, 5);
				Model.Box(At3(-0.3, -0.95, 0.06), FVector(1.5, 0.3, 0.1) * S, Char, Turn + 9.0);
			}
			else
			{
				// A lorry on its side, and what it carried.
				Model.Box(At3(0.0, 0.0, 0.32), FVector(1.7, 0.5, 0.64) * S, Rust, Turn, 0.0);
				Model.Box(At3(0.95, 0.0, 0.28), FVector(0.55, 0.5, 0.56) * S, Burnt, Turn);
				Model.Box(At3(-0.5, 0.75, 0.12), FVector(0.45, 0.45, 0.24) * S, Timber, Turn + 30.0);
				Model.Box(At3(0.2, 0.9, 0.1), FVector(0.4, 0.4, 0.2) * S, Timber, Turn - 15.0);
			}
			if (Age <= 1 && (Age == 0 || W == 0))
			{
				Smoulders.Add(At3(0.0, 0.0, 0.8)); // still burning
			}
		}
	}
	if (Model.Vertices.Num() > 0)
	{
		ScarsMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		ScarsMesh->SetMaterial(0, SceneryMaterial);
		SeamLaidAgain(ScarsMesh); // (seam-2: its twin over the chart's seam is laid again)
	}
}

/** What the builder of a town is told of the place (see FTownSite): the table's towns and the studio's picture of one are built from the same. */
void ACampaignMapActor::FillTownSite(const FCampaignRegion& Region, FMiniatureMesh::FTownSite& Site)
{
	const double U = UnitScale;
	const bool bCity = Region.Terrain == TEXT("urban");
	const bool bBig = Region.bCapital || bCity || Region.VictoryPoints >= 2 || Region.Factories >= 2;
	Site.Centre = Region.Center;
	Site.Radius = TownRadius(Region.Id);
	Site.Unit = U * 2.1;
	Site.MapUnit = U;
	// (a city that gave way to a greater town beside it is built as a town: see TownSpread)
	Site.Tier = Region.bCapital ? 3 : bCity ? (TownHumbled.IsValidIndex(Region.Id) && TownHumbled[Region.Id] ? 1 : 2) : bBig ? 1 : 0;
	Site.Factories = Region.Factories;
	Site.Ruined = FMath::Clamp(WreckedIn.FindRef(Region.Id), 0, Region.Factories); // (bombed-out works: the first of them are wrecks)
	Site.Seed = Region.Id;
	Site.bGrand = GrandCitiesWanted(); // (release 2, grander great cities: the builder keeps it to cities and capitals)
	Site.bGrandFar = GrandCitiesFarWanted(); // (and the far side's own kits, behind their own switch)
	{
		// Roofs and walls by where it lies (the map is forty units to the degree of latitude).
		const double Latitude = 71.4 - Region.Center.Y / 40.0, Longitude = -11.0 + Region.Center.X / 25.7;
		Site.Look = Latitude < 37.5 || (Latitude < 42.0 && Longitude > 26.0) ? 4 : Latitude < 45.5 ? 2
			: (Latitude > 55.0 && Longitude > 4.0) || Longitude > 22.0 ? 3 : Longitude < 7.5 ? 1 : 0;
		// (Release 2: the wider chart counts the Pacific's longitudes eastward past 180. East of Greenwich, -180 to 180.)
		const double East = FMath::Fmod(Longitude + 540.0, 360.0) - 180.0;
		const bool bBeyond = Data.bWorld && Data.FirstBeyond != INDEX_NONE && Region.Id >= Data.FirstBeyond;
		if (bBeyond && Latitude > 45.0 && (East < -150.0 || East > 165.0))
		{
			// (Release 2: Alaska's west and the Aleutians, the New World's kit)
			Site.Look = 5;
		}
		else if (bBeyond && East < -40.0 && East > -169.0 && !(Latitude < 30.0 && East < -125.0))
		{
			// (The art director's brief 3.4 and her round 30: in the Americas a town takes the kit of the land it
			// stands in. North of Mexico, the New World's brick and cream under tar; from Mexico south, the plaza
			// town's white and ochre under red tile. The islands of the eastern Atlantic keep Europe's rule.)
			const bool bNorth = Latitude > 33.0 || ((Region.Home == TEXT("USA") || Region.Home == TEXT("CAN")) && Latitude > 24.5);
			Site.Look = bNorth ? 5 : 6;
		}
		else if (bBeyond)
		{
			// (Release 2, the far side, by the same brief, 3.4: the kit of the land a town stands in, whoever holds it.
			// Home is whose it was when the chart was drawn; the lines are the brief's, a degree or two wide of the coasts.)
			const FString& Home = Region.Home;
			const bool bJapan = Home == TEXT("JAP") && ((East >= 129.5 && East <= 156.5 && Latitude >= 30.0 && Latitude <= 50.0 && !(East < 131.0 && Latitude > 34.5) && !(East < 141.0 && Latitude > 46.0))
				|| (Latitude >= 24.0 && Latitude < 30.0 && ((East >= 122.9 && East <= 131.5) || (East >= 139.0 && East <= 143.0))));
			const bool bChinaCoast = East >= 102.0 && East <= 131.0 && Latitude >= 18.0 && Latitude <= 43.5;
			const bool bWalled = ((Home == TEXT("CHN") || Home == TEXT("MAN")) && East >= 102.0)
				|| ((Home == TEXT("JAP") || Home == TEXT("UK") || Home == TEXT("POR")) && bChinaCoast) || (Home == TEXT("FRA") && bChinaCoast && East >= 109.5);
			const bool bIndia = (Home == TEXT("UK") || Home == TEXT("NEP") || Home == TEXT("POR") || Home == TEXT("FRA")) && East >= 60.0 && East <= 101.0
				&& Latitude >= 5.5 && Latitude <= 37.0 && !(East > 99.5 && Latitude < 7.0);
			const bool bNewWorld = (Home == TEXT("AUS") && Latitude <= -10.5) || (Home == TEXT("NZL") && Latitude <= -30.0) || Home == TEXT("SAF");
			if (bNewWorld)
			{
				Site.Look = 5; // Australia, New Zealand, South Africa: the New World grid
			}
			else if (bJapan)
			{
				Site.Look = 8; // the timber town
			}
			else if (bWalled)
			{
				Site.Look = 7; // the walled town: China, Manchuria, Korea, Formosa, the treaty ports
			}
			else if (bIndia)
			{
				Site.Look = 10; // the Indian town: India, Ceylon, Burma, Nepal
			}
			else if (Home == TEXT("SOV") && !(Latitude < 46.0 && East < 87.0))
			{
				Site.Look = 11; // the railway town: Siberia and the Soviet Far East
			}
			else if ((Latitude < 24.0 && (East > 92.0 || East < -125.0)) || (Latitude < 12.0 && East > -18.0 && East < 92.0))
			{
				Site.Look = 9; // thatch and tin: Siam, Indochina, Malaya, the Indies, the Philippines, the islands, Africa's middle
			}
			else if (Latitude >= 12.0 && East > -18.0 && East < 122.0)
			{
				Site.Look = 4; // the flat-roofed town: the Sahara, Arabia, Persia, Afghanistan, Central Asia, Tibet, Mongolia
			}
			else if (East > 102.0 || East < -125.0)
			{
				Site.Look = Latitude < 30.0 ? 9 : 5; // what is left of the Pacific's shores and islands
			}
		}
		// A capital's own great building: the great capitals have theirs, the rest one of four by turns.
		static const TPair<const TCHAR*, int32> Named[] = {
			TPair<const TCHAR*, int32>(TEXT("Berlin"), 8), TPair<const TCHAR*, int32>(TEXT("Paris"), 4), TPair<const TCHAR*, int32>(TEXT("London"), 5),
			TPair<const TCHAR*, int32>(TEXT("Moscow"), 6), TPair<const TCHAR*, int32>(TEXT("Rome"), 7), TPair<const TCHAR*, int32>(TEXT("Warsaw"), 2),
			TPair<const TCHAR*, int32>(TEXT("Budapest"), 3), TPair<const TCHAR*, int32>(TEXT("Madrid"), 0), TPair<const TCHAR*, int32>(TEXT("Athens"), 7),
			TPair<const TCHAR*, int32>(TEXT("Istanbul"), 1), TPair<const TCHAR*, int32>(TEXT("Stockholm"), 0), TPair<const TCHAR*, int32>(TEXT("Brussels"), 3),
		};
		Site.Landmark = (Region.Id * 7 + 1) % 4;
		for (const TPair<const TCHAR*, int32>& Each : Named)
		{
			if (Region.Name == Each.Key)
			{
				Site.Landmark = Each.Value;
			}
		}
	}
	Site.Ground = [this](double X, double Y, double Lift) { return SurfacePoint(X, Y, Lift); };
	// (a town is built on land, and on its own province's: one that stands by the sea or a frontier grows away from it)
	// (and a greater town leaves green beside a lesser one that gave way to it: the art director's rule for close towns)
	const TArray<FVector>* const Green = TownClear.Find(Region.Id);
	const TArray<FTownKeepOff>* const Beside = TownKeepOff.Find(Region.Id);
	Site.Dry = [this, Id = Region.Id, Clear = Green ? *Green : TArray<FVector>(), KeepOff = Beside ? *Beside : TArray<FTownKeepOff>()](double X, double Y)
	{
		for (const FVector& Each : Clear)
		{
			if (FMath::Square(X - Each.X) + FMath::Square(Y - Each.Y) < FMath::Square(Each.Z))
			{
				return false;
			}
		}
		// (two towns that both kept their size beside one another: neither builds nearer than its own distance to the
		// ground of the other's province that the other's roofs may reach, looked for at that distance and at two
		// thirds and a third of it, all round.)
		for (const FTownKeepOff& Each : KeepOff)
		{
			if (FMath::Square(X - Each.Middle.X) + FMath::Square(Y - Each.Middle.Y) >= FMath::Square(Each.Reach + Each.Off))
			{
				continue;
			}
			for (int32 K = 0; K < 24; ++K)
			{
				const double Turn = (K % 8) * UE_DOUBLE_PI / 4.0, Out = Each.Off * (3 - K / 8) / 3.0;
				const double Px = X + FMath::Cos(Turn) * Out, Py = Y + FMath::Sin(Turn) * Out;
				if (FMath::Square(Px - Each.Middle.X) + FMath::Square(Py - Each.Middle.Y) < FMath::Square(Each.Reach) && Data.FindRegionAt(Px, Py) == Each.Province)
				{
					return false;
				}
			}
		}
		return GroundAt(X, Y) > 0.0f && Data.FindRegionAt(X, Y) == Id;
	};
	Site.bPacked = IsTownPacked(Region.Id);
	// The roads and railways out of it run toward the towns of the provinces next door.
	for (const int32 Next : Region.Neighbors)
	{
		if (Data.Regions.IsValidIndex(Next))
		{
			const FVector2D Way = Data.Regions[Next].Center - Region.Center;
			Site.Arms.Add(FMath::RadiansToDegrees(FMath::Atan2(Way.X, -Way.Y)));
		}
	}
	if (Region.bPort)
	{
		// A port's quay stands at the water's edge: the nearest shore to the town's middle, looked for all round.
		auto LookForShore = [&](TFunctionRef<bool(const FVector2D&)> Water)
		{
			double Nearest = Site.Radius + 16.0;
			for (double Bearing = 0.0; Bearing < 360.0; Bearing += 10.0)
			{
				const FVector2D Way(FMath::Sin(FMath::DegreesToRadians(Bearing)), -FMath::Cos(FMath::DegreesToRadians(Bearing)));
				for (double Out = 1.5; Out < Nearest; Out += 0.5)
				{
					const FVector2D Wet = Region.Center + Way * Out, Beyond = Region.Center + Way * (Out + 3.0);
					if (Water(Wet))
					{
						// (open water, not a river's mouth or a pond: it is still water three units further out)
						if (Water(Beyond) && Out > 2.5)
						{
							Nearest = Out;
							Site.bQuay = true;
							Site.Quay = Region.Center + Way * (Out - 0.25);
							Site.SeaBearing = Bearing;
						}
						break;
					}
				}
			}
		};
		LookForShore([this](const FVector2D& At) { return GroundAt(At.X, At.Y) <= 0.0f; });
		if (!Site.bQuay && Data.bWorld && Data.FirstBeyond != INDEX_NONE && Region.Id >= Data.FirstBeyond && WorldTerrain.HasElevation())
		{
			// (Beyond Europe a town is built on the world's coarse sheet of heights, whose cells are too large to
			// show a narrow bay: Baltimore on the Chesapeake was a port with no quay. Where the coarse sheet finds no
			// shore, the real heights the fine sheets are made from are asked: water is where they lie at or below
			// the sea, there and a unit round, as a fine sheet's smoothed coast will show it.)
			LookForShore([this](const FVector2D& At)
			{
				for (const FVector2D& Step : { FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(-1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(0.0, -1.0) })
				{
					if (WorldTerrain.MetresAt(WrapX(At.X + Step.X), At.Y + Step.Y) > 0.0f)
					{
						return false;
					}
				}
				return true;
			});
			if (Site.bQuay)
			{
				UE_LOG(LogTheater, Verbose, TEXT("Towns: %s has its quay from the real heights' coast"), *Region.Name);
			}
		}
	}
	Site.bPlots = bTownPlots && CivicInside(Region.Id);
	Site.PlotsNear = CivicNear(Region.Id);
	Site.PlotsTurn = CivicTurn(Region.Id);
}

void ACampaignMapActor::BuildTowns()
{
	THEATER_SPEED_SCOPE(BuildTowns);
	TownMarkers->ClearInstances();
	CapitalMarkers->ClearInstances();
	if (!TownMesh)
	{
		TownMesh = NewObject<UProceduralMeshComponent>(this);
		TownMesh->SetupAttachment(Root);
		TownMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TownMesh->RegisterComponent();
	}
	TownMesh->ClearAllMeshSections();
	if (!TownFarMesh)
	{
		TownFarMesh = NewObject<UProceduralMeshComponent>(this);
		TownFarMesh->SetupAttachment(Root);
		TownFarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TownFarMesh->RegisterComponent();
		TownFarMesh->SetCastShadow(false);
	}
	TownFarMesh->ClearAllMeshSections();
	TownsFarShown = -1;
	// (Speed, 17 November 2026; "tc.Speed.TownSquares".) The towns are cut into squares of country, each square a mesh
	// of its own in its near form and its far: only the squares in view are drawn, and only those near a change are
	// drawn again into the shadows. Each town is built exactly as before, into its square's mesh in place of the one
	// mesh for the world. With the switch at 0 there is one square, and it is TownMesh and TownFarMesh as they were.
	const bool bSquares = TheaterSpeed::TownSquares();
	TownsCut = bSquares ? 1 : 0;
	TownsGrand = (GrandCitiesWanted() ? 1 : 0) + (GrandCitiesFarWanted() ? 2 : 0); // (release 2, grander great cities: how the towns are built this time)
	const double SquareSide = 260.0; // (in map units: twice a square of the woods, a few to the width of the view from playing height)
	TMap<FIntPoint, int32> SquareOf;
	TArray<FMiniatureMesh> NearModels, FarModels; // (FarModels: the same towns for the eye high up, see FTownSite::bFar)
	auto SquareFor = [&](const FVector2D& Where)
	{
		const FIntPoint Key = bSquares ? FIntPoint(FMath::FloorToInt32(Where.X / SquareSide), FMath::FloorToInt32(Where.Y / SquareSide)) : FIntPoint(0, 0);
		if (const int32* Known = SquareOf.Find(Key))
		{
			return *Known;
		}
		const int32 Index = NearModels.Num();
		NearModels.AddDefaulted();
		FarModels.AddDefaulted();
		NearModels[Index].Windows = &TownWindows;
		SquareOf.Add(Key, Index);
		return Index;
	};

	// Every town of note stands on the table as a cluster of houses: a few for a town, more for a city, a domed
	// palace in the middle of a capital, and a works with a chimney for each pair of factories the province has.
	const double U = UnitScale;
	int32 Count = 0;
	TownChimneys.Reset();
	TownChimneyRegion.Reset();
	TownWindows.Reset();
	PaneRegion.Reset();
	TownQuarters.Reset();
	TownBeacons.Reset();
	TownLandmarks.Reset();
	TownQuays.Reset();
	bPortRoadsStale = true; // (release 2, busy ports: the quays are found again, and so are the fairways off them)
	TownAvenues.Reset();
	bStreetLinesStale = true; // (release 2, street cars: the avenues are laid again, and so are the cars' lines on them)
	for (const FCampaignRegion& Region : Data.Regions)
	{
		const bool bCity = Region.Terrain == TEXT("urban");
		const bool bBig = Region.bCapital || bCity || Region.VictoryPoints >= 2 || Region.Factories >= 2;
		const bool bTown = bBig || Region.VictoryPoints > 0 || Region.Factories > 0 || Region.bOil || Region.bPort;
		if (!bTown)
		{
			continue;
		}
		const int32 Square = SquareFor(Region.Center);
		FMiniatureMesh& Model = NearModels[Square];
		FMiniatureMesh& FarModel = FarModels[Square];
		// What the place is worth, where the eye can count it: a stack of gold coins beside the town, one for each
		// point it scores toward victory.
		if (Region.VictoryPoints > 0)
		{
			const FVector Foot = SurfacePoint(Region.Center.X + 3.6 * TownSpread(Region.Id), Region.Center.Y + 1.4 * TownSpread(Region.Id), 0.02);
			const FLinearColor Gold(FColor::FromHex(TEXT("D9A521"))), GoldEdge(FColor::FromHex(TEXT("9C7412")));
			for (int32 Coin = 0; Coin < FMath::Min(Region.VictoryPoints, 9); ++Coin)
			{
				Model.Pillar(Foot + FVector(0.0, 0.0, Coin * 0.3 * U), 0.62 * U, 0.62 * U, 0.2 * U, Gold, 14);
				Model.Pillar(Foot + FVector(0.0, 0.0, Coin * 0.3 * U + 0.2 * U), 0.56 * U, 0.56 * U, 0.1 * U, GoldEdge, 14);
			}
		}
		// Oil: two derricks and a storage tank.
		if (Region.bOil)
		{
			const FVector Foot = SurfacePoint(Region.Center.X - 4.2 * TownSpread(Region.Id), Region.Center.Y - 2.6 * TownSpread(Region.Id), 0.02);
			const FLinearColor Iron(FColor::FromHex(TEXT("2B2A28"))), Tank(FColor::FromHex(TEXT("9A9C9E")));
			for (int32 Rig = 0; Rig < 2; ++Rig)
			{
				const FVector At = Foot + FVector(Rig * 1.5 * U, Rig * 0.9 * U, 0.0);
				Model.Pillar(At, 0.55 * U, 0.08 * U, 3.0 * U, Iron, 4);
				Model.Box(At + FVector(0.0, 0.0, 3.05 * U), FVector(0.3, 0.3, 0.12) * U, Iron);
			}
			Model.Pillar(Foot + FVector(-1.3 * U, 0.6 * U, 0.0), 0.7 * U, 0.7 * U, 0.8 * U, Tank, 12);
		}
		const int32 WindowsBefore = TownWindows.Num();
		// (Art fix 9: the town is built to a plan of its own, with streets and blocks, and by the manner of its part
		// of Europe. Its houses are the size houses always were: it is the town that is larger, not the houses.)
		FMiniatureMesh::FTownSite Site;
		FillTownSite(Region, Site);
		if (Site.bQuay)
		{
			TownQuays.Add(Region.Id, Site.Quay);
		}
		TArray<FVector2D> Quarters;
		FVector Beacon = FVector::ZeroVector, LandmarkTop = FVector::ZeroVector;
		TArray<FVector2D> Avenues; // (release 2, street cars: where a car may run on each arm of a city's avenues)
		Model.Township(Site, &TownChimneys, &Quarters, &Beacon, &LandmarkTop, &Avenues);
		if (Avenues.Num() >= 2)
		{
			TownAvenues.Add(Region.Id, MoveTemp(Avenues));
		}
		TownChimneyRegion.SetNumZeroed(TownChimneys.Num()); // (the new ones are this town's)
		for (int32 Stack = TownChimneyRegion.Num() - 1; Stack >= 0 && TownChimneyRegion[Stack] == 0; --Stack)
		{
			TownChimneyRegion[Stack] = Region.Id + 1;
		}
		if (!LandmarkTop.IsZero())
		{
			TownLandmarks.Add(Region.Id, LandmarkTop);
		}
		Site.bFar = true;
		FarModel.Township(Site);
		TownQuarters.Add(Region.Id, MoveTemp(Quarters));
		if (!Beacon.IsZero())
		{
			TownBeacons.Add(Region.Id, Beacon);
		}
		for (int32 Pane = WindowsBefore / 2; Pane < TownWindows.Num() / 2; ++Pane)
		{
			PaneRegion.Add(Region.Id);
		}
		++Count;
	}
	// (the squares' meshes are kept from one building to the next: those not wanted this time are left empty)
	auto SquareMesh = [this](TArray<TObjectPtr<UProceduralMeshComponent>>& Pool, int32 Index, bool bFar)
	{
		while (Pool.Num() <= Index)
		{
			UProceduralMeshComponent* Made = UTheaterStillMesh::Make(this, bFar ? TEXT("TownSquareFar") : TEXT("TownSquare"));
			Made->SetupAttachment(Root);
			Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (bFar || TownShadowsCast == 0)
			{
				Made->SetCastShadow(false); // (as TownFarMesh; a near square throws its shadows as TownMesh does)
			}
			Made->RegisterComponent();
			Pool.Add(Made);
		}
		return Pool[Index].Get();
	};
	auto Fill = [this](UProceduralMeshComponent* Mesh, const FMiniatureMesh& Each)
	{
		Mesh->ClearAllMeshSections();
		if (Each.Vertices.Num() > 0)
		{
			Mesh->CreateMeshSection(0, Each.Vertices, Each.Triangles, Each.Normals, TArray<FVector2D>(), Each.Colors, TArray<FProcMeshTangent>(), false);
			Mesh->SetMaterial(0, SceneryMaterial);
		}
	};
	int32 NearPoints = 0, FarPoints = 0;
	for (int32 Index = 0; Index < NearModels.Num(); ++Index)
	{
		NearPoints += NearModels[Index].Vertices.Num();
		FarPoints += FarModels[Index].Vertices.Num();
		Fill(bSquares ? SquareMesh(TownSquares, Index, false) : TownMesh.Get(), NearModels[Index]);
		Fill(bSquares ? SquareMesh(TownFarSquares, Index, true) : TownFarMesh.Get(), FarModels[Index]);
	}
	for (int32 Index = bSquares ? NearModels.Num() : 0; Index < TownSquares.Num(); ++Index)
	{
		if (TownSquares[Index])
		{
			TownSquares[Index]->ClearAllMeshSections();
		}
	}
	for (int32 Index = bSquares ? FarModels.Num() : 0; Index < TownFarSquares.Num(); ++Index)
	{
		if (TownFarSquares[Index])
		{
			TownFarSquares[Index]->ClearAllMeshSections();
		}
	}
	UE_LOG(LogTheater, Log, TEXT("Towns: %d, in %d points (and %d from high up), in %d square%s of country"), Count, NearPoints, FarPoints,
		bSquares ? NearModels.Num() : 1, bSquares && NearModels.Num() != 1 ? TEXT("s") : TEXT(""));
	UE_LOG(LogTheater, Log, TEXT("Grander great cities (tc.Map.GrandCities): %s"), (TownsGrand & 1) != 0 ? TEXT("on, the cities and capitals stand in their grander form") : TEXT("off"));
	UE_LOG(LogTheater, Log, TEXT("Grander great cities, the far side (tc.Map.GrandCitiesFar): %s"), (TownsGrand & 2) != 0 ? TEXT("on, the walled, timber, thatch and Indian towns stand in theirs") : TEXT("off"));

	// The windows, each a small warm pane a hair proud of its wall: dark by day, lit as night comes on.
	if (!LightsMesh)
	{
		LightsMesh = NewObject<UProceduralMeshComponent>(this);
		LightsMesh->SetupAttachment(Root);
		LightsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LightsMesh->RegisterComponent();
		LightsMesh->SetCastShadow(false);
	}
	LightsMesh->ClearAllMeshSections();
	// (seam-4: and their twin over the seam is emptied, to be filled from the new lights when it is next wanted)
	bLightsTwinStale = true;
	if (LightsTwin)
	{
		LightsTwin->ClearAllMeshSections();
	}
	LightColours.Reset();
	LightPoints.Reset();
	ShownNight = -1.0f;
	// (the towns have been built again: a victor's flag is flown again from where its landmark's top now is)
	bVictorsStale = true;
	ShowVictorFlags();
	const int32 Panes = TownWindows.Num() / 2;
	if (Panes > 0 && HighlightMaterial)
	{
		TArray<FVector>& Points = LightPoints;
		TArray<FVector> Normals;
		TArray<int32> Triangles;
		Points.Reset(Panes * 4);
		const double Wide = 0.075 * U * 2.1, High = 0.09 * U * 2.1;
		for (int32 Pane = 0; Pane < Panes; ++Pane)
		{
			const FVector At = TownWindows[Pane * 2], Facing = TownWindows[Pane * 2 + 1].GetSafeNormal();
			const FVector Side = FVector(-Facing.Y, Facing.X, 0.0) * Wide, Up(0.0, 0.0, High);
			const FVector Proud = At + Facing * (0.012 * U);
			const int32 First = Points.Num();
			Points.Append({ Proud - Side - Up, Proud + Side - Up, Proud + Side + Up, Proud - Side + Up });
			Triangles.Append({ First, First + 1, First + 2, First, First + 2, First + 3, First, First + 2, First + 1, First, First + 3, First + 2 });
		}
		Normals.Init(FVector::UpVector, Points.Num());
		LightColours.Init(FColor(0, 0, 0, 0), Points.Num());
		LightsMesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), LightColours, TArray<FProcMeshTangent>(), false);
		LightsMesh->SetMaterial(0, HighlightMaterial);

		// And beyond the towns, the lights of villages and farms scattered over the settled low ground: each a small
		// round glow lying on the land, bright in the middle and gone at its edge, seen from high up as a country's own.
		VillagePoints.Reset();
		VillageColours.Reset();
		VillageRegion.Reset();
		{
			TArray<int32> VillageTriangles;
			FRandomStream Random(4177);
			for (const FCampaignRegion& Region : Data.Regions)
			{
				const int32 Wanted = Region.Terrain == TEXT("urban") ? 28 : Region.Terrain == TEXT("plains") ? 20 : Region.Terrain == TEXT("hills") ? 12
					: Region.Terrain == TEXT("forest") ? 8 : Region.Terrain == TEXT("marsh") ? 4 : Region.Terrain == TEXT("mountain") ? 2 : 0;
				if (!Region.Bounds.bIsValid)
				{
					continue;
				}
				for (int32 Light = 0, Tries = 0; Light < Wanted && Tries < Wanted * 6; ++Tries)
				{
					const double X = Random.FRandRange(Region.Bounds.Min.X, Region.Bounds.Max.X), Y = Random.FRandRange(Region.Bounds.Min.Y, Region.Bounds.Max.Y);
					const float Ground = GroundAt(X, Y);
					if (Ground <= 0.02f || Ground > 2.2f || Data.FindRegionAt(X, Y) != Region.Id)
					{
						continue;
					}
					++Light;
					const FVector At = SurfacePoint(X, Y, 0.14);
					const double Round = Random.FRandRange(0.5f, 0.9f) * U;
					const int32 First = VillagePoints.Num();
					VillagePoints.Add(At);
					for (int32 K = 0; K < VillageRim; ++K)
					{
						const double Angle = K * UE_DOUBLE_TWO_PI / VillageRim;
						VillagePoints.Add(At + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Round);
						VillageTriangles.Append({ First, First + 1 + K, First + 1 + (K + 1) % VillageRim, First, First + 1 + (K + 1) % VillageRim, First + 1 + K });
					}
					VillageRegion.Add(Region.Id);
				}
			}
			if (VillagePoints.Num() > 0)
			{
				TArray<FVector> VillageNormals;
				VillageNormals.Init(FVector::UpVector, VillagePoints.Num());
				VillageColours.Init(FColor(0, 0, 0, 0), VillagePoints.Num());
				LightsMesh->CreateMeshSection(2, VillagePoints, VillageTriangles, VillageNormals, TArray<FVector2D>(), VillageColours, TArray<FProcMeshTangent>(), false);
				LightsMesh->SetMaterial(2, HighlightMaterial);
			}
		}

		// The glow of a whole town's lamps, for the view from high up: a small hot heart over each town with a soft
		// fall round it, lifted clear of the roofs so that no building shows dark through its middle. A city has a
		// few lit quarters beside it, a capital more. And under every lit town a very wide, very faint warm wash
		// lies on the country round it.
		GlowPoints.Reset();
		GlowColours.Reset();
		GlowRegion.Reset();
		GlowKind.Reset();
		TArray<int32> GlowTriangles;
		auto AddGlow = [this, &GlowTriangles](const FVector& Middle, double Spread, int32 RegionId, uint8 Kind)
		{
			static const double RingAt[GlowRings] = { 0.2, 0.5, 1.0 };
			const int32 First = GlowPoints.Num();
			GlowPoints.Add(Middle);
			for (int32 Ring = 0; Ring < GlowRings; ++Ring)
			{
				for (int32 K = 0; K < GlowRim; ++K)
				{
					const double Angle = (K + 0.5 * Ring) * UE_DOUBLE_TWO_PI / GlowRim;
					GlowPoints.Add(Middle + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Spread * RingAt[Ring]);
				}
			}
			for (int32 K = 0; K < GlowRim; ++K)
			{
				const int32 A = First + 1 + K, B = First + 1 + (K + 1) % GlowRim;
				GlowTriangles.Append({ First, A, B, First, B, A });
				for (int32 Ring = 0; Ring + 1 < GlowRings; ++Ring)
				{
					const int32 A0 = A + Ring * GlowRim, B0 = B + Ring * GlowRim, A1 = A0 + GlowRim, B1 = B0 + GlowRim;
					GlowTriangles.Append({ A0, A1, B1, A0, B1, A1, A0, B1, B0, A0, B0, B1 });
				}
			}
			GlowRegion.Add(RegionId);
			GlowKind.Add(Kind);
		};
		FRandomStream Scatter(2718);
		for (const FCampaignRegion& Region : Data.Regions)
		{
			const bool bCity = Region.Terrain == TEXT("urban");
			const bool bBig = Region.bCapital || bCity || Region.VictoryPoints >= 2 || Region.Factories >= 2;
			const bool bTown = bBig || Region.VictoryPoints > 0 || Region.Factories > 0 || Region.bOil || Region.bPort;
			if (!bTown)
			{
				continue;
			}
			const double Grown = TownSpread(Region.Id);
			const double Spread = (Region.bCapital ? 5.2 : bCity ? 4.4 : bBig ? 3.4 : 2.5) * U * Grown; // (half what it first was, on the town as it is now built)
			AddGlow(SurfacePoint(Region.Center.X, Region.Center.Y, 5.4 * Grown), Spread, Region.Id, 0);
			// (the lit quarters are where the town's tall blocks really stand: its builder says where)
			if (const TArray<FVector2D>* Lit = TownQuarters.Find(Region.Id))
			{
				for (int32 Quarter = 1; Quarter < Lit->Num(); ++Quarter)
				{
					AddGlow(SurfacePoint((*Lit)[Quarter].X, (*Lit)[Quarter].Y, 3.6), Scatter.FRandRange(1.5f, 2.2f) * U, Region.Id, 1);
				}
			}
			// A port's lighthouse: a small bright light at its lantern, lit where the country is at peace.
			if (const FVector* Lantern = TownBeacons.Find(Region.Id))
			{
				AddGlow(*Lantern, 1.5 * U, Region.Id, 3);
			}
			// (the wash is as wide as it was before the towns were built larger: a wider one cost frames by night)
			AddGlow(SurfacePoint(Region.Center.X, Region.Center.Y, 1.4), FMath::Max(Spread / Grown * 3.2, 8.0 * U), Region.Id, 2);
		}
		if (GlowPoints.Num() > 0)
		{
			TArray<FVector> GlowNormals;
			GlowNormals.Init(FVector::UpVector, GlowPoints.Num());
			GlowColours.Init(FColor(0, 0, 0, 0), GlowPoints.Num());
			LightsMesh->CreateMeshSection(1, GlowPoints, GlowTriangles, GlowNormals, TArray<FVector2D>(), GlowColours, TArray<FProcMeshTangent>(), false);
			LightsMesh->SetMaterial(1, HighlightMaterial);
		}
	}
	UE_LOG(LogTheater, Log, TEXT("Windows to light: %d"), Panes);
}

void ACampaignMapActor::SetWarAtNight(const TArray<FString>& Blackout, const TArray<int32>& Fronts)
{
	THEATER_SPEED_SCOPE(SetWarAtNight);
	if (Fronts != FrontPairs || (Fronts.Num() > 0 && FrontFires.Num() == 0))
	{
		// Where the fires of each front burn: along the very frontier the two provinces share. They stand thick
		// where the two armies face each other and thin out to embers and to dark gaps along the rest of it; they
		// are of three sizes, most of them small; and no two stretches are spaced alike. Under them, evenly along
		// the same frontier, go the points of one faint line of glow.
		FrontPairs = Fronts;
		FrontFires.Reset();
		FrontGlow.Reset();
		struct FSpot
		{
			FVector2D At;
			float Size, Heat;
		};
		TArray<FSpot> Spots;
		TArray<FVector2D> Line;
		FRandomStream Random(1939);
		const double Step = 0.5;
		for (int32 K = 0; K + 1 < FrontPairs.Num(); K += 2)
		{
			if (!Data.Regions.IsValidIndex(FrontPairs[K]) || !Data.Regions.IsValidIndex(FrontPairs[K + 1]))
			{
				continue;
			}
			const FCampaignRegion& Here = Data.Regions[FrontPairs[K]];
			const FCampaignRegion& There = Data.Regions[FrontPairs[K + 1]];
			const FVector2D Facing = (Here.Center + There.Center) * 0.5; // where the two armies look at each other across the line
			double ToFire = Random.FRandRange(0.5f, 3.0f), ToGlow = 2.0;
			for (const TArray<FVector2D>& Ring : Here.Polygons)
			{
				double SignedArea = 0.0;
				for (int32 i = 0, n = Ring.Num(); i < n; ++i)
				{
					SignedArea += Ring[i].X * Ring[(i + 1) % n].Y - Ring[(i + 1) % n].X * Ring[i].Y;
				}
				const double Outward = SignedArea >= 0.0 ? 1.0 : -1.0;
				for (int32 i = 0, n = Ring.Num(); i < n; ++i)
				{
					const FVector2D A = Ring[i], B = Ring[(i + 1) % n];
					const double Length = FVector2D::Distance(A, B);
					if (Length < 0.05)
					{
						continue;
					}
					const FVector2D Dir = (B - A) / Length, Side = FVector2D(Dir.Y, -Dir.X) * Outward;
					const FVector2D Probe = (A + B) * 0.5 + Side * 0.35;
					bool bShared = There.Bounds.bIsValid && There.Bounds.IsInside(Probe);
					if (bShared)
					{
						bShared = false;
						for (const TArray<FVector2D>& Theirs : There.Polygons)
						{
							bShared = bShared || FCampaignMapData::PointInRing(Theirs, Probe.X, Probe.Y);
						}
					}
					if (!bShared)
					{
						continue;
					}
					for (double Along = 0.0; Along < Length; Along += Step)
					{
						const FVector2D P = A + Dir * Along;
						ToFire -= Step;
						ToGlow -= Step;
						if (ToGlow <= 0.0)
						{
							Line.Add(P);
							ToGlow = 5.0;
						}
						if (ToFire > 0.0)
						{
							continue;
						}
						const float Thick = FMath::Clamp(1.3f - static_cast<float>(FVector2D::Distance(P, Facing)) / 10.0f, 0.1f, 1.0f);
						const float Throw = Random.FRand();
						// (uneven: now two fires close together, now a long dark gap; closer where the armies face)
						ToFire = Random.FRandRange(2.2f, 9.5f) * (Thick > 0.6f ? 0.7 : 1.0);
						FSpot Spot;
						Spot.At = P + Side * Random.FRandRange(-1.3f, 1.3f) + Dir * Random.FRandRange(-0.6f, 0.6f);
						if (Throw < Thick)
						{
							const float Pick = Random.FRand();
							Spot.Size = Pick < 0.2f ? 1.5f : Pick < 0.5f ? 0.95f : 0.6f;
							Spot.Heat = 1.0f;
						}
						else if (Throw < Thick + 0.4f)
						{
							Spot.Size = Random.FRandRange(0.34f, 0.5f); // an ember
							Spot.Heat = 0.4f;
						}
						else
						{
							continue; // a dark gap
						}
						Spots.Add(Spot);
					}
				}
			}
		}
		// (no more than the smoke can carry: a very long front is drawn with every other fire)
		while (Spots.Num() > 190)
		{
			for (int32 K = Spots.Num() - 1; K >= 0; K -= 2)
			{
				Spots.RemoveAt(K, EAllowShrinking::No);
			}
		}
		while (Line.Num() > 130)
		{
			for (int32 K = Line.Num() - 1; K >= 0; K -= 2)
			{
				Line.RemoveAt(K, EAllowShrinking::No);
			}
		}
		for (const FSpot& Spot : Spots)
		{
			FFrontFire& Fire = FrontFires.AddDefaulted_GetRef();
			Fire.At = SurfacePoint(Spot.At.X, Spot.At.Y, 0.5);
			Fire.Size = Spot.Size;
			Fire.Heat = Spot.Heat;
		}
		for (const FVector2D& Point : Line)
		{
			FrontGlow.Add(SurfacePoint(Point.X, Point.Y, 0.6));
		}
	}
	if (Blackout != BlackoutTags)
	{
		UE_LOG(LogTheater, Log, TEXT("The blackout: %d nations at war (%s), %d stretches of front"), Blackout.Num(), *FString::Join(Blackout, TEXT(" ")), Fronts.Num() / 2);
		BlackoutTags = Blackout;
		ShownNight = -1.0f; // (the lights are worked out again)
		SetNight(NightNow, bLamplightNow);
	}
}

void ACampaignMapActor::ApplyPaperLight()
{
	if (!LandMaterial || !SeaMaterial)
	{
		return;
	}
	// By day the sheet is parchment and its sea a pale grey-green, a step darker than the palest nation. By night
	// it is dim paper: the land much the same for every holder (the wash thinner), and the sea darker than the
	// land beside it, with some of its colour gone. The lights and the fires are left to shine on it.
	const float Moon = bLamplightNow ? 0.0f : NightNow;
	const float Dry = FMath::Lerp(1.0f, PaperNight[0], Moon), Water = FMath::Lerp(1.0f, PaperNight[1], Moon), Thin = FMath::Lerp(1.0f, PaperNight[2], Moon);
	LandVector(TEXT("PaperColour"), FLinearColor(PaperLand[0], PaperLand[1], PaperLand[2]) * Dry);
	LandVector(TEXT("PaperInk"), FLinearColor(0.2f, 0.135f, 0.075f) * FMath::Lerp(1.0f, 0.6f, Moon));
	// (by night unseen ground is lifted and warmed against the moon's blue: it was the colour of the sea beside it, and the coast was lost)
	LandVector(TEXT("FogColour"), FMath::Lerp(FLinearColor(0.15f, 0.128f, 0.085f), FLinearColor(0.3f, 0.13f, 0.024f), Moon));
	LandScalar(TEXT("HatchSharp"), -13.0f);
	LandScalar(TEXT("PaperShade"), PaperLand[3]);
	LandScalar(TEXT("PaperSun"), PaperLand[4]);
	LandScalar(TEXT("PaperWash"), PaperLand[5] * Thin);
	LandScalar(TEXT("PaperBand"), PaperLand[6] * FMath::Lerp(1.0f, 0.5f, Moon)); // (a pale holder's band and wash must not glow on the night sheet)
	const FLinearColor Sea(PaperSeaTone[0], PaperSeaTone[1], PaperSeaTone[2]);
	const float Grey = Sea.GetLuminance();
	const FLinearColor NightSea = FMath::Lerp(Sea, FLinearColor(Grey, Grey, Grey), 0.25f * Moon) * Water;
	SeaVector(TEXT("PaperSea"), NightSea);
	SeaVector(TEXT("PaperDeep"), NightSea * 0.86f);
	SeaVector(TEXT("PaperSeaInk"), FLinearColor(0.1f, 0.15f, 0.17f) * FMath::Lerp(1.0f, 0.45f, Moon));
}

void ACampaignMapActor::BuildSheetMargin()
{
	if (!Data.IsLoaded() || !SceneryMaterial || !HighlightMaterial)
	{
		return;
	}
	if (!MarginMesh)
	{
		MarginMesh = UTheaterStillMesh::Make(this, TEXT("Margin"));
		MarginMesh->SetupAttachment(Root);
		MarginMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MarginMesh->RegisterComponent();
		MarginMesh->SetCastShadow(false);
		MarginMesh->SetVisibility(false);
	}
	MarginMesh->ClearAllMeshSections();
	// Round the sheet, on the table: a margin of the same paper, a border ruled on it in ink a little way in from
	// its edge, and outside it the soft shadow the sheet throws on the wood.
	const FVector2D Half = GetWorldHalfExtent(); // X: north-south, Y: east-west
	// (round a world the ruled border is kept about a point and a half of the screen wide at any height: see where the eye's height is told)
	const double U = UnitScale, Top = 2.4 * U + 0.5 * U, Margin = 46.0 * U, Rule = 14.0 * U, Line = Data.bWorld ? FMath::Max(1.1 * U, MarginLine) : 1.1 * U, Soft = 26.0 * U;
	const bool bRollers = Data.bWorld;
	const double NorthEdge = MapToWorld(0.0, Data.WorldMinY).X, SouthEdge = MapToWorld(0.0, Data.WorldMinY + Data.WorldHeight).X, HalfLong = Data.WorldWidth * U;
	auto Frame = [&Half, bRollers, NorthEdge, SouthEdge, HalfLong](FCampaignMeshBuffers& Into, double Inner, double Outer, double Z, const FColor& In, const FColor& Out)
	{
		if (bRollers)
		{
			// (Round a world the chart is on rollers: the margin, its ruled border and its shadow run along the
			// north edge and the south, and there is none to east or west.)
			// (In pieces along its length: what is seen through, as the shadow is, takes the haze of the air from its
			// corners, and one piece two worlds long took it from corners half a world away and went pale.)
			const int32 Pieces = 96;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const double A = Side == 0 ? NorthEdge + Inner : SouthEdge - Inner, B = Side == 0 ? NorthEdge + Outer : SouthEdge - Outer;
				for (int32 Piece = 0; Piece < Pieces; ++Piece)
				{
					const double Y0 = -HalfLong + 2.0 * HalfLong * Piece / Pieces, Y1 = -HalfLong + 2.0 * HalfLong * (Piece + 1) / Pieces;
					const int32 First = Into.Vertices.Num();
					Into.Vertices.Append({ FVector(A, Y0, Z), FVector(A, Y1, Z), FVector(B, Y1, Z), FVector(B, Y0, Z) });
					Into.Normals.Append({ FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector });
					Into.Colors.Append({ In, In, Out, Out });
					Into.Triangles.Append({ First, First + 1, First + 2, First, First + 2, First + 3, First, First + 2, First + 1, First, First + 3, First + 2 });
				}
			}
			return;
		}
		// (a frame between two rectangles: four quads, coloured at their inner and outer edges)
		const FVector2D A(Half.X + Inner, Half.Y + Inner), B(Half.X + Outer, Half.Y + Outer);
		const FVector Ins[4] = { FVector(A.X, -A.Y, Z), FVector(A.X, A.Y, Z), FVector(-A.X, A.Y, Z), FVector(-A.X, -A.Y, Z) };
		const FVector Outs[4] = { FVector(B.X, -B.Y, Z), FVector(B.X, B.Y, Z), FVector(-B.X, B.Y, Z), FVector(-B.X, -B.Y, Z) };
		for (int32 K = 0; K < 4; ++K)
		{
			const int32 N = (K + 1) % 4, First = Into.Vertices.Num();
			Into.Vertices.Append({ Ins[K], Ins[N], Outs[N], Outs[K] });
			Into.Normals.Append({ FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector });
			Into.Colors.Append({ In, In, Out, Out });
			Into.Triangles.Append({ First, First + 1, First + 2, First, First + 2, First + 3, First, First + 2, First + 1, First, First + 3, First + 2 });
		}
	};
	{
		FCampaignMeshBuffers Paper;
		const FColor Sheet(255, 255, 255), Ink(96, 76, 56);
		Frame(Paper, -0.2 * U, Margin, Top, Sheet, Sheet);
		Frame(Paper, Rule, Rule + Line, Top + 0.06 * U, Ink, Ink);
		Frame(Paper, Rule + 3.0 * Line, Rule + 3.5 * Line, Top + 0.06 * U, Ink, Ink);
		if (!MarginPaper)
		{
			MarginPaper = UMaterialInstanceDynamic::Create(SceneryMaterial->GetBaseMaterial(), this);
			MarginPaper->SetScalarParameterValue(TEXT("Grain"), 0.05f);
		}
		Paper.Commit(MarginMesh, 0, MarginPaper);
	}
	{
		FCampaignMeshBuffers Shade;
		Frame(Shade, Margin, Margin + Soft, Top - 0.3 * U, FColor(0, 0, 0, 120), FColor(0, 0, 0, 0));
		if (!MarginShade)
		{
			MarginShade = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
			MarginShade->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
		}
		Shade.Commit(MarginMesh, 1, MarginShade);
	}
	PaperShown = -1.0f; // (so that it is shown or hidden, and lit, at once)
}

void ACampaignMapActor::SetNight(float Night, bool bLamplight)
{
	THEATER_SPEED_SCOPE(SetNight);
	const bool bLampChanged = bLamplight != bLamplightNow;
	NightNow = Night;
	bLamplightNow = bLamplight;
	if (!bLampChanged && FMath::Abs(Night - ShownNight) < 0.02f)
	{
		return;
	}
	ShownNight = Night;
	if (RiverMesh && Land.Rivers.Num() > 0 && HazeSet.Num() > 0 && (NightNow > 0.5f) != bRiversNightFor)
	{
		// (a river under the haze is darker still by night; speed, tc.Speed.RiverInks: its inks changed in place)
		if (!(TheaterSpeed::RiverInks() && RecolourRivers()))
		{
			BuildWaterways();
		}
	}
	ApplyPaperLight();
	PaperShown = -1.0f; // (the margin's paper takes the hour's light too)
	if ((SelectedRegion != INDEX_NONE || HoveredRegion != INDEX_NONE) && HighlightMesh && FMath::Abs(FMath::Clamp(GetMoonlight(), 0.0f, 1.0f) - HighlightMoon) > 0.04f)
	{
		RebuildHighlight(); // (the picked province's edge and the pointed one's follow the hour)
	}
	// The land's own shading follows the light of the hour: a warm sun and cool shade by day (the lit tones a tenth
	// brighter than they were first mixed), and by moonlight both of them cool. By lamplight they stay as by day.
	if (LandMaterial)
	{
		// (The art director's ruling of round 26: the moon's grading comes in after the evening, not through it. The
		// evening has a sun and a shade of its own on the land: gold on the slopes the lamp lights, cool in the shade.)
		const float Moon = bLamplight ? 0.0f : ACampaignEnvironment::MoonOf(Night), Dusk = bLamplight ? 0.0f : ACampaignEnvironment::DuskOf(Night);
		LandVector(TEXT("SunColour"), FMath::Lerp(FMath::Lerp(FLinearColor(0.69f, 0.62f, 0.5f), ACampaignEnvironment_DuskSun(), Dusk), ACampaignEnvironment::MoonLook().Sun, Moon));
		LandScalar(TEXT("WetMoon"), Moon); // (the lesser painted waters sink toward their ground by moonlight: see the land's paint)
		LandVector(TEXT("ShadeColour"), FMath::Lerp(FMath::Lerp(FLinearColor(0.43f, 0.48f, 0.58f), ACampaignEnvironment_DuskShade(), Dusk), ACampaignEnvironment::MoonLook().Shade, Moon));
		if (PieceMaterial)
		{
			// (and the pieces: under a blue moon a blue base was the loudest thing on the table after the fires.
			// Their paint is greyed by night so that its saturation stays under about 0.6, and dimmed a little.)
			PieceMaterial->SetScalarParameterValue(TEXT("Grey"), 0.5f * Moon);
			PieceMaterial->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(0.86f, 0.84f, 0.78f), Moon));
			// (a ship's wake is paint on the water: by night it goes down with the sea, or it shone like a searchlight)
			PieceMaterial->SetVectorParameterValue(TEXT("WakeTone"), FMath::Lerp(FLinearColor::White, FLinearColor(0.85f, 0.64f, 0.66f), Moon));
			LightFigures(PieceMaterial, Moon); // (the figures' fill and rim are the day's)
		}
		if (WoodsMaterial)
		{
			// (and the woods are cooled and dimmed with it, or the yellows of autumn shine like lamps in the blackout)
			WoodsMaterial->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, ACampaignEnvironment::MoonLook().Trees, Moon));
		}
		// (the grey floor a wood keeps under the snow: the art director's ruling of round 19)
		LandVector(TEXT("SnowWoodFloor"), SnowWoodFloor);
		// (by moonlight the nations' colours sink into the dark, as colours do: the land is mostly the land)
		LandScalar(TEXT("Politics"), FMath::Lerp(0.94f, ACampaignEnvironment::MoonLook().Politics, Moon));
	}
	{
		// The water by moonlight: greyed (a blue moon on blue water is cobalt, and a blue nation's coast is lost
		// in it), the shelf seas taken down a little so that moonlit land is brighter than the water beside it, the
		// deeps lifted a touch, and a thin pale line of surf kept along the shore. Rivers go to steel. By lamplight
		// all of it stays as by day.
		const float Moon = bLamplight ? 0.0f : ACampaignEnvironment::MoonOf(Night);
		if (SeaMaterial)
		{
			SeaScalar(TEXT("Grey"), SeaNight.X * Moon);
			SeaVector(TEXT("Tint"), FMath::Lerp(FLinearColor::White, SeaNightTint, Moon));
			SeaVector(TEXT("Lift"), FMath::Lerp(FLinearColor::Black, SeaNightLift, Moon));
			SeaScalar(TEXT("Surf"), FMath::Lerp(0.45f, SeaNight.Y, Moon));
		}
		if (WaterMaterial)
		{
			WaterMaterial->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, RiverNightTint, Moon));
		}
		// The shipping lanes and trade routes are dimmed with the land by night: in the blackout they shone like chalk.
		if (LanesInk)
		{
			LanesInk->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(0.3f, 0.36f, 0.5f), Moon));
			LanesInk->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(1.0f, 0.6f, Moon));
		}
		// (The art director's ruling of round 17: by night the washes round an army in hand stayed as bright as by
		// day while the ground under them fell to 0.38, so that the lamp's pool came out darker than the floor round
		// it. By night only they take the chart veil's step; and the ring round the piece in hand, a hot yellow by
		// day, is a warm cream at about 0.6.)
		if (OrdersInk)
		{
			OrdersInk->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(WashNight.R, WashNight.G, WashNight.B), Moon));
		}
		if (GunEarthInk)
		{
			// (the trodden earth under the guns goes down with the ground it lies on: nothing of a gun line glows)
			GunEarthInk->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(0.16f, 0.2f, 0.3f), Moon));
		}
		if (WakeInk)
		{
			// (a wake dims with the sea: the ribbon about 0.25 at its whitest by night)
			// (and it goes to the moon's side: left grey, foam a tenth as bright as by day was a brown smear on blue water)
			const float Dim = static_cast<float>(WakeLook.Y);
			WakeInk->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(Dim * 0.83f, Dim, Dim * 1.47f), Moon));
		}
		if (SeaReachInk)
		{
			// (the ring's line by night: brass at about 0.45, the art director's number)
			SeaReachInk->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(SeaLineNight.R, SeaLineNight.G, SeaLineNight.B), Moon));
			SeaReachInk->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(1.0f, SeaLineNight.A, Moon));
		}
		if (SeaFillInk)
		{
			// (and the water inside it: about 0.25 on a sea at 0.17)
			SeaFillInk->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(SeaFillNight.R, SeaFillNight.G, SeaFillNight.B), Moon));
			SeaFillInk->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(1.0f, SeaFillNight.A, Moon));
		}
		if (WakeTrailInk)
		{
			// (release 2: the foam of enemy wakes, kept faint by night with the sea's own lines)
			WakeTrailInk->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(SeaLineNight.R, SeaLineNight.G, SeaLineNight.B), Moon));
			WakeTrailInk->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(1.0f, SeaLineNight.A, Moon));
		}
		if (SightingInk)
		{
			// (a place where the enemy was last seen: red crayon, kept findable by night at about 0.35)
			SightingInk->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(SightingNight.R, SightingNight.G, SightingNight.B), Moon));
			SightingInk->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(1.0f, SightingNight.A, Moon));
		}
		if (UMaterialInstanceDynamic* Ring = GetSolidMaterial(FLinearColor(FColor::FromHex(TEXT("FFD27A"))), 0.4f, 0.0f, 2.5f))
		{
			Ring->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(FLinearColor(FColor::FromHex(TEXT("FFD27A"))), FLinearColor(FColor::FromHex(TEXT("6E6554"))), Moon)); // (the lamp over the piece lights the ring too: this is what comes out a warm cream at about 0.6 under it)
			Ring->SetScalarParameterValue(TEXT("Glow"), FMath::Lerp(2.5f, RingNightGlow, Moon));
		}
		// Province lines by night are blue-grey and faint (their strength is capped as the eye moves: UpdateForCamera).
		if (ProvincePaleInk)
		{
			ProvincePaleInk->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(0.5f, 0.62f, 0.86f), Moon));
		}
		// And the flags do not shine as they do by day: their cloth carries less light of its own.
		for (const TPair<FString, TObjectPtr<UMaterialInstanceDynamic>>& Flag : FlagMaterials)
		{
			if (Flag.Value)
			{
				Flag.Value->SetScalarParameterValue(TEXT("Glow"), 0.5f * (1.0f - 0.4f * Moon));
			}
		}
	}
	if (!LightsMesh || LightColours.Num() == 0 || LightPoints.Num() != LightColours.Num())
	{
		return;
	}
	// (by day there is nothing of them to see, so nothing of them is drawn: thousands of unlit panes still cost)
	LightsMesh->SetVisibility(Night > 0.02f);
	bLightsTwinStale = true; // (seam-4: the lamps below are lit for a new hour; their twin over the seam follows when it is next shown)
	// The lamps are lit one by one as the light goes: each has its own moment, and some stay dark all night. A
	// country at war keeps the blackout: its towns and villages show almost nothing.
	TSet<int32> Black;
	for (const FCampaignRegion& Region : Data.Regions)
	{
		if (BlackoutTags.Contains(Region.Owner))
		{
			Black.Add(Region.Id);
		}
	}
	const int32 Panes = LightColours.Num() / 4;
	for (int32 Pane = 0; Pane < Panes; ++Pane)
	{
		const uint32 Chance = (static_cast<uint32>(Pane) * 2654435761u) >> 24; // 0 to 255, the same for a pane every time
		const float Moment = 0.15f + 0.6f * (Chance / 255.0f);
		const bool bBlackedOut = PaneRegion.IsValidIndex(Pane) && Black.Contains(PaneRegion[Pane]);
		// (behind the blackout one window in twelve shows a chink of light, and that dimly)
		const bool bDark = bBlackedOut ? Chance % 12 != 0 : Chance % 5 == 0;
		const float Lit = bDark ? 0.0f : FMath::Clamp((Night - Moment) / 0.12f, 0.0f, 1.0f) * (bBlackedOut ? 0.3f : 1.0f);
		// Lamplight through a curtain: warm, and a little different from house to house.
		// (the glow's material brightens what it is given, so the colour is kept below white to stay amber)
		const FColor Glow(static_cast<uint8>(205 + Chance % 30), static_cast<uint8>(118 + Chance % 34), static_cast<uint8>(34 + Chance % 26), static_cast<uint8>(Lit * 240.0f));
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			LightColours[Pane * 4 + Corner] = Glow;
		}
	}
	// (the points go with the colours: the mesh takes no change that does not come with as many points as it has)
	LightsMesh->UpdateMeshSection(0, LightPoints, TArray<FVector>(), TArray<FVector2D>(), LightColours, TArray<FProcMeshTangent>());

	// The lights of the countryside: none behind the blackout; elsewhere uneven, most of them faint, a few bright.
	const int32 PerVillage = 1 + VillageRim;
	if (VillageColours.Num() == VillageRegion.Num() * PerVillage && VillagePoints.Num() == VillageColours.Num() && VillageColours.Num() > 0)
	{
		for (int32 Light = 0; Light < VillageRegion.Num(); ++Light)
		{
			const uint32 Chance = (static_cast<uint32>(Light) * 2246822519u) >> 24;
			const float Moment = 0.15f + 0.6f * (Chance / 255.0f);
			const bool bDark = Black.Contains(VillageRegion[Light]) || Chance % 6 == 0 || HazeSet.Contains(VillageRegion[Light]); // (and no village's lamp shows through the haze)
			const float Strength = Chance % 6 == 1 ? 1.0f : 0.4f + 0.42f * ((Chance >> 3) % 8) / 7.0f;
			const float Lit = bDark ? 0.0f : FMath::Clamp((Night - Moment) / 0.12f, 0.0f, 1.0f) * Strength;
			const uint8 Red = static_cast<uint8>(215 + Chance % 30), Green = static_cast<uint8>(150 + Chance % 34), Blue = static_cast<uint8>(62 + Chance % 30);
			VillageColours[Light * PerVillage] = FColor(Red, Green, Blue, static_cast<uint8>(Lit * 245.0f));
			for (int32 K = 1; K < PerVillage; ++K)
			{
				VillageColours[Light * PerVillage + K] = FColor(Red, Green, Blue, 0);
			}
		}
		LightsMesh->UpdateMeshSection(2, VillagePoints, TArray<FVector>(), TArray<FVector2D>(), VillageColours, TArray<FProcMeshTangent>());
	}

	// The towns' glow, only where there is no blackout: lamp yellow, a small hot heart and a soft fall. The wash a
	// lit town throws on the country round it is very wide and very faint.
	const int32 PerGlow = 1 + GlowRings * GlowRim;
	if (GlowColours.Num() == GlowRegion.Num() * PerGlow && GlowPoints.Num() == GlowColours.Num() && GlowKind.Num() == GlowRegion.Num() && GlowColours.Num() > 0)
	{
		const float Dusk = FMath::SmoothStep(0.3f, 0.85f, Night);
		// (A glow that is dark is not drawn at all: its points are gathered into its middle. A see-through disc
		// with nothing in it still costs as much to draw as a lit one, and half Europe is blacked out.)
		if (GlowShown.Num() != GlowPoints.Num())
		{
			GlowShown = GlowPoints;
		}
		for (int32 Glow = 0; Glow < GlowRegion.Num(); ++Glow)
		{
			// (the fog of war as a haze: a town's lights under it burn at a quarter strength and throw no pool)
			const bool bHazed = HazeSet.Contains(GlowRegion[Glow]);
			const float Lit = Black.Contains(GlowRegion[Glow]) || (bHazed && GlowKind[Glow] == 2) ? 0.0f : bHazed ? 0.25f * Dusk : Dusk;
			for (int32 K = 0; K < PerGlow; ++K)
			{
				GlowShown[Glow * PerGlow + K] = Lit > 0.003f ? GlowPoints[Glow * PerGlow + K] : GlowPoints[Glow * PerGlow];
			}
			const uint8 Kind = GlowKind[Glow];
			const float Heart = Kind == 2 ? TownWash : Kind == 1 ? 0.85f : 1.0f;
			const float Fall[GlowRings] = { Kind == 2 ? 0.95f : Kind == 3 ? 0.9f : 0.78f, Kind == 2 ? 0.55f : Kind == 3 ? 0.3f : 0.24f, 0.0f };
			// (lamp yellow, and kept below white: the glow's material brightens what it is given. A lighthouse's is whiter.)
			const FColor Hot = Kind == 2 ? FColor(255, 176, 96) : Kind == 3 ? FColor(255, 244, 214) : FColor(250, 208, 142);
			const FColor Warm = Kind == 2 ? FColor(255, 168, 88) : Kind == 3 ? FColor(255, 232, 186) : FColor(246, 190, 112);
			const FColor Edge = Kind == 2 ? FColor(255, 160, 80) : Kind == 3 ? FColor(255, 214, 150) : FColor(240, 168, 92);
			GlowColours[Glow * PerGlow] = FColor(Hot.R, Hot.G, Hot.B, static_cast<uint8>(Heart * Lit * 255.0f));
			for (int32 Ring = 0; Ring < GlowRings; ++Ring)
			{
				const FColor& Tone = Ring == 0 ? Warm : Edge;
				const uint8 Alpha = static_cast<uint8>(Heart * Fall[Ring] * Lit * 255.0f);
				for (int32 K = 0; K < GlowRim; ++K)
				{
					GlowColours[Glow * PerGlow + 1 + Ring * GlowRim + K] = FColor(Tone.R, Tone.G, Tone.B, Alpha);
				}
			}
		}
		LightsMesh->UpdateMeshSection(1, GlowShown, TArray<FVector>(), TArray<FVector2D>(), GlowColours, TArray<FProcMeshTangent>());
	}
}

UTexture2D* ACampaignMapActor::GetMinimap()
{
	if (!Data.IsLoaded() || Data.Width <= 0.0 || Data.Height <= 0.0)
	{
		return nullptr;
	}
	if (MiniRegion.Num() == 0)
	{
		// Which province lies under each point, and how the ground there takes the light: worked out once, since
		// neither the provinces nor the hills move. (Twice the size it is drawn at, so that it is crisp on a fine screen.)
		// (Where the world is attached the small map is the whole of it, in the world's own shape: 340 by 187 on the
		// screen. Beyond the provinces the land is wherever the world's heights stand above the sea, and nobody's.)
		const bool bWhole = Data.bWorld && Data.WorldWidth > 0.0 && Data.WorldHeight > 0.0;
		const double FrameX = bWhole ? Data.WorldMinX : 0.0, FrameY = bWhole ? Data.WorldMinY : 0.0;
		const double FrameWide = bWhole ? Data.WorldWidth : Data.Width, FrameHigh = bWhole ? Data.WorldHeight : Data.Height;
		MiniHigh = bWhole ? 374 : 472;
		MiniWide = FMath::Max(8, FMath::RoundToInt32(MiniHigh * FrameWide / FrameHigh));
		MiniRegion.SetNumUninitialized(MiniWide * MiniHigh);
		MiniShade.SetNumUninitialized(MiniWide * MiniHigh);
		const double StepX = FrameWide / MiniWide, StepY = FrameHigh / MiniHigh;
		const float Steep = 520.0f * static_cast<float>(FMath::Max(StepY / (1712.0 / 472.0), 1.0)); // (a coarser picture reads its slopes over a longer step)
		for (int32 Y = 0; Y < MiniHigh; ++Y)
		{
			for (int32 X = 0; X < MiniWide; ++X)
			{
				const double MapX = FrameX + (X + 0.5) * StepX, MapY = FrameY + (Y + 0.5) * StepY;
				const float Metres = Terrain.MetresAt(MapX, MapY);
				int32 Here = Data.FindRegionAt(MapX, MapY);
				if (Here < 0 && bWhole && (MapX < 0.0 || MapY < 0.0 || MapX > Data.Width || MapY > Data.Height) && Metres > 0.0f)
				{
					Here = MiniUnclaimed;
				}
				MiniRegion[Y * MiniWide + X] = static_cast<int16>(Here);
				if (Here != -1)
				{
					// Lit from the north-west like the table: a slope that faces the light is paler, one that
					// faces away darker, and the high ground a little darker all over.
					const float Slope = FMath::Max(Metres, 0.0f) - FMath::Max(Terrain.MetresAt(MapX - StepX * 1.5, MapY - StepY * 1.5), 0.0f);
					const float Light = 1.0f + FMath::Clamp(Slope / Steep, -0.3f, 0.24f) - 0.1f * FMath::Clamp(Metres / 2200.0f, 0.0f, 1.0f);
					MiniShade[Y * MiniWide + X] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Light * 160.0f), 0, 255));
				}
				else
				{
					// The sea: how deep it is, from the shelf to the abyss.
					MiniShade[Y * MiniWide + X] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(-Metres / 2600.0f * 255.0f), 0, 255));
				}
			}
		}
		bMiniStale = true;
	}
	if (!bMiniStale && MiniTexture)
	{
		return MiniTexture;
	}
	bMiniStale = false;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(MiniWide * MiniHigh);
	const bool bRound = Data.bWorld; // (the whole world's picture has no east or west edge: its last column lies beside its first)
	auto At = [this, bRound](int32 X, int32 Y) -> int32
	{
		if (bRound)
		{
			X = ((X % MiniWide) + MiniWide) % MiniWide;
		}
		return (X < 0 || Y < 0 || X >= MiniWide || Y >= MiniHigh) ? -1 : MiniRegion[Y * MiniWide + X];
	};
	// Each province's colour, once: its holder's colour as a stain over the paper of the map (as on the table:
	// the ground shows through it), or the colour of the view that is asked for.
	const FLinearColor Ground(0.3f, 0.3f, 0.2f);
	TArray<FLinearColor> Tints;
	TArray<int32> Holders; // (a number for each holder, so that two provinces' holders are compared without their names)
	Tints.SetNumUninitialized(Data.Regions.Num());
	Holders.SetNumUninitialized(Data.Regions.Num());
	{
		TMap<FString, int32> Numbers;
		for (int32 Id = 0; Id < Data.Regions.Num(); ++Id)
		{
			const FString& Holder = Data.Regions[Id].Owner;
			Holders[Id] = Numbers.FindOrAdd(Holder, Numbers.Num());
			const FCampaignNation* Nation = Data.FindNation(Holder);
			FLinearColor Tint = FMath::Lerp(Ground, Nation ? FLinearColor(Nation->Color) : FLinearColor(0.35f, 0.35f, 0.33f), 0.74f);
			if (bSides && SideOf.IsValidIndex(Id))
			{
				// The friend-and-foe view: the player's own ground green, his allies' blue, his enemies' red, the rest grey.
				const FString& Side = SideOf[Id];
				Tint = Side == TEXT("own") ? FLinearColor(0.2f, 0.62f, 0.3f) : Side == TEXT("ally") ? FLinearColor(0.16f, 0.4f, 0.85f)
					: Side == TEXT("enemy") ? FLinearColor(0.8f, 0.1f, 0.08f) : FLinearColor(0.3f, 0.3f, 0.29f);
			}
			else if (bWealth && Towns.IsValidIndex(Id))
			{
				// The wealth view: grey ground, gold by what the province pays.
				static const float Gold[6] = { 0.0f, 0.22f, 0.4f, 0.6f, 0.8f, 1.0f };
				const float Rich = Gold[FMath::Clamp(Towns[Id].Rank, 0, 5)];
				Tint = FMath::Lerp(FLinearColor(0.16f, 0.16f, 0.15f), FLinearColor(1.0f, 0.74f, 0.1f), Rich);
			}
			if (UnseenSet.Contains(Id))
			{
				Tint *= 0.55f; // under the fog of war
			}
			Tints[Id] = Tint;
		}
	}
	const FLinearColor Shelf(0.016f, 0.15f, 0.2f), Deep(0.004f, 0.035f, 0.1f), Surf(0.0f, 0.02f, 0.035f);
	// (land that is nobody's yet, beyond the provinces: bare paper in the plain view, grey in the others)
	const FLinearColor Nobodys = bSides ? FLinearColor(0.3f, 0.3f, 0.29f) : bWealth ? FLinearColor(0.16f, 0.16f, 0.15f) : FLinearColor(0.36f, 0.34f, 0.26f);
	for (int32 Y = 0; Y < MiniHigh; ++Y)
	{
		for (int32 X = 0; X < MiniWide; ++X)
		{
			const int32 Here = At(X, Y);
			FColor& Out = Pixels[Y * MiniWide + X];
			const float Shade = MiniShade[Y * MiniWide + X];
			if (Here == -1)
			{
				// The sea, paler on the shelf and dark in the deeps, with a dark line along the shore so that the
				// land stands out of it.
				const bool bCoast = At(X + 1, Y) != -1 || At(X - 1, Y) != -1 || At(X, Y + 1) != -1 || At(X, Y - 1) != -1;
				FLinearColor Water = bCoast ? Surf : FMath::Lerp(Shelf, Deep, FMath::Clamp(Shade / 255.0f, 0.0f, 1.0f));
				Water.A = 1.0f;
				Out = Water.ToFColor(true);
				continue;
			}
			if (Here < 0)
			{
				FLinearColor Bare = Nobodys * (Shade / 160.0f);
				Bare.A = 1.0f;
				Out = Bare.ToFColor(true);
				continue;
			}
			FLinearColor Tint = Tints[Here] * (Shade / 160.0f);
			// A frontier between two countries is drawn dark; a province line inside one is not drawn at all.
			const int32 East = At(X + 1, Y), South = At(X, Y + 1);
			if ((East >= 0 && Holders[East] != Holders[Here]) || (South >= 0 && Holders[South] != Holders[Here]))
			{
				Tint *= 0.4f;
			}
			Tint.A = 1.0f;
			Out = Tint.ToFColor(true);
		}
	}
	if (!MiniTexture)
	{
		MiniTexture = CampaignTextures::Create(MiniWide, MiniHigh, Pixels, true, false, TEXT("CampaignMinimap"));
		if (MiniTexture)
		{
			MiniTexture->Filter = TF_Bilinear;
		}
	}
	else
	{
		CampaignTextures::Update(MiniTexture, Pixels);
	}
	return MiniTexture;
}

FVector2D ACampaignMapActor::SortieStart(const FSortie& Sortie) const
{
	if (!Sortie.bFromSea)
	{
		return AirfieldSpot(Sortie.From);
	}
	// From the ships as they lie on the table, if the fleet is on it; otherwise from where the report puts them.
	if (const TObjectPtr<AArmyPiece>* Piece = FleetPieces.Find(Sortie.FromFleet))
	{
		if (*Piece)
		{
			return FleetPlace(Sortie.FromFleet);
		}
	}
	return Sortie.FromPoint;
}

FVector2D ACampaignMapActor::FleetPlace(int32 FleetId) const
{
	const TObjectPtr<AArmyPiece>* Piece = FleetPieces.Find(FleetId);
	return Piece && *Piece ? WorldToMap((*Piece)->GetRestLocation()) : FVector2D(Data.Width * 0.5, Data.Height * 0.5);
}

FVector2D ACampaignMapActor::FleetSpot(int32 Index) const
{
	int32 Count = 0;
	for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : FleetPieces)
	{
		if (Pair.Value && Count++ == Index)
		{
			return WorldToMap(Pair.Value->GetActorLocation());
		}
	}
	return FVector2D(Data.Width * 0.5, Data.Height * 0.5);
}

bool ACampaignMapActor::TrainSpot(int32 MinFlak, FVector2D& Out) const
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	// (the train this look already watches, wherever it has got to: its middle is some three and a half units behind its engine)
	for (const FTrain& Train : Trains)
	{
		if (Train.Mesh && WatchedTrain.IsValid() && Train.Mesh.Get() == WatchedTrain.Get())
		{
			Out = WorldToMap(Train.Mesh->GetComponentLocation() - Train.Mesh->GetForwardVector() * (3.5 * UnitScale));
			return true;
		}
	}
	for (const FTrain& Train : Trains)
	{
		const double Share = (Now - Train.Began) / 11.0;
		if (Train.Mesh && Train.FlakLevel >= MinFlak && Share > 0.08 && Share < 0.4)
		{
			WatchedTrain = Train.Mesh.Get();
			Out = WorldToMap(Train.Mesh->GetComponentLocation() - Train.Mesh->GetForwardVector() * (3.5 * UnitScale));
			return true;
		}
	}
	return false;
}

FVector2D ACampaignMapActor::ParadeSpot(int32 Index) const
{
	if (ParadeSpots.IsValidIndex(Index))
	{
		return ParadeSpots[Index];
	}
	FVector2D Middle = FVector2D::ZeroVector;
	for (const FVector2D& Spot : ParadeSpots)
	{
		Middle += Spot / ParadeSpots.Num();
	}
	return Middle;
}

void ACampaignMapActor::BuildParade()
{
	FString Where;
	if (!FParse::Value(FCommandLine::Get(), TEXT("TheaterParade="), Where, false))
	{
		return;
	}
	const int32* Id = Data.RegionByName.Find(Where);
	if (!Id)
	{
		UE_LOG(LogTheater, Warning, TEXT("Parade: no region called %s"), *Where);
		return;
	}
	// (The parade is a close view: its models wear a country's own layered mark where the map keeps the simpler one.)
	::UnitTable::SetCloseMarks(true);
	ON_SCOPE_EXIT { ::UnitTable::SetCloseMarks(false); };
	// ("-TheaterSeason=48" shows the parade's models as they are marked in that month of the war: turn 48 is August 1943.)
	{
		int32 Held = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeason="), Held))
		{
			::UnitTable::SetTurn(Held);
		}
	}
	// The models stand in line ahead on a plain board, all to one scale, in front of a wall ruled at every
	// half metre, with a man of 1.75 m by each: so each can be looked at closely, measured, and compared with
	// the next. "-TheaterModels=PanzerIV,T34" chooses which; all of them otherwise.
	TArray<FString> Names = FMiniatureMesh::VehicleNames();
	FString Wanted;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterModels="), Wanted, false))
	{
		Wanted.ParseIntoArray(Names, TEXT(","));
	}
	const FVector2D Centre = Data.Regions[*Id].Center;
	// "-TheaterModels=aircraft" is every aircraft; "ships:GER" is each class of warship as Germany's.
	if (Wanted == TEXT("aircraft"))
	{
		Names = FMiniatureMesh::AircraftNames();
	}
	FString Navy;
	if (Wanted.Split(TEXT(":"), nullptr, &Navy) && Wanted.StartsWith(TEXT("ships:")))
	{
		Names = FMiniatureMesh::ShipNames(Navy);
	}
	// "-TheaterParadeTurn=180" turns every aircraft about, to show its other side (the wall hides it otherwise).
	double ParadeTurn = 0.0;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterParadeTurn="), ParadeTurn);
	// "-TheaterParadeScale=0.1" draws everything a tenth the size, for ships.
	double Shrink = 1.0;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterParadeScale="), Shrink);
	// Each model has the room it needs in the row.
	TArray<double> Along;
	double Row = 0.0;
	for (const FString& Name : Names)
	{
		const double Room = Name.StartsWith(TEXT("Soldier:")) ? 6.0 : FMiniatureMesh::VehicleRoom(Name);
		Along.Add(Row + Room * 0.5);
		Row += Room;
	}
	const double Metre = 22.0 * Shrink, Apart = Row / FMath::Max(Names.Num(), 1) * Metre;
	const FVector Middle = SurfacePoint(Centre.X, Centre.Y, 0.0) + FVector(0.0, 0.0, 80.0);
	ParadeLevel = Middle.Z - GetActorLocation().Z + 1.3 * Metre;
	const double Long = Apart * Names.Num() + 30.0 * Metre;
	FMiniatureMesh Model;
	// (Ships, drawn a tenth the size or less, stand on a dark board against a dark wall: pale hulls are lost on the
	// pale one. Their wall stands beyond the beam and is high enough to fill the view.)
	const bool bShips = Shrink < 0.5;
	const FLinearColor Board(FColor::FromHex(bShips ? TEXT("1F2B36") : TEXT("B9AD8F"))), Wall(FColor::FromHex(bShips ? TEXT("27323E") : TEXT("D9D3C2"))), Ink(FColor::FromHex(TEXT("3A3A3A"))), Red(FColor::FromHex(TEXT("A02020")));
	Model.Box(Middle - FVector(0.0, 400.0 * Metre, 4.0), FVector(Long * 3.0, 860.0 * Metre, 8.0), Board);
	// The wall behind (toward +Y, so a view turned a quarter-circle looks square at it), ruled in half metres, the whole metres heavier.
	const double WallAt = (bShips ? 24.0 : 3.2) * Metre, WallHigh = (bShips ? 170.0 : 20.0) * Metre;
	Model.Box(Middle + FVector(0.0, WallAt + 2.0, 0.5 * WallHigh), FVector(Long * 3.0, 4.0, WallHigh), Wall);
	const FColor Lettering = bShips ? FColor(232, 224, 200) : FColor(40, 40, 40);
	auto Write = [this, &Middle, WallAt, Metre, Lettering](const FString& Text, double Along, double Up, double Size)
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(Root);
		Label->SetText(FText::FromString(Text));
		Label->SetWorldSize(Size * Metre);
		Label->SetTextRenderColor(Lettering);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetCastShadow(false);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Upright on the wall, facing out from it.
		Label->SetRelativeRotation(FMatrix(FVector(0.0, -1.0, 0.0), FVector(1.0, 0.0, 0.0), FVector(0.0, 0.0, 1.0), FVector::ZeroVector).Rotator());
		Label->SetWorldLocation(Middle + FVector(Along, WallAt - 1.0, Up * Metre));
		Label->RegisterComponent();
	};
	for (int32 Half = 1; Half <= 7 && !bShips; ++Half)
	{
		const bool bWhole = Half % 2 == 0;
		Model.Box(Middle + FVector(0.0, WallAt - 0.3, Half * 0.5 * Metre), FVector(Long, 0.6, (bWhole ? 0.05 : 0.02) * Metre), bWhole ? Ink : FLinearColor(0.35f, 0.35f, 0.35f));
	}
	// (the ships' wall is ruled at every ten metres above the water)
	for (int32 Ten = 1; Ten <= 4 && bShips; ++Ten)
	{
		Model.Box(Middle + FVector(0.0, WallAt - 0.3, Ten * 10.0 * Metre), FVector(Long, 0.6, 0.35 * Metre), FLinearColor(FColor::FromHex(TEXT("3D4B5A"))));
	}
	for (int32 K = 0; K < Names.Num(); ++K)
	{
		const FVector Spot = Middle + FVector(-(Along[K] - Row * 0.5) * Metre, 0.0, 0.0);
		// "Soldier:GER" is a nation's infantryman, facing out from the wall; anything else is a vehicle.
		FString Kind, Whose;
		if (Names[K].Split(TEXT(":"), &Kind, &Whose) && Kind == TEXT("Soldier"))
		{
			Model.Soldier(Whose, Spot, Metre, 90.0);
			Model.Pioneer(Whose, Spot + FVector(-1.3 * Metre, 0.0, 0.0), Metre, 35.0);
			Model.Rider(Whose, Spot + FVector(2.2 * Metre, 0.0, 0.0), Metre, 40.0);
			static const TMap<FString, FString> Peoples = { { TEXT("GER"), TEXT("German") }, { TEXT("SOV"), TEXT("Soviet") }, { TEXT("USA"), TEXT("American") },
				{ TEXT("UK"), TEXT("British") }, { TEXT("FRA"), TEXT("French") }, { TEXT("ITA"), TEXT("Italian") }, { TEXT("JAP"), TEXT("Japanese") }, { TEXT("POL"), TEXT("Polish") } };
			const FString* People = Peoples.Find(Whose);
			Write(People ? *People : Whose, Spot.X - Middle.X, 2.25, 0.3);
			ParadeSpots.Add(WorldToMap(Spot));
			continue;
		}
		const double Aloft = FMiniatureMesh::VehicleAloft(Names[K]);
		if (Aloft > 0.0)
		{
			// An aircraft flies over the board on a stand, well out from the wall, with a man under its nose for scale.
			const double Out = FMiniatureMesh::VehicleRoom(Names[K]) * 0.5 - 1.0;
			const FVector Flying = Spot + FVector(0.0, -Out * Metre, Aloft * Metre);
			Model.Vehicle(Names[K], Flying, Metre, ParadeTurn);
			Model.Rod(Spot + FVector(0.0, -Out * Metre, 0.0), Flying, 0.08 * Metre, Ink, 6);
			Model.Soldier(FMiniatureMesh::VehicleNation(Names[K]), Spot + FVector(-FMiniatureMesh::VehicleRoom(Names[K]) * 0.32 * Metre, -Out * Metre, 0.0), Metre, 60.0);
			Write(FMiniatureMesh::VehicleTitle(Names[K]), Spot.X - Middle.X, 9.5, 0.9);
			ParadeSpots.Add(WorldToMap(Flying));
			continue;
		}
		// ("-TheaterWash" shows the parade's vehicles whitewashed for the snow, where their country does so.)
		static const bool bParadeWash = FParse::Param(FCommandLine::Get(), TEXT("TheaterWash"));
		::UnitTable::SetWash(bParadeWash && ::UnitTable::WinterWash(FMiniatureMesh::VehicleNation(Names[K])));
		const bool bStood = Model.Vehicle(Names[K], Spot, Metre, 0.0);
		::UnitTable::SetWash(false);
		if (!bStood)
		{
			UE_LOG(LogTheater, Warning, TEXT("Parade: no model called %s"), *Names[K]);
		}
		if (bShips)
		{
			// A hull's facts over it: what it is, how long, and how it is armed (so two hulls can be told apart by the
			// lettering as well as by the eye).
			const bool bBritish = Whose == TEXT("UK");
			const FString Facts = Kind == TEXT("Battleship")
				? (bBritish ? TEXT("British battleship, 227 m: ten guns in three turrets (4, 2, 4), two funnels") : TEXT("The plain battleship, 215 m: six guns in three turrets of two, two funnels close together"))
				: Kind == TEXT("Cruiser")
				? (bBritish ? TEXT("British cruiser, 192 m: eight guns in four turrets of two, three tall funnels") : TEXT("The plain cruiser, 175 m: six guns in three turrets of two, two funnels"))
				: FMiniatureMesh::VehicleTitle(Names[K]);
			Write(Facts, Spot.X - Middle.X, 56.0, 5.4);
			for (int32 Ten = 1; Ten <= 4; ++Ten)
			{
				Write(FString::Printf(TEXT("%d m"), Ten * 10), Spot.X - Middle.X + 128.0 * Metre, Ten * 10.0 + 2.6, 3.2);
			}
			ParadeSpots.Add(WorldToMap(Spot));
			continue;
		}
		Write(FMiniatureMesh::VehicleTitle(Names[K]), Spot.X - Middle.X, 3.6, 0.42);
		for (int32 Whole = 1; Whole <= 3; ++Whole)
		{
			Write(FString::Printf(TEXT("%d m"), Whole), Spot.X - Middle.X + 4.1 * Metre, Whole + 0.2, 0.3);
		}
		// The man, a little ahead of the nose and in line with the middle of the vehicle; and a post beside him
		// marked red at 1.75 m, the height of his head.
		Model.Soldier(FMiniatureMesh::VehicleNation(Names[K]), Spot + FVector(-4.0 * Metre, 0.0, 0.0), Metre, 0.0);
		Write(TEXT("soldier 1.75 m"), Spot.X - Middle.X - 4.0 * Metre, 2.2, 0.22);
		ParadeSpots.Add(WorldToMap(Spot));
	}
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->RegisterComponent();
	Mesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
	Mesh->SetMaterial(0, SceneryMaterial);
}

void ACampaignMapActor::BuildTrees(const TSet<FIntPoint>* Only)
{
	// (With `Only`, just those squares of country are planted again and the rest stand as they are: a gun line that
	// comes or goes clears or gives back the trees under it.)
	if (!Only)
	{
		for (UProceduralMeshComponent* Mesh : TreeMeshes)
		{
			if (Mesh)
			{
				Mesh->DestroyComponent();
			}
		}
		TreeMeshes.Reset();
		TreeCentres.Reset();
		TreesShown.Reset();
		TreeSquareOf.Reset();
		TreesDue.Reset(); // (all planted afresh, in the look of the month)
	}

	// Trees stand wherever the paint says the ground is wooded: one to every couple of map units, each a little
	// off its place, firs in the north and on high ground and broad-leaved trees elsewhere. They are kept in
	// squares of country, and only the squares near the eye are drawn (UpdateForCamera): from far off the woods
	// are the dark cloth of the paint, and close up they have trees standing in them.
	const double Spacing = 1.75, Square = 130.0; // (trees stand close inside a wood: a mass, with the trees as its texture)
	TreeSquare = Square * UnitScale;
	const int32 SquaresX = FMath::CeilToInt32(Data.Width / Square), SquaresY = FMath::CeilToInt32(Data.Height / Square);
	const int32 PerSide = FMath::CeilToInt32(Square / Spacing);
	const double U = UnitScale;
	int32 Trees = 0;
	// Where pieces stand in each region (its first three armies, and its airfield either side of the town): kept clear.
	// (Release 2, map run 8: tc.Speed.TreesSpread. Laid once at a whole planting and kept: a square planted again on its
	// own, a few a frame, would otherwise find every province's spots afresh each time, the dearest part of a planting.)
	if (!Only || TreeClearingsKept.Num() != Data.Regions.Num())
	{
		TreeClearingsKept.Reset();
		TreeClearingsKept.SetNum(Data.Regions.Num());
		for (const FCampaignRegion& Region : Data.Regions)
		{
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				TreeClearingsKept[Region.Id].Add(WorldToMap(ArmySpot(Region.Id, Slot, Slot + 1)));
			}
			TreeClearingsKept[Region.Id].Add(Region.Center + FVector2D(-0.92, 0.4) * (TownRadius(Region.Id) + 5.0));
			TreeClearingsKept[Region.Id].Add(Region.Center + FVector2D(0.92, 0.4) * (TownRadius(Region.Id) + 5.0));
		}
	}
	const TArray<TArray<FVector2D>>& Clearings = TreeClearingsKept;
	for (int32 SY = 0; SY < SquaresY; ++SY)
	{
		for (int32 SX = 0; SX < SquaresX; ++SX)
		{
			if (Only && !Only->Contains(FIntPoint(SX, SY)))
			{
				continue;
			}
			FMiniatureMesh Model;
			FRandomStream Random(SY * 977 + SX * 131 + 5);
			for (int32 J = 0; J < PerSide; ++J)
			{
				for (int32 I = 0; I < PerSide; ++I)
				{
					const double X = SX * Square + (I + Random.FRand()) * Spacing;
					const double Y = SY * Square + (J + Random.FRand()) * Spacing;
					const float Chance = Random.FRand(), Size = Random.FRandRange(0.75f, 1.25f), Shade = Random.FRandRange(0.8f, 1.15f);
					if (X >= Data.Width || Y >= Data.Height)
					{
						continue;
					}
					const int32 GI = FMath::Clamp(FMath::RoundToInt32(X / Terrain.Cell), 0, Terrain.NX - 1);
					const int32 GJ = FMath::Clamp(FMath::RoundToInt32(Y / Terrain.Cell), 0, Terrain.NY - 1);
					const float Wooded = ChurnWoods(Terrain.Index(GI, GJ), Paint.Ground[Terrain.Index(GI, GJ)].A / 255.0f); // (release 2: thinned in the core of churned ground; as it was while that is off)
					if (Wooded < 0.35f || Chance > Wooded * 0.92f)
					{
						continue;
					}
					// Not on top of a town.
					const int32 RegionId = Terrain.Region[Terrain.Index(GI, GJ)];
					if (RegionId >= 0 && FVector2D::Distance(Data.Regions[RegionId].Center, FVector2D(X, Y)) < TownRadius(RegionId) + 2.1)
					{
						continue;
					}
					// Nor where the armies of the region stand: a clearing for each of the first three, and one for its airfield.
					if (RegionId >= 0 && Clearings[RegionId].ContainsByPredicate([X, Y](const FVector2D& Spot) { return FVector2D::DistSquared(Spot, FVector2D(X, Y)) < 7.8 * 7.8; }))
					{
						continue;
					}
					// Nor on the trodden earth of a gun line (the art director's round 38: the patch clears its trees).
					if (GunClearings.ContainsByPredicate([X, Y](const FVector2D& Spot) { return FVector2D::DistSquared(Spot, FVector2D(X, Y)) < 4.6 * 4.6; }))
					{
						continue;
					}
					const float Height = Terrain.Height[Terrain.Index(GI, GJ)];
					const FVector Foot = SurfacePoint(X, Y, 0.0);
					// Firs toward the north (small y) and on high ground.
					const bool bFir = Random.FRand() < FMath::Clamp(0.15 + (700.0 - Y) / 600.0 + Height * 0.35, 0.05, 0.92);
					// (a number of the tree's own, the same every month, so that a tree keeps its place in the turning)
					const float Own = static_cast<float>(FMath::Frac(FMath::Sin(X * 12.9898 + Y * 78.233) * 43758.5453));
					const float Lying = SnowLying(FVector2D(X, Y), WinterHard);
					if (bFir)
					{
						// Firs keep their green the year round, and carry the snow where it lies.
						const FLinearColor Green(0.035f * Shade, 0.085f * Shade, 0.045f * Shade);
						// (The art director's ruling of round 19: under snow the trees were pale grey dots, nearly the ground's own
						// colour. They are a cool grey-green at about 0.6 now, a texture on the wood's grey floor.)
						Model.Fir(Foot, 1.9 * Size * U, 0.55 * Size * U, FMath::Lerp(Green, FLinearColor(SnowOnTrees.R, SnowOnTrees.G, SnowOnTrees.B), 0.62f * Lying));
					}
					else
					{
						const FLinearColor Summer(0.055f * Shade, 0.115f * Shade, 0.038f * Shade), Fresh(0.085f * Shade, 0.17f * Shade, 0.045f * Shade);
						// (autumn is ochre and rust, not lemon: the art director's (196, 150, 60), a little under it since the lamp is strong)
						const FLinearColor Gold(0.15f * Shade, 0.105f * Shade, 0.032f), Orange(0.15f * Shade, 0.08f * Shade, 0.03f), Russet(0.11f * Shade, 0.055f * Shade, 0.028f);
						const FLinearColor Twigs = FMath::Lerp(FLinearColor(0.115f * Shade, 0.095f * Shade, 0.08f * Shade), FLinearColor(SnowOnTrees.R, SnowOnTrees.G * 0.93f, SnowOnTrees.B * 0.95f), 0.5f * Lying);
						FLinearColor Leaf = Summer;
						bool bBare = false;
						switch (TreeLook)
						{
						// (no more than a third of the canopy is ever turned: a wood must still read as its owner's)
						case 1: Leaf = Own < 0.16f ? Gold : Summer; break; // September: the first of them turn
						case 2: Leaf = Own < 0.12f ? Gold : Own < 0.23f ? Orange : Own < 0.33f ? Russet : Summer; break; // October
						case 3: bBare = Own < 0.3f; Leaf = Own < 0.45f ? Russet : Own < 0.55f ? Orange : Summer; break; // November: the first of them bare
						case 4: bBare = true; break; // winter
						case 5: bBare = Own < 0.5f; Leaf = Fresh; break; // March: in bud
						case 6: Leaf = Fresh; break; // April
						default: break;
						}
						if (bBare)
						{
							Model.Broadleaf(Foot, 1.4 * Size * U, 0.46 * Size * U, Twigs);
						}
						else
						{
							Model.Broadleaf(Foot, 1.5 * Size * U, 0.7 * Size * U, Leaf);
						}
					}
					++Trees;
				}
			}
			// (a square planted again takes the place of the one that stood there, shown or put away as it was)
			const int32 Old = Only ? TreeSquareOf.IndexOfByKey(FIntPoint(SX, SY)) : INDEX_NONE;
			if (Old != INDEX_NONE && TreeMeshes.IsValidIndex(Old) && TreeMeshes[Old])
			{
				TreeMeshes[Old]->DestroyComponent();
				TreeMeshes[Old] = nullptr;
			}
			if (Model.Vertices.Num() == 0)
			{
				continue;
			}
			UProceduralMeshComponent* Mesh = UTheaterStillMesh::Make(this, TEXT("TreeSquare"));
			Mesh->SetupAttachment(Root);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false); // thousands of small shadows cost more than they show
			Mesh->RegisterComponent();
			Mesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
			Mesh->SetMaterial(0, WoodsMaterial ? WoodsMaterial : SceneryMaterial);
			if (Old != INDEX_NONE && TreeMeshes.IsValidIndex(Old))
			{
				Mesh->SetVisibility(TreesShown.IsValidIndex(Old) && TreesShown[Old]);
				TreeMeshes[Old] = Mesh;
				continue;
			}
			Mesh->SetVisibility(false);
			TreeMeshes.Add(Mesh);
			TreeCentres.Add(MapToWorld((SX + 0.5) * Square, (SY + 0.5) * Square));
			TreesShown.Add(false);
			TreeSquareOf.Add(FIntPoint(SX, SY));
		}
	}
	if (!Only)
	{
		UE_LOG(LogTheater, Log, TEXT("Trees: %d, in %d squares of country"), Trees, TreeMeshes.Num());
	}
}

UTextRenderComponent* ACampaignMapActor::AddLabel(const FString& Text, const FVector2D& MapPos, float SizeUnits, const FColor& Color, int32 Tier, float Spacing, int32 OfRegion)
{
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	Label->SetupAttachment(Root);
	Label->SetText(FText::FromString(Text));
	Label->SetWorldSize(SizeUnits * UnitScale);
	Label->SetTextRenderColor(Color);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetHorizSpacingAdjust(Spacing * 26.0f); // the engine counts spacing in the font's own units, about 26 to a letter's height
	Label->SetCastShadow(false);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Lettering lies flat on the table, reading from the south: the face of the text looks up, its top points north.
	const FMatrix Flat(FVector(0.0, 0.0, 1.0), FVector(0.0, -1.0, 0.0), FVector(1.0, 0.0, 0.0), FVector::ZeroVector);
	Label->SetRelativeRotation(Flat.Rotator());

	// Clear the highest ground anywhere under the word.
	const double HalfWidth = (0.3 + Spacing * 0.5) * SizeUnits * Text.Len();
	double Ground = 0.0;
	for (int32 k = -4; k <= 4; ++k)
	{
		for (int32 m = -1; m <= 1; ++m)
		{
			Ground = FMath::Max(Ground, static_cast<double>(GroundAt(MapPos.X + HalfWidth * k / 4.0, MapPos.Y + SizeUnits * 0.5 * m)));
		}
	}
	Label->SetRelativeLocation(MapToWorld(MapPos.X, MapPos.Y, Ground + 0.3));
	Label->RegisterComponent();

	Labels.Add(Label);
	LabelTiers.Add(Tier);
	LabelRegions.Add(OfRegion);
	return Label;
}

FColor ACampaignMapActor::RegionInk(int32 RegionId) const
{
	const FCampaignNation* Nation = Data.Regions.IsValidIndex(RegionId) ? Data.FindNation(Data.Regions[RegionId].Owner) : nullptr;
	// (the stained ground is darker than the old flat paint everywhere: a region's name is written pale on all of it)
	return FromHex(TEXT("D9D2BC")).ToFColor(true);
}

void ACampaignMapActor::BuildLabels()
{
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	Labels.Reset();
	LabelTiers.Reset();
	LabelRegions.Reset();
	PlateSpots.Reset();
	LabelState = -1;

	const FColor Ink = FromHex(TEXT("17130F")).ToFColor(true);

	// Towns and regions.
	for (const FCampaignRegion& Region : Data.Regions)
	{
		const bool bTown = Region.bCapital || Region.Terrain == TEXT("urban") || Region.VictoryPoints >= 2;
		// A place worth having: it scores toward victory, or has factories, oil or a harbour.
		const bool bWorth = Region.VictoryPoints > 0 || Region.Factories > 0 || Region.bOil || Region.bPort;
		if (bTown || bWorth)
		{
			// Its name is on a plate that hangs before the town and faces whoever looks at the table (the screen
			// draws it), and under the name, close up, what the place is worth.
			TArray<FString> Worth;
			if (Towns.IsValidIndex(Region.Id) && Towns[Region.Id].Region == Region.Id)
			{
				// From the rules: the chief town where it is not the province's own name, and what the place pays a month.
				const FTownInfo& Town = Towns[Region.Id];
				if (!Town.Name.IsEmpty() && Town.Name != Region.Name)
				{
					Worth.Add(Town.Name);
				}
				if (Town.Worth > 0)
				{
					Worth.Add(FString::Printf(TEXT("pays %d"), Town.Worth));
				}
			}
			else if (Region.VictoryPoints > 0)
			{
				Worth.Add(FString::Printf(TEXT("worth %d"), Region.VictoryPoints));
			}
			if (Region.Factories > 0)
			{
				Worth.Add(FString::Printf(TEXT("%d %s"), Region.Factories, Region.Factories == 1 ? TEXT("factory") : TEXT("factories")));
			}
			if (Region.bOil)
			{
				Worth.Add(TEXT("oil"));
			}
			if (Region.bPort)
			{
				Worth.Add(TEXT("harbour"));
			}
			FPlateSpot& Spot = PlateSpots.AddDefaulted_GetRef();
			Spot.Region = Region.Id;
			Spot.World = MapToWorld(Region.Center.X, Region.Center.Y, GroundAt(Region.Center.X, Region.Center.Y));
			Spot.Reach = (Region.bCapital ? 7.0f : bTown ? 5.2f : 3.6f) * UnitScale * static_cast<float>(TownSpread(Region.Id));
			// (the plate is the town's: "Essen", not "Ruhr". The province's own name is lettered small on the ground.)
			Spot.Name = Towns.IsValidIndex(Region.Id) && Towns[Region.Id].Region == Region.Id && !Towns[Region.Id].Name.IsEmpty() ? Towns[Region.Id].Name : Region.Name;
			Spot.Line = FString::Join(Worth, TEXT("  \u00B7  "));
			Spot.Tier = Region.bCapital ? 0 : bTown ? 1 : 2;
			Spot.Rank = (Region.bCapital ? 1000 : 0) + Region.VictoryPoints * 20 + Region.Factories * 5 + (bTown ? 10 : 0);
		}
		else
		{
			// (its name is lettered by the screen with the rest of the map's lettering: see BuildWords)
		}
	}

	// The names of the countries and of the seas are lettered by the screen, as an atlas letters them (see
	// BuildWords): this only says where each word runs.
	BuildWords();
}

void ACampaignMapActor::BuildWords()
{
	THEATER_SPEED_SCOPE(BuildWords);
	MapWords.Reset();
	int32 NamesMoved = 0, NamesOffShore = 0;
	FString NamesOnWater, NamesOverRoofs;
	// A province's own name, small, lying on its ground: for every province that has no plate, and for every one
	// whose plate carries its town's name instead.
	for (const FCampaignRegion& Region : Data.Regions)
	{
		const bool bTown = Region.bCapital || Region.Terrain == TEXT("urban") || Region.VictoryPoints >= 2;
		const bool bWorth = Region.VictoryPoints > 0 || Region.Factories > 0 || Region.bOil || Region.bPort;
		const bool bPlate = bTown || bWorth;
		const bool bTownNamed = Towns.IsValidIndex(Region.Id) && Towns[Region.Id].Region == Region.Id && !Towns[Region.Id].Name.IsEmpty() && Towns[Region.Id].Name != Region.Name;
		if (bPlate && !bTownNamed)
		{
			continue; // (its plate says its name already)
		}
		FVector2D At = Region.Centroid;
		const double Tall = 1.5; // (a capital letter of the name, in map units)
		bool bOverRoofs = false;
		if (bPlate)
		{
			// Clear of the town, its roofs and its plate. (The art director, 18 November 2026, on build 1648: "Holland"
			// lay across Amsterdam's roofs, where a roof's edge and a spire made its last letters read double. A
			// place nine units off a town's middle was clear of a small town and inside a capital.) The name lies
			// wholly on its own province's open ground: at the province's middle where that is clear of the town,
			// or else round the town, the nearest clear place first, north before south.
			const double Roofs = TownRadius(Region.Id) + 2.5, HalfWide = 0.55 * Tall * Region.Name.Len() + 0.5;
			auto Clear = [&](const FVector2D& Spot)
			{
				if (Data.FindRegionAt(Spot.X, Spot.Y) != Region.Id || GroundAt(Spot.X, Spot.Y) <= 0.0f)
				{
					return false;
				}
				// (no nearer its own town's middle than the last roofs, anywhere along the word; and the plate hangs south of the middle)
				const double Across = FMath::Clamp(Region.Center.X, Spot.X - HalfWide, Spot.X + HalfWide);
				if (FVector2D::Distance(FVector2D(Across, Spot.Y), Region.Center) <= Roofs
					|| (FMath::Abs(Spot.X - Region.Center.X) < HalfWide + 6.0 && Spot.Y > Region.Center.Y && Spot.Y < Region.Center.Y + Roofs + 5.0))
				{
					return false;
				}
				// (nor across the roofs of a town next door, where an end of the word lies over the border)
				for (const int32 Next : Region.Neighbors)
				{
					if (Data.Regions.IsValidIndex(Next) && FirstTownRadius(Data.Regions[Next]) > 0.0)
					{
						const FVector2D Theirs = Data.Regions[Next].Center;
						const double Nearest = FMath::Clamp(Theirs.X, Spot.X - HalfWide, Spot.X + HalfWide);
						if (FVector2D::Distance(FVector2D(Nearest, Spot.Y), Theirs) <= TownRadius(Next) + 1.5)
						{
							return false;
						}
					}
				}
				return true;
			};
			// (The art director, 18 November 2026, on build 1668: Holland's "H" sat on Amsterdam's harbour crane at the
			// coast. A name keeps off the coast and the quays: every part of it lies on dry land with a letter's
			// height of land round it, and clear of its own harbour and its neighbours', whose cranes, sheds and
			// ships stand up in front of what lies north of them. A province with no such place keeps the place it
			// had by the rule above.)
			auto OffShore = [&](const FVector2D& Spot)
			{
				for (const double End : { -1.0, -0.5, 0.0, 0.5, 1.0 })
				{
					for (const double Up : { -1.0, 0.0, 1.0 })
					{
						if (GroundAt(Spot.X + End * (HalfWide + 0.8), Spot.Y + Up * Tall) <= 0.0f)
						{
							return false;
						}
					}
				}
				auto BehindQuay = [&](int32 Port)
				{
					const FVector2D* const Quay = TownQuays.Find(Port);
					// (beside the quay, or within five units north of it, where the crane stands in front of the word)
					return Quay && FMath::Abs(Spot.X - Quay->X) < HalfWide + 3.0 && Spot.Y - Quay->Y > -6.0 && Spot.Y - Quay->Y < 3.0;
				};
				if (BehindQuay(Region.Id))
				{
					return false;
				}
				for (const int32 Next : Region.Neighbors)
				{
					if (BehindQuay(Next))
					{
						return false;
					}
				}
				return true;
			};
			const bool bWasClear = Clear(At);
			bool bPlaced = bWasClear && OffShore(At);
			for (double Out = Roofs + 1.5; Out < Roofs + 26.0 && !bPlaced; Out += 3.0)
			{
				for (const double Bearing : { 0.0, -45.0, 45.0, -90.0, 90.0, 180.0, -135.0, 135.0 })
				{
					const double Turn = FMath::DegreesToRadians(Bearing);
					const FVector2D Spot = Region.Center + FVector2D(FMath::Sin(Turn) * (Out + FMath::Abs(FMath::Sin(Turn)) * HalfWide), -FMath::Cos(Turn) * Out);
					if (Clear(Spot) && OffShore(Spot))
					{
						At = Spot;
						bPlaced = true;
						if (bWasClear)
						{
							++NamesOffShore;
						}
						else
						{
							++NamesMoved;
						}
						break;
					}
				}
			}
			if (!bPlaced && !bWasClear)
			{
				// (The art director, the same day: "a province with no name is a bigger loss than a name over a few
				// roofs". The nearest open field of its own first; failing that, the water off its own shore; failing
				// both, it keeps the place it had, a little smaller, with a soft dark shadow under it.)
				auto OnWater = [&](const FVector2D& Spot)
				{
					for (const double End : { -1.0, 0.0, 1.0 })
					{
						if (Data.FindRegionAt(Spot.X + End * HalfWide, Spot.Y) != INDEX_NONE)
						{
							return false;
						}
					}
					// (its own water: the first land met on the way back to its town is its own)
					const double Far = FVector2D::Distance(Spot, Region.Center);
					for (double Back = 1.0; Back < Far; Back += 1.0)
					{
						const FVector2D Step = Spot + (Region.Center - Spot) * (Back / Far);
						const int32 Ashore = Data.FindRegionAt(Step.X, Step.Y);
						if (Ashore != INDEX_NONE)
						{
							return Ashore == Region.Id;
						}
					}
					return false;
				};
				const FVector2D Kept = At;
				int32 Found = 0; // 1 on its own open ground, 2 on its own water
				for (int32 Pass = 1; Pass <= 2 && Found == 0; ++Pass)
				{
					for (double Out = Roofs + 1.5; Out < Roofs + 26.0 && Found == 0; Out += 3.0)
					{
						for (const double Bearing : { 0.0, -45.0, 45.0, -90.0, 90.0, 180.0, -135.0, 135.0 })
						{
							const double Turn = FMath::DegreesToRadians(Bearing);
							// (to the side of the town a word needs its own half length more)
							const FVector2D Spot = Region.Center + FVector2D(FMath::Sin(Turn) * (Out + FMath::Abs(FMath::Sin(Turn)) * HalfWide), -FMath::Cos(Turn) * Out);
							if (Pass == 1 ? Clear(Spot) : OnWater(Spot))
							{
								At = Spot;
								Found = Pass;
								break;
							}
						}
					}
				}
				if (Found == 1)
				{
					++NamesMoved;
				}
				else if (Found == 2)
				{
					NamesOnWater += (NamesOnWater.IsEmpty() ? TEXT("") : TEXT(", ")) + Region.Name;
				}
				else
				{
					// (as it lay: north of the town, or wherever in the province there was room nine units off)
					At = Kept;
					if (FVector2D::Distance(At, Region.Center) < 9.0)
					{
						for (const FVector2D& Offset : { FVector2D(0.0, -9.0), FVector2D(-10.0, -4.0), FVector2D(10.0, -4.0), FVector2D(0.0, 13.0) })
						{
							if (Data.FindRegionAt(Region.Center.X + Offset.X, Region.Center.Y + Offset.Y) == Region.Id)
							{
								At = Region.Center + Offset;
								break;
							}
						}
					}
					bOverRoofs = true;
					NamesOverRoofs += (NamesOverRoofs.IsEmpty() ? TEXT("") : TEXT(", ")) + Region.Name;
				}
			}
		}
		FMapWord& Word = MapWords.AddDefaulted_GetRef();
		Word.Text = Region.Name;
		Word.Height = bOverRoofs ? Tall * 0.85 : Tall;
		Word.bOverRoofs = bOverRoofs;
		Word.From = Word.To = At;
		Word.bRegion = true;
	}
	if (NamesMoved > 0 || NamesOffShore > 0 || !NamesOnWater.IsEmpty() || !NamesOverRoofs.IsEmpty())
	{
		UE_LOG(LogTheater, Display, TEXT("Names: %d provinces' names are moved onto open ground clear of their towns' roofs, and %d more off the coast and the quays; on their own water: %s; over their roofs, smaller and shadowed: %s"),
			NamesMoved, NamesOffShore, NamesOnWater.IsEmpty() ? TEXT("none") : *NamesOnWater, NamesOverRoofs.IsEmpty() ? TEXT("none") : *NamesOverRoofs);
	}
	// Seas: across the middle of each, in spaced italic capitals.
	for (const FCampaignSeaZone& Sea : Data.Seas)
	{
		FMapWord& Word = MapWords.AddDefaulted_GetRef();
		Word.Sea = static_cast<int32>(&Sea - Data.Seas.GetData());
		Word.Text = Sea.Name.ToUpper();
		Word.Height = 4.6;
		const double Long = Word.Height * 1.75 * FMath::Max(Word.Text.Len() - 1, 1);
		Word.From = Sea.Center - FVector2D(Long * 0.5, 0.0);
		Word.To = Sea.Center + FVector2D(Long * 0.5, 0.0);
		Word.bSea = true;
	}
	// The great lakes of the wider chart: each named along its own length, in the seas' lettering, smaller.
	for (const FCampaignLake& Lake : Data.Lakes)
	{
		if (Lake.Outline.Num() < 3 || Lake.Name.IsEmpty())
		{
			continue;
		}
		// (its long axis: the line through its middle along which its shore is spread widest)
		FVector2D Middle = FVector2D::ZeroVector;
		for (const FVector2D& P : Lake.Outline)
		{
			Middle += P;
		}
		Middle /= Lake.Outline.Num();
		double Sxx = 0.0, Sxy = 0.0, Syy = 0.0;
		for (const FVector2D& P : Lake.Outline)
		{
			const FVector2D D = P - Middle;
			Sxx += D.X * D.X;
			Sxy += D.X * D.Y;
			Syy += D.Y * D.Y;
		}
		const double Turn = 0.5 * FMath::Atan2(2.0 * Sxy, Sxx - Syy);
		FVector2D Along(FMath::Cos(Turn), FMath::Sin(Turn));
		if (Along.X < 0.0)
		{
			Along = -Along; // (read from the west)
		}
		double Least = 0.0, Most = 0.0;
		for (const FVector2D& P : Lake.Outline)
		{
			const double T = (P - Middle) | Along;
			Least = FMath::Min(Least, T);
			Most = FMath::Max(Most, T);
		}
		FMapWord& Word = MapWords.AddDefaulted_GetRef();
		Word.Text = Lake.Name.ToUpper();
		// (no longer than six tenths of the lake: a small lake's name is lettered smaller, to a point)
		const double Room = 0.6 * (Most - Least);
		Word.Height = FMath::Clamp(Room / (1.75 * FMath::Max(Word.Text.Len() - 1, 1)), 1.3, 2.6);
		const double Long = Word.Height * 1.75 * FMath::Max(Word.Text.Len() - 1, 1);
		const FVector2D Centre = Middle + Along * (0.5 * (Least + Most));
		Word.From = Centre - Along * (Long * 0.5);
		Word.To = Centre + Along * (Long * 0.5);
		Word.bSea = true;
	}
	// Countries: each along the long axis of the ground it holds today, worked out from its provinces, spanning
	// about two thirds of it, with a slight arc. (It is worked out again whenever ground changes hands.)
	struct FHeld
	{
		double Area = 0.0;
		FVector2D Weighted = FVector2D::ZeroVector;
		TArray<int32> Regions;
	};
	TMap<FString, FHeld> Lands;
	for (const FCampaignRegion& Region : Data.Regions)
	{
		if (Region.bColony || Region.Area <= 0.0)
		{
			continue;
		}
		FHeld& Held = Lands.FindOrAdd(Region.Owner);
		Held.Area += Region.Area;
		Held.Weighted += Region.Centroid * Region.Area;
		Held.Regions.Add(Region.Id);
	}
	// A country's name goes across its main body: the ground joined to its capital (or, if the capital is lost, the
	// largest piece of what it holds). Its islands, its colonies and its far conquests do not pull the name askew.
	for (TPair<FString, FHeld>& Pair : Lands)
	{
		TSet<int32> Mine(Pair.Value.Regions), Best;
		TArray<int32> Seeds = Pair.Value.Regions;
		Seeds.Sort([this](int32 A, int32 B) { return Data.Regions[A].bCapital > Data.Regions[B].bCapital; });
		TSet<int32> Seen;
		double BestArea = 0.0;
		for (int32 Seed : Seeds)
		{
			if (Seen.Contains(Seed))
			{
				continue;
			}
			TSet<int32> Piece;
			TArray<int32> Open = { Seed };
			double Area = 0.0;
			while (Open.Num() > 0)
			{
				const int32 Id = Open.Pop();
				if (Piece.Contains(Id))
				{
					continue;
				}
				Piece.Add(Id);
				Seen.Add(Id);
				Area += Data.Regions[Id].Area;
				for (int32 Next : Data.Regions[Id].Neighbors)
				{
					if (Mine.Contains(Next) && !Piece.Contains(Next))
					{
						Open.Add(Next);
					}
				}
			}
			const bool bSeat = Data.Regions[Seed].bCapital;
			if (bSeat || Area > BestArea)
			{
				BestArea = Area;
				Best = Piece;
			}
			if (bSeat)
			{
				break; // the piece the capital stands in is the one
			}
		}
		Pair.Value.Regions = Best.Array();
		Pair.Value.Area = 0.0;
		Pair.Value.Weighted = FVector2D::ZeroVector;
		for (int32 Id : Pair.Value.Regions)
		{
			Pair.Value.Area += Data.Regions[Id].Area;
			Pair.Value.Weighted += Data.Regions[Id].Centroid * Data.Regions[Id].Area;
		}
	}
	for (const TPair<FString, FHeld>& Pair : Lands)
	{
		const FCampaignNation* Nation = Data.FindNation(Pair.Key);
		if (!Nation || Pair.Value.Area < 140.0)
		{
			continue; // too small a country to letter across
		}
		const FVector2D Middle = Pair.Value.Weighted / Pair.Value.Area;
		// The long axis: the direction along which the country's ground is spread the most.
		double Sxx = 0.0, Syy = 0.0, Sxy = 0.0;
		for (int32 Id : Pair.Value.Regions)
		{
			const FVector2D D = Data.Regions[Id].Centroid - Middle;
			const double W = Data.Regions[Id].Area;
			// (a province is not a point: its own spread counts too, so that a country of one province has an axis)
			Sxx += W * (D.X * D.X + W / 12.0);
			Syy += W * (D.Y * D.Y + W / 12.0);
			Sxy += W * D.X * D.Y;
		}
		const double Angle = 0.5 * FMath::Atan2(2.0 * Sxy, Sxx - Syy);
		FVector2D Axis(FMath::Cos(Angle), FMath::Sin(Angle));
		// A country nearly as broad as it is long is lettered straight across, as a reader expects.
		const double Spread = FMath::Sqrt(FMath::Square(Sxx - Syy) + 4.0 * Sxy * Sxy) / FMath::Max(Sxx + Syy, 1.0);
		if (Spread < 0.38)
		{
			Axis = FVector2D(1.0, 0.0);
		}
		if (Axis.X < 0.0)
		{
			Axis = -Axis; // read from west to east
		}
		{
			// (never stood on end: tilted fifty-five degrees at the most)
			const double Tilt = FMath::Clamp(FMath::Atan2(Axis.Y, Axis.X), -FMath::DegreesToRadians(55.0), FMath::DegreesToRadians(55.0));
			Axis = FVector2D(FMath::Cos(Tilt), FMath::Sin(Tilt));
		}
		const FVector2D Cross(Axis.Y, -Axis.X); // toward the letters' tops
		double Lo = 1.0e9, Hi = -1.0e9, Down = 1.0e9, Up = -1.0e9;
		for (int32 Id : Pair.Value.Regions)
		{
			const FVector2D D = Data.Regions[Id].Centroid - Middle;
			const double Reach = FMath::Sqrt(Data.Regions[Id].Area / UE_DOUBLE_PI);
			Lo = FMath::Min(Lo, FVector2D::DotProduct(D, Axis) - Reach);
			Hi = FMath::Max(Hi, FVector2D::DotProduct(D, Axis) + Reach);
			Down = FMath::Min(Down, FVector2D::DotProduct(D, Cross) - Reach);
			Up = FMath::Max(Up, FVector2D::DotProduct(D, Cross) + Reach);
		}
		const FString Name = Nation->Name.ToUpper();
		const int32 Gaps = FMath::Max(Name.Len() - 1, 1);
		const double Span = (Hi - Lo) * 0.66;
		// (letters well spaced out, as an atlas spaces them: a capital under half as high as the letters stand apart)
		const double Height = FMath::Min3(Span / Gaps * 0.44, (Up - Down) * 0.2, 20.0);
		// (A name that would come out smaller than eight units a capital is left off: under the flags and plates
		// of a small country it only made a smudge. Its capital's plate names the place.)
		if (Height < 8.0)
		{
			continue;
		}
		const double MiddleAlong = (Lo + Hi) * 0.5;
		FMapWord& Word = MapWords.AddDefaulted_GetRef();
		Word.Text = Name;
		Word.Height = Height;
		// (no wider than its letters can fill: a short name is not strung out across a long country)
		// (and no more than a capital and a half of space between one letter and the next, or the word falls apart)
		const double Long = FMath::Min(Span, Height * 2.2 * Gaps);
		FVector2D From = Middle + Axis * (MiddleAlong - Long * 0.5), To = Middle + Axis * (MiddleAlong + Long * 0.5);
		// The name keeps clear of the capital and its plate (which hangs just south of the town): the baseline is
		// slid a capital and a half off it, to whichever side stays inside the country. If neither does, the
		// plate wins and the country is not lettered.
		bool bRoom = true;
		for (int32 Id : Pair.Value.Regions)
		{
			const FCampaignRegion& Seat = Data.Regions[Id];
			if (!Seat.bCapital)
			{
				continue;
			}
			// (From high up, where the names are read, a plate is as big on the screen as some forty map units by
			// twelve, hanging south of a town that is itself a dozen across: that is the room the name must leave.)
			// (the town's models reach about six units north of its middle; its plate hangs from seven units south
			// of it and is another eighteen deep as the far view shows it)
			// (measured on the far view, where the table's far half is drawn at under a pixel to the map unit: the
			// plate there is as deep as twenty-five units and as wide as fifty)
			const FVector2D Plate = Seat.Center + FVector2D(0.0, 6.0 + 7.0 * TownSpread(Seat.Id));
			const double Along = FVector2D::DotProduct(Plate - From, Axis), Off = FVector2D::DotProduct(Plate - From, Cross);
			const double Clear = Height * 0.5 + 21.0;
			if (Along < -(28.0 + Height) || Along > Long + 28.0 + Height || FMath::Abs(Off) >= Clear)
			{
				continue; // the plate is not in the word's way
			}
			bRoom = false;
			for (const double Side : { Off >= 0.0 ? -1.0 : 1.0, Off >= 0.0 ? 1.0 : -1.0 })
			{
				// (to the near side first: the smaller slide)
				const FVector2D Slide = Cross * (Side * Clear + Off);
				const FVector2D Mid = (From + To) * 0.5 + Slide;
				const int32 Under = Data.FindRegionAt(Mid.X, Mid.Y);
				if (Under != INDEX_NONE && Data.Regions[Under].Owner == Pair.Key
					&& Data.FindRegionAt(From.X + Slide.X + Axis.X * Long * 0.2, From.Y + Slide.Y + Axis.Y * Long * 0.2) != INDEX_NONE
					&& Data.FindRegionAt(To.X + Slide.X - Axis.X * Long * 0.2, To.Y + Slide.Y - Axis.Y * Long * 0.2) != INDEX_NONE)
				{
					From += Slide;
					To += Slide;
					bRoom = true;
					break;
				}
			}
			break;
		}
		if (!bRoom)
		{
			MapWords.Pop(); // (the word was begun above)
			continue;
		}
		Word.From = From;
		Word.To = To;
		Word.Bulge = Long * 0.035;
	}
}

FVector ACampaignMapActor::ArmySpot(int32 RegionId, int32 Slot, int32 Count, bool* bOutSmall) const
{
	const FCampaignRegion& Region = Data.Regions[RegionId];
	// Beside the town and clear of its roofs. First a row north by east of it (not north-east: a town's first
	// factory stands out to the north-east), or the same to the north-west; then round the town by other
	// bearings; then further out, for a town that stands at the edge of its province. If the province has not
	// room for all its armies like that, they are drawn at four fifths and the search is made again with less
	// room asked for. Only when even that fails does an army stand in the middle of the province.
	const int32 Wanted = FMath::Max(Count, Slot + 1);
	auto Collect = [this, &Region, Wanted](double Size, TArray<FVector2D>& Spots)
	{
		const double Out = TownRadius(Region.Id) + 8.0 * Size, Apart = 14.6 * Size;
		auto Try = [this, &Region, &Spots, Wanted, Apart](const FVector2D& Spot)
		{
			if (Spots.Num() >= Wanted || Data.FindRegionAt(Spot.X, Spot.Y) != Region.Id || GroundAt(Spot.X, Spot.Y) <= 0.0f)
			{
				return;
			}
			for (const FVector2D& Taken : Spots)
			{
				if (FVector2D::DistSquared(Taken, Spot) < (Apart - 0.5) * (Apart - 0.5))
				{
					return;
				}
			}
			Spots.Add(Spot);
		};
		auto Toward = [](double Bearing) { const double Rad = FMath::DegreesToRadians(Bearing); return FVector2D(FMath::Sin(Rad), -FMath::Cos(Rad)); }; // (degrees east of north)
		for (int32 K = 0; K < 3; ++K)
		{
			Try(Region.Center + FVector2D(0.34 * Out + Apart * K, -0.94 * Out));
		}
		for (int32 K = 0; K < 3; ++K)
		{
			Try(Region.Center + FVector2D(-0.34 * Out - Apart * K, -0.94 * Out));
		}
		for (const double Bearing : { 75.0, -75.0, 125.0, -125.0, 180.0, 100.0, -100.0, 150.0, -150.0 })
		{
			Try(Region.Center + Toward(Bearing) * Out);
		}
		for (const double Further : { 6.0, 12.0, 20.0, 30.0 })
		{
			for (double Bearing = 20.0; Bearing < 380.0 && Spots.Num() < Wanted; Bearing += 20.0)
			{
				Try(Region.Center + Toward(Bearing) * (Out + Further));
			}
		}
	};
	TArray<FVector2D> Spots;
	Collect(1.0, Spots);
	bool bSmall = false;
	if (Spots.Num() < Wanted)
	{
		bSmall = true;
		Spots.Reset();
		Collect(0.8, Spots);
	}
	if (bOutSmall)
	{
		*bOutSmall = bSmall;
	}
	const FVector2D Spot = Spots.IsValidIndex(Slot) ? Spots[Slot] : Slot == 0 ? Region.Centroid : Region.Center + FVector2D(0.0, -6.0 * Slot);
	return SurfacePoint(Spot.X, Spot.Y, 0.0);
}

AArmyPiece* ACampaignMapActor::FindPiece(int32 RegionId, const FString& Nation) const
{
	const TObjectPtr<AArmyPiece>* Found = PieceByKey.Find(FString::Printf(TEXT("%d|%s"), RegionId, *Nation));
	return Found ? Found->Get() : nullptr;
}

AArmyPiece* ACampaignMapActor::FindPieceOfCorps(int32 CorpsId) const
{
	for (AArmyPiece* Piece : Pieces)
	{
		if (Piece && Piece->HasCorps(CorpsId))
		{
			return Piece;
		}
	}
	return nullptr;
}

AArmyPiece* ACampaignMapActor::PieceAtRay(const FVector& Origin, const FVector& Direction, double* OutDistance, const AArmyPiece* PassOver) const
{
	AArmyPiece* Best = nullptr;
	double Nearest = TNumericLimits<double>::Max();
	auto Try = [&](AArmyPiece* Piece)
	{
		double Distance = 0.0;
		if (Piece && Piece != PassOver && !Piece->IsHidden() && Piece->HitByRay(Origin, Direction, Distance) && Distance < Nearest)
		{
			Nearest = Distance;
			Best = Piece;
		}
	};
	for (AArmyPiece* Piece : Pieces)
	{
		Try(Piece);
	}
	// Fleets and air fleets can be pointed at too.
	for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : FleetPieces)
	{
		Try(Pair.Value);
	}
	for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : AirPieces)
	{
		Try(Pair.Value);
	}
	if (!Best && GunLineMesh && GunLineMesh->IsVisible())
	{
		// (Rulings 108 and 109.) A gun of an army's line: it belongs to the army, and a click on it picks the army up.
		for (const FGunLine& Line : GunLines)
		{
			const double Along = FVector::DotProduct(Line.Middle - Origin, Direction);
			AArmyPiece* Owner = Line.Piece.Get();
			if (Owner && Owner != PassOver && !Owner->IsHidden() && Along > 0.0 && Along < Nearest && FVector::DistSquared(Origin + Direction * Along, Line.Middle) < Line.Radius * Line.Radius)
			{
				Nearest = Along;
				Best = Owner;
			}
		}
	}
	if (OutDistance)
	{
		*OutDistance = Nearest;
	}
	return Best;
}

/** How hard the winter lies in each month of the year: 0 is January. */
static float WinterHardInMonth(int32 Month)
{
	static const float Winter[12] = { 1.0f, 0.92f, 0.5f, 0.12f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.08f, 0.42f, 0.86f };
	return Winter[((Month % 12) + 12) % 12];
}

void ACampaignMapActor::WarmModels(const FCampaignView& View)
{
	TArray<FString> Nations;
	for (const FViewCorps& Unit : View.Corps)
	{
		Nations.AddUnique(Unit.Nation);
	}
	for (const FViewFleet& Fleet : View.Fleets)
	{
		Nations.AddUnique(Fleet.Nation);
	}
	// (the year as ShowArmies counts it)
	FMiniatureMesh::WarmModels(Nations, 1939 + (FMath::Max(View.Turn, 1) + 7) / 12, View.DesignYears);
}

void ACampaignMapActor::ShowArmies(const FCampaignView& View, const TArray<FCorpsMove>& Marches, const FVector* CarriedFrom)
{
	THEATER_SPEED_SCOPE(ShowArmies);
	// (The painter is told the month before any piece is painted: a campaign taken up again lays out its armies
	// before its season, and a dominion's pieces of 1943 would wear the roundel of 1939.)
	{
		int32 Held = 0;
		::UnitTable::SetTurn(FParse::Value(FCommandLine::Get(), TEXT("TheaterSeason="), Held) ? Held : View.Turn);
	}
	// (Release 2, nicknames.ts) Whether earned names are lettered over the flags, told to the pieces before any is set.
	AArmyPiece::bNamesShown = View.bNickFlags;
	UWorld* World = GetWorld();
	if (!World || !Data.IsLoaded() || HasParade())
	{
		return; // (with the models out for inspection, the armies stay in their box)
	}
	// The first month is September 1939.
	FMiniatureMesh::Year = 1939 + (FMath::Max(View.Turn, 1) + 7) / 12;
	FMiniatureMesh::DesignYears = View.DesignYears;
	WarmModels(View); // (tc.Speed.ModelWarm: the model files for the months ahead are queued, to be read on another thread)
	// (Release 2: camps you can see.) How many months finish an army's works, for the stage each piece's camp has
	// reached; and who holds what and who is at war with whom, for the side each camp's trench faces.
	AArmyPiece::FullWorks = FMath::Max(1, View.DugFull);
	CampHolders = View.RegionHolder;
	CampWars = View.Wars;

	// Where every corps' piece stood before this change, for the ones that are about to march.
	TMap<int32, FVector> Before;
	for (const AArmyPiece* Piece : Pieces)
	{
		if (Piece)
		{
			for (const FViewCorps& Unit : Piece->GetCorps())
			{
				Before.Add(Unit.Id, Piece->GetRestLocation());
			}
		}
	}
	TSet<FString> KeysBefore;
	PieceByKey.GetKeys(KeysBefore);

	// One piece for each nation's force in a region, however many corps it holds.
	TArray<FString> Order;
	TMap<FString, TArray<FViewCorps>> Groups;
	TMap<int32, TArray<FString>> NationsIn;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (!Data.Regions.IsValidIndex(Unit.Region))
		{
			continue;
		}
		const FString Key = FString::Printf(TEXT("%d|%s"), Unit.Region, *Unit.Nation);
		if (!Groups.Contains(Key))
		{
			Order.Add(Key);
			NationsIn.FindOrAdd(Unit.Region).Add(Unit.Nation);
		}
		Groups.FindOrAdd(Key).Add(Unit);
	}

	// Take away the pieces of armies that are no longer there.
	for (auto It = PieceByKey.CreateIterator(); It; ++It)
	{
		if (!Groups.Contains(It.Key()))
		{
			if (It.Value())
			{
				It.Value()->Destroy();
			}
			It.RemoveCurrent();
		}
	}

	Pieces.Reset();
	for (const FString& Key : Order)
	{
		const TArray<FViewCorps>& Group = Groups[Key];
		const FCampaignNation* Nation = Data.FindNation(Group[0].Nation);
		if (!Nation)
		{
			continue;
		}
		TArray<FString>& Present = NationsIn[Group[0].Region];
		Present.Sort();
		// (the player's own army stands first, where it leads a crowded province)
		if (!View.Human.IsEmpty() && Present.Remove(View.Human) > 0)
		{
			Present.Insert(View.Human, 0);
		}
		bool bSmall = false;
		const FVector Spot = ArmySpot(Group[0].Region, Present.IndexOfByKey(Group[0].Nation), Present.Num(), &bSmall);
		const FVector PieceSize(bSmall ? 0.8 : 1.0); // (in a crowded province the bases are drawn at four fifths)

		TObjectPtr<AArmyPiece>& Piece = PieceByKey.FindOrAdd(Key);
		if (!Piece)
		{
			Piece = World->SpawnActor<AArmyPiece>(Spot, FRotator(0.0f, LastPieceYaw, 0.0f));
			RoundPiece(Piece); // (the far side: a march over the chart's seam, Alaska to the Yukon, goes the short way)
		}
		if (Piece)
		{
			// (Ruling 154: a foreign army in a province he only glimpses is a flag and no more.)
			Piece->SetGlimpsed(Group[0].Nation != View.Human && View.Glimpsed.Contains(Group[0].Region));
			{
				// (whether snow lies where it stands this month, for the whitewash of a winter army: the same snow the
				// season lays, asked here because a campaign taken up again lays out its armies before its season)
				int32 Held = 0;
				const int32 TurnNow = FParse::Value(FCommandLine::Get(), TEXT("TheaterSeason="), Held) ? Held : View.Turn;
				const float Hard = WinterHardInMonth(TurnNow + 7);
				Piece->SetOnSnow(Data.Regions.IsValidIndex(Group[0].Region) && SnowLying(Data.Regions[Group[0].Region].Center, Hard) >= 0.5f);
			}
			Piece->SetArmy(this, *Nation, Group[0].Region, Group);
			Piece->SetRestLocation(Spot);
			Piece->SetSize(static_cast<float>(PieceSize.X));
			Piece->SetStandard(StandardGrowth, StandardLean);
			Pieces.Add(Piece);
		}
	}

	// A crowded province shows one flag with a count, and fans out when it is pointed at.
	// (Flags of one side only are folded together: the player's and his allies', or the others'. An enemy's flag
	// is never put away behind a friend's.)
	CrowdLead.Reset();
	CrowdFriends = TSet<FString>(View.Allied);
	if (!View.Human.IsEmpty())
	{
		CrowdFriends.Add(View.Human);
	}
	for (const TPair<int32, TArray<FString>>& In : NationsIn)
	{
		for (int32 Side = 0; Side < 2; ++Side)
		{
			TArray<FString> Together;
			for (const FString& Tag : In.Value)
			{
				if ((CrowdFriends.Contains(Tag) ? 0 : 1) == Side)
				{
					Together.Add(Tag);
				}
			}
			if (Together.Num() >= 2)
			{
				CrowdLead.Add(FIntPoint(In.Key, Side), Together[0]);
			}
		}
	}
	ApplyCrowds();

	// Carry the corps that have just moved along the road they took. Corps that arrive together travel as one piece.
	TMap<AArmyPiece*, TArray<const FCorpsMove*>> Arrivals;
	for (const FCorpsMove& March : Marches)
	{
		if (AArmyPiece* Piece = FindPieceOfCorps(March.CorpsId))
		{
			Arrivals.FindOrAdd(Piece).Add(&March);
		}
	}
	for (const auto& Arrival : Arrivals)
	{
		AArmyPiece* Piece = Arrival.Key;
		const FCorpsMove& Lead = *Arrival.Value[0];
		const FVector* From = Before.Find(Lead.CorpsId);
		if (!From || Piece->IsGlimpsed())
		{
			continue; // (ruling 154: a flag at a glimpse does not march: it is simply found where it now is)
		}
		TArray<FVector> Road;
		if (CarriedFrom)
		{
			// The player carried it here by hand: it only has to settle onto its place.
			Road.Add(*CarriedFrom);
		}
		else
		{
			Road.Add(*From);
			for (int32 i = 0; i + 1 < Lead.Path.Num(); ++i)
			{
				if (Data.Regions.IsValidIndex(Lead.Path[i]))
				{
					Road.Add(ArmySpot(Lead.Path[i], 0));
				}
			}
		}
		Road.Add(Piece->GetRestLocation());

		const FString Key = FString::Printf(TEXT("%d|%s"), Piece->GetRegionId(), *Piece->GetNationTag());
		if (!KeysBefore.Contains(Key))
		{
			// Nobody of theirs was there before: the piece itself makes the march.
			Piece->MarchAlong(Road, false, CarriedFrom == nullptr);
		}
		else if (const FCampaignNation* Nation = Data.FindNation(Piece->GetNationTag()))
		{
			// They are joining an army already there: a piece of their own makes the march and is taken off on arrival.
			TArray<FViewCorps> Travellers;
			for (const FCorpsMove* March : Arrival.Value)
			{
				if (const FViewCorps* Unit = View.FindCorps(March->CorpsId))
				{
					Travellers.Add(*Unit);
				}
			}
			if (AArmyPiece* Courier = World->SpawnActor<AArmyPiece>(CarriedFrom ? *CarriedFrom : *From, Piece->Upright()))
			{
				RoundPiece(Courier);
				Courier->SetArmy(this, *Nation, Piece->GetRegionId(), Travellers);
				Courier->MarchAlong(Road, true, CarriedFrom == nullptr);
				if (AArmyPiece::CarriedWithWeight())
				{
					// (Release 2, armies with weight: the army it joins takes its weight as it lands. UpdateSmoke watches for it.)
					FJoining Join;
					Join.Sent = Courier;
					Join.Joined = Piece;
					Join.From = Road[0];
					Join.Stood = Piece->GetRestLocation();
					Join.Due = World->GetTimeSeconds() + Courier->MarchLasts();
					Joining.Add(Join);
				}
			}
		}
	}

	ApplyCrowds(); // (a piece on the march is not put away)

	// (How each province stands to the player, and who is at war with him: an army's guns stand on the side away
	// from its enemy.)
	GunStanding = View.RegionStanding;
	GunFoes.Reset();
	for (const FCampaignRegion& Region : Data.Regions)
	{
		if (GunStanding.IsValidIndex(Region.Id) && GunStanding[Region.Id] == TEXT("enemy"))
		{
			GunFoes.Add(Region.Owner);
		}
	}
	BuildGunLines();
	BuildCamps(); // (after the guns, which a camp keeps clear of)

	// The fleets and the air fleets are part of the same picture.
	ShowFleets(View);
}

void ACampaignMapActor::BuildGunLines()
{
	THEATER_SPEED_SCOPE(BuildGunLines);
	if (!GunLineMesh || !Data.IsLoaded() || !bPiecesShown)
	{
		return; // (with the pieces lifted off the table the lines are out of sight, and are made again when they are set back)
	}
	// ("-TheaterGuns" gives two armies in three guns they have not got, for a picture.)
	static const bool bGunsLent = FParse::Param(FCommandLine::Get(), TEXT("TheaterGuns"));
	const double U = UnitScale;
	struct FWanted
	{
		AArmyPiece* Piece = nullptr;
		int32 Guns = 0;
		int32 Side = 0; // 0 the player's, 1 his enemy's, 2 neither
		FVector2D At = FVector2D::ZeroVector; // the base's middle, on the map
		FVector2D Toward = FVector2D(0.0, -1.0); // where the enemy lies
		double Wide = 0.0; // the base's width, in the map's units
	};
	TArray<FWanted> Wanted;
	GunsAway.Reset();
	FString Made;
	uint32 Stood = 0; // (where every base stands: a line gives way to another army's base)
	for (AArmyPiece* Piece : Pieces)
	{
		if (!Piece || Piece->IsHidden() || !Piece->IsArmy() || !Data.Regions.IsValidIndex(Piece->GetRegionId()))
		{
			continue;
		}
		const FVector2D At = WorldToMap(Piece->GetRestLocation());
		Stood = HashCombine(Stood, GetTypeHash(FIntPoint(FMath::RoundToInt32(At.X * 4.0), FMath::RoundToInt32(At.Y * 4.0))));
		// (Only what the player knows: a corps only glimpsed is not known to be artillery, and an army under a
		// question mark shows no guns.)
		int32 Artillery = 0, Rest = 0, Known = 0;
		for (const FViewCorps& Unit : Piece->GetCorps())
		{
			Known += Unit.bDim ? 0 : 1;
			if (!Unit.bDim && Unit.Kind == TEXT("art"))
			{
				++Artillery;
			}
			else
			{
				++Rest;
			}
		}
		if (bGunsLent && Known > 0 && Rest > 0)
		{
			Artillery = FMath::Max(Artillery, Piece->GetRegionId() % 3);
		}
		if (Artillery == 0 || Rest == 0)
		{
			continue; // (no guns; or nothing but guns, which keeps its gun on the base)
		}
		if (Piece->IsMarching() || FVector::DistSquared2D(Piece->GetReckonedLocation(), Piece->GetRestLocation()) > FMath::Square(0.5 * U))
		{
			GunsAway.Add(Piece); // (in the hand or on the road: its guns are put down with it)
			continue;
		}
		FWanted& Line = Wanted.AddDefaulted_GetRef();
		Line.Piece = Piece;
		Line.Guns = Artillery >= 2 ? 3 : 2;
		Line.At = At;
		// (the base as it is seen: the counters' width and the wooden foot's ring round them)
		Line.Wide = 2.0 * (Piece->GetBaseRadius() + 0.72 * 1.4 * U) * Piece->GetActorScale3D().X / U;
		Line.Side = CrowdFriends.Contains(Piece->GetNationTag()) ? 0 : GunFoes.Contains(Piece->GetNationTag()) ? 1 : 2;
		// Where the enemy lies: the enemy's ground next door; failing that, his nearest ground; failing that (a country
		// at peace) away from its own capital.
		const int32 Here = Piece->GetRegionId();
		const int32 Side = Line.Side;
		auto Foe = [this, Side](int32 Region)
		{
			if (!GunStanding.IsValidIndex(Region) || Side == 2)
			{
				return false;
			}
			const FString& Standing = GunStanding[Region];
			return Side == 0 ? Standing == TEXT("enemy") : (Standing == TEXT("own") || Standing == TEXT("ally"));
		};
		FVector2D Sum = FVector2D::ZeroVector;
		for (const int32 Next : Data.Regions[Here].Neighbors)
		{
			if (Data.Regions.IsValidIndex(Next) && Foe(Next))
			{
				Sum += (Data.Regions[Next].Center - At).GetSafeNormal();
			}
		}
		if (Sum.IsNearlyZero(0.05) && Side != 2)
		{
			double Nearest = FMath::Square(900.0);
			for (const FCampaignRegion& Region : Data.Regions)
			{
				const double Far = FVector2D::DistSquared(Region.Center, At);
				if (Far < Nearest && Region.Id != Here && Foe(Region.Id))
				{
					Nearest = Far;
					Sum = Region.Center - At;
				}
			}
		}
		if (Sum.IsNearlyZero(0.05))
		{
			for (const FCampaignRegion& Region : Data.Regions)
			{
				if (Region.bCapital && Region.Owner == Piece->GetNationTag() && Region.Id != Here)
				{
					Sum = At - Region.Center;
					break;
				}
			}
		}
		Line.Toward = Sum.IsNearlyZero(0.05) ? FVector2D(0.0, -1.0) : Sum.GetSafeNormal();
		Made += FString::Printf(TEXT("%d|%s|%d|%.1f,%.1f|%.2f|%.2f;"), Here, *Piece->GetNationTag(), Line.Guns, At.X, At.Y, Line.Wide, FMath::Atan2(Line.Toward.Y, Line.Toward.X));
	}
	Made += FString::Printf(TEXT("#%u"), Stood);
	if (Made == GunLinesBuilt)
	{
		return;
	}
	GunLinesBuilt = Made;
	GunLines.Reset();
	GunLineMesh->ClearAllMeshSections();
	// (The art director's round 38: the patch clears its trees. The squares of country where a line stood, and where
	// one stands now, are planted again when the lines are made: see the end of this.)
	const TArray<FVector2D> ClearedBefore = MoveTemp(GunClearings);
	GunClearings.Reset();
	auto Replant = [this, &ClearedBefore]()
	{
		if (TreeSquare <= 0.0 || TreeMeshes.Num() == 0)
		{
			return;
		}
		const double Side = TreeSquare / UnitScale; // (a square of country, in the map's units)
		TSet<FIntPoint> Squares;
		auto Touch = [&Squares, Side](const TArray<FVector2D>& Spots)
		{
			for (const FVector2D& Spot : Spots)
			{
				for (const FVector2D& Corner : { FVector2D(-5.0, -5.0), FVector2D(5.0, -5.0), FVector2D(-5.0, 5.0), FVector2D(5.0, 5.0) })
				{
					Squares.Add(FIntPoint(FMath::FloorToInt32((Spot.X + Corner.X) / Side), FMath::FloorToInt32((Spot.Y + Corner.Y) / Side)));
				}
			}
		};
		// (only where something changed: a spot that was clear and is clear still needs nothing)
		TArray<FVector2D> Changed;
		for (const FVector2D& Spot : ClearedBefore)
		{
			if (!GunClearings.ContainsByPredicate([&Spot](const FVector2D& Each) { return FVector2D::DistSquared(Each, Spot) < 0.01; }))
			{
				Changed.Add(Spot);
			}
		}
		for (const FVector2D& Spot : GunClearings)
		{
			if (!ClearedBefore.ContainsByPredicate([&Spot](const FVector2D& Each) { return FVector2D::DistSquared(Each, Spot) < 0.01; }))
			{
				Changed.Add(Spot);
			}
		}
		Touch(Changed);
		if (Squares.Num() > 0)
		{
			BuildTrees(&Squares);
		}
	};
	if (Wanted.Num() == 0)
	{
		Replant();
		return;
	}
	FMiniatureMesh Model;
	FCampaignMeshBuffers Earth;
	// (the art director's earth: about (166,138,96), at half strength, soft at its edges)
	const FLinearColor Trodden(FColor(166, 138, 96));
	TArray<FVector2D> Taken; // (where other armies' guns already stand)
	int32 NoRoom = 0, Swung = 0, Closed = 0, Dropped = 0, Beside = 0;
	FString NoRoomAt;
	int32 Shut[7] = { 0, 0, 0, 0, 0, 0, 0 }; // (what shut the ground straight behind: water or the border, ground not its own, a town, a river or railway, a base, other guns)
	for (const FWanted& Line : Wanted)
	{
		const int32 Here = Line.Piece->GetRegionId();
		const double Wide = Line.Wide;
		// (the rivers and railways near enough to matter, found once)
		TArray<TPair<FVector2D, FVector2D>> Ways;
		const double Reach = FMath::Square(2.2 * Wide);
		for (const FLandRail& Rail : Land.Rails)
		{
			if (Data.Regions.IsValidIndex(Rail.A) && Data.Regions.IsValidIndex(Rail.B) && FVector2D::DistSquared(Rail.At, Line.At) < FMath::Square(160.0))
			{
				const FVector2D Stops[3] = { Data.Regions[Rail.A].Center, Rail.At, Data.Regions[Rail.B].Center };
				for (int32 Leg = 0; Leg < 2; ++Leg)
				{
					if (FMath::PointDistToSegmentSquared(FVector(Line.At, 0.0), FVector(Stops[Leg], 0.0), FVector(Stops[Leg + 1], 0.0)) < Reach)
					{
						Ways.Emplace(Stops[Leg], Stops[Leg + 1]);
					}
				}
			}
		}
		for (const FLandRiver& River : Land.Rivers)
		{
			for (int32 K = 0; K + 1 < River.Course.Num(); ++K)
			{
				if (FMath::PointDistToSegmentSquared(FVector(Line.At, 0.0), FVector(River.Course[K], 0.0), FVector(River.Course[K + 1], 0.0)) < Reach)
				{
					Ways.Emplace(River.Course[K], River.Course[K + 1]);
				}
			}
		}
		auto Spot = [&Line, Wide](double Aside, double Along)
		{
			const double Turn = FMath::Atan2(-Line.Toward.Y, -Line.Toward.X) + FMath::DegreesToRadians(Aside + Along);
			return Line.At + FVector2D(FMath::Cos(Turn), FMath::Sin(Turn)) * (1.1 * Wide);
		};
		// (what shuts a place to a gun: 0 nothing, it is open)
		auto ShutBy = [&](const FVector2D& Place, bool bAtHome)
		{
			const int32 Under = Data.FindRegionAt(Place.X, Place.Y);
			if (!Data.Regions.IsValidIndex(Under) || GroundAt(Place.X, Place.Y) <= 0.0f || (bAtHome && Under != Here))
			{
				return 1; // (water, or off the map, or over the border when room at home is asked for)
			}
			// (never on ground that stands otherwise than the army's own: not the enemy's, not a neutral's)
			if (Under != Here && GunStanding.IsValidIndex(Under) && GunStanding.IsValidIndex(Here) && GunStanding[Under] != GunStanding[Here])
			{
				return 2;
			}
			// (never on a town)
			for (const int32 Town : { Under, Here })
			{
				if (FVector2D::DistSquared(Place, Data.Regions[Town].Center) < FMath::Square(TownRadius(Town) + 0.24 * Wide))
				{
					return 3;
				}
			}
			// (never on a railway or in a river: a gun's own length clear of it)
			for (const TPair<FVector2D, FVector2D>& Way : Ways)
			{
				if (FMath::PointDistToSegmentSquared(FVector(Place, 0.0), FVector(Way.Key, 0.0), FVector(Way.Value, 0.0)) < FMath::Square(0.3 * Wide))
				{
					return 4;
				}
			}
			// (never over another base, nor among another army's guns)
			for (const AArmyPiece* Other : Pieces)
			{
				if (Other && Other != Line.Piece && !Other->IsHidden() && FVector2D::DistSquared(WorldToMap(Other->GetRestLocation()), Place) < FMath::Square(0.8 * Wide))
				{
					return 5;
				}
			}
			for (const FVector2D& Stands : Taken)
			{
				if (FVector2D::DistSquared(Stands, Place) < FMath::Square(0.4 * Wide))
				{
					return 6;
				}
			}
			return 0;
		};
		auto Open = [&ShutBy](const FVector2D& Place, bool bAtHome) { return ShutBy(Place, bAtHome) == 0; };
		// The guns in a shallow arc, 20 degrees either side of the middle for three and 11 for two. Straight behind
		// first; then swung either way, up to 45 degrees; then closed up; then one gun fewer. Room in the army's own
		// province is taken before room over a friendly border.
		auto Round = [](int32 Count, int32 K, double Close) { return (Count == 2 ? (K == 0 ? -11.0 : 11.0) : (K - 1) * 20.0) * Close; };
		int32 Guns = 0;
		double Swing = 0.0, Gap = 1.0;
		// (Last of all, where even that finds no room, the line may stand at the base's side, up to a quarter turn
		// round: better beside it than not at all, or he could not tell that the army has guns.)
		static const double Swings[] = { 0.0, 15.0, -15.0, 30.0, -30.0, 45.0, -45.0, 60.0, -60.0, 75.0, -75.0, 90.0, -90.0 };
		for (int32 Resort = 0; Resort < 2 && Guns == 0; ++Resort)
		for (int32 Asked = Line.Guns; Asked >= 2 && Guns == 0; --Asked)
		{
			for (const double TryGap : { 1.0, 0.75 })
			{
				for (int32 Home = 1; Home >= 0 && Guns == 0; --Home)
				{
					for (int32 Which = Resort == 0 ? 0 : 7; Which < (Resort == 0 ? 7 : 13); ++Which)
					{
						const double TrySwing = Swings[Which];
						bool bAll = true;
						for (int32 K = 0; K < Asked && bAll; ++K)
						{
							bAll = Open(Spot(TrySwing, Round(Asked, K, TryGap)), Home == 1);
						}
						if (bAll)
						{
							Guns = Asked;
							Swing = TrySwing;
							Gap = TryGap;
							break;
						}
					}
				}
				if (Guns > 0)
				{
					break;
				}
			}
		}
		if (Guns == 0)
		{
			++NoRoom; // (never one gun alone, which reads as a stray piece: an army with no room shows none)
			NoRoomAt += (NoRoomAt.IsEmpty() ? TEXT("") : TEXT(", ")) + Data.Regions[Here].Name;
			continue;
		}
		Swung += Swing != 0.0 ? 1 : 0;
		Beside += FMath::Abs(Swing) > 45.0 ? 1 : 0;
		if (FMath::Abs(Swing) > 45.0)
		{
			NoRoomAt += (NoRoomAt.IsEmpty() ? TEXT("") : TEXT(", ")) + FString::Printf(TEXT("at the side in %s"), *Data.Regions[Here].Name);
		}
		Closed += Gap < 1.0 ? 1 : 0;
		Dropped += Guns < Line.Guns ? 1 : 0;
		if (Swing != 0.0 || Gap < 1.0 || Guns < Line.Guns)
		{
			for (int32 K = 0; K < Line.Guns; ++K)
			{
				const int32 Why = ShutBy(Spot(0.0, Round(Line.Guns, K, 1.0)), true);
				if (Why != 0)
				{
					++Shut[Why];
					break;
				}
			}
		}
		// The nation's own gun or the standard one, at four fifths of the size of the pieces on the base: on the
		// art director's painting a gun runs 190 px beside a base 410 px across.
		const double Metre = (Line.Piece->GetBaseRadius() * Line.Piece->GetActorScale3D().X / 3.7) * 0.8;
		const FString Gun = FMiniatureMesh::MainGunOf(Line.Piece->GetNationTag());
		// (A model's front is toward -X: turned so that its front looks where the enemy lies. The way there is taken
		// on the table, whose axes are not the map's.)
		const FVector Ahead = MapToWorld(Line.At.X + Line.Toward.X, Line.At.Y + Line.Toward.Y) - MapToWorld(Line.At.X, Line.At.Y);
		const double Faces = FMath::RadiansToDegrees(FMath::Atan2(Ahead.Y, Ahead.X)) + 180.0;
		FVector Middle = FVector::ZeroVector;
		for (int32 K = 0; K < Guns; ++K)
		{
			const FVector2D Place = Spot(Swing, Round(Guns, K, Gap));
			const FVector Ground = SurfacePointOver(Place.X, Place.Y, 0.1);
			if (!Model.Vehicle(Gun, Ground, Metre, Faces, false))
			{
				Model.Vehicle(TEXT("Gun75:") + Line.Piece->GetNationTag(), Ground, Metre, Faces, false);
			}
			Taken.Add(Place);
			Middle += Ground;
			// (no tree on its earth: at the gun, and half way to the next)
			GunClearings.Add(Place);
			if (K + 1 < Guns)
			{
				GunClearings.Add((Place + Spot(Swing, Round(Guns, K + 1, Gap))) * 0.5);
			}
		}
		FGunLine& Kept = GunLines.AddDefaulted_GetRef();
		Kept.Piece = Line.Piece;
		Kept.Middle = Middle / Guns + FVector(0.0, 0.0, 0.5 * U);
		Kept.Radius = (Guns == 3 ? 0.52 : 0.4) * Wide * U;
		// The trodden earth: one worn band under the guns that joins them, half strength in the middle and nothing
		// at its edges and ends. No base, no rim.
		{
			const double First = Round(Guns, 0, Gap) - 13.0, Last = Round(Guns, Guns - 1, Gap) + 13.0;
			const int32 Steps = FMath::Max(4, FMath::CeilToInt32((Last - First) / 4.0));
			const double Across[4] = { -0.36, -0.17, 0.17, 0.36 };
			const double Back = FMath::Atan2(-Line.Toward.Y, -Line.Toward.X);
			TArray<int32> Before;
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const double Bearing = Back + FMath::DegreesToRadians(Swing + FMath::Lerp(First, Last, static_cast<double>(Step) / Steps));
				const float End = static_cast<float>(FMath::SmoothStep(0.0, 1.0, FMath::Min(Step, Steps - Step) / 2.0));
				TArray<int32> Row;
				for (int32 K = 0; K < 4; ++K)
				{
					const FVector2D Place = Line.At + FVector2D(FMath::Cos(Bearing), FMath::Sin(Bearing)) * ((1.1 + Across[K]) * Wide);
					FLinearColor Ink = Trodden;
					Ink.A = (K == 1 || K == 2) ? 0.5f * End : 0.0f;
					Row.Add(Earth.AddVertex(SurfacePointOver(Place.X, Place.Y, 0.07), FVector::UpVector, Ink.ToFColor(false)));
				}
				if (Before.Num() == 4)
				{
					for (int32 K = 0; K < 3; ++K)
					{
						Earth.AddTriangleFacingUp(Before[K], Before[K + 1], Row[K]);
						Earth.AddTriangleFacingUp(Before[K + 1], Row[K], Row[K + 1]);
					}
				}
				Before = Row;
			}
		}
	}
	if (Model.Vertices.Num() > 0)
	{
		GunLineMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		GunLineMesh->SetMaterial(0, PieceMaterial);
		SeamLaidAgain(GunLineMesh); // (seam-2: its twin over the chart's seam is laid again)
	}
	Earth.Commit(GunLineMesh, 1, GunEarthInk ? static_cast<UMaterialInterface*>(GunEarthInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
	Replant();
	UE_LOG(LogTheater, Log, TEXT("Gun lines: %d armies show their guns (%d swung round, %d of them to the base's side, %d closed up, %d a gun short), %d have no room (%s), %d are in the hand or on the road; %d corners. Straight behind was shut by water or the border %d, ground not its own %d, a town %d, a river or railway %d, a base %d, other guns %d"),
		GunLines.Num(), Swung, Beside, Closed, Dropped, NoRoom, *NoRoomAt, GunsAway.Num(), Model.Vertices.Num(), Shut[1], Shut[2], Shut[3], Shut[4], Shut[5], Shut[6]);
}

void ACampaignMapActor::ApplyCrowds()
{
	THEATER_SPEED_SCOPE(ApplyCrowds);
	if (bAwayForBattle)
	{
		return;
	}
	for (AArmyPiece* Piece : Pieces)
	{
		if (!Piece)
		{
			continue;
		}
		const FString* Lead = CrowdLead.Find(FIntPoint(Piece->GetRegionId(), CrowdFriends.Contains(Piece->GetNationTag()) ? 0 : 1));
		const bool bFanned = !Lead || Piece->GetRegionId() == HoveredRegion || Piece->GetRegionId() == SelectedRegion;
		const bool bPutAway = !bFanned && Piece->GetNationTag() != *Lead && !Piece->IsMarching();
		if (Piece->IsHidden() != bPutAway)
		{
			Piece->SetActorHiddenInGame(bPutAway);
		}
	}
}

void ACampaignMapActor::GetCrowds(TArray<TPair<AArmyPiece*, int32>>& Out) const
{
	Out.Reset();
	for (const TPair<FIntPoint, FString>& Crowd : CrowdLead)
	{
		if (Crowd.Key.X == HoveredRegion || Crowd.Key.X == SelectedRegion)
		{
			continue; // fanned out: every army shows its own flag
		}
		AArmyPiece* Lead = FindPiece(Crowd.Key.X, Crowd.Value);
		int32 Others = 0;
		for (const AArmyPiece* Piece : Pieces)
		{
			Others += Piece && Piece->GetRegionId() == Crowd.Key.X && Piece->IsHidden() && (CrowdFriends.Contains(Piece->GetNationTag()) ? 0 : 1) == Crowd.Key.Y ? 1 : 0;
		}
		if (Lead && Others > 0)
		{
			Out.Emplace(Lead, Others);
		}
	}
}

void ACampaignMapActor::ShowFleets(const FCampaignView& View)
{
	UWorld* World = GetWorld();
	if (!World || !Data.IsLoaded())
	{
		return;
	}
	const FRotator Facing(0.0f, LastPieceYaw, 0.0f);

	// (Release 2, a fleet under way: see CVarShipWay.) The way by water is laid as a ship's course and timed, on the
	// map (map units; the map's own east and south, which are the world's +Y and -X: a quarter turn, the same hand), and
	// handed to the piece as her times in the world. False where there is no course to sail: she sails as before.
	static const bool bShipWayAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterShipWay"));
	const bool bShipWay = bShipWayAsked || CVarShipWay.GetValueOnGameThread() != 0;
	// (Release 2, fleets that keep their heading: see CVarFleetHeading.)
	static const bool bFleetHeadingAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterFleetHeading"));
	const bool bFleetHeading = bFleetHeadingAsked || CVarFleetHeading.GetValueOnGameThread() != 0;
	auto SailShipWay = [this, bFleetHeading](AArmyPiece* Piece, const TArray<FVector2D>& MapWay, bool bRestsHeaded) -> bool
	{
		TheaterShipWay::FAsk Ask;
		Ask.Way.reserve(static_cast<size_t>(MapWay.Num()));
		for (const FVector2D& Point : MapWay)
		{
			Ask.Way.push_back(TheaterShipWay::FSpot{ Point.X, Point.Y });
		}
		// (A piece at rest faces the table, its bows at the table's yaw: she leaves so and comes to rest so. On the map
		// a yaw is the angle from east round toward south.)
		const double Yaw = FMath::DegreesToRadians(static_cast<double>(LastPieceYaw));
		Ask.Bows = TheaterShipWay::FSpot{ FMath::Cos(Yaw), FMath::Sin(Yaw) };
		Ask.Rest = Ask.Bows;
		if (bFleetHeading)
		{
			// (Fleets that keep their heading.) She leaves on the heading she lies on, which may not be the table's; and
			// where she will lie headed her course ends as its own last leg runs (no heading asked: TheaterShipWay::Plan).
			const double Departs = FMath::DegreesToRadians(Piece->GetActorRotation().Yaw);
			Ask.Bows = TheaterShipWay::FSpot{ FMath::Cos(Departs), FMath::Sin(Departs) };
			if (bRestsHeaded)
			{
				Ask.Rest = TheaterShipWay::FSpot{ 0.0, 0.0 };
			}
		}
		Ask.Turn = FMath::Max(CVarShipTurn.GetValueOnGameThread(), 0.0f);
		Ask.Sweep = FMath::Max(CVarShipSweep.GetValueOnGameThread(), 0.0f);
		Ask.Berth = FMath::Max(CVarShipBerth.GetValueOnGameThread(), 0.0f);
		Ask.Step = 0.75; // (as closely as the way itself was asked of the water)
		const TheaterShipWay::FCourse Course = TheaterShipWay::Plan(Ask, [this](TheaterShipWay::FSpot P) { return CoverAt(P.X, P.Y) <= 0.35f; });
		if (Course.Points.size() < 2 || Course.Length <= 0.0)
		{
			return false;
		}
		const double Pace = FMath::Clamp(CVarShipPace.GetValueOnGameThread(), 0.25f, 4.0f);
		const TheaterShipWay::FTimes Times = TheaterShipWay::Time(Course, TheaterShipWay::Seconds(Course.Length) * Pace, FMath::Max(CVarShipHelm.GetValueOnGameThread(), 0.2f));
		const int32 Many = static_cast<int32>(Times.At.size());
		TArray<FVector> Where;
		TArray<FVector2D> Heads;
		TArray<double> Ran;
		Where.Reserve(Many);
		Heads.Reserve(Many);
		Ran.Reserve(Many);
		for (size_t K = 0; K < Times.At.size(); ++K)
		{
			Where.Add(MapToWorld(Times.At[K].X, Times.At[K].Y, 0.02));
			Heads.Add(FVector2D(-Times.Head[K].Y, Times.Head[K].X));
			Ran.Add(Times.Ran[K] * UnitScale);
		}
		Piece->SailCourse(Where, Heads, Ran, Times.Tick, LastPieceYaw);
		return Piece->IsSailing();
	};

	// ---- Fleets. One at sea lies in the middle of its sea; one in harbour lies off the coast of its base.
	TSet<int32> Afloat;
	TMap<FIntPoint, int32> Crowd; // how many pieces are already at a place: (1, sea) or (2, base)
	for (const FViewFleet& Fleet : View.Fleets)
	{
		const FCampaignNation* Nation = Data.FindNation(Fleet.Nation);
		if (!Nation || !Data.Seas.IsValidIndex(Fleet.Zone))
		{
			continue;
		}
		const FVector2D Sea = Data.Seas[Fleet.Zone].Center;
		FVector2D At = Sea;
		const bool bInHarbour = Fleet.Stance == TEXT("port") && Data.Regions.IsValidIndex(Fleet.Base);
		int32& Others = Crowd.FindOrAdd(bInHarbour ? FIntPoint(2, Fleet.Base) : FIntPoint(1, Fleet.Zone));
		if (bInHarbour)
		{
			// Out from the port toward its sea until the water is reached, and a little beyond.
			// (Release 2, the far side: toward its sea the short way round, where the sea's middle lies over the chart's seam.)
			const FVector2D Port = Data.Regions[Fleet.Base].Center;
			const FVector2D Out = (FVector2D(NearX(Sea.X, Port.X), Sea.Y) - Port).GetSafeNormal();
			At = Port;
			for (int32 Step = 0; Step < 80 && CoverAt(At.X, At.Y) > 0.35f; ++Step)
			{
				At += Out * 1.5;
			}
			// (ships in harbour lie close in and close together: they are small there, with a badge on the port in
			// place of a standard over each fleet)
			At += Out * 3.2 + FVector2D(-Out.Y, Out.X) * ((Others % 2 == 0 ? 1.0 : -1.0) * ((Others + 1) / 2) * 5.0);
		}
		else
		{
			// Several fleets in one sea lie in a ring round its middle.
			const double Angle = Others * 2.1 + Fleet.Zone;
			At += FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Others > 0 ? 16.0 : 0.0);
		}
		At.X = LaidX(At.X); // (the far side: laid in the chart's own turn of the world, wherever the steps out ended)
		++Others;
		TObjectPtr<AArmyPiece>& Piece = FleetPieces.FindOrAdd(Fleet.Id);
		const bool bWasLaid = Piece != nullptr;
		if (!Piece)
		{
			Piece = World->SpawnActor<AArmyPiece>(MapToWorld(At.X, At.Y), Facing);
			RoundPiece(Piece); // (the far side: a sail over the chart's seam, San Diego to Pearl Harbor, goes the short way)
		}
		// (Release 2: ships that sail.) A fleet seen where it was and where it is now, that has changed its sea or
		// come in or gone out of harbour, sails there by water. A fleet only reported (a sighting, not a fleet seen)
		// does not sail: it is simply found where it is now.
		const FIntPoint Place(Fleet.Zone, bInHarbour ? Fleet.Base : INDEX_NONE);
		const FIntPoint* WasPlace = FleetPlaces.Find(Fleet.Id);
		const FVector2D* WasSpot = FleetSpots.Find(Fleet.Id);
		const bool bSail = bShipsSail && bWasLaid && WasPlace && WasSpot && *WasPlace != Place && !Fleet.bReport
			&& FVector2D::DistSquared(*WasSpot, At) > 4.0;
		// (Release 2: fading wakes.) An enemy fleet seen to change its sea or harbour leaves its wake behind it, the
		// way it went by water, whether or not ships sail on the map. Not the player's own fleets, nor his allies'.
		const bool bFriend = Fleet.Nation == View.Human || (View.RegionStanding.IsValidIndex(Fleet.Base)
			&& (View.RegionStanding[Fleet.Base] == TEXT("own") || View.RegionStanding[Fleet.Base] == TEXT("ally")));
		if (bWakes && bWasLaid && WasPlace && WasSpot && *WasPlace != Place && !Fleet.bReport && !bFriend
			&& FVector2D::DistSquared(*WasSpot, At) > 4.0)
		{
			TArray<FVector2D> Wake = WaterWay(*WasSpot, At);
			if (Wake.Num() >= 2)
			{
				Wakes.RemoveAll([&Fleet, &View](const FWakeTrail& Old) { return Old.Fleet == Fleet.Id && Old.Turn == View.Turn; });
				// (Release 2, the far side.) A way over the chart's seam comes back in the turn of the world it left from,
				// and ends a whole world from where the fleet is laid. Its wake is laid twice, once from where it left and
				// once to where it lies: each is whole, and each lies on ground that is drawn while the eye is near it.
				// (Seam-2: once only where the wakes have a twin over the seam, which lays the other.)
				const double WorldRound = At.X - Wake.Last().X;
				if (FMath::Abs(WorldRound) > 1.0 && !OverSeam())
				{
					FWakeTrail& Twin = Wakes.AddDefaulted_GetRef();
					Twin.Fleet = Fleet.Id;
					Twin.Turn = View.Turn;
					Twin.Points = Wake;
					for (FVector2D& Point : Twin.Points)
					{
						Point.X += WorldRound;
					}
				}
				FWakeTrail& Trail = Wakes.AddDefaulted_GetRef();
				Trail.Fleet = Fleet.Id;
				Trail.Turn = View.Turn;
				Trail.Points = MoveTemp(Wake);
				bWakesDirty = true;
			}
		}
		// (A fleet just let go after being carried by hand is gliding back to where it lay: it sails on from where it is.)
		const bool bCarried = bWasLaid && Piece && Piece->IsMarching() && !Piece->IsSailing();
		const FVector2D From = bCarried ? WorldToMap(Piece->GetActorLocation()) : WasSpot ? *WasSpot : At;
		if (Piece)
		{
			Piece->SetFleet(this, *Nation, Fleet);
			Piece->SetAtAnchor(bInHarbour && !Fleet.bReport);
			Piece->SetStandard(StandardGrowth, StandardLean);
			Piece->SetRestLocation(MapToWorld(At.X, At.Y, 0.02));
			if (bSail)
			{
				const FVector Now = Piece->GetActorLocation();
				const TArray<FVector2D> MapWay = WaterWay(From, At);
				TArray<FVector> Way;
				for (const FVector2D& Point : MapWay)
				{
					Way.Add(MapToWorld(Point.X, Point.Y, 0.02));
				}
				// (No way by water found: it is found at its new place, as before ships sailed, and never crosses land.)
				if (Way.Num() >= 2)
				{
					if (bCarried)
					{
						Way[0] = Now; // (down from the hand onto the water as it goes)
					}
					// (Release 2, a fleet under way.) A ship's course for her, where it is asked for. Not for a fleet let
					// go from the hand, which comes down onto the water as it goes, as before.
					if (bCarried || !bShipWay || !SailShipWay(Piece, MapWay, bFleetHeading && !bInHarbour))
					{
						Piece->SailAlong(Way, LastPieceYaw);
					}
				}
			}
			// (Release 2, fleets that keep their heading: Campaign/FleetHeading.h.) At sea she lies on the heading she came
			// in on; in harbour, and with the switch off, she faces the table as before. A fleet under sail is turned by
			// her sail and keeps what it leaves her with. One found at a new place without sailing there lies the way she
			// moved. One never seen to move lies out from her home port, or out of the sea her harbour is on. (A fleet
			// known only from a report gives nothing away by the way it lies: where its port is, is not said.)
			const bool bLieHeaded = bFleetHeading && !bInHarbour;
			float CameYaw = LastPieceYaw;
			bool bTakeIt = false;
			if (bLieHeaded)
			{
				TheaterFleetHeading::FWay Moved, FromBase, FromHome;
				if (WasPlace && WasSpot && *WasPlace != Place)
				{
					Moved = TheaterFleetHeading::FWay{ NearX(At.X, WasSpot->X) - WasSpot->X, At.Y - WasSpot->Y, true };
					bTakeIt = Moved.Leads() && !Piece->IsSailing();
				}
				if (!Fleet.bReport && Data.Regions.IsValidIndex(Fleet.Base))
				{
					const FVector2D BasePort = Data.Regions[Fleet.Base].Center;
					FromBase = TheaterFleetHeading::FWay{ NearX(At.X, BasePort.X) - BasePort.X, At.Y - BasePort.Y, true };
				}
				if (!Fleet.bReport && Data.Seas.IsValidIndex(Fleet.Home) && Fleet.Home != Fleet.Zone)
				{
					const FVector2D HomeSea = Data.Seas[Fleet.Home].Center;
					FromHome = TheaterFleetHeading::FWay{ NearX(At.X, HomeSea.X) - HomeSea.X, At.Y - HomeSea.Y, true };
				}
				CameYaw = static_cast<float>(TheaterFleetHeading::CameYaw(Moved, FromBase, FromHome, Fleet.Id));
			}
			Piece->LieHeaded(bLieHeaded, LastPieceYaw, CameYaw, bTakeIt);
			Afloat.Add(Fleet.Id);
		}
		FleetPlaces.Add(Fleet.Id, Place);
		FleetSpots.Add(Fleet.Id, At);
	}
	for (auto It = FleetPieces.CreateIterator(); It; ++It)
	{
		if (!Afloat.Contains(It.Key()))
		{
			if (It.Value())
			{
				It.Value()->Destroy();
			}
			FleetPlaces.Remove(It.Key());
			FleetSpots.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
	// (Release 2: fading wakes) laid again only when one is added or a month has passed.
	if (bWakesDirty || WakesLaidFor != View.Turn)
	{
		LayWakes(View.Turn);
	}

	// ---- Air fleets, each over its airfields: to the south-west of the town, where the army does not stand.
	// (Where the table shows the airfields themselves, as it does in the grand campaign, an air group has no piece
	// of its own: the airfield says it is in use, and a click on it shows who flies from it. The map is the clearer for it.)
	TSet<int32> WithAirfield;
	for (const FViewAirfield& Field : View.Airfields)
	{
		WithAirfield.Add(Field.Region);
	}
	TSet<int32> Flying;
	for (const FViewAirFleet& Wing : View.AirFleets)
	{
		const FCampaignNation* Nation = Data.FindNation(Wing.Nation);
		if (!Nation || !Data.Regions.IsValidIndex(Wing.Base) || WithAirfield.Contains(Wing.Base))
		{
			continue;
		}
		int32& Others = Crowd.FindOrAdd(FIntPoint(3, Wing.Base));
		const double Out = TownRadius(Wing.Base) + 5.0; // (outside the last roofs: never across the town or its name)
		FVector2D At = Data.Regions[Wing.Base].Center + FVector2D(-0.92 * Out - 6.5 * Others, 0.4 * Out);
		if (Data.FindRegionAt(At.X, At.Y) != Wing.Base)
		{
			At = Data.Regions[Wing.Base].Center + FVector2D(0.92 * Out + 6.5 * Others, 0.4 * Out);
		}
		++Others;
		TObjectPtr<AArmyPiece>& Piece = AirPieces.FindOrAdd(Wing.Id);
		if (!Piece)
		{
			Piece = World->SpawnActor<AArmyPiece>(SurfacePoint(At.X, At.Y, 0.0), Facing);
			RoundPiece(Piece);
		}
		if (Piece)
		{
			Piece->SetAirFleet(this, *Nation, Wing);
			Piece->SetStandard(StandardGrowth, StandardLean);
			Piece->SetRestLocation(SurfacePoint(At.X, At.Y, 0.0));
			Flying.Add(Wing.Id);
		}
	}
	for (auto It = AirPieces.CreateIterator(); It; ++It)
	{
		if (!Flying.Contains(It.Key()))
		{
			if (It.Value())
			{
				It.Value()->Destroy();
			}
			It.RemoveCurrent();
		}
	}
}

void ACampaignMapActor::SetWakes(bool bOn)
{
	bWakes = bOn;
	if (!bOn)
	{
		ForgetWakes();
	}
}

void ACampaignMapActor::ForgetWakes()
{
	Wakes.Reset();
	bWakesDirty = false;
	WakesLaidFor = INDEX_NONE;
	if (WakeMesh)
	{
		WakeMesh->ClearAllMeshSections();
	}
}

void ACampaignMapActor::LayWakes(int32 Turn)
{
	// (Release 2: fading wakes.) A wake seen this month, and last month's fainter; older ones are gone.
	Wakes.RemoveAll([Turn](const FWakeTrail& Old) { return Turn - Old.Turn > 1 || Old.Turn > Turn; });
	WakesLaidFor = Turn;
	bWakesDirty = false;
	if (!WakeMesh)
	{
		if (!bWakes || Wakes.Num() == 0)
		{
			return;
		}
		WakeMesh = NewObject<UProceduralMeshComponent>(this);
		WakeMesh->SetupAttachment(Root);
		WakeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WakeMesh->RegisterComponent();
		WakeMesh->SetCastShadow(false);
	}
	WakeMesh->ClearAllMeshSections();
	if (!bWakes || Wakes.Num() == 0 || !HighlightMaterial)
	{
		return;
	}
	if (!WakeTrailInk)
	{
		WakeTrailInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		ShownNight = -1.0f; // (laid by night as the other inks of the sea are)
		SetNight(NightNow, bLamplightNow);
	}
	// Foam in short dashes along the way, as a wake breaks up behind a ship: widest and brightest at the head, where
	// the fleet now lies, thinning back to where it came from. Last month's at half the strength.
	const double Dash = 2.6, Gap = 1.9;
	FCampaignMeshBuffers Foam;
	for (const FWakeTrail& Trail : Wakes)
	{
		const float Strength = Turn == Trail.Turn ? 0.42f : 0.2f;
		TArray<double> Marks = { 0.0 };
		for (int32 K = 1; K < Trail.Points.Num(); ++K)
		{
			Marks.Add(Marks.Last() + FVector2D::Distance(Trail.Points[K - 1], Trail.Points[K]));
		}
		const double Length = Marks.Last();
		if (Length < 1.0)
		{
			continue;
		}
		auto PointAt = [&](double D) -> FVector2D
		{
			int32 K = 1;
			while (K < Marks.Num() - 1 && Marks[K] < D)
			{
				++K;
			}
			const double Span = FMath::Max(Marks[K] - Marks[K - 1], 1.0e-6);
			return FMath::Lerp(Trail.Points[K - 1], Trail.Points[K], FMath::Clamp((D - Marks[K - 1]) / Span, 0.0, 1.0));
		};
		// (stopping short of the head, so that the foam does not run under the fleet's own piece)
		const double End = FMath::Max(0.0, Length - 4.0);
		for (double D = 0.0; D < End; D += Dash + Gap)
		{
			const double To = FMath::Min(D + Dash, End);
			const double Head = (D + To) * 0.5 / Length; // 0 at the old end, 1 at the fleet
			const FColor Ink = FLinearColor(0.86f, 0.92f, 0.96f, Strength * static_cast<float>(0.2 + 0.8 * Head)).ToFColor(false);
			AppendRibbon(Foam, { PointAt(D), PointAt(D + (To - D) * 0.5), PointAt(To) }, 0.18 + 0.22 * Head, 0.16, Ink, false);
		}
	}
	Foam.Commit(WakeMesh, 0, WakeTrailInk ? static_cast<UMaterialInterface*>(WakeTrailInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
}

TArray<FVector2D> ACampaignMapActor::WaterWay(const FVector2D& From, const FVector2D& Given) const
{
	// (Release 2: ships that sail.) A search over squares of the chart, in a box round the two points: water is
	// what the harbour's own way out counts as water (land cover no more than 0.35); a square beside land costs a
	// little more, which keeps a fleet off the coasts; then the corners are cut where open water lets them be.
	// (Release 2, the far side.) The short way round the world: the far end is counted in the turn nearest the near
	// one, and the box is searched there (the cover is read whichever turn a place is given in). So the way comes back
	// in the turn it left from, and over the chart's seam its last point is a whole world from the place given.
	const FVector2D To(NearX(Given.X, From.X), Given.Y);
	const double Apart = FVector2D::Distance(From, To);
	TArray<FVector2D> Way = { From, To };
	if (Apart < 1.0)
	{
		return Way;
	}
	auto Wet = [this](double X, double Y) { return CoverAt(X, Y) <= 0.35f; };
	auto Clear = [&](const FVector2D& A, const FVector2D& B, double Step) -> bool
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(A, B) / Step));
		for (int32 K = 1; K < Steps; ++K)
		{
			const FVector2D P = FMath::Lerp(A, B, double(K) / Steps);
			if (!Wet(P.X, P.Y))
			{
				return false;
			}
		}
		return true;
	};
	const double Margin = FMath::Max(30.0, 0.6 * Apart);
	const FVector2D Low(FMath::Min(From.X, To.X) - Margin, FMath::Min(From.Y, To.Y) - Margin);
	const FVector2D High(FMath::Max(From.X, To.X) + Margin, FMath::Max(From.Y, To.Y) + Margin);
	const double Cell = FMath::Max(1.5, FMath::Max(High.X - Low.X, High.Y - Low.Y) / 120.0); // (speed: at most about 120 squares a side, for a month's end that moves many fleets at once)
	if (Clear(From, To, Cell * 0.5))
	{
		return Way; // (open water all the way)
	}
	const int32 NX = FMath::Min(140, FMath::CeilToInt32((High.X - Low.X) / Cell) + 1);
	const int32 NY = FMath::Min(140, FMath::CeilToInt32((High.Y - Low.Y) / Cell) + 1);
	auto CellOf = [&](const FVector2D& P) { return FIntPoint(FMath::Clamp(FMath::RoundToInt32((P.X - Low.X) / Cell), 0, NX - 1), FMath::Clamp(FMath::RoundToInt32((P.Y - Low.Y) / Cell), 0, NY - 1)); };
	auto PointOf = [&](int32 I, int32 J) { return FVector2D(Low.X + I * Cell, Low.Y + J * Cell); };
	TArray<uint8> Water;
	Water.SetNumZeroed(NX * NY);
	for (int32 J = 0; J < NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const FVector2D P = PointOf(I, J);
			Water[J * NX + I] = Wet(P.X, P.Y) ? 1 : 0;
		}
	}
	const FIntPoint Start = CellOf(From), Goal = CellOf(To);
	Water[Start.Y * NX + Start.X] = 1; // (a fleet lying close in under the coast starts and ends on water)
	Water[Goal.Y * NX + Goal.X] = 1;
	auto Coast = [&](int32 I, int32 J) -> bool
	{
		for (const FIntPoint& D : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
		{
			const int32 X = I + D.X, Y = J + D.Y;
			if (X >= 0 && Y >= 0 && X < NX && Y < NY && !Water[Y * NX + X])
			{
				return true;
			}
		}
		return false;
	};
	TArray<float> Cost;
	Cost.Init(TNumericLimits<float>::Max(), NX * NY);
	TArray<int32> Came;
	Came.Init(INDEX_NONE, NX * NY);
	struct FOpen { float Guess; int32 At; };
	TArray<FOpen> Open;
	auto Sooner = [](const FOpen& A, const FOpen& B) { return A.Guess < B.Guess; };
	const int32 StartAt = Start.Y * NX + Start.X, GoalAt = Goal.Y * NX + Goal.X;
	Cost[StartAt] = 0.0f;
	Open.HeapPush(FOpen{ 0.0f, StartAt }, Sooner);
	bool bFound = false;
	while (Open.Num() > 0)
	{
		FOpen Next;
		Open.HeapPop(Next, Sooner);
		if (Next.At == GoalAt)
		{
			bFound = true;
			break;
		}
		const int32 I = Next.At % NX, J = Next.At / NX;
		const float Here = Cost[Next.At];
		if (Next.Guess - FMath::Sqrt(float(FMath::Square(Goal.X - I) + FMath::Square(Goal.Y - J))) > Here + 1.0e-3f)
		{
			continue; // (a stale entry: this square was reached more cheaply since)
		}
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				const int32 X = I + DX, Y = J + DY;
				if ((DX == 0 && DY == 0) || X < 0 || Y < 0 || X >= NX || Y >= NY || !Water[Y * NX + X])
				{
					continue;
				}
				if (DX != 0 && DY != 0 && (!Water[J * NX + X] || !Water[Y * NX + I]))
				{
					continue; // (no slipping between two corners of land)
				}
				const int32 There = Y * NX + X;
				const float Step = (DX != 0 && DY != 0 ? 1.4142f : 1.0f) * (Coast(X, Y) ? 1.6f : 1.0f);
				if (Here + Step < Cost[There])
				{
					Cost[There] = Here + Step;
					Came[There] = Next.At;
					Open.HeapPush(FOpen{ Cost[There] + FMath::Sqrt(float(FMath::Square(Goal.X - X) + FMath::Square(Goal.Y - Y))), There }, Sooner);
				}
			}
		}
	}
	if (!bFound)
	{
		return TArray<FVector2D>(); // (no way by water in the box: no sailing, rather than a line over the land)
	}
	TArray<FVector2D> Squares;
	for (int32 At = GoalAt; At != INDEX_NONE; At = Came[At])
	{
		Squares.Add(PointOf(At % NX, At / NX));
	}
	if (Squares.Num() < 2)
	{
		return TArray<FVector2D>(); // (the two ends in one square with land between: no sailing either)
	}
	Algo::Reverse(Squares);
	Squares[0] = From;
	Squares.Last() = To;
	// Corners cut: from each point kept, on along the squares while open water still lies straight between.
	Way.Reset();
	Way.Add(From);
	int32 Kept = 0;
	while (Kept < Squares.Num() - 1)
	{
		int32 Far = Kept + 1;
		while (Far + 1 < Squares.Num() && Clear(Squares[Kept], Squares[Far + 1], Cell * 0.5))
		{
			++Far;
		}
		Way.Add(Squares[Far]);
		Kept = Far;
	}
	return Way;
}

void ACampaignMapActor::LaySieges()
{
	THEATER_SPEED_SCOPE(LaySieges);
	Paint.Sieges.Reset();
	for (const TPair<int32, FSiegeShown>& Each : SiegesShown)
	{
		if (!Data.Regions.IsValidIndex(Each.Key) || Each.Value.Depth <= 0.0f)
		{
			continue;
		}
		FCampaignGround::FSiege& Siege = Paint.Sieges.AddDefaulted_GetRef();
		Siege.Region = Each.Key;
		Siege.By = Each.Value.By;
		Siege.Depth = Each.Value.Depth;
		Siege.bEverySide = Each.Value.bEverySide;
		Siege.From = Each.Value.From;
		Siege.Town = Data.Regions[Each.Key].Center;
		// (the town's island: its built ground and a quarter more, and never less than 28 pixels at working height, which is about eight map units)
		Siege.Island = static_cast<float>(FMath::Max(TownRadius(Each.Key) * 1.25, 7.9));
		// (but never more than two fifths of the province: in a city province, Warsaw's, the island filled the field
		// and the siege showed nothing. There the besieger's colour is a band inside the frontier that thickens.)
		if (Data.Regions[Each.Key].Area > 0.0)
		{
			Siege.Island = FMath::Min(Siege.Island, static_cast<float>(FMath::Sqrt(0.4 * Data.Regions[Each.Key].Area / UE_DOUBLE_PI)));
		}
	}
}

void ACampaignMapActor::RepaintBeyond(const FBox2D& Changed, FFineSheet* Only)
{
	THEATER_SPEED_SCOPE(RepaintBeyond);
	if (!Data.bWorld || !Changed.bIsValid || Data.Regions.Num() <= Data.FirstBeyond)
	{
		return;
	}
	if (Changed.Min.X >= 0.0 && Changed.Min.Y >= 0.0 && Changed.Max.X <= Data.Width && Changed.Max.Y <= Data.Height)
	{
		return; // (all of it on the map's own ground)
	}
	const double Margin = FCampaignGround::BandReach + 2.0;
	auto Again = [&](FCampaignGround& Painted, FCampaignTerrain& Shape, UTexture2D* Tint, UTexture2D* Relief, UTexture2D* Stripes)
	{
		if (Painted.Tint.Num() != Shape.NX * Shape.NY || Shape.NX <= 0)
		{
			return;
		}
		bool bAny = false;
		FIntRect Box(MAX_int32, MAX_int32, -1, -1); // (what was painted on this sheet, for sending no more than that)
		for (int32 Turn = -1; Turn <= 1; ++Turn)
		{
			// (the changed ground in whichever turn of the world this sheet keeps it)
			const double Shift = Turn * Data.WorldWidth;
			const double X0 = Changed.Min.X + Shift - Margin, X1 = Changed.Max.X + Shift + Margin, Y0 = Changed.Min.Y - Margin, Y1 = Changed.Max.Y + Margin;
			if (X1 < Shape.OriginX || X0 > Shape.OriginX + Shape.ExtentX || Y1 < Shape.OriginY || Y0 > Shape.OriginY + Shape.ExtentY)
			{
				continue;
			}
			bAny = true;
			const int32 I0 = FMath::Clamp(FMath::FloorToInt32((X0 - Shape.OriginX) / Shape.Cell), 0, Shape.NX - 1), I1 = FMath::Clamp(FMath::CeilToInt32((X1 - Shape.OriginX) / Shape.Cell), 0, Shape.NX - 1);
			const int32 J0 = FMath::Clamp(FMath::FloorToInt32((Y0 - Shape.OriginY) / Shape.Cell), 0, Shape.NY - 1), J1 = FMath::Clamp(FMath::CeilToInt32((Y1 - Shape.OriginY) / Shape.Cell), 0, Shape.NY - 1);
			Shape.MeasureFrontiers(Data, I0, J0, I1, J1); // (speed: in the box that changed, on this sheet)
			Painted.PaintTint(Data, Shape, I0, J0, I1, J1);
			Box.Min.X = FMath::Min(Box.Min.X, I0);
			Box.Min.Y = FMath::Min(Box.Min.Y, J0);
			Box.Max.X = FMath::Max(Box.Max.X, I1);
			Box.Max.Y = FMath::Max(Box.Max.Y, J1);
		}
		if (bAny && Tint && Box.Max.X >= Box.Min.X && Box.Max.Y >= Box.Min.Y)
		{
			CampaignTextures::UpdateBoxAll(Tint, Painted.Tint, Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y);
			if (Relief)
			{
				CampaignTextures::UpdateBoxAll(Relief, Painted.Relief, Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y);
			}
			if (Stripes && Painted.Occupied.Num() == Painted.Tint.Num())
			{
				CampaignTextures::UpdateBoxAll(Stripes, Painted.Occupied, Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y);
			}
		}
	};
	if (Only)
	{
		// (Speed: tc.Speed.HoldersLock. A sheet that was being modelled when this ground changed hands, now that it is.)
		Again(Only->Paint, Only->Shape, Only->TintPicture, Only->ReliefPicture, Only->OccupiedPicture);
		return;
	}
	Again(WorldPaint, WorldTerrain, WorldTintPicture, WorldReliefPicture, WorldOccupiedPicture);
	for (const auto& Pair : FineSheets)
	{
		FFineSheet& Sheet = *Pair.Value;
		if (Sheet.bShelved)
		{
			// (Speed: tc.Speed.SheetShelf. Nothing of it is on the table: the ground that changed is kept, and painted
			// when the sheet comes off the shelf, before any of it is seen. See UpdateFineSheets.)
			Sheet.HoldersChanged += Changed;
			continue;
		}
		if (Sheet.bBuilt)
		{
			// (one built and not yet dressed has no pictures: its paint is done again, and its pictures are made from that)
			// (speed, tc.Speed.DressSteps: a sheet part dressed has some of its pictures; one not begun has none)
			Again(Sheet.Paint, Sheet.Shape, Sheet.TintPicture, Sheet.ReliefPicture, Sheet.OccupiedPicture);
		}
	}
	bMiniStale = true;
}

void ACampaignMapActor::SoftenFog(TArray<FColor>& Pixels, int32 NX, int32 NY, double Cell)
{
	if (NX < 3 || NY < 3 || Pixels.Num() != NX * NY || Cell <= 0.0)
	{
		return;
	}
	bool bAny = false;
	TArray<float> Field;
	Field.SetNumUninitialized(Pixels.Num());
	for (int32 Index = 0; Index < Pixels.Num(); ++Index)
	{
		Field[Index] = Pixels[Index].R > 127 ? 1.0f : 0.0f;
		bAny |= Pixels[Index].R > 127;
	}
	if (!bAny)
	{
		return;
	}
	// (Blurred twice over a box: across a straight border that is a hat whose half-breadth is about (2R + 1) cells.
	// The share of it that lies inside, at a distance from the border, is then stretched so that the haze is nothing
	// 1.3 map units outside, about a third on the border, and whole 3.5 units inside: the art director's edge.)
	const int32 Radius = FMath::Clamp(FMath::RoundToInt32((4.5 / Cell - 1.0) * 0.5), 1, 4);
	FCampaignTerrain::Smooth(Field, NX, NY, Radius, 2);
	const double Half = (2.0 * Radius + 1.0) * Cell;
	auto Inside = [Half](double Off)
	{
		const double At = FMath::Clamp(Off / Half, -1.0, 1.0);
		return 0.5 + At - 0.5 * At * FMath::Abs(At);
	};
	const float From = static_cast<float>(Inside(-1.3)), To = static_cast<float>(Inside(3.5));
	for (int32 Index = 0; Index < Pixels.Num(); ++Index)
	{
		Pixels[Index].R = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(255.0f * (Field[Index] - From) / FMath::Max(To - From, 0.05f)), 0, 255));
	}
}

void ACampaignMapActor::HiddenBeyond(TArray<bool>& OutHidden, bool& bOutAny) const
{
	OutHidden.Init(false, Data.Regions.Num());
	bOutAny = false;
	for (const int32 RegionId : HazeSet)
	{
		if (OutHidden.IsValidIndex(RegionId) && RegionId >= Data.FirstBeyond)
		{
			OutHidden[RegionId] = true;
			bOutAny = true;
		}
	}
}

void ACampaignMapActor::LayFog(const TArray<bool>& Hidden, bool bAnyHidden, const FCampaignGround& Painted, const FCampaignTerrain& Shape, TArray<FColor>& OutPixels)
{
	OutPixels.Init(FColor(0, 0, 0, 255), Painted.NX * Painted.NY);
	if (!bAnyHidden || Shape.Region.Num() != OutPixels.Num())
	{
		return;
	}
	for (int32 Index = 0; Index < OutPixels.Num(); ++Index)
	{
		const int32 Id = Painted.PaintedRegion(Index, Shape);
		if (Id >= 0 && Hidden[Id])
		{
			OutPixels[Index].R = 255;
		}
	}
	SoftenFog(OutPixels, Painted.NX, Painted.NY, Shape.Cell);
}

void ACampaignMapActor::FogBeyond()
{
	THEATER_SPEED_SCOPE(FogBeyond);
	if (!Data.bWorld || !WorldLandMaterial || Data.Regions.Num() <= Data.FirstBeyond)
	{
		return;
	}
	TArray<bool> Hidden;
	bool bAnyHidden = false;
	HiddenBeyond(Hidden, bAnyHidden);
	if (!bAnyHidden && !WorldFogPicture)
	{
		return; // (nothing beyond the map is hidden, and nothing was)
	}
	++FogAsked; // (counted either way: a haze laid in the frame outdates one being laid aside, which is then not put on the table)
	if (WorldFogPicture && TheaterSpeed::FogAside())
	{
		// (Speed, 17 November 2026: tc.Speed.FogAside. The world's picture of the haze, 1544 by 851, and each fine
		// sheet's, were laid and softened here, in the frame of the press: most of ShowShroud's 131 ms on the march to
		// Cleveland. They are now laid on another thread and put on the table one a frame after, the same pixels a
		// frame or three later. The map's own haze already comes and goes over half a second.)
		if (!FogAsideBuilding.IsValid())
		{
			LaunchFogAside();
		}
		return; // (one already under way is for an older haze: UpdateFogAside begins again when it is done)
	}
	TArray<FColor> Pixels;
	LayFog(Hidden, bAnyHidden, WorldPaint, WorldTerrain, Pixels);
	if (!WorldFogPicture)
	{
		WorldFogPicture = CampaignTextures::Create(WorldPaint.NX, WorldPaint.NY, Pixels, false, false, TEXT("WorldFog"));
		WorldPictures.Add(WorldFogPicture);
		WorldLandMaterial->SetTextureParameterValue(TEXT("Fog"), WorldFogPicture);
	}
	else
	{
		CampaignTextures::Update(WorldFogPicture, Pixels);
	}
	for (const auto& Pair : FineSheets)
	{
		FFineSheet& Sheet = *Pair.Value;
		if (Sheet.FogPicture) // (speed, tc.Speed.DressSteps: a sheet part dressed has its fog picture or none yet; none before)
		{
			LayFog(Hidden, bAnyHidden, Sheet.Paint, Sheet.Shape, Pixels);
			CampaignTextures::Update(Sheet.FogPicture, Pixels);
		}
	}
}

void ACampaignMapActor::LaunchFogAside()
{
	TSharedPtr<FFogAside, ESPMode::ThreadSafe> Job = MakeShared<FFogAside, ESPMode::ThreadSafe>();
	Job->For = FogAsked;
	HiddenBeyond(Job->Hidden, Job->bAnyHidden);
	for (const auto& Pair : FineSheets)
	{
		if (Pair.Value->FogPicture)
		{
			Job->Sheets.Emplace(Pair.Value, TArray<FColor>());
		}
	}
	FogAside = Job;
	FogLaying = INDEX_NONE;
	// (what it reads is made once and not changed after: which province each point of a sheet is painted for)
	const FCampaignGround* World = &WorldPaint;
	const FCampaignTerrain* WorldShape = &WorldTerrain;
	FogAsideBuilding = Async(EAsyncExecution::ThreadPool, [Job, World, WorldShape]()
	{
		LayFog(Job->Hidden, Job->bAnyHidden, *World, *WorldShape, Job->World);
		for (auto& Each : Job->Sheets)
		{
			LayFog(Job->Hidden, Job->bAnyHidden, Each.Key->Paint, Each.Key->Shape, Each.Value);
		}
	});
}

void ACampaignMapActor::UpdateFogAside()
{
	THEATER_SPEED_SCOPE(UpdateFogAside);
	if (!FogAsideBuilding.IsValid() || !FogAsideBuilding.IsReady() || !FogAside.IsValid())
	{
		return;
	}
	if (FogAside->For != FogAsked)
	{
		if (TheaterSpeed::FogAside())
		{
			LaunchFogAside(); // (the haze has moved again since: laid again for it as it is now)
		}
		else
		{
			// (the switch was turned off since, and the haze was laid in the frame: this one is older, and dropped)
			FogAside.Reset();
			FogAsideBuilding = TFuture<void>();
			FogLaying = INDEX_NONE;
		}
		return;
	}
	// One picture a frame, the world's first.
	FogLaying = FMath::Max(FogLaying, 0);
	if (FogLaying == 0)
	{
		if (WorldFogPicture && FogAside->World.Num() == WorldPaint.NX * WorldPaint.NY)
		{
			CampaignTextures::Update(WorldFogPicture, FogAside->World);
		}
		++FogLaying;
		return;
	}
	while (FogAside->Sheets.IsValidIndex(FogLaying - 1))
	{
		auto& Each = FogAside->Sheets[FogLaying - 1];
		++FogLaying;
		// (a sheet taken away since is passed over; one still standing has its picture still)
		const TSharedPtr<FFineSheet, ESPMode::ThreadSafe>* Still = FineSheets.Find(Each.Key->Key);
		if (Still && *Still == Each.Key && Each.Key->FogPicture && Each.Value.Num() == Each.Key->Paint.NX * Each.Key->Paint.NY)
		{
			CampaignTextures::Update(Each.Key->FogPicture, Each.Value);
			return;
		}
	}
	FogAside.Reset();
	FogAsideBuilding = TFuture<void>();
	FogLaying = INDEX_NONE;
}

void ACampaignMapActor::SetHolders(const TArray<FString>& Holders, const TArray<FString>& League, const TArray<FString>& Owners, const TArray<FCampaignView::FSiegePaint>& Sieges)
{
	THEATER_SPEED_SCOPE(SetHolders);
	{
		// ("-TheaterHold=Virginia:GER,Norfolk:GER" shows those provinces as held by another country, for a picture of
		// occupied ground where the campaign has none: the rules know nothing of it.)
		static FString HoldAsked;
		static const bool bHold = FParse::Value(FCommandLine::Get(), TEXT("TheaterHold="), HoldAsked, false);
		static bool bHolding = false;
		if (bHold && !bHolding)
		{
			TArray<FString> Held = Holders, Lawful = Owners, Each;
			Held.SetNum(Data.Regions.Num());
			Lawful.SetNum(Data.Regions.Num());
			HoldAsked.ParseIntoArray(Each, TEXT(","));
			for (const FString& One : Each)
			{
				FString Province, Tag;
				const int32* Index = One.Split(TEXT(":"), &Province, &Tag) ? Data.RegionByName.Find(Province) : nullptr;
				if (Index && Data.Regions.IsValidIndex(*Index))
				{
					Lawful[*Index] = Data.Regions[*Index].Home;
					Held[*Index] = Tag;
				}
			}
			bHolding = true;
			SetHolders(Held, League, Lawful, Sieges);
			bHolding = false;
			return;
		}
	}
	// Countries that have joined or left the League since the table was last painted: their ground changes colour.
	const TSet<FString> Members(League);
	TSet<FString> Turned;
	for (const FString& Tag : Members)
	{
		if (!Data.League.Contains(Tag))
		{
			Turned.Add(Tag);
		}
	}
	for (const FString& Tag : Data.League)
	{
		if (!Members.Contains(Tag))
		{
			Turned.Add(Tag);
		}
	}
	TArray<FFineSheet*> InTheMaking; // (speed: tc.Speed.HoldersLock) sheets being modelled as this ground changes hands
	if (Data.bWorld)
	{
		// (A fine sheet of the world is modelled and painted on another thread, and reads whose each province is as it
		// goes. Before any of that is changed, a sheet still in the making is waited for: under a second, and only in
		// a month when ground does change hands.)
		// (Speed, 17 November 2026: tc.Speed.HoldersLock. Up to two seconds on the press in America, where the sheets
		// are made as the eye comes near. Now its reads are kept apart from the change by a lock held for a moment, and
		// the sheet has the ground that changed painted again when it is done, before it is shown: nothing waits.)
		bool bWrites = Turned.Num() > 0;
		for (int32 i = 0; i < Data.Regions.Num() && i < Holders.Num() && !bWrites; ++i)
		{
			const FString& Holds = Holders[i].IsEmpty() ? Data.Regions[i].Owner : Holders[i];
			const FString Lawful = Owners.IsValidIndex(i) && Owners[i] != Holds ? Owners[i] : FString();
			bWrites = Holds != Data.Regions[i].Owner || Lawful != Data.Regions[i].Lawful;
		}
		if (bWrites)
		{
			THEATER_SPEED_SCOPE(HoldersWait);
			const bool bLock = TheaterSpeed::HoldersLock();
			for (const auto& Pair : FineSheets)
			{
				if (!Pair.Value->bBuilt && Pair.Value->Building.IsValid())
				{
					if (bLock)
					{
						InTheMaking.Add(Pair.Value.Get());
					}
					else
					{
						Pair.Value->Building.Wait();
					}
				}
			}
		}
	}
	// (speed: tc.Speed.HoldersLock) held while whose each province is changes, where a sheet's thread may be reading it
	TheaterSpeed::FHoldersChange Changing(InTheMaking.Num() > 0);
	if (Turned.Num() > 0)
	{
		Data.League = Members;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// (release 2, the front you can read: where the player has skipped the month's watching, in this frame, the new
	// colour is laid at once, as where ink is not allowed: see SkipInk)
	const bool bInkHere = bInkAllowed && InkSkipFrame != GFrameCounter;
	FBox2D Changed(ForceInit);
	TArray<int32> Taken; // (ink that spreads: the provinces in sight that have a new holder)
	TSet<int32> TakenSet;
	bool bHolders = false;
	for (int32 i = 0; i < Data.Regions.Num() && i < Holders.Num(); ++i)
	{
		const bool bNewHolder = !Holders[i].IsEmpty() && Data.Regions[i].Owner != Holders[i];
		if (bNewHolder)
		{
			Data.Regions[i].Owner = Holders[i];
			bHolders = true;
			if (bLandLaid && bInkHere && GetWorld())
			{
				if (!UnseenSet.Contains(i) && Data.Regions[i].Bounds.bIsValid)
				{
					// (in sight: the new colour seeps in over a second)
					Taken.Add(i);
					TakenSet.Add(i);
				}
				else if (Flashes.Num() < 12)
				{
					// (out of sight, on the staff's paper: it flashes as it changes hands, as it always has)
					Flashes.Emplace(i, Now);
				}
			}
		}
		// (occupied ground: where the holder is not the lawful owner, the owner's stripes lie under the holder's colour)
		const FString Lawful = Owners.IsValidIndex(i) && Owners[i] != Data.Regions[i].Owner ? Owners[i] : FString();
		const bool bNewLaw = Data.Regions[i].Lawful != Lawful;
		if (bNewLaw)
		{
			Data.Regions[i].Lawful = Lawful;
		}
		if ((bNewHolder || bNewLaw || Turned.Contains(Data.Regions[i].Owner)) && Data.Regions[i].Bounds.bIsValid)
		{
			Changed += Data.Regions[i].Bounds;
		}
	}
	Changing.Release();
	for (FFineSheet* Sheet : InTheMaking)
	{
		Sheet->HoldersChanged += Changed; // (painted again when it is done: see UpdateFineSheets)
	}

	// The sieges: the besieger's colour on the field. A siege newly laid comes in from the frontier; one that has
	// gone further moves to its new depth; one that is lifted drains back; where the town has fallen the ink takes over
	// from where the colour stood. (None on ground nobody of the player's can see.)
	// ("-TheaterSiegeAt=0.5" shows every siege that far gone, and "-TheaterSiegeOpen" with its ring open, for pictures)
	static float SiegeHeldAt = -1.0f;
	static const bool bSiegeHeld = FParse::Value(FCommandLine::Get(), TEXT("TheaterSiegeAt="), SiegeHeldAt);
	static const bool bSiegeOpen = FParse::Param(FCommandLine::Get(), TEXT("TheaterSiegeOpen"));
	TSet<int32> Standing;
	for (const FCampaignView::FSiegePaint& Siege : Sieges)
	{
		if (!Data.Regions.IsValidIndex(Siege.Region) || UnseenSet.Contains(Siege.Region) || Siege.By.IsEmpty() || Siege.By == Data.Regions[Siege.Region].Owner)
		{
			continue;
		}
		Standing.Add(Siege.Region);
		// (The art director's round 28: with the depth running evenly the besieger's colour covered seven tenths of
		// the province when the siege was laid and nine tenths half way, so its second half could not be watched:
		// area goes with the square of depth. It is the defender's remaining ground that shrinks evenly.)
		const float Target = 1.0f - 0.65f * FMath::Sqrt(1.0f - FMath::Clamp(bSiegeHeld ? SiegeHeldAt : Siege.Progress, 0.0f, 1.0f));
		FSiegeShown* Known = SiegesShown.Find(Siege.Region);
		if (!Known)
		{
			Known = &SiegesShown.Add(Siege.Region);
			Known->Depth = bLandLaid && bInkHere ? 0.0f : Target; // (laid out with the table, it is already there)
		}
		else if (!bInkHere)
		{
			Known->Depth = Target;
		}
		Known->By = Siege.By;
		Known->From = Siege.From;
		Known->bEverySide = (Siege.bClosed && !bSiegeOpen) || Siege.From.Num() == 0;
		Known->bLeaving = false;
		if (Known->Target != Target || Known->Depth != Target)
		{
			Known->Target = Target;
			Known->Speed = FMath::Max(FMath::Abs(Target - Known->Depth), 0.05f) / 0.6f; // (to its new depth in six tenths of a second)
		}
	}
	TMap<int32, float> StoodAt; // (how far a siege's colour had come where the town has now fallen to it)
	for (auto It = SiegesShown.CreateIterator(); It; ++It)
	{
		if (Standing.Contains(It.Key()))
		{
			continue;
		}
		if (Data.Regions.IsValidIndex(It.Key()) && Data.Regions[It.Key()].Owner == It.Value().By)
		{
			StoodAt.Add(It.Key(), It.Value().Depth);
			It.RemoveCurrent();
		}
		else if (!It.Value().bLeaving)
		{
			It.Value().bLeaving = true;
			It.Value().Target = 0.0f;
			It.Value().Speed = FMath::Max(It.Value().Depth, 0.05f); // (lifted: it drains back to the frontier in a second)
		}
	}
	bSiegesMoving = false;
	for (const TPair<int32, FSiegeShown>& Each : SiegesShown)
	{
		bSiegesMoving |= Each.Value.Depth != Each.Value.Target;
	}
	if (!Changed.bIsValid)
	{
		if (!bLandLaid && SiegesShown.Num() > 0 && Paint.Sieges.Num() == 0)
		{
			// (the table's first laying, with sieges standing: painted in with it)
			for (const TPair<int32, FSiegeShown>& Each : SiegesShown)
			{
				if (Data.Regions[Each.Key].Bounds.bIsValid)
				{
					Changed += Data.Regions[Each.Key].Bounds;
				}
			}
		}
		if (!Changed.bIsValid)
		{
			return;
		}
	}
	bMiniStale = true;
	BuildWords();
	UE_LOG(LogTheater, Log, TEXT("Holders: ground changed hands (%d new holders in sight for ink, ink %s, land %s, %d sieges shown)"), Taken.Num(), bInkHere ? TEXT("allowed") : (bInkAllowed ? TEXT("skipped") : TEXT("not allowed")),
		bLandLaid ? TEXT("laid") : TEXT("not laid"), SiegesShown.Num());

	// Ground has changed hands. Repaint it, with a margin for the band of thick paint along the new
	// frontier, rebuild only the pieces of the land that touches, and redraw the frontier lines.
	const double Margin = FCampaignGround::BandReach + 2.0; // (the band of stronger stain along the new frontier reaches that far in)
	const int32 I0 = FMath::Clamp(FMath::FloorToInt32((Changed.Min.X - Margin) / Terrain.Cell), 0, Terrain.NX - 1);
	const int32 I1 = FMath::Clamp(FMath::CeilToInt32((Changed.Max.X + Margin) / Terrain.Cell), 0, Terrain.NX - 1);
	const int32 J0 = FMath::Clamp(FMath::FloorToInt32((Changed.Min.Y - Margin) / Terrain.Cell), 0, Terrain.NY - 1);
	const int32 J1 = FMath::Clamp(FMath::CeilToInt32((Changed.Max.Y + Margin) / Terrain.Cell), 0, Terrain.NY - 1);
	{
		// (Speed, 17 November: the frontiers are measured again in the box that was changed, not over the whole table.
		// Where the League has turned, ground far from the box changes colour too: then the whole of it.)
		THEATER_SPEED_SCOPE(MeasureFrontiers);
		if (Turned.Num() > 0 || !bLandLaid)
		{
			Terrain.MeasureFrontiers(Data);
		}
		else
		{
			Terrain.MeasureFrontiers(Data, I0, J0, I1, J1);
		}
	}
	// (names written on ground that has changed hands are inked to suit its new paint)
	for (int32 i = 0; i < Labels.Num() && i < LabelRegions.Num(); ++i)
	{
		if (Labels[i] && LabelRegions[i] != INDEX_NONE)
		{
			const FColor Ink = RegionInk(LabelRegions[i]);
			if (Labels[i]->TextRenderColor != Ink)
			{
				Labels[i]->SetTextRenderColor(Ink);
			}
		}
	}
	ShowVictorFlags(); // (a capital that has changed hands flies its new holder's flag, or its own country's is home again)
	// (Speed, ruling b414: the press that takes a province did 24 to 44 ms of its work here, and at times far more,
	// drawing the front where it now runs, and as much again on the frontier lines below. Once the table is laid
	// they are drawn in the two frames after the press instead: the order answers first. "-TheaterLinesAtOnce"
	// draws them in the press's own frame, as before.)
	static const bool bLinesLater = !FParse::Param(FCommandLine::Get(), TEXT("TheaterLinesAtOnce"));
	if (bLinesLater && bLandLaid)
	{
		// (By the frame's number, not by a count of the map's own ticks: an order's answer is laid on the table in the
		// player's tick, and the map's comes later in the same frame, so "the next tick" was this very frame.)
		FrontLineDue = GFrameCounter + 1;
	}
	else
	{
		BuildFrontLine(); // (and the front is drawn where it now runs)
	}

	// Ink that spreads: what is on the table now is kept as the old paint, and the new is let in over a second from
	// the frontier each province shares with ground its new holder held already. (Changes of the same moment run
	// together. A second already running is ended where it stands: it never holds anything up.)
	const bool bInk = Taken.Num() > 0 && Paint.Tint.Num() == Paint.NX * Paint.NY && Paint.Occupied.Num() == Paint.Tint.Num() && !FParse::Param(FCommandLine::Get(), TEXT("TheaterNoInk"));
	if (InkRuns.Num() > 0)
	{
		EndInk();
	}
	if (bInk)
	{
		TintBefore = Paint.Tint;
		OccupiedBefore = Paint.Occupied;
	}
	if (bInk || !bLandLaid)
	{
		// (while ink runs, and at the first laying, a siege's colour stands where it is going)
		for (TPair<int32, FSiegeShown>& Each : SiegesShown)
		{
			Each.Value.Depth = Each.Value.Target;
		}
		bSiegesMoving = false;
	}
	LaySieges();
	// Only the nations' paint changes: the land itself is left as it is.
	{
		THEATER_SPEED_SCOPE(PaintTint);
		Paint.PaintTint(Data, Terrain, I0, J0, I1, J1);
	}
	RepaintBeyond(Changed);
	// (Speed, 17 November: only the box that was painted is sent, with its share of each smaller copy. The whole
	// world's pictures were being halved and sent again for one province: 144 ms on the press, 125 a frame later.)
	CampaignTextures::UpdateBoxAll(ReliefPicture, Paint.Relief, I0, J0, I1, J1); // (it carries how much wash each nation's ground needs)
	if (!bInk)
	{
		CampaignTextures::UpdateBoxAll(TintPicture, Paint.Tint, I0, J0, I1, J1);
		CampaignTextures::UpdateBoxAll(OccupiedPicture, Paint.Occupied, I0, J0, I1, J1);
		if (bLinesLater && bLandLaid)
		{
			BordersDue = GFrameCounter + 3; // (two frames after the front line's: its second weight lands in the frame between)
		}
		else
		{
			BuildBorders();
		}
		return;
	}
	for (const int32 Region : Taken)
	{
		FInkRun& Run = InkRuns.AddDefaulted_GetRef();
		Run.Region = Region;
		// (from the ground its new holder held already; failing that, from ground he took in the same moment;
		// failing that the painter starts it from the coast, or from the town)
		for (int32 Pass = 0; Pass < 2 && Run.From.Num() == 0; ++Pass)
		{
			for (const int32 Neighbour : Data.Regions[Region].Neighbors)
			{
				if (Data.Regions.IsValidIndex(Neighbour) && Data.Regions[Neighbour].Owner == Data.Regions[Region].Owner && (Pass == 1 || !TakenSet.Contains(Neighbour)))
				{
					Run.From.Add(Neighbour);
				}
			}
		}
		if (const float* Stood = StoodAt.Find(Region))
		{
			Run.Start = *Stood;
		}
	}
	InkBegan = Now;
	InkHeldRun = 0.0; // (ink begun under an order's result card stands at its very start: see HoldInk)
	{
		// (release 2, the front you can read: the player's own ink crosses in a second, release 1's, as the art director
		// ruled on 22 November after run 3, so that an order's ink is done before he clicks the next; the plan's second
		// and a half ran on under his next click. "-TheaterInkSeconds=1.5" runs it in that many seconds, for a picture.)
		static float AskedSeconds = 1.0f;
		static const bool bAskedSeconds = FParse::Value(FCommandLine::Get(), TEXT("TheaterInkSeconds="), AskedSeconds);
		InkSeconds = bAskedSeconds ? FMath::Clamp(AskedSeconds, 0.2f, 5.0f) : 1.0f;
		InkBeganFrame = GFrameCounter; // (in the month's replay the step shortens it to fit: see FitInk)
	}
	InkBox = FIntRect(I0, J0, I1, J1);
	bInkBorders = true;
	TintShown = Paint.Tint;
	OccupiedShown = Paint.Occupied;
	StepInk(0.0);
}

void ACampaignMapActor::StepInk(double Second)
{
	THEATER_SPEED_SCOPE(StepInk);
	if (InkRuns.Num() == 0 || TintBefore.Num() != Paint.Tint.Num())
	{
		return;
	}
	// (It takes a second to cross a province, quick at first and slowing; the frontier's band and the paint of the
	// ground beside it come to their new place in the last three tenths of it; the wet rim behind the front dries
	// in three tenths more.)
	// (release 2, the front you can read: the run takes InkSeconds, a second, "-TheaterInkSeconds=1.5" for the plan's
	// second and a half; the shares are worked out in Campaign/FrontLine.cpp, InkPhase, where they are tested)
	const TheaterFront::FInkPhase Phase = TheaterFront::InkPhase(Second, InkSeconds);
	const float Late = Phase.Late;
	const float Dry = Phase.Dry;
	auto Blend = [](const FColor& A, const FColor& B, float Share, float Darker)
	{
		return FColor(static_cast<uint8>(FMath::Lerp(static_cast<float>(A.R), static_cast<float>(B.R), Share) * Darker), static_cast<uint8>(FMath::Lerp(static_cast<float>(A.G), static_cast<float>(B.G), Share) * Darker),
			static_cast<uint8>(FMath::Lerp(static_cast<float>(A.B), static_cast<float>(B.B), Share) * Darker), 0);
	};
	// The ground beside: its band and its paint follow the frontier to its new place late in the second.
	TArray<int32> RunOf;
	RunOf.Init(INDEX_NONE, Data.Regions.Num());
	for (int32 K = 0; K < InkRuns.Num(); ++K)
	{
		RunOf[InkRuns[K].Region] = K;
	}
	TArray<const FCampaignGround::FFront*> FrontOfRun;
	TArray<float> ShareOfRun;
	for (const FInkRun& Run : InkRuns)
	{
		FrontOfRun.Add(&Paint.FrontOf(Data, Terrain, Run.Region, Run.From, false));
		// (share covered = 1 - (1 - t) squared; where a siege's colour stood already, it goes on from there)
		ShareOfRun.Add(FMath::Lerp(Run.Start, 1.0f, Phase.Share));
	}
	for (int32 J = InkBox.Min.Y; J <= InkBox.Max.Y; ++J)
	{
		for (int32 I = InkBox.Min.X; I <= InkBox.Max.X; ++I)
		{
			const int32 Idx = Terrain.Index(I, J);
			const FColor &Old = TintBefore[Idx], &New = Paint.Tint[Idx];
			const int32 Region = Terrain.Region[Idx];
			const int32 Run = Region >= 0 ? RunOf[Region] : INDEX_NONE;
			float Share = Late, Darker = 1.0f;
			if (Run != INDEX_NONE)
			{
				const float Behind = FCampaignGround::BehindFront(*FrontOfRun[Run], Terrain, I, J, ShareOfRun[Run]);
				Share = FMath::SmoothStep(-1.4f, 1.4f, Behind);
				// (the wet rim: the twelve pixels just behind the front are 15% darker, and dry to the plain stain. Never brighter.)
				const float Wet = FMath::SmoothStep(-1.4f, 0.6f, Behind) * (1.0f - FMath::SmoothStep(2.2f, 3.4f, Behind));
				Darker = 1.0f - 0.15f * Wet * (1.0f - Dry);
			}
			FColor Shown = Blend(Old, New, Share, Darker);
			Shown.A = static_cast<uint8>(FMath::Lerp(static_cast<float>(Old.A), static_cast<float>(New.A), Late));
			TintShown[Idx] = Shown;
			// (the stripes of occupied ground arrive with the front, and leave with it)
			const FColor &OldStripe = OccupiedBefore[Idx], &NewStripe = Paint.Occupied[Idx];
			FColor Stripe = Blend(OldStripe, NewStripe, Share, Darker);
			Stripe.A = static_cast<uint8>(FMath::Lerp(static_cast<float>(OldStripe.A), static_cast<float>(NewStripe.A), Share));
			OccupiedShown[Idx] = Stripe;
		}
	}
	CampaignTextures::UpdateBox(TintPicture, TintShown, InkBox.Min.X, InkBox.Min.Y, InkBox.Max.X, InkBox.Max.Y);
	CampaignTextures::UpdateBox(OccupiedPicture, OccupiedShown, InkBox.Min.X, InkBox.Min.Y, InkBox.Max.X, InkBox.Max.Y);
	if (bInkBorders && Phase.bBorders)
	{
		bInkBorders = false;
		BuildBorders(); // (the frontier's line goes to its new place as the ink arrives there)
	}
}

// ---- (release 2, the front you can read) ----
void ACampaignMapActor::HoldInk(bool bHold)
{
	// (Release 2, the art director's ruling of 22 November, after run 4: "The ink is the reward for his own attack, and
	// the map is the main game, so he must see it. Hold it at its start under the card, and start its 1.0 s as the scrim
	// lifts. Nothing changes in the month's replay." An attack's ink was begun as its result came, under the card's dark
	// scrim, and was over before he had read the card. The controller says, each frame, whether an order's result card
	// is up; only it holds the ink, so the replay, which shows no such card, runs as it did.)
	if (bHold == bInkHeldByCard)
	{
		return;
	}
	bInkHeldByCard = bHold;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : InkBegan;
	if (bHold)
	{
		// (how far the ink had run when the card came up: none for the order's own, begun in this same frame; a part
		// of its second for ink an earlier order began, which goes on from there and is never run again from its start:
		// the code checker's note on bca710c9)
		InkHeldRun = InkRuns.Num() > 0 ? FMath::Max(Now - InkBegan, 0.0) : 0.0;
	}
	else if (InkRuns.Num() > 0)
	{
		InkBegan = Now - InkHeldRun; // (the card is put away: the second goes on from where it stood)
	}
}

void ACampaignMapActor::SkipInk()
{
	// The player has skipped the month's watching: ink still running is laid where it is going, and the ground that
	// changes hands in this same frame (FinishMonth shows the month's end at once) takes its new colour at once.
	EndInk();
	InkSkipFrame = GFrameCounter;
}

void ACampaignMapActor::FitInk(double Seconds)
{
	// The month's replay shows each step for a moment (0.55 s for marches, 0.8 for others' fighting, 1.3 for the
	// player's own) and the next step that moves ground ends the ink still running (SetHolders, EndInk). Left at a
	// second and a half, a 0.8 s step was cut with a fifth of the province and the whole frontier band still to come,
	// and the frontier's line not yet moved. The ink this step began is fitted into its moment instead. (At the
	// second the player's own ink now takes, the fitted runs are the same: 0.425 s for a march, 0.675 for others'
	// fighting, and the whole second for his own, as the art director asked on 22 November.)
	if (InkRuns.Num() > 0 && InkBeganFrame == GFrameCounter)
	{
		InkSeconds = static_cast<float>(TheaterFront::InkFitted(InkSeconds, Seconds));
	}
}
// ---- (end release 2, the front you can read) ----

void ACampaignMapActor::EndInk()
{
	THEATER_SPEED_SCOPE(EndInk);
	if (InkRuns.Num() == 0)
	{
		return;
	}
	InkRuns.Reset();
	TintBefore.Reset();
	OccupiedBefore.Reset();
	TintShown.Reset();
	OccupiedShown.Reset();
	if (InkBox.Width() >= 0 && InkBox.Height() >= 0 && InkBox.Max.X > 0)
	{
		// (the ink ran inside the box its ground was painted in: that box is what is sent, with its smaller copies)
		CampaignTextures::UpdateBoxAll(TintPicture, Paint.Tint, InkBox.Min.X, InkBox.Min.Y, InkBox.Max.X, InkBox.Max.Y);
		CampaignTextures::UpdateBoxAll(OccupiedPicture, Paint.Occupied, InkBox.Min.X, InkBox.Min.Y, InkBox.Max.X, InkBox.Max.Y);
	}
	else
	{
		CampaignTextures::Update(TintPicture, Paint.Tint);
		CampaignTextures::Update(OccupiedPicture, Paint.Occupied);
	}
	if (bInkBorders)
	{
		bInkBorders = false;
		BuildBorders();
	}
}

void ACampaignMapActor::UpdateOwnershipPaint()
{
	THEATER_SPEED_SCOPE(UpdateOwnershipPaint);
	UWorld* World = GetWorld();
	if (!World || (InkRuns.Num() == 0 && !bSiegesMoving))
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (InkRuns.Num() > 0)
	{
		if (bInkHeldByCard)
		{
			return; // (it stands where SetHolders laid it, at its start, until the card is put away: see HoldInk)
		}
		// ("-TheaterInkAt=0.4" holds the ink at that point of its second, for a picture)
		static float HeldAt = -1.0f;
		static const bool bHeld = FParse::Value(FCommandLine::Get(), TEXT("TheaterInkAt="), HeldAt);
		const double Second = bHeld ? HeldAt : Now - InkBegan;
		if (!bHeld && TheaterFront::InkPhase(Second, InkSeconds).bOver) // (release 2: the length of the run is InkSeconds)
		{
			EndInk();
			return;
		}
		// (in eight steps across the second, as the brief allows: fifteen cost three or four frames of the second on a
		// machine with other work to do, and the front is soft enough that eight read as one movement)
		if (Now - PaintStepped >= 0.125)
		{
			PaintStepped = Now;
			StepInk(Second);
		}
		return;
	}
	if (Now - PaintStepped < 0.066)
	{
		return;
	}
	const float Passed = static_cast<float>(FMath::Clamp(Now - PaintStepped, 0.0, 0.2));
	PaintStepped = Now;
	FBox2D Moved(ForceInit);
	bSiegesMoving = false;
	for (auto It = SiegesShown.CreateIterator(); It; ++It)
	{
		FSiegeShown& Siege = It.Value();
		if (Siege.Depth == Siege.Target)
		{
			continue;
		}
		Siege.Depth = FMath::FInterpConstantTo(Siege.Depth, Siege.Target, Passed, Siege.Speed);
		if (Data.Regions.IsValidIndex(It.Key()) && Data.Regions[It.Key()].Bounds.bIsValid)
		{
			Moved += Data.Regions[It.Key()].Bounds;
		}
		if (Siege.Depth != Siege.Target)
		{
			bSiegesMoving = true;
		}
		else if (Siege.bLeaving)
		{
			It.RemoveCurrent();
		}
	}
	if (!Moved.bIsValid)
	{
		return;
	}
	const int32 I0 = FMath::Clamp(FMath::FloorToInt32((Moved.Min.X - 2.0) / Terrain.Cell), 0, Terrain.NX - 1), I1 = FMath::Clamp(FMath::CeilToInt32((Moved.Max.X + 2.0) / Terrain.Cell), 0, Terrain.NX - 1);
	const int32 J0 = FMath::Clamp(FMath::FloorToInt32((Moved.Min.Y - 2.0) / Terrain.Cell), 0, Terrain.NY - 1), J1 = FMath::Clamp(FMath::CeilToInt32((Moved.Max.Y + 2.0) / Terrain.Cell), 0, Terrain.NY - 1);
	LaySieges();
	Paint.PaintTint(Data, Terrain, I0, J0, I1, J1);
	if (bSiegesMoving)
	{
		CampaignTextures::UpdateBox(TintPicture, Paint.Tint, I0, J0, I1, J1);
		CampaignTextures::UpdateBox(OccupiedPicture, Paint.Occupied, I0, J0, I1, J1);
	}
	else
	{
		// (at rest: the box's share of the pictures' smaller copies is made again, for the far view)
		CampaignTextures::UpdateBoxAll(TintPicture, Paint.Tint, I0, J0, I1, J1);
		CampaignTextures::UpdateBoxAll(OccupiedPicture, Paint.Occupied, I0, J0, I1, J1);
		CampaignTextures::UpdateBoxAll(ReliefPicture, Paint.Relief, I0, J0, I1, J1);
	}
}

void ACampaignMapActor::SetReach(const TArray<FReachRegion>& Reach, bool bByRail, bool bKeepEnemyReach)
{
	THEATER_SPEED_SCOPE(SetReach);
	if (RailLight.Tracks.Num() + RailLight.Tint.Num() + RailLight.Bars.Num() + RailLight.Dots.Num() + RailLight.Ringed.Num() > 0)
	{
		SetRailLight(FRailLight()); // (the railway's light goes out with the reach it belongs to: whoever shows a reach lights it again)
	}
	bReachShown = Reach.Num() > 0;
	OrdersMesh->ClearMeshSection(0);
	// (The enemy's reach goes with the army's own; unless the ShowEnemyReach that replaces it comes a frame or two
	// later, when the old wash stands until then rather than blink out between the two: the checker's note on b414.)
	if (!bKeepEnemyReach)
	{
		OrdersMesh->ClearMeshSection(9);
		if (EnemyReachMesh)
		{
			EnemyReachMesh->ClearAllMeshSections();
		}
	}
	if (Reach.Num() == 0)
	{
		return;
	}
	// Ground the army can march to is lightened a little; ground it could attack is marked in red and ringed.
	FCampaignMeshBuffers Buffers;
	// Ground that can be reached by rail is tinted the blue of a timetable instead.
	const FLinearColor March = bByRail ? FLinearColor(0.62f, 0.8f, 1.0f, 0.2f) : FLinearColor(0.8f, 1.0f, 0.82f, 0.13f);
	const FLinearColor Attack(0.9f, 0.06f, 0.03f, 0.42f);
	const FColor AttackRing = FLinearColor(1.0f, 0.25f, 0.18f, 0.9f).ToFColor(false);
	// Ground across a frontier at peace is amber: the army can go there, but only by declaring war. (The art
	// director, 22 November: the fill came down from 0.5, which painted the ground, to about 0.2; the amber ring
	// carries it.)
	const FLinearColor War(1.0f, 0.55f, 0.0f, 0.2f);
	const FColor WarRing = FLinearColor(1.0f, 0.72f, 0.16f, 0.9f).ToFColor(false);
	const FColor Edge = FLinearColor(0.045f, 0.03f, 0.022f, 0.9f).ToFColor(false); // (the province lines of ground in reach, at full strength)
	for (const FReachRegion& Entry : Reach)
	{
		AppendRegionFill(Buffers, Entry.Region, Entry.IsWar() ? War : Entry.bAttack ? Attack : March);
		if (!Entry.bAttack && !Entry.IsWar() && Data.Regions.IsValidIndex(Entry.Region))
		{
			for (const TArray<FVector2D>& Ring : Data.Regions[Entry.Region].Polygons)
			{
				AppendRibbon(Buffers, Ring, 0.12, 0.2, Edge, true);
			}
		}
		if ((Entry.bAttack || Entry.IsWar()) && Data.Regions.IsValidIndex(Entry.Region))
		{
			for (const TArray<FVector2D>& Ring : Data.Regions[Entry.Region].Polygons)
			{
				AppendRibbon(Buffers, Ring, 0.34, 0.26, Entry.IsWar() ? WarRing : AttackRing, true);
			}
		}
	}
	Buffers.Commit(OrdersMesh, 0, WashInk());
}

void ACampaignMapActor::ShowEnemyReach(const TArray<int32>& Danger)
{
	THEATER_SPEED_SCOPE(ShowEnemyReach);
	OrdersMesh->ClearMeshSection(9); // (a section of its own: the route to the province under the pointer is drawn and cleared in section 1)
	if (EnemyReachMesh)
	{
		EnemyReachMesh->ClearAllMeshSections();
	}
	if (Danger.Num() == 0 || !Data.IsLoaded())
	{
		return;
	}
	// The ground the enemy could strike next month takes a faint lift, and where it ends inside the player's own
	// country, or an ally's or a neutral's, a dashed line is ruled: this deep, and no deeper. Along the front itself
	// nothing more is drawn: the front line is there already. (The art director's rulings: the dashes are amber,
	// about three points of the screen wide at working height and heavier from the staff map's height as the front
	// line is, with no barbs. The wash was a warm orange; the run 5 read of 22 November makes it a light lift toward
	// paper-100, lighter with the ground's own hue kept, so that the amber dashes are the only warm line on the map.
	// The warm wash had tinted the province an army stood in orange, out to the dashes, and read as the border of
	// that army's own march.)
	// (Release 2, the art director's read of run 7, 22 November: at Moscow and Cairo the lift round the army read as one
	// pale area with his march's lift, and nothing told the two apart. The reach lays no lift on the ground: it is the
	// amber dashes and the threat card's words; his march's lift is the only lift. Section 9 stays cleared.)
	const TSet<int32> In(Danger);

	if (!EnemyReachMesh)
	{
		EnemyReachMesh = NewObject<UProceduralMeshComponent>(this);
		EnemyReachMesh->SetupAttachment(Root);
		EnemyReachMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EnemyReachMesh->RegisterComponent();
		EnemyReachMesh->SetCastShadow(false);
	}
	FindSharedEdges();
	// (Release 2, the art director's read of run 2, 22 November: ruling 4. The ink line is a fact, who holds what; the
	// reach is a risk. Where the ground in reach is the player's own, its dashes are the kit's amber (ruling 166: a real
	// risk at a decision), at their full width.)
	// (Release 2, the art director's read of run 4, 22 November, citing her ruling 149(g): "No red: it is a risk, not a
	// dated warning. Use the same amber at about 60% and a little thinner." Round an ally's or a neutral's ground the
	// dashes are that amber at six tenths, the ink and its dark edging each four fifths as wide, at both weights. This
	// ink is opaque paint (M_MapSurface), whose alpha says what the paint is and never how strong, so the six tenths
	// are mixed into the colour: into the dash's own dark edging, which is what lies under the amber, as a see-through
	// amber over it would show. Toward the ground it would be a pale sand as bright as the full amber, and the ground
	// under a dash, a holder's stain on the living table or on the staff's paper, is not known here. The edging keeps
	// its strength: it holds the thinner dash off whatever ground it crosses.)
	const FColor Edging = FromHex(TEXT("2A1410")).ToFColor(true);
	const FColor Amber = FromHex(TEXT("E0A13C")).ToFColor(true);
	const FColor AmberFaint = FMath::Lerp(FromHex(TEXT("2A1410")), FromHex(TEXT("E0A13C")), 0.6f).ToFColor(true);
	FCampaignMeshBuffers Weights[2];
	static const double Heavier[2] = { 1.0, 2.7 };
	for (const FSharedEdge& Edge : SharedEdges)
	{
		if (In.Contains(Edge.Here) == In.Contains(Edge.There)
			|| WarFoes.Contains(Data.Regions[Edge.Here].Owner) || WarFoes.Contains(Data.Regions[Edge.There].Owner))
		{
			continue;
		}
		const double Length = FVector2D::Distance(Edge.A, Edge.B);
		const FVector2D Dir = (Edge.B - Edge.A) / Length;
		const int32 InReach = In.Contains(Edge.Here) ? Edge.Here : Edge.There;
		const bool bOwn = !WarPlayer.IsEmpty() && Data.Regions[InReach].Owner == WarPlayer;
		const FColor& DashInk = bOwn ? Amber : AmberFaint;
		const double Thin = bOwn ? 1.0 : 0.8; // (an ally's or a neutral's ground: four fifths as wide, the run past each end as it was)
		for (int32 Weight = 0; Weight < 2; ++Weight)
		{
			FCampaignMeshBuffers& Into = Weights[Weight];
			const double W = Heavier[Weight], Long = 1.7 * W, Gap = 1.1 * W;
			// (dashes of an even length, centred on the edge so that each edge begins and ends in a gap or a whole dash)
			const int32 Count = FMath::Max(1, FMath::FloorToInt32((Length + Gap) / (Long + Gap)));
			const double Start = (Length - (Count * Long + (Count - 1) * Gap)) * 0.5;
			for (int32 K = 0; K < Count; ++K)
			{
				const double From = FMath::Max(Start + K * (Long + Gap), 0.0), To = FMath::Min(From + Long, Length);
				auto Dash = [&](double HalfWidth, double Run, double Lift, const FColor& Ink)
				{
					const FVector2D P0 = Edge.A + Dir * (From - Run), P1 = Edge.A + Dir * (To + Run);
					const int32 A = Into.AddVertex(SurfacePointKept(P0.X + Edge.Side.X * HalfWidth, P0.Y + Edge.Side.Y * HalfWidth, Lift), FVector::UpVector, Ink);
					const int32 B = Into.AddVertex(SurfacePointKept(P0.X - Edge.Side.X * HalfWidth, P0.Y - Edge.Side.Y * HalfWidth, Lift), FVector::UpVector, Ink);
					const int32 C = Into.AddVertex(SurfacePointKept(P1.X + Edge.Side.X * HalfWidth, P1.Y + Edge.Side.Y * HalfWidth, Lift), FVector::UpVector, Ink);
					const int32 D = Into.AddVertex(SurfacePointKept(P1.X - Edge.Side.X * HalfWidth, P1.Y - Edge.Side.Y * HalfWidth, Lift), FVector::UpVector, Ink);
					Into.AddTriangleFacingUp(A, B, C);
					Into.AddTriangleFacingUp(B, C, D);
				};
				Dash(0.36 * W * Thin, 0.1 * W, 0.24, Edging);
				Dash(0.24 * W * Thin, 0.0, 0.28, DashInk);
			}
		}
	}
	Weights[0].Commit(EnemyReachMesh, 0, InkMaterial);
	Weights[1].Commit(EnemyReachMesh, 1, InkMaterial);
	const bool bFar = FrontLineFar == 1;
	EnemyReachMesh->SetMeshSectionVisible(0, !bFar);
	EnemyReachMesh->SetMeshSectionVisible(1, bFar);
}

float ACampaignMapActor::SnowLying(const FVector2D& P, float Hard)
{
	if (Hard <= 0.0f)
	{
		return 0.0f;
	}
	// The snow comes down from the north as the winter hardens, and farther south in the east, where the winter
	// is a continental one. Its edge is ragged, not ruled.
	const double Line = FMath::Lerp(120.0, 800.0, static_cast<double>(Hard)) + 0.22 * (P.X - 600.0);
	const double Ragged = 95.0 * FMath::PerlinNoise2D(P * 0.011) + 40.0 * FMath::PerlinNoise2D(P * 0.047 + FVector2D(13.0, 71.0));
	return 1.0f - static_cast<float>(FMath::SmoothStep(Line - 170.0, Line + 60.0, P.Y + Ragged));
}

void ACampaignMapActor::MakeSnowMasks(float Hard, TArray<FColor>& Pixels) const
{
	// (Release 2, the season turns: SetSeason's own paint of the snow, unchanged, made a function of its own so that
	// the turn can paint the month before's snow too, for a turn played again for a picture.)
	Pixels = Paint.Masks;
	if (Hard > 0.0f)
	{
		const int32 NX = Terrain.NX, NY = Terrain.NY;
		ParallelFor(NY, [&](int32 J)
		{
			for (int32 I = 0; I < NX; ++I)
			{
				const int32 Idx = Terrain.Index(I, J);
				if (Terrain.Coverage[Idx] < 0.4f)
				{
					continue; // the sea does not lie under snow
				}
				const FVector2D P(I * Terrain.Cell, J * Terrain.Cell);
				const float Lying = SnowLying(P, Hard);
				// It lies thinner than on the crests, so that each country's colour still shows through it: under
				// the stained paint a dusting is enough to turn the country white, so it is a thin one.
				const float Plain = 0.15f * FMath::Sqrt(Hard) * Lying;
				// And the snow line on the mountains comes down.
				const float H = Terrain.Height[Idx];
				const float High = 0.9f * FMath::SmoothStep(5.0f - 1.5f * Hard, 6.6f - 1.7f * Hard, H);
				const float Rock = Pixels[Idx].R / 255.0f;
				const float Snow = FMath::Max3(Pixels[Idx].G / 255.0f, Plain * (1.0f - 0.35f * Rock), High);
				Pixels[Idx].G = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Snow * 255.0f), 0, 255));
			}
		});
	}
}

void ACampaignMapActor::SetSeason(int32 Turn)
{
	THEATER_SPEED_SCOPE(SetSeason);
	// ("-TheaterSeason=5" on the command line holds the table at one month of the war, for a picture)
	int32 Held = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeason="), Held))
	{
		Turn = Held;
	}
	::UnitTable::SetTurn(Turn); // (some countries' marks change by date: the painter is told the month)
	const int32 Month = ((Turn + 7) % 12 + 12) % 12; // 0 is January; the war's first month is September
	if (Month == ShownMonth || !MasksPicture || Paint.Masks.Num() == 0 || Paint.Masks.Num() != Terrain.Height.Num())
	{
		return;
	}
	const bool bStepped = ShownMonth >= 0 && Month == (ShownMonth + 1) % 12; // (release 2, the season turns: the month has turned on by one in play)
	ShownMonth = Month;
	// How hard the winter lies in each month of the year.
	const float Hard = WinterHardInMonth(Month);
	NoteSeason(Month, Hard, WinterHardInMonth(Month + 11), bStepped);
	{
		// (The stripes of occupied ground are laid stronger where snow lies, so that they show as well in January as
		// in July: the painter is told how far the snow takes each province toward white, and occupied ground is
		// painted again when that has changed.)
		bool bSnowMoved = false;
		// (Release 2, map run 7: tc.Speed.MonthSpread. Where the snow's weight moved, the box of those provinces only;
		// a province with no bounds that moved sends the whole picture, as before.)
		FBox2D SnowBox(ForceInit);
		bool bSnowEverywhere = false;
		Paint.RegionSnow.SetNumZeroed(Data.Regions.Num());
		for (const FCampaignRegion& Region : Data.Regions)
		{
			// (about a third in deep winter, by the game's own January pictures: Germany over Poland, and at Kiev)
			// (where the snow's ragged edge crosses a province, the most that lies anywhere on it: its stripes are for its snowed half too)
			float Most = SnowLying(Region.Center, Hard);
			if (Region.Bounds.bIsValid && Hard > 0.0f)
			{
				const FVector2D Middle = Region.Bounds.GetCenter(), Reach = Region.Bounds.GetExtent() * 0.6;
				for (int32 K = 0; K < 4; ++K)
				{
					Most = FMath::Max(Most, SnowLying(Middle + FVector2D((K % 2 ? 1.0 : -1.0) * Reach.X, (K / 2 ? 1.0 : -1.0) * Reach.Y), Hard));
				}
			}
			const float Lies = 0.34f * FMath::Sqrt(Hard) * Most;
			const bool bMovedHere = !Region.Lawful.IsEmpty() && FMath::RoundToInt32(Lies * 20.0f) != FMath::RoundToInt32(Paint.RegionSnow[Region.Id] * 20.0f);
			bSnowMoved |= bMovedHere;
			if (bMovedHere)
			{
				if (Region.Bounds.bIsValid)
				{
					SnowBox += Region.Bounds;
				}
				else
				{
					bSnowEverywhere = true;
				}
			}
			Paint.RegionSnow[Region.Id] = Lies;
		}
		if (bSnowMoved && bLandLaid && TintPicture && OccupiedPicture && Paint.Tint.Num() == Paint.NX * Paint.NY)
		{
			THEATER_SPEED_SCOPE(SeasonSnowPaint);
			EndInk();
			if (TheaterSpeed::MonthSpread() && !bSnowEverywhere && SnowBox.bIsValid)
			{
				// (the margin ground changing hands is painted with: the band of stronger stain along a frontier)
				const double Margin = FCampaignGround::BandReach + 2.0;
				const int32 I0 = FMath::Clamp(FMath::FloorToInt32((SnowBox.Min.X - Margin) / Terrain.Cell), 0, Terrain.NX - 1);
				const int32 I1 = FMath::Clamp(FMath::CeilToInt32((SnowBox.Max.X + Margin) / Terrain.Cell), 0, Terrain.NX - 1);
				const int32 J0 = FMath::Clamp(FMath::FloorToInt32((SnowBox.Min.Y - Margin) / Terrain.Cell), 0, Terrain.NY - 1);
				const int32 J1 = FMath::Clamp(FMath::CeilToInt32((SnowBox.Max.Y + Margin) / Terrain.Cell), 0, Terrain.NY - 1);
				Paint.PaintTint(Data, Terrain, I0, J0, I1, J1);
				CampaignTextures::UpdateBoxAll(TintPicture, Paint.Tint, I0, J0, I1, J1);
				CampaignTextures::UpdateBoxAll(OccupiedPicture, Paint.Occupied, I0, J0, I1, J1);
				CampaignTextures::UpdateBoxAll(ReliefPicture, Paint.Relief, I0, J0, I1, J1);
			}
			else
			{
				Paint.PaintTint(Data, Terrain, 0, 0, Terrain.NX - 1, Terrain.NY - 1);
				CampaignTextures::Update(TintPicture, Paint.Tint);
				CampaignTextures::Update(OccupiedPicture, Paint.Occupied);
				CampaignTextures::Update(ReliefPicture, Paint.Relief);
			}
		}
	}
	// (tc.Speed.MonthSpread: the masks carry the snow and nothing else that changes, so with the winter as hard as when
	// they were last sent, as through the summer, they are not sent again.)
	if (!TheaterSpeed::MonthSpread() || Hard != MasksSentFor)
	{
		THEATER_SPEED_SCOPE(SeasonMasks);
		MasksSentFor = Hard;
		TArray<FColor> Pixels;
		MakeSnowMasks(Hard, Pixels);
		// (Release 2, the season turns: with tc.Map.SeasonTurn the month's new snow is handed to the turn, which spreads
		// it over the land in the frames after this one, in place of sending it now.)
		if (!TakeSeasonSnow(Pixels))
		{
			SendMasks(Pixels); // (release 2: with churned ground on, laid over them as they go; else the plain send)
		}
	}
	// The year turns on the table. The woods: fresh green in spring, gold and russet through the autumn, bare in
	// winter where they are not fir. The cloth of the woods seen from far off takes the same turn.
	{
		static const int32 Looks[12] = { 4, 4, 5, 6, 0, 0, 0, 0, 1, 2, 3, 4 };
		static const FLinearColor Cloth[7] =
		{
			// (a dark cloth: a wood is darker than the fields beside it, whatever the nation's colour)
			// (the art director's canopy: about (52, 70, 52) as it is seen at working height, cool and dark)
			FLinearColor(0.017f, 0.047f, 0.03f), FLinearColor(0.021f, 0.047f, 0.029f), FLinearColor(0.028f, 0.047f, 0.027f), FLinearColor(0.03f, 0.04f, 0.028f),
			FLinearColor(0.029f, 0.036f, 0.031f), FLinearColor(0.026f, 0.042f, 0.03f), FLinearColor(0.021f, 0.054f, 0.03f)
		};
		const bool bTrees = Looks[Month] != TreeLook || FMath::Abs(Hard - WinterHard) > 0.3f;
		TreeLook = Looks[Month];
		WinterHard = Hard;
		if (LandMaterial)
		{
			LandVector(TEXT("WoodsColour"), Cloth[TreeLook]);
		}
		if (bTrees && TreeMeshes.Num() > 0)
		{
			THEATER_SPEED_SCOPE(SeasonTrees);
			if (TheaterSpeed::TreesSpread() > 0.0 && TreeSquareOf.Num() > 0)
			{
				// (Release 2, map run 8: tc.Speed.TreesSpread. Planted again a few squares a frame, those in sight
				// first, in place of every square in the month's frame: PlantTreesDue.)
				TreesDue.Reset(TreeSquareOf.Num());
				for (int32 K = 0; K < TreeSquareOf.Num(); ++K)
				{
					TreesDue.Add(K);
				}
				UE_LOG(LogTheater, Log, TEXT("The woods turn for the month over the next frames: %d squares"), TreesDue.Num());
			}
			else
			{
				const double Began = FPlatformTime::Seconds();
				BuildTrees();
				UE_LOG(LogTheater, Log, TEXT("The woods turned for the month in %.2f s"), FPlatformTime::Seconds() - Began);
			}
		}
		// The rivers of the east freeze where the snow lies, and run again in the thaw.
		if (FMath::Abs(Hard - RiversDrawnFor) > 0.01f && RiverMesh)
		{
			THEATER_SPEED_SCOPE(SeasonRivers);
			BuildWaterways();
		}
	}
	// The light of the season: colder and a little weaker in winter, warm in high summer.
	static const float Summer[12] = { 0.0f, 0.0f, 0.1f, 0.3f, 0.6f, 0.9f, 1.0f, 0.95f, 0.6f, 0.3f, 0.1f, 0.0f };
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ACampaignEnvironment> It(World); It; ++It)
		{
			It->SetSeason(Hard, Summer[Month]);
		}
	}
}

void ACampaignMapActor::SetTowns(const TArray<FTownInfo>& InTowns)
{
	THEATER_SPEED_SCOPE(SetTowns);
	Towns.Reset();
	Towns.SetNum(Data.Regions.Num());
	for (const FTownInfo& Town : InTowns)
	{
		if (Towns.IsValidIndex(Town.Region))
		{
			Towns[Town.Region] = Town;
		}
	}
	BuildLabels();
	bMiniStale = true;
}

void ACampaignMapActor::ShowWealth(bool bOn)
{
	THEATER_SPEED_SCOPE(ShowWealth); // (named in the long-frame lines, as ShowSides is: the Y key's cost)
	bWealth = bOn && Towns.Num() > 0;
	bMiniStale = true;
	OrdersMesh->ClearMeshSection(4);
	if (!bWealth)
	{
		return;
	}
	// Every province washed with gold, the richer the deeper, so the prizes stand out of the map.
	static const float Wash[6] = { 0.0f, 0.1f, 0.2f, 0.32f, 0.46f, 0.62f };
	FCampaignMeshBuffers Buffers;
	for (const FTownInfo& Town : Towns)
	{
		const int32 Rank = FMath::Clamp(Town.Rank, 0, 5);
		if (Rank > 0 && Data.Regions.IsValidIndex(Town.Region))
		{
			AppendRegionFill(Buffers, Town.Region, FLinearColor(1.0f, 0.78f, 0.12f, Wash[Rank]));
		}
	}
	Buffers.Commit(OrdersMesh, 4, HighlightMaterial);
}

namespace
{
	/** The sides view's wash for a province, by how the player stands to its holder. */
	FLinearColor SideWash(const FString& Side)
	{
		if (Side == TEXT("enemy"))
		{
			return FLinearColor(0.95f, 0.08f, 0.05f, 0.42f);
		}
		if (Side == TEXT("ally"))
		{
			return FLinearColor(0.15f, 0.5f, 1.0f, 0.38f);
		}
		if (Side == TEXT("own"))
		{
			return FLinearColor(0.3f, 1.0f, 0.45f, 0.2f);
		}
		// A country at peace with the player is greyed, so that a red country at peace is not taken for an enemy.
		return FLinearColor(0.36f, 0.36f, 0.34f, 0.6f);
	}
}

void ACampaignMapActor::ShowSides(bool bOn, const TArray<FString>& Standing)
{
	THEATER_SPEED_SCOPE(ShowSides);
	if (bOn == bSides && Standing == SideOf)
	{
		return;
	}
	bSides = bOn;
	SideOf = Standing;
	bMiniStale = true;
	if (TheaterSpeed::SidesKept())
	{
		if (const FProcMeshSection* Old = OrdersMesh->GetProcMeshSection(5); Old && Old->ProcVertexBuffer.Num() > 0)
		{
			OrdersMesh->ClearMeshSection(5); // (laid there with the switch off)
		}
		LaySidesKept(false);
		return;
	}
	if (SidesMesh && SidesMesh->IsVisible())
	{
		SidesMesh->SetVisibility(false); // (kept there with the switch on)
		SidesShown = 0.0f;
	}
	OrdersMesh->ClearMeshSection(5);
	if (!bSides)
	{
		return;
	}
	FCampaignMeshBuffers Buffers;
	for (int32 Id = 0; Id < Data.Regions.Num() && Id < SideOf.Num(); ++Id)
	{
		AppendRegionFill(Buffers, Id, SideWash(SideOf[Id]));
	}
	Buffers.Commit(OrdersMesh, 5, HighlightMaterial);
}

void ACampaignMapActor::WarmSides(const TArray<FString>& Standing)
{
	if (!TheaterSpeed::SidesKept() || bSides || !Data.IsLoaded())
	{
		return;
	}
	THEATER_SPEED_SCOPE(WarmSides);
	SideOf = Standing;
	LaySidesKept(true);
}

void ACampaignMapActor::LaySidesKept(bool bWarmOnly)
{
	// (Release 2, map run 7: tc.Speed.SidesKept. The Y key's third view laid the wash afresh over every province of the
	// world on each press, 211 ms. It is laid once, on a mesh of its own, and kept while the view is put away; shown
	// again, only the provinces whose side has changed are coloured afresh, in place, rim and all. The same wash either way.)
	if (!bSides && !bWarmOnly)
	{
		// (tc.Speed.SidesFade: put away, it fades out over the next frames and is hidden at nought, UpdateSidesFade)
		if (SidesMesh && SidesMesh->IsVisible() && !(SidesInk && TheaterSpeed::SidesFade() > 0.0f))
		{
			SidesMesh->SetVisibility(false);
			SidesShown = 0.0f;
		}
		return;
	}
	if (!SidesMesh)
	{
		SidesMesh = NewObject<UProceduralMeshComponent>(this, TEXT("SidesKept"));
		SidesMesh->SetupAttachment(Root);
		SidesMesh->bUseAsyncCooking = false;
		SidesMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SidesMesh->SetCastShadow(false);
		// (A mesh of its own is sorted against the orders' washes by distance, which could swap them as the camera
		// moves. It is held under them all, the reach, the routes and the pointer, as the ground's tint it is, and under
		// the firelight of burning towns (-1), which would otherwise swap with it and flare as the camera passed.)
		SidesMesh->SetTranslucentSortPriority(-2);
		SidesMesh->SetVisibility(false);
		SidesMesh->RegisterComponent();
		// (its own ink, the same M_Highlight, so that its "Opacity" can fade it without fading the orders' washes)
		if (HighlightMaterial)
		{
			SidesInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
			if (SidesInk)
			{
				SidesInk->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
			}
		}
		SidesShown = 0.0f;
	}
	FProcMeshSection* Laid = SidesMesh->GetProcMeshSection(0);
	if (!Laid || Laid->ProcVertexBuffer.Num() == 0 || Laid->ProcVertexBuffer.Num() != SidesPointRegion.Num()
		|| SidesColouredFor.Num() != SideOf.Num())
	{
		FCampaignMeshBuffers Buffers;
		SidesPointRegion.Reset();
		for (int32 Id = 0; Id < Data.Regions.Num() && Id < SideOf.Num(); ++Id)
		{
			const int32 Before = Buffers.Vertices.Num();
			AppendRegionFill(Buffers, Id, SideWash(SideOf[Id]));
			for (int32 K = Before; K < Buffers.Vertices.Num(); ++K)
			{
				SidesPointRegion.Add(Id);
			}
		}
		if (!Buffers.Commit(SidesMesh, 0, SidesInk ? static_cast<UMaterialInterface*>(SidesInk) : HighlightMaterial.Get()))
		{
			// (nothing laid: an old wash left on the mesh is not shown for the new sides)
			SidesPointRegion.Reset();
			SidesColouredFor.Reset();
			SidesMesh->ClearMeshSection(0);
			SidesMesh->SetVisibility(false);
			SidesShown = 0.0f;
			return;
		}
		SidesColouredFor = SideOf;
	}
	else if (SidesColouredFor != SideOf)
	{
		// (each point is its province's alone: AppendRegionFill lays every province's points afresh. A point the wash
		// does not reach, off the province or on its sea, keeps its nought.)
		// (which provinces changed side, and their new ink, are worked out once a province, not once a point)
		TBitArray<> Changed(false, SideOf.Num());
		TArray<FColor> Ink;
		Ink.SetNumZeroed(SideOf.Num());
		for (int32 Id = 0; Id < SideOf.Num(); ++Id)
		{
			if (SideOf[Id] != SidesColouredFor[Id])
			{
				Changed[Id] = true;
				Ink[Id] = SideWash(SideOf[Id]).ToFColor(false);
			}
		}
		for (int32 K = 0; K < Laid->ProcVertexBuffer.Num(); ++K)
		{
			const int32 Id = SidesPointRegion[K];
			FColor& Point = Laid->ProcVertexBuffer[K].Color;
			if (!Changed.IsValidIndex(Id) || !Changed[Id])
			{
				continue;
			}
			// (a point off the province, or on its sea, takes the new colour still at nought, as a fresh lay leaves it:
			// the colour fades across the rim toward it, and an old colour kept there showed as a seam of the old side)
			const uint8 WasSeen = Point.A;
			Point = Ink[Id];
			if (WasSeen == 0)
			{
				Point.A = 0;
			}
		}
		SidesColouredFor = SideOf;
		SidesMesh->MarkRenderStateDirty();
	}
	if (!bWarmOnly && !SidesMesh->IsVisible())
	{
		// (shown from nought, faded in by UpdateSidesFade; with no fade, at once and whole)
		SidesShown = SidesInk && TheaterSpeed::SidesFade() > 0.0f ? 0.0f : 1.0f;
		if (SidesInk)
		{
			SidesInk->SetScalarParameterValue(TEXT("Opacity"), SidesShown);
		}
		SidesMesh->SetVisibility(true);
	}
}

void ACampaignMapActor::PlantTreesDue(const FVector& CameraFocus)
{
	// (Release 2, map run 8: tc.Speed.TreesSpread. The woods' turn for the month took about 155 ms in the month's frame,
	// every square of country planted again. Each square is planted again on its own here, in its place and shown or put
	// away as it was, a few a frame: the squares in sight first, then the nearest to the eye. A square out of sight is
	// not drawn, so it can wait. How many go in a frame follows what a square has been costing.)
	if (TreesDue.Num() == 0)
	{
		return;
	}
	THEATER_SPEED_SCOPE(SeasonTreesDue);
	TreesDue.Sort([this, &CameraFocus](int32 A, int32 B)
	{
		const bool bSeenA = TreesShown.IsValidIndex(A) && TreesShown[A], bSeenB = TreesShown.IsValidIndex(B) && TreesShown[B];
		if (bSeenA != bSeenB)
		{
			return bSeenA;
		}
		const double FarA = TreeCentres.IsValidIndex(A) ? FVector::DistSquared2D(TreeCentres[A], CameraFocus) : 1.0e30;
		const double FarB = TreeCentres.IsValidIndex(B) ? FVector::DistSquared2D(TreeCentres[B], CameraFocus) : 1.0e30;
		return FarA < FarB;
	});
	const double FrameMs = TheaterSpeed::TreesSpread();
	const int32 Picked = FrameMs > 0.0 ? FMath::Clamp(FMath::FloorToInt32(FrameMs / FMath::Max(static_cast<double>(TreeSquareMs), 0.02)), 1, TreesDue.Num()) : TreesDue.Num();
	TSet<FIntPoint> Squares;
	for (int32 K = 0; K < Picked; ++K)
	{
		if (TreeSquareOf.IsValidIndex(TreesDue[K]))
		{
			Squares.Add(TreeSquareOf[TreesDue[K]]);
		}
	}
	TreesDue.RemoveAt(0, Picked);
	const double Started = FPlatformTime::Seconds();
	BuildTrees(&Squares);
	const double TookMs = (FPlatformTime::Seconds() - Started) * 1000.0;
	TreeSquareMs = 0.5f * TreeSquareMs + 0.5f * static_cast<float>(TookMs / FMath::Max(Picked, 1));
	if (TreesDue.Num() == 0)
	{
		UE_LOG(LogTheater, Log, TEXT("The woods have turned for the month (a square costing about %.2f ms)"), TreeSquareMs);
	}
}

void ACampaignMapActor::UpdateSidesFade(float DeltaSeconds)
{
	// (tc.Speed.SidesFade. The kept sides view comes up and goes away as a fade, not a jump: the art director's cover for
	// the Y key's step. A long frame counts as a thirtieth of a second, so that a stall on the press does not use the
	// fade up: the wash still rises over the frames after it.)
	if (!SidesMesh || !SidesInk || !SidesMesh->IsVisible())
	{
		return;
	}
	const float Goal = bSides ? 1.0f : 0.0f;
	if (SidesShown != Goal)
	{
		const float Lasts = TheaterSpeed::SidesFade();
		const float Step = Lasts > 0.0f ? FMath::Min(DeltaSeconds, 1.0f / 30.0f) / Lasts : 1.0f;
		SidesShown = bSides ? FMath::Min(SidesShown + Step, 1.0f) : FMath::Max(SidesShown - Step, 0.0f);
		SidesInk->SetScalarParameterValue(TEXT("Opacity"), FMath::SmoothStep(0.0f, 1.0f, SidesShown));
	}
	if (!bSides && SidesShown <= 0.0f)
	{
		SidesMesh->SetVisibility(false);
	}
}

void ACampaignMapActor::UpdateFlashes()
{
	THEATER_SPEED_SCOPE(UpdateFlashes);
	UWorld* World = GetWorld();
	if (!World || (Flashes.Num() == 0 && FlashesDrawn <= 0.0))
	{
		return;
	}
	const double Now = World->GetTimeSeconds(), Lasts = 2.2;
	Flashes.RemoveAll([Now, Lasts](const TPair<int32, double>& Flash) { return Now - Flash.Value > Lasts; });
	if (Flashes.Num() == 0)
	{
		OrdersMesh->ClearMeshSection(8);
		FlashesDrawn = 0.0;
		return;
	}
	// (drawn afresh about twenty times a second: often enough to fade smoothly)
	if (Now - FlashesDrawn < 0.05)
	{
		return;
	}
	FlashesDrawn = Now;
	FCampaignMeshBuffers Buffers;
	for (const TPair<int32, double>& Flash : Flashes)
	{
		// A bright wash that comes up at once and dies away, with a second, fainter beat as it goes.
		const double T = (Now - Flash.Value) / Lasts;
		const float Beat = static_cast<float>(FMath::Square(1.0 - T) * (0.75 + 0.25 * FMath::Cos(T * PI * 4.0)));
		// It is the new holder's own colour that sweeps over the ground, bright at first, and settles into the paint.
		const FCampaignNation* Holder = Data.Regions.IsValidIndex(Flash.Key) ? Data.FindNation(Data.Regions[Flash.Key].Owner) : nullptr;
		const FLinearColor Sweep = Holder ? FMath::Lerp(FLinearColor(Holder->Color), FLinearColor(0.75f, 0.7f, 0.55f), 0.35f) : FLinearColor(0.62f, 0.56f, 0.4f);
		AppendRegionFill(Buffers, Flash.Key, FLinearColor(Sweep.R, Sweep.G, Sweep.B, 0.8f * Beat));
	}
	Buffers.Commit(OrdersMesh, 8, HighlightMaterial);
}

void ACampaignMapActor::ShowMud(const TArray<int32>& Regions)
{
	THEATER_SPEED_SCOPE(ShowMud);
	if (Regions == ShownMud)
	{
		return;
	}
	ShownMud = Regions;
	OrdersMesh->ClearMeshSection(7);
	// (Release 2, the season turns, tc.Map.SeasonTurn: the mud painted into the ground itself, soaking out from the
	// rivers and the low ground and stopping inside the rules' ground; Campaign/CampaignSeasonTurn.cpp.)
	if (AskSeasonMud(Regions))
	{
		return;
	}
	if (Regions.Num() == 0)
	{
		return;
	}
	// The roads are rivers of mud: the ground is washed a wet brown.
	FCampaignMeshBuffers Buffers;
	for (int32 RegionId : Regions)
	{
		if (Data.Regions.IsValidIndex(RegionId))
		{
			AppendRegionFill(Buffers, RegionId, FLinearColor(0.075f, 0.045f, 0.02f, 0.46f)); // (the glow's material brightens what it is given: this comes out a dark wet brown)
		}
	}
	Buffers.Commit(OrdersMesh, 7, HighlightMaterial);
}

void ACampaignMapActor::UpdateClouds()
{
	THEATER_SPEED_SCOPE(UpdateClouds);
	UWorld* World = GetWorld();
	if (!CloudMesh || !World || !HighlightMaterial || !Data.IsLoaded())
	{
		return;
	}
	// (Ruling 127: once the weather is known, the shadows belong to it. A clear sky has none, and a sky in cloud has
	// its own, in the land's own paint. These drifting patches are for a table with no weather.)
	if (CloudMesh->IsVisible() == bSkyKnown)
	{
		CloudMesh->SetVisibility(!bSkyKnown);
	}
	if (bSkyKnown)
	{
		return;
	}
	// The shadows of clouds, drifting slowly eastward over the map: a dozen soft patches of shade, each fading to
	// nothing at its edge and as it comes to the edge of the map.
	// Each is a middle of even shade with a soft edge round it: a point at the centre, a ring where the shade begins to
	// thin, and a ring where it is gone.
	const int32 Clouds = 12, Rim = 22, Per = 1 + 2 * Rim;
	if (CloudVertices.Num() == 0)
	{
		CloudVertices.Init(FVector::ZeroVector, Clouds * Per);
		CloudVertices[1] = FVector(-20000.0, -20000.0, 0.0);
		CloudVertices[2] = FVector(20000.0, 20000.0, 2000.0);
		CloudColours.Init(FColor(0, 0, 0, 0), Clouds * Per);
		TArray<int32> Triangles;
		for (int32 Cloud = 0; Cloud < Clouds; ++Cloud)
		{
			const int32 First = Cloud * Per;
			for (int32 K = 0; K < Rim; ++K)
			{
				const int32 In0 = First + 1 + K, In1 = First + 1 + (K + 1) % Rim, Out0 = In0 + Rim, Out1 = In1 + Rim;
				// (both faces of every triangle, so that it shows whichever way it is wound to the eye)
				Triangles.Append({ First, In1, In0, First, In0, In1 });
				Triangles.Append({ In0, In1, Out0, In0, Out0, In1 });
				Triangles.Append({ In1, Out1, Out0, In1, Out0, Out1 });
			}
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, CloudVertices.Num());
		CloudMesh->CreateMeshSection(0, CloudVertices, Triangles, Normals, TArray<FVector2D>(), CloudColours, TArray<FProcMeshTangent>(), false);
		CloudMesh->SetMaterial(0, HighlightMaterial);
		CloudMesh->SetCastShadow(false);
	}
	const double Now = World->GetTimeSeconds();
	const double Wide = Data.Width, High = Data.Height, Span = Wide * 1.5;
	// ("-TheaterClouds=0.6" on the command line makes them darker, to see where they lie)
	// (The art director's land and light brief, 16 November 2026: close up the shadows were the heaviest thing on the
	// screen. A clear day is to have none, and they will belong to a sky in cloud; until the weather is in the game
	// they are half as dark as they were.)
	static const float Depth = []() { float Asked = 0.085f; FParse::Value(FCommandLine::Get(), TEXT("TheaterClouds="), Asked); return FMath::Clamp(Asked, 0.0f, 0.9f); }();
	const double Lift = 9.0; // above the highest ground, so that the shade falls on the land and on the pieces alike
	for (int32 Cloud = 0; Cloud < Clouds; ++Cloud)
	{
		FRandomStream Random(Cloud * 7919 + 13);
		const double Size = Random.FRandRange(80.0f, 170.0f);
		const double Speed = Random.FRandRange(2.2f, 4.0f); // map units a second: slow enough to be felt more than seen
		const double X = FMath::Fmod(Random.FRandRange(0.0f, static_cast<float>(Span)) + Now * Speed, Span) - Wide * 0.25;
		const double Y = Random.FRandRange(0.0f, static_cast<float>(High));
		const double Squash = Random.FRandRange(0.55f, 0.85f);
		// Fading in and out at the edges of the map, so that none falls on the table round it.
		const double Edge = FMath::Min(FMath::Min(X, Wide - X), FMath::Min(Y, High - Y));
		const uint8 Shade = static_cast<uint8>(FMath::Clamp(255.0 * Depth * FMath::SmoothStep(Size * 0.1, Size * 1.1, Edge), 0.0, 255.0));
		const int32 First = Cloud * Per;
		CloudVertices[First] = MapToWorld(X, Y, Lift);
		CloudColours[First] = FColor(4, 6, 12, Shade);
		for (int32 K = 0; K < Rim; ++K)
		{
			const double Angle = 2.0 * PI * K / Rim;
			// A ragged outline, the same for each cloud every frame.
			const double Reach = Size * (0.78 + 0.22 * FMath::Sin(Angle * 3.0 + Cloud) + 0.1 * FMath::Sin(Angle * 7.0 + Cloud * 2.3));
			const FVector2D Out(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach * Squash);
			CloudVertices[First + 1 + K] = MapToWorld(X + Out.X * 0.55, Y + Out.Y * 0.55, Lift);
			CloudColours[First + 1 + K] = FColor(4, 6, 12, Shade);
			CloudVertices[First + 1 + Rim + K] = MapToWorld(X + Out.X, Y + Out.Y, Lift);
			CloudColours[First + 1 + Rim + K] = FColor(4, 6, 12, 0);
		}
	}
	CloudMesh->UpdateMeshSection(0, CloudVertices, TArray<FVector>(), TArray<FVector2D>(), CloudColours, TArray<FProcMeshTangent>());
}

void ACampaignMapActor::ShowShroud(const TArray<int32>& Unseen, const TArray<int32>& Glimpsed)
{
	THEATER_SPEED_SCOPE(ShowShroud);
	if (Unseen == ShownUnseen && Glimpsed == ShownGlimpsed)
	{
		return;
	}
	ShownUnseen = Unseen;
	ShownGlimpsed = Glimpsed;
	UnseenSet = TSet<int32>(Unseen);
	GlimpsedSet = TSet<int32>(Glimpsed);
	HazeSet = UnseenSet.Union(GlimpsedSet); // (ruling 154: the haze stays over ground he only glimpses, at the same depth)
	bMiniStale = true;
	if (RiverMesh && Land.Rivers.Num() > 0 && (RiversHazedFor.Num() != HazeSet.Num() || !RiversHazedFor.Includes(HazeSet)))
	{
		// (a river is darker under the haze: where the haze has moved, the rivers are drawn again. Speed, 18 November
		// 2026, tc.Speed.RiverInks: their inks are changed in place, the same inks, 0.12 s on the wider chart before.)
		const double Began = FPlatformTime::Seconds();
		const bool bInPlace = TheaterSpeed::RiverInks() && RecolourRivers();
		if (!bInPlace)
		{
			BuildWaterways();
		}
		UE_LOG(LogTheater, Log, TEXT("The rivers %s under the haze in %.3f s"), bInPlace ? TEXT("inked again") : TEXT("drawn again"), FPlatformTime::Seconds() - Began);
	}
	OrdersMesh->ClearMeshSection(6);
	// Ground nobody of the player's can see is the staff's own paper map (art fix 11): the land's material draws
	// it so, and what stands on it is drawn rather than coloured, wherever this picture says. A pixel to each
	// point of the terrain, smoothed by the picture itself so that the paper has no hard edge.
	if (!FogPicture || Terrain.Region.Num() != Paint.NX * Paint.NY)
	{
		return;
	}
	TArray<FColor> Pixels;
	Pixels.Init(FColor(0, 0, 0, 255), Paint.NX * Paint.NY);
	if (HazeSet.Num() > 0)
	{
		TArray<bool> Hidden;
		Hidden.Init(false, Data.Regions.Num());
		for (const int32 RegionId : HazeSet)
		{
			if (Hidden.IsValidIndex(RegionId))
			{
				Hidden[RegionId] = true;
			}
		}
		for (int32 Index = 0; Index < Pixels.Num(); ++Index)
		{
			// (a point on the shore belongs to the province it is the shore of, as it does for the nations' paint)
			const int32 Id = Paint.PaintedRegion(Index, Terrain);
			if (Id >= 0 && Hidden[Id])
			{
				Pixels[Index].R = 255;
			}
		}
	}
	SoftenFog(Pixels, Paint.NX, Paint.NY, Terrain.Cell);
	if (!bFogLaidOnce || FogShownPixels.Num() != Pixels.Num())
	{
		// (a campaign begun or taken up lays its haze at once)
		bFogLaidOnce = true;
		bFogFading = false;
		FogShownPixels = Pixels;
		FogWantedPixels = Pixels;
		CampaignTextures::Update(FogPicture, Pixels);
	}
	else
	{
		// After that it comes and goes over a moment: the picture is walked toward what it should be, a little each
		// frame, inside the box of what has changed (UpdateFogFade).
		FogWantedPixels = MoveTemp(Pixels);
		FogFadeBox = FIntRect(Paint.NX, Paint.NY, -1, -1);
		for (int32 J = 0; J < Paint.NY; ++J)
		{
			const FColor* Shown = &FogShownPixels[J * Paint.NX];
			const FColor* Wanted = &FogWantedPixels[J * Paint.NX];
			for (int32 I = 0; I < Paint.NX; ++I)
			{
				if (Shown[I].R != Wanted[I].R)
				{
					FogFadeBox.Min.X = FMath::Min(FogFadeBox.Min.X, I);
					FogFadeBox.Min.Y = FMath::Min(FogFadeBox.Min.Y, J);
					FogFadeBox.Max.X = FMath::Max(FogFadeBox.Max.X, I);
					FogFadeBox.Max.Y = FMath::Max(FogFadeBox.Max.Y, J);
				}
			}
		}
		bFogFading = FogFadeBox.Max.X >= FogFadeBox.Min.X;
	}
	FogBeyond();
}

void ACampaignMapActor::UpdateFogFade(float DeltaSeconds)
{
	THEATER_SPEED_SCOPE(UpdateFogFade);
	if (!bFogFading || !FogPicture || FogShownPixels.Num() != Paint.NX * Paint.NY || FogWantedPixels.Num() != FogShownPixels.Num())
	{
		return;
	}
	// (thinning over 0.6 seconds where ground comes into view; returning over a second where it goes out)
	const int32 Thins = FMath::Max(1, FMath::RoundToInt32(255.0f * DeltaSeconds / 0.6f)), Returns = FMath::Max(1, FMath::RoundToInt32(255.0f * DeltaSeconds / 1.0f));
	bool bMore = false;
	for (int32 J = FogFadeBox.Min.Y; J <= FogFadeBox.Max.Y; ++J)
	{
		FColor* Shown = &FogShownPixels[J * Paint.NX];
		const FColor* Wanted = &FogWantedPixels[J * Paint.NX];
		for (int32 I = FogFadeBox.Min.X; I <= FogFadeBox.Max.X; ++I)
		{
			const int32 Now = Shown[I].R, Aim = Wanted[I].R;
			if (Now != Aim)
			{
				const int32 Next = Now > Aim ? FMath::Max(Aim, Now - Thins) : FMath::Min(Aim, Now + Returns);
				Shown[I].R = static_cast<uint8>(Next);
				bMore |= Next != Aim;
			}
		}
	}
	if (bMore)
	{
		CampaignTextures::UpdateBox(FogPicture, FogShownPixels, FogFadeBox.Min.X, FogFadeBox.Min.Y, FogFadeBox.Max.X, FogFadeBox.Max.Y);
	}
	else
	{
		bFogFading = false;
		CampaignTextures::Update(FogPicture, FogShownPixels); // (the whole picture, with its smaller copies, now that it has arrived)
	}
}

void ACampaignMapActor::ShowFreeCapitals(const TArray<int32>& Capitals)
{
	THEATER_SPEED_SCOPE(ShowFreeCapitals);
	if (Capitals == ShownFree)
	{
		return;
	}
	ShownFree = Capitals;
	OrdersMesh->ClearMeshSection(3);
	if (Capitals.Num() == 0)
	{
		return;
	}
	FCampaignMeshBuffers Buffers;
	for (int32 Capital : Capitals)
	{
		if (!Data.Regions.IsValidIndex(Capital))
		{
			continue;
		}
		// Two rings of gold round the town: a country waits here to be set free.
		for (const double Radius : { 6.5, 8.0 })
		{
			TArray<FVector2D> Round;
			const int32 Sides = 36;
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const double Angle = 2.0 * PI * Side / Sides;
				Round.Add(Data.Regions[Capital].Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
			}
			AppendRibbon(Buffers, Round, Radius > 7.0 ? 0.3 : 0.45, 0.32, FLinearColor(1.0f, 0.8f, 0.2f, 0.95f).ToFColor(false), true);
		}
	}
	Buffers.Commit(OrdersMesh, 3, HighlightMaterial);
}

void ACampaignMapActor::ShowSeaReach(const TArray<int32>& Zones, int32 Hover, int32 FleetId)
{
	OrdersMesh->ClearMeshSection(2);
	OrdersMesh->ClearMeshSection(10);
	if (Zones.Num() == 0)
	{
		return;
	}
	FCampaignMeshBuffers Buffers, Fills;
	// (The art director's ruling of round 19: the line carries the mark and the fill stays faint. The water inside a
	// ring was a ribbon folded over itself forty times, which came out as a pale disc at 0.8 that hid the ships and
	// the sea's name. It is one thin wash now, a fan from the middle, in an ink of its own.)
	auto Wash = [this, &Fills](const FVector2D& Middle, double Radius, const FLinearColor& Ink)
	{
		const int32 Sides = 48;
		const FColor Colour = Ink.ToFColor(false);
		const int32 Hub = Fills.AddVertex(SurfacePointOver(Middle.X, Middle.Y, 0.28), FVector::UpVector, Colour);
		int32 First = INDEX_NONE, Last = INDEX_NONE;
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const double Turn = 2.0 * PI * Side / Sides;
			const FVector2D Rim = Middle + FVector2D(FMath::Cos(Turn), FMath::Sin(Turn)) * Radius;
			const int32 Now = Fills.AddVertex(SurfacePointOver(Rim.X, Rim.Y, 0.28), FVector::UpVector, Colour);
			if (Last != INDEX_NONE)
			{
				Fills.AddTriangleFacingUp(Hub, Last, Now);
			}
			else
			{
				First = Now;
			}
			Last = Now;
		}
		Fills.AddTriangleFacingUp(Hub, Last, First);
	};
	auto Ring = [this, &Buffers](const FVector2D& Middle, double Radius, double HalfWidth, const FLinearColor& Ink)
	{
		TArray<FVector2D> Round;
		const int32 Sides = 40;
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const double Angle = 2.0 * PI * Side / Sides;
			Round.Add(Middle + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		AppendRibbon(Buffers, Round, HalfWidth, 0.3, Ink.ToFColor(false), true);
	};
	// Where the fleet lies now.
	FVector2D From = FVector2D::ZeroVector;
	bool bHaveFleet = false;
	if (const TObjectPtr<AArmyPiece>* Piece = FleetPieces.Find(FleetId))
	{
		if (*Piece)
		{
			From = WorldToMap((*Piece)->GetRestLocation());
			bHaveFleet = true;
			Ring(From, 9.0, 0.7, FLinearColor(1.0f, 1.0f, 1.0f, 0.9f));
		}
	}
	for (int32 Zone : Zones)
	{
		if (!Data.Seas.IsValidIndex(Zone))
		{
			continue;
		}
		const FVector2D Middle = Data.Seas[Zone].Center;
		const bool bHere = Zone == Hover;
		// An anchorage marked on the chart: a ring of brass, doubled and brighter under the pointer.
		const double Wide = bHere ? 15.0 : 13.0;
		auto Anchorage = [&](const FVector2D& Where)
		{
			Wash(Where, Wide, FLinearColor(1.0f, 0.82f, 0.3f, static_cast<float>(bHere ? SeaFill.Y : SeaFill.X))); // the water inside it, tinted
			Ring(Where, Wide, bHere ? 0.9 : 0.55, FLinearColor(1.0f, 0.82f, 0.3f, bHere ? 0.98f : 0.7f));
			Ring(Where, 1.8, bHere ? 1.4 : 1.0, FLinearColor(1.0f, 0.86f, 0.4f, bHere ? 0.98f : 0.75f));
		};
		// (Release 2, the far side.) A sea over the chart's seam from the fleet is marked a second time in the fleet's
		// own turn of the world, where the eye that picked the fleet up can see it (Hawaiian Waters from San Diego),
		// and the course is dashed to it the short way round.
		// (Seam-2: where the orders have a twin over the seam, such a sea is marked once only, at the place nearest
		// the fleet, and the twin marks it at its own place on the chart: marked twice here it would be laid four
		// times, and its wash double.)
		const FVector2D Nearer(bHaveFleet ? NearX(Middle.X, From.X) : Middle.X, Middle.Y);
		const bool bOverTheSeam = FMath::Abs(Nearer.X - Middle.X) > 1.0;
		if (!bOverTheSeam || !OverSeam())
		{
			Anchorage(Middle);
		}
		if (bOverTheSeam)
		{
			Anchorage(Nearer);
		}
		if (bHere && bHaveFleet && FVector2D::Distance(From, Nearer) > 12.0)
		{
			// The course it would steer, dashed from the fleet to the sea.
			const double Length = FVector2D::Distance(From, Nearer);
			const int32 Dashes = FMath::Clamp(FMath::RoundToInt32(Length / 6.0), 2, 200);
			for (int32 Dash = 0; Dash < Dashes; ++Dash)
			{
				TArray<FVector2D> Stroke;
				Stroke.Add(FMath::Lerp(From, Nearer, (Dash + 0.15) / Dashes));
				Stroke.Add(FMath::Lerp(From, Nearer, (Dash + 0.7) / Dashes));
				AppendRibbon(Buffers, Stroke, 0.5, 0.3, FLinearColor(1.0f, 0.92f, 0.6f, 0.85f).ToFColor(false), false);
			}
		}
	}
	// (The proving round of release 1: by night the brass rings of the seas in a fleet's reach, and the tinted water
	// inside them, shone at about 0.8 on a sea at 0.14. They have an ink of their own that the night lets down.)
	if (!SeaReachInk && HighlightMaterial)
	{
		SeaReachInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		ShownNight = -1.0f;
		SetNight(NightNow, bLamplightNow);
	}
	if (!SeaFillInk && HighlightMaterial)
	{
		SeaFillInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		ShownNight = -1.0f;
		SetNight(NightNow, bLamplightNow);
	}
	Buffers.Commit(OrdersMesh, 2, SeaReachInk ? static_cast<UMaterialInterface*>(SeaReachInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
	Fills.Commit(OrdersMesh, 10, SeaFillInk ? static_cast<UMaterialInterface*>(SeaFillInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
}

void ACampaignMapActor::SetRoute(const FVector& FromWorld, const TArray<int32>& Regions, int32 RouteEnds)
{
	OrdersMesh->ClearMeshSection(1);
	if (Regions.Num() == 0)
	{
		return;
	}
	TArray<FVector2D> Points;
	Points.Add(WorldToMap(FromWorld));
	for (int32 RegionId : Regions)
	{
		if (Data.Regions.IsValidIndex(RegionId))
		{
			Points.Add(WorldToMap(ArmySpot(RegionId, 0)));
		}
	}
	if (Points.Num() < 2)
	{
		return;
	}
	FCampaignMeshBuffers Buffers;
	// Gold for a march, red where it ends in a battle, amber where it means declaring war; and (ruling 87) cream
	// for a march kept on a standing order, which ends in a small ring and not in an arrowhead.
	const FColor Gold = (RouteEnds == 1 ? FLinearColor(1.0f, 0.3f, 0.2f, 0.95f) : RouteEnds == 2 ? FLinearColor(1.0f, 0.62f, 0.12f, 0.95f)
		: RouteEnds == 3 ? FLinearColor(0.86f, 0.82f, 0.68f, 0.95f) : FLinearColor(1.0f, 0.83f, 0.45f, 0.95f)).ToFColor(false);
	// A dotted line, as a staff officer pencils a march on the map: short dashes from province to province.
	for (int32 Leg = 0; Leg + 1 < Points.Num(); ++Leg)
	{
		const FVector2D A0 = Points[Leg], B0 = Points[Leg + 1];
		const double Long = FVector2D::Distance(A0, B0);
		const int32 Dashes = FMath::Max(1, FMath::RoundToInt32(Long / 3.4));
		for (int32 D = 0; D < Dashes; ++D)
		{
			const double T0 = (D + 0.14) / Dashes, T1 = (D + 0.72) / Dashes;
			TArray<FVector2D> Dash;
			Dash.Add(FMath::Lerp(A0, B0, T0));
			Dash.Add(FMath::Lerp(A0, B0, T1));
			AppendRibbon(Buffers, Dash, 0.52, 0.34, Gold, false);
		}
		// A mark where each week's march ends: a small ring at every province on the way.
		if (Leg + 2 < Points.Num())
		{
			TArray<FVector2D> Ring;
			for (int32 K = 0; K < 14; ++K)
			{
				Ring.Add(B0 + FVector2D(FMath::Cos(K * 0.4488), FMath::Sin(K * 0.4488)) * 1.25);
			}
			AppendRibbon(Buffers, Ring, 0.3, 0.35, Gold, true);
		}
	}

	if (RouteEnds == 3)
	{
		TArray<FVector2D> Ring;
		for (int32 K = 0; K < 18; ++K)
		{
			Ring.Add(Points.Last() + FVector2D(FMath::Cos(K * 0.349), FMath::Sin(K * 0.349)) * 1.7);
		}
		AppendRibbon(Buffers, Ring, 0.42, 0.36, Gold, true);
		Buffers.Commit(OrdersMesh, 1, HighlightMaterial);
		return;
	}
	// An arrowhead where the road ends.
	const FVector2D End = Points.Last();
	const FVector2D Dir = (End - Points[Points.Num() - 2]).GetSafeNormal();
	const FVector2D Side(Dir.Y, -Dir.X);
	const FVector2D Tip = End + Dir * 2.6, Left = End + Side * 1.7, Right = End - Side * 1.7;
	const int32 A = Buffers.AddVertex(SurfacePoint(Tip.X, Tip.Y, 0.36), FVector::UpVector, Gold);
	const int32 B = Buffers.AddVertex(SurfacePoint(Left.X, Left.Y, 0.36), FVector::UpVector, Gold);
	const int32 C = Buffers.AddVertex(SurfacePoint(Right.X, Right.Y, 0.36), FVector::UpVector, Gold);
	Buffers.AddTriangleFacingUp(A, B, C);
	Buffers.Commit(OrdersMesh, 1, HighlightMaterial);
}

// ---- (release 2, the enemy's month near you; Brian, 24 November 2026: "when the AI nations are doing their turn, show
// movements around my territories or if my units are attacked so i know whats going on".) The arrows of one step of the
// others' month, from army to army, laid in a section of the orders' mesh of their own (11), over the ground's own
// heights. Campaign/EnemyMoves draws them (plain C++, checked alone by Tools/enemy_moves/run.sh).
namespace
{
	/** The arrows' ends as the table stands its pieces (SpotOf: where an army in that province stands, in map units). */
	bool StepArrowEnds(const FCampaignMapData& Map, const TFunctionRef<FVector2D(int32)>& SpotOf, const FStepArrow& Arrow, TheaterEnemyMoves::FArrow& Out)
	{
		if (!Map.Regions.IsValidIndex(Arrow.To))
		{
			return false;
		}
		const FVector2D To = SpotOf(Arrow.To);
		Out.To = TheaterEnemyMoves::FPoint{ To.X, To.Y };
		Out.bFrom = Map.Regions.IsValidIndex(Arrow.From) && Arrow.From != Arrow.To;
		if (Out.bFrom)
		{
			const FVector2D From = SpotOf(Arrow.From);
			Out.From = TheaterEnemyMoves::FPoint{ From.X, From.Y };
			// (two ends a world apart are the two sides of the whole-world map's seam: the mark alone is drawn)
			Out.bFrom = FVector2D::Distance(From, To) < 400.0;
		}
		Out.Kind = Arrow.Kind == 1 ? TheaterEnemyMoves::EKind::Attack : Arrow.Kind == 2 ? TheaterEnemyMoves::EKind::Fell : TheaterEnemyMoves::EKind::Move;
		Out.bWar = Arrow.bWar;
		Out.bAway = Arrow.bAway;
		Out.bTaken = Arrow.bTaken;
		Out.bSiege = Arrow.bSiege;
		Out.bAlly = Arrow.bAlly;
		Out.bOurs = Arrow.bOurs;
		Out.Corps = FMath::Max(Arrow.Corps, 1);
		return true;
	}
}

void ACampaignMapActor::ShowStepArrows(const TArray<FStepArrow>& Arrows, float Grow)
{
	if (!MovesMesh)
	{
		return;
	}
	MovesMesh->ClearAllMeshSections();
	if (Arrows.Num() == 0 || Grow <= 0.0f || !Data.IsLoaded())
	{
		return;
	}
	auto SpotOf = [this](int32 RegionId) { return WorldToMap(ArmySpot(RegionId, 0)); };
	std::vector<TheaterEnemyMoves::FArrow> Laid;
	for (const FStepArrow& Arrow : Arrows)
	{
		TheaterEnemyMoves::FArrow One;
		if (StepArrowEnds(Data, SpotOf, Arrow, One))
		{
			Laid.push_back(One);
		}
	}
	TheaterEnemyMoves::FDrawing Drawing;
	TheaterEnemyMoves::Lay(Laid, Grow, Drawing);
	FCampaignMeshBuffers Buffers;
	auto Colour = [](const TheaterEnemyMoves::FInk& Ink) { return FColor(Ink.R, Ink.G, Ink.B, Ink.A); };
	// (the dark edges first, then the inks over them: one section, laid in that order)
	for (const TheaterEnemyMoves::FLayer* Layer : { &Drawing.Under, &Drawing.Over })
	{
		for (const TheaterEnemyMoves::FStroke& Stroke : Layer->Strokes)
		{
			TArray<FVector2D> Points;
			Points.Reserve(static_cast<int32>(Stroke.Points.size()));
			for (const TheaterEnemyMoves::FPoint& Point : Stroke.Points)
			{
				Points.Emplace(Point.X, Point.Y);
			}
			AppendRibbon(Buffers, Points, Stroke.Half, Stroke.Lift, Colour(Stroke.Ink), Stroke.bClosed);
		}
		for (const TheaterEnemyMoves::FTriangle& Face : Layer->Triangles)
		{
			const FColor Ink = Colour(Face.Ink);
			const int32 A = Buffers.AddVertex(SurfacePoint(Face.A.X, Face.A.Y, Face.Lift), FVector::UpVector, Ink);
			const int32 B = Buffers.AddVertex(SurfacePoint(Face.B.X, Face.B.Y, Face.Lift), FVector::UpVector, Ink);
			const int32 C = Buffers.AddVertex(SurfacePoint(Face.C.X, Face.C.Y, Face.Lift), FVector::UpVector, Ink);
			Buffers.AddTriangleFacingUp(A, B, C);
		}
	}
	Buffers.Commit(MovesMesh, 0, HighlightMaterial);
}

bool ACampaignMapActor::FrameStepArrows(const TArray<FStepArrow>& Arrows, FVector2D& OutMiddle, double& OutReach) const
{
	auto SpotOf = [this](int32 RegionId) { return Data.Regions[RegionId].Center; }; // (framing needs no more than the provinces' middles)
	std::vector<TheaterEnemyMoves::FPoint> Ends;
	for (const FStepArrow& Arrow : Arrows)
	{
		TheaterEnemyMoves::FArrow One;
		if (!StepArrowEnds(Data, SpotOf, Arrow, One))
		{
			continue;
		}
		Ends.push_back(One.To);
		if (One.bFrom)
		{
			Ends.push_back(One.From);
		}
	}
	const TheaterEnemyMoves::FFrame Frame = TheaterEnemyMoves::Frame(Ends);
	OutMiddle = FVector2D(Frame.Middle.X, Frame.Middle.Y);
	OutReach = Frame.Reach;
	return Frame.bValid;
}
// ---- (end release 2)

void ACampaignMapActor::ShowLanes(const TArray<FViewLane>& Lanes)
{
	THEATER_SPEED_SCOPE(ShowLanes);
	if (!LanesMesh || !Data.IsLoaded())
	{
		return;
	}
	FString Key;
	for (const FViewLane& Lane : Lanes)
	{
		Key += FString::Printf(TEXT("%s%d:%d;"), *Lane.State, Lane.Path.Num(), FMath::RoundToInt32(Lane.Flow * 20.0f));
	}
	// (seam-2: the short way over the chart's seam only while both sides of it are drawn, tc.Far.OverSeam: with the
	// switch off a lane is ruled as it always was, since nothing would lay the part that ran past the chart's frame)
	const bool bShortWay = OverSeam();
	if (bShortWay)
	{
		Key += TEXT("~");
	}
	if (Key == ShownLanes)
	{
		return;
	}
	ShownLanes = Key;
	LanesMesh->ClearAllMeshSections();
	FCampaignMeshBuffers Buffers;
	for (const FViewLane& Lane : Lanes)
	{
		if (Lane.Path.Num() < 2)
		{
			continue;
		}
		// (the player's trade routes are drawn in gold, over land as well as sea)
		const bool bTrade = Lane.State.StartsWith(TEXT("trade"));
		// (a trade route that is open is a dotted line in ink, as a clerk would rule it: sepia on the land and pale
		// on the water. Raided it is amber and cut it is red, still dotted and thin.)
		const FLinearColor Ink = Lane.State.EndsWith(TEXT("cut")) ? FLinearColor(0.95f, 0.14f, 0.1f, 0.62f)
			: Lane.State.EndsWith(TEXT("raided")) ? FLinearColor(1.0f, 0.68f, 0.14f, 0.58f)
			: bTrade ? FLinearColor(0.2f, 0.135f, 0.07f, 0.78f)
			: FLinearColor(0.86f, 0.93f, 1.0f, 0.5f); // (a sea lane is ruled like a trade route, a fine dotted line in pale ink: no chalk bars on the water)
		const FLinearColor TradeAtSea(0.9f, 0.84f, 0.66f, 0.42f);
		// (seam-2, the art director, 25 November 2026: over the chart's seam the lane is ruled the short way. Each
		// point is counted in the turn of the world nearest the one before it, so the lane from California to
		// Hawaii is not ruled east across two oceans; what then runs out past the chart's frame is laid on the
		// other side by the lanes' twin. Without the far side, or with the switch off, the points are as they were given.)
		TArray<FVector2D> Chained = Lane.Path;
		for (int32 Link = 1; bShortWay && Link < Chained.Num(); ++Link)
		{
			Chained[Link].X = NearX(Chained[Link].X, Chained[Link - 1].X);
		}
		// The lane as a smooth curve through its points, a step every map unit or so.
		TArray<FVector2D> Curve;
		const int32 Count = Chained.Num();
		for (int32 K = 0; K + 1 < Count; ++K)
		{
			const FVector2D P0 = Chained[FMath::Max(K - 1, 0)], P1 = Chained[K], P2 = Chained[K + 1], P3 = Chained[FMath::Min(K + 2, Count - 1)];
			const int32 Steps = FMath::Clamp(FMath::CeilToInt32(FVector2D::Distance(P1, P2) / 2.0), 2, 400);
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const double T = static_cast<double>(Step) / Steps;
				Curve.Add(0.5 * ((2.0 * P1) + (P2 - P0) * T + (2.0 * P0 - 5.0 * P1 + 4.0 * P2 - P3) * T * T + (3.0 * P1 - P0 - 3.0 * P2 + P3) * T * T * T));
			}
		}
		Curve.Add(Chained.Last());
		// Drawn as dashes, like a course pencilled on a chart; only over the water.
		const int32 Dash = 1, Gap = 2;
		TArray<TArray<FVector2D>> Strokes;
		TArray<bool> StrokeWet;
		for (int32 First = 0; First + 1 < Curve.Num(); First += Dash + Gap)
		{
			TArray<FVector2D>& Stroke = Strokes.AddDefaulted_GetRef();
			for (int32 K = First; K <= First + Dash && K < Curve.Num(); ++K)
			{
				if (!bTrade && GroundAt(Curve[K].X, Curve[K].Y) > 0.0f)
				{
					break;
				}
				Stroke.Add(Curve[K]);
			}
			StrokeWet.Add(GroundAt(Curve[First].X, Curve[First].Y) <= 0.0f);
		}
		for (int32 Index = 0; Index < Strokes.Num(); ++Index)
		{
			// (a dash of a sea lane with no dash before it and none after it, in the mouth of a river or a gap
			// between islands, is left out: alone it is a coloured tile lying on the water)
			const bool bBefore = Strokes.IsValidIndex(Index - 1) && Strokes[Index - 1].Num() >= 2, bAfter = Strokes.IsValidIndex(Index + 1) && Strokes[Index + 1].Num() >= 2;
			if (Strokes[Index].Num() >= 2 && (bTrade || bBefore || bAfter))
			{
				const bool bOpenTrade = bTrade && Lane.State == TEXT("trade");
				const FLinearColor Colour = bOpenTrade && StrokeWet[Index] ? TradeAtSea : Ink;
				AppendRibbon(Buffers, Strokes[Index], bTrade ? 0.24 : 0.3, bTrade ? 0.3 : 0.12, Colour.ToFColor(false), false);
			}
		}
	}
	if (!LanesInk && HighlightMaterial)
	{
		LanesInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		LanesInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
		ShownNight = -1.0f; // (so that the hour's light is laid on it at once)
	}
	Buffers.Commit(LanesMesh, 0, LanesInk ? static_cast<UMaterialInterface*>(LanesInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
	SetNight(NightNow, bLamplightNow);
}

FString ACampaignMapActor::ModelFor(const FString& Nation, const FString& Kind)
{
	if (Kind == TEXT("arm")) { return FMiniatureMesh::MainTankOf(Nation); }
	if (Kind == TEXT("heavy"))
	{
		const FString Own = FMiniatureMesh::HeavyTankOf(Nation);
		return Own.IsEmpty() ? FMiniatureMesh::MainTankOf(Nation) : Own;
	}
	if (Kind == TEXT("art")) { return FMiniatureMesh::MainGunOf(Nation); }
	if (Kind == TEXT("mot")) { return FMiniatureMesh::MainCarrierOf(Nation); }
	if (Kind == TEXT("cav")) { return TEXT("Rider:") + Nation; }
	if (Kind == TEXT("eng")) { return TEXT("Pioneer:") + Nation; }
	return TEXT("Soldier:") + Nation;
}

bool ACampaignMapActor::StageModel(const FString& Model, UProceduralMeshComponent* Stage, double& OutRadius)
{
	if (!Stage)
	{
		return false;
	}
	// Everything is built to one scale, a hundred units to the metre, and then set with its middle at the stage's.
	const double Metre = 100.0;
	FMiniatureMesh Built;
	FString Kind, Tag;
	if (Model.Split(TEXT(":"), &Kind, &Tag) && (Kind == TEXT("Soldier") || Kind == TEXT("Rider") || Kind == TEXT("Pioneer")))
	{
		if (Kind == TEXT("Rider")) { Built.Rider(Tag, FVector::ZeroVector, Metre, 0.0, true); }
		else if (Kind == TEXT("Pioneer")) { Built.Pioneer(Tag, FVector::ZeroVector, Metre, 0.0, true); }
		else { Built.Soldier(Tag, FVector::ZeroVector, Metre, 0.0, true); }
	}
	else if (Kind == TEXT("Civic"))
	{
		// A town's building for its picture: "Civic:<line 0-5>:<level>:<TAG>", or "Civic:lot" for an empty lot.
		TArray<FString> Part;
		Tag.ParseIntoArray(Part, TEXT(":"));
		const FCampaignNation* Whose = Part.Num() >= 3 ? Data.FindNation(Part[2]) : nullptr;
		if (Part.Num() >= 2)
		{
			const int32 Level = FCString::Atoi(*Part[1]);
			Built.Civic(FVector::ZeroVector, Metre, FCString::Atoi(*Part[0]), Level, Level <= 0, Whose ? FLinearColor(Whose->Color) : FLinearColor(0.5f, 0.5f, 0.5f));
		}
		else
		{
			// An empty lot: bare earth pegged out with posts and a rope, a stack of bricks and some planks waiting.
			const FLinearColor Earth(FColor::FromHex(TEXT("8C7A5A"))), Grass(FColor::FromHex(TEXT("6E8B4C"))), Post(FColor::FromHex(TEXT("4A3A2A"))), Rope(FColor::FromHex(TEXT("D8CFB4"))), Brick(FColor::FromHex(TEXT("8E5644"))), Plank(FColor::FromHex(TEXT("B79A6A")));
			Built.Box(FVector(0.0, 0.0, 1.5), FVector(210.0, 250.0, 3.0), Grass);
			Built.Box(FVector(0.0, 0.0, 3.5), FVector(170.0, 210.0, 2.0), Earth);
			FVector Last = FVector::ZeroVector;
			for (int32 K = 0; K <= 4; ++K)
			{
				const FVector Corner((K == 0 || K == 3 || K == 4) ? -85.0 : 85.0, (K == 0 || K == 1 || K == 4) ? -105.0 : 105.0, 0.0);
				if (K < 4)
				{
					Built.Rod(Corner, Corner + FVector(0.0, 0.0, 34.0), 2.6, Post, 5);
				}
				if (K > 0)
				{
					Built.Rod(Last + FVector(0.0, 0.0, 28.0), Corner + FVector(0.0, 0.0, 28.0), 0.9, Rope, 4);
				}
				Last = Corner;
			}
			Built.Box(FVector(-35.0, -50.0, 13.0), FVector(38.0, 26.0, 18.0), Brick, 12.0);
			Built.Box(FVector(-30.0, -48.0, 27.0), FVector(26.0, 20.0, 10.0), Brick, 30.0);
			for (int32 K = 0; K < 4; ++K)
			{
				Built.Box(FVector(30.0 + K * 3.0, 40.0 + K * 7.0, 6.0 + K * 3.2), FVector(90.0, 9.0, 3.0), Plank, 8.0 * K);
			}
		}
	}
	else if (Kind == TEXT("Target"))
	{
		// (Brian's ask of 15 November 2026, ruling 54.) The picture of a raid's target: "Target:<sort>:<province or
		// -1>:<holder>". A place with a town is shown as its own town, built as it stands on the table (its works, its
		// quay, its kit), on a round board of its own ground and water; a target with no town is a small scene of what
		// the bombs are meant for. (These stand until the art director's painted pictures come.)
		TArray<FString> Part;
		Tag.ParseIntoArray(Part, TEXT(":"));
		const FString Sort = Part.Num() > 0 ? Part[0] : FString();
		const int32 RegionId = Part.Num() > 1 ? FCString::Atoi(*Part[1]) : INDEX_NONE;
		const FString Holder = Part.Num() > 2 ? Part[2] : FString();
		const FCampaignNation* Whose = Data.FindNation(Holder);
		const FLinearColor Livery = Whose ? FLinearColor(Whose->Color) : FLinearColor(0.45f, 0.45f, 0.42f);
		const double U = UnitScale;
		// (the studio's lamp is bright: the board's green and its water are mixed deep, or they come out straw and chalk)
		const FLinearColor Field(FColor::FromHex(TEXT("4E6034"))), Water(FColor::FromHex(TEXT("1F4660"))), Iron(FColor::FromHex(TEXT("2B2A28")));
		const FLinearColor Earth(FColor::FromHex(TEXT("8C7A5A"))), Ballast(FColor::FromHex(TEXT("8A8478"))), Brick(FColor::FromHex(TEXT("8E5644"))), Slate(FColor::FromHex(TEXT("4A4A4E"))), Wood(FColor::FromHex(TEXT("6E5238")));
		const FCampaignRegion* Region = Data.Regions.IsValidIndex(RegionId) ? &Data.Regions[RegionId] : nullptr;
		const bool bPlace = Sort == TEXT("factories") || Sort == TEXT("rail") || Sort == TEXT("harbour") || Sort == TEXT("town");
		const bool bHasTown = Region && (Region->bCapital || Region->Terrain == TEXT("urban") || Region->VictoryPoints > 0 || Region->Factories > 0 || Region->bOil || Region->bPort);
		if (bPlace && bHasTown)
		{
			FMiniatureMesh::FTownSite Site;
			FillTownSite(*Region, Site);
			const FVector2D Middle = Region->Center;
			const double High = U * VerticalScale;
			Site.Ground = [U, High, Middle](double X, double Y, double Lift) { return FVector((Middle.Y - Y) * U, (X - Middle.X) * U, Lift * High); };
			Site.bPlots = false;
			Built.Township(Site);
			// (The board: a round wooden base, wide enough for a port's quay and the ships alongside it, laid with the
			// town's own ground where that is dry and with water where it is not.)
			const double Reach = FMath::Max(Site.Radius + 2.5, Site.bQuay ? FVector2D::Distance(Site.Quay, Middle) + 5.5 : 0.0);
			const int32 Cells = 30;
			const double Step = 2.0 * Reach / Cells;
			Built.Pillar(FVector(0.0, 0.0, -0.2 * U), Reach * U, Reach * U, 0.17 * U, FLinearColor(FColor::FromHex(TEXT("2E2218"))), 48);
			for (int32 J = 0; J < Cells; ++J)
			{
				for (int32 I = 0; I < Cells; ++I)
				{
					const double X = Middle.X - Reach + (I + 0.5) * Step, Y = Middle.Y - Reach + (J + 0.5) * Step;
					if (FVector2D(X - Middle.X, Y - Middle.Y).Size() < Reach - 0.75 * Step)
					{
						const bool bDry = GroundAt(X, Y) > 0.0f;
						Built.Box(FVector((Middle.Y - Y) * U, (X - Middle.X) * U, bDry ? -0.03 * U : -0.05 * U), FVector(Step * U * 1.03, Step * U * 1.03, 0.06 * U), bDry ? Field : Water);
					}
				}
			}
		}
		else if (Sort == TEXT("fleet"))
		{
			Built.Pillar(FVector(0.0, 0.0, -0.1 * U), 7.5 * U, 7.5 * U, 0.1 * U, Water, 44);
			Built.Fleet(1, 0, 1, 2, 0, U, Livery, Holder, true);
		}
		else if (Sort == TEXT("airfield"))
		{
			Built.AirFleet(1, 1, U, Livery, Holder);
		}
		else if (Sort == TEXT("troops"))
		{
			const TArray<FString> Kinds = { FString(TEXT("inf")), FString(TEXT("arm")), FString(TEXT("inf")), FString(TEXT("art")) };
			Built.Pillar(FVector(0.0, 0.0, -0.1 * U), 5.2 * U, 5.2 * U, 0.1 * U, Field, 40);
			Built.Force(Kinds, 4.3 * U, 0.0, U, Livery, Holder);
		}
		else if (Sort == TEXT("works"))
		{
			// Fieldworks: casemates behind a zigzag of thrown-up earth, a gun, and men standing to.
			Built.Pillar(FVector(0.0, 0.0, -0.1 * U), 5.2 * U, 5.2 * U, 0.1 * U, Field, 40);
			Built.Bunker(FVector(-1.4 * U, -0.6 * U, 0.0), 1.3 * U, Livery, 14.0);
			Built.Bunker(FVector(1.7 * U, 0.2 * U, 0.0), 1.1 * U, Livery, -8.0);
			Built.FieldGun(FVector(0.2 * U, -2.4 * U, 0.0), 1.0 * U, Livery, 6.0);
			for (int32 Leg = 0; Leg < 6; ++Leg)
			{
				Built.Box(FVector(2.9 * U + (Leg % 2 ? 0.25 : -0.25) * U, (Leg - 2.5) * 1.25 * U, 0.12 * U), FVector(0.36 * U, 1.5 * U, 0.24 * U), Earth, Leg % 2 ? 26.0 : -26.0);
			}
			for (int32 Man = 0; Man < 3; ++Man)
			{
				Built.Rifleman(FVector(2.2 * U, (Man - 1) * 1.3 * U, 0.0), 0.8 * U, Livery, 0.0);
			}
		}
		else
		{
			// A rail yard on open ground: four roads of track with wagons standing, a goods shed and a water tower.
			Built.Pillar(FVector(0.0, 0.0, -0.1 * U), 5.6 * U, 5.6 * U, 0.1 * U, Field, 44);
			for (int32 Road = 0; Road < 4; ++Road)
			{
				const double Y = (Road - 1.5) * 0.8 * U;
				Built.Box(FVector(0.0, Y, 0.015 * U), FVector(9.0 * U, 0.42 * U, 0.03 * U), Ballast);
				for (const double Side : { -0.12, 0.12 })
				{
					Built.Rod(FVector(-4.5 * U, Y + Side * U, 0.05 * U), FVector(4.5 * U, Y + Side * U, 0.05 * U), 0.022 * U, Iron, 4);
				}
				for (int32 Wagon = 0; Wagon < 3 + Road % 2; ++Wagon)
				{
					const double X = (-3.2 + Wagon * 1.15 + Road * 0.5) * U;
					Built.Box(FVector(X, Y, 0.24 * U), FVector(0.95 * U, 0.3 * U, 0.3 * U), (Wagon + Road) % 3 == 0 ? Slate : (Wagon + Road) % 3 == 1 ? Wood : Brick);
				}
			}
			Built.House(FVector(0.5 * U, 2.6 * U, 0.0), 2.6 * U, 0.9 * U, 0.5 * U, 0.32 * U, Brick, Slate);
			Built.Pillar(FVector(-3.6 * U, -2.4 * U, 0.0), 0.3 * U, 0.3 * U, 0.9 * U, Iron, 10);
		}
	}
	else if (!Built.Vehicle(Model, FVector::ZeroVector, Metre, 0.0, true))
	{
		return false;
	}
	if (Built.Vertices.Num() == 0)
	{
		return false;
	}
	const FBox Box(Built.Vertices);
	const FVector Middle = Box.GetCenter();
	for (FVector& Point : Built.Vertices)
	{
		Point -= Middle;
	}
	OutRadius = Box.GetExtent().Size();
	{
		// (For the large portrait's fit: the box of all but the outermost hundredth of the model's points each way,
		// so that a thin aerial does not make the whole tank small in its picture. The aerial may leave the frame.)
		TArray<double> Along[3];
		for (const FVector& Point : Built.Vertices)
		{
			Along[0].Add(Point.X);
			Along[1].Add(Point.Y);
			Along[2].Add(Point.Z);
		}
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Along[Axis].Sort();
			const int32 Skip = Along[Axis].Num() / 100;
			StagedExtent[Axis] = FMath::Max(FMath::Abs(Along[Axis][Skip]), FMath::Abs(Along[Axis][Along[Axis].Num() - 1 - Skip]));
		}
	}
	StagedPoints = Built.Vertices;
	Stage->ClearAllMeshSections();
	Stage->CreateMeshSection(0, Built.Vertices, Built.Triangles, Built.Normals, TArray<FVector2D>(), Built.Colors, TArray<FProcMeshTangent>(), false);
	Stage->SetMaterial(0, SceneryMaterial);
	return true;
}

void ACampaignMapActor::FitStudioCamera(USceneCaptureComponent2D* Camera, const FVector& Middle, double HighOverWide, double Fill, const FVector& From)
{
	// (The art director's ruling of round 22: an aircraft across at least 85% of its picture. A box round an aircraft
	// seen from the front quarter is mostly air, and fitted to the box the machine filled 64%. Here every point of the
	// model is looked at as the camera sees it: the camera is aimed at the middle of what it sees and stood where the
	// widest of it, or the tallest, fills the share asked for. Found by trying, six times over: it settles in three.)
	const FRotator Looks = (-From).Rotation();
	const FRotationMatrix Axes(Looks);
	const FVector Ahead = Axes.GetUnitAxis(EAxis::X), Across = Axes.GetUnitAxis(EAxis::Y), Upward = Axes.GetUnitAxis(EAxis::Z);
	const double TanWide = FMath::Tan(FMath::DegreesToRadians(Camera->FOVAngle * 0.5)), TanHigh = TanWide * HighOverWide;
	FVector Aim = FVector::ZeroVector;
	double Back = FMath::Max(StagedExtent.Size(), 1.0) / TanWide;
	for (int32 Try = 0; Try < 6 && StagedPoints.Num() > 0; ++Try)
	{
		double Least[2] = { 1.0e9, 1.0e9 }, Most[2] = { -1.0e9, -1.0e9 };
		for (const FVector& Point : StagedPoints)
		{
			const FVector Off = Point - Aim;
			const double Depth = FMath::Max(Back + (Off | Ahead), 1.0);
			const double Seen[2] = { (Off | Across) / Depth, (Off | Upward) / Depth };
			for (int32 Way = 0; Way < 2; ++Way)
			{
				Least[Way] = FMath::Min(Least[Way], Seen[Way]);
				Most[Way] = FMath::Max(Most[Way], Seen[Way]);
			}
		}
		Aim += (Across * (Least[0] + Most[0]) + Upward * (Least[1] + Most[1])) * 0.5 * Back;
		const double Needs = FMath::Max((Most[0] - Least[0]) * 0.5 / (TanWide * Fill), (Most[1] - Least[1]) * 0.5 / (TanHigh * Fill));
		Back *= FMath::Clamp(Needs, 0.2, 5.0);
	}
	const FVector Eye = Middle + Aim + From * Back;
	Camera->SetRelativeLocationAndRotation(Eye, Looks);
}

void ACampaignMapActor::AimStudioCamera(USceneCaptureComponent2D* Camera, const FVector& Middle, double Radius)
{
	// From in front, a little to one side and above: the three-quarter view a model is photographed from.
	// (a model's front is toward -X and its right toward -Y)
	const FVector From = FVector(-0.78, -0.5, 0.36).GetSafeNormal();
	const double Back = Radius / FMath::Tan(FMath::DegreesToRadians(Camera->FOVAngle * 0.5)) * 1.02;
	const FVector Eye = Middle + From * Back;
	Camera->SetRelativeLocationAndRotation(Eye, (Middle - Eye).Rotation());
}

void ACampaignMapActor::TakeStudioPicture(USceneCaptureComponent2D* Camera, bool bAircraft)
{
	// (The art director's ruling of round 22: an aircraft's broad wings and back face the studio's lamp squarely, and
	// in the exposure that suits a tank they were washed out: the two tones of a camouflage read as one. An aircraft
	// is taken a stop and a little darker. "-TheaterPlaneBias=-2.0" tries another.)
	static float PlaneBias = -2.2f;
	static const bool bAsked = FParse::Value(FCommandLine::Get(), TEXT("TheaterPlaneBias="), PlaneBias);
	// (The art director's ruling of round 23: the cards are brought to the tray's light, so that a tank's card and an
	// aircraft's side by side in the Arsenal are not two photographs from different studios.)
	static float CardBias = -0.9f;
	static const bool bCardAsked = FParse::Value(FCommandLine::Get(), TEXT("TheaterCardBias="), CardBias);
	Camera->PostProcessSettings.AutoExposureBias = bAircraft ? PlaneBias : Camera == ShowCamera ? -0.9f : CardBias;
	// (The art director's ruling of round 19: a unit's picture is taken in the studio's own steady light, the light
	// of a plain day. Lit by the room, a half-track in field grey came out blue by night, and a picture first taken
	// by night stayed blue by day. The room's lights are set to the day for the moment of the picture and put back:
	// the picture is rendered between the two, and the table itself never sees the change.)
	ACampaignEnvironment* Room = nullptr;
	if (UWorld* World = GetWorld())
	{
		TActorIterator<ACampaignEnvironment> It(World);
		Room = It ? *It : nullptr;
	}
	if (Room)
	{
		Room->StudioLight(true);
	}
	Camera->CaptureScene();
	if (Room)
	{
		Room->StudioLight(false);
	}
}

UTextureRenderTarget2D* ACampaignMapActor::GetPortrait(const FString& Model, bool bLarge)
{
	TMap<FString, TObjectPtr<UTextureRenderTarget2D>>& Kept_ = bLarge ? LargePortraits : Portraits;
	if (const TObjectPtr<UTextureRenderTarget2D>* Kept = Kept_.Find(Model))
	{
		return Kept->Get();
	}
	UTextureRenderTarget2D* Picture = nullptr;
	double Radius = 100.0;
	if (PortraitMesh && PortraitCamera && StageModel(Model, PortraitMesh, Radius))
	{
		Picture = NewObject<UTextureRenderTarget2D>(this);
		Picture->ClearColor = FLinearColor(0.03f, 0.028f, 0.024f, 1.0f);
		Picture->InitCustomFormat(bLarge ? 552 : 184, bLarge ? 336 : 112, PF_B8G8R8A8, false); // (the same shape, three times the size)
		Picture->UpdateResourceImmediate(true);
		AimStudioCamera(PortraitCamera, PortraitMesh->GetRelativeLocation(), Radius);
		if (bLarge)
		{
			// (The art director's ruling: the whole piece in the picture with a tenth of margin round it. The small
			// pictures are framed on the model's roundness, which cut a standing figure at the helmet and the knee.
			// The large one is fitted to the model's own box, as the camera sees it: its width and its height.)
			// (a raid's target is a town on a board, looked down on as the table is; a machine is looked at from lower)
			const FVector From = (Model.StartsWith(TEXT("Target:")) ? FVector(-0.66, -0.4, 0.64) : FVector(-0.78, -0.5, 0.36)).GetSafeNormal();
			const FRotator Looks = (-From).Rotation();
			const FRotationMatrix Axes(Looks);
			auto Half = [this](const FVector& Axis) { return FMath::Abs(StagedExtent.X * Axis.X) + FMath::Abs(StagedExtent.Y * Axis.Y) + FMath::Abs(StagedExtent.Z * Axis.Z); };
			const double Wide = Half(Axes.GetUnitAxis(EAxis::Y)), High = Half(Axes.GetUnitAxis(EAxis::Z)), Deep = Half(Axes.GetUnitAxis(EAxis::X));
			const double TanWide = FMath::Tan(FMath::DegreesToRadians(PortraitCamera->FOVAngle * 0.5)), TanHigh = TanWide * 336.0 / 552.0;
			// (An aircraft is as deep as it is wide, and set back by half its depth it filled little more than half the
			// picture. The art director's ruling of round 21: show the plane large. It is set back by a sixth.)
			FString Kind = Model, Whose;
			Model.Split(TEXT(":"), &Kind, &Whose);
			const double Back = FMath::Max(Wide / TanWide, High / TanHigh) * 1.1 + Deep * (FMiniatureMesh::VehicleAloft(Kind) > 0.0 ? 0.16 : 0.5);
			const FVector Middle = PortraitMesh->GetRelativeLocation(), Eye = Middle + From * Back;
			PortraitCamera->SetRelativeLocationAndRotation(Eye, (Middle - Eye).Rotation());
		}
		FString What = Model, Owner;
		Model.Split(TEXT(":"), &What, &Owner);
		const bool bAircraft = FMiniatureMesh::VehicleAloft(What) > 0.0;
		if (bAircraft)
		{
			// (An aircraft, large or small, is fitted to the machine itself: nine tenths of the picture's width.)
			// (And it is looked at from higher than a tank is: its camouflage and its markings are on its back and its
			// wings, and seen from low down the sky's light glances off them and washes them pale.)
			static float PlaneFill = 0.9f;
			static const bool bFillAsked = FParse::Value(FCommandLine::Get(), TEXT("TheaterPlaneFill="), PlaneFill);
			static FVector PlaneFrom(-0.5, -0.72, 0.5); // (from here every one of the fifteen fills its picture both ways)
			static bool bFromRead = false;
			if (!bFromRead)
			{
				bFromRead = true;
				FString Asked;
				TArray<FString> Parts;
				if (FParse::Value(FCommandLine::Get(), TEXT("TheaterPlaneFrom="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 3)
				{
					PlaneFrom = FVector(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), FCString::Atod(*Parts[2]));
				}
			}
			FitStudioCamera(PortraitCamera, PortraitMesh->GetRelativeLocation(), 336.0 / 552.0, PlaneFill, PlaneFrom.GetSafeNormal());
		}
		else if (Model.StartsWith(TEXT("Target:")))
		{
			// (a raid's target is looked down on as the table is, and its board fills the picture, large or small)
			FitStudioCamera(PortraitCamera, PortraitMesh->GetRelativeLocation(), 336.0 / 552.0, 0.95, FVector(-0.66, -0.4, 0.64).GetSafeNormal());
		}
		PortraitCamera->TextureTarget = Picture;
		TakeStudioPicture(PortraitCamera, bAircraft);
	}
	Kept_.Add(Model, Picture); // (kept even when there is none, so that it is not tried again every frame)
	return Picture;
}

void ACampaignMapActor::SetShowpiece(const FString& Model)
{
	if (Model == ShowModel || !ShowMesh || !ShowCamera)
	{
		return;
	}
	ShowModel = Model;
	double Radius = 100.0;
	if (Model.IsEmpty() || !StageModel(Model, ShowMesh, Radius))
	{
		ShowModel.Reset();
		return;
	}
	if (!ShowTarget)
	{
		ShowTarget = NewObject<UTextureRenderTarget2D>(this);
		ShowTarget->ClearColor = FLinearColor(0.03f, 0.028f, 0.024f, 1.0f);
		ShowTarget->InitCustomFormat(420, 260, PF_B8G8R8A8, false);
		ShowTarget->UpdateResourceImmediate(true);
	}
	AimStudioCamera(ShowCamera, ShowMesh->GetRelativeLocation(), Radius);
	ShowCamera->TextureTarget = ShowTarget;
	{
		FString What = Model, Owner;
		Model.Split(TEXT(":"), &What, &Owner);
		bShowAircraft = FMiniatureMesh::VehicleAloft(What) > 0.0;
	}
	TakeStudioPicture(ShowCamera, bShowAircraft);
}

FVector2D ACampaignMapActor::AirfieldSpot(int32 RegionId) const
{
	if (const FVector2D* Spot = AirfieldSpots.Find(RegionId))
	{
		return *Spot;
	}
	return Data.Regions.IsValidIndex(RegionId) ? Data.Regions[RegionId].Center + FVector2D(-10.0, 12.0) * TownSpread(RegionId) : FVector2D::ZeroVector;
}

int32 ACampaignMapActor::AirfieldNear(const FVector2D& MapPoint) const
{
	int32 Best = INDEX_NONE;
	double Nearest = 4.2 * 4.2; // about the reach of the field itself
	for (const TPair<int32, FVector2D>& Field : AirfieldSpots)
	{
		const double Far = FVector2D::DistSquared(Field.Value, MapPoint);
		if (Far < Nearest)
		{
			Nearest = Far;
			Best = Field.Key;
		}
	}
	return Best;
}

void ACampaignMapActor::AirfieldsWithin(const FVector2D& MapPoint, double Reach, TArray<TPair<int32, FVector2D>>& Out) const
{
	Out.Reset();
	const double Far = Reach * Reach;
	for (const TPair<int32, FVector2D>& Field : AirfieldSpots)
	{
		if (FVector2D::DistSquared(Field.Value, MapPoint) <= Far)
		{
			Out.Add(Field);
		}
	}
}

bool ACampaignMapActor::GetAirfieldOutline(int32 RegionId, TArray<FVector>& OutCorners) const
{
	OutCorners.Reset();
	const FVector2D* Spot = AirfieldSpots.Find(RegionId);
	const FVector2D* Lie = AirfieldLie.Find(RegionId);
	if (!Spot || !Lie)
	{
		return false;
	}
	// (as ShowBuildings lays the field: the towns' unit, an empty field built smaller, its grass Long by Wide, turned)
	const double Unit = UnitScale * 2.1;
	const double Field = Unit * 1.3 * (bAirfieldsKnown && !AirfieldsUsed.Contains(RegionId) ? 0.62 : 1.0);
	const double Long = (2.9 + 0.45 * Lie->Y) * 0.62 * Field, Wide = (1.5 + 0.3 * Lie->Y) * 0.66 * Field;
	const double Turn = FMath::DegreesToRadians(Lie->X), C = FMath::Cos(Turn), S = FMath::Sin(Turn);
	const FVector Middle = SurfacePoint(Spot->X, Spot->Y, 0.3);
	const double Along[4] = { Long, Long, -Long, -Long }, Across[4] = { -Wide, Wide, Wide, -Wide };
	for (int32 Each = 0; Each < 4; ++Each)
	{
		OutCorners.Add(Middle + FVector(C * Along[Each] - S * Across[Each], S * Along[Each] + C * Across[Each], 0.0));
	}
	return true;
}

void ACampaignMapActor::FlySortie(const FSortie& Sortie)
{
	if (!GetWorld() || (!Sortie.bFromSea && !Data.Regions.IsValidIndex(Sortie.From)))
	{
		return;
	}
	FFlight& Flight = Flights.AddDefaulted_GetRef();
	Flight.Sortie = Sortie;
	{
		// ("-TheaterFlak=2" gives every raid's target that many anti-aircraft guns, for a picture of the flak: few
		// places have guns when a war opens.)
		static int32 StagedFlak = -1;
		static const bool bFlakAsked = FParse::Value(FCommandLine::Get(), TEXT("TheaterFlak="), StagedFlak);
		if (bFlakAsked && StagedFlak >= 0 && Sortie.bBombers)
		{
			Flight.Sortie.Flak = StagedFlak;
		}
	}
	{
		// ("-TheaterFlakLight" gives every raid on a target with no guns built its light flak, for a picture of the pale
		// puffs: release 2, the flak brief, 2c. They are drawn only with tc.Look.FlakRing on.)
		// (The mend of 25 November 2026: a raid is up to four flights and the rules name the guns on the first alone, so
		// "no guns on this flight" is not "no guns built there". It is asked of the province's own works; and a strike
		// at sea has no town under it to fire.)
		static const bool bLightAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterFlakLight"));
		if (bLightAsked && Sortie.bBombers && Flight.Sortie.Flak <= 0 && Data.Regions.IsValidIndex(Sortie.Region)
			&& !(ShownBuildings.IsValidIndex(Sortie.Region) && ShownBuildings[Sortie.Region].Z > 0))
		{
			Flight.Sortie.bLightFlak = true;
		}
	}
	{
		// ("-TheaterNightRaid" flies every raid by night, for a picture of the searchlights.)
		static const bool bNightAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterNightRaid"));
		if (bNightAsked && Sortie.bBombers)
		{
			Flight.Sortie.bNight = true;
			Flight.Sortie.bEscort = false; // (no fighters go with the night bombers)
		}
		else if (CVarNightRaid.GetValueOnGameThread() == 0)
		{
			Flight.Sortie.bNight = false; // (tc.Look.NightRaid off: a raid the rules flew by night is watched by day, as before)
		}
	}
	{
		// ("-TheaterMet=FRA" has every bombing raid met by that nation's fighters, for a picture of the tracer.)
		static FString StagedMeet;
		static const bool bMeetAsked = FParse::Value(FCommandLine::Get(), TEXT("TheaterMet="), StagedMeet);
		if (bMeetAsked && !StagedMeet.IsEmpty() && Sortie.bBombers)
		{
			Flight.Sortie.Meet = StagedMeet;
		}
	}
	const bool bByNight = Flight.Sortie.bNight;
	const bool bEscorted = Flight.Sortie.bEscort;
	Flight.Began = GetWorld()->GetTimeSeconds();

	// The aircraft are the nation's own, as they stand on its pieces: its bomber in a vic of three with a pair of
	// its fighters above them if they are escorted, or four of its fighters alone. They are built once, nose
	// toward the model's own front, and then flown as one.
	FMiniatureMesh Model;
	const double U = UnitScale;
	const FString Bomber = FMiniatureMesh::MainBomberOf(Sortie.Nation), Fighter = FMiniatureMesh::MainFighterOf(Sortie.Nation);
	// (a model's front is toward -X and its right toward -Y; behind is +X)
	if (Sortie.bBombers)
	{
		const double Metre = 0.34 * U; // a heavy bomber's wings then span about ten units of the table
		Model.Vehicle(Bomber, FVector(0.0, 0.0, 0.0), Metre, 0.0, false);
		Model.Vehicle(Bomber, FVector(26.0 * Metre, 22.0 * Metre, 3.0 * Metre), Metre, 0.0, false);
		Model.Vehicle(Bomber, FVector(26.0 * Metre, -22.0 * Metre, 3.0 * Metre), Metre, 0.0, false);
		if (bEscorted)
		{
			const double Small = 0.5 * U;
			Model.Vehicle(Fighter, FVector(-6.0 * Metre, 34.0 * Metre, 16.0 * Metre), Small, 0.0, false);
			Model.Vehicle(Fighter, FVector(10.0 * Metre, -36.0 * Metre, 18.0 * Metre), Small, 0.0, false);
		}
	}
	else
	{
		const double Small = 0.55 * U;
		Model.Vehicle(Fighter, FVector(0.0, 0.0, 0.0), Small, 0.0, false);
		Model.Vehicle(Fighter, FVector(9.0 * Small, 8.0 * Small, 1.0 * Small), Small, 0.0, false);
		Model.Vehicle(Fighter, FVector(9.0 * Small, -8.0 * Small, 1.0 * Small), Small, 0.0, false);
		Model.Vehicle(Fighter, FVector(18.0 * Small, 16.0 * Small, 2.0 * Small), Small, 0.0, false);
	}
	if (Model.Vertices.Num() > 0)
	{
		Flight.Formation = NewObject<UProceduralMeshComponent>(this);
		Flight.Formation->SetupAttachment(Root);
		Flight.Formation->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Flight.Formation->RegisterComponent();
		Flight.Formation->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		Flight.Formation->SetMaterial(0, PieceMaterial);
		Flight.Formation->SetCastShadow(true);
		if (bByNight && Sortie.bBombers && BeamMaterial)
		{
			// (Ruling 107.) Each bomber again as a thin shell of light just off its skin, hidden until a searchlight holds
			// it: the bomber then shows pale, with its form still to be read, and never flat white.
			const double Metre = 0.34 * U;
			static const FVector Places[3] = { FVector(0.0, 0.0, 0.0), FVector(26.0, 22.0, 3.0), FVector(26.0, -22.0, 3.0) };
			Flight.LitInk = UMaterialInstanceDynamic::Create(BeamMaterial, this);
			for (int32 K = 0; K < 3; ++K)
			{
				FMiniatureMesh One;
				One.Vehicle(Bomber, Places[K] * Metre, Metre, 0.0, false);
				if (One.Vertices.Num() == 0)
				{
					break;
				}
				TArray<FColor> Light;
				Light.Init(FLinearColor(200.0f / 255.0f, 222.0f / 255.0f, 1.0f, 1.0f).ToFColor(false), One.Vertices.Num());
				for (int32 Point = 0; Point < One.Vertices.Num(); ++Point)
				{
					One.Vertices[Point] += One.Normals[Point] * (0.012 * U);
				}
				Flight.Formation->CreateMeshSection(1 + K, One.Vertices, One.Triangles, One.Normals, TArray<FVector2D>(), Light, TArray<FProcMeshTangent>(), false);
				Flight.Formation->SetMaterial(1 + K, Flight.LitInk);
				Flight.Formation->SetMeshSectionVisible(1 + K, false);
				Flight.LitMachines = K + 1;
			}
		}
	}
	// The defender's fighters, if any can reach: a pair of his own, that rise as the raid comes in.
	if (!Flight.Sortie.Meet.IsEmpty())
	{
		FMiniatureMesh Pair;
		const double Small = 0.55 * U;
		const FString Theirs = FMiniatureMesh::MainFighterOf(Flight.Sortie.Meet);
		Pair.Vehicle(Theirs, FVector(0.0, 0.0, 0.0), Small, 0.0, false);
		Pair.Vehicle(Theirs, FVector(10.0 * Small, 9.0 * Small, 1.5 * Small), Small, 0.0, false);
		if (Pair.Vertices.Num() > 0)
		{
			Flight.Rivals = NewObject<UProceduralMeshComponent>(this);
			Flight.Rivals->SetupAttachment(Root);
			Flight.Rivals->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Flight.Rivals->RegisterComponent();
			Flight.Rivals->CreateMeshSection(0, Pair.Vertices, Pair.Triangles, Pair.Normals, TArray<FVector2D>(), Pair.Colors, TArray<FProcMeshTangent>(), false);
			Flight.Rivals->SetMaterial(0, PieceMaterial);
			Flight.Rivals->SetCastShadow(true);
			Flight.Rivals->SetVisibility(false);
		}
	}
}

bool ACampaignMapActor::FirstFlight(FVector2D& OutSpot, float& OutHeight) const
{
	if (Flights.Num() == 0 || Flights[0].At.IsZero())
	{
		return false;
	}
	OutSpot = WorldToMap(Flights[0].At);
	OutSpot.X = LaidX(OutSpot.X); // (seam-4: a raid is flown on the eye's side of the seam; whoever asks is told its place on the chart)
	OutHeight = static_cast<float>(Flights[0].At.Z); // (the eye's focus is on the table's own level: this is how far above it)
	return true;
}

bool ACampaignMapActor::FirstRaidTarget(FVector2D& OutSpot, float& OutHeight) const
{
	if (Flights.Num() == 0 || Flights[0].At.IsZero())
	{
		return false;
	}
	OutSpot = Flights[0].Sortie.To;
	OutHeight = static_cast<float>(Flights[0].At.Z);
	return true;
}

void ACampaignMapActor::UpdateSorties()
{
	THEATER_SPEED_SCOPE(UpdateSorties);
	UWorld* World = GetWorld();
	if (!SortieMesh || !World)
	{
		return;
	}
	if (Flights.Num() == 0)
	{
		if (SortieMesh->GetNumSections() > 0)
		{
			SortieMesh->ClearAllMeshSections();
		}
		return;
	}
	const double Now = World->GetTimeSeconds();
	const double FlightLasts = 11.0;
	const double U = UnitScale;
	FCampaignMeshBuffers Courses;
	for (int32 Index = Flights.Num() - 1; Index >= 0; --Index)
	{
		FFlight& Flight = Flights[Index];
		// (held at one point of its flight while its picture is taken)
		const double Share = RaidHeldAt >= 0.0 ? RaidHeldAt : (Now - Flight.Began) / FlightLasts;
		if (Share >= 1.0)
		{
			if (Flight.Formation)
			{
				Flight.Formation->DestroyComponent();
			}
			if (Flight.Rivals)
			{
				Flight.Rivals->DestroyComponent();
			}
			Flights.RemoveAt(Index);
			continue;
		}
		// Out to the target, a turn over it, and home: the airfield is where the air groups stand, south-west of the town.
		// (seam-4: the airfield is counted in the turn of the world nearest the eye, so the whole raid, its aircraft,
		// its pencilled course and the fighters that meet it, is flown on the eye's side of the seam, where the towns'
		// twins stand under it. Only while both sides of the seam are drawn.)
		const FVector2D FieldLaid = SortieStart(Flight.Sortie);
		const FVector2D Field(OverSeam() ? NearX(FieldLaid.X, WorldToMap(LastAskedFocus).X) : FieldLaid.X, FieldLaid.Y);
		// (seam-2: the target is counted in the turn of the world nearest the airfield, so a raid over the chart's
		// seam, Alaska to the Yukon, is flown the short way and not across the whole table. Only while both sides of
		// the seam are drawn, tc.Far.OverSeam: the town it flies to stands a world on only as a twin.)
		const FVector2D Aimed(OverSeam() ? NearX(Flight.Sortie.To.X, Field.X) : Flight.Sortie.To.X, Flight.Sortie.To.Y);
		const FVector Field0 = SurfacePoint(Field.X, Field.Y, 0.3), Aim = SurfacePoint(Aimed.X, Aimed.Y, 0.3);
		const FVector Course = (Aim - Field0).GetSafeNormal2D();
		const FVector Abeam(-Course.Y, Course.X, 0.0);
		const double Leg = FVector::Dist2D(Field0, Aim);
		const double Circle = FMath::Clamp(Leg * 0.08, 3.0 * U, 9.0 * U); // the turn over the target
		FVector Where, Heading;
		double Bank = 0.0;
		const double Out = 0.44, TurnShare = 0.12;
		if (Share < Out)
		{
			const double T = FMath::SmoothStep(0.0, 1.0, Share / Out);
			Where = FMath::Lerp(Field0, Aim - Course * Circle, T);
			Heading = Course;
		}
		else if (Share < Out + TurnShare)
		{
			// Round in a half circle over the target, banked into the turn.
			const double Angle = PI * (Share - Out) / TurnShare;
			Where = Aim - Course * Circle * FMath::Cos(Angle) + Abeam * Circle * FMath::Sin(Angle) * 0.6;
			// The way it is moving: the path's own direction as it goes round.
			Heading = (Course * FMath::Sin(Angle) + Abeam * FMath::Cos(Angle) * 0.6).GetSafeNormal();
			Bank = 28.0 * FMath::Sin(Angle);
			if (!Flight.bStruck && Angle > PI * 0.4)
			{
				Flight.bStruck = true;
				if (Flight.Sortie.bBombers && Flight.Sortie.Region != INDEX_NONE)
				{
					AddBattleSmoke(Flight.Sortie.Region);
				}
			}
		}
		else
		{
			const double T = FMath::SmoothStep(0.0, 1.0, (Share - Out - TurnShare) / (1.0 - Out - TurnShare));
			Where = FMath::Lerp(Aim + Course * Circle, Field0, T);
			Heading = -Course;
		}
		// They climb away from the field, fly high, and let down again to land.
		const double Aloft = (2.5 + 9.0 * FMath::Sin(PI * FMath::Clamp(Share, 0.0, 1.0))) * U;
		if (Flight.Formation)
		{
			// The models face -X, so the formation is turned half round from the way it is heading.
			const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Heading.Y, Heading.X)) + 180.0;
			// (The modeller's flak rule and the art director's limit on it, 18 November 2026: a burst close by bumps the
			// bombers. A small jolt of a few degrees, a roll and a nod that settle in half a second; they do not hop.)
			const double Jolted = GetWorld()->GetTimeSeconds() - Flight.JoltAt;
			const double Jolt = Jolted >= 0.0 && Jolted < 0.5 ? (1.0 - FMath::SmoothStep(0.0, 0.5, Jolted)) * FMath::Cos(Jolted * 21.0) : 0.0;
			Flight.Formation->SetRelativeLocationAndRotation(Where + FVector(0.0, 0.0, Aloft), FRotator(Flight.JoltPitch * Jolt, Yaw, Bank + Flight.JoltRoll * Jolt));
		}
		Flight.At = Where + FVector(0.0, 0.0, Aloft);
		Flight.Dir = Heading;
		Flight.Share = Share;
		// The defender's fighters: up from the ground as the raid comes in, round and through the bombers while
		// it is over the target, and away again when it turns for home.
		Flight.bEngaged = false;
		if (Flight.Rivals)
		{
			// (a longer climb when they come from an airfield of their own, some way off)
			const double Meets = Out - 0.16, Parts = Out + TurnShare + 0.1, Climb = Data.Regions.IsValidIndex(Flight.Sortie.MeetFrom) ? 0.24 : 0.1;
			// They rise from their own airfield where that is known, and from under the raid where it is not.
			FVector Ground = Aim + FVector(0.0, 0.0, 1.0 * U);
			if (Data.Regions.IsValidIndex(Flight.Sortie.MeetFrom))
			{
				// (seam-4: their own field in the turn of the world the target is flown to, or they would climb to the
				// raid from a whole world away)
				const FVector2D Theirs = AirfieldSpot(Flight.Sortie.MeetFrom);
				Ground = SurfacePoint(OverSeam() ? NearX(Theirs.X, Aimed.X) : Theirs.X, Theirs.Y, 0.3) + FVector(0.0, 0.0, 1.0 * U);
			}
			const double Round = FMath::Clamp((Share - Meets) / (Parts - Meets), 0.0, 1.0) * PI * 3.0;
			const FVector Among = Flight.At + (Course * FMath::Cos(Round) + Abeam * FMath::Sin(Round)) * (6.5 * U) + FVector(0.0, 0.0, 2.2 * U * FMath::Sin(Round * 2.0));
			FVector Now3 = Among;
			bool bShown = true;
			if (Share < Meets - Climb || Share > Parts + Climb)
			{
				bShown = false;
			}
			else if (Share < Meets)
			{
				Now3 = FMath::Lerp(Ground, Among, FMath::SmoothStep(0.0, 1.0, (Share - (Meets - Climb)) / Climb));
			}
			else if (Share > Parts)
			{
				Now3 = FMath::Lerp(Among, Ground, FMath::SmoothStep(0.0, 1.0, (Share - Parts) / Climb));
			}
			else
			{
				Flight.bEngaged = true;
			}
			Flight.Rivals->SetVisibility(bShown);
			if (bShown)
			{
				const FVector Moved = Now3 - Flight.RivalAt;
				const double RivalYaw = Moved.SizeSquared2D() > 0.0001 ? FMath::RadiansToDegrees(FMath::Atan2(Moved.Y, Moved.X)) + 180.0 : 0.0;
				Flight.Rivals->SetRelativeLocationAndRotation(Now3, FRotator(0.0, RivalYaw, Flight.bEngaged ? 30.0 : 0.0));
			}
			Flight.RivalAt = Now3;
		}
		// The course of the raid, pencilled on the table from the airfield to the target while it is flown.
		const double Length = FVector2D::Distance(Field, Aimed);
		const int32 Dashes = FMath::Clamp(FMath::RoundToInt32(Length / 7.0), 2, 200);
		for (int32 Dash = 0; Dash < Dashes; ++Dash)
		{
			TArray<FVector2D> Stroke;
			Stroke.Add(FMath::Lerp(Field, Aimed, (Dash + 0.15) / Dashes));
			Stroke.Add(FMath::Lerp(Field, Aimed, (Dash + 0.75) / Dashes));
			// (The art director's round 37: close up the dashes were cream slabs 30 px wide through the middle of the
			// bombers, and by night the brightest thing after the beams. Thinned below playing height, let down by night.)
			const double Thin = FMath::Lerp(0.3, 1.0, FMath::SmoothStep(2600.0, 4600.0, static_cast<double>(SortieSeenFrom)));
			AppendRibbon(Courses, Stroke, 0.45 * Thin, 0.4, FLinearColor(1.0f, 0.86f, 0.4f, 0.75f * FMath::Lerp(1.0f, 0.4f, GetMoonlight())).ToFColor(false), false);
		}
	}
	if (!Courses.Commit(SortieMesh, 0, HighlightMaterial))
	{
		SortieMesh->ClearAllMeshSections();
	}
}

void ACampaignMapActor::UpdateSearchlights(float CameraDistance)
{
	THEATER_SPEED_SCOPE(UpdateSearchlights);
	UWorld* World = GetWorld();
	if (!BeamMesh || !World)
	{
		return;
	}
	LitBeams.Reset();
	// A night raid is watched in the dark: the hour falls to night as the bombers come in, and goes back when it is over.
	bool bNightNow = false;
	for (const FFlight& Flight : Flights)
	{
		bNightNow |= Flight.Sortie.bNight && Flight.Share > 0.05 && Flight.Share < 0.94;
	}
	if (bNightNow != bRaidNightAsked)
	{
		bRaidNightAsked = bNightNow;
		for (TActorIterator<ACampaignEnvironment> Room(World); Room; ++Room)
		{
			Room->SetRaidNight(bNightNow);
		}
	}
	if (!BeamMaterial || !bNightNow)
	{
		if (BeamMesh->GetNumSections() > 0)
		{
			BeamMesh->ClearAllMeshSections();
		}
		for (FFlight& Flight : Flights)
		{
			if (Flight.Formation && Flight.LitShown != INDEX_NONE)
			{
				Flight.Formation->SetMeshSectionVisible(1 + Flight.LitShown, false);
				Flight.LitShown = INDEX_NONE;
			}
		}
		return;
	}
	FVector Eye = GetActorLocation() + FVector(0.0, 0.0, CameraDistance), Right = FVector::RightVector, Up = FVector::UpVector;
	if (const APlayerController* Player = World->GetFirstPlayerController())
	{
		if (Player->PlayerCameraManager)
		{
			Eye = Player->PlayerCameraManager->GetCameraLocation();
			const FRotationMatrix Looking(Player->PlayerCameraManager->GetCameraRotation());
			Right = Looking.GetUnitAxis(EAxis::Y);
			Up = Looking.GetUnitAxis(EAxis::Z);
		}
	}
	const double FlightLasts = 11.0; // (as the raids are flown)
	const double U = UnitScale;
	// (how strong the light is by the hour: nothing by day, all of it once it is dark)
	const float Dark = FMath::SmoothStep(0.45f, 0.9f, NightNow);
	// (a point of the screen in the table's own measure, from playing height: nothing is ever thinner than this allows)
	const double Point = 0.00037 * FMath::Min(static_cast<double>(CameraDistance), 6500.0);
	const double Playing = FMath::SmoothStep(2600.0, 4600.0, static_cast<double>(CameraDistance));
	const FLinearColor BeamInk(200.0f / 255.0f, 222.0f / 255.0f, 1.0f), HeartInk(1.0f, 252.0f / 255.0f, 240.0f / 255.0f);
	// (what the screen shows for a light of a given strength, found by measuring pictures: see the brief's figures)
	const float Shown = 0.62f;
	FCampaignMeshBuffers Light;
	auto Ink = [](const FLinearColor& Colour, float Strength)
	{
		FLinearColor Out = Colour;
		Out.A = FMath::Clamp(Strength, 0.0f, 1.0f);
		return Out.ToFColor(false);
	};
	// A soft round light: all of it at its middle, nothing at its rim.
	auto Glow = [&](const FVector& Where, const FVector& AxisA, const FVector& AxisB, const FLinearColor& Colour, float Strength)
	{
		if (Strength <= 0.004f)
		{
			return;
		}
		const int32 Middle = Light.AddVertex(Where, FVector::UpVector, Ink(Colour, Strength));
		int32 First = INDEX_NONE, Last = INDEX_NONE;
		for (int32 K = 0; K < 12; ++K)
		{
			const double Angle = K * 2.0 * PI / 12.0;
			const int32 Here = Light.AddVertex(Where + AxisA * FMath::Cos(Angle) + AxisB * FMath::Sin(Angle), FVector::UpVector, Ink(Colour, 0.0f));
			if (K == 0)
			{
				First = Here;
			}
			else
			{
				Light.Triangles.Append({ Middle, Last, Here });
			}
			Last = Here;
		}
		Light.Triangles.Append({ Middle, Last, First });
	};
	// (one set of lights a place, however many groups fly the raid: by the most guns any of them is told of)
	TMap<int32, int32> GunsAt;
	for (const FFlight& Flight : Flights)
	{
		if (Flight.Sortie.bNight && Data.Regions.IsValidIndex(Flight.Sortie.Region))
		{
			int32& Most = GunsAt.FindOrAdd(Flight.Sortie.Region);
			Most = FMath::Max(Most, Flight.Sortie.Flak);
		}
	}
	TSet<int32> LitPlaces;
	for (FFlight& Flight : Flights)
	{
		if (!Flight.Sortie.bNight || !Data.Regions.IsValidIndex(Flight.Sortie.Region) || Flight.At.IsZero() || LitPlaces.Contains(Flight.Sortie.Region))
		{
			continue;
		}
		const int32 Region = Flight.Sortie.Region;
		LitPlaces.Add(Region);
		// (the raid's own clock: the same at every showing, so that a still of it is the same still)
		const double Time = Flight.Share * FlightLasts;
		const double Wings = (Flight.Sortie.bBombers ? 8.0 : 5.0) * U;
		const int32 Guns = FMath::Clamp(GunsAt.FindRef(Region), 0, 3);
		static const int32 LampsBy[4] = { 2, 3, 4, 6 }, HoldBy[4] = { 1, 1, 2, 3 };
		const int32 Lamps = LampsBy[Guns], Holding = HoldBy[Guns];
		// (on when the bombers are about four wingspans short, before the guns; they search two seconds after the
		// bombers turn for home, and then go out one by one)
		const double On = 0.2 * FlightLasts, Home = 0.56 * FlightLasts + 2.0;
		// (seam-4: the lamps stand round the place in the turn of the world the raid is flown in, which is the eye's side
		// of the seam: UpdateSorties. Else they would search a sky a whole world from the bombers.)
		const FVector2D Middle(OverSeam() ? NearX(Data.Regions[Region].Center.X, WorldToMap(Flight.At).X) : Data.Regions[Region].Center.X, Data.Regions[Region].Center.Y);
		const double Round = 4.6 * TownSpread(Region); // (open ground at the edge of the place, in the map's units)
		const double High = FMath::Max(11.5 * U + Flight.At.Z * 0.0, 2.0 * U); // (the bombers' height over the place, as they fly over it)
		// The bombers themselves, as they are now: the leader and the two behind it.
		TArray<FVector, TInlineAllocator<4>> Machines;
		if (Flight.Formation)
		{
			const FTransform Flown = Flight.Formation->GetComponentTransform();
			const double Metre = 0.34 * U;
			if (Flight.Sortie.bBombers)
			{
				Machines.Add(Flown.TransformPosition(FVector(0.0, 0.0, 0.0)));
				Machines.Add(Flown.TransformPosition(FVector(26.0, 22.0, 3.0) * Metre));
				Machines.Add(Flown.TransformPosition(FVector(26.0, -22.0, 3.0) * Metre));
			}
			else
			{
				Machines.Add(Flown.TransformPosition(FVector::ZeroVector));
			}
		}
		// (which bomber is held: it slips out after about three and a half seconds, and another is caught)
		const double HoldLasts = 3.6;
		const int32 HeldNow = Machines.Num() > 0 ? FMath::FloorToInt32(FMath::Max(Time - On, 0.0) / HoldLasts + (Region % 3)) % Machines.Num() : INDEX_NONE;
		const int32 HeldBefore = Machines.Num() > 0 ? (HeldNow + Machines.Num() - 1) % Machines.Num() : INDEX_NONE;
		const double SinceSwitch = FMath::Fmod(FMath::Max(Time - On, 0.0), HoldLasts);
		float HeldLight = 0.0f;
		for (int32 K = 0; K < Lamps; ++K)
		{
			FRandomStream Own(Region * 131 + K * 977 + 5);
			// The lamp: on the ground at the edge of the place, at uneven spacing.
			const double Angle = (K + Own.FRandRange(-0.28f, 0.28f)) * 2.0 * PI / Lamps + (Region % 7) * 0.6;
			const FVector2D Stand = Middle + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Round * Own.FRandRange(0.9f, 1.2f));
			const FVector Lamp = SurfacePoint(Stand.X, Stand.Y, 0.12);
			// (they strike one after another, a third of a second apart; and go out one by one, each over half a second)
			const double Struck = On + K * 0.33, Out = Home + ((K * 3) % Lamps) * 0.45;
			const float Lit = static_cast<float>(FMath::SmoothStep(0.0, 0.12, Time - Struck) * (1.0 - FMath::SmoothStep(0.0, 0.5, Time - Out))) * Dark;
			if (Lit <= 0.004f)
			{
				continue;
			}
			const double Grown = FMath::SmoothStep(0.0, 0.4, Time - Struck); // (the beam grows up from its lamp)
			// Searching: it leans its own way, and swings slowly either side of that, at its own pace.
			const double LeanTurn = Own.FRandRange(0.0f, 6.283f), Lean = FMath::DegreesToRadians(Own.FRandRange(12.0f, 22.0f));
			const double Swing = FMath::DegreesToRadians(Own.FRandRange(20.0f, 30.0f)), Pace = Own.FRandRange(5.0f, 8.0f), Step = Own.FRandRange(0.0f, 6.283f);
			const FVector LeanWay(FMath::Cos(LeanTurn), FMath::Sin(LeanTurn), 0.0), SwingWay(-FMath::Sin(LeanTurn), FMath::Cos(LeanTurn), 0.0);
			FVector Way = (FVector::UpVector + LeanWay * FMath::Tan(Lean) + SwingWay * FMath::Tan(Swing * FMath::Sin(Time * 2.0 * PI / Pace + Step))).GetSafeNormal();
			// Holding: it swings onto one bomber and follows it, while that bomber is within its reach.
			if (K < Holding && HeldNow != INDEX_NONE)
			{
				auto Toward = [&](int32 Which, float& OutReach)
				{
					const FVector To = (Machines[Which] - Lamp).GetSafeNormal();
					// (how far it must lean to hold it: the bombers fly low over a wide place, so it holds to about sixty degrees and lets go by seventy)
					OutReach = static_cast<float>(1.0 - FMath::SmoothStep(62.0, 72.0, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(To.Z, -1.0, 1.0)))));
					return To;
				};
				float ReachNow = 0.0f, ReachBefore = 0.0f;
				const FVector ToNow = Toward(HeldNow, ReachNow), ToBefore = Toward(HeldBefore, ReachBefore);
				// (it swings across from the last bomber to this one in about a second)
				const float Swung = static_cast<float>(FMath::SmoothStep(0.0, 1.0, SinceSwitch));
				const FVector Held = (ToBefore * ((1.0f - Swung) * ReachBefore) + ToNow * (Swung * ReachNow)).GetSafeNormal();
				const float Holds = FMath::Max(Swung * ReachNow, (1.0f - Swung) * ReachBefore);
				if (!Held.IsNearlyZero())
				{
					Way = FMath::Lerp(Way, Held, Holds).GetSafeNormal();
				}
				if (K == 0)
				{
					HeldLight = Swung * ReachNow * Lit * static_cast<float>(Grown);
				}
			}
			// The beam: narrow at the lamp, wider at the bombers' height, running on past it and fading with no hard end.
			const double ToHeight = High / FMath::Max(Way.Z, 0.35);
			// (As it is seen: the table is looked down on, and a beam that stands nearly upright is drawn much shorter than
			// it is. From playing height it is never under about two hundred points on the screen; nor ever more than
			// nine times the bombers' height, however nearly the eye looks along it.)
			const FVector Sight = (Lamp - Eye).GetSafeNormal();
			const double Seen = FMath::Max((Way - Sight * FVector::DotProduct(Way, Sight)).Size(), 0.2);
			// (round 37: asked for two hundred points they measured 150 to 175 px, with their upper half faded out)
			// (in true pixels of the 1600 view, whatever the height: the measure `Point` stops growing above 6,500, and
			// the raid's own picture is taken from higher than that)
			const double Long = FMath::Max(1.6 * ToHeight, FMath::Min(230.0 * (0.00037 * CameraDistance) * Playing / Seen, 9.0 * ToHeight)) * Grown;
			FLitBeam& Kept = LitBeams.AddDefaulted_GetRef();
			Kept.Lamp = Lamp;
			Kept.Way = Way;
			Kept.Long = Long;
			Kept.ToHeight = ToHeight;
			Kept.Wings = Wings;
			Kept.Strength = Lit;
			const int32 Rings = 12;
			static const double Across[7] = { -1.9, -1.5, -0.5, 0.0, 0.5, 1.5, 1.9 };
			static const float Share[7] = { 0.0f, 0.125f, 0.34f, 1.0f, 0.34f, 0.125f, 0.0f };
			int32 Before[7] = { 0, 0, 0, 0, 0, 0, 0 };
			for (int32 Ring = 0; Ring <= Rings; ++Ring)
			{
				const double Along = Long * Ring / Rings;
				const FVector At = Lamp + Way * Along;
				// (0.07 of a wingspan wide at the lamp, 0.22 at the bombers' height, 0.31 at its end; never thinner than three
				// points at the lamp nor twelve at its end)
				const double Wide = FMath::Max(Wings * (Along < ToHeight ? FMath::Lerp(0.07, 0.22, Along / ToHeight) : FMath::Lerp(0.22, 0.31, FMath::Clamp((Along - ToHeight) / FMath::Max(Long - ToHeight, 1.0), 0.0, 1.0))),
					FMath::Lerp(3.0, 12.0, static_cast<double>(Ring) / Rings) * Point);
				// (0.80 at the lamp, 0.36 at the bombers' height, nothing at the end; a little stronger from playing height;
				// and a little dust in it, which wavers slowly along its length)
				float Strong = static_cast<float>(Along < ToHeight ? FMath::Lerp(0.80, 0.36, Along / ToHeight) : FMath::Lerp(0.36, 0.0, FMath::Clamp((Along - ToHeight) / FMath::Max(Long - ToHeight, 1.0), 0.0, 1.0)));
				Strong *= static_cast<float>(FMath::Lerp(1.0, 1.15, Playing) * (1.0 + 0.14 * FMath::Sin(Along / (0.9 * Wings) - Time * 0.5 + Step))) * Lit * Shown;
				const FVector Side = FVector::CrossProduct(Way, At - Eye).GetSafeNormal();
				int32 Here[7];
				for (int32 A = 0; A < 7; ++A)
				{
					Here[A] = Light.AddVertex(At + Side * (Across[A] * Wide), FVector::UpVector, Ink(BeamInk, Strong * Share[A]));
				}
				if (Ring > 0)
				{
					for (int32 A = 0; A + 1 < 7; ++A)
					{
						Light.Triangles.Append({ Before[A], Here[A], Here[A + 1], Before[A], Here[A + 1], Before[A + 1] });
					}
				}
				FMemory::Memcpy(Before, Here, sizeof(Here));
			}
			// The lamp: a white heart inside a glow in the beam's colour, and a faint pool on the ground (the only ground a
			// searchlight lights).
			// (the lamp's heart is never under three points across, from any height)
			const double Heart = FMath::Max(0.03 * Wings, 1.5 * 0.00037 * CameraDistance), Halo = FMath::Max(0.1 * Wings, 4.0 * Point);
			Glow(Lamp + FVector(0.0, 0.0, 0.2 * U), Right * Halo, Up * Halo, BeamInk, 0.7f * Lit * Shown);
			Glow(Lamp + FVector(0.0, 0.0, 0.2 * U), Right * Heart * 1.6, Up * Heart * 1.6, HeartInk, Lit);
			Glow(Lamp + FVector(0.0, 0.0, 0.05 * U), FVector(0.3 * Wings, 0.0, 0.0), FVector(0.0, 0.3 * Wings, 0.0), BeamInk, 0.2f * Lit * Shown);
		}
		// The bomber that is held shows pale: its own shell of light, and no other's.
		if (Flight.Formation && Flight.LitMachines > 0)
		{
			const int32 Wanted = HeldLight > 0.03f && HeldNow < Flight.LitMachines ? HeldNow : INDEX_NONE;
			if (Wanted != Flight.LitShown)
			{
				if (Flight.LitShown != INDEX_NONE)
				{
					Flight.Formation->SetMeshSectionVisible(1 + Flight.LitShown, false);
				}
				if (Wanted != INDEX_NONE)
				{
					Flight.Formation->SetMeshSectionVisible(1 + Wanted, true);
				}
				Flight.LitShown = Wanted;
			}
			if (Flight.LitInk)
			{
				// (the others are about (44,48,58) by night and the one held about (162,179,205): six tenths of the beam's colour added)
				// (measured on the first pictures: at six tenths, under the beams that hold it, it came out flat white)
				Flight.LitInk->SetScalarParameterValue(TEXT("Strength"), 0.34f * HeldLight);
			}
		}
	}
	if (!Light.Commit(BeamMesh, 0, BeamMaterial))
	{
		BeamMesh->ClearAllMeshSections();
	}
	else
	{
		BeamMesh->SetCastShadow(false);
		BeamMesh->SetTranslucentSortPriority(3); // (over the ground, the town, the smoke and the bombers)
	}
}

void ACampaignMapActor::AddBattleSmoke(int32 RegionId)
{
	if (!Data.Regions.IsValidIndex(RegionId) || !GetWorld())
	{
		return;
	}
	// Between the town and where the armies stand, so that it rises clear of both.
	const FVector2D Spot = Data.Regions[RegionId].Center + FVector2D(3.5, -3.0) * TownSpread(RegionId);
	FBattleSmoke& Battle = Battles.AddDefaulted_GetRef();
	Battle.Where = SurfacePoint(Spot.X, Spot.Y, 0.05);
	Battle.Began = GetWorld()->GetTimeSeconds();
}

void ACampaignMapActor::AddSeaBattle(int32 Zone)
{
	if (!Data.Seas.IsValidIndex(Zone) || !GetWorld())
	{
		return;
	}
	// Two bursts a little apart, as two lines of ships firing at each other.
	for (const double Side : { -1.0, 1.0 })
	{
		const FVector2D Spot = Data.Seas[Zone].Center + FVector2D(5.0 * Side, 3.0 * Side);
		FBattleSmoke& Battle = Battles.AddDefaulted_GetRef();
		Battle.Where = MapToWorld(Spot.X, Spot.Y, 0.3);
		Battle.Began = GetWorld()->GetTimeSeconds() + (Side > 0.0 ? 0.35 : 0.0);
	}
}

void ACampaignMapActor::UpdateSmoke(float CameraDistance, const FVector& CameraFocus)
{
	THEATER_SPEED_SCOPE(UpdateSmoke);
	UWorld* World = GetWorld();
	if (!SmokeMesh || !World || !HighlightMaterial)
	{
		return;
	}
	// (Release 2, the far side: seam-4.) Smoke and fire are drawn afresh each frame from lists of places kept in the
	// world's own measure, each laid where its builder stood it on the chart. A chimney, a burning depot or a bombed
	// town a little way over the chart's seam is then a whole world from the eye, and none of its smoke was seen from
	// this side. Every such place is brought, here, into the turn of the world nearest what the eye looks at, as a
	// piece is (LookFrom): what smokes there has its twin on this side, or is a piece itself. With tc.Far.OverSeam
	// off they are brought back to the chart's own turn, which is where they were built; on a table with edges
	// nothing is done. (Nobody else reads these lists: their builders fill them and this draws them.)
	if (Data.bFar && Data.Charted.bIsValid && Data.WorldWidth > 0.0)
	{
		const double SeamWorld = Data.WorldWidth * UnitScale;
		const double SeamBeside = OverSeam() ? CameraFocus.Y : MapToWorld((Data.Charted.Min.X + Data.Charted.Max.X) * 0.5, 0.0).Y;
		auto Brought = [SeamWorld, SeamBeside](FVector& Place)
		{
			Place.Y += FMath::RoundToDouble((SeamBeside - Place.Y) / SeamWorld) * SeamWorld;
		};
		for (FVector& Place : TownChimneys) { Brought(Place); }
		for (FVector& Place : WorksChimneys) { Brought(Place); }
		for (FVector& Place : Smoulders) { Brought(Place); }
		for (FVector& Place : SiegeFires) { Brought(Place); }
		for (FVector& Place : PrizeFires) { Brought(Place); }
		for (FVector& Place : FrontGlow) { Brought(Place); }
		for (FFrontFire& Fire : FrontFires) { Brought(Fire.At); }
		for (FBattleSmoke& Battle : Battles) { Brought(Battle.Where); }
		for (FBombSmoke& Smoke : BombSmokes) { Brought(Smoke.Root); }
		// (THE FLAK BURSTS ARE NOT BROUGHT TO THE EYE. The art director ruled at 05:10Z that they must come round with
		// the rest, so that when the eye wraps at the chart's edge (the controller's SetWrap) or the photographer jumps,
		// the bursts in the air come round with their raid instead of hanging a world behind for the rest of their life.
		// She is right about what has to happen; the eye is the wrong thing to measure it from. A raid is NOT drawn in
		// the eye's turn: UpdateSorties counts the airfield beside the eye and then the target beside the AIRFIELD, so a
		// raid from a near field to a far target is drawn up to a world from the eye, and the guns' own fire follows the
		// raid there (RaidTurn and ToRaid below), as do the searchlights. A pass rounding bursts to the eye therefore
		// moves them off their raid and onto the twin of the bombed town, which stands within sight of the seam with no
		// bombers over it and no gun flashes under it: smoke in an empty sky, measured by a third read (eye at map x
		// 4900, field 300, target -3000: the raid is drawn at -3000 and its bursts land at 6256, about 1,356 units from
		// the eye and well inside the 1,700 a burst is drawn within).
		// So each burst is brought round to the turn of ITS OWN RAID, named by the province bombed and the province
		// flown from. That gives her what she asked for -- the eye wraps, the raid comes round a world, the bursts come
		// with it in the same frame -- and it cannot part a burst from the fire that made it. A burst whose raid has
		// gone home is left where it hangs and fades there, which is the only honest place for it.)
		for (FFlakBurst& Burst : FlakBursts)
		{
			if (Burst.Over == INDEX_NONE)
			{
				continue;
			}
			const FFlight* Maker = Flights.FindByPredicate([&Burst](const FFlight& Flier)
			{
				return Flier.Sortie.Region == Burst.Over && Flier.Sortie.From == Burst.From && !Flier.At.IsZero();
			});
			if (Maker)
			{
				Burst.At.Y += FMath::RoundToDouble((Maker->At.Y - Burst.At.Y) / SeamWorld) * SeamWorld;
			}
		}
		for (TPair<FVector, double>& Down : SetDowns) { Brought(Down.Key); }
		for (FThud& Thud : Thuds) { Brought(Thud.Where); }
		for (FVector4& Fire : ColumnFires)
		{
			Fire.Y += FMath::RoundToDouble((SeamBeside - Fire.Y) / SeamWorld) * SeamWorld;
		}
	}
	// (A puff is a middle point and two rings of eight: the inner ring lets a puff be thick across its heart and fade
	// only across its rim. With one ring a puff was thick at its very middle alone, a third as thick as it was meant
	// taken all over, and smoke built of such puffs was a haze of thin streaks.)
	const int32 MaxPuffs = 2400, Rim = 8, PerPuff = 2 * Rim + 1; // (room for the columns over bombed places, which are dense)
	// The mesh on the graphics chip, with room for so many puffs (the first so many of SmokeVertices: see the end).
	auto MakeSmokeSection = [&](int32 Room, const TArray<FVector>& Points, const TArray<FColor>& Colours)
	{
		TArray<int32> Triangles;
		Triangles.Reserve(Room * Rim * 18);
		for (int32 Each = 0; Each < Room; ++Each)
		{
			const int32 First = Each * PerPuff;
			// (both faces of every triangle, so that a puff shows whichever way it is wound to the eye)
			auto Both = [&Triangles](int32 A, int32 B, int32 C)
			{
				Triangles.Append({ A, B, C, A, C, B });
			};
			for (int32 K = 0; K < Rim; ++K)
			{
				const int32 Next = (K + 1) % Rim;
				const int32 In0 = First + 1 + K, In1 = First + 1 + Next, Out0 = First + 1 + Rim + K, Out1 = First + 1 + Rim + Next;
				Both(First, In1, In0); // the heart
				Both(In0, In1, Out1); // and the rim round it
				Both(In0, Out1, Out0);
			}
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Points.Num());
		SmokeMesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		SmokeMesh->SetMaterial(0, HighlightMaterial);
		SmokeRoom = Room;
	};
	if (!bSmokeMade)
	{
		// The mesh is made once, with room for every puff; after that only its points and colours change.
		bSmokeMade = true;
		SmokeVertices.Init(FVector::ZeroVector, MaxPuffs * PerPuff);
		// (two far corners to begin with, so that the mesh is never thought too small to draw)
		SmokeVertices[1] = FVector(-20000.0, -20000.0, 0.0);
		SmokeVertices[2] = FVector(20000.0, 20000.0, 2000.0);
		SmokeColours.Init(FColor(0, 0, 0, 0), MaxPuffs * PerPuff);
		MakeSmokeSection(MaxPuffs, SmokeVertices, SmokeColours);
		SmokeMesh->SetCastShadow(false);
	}

	// Which way the eye is looking, to turn every puff to face it.
	FVector Right = FVector::RightVector, Up = FVector::UpVector;
	// (and where the eye is, with how many pixels of the window a thing of one unit takes at one unit's distance:
	// for what must never be drawn smaller than a few pixels, a tracer)
	FVector EyeAt = CameraFocus + FVector(0.0, 0.0, CameraDistance);
	double PixelsAtOne = 1500.0;
	FRotator EyeFacing(-60.0, 0.0, 0.0);
	float EyeFieldOfView = 40.0f;
	int32 EyeWindowWide = 1600;
	int32 EyeDrawnWide = 1600; // (how many pixels wide the scene is really drawn, before the upscaler fills the window)
	int32 EyeWindowHigh = 1000, EyeDrawnHigh = 1000; // (and how high: the kit's pixel rules are exact in a tall window with it)
	if (const APlayerController* Player = World->GetFirstPlayerController())
	{
		if (Player->PlayerCameraManager)
		{
			const FRotationMatrix Looking(Player->PlayerCameraManager->GetCameraRotation());
			Right = Looking.GetUnitAxis(EAxis::Y);
			Up = Looking.GetUnitAxis(EAxis::Z);
			EyeAt = Player->PlayerCameraManager->GetCameraLocation();
			EyeFacing = Player->PlayerCameraManager->GetCameraRotation();
			EyeFieldOfView = Player->PlayerCameraManager->GetFOVAngle();
			int32 WindowWide = 0, WindowHigh = 0;
			Player->GetViewportSize(WindowWide, WindowHigh);
			if (WindowWide > 0)
			{
				EyeWindowWide = WindowWide;
				EyeWindowHigh = FMath::Max(WindowHigh, 1);
				PixelsAtOne = WindowWide / (2.0 * FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(EyeFieldOfView), 5.0, 150.0) * 0.5)));
			}
		}
	}
	{
		// (The effects artist, 14:02Z on 19 November 2026: the kit's pixel rules are to count the pixels the scene is
		// really drawn at. The map is drawn to a budget and filled out to the window by Unreal's upscaler: 1072 of a
		// picture run's 1600, about 1080 of Brian's 2881. The table's own puffs keep the window's pixels they were
		// tuned in.)
		static IConsoleVariable* const KitDrawnPercent = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
		static IConsoleVariable* const KitSecondPercent = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport"));
		const float KitFirstShare = KitDrawnPercent ? KitDrawnPercent->GetFloat() : 100.0f, KitNextShare = KitSecondPercent ? KitSecondPercent->GetFloat() : 100.0f;
		const float KitDrawnShare = (KitFirstShare > 0.0f ? FMath::Min(KitFirstShare, 100.0f) : 100.0f) / 100.0f * (KitNextShare > 0.0f ? FMath::Min(KitNextShare, 100.0f) : 100.0f) / 100.0f;
		EyeDrawnWide = FMath::Max(FMath::RoundToInt32(EyeWindowWide * KitDrawnShare), 64);
		EyeDrawnHigh = FMath::Max(FMath::RoundToInt32(EyeWindowHigh * KitDrawnShare), 40);
	}
	const double Now = World->GetTimeSeconds();
	const double Unit = UnitScale * 2.1;
	const FVector Wind(0.25, 0.9, 0.0); // the wind is from the west
	int32 Puff = 0;
	int32 FlakPicture = 0; // (and how many of the flak's painted shapes)
	int32 FlakLitShapes = 0; // (and how many of those again as light, inside a searchlight's beam)
	// (what the hour does to smoke: nothing by day, about a third and bluer in the deep of the night)
	const FLinearColor SmokeHour = FMath::Lerp(FLinearColor(1.0f, 1.0f, 1.0f), FLinearColor(0.30f, 0.38f, 0.58f), GetMoonlight());
	if (Blasts.IsValid() && Blasts->IsReady())
	{
		// (the effects kit's test barrage, round whatever the eye looks at: laid again when the eye has gone thirty
		// units from where it was laid. The camera's jolt it returns is not used here: the campaign's eye is the
		// player's own.)
		// ("-TheaterBlastSlow=8" plays the barrage at an eighth of its pace, so that stills four seconds apart show it
		// half a second apart. "-TheaterBlastOff=40,90" lays it so that the shot the kit places 40 metres north and
		// 90 east of its middle falls where the eye looks: the bomb. Both are for the test frames.)
		// ("-TheaterBlastHold=4.033" stops the barrage's clock that many of its own seconds after it is laid: the shell
		// that lands at 4.0 is then held at its second frame for a still. With the slow pace the clock comes to the
		// moment in steps of two thousandths of a second.)
		// ("-TheaterBlastFrames=4.0" shows the shot the kit fires at 4.0 a frame at a time: the clock runs to half a frame
		// short of it and then goes on one sixtieth of a second every five seconds, saying in the log which frame it
		// stands at. Frame 1 is the shot's first; the stills taken meanwhile are found by their place in the log.)
		static double BlastFrames = -1.0, BlastLaidWall = 0.0;
		static int32 BlastFrameSaid = -1;
		static double BlastSlow = 1.0, BlastHold = -1.0, BlastLaidAt = 0.0;
		static FVector BlastOff = FVector::ZeroVector;
		static bool bBlastAsked = false;
		if (!bBlastAsked)
		{
			bBlastAsked = true;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastSlow="), BlastSlow);
			FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastHold="), BlastHold);
			FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastFrames="), BlastFrames);
			FString Asked;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastOff="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) >= 2)
			{
				// (a third number lowers the whole barrage: it is read where the kit is begun)
				BlastOff = FVector(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), 0.0);
			}
		}
		double BlastNow = Now / FMath::Max(BlastSlow, 0.01);
		bool bBlastTicked = false;
		if (FVector::DistSquared2D(BlastTestAt, CameraFocus) > FMath::Square(30.0 * UnitScale))
		{
			BlastTestAt = CameraFocus;
			BlastLaidAt = BlastNow;
			BlastLaidWall = Now;
			BlastFrameSaid = -1;
			UE_LOG(LogTheater, Display, TEXT("Effects: the barrage is laid; the eye is %.0f units from what it looks at, pitched %.1f degrees, its view %.1f degrees; the window is %d pixels wide and the scene is drawn %d wide, which is the width the kit is told; a metre is %.2f units"),
				FVector::Distance(EyeAt, CameraFocus), EyeFacing.Pitch, EyeFieldOfView, EyeWindowWide, EyeDrawnWide, BlastUnitsAMetre);
			const FVector2D Under = WorldToMap(CameraFocus - BlastOff * BlastUnitsAMetre);
			Blasts->Clear();
			Blasts->Tick(BlastNow, EyeAt, EyeFacing, EyeFieldOfView, EyeDrawnWide, SmokeHour, EyeDrawnHigh); // (its clock first: the barrage is timed from it)
			// (The art director's ruling of 19 November 2026: the test's tank gun and machine gun are told the ground
			// they stand on, where they were told "earth" whatever it was. The kit's own kinds, read where the look's
			// focus stands from what the land painter laid there: standing snow; by the province a marsh or a town; a
			// wood (the bare ground's picture keeps how wooded it is); dry country, which is the painter's desert
			// mask, a desert province, or bare ground that is itself pale and tan, as the dry plains beyond Europe are
			// (their dryness is in their colour, not in the mask); anything else is earth. What was read is said in
			// the log beside the kind. "-TheaterBlastGround=sand" names it outright.
			// The first reading, on build 1748, took the terrain's land cover for its woods and called Idaho a wood.)
			FString BlastGround = TEXT("earth");
			{
				const FVector2D GunAt = WorldToMap(CameraFocus);
				const double GunX = WrapX(GunAt.X);
				const FCampaignTerrain* Shape = &Terrain;
				const FCampaignGround* Laid = &Paint;
				if (Data.bWorld && (GunX < 0.0 || GunAt.Y < 0.0 || GunX > Data.Width || GunAt.Y > Data.Height))
				{
					const FFineSheet* Fine = FineSheetAt(GunX, GunAt.Y);
					const bool bFine = Fine && Fine->bBuilt.load();
					Shape = bFine ? &Fine->Shape : &WorldTerrain;
					Laid = bFine ? &Fine->Paint : &WorldPaint;
				}
				FColor Mask(0, 0, 0, 0), Bare(0, 0, 0, 0);
				const int32 Points = Shape->NX * Shape->NY;
				if (Shape->NX > 1 && Shape->NY > 1 && Laid->Masks.Num() == Points && Laid->Ground.Num() == Points)
				{
					const int32 MaskI = FMath::Clamp(FMath::RoundToInt32((GunX - Shape->OriginX) / Shape->Cell), 0, Shape->NX - 1);
					const int32 MaskJ = FMath::Clamp(FMath::RoundToInt32((GunAt.Y - Shape->OriginY) / Shape->Cell), 0, Shape->NY - 1);
					Mask = Laid->Masks[Shape->Index(MaskI, MaskJ)];
					Bare = Laid->Ground[Shape->Index(MaskI, MaskJ)];
				}
				const int32 Province = RegionAtWorld(CameraFocus);
				const FString Lie = Data.Regions.IsValidIndex(Province) ? Data.Regions[Province].Terrain : FString();
				const float Bright = 0.299f * Bare.R + 0.587f * Bare.G + 0.114f * Bare.B;
				const bool bPaleTan = Bright >= 135.0f && Bare.R - Bare.B >= 45;
				BlastGround = Mask.G > 140 ? TEXT("snow") : Lie == TEXT("marsh") ? TEXT("mud") : Lie == TEXT("urban") ? TEXT("town") : Bare.A > 128 ? TEXT("woods")
					: (Mask.A > 96 || Lie == TEXT("desert") || bPaleTan) ? TEXT("sand") : TEXT("earth");
				FString Named;
				if (FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastGround="), Named) && !Named.IsEmpty())
				{
					BlastGround = Named;
				}
				UE_LOG(LogTheater, Display, TEXT("Effects: the test's guns stand on %s (the province is %s; the bare ground there is %d,%d,%d, wooded %d of 255; the painter's masks: rock %d, snow %d, shore sand %d, desert %d of 255)"),
					*BlastGround, Lie.IsEmpty() ? TEXT("none") : *Lie, Bare.R, Bare.G, Bare.B, Bare.A, Mask.R, Mask.G, Mask.B, Mask.A);
			}
			// (The VFX artist's hook, 20 November 2026: a heavy gun's dust takes the colour of the ground as the SCREEN
			// shows it. On the campaign's map that colour is not in the bare ground's picture (Idaho's bare paint is
			// 106,108,86 and the screen shows 176,159,117: the dry range is laid over it by the land's material), so
			// for the test rig it is measured off a still and given by a switch: "-TheaterBlastGroundColour=176,159,117",
			// pixel values, the same for every shot of the test. Without the switch the kit is told nothing and keeps
			// its named ground's paint. A real battle's scene feeds the hook from its own ground.)
			{
				FString SeenAsked;
				TArray<FString> SeenParts;
				if (FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastGroundColour="), SeenAsked, false) && SeenAsked.ParseIntoArray(SeenParts, TEXT(",")) >= 3)
				{
					const FLinearColor Seen(FCString::Atof(*SeenParts[0]) / 255.0f, FCString::Atof(*SeenParts[1]) / 255.0f, FCString::Atof(*SeenParts[2]) / 255.0f, 1.0f);
					Blasts->GroundColour = [Seen](const FVector&, FLinearColor& Out) { Out = Seen; return true; };
					UE_LOG(LogTheater, Display, TEXT("Effects: the test tells the kit the ground is seen as %s,%s,%s (by switch)"), *SeenParts[0], *SeenParts[1], *SeenParts[2]);
				}
			}
			Blasts->RunTest(SurfacePoint(Under.X, Under.Y, 0.0), BlastGround);
			bBlastTicked = true; // (the kit is ticked once a frame: a second tick in this frame cut its lamps short)
			{
				// (For the effects artist, 23:04Z on 18 November 2026: the near half of the test machine gun's tracer did
				// not show. The kit's test gun stands 36 m south and 10 west of the middle, 0.6 m up, and fires at a
				// point 20 m north and 20 east of the middle and 0.5 m over the middle's own height, which is not put
				// on the ground. The log says how far over the ground that line runs along its 63 m.)
				static double LoweredMetres = 0.0;
				static bool bLowAsked = false;
				if (!bLowAsked)
				{
					bLowAsked = true;
					FString Asked;
					TArray<FString> Parts;
					if (FParse::Value(FCommandLine::Get(), TEXT("TheaterBlastOff="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 3)
					{
						LoweredMetres = FCString::Atod(*Parts[2]);
					}
				}
				const FVector Middle = SurfacePoint(Under.X, Under.Y, 0.0);
				auto GroundUnder = [this](const FVector& Flat)
				{
					const FVector2D OnMap = WorldToMap(Flat);
					return SurfacePoint(OnMap.X, OnMap.Y, 0.0) - FVector(0.0, 0.0, LoweredMetres * BlastUnitsAMetre);
				};
				const FVector Gun = GroundUnder(Middle + FVector(-36.0, -10.0, 0.0) * BlastUnitsAMetre) + FVector(0.0, 0.0, 0.6 * BlastUnitsAMetre);
				const FVector Target = Middle + FVector(20.0, 20.0, 0.5) * BlastUnitsAMetre;
				FString Said;
				for (const double Along : { 0.0, 10.0, 21.0, 32.0, 42.0, 53.0, 63.0 })
				{
					const FVector On = FMath::Lerp(Gun, Target, FMath::Clamp(Along / 63.5, 0.0, 1.0));
					Said += FString::Printf(TEXT(" at %.0f m %.2f m over the ground;"), Along, (On.Z - GroundUnder(On).Z) / BlastUnitsAMetre);
				}
				UE_LOG(LogTheater, Display, TEXT("Effects: the test machine gun's line of fire:%s the middle's ground is %.2f m above the gun's"), *Said,
					(Middle.Z - (Gun.Z - 0.6 * BlastUnitsAMetre) - LoweredMetres * BlastUnitsAMetre) / BlastUnitsAMetre);
				// (For the effects artist, 14:03Z on 19 November 2026: brass rests a centimetre over the ground the kit
				// is given, which is the map's height function, read smoothly between the grid points of the sheet under
				// it. The land is drawn as flat triangles between those points. How far the two part at the gun's place,
				// by either way a cell can be cut in two; and how far the gun's ground stands over the table's flat plane,
				// which is what the eye turns about.)
				{
					const FVector2D GunOnMap = WorldToMap(Gun);
					const double GunX = WrapX(GunOnMap.X);
					const FCampaignTerrain* GunShape = &Terrain;
					if (Data.bWorld && (GunX < 0.0 || GunOnMap.Y < 0.0 || GunX > Data.Width || GunOnMap.Y > Data.Height))
					{
						const FFineSheet* GunSheet = FineSheetAt(GunX, GunOnMap.Y);
						GunShape = GunSheet ? &GunSheet->Shape : nullptr;
					}
					if (GunShape && GunShape->NX > 1 && GunShape->NY > 1 && GunShape->Cell > 0.0 && GunShape->Height.Num() == GunShape->NX * GunShape->NY)
					{
						const double AlongCells = FMath::Clamp((GunX - GunShape->OriginX) / GunShape->Cell, 0.0, static_cast<double>(GunShape->NX - 1));
						const double DownCells = FMath::Clamp((GunOnMap.Y - GunShape->OriginY) / GunShape->Cell, 0.0, static_cast<double>(GunShape->NY - 1));
						const int32 CellI = FMath::Min(FMath::FloorToInt32(AlongCells), GunShape->NX - 2), CellJ = FMath::Min(FMath::FloorToInt32(DownCells), GunShape->NY - 2);
						const double InU = AlongCells - CellI, InV = DownCells - CellJ;
						auto CornerOf = [GunShape](int32 I, int32 J) { return FMath::Max(static_cast<double>(GunShape->Height[GunShape->Index(I, J)]), 0.0); };
						const double H00 = CornerOf(CellI, CellJ), H10 = CornerOf(CellI + 1, CellJ), H01 = CornerOf(CellI, CellJ + 1), H11 = CornerOf(CellI + 1, CellJ + 1);
						const double CutOne = InU >= InV ? H00 + InU * (H10 - H00) + InV * (H11 - H10) : H00 + InU * (H11 - H01) + InV * (H01 - H00);
						const double CutOther = InU + InV <= 1.0 ? H00 + InU * (H10 - H00) + InV * (H01 - H00) : H11 + (1.0 - InU) * (H01 - H11) + (1.0 - InV) * (H10 - H11);
						const double Read = FMath::Max(static_cast<double>(GroundAt(GunOnMap.X, GunOnMap.Y)), 0.0);
						// (one unit of the map's height, in the kit's metres)
						const double MetresAUnit = (MapToWorld(GunOnMap.X, GunOnMap.Y, 1.0).Z - MapToWorld(GunOnMap.X, GunOnMap.Y, 0.0).Z) / BlastUnitsAMetre;
						UE_LOG(LogTheater, Display, TEXT("Effects: the test gun's ground stands %.3f m over the table's flat plane by the height function (the eye's focus is %.3f m over it); the sheet's grid points are %.2f map units apart, %.2f m; flat triangles between them would stand %+.3f m from the function at the gun cut one way and %+.3f m cut the other; the cell's corners stand at %.3f, %.3f, %.3f and %.3f m"),
							Read * MetresAUnit, (CameraFocus.Z - MapToWorld(GunOnMap.X, GunOnMap.Y, 0.0).Z) / BlastUnitsAMetre, GunShape->Cell, GunShape->Cell * UnitScale / BlastUnitsAMetre,
							(CutOne - Read) * MetresAUnit, (CutOther - Read) * MetresAUnit, H00 * MetresAUnit, H10 * MetresAUnit, H01 * MetresAUnit, H11 * MetresAUnit);
					}
					else
					{
						UE_LOG(LogTheater, Display, TEXT("Effects: no sheet of the land stands under the test gun yet; its ground cannot be compared"));
					}
				}
				// (For the effects artist, 16:29Z on 19 November 2026: "put a gun and crew model at the muzzle on the next MG
				// stills, because the gun-smoke wisp only shows against something darker than pale ground." Two of the
				// game's own riflemen in field grey stand at the test gun, one each side of its line of fire and a little
				// behind the muzzle, at the test's own scale. "-TheaterBlastBare" leaves the ground bare as it was.)
				{
					static TWeakObjectPtr<UProceduralMeshComponent> BlastCrew;
					if (UProceduralMeshComponent* Old = BlastCrew.Get())
					{
						Old->DestroyComponent();
					}
					BlastCrew.Reset();
					if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterBlastBare")))
					{
						const FVector Fire = (Target - Gun).GetSafeNormal2D();
						const FVector Aside(-Fire.Y, Fire.X, 0.0);
						const FVector Foot = Gun - FVector(0.0, 0.0, 0.6 * BlastUnitsAMetre);
						FMiniatureMesh Crew;
						Crew.Soldier(TEXT("GER"), GroundUnder(Foot - Fire * (0.7 * BlastUnitsAMetre) - Aside * (0.45 * BlastUnitsAMetre)), BlastUnitsAMetre, Fire.Rotation().Yaw);
						Crew.Soldier(TEXT("GER"), GroundUnder(Foot - Fire * (1.1 * BlastUnitsAMetre) + Aside * (0.55 * BlastUnitsAMetre)), BlastUnitsAMetre, Fire.Rotation().Yaw + 20.0);
						if (Crew.Vertices.Num() > 0)
						{
							UProceduralMeshComponent* Stood = NewObject<UProceduralMeshComponent>(this);
							Stood->SetupAttachment(Root);
							Stood->SetCollisionEnabled(ECollisionEnabled::NoCollision);
							Stood->RegisterComponent();
							Stood->CreateMeshSection(0, Crew.Vertices, Crew.Triangles, Crew.Normals, TArray<FVector2D>(), Crew.Colors, TArray<FProcMeshTangent>(), false);
							Stood->SetMaterial(0, PieceMaterial ? PieceMaterial : SceneryMaterial); // (the pieces' paint: they are figures, and take the figures' light)
							BlastCrew = Stood;
							UE_LOG(LogTheater, Display, TEXT("Effects: two riflemen stand at the test gun (%d points)"), Crew.Vertices.Num());
						}
					}
				}
			}
		}
		if (BlastHold >= 0.0)
		{
			BlastNow = FMath::Min(BlastNow, BlastLaidAt + BlastHold);
		}
		if (BlastFrames >= 0.0)
		{
			const double Elapsed = Now - BlastLaidWall, Runs = BlastFrames - 1.0 / 120.0;
			const int32 Frame = Elapsed > Runs ? FMath::FloorToInt32((Elapsed - Runs) / 5.0) : -1;
			BlastNow = BlastLaidAt + (Frame < 0 ? Elapsed : Runs + Frame / 60.0);
			if (Frame != BlastFrameSaid)
			{
				BlastFrameSaid = Frame;
				UE_LOG(LogTheater, Display, TEXT("Effects: the barrage stands at frame %d of its shot at %.1f s"), Frame, BlastFrames);
			}
			else if (Frame >= 0)
			{
				// (The kit's lamps and its shortest flashes go by the frames it draws, not by its clock: a lamp is full in
				// the first frame drawn after its flash is born, half in the next, then out. So each frame of the shot is
				// drawn once and then stands until the next: a still shows what that one frame of play would show. Drawn
				// over and over at one moment, as it was, a lamp was out long before the still was taken.)
				bBlastTicked = true;
			}
		}
		if (!bBlastTicked)
		{
			Blasts->Tick(BlastNow, EyeAt, EyeFacing, EyeFieldOfView, EyeDrawnWide, SmokeHour, EyeDrawnHigh);
		}
	}
	bool bDrawingLight = false;
	// One puff: its middle, how far out its heart reaches (as a share of the whole), and how thick it is at its
	// middle, at the heart's edge and at its rim. The two axes are its half-length and half-width as the eye sees it.
	auto DrawPuff = [&](const FVector& Where, const FVector& AxisA, const FVector& AxisB, const FLinearColor& Colour, float AtMiddle, double HeartOut, float AtHeart, float AtRim)
	{
		if (Puff >= MaxPuffs || FMath::Max3(AtMiddle, AtHeart, AtRim) <= 0.004f)
		{
			return;
		}
		const int32 First = Puff * PerPuff;
		// (The art director's round 36, the night rule: by dusk and by night smoke of every kind takes the hour's light
		// as the bombers do, about half and bluer, and is never brighter than the lit ground beside it. Fire, flashes
		// and tracer are lights and keep their own: a light is known here by its colour, warm or very bright.)
		const float Warm = FMath::Clamp((Colour.R - 2.2f * Colour.B - 0.03f) / 0.2f, 0.0f, 1.0f);
		const float Bright = FMath::Clamp((0.3f * Colour.R + 0.59f * Colour.G + 0.11f * Colour.B - 0.75f) / 0.15f, 0.0f, 1.0f);
		// (bDrawingLight: what is being drawn is a light whatever its colour, a yellow tracer)
		const FLinearColor HourLight = bDrawingLight ? FLinearColor(1.0f, 1.0f, 1.0f) : FMath::Lerp(SmokeHour, FLinearColor(1.0f, 1.0f, 1.0f), FMath::Max(Warm, Bright));
		const FLinearColor Lit(Colour.R * HourLight.R, Colour.G * HourLight.G, Colour.B * HourLight.B);
		auto Inked = [&Lit](float Alpha) { FLinearColor Ink = Lit; Ink.A = Alpha; return Ink.ToFColor(false); };
		const FColor MiddleInk = Inked(AtMiddle), HeartInk = Inked(AtHeart), RimInk = Inked(AtRim);
		SmokeVertices[First] = Where;
		SmokeColours[First] = MiddleInk;
		for (int32 K = 0; K < Rim; ++K)
		{
			const double Angle = K * 2.0 * PI / Rim;
			const FVector Out = AxisA * FMath::Cos(Angle) + AxisB * FMath::Sin(Angle);
			SmokeVertices[First + 1 + K] = Where + Out * HeartOut;
			SmokeColours[First + 1 + K] = HeartInk;
			SmokeVertices[First + 1 + Rim + K] = Where + Out;
			SmokeColours[First + 1 + Rim + K] = RimInk;
		}
		++Puff;
	};
	// (A soft puff: thick at its middle and nothing at its rim, thinning evenly between. One alone is a soft glow or a
	// soft shadow, never a body.)
	auto Draw = [&](const FVector& Where, double Radius, const FLinearColor& Colour, float Alpha)
	{
		DrawPuff(Where, Right * Radius, Up * Radius, Colour, Alpha, 0.5, 0.5f * Alpha, 0.0f);
	};
	// (A firm one, for a ball of smoke that is to read as a thing: a core that is still half as thick at its own edge,
	// inside a soft one that gives it its outline.)
	auto DrawFirm = [&](const FVector& Where, double Radius, const FLinearColor& Colour, float Alpha)
	{
		if (Puff + 1 >= MaxPuffs || Alpha <= 0.004f)
		{
			return;
		}
		DrawPuff(Where, Right * (Radius * 0.72), Up * (Radius * 0.72), Colour, Alpha, 0.5, 0.775f * Alpha, 0.55f * Alpha);
		Draw(Where, Radius, Colour, 0.8f * Alpha);
	};
	// (A puff drawn out along a line, as the wind draws smoke out: longer one way than the other, the line taken as the
	// eye sees it. Thick across its heart, which reaches the given share of the way out, and fading to nothing over its rim.)
	auto DrawSmear = [&](const FVector& Where, const FVector& Along, double HalfLong, double HalfWide, const FLinearColor& Colour, float Alpha, double HeartOut = 0.5, float AtHeart = -1.0f)
	{
		FVector Seen = Right * FVector::DotProduct(Along, Right) + Up * FVector::DotProduct(Along, Up);
		Seen = Seen.IsNearlyZero() ? Right : Seen.GetSafeNormal();
		const FVector Across = (Right * -FVector::DotProduct(Seen, Up) + Up * FVector::DotProduct(Seen, Right)).GetSafeNormal();
		DrawPuff(Where, Seen * HalfLong, Across * HalfWide, Colour, Alpha, HeartOut, AtHeart < 0.0f ? static_cast<float>(1.0 - HeartOut) * Alpha : AtHeart, 0.0f);
	};

	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterSmokeTest")))
	{
		Draw(CameraFocus + FVector(0.0, 0.0, 120.0), 300.0, FLinearColor(1.0f, 0.5f, 0.1f), 0.9f); // one great puff, to see that puffs draw at all
		Draw(CameraFocus + FVector(500.0, 0.0, 120.0), 200.0, FLinearColor(0.03f, 0.03f, 0.03f), 0.9f); // and a black one beside it, to see that smoke can darken
	}
	// Chimneys: a thin grey plume from each, leaning with the wind. Only those near what the eye is looking at.
	const FLinearColor Grey(0.6f, 0.6f, 0.62f);
	const double Reach = FMath::Min(CameraDistance * 1.4 + 1500.0, 9000.0);
	auto Plume = [&](const FVector& Top, int32 Seed, double Scale = 1.0, const FLinearColor* Ink = nullptr, float Strength = 1.0f)
	{
		if (FVector::DistSquared2D(Top, CameraFocus) > Reach * Reach)
		{
			return;
		}
		const int32 Count = 6;
		const double Life = 7.0;
		const double Big = Unit * Scale;
		for (int32 K = 0; K < Count; ++K)
		{
			const double Age = FMath::Frac(Now / Life + static_cast<double>(K) / Count + Seed * 0.137);
			const double Sway = FMath::Sin(Now * 0.7 + Seed + K * 1.7) * 0.12 * Big * Age;
			const FVector At = Top + FVector(0.0, 0.0, 4.0 * Big * Age) + Wind * (2.8 * Big * Age * Age) + Right * Sway;
			const float Alpha = 0.55f * Strength * FMath::SmoothStep(0.0, 0.1, Age) * FMath::Pow(1.0 - Age, 1.3);
			Draw(At, (0.22 + 1.1 * Age) * Big, Ink ? *Ink : Grey, Alpha);
		}
	};
	// A ship going down burns as she heels over: dark smoke from her, thinning as she goes under. (The art director
	// asked for the works' plume at about half its size. A ship is drawn far larger than a works, and at that size
	// the plume was three points of the screen from working height, where it is wanted: it is sized to the ship.)
	for (int32 K = 0; K < SinkingShips.Num(); ++K)
	{
		const FSinkingShip& Ship = SinkingShips[K];
		const float Burning = static_cast<float>(FMath::SmoothStep(0.0, 0.12, Ship.T) * (1.0 - FMath::SmoothStep(0.7, 1.05, Ship.T)));
		if (Burning > 0.01f)
		{
			const FLinearColor Oily(0.055f, 0.05f, 0.05f);
			// (two plumes run together, so that the smoke is a column and not a string of puffs)
			Plume(Ship.At + FVector(0.0, 0.0, Ship.High * 0.5), K * 11 + 700, Ship.Long / (Unit * 7.5), &Oily, 2.2f * Burning);
			Plume(Ship.At + FVector(0.0, 0.0, Ship.High * 0.5), K * 11 + 733, Ship.Long / (Unit * 7.5), &Oily, 2.2f * Burning);
		}
	}
	// The wrecks of a battle lately fought still burn: dark smoke from each.
	{
		const FLinearColor Oily(0.13f, 0.12f, 0.115f);
		for (int32 K = 0; K < Smoulders.Num(); ++K)
		{
			Plume(Smoulders[K], K * 7 + 300, 1.7, &Oily);
		}
		// By night the guns are seen along every front where armies at war stand face to face: short flashes that
		// come and go along the frontier between them. And the ground fought over this month still glows.
		if (NightNow > 0.25f)
		{
			const float Dark = FMath::SmoothStep(0.25f, 0.8f, NightNow);
			const FLinearColor Flash(1.0f, 0.82f, 0.5f), Ember(1.0f, 0.34f, 0.09f);
			// A fire has a near-white yellow heart, a body of deep orange, and (the great ones) red-brown about it.
			const FLinearColor Heart(1.0f, 0.88f, 0.63f), Body(1.0f, 0.47f, 0.16f), Rust(0.66f, 0.2f, 0.06f);
			// (seen from high up a fire is drawn larger than life, or the front would not be seen at all)
			const double Seen = FMath::Clamp(CameraDistance / (Unit * 480.0), 0.34, 1.3);
			const float Soft = static_cast<float>(FMath::GetMappedRangeValueClamped(FVector2D(0.34, 1.0), FVector2D(0.7, 1.0), Seen));
			// One faint line of glow under it all: from high up the front is one line, while its fires stay apart.
			for (int32 K = 0; K < FrontGlow.Num(); ++K)
			{
				if (FVector::DistSquared2D(FrontGlow[K], CameraFocus) <= Reach * Reach)
				{
					Draw(FrontGlow[K] + FVector(0.0, 0.0, 1.0 * Unit), 2.7 * Unit, Ember, FrontLineGlow * Dark);
				}
			}
			// The fires that burn along the line all night, each flickering; and the flashes of the guns among them.
			for (int32 K = 0; K < FrontFires.Num(); ++K)
			{
				const FFrontFire& Fire = FrontFires[K];
				if (FVector::DistSquared2D(Fire.At, CameraFocus) > Reach * Reach)
				{
					continue;
				}
				const float Flicker = 0.74f + 0.26f * FMath::Sin(static_cast<float>(Now) * (2.1f + (K % 5) * 0.37f) + K * 1.9f);
				const double Big = Fire.Size * Unit * Seen;
				const FVector At = Fire.At + FVector(0.0, 0.0, Big * 0.7);
				if (Fire.Heat < 0.9f)
				{
					Draw(At, Big, Ember, 0.6f * Flicker * Dark); // an ember: small, red, no heart
					continue;
				}
				if (Fire.Size > 1.2f)
				{
					Draw(At, Big * 1.9, Rust, 0.34f * Dark);
				}
				Draw(At, Big, Body, 0.9f * Flicker * Dark * Soft);
				Draw(At, Big * 0.45, Heart, Flicker * Dark);
			}
			const int32 Guns = FMath::Min(FrontFires.Num() / 4, 40);
			for (int32 Gun = 0; Gun < Guns; ++Gun)
			{
				const double Beat = Now * (0.55 + 0.13 * (Gun % 4)) + Gun * 0.61;
				const double Phase = FMath::Frac(Beat);
				if (Phase > 0.09)
				{
					continue;
				}
				FRandomStream Where(static_cast<int32>(FMath::FloorToDouble(Beat)) * 31 + Gun * 7);
				const FVector At = FrontFires[Where.RandRange(0, FrontFires.Num() - 1)].At + FVector(Where.FRandRange(-2.0f, 2.0f), Where.FRandRange(-2.0f, 2.0f), 0.1) * Unit;
				const double Burst = (0.9 + 5.0 * Phase) * Unit * Seen;
				Draw(At + FVector(0.0, 0.0, Burst * 0.7), Burst, Flash, 0.9f * Dark * static_cast<float>(1.0 - Phase / 0.09));
			}
			// (The art director's mend to ruling 205, 18 November 2026: an old battlefield gives off no light of its own.
			// The wrecks still smouldering each laid an ember's glow three units wide on the ground, and from playing
			// height a month-old battle was nearly as bright by night as a town on fire. Their smoke still rises; the
			// craters and the wrecks take the moon as the ground beside them does. "-TheaterScorchGlow" brings it back.)
			static const bool bScorchGlow = FParse::Param(FCommandLine::Get(), TEXT("TheaterScorchGlow"));
			for (int32 K = 0; bScorchGlow && K < Smoulders.Num(); ++K)
			{
				Draw(Smoulders[K] + FVector(0.0, 0.0, 0.8 * Unit), 3.2 * Unit, Ember, 0.2f * Dark * (0.8f + 0.2f * FMath::Sin(static_cast<float>(Now) * 1.7f + K)));
			}
		}
		// And a pall hangs over every town that is besieged.
		for (int32 K = 0; K < SiegeFires.Num(); ++K)
		{
			Plume(SiegeFires[K], K * 11 + 700, 2.3, &Oily);
		}
		// A prize that has been wrecked burns for as long as it is out of use: flame at its foot, black smoke above.
		{
			const FLinearColor Flame(1.0f, 0.5f, 0.12f);
			const double Seen = FMath::Clamp(CameraDistance / (Unit * 480.0), 0.6, 1.5);
			for (int32 K = 0; K < PrizeFires.Num(); ++K)
			{
				if (FVector::DistSquared2D(PrizeFires[K], CameraFocus) > Reach * Reach)
				{
					continue;
				}
				// A thick black column, leaning with the wind: oil and stores burning are seen for miles.
				// (a puff is densest at its very middle and thins to nothing at its rim, so each is laid on three
				// times over: one alone is a wisp)
				for (int32 P = 0; P < 8; ++P)
				{
					const double Age = FMath::Frac(Now / 7.0 + P / 8.0 + K * 0.31);
					const FVector At = PrizeFires[K] + FVector(0.0, 0.0, (1.6 + 11.0 * Age) * Unit) + Wind * (5.0 * Unit * Age * Age)
						+ Right * (FMath::Sin(Now * 0.6 + P * 1.7 + K) * 0.6 * Unit * Age);
					const float Dense = static_cast<float>(FMath::SmoothStep(0.0, 0.08, Age) * FMath::Pow(1.0 - Age, 0.8));
					for (int32 Layer = 0; Layer < 3; ++Layer)
					{
						Draw(At, (1.5 + 3.6 * Age) * Unit * (1.0 - 0.14 * Layer), FLinearColor(0.02f, 0.019f, 0.018f), Dense);
					}
				}
				// (a broad red glow at the foot, and brighter tongues of flame over it)
				Draw(PrizeFires[K] + FVector(0.0, 0.0, 0.7 * Unit * Seen), 1.9 * Unit * Seen, FLinearColor(1.0f, 0.28f, 0.06f), 0.55f + 0.1f * FMath::Sin(static_cast<float>(Now) * 3.1f + K));
				for (int32 Tongue = 0; Tongue < 3; ++Tongue)
				{
					const float Lick = 0.7f + 0.28f * FMath::Sin(static_cast<float>(Now) * (8.0f + Tongue * 3.1f) + K * 2.3f + Tongue);
					const double Big = (1.15 - 0.25 * Tongue) * Unit * Seen;
					Draw(PrizeFires[K] + FVector((Tongue - 1.0) * 0.7 * Unit, 0.0, Big * (0.8 + 0.45 * Tongue)), Big, Flame, Lick);
				}
			}
			// (Release 2, columns.ts) Where a supply column was caught on the road this month, its lorries burn: a low red
			// glow, two tongues of flame and a thin black plume, smaller than a burning depot's and sized to the lorries lost.
			for (int32 K = 0; K < ColumnFires.Num(); ++K)
			{
				const FVector At(ColumnFires[K].X, ColumnFires[K].Y, ColumnFires[K].Z);
				if (FVector::DistSquared2D(At, CameraFocus) > Reach * Reach)
				{
					continue;
				}
				const double Big = static_cast<double>(ColumnFires[K].W);
				const FLinearColor Burning(0.06f, 0.055f, 0.05f);
				Plume(At + FVector(0.0, 0.0, 0.8 * Unit), K * 13 + 1100, 1.1 * Big, &Burning, 1.6f);
				Draw(At + FVector(0.0, 0.0, 0.5 * Unit * Seen), 1.2 * Unit * Seen * Big, FLinearColor(1.0f, 0.28f, 0.06f), 0.5f + 0.1f * FMath::Sin(static_cast<float>(Now) * 3.3f + K));
				for (int32 Tongue = 0; Tongue < 2; ++Tongue)
				{
					const float Lick = 0.7f + 0.28f * FMath::Sin(static_cast<float>(Now) * (8.5f + Tongue * 2.7f) + K * 1.9f + Tongue);
					const double Tall = (0.8 - 0.2 * Tongue) * Unit * Seen * Big;
					Draw(At + FVector((Tongue - 0.5) * 0.6 * Unit, 0.0, Tall * (0.8 + 0.45 * Tongue)), Tall, Flame, Lick);
				}
			}
		}
	}
	// (Release 2, armies with weight.) What a piece set down with weight raises: how much of an army it is, and what it
	// lands on. Snow as the piece itself was told this month; sand where its province is desert.
	const bool bWithWeight = AArmyPiece::CarriedWithWeight();
	auto ThudOf = [this, Now](const AArmyPiece* Landed, double HowMuch)
	{
		FThud Thud;
		Thud.Where = Landed->GetActorLocation();
		Thud.Began = Now;
		Thud.Much = static_cast<float>(TheaterArmyCarry::Heft(Landed->GetCorps().Num()) * HowMuch);
		Thud.Ground = Landed->IsOnSnow() ? 1 : (Data.Regions.IsValidIndex(Landed->GetRegionId()) && Data.Regions[Landed->GetRegionId()].Terrain == TEXT("desert") ? 2 : 0);
		return Thud;
	};
	// A piece sent to join an army already standing is taken off as it lands. It is not among the pieces below and
	// raised no dust before: with weight, the army it joins rocks under it, and the dust is raised there.
	for (int32 JoinIndex = Joining.Num() - 1; JoinIndex >= 0; --JoinIndex)
	{
		if (Joining[JoinIndex].Sent.IsValid())
		{
			continue;
		}
		if (AArmyPiece* Joined = Joining[JoinIndex].Joined.Get())
		{
			// (Only a landing: the piece came down in its time, and the army still stands where it was joined.)
			const bool bLanded = Now >= Joining[JoinIndex].Due - 0.15 && FVector::DistSquared(Joined->GetRestLocation(), Joining[JoinIndex].Stood) < 1.0;
			if (bWithWeight && bLanded && !Joined->IsMarching())
			{
				Thuds.Add(ThudOf(Joined, 0.85));
				Joined->TakeWeight(Joined->GetActorLocation() - Joining[JoinIndex].From);
			}
		}
		Joining.RemoveAtSwap(JoinIndex);
	}
	// The fires of armies in camp: a thin thread of smoke from each.
	// And a puff of dust as a piece that has been moved is set down on the table again.
	for (AArmyPiece* Piece : Pieces)
	{
		if (!Piece)
		{
			continue;
		}
		if (Piece->IsInCamp() && !Piece->IsMarching())
		{
			Plume(Piece->GetCampFire(), Piece->GetRegionId() + 900, 0.62);
		}
		const TWeakObjectPtr<AArmyPiece> Key(Piece);
		if (Piece->IsMarching())
		{
			OnTheMarch.Add(Key);
		}
		else if (OnTheMarch.Contains(Key))
		{
			OnTheMarch.Remove(Key);
			if (bWithWeight)
			{
				Thuds.Add(ThudOf(Piece, 1.0));
			}
			else
			{
				SetDowns.Emplace(Piece->GetActorLocation(), Now);
			}
		}
	}
	SetDowns.RemoveAll([Now](const TPair<FVector, double>& Down) { return Now - Down.Value > 1.6; });
	for (const TPair<FVector, double>& Down : SetDowns)
	{
		const double Age = (Now - Down.Value) / 1.6;
		const FLinearColor Dusty(0.62f, 0.56f, 0.45f);
		for (int32 K = 0; K < 9; ++K)
		{
			const double Angle = K * 0.698 + 0.3;
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
			Draw(Down.Key + Out * (2.6 + 3.4 * Age) * Unit + FVector(0.0, 0.0, (0.3 + 1.1 * Age) * Unit), (0.7 + 1.5 * Age) * Unit, Dusty, 0.5f * static_cast<float>(FMath::Pow(1.0 - Age, 1.4)));
		}
	}
	// (Release 2, armies with weight.) And with weight: a low ring that runs out along the table from under the base as
	// it lands, quick and soon gone, and a cloud that rises behind it and drifts off down the wind. More of both for a
	// bigger army; white and soon fallen on snow, pale and long in the air on sand (Campaign/ArmyCarry.h).
	Thuds.RemoveAll([Now](const FThud& Thud) { return Now - Thud.Began > 2.3; });
	for (const FThud& Thud : Thuds)
	{
		if (FVector::DistSquared2D(Thud.Where, CameraFocus) > Reach * Reach)
		{
			continue;
		}
		const double Since = Now - Thud.Began;
		// (the snow's own white is kept under the brightness at which a puff counts as a light by night: see DrawPuff)
		const FLinearColor Raised = Thud.Ground == 1 ? FLinearColor(0.72f, 0.74f, 0.79f) : Thud.Ground == 2 ? FLinearColor(0.80f, 0.71f, 0.54f) : FLinearColor(0.62f, 0.56f, 0.45f);
		const double Turned = FMath::Frac(Thud.Where.X * 0.0137 + Thud.Where.Y * 0.0071) * 0.7; // (no two rings set alike)
		const TheaterArmyCarry::FDust Low = TheaterArmyCarry::Skirt(Since, Thud.Much);
		if (Low.Thick > 0.0)
		{
			for (int32 Puffed = 0; Puffed < 8; ++Puffed)
			{
				const double Angle = Puffed * 0.7854 + 0.2 + Turned;
				const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
				Draw(Thud.Where + Out * (Low.Out * Unit) + FVector(0.0, 0.0, Low.High * Unit), Low.Size * Unit, Raised, static_cast<float>(Low.Thick));
			}
		}
		const TheaterArmyCarry::FDust Cloud = TheaterArmyCarry::Billow(Since, Thud.Much, Thud.Ground);
		if (Cloud.Thick > 0.0)
		{
			const double Aged = Since / TheaterArmyCarry::BillowLasts(Thud.Ground);
			const FVector Drift = Wind * ((Thud.Ground == 2 ? 1.8 : 1.2) * Unit * Aged * Aged);
			for (int32 Puffed = 0; Puffed < 9; ++Puffed)
			{
				const double Angle = Puffed * 0.698 + 0.3 + Turned;
				const double Uneven = 0.85 + 0.3 * FMath::Frac(Puffed * 0.618 + Turned * 3.0);
				const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
				Draw(Thud.Where + Out * (Cloud.Out * Uneven * Unit) + FVector(0.0, 0.0, Cloud.High * Unit) + Drift, Cloud.Size * Unit, Raised, static_cast<float>(Cloud.Thick));
			}
		}
	}
	for (int32 K = 0; K < TownChimneys.Num(); ++K)
	{
		// (a city with nothing on order is idle, and its stacks stand cold)
		if (TownChimneyRegion.IsValidIndex(K) && IdleIn.Contains(TownChimneyRegion[K] - 1))
		{
			continue;
		}
		Plume(TownChimneys[K], K);
	}
	for (int32 K = 0; K < WorksChimneys.Num(); ++K)
	{
		// (Where more factories are wrecked than the town had before the war, the stacks of the works built since
		// stand cold too, as many as are wrecked.)
		if (WorksChimneyRegion.IsValidIndex(K) && Data.Regions.IsValidIndex(WorksChimneyRegion[K].X)
			&& WorksChimneyRegion[K].Y < WreckedIn.FindRef(WorksChimneyRegion[K].X) - Data.Regions[WorksChimneyRegion[K].X].Factories)
		{
			continue;
		}
		if (WorksChimneyRegion.IsValidIndex(K) && IdleIn.Contains(WorksChimneyRegion[K].X))
		{
			continue;
		}
		Plume(WorksChimneys[K], K + 500);
	}

	// (Release 2, funnel smoke: the campaign map's upgrade 3, Campaign/ShipSmoke.h; tc.Map.ShipSmoke.) Every ship of a
	// fleet at sea has steam up: a pale wisp over her lying still, and under way a plume that streams astern along the
	// water she has crossed, black while she works up to speed. Near what the eye looks at only. The fleets under way
	// are drawn first and those lying still after, while there is room in what the fleets may take of the table's smoke.
	// Nothing is kept from frame to frame: each puff is where the clock says it is.
	if (ShipSmokeWanted())
	{
		const double FunnelMuch = FMath::Clamp(static_cast<double>(CVarShipSmokeMuch.GetValueOnGameThread()), 0.0, 2.0);
		const double FunnelLong = FMath::Clamp(static_cast<double>(CVarShipSmokeLong.GetValueOnGameThread()), 0.0, 2.0);
		const int32 SmokeLevel = TheaterShipSmoke::Level(CameraDistance);
		const bool bEachFunnel = TheaterShipSmoke::EachFunnel(SmokeLevel);
		const FVector LeeWay = Wind.GetSafeNormal();
		int32 FleetPuffs = 0;
		TArray<AArmyPiece::FFunnelNow> FunnelsSeen;
		for (int32 SmokePass = 0; SmokePass < 2 && FunnelMuch > 0.0; ++SmokePass)
		{
			for (const TPair<int32, TObjectPtr<AArmyPiece>>& FleetEntry : FleetPieces)
			{
				const AArmyPiece* Steamer = FleetEntry.Value.Get();
				if (!Steamer || FVector::DistSquared2D(Steamer->GetActorLocation(), CameraFocus) > Reach * Reach)
				{
					continue;
				}
				const bool bSteamsOn = Steamer->IsSailing();
				const int32 PlumePuffs = TheaterShipSmoke::Puffs(SmokeLevel, bSteamsOn);
				double SteamPace = 0.0, SteamGather = 0.0;
				if (bSteamsOn != (SmokePass == 0) || PlumePuffs <= 0 || !Steamer->FunnelsNow(FunnelsSeen, SteamPace, SteamGather))
				{
					continue;
				}
				const TheaterShipSmoke::FFire Steam = TheaterShipSmoke::Fire(SteamPace, SteamGather, FunnelMuch, FunnelLong);
				// One plume for each funnel close to; from further off one for each ship, from the middle of her funnels.
				// (Her funnels come ship by ship.)
				for (int32 FunnelFirst = 0; FunnelFirst < FunnelsSeen.Num();)
				{
					int32 FunnelsOn = 1;
					while (FunnelFirst + FunnelsOn < FunnelsSeen.Num() && FunnelsSeen[FunnelFirst + FunnelsOn].Ship == FunnelsSeen[FunnelFirst].Ship)
					{
						++FunnelsOn;
					}
					const int32 PlumesOn = bEachFunnel ? FunnelsOn : 1;
					for (int32 PlumeIndex = 0; PlumeIndex < PlumesOn; ++PlumeIndex)
					{
						const AArmyPiece::FFunnelNow& PlumeOf = FunnelsSeen[FunnelFirst + PlumeIndex];
						FVector PlumeTop = PlumeOf.Top;
						if (!bEachFunnel)
						{
							for (int32 FunnelNext = 1; FunnelNext < FunnelsOn; ++FunnelNext)
							{
								PlumeTop += FunnelsSeen[FunnelFirst + FunnelNext].Top;
							}
							PlumeTop /= static_cast<double>(FunnelsOn);
						}
						// (two funnels' smoke is each a little less than one funnel's alone; run together it is a little more)
						const double PlumeGirth = TheaterShipSmoke::Girth(PlumeOf.Class) * (FunnelsOn < 2 ? 1.0 : bEachFunnel ? 0.8 : 1.15);
						const double PlumeLong = PlumeOf.Long;
						if (PlumeGirth <= 0.0 || PlumeLong <= 0.0 || FleetPuffs + PlumePuffs > TheaterShipSmoke::MostPuffs)
						{
							continue;
						}
						const double PlumeSeed = FMath::Frac(FleetEntry.Key * 0.137 + (FunnelFirst + PlumeIndex) * 0.29);
						for (int32 PuffIndex = 0; PuffIndex < PlumePuffs; ++PuffIndex)
						{
							const double PuffAge = TheaterShipSmoke::AgeOf(Now, PuffIndex, PlumePuffs, PlumeSeed);
							// (Under way each puff is as thick and as dark as the fire that made it, its age ago, and
							// not as the fire is now: the plume never changes colour all at once. ShipSmoke.h, Carries.)
							TheaterShipSmoke::FFire PuffFire = Steam;
							if (bSteamsOn)
							{
								double MadePace = 0.0, MadeGather = 0.0;
								const double MadeKeeps = Steamer->SteamedAt(PuffAge * TheaterShipSmoke::Life, MadePace, MadeGather);
								PuffFire = TheaterShipSmoke::Carries(Steam, TheaterShipSmoke::Fire(MadePace, MadeGather, FunnelMuch, FunnelLong), MadeKeeps);
							}
							const TheaterShipSmoke::FPuff OnePuff = TheaterShipSmoke::Puff(PuffFire, PuffAge, PlumePuffs, PlumeGirth);
							++FleetPuffs;
							if (OnePuff.Thick <= 0.004)
							{
								continue;
							}
							const double PuffBack = OnePuff.Astern * PlumeLong;
							const FVector PuffAstern = Steamer->AsternBy(PlumeTop, PuffBack);
							const double PuffSway = FMath::Sin(Now * 0.7 + PlumeSeed * 40.0 + PuffIndex * 1.7) * 0.25 * OnePuff.Size * PlumeLong * PuffAge;
							const FVector PuffAt = PlumeTop + PuffAstern + FVector(0.0, 0.0, OnePuff.Rise * PlumeLong) + LeeWay * (OnePuff.Lee * PlumeLong) + Right * PuffSway;
							const TheaterShipSmoke::FInk PuffInk = TheaterShipSmoke::Ink(PuffFire.Dark, PuffAge);
							const FLinearColor PuffColour(static_cast<float>(PuffInk.R), static_cast<float>(PuffInk.G), static_cast<float>(PuffInk.B));
							const double PuffSize = OnePuff.Size * PlumeLong;
							if (OnePuff.Stretch > 1.0)
							{
								// The way the plume runs here: astern along the water she crossed, up, and down the wind. A puff
								// is drawn out along it only by as much of that as lies across the eye's view: one that runs
								// straight at the eye is round.
								const FVector PuffFurther = Steamer->AsternBy(PlumeTop, PuffBack + 0.02 * PlumeLong) - PuffAstern;
								const FVector PuffRuns = PuffFurther.GetSafeNormal() * OnePuff.RunAstern + FVector::UpVector * OnePuff.RunRise + LeeWay * OnePuff.RunLee;
								const FVector PuffSeen = Right * FVector::DotProduct(PuffRuns, Right) + Up * FVector::DotProduct(PuffRuns, Up);
								const double PuffShown = FMath::Min(PuffSeen.Size(), 1.0);
								DrawSmear(PuffAt, PuffRuns, PuffSize * (1.0 + (OnePuff.Stretch - 1.0) * PuffShown), PuffSize, PuffColour, static_cast<float>(OnePuff.Thick));
							}
							else
							{
								Draw(PuffAt, PuffSize, PuffColour, static_cast<float>(OnePuff.Thick));
							}
						}
					}
					FunnelFirst += FunnelsOn;
				}
			}
		}
	}

	// Battles: for a few seconds the guns flash, and dark smoke rolls up and drifts away.
	const double Lasts = 11.0;
	const double Big = Unit * 2.3; // a battle is a bigger thing than a chimney
	Battles.RemoveAll([Now, Lasts](const FBattleSmoke& Battle) { return Now - Battle.Began > Lasts + 4.0; });
	const FLinearColor Dark(0.16f, 0.15f, 0.14f), Dust(0.55f, 0.5f, 0.42f), Fire(1.0f, 0.62f, 0.22f);
	for (int32 B = 0; B < Battles.Num(); ++B)
	{
		const FBattleSmoke& Battle = Battles[B];
		const double Since = Now - Battle.Began;
		FRandomStream Random(B * 977 + static_cast<int32>(Battle.Where.X));
		// Shell bursts round the field while the fight lasts: each a flash, then a puff of dust.
		for (int32 K = 0; K < 14; ++K)
		{
			const double Starts = Random.FRandRange(0.0f, static_cast<float>(Lasts * 0.75));
			const FVector Offset(Random.FRandRange(-2.6f, 2.6f) * Big, Random.FRandRange(-2.6f, 2.6f) * Big, 0.0);
			const double Age = (Since - Starts) / 2.4;
			if (Age < 0.0 || Age > 1.0)
			{
				continue;
			}
			if (Age < 0.09)
			{
				Draw(Battle.Where + Offset + FVector(0.0, 0.0, 0.2 * Big), (0.25 + 4.0 * Age) * Big, Fire, 0.95f * (1.0f - static_cast<float>(Age / 0.09)));
			}
			Draw(Battle.Where + Offset + FVector(0.0, 0.0, (0.3 + 1.6 * Age) * Big) + Wind * (0.9 * Big * Age), (0.45 + 1.3 * Age) * Big, Dust, 0.65f * FMath::Pow(1.0 - Age, 1.2));
		}
		// The pall over it all.
		for (int32 K = 0; K < 9; ++K)
		{
			const double Starts = K * 0.9;
			const double Age = (Since - Starts) / 6.5;
			if (Age < 0.0 || Age > 1.0)
			{
				continue;
			}
			const FVector Offset(Random.FRandRange(-1.2f, 1.2f) * Big, Random.FRandRange(-1.2f, 1.2f) * Big, 0.0);
			const FVector At = Battle.Where + Offset + FVector(0.0, 0.0, (0.6 + 5.0 * Age) * Big) + Wind * (3.2 * Big * Age * Age);
			Draw(At, (0.9 + 2.6 * Age) * Big, Dark, 0.72f * FMath::SmoothStep(0.0, 0.1, Age) * FMath::Pow(1.0 - Age, 1.2));
		}
	}

	// (Brian's ask of 16 November 2026; the art director's brief.) Black smoke over every place a raid has damaged, from
	// the bombs falling through the whole of the next month. A leaning column, never a ball: narrow and dense at its
	// root, drifting downwind as the flags fly and flattening into a pall; black at the root, thinning to a grey
	// haze at the top; a small fire at its foot, seen from close by. From the top it is never smaller than a town's
	// plate and there is one column a place. Nothing like a chimney's thin grey plume.
	{
		const FLinearColor RootInk(FColor(24, 20, 18)), MidInk(FColor(52, 46, 42)), TopInk(FColor(96, 88, 80)), FireInk(FColor(204, 88, 36));
		const float Moon = GetMoonlight();
		const FVector Skyward(0.0, 0.0, 1.0);
		// (the towns that burn, for the glow of their fires: one a place, however many columns stand over it)
		struct FBurningTown
		{
			FVector FireAt = FVector::ZeroVector; // the root of its first column
			float Risen = 0.0f; // how far its light has come up, or died away
			int32 Columns = 0;
			int32 Seed = 0;
		};
		TMap<int32, FBurningTown> BurningTowns;
		// (The art director's round 34: the head of a column hid the army standing behind it. Flags are things of the
		// table and smoke is drawn over whatever is behind it, so a flag cannot be laid over the smoke; the smoke is
		// thinned instead wherever it comes between the eye and a standard.)
		FVector SmokeEye = CameraFocus + FVector(0.0, 0.0, CameraDistance);
		if (const APlayerController* Watcher = World->GetFirstPlayerController())
		{
			if (Watcher->PlayerCameraManager)
			{
				SmokeEye = Watcher->PlayerCameraManager->GetCameraLocation();
			}
		}
		for (FBombSmoke& Smoke : BombSmokes)
		{
			// (it rises when the bombs are seen to fall, not when the order is given)
			bool bWaiting = false;
			for (const FFlight& Flight : Flights)
			{
				bWaiting |= Flight.Sortie.Region == Smoke.Region && Flight.Sortie.bBombers && !Flight.bStruck;
			}
			if (bWaiting && Smoke.Ends < 0.0)
			{
				Smoke.Began = Now;
				continue;
			}
			float Strength = static_cast<float>(FMath::SmoothStep(0.0, 1.5, Now - Smoke.Began));
			if (Smoke.Ends >= 0.0)
			{
				Strength *= 1.0f - static_cast<float>(FMath::SmoothStep(0.0, 2.0, Now - Smoke.Ends));
			}
			{
				// (its fire's light comes up over about two seconds as the bombs land, and dies with the column)
				FBurningTown& Burning = BurningTowns.FindOrAdd(Smoke.Region);
				const float Up2 = static_cast<float>(FMath::SmoothStep(0.0, 2.0, Now - Smoke.Began));
				Burning.Risen = FMath::Max(Burning.Risen, Smoke.Ends >= 0.0 ? Up2 * (1.0f - static_cast<float>(FMath::SmoothStep(0.0, 2.0, Now - Smoke.Ends))) : Up2);
				Burning.Columns += Smoke.Ends < 0.0 ? 1 : 0;
				if (Smoke.bFirst || Burning.FireAt.IsZero())
				{
					Burning.FireAt = Smoke.Root;
					Burning.Seed = Smoke.Seed;
				}
			}
			const bool bFar = CameraDistance > 9000.0f;
			if (Strength <= 0.01f || (bFar && !Smoke.bFirst) || FVector::DistSquared2D(Smoke.Root, CameraFocus) > FMath::Square(CameraDistance * 1.6 + 2500.0))
			{
				continue;
			}
			// (from the top it is a mark, never smaller than a town's plate: about forty points of the screen)
			const double Tall = FMath::Max(Smoke.Tall, (bFar ? 0.06 : 0.032) * CameraDistance);
			// (Many small puffs that overlap: a puff is thick at its middle and nothing at its rim, and sixteen of them
			// up a column were a row of soft shadows, not smoke. Fewer from the top, where it is a mark.)
			const int32 Count = bFar ? 22 : 46;
			const double Life = 11.0;
			// (the standards that the column might stand before: each army's flag)
			TArray<FVector, TInlineAllocator<16>> Standards;
			if (!bFar)
			{
				for (const TObjectPtr<AArmyPiece>& Piece : Pieces)
				{
					if (Piece && !Piece->IsHidden() && FVector::DistSquared2D(Piece->GetActorLocation(), Smoke.Root) < FMath::Square(3.0 * Tall)
						&& FVector::DistSquared(Piece->GetActorLocation(), SmokeEye) > FVector::DistSquared(Smoke.Root, SmokeEye))
					{
						Standards.Add(Piece->GetActorLocation() + Skyward * (13.0 * UnitScale));
					}
				}
			}
			for (int32 K = 0; K < Count; ++K)
			{
				const double Age = FMath::Frac(Now / Life + static_cast<double>(K) / Count + Smoke.Seed * 0.137);
				// (it climbs fast and then flattens, and the wind takes it further the higher it is: a pall, not a pillar)
				const double High = Tall * (1.0 - FMath::Pow(1.0 - Age, 1.7));
				// (each puff keeps a place of its own beside the column's line, and billows a little as it goes up)
				const double Aside = FMath::Sin(K * 12.9898 + Smoke.Seed) * 0.5 + FMath::Sin(Now * 0.45 + K * 1.7) * 0.2;
				const double Radius = Tall * FMath::Lerp(1.0 / 26.0, 1.0 / 7.0, FMath::Pow(Age, 1.2)) * (0.85 + 0.3 * FMath::Abs(FMath::Sin(K * 4.1 + Smoke.Seed)));
				const FVector At = Smoke.Root + Skyward * (High + 0.5 * Radius * FMath::Cos(K * 7.3)) + Wind * (0.8 * Tall * Age * Age) + Right * (Aside * Radius * 0.9);
				FLinearColor Ink = Age < 0.5 ? FMath::Lerp(RootInk, MidInk, static_cast<float>(Age * 2.0)) : FMath::Lerp(MidInk, TopInk, static_cast<float>(Age * 2.0 - 1.0));
				// (by night the fire lights its lowest third)
				// (the puff's own colour takes the hour in DrawPuff; the fire's share is given so that it comes through whole)
				{
					// (The burning town brief: the root goes to about (178,63,33), halfway up it is a dull red-brown, and
					// the head stays as it was: the light does not reach it.)
					// (It is a light, and DrawPuff leaves a light its own colour: it is not brightened again for the hour.
					// Brightened twice, the lit root read as one great flame.)
					const float Fired = Moon * (1.0f - static_cast<float>(FMath::SmoothStep(0.0, 0.42, Age)));
					Ink = FMath::Lerp(Ink, FLinearColor(FColor(178, 63, 33)), Fired);
				}
				// (round 34: the head about two fifths thick where it was three quarters; from the top it stays a mark)
				const float Thick = Age < 0.35 ? FMath::Lerp(1.0f, 0.7f, static_cast<float>(Age / 0.35))
					: FMath::Lerp(0.7f, bFar ? 0.4f : 0.05f, FMath::Pow(static_cast<float>((Age - 0.35) / 0.65), 0.7f));
				float Shown = Thick * Strength * static_cast<float>(FMath::SmoothStep(0.0, 0.02, Age)) * (1.0f - static_cast<float>(FMath::SmoothStep(0.88, 1.0, Age)));
				for (const FVector& Standard : Standards)
				{
					// (how near the puff's middle comes to the line from the eye to the standard, in its own radii)
					const FVector Sight = (Standard - SmokeEye).GetSafeNormal();
					const double SightAlong = FVector::DotProduct(At - SmokeEye, Sight);
					const double SightOff = FVector::Dist(SmokeEye + Sight * SightAlong, At) / FMath::Max(Radius + 2.5 * UnitScale, 1.0);
					// (the column's black root stays whole: it is its head and its pall that part before a flag)
					Shown *= FMath::Lerp(1.0f, FMath::Lerp(0.3f, 1.0f, static_cast<float>(FMath::SmoothStep(0.7, 1.4, SightOff))), static_cast<float>(FMath::SmoothStep(0.22, 0.42, Age)));
				}
				// (a body of smoke in its lower two thirds, a haze where it flattens out)
				if (Age < 0.62)
				{
					DrawFirm(At, Radius, Ink, Shown);
				}
				else
				{
					Draw(At, Radius, Ink, Shown);
				}
			}
			if (!bFar && CameraDistance < 6500.0f)
			{
				Draw(Smoke.Root + Skyward * (0.015 * Tall), Tall / 24.0 * (1.0 + 0.2 * FMath::Sin(Now * 9.0 + Smoke.Seed)), FireInk, 0.85f * Strength);
			}
		}

		// THE RED GLOW OF A BURNING TOWN BY NIGHT (ruling 107(f); the art director's brief of 16 November 2026).
		// By night a bombed town had its flames and its black column, and nothing was lit by them. The glow is the
		// fire's own light on three things: the ground round the town, an uneven pool of three lobes that fades to
		// nothing with no edge; the town itself, its roofs and walls, strongest beside the fire; and the air on the
		// downwind side, a low red haze. (The smoke's root is lit from below where the column is drawn.) It is a
		// soft light laid over what is there, orange at the heart and red at the rim, and nothing under it goes
		// white. It shows by night only, and at half strength at dusk; wherever a raid's smoke stands; full over a
		// city hit hard, two thirds as wide and six tenths as strong over a plain hit. It comes up over two seconds
		// as the bombs land and it breathes slowly, never faster than once a second. It lights no flag and no
		// army's piece: the town's light and the haze are thinned to nothing where a piece stands before them. The
		// smoke, the lit railway and a cut's lamp are drawn over it. It never covers a province, and it is never a
		// disc: if the pool's edge can be traced, it is wrong.
		if (FireGlowMesh && FireGlowInk)
		{
			FCampaignMeshBuffers GlowFire;
			const float GlowNight = FMath::Clamp(Moon, 0.0f, 1.0f) * (1.0f - GetPaper());
			const APlayerController* GlowWatcher = World->GetFirstPlayerController();
			const TheaterSpeed::FEye OneView(GlowWatcher); // (the view worked out once for all these points: see TheaterSpeed)
			const FLinearColor GlowHeart(FColor(255, 108, 38)), GlowRed(FColor(170, 34, 14));
			auto GlowInked = [](const FLinearColor& Colour, float Alpha)
			{
				FLinearColor Out = Colour;
				Out.A = FMath::Clamp(Alpha, 0.0f, 1.0f);
				return Out.ToFColor(false);
			};
			const FVector WindWay = Wind.GetSafeNormal(), WindAcross(-WindWay.Y, WindWay.X, 0.0);
			// (from where he plays the pool and the haze are drawn about half as large again, and a little weaker)
			const double GlowHigh = FMath::SmoothStep(2600.0, 4600.0, static_cast<double>(CameraDistance));
			const int32 Spokes = 16;
			for (const TPair<int32, FBurningTown>& Lit : BurningTowns)
			{
				const FBurningTown& Burning = Lit.Value;
				if (GlowNight <= 0.02f || Burning.Risen <= 0.01f || !Data.Regions.IsValidIndex(Lit.Key)
					|| FVector::DistSquared2D(Burning.FireAt, CameraFocus) > FMath::Square(CameraDistance * 1.6 + 2500.0))
				{
					continue;
				}
				const FCampaignRegion& Province = Data.Regions[Lit.Key];
				const bool bHard = Burning.Columns >= 3;
				// (it breathes: about an eighth up and down, over three to five seconds, unevenly)
				const double Breath = 1.0 + 0.08 * FMath::Sin(Now * 1.53 + Burning.Seed) + 0.045 * FMath::Sin(Now * 2.03 + Burning.Seed * 1.7);
				const float Glowing = GlowNight * Burning.Risen * static_cast<float>(Breath) * (bHard ? 1.0f : 0.6f);
				const double Houses = TownRadius(Lit.Key) * UnitScale;
				// (seam-4: the town in the turn of the world its fire is drawn in, which is the eye's side of the seam)
				const FVector TownLaid = SurfacePoint(Province.Center.X, Province.Center.Y, 0.0);
				const FVector TownAt(TownLaid.X, TownLaid.Y + (Data.bFar && Data.WorldWidth > 0.0 ? FMath::RoundToDouble((Burning.FireAt.Y - TownLaid.Y) / (Data.WorldWidth * UnitScale)) * (Data.WorldWidth * UnitScale) : 0.0), TownLaid.Z);
				// The pool reaches half a town's width beyond the houses; it stays under about a third of its
				// province's width; and from the top it is never under about forty pixels across.
				double PoolOut = 2.0 * Houses * (bHard ? 1.0 : 0.67) * FMath::Lerp(1.0, 1.45, GlowHigh);
				if (Province.Area > 1.0)
				{
					PoolOut = FMath::Min(PoolOut, FMath::Max(0.15 * FMath::Sqrt(Province.Area) * UnitScale, 1.1 * Houses));
				}
				PoolOut = FMath::Max(PoolOut, 20.0 * 0.00037 * CameraDistance);
				const float PoolThick = Glowing * static_cast<float>(FMath::Lerp(1.0, 0.72, GlowHigh));
				// (the armies' pieces that stand near, as boxes on the screen: the town's light and the haze are kept off them)
				TArray<FBox2D, TInlineAllocator<8>> Spared;
				if (GlowWatcher)
				{
					for (const TObjectPtr<AArmyPiece>& Piece : Pieces)
					{
						if (!Piece || Piece->IsHidden() || FVector::DistSquared2D(Piece->GetActorLocation(), TownAt) > FMath::Square(PoolOut + 2.0 * Houses + 30.0 * UnitScale))
						{
							continue;
						}
						FVector BoundsAt, BoundsOut;
						Piece->GetActorBounds(false, BoundsAt, BoundsOut);
						FBox2D Shape(ForceInit);
						bool bAll = true;
						for (int32 Corner = 0; Corner < 8 && bAll; ++Corner)
						{
							FVector2D OnScreen;
							bAll = OneView.Project(BoundsAt + BoundsOut * FVector(Corner & 1 ? 1.0 : -1.0, Corner & 2 ? 1.0 : -1.0, Corner & 4 ? 1.0 : -1.0), OnScreen);
							Shape += OnScreen;
						}
						if (bAll)
						{
							Spared.Add(Shape);
						}
					}
				}
				auto KeptOff = [&](const FVector& Point) -> float
				{
					FVector2D OnScreen;
					if (Spared.Num() == 0 || !OneView.Project(Point, OnScreen))
					{
						return 1.0f;
					}
					float Whole = 1.0f;
					for (const FBox2D& Shape : Spared)
					{
						const double Outside = FMath::Max(FMath::Max(Shape.Min.X - OnScreen.X, OnScreen.X - Shape.Max.X), FMath::Max(Shape.Min.Y - OnScreen.Y, OnScreen.Y - Shape.Max.Y));
						Whole = FMath::Min(Whole, static_cast<float>(FMath::SmoothStep(2.0, 28.0, Outside)));
					}
					return Whole;
				};
				// A soft patch of light: thick at its hub, thinning across two rings to nothing at an uneven edge;
				// orange at the hub and red at the edge. On the ground it follows the ground.
				auto GlowFan = [&](const FVector& Hub, const TArray<FVector>& Brim, float Thick, bool bOnGround, bool bSpare)
				{
					static const double RingOut[3] = { 0.34, 0.68, 1.0 };
					static const float RingThick[3] = { 0.82f, 0.36f, 0.0f }, RingRed[3] = { 0.3f, 0.7f, 1.0f };
					auto Laid = [&](const FVector& Point)
					{
						if (!bOnGround)
						{
							return Point;
						}
						const FVector2D OnMap = WorldToMap(Point);
						return SurfacePoint(OnMap.X, OnMap.Y, 0.3);
					};
					const int32 Around = Brim.Num();
					const FVector HubAt = Laid(Hub);
					const int32 Start = GlowFire.AddVertex(HubAt, FVector::UpVector, GlowInked(GlowHeart, Thick * (bSpare ? KeptOff(HubAt) : 1.0f)));
					for (int32 Band = 0; Band < 3; ++Band)
					{
						for (int32 Spoke = 0; Spoke < Around; ++Spoke)
						{
							const FVector Point = Laid(FMath::Lerp(Hub, Brim[Spoke], RingOut[Band]));
							GlowFire.AddVertex(Point, FVector::UpVector, GlowInked(FMath::Lerp(GlowHeart, GlowRed, RingRed[Band]), Thick * RingThick[Band] * (bSpare ? KeptOff(Point) : 1.0f)));
						}
					}
					for (int32 Spoke = 0; Spoke < Around; ++Spoke)
					{
						const int32 Near0 = Start + 1 + Spoke, Next0 = Start + 1 + (Spoke + 1) % Around;
						GlowFire.Triangles.Append({ Start, Near0, Next0, Start, Next0, Near0 });
						for (int32 Band = 0; Band + 1 < 3; ++Band)
						{
							const int32 A0 = Near0 + Band * Around, B0 = Next0 + Band * Around, A1 = A0 + Around, B1 = B0 + Around;
							GlowFire.Triangles.Append({ A0, A1, B1, A0, B1, A1, A0, B1, B0, A0, B0, B1 });
						}
					}
				};
				auto Uneven = [&Burning](double Angle, double Turn, double By)
				{
					return 1.0 + By * (0.17 * FMath::Sin(2.0 * Angle + Turn + Burning.Seed) + 0.1 * FMath::Sin(3.0 * Angle + 1.7 * Turn + Burning.Seed * 0.37) + 0.06 * FMath::Sin(5.0 * Angle + Burning.Seed * 0.11));
				};
				const FVector FireFoot(Burning.FireAt.X, Burning.FireAt.Y, TownAt.Z);
				// 1. The ground: three lobes of different sizes, the largest under the fire, one leaning downwind the
				// way the smoke leans, one smaller to the other side.
				{
					struct FLobe
					{
						FVector Centre;
						double Size, Long, Turn;
						float Share;
					};
					const FLobe Lobes[3] = {
						{ FireFoot, 1.0, 1.0, 0.0, 1.0f },
						{ FireFoot + WindWay * (0.5 * PoolOut), 0.8, 1.4, 1.3, 0.7f },
						{ FireFoot - WindWay * (0.22 * PoolOut) + WindAcross * ((Burning.Seed % 2 == 0 ? 0.48 : -0.48) * PoolOut), 0.55, 1.0, 2.9, 0.6f } };
					for (const FLobe& Lobe : Lobes)
					{
						TArray<FVector> Brim;
						for (int32 Spoke = 0; Spoke < Spokes; ++Spoke)
						{
							const double Angle = Spoke * 2.0 * PI / Spokes;
							Brim.Add(Lobe.Centre + (WindWay * (FMath::Cos(Angle) * Lobe.Long) + WindAcross * FMath::Sin(Angle)) * (PoolOut * Lobe.Size * Uneven(Angle, Lobe.Turn, 1.0)));
						}
						// (As added light the ground sixty pixels beyond the houses goes from night's (46,64,77) to about
						// (77,72,80), a faint rose; laid over as a wash it needs more to show at all.)
						GlowFan(Lobe.Centre, Brim, (bFireGlowAdds ? 0.24f : 0.36f) * PoolThick * Lobe.Share, true, false);
					}
				}
				// 2. The town: its roofs and walls take the light, strongest beside the fire and a dark red at the far
				// edge. A patch that faces the eye, standing just before the nearest houses on the line from the eye
				// to the town, as wide and as deep as the town looks from there. (Over a plain hit it is two thirds
				// the size and drawn toward the fire: it warms the nearest roofs and no more.)
				{
					const FVector ToEye = (SmokeEye - TownAt).GetSafeNormal();
					const double Before = 1.2 * Houses + 2.0 * UnitScale;
					const double Tilted = FMath::Sqrt(FMath::Max(1.0 - Up.Z * Up.Z, 0.0)); // (how much of the ground's depth the eye sees)
					const double Part = bHard ? 1.0 : 0.67;
					const double Wide = 1.3 * 1.05 * Houses * Part, Deep = 1.3 * (1.05 * Houses * FMath::Max(Tilted, 0.3) + 0.25 * Houses) * Part;
					const FVector Lift(0.0, 0.0, 0.25 * Houses);
					const FVector Over = FMath::Lerp(TownAt, FireFoot, bHard ? 0.0 : 0.6) + Lift + ToEye * Before;
					TArray<FVector> Brim;
					for (int32 Spoke = 0; Spoke < Spokes; ++Spoke)
					{
						const double Angle = Spoke * 2.0 * PI / Spokes;
						Brim.Add(Over + (Right * (FMath::Cos(Angle) * Wide) + Up * (FMath::Sin(Angle) * Deep)) * Uneven(Angle, 0.7, 0.5));
					}
					// (a roof beside the fire goes from night's (29,32,39) to about (221,104,63); one at the far edge to a dark red)
					GlowFan(FireFoot + Lift + ToEye * Before, Brim, (bFireGlowAdds ? 0.52f : 0.6f) * Glowing, false, true);
				}
				// 3. The haze: low in the air on the downwind side, no taller than the town is wide. (It stands just
				// clear of the ground, so that the ground does not cut it.)
				{
					const double HazeWide = 0.85 * PoolOut, HazeHigh = 0.8 * Houses;
					const FVector Hub = TownAt + WindWay * (0.9 * Houses + 0.25 * PoolOut) + FVector(0.0, 0.0, HazeHigh * FMath::Abs(Up.Z) + 0.08 * Houses);
					TArray<FVector> Brim;
					for (int32 Spoke = 0; Spoke < Spokes; ++Spoke)
					{
						const double Angle = Spoke * 2.0 * PI / Spokes;
						Brim.Add(Hub + (Right * (FMath::Cos(Angle) * HazeWide) + Up * (FMath::Sin(Angle) * HazeHigh)) * Uneven(Angle, 2.1, 1.0));
					}
					GlowFan(Hub, Brim, (bFireGlowAdds ? 0.13f : 0.2f) * PoolThick, false, true);
				}
			}
			if (!GlowFire.Commit(FireGlowMesh, 0, FireGlowInk))
			{
				if (FireGlowMesh->GetNumSections() > 0)
				{
					FireGlowMesh->ClearAllMeshSections();
				}
			}
			else
			{
				FireGlowMesh->SetCastShadow(false);
				FireGlowMesh->SetTranslucentSortPriority(-1); // (under the smoke, the lit railway and a cut's lamp)
			}
		}
	}

	// Raids in flight: flak round the bombers over a defended target, tracer between them and the fighters that
	// rise to meet them, and a damaged bomber trailing smoke on the way home.
	const FLinearColor FlakBlack(0.03f, 0.03f, 0.035f);
	const double TracerBurns = 0.6; // (seconds a tracer round is alight)
	const double Table = UnitScale;
	// (release 2, the art director's flak brief of 24 November 2026; tc.Look.FlakRing, off as it comes)
	const bool bFlakRing = FlakRingAsked();
	// (a town's guns are drawn firing once, however many raids are over it; with the far side's seam, once in each
	// turn of the world that has a raid over it: the town's number, and the turn)
	TArray<FIntPoint, TInlineAllocator<8>> TownsFiring;
	// (The mend of 25 November 2026. A raid is up to four flights, and each was puffing for itself: three times the
	// puffs, and pale puffs over a gunned town beside its heavy bursts, since the rules name the guns on the first
	// flight alone. Light flak is fired by one flight a town at a time, over a province of the map, and never over
	// a town with guns built or with a flight under its heavy guns.)
	TArray<int32, TInlineAllocator<8>> TownsPuffing;
	auto TownGunned = [this](int32 Town) -> bool
	{
		if (ShownBuildings.IsValidIndex(Town) && ShownBuildings[Town].Z > 0)
		{
			return true;
		}
		for (const FFlight& Raiding : Flights)
		{
			if (Raiding.Sortie.Region == Town && Raiding.Sortie.Flak > 0)
			{
				return true;
			}
		}
		return false;
	};
	for (int32 F = 0; F < Flights.Num(); ++F)
	{
		FFlight& Flight = Flights[F];
		if (Flight.At.IsZero())
		{
			continue;
		}
		if (Flight.Sortie.Flak > 0 && Flight.Share > 0.3 && Flight.Share < 0.64)
		{
			// (Brian, 16 November 2026: "dont forget to show flak when bombers go to a target with AA guns"; and of the
			// solid bursts, "its solid", "and looks like a ball": he picked hanging smoke. The art director's brief.)
			// The guns fire while the bombers are over a defended target: light guns about every two and a half seconds,
			// medium every one and a half, heavy every second, in ragged time. Each burst is born at the bombers'
			// height within about two wingspans of the formation, more of them behind than ahead and never dead on a
			// machine; and there it stays. When the bombers turn for home no more are fired, and those in the air
			// finish their fade.
			const double Wings = (Flight.Sortie.bBombers ? 8.0 : 5.0) * Table; // (a bomber's wings as it is built for a raid)
			// (The art director's round 36: from playing height she counted four or five where the brief asks nine to
			// eleven for heavy guns. A burst is seen for six seconds from there, so the heavy guns fire about every six
			// tenths of a second.)
			// (Round 37: at 0.6 s six or seven of ten could be counted from playing height, where nine to eleven are asked.)
			const double Every = Flight.Sortie.Flak >= 3 ? 0.45 : Flight.Sortie.Flak == 2 ? 0.8 : 1.5;
			const FVector Onward = FVector(Flight.Dir.X, Flight.Dir.Y, 0.0).GetSafeNormal();
			const FVector Abeam(-Onward.Y, Onward.X, 0.0);
			auto FireOne = [&](double Ago)
			{
				FRandomStream Chance(++FlakFired * 7919 + F * 131);
				// (round the formation as it is now, a little more behind than ahead: the wake is made by the bombers flying on)
				// (The art director's round 37: from playing height the bursts over heavy guns ran together into about
				// four. They are fired over a wider piece of sky, about two and a half wingspans either side.)
				const double Ahead = Chance.FRandRange(-2.1f, 1.7f), Rise = Chance.FRandRange(-0.6f, 0.45f);
				double Aside = Chance.FRandRange(-2.6f, 2.6f);
				// (the leader, and the two that fly behind it and to its sides)
				for (const FVector2D& Machine : { FVector2D(0.0, 0.0), FVector2D(-1.1, 0.95), FVector2D(-1.1, -0.95) })
				{
					if (FMath::Abs(Ahead - Machine.X) < 0.8 && FMath::Abs(Aside - Machine.Y) < 0.65)
					{
						Aside += Aside < Machine.Y ? -0.75 : 0.75;
					}
				}
				FFlakBurst& Burst = FlakBursts.AddDefaulted_GetRef();
				// (one fired some seconds ago was fired where the formation then was: about a third of a wingspan a second back)
				Burst.At = Flight.At + (Onward * (Ahead - 0.33 * Ago) + Abeam * Aside + FVector(0.0, 0.0, Rise)) * Wings;
				Burst.Born = Now - Ago;
				Burst.Seed = Chance.RandRange(1, 1 << 20);
				Burst.Wings = Wings;
				Burst.Over = Flight.Sortie.Region; // (seam-4's mend: whose raid it is, for its turn of the world)
				Burst.From = Flight.Sortie.From;
				// (a burst that goes off now, within about a wingspan and a quarter of the leader, jolts the formation:
				// up to three and a half degrees of roll and one and a half of nod, the nearer the more)
				const double Near = 1.0 - FMath::Clamp(FVector::Distance(Burst.At, Flight.At) / (1.25 * Wings), 0.0, 1.0);
				if (Ago <= 0.0 && Near > 0.15)
				{
					Flight.JoltAt = Now;
					Flight.JoltRoll = (Aside < 0.0 ? 1.0 : -1.0) * FMath::Lerp(1.2, 3.5, Near);
					Flight.JoltPitch = (Rise < 0.0 ? 1.0 : -1.0) * FMath::Lerp(0.4, 1.5, Near);
				}
			};
			if (Flight.NextFlak <= 0.0)
			{
				Flight.NextFlak = Now;
				// (a raid held still for its picture is shown as it would be some seconds into the fire, with its wake)
				for (double Ago = 8.2; RaidHeldAt >= 0.0 && Ago > 0.4; Ago -= Every)
				{
					FireOne(Ago);
				}
			}
			if (Now >= Flight.NextFlak && FlakBursts.Num() < 70)
			{
				FireOne(0.0);
				Flight.NextFlak = Now + Every * FMath::FRandRange(0.6, 1.4);
			}
			// (Release 2, the flak brief, section 2b; tc.Look.FlakRing.) THE GUNS FIRE. While the raid is in its firing
			// window each pit of the town shows a muzzle flash and a short grey puff at its barrel's mouth, each gun in
			// its own time, so that the ring ripples and never fires as one. They are drawn at the barrels' known
			// mouths: the barrels are in the merged mesh and do not turn. From the top, the flash alone.
			const TArray<FFlakGun>* TownGuns = bFlakRing ? FlakGuns.Find(Flight.Sortie.Region) : nullptr;
			// (The far side, seam-2, where it meets the flak ring.) A raid over the chart's seam is flown to its
			// target's place a world on (UpdateSorties: the turn nearest its airfield), where the buildings' twin
			// stands the town's pits. The guns' fire is drawn there with the raid, in the turn of the world nearest
			// the flight, and not a world away under an empty sky; and two raids on one town flown in two turns each
			// have the guns under them. With the far side off, or tc.Far.OverSeam off, the span is nothing, the turn
			// is 0 and the fire is drawn at the pits as it always was.
			const double SeamSpan = (TownGuns && TownGuns->Num() > 0 && OverSeam()) ? Data.WorldWidth * UnitScale : 0.0;
			const int32 RaidTurn = SeamSpan > 0.0 ? FMath::RoundToInt32((Flight.At.Y - (*TownGuns)[0].Muzzle.Y) / SeamSpan) : 0;
			const FIntPoint FiringKey(Flight.Sortie.Region, RaidTurn);
			if (TownGuns && TownGuns->Num() > 0 && !TownsFiring.Contains(FiringKey))
			{
				TownsFiring.Add(FiringKey);
				const double GunUnit = UnitScale * 2.1; // (the size the pits are built to)
				static const bool bMuzzleHeld = FParse::Param(FCommandLine::Get(), TEXT("TheaterFlakFlashHeld")); // (for a still: every flash held)
				const bool bFromTop = CameraDistance > 9000.0f;
				const float NightDim = FMath::Lerp(1.0f, 0.6f, GetMoonlight()); // (let down by night, when the eye is wide open)
				// (never under about eight points across, so that it is seen from the working height)
				const double FlashOut = FMath::Max(0.2 * GunUnit, 0.0015 * CameraDistance);
				const FLinearColor MuzzleBody(FColor(214, 128, 52)), MuzzleHeart(FColor(238, 176, 96)), GunSmoke(0.46f, 0.46f, 0.47f);
				const FVector DownTheWind = Wind.GetSafeNormal();
				const FVector ToRaid(0.0, RaidTurn * SeamSpan, 0.0); // (whole worlds along the chart, to the raid's turn)
				// (The mend of 25 November 2026. Each gun had a reload of its own, 1.7 to 2.4 seconds: guns on different
				// clocks drift through one another, and every so often several fired within a frame or two, as one. A
				// town's guns now load to ONE time, the town's own, and each fires at its turn round the ring and a
				// little late of it, by up to four tenths of the gap to the next: so the fire runs round the ring and
				// no two guns ever fire together.)
				const double Reload = FRandomStream(Flight.Sortie.Region * 613 + 11).FRandRange(1.7f, 2.4f);
				// (And the smoke at the barrels came and went at a step as the raid crossed into and out of its firing
				// window. It comes in over the window's first tenth and goes out over its last, with the raid that is
				// deepest in its window over this town.)
				float InWindow = 0.0f;
				for (const FFlight& RaidOver : Flights)
				{
					if (RaidOver.Sortie.Region == Flight.Sortie.Region && RaidOver.Sortie.Flak > 0 && !RaidOver.At.IsZero())
					{
						InWindow = FMath::Max(InWindow, static_cast<float>(FMath::SmoothStep(0.30, 0.33, RaidOver.Share) * (1.0 - FMath::SmoothStep(0.60, 0.64, RaidOver.Share))));
					}
				}
				for (int32 GunNo = 0; GunNo < TownGuns->Num(); ++GunNo)
				{
					const FFlakGun& Barrel = (*TownGuns)[GunNo];
					const FVector Mouth = Barrel.Muzzle + ToRaid; // (the barrel's mouth, in the raid's turn of the world)
					FRandomStream Own(Flight.Sortie.Region * 613 + GunNo * 97 + 11);
					// (a round every two seconds or so: its turn in the ring, and a little late of it)
					const double SinceShot = FMath::Fmod(Now + Reload * ((GunNo + Own.FRandRange(0.0f, 0.4f)) / TownGuns->Num()), Reload);
					if (SinceShot < (bMuzzleHeld ? 0.6 : 0.09))
					{
						bDrawingLight = true;
						DrawSmear(Mouth + Barrel.Bore * (0.9 * FlashOut), Barrel.Bore, 1.5 * FlashOut, 0.7 * FlashOut, MuzzleBody, 0.9f * NightDim, 0.6);
						Draw(Mouth + Barrel.Bore * (0.3 * FlashOut), 0.55 * FlashOut, MuzzleHeart, 0.95f * NightDim);
						bDrawingLight = false;
					}
					if (!bFromTop && SinceShot > 0.04 && SinceShot < 1.3)
					{
						// (its smoke: thrown up the barrel's line and slowing, then taken by the wind; thin within a second)
						const double SmokeAged = SinceShot / 1.3;
						const FVector SmokeAt = Mouth + Barrel.Bore * ((0.25 + 0.7 * FMath::Sqrt(SmokeAged)) * GunUnit) + DownTheWind * (0.3 * SmokeAged * GunUnit);
						Draw(SmokeAt, FMath::Max((0.16 + 0.34 * SmokeAged) * GunUnit, 0.6 * FlashOut), GunSmoke, InWindow * static_cast<float>(0.55 * (1.0 - SmokeAged) * FMath::SmoothStep(0.04, 0.12, SinceShot)));
					}
				}
			}
		}
		else if (bFlakRing && Flight.Sortie.bLightFlak && Flight.Sortie.bBombers && Flight.Share > 0.3 && Flight.Share < 0.64
			&& Data.Regions.IsValidIndex(Flight.Sortie.Region) && !TownsPuffing.Contains(Flight.Sortie.Region) && !TownGunned(Flight.Sortie.Region))
		{
			TownsPuffing.Add(Flight.Sortie.Region); // (one flight fires a town's light flak at a time: the mend above)
			// (Release 2, the flak brief, section 2c; the air thread sends "light" on a bomber flight over a target with
			// no built guns.) LIGHT FLAK. No guns are built here, and the rules still cost the raid bombers there. It is
			// seen only by its fire: a pale grey puff, half the size of a heavy burst, at two thirds of the bombers'
			// height, one every 2.2 seconds and gone in two and a half. Something is shooting, but not much: it jolts
			// nobody and throws no shadow.
			const double LightWings = 0.5 * 8.0 * Table; // (half a bomber's wings: every size of a burst is in these)
			auto PuffOne = [&](double HeldAgo)
			{
				FRandomStream Dice(++FlakFired * 7919 + F * 131);
				const FVector Heading = FVector(Flight.Dir.X, Flight.Dir.Y, 0.0).GetSafeNormal();
				const FVector Beside(-Heading.Y, Heading.X, 0.0);
				// (over the same piece of sky as the heavy guns', measured in a bomber's whole wings)
				const FVector OverAt = Flight.At + (Heading * (Dice.FRandRange(-2.1f, 1.7f) - 0.33 * HeldAgo) + Beside * Dice.FRandRange(-2.6f, 2.6f)) * (2.0 * LightWings);
				const FVector2D UnderIt = WorldToMap(OverAt);
				const double Floor = SurfacePoint(UnderIt.X, UnderIt.Y, 0.0).Z;
				FFlakBurst& Puffed = FlakBursts.AddDefaulted_GetRef();
				Puffed.At = FVector(OverAt.X, OverAt.Y, Floor + (Flight.At.Z - Floor) * (2.0 / 3.0) + Dice.FRandRange(-0.3f, 0.3f) * LightWings);
				Puffed.Born = Now - HeldAgo;
				Puffed.Seed = Dice.RandRange(1, 1 << 20);
				Puffed.Wings = LightWings;
				Puffed.bLight = true;
				Puffed.Over = Flight.Sortie.Region; // (seam-4's mend: whose raid it is, for its turn of the world)
				Puffed.From = Flight.Sortie.From;
			};
			if (Flight.NextFlak <= 0.0)
			{
				Flight.NextFlak = Now;
				if (RaidHeldAt >= 0.0)
				{
					PuffOne(1.4); // (a raid held still for its picture has one in the air already)
				}
			}
			if (Now >= Flight.NextFlak && FlakBursts.Num() < 70)
			{
				PuffOne(0.0);
				Flight.NextFlak = Now + 2.2 * FMath::FRandRange(0.8, 1.2);
			}
		}
		if (Flight.bEngaged)
		{
			// Tracer: the fighters at the bombers, and the bombers' gunners back at them.
			// (Brian, 18 November 2026: "i want tracer fire shown as well. 1 out of every 5 bullets with yellow". The
			// aircraft modeller's sheet for it, brought to the table's scale, where a bomber's wings span ten units
			// and the fighters fly within a wingspan of it. Each gun fires thirteen rounds a second while its side
			// fires. Every fifth round is a tracer and the four between are not drawn; each gun's belt starts out of
			// step with its neighbours'. A tracer leaves along the line to its mark, a little scattered, at its own
			// speed and its shooter's, flies straight whatever its shooter then does, and burns out after six tenths
			// of a second, well past its mark. Every round, drawn or not, flickers at its muzzle for one frame.
			// Before this the tracer was four amber streaks in a row that turned with the fighters.)
			const double Small = 0.55 * Table, Metre = 0.34 * Table; // (a table unit to the metre, as the fighters and the bombers are built)
			const double RoundsASecond = 13.0;
			static const int32 BeltStarts[7] = { 0, 3, 1, 4, 2, 0, 3 };
			// (the fighters fire in bursts, and the gunners back at them in the gaps and a little over)
			// (The modeller on the first pictures of the streams: a stream should last half a second of fire or more. The
			// bursts were two thirds of a second in every second and a bit; they are a second and a third in every two
			// and a bit, three or four tracers a gun.)
			auto FightersFireAt = [](double When) { return FMath::Frac(When * 0.45) < 0.6; };
			auto GunnersFireAt = [](double When) { return FMath::Frac(When * 0.45 + 0.5) < 0.6; };
			const FTransform Pair = Flight.Rivals ? Flight.Rivals->GetComponentTransform() : FTransform(Flight.RivalAt);
			const FTransform Vic = Flight.Formation ? Flight.Formation->GetComponentTransform() : FTransform(Flight.At);
			const FVector Fighters[2] = { Pair.TransformPosition(FVector::ZeroVector), Pair.TransformPosition(FVector(10.0, 9.0, 1.5) * Small) };
			const FVector Bombers[3] = { Vic.TransformPosition(FVector::ZeroVector), Vic.TransformPosition(FVector(26.0, 22.0, 3.0) * Metre), Vic.TransformPosition(FVector(26.0, -22.0, 3.0) * Metre) };
			// (how fast each side is going, from where it was a frame ago)
			const double Since = Now - Flight.SeenWhen;
			const bool bSeen = Flight.SeenWhen > 0.0 && Since > 0.0005 && Since < 0.25;
			const FVector BomberSpeed = bSeen ? (Flight.At - Flight.SeenAt) / Since : FVector::ZeroVector;
			const FVector FighterSpeed = bSeen ? (Flight.RivalAt - Flight.SeenRivalAt) / Since : FVector::ZeroVector;
			// (The modeller's look rule of 16:17 UTC the same day: tracers are streams, not scattered dashes. Each gun lays
			// its tracers evenly along one line from its muzzle to a point led ahead of its mark; the whole stream
			// wanders slowly by about a third of a degree, and each round is scattered a little about it. A fighter's
			// paired streams meet at its mark; the bombers' streams converge on one fighter.)
			auto FireGun = [&](int32 Gun, bool bFighters, const FVector& Muzzle, const FVector& Mark, const FVector& ShooterSpeed, const FVector& MarkSpeed)
			{
				double& Next = Flight.NextRound[Gun];
				if (Next <= 0.0 || Next < Now - 0.25)
				{
					Next = Now; // (the first round, or the first after the game stood still)
				}
				bool bFlicker = false;
				while (Next <= Now)
				{
					const int32 Round = Flight.RoundsFired[Gun]++;
					if (bFighters ? FightersFireAt(Next) : GunnersFireAt(Next))
					{
						bFlicker = true;
						if ((Round + BeltStarts[Gun]) % 5 == 0 && TracerRounds.Num() < 160)
						{
							FRandomStream Chance(Round * 7919 + Gun * 131 + F * 17);
							// (it burns out about as far past its mark as its mark is from it)
							const double Range = FMath::Max(2.0 * FVector::Distance(Muzzle, Mark), 9.0 * Table);
							const double Speed = Range / TracerBurns;
							// (led: aimed where the mark will be when the round has flown to it)
							const FVector Led = Mark + MarkSpeed * (FVector::Distance(Muzzle, Mark) / Speed);
							const FVector Aim = (Led - Muzzle).GetSafeNormal();
							// (the stream's slow wander, a third of a degree; and each round's own scatter, a little under half a degree)
							const FVector Wander(FMath::Sin(Next * 0.7 + Gun * 1.9), FMath::Sin(Next * 0.53 + Gun * 2.7), FMath::Sin(Next * 0.61 + Gun * 1.0));
							const FVector Scatter(Chance.FRandRange(-1.0f, 1.0f), Chance.FRandRange(-1.0f, 1.0f), Chance.FRandRange(-1.0f, 1.0f));
							FTracerRound& Tracer = TracerRounds.AddDefaulted_GetRef();
							Tracer.From = Muzzle;
							Tracer.Velocity = (Aim + Wander * 0.006 + Scatter * 0.007).GetSafeNormal() * Speed + ShooterSpeed;
							Tracer.Born = Next;
						}
					}
					Next += 1.0 / RoundsASecond;
				}
				if (bFlicker)
				{
					// (the flash at the muzzle: small, pale yellow, and gone with the frame. Seen at three tenths of a
					// unit and near white in the first picture, it was a lamp on each wing.)
					bDrawingLight = true;
					Draw(Muzzle, 0.15 * Table, FLinearColor(0.82f, 0.7f, 0.3f), 0.7f);
					bDrawingLight = false;
				}
			};
			for (int32 K = 0; K < 2; ++K)
			{
				// (each fighter at a bomber of its own: the leader's, and the nearer of the two behind it)
				const FVector Mark = K == 0 ? Bombers[0] : (FVector::DistSquared(Fighters[1], Bombers[1]) < FVector::DistSquared(Fighters[1], Bombers[2]) ? Bombers[1] : Bombers[2]);
				for (int32 Wing = 0; Wing < 2; ++Wing)
				{
					// (a gun in each wing, two metres out and a metre and a half ahead: a model's front is toward -X)
					const FVector Muzzle = Fighters[K] + Pair.TransformVector(FVector(-1.5, Wing == 0 ? -2.0 : 2.0, 0.0) * Small);
					FireGun(K * 2 + Wing, true, Muzzle, Mark, FighterSpeed, BomberSpeed);
				}
			}
			// (a gunner in each bomber, from its back: all three at one fighter, the one nearer their leader)
			const FVector& Hunted = FVector::DistSquared(Bombers[0], Fighters[0]) <= FVector::DistSquared(Bombers[0], Fighters[1]) ? Fighters[0] : Fighters[1];
			for (int32 K = 0; K < 3; ++K)
			{
				FireGun(4 + K, false, Bombers[K] + FVector(0.0, 0.0, 0.6 * Table), Hunted, BomberSpeed, FighterSpeed);
			}
		}
		Flight.SeenAt = Flight.At;
		Flight.SeenRivalAt = Flight.RivalAt;
		Flight.SeenWhen = Now;
		if (Flight.Sortie.Loss >= 0.1f && Flight.Share > 0.5)
		{
			// One of them has been hit: it trails smoke all the way home. (The art director's round 37: by night it is
			// the one the searchlights held as the bombs fell: the leader or one of the two behind it, by the place.)
			FVector Hit = Flight.At;
			double Behind = 7.0;
			if (Flight.Formation && Flight.Sortie.bBombers)
			{
				static const FVector Seats[3] = { FVector(0.0, 0.0, 0.0), FVector(26.0, 22.0, 3.0), FVector(26.0, -22.0, 3.0) };
				Hit = Flight.Formation->GetComponentTransform().TransformPosition(Seats[FMath::Max(Flight.Sortie.Region, 0) % 3] * (0.34 * Table));
				Behind = 3.0;
			}
			for (int32 K = 0; K < 7; ++K)
			{
				const double Back = (Behind + K * 1.9) * Table;
				Draw(Hit - Flight.Dir * Back + FVector(0.0, 0.0, -0.25 * Table * K + 0.4 * Table * FMath::Sin(Now * 2.0 + K)), (0.7 + 0.3 * K) * Table, Dark, 0.55f - 0.06f * K);
			}
		}
	}

	// The tracer rounds in flight: each a solid streak of yellow, paler at its heart, with a tail that thins away
	// behind it. Never drawn under about three pixels across, however high the eye stands. (The modeller's yellow is
	// to be seen at about 213, 199, 104: not amber, and not white. A puff's ink is laid in the light's own measure
	// and the screen's is another: laid at those figures it was seen near white, 236, 229, 172. It is laid at the
	// figures that are seen as his.)
	// (Its length: the modeller's streak is half a bomber's wingspan long, which on this table, where the fighters fly
	// within a wingspan of the bombers, would join gun to mark in one line. First drawn at eight tenths of a unit,
	// as the old streaks were, it could hardly be seen beside a bomber. It is three units, under a third of a wingspan.)
	{
		const FLinearColor TracerYellow(0.665f, 0.571f, 0.138f), TracerPale(0.87f, 0.80f, 0.42f), TracerEdge(0.09f, 0.06f, 0.02f);
		bDrawingLight = true;
		for (int32 K = TracerRounds.Num() - 1; K >= 0; --K)
		{
			const FTracerRound& Round = TracerRounds[K];
			const double Age = Now - Round.Born;
			if (Age < 0.0 || Age >= TracerBurns)
			{
				TracerRounds.RemoveAtSwap(K, EAllowShrinking::No);
				continue;
			}
			const FVector Head = Round.From + Round.Velocity * Age;
			const FVector Flies = Round.Velocity.GetSafeNormal();
			const double Pixel = FMath::Max(FVector::Distance(EyeAt, Head), 1.0) / PixelsAtOne; // (what one pixel of the window spans there)
			// (The modeller on the first pictures: over a pale place, an army's sand-coloured base, the dashes were
			// faint. The pale heart is a full three pixels across at the least, the yellow round it wider, and a thin
			// dark edge lies under both, as a bright thing has against a bright ground.)
			const double HalfWide = FMath::Max(0.09 * Table, 2.6 * Pixel);
			// (and no longer than it has yet flown: it grows out of the muzzle)
			const double Long = FMath::Min(FMath::Max(3.0 * Table, 9.0 * HalfWide), FMath::Max(Round.Velocity.Size() * Age, 2.0 * HalfWide));
			// (it dwindles over the last two fifths of its flight, and is out at the end)
			const float Burning = static_cast<float>(1.0 - FMath::SmoothStep(0.6 * TracerBurns, TracerBurns, Age));
			DrawSmear(Head - Flies * (0.42 * Long), Flies, 0.46 * Long, 1.5 * HalfWide, TracerEdge, 0.5f * Burning, 0.8, 0.42f * Burning); // the dark edge under it
			DrawSmear(Head - Flies * (0.9 * Long), Flies, 0.6 * Long, 0.55 * HalfWide, TracerYellow, 0.6f * Burning, 0.5); // the tail, thinning
			DrawSmear(Head - Flies * (0.4 * Long), Flies, 0.4 * Long, HalfWide, TracerYellow, 1.0f * Burning, 0.75, 0.95f * Burning); // the streak, solid
			DrawSmear(Head - Flies * (0.25 * Long), Flies, 0.2 * Long, FMath::Max(0.5 * HalfWide, 1.5 * Pixel), TracerPale, 0.9f * Burning, 0.6, 0.7f * Burning); // its paler heart
		}
		bDrawingLight = false;
	}

	// Flak: every burst in the air, as hanging smoke. Sizes are in the bombers' wingspans, long (down the wind) by
	// across, as the art director painted them: new, nearly black and 0.7 by 0.5; at eight seconds grey, 2.1 by 0.8 and
	// half see-through; gone by nine. From playing height a burst is held larger than life and darker, and is gone by
	// six; from the top it is a mark of smoke, never smaller than about fourteen points by nine, with no flash.
	{
		static const bool bFlakTest = FParse::Param(FCommandLine::Get(), TEXT("TheaterFlakTest"));
		if (bFlakTest)
		{
			// ("-TheaterFlakTest": three bursts held at half a second, three seconds and eight, side by side over whatever
			// the eye is looking at, to be looked at)
			FlakBursts.Reset();
			for (int32 K = 0; K < 3; ++K)
			{
				FFlakBurst& Burst = FlakBursts.AddDefaulted_GetRef();
				Burst.Wings = 8.0 * Table;
				// (over the ground itself, wherever the table's top stands)
				const FVector2D TestUnder = WorldToMap(CameraFocus + Right * ((K - 1) * 2.6 * Burst.Wings));
				Burst.At = SurfacePoint(TestUnder.X, TestUnder.Y, 0.0) + FVector(0.0, 0.0, 0.7 * Burst.Wings);
				Burst.Born = -(K == 0 ? 0.5 : K == 1 ? 3.0 : 8.0); // (a time before the game began: such a burst is held at that age)
				Burst.Seed = 4711 + K * 977;
			}
		}
		const double Playing = FMath::SmoothStep(2600.0, 4600.0, static_cast<double>(CameraDistance)); // (0 from close by, 1 from playing height and above)
		const bool bTop = CameraDistance > 9000.0f;
		const float Moonlit = GetMoonlight();
		// (from playing height, and by night, the pale tail is dropped: gone by six seconds)
		const double SmokeLife = FMath::Lerp(9.0, 6.0, FMath::Max(Playing, static_cast<double>(Moonlit)));
		// The sheet of painted shapes and the flat pictures cut from it: made once, with room for every burst.
		const int32 MaxFlakPictures = 72 * 6;
		if (!bFlakSmokeMade)
		{
			bFlakSmokeMade = true;
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_FlakSmoke.M_FlakSmoke")))
			{
				FlakSheet = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir() / TEXT("Effects/flak-smoke-atlas-4x3.png"));
				if (FlakSheet)
				{
					FlakSmokeInk = UMaterialInstanceDynamic::Create(Base, this);
					FlakSmokeInk->SetTextureParameterValue(TEXT("Sheet"), FlakSheet);
				}
			}
			if (!FlakSmokeInk)
			{
				UE_LOG(LogTheater, Error, TEXT("Flak: the smoke's material or its sheet of shapes is missing (M_FlakSmoke, Content/Effects/flak-smoke-atlas-4x3.png)"));
			}
			FlakVertices.Init(FVector::ZeroVector, MaxFlakPictures * 4);
			FlakVertices[1] = FVector(-20000.0, -20000.0, 0.0); // (so that the mesh is never thought out of sight)
			FlakVertices[2] = FVector(20000.0, 20000.0, 2000.0);
			FlakPlaces.Init(FVector2D::ZeroVector, MaxFlakPictures * 4);
			FlakColours.Init(FColor(0, 0, 0, 0), MaxFlakPictures * 4);
			TArray<int32> Corners;
			Corners.Reserve(MaxFlakPictures * 6);
			for (int32 Picture = 0; Picture < MaxFlakPictures; ++Picture)
			{
				const int32 First = Picture * 4;
				Corners.Append({ First, First + 1, First + 2, First, First + 2, First + 3 });
			}
			TArray<FVector> Upward;
			Upward.Init(FVector::UpVector, FlakVertices.Num());
			FlakSmokeMesh->CreateMeshSection(0, FlakVertices, Corners, Upward, FlakPlaces, FlakColours, TArray<FProcMeshTangent>(), false);
			FlakSmokeMesh->SetMaterial(0, FlakSmokeInk);
			FlakSmokeMesh->SetCastShadow(false);
			FlakSmokeMesh->SetTranslucentSortPriority(1); // (over the smoke of the ground, which it hangs high above)
		}
		// (The same painted shape as light, for smoke inside a searchlight's beam: a second section of the flak's own
		// mesh, in a material that adds its light.)
		const int32 MaxLitShapes = 36;
		if (FlakLitVertices.Num() == 0 && FlakSmokeInk && FlakSheet)
		{
			FlakLitVertices.Init(FVector::ZeroVector, MaxLitShapes * 4);
			FlakLitVertices[1] = FVector(-20000.0, -20000.0, 0.0);
			FlakLitVertices[2] = FVector(20000.0, 20000.0, 2000.0);
			FlakLitPlaces.Init(FVector2D::ZeroVector, MaxLitShapes * 4);
			FlakLitColours.Init(FColor(0, 0, 0, 0), MaxLitShapes * 4);
			TArray<int32> LitCorners;
			for (int32 Picture = 0; Picture < MaxLitShapes; ++Picture)
			{
				const int32 First = Picture * 4;
				LitCorners.Append({ First, First + 1, First + 2, First, First + 2, First + 3 });
			}
			TArray<FVector> LitUp;
			LitUp.Init(FVector::UpVector, FlakLitVertices.Num());
			if (UMaterialInterface* LitBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_FlakLit.M_FlakLit")))
			{
				FlakLitInk = UMaterialInstanceDynamic::Create(LitBase, this);
				FlakLitInk->SetTextureParameterValue(TEXT("Sheet"), FlakSheet);
				FlakSmokeMesh->CreateMeshSection(1, FlakLitVertices, LitCorners, LitUp, FlakLitPlaces, FlakLitColours, TArray<FProcMeshTangent>(), false);
				FlakSmokeMesh->SetMaterial(1, FlakLitInk);
			}
		}
		int32& LitShape = FlakLitShapes;
		auto LightShape = [&](const FVector& Middle, const FVector& HalfAlong, const FVector& HalfAcross, int32 WhichShape, int32 WhichAge, float Strong, bool bTurnedOver)
		{
			if (!FlakLitInk || LitShape >= MaxLitShapes || Strong <= 0.004f)
			{
				return;
			}
			const int32 First = LitShape++ * 4;
			const FColor Inked = FLinearColor(200.0f / 255.0f, 222.0f / 255.0f, 1.0f, FMath::Clamp(Strong, 0.0f, 1.0f)).ToFColor(false);
			const double U0 = WhichShape * 0.25, U1 = U0 + 0.25, V0 = WhichAge / 3.0, V1 = V0 + 1.0 / 3.0;
			const FVector Over = bTurnedOver ? -HalfAcross : HalfAcross;
			FlakLitVertices[First] = Middle - HalfAlong + Over;
			FlakLitVertices[First + 1] = Middle + HalfAlong + Over;
			FlakLitVertices[First + 2] = Middle + HalfAlong - Over;
			FlakLitVertices[First + 3] = Middle - HalfAlong - Over;
			FlakLitPlaces[First] = FVector2D(U0, V0);
			FlakLitPlaces[First + 1] = FVector2D(U1, V0);
			FlakLitPlaces[First + 2] = FVector2D(U1, V1);
			FlakLitPlaces[First + 3] = FVector2D(U0, V1);
			for (int32 K = 0; K < 4; ++K)
			{
				FlakLitColours[First + K] = Inked;
			}
		};
		// One painted shape: the middle of its square, half the square along the wind and half across it, which shape
		// (0 to 3) at which age (0 new, 1 middle, 2 old), its colour and how much of it is left (a half is as painted).
		auto DrawShape = [&](const FVector& Middle, const FVector& HalfAlong, const FVector& HalfAcross, int32 WhichShape, int32 WhichAge, const FLinearColor& Ink, float Kept, bool bTurnedOver)
		{
			if (FlakPicture >= MaxFlakPictures || Kept <= 0.004f)
			{
				return;
			}
			const int32 First = FlakPicture++ * 4;
			const FColor Inked = FLinearColor(Ink.R, Ink.G, Ink.B, FMath::Clamp(Kept, 0.0f, 1.0f)).ToFColor(false);
			const double U0 = WhichShape * 0.25, U1 = U0 + 0.25, V0 = WhichAge / 3.0, V1 = V0 + 1.0 / 3.0;
			const FVector Over = bTurnedOver ? -HalfAcross : HalfAcross;
			FlakVertices[First] = Middle - HalfAlong + Over;
			FlakVertices[First + 1] = Middle + HalfAlong + Over;
			FlakVertices[First + 2] = Middle + HalfAlong - Over;
			FlakVertices[First + 3] = Middle - HalfAlong - Over;
			FlakPlaces[First] = FVector2D(U0, V0);
			FlakPlaces[First + 1] = FVector2D(U1, V0);
			FlakPlaces[First + 2] = FVector2D(U1, V1);
			FlakPlaces[First + 3] = FVector2D(U0, V1);
			for (int32 K = 0; K < 4; ++K)
			{
				FlakColours[First + K] = Inked;
			}
		};
		const FVector Downwind = Wind.GetSafeNormal();
		const FVector SunSide = FVector(-0.55, -0.25, 0.8).GetSafeNormal(); // (the lamp of the table: from the west, and high)
		int32 Shown = 0;
		for (int32 B = FlakBursts.Num() - 1; B >= 0; --B)
		{
			FFlakBurst& Burst = FlakBursts[B];
			const double Age = Burst.Born < 0.0 ? -Burst.Born : Now - Burst.Born;
			// (release 2, the flak brief, 2c: light flak's puff is gone in two and a half seconds)
			const double LastsFor = Burst.bLight ? 2.5 : SmokeLife;
			if (Age >= LastsFor + (Burst.bLight ? 0.0 : 0.5) && Burst.Born >= 0.0)
			{
				if (Age >= (Burst.bLight ? 2.6 : 9.5))
				{
					FlakBursts.RemoveAtSwap(B, EAllowShrinking::No);
				}
				continue;
			}
			// (Release 2, the flak brief, 2c, with the ring on: by night a burst is an orange flash for a fifth of a
			// second and then a smoke that is barely there.)
			const bool bNightBurst = bFlakRing && Moonlit > 0.5f;
			// (The mend of 25 November 2026: the smoke was let down to three tenths at a step as the moonlight passed
			// the half, and every burst in the air thinned in one frame at dusk and thickened in one at dawn. It
			// thins between three and seven tenths of the moonlight; the flash keeps its day and night rules. The
			// burst's shadow on the ground thins with its smoke.)
			const float NightThin = bFlakRing ? FMath::Lerp(1.0f, 0.3f, FMath::SmoothStep(0.3f, 0.7f, Moonlit)) : 1.0f;
			if (bTop && ++Shown > 5)
			{
				continue; // (from the top: three to five, the newest)
			}
			FRandomStream Shape(Burst.Seed);
			// (The modeller's flak rule, passed by the art director on 18 November 2026: no two bursts the same size.
			// Each is between 0.65 and 1.35 of the size the brief gave them all; its dice are its own, so that the
			// shapes the bursts had are the shapes they keep.)
			const double W = Burst.Wings * (0.65 + 0.7 * FRandomStream(Burst.Seed * 31 + 7).FRand());
			// (larger than life from playing height; and from the top never under about fourteen points by nine)
			// (round 37: at 1.55 a burst was 40 by 34 px from playing height and eleven of them ran together; the brief's 28 by 21)
			const double Held = FMath::Lerp(1.0, 1.1, Playing);
			// (it fades out, it does not pop: over its last second and a half; from playing height from the four-and-a-half-second look)
			const double Going = 1.0 - FMath::SmoothStep(SmokeLife - FMath::Lerp(1.5, 1.0, Playing), SmokeLife, Age);
			// (it drifts the way the flags fly: about half a wingspan over its life)
			const FVector Heartland = Burst.At + Downwind * (0.055 * W * Age) + FVector(0.0, 0.0, 0.01 * W * Age);
			// (The art director's round 36: "the lobes are round".) The smoke is her painted shapes, one or two a burst and
			// a different shape each time, turned to the wind and to the eye. The sheet has each shape at three ages, new
			// at about a tenth of a burst's nine seconds, middle at under a half, old at nine tenths: each melts into the
			// next, drawn at the one width the smoke has reached (0.77, 1.17 and 1.70 wingspans at those ages).
			static const double SheetAge[3] = { 0.1, 0.45, 0.9 }, SheetSquare[3] = { 1.21, 2.22, 3.95 }, SheetSmoke[3] = { 0.77, 1.17, 1.70 };
			// (Round 37, the count from playing height: seen from there a burst keeps the look of its first seconds,
			// small and dark, for the six seconds it is shown; its wide pale age could not be counted.)
			// (Round 38: about five of the dozen alive could be counted. From playing height a burst is at its darkest
			// and smallest for its first three seconds, and ages only a little in the three that follow.)
			const double Lived = FMath::Clamp(FMath::Lerp(Age / 9.0, Age <= 3.0 ? Age / 30.0 : 0.1 + (Age - 3.0) / 20.0, Playing), 0.0, 1.0);
			// (light flak's puff opens from seven tenths of its halved wingspan to one and a quarter over its short life)
			const double SmokeWide = Burst.bLight ? FMath::Lerp(0.7, 1.25, FMath::Clamp(Age / 2.5, 0.0, 1.0))
				: Lived < SheetAge[0] ? FMath::Lerp(0.55, SheetSmoke[0], Lived / SheetAge[0])
				: Lived < SheetAge[1] ? FMath::Lerp(SheetSmoke[0], SheetSmoke[1], (Lived - SheetAge[0]) / (SheetAge[1] - SheetAge[0]))
				: Lived < SheetAge[2] ? FMath::Lerp(SheetSmoke[1], SheetSmoke[2], (Lived - SheetAge[1]) / (SheetAge[2] - SheetAge[1]))
				: FMath::Lerp(SheetSmoke[2], 1.8, (Lived - SheetAge[2]) / (1.0 - SheetAge[2]));
			// (larger than life from playing height; and from the top never under about fourteen points long)
			const double SmokeLong = FMath::Max(SmokeWide * W * Held, bTop ? (Burst.bLight ? 0.0026 : 0.0052) * CameraDistance : 0.0);
			float AgeShare[3] = { 0.0f, 0.0f, 0.0f };
			if (Lived <= SheetAge[0]) { AgeShare[0] = 1.0f; }
			else if (Lived < SheetAge[1]) { AgeShare[1] = static_cast<float>((Lived - SheetAge[0]) / (SheetAge[1] - SheetAge[0])); AgeShare[0] = 1.0f - AgeShare[1]; }
			else if (Lived < SheetAge[2]) { AgeShare[2] = static_cast<float>((Lived - SheetAge[1]) / (SheetAge[2] - SheetAge[1])); AgeShare[1] = 1.0f - AgeShare[2]; }
			else { AgeShare[2] = 1.0f; }
			// (how much of it is left: as painted, a third thicker from playing height; in at its birth and out at its end, never popping)
			const float Remains = static_cast<float>(FMath::Min(0.5 * FMath::Lerp(1.0, 2.0, Playing), 1.0) * Going * FMath::SmoothStep(0.0, 0.12, Age));
			// (Smoke of every age takes the hour's light as the bombers do: about half by night, and bluer. The painted
			// shapes carry their own dark and grey. From playing height a burst is held a little darker.)
			const float Held2 = static_cast<float>(FMath::Lerp(1.0, 0.6, Playing));
			// (The same rule: a burst far from the eye is lifted by the haze between, paler and thinner. Counted from the
			// distance of what the eye looks at: nothing there, and all there is of it at twice as far.)
			const float Hazed = static_cast<float>(FMath::Clamp((FVector::Distance(EyeAt, Burst.At) / FMath::Max(static_cast<double>(CameraDistance), 1.0) - 1.0), 0.0, 1.0));
			const FLinearColor Hour = FMath::Lerp(SmokeHour * Held2, FLinearColor(SmokeHour.R, SmokeHour.G * 1.02f, SmokeHour.B * 1.06f).GetClamped(), 0.6f * Hazed);
			// (and it ages from black through charcoal to a grey-brown: the painted greys of its middle and old age are warmed)
			const FLinearColor Aged[3] = { Hour, FLinearColor(Hour.R, Hour.G * 0.95f, Hour.B * 0.88f), FLinearColor(Hour.R, Hour.G * 0.91f, Hour.B * 0.8f) };
			// (flat to the eye, with the wind along it: across it is square to both the wind and the eye's own line)
			const FVector Sight = FVector::CrossProduct(Right, Up);
			FVector AcrossWind = FVector::CrossProduct(Downwind, Sight);
			if (AcrossWind.SizeSquared() < 0.04)
			{
				AcrossWind = Up; // (looking straight down the wind)
			}
			AcrossWind = AcrossWind.GetSafeNormal();
			if (FVector::DotProduct(AcrossWind, Up) < 0.0)
			{
				AcrossWind = -AcrossWind;
			}
			const int32 FirstShape = Shape.RandRange(0, 3), SecondShape = (FirstShape + Shape.RandRange(1, 3)) % 4;
			const bool bTwo = Shape.FRand() < 0.55f, bTurned = Shape.FRand() < 0.5f;
			const double SecondAlong = Shape.FRandRange(0.22f, 0.42f), SecondUp = Shape.FRandRange(-0.16f, 0.16f), SecondSize = Shape.FRandRange(0.62f, 0.82f);
			// (its shadow first, on the ground where the bombers' shadows fall, at about three parts in ten; none from the top)
			if (!bTop && !Burst.bLight)
			{
				const FVector2D Under = WorldToMap(Heartland + SunSide * (-(Heartland.Z - GetActorLocation().Z) / SunSide.Z));
				const FVector OnGround = SurfacePoint(Under.X, Under.Y, 0.25);
				const FVector Level = FVector::CrossProduct(Downwind, FVector::UpVector).GetSafeNormal();
				for (int32 Row = 0; Row < 3; ++Row)
				{
					const double Square = 0.5 * SmokeLong * SheetSquare[Row] / SheetSmoke[Row];
					DrawShape(OnGround, Downwind * Square, Level * (Square * 0.9), FirstShape, Row, FLinearColor(0.0f, 0.0f, 0.0f), 0.3f * NightThin * Remains * AgeShare[Row], bTurned);
				}
			}
			// (Ruling 107; the art director's brief.) Smoke inside a searchlight's beam is lit by it, about 1.7 times as
			// bright as the beam is over clear air: with the flash, the only smoke by night that is brighter than the ground.
			float InBeam = 0.0f;
			for (const FLitBeam& Beam : LitBeams)
			{
				const double Along = FVector::DotProduct(Heartland - Beam.Lamp, Beam.Way);
				if (Along <= 0.0 || Along >= Beam.Long)
				{
					continue;
				}
				const double Off = FVector::Dist(Beam.Lamp + Beam.Way * Along, Heartland);
				const double Wide = Beam.Wings * (Along < Beam.ToHeight ? FMath::Lerp(0.07, 0.22, Along / Beam.ToHeight) : 0.26) + 0.35 * SmokeLong;
				const double Strong = Along < Beam.ToHeight ? FMath::Lerp(0.80, 0.36, Along / Beam.ToHeight) : FMath::Lerp(0.36, 0.0, (Along - Beam.ToHeight) / FMath::Max(Beam.Long - Beam.ToHeight, 1.0));
				InBeam = FMath::Max(InBeam, static_cast<float>(Strong * (1.0 - FMath::SmoothStep(0.5 * Wide, Wide, Off))) * Beam.Strength);
			}
			if (InBeam > 0.01f && !Burst.bLight)
			{
				for (int32 Row = 0; Row < 3; ++Row)
				{
					const double Square = 0.5 * SmokeLong * SheetSquare[Row] / SheetSmoke[Row];
					LightShape(Heartland, Downwind * Square, AcrossWind * Square, FirstShape, Row, 0.7f * 0.62f * InBeam * Remains * 2.0f * AgeShare[Row], bTurned);
				}
			}
			// ("-TheaterFlakFlashHeld" holds every flash for six tenths of a second, so that a still catches some: a
			// flash of two frames is in one still in ten. Its smoke is held as thin as it is in those two frames.)
			static const bool bFlashHeld = FParse::Param(FCommandLine::Get(), TEXT("TheaterFlakFlashHeld"));
			const float Thinned = Remains * (1.0f - 0.3f * Hazed) * (bFlashHeld && Age < 0.6 && Burst.Born >= 0.0 ? 0.15f : 1.0f) * NightThin;
			if (Burst.bLight)
			{
				// (The art director's mend of the flak brief's 2c, 25 November 2026: light flak is drawn with the soft puffs
				// in a set pale grey, not with the painted sheet, whose shapes are a heavy burst's. A warm off-white, about
				// 200, 198, 190 as it is seen, six tenths strong at its birth and fading evenly until it is gone at two and
				// a half seconds: one soft puff, and a smaller one a little down the wind of it. By night it takes the
				// hour's light as all smoke does, and is barely there.)
				const FLinearColor PaleGrey(FColor(200, 198, 190));
				const float PaleStrong = static_cast<float>(0.6 * (1.0 - FMath::Clamp(Age / 2.5, 0.0, 1.0)) * FMath::SmoothStep(0.0, 0.06, Age)) * NightThin;
				const double PaleOut = 0.55 * SmokeLong;
				Draw(Heartland, PaleOut, PaleGrey, PaleStrong);
				if (!bTop)
				{
					Draw(Heartland + Downwind * (0.45 * PaleOut) + FVector(0.0, 0.0, (bTurned ? 0.2 : -0.12) * PaleOut), 0.68 * PaleOut, PaleGrey, 0.75f * PaleStrong);
				}
			}
			else
			{
				for (int32 Row = 0; Row < 3; ++Row)
				{
					const double Square = 0.5 * SmokeLong * SheetSquare[Row] / SheetSmoke[Row];
					DrawShape(Heartland, Downwind * Square, AcrossWind * Square, FirstShape, Row, Aged[Row], Thinned * AgeShare[Row], bTurned);
					if (bTwo && !bTop)
					{
						DrawShape(Heartland + Downwind * (SecondAlong * SmokeLong) + AcrossWind * (SecondUp * SmokeLong), Downwind * (Square * SecondSize), AcrossWind * (Square * SecondSize),
							SecondShape, Row, Aged[Row], 0.85f * Thinned * AgeShare[Row], !bTurned);
					}
				}
			}
			const double Long = SmokeLong, Wide = SmokeLong * 0.6; // (for the flash, which is placed by the smoke's size)
			// The flash: one frame or two and gone, lumpy, a deep orange-red; off the middle of the smoke, with no sparks.
			// (The modeller's flak rule, with the art director's limit of 18 November 2026: "deep orange-red, never
			// peach". It was a fifth of a second of two warm smears with a cream heart. It is three lumps of red and
			// orange, a little off one another, the smallest the hottest and still orange; by dusk and night, when
			// the eye is wide open, it is let down so that it does not bleach.)
			// (release 2, the flak brief: by night with the ring on the flash is held a fifth of a second, at full for the
			// first tenth; light flak has no flash by day)
			if ((bFlashHeld ? Age < 0.6 : bNightBurst ? Age < 0.2 : (Age < 0.25 && Burst.Flashed < 2)) && Burst.Born >= 0.0 && !bTop && (!Burst.bLight || bNightBurst))
			{
				++Burst.Flashed;
				// (The art director, 21:27Z on 18 November 2026: "one frame at full, one at half". Held for a still, it is
				// at full for three tenths of a second and at half for three.)
				const float Lit0 = FMath::Lerp(1.0f, 0.6f, Moonlit) * ((bFlashHeld ? Age < 0.3 : bNightBurst ? Age < 0.1 : Burst.Flashed < 2) ? 1.0f : 0.5f);
				const FVector FlashAt = Heartland - Downwind * (0.12 * Long) + FVector(0.0, 0.0, 0.05 * Wide);
				const double Skew = Shape.FRandRange(0.0f, 6.283f);
				const FVector Lean = Right * FMath::Cos(Skew) + Up * FMath::Sin(Skew);
				const FVector Athwart = FVector::CrossProduct(Lean, FVector::CrossProduct(Right, Up));
				bDrawingLight = true;
				// (Measured in the first picture of it, through its own smoke: 212, 119, 77 at its brightest, a dull
				// orange on the way to peach. Laid deeper and redder it measured 163, 67, 47, and the art director
				// ruled it too red, a red stain. Her colours, as they are to be seen: a yellow-orange heart of about
				// 225, 150, 60, an orange body of about 205, 110, 45, and red-brown only at the torn edge, about
				// 180, 75, 35. Each is laid a little cleaner than it is to be seen, for the smoke that veils it.)
				// (Measured again on build 1690, held and with the smoke thin: the body 190 to 209, 94 to 117, 38 to 58 and
				// the edge 171, 77, 48, both as asked. The heart seemed to have lost its blue (5 to 10), but that was
				// the fighters' yellow tracer crossing the flash in the stills. She set it at 21:27Z: the heart about
				// 225, 150, 60 "with blue at least 45, about a quarter of the flash's width", then the body, the edge,
				// "then grey-brown smoke, with no red rim". The same flash was then photographed with its heart laid
				// two ways: laid 232, 158, 70 it is seen as 214, 148, 53, a yellow-orange; laid 232, 160, 120 it is
				// seen as 220, 153, 92, a peach, which she ruled out. So it is laid a little above her figures and is
				// a quarter of the width; the edge is a little smaller and ends more sharply, so that no thin red
				// ring stands round the fire. "-TheaterFlakHeart=ECA04A" lays the heart in another colour, for a picture.)
				// (Measured on build 1702 with nothing crossing it: the heart 231, 164, 42 and the body 210, 90, 28, each a
				// little short of her blue and the body short of her green. Both are laid up to meet her figures.)
				static FColor HeartLaid(236, 160, 88);
				static bool bHeartAsked = false;
				if (!bHeartAsked)
				{
					bHeartAsked = true;
					FString Hex;
					if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFlakHeart="), Hex, false) && Hex.Len() == 6)
					{
						HeartLaid = FColor::FromHex(Hex);
						UE_LOG(LogTheater, Display, TEXT("Flak: the flash's heart is laid as %d, %d, %d (-TheaterFlakHeart)"), HeartLaid.R, HeartLaid.G, HeartLaid.B);
					}
				}
				const FLinearColor FlashEdge(FColor(185, 62, 22)), FlashBody(FColor(210, 122, 50)), FlashHeart(HeartLaid);
				DrawSmear(FlashAt + Lean * (0.04 * W), Lean, 0.15 * W, 0.105 * W, FlashEdge, 0.9f * Lit0, 0.75);
				DrawSmear(FlashAt + Lean * (0.02 * W), Lean, 0.115 * W, 0.08 * W, FlashBody, 0.92f * Lit0, 0.65);
				DrawSmear(FlashAt - Lean * (0.07 * W) + Athwart * (0.04 * W), Athwart, 0.07 * W, 0.055 * W, FlashBody, 0.9f * Lit0, 0.6);
				Draw(FlashAt - Athwart * (0.02 * W), 0.04 * W, FlashHeart, 0.95f * Lit0);
				bDrawingLight = false;
			}
		}
	}

	// The painted shapes not used this time are folded away to nothing, and the flak's mesh is told.
	if (bFlakSmokeMade && FlakSmokeMesh)
	{
		for (int32 Spare = FlakPicture; Spare * 4 < FlakColours.Num(); ++Spare)
		{
			const int32 First = Spare * 4;
			if (FlakColours[First].A == 0)
			{
				continue;
			}
			for (int32 K = 0; K < 4; ++K)
			{
				FlakColours[First + K] = FColor(0, 0, 0, 0);
			}
		}
		FlakSmokeMesh->UpdateMeshSection(0, FlakVertices, TArray<FVector>(), FlakPlaces, FlakColours, TArray<FProcMeshTangent>());
		if (FlakLitInk && FlakLitColours.Num() > 0)
		{
			// (the lit shapes not used this time are folded away as well)
			for (int32 Spare = FlakLitShapes; Spare * 4 < FlakLitColours.Num(); ++Spare)
			{
				for (int32 K = 0; K < 4; ++K)
				{
					FlakLitColours[Spare * 4 + K] = FColor(0, 0, 0, 0);
				}
			}
			FlakSmokeMesh->UpdateMeshSection(1, FlakLitVertices, TArray<FVector>(), FlakLitPlaces, FlakLitColours, TArray<FProcMeshTangent>());
		}
	}

	// The puffs not used this time are folded away to nothing.
	for (int32 Spare = Puff; Spare < MaxPuffs; ++Spare)
	{
		const int32 First = Spare * PerPuff;
		if (SmokeColours[First].A == 0 && SmokeVertices[First + 1].Equals(SmokeVertices[First]))
		{
			continue;
		}
		for (int32 K = 0; K < PerPuff; ++K)
		{
			SmokeVertices[First + K] = SmokeVertices[First];
			SmokeColours[First + K] = FColor(0, 0, 0, 0);
		}
	}

	// (Speed, 17 November 2026; "tc.Speed.SmokeTrim".) Every point of all 2,400 puffs was sent to the graphics chip
	// every frame, some forty thousand, when a few hundred puffs are in the air from playing height. Only the puffs in
	// use are sent now, with room kept in steps of 300 (let down only after two seconds with fewer, so that it is not
	// made again and again), and two points more that are no puff's: the corners of all the puffs, folded ones too,
	// so that the mesh's bounds are what they always were and it is sorted among the see-through things as before.
	// The puffs are the same puffs in the same order, so the picture is the same. At 0, all of them, as before.
	int32 Wanted = MaxPuffs;
	if (TheaterSpeed::SmokeTrim())
	{
		const int32 Step = 300;
		Wanted = FMath::Clamp(FMath::DivideAndRoundUp(FMath::Max(Puff, 1), Step) * Step, Step, MaxPuffs);
		if (Wanted < SmokeRoom && ++SmokeRoomSpare < 120)
		{
			Wanted = SmokeRoom;
		}
		else if (Wanted >= SmokeRoom)
		{
			SmokeRoomSpare = 0;
		}
	}
	if (Wanted == MaxPuffs)
	{
		if (SmokeRoom != MaxPuffs)
		{
			MakeSmokeSection(MaxPuffs, SmokeVertices, SmokeColours);
		}
		else
		{
			SmokeMesh->UpdateMeshSection(0, SmokeVertices, TArray<FVector>(), TArray<FVector2D>(), SmokeColours, TArray<FProcMeshTangent>());
		}
		return;
	}
	const int32 Sent = Wanted * PerPuff;
	SmokeSentVertices.SetNumUninitialized(Sent + 2);
	SmokeSentColours.SetNumUninitialized(Sent + 2);
	FMemory::Memcpy(SmokeSentVertices.GetData(), SmokeVertices.GetData(), Sent * sizeof(FVector));
	FMemory::Memcpy(SmokeSentColours.GetData(), SmokeColours.GetData(), Sent * sizeof(FColor));
	FBox Whole(ForceInit);
	for (const FVector& Point : SmokeVertices)
	{
		Whole += Point;
	}
	SmokeSentVertices[Sent] = Whole.Min;
	SmokeSentVertices[Sent + 1] = Whole.Max;
	SmokeSentColours[Sent] = FColor(0, 0, 0, 0);
	SmokeSentColours[Sent + 1] = FColor(0, 0, 0, 0);
	if (Wanted != SmokeRoom)
	{
		MakeSmokeSection(Wanted, SmokeSentVertices, SmokeSentColours);
	}
	else
	{
		SmokeMesh->UpdateMeshSection(0, SmokeSentVertices, TArray<FVector>(), TArray<FVector2D>(), SmokeSentColours, TArray<FProcMeshTangent>());
	}
}

void ACampaignMapActor::UpdateForCamera(float CameraDistance, float CameraYaw, const FVector& CameraFocus)
{
	// (release 2: held under a land battle, the view kept for its release; HoldUpdates)
	LastAskedDistance = CameraDistance;
	LastAskedYaw = CameraYaw;
	LastAskedFocus = CameraFocus;
	bViewAsked = true;
	// (Release 2, the far side: seam-2. What the screen draws at a place on the table is counted in the turn of the world
	// nearest the eye; on a table with edges, as it is given.)
	TheaterSpeed::RoundWorld(CameraFocus.Y, OverSeam() ? Data.WorldWidth * UnitScale : 0.0);
	if (bUpdatesHeld)
	{
		return;
	}
	THEATER_SPEED_SCOPE(MapUpdate);
	// (lines put off by a press that changed whose ground is whose: see SetHolders)
	// (Release 2: while an order's result card holds its ink at the start, the front line waits with it, and is drawn in
	// its new place as the card is put away and the ink sets off, as it is in the frame after any other press: so that
	// under the card the line and the paint tell the same story. The code checker's note on bca710c9.)
	if (FrontLineDue != 0 && GFrameCounter >= FrontLineDue && !bInkHeldByCard)
	{
		FrontLineDue = 0;
		BuildFrontLine();
	}
	if (BordersDue != 0 && GFrameCounter >= BordersDue)
	{
		BordersDue = 0;
		BuildBorders();
	}
	if (bVictorsMoving)
	{
		DrawVictorFlags(); // (a flag on its way up or down a captured capital's pole)
	}
	// The lamp over the army in hand: a warm pool of light about a province wide on the table round the piece,
	// from a shaded lamp hung over it. It follows the piece as it is carried or marches, comes up and goes down
	// over a third of a second, and is a little stronger by night, when there is less light to compete with.
	// ("-TheaterLamp=day,night,height,cone" tries other strengths, a height in map units and a cone in degrees.)
	{
		AArmyPiece* Held = bPiecesShown ? HandLampPiece.Get() : nullptr;
		const float Step = GetWorld() ? GetWorld()->GetDeltaSeconds() * 3.0f : 0.05f;
		HandLampGlow = FMath::Clamp(HandLampGlow + (Held ? Step : -Step), 0.0f, 1.0f);
		if (HandLampGlow > 0.0f)
		{
			static float Day = 1200.0f, Night = 1400.0f, High = 90.0f, Cone = 15.0f; // (candelas; the lamp hangs high, so that its pool is even and has no hot middle)
			// (The art director's ruling of round 19: by day a fleet in hand stood on a pale disc at 0.85, its pale decks
			// lost in it and the sea's name hidden under it. That disc was this lamp's pool on the water, which answers
			// a lamp far more than the land does. Over the sea by day it burns at a fraction; by night it is as it was.)
			// (By night the pool stood at a warm 0.46 under a film that is gone now, and the ships stood out on it: the
			// lamp is turned down over the sea by night too, to keep it there.)
			static float LampSeaDay = 0.1f, LampSeaNight = 0.4f;
			static bool bRead = false;
			if (!bRead)
			{
				bRead = true;
				FString Asked;
				if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLamp="), Asked, false))
				{
					TArray<FString> Parts;
					Asked.ParseIntoArray(Parts, TEXT(","));
					if (Parts.IsValidIndex(0)) { Day = FCString::Atof(*Parts[0]); }
					if (Parts.IsValidIndex(1)) { Night = FCString::Atof(*Parts[1]); }
					if (Parts.IsValidIndex(2)) { High = FCString::Atof(*Parts[2]); }
					if (Parts.IsValidIndex(3)) { Cone = FCString::Atof(*Parts[3]); }
					if (Parts.IsValidIndex(4)) { LampSeaDay = FCString::Atof(*Parts[4]); }
					if (Parts.IsValidIndex(5)) { LampSeaNight = FCString::Atof(*Parts[5]); }
				}
			}
			if (!HandLamp)
			{
				HandLamp = NewObject<USpotLightComponent>(this);
				HandLamp->SetMobility(EComponentMobility::Movable);
				HandLamp->SetupAttachment(Root);
				HandLamp->SetCastShadows(false);
				HandLamp->IntensityUnits = ELightUnits::Candelas;
				HandLamp->bUseInverseSquaredFalloff = true;
				HandLamp->SetLightColor(FLinearColor(FColor(255, 190, 120)));
				HandLamp->SetSpecularScale(0.15f); // (it lights the table: it is not mirrored in the sea)
				HandLamp->SetSourceRadius(4.0f);
				HandLamp->RegisterComponent();
				HandLamp->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
			}
			if (Held)
			{
				HandLampAt = Held->GetActorLocation();
				bHandLampAtSea = Held->IsFleet();
			}
			HandLamp->SetInnerConeAngle(Cone * 0.3f);
			HandLamp->SetOuterConeAngle(Cone);
			HandLamp->SetAttenuationRadius(High * UnitScale * 1.8f);
			HandLamp->SetWorldLocation(HandLampAt + FVector(0.0, 0.0, High * UnitScale));
			HandLamp->SetIntensity(HandLampGlow * HandLampGlow * FMath::Lerp(bHandLampAtSea ? Day * LampSeaDay : Day, bHandLampAtSea ? Night * LampSeaNight : Night, FMath::Clamp(NightNow, 0.0f, 1.0f)));
			HandLamp->SetVisibility(true);
		}
		else if (HandLamp && HandLamp->IsVisible())
		{
			HandLamp->SetVisibility(false);
		}
	}
	// The front line is drawn heavier from the staff map's height, where the working line would be a hair.
	FrontLineSeenFrom = CameraDistance;
	if (FrontLineMesh && FrontLineFar >= 0)
	{
		const int32 Far = CameraDistance > (FrontLineFar == 1 ? 8200.0f : 9000.0f) ? 1 : 0;
		// (speed, tc.Speed.FrontWeights: the weight left over is laid a frame after the other, or now if it is wanted now)
		if (FrontLineOwed != INDEX_NONE && (Far == FrontLineOwed || GFrameCounter > FrontLineOwedFrame))
		{
			THEATER_SPEED_SCOPE(FrontLineOwed);
			const int32 Owed = FrontLineOwed;
			FrontLineOwed = INDEX_NONE;
			LayFrontLine(Owed, true);
		}
		if (Far != FrontLineFar)
		{
			FrontLineFar = Far;
			FrontLineMesh->SetMeshSectionVisible(0, Far == 0);
			FrontLineMesh->SetMeshSectionVisible(1, Far == 1);
			FrontLineMesh->SetMeshSectionVisible(2, Far == 0); // (release 2, the front you can read: the pen's shadow at each weight)
			FrontLineMesh->SetMeshSectionVisible(3, Far == 1);
			if (EnemyReachMesh)
			{
				EnemyReachMesh->SetMeshSectionVisible(0, Far == 0);
				EnemyReachMesh->SetMeshSectionVisible(1, Far == 1);
			}
		}
	}
	ShowParkedPlanes(CameraDistance, CameraFocus);
	UpdateSinkings(); // (before the smoke, which reads how far each has gone)
	UpdateSmoke(CameraDistance, CameraFocus);
	UpdateSeaMotion(CameraDistance, CameraFocus);
	UpdateSky(CameraDistance, CameraFocus);
	UpdateFogFade(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f);
	UpdateFogAside(); // (speed: the haze beyond the map, laid on another thread)
	FMiniatureMesh::PumpWarm(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f); // (speed: model files read on another thread, put in one a frame)
	SortieSeenFrom = CameraDistance;
	UpdateSorties();
	UpdateSearchlights(CameraDistance);
	UpdateAgents(CameraDistance, CameraYaw); // (before the signs: from high up each agent is one of them)
	UpdateCutLamps(CameraDistance);
	UpdateRailLight(CameraDistance);
	UpdateTrains(CameraFocus, CameraDistance);
	UpdateColumns(CameraFocus, CameraDistance); // (release 2: supply columns' convoys on the roads, CampaignColumns.cpp)
	UpdatePortLife(CameraFocus, CameraDistance); // (release 2: the little ships at the ports, CampaignPortLife.cpp; tc.Map.PortLife)
	UpdateStreetLife(CameraFocus, CameraDistance); // (release 2: the street cars on the cities' avenues, CampaignStreetLife.cpp; tc.Map.StreetLife)
	UpdateClouds();
	UpdateFlashes();
	UpdateSidesFade(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f); // (release 2: the Y key's sides view fades, tc.Speed.SidesFade)
	PlantTreesDue(CameraFocus); // (release 2, map run 8: the woods' month turn, a few squares a frame, tc.Speed.TreesSpread)
	UpdateOwnershipPaint();
	UpdateSeasonTurn(); // (release 2: the season turns, CampaignSeasonTurn.cpp; tc.Map.SeasonTurn, nothing while it is off)
	UpdateFrontChurn(CameraFocus, CameraDistance); // (release 2: churned ground between dug-in lines, CampaignFrontChurn.cpp; tc.Map.FrontChurn, nothing while it is off)
	// The showpiece turns slowly on its stand.
	if (ShowMesh && !ShowModel.IsEmpty() && GetWorld())
	{
		ShowTurn = FMath::Fmod(ShowTurn + 28.0 * GetWorld()->GetDeltaSeconds(), 360.0);
		ShowMesh->SetRelativeRotation(FRotator(0.0, ShowTurn, 0.0));
		// Its picture is taken afresh about twenty times a second: smooth enough to the eye, and a third of the work.
		static double LastTaken = 0.0;
		const double Clock = GetWorld()->GetTimeSeconds();
		if (ShowCamera && ShowTarget && Clock - LastTaken > 0.05)
		{
			LastTaken = Clock;
			TakeStudioPicture(ShowCamera, bShowAircraft); // (in the studio's own light, like every other picture of a unit)
		}
	}

	// The land's tiles: fine near what the eye looks at, coarse far from it.
	// (Speed: tc.Speed.TileTime. Whether tiles of these two are still to come is kept for the photographer and the
	// step player, which waited for the fine sheets by AreFineSheetsComing but for these only by the clock: with the
	// frame's time for tiles limited, fewer could be up at the shot than before. The pre-checker's note on 0108ac6.)
	const bool bHomeComing = UpdateLand(HomeLand, CameraDistance, CameraFocus, false);
	const bool bWorldComing = UpdateLand(WorldLand, CameraDistance, CameraFocus, false);
	bLandTilesComing = bHomeComing || bWorldComing;
	UpdateFineSheets(CameraDistance, CameraFocus);
	// (Release 2, the far side: seam-2. What is laid once is seen from both sides of the chart's seam: the squares of
	// woods and towns are shown on the eye's side, and the great models have their twins.)
	if (Data.bWorld || SeamLaid.Num() > 0)
	{
		SquaresRound(CameraFocus);
		KeepSeamTwins();
		KeepLightsTwin(CameraFocus); // (seam-4: and the lights of the night)
	}

	// Pieces cast shadows only near what the eye is looking at, and not at all from far off.
	{
		const double ShadowReach = CameraDistance * 1.1 + 500.0;
		const bool bAny = CameraDistance < 7000.0f;
		const bool bOverSeam = OverSeam(); // (the far side, seam-2: each piece is shown in the turn of the world nearest the eye)
		auto Shade = [&](AArmyPiece* Piece)
		{
			if (Piece)
			{
				if (Data.bFar)
				{
					Piece->LookFrom(CameraFocus.Y, bOverSeam);
				}
				const bool bByFocus = FVector::DistSquared2D(Piece->GetActorLocation(), CameraFocus) < ShadowReach * ShadowReach;
				Piece->SetShadows(bAny && bByFocus);
				Piece->SetEye(CameraDistance, bByFocus); // (release 2, a fleet under way, second part: near the eye a fleet's ships heel and ride the swell; tc.Map.ShipRide)
			}
		};
		for (AArmyPiece* Piece : Pieces)
		{
			Shade(Piece);
		}
		for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : FleetPieces)
		{
			Shade(Pair.Value);
		}
		for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : AirPieces)
		{
			Shade(Pair.Value);
		}
	}

	// Trees are drawn only close to: in the squares of country round what the eye is looking at.
	const bool bNear = CameraDistance < 7200.0f; // (from working height and above the woods are one canopy: art fix 7)
	const double Reach = CameraDistance * 1.25 + TreeSquare * 0.75;
	// (Speed, 18 November 2026: tc.Speed.ShowSpread. A square that is shown is handed whole to the draw thread, every
	// point of it, in that frame. When the eye came down through this height every square in reach was shown in the
	// one frame: 39 squares and 368 thousand points, a frame of 47 ms, in the builder's capture 2 (build 1684). And
	// the towns below went all 105 at once, a million points and 50 to 64 ms, each time the eye crossed their height.
	// Now the squares newly shown are counted by their points and shown so many a frame, the nearest to the eye's
	// point first and one always; the rest are still owed the frame after, and the photographer and the step player
	// wait for them (bSquaresComing). A square taken away costs nothing and goes at once, as before.)
	const int32 ShowSpread = TheaterSpeed::ShowSpread();
	auto PointsOf = [](UProceduralMeshComponent* Mesh)
	{
		int32 Points = 0;
		for (int32 Section = 0; Mesh && Section < Mesh->GetNumSections(); ++Section)
		{
			const FProcMeshSection* Its = Mesh->GetProcMeshSection(Section);
			Points += Its ? Its->ProcVertexBuffer.Num() : 0;
		}
		return Points;
	};
	bSquaresComing = false;
	{
		const double TreesRound = OverSeam() ? Data.WorldWidth * UnitScale : 0.0;
		TArray<TPair<double, int32>> Owed; // (how far from the eye's point, squared, and which square)
		for (int32 Index = 0; Index < TreeMeshes.Num(); ++Index)
		{
			// (the far side, seam-2: a square of woods is shown in the turn of the world nearest the eye, and is so far from it)
			const double AwayEast = TreeCentres[Index].Y - CameraFocus.Y;
			const double Away = FMath::Square(TreeCentres[Index].X - CameraFocus.X)
				+ FMath::Square(TreesRound > 0.0 ? AwayEast - FMath::RoundToDouble(AwayEast / TreesRound) * TreesRound : AwayEast);
			const bool bShow = bNear && Away < Reach * Reach;
			if (bShow != TreesShown[Index])
			{
				if (bShow && ShowSpread > 0)
				{
					Owed.Emplace(Away, Index);
					continue;
				}
				TreesShown[Index] = bShow;
				if (TreeMeshes[Index])
				{
					TreeMeshes[Index]->SetVisibility(bShow);
				}
			}
		}
		Owed.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
		int32 Shown = 0;
		for (const TPair<double, int32>& Next : Owed)
		{
			const int32 Its = PointsOf(TreeMeshes[Next.Value]);
			if (Shown > 0 && Shown + Its > ShowSpread)
			{
				bSquaresComing = true;
				break;
			}
			Shown += Its;
			TreesShown[Next.Value] = true;
			if (TreeMeshes[Next.Value])
			{
				TreeMeshes[Next.Value]->SetVisibility(true);
			}
		}
	}

	// The woods are one dense canopy from working height and above (the nation's stain lies over it at half
	// strength, so a wood is still seen to be its owner's); close to, where single trees stand on it, the cloth
	// under them is laid a little thinner.
	// (By moonlight the cloth is laid thinner: a wood on the player's own ground was as dark as the fog of war beside
	// it, and read as fog. See FMoonLook::Woods.)
	const float WoodsMoon = bLamplightNow ? 0.0f : ACampaignEnvironment::MoonOf(NightNow);
	const float Woods = FMath::Lerp(0.8f, 0.86f, FMath::SmoothStep(3200.0f, 6500.0f, CameraDistance)) // (close to it is the dark floor of the forest)
		* FMath::Lerp(1.0f, ACampaignEnvironment::MoonLook().Woods, WoodsMoon);
	if (LandMaterial && FMath::Abs(Woods - WoodsShown) > 0.01f)
	{
		WoodsShown = Woods;
		LandScalar(TEXT("WoodsStrength"), Woods);
		// (the tree-tops' own bumps are for close to: from working height they are let down, so that the lie of
		// the land still shades the canopy and a wood does not sit on the hills like a sticker)
		LandScalar(TEXT("TreeBump"), FMath::Lerp(0.55f, 0.16f, FMath::SmoothStep(3200.0f, 6500.0f, CameraDistance)));
	}
	// The quilt of fields: plain to see close to, quieter at working height (where it must not fight the province
	// lines), and fading out toward the top of the zoom. Its hedges are seen only close to.
	// (by moonlight the quilt is halved: the blocks showed under the moon)
	const float Fields = (CameraDistance < 9000.0f ? FMath::GetMappedRangeValueClamped(FVector2f(3000.0f, 9000.0f), FVector2f(FieldsNear, FieldsWorking), CameraDistance)
		: FMath::GetMappedRangeValueClamped(FVector2f(9000.0f, 19000.0f), FVector2f(FieldsWorking, FieldsWorking * 0.45f), CameraDistance)) * (1.0f - 0.5f * GetMoonlight());
	const float Hedges = FMath::GetMappedRangeValueClamped(FVector2f(2600.0f, 7000.0f), FVector2f(HedgesNear, 0.0f), CameraDistance);
	// (On the world's coarse sheet, which is what is seen from high up beyond Europe, the quilt all but goes as the
	// eye climbs: its squares and strips were ruled lines from there, and where one manner of field met another the
	// ground stepped along a ruled line.)
	// (The art director's ruling of round 13: fading the quilt itself cost the plains a sixth of their colour. So
	// the quilt keeps its strength and its fields are drawn toward the mean of their own kind instead: the pattern
	// goes, the gold stays.)
	const float FarFlat = 0.85f * FMath::SmoothStep(11000.0f, 18000.0f, CameraDistance);
	const float FarFields = Fields;
	const bool bFieldsMoved = LandMaterial && FMath::Abs(Fields - FieldsShown) > 0.005f;
	if (bFieldsMoved)
	{
		FieldsShown = Fields;
		LandScalar(TEXT("FieldsStrength"), Fields);
	}
	if (WorldLandMaterial && (bFieldsMoved || FMath::Abs(FarFields - FarFieldsShown) > 0.003f))
	{
		FarFieldsShown = FarFields;
		WorldLandMaterial->SetScalarParameterValue(TEXT("FieldsStrength"), FarFields);
	}
	if (WorldLandMaterial && FMath::Abs(FarFlat - FarFlatShown) > 0.003f)
	{
		FarFlatShown = FarFlat;
		WorldLandMaterial->SetScalarParameterValue(TEXT("FieldsFlat"), FarFlat);
	}
	if (LandMaterial && FMath::Abs(Hedges - HedgesShown) > 0.005f)
	{
		HedgesShown = Hedges;
		LandScalar(TEXT("HedgeStrength"), Hedges);
	}

	// The quiet lines. Province lines are faint from high up and a little firmer on the way down; every one is
	// at full strength while the "show provinces" key is held. Railways are hidden from high up, a fine line at
	// working height, and have their sleepers only close to.
	{
		const float Province = bShowProvinces ? 1.0f
			: CameraDistance > 9000.0f ? FMath::GetMappedRangeValueClamped(FVector2f(9000.0f, 13000.0f), FVector2f(0.2f, 0.14f), CameraDistance)
			: FMath::GetMappedRangeValueClamped(FVector2f(3000.0f, 9000.0f), FVector2f(0.32f, 0.2f), CameraDistance);
		const float Rail = CameraDistance > 9000.0f ? FMath::GetMappedRangeValueClamped(FVector2f(9000.0f, 12500.0f), FVector2f(0.4f, 0.0f), CameraDistance)
			: FMath::GetMappedRangeValueClamped(FVector2f(3000.0f, 9000.0f), FVector2f(0.7f, 0.4f), CameraDistance);
		const float Sleeper = FMath::GetMappedRangeValueClamped(FVector2f(3200.0f, 5200.0f), FVector2f(0.7f, 0.0f), CameraDistance);
		// (by moonlight every province line is capped: close up the pale ones were a white net over the dark country)
		const float NightCap = bShowProvinces ? 1.0f : FMath::Lerp(1.0f, 0.15f, GetMoonlight());
		if (ProvinceInk && FMath::Abs(FMath::Min(Province, NightCap) - ProvinceShown) > 0.004f)
		{
			ProvinceShown = FMath::Min(Province, NightCap);
			ProvinceInk->SetScalarParameterValue(TEXT("Opacity"), ProvinceShown);
		}
		// (cream lines on dark ground are a touch fainter close to, where the brown ones grow firmer: close up
		// cream turned into a white net over the country)
		const float Pale = bShowProvinces ? 0.9f : FMath::Min3(Province, NightCap, FMath::GetMappedRangeValueClamped(FVector2f(3000.0f, 9000.0f), FVector2f(0.18f, 0.2f), CameraDistance));
		if (ProvincePaleInk && FMath::Abs(Pale - ProvincePaleShown) > 0.004f)
		{
			ProvincePaleShown = Pale;
			ProvincePaleInk->SetScalarParameterValue(TEXT("Opacity"), Pale);
		}
		if (RailInk && FMath::Abs(Rail - RailShown) > 0.004f)
		{
			RailShown = Rail;
			RailInk->SetScalarParameterValue(TEXT("Opacity"), Rail);
		}
		if (SleeperInk && FMath::Abs(Sleeper - SleeperShown) > 0.004f)
		{
			SleeperShown = Sleeper;
			SleeperInk->SetScalarParameterValue(TEXT("Opacity"), Sleeper);
		}
	}

	// (The gun lines go from the height where the bases' own pieces can no longer be told apart. A line goes with its
	// army when that is picked up or sets out, and is put down with it.)
	if (GunLineMesh)
	{
		const bool bGunsSeen = bPiecesShown && CameraDistance < 12500.0f;
		if (GunLineMesh->IsVisible() != bGunsSeen)
		{
			GunLineMesh->SetVisibility(bGunsSeen);
		}
		if (bGunsSeen)
		{
			const double Slack = FMath::Square(0.5 * UnitScale);
			bool bMoved = false;
			for (const FGunLine& Line : GunLines)
			{
				const AArmyPiece* Piece = Line.Piece.Get();
				bMoved |= !Piece || Piece->IsMarching() || FVector::DistSquared2D(Piece->GetReckonedLocation(), Piece->GetRestLocation()) > Slack;
			}
			for (const TWeakObjectPtr<AArmyPiece>& Waited : GunsAway)
			{
				const AArmyPiece* Piece = Waited.Get();
				bMoved |= !Piece || (!Piece->IsMarching() && FVector::DistSquared2D(Piece->GetReckonedLocation(), Piece->GetRestLocation()) <= Slack);
			}
			if (bMoved)
			{
				BuildGunLines();
			}
		}
	}

	// The flock on the pieces' bases is seen only close to: from higher it swallowed the figures and the holder's
	// colour, so there the base is plain trodden earth inside its ring.
	{
		const bool bTurfNear = CameraDistance < (AArmyPiece::bFlockNear ? 4600.0f : 4200.0f);
		if (bTurfNear != AArmyPiece::bFlockNear)
		{
			AArmyPiece::bFlockNear = bTurfNear;
			for (AArmyPiece* Based : Pieces)
			{
				if (Based)
				{
					Based->SetFlockShown(bTurfNear);
				}
			}
		}
	}

	// (Release 2.) The camps round the bases are seen from the height the gun lines are, and are put away above it
	// with them: from higher their tents and trenches are specks, and the frame is better spent elsewhere.
	{
		const bool bCampsSeen = CameraDistance < (AArmyPiece::bCampsNear ? 12500.0f : 12000.0f);
		if (bCampsSeen != AArmyPiece::bCampsNear)
		{
			AArmyPiece::bCampsNear = bCampsSeen;
			for (AArmyPiece* Camped : Pieces)
			{
				if (Camped)
				{
					Camped->SetCampShown(bCampsSeen);
				}
			}
		}
	}

	// Beyond working height the towns are drawn in their cheaper form: one piece to a block, laid out just the same,
	// and throwing no shadows. (The windows' panes are too small to see from there, and are not drawn either.)
	{
		// (Speed: "tc.Speed.TownSquares" turned the other way since the towns were built: they are built again to match,
		// as when the cities leave room for a quarter. And "tc.Speed.TownShadows", which is for measuring only.)
		if (TownMesh && TownsCut != -1 && TownsCut != (TheaterSpeed::TownSquares() ? 1 : 0) && Data.IsLoaded())
		{
			BuildTowns();
			ShownNight = -1.0f;
			SetNight(NightNow, bLamplightNow);
		}
		// (Release 2, grander great cities: "tc.Map.GrandCities" turned since the towns were built: they are built again
		// to match, in the same way.)
		if (TownMesh && TownsGrand != -1 && TownsGrand != (GrandCitiesWanted() ? 1 : 0) + (GrandCitiesFarWanted() ? 2 : 0) && Data.IsLoaded())
		{
			BuildTowns();
			ShownNight = -1.0f;
			SetNight(NightNow, bLamplightNow);
		}
		if ((TownShadowsCast == 0) == TheaterSpeed::TownShadows())
		{
			SetTownShadows(TheaterSpeed::TownShadows());
		}
		const int32 FarNow = CameraDistance > (TownsFarShown == 1 ? 10800.0f : 11600.0f) ? 1 : 0;
		if (FarNow != TownsFarShown && TownMesh && TownFarMesh)
		{
			// (Speed: tc.Speed.ShowSpread, as for the trees above. The first time, and after the towns are built again,
			// every square is put in its form at once, as before: that is a load's frame. At a change of height only
			// what is not a square changes here, the windows' panes among it, and the squares follow below.)
			const bool bSpread = ShowSpread > 0 && TownsFarShown != -1 && TownSquareForm.Num() > 0 && TownSquareForm.Num() == FMath::Max(TownSquares.Num(), TownFarSquares.Num());
			TownsFarShown = FarNow;
			if (bSpread)
			{
				TownMesh->SetVisibility(FarNow != 1);
				TownFarMesh->SetVisibility(FarNow == 1);
				if (LightsMesh && LightsMesh->GetNumSections() > 0)
				{
					LightsMesh->SetMeshSectionVisible(0, FarNow != 1);
				}
			}
			else
			{
				ShowTownForm(FarNow == 1);
			}
		}
		// (Speed: tc.Speed.ShowSpread.) The squares of towns not yet in the form the eye's height asks for change to
		// it here, the nearest to the eye's point first: a square's two forms change places in the one frame, so no
		// town is ever missing or drawn twice. So many points of the form newly shown a frame, and one square always.
		if (TownsFarShown != -1 && TownSquareForm.Num() == FMath::Max(TownSquares.Num(), TownFarSquares.Num()))
		{
			const int8 Wanted = static_cast<int8>(TownsFarShown == 1 ? 1 : 0);
			TArray<TPair<double, int32>> Owed;
			for (int32 Index = 0; Index < TownSquareForm.Num(); ++Index)
			{
				if (TownSquareForm[Index] != Wanted)
				{
					const UProceduralMeshComponent* Either = TownSquares.IsValidIndex(Index) && TownSquares[Index] ? TownSquares[Index].Get() : TownFarSquares.IsValidIndex(Index) ? TownFarSquares[Index].Get() : nullptr;
					Owed.Emplace(Either ? FVector::DistSquared2D(Either->Bounds.Origin, CameraFocus) : 0.0, Index);
				}
			}
			Owed.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
			int32 Shown = 0;
			for (const TPair<double, int32>& Next : Owed)
			{
				UProceduralMeshComponent* Near = TownSquares.IsValidIndex(Next.Value) ? TownSquares[Next.Value].Get() : nullptr;
				UProceduralMeshComponent* Far = TownFarSquares.IsValidIndex(Next.Value) ? TownFarSquares[Next.Value].Get() : nullptr;
				const int32 Its = PointsOf(Wanted == 1 ? Far : Near);
				if (ShowSpread > 0 && Shown > 0 && Shown + Its > ShowSpread)
				{
					bSquaresComing = true;
					break;
				}
				Shown += Its;
				TownSquareForm[Next.Value] = Wanted;
				if (Near)
				{
					Near->SetVisibility(Wanted != 1);
				}
				if (Far)
				{
					Far->SetVisibility(Wanted == 1);
				}
			}
		}
	}

	// The staff map (art fix 11): from the top of the zoom the living table gives way to the staff's own paper map,
	// little by little as the eye rises through the last of its climb.
	{
		// ("-TheaterPaperFrom=17500" and "-TheaterPaperFull=25000" try other heights)
		static float From = -1.0f, Full = -1.0f;
		if (From < 0.0f)
		{
			From = 17500.0f;
			Full = 25000.0f;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterPaperFrom="), From);
			FParse::Value(FCommandLine::Get(), TEXT("TheaterPaperFull="), Full);
		}
		const float Sheet = FMath::SmoothStep(From, FMath::Max(Full, From + 1.0f), CameraDistance);
		if (Data.bWorld)
		{
			// The chart's ruled border, kept about a point and a half of the screen wide: from the staff map's height
			// a line as fine as the one drawn for the working view broke into dashes.
			const double Wanted = FMath::Max(1.1 * UnitScale, CameraDistance * 0.00105);
			if (FMath::Abs(Wanted - MarginLine) > 0.15 * MarginLine)
			{
				MarginLine = Wanted;
				BuildSheetMargin();
			}
			// The prairie's section roads and furrows: nothing from high up, where they would only shimmer.
			const float Work = 1.0f - FMath::SmoothStep(8000.0f, 13000.0f, CameraDistance);
			if (FMath::Abs(Work - SquareWorkShown) > 0.02f)
			{
				SquareWorkShown = Work;
				LandScalar(TEXT("SquareWork"), Work);
			}
			// (and as the eye climbs on, the smallest woods on the world's coarse sheet are let go)
			// (no further than this: a stronger fade began to eat the great masses. The corn belt's rash of small
			// woods is thinned in the painter instead.)
			const float Lift = 0.24f * FMath::SmoothStep(11000.0f, 19000.0f, CameraDistance);
			if (WorldLandMaterial && FMath::Abs(Lift - WoodsLiftShown) > 0.004f)
			{
				WoodsLiftShown = Lift;
				WorldLandMaterial->SetScalarParameterValue(TEXT("WoodsLift"), Lift);
			}
		}
		if (FMath::Abs(Sheet - PaperShown) > 0.004f)
		{
			PaperShown = Sheet;
			// The things left lying on the table are specks from this height: they are put away, and the sheet is
			// seen whole, with its margin of paper and the shadow it throws on the wood.
			if (RoomMesh)
			{
				RoomMesh->SetVisibility(Sheet < 0.2f);
			}
			if (MarginMesh)
			{
				// (Round a world the chart's paper margin and ruled border are there at every height: Narvik and the
				// Murmansk run lie on its north edge, and the sea was cut straight against the bare table.)
				const float Shown = Data.bWorld ? 1.0f : FMath::SmoothStep(0.0f, 0.5f, Sheet);
				MarginMesh->SetVisibility(Sheet > 0.01f || Data.bWorld);
				const float Moon = bLamplightNow ? 0.0f : NightNow;
				const FLinearColor Wood(FColor::FromHex(TEXT("2B1B11"))), Paper = FLinearColor(PaperLand[0], PaperLand[1], PaperLand[2]) * FMath::Lerp(1.0f, PaperNight[0], Moon);
				if (MarginPaper)
				{
					MarginPaper->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(Wood, Paper, Shown));
				}
				if (MarginShade)
				{
					MarginShade->SetScalarParameterValue(TEXT("Opacity"), Shown);
				}
			}
			if (LandMaterial)
			{
				LandScalar(TEXT("Paper"), Sheet);
			}
			if (SeaMaterial)
			{
				SeaScalar(TEXT("Paper"), Sheet);
			}
		}
	}

	// The nations' stain: a thicker wash from high up, where the eye wants to see whose a country is, and a
	// thinner one close to, where it wants to see the country itself.
	{
		static const float Strengths[3] = { 0.72f, 1.0f, 1.4f };
		// (The art director's starting values were 0.42, 0.34 and 0.2. Under the room's lamp those left the
		// inside of a country reading as nobody's: a red or pink stain over green ground mixes to plain khaki.
		// Brian's rule is that every tile is a shade of its holder's colour, so the wash is laid on thicker.)
		const float Far = FMath::GetMappedRangeValueClamped(FVector2f(9000.0f, 13000.0f), FVector2f(0.64f, 0.74f), CameraDistance);
		const float Near = FMath::GetMappedRangeValueClamped(FVector2f(3000.0f, 9000.0f), FVector2f(0.5f, 0.64f), CameraDistance);
		// ("-TheaterWash=1.3" and "-TheaterBand=0.7" try other strengths without a rebuild)
		static float Tried = -1.0f;
		if (Tried < 0.0f)
		{
			Tried = 1.0f;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterWash="), Tried);
			float Band = 0.0f;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterBand="), Band) && LandMaterial)
			{
				LandScalar(TEXT("Band"), Band);
			}
		}
		const float Wash = (CameraDistance > 9000.0f ? Far : Near) * Strengths[FMath::Clamp(ColourStrength, 0, 2)] * Tried;
		if (LandMaterial && FMath::Abs(Wash - WashShown) > 0.004f)
		{
			WashShown = Wash;
			LandScalar(TEXT("Wash"), Wash);
		}
	}

	// Lettering: nations from far off, every region's name close up, capitals and seas always.
	const int32 State = CameraDistance > 12500.0f ? 0 : (CameraDistance > 8500.0f ? 1 : 2);
	if (State != LabelState)
	{
		LabelState = State;
		for (int32 i = 0; i < Labels.Num(); ++i)
		{
			const bool bShow = LabelTiers[i] == TierAlways
				|| (LabelTiers[i] == TierFar && State == 0)
				|| (LabelTiers[i] == TierNear && State == 2)
				|| (LabelTiers[i] == TierMiddle && State >= 1);
			if (Labels[i])
			{
				Labels[i]->SetVisibility(bShow);
			}
		}
	}

	// The standards lead the eye from working height and above (art fix 8): the base keeps its size on the table at
	// every height, while the flag and its plate hold a size on the screen as the eye rises (about one and two
	// thirds of life at working height, where a flag is then 26 pixels wide on a screen of 1,600 and the plate's
	// figures 8 high), and lean back to face an eye that looks down on them. Close to they are as they were made.
	{
		const float Growth = FMath::Clamp(CameraDistance / 6000.0f, 1.0f, 3.0f);
		// (the eye looks down more steeply the higher it stands: about 40 degrees close to, 74 at the top)
		const float LooksDown = FMath::Lerp(40.0f, 74.0f, FMath::Pow(FMath::Clamp(FMath::LogX(40.0f, FMath::Max(CameraDistance, 750.0f) / 750.0f), 0.0f, 1.0f), 0.85f));
		const float Lean = FMath::Clamp((LooksDown - 30.0f) * 0.85f, 0.0f, 48.0f) * FMath::SmoothStep(3000.0f, 8500.0f, CameraDistance);
		if (FMath::Abs(Growth - StandardGrowth) > 0.004f || FMath::Abs(Lean - StandardLean) > 0.2f)
		{
			StandardGrowth = Growth;
			StandardLean = Lean;
			for (AArmyPiece* Piece : Pieces)
			{
				if (Piece)
				{
					Piece->SetStandard(Growth, Lean);
				}
			}
			for (const auto& Pair : FleetPieces)
			{
				if (Pair.Value)
				{
					Pair.Value->SetStandard(Growth, Lean);
				}
			}
			for (const auto& Pair : AirPieces)
			{
				if (Pair.Value)
				{
					Pair.Value->SetStandard(Growth, Lean);
				}
			}
		}
	}

	// Standards turn to face whoever is looking at the table, and the lettering turns to read the right way up.
	if (!bFacingSet || FMath::Abs(FRotator::NormalizeAxis(CameraYaw - LastPieceYaw)) > 0.5f)
	{
		bFacingSet = true;
		LastPieceYaw = CameraYaw;
		const FMatrix Flat(FVector(0.0, 0.0, 1.0), FVector(0.0, -1.0, 0.0), FVector(1.0, 0.0, 0.0), FVector::ZeroVector);
		const FQuat Turned = FQuat(FVector::UpVector, FMath::DegreesToRadians(CameraYaw)) * Flat.ToQuat();
		for (UTextRenderComponent* Label : Labels)
		{
			if (Label)
			{
				Label->SetRelativeRotation(Turned);
			}
		}
		for (AArmyPiece* Piece : Pieces)
		{
			if (Piece)
			{
				Piece->FaceEye(CameraYaw); // (armies with weight: one that leans keeps its lean; one that stands is turned as before)
			}
		}
		for (const auto& Pair : FleetPieces)
		{
			if (Pair.Value)
			{
				Pair.Value->FaceTable(CameraYaw); // (a fleet under sail keeps its heading, and turns to this as it comes in)
			}
		}
		for (const auto& Pair : AirPieces)
		{
			if (Pair.Value)
			{
				Pair.Value->SetActorRotation(FRotator(0.0f, CameraYaw, 0.0f));
			}
		}
	}
}

void ACampaignMapActor::SetSelectedRegion(int32 RegionId)
{
	if (RegionId != SelectedRegion)
	{
		SelectedRegion = RegionId;
		RebuildHighlight();
		if (CrowdLead.Num() > 0)
		{
			ApplyCrowds(); // (a crowded province that is picked stays fanned out)
			BuildGunLines();
		}
	}
}


// ---------------------------------------------------------------------------- the weather's sky (ruling 127)

void ACampaignMapActor::ShowSky(const TArray<FSkyZone>& Zones)
{
	SkyZones = Zones;
	bSkyKnown = Zones.Num() > 0;
	if (!bSkyKnown || !Data.IsLoaded())
	{
		return;
	}
	bSkyEverKnown = true;
	// Each province's sky; each zone's plate; and the stretches of border where one sky ends. A stretch belongs to the
	// worse of its two skies, and is laid once: the scallops bulge out of it into the better sky.
	SkyOfRegion.Reset();
	SkyPlates.Reset();
	SkyRuns.Reset();
	for (const FSkyZone& Zone : Zones)
	{
		FSkyPlate& Plate = SkyPlates.AddDefaulted_GetRef();
		Plate.Name = Zone.Name;
		Plate.Sky = FMath::Clamp(Zone.Sky, 0, 2);
		for (const int32 RegionId : Zone.Regions)
		{
			if (Data.Regions.IsValidIndex(RegionId))
			{
				if (Zone.Sky > 0)
				{
					SkyOfRegion.Add(RegionId, FMath::Min(Zone.Sky, 2));
				}
				const FVector2D Middle = Data.Regions[RegionId].Centroid;
				Plate.Grounds.Add(SurfacePoint(Middle.X, Middle.Y, 0.2));
			}
		}
	}
	for (const TPair<int32, int32>& Pair : SkyOfRegion)
	{
		const FCampaignRegion& Region = Data.Regions[Pair.Key];
		for (const TArray<FVector2D>& Ring : Region.Polygons)
		{
			const int32 Count = Ring.Num();
			if (Count < 3)
			{
				continue;
			}
			double Area = 0.0;
			for (int32 K = 0; K < Count; ++K)
			{
				Area += Ring[K].X * Ring[(K + 1) % Count].Y - Ring[(K + 1) % Count].X * Ring[K].Y;
			}
			const double Hand = Area > 0.0 ? 1.0 : -1.0; // (which side of each stretch the province lies on)
			// The sky beyond each stretch of the outline: that of the province there, or none where it is the sea
			// (a zone takes the water off its shores with it) or the edge of the map.
			TArray<int32> Beyond;
			for (int32 K = 0; K < Count; ++K)
			{
				const FVector2D A = Ring[K], B = Ring[(K + 1) % Count];
				const double Length = FVector2D::Distance(A, B);
				int32 Sky = -1;
				if (Length > 0.05)
				{
					const FVector2D Along = (B - A) / Length;
					const FVector2D Outside = 0.5 * (A + B) - FVector2D(-Along.Y, Along.X) * Hand * 0.4;
					const int32 Next = Data.FindRegionAt(Outside.X, Outside.Y);
					if (Next != INDEX_NONE && Next != Pair.Key)
					{
						const int32* Theirs = SkyOfRegion.Find(Next);
						Sky = Theirs ? *Theirs : 0;
					}
				}
				Beyond.Add(Sky >= 0 && Sky < Pair.Value ? Sky : -1);
			}
			// Begun where a run begins, so that one that passes the ring's first corner is not cut in two.
			int32 First = 0;
			for (int32 K = 0; K < Count; ++K)
			{
				if (Beyond[K] >= 0 && Beyond[(K + Count - 1) % Count] != Beyond[K])
				{
					First = K;
					break;
				}
			}
			for (int32 Step = 0; Step < Count; )
			{
				const int32 K = (First + Step) % Count;
				if (Beyond[K] < 0)
				{
					++Step;
					continue;
				}
				FSkyRun& Run = SkyRuns.AddDefaulted_GetRef();
				Run.Worse = Pair.Value;
				Run.Better = Beyond[K];
				Run.Side = -Hand;
				Run.Points.Add(Ring[K]);
				while (Step < Count && Beyond[(First + Step) % Count] == Run.Better)
				{
					Run.Points.Add(Ring[(First + Step + 1) % Count]);
					++Step;
				}
				for (const FVector2D& Point : Run.Points)
				{
					Run.Bounds += Point;
				}
			}
		}
	}
	SkyEdgeDrawnAt = -1.0f; // (the edge is drawn afresh)
	// The picture of where each sky lies: the whole table, a pixel to every two units of the map (four on the map of
	// the whole world). Its red is cloud and its green a closed sky.
	const bool bWhole = Data.bWorld && Data.WorldWidth > 0.0 && Data.WorldHeight > 0.0;
	const double X0 = bWhole ? Data.WorldMinX : 0.0, Y0 = bWhole ? Data.WorldMinY : 0.0;
	const double Wide = bWhole ? Data.WorldWidth : Data.Width, High = bWhole ? Data.WorldHeight : Data.Height;
	const double Texel = bWhole ? 4.0 : 2.0;
	const int32 NX = FMath::Clamp(FMath::CeilToInt32(Wide / Texel), 4, 4096), NY = FMath::Clamp(FMath::CeilToInt32(High / Texel), 4, 4096);
	TArray<uint8> Kind;
	Kind.Init(0, NX * NY);
	auto InRing = [](const TArray<FVector2D>& Ring, double X, double Y)
	{
		bool bIn = false;
		for (int32 K = 0, Before = Ring.Num() - 1; K < Ring.Num(); Before = K++)
		{
			const FVector2D& A = Ring[K];
			const FVector2D& B = Ring[Before];
			if ((A.Y > Y) != (B.Y > Y) && X < (B.X - A.X) * (Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bIn = !bIn;
			}
		}
		return bIn;
	};
	for (const FSkyZone& Zone : Zones)
	{
		if (Zone.Sky <= 0)
		{
			continue;
		}
		for (const int32 RegionId : Zone.Regions)
		{
			if (!Data.Regions.IsValidIndex(RegionId) || !Data.Regions[RegionId].Bounds.bIsValid)
			{
				continue;
			}
			const FCampaignRegion& Region = Data.Regions[RegionId];
			// (On the map of the whole world a province may be kept a turn of the world west of where the picture has
			// it, as North America is: its columns are counted where it is kept, and written where the picture has them.)
			int32 I0 = FMath::FloorToInt32((Region.Bounds.Min.X - X0) / Texel), I1 = FMath::CeilToInt32((Region.Bounds.Max.X - X0) / Texel);
			if (!bWhole)
			{
				I0 = FMath::Clamp(I0, 0, NX - 1);
				I1 = FMath::Clamp(I1, 0, NX - 1);
			}
			const int32 J0 = FMath::Clamp(FMath::FloorToInt32((Region.Bounds.Min.Y - Y0) / Texel), 0, NY - 1), J1 = FMath::Clamp(FMath::CeilToInt32((Region.Bounds.Max.Y - Y0) / Texel), 0, NY - 1);
			for (int32 J = J0; J <= J1; ++J)
			{
				for (int32 I = I0; I <= I1; ++I)
				{
					const double X = X0 + (I + 0.5) * Texel, Y = Y0 + (J + 0.5) * Texel;
					for (const TArray<FVector2D>& Ring : Region.Polygons)
					{
						if (InRing(Ring, X, Y))
						{
							uint8& Here = Kind[J * NX + ((I % NX) + NX) % NX];
							Here = FMath::Max(Here, static_cast<uint8>(FMath::Min(Zone.Sky, 2)));
							break;
						}
					}
				}
			}
		}
	}
	// A zone takes the water off its shores with it (north-west Europe with the Channel and the North Sea is one):
	// its sky spreads over the sea for some thirty-six units of the map, and stops at any other land.
	{
		TArray<int8> Wet; // (unknown until asked: the ground's height is only looked up along the spreading edge)
		Wet.Init(-1, NX * NY);
		TArray<int32> Front;
		for (int32 Index = 0; Index < Kind.Num(); ++Index)
		{
			if (Kind[Index] > 0)
			{
				Front.Add(Index);
			}
		}
		const int32 Reach = FMath::RoundToInt32(36.0 / Texel);
		for (int32 Depth = 0; Depth < Reach && Front.Num() > 0; ++Depth)
		{
			TArray<int32> Next;
			for (const int32 From : Front)
			{
				const int32 I = From % NX, J = From / NX;
				for (const FIntPoint& Step : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
				{
					const int32 NI = bWhole ? ((I + Step.X) % NX + NX) % NX : I + Step.X, NJ = J + Step.Y; // (the whole world goes round)
					if (NI < 0 || NJ < 0 || NI >= NX || NJ >= NY || Kind[NJ * NX + NI] > 0)
					{
						continue;
					}
					int8& Sea = Wet[NJ * NX + NI];
					if (Sea < 0)
					{
						Sea = GroundAt(X0 + (NI + 0.5) * Texel, Y0 + (NJ + 0.5) * Texel) <= 0.0f ? 1 : 0;
					}
					if (Sea == 1)
					{
						Kind[NJ * NX + NI] = Kind[From];
						Next.Add(NJ * NX + NI);
					}
				}
			}
			Front = MoveTemp(Next);
		}
	}
	// Soft at the zones' edges.
	TArray<float> Cloud, Closed;
	Cloud.SetNumUninitialized(Kind.Num());
	Closed.SetNumUninitialized(Kind.Num());
	for (int32 Index = 0; Index < Kind.Num(); ++Index)
	{
		Cloud[Index] = Kind[Index] == 1 ? 1.0f : 0.0f;
		Closed[Index] = Kind[Index] == 2 ? 1.0f : 0.0f;
	}
	FCampaignTerrain::Smooth(Cloud, NX, NY, 1, 1);
	FCampaignTerrain::Smooth(Closed, NX, NY, 1, 1);
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Kind.Num());
	for (int32 Index = 0; Index < Kind.Num(); ++Index)
	{
		Pixels[Index] = FColor(static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Cloud[Index] * 255.0f), 0, 255)), static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Closed[Index] * 255.0f), 0, 255)), 0, 255);
	}
	if (!SkyPicture || SkyNX != NX || SkyNY != NY)
	{
		SkyPicture = CampaignTextures::Create(NX, NY, Pixels, false, bWhole, TEXT("TableSky")); // (the whole world's goes round: its twin to the west reads the same picture)
		SkyNX = NX;
		SkyNY = NY;
		SkyDressed.Reset(); // (every material is handed the new picture)
	}
	else
	{
		CampaignTextures::Update(SkyPicture, Pixels);
	}
	// (how a point of the table finds its pixel: east and north, as the fog's picture is found)
	SkyFrame = FLinearColor(static_cast<float>(1.0 / (UnitScale * Wide)), static_cast<float>((Data.Width * 0.5 - X0) / Wide),
		static_cast<float>(-1.0 / (UnitScale * High)), static_cast<float>((Data.Height * 0.5 - Y0) / High));
	int32 InCloud = 0, Shut = 0;
	for (const uint8 Each : Kind)
	{
		InCloud += Each == 1 ? 1 : 0;
		Shut += Each == 2 ? 1 : 0;
	}
	UE_LOG(LogTheater, Log, TEXT("The sky laid: %d zones, a picture %d by %d, %.0f%% of it in cloud and %.0f%% closed"), Zones.Num(), NX, NY, 100.0 * InCloud / Kind.Num(), 100.0 * Shut / Kind.Num());
}

void ACampaignMapActor::DrawSkyEdge(float CameraDistance, const FVector& CameraFocus)
{
	if (!SkyEdgeMesh)
	{
		return;
	}
	const float Shown = bSkyKnown ? SkyViewNow : 0.0f;
	if (Shown <= 0.001f || SkyRuns.Num() == 0)
	{
		if (SkyEdgeMesh->GetNumSections() > 0)
		{
			SkyEdgeMesh->ClearAllMeshSections();
		}
		SkyEdgeDrawnAt = -1.0f;
		return;
	}
	// (Its line keeps its width on the screen: it is drawn again when the eye has risen or come down by a twelfth,
	// when the eye has moved a quarter of its height across the table, and through the third of a second it fades.)
	if (SkyEdgeDrawnAt > 0.0f && FMath::Abs(CameraDistance - SkyEdgeDrawnAt) < 0.085f * SkyEdgeDrawnAt && FMath::Abs(Shown - SkyEdgeDrawnView) < 0.001f
		&& FVector::DistSquared2D(CameraFocus, SkyEdgeDrawnFocus) < FMath::Square(0.25 * CameraDistance))
	{
		return;
	}
	SkyEdgeDrawnAt = CameraDistance;
	SkyEdgeDrawnView = Shown;
	SkyEdgeDrawnFocus = CameraFocus;
	SkyEdgeMesh->ClearAllMeshSections();
	if (!SkyEdgeInk && HighlightMaterial)
	{
		SkyEdgeInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		SkyEdgeInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
	}
	UMaterialInterface* Ink = SkyEdgeInk ? static_cast<UMaterialInterface*>(SkyEdgeInk) : static_cast<UMaterialInterface*>(HighlightMaterial);
	// WHERE ONE SKY ENDS (the art director's weather-sky brief, section 3). Only in the sky view. Where two zones with
	// different skies meet, a pale scalloped line, a row of small arcs like the edge of a cloud in a child's drawing,
	// follows the provinces' edges. The scallops bulge out of the worse sky, so that a closed zone reads as one great
	// cloud. Arcs about 17 pixels across from the top; the line 1.9 pixels of (246, 248, 251) over a soft dark line
	// of 4.4; thinner, 1.3 pixels at 82 parts in 100, between cloud and clear. Scallops and never dashes: it cannot
	// be taken for the front line, a route or a railway.
	const double Pixel = 0.00037 * CameraDistance / UnitScale; // (a pixel of the 1600 view, in the map's own measure)
	const double Chord = FMath::Lerp(24.0, 17.0, FMath::SmoothStep(4000.0, 14000.0, static_cast<double>(CameraDistance))) * Pixel;
	const FVector2D Eye = WorldToMap(CameraFocus);
	const double Reach = (1.7 * CameraDistance + 600.0) / UnitScale;
	const FBox2D Seen(Eye - FVector2D(Reach, Reach), Eye + FVector2D(Reach, Reach));
	FLinearColor Pale(FColor(246, 248, 251)), Dark(FColor(18, 22, 30));
	FCampaignMeshBuffers Under, Over;
	for (const FSkyRun& Run : SkyRuns)
	{
		if (!Run.Bounds.Intersect(Seen) || Run.Points.Num() < 2)
		{
			continue;
		}
		const bool bFaint = Run.Worse == 1; // (between cloud and clear)
		Pale.A = (bFaint ? 0.82f : 1.0f) * Shown;
		Dark.A = (bFaint ? 0.3f : 0.42f) * Shown;
		const double HalfLine = (bFaint ? 0.65 : 0.95) * Pixel, HalfUnder = 2.2 * Pixel;
		// How far along the run each of its points lies.
		TArray<double> Marks;
		Marks.Add(0.0);
		for (int32 K = 1; K < Run.Points.Num(); ++K)
		{
			Marks.Add(Marks.Last() + FVector2D::Distance(Run.Points[K - 1], Run.Points[K]));
		}
		const double Long = Marks.Last();
		if (Long < 0.4 * Chord)
		{
			continue;
		}
		const int32 Arcs = FMath::Max(1, FMath::RoundToInt32(Long / Chord));
		auto At = [&](double Along)
		{
			int32 K = 1;
			while (K < Marks.Num() - 1 && Marks[K] < Along)
			{
				++K;
			}
			const double Span = FMath::Max(Marks[K] - Marks[K - 1], 1.0e-6);
			return FMath::Lerp(Run.Points[K - 1], Run.Points[K], FMath::Clamp((Along - Marks[K - 1]) / Span, 0.0, 1.0));
		};
		for (int32 Arc = 0; Arc < Arcs; ++Arc)
		{
			const FVector2D From = At(Long * Arc / Arcs), To = At(Long * (Arc + 1) / Arcs);
			if (!Seen.IsInside(From))
			{
				continue;
			}
			const double Half = 0.5 * FVector2D::Distance(From, To);
			if (Half < 1.0e-4)
			{
				continue;
			}
			const FVector2D Way = (To - From) / (2.0 * Half), Out = FVector2D(-Way.Y, Way.X) * Run.Side, Middle = 0.5 * (From + To);
			TArray<FVector2D> Bow;
			for (int32 Step = 0; Step <= 8; ++Step)
			{
				const double Angle = PI * Step / 8.0;
				Bow.Add(Middle - Way * (FMath::Cos(Angle) * Half) + Out * (FMath::Sin(Angle) * 0.84 * Half));
			}
			AppendRibbon(Under, Bow, HalfUnder, 0.3, Dark.ToFColor(false), false);
			AppendRibbon(Over, Bow, HalfLine, 0.34, Pale.ToFColor(false), false);
		}
	}
	Under.Commit(SkyEdgeMesh, 0, Ink);
	Over.Commit(SkyEdgeMesh, 1, Ink);
	SkyEdgeMesh->SetCastShadow(false);
	SkyEdgeMesh->SetTranslucentSortPriority(1); // (over the ground's washes; under the table's signs)
}

void ACampaignMapActor::UpdateSky(float CameraDistance, const FVector& CameraFocus)
{
	THEATER_SPEED_SCOPE(UpdateSky);
	UWorld* World = GetWorld();
	if (!World || !LandMaterial || !Data.IsLoaded())
	{
		return;
	}
	// ("-TheaterSky=closed:North-west Europe:FRA+BEL+NED+UK;cloud:Poland and the Danube:POL+HUN" lays skies for a
	// picture: each zone's sky, its name, and its ground by country as the map was drawn or by province name.
	// "-TheaterSkyView" holds the sky view on, "-TheaterSkySnow" makes it winter under a closed sky, and
	// "-TheaterTune=SkyDim:0.5;SkyCast:0.9,0.98,1.1" tries other numbers on the table's materials.)
	static bool bAsked = false;
	if (!bAsked)
	{
		bAsked = true;
		bSkyViewHeld = FParse::Param(FCommandLine::Get(), TEXT("TheaterSkyView"));
		bSkySnowHeld = FParse::Param(FCommandLine::Get(), TEXT("TheaterSkySnow"));
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSky="), Asked, false))
		{
			TArray<FSkyZone> Staged;
			TArray<FString> Each;
			Asked.ParseIntoArray(Each, TEXT(";"));
			for (const FString& One : Each)
			{
				TArray<FString> Parts;
				if (One.ParseIntoArray(Parts, TEXT(":")) < 3)
				{
					continue;
				}
				FSkyZone& Zone = Staged.AddDefaulted_GetRef();
				Zone.Sky = Parts[0] == TEXT("closed") ? 2 : Parts[0] == TEXT("cloud") ? 1 : 0; // ("clear:" names a zone with a clear sky: it has a plate too)
				Zone.Name = Parts[1];
				TArray<FString> Grounds;
				Parts[2].ParseIntoArray(Grounds, TEXT("+"));
				for (const FString& Ground : Grounds)
				{
					if (const int32* Named = Data.RegionByName.Find(Ground))
					{
						Zone.Regions.AddUnique(*Named);
						continue;
					}
					for (const FCampaignRegion& Region : Data.Regions)
					{
						if (Region.Owner == Ground)
						{
							Zone.Regions.AddUnique(Region.Id);
						}
					}
				}
			}
			ShowSky(Staged);
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterTune="), Asked, false))
		{
			TArray<FString> Each;
			Asked.ParseIntoArray(Each, TEXT(";"));
			for (const FString& One : Each)
			{
				FString Name, Value;
				TArray<FString> Bits;
				if (!One.Split(TEXT(":"), &Name, &Value))
				{
					continue;
				}
				if (Value.ParseIntoArray(Bits, TEXT(",")) >= 3)
				{
					TuneVectors.Emplace(FName(*Name), FLinearColor(FCString::Atof(*Bits[0]), FCString::Atof(*Bits[1]), FCString::Atof(*Bits[2]), Bits.IsValidIndex(3) ? FCString::Atof(*Bits[3]) : 1.0f));
				}
				else
				{
					TuneScalars.Emplace(FName(*Name), FCString::Atof(*Value));
				}
			}
		}
	}
	if (!bSkyEverKnown && TuneScalars.Num() == 0 && TuneVectors.Num() == 0)
	{
		return; // (no weather has ever been laid: every material's sky is off, as it was made)
	}
	// Everything that wears the sky: the ground, the sea, the towns and the woods. Never a piece, a flag or a plate.
	TArray<UMaterialInstanceDynamic*, TInlineAllocator<16>> Wearing;
	// (and the rivers that are laid as ribbons: left out, they ran bright blue through a closed zone)
	for (UMaterialInstanceDynamic* Each : { LandMaterial.Get(), WorldLandMaterial.Get(), SeaMaterial.Get(), WorldSeaMaterial.Get(), SceneryMaterial.Get(), WoodsMaterial.Get(), WaterMaterial.Get() })
	{
		if (Each)
		{
			Wearing.Add(Each);
		}
	}
	for (const auto& Pair : FineSheets)
	{
		if (Pair.Value->LandPaint)
		{
			Wearing.Add(Pair.Value->LandPaint);
		}
	}
	if (bSkyEverKnown)
	{
		if (!WeatherPicture)
		{
			TArray<FColor> Pixels;
			CampaignTextures::MakeWeather(Pixels, false);
			WeatherPicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableWeather"));
			CampaignTextures::MakeWeather(Pixels, true);
			WeatherSnowPicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableWeatherSnow"));
		}
		// (in the rules' winter months a closed sky snows instead of rains)
		const bool bSnow = bSkySnowHeld || WinterHard > 0.5f;
		if (bSnow != bSkySnowShown)
		{
			bSkySnowShown = bSnow;
			SkyDressed.Reset();
		}
		const float Delta = World->GetDeltaSeconds();
		SkyViewNow = FMath::Clamp(SkyViewNow + ((bSkyViewWanted || bSkyViewHeld) ? Delta : -Delta) * 3.0f, 0.0f, 1.0f);
		const float Moon = GetMoonlight();
		const double Clock = World->GetTimeSeconds();
		// (The cloud drifts about one screen in three minutes, and starts again unseen where both its pictures have
		// turned a whole number of times.)
		const float Travel = static_cast<float>(FMath::Fmod(Clock * 13.0, 327600.0));
		// (Rain falls down the screen and a third as far across it; snow falls slowly. Neither falls fast: the picture
		// is smoothed from frame to frame, and a streak that crossed many pixels in a frame was smoothed away to nothing.)
		const double Fall = bSnow ? 0.05 : 0.22;
		const FLinearColor RainFall(static_cast<float>(FMath::Frac(-Clock * Fall / 3.0)), static_cast<float>(FMath::Frac(-Clock * Fall)), 0.0f, 0.0f);
		int32 ViewWide = 1600, ViewHigh = 1000;
		if (const APlayerController* Viewer = World->GetFirstPlayerController())
		{
			Viewer->GetViewportSize(ViewWide, ViewHigh);
		}
		const FLinearColor RainTiles(3.125f, 3.125f * ViewHigh / FMath::Max(ViewWide, 1), 0.0f, 0.0f);
		// By night the sheet and the puffs are moonlit, and the lid dims less: the night is already dark.
		// (Tuned by measure over Germany from playing height, 17 November 2026. Closed at rest: the ground (108, 106, 79)
		// stands at (84, 81, 67), 78 parts in 100 as bright, and the sea (59, 154, 169) at (66, 108, 126). In the sky
		// view the sheet brings the sea to (109, 133, 146). Cloud at rest takes the ground down a twentieth all told.)
		// (The art director's ruling of 25 November 2026 04:54Z, art-director/reviews/map-winter-sky-ruling.md, on a 1.5
		// still of France in January 1940: by day the lid was a cyan DYE over the land, not a pearl wash. She measured
		// France inside the zone at red/green 0.67 to 0.76 with blue/green 1.19 to 1.30, against Belgium a step outside
		// at 1.14 / 0.76, and ruled that washed land must keep its own hue direction and only be pulled toward neutral:
		// red/green 0.88 to 1.02, blue/green 1.00 to 1.08. The old sheet stood at blue/green 1.15, which over
		// green-olive land multiplies into water. These are her starting numbers at the same brightness, hers to judge
		// again on the next pictures. By night the lerp is untouched: a cold blue lid by moonlight is right.)
		const FLinearColor Sheet = FMath::Lerp(FLinearColor(0.66f, 0.665f, 0.70f), FLinearColor(0.107f, 0.138f, 0.223f), Moon);
		const FLinearColor Puff = FMath::Lerp(FLinearColor(0.902f, 0.906f, 0.916f), FLinearColor(0.202f, 0.246f, 0.352f), Moon);
		const FLinearColor Mist = FMath::Lerp(FLinearColor(0.645f, 0.658f, 0.680f), FLinearColor(0.15f, 0.17f, 0.21f), Moon);
		// (snow lying under the lid is to read L150 to 175 at blue/green 1.02 to 1.05; for rain her word is that the
		// present ink is near enough, so each weather keeps its own)
		const FLinearColor RainInk = FMath::Lerp(bSnow ? FLinearColor(0.86f, 0.865f, 0.875f) : FLinearColor(0.776f, 0.823f, 0.888f), FLinearColor(0.24f, 0.26f, 0.31f), Moon);
		const float Dim = FMath::Lerp(0.57f, 0.71f, Moon);
		for (UMaterialInstanceDynamic* Each : Wearing)
		{
			if (!SkyDressed.Contains(Each))
			{
				SkyDressed.Add(Each);
				Each->SetTextureParameterValue(TEXT("Sky"), SkyPicture);
				Each->SetTextureParameterValue(TEXT("Weather"), bSnow ? WeatherSnowPicture : WeatherPicture);
				Each->SetVectorParameterValue(TEXT("SkyFrame"), SkyFrame);
			}
			// (The brief, section 10: on the staff's paper map there is no wash and no cloud, only the zones' edges and
			// their plates. The sky goes as the paper comes.)
			Each->SetScalarParameterValue(TEXT("SkyOn"), bSkyKnown && SkyPicture ? 1.0f - FMath::Clamp(GetPaper(), 0.0f, 1.0f) : 0.0f);
			Each->SetScalarParameterValue(TEXT("SkyView"), SkyViewNow);
			Each->SetScalarParameterValue(TEXT("SkyTravel"), Travel);
			Each->SetScalarParameterValue(TEXT("SkyDim"), Dim);
			// (Her fault 2, the same ruling: inside a 100 by 100 patch of washed land the spread of brightness is to be
			// no more than a quarter of the patch's mean, where Touraine stood at 0.45. The falling weather is the loud
			// part of that spread, so it is laid on at about three fifths of its old weight; its marks are softened and
			// turned every way in MakeWeather. Hers to measure again.)
			Each->SetScalarParameterValue(TEXT("SkyRain"), bSnow ? 0.3f : 0.2f);
			// (the mist's bands are for where he plays: from high up they are gone before they can read as ruled lines)
			Each->SetScalarParameterValue(TEXT("SkyMists"), 0.02f * (1.0f - FMath::SmoothStep(5000.0f, 9000.0f, CameraDistance)));
			Each->SetVectorParameterValue(TEXT("RainFall"), RainFall);
			Each->SetVectorParameterValue(TEXT("RainTiles"), RainTiles);
			Each->SetVectorParameterValue(TEXT("SkySheet"), Sheet);
			Each->SetVectorParameterValue(TEXT("SkyPuff"), Puff);
			Each->SetVectorParameterValue(TEXT("SkyMist"), Mist);
			Each->SetVectorParameterValue(TEXT("SkyRainInk"), RainInk);
		}
		if (SkyDressed.Num() > 96)
		{
			SkyDressed.Reset(); // (sheets of ground come and go: the list is begun again now and then)
		}
		DrawSkyEdge(CameraDistance, CameraFocus);
	}
	for (UMaterialInstanceDynamic* Each : Wearing)
	{
		for (const TPair<FName, float>& Tune : TuneScalars)
		{
			Each->SetScalarParameterValue(Tune.Key, Tune.Value);
		}
		for (const TPair<FName, FLinearColor>& Tune : TuneVectors)
		{
			Each->SetVectorParameterValue(Tune.Key, Tune.Value);
		}
	}
}

// ---------------------------------------------------------------------------- the agents' pieces (ruling 153)

namespace
{
	/**
	 * How tall an agent's piece stands on the table, from the foot of its base to the crown of its hat. The art
	 * director's brief: 40 pixels from where he plays, on a base 20 across, so that he is easy to click and is never
	 * taken for an army. Her drawing looks down on him from 40 degrees; the game's eye looks down more steeply, and
	 * what stands upright is seen shorter: at her base's width he was 22 pixels tall. He is made larger, to stand 30
	 * pixels tall on a base 29 across: three times an infantryman, a third of a corps' base. ("-TheaterAgentHigh=78"
	 * tries another.)
	 */
	double AgentHigh()
	{
		static float High = -1.0f;
		if (High < 0.0f)
		{
			High = 78.0f;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterAgentHigh="), High);
		}
		return High;
	}
}

FVector2D ACampaignMapActor::AgentSpot(int32 RegionId, int32 Slot, int32 Count) const
{
	// (The brief, section 4.) On open ground in the province where he works, about a third of the way from the town
	// toward the province's edge. Never on the town's roofs, an army's base, the airfield or a cut's board: he moves
	// round to the nearest clear ground, as a ring's weapons do.
	const FCampaignRegion& Region = Data.Regions[RegionId];
	const double Town = TownRadius(RegionId);
	TArray<TPair<FVector2D, double>, TInlineAllocator<8>> Keep;
	bool bArmyHere = false;
	for (const AArmyPiece* Piece : Pieces)
	{
		if (!Piece || !Piece->IsArmy())
		{
			continue;
		}
		// (The art director's review of the piece: his seat, and a caught mark's, is at least 30 pixels from the foot
		// of any flag's pole. The pole stands 5.6 units from the middle of its base, on whichever side the eye puts it.
		// A neighbouring province's army counts too: it may stand just over the border.)
		const FVector2D Stands = WorldToMap(Piece->GetRestLocation());
		if (Piece->GetRegionId() == RegionId || (Stands.X > Region.Bounds.Min.X - 24.0 && Stands.X < Region.Bounds.Max.X + 24.0 && Stands.Y > Region.Bounds.Min.Y - 24.0 && Stands.Y < Region.Bounds.Max.Y + 24.0))
		{
			Keep.Emplace(Stands, 10.5);
			bArmyHere |= Piece->GetRegionId() == RegionId;
		}
	}
	if (!bArmyHere)
	{
		Keep.Emplace(WorldToMap(ArmySpot(RegionId, 0)), 10.5); // (where an army would stand, if one comes)
	}
	// (Nor behind a flag: from where he plays, a standard's pole, its tag and its cloth hang over the ground north of
	// its base, and a seat there is hidden by them. The ground from four units to twenty north of each base is kept
	// clear, a little to the west of it for the pole and to the east for the cloth.)
	for (int32 Army = Keep.Num() - 1; Army >= 0; --Army)
	{
		const FVector2D Base = Keep[Army].Key;
		Keep.Emplace(Base + FVector2D(-2.5, -8.0), 4.5);
		Keep.Emplace(Base + FVector2D(-0.5, -14.0), 7.0);
	}
	const int32 ArmiesKept = Keep.Num();
	if (const FVector2D* Field = AirfieldSpots.Find(RegionId))
	{
		Keep.Emplace(*Field, 6.5);
	}
	Keep.Emplace(CutSpot(RegionId), 3.0);
	auto Bearing = [](double Degrees) { const double Rad = FMath::DegreesToRadians(Degrees); return FVector2D(FMath::Sin(Rad), -FMath::Cos(Rad)); }; // (east of north)
	// Beside the town first, east of it or west, where neither its roofs nor its name plate (which hangs toward the
	// eye) cover him; then round it by other bearings.
	FVector2D BestSpot = Region.Centroid;
	double BestScore = -1.0e9;
	int32 Tried = 0;
	for (const double Degrees : { 100.0, 260.0, 70.0, 290.0, 130.0, 230.0, 40.0, 320.0, 160.0, 200.0, 10.0, 180.0 })
	{
		++Tried;
		const FVector2D Way = Bearing(Degrees);
		// How far the province runs that way from its town.
		double Edge = Town;
		while (Edge < 60.0)
		{
			const FVector2D Next = Region.Center + Way * (Edge + 1.0);
			if (Data.FindRegionAt(Next.X, Next.Y) != RegionId || GroundAt(Next.X, Next.Y) <= 0.0f)
			{
				break;
			}
			Edge += 1.0;
		}
		if (Edge - Town < 1.5)
		{
			continue; // (no room that way at all)
		}
		const FVector2D Across(-Way.Y, Way.X);
		// (two agents in one province stand side by side, 24 pixels apart; where the province is narrow that way he
		// stands halfway between the last house and the border)
		const double Out = Edge - Town >= 4.0 ? FMath::Max((Edge - Town) / 3.0, 2.4) : 0.5 * (Edge - Town);
		const FVector2D Spot = Region.Center + Way * (Town + Out) + Across * (3.5 * (Slot - 0.5 * (Count - 1)));
		if (Data.FindRegionAt(Spot.X, Spot.Y) != RegionId || GroundAt(Spot.X, Spot.Y) <= 0.0f)
		{
			continue;
		}
		double Room = 1.0e9;
		for (const TPair<FVector2D, double>& Thing : Keep)
		{
			Room = FMath::Min(Room, FVector2D::Distance(Spot, Thing.Key) - Thing.Value);
		}
		// (a clear place before any that is not; a roomy way before a narrow one; then the order they are tried in)
		const double Score = (Room >= 0.0 ? 1000.0 : 20.0 * Room) - (Edge - Town >= 4.0 ? 0.0 : 300.0) - 10.0 * Tried;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestSpot = Spot;
		}
	}
	if (BestScore < 500.0)
	{
		// Nowhere beside the town is clear: a city that fills its province, or one crowded with armies. Every point of
		// its ground is weighed, and the one that trespasses least is taken. On an army's base counts most; then the
		// airfield; then behind a flag; and among a city's outer houses hardly at all, though not in its middle.
		// (A base at the foot of an army's pole was taken for the flag's own, and a figure on a base for its soldier.)
		const int32 BasesKept = ArmiesKept / 3; // (each base brought two more places with it: behind its pole, and behind its cloth)
		double Least = 1.0e18;
		for (double Y = Region.Bounds.Min.Y; Y <= Region.Bounds.Max.Y; Y += 1.25)
		{
			for (double X = Region.Bounds.Min.X; X <= Region.Bounds.Max.X; X += 1.25)
			{
				bool bStands = true; // (his whole base on this province's dry ground)
				for (const FVector2D& Rim : { FVector2D(0.0, 0.0), FVector2D(2.2, 0.0), FVector2D(-2.2, 0.0), FVector2D(0.0, 2.2), FVector2D(0.0, -2.2) })
				{
					bStands &= Data.FindRegionAt(X + Rim.X, Y + Rim.Y) == RegionId && GroundAt(X + Rim.X, Y + Rim.Y) > 0.0f;
				}
				if (!bStands)
				{
					continue;
				}
				const FVector2D Here(X, Y);
				double Cost = FMath::Max(0.0, 0.75 * Town - FVector2D::Distance(Here, Region.Center)) + 0.01 * FVector2D::Distance(Here, Region.Centroid);
				for (int32 Kept = 0; Kept < Keep.Num(); ++Kept)
				{
					const double Short = Keep[Kept].Value - FVector2D::Distance(Here, Keep[Kept].Key);
					if (Short > 0.0)
					{
						Cost += Short * (Kept < BasesKept ? 4.0 : Kept < ArmiesKept ? 1.5 : 2.0);
					}
				}
				if (Cost < Least)
				{
					Least = Cost;
					BestSpot = Here + FVector2D(3.5 * (Slot - 0.5 * (Count - 1)), 0.0);
				}
			}
		}
	}
	return BestSpot;
}

void ACampaignMapActor::ShowAgents(const FCampaignView& View)
{
	THEATER_SPEED_SCOPE(ShowAgents);
	if (!GetWorld() || !Data.IsLoaded() || HasParade())
	{
		return;
	}
	// (his place depends on the armies that stand on his ground and round it: their number there, and all told)
	auto ArmiesIn = [this](int32 RegionId)
	{
		int32 Standing = 0;
		for (const AArmyPiece* Piece : Pieces)
		{
			Standing += Piece && Piece->GetRegionId() == RegionId ? 1000 : Piece ? 1 : 0;
		}
		return Standing;
	};
	// (an agent's place depends on the armies that stand on his ground: he is stood afresh when they come or go)
	FString Told;
	for (const FViewAgent& Agent : View.Agents)
	{
		Told += FString::Printf(TEXT("%d@%d/%d;"), Agent.Id, Agent.Region, ArmiesIn(Agent.Region));
	}
	for (const FAgentMark& Mark : View.AgentMarks)
	{
		Told += FString::Printf(TEXT("x@%d/%d;"), Mark.Region, ArmiesIn(Mark.Region));
	}
	for (const FViewFoeAgent& Foe : View.FoeAgents)
	{
		Told += FString::Printf(TEXT("f%d@%d/%d:%s;"), Foe.Id, Foe.Region, ArmiesIn(Foe.Region), *Foe.Nat);
	}
	if (Told == AgentSignature)
	{
		return;
	}
	AgentSignature = Told;
	for (USceneComponent* Part : AgentParts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	AgentParts.Reset();
	AgentStands.Reset();
	TMap<int32, int32> CountIn, SlotIn;
	for (const FViewAgent& Agent : View.Agents)
	{
		++CountIn.FindOrAdd(Agent.Region);
	}
	for (const FAgentMark& Mark : View.AgentMarks)
	{
		++CountIn.FindOrAdd(Mark.Region);
	}
	for (const FViewFoeAgent& Foe : View.FoeAgents)
	{
		++CountIn.FindOrAdd(Foe.Region);
	}
	const double High = AgentHigh();
	UMaterialInterface* RingGlow = GetSolidMaterial(FLinearColor(FColor::FromHex(TEXT("FFD27A"))), 0.4f, 0.0f, 2.5f);
	// (the brass of the sculpt's own rim, 224 184 114, giving a little light of its own: see section 2 below)
	UMaterialInterface* RimGlow = GetSolidMaterial(FLinearColor(FColor(224, 184, 114)), 0.4f, 0.0f, 0.6f);
	// (Release 2: the warm pool at his feet is light added to the ground, in the paint of a burning town's glow (M_Glow),
	// and one ink serves every agent's pool. Without that paint there is no pool; the log has said so already.)
	UMaterialInstanceDynamic* PoolInk = bFireGlowAdds && FireGlowInk ? UMaterialInstanceDynamic::Create(FireGlowInk->GetMaterial(), this) : nullptr;
	auto StandOne = [&](int32 Id, int32 RegionId, bool bMark, const FViewFoeAgent* Foe = nullptr)
	{
		if (!Data.Regions.IsValidIndex(RegionId))
		{
			return;
		}
		FAgentStand& Made = AgentStands.AddDefaulted_GetRef();
		Made.Id = Id;
		Made.Region = RegionId;
		Made.bMark = bMark;
		Made.bFoe = Foe != nullptr;
		if (Foe)
		{
			// (Release 2, the spies thread of 24 November 2026; the art director's brief of 22 November: an uncovered enemy
			// agent is the same sculpt, its rim in its country's colour, and no pool.)
			const FCampaignNation* Whose = Data.FindNation(Foe->Nat);
			Made.FoeInk = Whose ? FLinearColor(Whose->Color) : FLinearColor(FColor(150, 40, 40));
		}
		const FVector2D Spot = AgentSpot(RegionId, SlotIn.FindOrAdd(RegionId)++, CountIn[RegionId]);
		Made.Rest = SurfacePoint(Spot.X, Spot.Y, 0.02);
		USceneComponent* Holder = NewObject<USceneComponent>(this);
		Holder->SetupAttachment(Root);
		Holder->RegisterComponent();
		Holder->SetWorldLocationAndRotation(Made.Rest, FRotator(0.0f, LastPieceYaw, 0.0f));
		AgentParts.Add(Holder);
		Made.Holder = Holder;
		// The art director's own sculpt for release 2, read from Content/Models (her brief of 22 November 2026): a man in a
		// trench coat and fedora reading an open newspaper, one for every country, on his brass-rimmed base. The mark of
		// one caught is the empty base, with his hat lying brim-down beside a folded newspaper. (Both are made by
		// Tools/obj_to_model.py from Tools/sculpts/agent-r2, by the two commands at the head of that script, at the
		// lantern piece's scale and foot line. The lantern piece's own files are kept for release 1 and not read here.)
		// (Release 2, the art director's read of run 2, 22 November: mend 8. The caught mark read as a pale scrap in a brass
		// ring, its hat lost at that size: it stands a quarter as large again as the agent's own piece.)
		FMiniatureMesh Figure;
		const double PieceHigh = bMark ? 1.25 * High : High;
		const FVector Foot(0.0, 0.0, 0.05 * PieceHigh); // (the sculpt's base runs a twentieth of his height under its own top)
		Figure.Sculpted(bMark ? TEXT("AgentReaderMark") : TEXT("AgentReader"), TEXT("own"), Foot, PieceHigh, 0.0);
		UProceduralMeshComponent* Body = NewObject<UProceduralMeshComponent>(this);
		Body->SetupAttachment(Holder);
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->RegisterComponent();
		AgentParts.Add(Body);
		Made.Body = Body;
		if (Figure.Triangles.Num() > 0)
		{
			Body->CreateMeshSection(0, Figure.Vertices, Figure.Triangles, Figure.Normals, TArray<FVector2D>(), Figure.Colors, TArray<FProcMeshTangent>(), false);
			Body->SetMaterial(0, GetPieceMaterial());
		}
		if (bMark)
		{
			return;
		}
		// In the hand, and under the pointer: a second brass ring lit on the base, inside its rim (section 1).
		{
			TArray<FVector> Points, Facing;
			TArray<int32> Faces;
			const int32 Sides = 40;
			const double Inner = 0.186 * High, Outer = 0.224 * High, Level = Foot.Z + 0.003 * High;
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const double Angle = 2.0 * PI * Side / Sides;
				Points.Add(FVector(FMath::Cos(Angle) * Inner, FMath::Sin(Angle) * Inner, Level));
				Points.Add(FVector(FMath::Cos(Angle) * Outer, FMath::Sin(Angle) * Outer, Level));
				Facing.Add(FVector::UpVector);
				Facing.Add(FVector::UpVector);
			}
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const int32 A0 = 2 * Side, B0 = A0 + 1, A1 = 2 * ((Side + 1) % Sides), B1 = A1 + 1;
				Faces.Append({ A0, A1, B0, B0, A1, B1, A0, B0, A1, B0, B1, A1 }); // (both ways round: it is seen from above whichever the engine takes for its face)
			}
			Body->CreateMeshSection(1, Points, Faces, Facing, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
			Body->SetMaterial(1, RingGlow);
			Body->SetMeshSectionVisible(1, false);
			// (Release 2, the art director's read of run 2, 22 November: mend 7.) By night the sculpt's brass rim went dark
			// with the table, and it is how his own agent is told from the enemy's. A thin band of the same brass lies on
			// the rim's top (the sculpt's rim runs from 0.232 to 0.27 of his height out, its top at the base's own top),
			// giving a little light of its own, and is shown only once dusk has come: by day the rim is lit as it was.
			Points.Reset();
			Facing.Reset();
			const double RimIn = 0.236 * High, RimOut = 0.266 * High, RimLevel = Foot.Z + 0.0015 * High;
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const double Angle = 2.0 * PI * Side / Sides;
				Points.Add(FVector(FMath::Cos(Angle) * RimIn, FMath::Sin(Angle) * RimIn, RimLevel));
				Points.Add(FVector(FMath::Cos(Angle) * RimOut, FMath::Sin(Angle) * RimOut, RimLevel));
				Facing.Add(FVector::UpVector);
				Facing.Add(FVector::UpVector);
			}
			Body->CreateMeshSection(2, Points, Faces, Facing, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
			// (Release 2: an enemy agent's rim band is its country's colour and is lit by day as well: UpdateAgents shows it.)
			Body->SetMaterial(2, Made.bFoe ? GetSolidMaterial(Made.FoeInk, 0.4f, 0.0f, 0.9f) : RimGlow);
			Body->SetMeshSectionVisible(2, false);
		}
		if (Made.bFoe)
		{
			return; // (no pool at an enemy agent's feet)
		}
		// The warm pool on the ground at his feet, which is how he is found on a night map. (The art director's release 2
		// brief: "the agent lights the fog" is a pool on the ground and not a light on him. The lantern's real light is
		// gone.) It lies on the table itself, over its rises and falls, and not on his base: UpdateAgents lays it across
		// the eye's view when he is first seen, and again when the eye turns. An agent of the player's own has it; the
		// mark of one caught has none, and nor will an enemy agent his hunters uncover, when the map is given one.
		if (PoolInk)
		{
			UProceduralMeshComponent* Pool = NewObject<UProceduralMeshComponent>(this);
			Pool->SetupAttachment(Root);
			Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Pool->SetCastShadow(false);
			Pool->RegisterComponent();
			Pool->SetMaterial(0, PoolInk);
			Pool->SetVisibility(false);
			AgentParts.Add(Pool);
			Made.Pool = Pool;
			Made.PoolInk = PoolInk;
		}
	};
	for (const FViewAgent& Agent : View.Agents)
	{
		StandOne(Agent.Id, Agent.Region, false);
	}
	for (const FAgentMark& Mark : View.AgentMarks)
	{
		StandOne(INDEX_NONE, Mark.Region, true);
	}
	for (const FViewFoeAgent& Foe : View.FoeAgents)
	{
		StandOne(FoeStand(Foe.Id), Foe.Region, false, &Foe);
	}
	if (AgentInHand != INDEX_NONE && !AgentStands.ContainsByPredicate([this](const FAgentStand& Each) { return !Each.bMark && Each.Id == AgentInHand; }))
	{
		AgentInHand = INDEX_NONE;
	}
	UE_LOG(LogTheater, Log, TEXT("Agents on the map: %d, %d marks where one was caught, and %d enemy agents uncovered"), View.Agents.Num(), View.AgentMarks.Num(), View.FoeAgents.Num());
}

void ACampaignMapActor::SetAgentInHand(int32 AgentId)
{
	if (AgentId != AgentInHand)
	{
		for (FAgentStand& Stand : AgentStands)
		{
			Stand.bCarried = false;
		}
		AgentInHand = AgentId;
	}
}

void ACampaignMapActor::CarryAgent(const FVector& GroundPoint)
{
	for (FAgentStand& Stand : AgentStands)
	{
		if (!Stand.bMark && Stand.Id == AgentInHand)
		{
			Stand.bCarried = true;
			Stand.CarriedAt = GroundPoint;
		}
	}
}

void ACampaignMapActor::ReturnAgent()
{
	for (FAgentStand& Stand : AgentStands)
	{
		Stand.bCarried = false;
	}
}

bool ACampaignMapActor::GetAgentSpot(int32 Index, FVector2D& OutSpot) const
{
	if (!AgentStands.IsValidIndex(Index))
	{
		return false;
	}
	OutSpot = WorldToMap(AgentStands[Index].Rest);
	return true;
}

bool ACampaignMapActor::GetAgentInHandStand(FVector& OutFoot, double& OutHigh) const
{
	for (const FAgentStand& Stand : AgentStands)
	{
		if (!Stand.bMark && Stand.Id == AgentInHand && Stand.Holder && bAgentFigures && bPiecesShown)
		{
			OutFoot = Stand.Holder->GetComponentLocation();
			OutHigh = AgentHigh();
			return true;
		}
	}
	return false;
}

int32 ACampaignMapActor::AgentAtRay(const FVector& Origin, const FVector& Direction, bool* bOutOnBody) const
{
	// (The art director's ruling: he is taken hold of within about a third of his height of his middle line. With two
	// agents the nearer middle line wins. bOutOnBody says whether the pointer is on the figure itself: over an army's
	// flag or base the army wins unless it is.)
	int32 Found = INDEX_NONE;
	double Nearest = TNumericLimits<double>::Max();
	bool bOnBody = false;
	if (bPiecesShown)
	{
		const FVector End = Origin + Direction * 200000.0;
		const double High = AgentHigh();
		for (const FAgentStand& Stand : AgentStands)
		{
			if (Stand.bMark || !Stand.Holder)
			{
				continue;
			}
			FVector OnRay, OnPart;
			if (bAgentFigures)
			{
				// (the figure and his base: a little wider than he is, so that he is easy to take hold of)
				const FVector Foot = Stand.Holder->GetComponentLocation();
				FMath::SegmentDistToSegmentSafe(Origin, End, Foot, Foot + FVector(0.0, 0.0, High), OnRay, OnPart);
				if (FVector::DistSquared(OnRay, OnPart) > FMath::Square(0.3 * High))
				{
					continue;
				}
			}
			else
			{
				if (Stand.SignRadius <= 0.0)
				{
					continue;
				}
				FMath::SegmentDistToSegmentSafe(Origin, End, Stand.SignAt, Stand.SignAt, OnRay, OnPart);
				if (FVector::DistSquared(OnRay, OnPart) > FMath::Square(1.3 * Stand.SignRadius))
				{
					continue;
				}
			}
			const double Off = FVector::Dist(OnRay, OnPart);
			if (Off < Nearest)
			{
				Nearest = Off;
				Found = Stand.Id;
				bOnBody = bAgentFigures ? Off < 0.17 * High : Off < Stand.SignRadius;
			}
		}
	}
	if (bOutOnBody)
	{
		*bOutOnBody = bOnBody;
	}
	return Found;
}

void ACampaignMapActor::UpdateAgents(float CameraDistance, float CameraYaw)
{
	THEATER_SPEED_SCOPE(UpdateAgents);
	if (AgentStands.Num() > 0)
	{
		const float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		// Figures from where he plays; from high up a sign for each, drawn with the table's other signs.
		bAgentFigures = bAgentFigures ? CameraDistance < 9400.0f : CameraDistance < 8600.0f;
		const double Pixel = 0.00037 * CameraDistance;
		const float Dusk = FMath::Clamp(NightNow, 0.0f, 1.0f);
		// The warm pool at his feet (the art director's release 2 brief, 22 November 2026): (255, 192, 112), centred
		// under his base, a faint tint by day (0.25 of full) and clear by night (0.45), with a soft edge: the art director
		// set those from the pictures of run 2 (22 November; her brief had 0.30 and 0.75). It is light added
		// to the ground, as a burning town's glow is, and no lamp: it lights no figure from below, throws no shadow and
		// costs a few hundred triangles and the pixels it covers, where the lantern's light shaded every pixel within a
		// height and a quarter of him. ("-TheaterAgentPool=0.25,0.45" tries other strengths by day and by night.)
		static float PoolDay = 0.25f, PoolNight = 0.45f;
		static bool bPoolAsked = false;
		if (!bPoolAsked)
		{
			bPoolAsked = true;
			FString Asked;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterAgentPool="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 2)
			{
				PoolDay = FCString::Atof(*Parts[0]);
				PoolNight = FCString::Atof(*Parts[1]);
			}
		}
		const float PoolStrength = FMath::Lerp(PoolDay, PoolNight, Dusk);
		const bool bShown = bPiecesShown && bAgentFigures;
		const bool bShadows = CameraDistance < 7000.0f;
		// (the pool lies across the eye's own view, even where his piece is held facing one way for a picture)
		const float EyeYaw = CameraYaw;
		// ("-TheaterAgentFacing=0" holds every agent's piece facing one way on the table, whatever the eye does: for
		// pictures of him from all round. In play he turns to face the eye, as the armies' pieces do.)
		static float HeldFacing = -1000.0f;
		static bool bFacingAsked = false;
		if (!bFacingAsked)
		{
			bFacingAsked = true;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterAgentFacing="), HeldFacing);
		}
		if (HeldFacing > -999.0f)
		{
			CameraYaw = HeldFacing;
		}
		// The pool's shape: about 1.2 of his heights across and 0.7 deep as seen from where he plays, 48 pixels by 28,
		// measured as on her sheet to where it has fallen to a third of its middle; it fades on to nothing at twice
		// that. (From playing height the eye looks down at about 58 degrees and his 40 pixels, base to crown, are 0.81
		// of his height on the table: so on the ground the pool is 0.97 of his height across the view and 0.67 along
		// it.) It is laid point by point on the ground, a little clear of it as a burning town's glow is.
		auto LayPool = [this](FAgentStand& Laying, float Yaw)
		{
			const double Tall = AgentHigh();
			const double Across = 0.485 * Tall, Along = 0.335 * Tall;
			static const double RingOut[6] = { 0.0, 0.4, 0.8, 1.2, 1.6, 2.0 };
			const int32 Spokes = 24;
			const FVector Way = FRotator(0.0f, Yaw, 0.0f).Vector(), Side(-Way.Y, Way.X, 0.0);
			const FLinearColor Warm(FColor(255, 192, 112));
			TArray<FVector> Points, Facing;
			TArray<FColor> Colours;
			for (int32 Ring = 0; Ring < 6; ++Ring)
			{
				FLinearColor Ink = Warm;
				Ink.A = Ring == 5 ? 0.0f : static_cast<float>(FMath::Exp(-RingOut[Ring] * RingOut[Ring])); // (her sheet's soft fall, and nothing at the rim)
				for (int32 Spoke = 0; Spoke < (Ring == 0 ? 1 : Spokes); ++Spoke)
				{
					const double Angle = 2.0 * PI * Spoke / Spokes;
					const FVector Over = Laying.Rest + (Way * (FMath::Cos(Angle) * Along) + Side * (FMath::Sin(Angle) * Across)) * RingOut[Ring];
					const FVector2D OnMap = WorldToMap(Over);
					Points.Add(FVector(Over.X, Over.Y, SurfacePoint(OnMap.X, OnMap.Y, 0.3).Z));
					Facing.Add(FVector::UpVector);
					Colours.Add(Ink.ToFColor(false));
				}
			}
			if (Laying.Pool->GetNumSections() > 0)
			{
				Laying.Pool->UpdateMeshSection(0, Points, Facing, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
			}
			else
			{
				TArray<int32> Faces;
				for (int32 Spoke = 0; Spoke < Spokes; ++Spoke)
				{
					const int32 A = 1 + Spoke, B = 1 + (Spoke + 1) % Spokes;
					Faces.Append({ 0, A, B }); // (one way round: its paint is seen from both sides)
					for (int32 Ring = 1; Ring + 1 < 6; ++Ring)
					{
						const int32 A0 = A + (Ring - 1) * Spokes, B0 = B + (Ring - 1) * Spokes, A1 = A0 + Spokes, B1 = B0 + Spokes;
						Faces.Append({ A0, A1, B1, A0, B1, B0 });
					}
				}
				Laying.Pool->CreateMeshSection(0, Points, Faces, Facing, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
			}
			Laying.PoolYaw = Yaw;
		};
		for (FAgentStand& Stand : AgentStands)
		{
			if (!Stand.Holder || !Stand.Body)
			{
				continue;
			}
			const bool bHeld = !Stand.bMark && Stand.Id == AgentInHand;
			// (In the hand he is lifted thirteen pixels, and his shadow stays on the ground. He rises and settles in a sixth of a second.)
			Stand.Lift = FMath::Clamp(Stand.Lift + (bHeld ? Delta : -Delta) / 0.16f, 0.0f, 1.0f);
			const double Raised = 13.0 * Pixel * Stand.Lift * (2.0 - Stand.Lift);
			Stand.Holder->SetWorldLocationAndRotation((bHeld && Stand.bCarried ? Stand.CarriedAt : Stand.Rest) + FVector(0.0, 0.0, Raised), FRotator(0.0f, CameraYaw, 0.0f));
			if (Stand.Body->IsVisible() != bShown)
			{
				Stand.Body->SetVisibility(bShown);
			}
			if (Stand.Body->CastShadow != bShadows)
			{
				Stand.Body->SetCastShadow(bShadows);
			}
			const bool bRing = !Stand.bMark && (bHeld || Stand.Id == AgentHovered);
			if (bRing != Stand.bRing)
			{
				Stand.bRing = bRing;
				Stand.Body->SetMeshSectionVisible(1, bRing);
			}
			const bool bRimLit = !Stand.bMark && (Stand.bFoe || Dusk >= 0.35f); // (mend 7: the rim stays lit by night; an enemy agent's country colour, always)
			if (bRimLit != Stand.bRimLit)
			{
				Stand.bRimLit = bRimLit;
				Stand.Body->SetMeshSectionVisible(2, bRimLit);
			}
			if (Stand.Pool)
			{
				// (the pool stays on the ground, and waits until he is down; it is laid again when the eye has turned by
				// more than two degrees)
				const bool bPool = bShown && Stand.Lift < 0.5f;
				if (bPool && (Stand.Pool->GetNumSections() == 0 || FMath::Abs(FRotator::NormalizeAxis(EyeYaw - Stand.PoolYaw)) > 2.0f))
				{
					LayPool(Stand, EyeYaw);
				}
				if (Stand.Pool->IsVisible() != bPool)
				{
					Stand.Pool->SetVisibility(bPool);
				}
				if (bPool && Stand.PoolInk)
				{
					Stand.PoolInk->SetScalarParameterValue(TEXT("Strength"), PoolStrength);
				}
			}
		}
	}
	DrawWatchEdge(CameraDistance);
	KeepEdgeWidth(CameraDistance);
}

void ACampaignMapActor::SetWatchEdge(int32 RegionId)
{
	if (RegionId != WatchRegion)
	{
		const bool bChanged = (RegionId == INDEX_NONE) != (WatchRegion == INDEX_NONE);
		WatchRegion = RegionId;
		WatchDrawnAt = -1.0f; // (drawn afresh at the next look)
		if (bChanged)
		{
			RebuildHighlight(); // (the pale wash of the province pointed at gives way to the edge, and comes back after it)
		}
	}
}

void ACampaignMapActor::DrawWatchEdge(float CameraDistance)
{
	if (!WatchMesh)
	{
		return;
	}
	if (!Data.Regions.IsValidIndex(WatchRegion))
	{
		if (WatchMesh->GetNumSections() > 0)
		{
			WatchMesh->ClearAllMeshSections();
		}
		return;
	}
	// (its line is two pixels wide from any height: it is drawn again when the eye has risen or come down by a twelfth)
	if (WatchDrawnAt > 0.0f && FMath::Abs(CameraDistance - WatchDrawnAt) < 0.085f * WatchDrawnAt)
	{
		return;
	}
	WatchDrawnAt = CameraDistance;
	WatchMesh->ClearAllMeshSections();
	if (!WatchInk && HighlightMaterial)
	{
		WatchInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		WatchInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
	}
	UMaterialInterface* Ink = WatchInk ? static_cast<UMaterialInterface*>(WatchInk) : static_cast<UMaterialInterface*>(HighlightMaterial);
	// (Ruling 163; the art director's brief, section 4.3.) An agent may go anywhere, so no reach is lit. The province
	// under the pointer and its one ring are edged as the ground he would watch from there: one edge round the whole,
	// drawn as one shape with its corners eased, a two-pixel line of the warm light at three quarters strength
	// with a soft band of the same inside it, 30% at the line and gone 25 pixels in; the province he would stand in a
	// shade warmer than its ring, with a faint edge of its own. Borders shared inside the ring are not drawn. It is a
	// sign and not light on the ground: the same by night, and the haze stays under it until he is put down.
	TSet<int32> Watched;
	Watched.Add(WatchRegion);
	for (const int32 Next : Data.Regions[WatchRegion].Neighbors)
	{
		if (Data.Regions.IsValidIndex(Next))
		{
			Watched.Add(Next);
		}
	}
	const FLinearColor WarmLight(FColor(255, 222, 140)); // (the warm light, as built: release 2 only renames it, from the lantern's colour)
	auto Inked = [&WarmLight](float Alpha)
	{
		FLinearColor Out = WarmLight;
		Out.A = Alpha;
		return Out.ToFColor(false);
	};
	{
		FCampaignMeshBuffers Wash;
		for (const int32 Each : Watched)
		{
			AppendRegionFill(Wash, Each, FLinearColor(WarmLight.R, WarmLight.G, WarmLight.B, Each == WatchRegion ? 0.09f : 0.04f));
		}
		Wash.Commit(WatchMesh, 0, Ink);
	}
	const double Pixel = 0.00037 * CameraDistance / UnitScale; // (a pixel of the 1600 view, in the map's own measure)
	const double HalfLine = 1.0 * Pixel, Band = 25.0 * Pixel, Ease = 7.0 * Pixel;
	const FColor LineInk = Inked(0.75f), BandNear = Inked(0.3f), BandFar = Inked(0.0f);
	FCampaignMeshBuffers Edge;
	for (const int32 Each : Watched)
	{
		for (const TArray<FVector2D>& Ring : Data.Regions[Each].Polygons)
		{
			const int32 Count = Ring.Num();
			if (Count < 3)
			{
				continue;
			}
			double Area = 0.0;
			for (int32 K = 0; K < Count; ++K)
			{
				Area += Ring[K].X * Ring[(K + 1) % Count].Y - Ring[(K + 1) % Count].X * Ring[K].Y;
			}
			const double Hand = Area > 0.0 ? 1.0 : -1.0; // (which side of each stretch the province lies on)
			// Each stretch of the outline, in pieces of a unit and a half; a piece is edge where the ground just beyond it is not watched.
			TArray<FVector2D> Inward;
			TArray<double> Long;
			TArray<TArray<bool>> Open;
			for (int32 K = 0; K < Count; ++K)
			{
				const FVector2D A = Ring[K], B = Ring[(K + 1) % Count];
				const double Length = FVector2D::Distance(A, B);
				const FVector2D Along = Length > 1.0e-6 ? (B - A) / Length : FVector2D(1.0, 0.0);
				Inward.Add(FVector2D(-Along.Y, Along.X) * Hand);
				Long.Add(Length);
				TArray<bool>& OpenHere = Open.AddDefaulted_GetRef();
				const int32 Parts = FMath::Max(1, FMath::CeilToInt32(Length / 1.5));
				for (int32 Part = 0; Part < Parts; ++Part)
				{
					const FVector2D Beyond = A + (B - A) * ((Part + 0.5) / Parts) - Inward.Last() * 0.4;
					OpenHere.Add(Length > 0.05 && !Watched.Contains(Data.FindRegionAt(Beyond.X, Beyond.Y)));
				}
			}
			// The line as a run of points, each stretch cut short at a corner both of whose stretches are edge, with a short
			// piece across the corner: so the corners are eased.
			TArray<FVector2D> Line;
			TArray<bool> Drawn; // (whether the piece from this point to the next is edge)
			for (int32 K = 0; K < Count; ++K)
			{
				const int32 Before = (K + Count - 1) % Count, After = (K + 1) % Count;
				const FVector2D A = Ring[K], B = Ring[After];
				const bool bEaseHere = Open[Before].Last() && Open[K][0], bEaseThere = Open[K].Last() && Open[After][0];
				const double CutHere = bEaseHere ? FMath::Min3(Ease, 0.3 * Long[Before], 0.3 * Long[K]) : 0.0;
				const double CutThere = bEaseThere ? FMath::Min3(Ease, 0.3 * Long[K], 0.3 * Long[After]) : 0.0;
				const FVector2D Along = Long[K] > 1.0e-6 ? (B - A) / Long[K] : FVector2D(1.0, 0.0);
				const int32 Parts = Open[K].Num();
				for (int32 Part = 0; Part < Parts; ++Part)
				{
					Line.Add(Part == 0 ? A + Along * CutHere : A + (B - A) * (static_cast<double>(Part) / Parts));
					Drawn.Add(Open[K][Part]);
				}
				if (bEaseThere && CutThere > 1.0e-6)
				{
					Line.Add(B - Along * CutThere);
					Drawn.Add(true); // (the short piece across the corner)
				}
			}
			const int32 Points = Line.Num();
			// Which way the ground lies from each point: between its two pieces, so that the band runs on round a corner unbroken.
			TArray<FVector2D> Lean;
			for (int32 K = 0; K < Points; ++K)
			{
				auto Side = [&](int32 From)
				{
					const FVector2D Step = Line[(From + 1) % Points] - Line[From];
					return (FVector2D(-Step.Y, Step.X) * Hand).GetSafeNormal();
				};
				const int32 Before = (K + Points - 1) % Points;
				const FVector2D One = Side(Before), Two = Side(K);
				FVector2D Mid = Drawn[Before] && Drawn[K] ? (One + Two).GetSafeNormal() : Drawn[K] ? Two : One;
				if (Mid.IsNearlyZero())
				{
					Mid = Two;
				}
				Lean.Add(Mid / FMath::Max(FVector2D::DotProduct(Mid, Two), 0.5));
			}
			for (int32 K = 0; K < Points; ++K)
			{
				if (!Drawn[K])
				{
					continue;
				}
				const int32 After = (K + 1) % Points;
				auto Lay = [&](const FVector2D& Point, const FColor& Colour, double Lifted)
				{
					return Edge.AddVertex(SurfacePointOver(Point.X, Point.Y, Lifted), FVector::UpVector, Colour);
				};
				// (the soft band inside the line)
				{
					const int32 A0 = Lay(Line[K] + Lean[K] * HalfLine, BandNear, 0.2), A1 = Lay(Line[K] + Lean[K] * (HalfLine + Band), BandFar, 0.2);
					const int32 B0 = Lay(Line[After] + Lean[After] * HalfLine, BandNear, 0.2), B1 = Lay(Line[After] + Lean[After] * (HalfLine + Band), BandFar, 0.2);
					Edge.AddTriangleFacingUp(A0, A1, B0);
					Edge.AddTriangleFacingUp(A1, B0, B1);
				}
				// (the line itself)
				{
					const int32 A0 = Lay(Line[K] - Lean[K] * HalfLine, LineInk, 0.26), A1 = Lay(Line[K] + Lean[K] * HalfLine, LineInk, 0.26);
					const int32 B0 = Lay(Line[After] - Lean[After] * HalfLine, LineInk, 0.26), B1 = Lay(Line[After] + Lean[After] * HalfLine, LineInk, 0.26);
					Edge.AddTriangleFacingUp(A0, A1, B0);
					Edge.AddTriangleFacingUp(A1, B0, B1);
				}
			}
		}
	}
	Edge.Commit(WatchMesh, 1, Ink);
	{
		// The province he would stand in: a faint edge of its own, one pixel at 30%.
		FCampaignMeshBuffers Own;
		for (const TArray<FVector2D>& Ring : Data.Regions[WatchRegion].Polygons)
		{
			AppendRibbon(Own, Ring, 0.5 * Pixel, 0.22, Inked(0.3f), true);
		}
		Own.Commit(WatchMesh, 2, Ink);
	}
	WatchMesh->SetCastShadow(false);
	WatchMesh->SetTranslucentSortPriority(1); // (over the ground's washes; under the table's signs)
}

void ACampaignMapActor::SetHoveredRegion(int32 RegionId)
{
	THEATER_SPEED_SCOPE(SetHoveredRegion);
	if (RegionId != HoveredRegion)
	{
		HoveredRegion = RegionId;
		RebuildHighlight();
		if (CrowdLead.Num() > 0)
		{
			ApplyCrowds(); // (the crowded province that is pointed at fans out; the one left folds up)
			BuildGunLines();
		}
	}
}

void ACampaignMapActor::RebuildHighlight()
{
	THEATER_SPEED_SCOPE(RebuildHighlight);
	HighlightMesh->ClearAllMeshSections();
	// (By moonlight both are taken down: this paint is not lit by the room, so by night the white edge round the
	// province under the pointer stood at grey 223 on ground at 60, brighter than a town on fire. The art director's
	// question of 18 November 2026 about the white bands on the coast at Vancouver.)
	const float Moon = FMath::Clamp(GetMoonlight(), 0.0f, 1.0f);
	HighlightMoon = Moon;
	auto ByMoon = [Moon](const FLinearColor& Day, const FLinearColor& Night) { return FMath::Lerp(Day, Night, Moon); };
	// (The mesh's colours reach this paint as plain light and it brightens them 1.6 times, so the kit's colours, which
	// are screen colours, are turned to light and taken down by that much first; by moonlight they are dimmer. Both
	// edges are drawn in pixels: KeepEdgeWidth draws them again as the eye rises or comes down.)
	auto KitInk = [](const FColor& Screen, float Share, float Opacity)
	{
		FLinearColor Light = FLinearColor(Screen) * (Share / 1.6f);
		Light.A = Opacity;
		return Light;
	};
	const FColor Brass300(224, 184, 114), Paper100(237, 230, 214), Desk950(14, 11, 8);
	const double Pixel = 0.00037 * (EdgeWidthAt > 0.0f ? EdgeWidthAt : 4000.0f) / UnitScale;
	const FLinearColor UnderStroke = ByMoon(KitInk(Desk950, 1.0f, 0.7f), KitInk(Desk950, 1.0f, 0.55f));
	// (with an agent in hand the province pointed at wears the edge of the ground he would watch, and not this pale wash)
	// (Release 2: drawn first, in sections 0 and 1, so that on the side it shares with the picked province its edge
	// lies under the picked one's brass line and does not cover it: one mesh draws its sections in order.)
	// (Release 2, the art director's read of run 3, 22 November: the edge under the pointer was about six pixels of
	// near white, the loudest line on the map whenever the pointer moved, louder than the pick. It is now paper-100, two
	// pixels at six tenths, over the picked edge's own dark under-stroke: quieter than the pick, and the same make.)
	if (HoveredRegion != INDEX_NONE && HoveredRegion != SelectedRegion && WatchRegion == INDEX_NONE)
	{
		AddRegionOverlay(HoveredRegion, ByMoon(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), FLinearColor(0.5f, 0.56f, 0.68f, 0.07f)),
			ByMoon(KitInk(Paper100, 1.0f, 0.6f), KitInk(Paper100, 0.6f, 0.5f)), 0,
			1.0 * Pixel, UnderStroke, 2.0 * Pixel);
	}
	if (SelectedRegion != INDEX_NONE)
	{
		// (a light wash and a bright edge: the ground keeps the colour of whoever holds it while it is picked)
		// (Release 2, the art director's read of run 2, 22 November: mend 9. The edge was a thick line of near white, the
		// loudest line on the map. It is now brass-300, two pixels wide from any height, over a dark under-stroke a pixel
		// wider each side, as the town plate's lit edge is.)
		AddRegionOverlay(SelectedRegion, ByMoon(FLinearColor(1.0f, 0.82f, 0.42f, 0.11f), FLinearColor(0.62f, 0.56f, 0.4f, 0.07f)),
			ByMoon(KitInk(Brass300, 1.0f, 1.0f), KitInk(Brass300, 0.7f, 0.9f)), 2,
			1.0 * Pixel, UnderStroke, 2.0 * Pixel);
	}
}

void ACampaignMapActor::KeepEdgeWidth(float CameraDistance)
{
	// (the edges of the picked province and of the one under the pointer are two pixels wide from any height: drawn
	// again when the eye has risen or come down by a twelfth)
	if ((SelectedRegion == INDEX_NONE && HoveredRegion == INDEX_NONE) || !HighlightMesh || CameraDistance <= 0.0f
		|| (EdgeWidthAt > 0.0f && FMath::Abs(CameraDistance - EdgeWidthAt) < 0.085f * EdgeWidthAt))
	{
		return;
	}
	EdgeWidthAt = CameraDistance;
	RebuildHighlight();
}

void ACampaignMapActor::AppendRegionFill(FCampaignMeshBuffers& Buffers, int32 RegionId, const FLinearColor& Fill) const
{
	if (!Data.Regions.IsValidIndex(RegionId))
	{
		return;
	}
	// A glow over the region's ground: the terrain's own triangles, lifted a hair, fading out at the region's edge.
	const FCampaignRegion& Region = Data.Regions[RegionId];
	const double Cell = Terrain.Cell;
	const int32 I0 = FMath::Clamp(FMath::FloorToInt32(Region.Bounds.Min.X / Cell) - 2, 0, Terrain.NX - 2);
	const int32 I1 = FMath::Clamp(FMath::CeilToInt32(Region.Bounds.Max.X / Cell) + 2, 0, Terrain.NX - 2);
	const int32 J0 = FMath::Clamp(FMath::FloorToInt32(Region.Bounds.Min.Y / Cell) - 2, 0, Terrain.NY - 2);
	const int32 J1 = FMath::Clamp(FMath::CeilToInt32(Region.Bounds.Max.Y / Cell) + 2, 0, Terrain.NY - 2);
	const int32 Stride = I1 - I0 + 2;
	TArray<int32> Remap;
	Remap.Init(INDEX_NONE, Stride * (J1 - J0 + 2));

	auto Mine = [&](int32 I, int32 J) { return Terrain.Region[Terrain.Index(I, J)] == RegionId; };
	const bool bLandOnly = Data.bWorld && Data.FirstBeyond != INDEX_NONE && RegionId >= Data.FirstBeyond;
	auto VertexAt = [&](int32 I, int32 J)
	{
		int32& Slot = Remap[(J - J0) * Stride + (I - I0)];
		if (Slot == INDEX_NONE)
		{
			FLinearColor C = Fill;
			const double X = FMath::Min(I * Cell, Data.Width), Y = FMath::Min(J * Cell, Data.Height);
			// (Beyond Europe the chart gives each province squares of the sea off its coast, now one's and now its
			// neighbour's: a wash lay on them as a band of chequers in the water. There it lies on the land only.)
			C.A = Mine(I, J) && (!bLandOnly || Terrain.CoverageAt(X, Y) >= 0.35f) ? Fill.A : 0.0f;
			Slot = Buffers.AddVertex(SurfacePoint(X, Y, 0.16), FVector::UpVector, C.ToFColor(false));
		}
		return Slot;
	};
	for (int32 J = J0; J <= J1; ++J)
	{
		for (int32 I = I0; I <= I1; ++I)
		{
			if (!(Mine(I, J) || Mine(I + 1, J) || Mine(I, J + 1) || Mine(I + 1, J + 1)))
			{
				continue;
			}
			const int32 A = VertexAt(I, J), B = VertexAt(I + 1, J), C = VertexAt(I, J + 1), D = VertexAt(I + 1, J + 1);
			Buffers.Triangles.Append({ A, C, B, B, C, D });
		}
	}
}

void ACampaignMapActor::AppendRibbon(FCampaignMeshBuffers& Buffers, const TArray<FVector2D>& Points, double HalfWidth, double Lift, const FColor& Ink, bool bClosed) const
{
	const int32 Count = Points.Num();
	const int32 Edges = bClosed ? Count : Count - 1;
	for (int32 i = 0; i < Edges; ++i)
	{
		const FVector2D A = Points[i];
		const FVector2D B = Points[(i + 1) % Count];
		const double Length = FVector2D::Distance(A, B);
		if (Length < 0.05)
		{
			continue;
		}
		const FVector2D Dir = (B - A) / Length;
		const FVector2D Side(Dir.Y, -Dir.X);
		// Laid over the relief a short step at a time; the ends run on a little so that consecutive pieces meet at corners.
		const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Length / 1.4));
		int32 PrevLeft = INDEX_NONE, PrevRight = INDEX_NONE;
		for (int32 k = 0; k <= Steps; ++k)
		{
			const double T = k == 0 ? -HalfWidth * 0.6 : (k == Steps ? Length + HalfWidth * 0.6 : Length * k / Steps);
			const FVector2D P = A + Dir * T;
			const FVector2D L = P + Side * HalfWidth;
			const FVector2D R = P - Side * HalfWidth;
			const int32 Left = Buffers.AddVertex(SurfacePointOver(L.X, L.Y, Lift), FVector::UpVector, Ink);
			const int32 Right = Buffers.AddVertex(SurfacePointOver(R.X, R.Y, Lift), FVector::UpVector, Ink);
			if (PrevLeft != INDEX_NONE)
			{
				Buffers.AddTriangleFacingUp(PrevLeft, PrevRight, Left);
				Buffers.AddTriangleFacingUp(PrevRight, Left, Right);
			}
			PrevLeft = Left;
			PrevRight = Right;
		}
	}
}

void ACampaignMapActor::AddRegionOverlay(int32 RegionId, const FLinearColor& Fill, const FLinearColor& Outline, int32 FirstSection, double HalfLine, const FLinearColor& Under, double UnderHalf)
{
	if (!Data.Regions.IsValidIndex(RegionId))
	{
		return;
	}
	{
		FCampaignMeshBuffers Buffers;
		AppendRegionFill(Buffers, RegionId, Fill);
		Buffers.Commit(HighlightMesh, FirstSection, HighlightMaterial);
	}
	{
		// A bright line round its outline.
		// (Beyond Europe a province's coast is traced along the squares of the chart it was read from, and the line
		// round it climbed the coast in steps. Each corner there is cut once, which turns a step into a slope; Europe's
		// own outlines were drawn by hand and are left as they are.)
		// (Where an under-stroke is asked for, it is laid first in the same section, a little wider, and the line over
		// it: one draw, in that order, so the dark is always under the line.)
		FCampaignMeshBuffers Buffers;
		const bool bRound = Data.bWorld && Data.FirstBeyond != INDEX_NONE && RegionId >= Data.FirstBeyond;
		TArray<FVector2D> Rounded;
		const bool bUnder = UnderHalf > 0.0 && Under.A > 0.0f;
		for (int32 Pass = bUnder ? 0 : 1; Pass < 2; ++Pass)
		{
			const double Half = Pass == 0 ? UnderHalf : HalfLine;
			const FColor Ink = (Pass == 0 ? Under : Outline).ToFColor(false);
			for (const TArray<FVector2D>& Ring : Data.Regions[RegionId].Polygons)
			{
				if (bRound && Ring.Num() >= 6)
				{
					Rounded.Reset(Ring.Num() * 2);
					for (int32 K = 0; K < Ring.Num(); ++K)
					{
						const FVector2D A = Ring[K], B = Ring[(K + 1) % Ring.Num()];
						Rounded.Add(A * 0.75 + B * 0.25);
						Rounded.Add(A * 0.25 + B * 0.75);
					}
					AppendRibbon(Buffers, Rounded, Half, 0.24, Ink, true);
					continue;
				}
				AppendRibbon(Buffers, Ring, Half, 0.24, Ink, true);
			}
		}
		Buffers.Commit(HighlightMesh, FirstSection + 1, HighlightMaterial);
	}
}
