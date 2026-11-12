#include "Campaign/CampaignPhotographer.h"

#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignPlayerController.h"
#include "TheaterCommand.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "RenderTimer.h"
#include "RHI.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

ACampaignPhotographer::ACampaignPhotographer()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ACampaignPhotographer::Begin(const FString& InFolder)
{
	Folder = FPaths::ConvertRelativePathToFull(InFolder);
	IFileManager::Get().MakeDirectory(*Folder, true);

	Shots = {
		{ TEXT("1-whole-map"), TEXT(""), 0.97f, 0.0f, 0.0f, TEXT("") },
		{ TEXT("2-opening-view"), TEXT("Frankfurt"), 0.56f, 0.0f, 0.0f, TEXT("") },
		{ TEXT("3-poland-close"), TEXT("Warsaw"), 0.22f, -18.0f, 0.0f, TEXT("") },
		{ TEXT("4-alps"), TEXT("Tyrol"), 0.2f, 25.0f, 4.0f, TEXT("") },
		{ TEXT("5-paris-picked"), TEXT("Paris"), 0.36f, 0.0f, 0.0f, TEXT("Paris") },
		{ TEXT("6-mediterranean"), TEXT("Sicily"), 0.5f, 0.0f, 0.0f, TEXT("Malta") },
		{ TEXT("7-eastern-frontier"), TEXT("Lwow"), 0.42f, 35.0f, 0.0f, TEXT("") },
		{ TEXT("8-channel-low"), TEXT("Calais"), 0.12f, -30.0f, 6.0f, TEXT("") },
	};
	bStarted = true;
	UE_LOG(LogTheater, Display, TEXT("Photographer: %d pictures to take, into %s"), Shots.Num(), *Folder);
}

void ACampaignPhotographer::Arrange(const FShot& Shot)
{
	ACampaignPlayerController* Controller = Cast<ACampaignPlayerController>(GetWorld()->GetFirstPlayerController());
	ACampaignCameraPawn* Eye = Controller ? Cast<ACampaignCameraPawn>(Controller->GetPawn()) : nullptr;
	ACampaignMapActor* Table = Controller ? Controller->GetMap() : nullptr;
	if (!Eye || !Table)
	{
		return;
	}
	const FCampaignMapData& Data = Table->GetData();
	FVector2D At(Data.Width * 0.5, Data.Height * 0.5);
	if (const int32* Id = Data.RegionByName.Find(Shot.Region))
	{
		At = Data.Regions[*Id].Center;
	}
	Eye->SetHeading(Shot.Yaw, Shot.Tilt);
	Eye->GoTo(Table->MapToWorld(At.X, At.Y), Shot.Zoom, true);

	const int32* Pick = Data.RegionByName.Find(Shot.Select);
	Controller->SelectRegion(Pick ? *Pick : INDEX_NONE);
}

void ACampaignPhotographer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bStarted)
	{
		return;
	}

	// Wait for the materials to finish compiling the first time the game is run.
	if (Current < 0)
	{
		WarmUp += DeltaSeconds;
		const int32 Remaining = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
		if (FMath::FloorToInt32(WarmUp / 5.0f) != FMath::FloorToInt32((WarmUp - DeltaSeconds) / 5.0f))
		{
			UE_LOG(LogTheater, Display, TEXT("Photographer: waiting, %d shader jobs left after %.0f s"), Remaining, WarmUp);
		}
		if (WarmUp < 4.0f || (Remaining > 0 && WarmUp < 240.0f))
		{
			return;
		}
		Current = 0;
		Clock = 0.0f;
		Frames = 0;
		bTaken = false;
		Arrange(Shots[0]);
		return;
	}

	Clock += DeltaSeconds;
	++Frames;

	// Give the picture three seconds to settle, then take it.
	if (!bTaken && Clock >= 3.0f)
	{
		const FShot& Shot = Shots[Current];
		const FString File = FPaths::Combine(Folder, Shot.Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		FIntPoint View(0, 0);
		if (const APlayerController* Controller = GetWorld()->GetFirstPlayerController())
		{
			Controller->GetViewportSize(View.X, View.Y);
		}
		UE_LOG(LogTheater, Display, TEXT("Photographer: %s at %.1f frames a second (%dx%d; game %.1f ms, draw %.1f ms, graphics chip %.1f ms)"),
			*Shot.Name, Frames / Clock, View.X, View.Y,
			FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
		bTaken = true;
	}

	// A second later, move on.
	if (bTaken && Clock >= 4.0f)
	{
		++Current;
		if (!Shots.IsValidIndex(Current))
		{
			bStarted = false;
			UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
			return;
		}
		Clock = 0.0f;
		Frames = 0;
		bTaken = false;
		Arrange(Shots[Current]);
	}
}
