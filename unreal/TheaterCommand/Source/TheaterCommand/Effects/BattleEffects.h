#pragma once

#include "CoreMinimal.h"
#include "Effects/TheaterFxCore.h"
#include "Tasks/Task.h"
#include "Templates/Function.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <functional>
#include <map>
#include <string>

class AActor;
class FJsonValue;
struct FPostProcessSettings;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProceduralMeshComponent;
class USceneComponent;

/** One point of a holed ship's oil lane (release 2, item 10: FBattleEffects::OilLane), oldest first, in world units:
 * Mid its middle on the sea, LeftEdge and RightEdge its two edges (ragged), Dark how dark the oil is (0 none to 1, the
 * scene's slick paint's darkness), Sheen a faint sheen's share, AgeSeconds since it left her. */
struct FOilLanePoint
{
	FVector Mid = FVector::ZeroVector, LeftEdge = FVector::ZeroVector, RightEdge = FVector::ZeroVector;
	float Dark = 0.0f, Sheen = 0.0f, AgeSeconds = 0.0f;
};

/** How the scene paints one point of an oil lane (FBattleEffects::OilLook): Widen, how many times wider than its strip
 * to lay it (1 when its dark middle is 1.5 px or more on the screen); Mid and Shoulder, its alpha at the middle and at
 * 0.55 of the way out (its edges clear); Calm, 0 to 1, how far its paint goes over to the scene's calm paint (a pale
 * reflected sky) when seen low; CalmAlpha, that paint's own alpha. */
struct FOilLaneLook
{
	float Widen = 1.0f, Mid = 0.0f, Shoulder = 0.0f, Calm = 0.0f, CalmAlpha = 0.0f;
};

/** The lane paint rule's numbers from the recipe (FBattleEffects::OilRule), read once a frame. */
struct FOilLaneRule
{
	TheaterFx::Battle::LaneRule Rule;
};

/** The camera's jolt from hits close by, for the caller to add to its own camera if it wants it: it is never moved
 * here. Offset is in world units. A setting turns it off (FBattleEffects::bShake). */
struct FBattleJolt
{
	FVector Offset = FVector::ZeroVector;
	float RollDegrees = 0.0f;
};

/** The recipe, Content/Effects/battle-fx.json, read for the effects (paths "sizes.how105.crown_m.0"). A launch line
 * -TheaterFxSet=game.from_above.on=1,grounds.rock.on=1 sets those numbers over the file's at each Load (for the Mac's
 * switch off/on runs, with no file changed and nothing rebuilt); each one set is logged. */
