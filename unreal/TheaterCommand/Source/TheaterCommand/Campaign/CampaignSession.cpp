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

namespace
{
	FString IdList(const TArray<int32>& Ids)
	{
		return FString::JoinBy(Ids, TEXT(","), [](int32 Id) { return FString::FromInt(Id); });
	}
}

FCampaignSession::FCampaignSession() = default;
FCampaignSession::~FCampaignSession() = default;

bool FCampaignSession::Start(const FString& Human, FString& OutError)
{
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

	FString ViewJson;
	if (!Host->Call(TEXT("start"), FString::Printf(TEXT("[{\"human\":\"%s\"}]"), *Human), ViewJson, OutError))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Object;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ViewJson), Object) || !Object.IsValid())
	{
		OutError = TEXT("The rules did not return a picture of the war");
		return false;
	}
	ReadView(Object);
	bRunning = true;
	UE_LOG(LogTheater, Display, TEXT("Campaign started on the %s rules: %s, %d corps, the player leads %s"),
		*View.Rules, *View.Date, View.Corps.Num(), *View.Human);
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

void FCampaignSession::ReadView(const TSharedPtr<FJsonObject>& Object)
{
	View = FCampaignView();
	Object->TryGetStringField(TEXT("rules"), View.Rules);
	View.Turn = Object->GetIntegerField(TEXT("turn"));
	Object->TryGetStringField(TEXT("date"), View.Date);
	Object->TryGetStringField(TEXT("human"), View.Human);
	Object->TryGetStringField(TEXT("current"), View.Current);
	Object->TryGetBoolField(TEXT("humanUp"), View.bHumanUp);

	const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
	if (Object->TryGetArrayField(TEXT("regions"), Regions))
	{
		View.RegionHolder.SetNum(Regions->Num());
		for (const TSharedPtr<FJsonValue>& Value : *Regions)
		{
			const TSharedPtr<FJsonObject> Region = Value->AsObject();
			const int32 Id = Region->GetIntegerField(TEXT("id"));
			if (View.RegionHolder.IsValidIndex(Id))
			{
				View.RegionHolder[Id] = Region->GetStringField(TEXT("ctrl"));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Corps = nullptr;
	if (Object->TryGetArrayField(TEXT("corps"), Corps))
	{
		View.Corps.Reserve(Corps->Num());
		for (const TSharedPtr<FJsonValue>& Value : *Corps)
		{
			const TSharedPtr<FJsonObject> Unit = Value->AsObject();
			FViewCorps C;
			C.Id = Unit->GetIntegerField(TEXT("id"));
			C.Nation = Unit->GetStringField(TEXT("nat"));
			C.Name = Unit->GetStringField(TEXT("name"));
			C.Kind = Unit->GetStringField(TEXT("kind"));
			Unit->TryGetStringField(TEXT("gen"), C.General); // null when nobody is named
			Unit->TryGetStringField(TEXT("posture"), C.Posture);
			C.Divisions = Unit->GetIntegerField(TEXT("divs"));
			C.Strength = static_cast<float>(Unit->GetNumberField(TEXT("str")));
			C.Cohesion = static_cast<float>(Unit->GetNumberField(TEXT("org")));
			C.Region = Unit->GetIntegerField(TEXT("reg"));
			C.Moves = Unit->GetIntegerField(TEXT("ap"));
			C.MovesMax = Unit->GetIntegerField(TEXT("apMax"));
			Unit->TryGetBoolField(TEXT("canOrder"), C.bCanOrder);
			View.Corps.Add(MoveTemp(C));
		}
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
			Out.Add(R);
		}
	}
	return Out;
}

TArray<FCorpsMove> FCampaignSession::Move(const TArray<int32>& CorpsIds, int32 RegionId)
{
	TArray<FCorpsMove> Out;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("move"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		return Out;
	}
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
	const TSharedPtr<FJsonObject>* NewView = nullptr;
	if (Object->TryGetObjectField(TEXT("view"), NewView))
	{
		ReadView(*NewView);
		OnViewChanged.Broadcast();
	}
	return Out;
}

TArray<FString> FCampaignSession::EndMonth()
{
	TArray<FString> Notes;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("endMonth"), TEXT("[]"), Object))
	{
		return Notes;
	}
	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	if (Object->TryGetArrayField(TEXT("events"), Events))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Events)
		{
			FString Text;
			if (Value->AsObject()->TryGetStringField(TEXT("text"), Text))
			{
				Notes.Add(Text);
			}
		}
	}
	const TSharedPtr<FJsonObject>* NewView = nullptr;
	if (Object->TryGetObjectField(TEXT("view"), NewView))
	{
		ReadView(*NewView);
		OnViewChanged.Broadcast();
	}
	return Notes;
}
