#include "Campaign/CampaignMapActor.h"

#include "Campaign/CampaignMiniatures.h"
#include "Campaign/TheaterSpeed.h"
#include "TheaterCommand.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

/*
 * (Release 2: supply columns. Brian said yes on 23 November 2026, 12:05Z: "lorry convoys crawl along the campaign
 * map's roads, and enemy planes or partisans can ambush them". The rules are src/campaign/columns.ts.)
 *
 * Each month every nation at war sends a column of lorries from its railhead along the road to the armies standing
 * beyond it. The rules hand the table the columns the player can see: the road, province by province, whose it is, the
 * lorries sent and lost, and where they were caught. The table draws:
 *
 *   - the road, a pale track on a dark bed, in the colour of whose it is: sand for the player's own, straw for a
 *     friend's, brick for the enemy's, and amber where less than 85 in a hundred of what was sent got through;
 *   - convoys on it, three lorries nose to tail in the nation's colour, crawling from the railhead to the front (one
 *     convoy for every hundred and fifty lorries or so, four at most), only near the eye, as the trains are;
 *   - and where a column was caught, a burnt-out lorry slewed across the road with its fire and black smoke (the
 *     smoke is drawn with the rest, in UpdateSmoke).
 *
 * The roads and the wrecks are one still model, made again only when the month's columns change. With the columns off
 * the rules send none, and nothing here is drawn.
 */

namespace
{
	FLinearColor ColumnHex(const TCHAR* Hex, float Alpha = 1.0f)
	{
		FLinearColor Colour(FColor::FromHex(Hex)); // sRGB in, linear out
		Colour.A = Alpha;
		return Colour;
	}

	/** The road as a smooth curve through the provinces it runs through, a point every two map units or so. */
	void SmoothRoad(const TArray<FVector2D>& Path, TArray<FVector2D>& Curve)
	{
		Curve.Reset();
		const int32 Count = Path.Num();
		for (int32 K = 0; K + 1 < Count; ++K)
		{
			const FVector2D P0 = Path[FMath::Max(K - 1, 0)], P1 = Path[K], P2 = Path[K + 1], P3 = Path[FMath::Min(K + 2, Count - 1)];
			const int32 Steps = FMath::Clamp(FMath::CeilToInt32(FVector2D::Distance(P1, P2) / 2.0), 2, 200);
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const double T = static_cast<double>(Step) / Steps;
				Curve.Add(0.5 * ((2.0 * P1) + (P2 - P0) * T + (2.0 * P0 - 5.0 * P1 + 4.0 * P2 - P3) * T * T + (3.0 * P1 - P0 - 3.0 * P2 + P3) * T * T * T));
			}
		}
		if (Count > 0)
		{
			Curve.Add(Path.Last());
		}
	}

	/** Mesh buffers of its own (the map's are kept to CampaignMapActor.cpp). */
	struct FColumnInk
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FColor> Colors;

		int32 Add(const FVector& At, const FColor& Colour)
		{
			Vertices.Add(At);
			Normals.Add(FVector::UpVector);
			Colors.Add(Colour);
			return Vertices.Num() - 1;
		}

		/** A triangle wound so that its face looks up. */
		void Face(int32 A, int32 B, int32 C)
		{
			const FVector Normal = (Vertices[B] - Vertices[C]) ^ (Vertices[A] - Vertices[C]);
			Triangles.Add(A);
			Triangles.Add(Normal.Z >= 0.0 ? B : C);
			Triangles.Add(Normal.Z >= 0.0 ? C : B);
		}
	};
}

void ACampaignMapActor::ParkColumnConvoys(int32 Road)
{
	for (int32 Index = ColumnConvoys.Num() - 1; Index >= 0; --Index)
	{
		if (Road == INDEX_NONE || ColumnConvoys[Index].Road == Road)
		{
			if (ColumnConvoys[Index].Mesh)
			{
				ColumnConvoys[Index].Mesh->DestroyComponent();
			}
			ColumnConvoys.RemoveAt(Index);
		}
	}
	if (ColumnRoads.IsValidIndex(Road))
	{
		ColumnRoads[Road].bManned = false;
	}
	else if (Road == INDEX_NONE)
	{
		for (FColumnRoad& Each : ColumnRoads)
		{
			Each.bManned = false;
		}
	}
}

