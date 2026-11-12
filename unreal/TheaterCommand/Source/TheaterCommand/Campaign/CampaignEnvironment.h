#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CampaignEnvironment.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;

/**
 * The light over the campaign table: one warm lamp from the north-west, as on
 * a printed relief map, a faint cool light from the other side so the shadowed
 * slopes are not black, and soft light from all round.
 */
UCLASS()
class THEATERCOMMAND_API ACampaignEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ACampaignEnvironment();

private:
	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<UDirectionalLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<USkyLightComponent> SkyLight;
};
