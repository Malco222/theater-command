#include "Campaign/CampaignMapActor.h"
#include "Async/ParallelFor.h"
#include "EngineUtils.h"
#include "Campaign/CampaignEnvironment.h"

#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignFlags.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignTextures.h"
#include "TheaterCommand.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
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
		return true;
	}
};

namespace
{
	FLinearColor FromHex(const TCHAR* Hex)
	{
		return FLinearColor(FColor::FromHex(Hex)); // sRGB in, linear out
	}

	const TCHAR* SurfaceMaterialPath = TEXT("/Game/Campaign/Materials/M_MapSurface.M_MapSurface");
	const TCHAR* SolidMaterialPath = TEXT("/Game/Campaign/Materials/M_Solid.M_Solid");
	const TCHAR* HighlightMaterialPath = TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight");

	// Lettering tiers.
	constexpr int32 TierAlways = 0;
	constexpr int32 TierFar = 1;
	constexpr int32 TierNear = 2;
	constexpr int32 TierMiddle = 3; // from the middle distance in: the places worth having, and what each is worth
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
	TerrainMesh = MakeMesh(TEXT("Terrain"), true);
	SeaMesh = MakeMesh(TEXT("Sea"), false);
	BorderMesh = MakeMesh(TEXT("Borders"), false);
	RiverMesh = MakeMesh(TEXT("Rivers"), false);
	RailMesh = MakeMesh(TEXT("Railways"), false);
	WorksMesh = MakeMesh(TEXT("Works"), false);
	BuildingsMesh = MakeMesh(TEXT("Buildings"), false);
	RoomMesh = MakeMesh(TEXT("WarRoom"), false);
	CloudMesh = MakeMesh(TEXT("Clouds"), false);
	SmokeMesh = MakeMesh(TEXT("Smoke"), false);
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
		Camera->bAlwaysPersistRenderingState = bEveryFrame;
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
	BuildBorders();
	BuildTable();
	BuildWarRoom();
	BuildTowns();
	BuildParade();
	BuildTrees();
	BuildLabels();

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

FVector ACampaignMapActor::SurfacePoint(double X, double Y, double Lift) const
{
	const double Ground = FMath::Max(static_cast<double>(Terrain.HeightAt(X, Y)), 0.0);
	return MapToWorld(X, Y, Ground + Lift);
}

FVector ACampaignMapActor::SurfacePointOver(double X, double Y, double Lift) const
{
	const double Reach = 0.8;
	double Ground = FMath::Max(static_cast<double>(Terrain.HeightAt(X, Y)), 0.0);
	for (const FVector2D& Step : { FVector2D(Reach, 0.0), FVector2D(-Reach, 0.0), FVector2D(0.0, Reach), FVector2D(0.0, -Reach) })
	{
		Ground = FMath::Max(Ground, static_cast<double>(Terrain.HeightAt(X + Step.X, Y + Step.Y)));
	}
	return MapToWorld(X, Y, Ground + Lift);
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
	// The pieces: painted metal and cloth, not bare plastic. A fine grain in the paint and a slight tooth to the
	// surface catch the lamp, at a scale that suits things the size of a model.
	PieceMaterial = MakeSurface(0.56f, 0.4f, 0.1f, 0.3f);
	PieceMaterial->SetScalarParameterValue(TEXT("GrainTilingCoarse"), 0.012f);
	PieceMaterial->SetScalarParameterValue(TEXT("GrainTilingFine"), 0.06f);
	PieceMaterial->SetScalarParameterValue(TEXT("ToothTiling"), 0.09f);
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")))
	{
		PieceMaterial = MakeSurface(0.5f, 0.38f, 0.0f, 0.0f); // as it was: smooth and a little shiny, like moulded plastic
	}
	SceneryMaterial = MakeSurface(0.85f, 0.2f, 0.0f, 0.0f);
	WoodsMaterial = MakeSurface(0.85f, 0.2f, 0.0f, 0.0f);

	// The land and the sea take their colour from pictures painted in PaintGround.
	LandMaterial = UMaterialInstanceDynamic::Create(Load(TEXT("/Game/Campaign/Materials/M_Land.M_Land")), this);
	SeaMaterial = UMaterialInstanceDynamic::Create(Load(TEXT("/Game/Campaign/Materials/M_Sea.M_Sea")), this);
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

UMaterialInterface* ACampaignMapActor::GetFlagMaterial(const FString& Tag)
{
	if (!FlagMaterial)
	{
		return nullptr;
	}
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = FlagMaterials.Find(Tag))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(FlagMaterial, this);
	Instance->SetTextureParameterValue(TEXT("Flag"), GetFlagTexture(Tag));
	Instance->SetScalarParameterValue(TEXT("Glow"), 0.5f * (1.0f - 0.4f * GetMoonlight()));
	FlagMaterials.Add(Tag, Instance);
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
	CoverPicture = CampaignTextures::Create(Paint.NX, Paint.NY, Paint.Cover, false, false, TEXT("TableCover"));
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
	FieldsPicture = CampaignTextures::Create(CampaignTextures::FieldsSize, CampaignTextures::FieldsSize, Pixels, false, true, TEXT("TableFields"));
	if (FieldsPicture)
	{
		FieldsPicture->Filter = TF_Nearest;
		FieldsPicture->UpdateResource();
	}
	CampaignTextures::MakeWaterBumps(Pixels);
	RipplesPicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableRipples"));

	LandMaterial->SetTextureParameterValue(TEXT("Ground"), GroundPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Tint"), TintPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Relief"), ReliefPicture);
	LandMaterial->SetScalarParameterValue(TEXT("Band"), 0.4f);
	LandMaterial->SetTextureParameterValue(TEXT("Masks"), MasksPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Cover"), CoverPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Fields"), FieldsPicture);
	// (the nation's stain lies over the canopy at a little under half its strength: a wood is green first, and its owner's second)
	LandMaterial->SetScalarParameterValue(TEXT("WoodsStain"), 0.42f);
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
	{
		// "-TheaterSheen=0.1" tries another strength for the sheen of the painted land.
		float Sheen = 0.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSheen="), Sheen))
		{
			LandMaterial->SetScalarParameterValue(TEXT("Specular"), Sheen);
		}
	}
	LandMaterial->SetVectorParameterValue(TEXT("RockColour"), FCampaignGround::RockColour());
	LandMaterial->SetVectorParameterValue(TEXT("SnowColour"), FCampaignGround::SnowColour() * 0.86f); // (the baked sun lifts it to white where it is lit)
	LandMaterial->SetVectorParameterValue(TEXT("SandColour"), FCampaignGround::SandColour());
	SeaMaterial->SetTextureParameterValue(TEXT("Water"), WaterPicture);
	SeaMaterial->SetTextureParameterValue(TEXT("Shore"), ShorePicture);
	SeaMaterial->SetTextureParameterValue(TEXT("Ripples"), RipplesPicture);

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
	LandTiles.Reset();

	// The heights at every level. Each coarser level keeps every second point of the one below, each the
	// average of itself and its neighbours there, so that far-off ground is smooth and not a jagged sample.
	// Alongside, the most land near each point, so that a tile that is all open sea can be left out.
	LandHeights.Reset();
	LandCover.Reset();
	LandSizes.Reset();
	LandHeights.Add(Terrain.Height);
	LandCover.Add(Terrain.Coverage);
	LandSizes.Add(FIntPoint(Terrain.NX, Terrain.NY));
	const double Longest = FMath::Max(Data.Width, Data.Height);
	LandTopLevel = 0;
	while (TileQuads * Terrain.Cell * (1 << LandTopLevel) < Longest * 0.5 && LandTopLevel < 12)
	{
		++LandTopLevel;
	}
	for (int32 Level = 1; Level <= LandTopLevel; ++Level)
	{
		const FIntPoint Fine = LandSizes[Level - 1];
		const FIntPoint Coarse((Fine.X - 1) / 2 + 1, (Fine.Y - 1) / 2 + 1);
		const TArray<float>& FineHeights = LandHeights[Level - 1];
		const TArray<float>& FineCover = LandCover[Level - 1];
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
		LandHeights.Add(MoveTemp(Heights));
		LandCover.Add(MoveTemp(Cover));
		LandSizes.Add(Coarse);
	}
	UE_LOG(LogTheater, Log, TEXT("Land: %d levels of tiles, the finest %.0f map units across and the coarsest %.0f"),
		LandTopLevel + 1, TileQuads * Terrain.Cell, TileQuads * Terrain.Cell * (1 << LandTopLevel));

	// Stand the table's first tiles up at once, for the view the game opens on.
	UpdateLand(12000.0f, FVector::ZeroVector, true);
}

bool ACampaignMapActor::LandTileHasLand(int32 Level, int32 TileX, int32 TileY) const
{
	const FIntPoint Size = LandSizes[Level];
	const TArray<float>& Cover = LandCover[Level];
	const int32 I0 = TileX * TileQuads, J0 = TileY * TileQuads;
	if (I0 >= Size.X - 1 || J0 >= Size.Y - 1)
	{
		return false; // off the edge of the map
	}
	const int32 I1 = FMath::Min(I0 + TileQuads, Size.X - 1), J1 = FMath::Min(J0 + TileQuads, Size.Y - 1);
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

UProceduralMeshComponent* ACampaignMapActor::BuildLandTile(int32 Level, int32 TileX, int32 TileY)
{
	const FIntPoint Size = LandSizes[Level];
	const TArray<float>& Heights = LandHeights[Level];
	const TArray<float>& Cover = LandCover[Level];
	const int32 Step = 1 << Level; // how many of the finest grid's points one quad of this tile spans
	const int32 I0 = TileX * TileQuads, J0 = TileY * TileQuads;
	const int32 I1 = FMath::Min(I0 + TileQuads, Size.X - 1), J1 = FMath::Min(J0 + TileQuads, Size.Y - 1);
	const int32 Stride = I1 - I0 + 1;

	FCampaignMeshBuffers Buffers;
	TArray<int32> Remap;
	Remap.Init(INDEX_NONE, Stride * (J1 - J0 + 1));
	auto PointAt = [&](int32 I, int32 J, double Drop)
	{
		const double X = FMath::Min(I * Step * Terrain.Cell, Data.Width), Y = FMath::Min(J * Step * Terrain.Cell, Data.Height);
		// The paint has one pixel for every point of the finest grid: this point's is at the middle of its pixel.
		const FVector2D UV((FMath::Min(I * Step, Terrain.NX - 1) + 0.5) / Terrain.NX, (FMath::Min(J * Step, Terrain.NY - 1) + 0.5) / Terrain.NY);
		return Buffers.AddVertex(MapToWorld(X, Y, Heights[J * Size.X + I] - Drop), FVector::UpVector, FColor::White, UV);
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
		// Sea bed well away from any shore is never seen: leave it out.
		return FMath::Max(FMath::Max(Cover[J * Size.X + I], Cover[J * Size.X + I + 1]), FMath::Max(Cover[(J + 1) * Size.X + I], Cover[(J + 1) * Size.X + I + 1])) >= 0.3f;
	};
	// A skirt hangs from the tile's edge, so that where a fine tile meets a coarse one no daylight shows between them.
	const double SkirtDrop = 0.9 * Step * Terrain.Cell;
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
		Mesh = NewObject<UProceduralMeshComponent>(this);
		Mesh->SetupAttachment(Root);
		Mesh->bUseAsyncCooking = false;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(bLandShadows);
		Mesh->RegisterComponent();
		LandMeshes.Add(Mesh);
	}
	Buffers.Commit(Mesh, 0, LandMaterial);
	Mesh->SetVisibility(true);
	return Mesh;
}

