#include "Campaign/CampaignSession.h"

#include "Campaign/CampaignMapData.h"
#include "Rules/RulesHost.h"
#include "TheaterCommand.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static void ReadSorties(const TArray<TSharedPtr<FJsonValue>>& List, TArray<FSortie>& Out);

namespace
{
	FString IdList(const TArray<int32>& Ids)
	{
		return FString::JoinBy(Ids, TEXT(","), [](int32 Id) { return FString::FromInt(Id); });
	}
}

FCampaignSession::FCampaignSession() = default;
FCampaignSession::~FCampaignSession() = default;

namespace
{
	/** Text as a JSON string, quotes and all, for handing to the rules as an argument. */
	FString JsonQuote(const FString& Text)
	{
		FString Out;
		Out.Reserve(Text.Len() + Text.Len() / 8 + 2);
		Out.AppendChar(TEXT('"'));
		for (const TCHAR Ch : Text)
		{
			switch (Ch)
			{
			case TEXT('"'): Out += TEXT("\\\""); break;
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('\n'): Out += TEXT("\\n"); break;
			case TEXT('\r'): Out += TEXT("\\r"); break;
			case TEXT('\t'): Out += TEXT("\\t"); break;
			default:
				if (Ch < 0x20)
				{
					Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Ch));
				}
				else
				{
					Out.AppendChar(Ch);
				}
			}
		}
		Out.AppendChar(TEXT('"'));
		return Out;
	}
}

bool FCampaignSession::Prepare(FString& OutError)
{
	bReady = false;
	bRunning = false;
	Host = MakeUnique<FRulesHost>();
	if (!Host->IsAvailable())
	{
		OutError = TEXT("This build has no engine to run the rules on");
		return false;
	}

	// The real rules carry their own data. The stand-in is handed the same three files the map is drawn from.
	const FString RulesFolder = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules"));
	const FString RealRules = FPaths::Combine(RulesFolder, TEXT("campaign-rules.js"));
	const bool bReal = IFileManager::Get().FileExists(*RealRules);
	if (!bReal)
	{
		FString Map, Nations, Setup;
		if (!FFileHelper::LoadFileToString(Map, *FCampaignMapData::ResolveDataFile(TEXT("campaign-map.json"), TEXT("src/generated")))
			|| !FFileHelper::LoadFileToString(Nations, *FCampaignMapData::ResolveDataFile(TEXT("nations.json"), TEXT("data/campaign")))
			|| !FFileHelper::LoadFileToString(Setup, *FCampaignMapData::ResolveDataFile(TEXT("setup-1939.json"), TEXT("data/campaign"))))
		{
			OutError = TEXT("Could not read the campaign data for the rules");
			return false;
		}
		const FString Data = FString::Printf(TEXT("globalThis.__data = { map: %s, nations: %s, setup: %s };"), *Map, *Nations, *Setup);
		if (!Host->Evaluate(Data, TEXT("campaign data"), OutError))
		{
			return false;
		}
	}
	if (!Host->LoadScript(bReal ? RealRules : FPaths::Combine(RulesFolder, TEXT("stand-in-rules.js")), OutError))
	{
		return false;
	}
	bReady = true;
	return true;
}

TArray<FStartGroup> FCampaignSession::Starts()
{
	TArray<FStartGroup> Out;
	FString Json, Error;
	TSharedPtr<FJsonObject> Object;
	if (!bReady || !Host->Call(TEXT("starts"), TEXT("[]"), Json, Error)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
	{
		// Rules that do not offer a choice (the stand-in): one way in.
		FStartGroup& Group = Out.AddDefaulted_GetRef();
		Group.Title = TEXT("September 1939");
		FStartSide& Side = Group.Sides.AddDefaulted_GetRef();
		Side.Nation = TEXT("GER");
		Side.Name = TEXT("Germany");
		return Out;
	}
	Years.Reset();
	const TArray<TSharedPtr<FJsonValue>>* YearValues = nullptr;
	if (Object->TryGetArrayField(TEXT("years"), YearValues))
	{
		for (const TSharedPtr<FJsonValue>& YearValue : *YearValues)
		{
			const TSharedPtr<FJsonObject> YearObject = YearValue->AsObject();
			FGrandYear& Year = Years.AddDefaulted_GetRef();
			YearObject->TryGetStringField(TEXT("year"), Year.Year);
			YearObject->TryGetStringField(TEXT("title"), Year.Title);
			YearObject->TryGetStringField(TEXT("note"), Year.Note);
			YearObject->TryGetNumberField(TEXT("from"), Year.From);
		}
	}
	Levels.Reset();
	Object->TryGetStringField(TEXT("level"), DefaultLevel);
	const TArray<TSharedPtr<FJsonValue>>* LevelValues = nullptr;
	if (Object->TryGetArrayField(TEXT("levels"), LevelValues))
	{
		for (const TSharedPtr<FJsonValue>& LevelValue : *LevelValues)
		{
			const TSharedPtr<FJsonObject> LevelObject = LevelValue->AsObject();
			FGrandLevel& Level = Levels.AddDefaulted_GetRef();
			LevelObject->TryGetStringField(TEXT("id"), Level.Id);
			LevelObject->TryGetStringField(TEXT("name"), Level.Name);
			LevelObject->TryGetStringField(TEXT("blurb"), Level.Blurb);
			LevelObject->TryGetStringArrayField(TEXT("lines"), Level.Lines);
		}
	}
	for (const TSharedPtr<FJsonValue>& GroupValue : Object->GetArrayField(TEXT("groups")))
	{
		const TSharedPtr<FJsonObject> GroupObject = GroupValue->AsObject();
		FStartGroup& Group = Out.AddDefaulted_GetRef();
		GroupObject->TryGetStringField(TEXT("title"), Group.Title);
		GroupObject->TryGetNumberField(TEXT("from"), Group.From);
		GroupObject->TryGetStringField(TEXT("view"), Group.ViewMode);
		for (const TSharedPtr<FJsonValue>& SideValue : GroupObject->GetArrayField(TEXT("sides")))
		{
			const TSharedPtr<FJsonObject> SideObject = SideValue->AsObject();
			FStartSide& Side = Group.Sides.AddDefaulted_GetRef();
			SideObject->TryGetStringField(TEXT("nat"), Side.Nation); // null to watch
			SideObject->TryGetStringField(TEXT("name"), Side.Name);
			SideObject->TryGetStringField(TEXT("blurb"), Side.Blurb);
			SideObject->TryGetStringArrayField(TEXT("aims"), Side.Aims);
		}
	}
	return Out;
}

TArray<FGrandNation> FCampaignSession::GrandNations(int32 From, int32 Seed)
{
	TArray<FGrandNation> Out;
	FString Json, Error;
	TSharedPtr<FJsonObject> Object;
	const double Began = FPlatformTime::Seconds();
	if (!bReady || !Host->Call(TEXT("grand"), FString::Printf(TEXT("[%d,%d]"), From, Seed), Json, Error)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
	{
		UE_LOG(LogTheater, Warning, TEXT("The rules could not say who can be led from month %d: %s"), From, *Error);
		return Out;
	}
	for (const TSharedPtr<FJsonValue>& Value : Object->GetArrayField(TEXT("nations")))
	{
		const TSharedPtr<FJsonObject> Entry = Value->AsObject();
		FGrandNation& Nation = Out.AddDefaulted_GetRef();
		Entry->TryGetStringField(TEXT("nat"), Nation.Nation);
		Entry->TryGetStringField(TEXT("name"), Nation.Name);
		Entry->TryGetStringField(TEXT("blurb"), Nation.Blurb);
		Entry->TryGetStringField(TEXT("standing"), Nation.Standing);
		Entry->TryGetBoolField(TEXT("major"), Nation.bMajor);
		Entry->TryGetBoolField(TEXT("small"), Nation.bSmall);
		Entry->TryGetStringArrayField(TEXT("abilities"), Nation.Abilities);
		Entry->TryGetNumberField(TEXT("corps"), Nation.Corps);
		Entry->TryGetNumberField(TEXT("regions"), Nation.Regions);
	}
	UE_LOG(LogTheater, Display, TEXT("Grand campaign from month %d: %d countries can be led (worked out in %.0f ms)"), From, Out.Num(), (FPlatformTime::Seconds() - Began) * 1000.0);
	return Out;
}

bool FCampaignSession::Begin(const FString& Human, int32 From, const FString& ViewMode, int32 Seed, FString& OutError, const FString& Level)
{
	bRunning = false;
	if (!bReady && !Prepare(OutError))
	{
		return false;
	}
	const FString Arguments = FString::Printf(TEXT("[{\"human\":%s,\"from\":%d,\"view\":%s,\"seed\":%d%s}]"),
		Human.IsEmpty() ? TEXT("null") : *JsonQuote(Human), From, *JsonQuote(ViewMode), Seed,
		Level.IsEmpty() ? TEXT("") : *(TEXT(",\"level\":") + JsonQuote(Level)));
	FString ViewJson;
	if (!Host->Call(TEXT("start"), Arguments, ViewJson, OutError))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Object;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ViewJson), Object) || !Object.IsValid())
	{
		OutError = TEXT("The rules did not return a picture of the war");
		return false;
	}
	View = ParseView(Object);
	bRunning = true;
	UE_LOG(LogTheater, Display, TEXT("Campaign started on the %s rules: %s, %d corps, the player leads %s"),
		*View.Rules, *View.Date, View.Corps.Num(), View.Human.IsEmpty() ? TEXT("nobody") : *View.Human);
	OnViewChanged.Broadcast();
	return true;
}

FString FCampaignSession::Save()
{
	FString Json, Error;
	if (!bRunning || !Host->Call(TEXT("save"), TEXT("[]"), Json, Error))
	{
		return FString();
	}
	TSharedPtr<FJsonObject> Object;
	FString Text;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) && Object.IsValid())
	{
		Object->TryGetStringField(TEXT("text"), Text);
	}
	return Text;
}

