#include "Campaign/CampaignMapActor.h"

#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignFlags.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignTextures.h"
#include "TheaterCommand.h"
#include "Components/InstancedStaticMeshComponent.h"
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
	SmokeMesh = MakeMesh(TEXT("Smoke"), false);
	LanesMesh = MakeMesh(TEXT("Lanes"), false);
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

	// The land and the sea take their colour from pictures painted in PaintGround.
	LandMaterial = UMaterialInstanceDynamic::Create(Load(TEXT("/Game/Campaign/Materials/M_Land.M_Land")), this);
	SeaMaterial = UMaterialInstanceDynamic::Create(Load(TEXT("/Game/Campaign/Materials/M_Sea.M_Sea")), this);

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
	WaterPicture = CampaignTextures::Create(Paint.WaterNX, Paint.WaterNY, Paint.Water, true, false, TEXT("TableWater"));

	// The fine detail that repeats over it: grass, tree-tops, rock and sand, and the ripples on the water.
	TArray<FColor> Pixels;
	CampaignTextures::MakeGroundDetail(Pixels);
	DetailPicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableDetail"));
	CampaignTextures::MakeGroundBumps(Pixels);
	BumpsPicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableBumps"));
	CampaignTextures::MakeWaterBumps(Pixels);
	RipplesPicture = CampaignTextures::Create(CampaignTextures::DetailSize, CampaignTextures::DetailSize, Pixels, false, true, TEXT("TableRipples"));

	LandMaterial->SetTextureParameterValue(TEXT("Ground"), GroundPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Tint"), TintPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Relief"), ReliefPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Masks"), MasksPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Detail"), DetailPicture);
	LandMaterial->SetTextureParameterValue(TEXT("Bumps"), BumpsPicture);
	LandMaterial->SetVectorParameterValue(TEXT("RockColour"), FCampaignGround::RockColour());
	LandMaterial->SetVectorParameterValue(TEXT("SnowColour"), FCampaignGround::SnowColour());
	LandMaterial->SetVectorParameterValue(TEXT("SandColour"), FCampaignGround::SandColour());
	SeaMaterial->SetTextureParameterValue(TEXT("Water"), WaterPicture);
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

	const FColor NationalInk = FromHex(TEXT("1B1612")).ToFColor(true);
	const FColor RegionInk = FromHex(TEXT("4A4236")).ToFColor(true);
	const double Lift = 0.13;

	FCampaignMeshBuffers Buffers;
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
				const double HalfWidth = bNational ? 0.5 : 0.15;
				const FColor Ink = bNational ? NationalInk : RegionInk;
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
	}
	Buffers.Commit(BorderMesh, 0, InkMaterial);
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
			static const FVector2D Taken[] = { FVector2D(9.0, -7.0), FVector2D(-9.0, -7.0), FVector2D(-10.0, 8.0), FVector2D(10.0, 9.0), FVector2D(0.0, -9.0), FVector2D(0.0, 10.0), FVector2D(-9.5, 0.5) };
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
		// An airfield: a mown field with a runway (two for a big one), a hangar and a windsock.
		if (Built.W > 0 && Place(FVector2D(0.0, 10.0), Spot))
		{
			const FVector At = SurfacePoint(Spot.X, Spot.Y, 0.02);
			const double Turn = Facing(Spot) + 90.0;
			// The field is a slab thick enough to stand clear of the roll of the ground under it.
			Model.Box(At + FVector(0.0, 0.0, 0.0), FVector(3.4, 1.9, 0.16) * Unit, Turf, Turn);
			Model.Box(At + FVector(0.0, 0.0, 0.085 * Unit), FVector(3.2, 0.32, 0.02) * Unit, Apron, Turn);
			if (Built.W >= 2)
			{
				Model.Box(At + FVector(0.0, 0.0, 0.088 * Unit), FVector(2.4, 0.28, 0.02) * Unit, Apron, Turn + 62.0);
			}
			for (int32 Hangar = 0; Hangar < FMath::Clamp(Built.W, 1, 3); ++Hangar)
			{
				const FVector Foot = At + Ahead(Turn, (Hangar - 1) * 0.85 * Unit, 0.72 * Unit) + FVector(0.0, 0.0, 0.08 * Unit);
				Model.House(Foot, 0.7 * Unit, 0.55 * Unit, 0.22 * Unit, 0.14 * Unit, Tin, Girder, Turn + 90.0);
			}
			const FVector Pole = At + Ahead(Turn, 1.5 * Unit, -0.7 * Unit) + FVector(0.0, 0.0, 0.08 * Unit);
			Model.Pillar(Pole, 0.02 * Unit, 0.02 * Unit, 0.5 * Unit, Timber, 5);
			Model.Rod(Pole + FVector(0.0, 0.0, 0.5 * Unit), Pole + Ahead(Turn, 0.26 * Unit, 0.06 * Unit) + FVector(0.0, 0.0, 0.46 * Unit), 0.035 * Unit, Orange);
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
	// A river is a ribbon of water laid over the relief along its course: narrow at the source, broad at the
	// mouth, its course rounded so it winds instead of turning corners, its banks a shade darker than midstream.
	const FColor Midstream = FromHex(TEXT("3E86B4")).ToFColor(true), Bank = FromHex(TEXT("2B5F86")).ToFColor(true);
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
		int32 Previous[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
		for (int32 K = 0; K < Path.Num(); ++K)
		{
			const FVector2D Along = (Path[FMath::Min(K + 1, Path.Num() - 1)] - Path[FMath::Max(K - 1, 0)]).GetSafeNormal();
			const FVector2D Side(Along.Y, -Along.X);
			const double Half = FMath::Lerp(0.45, 1.25, FMath::Sqrt(static_cast<double>(K) / FMath::Max(Path.Num() - 1, 1)));
			const FVector2D L = Path[K] + Side * Half, R = Path[K] - Side * Half;
			// A river does not run over the sea: stop where the land does.
			if (Terrain.HeightAt(Path[K].X, Path[K].Y) <= 0.0f && K > Path.Num() / 2)
			{
				break;
			}
			const int32 Here[3] = {
				Buffers.AddVertex(SurfacePointOver(L.X, L.Y, Lift), FVector::UpVector, Bank),
				Buffers.AddVertex(SurfacePointOver(Path[K].X, Path[K].Y, Lift), FVector::UpVector, Midstream),
				Buffers.AddVertex(SurfacePointOver(R.X, R.Y, Lift), FVector::UpVector, Bank) };
			if (Previous[0] != INDEX_NONE)
			{
				for (int32 Strip = 0; Strip < 2; ++Strip)
				{
					Buffers.AddTriangleFacingUp(Previous[Strip], Previous[Strip + 1], Here[Strip]);
					Buffers.AddTriangleFacingUp(Previous[Strip + 1], Here[Strip], Here[Strip + 1]);
				}
			}
			Previous[0] = Here[0];
			Previous[1] = Here[1];
			Previous[2] = Here[2];
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

void ACampaignMapActor::BuildRailways()
{
	RailMesh->ClearAllMeshSections();
	// A railway runs from the middle of one region to the middle of the next through the point where it
	// crosses their border: a dark line with sleepers across it, as railways are drawn on maps.
	const FColor Iron = FromHex(TEXT("2E2A27")).ToFColor(true);
	const double Lift = 0.16, Half = 0.17;
	FCampaignMeshBuffers Buffers;
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
				const int32 T0 = Buffers.AddVertex(SurfacePointOver(S0.X - Back.X, S0.Y - Back.Y, Lift), FVector::UpVector, Iron);
				const int32 T1 = Buffers.AddVertex(SurfacePointOver(S1.X - Back.X, S1.Y - Back.Y, Lift), FVector::UpVector, Iron);
				const int32 T2 = Buffers.AddVertex(SurfacePointOver(S0.X + Back.X, S0.Y + Back.Y, Lift), FVector::UpVector, Iron);
				const int32 T3 = Buffers.AddVertex(SurfacePointOver(S1.X + Back.X, S1.Y + Back.Y, Lift), FVector::UpVector, Iron);
				Buffers.AddTriangleFacingUp(T0, T1, T2);
				Buffers.AddTriangleFacingUp(T1, T2, T3);
			}
		}
	}
	Buffers.Commit(RailMesh, 0, InkMaterial);
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
	for (const FCampaignRegion& Region : Data.Regions)
	{
		const bool bCity = Region.Terrain == TEXT("urban");
		const bool bTown = Region.bCapital || bCity || Region.VictoryPoints >= 2 || Region.Factories >= 2;
		if (!bTown)
		{
			continue;
		}
		const int32 Houses = Region.bCapital ? 13 : (bCity ? 10 : 6);
		Model.Town(SurfacePoint(Region.Center.X, Region.Center.Y, 0.02), U * 2.1, Houses, Region.bCapital, bCity, FMath::Min(Region.Factories / 2, 3), Region.Id, &TownChimneys);
		++Count;
	}
	if (Model.Vertices.Num() > 0)
	{
		TownMesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
		TownMesh->SetMaterial(0, SceneryMaterial);
	}
	UE_LOG(LogTheater, Log, TEXT("Towns: %d, in %d points"), Count, Model.Vertices.Num());
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
					if (bFir)
					{
						Model.Fir(Foot, 1.9 * Size * U, 0.55 * Size * U, FLinearColor(0.035f * Shade, 0.085f * Shade, 0.045f * Shade));
					}
					else
					{
						Model.Broadleaf(Foot, 1.5 * Size * U, 0.7 * Size * U, FLinearColor(0.055f * Shade, 0.115f * Shade, 0.038f * Shade));
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
			Mesh->SetMaterial(0, SceneryMaterial);
			Mesh->SetVisibility(false);
			TreeMeshes.Add(Mesh);
			TreeCentres.Add(MapToWorld((SX + 0.5) * Square, (SY + 0.5) * Square));
			TreesShown.Add(false);
		}
	}
	UE_LOG(LogTheater, Log, TEXT("Trees: %d, in %d squares of country"), Trees, TreeMeshes.Num());
}