void ACampaignMapActor::UpdateLand(float CameraDistance, const FVector& CameraFocus, bool bAllAtOnce)
{
	if (LandHeights.Num() == 0)
	{
		return;
	}
	// Which tiles the view wants: starting from the coarsest, a tile is cut into its four quarters for as long
	// as it is large for how far off it is. "How far off" is the eye's distance from what it looks at, and the
	// tile's distance from that point along the table.
	const double TileUnits = TileQuads * Terrain.Cell * UnitScale; // the side of a finest tile, in the world
	const FVector2D Focus(CameraFocus.X, CameraFocus.Y);
	TArray<uint64> Wanted;
	TArray<FIntVector> Stack;
	const int32 TopSide = 1 << LandTopLevel;
	const int32 RootsX = FMath::DivideAndRoundUp(FMath::DivideAndRoundUp(Terrain.NX - 1, TileQuads), TopSide);
	const int32 RootsY = FMath::DivideAndRoundUp(FMath::DivideAndRoundUp(Terrain.NY - 1, TileQuads), TopSide);
	for (int32 Y = 0; Y < RootsY; ++Y)
	{
		for (int32 X = 0; X < RootsX; ++X)
		{
			Stack.Add(FIntVector(X, Y, LandTopLevel));
		}
	}
	while (Stack.Num() > 0)
	{
		const FIntVector Tile = Stack.Pop();
		const int32 Level = Tile.Z;
		if (!LandTileHasLand(Level, Tile.X, Tile.Y))
		{
			continue;
		}
		const double Side = TileUnits * (1 << Level);
		// The tile's box on the table, in the world: north is +X and east is +Y, and the map's corner is its north-west.
		const FVector NorthWest = MapToWorld(Tile.X * Side / UnitScale, Tile.Y * Side / UnitScale);
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
		}
	}

	// Stand up the tiles that are wanted and not there yet: all of them when asked, otherwise a few each frame,
	// so that sweeping across the table never stalls.
	int32 Budget = bAllAtOnce ? MAX_int32 : 5;
	TSet<uint64> Missing;
	for (const uint64 Key : Wanted)
	{
		if (LandTiles.Contains(Key))
		{
			continue;
		}
		if (Budget > 0)
		{
			--Budget;
			const int32 Level = static_cast<int32>(Key >> 48), TileX = static_cast<int32>((Key >> 24) & 0xFFFFFF), TileY = static_cast<int32>(Key & 0xFFFFFF);
			LandTiles.Add(Key, BuildLandTile(Level, TileX, TileY)); // null for a tile with nothing in it: remembered, so it is not tried again
		}
		else
		{
			Missing.Add(Key);
		}
	}

	// Take away the tiles that are no longer wanted, but only once whatever replaces them is standing: a tile
	// stays for as long as any wanted tile over the same ground (finer or coarser) is still to come.
	const TSet<uint64> WantedSet(Wanted);
	for (auto It = LandTiles.CreateIterator(); It; ++It)
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
				const int32 Other = Data.FindRegionAt(Probe.X, Probe.Y);
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
					const int32 Left = Into.AddVertex(SurfacePointOver(L.X, L.Y, Lift), FVector::UpVector, Ink);
					const int32 Right = Into.AddVertex(SurfacePointOver(R.X, R.Y, Lift), FVector::UpVector, Ink);
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
	Buffers.Commit(BorderMesh, 0, InkMaterial);
	Faint.Commit(BorderMesh, 1, ProvinceInk);
	FaintPale.Commit(BorderMesh, 2, ProvincePaleInk);
	UE_LOG(LogTheater, Log, TEXT("Borders: %d frontier edges, %d region edges"), NationalCount, RegionCount);
}

void ACampaignMapActor::SetLand(const FLandFeatures& InLand)
{
	Land = InLand;
	bLandLaid = true;
	bWorksShown = false;
	BuildWaterways();
	BuildRailways();
	BuildWorks();
}

