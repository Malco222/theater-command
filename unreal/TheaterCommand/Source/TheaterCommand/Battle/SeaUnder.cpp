#include "Battle/SeaUnder.h"

#include "Battle/AirStage.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "TheaterCommand.h"

#include <algorithm>
#include <cmath>

namespace SeaUnderParts
{
	/** The looks mesh's sections: the charges (the map's surface paint, lit), the soft things (the silt, the bubbles, the
	 * globes: M_Highlight), light (a burst's flash, the sun's shafts: M_Glow). */
	static constexpr int32 SolidSection = 0, SoftSection = 1, GlowSection = 2, Sections = 3;
	/** [EST] The sea's face from below (M_UnderSea's Brightness; "-TheaterUnderSky="). */
	static constexpr float SkyBright = 1.6f;
	/** [EST] A sound from above the sea heard under it: nothing over this (Hz), at this share. */
	static constexpr float MuffledAbove = 600.0f, MuffledShare = 0.5f;
	/** Who is heard: an escort's screws within ScrewsHeard m (level), a burst within ThudHeard m; [EST] an asdic's pings
	 * PingMost s apart at the most, a second apart at 700 m and in (the war film's quickening ping). */
	static constexpr double ScrewsHeard = 500.0, ThudHeard = 400.0, PingMost = 3.2, PingClose = 1.0;
	/** The key lamp's strength the stage lights a day with (ASeaBattle::LightTheBattle's 7 lux): the share of it the room
	 * has now is the day the water is given. */
	static constexpr double KeyLux = 7.0;
	/** The paint: a charge as ConvoyLooks paints it in the air; the silt; the froth of a globe or a bubble. */
	static const FColor ChargeDark(46, 50, 50, 255);
	static const FColor Silt(196, 214, 190, 90);
	static const FColor Froth(220, 236, 228, 150);

	static FString SettingFile()
	{
		// (a run that takes pictures or plays the playtester's steps keeps its own, as the sub scene's setting does)
		FString Run;
		const bool bRun = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Run, false) || FParse::Value(FCommandLine::Get(), TEXT("TheaterSteps="), Run, false);
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Campaign"), bRun ? TEXT("seaunder-runs.json") : TEXT("seaunder.json"));
	}

	/** The haze's colour as the room had it (the engine's field has had two names: SeaBattle.cpp's SeaParts::HazeColour). */
	template <typename TFog>
	static FLinearColor HazeColour(const TFog* Fog, const FLinearColor& Else)
	{
		if constexpr (requires(const TFog* F) { FLinearColor(F->FogInscatteringLuminance); })
		{
			return Fog->FogInscatteringLuminance;
		}
		else if constexpr (requires(const TFog* F) { FLinearColor(F->FogInscatteringColor); })
		{
			return Fog->FogInscatteringColor;
		}
		else
		{
			return Else;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// The looks mesh's sections

void FSeaUnder::FLaid::Reset()
{
	Points.Reset();
	Triangles.Reset();
	Normals.Reset();
	Colours.Reset();
}

void FSeaUnder::FLaid::Wind(int32 First, const FVector& Outward)
{
	// The engine draws the side of a triangle whose corners run one way round: put them that way round for the side that
	// faces out (FMiniatureMesh::Face's rule; the one rule for every face here, a disc's as a rod's).
	const FVector Facing = (Points[First + 1] - Points[First + 2]) ^ (Points[First] - Points[First + 2]);
	if ((Facing | Outward) >= 0.0)
	{
		Triangles.Append({ First, First + 1, First + 2 });
	}
	else
	{
		Triangles.Append({ First, First + 2, First + 1 });
	}
}

void FSeaUnder::FLaid::Tri(const FVector& A, const FVector& B, const FVector& C, const FVector& Outward, const FColor& Colour)
{
	const FVector Normal = Outward.GetSafeNormal();
	const int32 First = Points.Num();
	Points.Append({ A, B, C });
	Normals.Append({ Normal, Normal, Normal });
	Colours.Append({ Colour, Colour, Colour });
	Wind(First, Outward);
}

void FSeaUnder::FLaid::Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Outward, const FColor& Colour)
{
	Tri(A, B, C, Outward, Colour);
	Tri(A, C, D, Outward, Colour);
}

void FSeaUnder::FLaid::Flat(const FVector& Mid, const FVector& Across, const FVector& Along, const FVector& Outward, const FColor& Colour)
{
	Quad(Mid - Across - Along, Mid + Across - Along, Mid + Across + Along, Mid - Across + Along, Outward, Colour);
}

void FSeaUnder::FLaid::Disc(const FVector& Mid, const FVector& Across, const FVector& Up, double Radius, int32 Sides, const FVector& Outward, const FColor& Middle, const FColor& Rim)
{
	const FVector Normal = Outward.GetSafeNormal();
	for (int32 K = 0; K < Sides; ++K)
	{
		const double A0 = 2.0 * UE_DOUBLE_PI * K / Sides, A1 = 2.0 * UE_DOUBLE_PI * (K + 1) / Sides;
		const FVector R0 = (Across * FMath::Cos(A0) + Up * FMath::Sin(A0)) * Radius;
		const FVector R1 = (Across * FMath::Cos(A1) + Up * FMath::Sin(A1)) * Radius;
		const int32 First = Points.Num();
		Points.Append({ Mid, Mid + R0, Mid + R1 });
		Normals.Append({ Normal, Normal, Normal });
		Colours.Append({ Middle, Rim, Rim });
		Wind(First, Outward);
	}
}

void FSeaUnder::FLaid::Rod(const FVector& From, const FVector& To, double Radius, int32 Sides, const FColor& Colour)
{
	const FVector Along = To - From;
	if (Along.IsNearlyZero() || Radius <= 0.0)
	{
		return;
	}
	const FVector Axis = Along.GetSafeNormal();
	const FVector A = (FMath::Abs(Axis.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).Cross(Axis).GetSafeNormal() * Radius;
	const FVector B = Axis.Cross(A).GetSafeNormal() * Radius;
	for (int32 K = 0; K < Sides; ++K)
	{
		const double A0 = 2.0 * UE_DOUBLE_PI * K / Sides, A1 = 2.0 * UE_DOUBLE_PI * (K + 1) / Sides;
		const FVector R0 = A * FMath::Cos(A0) + B * FMath::Sin(A0), R1 = A * FMath::Cos(A1) + B * FMath::Sin(A1);
		Quad(From + R0, From + R1, To + R1, To + R0, R0 + R1, Colour);
		Tri(From, From + R0, From + R1, -Axis, Colour);
		Tri(To, To + R0, To + R1, Axis, Colour);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// The view

FSeaUnder::~FSeaUnder()
{
	End();
}

bool FSeaUnder::Wanted()
{
	// Brian's standing rule: everything new can be turned off. The flag first; a picture run or the playtester's steps
	// leave it off unless asked (their pictures and steps as they were); else the player's own setting; else on.
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaUnder="), Asked))
	{
		return Asked.ToLower() != TEXT("off");
	}
	FString Run;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattleShots="), Run) || FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Run, false) ||
		FParse::Value(FCommandLine::Get(), TEXT("TheaterSteps="), Run, false))
	{
		return false;
	}
	FString Text, Kept;
	TSharedPtr<FJsonObject> Object;
	return !(FFileHelper::LoadFileToString(Text, *SeaUnderParts::SettingFile()) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) &&
		Object.IsValid() && Object->TryGetStringField(TEXT("under"), Kept) && Kept == TEXT("off"));
}

void FSeaUnder::SetWanted(bool bInOn)
{
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"under\":\"%s\"}\n"), bInOn ? TEXT("on") : TEXT("off")), *SeaUnderParts::SettingFile(),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void FSeaUnder::Bind(const FRoom& InRoom, const SeaShow::Field& InBoard)
{
	Room = InRoom;
	Board = InBoard;
	bOn = Wanted();
	// [EST] how far the water lets the eye see by day and by night ("-TheaterUnderSee=85,60": the art director's to set;
	// read past its comma, as the other two-number knobs are), for the fog's murk and the water's looks alike
	FString See, Day, Night;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterUnderSee="), See, false) && See.Split(TEXT(","), &Day, &Night))
	{
		DaySee = FMath::Clamp(FCString::Atod(*Day), 10.0, 300.0);
		NightSee = FMath::Clamp(FCString::Atod(*Night), 10.0, 300.0);
	}
	// (clearer water sees farther: an unfound boat must be the farther off for the eye to go under near her)
	ClearBy = SeaUnder::UnheldClearBy(DaySee, NightSee);
	Sea.Begin(Board, DaySee, NightSee);
	ReadSun();
	UE_LOG(LogTheater, Display, TEXT("Sea battle: the view under the water is %s (Shift+U turns it on or off; U goes under by hand). The stage's sun stands %.0f deg up from %.0f deg: the water's day %.2f."),
		bOn ? TEXT("on") : TEXT("off"), SunUp, SunFrom, Daylight);
}

void FSeaUnder::SetSky(double Up, double From)
{
	bSkyGiven = true;
	SkyUp = FMath::Clamp(Up, -90.0, 90.0);
	SkyFrom = From;
}

void FSeaUnder::ReadSun()
{
	// The stage's own sun, which the key lamp is laid on (AAirStage::TowardSun, ASeaBattle::TakeTheRoom): the day under
	// the water is the day over it. Its height and bearing in the board's own frame, and the share of the day the lamp
	// gives now (dimmed while under, so what is read at the dive is the room's own). Where the sea's light core gave the
	// battle's own sun (SetSky, field mode), that one: the key lamp is laid on it.
	if (bSkyGiven)
	{
		SunUp = SkyUp;
		SunFrom = SkyFrom;
	}
	else if (Room.Stage)
	{
		const FVector Toward = Room.Stage->TowardSun();
		SunUp = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Toward.Z, -1.0, 1.0)));
		// (the world's X is the board's east and its Y the board's north: ASeaBattle::World)
		SunFrom = ConvoyLook::HeadingOf(SeaUnder::P3(Toward.Y, Toward.X, 0.0), 0.0);
	}
	double Share = 1.0;
	if (const UDirectionalLightComponent* Key = Room.Key.Get())
	{
		Share = FMath::Clamp(Key->Intensity / SeaUnderParts::KeyLux, 0.2, 1.0);
	}
	Daylight = FMath::Clamp(SeaUnder::DaylightOf(SunUp) * Share, 0.0, 1.0);
}