FString FCampaignSession::DescribeSave(const FString& SaveText)
{
	FString Json, Error;
	TSharedPtr<FJsonObject> Object;
	if (!bReady || SaveText.IsEmpty() || !Host->Call(TEXT("peek"), FString::Printf(TEXT("[%s]"), *JsonQuote(SaveText)), Json, Error)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
	{
		return FString();
	}
	FString Name, Date;
	bool bFreePlay = false;
	Object->TryGetStringField(TEXT("name"), Name);
	Object->TryGetStringField(TEXT("date"), Date);
	Object->TryGetBoolField(TEXT("open"), bFreePlay);
	const FString Who = Name.IsEmpty() ? FString(bFreePlay ? TEXT("Watching a free-play war") : TEXT("Watching history")) : Name;
	return FString::Printf(TEXT("%s, %s%s"), *Who, *Date, !Name.IsEmpty() && bFreePlay ? TEXT(", free play") : TEXT(""));
}

bool FCampaignSession::Load(const FString& SaveText)
{
	FString Json, Error;
	TSharedPtr<FJsonObject> Object;
	if (!bReady || SaveText.IsEmpty() || !Host->Call(TEXT("load"), FString::Printf(TEXT("[%s]"), *JsonQuote(SaveText)), Json, Error)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
	{
		if (!Error.IsEmpty())
		{
			UE_LOG(LogTheater, Error, TEXT("Rules: %s"), *Error);
		}
		return false;
	}
	View = ParseView(Object);
	bRunning = true;
	UE_LOG(LogTheater, Display, TEXT("Campaign continued: %s, the player leads %s"), *View.Date, View.Human.IsEmpty() ? TEXT("nobody") : *View.Human);
	OnViewChanged.Broadcast();
	return true;
}

bool FCampaignSession::CallRules(const FString& Function, const FString& ArgumentsJson, TSharedPtr<FJsonObject>& OutObject)
{
	if (!bRunning)
	{
		return false;
	}
	FString ResultJson, Error;
	if (!Host->Call(Function, ArgumentsJson, ResultJson, Error))
	{
		UE_LOG(LogTheater, Error, TEXT("Rules: %s"), *Error);
		return false;
	}
	return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResultJson), OutObject) && OutObject.IsValid();
}

namespace
{
	FOrderLine ReadOrder(const TSharedPtr<FJsonObject>& Object)
	{
		FOrderLine Line;
		Object->TryGetNumberField(TEXT("id"), Line.Id);
		Object->TryGetStringField(TEXT("name"), Line.Name);
		Object->TryGetStringField(TEXT("where"), Line.Where);
		Object->TryGetStringField(TEXT("due"), Line.Due);
		Object->TryGetNumberField(TEXT("months"), Line.Months);
		Object->TryGetNumberField(TEXT("cost"), Line.Cost);
		Object->TryGetBoolField(TEXT("canCancel"), Line.bCanCancel);
		return Line;
	}

	TArray<FOrderLine> ReadOrders(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		TArray<FOrderLine> Out;
		const TArray<TSharedPtr<FJsonValue>>* Orders = nullptr;
		if (Object->TryGetArrayField(Field, Orders))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Orders)
			{
				Out.Add(ReadOrder(Value->AsObject()));
			}
		}
		return Out;
	}

	/** Honours as the rules list them: a title and the two colours of a ribbon. */
	TArray<FHonourRibbon> ReadRibbons(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		TArray<FHonourRibbon> Out;
		const TArray<TSharedPtr<FJsonValue>>* Honours = nullptr;
		if (Object->TryGetArrayField(Field, Honours))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Honours)
			{
				const TSharedPtr<FJsonObject> Honour = Value->AsObject();
				FHonourRibbon Ribbon;
				Honour->TryGetStringField(TEXT("title"), Ribbon.Title);
				TArray<FString> Colours;
				if (Honour->TryGetStringArrayField(TEXT("ribbon"), Colours) && Colours.Num() >= 2)
				{
					Ribbon.First = FColor::FromHex(Colours[0]);
					Ribbon.Second = FColor::FromHex(Colours[1]);
				}
				Out.Add(MoveTemp(Ribbon));
			}
		}
		return Out;
	}
}

FCampaignView FCampaignSession::ParseView(const TSharedPtr<FJsonObject>& Object)
{
	FCampaignView Out;
	Object->TryGetStringField(TEXT("rules"), Out.Rules);
	Out.Turn = Object->GetIntegerField(TEXT("turn"));
	Object->TryGetStringField(TEXT("date"), Out.Date);
	Object->TryGetStringField(TEXT("human"), Out.Human); // null once the player's nation has fallen
	Object->TryGetStringField(TEXT("current"), Out.Current);
	Object->TryGetBoolField(TEXT("humanUp"), Out.bHumanUp);
	Object->TryGetBoolField(TEXT("eve"), Out.bEve);
	Object->TryGetBoolField(TEXT("open"), Out.bOpen);
	Object->TryGetStringField(TEXT("level"), Out.Level);
	Object->TryGetStringField(TEXT("fallen"), Out.Fallen); // null unless the player's nation has been beaten
	Object->TryGetStringArrayField(TEXT("declare"), Out.Declarable);
	Object->TryGetStringArrayField(TEXT("league"), Out.League);
	Object->TryGetNumberField(TEXT("idle"), Out.IdleTeams);
	Object->TryGetStringArrayField(TEXT("alerts"), Out.Alerts);
	const TArray<TSharedPtr<FJsonValue>>* Pages = nullptr;
	if (Object->TryGetArrayField(TEXT("pages"), Pages))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Pages)
		{
			FString Id, Label;
			Value->AsObject()->TryGetStringField(TEXT("id"), Id);
			Value->AsObject()->TryGetStringField(TEXT("label"), Label);
			Out.PageIds.Add(Id);
			Out.PageLabels.Add(Label);
		}
	}
	const TSharedPtr<FJsonObject>* Stores = nullptr;
	if (Object->TryGetObjectField(TEXT("stores"), Stores)) // null for a small country and in a scenario
	{
		Out.bStores = true;
		for (const TPair<const TCHAR*, FViewStock*>& Good : { TPair<const TCHAR*, FViewStock*>(TEXT("oil"), &Out.Oil), TPair<const TCHAR*, FViewStock*>(TEXT("sup"), &Out.Supplies) })
		{
			const TSharedPtr<FJsonObject>* Book = nullptr;
			if ((*Stores)->TryGetObjectField(Good.Key, Book))
			{
				double Stock = 0.0, Cap = 0.0, Net = 0.0;
				(*Book)->TryGetNumberField(TEXT("stock"), Stock);
				(*Book)->TryGetNumberField(TEXT("cap"), Cap);
				(*Book)->TryGetNumberField(TEXT("net"), Net);
				Good.Value->Stock = static_cast<float>(Stock);
				Good.Value->Cap = static_cast<float>(Cap);
				Good.Value->Net = static_cast<float>(Net);
				(*Book)->TryGetNumberField(TEXT("months"), Good.Value->Months);
				(*Book)->TryGetBoolField(TEXT("short"), Good.Value->bShort);
				(*Book)->TryGetStringField(TEXT("tip"), Good.Value->Tip);
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Lanes = nullptr;
	if (Object->TryGetArrayField(TEXT("lanes"), Lanes))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Lanes)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FViewLane& Lane = Out.Lanes.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("state"), Lane.State);
			double Flow = 1.0;
			Entry->TryGetNumberField(TEXT("flow"), Flow);
			Lane.Flow = static_cast<float>(Flow);
			const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
			if (Entry->TryGetArrayField(TEXT("path"), Points))
			{
				for (int32 K = 0; K + 1 < Points->Num(); K += 2)
				{
					Lane.Path.Add(FVector2D((*Points)[K]->AsNumber(), (*Points)[K + 1]->AsNumber()));
				}
			}
		}
	}
	const TSharedPtr<FJsonObject>* Designs = nullptr;
	if (Object->TryGetObjectField(TEXT("designs"), Designs))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*Designs)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Years = nullptr;
			if (Entry.Value.IsValid() && Entry.Value->TryGetArray(Years) && Years->Num() >= 3)
			{
				Out.DesignYears.Add(Entry.Key, FIntVector(static_cast<int32>((*Years)[0]->AsNumber()), static_cast<int32>((*Years)[1]->AsNumber()), static_cast<int32>((*Years)[2]->AsNumber())));
			}
		}
	}
	for (const TPair<const TCHAR*, TArray<int32>*>& List : { TPair<const TCHAR*, TArray<int32>*>(TEXT("down"), &Out.BridgesDown), TPair<const TCHAR*, TArray<int32>*>(TEXT("cut"), &Out.RailsCut) })
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object->TryGetArrayField(List.Key, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				List.Value->Add(static_cast<int32>(Value->AsNumber()));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Fleets = nullptr;
	if (Object->TryGetArrayField(TEXT("fleets"), Fleets))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Fleets)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FViewFleet& Fleet = Out.Fleets.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("id"), Fleet.Id);
			Entry->TryGetStringField(TEXT("nat"), Fleet.Nation);
			Entry->TryGetStringField(TEXT("name"), Fleet.Name);
			Entry->TryGetNumberField(TEXT("zone"), Fleet.Zone);
			Entry->TryGetNumberField(TEXT("base"), Fleet.Base);
			Entry->TryGetStringField(TEXT("port"), Fleet.Port);
			Entry->TryGetStringField(TEXT("stance"), Fleet.Stance);
			Entry->TryGetNumberField(TEXT("bb"), Fleet.Battleships);
			Entry->TryGetNumberField(TEXT("cv"), Fleet.Carriers);
			Entry->TryGetNumberField(TEXT("ca"), Fleet.Cruisers);
			Entry->TryGetNumberField(TEXT("dd"), Fleet.Destroyers);
			Entry->TryGetNumberField(TEXT("ss"), Fleet.Submarines);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* AirFleets = nullptr;
	if (Object->TryGetArrayField(TEXT("air"), AirFleets))
	{
		for (const TSharedPtr<FJsonValue>& Value : *AirFleets)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FViewAirFleet& Fleet = Out.AirFleets.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("id"), Fleet.Id);
			Entry->TryGetStringField(TEXT("nat"), Fleet.Nation);
			Entry->TryGetStringField(TEXT("name"), Fleet.Name);
			Entry->TryGetNumberField(TEXT("base"), Fleet.Base);
			Entry->TryGetNumberField(TEXT("ftr"), Fleet.Fighters);
			Entry->TryGetNumberField(TEXT("bmr"), Fleet.Bombers);
			Entry->TryGetStringField(TEXT("mission"), Fleet.Mission);
		}
	}
	// The airfields (the grand campaign). There the aircraft belong to their airfield, so the table shows one
	// piece for each airfield with aircraft on it instead of one for each group.
	const TArray<TSharedPtr<FJsonValue>>* Airfields = nullptr;
	if (Object->TryGetArrayField(TEXT("airfields"), Airfields))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Airfields)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FViewAirfield& Field = Out.Airfields.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("region"), Field.Region);
			Entry->TryGetStringField(TEXT("nat"), Field.Nation);
			Entry->TryGetStringField(TEXT("side"), Field.Side);
			Entry->TryGetStringField(TEXT("word"), Field.Word);
			Entry->TryGetNumberField(TEXT("size"), Field.Size);
			Entry->TryGetNumberField(TEXT("holds"), Field.Holds);
			Entry->TryGetNumberField(TEXT("groups"), Field.Groups);
			Entry->TryGetNumberField(TEXT("aircraft"), Field.Aircraft);
			Entry->TryGetBoolField(TEXT("crowded"), Field.bCrowded);
		}
	}
	if (Out.Airfields.Num() > 0)
	{
		TArray<FViewAirFleet> Stood;
		for (const FViewAirFleet& Group : Out.AirFleets)
		{
			FViewAirFleet* Same = Stood.FindByPredicate([&Group](const FViewAirFleet& Other) { return Other.Base == Group.Base && Other.Nation == Group.Nation; });
			if (Same)
			{
				Same->Fighters += Group.Fighters;
				Same->Bombers += Group.Bombers;
				++Same->Groups;
			}
			else
			{
				Stood.Add(Group);
			}
		}
		Out.AirFleets = MoveTemp(Stood);
	}

	const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
	if (Object->TryGetArrayField(TEXT("regions"), Regions))
	{
		Out.RegionHolder.SetNum(Regions->Num());
		Out.RegionStanding.SetNum(Regions->Num());
		Out.RegionWorks.Init(FIntVector4(0, 0, 0, 0), Regions->Num());
		for (const TSharedPtr<FJsonValue>& Value : *Regions)
		{
			const TSharedPtr<FJsonObject> Region = Value->AsObject();
			const int32 Id = Region->GetIntegerField(TEXT("id"));
			if (Out.RegionHolder.IsValidIndex(Id))
			{
				Out.RegionHolder[Id] = Region->GetStringField(TEXT("ctrl"));
				Region->TryGetStringField(TEXT("stand"), Out.RegionStanding[Id]);
				const TArray<TSharedPtr<FJsonValue>>* Built = nullptr;
				if (Region->TryGetArrayField(TEXT("w"), Built) && Built->Num() >= 4)
				{
					Out.RegionWorks[Id] = FIntVector4(static_cast<int32>((*Built)[0]->AsNumber()), static_cast<int32>((*Built)[1]->AsNumber()), static_cast<int32>((*Built)[2]->AsNumber()), static_cast<int32>((*Built)[3]->AsNumber()));
				}
			}
		}
	}

	const TSharedPtr<FJsonObject>* Treasury = nullptr;
	if (Object->TryGetObjectField(TEXT("treasury"), Treasury)) // null when nobody is being led
	{
		Out.Treasury.bValid = true;
		(*Treasury)->TryGetNumberField(TEXT("pp"), Out.Treasury.Production);
		(*Treasury)->TryGetNumberField(TEXT("income"), Out.Treasury.Income);
		(*Treasury)->TryGetNumberField(TEXT("pc"), Out.Treasury.Political);
		Out.Treasury.Orders = ReadOrders(*Treasury, TEXT("orders"));
		(*Treasury)->TryGetBoolField(TEXT("budget"), Out.Treasury.bBudget);
		(*Treasury)->TryGetNumberField(TEXT("money"), Out.Treasury.Money);
		(*Treasury)->TryGetNumberField(TEXT("gross"), Out.Treasury.Gross);
		(*Treasury)->TryGetNumberField(TEXT("upkeep"), Out.Treasury.Upkeep);
		(*Treasury)->TryGetNumberField(TEXT("net"), Out.Treasury.Net);
		(*Treasury)->TryGetNumberField(TEXT("debt"), Out.Treasury.Debt);
	}

	const TArray<TSharedPtr<FJsonValue>>* Corps = nullptr;
	if (Object->TryGetArrayField(TEXT("corps"), Corps))
	{
		Out.Corps.Reserve(Corps->Num());
		for (const TSharedPtr<FJsonValue>& Value : *Corps)
		{
			const TSharedPtr<FJsonObject> Unit = Value->AsObject();
			FViewCorps C;
			C.Id = Unit->GetIntegerField(TEXT("id"));
			C.Nation = Unit->GetStringField(TEXT("nat"));
			C.Name = Unit->GetStringField(TEXT("name"));
			C.Kind = Unit->GetStringField(TEXT("kind"));
			Unit->TryGetStringField(TEXT("elite"), C.Elite);
			Unit->TryGetBoolField(TEXT("dim"), C.bDim);
			Unit->TryGetStringField(TEXT("gen"), C.General); // null when nobody is named
			Unit->TryGetStringField(TEXT("posture"), C.Posture);
			C.Divisions = Unit->GetIntegerField(TEXT("divs"));
			C.Strength = static_cast<float>(Unit->GetNumberField(TEXT("str")));
			// Rules that do not count heads (the stand-in) are taken at a full division's strength.
			if (!Unit->TryGetNumberField(TEXT("men"), C.Men))
			{
				C.Men = FMath::RoundToInt32(C.Divisions * C.Strength * 15000.0f);
			}
			C.Cohesion = static_cast<float>(Unit->GetNumberField(TEXT("org")));
			C.Region = Unit->GetIntegerField(TEXT("reg"));
			C.Moves = Unit->GetIntegerField(TEXT("ap"));
			C.MovesMax = Unit->GetIntegerField(TEXT("apMax"));
			Unit->TryGetBoolField(TEXT("canOrder"), C.bCanOrder);
			Unit->TryGetNumberField(TEXT("xp"), C.Experience);
			Unit->TryGetStringField(TEXT("grade"), C.Grade);
			Unit->TryGetStringField(TEXT("nick"), C.Nickname); // null until it has earned one
			C.Honours = ReadRibbons(Unit, TEXT("honours"));
			Out.Corps.Add(MoveTemp(C));
		}
	}
	return Out;
}

