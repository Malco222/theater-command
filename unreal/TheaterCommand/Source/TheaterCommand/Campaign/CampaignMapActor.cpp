#include "Campaign/CampaignMapActor.h"

#include "Campaign/ArmyPiece.h"
#include "TheaterCommand.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
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

	/** The colour of the bare ground for each kind of country, before any nation is painted over it. */
	FLinearColor GroundColor(const FString& Terrain)
	{
		if (Terrain == TEXT("forest")) return FromHex(TEXT("6C7F55"));
		if (Terrain == TEXT("hills")) return FromHex(TEXT("A39670"));
		if (Terrain == TEXT("mountain")) return FromHex(TEXT("8D8676"));
		if (Terrain == TEXT("urban")) return FromHex(TEXT("9C9488"));
		if (Terrain == TEXT("desert")) return FromHex(TEXT("D3BC86"));
		if (Terrain == TEXT("marsh")) return FromHex(TEXT("7C8F78"));
		return FromHex(TEXT("A8A777")); // plains
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
	TerrainMesh->SetCastShadow(FParse::Param(FCommandLine::Get(), TEXT("TheaterLandShadows")));

	LoadMaterials();
	Terrain.Build(Data);
	ComputeVertexColors();
	BuildTerrainMesh();
	BuildSeaMesh();
	BuildBorders();
	BuildTable();
	BuildTownMarkers();
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

	auto MakeSurface = [this](float Roughness, float Specular, float Grain, float Tooth)
	{
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(SurfaceMaterial, this);
		Instance->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		Instance->SetScalarParameterValue(TEXT("Specular"), Specular);
		Instance->SetScalarParameterValue(TEXT("Grain"), Grain);
		Instance->SetScalarParameterValue(TEXT("Tooth"), Tooth);
		return Instance;
	};
	LandMaterial = MakeSurface(0.92f, 0.22f, 0.16f, 0.4f); // matt, like painted plaster
	SeaMaterial = MakeSurface(0.14f, 0.75f, 0.05f, 0.0f); // glossy and smooth, like poured resin
	InkMaterial = MakeSurface(0.8f, 0.2f, 0.0f, 0.0f);

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

void ACampaignMapActor::ComputeVertexColors()
{
	const int32 NX = Terrain.NX, NY = Terrain.NY, Count = NX * NY;

	// The ground colour of each kind of country, softened across region boundaries.
	TArray<FLinearColor> RegionGround;
	RegionGround.SetNum(Data.Regions.Num());
	for (const FCampaignRegion& Region : Data.Regions)
	{
		RegionGround[Region.Id] = GroundColor(Region.Terrain);
	}
	const FLinearColor Unclaimed = FromHex(TEXT("A49C86"));
	TArray<float> R, G, B;
	R.SetNumUninitialized(Count);
	G.SetNumUninitialized(Count);
	B.SetNumUninitialized(Count);
	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		const int16 Id = Terrain.Region[Idx];
		const FLinearColor& C = Id >= 0 ? RegionGround[Id] : Unclaimed;
		R[Idx] = C.R;
		G[Idx] = C.G;
		B[Idx] = C.B;
	}
	const int32 Soften = FMath::RoundToInt32(4.0 / Terrain.Cell);
	FCampaignTerrain::Smooth(R, NX, NY, Soften, 2);
	FCampaignTerrain::Smooth(G, NX, NY, Soften, 2);
	FCampaignTerrain::Smooth(B, NX, NY, Soften, 2);
	const float PlainsLightness = GroundColor(TEXT("plains")).GetLuminance();

	// Each nation's colour, looked up once per region.
	TArray<FLinearColor> RegionTint;
	RegionTint.SetNum(Data.Regions.Num());
	for (const FCampaignRegion& Region : Data.Regions)
	{
		const FCampaignNation* Nation = Data.FindNation(Region.Owner);
		RegionTint[Region.Id] = Nation ? FLinearColor(Nation->Color) : Unclaimed;
	}

	// How each point stands against the ground around it: crests come out lighter, valley floors darker.
	TArray<float> Surround = Terrain.Height;
	FCampaignTerrain::Smooth(Surround, NX, NY, FMath::RoundToInt32(5.0 / Terrain.Cell), 2);

	// The direction the lamp shines from, in map terms (x east, y south, z up): low in the west-south-west.
	// The relief is shaded into the paint as well as lit by the lamp, the way a printed relief map is,
	// so that gentle country still reads as country.
	const FVector3f ToLamp = FVector3f(-0.80f, 0.30f, 0.52f).GetSafeNormal();
	const float Exaggerate = 1.8f;

	const FLinearColor Rock = FromHex(TEXT("857D70"));
	const FLinearColor Snow = FromHex(TEXT("F2F0EA"));
	const FLinearColor Sand = FromHex(TEXT("D9CBA4"));

	VertexColors.SetNumUninitialized(Count);
	for (int32 J = 0; J < NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const int32 Idx = Terrain.Index(I, J);
			const FLinearColor Ground(R[Idx], G[Idx], B[Idx]);
			FLinearColor C = Ground;

			// The nation's colour is painted over the ground. The kind of country shows through as light
			// and dark (forest darker, desert paler) and as a trace of its own colour; along the frontier
			// the paint is laid on thick.
			const int16 Id = Terrain.Region[Idx];
			if (Id >= 0)
			{
				const FLinearColor& Tint = RegionTint[Id];
				const float Lightness = FMath::Lerp(1.0f, Ground.GetLuminance() / PlainsLightness, 0.55f);
				const FLinearColor Painted = FMath::Lerp(Tint * Lightness, Ground, 0.2f);
				const float Edge = 1.0f - FMath::SmoothStep(0.6f, 4.5f, Terrain.BorderDistance[Idx] * static_cast<float>(Terrain.Cell));
				C = FMath::Lerp(Painted, Tint * 0.8f, 0.6f * Edge);
			}

			// High ground turns to bare rock, and the highest crests to snow.
			const float H = Terrain.Height[Idx];
			const FVector2f Slope = Terrain.GradientAt(I, J);
			const float Steepness = FMath::Clamp(Slope.Size() * 0.9f, 0.0f, 1.0f);
			C = FMath::Lerp(C, Rock, 0.7f * FMath::SmoothStep(1.6f, 4.2f, H + Steepness));
			C = FMath::Lerp(C, Snow, 0.9f * FMath::SmoothStep(5.0f, 6.6f, H) * (1.0f - 0.6f * Steepness));

			// A narrow strip of sand along the shore.
			C = FMath::Lerp(C, Sand, 0.45f * (1.0f - FMath::SmoothStep(0.2f, 1.8f, Terrain.ShoreDistance[Idx])));

			// Shaded relief.
			const FVector3f Normal = FVector3f(-Slope.X * Exaggerate, -Slope.Y * Exaggerate, 1.0f).GetSafeNormal();
			const float Lit = FMath::Clamp(FVector3f::DotProduct(Normal, ToLamp) / ToLamp.Z, 0.35f, 1.45f);
			const float Standing = FMath::Clamp(1.0f + (H - Surround[Idx]) * 0.16f, 0.72f, 1.18f);
			const float Shade = FMath::Lerp(1.0f, Lit, 0.5f) * Standing;

			// A little unevenness, so that large areas do not look printed.
			const float Grain = 1.0f + 0.03f * FMath::PerlinNoise2D(FVector2D(I * 0.61, J * 0.61));
			C.R *= Shade * Grain;
			C.G *= Shade * Grain;
			C.B *= Shade * Grain;
			C.A = 1.0f;
			VertexColors[Idx] = C.GetClamped().ToFColor(true);
		}
	}
}