void FSeaUnder::TakeWindows(std::vector<SeaUnder::Window>&& InWindows, const SeaUnder::Sight& InSight)
{
	Plan = MoveTemp(InWindows);
	Pings = InSight;
	Waived.Reset();
	for (const SeaUnder::Window& W : Plan)
	{
		UE_LOG(LogTheater, Display, TEXT("Sea battle: under the water (%s) from %.1f s to %.1f s, by boat %d."), UTF8_TO_TCHAR(W.Name.c_str()), W.From, W.Until, W.Boat);
	}
}

int32 FSeaUnder::StandsIn(double Now, double WantCut, bool bPictures) const
{
	// The player's own dive stands in for any shot. A picture run plays the windows as its own shots instead, so the
	// director's windows never stand in there.
	if (!bOn)
	{
		return INDEX_NONE;
	}
	if (HandBoat >= 0)
	{
		return ByHandWindow;
	}
	if (bPictures)
	{
		return INDEX_NONE;
	}
	for (int32 K = 0; K < static_cast<int32>(Plan.size()); ++K)
	{
		if (Waived.Contains(K))
		{
			continue;
		}
		const SeaUnder::Window& W = Plan[K];
		if (Now >= W.From && Now <= W.Until)
		{
			return K;
		}
		// (over the pattern on the surface afterwards, while no later shot has cut in)
		if (Now > W.Until && Now <= W.HoldUntil && WantCut <= W.Until + 1e-6)
		{
			return K;
		}
	}
	return INDEX_NONE;
}

const std::vector<SeaUnder::Hull>& FSeaUnder::HullsOf(const SeaShow::Show& Show)
{
	HullsNow.clear();
	for (const SeaShow::ShipOut& Ship : Show.Ships())
	{
		if (Ship.Doing != SeaShow::Fate::Gone)
		{
			HullsNow.push_back(SeaUnder::HullOf(Ship));
		}
	}
	return HullsNow;
}

bool FSeaUnder::EyeFor(int32 Index, const SeaShow::Show& Show, double Now, FVector& OutEye, FVector& OutLook, double& OutLens)
{
	if (!bOn)
	{
		return false;
	}
	SeaUnder::EyePose Pose;
	if (Index == ByHandWindow)
	{
		// (never beside a boat the show hides, nor one up with her crew on deck: SeaUnder::HandHolds, which Tick asks after
		// this, said here too so not one frame shows her)
		const SeaShow::ShipOut* Boat = HandBoat >= 0 ? Show.Ship(HandBoat) : nullptr;
		if (!Boat || Boat->Doing == SeaShow::Fate::Gone || !Boat->bShown || SeaUnder::CrewUp(*Boat, Now))
		{
			HandBoat = -1;
			return false;
		}
		Pose = SeaUnder::EyeByHand(SeaUnder::PoseOf(*Boat), HullsOf(Show), Now - HandSince, HandSide);
	}
	else
	{
		if (Index < 0 || Index >= static_cast<int32>(Plan.size()) || Waived.Contains(Index))
		{
			return false;
		}
		const SeaUnder::Window& W = Plan[Index];
		const SeaShow::ShipOut* Boat = Show.Ship(W.Boat);
		// (posed only while the show draws her, PoseOf, and her crew are below, CrewUp: else the eye is up over the pattern,
		// as the director planned it from the same show)
		SeaUnder::BoatPose Her = Boat ? SeaUnder::PoseOf(*Boat) : SeaUnder::BoatPose();
		Her.bThere = Her.bThere && Boat != nullptr && !SeaUnder::CrewUp(*Boat, Now);
		if (!SeaUnder::EyeOf(W, Her, HullsOf(Show), Now, Pose))
		{
			return false;
		}
	}
	OutEye = World(Pose.Eye);
	OutLook = World(Pose.Look);
	OutLens = Pose.Lens;
	return true;
}

