// Strikes on the aircraft (Brian, 22 November 2026 04:12Z: "if a plane gets hit make sure the bullet holes and damage
// shows on the wing or fuselage"). Each strike the flight core shows (its Hit) is kept on the aircraft it struck, in the
// aircraft's own frame, for the rest of its flight; while it is near the eye each is laid on its skin as a decal
// (M_HitMark) carried with it: the hole, a lip of torn bare metal, and soot blown back over the paint.

#include "Battle/AirBattle.h"

#include "Battle/AirBattleState.h"
#include "Battle/AirShow.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "TheaterCommand.h"

#include <string>

namespace
{
	/** Which ink a strike wears, from what struck it (a gun's kind, or the ship's gun's): 2 a heavy shell, a 37 or 40 mm
	 * or flak; 1 a cannon shell, 20 or 25 mm; 0 a rifle-calibre or heavy machine-gun round. */
	int32 MarkKindOf(const std::string& What)
	{
		auto Has = [&What](const char* Part) { return What.find(Part) != std::string::npos; };
		if (Has("flak") || Has("heavy") || Has("37") || Has("40") || Has("88") || Has("127") || Has("5in"))
		{
			return 2;
		}
		if (Has("cannon") || Has("20") || Has("25") || Has("30"))
		{
			return 1;
		}
		return 0;
	}

	/** Each kind's soot, its half width in metres, and its hole's share of that; how dark the soot lies, and how torn the
	 * edges. A rifle round's hole is a finger's width and its soot a hand's; a 20 mm shell tears a hole the size of a
	 * plate; a 40 mm or a flak burst's splinters a hole a man could put his head through. Each a little larger than life
	 * (a few pixels at the ride's 30 to 60 m), as the tracers are drawn brighter than life. */
	struct FMarkLook
	{
		double Soot, Hole, Dark, Ragged;
	};
	constexpr FMarkLook MarkLooks[3] = {
		{ 0.12, 0.18, 0.75, 0.30 },
		{ 0.30, 0.30, 0.85, 0.45 },
		{ 0.55, 0.36, 0.95, 0.60 },
	};

	/** A whole number from an aircraft's key and its strike's count: its holes' places, turns and sizes (no draw of the
	 * battle's own, so the battle plays the same with or without the marks). */
	uint32 MarkHash(uint32 A, uint32 B)
	{
		uint32 H = A * 2654435761u ^ (B * 0x9E3779B9u + 0x7F4A7C15u);
		H ^= H >> 15;
		H *= 0x2C1B3C6Du;
		H ^= H >> 12;
		H *= 0x297A2D39u;
		H ^= H >> 15;
		return H;
	}

	/** A strike's place on an aircraft from where the flight core showed it. Every Hit carries the way out from the
	 * aircraft's middle to the strike (Normal) and the aircraft's velocity (Vel), and is shown where the aircraft was
	 * when it was struck: a gunner's strike before its pilots flew the frame, a fighter's or a ship's after. So the
	 * strike lies at Normal * R from where the aircraft was, Vel * Lag behind where it is drawn now: R and Lag are taken
	 * as those that fit best (Lag at most an air second, the x10 clock's longest frame, or half a second the other
	 * way). Returns R in engine units, or a negative number when it does not fit this aircraft. */
	double StrikeReach(const FVector& Off, const FVector& Way, const FVector& Vel, double Span, double& Miss)
	{
		const double B = -FVector::DotProduct(Way, Vel);
		const double C = Vel.SizeSquared();
		const double P = FVector::DotProduct(Way, Off);
		const double Q = -FVector::DotProduct(Vel, Off);
		const double Det = C - B * B;
		double Lag = 0.0;
		if (C > 1.0 && Det > 1e-6 * C)
		{
			Lag = (Q - B * P) / Det;
		}
		Lag = FMath::Clamp(Lag, -0.5, 1.0);
		const FVector Then = Off + Vel * Lag; // (the strike from the middle, where the aircraft was)
		const double R = FVector::DotProduct(Way, Then);
		Miss = FVector::Dist(Then, Way * FMath::Max(R, 0.0));
		return R >= 0.0 && R <= Span * 0.75 && Miss <= Span * 0.25 ? R : -1.0;
	}
}

