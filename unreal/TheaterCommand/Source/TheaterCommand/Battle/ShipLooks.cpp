#include "Battle/ShipLooks.h"

#include "Effects/BattleEffects.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TheaterCommand.h"

namespace ShipLookParts
{
	// The effects kit's newer calls (the effects artist, 20 November 14:39Z), made only where the kit has them. A call
	// whose arguments differ from these is quietly off (no error says so): Begin logs which the kit has, so the Mac's
	// log proves the hand-over (the pre-check, 20 November 15:19Z).
	template <typename TKit>
	constexpr bool HasSteam = requires(TKit* K, int32 I, const FVector& A, const FVector& W, float C, float S, const FString& Kind) { K->Steam(I, A, W, C, S, Kind); K->SteamStop(I); };
	template <typename TKit>
	constexpr bool HasCookOff = requires(TKit* K, const FVector& A, const FVector& U, float C, int32 P, float O, float J) { K->CookOff(A, U, C, P, O, J); };
	template <typename TKit>
	constexpr bool HasBurnOut = requires(TKit* K, const FVector& A, float C, float S, int32 I) { K->Burn(A, C, S, I); K->BurnOut(I, C); };
	template <typename TKit>
	constexpr bool HasGlance = requires(TKit* K, const FVector& A, const FVector& I, const FVector& N, float Mm, const FString& G) { K->Glance(A, I, N, Mm, G); };

	/** Kind: the kit's "vent" (a deck vent, a hole in her side) or "funnel" (the escape pipes on a funnel: a tall plume). */
	template <typename TKit>
	static bool Steam(TKit* Kit, int32 Id, const FVector& At, const FVector& Way, float Across, float Seconds, const FString& Kind)
	{
		if constexpr (HasSteam<TKit>)
		{
			if (Kit)
			{
				Kit->Steam(Id, At, Way, Across, Seconds, Kind);
				return true;
			}
		}
		return false;
	}

	template <typename TKit>
	static void SteamStop(TKit* Kit, int32 Id)
	{
		if constexpr (HasSteam<TKit>)
		{
			if (Kit)
			{
				Kit->SteamStop(Id);
			}
		}
	}

	template <typename TKit>
	static bool CookOff(TKit* Kit, const FVector& At, float Across, int32 Pops, float Over, float Jet)
	{
		if constexpr (HasCookOff<TKit>)
		{
			if (Kit)
			{
				Kit->CookOff(At, FVector::UpVector, Across, Pops, Over, Jet);
				return true;
			}
		}
		return false;
	}

	/** A fire with an id, so that it can be beaten out on cue; false where the kit has none (a plain Burn then). */
	template <typename TKit>
	static bool BurnWithId(TKit* Kit, const FVector& At, float Across, float Seconds, int32 Id)
	{
		if constexpr (HasBurnOut<TKit>)
		{
			if (Kit)
			{
				Kit->Burn(At, Across, Seconds, Id, true); // (true: a ship's deck fire, never a burning tank's look: the kit's 08a18f21)
				return true;
			}
		}
		return false;
	}

	template <typename TKit>
	static void BurnOut(TKit* Kit, int32 Id, float Over)
	{
		if constexpr (HasBurnOut<TKit>)
		{
			if (Kit)
			{
				Kit->BurnOut(Id, Over);
			}
		}
	}

	/** Calibre: the table's glance dial ("gun203": a cruiser's 203 mm shell), as the kit's number in millimetres. */
	template <typename TKit>
	static bool Glance(TKit* Kit, const FVector& At, const FVector& Incoming, const FVector& Normal, const FString& Calibre)
	{
		if constexpr (HasGlance<TKit>)
		{
			const float Mm = FCString::Atof(*Calibre.RightChop(3));
			if (Kit && Calibre.StartsWith(TEXT("gun")) && Mm > 0.0f)
			{
				Kit->Glance(At, Incoming, Normal, Mm, FString(TEXT("water")));
				return true;
			}
		}
		return false;
	}

	/** The table's family for the air battle's class ("ag", the old battleship Utah, is a battleship). */
	static std::string FamilyOf(const FString& Class)
	{
		return Class == TEXT("ag") ? std::string("bb") : std::string(TCHAR_TO_UTF8(*Class));
	}

