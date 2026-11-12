#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CampaignGameMode.generated.h"

/**
 * Sets the campaign up when the game starts: lays the table, lights it, and
 * gives the player a view of it. The level itself is empty; everything on the
 * table is built from the campaign data.
 */
UCLASS()
class THEATERCOMMAND_API ACampaignGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACampaignGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};
