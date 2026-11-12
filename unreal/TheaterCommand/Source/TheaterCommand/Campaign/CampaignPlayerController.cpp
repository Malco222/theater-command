#include "Campaign/CampaignPlayerController.h"

#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignMapActor.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"

ACampaignPlayerController::ACampaignPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	PrimaryActorTick.bCanEverTick = true;
}

void ACampaignPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	if (TActorIterator<ACampaignMapActor> It(GetWorld()); It)
	{
		Map = *It;
	}

	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		if (Map.IsValid() && Map->GetData().IsLoaded())
		{
			Eye->SetBounds(Map->GetWorldHalfExtent());
			// Open on central Europe, with Germany, Poland and France all in view.
			const int32* Start = Map->GetData().RegionByName.Find(TEXT("Frankfurt"));
			const FVector2D At = Start ? Map->GetData().Regions[*Start].Center : FVector2D(Map->GetData().Width * 0.42, Map->GetData().Height * 0.46);
			Eye->GoTo(Map->MapToWorld(At.X + 60.0, At.Y + 10.0), 0.56f, true);
		}
	}
}

void ACampaignPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ACampaignPlayerController::WheelUp);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ACampaignPlayerController::WheelDown);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ACampaignPlayerController::ClearSelection);
}

void ACampaignPlayerController::WheelUp()
{
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		Eye->ZoomBy(-1.0f);
	}
}

void ACampaignPlayerController::WheelDown()
{
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		Eye->ZoomBy(1.0f);
	}
}

void ACampaignPlayerController::ClearSelection()
{
	SelectRegion(INDEX_NONE);
}

bool ACampaignPlayerController::PointerOnTable(FVector& OutWorld) const
{
	FVector Origin, Direction;
	if (!Map.IsValid() || !DeprojectMousePositionToWorld(Origin, Direction) || Direction.Z > -0.01)
	{
		return false;
	}
	// Meet the water level first, then step down onto the ground that is actually there.
	double PlaneZ = 0.0;
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		const double T = (PlaneZ - Origin.Z) / Direction.Z;
		OutWorld = Origin + Direction * T;
		const FVector2D P = Map->WorldToMap(OutWorld);
		PlaneZ = FMath::Max(0.0, static_cast<double>(Map->GetTerrain().HeightAt(P.X, P.Y))) * Map->UnitScale * Map->VerticalScale;
	}
	return true;
}

void ACampaignPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Eye || !Map.IsValid())
	{
		return;
	}

	// Keys: W A S D or the arrows slide the view, Q and E turn it, + and - zoom.
	FVector2D Slide = FVector2D::ZeroVector;
	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) Slide.Y += 1.0;
	if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) Slide.Y -= 1.0;
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) Slide.X += 1.0;
	if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) Slide.X -= 1.0;
	Eye->Pan(Slide, DeltaTime);
	if (IsInputKeyDown(EKeys::Q)) Eye->RotateBy(-70.0f * DeltaTime);
	if (IsInputKeyDown(EKeys::E)) Eye->RotateBy(70.0f * DeltaTime);
	if (IsInputKeyDown(EKeys::Equals) || IsInputKeyDown(EKeys::Add)) Eye->ZoomBy(-12.0f * DeltaTime);
	if (IsInputKeyDown(EKeys::Hyphen) || IsInputKeyDown(EKeys::Subtract)) Eye->ZoomBy(12.0f * DeltaTime);

	float MouseX = 0.0f, MouseY = 0.0f;
	const bool bHaveMouse = GetMousePosition(MouseX, MouseY);
	const FVector2D Mouse(MouseX, MouseY);
	const FVector2D MouseDelta = (bHaveMouse && bHaveLastMouse) ? Mouse - LastMouse : FVector2D::ZeroVector;

	int32 ViewWidth = 0, ViewHeight = 0;
	GetViewportSize(ViewWidth, ViewHeight);

	// A bigger window is not allowed to cost more: the picture is drawn at about the same number of
	// points whatever the window's size, and Unreal's upscaler fills in the rest.
	if (ViewWidth != SizedForWidth || ViewHeight != SizedForHeight)
	{
		SizedForWidth = ViewWidth;
		SizedForHeight = ViewHeight;
		if (ViewWidth > 0 && ViewHeight > 0)
		{
			const double Budget = 1250.0 * 780.0;
			const int32 Percent = FMath::Clamp(FMath::RoundToInt32(100.0 * FMath::Sqrt(Budget / (static_cast<double>(ViewWidth) * ViewHeight))), 50, 100);
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
			{
				Variable->Set(Percent, ECVF_SetByCode);
			}
		}
	}

	// Right button held: turn and tilt the view.
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		Eye->RotateBy(static_cast<float>(MouseDelta.X) * 0.3f);
		Eye->TiltBy(static_cast<float>(-MouseDelta.Y) * 0.2f);
	}

	// Left button: a press and release picks a territory; a press and drag pulls the map along under the pointer.
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bHaveMouse)
	{
		bLeftHeld = true;
		bDragging = false;
		PressPosition = Mouse;
	}
	if (bLeftHeld && IsInputKeyDown(EKeys::LeftMouseButton) && bHaveMouse)
	{
		if (!bDragging && FVector2D::Distance(Mouse, PressPosition) > 6.0)
		{
			bDragging = true;
		}
		if (bDragging && ViewWidth > 0)
		{
			// How much table one pixel covers at the point being looked at.
			const double PerPixel = 2.0 * Eye->GetDistance() * FMath::Tan(FMath::DegreesToRadians(Eye->GetHorizontalFov() * 0.5)) / ViewWidth;
			const FRotator Heading(0.0f, Eye->GetYaw(), 0.0f);
			const FVector Forward = Heading.Vector();
			const FVector Right = FRotationMatrix(Heading).GetUnitAxis(EAxis::Y);
			const double Foreshorten = 1.0 / FMath::Max(0.3, FMath::Sin(FMath::DegreesToRadians(-Eye->GetPitch())));
			Eye->PanWorld(-Right * MouseDelta.X * PerPixel + Forward * MouseDelta.Y * PerPixel * Foreshorten);
		}
	}
	if (bLeftHeld && !IsInputKeyDown(EKeys::LeftMouseButton))
	{
		if (!bDragging)
		{
			FVector Hit;
			SelectRegion(PointerOnTable(Hit) ? Map->RegionAtWorld(Hit) : INDEX_NONE);
		}
		bLeftHeld = false;
		bDragging = false;
	}

	// Whatever the pointer rests on is lit faintly.
	if (!bDragging && !IsInputKeyDown(EKeys::RightMouseButton))
	{
		FVector Hit;
		Map->SetHoveredRegion(bHaveMouse && PointerOnTable(Hit) ? Map->RegionAtWorld(Hit) : INDEX_NONE);
	}

	Map->UpdateForCamera(Eye->GetDistance(), Eye->GetYaw());

	LastMouse = Mouse;
	bHaveLastMouse = bHaveMouse;
}