void AAirBattle::MarkStrike(const AirShow::Fx& E)
{
	using FMark = FPlaneMade::FMark;
	if (!State)
	{
		return;
	}
	FAirBattleState& S = *State;
	const double U = UnitsPerMetre;
	const FVector At = World(E.At.X, E.At.Y, E.At.Z);
	const FVector Way = WorldVector(E.Normal.X, E.Normal.Y, E.Normal.Z).GetSafeNormal();
	const FVector Vel = WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z);
	if (Way.IsNearlyZero())
	{
		return;
	}
	// The aircraft struck: the one the strike names (a ship's), else the one it fits best (a fighter's or a gunner's:
	// in a box the next is 60 m off or more, and a fighter on a bomber's tail is behind its tail).
	const AirShow::PlaneOut* Struck = nullptr;
	double Reach = -1.0, BestMiss = 0.0;
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		if (!P.bShown || P.bOnGround || (E.Key != 0 && P.Key != E.Key))
		{
			continue;
		}
		const FVector Off = At - World(P.At.X, P.At.Y, P.At.Z);
		if (Off.Size() > P.Span * U + Vel.Size() * 1.0)
		{
			continue;
		}
		double Miss = 0.0;
		const double R = StrikeReach(Off, Way, Vel, P.Span * U, Miss);
		if (R >= 0.0 && (!Struck || Miss < BestMiss))
		{
			Struck = &P;
			Reach = R;
			BestMiss = Miss;
		}
	}
	FPlaneMade* Made = Struck ? PlaneMade.Find(Struck->Key) : nullptr;
	if (!Made)
	{
		return;
	}
	// Kept in its mesh's own frame (nose toward -X, right hand toward -Y): its attitude as Draw lays it.
	const FQuat Attitude = FRotator(Struck->Pitch, Struck->Heading, Struck->Bank).Quaternion() * FRotator(0.0, 180.0, 0.0).Quaternion();
	const int32 Kind = MarkKindOf(E.What);
	const FVector Middle = Attitude.UnrotateVector(Way * Reach);
	const FVector Out = Attitude.UnrotateVector(Way);
	++MarkStrikes;
	// A burst puts its rounds in together: a machine-gun strike is a stitch of three holes within about a metre, a
	// cannon strike its tear and a round's hole beside it; a heavy shell's is one tear. Each is laid on the skin by
	// itself (DrawMarks), so a stitch across a wing root runs on over the fuselage.
	const int32 Holes = Kind == 0 ? 3 : Kind == 1 ? 2 : 1;
	for (int32 J = 0; J < Holes; ++J)
	{
		const uint32 Hash = MarkHash(static_cast<uint32>(Struck->Key), static_cast<uint32>(MarkStrikes * 4 + J));
		auto Share = [Hash](int32 Bit) { return static_cast<double>((Hash >> Bit) & 255u) / 255.0; };
		FMark Mark;
		Mark.Kind = J == 0 ? Kind : 0;
		Mark.At = Middle + (J == 0 ? FVector::ZeroVector : FVector(Share(0) - 0.5, Share(8) - 0.5, (Share(16) - 0.5) * 0.3) * 1.2 * U);
		Mark.Out = Out;
		Mark.Soot = MarkLooks[Mark.Kind].Soot * (0.8 + 0.45 * Share(24)) * (J == 0 ? 1.0 : 0.85);
		Mark.Hole = MarkLooks[Mark.Kind].Hole;
		Mark.Turn = static_cast<int32>((Hash >> 4) % 360u);
		if (Made->Marks.Num() < MostMarks)
		{
			Made->Marks.Add(Mark);
			continue;
		}
		// (full: it takes the place of the smallest, when it is the larger; the rest are kept as they were)
		int32 Least = 0;
		for (int32 K = 1; K < Made->Marks.Num(); ++K)
		{
			const FMark& Was = Made->Marks[K];
			const FMark& Low = Made->Marks[Least];
			if (Was.Kind < Low.Kind || (Was.Kind == Low.Kind && Was.Soot < Low.Soot))
			{
				Least = K;
			}
		}
		FMark& Low = Made->Marks[Least];
		if (Low.Kind > Mark.Kind || (Low.Kind == Mark.Kind && Low.Soot >= Mark.Soot))
		{
			continue;
		}
		if (UDecalComponent* Decal = Low.Decal.Get())
		{
			Decal->DestroyComponent();
			MarkDecals = FMath::Max(0, MarkDecals - 1);
		}
		Low = Mark;
	}
	if (MarkStrikes == 1)
	{
		UE_LOG(LogTheater, Display, TEXT("Air battle: strike marks: the first kept on %s (key %d, %s, %.1f m from its middle); laid on the skin of those within %.0f m of the eye and the ridden one, at most %d an aircraft and %d in all."),
			*Made->Model, Struck->Key, Kind == 2 ? TEXT("a heavy shell") : Kind == 1 ? TEXT("a cannon shell") : TEXT("machine-gun rounds"), Reach / U, MarkNearM,
			MostMarks, MostMarkDecals);
	}
}