	/** The table's weapon for the air battle's strike: a dive bomber's bomb, a level bomber's heavy one, a torpedo. */
	static std::string WeaponOf(const FString& Strike)
	{
		return Strike == TEXT("torpedo") ? std::string("torpedo") : Strike == TEXT("dive") ? std::string("bomb") : Strike == TEXT("level") ? std::string("bomb_heavy") : std::string("bomb");
	}

	/** How far along her a place lies, as a share of her length from amidships (minus toward the bow, her model's -X). */
	static double AlongOf(const FString& At, uint32 Mix)
	{
		if (At == TEXT("bow")) return -0.44;
		if (At == TEXT("fore")) return -0.27;
		if (At == TEXT("turret_fore")) return -0.31;
		if (At == TEXT("bridge")) return -0.12;
		if (At == TEXT("funnel")) return 0.04;
		if (At == TEXT("aft")) return 0.27;
		if (At == TEXT("turret_aft")) return 0.31;
		if (At == TEXT("stern")) return 0.44;
		if (At == TEXT("deck") || At == TEXT("hangar"))
		{
			// (anywhere along her deck, fixed by the step: never the same spot twice running)
			return -0.4 + 0.8 * static_cast<double>(Mix % 1000u) / 999.0;
		}
		return 0.0; // mid, waterline
	}

	/** How high above her waterline a place lies (metres). */
	static double HeightOf(const FString& At, double High)
	{
		const double Deck = FMath::Min(High * 0.3, 9.0);
		if (At == TEXT("bridge")) return FMath::Min(High * 0.5, 16.0);
		if (At == TEXT("funnel")) return FMath::Min(High * 0.55, 18.0);
		if (At == TEXT("turret_fore") || At == TEXT("turret_aft")) return Deck + 1.5;
		if (At == TEXT("waterline")) return 0.5;
		return Deck;
	}
}

bool FShipLooks::Begin(uint64 Seed)
{
	End();
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data/Battle/fates.json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTheater, Warning, TEXT("Ship looks: no %s; hits keep their plain look."), *Path);
		return false;
	}
	TUniquePtr<Fates::Table> Loaded = MakeUnique<Fates::Table>();
	std::string Why;
	if (!Loaded->Load(std::string(TCHAR_TO_UTF8(*Text)), Why))
	{
		UE_LOG(LogTheater, Warning, TEXT("Ship looks: %s does not read (%s); hits keep their plain look."), *Path, UTF8_TO_TCHAR(Why.c_str()));
		return false;
	}
	Table = MoveTemp(Loaded);
	Picker = MakeUnique<Fates::Director>(*Table, Seed);
	UE_LOG(LogTheater, Log, TEXT("Ship looks: %d looks read."), static_cast<int32>(Table->Ways().size()));
	auto Has = [](bool bHas) { return bHas ? TEXT("yes") : TEXT("no"); };
	UE_LOG(LogTheater, Log, TEXT("Ship looks: the effects kit has steam %s, cook-off %s, a fire beaten out %s, a glancing shot %s."),
		Has(ShipLookParts::HasSteam<FBattleEffects>), Has(ShipLookParts::HasCookOff<FBattleEffects>), Has(ShipLookParts::HasBurnOut<FBattleEffects>), Has(ShipLookParts::HasGlance<FBattleEffects>));
	return true;
}

void FShipLooks::End()
{
	Picker.Reset();
	Table.Reset();
	Due.Reset();
	Hulls.Reset();
	Sides.Reset();
	Fires.Reset();
	Ends.Reset();
	ReelAt.Reset();
	Burns.Reset();
	Leaks.Reset();
	LeakStops.Reset();
	CookStops.Reset();
	Kit = nullptr;
	NextBurn = 0;
}

