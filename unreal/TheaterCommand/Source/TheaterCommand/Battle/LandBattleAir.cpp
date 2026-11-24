// Effects (R2): the fighters on call over the land battle, -TheaterLandAirSupport (Brian's ask through the coordinator,
// 23 November: the ground-attack fighters "fired on" from the ground, "rarely shot down", and they "crash").
//
// ALandBattle's part that draws LandAir's plan. The rules decide every strike, its run, its bombs and what it cost
// (LandFeed::AirNote); LandAir flies the planes down the run, stands the ground's guns and plans each loss's fall;
// here each frame the planes are drawn where the plan has them (one mesh a plane, the kit's model of its machine), their
// wing guns fire on the effects kit (AirGun: the flash, the smoke, the nation's tracer, carried with the plane) with
// the ground's dust kicked up where the rounds go in (Shots), the ground's guns fire up at them (AirGun, the defender's
// tracer), and the one lost is hit (AirHit), goes down (AirFall, with its fire or smoke trail) and strikes (its fuel
// going up, the earth thrown, a fire that burns on). Off, nothing of it is drawn and nothing else changes.
//
// Like a puppeteer with a script: the play is written (the rules), the moves are blocked (LandAir); here the strings
// are only pulled, frame by frame, on the script's own clock.

#include "Battle/LandBattle.h"

#include "Battle/LandAir.h"
#include "Battle/LandBattleState.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/TheaterSpeed.h"
#include "Components/DirectionalLightComponent.h"
#include "Effects/BattleEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TheaterCommand.h"

#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <string>

namespace
{
	FString AirWide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	/** The kit's name for a nation's tracer colours (the recipe's air_guns.tracers.nations), as the air battle names
	 * them; the Empire's air forces burn the British colours; empty for the default. */
	FString KitNationOf(const std::string& Nation)
	{
		std::string Up;
		for (const char Ch : Nation)
		{
			Up += static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
		}
		if (Up == "USA" || Up == "US") return TEXT("us");
		if (Up == "UK" || Up == "GBR" || Up == "GB" || Up == "ENG" || Up == "CAN" || Up == "AUS" || Up == "NZL" || Up == "SAF" || Up == "IND") return TEXT("gb");
		if (Up == "GER" || Up == "DE") return TEXT("de");
		if (Up == "ITA" || Up == "IT") return TEXT("it");
		if (Up == "JAP" || Up == "JPN" || Up == "JP") return TEXT("jp");
		if (Up == "SOV" || Up == "USSR" || Up == "SU") return TEXT("su");
		return FString();
	}

	/** A machine's shape at its true size, its middle at its own origin (models are built nose toward -X, the right
	 * hand at -Y). The kit's sculpted model of its name in the nation's paint where there is one, else the nation's
	 * plain fighter sized to its span; its far form (a few thousand points) where it has one: a strike is seen from the
	 * ground, a few hundred metres off, and a leg is up to eight of them at once. */
	struct FAirShape
	{
		TArray<FVector> Points;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FColor> Colors;
	};

	const FAirShape& AirShapeOf(const std::string& Model, const std::string& Nat, double Span, double U)
	{
		static TMap<FString, FAirShape> Shapes;
		const FString Key = FString::Printf(TEXT("%s:%s@%.1f"), *AirWide(Model), *AirWide(Nat), Span);
		if (const FAirShape* Have = Shapes.Find(Key))
		{
			return *Have;
		}
		FMiniatureMesh Near;
		FString Name = AirWide(Model) + TEXT(":") + AirWide(Nat);
		bool bStandIn = false;
		if (Model.empty() || !Near.Vehicle(Name, FVector::ZeroVector, U, 0.0, true) || Near.Vertices.Num() == 0)
		{
			Near = FMiniatureMesh();
			Name = TEXT("Fighter:") + AirWide(Nat);
			bStandIn = true;
			Near.Vehicle(Name, FVector::ZeroVector, U, 0.0, true);
		}
		FAirShape Shape;
		if (Near.Vertices.Num() > 0)
		{
			const FBox Box(Near.Vertices);
			const FVector Middle = Box.GetCenter();
			const double Fit = bStandIn ? Span * U / FMath::Max(1.0, Box.GetSize().Y) : 1.0;
			FString Kind, Whose;
			Name.Split(TEXT(":"), &Kind, &Whose);
			FMiniatureMesh Far;
			const bool bFar = Far.Sculpted(Kind, Whose, FVector::ZeroVector, U, 0.0, true) && Far.Vertices.Num() > 0 && Far.Vertices.Num() < Near.Vertices.Num();
			const FMiniatureMesh& Use = bFar ? Far : Near;
			Shape.Points.Reserve(Use.Vertices.Num());
			for (const FVector& Point : Use.Vertices)
			{
				Shape.Points.Add((Point - Middle) * Fit);
			}
			Shape.Triangles = Use.Triangles;
			Shape.Normals = Use.Normals;
			Shape.Colors = Use.Colors;
			UE_LOG(LogTheater, Log, TEXT("Land battle: the fighters on call: %s drawn as %s (%s, %d points)"), *Key, *Name, bFar ? TEXT("its far form") : TEXT("its one form"), Shape.Points.Num());
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle: the fighters on call: no model for %s, nor a plain fighter: not drawn"), *Key);
		}
		return Shapes.Add(Key, MoveTemp(Shape));
	}

