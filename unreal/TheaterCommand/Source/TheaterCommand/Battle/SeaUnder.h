#pragma once

#include "CoreMinimal.h"
#include "Battle/SeaUnderCore.h"
#include "Engine/Scene.h"
#include "UObject/StrongObjectPtr.h"
#include <map>
#include <vector>

class AActor;
class AAirStage;
class ACameraActor;
class UAudioComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class USoundBase;

/**
 * THE UNDERWATER WAR-FILM VIEW (U1), the engine's side; the reckoning is Battle/SeaUnderCore's. Once AimEye has put the
 * eye under the sea's face (a director's window standing in for the shots, ESeaShot::Under in a picture run, or the
 * player's own U), this turns the room into the water and back: the battle's height fog made a short green murk and the
 * camera's own lens tinted (both kept and given back as they were), the key light's shadows off; Snell's window, a small
 * disc just under the face, facing down, unlit (the stage's sea is one-sided and faces up: from below the murk alone hides
 * the sky, and this is the light coming through); the charges sinking, the silt, each burst's flash, globe and bubbles,
 * the sun's shafts, on one mesh of the existing paints; and the sounds of it, heard in the boat: the water's roar, an
 * escort's screws over her, the asdic's ping on her hull, the thuds, the creaks. The lamps are said at the water's share
 * (ASeaBattle::LightTheBattle), sounds from above dulled (ASeaBattle::Sound), the surface's beds pressed down
 * (ASeaBattle::Beds). The stage's own sun (AAirStage::TowardSun, the day it always lights) sets the murk and the light, so
 * above and below agree. Behind the player's switch: off, the eye never goes under and nothing of it is made.
 */
class FSeaUnder
{
public:
	/** The player's own dive, as a window (INDEX_NONE: none). */
	static constexpr int32 ByHandWindow = -2;

	struct FRoom
	{
		AActor* Owner = nullptr;
		USceneComponent* Root = nullptr;
		ACameraActor* Eye = nullptr;
		AAirStage* Stage = nullptr;
		TArray<TWeakObjectPtr<UExponentialHeightFogComponent>> Fogs; // the room's hazes, as TakeTheRoom set them
		TWeakObjectPtr<UDirectionalLightComponent> Key;               // the battle's sun lamp (LightTheBattle's key)
		double UnitsPerMetre = 100.0;
	};

	FSeaUnder() = default;
	~FSeaUnder();
	FSeaUnder(const FSeaUnder&) = delete;
	FSeaUnder& operator=(const FSeaUnder&) = delete;

	/** The player's switch: "-TheaterSeaUnder=on|off"; else a picture run or the playtester's steps leave it off; else
	 * Saved/Campaign/seaunder.json {"under":...} (Shift+U writes it); else on. */
	static bool Wanted();
	static void SetWanted(bool bInOn);

