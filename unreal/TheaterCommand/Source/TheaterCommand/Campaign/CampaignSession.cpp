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
	View = ParseView(Object);
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

	const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
	if (Object->TryGetArrayField(TEXT("regions"), Regions))
	{
		Out.RegionHolder.SetNum(Regions->Num());
		Out.RegionStanding.SetNum(Regions->Num());
		for (const TSharedPtr<FJsonValue>& Value : *Regions)
		{
			const TSharedPtr<FJsonObject> Region = Value->AsObject();
			const int32 Id = Region->GetIntegerField(TEXT("id"));
			if (Out.RegionHolder.IsValidIndex(Id))
			{
				Out.RegionHolder[Id] = Region->GetStringField(TEXT("ctrl"));
				Region->TryGetStringField(TEXT("stand"), Out.RegionStanding[Id]);
			}
		}
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
			Unit->TryGetStringField(TEXT("gen"), C.General); // null when nobody is named
			Unit->TryGetStringField(TEXT("posture"), C.Posture);
			C.Divisions = Unit->GetIntegerField(TEXT("divs"));
			C.Strength = static_cast<float>(Unit->GetNumberField(TEXT("str")));
			C.Cohesion = static_cast<float>(Unit->GetNumberField(TEXT("org")));
			C.Region = Unit->GetIntegerField(TEXT("reg"));
			C.Moves = Unit->GetIntegerField(TEXT("ap"));
			C.MovesMax = Unit->GetIntegerField(TEXT("apMax"));
			Unit->TryGetBoolField(TEXT("canOrder"), C.bCanOrder);
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
	TakeView(Object);
	return Out;
}