	const TCHAR* FallWord(LandAir::Fall How)
	{
		return How == LandAir::Fall::Explode ? TEXT("explode") : How == LandAir::Fall::Burn ? TEXT("burn") : TEXT("spin");
	}

	/** The ground's dust kicked up where a plane's rounds go in: so many a life second for each wing (the rounds fired
	 * are the kit's AirGun's own, at its row's rate; the recipe's bullet_strike.spurt_chance of these kick a spurt). */
	constexpr double SpurtsPerSecond = 14.0;
	/** Each wing's guns laid this far to its own side of the aim (metres): converged just beyond, a pass leaves two
	 * lines of strikes a few metres apart down its lane, not one. */
	constexpr double WingApart = 1.2;

	/** A downed pilot's mesh's own number among the planes' (AirSupportMeshes). */
	int32 ChuteMeshKey(int Key) { return (1 << 20) + Key; }
	using LandAir::HangFeet;
	using LandAir::LinesLong;
	const FLinearColor ChuteSuit(0.30f, 0.26f, 0.19f), ChuteHead(0.22f, 0.15f, 0.10f), ChuteCord(0.70f, 0.68f, 0.62f);

	FVector Metres(const LandAir::V3& At, double U) { return FVector(At.X * U, At.Y * U, At.Z * U); }

	/** LandAir's silk into a mesh (units): its own colours (her off-white, its seams and hem a little darker) in the
	 * points' colours, so that the seams cost nothing a frame; each triangle wound to face the way LandAir says. */
	void SilkInto(FMiniatureMesh& M, const LandAir::SilkMesh& Silk, double U)
	{
		const int32 First = M.Vertices.Num();
		for (size_t K = 0; K < Silk.Points.size(); ++K)
		{
			const LandAir::V3& N = Silk.Facings[K];
			const FLinearColor Colour(static_cast<float>(Silk.Colours[K * 3]), static_cast<float>(Silk.Colours[K * 3 + 1]), static_cast<float>(Silk.Colours[K * 3 + 2]));
			M.Add(Metres(Silk.Points[K], U), FVector(N.X, N.Y, N.Z), Colour.ToFColor(true));
		}
		for (size_t T = 0; T + 2 < Silk.Triangles.size(); T += 3)
		{
			const LandAir::V3& Out = Silk.Outward[T / 3];
			M.Face(First + Silk.Triangles[T], First + Silk.Triangles[T + 1], First + Silk.Triangles[T + 2], FVector(Out.X, Out.Y, Out.Z));
		}
	}

	/** One mesh into another, turned and moved (its triangles keep their winding: a turn never mirrors). */
	void TurnedInto(FMiniatureMesh& Into, const FMiniatureMesh& From, const FRotator& Turn, const FVector& Shift)
	{
		const int32 First = Into.Vertices.Num();
		for (int32 K = 0; K < From.Vertices.Num(); ++K)
		{
			Into.Add(Turn.RotateVector(From.Vertices[K]) + Shift, Turn.RotateVector(From.Normals[K]), From.Colors[K]);
		}
		for (const int32 Corner : From.Triangles)
		{
			Into.Triangles.Add(First + Corner);
		}
	}

	/**
	 * A downed pilot and his lines, in his chute mesh's own space (units, x the way he drifts, z up). In the air its
	 * origin is the middle of his canopy's skirt, so that he swings under it: he hangs in his harness LinesLong under
	 * it, his feet HangFeet under that, his legs a little forward and his hands up on his risers, and a line runs down
	 * from each of his canopy's seams (Silk's) to the riser under it, four risers to his shoulders (no lines while he
	 * falls free: no silk yet).
	 */
	void HangingShape(FMiniatureMesh& M, const LandAir::SilkMesh& Silk, double U)
	{
		const double Harness = -LinesLong, Riser = -(LinesLong - 0.8), Hip = Harness - 0.3, Feet = Harness - HangFeet, Shoulder = Harness + 0.3;
		M.Limb(FVector(0.0, 0.0, Hip * U), FVector(0.0, 0.0, (Harness + 0.35) * U), 0.17 * U, 0.2 * U, ChuteSuit, 8);
		M.Limb(FVector(0.0, 0.0, (Harness + 0.38) * U), FVector(0.0, 0.0, (Harness + 0.62) * U), 0.11 * U, 0.1 * U, ChuteHead, 8);
		for (const double Side : { -1.0, 1.0 })
		{
			M.Limb(FVector(0.0, Side * 0.1 * U, Hip * U), FVector(0.18 * U, Side * 0.13 * U, Feet * U), 0.085 * U, 0.065 * U, ChuteSuit, 6);
			M.Limb(FVector(0.0, Side * 0.2 * U, Shoulder * U), FVector(0.0, Side * 0.26 * U, (Riser - 0.1) * U), 0.055 * U, 0.05 * U, ChuteSuit, 6);
		}
		if (Silk.Seams.empty())
		{
			return;
		}
		// (his four risers, from his shoulders up to where his lines gather; each line to the riser on its own quarter)
		for (const double Fore : { -1.0, 1.0 })
		{
			for (const double Side : { -1.0, 1.0 })
			{
				M.Rod(FVector(0.0, Side * 0.2 * U, Shoulder * U), FVector(Fore * 0.08 * U, Side * 0.24 * U, Riser * U), 0.02 * U, ChuteCord, 3);
			}
		}
		for (const int Seam : Silk.Seams)
		{
			const FVector End = Metres(Silk.Points[static_cast<size_t>(Seam)], U);
			const FVector Top(End.X >= 0.0 ? 0.08 * U : -0.08 * U, End.Y >= 0.0 ? 0.24 * U : -0.24 * U, Riser * U);
			M.Rod(Top, End, 0.012 * U, ChuteCord, 3);
		}
	}

