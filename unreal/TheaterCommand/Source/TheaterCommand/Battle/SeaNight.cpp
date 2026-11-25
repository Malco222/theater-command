#include "Battle/SeaNight.h"

#include "Battle/SeaShow.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "TheaterCommand.h"

#include <cmath>

namespace SeaNightParts
{
	/** [P] A high-intensity carbon arc's light is a bluish white (NAVPERS 10548, chapter 15: the 24- and 36-inch
	 * searchlights' "bluish white" beam); the art director's searchlight ink on the campaign map, kept the same here so
	 * the two reads as one lamp: (200, 222, 255), its heart all but white. */
	static const FLinearColor BeamInk(200.0f / 255.0f, 222.0f / 255.0f, 1.0f), HeartInk(1.0f, 0.99f, 0.96f);
	/** [CL] A fleet star shell's lamp, the convoy's own (ConvoyLooks: a few hundred thousand candelas, reaching 3 km,
	 * white with a little yellow), so that the rules' starshells and these look like the one shell. */
	static constexpr double StarCandela = 400000.0, StarReach = 3000.0;
	static const FLinearColor StarInk(1.0f, 0.93f, 0.78f);
	/** [EST] The flares' glow and the beams' hearts and pools (M_Glow's and M_Beam's Strength): the convoy looks' own
	 * flare strength, so a star shell burning over a fleet action reads as one over a convoy. */
	static constexpr float StarGlow = 6.0f, ShaftGlow = 1.0f;
	/** The shaft: rings along its axis and points across it, with the art director's fall-off (the campaign map's beams:
	 * 0.80 of its strength at the lamp, 0.36 at the ship it is on, out to nothing past her with no hard end). */
	static constexpr int32 Rings = 12;
	static constexpr double Across[7] = { -1.9, -1.5, -0.5, 0.0, 0.5, 1.5, 1.9 };
	static constexpr float Share[7] = { 0.0f, 0.125f, 0.34f, 1.0f, 0.34f, 0.125f, 0.0f };
	static constexpr double AtLamp = 0.80, AtShip = 0.36;
	/** [EST] How far past the ship a beam's shaft is drawn (a share of the way to her), and the least a shaft is drawn
	 * across at its far end (metres), so that a beam a few kilometres off still reads at the battle camera. */
	static constexpr double PastHer = 1.25, LeastWide = 6.0;
	/** [EST] A lamp's heart and its halo (metres across), and the pool laid on the water where a beam's axis ends on the
	 * sea (a sweep, or a target lost): the beam's own width there, a little over. */
	static constexpr double Heart = 1.2, Halo = 4.0, PoolOver = 1.6;

