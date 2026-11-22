#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Effects/BattleEffects.h"
#include "GameFramework/Actor.h"
#include "AirBattle.generated.h"

class AAirStage;
class UTexture2D;
class ACampaignEnvironment;
class ACampaignMapActor;
class FJsonObject;
class ACameraActor;
class FCampaignSession;
class SAirBattleScreen;
class SWidget;
class UAudioComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UDecalComponent;
namespace AirShow { struct Fx; struct Shape; }
class USceneComponent;
class USkyLightComponent;
class USoundAttenuation;
class USoundBase;
struct FAirBattleState;
struct FAirLandMade;
class UTexture2D;

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
	/** A nation's flag of the period as the table paints it (the release 1 card's own), for the ends of the tug-of-war
	 * bar; null with no table in the world or no tag. */
	UTexture2D* FlagPicture(const FString& Tag);
	/** A left click: the group whose tag is under it in hand (bAdd: added to the hand), or none; in the close view the
	 * ride moves to that squadron too. */
	void PickAt(const FVector2D& Pixel, bool bAdd);
	/** A double click on a tag, his or the enemy's: the chase view, riding behind that squadron's leader. */
	void RideAt(const FVector2D& Pixel);
	/** A right click: attack the enemy group whose tag is under it, or patrol the sky there. */
	void OrderAt(const FVector2D& Pixel);
	/** A button of the orders strip, or its key: "hold", "breakoff", "breakoffall", "staff", "close", "ahead", "hunt",
	 * "jettison", "low", "middle", "high", "ride" (the close view on or off), "ride:next" and "ride:prev" (the next or
	 * the last aircraft of the squadron ridden), "ride:down" and "ride:up" (the next or the last squadron), "continue". */
	void Press(const FString& Button);
	/** 0 (held), 0.5, 1, 2 or 4. */
	void SetSpeed(double Speed);
	/** The question standing answered: yes (HAND IT OVER, BREAK OFF ALL) or no (KEEP FIGHTING, Esc). */
	void Answer(bool bYes);