	/**
	 * Down, in his chute mesh's own space (units, its origin his feet as he landed, x the way his canopy falls, z up):
	 * while his chute's mesh still draws him (bMan), he goes over from standing on his feet (Fallen 0) to lying flat
	 * (1) the way he faced as he landed (Facing, degrees from x), pitched LandAir::OverPitch about his feet and moved by
	 * LandAir::OverShift, so that flat his hips lie where the crowd's man's will: go_prone's end offset (EndX forward,
	 * EndY to his right) forward of his feet, as a rider is posed. The crowd's man was put 0.7 m on from where this one
	 * had lain, and jumped there in plain view under the canopy (a review of 24 November). And his lines, from his
	 * harness to the half of his canopy's skirt toward him (LandAir::LineBack) as it falls in; once the crowd's man has
	 * him, the lines only, to where his harness lies.
	 */
	void DownShape(FMiniatureMesh& M, const LandAir::SilkMesh& Silk, bool bMan, double Fallen, double Facing, double EndX, double EndY, double U)
	{
		const double F = FMath::Clamp(Fallen, 0.0, 1.0);
		const FRotator Over(-LandAir::OverPitch * F, Facing, 0.0);
		const LandAir::V3 By = LandAir::OverShift(F, EndX, EndY);
		const FVector Shift = FRotator(0.0, Facing, 0.0).RotateVector(FVector(By.X * U, By.Y * U, 0.0)) + FVector(0.0, 0.0, By.Z * U);
		// (his harness, where his lines meet him: at his chest, 1 m up him; where it lies once the crowd's man has him)
		const FVector Harness = Over.RotateVector(FVector(0.0, 0.0, 1.0 * U)) + Shift;
		if (bMan)
		{
			// (standing as he lands: his feet at the origin, his hips OverHip up, his hands down by his harness)
			const double Hip = LandAir::OverHip;
			FMiniatureMesh Him;
			Him.Limb(FVector(0.0, 0.0, Hip * U), FVector(0.0, 0.0, 1.45 * U), 0.17 * U, 0.2 * U, ChuteSuit, 8);
			Him.Limb(FVector(0.0, 0.0, 1.48 * U), FVector(0.0, 0.0, 1.72 * U), 0.11 * U, 0.1 * U, ChuteHead, 8);
			for (const double Side : { -1.0, 1.0 })
			{
				Him.Limb(FVector(0.0, Side * 0.1 * U, Hip * U), FVector(0.05 * U, Side * 0.12 * U, 0.0), 0.085 * U, 0.065 * U, ChuteSuit, 6);
				Him.Limb(FVector(0.0, Side * 0.2 * U, 1.4 * U), FVector(0.12 * U, Side * 0.18 * U, 0.85 * U), 0.055 * U, 0.05 * U, ChuteSuit, 6);
			}
			TurnedInto(M, Him, Over, Shift);
		}
		// (the same lines all the while it falls in, so that his part of the mesh keeps its make and is moved in place)
		for (size_t K = 0; K < Silk.Seams.size(); ++K)
		{
			if (LandAir::LineBack(K, Silk.Seams.size()))
			{
				M.Rod(Harness, Metres(Silk.Points[static_cast<size_t>(Silk.Seams[K])], U), 0.012 * U, ChuteCord, 3);
			}
		}
	}

	/** A chute mesh's makings, kept from frame to frame so that their storage is used again (the game thread's alone;
	 * a review of 24 November: each build had made new storage for some 1,500 points). */
	struct FChuteScratch
	{
		LandAir::SilkMesh Silk;
		FMiniatureMesh Built, Canopy;
	};
	FChuteScratch& ChuteScratch()
	{
		static FChuteScratch Scratch;
		return Scratch;
	}
	void Emptied(FMiniatureMesh& M)
	{
		M.Vertices.Reset();
		M.Triangles.Reset();
		M.Normals.Reset();
		M.Colors.Reset();
	}

