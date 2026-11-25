#include "TheaterPlaytestToolset.h"

#include "Editor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TheaterPlaytestSubsystem.h"

namespace
{
	/** The step file's word for a verb. */
	const TCHAR* VerbWord(ETheaterStepVerb Verb)
	{
		switch (Verb)
		{
		case ETheaterStepVerb::Start: return TEXT("start");
		case ETheaterStepVerb::Load: return TEXT("load");
		case ETheaterStepVerb::Snapshot: return TEXT("snapshot");
		case ETheaterStepVerb::Restore: return TEXT("restore");
		case ETheaterStepVerb::SelectProvince: return TEXT("select-province");
		case ETheaterStepVerb::CheckProvince: return TEXT("check-province");
		case ETheaterStepVerb::SelectUnit: return TEXT("select-unit");
		case ETheaterStepVerb::OrderProvince: return TEXT("order-province");
		case ETheaterStepVerb::OrderUnit: return TEXT("order-unit");
		case ETheaterStepVerb::OrderSea: return TEXT("order-sea");
		case ETheaterStepVerb::AskWar: return TEXT("ask-war");
		case ETheaterStepVerb::PointAt: return TEXT("point-at");
		case ETheaterStepVerb::ClickMark: return TEXT("click-mark");
		case ETheaterStepVerb::LookAt: return TEXT("look-at");
		case ETheaterStepVerb::PanZoom: return TEXT("pan-zoom");
		case ETheaterStepVerb::OpenPage: return TEXT("open-page");
		case ETheaterStepVerb::Press: return TEXT("press");
		case ETheaterStepVerb::Close: return TEXT("close");
		case ETheaterStepVerb::Key: return TEXT("key");
		case ETheaterStepVerb::EndMonth: return TEXT("end-month");
		case ETheaterStepVerb::LandKey: return TEXT("land-key");
		case ETheaterStepVerb::LandClick: return TEXT("land-click");
		case ETheaterStepVerb::LandWait: return TEXT("land-wait");
		case ETheaterStepVerb::AirKey: return TEXT("air-key");
		case ETheaterStepVerb::AirWait: return TEXT("air-wait");
		}
		return nullptr;
	}
}

FTheaterScreen UTheaterPlaytestToolset::GetScreen()
{
	FString Error;
	const ACampaignPlayerController* Player = UTheaterPlaytestSubsystem::FindPlayer(Error);
	if (!Player)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return FTheaterScreen();
	}
	return UTheaterPlaytestSubsystem::ReadScreen(*Player);
}

UTheaterStepAsyncResult* UTheaterPlaytestToolset::PlayStep(ETheaterStepVerb Verb, const FString& Target, bool bPicture)
{
	UTheaterStepAsyncResult* Result = NewObject<UTheaterStepAsyncResult>();
	const TCHAR* Word = VerbWord(Verb);
	const bool bTakesNoTarget = Verb == ETheaterStepVerb::Close || Verb == ETheaterStepVerb::EndMonth;
	UTheaterPlaytestSubsystem* Runner = GEditor ? GEditor->GetEditorSubsystem<UTheaterPlaytestSubsystem>() : nullptr;
	FString Error;
	if (!Word)
	{
		Error = TEXT("Unknown verb.");
	}
	else if (!bTakesNoTarget && Target.TrimStartAndEnd().IsEmpty())
	{
		Error = FString::Printf(TEXT("Target must not be empty for %s."), *UEnum::GetValueAsString(Verb));
	}
	else if (!Runner)
	{
		Error = TEXT("The editor is not available.");
	}
	else
	{
		Runner->Play(Word, bTakesNoTarget ? FString() : Target, bPicture, Result, Error);
	}
	if (!Error.IsEmpty())
	{
		Result->SetError(Error);
	}
	return Result;
}