UTextRenderComponent* ACampaignMapActor::AddLabel(const FString& Text, const FVector2D& MapPos, float SizeUnits, const FColor& Color, int32 Tier, float Spacing)
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
	return Label;
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
	LabelState = -1;

	const FColor Ink = FromHex(TEXT("17130F")).ToFColor(true);
	const FColor FaintInk = FromHex(TEXT("2B251E")).ToFColor(true);
	const FColor SeaInk = FromHex(TEXT("A9C6CE")).ToFColor(true);

	// Towns and regions.
	for (const FCampaignRegion& Region : Data.Regions)
	{
		const bool bTown = Region.bCapital || Region.Terrain == TEXT("urban") || Region.VictoryPoints >= 2;
		if (bTown)
		{
			const float Size = Region.bCapital ? 4.4f : 3.4f;
			// The name sits just south of the town's marker.
			AddLabel(Region.Name, Region.Center + FVector2D(0.0, Size * 0.5 + 2.0), Size, Ink, Region.bCapital ? TierAlways : TierNear);
		}
		else
		{
			AddLabel(Region.Name, Region.Centroid, 2.6f, FaintInk, TierNear);
		}
	}

	// Seas.
	for (const FCampaignSeaZone& Sea : Data.Seas)
	{
		AddLabel(Sea.Name.ToUpper(), Sea.Center, 6.0f, SeaInk, TierAlways, 0.75f);
	}

	// Nations, written large across their home country, for the view from far off.
	struct FNationExtent
	{
		double Area = 0.0;
		FVector2D Weighted = FVector2D::ZeroVector;
	};
	TMap<FString, FNationExtent> Extents;
	for (const FCampaignRegion& Region : Data.Regions)
	{
		if (Region.bColony || Region.Area <= 0.0)
		{
			continue;
		}
		FNationExtent& Extent = Extents.FindOrAdd(Region.Owner);
		Extent.Area += Region.Area;
		Extent.Weighted += Region.Centroid * Region.Area;
	}
	for (const auto& Pair : Extents)
	{
		const FCampaignNation* Nation = Data.FindNation(Pair.Key);
		if (!Nation || Pair.Value.Area < 600.0)
		{
			continue; // too small a country to letter across
		}
		const FVector2D Middle = Pair.Value.Weighted / Pair.Value.Area;
		// Sized so that the whole name fits across the country.
		const double Room = FMath::Sqrt(Pair.Value.Area) * 1.1;
		const float Size = static_cast<float>(FMath::Clamp(Room / (Nation->Name.Len() * 0.95), 6.0, 34.0));
		// Dark lettering on a pale country, pale lettering on a dark one. It is held clear of the mountains
		// and the pieces: it is only shown from high above, where that height does not shift it.
		const bool bPaleCountry = FLinearColor(Nation->Color).GetLuminance() > 0.3f;
		const FColor Lettering = FromHex(bPaleCountry ? TEXT("1A1511") : TEXT("F1EBDC")).ToFColor(true);
		UTextRenderComponent* Label = AddLabel(Nation->Name.ToUpper(), Middle, Size, Lettering, TierFar, 0.35f);
		Label->SetRelativeLocation(MapToWorld(Middle.X, Middle.Y, 30.0));
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

	// Ground has changed hands. Repaint it, with a margin for the band of thick paint along the new
	// frontier, rebuild only the pieces of the land that touches, and redraw the frontier lines.
	Terrain.MeasureFrontiers(Data);
	const double Margin = 8.0;
	const int32 I0 = FMath::Clamp(FMath::FloorToInt32((Changed.Min.X - Margin) / Terrain.Cell), 0, Terrain.NX - 1);
	const int32 I1 = FMath::Clamp(FMath::CeilToInt32((Changed.Max.X + Margin) / Terrain.Cell), 0, Terrain.NX - 1);
	const int32 J0 = FMath::Clamp(FMath::FloorToInt32((Changed.Min.Y - Margin) / Terrain.Cell), 0, Terrain.NY - 1);
	const int32 J1 = FMath::Clamp(FMath::CeilToInt32((Changed.Max.Y + Margin) / Terrain.Cell), 0, Terrain.NY - 1);
	// Only the nations' paint changes: the land itself is left as it is.
	Paint.PaintTint(Data, Terrain, I0, J0, I1, J1);
	CampaignTextures::Update(TintPicture, Paint.Tint);
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
	for (const FReachRegion& Entry : Reach)
	{
		AppendRegionFill(Buffers, Entry.Region, Entry.IsWar() ? War : Entry.bAttack ? Attack : March);
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

void ACampaignMapActor::SetRoute(const FVector& FromWorld, const TArray<int32>& Regions)
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
	const FColor Gold = FLinearColor(1.0f, 0.83f, 0.45f, 0.95f).ToFColor(false);
	AppendRibbon(Buffers, Points, 0.5, 0.34, Gold, false);

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
		const FLinearColor Ink = Lane.State == TEXT("cut") ? FLinearColor(0.95f, 0.14f, 0.1f, 0.62f)
			: Lane.State == TEXT("raided") ? FLinearColor(1.0f, 0.68f, 0.14f, 0.58f)
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
				if (Terrain.HeightAt(Curve[K].X, Curve[K].Y) > 0.0f)
				{
					break;
				}
				Stroke.Add(Curve[K]);
			}
			if (Stroke.Num() >= 2)
			{
				AppendRibbon(Buffers, Stroke, Lane.State == TEXT("open") ? 0.55 : 0.75, 0.12, Ink.ToFColor(false), false);
			}
		}
	}
	Buffers.Commit(LanesMesh, 0, HighlightMaterial);
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