void FShipLooks::Hit(int32 Id, const FString& Class, const FString& Strike, bool bHarbour, double Hurt, double Now)
{
	if (!Picker.IsValid())
	{
		return;
	}
	Fates::Blow Blow;
	Blow.Domain = "sea";
	Blow.Family = ShipLookParts::FamilyOf(Class);
	Blow.Weapon = ShipLookParts::WeaponOf(Strike);
	Blow.Result = "worn";
	Blow.Hurt = FMath::Clamp(Hurt, 0.0, 1.0);
	// (a crew's nerve is not in the air battle's view of a ship: a steady crew, a fire beaten more often than not)
	Blow.Nerve = 0.65;
	Blow.bHarbour = bHarbour;
	Blow.Unit = Id;
	const Fates::Fate Look = Picker->Choose(Blow);
	if (Look.Key.empty())
	{
		return;
	}
	if (Ends.Contains(Id))
	{
		// A hit that lands after her end (each strike's own wait: a dive bomber's 16 s after a torpedo has ended her):
		// its burst only, on a ship burning, settled at her berth or going down; her end keeps her pose and her fires.
		Queue(Id, Look, Now, true);
		UE_LOG(LogTheater, Log, TEXT("Ship looks: ship %d, %s after her end, %s (its burst only)."), Id, *Strike, UTF8_TO_TCHAR(Look.Key.c_str()));
		return;
	}
	Hulls.FindOrAdd(Id).Take(Look, Now);
	if (Look.Dials.count("side"))
	{
		Sides.Add(Id, Look.Dial("side", 1.0) < 0.0 ? -1.0 : 1.0);
	}
	Queue(Id, Look, Now);
	UE_LOG(LogTheater, Log, TEXT("Ship looks: ship %d, %s, %s."), Id, *Strike, UTF8_TO_TCHAR(Look.Key.c_str()));
}

const Fates::Fate* FShipLooks::Sink(int32 Id, const FString& Class, const FString& How, bool bHarbour, double Now)
{
	if (!Picker.IsValid())
	{
		return nullptr;
	}
	Fates::Blow Blow;
	Blow.Domain = "sea";
	Blow.Family = ShipLookParts::FamilyOf(Class);
	Blow.Weapon = "bomb";
	Blow.Result = "lost";
	Blow.How = TCHAR_TO_UTF8(*How);
	Blow.Hurt = 1.0;
	Blow.Nerve = 0.65;
	Blow.bHarbour = bHarbour;
	Blow.Unit = Id;
	// (at sea she goes under, unless her end is to burn: then her fires burn on with the end's own; given way before
	// the pick, so that her earlier looks stop even if the table has no end look for her)
	GiveWay(Id, !bHarbour && How != TEXT("burn"), Now);
	const Fates::Fate Look = Picker->Choose(Blow);
	if (Look.Key.empty())
	{
		return nullptr;
	}
	Queue(Id, Look, Now);
	UE_LOG(LogTheater, Log, TEXT("Ship looks: ship %d, %s, %s."), Id, *How, UTF8_TO_TCHAR(Look.Key.c_str()));
	return &Ends.Add(Id, Look);
}

void FShipLooks::GiveWay(int32 Id, bool bUnder, double Now)
{
	// Her end overtakes her earlier looks (the pre-check, 20 November 15:19Z: 13 of the 14 recorded hits come with her
	// end, 12 in the same instant). The burst of the hit itself still shows; what that look had still to come (a fire
	// her crew would beat, steam, a cook-off) gives way to her end. In harbour a fire already burning burns on, her
	// upperworks above the water, as it does on a ship whose end is to burn; a ship going under at sea takes her fires
	// down with her (where the kit can put them out: Burn with an id).
	for (int32 K = Due.Num() - 1; K >= 0; --K)
	{
		const FDue& Step = Due[K];
		if (Step.Ship != Id)
		{
			continue;
		}
		const bool bKept = (IsBurst(Step.Step) && Step.At <= Now + 2.0) || (!bUnder && Step.Step.Do == "burnout");
		if (!bKept)
		{
			Due.RemoveAt(K, 1, EAllowShrinking::No);
		}
	}
	if (bUnder)
	{
		if (const TArray<int32>* Lit = Burns.Find(Id))
		{
			for (const int32 Burn : *Lit)
			{
				Fates::Beat Out;
				Out.Do = "burnout";
				Out.N = Burn;
				Out.A = 20.0;
				FDue& Later = Due.AddDefaulted_GetRef();
				Later.At = Now;
				Later.Ship = Id;
				Later.Step = Out;
			}
			if (TArray<double>* Until = Fires.Find(Id))
			{
				for (double& Stops : *Until)
				{
					Stops = FMath::Min(Stops, Now + 20.0);
				}
			}
		}
		Burns.Remove(Id);
		StopLeaks(Id);
	}
}