void FCampaignSession::TakeView(const TSharedPtr<FJsonObject>& Holder)
{
	const TSharedPtr<FJsonObject>* NewView = nullptr;
	if (Holder.IsValid() && Holder->TryGetObjectField(TEXT("view"), NewView))
	{
		View = ParseView(*NewView);
		OnViewChanged.Broadcast();
	}
}

namespace
{
	TArray<FCorpsMove> ReadMoves(const TSharedPtr<FJsonObject>& Object)
	{
		TArray<FCorpsMove> Out;
		const TArray<TSharedPtr<FJsonValue>>* Moved = nullptr;
		if (Object->TryGetArrayField(TEXT("moved"), Moved))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Moved)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FCorpsMove M;
				M.CorpsId = Entry->GetIntegerField(TEXT("id"));
				for (const TSharedPtr<FJsonValue>& Step : Entry->GetArrayField(TEXT("path")))
				{
					M.Path.Add(FMath::RoundToInt32(Step->AsNumber()));
				}
				Out.Add(MoveTemp(M));
			}
		}
		return Out;
	}

	TArray<FBattleUnit> ReadUnits(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		TArray<FBattleUnit> Out;
		const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
		if (Object->TryGetArrayField(Field, Units))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Units)
			{
				const TSharedPtr<FJsonObject> Unit = Value->AsObject();
				FBattleUnit U;
				Unit->TryGetStringField(TEXT("nat"), U.Nation);
				Unit->TryGetStringField(TEXT("name"), U.Name);
				Unit->TryGetStringField(TEXT("kind"), U.Kind);
				Unit->TryGetStringField(TEXT("gen"), U.General);
				Unit->TryGetStringField(TEXT("fate"), U.Fate);
				Unit->TryGetNumberField(TEXT("divs"), U.Divisions);
				Unit->TryGetNumberField(TEXT("men"), U.Men);
				Unit->TryGetNumberField(TEXT("lost"), U.Lost);
				Out.Add(MoveTemp(U));
			}
		}
		return Out;
	}
}

TArray<FReachRegion> FCampaignSession::Reach(const TArray<int32>& CorpsIds)
{
	TArray<FReachRegion> Out;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("reach"), FString::Printf(TEXT("[[%s]]"), *IdList(CorpsIds)), Object))
	{
		return Out;
	}
	const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
	if (Object->TryGetArrayField(TEXT("regions"), Regions))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Regions)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FReachRegion R;
			R.Region = Entry->GetIntegerField(TEXT("id"));
			R.Cost = Entry->GetIntegerField(TEXT("cost"));
			R.Previous = Entry->GetIntegerField(TEXT("prev"));
			Entry->TryGetBoolField(TEXT("attack"), R.bAttack);
			Entry->TryGetStringField(TEXT("war"), R.War);
			Out.Add(R);
		}
	}
	return Out;
}

TArray<FCorpsMove> FCampaignSession::Move(const TArray<int32>& CorpsIds, int32 RegionId)
{
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("move"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		return TArray<FCorpsMove>();
	}
	TArray<FCorpsMove> Out = ReadMoves(Object);
	TakeView(Object);
	return Out;
}

FAttackPreview FCampaignSession::PreviewAttack(const TArray<int32>& CorpsIds, int32 RegionId)
{
	FAttackPreview Out;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("previewAttack"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		return Out; // also what comes back when the rules answer null: there is nobody there to attack
	}
	Out.bValid = true;
	Out.Region = RegionId;
	Out.Attack = static_cast<float>(Object->GetNumberField(TEXT("attack")));
	Out.Defence = static_cast<float>(Object->GetNumberField(TEXT("defence")));
	Out.Odds = static_cast<float>(Object->GetNumberField(TEXT("odds")));
	const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
	if (Object->TryGetArrayField(TEXT("lines"), Lines))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Lines)
		{
			const TSharedPtr<FJsonObject> Line = Value->AsObject();
			FTicketLine L;
			Line->TryGetStringField(TEXT("label"), L.Label);
			Line->TryGetStringField(TEXT("value"), L.Value);
			Line->TryGetStringField(TEXT("effect"), L.Effect);
			Out.Lines.Add(MoveTemp(L));
		}
	}
	Out.Attackers = ReadUnits(Object, TEXT("attackers"));
	Out.Defenders = ReadUnits(Object, TEXT("defenders"));
	return Out;
}

