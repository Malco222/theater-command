#pragma once

#include "CoreMinimal.h"
#include "Effects/BattleEffects.h"
#include "GameFramework/Actor.h"
#include "AirBattle.generated.h"

class AAirStage;
class FJsonObject;
class ACameraActor;
class FCampaignSession;
class SAirBattleScreen;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
struct FAirBattleState;

/**
 * The air battle's 3D scene (the air battles thread's 3D handoff, sections 1 to 10; ruling 238 for Pearl Harbor and
 * Midway). The battle itself is the rules' (src/battle/air): this scene asks it for a view a battle second, hands the
 * views to the flight core (AirShow), and draws what the core says: every aircraft of every formation flying as an
 * aircraft flies, its guns, the hits, the falls, the parachutes, the flak, the bombs, the smoke; the set piece's stage
 * and its ships under them; the sound. The screen's words and clock are AirDesk's; SAirBattleScreen draws them.
 *
 * Two feeds, one scene:
 *  - A recorded battle (Content/Data/AirBattles/<key>.json, written by tools/air-record on the rules branch),
 *    "-TheaterAirBattle=<key>" on the command line: played back as it was fought, the orders strip shown but not taken.
 *    With "-TheaterAirBattleShots=<folder>" it takes the art director's pictures there and quits.
 *  - The live battle, BeginLive: TC.airStart, then TC.battleTick a frame (kept a battle second ahead of what is shown),
 *    TC.battleOrder for his orders and TC.battleEnd for the after card (handoff section 1; land's battle-bridge.ts and
 *    the air diff must be in the rules bundle).
 *
 * The world: X up the board (the board's north), Y across it (the board's east), Z up from the sea, 100 units a metre;
 * a heading clockwise from north is Unreal's yaw. The actor stands at the origin, unturned, as the stage does.
 */
UCLASS()
class THEATERCOMMAND_API AAirBattle : public AActor
{
	GENERATED_BODY()

public:
	AAirBattle();

	/** "-TheaterAirBattle=<key>" on the command line (oahu, midway-fleet). */
	static bool WantsReplay(FString& OutKey);
	/** Plays a recorded battle. False, with a warning, if the file is missing or unreadable. */
	bool BeginReplay(const FString& Key);
	/** The same, two seconds in, once the table and the hour's light are up (so that they are put away after). */
	void ReplayWhenReady(const FString& Key);
	/** Opens the live battle for a raid (TC.airStart(kind, target, ids)): Kind "bomb", "superiority" or "ship". */
	bool BeginLive(FCampaignSession* InSession, const FString& Kind, int32 Target, const TArray<int32>& Ids);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// What the battle screen reads and asks.
	const FAirBattleState* GetState() const { return State.Get(); }
	/** Drag with the left button: the camera turns round what it looks at (pixels of the viewport). */
	void Orbit(const FVector2D& DeltaPixels);
	/** The wheel: nearer or farther. */
	void Zoom(float Steps);
	/** A left click: the group whose tag is under it in hand (bAdd: added to the hand), or none. */
	void PickAt(const FVector2D& Pixel, bool bAdd);
	/** A right click: attack the enemy group whose tag is under it, or patrol the sky there. */
	void OrderAt(const FVector2D& Pixel);
	/** A button of the orders strip, or its key: "hold", "breakoff", "breakoffall", "staff", "close", "ahead", "hunt",
	 * "jettison", "low", "middle", "high", "ride" (the close view), "continue". */
	void Press(const FString& Button);
	/** 0 (held), 0.5, 1, 2 or 4. */
	void SetSpeed(double Speed);
	/** The question standing answered: yes (HAND IT OVER, BREAK OFF ALL) or no (KEEP FIGHTING, Esc). */
	void Answer(bool bYes);

private:
	TSharedPtr<FAirBattleState> State;
	FCampaignSession* Session = nullptr;
	TUniquePtr<FBattleEffects> Effects;
	TSharedPtr<SAirBattleScreen> Screen;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	UPROPERTY()
	TObjectPtr<AAirStage> Stage;
	UPROPERTY()
	TObjectPtr<ACameraActor> Eye;
	/** One mesh an aircraft, by the flight core's key. */
	UPROPERTY()
	TMap<int32, TObjectPtr<UProceduralMeshComponent>> Planes;
	/** Parachutes, pieces and bombs: one mesh, laid afresh each frame. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Small;
	/** The sea and the land under a battle with no stage. */
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> Ground;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PlaneInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PlainInk;
	/** Each formation's engines, by group. */
	UPROPERTY()
	TMap<int32, TObjectPtr<UAudioComponent>> Engines;
	/** The mix's distance families, by name (mix.json distance). */
	UPROPERTY()
	TMap<FName, TObjectPtr<USoundAttenuation>> Families;
	UPROPERTY()
	TMap<FName, TObjectPtr<USoundBase>> Sounds;