void FShipLooks::StopLeaks(int32 Id)
{
	// (her steam stops now, where the kit has steam; what is in the air thins on its own)
	if (const TArray<int32>* Open = Leaks.Find(Id))
	{
		for (const int32 Leak : *Open)
		{
			ShipLookParts::SteamStop(Kit, Leak);
		}
	}
	Leaks.Remove(Id);
}

bool FShipLooks::IsBurst(const Fates::Beat& Step)
{
	return Step.Do == "blast" || Step.Do == "column" || Step.Do == "spark" || Step.Do == "glance";
}

void FShipLooks::Queue(int32 Id, const Fates::Fate& Look, double Now, bool bBurstsOnly)
{
	uint32 Mix = static_cast<uint32>(Fates::NameKey(Look.Key) ^ static_cast<uint64>(Id) * 2654435761ull ^ static_cast<uint64>(Due.Num() + 1) * 40503ull);
	for (const Fates::Beat& Step : Look.Beats)
	{
		if (Step.Do == "list" || Step.Do == "right" || Step.Do == "trim" || Step.Do == "settle")
		{
			continue; // (the pose: Hulls, taken at the hit)
		}
		if (bBurstsOnly && (!IsBurst(Step) || Step.T > 2.0))
		{
			continue;
		}
		FDue& D = Due.AddDefaulted_GetRef();
		D.At = Now + Step.T;
		D.Ship = Id;
		D.Step = Step;
		Mix = Mix * 1664525u + 1013904223u;
		D.Mix = Mix >> 8;
	}
}

void FShipLooks::Advance(double Now, FBattleEffects* Effects, TFunctionRef<bool(int32, FShipLie&)> LieOf)
{
	Kit = Effects;
	int32 Glances = 0; // (a salvo's glances are spread over frames: each is a burst of work for the kit)
	for (int32 K = 0; K < Due.Num();)
	{
		if (Due[K].At > Now || (Due[K].Step.Do == "glance" && Glances >= MostGlancesFrame))
		{
			++K;
			continue;
		}
		Glances += Due[K].Step.Do == "glance" ? 1 : 0;
		const FDue Step = Due[K];
		Due.RemoveAt(K, 1, EAllowShrinking::No);
		FShipLie Lie;
		if (LieOf(Step.Ship, Lie))
		{
			Play(Step, Lie, Effects, Now);
		}
	}
}