	void Bind(const FRoom& InRoom, const SeaShow::Field& InBoard);
	/** The battle's own sun where the sea's light core gives one (the release 2 ocean plan, step 11(d): field mode, the
	 * field's sun unclamped), in place of the stage's, which is kept 12 to 70 degrees up: so night under the water is
	 * night, and its 0.8 stop opens. Read at the next ReadSun (Bind's, and each dive's); never called in seed mode. */
	void SetSky(double Up, double From);
	bool IsOn() const { return bOn; }
	/** How far a boat the show hides must be from her for the eye to go under beside her, by the water's sight in use
	 * (SeaUnder::UnheldClearBy): for PlanShots' director, as for the player's own dive. */
	double UnheldClear() const { return ClearBy; }
	/** PlanShots' windows (always planned: the switch is read as they play) and its Sight (the asdic's pings on her hull;
	 * whom the eye may go under beside is the show's own ShipOut::bShown, read live). */
	void TakeWindows(std::vector<SeaUnder::Window>&& InWindows, const SeaUnder::Sight& InSight);
	const std::vector<SeaUnder::Window>& Windows() const { return Plan; }
	/** The window standing in for the shots now, watched or in the month's scene (ByHandWindow the player's own dive,
	 * INDEX_NONE none): under from its start to its end, then on the surface over it while no later shot has cut in
	 * (WantCut: the cut moment of the shot AimEye would show). A picture run takes the windows as its shots instead. */
	int32 StandsIn(double Now, double WantCut, bool bPictures) const;
	/** The eye for a window (its index) or ByHandWindow; false: none (off, let go, or over). */
	bool EyeFor(int32 Index, const SeaShow::Show& Show, double Now, FVector& OutEye, FVector& OutLook, double& OutLens);
	/** U (bShift: Shift+U), watched or handed over by the month scene's screen. */
	void PressU(bool bShift, const SeaShow::Show& Show, double Now, const FVector& EyeAt);
	/** PlayEffects' hook, every effect: ChargeBurst, ChargeDrop (ahead), Sink are taken. */
	void Effect(const SeaShow::Fx& E, const SeaShow::Show& Show, double Now, bool bSkip);
	/** After AimEye, every frame: InWindow the window AimEye shows (ByHandWindow, or INDEX_NONE none); bKeys: read U from
	 * the first player's controller (watched only). */
	void Tick(const SeaShow::Show& Show, double Now, const FVector& EyeAt, const FRotator& Facing, int32 InWindow, bool bSkip, bool bKeys);
	/** The battle skipped ahead (SkipTo): nothing heard for what was skipped. */
	void Skipped();
	bool IsUnder() const { return bUnder; }
	/** The eye went through the sea's face this frame (the lamps are said again at once). */
	bool Crossed() const { return bCrossed; }
	float LightShare() const;
	FLinearColor LightTint() const;
	double Shudder(int32 ShipKey, double Now) const;
	FVector ShakeAt(double Now) const;
	void Muffle(UAudioComponent* Heard) const;
	/** The sea's beds, each with its own level (its fader's, as ASeaBattle::Beds brings it in): pressed down under the sea
	 * and given back to that level coming up, touched only as the eye goes through the face. */
	void Beds(UAudioComponent* DayBed, float DayLevel, UAudioComponent* BattleBed, float BattleLevel, const SeaShow::Show& Show);
	double PaceCap(double InSpeed, bool bOwnPace) const;
	void Took(double Seconds);
	void SayFrameTimes() const;
	void End();

private:
	struct FKeptFog
	{
		TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
		float Start = 0.0f, Density = 0.0f, Falloff = 0.0f, MostOpacity = 0.0f;
		FLinearColor Colour;
	};

	/** One section of the looks mesh (ConvoyLooks.cpp's FLaid), laid each frame into the same arrays, their room kept, so
	 * nothing is made afresh once they have grown; every face wound by the one rule (Wind), so one-sided paints show. */
	struct FLaid
	{
		TArray<FVector> Points;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FColor> Colours;

		bool IsEmpty() const { return Triangles.Num() == 0; }
		void Reset();
		/** A triangle whose drawn side faces Outward. */
		void Tri(const FVector& A, const FVector& B, const FVector& C, const FVector& Outward, const FColor& Colour);
		void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Outward, const FColor& Colour);
		/** A flat piece facing the eye: its middle, half across, half along. */
		void Flat(const FVector& Mid, const FVector& Across, const FVector& Along, const FVector& Outward, const FColor& Colour);
		/** A disc facing the eye, its middle one paint and its rim another. */
		void Disc(const FVector& Mid, const FVector& Across, const FVector& Up, double Radius, int32 Sides, const FVector& Outward, const FColor& Middle, const FColor& Rim);
		/** A rod from From to To, Sides-sided, its ends closed: each face wound to face outward. */
		void Rod(const FVector& From, const FVector& To, double Radius, int32 Sides, const FColor& Colour);