void ACampaignMapActor::BuildTerrainMesh()
{
	TerrainMesh->ClearAllMeshSections();

	const int32 NX = Terrain.NX, NY = Terrain.NY;
	const int32 ChunkCells = 96;
	int32 Section = 0;
	int32 TriangleCount = 0;
	TArray<int32> Remap;

	for (int32 CJ = 0; CJ < NY - 1; CJ += ChunkCells)
	{
		for (int32 CI = 0; CI < NX - 1; CI += ChunkCells)
		{
			const int32 I1 = FMath::Min(CI + ChunkCells, NX - 1);
			const int32 J1 = FMath::Min(CJ + ChunkCells, NY - 1);
			const int32 Stride = I1 - CI + 1;
			Remap.Init(INDEX_NONE, Stride * (J1 - CJ + 1));
			FCampaignMeshBuffers Buffers;

			auto VertexAt = [&](int32 I, int32 J)
			{
				int32& Slot = Remap[(J - CJ) * Stride + (I - CI)];
				if (Slot == INDEX_NONE)
				{
					const int32 Idx = Terrain.Index(I, J);
					const double X = FMath::Min(I * Terrain.Cell, Data.Width);
					const double Y = FMath::Min(J * Terrain.Cell, Data.Height);
					const FVector2f Slope = Terrain.GradientAt(I, J) * VerticalScale;
					const FVector Normal = FVector(Slope.Y, -Slope.X, 1.0).GetSafeNormal();
					const FVector Tangent = FVector(0.0, 1.0, Slope.X).GetSafeNormal();
					Slot = Buffers.AddVertex(MapToWorld(X, Y, Terrain.Height[Idx]), Normal, VertexColors[Idx],
						FVector2D(X / Data.Width, Y / Data.Height), Tangent);
				}
				return Slot;
			};

			for (int32 J = CJ; J < J1; ++J)
			{
				for (int32 I = CI; I < I1; ++I)
				{
					// Sea bed well away from any shore is never seen: leave it out.
					const float Most = FMath::Max(
						FMath::Max(Terrain.Coverage[Terrain.Index(I, J)], Terrain.Coverage[Terrain.Index(I + 1, J)]),
						FMath::Max(Terrain.Coverage[Terrain.Index(I, J + 1)], Terrain.Coverage[Terrain.Index(I + 1, J + 1)]));
					if (Most < 0.3f)
					{
						continue;
					}
					const int32 A = VertexAt(I, J), B = VertexAt(I + 1, J), C = VertexAt(I, J + 1), D = VertexAt(I + 1, J + 1);
					Buffers.Triangles.Append({ A, C, B, B, C, D });
				}
			}

			if (Buffers.Commit(TerrainMesh, Section, LandMaterial))
			{
				++Section;
				TriangleCount += Buffers.Triangles.Num() / 3;
			}
		}
	}
	UE_LOG(LogTheater, Log, TEXT("Land: %d triangles in %d pieces"), TriangleCount, Section);
}