void FShipLooks::Play(const FDue& Item, const FShipLie& Lie, FBattleEffects* Effects, double Now)
{
	if (!Effects)
	{
		return;
	}
	const Fates::Beat& Step = Item.Step;
	const FString Do = UTF8_TO_TCHAR(Step.Do.c_str());
	const FString At = UTF8_TO_TCHAR(Step.At.c_str());
	const FString Kind = UTF8_TO_TCHAR(Step.Kind.c_str());
	const double U = Lie.UnitsPerMetre;
	const FRotator Turned(0.0, Lie.Yaw, 0.0);
	const FVector Along = Turned.RotateVector(FVector::ForwardVector), Abeam = Turned.RotateVector(FVector::RightVector);
	const double Side = SideOf(Item.Ship) != 0.0 ? SideOf(Item.Ship) : ((Item.Mix & 1u) ? 1.0 : -1.0);
	FVector Local = Lie.Base + Along * (ShipLookParts::AlongOf(At, Item.Mix) * Lie.Long * U) + FVector(0.0, 0.0, ShipLookParts::HeightOf(At, Lie.High) * U);
	const FVector Place = Lie.World.TransformPosition(Local);
	if (Do == TEXT("blast"))
	{
		Effects->ShellOfSize(Place, Kind.IsEmpty() ? FString(TEXT("bomb250")) : Kind, TEXT("town"), Step.A > 0.5);
	}
	else if (Do == TEXT("column"))
	{
		// beside her, on the side the look names (b), at the waterline
		const double Toward = Step.B < 0.0 ? -1.0 : Step.B > 0.0 ? 1.0 : Side;
		// (from the sea's face, not her settled waterline: the sea has not moved)
		const FVector Beside = FVector(Lie.Base.X, Lie.Base.Y, Lie.SeaZ) + Along * (ShipLookParts::AlongOf(At, Item.Mix) * Lie.Long * U) + Abeam * ((Lie.Beam * 0.5 + 3.0) * U * Toward);
		Effects->ShellOfSize(Lie.World.TransformPosition(Beside), Kind.IsEmpty() ? FString(TEXT("bomb500")) : Kind, TEXT("water"));
	}
	else if (Do == TEXT("fire"))
	{
		if (FiresOn(Item.Ship, Now) >= MostFires || FiresAll(Now) >= MostFiresAll)
		{
			return;
		}
		const float Across = static_cast<float>(FMath::Clamp(Step.A, 0.05, 0.6) * Lie.Beam);
		const float Seconds = static_cast<float>(FMath::Clamp(Step.B, 5.0, 1800.0));
		const bool bBeaten = Step.C > 0.5;
		// Each fire her looks light has an id where the kit takes one: her crew beat it out on cue, and at sea her fires
		// go out as she goes under (Settle). (a beaten fire burns a little past its time, for the crew to end it)
		const int32 Burn = 239000 + (NextBurn++ % 4000);
		// (kind "side": in her side under the deck's edge, on the side she was hit, as out of a carrier's hangar openings)
		const FVector FirePlace = Kind == TEXT("side")
			? Lie.World.TransformPosition(Lie.Base + Along * (ShipLookParts::AlongOf(At, Item.Mix) * Lie.Long * U) + Abeam * ((Lie.Beam * 0.5 + 1.0) * U * Side)
				+ FVector(0.0, 0.0, ShipLookParts::HeightOf(At, Lie.High) * 0.7 * U))
			: Place;
		if (ShipLookParts::BurnWithId(Effects, FirePlace, Across, bBeaten ? Seconds + 60.0f : Seconds, Burn))
		{
			Burns.FindOrAdd(Item.Ship).Add(Burn);
			if (bBeaten)
			{
				Fates::Beat Out;
				Out.Do = "burnout";
				Out.N = Burn;
				Out.A = 20.0 + static_cast<double>(Item.Mix % 30u);
				FDue& Later = Due.AddDefaulted_GetRef();
				Later.At = Now + Seconds;
				Later.Ship = Item.Ship;
				Later.Step = Out;
			}
		}
		else
		{
			Effects->Burn(FirePlace, Across, Seconds, -1, true);
		}
		AddFire(Item.Ship, Now + Seconds);
	}
	else if (Do == TEXT("burnout"))
	{
		ShipLookParts::BurnOut(Effects, Step.N, static_cast<float>(Step.A));
	}
	else if (Do == TEXT("steam"))
	{
		// Straight up from her funnel's escape pipes (the kit's tall "funnel" plume), else from a vent or a hole in her
		// side ("vent"), out of her side where the look says so. One leak per place on her, each its own id: the kit
		// starts a leak afresh when an id comes again.
		LeakStops.RemoveAll([Now](double Stops) { return Stops <= Now; });
		if (LeakStops.Num() >= MostLeaks)
		{
			return;
		}
		const bool bFunnel = Kind != TEXT("side") && At == TEXT("funnel");
		const FVector Way = Kind == TEXT("side") ? (Abeam * Side * 0.8 + FVector(0.0, 0.0, 0.6)).GetSafeNormal() : FVector::UpVector;
		const int32 Leak = 300000 + Item.Ship * 4 + (bFunnel ? 0 : Kind == TEXT("side") ? 1 : 2);
		if (ShipLookParts::Steam(Effects, Leak, Place, Way, static_cast<float>(FMath::Clamp(Step.A, 0.2, 4.0)), static_cast<float>(FMath::Clamp(Step.B, 2.0, 60.0)), bFunnel ? FString(TEXT("funnel")) : FString(TEXT("vent"))))
		{
			Leaks.FindOrAdd(Item.Ship).AddUnique(Leak);
			LeakStops.Add(Now + FMath::Clamp(Step.B, 2.0, 60.0));
		}
	}
	else if (Do == TEXT("cookoff"))
	{
		CookStops.RemoveAll([Now](double Stops) { return Stops <= Now; });
		if (CookStops.Num() >= MostCookOffs)
		{
			return;
		}
		CookStops.Add(Now + FMath::Clamp(Step.B, 1.0, 120.0));
		const int32 Pops = FMath::Clamp(Step.N, 1, 10);
		if (!ShipLookParts::CookOff(Effects, Place, static_cast<float>(FMath::Clamp(Step.A, 0.3, 10.0)), Pops, static_cast<float>(FMath::Clamp(Step.B, 1.0, 120.0)), static_cast<float>(FMath::Clamp(Step.C, 0.0, 10.0))))
		{
			// A deck park going up plane by plane: small bursts over the look's time, at most four (a stand-in the kit's
			// CookOff replaces; the art director sees it as such, never as the cook-off itself).
			const int32 Bursts = FMath::Min(Pops, 4);
			for (int32 K = 0; K < Bursts; ++K)
			{
				Fates::Beat Pop;
				Pop.Do = "blast";
				Pop.At = Step.At;
				Pop.Kind = "light";
				Pop.A = 1.0;
				FDue& Later = Due.AddDefaulted_GetRef();
				Later.At = Now + Step.B * (K + 0.5) / Bursts;
				Later.Ship = Item.Ship;
				Later.Step = Pop;
				Later.Mix = Item.Mix * 31u + static_cast<uint32>(K);
			}
		}
	}
	else if (Do == TEXT("spark"))
	{
		Effects->ShellOfSize(Place, TEXT("light"), TEXT("town"), true);
	}
	else if (Do == TEXT("glance"))
	{
		// (a shell off her armour; a bomb has no glance, its look's burst on her armour is the blast)
		const FVector Normal = (Abeam * Side).GetSafeNormal();
		ShipLookParts::Glance(Effects, Place, -Normal + FVector(0.0, 0.0, -0.2), Normal, Kind);
	}
	// ("smoke" and "silence" have no call on the stage: a fire's own smoke stands for the one, and the stage's ships
	// fire no guns the other could stop.)
}

