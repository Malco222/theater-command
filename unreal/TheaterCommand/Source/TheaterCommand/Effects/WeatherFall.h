#pragma once

// Rain and snow falling round the eye in the 3D game (release 2, the look bible's G10), the engine's half: a few meshes
// made once (one a layer: WeatherFallCore's), M_LandFall on each (Tools/build_content.py: the drops carried, wrapped
// about the eye and turned to it on the graphics chip, WeatherFallChip.inl), and a few numbers a frame. It knows no
// scene: the land battle tells it what falls (LandWeather), the sea's storms may tell it theirs. Nothing in any fight
// reads it.

#include "CoreMinimal.h"
#include "Effects/BattleEffects.h"
#include "Effects/WeatherFallCore.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;

/**
 * What falls round the eye. Make it with MakeUnique and keep the pointer; its meshes are its owner's components (hung
 * on the parent, which keeps them alive): Clear takes them away, or let them go with the owner.
 *
 *   Fall = MakeUnique<FWeatherFall>();
 *   Fall->LoadRecipe();                        // Content/Effects/land-weather.json ("fall" its layers)
 *   Fall->Setup(Root, UnitsPerMetre);          // false (told) without M_LandFall: nothing falls
 *   Fall->SetFall(WeatherFall::Kind::Rain, 0.8);
 *   each frame: SetWind, SetInk, SetLight as they change, then Tick(eye, lens, picture's width, seconds, running)
 *
 * The drops stay put in the world while they are near (each layer's mesh is set down on its own grid of whole boxes
 * nearest the eye, so the chip's wrap about the eye lands each drop where it was), and the clock runs only while the
 * scene's does (a held battle holds its rain in the air).
 */
class FWeatherFall
{
public:
	FWeatherFall();
	~FWeatherFall();
	FWeatherFall(const FWeatherFall&) = delete;
	FWeatherFall& operator=(const FWeatherFall&) = delete;

	/** Reads the recipe (a file in Content/Effects) and the layers in its "fall" over the standing ones. False, with a
	 * warning, where the file is missing: the standing numbers then. Setup reads it if it was not read. */
	bool LoadRecipe(const FString& FileName = TEXT("land-weather.json"));
	/** A number from the recipe ("pick.rain.from_cover"): false where it gives none. */
	bool Number(const char* Path, double& Out) const;

	/** Loads M_LandFall; false, with a warning, without it (nothing will fall; the rest of the weather is the scene's).
	 * Seed: the drops' places (the same battle, the same drops). */
	bool Setup(USceneComponent* Parent, double InUnitsPerMetre, uint64 InSeed = 1);
	bool IsReady() const { return bReady; }

	/** What falls and how hard (0 to 1); None or 0 draws none. A change of kind makes that kind's meshes. */
	void SetFall(WeatherFall::Kind InKind, double InStrength);
	/** The wind, metres a second along the world's own X and Y. */
	void SetWind(double WorldX, double WorldY);
	/** The drops' colour (linear): what the haze shows behind them, as the scene's haze is lit. */
	void SetInk(const FLinearColor& Linear);
	/** The light's share on them (night lower), 0 to 1. */
	void SetLight(double Share);

	/** Once a frame: the eye (world), the lens's width (degrees, across), the picture's pixels across, the real
	 * seconds since the last frame, and whether the scene's clock runs. */
	void Tick(const FVector& EyeWorld, double FovDegrees, double ViewWidthPx, double Seconds, bool bRunning);

	/** Takes the meshes away. */
	void Clear();

	bool IsFalling() const { return Kind != WeatherFall::Kind::None && Strength > 0.0 && Layers.Num() > 0; }
	WeatherFall::Kind KindNow() const { return Kind; }
	double StrengthNow() const { return Strength; }
	/** In plain words, for the log: the layers, their drops, and what shows. */
	FString Said() const;

private:
	struct FShownLayer
	{
		WeatherFall::Layer Layer;
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Ink;
		int32 Drops = 0;
		FVector Placed = FVector(1.0e30, 1.0e30, 1.0e30);
	};

	void Make();

	FBattleFxRecipe Book;
	bool bBook = false;
	WeatherFall::Recipe Recipe;
	TWeakObjectPtr<UMaterialInterface> Material; // (an asset; loaded again should it have gone before a mesh held it)
	bool bReady = false;
	TWeakObjectPtr<USceneComponent> Holder;
	double UnitsPerMetre = 100.0;
	uint64 Seed = 1;
	WeatherFall::Kind Kind = WeatherFall::Kind::None;
	WeatherFall::Kind Made = WeatherFall::Kind::None;
	double Strength = 0.0;
	double WindX = 0.0, WindY = 0.0;
	FLinearColor InkNow = FLinearColor(0.5f, 0.52f, 0.55f, 1.0f);
	double Light = 1.0;
	double Clock = 0.0;
	TArray<FShownLayer> Layers;
};
