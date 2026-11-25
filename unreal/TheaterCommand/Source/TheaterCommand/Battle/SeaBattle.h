#pragma once

#include "CoreMinimal.h"
#include "Effects/BattleEffects.h"
#include "Battle/ConvoyLooks.h" // D1 hook (convoy looks)
#include "Battle/SeaUnder.h" // U1 hook (under water)
#include "Battle/SeaWeather.h" // the clouds and the weather (step 13)
#include "Battle/SeaFrames.h" // (the release 2 ocean, step 0: the frames by thread)
#include "Battle/SeaLookSwitches.h" // (the release 2 ocean, step 0: the sea's look in force)
#include "Battle/SeaMarksPool.h" // (the release 2 ocean, step 1(a): DrawMarks' meshes kept)
#include "Battle/HeavyWeather.h" // S6 hook (heavy weather: bow spray and ship ice)
#include "Battle/SeaNight.h" // (the release 2 ocean, step 11: night fighting's looks, -TheaterSeaNightFight=)
#include "GameFramework/Actor.h"
#include "SeaBattle.generated.h"

class AAirStage;
class ACameraActor;
class ACampaignEnvironment;
class FJsonObject;
class UAudioComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class USkyLightComponent;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UTexture2D;
class SSeaBattleScreen;
class SWidget;
struct FSeaBattleState;