void FSeaUnder::PressU(bool bShift, const SeaShow::Show& Show, double Now, const FVector& EyeAt)
{
	if (bShift)
	{
		// The switch, kept for the next battle; off, the camera comes up at once and the director's windows are let go.
		bOn = !bOn;
		SetWanted(bOn);
		HandBoat = -1;
		UE_LOG(LogTheater, Display, TEXT("Sea battle: the view under the water turned %s."), bOn ? TEXT("on") : TEXT("off"));
		return;
	}
	if (!bOn)
	{
		return;
	}
	if (HandBoat >= 0)
	{
		HandBoat = -1; // (back up)
		return;
	}
	if (ShownWindow >= 0 && bUnder)
	{
		Waived.Add(ShownWindow); // (the director's window let go: up now)
		return;
	}
	// Down by hand beside the nearest boat he may see (SeaUnder::HandBoatFor): one the show draws (his own, or one found
	// now: ShipOut::bShown, the surface's own rule), under way, her crew below, within 6 km of the eye, and with no boat the
	// show hides nor one at her gun near her (the director's own rule, ClearOfUnheld), so a dive never gives one away. None:
	// the eye stays up.
	SeaUnder::BoatsOf(Show, BoatsNow);
	const int32 Chosen = SeaUnder::HandBoatFor(BoatsNow, BoardOf(EyeAt), ClearBy);
	if (Chosen < 0)
	{
		UE_LOG(LogTheater, Display, TEXT("Sea battle: no boat to go under beside at %.1f s (none of his own or found within 6 km with her crew below, or one not yet found near her)."), Now);
		return;
	}
	HandBoat = Chosen;
	HandSince = Now;
	HandSide = SeaUnder::HandOf(Chosen);
}

void FSeaUnder::Effect(const SeaShow::Fx& E, const SeaShow::Show& Show, double Now, bool bSkip)
{
	using namespace SeaUnderParts;
	// A boat's end, whichever boat she is and with the view on or off: her air goes up from her alone, once it has come,
	// whenever she is posed after it (SeaUnder::Water::Lay); her hull's last rattle heard if she is the one beside the eye.
	if (E.Kind == SeaShow::FxKind::Sink)
	{
		const SeaShow::ShipOut* Lost = Show.Ship(E.Key);
		if (Lost && Lost->Class == "ss")
		{
			Sea.Ended(Now, E.Key);
		}
		if (bOn && bUnder && !bSkip && E.Key == BoatNow())
		{
			Play(TEXT("hull_rattle"), 1.0f);
		}
		return;
	}
	if (!bOn)
	{
		return;
	}
	if (E.Kind == SeaShow::FxKind::ChargeBurst)
	{
		const SeaUnder::P3 At(E.At.X, E.At.Y, E.At.Z);
		Sea.Burst(Now, At, E.What);
		// (heard through the water and her hull at once at these ranges, the louder the nearer; none past ThudHeard m)
		const double Far = FVector::Dist(World(At), EyeNow) / Room.UnitsPerMetre;
		if (bUnder && !bSkip && Far <= ThudHeard)
		{
			Play(TEXT("under_thud"), static_cast<float>(FMath::Clamp(60.0 / FMath::Max(Far, 20.0), 0.2, 1.0)), static_cast<float>(FMath::Clamp(1.1 - Far / 1000.0, 0.8, 1.1)));
		}
	}
	else if (E.Kind == SeaShow::FxKind::ChargeDrop && E.What == "ahead")
	{
		// (the same plan and seed as ConvoyLooks' Drop, so its splashes above and these bombs below agree)
		ConvoyLook::DropIn In;
		In.Kind = E.What;
		In.N = static_cast<int>(std::lround(E.Size));
		In.Spread = E.Spread;
		In.Sinking = E.Sinking;
		In.Until = E.Until;
		In.At = ConvoyLook::P3(E.At.X, E.At.Y, E.At.Z);
		In.Aim = ConvoyLook::P3(E.Aim.X, E.Aim.Y, 0.0);
		In.Vel = ConvoyLook::P3(E.Vel.X, E.Vel.Y, 0.0);
		if (const SeaShow::ShipOut* Escort = Show.Ship(E.Key))
		{
			// (her beam the show's; her length as the stage draws her, for her ahead thrower)
			In.Beam = Escort->Beam;
			In.Long = Escort->Long;
			double Drawn = 0.0;
			FVector Bow = FVector::ZeroVector;
			bool bHarbour = false;
			if (Room.Stage && Room.Stage->ShipLie(E.Key, Drawn, Bow, bHarbour) && Drawn > 1.0)
			{
				In.Long = Drawn;
			}
		}
		double Deepest = 120.0;
		for (const SeaShow::ShipOut& Ship : Show.Ships())
		{
			if (Ship.Class == "ss")
			{
				Deepest = FMath::Max(Deepest, Ship.Keel + 30.0);
			}
		}
		const uint64 Seed = static_cast<uint64>(static_cast<uint32>(E.Key)) * 7919ull + static_cast<uint64>(FMath::Max(0.0, Now) * 10.0);
		Sea.AheadDropped(Now, In, Seed, Deepest);
	}
}

