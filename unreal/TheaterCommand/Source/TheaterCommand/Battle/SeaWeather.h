#pragma once

#include "CoreMinimal.h"
#include "Battle/SeaLookSwitches.h"
#include "Battle/SeaSky.h"
#include "UObject/StrongObjectPtr.h"

#include <memory>

class AActor;
class UDirectionalLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class UTexture2D;
class UTheaterStillMesh;

/**
 * THE FLEET'S WEATHER (the release 2 ocean plan, step 13: clouds and weather, tier 3), the engine's side; the sky
 * itself is Battle/SeaSky's, and the cloud test (Tools/sea_waves/sea_sky_test) lays and paints exactly what this draws.
 *
 * What it lays, when the day gives a sky (or the stand-in switch is on) and the switches ask for it:
 * - one sky dome, a still mesh of 48 by 12 quads at 150 km on M_SkyDome, showing the picture SeaSky::PaintDome paints of
 *   the laid clouds as the shot's own eye sees them. The picture is repainted at each cut on a pool thread and handed to
 *   the chip as soon as it lands (in a picture run, waited for in the settling ticks before a frame is kept; in play
 *   never waited for); until a repaint lands, the last picture stays. The material
 *   reads it by the camera's own way (u the bearing, v one less the height's sine), drifting slowly downwind;
 * - under a deck, the deck carried on to the horizon as one far sheet, and a haze band over the sea's far edge, as the
 *   air battle lays them (AAirBattle::LayDeckSheet and LayHazeBand), on the same M_Highlight;
 * - the clouds' shade on the sea and everything on it, by the key lamp's own light function (M_DeckShade, as
 *   AAirBattle::LayDeckShade lays it), so a cloud's shadow crosses the sea and the ships alike. The lamp's light
 *   function is kept as it was and given back in GiveBack (the plan's step 11(i) rule, Tools/sea_waves/room_scan);
 * - up to six far rain curtains under the darkest clouds, 6 to 18 km out, as the air battle's rain shafts, and the
 *   heaviest one's footprint for the sea's own material to darken and roughen (Squall). The rain at the eye is not
 *   here: that is the rain thread's own FWeatherFall, which takes the same strength (RainStrength).
 *
 * Everything is scaled by the day's light share (SetDayLight, the sea day's UnlitShare): the dome, the sheets, the band
 * and the rain dim together as the day does.
 *
 * Board axes are the sea scene's: X east, Y north, Z up, metres; the world's X is the board's north and its Y the
 * board's east (ASeaBattle::World), UnitsPerMetre to a metre. Nothing here counts: it is only what is drawn.
 */
class FSeaWeather
{
public:
	/** What the scene lends it. */
	struct FRoom
	{
		AActor* Owner = nullptr;
		USceneComponent* Root = nullptr;
		/** The battle's sun lamp (ASeaBattle's BattleKey), whose light function lays the clouds' shade. */
		TWeakObjectPtr<UDirectionalLightComponent> Key;
		/** The way to the key body in the world's axes (ASeaBattle::TowardSun, the stage's by day, the moon's by night). */
		FVector Sun = FVector::UpVector;
		double UnitsPerMetre = 100.0;
		/** A night action: no clouds are drawn (the night dome has the stars, and a cloud dome by moonlight is the rain
		 * thread's and the night sky's business, not this one's). */
		bool bNight = false;
		/** A picture run (-TheaterSeaBattleShots=): a dome painted at a cut is waited for before the frame is kept. In
		 * play it never is: the last picture stays until the new one lands, so a cut never stalls the game thread. */
		bool bPictures = false;
	};

	FSeaWeather() = default;
	~FSeaWeather();
	FSeaWeather(const FSeaWeather&) = delete;
	FSeaWeather& operator=(const FSeaWeather&) = delete;

	/**
	 * Reads the five weather switches from the look (writing each one's value in force back into it, so the "Sea look:"
	 * line says what was applied), chooses the day from the recording's field sky, lays it, and makes what is asked for.
	 * MiddleX and MiddleY are the board's middle in metres; WindBoard the field's wind in board axes (metres a second,
	 * the way it blows toward, SeaShow::Wind); Seed the battle's.
	 */
	void Begin(const FRoom& InRoom, SeaLookSwitches::Look& InForce, const FString& RecordingText, uint64 Seed,
		double MiddleX, double MiddleY, const FVector2D& WindBoard);

	/** Anything of the weather drawn at all. */
	bool IsOn() const { return bOn; }
	/** The day laid, and why, for the log. */
	const SeaSky::Sky& Day() const { return Chosen; }

	/** The day's light share (the sea day's UnlitShare, step 3): everything drawn here is scaled by it. 1 by default. */
	void SetDayLight(double Share);
	double DayLight() const { return Lit; }

	/** After AimEye, every frame: the eye in the world, whether the camera cut this frame, and the battle's second. */
	void Tick(const FVector& EyeWorld, bool bCut, double BattleSeconds);

