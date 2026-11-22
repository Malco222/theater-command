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
	 * The two parts of the hour's light (the art director's ruling of round 26). The evening is not a point on a
	 * straight road from the warm day to the blue night, which passes through grey: it is a light of its own, amber
	 * on what the lamp lights and cool in the shade, most itself where the evening rests when a month ends. The
	 * moon's blue comes in only after it.
	 */
	static float DuskOf(float Night); // how much of the evening's own light there is: 0 by day, 1 where the evening rests, 0 from the deep of the night
	static float MoonOf(float Night); // how far the moonlight has come in: 0 until the evening rests, 1 at night
	static float DuskRests(); // how far into the night the evening rests when a month ends
	/**
	 * (Ruling 205(a); the art director's speed and night brief, 17 November 2026. Brian took the night for the fog of
	 * war: "the US seems like fog of war even though i own the entire country".) The moon's light and what the land
	 * keeps under it, so that ground the player sees rests at about six tenths of its day brightness, at least half
	 * again as bright as the fog beside it and no bluer than that fog. One place holds the numbers for the room's
	 * lights and for the map's paint. ("-TheaterMoonLook=key,fill,sky,keyR,keyG,keyB,skyR,skyG,skyB,haze,politics,
	 * sunR,sunG,sunB,shadeR,shadeG,shadeB,hazeFloor" tries others without a rebuild.)
	 */
	struct FMoonLook
	{
		// (Found by measure on 17 November 2026, from Germany's chair over the Saar and from America's over Texas and
		// New York. Before: key 3.0 of (0.3, 0.5, 1.0), fill 0.55, sky 0.52 of (0.42, 0.58, 1.0), haze 1.35, politics
		// 0.42: Germany's own ground stood at (31, 58, 75) beside fog at (23, 33, 45), and America's at (41, 62, 81)
		// beside Mexico's at (25, 43, 63): as dark as the fog and bluer. Now: Germany's (58, 80, 85) beside fog at
		// (30, 37, 44), nearly twice as bright as it and about two thirds of its day brightness; America's (76, 90, 87).)
		float Key = 3.35f, Fill = 0.60f, Sky = 0.56f; // the lamp's, the fill's and the sky's strength at full night
		FLinearColor KeyColour = FLinearColor(0.55f, 0.68f, 1.0f);
		FLinearColor SkyColour = FLinearColor(0.60f, 0.70f, 1.0f);
		float Haze = 0.72f; // how much of the day's share of light the fog of war's haze keeps by night: less, so that the fog stays as dark as it was built while the ground in view is lifted
		float Politics = 0.60f; // how much of a nation's colour its ground keeps by night (0.94 by day)
		float HazeFloor = 1.0f; // how much of the haze's floor (the least it lets through) is kept by night: lower, and the fog stays dark while the ground in view is lifted
		FLinearColor Sun = FLinearColor(0.90f, 0.90f, 1.0f); // the land's own printed light and shade by the moon
		FLinearColor Shade = FLinearColor(0.55f, 0.58f, 0.72f);
		// (The art director's check of ruling 205(a), 17 November 2026: about a sixth of Germany's own ground by night
		// was as dark as the fog beside it or darker. Measured cell by cell, those patches were the woods: the dark
		// cloth of a wood and the trees standing on it kept 0.44 of their day brightness by night where open ground
		// keeps 0.6, and a wood is dark to begin with. By night the cloth is laid thinner, so that the moonlit ground
		// and its nation's colour show through it, and the trees are dimmed less.)
		// (Measured over the Saar from Germany's chair: a wood's floor in Hesse went from (21, 38, 45), under the fog's
		// (29, 37, 45), to (38, 55, 62), half as bright again as the fog; the share of Germany's ground at the fog's
		// level or darker fell from 16 to 13 parts in 100, and what is left is the trees' own dark sides, the pieces
		// and the hills' shade, which keeps seven tenths of the open ground's brightness.)
		float Woods = 0.5f; // how much of its day strength the woods' dark cloth keeps by night
		FLinearColor Trees = FLinearColor(0.80f, 0.95f, 1.15f); // the trees' own tint by the moon (it was (0.36, 0.56, 0.8))
	};
	static const FMoonLook& MoonLook();
	/**
	 * Dusk when the month ends (Brian's approved list). While the rest of the world moves, evening comes over the
	 * table: not the full night, so that the others' moves are still seen. With the player's new month it is
	 * morning again, and the room's day begins afresh from there. Nothing where the player has held the hour (T).
	 */
	void SetMonthsEnd(bool bEnded);
	bool IsMonthsEnd() const { return bMonthsEnd; }
	/**
	 * (Ruling 107; the art director's brief.) A raid flown by night is watched in the dark: while one plays the hour
	 * falls to night, over about a second and a half, whatever the hour was and however the player keeps time; when
	 * it is over the hour goes back to where it was.
	 */
	void SetRaidNight(bool bOn) { bRaidNight = bOn; }
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
	bool bRaidNight = false; // a raid by night is playing: it is night until it is over
	float HeldNight = -1.0f; // (for pictures, "-TheaterDusk=0.5": the hour is held this far into the night)
	bool bLamplight = false;
	void ApplyLight();
	void LightRoom();

};