void ACampaignMapActor::BuildSeaMesh()
{
	SeaMesh->ClearAllMeshSections();

	const double Step = 3.0;
	const int32 NX = FMath::CeilToInt32(Data.Width / Step) + 1;
	const int32 NY = FMath::CeilToInt32(Data.Height / Step) + 1;
	const FLinearColor Deep = FromHex(TEXT("15304A"));
	const FLinearColor Shallow = FromHex(TEXT("356E80"));

	FCampaignMeshBuffers Buffers;
	Buffers.Vertices.Reserve(NX * NY);
	for (int32 J = 0; J < NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const double X = FMath::Min(I * Step, Data.Width);
			const double Y = FMath::Min(J * Step, Data.Height);
			// The water pales toward every coast, the way a chart shows shoal water.
			const float OffShore = -Terrain.ShoreAt(X, Y);
			const float Depth = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(OffShore / 9.0f, 0.0f, 1.0f));
			const FLinearColor C = FMath::Lerp(Shallow, Deep, FMath::Pow(Depth, 0.7f));
			Buffers.AddVertex(MapToWorld(X, Y, 0.0), FVector::UpVector, C.ToFColor(true), FVector2D(X / Data.Width, Y / Data.Height));
		}
	}
	for (int32 J = 0; J < NY - 1; ++J)
	{
		for (int32 I = 0; I < NX - 1; ++I)
		{
			const int32 A = J * NX + I, B = A + 1, C = A + NX, D = C + 1;
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
					const int32 Left = Buffers.AddVertex(SurfacePoint(L.X, L.Y, Lift), FVector::UpVector, Ink);
					const int32 Right = Buffers.AddVertex(SurfacePoint(R.X, R.Y, Lift), FVector::UpVector, Ink);
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

void ACampaignMapActor::BuildTownMarkers()
{
	TownMarkers->ClearInstances();
	CapitalMarkers->ClearInstances();
	TownMarkers->SetStaticMesh(CylinderMesh);
	CapitalMarkers->SetStaticMesh(CylinderMesh);
	TownMarkers->SetMaterial(0, GetSolidMaterial(FromHex(TEXT("2A2420")), 0.6f, 0.0f));
	CapitalMarkers->SetMaterial(0, GetSolidMaterial(FromHex(TEXT("C9A227")), 0.3f, 1.0f));

	const double U = UnitScale;
	for (const FCampaignRegion& Region : Data.Regions)
	{
		const bool bTown = Region.bCapital || Region.Terrain == TEXT("urban") || Region.VictoryPoints >= 2;
		if (!bTown)
		{
			continue;
		}
		// The engine's cylinder is one metre across and one metre tall.
		const double Diameter = (Region.bCapital ? 2.3 : 1.5) * U;
		const double Height = (Region.bCapital ? 0.5 : 0.35) * U;
		const FVector Location = SurfacePoint(Region.Center.X, Region.Center.Y, 0.0) + FVector(0.0, 0.0, Height * 0.5);
		const FTransform Transform(FRotator::ZeroRotator, Location, FVector(Diameter / 100.0, Diameter / 100.0, Height / 100.0));
		(Region.bCapital ? CapitalMarkers : TownMarkers)->AddInstance(Transform, true);
	}
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
	FVector2D Spot = Region.Center + FVector2D(5.5 + 9.0 * Slot, -4.5);
	if (Data.FindRegionAt(Spot.X, Spot.Y) != Region.Id)
	{
		Spot = Region.Center + FVector2D(-5.5 - 9.0 * Slot, -4.5);
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

void ACampaignMapActor::ShowArmies(const FCampaignView& View, const TArray<FCorpsMove>& Marches)
{
	UWorld* World = GetWorld();
	if (!World || !Data.IsLoaded())
	{
		return;
	}

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
		Road.Add(*From);
		for (int32 i = 0; i + 1 < Lead.Path.Num(); ++i)
		{
			if (Data.Regions.IsValidIndex(Lead.Path[i]))
			{
				Road.Add(ArmySpot(Lead.Path[i], 0));
			}
		}
		Road.Add(Piece->GetRestLocation());

		const FString Key = FString::Printf(TEXT("%d|%s"), Piece->GetRegionId(), *Piece->GetNationTag());
		if (!KeysBefore.Contains(Key))
		{
			// Nobody of theirs was there before: the piece itself makes the march.
			Piece->MarchAlong(Road, false);
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
			if (AArmyPiece* Courier = World->SpawnActor<AArmyPiece>(*From, Piece->GetActorRotation()))
			{
				Courier->SetArmy(this, *Nation, Piece->GetRegionId(), Travellers);
				Courier->MarchAlong(Road, true);
			}
		}
	}
}

void ACampaignMapActor::SetHolders(const TArray<FString>& Holders)
{
	bool bChanged = false;
	for (int32 i = 0; i < Data.Regions.Num() && i < Holders.Num(); ++i)
	{
		if (!Holders[i].IsEmpty() && Data.Regions[i].Owner != Holders[i])
		{
			Data.Regions[i].Owner = Holders[i];
			bChanged = true;
		}
	}
	if (bChanged)
	{
		// Ground has changed hands: repaint it and redraw the frontiers.
		Terrain.MeasureFrontiers(Data);
		ComputeVertexColors();
		BuildTerrainMesh();
		BuildBorders();
	}
}

void ACampaignMapActor::SetReach(const TArray<FReachRegion>& Reach)
{
	OrdersMesh->ClearMeshSection(0);
	if (Reach.Num() == 0)
	{
		return;
	}
	// Ground the army can march to is lightened a little; ground it could attack is marked in red and ringed.
	FCampaignMeshBuffers Buffers;
	const FLinearColor March(0.8f, 1.0f, 0.82f, 0.13f);
	const FLinearColor Attack(0.9f, 0.06f, 0.03f, 0.42f);
	const FColor AttackRing = FLinearColor(1.0f, 0.25f, 0.18f, 0.9f).ToFColor(false);
	for (const FReachRegion& Entry : Reach)
	{
		AppendRegionFill(Buffers, Entry.Region, Entry.bAttack ? Attack : March);
		if (Entry.bAttack && Data.Regions.IsValidIndex(Entry.Region))
		{
			for (const TArray<FVector2D>& Ring : Data.Regions[Entry.Region].Polygons)
			{
				AppendRibbon(Buffers, Ring, 0.34, 0.26, AttackRing, true);
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

void ACampaignMapActor::UpdateForCamera(float CameraDistance, float CameraYaw)
{
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
			const int32 Left = Buffers.AddVertex(SurfacePoint(L.X, L.Y, Lift), FVector::UpVector, Ink);
			const int32 Right = Buffers.AddVertex(SurfacePoint(R.X, R.Y, Lift), FVector::UpVector, Ink);
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