class FBattleFxRecipe final : public TheaterFx::Recipe
{
public:
	/** Over: further recipe files (full paths) whose numbers are laid over the file's, in order, before the launch
	 * line's (a scene's own look behind a switch: the land battle's -TheaterLandFilmFire lays battle-fx-film.json). */
	bool Load(const FString& Path, const TArray<FString>& Over = TArray<FString>());
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
 * make a metre, and a way to find the ground under a point; tell it what happens (Shell, Muzzle, VehicleDust, Shots, AirGun, AirHit, the Trail calls, the AirFall calls, AirFlak, BombStick, Burn, FuelBall); and
 * tick it once a frame with the eye. It draws everything as soft puffs in one procedural mesh with the campaign map's
 * own M_Highlight (see-through, unlit, coloured by its points), the same shape of puff as the map's smoke, so no new
 * material is needed; flashes light the ground with at most two point lamps (game.max_lamps), shadowless (a scene may
 * ask for up to four, handed out its own way: SetLampRule).
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
	/** A further recipe file in Content/Effects laid over battle-fx.json at Begin (call it before Begin): its numbers
	 * replace the file's for this kit alone (release 2: the land battle's film fire). */
	void LayOver(const FString& FileName) { Over.Add(FileName); }
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
	 * how sooty its flame is; -1 the recipe's usual one. The kit throws no dice for a look.
	 * A ship's funnel smoke is the kind "funnel": one trail a funnel, At the funnel's top, Velocity the ship's,
	 * SpanMetres the funnel's width at its top (about 3 m on a destroyer, 4 to 6 on a cruiser, 6 to 10 on a
	 * battleship or a carrier). Call TrailMove every frame, also when she lies stopped (velocity zero: she smokes on,
	 * straight up), with Power the share of her top speed she is making, 0 to 1: cruising haze at 0.4 and under,
	 * full power's dark smoke from 0.75, and a black surge for some seconds when it rises fast (she rings up).
	 * Power under 0: not given, and her speed over the recipe's top_speed_mps stands in. The smoke keeps none of
	 * her speed: it drifts with the kit's wind and climbs. */
	void TrailStart(double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres = 0.0f, int32 Look = -1);
	void TrailMove(double Now, int32 Id, const FVector& At, const FVector& Velocity, float Power = -1.0f);
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
	/** A stick of bombs, on the same clock as AirGun: Count bombs (1 to 48) of Size (a sizes row, "bomb250",
	 * "bomb500") landing evenly along the ground from First to Last on Ground ("earth", "town", "water", ...), the
	 * first at Now and the last OverSeconds later (under 0: the line at the bombers' speed, far.bombs.walk_mps, at most
	 * 4 s). Each bomb lands at its own moment, every one of them seen. Within the draw range it is the ordinary
	 * burst; beyond it, with the recipe's far row, the far form: a ripple of flashes along the line and the ground's
	 * own dust and smoke standing and spreading into one pall, drawn out to far.draw_beyond_m (20 km) and fading as
	 * the near cloud does. Id: the stick's own number (one stick per number while its bombs land). */
	void BombStick(double Now, int32 Id, const FVector& First, const FVector& Last, int32 Count, const FString& Size, const FString& Ground, float OverSeconds = -1.0f);
	/** Something burning, about AcrossMetres wide: flames for Seconds, then a thinning grey column. Id, 0 or more: the
	 * caller's own number for this fire, so BurnOut finds it (under 0: none). bOnShip: true for a fire on a ship's
	 * deck (anything afloat), so it burns as a ship's fire, never as a burning tank's, however small, low or still. */
	void Burn(const FVector& WorldAt, float AcrossMetres, float Seconds, int32 Id = -1, bool bOnShip = false);
	/** A burning opening (release 2): flame and smoke forced out of a hatch or an engine grille AcrossMetres wide, for
	 * Seconds, then a thinning grey column. A fire as Burn's (BurnOut and BurnMove find it by Id), but it never joins
	 * another fire nor founds a column of its own, and its flames are the recipe's fire.vent_tongues: narrow and tall
	 * out of the hole. A knocked-out tank's hatches so burn as hatches, never melted into one bonfire on her deck. */
	void Vent(const FVector& WorldAt, float AcrossMetres, float Seconds, int32 Id = -1);
	/** Fuel going up: a ragged ball of burning fuel about AcrossMetres wide that rolls and rises and turns to black
	 * smoke boiling off its top, lighting the ground while it burns. A vehicle's fuel 6 to 10 m (about 1 to 1.5 s),
	 * an aircraft's crash or a fuel dump 15 to 25 m (1.5 to 2 s). Velocity: what it was moving at (a crashing
	 * aeroplane's, a truck's), of which the fire keeps a little. Call it at the moment the fuel goes; a fire that
	 * burns on after it is Burn's, called with it. */
	void FuelBall(const FVector& WorldAt, float AcrossMetres, const FVector& Velocity = FVector::ZeroVector);
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
	/** A smoke round landing at WorldAt: its burst and the white wall that drifts from it down the wind, AcrossMetres
	 * wide at its fullest and laid for Seconds (on the kit's life clock). Kind "wp" (white phosphorus: a hard white
	 * burst of burning streamers), "canister" (a burning bomb's or a base-ejection shell's: a gush from the ground) or
	 * "grenade" (a tank commander's smoke grenade: one canister, swelling, drawn down the wind into a low plume).
	 * No crater. Id: the caller's own number for it, or under 0 none. */
	void SmokeScreen(const FVector& WorldAt, float AcrossMetres, float Seconds, const FString& Kind = TEXT("wp"), int32 Id = -1);
	/** A smoke screen laid at sea by a ship under way (the sea battle's escorts; not SmokeScreen above, a smoke round
	 * on land). Call it every frame while she lays it: Id, her own number for this screen; WorldAt, her stern;
	 * Velocity, hers (world units a second). bFunnel: black smoke from her funnels, else white smoke from her stern
	 * generators. It keeps none of her speed: dense at her stern, low on the water, drifting and spreading down the
	 * wind (SetWind) and thinning over minutes, with gaps in its old parts. The recipe's sea_screen row; without it,
	 * nothing. */
	void ScreenLay(int32 Id, const FVector& WorldAt, const FVector& Velocity, bool bFunnel = false);
	/** She stops laying that screen: what is laid hangs and thins away on its own. ScreenLay again under the Id
	 * starts a new one. */
	void ScreenLayStop(int32 Id);
	/** A shell falling in the sea by its calibre in millimetres (a 16-inch shell 406): a column of white water as
	 * tall and as slow as the calibre throws it, standing seconds before it falls, coloured by Dye, the firing ship's
	 * dye name from the recipe's sea_splash.dyes ("pink" Nagato, "red" Kongo, "yellow" Haruna, ...; empty for none,
	 * Yamato), and a stained foam patch. Without the sea_splash row: ShellOfSize on "water" as before. */
	void SeaSplash(const FVector& WorldAt, float CalibreMm, const FString& Dye = FString());
	// ---- Release 2, the ten dramatic moments: the sea's items 9, 10 and 11 (TheaterFx::Battle's, TheaterFxSea.inl). No
	// scene calls them until the sea scene wires each behind its own switch; uncalled, every frame is as before.
	/** A shell's column close off a ship's side whose head falls across her deck (item 9): ColumnAt where the shell fell
	 * (world), CalibreMm as SeaSplash's, Toward from the column to her nearest deck edge, level (world units; its length
	 * is how far off her side it stands), DeckZ her deck's world height there (world units), ShipId hers, BeamMetres her
	 * beam (metres; 0: her ShipRide pose's, if she is ridden). Call it with the column's SeaSplash, and ShipRide her
	 * first, so its sheets lie in her own frame and go with her. Nothing beyond sea_splash.drench.reach_k of the
	 * column's height, or with two drenches already falling on her; nothing lands past her far edge. */
	void DeckDrench(const FVector& ColumnAt, float CalibreMm, const FVector& Toward, float DeckZ, int32 ShipId = -1, float BeamMetres = 0.0f);
	/** Ship ShipId lies so this frame (once a frame for each ship that carries anything in her own frame: DeckWash's
	 * water, the deck's brass; before Tick). Hull: her frame in the world, its X toward her bow, Z up through her
	 * deck, its origin on her waterline amidships (her hull as drawn, turned, heeled and trimmed; its scale is not
	 * read); as Unreal turns a frame its Y then points to her STARBOARD side, and the kit takes her port side as minus Y
	 * (what DeckWash's edge calls Y is to port). LongMetres her length, HalfBeamMetres half her beam, CoamingMetres what stands at her deck's
	 * edge (a bulwark; 0 for a rail with open sides), DeckMetres her deck's height over her waterline amidships (her
	 * freeboard: the deck's brass lies on it; 0, not known). A ship not told of for 5 s is let go, and all she carries
	 * fades out over a second where it was. */
	void ShipRide(int32 ShipId, const FTransform& Hull, float LongMetres, float HalfBeamMetres, float CoamingMetres = 0.0f, float DeckMetres = 0.0f);
	/** The plan's name for ShipRide (item 11, the deck's brass): the same call. */
	void CasesRide(int32 ShipId, const FTransform& Hull, float LongMetres, float HalfBeamMetres, float CoamingMetres = 0.0f, float DeckMetres = 0.0f) { ShipRide(ShipId, Hull, LongMetres, HalfBeamMetres, CoamingMetres, DeckMetres); }
	/** She is gone (sunk, or out of the scene): her wash stops, and what she carries fades out over a second where it is. */
	void ShipRideStop(int32 ShipId);
	/** Water on her deck running off her edge (item 9's runoff after a drench, item 10's awash on a ship listing far
	 * over), in her own frame and moved with her (ShipRide first). EdgeMetres: points along the edge it runs over, in
	 * METRES in her frame (X toward her bow, Y to port, Z up from her waterline: Z is her freeboard there). Strength 0 to
	 * 1 scales how much; Seconds how long it runs; Kind "runoff" or "awash". The recipe's sea_splash.runoff row. */
	void DeckWash(int32 ShipId, const TArray<FVector>& EdgeMetres, float Strength, float Seconds, const FString& Kind = TEXT("runoff"));
	/** Her wash stops: what is running is gone in a second or two. */
	void DeckWashStop(int32 ShipId);
	/** A holed ship leaking oil as she steams on (item 10): call it every frame while she leaks, WorldAt where the oil
	 * leaves her, Rate 0 to 1. The kit keeps her track (sea_leak: a point every 2 s, at most 200 a ship, 6 ships) and
	 * draws nothing; the scene lays OilLane's strip with its own slick paint. Oil, never fire: a trail never burns. */
	void OilLeak(int32 ShipId, const FVector& WorldAt, float Rate = 1.0f);
	/** She stops leaking (sunk, or the leak stopped): her lane stays, widening and fading, until it is gone. */
	void OilLeakStop(int32 ShipId);
	/** Her lane now, oldest first, into Out (world units): how many points. Reads only the lanes, which the kit's draw on
	 * its worker never touches, so it may be asked any time on the game thread. */
	int32 OilLane(int32 ShipId, TArray<FOilLanePoint>& Out) const;
	/** One lane point's paint by the kit's rule (sea_leak.floor_px, floor_alpha, calm_from_deg, calm_full_deg,
	 * calm_alpha, calm_far_deg, calm_far_pow, calm_far_most, dark): from its Dark, its dark middle's width on the screen across the lane (pixels) and the eye's grazing
	 * angle to the sea there (degrees). Reads the recipe only; may be asked any time on the game thread. */
	FOilLaneLook OilLook(float Dark, double MidPixels, double GrazeDegrees) const;
	/** The same with the rule read once (OilRule, once a frame): what a strip of 1,200 points should use. CalmShare (0 to
	 * 1, 1 by default: today's numbers bit for bit) is how much of the calm band the light there allows: 0 on a night
	 * sea (no band: the lane a dark film), the scene's day share for its unlit inks at dusk; Calm, Mid and Shoulder follow. */
	FOilLaneRule OilRule() const;
	static FOilLaneLook OilLook(const FOilLaneRule& Rule, float Dark, double MidPixels, double GrazeDegrees, double CalmShare = 1.0);
	/** The ships with a lane on the sea now. */
	void OilLanes(TArray<int32>& Ships) const;
	/** Item 11 part 4, the deck's brass: Count (1 to 16) spent cases of Kind (deck_cases.<kind>: aa25, aa40, in4, in5,
	 * cm127, hmg) thrown from Breech (world) along Away (world, rough) onto ship ShipId's deck (DeckMetres of her ShipRide),
	 * lying and rolling in her frame (ShipRide once a frame first). Laid within 90 m of the eye, drawn within 40 to 60 m.
	 * Nothing without the row or her pose. */
	void DeckBrass(int32 ShipId, const FVector& Breech, const FVector& Away, const FString& Kind, int32 Count = 1);
	/** A ship's light gun stream (AirGun's Gun) is on ship ShipId (-1: none): while she is ridden its aircraft-style
	 * cases stay off, and each round it fires throws its case on her deck (DeckBrass) when deck_cases has its kind. */
	void AirGunShip(int32 Gun, int32 ShipId);
	/** The battle's wind (metres a second, world axes): smoke leans and drifts with it. */
	void SetWind(const FVector& MetresPerSecond);
	/** The sky or haze behind the battle's smoke as the SCREEN shows it at this hour (as GroundColour's: red, green and
	 * blue as pixel / 255, not linearised; alpha is not read): the horizon's haze for a ground or sea battle, the sky at
	 * the fight's height for an air one. Old smoke pales toward a shade just under it as it thins, so it goes into the
	 * air. Call it once the scene's sky is set, and again when the hour or the weather changes it. Not called: the
	 * recipe's own grey (linger.to) by the hour's light. */
	void SetSky(const FLinearColor& OnScreen);
	/** For an air scene: draw out to the recipe's air.draw_beyond_m (4,000 m), so guns, hits and flames are seen from
	 * the strategy camera 800 to 2,000 m off the fight. Call once after it is made. */
	void UseAirDistance();
	/** For a sea battle: draw out to the recipe's sea.draw_beyond_m (16 km), so the far line's gun flashes, shell
	 * columns and fires are seen 5 to 15 km off, and held to the recipe's sea floors there (a far gun flash a bright
	 * point with a little glow, a far column a small white pillar). Call once after it is made, in place of
	 * UseAirDistance. Without a sea row in the recipe it is UseAirDistance. */
	void UseSeaDistance();
	/** For a scene whose camera stands among the bursts (the land battle; the air and sea scenes never call it and keep
	 * what they had). A puff is laid flat to the lens, so smoke whose middle is level with the camera a few metres to
	 * the side was laid across the whole picture as a huge dark disc. With this, smoke and dust thin out as their near
	 * side comes within WholeMetres in front of the lens and are gone within GoneMetres, and nothing at all is laid
	 * whose middle is behind the lens or within PlaneMetres in front of it (TheaterFx::Battle::SetLensFade). Call once
	 * after Begin; 0, 0 turns it off. */
	void SetLensFade(float GoneMetres, float WholeMetres, float PlaneMetres = 0.3f);
	/** Whether a shell leaves the kit's own flat painted crater (on as made). A scene that lays craters of its own in
	 * the ground (the land battle's torn earth) turns it off after Begin; nothing else of a shell changes. */
	void SetCraters(bool bOn);
	/** For a scene that grades its picture as the land battle does: the kit's colours written for that grade where the
	 * recipe has them (land_grade: the clods earth brown with their form, never black silhouettes; the yellow tracer
	 * yellow, not khaki), as TheaterFx::Battle::SetLandGrade. Call once after Begin; off as made. */
	void SetLandGrade(bool bOn);
	/** For a scene seen from high above the ground (the land battle, for its general's eye 300 m up): as the eye climbs
	 * and looks down steeply, smoke's face toward the sun is lit and its big puffs lay their shadows on the ground away
	 * from the sun, by the recipe's game.from_above row (the art director's read of the general's stills, item 5). A low
	 * eye sees nothing changed. As TheaterFx::Battle::SetFromAbove; call once after Begin; off as made. */
	void SetFromAbove(bool bOn);
	/** The way toward the sun (world axes): each puff is lightened on that side, and looking toward it smoke's thin
	 * edges glow warm where the sun shows through (by day and dusk, as the hour's light says; none at night). */
	void SetSun(const FVector& TowardSun);
	/** Fire keeps its orange under the scene's colour grade (the art director, 22 November: the land battle's grade,
	 * made for its fields, drained a burning wreck's flame to salmon and peach). The kit's mesh marks its pixels in the
	 * custom depth, and M_FireSpread, drawn after the tonemapper, spreads the colour of only those marked pixels that
	 * are fire's colour (orange to yellow, deep or bright, never grey; a red tracer's too) or a bright green light (a
	 * Soviet tracer) by Spread from their own grey, and raises the green of fire's orange by Warm. A white-hot core
	 * stays as ruled, and so does the sky's blue. Smoke, dust and earth stay as the grade leaves them, and so does the field
	 * behind a thin flame.
	 * Call it after Begin with the lens the scene's eye looks through (its camera's or post-process volume's settings:
	 * one the engine keeps, as the lens holds the material); again to change the numbers; Spread 1 or less takes it
	 * off. One lens at a time: it is taken out only of the lens it is given, so take it off one (Spread 1) before
	 * putting it on another. Costs the custom depth pass for the kit's mesh. False, with a log line, without M_BattlePuffs and
	 * M_FireSpread (Tools/build_content.sh with TC_REBUILD=M_BattlePuffs,M_FireSpread). */
	bool SetFireSpread(FPostProcessSettings& Lens, float Spread = 2.0f, float Warm = 1.2f);
	/** Soft contact, for the land battle (the art director's cut 5, still 07: a burning wreck's flame was cut along a
	 * hard straight line where it met the hull; ruling 14:16Z 22 November, 0.1 m). The kit's mesh takes
	 * M_BattlePuffsSoft, which fades every piece where it comes within Metres in front of the ground or a hull, and the
	 * kit lays its lying pieces (a clod or fragment come to rest, a spent case, a crater's mark) nearer the eye and
	 * smaller to match, the same picture, so they stay whole (TheaterFx::Battle::SetSoftContact). Call it after Begin,
	 * on this thread; 0 takes it off. Costs one depth read for each of the kit's pixels; the scenes that never call it
	 * pay nothing. False, with a log line, without M_BattlePuffs and M_BattlePuffsSoft (Tools/build_content.sh with
	 * TC_REBUILD=M_BattlePuffsSoft), and the kit draws as before. */
	bool SetSoftContact(float Metres = 0.1f);
	/** How the flashes' lamps are handed out, for a scene that wants its own (the sea's night fight, 25 November: look
	 * bible S7, "gun flashes light the hulls"). Call once at open, before Begin or after it, on this thread. Without it
	 * (ground, air) nothing changes: game.max_lamps lamps, the strongest flashes first, and no point light more is made.
	 * Rule.Most lamps are made (0: game.max_lamps; never over MostLamps, 4), shadowless as ever and, under a rule, with
	 * no Lumen bounce and no volumetric fog (a flash lit for a tenth of a second is more than Lumen's surface cache can
	 * follow); the kit ranks them by the light each sends the eye (bByEye: the eye and view Tick is given; a lamp that
	 * can light nothing on the screen ranks nothing), lights a salvo's flashes as one lamp (MergeM, MergeS, MergeMost)
	 * and lets a steady fire's own lamp go to a flash left unlit that outshines it Yield times: see
	 * TheaterFx::Battle::LampRule. Kept until End.
	 *
	 * The sea battle's calls (the sea scene's lane wires them; nothing here knows the sea):
	 *   - once at open, beside Effects->SetSun(...):
	 *         Effects->SetLampRule(TheaterFx::Battle::LampRule::Sea()); // 3 lamps, by the eye, a salvo within 200 m and 0.3 s one lamp
	 *   - each turret (or mount) that fires, its row by calibre, as any gun:
	 *         Effects->Muzzle(MuzzleAt, Facing, CalibreMm >= 280.0 ? TEXT("naval_heavy") : CalibreMm >= 150.0 ? TEXT("naval_medium") : TEXT("naval_light"), TEXT("water"));
	 *     (a light cruiser whose main guns are 127 to 149 mm, hull 139 to 163 m, may take naval_medium for its reach)
	 *   - every frame, as today, Effects->Tick(Now, Eye, Facing, Fov, ViewW, HourLight, ViewH): with FLinearColor::White
	 *     (today) it is full day to the kit, so every naval row lights at game.day_lamp_share (a quarter: about 0.4 lux
	 *     on her side); with the hour's own light (step 11) at night every row lights at full (about 1.6 lux by the gun).
	 * Rows and numbers: Content/Effects/battle-fx.json muzzle._naval, and the sea's effects-lamps-for-sea.md. */
	void SetLampRule(const TheaterFx::Battle::LampRule& Rule);
	/** Everything gone: a new battle. */
	void Clear();

	/** Once a frame, with the world's time in seconds (what happens between ticks is stamped with the last tick's time)
	 * and the eye. HourLight is what the hour does to smoke (white by day; the campaign map's night rule): flame,
	 * flashes and tracer keep their own colour. ViewHeight: the picture's height in the same pixels as ViewWidth, so
	 * what is off its top or bottom is known exactly; left at 0, the view is taken as a quarter taller than wide, which
	 * covers any window wider than it is tall. Returns the camera's jolt. */
	FBattleJolt Tick(double Now, const FVector& Eye, const FRotator& Facing, float FieldOfViewDegrees, int32 ViewWidth, const FLinearColor& HourLight = FLinearColor::White, int32 ViewHeight = 0);

	/** Tick in two halves (the production lead's FX2), so the kit's own frame runs on a worker while the scene draws its
	 * men and tanks. TickBegin takes what Tick takes and does on this thread what must be done here (the test barrage,
	 * the camera's jolt, the view), starts the kit's Draw on a worker and returns the jolt at once. TickEnd waits for
	 * the worker, sends the mesh and lights the lamps, then does what was asked of the kit in between, in the order
	 * asked. Between the two:
	 * - every call on the kit is held, its arguments copied, and done at TickEnd, stamped with this frame's time as a
	 *   call after a Tick is: so it is first seen at the next frame. Call TickBegin after the frame's shells and fires
	 *   where you can, so only what comes after (a vehicle's dust) waits;
	 * - GroundUnder and GroundColour are called from the worker: nothing may change what they read (the field's
	 *   heights, a crater, a rut) until TickEnd;
	 * - PuffsDrawn and PuffsAsked give the last frame's; Clear and End wait for the worker first.
	 * Tick is both halves at once, all on this thread, as before. A TickBegin left without its TickEnd is ended by the
	 * next TickBegin or Tick. */
	FBattleJolt TickBegin(double Now, const FVector& Eye, const FRotator& Facing, float FieldOfViewDegrees, int32 ViewWidth, const FLinearColor& HourLight = FLinearColor::White, int32 ViewHeight = 0);
	void TickEnd();

	/** Camera shake on or off (Options: "Camera shake", default on). */
	bool bShake = true;

	/** For a scene's log line: the puffs the kit drew at the last Tick (or TickEnd), and the room asked for then (what was
	 * on the screen and what had been made since; over the room, 3,000, when it is short). Both by the kit's clock.
	 * -TheaterFxRoomLog on the command line also writes a line of its own every 10 s of that clock. */
	int32 PuffsDrawn() const;
	int32 PuffsAsked() const;

	/** -TheaterBlastTest on the command line: lay a test barrage round a place (RunTest) for test frames. */
	static bool WantsTest();
	/** A barrage that plays again every 24 seconds round Centre (a world point on the ground), every size and kind:
	 * shells of each calibre, a bomb, a hit on armour, a skip, a tank gun and a machine gun firing tracer, a wreck
	 * burning. GroundKind: the kit's ground under Centre (earth, sand, snow, mud, woods, town), which the tank gun's and
	 * the machine gun's dust take; the shells keep their own test grounds. For pictures taken by Tools/take_pictures.sh. */
	void RunTest(const FVector& Centre, const FString& GroundKind = TEXT("earth"));

