// (Release 2) A battle of your own (Brian, 24 November 2026 18:36Z): "an option on the screen just to do an air, land, sea
// battle. You pick the two sides. One you play and the other is the computer. You then pick the units you want for the
// battle or the computer can pick their own. Both sides have a certain treasury budget they have to fit in."
//
// The opening screen's fourth door. The rules (Rules/custom-bridge.ts) price both armies from the campaign's own list,
// pick the computer's army, check both, and make the battle on a war of its own holding only the two armies, so that no
// campaign is touched and the units decide it, never a nation's flag or the calendar. The battle is the 3D game's own:
// the land rules' field (ALandBattle, as FIGHT IT YOURSELF opens it) or the air battle (AAirBattle, as FLY IT YOURSELF
// does). Only battlefields whose look passed the art director and the Mac are offered (FieldLookPassed). The sea is shown
// and locked: its 3D battle is watched, not commanded, and has not been through the gates.

#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignGameMode.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignSession.h"
#include "Campaign/CampaignSounds.h"
#include "TheaterCommand.h"
#include "Battle/AirBattle.h"
#include "Battle/LandBattle.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace CustomBattle
{
	/** The rules, loaded: a battle of your own needs no war running (FCampaignSession::CallBattle's IsOwnBattle). */
	FCampaignSession* RulesOf(ACampaignGameMode* Mode)
	{
		FCampaignSession* Rules = Mode ? Mode->GetSession() : nullptr;
		return Rules && Rules->IsReady() ? Rules : nullptr;
	}

	/** The rules' level of a land battle ("Decisive victory"), read as the attacker reads it, as his side reads it. */
	FString LevelFor(const FString& Level, bool bAttacks)
	{
		if (bAttacks)
		{
			return Level;
		}
		static const TMap<FString, FString> Flip = {
			{ TEXT("Heroic victory"), TEXT("Crushing defeat") }, { TEXT("Decisive victory"), TEXT("Decisive defeat") },
			{ TEXT("Close victory"), TEXT("Close defeat") }, { TEXT("Pyrrhic victory"), TEXT("Close defeat") },
			{ TEXT("Close defeat"), TEXT("Close victory") }, { TEXT("Decisive defeat"), TEXT("Decisive victory") },
			{ TEXT("Crushing defeat"), TEXT("Heroic victory") },
		};
		const FString* Mirrored = Flip.Find(Level);
		return Mirrored ? *Mirrored : Level;
	}

	FString Written(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	TSharedPtr<FJsonObject> Read(const FString& Text)
	{
		TSharedPtr<FJsonObject> Out;
		if (Text.IsEmpty() || Text == TEXT("null"))
		{
			return nullptr;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid() ? Out : nullptr;
	}

	/** One of the rules' calls for a battle of your own; its answer read, or null. */
	TSharedPtr<FJsonObject> Ask(FCampaignSession* Rules, const TCHAR* Function, const FString& Arguments)
	{
		FString Reply, Error;
		if (!Rules || !Rules->CallBattle(Function, Arguments, Reply, Error))
		{
			UE_LOG(LogTheater, Warning, TEXT("A battle of your own: TC.%s failed: %s"), Function, *Error);
			return nullptr;
		}
		return Read(Reply);
	}

	/** Why the rules would not make the battle (TC.customWhy), in their words. */
	FString WhyNot(FCampaignSession* Rules)
	{
		FString Reply, Error;
		if (Rules && Rules->CallBattle(TEXT("customWhy"), TEXT("[]"), Reply, Error))
		{
			FString Why;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Reply);
			TSharedPtr<FJsonValue> Value;
			if (FJsonSerializer::Deserialize(Reader, Value) && Value.IsValid() && Value->TryGetString(Why) && !Why.IsEmpty())
			{
				return Why;
			}
		}
		return TEXT("This battle cannot be fought as it stands.");
	}

	const TArray<TSharedPtr<FJsonValue>>& ArrayOf(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		static const TArray<TSharedPtr<FJsonValue>> None;
		const TArray<TSharedPtr<FJsonValue>>* Out = nullptr;
		return Object.IsValid() && Object->TryGetArrayField(Field, Out) && Out ? *Out : None;
	}

	TSharedPtr<FJsonObject> ObjectOf(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return Object.IsValid() && Object->TryGetObjectField(Field, Out) && Out ? *Out : nullptr;
	}

	FString StringOf(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		FString Out;
		return Object.IsValid() && Object->TryGetStringField(Field, Out) ? Out : FString();
	}

	int32 NumberOf(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32 Default = 0)
	{
		double Out = 0.0;
		return Object.IsValid() && Object->TryGetNumberField(Field, Out) ? FMath::RoundToInt(Out) : Default;
	}

	/** The rules' month (turn 1 is September 1939) of a year and a month, as custom-bridge.ts turnOf counts it. */
	int32 TurnOf(int32 Year, int32 Month)
	{
		const int32 T = (Year - 1939) * 12 + (Month - 1) - 7;
		return T < 1 ? T + 12 : T;
	}

	/** The province of the map by its name, or INDEX_NONE. */
	int32 RegionNamed(const FCampaignMapData& Data, const FString& Name)
	{
		for (int32 Index = 0; Index < Data.Regions.Num(); ++Index)
		{
			if (Data.Regions[Index].Name.Equals(Name, ESearchCase::IgnoreCase))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** "18,800": the treasury's money, the rules' price a hundred times over, as the rules' own words give it. */
	FString Money(int32 Units)
	{
		return FText::AsNumber(static_cast<int64>(Units) * 100).ToString();
	}
}

void ACampaignPlayerController::LoadCustomOffer()
{
	FCampaignSession* Rules = CustomBattle::RulesOf(GetMode());
	Custom.Offer = CustomBattle::Ask(Rules, TEXT("customOffer"), TEXT("[]"));
	if (!Custom.Offer.IsValid())
	{
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>& Nations = CustomBattle::ArrayOf(Custom.Offer, TEXT("nations"));
	// His side the first great power, the computer's the next; each army trained as its nation's own army was.
	Custom.Nat[0] = 0;
	Custom.Nat[1] = Nations.Num() > 1 ? 1 : 0;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const TSharedPtr<FJsonObject> Nation = Nations.IsValidIndex(Custom.Nat[Side]) ? Nations[Custom.Nat[Side]]->AsObject() : nullptr;
		Custom.Train[Side] = CustomBattle::NumberOf(Nation, TEXT("train"), 1);
		Custom.Kit[Side] = CustomBattle::NumberOf(Custom.Offer, TEXT("kit"), 3);
	}
	Custom.Size = CustomBattle::StringOf(Custom.Offer, TEXT("size"));
	if (Custom.Size.IsEmpty())
	{
		Custom.Size = TEXT("battle");
	}
}

FString ACampaignPlayerController::CustomSpecJson(bool bWithTheirs) const
{
	const TArray<TSharedPtr<FJsonValue>>& Nations = CustomBattle::ArrayOf(Custom.Offer, TEXT("nations"));
	const bool bAir = Custom.Domain == TEXT("air");
	TSharedRef<FJsonObject> Spec = MakeShared<FJsonObject>();
	Spec->SetStringField(TEXT("domain"), Custom.Domain);
	Spec->SetStringField(TEXT("place"), bAir ? Custom.AirPlace : Custom.Place);
	Spec->SetStringField(TEXT("size"), Custom.Size);
	Spec->SetStringField(TEXT("role"), Custom.Role);
	Spec->SetStringField(TEXT("kind"), Custom.Kind);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject> Nation = Nations.IsValidIndex(Custom.Nat[Side]) ? Nations[Custom.Nat[Side]]->AsObject() : nullptr;
		Out->SetStringField(TEXT("nat"), CustomBattle::StringOf(Nation, TEXT("nat")));
		Out->SetNumberField(TEXT("kit"), Custom.Kit[Side]);
		Out->SetNumberField(TEXT("train"), Custom.Train[Side]);
		if (Side == 1 && Custom.bTheyPick && !bWithTheirs)
		{
			// (left to the computer: the rules pick it)
			Out->SetField(TEXT("buys"), MakeShared<FJsonValueNull>());
		}
		else
		{
			TSharedRef<FJsonObject> Buys = MakeShared<FJsonObject>();
			for (const TPair<FString, int32>& Buy : Custom.Buys[Side])
			{
				if (Buy.Value > 0)
				{
					Buys->SetNumberField(Buy.Key, Buy.Value);
				}
			}
			Out->SetObjectField(TEXT("buys"), Buys);
			Out->SetNumberField(TEXT("works"), Custom.Works[Side]);
		}
		Spec->SetObjectField(Side == 0 ? TEXT("me") : TEXT("them"), Out);
	}
	return CustomBattle::Written(Spec);
}

void ACampaignPlayerController::RefreshCustom()
{
	FCustomPage& P = Opening.Custom;
	P = FCustomPage();
	P.Last = Custom.Last;
	FCampaignSession* Rules = CustomBattle::RulesOf(GetMode());
	if (!Custom.Offer.IsValid())
	{
		LoadCustomOffer();
	}
	if (!Rules || !Custom.Offer.IsValid())
	{
		P.Problem = FText::FromString(TEXT("The rules have not offered a battle of your own yet."));
		++Opening.Revision;
		return;
	}
	P.bReady = true;
	const TSharedPtr<FJsonObject>& O = Custom.Offer;
	const bool bAir = Custom.Domain == TEXT("air");
	const bool bAttack = Custom.Role == TEXT("att");

	auto Chip = [](TArray<FCustomChip>& Into, const FString& Id, const FString& Name, const FString& Tip, bool bOn, bool bLocked = false)
	{
		FCustomChip& C = Into.AddDefaulted_GetRef();
		C.Id = Id;
		C.Name = FText::FromString(Name);
		C.Tip = FText::FromString(Tip);
		C.bOn = bOn;
		C.bLocked = bLocked;
	};

	// The kind of battle: land and air; the sea shown, locked, with why.
	Chip(P.Domains, TEXT("land"), TEXT("LAND"), TEXT("Armies on a battlefield of the real map, fought in the 3D field."), !bAir);
	Chip(P.Domains, TEXT("air"), TEXT("AIR"), TEXT("A raid or a fight for the sky, flown in the 3D air battle."), bAir);
	{
		const TSharedPtr<FJsonObject> Sea = CustomBattle::ObjectOf(O, TEXT("sea"));
		bool bOpen = false;
		if (Sea.IsValid())
		{
			Sea->TryGetBoolField(TEXT("open"), bOpen);
		}
		Chip(P.Domains, TEXT("sea"), TEXT("SEA"), CustomBattle::StringOf(Sea, TEXT("why")), false, !bOpen);
	}

	// The nations, the kit and the training of each side.
	const TArray<TSharedPtr<FJsonValue>>& Nations = CustomBattle::ArrayOf(O, TEXT("nations"));
	const TArray<TSharedPtr<FJsonValue>>& Kits = CustomBattle::ArrayOf(O, TEXT("kits"));
	const TArray<TSharedPtr<FJsonValue>>& Trainings = CustomBattle::ArrayOf(O, TEXT("training"));
	int32 Year = 0; // (the newer kit's year: the battle is fought then, in its battlefield's month)
	for (int32 Side = 0; Side < 2; ++Side)
	{
		Custom.Nat[Side] = Nations.Num() > 0 ? (Custom.Nat[Side] % Nations.Num() + Nations.Num()) % Nations.Num() : 0;
		Custom.Kit[Side] = FMath::Clamp(Custom.Kit[Side], 1, FMath::Max(1, Kits.Num()));
		Custom.Train[Side] = FMath::Clamp(Custom.Train[Side], 0, FMath::Max(0, Trainings.Num() - 1));
		FCustomSideView& V = Side == 0 ? P.Mine : P.Theirs;
		const TSharedPtr<FJsonObject> Nation = Nations.IsValidIndex(Custom.Nat[Side]) ? Nations[Custom.Nat[Side]]->AsObject() : nullptr;
		V.Nat = CustomBattle::StringOf(Nation, TEXT("nat"));
		V.Name = FText::FromString(CustomBattle::StringOf(Nation, TEXT("name")));
		const FString Hex = CustomBattle::StringOf(Nation, TEXT("color"));
		V.Color = Hex.IsEmpty() ? FLinearColor::Gray : FLinearColor(FColor::FromHex(Hex));
		const TSharedPtr<FJsonObject> Kit = Kits.IsValidIndex(Custom.Kit[Side] - 1) ? Kits[Custom.Kit[Side] - 1]->AsObject() : nullptr;
		V.Kit = FText::FromString(CustomBattle::StringOf(Kit, TEXT("name")));
		V.KitTip = FText::FromString(FString::Printf(TEXT("Tanks, guns and aircraft as the armies had them in %d. Better kit raises the odds; the flag does not."), CustomBattle::NumberOf(Kit, TEXT("year"), 1943)));
		Year = FMath::Max(Year, CustomBattle::NumberOf(Kit, TEXT("year"), 1943));
		const TSharedPtr<FJsonObject> Training = Trainings.IsValidIndex(Custom.Train[Side]) ? Trainings[Custom.Train[Side]]->AsObject() : nullptr;
		V.Training = FText::FromString(CustomBattle::StringOf(Training, TEXT("name")));
		V.TrainingTip = FText::FromString(CustomBattle::StringOf(Training, TEXT("blurb")));
		V.bComputerPicks = Side == 1 && Custom.bTheyPick;
		V.bAttacks = (Side == 0) == bAttack;
	}

	// The battlefields: on land only those whose look passed the art director and the Mac's speed check.
	{
		FString& Chosen = bAir ? Custom.AirPlace : Custom.Place;
		const TArray<TSharedPtr<FJsonValue>>& Places = CustomBattle::ArrayOf(O, bAir ? TEXT("airPlaces") : TEXT("places"));
		const FCampaignMapData* Data = Map.IsValid() ? &Map->GetData() : nullptr;
		bool bFound = false;
		FString First;
		for (const TSharedPtr<FJsonValue>& Value : Places)
		{
			const TSharedPtr<FJsonObject> Place = Value.IsValid() ? Value->AsObject() : nullptr;
			const FString Id = CustomBattle::StringOf(Place, TEXT("id"));
			if (Id.IsEmpty())
			{
				continue;
			}
			if (!bAir)
			{
				const int32 Region = Data ? CustomBattle::RegionNamed(*Data, CustomBattle::StringOf(Place, TEXT("region"))) : INDEX_NONE;
				const FString Look = Region != INDEX_NONE ? FieldLookAt(*Data, Region, CustomBattle::TurnOf(Year, CustomBattle::NumberOf(Place, TEXT("month"), 6))) : FString();
				if (Look.IsEmpty() || !FieldLookPassed(Look))
				{
					continue;
				}
			}
			if (First.IsEmpty())
			{
				First = Id;
			}
			const bool bOn = Id == Chosen;
			bFound |= bOn;
			Chip(P.Places, Id, CustomBattle::StringOf(Place, TEXT("name")), CustomBattle::StringOf(Place, TEXT("blurb")), bOn);
			if (bOn)
			{
				P.PlaceBlurb = FText::FromString(CustomBattle::StringOf(Place, TEXT("blurb")));
			}
		}
		if (!bFound && !First.IsEmpty())
		{
			Chosen = First;
			P.Places[0].bOn = true;
			P.PlaceBlurb = P.Places[0].Tip;
		}
		if (P.Places.Num() == 0)
		{
			P.Problem = FText::FromString(TEXT("No battlefield of this kind has passed the art director and the Mac's speed check yet."));
		}
	}

	// The treasury, the sides and, in the air, the kind of battle.
	for (const TSharedPtr<FJsonValue>& Value : CustomBattle::ArrayOf(O, TEXT("sizes")))
	{
		const TSharedPtr<FJsonObject> Size = Value.IsValid() ? Value->AsObject() : nullptr;
		const FString Id = CustomBattle::StringOf(Size, TEXT("id"));
		const int32 Budget = CustomBattle::NumberOf(Size, bAir ? TEXT("air") : TEXT("land"));
		Chip(P.Sizes, Id, FString::Printf(TEXT("%s  ·  %s"), *CustomBattle::StringOf(Size, TEXT("name")).ToUpper(), *CustomBattle::Money(Budget)),
			CustomBattle::StringOf(Size, TEXT("blurb")), Id == Custom.Size);
	}
	Chip(P.Roles, TEXT("att"), bAir ? TEXT("YOU RAID") : TEXT("YOU ATTACK"), bAir ? TEXT("Your aircraft fly to the target; the computer's defend it.") : TEXT("Your army attacks; the computer's holds the ground and may dig in."), bAttack);
	Chip(P.Roles, TEXT("def"), TEXT("YOU DEFEND"), bAir ? TEXT("The computer's aircraft come for the target; yours defend it.") : TEXT("The computer's army attacks; yours holds the ground and may dig in."), !bAttack);
	if (bAir)
	{
		const TSharedPtr<FJsonObject> Air = CustomBattle::ObjectOf(O, TEXT("air"));
		for (const TSharedPtr<FJsonValue>& Value : CustomBattle::ArrayOf(Air, TEXT("kinds")))
		{
			const TSharedPtr<FJsonObject> Kind = Value.IsValid() ? Value->AsObject() : nullptr;
			const FString Id = CustomBattle::StringOf(Kind, TEXT("id"));
			Chip(P.Kinds, Id, CustomBattle::StringOf(Kind, TEXT("name")).ToUpper(), CustomBattle::StringOf(Kind, TEXT("blurb")), Id == Custom.Kind);
		}
	}

	// The armies: his made by the computer the first time, so the door opens on a battle ready to fight; the computer's
	// its own pick, made again whenever the battle changes, unless he chooses it himself.
	auto Take = [this](int32 Side, const TSharedPtr<FJsonObject>& Army)
	{
		Custom.Buys[Side].Reset();
		const TSharedPtr<FJsonObject> Buys = CustomBattle::ObjectOf(Army, TEXT("buys"));
		if (Buys.IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Buy : Buys->Values)
			{
				double N = 0.0;
				if (Buy.Value.IsValid() && Buy.Value->TryGetNumber(N) && N > 0.0)
				{
					Custom.Buys[Side].Add(Buy.Key, FMath::RoundToInt(N));
				}
			}
		}
		Custom.Works[Side] = CustomBattle::NumberOf(Army, TEXT("works"));
	};
	if (!Custom.bPicked && P.Places.Num() > 0)
	{
		Take(0, CustomBattle::Ask(Rules, TEXT("customArmy"), FString::Printf(TEXT("[%s,\"me\"]"), *CustomSpecJson(false))));
		Custom.bPicked = true;
	}
	if (Custom.bTheyPick && P.Places.Num() > 0)
	{
		Take(1, CustomBattle::Ask(Rules, TEXT("customArmy"), FString::Printf(TEXT("[%s,\"them\"]"), *CustomSpecJson(false))));
	}

	// The price list: what each side has of each, and the defence's works.
	{
		const TSharedPtr<FJsonObject> List = CustomBattle::ObjectOf(O, bAir ? TEXT("air") : TEXT("land"));
		for (const TSharedPtr<FJsonValue>& Value : CustomBattle::ArrayOf(List, TEXT("buys")))
		{
			const TSharedPtr<FJsonObject> Buy = Value.IsValid() ? Value->AsObject() : nullptr;
			FCustomBuyRow& Row = P.Buys.AddDefaulted_GetRef();
			Row.Kind = CustomBattle::StringOf(Buy, TEXT("kind"));
			Row.Name = FText::FromString(CustomBattle::StringOf(Buy, TEXT("label")));
			Row.Tip = FText::FromString(CustomBattle::StringOf(Buy, TEXT("blurb")));
			Row.Cost = CustomBattle::NumberOf(Buy, TEXT("cost"));
			Row.Mine = Custom.Buys[0].FindRef(Row.Kind);
			Row.Theirs = Custom.Buys[1].FindRef(Row.Kind);
		}
		const TSharedPtr<FJsonObject> Works = CustomBattle::ObjectOf(List, bAir ? TEXT("flak") : TEXT("works"));
		if (Works.IsValid())
		{
			FCustomBuyRow& Row = P.Buys.AddDefaulted_GetRef();
			Row.Kind = TEXT("works");
			Row.Name = FText::FromString(CustomBattle::StringOf(Works, TEXT("label")));
			Row.Tip = FText::FromString(CustomBattle::StringOf(Works, TEXT("blurb")));
			Row.bDefenceOnly = true;
			Row.Mine = bAttack ? 0 : Custom.Works[0];
			Row.Theirs = bAttack ? Custom.Works[1] : 0;
			if (bAir)
			{
				// (the guns in levels, each dearer than the one before: the next level's price)
				const TArray<TSharedPtr<FJsonValue>>& Costs = CustomBattle::ArrayOf(Works, TEXT("costs"));
				Row.Most = CustomBattle::NumberOf(Works, TEXT("most"), Costs.Num());
				const int32 Have = bAttack ? Row.Theirs : Row.Mine;
				Row.Cost = Costs.IsValidIndex(Have) ? FMath::RoundToInt(Costs[Have]->AsNumber()) : 0;
			}
			else
			{
				Row.Most = 1;
				Row.Cost = CustomBattle::NumberOf(Works, TEXT("cost"));
			}
		}
	}

	// The rules' check of both armies: the treasury, the room on the ground, and why it cannot be fought yet.
	if (P.Places.Num() > 0)
	{
		const TSharedPtr<FJsonObject> Check = CustomBattle::Ask(Rules, TEXT("customCheck"), FString::Printf(TEXT("[%s]"), *CustomSpecJson(true)));
		if (!Check.IsValid())
		{
			P.Problem = FText::FromString(TEXT("The rules could not weigh this battle."));
		}
		else
		{
			bool bOk = false;
			Check->TryGetBoolField(TEXT("ok"), bOk);
			P.bCanFight = bOk;
			P.Problem = bOk ? FText::GetEmpty() : FText::FromString(CustomBattle::StringOf(Check, TEXT("why")));
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const TSharedPtr<FJsonObject> S = CustomBattle::ObjectOf(Check, Side == 0 ? TEXT("me") : TEXT("them"));
				FCustomSideView& V = Side == 0 ? P.Mine : P.Theirs;
				V.Spent = CustomBattle::NumberOf(S, TEXT("spent"));
				V.Budget = CustomBattle::NumberOf(S, TEXT("budget"));
				const int32 Count = CustomBattle::NumberOf(S, TEXT("count"));
				const int32 Room = CustomBattle::NumberOf(S, TEXT("room"));
				V.Room = FText::FromString(bAir
					? FString::Printf(TEXT("%d of the %d wings the sky has room for"), Count, Room)
					: FString::Printf(TEXT("%d of the %d corps the ground has room for"), Count, Room));
			}
		}
	}
	++Opening.Revision;
}