private:
	TSharedPtr<FAirBattleState> State;
	FCampaignSession* Session = nullptr;
	TUniquePtr<FBattleEffects> Effects;
	/** The effects on the ground and the sea (bomb bursts, crashes, splashes, fires, the stage's oil): a kit of their
	 * own, so that seen from above the clouds they are drawn under them, and the air fight over them. */
	TUniquePtr<FBattleEffects> GroundFx;
	TWeakObjectPtr<UProceduralMeshComponent> GroundFxMesh;
	/** The air kit's mesh: drawn over the clouds while the eye is over them, behind them while it is under them. */
	TWeakObjectPtr<UProceduralMeshComponent> AirFxMesh;
	/** Which is drawn over which this frame: 1 the eye over the clouds, 2 under them, 0 not yet set. */
	int32 SkyOrder = 0;
	TSharedPtr<SAirBattleScreen> Screen;
	/** What holds the screen on the game's viewport: it draws the screen a size larger (ShowScreen). */
	TSharedPtr<SWidget> ScreenFrame;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	UPROPERTY()
	TObjectPtr<AAirStage> Stage;
	UPROPERTY()
	TObjectPtr<ACameraActor> Eye;
	/** One mesh an aircraft, by the flight core's key, and the model and span it was made as. */
	UPROPERTY()
	TMap<int32, TObjectPtr<UProceduralMeshComponent>> Planes;
	/** What each aircraft's mesh was made as; whether it has a far shape (section 0) and its near one made (section 1),
	 * and which is shown. */
	struct FPlaneMade
	{
		FString Model;
		double Span = 0.0;
		FString Shape;
		bool bTwoShapes = false;
		bool bNearMade = false;
		bool bNear = false;
		/** It casts its shadow (made casting; AirParts::ShadowFarM). */
		bool bShadow = true;
		/** Its airscrews' turning discs, one an engine, shown with the near shape (Brian 11-20 14:20Z: "the japanese
		 * zero's propeller didnt spin"); and where in its turn each aircraft's screws start, so that no two turn as one. */
		TArray<TWeakObjectPtr<UProceduralMeshComponent>> Discs;
		double Phase = 0.0;
		/** Each disc's engine (from 1, the model's engine.N), taken when its discs are first turned; and whether it is
		 * shown standing still (its engine dead and feathered: its blades, section 1, in place of the turning disc). */
		TArray<int32> DiscEngine;
		TArray<bool> DiscStill;
		/** Its near shape has a bay (sections 2 and 3 its doors, 4 its racked load); the doors' opening as last drawn,
		 * 0 shut to 1 open; and its load gone with its stick. */
		bool bBay = false;
		double BayShown = 0.0;
		bool bBombsGone = false;
		/**
		 * Where rounds have struck it (Brian, 22 November 04:12Z: "if a plane gets hit make sure the bullet holes and
		 * damage shows on the wing or fuselage"): each strike in its mesh's own frame (engine units, its nose toward
		 * -X), and once laid on its skin, the way out of the skin there; the half width of its soot and the share of
		 * that its hole takes (metres); which ink it wears (0 a rifle or machine-gun round, 1 a cannon shell, 2 a
		 * heavy shell or flak); and its decal while the aircraft is near the eye (AirBattleMarks.cpp). Kept for the
		 * aircraft's whole flight, at most MostMarks of them, the bigger strikes kept when it is full.
		 */
		struct FMark
		{
			FVector At = FVector::ZeroVector;
			FVector Out = FVector::UpVector;
			bool bOnSkin = false;
			double Soot = 0.2, Hole = 0.3;
			int32 Kind = 0;
			int32 Turn = 0;
			TWeakObjectPtr<UDecalComponent> Decal;
		};
		TArray<FMark> Marks;
	};
	TMap<int32, FPlaneMade> PlaneMade;
	/** The strikes' decals (M_HitMark), one ink a kind of strike; the most strikes kept on one aircraft, and the most
	 * decals laid at once over all the aircraft near the eye. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MarkInks;
	// (a machine-gun burst a walking line of 6 to 14 holes since a1acb552's frames: room for some five such on one
	// aircraft, and as many decals over all of them near the eye as about thirty bursts)
	static constexpr int32 MostMarks = 64;
	static constexpr int32 MostMarkDecals = 400;
	int32 MarkDecals = 0;
	/** Strikes kept so far (each one's turn and size are drawn from it and its aircraft's key); the ink looked for once;
	 * the first decal's line written. */
	int32 MarkStrikes = 0;
	bool bMarkInksTried = false;
	bool bMarkLaidLogged = false;
	/** A strike the flight core shows on an aircraft (its Hit): kept on the aircraft it struck, in its own frame. */
	void MarkStrike(const AirShow::Fx& E);
	/** The strikes on each aircraft near the eye (the ridden one, and those within MarkNearM metres) laid as decals on
	 * its skin; taken up again from the others. */
	void DrawMarks(const FVector& EyeAt);
	static constexpr double MarkNearM = 400.0;
	/** An aircraft's decals taken up (it has gone far, out of sight, or is made again). */
	void LiftMarks(FPlaneMade& Made);
	/** The aircraft flying that has been struck the most, a cannon shell's tear weighed as three rounds and a heavy
	 * shell's as six (the picture run's strike-marks ride); INDEX_NONE when none has been struck. */
	int32 MostMarked() const;
	/** The aircraft the picture run's strike-marks ride is on (INDEX_NONE when none): a bomber's eye then stands on the
	 * side its strikes are on, and under it when they are mostly underneath (MarksSide). */
	int32 MarksEyeKey = INDEX_NONE;
	/** Which side of an aircraft its strikes mostly lie on: Hand 1 its right, -1 its left; Under true when most face down. */
	void MarksSide(int32 Key, double& Hand, bool& bUnder) const;
	/** Parachutes, pieces and bombs: one mesh, laid afresh each frame. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Small;
	/** The day's clouds: one see-through mesh of soft puffs (the effects kit's shape and M_Highlight), turned to the eye
	 * when it has moved enough, and carried by the wind. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Clouds;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CloudInk;
	TArray<FVector> CloudPoints;
	TArray<FColor> CloudColours;
	/** The clouds drawn from the sheet of cloud shapes (M_Cloud; AirSky::MakeFragments, made once a run): the sheet, its
	 * points' places on it and the sun's way on them (each room's), and whether it is so; else soft discs (M_Highlight).
	 * The shadows' own ink (soft discs always). */
	UPROPERTY()
	TObjectPtr<UTexture2D> CloudSheet;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CloudShadowInk;
	TArray<FVector2D> CloudPlaces, CloudWays, NearCloudPlaces, NearCloudWays;
	bool bCloudShapes = false;
	/** The near clouds (AirSky::Rooms): a small mesh of their own, turned as often as the eye's moves ask, drawn after
	 * the far ones (Clouds), which are turned only when the eye has moved far enough to show. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> NearClouds;
	TArray<FVector> NearCloudPoints;
	TArray<FColor> NearCloudColours;
	/** The sky's own clock for the log (every 5 s: turns of each room, what painting them cost on the pool's thread
	 * and what sending them cost this one, the slowest frame's share, the turns painted at once (at a cut, or with
	 * -TheaterSkyAtOnce), and the frames a turn was still painting). */
	int32 SkyNearTurns = 0, SkyBothTurns = 0, SkyAtOnce = 0, SkyLate = 0;
	double SkyPaintSeconds = 0.0, SkySendSeconds = 0.0, SkySlowest = 0.0, SkyClockAt = -1.0;
	/** A turn of the clouds being painted aside, on the engine's pool of threads (build 1756: a far turn painted on the
	 * game thread cost the frame 20 to 50 ms). While it runs, it alone touches the rooms, their field (the state's
	 * SkyAside) and the three meshes' points and colours here; DrawSky sends what it painted the frame it is done,
	 * and EndPlay and LaySky wait for it. What it turned, and what painting it cost there. */
	TFuture<void> SkyTurning;
	int32 SkyTurningWhat = 0;
	double SkyTurningSeconds = 0.0;
	/** Where the eye was last frame, on the board, and how far it moved the frame before (a cut: see SkyCut). */
	FVector SkyEyeWas = FVector(0.0, 0.0, -1e9);
	double SkyStepWas = 0.0;
	/** The line between the rooms as last painted (the log's; read once a turn is done). */
	double SkyLine = 0.0;
	/**
	 * A cut: the eye moves more than SkyCut in a frame (metres; a fast chase moves 2.5 m a frame) and more than
	 * SkyCutJump times what it moved the frame before. A glide's eased sweep (the flyover into the strategy view, up to
	 * 100 m a frame) is no cut: on build 1760 it was, and its 170 to 230 frames each painted the sky at once, the
	 * battle's slowest frames.
	 */
	static constexpr double SkyCut = 40.0;
	static constexpr double SkyCutJump = 4.0;
	/** A step past this is a cut whatever the frame before did (a cut straight after a cut; no glide goes 3 km a frame at 45 frames a second or more). */
	static constexpr double SkyCutFar = 3000.0;
	/** How many frames of the eye's step the painters' sweep covers for a turn painted aside (see DrawSky). */
	static constexpr double SkySweep = 8.0;
	/** "-TheaterSkyAtOnce": every turn painted on the game thread, as before round 11 (to time the two). */
	bool bSkyAtOnce = false;
	/** Waits for a turn painted aside, if one is under way, and sends it (bSend). */
	void SkyFinish(bool bSend);
	/** Sends the rooms a turn painted (What, AirSky::Rooms::Sent as a number) to the three meshes, from the points and
	 * colours made for the engine; returns the seconds it took this thread. */
	double SkySend(int32 What);
	/** The rooms' points and colours made for the engine (World's frame, FColor), for a turn painted (What): on the
	 * thread that painted it. */
	void SkyFill(FAirBattleState& S, int32 What);
	/** The clouds' shadows on the sea and the land: a mesh of their own, drawn under every other see-through thing (a
	 * shadow lies on the ground; the ground's fires and smoke, and the air fight, stand in front of it). */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> CloudShadows;
	TArray<FVector> ShadowPoints;
	TArray<FColor> ShadowColours;
	/** The sea and the land under a battle with no stage. */
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> Ground;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PlaneInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PlainInk;
	/** The ground's pictures under a battle with no stage (Battle/AirLand: fields, woods, villages, rivers, the town),
	 * painted on the pool's threads as the battle opens and laid over the plain sheets when done (LayLand). */
	TSharedPtr<FAirLandMade, ESPMode::ThreadSafe> LandMade;
	TFuture<void> LandPainting;
	double LandAsked = 0.0;
	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> LandPictures;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LandInks;
	/** The near picture's texels as laid, kept for the craters. */
	TArray<FColor> LandNear;
	/** A bomb's crater on the near picture where it fell on land (AirLand's Scar), sent with its smaller copies. */
	void ScarLand(double X, double Y, double Kilograms);
	/** An airscrew turning: a see-through disc, its blades a blur (M_Highlight, unlit, by each point's alpha). */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DiscInk;
	/** Turns a second an airscrew's blur is seen to make, a film's: "-TheaterScrewTurns=" tries another. */
	double ScrewTurns = 1.9;
	/** An aircraft's discs shown or not, with its near shape. */
	void ShowDiscs(FPlaneMade& Made, bool bShow);
	/** Which engine each of an aircraft's discs is (FPlaneMade::DiscEngine), from its model's engines across the span. */
	void TakeDiscEngines(FPlaneMade& Made, const AirShow::Shape& Shape);
	/** Each formation's engines, by group, and the kind of loop each was chosen as (B-17, fourEngined, ...). */
	UPROPERTY()
	TMap<int32, TObjectPtr<UAudioComponent>> Engines;
	TMap<int32, FName> EngineKind;
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
	/** Where the crowd it follows was last frame: the look is carried with it, and only a jump to another crowd eased. */
	FVector LookWanted = FVector::ZeroVector;
	// (from 2,600 m the aircraft were specks: the art director's note on the first Mac run; from 1,100 m still specks in
	// the strategy pictures, a B-17 50 pixels across, the art director on 2b177386's frames, 22 November 04:16Z)
	double Distance = 800.0, Yaw = 200.0, Pitch = -12.0;
	/** The aircraft the strategy camera's crowd is held on (AirDesk::CrowdOf); -1 none yet. */
	int FocusAnchor = -1;
	/** The strategy view as last framed (for the picture run's log): how far off, the crowd's middle span (metres; 0
	 * none found) and its groups. */
	double StrategyAway = 0.0, StrategySpan = 0.0;
	int32 StrategyGroups = 0;
	bool bLookSet = false;
	/** The close view: the eye's place and facing, and the ridden aircraft's way eased (what the eye trails along). */
	FVector CloseEye = FVector::ZeroVector;
	FRotator CloseFacing = FRotator::ZeroRotator;
	FVector CloseAhead = FVector::ZeroVector;
	/** The first ship to go down, and the ship the overview frames for "the ships" picture (-1: none). */
	int32 FirstSunk = -1;
	int32 ShipsLook = -1;
	/** "The ships" picture's eye in harbour, chosen once for the ship it looks at (ShipsEyeFor) and held to its end. */
	int32 ShipsEyeFor = -1;
	double ShipsEyeAway = 1600.0, ShipsEyeDown = -24.0, ShipsEyeHeading = 0.0;
	/** r.ScreenPercentage as the campaign had it, while "-TheaterAirSize=" holds its own (EndPlay puts it back); -1 none. */
	float KeptScreenPercent = -1.0f;
	/** Falls a kit is shedding debris for (burning or spinning), and the kit; blow-ups whose blast the kit's AirFall
	 * drew. */
	TMap<int32, FBattleEffects*> FallsOn;
	TSet<int32> BlastsTaken;
	/** Which kit each running gun and trail is in: chosen where it starts, a trail carried into the other kit as it
	 * passes the clouds' base (its kind and span kept to start it there). */
	/** A lit trail: the kit it is in, its kind and span, and when its kind began (on the air clock), so that passing
	 * from one kit to the other it carries on at its full look. */
	struct FTrailOn { FBattleEffects* Kit = nullptr; FString Kind; float Span = 0.0f; double KindT = 0.0; };
	TMap<int32, FTrailOn> TrailKits;
	TMap<int32, FBattleEffects*> GunKits;
	/** Each aircraft's nation (by its key), for its tracers' colour. */
	TMap<int32, FString> GunNations;
	/** Each ship's name (else her class), by her key, and the battleships and carriers (the guns-fire-back picture's
	 * ship and its log line). */
	TMap<int32, FString> ShipNames;
	TSet<int32> CapitalShips;
	// The sounds' own clocks.
	TMap<int32, double> GunHeardAt;  // a gun's last burst heard, real seconds
	TMultiMap<int32, TWeakObjectPtr<UAudioComponent>> GunTakes;  // the bursts a gun has playing, cut when it stops
	TMap<int32, double> PassHeardAt; // a fighter group's last pass heard
	double FlakHeardAt = -10.0, FlakGunsAt = 0.0;
	double ShipGunHeardAt = -10.0; // a ship's heavy gun's last boom heard
	/** The bombs landing heard (BombHit): when the last stick take began and how far off it was, and the last crump. */
	double StickHeardAt = -10.0, StickHeardMetres = 0.0, CrumpHeardAt = -10.0;
	struct FLater { double At; FName Name; FVector Where; FName Family; float Volume; float Cut; float From; };
	TArray<FLater> Later;
	TSet<FName> SoundsMissing;
	// The sound director's release 2 air sound (Brian, 21 November 16:20Z: "lets work on the sound effects more",
	// "when you behind the plane view make it louder and more intense", "when ever there is gun fire make sure the
	// sound of the guns happens").
	/** The whole battle's level now, as a gain: mix.json air.closeView.masterDb in the close view, else
	 * air.masterDbByCameraDistance by the eye's distance from the fight. Every take is laid on at it. */
	float MasterGain = 1.0f;
	/** The ridden aircraft's own engines, unplaced at the eye (air.closeView.bed), and which aircraft they are. */
	UPROPERTY()
	TObjectPtr<UAudioComponent> OwnEngine;
	int32 OwnEngineFor = -1;
	/** The far air bed (beds.list's air row, air_battle_far_loop), unplaced, by the fight's share (air.intensity). */
	UPROPERTY()
	TObjectPtr<UAudioComponent> AirBed;
	double AirIntensity = 0.0;
	/** Rounds passing the eye (air.roundsPassing): each firing aircraft's next crack due, real seconds (one aircraft's
	 * converging guns crack as one), and when each crack take last began (never again inside air.sameTakeGap). */
	TMap<int32, double> CrackHeardAt;
	TMap<FName, double> TakeStartedAt;
	/** A fighter's fixed guns of one kind are heard as one take (PlayEffects' wing key): each barrel's wing, and how many
	 * of a wing's barrels still fire, so its take is cut only when the last of them stops. */
	TMap<int32, int32> WingOf;
	TMap<int32, int32> WingLive;
	/** The master limiter at buses.master.limiterCeilingDb while the scene is up (taken off in EndPlay). */
	UPROPERTY()
	TObjectPtr<class USoundEffectSubmixPreset> Limiter;
	/** The guns' tally, logged every 20 s: bursts begun, takes laid on, takes that could not play. The Mac run's log
	 * shows every gun seen firing was heard. */
	int32 GunBursts = 0, GunTakesLaid = 0, GunTakesLost = 0, GunTakesFar = 0, CracksHeard = 0;
	double GunTallyAt = 20.0;
	/** A take unplaced at the eye (the ridden aircraft's own guns and engines): heard at Volume whatever the
	 * distance rules, and never the one the engine's voice limit drops. */
	UAudioComponent* SoundAtEye(const TCHAR* Name, float Volume, float Cut = 0.0f, bool bLoop = false, float Priority = 4.0f);
	/** Makes a take's sound, gives it its voice priority (0: the recording's own), and only then begins it: Play hands the
	 * priority to the engine, so one set after it reaches nothing. At *Where by its distance family, or unplaced at the
	 * eye when Where is null. Null when the recording is missing, or Where is past its family's silent distance. */
	UAudioComponent* Lay(USoundBase* Played, const FVector* Where, USoundAttenuation* Family, float Volume, float From, float Priority, float PitchBy = 1.0f, bool bAutoDestroy = true);
	/** The level of the whole battle for this frame (MasterGain), the ridden aircraft's engines and the far bed. */
	void AirLevels(double Real);
	/** Big moments (mix.json big, air.bigDuck): a plane going down near the eye, a flak burst within 20 m, a stick of
	 * bombs within 120 m plays at its full level and the rest dips for a moment (attack, hold, release), each layer by
	 * its big.duckDb. DuckFrom: real seconds the latest began (-1 none); DuckLevel 0 to 1, now. */
	double DuckFrom = -1.0, DuckLevel = 0.0;
	float DuckBedGain = 1.0f, DuckLoopsGain = 1.0f, DuckCloseGain = 1.0f;
	double DuckMusicDb = 0.0;
	/** Whether a take laid at Where is a big moment (and so dips the rest, and is not dipped itself). */
	bool BigMoment(const TCHAR* Name, const FVector& Where);
	/** The most engines each formation has flown with (air.engineLost: its drone thins as they go). */
	TMap<int32, int32> GroupEnginesMost;
	/** Rounds striking a plane (air.roundsHit): the last heard, real seconds. */
	double HitHeardAt = -1.0;
	/** The battle's end (mix.json music.end): the battle's sounds come down under the stinger. */
	float EndGain = 1.0f;
	/** The battle's music as this scene drives it (mix.json music; ACampaignGameMode::BeginBattleScore): begun, first
	 * contact passed, the stinger asked for; the level it is making for and the level now, in dB under the setting. */
	bool bScoreBegun = false, bScoreContact = false, bScoreEnded = false;
	double ScoreWantDb = 0.0, ScoreShownDb = 0.0, ScoreRampDbPerS = 10.0, LullSince = -1.0;
	void BattleScore(double Real);
	/** An aircraft's own engine loop by its model (mix.json air.loops), its kind's when its own is not in the game. */
	FString EngineLoop(const FString& Model);
	/** The sound director's mix (Content/Data/Sound/mix.json). */
	TSharedPtr<FJsonObject> Mix;
	/** A blast's torn flame, streaming on with the wreck for a moment. */
	struct FTorn { int32 Key = 0; FVector At = FVector::ZeroVector; FVector Velocity = FVector::ZeroVector; double Until = 0.0; FBattleEffects* Kit = nullptr; };
	TArray<FTorn> Torn;
	int32 TornNext = 0;
	/** The next bomb burst's id for the kit's BombStick (AirParts::BombBurst): one stick a bomb, its id free the frame after. */
	int32 BombNext = 0;

	// What the table had before the battle took its place, put back by Finish: its lamps, its haze, its own tick and
	// its pieces (the pre-check, 06:08Z).
	struct FKeptLamp { TWeakObjectPtr<UDirectionalLightComponent> Lamp; FRotator Rotation; float Intensity = 0.0f; FLinearColor Colour; float Temperature = 0.0f; float SourceAngle = 0.0f; FLinearColor SunDisc = FLinearColor::White; bool bAirByHeight = false; TWeakObjectPtr<UMaterialInterface> LightFunction; float LightFunctionFade = 0.0f, LightFunctionDisabled = 0.0f; };
	struct FKeptSky { TWeakObjectPtr<USkyLightComponent> Sky; float Intensity = 0.0f; FLinearColor Colour; bool bLowerBlack = false; FLinearColor LowerColour = FLinearColor::Black; };
	struct FKeptHaze { TWeakObjectPtr<UExponentialHeightFogComponent> Haze; float Start = 0.0f, Density = 0.0f, Falloff = 0.0f, MostOpacity = 0.0f; FLinearColor Colour; };
	TArray<FKeptLamp> KeptLamps;
	TArray<FKeptSky> KeptSkies;
	TArray<FKeptHaze> KeptHazes;
	TArray<TWeakObjectPtr<ACampaignEnvironment>> KeptRooms;
	TArray<TWeakObjectPtr<AActor>> KeptPieces;
	TWeakObjectPtr<ACampaignMapActor> Table;
	TWeakObjectPtr<UDirectionalLightComponent> BattleKey;
	bool bTableHidden = false;

	// The picture run (-TheaterAirBattleShots).
	FString ShotFolder;
	/** bOther: a ride picture that rides the other side's aircraft (the art director on the Bremen film, build 1778:
	 * both rides fell on a Mustang and the German fighters were small in every frame). bChase: one in the chase view,
	 * taken as he would take it (his first fighter squadron ridden, then one step along it), its plate in the picture.
	 * bGuns: "the guns fire back", timed from the first attack run on a ship (GunsRealT). */
	struct FShot { FString Name; double At; bool bClose; bool bShips; bool bTaken; bool bOther = false; bool bChase = false; bool bGuns = false; bool bMarks = false; bool bScrew = false; bool bChutes = false; };
	TArray<FShot> Shots;
	double MetRealT = -1.0, ShipRealT = -1.0;
	int32 ShotsTaken = 0;
	double QuitAt = -1.0, CloseOffAt = -1.0, CardShotAt = -1.0, ShipsLookOffAt = -1.0;
	/** The first attack run on a ship: when her light guns were first laid on an attacker within GunsReach of her
	 * (real seconds; -1 not yet), the ship and that aircraft. GunsLook: the ship the overview frames for the picture
	 * (-1 none), seen side-on to the line from her to him (its way chosen once, GunsEyeFor, and held); GunsPlane: where
	 * he is, or was last seen. */
	static constexpr double GunsReach = 1600.0;
	double GunsRealT = -1.0, GunsLookOffAt = -1.0;
	int32 GunsShip = -1, GunsOn = -1, GunsLook = -1, GunsEyeFor = -1;
	FVector GunsPlane = FVector::ZeroVector;
	double GunsEyeHeading = 0.0;
	/** The dead-engine picture's ride (the art director on a1acb552's frames, item 8: "windmilling airscrews and
	 * parachutes: the picture run needs a shot of each"): the aircraft, and the side its dead engine is on (1 its right,
	 * -1 its left), the bomber's eye on that side. */
	int32 ScrewEyeKey = INDEX_NONE;
	double ScrewEyeHand = 1.0;
	/** The parachutes picture: when three or more open canopies first hung within 1.5 km of each other (real seconds;
	 * -1 not yet), where; and whether the overview is on them now (ChutesLook), until ChutesLookOffAt. */
	double ChutesRealT = -1.0, ChutesLookOffAt = -1.0;
	FVector ChutesWorld = FVector::ZeroVector;
	bool bChutesLook = false;
	/** Real seconds a ride picture's ride is held after it ("-TheaterAirBattleRideHold=40", 0.5 to 300; a film stays
	 * behind the fighter through the fight). */
	double RideHold = 0.5;
	/** The close view rides the other side's aircraft, not his own (a picture run's ride picture with bOther). */
	bool bRideOther = false;
	bool bCardShot = false;

	bool Open(const FString& Why);
	void Raise();
	void LayGround();
	/** The ground's pictures begun on the pool's threads (LayGround, no stage). */
	void PaintLand();
	/** The pictures, painted, laid as sheets over the plain ones: the finest round the aim, each coarser one round that,
	 * the plain colour of the far picture's edge out to the sea sheet's rim. */
	void LayLand();
	/** The day's clouds laid round the target (AirSky) and their mesh made; once they are drawn, the deck handed to the
	 * flight core (where falls begun above it pass out of sight). */
	void LaySky();
	/** The clouds moved with the wind and, when the eye has moved enough, turned to it again; the see-through things
	 * put in order for the eye (over the clouds or under them). */
	void DrawSky();
	/** The sky's deep blue looking up (LayZenith, ZenithAt). */
	void LayZenith();
	void ZenithAt(const FVector& EyeWorld);
	/** Light added to the sky above 12 degrees, deepest blue at the zenith, on a bowl round the eye drawn behind every
	 * other see-through thing (M_Glow): high up, under little air, the engine's sky looking up went near black through
	 * the battle's grade (the art director on the Bremen film, build 1778: luma 29; her mark a deep blue at 90 to 110;
	 * on build 1780, at 0.3 from 25 degrees: 43 to 50, a dusk). Its strength grows with the eye's height above the
	 * low haze, none below 1,000 m and all of it at 7,500 m. "-TheaterZenith=1.3,12" tries another strength and lowest
	 * angle (0 turns it off). */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Zenith;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ZenithInk;
	double ZenithStrength = 6.0, ZenithFrom = 12.0, ZenithShown = -1.0;
	/** The haze band over the horizon (LayHazeBand, HazeBandAt). */
	void LayHazeBand();
	void HazeBandAt(const FVector& EyeWorld);
	/** The day's light where the eye is (AirSky::LightOf: under a deck, the sun dimmed by its cover and the sky's light
	 * greyer), as shares of a clear day's the lamps are lit to in LightTheBattle. */
	void WeatherAt(const FVector& EyeWorld);
	double WeatherSun = 1.0, WeatherSky = 1.0;
	/** The key's temperature: the table's 4,900 K over a stage, white over land with no stage (LightTheBattle). */
	float KeyWarmth = 4900.0f;
	/** The light thrown up from under the eye (the sky light's lower half): the sunlit cloud tops when the eye is over
	 * them, so the bombers' bellies are not black over a bright deck (the art director, round 18d); the dark sea or
	 * ground under the cloud. WeatherAt sets it, LightTheBattle gives it to the sky light when it has moved. */
	double WeatherBounce = 0.0, BounceShown = -1.0;
	/** The light thrown up on the aircraft alone (their lighting channel 1, no other thing's): a shadowless lamp shining
	 * straight up, as bright and as coloured as what lies under the eye, the sunlit deck or the dark sea, by
	 * WeatherBounce. The sky light's lower half did not reach them on the Mac: Bremen 09's B-17 wing 20 to 26 over a
	 * deck of 227, where a bright deck lifts the undersides to about 80 to 110 and the dark sea leaves them at 40 to
	 * 60 (the art director on a1acb552's frames, 22 November 08:13Z). Made in LightTheBattle, taken down with the
	 * battle; "-TheaterUnderLight=1" scales it, 0 none. */
	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> UnderLight;
	/** The UnderLight's strength last told the log (-1 none yet this battle). */
	float UnderLogged = -1.0f;
	/** The sky behind the smoke last sent to the air kit and the ground kit (Effects->SetSky; none yet: below 0). */
	FLinearColor SmokeSkySent = FLinearColor(-1.0f, -1.0f, -1.0f), GroundSkySent = FLinearColor(-1.0f, -1.0f, -1.0f);
	/** The deck's shade on all under it (AirSky::ShadeOf), the key light's light function M_DeckShade: under a deck
	 * the key keeps its whole strength, the shade taking the sun away point by point (the art director, 22:37Z). */
	void LayDeckShade();
	/** The deck carried on past the drawn tiles to the horizon as one smooth sheet (the art director, 22:37Z: the far
	 * deck melting into a smooth pale sheet; under an overcast no bright band under its far edge): pale from over it,
	 * the base's grey from under it. */
	void LayDeckSheet();
	void DeckSheetAt(const FVector& EyeWorld);
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> DeckSheet;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DeckSheetInk;
	/** A clear day's thin cirrus 8 to 10 km up, in streaks along the wind aloft, and the odd grey rain shaft under a
	 * heavy day's biggest clouds (the art director, 22:37Z: the kinds of day differ in character). */
	void LayCirrus();
	void LayRainShafts();
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Cirrus;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CirrusInk;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> RainShafts;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RainInk;
	double DeckSheetZ = 0.0, DeckSheetShown = -2.0;
	UPROPERTY()
	TObjectPtr<UTexture2D> DeckShadeSheet;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DeckShadeInk;
	bool bDeckShade = false;
	/**
	 * A band of haze standing round the eye between it and the sea's far edge, over the line where the edge meets the
	 * sky, drawn behind every other see-through thing (M_Highlight, the haze's own colour, the height fog over it as
	 * over the sea behind it): high up the sea's flat sheet ends short of the round world's horizon, and from bomber
	 * height its far edge showed as a hard straight line across the sky's foot, the hazed sea below it lighter than
	 * the thin-aired sky above (the art director on build 1778's Bremen 01: the haze should swallow the horizon). It
	 * comes in from none below the edge to all of it just over it and thins away 7 degrees up: the pale band low on the
	 * horizon of the trade-cumulus brief (item 3), now that the haze lies over the sky at a third (BattleLook's
	 * AirSkyBegin). It is none with the eye under HazeBandFrom metres, all of it over HazeBandFull (by default all of
	 * it at any height). "-TheaterHazeBand=1,0,1" tries another strength (the colour's brightness against the haze's;
	 * 0 turns it off) and heights.
	 */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> HazeBand;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HazeBandInk;
	double HazeBandStrength = 1.0, HazeBandFrom = 0.0, HazeBandFull = 1.0, HazeBandShown = -1.0;
	/** Where the band was laid for (world), and the sea's square it stands inside (world middle, half its side). */
	FVector HazeBandLaidAt = FVector(1e30, 1e30, 1e30);
	FVector SeaMiddle = FVector::ZeroVector;
	double SeaHalf = 0.0;
	/** The kit for an effect at a world point: the ground's under the clouds' base as drawn there (lifted to 300 m
	 * over the ground under it), the air's above it. Given the kit something is in now, it changes only 50 m past the
	 * base, so a trail along the base does not flicker between them. */
	FBattleEffects* KitAt(const FVector& WorldAt, const FBattleEffects* InNow = nullptr) const;
	/** The kit for what happens on the ground or the sea: the ground's, once the clouds are drawn. */
	FBattleEffects* GroundKit() const;
	void HideTheTable();
	/** Every frame the battle is up: a piece the table stood again (its crowds unfolding under the pointer) is put away. */
	void KeepTheTableHidden();
	void ShowTheTable();
	/** The battle's day light on the table's lamps, said again wherever the room has changed it. */
	void LightTheBattle();
	/** The sun a hard small disc while the battle stands (the art director, build 1750); the lamp kept, put back with the table. */
	void TheSun(UDirectionalLightComponent& Key);
	/** Toward the battle's sun, a unit vector in the world: the set piece's own sun where its stage stands; over a raid with
	 * no stage the sun of its place and month at half past noon (AirLand::SunAt; place.lat); with neither, mid-morning
	 * from the south-east. */
	FVector TowardSun() const;
	void Feed();
	void Draw(double Real);
	void Play(double Real);
	void PlayEffects();
	void AimEye(double Real);
	void PlaceTags();
	/** A one-shot at a world point, in a distance family of the mix; After: held that many real seconds; Cut: only the first seconds of the take. */
	/** A one-shot at Where in its mix family: After seconds from now, cut off Cut seconds in, begun From seconds into its take. */
	UAudioComponent* Sound(const TCHAR* Name, const FVector& Where, const TCHAR* Family, float Volume = 1.0f, float After = 0.0f, float Cut = 0.0f, float From = 0.0f, float Priority = 0.0f);
	USoundBase* SoundOf(const TCHAR* Name, bool bLoop = false);
	USoundAttenuation* FamilyOf(const TCHAR* Name);
	void EngineSounds();
	void TakeShots(double Real);
	/** The picture run's log line for a strategy picture: how it was framed and where the aircraft stood from the look. */
	void StrategyShotLog(const FString& Name) const;
	bool Order(const FString& Command);
	FVector World(double X, double Y, double Z) const;
	FVector WorldVector(double X, double Y, double Z) const;
	bool BoardUnder(const FVector2D& Pixel, double Height, FVector2D& OutBoard) const;
	int32 TagUnder(const FVector2D& Pixel) const;
	/** The close view on, riding aircraft Key of squadron Group, chosen by him (kept until he steps or it is gone). */
	void RideOn(int32 Group, int32 Key);
	FString ModelFor(const FString& Model, const FString& Nation, bool bFighter) const;
	UProceduralMeshComponent* PlaneMesh(int32 Key, const FString& Model, const FString& Nation, bool bFighter, double Span);
	/** Shows aircraft Key's near shape (made the first time) or its far one. */
	void PlaneNear(int32 Key, UProceduralMeshComponent* Mesh, bool bNear);
	void ShowScreen();
	void HideScreen();
	void Finish();
};
