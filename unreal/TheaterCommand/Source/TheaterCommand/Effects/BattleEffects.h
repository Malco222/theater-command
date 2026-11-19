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
 * make a metre, and a way to find the ground under a point; tell it what happens (Shell, Muzzle, VehicleDust, Shots, AirGun, AirHit, the Trail calls, the AirFall calls, AirFlak, Burn); and
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
	 * "howitzer" or "mortar". Ground is what its blast throws up: on "sand" a tank or field gun's shot (a towed gun's
	 * `muzzle` socket too) raises a veil of dust that hides the gun and its line of sight for a second or two. */
	void Muzzle(const FVector& WorldAt, const FVector& Facing, const FString& Gun, const FString& Ground = TEXT("earth"));
	/** Dust behind a moving vehicle: every frame, for each of its dust sockets ("dust.left", "dust.right"), where the
	 * socket is and the vehicle's velocity (world units a second). Id: any number that stays the same for that socket
	 * (the vehicle's number times two, plus one for the right). Ground is the ground it drives on ("sand" throws the
	 * most; snow, mud and water none); bTracked for tanks and half-tracks, which throw more than wheels. Nothing when
	 * it stands still; low, long and lingering at speed, drifting on the wind. */
	void VehicleDust(int32 Id, const FVector& SocketAt, const FVector& Velocity, const FString& Ground, bool bTracked = true);
	/** Rounds fired by one gun (any number that stays the same for that gun) toward To: every fifth a tracer. Ground
	 * is what they strike, for the little dust spurts there ("earth" if left out). */
	void Shots(int32 Gun, const FVector& From, const FVector& To, const FString& Weapon, int32 Rounds, float PerSecond, const FString& Ground = TEXT("earth"));
	/** A gun on an aircraft firing: call it every frame while the trigger is down, for each gun (Gun: any number that
	 * stays the same for that gun). Now is this frame's time on the same clock Tick is given; an air scene with its
	 * own air time (slowed when the player slows it) gives its own FBattleEffects that time in both. Muzzle is where
	 * the gun is now and Aim the point it is laid on: the battle decides lead and convergence, so each gun gets its
	 * own aim point. ShooterVelocity is the aircraft's velocity in world units a second. Kind is "rifle", "hmg" or
	 * "cannon20" (the recipe's air_guns); RatePerMinute and MuzzleSpeed (world units a second) of 0 take the row's.
	 * The kit lays each round from where the muzzle was at that moment, so the stream stays on the gun as it turns;
	 * one round in five is the ruled tracer. */
	void AirGun(double Now, int32 Gun, const FVector& Muzzle, const FVector& Aim, const FVector& ShooterVelocity, const FString& Kind, float RatePerMinute = 0.0f, float MuzzleSpeed = 0.0f);
	/** That gun's trigger let go. */
	void AirGunStop(int32 Gun);
	/** Rounds striking an aircraft, on the same clock as AirGun: At the point hit, Normal the skin's outward way there,
	 * AircraftVelocity that aircraft's velocity (world units a second), Kind the hitting gun's row ("rifle", "hmg",
	 * "cannon20"). A hot spit with sparks, a scrap or two of skin left tumbling behind, a small dark puff; bigger for
	 * cannon. */
	void AirHit(double Now, const FVector& At, const FVector& Normal, const FVector& AircraftVelocity, const FString& Kind);
	/** A trail from a moving point on an aircraft, such as an engine socket: TrailStart once, TrailMove every frame
	 * with the point and its velocity (world units a second), TrailStop when it ends; what is in the air fades on
	 * its own. Id: a number that stays the same for that trail. Kind: "smoke" (thin and grey, thickening and
	 * darkening), "fire" (flame at the root, black smoke) or "glycol" (thin white vapour); TrailStart again on a
	 * running Id changes its kind (smoke catching fire). SpanMetres: the aircraft's wingspan in metres, so a fire's
	 * flame is sized to it (half the span, at most 10 m, by the recipe); 0 keeps the recipe's own flame_m. */
	void TrailStart(double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres = 0.0f);
	void TrailMove(double Now, int32 Id, const FVector& At, const FVector& Velocity);
	void TrailStop(int32 Id);
	/** An aircraft lost: what comes off it as it goes down. AirFall once when it is lost, AirFallMove every frame with
	 * where it is and its velocity (world units a second), AirFallStop when it is gone (hits the ground or is
	 * removed). Id: a number that stays the same for that aircraft. Kind: "explode" (blown apart: all its pieces at
	 * once round a short torn fireball; no AirFallMove needed), "burn" (going down on fire: 5 to 10 pieces shed over
	 * its fall) or "spin" (out of control: a few pieces, sometimes a wing). SpanMetres: its wingspan, so the pieces
	 * are sized to it (a bomber, 20 m or more, sheds 60 to 100 at once, a fighter 30 to 50). DeckZ: the world height
	 * where the pieces leave sight, the cloud deck's top, or the ground under the fight; nothing pops out of view.
	 * Light torn skin plates flutter and glint as they sink; a few heavy dark pieces drop straight. */
	void AirFall(double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres, double DeckZ);
	void AirFallMove(double Now, int32 Id, const FVector& At, const FVector& Velocity);
	void AirFallStop(int32 Id);
	/** A flak shell bursting at At, at the bombers' height, on the same clock as AirGun. Kind: an air_flak row
	 * ("heavy", the 88 mm class). A soft flash, then see-through lobes of smoke in a crooked chain that hang where
	 * they burst, drift with the wind, grey and stretch, and are gone in about 9 s. The battle decides where and how
	 * often (the art director's brief: within about two wingspans of the formation, more behind than ahead, every
	 * 2.5 s for light guns, 1.4 s medium, 0.9 s heavy, ragged). */
	void AirFlak(double Now, const FVector& At, const FString& Kind = TEXT("heavy"));
	/** Something burning, about AcrossMetres wide: flames for Seconds, then a thinning grey column. */
	void Burn(const FVector& WorldAt, float AcrossMetres, float Seconds);
	/** Oil burning on the sea where a ship went down (Kind "sunk"), or where a ship on fire left the line ("burning"):
	 * low flames on the water and a tall black column leaning down the wind (SetWind; one wind for every column),
	 * kept even when the screen is full and drawn kilometres off, so the far columns stay on the horizon. One call for
	 * each smoke zone of the sea battle (its id, x, y and r): ReachMetres is its r (0.6 of her length). UntilSeconds is
	 * when it stops, on Tick's clock (the fight's end; 0: until OilStop); SinceSeconds, how long it has already burned
	 * when the scene opens (the battle's second now minus the zone's at), so the column stands grown. A WorldAt above
	 * the sea (a burning ship's deck) puts the flames there. Calling again with the same Id starts it afresh. */
	void OilFire(int32 Id, const FVector& WorldAt, float ReachMetres, const FString& Kind = TEXT("sunk"), double UntilSeconds = 0.0, float SinceSeconds = 0.0f);
	/** That oil fire goes out: no new flame or smoke, and its column thins away on its own. */
	void OilStop(int32 Id);
	/** The battle's wind (metres a second, world axes): smoke leans and drifts with it. */
	void SetWind(const FVector& MetresPerSecond);
	/** For an air scene: draw out to the recipe's air.draw_beyond_m (4,000 m), so guns, hits and flames are seen from
	 * the strategy camera 800 to 2,000 m off the fight. Call once after it is made. */
	void UseAirDistance();
	/** The way toward the sun (world axes): each puff is lightened on that side, and looking toward it smoke's thin
	 * edges glow warm where the sun shows through (by day and dusk, as the hour's light says; none at night). */
	void SetSun(const FVector& TowardSun);
	/** Everything gone: a new battle. */
	void Clear();

	/** Once a frame, with the world's time in seconds (what happens between ticks is stamped with the last tick's time)
	 * and the eye. HourLight is what the hour does to smoke (white by day; the campaign map's night rule): flame,
	 * flashes and tracer keep their own colour. ViewHeight: the picture's height in the same pixels as ViewWidth, so
	 * what is off its top or bottom is known exactly; left at 0, the view is taken as a quarter taller than wide, which
	 * covers any window wider than it is tall. Returns the camera's jolt. */
	FBattleJolt Tick(double Now, const FVector& Eye, const FRotator& Facing, float FieldOfViewDegrees, int32 ViewWidth, const FLinearColor& HourLight = FLinearColor::White, int32 ViewHeight = 0);

	/** Camera shake on or off (Options: "Camera shake", default on). */
	bool bShake = true;

	/** -TheaterBlastTest on the command line: lay a test barrage round a place (RunTest) for test frames. */
	static bool WantsTest();
	/** A barrage that plays again every 24 seconds round Centre (a world point on the ground), every size and kind:
	 * shells of each calibre, a bomb, a hit on armour, a skip, a tank gun and a machine gun firing tracer, a wreck
	 * burning. GroundKind: the kit's ground under Centre (earth, sand, snow, mud, woods, town), which the tank gun's and
	 * the machine gun's dust take; the shells keep their own test grounds. For pictures taken by Tools/take_pictures.sh. */
	void RunTest(const FVector& Centre, const FString& GroundKind = TEXT("earth"));

private:
	FBattleFxRecipe Book;
	TheaterFx::Battle Core;
	TWeakObjectPtr<UProceduralMeshComponent> Mesh;
	TWeakObjectPtr<UMaterialInstanceDynamic> Ink;
	static constexpr int32 MostLamps = 4;
	TWeakObjectPtr<UPointLightComponent> LampLights[MostLamps];
	int32 LampsInUse = 2;
	double LampCandela = 400.0; // at full, already scaled for the world's units
	int32 LampsLitBefore = 0; // (-TheaterFxLampLog: how many were lit the frame before, so the first frame with none is logged)
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
	FString TestGround = TEXT("earth");
	double TestStart = -1.0;

	static double GroundHeight(void* Context, double X, double Y);
	static std::string Named(const FString& Text);
	TheaterFx::Vec ToFx(const FVector& World) const;
	TheaterFx::Vec ToFxWay(const FVector& Way) const;
	double Time() const;
	void MakeSection(int32 NewRoom, const FVector& Fill);
	void Hide();
	void PlayTest(double Now);
};