	private:
		/** The last three points made one triangle, its drawn side facing Outward (FMiniatureMesh::Face's rule). */
		void Wind(int32 First, const FVector& Outward);
	};
	/** The looks mesh's sections (SeaUnder.cpp's SeaUnderParts: the charges, the soft things, the light). */
	static constexpr int32 LooksSections = 3;

	/** The stage's own sun (AAirStage::TowardSun) and the day its key lamp gives: read as the battle opens and again at
	 * each dive, so the water's light and the sky above it are the one sun. */
	void ReadSun();
	void GoUnder(const SeaShow::Show& Show, double Now, bool bSkip);
	void ComeUp();
	void SetMurk(const SeaUnder::Murk& M);
	void LayCeiling(const SeaUnder::P3& At, double Now, const SeaUnder::Murk& M);
	void LayLooks(const SeaShow::Show& Show, double Now, const SeaUnder::P3& At, const FRotator& Facing);
	void Hear(const SeaShow::Show& Show, double Now, bool bSkip);
	UProceduralMeshComponent* MakeMesh(const TCHAR* Name);
	UAudioComponent* MakeBed(const TCHAR* Name);
	USoundBase* SoundOf(const TCHAR* Name, bool bLoop = false);
	void Play(const TCHAR* Name, float Volume, float Pitch = 1.0f);
	int32 BoatNow() const;
	double WindowFrom() const;
	/** Every hull afloat now, into HullsNow (its room kept). */
	const std::vector<SeaUnder::Hull>& HullsOf(const SeaShow::Show& Show);
	FVector World(const SeaUnder::P3& P) const { return FVector(P.Y * Room.UnitsPerMetre, P.X * Room.UnitsPerMetre, P.Z * Room.UnitsPerMetre); }
	SeaUnder::P3 BoardOf(const FVector& W) const { return SeaUnder::P3(W.Y / Room.UnitsPerMetre, W.X / Room.UnitsPerMetre, W.Z / Room.UnitsPerMetre); }

	FRoom Room;
	SeaShow::Field Board;
	bool bOn = true, bUnder = false, bCrossed = false, bCeilingLaid = false, bShadowsKept = false, bKeyShadows = true;
	/** The stage's sun: its height and bearing (degrees), the day it gives (SeaUnder::DaylightOf, by the key lamp's share). */
	double SunUp = 45.0, SunFrom = 0.0, Daylight = 1.0;
	/** SetSky's sun (step 11(d)), where given. */
	bool bSkyGiven = false;
	double SkyUp = 45.0, SkyFrom = 0.0;
	double DaySee = SeaUnder::SeeDay, NightSee = SeaUnder::SeeNight, LightAtBoat = 0.6;
	double ClearBy = SeaUnder::UnheldClearBy();
	std::vector<SeaUnder::Window> Plan;
	SeaUnder::Sight Pings;
	SeaUnder::Water Sea;
	std::vector<SeaShow::Show::ChargeDown> Down;
	TSet<int32> Waived;
	int32 ShownWindow = INDEX_NONE, HandBoat = -1, HandSide = 1;
	double HandSince = 0.0, HeardTo = -1.0, PingAt = -1.0, LastCreak = -1e9, LastKeel = -1.0, MurkDensity = -1.0, MurkGreen = -1.0;
	FVector EyeNow = FVector::ZeroVector;
	SeaUnder::P3 LastBoatAt;
	TArray<FKeptFog> KeptFogs;
	FPostProcessSettings KeptLens;
	TWeakObjectPtr<UProceduralMeshComponent> Ceiling, Looks;
	ConvoyLook::MeshShape Shapes[LooksSections];
	int32 SectionsLaid = 0;
	std::vector<SeaUnder::CeilingPoint> CeilingPoints;
	TArray<int32> CeilingTriangles;
	/** What is laid each frame, kept from frame to frame for its room (45 frames a second on the MacBook Air): the hulls about the
	 * eye, the boats and their way (set in place: the pieces listed this frame), the water's frame, the looks' sections, the
	 * face's points, the creaks. */
	std::vector<SeaUnder::Hull> HullsNow;
	std::vector<SeaUnder::BoatAt> BoatsNow;
	std::map<int, SeaUnder::BoatNow> BoatsMoving;
	std::vector<int> BoatsMovingSeen;
	SeaUnder::Frame InWater;
	FLaid Laid[LooksSections];
	TArray<FVector> CeilingPlaces, CeilingNormals;
	TArray<FColor> CeilingColours;
	std::vector<SeaUnder::Creak> Creaks;
	TMap<FName, TStrongObjectPtr<USoundBase>> Sounds;
	TSet<FName> SoundsMissing;
	TWeakObjectPtr<UAudioComponent> UnderBed, Screws, BedsSeen[2];
	bool bBedDulled[2] = { false, false };
	TArray<float> FramesUnder;
};
