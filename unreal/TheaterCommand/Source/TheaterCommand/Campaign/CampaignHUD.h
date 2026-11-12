#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CampaignHUD.generated.h"

class SWidget;

/** Puts the campaign's panels on the screen: the date plate, the territory panel, and the reminder of the controls. */
UCLASS()
class THEATERCOMMAND_API ACampaignHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TSharedPtr<SWidget> Overlay;
};
