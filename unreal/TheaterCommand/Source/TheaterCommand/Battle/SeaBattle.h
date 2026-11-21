#pragma once

#include "CoreMinimal.h"
#include "Effects/BattleEffects.h"
#include "GameFramework/Actor.h"
#include "SeaBattle.generated.h"

class AAirStage;
class ACameraActor;
class ACampaignEnvironment;
class FJsonObject;
class UAudioComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USceneComponent;
class USkyLightComponent;
class USoundAttenuation;
class USoundBase;
class UTexture2D;
class SSeaBattleScreen;
class SWidget;
struct FSeaBattleState;

/** The sea battle camera's kinds of shot (ASeaBattle::ShotEye). */
enum class ESeaShot : uint8 { Fleets, Line, Broadside, Rangefinder, Hit, Tracks, Strike, Sinking, Overview, Burning, Funnels };

/**
 * The fleet action's 3D scene (the ship behaviour plan's B0: the sea battle had no scene, so its ships' seamanship could
 * not be seen). The battle itself is the rules' (src/battle/sea): this scene hands its views to the playback core
 * (SeaShow), which steams every ship of every piece, and draws what the core says: the ships under way on the open sea
 * (the Pearl Harbor stage's sea, ships, hit looks and ends: AAirStage raised as the open sea, each ship steered), their
 * wakes and bow waves, the salvos, the columns of water round the target, the hits, the fires, the ships going down and
 * their oil burning, the torpedo tracks, the carriers' strikes and the flak, and the sound.
 *
 * One feed for now: a recorded action (Content/Data/SeaBattles/<key>.json, written by tools/sea-record on the rules
 * branch), "-TheaterSeaBattle=<key>" on the command line (two-lines, light-ships, samar), played as it was fought, the
 * camera cutting between the moments the action holds (the fleets before contact, the battle line steaming in line ahead
 * once it forms, a battleship's broadside, the fall of shot
 * through a rangefinder's long lens, a hit, the torpedo tracks, a strike diving, a ship going down, the whole battle from
 * above, its end). With "-TheaterSeaBattleShots=<folder>" it takes those as the art director's pictures, skipping the
 * quiet stretches between them, and quits. The live battle (TC.seaStart and battleCalls) comes with the shared wiring.
 *
 * Over it the screen (SSeaBattleScreen): for now the tug-of-war bar alone at the top, each side's flag at its end and
 * the knot sliding toward the side that is losing (SeaFeed::KnotOf on the view the picture has reached, eased here).
 *
 * The world: X up the board (the board's north), Y across it (the board's east), Z up from the sea, 100 units a metre;
 * the actor stands at the origin, unturned, as the stage does. A heading clockwise from north is Unreal's yaw.
 *
 * Other flags: "-TheaterSeaBattleSpeed=2" (the battle's pace, 0.25 to 4), "-TheaterSeaBattleFrom=300" (start there),
 * "-TheaterSeaHour=9.5" (the hour of the day, for the sun), "-TheaterSeaWake=1,1" (the wakes' brightness and strength),
 * "-TheaterSeaNoFunnels" (no funnel smoke; there is none either on an effects kit without its own "funnel" kind),
 * "-TheaterSeaTextScale=1.25" (the screen's size over Unreal's own, 0.75 to 2, as the air screen's).
 */
UCLASS()
class THEATERCOMMAND_API ASeaBattle : public AActor
{
	GENERATED_BODY()

public:
	ASeaBattle();

	/** "-TheaterSeaBattle=<key>" on the command line. */
	static bool WantsReplay(FString& OutKey);
	/** Plays a recorded action two seconds in, once the table and the room's light are up (so that they are put away after). */
	void ReplayWhenReady(const FString& InKey);
	/** Plays a recorded action now. False, with a warning, if the file is missing or unreadable. */
	bool BeginReplay(const FString& InKey);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** The tug-of-war bar for the screen (SSeaBattleScreen): the knot from the player's flag, 0 to 1, eased (0.5 level,
	 * past the middle he is winning), and each side's flag; false while the recording gives no lost shares for it. */
	bool GetBar(double& OutKnot, UTexture2D*& OutMine, UTexture2D*& OutTheirs) const;

