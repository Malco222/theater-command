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
	/** The ground under a world point as the SCREEN shows it there (after its material layers, the sun and the grade):
	 * Out's red, green and blue as pixel / 255, not linearised; its alpha is not read. False where the game cannot
	 * say. Set it at any time; unset, or false, a heavy gun's dust on dry ground takes its ground row's own paint. The
	 * kit asks once for each such shot, at the muzzle, never each frame (recipe: muzzle.dry, picture.to_land). */
	TFunction<bool(const FVector& World, FLinearColor& Out)> GroundColour;

	/** A shell from the field battle: its calibre ("light", "mortar", "field", "howitzer", "heavy"), the ground it
	 * fell on ("earth", "sand", "snow", "mud", "town", "woods", "water") and whether it struck armour. */
	void Shell(const FVector& WorldAt, const FString& Calibre, const FString& Ground, bool bOnArmour = false);
	/** The same by the recipe's own size row ("grenade", "light", "mortar", "gun75", "how105", "how150", "bomb250",
	 * "bomb500"): bombs, grenades, anything that is not a gun's calibre. On "water" also "torpedo" (a torpedo's
	 * column against a ship's side) and "churn" (the white water round a ship going down: no flash and no jolt).
	 * With bOnArmour a hit draws its sparks on the plate and no column, on water too. */
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
	 * one round in five is a tracer. Nation is whose gun it is ("us", "gb", "de", "it", "jp", "su"; empty or one
	 * the recipe does not know takes the default): its tracers burn that nation's colours (the recipe's
	 * air_guns.tracers; the Soviet cannon's red is picked from Kind "cannon20" with "su"). */
	void AirGun(double Now, int32 Gun, const FVector& Muzzle, const FVector& Aim, const FVector& ShooterVelocity, const FString& Kind, float RatePerMinute = 0.0f, float MuzzleSpeed = 0.0f, const FString& Nation = FString());
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
	 * flame is sized to it (half the span, at most 10 m, by the recipe); 0 keeps the recipe's own flame_m.
	 * Two kinds for the ways down: "fuel" (a flamer: long torn tongues of fire streaming 1 to 3 aircraft lengths
	 * behind the point, then thick black smoke) and "mist" (white fuel spraying from a torn wing root; TrailStart
	 * "fuel" on the same Id when it catches). Look: the battle event's look, 0 to 3, which sets a flamer's length and
	 * how sooty its flame is; -1 the recipe's usual one. The kit throws no dice for a look. */
	void TrailStart(double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres = 0.0f, int32 Look = -1);
	void TrailMove(double Now, int32 Id, const FVector& At, const FVector& Velocity);
	void TrailStop(int32 Id);
	/** An aircraft lost: what comes off it as it goes down. AirFall once when it is lost, AirFallMove every frame with
	 * where it is and its velocity (world units a second), AirFallStop when it is gone (hits the ground or is
	 * removed). Id: a number that stays the same for that aircraft. Kind: "explode" (blown apart: all its pieces at
	 * once round a short torn fireball; no AirFallMove needed), "burn" (going down on fire: 5 to 10 pieces shed over
	 * its fall) or "spin" (out of control: a few pieces, sometimes a wing). SpanMetres: its wingspan, so the pieces
	 * are sized to it (a bomber, 20 m or more, sheds 60 to 100 at once, a fighter 30 to 50). DeckZ: the world height
	 * where the pieces leave sight, the cloud deck's top, or the ground under the fight; nothing pops out of view.
	 * Light torn skin plates flutter and glint as they sink; a few heavy dark pieces drop straight.
	 * The ways down (the battle event's how): "wing", "tail", "breakup", "struck" and "collide" throw a burst of small
	 * torn panels at At as they start (the wing root, the tail's joint, the point where two meet), with a sparkle of
	 * metal and, where two meet, sparks; "pilot" and "glide" throw nothing; "flamer" sheds as "burn" does. The big
	 * pieces (a wing, the tail, an engine) are the model's own. When a burning one blows up (the event's boom), call
	 * AirFall "explode" with the same Id. Look: the event's look, 0 to 3: an explosion's size, how long its black
	 * cloud hangs, how many pieces and how many streaks of burning fuel fall out of it; a burst's size; -1 the usual
	 * one. bBombs: bombs aboard, a bigger and whiter-hearted explosion that always throws fuel streaks. Far off or on
	 * a crowded screen a fall throws nothing, and an explosion keeps its flash and its black cloud. */
	void AirFall(double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres, double DeckZ, int32 Look = -1, bool bBombs = false);
	void AirFallMove(double Now, int32 Id, const FVector& At, const FVector& Velocity);
	void AirFallStop(int32 Id);
	/** A flak shell bursting at At, at the bombers' height, on the same clock as AirGun. Kind: an air_flak row
	 * ("heavy", the 88 mm class). A soft flash, then see-through lobes of smoke in a crooked chain that hang where
	 * they burst, drift with the wind, grey and stretch, and are gone in about 9 s. The battle decides where and how
	 * often (the art director's brief: within about two wingspans of the formation, more behind than ahead, every
	 * 2.5 s for light guns, 1.4 s medium, 0.9 s heavy, ragged). */
	void AirFlak(double Now, const FVector& At, const FString& Kind = TEXT("heavy"));
	/** Something burning, about AcrossMetres wide: flames for Seconds, then a thinning grey column. Id, 0 or more: the
	 * caller's own number for this fire, so BurnOut finds it (under 0: none). */
	void Burn(const FVector& WorldAt, float AcrossMetres, float Seconds, int32 Id = -1);
	/** The crew puts that fire out over OverSeconds (0: the recipe's 6 to 12 s): its flames are eaten back from the
	 * edges to the seat, shrinking steadily and never all gone and back; a dull glow and small flickering embers
	 * stay 10 to 20 s; its smoke pales from black through grey to grey-white and is gone inside about a minute. How "water" (a ship's hoses: a surge of white steam at the
	 * seat for the first seconds) or "extinguisher" (a tank's crew: a brief pale puff). A fire whose flames are
	 * already over, or one with no such Id, is left alone. The scorch that stays is the model's to show. */
	void BurnOut(int32 Id, float OverSeconds = 0.0f, const FString& How = TEXT("water"));
	/** That fire is on something under way (a ship steaming on after the hit): it is now at WorldAt, going at
	 * Velocity (world units a second). Call it every frame while she moves, and once more with no velocity when she
	 * stops. The flames, the glow and the embers stay on her and lean by the wind over her deck; the smoke is left
	 * to the air and streams astern. A fire with no such Id, or a place that is no number, is left alone. */
	void BurnMove(int32 Id, const FVector& WorldAt, const FVector& Velocity);
	/** Steam let go from a hit boiler or engine room, at WorldAt along Way, from an opening AcrossMetres wide, for
	 * Seconds: a narrow white jet with a clear gap at its mouth, breaking into rags that rise fast and vanish, its
	 * length and speed falling together to a wavering wisp. Never a bank, never a flash. Kind "vent" (a deck vent,
	 * a skylight, a hole in the side: 5 to 15 m; a tank's engine deck far shorter by its width) or "funnel" (the
	 * escape pipes on a funnel's face: a tall plume straight up, 30 to 60 m). Id: any number that stays the same for
	 * that leak; calling again with it starts the leak afresh. */
	void Steam(int32 Id, const FVector& WorldAt, const FVector& Way, float AcrossMetres, float Seconds, const FString& Kind = TEXT("vent"));
	/** That leak stops now; what is in the air evaporates on its own. */
	void SteamStop(int32 Id);
	/** That leak is on something under way: its opening is now at WorldAt, facing Way, going at Velocity (world units
	 * a second). Every frame while she moves, as BurnMove. The jet leaves the opening and bends astern. */
	void SteamMove(int32 Id, const FVector& WorldAt, const FVector& Way, const FVector& Velocity);
	/** Ammunition cooking off at WorldAt (a tank's hatch, a ship's gun mount, aircraft parked on a deck), Up the way
	 * out of it, the opening AcrossMetres wide: Pops small pops over OverSeconds, each a small flash, a grey-black
	 * puff, embers in short arcs and now and then a tumbling fragment (never a glowing round going skyward); and
	 * flame roaring from the hatch for JetSeconds (0: none), 3 to 6 m tall from a tank's hatch, in one or two
	 * pulses, with black smoke off its top. JetFromSeconds says when the jet starts after the call (under 0: just
	 * after the last pop), so it may come first, between the pops or last. The burning wreck after is Burn's. Id, 0
	 * or more: the caller's own number for it, so CookOffMove finds it (under 0: none; it stays where it was laid). */
	void CookOff(const FVector& WorldAt, const FVector& Up, float AcrossMetres, int32 Pops, float OverSeconds, float JetSeconds, float JetFromSeconds = -1.0f, int32 Id = -1);
	/** That cook-off (the Id given to CookOff, 0 or more) is on something under way: its opening is now at WorldAt,
	 * facing Up, going at Velocity (world units a second). Every frame while she moves, as BurnMove: the pops and
	 * the jet stay at the hatch. */
	void CookOffMove(int32 Id, const FVector& WorldAt, const FVector& Up, const FVector& Velocity);
	/** An armour-piercing shot glancing off armour at WorldAt: Incoming the way it was flying, Normal the plate's
	 * outward way, CalibreMm its calibre (37 to 88 for tank and anti-tank guns; 203 for a cruiser's shell off a
	 * battleship's belt). A small two-frame flash, sparks thrown along the glance, a tiny grey puff, and the shot
	 * going off glowing at a shallow angle, slowing, veering and wobbling as it tumbles; where it comes down in
	 * sight it kicks up a little of Ground's dust. The bright gouge it leaves is the model's to show. */
	void Glance(const FVector& WorldAt, const FVector& Incoming, const FVector& Normal, float CalibreMm = 75.0f, const FString& Ground = TEXT("earth"));
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
	/** For a sea battle: draw out to the recipe's sea.draw_beyond_m (16 km), so the far line's gun flashes, shell
	 * columns and fires are seen 5 to 15 km off, and held to the recipe's sea floors there (a far gun flash a bright
	 * point with a little glow, a far column a small white pillar). Call once after it is made, in place of
	 * UseAirDistance. Without a sea row in the recipe it is UseAirDistance. */
	void UseSeaDistance();
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
	static bool GroundColourAt(void* Context, double X, double Y, double& Red, double& Green, double& Blue);
	static std::string Named(const FString& Text);
	TheaterFx::Vec ToFx(const FVector& World) const;
	TheaterFx::Vec ToFxWay(const FVector& Way) const;
	double Time() const;
	void MakeSection(int32 NewRoom, const FVector& Fill);
	void Hide();
	void PlayTest(double Now);
};