void ACampaignPlayerController::PressCustom(const FString& What)
{
	if (Custom.bOpen)
	{
		return; // (one battle at a time)
	}
	TArray<FString> Part;
	What.ParseIntoArray(Part, TEXT(":"), false);
	if (Part.Num() == 0)
	{
		return;
	}
	const FString& Verb = Part[0];
	const int32 Side = Part.Num() > 1 && Part[1] == TEXT("them") ? 1 : 0;
	const int32 Step = Part.Num() > 2 && Part.Last().StartsWith(TEXT("-")) ? -1 : 1;
	const TArray<TSharedPtr<FJsonValue>>& Nations = CustomBattle::ArrayOf(Custom.Offer, TEXT("nations"));
	if (Verb == TEXT("domain") && Part.Num() > 1)
	{
		if (Part[1] == TEXT("land") || Part[1] == TEXT("air"))
		{
			if (Custom.Domain != Part[1])
			{
				// (another price list: his army is made again by the computer, which he can then change)
				Custom.Domain = Part[1];
				Custom.bPicked = false;
				Custom.Works[0] = Custom.Works[1] = 0;
				Custom.Buys[1].Reset();
			}
		}
		else
		{
			return; // (the sea is locked: its chip says why)
		}
	}
	else if (Verb == TEXT("place") && Part.Num() > 1)
	{
		(Custom.Domain == TEXT("air") ? Custom.AirPlace : Custom.Place) = Part[1];
	}
	else if (Verb == TEXT("size") && Part.Num() > 1)
	{
		Custom.Size = Part[1];
	}
	else if (Verb == TEXT("role") && Part.Num() > 1)
	{
		Custom.Role = Part[1] == TEXT("def") ? TEXT("def") : TEXT("att");
		// (works are the defence's alone: they go with the side that no longer defends)
		Custom.Works[Custom.Role == TEXT("att") ? 0 : 1] = 0;
	}
	else if (Verb == TEXT("kind") && Part.Num() > 1)
	{
		Custom.Kind = Part[1] == TEXT("superiority") ? TEXT("superiority") : TEXT("bomb");
	}
	else if (Verb == TEXT("nat") && Nations.Num() > 1)
	{
		// (the next nation that is not the other side's)
		int32 N = Custom.Nat[Side];
		do
		{
			N = (N + Step + Nations.Num()) % Nations.Num();
		}
		while (N == Custom.Nat[1 - Side]);
		Custom.Nat[Side] = N;
		const TSharedPtr<FJsonObject> Nation = Nations[N]->AsObject();
		Custom.Train[Side] = CustomBattle::NumberOf(Nation, TEXT("train"), Custom.Train[Side]);
	}
	else if (Verb == TEXT("kit"))
	{
		Custom.Kit[Side] += Step;
	}
	else if (Verb == TEXT("train"))
	{
		Custom.Train[Side] += Step;
	}
	else if (Verb == TEXT("buy") && Part.Num() > 2)
	{
		if (Side == 1)
		{
			Custom.bTheyPick = false; // (he is choosing the computer's army now)
		}
		const FString Kind = Part[2];
		if (Kind == TEXT("works"))
		{
			Custom.Works[Side] = FMath::Clamp(Custom.Works[Side] + Step, 0, Custom.Domain == TEXT("air") ? 3 : 1);
		}
		else
		{
			int32& N = Custom.Buys[Side].FindOrAdd(Kind);
			N = FMath::Clamp(N + Step, 0, 20);
		}
	}
	else if (Verb == TEXT("pick"))
	{
		if (Side == 0)
		{
			Custom.bPicked = false; // (made again by the computer on the refresh)
		}
		else
		{
			Custom.bTheyPick = !Custom.bTheyPick;
		}
	}
	else if (Verb == TEXT("clear"))
	{
		Custom.Buys[Side].Reset();
		Custom.Works[Side] = 0;
		Custom.bPicked = true;
		if (Side == 1)
		{
			Custom.bTheyPick = false;
		}
	}
	else if (Verb == TEXT("fight"))
	{
		FightCustom();
		return;
	}
	CampaignSounds::Play(this, TEXT("click"), 0.35f);
	RefreshCustom();
}

