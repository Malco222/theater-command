#include "Campaign/CampaignGameMode.h"

#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignHUD.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignPhotographer.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
#include "TheaterCommand.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ACampaignGameMode::ACampaignGameMode()
{
	DefaultPawnClass = ACampaignCameraPawn::StaticClass();
	PlayerControllerClass = ACampaignPlayerController::StaticClass();
	HUDClass = ACampaignHUD::StaticClass();
}

namespace
{
	void SetRenderSetting(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByCode);
		}
	}

	/**
	 * The campaign table is one large model under steady light, seen from above. Unreal's bounced-light
	 * system (Lumen) costs about a third of every frame and adds nothing that can be seen here, so the
	 * campaign runs without it. Battles, which are full scenes, will choose for themselves.
	 */
	void ApplyCampaignRendering()
	{
		SetRenderSetting(TEXT("r.DynamicGlobalIlluminationMethod"), 0);
		SetRenderSetting(TEXT("r.ReflectionMethod"), 2); // reflections from what is on screen
		// The table is one very large model that is not yet in the form Unreal's shadow system likes best,
		// and from high above it says so on screen. The shadows are right; the note is switched off.
		SetRenderSetting(TEXT("r.Shadow.Virtual.AllowScreenOverflowMessages"), 0);
	}

	/** Opens the game in a window that suits the screen it is on, unless the command line asks for a size ("-TheaterWindow=1600x1000"). */
	void ApplyWindow()
	{
		UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (!Settings)
		{
			return;
		}
		FIntPoint Size(0, 0);
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterWindow="), Asked))
		{
			FString Width, Height;
			if (Asked.Split(TEXT("x"), &Width, &Height))
			{
				Size = FIntPoint(FCString::Atoi(*Width), FCString::Atoi(*Height));
			}
		}
		if (Size.X < 640 || Size.Y < 400)
		{
			const FIntPoint Desktop = Settings->GetDesktopResolution();
			Size.X = FMath::Clamp(FMath::RoundToInt32(Desktop.X * 0.86f), 1100, 2240);
			Size.Y = FMath::Clamp(FMath::RoundToInt32(Desktop.Y * 0.84f), 700, 1260);
		}
		Settings->SetScreenResolution(Size);
		Settings->SetFullscreenMode(EWindowMode::Windowed);
		Settings->ApplyResolutionSettings(false);
	}
}

void ACampaignGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ApplyCampaignRendering();
	ApplyWindow();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->SpawnActor<ACampaignEnvironment>(FVector::ZeroVector, FRotator::ZeroRotator);
	Table = World->SpawnActor<ACampaignMapActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (Table && Table->BuildMap())
	{
		// Start the campaign. The player leads Germany unless the command line says otherwise ("-TheaterNation=FRA").
		FString Human = TEXT("GER");
		FParse::Value(FCommandLine::Get(), TEXT("TheaterNation="), Human);
		Session = MakeShared<FCampaignSession>();
		FString Error;
		if (Session->Start(Human, Error))
		{
			Table->SetHolders(Session->GetView().RegionHolder);
			Table->ShowArmies(Session->GetView(), TArray<FCorpsMove>());
		}
		else
		{
			UE_LOG(LogTheater, Error, TEXT("The campaign could not start: %s"), *Error);
		}
	}

	// "-TheaterShots=<folder>" on the command line takes a set of pictures of the table and quits.
	FString ShotFolder;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), ShotFolder))
	{
		if (ACampaignPhotographer* Photographer = World->SpawnActor<ACampaignPhotographer>())
		{
			Photographer->Begin(ShotFolder);
		}
	}
}
