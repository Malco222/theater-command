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
	/** The light of the time of year: how hard the winter is and how high the summer, each from 0 to 1. */
	void SetSeason(float Winter, float Summer);
	ACampaignEnvironment();
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * The room's day goes slowly by: daylight from the windows, dusk, the lamp alone at night, and dawn. This
	 * steps to the next way of keeping time (running, held at day, held at night) and says which it now is.
	 */
	int32 NextTimeKeeping();

private:
	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<UDirectionalLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<USkyLightComponent> SkyLight;

	float SeasonWinter = 0.0f, SeasonSummer = 0.0f;
	double DayClock = 0.12; // where in the room's day it is, from 0 to 1: the morning to begin with
	float Night = 0.0f; // how far into the night the light has gone, 0 to 1
	FString DayClockHeld; // (only read from the command line)
	int32 TimeKeeping = 0; // 0 the day runs, 1 held at day, 2 held at night
	void ApplyLight();

};