FAttackResult FCampaignSession::Attack(const TArray<int32>& CorpsIds, int32 RegionId)
{
	FAttackResult Out;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("attack"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		return Out;
	}
	const TSharedPtr<FJsonObject>* Report = nullptr;
	if (Object->TryGetObjectField(TEXT("report"), Report)) // null when the rules would not let the attack go in
	{
		Out.bFought = true;
		(*Report)->TryGetStringField(TEXT("level"), Out.Level);
		(*Report)->TryGetBoolField(TEXT("taken"), Out.bTaken);
		(*Report)->TryGetBoolField(TEXT("yielded"), Out.bYielded);
		(*Report)->TryGetNumberField(TEXT("attLost"), Out.AttackerLost);
		(*Report)->TryGetNumberField(TEXT("defLost"), Out.DefenderLost);
		(*Report)->TryGetNumberField(TEXT("prisoners"), Out.Prisoners);
		Out.Attackers = ReadUnits(*Report, TEXT("attackers"));
		Out.Defenders = ReadUnits(*Report, TEXT("defenders"));
	}
	Object->TryGetStringField(TEXT("note"), Out.Note);
	Out.Moved = ReadMoves(Object);
	TakeView(Object);
	return Out;
}

FMonthResult FCampaignSession::EndMonth()
{
	FMonthResult Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("endMonth"), TEXT("[]"), Object))
	{
		return Out;
	}
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (Object->TryGetArrayField(TEXT("steps"), Steps))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Steps)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FMonthStep Step;
			Entry->TryGetStringField(TEXT("nat"), Step.Nation);
			Entry->TryGetStringField(TEXT("note"), Step.Note);
			Entry->TryGetBoolField(TEXT("mine"), Step.bMine);
			Entry->TryGetBoolField(TEXT("moved"), Step.bMarchOnly);
			const TArray<TSharedPtr<FJsonValue>>* Raids = nullptr;
			if (Entry->TryGetArrayField(TEXT("raids"), Raids))
			{
				ReadSorties(*Raids, Step.Raids);
			}
			const TArray<TSharedPtr<FJsonValue>>* Battles = nullptr;
			if (Entry->TryGetArrayField(TEXT("battles"), Battles))
			{
				for (const TSharedPtr<FJsonValue>& Region : *Battles)
				{
					Step.Battles.AddUnique(FMath::RoundToInt32(Region->AsNumber()));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Focus = nullptr;
			if (Entry->TryGetArrayField(TEXT("focus"), Focus))
			{
				for (const TSharedPtr<FJsonValue>& Region : *Focus)
				{
					Step.Focus.Add(FMath::RoundToInt32(Region->AsNumber()));
				}
			}
			const TSharedPtr<FJsonObject>* StepView = nullptr;
			if (Entry->TryGetObjectField(TEXT("view"), StepView))
			{
				Step.View = ParseView(*StepView);
			}
			Out.Steps.Add(MoveTemp(Step));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	if (Object->TryGetArrayField(TEXT("events"), Events))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Events)
		{
			FString Text;
			if (Value->AsObject()->TryGetStringField(TEXT("text"), Text))
			{
				Out.Headlines.Add(Text);
			}
		}
	}
	Object->TryGetStringField(TEXT("fallen"), Out.Fallen);
	const TArray<TSharedPtr<FJsonValue>>* Announce = nullptr;
	if (Object->TryGetArrayField(TEXT("announce"), Announce))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Announce)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FAnnouncement& Told = Out.Announcements.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Told.Tag);
			Entry->TryGetStringField(TEXT("title"), Told.Title);
			Entry->TryGetStringField(TEXT("text"), Told.Text);
			Entry->TryGetBoolField(TEXT("good"), Told.bGood);
		}
	}
	TakeView(Object);
	return Out;
}

FDocumentPage FCampaignSession::Record(int32 CorpsId)
{
	FDocumentPage Page;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("record"), FString::Printf(TEXT("[%d]"), CorpsId), Object))
	{
		return Page; // also what the stand-in rules come to: they keep no records
	}
	FString Name, Nick, What, Where, Raised, Grade;
	int32 Experience = 0;
	Object->TryGetStringField(TEXT("name"), Name);
	Object->TryGetStringField(TEXT("nick"), Nick);
	Object->TryGetStringField(TEXT("what"), What);
	Object->TryGetStringField(TEXT("where"), Where);
	Object->TryGetStringField(TEXT("raised"), Raised);
	Object->TryGetStringField(TEXT("grade"), Grade);
	Object->TryGetNumberField(TEXT("xp"), Experience);

	Page.bValid = true;
	Page.Title = Nick.IsEmpty() ? Name : FString::Printf(TEXT("%s, \u201C%s\u201D"), *Name, *Nick);
	Page.Subtitle = FString::Printf(TEXT("%s\n%s\n%s\n%s, experience %d of 100"), *What, *Where, *Raised, *Grade, Experience);
	Page.Ribbons = ReadRibbons(Object, TEXT("honours"));

	TArray<FString> Notes;
	if (Object->TryGetStringArrayField(TEXT("notes"), Notes) && Notes.Num() > 0)
	{
		Page.Sections.Add({ TEXT("Ties"), FString::Join(Notes, TEXT("\n\n")) });
	}

	TArray<FString> Honours;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Object->TryGetArrayField(TEXT("honours"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Honour = Value->AsObject();
			FString Title, Kind, Citation, Date;
			Honour->TryGetStringField(TEXT("title"), Title);
			Honour->TryGetStringField(TEXT("name"), Kind);
			Honour->TryGetStringField(TEXT("citation"), Citation);
			Honour->TryGetStringField(TEXT("date"), Date);
			Honours.Add(FString::Printf(TEXT("%s  \u00B7  %s, %s\n%s"), *Title, *Kind, *Date, *Citation));
		}
	}
	Page.Sections.Add({ TEXT("Honours"), Honours.Num() > 0 ? FString::Join(Honours, TEXT("\n\n")) : FString(TEXT("None yet.")) });

	TArray<FString> Diary;
	if (Object->TryGetArrayField(TEXT("diary"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Line = Value->AsObject();
			FString Date, Text;
			Line->TryGetStringField(TEXT("date"), Date);
			Line->TryGetStringField(TEXT("text"), Text);
			Diary.Add(FString::Printf(TEXT("%s:  %s"), *Date, *Text));
		}
	}
	Page.Sections.Add({ TEXT("Diary, newest first"), Diary.Num() > 0 ? FString::Join(Diary, TEXT("\n")) : FString(TEXT("Nothing written yet.")) });
	return Page;
}

FDocumentPage FCampaignSession::Roll()
{
	FDocumentPage Page;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("roll"), TEXT("[]"), Object))
	{
		return Page;
	}
	Page.bValid = true;
	Page.Title = TEXT("The roll of honour");

	TArray<FString> Awards;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Object->TryGetArrayField(TEXT("awards"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Award = Value->AsObject();
			FString Title, Unit, Citation, Date;
			Award->TryGetStringField(TEXT("title"), Title);
			Award->TryGetStringField(TEXT("unit"), Unit);
			Award->TryGetStringField(TEXT("citation"), Citation);
			Award->TryGetStringField(TEXT("date"), Date);
			Awards.Add(FString::Printf(TEXT("%s  \u00B7  %s, %s\n%s"), *Unit, *Title, *Date, *Citation));
		}
	}
	Page.Subtitle = FString::Printf(TEXT("%d honour%s won so far. The newest come first."), Awards.Num(), Awards.Num() == 1 ? TEXT("") : TEXT("s"));
	Page.Sections.Add({ TEXT("Honours"), Awards.Num() > 0 ? FString::Join(Awards, TEXT("\n\n")) : FString(TEXT("No unit has been honoured yet. Take a capital, break a line, hold out under siege.")) });

	TArray<FString> Fallen;
	if (Object->TryGetArrayField(TEXT("fallen"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Unit = Value->AsObject();
			FString Name, Date, Where, How, By, General;
			Unit->TryGetStringField(TEXT("name"), Name);
			Unit->TryGetStringField(TEXT("date"), Date);
			Unit->TryGetStringField(TEXT("where"), Where);
			Unit->TryGetStringField(TEXT("how"), How);
			Unit->TryGetStringField(TEXT("by"), By);
			Unit->TryGetStringField(TEXT("general"), General);
			FString Line = FString::Printf(TEXT("%s  \u00B7  %s at %s, %s, by %s forces"), *Name, *How, *Where, *Date, *By);
			if (!General.IsEmpty())
			{
				Line += FString::Printf(TEXT("  \u00B7  %s was with it at the end"), *General);
			}
			TArray<FString> Carried;
			if (Unit->TryGetStringArrayField(TEXT("honours"), Carried) && Carried.Num() > 0)
			{
				Line += FString::Printf(TEXT("\nIt carried: %s"), *FString::Join(Carried, TEXT(", ")));
			}
			Fallen.Add(Line);
		}
	}
	Page.Sections.Add({ TEXT("The memorial wall"), Fallen.Num() > 0 ? FString::Join(Fallen, TEXT("\n\n")) : FString(TEXT("No corps has been lost.")) });
	return Page;
}

void FCampaignSession::SetStance(const TArray<int32>& CorpsIds, const FString& Posture)
{
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("stance"), FString::Printf(TEXT("[[%s],\"%s\"]"), *IdList(CorpsIds), *Posture), Object))
	{
		View = ParseView(Object); // this call answers with the picture itself
		OnViewChanged.Broadcast();
	}
}

TArray<int32> FCampaignSession::RailReach(const TArray<int32>& CorpsIds)
{
	TArray<int32> Out;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("railReach"), FString::Printf(TEXT("[[%s]]"), *IdList(CorpsIds)), Object))
	{
		const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
		if (Object->TryGetArrayField(TEXT("regions"), Regions))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Regions)
			{
				Out.Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
	}
	return Out;
}

TArray<FCorpsMove> FCampaignSession::Rail(const TArray<int32>& CorpsIds, int32 RegionId)
{
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("rail"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		return TArray<FCorpsMove>();
	}
	TArray<FCorpsMove> Out = ReadMoves(Object);
	TakeView(Object);
	return Out;
}

void FCampaignSession::SeaReach(const TArray<int32>& CorpsIds, TArray<int32>& OutPorts, TArray<int32>& OutBeaches)
{
	OutPorts.Reset();
	OutBeaches.Reset();
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("seaReach"), FString::Printf(TEXT("[[%s]]"), *IdList(CorpsIds)), Object))
	{
		return;
	}
	for (const TPair<const TCHAR*, TArray<int32>*>& List : { TPair<const TCHAR*, TArray<int32>*>(TEXT("ports"), &OutPorts), TPair<const TCHAR*, TArray<int32>*>(TEXT("beaches"), &OutBeaches) })
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object->TryGetArrayField(List.Key, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				List.Value->Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
	}
}