void AAirBattle::DrawMarks(const FVector& EyeAt)
{
	using FMark = FPlaneMade::FMark;
	if (!State)
	{
		return;
	}
	FAirBattleState& S = *State;
	const double U = UnitsPerMetre;
	if (!bMarkInksTried)
	{
		bMarkInksTried = true;
		if (UMaterialInterface* Mark = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_HitMark.M_HitMark")))
		{
			for (const FMarkLook& Look : MarkLooks)
			{
				UMaterialInstanceDynamic* Ink = UMaterialInstanceDynamic::Create(Mark, this);
				Ink->SetScalarParameterValue(TEXT("Hole"), static_cast<float>(Look.Hole));
				Ink->SetScalarParameterValue(TEXT("Soot"), static_cast<float>(Look.Dark));
				Ink->SetScalarParameterValue(TEXT("Ragged"), static_cast<float>(Look.Ragged));
				MarkInks.Add(Ink);
			}
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Air battle: strike marks: no M_HitMark (TC_REBUILD=M_HitMark Tools/build_content.sh), so strikes are kept but not drawn."));
		}
	}
	const int32 Ridden = S.bClose ? S.Subject.Plane : INDEX_NONE;
	// The aircraft near the eye that carry strikes, the nearest first (when the decals run short, it is the farthest
	// that go without); taken up from the rest.
	struct FNear
	{
		int32 Key;
		double Away;
	};
	TArray<FNear> Near;
	for (TPair<int32, FPlaneMade>& Pair : PlaneMade)
	{
		FPlaneMade& Made = Pair.Value;
		if (Made.Marks.Num() == 0)
		{
			continue;
		}
		const TObjectPtr<UProceduralMeshComponent>* Have = Planes.Find(Pair.Key);
		const UProceduralMeshComponent* Mesh = Have ? Have->Get() : nullptr;
		const double Away = Mesh ? FVector::Dist(Mesh->GetComponentLocation(), EyeAt) / U : 1e9;
		// (on the shape it is drawn with: a sculpted model's near one, which is the one its strikes are laid on)
		const bool bOnShape = !Made.bTwoShapes || Made.bNear;
		if (MarkInks.Num() == static_cast<int32>(UE_ARRAY_COUNT(MarkLooks)) && Mesh && Mesh->IsVisible() && bOnShape && (Pair.Key == Ridden || Away < MarkNearM))
		{
			Near.Add({ Pair.Key, Pair.Key == Ridden ? 0.0 : Away });
		}
		else
		{
			LiftMarks(Made);
		}
	}
	Near.Sort([](const FNear& A, const FNear& B) { return A.Away < B.Away; });
	// Each strike laid on its skin once (the nearest point of the shape it is drawn with, and the way out of the skin
	// there): one aircraft's at a time, its triangles walked once for all of them, so a frame never walks more than one
	// near shape (tens of thousands of triangles).
	bool bWalked = false;
	int32 LaysLeft = 24;
	for (const FNear& It : Near)
	{
		FPlaneMade& Made = PlaneMade[It.Key];
		UProceduralMeshComponent* Mesh = Planes[It.Key];
		TArray<int32> Loose;
		for (int32 K = 0; K < Made.Marks.Num(); ++K)
		{
			if (!Made.Marks[K].bOnSkin)
			{
				Loose.Add(K);
			}
		}
		if (Loose.Num() > 0 && !bWalked)
		{
			bWalked = true;
			const FProcMeshSection* Skin = Mesh->GetProcMeshSection(Made.bTwoShapes ? 1 : 0);
			if (Skin && Skin->ProcIndexBuffer.Num() >= 3)
			{
				const TArray<FProcMeshVertex>& Points = Skin->ProcVertexBuffer;
				const TArray<uint32>& Corners = Skin->ProcIndexBuffer;
				TArray<double> Best;
				TArray<FVector> BestAt, BestOut;
				Best.Init(TNumericLimits<double>::Max(), Loose.Num());
				BestAt.Init(FVector::ZeroVector, Loose.Num());
				BestOut.Init(FVector::UpVector, Loose.Num());
				// (the nearest corner first, which bounds the nearest point; then only the triangles that may hold a nearer)
				for (int32 L = 0; L < Loose.Num(); ++L)
				{
					const FVector& From = Made.Marks[Loose[L]].At;
					for (const FProcMeshVertex& Point : Points)
					{
						const double D = FVector::DistSquared(From, Point.Position);
						if (D < Best[L])
						{
							Best[L] = D;
							BestAt[L] = Point.Position;
							BestOut[L] = Point.Normal;
						}
					}
					Best[L] = FMath::Sqrt(Best[L]);
				}
				for (int32 T = 0; T + 2 < Corners.Num(); T += 3)
				{
					const FProcMeshVertex& A = Points[static_cast<int32>(Corners[T])];
					const FProcMeshVertex& B = Points[static_cast<int32>(Corners[T + 1])];
					const FProcMeshVertex& C = Points[static_cast<int32>(Corners[T + 2])];
					const double Across = FMath::Sqrt(FMath::Max3(FVector::DistSquared(A.Position, B.Position), FVector::DistSquared(B.Position, C.Position),
						FVector::DistSquared(C.Position, A.Position)));
					for (int32 L = 0; L < Loose.Num(); ++L)
					{
						const FVector& From = Made.Marks[Loose[L]].At;
						if (FVector::Dist(From, A.Position) - Across >= Best[L])
						{
							continue;
						}
						const FVector On = FMath::ClosestPointOnTriangleToPoint(From, A.Position, B.Position, C.Position);
						const double D = FVector::Dist(From, On);
						if (D < Best[L])
						{
							FVector Face = FVector::CrossProduct(B.Position - A.Position, C.Position - A.Position).GetSafeNormal();
							if (FVector::DotProduct(Face, A.Normal + B.Normal + C.Normal) < 0.0)
							{
								Face = -Face;
							}
							Best[L] = D;
							BestAt[L] = On;
							BestOut[L] = Face.IsNearlyZero() ? A.Normal : Face;
						}
					}
				}
				for (int32 L = 0; L < Loose.Num(); ++L)
				{
					FMark& Mark = Made.Marks[Loose[L]];
					Mark.At = BestAt[L];
					Mark.Out = BestOut[L].GetSafeNormal(UE_SMALL_NUMBER, Mark.Out);
					Mark.bOnSkin = true;
				}
			}
		}
		for (FMark& Mark : Made.Marks)
		{
			if (!Mark.bOnSkin || Mark.Decal.IsValid() || LaysLeft <= 0 || MarkDecals >= MostMarkDecals)
			{
				continue;
			}
			--LaysLeft;
			UDecalComponent* Decal = NewObject<UDecalComponent>(this, MakeUniqueObjectName(this, UDecalComponent::StaticClass(), *FString::Printf(TEXT("AirMark_%d"), It.Key)));
			Decal->SetupAttachment(Mesh);
			Decal->SetDecalMaterial(MarkInks[Mark.Kind]);
			// (projected into the skin along its own X: deep enough for its curve, and for a shell's to show where it
			// came out of a thin wing's other face too; never faded for its size on the screen)
			const double Half = Mark.Soot * U;
			Decal->DecalSize = FVector(FMath::Max(0.06, Mark.Soot * 0.4) * U, Half, Half);
			Decal->SetFadeScreenSize(0.0f);
			Decal->SortOrder = Mark.Kind;
			FRotator Into = (-Mark.Out).Rotation();
			Into.Roll = static_cast<double>(Mark.Turn);
			Decal->SetRelativeLocationAndRotation(Mark.At, Into);
			Decal->RegisterComponent();
			Mark.Decal = Decal;
			++MarkDecals;
			if (!bMarkLaidLogged)
			{
				bMarkLaidLogged = true;
				UE_LOG(LogTheater, Display, TEXT("Air battle: strike marks: the first laid on the skin of %s (key %d, %.0f m from the eye), %d strikes kept so far."),
					*Made.Model, It.Key, It.Away, MarkStrikes);
			}
		}
	}
}

void AAirBattle::LiftMarks(FPlaneMade& Made)
{
	using FMark = FPlaneMade::FMark;
	for (FMark& Mark : Made.Marks)
	{
		if (UDecalComponent* Decal = Mark.Decal.Get())
		{
			Decal->DestroyComponent();
			MarkDecals = FMath::Max(0, MarkDecals - 1);
		}
		Mark.Decal.Reset();
	}
}

int32 AAirBattle::MostMarked() const
{
	using FMark = FPlaneMade::FMark;
	// (weighed by kind: a cannon shell's tear reads as three rifle rounds, a heavy shell's as six)
	if (!State)
	{
		return INDEX_NONE;
	}
	int32 Best = INDEX_NONE, BestWeight = 0;
	for (const AirShow::PlaneOut& P : State->Show.Planes())
	{
		const FPlaneMade* Made = PlaneMade.Find(P.Key);
		if (!Made || !P.bShown || P.bOnGround || P.bFalling)
		{
			continue;
		}
		int32 Weight = 0;
		for (const FMark& Mark : Made->Marks)
		{
			Weight += Mark.Kind == 2 ? 6 : Mark.Kind == 1 ? 3 : 1;
		}
		if (Weight > BestWeight)
		{
			Best = P.Key;
			BestWeight = Weight;
		}
	}
	return Best;
}
