#include "Battle/ConvoyLooks.h"

#include "Battle/AirFeed.h"
#include "Battle/AirStage.h"
#include "Battle/SeaShow.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Effects/BattleEffects.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "TheaterCommand.h"

#include <cmath>

namespace ConvoyLooksParts
{
	using ConvoyLook::P3;

	/** The kit's trail ids for the starshells' smoke: apart from the scene's own (fires 810000 and up, funnels 1000000 and
	 * up, a ship's key times four) and the stage's (238000 and up). */
	static constexpr int32 StarTrailIds = 3600000;
	/** [EST] A starshell's candle: a few hundred thousand candelas (the period's illuminating shells), its light reaching
	 * 3 km; white with a little yellow. "-TheaterStarCandela=" tries others. */
	static constexpr double StarCandela = 400000.0, StarReach = 3000.0;
	static const FLinearColor StarColour(1.0f, 0.93f, 0.78f);
	/** At most this many lamps for starshells at once (the newest burning). */
	static constexpr int32 MostStarLamps = 3;
	/** Small things drawn about 1.3 times real, as the effects kit's are, so that they read at the battle camera. */
	static constexpr double LookScale = 1.3;
	/** [EST] The flares' glow (M_Glow's Strength: "-TheaterStarGlow=") and the deep swirls' ("-TheaterSwirlGlow="): a
	 * swirl's alpha is at most SwirlAlpha of the light falling on it. */
	static constexpr float StarGlow = 6.0f, SwirlGlow = 1.0f;
	static constexpr double SwirlAlpha = 0.35, SwirlBand = 1.4;
	/** A boat not in the show's ships for this long is forgotten (gone down, or out of the picture). */
	static constexpr double ForgetAfter = 5.0;
	/** The mesh's sections: solid things, the flares' glow, the swirls. */
	static constexpr int32 SolidSection = 0, GlowSection = 1, SwirlSection = 2, Sections = 3;

	/** The scene's bOnWater held true while the kit is called, so its water goes on the sea and never on a ship's deck
	 * (the stage's DeckUnder puts a point over a dived boat on her deck, metres under the sea). */
	struct FHoldOnWater
	{
		bool* Flag = nullptr;
		bool bWas = false;
		explicit FHoldOnWater(bool* InFlag) : Flag(InFlag), bWas(InFlag ? *InFlag : false)
		{
			if (Flag)
			{
				*Flag = true;
			}
		}
		~FHoldOnWater()
		{
			if (Flag)
			{
				*Flag = bWas;
			}
		}
		FHoldOnWater(const FHoldOnWater&) = delete;
		FHoldOnWater& operator=(const FHoldOnWater&) = delete;
	};

	/** One section of the mesh, laid afresh each step. */
	struct FLaid
	{
		TArray<FVector> Points;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FColor> Colours;

		/** A triangle whose drawn side faces Outward (the miniature kit's own rule, FMiniatureMesh::Face). */
		void Tri(const FVector& A, const FVector& B, const FVector& C, const FVector& Outward, const FColor& Colour)
		{
			const FVector Normal = Outward.GetSafeNormal();
			const int32 First = Points.Num();
			Points.Append({ A, B, C });
			Normals.Append({ Normal, Normal, Normal });
			Colours.Append({ Colour, Colour, Colour });
			const FVector Facing = (B - C) ^ (A - C);
			if ((Facing | Outward) >= 0.0)
			{
				Triangles.Append({ First, First + 1, First + 2 });
			}
			else
			{
				Triangles.Append({ First, First + 2, First + 1 });
			}
		}

		void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Outward, const FColor& Colour)
		{
			Tri(A, B, C, Outward, Colour);
			Tri(A, C, D, Outward, Colour);
		}

