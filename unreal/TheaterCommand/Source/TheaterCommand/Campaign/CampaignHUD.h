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
	/** (air battle) The campaign's frame is folded away while a battle's scene and its screen are up, and comes back as it was. */
	void ShowFrame(bool bShow);

private:
	TSharedPtr<SWidget> Overlay;
};