void ACampaignMapActor::ShowBuildings(const TArray<FIntVector4>& Works)
{
	if (!BuildingsMesh || !Data.IsLoaded() || Works == ShownBuildings)
	{
		return;
	}
	ShownBuildings = Works;
	BuildingsMesh->ClearAllMeshSections();
	WorksChimneys.Reset();
	AirfieldSpots.Reset();

	const FLinearColor Brick(FColor::FromHex(TEXT("7A5547"))), Soot(FColor::FromHex(TEXT("3B3532"))), Pane(FColor::FromHex(TEXT("8FA6A8")));
	const FLinearColor Apron(FColor::FromHex(TEXT("A9A69C"))), Girder(FColor::FromHex(TEXT("4A4F55"))), RedLead(FColor::FromHex(TEXT("8A3B2A")));
	const FLinearColor Sandbag(FColor::FromHex(TEXT("B9A77C"))), GunPaint(FColor::FromHex(TEXT("3E4640"))), Tin(FColor::FromHex(TEXT("7E8A84")));
	const FLinearColor Turf(FColor::FromHex(TEXT("6E7F4E"))), Orange(FColor::FromHex(TEXT("D9772B"))), Timber(FColor::FromHex(TEXT("6B5236")));

	FMiniatureMesh Model;
	const double Unit = UnitScale * 2.1; // the size the towns are built to
	int32 Count = 0;
	for (int32 Id = 0; Id < Works.Num() && Id < Data.Regions.Num(); ++Id)
	{
		const FIntVector4& Built = Works[Id];
		if (Built.X <= 0 && Built.Y <= 0 && Built.Z <= 0 && Built.W <= 0)
		{
			continue;
		}
		const FCampaignRegion& Region = Data.Regions[Id];
		const FVector2D Middle = Region.Center;
		// Where things already stand round a town: the army's flag to the north-east (or north-west), the air
		// fleets to the south-west and south-east. The works take the gaps: north, south, west and east.
		auto Place = [this, &Middle, Id](const FVector2D& Offset, FVector2D& OutSpot) -> bool
		{
			for (const double Share : { 1.0, 0.75, 0.55 })
			{
				OutSpot = Middle + Offset * Share;
				if (Data.FindRegionAt(OutSpot.X, OutSpot.Y) == Id)
				{
					return true;
				}
			}
			return false; // no room in this province: leave it off the table
		};
		// A shipyard wants the water in front of it: the nearest stretch of the province's own coast that is clear of the pieces.
		auto PlaceOnCoast = [this, &Middle, Id](FVector2D& OutSpot) -> bool
		{
			static const FVector2D Taken[] = { FVector2D(9.0, -7.0), FVector2D(-9.0, -7.0), FVector2D(-10.0, 8.0), FVector2D(10.0, 9.0), FVector2D(0.0, -9.0), FVector2D(-10.0, 13.0), FVector2D(-9.5, 0.5) };
			for (double Reach = 6.0; Reach <= 34.0; Reach += 1.5)
			{
				for (int32 Step = 0; Step < 24; ++Step)
				{
					const FVector2D Way(FMath::Cos(Step * PI / 12.0), FMath::Sin(Step * PI / 12.0));
					const FVector2D Offset = Way * Reach;
					const FVector2D Spot = Middle + Offset, Beyond = Middle + Way * (Reach + 3.5);
					if (Data.FindRegionAt(Spot.X, Spot.Y) != Id || Terrain.HeightAt(Spot.X, Spot.Y) <= 0.0f
						|| Data.FindRegionAt(Beyond.X, Beyond.Y) != INDEX_NONE || Terrain.HeightAt(Beyond.X, Beyond.Y) > 0.0f)
					{
						continue;
					}
					bool bClear = true;
					for (const FVector2D& Other : Taken)
					{
						bClear &= FVector2D::Distance(Offset, Other) > 6.0;
					}
					if (bClear)
					{
						OutSpot = Spot;
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
		if (Built.Z > 0 && Place(FVector2D(-9.5, 0.5), Spot))
		{
			const FVector At = SurfacePoint(Spot.X, Spot.Y, 0.02);
			const double Turn = Facing(Spot) + 90.0; // the pits in a row running out from the town
			const int32 Guns = FMath::Clamp(Built.Z, 1, 4);
			for (int32 Pit = 0; Pit < Guns; ++Pit)
			{
				const FVector Foot = At + Ahead(Turn, 0.0, (Pit - (Guns - 1) * 0.5) * 0.7 * Unit);
				Model.Pillar(Foot, 0.3 * Unit, 0.27 * Unit, 0.13 * Unit, Sandbag, 10);
				Model.Pillar(Foot + FVector(0.0, 0.0, 0.13 * Unit), 0.2 * Unit, 0.2 * Unit, 0.01 * Unit, Soot, 10);
				Model.Pillar(Foot + FVector(0.0, 0.0, 0.1 * Unit), 0.07 * Unit, 0.05 * Unit, 0.2 * Unit, GunPaint, 6);
				const FVector Breech = Foot + FVector(0.0, 0.0, 0.3 * Unit);
				const FVector Up = (Ahead(Turn + Pit * 37.0, 0.45, 0.0) + FVector(0.0, 0.0, 0.89)).GetSafeNormal();
				Model.Rod(Breech - Up * 0.08 * Unit, Breech + Up * 0.55 * Unit, 0.028 * Unit, GunPaint);
				Model.Box(Breech, FVector(0.16, 0.12, 0.1) * Unit, GunPaint, Turn + Pit * 37.0);
			}
			++Count;
		}
		// An airfield, by its size: a landing ground is a mown strip with a hut and a windsock; an airfield has a
		// hard runway and hangars; an air base has crossing runways, a row of hangars and a control tower; and
		// the greatest has three runways in a triangle.
		if (Built.W > 0 && Place(FVector2D(-10.0, 13.0), Spot))
		{
			AirfieldSpots.Add(Id, Spot);
			const double Field = Unit * 1.3; // an airfield is a big place, and the player has to find it
			const int32 Size = FMath::Clamp(Built.W, 1, 4);
			const FVector At = SurfacePoint(Spot.X, Spot.Y, 0.02);
			const double Turn = Facing(Spot) + 90.0;
			const double Long = 2.9 + 0.45 * Size, Wide = 1.5 + 0.3 * Size;
			// The field is a slab thick enough to stand clear of the roll of the ground under it.
			Model.Box(At + FVector(0.0, 0.0, 0.0), FVector(Long, Wide, 0.16) * Field, Turf, Turn);
			const FLinearColor Strip = Size >= 2 ? Apron : FLinearColor(FColor::FromHex(TEXT("8E9A62")));
			Model.Box(At + FVector(0.0, 0.0, 0.085 * Field), FVector(Long - 0.25, 0.3, 0.02) * Field, Strip, Turn);
			if (Size >= 3)
			{
				Model.Box(At + FVector(0.0, 0.0, 0.088 * Field), FVector(Wide * 1.5, 0.26, 0.02) * Field, Apron, Turn + 58.0);
			}
			if (Size >= 4)
			{
				Model.Box(At + FVector(0.0, 0.0, 0.091 * Field), FVector(Wide * 1.5, 0.26, 0.02) * Field, Apron, Turn - 58.0);
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
	UE_LOG(LogTheater, Log, TEXT("Works on the table: %d, in %d points"), Count, Model.Vertices.Num());
}

void ACampaignMapActor::ShowWorks(const TArray<int32>& BridgesDown, const TArray<int32>& RailsCut)
{
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
	// A river is a ribbon of water set into the land along its course: narrow at the source, broader at the
	// mouth (a great river about a map unit and a quarter across, the rest under one), its course rounded and
	// given a gentle wander so that it winds, blue-green, with a dark bank on either side.
	// (the paint is laid on darker than it is meant to look, to suit the lamp, as the land's is)
	const FLinearColor MidstreamRuns = FLinearColor(FColor(72, 104, 125)), BankRuns = FLinearColor(FColor(60, 88, 106));
	const FColor Earth = FColor(26, 38, 36);
	static const TSet<FString> Great = { TEXT("Rhine"), TEXT("Danube"), TEXT("Vistula"), TEXT("Dnieper"), TEXT("Volga"), TEXT("Don"), TEXT("Elbe"), TEXT("Oder"), TEXT("Loire"), TEXT("Seine"),
		TEXT("Rhone"), TEXT("Po"), TEXT("Nile"), TEXT("Dvina"), TEXT("Niemen"), TEXT("Bug"), TEXT("Dniester") };
	// (in the depth of winter the rivers freeze where the snow lies: ice, pale and dull, with a darker lead midstream)
	const FLinearColor MidstreamIce = FLinearColor(FColor(140, 162, 172)), BankIce = FLinearColor(FColor(186, 196, 199));
	RiversDrawnFor = WinterHard;
	const double Lift = 0.1;
	FCampaignMeshBuffers Buffers;
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
				Path.Add(0.5 * ((2.0 * P1) + (-P0 + P2) * T + (2.0 * P0 - 5.0 * P1 + 4.0 * P2 - P3) * T * T + (-P0 + 3.0 * P1 - 3.0 * P2 + P3) * T * T * T));
			}
		}
		Path.Add(River.Course.Last());
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
		const double Broad = Great.Contains(River.Name) ? 0.625 : 0.425;
		int32 Previous[5] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
		for (int32 K = 0; K < Path.Num(); ++K)
		{
			const FVector2D Along = (Path[FMath::Min(K + 1, Path.Num() - 1)] - Path[FMath::Max(K - 1, 0)]).GetSafeNormal();
			const FVector2D Side(Along.Y, -Along.X);
			const double Half = Broad * FMath::Lerp(0.5, 1.0, FMath::Sqrt(static_cast<double>(K) / FMath::Max(Path.Num() - 1, 1)));
			const double Shore = Half + 0.2; // the dark of the bank beside the water
			// A river does not run over the sea: stop where the land does.
			if (Terrain.HeightAt(Path[K].X, Path[K].Y) <= 0.0f && K > Path.Num() / 2)
			{
				break;
			}
			const float Frozen = WinterHard >= 0.8f ? FMath::SmoothStep(0.45f, 0.8f, SnowLying(Path[K], WinterHard)) : 0.0f;
			const FColor Midstream = FMath::Lerp(MidstreamRuns, MidstreamIce, Frozen).ToFColor(true), Bank = FMath::Lerp(BankRuns, BankIce, Frozen).ToFColor(true);
			const FVector2D Across[5] = { Path[K] + Side * Shore, Path[K] + Side * Half, Path[K], Path[K] - Side * Half, Path[K] - Side * Shore };
			const FColor Inks[5] = { Earth, Bank, Midstream, Bank, Earth };
			int32 Here[5];
			for (int32 P = 0; P < 5; ++P)
			{
				// (the banks a hair lower than the water between them, so the water is never hidden by its bank)
				Here[P] = Buffers.AddVertex(SurfacePointOver(Across[P].X, Across[P].Y, P == 0 || P == 4 ? Lift - 0.03 : Lift), FVector::UpVector, Inks[P]);
			}
			if (Previous[0] != INDEX_NONE)
			{
				for (int32 Strip = 0; Strip < 4; ++Strip)
				{
					Buffers.AddTriangleFacingUp(Previous[Strip], Previous[Strip + 1], Here[Strip]);
					Buffers.AddTriangleFacingUp(Previous[Strip + 1], Here[Strip], Here[Strip + 1]);
				}
			}
			for (int32 P = 0; P < 5; ++P)
			{
				Previous[P] = Here[P];
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
	Buffers.Commit(RiverMesh, 0, WaterMaterial);
	UE_LOG(LogTheater, Log, TEXT("Rivers: %d, in %d points"), Land.Rivers.Num(), Buffers.Vertices.Num());
}

void ACampaignMapActor::UpdateTrains(const FVector& CameraFocus, float CameraDistance)
{
	UWorld* World = GetWorld();
	if (!World || Land.Rails.Num() == 0)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const double U = UnitScale;
	const double LinkLasts = 11.0; // seconds from one town to the next
	const FVector2D Looking = WorldToMap(CameraFocus);
	// From far off the trains are too small to see, and are put away.
	const bool bNear = CameraDistance < 9000.0f;

	// The trains running: each along its link, and on to another when it arrives.
	for (int32 Index = Trains.Num() - 1; Index >= 0; --Index)
	{
		FTrain& Train = Trains[Index];
		double Share = (Now - Train.Began) / LinkLasts;
		const bool bGone = !bNear || !Land.Rails.IsValidIndex(Train.Rail) || ShownCut.Contains(Land.Rails[Train.Rail].A) || ShownCut.Contains(Land.Rails[Train.Rail].B)
			|| FVector2D::DistSquared(Land.Rails[Train.Rail].At, Looking) > 260.0 * 260.0;
		if (!bGone && Share >= 1.0)
		{
			// Arrived: onward by another line out of this town, if there is one and the journey is not over.
			const int32 Here = Train.bBackward ? Land.Rails[Train.Rail].A : Land.Rails[Train.Rail].B;
			TArray<int32> Onward;
			for (int32 Other = 0; Other < Land.Rails.Num(); ++Other)
			{
				if (Other != Train.Rail && (Land.Rails[Other].A == Here || Land.Rails[Other].B == Here) && !ShownCut.Contains(Land.Rails[Other].A) && !ShownCut.Contains(Land.Rails[Other].B))
				{
					Onward.Add(Other);
				}
			}
			if (Onward.Num() > 0 && ++Train.Links < 4)
			{
				Train.Rail = Onward[FMath::RandHelper(Onward.Num())];
				Train.bBackward = Land.Rails[Train.Rail].B == Here;
				Train.Began = Now;
				Share = 0.0;
			}
		}
		if (bGone || Share >= 1.0)
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
		// It draws out of the station and slows into the next.
		const double Along = FMath::SmoothStep(0.0, 1.0, Share) * (First + Second);
		const FVector2D A = Along < First ? From : Rail.At, B = Along < First ? Rail.At : To;
		const double Leg = Along < First ? First : Second, Done = Along < First ? Along : Along - First;
		const FVector2D At = FMath::Lerp(A, B, Leg > 0.01 ? Done / Leg : 0.0);
		const FVector Here = SurfacePointOver(At.X, At.Y, 0.2);
		const FVector Ahead = (MapToWorld(B.X, B.Y) - MapToWorld(A.X, A.Y)).GetSafeNormal2D();
		if (Train.Mesh)
		{
			Train.Mesh->SetRelativeLocationAndRotation(Here, FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Ahead.Y, Ahead.X)), 0.0));
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
		if (Data.Regions.IsValidIndex(Rail.A) && Data.Regions.IsValidIndex(Rail.B) && FVector2D::DistSquared(Rail.At, Looking) < Reach * Reach && !ShownCut.Contains(Rail.A) && !ShownCut.Contains(Rail.B)
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
	for (int32 Wagon = 0; Wagon < 3; ++Wagon)
	{
		const double X = (-2.75 - Wagon * 1.7) * U;
		Model.Box(FVector(X, 0.0, 0.52 * U), FVector(1.5 * U, W, 0.7 * U), Wagons[(Sort + Wagon) % 4]);
		Model.Box(FVector(X, 0.0, 0.12 * U), FVector(1.3 * U, W * 0.8, 0.14 * U), Black);
	}
	FTrain& Train = Trains.AddDefaulted_GetRef();
	Train.Rail = Near[FMath::RandHelper(Near.Num())];
	Train.bBackward = FMath::RandBool();
	Train.Began = Now;
	Train.Mesh = NewObject<UProceduralMeshComponent>(this);
	Train.Mesh->SetupAttachment(Root);
	Train.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Train.Mesh->RegisterComponent();
	Train.Mesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
	Train.Mesh->SetMaterial(0, SceneryMaterial);
	Train.Mesh->SetCastShadow(true);
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
			// (a little to one side of the town, on a post, so it stands clear of the houses and the lettering)
			const FVector Foot = SurfacePointOver(Data.Regions[RegionId].Center.X - 3.0, Data.Regions[RegionId].Center.Y + 3.0, 0.1);
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
			const FVector2D Spot = Data.Regions[RegionId].Center + FVector2D(4.0, 5.0);
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
	if (Model.Vertices.Num() > 0)
	{
		WorksMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		WorksMesh->SetMaterial(0, SceneryMaterial);
	}
	UE_LOG(LogTheater, Log, TEXT("Works: %d crossings (%d bridges down), %d railways cut"), Land.Crossings.Num(), ShownDown.Num(), ShownCut.Num());
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

	// Four slabs round the tray.
	AddBlock(FVector(Half.X + Reach * 0.5, 0.0, CenterZ), FVector(Reach, 2.0 * (Half.Y + Reach), Thickness), Wood);
	AddBlock(FVector(-Half.X - Reach * 0.5, 0.0, CenterZ), FVector(Reach, 2.0 * (Half.Y + Reach), Thickness), Wood);
	AddBlock(FVector(0.0, Half.Y + Reach * 0.5, CenterZ), FVector(2.0 * Half.X, Reach, Thickness), Wood);
	AddBlock(FVector(0.0, -Half.Y - Reach * 0.5, CenterZ), FVector(2.0 * Half.X, Reach, Thickness), Wood);

	// The brass lip, standing a little proud of the wood.
	const double Lip = 1.3 * U, LipHeight = 0.35 * U;
	const double LipZ = Top + LipHeight * 0.5;
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
	if (!Data.IsLoaded() || (CivicMesh && Civic == ShownCivic))
	{
		return;
	}
	ShownCivic = Civic;
	if (!CivicMesh)
	{
		CivicMesh = NewObject<UProceduralMeshComponent>(this);
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
		// (the near row stands just outside the last of the town's own houses)
		const double Clear = Region.bCapital ? 14.5 : (bCity ? 13.5 : 10.5);
		const double Size = Region.bCapital ? 2.9 : (bCity ? 2.7 : 2.4);
		const FVector2D Offset(Plots[Line].X, Clear + Plots[Line].Y);
		// If its place is in the sea or over the frontier, the quarter is tried on the town's other sides.
		FVector2D At = Region.Center;
		int32 Found = 0; // 2 on the province's own dry land, 1 on dry land
		for (const double Shift : { 0.0, -55.0, 55.0, -110.0, 110.0, 180.0 })
		{
			const double Angle = FMath::DegreesToRadians(Shift);
			const FVector2D Try = Region.Center + FVector2D(Offset.X * FMath::Cos(Angle) - Offset.Y * FMath::Sin(Angle), Offset.X * FMath::Sin(Angle) + Offset.Y * FMath::Cos(Angle));
			if (Terrain.HeightAt(Try.X, Try.Y) <= 0.0f)
			{
				continue;
			}
			const int32 Here = Data.FindRegionAt(Try.X, Try.Y) == RegionId ? 2 : 1;
			if (Here > Found)
			{
				Found = Here;
				At = Try;
			}
			if (Found == 2)
			{
				break;
			}
		}
		if (Found == 0)
		{
			continue;
		}
		const FCampaignNation* Holder = Data.FindNation(Region.Owner);
		Model.Civic(SurfacePoint(At.X, At.Y, 0.02), U * Size, Line, Level, Civic[K + 3] != 0, Holder ? FLinearColor(Holder->Color) : FLinearColor::Gray);
	}
	if (Model.Vertices.Num() > 0)
	{
		CivicMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		CivicMesh->SetMaterial(0, SceneryMaterial);
	}
}

void ACampaignMapActor::SetFlatColours(bool bFlat)
{
	bFlatColours = bFlat;
	if (LandMaterial)
	{
		LandMaterial->SetScalarParameterValue(TEXT("Flat"), bFlat ? 1.0f : 0.0f);
	}
}

void ACampaignMapActor::SetColourStrength(int32 Level)
{
	ColourStrength = FMath::Clamp(Level, 0, 2);
	WashShown = -1.0f; // (set again at the next look)
	if (LandMaterial)
	{
		static const float Bands[3] = { 0.32f, 0.4f, 0.48f };
		LandMaterial->SetScalarParameterValue(TEXT("Band"), Bands[ColourStrength]);
	}
}

void ACampaignMapActor::ShowSieges(const TArray<int32>& Sieges)
{
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
			SiegeFires.Add(SurfacePoint(Region.Center.X + FMath::Cos(Angle) * 2.2, Region.Center.Y + FMath::Sin(Angle) * 2.2, 1.6));
		}
	}
}

void ACampaignMapActor::ShowPrizes(const TArray<FPrizeMark>& Marks)
{
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
		PrizesMesh = NewObject<UProceduralMeshComponent>(this);
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
			PrizeFires.Add(SurfacePoint(Region.Center.X - 3.0, Region.Center.Y - 1.9, 0.5));
		}
		if (!Mark.By.IsEmpty())
		{
			// A pennant in the colour of the army that holds what lies outside the walls, though the town holds:
			// two cloths crossed on one pole, so that it is read from any side of the table.
			const FCampaignNation* Whose = Data.FindNation(Mark.By);
			const FLinearColor Cloth = Whose ? FLinearColor(Whose->Color) : FLinearColor(0.7f, 0.7f, 0.7f);
			// (a pale band along its top edge, so that a dark colour is still seen to be a flag)
			const FLinearColor Band(FColor::FromHex(TEXT("EFE6CC")));
			const FVector Foot = SurfacePoint(Region.Center.X + 3.6, Region.Center.Y + 2.6, 0.02);
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
	}
}

void ACampaignMapActor::ShowScars(const TArray<int32>& Scars)
{
	if (!Data.IsLoaded() || (ScarsMesh && Scars == ShownScars))
	{
		return;
	}
	ShownScars = Scars;
	if (!ScarsMesh)
	{
		ScarsMesh = NewObject<UProceduralMeshComponent>(this);
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
		if (FVector2D::Distance(Field, Region.Center) < 16.0)
		{
			bool bFound = false;
			const double Turn = Random.FRandRange(0.0f, 360.0f);
			for (int32 Try = 0; Try < 8 && !bFound; ++Try)
			{
				// (west, north-west and north first: the east is the army's, the south the town's new quarter)
				const double Angle = FMath::DegreesToRadians(200.0 + Try * 45.0 + Turn * 0.1);
				const FVector2D At = Region.Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 19.0;
				if (Terrain.HeightAt(At.X, At.Y) > 0.0f && Data.FindRegionAt(At.X, At.Y) == RegionId)
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
				if (Terrain.HeightAt(Out.X, Out.Y) > 0.0f)
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
	}
}

void ACampaignMapActor::BuildTowns()
{
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

	// Every town of note stands on the table as a cluster of houses: a few for a town, more for a city, a domed
	// palace in the middle of a capital, and a works with a chimney for each pair of factories the province has.
	FMiniatureMesh Model;
	const double U = UnitScale;
	int32 Count = 0;
	TownChimneys.Reset();
	TownWindows.Reset();
	PaneRegion.Reset();
	Model.Windows = &TownWindows;
	for (const FCampaignRegion& Region : Data.Regions)
	{
		const bool bCity = Region.Terrain == TEXT("urban");
		const bool bBig = Region.bCapital || bCity || Region.VictoryPoints >= 2 || Region.Factories >= 2;
		const bool bTown = bBig || Region.VictoryPoints > 0 || Region.Factories > 0 || Region.bOil || Region.bPort;
		if (!bTown)
		{
			continue;
		}
		// What the place is worth, where the eye can count it: a stack of gold coins beside the town, one for each
		// point it scores toward victory.
		if (Region.VictoryPoints > 0)
		{
			const FVector Foot = SurfacePoint(Region.Center.X + 3.6, Region.Center.Y + 1.4, 0.02);
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
			const FVector Foot = SurfacePoint(Region.Center.X - 4.2, Region.Center.Y - 2.6, 0.02);
			const FLinearColor Iron(FColor::FromHex(TEXT("2B2A28"))), Tank(FColor::FromHex(TEXT("9A9C9E")));
			for (int32 Rig = 0; Rig < 2; ++Rig)
			{
				const FVector At = Foot + FVector(Rig * 1.5 * U, Rig * 0.9 * U, 0.0);
				Model.Pillar(At, 0.55 * U, 0.08 * U, 3.0 * U, Iron, 4);
				Model.Box(At + FVector(0.0, 0.0, 3.05 * U), FVector(0.3, 0.3, 0.12) * U, Iron);
			}
			Model.Pillar(Foot + FVector(-1.3 * U, 0.6 * U, 0.0), 0.7 * U, 0.7 * U, 0.8 * U, Tank, 12);
		}
		const int32 Houses = Region.bCapital ? 13 : (bCity ? 10 : (bBig ? 6 : 4));
		const int32 WindowsBefore = TownWindows.Num();
		Model.Town(SurfacePoint(Region.Center.X, Region.Center.Y, 0.02), U * 2.1, Houses, Region.bCapital, bCity, FMath::Min(Region.Factories / 2, 3), Region.Id, &TownChimneys, Region.bPort);
		for (int32 Pane = WindowsBefore / 2; Pane < TownWindows.Num() / 2; ++Pane)
		{
			PaneRegion.Add(Region.Id);
		}
		++Count;
	}
	if (Model.Vertices.Num() > 0)
	{
		TownMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		TownMesh->SetMaterial(0, SceneryMaterial);
	}
	UE_LOG(LogTheater, Log, TEXT("Towns: %d, in %d points"), Count, Model.Vertices.Num());

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
	LightColours.Reset();
	LightPoints.Reset();
	ShownNight = -1.0f;
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
					const float Ground = Terrain.HeightAt(X, Y);
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
			const double Spread = (Region.bCapital ? 5.2 : bCity ? 4.4 : bBig ? 3.4 : 2.5) * U; // (half what it first was)
			AddGlow(SurfacePoint(Region.Center.X, Region.Center.Y, 5.4), Spread, Region.Id, 0);
			const int32 Quarters = Region.bCapital ? 4 : bCity ? 2 : 0;
			for (int32 Quarter = 0; Quarter < Quarters; ++Quarter)
			{
				const double Angle = Scatter.FRandRange(0.0f, 6.283f) + Quarter * 1.7, Out = Scatter.FRandRange(2.0f, 3.9f);
				AddGlow(SurfacePoint(Region.Center.X + FMath::Cos(Angle) * Out, Region.Center.Y + FMath::Sin(Angle) * Out, 3.4), Scatter.FRandRange(1.4f, 2.1f) * U, Region.Id, 1);
			}
			AddGlow(SurfacePoint(Region.Center.X, Region.Center.Y, 1.4), FMath::Max(Spread * 3.2, 8.0 * U), Region.Id, 2);
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

void ACampaignMapActor::SetNight(float Night, bool bLamplight)
{
	const bool bLampChanged = bLamplight != bLamplightNow;
	NightNow = Night;
	bLamplightNow = bLamplight;
	if (!bLampChanged && FMath::Abs(Night - ShownNight) < 0.02f)
	{
		return;
	}
	ShownNight = Night;
	// The land's own shading follows the light of the hour: a warm sun and cool shade by day (the lit tones a tenth
	// brighter than they were first mixed), and by moonlight both of them cool. By lamplight they stay as by day.
	if (LandMaterial)
	{
		const float Moon = bLamplight ? 0.0f : Night;
		LandMaterial->SetVectorParameterValue(TEXT("SunColour"), FMath::Lerp(FLinearColor(0.69f, 0.62f, 0.5f), FLinearColor(0.77f, 0.81f, 1.0f), Moon));
		LandMaterial->SetVectorParameterValue(TEXT("ShadeColour"), FMath::Lerp(FLinearColor(0.43f, 0.48f, 0.58f), FLinearColor(0.44f, 0.48f, 0.7f), Moon));
		if (WoodsMaterial)
		{
			// (and the woods are cooled and dimmed with it, or the yellows of autumn shine like lamps in the blackout)
			WoodsMaterial->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(0.36f, 0.56f, 0.8f), Moon));
		}
		// (by moonlight the nations' colours sink into the dark, as colours do: the land is mostly the land)
		LandMaterial->SetScalarParameterValue(TEXT("Politics"), FMath::Lerp(0.94f, 0.42f, Moon));
	}
	{
		// The water by moonlight: greyed (a blue moon on blue water is cobalt, and a blue nation's coast is lost
		// in it), the shelf seas taken down a little so that moonlit land is brighter than the water beside it, the
		// deeps lifted a touch, and a thin pale line of surf kept along the shore. Rivers go to steel. By lamplight
		// all of it stays as by day.
		const float Moon = bLamplight ? 0.0f : Night;
		if (SeaMaterial)
		{
			SeaMaterial->SetScalarParameterValue(TEXT("Grey"), SeaNight.X * Moon);
			SeaMaterial->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, SeaNightTint, Moon));
			SeaMaterial->SetVectorParameterValue(TEXT("Lift"), FMath::Lerp(FLinearColor::Black, SeaNightLift, Moon));
			SeaMaterial->SetScalarParameterValue(TEXT("Surf"), FMath::Lerp(0.45f, SeaNight.Y, Moon));
		}
		if (WaterMaterial)
		{
			WaterMaterial->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, RiverNightTint, Moon));
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
			const bool bDark = Black.Contains(VillageRegion[Light]) || Chance % 6 == 0;
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
		for (int32 Glow = 0; Glow < GlowRegion.Num(); ++Glow)
		{
			const float Lit = Black.Contains(GlowRegion[Glow]) ? 0.0f : Dusk;
			const uint8 Kind = GlowKind[Glow];
			const float Heart = Kind == 2 ? TownWash : Kind == 1 ? 0.85f : 1.0f;
			const float Fall[GlowRings] = { Kind == 2 ? 0.95f : 0.78f, Kind == 2 ? 0.55f : 0.24f, 0.0f };
			// (lamp yellow, and kept below white: the glow's material brightens what it is given)
			const FColor Hot = Kind == 2 ? FColor(255, 176, 96) : FColor(250, 208, 142), Warm = Kind == 2 ? FColor(255, 168, 88) : FColor(246, 190, 112), Edge = Kind == 2 ? FColor(255, 160, 80) : FColor(240, 168, 92);
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
		LightsMesh->UpdateMeshSection(1, GlowPoints, TArray<FVector>(), TArray<FVector2D>(), GlowColours, TArray<FProcMeshTangent>());
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
		// Which province lies under each point: worked out once, since the provinces do not move.
		MiniHigh = 264;
		MiniWide = FMath::Max(8, FMath::RoundToInt32(MiniHigh * Data.Width / Data.Height));
		MiniRegion.SetNumUninitialized(MiniWide * MiniHigh);
		for (int32 Y = 0; Y < MiniHigh; ++Y)
		{
			for (int32 X = 0; X < MiniWide; ++X)
			{
				MiniRegion[Y * MiniWide + X] = static_cast<int16>(Data.FindRegionAt((X + 0.5) * Data.Width / MiniWide, (Y + 0.5) * Data.Height / MiniHigh));
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
	const FColor Water(22, 52, 74), Shore(12, 28, 40);
	auto At = [this](int32 X, int32 Y) -> int32
	{
		return (X < 0 || Y < 0 || X >= MiniWide || Y >= MiniHigh) ? -1 : MiniRegion[Y * MiniWide + X];
	};
	auto HolderOf = [this](int32 RegionId) -> const FString&
	{
		static const FString None;
		return Data.Regions.IsValidIndex(RegionId) ? Data.Regions[RegionId].Owner : None;
	};
	for (int32 Y = 0; Y < MiniHigh; ++Y)
	{
		for (int32 X = 0; X < MiniWide; ++X)
		{
			const int32 Here = At(X, Y);
			FColor& Out = Pixels[Y * MiniWide + X];
			if (Here < 0)
			{
				// The sea, darker along the shore so that the land stands out of it.
				const bool bCoast = At(X + 1, Y) >= 0 || At(X - 1, Y) >= 0 || At(X, Y + 1) >= 0 || At(X, Y - 1) >= 0;
				Out = bCoast ? Shore : Water;
				continue;
			}
			const FString& Holder = HolderOf(Here);
			const FCampaignNation* Nation = Data.FindNation(Holder);
			FLinearColor Tint = Nation ? FLinearColor(Nation->Color) : FLinearColor(0.35f, 0.35f, 0.33f);
			if (bSides && SideOf.IsValidIndex(Here))
			{
				// The friend-and-foe view: the player's own ground green, his allies' blue, his enemies' red, the rest grey.
				const FString& Side = SideOf[Here];
				Tint = Side == TEXT("own") ? FLinearColor(0.2f, 0.62f, 0.3f) : Side == TEXT("ally") ? FLinearColor(0.16f, 0.4f, 0.85f)
					: Side == TEXT("enemy") ? FLinearColor(0.8f, 0.1f, 0.08f) : FLinearColor(0.3f, 0.3f, 0.29f);
			}
			else if (bWealth && Towns.IsValidIndex(Here))
			{
				// The wealth view: grey ground, gold by what the province pays.
				static const float Gold[6] = { 0.0f, 0.22f, 0.4f, 0.6f, 0.8f, 1.0f };
				const float Rich = Gold[FMath::Clamp(Towns[Here].Rank, 0, 5)];
				Tint = FMath::Lerp(FLinearColor(0.16f, 0.16f, 0.15f), FLinearColor(1.0f, 0.74f, 0.1f), Rich);
			}
			if (UnseenSet.Contains(Here))
			{
				Tint *= 0.55f; // under the fog of war
			}
			// A frontier between two countries is drawn dark; a province line inside one is not drawn at all.
			const int32 East = At(X + 1, Y), South = At(X, Y + 1);
			if ((East >= 0 && HolderOf(East) != Holder) || (South >= 0 && HolderOf(South) != Holder))
			{
				Tint *= 0.45f;
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
	const FLinearColor Board(FColor::FromHex(TEXT("B9AD8F"))), Wall(FColor::FromHex(TEXT("D9D3C2"))), Ink(FColor::FromHex(TEXT("3A3A3A"))), Red(FColor::FromHex(TEXT("A02020")));
	Model.Box(Middle - FVector(0.0, 400.0 * Metre, 4.0), FVector(Long * 3.0, 860.0 * Metre, 8.0), Board);
	// The wall behind (toward +Y, so a view turned a quarter-circle looks square at it), ruled in half metres, the whole metres heavier.
	const double WallAt = 3.2 * Metre;
	Model.Box(Middle + FVector(0.0, WallAt + 2.0, 10.0 * Metre), FVector(Long * 3.0, 4.0, 20.0 * Metre), Wall);
	auto Write = [this, &Middle, WallAt, Metre](const FString& Text, double Along, double Up, double Size)
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(Root);
		Label->SetText(FText::FromString(Text));
		Label->SetWorldSize(Size * Metre);
		Label->SetTextRenderColor(FColor(40, 40, 40));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetCastShadow(false);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Upright on the wall, facing out from it.
		Label->SetRelativeRotation(FMatrix(FVector(0.0, -1.0, 0.0), FVector(1.0, 0.0, 0.0), FVector(0.0, 0.0, 1.0), FVector::ZeroVector).Rotator());
		Label->SetWorldLocation(Middle + FVector(Along, WallAt - 1.0, Up * Metre));
		Label->RegisterComponent();
	};
	for (int32 Half = 1; Half <= 7; ++Half)
	{
		const bool bWhole = Half % 2 == 0;
		Model.Box(Middle + FVector(0.0, WallAt - 0.3, Half * 0.5 * Metre), FVector(Long, 0.6, (bWhole ? 0.05 : 0.02) * Metre), bWhole ? Ink : FLinearColor(0.35f, 0.35f, 0.35f));
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
			Model.Vehicle(Names[K], Flying, Metre, 0.0);
			Model.Rod(Spot + FVector(0.0, -Out * Metre, 0.0), Flying, 0.08 * Metre, Ink, 6);
			Model.Soldier(FMiniatureMesh::VehicleNation(Names[K]), Spot + FVector(-FMiniatureMesh::VehicleRoom(Names[K]) * 0.32 * Metre, -Out * Metre, 0.0), Metre, 60.0);
			Write(FMiniatureMesh::VehicleTitle(Names[K]), Spot.X - Middle.X, 9.5, 0.9);
			ParadeSpots.Add(WorldToMap(Flying));
			continue;
		}
		if (!Model.Vehicle(Names[K], Spot, Metre, 0.0))
		{
			UE_LOG(LogTheater, Warning, TEXT("Parade: no model called %s"), *Names[K]);
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

void ACampaignMapActor::BuildTrees()
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

	// Trees stand wherever the paint says the ground is wooded: one to every couple of map units, each a little
	// off its place, firs in the north and on high ground and broad-leaved trees elsewhere. They are kept in
	// squares of country, and only the squares near the eye are drawn (UpdateForCamera): from far off the woods
	// are the dark cloth of the paint, and close up they have trees standing in them.
	const double Spacing = 2.1, Square = 130.0;
	TreeSquare = Square * UnitScale;
	const int32 SquaresX = FMath::CeilToInt32(Data.Width / Square), SquaresY = FMath::CeilToInt32(Data.Height / Square);
	const int32 PerSide = FMath::CeilToInt32(Square / Spacing);
	const double U = UnitScale;
	int32 Trees = 0;
	// Where pieces stand in each region (its first three armies, and its airfield either side of the town): kept clear.
	TArray<TArray<FVector2D>> Clearings;
	Clearings.SetNum(Data.Regions.Num());
	for (const FCampaignRegion& Region : Data.Regions)
	{
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			Clearings[Region.Id].Add(WorldToMap(ArmySpot(Region.Id, Slot)));
		}
		Clearings[Region.Id].Add(Region.Center + FVector2D(-10.0, 8.0));
		Clearings[Region.Id].Add(Region.Center + FVector2D(10.0, 9.0));
	}
	for (int32 SY = 0; SY < SquaresY; ++SY)
	{
		for (int32 SX = 0; SX < SquaresX; ++SX)
		{
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
					const float Wooded = Paint.Ground[Terrain.Index(GI, GJ)].A / 255.0f;
					if (Wooded < 0.35f || Chance > Wooded * 0.92f)
					{
						continue;
					}
					// Not on top of a town.
					const int32 RegionId = Terrain.Region[Terrain.Index(GI, GJ)];
					if (RegionId >= 0 && FVector2D::Distance(Data.Regions[RegionId].Center, FVector2D(X, Y)) < 8.5)
					{
						continue;
					}
					// Nor where the armies of the region stand: a clearing for each of the first three, and one for its airfield.
					if (RegionId >= 0 && Clearings[RegionId].ContainsByPredicate([X, Y](const FVector2D& Spot) { return FVector2D::DistSquared(Spot, FVector2D(X, Y)) < 6.2 * 6.2; }))
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
						Model.Fir(Foot, 1.9 * Size * U, 0.55 * Size * U, FMath::Lerp(Green, FLinearColor(0.5f, 0.54f, 0.54f), 0.62f * Lying));
					}
					else
					{
						const FLinearColor Summer(0.055f * Shade, 0.115f * Shade, 0.038f * Shade), Fresh(0.085f * Shade, 0.17f * Shade, 0.045f * Shade);
						// (autumn is ochre and rust, not lemon: the art director's (196, 150, 60), a little under it since the lamp is strong)
						const FLinearColor Gold(0.15f * Shade, 0.105f * Shade, 0.032f), Orange(0.15f * Shade, 0.08f * Shade, 0.03f), Russet(0.11f * Shade, 0.055f * Shade, 0.028f);
						const FLinearColor Twigs = FMath::Lerp(FLinearColor(0.115f * Shade, 0.095f * Shade, 0.08f * Shade), FLinearColor(0.5f, 0.52f, 0.52f), 0.5f * Lying);
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
			if (Model.Vertices.Num() == 0)
			{
				continue;
			}
			UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
			Mesh->SetupAttachment(Root);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false); // thousands of small shadows cost more than they show
			Mesh->RegisterComponent();
			Mesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
			Mesh->SetMaterial(0, WoodsMaterial ? WoodsMaterial : SceneryMaterial);
			Mesh->SetVisibility(false);
			TreeMeshes.Add(Mesh);
			TreeCentres.Add(MapToWorld((SX + 0.5) * Square, (SY + 0.5) * Square));
			TreesShown.Add(false);
		}
	}
	UE_LOG(LogTheater, Log, TEXT("Trees: %d, in %d squares of country"), Trees, TreeMeshes.Num());
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
			Ground = FMath::Max(Ground, static_cast<double>(Terrain.HeightAt(MapPos.X + HalfWidth * k / 4.0, MapPos.Y + SizeUnits * 0.5 * m)));
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
			Spot.World = MapToWorld(Region.Center.X, Region.Center.Y, Terrain.HeightAt(Region.Center.X, Region.Center.Y));
			Spot.Reach = (Region.bCapital ? 7.0f : bTown ? 5.2f : 3.6f) * UnitScale;
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
	MapWords.Reset();
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
		if (bPlate && FVector2D::Distance(At, Region.Center) < 9.0)
		{
			// Clear of the town and its plate: north of it, or wherever in the province there is room.
			for (const FVector2D& Offset : { FVector2D(0.0, -9.0), FVector2D(-10.0, -4.0), FVector2D(10.0, -4.0), FVector2D(0.0, 13.0) })
			{
				if (Data.FindRegionAt(Region.Center.X + Offset.X, Region.Center.Y + Offset.Y) == Region.Id)
				{
					At = Region.Center + Offset;
					break;
				}
			}
		}
		FMapWord& Word = MapWords.AddDefaulted_GetRef();
		Word.Text = Region.Name;
		Word.Height = 1.5;
		Word.From = Word.To = At;
		Word.bRegion = true;
	}
	// Seas: across the middle of each, in spaced italic capitals.
	for (const FCampaignSeaZone& Sea : Data.Seas)
	{
		FMapWord& Word = MapWords.AddDefaulted_GetRef();
		Word.Text = Sea.Name.ToUpper();
		Word.Height = 4.6;
		const double Long = Word.Height * 1.75 * FMath::Max(Word.Text.Len() - 1, 1);
		Word.From = Sea.Center - FVector2D(Long * 0.5, 0.0);
		Word.To = Sea.Center + FVector2D(Long * 0.5, 0.0);
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
			const FVector2D Plate = Seat.Center + FVector2D(0.0, 13.0);
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

FVector ACampaignMapActor::ArmySpot(int32 RegionId, int32 Slot) const
{
	const FCampaignRegion& Region = Data.Regions[RegionId];
	// Beside the town, clear of its houses: to the north-east of it, or the north-west if that is over the border.
	FVector2D Spot = Region.Center + FVector2D(9.0 + 11.0 * Slot, -7.0);
	if (Data.FindRegionAt(Spot.X, Spot.Y) != Region.Id)
	{
		Spot = Region.Center + FVector2D(-9.0 - 11.0 * Slot, -7.0);
		if (Data.FindRegionAt(Spot.X, Spot.Y) != Region.Id)
		{
			Spot = Slot == 0 ? Region.Centroid : Region.Center + FVector2D(0.0, -6.0 * Slot);
		}
	}
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

AArmyPiece* ACampaignMapActor::PieceAtRay(const FVector& Origin, const FVector& Direction) const
{
	AArmyPiece* Best = nullptr;
	double Nearest = TNumericLimits<double>::Max();
	auto Try = [&](AArmyPiece* Piece)
	{
		double Distance = 0.0;
		if (Piece && Piece->HitByRay(Origin, Direction, Distance) && Distance < Nearest)
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
	return Best;
}

void ACampaignMapActor::ShowArmies(const FCampaignView& View, const TArray<FCorpsMove>& Marches, const FVector* CarriedFrom)
{
	UWorld* World = GetWorld();
	if (!World || !Data.IsLoaded() || HasParade())
	{
		return; // (with the models out for inspection, the armies stay in their box)
	}
	// The first month is September 1939.
	FMiniatureMesh::Year = 1939 + (FMath::Max(View.Turn, 1) + 7) / 12;
	FMiniatureMesh::DesignYears = View.DesignYears;

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
		const FVector Spot = ArmySpot(Group[0].Region, Present.IndexOfByKey(Group[0].Nation));

		TObjectPtr<AArmyPiece>& Piece = PieceByKey.FindOrAdd(Key);
		if (!Piece)
		{
			Piece = World->SpawnActor<AArmyPiece>(Spot, FRotator(0.0f, LastPieceYaw, 0.0f));
		}
		if (Piece)
		{
			Piece->SetArmy(this, *Nation, Group[0].Region, Group);
			Piece->SetRestLocation(Spot);
			Pieces.Add(Piece);
		}
	}

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
		if (!From)
		{
			continue;
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
			if (AArmyPiece* Courier = World->SpawnActor<AArmyPiece>(CarriedFrom ? *CarriedFrom : *From, Piece->GetActorRotation()))
			{
				Courier->SetArmy(this, *Nation, Piece->GetRegionId(), Travellers);
				Courier->MarchAlong(Road, true, CarriedFrom == nullptr);
			}
		}
	}

	// The fleets and the air fleets are part of the same picture.
	ShowFleets(View);
}

void ACampaignMapActor::ShowFleets(const FCampaignView& View)
{
	UWorld* World = GetWorld();
	if (!World || !Data.IsLoaded())
	{
		return;
	}
	const FRotator Facing(0.0f, LastPieceYaw, 0.0f);

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
			const FVector2D Port = Data.Regions[Fleet.Base].Center;
			const FVector2D Out = (Sea - Port).GetSafeNormal();
			At = Port;
			for (int32 Step = 0; Step < 80 && Terrain.CoverageAt(At.X, At.Y) > 0.35f; ++Step)
			{
				At += Out * 1.5;
			}
			At += Out * 6.0 + FVector2D(-Out.Y, Out.X) * (Others * 11.0);
		}
		else
		{
			// Several fleets in one sea lie in a ring round its middle.
			const double Angle = Others * 2.1 + Fleet.Zone;
			At += FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Others > 0 ? 16.0 : 0.0);
		}
		++Others;
		TObjectPtr<AArmyPiece>& Piece = FleetPieces.FindOrAdd(Fleet.Id);
		if (!Piece)
		{
			Piece = World->SpawnActor<AArmyPiece>(MapToWorld(At.X, At.Y), Facing);
		}
		if (Piece)
		{
			Piece->SetFleet(this, *Nation, Fleet);
			Piece->SetRestLocation(MapToWorld(At.X, At.Y, 0.02));
			Afloat.Add(Fleet.Id);
		}
	}
	for (auto It = FleetPieces.CreateIterator(); It; ++It)
	{
		if (!Afloat.Contains(It.Key()))
		{
			if (It.Value())
			{
				It.Value()->Destroy();
			}
			It.RemoveCurrent();
		}
	}

	// ---- Air fleets, each over its airfields: to the south-west of the town, where the army does not stand.
	TSet<int32> Flying;
	for (const FViewAirFleet& Wing : View.AirFleets)
	{
		const FCampaignNation* Nation = Data.FindNation(Wing.Nation);
		if (!Nation || !Data.Regions.IsValidIndex(Wing.Base))
		{
			continue;
		}
		int32& Others = Crowd.FindOrAdd(FIntPoint(3, Wing.Base));
		FVector2D At = Data.Regions[Wing.Base].Center + FVector2D(-10.0 - 11.0 * Others, 8.0);
		if (Data.FindRegionAt(At.X, At.Y) != Wing.Base)
		{
			At = Data.Regions[Wing.Base].Center + FVector2D(10.0 + 11.0 * Others, 9.0);
		}
		++Others;
		TObjectPtr<AArmyPiece>& Piece = AirPieces.FindOrAdd(Wing.Id);
		if (!Piece)
		{
			Piece = World->SpawnActor<AArmyPiece>(SurfacePoint(At.X, At.Y, 0.0), Facing);
		}
		if (Piece)
		{
			Piece->SetAirFleet(this, *Nation, Wing);
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

void ACampaignMapActor::SetHolders(const TArray<FString>& Holders, const TArray<FString>& League)
{
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
	Data.League = Members;

	FBox2D Changed(ForceInit);
	for (int32 i = 0; i < Data.Regions.Num() && i < Holders.Num(); ++i)
	{
		const bool bNewHolder = !Holders[i].IsEmpty() && Data.Regions[i].Owner != Holders[i];
		if (bNewHolder)
		{
			Data.Regions[i].Owner = Holders[i];
			// It flashes as it changes hands (not at the first laying out of the table, nor more than a dozen at once).
			if (bLandLaid && GetWorld() && Flashes.Num() < 12)
			{
				Flashes.Emplace(i, GetWorld()->GetTimeSeconds());
			}
		}
		if ((bNewHolder || Turned.Contains(Data.Regions[i].Owner)) && Data.Regions[i].Bounds.bIsValid)
		{
			Changed += Data.Regions[i].Bounds;
		}
	}
	if (!Changed.bIsValid)
	{
		return;
	}
	bMiniStale = true;
	BuildWords();

	// Ground has changed hands. Repaint it, with a margin for the band of thick paint along the new
	// frontier, rebuild only the pieces of the land that touches, and redraw the frontier lines.
	Terrain.MeasureFrontiers(Data);
	const double Margin = FCampaignGround::BandReach + 2.0; // (the band of stronger stain along the new frontier reaches that far in)
	const int32 I0 = FMath::Clamp(FMath::FloorToInt32((Changed.Min.X - Margin) / Terrain.Cell), 0, Terrain.NX - 1);
	const int32 I1 = FMath::Clamp(FMath::CeilToInt32((Changed.Max.X + Margin) / Terrain.Cell), 0, Terrain.NX - 1);
	const int32 J0 = FMath::Clamp(FMath::FloorToInt32((Changed.Min.Y - Margin) / Terrain.Cell), 0, Terrain.NY - 1);
	const int32 J1 = FMath::Clamp(FMath::CeilToInt32((Changed.Max.Y + Margin) / Terrain.Cell), 0, Terrain.NY - 1);
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
	// Only the nations' paint changes: the land itself is left as it is.
	Paint.PaintTint(Data, Terrain, I0, J0, I1, J1);
	CampaignTextures::Update(TintPicture, Paint.Tint);
	CampaignTextures::Update(ReliefPicture, Paint.Relief); // (it carries how much wash each nation's ground needs)
	BuildBorders();
}

void ACampaignMapActor::SetReach(const TArray<FReachRegion>& Reach, bool bByRail)
{
	OrdersMesh->ClearMeshSection(0);
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
	// Ground across a frontier at peace is amber: the army can go there, but only by declaring war.
	const FLinearColor War(1.0f, 0.55f, 0.0f, 0.5f);
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
	Buffers.Commit(OrdersMesh, 0, HighlightMaterial);
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

void ACampaignMapActor::SetSeason(int32 Turn)
{
	// ("-TheaterSeason=5" on the command line holds the table at one month of the war, for a picture)
	int32 Held = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeason="), Held))
	{
		Turn = Held;
	}
	const int32 Month = ((Turn + 7) % 12 + 12) % 12; // 0 is January; the war's first month is September
	if (Month == ShownMonth || !MasksPicture || Paint.Masks.Num() == 0 || Paint.Masks.Num() != Terrain.Height.Num())
	{
		return;
	}
	ShownMonth = Month;
	// How hard the winter lies in each month of the year.
	static const float Winter[12] = { 1.0f, 0.92f, 0.5f, 0.12f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.08f, 0.42f, 0.86f };
	const float Hard = Winter[Month];
	TArray<FColor> Pixels = Paint.Masks;
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
	CampaignTextures::Update(MasksPicture, Pixels);
	// The year turns on the table. The woods: fresh green in spring, gold and russet through the autumn, bare in
	// winter where they are not fir. The cloth of the woods seen from far off takes the same turn.
	{
		static const int32 Looks[12] = { 4, 4, 5, 6, 0, 0, 0, 0, 1, 2, 3, 4 };
		static const FLinearColor Cloth[7] =
		{
			// (a dark cloth: a wood is darker than the fields beside it, whatever the nation's colour)
			FLinearColor(0.048f, 0.104f, 0.042f), FLinearColor(0.056f, 0.102f, 0.04f), FLinearColor(0.07f, 0.096f, 0.038f), FLinearColor(0.068f, 0.08f, 0.042f),
			FLinearColor(0.06f, 0.07f, 0.054f), FLinearColor(0.058f, 0.086f, 0.045f), FLinearColor(0.058f, 0.118f, 0.042f)
		};
		const bool bTrees = Looks[Month] != TreeLook || FMath::Abs(Hard - WinterHard) > 0.3f;
		TreeLook = Looks[Month];
		WinterHard = Hard;
		if (LandMaterial)
		{
			LandMaterial->SetVectorParameterValue(TEXT("WoodsColour"), Cloth[TreeLook]);
		}
		if (bTrees && TreeMeshes.Num() > 0)
		{
			const double Began = FPlatformTime::Seconds();
			BuildTrees();
			UE_LOG(LogTheater, Log, TEXT("The woods turned for the month in %.2f s"), FPlatformTime::Seconds() - Began);
		}
		// The rivers of the east freeze where the snow lies, and run again in the thaw.
		if (FMath::Abs(Hard - RiversDrawnFor) > 0.01f && RiverMesh)
		{
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

void ACampaignMapActor::ShowSides(bool bOn, const TArray<FString>& Standing)
{
	if (bOn == bSides && Standing == SideOf)
	{
		return;
	}
	bSides = bOn;
	SideOf = Standing;
	bMiniStale = true;
	OrdersMesh->ClearMeshSection(5);
	if (!bSides)
	{
		return;
	}
	FCampaignMeshBuffers Buffers;
	for (int32 Id = 0; Id < Data.Regions.Num() && Id < SideOf.Num(); ++Id)
	{
		const FString& Side = SideOf[Id];
		if (Side == TEXT("enemy"))
		{
			AppendRegionFill(Buffers, Id, FLinearColor(0.95f, 0.08f, 0.05f, 0.42f));
		}
		else if (Side == TEXT("ally"))
		{
			AppendRegionFill(Buffers, Id, FLinearColor(0.15f, 0.5f, 1.0f, 0.38f));
		}
		else if (Side == TEXT("own"))
		{
			AppendRegionFill(Buffers, Id, FLinearColor(0.3f, 1.0f, 0.45f, 0.2f));
		}
		else
		{
			// A country at peace with the player is greyed, so that a red country at peace is not taken for an enemy.
			AppendRegionFill(Buffers, Id, FLinearColor(0.36f, 0.36f, 0.34f, 0.6f));
		}
	}
	Buffers.Commit(OrdersMesh, 5, HighlightMaterial);
}

void ACampaignMapActor::UpdateFlashes()
{
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
	if (Regions == ShownMud)
	{
		return;
	}
	ShownMud = Regions;
	OrdersMesh->ClearMeshSection(7);
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
	UWorld* World = GetWorld();
	if (!CloudMesh || !World || !HighlightMaterial || !Data.IsLoaded())
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
	static const float Depth = []() { float Asked = 0.17f; FParse::Value(FCommandLine::Get(), TEXT("TheaterClouds="), Asked); return FMath::Clamp(Asked, 0.0f, 0.9f); }();
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

void ACampaignMapActor::ShowShroud(const TArray<int32>& Unseen)
{
	if (Unseen == ShownUnseen)
	{
		return;
	}
	ShownUnseen = Unseen;
	UnseenSet = TSet<int32>(Unseen);
	bMiniStale = true;
	OrdersMesh->ClearMeshSection(6);
	if (Unseen.Num() == 0)
	{
		return;
	}
	// A shadow over the ground nobody of the player's can see: what stands there is not known.
	FCampaignMeshBuffers Buffers;
	for (int32 RegionId : Unseen)
	{
		if (Data.Regions.IsValidIndex(RegionId))
		{
			AppendRegionFill(Buffers, RegionId, FLinearColor(0.02f, 0.03f, 0.05f, 0.34f));
		}
	}
	Buffers.Commit(OrdersMesh, 6, HighlightMaterial);
}

void ACampaignMapActor::ShowFreeCapitals(const TArray<int32>& Capitals)
{
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
	if (Zones.Num() == 0)
	{
		return;
	}
	FCampaignMeshBuffers Buffers;
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
		Ring(Middle, Wide * 0.5, Wide * 0.5, FLinearColor(1.0f, 0.82f, 0.3f, bHere ? 0.1f : 0.045f)); // the water inside it, tinted
		Ring(Middle, Wide, bHere ? 0.9 : 0.55, FLinearColor(1.0f, 0.82f, 0.3f, bHere ? 0.98f : 0.7f));
		Ring(Middle, 1.8, bHere ? 1.4 : 1.0, FLinearColor(1.0f, 0.86f, 0.4f, bHere ? 0.98f : 0.75f));
		if (bHere && bHaveFleet && FVector2D::Distance(From, Middle) > 12.0)
		{
			// The course it would steer, dashed from the fleet to the sea.
			const double Length = FVector2D::Distance(From, Middle);
			const int32 Dashes = FMath::Clamp(FMath::RoundToInt32(Length / 6.0), 2, 200);
			for (int32 Dash = 0; Dash < Dashes; ++Dash)
			{
				TArray<FVector2D> Stroke;
				Stroke.Add(FMath::Lerp(From, Middle, (Dash + 0.15) / Dashes));
				Stroke.Add(FMath::Lerp(From, Middle, (Dash + 0.7) / Dashes));
				AppendRibbon(Buffers, Stroke, 0.5, 0.3, FLinearColor(1.0f, 0.92f, 0.6f, 0.85f).ToFColor(false), false);
			}
		}
	}
	Buffers.Commit(OrdersMesh, 2, HighlightMaterial);
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
	// Gold for a march, red where it ends in a battle, amber where it means declaring war.
	const FColor Gold = (RouteEnds == 1 ? FLinearColor(1.0f, 0.3f, 0.2f, 0.95f) : RouteEnds == 2 ? FLinearColor(1.0f, 0.62f, 0.12f, 0.95f) : FLinearColor(1.0f, 0.83f, 0.45f, 0.95f)).ToFColor(false);
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

void ACampaignMapActor::ShowLanes(const TArray<FViewLane>& Lanes)
{
	if (!LanesMesh || !Data.IsLoaded())
	{
		return;
	}
	FString Key;
	for (const FViewLane& Lane : Lanes)
	{
		Key += FString::Printf(TEXT("%s%d:%d;"), *Lane.State, Lane.Path.Num(), FMath::RoundToInt32(Lane.Flow * 20.0f));
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
		const FLinearColor Ink = Lane.State.EndsWith(TEXT("cut")) ? FLinearColor(0.95f, 0.14f, 0.1f, 0.62f)
			: Lane.State.EndsWith(TEXT("raided")) ? FLinearColor(1.0f, 0.68f, 0.14f, 0.58f)
			: bTrade ? FLinearColor(1.0f, 0.84f, 0.42f, 0.5f)
			: FLinearColor(0.86f, 0.93f, 1.0f, 0.3f);
		// The lane as a smooth curve through its points, a step every map unit or so.
		TArray<FVector2D> Curve;
		const int32 Count = Lane.Path.Num();
		for (int32 K = 0; K + 1 < Count; ++K)
		{
			const FVector2D P0 = Lane.Path[FMath::Max(K - 1, 0)], P1 = Lane.Path[K], P2 = Lane.Path[K + 1], P3 = Lane.Path[FMath::Min(K + 2, Count - 1)];
			const int32 Steps = FMath::Clamp(FMath::CeilToInt32(FVector2D::Distance(P1, P2) / 2.0), 2, 400);
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const double T = static_cast<double>(Step) / Steps;
				Curve.Add(0.5 * ((2.0 * P1) + (P2 - P0) * T + (2.0 * P0 - 5.0 * P1 + 4.0 * P2 - P3) * T * T + (3.0 * P1 - P0 - 3.0 * P2 + P3) * T * T * T));
			}
		}
		Curve.Add(Lane.Path.Last());
		// Drawn as dashes, like a course pencilled on a chart; only over the water.
		const int32 Dash = 4, Gap = 3;
		for (int32 First = 0; First + 1 < Curve.Num(); First += Dash + Gap)
		{
			TArray<FVector2D> Stroke;
			for (int32 K = First; K <= First + Dash && K < Curve.Num(); ++K)
			{
				if (!bTrade && Terrain.HeightAt(Curve[K].X, Curve[K].Y) > 0.0f)
				{
					break;
				}
				Stroke.Add(Curve[K]);
			}
			if (Stroke.Num() >= 2)
			{
				AppendRibbon(Buffers, Stroke, Lane.State == TEXT("open") ? 0.55 : bTrade ? 0.42 : 0.75, bTrade ? 0.3 : 0.12, Ink.ToFColor(false), false);
			}
		}
	}
	Buffers.Commit(LanesMesh, 0, HighlightMaterial);
}

FString ACampaignMapActor::ModelFor(const FString& Nation, const FString& Kind)
{
	if (Kind == TEXT("arm")) { return FMiniatureMesh::MainTankOf(Nation); }
	if (Kind == TEXT("heavy")) { return Nation == TEXT("GER") ? FString(TEXT("Tiger")) : FMiniatureMesh::MainTankOf(Nation); }
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
	Stage->ClearAllMeshSections();
	Stage->CreateMeshSection(0, Built.Vertices, Built.Triangles, Built.Normals, TArray<FVector2D>(), Built.Colors, TArray<FProcMeshTangent>(), false);
	Stage->SetMaterial(0, SceneryMaterial);
	return true;
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

UTextureRenderTarget2D* ACampaignMapActor::GetPortrait(const FString& Model)
{
	if (const TObjectPtr<UTextureRenderTarget2D>* Kept = Portraits.Find(Model))
	{
		return Kept->Get();
	}
	UTextureRenderTarget2D* Picture = nullptr;
	double Radius = 100.0;
	if (PortraitMesh && PortraitCamera && StageModel(Model, PortraitMesh, Radius))
	{
		Picture = NewObject<UTextureRenderTarget2D>(this);
		Picture->ClearColor = FLinearColor(0.03f, 0.028f, 0.024f, 1.0f);
		Picture->InitCustomFormat(184, 112, PF_B8G8R8A8, false);
		Picture->UpdateResourceImmediate(true);
		AimStudioCamera(PortraitCamera, PortraitMesh->GetRelativeLocation(), Radius);
		PortraitCamera->TextureTarget = Picture;
		PortraitCamera->CaptureScene();
	}
	Portraits.Add(Model, Picture); // (kept even when there is none, so that it is not tried again every frame)
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
	ShowCamera->CaptureScene();
}

FVector2D ACampaignMapActor::AirfieldSpot(int32 RegionId) const
{
	if (const FVector2D* Spot = AirfieldSpots.Find(RegionId))
	{
		return *Spot;
	}
	return Data.Regions.IsValidIndex(RegionId) ? Data.Regions[RegionId].Center + FVector2D(-10.0, 12.0) : FVector2D::ZeroVector;
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

void ACampaignMapActor::FlySortie(const FSortie& Sortie)
{
	if (!GetWorld() || (!Sortie.bFromSea && !Data.Regions.IsValidIndex(Sortie.From)))
	{
		return;
	}
	FFlight& Flight = Flights.AddDefaulted_GetRef();
	Flight.Sortie = Sortie;
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
		Model.Vehicle(Bomber, FVector(0.0, 0.0, 0.0), Metre, 0.0, true);
		Model.Vehicle(Bomber, FVector(26.0 * Metre, 22.0 * Metre, 3.0 * Metre), Metre, 0.0, true);
		Model.Vehicle(Bomber, FVector(26.0 * Metre, -22.0 * Metre, 3.0 * Metre), Metre, 0.0, true);
		if (Sortie.bEscort)
		{
			const double Small = 0.5 * U;
			Model.Vehicle(Fighter, FVector(-6.0 * Metre, 34.0 * Metre, 16.0 * Metre), Small, 0.0, true);
			Model.Vehicle(Fighter, FVector(10.0 * Metre, -36.0 * Metre, 18.0 * Metre), Small, 0.0, true);
		}
	}
	else
	{
		const double Small = 0.55 * U;
		Model.Vehicle(Fighter, FVector(0.0, 0.0, 0.0), Small, 0.0, true);
		Model.Vehicle(Fighter, FVector(9.0 * Small, 8.0 * Small, 1.0 * Small), Small, 0.0, true);
		Model.Vehicle(Fighter, FVector(9.0 * Small, -8.0 * Small, 1.0 * Small), Small, 0.0, true);
		Model.Vehicle(Fighter, FVector(18.0 * Small, 16.0 * Small, 2.0 * Small), Small, 0.0, true);
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
	}
	// The defender's fighters, if any can reach: a pair of his own, that rise as the raid comes in.
	if (!Sortie.Meet.IsEmpty())
	{
		FMiniatureMesh Pair;
		const double Small = 0.55 * U;
		const FString Theirs = FMiniatureMesh::MainFighterOf(Sortie.Meet);
		Pair.Vehicle(Theirs, FVector(0.0, 0.0, 0.0), Small, 0.0, true);
		Pair.Vehicle(Theirs, FVector(10.0 * Small, 9.0 * Small, 1.5 * Small), Small, 0.0, true);
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

void ACampaignMapActor::UpdateSorties()
{
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
		const double Share = (Now - Flight.Began) / FlightLasts;
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
		const FVector2D Field = SortieStart(Flight.Sortie);
		const FVector Field0 = SurfacePoint(Field.X, Field.Y, 0.3), Aim = SurfacePoint(Flight.Sortie.To.X, Flight.Sortie.To.Y, 0.3);
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
			Flight.Formation->SetRelativeLocationAndRotation(Where + FVector(0.0, 0.0, Aloft), FRotator(0.0, Yaw, Bank));
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
				const FVector2D Theirs = AirfieldSpot(Flight.Sortie.MeetFrom);
				Ground = SurfacePoint(Theirs.X, Theirs.Y, 0.3) + FVector(0.0, 0.0, 1.0 * U);
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
		const double Length = FVector2D::Distance(Field, Flight.Sortie.To);
		const int32 Dashes = FMath::Clamp(FMath::RoundToInt32(Length / 7.0), 2, 200);
		for (int32 Dash = 0; Dash < Dashes; ++Dash)
		{
			TArray<FVector2D> Stroke;
			Stroke.Add(FMath::Lerp(Field, Flight.Sortie.To, (Dash + 0.15) / Dashes));
			Stroke.Add(FMath::Lerp(Field, Flight.Sortie.To, (Dash + 0.75) / Dashes));
			AppendRibbon(Courses, Stroke, 0.45, 0.4, FLinearColor(1.0f, 0.86f, 0.4f, 0.75f).ToFColor(false), false);
		}
	}
	if (!Courses.Commit(SortieMesh, 0, HighlightMaterial))
	{
		SortieMesh->ClearAllMeshSections();
	}
}

void ACampaignMapActor::AddBattleSmoke(int32 RegionId)
{
	if (!Data.Regions.IsValidIndex(RegionId) || !GetWorld())
	{
		return;
	}
	// Between the town and where the armies stand, so that it rises clear of both.
	const FVector2D Spot = Data.Regions[RegionId].Center + FVector2D(3.5, -3.0);
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
	UWorld* World = GetWorld();
	if (!SmokeMesh || !World || !HighlightMaterial)
	{
		return;
	}
	const int32 MaxPuffs = 1000, Rim = 8, PerPuff = Rim + 1;
	if (!bSmokeMade)
	{
		// The mesh is made once, with room for every puff; after that only its points and colours change.
		bSmokeMade = true;
		SmokeVertices.Init(FVector::ZeroVector, MaxPuffs * PerPuff);
		// (two far corners to begin with, so that the mesh is never thought too small to draw)
		SmokeVertices[1] = FVector(-20000.0, -20000.0, 0.0);
		SmokeVertices[2] = FVector(20000.0, 20000.0, 2000.0);
		SmokeColours.Init(FColor(0, 0, 0, 0), MaxPuffs * PerPuff);
		TArray<int32> Triangles;
		Triangles.Reserve(MaxPuffs * Rim * 3);
		for (int32 Puff = 0; Puff < MaxPuffs; ++Puff)
		{
			const int32 First = Puff * PerPuff;
			for (int32 K = 0; K < Rim; ++K)
			{
				// Both faces, so that a puff shows whichever way it is wound to the eye.
				Triangles.Add(First);
				Triangles.Add(First + 1 + (K + 1) % Rim);
				Triangles.Add(First + 1 + K);
				Triangles.Add(First);
				Triangles.Add(First + 1 + K);
				Triangles.Add(First + 1 + (K + 1) % Rim);
			}
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, SmokeVertices.Num());
		SmokeMesh->CreateMeshSection(0, SmokeVertices, Triangles, Normals, TArray<FVector2D>(), SmokeColours, TArray<FProcMeshTangent>(), false);
		SmokeMesh->SetMaterial(0, HighlightMaterial);
		SmokeMesh->SetCastShadow(false);
	}

	// Which way the eye is looking, to turn every puff to face it.
	FVector Right = FVector::RightVector, Up = FVector::UpVector;
	if (const APlayerController* Player = World->GetFirstPlayerController())
	{
		if (Player->PlayerCameraManager)
		{
			const FRotationMatrix Looking(Player->PlayerCameraManager->GetCameraRotation());
			Right = Looking.GetUnitAxis(EAxis::Y);
			Up = Looking.GetUnitAxis(EAxis::Z);
		}
	}
	const double Now = World->GetTimeSeconds();
	const double Unit = UnitScale * 2.1;
	const FVector Wind(0.25, 0.9, 0.0); // the wind is from the west
	int32 Puff = 0;
	auto Draw = [&](const FVector& Where, double Radius, const FLinearColor& Colour, float Alpha)
	{
		if (Puff >= MaxPuffs || Alpha <= 0.004f)
		{
			return;
		}
		const int32 First = Puff * PerPuff;
		FLinearColor Middle = Colour;
		Middle.A = Alpha;
		FLinearColor Edge = Colour;
		Edge.A = 0.0f;
		SmokeVertices[First] = Where;
		SmokeColours[First] = Middle.ToFColor(false);
		for (int32 K = 0; K < Rim; ++K)
		{
			const double Angle = K * 2.0 * PI / Rim;
			SmokeVertices[First + 1 + K] = Where + (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * Radius;
			SmokeColours[First + 1 + K] = Edge.ToFColor(false);
		}
		++Puff;
	};

	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterSmokeTest")))
	{
		Draw(CameraFocus + FVector(0.0, 0.0, 120.0), 300.0, FLinearColor(1.0f, 0.5f, 0.1f), 0.9f); // one great puff, to see that puffs draw at all
		Draw(CameraFocus + FVector(500.0, 0.0, 120.0), 200.0, FLinearColor(0.03f, 0.03f, 0.03f), 0.9f); // and a black one beside it, to see that smoke can darken
	}
	// Chimneys: a thin grey plume from each, leaning with the wind. Only those near what the eye is looking at.
	const FLinearColor Grey(0.6f, 0.6f, 0.62f);
	const double Reach = FMath::Min(CameraDistance * 1.4 + 1500.0, 9000.0);
	auto Plume = [&](const FVector& Top, int32 Seed, double Scale = 1.0, const FLinearColor* Ink = nullptr)
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
			const float Alpha = 0.55f * FMath::SmoothStep(0.0, 0.1, Age) * FMath::Pow(1.0 - Age, 1.3);
			Draw(At, (0.22 + 1.1 * Age) * Big, Ink ? *Ink : Grey, Alpha);
		}
	};
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
			for (int32 K = 0; K < Smoulders.Num(); ++K)
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
		}
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
			SetDowns.Emplace(Piece->GetActorLocation(), Now);
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
	for (int32 K = 0; K < TownChimneys.Num(); ++K)
	{
		Plume(TownChimneys[K], K);
	}
	for (int32 K = 0; K < WorksChimneys.Num(); ++K)
	{
		Plume(WorksChimneys[K], K + 500);
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

	// Raids in flight: flak round the bombers over a defended target, tracer between them and the fighters that
	// rise to meet them, and a damaged bomber trailing smoke on the way home.
	const FLinearColor FlakBlack(0.03f, 0.03f, 0.035f), Tracer(1.0f, 0.86f, 0.35f);
	const double Table = UnitScale;
	for (int32 F = 0; F < Flights.Num(); ++F)
	{
		const FFlight& Flight = Flights[F];
		if (Flight.At.IsZero())
		{
			continue;
		}
		if (Flight.Sortie.Flak > 0 && Flight.Share > 0.3 && Flight.Share < 0.64)
		{
			// More guns, more bursts: each lives a second and a half and is thrown up somewhere new each time.
			const int32 Bursts = FMath::Min(Flight.Sortie.Flak, 4) * 6;
			for (int32 K = 0; K < Bursts; ++K)
			{
				const double Clock = Now / 1.5 + K * 0.371;
				const double Age = FMath::Frac(Clock);
				FRandomStream Random(static_cast<int32>(FMath::FloorToDouble(Clock)) * 7919 + K * 131 + F * 17);
				const FVector Offset(Random.FRandRange(-9.0f, 9.0f) * Table, Random.FRandRange(-9.0f, 9.0f) * Table, Random.FRandRange(-3.0f, 2.5f) * Table);
				if (Age < 0.07)
				{
					Draw(Flight.At + Offset, 1.1 * Table, Fire, 0.9f);
				}
				Draw(Flight.At + Offset + FVector(0.0, 0.0, 0.6 * Table * Age), (0.7 + 1.5 * Age) * Table, FlakBlack, 0.78f * FMath::Pow(1.0 - Age, 1.1));
			}
		}
		if (Flight.bEngaged)
		{
			// Bursts of tracer: the fighters at the bombers, and the bombers' gunners (and their escort) back at them.
			for (int32 K = 0; K < 6; ++K)
			{
				const double T = FMath::Frac(Now * 2.6 + K / 6.0);
				if (FMath::Frac(Now * 0.9) < 0.6)
				{
					Draw(FMath::Lerp(Flight.RivalAt, Flight.At, T), 0.26 * Table, Tracer, 0.95f);
				}
				if (FMath::Frac(Now * 0.9 + 0.5) < 0.6)
				{
					Draw(FMath::Lerp(Flight.At + FVector(0.0, 0.0, 0.6 * Table), Flight.RivalAt, T), 0.2 * Table, FLinearColor(1.0f, 0.5f, 0.3f), 0.9f);
				}
			}
		}
		if (Flight.Sortie.Loss >= 0.1f && Flight.Share > 0.5)
		{
			// One of them has been hit: it trails smoke behind the formation all the way home.
			for (int32 K = 0; K < 7; ++K)
			{
				const double Back = (7.0 + K * 1.9) * Table;
				Draw(Flight.At - Flight.Dir * Back + FVector(0.0, 0.0, -0.25 * Table * K + 0.4 * Table * FMath::Sin(Now * 2.0 + K)), (0.7 + 0.3 * K) * Table, Dark, 0.55f - 0.06f * K);
			}
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
	SmokeMesh->UpdateMeshSection(0, SmokeVertices, TArray<FVector>(), TArray<FVector2D>(), SmokeColours, TArray<FProcMeshTangent>());
}

void ACampaignMapActor::UpdateForCamera(float CameraDistance, float CameraYaw, const FVector& CameraFocus)
{
	UpdateSmoke(CameraDistance, CameraFocus);
	UpdateSorties();
	UpdateTrains(CameraFocus, CameraDistance);
	UpdateClouds();
	UpdateFlashes();
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
			ShowCamera->CaptureScene();
		}
	}

	// The land's tiles: fine near what the eye looks at, coarse far from it.
	UpdateLand(CameraDistance, CameraFocus, false);

	// Pieces cast shadows only near what the eye is looking at, and not at all from far off.
	{
		const double ShadowReach = CameraDistance * 1.1 + 500.0;
		const bool bAny = CameraDistance < 7000.0f;
		auto Shade = [&](AArmyPiece* Piece)
		{
			if (Piece)
			{
				Piece->SetShadows(bAny && FVector::DistSquared2D(Piece->GetActorLocation(), CameraFocus) < ShadowReach * ShadowReach);
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
	for (int32 Index = 0; Index < TreeMeshes.Num(); ++Index)
	{
		const bool bShow = bNear && FVector::DistSquared2D(TreeCentres[Index], CameraFocus) < Reach * Reach;
		if (bShow != TreesShown[Index])
		{
			TreesShown[Index] = bShow;
			if (TreeMeshes[Index])
			{
				TreeMeshes[Index]->SetVisibility(bShow);
			}
		}
	}

	// The woods are one dense canopy from working height and above (the nation's stain lies over it at half
	// strength, so a wood is still seen to be its owner's); close to, where single trees stand on it, the cloth
	// under them is laid a little thinner.
	const float Woods = FMath::Lerp(0.66f, 0.84f, FMath::SmoothStep(3200.0f, 6500.0f, CameraDistance));
	if (LandMaterial && FMath::Abs(Woods - WoodsShown) > 0.01f)
	{
		WoodsShown = Woods;
		LandMaterial->SetScalarParameterValue(TEXT("WoodsStrength"), Woods);
	}
	// The quilt of fields: plain to see close to, quieter at working height (where it must not fight the province
	// lines), and fading out toward the top of the zoom. Its hedges are seen only close to.
	const float Fields = CameraDistance < 9000.0f ? FMath::GetMappedRangeValueClamped(FVector2f(3000.0f, 9000.0f), FVector2f(FieldsNear, FieldsWorking), CameraDistance)
		: FMath::GetMappedRangeValueClamped(FVector2f(9000.0f, 19000.0f), FVector2f(FieldsWorking, FieldsWorking * 0.45f), CameraDistance);
	const float Hedges = FMath::GetMappedRangeValueClamped(FVector2f(2600.0f, 7000.0f), FVector2f(HedgesNear, 0.0f), CameraDistance);
	if (LandMaterial && FMath::Abs(Fields - FieldsShown) > 0.005f)
	{
		FieldsShown = Fields;
		LandMaterial->SetScalarParameterValue(TEXT("FieldsStrength"), Fields);
	}
	if (LandMaterial && FMath::Abs(Hedges - HedgesShown) > 0.005f)
	{
		HedgesShown = Hedges;
		LandMaterial->SetScalarParameterValue(TEXT("HedgeStrength"), Hedges);
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
				LandMaterial->SetScalarParameterValue(TEXT("Band"), Band);
			}
		}
		const float Wash = (CameraDistance > 9000.0f ? Far : Near) * Strengths[FMath::Clamp(ColourStrength, 0, 2)] * Tried;
		if (LandMaterial && FMath::Abs(Wash - WashShown) > 0.004f)
		{
			WashShown = Wash;
			LandMaterial->SetScalarParameterValue(TEXT("Wash"), Wash);
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
				Piece->SetActorRotation(FRotator(0.0f, CameraYaw, 0.0f));
			}
		}
		for (const auto& Pair : FleetPieces)
		{
			if (Pair.Value)
			{
				Pair.Value->SetActorRotation(FRotator(0.0f, CameraYaw, 0.0f));
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
	}
}

void ACampaignMapActor::SetHoveredRegion(int32 RegionId)
{
	if (RegionId != HoveredRegion)
	{
		HoveredRegion = RegionId;
		RebuildHighlight();
	}
}

void ACampaignMapActor::RebuildHighlight()
{
	HighlightMesh->ClearAllMeshSections();
	if (SelectedRegion != INDEX_NONE)
	{
		// (a light wash and a bright edge: the ground keeps the colour of whoever holds it while it is picked)
		AddRegionOverlay(SelectedRegion, FLinearColor(1.0f, 0.82f, 0.42f, 0.11f), FLinearColor(1.0f, 0.9f, 0.62f, 0.95f), 0);
	}
	if (HoveredRegion != INDEX_NONE && HoveredRegion != SelectedRegion)
	{
		AddRegionOverlay(HoveredRegion, FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), FLinearColor(1.0f, 1.0f, 1.0f, 0.55f), 2);
	}
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
	auto VertexAt = [&](int32 I, int32 J)
	{
		int32& Slot = Remap[(J - J0) * Stride + (I - I0)];
		if (Slot == INDEX_NONE)
		{
			FLinearColor C = Fill;
			C.A = Mine(I, J) ? Fill.A : 0.0f;
			const double X = FMath::Min(I * Cell, Data.Width), Y = FMath::Min(J * Cell, Data.Height);
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

void ACampaignMapActor::AddRegionOverlay(int32 RegionId, const FLinearColor& Fill, const FLinearColor& Outline, int32 FirstSection)
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
		FCampaignMeshBuffers Buffers;
		for (const TArray<FVector2D>& Ring : Data.Regions[RegionId].Polygons)
		{
			AppendRibbon(Buffers, Ring, 0.42, 0.24, Outline.ToFColor(false), true);
		}
		Buffers.Commit(HighlightMesh, FirstSection + 1, HighlightMaterial);
	}
}