	/** One section of a chute's mesh: moved in place (its points, facings and colours; no new proxy) when bInPlace and
	 * it is made as it was (as many points and triangles), else made again; cleared when there is nothing of it. True
	 * when it was made again or cleared (either makes the mesh's proxy again). */
	bool PutSection(UProceduralMeshComponent* Mesh, int32 Section, const FMiniatureMesh& M, bool bInPlace)
	{
		const FProcMeshSection* Had = Section < Mesh->GetNumSections() ? Mesh->GetProcMeshSection(Section) : nullptr;
		const int32 HadPoints = Had ? Had->ProcVertexBuffer.Num() : 0, HadCorners = Had ? Had->ProcIndexBuffer.Num() : 0;
		if (M.Triangles.Num() == 0)
		{
			if (HadCorners == 0)
			{
				return false;
			}
			Mesh->ClearMeshSection(Section);
			return true;
		}
		if (bInPlace && HadPoints == M.Vertices.Num() && HadCorners == M.Triangles.Num())
		{
			Mesh->UpdateMeshSection(Section, M.Vertices, M.Normals, TArray<FVector2D>(), M.Colors, TArray<FProcMeshTangent>());
			return false;
		}
		Mesh->CreateMeshSection(Section, M.Vertices, M.Triangles, M.Normals, TArray<FVector2D>(), M.Colors, TArray<FProcMeshTangent>(), false);
		return true;
	}
}