TArray<FCorpsMove> FCampaignSession::Ship(const TArray<int32>& CorpsIds, int32 RegionId)
{
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("ship"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		return TArray<FCorpsMove>();
	}
	TArray<FCorpsMove> Out = ReadMoves(Object);
	TakeView(Object);
	return Out;
}

FWarPlan FCampaignSession::PlanWar(const FString& Nation, int32 RegionId)
{
	FWarPlan Plan;
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("warPlan"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Nation), RegionId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), Plan.bOk);
		Object->TryGetStringField(TEXT("name"), Plan.Name);
		Object->TryGetStringField(TEXT("lead"), Plan.Lead);
		Object->TryGetStringField(TEXT("friends"), Plan.Friends);
		Object->TryGetStringArrayField(TEXT("lines"), Plan.Lines);
	}
	return Plan;
}

FLandingPlan FCampaignSession::PlanLanding(const TArray<int32>& CorpsIds, int32 RegionId)
{
	FLandingPlan Plan;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("landingPlan"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), Plan.bOk);
		Object->TryGetStringField(TEXT("why"), Plan.Why);
		Object->TryGetStringField(TEXT("name"), Plan.Name);
		Object->TryGetStringArrayField(TEXT("lines"), Plan.Lines);
	}
	return Plan;
}

FLandingResult FCampaignSession::Land(const TArray<int32>& CorpsIds, int32 RegionId)
{
	FLandingResult Result;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("landOn"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), Result.bOk);
		Object->TryGetBoolField(TEXT("taken"), Result.bTaken);
		Object->TryGetStringField(TEXT("note"), Result.Note);
		Result.Moved = ReadMoves(Object);
		TakeView(Object);
	}
	return Result;
}

FProvinceInfo FCampaignSession::Province(int32 RegionId)
{
	FProvinceInfo Info;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("province"), FString::Printf(TEXT("[%d]"), RegionId), Object))
	{
		return Info; // the stand-in rules have no provinces to manage
	}
	Info.bValid = true;
	Info.Region = RegionId;
	Object->TryGetBoolField(TEXT("home"), Info.bHome);
	Object->TryGetStringField(TEXT("raiseNote"), Info.Note);

	int32 Industry = 0, Works = 0;
	bool bHarbour = false;
	Object->TryGetNumberField(TEXT("industry"), Industry);
	Object->TryGetNumberField(TEXT("works"), Works);
	Object->TryGetBoolField(TEXT("harbour"), bHarbour);
	TArray<FString> Holdings;
	Holdings.Add(Industry > 0 ? FString::Printf(TEXT("Industry %d"), Industry) : FString(TEXT("No industry")));
	Holdings.Add(Works > 0 ? FString::Printf(TEXT("Fortified, level %d"), Works) : FString(TEXT("No works")));
	Holdings.Add(bHarbour ? TEXT("Harbour") : TEXT("No harbour"));
	Info.Holdings = FString::Join(Holdings, TEXT("   \u00B7   "));
	// What kind of country it is for an army, its rivers and bridges, its railway.
	TArray<FString> Country;
	if (Object->TryGetStringArrayField(TEXT("country"), Country) && Country.Num() > 0)
	{
		Info.Holdings += TEXT("\n") + FString::Join(Country, TEXT("\n"));
	}

	// What the province gives whoever holds it each month.
	const TSharedPtr<FJsonObject>* Yield = nullptr;
	if (Object->TryGetObjectField(TEXT("yield"), Yield))
	{
		int32 Land = 0, Port = 0, Oil = 0, Factories = 0, Capital = 0, Total = 0;
		(*Yield)->TryGetNumberField(TEXT("land"), Land);
		(*Yield)->TryGetNumberField(TEXT("port"), Port);
		(*Yield)->TryGetNumberField(TEXT("oil"), Oil);
		(*Yield)->TryGetNumberField(TEXT("fac"), Factories);
		(*Yield)->TryGetNumberField(TEXT("capital"), Capital);
		(*Yield)->TryGetNumberField(TEXT("total"), Total);
		TArray<FString> Parts;
		auto Part = [&Parts](const TCHAR* What, int32 Amount)
		{
			if (Amount > 0)
			{
				Parts.Add(FString::Printf(TEXT("%s %s"), What, *FText::AsNumber(Amount).ToString()));
			}
		};
		Part(TEXT("factories"), Factories);
		Part(TEXT("land"), Land);
		Part(TEXT("harbour"), Port);
		Part(TEXT("oil"), Oil);
		Part(TEXT("seat of government"), Capital);
		Info.Yield = Total > 0
			? FString::Printf(TEXT("%s a month%s"), *FText::AsNumber(Total).ToString(), Parts.Num() > 1 ? *FString::Printf(TEXT(" (%s)"), *FString::Join(Parts, TEXT(", "))) : TEXT(""))
			: FString(TEXT("nothing"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Raise = nullptr;
	if (Object->TryGetArrayField(TEXT("raise"), Raise))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Raise)
		{
			const TSharedPtr<FJsonObject> Spec = Value->AsObject();
			FProvinceOption Option;
			int32 Cost = 0, Months = 0, Divisions = 0, Keep = 0;
			Spec->TryGetNumberField(TEXT("keep"), Keep);
			Spec->TryGetStringField(TEXT("kind"), Option.Action);
			Spec->TryGetStringField(TEXT("label"), Option.Label);
			Spec->TryGetNumberField(TEXT("cost"), Cost);
			Spec->TryGetNumberField(TEXT("months"), Months);
			Spec->TryGetNumberField(TEXT("divs"), Divisions);
			Spec->TryGetBoolField(TEXT("ok"), Option.bEnabled);
			Spec->TryGetStringField(TEXT("why"), Option.Why);
			Option.Label = FString::Printf(TEXT("Raise: %s"), *Option.Label);
			Option.Detail = FString::Printf(TEXT("%d divisions  \u00B7  costs %s  \u00B7  ready in %d months"), Divisions, *FText::AsNumber(Cost).ToString(), Months);
			if (Keep > 0)
			{
				Option.Detail += FString::Printf(TEXT("  \u00B7  then %s a month to keep"), *FText::AsNumber(Keep).ToString());
			}
			Info.Options.Add(MoveTemp(Option));
		}
	}
	const TSharedPtr<FJsonObject>* Fort = nullptr;
	if (Object->TryGetObjectField(TEXT("fort"), Fort))
	{
		FProvinceOption Option;
		int32 Level = 0, Cost = 0, Months = 0;
		(*Fort)->TryGetNumberField(TEXT("level"), Level);
		(*Fort)->TryGetNumberField(TEXT("cost"), Cost);
		(*Fort)->TryGetNumberField(TEXT("months"), Months);
		(*Fort)->TryGetBoolField(TEXT("ok"), Option.bEnabled);
		(*Fort)->TryGetStringField(TEXT("why"), Option.Why);
		Option.Action = TEXT("fort");
		Option.Label = FString::Printf(TEXT("Build: fortifications, level %d"), Level);
		Option.Detail = FString::Printf(TEXT("Costs %s  \u00B7  finished in %d months"), *FText::AsNumber(Cost).ToString(), Months);
		Info.Options.Add(MoveTemp(Option));
	}
	// What can be done to the country itself: bridges, the railway, engineers' work.
	const TArray<TSharedPtr<FJsonValue>>* Jobs = nullptr;
	if (Object->TryGetArrayField(TEXT("jobs"), Jobs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Jobs)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FProvinceOption Option;
			FString Key;
			Entry->TryGetStringField(TEXT("key"), Key);
			Option.Action = TEXT("work:") + Key;
			Entry->TryGetStringField(TEXT("label"), Option.Label);
			Entry->TryGetStringField(TEXT("detail"), Option.Detail);
			Entry->TryGetStringField(TEXT("why"), Option.Why);
			Entry->TryGetBoolField(TEXT("ok"), Option.bEnabled);
			Info.Options.Add(MoveTemp(Option));
		}
	}
	Info.Forming = ReadOrders(Object, TEXT("forming"));
	return Info;
}

bool FCampaignSession::OrderInProvince(const FString& Action, int32 RegionId, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (Action.StartsWith(TEXT("work:")))
	{
		// A work on the country itself: a bridge blown, the railway torn up, engineers set to a task.
		if (!CallRules(TEXT("work"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Action.Mid(5)), RegionId), Object))
		{
			return false;
		}
		bool bDone = false;
		Object->TryGetBoolField(TEXT("ok"), bDone);
		Object->TryGetStringField(TEXT("note"), OutNote);
		TakeView(Object);
		return bDone;
	}
	const bool bFort = Action == TEXT("fort");
	const FString Arguments = bFort ? FString::Printf(TEXT("[%d]"), RegionId) : FString::Printf(TEXT("[\"%s\",%d]"), *Action, RegionId);
	if (!CallRules(bFort ? TEXT("fortify") : TEXT("raise"), Arguments, Object))
	{
		return false;
	}
	bool bOk = false;
	FString Name, Due;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("name"), Name);
	Object->TryGetStringField(TEXT("due"), Due);
	TakeView(Object);
	OutNote = bOk ? FString::Printf(TEXT("%s ordered. Due %s."), *Name, *Due) : FString(TEXT("That cannot be ordered now."));
	return bOk;
}

FForcePanel FCampaignSession::Force(const FString& Kind, int32 Id)
{
	FForcePanel Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("force"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Kind), Id), Object))
	{
		return Out;
	}
	Out.bValid = Object->TryGetStringField(TEXT("name"), Out.Name);
	Object->TryGetStringField(TEXT("nat"), Out.Nation);
	Object->TryGetBoolField(TEXT("mine"), Out.bMine);
	Object->TryGetStringArrayField(TEXT("lines"), Out.Lines);
	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (Object->TryGetArrayField(TEXT("options"), Options))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Options)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FProvinceOption Option;
			Entry->TryGetStringField(TEXT("key"), Option.Action);
			Entry->TryGetStringField(TEXT("label"), Option.Label);
			Entry->TryGetStringField(TEXT("detail"), Option.Detail);
			Entry->TryGetStringField(TEXT("why"), Option.Why);
			Entry->TryGetBoolField(TEXT("ok"), Option.bEnabled);
			Out.Options.Add(MoveTemp(Option));
		}
	}
	return Out;
}