	/** A vertex of added light: the paint's own colour, its strength in the alpha (M_Beam and M_Glow both read the
	 * emissive as colour times alpha times Strength, as the campaign map's beams do). */
	static FColor Ink(const FLinearColor& Colour, double Strength)
	{
		FLinearColor Out = Colour;
		Out.A = static_cast<float>(FMath::Clamp(Strength, 0.0, 1.0));
		return Out.ToFColor(false);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The mesh's arrays

void FSeaNight::FLaid::Reset()
{
	Points.Reset();
	Triangles.Reset();
	Normals.Reset();
	Colours.Reset();
}

int32 FSeaNight::FLaid::Add(const FVector& At, const FColor& Paint)
{
	const int32 Which = Points.Num();
	Points.Add(At);
	Normals.Add(FVector::UpVector);
	Colours.Add(Paint);
	return Which;
}

void FSeaNight::FLaid::Flat(const FVector& Mid, const FVector& AcrossBy, const FVector& AlongBy, const FColor& Paint)
{
	const int32 A = Add(Mid - AcrossBy - AlongBy, Paint), B = Add(Mid + AcrossBy - AlongBy, Paint);
	const int32 C = Add(Mid + AcrossBy + AlongBy, Paint), D = Add(Mid - AcrossBy + AlongBy, Paint);
	Triangles.Append({ A, B, C, A, C, D });
}

// ---------------------------------------------------------------------------------------------------------------------
// The battle

void FSeaNight::Begin(const FRoom& InRoom, const SeaShow::Show& Show, bool bInOn, uint64 Seed)
{
	Room = InRoom;
	bOn = bInOn;
	if (!bOn)
	{
		return; // (off: nothing made, nothing read, nothing said)
	}
	SeaNight::Light Sky;
	const SeaShow::Field& Board = Show.Board();
	Sky.bGiven = Board.bLight;
	Sky.SunUp = Board.SunUp;
	Sky.MoonUp = Board.MoonUp;
	Sky.MoonLit = Board.MoonLit;
	Sky.MoonFrom = Board.MoonFrom;
	Sky.Seeing = Board.Seeing;
	Reckoned.Begin(Seed, Sky);
	// ("-TheaterSeaNightBeam=lamp,shaft": the spot lights' share of a real searchlight's candelas, and the shafts')
	FString Asked;
	TArray<FString> Bits;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaNightBeam="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(","), false) >= 1)
	{
		LampShare = FMath::Max(0.0, FCString::Atod(*Bits[0]));
		ShaftShare = Bits.Num() > 1 ? FMath::Max(0.0, FCString::Atod(*Bits[1])) : ShaftShare;
	}
	UE_LOG(LogTheater, Log, TEXT("Sea night fighting: on (-TheaterSeaNightFight=1): %s, the night share %.2f; the lamps at %.2f of their own candelas, the shafts at %.2f."),
		Reckoned.IsOn() ? TEXT("a night") : TEXT("no night in this recording, so nothing is lit"), Reckoned.Share(), LampShare, ShaftShare);
}

void FSeaNight::SetShare(double Share)
{
	if (bOn)
	{
		Reckoned.SetShare(Share);
	}
}

void FSeaNight::Tick(const SeaShow::Show& Show, double Now, bool bSkip, const FVector& EyeAt)
{
	if (!bOn)
	{
		return;
	}
	if (bSkip)
	{
		// (a picture run skipping ahead: every beam out at once, nothing drawn along the jump)
		Reckoned.Reset();
		Dark();
		return;
	}
	SeaNight::FrameOf(Show, Read);
	Read.T = Now;
	Reckoned.Step(Read);
	if (!Reckoned.IsOn() || (Reckoned.Beams().empty() && Reckoned.Stars().empty()))
	{
		Dark();
		return;
	}
	bDark = false;
	for (FLaid& Part : Parts)
	{
		Part.Reset();
	}
	LayBeams(EyeAt);
	LightStars(Now);
	UProceduralMeshComponent* M = Mesh();
	if (!M)
	{
		return;
	}
	for (int32 I = 0; I < Sections; ++I)
	{
		const FLaid& L = Parts[I];
		if (!L.IsEmpty())
		{
			// (made afresh only when its shape changed: a beam that only swung is sent again in place, as the convoy
			// looks' flares and the scene's wrecks are)
			if ((SectionsLaid & (1 << I)) && Shapes[I].Holds(L.Points.Num(), L.Triangles.GetData(), L.Triangles.Num()))
			{
				M->UpdateMeshSection(I, L.Points, L.Normals, TArray<FVector2D>(), L.Colours, TArray<FProcMeshTangent>());
			}
			else
			{
				M->CreateMeshSection(I, L.Points, L.Triangles, L.Normals, TArray<FVector2D>(), L.Colours, TArray<FProcMeshTangent>(), false);
				Shapes[I].Keep(L.Points.Num(), L.Triangles.GetData(), L.Triangles.Num());
			}
			SectionsLaid |= 1 << I;
		}
		else if (SectionsLaid & (1 << I))
		{
			M->ClearMeshSection(I);
			Shapes[I].Forget();
			SectionsLaid &= ~(1 << I);
		}
	}
}

void FSeaNight::Dark()
{
	if (bDark)
	{
		return;
	}
	bDark = true;
	for (TWeakObjectPtr<USpotLightComponent>& Lamp : BeamLamps)
	{
		if (USpotLightComponent* L = Lamp.Get())
		{
			L->SetVisibility(false);
		}
	}
	for (TWeakObjectPtr<UPointLightComponent>& Lamp : StarLamps)
	{
		if (UPointLightComponent* L = Lamp.Get())
		{
			L->SetVisibility(false);
		}
	}
	if (UProceduralMeshComponent* M = Laid.Get())
	{
		M->ClearAllMeshSections();
	}
	for (ConvoyLook::MeshShape& Shape : Shapes)
	{
		Shape.Forget();
	}
	SectionsLaid = 0;
}

void FSeaNight::End()
{
	Dark();
	Reckoned.Reset();
	bOn = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// The beams

void FSeaNight::LayBeams(const FVector& EyeAt)
{
	using namespace SeaNightParts;
	const double U = Room.UnitsPerMetre, Night = Reckoned.Glow();
	const std::vector<SeaNight::Beam>& Beams = Reckoned.Beams();
	int32 Lit = 0;
	for (const SeaNight::Beam& B : Beams)
	{
		if (Lit >= MostBeamLamps || B.Light <= 0.0 || B.Long <= 1.0)
		{
			continue;
		}
		const FVector From = World(B.From), To = World(B.To);
		const FVector Way = (To - From).GetSafeNormal();
		if (Way.IsNearlyZero())
		{
			continue;
		}
		// The lamp: one shadowless spot light down the axis, its cone the real pencil's spread (a little wider, so the
		// water about her hull is lit too), by the real lamp's candelas at LampShare.
		if (USpotLightComponent* Lamp = BeamLamp(Lit))
		{
			const float Pencil = static_cast<float>(FMath::RadiansToDegrees(B.Spread));
			Lamp->SetWorldLocationAndRotation(From, Way.Rotation().Quaternion());
			Lamp->SetInnerConeAngle(FMath::Max(0.35f, Pencil));
			Lamp->SetOuterConeAngle(FMath::Max(0.8f, 2.2f * Pencil));
			Lamp->SetLightColor(BeamInk);
			Lamp->SetAttenuationRadius(static_cast<float>(PastHer * B.Long * U));
			// (candelas as the kit's lamps are said, at 100 units a metre)
			Lamp->SetIntensity(static_cast<float>(B.Candela * LampShare * B.Light * FMath::Square(U / 100.0)));
			Lamp->SetVisibility(true);
		}
		// The shaft: as much of it as the night's haze and smoke carry (the core's Shaft), facing the eye, narrow at the
		// lamp (its own mirror) and widening on the pencil's spread, strongest at the lamp and out to nothing past her.
		const double Shaft = B.Shaft * ShaftShare * B.Light * Night;
		if (Shaft > 0.01)
		{
			const double Full = B.Long * PastHer;
			int32 Before[7] = { 0, 0, 0, 0, 0, 0, 0 };
			for (int32 Ring = 0; Ring <= Rings; ++Ring)
			{
				const double Along = Full * Ring / Rings;
				const FVector At = From + Way * (Along * U);
				const double Wide = FMath::Max(0.5 * B.MirrorM + Along * std::tan(B.Spread), 0.5 * LeastWide * Along / FMath::Max(Full, 1.0)) * U;
				const double Fades = Along <= B.Long ? FMath::Lerp(AtLamp, AtShip, Along / FMath::Max(B.Long, 1.0))
													: FMath::Lerp(AtShip, 0.0, (Along - B.Long) / FMath::Max(Full - B.Long, 1.0));
				const FVector Side = FVector::CrossProduct(Way, At - EyeAt).GetSafeNormal();
				int32 Here[7];
				for (int32 A = 0; A < 7; ++A)
				{
					Here[A] = Parts[ShaftSection].Add(At + Side * (Across[A] * Wide), Ink(BeamInk, Shaft * Fades * Share[A]));
				}
				if (Ring > 0)
				{
					for (int32 A = 0; A + 1 < 7; ++A)
					{
						Parts[ShaftSection].Triangles.Append({ Before[A], Here[A], Here[A + 1], Before[A], Here[A + 1], Before[A + 1] });
					}
				}
				FMemory::Memcpy(Before, Here, sizeof(Here));
			}
		}
		// Its lamp as it is seen from off her beam: a white heart in a halo of its own light.
		const FVector Toward = (EyeAt - From).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(Toward, FVector::UpVector).GetSafeNormal(), Up = FVector::CrossProduct(Right, Toward).GetSafeNormal();
		const double Burns = B.Light * Night;
		Parts[GlowSection].Flat(From, Right * (Halo * U), Up * (Halo * U), Ink(BeamInk, 0.5 * Burns));
		Parts[GlowSection].Flat(From, Right * (Heart * U), Up * (Heart * U), Ink(HeartInk, Burns));
		// Where its axis ends on the sea (a sweep, or a target lost): a pool on the water, the beam's own width there.
		if (B.Target < 0 && To.Z < 2.0 * U)
		{
			const double Pool = (0.5 * B.MirrorM + B.Long * std::tan(B.Spread)) * PoolOver * U;
			Parts[GlowSection].Flat(FVector(To.X, To.Y, 0.15 * U), FVector(Pool, 0.0, 0.0), FVector(0.0, Pool, 0.0), Ink(BeamInk, 0.45 * Burns));
		}
		++Lit;
	}
	for (int32 I = Lit; I < BeamLamps.Num(); ++I)
	{
		if (USpotLightComponent* L = BeamLamps[I].Get())
		{
			L->SetVisibility(false);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The star shells

void FSeaNight::LightStars(double Now)
{
	using namespace SeaNightParts;
	const double U = Room.UnitsPerMetre, Night = Reckoned.Glow();
	struct FBurning
	{
		FVector At;
		double Light = 0.0, T0 = 0.0;
	};
	TArray<FBurning> Burning;
	for (const SeaNight::Star& S : Reckoned.Stars())
	{
		const ConvoyLook::StarOut O = ConvoyLook::StarAt(S.In, Now);
		if (!O.bBurning || O.Light <= 0.0)
		{
			continue;
		}
		Burning.Add(FBurning{ World(O.Flare), O.Light, S.In.T0 });
	}
	// The flares: each as a glow at its own light (the convoy looks' own flare), and the newest few given a lamp.
	for (const FBurning& Each : Burning)
	{
		const double Wide = 1.4 * U; // [EST] (the flare and its parachute, drawn as the convoy's own are)
		Parts[GlowSection].Flat(Each.At, FVector(Wide, 0.0, 0.0), FVector(0.0, Wide, 0.0), Ink(StarInk, Each.Light * Night));
	}
	Burning.Sort([](const FBurning& A, const FBurning& B) { return A.T0 > B.T0; });
	const int32 Lamps = FMath::Min(Burning.Num(), MostStarLamps);
	for (int32 I = 0; I < Lamps; ++I)
	{
		if (UPointLightComponent* Lamp = StarLamp(I))
		{
			Lamp->SetWorldLocation(Burning[I].At);
			Lamp->SetLightColor(StarInk);
			Lamp->SetAttenuationRadius(static_cast<float>(StarReach * U));
			Lamp->SetIntensity(static_cast<float>(Burning[I].Light * StarCandela * FMath::Square(U / 100.0)));
			Lamp->SetVisibility(true);
		}
	}
	for (int32 I = Lamps; I < StarLamps.Num(); ++I)
	{
		if (UPointLightComponent* L = StarLamps[I].Get())
		{
			L->SetVisibility(false);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// What is made, and only once there is something to draw

UProceduralMeshComponent* FSeaNight::Mesh()
{
	if (UProceduralMeshComponent* Made = Laid.Get())
	{
		return Made;
	}
	USceneComponent* Parent = Room.Root;
	AActor* Owner = Room.Owner;
	if (!Owner || !Parent || bBegun)
	{
		return nullptr;
	}
	bBegun = true;
	// One mesh in world places (it takes no turn or scale from its parent), never in shadow or collision, as the convoy
	// looks' is.
	UProceduralMeshComponent* Made = NewObject<UProceduralMeshComponent>(Owner, MakeUniqueObjectName(Owner, UProceduralMeshComponent::StaticClass(), TEXT("SeaNight")));
	Made->bUseAsyncCooking = false;
	Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Made->SetCastShadow(false);
	Made->SetupAttachment(Parent);
	Made->SetUsingAbsoluteLocation(true);
	Made->SetUsingAbsoluteRotation(true);
	Made->SetUsingAbsoluteScale(true);
	Made->RegisterComponent();
	Made->SetWorldTransform(FTransform::Identity);
	Made->SetTranslucentSortPriority(3); // (over the sea, the hulls and their smoke, as the campaign map's beams are)
	Laid = Made;
	// The shafts: the campaign map's own searchlight paint (M_Beam: added light, unlit, two-sided, drawn after the lens's
	// blur, so a beam from close by is not smeared with the ground). The hearts, pools and flares: M_Glow, in the
	// ordinary translucent pass, as the convoy's flares are.
	if (UMaterialInterface* Beam = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Beam.M_Beam")))
	{
		UMaterialInstanceDynamic* Shaft = UMaterialInstanceDynamic::Create(Beam, Made);
		Shaft->SetScalarParameterValue(TEXT("Strength"), SeaNightParts::ShaftGlow);
		Made->SetMaterial(ShaftSection, Shaft);
	}
	else
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea night fighting: M_Beam is missing (run Tools/build_content.sh): a searchlight lights its target, with no shaft shown."));
	}
	if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Glow.M_Glow")))
	{
		UMaterialInstanceDynamic* Lit = UMaterialInstanceDynamic::Create(Glow, Made);
		Lit->SetScalarParameterValue(TEXT("Strength"), SeaNightParts::StarGlow);
		Made->SetMaterial(GlowSection, Lit);
	}
	else
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea night fighting: M_Glow is missing (run Tools/build_content.sh): a star shell shows its lamp, no flare."));
	}
	return Made;
}

USpotLightComponent* FSeaNight::BeamLamp(int32 Which)
{
	if (Which < BeamLamps.Num())
	{
		return BeamLamps[Which].Get();
	}
	USceneComponent* Parent = Room.Root;
	AActor* Owner = Room.Owner;
	if (!Owner || !Parent || Which >= MostBeamLamps)
	{
		return nullptr;
	}
	// (made as the kit makes its flashes' lamps: shadowless, by candelas, falling off as the square)
	USpotLightComponent* Lamp = NewObject<USpotLightComponent>(Owner);
	Lamp->SetMobility(EComponentMobility::Movable);
	Lamp->SetupAttachment(Parent);
	Lamp->SetUsingAbsoluteLocation(true);
	Lamp->SetUsingAbsoluteRotation(true);
	Lamp->SetCastShadows(false);
	Lamp->IntensityUnits = ELightUnits::Candelas;
	Lamp->bUseInverseSquaredFalloff = true;
	Lamp->SetSpecularScale(0.5f);
	Lamp->SetIntensity(0.0f);
	Lamp->RegisterComponent();
	BeamLamps.Add(Lamp);
	return Lamp;
}

UPointLightComponent* FSeaNight::StarLamp(int32 Which)
{
	if (Which < StarLamps.Num())
	{
		return StarLamps[Which].Get();
	}
	USceneComponent* Parent = Room.Root;
	AActor* Owner = Room.Owner;
	if (!Owner || !Parent || Which >= MostStarLamps)
	{
		return nullptr;
	}
	UPointLightComponent* Lamp = NewObject<UPointLightComponent>(Owner);
	Lamp->SetMobility(EComponentMobility::Movable);
	Lamp->SetupAttachment(Parent);
	Lamp->SetUsingAbsoluteLocation(true);
	Lamp->SetCastShadows(false);
	Lamp->IntensityUnits = ELightUnits::Candelas;
	Lamp->bUseInverseSquaredFalloff = true;
	Lamp->SetSpecularScale(0.5f);
	Lamp->SetIntensity(0.0f);
	Lamp->RegisterComponent();
	StarLamps.Add(Lamp);
	return Lamp;
}