void ACampaignPlayerController::FightCustom()
{
	FCampaignSession* Rules = CustomBattle::RulesOf(GetMode());
	UWorld* Here = GetWorld();
	if (!Rules || !Here || !Opening.Custom.bCanFight || TActorIterator<ALandBattle>(Here) || TActorIterator<AAirBattle>(Here))
	{
		return;
	}
	TSharedRef<FJsonObject> Spec = MakeShared<FJsonObject>();
	{
		TSharedPtr<FJsonObject> Made = CustomBattle::Read(CustomSpecJson(true));
		if (!Made.IsValid())
		{
			return;
		}
		Spec = Made.ToSharedRef();
	}
	// (a new battle each time: its own dice)
	Spec->SetNumberField(TEXT("seed"), NewSeed());
	const FString SpecText = CustomBattle::Written(Spec);
	const FString Domain = Custom.Domain;
	const TWeakObjectPtr<ACampaignPlayerController> Self(this);
	Custom.Last = FText::GetEmpty();
	CampaignSounds::Play(this, TEXT("stamp"), 0.6f);

	if (Domain == TEXT("air"))
	{
		AAirBattle* Sky = Here->SpawnActor<AAirBattle>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!Sky)
		{
			return;
		}
		Sky->HandBack = [Self](const FString& EndJson)
		{
			if (ACampaignPlayerController* Me = Self.Get())
			{
				Me->CustomHandBack(TEXT("air"), EndJson);
			}
		};
		Custom.bOpen = true;
		if (!Sky->BeginLiveCall(Rules, TEXT("customAirStart"), FString::Printf(TEXT("[%s]"), *SpecText)))
		{
			Sky->HandBack = nullptr;
			Sky->Destroy();
			Custom.bOpen = false;
			Opening.Custom.Problem = FText::FromString(CustomBattle::WhyNot(Rules));
			Opening.Custom.bCanFight = false;
			++Opening.Revision;
			return;
		}
		UE_LOG(LogTheater, Display, TEXT("A battle of your own: the air battle opened (%s)"), *SpecText);
		return;
	}

	// On land: the look made on the war of its own, the place on the earth added for the land rules' sky and ground, as
	// FIGHT IT YOURSELF adds it.
	FString LookText, Error;
	if (!Rules->CallBattle(TEXT("customLook"), FString::Printf(TEXT("[%s]"), *SpecText), LookText, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("A battle of your own: TC.customLook failed: %s"), *Error);
	}
	const TSharedPtr<FJsonObject> Look = CustomBattle::Read(LookText);
	if (!Look.IsValid() || !CustomBattle::ObjectOf(Look, TEXT("ticket")).IsValid())
	{
		Opening.Custom.Problem = FText::FromString(CustomBattle::WhyNot(Rules));
		Opening.Custom.bCanFight = false;
		++Opening.Revision;
		return;
	}
	if (Map.IsValid())
	{
		FString RegionName;
		for (const TSharedPtr<FJsonValue>& Value : CustomBattle::ArrayOf(Custom.Offer, TEXT("places")))
		{
			const TSharedPtr<FJsonObject> Place = Value.IsValid() ? Value->AsObject() : nullptr;
			if (CustomBattle::StringOf(Place, TEXT("id")) == Custom.Place)
			{
				RegionName = CustomBattle::StringOf(Place, TEXT("region"));
			}
		}
		const FCampaignMapData& Data = Map->GetData();
		const int32 Region = CustomBattle::RegionNamed(Data, RegionName);
		if (Data.Regions.IsValidIndex(Region))
		{
			const FVector2D At = Data.Regions[Region].Center;
			double Lon = Data.LongitudeAt(At.X);
			while (Lon > 180.0) Lon -= 360.0;
			while (Lon < -180.0) Lon += 360.0;
			TSharedPtr<FJsonObject> Live = CustomBattle::ObjectOf(Look, TEXT("live"));
			if (!Live.IsValid())
			{
				Live = MakeShared<FJsonObject>();
			}
			TArray<TSharedPtr<FJsonValue>> LatLon;
			LatLon.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Data.LatitudeAt(At.Y) * 100.0) / 100.0));
			LatLon.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Lon * 100.0) / 100.0));
			Live->SetArrayField(TEXT("latLon"), LatLon);
			Look->SetObjectField(TEXT("live"), Live);
			LookText = CustomBattle::Written(Look.ToSharedRef());
		}
	}
	ALandBattle* Field = Here->SpawnActor<ALandBattle>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Field)
	{
		return;
	}
	Field->HandBack = [Self](const FString& ResultJson)
	{
		if (ACampaignPlayerController* Me = Self.Get())
		{
			Me->CustomHandBack(TEXT("land"), ResultJson);
		}
	};
	Custom.bOpen = true;
	UE_LOG(LogTheater, Display, TEXT("A battle of your own: the land battle opened (%s)"), *SpecText);
	Field->LiveTicketWhenReady(LookText, Custom.Role, true);
}

