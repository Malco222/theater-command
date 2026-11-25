#include "Battle/HeavyWeather.h"

#include "Battle/AirStage.h"
#include "Battle/SeaShow.h"
#include "Battle/ShipIce.h"
#include "Effects/BattleEffects.h"
#include "TheaterCommand.h"

namespace HeavyWeatherParts
{
	/** A board point as the scene's world (ASeaBattle::World: X up the board, the board's north; Y across it, east). */
	static FVector World(double X, double Y, double Z, double Units)
	{
		return FVector(Y * Units, X * Units, Z * Units);
	}

	/** The scene's bOnWater held while a spurt beside her stem is laid, so it goes on the sea and never on her deck; let
	 * go for one laid on her forecastle, so the kit's ground there is her deck (the stage's DeckUnder). */
	struct FHoldWater
	{
		bool& Flag;
		bool bWas;
		FHoldWater(bool& InFlag, bool bOn) : Flag(InFlag), bWas(InFlag) { Flag = bOn; }
		~FHoldWater() { Flag = bWas; }
		FHoldWater(const FHoldWater&) = delete;
		FHoldWater& operator=(const FHoldWater&) = delete;
	};

	/** A sheet through the kit's naval spray row where it has one (the requires-probe); false on a kit without it. */
	template <typename TKit>
	static bool SprayRow(TKit* Kit, const FVector& At, const FVector& Way, const FVector& Drift, float Top, float Fan, float Seconds, const FString& Kind)
	{
		if constexpr (FleetSpray::HasSprayRow<TKit, FVector, FString>)
		{
			Kit->BowSpray(At, Way, Drift, Top, Fan, Seconds, Kind);
			return true;
		}
		else
		{
			(void)Kit;
			(void)At;
			(void)Way;
			(void)Drift;
			(void)Top;
			(void)Fan;
			(void)Seconds;
			(void)Kind;
			return false;
		}
	}
}

void FHeavyWeather::Open(TUniquePtr<FHeavyWeather>& Held, bool bSpray, bool bGale, uint64 Seed)
{
	Held.Reset();
	if (!bSpray)
	{
		return;
	}
	Held = MakeUnique<FHeavyWeather>();
	FleetSpray::Settings Given;
	Given.bGale = bGale;
	Given.bSprayRow = FleetSpray::HasSprayRow<FBattleEffects, FVector, FString>;
	Given.Seed = Seed;
	Held->Core = FleetSpray::Spray(Given);
	UE_LOG(LogTheater, Display, TEXT("Sea spray: bow sheets on (-TheaterSeaSpray=1), through the kit's %s; Beaufort 7 and over (green seas, spindrift) %s."),
		Given.bSprayRow ? TEXT("naval spray row") : TEXT("Bow row (its low spurts on the water: the kit has no naval spray row yet)"),
		bGale ? TEXT("with the gale") : TEXT("held back: the wind held to 13.8 m/s without the gale (-TheaterSeaGale)"));
}

void FHeavyWeather::Ship(TUniquePtr<FHeavyWeather>& Held, FBattleEffects* Kit, const SeaShow::Show& Show, const SeaShow::ShipOut& Ship, double UnitsPerMetre, double Now,
                         bool bSkip, bool& bOnWater)
{
	(void)Ship;
	if (!Held || !Kit)
	{
		return;
	}
	if (Now != Held->SteppedAt)
	{
		Held->SteppedAt = Now;
		Held->Step(Kit, Show, UnitsPerMetre > 0.0 ? UnitsPerMetre : 100.0, Now, bSkip, bOnWater);
	}
}

void FHeavyWeather::Step(FBattleEffects* Kit, const SeaShow::Show& Show, double UnitsPerMetre, double Now, bool bSkip, bool& bOnWater)
{
	using namespace HeavyWeatherParts;
	// Every ship of the show as the spray sees her: today's table ride (step 5's look ride drops in here), the still face
	// at her stem (the drawn water, SeaNear, once the scene draws the swell).
	Hulls.clear();
	for (const SeaShow::ShipOut& S : Show.Ships())
	{
		FleetSpray::Hull& H = Hulls.emplace_back();
		H.Key = S.Key;
		H.Class = S.Class;
		H.Long = S.Long;
		H.Beam = S.Beam;
		H.X = S.At.X;
		H.Y = S.At.Y;
		H.Heading = FMath::DegreesToRadians(S.Heading);
		H.Speed = S.Speed;
		H.Heave = S.Heave - S.Dived;
		H.Pitch = S.Pitch;
		H.WaterAtStem = 0.0;
		H.bTableRide = true;
		H.bThrows = S.bShown && (S.Doing == SeaShow::Fate::Line || S.Doing == SeaShow::Fate::Limping) && (S.Class != "ss" || S.Dive.empty() || S.Dive == "surfaced");
	}
	FleetSpray::Wind W;
	W.U = Show.WindSpeed();
	W.From = Show.WindFrom();
	W.bGale = Core.Set().bGale;
	Core.Step(Now, Hulls, Show.Eye.X, Show.Eye.Y, W, bSkip, Calls);
	const double U = UnitsPerMetre;
	for (const FleetSpray::Call& C : Calls)
	{
		const FVector At = World(C.X, C.Y, C.Z, U);
		const FVector Way = World(C.WayX, C.WayY, C.WayZ, 1.0).GetSafeNormal();
		switch (C.Kit)
		{
		case FleetSpray::Row::Sheet:
			SprayRow(Kit, At, Way, World(C.DriftX, C.DriftY, 0.0, U), static_cast<float>(C.Top), static_cast<float>(C.Fan), static_cast<float>(C.Life),
				FString(UTF8_TO_TCHAR(FleetSpray::KindWord(C.Is))));
			break;
		case FleetSpray::Row::SeaSpurt:
		{
			FHoldWater Hold(bOnWater, true);
			Kit->Skip(At, Way.IsNearlyZero() ? FVector::ForwardVector : Way, TEXT("water"));
			break;
		}
		case FleetSpray::Row::DeckSpurt:
		{
			FHoldWater Hold(bOnWater, false);
			Kit->Skip(At, Way.IsNearlyZero() ? FVector::ForwardVector : Way, TEXT("water"));
			break;
		}
		}
	}
}

void FHeavyWeather::Ice(AAirStage* Stage, const SeaShow::Field& Board, double WindU, double WindFrom, double BeginT)
{
	const ShipIce::Frost Frost = ShipIce::FrostOf(Board.Lat, Board.Lon, Board.Month, WindU, WindFrom, BeginT);
	if (Stage)
	{
		Stage->SetFrost(Frost);
	}
	UE_LOG(LogTheater, Display, TEXT("%s"), UTF8_TO_TCHAR(ShipIce::Line(Frost, Board.Month).c_str()));
}

void FHeavyWeather::Say(const TUniquePtr<FHeavyWeather>& Held)
{
	if (Held)
	{
		UE_LOG(LogTheater, Display, TEXT("%s"), UTF8_TO_TCHAR(Held->Core.Line().c_str()));
	}
}