void ACampaignMapActor::ShowColumns(const TArray<FViewColumn>& Columns)
{
	THEATER_SPEED_SCOPE(ShowColumns);
	if (!Data.IsLoaded())
	{
		return;
	}
	FString Key;
	for (const FViewColumn& Column : Columns)
	{
		if (Column.Path.Num() < 2)
		{
			continue;
		}
		Key += FString::Printf(TEXT("%s%s%d:%.0f,%.0f>%.0f,%.0f:%d/%d:%d;"), *Column.Nation, *Column.Side, Column.Path.Num(), Column.Path[0].X, Column.Path[0].Y,
			Column.Path.Last().X, Column.Path.Last().Y, Column.Lost, Column.Lorries, Column.Hits.Num());
	}
	if (Key == ShownColumns)
	{
		return;
	}
	ShownColumns = Key;
	ParkColumnConvoys(INDEX_NONE);
	ColumnRoads.Reset();
	ColumnFires.Reset();
	if (Columns.Num() == 0 && !ColumnsMesh)
	{
		return; // (the columns are off: nothing is made at all)
	}
	if (!ColumnsMesh)
	{
		ColumnsMesh = NewObject<UProceduralMeshComponent>(this, TEXT("SupplyColumns"));
		ColumnsMesh->SetupAttachment(Root);
		ColumnsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ColumnsMesh->SetCastShadow(false);
		ColumnsMesh->RegisterComponent();
	}
	ColumnsMesh->ClearAllMeshSections();

	const double U = UnitScale;
	FColumnInk Roads;
	FMiniatureMesh Wrecks;
	const FColor Bed = ColumnHex(TEXT("281C0C"), 0.42f).ToFColor(false);
	for (const FViewColumn& Column : Columns)
	{
		if (Column.Path.Num() < 2)
		{
			continue;
		}
		FColumnRoad& Road = ColumnRoads.AddDefaulted_GetRef();
		Road.Nation = Column.Nation;
		SmoothRoad(Column.Path, Road.Curve);
		Road.Run.Reset(Road.Curve.Num());
		Road.Run.Add(0.0);
		for (int32 K = 1; K < Road.Curve.Num(); ++K)
		{
			Road.Run.Add(Road.Run.Last() + FVector2D::Distance(Road.Curve[K - 1], Road.Curve[K]));
		}
		Road.Length = Road.Run.Last();
		Road.Convoys = FMath::Clamp(FMath::RoundToInt32(Column.Lorries / 150.0f), 1, 4);

		// The road: a dark bed under a pale track, laid over the relief a step at a time.
		const FColor Track = (Column.Share < 0.85f ? ColumnHex(TEXT("E9B548"), 0.85f)
			: Column.Side == TEXT("theirs") ? ColumnHex(TEXT("C98A7A"), 0.8f)
			: Column.Side == TEXT("friend") ? ColumnHex(TEXT("C8C19A"), 0.75f)
			: ColumnHex(TEXT("C9A35E"), 0.85f)).ToFColor(false);
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const double Half = Pass == 0 ? 0.62 : 0.24, Lift = Pass == 0 ? 0.22 : 0.3;
			const FColor Ink = Pass == 0 ? Bed : Track;
			int32 PrevLeft = INDEX_NONE, PrevRight = INDEX_NONE;
			for (int32 K = 0; K < Road.Curve.Num(); ++K)
			{
				const FVector2D Before = Road.Curve[FMath::Max(K - 1, 0)], After = Road.Curve[FMath::Min(K + 1, Road.Curve.Num() - 1)];
				const FVector2D Dir = (After - Before).GetSafeNormal();
				if (Dir.IsNearlyZero())
				{
					continue;
				}
				const FVector2D Side(Dir.Y, -Dir.X);
				const FVector2D L = Road.Curve[K] + Side * Half, R = Road.Curve[K] - Side * Half;
				const int32 Left = Roads.Add(SurfacePointOver(L.X, L.Y, Lift), Ink);
				const int32 Right = Roads.Add(SurfacePointOver(R.X, R.Y, Lift), Ink);
				if (PrevLeft != INDEX_NONE)
				{
					Roads.Face(PrevLeft, PrevRight, Left);
					Roads.Face(PrevRight, Left, Right);
				}
				PrevLeft = Left;
				PrevRight = Right;
			}
		}

		// Where it was caught: a burnt-out lorry slewed across the road (two where many were lost), and its fire.
		const FLinearColor Char = ColumnHex(TEXT("1E1B18"));
		for (const FViewColumnHit& Hit : Column.Hits)
		{
			// (the road's heading where it was caught, to slew the wreck across it)
			int32 Nearest = 0;
			double Best = TNumericLimits<double>::Max();
			for (int32 K = 0; K < Road.Curve.Num(); ++K)
			{
				const double Far = FVector2D::DistSquared(Road.Curve[K], Hit.At);
				if (Far < Best)
				{
					Best = Far;
					Nearest = K;
				}
			}
			const FVector2D RoadFore = Road.Curve[FMath::Min(Nearest + 1, Road.Curve.Num() - 1)], RoadAft = Road.Curve[FMath::Max(Nearest - 1, 0)];
			const FVector2D Way = RoadFore - RoadAft;
			// (the turn is a yaw in the world, which lies a quarter turn from the map: the way is read in the world, as the convoys' is)
			const FVector Downroad = (MapToWorld(RoadFore.X, RoadFore.Y) - MapToWorld(RoadAft.X, RoadAft.Y)).GetSafeNormal2D();
			const double Heading = FMath::RadiansToDegrees(FMath::Atan2(Downroad.Y, Downroad.X));
			const int32 Seed = FMath::Abs(FMath::RoundToInt32(Hit.At.X * 7.0 + Hit.At.Y * 13.0));
			const FVector2D At = Road.Curve[Nearest];
			// (a figure not turned faces along -X: a turn of 180 more faces it down the road, and the slew is added)
			Wrecks.Lorry(SurfacePoint(At.X, At.Y, 0.05), 0.85 * U, Char, Heading + 180.0 + 35.0 + (Seed % 40));
			if (Hit.Lost >= 120)
			{
				const FVector2D Behind = At - Way.GetSafeNormal() * 2.6;
				Wrecks.Lorry(SurfacePoint(Behind.X, Behind.Y, 0.05), 0.85 * U, Char, Heading + 180.0 - 30.0 - (Seed % 25));
			}
			const float Size = FMath::Clamp(Hit.Lost / 80.0f, 0.55f, 1.5f) * (Hit.bByAir ? 1.0f : 0.8f);
			const FVector Fire = SurfacePoint(At.X, At.Y, 0.5);
			ColumnFires.Add(FVector4(Fire.X, Fire.Y, Fire.Z, Size));
		}
	}
	if (Roads.Triangles.Num() > 0)
	{
		if (!LanesInk && HighlightMaterial)
		{
			LanesInk = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
			LanesInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
			ShownNight = -1.0f; // (so that the hour's light is laid on it at once)
		}
		ColumnsMesh->CreateMeshSection(0, Roads.Vertices, Roads.Triangles, Roads.Normals, TArray<FVector2D>(), Roads.Colors, TArray<FProcMeshTangent>(), false);
		ColumnsMesh->SetMaterial(0, LanesInk ? static_cast<UMaterialInterface*>(LanesInk) : static_cast<UMaterialInterface*>(HighlightMaterial));
	}
	if (Wrecks.Triangles.Num() > 0)
	{
		ColumnsMesh->CreateMeshSection(1, Wrecks.Vertices, Wrecks.Triangles, Wrecks.Normals, TArray<FVector2D>(), Wrecks.Colors, TArray<FProcMeshTangent>(), false);
		ColumnsMesh->SetMaterial(1, PieceMaterial);
	}
	{
		// (for whoever checks the columns from the log)
		int32 Caught = 0;
		for (const FViewColumn& Column : Columns)
		{
			Caught += Column.Hits.Num();
		}
		UE_LOG(LogTheater, Log, TEXT("Supply columns: %d roads drawn, caught in %d places"), ColumnRoads.Num(), Caught);
	}
	SetNight(NightNow, bLamplightNow);
}

