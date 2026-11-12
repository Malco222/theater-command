#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CampaignPlayerController.generated.h"

class ACampaignMapActor;
class ACampaignCameraPawn;

/** What the panel shows about the territory the player has clicked. Worked out once, when the selection changes. */
struct FRegionPanelInfo
{
	bool bVisible = false;
	FText Name;
	FText OwnerLine;
	FLinearColor OwnerColor = FLinearColor::Gray;
	FText Details;
	FText Forces;
};

/**
 * The player's hands on the campaign table: moving the view, pointing at
 * territories and picking them.
 */
UCLASS()
class THEATERCOMMAND_API ACampaignPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACampaignPlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	const FRegionPanelInfo& GetRegionPanel() const { return RegionPanel; }
	ACampaignMapActor* GetMap() const { return Map.Get(); }

	/** Selects a territory, or none with INDEX_NONE. */
	void SelectRegion(int32 RegionId);

private:
	void WheelUp();
	void WheelDown();
	void ClearSelection();

	/** Where on the table the pointer is, allowing for the height of the ground. */
	bool PointerOnTable(FVector& OutWorld) const;

	void RefreshRegionPanel(int32 RegionId);

	TWeakObjectPtr<ACampaignMapActor> Map;
	FRegionPanelInfo RegionPanel;

	bool bLeftHeld = false;
	bool bDragging = false;
	bool bHaveLastMouse = false;
	int32 SizedForWidth = 0;
	int32 SizedForHeight = 0;
	FVector2D PressPosition = FVector2D::ZeroVector;
	FVector2D LastMouse = FVector2D::ZeroVector;
};