	FString PendingReplay;
	double UnitsPerMetre = 100.0;
	double RealT = 0.0;          // real seconds since the scene opened
	bool bBegun = false;
	bool bOwnInput = false;
	// The strategy camera: what it looks at (board metres), how far, which way.
	FVector LookAt = FVector::ZeroVector;
	double Distance = 2600.0, Yaw = 200.0, Pitch = -18.0;
	bool bLookSet = false;
	FVector CloseEye = FVector::ZeroVector;
	FRotator CloseFacing = FRotator::ZeroRotator;
	// The trails running (the kit has room for about a dozen burning aircraft: handoff, the effects kit 05:25Z).
	TSet<int32> TrailsOn;
	static constexpr int32 MostTrails = 10;
	/** Falls the kit is shedding debris for (burning or spinning), and blow-ups whose blast the kit's AirFall drew. */
	TSet<int32> FallsOn;
	TSet<int32> BlastsTaken;
	// The sounds' own clocks.
	TMap<int32, double> GunHeardAt;  // a gun's last burst heard, real seconds
	TMap<int32, double> PassHeardAt; // a fighter group's last pass heard
	double FlakHeardAt = -10.0, FlakGunsAt = 0.0;
	struct FLater { double At; FName Name; FVector Where; FName Family; float Volume; float Cut; };
	TArray<FLater> Later;
	TSet<FName> SoundsMissing;
	/** The sound director's mix (Content/Data/Sound/mix.json). */
	TSharedPtr<FJsonObject> Mix;
	/** A blast's torn flame, streaming on with the wreck for a moment. */
	struct FTorn { int32 Key = 0; FVector At = FVector::ZeroVector; FVector Velocity = FVector::ZeroVector; double Until = 0.0; };
	TArray<FTorn> Torn;
	int32 TornNext = 0;

	// The picture run (-TheaterAirBattleShots).
	FString ShotFolder;
	struct FShot { FString Name; double At; bool bClose; bool bShips; bool bTaken; };
	TArray<FShot> Shots;
	double MetRealT = -1.0, ShipRealT = -1.0;
	int32 ShotsTaken = 0;
	double QuitAt = -1.0, CloseOffAt = -1.0, CardShotAt = -1.0;
	bool bCardShot = false;

	bool Open(const FString& Why);
	void Raise();
	void LayGround();
	void HideTheTable();
	void Feed();
	void Draw(double Real);
	void Play(double Real);
	void PlayEffects();
	void AimEye(double Real);
	void PlaceTags();
	/** A one-shot at a world point, in a distance family of the mix; After: held that many real seconds; Cut: only the first seconds of the take. */
	void Sound(const TCHAR* Name, const FVector& Where, const TCHAR* Family, float Volume = 1.0f, float After = 0.0f, float Cut = 0.0f);
	USoundBase* SoundOf(const TCHAR* Name, bool bLoop = false);
	USoundAttenuation* FamilyOf(const TCHAR* Name);
	void EngineSounds();
	void TakeShots(double Real);
	bool Order(const FString& Command);
	FVector World(double X, double Y, double Z) const;
	FVector WorldVector(double X, double Y, double Z) const;
	bool BoardUnder(const FVector2D& Pixel, double Height, FVector2D& OutBoard) const;
	int32 TagUnder(const FVector2D& Pixel) const;
	FString ModelFor(const FString& Model, const FString& Nation, bool bFighter) const;
	UProceduralMeshComponent* PlaneMesh(int32 Key, const FString& Model, const FString& Nation, bool bFighter, double Span);
	void ShowScreen();
	void HideScreen();
	void Finish();
};