void ALandBattle::FlyOver(const FVector& EyeWorld)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Land_FlyOver);
	THEATER_SPEED_SCOPE(LandFlyOver);
	FLandBattleState& S = *State;
	FLandBattleState::FAirSupport& A = S.AirSupport;
	if (!A.bAsked)
	{
		A.bAsked = true;
		A.bOn = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandAirSupport"));
		UE_LOG(LogTheater, Log, TEXT("Land battle: the fighters on call %s"),
			A.bOn ? TEXT("flown over the field, fired on, the few lost going down (-TheaterLandAirSupport, release 2)") : TEXT("not drawn, as before"));
		// (their downed pilots under their parachutes, where the rules say they got out: release 2, off until the art
		// director and a 45 fps reading pass; Brian, 23 November 20:09Z)
		A.bChutes = A.bOn && FParse::Param(FCommandLine::Get(), TEXT("TheaterLandChutes"));
		UE_LOG(LogTheater, Log, TEXT("Land battle: downed pilots %s"), A.bChutes ? TEXT("come down under their parachutes (-TheaterLandChutes, release 2)") : TEXT("not drawn"));
	}
	if (!A.bOn || !S.Field)
	{
		return;
	}
	const LandField::Ground* FieldPtr = S.Field.get();
	const LandAir::GroundFn Ground = [FieldPtr](double X, double Y) { return FieldPtr->HeightAt(X, Y); };
	// (planned again whenever the recording has more air notes: a live battle's strikes come as it is fought. The legs
	// already flown are planned the same; a loss whose note came after its plane was hit begins late, where it is)
	bool bLate = false;
	if (!A.bMade || S.Rec.Air.size() != A.Built)
	{
		A.Sky.Build(S.Rec, Ground, S.Life);
		bLate = A.bMade;
		A.bMade = true;
		A.Built = S.Rec.Air.size();
		UE_LOG(LogTheater, Log, TEXT("Land battle: the fighters on call: %d legs planned from %d air notes"), static_cast<int32>(A.Sky.Legs()), static_cast<int32>(A.Built));
	}
	const double T = S.Battle;
	const double Kit = S.Lived; // (the effects kit's clock, as TickBegin is given it)
	const double U = UnitsPerMetre;
	if (A.Told >= 0.0 && T < A.Told - 1e-6)
	{
		// (the battle wound back, a replay begun again: what was going down is let go, and the rest told again from here)
		if (Effects)
		{
			for (const int Down : A.Falling)
			{
				Effects->TrailStop(LandAir::FireIdOf(Down));
				Effects->AirFallStop(LandAir::FireIdOf(Down));
			}
			for (const int Id : A.Firing)
			{
				Effects->AirGunStop(Id);
			}
			for (const int Id : A.Gunning)
			{
				Effects->AirGunStop(Id);
			}
		}
		A.Falling.clear();
		A.Hit.clear();
		A.Struck.clear();
		A.Firing.clear();
		A.Gunning.clear();
		bLate = true;
	}
	A.Sky.At(T, A.Planes, A.Guns);

	// Each plane where the plan has it: its nose along its path, banked into its turn, a wreck nose-in where it struck.
	const FQuat HalfRound = FRotator(0.0, 180.0, 0.0).Quaternion();
	UMaterialInterface* Ink = SteelInk ? SteelInk.Get() : ThingInk.Get();
	TSet<int32> Seen;
	for (const LandAir::Plane& P : A.Planes)
	{
		TObjectPtr<UProceduralMeshComponent>* Have = AirSupportMeshes.Find(P.Key);
		UProceduralMeshComponent* Mesh = Have ? Have->Get() : nullptr;
		if (!Mesh)
		{
			const FAirShape& Shape = AirShapeOf(P.Model, P.Nat, P.Span, U);
			if (Shape.Points.Num() == 0)
			{
				continue;
			}
			Mesh = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("LandAirPlane_%d"), P.Key)));
			Mesh->SetupAttachment(Root);
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(true);
			Mesh->bUseAsyncCooking = false;
			Mesh->RegisterComponent();
			Mesh->CreateMeshSection(0, Shape.Points, Shape.Triangles, Shape.Normals, TArray<FVector2D>(), Shape.Colors, TArray<FProcMeshTangent>(), false);
			if (Ink)
			{
				Mesh->SetMaterial(0, Ink);
			}
			AirSupportMeshes.Add(P.Key, Mesh);
		}
		Seen.Add(P.Key);
		Mesh->SetVisibility(true);
		Mesh->SetWorldLocationAndRotation(World(P.At.X, P.At.Y, P.At.Z), FRotator(P.Pitch, P.Heading, P.Bank).Quaternion() * HalfRound);
	}
	// Effects (R2): the downed pilots (-TheaterLandChutes). Each is one small mesh: he and his lines on the things' ink
	// (its first section), his canopy's silk on its own copy of it (its second), so that its glow against the sun is
	// one number a frame and its seams are in its points' colours (the art director's sixth note, 24 November 02:26Z:
	// cheaply, no mesh made again a frame and no new material). In the air its pivot is his canopy, so that he swings
	// under it; it is moved in place only as his canopy fills (its open share by 0.02). Down, his canopy tips over and
	// falls in downwind, lying in long loose folds laid on the ground under it (made again by 0.05 of its slump:
	// LandAir::SilkMoved says why; the art director's verdict of 24 November 14:34Z), and his chute's mesh draws him
	// going over flat under it (moved in place); there he is swapped for a man of the crowd (her third note): in his
	// army's tank crews' file where it has one, with its own head, else its infantry's, bareheaded; unarmed, in the
	// clips LandAir says, at the rates and blends it says.
	if (A.bChutes)
	{
		A.Sky.Chutes(T, A.ChutesNow);
		std::set<int> Down;
		const LandAir::V3 EyeAt{ EyeWorld.X, EyeWorld.Y, EyeWorld.Z }, ToSun{ TowardSun.X, TowardSun.Y, TowardSun.Z };
		// (the key light's strength against a clear day's, 4: an overcast day's silk glows less, a night's not at all)
		const double KeyShare = BattleKey.IsValid() ? FMath::Clamp(static_cast<double>(BattleKey->Intensity) / 4.0, 0.0, 1.25) : 1.0;
		for (const LandAir::Chute& C : A.ChutesNow)
		{
			const int32 MeshKey = ChuteMeshKey(C.Key);
			TObjectPtr<UProceduralMeshComponent>* Have = AirSupportMeshes.Find(MeshKey);
			UProceduralMeshComponent* Mesh = Have ? Have->Get() : nullptr;
			if (!Mesh)
			{
				Mesh = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("LandAirChute_%d"), C.Key)));
				Mesh->SetupAttachment(Root);
				Mesh->SetMobility(EComponentMobility::Movable);
				Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Mesh->SetCastShadow(true);
				Mesh->bUseAsyncCooking = false;
				Mesh->RegisterComponent();
				if (ThingInk)
				{
					Mesh->SetMaterial(0, ThingInk.Get());
					Mesh->SetMaterial(1, ThingInk.Get());
				}
				if (ThingInk && ThingInk->Parent)
				{
					// (his silk's own ink: the things' ink copied, as a tree's leaves' is, its fill his canopy's glow)
					UMaterialInstanceDynamic* SilkInk = UMaterialInstanceDynamic::Create(ThingInk->Parent.Get(), this);
					SilkInk->CopyParameterOverrides(ThingInk.Get());
					Mesh->SetMaterial(1, SilkInk);
				}
				AirSupportMeshes.Add(MeshKey, Mesh);
				UE_LOG(LogTheater, Log, TEXT("Land battle: the pilot of plane %d is out under his parachute, %.0f m up (%d gores, %.1f m across)"), C.Key,
					C.At.Z - FieldPtr->HeightAt(C.At.X, C.At.Y), C.Gores, C.Across);
			}
			FLandBattleState::FAirSupport::FChuteMan& Him = A.ChuteMen[C.Key];
			const double Shape = C.bDown ? 2.0 + C.Slump : C.Open;
			// (his chute's mesh lets him go only once the crowd's man has been given his ride and posed with it: the
			// crowd is posed before FlyOver, so a ride set here shows the frame after; until then, and for good where no
			// man of the crowd could be made, he lies here flat. A review of 24 November: he had gone for a frame at the
			// swap)
			const double Lying = C.bDown && (C.bInMesh || Him.Clip.empty()) ? (C.bInMesh ? C.Fallen : 1.0) : -1.0;
			const bool bSilk = LandAir::SilkMoved(Shape, Him.Shape), bMan = LandAir::ManMoved(Lying, Him.Lying);
			if (bSilk || bMan)
			{
				// (his part built against the silk as it is drawn: as it is now if that is built again, else as last built)
				const double SilkAt = bSilk ? Shape : Him.Shape;
				FChuteScratch& Make = ChuteScratch();
				Make.Silk.Clear();
				if (C.bDown)
				{
					LandAir::HeapShape(C, SilkAt - 2.0, std::hypot(C.Heap.X - C.Landed.X, C.Heap.Y - C.Landed.Y), Make.Silk, Ground);
				}
				else if (SilkAt > 0.0)
				{
					LandAir::CanopyShape(C, SilkAt, Make.Silk);
				}
				Emptied(Make.Built);
				if (C.bDown)
				{
					// (lying where a rider in go_prone's end lies: by his own file's end offset once it is known)
					double EndX = LandAir::ProneEndX, EndY = LandAir::ProneEndY;
					if (const LandMen::Soldier* Own = (S.Men && Him.Man >= 0) ? S.Men->SoldierFile(Him.File) : nullptr)
					{
						const int Prone = Own->ClipIndex("go_prone");
						if (Prone >= 0 && Own->Clips[static_cast<size_t>(Prone)].bEndOffset)
						{
							EndX = Own->Clips[static_cast<size_t>(Prone)].EndOffset.X;
							EndY = Own->Clips[static_cast<size_t>(Prone)].EndOffset.Y;
						}
					}
					DownShape(Make.Built, Make.Silk, Lying >= 0.0, Lying >= 0.0 ? Lying : 1.0, C.Face - C.HeapHeading, EndX, EndY, U);
				}
				else
				{
					HangingShape(Make.Built, Make.Silk, U);
				}
				// (his part moved in place while its make is the same; his silk in the air too, but down made again, its
				// folds turning some of its triangles over: LandAir::SilkMoved)
				bool bMade = PutSection(Mesh, 0, Make.Built, true);
				if (bSilk)
				{
					Emptied(Make.Canopy);
					SilkInto(Make.Canopy, Make.Silk, U);
					bMade = PutSection(Mesh, 1, Make.Canopy, !C.bDown) || bMade;
					Him.Shape = Shape;
				}
				if (bMan)
				{
					Him.Lying = Lying;
				}
				// (counted for the 45 fps reading: told once his canopy lies flat)
				Him.Made += bMade ? 1 : 0;
				Him.Moved += bMade ? 0 : 1;
				if (Shape >= 3.0 && !Him.bCounted)
				{
					Him.bCounted = true;
					UE_LOG(LogTheater, Log, TEXT("Land battle: the pilot of plane %d's chute: its mesh made %d times, moved in place %d"), C.Key, Him.Made, Him.Moved);
				}
			}
			Seen.Add(MeshKey);
			Mesh->SetVisibility(true);
			FVector Middle;
			if (C.bDown)
			{
				Mesh->SetWorldLocationAndRotation(World(C.Landed.X, C.Landed.Y, C.Landed.Z), FRotator(0.0, C.HeapHeading, 0.0).Quaternion());
				Middle = World(C.Heap.X, C.Heap.Y, C.Heap.Z + 0.3);
			}
			else
			{
				// (his feet at At: the mesh hangs from his canopy's skirt, LinesLong and HangFeet over them)
				Mesh->SetWorldLocationAndRotation(World(C.At.X, C.At.Y, C.At.Z + LinesLong + HangFeet), FRotator(C.Swing, C.Heading, 0.0).Quaternion());
				Middle = Mesh->GetComponentTransform().TransformPosition(FVector(0.0, 0.0, 0.5 * C.Deep * U));
			}
			// His canopy's glow (her sixth note): the light through its silk seen against the sun, a little from under it,
			// as the pieces' fill gives back a figure's own colour on its side from the sun; its rim the pieces' own.
			if (UMaterialInstanceDynamic* SilkInk = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(1)))
			{
				const double Glow = LandAir::Glow(EyeAt, LandAir::V3{ Middle.X, Middle.Y, Middle.Z }, ToSun) * KeyShare;
				SilkInk->SetScalarParameterValue(TEXT("FigureFill"), static_cast<float>(Glow));
				SilkInk->SetVectorParameterValue(TEXT("FigureSun"), FLinearColor(static_cast<float>(TowardSun.X), static_cast<float>(TowardSun.Y), static_cast<float>(TowardSun.Z),
					static_cast<float>(0.6 * KeyShare)));
			}
			if (!C.bDown || !S.Men)
			{
				continue;
			}
			// Down: made a man of the crowd as he lands, shown once his chute's mesh lets him go.
			if (Him.Man < 0 && Him.File.empty())
			{
				std::string Row;
				bool bBare = false;
				const std::string Crew = C.Nat == "USA" ? "USA_tanker" : C.Nat == "GER" ? "GER_panzer" : std::string();
				if (!Crew.empty() && S.Men->SoldierFile(Crew))
				{
					Him.File = Crew;
				}
				else
				{
					Him.File = S.Men->RiderFileOf(C.Nat, Row);
					bBare = true;
				}
				Him.Man = Him.File.empty() ? -1 : S.Men->AddRider(Him.File, C.Nat, 0xC47E2ull ^ static_cast<uint64_t>(C.Key) * 0x9E3779B97F4A7C15ull, Row, bBare);
				if (Him.File.empty())
				{
					Him.File = "-";
				}
				MenDrawn.SetNum(S.Men->Count());
				UE_LOG(LogTheater, Log, TEXT("Land battle: the pilot of plane %d is down%s, a man of the crowd (%s%s)"), C.Key, C.bHard ? TEXT(" hard") : TEXT(""), *AirWide(Him.File),
					bBare ? TEXT(", bareheaded") : TEXT(""));
			}
			const LandMen::Soldier* Sol = Him.Man >= 0 ? S.Men->SoldierFile(Him.File) : nullptr;
			const int Clip = Sol && !C.bInMesh ? Sol->ClipIndex(C.Clip) : -1;
			if (Clip < 0)
			{
				continue;
			}
			Down.insert(C.Key);
			if (C.Clip != Him.Clip)
			{
				Him.WasClip = Him.Clip;
				Him.WasTime = Him.NowTime;
				Him.WasRate = Him.NowRate;
				Him.Switched = T;
				Him.Clip = C.Clip;
			}
			Him.NowTime = C.Time;
			Him.NowRate = C.Rate;
			LandMen::Crowd::Ride Ride;
			// (over LandAir's time for the clip he goes into: a quarter of a second, half a second into his harness's
			// kneel)
			const double Blended = Him.Switched >= 0.0 ? (T - Him.Switched) * S.Life / FMath::Max(0.05, C.Blend) : 1.0;
			const int WasClip = Him.WasClip.empty() ? -1 : Sol->ClipIndex(Him.WasClip);
			if (WasClip >= 0 && Blended < 1.0)
			{
				// (the clip he leaves goes on as it was going, but one played back, getting up onto his knees, is held where
				// it stopped: he kneels)
				Ride.FromClip = WasClip;
				Ride.FromTime = static_cast<float>(Him.WasTime + (Him.WasRate > 0.0 ? (T - Him.Switched) * S.Life * Him.WasRate : 0.0));
				Ride.W = static_cast<float>(FMath::Clamp(Blended, 0.0, 1.0));
			}
			const double Fx = std::sin(FMath::DegreesToRadians(C.Heading)), Fy = std::cos(FMath::DegreesToRadians(C.Heading));
			float* Rm = Ride.Seat.R;
			Rm[0] = static_cast<float>(Fx); Rm[1] = static_cast<float>(Fy); Rm[2] = 0.f;
			Rm[3] = static_cast<float>(Fy); Rm[4] = static_cast<float>(-Fx); Rm[5] = 0.f;
			Rm[6] = 0.f; Rm[7] = 0.f; Rm[8] = 1.f;
			Ride.Seat.T[0] = static_cast<float>(C.At.X);
			Ride.Seat.T[1] = static_cast<float>(C.At.Y);
			Ride.Seat.T[2] = static_cast<float>(C.At.Z);
			Ride.bShown = true;
			Ride.Clip = Clip;
			Ride.Time = static_cast<float>(C.Time);
			Ride.bUnarmed = true;
			S.Men->SetRide(Him.Man, Ride);
		}
		// (a battle wound back to before a pilot came down, or to while his chute's mesh still draws him: his man is put
		// away again)
		for (std::pair<const int, FLandBattleState::FAirSupport::FChuteMan>& Each : A.ChuteMen)
		{
			if (Each.second.Man >= 0 && !Down.count(Each.first) && !Each.second.Clip.empty())
			{
				S.Men->SetRide(Each.second.Man, LandMen::Crowd::Ride());
				Each.second.Clip.clear();
			}
		}
	}
	for (TPair<int32, TObjectPtr<UProceduralMeshComponent>>& Each : AirSupportMeshes)
	{
		if (Each.Value && !Seen.Contains(Each.Key))
		{
			Each.Value->SetVisibility(false);
		}
	}
	if (!Effects)
	{
		A.Told = T;
		return;
	}

	// The planes' wing guns, while each is in its pass: the kit's aircraft guns, from where each muzzle is, at the
	// ground the pass rakes; and the dust kicked up there.
	const double From = A.Told >= 0.0 && !bLate ? A.Told : T;
	std::set<int> FiringNow;
	std::map<int, int> SideOfLeg;
	for (const LandAir::Plane& P : A.Planes)
	{
		SideOfLeg[P.Leg] = P.Side;
		if (!P.bFiring || P.How != LandAir::Fall::None || P.bWreck)
		{
			continue;
		}
		const FVector Carry = WorldWay(P.Vel.X, P.Vel.Y, P.Vel.Z) * U;
		const FString Kind = AirWide(P.Gun);
		const FString Whose = KitNationOf(P.Nat);
		const double H = FMath::DegreesToRadians(P.Heading);
		const double Rx = std::cos(H), Ry = -std::sin(H); // (the right hand of its heading, on the ground)
		const int Spurts = FMath::Min(8, A.Sky.Rounds(From, T, SpurtsPerSecond));
		for (int W = 0; W < 2; ++W)
		{
			const int Id = LandAir::GunIdOf(P.Key, W);
			FiringNow.insert(Id);
			const double Side = W ? WingApart : -WingApart;
			const double Sx = P.Aim.X + Rx * Side, Sy = P.Aim.Y + Ry * Side;
			const FVector Muzzle = World(P.Muzzle[W].X, P.Muzzle[W].Y, P.Muzzle[W].Z);
			const FVector Strike = World(Sx, Sy, FieldPtr->HeightAt(Sx, Sy));
			Effects->AirGun(Kit, Id, Muzzle, Strike, Carry, Kind, 0.0f, 0.0f, Whose);
			if (Spurts > 0)
			{
				// (the last few metres of the rounds' way in, so that only their strikes are seen: the stream itself is
				// AirGun's)
				const FVector Back = (Muzzle - Strike).GetSafeNormal() * (6.0 * U);
				Effects->Shots(Id, Strike + Back, Strike, TEXT("hmg"), Spurts, static_cast<float>(SpurtsPerSecond), AirWide(FieldPtr->At(Sx, Sy).Fx));
			}
		}
	}
	for (const int Id : A.Firing)
	{
		if (!FiringNow.count(Id))
		{
			Effects->AirGunStop(Id);
		}
	}
	A.Firing.swap(FiringNow);

	// The ground's guns firing up at them: the defender's machine guns, in his own tracer.
	std::set<int> GunningNow;
	for (const LandAir::Gunner& G : A.Guns)
	{
		const std::map<int, int>::const_iterator Leg = SideOfLeg.find(G.Key / 8);
		const int Theirs = Leg != SideOfLeg.end() && (Leg->second == 0 || Leg->second == 1) ? 1 - Leg->second : -1;
		const FString Whose = Theirs >= 0 ? KitNationOf(S.Rec.Sides[Theirs].Nat) : FString();
		const int Id = LandAir::GunnerIdOf(G.Key);
		GunningNow.insert(Id);
		Effects->AirGun(Kit, Id, World(G.From.X, G.From.Y, G.From.Z), World(G.To.X, G.To.Y, G.To.Z), FVector::ZeroVector, Whose == TEXT("us") ? TEXT("hmg") : TEXT("rifle"), 0.0f, 0.0f, Whose);
	}
	for (const int Id : A.Gunning)
	{
		if (!GunningNow.count(Id))
		{
			Effects->AirGunStop(Id);
		}
	}
	A.Gunning.swap(GunningNow);

	// What happened since the last frame: a plane hit (its fall begins, with its trail), a plane into the ground (its
	// fuel going up, the earth thrown, and a fire that burns on where it lies).
	A.Happens.clear();
	A.Sky.Between(bLate || A.Told < 0.0 ? -1.0e9 : A.Told, T, A.Happens);
	for (const LandAir::Happen& E : A.Happens)
	{
		const int Id = LandAir::FireIdOf(E.Key);
		const FVector At = World(E.At.X, E.At.Y, E.At.Z);
		const FVector Vel = WorldWay(E.Vel.X, E.Vel.Y, E.Vel.Z) * U;
		if (E.Kind == LandAir::Happen::What::Hit)
		{
			if (!A.Hit.insert(E.Key).second)
			{
				continue;
			}
			Effects->AirHit(Kit, At, FVector::UpVector, Vel, TEXT("hmg"));
			Effects->AirFall(Kit, Id, At, Vel, FallWord(E.How), static_cast<float>(E.Span), FieldPtr->HeightAt(E.At.X, E.At.Y) * U);
			if (E.How != LandAir::Fall::Explode)
			{
				Effects->TrailStart(Kit, Id, At, Vel, E.How == LandAir::Fall::Burn ? TEXT("fire") : TEXT("smoke"), static_cast<float>(E.Span));
				A.Falling.insert(E.Key);
			}
			UE_LOG(LogTheater, Log, TEXT("Land battle: the fighters on call: plane %d hit at %.0f m (%s)"), E.Key, E.At.Z - FieldPtr->HeightAt(E.At.X, E.At.Y), FallWord(E.How));
			continue;
		}
		if (!A.Struck.insert(E.Key).second)
		{
			continue;
		}
		if (A.Falling.erase(E.Key))
		{
			Effects->TrailStop(Id);
			Effects->AirFallStop(Id);
		}
		const bool bWhole = E.How != LandAir::Fall::Explode;
		const FString Under = AirWide(FieldPtr->At(E.At.X, E.At.Y).Fx);
		if (bWhole)
		{
			Effects->FuelBall(At, 18.0f, Vel);
		}
		Effects->ShellOfSize(At, bWhole ? TEXT("gun75") : TEXT("mortar"), Under);
		// (burning on where it lies: the whole of a fighter for the rest of the battle, at most four minutes; an
		// exploded one's pieces for a minute and a half)
		const double Left = FMath::Max(20.0, (S.Rec.End + 8.0 - E.T) * S.Life);
		Effects->Burn(At, bWhole ? 5.0f : 2.5f, static_cast<float>(FMath::Min(Left, bWhole ? 240.0 : 90.0)), Id);
	}
	// Each one going down: its trail and its shedding where it is now.
	for (const LandAir::Plane& P : A.Planes)
	{
		if (P.How == LandAir::Fall::None || P.bWreck || !A.Falling.count(P.Key))
		{
			continue;
		}
		const int Id = LandAir::FireIdOf(P.Key);
		const FVector At = World(P.At.X, P.At.Y, P.At.Z);
		const FVector Vel = WorldWay(P.Vel.X, P.Vel.Y, P.Vel.Z) * U;
		Effects->TrailMove(Kit, Id, At, Vel);
		Effects->AirFallMove(Kit, Id, At, Vel);
	}
	A.Told = T;
}
