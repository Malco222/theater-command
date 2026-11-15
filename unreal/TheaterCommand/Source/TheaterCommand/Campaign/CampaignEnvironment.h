#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CampaignEnvironment.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;

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
	int32 GetTimeKeeping() const { return TimeKeeping; }
	void SetTimeKeeping(int32 Keeping) { TimeKeeping = FMath::Clamp(Keeping, 0, 2); }
	/** How far into the night the room's light has gone, 0 to 1. */
	float GetNight() const { return Night; }
	/**
	 * Dusk when the month ends (Brian's approved list). While the rest of the world moves, evening comes over the
	 * table: not the full night, so that the others' moves are still seen. With the player's new month it is
	 * morning again, and the room's day begins afresh from there. Nothing where the player has held the hour (T).
	 */
	void SetMonthsEnd(bool bEnded);
	bool IsMonthsEnd() const { return bMonthsEnd; }
	/** The night as it was first made: the room's warm lamp alone. Otherwise the night is moonlight. */
	void SetLamplight(bool bLamp) { if (bLamp != bLamplight) { bLamplight = bLamp; ApplyLight(); } }
	bool IsLamplight() const { return bLamplight; }
	/**
	 * The studio's light (the art director's ruling of round 19): a unit's picture is a plate in a recognition manual,
	 * and must look the same at any hour and in any season. While this is on the room's three lights stand as on a
	 * plain day; turned off, they go back to the hour. It is put on for the moment a picture is taken and no longer.
	 */
	void StudioLight(bool bOn);

private:
	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<UDirectionalLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<USkyLightComponent> SkyLight;

	/** The air of the room: the far part of the table fades a little toward a pale blue-grey. */
	UPROPERTY(VisibleAnywhere, Category = "Light")
	TObjectPtr<UExponentialHeightFogComponent> Air;
	float AirStart = -1.0f, AirDensity = -1.0f, AirNight = -1.0f;
	void UpdateAir();

	float SeasonWinter = 0.0f, SeasonSummer = 0.0f;
	double DayClock = 0.12; // where in the room's day it is, from 0 to 1: the morning to begin with
	float Night = 0.0f; // how far into the night the light has gone, 0 to 1
	FString DayClockHeld; // (only read from the command line)
	int32 TimeKeeping = 0; // 0 the day runs, 1 held at day, 2 held at night
	bool bMonthsEnd = false; // the month has ended and the others are moving: it is evening
	float HeldNight = -1.0f; // (for pictures, "-TheaterDusk=0.5": the hour is held this far into the night)
	bool bLamplight = false;
	void ApplyLight();
	void LightRoom();

};