/** The sea battle camera's kinds of shot (ASeaBattle::ShotEye). */
enum class ESeaShot : uint8 { Fleets, Line, Broadside, Rangefinder, Hit, Tracks, Strike, Sinking, Overview, Burning, Funnels, Water, BothLines, Under /* U1 hook (under water) */ };

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
 * "-TheaterSeaShowAll" (every boat drawn: else an enemy boat is drawn only once the player's ships have found her),
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
	/**
	 * (D1, ruling 241) The month's submarine scene inside the campaign: a staged recording's text (TC.subScene's
	 * `recording`), played through its windows (field.staged.beats, each at (to - from) / screen, cut between), its
	 * caption (the recording's after lines) over the last window and a few seconds after, then closed, the table given
	 * back. A key or a click skips it at any moment. The picture run's and the test's flags are not read. False, with a
	 * warning, where the text does not read (nothing taken from the screen then).
	 */
	bool BeginMonthScene(const FString& Text, const FString& Name);
	/** The month's scene is playing (BeginMonthScene). */
	bool IsMonthScene() const { return bMonthScene; }
	/** The month's scene's caption as the screen shows it now: its lines, and how far it has come up (0 to 1); false
	 * while none is shown. */
	bool GetCaption(TArray<FString>& OutLines, float& OutShown) const;
	/** The month's scene skipped (a key or a click on its screen): it closes on the next frame. */
	void SkipMonthScene() { bMonthEnding = bMonthScene; }
	/** U1 hook (under water): the month scene's U and Shift+U, handed over by its screen (which takes every key there). */
	void UnderKey(bool bShift);

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
		int32 Other = -1;        // the ship she fires on (Broadside), her piece (Fleets), the side (Line), 1 for a salvo
		                         // from a boat the player has not found (Tracks: a lookout's view), or -1
		FVector2D Spot = FVector2D::ZeroVector; // board metres: where it happens (a splash, a track's start, an oil fire)
		FVector2D Way = FVector2D::ZeroVector;  // a track's way; a ridden shot's (BothLines) speed, board metres a second
		/** A shot PlanShots weighed whole (BothLines): the eye and the point it looks at as it cuts in (board metres, Z up),
		 * the lens, and how long it holds (seconds from the cut) before the camera goes up to the whole battle. */
		FVector EyeFrom = FVector::ZeroVector, LookAt = FVector::ZeroVector;
		double Lens = 0.0, Hold = 0.0;
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
	TUniquePtr<FConvoyLooks> SubLooks; // D1 hook (convoy looks)
	TUniquePtr<FHeavyWeather> Weather;  // S6 hook (heavy weather): the bow spray, made only under -TheaterSeaSpray=1
	TUniquePtr<FSeaUnder> Under;       // U1 hook (under water)
	TUniquePtr<FSeaWeather> SkyWeather; // the clouds and the weather (the release 2 ocean plan, step 13)
	int32 UnderWindow = INDEX_NONE;    // U1 hook (under water): the window AimEye stands in (FSeaUnder::ByHandWindow his own dive)
	FSeaNight Night;                   // (the release 2 ocean, step 11: the searchlights and the fleet's star shells; off, it draws nothing)

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
	/** Where ships went down (DrawWrecks): their rafts, boats and wreckage and their men in the water, solid and lit on
	 * the pieces' material; near the eye laid every frame, throwing shadows, the rest twice a second. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> WreckNear;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> WreckFar;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WreckInk;
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
	// The wrecks as drawn: each wreck's navy (the kit's rows) and the merchant ships among them; how black with oil each
	// man in the water is (0 to 1); the floats and men in the far mesh as last laid, and when; the battle time last drawn;
	// the most points laid in a frame.
	TMap<int32, FString> WreckNavy;
	TSet<int32> WreckMerchants;
	TMap<int32, float> OilOnMen;
	TSet<int32> FarFloats, FarMen;
	// (the men going over the side now, and when each landed in the water: his splash, DrawMarks)
	TSet<int32> MenJumping;
	TMap<int32, double> MenLanded;
	double WrecksFarAt = -100.0, WrecksDrawnAt = 0.0;
	int32 WrecksMostDrawn = 0;
	/** The near wreck mesh's shape as last made: its points' count, then its triangles (DrawWrecks sends only the points
	 * again while it holds). */
	TArray<int32> WreckNearShape;
	/**
	 * The men in the water drawn as the sailor files' men (Battle/SeaSailors, DrawWrecks): the nearest few skinned afresh
	 * every frame, each a mesh of his own put where he is (SailorLive, and whom each shows and what its triangles and
	 * colours were made for); the rest as copies of a pose bank's meshes, one a file, dress, act, pose, level and hand,
	 * made as first needed (SailorCopies by SailorCopyOf's key, -2 where it cannot be made), each man's copy and slot, the
	 * slots put away to be used again, and the copies moved this frame.
	 */
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> SailorLive;
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> SailorCopies;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> SailorMeshes;
	struct FLiveSailor
	{
		int32 Man = 0;
		int64 Made = -1;
	};
	TArray<FLiveSailor> SailorLiveOf;
	struct FBankedSailor
	{
		int32 Copy = -1, Slot = -1;
		FVector At = FVector::ZeroVector;
		double Yaw = 0.0;
		bool bSeen = false;
	};
	TMap<int32, FBankedSailor> SailorBanked;
	TMap<int64, int32> SailorCopyOf;
	TArray<TArray<int32>> SailorFree;
	TArray<uint8> SailorDirty;
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
	/** A cut is told to the renderer (so the last shot's smoke, wakes and light are not carried a frame into the new one):
	 * owed on the first picture and after a skip; and the ticks since the last one, so that a picture waits for its
	 * shot to settle. */
	bool bEyeCut = true;
	int32 TicksSinceCut = 0;
	/** Where each ship last stood (board metres) and her heading, for a shot on a ship the show no longer draws. */
	TMap<int32, FVector4> LastSeen;

	// The picture run.
	FString ShotFolder;
	int32 NextPicture = 0;
	double PictureAt = -1.0;
	double QuitAt = -1.0;
	bool bSkipping = false;
	int32 PicturesTaken = 0;
	/** How long each frame took (seconds), the battle's own from its second second on (not a picture run's skips, nor a
	 * picture's own frame: those counted apart), for the frame-time line at the end (SayFrameTimes); whether the last
	 * frame was one of those; and whether the line is said. */
	TArray<float> FrameTimes;
	/** (the real clock at the last tick, FPlatformTime: the engine holds a tick's DeltaSeconds to 0.4 s at the most,
	 * so a stall would read short; the pre-check, 22 November) */
	double LastTickClock = -1.0;
	int32 FramesLeftOut = 0;
	bool bLastFrameApart = false;
	bool bFrameTimesSaid = false;
	/** (the release 2 ocean plan, step 0: measure first) The same frames by thread, the game's, the renderer's, the RHI's
	 * and the graphics chip's, with the whole frame and the effects kit's ground asks in each (GroundAsks, counted in its
	 * ground), from ThreadsFrom seconds of play, for the threads' line and the "Sea look:" line after the frame-time line
	 * (SayThreadsAndLook). "-TheaterSeaBench=drop,span" (Tools/sea_bench.sh) counts them from its drop and quits span
	 * seconds later (BenchUntil), or at the battle's end ("drop,end": there). SeaLook: the sea's look as OpenOn applied
	 * it, which the "Sea look:" line says. */
	SeaFrames::Record Threads;
	int32 GroundAsks = 0;
	double ThreadsFrom = 1.0;
	double BenchUntil = -1.0;
	SeaLookSwitches::Look SeaLook;
	/** (the release 2 ocean plan, step 1(a), pay first) -TheaterSeaMarksPool=1: DrawMarks' two meshes each made afresh only
	 * when its shape changes, its points sent again in place while the same points and triangles hold, and cleared once when
	 * it has nothing (Battle/SeaMarksPool.h: each one's shape as last made); by default, until the Mac's A/B, today's: both
	 * meshes made afresh every frame. */
	bool bMarksPool = false;
	SeaMarksPool::Shape FoamShape, MarksShape;
	TArray<FOilLater> OilLater;
	TArray<FFlakLater> FlakLater;
	TArray<FLater> Later;
	/** A take filtered by the family's low-pass table, its filter set again each frame from where the eye is now (a cut
	 * from far to near opens it; the release 2 sound check, 25 Nov). */
	struct FFiltered
	{
		TWeakObjectPtr<UAudioComponent> Player;
		FName Family;
		FVector Where = FVector::ZeroVector;
	};
	TArray<FFiltered> Filtered;
	void RefilterSounds();
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

	/** The scene opened on a recording's text (BeginReplay's file, or BeginMonthScene's); Where names it in a warning. */
	bool OpenOn(const FString& Text, const FString& Name, const FString& Where);
	/** The show on to battle time To at once, keeping what lasts (ships gone down, oil burning), the camera cut: a
	 * picture run's skip, or the month's scene's cut between its windows. */
	void SkipTo(double To);
	/** The month's scene (BeginMonthScene): its windows played in turn, the pace of the one in hand, the caption and
	 * the end. Each tick before the show is stepped; true where it cut ahead. */
	bool RunMonthScene();
	/** The month's scene closed (its end, or a key's): the screen, the keys and the pointer, the table, the music and
	 * the campaign's own eye given back, the scene taken down. */
	void EndMonthScene();
	bool bMonthScene = false;
	/** The window in hand (field.staged.beats); when the caption came up and when the scene ends (the real clock); the
	 * caption's lines; whether the scene holds the keys and the pointer (its screen focused), and the table put away. */
	int32 BeatAt = 0;
	double CaptionAt = -1.0, MonthEndsAt = -1.0;
	TArray<FString> Caption;
	bool bOwnInput = false;
	bool bTablePutAway = false;
	bool bMonthEnding = false;

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
	/** (the release 2 ocean plan, step 1(a)) DrawMarks' end under -TheaterSeaMarksPool=1: the frame's foam (FV...) and marks
	 * (DV...) sent to their meshes, each kept while its shape holds, the marks taking the foam too where there is no wake
	 * ink, as today. */
	void DrawMarksPooled(const TArray<FVector>& FV, const TArray<FVector>& FN, const TArray<FVector2D>& FU, const TArray<FColor>& FC, const TArray<int32>& FT,
		TArray<FVector>& DV, TArray<FVector>& DN, TArray<FColor>& DC, TArray<int32>& DT, bool bInk);
	/** One mesh's frame sent: made afresh when its shape is new or changed (as today), its points sent again in place while
	 * the shape holds, cleared once when there is nothing to draw. U: the frame's UVs, or none (the marks' mesh has none). */
	void PoolMarks(SeaMarksPool::Shape& Into, UProceduralMeshComponent* Mesh, const TArray<FVector>& V, const TArray<int32>& T, const TArray<FVector>& N, const TArray<FVector2D>& U,
		const TArray<FColor>& C, UMaterialInterface* Ink);
	/** Where ships went down: rafts, boats, wreckage and the men in the water (SeaShow's Floats and Men). */
	void DrawWrecks();
	/** The pose bank's copies for one mesh (SeaParts::SailorKey), made if Builds allows (-1 not yet, -2 never). */
	int32 SailorCopy(int64 Key, int32 File, int32 Dress, int32 What, int32 Pose, int32 Lod, bool bMirror, bool bOiled, int32& Builds);
	/** A banked man's copy put away (shrunk where he was, its slot kept for the next). */
	void SailorAway(FBankedSailor& Man);
	void RunTrails();
	/** The show played through on its own once (it is the same show every time), to find its moments: the shots. */
	void PlanShots();
	void AimEye();
	bool ShotEye(const FShot& Shot, FVector& OutEye, FVector& OutLook, double& OutLens);
	void TakePictures();
	void Quit(const FString& Why);
	/** The frame-time line (the production lead, 22 November: the real frame cost of each battle kind): how many frames,
	 * and their mean, middle, 95th and 99th in the hundred and worst times, once a battle. */
	void SayFrameTimes();
	/** (the release 2 ocean plan, step 0) The frames by thread (Battle/SeaFrames) and the sea's tier and switches in force
	 * (Battle/SeaLookSwitches), each on a line of its own, after the frame-time line. */
	void SayThreadsAndLook() const;
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