TArray<FString> FShipLooks::ReelOf(const FString& Class, bool bHarbour) const
{
	TArray<FString> Reel;
	if (!Picker.IsValid())
	{
		return Reel;
	}
	// Each look a ship that survives her hit can show, with the first strike that brings it: "key|strike".
	for (const TCHAR* Strike : { TEXT("torpedo"), TEXT("dive"), TEXT("level") })
	{
		Fates::Blow Blow;
		Blow.Domain = "sea";
		Blow.Family = ShipLookParts::FamilyOf(Class);
		Blow.Weapon = ShipLookParts::WeaponOf(Strike);
		Blow.Result = "worn";
		Blow.Nerve = 0.65;
		Blow.bHarbour = bHarbour;
		for (const std::pair<std::string, double>& Odd : Picker->Odds(Blow))
		{
			const FString Key = UTF8_TO_TCHAR(Odd.first.c_str());
			if (!Reel.ContainsByPredicate([&Key](const FString& Had) { return Had.StartsWith(Key + TEXT("|")); }))
			{
				Reel.Add(Key + TEXT("|") + Strike);
			}
		}
	}
	return Reel;
}

FString FShipLooks::Reel(int32 Id, const FString& Class, const FString& KeyAndStrike, bool bHarbour, double Now)
{
	FString Key, Strike;
	if (!Picker.IsValid() || !KeyAndStrike.Split(TEXT("|"), &Key, &Strike))
	{
		return FString();
	}
	// Each look on a ship the test keeps afloat, one after another: her pose, her fires (where the kit can put them out)
	// and what was still to come cleared first, so that each picture shows one look alone.
	Due.RemoveAll([Id](const FDue& Step) { return Step.Ship == Id; });
	if (const TArray<int32>* Lit = Burns.Find(Id))
	{
		for (const int32 Burn : *Lit)
		{
			Fates::Beat Out;
			Out.Do = "burnout";
			Out.N = Burn;
			Out.A = 3.0;
			FDue& Douse = Due.AddDefaulted_GetRef();
			Douse.At = Now;
			Douse.Ship = Id;
			Douse.Step = Out;
		}
	}
	Burns.Remove(Id);
	Fires.Remove(Id);
	StopLeaks(Id);
	Hulls.Remove(Id);
	Sides.Remove(Id);
	Fates::Blow Blow;
	Blow.Domain = "sea";
	Blow.Family = ShipLookParts::FamilyOf(Class);
	Blow.Weapon = ShipLookParts::WeaponOf(Strike);
	Blow.Result = "worn";
	Blow.Nerve = 0.65;
	Blow.bHarbour = bHarbour;
	Blow.Unit = Id;
	const Fates::Fate Look = Picker->Show(Blow, std::string(TCHAR_TO_UTF8(*Key)));
	if (Look.Key.empty())
	{
		return FString();
	}
	Hulls.FindOrAdd(Id).Take(Look, Now);
	// (the reel's camera stands on the side the look acts on: a look that names no side acts on her starboard side)
	Sides.Add(Id, Look.Dials.count("side") && Look.Dial("side", 1.0) < 0.0 ? -1.0 : 1.0);
	FString Aim = TEXT("mid");
	for (const Fates::Beat& Step : Look.Beats)
	{
		if (!Step.At.empty() && (Step.Do == "blast" || Step.Do == "column" || Step.Do == "fire" || Step.Do == "glance" || Step.Do == "spark" || Step.Do == "steam" || Step.Do == "cookoff"))
		{
			Aim = UTF8_TO_TCHAR(Step.At.c_str());
			break;
		}
	}
	ReelAt.Add(Id, Aim);
	Queue(Id, Look, Now);
	UE_LOG(LogTheater, Display, TEXT("Ship looks reel: ship %d, %s, %s, at %s."), Id, *Strike, *Key, *Aim);
	return Key;
}