		/** A rod from From to To, Sides-sided, with its ends closed. */
		void Rod(const FVector& From, const FVector& To, double Radius, int32 Sides, const FColor& Colour)
		{
			const FVector Along = To - From;
			if (Along.IsNearlyZero() || Radius <= 0.0)
			{
				return;
			}
			const FVector Axis = Along.GetSafeNormal();
			const FVector U = (FMath::Abs(Axis.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).Cross(Axis).GetSafeNormal() * Radius;
			const FVector V = Axis.Cross(U).GetSafeNormal() * Radius;
			for (int32 I = 0; I < Sides; ++I)
			{
				const double A0 = 2.0 * UE_DOUBLE_PI * I / Sides, A1 = 2.0 * UE_DOUBLE_PI * (I + 1) / Sides;
				const FVector R0 = U * FMath::Cos(A0) + V * FMath::Sin(A0), R1 = U * FMath::Cos(A1) + V * FMath::Sin(A1);
				Quad(From + R0, From + R1, To + R1, To + R0, R0 + R1, Colour);
				Tri(From, From + R0, From + R1, -Axis, Colour);
				Tri(To, To + R0, To + R1, Axis, Colour);
			}
		}

		/** A shallow dome (a parachute's canopy): its rim's middle, radius and height. */
		void Dome(const FVector& Rim, double Radius, double High, int32 Sides, const FColor& Colour)
		{
			const int32 Rings = 3;
			for (int32 R = 0; R < Rings; ++R)
			{
				const double S0 = static_cast<double>(R) / Rings, S1 = static_cast<double>(R + 1) / Rings;
				for (int32 I = 0; I < Sides; ++I)
				{
					const double A0 = 2.0 * UE_DOUBLE_PI * I / Sides, A1 = 2.0 * UE_DOUBLE_PI * (I + 1) / Sides;
					auto On = [&Rim, Radius, High](double S, double A)
					{
						const double Out = Radius * FMath::Cos(S * UE_DOUBLE_HALF_PI);
						return Rim + FVector(FMath::Cos(A) * Out, FMath::Sin(A) * Out, High * FMath::Sin(S * UE_DOUBLE_HALF_PI));
					};
					const FVector Outward = FVector(FMath::Cos(0.5 * (A0 + A1)), FMath::Sin(0.5 * (A0 + A1)), 0.6 + S0);
					Quad(On(S0, A0), On(S0, A1), On(S1, A1), On(S1, A0), Outward, Colour);
				}
			}
		}

		/** A flat disc of light, bright in the middle and gone at its rim (for M_Glow: its alpha is how much). */
		void Fan(const FVector& Mid, const FVector& Normal, double Radius, int32 Sides, const FColor& Inner, const FColor& Rim)
		{
			const FVector Axis = Normal.GetSafeNormal();
			const FVector U = (FMath::Abs(Axis.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).Cross(Axis).GetSafeNormal() * Radius;
			const FVector V = Axis.Cross(U).GetSafeNormal() * Radius;
			const int32 First = Points.Num();
			Points.Add(Mid);
			Normals.Add(Axis);
			Colours.Add(Inner);
			for (int32 I = 0; I < Sides; ++I)
			{
				const double A = 2.0 * UE_DOUBLE_PI * I / Sides;
				Points.Add(Mid + U * FMath::Cos(A) + V * FMath::Sin(A));
				Normals.Add(Axis);
				Colours.Add(Rim);
			}
			for (int32 I = 0; I < Sides; ++I)
			{
				Triangles.Append({ First, First + 1 + I, First + 1 + (I + 1) % Sides });
			}
		}

		/** A flat ring on the sea (for M_Glow): its middle, radius and width, brightest along its middle line. */
		void Ring(const FVector& Mid, double Radius, double Width, int32 Sides, const FColor& Colour)
		{
			const int32 First = Points.Num();
			const FColor Edge(Colour.R, Colour.G, Colour.B, 0);
			for (int32 I = 0; I < Sides; ++I)
			{
				const double A = 2.0 * UE_DOUBLE_PI * I / Sides;
				const FVector Way(FMath::Cos(A), FMath::Sin(A), 0.0);
				Points.Append({ Mid + Way * (Radius - Width * 0.5), Mid + Way * Radius, Mid + Way * (Radius + Width * 0.5) });
				Normals.Append({ FVector::UpVector, FVector::UpVector, FVector::UpVector });
				Colours.Append({ Edge, Colour, Edge });
			}
			for (int32 I = 0; I < Sides; ++I)
			{
				const int32 A = First + 3 * I, B = First + 3 * ((I + 1) % Sides);
				for (int32 K = 0; K < 2; ++K)
				{
					Triangles.Append({ A + K, B + K, B + K + 1 });
					Triangles.Append({ A + K, B + K + 1, A + K + 1 });
				}
			}
		}

		bool IsEmpty() const { return Triangles.Num() == 0; }
	};

	/** Painted metal (alpha 128: the map's surface paint wears it as metal), cloth and rope (255: none). */
	static const FColor ChargeDark(46, 50, 50, 128);
	static const FColor ChuteCloth(222, 218, 204, 255);
	static const FColor NetRope(150, 132, 98, 255);
	static const FColor FlareCore(255, 246, 222, 255);
	static const FColor SwirlWater(196, 214, 218, 255);
}

// ---------------------------------------------------------------------------------------------------------------------
// The hooks

double FConvoyLooks::Steer(TUniquePtr<FConvoyLooks>& Held, USceneComponent* InRoot, AAirStage* InStage, FBattleEffects* InKit, const SeaShow::Show& InShow, double InUnitsPerMetre,
                           double InNow, bool bInSkip, bool& bOnWater, const SeaShow::ShipOut& Ship)
{
	if (!Held)
	{
		Held = MakeUnique<FConvoyLooks>();
	}
	FConvoyLooks& L = *Held;
	L.Root = InRoot;
	L.Stage = InStage;
	L.Kit = InKit;
	L.Show = &InShow;
	L.OnWater = &bOnWater;
	L.UnitsPerMetre = InUnitsPerMetre > 0.0 ? InUnitsPerMetre : 100.0;
	L.bSkipping = bInSkip;
	if (L.SteppedAt >= 0.0 && InNow + 5.0 < L.SteppedAt)
	{
		// (the clock gone back: another battle in the same scene)
		L.Clear();
	}
	L.Now = InNow;
	if (InNow != L.SteppedAt)
	{
		L.SteppedAt = InNow;
		L.Step();
	}
	if (Ship.Class == "ss")
	{
		if (const FBoatKept* B = L.Boats.Find(Ship.Key); B && B->SeenAt == InNow)
		{
			return B->Out.Heave;
		}
	}
	return Ship.Heave - Ship.Dived;
}

void FConvoyLooks::Effect(const SeaShow::Fx& E, double InNow, bool bInSkip)
{
	using ConvoyLook::P3;
	Now = InNow;
	switch (E.Kind)
	{
	case SeaShow::FxKind::Sink:
		End(E, bInSkip);
		break;
	// (what passes: none while a picture run skips ahead, as the scene's salvos and splashes)
	case SeaShow::FxKind::ChargeDrop:
		if (!bInSkip)
		{
			Drop(E);
		}
		break;
	case SeaShow::FxKind::ChargeBurst:
		if (!bInSkip)
		{
			Burst(E);
		}
		break;
	case SeaShow::FxKind::Starshell:
		if (!Stars.ContainsByPredicate([&E](const FStar& S) { return S.Key == E.Key; }))
		{
			FStar& S = Stars.AddDefaulted_GetRef();
			S.Key = E.Key;
			S.In.Burst = P3(E.At.X, E.At.Y, E.At.Z);
			S.In.T0 = Now;
			// (the show's burn-out; a show that gives none, the look's usual)
			S.In.Until = E.Until > Now + 1.0 ? E.Until : Now + ConvoyLook::StarBurnUsual();
			if (Show)
			{
				const SeaShow::Vec Blow = Show->Wind();
				S.In.Wind = P3(Blow.X, Blow.Y, 0.0);
			}
			S.In.Seed = static_cast<uint64>(static_cast<uint32>(E.Key)) * 2654435761ull + 11ull;
		}
		break;
	case SeaShow::FxKind::Dive:
		// (a boat forced to the surface has blown all her tanks: she rides high; one diving again floods them)
		if (E.What == "surface" && E.Text == "forced")
		{
			ConvoyLook::Blow(Boats.FindOrAdd(E.Key).State, true);
		}
		else if (E.What == "dive")
		{
			if (FBoatKept* B = Boats.Find(E.Key))
			{
				ConvoyLook::Blow(B->State, false);
			}
		}
		break;
	case SeaShow::FxKind::Rescue:
	{
		// (her nets go where the sea scene's men climb her side, while the scene has her standing by: StepNets)
		FRescue* R = Rescues.FindByPredicate([&E](const FRescue& X) { return X.Key == E.Key; });
		if (!R)
		{
			R = &Rescues.AddDefaulted_GetRef();
			R->Key = E.Key;
		}
		R->Until = FMath::Max(R->Until, E.Until > Now ? E.Until : Now + 600.0);
		break;
	}
	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The show's effects

void FConvoyLooks::Drop(const SeaShow::Fx& E)
{
	using namespace ConvoyLook;
	DropIn In;
	In.Kind = E.What;
	In.N = static_cast<int>(std::lround(E.Size));
	In.Spread = E.Spread;
	In.Sinking = E.Sinking;
	In.Until = E.Until;
	In.At = P3(E.At.X, E.At.Y, E.At.Z);
	In.Aim = P3(E.Aim.X, E.Aim.Y, 0.0);
	In.Vel = P3(E.Vel.X, E.Vel.Y, 0.0);
	if (const SeaShow::ShipOut* Escort = Show ? Show->Ship(E.Key) : nullptr)
	{
		// (her beam the show's, as it lays the bursts by it; her length as the stage draws her, for her throwers)
		In.Beam = Escort->Beam;
		In.Long = Escort->Long;
		double Drawn = 0.0;
		FVector Bow = FVector::ZeroVector;
		bool bHarbour = false;
		if (Stage && Stage->ShipLie(E.Key, Drawn, Bow, bHarbour) && Drawn > 1.0)
		{
			In.Long = Drawn;
		}
	}
	const uint64 Seed = static_cast<uint64>(static_cast<uint32>(E.Key)) * 7919ull + static_cast<uint64>(FMath::Max(0.0, Now) * 10.0);
	bool bFirstAhead = true;
	for (const DropShot& Shot : DropPlan(In, Seed))
	{
		FCharge& C = Charges.AddDefaulted_GetRef();
		C.T0 = Now;
		C.Shot = Shot;
		C.bAhead = Shot.Side == 2;
		// A thrower's discharge as it goes (the ahead thrower's once, as its ripple starts).
		if (Shot.Side != 0 && (!C.bAhead || bFirstAhead))
		{
			FDue& D = Due.AddDefaulted_GetRef();
			D.At = Now + Shot.Dt;
			D.Kind = EDue::Discharge;
			D.Where = Shot.From;
			D.Way = P3(Shot.To.X - Shot.From.X, Shot.To.Y - Shot.From.Y, 0.0) * (1.0 / FMath::Max(1.0, FMath::Sqrt(FMath::Square(Shot.To.X - Shot.From.X) + FMath::Square(Shot.To.Y - Shot.From.Y)))) + P3(0.0, 0.0, 0.8);
			bFirstAhead = bFirstAhead && !C.bAhead;
		}
	}
}

void FConvoyLooks::Burst(const SeaShow::Fx& E)
{
	using namespace ConvoyLook;
	const BurstLook Look = BurstOf(-E.At.Z, ChargeKg(E.What));
	// (her air comes up within the charge's lethal reach: the show's, as it places the booked burst by it)
	const double Lethal = Show && Show->Board().ChargeLethal > 0.0 ? Show->Board().ChargeLethal : 6.4;
	const uint64 Seed = static_cast<uint64>(static_cast<uint32>(E.Key)) * 104729ull ^ static_cast<uint64>(std::llround(std::fabs(E.At.X) * 3.0 + std::fabs(E.At.Y) * 7.0)) ^ static_cast<uint64>(FMath::Max(0.0, Now) * 100.0);
	for (const KitSpout& S : SpoutsOf(Look, E.Size > 0.5, Lethal, Seed))
	{
		FDue& D = Due.AddDefaulted_GetRef();
		D.At = Now + S.Dt;
		D.Where = P3(E.At.X + S.Dx, E.At.Y + S.Dy, 0.0);
		if (S.Row == "skip")
		{
			D.Kind = EDue::Spurt;
			D.Way = P3(S.WayX, S.WayY, 0.0);
		}
		else
		{
			D.Kind = EDue::Spout;
			D.Row = UTF8_TO_TCHAR(S.Row.c_str());
		}
	}
}

void FConvoyLooks::End(const SeaShow::Fx& E, bool bInSkip)
{
	using namespace ConvoyLook;
	// ("rammed" and "shelled" need nothing of these looks: the stage's sinking, the scene's oil, wreckage and men)
	if ((E.What != "crushed" && E.What != "scuttled") || Ended.Contains(E.Key))
	{
		return;
	}
	Ended.Add(E.Key);
	FBoatKept* B = Boats.Find(E.Key);
	if (E.What == "scuttled")
	{
		// Brought up: her tanks blown, she comes to the surface wherever the show still has her, and goes from there,
		// riding high while her crew get out over her side (the sea scene's men, from her deck's height).
		if (B)
		{
			ForceUp(B->State, Now, B->State.Sub);
			Blow(B->State, true);
		}
		return;
	}
	// Crushed: her air, a hump and the boils over where she was (none when a picture run skips past: they pass).
	if (bInSkip)
	{
		return;
	}
	const P3 Where = B ? B->At : P3(E.At.X, E.At.Y, 0.0);
	for (const BoilShot& S : CrushPlan(static_cast<uint64>(static_cast<uint32>(E.Key)) * 6151ull + 3ull))
	{
		FDue& D = Due.AddDefaulted_GetRef();
		D.At = Now + S.Dt;
		D.Kind = EDue::Spout;
		D.Where = P3(Where.X + S.Dx, Where.Y + S.Dy, 0.0);
		D.Row = TEXT("churn");
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Each step

void FConvoyLooks::Step()
{
	StepBoats();
	ConvoyLooksParts::FHoldOnWater Water(OnWater);
	FlyCharges();
	PlayDue();
	LightStars();
	StepNets();
	if (!bSkipping)
	{
		Lay();
	}
}

double FConvoyLooks::LitAt(const ConvoyLook::P3& At) const
{
	double Lit = 1.0;
	if (Show)
	{
		const SeaShow::Field& F = Show->Board();
		Lit = ConvoyLook::SkyLit(F.bLight, F.SunUp, F.MoonUp, F.MoonLit);
	}
	for (const FStar& S : Stars)
	{
		Lit += ConvoyLook::LitBy(ConvoyLook::StarAt(S.In, Now), At);
	}
	return FMath::Clamp(Lit, 0.0, 1.0);
}

void FConvoyLooks::StepBoats()
{
	using namespace ConvoyLook;
	if (!Show)
	{
		return;
	}
	const double ScopeKeel = Show->Board().ScopeKeel;
	for (const SeaShow::ShipOut& Ship : Show->Ships())
	{
		if (Ship.Class != "ss" || Ship.Dive.empty())
		{
			continue;
		}
		// (the stage draws her with the battle modeller's boat where the game has one (AirStage's ShipFileOf: Submarine_GER,
		// Submarine_USA, Submarine), fitted only if she is a named boat; else as "Submarine:<her navy>" (its KindOf), built
		// in code by her navy (Campaign/CampaignShips.cpp) and fitted to AirFeed::HullOf's size. The looks' rows are that
		// model's, SculptedOf's or ModelOf's: so a boat the scene has not yet laid on the stage this frame waits for it.)
		FString File;
		FVector Stretch = FVector::OneVector;
		const bool bLaid = Stage && Stage->ShipModel(Ship.Key, File, Stretch);
		const FBoatKept* Kept = Boats.Find(Ship.Key);
		if (Stage && !bLaid && (!Kept || Kept->SeenAt < 0.0))
		{
			continue;
		}
		FBoatKept& B = Boats.FindOrAdd(Ship.Key);
		if (B.SeenAt < 0.0)
		{
			B.Model = BoatModel();
			if (bLaid && !File.IsEmpty())
			{
				B.Model = SculptedOf(std::string(TCHAR_TO_UTF8(*FPaths::GetBaseFilename(File))));
				B.Drawn.Along = Stretch.X;
				B.Drawn.Up = Stretch.Z;
			}
			if (B.Model.Name.empty())
			{
				B.Model = ModelOf(Ship.Nation);
				const AirFeed::HullSize Hull = AirFeed::HullOf("ss", std::string(), Ship.Nation);
				double DrawnLong = Hull.Long;
				FVector Bow = FVector::ZeroVector;
				bool bHarbour = false;
				if (!Stage || !Stage->ShipLie(Ship.Key, DrawnLong, Bow, bHarbour) || DrawnLong <= 1.0)
				{
					DrawnLong = Hull.Long;
				}
				B.Drawn = DrawnOf(B.Model, DrawnLong, Hull.Beam);
			}
		}
		BoatIn In;
		In.T = Now;
		In.Dive = Ship.Dive;
		In.Keel = Ship.Keel;
		In.Dived = Ship.Dived;
		In.bScope = Ship.bScope;
		In.Heave = Ship.Heave;
		In.Pitch = Ship.Pitch;
		In.Speed = Ship.Speed;
		In.At = P3(Ship.At.X, Ship.At.Y, 0.0);
		In.Heading = Ship.Heading;
		In.ScopeKeel = ScopeKeel;
		// (the light over her, only worth working out while she runs deep)
		In.Lit = Ship.Dive == "deep" ? LitAt(In.At) : 0.0;
		B.Out = StepBoat(B.State, In, B.Model, B.Drawn);
		B.At = In.At;
		B.SeenAt = Now;
		for (const BoatSplash& Sp : B.Out.Splashes)
		{
			FDue& D = Due.AddDefaulted_GetRef();
			D.At = Sp.T;
			D.Where = Sp.At;
			D.Way = Sp.Way;
			if (Sp.Kind == SplashKind::Feather || Sp.Kind == SplashKind::Bow)
			{
				D.Kind = EDue::Spurt;
			}
			else
			{
				// (the water off her casing, and the swirl where her bridge went under: the kit's churn, its foam lasting)
				D.Kind = EDue::Spout;
				D.Row = TEXT("churn");
			}
		}
	}
	for (auto It = Boats.CreateIterator(); It; ++It)
	{
		if (Now - It.Value().SeenAt > ConvoyLooksParts::ForgetAfter)
		{
			It.RemoveCurrent();
		}
	}
}

void FConvoyLooks::PlayDue()
{
	for (int32 I = Due.Num() - 1; I >= 0; --I)
	{
		const FDue& D = Due[I];
		if (D.At > Now)
		{
			continue;
		}
		// (what was due while a picture run skipped is gone with the moment, as the scene's columns are)
		if (!bSkipping && Kit)
		{
			const FVector Where = World(D.Where);
			const FVector Way = World(D.Way).GetSafeNormal();
			switch (D.Kind)
			{
			case EDue::Spout:
				Kit->ShellOfSize(Where, D.Row, TEXT("water"));
				break;
			case EDue::Spurt:
				Kit->Skip(Where, Way.IsNearlyZero() ? FVector::ForwardVector : Way, TEXT("water"));
				break;
			case EDue::Discharge:
				Kit->Muzzle(Where, Way.IsNearlyZero() ? FVector::UpVector : Way, TEXT("mortar"), TEXT("water"));
				break;
			}
		}
		Due.RemoveAtSwap(I);
	}
}

void FConvoyLooks::FlyCharges()
{
	if (bSkipping)
	{
		Charges.Reset();
		return;
	}
	for (int32 I = Charges.Num() - 1; I >= 0; --I)
	{
		const FCharge& C = Charges[I];
		if (Now >= C.T0 + C.Shot.Dt + C.Shot.Flight)
		{
			// In the water: a low spurt where it went in, along its way.
			FDue& D = Due.AddDefaulted_GetRef();
			D.At = Now;
			D.Kind = EDue::Spurt;
			D.Where = C.Shot.To;
			D.Way = ConvoyLook::P3(C.Shot.To.X - C.Shot.From.X, C.Shot.To.Y - C.Shot.From.Y, 0.0);
			Charges.RemoveAtSwap(I);
		}
	}
}

void FConvoyLooks::LightStars()
{
	using namespace ConvoyLooksParts;
	struct FLit
	{
		FVector At;
		double Light = 0.0;
		double T0 = 0.0;
	};
	TArray<FLit> Lit;
	for (int32 I = Stars.Num() - 1; I >= 0; --I)
	{
		FStar& S = Stars[I];
		const ConvoyLook::StarOut O = ConvoyLook::StarAt(S.In, Now);
		const int32 TrailId = StarTrailIds + S.Key % 100000;
		if (!O.bShown && Now > S.In.T0)
		{
			if (S.bTrail && Kit)
			{
				Kit->TrailStop(TrailId);
			}
			Stars.RemoveAtSwap(I);
			continue;
		}
		const FVector At = World(O.Flare);
		// Its smoke trail (the kit's thin smoke) while it burns; stopped across a picture run's skip, so the trail is never
		// drawn along the jump.
		if (Kit)
		{
			if (O.bBurning && !bSkipping)
			{
				// (its way a second, from where it will be a quarter-second on)
				const FVector Way = (World(ConvoyLook::StarAt(S.In, Now + 0.25).Flare) - At) * 4.0;
				if (!S.bTrail)
				{
					Kit->TrailStart(Now, TrailId, At, Way, TEXT("smoke"), 0.0f);
					S.bTrail = true;
				}
				else
				{
					Kit->TrailMove(Now, TrailId, At, Way);
				}
			}
			else if (S.bTrail)
			{
				Kit->TrailStop(TrailId);
				S.bTrail = false;
			}
		}
		if (O.bBurning && O.Light > 0.0)
		{
			Lit.Add(FLit{ At, O.Light, S.In.T0 });
		}
	}
	// The lamps: the newest burning ones, each at its flare, by its light.
	Lit.Sort([](const FLit& A, const FLit& B) { return A.T0 > B.T0; });
	USceneComponent* Parent = Root.Get();
	AActor* Owner = Parent ? Parent->GetOwner() : nullptr;
	float Asked = static_cast<float>(StarCandela);
	FParse::Value(FCommandLine::Get(), TEXT("TheaterStarCandela="), Asked);
	const double Candela = FMath::Max(0.0, static_cast<double>(Asked)) * FMath::Square(UnitsPerMetre / 100.0); // (candelas at 100 units a metre, as the kit's lamps)
	for (int32 I = 0; I < FMath::Min(Lit.Num(), MostStarLamps); ++I)
	{
		if (I >= Lamps.Num() && Owner)
		{
			// (made as the kit makes its flashes' lamps: shadowless, by candelas, falling off as the square)
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
			Lamps.Add(Lamp);
		}
		if (UPointLightComponent* Lamp = Lamps.IsValidIndex(I) ? Lamps[I].Get() : nullptr)
		{
			Lamp->SetWorldLocation(Lit[I].At);
			Lamp->SetLightColor(StarColour);
			Lamp->SetAttenuationRadius(static_cast<float>(StarReach * UnitsPerMetre));
			Lamp->SetIntensity(static_cast<float>(Lit[I].Light * Candela));
			Lamp->SetVisibility(true);
		}
	}
	for (int32 I = FMath::Min(Lit.Num(), MostStarLamps); I < Lamps.Num(); ++I)
	{
		if (UPointLightComponent* Lamp = Lamps[I].Get())
		{
			Lamp->SetVisibility(false);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The mesh

void FConvoyLooks::Begin()
{
	bBegun = true;
	USceneComponent* Parent = Root.Get();
	AActor* Owner = Parent ? Parent->GetOwner() : nullptr;
	if (!Owner)
	{
		return;
	}
	// One mesh in world places (it takes no turn or scale from its parent), never in shadow or collision, as the kit's.
	UProceduralMeshComponent* Made = NewObject<UProceduralMeshComponent>(Owner, MakeUniqueObjectName(Owner, UProceduralMeshComponent::StaticClass(), TEXT("ConvoyLooks")));
	Made->bUseAsyncCooking = false;
	Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Made->SetCastShadow(false);
	Made->SetupAttachment(Parent);
	Made->SetUsingAbsoluteLocation(true);
	Made->SetUsingAbsoluteRotation(true);
	Made->SetUsingAbsoluteScale(true);
	Made->RegisterComponent();
	Made->SetWorldTransform(FTransform::Identity);
	Mesh = Made;
	// Its solid things (the charges, the parachutes, the nets): the map's surface paint, lit, as the
	// scene's aircraft are. The flares' glow and the swirls: added light (M_Glow), each at its own strength. Held in the
	// mesh's own list before it has any section.
	UMaterialInterface* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_MapSurface.M_MapSurface"));
	if (!Surface)
	{
		Surface = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	UMaterialInstanceDynamic* Solid = UMaterialInstanceDynamic::Create(Surface, Made);
	Solid->SetScalarParameterValue(TEXT("Roughness"), 0.6f);
	Solid->SetScalarParameterValue(TEXT("Specular"), 0.35f);
	Solid->SetScalarParameterValue(TEXT("Grain"), 0.0f);
	Solid->SetScalarParameterValue(TEXT("Tooth"), 0.0f);
	Made->SetMaterial(ConvoyLooksParts::SolidSection, Solid);
	if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Glow.M_Glow")))
	{
		float Flare = ConvoyLooksParts::StarGlow, Swirl = ConvoyLooksParts::SwirlGlow;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterStarGlow="), Flare);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterSwirlGlow="), Swirl);
		UMaterialInstanceDynamic* Lit = UMaterialInstanceDynamic::Create(Glow, Made);
		Lit->SetScalarParameterValue(TEXT("Strength"), Flare);
		Made->SetMaterial(ConvoyLooksParts::GlowSection, Lit);
		UMaterialInstanceDynamic* Faint = UMaterialInstanceDynamic::Create(Glow, Made);
		Faint->SetScalarParameterValue(TEXT("Strength"), Swirl);
		Made->SetMaterial(ConvoyLooksParts::SwirlSection, Faint);
	}
	else
	{
		UE_LOG(LogTheater, Warning, TEXT("Convoy looks: M_Glow is missing (run Tools/build_content.sh): a starshell shows its lamp and smoke, no glow, and a deep boat no swirl."));
	}
}

void FConvoyLooks::Clear()
{
	for (FStar& S : Stars)
	{
		if (S.bTrail && Kit)
		{
			Kit->TrailStop(ConvoyLooksParts::StarTrailIds + S.Key % 100000);
		}
	}
	for (TWeakObjectPtr<UPointLightComponent>& Lamp : Lamps)
	{
		if (UPointLightComponent* L = Lamp.Get())
		{
			L->SetVisibility(false);
		}
	}
	Boats.Reset();
	Due.Reset();
	Charges.Reset();
	Stars.Reset();
	Rescues.Reset();
	Ended.Reset();
	if (UProceduralMeshComponent* M = Mesh.Get())
	{
		M->ClearAllMeshSections();
	}
	SectionsLaid = 0;
	for (ConvoyLook::MeshShape& Shape : Shapes)
	{
		Shape.Forget();
	}
	SteppedAt = -1.0;
}

void FConvoyLooks::StepNets()
{
	using namespace ConvoyLook;
	if (!Show)
	{
		return;
	}
	std::vector<ManIn> Near;
	for (int32 I = Rescues.Num() - 1; I >= 0; --I)
	{
		FRescue& R = Rescues[I];
		const SeaShow::ShipOut* Ship = Show->Ship(R.Key);
		const bool bRescue = Now <= R.Until;
		if (!Ship || (!bRescue && R.Nets.empty()))
		{
			Rescues.RemoveAtSwap(I);
			continue;
		}
		RescuerIn In;
		In.At = P3(Ship->At.X, Ship->At.Y, 0.0);
		In.Heading = Ship->Heading;
		In.Long = Ship->Long;
		In.Beam = Ship->Beam;
		In.Speed = Ship->Speed;
		// (the scene's men near her: climbing her side, or swimming for it)
		Near.clear();
		const double Reach = 0.5 * Ship->Long + 80.0;
		for (const SeaShow::ManOut& Man : Show->Men())
		{
			if (Man.Doing != SeaShow::Swim::Swim && Man.Doing != SeaShow::Swim::Climb)
			{
				continue;
			}
			if (FMath::Abs(Man.At.X - In.At.X) > Reach || FMath::Abs(Man.At.Y - In.At.Y) > Reach)
			{
				continue;
			}
			ManIn& M = Near.emplace_back();
			M.At = P3(Man.At.X, Man.At.Y, Man.At.Z);
			M.Heading = Man.Heading;
			M.Doing = Man.Doing == SeaShow::Swim::Climb ? ManDoing::Climb : ManDoing::Swim;
		}
		const double Dt = R.SteppedAt >= 0.0 ? FMath::Clamp(Now - R.SteppedAt, 0.0, 10.0) : 0.0;
		R.SteppedAt = Now;
		ConvoyLook::StepNets(R.Nets, In, Near, bRescue, Dt);
	}
}

void FConvoyLooks::Lay()
{
	using namespace ConvoyLooksParts;
	using ConvoyLook::P3;
	const double U = UnitsPerMetre;
	FLaid Solid, Glow, Faint;
	// Deep boats: the faint swirls over them where light falls. (A boat's periscopes are her model's own, on the stage.)
	for (const TPair<int32, FBoatKept>& Kept : Boats)
	{
		const FBoatKept& B = Kept.Value;
		if (B.SeenAt != Now)
		{
			continue;
		}
		for (const ConvoyLook::Swirl& W : B.State.Swirls)
		{
			double Radius = 0.0, Shown = 0.0;
			if (ConvoyLook::SwirlAt(W, Now, Radius, Shown) && Shown > 0.005)
			{
				const uint8 Alpha = static_cast<uint8>(FMath::Clamp(Shown * SwirlAlpha * 255.0, 0.0, 255.0));
				Faint.Ring(World(P3(W.At.X, W.At.Y, 0.15)), Radius * U, SwirlBand * U, 24, FColor(SwirlWater.R, SwirlWater.G, SwirlWater.B, Alpha));
			}
		}
	}
	// The charges in the air: a drum (a pattern's) or a slim bomb (the ahead thrower's).
	for (const FCharge& C : Charges)
	{
		const double Age = Now - C.T0 - C.Shot.Dt;
		if (Age < 0.0)
		{
			continue;
		}
		const P3 At = ConvoyLook::Arc(C.Shot.From, C.Shot.To, C.Shot.High, C.Shot.Flight, Age);
		const P3 Next = ConvoyLook::Arc(C.Shot.From, C.Shot.To, C.Shot.High, C.Shot.Flight, Age + 0.05);
		const FVector Mid = World(At);
		FVector Way = World(Next) - Mid;
		if (Way.IsNearlyZero())
		{
			Way = FVector::DownVector;
		}
		Way.Normalize();
		const double Half = (C.bAhead ? 0.45 : 0.35) * LookScale * U;
		Solid.Rod(Mid - Way * Half, Mid + Way * Half, (C.bAhead ? 0.09 : 0.23) * LookScale * U, 8, ChargeDark);
	}
	// The starshells: the flare's glow (three crossed discs of light, so it reads round from anywhere), a small white-hot
	// core, and its parachute above it on four lines.
	for (const FStar& S : Stars)
	{
		const ConvoyLook::StarOut O = ConvoyLook::StarAt(S.In, Now);
		if (!O.bShown)
		{
			continue;
		}
		const FVector At = World(O.Flare), Crown = World(O.Chute);
		const double ChuteR = 0.9 * LookScale * U, ChuteH = 0.45 * LookScale * U;
		const FVector Rim = Crown - FVector(0.0, 0.0, ChuteH);
		Solid.Dome(Rim, ChuteR, ChuteH, 10, ChuteCloth);
		for (int32 I = 0; I < 4; ++I)
		{
			const double A = UE_DOUBLE_HALF_PI * I;
			Solid.Rod(Rim + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * ChuteR, At, 0.02 * LookScale * U, 3, ChuteCloth);
		}
		Solid.Rod(At - FVector(0.0, 0.0, 0.25 * U), At + FVector(0.0, 0.0, 0.25 * U), 0.08 * LookScale * U, 6, ChargeDark);
		if (O.bBurning && O.Light > 0.0)
		{
			const uint8 Alpha = static_cast<uint8>(FMath::Clamp(O.Light * 255.0, 0.0, 255.0));
			const double Halo = (2.5 + 3.5 * O.Light) * U; // [EST] the glow round it, 6 m at full
			for (const FVector& Normal : { FVector::UpVector, FVector::ForwardVector, FVector::RightVector })
			{
				Glow.Fan(At, Normal, Halo, 16, FColor(255, 238, 200, Alpha), FColor(255, 214, 150, 0));
				Glow.Fan(At, Normal, 0.6 * U, 10, FColor(FlareCore.R, FlareCore.G, FlareCore.B, Alpha), FColor(255, 240, 210, static_cast<uint8>(Alpha / 2)));
			}
		}
	}
	// A rescuer's scrambling nets down her side where the scene's men climb it, riding her heave.
	for (const FRescue& R : Rescues)
	{
		const SeaShow::ShipOut* Ship = Show ? Show->Ship(R.Key) : nullptr;
		if (!Ship)
		{
			continue;
		}
		ConvoyLook::NetIn In;
		In.Ship = P3(Ship->At.X, Ship->At.Y, 0.0);
		In.Heading = Ship->Heading;
		// (hung on her side as the stage draws her where that is wider than the show has her, so it is never inside her)
		In.Beam = FMath::Max(Ship->Beam, AirFeed::HullOf(Ship->Class, std::string(), Ship->Nation).Beam);
		In.Free = ConvoyLook::FreeOf(Ship->Class);
		const FVector Lift(0.0, 0.0, Ship->Heave * U);
		for (const ConvoyLook::NetHung& N : R.Nets)
		{
			In.Side = N.Side;
			In.Along = N.Along;
			In.Out = N.Out;
			for (const ConvoyLook::Rope& Line : ConvoyLook::NetOf(In))
			{
				Solid.Rod(World(Line.From) + Lift, World(Line.To) + Lift, 0.04 * LookScale * U, 3, NetRope);
			}
		}
	}
	const FLaid* Laid[Sections] = { &Solid, &Glow, &Faint };
	UProceduralMeshComponent* M = Mesh.Get();
	if (!M && !bBegun && (!Solid.IsEmpty() || !Glow.IsEmpty() || !Faint.IsEmpty()))
	{
		// (made only once there is something to draw: a fleet action's scene never has it)
		Begin();
		M = Mesh.Get();
	}
	if (!M)
	{
		return;
	}
	static_assert(Sections == sizeof(Shapes) / sizeof(Shapes[0]), "one kept shape for each section");
	for (int32 I = 0; I < Sections; ++I)
	{
		const FLaid& L = *Laid[I];
		if (!L.IsEmpty())
		{
			// (made afresh only when its shape changed: while the same points and triangles hold, a hung net or a burning
			// flare only moved, only where they are, which way they face and their paint are sent, as SeaBattle.cpp's
			// wreck mesh is)
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

FVector FConvoyLooks::World(const ConvoyLook::P3& P) const
{
	// (as ASeaBattle::World: X up the board, the board's north; Y across it, the board's east)
	return FVector(P.Y * UnitsPerMetre, P.X * UnitsPerMetre, P.Z * UnitsPerMetre);
}