private:
	FBattleFxRecipe Book;
	TArray<FString> Over; // (LayOver's files, by name in Content/Effects)
	TheaterFx::Battle Core;
	TWeakObjectPtr<UProceduralMeshComponent> Mesh;
	TWeakObjectPtr<UMaterialInstanceDynamic> Ink;
	bool bOwnPuffs = false; // (M_BattlePuffs found at Begin: only it marks the kit's pixels for SetFireSpread)
	TWeakObjectPtr<UMaterialInstanceDynamic> SpreadInk; // (M_FireSpread as last put in a lens, so it is taken out again)
	float InkBrightness = 1.0f; // (the Brightness Begin gave Ink: picture.lit_gain)
	bool bSoftInk = false; // (Ink is M_BattlePuffsSoft's: SetSoftContact)
	static constexpr int32 MostLamps = TheaterFx::Battle::MostLamps;
	TWeakObjectPtr<UPointLightComponent> LampLights[MostLamps];
	int32 LampsInUse = 2;
	TheaterFx::Battle::LampRule AskedRule; // (SetLampRule's, kept for Begin; the default until a scene asks)
	UPointLightComponent* MakeLamp(AActor* Owner, USceneComponent* Parent);
	int32 LampsAsked() const; // (the rule's Most, else game.max_lamps; never over MostLamps)
	void LampsByRule(); // (under a scene's rule, its lamps light directly only: no Lumen bounce, no fog)
	double LampCandela = 400.0; // at full, already scaled for the world's units
	int32 LampsLitBefore = 0; // (-TheaterFxLampLog: how many were lit the frame before, so the first frame with none is logged)
	TFunction<FVector(const FVector&)> GroundUnder;
	TWeakObjectPtr<USceneComponent> Holder;
	double UnitsPerMetre = 100.0;
	double Clock = 0.0;
	FVector Sun = FVector(-0.45, -0.35, 0.82);
	// (the mesh, the production lead's FX1 of 22 November: Sections sections of PuffsASection puffs each, made once at
	// the first frame with something to draw and never rebuilt; a section is sent only while it holds puffs, and the
	// rest are folded flat and hidden)
	static constexpr int32 PuffsASection = 300;
	int32 Sections = 0;
	int32 SectionsUsed = 0; // sections holding puffs at the last Tick (shown)
	FVector FoldedAt = FVector::ZeroVector; // where the sections holding no puffs were last folded (the pre-check on FX1: their boxes count in the mesh's bounds)
	bool bShown = false;
	TArray<FVector> Points; // one section's points and colours, filled for each section in turn
	TArray<FColor> Colours;
	// (-TheaterFxRoomLog: the most drawn in the 10 s of the kit's clock, when, and the room asked then)
	int32 LogMost = -1, LogAsked = 0;
	double LogMostAt = 0.0, LogFrom = -1.0;
	int32 LastDrawn = 0, LastAsked = 0; // (PuffsDrawn, PuffsAsked)
	// (the production lead's FX2: the kit's Draw on a worker from TickBegin to TickEnd, the frame it draws, and what is
	// asked of the kit meanwhile, held in order for TickEnd)
	UE::Tasks::TTask<int32> Drawing;
	bool bDrawing = false;
	double DrawNow = 0.0, DrawDay = 1.0;
	FVector DrawEye = FVector::ZeroVector;
	TArray<TUniqueFunction<void()>> Held;

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
	void MakeSections(const FVector& Fill);
	void RoomLog(double Now, int32 Laid);
	/** Before the kit's Draw, on this thread: the test barrage, the jolt, the view and the day. False: nothing to draw. */
	bool Ahead(double Now, const FVector& Eye, const FRotator& Facing, float FieldOfViewDegrees, int32 ViewWidth, const FLinearColor& HourLight, int32 ViewHeight, FBattleJolt& Jolt, TheaterFx::Battle::View& Sight, double& Day);
	/** After it, on this thread: the puffs it laid into the mesh, the room log and the lamps. */
	void Lay(double Now, const FVector& Eye, int32 Drawn, double Day);
	/** A call on the kit: done now, or held while the kit draws on its worker. bWhenReady: only once Begin has made it. */
	template <typename FWork>
	void Do(FWork&& Work, bool bWhenReady = true);
	void Hide();
	void PlayTest(double Now);
};