void FShipLooks::Cleared()
{
	Due.Reset();
	Fires.Reset();
	Burns.Reset();
	Leaks.Reset();
	LeakStops.Reset();
	CookStops.Reset();
}

bool FShipLooks::ReelAim(int32 Id, double High, double& OutAlong, double& OutUp, double& OutSide) const
{
	const FString* At = ReelAt.Find(Id);
	if (!At)
	{
		return false;
	}
	// ("deck" and "hangar" fall anywhere along her: the camera takes her middle)
	OutAlong = (*At == TEXT("deck") || *At == TEXT("hangar")) ? 0.0 : ShipLookParts::AlongOf(*At, 0u);
	OutUp = ShipLookParts::HeightOf(*At, High);
	OutSide = SideOf(Id) < 0.0 ? -1.0 : 1.0;
	return true;
}

bool FShipLooks::Pose(int32 Id, double Now, double& OutList, double& OutTrim, double& OutSettle) const
{
	OutList = OutTrim = OutSettle = 0.0;
	const Fates::Hull* Hull = Hulls.Find(Id);
	if (!Hull)
	{
		return false;
	}
	Hull->Pose(Now, OutList, OutTrim, OutSettle);
	return true;
}

double FShipLooks::SideOf(int32 Id) const
{
	const double* Side = Sides.Find(Id);
	return Side ? *Side : 0.0;
}

int32 FShipLooks::FiresOn(int32 Id, double Now) const
{
	int32 Burning = 0;
	if (const TArray<double>* Until = Fires.Find(Id))
	{
		for (const double Stops : *Until)
		{
			Burning += Stops > Now ? 1 : 0;
		}
	}
	return Burning;
}

int32 FShipLooks::FiresAll(double Now) const
{
	int32 Burning = 0;
	for (const TPair<int32, TArray<double>>& Pair : Fires)
	{
		for (const double Stops : Pair.Value)
		{
			Burning += Stops > Now ? 1 : 0;
		}
	}
	return Burning;
}

void FShipLooks::AddFire(int32 Id, double Until)
{
	Fires.FindOrAdd(Id).Add(Until);
}