void ACampaignPlayerController::SelectRegion(int32 RegionId)
{
	if (!Map.IsValid())
	{
		return;
	}
	Map->SetSelectedRegion(RegionId);
	RefreshRegionPanel(RegionId);
}

void ACampaignPlayerController::RefreshRegionPanel(int32 RegionId)
{
	RegionPanel = FRegionPanelInfo();
	if (!Map.IsValid() || !Map->GetData().Regions.IsValidIndex(RegionId))
	{
		return;
	}
	const FCampaignMapData& Data = Map->GetData();
	const FCampaignRegion& Region = Data.Regions[RegionId];
	const FCampaignNation* Owner = Data.FindNation(Region.Owner);

	RegionPanel.bVisible = true;
	RegionPanel.Name = FText::FromString(Region.Name);
	RegionPanel.OwnerColor = Owner ? FLinearColor(Owner->Color) : FLinearColor::Gray;

	FString OwnerLine = Owner ? Owner->Name : Region.Owner;
	if (Region.bCapital)
	{
		OwnerLine += TEXT("  ·  Capital");
	}
	else if (Region.bColony)
	{
		OwnerLine += TEXT("  ·  Overseas territory");
	}
	RegionPanel.OwnerLine = FText::FromString(OwnerLine);

	TArray<FString> Lines;
	Lines.Add(FCampaignMapData::TerrainLabel(Region.Terrain));
	if (Region.Factories > 0)
	{
		Lines.Add(FString::Printf(TEXT("Industry %d"), Region.Factories));
	}
	if (Region.VictoryPoints > 0)
	{
		Lines.Add(FString::Printf(TEXT("Worth %d"), Region.VictoryPoints));
	}
	if (Region.bPort) Lines.Add(TEXT("Port"));
	if (Region.FortLevel > 0) Lines.Add(FString::Printf(TEXT("Fortified, level %d"), Region.FortLevel));
	if (Region.bOil) Lines.Add(TEXT("Oilfields"));
	if (Region.bOre) Lines.Add(TEXT("Iron ore"));
	if (Region.bDepot) Lines.Add(TEXT("Supply depot"));
	RegionPanel.Details = FText::FromString(FString::Join(Lines, TEXT("   ·   ")));

	// Who is standing here.
	TArray<FString> ForceLines;
	for (const AArmyPiece* Piece : Map->GetPieces())
	{
		if (!Piece || Piece->GetRegionId() != RegionId)
		{
			continue;
		}
		const FCampaignNation* Nation = Data.FindNation(Piece->GetNationTag());
		const FString Adjective = Nation && !Nation->Adjective.IsEmpty() ? Nation->Adjective : Piece->GetNationTag();
		FString Header = FString::Printf(TEXT("%s army, %d division%s"), *Adjective, Piece->GetDivisions(), Piece->GetDivisions() == 1 ? TEXT("") : TEXT("s"));
		if (!Piece->GetGeneral().IsEmpty())
		{
			Header += FString::Printf(TEXT(", under %s"), *Piece->GetGeneral());
		}
		ForceLines.Add(Header);
		for (const FCampaignCorps& Unit : Piece->GetCorps())
		{
			ForceLines.Add(FString::Printf(TEXT("      %s  (%s, %d)"), *Unit.Name, *FCampaignMapData::CorpsKindLabel(Unit.Kind), Unit.Divisions));
		}
	}
	RegionPanel.Forces = FText::FromString(ForceLines.Num() > 0 ? FString::Join(ForceLines, TEXT("\n")) : FString(TEXT("No troops here.")));
}