void FSeaUnder::Tick(const SeaShow::Show& Show, double Now, const FVector& EyeAt, const FRotator& Facing, int32 InWindow, bool bSkip, bool bKeys)
{
	ShownWindow = InWindow;
	EyeNow = EyeAt;
	// The keys, watched (the month's scene's screen hands them over itself: ASeaBattle::UnderKey).
	if (bKeys)
	{
		UWorld* Here = Room.Owner ? Room.Owner->GetWorld() : nullptr;
		APlayerController* Player = Here ? Here->GetFirstPlayerController() : nullptr;
		if (Player && Player->WasInputKeyJustPressed(EKeys::U))
		{
			PressU(Player->IsInputKeyDown(EKeys::LeftShift) || Player->IsInputKeyDown(EKeys::RightShift), Show, Now, EyeAt);
		}
	}
	// (the player's own dive lasts while the show draws her, to her last, her crew below; and no boat the show hides nor one
	// at her gun comes near her, SeaUnder::HandHolds, the director's rule: one that does takes the eye up at once)
	if (HandBoat >= 0)
	{
		SeaUnder::BoatsOf(Show, BoatsNow);
		if (!SeaUnder::HandHolds(BoatsNow, HandBoat, ClearBy))
		{
			HandBoat = -1;
		}
	}
	// The charges as first seen going down, every frame while the view is on (so one gone under for later is still drawn
	// from its drop, as the cloud test plays it), and where every boat is for those told on one; into lists kept from frame
	// to frame. Off, none of it: the eye never goes under. (The boats' way set in place, the first the show lists of a
	// piece kept, and those it no longer lists let go after: no room made or freed while the boats are the same.)
	if (bOn)
	{
		Show.ChargesDown(Down);
		BoatsMovingSeen.clear();
		for (const SeaShow::ShipOut& Ship : Show.Ships())
		{
			if (Ship.Class != "ss" || Ship.Doing == SeaShow::Fate::Gone || std::find(BoatsMovingSeen.begin(), BoatsMovingSeen.end(), Ship.Piece) != BoatsMovingSeen.end())
			{
				continue;
			}
			BoatsMovingSeen.push_back(Ship.Piece);
			BoatsMoving[Ship.Piece] = SeaUnder::BoatNow{ SeaUnder::P3(Ship.At.X, Ship.At.Y, -Ship.Keel), ConvoyLook::AheadOf(Ship.Heading) * Ship.Speed };
		}
		for (auto Moving = BoatsMoving.begin(); Moving != BoatsMoving.end();)
		{
			if (std::find(BoatsMovingSeen.begin(), BoatsMovingSeen.end(), Moving->first) == BoatsMovingSeen.end())
			{
				Moving = BoatsMoving.erase(Moving);
			}
			else
			{
				++Moving;
			}
		}
		Sea.Remember(Now, Down, BoatsMoving);
	}
	const bool bNowUnder = bOn && EyeAt.Z < -0.05 * Room.UnitsPerMetre;
	bCrossed = bNowUnder != bUnder;
	if (bCrossed)
	{
		bUnder = bNowUnder;
		if (bNowUnder)
		{
			GoUnder(Show, Now, bSkip);
		}
		else
		{
			ComeUp();
		}
	}
	if (!bUnder)
	{
		return;
	}
	const SeaUnder::P3 Eye = BoardOf(EyeAt);
	const SeaUnder::Murk M = SeaUnder::MurkAt(-Eye.Z, Daylight, SunUp, DaySee, NightSee);
	SetMurk(M);
	LayCeiling(Eye, Now, M);
	LayLooks(Show, Now, Eye, Facing);
	Hear(Show, Now, bSkip);
}

void FSeaUnder::GoUnder(const SeaShow::Show& Show, double Now, bool bSkip)
{
	// The battle's haze kept as it is, then made the water: from the eye, all the way, no thinning with height.
	ReadSun();
	KeptFogs.Reset();
	for (const TWeakObjectPtr<UExponentialHeightFogComponent>& Each : Room.Fogs)
	{
		if (UExponentialHeightFogComponent* Fog = Each.Get())
		{
			FKeptFog& Kept = KeptFogs.AddDefaulted_GetRef();
			Kept.Fog = Fog;
			Kept.Start = Fog->StartDistance;
			Kept.Density = Fog->FogDensity;
			Kept.Falloff = Fog->FogHeightFalloff;
			Kept.MostOpacity = Fog->FogMaxOpacity;
			Kept.Colour = SeaUnderParts::HazeColour(Fog, FLinearColor(0.80f, 0.84f, 0.88f));
			Fog->SetStartDistance(0.0f);
			Fog->SetFogHeightFalloff(0.001f);
			Fog->SetFogMaxOpacity(1.0f);
		}
	}
	MurkDensity = -1.0;
	MurkGreen = -1.0;
	// The camera's own lens kept, then the film's look under the water: the light green, less colour, the edges dark, the
	// bright things glowing, a little grain; by night opened 0.8 of a stop (day for night).
	if (Room.Eye)
	{
		FPostProcessSettings& Lens = Room.Eye->GetCameraComponent()->PostProcessSettings;
		KeptLens = Lens;
		Lens.bOverride_SceneColorTint = true;
		Lens.SceneColorTint = FLinearColor(0.78f, 1.0f, 0.9f);
		Lens.bOverride_ColorSaturation = true;
		Lens.ColorSaturation = KeptLens.ColorSaturation * FVector4(0.8, 0.8, 0.8, 1.0);
		Lens.bOverride_VignetteIntensity = true;
		Lens.VignetteIntensity = 0.6f;
		Lens.bOverride_BloomIntensity = true;
		Lens.BloomIntensity = 1.4f;
		Lens.bOverride_FilmGrainIntensity = true;
		Lens.FilmGrainIntensity = 0.25f;
		Lens.bOverride_AutoExposureBias = true;
		Lens.AutoExposureBias = KeptLens.AutoExposureBias + (Daylight < 0.3 ? 0.8f : 0.0f);
	}
	// The sun's lamp casts no shadow under the water: the stage's sea is one-sided and faces up, so from below a hull's
	// shadow on it would stand in the murk with nothing to fall on (and the near hulls' shadows cost frames).
	if (UDirectionalLightComponent* Key = Room.Key.Get())
	{
		bKeyShadows = Key->CastShadows;
		bShadowsKept = true;
		Key->SetCastShadows(false);
	}
	// The light on her: the share of the sun that reaches her middle as the window opens, steady through it.
	const int32 Who = BoatNow();
	const SeaShow::ShipOut* Boat = Who >= 0 ? Show.Ship(Who) : nullptr;
	LightAtBoat = SeaUnder::MurkAt(Boat ? SeaUnder::MiddleOf(Boat->Keel) : -BoardOf(EyeNow).Z, Daylight, SunUp, DaySee, NightSee).SunShare;
	LastKeel = Boat ? Boat->Keel : -1.0;
	HeardTo = Now;
	PingAt = -1.0;
	for (const TWeakObjectPtr<UProceduralMeshComponent>& Each : { Ceiling, Looks })
	{
		if (UProceduralMeshComponent* Mesh = Each.Get())
		{
			Mesh->SetVisibility(true);
		}
	}
	if (!bSkip)
	{
		Play(TEXT("under_plunge"), 0.8f);
	}
}

