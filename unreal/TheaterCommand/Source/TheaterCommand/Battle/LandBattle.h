#pragma once

#include "CoreMinimal.h"
#include "Effects/BattleEffects.h"
#include "GameFramework/Actor.h"

#include "LandBattle.generated.h"

class ACameraActor;
class ACampaignEnvironment;
class ACampaignMapActor;
class FJsonObject;
class SLandBattleScreen;
class SWidget;
class UAudioComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class USceneComponent;
class USkyLightComponent;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UTexture2D;
struct FLandBattleState;

/**
 * The ground battle's 3D scene (release 2; Brian, 21 November 2026: "when a soldier gets hit have their body be on
 * the ground dead", "i want it to be realistic", the same intense feeling as the air battle, "as if im watching" a war
 * film, a feeling only, nothing copied; "lets do a normandy one with shermans vs tiger tanks and soldiers and
 * artillery", "a dry run of a land battle just like the air").
 *
 * The battle is the rules' (src/battle/land and the squad brain): a recorded battle, Content/Data/LandBattles/<key>.json
 * (Tools/land_record), is played back exactly as it was fought. Everything the scene decides is in plain C++ cores the
 * cloud test plays too (Tools/land_play, land_show, land_field, land_kit):
 *  - LandPlay: every round fired, once: its flash, its sound, where it lands and what it does there.
 *  - LandSound: what is heard (the sound director's mix): every shot on the screen, cracks, flash-to-bang, the beds.
 *  - LandEye: the camera, shot by shot among the men, a few seconds ahead of what is about to happen.
 *  - LandDesk: the words and the tug-of-war bar.
 *  - LandField: the bocage: banks and hedges, sunken lanes, fields, orchards, the village, out to the haze.
 *  - LandMen: every soldier, his clips, his shots and his fall; the dead stay where they fell to the end.
 *  - LandKit: the Shermans, the Tigers, the guns and the lorries: tracks, turrets, recoil, knock-outs and fires.
 * This actor only lays what they say into the world, plays it and draws the screen (SLandBattleScreen).
 *
 * "-TheaterLandBattle=<key>" plays a recording; with "-TheaterLandBattleShots=<folder>" it takes the art director's
 * pictures there and quits. The world: Unreal X the field's north (y), Y its east (x), Z up, 100 units a metre, as the
 * air battle's; the field's origin (its south-west corner) at the world's origin.
 */
UCLASS()
class THEATERCOMMAND_API ALandBattle : public AActor
{
	GENERATED_BODY()

public:
	ALandBattle();

	/** "-TheaterLandBattle=<key>" on the command line (normandy-1944). */
	static bool WantsReplay(FString& OutKey);
	/** Plays a recorded battle two seconds in, once the table and its light are up (so that they are put away after). */
	void ReplayWhenReady(const FString& Key);
	/** Plays a recorded battle now. False, with a warning, if it is missing or unreadable. */
	bool BeginReplay(const FString& Key);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// What the screen reads and asks.
	const FLandBattleState* GetState() const { return State.Get(); }
	/** 0 (held), 0.5, 1, 2 or 4. */
	void SetSpeed(double Speed);
	/** "cut" (a new shot now), "hold" (keep this shot until pressed again), "continue" (the after card). */
	void Press(const FString& Button);
	/** A drag on the screen: the hand-held camera looks a little aside (pixels), and comes back when let go. */
	void Look(const FVector2D& DeltaPixels);
	bool IsShotHeld() const { return bShotHeld; }
	/** The flag pictures for the bar's two ends: the attack's (his) and the defence's. */
	UTexture2D* FlagOf(int32 Side) const;

private:
	TSharedPtr<FLandBattleState> State;
	TUniquePtr<FBattleEffects> Effects;
	TSharedPtr<SLandBattleScreen> Screen;
	TSharedPtr<SWidget> ScreenFrame;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	UPROPERTY()
	TObjectPtr<ACameraActor> Eye;

