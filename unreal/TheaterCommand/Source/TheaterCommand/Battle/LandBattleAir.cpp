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
#include "Effects/BattleEffects.h"
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
}

void ALandBattle::FlyOver()
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