void FSeaUnder::ComeUp()
{
	for (const FKeptFog& Kept : KeptFogs)
	{
		if (UExponentialHeightFogComponent* Fog = Kept.Fog.Get())
		{
			Fog->SetStartDistance(Kept.Start);
			Fog->SetFogDensity(Kept.Density);
			Fog->SetFogHeightFalloff(Kept.Falloff);
			Fog->SetFogMaxOpacity(Kept.MostOpacity);
			Fog->SetFogInscatteringColor(Kept.Colour); // (as TakeTheRoom and GiveBackTheRoom set it)
		}
	}
	KeptFogs.Reset();
	if (Room.Eye)
	{
		Room.Eye->GetCameraComponent()->PostProcessSettings = KeptLens;
	}
	if (bShadowsKept)
	{
		if (UDirectionalLightComponent* Key = Room.Key.Get())
		{
			Key->SetCastShadows(bKeyShadows);
		}
		bShadowsKept = false;
	}
	for (const TWeakObjectPtr<UProceduralMeshComponent>& Each : { Ceiling, Looks })
	{
		if (UProceduralMeshComponent* Mesh = Each.Get())
		{
			Mesh->SetVisibility(false);
		}
	}
}

void FSeaUnder::SetMurk(const SeaUnder::Murk& M)
{
	// (said only as it changes by a hundredth: each call marks the fog's render state)
	const double Density = SeaUnder::FogDensity(M.See, Room.UnitsPerMetre);
	if (MurkDensity > 0.0 && FMath::Abs(Density - MurkDensity) < 0.01 * MurkDensity && FMath::Abs(M.G - MurkGreen) < 0.0005)
	{
		return;
	}
	MurkDensity = Density;
	MurkGreen = M.G;
	for (const FKeptFog& Kept : KeptFogs)
	{
		if (UExponentialHeightFogComponent* Fog = Kept.Fog.Get())
		{
			Fog->SetFogDensity(static_cast<float>(Density));
			Fog->SetFogInscatteringColor(FLinearColor(static_cast<float>(M.R), static_cast<float>(M.G), static_cast<float>(M.B)));
		}
	}
}

UProceduralMeshComponent* FSeaUnder::MakeMesh(const TCHAR* Name)
{
	// (as ConvoyLooks makes its own: on the scene, in the world's frame, no shadow, nothing to collide with, and a name
	// of its own so a second battle or a replay in the same world does not clash with it)
	UProceduralMeshComponent* Made = NewObject<UProceduralMeshComponent>(Room.Owner, MakeUniqueObjectName(Room.Owner, UProceduralMeshComponent::StaticClass(), FName(Name)));
	Made->bUseAsyncCooking = false;
	Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Made->SetCastShadow(false);
	Made->SetupAttachment(Room.Root);
	Made->SetUsingAbsoluteLocation(true);
	Made->SetUsingAbsoluteRotation(true);
	Made->SetUsingAbsoluteScale(true);
	Made->RegisterComponent();
	Made->SetWorldTransform(FTransform::Identity);
	return Made;
}

void FSeaUnder::LayCeiling(const SeaUnder::P3& At, double Now, const SeaUnder::Murk& M)
{
	using namespace SeaUnderParts;
	if (!Room.Owner)
	{
		return;
	}
	UProceduralMeshComponent* Sheet = Ceiling.Get();
	if (!Sheet)
	{
		Sheet = MakeMesh(TEXT("UnderSeaFace"));
		Ceiling = Sheet;
		bCeilingLaid = false;
		UMaterialInterface* Face = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_UnderSea.M_UnderSea"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		const bool bOwn = Face != nullptr;
		if (!Face)
		{
			Face = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
			UE_LOG(LogTheater, Warning, TEXT("Sea battle: M_UnderSea is missing (run Tools/build_content.sh): the sea's face from below on M_Highlight."));
		}
		UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(Face ? Face : UMaterial::GetDefaultMaterial(MD_Surface), Sheet);
		float Bright = SkyBright;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterUnderSky="), Bright);
		Paint->SetScalarParameterValue(TEXT("Brightness"), Bright);
		if (!bOwn)
		{
			// (M_Highlight takes its see-through from the alpha, and the face is whole)
			Paint->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
		}
		Sheet->SetMaterial(0, Paint);
	}
	SeaUnder::CeilingAt(At, Now, Daylight, SunUp, SunFrom, M, CeilingPoints);
	// (into the arrays kept from frame to frame: laid every frame under the water, so nothing is made afresh)
	CeilingPlaces.Reset();
	CeilingNormals.Reset();
	CeilingColours.Reset();
	const int32 Count = static_cast<int32>(CeilingPoints.size());
	for (const SeaUnder::CeilingPoint& P : CeilingPoints)
	{
		CeilingPlaces.Add(World(P.At));
		CeilingNormals.Add(FVector::DownVector);
		// (sRGB in the vertex: M_UnderSea raises it to 2.2 again, as M_StageSea does its paint)
		CeilingColours.Add(FLinearColor(static_cast<float>(P.R), static_cast<float>(P.G), static_cast<float>(P.B), 1.0f).ToFColor(true));
	}
	if (bCeilingLaid && CeilingTriangles.Num() > 0 && Count == 1 + SeaUnder::CeilingRings * SeaUnder::CeilingSpokes)
	{
		Sheet->UpdateMeshSection(0, CeilingPlaces, CeilingNormals, TArray<FVector2D>(), CeilingColours, TArray<FProcMeshTangent>());
		return;
	}
	// The centre's fan, then each ring's band, out to the skirt; every face wound to be seen from below, the only side
	// it is ever seen from (the kit's winding rule, so a one-sided paint shows).
	CeilingTriangles.Reset();
	auto Wind = [this](int32 A, int32 B, int32 C)
	{
		const FVector Facing = (CeilingPlaces[B] - CeilingPlaces[C]) ^ (CeilingPlaces[A] - CeilingPlaces[C]);
		if ((Facing | FVector::DownVector) >= 0.0)
		{
			CeilingTriangles.Append({ A, B, C });
		}
		else
		{
			CeilingTriangles.Append({ A, C, B });
		}
	};
	const int32 Spokes = SeaUnder::CeilingSpokes, Rings = SeaUnder::CeilingRings;
	for (int32 S = 0; S < Spokes; ++S)
	{
		Wind(0, 1 + S, 1 + (S + 1) % Spokes);
	}
	for (int32 R = 0; R + 1 < Rings; ++R)
	{
		for (int32 S = 0; S < Spokes; ++S)
		{
			const int32 A = 1 + R * Spokes + S, B = 1 + R * Spokes + (S + 1) % Spokes;
			Wind(A, B, B + Spokes);
			Wind(A, B + Spokes, A + Spokes);
		}
	}
	Sheet->CreateMeshSection(0, CeilingPlaces, CeilingTriangles, CeilingNormals, TArray<FVector2D>(), CeilingColours, TArray<FProcMeshTangent>(), false);
	bCeilingLaid = true;
}