bool FCampaignSession::OrderForce(const FString& Key, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("forceOrder"), FString::Printf(TEXT("[%s]"), *JsonQuote(Key)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}

FLandFeatures FCampaignSession::Land()
{
	FLandFeatures Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("land"), TEXT("[]"), Object))
	{
		return Out;
	}
	auto Numbers = [](const TSharedPtr<FJsonValue>& Value)
	{
		TArray<double> List;
		for (const TSharedPtr<FJsonValue>& Entry : Value->AsArray())
		{
			List.Add(Entry->AsNumber());
		}
		return List;
	};
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object->TryGetArrayField(TEXT("rivers"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FLandRiver& River = Out.Rivers.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("name"), River.Name);
			const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
			if (Entry->TryGetArrayField(TEXT("pts"), Points))
			{
				for (const TSharedPtr<FJsonValue>& Point : *Points)
				{
					const TArray<double> XY = Numbers(Point);
					if (XY.Num() >= 2)
					{
						River.Course.Add(FVector2D(XY[0], XY[1]));
					}
				}
			}
		}
	}
	if (Object->TryGetArrayField(TEXT("crossings"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TArray<double> N = Numbers(Value);
			if (N.Num() >= 5)
			{
				FLandCrossing& Crossing = Out.Crossings.AddDefaulted_GetRef();
				Crossing.A = static_cast<int32>(N[0]);
				Crossing.B = static_cast<int32>(N[1]);
				Crossing.River = static_cast<int32>(N[2]);
				Crossing.At = FVector2D(N[3], N[4]);
			}
		}
	}
	if (Object->TryGetArrayField(TEXT("rails"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TArray<double> N = Numbers(Value);
			if (N.Num() >= 4)
			{
				FLandRail& Rail = Out.Rails.AddDefaulted_GetRef();
				Rail.A = static_cast<int32>(N[0]);
				Rail.B = static_cast<int32>(N[1]);
				Rail.At = FVector2D(N[2], N[3]);
			}
		}
	}
	if (Object->TryGetArrayField(TEXT("steep"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			Out.Steep.Add(static_cast<int32>(Value->AsNumber()));
		}
	}
	return Out;
}

FArsenal FCampaignSession::Arsenal(int32 RegionId)
{
	FArsenal Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("arsenal"), FString::Printf(TEXT("[%d]"), RegionId), Object))
	{
		return Out;
	}
	Out.bValid = true;
	Object->TryGetBoolField(TEXT("budget"), Out.bBudget);
	Object->TryGetNumberField(TEXT("money"), Out.Money);
	Object->TryGetStringField(TEXT("region"), Out.Region);
	const TArray<TSharedPtr<FJsonValue>>* Offers = nullptr;
	if (Object->TryGetArrayField(TEXT("offers"), Offers))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Offers)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FArsenalOffer& Offer = Out.Offers.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("key"), Offer.Key);
			Entry->TryGetStringField(TEXT("group"), Offer.Group);
			Entry->TryGetStringField(TEXT("label"), Offer.Label);
			Entry->TryGetStringField(TEXT("blurb"), Offer.Blurb);
			Entry->TryGetStringField(TEXT("size"), Offer.Size);
			Entry->TryGetStringField(TEXT("note"), Offer.Note);
			Entry->TryGetStringField(TEXT("why"), Offer.Why); // null when it can be ordered
			Entry->TryGetNumberField(TEXT("cost"), Offer.Cost);
			Entry->TryGetNumberField(TEXT("upkeep"), Offer.Upkeep);
			Entry->TryGetNumberField(TEXT("months"), Offer.Months);
			Entry->TryGetBoolField(TEXT("poor"), Offer.bPoor);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Orders = nullptr;
	if (Object->TryGetArrayField(TEXT("orders"), Orders))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Orders)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FLedgerLine& Line = Out.Orders.AddDefaulted_GetRef();
			FString Name, Where, Due;
			Entry->TryGetStringField(TEXT("name"), Name);
			Entry->TryGetStringField(TEXT("where"), Where);
			Entry->TryGetStringField(TEXT("due"), Due);
			Entry->TryGetNumberField(TEXT("cost"), Line.Amount);
			Line.Label = Where.IsEmpty() ? FString::Printf(TEXT("%s, due %s"), *Name, *Due) : FString::Printf(TEXT("%s, %s, due %s"), *Name, *Where, *Due);
		}
	}
	return Out;
}

bool FCampaignSession::OrderArsenal(const FString& Key, int32 RegionId, FString& OutName)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("order"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Key), RegionId), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("name"), OutName);
	TakeView(Object);
	return bOk;
}

FLedger FCampaignSession::Ledger()
{
	FLedger Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("ledger"), TEXT("[]"), Object))
	{
		return Out;
	}
	Out.bValid = true;
	Object->TryGetBoolField(TEXT("budget"), Out.bBudget);
	Object->TryGetStringField(TEXT("nation"), Out.Nation);
	Object->TryGetStringField(TEXT("date"), Out.Date);
	Object->TryGetNumberField(TEXT("money"), Out.Money);
	Object->TryGetNumberField(TEXT("gross"), Out.Gross);
	Object->TryGetNumberField(TEXT("upkeepTotal"), Out.UpkeepTotal);
	Object->TryGetNumberField(TEXT("net"), Out.Net);
	Object->TryGetNumberField(TEXT("debt"), Out.Debt);
	Object->TryGetNumberField(TEXT("debtFloor"), Out.DebtFloor);
	Object->TryGetNumberField(TEXT("onOrder"), Out.OnOrder);
	auto Lines = [&Object](const TCHAR* Field, TArray<FLedgerLine>& Into)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object->TryGetArrayField(Field, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FLedgerLine& Line = Into.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("label"), Line.Label);
				Entry->TryGetNumberField(TEXT("amount"), Line.Amount);
				Entry->TryGetNumberField(TEXT("count"), Line.Count);
			}
		}
	};
	Lines(TEXT("income"), Out.Income);
	Lines(TEXT("changes"), Out.Changes);
	Lines(TEXT("upkeep"), Out.Upkeep);
	const TArray<TSharedPtr<FJsonValue>>* Provinces = nullptr;
	if (Object->TryGetArrayField(TEXT("provinces"), Provinces))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Provinces)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FLedgerProvince& Province = Out.Provinces.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("id"), Province.Region);
			Entry->TryGetStringField(TEXT("name"), Province.Name);
			Entry->TryGetBoolField(TEXT("home"), Province.bHome);
			Entry->TryGetNumberField(TEXT("land"), Province.Land);
			Entry->TryGetNumberField(TEXT("port"), Province.Port);
			Entry->TryGetNumberField(TEXT("oil"), Province.Oil);
			Entry->TryGetNumberField(TEXT("fac"), Province.Factories);
			Entry->TryGetNumberField(TEXT("capital"), Province.Capital);
			Entry->TryGetNumberField(TEXT("total"), Province.Total);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Orders = nullptr;
	if (Object->TryGetArrayField(TEXT("orders"), Orders))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Orders)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FLedgerLine& Line = Out.Orders.AddDefaulted_GetRef();
			FString Name, Where, Due;
			Entry->TryGetStringField(TEXT("name"), Name);
			Entry->TryGetStringField(TEXT("where"), Where);
			Entry->TryGetStringField(TEXT("due"), Due);
			Entry->TryGetNumberField(TEXT("cost"), Line.Amount);
			Line.Label = FString::Printf(TEXT("%s, %s, due %s"), *Name, *Where, *Due);
		}
	}
	return Out;
}

FDiplomacy FCampaignSession::Diplomacy()
{
	FDiplomacy Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("diplomacy"), TEXT("[]"), Object) || !Object.IsValid())
	{
		return Out;
	}
	Out.bValid = Object->TryGetStringField(TEXT("nation"), Out.Nation);
	Object->TryGetBoolField(TEXT("up"), Out.bUp);
	Object->TryGetStringField(TEXT("date"), Out.Date);
	Object->TryGetStringField(TEXT("bloc"), Out.Bloc);
	Object->TryGetStringField(TEXT("wind"), Out.Wind);
	Object->TryGetStringField(TEXT("help"), Out.Help);
	Object->TryGetNumberField(TEXT("capital"), Out.Capital);
	Object->TryGetNumberField(TEXT("income"), Out.Income);
	Object->TryGetNumberField(TEXT("cap"), Out.Cap);
	Object->TryGetNumberField(TEXT("partners"), Out.Partners);
	Object->TryGetNumberField(TEXT("cost"), Out.Cost);
	const TArray<TSharedPtr<FJsonValue>>* Powers = nullptr;
	if (Object->TryGetArrayField(TEXT("powers"), Powers))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Powers)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FDiplomacyPower& Power = Out.Powers.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Power.Tag);
			Entry->TryGetStringField(TEXT("name"), Power.Name);
			Entry->TryGetStringField(TEXT("side"), Power.Side);
			Entry->TryGetStringField(TEXT("note"), Power.Note);
			Entry->TryGetNumberField(TEXT("divs"), Power.Divisions);
			double Will = 0.0;
			Entry->TryGetNumberField(TEXT("will"), Will);
			Power.Will = static_cast<float>(Will);
			Entry->TryGetBoolField(TEXT("alive"), Power.bAlive);
			Entry->TryGetBoolField(TEXT("war"), Power.bWar);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (Object->TryGetArrayField(TEXT("rows"), Rows))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Rows)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FDiplomacyRow& Row = Out.Rows.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Row.Tag);
			Entry->TryGetStringField(TEXT("name"), Row.Name);
			Entry->TryGetStringField(TEXT("label"), Row.Label);
			Entry->TryGetStringField(TEXT("word"), Row.Word);
			Entry->TryGetStringField(TEXT("why"), Row.Why);
			Entry->TryGetStringField(TEXT("friends"), Row.Friends);
			Entry->TryGetNumberField(TEXT("opinion"), Row.Opinion);
			Entry->TryGetNumberField(TEXT("divs"), Row.Divisions);
			Entry->TryGetBoolField(TEXT("court"), Row.bCourt);
			Entry->TryGetBoolField(TEXT("canCourt"), Row.bCanCourt);
			Entry->TryGetBoolField(TEXT("trades"), Row.bTrades);
			Entry->TryGetBoolField(TEXT("war"), Row.bWar);
		}
	}
	const TSharedPtr<FJsonObject>* League = nullptr;
	if (Object->TryGetObjectField(TEXT("league"), League))
	{
		(*League)->TryGetBoolField(TEXT("founded"), Out.bLeague);
		(*League)->TryGetStringField(TEXT("full"), Out.LeagueName);
		(*League)->TryGetStringField(TEXT("since"), Out.LeagueSince);
		const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
		if ((*League)->TryGetArrayField(TEXT("members"), Members))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Members)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FDiplomacyPower& Member = Out.Members.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("nat"), Member.Tag);
				Entry->TryGetStringField(TEXT("name"), Member.Name);
				Entry->TryGetStringField(TEXT("note"), Member.Note);
				Entry->TryGetNumberField(TEXT("divs"), Member.Divisions);
				Entry->TryGetBoolField(TEXT("war"), Member.bWar);
				bool bYou = false;
				Entry->TryGetBoolField(TEXT("you"), bYou);
				Member.Side = bYou ? TEXT("own") : TEXT("other");
			}
		}
	}
	const TSharedPtr<FJsonObject>* Path = nullptr;
	if (Object->TryGetObjectField(TEXT("path"), Path))
	{
		(*Path)->TryGetStringField(TEXT("lead"), Out.PathLead);
		(*Path)->TryGetNumberField(TEXT("inviteCost"), Out.InviteCost);
		const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
		if ((*Path)->TryGetArrayField(TEXT("options"), Options))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Options)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FPathOption& Option = Out.Paths.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("id"), Option.Id);
				Entry->TryGetStringField(TEXT("title"), Option.Title);
				Entry->TryGetStringField(TEXT("text"), Option.Text);
				Entry->TryGetStringField(TEXT("why"), Option.Why);
				Entry->TryGetBoolField(TEXT("ok"), Option.bOk);
				Entry->TryGetBoolField(TEXT("now"), Option.bNow);
				Entry->TryGetBoolField(TEXT("final"), Option.bFinal);
			}
		}
		for (const TPair<const TCHAR*, TArray<FLeagueAnswer>*>& List : { TPair<const TCHAR*, TArray<FLeagueAnswer>*>(TEXT("partners"), &Out.Founders), TPair<const TCHAR*, TArray<FLeagueAnswer>*>(TEXT("invites"), &Out.Invites) })
		{
			const TArray<TSharedPtr<FJsonValue>>* Answers = nullptr;
			if ((*Path)->TryGetArrayField(List.Key, Answers))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Answers)
				{
					const TSharedPtr<FJsonObject> Entry = Value->AsObject();
					FLeagueAnswer& Answer = List.Value->AddDefaulted_GetRef();
					Entry->TryGetStringField(TEXT("nat"), Answer.Tag);
					Entry->TryGetStringField(TEXT("name"), Answer.Name);
					Entry->TryGetStringField(TEXT("why"), Answer.Why);
					Entry->TryGetBoolField(TEXT("ok"), Answer.bOk);
					Answer.bCan = Answer.bOk;
					Entry->TryGetBoolField(TEXT("can"), Answer.bCan);
				}
			}
		}
	}
	return Out;
}

