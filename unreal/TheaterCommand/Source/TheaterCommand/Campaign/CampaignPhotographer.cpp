#include "Campaign/CampaignPhotographer.h"

#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignPlayerController.h"
#include "TheaterCommand.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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

	auto Add = [this](const TCHAR* Name, const TCHAR* Region, float Zoom, float Yaw = 0.0f, float Tilt = 0.0f) -> FShot&
	{
		FShot& Shot = Shots.AddDefaulted_GetRef();
		Shot.Name = Name;
		Shot.Region = Region;
		Shot.Zoom = Zoom;
		Shot.Yaw = Yaw;
		Shot.Tilt = Tilt;
		return Shot;
	};
	Shots.Reset();
	// The opening screen, as the game starts.
	Add(TEXT("00-opening-screen"), TEXT("Frankfurt"), 0.56f).bOpening = true;
	// The table.
	Add(TEXT("01-whole-map"), TEXT(""), 0.97f);
	Add(TEXT("02-opening-view"), TEXT("Frankfurt"), 0.56f);
	Add(TEXT("03-poland-close"), TEXT("Warsaw"), 0.22f, -18.0f);
	Add(TEXT("04-alps"), TEXT("Tyrol"), 0.2f, 25.0f, 4.0f);
	Add(TEXT("05-paris-picked"), TEXT("Paris"), 0.36f).Select = TEXT("Paris");
	Add(TEXT("06-mediterranean"), TEXT("Sicily"), 0.5f).Select = TEXT("Malta");
	Add(TEXT("07-eastern-frontier"), TEXT("Lwow"), 0.42f, 35.0f);
	Add(TEXT("08-channel-low"), TEXT("Calais"), 0.12f, -30.0f, 6.0f);
	// The game: an army in hand, an attack, its result, a march, and the month after.
	{
		FShot& Shot = Add(TEXT("09-army-in-hand"), TEXT("Berlin"), 0.4f);
		Shot.Select = Shot.Army = TEXT("Farther Pomerania");
		Shot.Route = TEXT("Hamburg");
	}
	{
		FShot& Shot = Add(TEXT("10-face-off"), TEXT("Pomorze"), 0.34f);
		Shot.Army = TEXT("Farther Pomerania");
		Shot.Attack = TEXT("Pomorze");
	}
	{
		FShot& Shot = Add(TEXT("11-battle-result"), TEXT("Pomorze"), 0.34f);
		Shot.bKeep = Shot.bConfirm = true;
	}
	{
		FShot& Shot = Add(TEXT("12-ground-taken"), TEXT("Pomorze"), 0.34f);
		Shot.bKeep = Shot.bClose = true;
	}
	{
		FShot& Shot = Add(TEXT("13-army-on-the-march"), TEXT("Berlin"), 0.4f);
		Shot.Army = TEXT("Neumark");
		Shot.March = TEXT("Magdeburg");
	}
	{
		// The flag picked up by hand and carried: held over ground it can reach, then let go there.
		FShot& Shot = Add(TEXT("13b-flag-in-the-hand"), TEXT("Mecklenburg"), 0.3f);
		Shot.Army = TEXT("Hamburg");
		Shot.Carry = TEXT("Mecklenburg");
	}
	{
		FShot& Shot = Add(TEXT("13c-flag-set-down"), TEXT("Mecklenburg"), 0.3f);
		Shot.bKeep = true;
		Shot.Drop = TEXT("Mecklenburg");
	}
	{
		FShot& Shot = Add(TEXT("14-next-month"), TEXT("Warsaw"), 0.5f);
		Shot.Months = 1;
	}
	Add(TEXT("14b-menu-with-save"), TEXT("Warsaw"), 0.5f).bMenu = true;
	{
		// A province of one's own: what it holds, what can be raised there, and something put on order.
		FShot& Shot = Add(TEXT("14c-province"), TEXT("Berlin"), 0.4f);
		Shot.Select = TEXT("Berlin");
		Shot.Order = TEXT("mtn");
	}
	{
		FShot& Shot = Add(TEXT("14d-by-rail"), TEXT("Saxony"), 0.5f);
		Shot.Army = TEXT("Rhineland");
		Shot.bRail = true;
	}
	{
		// Attack again, then end the month and catch the other nations' month being played out.
		FShot& Shot = Add(TEXT("15-attack-again"), TEXT("Pomorze"), 0.34f);
		Shot.Army = TEXT("Grenzmark");
		Shot.Attack = TEXT("Pomorze");
	}
	{
		FShot& Shot = Add(TEXT("16-watching-the-others"), TEXT("Pomorze"), 0.45f);
		Shot.bKeep = Shot.bConfirm = Shot.bClose = Shot.bWatch = true;
	}

	// "-TheaterStart=5,0" begins from another place on the opening screen (here the sixth group, its first side) and
	// takes the pictures that show what units have made of themselves by then.
	FString Start;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterStart="), Start, false)) // false: the comma is part of the value
	{
		FString Group, Side;
		if (Start.Split(TEXT(","), &Group, &Side))
		{
			StartGroup = FCString::Atoi(*Group);
			StartSide = FCString::Atoi(*Side);
		}
		Shots.Reset();
		Add(TEXT("20-honours-on-the-standards"), TEXT("Masuria"), 0.2f, -10.0f, 4.0f).Army = TEXT("Masuria");
		Add(TEXT("21-army-with-stories"), TEXT("Masuria"), 0.36f).Army = TEXT("Masuria");
		{
			FShot& Shot = Add(TEXT("22-service-record"), TEXT("Masuria"), 0.36f);
			Shot.Army = TEXT("Masuria");
			Shot.bRecord = true;
		}
		Add(TEXT("23-roll-of-honour"), TEXT("Masuria"), 0.36f).bRoll = true;
	}

	// "-TheaterOnly=13b,13c" takes only the pictures whose names begin with one of these.
	FString Only;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterOnly="), Only, false))
	{
		TArray<FString> Wanted;
		Only.ParseIntoArray(Wanted, TEXT(","));
		Shots.RemoveAll([&Wanted](const FShot& Shot)
		{
			return !Wanted.ContainsByPredicate([&Shot](const FString& Prefix) { return Shot.Name.StartsWith(Prefix); });
		});
	}

	// "-TheaterQuick" takes two pictures only, for trying a setting out.
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterQuick")))
	{
		const TArray<FShot> All = Shots;
		if (All.Num() > 8)
		{
			Shots = { All[2], All[4], All[8] };
		}
	}
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
	// The first picture after the opening screen begins a campaign through it, as Germany in September 1939.
	if (!Shot.bOpening && !Controller->GetSession())
	{
		Controller->BeginCampaign(StartGroup, StartSide);
	}
	const FCampaignMapData& Data = Table->GetData();
	FVector2D At(Data.Width * 0.5, Data.Height * 0.5);
	if (const int32* Id = Data.RegionByName.Find(Shot.Region))
	{
		At = Data.Regions[*Id].Center;
	}
	Eye->SetHeading(Shot.Yaw, Shot.Tilt);
	Eye->GoTo(Table->MapToWorld(At.X, At.Y), Shot.Zoom, true);

	Controller->SetScripted(true);
	Table->SetHoveredRegion(INDEX_NONE);
	if (Shot.bOpening)
	{
		return;
	}
	if (Shot.bMenu)
	{
		Controller->OpenMenu();
		return;
	}
	Controller->CloseMenu();
	Controller->CloseDocument();
	if (!Shot.bKeep)
	{
		Controller->CloseFaceOff();
		Controller->PutDownArmy();
	}
	if (Shot.bConfirm)
	{
		Controller->ConfirmAttack();
	}
	if (Shot.bClose)
	{
		Controller->CloseFaceOff();
	}
	for (int32 i = 0; i < Shot.Months; ++i)
	{
		Controller->EndMonth();
	}
	if (Shot.bWatch)
	{
		Controller->PutDownArmy();
		Controller->EndMonthAndWatch();
		return; // the view now follows the action
	}
	if (!Shot.bKeep)
	{
		const int32* Pick = Data.RegionByName.Find(Shot.Select);
		Controller->SelectRegion(Pick ? *Pick : INDEX_NONE);
	}
	if (const int32* Army = Data.RegionByName.Find(Shot.Army))
	{
		if (Shot.Carry.IsEmpty())
		{
			Controller->PickUpArmyIn(*Army);
		}
		else if (const int32* Over = Data.RegionByName.Find(Shot.Carry))
		{
			const bool bHeld = Controller->HoldArmyIn(*Army);
			Controller->CarryHeldOver(*Over);
			UE_LOG(LogTheater, Display, TEXT("Photographer: flag in %s %s, carried over %s"), *Shot.Army, bHeld ? TEXT("taken in hand") : TEXT("NOT taken"), *Shot.Carry);
		}
	}
	if (const int32* Drop = Data.RegionByName.Find(Shot.Drop))
	{
		Controller->DropHeldOver(*Drop);
	}
	if (const int32* Route = Data.RegionByName.Find(Shot.Route))
	{
		Controller->ShowRouteTo(*Route);
	}
	if (const int32* Target = Data.RegionByName.Find(Shot.Attack))
	{
		Controller->OrderTo(*Target);
	}
	if (!Shot.Order.IsEmpty())
	{
		Controller->ProvinceAction(Shot.Order);
	}
	if (Shot.bRail)
	{
		Controller->ToggleRail();
	}
	if (Shot.bRecord)
	{
		// The corps of the army in hand that has the most to tell.
		int32 Best = INDEX_NONE, Most = -1;
		for (const FTrayCard& Card : Controller->GetArmyTray().Cards)
		{
			if (Card.Honours.Num() > Most)
			{
				Most = Card.Honours.Num();
				Best = Card.CorpsId;
			}
		}
		if (Best != INDEX_NONE)
		{
			Controller->OpenRecord(Best);
		}
	}
	if (Shot.bRoll)
	{
		Controller->OpenRoll();
	}
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

	// An army ordered off just before the picture is caught in the air.
	if (!bMarched && Clock >= 2.45f && !Shots[Current].March.IsEmpty())
	{
		bMarched = true;
		ACampaignPlayerController* Controller = Cast<ACampaignPlayerController>(GetWorld()->GetFirstPlayerController());
		const int32* To = Controller && Controller->GetMap() ? Controller->GetMap()->GetData().RegionByName.Find(Shots[Current].March) : nullptr;
		if (To)
		{
			Controller->OrderTo(*To);
		}
	}

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
		bMarched = false;
		Arrange(Shots[Current]);
	}
}