void FSeaUnder::LayLooks(const SeaShow::Show& Show, double Now, const SeaUnder::P3& At, const FRotator& Facing)
{
	using namespace SeaUnderParts;
	if (!Room.Owner)
	{
		return;
	}
	UProceduralMeshComponent* Mesh = Looks.Get();
	if (!Mesh)
	{
		Mesh = MakeMesh(TEXT("UnderSeaLooks"));
		Looks = Mesh;
		SectionsLaid = 0;
		// Its charges: the map's surface paint, lit, with the settings ConvoyLooks gives its own things in the air. The
		// soft things: M_Highlight. The light: M_Glow.
		UMaterialInterface* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_MapSurface.M_MapSurface"));
		UMaterialInstanceDynamic* Solid = UMaterialInstanceDynamic::Create(Surface ? Surface : UMaterial::GetDefaultMaterial(MD_Surface), Mesh);
		Solid->SetScalarParameterValue(TEXT("Roughness"), 0.6f);
		Solid->SetScalarParameterValue(TEXT("Specular"), 0.35f);
		Solid->SetScalarParameterValue(TEXT("Grain"), 0.0f);
		Solid->SetScalarParameterValue(TEXT("Tooth"), 0.0f);
		Mesh->SetMaterial(SolidSection, Solid);
		if (UMaterialInterface* Soft = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight")))
		{
			Mesh->SetMaterial(SoftSection, Soft);
		}
		if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Glow.M_Glow")))
		{
			UMaterialInstanceDynamic* Lit = UMaterialInstanceDynamic::Create(Glow, Mesh);
			Lit->SetScalarParameterValue(TEXT("Strength"), 2.0f);
			Mesh->SetMaterial(GlowSection, Lit);
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Sea battle: M_Glow is missing (run Tools/build_content.sh): no flash and no sun shafts under the water."));
		}
	}
	const int32 Who = BoatNow();
	const SeaShow::ShipOut* Boat = Who >= 0 ? Show.Ship(Who) : nullptr;
	const SeaUnder::BoatPose Pose = Boat ? SeaUnder::PoseOf(*Boat) : SeaUnder::BoatPose();
	LastBoatAt = Pose.At;
	// (the frame and the sections laid into what was kept from the last frame, their room kept)
	Sea.Lay(Now, At, Pose.bThere ? &Pose : nullptr, Daylight, SunUp, SunFrom, InWater);
	static_assert(Sections == LooksSections, "one kept section for each of the looks mesh's sections");
	for (FLaid& Section : Laid)
	{
		Section.Reset();
	}
	const double U = Room.UnitsPerMetre;
	// (the camera's own frame: the flat things face it, so they are never seen edge on)
	const FQuat Turn = Facing.Quaternion();
	const FVector Across = Turn.GetAxisY(), Up = Turn.GetAxisZ(), Toward = -Turn.GetAxisX();
	for (const SeaUnder::Drum& D : InWater.Drums)
	{
		// [P] The Mk VII's drum, 0.71 m long and 0.46 across, lying across its fall and turning as it sinks; an ahead
		// bomb about 1.2 m with its fins and 0.18 across, nose down.
		const double Long = (D.bBomb ? 1.2 : 0.71) * U, Radius = (D.bBomb ? 0.09 : 0.23) * U;
		const FVector Axis = D.bBomb ? FVector::UpVector : FRotator(D.Tilt, D.Yaw, 0.0).Vector();
		const FVector Mid = World(D.At);
		Laid[SolidSection].Rod(Mid - Axis * (0.5 * Long), Mid + Axis * (0.5 * Long), Radius, 8, ChargeDark);
	}
	for (const SeaUnder::P3& P : InWater.Motes)
	{
		Laid[SoftSection].Flat(World(P), Across * (0.03 * U), Up * (0.03 * U), Toward, Silt);
	}
	for (const SeaUnder::Glob& G : InWater.Globes)
	{
		const uint8 Alpha = static_cast<uint8>(FMath::Clamp(G.Alpha * 255.0, 0.0, 255.0));
		Laid[SoftSection].Disc(World(G.At), Across, Up, G.R * U, 16, Toward, FColor(Froth.R, Froth.G, Froth.B, static_cast<uint8>(Alpha / 3)), FColor(Froth.R, Froth.G, Froth.B, Alpha));
	}
	// (the bursts' bubbles, and her air once she is lost: alike to look at)
	for (const std::vector<SeaUnder::Glob>* Rising : { &InWater.Bubbles, &InWater.Air })
	{
		for (const SeaUnder::Glob& G : *Rising)
		{
			const uint8 Alpha = static_cast<uint8>(FMath::Clamp(G.Alpha * 255.0, 0.0, 255.0));
			Laid[SoftSection].Disc(World(G.At), Across, Up, G.R * U, 6, Toward, FColor(Froth.R, Froth.G, Froth.B, static_cast<uint8>(Alpha / 2)), FColor(Froth.R, Froth.G, Froth.B, Alpha));
		}
	}
	for (const SeaUnder::Glob& G : InWater.Flashes)
	{
		const uint8 Alpha = static_cast<uint8>(FMath::Clamp(G.Alpha * 255.0, 0.0, 255.0));
		Laid[GlowSection].Disc(World(G.At), Across, Up, G.R * U, 12, Toward, FColor(255, 240, 200, Alpha), FColor(255, 220, 160, 0));
	}
	for (const SeaUnder::Shaft& S : InWater.Shafts)
	{
		const FVector Way = FVector(S.Way.Y, S.Way.X, S.Way.Z).GetSafeNormal();
		const FVector Mid = World(S.Top) + Way * (0.5 * S.Long * U);
		const FVector Side = FVector::CrossProduct(Way, Toward).GetSafeNormal() * (0.5 * S.Wide * U);
		Laid[GlowSection].Flat(Mid, Side, Way * (0.5 * S.Long * U), Toward, FColor(200, 235, 210, static_cast<uint8>(FMath::Clamp(S.Strength * 255.0, 0.0, 255.0))));
	}
	// (made afresh only when a section's shape changed, as ConvoyLooks and the wreck mesh do)
	for (int32 I = 0; I < Sections; ++I)
	{
		const FLaid& L = Laid[I];
		if (!L.IsEmpty())
		{
			if ((SectionsLaid & (1 << I)) && Shapes[I].Holds(L.Points.Num(), L.Triangles.GetData(), L.Triangles.Num()))
			{
				Mesh->UpdateMeshSection(I, L.Points, L.Normals, TArray<FVector2D>(), L.Colours, TArray<FProcMeshTangent>());
			}
			else
			{
				Mesh->CreateMeshSection(I, L.Points, L.Triangles, L.Normals, TArray<FVector2D>(), L.Colours, TArray<FProcMeshTangent>(), false);
				Shapes[I].Keep(L.Points.Num(), L.Triangles.GetData(), L.Triangles.Num());
			}
			SectionsLaid |= 1 << I;
		}
		else if (SectionsLaid & (1 << I))
		{
			Mesh->ClearMeshSection(I);
			Shapes[I].Forget();
			SectionsLaid &= ~(1 << I);
		}
	}
}

void FSeaUnder::Hear(const SeaShow::Show& Show, double Now, bool bSkip)
{
	using namespace SeaUnderParts;
	const double Since = HeardTo;
	HeardTo = Now;
	const int32 Who = BoatNow();
	const SeaShow::ShipOut* Boat = Who >= 0 ? Show.Ship(Who) : nullptr;
	if (bSkip || Since < 0.0 || !Boat)
	{
		return;
	}
	// The asdic on her hull while an escort's is on her (SeaUnder::Sight): a ping, the next sooner the nearer he is; it
	// stops as he loses her close under him, and then the charges come.
	int Escort = -1;
	if (Pings.Pinged(Boat->Piece, Now, Escort))
	{
		SeaShow::Vec Where;
		double Face = 0.0;
		const double Far = Show.PieceMiddle(Escort, Where, Face) ? FMath::Sqrt(FMath::Square(Where.X - Boat->At.X) + FMath::Square(Where.Y - Boat->At.Y)) : 1500.0;
		if (PingAt < 0.0 || Now >= PingAt)
		{
			Play(TEXT("under_ping"), static_cast<float>(FMath::Clamp(1.2 - Far / 2500.0, 0.35, 1.0)));
			PingAt = Now + FMath::Clamp(Far / 700.0 * PingClose, PingClose, PingMost);
		}
	}
	else
	{
		PingAt = -1.0;
	}
	// The hull's creaks (SeaUnder::Water::CreaksBetween): her depth, how long she has been held down, and each burst.
	Creaks.clear();
	Sea.CreaksBetween(Since, Now, WindowFrom(), static_cast<uint64_t>(static_cast<uint32>(Who)), LastKeel < 0.0 ? Boat->Keel : LastKeel, Boat->Keel,
		SeaUnder::P3(Boat->At.X, Boat->At.Y, -Boat->Keel), LastCreak, Creaks);
	LastKeel = Boat->Keel;
	for (const SeaUnder::Creak& C : Creaks)
	{
		Play(*FString::Printf(TEXT("hull_creak_%d"), C.Which), static_cast<float>(C.Gain));
		if (C.bRattle)
		{
			Play(TEXT("hull_rattle"), 0.8f);
		}
	}
}

void FSeaUnder::Beds(UAudioComponent* DayBed, float DayLevel, UAudioComponent* BattleBed, float BattleLevel, const SeaShow::Show& Show)
{
	using namespace SeaUnderParts;
	// Nothing of the view's ever made or pressed down (off, or not yet under): the beds are the battle's alone, as they
	// were before the view (the battle's own fades in at contact over 4 s to its level, untouched).
	const bool bHeard = bOn && bUnder;
	if (!bUnder && !bBedDulled[0] && !bBedDulled[1] && !UnderBed.IsValid() && !Screws.IsValid())
	{
		return;
	}
	// The sea's beds pressed down and dulled as the eye goes under, and given back to their own levels as it comes up;
	// touched only at the face (a bed first heard on the surface is left as it is; one that comes in while under, the
	// battle's at contact, is dulled as it comes).
	UAudioComponent* Given[2] = { DayBed, BattleBed };
	const float Own[2] = { DayLevel, BattleLevel };
	for (int32 K = 0; K < 2; ++K)
	{
		if (!Given[K])
		{
			continue;
		}
		if (BedsSeen[K].Get() != Given[K])
		{
			BedsSeen[K] = Given[K];
			bBedDulled[K] = false;
		}
		if (bBedDulled[K] == bUnder)
		{
			continue;
		}
		bBedDulled[K] = bUnder;
		Given[K]->SetLowPassFilterEnabled(bUnder);
		Given[K]->SetLowPassFilterFrequency(MuffledAbove);
		Given[K]->AdjustVolume(0.4f, Own[K] * (bUnder ? 0.25f : 1.0f));
	}
	// The water's own roar and the escort's screws: one component each, made once, faded in under the water and out at the
	// face, or as soon as the view is turned off (Shift+U while under), so neither plays on above.
	if (bHeard && !UnderBed.IsValid())
	{
		UnderBed = MakeBed(TEXT("under_bed"));
	}
	if (bHeard && !Screws.IsValid())
	{
		Screws = MakeBed(TEXT("under_screws"));
	}
	if (UAudioComponent* Roar = UnderBed.Get())
	{
		if (bHeard && !Roar->IsPlaying())
		{
			Roar->SetVolumeMultiplier(0.5f);
			Roar->FadeIn(0.6f, 0.5f);
		}
		else if (!bHeard && Roar->IsPlaying())
		{
			Roar->FadeOut(0.4f, 0.0f);
		}
	}
	UAudioComponent* Thrashing = Screws.Get();
	if (!Thrashing)
	{
		return;
	}
	if (!bHeard)
	{
		if (Thrashing->IsPlaying())
		{
			Thrashing->FadeOut(0.4f, 0.0f);
		}
		return;
	}
	// The nearest escort's screws over her (500 m and in, the louder the nearer and the faster she runs).
	const SeaUnder::P3 Eye = BoardOf(EyeNow);
	double Best = 0.0, Pitch = 1.0;
	for (const SeaShow::ShipOut& Ship : Show.Ships())
	{
		if (Ship.Class == "ss" || Ship.Doing == SeaShow::Fate::Gone || Ship.Speed < 2.0)
		{
			continue;
		}
		const double Far = FMath::Sqrt(FMath::Square(Ship.At.X - Eye.X) + FMath::Square(Ship.At.Y - Eye.Y));
		const double Loud = (1.0 - FMath::SmoothStep(60.0, ScrewsHeard, Far)) * FMath::Min(1.0, Ship.Speed / 8.0);
		if (Loud > Best)
		{
			Best = Loud;
			Pitch = 0.8 + 0.4 * FMath::Min(1.0, Ship.Speed / 12.0);
		}
	}
	Thrashing->SetVolumeMultiplier(static_cast<float>(FMath::Max(0.001, Best)));
	Thrashing->SetPitchMultiplier(static_cast<float>(Pitch));
	if (!Thrashing->IsPlaying())
	{
		Thrashing->Play();
	}
}

void FSeaUnder::Muffle(UAudioComponent* Heard) const
{
	// A sound from above the sea heard under it: the face sends nearly all of it back up; what comes through is low.
	if (Heard && bUnder)
	{
		Heard->SetLowPassFilterEnabled(true);
		Heard->SetLowPassFilterFrequency(SeaUnderParts::MuffledAbove);
		Heard->SetVolumeMultiplier(Heard->VolumeMultiplier * SeaUnderParts::MuffledShare);
	}
}

float FSeaUnder::LightShare() const
{
	// (the sun's lamp under the water: what reaches her middle as the window opened, of what reaches the face, MurkAt's 0.6)
	return bUnder ? static_cast<float>(FMath::Clamp(LightAtBoat / 0.6, 0.1, 1.0)) : 1.0f;
}

FLinearColor FSeaUnder::LightTint() const
{
	// [EST] the water takes the red first: green by day, a colder green by night
	return !bUnder ? FLinearColor::White : (Daylight >= 0.3 ? FLinearColor(0.62f, 0.92f, 0.80f) : FLinearColor(0.50f, 0.72f, 0.78f));
}

double FSeaUnder::Shudder(int32 ShipKey, double Now) const
{
	// (only the boat in the picture, only under the water: the surface's picture is as it was)
	return bUnder && ShipKey == BoatNow() ? Sea.ShudderAt(Now, LastBoatAt) : 0.0;
}

FVector FSeaUnder::ShakeAt(double Now) const
{
	if (!bUnder)
	{
		return FVector::ZeroVector;
	}
	const SeaUnder::P3 Shake = Sea.ShakeAt(Now, BoardOf(EyeNow));
	return FVector(Shake.Y, Shake.X, Shake.Z) * Room.UnitsPerMetre;
}

double FSeaUnder::PaceCap(double InSpeed, bool bOwnPace) const
{
	// Under the water the battle runs no faster than one and a half times: the charges take 20 s to sink, and the film
	// needs them to. A picture run and the month's scene keep their own pace (the stager's windows are timed).
	return bUnder && !bOwnPace ? FMath::Min(InSpeed, 1.5) : InSpeed;
}

void FSeaUnder::Took(double Seconds)
{
	if (bUnder && FramesUnder.Num() < 1000000)
	{
		FramesUnder.Add(static_cast<float>(Seconds));
	}
}

void FSeaUnder::SayFrameTimes() const
{
	if (FramesUnder.Num() == 0)
	{
		return;
	}
	TArray<float> Sorted = FramesUnder;
	Sorted.Sort();
	double Sum = 0.0;
	for (const float Each : Sorted)
	{
		Sum += Each;
	}
	auto At = [&Sorted](double Share) { return 1000.0 * Sorted[FMath::Clamp(FMath::FloorToInt32(Share * (Sorted.Num() - 1) + 0.5), 0, Sorted.Num() - 1)]; };
	UE_LOG(LogTheater, Display, TEXT("Sea battle: frame times under the water over %d frames (%.0f s): mean %.1f ms (%.0f a second), middle %.1f, 95th in the hundred %.1f, 99th %.1f, worst %.1f."),
		Sorted.Num(), Sum, 1000.0 * Sum / Sorted.Num(), Sum > 0.0 ? Sorted.Num() / Sum : 0.0, At(0.5), At(0.95), At(0.99), At(1.0));
}

void FSeaUnder::Skipped()
{
	// The battle jumped ahead: nothing of what was skipped is heard, and the hull starts creaking afresh.
	HeardTo = -1.0;
	PingAt = -1.0;
	LastCreak = -1e9;
	LastKeel = -1.0;
}

void FSeaUnder::End()
{
	if (bUnder)
	{
		ComeUp();
		bUnder = false;
	}
	for (TWeakObjectPtr<UAudioComponent>* Each : { &UnderBed, &Screws })
	{
		if (UAudioComponent* Playing = Each->Get())
		{
			Playing->Stop();
			Playing->DestroyComponent();
		}
		Each->Reset();
	}
	for (TWeakObjectPtr<UProceduralMeshComponent>* Each : { &Ceiling, &Looks })
	{
		if (UProceduralMeshComponent* Mesh = Each->Get())
		{
			Mesh->DestroyComponent();
		}
		Each->Reset();
	}
	for (int32 K = 0; K < 2; ++K)
	{
		BedsSeen[K].Reset();
		bBedDulled[K] = false;
	}
	Sounds.Reset();
	SoundsMissing.Reset();
	bCeilingLaid = false;
	SectionsLaid = 0;
}

int32 FSeaUnder::BoatNow() const
{
	if (HandBoat >= 0)
	{
		return HandBoat;
	}
	return ShownWindow >= 0 && ShownWindow < static_cast<int32>(Plan.size()) ? Plan[ShownWindow].Boat : -1;
}

double FSeaUnder::WindowFrom() const
{
	if (HandBoat >= 0)
	{
		return HandSince;
	}
	return ShownWindow >= 0 && ShownWindow < static_cast<int32>(Plan.size()) ? Plan[ShownWindow].From : 0.0;
}

UAudioComponent* FSeaUnder::MakeBed(const TCHAR* Name)
{
	USoundBase* Sound = SoundOf(Name, true);
	if (!Sound || !Room.Owner)
	{
		return nullptr;
	}
	// (as the air scene lays its own beds, AAirBattle::Lay: made 2D and not destroyed when it ends, so the one component
	// serves every crossing of the face instead of a new one each time)
	UAudioComponent* Made = UGameplayStatics::CreateSound2D(Room.Owner, Sound, 0.001f, 1.0f, 0.0f, nullptr, false, false);
	if (Made)
	{
		Made->bIsUISound = false; // (CreateSound2D makes an interface sound, which would play on through a pause)
	}
	return Made;
}

USoundBase* FSeaUnder::SoundOf(const TCHAR* Name, bool bLoop)
{
	// The sound director's pack first (/Game/Campaign/Sounds/Real), then the made ones (Tools/make_sounds.mjs, brought
	// in by build_sounds.py to /Game/Campaign/Sounds); one missing is said once and stays silent.
	const FName Key(Name);
	if (const TStrongObjectPtr<USoundBase>* Found = Sounds.Find(Key))
	{
		return Found->Get();
	}
	if (SoundsMissing.Contains(Key))
	{
		return nullptr;
	}
	USoundBase* Got = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Campaign/Sounds/Real/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Got)
	{
		Got = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Campaign/Sounds/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!Got)
	{
		SoundsMissing.Add(Key);
		UE_LOG(LogTheater, Display, TEXT("Sea battle: no sound %s for the view under the water (Tools/build_sounds.sh brings it in): silent."), Name);
		return nullptr;
	}
	if (bLoop)
	{
		if (USoundWave* Wave = Cast<USoundWave>(Got))
		{
			Wave->bLooping = true;
		}
	}
	Sounds.Add(Key, TStrongObjectPtr<USoundBase>(Got));
	return Got;
}

void FSeaUnder::Play(const TCHAR* Name, float Volume, float Pitch)
{
	if (USoundBase* Sound = SoundOf(Name))
	{
		UGameplayStatics::PlaySound2D(Room.Owner, Sound, Volume, Pitch);
	}
}