TArray<FString> FCampaignSession::Abilities(bool& bOutActive)
{
	TArray<FString> Lines;
	bOutActive = false;
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("abilities"), TEXT("[]"), Object) && Object.IsValid())
	{
		Object->TryGetBoolField(TEXT("active"), bOutActive);
		Object->TryGetStringArrayField(TEXT("lines"), Lines);
	}
	return Lines;
}

namespace
{
	void ReadButtons(const TSharedPtr<FJsonObject>& From, const TCHAR* Field, TArray<FPageButton>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Buttons = nullptr;
		if (From->TryGetArrayField(Field, Buttons))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Buttons)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FPageButton& Button = Out.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("label"), Button.Label);
				Entry->TryGetStringField(TEXT("act"), Button.Act);
				Entry->TryGetStringField(TEXT("tip"), Button.Tip);
				Entry->TryGetBoolField(TEXT("ok"), Button.bOk);
				Entry->TryGetBoolField(TEXT("danger"), Button.bDanger);
			}
		}
	}
}

FOfficePage FCampaignSession::Page(const FString& Id, int32 RegionId)
{
	FOfficePage Out;
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (!CallRules(TEXT("page"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Id), RegionId), Object) || !Object.IsValid() || !Object->TryGetBoolField(TEXT("ok"), bOk) || !bOk)
	{
		return Out;
	}
	Out.bValid = true;
	Out.Id = Id;
	Object->TryGetStringField(TEXT("title"), Out.Title);
	Object->TryGetStringField(TEXT("headline"), Out.Headline);
	Object->TryGetStringField(TEXT("note"), Out.Note);
	ReadButtons(Object, TEXT("top"), Out.Top);
	const TArray<TSharedPtr<FJsonValue>>* Sections = nullptr;
	if (Object->TryGetArrayField(TEXT("sections"), Sections))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Sections)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FPageSection& Section = Out.Sections.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("heading"), Section.Heading);
			Entry->TryGetStringField(TEXT("text"), Section.Text);
			const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
			if (Entry->TryGetArrayField(TEXT("rows"), Rows))
			{
				for (const TSharedPtr<FJsonValue>& RowValue : *Rows)
				{
					const TSharedPtr<FJsonObject> RowEntry = RowValue->AsObject();
					FPageRow& Row = Section.Rows.AddDefaulted_GetRef();
					RowEntry->TryGetStringField(TEXT("nat"), Row.Tag);
					RowEntry->TryGetStringField(TEXT("title"), Row.Title);
					RowEntry->TryGetStringArrayField(TEXT("lines"), Row.Lines);
					ReadButtons(RowEntry, TEXT("buttons"), Row.Buttons);
				}
			}
		}
	}
	return Out;
}

/** The formations of a raid, as the rules list them. */
static void ReadSorties(const TArray<TSharedPtr<FJsonValue>>& List, TArray<FSortie>& Out)
{
	for (const TSharedPtr<FJsonValue>& Value : List)
	{
		const TSharedPtr<FJsonObject> Fly = Value->AsObject();
		if (!Fly.IsValid())
		{
			continue;
		}
		FSortie& Sortie = Out.AddDefaulted_GetRef();
		Sortie.bValid = true;
		Fly->TryGetNumberField(TEXT("from"), Sortie.From);
		Fly->TryGetNumberField(TEXT("region"), Sortie.Region);
		Fly->TryGetStringField(TEXT("nat"), Sortie.Nation);
		Fly->TryGetBoolField(TEXT("bombers"), Sortie.bBombers);
		Fly->TryGetBoolField(TEXT("escort"), Sortie.bEscort);
		Fly->TryGetNumberField(TEXT("aa"), Sortie.Flak);
		Fly->TryGetStringField(TEXT("meet"), Sortie.Meet);
		Fly->TryGetNumberField(TEXT("meetFrom"), Sortie.MeetFrom);
		double Loss = 0.0, X = 0.0, Y = 0.0;
		Fly->TryGetNumberField(TEXT("loss"), Loss);
		Sortie.Loss = static_cast<float>(Loss);
		Fly->TryGetNumberField(TEXT("x"), X);
		Fly->TryGetNumberField(TEXT("y"), Y);
		Sortie.To = FVector2D(X, Y);
	}
}

bool FCampaignSession::Act(const FString& Action, int32 RegionId, FString& OutNote, TArray<FSortie>* OutSorties, FString* OutOpen)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("act"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Action), RegionId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("note"), OutNote);
		if (OutOpen)
		{
			Object->TryGetStringField(TEXT("open"), *OutOpen);
		}
		const TArray<TSharedPtr<FJsonValue>>* Flies = nullptr;
		if (OutSorties && Object->TryGetArrayField(TEXT("flies"), Flies))
		{
			ReadSorties(*Flies, *OutSorties);
		}
		TakeView(Object);
	}
	return bOk;
}

FGeneralStaff FCampaignSession::Generals()
{
	FGeneralStaff Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("generals"), TEXT("[]"), Object) || !Object.IsValid())
	{
		return Out;
	}
	Out.bValid = Object->TryGetStringField(TEXT("nation"), Out.Nation);
	Object->TryGetStringField(TEXT("date"), Out.Date);
	Object->TryGetStringField(TEXT("note"), Out.Note);
	Object->TryGetBoolField(TEXT("up"), Out.bUp);
	Object->TryGetNumberField(TEXT("capital"), Out.Capital);
	Object->TryGetNumberField(TEXT("promoteCost"), Out.PromoteCost);
	Object->TryGetNumberField(TEXT("commanding"), Out.Commanding);
	Object->TryGetNumberField(TEXT("waiting"), Out.Waiting);
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Object->TryGetArrayField(TEXT("generals"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FGeneralEntry& General = Out.Generals.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("id"), General.Id);
			Entry->TryGetStringField(TEXT("name"), General.Name);
			Entry->TryGetStringField(TEXT("rank"), General.Rank);
			Entry->TryGetStringField(TEXT("where"), General.Where);
			Entry->TryGetStringField(TEXT("record"), General.Record);
			Entry->TryGetNumberField(TEXT("level"), General.Level);
			Entry->TryGetNumberField(TEXT("points"), General.Points);
			double Progress = 0.0;
			Entry->TryGetNumberField(TEXT("progress"), Progress);
			General.Progress = static_cast<float>(Progress);
			Entry->TryGetBoolField(TEXT("commands"), General.bCommands);
			Entry->TryGetBoolField(TEXT("waiting"), General.bWaiting);
			Entry->TryGetBoolField(TEXT("relieve"), General.bRelieve);
			const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
			if (Entry->TryGetArrayField(TEXT("skills"), Skills))
			{
				for (const TSharedPtr<FJsonValue>& SkillValue : *Skills)
				{
					const TSharedPtr<FJsonObject> SkillEntry = SkillValue->AsObject();
					FGeneralSkill& Skill = General.Skills.AddDefaulted_GetRef();
					SkillEntry->TryGetStringField(TEXT("id"), Skill.Id);
					SkillEntry->TryGetStringField(TEXT("label"), Skill.Label);
					SkillEntry->TryGetStringField(TEXT("blurb"), Skill.Blurb);
					SkillEntry->TryGetNumberField(TEXT("value"), Skill.Value);
					SkillEntry->TryGetNumberField(TEXT("max"), Skill.Max);
					SkillEntry->TryGetBoolField(TEXT("more"), Skill.bMore);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Traits = nullptr;
			if (Entry->TryGetArrayField(TEXT("traits"), Traits))
			{
				for (const TSharedPtr<FJsonValue>& TraitValue : *Traits)
				{
					const TSharedPtr<FJsonObject> TraitEntry = TraitValue->AsObject();
					FString Label, Effect;
					bool bGood = true;
					TraitEntry->TryGetStringField(TEXT("label"), Label);
					TraitEntry->TryGetStringField(TEXT("effect"), Effect);
					TraitEntry->TryGetBoolField(TEXT("good"), bGood);
					General.Traits.Add(Effect.IsEmpty() ? Label : Label + TEXT(": ") + Effect);
					General.TraitGood.Add(bGood);
				}
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Vacant = nullptr;
	if (Object->TryGetArrayField(TEXT("vacant"), Vacant))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Vacant)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FVacantCorps& Corps = Out.Vacant.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("corps"), Corps.Corps);
			Entry->TryGetStringField(TEXT("name"), Corps.Name);
			Entry->TryGetStringField(TEXT("line"), Corps.Line);
			Entry->TryGetBoolField(TEXT("promote"), Corps.bPromote);
			Entry->TryGetStringField(TEXT("whyPromote"), Corps.WhyPromote);
			const TSharedPtr<FJsonObject>* Best = nullptr;
			if (Entry->TryGetObjectField(TEXT("best"), Best))
			{
				FString Rank, Name;
				int32 Level = 0;
				(*Best)->TryGetNumberField(TEXT("id"), Corps.BestId);
				(*Best)->TryGetStringField(TEXT("rank"), Rank);
				(*Best)->TryGetStringField(TEXT("name"), Name);
				(*Best)->TryGetNumberField(TEXT("level"), Level);
				Corps.BestName = FString::Printf(TEXT("%s, level %d"), *Name, Level);
			}
		}
	}
	return Out;
}

