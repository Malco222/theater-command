#pragma once

#include "CoreMinimal.h"
#include "Effects/BattleEffects.h"
#include "GameFramework/Actor.h"

#include "LandBattle.generated.h"

class ACameraActor;
class ACampaignEnvironment;
class ACampaignMapActor;
class FJsonObject;
class FLandCrowd; // TK1
class FLandLive;
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
namespace LandCarpet
{
	class Squares;
}
namespace LandKit
{
	class Model;
}
struct FLandSky;

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

	/**
	 * "-TheaterLandLive=<key>" on the command line (normandy-1944): the set piece fought live, the player its general
	 * (release 2; Brian, 22 November: "im the general who controls their deployment and who to attack and move on a map
	 * with tactics"). "-TheaterLandSide=" says his side: att (the attack, the default), def, or none (a film, the staffs
	 * commanding both sides); "-TheaterLandSeed=<n>" a seed of the piece's own; "-TheaterLandNoDeploy" skips the setup step.
	 */
	static bool WantsLive(FString& OutKey);
	/** Opens a live battle two seconds in, as ReplayWhenReady does, its side and seed from the command line. */
	/** A live battle opened from the game's own menus (the scenarios page's practice battle): Me "att", "def" or "" (a
	 * film), Seed 0 the battle's own. */
	void LiveWhenReady(const FString& InKey, const FString& Me, int32 Seed, bool bDeploy);
	void LiveWhenReady(const FString& Key);
	/** Starts the rules on their own thread (LandLive) on the set piece Key, the player on side Me ("att", "def", or ""
	 * for a film), deploying first when bDeploy; the scene opens when the rules have opened the battle. False if not. */
	bool BeginLive(const FString& Key, int32 Seed, const FString& Me, bool bDeploy);
	bool IsLive() const { return Live.IsValid(); }
	/**
	 * The general's word to the rules (LandCommand): an order (TC.landOrder's command as JSON, given at the battle time
	 * shown unless it says its own "at"), a block placed in the setup step (Face in radians, 0 north, clockwise), the setup
	 * step ended. The rules answer within a frame or two; a refusal is said on the screen in their own words.
	 */
	void OrderLive(const FString& CommandJson);
	void PlaceLive(int32 Block, double X, double Y, double Face);
	void BeginFight();
	/** The scene's state for the general's screen to change (his hand, what is armed): the screen's own. */
	FLandBattleState* EditState() { return State.Get(); }
	/**
	 * The general's pointer (LandBattleCommand.cpp), in the viewport's pixels: pressed (Button 0 the left, 1 the right),
	 * moved, let go; OnTag the block whose tag it is on (-1 none). A click picks his block up or gives the order in hand;
	 * a drag draws a box round his blocks, the right a line for them, and in the setup step carries one of his blocks.
	 */
	void GeneralPress(const FVector2D& Pixel, int32 Button, bool bShift, int32 OnTag);
	void GeneralMove(const FVector2D& Pixel, int32 OnTag);
	void GeneralRelease(const FVector2D& Pixel, int32 Button, bool bShift, int32 OnTag);
	/** His word from a button or a key: double (the next order at the double), fire (fire on), hold, fallback, staff,
	 * next, back (Tab, Shift-Tab), clear (Esc), film (C), map (Esc from the film), begin. */
	void GeneralDo(const FString& Word);

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

	/** The paint: vertex colours on the campaign's surface (sRGB in, made linear there); the ground tiles on the battle's
	 * own ground surface (M_LandSurface: the same colours with the ground's grain), or on the campaign's without it. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GroundInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ThingInk;
	/** The hedges' and trees' leaves on their cards (M_LandLeaves: cut out, both faces lit; build_content.py); none
	 * without it, and then the shrubs and trees carry their solid sprigs instead. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> LeafInk;
	/** The corn's stalks near a low lens (M_LandStalks: lit as foliage, faded in past the lens and out where the bundles
	 * take over; build_content.py); none without it, and then the corn keeps its bundles to the lens. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> StalkInk;
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
		/** Once a hole is dug in it, its near ground by parts (LandCrater::Dug), drawn in Near's place. */
		TArray<TWeakObjectPtr<UProceduralMeshComponent>> NearParts;
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
	/** Each base's two shade edges (the brightness of an instance's tint, R + G + B), and for the clutter each
	 * brightness third's two warmth edges (R - G; WarmEdges, three a kind): a thing's shade is its brightness third, and
	 * the clutter's is that third split again in thirds by warmth, nine a kind, so that a blade of grass takes about its
	 * own ground's hue (a hay field's straw yellow, a lush one's green) and not the whole field's mean. */
	TArray<FVector2f> ShadeEdges;
	TArray<FVector2f> WarmEdges;
	/** How far from the eye each clutter kind is laid, metres: where it is no longer drawn (its cull) and 12 m more. */
	TArray<float> ClutterReach;
	/** The grass, corn, flowers and stones round the eye and round what it looks at (LandField::Clutter), laid in 16 m
	 * squares as they come into reach and given back as they leave it (Carpet): one instanced mesh a clutter kind and
	 * shade (ClutterCopyOf, -1 none). */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ClutterCopies;
	TArray<int32> ClutterCopyOf;
	TSharedPtr<LandCarpet::Squares> Carpet;
	/** The corn drawn stalk by stalk round the lens (the art director's cut 4, fix 4): whether the stalks' meshes were
	 * made (StalkInk), and whether the eye is low enough for them now (they are shown, and the bundles cleared within
	 * StalkClear of it, only then: from above, a clearing would be a bald ring in the corn). */
	bool bStalksMade = false;
	bool bStalksShown = false;
	/** The shell holes (LandCrater): torn earth on the ground's own paint, one mesh a patch (a 64 m square of the
	 * field, by its number), one section for each 24 of its holes. */
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> CraterMeshes;
	/** The track marks (LandCrater::Ruts), the same way: one mesh a 64 m patch, a section for each 48 pieces, sent again
	 * a quarter second after a piece is laid in it. */
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> RutMeshes;
	double RutsSentAt = -10.0;

	// ---- The men (LandMen) ----
	struct FManDrawn
	{
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		int32 Lod = -1;
		uint32 Stamp = 0xFFFFFFFFu;
		/** Whether his mesh casts a shadow now (made with one; none past about 80 m as seen). */
		bool bShadow = true;
		/** Banked far (SO1): his place among the instances of his set's pose (BankCopies; -1 none), and the frame it was
		 * last put at (his WorldFrame's R and T), so a man standing still is not put again. */
		int32 BankCopy = -1, BankSlot = -1;
		float BankFrame[12] = {};
	};
	TArray<FManDrawn> MenDrawn;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> MenMeshes;
	TArray<float> SkinAt, SkinFacing;

	// ---- The far men as instances of the soldier pose bank (the true-scale plan's SO1) ----
	/** One instanced mesh for each (set, pose) shown (LandMen::Crowd::BankMesh), made as first needed, a few a frame;
	 * its places given back are shrunk to nothing (BankFree) for the next man. BankCopyOf: set * poses + pose to the
	 * mesh's number (-2: cannot be made, the man skinned as before). Off with -TheaterLandNoPoseBank. */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> BankCopies;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> BankMeshes;
	TArray<TArray<int32>> BankFree;
	TArray<uint8> BankDirty;
	/** How many men each mesh holds now (one with none is hidden: its shrunk places would still be drawn). */
	TArray<int32> BankLive;
	TMap<int32, int32> BankCopyOf;
	/** How many banked men's meshes were made this battle, and their cost (ms), for the log. */
	int32 BankMade = 0;
	double BankMadeMs = 0.0;

	// ---- The tanks, guns and lorries (LandKit) ----
	struct FBodyDrawn
	{
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		int32 Vehicle = 0, Body = 0;
		bool bTrack = false;
		bool bNear = true;
		/** Drawn only while her roof gun is manned (a German tank's MG 34, its cradle part). */
		bool bWhileManned = false;
		double SootShown = 0.0;
		/** TK1: whether its far level is a section of its own (a body the crowd merges has none), and whether her
		 * instances draw it now (its sections hidden). TK3: the run its links were last posed for (-1e300: to be posed). */
		bool bFarSection = false;
		bool bHeld = false;
		double LinksRun = -1e300;
	};
	TArray<FBodyDrawn> BodiesDrawn;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> BodyMeshes;
	TArray<float> LinkPoints;
	/** TK1: far vehicles as instances of their models' merged far pieces, and TK3's who is in the picture
	 * (Battle/LandCrowd); what it makes is held here. */
	TSharedPtr<FLandCrowd> FarCrowd;
	UPROPERTY()
	TArray<TObjectPtr<UObject>> CrowdKept;

	// ---- The footprint shade (LandShade on M_LandShade: the art director's cut 4, fix 3) ----
	/** The shade's ink (none without M_LandShade: then no shade is drawn, said once), its meshes (one a kind of tank,
	 * gun or lorry, by her model's name, ShadeCopyByName) drawn by instancing, and each vehicle's instance. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ShadeInk;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> ShadeMeshes;
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ShadeCopies;
	TMap<FString, int32> ShadeCopyByName;
	struct FShadeDrawn
	{
		/** The model her shade was made for (a live battle's reset can change who she is). */
		const LandKit::Model* Model = nullptr;
		int32 Copy = -1, Slot = -1;
		bool bShown = false;
		FVector At = FVector::ZeroVector;
		FQuat Q = FQuat::Identity;
	};
	TArray<FShadeDrawn> ShadesDrawn;
	/** Each shade mesh's instances given back (shrunk to nothing), to be used again before a new one is added. */
	TArray<TArray<int32>> ShadeFree;
	/** The men's: a round blob under a standing man, his body's shape under a prone one, a wider blob under a body at
	 * rest; one instanced mesh each (in that order), an instance a man within 150 m as the picture sees him. */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ManShadeCopies;
	struct FManShade
	{
		int32 Kind = -1, Slot = -1;
		FVector At = FVector::ZeroVector;
		float Yaw = 0.0f;
	};
	TArray<FManShade> ManShades;
	TArray<TArray<int32>> ManShadeFree;
	bool bManShadesMade = false;

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
	struct FKeptSky { TWeakObjectPtr<USkyLightComponent> Sky; float Intensity = 0.0f; FLinearColor Colour; bool bLowerBlack = true; FLinearColor Lower = FLinearColor::Black; };
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
	/** The day over the field: the recording's weather, the light it gives (key, sky fill, sky light), its clouds and
	 * its haze (FLandSky, in LandBattle.cpp). Made in HideTheTable. */
	TSharedPtr<FLandSky> SkyDay;

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
	/** The battle's clock held, the first time the game is run, until the materials' shaders are compiled. */
	bool bShadersReady = false;
	double ShaderWait = 0.0;
	bool bBegun = false;
	bool bOwnInput = false;
	/** r.ScreenPercentage as the campaign had it, while "-TheaterLandSize=" holds its own; -1 none. */
	float KeptScreenPercent = -1.0f;
	/** The map's light trims as they stood when the battle began (console variable, value), put back as it ends; and
	 * its sun's contact shadow length (under 0: not changed). */
	TArray<TPair<FString, FString>> KeptTrims;
	float KeptContactShadow = -1.0f;
	bool bKeptContactInWorld = false;
	/** The log every 10 s: frames, the slowest, what the cores and the drawing cost. */
	double LogAt = 0.0, SlowestFrame = 0.0, CoresMs = 0.0, MenMs = 0.0, ArmourMs = 0.0, FieldMs = 0.0;
	/** ...and the drawing's own parts (the men's meshes, the tanks', the effects kit, the sound), the men in the picture
	 * and their meshes made again or held at the old level (the GS3 cap) over the span; the most in one frame. */
	double DrawMenMs = 0.0, DrawArmourMs = 0.0, EffectsMs = 0.0, SoundMs = 0.0;
	int32 LogFrames = 0, MenInPicture = 0, MenRebuilt = 0, RebuildsPut = 0, MostInPicture = 0;

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

	// ---- The battle fought live (LandBattleLive.cpp) ----
	/** The rules on their own thread, fighting the battle a little ahead of the screen (none for a recording). */
	TSharedPtr<FLandLive> Live;
	FString PendingLive, LiveMe;
	int32 LiveSeed = 0;
	bool bLiveDeploy = false;
	/** Started, and waiting on the rules to open the battle (its header): for how long, real seconds. */
	bool bLiveWaiting = false;
	double LiveWaited = 0.0;
	/** Real seconds the clock has waited on the rules this time; whether they have stopped (and it was said). */
	double LiveHeld = 0.0;
	bool bLiveBroken = false;
	/** The tanks' meshes made again (a block placed anew in the setup step, her vehicles changed): their names' round. */
	int32 BodyRound = 0;
	/** A live battle's opening, frame by frame: true while it has the frame (the scene not open yet). */
	bool TickLiveOpening(float DeltaSeconds);
	/** Each frame before the clock: the rules' answers, then the pieces fought, laid into the recording and the cores. */
	void PumpLive();
	/** Each track's man and vehicle from TracksFrom on (all of them after a reset), and the new vehicles' meshes. */
	void MapLive(int32 TracksFrom, int32 VehiclesFrom, bool bReset, bool bVehiclesKept);
	/** The battle's clock this frame never past what the rules have fought (less a margin); and the rules kept ahead. */
	double LiveClock(double From, double To, double Real);
	void CloseLive();

	// ---- The general's controls (LandBattleCommand.cpp) ----
	/** His pointer pressed: where, which button, and whether it has moved past a click since. */
	FVector2D GeneralDownAt = FVector2D::ZeroVector;
	int32 GeneralButton = -1;
	bool bGeneralMoved = false;
	/** A block carried in the setup step (-1 none), and its facing. */
	int32 GeneralCarry = -1;
	double GeneralCarryFace = 0.0;
	/** His last click on open ground with an order given there: when (real seconds) and where (pixels), for a second
	 * click there that sends them at the double. */
	double GeneralClickAt = -10.0;
	FVector2D GeneralClickWhere = FVector2D::ZeroVector;
	/** Each frame, after the clock: the clipboard brought to the time shown, its tags and routes put on the screen. */
	void ShowGeneral();
	/** Each frame, after the film's pose: the general's own view takes the picture while his map is up (the film's
	 * again on C), cutting where the two switch. */
	void GeneralView(double Real);
	/** The field's ground under a pixel (metres, the field's own x east and y north), false off the field. */
	bool GroundUnder(const FVector2D& Pixel, double& OutX, double& OutY) const;
	/** A point of the field on the screen (pixels), false behind the camera. */
	bool PixelOf(double X, double Y, double Lift, FVector2D& OutPixel) const;
	/** How far a click reaches on the ground at a pixel, metres (some 24 pixels' worth there). */
	double ReachAt(const FVector2D& Pixel) const;
	/** The order in hand given (TC.landOrder's command): sent, shown at once, and the click answered on the screen. */
	void GiveGeneral(const FString& Command, const FString& Kind, int32 Target, const TArray<FVector2D>& Points, double PingX, double PingY);

	bool Open();
	void LoadInks();
	void LayField();
	/** The near things (hedges, trees, gates) laid into a tile's mesh, or taken out. */
	void LayThings();
	/** A thing's shade, 0 to 8: three times its brightness third, and its warmth third (the middle one, 1, for a thing
	 * that is not clutter). */
	int32 ShadeOf(int32 Base, uint8 R, uint8 G, uint8 B) const;
	void ShowField(const FVector& EyeWorld, double Battle);
	void LayClutter(const FVector2D& EyeAt, const FVector2D& LookAt, bool bCut, double EyeHigh);
	/** The instanced mesh of set Set (LandMen::Crowd::BankKey) in pose Pose: made now if Builds allows (counted down),
	 * else -1 for this frame; -2 if it cannot be made. */
	int32 BankCopyFor(int32 Set, int32 Pose, int32& Builds);
	/** His place among the banked instances given back (shrunk to nothing where he stood). */
	void BankOut(FManDrawn& D);
	void MakeMen();
	void DrawMen(const FVector& EyeWorld);
	void MakeArmour();
	/** The tank commanders: a rider in the crowd for each American and German tank his crew file has a place on, with
	 * her roof gun's bursts and his end read from the recording (after the replay has begun: its TankMg). */
	void MakeRiders();
	/** Each commander seated and posed for this frame (after the tanks are posed, before the men are updated). */
	void RideTanks();
	/** Each vehicle's bodies from FromVehicle on: one mesh a body, its near level as section 0 and its far as 1. */
	void MakeBodies(int32 FromVehicle);
	/** A man's or a vehicle's block is the enemy's, hidden from the player in woods and not yet found: not drawn. */
	bool HiddenMan(int32 Man) const;
	bool HiddenVehicle(int32 Vehicle) const;
	void DrawArmour(const FVector& EyeWorld);
	/** The instanced mesh of a model's footprint shade, made the first time it is asked for (-1: none). */
	int32 ShadeCopyFor(const LandKit::Model& Model);
	/** Each vehicle's footprint shade under her as she stands, moved only when she has. */
	void DrawShades();
	/** Each near man's: under him as he stands, lies or lies dead, tilted to the ground. */
	void DrawManShades();
	void HideTheTable();
	void KeepTheTableHidden();
	void LightTheBattle();
	/** The day's cumulus or deck, the air battle's own (AirSky), laid once over the field; DrawSky turns them to the eye. */
	void LaySky();
	void DrawSky();
	/** The battle's eye kept as the camera the picture is drawn through, and its grade said in the log once. */
	void KeepTheLook();
	void ShowTheTable();
	void MakeFlags();

	void PlayEffects();
	/** This frame's shells, bombs and grenades into the ground: their holes, kept the whole battle. */
	void MakeCraters();
	/** This frame's tracks on the ground: each moving tank's and lorry's marks, the corn laid flat behind her. */
	void MakeRuts();
	/** The dug ground's parts (LandCrater::Dug): made when a tile is first dug in, sent again when cut. */
	void SendDug();
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