void ACampaignPlayerController::CustomHandBack(const FString& Domain, const FString& ResultJson)
{
	Custom.bOpen = false;
	// How it ended, in a line on the door: the rules' own level ("Decisive victory") from his side.
	FString Line;
	const TSharedPtr<FJsonObject> Result = CustomBattle::Read(ResultJson);
	if (Domain == TEXT("air"))
	{
		const TSharedPtr<FJsonObject> After = CustomBattle::ObjectOf(Result, TEXT("after"));
		const FString Level = CustomBattle::StringOf(After, TEXT("level"));
		TArray<FString> Lines;
		for (const TSharedPtr<FJsonValue>& Value : CustomBattle::ArrayOf(After, TEXT("lines")))
		{
			FString Text;
			if (Value.IsValid() && Value->TryGetString(Text))
			{
				Lines.Add(Text);
			}
		}
		Line = Level.IsEmpty() ? FString::Join(Lines, TEXT(" ")) : FString::Printf(TEXT("%s. %s"), *Level, *FString::Join(Lines, TEXT(" ")));
	}
	else
	{
		const TSharedPtr<FJsonObject> Over = CustomBattle::ObjectOf(Result, TEXT("over"));
		const FString Winner = CustomBattle::StringOf(Over, TEXT("winner"));
		const FString Level = CustomBattle::LevelFor(CustomBattle::StringOf(Over, TEXT("level")), Custom.Role == TEXT("att"));
		if (!Level.IsEmpty())
		{
			Line = FString::Printf(TEXT("%s."), *Level);
		}
		else if (!Winner.IsEmpty())
		{
			Line = Winner == Custom.Role ? TEXT("You won the field.") : TEXT("The computer won the field.");
		}
	}
	Custom.Last = FText::FromString(Line.IsEmpty() ? TEXT("The battle was closed before its end.") : FString::Printf(TEXT("Your last battle: %s"), *Line));
	UE_LOG(LogTheater, Display, TEXT("A battle of your own: handed back (%s): %s"), *Domain, *Custom.Last.ToString());
	Opening.Tab = 3;
	RefreshCustom();
}
