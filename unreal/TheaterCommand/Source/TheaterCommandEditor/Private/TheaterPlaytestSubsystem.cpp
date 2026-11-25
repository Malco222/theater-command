#include "TheaterPlaytestSubsystem.h"

#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignStepPlayer.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogTheaterPlaytest, Log, All);

namespace
{
	/** The step player's verdict word as the tools name it. */
	ETheaterStepVerdict VerdictOf(const FString& Word)
	{
		if (Word == TEXT("DID")) { return ETheaterStepVerdict::Did; }
		if (Word == TEXT("SAID WHY")) { return ETheaterStepVerdict::SaidWhy; }
		if (Word == TEXT("MUTE")) { return ETheaterStepVerdict::Mute; }
		if (Word == TEXT("MISSING")) { return ETheaterStepVerdict::Missing; }
		if (Word == TEXT("ERROR")) { return ETheaterStepVerdict::Error; }
		return ETheaterStepVerdict::Dead;
	}
}

void UTheaterPlaytestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &UTheaterPlaytestSubsystem::OnPostActorTick);
	EndHandle = FEditorDelegates::EndPIE.AddUObject(this, &UTheaterPlaytestSubsystem::OnEndPlay);
}

void UTheaterPlaytestSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
	FEditorDelegates::EndPIE.Remove(EndHandle);
	Fail(TEXT("The editor closed before the step finished."));
	Super::Deinitialize();
}

ACampaignPlayerController* UTheaterPlaytestSubsystem::FindPlayer(FString& OutError)
{
	UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
	if (!World)
	{
		OutError = TEXT("No play session is running. Start one on the campaign map.");
		return nullptr;
	}
	ACampaignPlayerController* Player = Cast<ACampaignPlayerController>(World->GetFirstPlayerController());
	if (!Player)
	{
		OutError = TEXT("The play session is not playing the campaign map.");
		return nullptr;
	}
	return Player;
}

FTheaterScreen UTheaterPlaytestSubsystem::ReadScreen(const ACampaignPlayerController& Player)
{
	FTheaterScreen Screen;
	Player.DescribeScreen(Screen.Open, Screen.Words, Screen.Hand);
	Screen.Showing = Player.StepState();
	Screen.bInBattle = Player.IsFightingLive();
	return Screen;
}

bool UTheaterPlaytestSubsystem::Play(const FString& Verb, const FString& Target, bool bInPicture, UTheaterStepAsyncResult* Result, FString& OutError)
{
	ACampaignPlayerController* Player = FindPlayer(OutError);
	if (!Player)
	{
		return false;
	}
	if (Pending)
	{
		OutError = TEXT("A step is already playing. Wait for it to finish.");
		return false;
	}
	if (!Steps.IsValid() || StepsWorld.Get() != Player->GetWorld())
	{
		// (a new play session: a fresh run, with its own log and pictures)
		const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Playtest"), FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		IFileManager::Get().MakeDirectory(*Folder, true);
		Steps = MakeShared<FStepPlayer>();
		Steps->BeginLive(Folder);
		StepsWorld = Player->GetWorld();
	}
	LoggedBefore = Steps->LoggedCount();
	Steps->Add(Verb, Target);
	Pending = Result;
	bPicture = bInPicture;
	PictureDeadline = 0.0;
	return true;
}

void UTheaterPlaytestSubsystem::OnPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (!Steps.IsValid() || !World || World != StepsWorld.Get())
	{
		return;
	}
	ACampaignPlayerController* Player = Cast<ACampaignPlayerController>(World->GetFirstPlayerController());
	if (!Player)
	{
		return;
	}
	if (Steps->IsPlaying())
	{
		Steps->Tick(*Player, DeltaSeconds);
		return;
	}
	if (Pending && Finish(*Player))
	{
		Pending = nullptr;
	}
}

bool UTheaterPlaytestSubsystem::Finish(const ACampaignPlayerController& Player)
{
	if (Steps->LoggedCount() <= LoggedBefore)
	{
		Pending->SetError(TEXT("The step ended without being judged."));
		return true;
	}
	const FStepPlayer::FRecord& Record = Steps->LastRecord();
	FTheaterStepResult Out;
	if (bPicture)
	{
		// (the step player asked for its picture as it logged the step; the file is written a frame or more later)
		const double Now = FPlatformTime::Seconds();
		if (PictureDeadline <= 0.0)
		{
			PictureDeadline = Now + 5.0;
		}
		if (!IFileManager::Get().FileExists(*Record.Picture) && Now < PictureDeadline)
		{
			return false;
		}
		if (!Out.Picture.SetFromFile(Record.Picture))
		{
			Pending->SetError(FString::Printf(TEXT("The step was played, but its picture could not be read: %s"), *Record.Picture));
			return true;
		}
	}
	Out.Verdict = VerdictOf(Record.Verdict);
	Out.Note = Record.Note;
	Out.Screen = ReadScreen(Player);
	// (the screen as the step was judged: for PointAt its words are the pointer's tip)
	Out.Screen.Open = Record.Open;
	Out.Screen.Words = Record.Words;
	Out.Screen.Hand = Record.Hand;
	Out.InputMs = static_cast<float>(Record.ActMs);
	Out.FirstChangeMs = static_cast<float>(Record.FirstMs);
	Out.LastChangeMs = static_cast<float>(Record.SettledMs);
	Out.ElapsedMs = static_cast<float>(Record.WaitMs);
	Out.SlowestFps = Record.FpsLow;
	Out.MedianFps = Record.FpsMid;
	Out.GameThreadMs = static_cast<float>(Record.GameMs);
	Out.RenderThreadMs = static_cast<float>(Record.DrawMs);
	Out.GpuMs = static_cast<float>(Record.ChipMs);
	Out.MonthScreens = Record.Screens;
	UE_LOG(LogTheaterPlaytest, Display, TEXT("Playtest: %s %s: %s"), *Record.Verb, *Record.Target, *Record.Verdict);
	Pending->SetValue(Out);
	return true;
}

void UTheaterPlaytestSubsystem::OnEndPlay(bool bIsSimulating)
{
	Fail(TEXT("The play session ended before the step finished."));
	Steps.Reset();
	StepsWorld.Reset();
}

void UTheaterPlaytestSubsystem::Fail(const FString& Error)
{
	if (Pending)
	{
		Pending->SetError(Error);
		Pending = nullptr;
	}
}
