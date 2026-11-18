#pragma once

#include "CoreMinimal.h"
#include "Effects/TheaterFxCore.h"
#include "Templates/Function.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <functional>
#include <map>
#include <string>

class FJsonValue;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProceduralMeshComponent;
class USceneComponent;

/** The camera's jolt from hits close by, for the caller to add to its own camera if it wants it: it is never moved
 * here. Offset is in world units. A setting turns it off (FBattleEffects::bShake). */
struct FBattleJolt
{
	FVector Offset = FVector::ZeroVector;
	float RollDegrees = 0.0f;
};

/** The recipe, Content/Effects/battle-fx.json, read for the effects (paths "sizes.how105.crown_m.0"). */
class FBattleFxRecipe final : public TheaterFx::Recipe
{
public:
	bool Load(const FString& Path);
	virtual bool Find(const char* Path, double& Out) const override;
	virtual bool FindWord(const char* Path, std::string& Out) const override;

private:
	// (flattened once at Load, path to value, as tools/battle-fx/preview/flatten.py does: a landing shell asks
	// thousands of times, so each ask is one look-up, with no string made and no tree walked)
	std::map<std::string, double, std::less<>> Numbers;
	std::map<std::string, std::string, std::less<>> Words;
	void Walk(const TSharedPtr<FJsonValue>& Value, const std::string& Path);
};

/**
 * The battle effects in the 3D game: shells and bombs landing, guns firing, tracer, burning wrecks, the craters left
 * behind and the camera's jolt (the art director's sheet, art-director/explosions; Brian 18 November: "very dramatic
 * and life like... like im in it and watching a movie at the same time").
 *
 * It knows nothing of the scene it is put in. Give it a parent to hang its mesh and lamps on, how many world units
 * make a metre, and a way to find the ground under a point; tell it what happens (Shell, Muzzle, Shots, Burn); and
 * tick it once a frame with the eye. It draws everything as soft puffs in one procedural mesh with the campaign map's
 * own M_Highlight (see-through, unlit, coloured by its points), the same shape of puff as the map's smoke, so no new
 * material is needed; flashes light the ground with at most two point lamps (game.max_lamps), shadowless.
 *
 * Every number is in the recipe, so the look is tuned there without a code change. The work itself is plain C++
 * (TheaterFxCore), which the cloud preview draws too (tools/battle-fx/preview).
 *
 * Keep it where it was made (it hands its own address to the core): make it with MakeUnique and keep the pointer.
 * Its mesh and lamps are its owner's components (hung on the parent, so the parent keeps them alive); call End in the
 * owner's EndPlay to take them away, or let them go with the owner.
 */
class FBattleEffects
{
public:
	FBattleEffects();
	~FBattleEffects();
	FBattleEffects(const FBattleEffects&) = delete;
	FBattleEffects& operator=(const FBattleEffects&) = delete;

	/** Reads the recipe and makes the mesh and lamps under Parent (its owner holds them). GroundUnder gives the
	 * world point on the ground under a world point. False, with a warning, if the recipe or material is missing. */
	bool Begin(USceneComponent* Parent, double InUnitsPerMetre, TFunction<FVector(const FVector&)> InGroundUnder);
	bool IsReady() const;
	/** Takes the mesh and lamps away (the owner's EndPlay). Begin may be called again after. */
	void End();

