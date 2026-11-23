#pragma once

#include "CoreMinimal.h"

struct FPostProcessSettings;
class AActor;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;

/**
 * The look of a battle seen through its own cameras (the art director's grit direction of 19 November 2026, item 2,
 * and her ruling on the air battle the same day): a colour grade that is on battle cameras only, and the haze of a
 * battle fought in the air. The campaign's map and its frame keep their own colours.
 */
namespace BattleLook
{
	/**
	 * The battle grade, written into a battle camera's own settings. The exposure is fixed, as the table's is, so that
	 * a dark sea under bright cloud stays dark; there is more contrast; the blue cast is taken down; the darks and the
	 * middle tones are drained of colour and the brights left theirs, so that fire, flashes and tracer stay saturated
	 * inside the grey. "-TheaterBattleGrade=contrast,darks,middles,brights,red,blue,stops" tries other numbers (leave
	 * one empty to keep it); "-TheaterNoBattleGrade" leaves the camera as its scene made it.
	 */
	void Grade(FPostProcessSettings& Lens);
	/**
	 * The haze of an air battle: none near the eye, so that the sea and the land under it keep their darks beneath
	 * the clouds, and thick toward the horizon. "-TheaterAirHaze=start in metres,density,falloff,most" tries other
	 * numbers (leave one empty to keep it). Start is where it begins, in metres from the eye, and Thicker how many times
	 * the scene's own haze it is.
	 */
	void AirHaze(UExponentialHeightFogComponent& Haze, double UnitsPerMetre, double Start = 15000.0, double Thicker = 2.0);
	/**
	 * A sky behind an air battle. The table has none (it is looked down on), and a battle seen from the air showed
	 * its haze over nothing: black behind it, darker the higher the eye looked, and a straight seam along the horizon
	 * where the hazed sea ended (the art director, 19 November 2026). This is the engine's own air and sky, lit by the
	 * battle's sun; the scene's other lamps are kept out of it. AirSkyEnd takes it away and puts the lamps back as they
	 * were. "-TheaterAirSky=air,dust,far,haze,bright" tries other numbers (how blue the air, how much dust in it, how
	 * fast distance pales, how much of the haze lies over the sky, how bright the sky); "-TheaterNoAirSky" leaves the
	 * sky as it was.
	 */
	void AirSkyBegin(AActor& Scene, UDirectionalLightComponent* Sun);
	void AirSkyEnd();
}