void ACampaignMapActor::UpdateSmoke(float CameraDistance, const FVector& CameraFocus)
{
	UWorld* World = GetWorld();
	if (!SmokeMesh || !World || !HighlightMaterial)
	{
		return;
	}
	const int32 MaxPuffs = 420, Rim = 8, PerPuff = Rim + 1;
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
	}
	// Chimneys: a thin grey plume from each, leaning with the wind. Only those near what the eye is looking at.
	const FLinearColor Grey(0.6f, 0.6f, 0.62f);
	const double Reach = FMath::Min(CameraDistance * 1.4 + 1500.0, 9000.0);
	auto Plume = [&](const FVector& Top, int32 Seed)
	{
		if (FVector::DistSquared2D(Top, CameraFocus) > Reach * Reach)
		{
			return;
		}
		const int32 Count = 6;
		const double Life = 7.0;
		for (int32 K = 0; K < Count; ++K)
		{
			const double Age = FMath::Frac(Now / Life + static_cast<double>(K) / Count + Seed * 0.137);
			const double Sway = FMath::Sin(Now * 0.7 + Seed + K * 1.7) * 0.12 * Unit * Age;
			const FVector At = Top + FVector(0.0, 0.0, 4.0 * Unit * Age) + Wind * (2.8 * Unit * Age * Age) + Right * Sway;
			const float Alpha = 0.55f * FMath::SmoothStep(0.0, 0.1, Age) * FMath::Pow(1.0 - Age, 1.3);
			Draw(At, (0.22 + 1.1 * Age) * Unit, Grey, Alpha);
		}
	};
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
	const bool bNear = CameraDistance < 11500.0f;
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

	// From far off the map is read by its nations' colours, so the woods are laid on thinly; close up they
	// are laid on thick, and the country reads as country.
	const float Woods = FMath::Lerp(0.7f, 0.26f, FMath::SmoothStep(6000.0f, 17000.0f, CameraDistance));
	if (LandMaterial && FMath::Abs(Woods - WoodsShown) > 0.01f)
	{
		WoodsShown = Woods;
		LandMaterial->SetScalarParameterValue(TEXT("WoodsStrength"), Woods);
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
				|| (LabelTiers[i] == TierNear && State == 2);
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
		AddRegionOverlay(SelectedRegion, FLinearColor(1.0f, 0.82f, 0.42f, 0.34f), FLinearColor(1.0f, 0.9f, 0.62f, 0.95f), 0);
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