	/** The room given back as it was (the key lamp's light function): the scene ending. */
	void GiveBack();
	/** The scene ending however it ends (ASeaBattle::EndPlay: taken down, a level unloaded, the game quit): a paint still
	 * on a pool thread is waited for (a whole dome is under a second; it writes only this one's own picture and holds
	 * nothing of the actor's), then everything is given back as GiveBack gives it. */
	void End();

	/** The overview shot's eye height, metres (SeaSky::OverviewHeight): 1,300 unless the dome draws a deck, then under
	 * its base. Fixed for the battle. */
	double OverviewHeight() const;
	/** Whether the dome draws a deck, and its base, metres (the sea free eye's ceiling stays under it: SeaFreeEye::Ceiling).
	 * Fixed for the battle. */
	bool DeckDrawn(double& OutBase) const;

	/** The heaviest rain's footprint on the sea, for MPC_FleetSea's Squall (step 7). None without rain. */
	const SeaSky::Squall& Squall() const { return Wet; }
	/** How heavy the rain is, 0 to 1: what the rain at the eye (FWeatherFall) must be given too, so the two agree. */
	double RainStrength() const { return Chosen.bLaid ? Chosen.Rain : 0.0; }
	/** How much of the sky is cloud, 0 to 1 (the night dome thins its stars by it; 0 with no sky laid). */
	double CoverShare() const { return Chosen.bLaid ? Chosen.Cover : 0.0; }
	/** How much the clouds take off the sun over the whole square, 0 to 1 (M_FleetSea's CloudShade darkens the crests'
	 * glow and the sky's reflection by it); 0 with no shade laid. */
	double CloudShade() const;
	/** The day's light under its cloud at an eye's height, as shares of a clear day's (AirSky::LightOf). */
	AirSky::Lighting LightShares(double EyeZMetres) const;

	/** The dome's own frame times, for the "Sea frames:" line. */
	void SayFrameTimes() const;

private:
	FRoom Room;
	SeaSky::Asked Switches;
	SeaSky::Sky Chosen;
	AirSky::Field Sky;
	AirSky::DeckShade Shade;
	SeaSky::Rain Rain;
	SeaSky::Squall Wet;
	bool bOn = false;
	bool bShade = false;
	double Lit = 1.0;
	double MiddleEast = 0.0, MiddleNorth = 0.0;
	FVector2D Wind = FVector2D::ZeroVector;
	uint64 Seed = 0;

	// The dome: one still mesh, its picture painted on a pool thread and handed to the chip when it is ready.
	TWeakObjectPtr<UTheaterStillMesh> Dome;
	TWeakObjectPtr<UMaterialInstanceDynamic> DomeInk;
	TStrongObjectPtr<UTexture2D> Picture;
	std::shared_ptr<SeaSky::Dome> Painted;  // (the picture last handed over)
	std::shared_ptr<SeaSky::Dome> Painting; // (the one a pool thread is painting now)
	std::shared_ptr<AirSky::Painter> Brush;
	std::vector<uint8_t> Shapes;
	TArray<FColor> Pixels;
	TFuture<void> Job;
	bool bWaiting = false;
	bool bCutLater = false; // (a cut that came while a paint was under way: painted for as soon as that one lands)
	int32 SinceAsked = 0;
	double AskedAt = -1e9, PaintedAt = 0.0;
	FVector AskedEye = FVector::ZeroVector;
	int32 Repaints = 0, Waits = 0;
	double PaintCost = 0.0, WorstPaint = 0.0, UploadCost = 0.0;

	// The deck's far sheet and the haze band over the sea's far edge (a deck day only).
	TWeakObjectPtr<UProceduralMeshComponent> DeckSheet;
	TWeakObjectPtr<UMaterialInstanceDynamic> DeckSheetInk;
	double DeckSheetZ = 0.0, DeckSheetShown = -2.0;
	TWeakObjectPtr<UProceduralMeshComponent> HazeBand;
	TWeakObjectPtr<UMaterialInstanceDynamic> HazeBandInk;
	FVector HazeBandLaidAt = FVector(1e30, 1e30, 1e30);
	double HazeBandShown = -1.0;
	double SeaHalf = 0.0;

	// The rain's curtains.
	TWeakObjectPtr<UProceduralMeshComponent> Curtains;
	TWeakObjectPtr<UMaterialInstanceDynamic> CurtainsInk;

	// The key lamp's light function, kept as it was.
	struct FKeptFunction
	{
		TWeakObjectPtr<UMaterialInterface> Function;
		float Fade = 0.0f;
		float Dark = 0.0f;
	};
	FKeptFunction KeptKey;
	TWeakObjectPtr<UMaterialInstanceDynamic> ShadeInk;
	TStrongObjectPtr<UTexture2D> ShadeSheet;

	/** A board place in the world's own axes (ASeaBattle::World). */
	FVector World(double East, double North, double Up) const;
	void LayDome();
	void LayDeckSheet();
	void LayHazeBand();
	void LayShade();
	void LayRain();
	void AskPaint(const FVector& EyeWorld, double BattleSeconds);
	void TakePaint();
	void DeckSheetAt(const FVector& EyeWorld);
	void HazeBandAt(const FVector& EyeWorld);
	void SetLightEverywhere();
};