bool FCampaignSession::AssignGeneral(int32 GeneralId, int32 CorpsId)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("assign"), FString::Printf(TEXT("[%d,%d]"), GeneralId, CorpsId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::PromoteOfficer(int32 CorpsId, FString& OutName)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("promote"), FString::Printf(TEXT("[%d]"), CorpsId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("name"), OutName);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::RelieveGeneral(int32 GeneralId)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("relieve"), FString::Printf(TEXT("[%d]"), GeneralId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::SpendSkill(int32 GeneralId, const FString& Skill)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("skill"), FString::Printf(TEXT("[%d,%s]"), GeneralId, *JsonQuote(Skill)), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::StartTech(const FString& Id, FString& OutWhy)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("techStart"), FString::Printf(TEXT("[%s]"), *JsonQuote(Id)), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("why"), OutWhy);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::StopTech(const FString& Id)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("techStop"), FString::Printf(TEXT("[%s]"), *JsonQuote(Id)), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		TakeView(Object);
	}
	return bOk;
}

FResearch FCampaignSession::Research()
{
	FResearch Out;
	TSharedPtr<FJsonObject> Object;
	// The grand campaign has a technology tree; a historical scenario keeps history's clock and its design teams.
	bool bTree = false;
	if (CallRules(TEXT("tech"), TEXT("[]"), Object) && Object.IsValid() && Object->TryGetBoolField(TEXT("ok"), bTree) && bTree)
	{
		Out.bValid = Out.bTree = Out.bOwn = true;
		Object->TryGetStringField(TEXT("nation"), Out.Nation);
		Object->TryGetStringField(TEXT("date"), Out.Date);
		Object->TryGetBoolField(TEXT("up"), Out.bUp);
		Object->TryGetNumberField(TEXT("teams"), Out.Teams);
		Object->TryGetNumberField(TEXT("busy"), Out.Busy);
		Object->TryGetNumberField(TEXT("idle"), Out.Free);
		Object->TryGetNumberField(TEXT("done"), Out.Done);
		Object->TryGetNumberField(TEXT("total"), Out.Total);
		Object->TryGetStringArrayField(TEXT("eras"), Out.Eras);
		Out.Note = TEXT("Each research team works on one technology at a time, and a team with nothing to do stays idle until you give it work. A technology begun before its time takes longer. Rest the mouse on one to read what it is.");
		const TArray<TSharedPtr<FJsonValue>>* Branches = nullptr;
		if (Object->TryGetArrayField(TEXT("branches"), Branches))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Branches)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FTechBranch& Branch = Out.Branches.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("id"), Branch.Id);
				Entry->TryGetStringField(TEXT("label"), Branch.Label);
				Entry->TryGetStringField(TEXT("text"), Branch.Text);
				Entry->TryGetNumberField(TEXT("done"), Branch.Done);
				Entry->TryGetNumberField(TEXT("total"), Branch.Total);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if (Object->TryGetArrayField(TEXT("nodes"), Nodes))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Nodes)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FTechNode& Node = Out.Nodes.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("id"), Node.Id);
				Entry->TryGetStringField(TEXT("branch"), Node.Branch);
				Entry->TryGetNumberField(TEXT("tier"), Node.Tier);
				Entry->TryGetStringField(TEXT("name"), Node.Name);
				Entry->TryGetStringField(TEXT("text"), Node.Text);
				Entry->TryGetStringField(TEXT("rule"), Node.Rule);
				Entry->TryGetStringField(TEXT("state"), Node.State);
				Entry->TryGetStringField(TEXT("why"), Node.Why);
				Entry->TryGetNumberField(TEXT("months"), Node.Months);
				Entry->TryGetNumberField(TEXT("left"), Node.Left);
				Entry->TryGetNumberField(TEXT("of"), Node.Of);
				Entry->TryGetBoolField(TEXT("can"), Node.bCan);
				Entry->TryGetBoolField(TEXT("stop"), Node.bStop);
				TArray<FString> Needs;
				Entry->TryGetStringArrayField(TEXT("needs"), Needs);
				Node.Needs = FString::Join(Needs, TEXT(", "));
			}
		}
		return Out;
	}
	Object.Reset();
	if (!CallRules(TEXT("research"), TEXT("[]"), Object) || !Object.IsValid())
	{
		return Out;
	}
	Out.bValid = Object->TryGetStringField(TEXT("nation"), Out.Nation);
	Object->TryGetStringField(TEXT("date"), Out.Date);
	Object->TryGetBoolField(TEXT("up"), Out.bUp);
	Object->TryGetBoolField(TEXT("own"), Out.bOwn);
	Object->TryGetStringField(TEXT("doctrine"), Out.Doctrine);
	Object->TryGetStringField(TEXT("note"), Out.Note);
	Object->TryGetNumberField(TEXT("teams"), Out.Teams);
	Object->TryGetNumberField(TEXT("free"), Out.Free);
	Object->TryGetNumberField(TEXT("money"), Out.Money);
	const TSharedPtr<FJsonObject>* Bureau = nullptr;
	if (Object->TryGetObjectField(TEXT("bureau"), Bureau))
	{
		Out.bBureau = true;
		(*Bureau)->TryGetNumberField(TEXT("cost"), Out.BureauCost);
		(*Bureau)->TryGetBoolField(TEXT("can"), Out.bBureauCan);
		(*Bureau)->TryGetStringField(TEXT("why"), Out.BureauWhy);
	}
	const TArray<TSharedPtr<FJsonValue>>* Fields = nullptr;
	if (Object->TryGetArrayField(TEXT("fields"), Fields))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Fields)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FResearchField& Field = Out.Fields.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("id"), Field.Id);
			Entry->TryGetStringField(TEXT("label"), Field.Label);
			Entry->TryGetStringField(TEXT("pace"), Field.Pace);
			Entry->TryGetStringField(TEXT("now"), Field.Now);
			Entry->TryGetStringField(TEXT("next"), Field.Next);
			Entry->TryGetStringField(TEXT("effect"), Field.Effect);
			Entry->TryGetStringField(TEXT("work"), Field.Work);
			Entry->TryGetNumberField(TEXT("ahead"), Field.Ahead);
			Entry->TryGetNumberField(TEXT("teams"), Field.Teams);
			Entry->TryGetNumberField(TEXT("max"), Field.Max);
			Entry->TryGetBoolField(TEXT("good"), Field.bGood);
			Entry->TryGetBoolField(TEXT("more"), Field.bMore);
			Entry->TryGetBoolField(TEXT("fewer"), Field.bFewer);
			double Fill = 0.0, Today = 0.0;
			Entry->TryGetNumberField(TEXT("fill"), Fill);
			Entry->TryGetNumberField(TEXT("today"), Today);
			Field.Fill = static_cast<float>(Fill);
			Field.Today = static_cast<float>(Today);
			const TArray<TSharedPtr<FJsonValue>>* Marks = nullptr;
			if (Entry->TryGetArrayField(TEXT("marks"), Marks))
			{
				for (const TSharedPtr<FJsonValue>& MarkValue : *Marks)
				{
					const TSharedPtr<FJsonObject> MarkEntry = MarkValue->AsObject();
					FResearchMark& Mark = Field.Marks.AddDefaulted_GetRef();
					double At = 0.0;
					MarkEntry->TryGetNumberField(TEXT("at"), At);
					Mark.At = static_cast<float>(At);
					MarkEntry->TryGetBoolField(TEXT("got"), Mark.bGot);
					MarkEntry->TryGetStringField(TEXT("name"), Mark.Name);
				}
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Others = nullptr;
	if (Object->TryGetArrayField(TEXT("others"), Others))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Others)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FResearchPower& Power = Out.Others.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Power.Tag);
			Entry->TryGetStringField(TEXT("name"), Power.Name);
			Entry->TryGetStringField(TEXT("doctrine"), Power.Doctrine);
			const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
			if (Entry->TryGetArrayField(TEXT("lines"), Lines))
			{
				for (const TSharedPtr<FJsonValue>& LineValue : *Lines)
				{
					const TSharedPtr<FJsonObject> Line = LineValue->AsObject();
					FString Label, Design;
					Line->TryGetStringField(TEXT("label"), Label);
					Line->TryGetStringField(TEXT("name"), Design);
					Power.Labels.Add(Label);
					Power.Designs.Add(Design);
				}
			}
		}
	}
	return Out;
}

bool FCampaignSession::SetTeams(const FString& Field, int32 Teams)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("teams"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Field), Teams), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::FoundBureau()
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("bureau"), TEXT("[]"), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::TakePath(const FString& Id, const FString& Partner, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("path"), FString::Printf(TEXT("[%s,%s]"), *JsonQuote(Id), *JsonQuote(Partner)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::InviteToLeague(const FString& Nation, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("invite"), FString::Printf(TEXT("[%s]"), *JsonQuote(Nation)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::Court(const FString& Nation, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("court"), FString::Printf(TEXT("[%s]"), *JsonQuote(Nation)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::Declare(const FString& Nation)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("declare"), FString::Printf(TEXT("[%s]"), *JsonQuote(Nation)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::CancelOrder(int32 OrderId)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("cancel"), FString::Printf(TEXT("[%d]"), OrderId), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	TakeView(Object);
	return bOk;
}
