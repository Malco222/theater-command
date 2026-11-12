#include "Campaign/CampaignGameMode.h"

#include "Campaign/CampaignFlags.h"
#include "ImageUtils.h"

#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignHUD.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignPhotographer.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
#include "TheaterCommand.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
		// The upscaler one step below its costliest setting: the picture is the same to the eye and the frame is quicker.
		SetRenderSetting(TEXT("sg.AntiAliasingQuality"), 2);
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
		Session = MakeShared<FCampaignSession>();
		FString Error;
		if (!Session->Prepare(Error))
		{
			UE_LOG(LogTheater, Error, TEXT("The rules could not be started: %s"), *Error);
		}
		// Normally the opening screen asks which campaign to play. "-TheaterNation=FRA" on the command line skips it
		// and starts the grand campaign as that country in September 1939.
		FString Human;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterNation="), Human))
		{
			StartCampaign(Human, 1, TEXT("grand"));
		}
	}

	// "-TheaterFlags=<file.png>" on the command line saves every flag on one sheet, for checking them.
	FString FlagSheet;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFlags="), FlagSheet))
	{
		TArray<FString> Tags = CampaignFlags::KnownTags();
		Tags.Sort();
		const int32 PerRow = 7, Gap = 16, W = CampaignFlags::SizeX * 3 / 4, H = CampaignFlags::SizeY; // shown three by two
		const int32 SheetRows = (Tags.Num() + PerRow - 1) / PerRow;
		const int32 SheetW = PerRow * (W + Gap) + Gap, SheetH = SheetRows * (H + Gap) + Gap;
		TArray<FColor> Sheet;
		Sheet.Init(FColor(70, 70, 70), SheetW * SheetH);
		for (int32 Index = 0; Index < Tags.Num(); ++Index)
		{
			TArray<FLinearColor> Pixels;
			CampaignFlags::Paint(Tags[Index], FLinearColor::Gray, Pixels);
			const int32 X0 = Gap + (Index % PerRow) * (W + Gap), Y0 = Gap + (Index / PerRow) * (H + Gap);
			for (int32 Y = 0; Y < H; ++Y)
			{
				for (int32 X = 0; X < W; ++X)
				{
					Sheet[(Y0 + Y) * SheetW + X0 + X] = Pixels[Y * CampaignFlags::SizeX + X * CampaignFlags::SizeX / W].ToFColor(true);
				}
			}
		}
		FImageUtils::SaveImageByExtension(*FlagSheet, FImageView(Sheet.GetData(), SheetW, SheetH));
		UE_LOG(LogTheater, Display, TEXT("Flags: %s on %s"), *FString::Join(Tags, TEXT(" ")), *FlagSheet);
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

void ACampaignGameMode::LayOutWar()
{
	if (Table && Session.IsValid() && Session->IsRunning())
	{
		// The rivers and railways the rules know are laid on the table, then the war as it stands.
		Table->SetLand(Session->Land());
		Table->SetHolders(Session->GetView().RegionHolder, Session->GetView().League);
		Table->ShowArmies(Session->GetView(), TArray<FCorpsMove>());
		Table->ShowWorks(Session->GetView().BridgesDown, Session->GetView().RailsCut);
		Table->ShowBuildings(Session->GetView().RegionWorks);
	}
}

bool ACampaignGameMode::StartCampaign(const FString& Human, int32 From, const FString& ViewMode, int32 Seed, const FString& Level)
{
	FString Error;
	if (!Session.IsValid() || !Session->Begin(Human, From, ViewMode, Seed, Error, Level))
	{
		UE_LOG(LogTheater, Error, TEXT("The campaign could not start: %s"), *Error);
		return false;
	}
	LayOutWar();
	return true;
}

FString ACampaignGameMode::SaveFile()
{
	// A run that only takes pictures keeps its own save, so that it never writes over the player's campaign.
	FString Shots;
	const bool bPictures = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Shots, false);
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Campaign"), bPictures ? TEXT("campaign-pictures.json") : TEXT("campaign.json"));
}

void ACampaignGameMode::SaveCampaign()
{
	if (!Session.IsValid() || !Session->IsRunning())
	{
		return;
	}
	const FString Text = Session->Save();
	if (!Text.IsEmpty())
	{
		FFileHelper::SaveStringToFile(Text, *SaveFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

FString ACampaignGameMode::DescribeSavedCampaign()
{
	FString Text;
	if (!Session.IsValid() || !FFileHelper::LoadFileToString(Text, *SaveFile()))
	{
		return FString();
	}
	return Session->DescribeSave(Text);
}

bool ACampaignGameMode::ContinueCampaign()
{
	FString Text;
	if (!Session.IsValid() || !FFileHelper::LoadFileToString(Text, *SaveFile()) || !Session->Load(Text))
	{
		return false;
	}
	LayOutWar();
	return true;
}

void ACampaignGameMode::BeginPlay()
{
	Super::BeginPlay();

	// The music: one original piece on a loop, quiet enough to think over. It is made by Tools/build_music.sh
	// and is not kept in the repository, so a copy of the game without it simply plays in silence.
	if (USoundBase* Theme = LoadObject<USoundBase>(nullptr, TEXT("/Game/Campaign/Music/Theme.Theme")))
	{
		// "-TheaterQuiet" plays it too softly to hear: for checking that it plays without disturbing anyone.
		const float Volume = FParse::Param(FCommandLine::Get(), TEXT("TheaterQuiet")) ? 0.0005f : 0.55f;
		Music = UGameplayStatics::SpawnSound2D(this, Theme, Volume, 1.0f, 0.0f, nullptr, false, false);
		if (Music)
		{
			Music->bIsUISound = true; // keeps playing if the game is paused
			Music->FadeIn(4.0f, Volume);
		}
		UE_LOG(LogTheater, Display, TEXT("Music: %s, %.0f seconds on a loop"), Music && Music->IsPlaying() ? TEXT("playing") : TEXT("not playing"), Theme->GetDuration());
	}
	else
	{
		UE_LOG(LogTheater, Display, TEXT("No music found. Run Tools/build_music.sh to make it."));
	}
}

bool ACampaignGameMode::ToggleMusic()
{
	if (!Music)
	{
		return false;
	}
	bMusicOn = !bMusicOn;
	if (bMusicOn)
	{
		Music->FadeIn(1.5f, 0.55f);
	}
	else
	{
		Music->FadeOut(1.0f, 0.0f);
	}
	return bMusicOn;
}