void ACampaignMapActor::UpdateColumns(const FVector& CameraFocus, float CameraDistance)
{
	THEATER_SPEED_SCOPE(UpdateColumns);
	UWorld* World = GetWorld();
	if (!World || ColumnRoads.Num() == 0)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const double U = UnitScale;
	const FVector2D Looking = WorldToMap(CameraFocus);
	// From far off the lorries are too small to see, and are put away, as the trains are.
	const bool bNear = CameraDistance < 9000.0f;
	const int32 MostConvoys = 32;
	const double Reach = FMath::Clamp(0.5 * CameraDistance / U, 40.0, 200.0);

	// The convoys go out on the roads near the eye, and are taken off those it has left.
	for (int32 Index = 0; Index < ColumnRoads.Num(); ++Index)
	{
		FColumnRoad& Road = ColumnRoads[Index];
		bool bClose = false;
		if (bNear)
		{
			for (int32 K = 0; K < Road.Curve.Num(); K += 4)
			{
				if (FVector2D::DistSquared(Road.Curve[K], Looking) < Reach * Reach)
				{
					bClose = true;
					break;
				}
			}
		}
		if (!bClose && Road.bManned)
		{
			ParkColumnConvoys(Index);
		}
		else if (bClose && !Road.bManned && ColumnConvoys.Num() + Road.Convoys <= MostConvoys && Road.Length > 1.0)
		{
			const FCampaignNation* Whose = Data.FindNation(Road.Nation);
			const FLinearColor Livery = Whose ? FLinearColor(Whose->Color) : ColumnHex(TEXT("6D5A3A"));
			// Three lorries nose to tail, the first at the middle of the model, facing +X (a figure not turned faces -X).
			FMiniatureMesh Model;
			const double Scale = 0.85 * U;
			for (int32 Lorry = 0; Lorry < 3; ++Lorry)
			{
				Model.Lorry(FVector(-Lorry * 2.9 * Scale, 0.0, 0.0), Scale, Livery, 180.0);
			}
			if (Model.Vertices.Num() == 0)
			{
				continue;
			}
			for (int32 Place = 0; Place < Road.Convoys; ++Place)
			{
				FColumnConvoy& Convoy = ColumnConvoys.AddDefaulted_GetRef();
				Convoy.Road = Index;
				Convoy.Place = Place;
				Convoy.Mesh = NewObject<UProceduralMeshComponent>(this);
				Convoy.Mesh->SetupAttachment(Root);
				Convoy.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Convoy.Mesh->SetCastShadow(false); // (a thing that moves and throws a shadow has every shadow near it drawn again each frame)
				Convoy.Mesh->RegisterComponent();
				Convoy.Mesh->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
				Convoy.Mesh->SetMaterial(0, PieceMaterial);
			}
			Road.bManned = true;
		}
	}

	// Each convoy crawls from the railhead to the front, about two map units a second, and sets out again from the
	// railhead when it is there; it grows out of the road as it sets out and sinks into it as it arrives.
	const double Speed = 2.0;
	for (FColumnConvoy& Convoy : ColumnConvoys)
	{
		if (!Convoy.Mesh || !ColumnRoads.IsValidIndex(Convoy.Road))
		{
			continue;
		}
		const FColumnRoad& Road = ColumnRoads[Convoy.Road];
		const double Trip = FMath::Max(Road.Length / Speed, 4.0);
		const double Share = FMath::Frac(Now / Trip + static_cast<double>(Convoy.Place) / Road.Convoys + Convoy.Road * 0.173);
		const double Along = Share * Road.Length;
		int32 K = 1;
		while (K < Road.Run.Num() - 1 && Road.Run[K] < Along)
		{
			++K;
		}
		const double Leg = Road.Run[K] - Road.Run[K - 1];
		const FVector2D At = FMath::Lerp(Road.Curve[K - 1], Road.Curve[K], Leg > 0.001 ? (Along - Road.Run[K - 1]) / Leg : 0.0);
		const FVector Ahead = (MapToWorld(Road.Curve[K].X, Road.Curve[K].Y) - MapToWorld(Road.Curve[K - 1].X, Road.Curve[K - 1].Y)).GetSafeNormal2D();
		const double Heading = FMath::RadiansToDegrees(FMath::Atan2(Ahead.Y, Ahead.X));
		const double Grown = FMath::SmoothStep(0.0, 0.05, Share) * (1.0 - FMath::SmoothStep(0.95, 1.0, Share));
		Convoy.Mesh->SetVisibility(Grown > 0.02);
		Convoy.Mesh->SetRelativeLocationAndRotation(SurfacePointOver(At.X, At.Y, 0.2), FRotator(0.0, Heading, 0.0));
		Convoy.Mesh->SetRelativeScale3D(FVector(FMath::Max(Grown, 0.02)));
	}
}