	/** The paint: vertex colours on the campaign's surface (sRGB in, made linear there), a fine grain on the ground. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GroundInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ThingInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ManInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SteelInk;

	// ---- The ground (LandField) ----
	/** One drawn square of the field (or of the far ring): its ground at two levels, what stands on it, and the
	 * hedges', trees' and houses' instances laid into one mesh at two levels. */
	struct FTileDrawn
	{
		FBox2D Box;
		TWeakObjectPtr<UProceduralMeshComponent> Near, Far, Built;
		bool bNearShown = false;
		double FarFrom = 0.0;
	};
	TArray<FTileDrawn> Tiles;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> FieldMeshes;
	/** The ground's things (hedge shrubs, trees, gates; LandField's bases) are far too many to lay into meshes (some
	 * 129,000 of them, 150 million triangles at their near level): each kind is made once as a mesh of its own, in three
	 * shades of its instances' tints, its near level the mesh's first level of detail and its far the second, and drawn
	 * by instancing. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> ThingMeshes;
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ThingCopies;
	/** Each base's two shade edges (the brightness of an instance's tint, R + G + B) and its shades' tints. */
	TArray<FVector2f> ShadeEdges;
	/** The grass, corn, flowers and stones round the eye and round what it looks at (LandField::Clutter), laid again
	 * when either has moved some metres: one instanced mesh a clutter kind and shade (ClutterCopyOf, -1 none). */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ClutterCopies;
	TArray<int32> ClutterCopyOf;
	FVector2D ClutterEyeAt = FVector2D(1e9, 1e9), ClutterLookAt = FVector2D(1e9, 1e9);
	/** A shell hole made near the laid clutter (its grass to go): laid again half a second after the last laying. */
	bool bClutterHoled = false;
	double ClutterHoledAt = -10.0;
	/** The shell holes (LandCrater): torn earth on the ground's own paint, one section a chunk of holes. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> CraterMesh;

	// ---- The men (LandMen) ----
	struct FManDrawn
	{
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		int32 Lod = -1;
		uint32 Stamp = 0xFFFFFFFFu;
	};
	TArray<FManDrawn> MenDrawn;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> MenMeshes;
	TArray<float> SkinAt, SkinFacing;

	// ---- The tanks, guns and lorries (LandKit) ----
	struct FBodyDrawn
	{
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		int32 Vehicle = 0, Body = 0;
		bool bTrack = false;
		bool bNear = true;
		double SootShown = 0.0;
	};
	TArray<FBodyDrawn> BodiesDrawn;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> BodyMeshes;
	TArray<float> LinkPoints;

	// ---- The sound (LandSound, the sound director's mix) ----
	TSharedPtr<FJsonObject> Mix;
	UPROPERTY()
	TMap<FName, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY()
	TMap<FName, TObjectPtr<USoundAttenuation>> Families;
	TSet<FName> SoundsMissing;
	/** The beds, one loop each, at the level the mixer says. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UAudioComponent>> BedPlayers;
	/** Each vehicle's engine loop, and the cue it plays. */
	UPROPERTY()
	TMap<int32, TObjectPtr<UAudioComponent>> Engines;
	TMap<int32, FName> EngineCue;
	/** Takes that move with their shooter: the take and its track. */
	struct FMoving
	{
		TWeakObjectPtr<UAudioComponent> Player;
		int32 Track = -1;
	};
	TArray<FMoving> Moving;
	/** Takes started in the last two seconds, for the ears' dulling after a blast close by. */
	struct FRecent
	{
		TWeakObjectPtr<UAudioComponent> Player;
		double Started = 0.0;
	};
	TArray<FRecent> Recent;
	/** The ears dulled by a blast close by: when it began, its low-pass, and how long down, held and back. */
	double ShockAt = -100.0, ShockLpf = 600.0, ShockDown = 0.1, ShockHold = 1.2, ShockBack = 2.5, ShockDb = -6.0;
	/** The real seconds each take of the mix lasts (read from the files as they load). */
	TMap<FName, float> TakeSeconds;
	/** The whole battle's lift down among the men this frame (mix.json land.closeView), dB: the engines take it here, the
	 * beds and takes in the mixer. */
	double LiftDb = 0.0;
	/** The master limiter at buses.master.limiterCeilingDb while the scene is up (taken off in EndPlay). */
	UPROPERTY()
	TObjectPtr<class USoundEffectSubmixPreset> Limiter;
	/** When the sound's tally was last logged (real seconds). */
	double SoundLogAt = 0.0;

	// ---- The flags for the bar ----
	UPROPERTY()
	TObjectPtr<UTexture2D> FlagAttack;
	UPROPERTY()
	TObjectPtr<UTexture2D> FlagDefence;