private:
	/** One of the camera's moments (the picture run's pictures; the shots it cuts between when watched). */
	struct FShot
	{
		FString Name;
		ESeaShot Kind = ESeaShot::Overview;
		double At = 0.0;         // battle seconds
		int32 Subject = -1;      // a ship's key (the show's), or -1
		int32 Other = -1;        // the ship she fires on (Broadside), her piece (Fleets), the side (Line), or -1
		FVector2D Spot = FVector2D::ZeroVector; // board metres: where it happens (a splash, a track's start, an oil fire)
		FVector2D Way = FVector2D::ZeroVector;  // a track's way
		bool bTaken = false;
	};
	/** A torpedo's track on the sea: from where it was fired to its head, and when it stopped (battle seconds, -1 running);
	 * an oxygen torpedo's (the Japanese ships' Type 93) leaves a third of the bubbles. */
	struct FTrackLine
	{
		FVector2D From = FVector2D::ZeroVector, Head = FVector2D::ZeroVector;
		double StopT = -1.0;
		bool bOxygen = false;
	};
	/** A ship's wake: her stern's path (board metres), each point's battle time, how much wake she made there (Strength,
	 * her speed over her flank speed), her speed (m/s), beam and length then, and how far along her path it lies (S,
	 * metres from where the scene began drawing her: the foam's pattern rides on the water by it). */
	struct FWakePoint
	{
		FVector2D P = FVector2D::ZeroVector;
		double T = 0.0, Strength = 0.0, Beam = 20.0, Long = 100.0, Speed = 0.0, S = 0.0;
	};
	/** A ship broken in two as she goes: where (board metres, the sea over her cut), when, and her size. */
	struct FBreakUp
	{
		FVector2D At = FVector2D::ZeroVector;
		double T = 0.0, Heading = 0.0, Long = 100.0, Beam = 12.0, NextSteam = 0.0;
		int32 Puffs = 0;
		bool bSmoke = false;
	};
	/** A sound held back (flash before bang), on the real clock. */
	struct FLater
	{
		double At = 0.0;
		FName Name;
		FVector Where = FVector::ZeroVector;
		FName Family;
		float Volume = 1.0f;
	};
	/** An oil fire lit while the picture run skipped ahead: lit once it lands, as long burning as it should be. */
	struct FOilLater
	{
		int32 Id = 0;
		FVector2D At = FVector2D::ZeroVector;
		double Reach = 0.0, Until = 0.0, Since = 0.0, LitAt = 0.0;
		FString Kind;
	};
	/** A flak burst due (the show gives each a delay). */
	struct FFlakLater
	{
		double At = 0.0;
		FVector Where = FVector::ZeroVector;
		FString Kind;
	};

	TSharedPtr<FSeaBattleState> State;
	TUniquePtr<FBattleEffects> Effects;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	UPROPERTY()
	TObjectPtr<AAirStage> Stage;
	UPROPERTY()
	TObjectPtr<ACameraActor> Eye;
	/** The wakes, bow waves and torpedo tracks: the foam on the campaign map's wake ink (M_Wake: its noise breaks the
	 * foam up), laid afresh each frame; the marks that darken (oil on the water) on M_Highlight, which also takes the
	 * foam where the build has no M_Wake. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Marks;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Foam;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MarkInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FoamInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PlaneInk;
	/** One mesh an aircraft of the strikes, by the show's key. */
	UPROPERTY()
	TMap<int32, TObjectPtr<UProceduralMeshComponent>> Planes;
	UPROPERTY()
	TMap<FName, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY()
	TMap<FName, TObjectPtr<USoundAttenuation>> Families;
	UPROPERTY()
	TObjectPtr<UAudioComponent> DayBed;
	UPROPERTY()
	TObjectPtr<UAudioComponent> BattleBed;
	/** The two flags at the bar's ends, the player's and the enemy's (Campaign/CampaignFlags, the card's own). */
	UPROPERTY()
	TObjectPtr<UTexture2D> MyFlag;
	UPROPERTY()
	TObjectPtr<UTexture2D> TheirFlag;

	/** The screen over the scene (the bar), and what holds it on the viewport a size larger (ShowScreen). */
	TSharedPtr<SSeaBattleScreen> Screen;
	TSharedPtr<SWidget> ScreenFrame;
	/** The bar's knot as drawn (eased toward the view's: MoveKnot), and whether the recording gives one at all. */
	double BarKnot = 0.5;
	bool bBarGiven = false;

	FString PendingReplay;
	FString RecordKey;
	double UnitsPerMetre = 100.0;
	double RealT = 0.0;
	double Speed = 1.0;
	bool bBegun = false;

	// The ships laid on the stage, by the show's key; the falls the kit is shedding debris for; the trails running
	// (fires on ships under way, funnel smoke), by the trail's id; the ships chosen to smoke from their funnels (2 a
	// plume a funnel, 1 one plume) and when they were chosen.
	TSet<int32> Laid;
	TMap<int32, double> DrawnLong; // (her model's length as the stage fitted it, metres: her stern for the wake)
	TMap<int32, FVector> Falling;
	TSet<int32> FireTrails, FunnelTrails;
	TMap<int32, uint8> FunnelsChosen;
	double FunnelsChosenAt = -100.0;
	int32 FunnelsShot = -2; // (the shot the funnels were chosen for: chosen again at once on a cut)
	// The ships broken in two, by the show's key, and the trails pouring smoke from their breaks.
	TMap<int32, FBreakUp> Breaks;
	TSet<int32> BreakTrails;
	bool bFunnels = false;
	// (while the scene lays its own oil fires: the kit's ground is the sea there, not the deck of a ship over it)
	bool bOnWater = false;
	TMap<int32, TArray<FWakePoint>> Wakes;
	TMap<int32, FTrackLine> Tracks;
	TMap<FString, int32> ShapeOf; // a model's name to its shape's place in Shapes
	struct FPlaneShape
	{
		TArray<FVector> Points;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FColor> Colors;
	};
	TArray<FPlaneShape> Shapes;
	double WakeBright = 0.85, WakeStrength = 1.0;

	// The camera.
	TArray<FShot> Shots;
	int32 ShotNow = -1;
	FVector EyeAt = FVector::ZeroVector;
	FRotator EyeFacing = FRotator::ZeroRotator;
	double EyeLens = 50.0;
	/** Where each ship last stood (board metres) and her heading, for a shot on a ship the show no longer draws. */
	TMap<int32, FVector4> LastSeen;

	// The picture run.
	FString ShotFolder;
	int32 NextPicture = 0;
	double PictureAt = -1.0;
	double QuitAt = -1.0;
	bool bSkipping = false;
	TArray<FOilLater> OilLater;
	TArray<FFlakLater> FlakLater;
	TArray<FLater> Later;
	TSet<FName> SoundsMissing;
	TSharedPtr<FJsonObject> Mix;
	bool bBattleBedUp = false;
	// The ships' guns at aircraft (Battle/ShipGuns): each stream firing to its battery's sound key, when each battery's
	// next take is due (real seconds) and the take playing, when a heavy gun's boom and a burst were last heard.
	TMap<int32, int32> GunsFiring;
	TMap<int32, double> GunHeardAt;
	TMap<int32, TWeakObjectPtr<UAudioComponent>> GunTakes;
	double ShipGunHeardAt = -100.0;
	double FlakHeardAt = -100.0;
	bool bScreensSaid = false;

	// What the room had before the battle took its place, put back by EndPlay.
	struct FKeptLamp { TWeakObjectPtr<UDirectionalLightComponent> Lamp; FRotator Rotation; float Intensity = 0.0f; FLinearColor Colour; float Temperature = 0.0f; float SourceAngle = 0.0f; FLinearColor SunDisc = FLinearColor::White; };
	struct FKeptSky { TWeakObjectPtr<USkyLightComponent> Sky; float Intensity = 0.0f; FLinearColor Colour; };
	struct FKeptHaze { TWeakObjectPtr<UExponentialHeightFogComponent> Haze; float Start = 0.0f, Density = 0.0f, Falloff = 0.0f, MostOpacity = 0.0f; FLinearColor Colour; };
	TArray<FKeptLamp> KeptLamps;
	TArray<FKeptSky> KeptSkies;
	TArray<FKeptHaze> KeptHazes;
	TArray<TWeakObjectPtr<ACampaignEnvironment>> KeptRooms;
	TArray<TWeakObjectPtr<AActor>> KeptHidden;
	TWeakObjectPtr<UDirectionalLightComponent> BattleKey;
	bool bRoomTaken = false;
	bool bHudWasShown = false;
	double RoomCheckedAt = -10.0;

	void TakeTheRoom(double SunAzimuth, double SunElevation);
	void LightTheBattle();
	void GiveBackTheRoom();

	/** Hands the show every view it needs to show battle time T (one view in hand ahead of it, as the cloud test does). */
	void Feed(double T);
	/** The show to battle time T, its ships steered on the stage and its effects played (bSkip: a picture run skipping
	 * ahead, only what lasts is kept). */
	void StepTo(double T, bool bSkip);
	void LayAndSteer();
	void PlayEffects(bool bSkip);
	void DrawPlanes();
	void LayWakes();
	void DrawMarks();
	void RunTrails();
	/** The show played through on its own once (it is the same show every time), to find its moments: the shots. */
	void PlanShots();
	void AimEye();
	bool ShotEye(const FShot& Shot, FVector& OutEye, FVector& OutLook, double& OutLens);
	void TakePictures();
	void Quit(const FString& Why);
	/** The screen on the viewport and off it. */
	void ShowScreen();
	void HideScreen();
	/** The bar's knot toward the view's at the moment shown (SeaFeed::KnotOf), on the screen's clock: about a second and
	 * a half to arrive, every move alike, the break's too (the UI director); at once across a picture run's skip. */
	void MoveKnot(double RealDt, bool bAtOnce);

	UAudioComponent* Sound(const FName& Name, const FVector& Where, const FName& Family, float Volume = 1.0f, bool bFlashFirst = true);
	/** A ship's light gun firing this step (its stream's key, ShipGuns::KindSlot of its kind, its slot: 0 hmg, 1 aa25, 2 aa40), heard
	 * as her battery of that kind; and ceasing fire, the battery's take faded once its last mount is silent. */
	void GunFired(int32 Key, int32 Slot, const FVector& At);
	void GunStopped(int32 Key);
	USoundBase* SoundOf(const FName& Name, bool bLoop = false);
	USoundAttenuation* FamilyOf(const FName& Name);
	void Beds();

	FVector World(double X, double Y, double Z) const;
	FVector WorldVector(double X, double Y, double Z) const;
	/** The board point (metres) under a world point. */
	FVector BoardOf(const FVector& WorldAt) const;
};
