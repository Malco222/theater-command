#pragma once

#include "CoreMinimal.h"
#include "Battle/SeaNightSky.h"
#include <vector>

class AActor;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USceneComponent;
class UWorld;

/**
 * THE NIGHT SKY'S DOME ON THE SEA (the release 2 ocean plan, step 11(f): the moon and stars on one unfogged dome; look
 * bible S7, "Night at sea"). The engine's half of SeaNightSky (engine-free, cloud-tested by Tools/sea_waves/
 * run_night_sky.sh): the real stars over the battle at its hour and the moon with her phase, laid as one still mesh
 * 400 km round the eye on M_NightSky (unlit, masked, "Is Sky", so neither the height fog nor the air's own haze touches
 * it). Two sections, the stars and the moon, each with its own strength. Laid afresh at each cut (the stars' squares
 * sized for the lens there, at least 1.5 of the screen's pixels across), carried with the eye between (the stars are at
 * infinity, so the eye never nears them), never rebuilt in between: under 0.1 ms a frame.
 *
 * Off unless the battle's rules gave its night (the field's light{sun, moon}) with the sun at least 4 deg under the
 * horizon, and the stars' strength above 0 (the scene reads "-TheaterSeaStars=" when step 11 wires it, in field mode).
 * Look only: the battle decides nothing by it.
 */
class FSeaNightDome
{
public:
	struct FRoom
	{
		AActor* Owner = nullptr;
		USceneComponent* Root = nullptr;
		double UnitsPerMetre = 100.0;
	};

	/** The sun this far under the horizon or more opens the dome (the light core's key turns from the sun to the moon at
	 * -4 deg); under -12 deg (the nautical twilight's end) the moon's whole disc is laid, earthshine on her dark side. */
	static constexpr double OpensUnder = -4.0, DarkUnder = -12.0;
	/** [EST] On the screen after the exposure: a magnitude-one star's light, and the full moon's. The Mac and the art
	 * director tune them ("-TheaterSeaStars=" scales the stars'). */
	static constexpr double StarScreen = 1.0, MoonScreen = 1.6;
	/** How far round the eye (metres), and how far the eye moves before the dome is carried after it (metres). */
	static constexpr double DomeMetres = 400000.0, FollowMetres = 25.0;

	/** Opens the dome for a battle: its place, date and night, and the stars' light (the scene's "-TheaterSeaStars=", 1
	 * as made; 0 leaves the dome off). Nothing is drawn until Lay. Returns whether it is on. */
	bool Open(const FRoom& InRoom, const SeaNightSky::When& InWhen, double InStarStrength);

	/** What else dims the stars: the sky's cloud (0 to 1) and the light core's StarShare (below 0: worked out from the
	 * sun's depth). Takes hold at the next Lay. */
	void SetSeeing(double Cover, double StarShare);

	/** The scene's exposure, in stops over the grade's (the light core's bias at night: +1.2 twilight, +1.8 moonlit,
	 * +2.4 dark), so a star looks as bright on the screen whatever the bias. Takes hold at the next Lay. */
	void SetExposure(double BiasStops);

	/** At each cut: the lens's full width across (degrees) and the eye. Lays the mesh afresh for the view's width in its
	 * internal pixels. */
	void Lay(double FovDeg, const FVector& Eye);

	/** Every frame: the dome stays round the eye. */
	void Follow(const FVector& Eye);

	void Close();

	bool IsOn() const { return bOn; }
	int32 StarsLaid() const { return static_cast<int32>(Sky.Stars.size()); }
	int32 GlowLaid() const { return static_cast<int32>(Sky.Glow.size()); }
	double LastLayMs() const { return LayMs; }

	/** The view's width in the pixels it is drawn at, before any upscaling (the screen's width x r.ScreenPercentage). */
	static double InternalWidth(UWorld* World);

private:
	FRoom Room;
	SeaNightSky::When When;
	SeaNightSky::Seeing See;
	SeaNightSky::Sky Sky;
	double StarStrength = 1.0, BiasStops = 2.4, LayMs = 0.0;
	bool bOn = false, bSaid = false;
	FVector Centre = FVector::ZeroVector;
	TWeakObjectPtr<UProceduralMeshComponent> Dome;
	TWeakObjectPtr<UMaterialInstanceDynamic> StarPaint, MoonPaint;
	void Paint();
};