	/** A shell from the field battle: its calibre ("light", "mortar", "field", "howitzer", "heavy"), the ground it
	 * fell on ("earth", "sand", "snow", "mud", "town", "woods", "water") and whether it struck armour. */
	void Shell(const FVector& WorldAt, const FString& Calibre, const FString& Ground, bool bOnArmour = false);
	/** The same by the recipe's own size row ("grenade", "light", "mortar", "gun75", "how105", "how150", "bomb250",
	 * "bomb500"): bombs, grenades, anything that is not a gun's calibre. */
	void ShellOfSize(const FVector& WorldAt, const FString& Size, const FString& Ground, bool bOnArmour = false);
	/** An armour-piercing round that misses and skips along Heading: a low spurt of earth, no crown, no crater. */
	void Skip(const FVector& WorldAt, const FVector& Heading, const FString& Ground);
	/** A gun firing: its muzzle at WorldAt, pointing along Facing. Gun is "rifle", "mg", "tank_gun", "at_gun",
	 * "howitzer" or "mortar". */
	void Muzzle(const FVector& WorldAt, const FVector& Facing, const FString& Gun);
	/** Rounds fired by one gun (any number that stays the same for that gun) toward To: every fifth a tracer. */
	void Shots(int32 Gun, const FVector& From, const FVector& To, const FString& Weapon, int32 Rounds, float PerSecond);
	/** Something burning, about AcrossMetres wide: flames for Seconds, then a thinning grey column. */
	void Burn(const FVector& WorldAt, float AcrossMetres, float Seconds);
	/** The battle's wind (metres a second, world axes): smoke leans and drifts with it. */
	void SetWind(const FVector& MetresPerSecond);
	/** The way toward the sun (world axes): each puff is lightened on that side. */
	void SetSun(const FVector& TowardSun);
	/** Everything gone: a new battle. */
	void Clear();

	/** Once a frame, with the world's time in seconds (what happens between ticks is stamped with the last tick's time)
	 * and the eye. HourLight is what the hour does to smoke (white by day; the campaign map's night rule): flame,
	 * flashes and tracer keep their own colour. Returns the camera's jolt. */
	FBattleJolt Tick(double Now, const FVector& Eye, const FRotator& Facing, float FieldOfViewDegrees, int32 ViewWidth, const FLinearColor& HourLight = FLinearColor::White);

	/** Camera shake on or off (Options: "Camera shake", default on). */
	bool bShake = true;

	/** -TheaterBlastTest on the command line: lay a test barrage round a place (RunTest) for test frames. */
	static bool WantsTest();
	/** A barrage that plays again every 24 seconds round Centre (a world point on the ground), every size and kind:
	 * shells of each calibre, a bomb, a hit on armour, a skip, a tank gun and a machine gun firing tracer, a wreck
	 * burning. For pictures taken by Tools/take_pictures.sh. */
	void RunTest(const FVector& Centre);

private:
	FBattleFxRecipe Book;
	TheaterFx::Battle Core;
	TWeakObjectPtr<UProceduralMeshComponent> Mesh;
	TWeakObjectPtr<UMaterialInstanceDynamic> Ink;
	static constexpr int32 MostLamps = 4;
	TWeakObjectPtr<UPointLightComponent> LampLights[MostLamps];
	int32 LampsInUse = 2;
	double LampCandela = 400.0; // at full, already scaled for the world's units
	TFunction<FVector(const FVector&)> GroundUnder;
	TWeakObjectPtr<USceneComponent> Holder;
	double UnitsPerMetre = 100.0;
	double Clock = 0.0;
	FVector Sun = FVector(-0.45, -0.35, 0.82);
	int32 Room = 0; // puffs the mesh has room for (in steps of 300, as the map's smoke)
	int32 RoomSpare = 0; // frames running that less room would have done
	int32 LastSent = 0;
	bool bShown = false;
	TArray<FVector> Points;
	TArray<FColor> Colours;

	struct FTestShot
	{
		double When = 0.0;
		int32 Kind = 0;
		FVector Where = FVector::ZeroVector;
		FString Size;
		FString Ground;
	};
	TArray<FTestShot> TestShots;
	FVector TestCentre = FVector::ZeroVector;
	double TestStart = -1.0;

	static double GroundHeight(void* Context, double X, double Y);
	TheaterFx::Vec ToFx(const FVector& World) const;
	TheaterFx::Vec ToFxWay(const FVector& Way) const;
	double Time() const;
	void MakeSection(int32 NewRoom, const FVector& Fill);
	void Hide();
	void PlayTest(double Now);
};