	// ---- What the table had before the battle took its place (as the air battle keeps it) ----
	struct FKeptLamp { TWeakObjectPtr<UDirectionalLightComponent> Lamp; FRotator Rotation; float Intensity = 0.0f; FLinearColor Colour; float Temperature = 0.0f; float SourceAngle = 0.0f; FLinearColor SunDisc = FLinearColor::White; };
	struct FKeptSky { TWeakObjectPtr<USkyLightComponent> Sky; float Intensity = 0.0f; FLinearColor Colour; };
	struct FKeptHaze { TWeakObjectPtr<UExponentialHeightFogComponent> Haze; float Start = 0.0f, Density = 0.0f, Falloff = 0.0f, MostOpacity = 0.0f; FLinearColor Colour; };
	TArray<FKeptLamp> KeptLamps;
	TArray<FKeptSky> KeptSkies;
	TArray<FKeptHaze> KeptHazes;
	TArray<TWeakObjectPtr<ACampaignEnvironment>> KeptRooms;
	TArray<TWeakObjectPtr<AActor>> KeptPieces;
	TWeakObjectPtr<ACampaignMapActor> Table;
	TWeakObjectPtr<UDirectionalLightComponent> BattleKey;
	bool bTableHidden = false;
	/** The way toward the sun (world), from the recording's place, date and hour. */
	FVector TowardSun = FVector(0.3, 0.5, 0.8);

	// ---- The camera's own state ----
	/** A drag's look aside (degrees of yaw and pitch), easing back to nothing when let go. */
	FVector2D LookAside = FVector2D::ZeroVector;
	double LookAsideAt = -10.0;
	bool bShotHeld = false;

	// ---- The run ----
	FString PendingReplay;
	FString Key;
	double UnitsPerMetre = 100.0;
	double RealT = 0.0;
	bool bBegun = false;
	bool bOwnInput = false;
	/** r.ScreenPercentage as the campaign had it, while "-TheaterLandSize=" holds its own; -1 none. */
	float KeptScreenPercent = -1.0f;
	/** The log every 10 s: frames, the slowest, what the cores and the drawing cost. */
	double LogAt = 0.0, SlowestFrame = 0.0, CoresMs = 0.0, MenMs = 0.0, ArmourMs = 0.0, FieldMs = 0.0;
	int32 LogFrames = 0;

	// ---- The picture run (-TheaterLandBattleShots) ----
	FString ShotFolder;
	struct FShot
	{
		FString Name;
		/** Battle second it is taken at (the shot asked for a little before). */
		double At = 0.0;
		/** The shot asked of the camera (LandEye::Kind as a number; -1 the camera's own choice), and on whom. */
		int32 Kind = -1;
		int32 Subject = -1;
		bool bAsked = false;
		bool bTaken = false;
	};
	TArray<FShot> Shots;
	int32 ShotsTaken = 0;
	double QuitAt = -1.0, CardShotAt = -1.0;
	bool bCardShot = false;

	bool Open();
	void LoadInks();
	void LayField();
	/** The near things (hedges, trees, gates) laid into a tile's mesh, or taken out. */
	void LayThings();
	int32 ShadeOf(int32 Base, uint8 R, uint8 G, uint8 B) const;
	void ShowField(const FVector& EyeWorld, double Battle);
	void LayClutter(const FVector2D& EyeAt, const FVector2D& LookAt);
	void MakeMen();
	void DrawMen(const FVector& EyeWorld);
	void MakeArmour();
	void DrawArmour(const FVector& EyeWorld);
	void HideTheTable();
	void KeepTheTableHidden();
	void LightTheBattle();
	void ShowTheTable();
	void MakeFlags();

	void PlayEffects();
	/** This frame's shells, bombs and grenades into the ground: their holes, kept the whole battle. */
	void MakeCraters();
	void PlayArmourEvents();
	void PlayMenEvents();
	void PlaySound(double RealDt);
	/** The mixer's takes due now (this frame's and those held for later: flash to bang, the 0.15 s rule). */
	void StartTakes();
	/** The sound's tally in the log, every 20 s and at the end. */
	void LogSound() const;
	void PlayEngines();
	UAudioComponent* StartTake(const TCHAR* Name, const FVector& Where, bool bSpatial, const TCHAR* Family, float Volume, float Pitch, float From, float Cut, float CutFade, float FadeIn);
	USoundBase* SoundOf(const FName& Name, bool bLoop = false);
	USoundAttenuation* FamilyOf(const FName& Name);
	double BusDb(const FString& Bus) const;
	void AimEye(float DeltaSeconds);
	void TakeShots();
	void ShowScreen();
	void HideScreen();
	void Finish();

	FVector World(double X, double Y, double Z) const { return FVector(Y * UnitsPerMetre, X * UnitsPerMetre, Z * UnitsPerMetre); }
	FVector WorldWay(double X, double Y, double Z) const { return FVector(Y, X, Z); }
	double GroundZ(double X, double Y) const;
	FString GroundWordAt(double X, double Y) const;
};
