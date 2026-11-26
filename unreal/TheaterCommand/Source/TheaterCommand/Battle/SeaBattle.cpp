#include "Battle/SeaBattle.h"

#include "Battle/AirStage.h"
#include "Battle/DeckCrewCore.h"
#include "Battle/SeaBattleScreen.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaSailors.h"
#include "Battle/SeaShow.h"
#include "Battle/ShipGuns.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignBattleLook.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignFlags.h"
#include "Campaign/CampaignGameMode.h"
#include "Campaign/FilmCritic.h" // film critic: the development facts (-FilmCritic)
#include "Campaign/CampaignHUD.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "StaticMeshAttributes.h"
#include "TheaterCommand.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SDPIScaler.h"

#include <cctype>
#include <map>
#include <string>
#include <utility>
#include <vector>

/** What the scene keeps of the action: the recording, the playback core, and the battle's clock. */
struct FSeaBattleState
{
	explicit FSeaBattleState(uint64 Seed) : Show(Seed) {}
	SeaFeed::Recording Rec;
	SeaShow::Show Show;
	size_t Taken = 0;       // the recording's views handed to the show so far
	double BattleT = 0.0;   // the battle's clock now (seconds)
	double BeginT = 0.0, EndT = 0.0;
	/** The sailor files read (Content/Models/Soldiers/soldier_<ARMY>_sailor.json), and the navies' dresses over them as
	 * asked (SeaParts::DressFor): by army, file and variant, their place in Dresses. */
	SeaSailors::Crew Crew;
	std::vector<SeaSailors::Dress> Dresses;
	std::map<std::string, int> DressAt;
	/** The guns' crews (Battle/DeckCrewCore): each armed ship's watch by her key, stepped every frame whether she is drawn
	 * or not, and each sailor file's gun clips' blocks, by file. */
	std::map<int, DeckCrew::Watch> GunWatches;
	std::map<int, DeckCrew::Blocks> GunBlocks;
	/** Each ship's gun her crew work (by her key twice, one more for a merchant's stern gun): its mount on her model and
	 * its shape, the stage's name for it; an empty shape where her model has none. */
	struct FGunOf
	{
		ConvoyLook::GunMount Mount;
		DeckCrew::GunShape Shape;
		FString Part;
	};
	std::map<int, FGunOf> GunsOf;
};

namespace SeaParts
{
	/** The stage's way down for the show's (a convoy's boat crushed, scuttled, rammed or shelled goes as one that sinks). */
	static FString StageWay(const std::string& How)
	{
		return How == "crushed" || How == "scuttled" || How == "rammed" || How == "shelled" ? FString(TEXT("sink")) : FString(UTF8_TO_TCHAR(How.c_str()));
	}

	static FString Wide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	static std::string Narrow(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	/** A heading (degrees clockwise from north) as a way on the board (X east, Y north), and the way to her starboard. */
	static FVector2D Ahead(double HeadingDegrees)
	{
		const double R = FMath::DegreesToRadians(HeadingDegrees);
		return FVector2D(FMath::Sin(R), FMath::Cos(R));
	}

	static FVector2D Starboard(double HeadingDegrees)
	{
		const double R = FMath::DegreesToRadians(HeadingDegrees);
		return FVector2D(FMath::Cos(R), -FMath::Sin(R));
	}

	static float FromDb(double Gain)
	{
		return static_cast<float>(FMath::Pow(10.0, Gain / 20.0));
	}

	/** The kit's sea distance where it has one (the effects head 78f6edf: drawn out to 16 km, the far line's flashes,
	 * columns and fires seen 5 to 15 km off), else its air distance (4 km), so this builds on the kit before and after. */
	template <typename TKit>
	static void UseSeaDistance(TKit* Kit)
	{
		if constexpr (requires(TKit* K) { K->UseSeaDistance(); })
		{
			Kit->UseSeaDistance();
		}
		else
		{
			Kit->UseAirDistance();
		}
	}

	/** Whether the kit has funnel smoke of its own (the effects head 52fffe1: the trail kind "funnel", whose TrailMove
	 * takes her power), so that the scene smokes from her funnels only where the kit can draw it, and builds on both. */
	template <typename TKit>
	static constexpr bool HasFunnelSmoke()
	{
		return requires(TKit* K, const FVector& V) { K->TrailMove(0.0, int32(0), V, V, 0.5f); };
	}

	/** A funnel's smoke this frame, with her power (0 to 1, her speed over her flank speed): only on a kit that has it. */
	template <typename TKit>
	static void FunnelMove(TKit* Kit, double Now, int32 Id, const FVector& At, const FVector& Velocity, float Power)
	{
		if constexpr (HasFunnelSmoke<TKit>())
		{
			Kit->TrailMove(Now, Id, At, Velocity, Power);
		}
	}

	/** A puff of steam from a broken ship (the kit's Steam, where the kit has it; none on an older kit). */
	template <typename TKit>
	static void SteamPuff(TKit* Kit, int32 Id, const FVector& At, const FVector& Way, float Across, float Seconds)
	{
		if constexpr (requires(TKit* K, int32 I, const FVector& A, float C, const FString& W) { K->Steam(I, A, A, C, C, W); })
		{
			Kit->Steam(Id, At, Way, Across, Seconds, FString(TEXT("vent")));
		}
		else
		{
			(void)Kit;
			(void)Id;
			(void)At;
			(void)Way;
			(void)Across;
			(void)Seconds;
		}
	}

	/**
	 * Where the sun stands (degrees: its bearing clockwise from north, its height) at a latitude, in a month (0 January)
	 * and at an hour of the day (local sun time): the declination of the month's middle day and the hour's angle. The
	 * recordings carry the year and month, not the hour; the scene takes the hour from the battle's seed, by day.
	 */
	static void SunAt(double Latitude, int32 Month0, double Hour, double& OutAzimuth, double& OutElevation)
	{
		const double Day = 30.4 * FMath::Clamp(Month0, 0, 11) + 15.0;
		const double Decl = FMath::DegreesToRadians(23.44) * FMath::Sin(2.0 * UE_DOUBLE_PI * (284.0 + Day) / 365.0);
		const double Phi = FMath::DegreesToRadians(FMath::Clamp(Latitude, -80.0, 80.0));
		const double H = FMath::DegreesToRadians(15.0 * (Hour - 12.0));
		const double SinUp = FMath::Sin(Phi) * FMath::Sin(Decl) + FMath::Cos(Phi) * FMath::Cos(Decl) * FMath::Cos(H);
		OutElevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinUp, -1.0, 1.0)));
		const double Bearing = FMath::Atan2(-FMath::Sin(H) * FMath::Cos(Decl), FMath::Sin(Decl) * FMath::Cos(Phi) - FMath::Cos(Decl) * FMath::Sin(Phi) * FMath::Cos(H));
		OutAzimuth = FMath::Fmod(FMath::RadiansToDegrees(Bearing) + 360.0, 360.0);
	}

	/** The effects kit's size row for a column of water (the kit has no naval rows yet: the nearest it has; "small", a
	 * boat's deck gun's or a merchant's gun's round, the kit's 105 mm shell's). */
	static const TCHAR* SplashRow(const std::string& What)
	{
		return What == "heavy" ? TEXT("bomb500") : What == "medium" ? TEXT("bomb250") : What == "small" ? TEXT("how105") : TEXT("how150");
	}

	/** The effects kit's nation rows (us, gb, de, it, jp, su, as the air scene's KitNation) from a ship's nation (USA,
	 * UK, GER, ITA, JAP, SOV): empty for any other, which the kit draws in its default colour. */
	static FString KitNation(const std::string& Nation)
	{
		std::string Up;
		for (const char Ch : Nation) Up += static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
		if (Up == "USA" || Up == "US") return TEXT("us");
		if (Up == "UK" || Up == "GBR" || Up == "GB" || Up == "ENG") return TEXT("gb");
		if (Up == "GER" || Up == "DE") return TEXT("de");
		if (Up == "ITA" || Up == "IT") return TEXT("it");
		if (Up == "JAP" || Up == "JPN" || Up == "JP") return TEXT("jp");
		if (Up == "SOV" || Up == "USSR" || Up == "SU") return TEXT("su");
		return FString();
	}

	/** A ship's light guns firing at aircraft (Battle/ShipGuns' streams), in her nation's tracer colour where the kit
	 * takes the nation (as the air scene's AirGun); an older kit draws its one colour. */
	template <typename TKit>
	static void AirGun(TKit* Kit, double Now, int32 Gun, const FVector& Muzzle, const FVector& Aim, const FVector& Carry, const FString& Kind, float Rate, float Mv, const FString& Nation)
	{
		if constexpr (requires(TKit* K) { K->AirGun(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f, 0.0f, FString()); })
		{
			Kit->AirGun(Now, Gun, Muzzle, Aim, Carry, Kind, Rate, Mv, Nation);
		}
		else
		{
			Kit->AirGun(Now, Gun, Muzzle, Aim, Carry, Kind, Rate, Mv);
		}
	}

	/** A value of the sound director's mix by its dotted path ("air.flak.soundEvery"): null where it has none. */
	static TSharedPtr<FJsonValue> MixAt(const TSharedPtr<FJsonObject>& Mix, const FString& Path)
	{
		TArray<FString> Steps;
		Path.ParseIntoArray(Steps, TEXT("."));
		TSharedPtr<FJsonObject> On = Mix;
		for (int32 K = 0; On.IsValid() && K < Steps.Num(); ++K)
		{
			const TSharedPtr<FJsonValue> Got = On->TryGetField(Steps[K]);
			if (!Got.IsValid() || K == Steps.Num() - 1)
			{
				return Got;
			}
			On = Got->Type == EJson::Object ? Got->AsObject() : TSharedPtr<FJsonObject>();
		}
		return nullptr;
	}

	/** The mix's take there: its word, or the Pick-th of its list (wrapping); empty where it has none. */
	static FString MixTake(const TSharedPtr<FJsonObject>& Mix, const FString& Path, int32 Pick = 0)
	{
		const TSharedPtr<FJsonValue> Got = MixAt(Mix, Path);
		if (Got.IsValid() && Got->Type == EJson::String)
		{
			return Got->AsString();
		}
		if (Got.IsValid() && Got->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& List = Got->AsArray();
			if (List.Num() > 0)
			{
				const TSharedPtr<FJsonValue>& One = List[FMath::Abs(Pick) % List.Num()];
				if (One.IsValid() && One->Type == EJson::String)
				{
					return One->AsString();
				}
			}
		}
		return FString();
	}

	/** The mix's number there; for a list, its last (a take's part [from, to] gives where it ends). */
	static double MixNumber(const TSharedPtr<FJsonObject>& Mix, const FString& Path, double Else)
	{
		const TSharedPtr<FJsonValue> Got = MixAt(Mix, Path);
		if (Got.IsValid() && Got->Type == EJson::Number)
		{
			return Got->AsNumber();
		}
		if (Got.IsValid() && Got->Type == EJson::Array && Got->AsArray().Num() > 0 && Got->AsArray().Last().IsValid() && Got->AsArray().Last()->Type == EJson::Number)
		{
			return Got->AsArray().Last()->AsNumber();
		}
		return Else;
	}

	/** The sound of a ship's guns by her class (the battle pack's takes): a boat's deck gun and a merchant's gun, one gun of
	 * a field gun's size, the field gun's crack, not a warship's broadside. */
	static FName GunsOf(const std::string& Class)
	{
		if (Class == "ss" || Class == "mv")
		{
			return FName(TEXT("field_gun_fire"));
		}
		return Class == "bb" ? FName(TEXT("ship_gun_heavy")) : Class == "dd" ? FName(TEXT("ship_guns")) : FName(TEXT("ship_gun_medium"));
	}

	/**
	 * The miniature kit's model for a strike's aircraft: a sculpted one of the name where Content/Models has it, else the
	 * kit's plain fighter or bomber in its navy's paint (as the air battle's ModelFor).
	 */
	static FString ModelName(const FString& Model, const FString& Nation, bool bFighter)
	{
		static const TMap<FString, FString> Known = {
			{ TEXT("B5N"), TEXT("Kate") }, { TEXT("D3A"), TEXT("Val") }, { TEXT("SBD"), TEXT("Dauntless") }, { TEXT("TBD"), TEXT("Devastator") },
			{ TEXT("TBF"), TEXT("Avenger") }, { TEXT("F4F"), TEXT("Wildcat") }, { TEXT("A6M"), TEXT("Zero") },
		};
		static TMap<FString, FString> Found;
		const FString Asked = Model + TEXT("|") + Nation + (bFighter ? TEXT("|f") : TEXT("|b"));
		if (const FString* Answered = Found.Find(Asked))
		{
			return *Answered;
		}
		const FString* Named = Known.Find(Model);
		const FString Name = Named ? *Named : Model;
		FMiniatureMesh Probe;
		const FString Out = !Name.IsEmpty() && !Name.Contains(TEXT(":")) && Probe.Vehicle(Name + TEXT(":") + Nation, FVector::ZeroVector, 1.0, 0.0, false)
			? Name + TEXT(":") + Nation : (bFighter ? TEXT("Fighter:") : TEXT("Bomber:")) + Nation;
		Found.Add(Asked, Out);
		return Out;
	}

	/** The haze's colour as the room had it (the engine's field has had two names). */
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

	// The ids the scene gives the kit's lasting things, apart from each other and from the stage's (238000 and up).
	// (a ship's funnels: her key times four, and 0 to 2 a funnel each or 3 her one plume, from a million: out of the way)
	static constexpr int32 FireIds = 810000, FallIds = 830000, OilIds = 840000, FunnelIds = 1000000;
	// (a broken ship's smoke from her break, a trail, by her key; the steam from it, a puff at a time, by her key times 8)
	static constexpr int32 BreakIds = 1900000, BreakSteamIds = 2200000;
	/** Funnel smoke (the kit's 3,000 pieces are shared by everything): a plume a funnel on the NearFunnels ships nearest
	 * the eye, one plume a ship on the rest of the FunnelShips nearest, none beyond FunnelReach metres. At 25 knots a
	 * destroyer's plume is about 60 pieces near, 33 far; a capital ship's 36 and 30 (the effects thread, 21 November). */
	static constexpr int32 NearFunnels = 3, FunnelShips = 8;
	static constexpr double FunnelReach = 12000.0;
	/** Seconds the camera stands on a shot's subject before its moment (watched; a picture stands on it from the last). */
	static constexpr double CutBefore = 4.0;
	/** Frames a picture waits after a cut, for the renderer's history (its sharpening, the light it gathers) to fill. */
	static constexpr int32 SettleTicks = 10;
	/** The longest a wake point is kept (seconds: each fades by how far astern it lies, WakeRun), how long a stopped
	 * torpedo's streak takes to go, and how long a streak is behind its torpedo (metres; the art director's 11-19 ruling:
	 * 100 to 200 m). */
	static constexpr double WakeLife = 240.0, TrackFade = 15.0, TrackStreak = 150.0;
	/** The wreckage and men in the water (DrawWrecks): within LiveReach metres of the eye laid every frame, beyond it
	 * every WreckFarEvery seconds; a man drawn whole (head, jacket, limbs) where he is 7 pixels or more across, the
	 * MostDetailedMen of them at the most, the rest as stones in their colours. */
	static constexpr double LiveReach = 350.0, WreckFarEvery = 0.5;
	static constexpr int32 MostDetailedMen = 240;
	/**
	 * The men drawn whole as the sailor files' men (Battle/SeaSailors), where those are read: the nearest LiveMost of them
	 * LiveLeast pixels or more (his head and shoulders' 0.45 m across) skinned afresh every frame, the near level where
	 * NearLodFrom or more; the rest as copies of the pose bank's meshes, the mid level where MidLodFrom or more, else the
	 * far; SailorBuilds of the bank's meshes made a frame at the most (a man whose mesh waits is a stand-in meanwhile), and
	 * LiveNewMost live meshes made afresh (a new man, level, dress or hand: after a cut, the rest are the bank's copies for a
	 * frame or three). An oar's blade OarOutboard metres out from its rowlock (a whaleboat's 14 to 15 foot oar, with the
	 * clip's handle to rowlock).
	 */
	static constexpr int32 LiveMost = 10, SailorBuilds = 4, SailorBuildsAtCut = 10, LiveNewMost = 3;
	static constexpr double LiveLeast = 30.0, NearLodFrom = 70.0, MidLodFrom = 14.0, OarOutboard = 3.6;
	/** The most men in the water with a ring of white water about them (DrawMarks): the nearest. */
	static constexpr int32 MostFoamRings = 240;
	/** How far astern a ship's foam lasts (metres): three of her lengths, never under 300 m nor over 800 m (the art
	 * director, Samar r2e: "widens and breaks up over 300-800 m and fades to nothing"). */
	static double WakeRun(double Long)
	{
		return FMath::Clamp(3.0 * Long, 300.0, 800.0);
	}
	/** How whole a wake's foam still is (the material breaks up the rest) at Lengths of her length astern, Run her
	 * lengths to its end: the campaign map's passed curve, whole to half a length, about half left at one, a fifth at
	 * its end. */
	static double WakeWhole(double Lengths, double Run)
	{
		return Lengths < 0.5 ? FMath::Lerp(1.0, 0.58, Lengths / 0.5) : Lengths < 1.0 ? FMath::Lerp(0.58, 0.43, (Lengths - 0.5) / 0.5)
			: Lengths < 2.0 ? FMath::Lerp(0.43, 0.31, Lengths - 1.0) : FMath::Lerp(0.31, 0.2, FMath::Min((Lengths - 2.0) / FMath::Max(Run - 2.0, 0.1), 1.0));
	}
	/** A wake's half width at Lengths of her length astern: her stern's width, a beam by half a length, two at its end. */
	static double WakeHalf(double Beam, double Lengths, double Run)
	{
		return Beam * 0.5 * (Lengths < 0.5 ? FMath::Lerp(0.72, 1.0, Lengths / 0.5) : FMath::Lerp(1.0, 2.0, FMath::Min((Lengths - 0.5) / FMath::Max(Run - 0.5, 0.1), 1.0)));
	}
	/** A number from 0 to 1 that is the same each time for the same X (a foam edge's raggedness, an oil slick's). */
	static double Hash01(double X)
	{
		return FMath::Frac(FMath::Sin(X * 12.9898 + 78.233) * 43758.5453);
	}
	static const FLinearColor HazeInk(0.58f, 0.67f, 0.76f);
	static const FColor Foam(226, 232, 234);

	// WHERE A SHIP WENT DOWN (Brian, 22 November 05:32Z to 05:33Z: "sailors should jump in the water if the boat is going
	// down", "there should be debris floating in the water. maybe oil pools?"): what floats there and her men in the
	// water, as the show has them (SeaShow's Floats and Men), drawn solid and lit on the pieces' material.

	/** A place on the board (metres: X east, Y north, Z up) and three ways from it: X along, Y to the right, Z up. */
	struct FWreckFrame
	{
		FVector O = FVector::ZeroVector, X = FVector(0.0, 1.0, 0.0), Y = FVector(1.0, 0.0, 0.0), Z = FVector::UpVector;
		FVector At(double Along, double Right, double Up) const { return O + X * Along + Y * Right + Z * Up; }
		FVector Way(double Along, double Right, double Up) const { return X * Along + Y * Right + Z * Up; }
	};

	/** A frame at At, heading the way given (radians clockwise from north), its bow lifted by Pitch and its right side
	 * put down by Roll (radians). */
	static FWreckFrame WreckFrame(const FVector& At, double Heading, double Pitch = 0.0, double Roll = 0.0)
	{
		FWreckFrame F;
		F.O = At;
		const FVector Along(FMath::Sin(Heading), FMath::Cos(Heading), 0.0), Right(FMath::Cos(Heading), -FMath::Sin(Heading), 0.0);
		const FVector Lifted = Along * FMath::Cos(Pitch) + FVector::UpVector * FMath::Sin(Pitch);
		const FVector Up = FVector::UpVector * FMath::Cos(Pitch) - Along * FMath::Sin(Pitch);
		F.X = Lifted;
		F.Y = Right * FMath::Cos(Roll) - Up * FMath::Sin(Roll);
		F.Z = Up * FMath::Cos(Roll) + Right * FMath::Sin(Roll);
		return F;
	}

	/**
	 * The wreckage's mesh, built in board metres and laid in the world as the scene lays everything (X and Y change
	 * places, metres to units). Each point's paint as the pieces' material reads it: sRGB, its alpha what the paint
	 * lies on (255 cloth, wood, skin and rubber; 128 painted metal). A face shows the side its Out says, whichever way
	 * round its corners are given (FMiniatureMesh::Face's rule: the engine shows a triangle A B C from the side its
	 * (B - A) x (C - A) points away from).
	 */
	struct FWreckMesh
	{
		TArray<FVector> V, N;
		TArray<FColor> C;
		TArray<int32> T;
		double Units = 100.0;

		FVector World(const FVector& B) const { return FVector(B.Y * Units, B.X * Units, B.Z * Units); }
		static FVector WorldWay(const FVector& B) { return FVector(B.Y, B.X, B.Z).GetSafeNormal(); }
		int32 Point(const FVector& B, const FVector& Facing, const FColor& Paint)
		{
			V.Add(World(B));
			N.Add(WorldWay(Facing));
			C.Add(Paint);
			return V.Num() - 1;
		}
		/** A triangle of three points already laid, showing the side that faces Out (the world's way). */
		void TriWorld(int32 A, int32 B, int32 D, const FVector& Out)
		{
			const FVector Facing = (V[B] - V[A]) ^ (V[D] - V[A]);
			if (FVector::DotProduct(Facing, Out) < 0.0)
			{
				T.Append({ A, B, D });
			}
			else
			{
				T.Append({ A, D, B });
			}
		}
		/** The same, Out the board's way. */
		void Tri(int32 A, int32 B, int32 D, const FVector& Out)
		{
			TriWorld(A, B, D, WorldWay(Out));
		}
		/** A flat face round its corners (in order round its edge, convex), showing the side Out faces. */
		void Face(const FVector* P, int32 Count, const FVector& Out, const FColor& Paint)
		{
			const int32 First = V.Num();
			for (int32 K = 0; K < Count; ++K)
			{
				Point(P[K], Out, Paint);
			}
			for (int32 K = 1; K + 1 < Count; ++K)
			{
				Tri(First, First + K, First + K + 1, Out);
			}
		}
		void Quad(const FVector& A, const FVector& B, const FVector& D, const FVector& E, const FVector& Out, const FColor& Paint)
		{
			const FVector P[4] = { A, B, D, E };
			Face(P, 4, Out, Paint);
		}
		/** A box in a frame: its middle (along, right, up) and its size; its face under the water left off. */
		void Box(const FWreckFrame& F, double A, double B, double U, double Long, double Wide, double High, const FColor& Paint, const FColor& Top)
		{
			const double L = Long * 0.5, W = Wide * 0.5, H = High * 0.5;
			auto P = [&](double X, double Y, double Z) { return F.At(A + X * L, B + Y * W, U + Z * H); };
			Quad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), F.Z, Top);
			Quad(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), F.X, Paint);
			Quad(P(-1, -1, -1), P(-1, 1, -1), P(-1, 1, 1), P(-1, -1, 1), -F.X, Paint);
			Quad(P(-1, 1, -1), P(1, 1, -1), P(1, 1, 1), P(-1, 1, 1), F.Y, Paint);
			Quad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), -F.Y, Paint);
		}
		/**
		 * An egg about Middle (a place in the frame), its half sizes along, across and up, smooth: Rings bands from top
		 * to bottom (2: an eight-faced stone, a far man), Sides round. Paint by height: Top over the upper third, Mid
		 * between, Low under the lower third.
		 */
		void Egg(const FWreckFrame& F, const FVector& Middle, const FVector& Half, int32 Rings, int32 Sides, const FColor& Top, const FColor& Mid, const FColor& Low)
		{
			const FVector Centre = World(F.At(Middle.X, Middle.Y, Middle.Z));
			auto Paint = [&](double Z) { return Z > 0.34 ? Top : Z < -0.34 ? Low : Mid; };
			auto Along = [&](double Ax, double Ay, double Az)
			{
				const FVector P = F.At(Middle.X + Ax * Half.X, Middle.Y + Ay * Half.Y, Middle.Z + Az * Half.Z);
				const FVector Facing = F.Way(Ax / FMath::Max(Half.X, 0.001), Ay / FMath::Max(Half.Y, 0.001), Az / FMath::Max(Half.Z, 0.001));
				return Point(P, Facing, Paint(Az));
			};
			// (out from its middle through the triangle's: the way each of its faces looks, as an egg is round)
			auto Shell = [&](int32 A, int32 B, int32 D) { TriWorld(A, B, D, (V[A] + V[B] + V[D]) / 3.0 - Centre); };
			const int32 North = Along(0.0, 0.0, 1.0);
			TArray<int32> Ring, Last;
			for (int32 R = 1; R < Rings; ++R)
			{
				const double Lat = UE_DOUBLE_PI * (0.5 - static_cast<double>(R) / Rings);
				Ring.Reset();
				for (int32 S = 0; S < Sides; ++S)
				{
					const double Lon = 2.0 * UE_DOUBLE_PI * S / Sides;
					Ring.Add(Along(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat)));
				}
				for (int32 S = 0; S < Sides; ++S)
				{
					const int32 A = Ring[S], B = Ring[(S + 1) % Sides];
					if (R == 1)
					{
						Shell(North, A, B);
					}
					else
					{
						const int32 P0 = Last[S], P1 = Last[(S + 1) % Sides];
						Shell(P0, P1, B);
						Shell(P0, B, A);
					}
				}
				Last = Ring;
			}
			const int32 South = Along(0.0, 0.0, -1.0);
			for (int32 S = 0; S < Last.Num(); ++S)
			{
				Shell(South, Last[S], Last[(S + 1) % Last.Num()]);
			}
		}
		/** A rod from one place to another, Sides round (an arm, a leg, an oar's loom), smooth, open at its ends. */
		void Rod(const FVector& From, const FVector& To, double R, int32 Sides, const FColor& Paint)
		{
			const FVector Axis = (To - From).GetSafeNormal();
			if (Axis.IsNearlyZero())
			{
				return;
			}
			const FVector Side = (FMath::Abs(Axis.Z) < 0.9 ? Axis ^ FVector::UpVector : Axis ^ FVector(1.0, 0.0, 0.0)).GetSafeNormal();
			const FVector Other = Axis ^ Side;
			const int32 First = V.Num();
			for (int32 S = 0; S < Sides; ++S)
			{
				const double Turn = 2.0 * UE_DOUBLE_PI * S / Sides;
				const FVector Out = Side * FMath::Cos(Turn) + Other * FMath::Sin(Turn);
				Point(From + Out * R, Out, Paint);
				Point(To + Out * R, Out, Paint);
			}
			for (int32 S = 0; S < Sides; ++S)
			{
				const int32 A = First + 2 * S, B = First + 2 * ((S + 1) % Sides);
				const double Turn = 2.0 * UE_DOUBLE_PI * (S + 0.5) / Sides;
				const FVector Out = Side * FMath::Cos(Turn) + Other * FMath::Sin(Turn);
				Tri(A, B, B + 1, Out);
				Tri(A, B + 1, A + 1, Out);
			}
		}
		/**
		 * Tubing bent round an oval, smooth (a Carley float's body, a life ring): the oval's half sizes along and across
		 * (to the tube's middle) and the tube's radius, its middle Up over the frame's place; Band paints the outer side
		 * about the middle of the tube (a Carley float's lifeline, a life ring's stripes where Stripes is over 0).
		 */
		void Tube(const FWreckFrame& F, double HalfLong, double HalfWide, double R, double Up, int32 Round, int32 Sides, const FColor& Paint, const FColor& Band, int32 Stripes = 0)
		{
			const int32 First = V.Num();
			for (int32 K = 0; K < Round; ++K)
			{
				const double A = 2.0 * UE_DOUBLE_PI * K / Round;
				const FVector Out = F.Way(FMath::Cos(A) / FMath::Max(HalfLong, 0.01), FMath::Sin(A) / FMath::Max(HalfWide, 0.01), 0.0).GetSafeNormal();
				const FVector Middle = F.At(FMath::Cos(A) * HalfLong, FMath::Sin(A) * HalfWide, Up);
				const bool bStripe = Stripes > 0 && (K * Stripes * 2 / Round) % 2 == 1;
				for (int32 S = 0; S < Sides; ++S)
				{
					const double B = 2.0 * UE_DOUBLE_PI * S / Sides;
					const FVector Facing = Out * FMath::Cos(B) + F.Z * FMath::Sin(B);
					const bool bBand = Stripes > 0 ? bStripe : FMath::Cos(B) > 0.7 && FMath::Abs(FMath::Sin(B)) < 0.5;
					Point(Middle + Facing * R, Facing, bBand ? Band : Paint);
				}
			}
			for (int32 K = 0; K < Round; ++K)
			{
				for (int32 S = 0; S < Sides; ++S)
				{
					const int32 A = First + K * Sides + S, B = First + ((K + 1) % Round) * Sides + S;
					const int32 A1 = First + K * Sides + (S + 1) % Sides, B1 = First + ((K + 1) % Round) * Sides + (S + 1) % Sides;
					const FVector Out = N[A] + N[B] + N[A1] + N[B1];
					TriWorld(A, B, B1, Out);
					TriWorld(A, B1, A1, Out);
				}
			}
		}
		/** A cylinder lying along the frame's X (a drum, a spar), its middle Up over the frame's place, Sides round. */
		void Log(const FWreckFrame& F, double Long, double R, double Up, int32 Sides, const FColor& Paint, const FColor& Ends)
		{
			const int32 First = V.Num();
			for (int32 S = 0; S < Sides; ++S)
			{
				const double B = 2.0 * UE_DOUBLE_PI * S / Sides;
				const FVector Out = F.Y * FMath::Cos(B) + F.Z * FMath::Sin(B);
				Point(F.At(-0.5 * Long, 0.0, Up) + Out * R, Out, Paint);
				Point(F.At(0.5 * Long, 0.0, Up) + Out * R, Out, Paint);
			}
			for (int32 S = 0; S < Sides; ++S)
			{
				const int32 A = First + 2 * S, B = First + 2 * ((S + 1) % Sides);
				const double Bm = 2.0 * UE_DOUBLE_PI * (S + 0.5) / Sides;
				const FVector Out = F.Y * FMath::Cos(Bm) + F.Z * FMath::Sin(Bm);
				Tri(A, B, B + 1, Out);
				Tri(A, B + 1, A + 1, Out);
			}
			for (const double End : { -1.0, 1.0 })
			{
				TArray<FVector> Cap;
				for (int32 S = 0; S < Sides; ++S)
				{
					const double B = 2.0 * UE_DOUBLE_PI * S / Sides;
					Cap.Add(F.At(0.5 * Long * End, 0.0, Up) + (F.Y * FMath::Cos(B) + F.Z * FMath::Sin(B)) * R);
				}
				Face(&Cap[0], Cap.Num(), F.X * End, Ends);
			}
		}
	};

	/** One paint's channel: 0 to 255 from 0 to 1. */
	static uint8 Channel(double X)
	{
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(X * 255.0), 0, 255));
	}
	/** A paint toward another by a share. */
	static FColor Toward(const FColor& A, const FColor& B, double Share)
	{
		const double S = FMath::Clamp(Share, 0.0, 1.0);
		return FColor(static_cast<uint8>(FMath::RoundToInt32(A.R + (B.R - A.R) * S)), static_cast<uint8>(FMath::RoundToInt32(A.G + (B.G - A.G) * S)),
			static_cast<uint8>(FMath::RoundToInt32(A.B + (B.B - A.B) * S)), A.A);
	}
	/** A paint a little lighter or darker (Times about 1), by a man's or a piece's own hash. */
	static FColor Shaded(const FColor& A, double Times)
	{
		return FColor(Channel(A.R / 255.0 * Times), Channel(A.G / 255.0 * Times), Channel(A.B / 255.0 * Times), A.A);
	}

	/**
	 * A sailor's paint, until the sailor files land (the soldier modeller's, 22 November): his skin, his hair or his
	 * cap, his shirt and trousers, his life jacket and whether he has one, by his navy (the kit's rows) and his own hash.
	 * The navies' working dress at sea in the war: the US Navy's chambray shirt, dungarees and white hat and its kapok
	 * jacket; the Japanese navy's white or green working dress, black hair, kapok jacket; the Royal Navy's blue
	 * overalls and inflatable belt; the German and Italian navies' blue-grey; a merchant seaman as he stood.
	 */
	struct FSailorPaint
	{
		FColor Skin, Top, Shirt, Legs, Jacket;
		bool bJacket = true;
	};
	static FSailorPaint SailorPaint(const FString& Navy, uint32 Look, bool bWarm)
	{
		auto Bit = [Look](int32 Shift) { return static_cast<double>((Look >> Shift) & 255u) / 255.0; };
		FSailorPaint P;
		static const FColor Hairs[4] = { FColor(38, 30, 24), FColor(72, 52, 36), FColor(28, 24, 22), FColor(140, 110, 70) };
		const FColor Hair = Navy == TEXT("jp") ? FColor(24, 22, 22) : Hairs[(Look >> 3) & 3u];
		P.Skin = Shaded(Navy == TEXT("jp") ? FColor(206, 168, 128) : FColor(204, 158, 126), 0.86 + 0.18 * Bit(8));
		const double Hat = Bit(16), Coat = Bit(24), Vest = Bit(4);
		if (Navy == TEXT("jp"))
		{
			// (his whites only in summer in warm water, SeaSailors::SummerWhites)
			const bool bWhites = bWarm && Coat < 0.55;
			P.Shirt = bWhites ? FColor(196, 192, 178) : FColor(118, 116, 84);
			P.Legs = bWhites ? FColor(186, 182, 168) : FColor(106, 104, 76);
			P.Top = Hat < 0.25 ? (bWarm ? FColor(214, 212, 204) : FColor(118, 116, 84)) : Hat < 0.45 ? FColor(30, 32, 44) : Hair;
			P.Jacket = FColor(148, 134, 96);
			P.bJacket = Vest < 0.5;
		}
		else if (Navy == TEXT("gb"))
		{
			P.Shirt = Coat < 0.7 ? FColor(44, 54, 74) : FColor(170, 176, 182);
			P.Legs = FColor(40, 46, 62);
			P.Top = Hat < 0.15 ? FColor(30, 32, 40) : Hair;
			P.Jacket = FColor(96, 100, 98);
			P.bJacket = Vest < 0.7;
		}
		else if (Navy == TEXT("de") || Navy == TEXT("it") || Navy == TEXT("su"))
		{
			P.Shirt = Coat < 0.6 ? FColor(46, 52, 66) : FColor(150, 152, 150);
			P.Legs = FColor(42, 46, 58);
			P.Top = Hat < 0.2 ? FColor(28, 30, 40) : Hair;
			P.Jacket = Navy == TEXT("de") ? FColor(124, 122, 108) : FColor(150, 136, 98);
			P.bJacket = Vest < 0.6;
		}
		else
		{
			// (the US Navy, and a merchant seaman much as one)
			P.Shirt = Coat < 0.08 ? FColor(166, 150, 108) : Coat < 0.85 || !bWarm ? FColor(118, 142, 170) : FColor(206, 206, 200);
			P.Legs = FColor(48, 58, 84);
			P.Top = Hat < 0.45 ? FColor(226, 226, 220) : Hair;
			P.Jacket = Vest < 0.55 ? FColor(164, 148, 104) : FColor(96, 106, 116);
			P.bJacket = Vest < 0.7;
		}
		return P;
	}
	/** A man's paint soaked in fuel oil, by how much of him is black with it. */
	static FSailorPaint Oiled(const FSailorPaint& P, double Share)
	{
		static const FColor Oil(20, 18, 16);
		FSailorPaint Out = P;
		Out.Skin = Toward(P.Skin, Oil, Share * 0.85);
		Out.Top = Toward(P.Top, Oil, Share * 0.75);
		Out.Shirt = Toward(P.Shirt, Oil, Share * 0.9);
		Out.Legs = Toward(P.Legs, Oil, Share * 0.9);
		Out.Jacket = Toward(P.Jacket, Oil, Share * 0.6);
		return Out;
	}
	/** A ship's rafts and boats by her navy: wartime grey; the Japanese navy's darker; a merchant's buff. */
	static FColor CraftPaint(const FString& Navy, bool bMerchant)
	{
		if (bMerchant)
		{
			return FColor(176, 160, 120);
		}
		return Navy == TEXT("jp") ? FColor(96, 100, 102) : Navy == TEXT("gb") ? FColor(140, 146, 150) : FColor(118, 124, 128);
	}

	/** How a thing afloat rides the swell, by its own key: X metres up, Y its bow up and Z its right side down (radians);
	 * the smaller it is, the livelier (a life ring tosses, a boat lifts and settles). */
	static FVector FloatRide(int32 Key, double Now, double Long)
	{
		const double Lively = FMath::Clamp(2.5 / FMath::Max(Long, 0.5), 0.35, 2.5);
		const double P1 = Hash01(Key * 3.7) * 2.0 * UE_DOUBLE_PI, P2 = Hash01(Key * 5.3) * 2.0 * UE_DOUBLE_PI;
		const double W = 2.0 * UE_DOUBLE_PI / (3.2 + 1.6 * Hash01(Key * 7.1));
		return FVector(0.05 * FMath::Sin(W * Now + P1), 0.035 * Lively * FMath::Sin(W * Now + P1 + 1.2), 0.05 * Lively * FMath::Sin(W * 0.83 * Now + P2));
	}

	/** A float's ride with the show's own height over the sea added (a captor's whaler going down from his davits or up to
	 * them, SeaShow's Whalers; every other float lies on the sea, its height 0). */
	static FVector FloatRideOf(const SeaShow::FloatOut& F, double Now)
	{
		return FloatRide(F.Key, Now, F.Long) + FVector(FMath::Max(0.0, F.At.Z), 0.0, 0.0);
	}

	/** A whaleboat's or lifeboat's half breadth at its gunwale and its gunwale's height over the sea, U from -1 (her
	 * stern) to 1 (her bow): double-ended, her sheer rising toward both ends (the show keeps her lines, SeaShow::BoatHalf,
	 * for the men who hold on to her grab line). */
	static double BoatHalf(double U, double Beam)
	{
		return SeaShow::BoatHalf(U, Beam);
	}
	static double BoatSheer(double U)
	{
		return SeaShow::BoatSheer(U);
	}

	/** A boat's tiller: how far forward of her sternpost it reaches, how far to starboard and how high over her waterline
	 * its end lies (metres), by her coxswain's right hand; her coxswain's place on her boards facing forward, his hips this
	 * far forward of her stern. A man sitting knees up (the sailor files' sit_raft) reaches 0.27 m behind his hips and 0.59
	 * m ahead of them, 0.32 m to either side; her stroke oarsmen's boots come 0.73 m aft of their thwart. So he has her
	 * stern sheets to himself, his back clear of her narrowing stern and his boots of her oarsmen's. */
	static constexpr double TillerLong = 0.7, TillerAcross = 0.33, TillerLow = 0.45, CoxAt = 0.65;

	/** A pulling boat: her sides from under the water to the gunwale, the inside of them, the gunwale's capping, her
	 * bottom boards and four thwarts; bLines, her grab lines too (near enough for a rope to show), the loops her holders
	 * hold pulled out to their hands (Held: starboard's loops in its low 32 bits, port's in its high, SeaShow::GrabHeld). */
	static void DrawBoat(FWreckMesh& M, const FWreckFrame& Fr, double Long, double Beam, const FColor& Hull, const FColor& Inside, const FColor& Wood, bool bLines, uint64 Held = 0)
	{
		const int32 Steps = SeaShow::BoatStations;
		for (const double Hand : { -1.0, 1.0 })
		{
			for (int32 K = 0; K < Steps; ++K)
			{
				const double U0 = -1.0 + 2.0 * K / Steps, U1 = -1.0 + 2.0 * (K + 1) / Steps;
				const double X0 = U0 * 0.5 * Long, X1 = U1 * 0.5 * Long;
				const double H0 = BoatHalf(U0, Beam), H1 = BoatHalf(U1, Beam);
				const double I0 = FMath::Max(H0 - 0.05, 0.0), I1 = FMath::Max(H1 - 0.05, 0.0);
				const double S0 = BoatSheer(U0), S1 = BoatSheer(U1);
				M.Quad(Fr.At(X0, Hand * H0 * 0.84, -0.12), Fr.At(X1, Hand * H1 * 0.84, -0.12), Fr.At(X1, Hand * H1, S1), Fr.At(X0, Hand * H0, S0), Fr.Way(0.6 * (U0 + U1), Hand, 0.0), Hull);
				M.Quad(Fr.At(X0, Hand * I0, S0 - 0.02), Fr.At(X1, Hand * I1, S1 - 0.02), Fr.At(X1, Hand * I1 * 0.7, 0.12), Fr.At(X0, Hand * I0 * 0.7, 0.12), Fr.Way(0.0, -Hand, 0.3), Inside);
				M.Quad(Fr.At(X0, Hand * H0, S0), Fr.At(X1, Hand * H1, S1), Fr.At(X1, Hand * I1, S1 - 0.02), Fr.At(X0, Hand * I0, S0 - 0.02), Fr.Z, Wood);
			}
		}
		TArray<FVector> Floor;
		for (int32 K = 0; K <= Steps; ++K)
		{
			const double U = -1.0 + 2.0 * K / Steps;
			Floor.Add(Fr.At(U * 0.5 * Long, -FMath::Max(BoatHalf(U, Beam) - 0.05, 0.0) * 0.7, 0.12));
		}
		for (int32 K = Steps - 1; K >= 1; --K)
		{
			const double U = -1.0 + 2.0 * K / Steps;
			Floor.Add(Fr.At(U * 0.5 * Long, FMath::Max(BoatHalf(U, Beam) - 0.05, 0.0) * 0.7, 0.12));
		}
		M.Face(Floor.GetData(), Floor.Num(), Fr.Z, Shaded(Wood, 0.75));
		// (her five thwarts under her oarsmen: the show's rows 2 to 6 of twelve, two abreast, from 0.38 of her length aft
		// of her middle to as far forward; their tops where the sailors sit, SeaSailors::ThwartTop)
		for (int32 Row = 2; Row <= 6; ++Row)
		{
			const double U = 2.0 * (-0.38 + 0.76 * Row / 11.0);
			M.Box(Fr, U * 0.5 * Long, 0.0, SeaSailors::ThwartTop - 0.025, 0.24, 2.0 * FMath::Max(BoatHalf(U, Beam) - 0.07, 0.1), 0.05, Wood, Shaded(Wood, 1.08));
		}
		// (a double-ended boat told bow from stern, the art director, 22 November 12:17Z: at her stern a rudder hung on her
		// sternpost down past her keel, its tiller forward over her stern sheets by her coxswain's right hip; at her bow her
		// stem standing a hand over her gunwale. Her oarsmen face her stern.)
		const double Aft = -0.5 * Long, Fore = 0.5 * Long, AftTop = BoatSheer(-1.0) + 0.06, ForeTop = BoatSheer(1.0) + 0.16;
		M.Box(Fr, Aft - 0.17, 0.0, 0.5 * (AftTop - 0.5), 0.3, 0.05, AftTop + 0.5, Hull, Shaded(Hull, 1.08));
		M.Box(Fr, Aft - 0.01, 0.0, 0.5 * (AftTop - 0.12), 0.06, 0.07, AftTop + 0.12, Wood, Wood);
		M.Rod(Fr.At(Aft - 0.04, 0.0, AftTop - 0.03), Fr.At(Aft + TillerLong, TillerAcross, TillerLow), 0.025, 4, Wood);
		M.Box(Fr, Fore + 0.01, 0.0, 0.5 * (ForeTop - 0.12), 0.07, 0.07, ForeTop + 0.12, Wood, Wood);
		// (her grab lines, the art director, 22 November 13:47Z: a dark rope in festoons along each side, made fast under her
		// gunwale about every metre and dipping to just over the water, where the men in the water hold on: SeaShow::GrabAt)
		if (!bLines)
		{
			return;
		}
		static const FColor GrabRope(52, 46, 38);
		const int32 Loops = SeaShow::GrabLoops(Long), Bits = 6;
		const double From = -SeaShow::GrabFrom * Long, Span = 2.0 * SeaShow::GrabFrom * Long / Loops;
		for (const double Hand : { -1.0, 1.0 })
		{
			// (the loops her holders have hold of on this side, pulled out to their hands: a held loop drawn at its pull's
			// points too, either side of its dip)
			const uint32_t Side = static_cast<uint32_t>(Hand > 0.0 ? Held & 0xffffffffull : Held >> 32);
			SeaShow::Vec Was = SeaShow::GrabAt(Long, Beam, Hand, From, Side);
			for (int32 Loop = 0; Loop < Loops; ++Loop)
			{
				const bool bHeld = Loop < 32 && ((Side >> Loop) & 1u) != 0u;
				const double Dip = SeaShow::GrabDip(Long, Loop);
				int32 Pull = 0;
				for (int32 K = 1; K <= Bits; ++K)
				{
					const double Next = From + Span * (Loop + static_cast<double>(K) / Bits);
					while (bHeld && Pull < static_cast<int32>(UE_ARRAY_COUNT(SeaShow::GrabPullAt)) && Dip + SeaShow::GrabPullAt[Pull] * SeaShow::GrabPullHalf < Next - 1e-9)
					{
						const double At = Dip + SeaShow::GrabPullAt[Pull++] * SeaShow::GrabPullHalf;
						const SeaShow::Vec On = SeaShow::GrabAt(Long, Beam, Hand, At, Side);
						M.Rod(Fr.At(Was.X, Was.Y, Was.Z), Fr.At(On.X, On.Y, On.Z), SeaShow::GrabThick, 4, GrabRope);
						Was = On;
					}
					const SeaShow::Vec On = SeaShow::GrabAt(Long, Beam, Hand, Next, Side);
					M.Rod(Fr.At(Was.X, Was.Y, Was.Z), Fr.At(On.X, On.Y, On.Z), SeaShow::GrabThick, 4, GrabRope);
					Was = On;
				}
			}
		}
	}

	/** One thing afloat where a ship went down (SeaShow's FloatOut): a Carley float, a pulling boat, or her wreckage:
	 * planks, spars, crates, drums, hatch covers, life rings. bFine: near enough for a rope to show (a quarter of a pixel
	 * thick or more). */
	static void DrawFloat(FWreckMesh& M, const SeaShow::FloatOut& F, const FString& Navy, bool bMerchant, double Now, bool bFine, uint64 Held = 0)
	{
		const FVector Ride = FloatRideOf(F, Now);
		const FWreckFrame Fr = WreckFrame(FVector(F.At.X, F.At.Y, Ride.X), F.Heading, Ride.Y, Ride.Z);
		const double H1 = Hash01(F.Key * 1.37), H2 = Hash01(F.Key * 2.71);
		const FColor Craft = CraftPaint(Navy, bMerchant);
		static const FColor Wood(128, 104, 74), Pale(156, 132, 96), Burnt(42, 38, 34), Rope(92, 80, 60);
		const FColor Timber = H2 < 0.15 ? Burnt : Shaded(H1 < 0.5 ? Wood : Pale, 0.85 + 0.3 * H2);
		const std::string& Kind = F.Kind;
		if (Kind == "raft")
		{
			// (its body a ring of tubing, cork in canvas, with a lifeline in loops round its outside; its grating hangs
			// under the water, where the men inside stand or sit)
			M.Tube(Fr, F.Long * 0.5 - 0.2, F.Beam * 0.5 - 0.2, 0.2, 0.06, 20, 6, Craft, Rope);
		}
		else if (Kind == "boat")
		{
			DrawBoat(M, Fr, F.Long, F.Beam, Craft, Shaded(Craft, 1.15), Wood, bFine, Held);
		}
		else if (Kind == "plank")
		{
			M.Box(Fr, 0.0, 0.0, 0.0, F.Long, F.Beam, 0.07, Timber, Timber);
		}
		else if (Kind == "spar")
		{
			M.Log(Fr, F.Long, F.Beam * 0.5, -0.03, 6, H1 < 0.4 ? Craft : Timber, Shaded(Timber, 0.8));
		}
		else if (Kind == "crate")
		{
			M.Box(Fr, 0.0, 0.0, 0.08, F.Long, F.Beam, 0.55, Timber, Shaded(Timber, 1.1));
		}
		else if (Kind == "drum")
		{
			const FColor Steel = H1 < 0.4 ? FColor(70, 74, 50, 128) : H1 < 0.75 ? FColor(34, 34, 34, 128) : FColor(116, 64, 38, 128);
			M.Log(Fr, F.Long, F.Beam * 0.5, 0.06, 8, Steel, Shaded(Steel, 0.8));
		}
		else if (Kind == "hatch")
		{
			M.Box(Fr, 0.0, 0.0, 0.0, F.Long, F.Beam, 0.1, H1 < 0.5 ? Craft : Timber, H1 < 0.5 ? Shaded(Craft, 1.05) : Timber);
		}
		else if (Kind == "ring")
		{
			M.Tube(Fr, 0.33, 0.33, 0.07, 0.02, 12, 5, FColor(222, 218, 206), Navy == TEXT("jp") ? FColor(200, 198, 190) : FColor(196, 72, 40), 4);
		}
	}

	/** A man of a struck boat's crew on her casing, or of her boarding party (SeaShow::Hands, but its whaler's crew): out
	 * of the water and the oil, moving with her each frame. */
	static bool OnHerCasing(SeaShow::Swim Doing)
	{
		using SeaShow::Swim;
		return Doing == Swim::Hatch || Doing == Swim::Stand || Doing == Swim::Walk || Doing == Swim::HandsUp || Doing == Swim::Run;
	}

	/**
	 * A sailor in the water, on a raft or in a boat, going over her side or up a rescuer's: a stand-in until the sailor
	 * files land, his paint his navy's. bDetail: a near man's head, life jacket or shoulders, body and limbs; else a far
	 * man's eight-faced stone in his colours, a few pixels. Lift: the float he is on riding the swell. Handle: the oar
	 * he pulls (its loom's inboard end), or none.
	 */
	static void DrawSailor(FWreckMesh& M, const SeaShow::ManOut& Man, const FSailorPaint& P, bool bDetail, double Now, double Lift, const FVector* Handle, double Pull)
	{
		using SeaShow::Swim;
		const double Ph = Hash01(Man.Key * 0.731) * 2.0 * UE_DOUBLE_PI;
		const FColor Body = P.bJacket ? P.Jacket : P.Shirt;
		const double Feet = Man.At.Z;
		const FWreckFrame Level = WreckFrame(FVector(Man.At.X, Man.At.Y, 0.0), Man.Heading);
		if (!bDetail)
		{
			switch (Man.Doing)
			{
			case Swim::Jump:
			case Swim::Climb:
			case Swim::Hatch:
			case Swim::Stand:
			case Swim::Walk:
			case Swim::HandsUp:
			case Swim::Run:
				// (standing: going over, up a net, or on a struck boat's casing)
				M.Egg(Level, FVector(0.0, 0.0, Feet + 0.9), FVector(0.14, 0.2, 0.9), 2, 4, P.Top, P.Shirt, P.Legs);
				break;
			case Swim::Raft:
				M.Egg(Level, FVector(0.0, 0.0, Lift + 0.62), FVector(0.14, 0.2, 0.42), 2, 4, P.Top, Body, P.Legs);
				break;
			case Swim::Boat:
				M.Egg(Level, FVector(0.0, 0.0, Lift + 0.8), FVector(0.14, 0.2, 0.42), 2, 4, P.Top, Body, P.Legs);
				break;
			case Swim::Dead:
				M.Egg(Level, FVector(0.0, 0.0, 0.02), FVector(0.32, 0.2, 0.08), 2, 4, Body, Body, Body);
				break;
			default:
				M.Egg(Level, FVector(0.0, 0.0, 0.05), FVector(0.16, 0.22, 0.16), 2, 4, P.Top, Body, Body);
				break;
			}
			return;
		}
		switch (Man.Doing)
		{
		case Swim::Jump:
		case Swim::Climb:
		case Swim::Hatch:
		case Swim::HandsUp:
		{
			// (standing: going feet first over her side, his arms up, tipping a little as he falls; or up a rescuer's
			// net hand over hand, facing her side, or up out of a struck boat's hatch; or on her casing, his hands up)
			const bool bClimb = Man.Doing == Swim::Climb || Man.Doing == Swim::Hatch;
			const double Tip = Man.Doing == Swim::Jump ? FMath::Clamp(0.12 * Man.Since, 0.0, 0.35) * (Hash01(Man.Key * 1.9) < 0.5 ? 1.0 : -1.0) : 0.0;
			const FWreckFrame F = WreckFrame(FVector(Man.At.X, Man.At.Y, Feet), Man.Heading, Tip, 0.0);
			const double Step = bClimb ? 0.22 * FMath::Sin(Now * 2.4 + Ph) : 0.0;
			for (const double Hand : { -1.0, 1.0 })
			{
				M.Rod(F.At(0.0, Hand * 0.1, 0.92), F.At(0.04, Hand * 0.12, 0.04 + FMath::Max(0.0, Hand * Step)), 0.07, 5, P.Legs);
				const double Reach = bClimb ? 1.95 - Hand * Step : 2.05;
				M.Rod(F.At(0.0, Hand * 0.21, 1.45), F.At(bClimb ? 0.18 : 0.02, Hand * (bClimb ? 0.24 : 0.3), Reach), 0.05, 5, P.Shirt);
			}
			M.Egg(F, FVector(0.0, 0.0, 1.2), FVector(0.13, 0.21, 0.3), 3, 8, Body, Body, P.Legs);
			M.Egg(F, FVector(0.02, 0.0, 1.66), FVector(0.1, 0.085, 0.115), 5, 10, P.Top, P.Skin, P.Skin);
			break;
		}
		case Swim::Stand:
		case Swim::Walk:
		case Swim::Run:
		{
			// (on a struck boat's casing, his arms at his sides; walking or running, his legs and arms swinging at the sailor
			// files' own strides: 1.133 s walking, 0.8 s running)
			const FWreckFrame F = WreckFrame(FVector(Man.At.X, Man.At.Y, Feet), Man.Heading);
			const double Stride = Man.Doing == Swim::Run ? 0.8 : 1.133;
			const double Swing = Man.Doing == Swim::Stand ? 0.0 : (Man.Doing == Swim::Run ? 0.35 : 0.22) * FMath::Sin(2.0 * UE_DOUBLE_PI * Man.Since / Stride);
			for (const double Hand : { -1.0, 1.0 })
			{
				M.Rod(F.At(0.0, Hand * 0.1, 0.92), F.At(Hand * Swing, Hand * 0.11, 0.04), 0.07, 5, P.Legs);
				M.Rod(F.At(0.0, Hand * 0.21, 1.45), F.At(-0.8 * Hand * Swing, Hand * 0.26, 0.82), 0.05, 5, P.Shirt);
			}
			M.Egg(F, FVector(0.0, 0.0, 1.2), FVector(0.13, 0.21, 0.3), 3, 8, Body, Body, P.Legs);
			M.Egg(F, FVector(0.02, 0.0, 1.66), FVector(0.1, 0.085, 0.115), 5, 10, P.Top, P.Skin, P.Skin);
			break;
		}
		case Swim::Raft:
		case Swim::Boat:
		{
			// (sitting: on a raft's rim, his legs in the water inside it; on a thwart, facing aft, pulling an oar or not)
			const bool bBoat = Man.Doing == Swim::Boat;
			const double Seat = Lift + (bBoat ? 0.47 : 0.27);
			const double Lean = Handle ? 0.38 * FMath::Sin(Pull) : (Hash01(Man.Key * 2.3) - 0.5) * 0.3;
			const FWreckFrame Hips = WreckFrame(FVector(Man.At.X, Man.At.Y, Seat), Man.Heading);
			const FWreckFrame F = WreckFrame(FVector(Man.At.X, Man.At.Y, Seat), Man.Heading, -Lean, 0.0);
			for (const double Hand : { -1.0, 1.0 })
			{
				M.Rod(Hips.At(0.0, Hand * 0.1, 0.03), Hips.At(0.42, Hand * 0.11, -0.05), 0.075, 5, P.Legs);
				M.Rod(Hips.At(0.42, Hand * 0.11, -0.05), Hips.At(0.5, Hand * 0.12, -0.45), 0.06, 5, P.Legs);
				const FVector Hands = Handle ? *Handle + Hips.Way(0.0, Hand * 0.1, 0.0) : Hips.At(0.4, Hand * 0.15, 0.05);
				M.Rod(F.At(0.0, Hand * 0.21, 0.52), Hands, 0.048, 5, P.Shirt);
			}
			M.Egg(F, FVector(0.0, 0.0, 0.3), FVector(0.13, 0.21, 0.3), 3, 8, Body, Body, P.Legs);
			M.Egg(F, FVector(0.02, 0.0, 0.76), FVector(0.1, 0.085, 0.115), 5, 10, P.Top, P.Skin, P.Skin);
			break;
		}
		case Swim::Dead:
		{
			// (face down, his back and the top of his head out of the water, a jacket holding him higher)
			const FWreckFrame F = WreckFrame(FVector(Man.At.X, Man.At.Y, P.bJacket ? 0.04 : -0.01), Man.Heading);
			M.Egg(F, FVector(0.0, 0.0, 0.0), FVector(0.3, 0.2, 0.09), 3, 8, Body, Body, Body);
			M.Egg(F, FVector(0.36, 0.0, -0.03), FVector(0.115, 0.09, 0.1), 5, 10, P.Top, P.Top, P.Skin);
			for (const double Hand : { -1.0, 1.0 })
			{
				M.Rod(F.At(0.2, Hand * 0.17, 0.0), F.At(0.42, Hand * 0.55, -0.04), 0.045, 5, P.Shirt);
				M.Rod(F.At(-0.25, Hand * 0.09, -0.03), F.At(-0.95, Hand * 0.14, -0.1), 0.065, 5, P.Legs);
			}
			break;
		}
		default:
		{
			// (in the water to his chin, or his shoulders where a jacket holds him up: treading water, an arm up to a
			// ship coming; swimming the breast stroke, his hands sweeping at the surface; holding on to what floats)
			const bool bSwim = Man.Doing == Swim::Swim;
			const double Bob = 0.03 * FMath::Sin(Now * 2.6 + Ph);
			const double Up = (P.bJacket ? 0.04 : -0.02) + Bob;
			M.Egg(Level, FVector(0.0, 0.0, Up), FVector(bSwim ? 0.25 : 0.13, 0.22, 0.1), 3, 8, Body, Body, P.Shirt);
			M.Egg(Level, FVector(bSwim ? 0.22 : 0.03, 0.0, Up + (bSwim ? 0.1 : 0.15)), FVector(0.1, 0.085, 0.115), 5, 10, P.Top, P.Skin, P.Skin);
			if (bSwim)
			{
				const double Reach = 0.5 + 0.5 * FMath::Sin(Now * 3.6 + Ph);
				for (const double Hand : { -1.0, 1.0 })
				{
					M.Rod(Level.At(0.05, Hand * 0.18, Up - 0.02), Level.At(0.25 + 0.3 * Reach, Hand * (0.12 + 0.28 * (1.0 - Reach)), 0.01), 0.045, 5, P.Shirt);
				}
			}
			else if (Man.Doing == Swim::Cling)
			{
				for (const double Hand : { -1.0, 1.0 })
				{
					if (Hand > 0.0 || Hash01(Man.Key * 3.1) < 0.5)
					{
						M.Rod(Level.At(0.05, Hand * 0.18, Up + 0.04), Level.At(0.42, Hand * 0.1, 0.22), 0.045, 5, P.Shirt);
					}
				}
			}
			else if (Man.bWave)
			{
				M.Rod(Level.At(0.0, 0.2, Up + 0.06), Level.At(0.1, 0.32 + 0.12 * FMath::Sin(Now * 5.0 + Ph), Up + 0.72), 0.045, 5, P.Shirt);
			}
			break;
		}
		}
	}

	/**
	 * The dress of a navy's sailors drawn with sailor file File (Battle/SeaSailors), variant 0 or 1, as its place in
	 * S.Dresses: a navy's own file in its own colours, its variant 1 the file's other dress (the American officer's and
	 * chief's khaki, the Japanese grey-green); another navy's men, until their own file is made, in the American's rig in
	 * their own working dress, as the stand-ins wear it (SailorPaint): the Royal Navy's blue overalls, dark cap and grey
	 * belt; the German, Italian and Soviet navies' blue-grey; a Japanese sailor's white or green. Whites only where the
	 * battle is known to fall in summer in warm water (SeaSailors::SummerWhites; the art director, 22 November 13:47Z).
	 */
	static int32 DressFor(FSeaBattleState& S, const std::string& Nation, int32 File, int32 Variant)
	{
		const SeaShow::Field& Board = S.Rec.Board;
		const bool bWarm = SeaSailors::SummerWhites(Board.bWhen, Board.Month, Board.Lat, Board.Lon);
		const std::string Army = SeaSailors::ArmyOf(Nation);
		const std::string Key = Army + "/" + std::to_string(File) + "/" + std::to_string(Variant);
		const auto Found = S.DressAt.find(Key);
		if (Found != S.DressAt.end())
		{
			return Found->second;
		}
		SeaSailors::Dress D;
		const bool bSecond = Variant == 1;
		if (S.Crew.Army(File) == Army)
		{
			S.Crew.DressOf(File, Variant, bWarm, D);
		}
		else if (Army == "JAP")
		{
			const bool bGreen = bSecond || !bWarm;
			D.Put(SeaSailors::Shirt, bGreen ? 0x767454u : 0xC4C0B2u);
			D.Put(SeaSailors::Trousers, bGreen ? 0x6A684Cu : 0xBAB6A8u);
			D.Put(SeaSailors::Hat, bGreen ? 0x1E202Cu : 0xD6D4CCu);
			D.Put(SeaSailors::Kapok, 0x948660u);
		}
		else if (Army == "GBR")
		{
			D.Put(SeaSailors::Shirt, bSecond ? 0xAAB0B6u : 0x2C364Au);
			D.Put(SeaSailors::Trousers, 0x282E3Eu);
			D.Put(SeaSailors::Hat, 0x1E2028u);
			D.Put(SeaSailors::Kapok, 0x606462u);
		}
		else if (Army == "USA")
		{
			D.Put(SeaSailors::Shirt, bSecond ? 0xA6966Cu : 0x768EAAu);
			D.Put(SeaSailors::Trousers, 0x303A54u);
			D.Put(SeaSailors::Hat, 0xE2E2DCu);
			D.Put(SeaSailors::Kapok, 0xA49468u);
		}
		else
		{
			D.Put(SeaSailors::Shirt, bSecond ? 0x969896u : 0x2E3442u);
			D.Put(SeaSailors::Trousers, 0x2A2E3Au);
			D.Put(SeaSailors::Hat, 0x1C1E28u);
			D.Put(SeaSailors::Kapok, Army == "GER" ? 0x7C7A6Cu : 0x968862u);
		}
		S.Dresses.push_back(D);
		const int32 Index = static_cast<int32>(S.Dresses.size()) - 1;
		S.DressAt[Key] = Index;
		return Index;
	}

	/** A pose bank mesh's key: its file, dress, act, pose, level, whether drawn left for right and whether black with oil. */
	static int64 SailorKey(int32 File, int32 Dress, int32 What, int32 Pose, int32 Lod, bool bMirror, bool bOiled)
	{
		// (sixty-four acts and sixty-four poses of each at the most: the gun crew's and hands up made nineteen acts, past the
		// sixteen this packed until then; Tools/convoy_looks checks PosesOf)
		static_assert(SeaSailors::Acts <= 64, "SailorKey packs an act in six bits");
		return ((((((static_cast<int64>(File) * 64 + Dress) * 64 + What) * 64 + Pose) * 4 + Lod) * 2 + (bMirror ? 1 : 0)) * 2 + (bOiled ? 1 : 0));
	}

	/** A place in a man's own axes (metres: x forward, y to his right, z up) where his transform puts it, on the board
	 * (the wreck mesh's metres: x east, y north). */
	static FVector GunBoard(const FTransform& Man, double Units, double X, double Y, double Z)
	{
		const FVector Placed = Man.TransformPosition(FVector(X, Y, Z) * Units);
		return FVector(Placed.Y, Placed.X, Placed.Z) / Units;
	}

	/**
	 * A gun's handwheel where a layer's or a trainer's clip has his hands (Battle/DeckCrewCore): its rim about its hub,
	 * square to its axle (his x), turned as his clip turns it (a spoke across it and a handle on its rim show the turn),
	 * and its shaft from the hub into the gun (ShaftAhead along his x, ShaftIn toward the gun: to his right at its left
	 * side, bGunOnRight); in his own axes, as his transform lays him.
	 */
	static void DrawGunWheel(FWreckMesh& Mesh, const FTransform& Man, double Units, const SeaSailors::Wheel& W, float Turned, double ShaftAhead, double ShaftIn, bool bGunOnRight)
	{
		static const FColor Iron(58, 60, 62, 128);
		const int32 Rim = 10;
		const double Hx = W.Hub[0], Hy = W.Hub[1], Hz = W.Hub[2], R = W.Radius;
		auto RimAt = [&](double Angle) { return GunBoard(Man, Units, Hx, Hy + R * FMath::Cos(Angle), Hz + R * FMath::Sin(Angle)); };
		for (int32 Seg = 0; Seg < Rim; ++Seg)
		{
			Mesh.Rod(RimAt(2.0 * UE_DOUBLE_PI * Seg / Rim), RimAt(2.0 * UE_DOUBLE_PI * (Seg + 1) / Rim), 0.011, 4, Iron);
		}
		const double Spoke = FMath::DegreesToRadians(static_cast<double>(Turned));
		Mesh.Rod(RimAt(Spoke), RimAt(Spoke + UE_DOUBLE_PI), 0.008, 3, Iron);
		const FVector Grip = RimAt(Spoke);
		Mesh.Rod(Grip, GunBoard(Man, Units, Hx - 0.07, Hy + R * FMath::Cos(Spoke), Hz + R * FMath::Sin(Spoke)), 0.012, 4, Iron);
		Mesh.Rod(GunBoard(Man, Units, Hx, Hy, Hz), GunBoard(Man, Units, Hx + ShaftAhead, Hy + (bGunOnRight ? ShaftIn : -ShaftIn), Hz), 0.018, 4, Iron);
	}

	/** A gun's sight where a layer's or a trainer's clip has his eye: its eyepiece just ahead of his eye along his x, its
	 * tube on forward, and a bracket down to the wheel's shaft (W's hub, the shaft's end as DrawGunWheel draws it). */
	static void DrawGunSight(FWreckMesh& Mesh, const FTransform& Man, double Units, const SeaSailors::Wheel& W, double ShaftAhead, double ShaftIn, bool bGunOnRight)
	{
		static const FColor Black(30, 31, 33, 128);
		const FVector Eyepiece = GunBoard(Man, Units, W.Eye[0] + 0.03, W.Eye[1], W.Eye[2]);
		const FVector Objective = GunBoard(Man, Units, W.Eye[0] + 0.33, W.Eye[1], W.Eye[2]);
		Mesh.Rod(Eyepiece, Objective, 0.022, 6, Black);
		Mesh.Rod(Objective, GunBoard(Man, Units, W.Hub[0] + ShaftAhead, W.Hub[1] + (bGunOnRight ? ShaftIn : -ShaftIn), W.Hub[2]), 0.012, 4, Black);
	}

	/** A round in a man's hands (his own axes: its base and its nose's way, metres), its brass case and its shell; a round
	 * not shown drawn as nothing at its base (the mesh keeps its shape while he works). */
	static void DrawGunRound(FWreckMesh& Mesh, const FTransform& Man, double Units, const SeaSailors::RoundAt& R, double Long, double Radius)
	{
		static const FColor Brass(176, 136, 62, 128), Shell(92, 96, 88, 128);
		const double Thick = R.bShown ? Radius : 0.001, Length = R.bShown ? Long : 0.002;
		const FVector Base = GunBoard(Man, Units, R.Base.X, R.Base.Y, R.Base.Z);
		const FVector Joint = GunBoard(Man, Units, R.Base.X + R.Way.X * Length * 0.62, R.Base.Y + R.Way.Y * Length * 0.62, R.Base.Z + R.Way.Z * Length * 0.62);
		const FVector Nose = GunBoard(Man, Units, R.Base.X + R.Way.X * Length, R.Base.Y + R.Way.Y * Length, R.Base.Z + R.Way.Z * Length);
		Mesh.Rod(Base, Joint, Thick, 6, Brass);
		Mesh.Rod(Joint, Nose, Thick * 0.85, 6, Shell);
	}

	/** A ready-use rack at a loader's left (his own axes, as his transform lays him): two uprights from the deck to a
	 * cradle under where the round waits on it (Battle/DeckCrewCore's RackRound, its base and nose), its round's length
	 * apart. */
	static void DrawGunRack(FWreckMesh& Mesh, const FTransform& Man, double Units, const DeckCrew::Blocks& Kit)
	{
		static const FColor Steel(70, 74, 70, 128);
		SeaSailors::RoundAt Waits;
		if (!DeckCrew::RackRound(Kit, 0.f, Waits))
		{
			return;
		}
		const double Cradle = Kit.Loading.Radius + 0.01;
		for (const double Share : { 0.15, 0.85 })
		{
			const double X = Waits.Base.X + Waits.Way.X * Kit.Loading.Long * Share, Y = Waits.Base.Y + Waits.Way.Y * Kit.Loading.Long * Share;
			const double Z = Waits.Base.Z + Waits.Way.Z * Kit.Loading.Long * Share - Cradle;
			Mesh.Rod(GunBoard(Man, Units, X, Y, 0.0), GunBoard(Man, Units, X, Y, Z), 0.015, 4, Steel);
			Mesh.Rod(GunBoard(Man, Units, X, Y - 0.06, Z), GunBoard(Man, Units, X, Y + 0.06, Z), 0.012, 4, Steel);
		}
	}

	/** A hatch open on a deck (Her: its middle on the deck, her turn, Unreal's world): its dark opening and its lid raised
	 * on its far side from the gun (Away: the way from the gun to the hatch, her own axes). */
	static void DrawGunHatch(FWreckMesh& Mesh, const FTransform& Her, double Units, const FVector2D& Away)
	{
		static const FColor Dark(16, 17, 19, 128), Lid(84, 88, 92, 128);
		const FVector2D A = Away.GetSafeNormal(), B(-A.Y, A.X);
		const double Half = 0.33;
		auto On = [&](double OnAlong, double OnAcross, double OnUp) { return GunBoard(Her, Units, A.X * OnAlong + B.X * OnAcross, A.Y * OnAlong + B.Y * OnAcross, OnUp); };
		const FVector Upward = (On(0.0, 0.0, 1.0) - On(0.0, 0.0, 0.0)).GetSafeNormal();
		Mesh.Quad(On(-Half, -Half, 0.03), On(Half, -Half, 0.03), On(Half, Half, 0.03), On(-Half, Half, 0.03), Upward, Dark);
		// (its lid on its hinge at the far edge, stood up and a little over)
		const FVector Outward = (On(1.0, 0.0, 0.0) - On(0.0, 0.0, 0.0)).GetSafeNormal();
		Mesh.Quad(On(Half, -Half, 0.03), On(Half + 0.1, -Half, 0.68), On(Half + 0.1, Half, 0.68), On(Half, Half, 0.03), -Outward, Lid);
		Mesh.Quad(On(Half + 0.02, -Half, 0.03), On(Half + 0.12, -Half, 0.68), On(Half + 0.12, Half, 0.68), On(Half + 0.02, Half, 0.03), Outward, Lid);
	}

	/**
	 * A sailor of the pose bank (one file in one dress, one act's pose, one level of detail) as a mesh of its own on the
	 * wreckage's material, for instancing: his own axes (x forward, y to his right, z up: the same hand as Unreal's) as the
	 * mesh's, metres to Unreal's units; colours as sRGB bytes, as the ground scene's bank keeps them. bMirror: drawn left
	 * for right (an oarsman on her starboard side), each triangle turned the other way round so it faces out still. Null
	 * where the points, colours and triangles do not agree.
	 */
	static UStaticMesh* SailorMesh(UObject* Owner, const FName& Name, const std::vector<float>& At, const std::vector<float>& Facing, const std::vector<uint8_t>& Rgb,
		const std::vector<uint32_t>& Triangles, bool bMirror, UMaterialInterface* Ink, double U)
	{
		const int32 Count = static_cast<int32>(At.size() / 3);
		if (!Ink || Count <= 0 || Facing.size() != At.size() || Rgb.size() != At.size() || Triangles.empty())
		{
			return nullptr;
		}
		FMeshDescription D;
		FStaticMeshAttributes Attributes(D);
		Attributes.Register();
		auto Positions = Attributes.GetVertexPositions();
		auto Normals = Attributes.GetVertexInstanceNormals();
		auto Tangents = Attributes.GetVertexInstanceTangents();
		auto Signs = Attributes.GetVertexInstanceBinormalSigns();
		auto Colours = Attributes.GetVertexInstanceColors();
		auto Uvs = Attributes.GetVertexInstanceUVs();
		Uvs.SetNumChannels(1);
		const FPolygonGroupID Group = D.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Ink");
		D.ReserveNewVertices(Count);
		D.ReserveNewVertexInstances(Count);
		D.ReserveNewTriangles(static_cast<int32>(Triangles.size() / 3));
		const float Hand = bMirror ? -1.0f : 1.0f;
		TArray<FVertexInstanceID> Corners;
		Corners.SetNum(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const size_t K = static_cast<size_t>(I) * 3;
			const FVertexID Vertex = D.CreateVertex();
			Positions[Vertex] = FVector3f(At[K] * U, Hand * At[K + 1] * U, At[K + 2] * U);
			const FVertexInstanceID Corner = D.CreateVertexInstance(Vertex);
			const FVector3f Normal = FVector3f(Facing[K], Hand * Facing[K + 1], Facing[K + 2]).GetSafeNormal(1e-6f, FVector3f::UpVector);
			Normals[Corner] = Normal;
			// (no normal map: any tangent across the normal will do)
			const FVector3f Across = FMath::Abs(Normal.Z) < 0.9f ? FVector3f::UpVector : FVector3f::ForwardVector;
			Tangents[Corner] = FVector3f::CrossProduct(Across, Normal).GetSafeNormal(1e-6f, FVector3f::ForwardVector);
			Signs[Corner] = 1.0f;
			Colours[Corner] = FVector4f(FLinearColor(FColor(Rgb[K], Rgb[K + 1], Rgb[K + 2], 255)));
			Uvs.Set(Corner, 0, FVector2f::ZeroVector);
			Corners[I] = Corner;
		}
		for (size_t K = 0; K + 2 < Triangles.size(); K += 3)
		{
			const uint32_t A = Triangles[K], B = Triangles[bMirror ? K + 2 : K + 1], C = Triangles[bMirror ? K + 1 : K + 2];
			if (A >= static_cast<uint32_t>(Count) || B >= static_cast<uint32_t>(Count) || C >= static_cast<uint32_t>(Count))
			{
				return nullptr;
			}
			const FVertexInstanceID Three[3] = { Corners[static_cast<int32>(A)], Corners[static_cast<int32>(B)], Corners[static_cast<int32>(C)] };
			D.CreateTriangle(Group, TArrayView<const FVertexInstanceID>(Three, 3));
		}
		UStaticMesh* Made = NewObject<UStaticMesh>(Owner, Name);
		Made->GetStaticMaterials().Add(FStaticMaterial(Ink, TEXT("Ink")));
		TArray<const FMeshDescription*> Levels;
		Levels.Add(&D);
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Made->BuildFromMeshDescriptions(Levels, Params);
		return Made;
	}

	/** The haze at the horizon as the screen shows it, as pixel / 255 (not linearised): the band just above the sea in
	 * the Mac stills of 21 November (sea-r2b: the three battles at their hours of 8 in the morning to 3 in the
	 * afternoon, 221, 221, 223 in each). */
	static const FLinearColor SkyOnScreen(221.0f / 255.0f, 221.0f / 255.0f, 223.0f / 255.0f);

	/** The sky just over the horizon as a still shows it (SkyOnScreen), or "-TheaterSeaSky=red,green,blue" (0 to 255). */
	static FLinearColor SkyColour()
	{
		FLinearColor Sky = SkyOnScreen;
		FString Asked;
		TArray<FString> Bits;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaSky="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(","), false) == 3)
		{
			Sky.R = static_cast<float>(FMath::Clamp(FCString::Atod(*Bits[0]), 0.0, 255.0) / 255.0);
			Sky.G = static_cast<float>(FMath::Clamp(FCString::Atod(*Bits[1]), 0.0, 255.0) / 255.0);
			Sky.B = static_cast<float>(FMath::Clamp(FCString::Atod(*Bits[2]), 0.0, 255.0) / 255.0);
		}
		return Sky;
	}

	/** The battle grade's red and blue gains (BattleLook::Grade's own, 1.08 and 0.87), under which the Mac's first sea
	 * frames were taken and the tone curve fitted. */
	static const FVector2D GradeOfTheFit(1.08, 0.87);

	/** Whether the sea battle sets its own colour gains over the battle grade: unless the grade is off
	 * ("-TheaterNoBattleGrade") or given whole ("-TheaterBattleGrade=", which then holds as given). */
	static bool OwnGrade()
	{
		const TCHAR* Line = FCommandLine::Get();
		return !FParse::Param(Line, TEXT("TheaterNoBattleGrade")) && !FString(Line).Contains(TEXT("TheaterBattleGrade="));
	}

	/** The red and blue gains the battle's camera grades with. At sea the art director's neutral grade (22 November
	 * 04:20Z: red 1.0 and blue 0.97, as the air scene's over the sea; a hull's warmth from the low sun on her lit side
	 * only, not from the grade, which turned the ships' cool greys tan in Samar c365), "-TheaterSeaGrade=red,blue" to try
	 * others; else the grade as given ("-TheaterBattleGrade=", its fifth and sixth), or none. */
	static FVector2D GradeNow()
	{
		const TCHAR* Line = FCommandLine::Get();
		if (FParse::Param(Line, TEXT("TheaterNoBattleGrade")))
		{
			return FVector2D(1.0, 1.0);
		}
		FVector2D Gain(1.0, 0.97);
		FString Asked;
		TArray<FString> Bits;
		int32 Red = 0;
		if (FParse::Value(Line, TEXT("TheaterBattleGrade="), Asked, false))
		{
			Gain = GradeOfTheFit;
			Red = 4;
		}
		else if (!FParse::Value(Line, TEXT("TheaterSeaGrade="), Asked, false))
		{
			return Gain;
		}
		Asked.ParseIntoArray(Bits, TEXT(","), false);
		if (Bits.IsValidIndex(Red) && !Bits[Red].TrimStartAndEnd().IsEmpty())
		{
			Gain.X = FCString::Atod(*Bits[Red]);
		}
		if (Bits.IsValidIndex(Red + 1) && !Bits[Red + 1].TrimStartAndEnd().IsEmpty())
		{
			Gain.Y = FCString::Atod(*Bits[Red + 1]);
		}
		return FVector2D(FMath::Clamp(Gain.X, 0.5, 1.5), FMath::Clamp(Gain.Y, 0.5, 1.5));
	}

	/** The light (as the engine takes it, before the battle's grade) that a battle camera shows as a still's pixel (0 to
	 * 1), under a grade of these red and blue gains: the grade and its tone curve undone, as the Mac's first sea frames
	 * fit them (22 November: the engine's ACES curve at 1.58 of the light, within two levels; under the battle grade,
	 * red 1.08 and blue 0.87, the sea's own light kept at red 1.01, green 0.97 and blue 0.87; under another, those
	 * by the gains' change). */
	static FLinearColor LightOfStill(const FLinearColor& Still, const FVector2D& Grade)
	{
		auto One = [](double V, double Gain)
		{
			V = FMath::Clamp(V, 0.0, 0.999);
			const double Shown = V <= 0.04045 ? V / 12.92 : FMath::Pow((V + 0.055) / 1.055, 2.4);
			// (ACES fitted, Shown = X (2.51 X + 0.03) / (X (2.43 X + 0.59) + 0.14), solved for X: its one root above 0)
			const double A = 2.43 * Shown - 2.51, B = 0.59 * Shown - 0.03, C = 0.14 * Shown;
			const double X = (-B - FMath::Sqrt(FMath::Max(B * B - 4.0 * A * C, 0.0))) / (2.0 * A);
			return static_cast<float>(X / 1.58 / Gain);
		};
		const double Red = 1.01 * Grade.X / GradeOfTheFit.X, Blue = 0.87 * Grade.Y / GradeOfTheFit.Y;
		return FLinearColor(One(Still.R, FMath::Max(Red, 0.1)), One(Still.G, 0.97), One(Still.B, FMath::Max(Blue, 0.1)), 1.0f);
	}

	/** The sky's light at the horizon for the sea to mirror: "-TheaterSeaSky=" (a still's pixel, under the grade the
	 * camera has now) undone through the grade, or the Mac's first sea frames' sky, a still's 229, 230, 231 under the
	 * battle grade: the sky's own light, whatever the grade now. */
	static FLinearColor SeaSkyLight()
	{
		const bool bAsked = FString(FCommandLine::Get()).Contains(TEXT("TheaterSeaSky="));
		return bAsked ? LightOfStill(SkyColour(), GradeNow()) : LightOfStill(FLinearColor(229.0f / 255.0f, 230.0f / 255.0f, 231.0f / 255.0f), GradeOfTheFit);
	}

	/** Tells the kit the horizon's haze, where the kit takes it (the effects head 86d71f2: smoke far off pales into it,
	 * a mid grey until it is told), so this builds on the kit before and after. "-TheaterSeaSky=red,green,blue" (0 to
	 * 255, as a still's pixel) tries another. The scene's sky and hour are set once a battle and have no weather yet,
	 * so it is told once. */
	template <typename TKit>
	static void SetSky(TKit* Kit)
	{
		if constexpr (requires(TKit* K) { K->SetSky(FLinearColor()); })
		{
			Kit->SetSky(SkyColour());
		}
		else
		{
			(void)Kit;
		}
	}
}

ASeaBattle::ASeaBattle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

bool ASeaBattle::WantsReplay(FString& OutKey)
{
	return FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattle="), OutKey) && !OutKey.IsEmpty();
}

void ASeaBattle::ReplayWhenReady(const FString& InKey)
{
	// (one battle a scene: what it draws is kept by the show's keys for the whole battle, the wakes, the wrecks' navies,
	// the men's oil, and none of it is emptied; another battle takes a scene of its own)
	if (bBegun || !PendingReplay.IsEmpty())
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: %s asked for on a scene already playing one; a battle takes a scene of its own."), *InKey);
		return;
	}
	PendingReplay = InKey;
	SetActorTickEnabled(true);
}

FVector ASeaBattle::World(double X, double Y, double Z) const
{
	return FVector(Y * UnitsPerMetre, X * UnitsPerMetre, Z * UnitsPerMetre);
}

FVector ASeaBattle::WorldVector(double X, double Y, double Z) const
{
	return FVector(Y * UnitsPerMetre, X * UnitsPerMetre, Z * UnitsPerMetre);
}

FVector ASeaBattle::BoardOf(const FVector& WorldAt) const
{
	return FVector(WorldAt.Y, WorldAt.X, WorldAt.Z) / UnitsPerMetre;
}

// ---------------------------------------------------------------------------------------------------------------
// Opening the scene.

bool ASeaBattle::BeginReplay(const FString& InKey)
{
	if (bBegun || State.IsValid())
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: %s asked for on a scene already playing one; a battle takes a scene of its own."), *InKey);
		return false;
	}
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattleShots="), ShotFolder);
	float Pace = 1.0f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattleSpeed="), Pace);
	Speed = FMath::Clamp(static_cast<double>(Pace), 0.25, 4.0);
	// U1 hook (under water): a recording named by its path ("-TheaterSeaBattle=Tools/sea_show/stand-ins/convoy-night.json"),
	// so a Mac can play a convoy night before the battle's own recordings of one land; its pictures named by its file. Only
	// in a development build: a shipped game plays what is staged with it.
	bool bPath = false;
#if !UE_BUILD_SHIPPING
	bPath = InKey.EndsWith(TEXT(".json"));
#endif
	const FString File = bPath ? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), InKey) : FPaths::ProjectContentDir() / TEXT("Data/SeaBattles") / InKey + TEXT(".json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: no recorded action %s (is Content/Data/SeaBattles staged with the build?)"), *File);
		return false;
	}
	return OpenOn(Text, bPath ? FPaths::GetBaseFilename(InKey) : InKey, File);
}

bool ASeaBattle::BeginMonthScene(const FString& Text, const FString& Name)
{
	if (bBegun || State.IsValid() || !PendingReplay.IsEmpty())
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: the month's scene %s asked for on a scene already playing one; a battle takes a scene of its own."), *Name);
		return false;
	}
	// (inside the campaign: no picture run, no start later, its own pace by its windows; RunMonthScene)
	bMonthScene = true;
	ShotFolder.Empty();
	Speed = 1.0;
	BeatAt = 0;
	if (!OpenOn(Text, Name, TEXT("the month's sea scene (") + Name + TEXT(")")))
	{
		bMonthScene = false;
		return false;
	}
	return true;
}

bool ASeaBattle::OpenOn(const FString& Text, const FString& InKey, const FString& Where)
{
	using namespace SeaParts;
	// (funnel smoke wherever the kit has its own "funnel" kind; with a kit before it, none: the air trail borrowed for it
	// stood as a fence of streaks behind a ship, the first Mac frames of 21 November. "-TheaterSeaNoFunnels" for none.)
	bFunnels = SeaParts::HasFunnelSmoke<FBattleEffects>() && !FParse::Param(FCommandLine::Get(), TEXT("TheaterSeaNoFunnels"));
	{
		FString Asked;
		TArray<FString> Bits;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaWake="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(","), false) >= 1)
		{
			WakeBright = Bits[0].TrimStartAndEnd().IsEmpty() ? WakeBright : FMath::Clamp(FCString::Atod(*Bits[0]), 0.0, 4.0);
			if (Bits.Num() >= 2 && !Bits[1].TrimStartAndEnd().IsEmpty())
			{
				WakeStrength = FMath::Clamp(FCString::Atod(*Bits[1]), 0.0, 4.0);
			}
		}
	}
	SeaFeed::Recording Rec;
	std::string Error;
	if (!SeaFeed::ReadRecording(Narrow(Text), Rec, Error) || Rec.Views.empty())
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: %s: %s"), *Where, Error.empty() ? TEXT("no views") : *Wide(Error));
		return false;
	}
	const uint64 Seed = Rec.Seed ? Rec.Seed : 239;
	State = MakeShared<FSeaBattleState>(Seed);
	FSeaBattleState& S = *State;
	S.Rec = std::move(Rec);
	S.Show.SetField(S.Rec.Board);
	// Enemy boats unseen until found (the player's side from the first view's "me", as bDefender below): her hull, her
	// wake, her looks and the camera's eye kept off her until an escort holds her, a blow finds her or she is out of the
	// fight. "-TheaterSeaShowAll" draws every boat (the art director's look at a whole night).
	S.Show.SetMine(FParse::Param(FCommandLine::Get(), TEXT("TheaterSeaShowAll")) ? -1 : SeaFeed::SideOf(S.Rec.Views.front().Head.Me));
	S.BeginT = S.Rec.Views.front().Head.T;
	S.EndT = S.Rec.Views.back().Head.T;
	S.BattleT = S.BeginT;
	RecordKey = InKey;
	UWorld* Here = GetWorld();
	if (!Here)
	{
		return false;
	}

	// The mix (the sound director's mix.json): the scene reads its numbers, never copies them.
	{
		FString MixText;
		if (FFileHelper::LoadFileToString(MixText, *(FPaths::ProjectContentDir() / TEXT("Data/Sound/mix.json"))))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MixText), Mix);
		}
		if (!Mix.IsValid())
		{
			UE_LOG(LogTheater, Warning, TEXT("Sea battle: Content/Data/Sound/mix.json is missing: the battle is silent."));
		}
	}

	// The effects kit, drawn out to the sea distance (a fleet action is watched from far off).
	Effects = MakeUnique<FBattleEffects>();
	// (its ground: a ship's deck where the point lies over one, so a hit that is not on armour bursts on her deck, not at
	// the sea inside her hull; the sea's face everywhere else)
	if (!Effects->Begin(Root, UnitsPerMetre, [this](const FVector& Point)
		{
			FVector Deck;
			if (!bOnWater && Stage && Stage->DeckUnder(Point, Deck))
			{
				return Deck;
			}
			return FVector(Point.X, Point.Y, 0.0);
		}))
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: the effects kit did not begin (its recipe or material is missing): no guns, columns or smoke are drawn."));
		Effects.Reset();
	}
	else
	{
		// (a fleet action is fought 10 to 15 km apart: the air battle's 4 km would leave the far line unlit)
		UseSeaDistance(Effects.Get());
		const SeaShow::Vec Blow = S.Show.Wind();
		Effects->SetWind(FVector(Blow.Y, Blow.X, 0.0));
	}

	// The sun of the battle's day: its latitude and month, at an hour of the day from its seed (8 in the morning to 3 in
	// the afternoon), or "-TheaterSeaHour=".
	double Hour = 8.0 + static_cast<double>((Seed * 2654435761ull) % 7000ull) / 1000.0;
	{
		float Asked = -1.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaHour="), Asked) && Asked >= 5.0f && Asked <= 19.0f)
		{
			Hour = Asked;
		}
	}
	double SunAzimuth = 120.0, SunElevation = 30.0;
	SunAt(S.Rec.Board.Lat, S.Rec.Board.Month, Hour, SunAzimuth, SunElevation);
	SunElevation = FMath::Clamp(SunElevation, 12.0, 70.0);

	// The open sea and its ships: the Pearl Harbor stage's sea, hulls, hit looks and ends, raised with no land.
	Stage = Here->SpawnActor<AAirStage>(FVector::ZeroVector, FRotator::ZeroRotator);
	const double Middle = S.Rec.Board.Size * 0.5;
	if (Stage)
	{
		// (its sea the battle's own wind's, Brian's 23:37Z "make the water more realistic": the waves run the way it
		// blows, as long, as steep and as broken by whitecaps as its speed raises them; and the sky it mirrors low across
		// it, the sky's light over the horizon as the engine takes it, SeaSkyLight)
		const SeaShow::Vec Blow = S.Show.Wind();
		const double Blowing = S.Show.WindSpeed();
		const FLinearColor Sky = SeaParts::SeaSkyLight();
		// (a recorded calm is a light air on the stage, which takes no speed for no wind given and would lay Pearl's
		// trade-wind sea: at its least, 2 m/s, the sea lies almost glassy, a whitecap at most here and there; its swell
		// runs off the field's bearing)
		Stage->SetSeaWind(Blowing > 0.0 ? FMath::Atan2(Blow.X, Blow.Y) : S.Show.WindFrom() + UE_DOUBLE_PI, FMath::Max(Blowing, 0.5),
			Sky, SeaParts::GradeNow());
	}
	if (!Stage || !Stage->RaiseSea(FVector2D(Middle, Middle), SunAzimuth, SunElevation, UnitsPerMetre, Effects.Get(), Seed))
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: the open sea could not be raised."));
		return false;
	}
	if (Effects)
	{
		Effects->SetSun(Stage->TowardSun());
	}

	// The paint: the wakes and tracks see-through and unlit (M_Highlight, by each point's alpha), the aircraft as the
	// air battle's.
	{
		if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight")))
		{
			MarkInk = UMaterialInstanceDynamic::Create(Glow, this);
			MarkInk->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(WakeBright));
		}
		// (the foam on the map's wake ink, its noise breaking it up as it ages: Brightness a little over the marks' to
		// match their white, as its foam colour is greyer; no west edge at sea)
		if (UMaterialInterface* Wake = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Wake.M_Wake")))
		{
			FoamInk = UMaterialInstanceDynamic::Create(Wake, this);
			FoamInk->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(WakeBright * 1.15));
			FoamInk->SetScalarParameterValue(TEXT("WestEdge"), -1.0e9f);
			// (the churned band under the foam: the sea's own light a little lifted and greener, from the sky's light it
			// mirrors, 0.40, 0.47 and 0.44 of it: worked out on the Mac's fitted curve, it lifts the water under it by 20 to
			// 40 levels at a fifth to two fifths. The map's band, a bright teal, showed darker than the bright sea of
			// Samar c365, its streaks a grid, and would glare as a cyan stripe on the darker sea. "-TheaterSeaBand=r,g,b"
			// tries other shares.)
			FVector Band(0.40, 0.47, 0.44);
			FString BandAsked;
			TArray<FString> BandBits;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBand="), BandAsked, false) && BandAsked.ParseIntoArray(BandBits, TEXT(","), false) == 3)
			{
				Band = FVector(FCString::Atod(*BandBits[0]), FCString::Atod(*BandBits[1]), FCString::Atod(*BandBits[2])).ComponentMax(FVector::ZeroVector);
			}
			const FLinearColor Sky = SeaParts::SeaSkyLight();
			FoamInk->SetVectorParameterValue(TEXT("Band"), FLinearColor(static_cast<float>(Sky.R * Band.X), static_cast<float>(Sky.G * Band.Y), static_cast<float>(Sky.B * Band.Z), 1.0f));
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Sea battle: M_Wake is missing (run Tools/build_content.sh): the wakes are drawn whole, on M_Highlight."));
		}
		UMaterialInterface* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_MapSurface.M_MapSurface"));
		if (!Surface)
		{
			Surface = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		PlaneInk = UMaterialInstanceDynamic::Create(Surface, this);
		PlaneInk->SetScalarParameterValue(TEXT("Roughness"), 0.5f);
		PlaneInk->SetScalarParameterValue(TEXT("Specular"), 0.45f);
		PlaneInk->SetScalarParameterValue(TEXT("Grain"), 0.0f);
		PlaneInk->SetScalarParameterValue(TEXT("Tooth"), 0.0f);
		// (the wreckage and the men in the water: wet wood, canvas, paint and cloth, a little glossier than an
		// aircraft's paint, with the material's own unevenness)
		WreckInk = UMaterialInstanceDynamic::Create(Surface, this);
		WreckInk->SetScalarParameterValue(TEXT("Roughness"), 0.42f);
		WreckInk->SetScalarParameterValue(TEXT("Specular"), 0.5f);
		WreckInk->SetScalarParameterValue(TEXT("Grain"), 0.1f);
		WreckInk->SetScalarParameterValue(TEXT("Tooth"), 0.0f);
	}
	Marks = NewObject<UProceduralMeshComponent>(this, TEXT("SeaMarks"));
	Marks->SetupAttachment(Root);
	Marks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marks->SetCastShadow(false);
	Marks->bUseAsyncCooking = false;
	Marks->RegisterComponent();
	Foam = NewObject<UProceduralMeshComponent>(this, TEXT("SeaFoam"));
	Foam->SetupAttachment(Root);
	Foam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Foam->SetCastShadow(false);
	Foam->bUseAsyncCooking = false;
	Foam->RegisterComponent();
	for (TObjectPtr<UProceduralMeshComponent>* Made : { &WreckNear, &WreckFar })
	{
		*Made = NewObject<UProceduralMeshComponent>(this, Made == &WreckNear ? TEXT("SeaWreckNear") : TEXT("SeaWreckFar"));
		(*Made)->SetupAttachment(Root);
		(*Made)->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		(*Made)->SetCastShadow(Made == &WreckNear);
		(*Made)->bUseAsyncCooking = false;
		(*Made)->RegisterComponent();
	}
	// The sailor files (the soldiers thread's: Content/Models/Soldiers/soldier_<ARMY>_sailor.json, the soldiers' man on
	// their 32 bones with the clips of a man going over the side and in the water): the men drawn whole are their men
	// where one is read, each navy in its own file or, until that is made, in the American's rig in its own dress. (The
	// soldiers thread names the Royal Navy's file UK; a navy's first file read is its own.)
	TSet<FString> NaviesRead;
	for (const TPair<const TCHAR*, const TCHAR*>& Navy : { TPair<const TCHAR*, const TCHAR*>(TEXT("USA"), TEXT("USA")), TPair<const TCHAR*, const TCHAR*>(TEXT("JAP"), TEXT("JAP")),
			 TPair<const TCHAR*, const TCHAR*>(TEXT("GBR"), TEXT("UK")), TPair<const TCHAR*, const TCHAR*>(TEXT("GBR"), TEXT("GBR")), TPair<const TCHAR*, const TCHAR*>(TEXT("GER"), TEXT("GER")),
			 TPair<const TCHAR*, const TCHAR*>(TEXT("ITA"), TEXT("ITA")), TPair<const TCHAR*, const TCHAR*>(TEXT("SOV"), TEXT("SOV")), TPair<const TCHAR*, const TCHAR*>(TEXT("SOV"), TEXT("USSR")) })
	{
		const FString Army(Navy.Key);
		const FString Path = FPaths::ProjectContentDir() / TEXT("Models/Soldiers") / FString::Printf(TEXT("soldier_%s_sailor.json"), Navy.Value);
		FString Json;
		if (NaviesRead.Contains(Army) || !FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(Json, *Path))
		{
			continue;
		}
		std::string Why;
		if (S.Crew.AddFile(Narrow(Army), Narrow(Json), Why))
		{
			NaviesRead.Add(Army);
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Sea battle: %s: %s (that navy's men drawn as stand-ins)"), *Path, *Wide(Why));
		}
	}
	UE_LOG(LogTheater, Display, TEXT("Sea battle: %d sailor files read%s."), S.Crew.Files(), S.Crew.Files() == 0 ? TEXT(" (the men in the water drawn as stand-ins)") : TEXT(""));

	Eye = Here->SpawnActor<ACameraActor>();
	if (Eye)
	{
		Eye->GetCameraComponent()->SetFieldOfView(50.0f);
		// (the whole window, whatever its shape: a camera actor keeps 16:9 and bars the rest)
		Eye->GetCameraComponent()->bConstrainAspectRatio = false;
		Eye->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Eye->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
		// (the art director's battle grade, on battle cameras only; at sea neutral in colour, GradeNow)
		FPostProcessSettings& Lens = Eye->GetCameraComponent()->PostProcessSettings;
		BattleLook::Grade(Lens);
		if (SeaParts::OwnGrade())
		{
			const FVector2D Gain = SeaParts::GradeNow();
			Lens.bOverride_ColorGain = true;
			Lens.ColorGain = FVector4(Gain.X, 1.0, Gain.Y, 1.0);
			UE_LOG(LogTheater, Display, TEXT("Sea battle: the grade's colour at sea: red %.2f, blue %.2f (\"-TheaterSeaGrade=red,blue\" tries others)."), Gain.X, Gain.Y);
		}
	}
	TakeTheRoom(SunAzimuth, SunElevation);
	if (Effects)
	{
		// (the sky is set: the kit's far smoke pales into the haze the screen shows at the horizon)
		SeaParts::SetSky(Effects.Get());
	}
	if (APlayerController* Controller = Here->GetFirstPlayerController())
	{
		if (Eye)
		{
			Controller->SetViewTarget(Eye);
		}
		if (AHUD* Hud = Controller->GetHUD())
		{
			bHudWasShown = Hud->bShowHUD;
			Hud->bShowHUD = false;
			// (the campaign's own frame is a screen layer of its own, under the bar: off too, as the air scene has it,
			// or it stands in every picture now that the pictures take the screen)
			if (ACampaignHUD* Frame = Cast<ACampaignHUD>(Hud))
			{
				Frame->ShowFrame(false);
			}
		}
	}

	// U1 hook (under water): the view under the sea, bound to the room as it now is (the battle's haze, the eye, the
	// stage's sun and its key lamp), before the shots are planned (they ask it whether it is on).
	Under = MakeUnique<FSeaUnder>();
	{
		FSeaUnder::FRoom Below;
		Below.Owner = this;
		Below.Root = Root;
		Below.Eye = Eye;
		Below.Stage = Stage;
		for (const FKeptHaze& Kept : KeptHazes)
		{
			Below.Fogs.Add(Kept.Haze);
		}
		Below.Key = BattleKey;
		Below.UnitsPerMetre = UnitsPerMetre;
		Under->Bind(Below, S.Rec.Board);
	}

	PlanShots();
	if (!ShotFolder.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*ShotFolder, true);
	}
	// "-TheaterSeaBattleFrom=300": the action from there (what lasts of the time before is kept, as a picture run's skip).
	// The month's scene from its first window instead (RunMonthScene).
	bool bStartedLater = false;
	if (bMonthScene)
	{
		for (const std::string& Line : S.Rec.Card.Lines)
		{
			if (!Line.empty())
			{
				Caption.Add(Wide(Line));
			}
		}
		if (!S.Rec.Beats.empty() && S.Rec.Beats.front().From > S.BeginT + 1.0)
		{
			SkipTo(FMath::Min(S.Rec.Beats.front().From, S.EndT));
			bStartedLater = true;
		}
	}
	else
	{
		float From = -1.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattleFrom="), From) && From > S.BeginT + 1.0)
		{
			const double To = FMath::Min(static_cast<double>(From), S.EndT);
			bSkipping = true;
			for (double T = S.BattleT + 1.0; T < To; T += 1.0)
			{
				StepTo(T, true);
			}
			StepTo(To, true);
			bSkipping = false;
			bStartedLater = true;
		}
	}
	Feed(S.BattleT);
	Beds();

	// The tug-of-war bar (release 2; the UI director's ruling of 21 November): the player's flag at its left end and the
	// enemy's at its right, the card's own flags (Campaign/CampaignFlags); level at the start, or where the action stands
	// when it starts later.
	{
		const bool bDefender = SeaFeed::SideOf(S.Rec.Views.front().Head.Me) == 1;
		auto Flag = [this](const std::string& Nation) -> UTexture2D*
		{
			const FString Tag = Wide(Nation);
			if (Tag.IsEmpty())
			{
				return nullptr;
			}
			TArray<FLinearColor> Pixels;
			CampaignFlags::Paint(Tag, FLinearColor(0.4f, 0.4f, 0.4f), Pixels);
			return CampaignFlags::MakeTexture(Pixels, MakeUniqueObjectName(this, UTexture2D::StaticClass(), *(TEXT("SeaFlag_") + Tag)));
		};
		MyFlag = Flag(bDefender ? S.Rec.Board.DefNat : S.Rec.Board.AttNat);
		TheirFlag = Flag(bDefender ? S.Rec.Board.AttNat : S.Rec.Board.DefNat);
		BarKnot = 0.5;
		// (a recording that opens on the battle under way, a convoy night's window, its first view after the battle's own
		// start: where the action stands then, not level and sliding over with nothing happening)
		MoveKnot(0.0, bStartedLater || S.BeginT > 0.0);
		ShowScreen();
	}
	// The eye on its first shot before the first frame (not the camera's spawn spot), told as a cut; and again on the
	// first tick, when the stage has the ships where the action has them.
	AimEye();
	bEyeCut = true;
	bBegun = true;
	SetActorTickEnabled(true);
	if (bMonthScene)
	{
		// (the table put away under the scene, as the air scene has it: every piece off it and its hover quiet, and the
		// controller's clicks and keys kept from it while it is; the scene's screen takes the keys and the pointer)
		if (ACampaignMapActor* Map = Cast<ACampaignMapActor>(UGameplayStatics::GetActorOfClass(this, ACampaignMapActor::StaticClass())))
		{
			Map->SetPutAway(true);
			bTablePutAway = true;
		}
		if (APlayerController* Controller = Here->GetFirstPlayerController(); Controller && Screen.IsValid())
		{
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(Screen);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Controller->SetInputMode(Mode);
			bOwnInput = true;
		}
		if (ACampaignGameMode* Campaign = Cast<ACampaignGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			Campaign->DuckForScene(true);
		}
	}
	UE_LOG(LogTheater, Display, TEXT("Sea battle: %s, %s (%s), %d views, %.0f s, the sun at %.0f degrees and %.0f up at %.1f h, %d shots%s%s."), *RecordKey,
		*Wide(S.Rec.Board.Title), *Wide(S.Rec.Board.Date), static_cast<int32>(S.Rec.Views.size()), S.EndT - S.BeginT, SunAzimuth, SunElevation, Hour, Shots.Num(),
		ShotFolder.IsEmpty() ? TEXT("") : *(TEXT(", pictures into ") + ShotFolder),
		bMonthScene ? *FString::Printf(TEXT(", the month's scene in %d windows"), static_cast<int32>(S.Rec.Beats.size())) : TEXT(""));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// The room: the table put away, the battle's light, haze and sky; given back after.

void ASeaBattle::TakeTheRoom(double SunAzimuth, double SunElevation)
{
	if (bRoomTaken)
	{
		return;
	}
	bRoomTaken = true;
	const FVector Sun = Stage ? Stage->TowardSun() : FRotator(SunElevation, SunAzimuth, 0.0).Vector();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<ACampaignMapActor>() || It->IsA<AArmyPiece>())
		{
			if (!It->IsHidden())
			{
				KeptHidden.Add(*It);
				It->SetActorHiddenInGame(true);
			}
		}
		ACampaignEnvironment* Environment = Cast<ACampaignEnvironment>(*It);
		if (!Environment)
		{
			continue;
		}
		KeptRooms.Add(Environment);
		Environment->SetActorTickEnabled(false);
		// The haze of a battle seen from far off (the air battle's, the art director's ruling): none near the eye, thick
		// toward the horizon, so the near ships keep their darks and the far line greys into the sea's edge.
		TArray<UExponentialHeightFogComponent*> Fogs;
		Environment->GetComponents(Fogs);
		for (UExponentialHeightFogComponent* Fog : Fogs)
		{
			FKeptHaze& Kept = KeptHazes.AddDefaulted_GetRef();
			Kept.Haze = Fog;
			Kept.Start = Fog->StartDistance;
			Kept.Density = Fog->FogDensity;
			Kept.Falloff = Fog->FogHeightFalloff;
			Kept.MostOpacity = Fog->FogMaxOpacity;
			Kept.Colour = SeaParts::HazeColour(Fog, FLinearColor(0.80f, 0.84f, 0.88f));
			Fog->SetStartDistance(2000.0f * static_cast<float>(UnitsPerMetre));
			Fog->SetFogDensity(0.00045f);
			Fog->SetFogHeightFalloff(0.006f);
			Fog->SetFogMaxOpacity(0.8f);
			Fog->SetFogInscatteringColor(SeaParts::HazeInk);
			BattleLook::AirHaze(*Fog, UnitsPerMetre);
		}
		TArray<UDirectionalLightComponent*> Lights;
		Environment->GetComponents(Lights);
		UDirectionalLightComponent* Main = nullptr;
		for (UDirectionalLightComponent* Light : Lights)
		{
			FKeptLamp& Kept = KeptLamps.AddDefaulted_GetRef();
			Kept.Lamp = Light;
			Kept.Rotation = Light->GetRelativeRotation();
			Kept.Intensity = Light->Intensity;
			Kept.Colour = Light->GetLightColor();
			Kept.Temperature = Light->Temperature;
			Kept.SourceAngle = Light->LightSourceAngle;
			Kept.SunDisc = Light->AtmosphereSunDiskColorScale;
			if (Light->GetFName() == TEXT("KeyLight") || (!Main && Lights.Num() == 1))
			{
				Main = Light;
			}
		}
		if (!Main)
		{
			for (UDirectionalLightComponent* Light : Lights)
			{
				if (!Main || Light->Intensity > Main->Intensity)
				{
					Main = Light;
				}
			}
		}
		if (Main)
		{
			BattleKey = Main;
			Main->SetWorldRotation((-Sun).Rotation());
			// (a sky behind the battle, lit by its sun; taken away in GiveBackTheRoom)
			BattleLook::AirSkyBegin(*this, Main);
			// The sun a hard small disc, as the air battle's (the art director on build 1750): the true sun's half degree,
			// holding six thousandths of its light, the haze throwing it forward at 0.75.
			Main->LightSourceAngle = 0.53f;
			Main->AtmosphereSunDiskColorScale = FLinearColor(0.006f, 0.006f, 0.006f);
			Main->MarkRenderStateDirty();
			TArray<USkyAtmosphereComponent*> Atmospheres;
			GetComponents(Atmospheres);
			for (USkyAtmosphereComponent* Atmosphere : Atmospheres)
			{
				Atmosphere->SetMieAnisotropy(0.75f);
			}
		}
		TArray<USkyLightComponent*> Skies;
		Environment->GetComponents(Skies);
		for (USkyLightComponent* Sky : Skies)
		{
			FKeptSky& Kept = KeptSkies.AddDefaulted_GetRef();
			Kept.Sky = Sky;
			Kept.Intensity = Sky->Intensity;
			Kept.Colour = Sky->GetLightColor();
		}
	}
	LightTheBattle();
}

void ASeaBattle::LightTheBattle()
{
	// The battle's day, as the air battle's: every lamp white, the key at the table's day strength and warmth, the
	// others at the fill's, the sky's light the day's; said again where the room has changed it.
	for (const FKeptLamp& Kept : KeptLamps)
	{
		UDirectionalLightComponent* Light = Kept.Lamp.Get();
		if (!Light)
		{
			continue;
		}
		const bool bMain = Light == BattleKey.Get();
		// U1 hook (under water): under the sea the sun's lamp is what reaches her depth, in the water's own colour
		const float Want = (bMain ? 7.0f : 0.75f) * (Under ? Under->LightShare() : 1.0f);
		const FLinearColor Shade = Under ? Under->LightTint() : FLinearColor::White;
		if (!Light->GetLightColor().Equals(Shade, 0.002f))
		{
			Light->SetLightColor(Shade);
		}
		if (!FMath::IsNearlyEqual(Light->Intensity, Want, 0.001f))
		{
			Light->SetIntensity(Want);
		}
		if (bMain && !FMath::IsNearlyEqual(Light->Temperature, 4900.0f, 1.0f))
		{
			Light->SetTemperature(4900.0f);
		}
	}
	for (const FKeptSky& Kept : KeptSkies)
	{
		USkyLightComponent* Sky = Kept.Sky.Get();
		if (!Sky)
		{
			continue;
		}
		// U1 hook (under water): the sky's light tinted and dimmed with the sun's, so the under-water picture does not
		// wash out and nothing pops at the face
		const FLinearColor Shade = Under ? Under->LightTint() : FLinearColor::White;
		const float SkyWant = 0.72f * (Under ? FMath::Max(0.3f, Under->LightShare()) : 1.0f);
		if (!Sky->GetLightColor().Equals(Shade, 0.002f))
		{
			Sky->SetLightColor(Shade);
		}
		if (!FMath::IsNearlyEqual(Sky->Intensity, SkyWant, 0.001f))
		{
			Sky->SetIntensity(SkyWant);
		}
	}
	// (the campaign's controller may stand a piece again under the pointer: put away again)
	for (TActorIterator<AArmyPiece> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden())
		{
			KeptHidden.AddUnique(*It);
			It->SetActorHiddenInGame(true);
		}
	}
}

void ASeaBattle::GiveBackTheRoom()
{
	if (!bRoomTaken)
	{
		return;
	}
	bRoomTaken = false;
	for (const FKeptHaze& Kept : KeptHazes)
	{
		if (UExponentialHeightFogComponent* Fog = Kept.Haze.Get())
		{
			Fog->SetStartDistance(Kept.Start);
			Fog->SetFogDensity(Kept.Density);
			Fog->SetFogHeightFalloff(Kept.Falloff);
			Fog->SetFogMaxOpacity(Kept.MostOpacity);
			Fog->SetFogInscatteringColor(Kept.Colour);
		}
	}
	BattleLook::AirSkyEnd();
	for (const FKeptLamp& Kept : KeptLamps)
	{
		if (UDirectionalLightComponent* Light = Kept.Lamp.Get())
		{
			Light->SetRelativeRotation(Kept.Rotation);
			Light->SetIntensity(Kept.Intensity);
			Light->SetLightColor(Kept.Colour);
			Light->SetTemperature(Kept.Temperature);
			Light->LightSourceAngle = Kept.SourceAngle;
			Light->AtmosphereSunDiskColorScale = Kept.SunDisc;
			Light->MarkRenderStateDirty();
		}
	}
	for (const FKeptSky& Kept : KeptSkies)
	{
		if (USkyLightComponent* Sky = Kept.Sky.Get())
		{
			Sky->SetIntensity(Kept.Intensity);
			Sky->SetLightColor(Kept.Colour);
		}
	}
	for (const TWeakObjectPtr<ACampaignEnvironment>& Room : KeptRooms)
	{
		if (Room.IsValid())
		{
			Room->SetActorTickEnabled(true);
		}
	}
	for (const TWeakObjectPtr<AActor>& Hidden : KeptHidden)
	{
		if (Hidden.IsValid())
		{
			Hidden->SetActorHiddenInGame(false);
		}
	}
	if (UWorld* Here = GetWorld())
	{
		if (APlayerController* Controller = Here->GetFirstPlayerController())
		{
			if (AHUD* Hud = Controller->GetHUD())
			{
				Hud->bShowHUD = bHudWasShown;
				if (ACampaignHUD* Frame = Cast<ACampaignHUD>(Hud))
				{
					Frame->ShowFrame(true);
				}
			}
		}
	}
	BattleKey.Reset();
	KeptHazes.Reset();
	KeptLamps.Reset();
	KeptSkies.Reset();
	KeptRooms.Reset();
	KeptHidden.Reset();
}

// ---------------------------------------------------------------------------------------------------------------
// The action, frame by frame.

void ASeaBattle::Feed(double T)
{
	FSeaBattleState& S = *State;
	auto TakeNext = [&S]()
	{
		const SeaFeed::Frame& F = S.Rec.Views[S.Taken++];
		S.Show.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
	};
	// (one view in hand ahead of what is shown, so the picture between views has somewhere to go: the cloud test's way)
	if (S.Taken == 0 && !S.Rec.Views.empty())
	{
		TakeNext();
	}
	while (S.Taken < S.Rec.Views.size() && S.Rec.Views[S.Taken - 1].Head.T <= T + 1e-9)
	{
		TakeNext();
	}
}

void ASeaBattle::StepTo(double T, bool bSkip)
{
	FSeaBattleState& S = *State;
	Feed(T);
	const FVector Seen = BoardOf(EyeAt);
	S.Show.Eye = SeaShow::Vec(Seen.X, Seen.Y, FMath::Max(Seen.Z, 2.0));
	S.Show.Advance(T);
	S.BattleT = T;
	LayAndSteer();
	LayWakes();
	PlayEffects(bSkip);
}

void ASeaBattle::LayAndSteer()
{
	FSeaBattleState& S = *State;
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		const FVector2D At(Ship.At.X, Ship.At.Y);
		const double Face = FMath::DegreesToRadians(Ship.Heading);
		if (!Laid.Contains(Ship.Key))
		{
			FStageShip Given;
			Given.Id = Ship.Key;
			Given.Class = SeaParts::Wide(Ship.Class);
			Given.Nation = SeaParts::Wide(Ship.Nation);
			Given.At = At;
			Given.Face = Face;
			Stage->SetShips(TArray<FStageShip>{ Given });
			Laid.Add(Ship.Key);
			double Long = Ship.Long;
			FVector Bow = FVector::ZeroVector;
			bool bHarbour = false;
			if (Stage->ShipLie(Ship.Key, Long, Bow, bHarbour))
			{
				DrawnLong.Add(Ship.Key, Long);
			}
		}
		// A boat (ruling 241) drawn as far under as the show has her dived. D1 hook (convoy looks): the submarine scene's
		// boat states (surfaced, periscope, deep) may give her own heave here.
		const double Heave = FConvoyLooks::Steer(SubLooks, Root, Stage, Effects.Get(), S.Show, UnitsPerMetre, S.BattleT, bSkipping, bOnWater, Ship); // D1 hook (convoy looks)
		// U1 hook (under water): the boat in the picture shaken by a burst near her, under the water only
		Stage->Steer(Ship.Key, At, Face, Ship.Heel + (Under ? Under->Shudder(Ship.Key, S.BattleT) : 0.0), Ship.Pitch, Heave);
		// (an enemy boat the player has not found: steered in her place, so that found she is where she should be, but not
		// drawn, and no last sight of her for the camera: SeaShow's SetMine)
		Stage->SetShown(Ship.Key, Ship.bShown);
		if (Ship.bShown)
		{
			LastSeen.Add(Ship.Key, FVector4(At.X, At.Y, Ship.Heading, DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long));
		}
		// A boat's deck gun and a merchant's gun drawn laid as the show lays it (the review of 23 November: the model's gun
		// stood stowed while its rounds flew off trained), run back as it fires (a tenth of a second) and out again (half a
		// second more), a merchant's other gun lying stowed (TrainGun); and a boat told down or under, her hits' looks put out
		// so that nothing of them burns on under the sea (PutOut, when ConvoyLook::PutsOutLooks says: the rule the sea show's
		// cloud test holds the show's hits on boats to).
		if (Ship.Class == "ss" || Ship.Class == "mv")
		{
			const bool bStern = Ship.Class == "mv" && Ship.bAftGun;
			const double Since = S.BattleT - Ship.FiredAt;
			const double Back = Ship.FiredAt < 0.0 || Since < 0.0 || Since >= 0.6 ? 0.0 : Since < 0.1 ? Since / 0.1 : 1.0 - (Since - 0.1) / 0.5;
			Stage->TrainGun(Ship.Key, bStern ? TEXT("turret.Y") : TEXT("turret.A"), Ship.Train, Ship.Elevation, Back);
			if (Ship.Class == "mv")
			{
				Stage->TrainGun(Ship.Key, bStern ? TEXT("turret.A") : TEXT("turret.Y"), bStern ? 0.0 : 180.0, 0.0, 0.0);
			}
			if (Ship.Class == "ss" && ConvoyLook::PutsOutLooks(Ship.Dive, Ship.Dived))
			{
				Stage->PutOut(Ship.Key, S.BattleT);
			}
		}
	}
}

void ASeaBattle::PlayEffects(bool bSkip)
{
	using namespace SeaParts;
	FSeaBattleState& S = *State;
	const double Now = S.BattleT;
	FBattleEffects* Kit = Effects.Get();
	// (the words, each said once a step: six carriers flying off alike at one moment are one line, "(6 times)")
	TArray<TPair<FString, int32>> Said;
	for (const SeaShow::Fx& E : S.Show.Effects)
	{
		if (Under)
		{
			Under->Effect(E, S.Show, Now, bSkip); // U1 hook (under water): the bursts, an ahead salvo's bombs, her end
		}
		const FVector At = World(E.At.X, E.At.Y, E.At.Z);
		const FVector OnSea = World(E.At.X, E.At.Y, 0.0);
		// What lasts, played even while a picture run skips ahead: every hit on the ship it struck, a ship's end, the
		// torpedo tracks, oil burning, the words.
		switch (E.Kind)
		{
		case SeaShow::FxKind::ShellHit:
		case SeaShow::FxKind::BombHit:
			// Every hit the battle lands shows on the ship it struck (Brian, 21 November: the odds the rules', but a hit always
			// marks what it hit). Her looks (Battle/ShipLooks, by the shell's or the bomb's weight) keep her hurt, list,
			// fires and steam; in a stretch a picture run skips, they play from the moment it lands, so a ship hit while
			// the run skipped is as hurt in the next picture as she should be.
			Stage->Hit(E.Key, Wide(E.What), Now);
			break;
		case SeaShow::FxKind::TorpedoHit:
			// (its sound is the battle's own cue)
			Stage->Hit(E.Key, TEXT("torpedo"), Now);
			break;
		case SeaShow::FxKind::Sink:
		{
			// (a convoy's boat crushed deep, scuttled, rammed or shelled, ruling 241, goes down on the stage as a ship that
			// sinks: the stage knows a ship's six ways, and is the air scene's too; one that struck and that her crew then
			// scuttled floods aft first through her opened vents and goes by the stern)
			const SeaShow::ShipOut* Sunk = S.Show.Ship(E.Key);
			const std::string StageEnd = Sunk ? SeaShow::StageEndOf(E.What, Sunk->Struck, Sunk->Lowered) : std::string();
			Stage->Sink(E.Key, StageWay(E.What), E.Size > 0.5, Now, Wide(StageEnd));
			// D1 hook (convoy looks): Sink (E.What "crushed": the hump of air and the boil; "scuttled": men and rafts)
			if (SubLooks) { SubLooks->Effect(E, Now, bSkip); } // D1 hook (convoy looks)
			break;
		}
		case SeaShow::FxKind::Surrender:
			// (a boat striking: her blown tanks, her flag and, scuttled, her vents are the convoy looks'; her crew on her casing
			// the scene's men (DrawWrecks, Show::Hands). Kept while a picture run skips, so a boat that struck in it is up,
			// her flag flying, after)
			// D1 hook (convoy looks): Surrender
			if (SubLooks) { SubLooks->Effect(E, Now, bSkip); } // D1 hook (convoy looks)
			break;
		case SeaShow::FxKind::Dive:
			// (kept while a picture run skips too: a boat forced up in it rides high after, as ConvoyLooks.h says she does)
			// D1 hook (convoy looks): Dive
			if (SubLooks) { SubLooks->Effect(E, Now, bSkip); } // D1 hook (convoy looks)
			break;
		case SeaShow::FxKind::Rescue:
			// (kept while a picture run skips too: a rescuer's nets are kept across it, ConvoyLooks.h, for the men she picks up
			// after)
			// D1 hook (convoy looks): Rescue
			if (SubLooks) { SubLooks->Effect(E, Now, bSkip); } // D1 hook (convoy looks)
			break;
		// (the torpedo tracks: a track begun while a run skips is drawn from where it was fired, its head where it is now)
		case SeaShow::FxKind::TrackStart:
		{
			FTrackLine& Line = Tracks.FindOrAdd(E.Key);
			Line.From = Line.Head = FVector2D(E.At.X, E.At.Y);
			Line.StopT = -1.0;
			// (a Japanese warship's torpedo from 1938 is the oxygen-driven Type 93: a third of the bubbles; the art director,
			// Samar r2e frame 07; and a boat's electric torpedo, ruling 241, leaves no more)
			Line.bOxygen = (KitNation(E.Text) == TEXT("jp") && S.Rec.Board.Year >= 1938) || E.What == "wakeless";
			break;
		}
		case SeaShow::FxKind::TrackMove:
			if (FTrackLine* Line = Tracks.Find(E.Key))
			{
				Line->Head = FVector2D(E.At.X, E.At.Y);
			}
			break;
		case SeaShow::FxKind::TrackStop:
			if (FTrackLine* Line = Tracks.Find(E.Key))
			{
				Line->Head = FVector2D(E.At.X, E.At.Y);
				Line->StopT = Now;
			}
			break;
		case SeaShow::FxKind::OilFire:
			// (where a ship went down the stage lights her own column as she goes: only a burning ship's is the scene's)
			if (E.What == "sunk")
			{
				break;
			}
			if (bSkip)
			{
				FOilLater& Oil = OilLater.AddDefaulted_GetRef();
				Oil.Id = OilIds + E.Key;
				Oil.At = FVector2D(E.At.X, E.At.Y);
				Oil.Reach = E.Size;
				Oil.Until = E.Until;
				Oil.Since = E.Since;
				Oil.LitAt = Now;
				Oil.Kind = Wide(E.What);
			}
			else if (Kit)
			{
				// (on the water where she caught fire, not on her deck: nothing moves an oil fire, and she limps on)
				TGuardValue<bool> Water(bOnWater, true);
				Kit->OilFire(OilIds + E.Key, OnSea, static_cast<float>(E.Size), Wide(E.What), E.Until, static_cast<float>(E.Since));
			}
			break;
		case SeaShow::FxKind::GunStop:
			// (a ship's light gun ceasing fire, kept while a run skips too, so no stream is left firing at nothing)
			GunStopped(E.Key);
			break;
		default:
			break;
		}
		if (E.Kind == SeaShow::FxKind::Dispatch ||
		    ((E.Kind == SeaShow::FxKind::Sink || E.Kind == SeaShow::FxKind::LeaveLine || E.Kind == SeaShow::FxKind::Surrender) && !E.Text.empty()))
		{
			const FString Words = Wide(E.Text);
			if (TPair<FString, int32>* Same = Said.FindByPredicate([&Words](const TPair<FString, int32>& Told) { return Told.Key == Words; }))
			{
				++Same->Value;
			}
			else
			{
				Said.Add(TPair<FString, int32>(Words, 1));
			}
		}
		if (bSkip)
		{
			continue;
		}
		switch (E.Kind)
		{
		// A convoy's night (ruling 241): the submarine scene's looks are the production lead's helper's (new files of its
		// own), at the hooks marked so; until they land, only the drop is told to the log.
		case SeaShow::FxKind::ChargeDrop:
			// D1 hook (convoy looks): ChargeDrop
			if (SubLooks) { SubLooks->Effect(E, Now, bSkip); } // D1 hook (convoy looks)
			UE_LOG(LogTheater, Verbose, TEXT("Sea battle %.0f s: %s depth charges from ship %d, %d of them"), Now, *Wide(E.What), E.Key, static_cast<int32>(E.Size));
			break;
		case SeaShow::FxKind::ChargeBurst:
			// D1 hook (convoy looks): ChargeBurst
			if (SubLooks) { SubLooks->Effect(E, Now, bSkip); } // D1 hook (convoy looks)
			break;
		case SeaShow::FxKind::Starshell:
			// D1 hook (convoy looks): Starshell
			if (SubLooks) { SubLooks->Effect(E, Now, bSkip); } // D1 hook (convoy looks)
			break;
		case SeaShow::FxKind::Salvo:
		{
			// A boat's deck gun or a merchant's gun (the convoy battle's "gun": one gun, one round): its flash and smoke at its
			// muzzle as the stage draws it laid now (TrainGun, LayAndSteer: muzzle.turret.A.1, a merchant's stern gun's
			// muzzle.turret.Y.1 where the show's bAftGun has that one bearing) and out along its bore; its crack after the
			// flash (the mix's sea.deckGun.fire where the sound director has given one, else the field gun's).
			if (E.What == "ss" || E.What == "mv")
			{
				const SeaShow::ShipOut* Firer = S.Show.Ship(E.Key);
				if (Firer && Firer->bShown && Kit)
				{
					const bool bStern = E.What == "mv" && Firer->bAftGun;
					FVector GunMuzzle = FVector::ZeroVector, GunBore = FVector::ZeroVector;
					if (!Stage || !Stage->SocketAt(E.Key, bStern ? TEXT("muzzle.turret.Y.1") : TEXT("muzzle.turret.A.1"), GunMuzzle, GunBore))
					{
						// (no model of hers with the gun, as on the kit's code-built boat: its mount where the show puts it, a
						// boat's a quarter of her length forward of her middle, an American fleet boat's aft of her tower; a
						// merchant's on her forecastle or her poop; laid as the show lays it, a barrel of 45 calibres out from it,
						// an 88 mm's about 4 m and a 4-inch's 4.6)
						const FVector2D GunWay = Ahead(Firer->Heading + Firer->Train);
						const double GunUp = FMath::DegreesToRadians(FMath::Clamp(Firer->Elevation, 0.0, 45.0));
						GunBore = WorldVector(GunWay.X * FMath::Cos(GunUp), GunWay.Y * FMath::Cos(GunUp), FMath::Sin(GunUp)).GetSafeNormal();
						const double Barrel = FMath::Clamp(E.Size * 0.045, 2.0, 6.5) * UnitsPerMetre;
						const double GunLong = DrawnLong.Contains(E.Key) ? DrawnLong[E.Key] : Firer->Long;
						const double Along = E.What == "ss" ? (Firer->bAftGun ? -0.18 : 0.26) : bStern ? -0.47 : 0.45;
						const FVector2D Mount = FVector2D(Firer->At.X, Firer->At.Y) + Ahead(Firer->Heading) * (Along * GunLong);
						const double High = E.What == "ss" ? GunLong * 0.052 : Firer->Beam * 0.52;
						GunMuzzle = World(Mount.X, Mount.Y, High + Firer->Heave) + GunBore * Barrel;
					}
					Kit->Muzzle(GunMuzzle, GunBore.IsNearlyZero() ? FVector::UpVector : GunBore, TEXT("ship_aa"), TEXT("water"));
				}
				// (an enemy boat out of the player's sight is never heard: SeaShow's SetMine finds her as she fires, so this is
				// only a guard)
				if (!Firer || Firer->bShown)
				{
					const FString Bang = MixTake(Mix, TEXT("sea.deckGun.fire"), 0);
					Sound(Bang.IsEmpty() ? GunsOf(E.What) : FName(*Bang), At, TEXT("big_guns"));
				}
				break;
			}
			// Her turrets' flashes along her deck, laid toward the target and up; her guns heard after the flash.
			if (const SeaShow::ShipOut* From = S.Show.Ship(E.Key); From && Kit)
			{
				const FVector2D Bow = Ahead(From->Heading);
				const FVector2D ToAim = (FVector2D(E.Aim.X, E.Aim.Y) - FVector2D(From->At.X, From->At.Y)).GetSafeNormal();
				const FVector Facing = (WorldVector(ToAim.X, ToAim.Y, 0.0).GetSafeNormal() + FVector(0.0, 0.0, 0.3)).GetSafeNormal();
				const double Long = DrawnLong.Contains(E.Key) ? DrawnLong[E.Key] : From->Long;
				const TCHAR* Gun = E.Size >= 180.0 ? TEXT("howitzer") : TEXT("tank_gun");
				TArray<double> Turrets;
				if (E.What == "bb")
				{
					Turrets = { 0.30, 0.17, -0.28 };
				}
				else if (E.What == "dd")
				{
					Turrets = { 0.30, -0.22 };
				}
				else
				{
					Turrets = { 0.27, -0.26 };
				}
				for (const double Along : Turrets)
				{
					const FVector2D Muzzle = FVector2D(From->At.X, From->At.Y) + Bow * (Along * Long) + ToAim * (From->Beam * 0.45);
					Kit->Muzzle(World(Muzzle.X, Muzzle.Y, FMath::Max(From->Beam * 0.35, 4.0)), Facing, Gun, TEXT("water"));
				}
			}
			Sound(GunsOf(E.What), At, TEXT("big_guns"));
			break;
		}
		case SeaShow::FxKind::Splash:
			if (Kit)
			{
				Kit->ShellOfSize(OnSea, SplashRow(E.What), TEXT("water"));
			}
			Sound(TEXT("shell_splash"), OnSea, TEXT("explosions"), 0.8f);
			break;
		case SeaShow::FxKind::ShellHit:
		case SeaShow::FxKind::BombHit:
			// (laid on her above, with what lasts)
			Sound(TEXT("shell_impact_near"), At, TEXT("explosions"));
			break;
		case SeaShow::FxKind::NearMiss:
			if (Kit)
			{
				Kit->ShellOfSize(OnSea, E.What == "bomb250" ? TEXT("bomb250") : TEXT("how105"), TEXT("water"));
			}
			Sound(TEXT("shell_splash"), OnSea, TEXT("explosions"));
			break;
		case SeaShow::FxKind::ScreenStart:
			if (!bScreensSaid)
			{
				bScreensSaid = true;
				UE_LOG(LogTheater, Display, TEXT("Sea battle: a smoke screen is laid (the effects kit has no smoke screen yet: not drawn)."));
			}
			break;
		case SeaShow::FxKind::Flak:
		{
			FFlakLater& Burst = FlakLater.AddDefaulted_GetRef();
			Burst.At = Now + E.Size;
			Burst.Where = At;
			Burst.Kind = Wide(E.What);
			break;
		}
		case SeaShow::FxKind::PlaneDown:
			if (Kit)
			{
				Kit->AirFall(Now, FallIds + E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), TEXT("burn"), 13.0f, 0.0);
				Falling.Add(E.Key, At);
			}
			break;
		case SeaShow::FxKind::Ditch:
			if (Kit)
			{
				Kit->ShellOfSize(OnSea, TEXT("how105"), TEXT("water"));
			}
			break;
		case SeaShow::FxKind::Sound:
			Sound(FName(*Wide(E.What)), At, TEXT("ships"));
			break;
		case SeaShow::FxKind::Gun:
			// A ship's light guns at the aircraft (Battle/ShipGuns, the historian's ship-air-defence.md B1/B2): each mount's
			// stream of tracer from her deck, laid each step while it fires, in her nation's colour.
			if (Kit)
			{
				AirGun(Kit, Now, E.Key, At, World(E.Aim.X, E.Aim.Y, E.Aim.Z), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), Wide(E.What), static_cast<float>(E.Rate),
				       static_cast<float>(E.Mv * UnitsPerMetre), KitNation(E.Text));
			}
			GunFired(E.Key, ShipGuns::KindSlot(E.What), At);
			break;
		case SeaShow::FxKind::ShipGun:
		{
			// A heavy gun's round at the aircraft (its burst, or its splash ahead of a torpedo plane, follows when it gets
			// there): the flash and smoke along the bore at her deck (muzzle.ship_aa; a main battery's, muzzle.ship_main),
			// and the boom (the mix's flak firing take, family big_guns) after its flash, no closer together than the mix's
			// soundEvery.
			if (Kit)
			{
				const FVector Bore = (World(E.Aim.X, E.Aim.Y, E.Aim.Z) - At).GetSafeNormal();
				Kit->Muzzle(At, Bore.IsNearlyZero() ? FVector::UpVector : Bore, E.Size >= 200.0 ? TEXT("ship_main") : TEXT("ship_aa"), TEXT("water"));
			}
			if (RealT - ShipGunHeardAt >= MixNumber(Mix, TEXT("air.flak.soundEvery"), 0.6))
			{
				ShipGunHeardAt = RealT;
				const FString Boom = MixTake(Mix, TEXT("air.flak.fire"), 0);
				const FString Family = MixTake(Mix, TEXT("air.flak.fireFamily"));
				Sound(FName(Boom.IsEmpty() ? TEXT("flak_heavy_fire_01") : *Boom), At, FName(Family.IsEmpty() ? TEXT("big_guns") : *Family));
			}
			break;
		}
		case SeaShow::FxKind::PlaneHit:
			// (rounds striking an aircraft: the kit's sparks and pieces, as the air scene's)
			if (Kit)
			{
				Kit->AirHit(Now, At, WorldVector(E.Normal.X, E.Normal.Y, E.Normal.Z).GetSafeNormal(), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), Wide(E.What));
			}
			break;
		default:
			break;
		}
	}
	for (const TPair<FString, int32>& Told : Said)
	{
		if (Told.Value > 1)
		{
			UE_LOG(LogTheater, Display, TEXT("Sea battle %.0f s: %s (%d times)"), Now, *Told.Key, Told.Value);
		}
		else
		{
			UE_LOG(LogTheater, Display, TEXT("Sea battle %.0f s: %s"), Now, *Told.Key);
		}
	}
	S.Show.Effects.clear();
}

// ---------------------------------------------------------------------------------------------------------------
// Drawing: the strikes' aircraft, the wakes and tracks on the sea, the trails of smoke and fire.

void ASeaBattle::DrawPlanes()
{
	FSeaBattleState& S = *State;
	const double Now = S.BattleT;
	FBattleEffects* Kit = Effects.Get();
	// (the models are built nose toward -X with the right hand at -Y: each is turned half round before its heading,
	// climb and bank, as the air battle's)
	const FQuat HalfRound = FRotator(0.0, 180.0, 0.0).Quaternion();
	TSet<int32> Seen;
	for (const SeaShow::PlaneOut& P : S.Show.Planes())
	{
		if (!P.bShown)
		{
			continue;
		}
		Seen.Add(P.Key);
		UProceduralMeshComponent* Mesh = Planes.FindRef(P.Key);
		if (!Mesh)
		{
			const FString Name = SeaParts::ModelName(SeaParts::Wide(P.Model), SeaParts::Wide(P.Nation), P.Role == "cover");
			const int32* Index = ShapeOf.Find(Name);
			if (!Index)
			{
				FMiniatureMesh Made;
				Made.Vehicle(Name, FVector::ZeroVector, UnitsPerMetre, 0.0, false);
				FPlaneShape& Shape = Shapes.AddDefaulted_GetRef();
				if (Made.Vertices.Num() > 0)
				{
					const FVector Middle = FBox(Made.Vertices).GetCenter();
					Shape.Points.Reserve(Made.Vertices.Num());
					for (const FVector& Point : Made.Vertices)
					{
						Shape.Points.Add(Point - Middle);
					}
					Shape.Triangles = Made.Triangles;
					Shape.Normals = Made.Normals;
					Shape.Colors = Made.Colors;
				}
				Index = &ShapeOf.Add(Name, Shapes.Num() - 1);
			}
			const FPlaneShape& Shape = Shapes[*Index];
			Mesh = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("SeaPlane_%d"), P.Key)));
			Mesh->SetupAttachment(Root);
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(true);
			Mesh->bUseAsyncCooking = false;
			Mesh->RegisterComponent();
			if (Shape.Points.Num() > 0)
			{
				Mesh->CreateMeshSection(0, Shape.Points, Shape.Triangles, Shape.Normals, TArray<FVector2D>(), Shape.Colors, TArray<FProcMeshTangent>(), false);
				Mesh->SetMaterial(0, PlaneInk);
			}
			Planes.Add(P.Key, Mesh);
		}
		const FVector Where = World(P.At.X, P.At.Y, P.At.Z);
		Mesh->SetWorldLocationAndRotation(Where, FRotator(P.Pitch, P.Heading, P.Bank).Quaternion() * HalfRound);
		if (P.bFalling && Kit && Falling.Contains(P.Key))
		{
			const FVector2D Way = SeaParts::Ahead(P.Heading);
			const double Climb = FMath::DegreesToRadians(P.Pitch);
			const FVector Velocity = WorldVector(Way.X * P.Speed * FMath::Cos(Climb), Way.Y * P.Speed * FMath::Cos(Climb), P.Speed * FMath::Sin(Climb));
			Kit->AirFallMove(Now, SeaParts::FallIds + P.Key, Where, Velocity);
			Falling.Add(P.Key, Where);
		}
	}
	for (auto It = Planes.CreateIterator(); It; ++It)
	{
		if (Seen.Contains(It.Key()))
		{
			continue;
		}
		if (It.Value())
		{
			It.Value()->DestroyComponent();
		}
		// (a falling aircraft gone: into the sea where it last was)
		if (const FVector* Last = Falling.Find(It.Key()))
		{
			if (Kit)
			{
				Kit->AirFallStop(SeaParts::FallIds + It.Key());
				Kit->ShellOfSize(FVector(Last->X, Last->Y, 0.0), TEXT("how105"), TEXT("water"));
			}
			Falling.Remove(It.Key());
		}
		It.RemoveCurrent();
	}
}

void ASeaBattle::LayWakes()
{
	// Each ship's wake: a point at her stern every 15 m she makes (or 2 s), laid every step, a picture run's skips too, so
	// the wake behind her after a skip is the one she made.
	FSeaBattleState& S = *State;
	const double Now = S.BattleT;
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		if (Ship.Doing == SeaShow::Fate::Sinking && Ship.Down >= 0.4)
		{
			continue;
		}
		const double Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
		const FVector2D Stern = FVector2D(Ship.At.X, Ship.At.Y) - SeaParts::Ahead(Ship.Heading) * (Long * 0.48);
		TArray<FWakePoint>& Path = Wakes.FindOrAdd(Ship.Key);
		if (Path.Num() == 0 || FVector2D::Distance(Path.Last().P, Stern) >= 15.0 || Now - Path.Last().T >= 2.0)
		{
			const double Along = Path.Num() == 0 ? 0.0 : Path.Last().S + FVector2D::Distance(Path.Last().P, Stern);
			FWakePoint& Made = Path.AddDefaulted_GetRef();
			Made.P = Stern;
			Made.T = Now;
			Made.Strength = Ship.Wake;
			Made.Beam = Ship.Beam;
			Made.Long = Long;
			Made.Speed = Ship.Speed;
			Made.S = Along;
		}
	}
}

void ASeaBattle::DrawMarks()
{
	using namespace SeaParts;
	FSeaBattleState& S = *State;
	const double Now = S.BattleT;
	// The foam, on the wake ink where the build has it (each point's paint: red how white, green how whole, the material
	// breaking up the rest with its noise, blue 1 for the churned band under the foam, alpha how much; its texture place
	// in the ship's beams, along the water and across), else on the marks' ink, whole, as much as is whole.
	const bool bInk = FoamInk != nullptr;
	TArray<FVector> FV, FN;
	TArray<FVector2D> FU;
	TArray<FColor> FC;
	TArray<int32> FT;
	auto Byte = [](double X) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(X * 255.0), 0, 255)); };
	auto Foamy = [&](const FVector2D& P, double Z, const FVector2D& UV, double White, double Whole, double Much, bool bBand)
	{
		FV.Add(World(P.X, P.Y, Z));
		FN.Add(FVector::UpVector);
		FU.Add(UV);
		if (bInk)
		{
			FC.Add(FColor(Byte(White), Byte(Whole), bBand ? 255 : 0, Byte(Much)));
		}
		else
		{
			FColor Paint = SeaParts::Foam;
			Paint.A = bBand ? 0 : Byte(Much * White * (0.35 + 0.65 * Whole));
			FC.Add(Paint);
		}
		return FV.Num() - 1;
	};
	// (a quad A B C D round its edge, the way round that faces up, as FMiniatureMesh::Face lays them; a sheet standing
	// on the water both ways round where the ink shows one side only)
	auto Quad = [&](TArray<int32>& Into, const TArray<FVector>& At, int32 A, int32 B, int32 C, int32 D, bool bStanding)
	{
		const FVector Facing = (At[B] - At[A]) ^ (At[D] - At[A]);
		if (bStanding && !bInk)
		{
			Into.Append({ A, B, C, A, C, D, A, C, B, A, D, C });
		}
		else if (Facing.Z <= 0.0)
		{
			Into.Append({ A, B, C, A, C, D });
		}
		else
		{
			Into.Append({ A, C, B, A, D, C });
		}
	};
	// A ribbon along a line of stations: each its middle, half width, place along the water (U, in beams) and five
	// columns across (Across, shares of the half width), each column with its height, wholeness and amount.
	struct FStation
	{
		FVector2D Mid = FVector2D::ZeroVector;
		double Half = 1.0, U = 0.0, Beam = 10.0, White = 1.0;
		double Z[5] = { 0.3, 0.3, 0.3, 0.3, 0.3 };
		double Whole[5] = { 1.0, 1.0, 1.0, 1.0, 1.0 };
		double Much[5] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
	};
	auto Ribbon = [&](const TArray<FStation>& Line, const double (&Across)[5], bool bBand)
	{
		const int32 Count = Line.Num();
		if (Count < 2)
		{
			return;
		}
		const int32 First = FV.Num();
		for (int32 K = 0; K < Count; ++K)
		{
			const FStation& St = Line[K];
			const FVector2D Way = (Line[FMath::Min(K + 1, Count - 1)].Mid - Line[FMath::Max(K - 1, 0)].Mid).GetSafeNormal();
			const FVector2D Side(Way.Y, -Way.X);
			for (int32 C = 0; C < 5; ++C)
			{
				const double Off = St.Half * Across[C];
				Foamy(St.Mid + Side * Off, St.Z[C], FVector2D(St.U, Off / FMath::Max(St.Beam, 1.0)), St.White, St.Whole[C], St.Much[C], bBand);
			}
		}
		for (int32 K = 0; K + 1 < Count; ++K)
		{
			for (int32 C = 0; C < 4; ++C)
			{
				const int32 A = First + K * 5 + C, B = A + 5;
				Quad(FT, FV, A, A + 1, B + 1, B, false);
			}
		}
	};
	static const double WakeAcross[5] = { -1.0, -0.8, 0.0, 0.8, 1.0 };
	static const double BandAcross[5] = { -1.25, -0.7, 0.0, 0.7, 1.25 };
	static const double ArmAcross[5] = { -1.0, -0.55, 0.0, 0.55, 1.0 };

	// The wakes (the campaign map's passed recipe, at the sea's own size; the art director on Samar r2e): from each
	// ship's stern a band of churned water and on it the foam, whitest and whole by the stern, widening to two beams,
	// breaking up and fading to nothing by WakeRun astern, never a crisp line of one width, never brighter far out than
	// at her stern. A point's place astern is how long ago she laid it times her speed then, so a ship that slows or
	// stops leaves her old wake to age on, and a stopped ship makes none.
	TSet<int32> Afloat;
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		Afloat.Add(Ship.Key);
	}
	TArray<TArray<FStation>> Bands, Wakes2;
	for (auto It = Wakes.CreateIterator(); It; ++It)
	{
		TArray<FWakePoint>& Path = It.Value();
		int32 Old = 0;
		while (Old < Path.Num() && Now - Path[Old].T > WakeLife)
		{
			++Old;
		}
		if (Old > 0)
		{
			Path.RemoveAt(0, Old);
		}
		if (Path.Num() == 0)
		{
			if (!Afloat.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
			continue;
		}
		TArray<FWakePoint> Line = Path;
		if (const SeaShow::ShipOut* Ship = S.Show.Ship(It.Key()); Ship && (Ship->Doing != SeaShow::Fate::Sinking || Ship->Down < 0.4))
		{
			const double Long = DrawnLong.Contains(It.Key()) ? DrawnLong[It.Key()] : Ship->Long;
			const FVector2D Stern = FVector2D(Ship->At.X, Ship->At.Y) - Ahead(Ship->Heading) * (Long * 0.48);
			if (FVector2D::Distance(Line.Last().P, Stern) > 1.0)
			{
				FWakePoint Here = Line.Last();
				Here.S += FVector2D::Distance(Line.Last().P, Stern);
				Here.P = Stern;
				Here.T = Now;
				Here.Strength = Ship->Wake;
				Here.Speed = Ship->Speed;
				Line.Add(Here);
			}
		}
		// (newest first: from her stern astern; a wake broken where time jumped, as a run started part way through)
		TArray<FStation> Band, Top;
		auto Flush = [&]()
		{
			if (Band.Num() >= 2)
			{
				Bands.Add(Band);
			}
			if (Top.Num() >= 2)
			{
				Wakes2.Add(Top);
			}
			Band.Reset();
			Top.Reset();
		};
		bool bFoamDone = false, bBandDone = false;
		for (int32 K = Line.Num() - 1; K >= 0; --K)
		{
			const FWakePoint& W = Line[K];
			if (K < Line.Num() - 1 && Line[K + 1].T - W.T > 6.0)
			{
				Flush();
				bFoamDone = bBandDone = false;
			}
			const double Long = FMath::Max(W.Long, 20.0);
			const double Run = WakeRun(Long) / Long;
			const double Lengths = FMath::Max(0.0, Now - W.T) * FMath::Max(W.Speed, 1.5) / Long;
			// (how much wake she made there: none stopped, all of it from about three quarters of her flank speed)
			const double Made = FMath::Clamp(W.Strength * 1.4, 0.0, 1.0) * WakeStrength;
			const double Ragged = 1.0 + 0.18 * (Hash01(W.S * 0.37 + It.Key()) - 0.5) * FMath::Min(1.0, Lengths * 2.0);
			if (!bFoamDone)
			{
				const double Far = FMath::Min(Lengths / Run, 1.0);
				const double Whole = WakeWhole(Lengths, Run);
				const double Much = Made * (1.0 - 0.55 * Far) * FMath::Pow(FMath::Clamp((1.0 - Far) / 0.25, 0.0, 1.0), 1.5);
				FStation St;
				St.Mid = W.P;
				St.Beam = W.Beam;
				St.U = -W.S / FMath::Max(W.Beam, 1.0);
				St.Half = WakeHalf(W.Beam, Lengths, Run) * Ragged;
				St.White = 1.0 - 0.3 * Far;
				const double Wholes[5] = { Whole * 0.6, Whole * 0.94, Whole, Whole * 0.94, Whole * 0.6 };
				const double Muches[5] = { 0.0, Much, Much, Much, 0.0 };
				for (int32 C = 0; C < 5; ++C)
				{
					St.Whole[C] = Wholes[C];
					St.Much[C] = Muches[C];
				}
				Top.Add(St);
				bFoamDone = Far >= 1.0;
			}
			if (bInk && !bBandDone)
			{
				// (the churned band under the foam: the sea's own colour lifted, strongest a little astern of her, half a
				// length longer than the foam, thinning to nothing; a ship that follows sits in it, faded and broken)
				const double Far = FMath::Min(Lengths / (Run + 0.5), 1.0);
				const double Much = Made * 0.4 * FMath::Min(1.0, Far * 10.0 + 0.5) * FMath::Pow(1.0 - Far, 0.7);
				FStation St;
				St.Mid = W.P;
				St.Beam = W.Beam;
				St.U = -W.S / FMath::Max(W.Beam, 1.0);
				St.Half = WakeHalf(W.Beam, FMath::Min(Lengths, Run), Run) * Ragged;
				for (int32 C = 0; C < 5; ++C)
				{
					St.Z[C] = 0.2;
					St.Much[C] = C == 0 || C == 4 ? 0.0 : Much;
				}
				Band.Add(St);
				bBandDone = Far >= 1.0;
			}
			if (bFoamDone && (bBandDone || !bInk))
			{
				break;
			}
		}
		Flush();
	}
	// (the band under all the foam: laid first, so the foam is drawn over it)
	for (const TArray<FStation>& Line : Bands)
	{
		Ribbon(Line, BandAcross, true);
	}
	for (const TArray<FStation>& Line : Wakes2)
	{
		Ribbon(Line, WakeAcross, false);
	}

	// Each ship under way: her bow wave climbing her stem, the white water along her sides, the faint arms of her
	// wave, and the boil at her stern (the art director on Samar r2e, frame 01).
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		const bool bMaking = Ship.Doing != SeaShow::Fate::Sinking || Ship.Down < 0.4;
		if (!bMaking || !Ship.bShown || Ship.Wake <= 0.12)
		{
			continue;
		}
		const double Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
		const double Beam = FMath::Max(Ship.Beam, 3.0);
		const double Strength = FMath::Clamp(Ship.Wake, 0.0, 1.0);
		const double Made = FMath::Clamp(Strength * 1.2, 0.0, 1.0) * WakeStrength;
		const FVector2D At(Ship.At.X, Ship.At.Y), Bow = Ahead(Ship.Heading), Right = Starboard(Ship.Heading);
		const FVector2D Stem = At + Bow * (Long * 0.5), Stern = At - Bow * (Long * 0.48);
		// (her half width X metres aft of her stem, as her hull narrows to it over the first quarter of her length)
		auto HullHalf = [Long, Beam](double X) { return 0.5 * Beam * FMath::Pow(FMath::Min(1.0, FMath::Max(X, 0.0) / (0.25 * Long)), 0.6); };
		// (and her half breadth at the waterline X metres aft of her stem, as her model has it, CampaignShips' hull: its
		// deck's breadth there, drawn in toward the water, more so toward the bow, and filling out to a broad stern)
		auto WaterHalf = [Long, Beam](double X)
		{
			const double T = 0.5 - FMath::Clamp(X / Long, 0.0, 1.0);
			double Top = 0.0;
			if (T >= 0.0)
			{
				Top = 0.5 * Beam * FMath::Pow(FMath::Max(0.0, 1.0 - FMath::Pow(T / 0.5, 1.9)), 0.62);
			}
			else
			{
				const double Full = FMath::Pow(FMath::Max(0.0, 1.0 - FMath::Pow(-T / 0.5, 3.2)), 0.5);
				Top = 0.5 * Beam * FMath::Lerp(Full, FMath::Max(Full, 0.55), 0.45 * FMath::Clamp((-T - 0.3) / 0.2, 0.0, 1.0));
			}
			return FMath::Max(Top, 0.25) * (T > 0.15 ? FMath::Lerp(0.94, 0.55, (T - 0.15) / 0.35) : 0.94);
		};
		// The bow wave: a sheet of white water standing against each side of her bow, highest just aft of the stem (a
		// destroyer's at flank speed about 3.5 m), curling out as it falls away aft over a third of her length.
		const double Rise = (0.4 + 3.0 * Strength * Strength) * FMath::Clamp(FMath::Sqrt(Long / 110.0), 0.8, 1.5);
		for (const double Hand : { -1.0, 1.0 })
		{
			const int32 Steps = 8;
			const int32 First = FV.Num();
			for (int32 K = 0; K <= Steps; ++K)
			{
				const double Share = static_cast<double>(K) / Steps;
				const double X = Share * 0.3 * Long, Ls = X / Long;
				const double Crest = Rise * (Ls < 0.06 ? FMath::Lerp(0.75, 1.0, Ls / 0.06) : FMath::Pow(FMath::Max(0.0, 1.0 - (Ls - 0.06) / 0.24), 1.3));
				const double Aft = 1.0 - Share * Share;
				const double Off = HullHalf(X) + 0.35 + 0.04 * Beam;
				const FVector2D Foot = Stem - Bow * X + Right * (Hand * Off);
				const double U = X / Beam;
				Foamy(Foot, 0.25, FVector2D(U, 0.0), 1.0, 1.0, 0.9 * Made * Aft, false);
				Foamy(Foot + Right * (Hand * 0.12 * Crest), 0.25 + 0.55 * Crest, FVector2D(U, 1.6 * Crest / Beam), 1.0, 0.85, 0.7 * Made * Aft, false);
				Foamy(Foot + Right * (Hand * 0.35 * Crest), 0.25 + Crest, FVector2D(U, 3.0 * Crest / Beam), 1.0, 0.6, 0.0, false);
			}
			for (int32 K = 0; K < Steps; ++K)
			{
				const int32 A = First + K * 3, B = A + 3;
				Quad(FT, FV, A, A + 1, B + 1, B, true);
				Quad(FT, FV, A + 1, A + 2, B + 2, B + 1, true);
			}
			// The white water along her side, from the bow wave's foot aft, thinning out by two thirds of her length.
			TArray<FStation> Side;
			for (int32 K = 0; K <= 6; ++K)
			{
				const double Share = static_cast<double>(K) / 6.0;
				const double X = Share * 0.7 * Long;
				const double Wide = 1.5 + 2.5 * Strength;
				FStation St;
				St.Mid = Stem - Bow * X + Right * (Hand * (HullHalf(X) + 0.3 + Wide * 0.5));
				St.Half = Wide * 0.5;
				St.Beam = Beam;
				St.U = X / Beam;
				const double Much = 0.55 * Made * (1.0 - Share);
				for (int32 C = 0; C < 5; ++C)
				{
					St.Whole[C] = C == 2 ? 0.75 : 0.55;
					St.Much[C] = C == 0 || C == 4 ? 0.0 : Much;
				}
				Side.Add(St);
			}
			Ribbon(Side, ArmAcross, false);
			// The foam line along her side (the art director on the Mac's Samar frames of 22 November: from a low eye the
			// white water lying flat beside her is a line too thin to see, and the hulls sat on the sea like blocks): a low
			// wall of broken white water against her side at the waterline, from the bow wave's foot to her stern, its
			// crests where her own waves' are (2 pi V^2 / g apart, the first at her bow), so highest at the bow and again
			// at the quarter, at flank speed about 0.65 m, lower in the troughs, whitest at its foot and broken toward its
			// top.
			{
				const double Lambda = FMath::Max(2.0 * UE_DOUBLE_PI * Ship.Speed * Ship.Speed / 9.81, 20.0);
				const double Tall = 0.2 + 0.6 * Strength;
				const int32 WallSteps = 16;
				const int32 WallFirst = FV.Num();
				for (int32 K = 0; K <= WallSteps; ++K)
				{
					const double Share = static_cast<double>(K) / WallSteps;
					const double X = FMath::Lerp(0.05, 0.99, Share) * Long;
					const double Crest = 0.5 + 0.5 * FMath::Cos(2.0 * UE_DOUBLE_PI * (X - 0.05 * Long) / Lambda);
					const double Wall = Tall * (0.35 + 0.65 * Crest) * (Share < 0.1 ? FMath::Lerp(1.5, 1.0, Share / 0.1) : 1.0);
					const FVector2D Foot = Stem - Bow * X + Right * (Hand * (WaterHalf(X) + 0.25));
					const double U = X / Beam;
					const double Much = 0.8 * Made * (0.45 + 0.55 * Crest) * FMath::Clamp((1.0 - Share) / 0.08, 0.0, 1.0);
					Foamy(Foot, 0.1, FVector2D(U, 0.0), 1.0, 0.95, Much, false);
					Foamy(Foot + Right * (Hand * 0.1 * Wall), 0.1 + 0.6 * Wall, FVector2D(U, 0.8 * Wall / Beam), 1.0, 0.75, 0.75 * Much, false);
					Foamy(Foot + Right * (Hand * 0.2 * Wall), 0.1 + Wall, FVector2D(U, 1.4 * Wall / Beam), 1.0, 0.5, 0.0, false);
				}
				for (int32 K = 0; K < WallSteps; ++K)
				{
					const int32 A = WallFirst + K * 3, B = A + 3;
					Quad(FT, FV, A, A + 1, B + 1, B, true);
					Quad(FT, FV, A + 1, A + 2, B + 2, B + 1, true);
				}
			}
			// The arm of her wave, faint: from her stem, close along her side and then away at about 20 degrees.
			TArray<FStation> Arm;
			const FVector2D Points[3] = {
				Stem,
				Stem - Bow * (Long * 0.45) + Right * (Hand * Beam * 0.62),
				Stem - Bow * (Long * 1.15) + Right * (Hand * (Beam * 0.5 + Long * 0.3)),
			};
			const double Halves[3] = { 1.5, 3.5, 7.0 }, Muches[3] = { 0.35, 0.25, 0.0 };
			for (int32 K = 0; K < 3; ++K)
			{
				FStation St;
				St.Mid = Points[K];
				St.Half = Halves[K];
				St.Beam = Beam;
				St.U = K * 0.5 * Long / Beam;
				for (int32 C = 0; C < 5; ++C)
				{
					St.Whole[C] = 0.5;
					St.Much[C] = C == 0 || C == 4 ? 0.0 : Muches[K] * Made;
				}
				Arm.Add(St);
			}
			Ribbon(Arm, ArmAcross, false);
		}
		// The boil at her stern: her screws' white water heaped up a metre or two just astern of her, highest a little
		// aft of her stern and settling over two fifths of her length into the wake behind.
		{
			const double Heap = FMath::Min(0.5 + 1.8 * Strength * Strength, 2.6);
			const double Run = WakeRun(Long) / Long;
			const double Along0 = Wakes.Contains(Ship.Key) && Wakes[Ship.Key].Num() > 0 ? Wakes[Ship.Key].Last().S : 0.0;
			TArray<FStation> Boil;
			for (int32 K = 0; K <= 6; ++K)
			{
				const double Ls = 0.4 * K / 6.0;
				const double Bump = FMath::Max(0.0, FMath::Sin(UE_DOUBLE_PI * (Ls + 0.06) / 0.46));
				FStation St;
				St.Mid = Stern - Bow * (Ls * Long);
				St.Half = WakeHalf(Beam, Ls, Run) * 1.1;
				St.Beam = Beam;
				St.U = -(Along0 - Ls * Long) / Beam;
				const double Much = 0.95 * Made * FMath::Pow(1.0 - Ls / 0.4, 0.6);
				for (int32 C = 0; C < 5; ++C)
				{
					const double Across = ArmAcross[C];
					St.Z[C] = 0.35 + Heap * Bump * (1.0 - 0.85 * Across * Across);
					St.Whole[C] = C == 2 ? 1.0 : 0.85;
					St.Much[C] = C == 0 || C == 4 ? 0.0 : Much;
				}
				Boil.Add(St);
			}
			Ribbon(Boil, ArmAcross, false);
		}
	}

	// The torpedo tracks (the art director's ruling of 19 November, Samar r2e frame 07): no line from where each was
	// fired, but a streak of bubbles TrackStreak metres behind its (unseen) torpedo, brightest just behind the head,
	// widening three times and breaking up to nothing at its tail; an oxygen torpedo's a third as strong. A track whose
	// torpedo has struck or run out goes in TrackFade seconds.
	for (auto It = Tracks.CreateIterator(); It; ++It)
	{
		const FTrackLine& Line = It.Value();
		const double Left = Line.StopT < 0.0 ? 1.0 : 1.0 - (Now - Line.StopT) / TrackFade;
		if (Left <= 0.0)
		{
			It.RemoveCurrent();
			continue;
		}
		const double Run = FVector2D::Distance(Line.From, Line.Head);
		if (Run < 5.0)
		{
			continue;
		}
		const FVector2D Way = (Line.Head - Line.From) / Run;
		const double Streak = FMath::Min(TrackStreak, Run);
		const double Strength = 0.75 * Left * WakeStrength * (Line.bOxygen ? 1.0 / 3.0 : 1.0);
		const double Unit = 6.0; // (its "beam" for the ink's pattern: about a torpedo's bubbles across)
		TArray<FStation> Streak3;
		const int32 Steps = 12;
		for (int32 K = 0; K <= Steps; ++K)
		{
			const double Back = Streak * K / Steps; // (metres behind its head)
			const double Share = Back / Streak;
			const double Peak = Back < 8.0 ? FMath::Lerp(0.25, 1.0, Back / 8.0) : FMath::Pow(FMath::Max(0.0, 1.0 - (Back - 8.0) / (Streak - 8.0)), 1.4);
			FStation St;
			St.Mid = Line.Head - Way * Back;
			St.Half = 1.2 + 2.4 * Share;
			St.Beam = Unit;
			St.U = -(Run - Back) / Unit;
			const double Whole = FMath::Lerp(0.95, 0.25, Share);
			for (int32 C = 0; C < 5; ++C)
			{
				St.Whole[C] = C == 2 ? Whole : Whole * 0.6;
				St.Much[C] = C == 0 || C == 4 ? 0.0 : Strength * Peak;
			}
			Streak3.Add(St);
		}
		Ribbon(Streak3, WakeAcross, false);
	}

	// The men in the water near the eye (DrawWrecks draws them): a swimmer's arms and legs churn a ring of white water
	// about him, brighter at each stroke, and a man treading water a fainter one; a man just in from her side lands in a
	// splash some two metres across that settles in a second and a half. Each a ring with a hole for the man, a hand's
	// breadth over the sea (under his shoulders, over the water). The nearest MostFoamRings of them at the most (the
	// pre-check, 22 November: light-ships has 1,684 men afloat at once, and nothing else capped the rings).
	{
		using SeaShow::Swim;
		const FVector EyeBoard = BoardOf(EyeAt);
		const std::vector<SeaShow::ManOut>& Men = S.Show.Men();
		TArray<TPair<double, int32>> Ringed;
		for (int32 K = 0; K < static_cast<int32>(Men.size()); ++K)
		{
			const SeaShow::ManOut& Man = Men[K];
			if (Man.Doing == Swim::Jump)
			{
				MenJumping.Add(Man.Key);
				continue;
			}
			if (MenJumping.Contains(Man.Key))
			{
				MenJumping.Remove(Man.Key);
				MenLanded.Add(Man.Key, Now);
			}
			if (Man.Doing != Swim::Swim && Man.Doing != Swim::Tread && Man.Doing != Swim::Cling)
			{
				continue;
			}
			const double Dx = Man.At.X - EyeBoard.X, Dy = Man.At.Y - EyeBoard.Y;
			if (Dx * Dx + Dy * Dy <= 400.0 * 400.0)
			{
				Ringed.Add(TPair<double, int32>(Dx * Dx + Dy * Dy, K));
			}
		}
		if (Ringed.Num() > MostFoamRings)
		{
			Ringed.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
			Ringed.SetNum(MostFoamRings);
		}
		for (const TPair<double, int32>& One : Ringed)
		{
			const SeaShow::ManOut& Man = Men[One.Value];
			const double* Landed = MenLanded.Find(Man.Key);
			const double Since = Landed ? Now - *Landed : 100.0;
			const double Ph = Hash01(Man.Key * 0.731) * 2.0 * UE_DOUBLE_PI;
			double Inner = 0.32, Outer = 0.62, Much = 0.0;
			if (Since < 1.5)
			{
				Inner = 0.2 + 0.4 * Since;
				Outer = 0.7 + 0.9 * Since;
				Much = 0.95 * (1.0 - Since / 1.5);
			}
			else if (Man.Doing == Swim::Swim)
			{
				Much = 0.3 + 0.2 * FMath::Max(0.0, FMath::Sin(Now * 3.6 + Ph));
			}
			else
			{
				Much = 0.14;
			}
			if (Much <= 0.02)
			{
				continue;
			}
			const int32 Round = 8;
			const int32 First = FV.Num();
			const double Radii[3] = { Inner, 0.5 * (Inner + Outer), Outer }, Muches[3] = { 0.0, Much, 0.0 };
			for (int32 Ring = 0; Ring < 3; ++Ring)
			{
				for (int32 K = 0; K < Round; ++K)
				{
					const double Angle = 2.0 * UE_DOUBLE_PI * (K + 0.5 * Ring) / Round;
					const double R = Radii[Ring] * (0.85 + 0.3 * Hash01(Man.Key * 1.3 + K * 2.1 + Ring));
					const FVector2D P(Man.At.X + FMath::Sin(Angle) * R, Man.At.Y + FMath::Cos(Angle) * R);
					Foamy(P, 0.06, FVector2D(P.X * 0.5, P.Y * 0.5), 1.0, 0.7, Muches[Ring], false);
				}
			}
			for (int32 Ring = 0; Ring < 2; ++Ring)
			{
				for (int32 K = 0; K < Round; ++K)
				{
					const int32 A0 = First + Ring * Round + K, A1 = First + Ring * Round + (K + 1) % Round;
					Quad(FT, FV, A0, A1, A1 + Round, A0 + Round, false);
				}
			}
		}
	}

	// A ship broken in two (the art director, Samar r2e frame 08): the gap hidden in a boil of white water, heaviest in
	// the middle, a lighter ring where each half meets the sea, both going as she goes under. (Her oil and her wreckage
	// are every sinking's now: the slicks below, the floats in DrawWrecks.)
	TArray<FVector> DV, DN;
	TArray<FColor> DC;
	TArray<int32> DT;
	for (const TPair<int32, FBreakUp>& Pair : Breaks)
	{
		const FBreakUp& B = Pair.Value;
		const double Age = Now - B.T;
		if (Age < 0.0 || Age > 150.0)
		{
			continue;
		}
		const FVector2D Bow = Ahead(B.Heading), Right = Starboard(B.Heading);
		const double Going = 1.0 - FMath::Clamp((Age - 25.0) / 35.0, 0.0, 1.0);
		const int32 Round = 16;
		// (an ellipse round Middle, A along her and Bb across, in rings: each ring's share of the size and its paint)
		auto Disc = [&](const FVector2D& Middle, double A, double Bb, const TArray<double>& Rings, const TArray<double>& Muches, const TArray<double>& Wholes, double Heave, int32 Seed)
		{
			const int32 First = FV.Num();
			for (int32 Ring = 0; Ring < Rings.Num(); ++Ring)
			{
				for (int32 K = 0; K < Round; ++K)
				{
					const double Angle = 2.0 * UE_DOUBLE_PI * K / Round;
					const double Rag = Ring == Rings.Num() - 1 ? 0.8 + 0.4 * Hash01(Seed * 7.0 + K) : 1.0;
					const FVector2D P = Middle + Bow * (FMath::Cos(Angle) * A * Rings[Ring] * Rag) + Right * (FMath::Sin(Angle) * Bb * Rings[Ring] * Rag);
					Foamy(P, 0.3 + Heave * (1.0 - Rings[Ring]), FVector2D(FMath::Cos(Angle) * A * Rings[Ring] / B.Beam, FMath::Sin(Angle) * Bb * Rings[Ring] / B.Beam),
						1.0, Wholes[Ring], Muches[Ring], false);
				}
			}
			for (int32 Ring = 0; Ring + 1 < Rings.Num(); ++Ring)
			{
				for (int32 K = 0; K < Round; ++K)
				{
					const int32 A0 = First + Ring * Round + K, A1 = First + Ring * Round + (K + 1) % Round;
					Quad(FT, FV, A0, A1, A1 + Round, A0 + Round, false);
				}
			}
		};
		// The boil over the gap: a beam and a half across at once, three by twenty seconds, heaving a metre at first.
		const double Wide = B.Beam * (1.5 + 1.5 * FMath::Clamp(Age / 20.0, 0.0, 1.0));
		const double Boil = 0.95 * Going * FMath::Min(1.0, Age / 1.5 + 0.3);
		Disc(B.At, Wide * 1.2, Wide, { 0.05, 0.5, 1.0 }, { Boil, Boil * 0.85, 0.0 }, { 1.0, 0.8, 0.45 }, 1.0 * FMath::Max(0.0, 1.0 - Age / 30.0), Pair.Key);
		// The ring where each half meets the sea, about a quarter of her length either side of the break.
		for (const double End : { -1.0, 1.0 })
		{
			const FVector2D Middle = B.At + Bow * (End * B.Long * 0.27);
			const double Ring = 0.6 * Going;
			Disc(Middle, B.Long * 0.25, B.Beam * 0.9, { 0.75, 0.95, 1.15 }, { 0.0, Ring, 0.0 }, { 0.7, 0.6, 0.4 }, 0.0, Pair.Key * 3 + (End > 0.0 ? 1 : 2));
		}
	}
	// Every ship's oil where she went down (SeaShow's Slicks; Brian, 22 November 05:33Z: "maybe oil pools?"): fuel oil
	// dark on the water and a little see-through, thickest in the middle, a brown sheen where it thins at its ragged
	// edge, the edge in broad lobes and smaller tatters that change slowly, and drawn out in streaks down the wind. A
	// hand's breadth over the sea, so a swimmer's head and a raft stand out of it and a jacket is half in it. Where it
	// burns, the kit's oil fire is over it (the show's OilFire).
	for (const SeaShow::SlickOut& Oil : S.Show.Slicks())
	{
		if (Oil.Dark <= 0.01 || Oil.Long < 1.0)
		{
			continue;
		}
		const int32 Round = 32;
		static const double Rings[5] = { 0.0, 0.42, 0.72, 0.9, 1.0 };
		const double Alphas[5] = { Oil.Dark, Oil.Dark, Oil.Dark * 0.85, Oil.Dark * 0.45, 0.0 };
		static const FColor Paints[5] = { FColor(11, 10, 9), FColor(12, 11, 10), FColor(20, 17, 13), FColor(46, 38, 27), FColor(58, 50, 38) };
		const FVector2D Along(FMath::Sin(Oil.Heading), FMath::Cos(Oil.Heading)), Across(Along.Y, -Along.X);
		const FVector2D Middle(Oil.At.X, Oil.At.Y);
		const double Drift = Oil.Age * 0.004; // (its lobes wander round as it ages)
		const int32 First = DV.Num();
		for (int32 Ring = 0; Ring < 5; ++Ring)
		{
			for (int32 K = 0; K < Round; ++K)
			{
				const double Angle = 2.0 * UE_DOUBLE_PI * K / Round;
				// (seven broad lobes round it, eased from one to the next, and a tatter at each point; the edge rings the
				// most ragged, the middle not at all; fingers down the wind)
				const double Lobes = 7.0 * K / Round + Drift;
				const int32 L0 = FMath::FloorToInt32(Lobes);
				const double Ease = 0.5 - 0.5 * FMath::Cos(UE_DOUBLE_PI * (Lobes - L0));
				const double Lobe = FMath::Lerp(Hash01(Oil.Key * 7.0 + (L0 % 7)), Hash01(Oil.Key * 7.0 + ((L0 + 1) % 7)), Ease) - 0.5;
				const double Tatter = Hash01(Oil.Key * 31.0 + K * 3.3 + Ring) - 0.5;
				const double Edge = Rings[Ring] < 0.5 ? 0.0 : (Rings[Ring] - 0.4) / 0.6;
				const double Down = FMath::Max(0.0, FMath::Cos(Angle));
				const double Finger = 1.0 + 0.3 * Down * Down * Down * Hash01(Oil.Key * 13.0 + K * 1.7) * Edge;
				const double Rag = (1.0 + Edge * (0.32 * Lobe + 0.12 * Tatter)) * Finger;
				const FVector2D P = Middle + Along * (FMath::Cos(Angle) * 0.5 * Oil.Long * Rings[Ring] * Rag) + Across * (FMath::Sin(Angle) * 0.5 * Oil.Wide * Rings[Ring] * Rag);
				DV.Add(World(P.X, P.Y, 0.08));
				DN.Add(FVector::UpVector);
				FColor Paint = Paints[Ring];
				Paint.A = Byte(Alphas[Ring]);
				DC.Add(Paint);
			}
		}
		for (int32 Ring = 0; Ring < 4; ++Ring)
		{
			for (int32 K = 0; K < Round; ++K)
			{
				const int32 A0 = First + Ring * Round + K, A1 = First + Ring * Round + (K + 1) % Round;
				Quad(DT, DV, A0, A1, A1 + Round, A0 + Round, false);
			}
		}
	}

	if (Foam && bInk)
	{
		if (FV.Num() == 0)
		{
			Foam->ClearMeshSection(0);
		}
		else
		{
			Foam->CreateMeshSection(0, FV, FT, FN, FU, FC, TArray<FProcMeshTangent>(), false);
			Foam->SetMaterial(0, FoamInk);
		}
	}
	if (!Marks)
	{
		return;
	}
	// (the marks' mesh: the oil, and the foam too where there is no wake ink, the oil drawn first, under it)
	if (!bInk)
	{
		const int32 Base = DV.Num();
		DV.Append(FV);
		DN.Append(FN);
		DC.Append(FC);
		for (const int32 K : FT)
		{
			DT.Add(K + Base);
		}
	}
	if (DV.Num() == 0)
	{
		Marks->ClearMeshSection(0);
		return;
	}
	Marks->CreateMeshSection(0, DV, DT, DN, TArray<FVector2D>(), DC, TArray<FProcMeshTangent>(), false);
	if (MarkInk)
	{
		Marks->SetMaterial(0, MarkInk);
	}
}

void ASeaBattle::DrawWrecks()
{
	using namespace SeaParts;
	using SeaShow::Swim;
	FSeaBattleState& S = *State;
	if (!WreckNear || !WreckFar)
	{
		return;
	}
	const double Now = S.BattleT;
	// (a struck boat's crew on her casing and her boarding party, and her captor's whaler and its crew, are drawn as the men
	// in the water and their boats are: Show::Hands and Show::Whalers, kept apart from the show's counts of those; joined
	// only while there are any, so the usual frame copies nothing)
	const std::vector<SeaShow::FloatOut>& ShowFloats = S.Show.Floats();
	const std::vector<SeaShow::ManOut>& ShowMen = S.Show.Men();
	std::vector<SeaShow::FloatOut> FloatsJoined;
	std::vector<SeaShow::ManOut> MenJoined;
	if (!S.Show.Whalers().empty())
	{
		FloatsJoined.reserve(ShowFloats.size() + S.Show.Whalers().size());
		FloatsJoined.insert(FloatsJoined.end(), ShowFloats.begin(), ShowFloats.end());
		FloatsJoined.insert(FloatsJoined.end(), S.Show.Whalers().begin(), S.Show.Whalers().end());
	}
	if (!S.Show.Hands().empty())
	{
		MenJoined.reserve(ShowMen.size() + S.Show.Hands().size());
		MenJoined.insert(MenJoined.end(), ShowMen.begin(), ShowMen.end());
		MenJoined.insert(MenJoined.end(), S.Show.Hands().begin(), S.Show.Hands().end());
	}
	const std::vector<SeaShow::FloatOut>& Floats = FloatsJoined.empty() ? ShowFloats : FloatsJoined;
	const std::vector<SeaShow::ManOut>& Men = MenJoined.empty() ? ShowMen : MenJoined;
	const std::vector<SeaShow::SlickOut>& Slicks = S.Show.Slicks();
	const double Dt = FMath::Clamp(Now - WrecksDrawnAt, 0.0, 1.0);
	WrecksDrawnAt = Now;
	// (whether the men may wear their whites: the battle known to fall in summer in warm water)
	const bool bWarm = SeaSailors::SummerWhites(S.Rec.Board.bWhen, S.Rec.Board.Month, S.Rec.Board.Lat, S.Rec.Board.Lon);

	// Each wreck's navy and whether she was a merchant ship, while she is still to be seen going down (her rafts and
	// boats in her paint), else from her men.
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		if (Ship.Doing == SeaShow::Fate::Sinking && !WreckNavy.Contains(Ship.Key))
		{
			WreckNavy.Add(Ship.Key, KitNation(Ship.Nation));
			if (Ship.Class == "mv")
			{
				WreckMerchants.Add(Ship.Key);
			}
		}
	}
	// (a captor's whaler in his navy's paint: the whaler's ship is his, afloat)
	for (const SeaShow::FloatOut& Whaler : S.Show.Whalers())
	{
		const SeaShow::ShipOut* Captor = WreckNavy.Contains(Whaler.Ship) ? nullptr : S.Show.Ship(Whaler.Ship);
		if (Captor)
		{
			WreckNavy.Add(Whaler.Ship, KitNation(Captor->Nation));
		}
	}
	// A man who swims in her oil comes out of it black: each man's share of it grows while he is in a slick that is still
	// thick (to four fifths: his jacket and his hat keep a little of their colour, so he still reads on the oil), and
	// stays.
	for (const SeaShow::ManOut& Man : Men)
	{
		if (Man.Doing == Swim::Jump || Man.Doing == Swim::Climb || Man.Doing == Swim::Boat || Man.Doing == Swim::Raft || OnHerCasing(Man.Doing))
		{
			continue;
		}
		if (!WreckNavy.Contains(Man.Ship))
		{
			WreckNavy.Add(Man.Ship, KitNation(Man.Nation));
		}
		for (const SeaShow::SlickOut& Oil : Slicks)
		{
			if (Oil.Dark < 0.15)
			{
				continue;
			}
			const double Dx = Man.At.X - Oil.At.X, Dy = Man.At.Y - Oil.At.Y;
			const double U = Dx * FMath::Sin(Oil.Heading) + Dy * FMath::Cos(Oil.Heading), V = Dx * FMath::Cos(Oil.Heading) - Dy * FMath::Sin(Oil.Heading);
			const double A = FMath::Max(Oil.Long * 0.5, 1.0), B = FMath::Max(Oil.Wide * 0.5, 1.0);
			if (U * U / (A * A) + V * V / (B * B) < 1.0)
			{
				float* Share = OilOnMen.Find(Man.Key);
				if (!Share)
				{
					Share = &OilOnMen.Add(Man.Key, 0.0f);
				}
				*Share = FMath::Min(0.8f, *Share + static_cast<float>(Dt * 0.15 * Oil.Dark));
				break;
			}
		}
	}
	// How big a thing looks: pixels a metre at a distance, by the eye's field and the window's width.
	int32 ViewW = 1920, ViewH = 1080;
	if (UWorld* Here = GetWorld())
	{
		if (APlayerController* Controller = Here->GetFirstPlayerController())
		{
			Controller->GetViewportSize(ViewW, ViewH);
		}
	}
	const double PerRadian = FMath::Max(ViewW, 320) / (2.0 * FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(EyeLens, 5.0, 120.0) * 0.5)));
	const FVector EyeBoard = BoardOf(EyeAt);
	auto DistanceTo = [&](double X, double Y, double Z) { return FMath::Max(1.0, FMath::Sqrt(FMath::Square(X - EyeBoard.X) + FMath::Square(Y - EyeBoard.Y) + FMath::Square(Z - EyeBoard.Z))); };

	// Two meshes: what is near the eye (LiveReach), every man going over the side or up a net and the boats, laid every
	// frame and throwing shadows; the rest laid twice a second and at a cut, as it barely moves at that distance. A thing
	// new since the far mesh was laid is laid with the near until the next.
	const bool bFarDue = TicksSinceCut == 0 || Now - WrecksFarAt >= WreckFarEvery || Now < WrecksFarAt;
	if (bFarDue)
	{
		WrecksFarAt = Now;
		FarFloats.Reset();
		FarMen.Reset();
	}
	FWreckMesh NearMesh, FarMesh;
	NearMesh.Units = FarMesh.Units = UnitsPerMetre;
	// (the loops of each boat's grab lines that her holders have hold of, drawn pulled out to their hands: the show puts
	// each holder by the loop he holds, SeaShow::GrabHold, and his place in her frame says which, SeaShow::GrabHeld)
	TMap<int32, uint64> HeldLoops;
	{
		TMap<int32, int32> BoatAt;
		for (int32 K = 0; K < static_cast<int32>(Floats.size()); ++K)
		{
			if (Floats[K].Kind == "boat")
			{
				BoatAt.Add(Floats[K].Key, K);
			}
		}
		for (const SeaShow::ManOut& Holder : Men)
		{
			const int32* Boat = Holder.Doing == SeaShow::Swim::Cling ? BoatAt.Find(Holder.On) : nullptr;
			if (!Boat)
			{
				continue;
			}
			const SeaShow::FloatOut& Hers = Floats[*Boat];
			const double Dx = Holder.At.X - Hers.At.X, Dy = Holder.At.Y - Hers.At.Y;
			const double Along = Dx * FMath::Sin(Hers.Heading) + Dy * FMath::Cos(Hers.Heading);
			const double Across = Dx * FMath::Cos(Hers.Heading) - Dy * FMath::Sin(Hers.Heading);
			double Hand = 1.0;
			const int32 Loop = SeaShow::GrabHeld(Hers.Long, Along, Across, Hand);
			if (Loop >= 0 && Loop < 32)
			{
				HeldLoops.FindOrAdd(Hers.Key) |= static_cast<uint64>(1) << (Loop + (Hand > 0.0 ? 0 : 32));
			}
		}
	}
	TMap<int32, int32> FloatAt;
	for (int32 K = 0; K < static_cast<int32>(Floats.size()); ++K)
	{
		const SeaShow::FloatOut& F = Floats[K];
		FloatAt.Add(F.Key, K);
		const uint64* Held = HeldLoops.Find(F.Key);
		const FString* Navy = WreckNavy.Find(F.Ship);
		const FString Paint = Navy ? *Navy : FString();
		const bool bMerchant = WreckMerchants.Contains(F.Ship);
		const double D = DistanceTo(F.At.X, F.At.Y, 0.0);
		if (F.Long * PerRadian / D < 0.4)
		{
			continue; // (under half a pixel)
		}
		const bool bFine = 2.0 * SeaShow::GrabThick * PerRadian / D >= 0.25;
		if (D < LiveReach || F.Kind == "boat")
		{
			DrawFloat(NearMesh, F, Paint, bMerchant, Now, bFine, Held ? *Held : 0);
		}
		else if (bFarDue)
		{
			DrawFloat(FarMesh, F, Paint, bMerchant, Now, bFine, Held ? *Held : 0);
			FarFloats.Add(F.Key);
		}
		else if (!FarFloats.Contains(F.Key))
		{
			DrawFloat(NearMesh, F, Paint, bMerchant, Now, bFine, Held ? *Held : 0);
		}
	}
	// (the men drawn whole: those 7 pixels across or more, the nearest MostDetailedMen of them where there are more)
	double DetailFrom = 7.0;
	{
		TArray<double> Sizes;
		for (const SeaShow::ManOut& Man : Men)
		{
			const double Pixels = 0.45 * PerRadian / DistanceTo(Man.At.X, Man.At.Y, Man.At.Z);
			if (Pixels >= DetailFrom)
			{
				Sizes.Add(Pixels);
			}
		}
		if (Sizes.Num() > MostDetailedMen)
		{
			Sizes.Sort([](double A, double B) { return A > B; });
			DetailFrom = Sizes[MostDetailedMen - 1];
		}
	}
	// The men drawn whole as the sailor files' men where those are read (Battle/SeaSailors; the stand-ins where not, and
	// while a man's bank mesh waits to be made): the nearest LiveMost skinned afresh each frame, each a mesh of his own put
	// where he is; the rest as copies of the pose bank's meshes. A live mesh or a banked copy not drawn this frame is put
	// away after.
	const bool bFiles = S.Crew.Files() > 0 && WreckInk != nullptr;
	// A gun's crew at it (Battle/DeckCrewCore; the review of 23 November: no one served the gun; Brian, 24 November: the
	// files' own gun clips): each armed ship's watch stepped every frame from what the show says of her, near or far, drawn
	// or hidden, so her crew are where they should be as she comes into sight and her loader keeps time with her rounds.
	// Its men are drawn where the show draws her (none on a boat it hides) and the stage her gun (GunDeck: not as she goes
	// down), within LiveReach of the eye, on her deck as the stage lays her and her gun, as the men in the water are (the
	// nearest skinned live, the rest the pose bank's copies, below); their work's cues heard at their hands ("body": only
	// zoomed in; silent where the sound is not brought in). They are not the show's men, and are keyed apart, below 0.
	struct FGunManned
	{
		int32 ShipKey = 0;
		int32 CrewFile = -1, CrewDress = 0;
		const FSeaBattleState::FGunOf* Mounted = nullptr;
		const DeckCrew::Blocks* Kit = nullptr;
		DeckCrew::Told Said;
		// (a boat's hatch open while her crew are up, the passer in it where he hands up: on her deck, her turn there, and
		// the way to it from the gun; the loader's rounds from a ready-use rack at his left where no passer hands them up)
		bool bHatchOpen = false, bRack = false;
		FTransform Hatch = FTransform::Identity;
		FVector2D HatchAway = FVector2D(1.0, 0.0);
	};
	struct FGunHand
	{
		int32 Mine = 0;  // his key (below 0)
		int32 GunAt = 0; // his gun's place in GunsManned
		int32 Who = 0;   // his role (DeckCrew::Role)
		bool bAtGun = false;
		DeckCrew::Shown Is;
		FVector Feet = FVector::ZeroVector;
		FQuat Turn = FQuat::Identity;
		double Pixels = 0.0;
	};
	std::vector<FGunManned> GunsManned;
	std::vector<FGunHand> GunHands;
	if (Stage && S.Crew.Files() > 0)
	{
		std::vector<DeckCrew::Cue> GunCues;
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			if (Ship.Class != "ss" && Ship.Class != "mv")
			{
				continue;
			}
			const bool bAftMount = Ship.Class == "mv" && Ship.bAftGun;
			const int32 MountKey = Ship.Key * 2 + (bAftMount ? 1 : 0);
			auto Known = S.GunsOf.find(MountKey);
			if (Known == S.GunsOf.end())
			{
				FString ModelFile;
				FVector ModelStretch = FVector::OneVector;
				if (!Stage->ShipModel(Ship.Key, ModelFile, ModelStretch) || ModelFile.IsEmpty())
				{
					continue;
				}
				FSeaBattleState::FGunOf Found;
				Found.Mount = ConvoyLook::MountOf(std::string(TCHAR_TO_UTF8(*FPaths::GetCleanFilename(ModelFile))), bAftMount);
				Found.Shape = DeckCrew::ShapeOf(Found.Mount);
				Found.Part = UTF8_TO_TCHAR(Found.Mount.Part.c_str());
				Known = S.GunsOf.emplace(MountKey, Found).first;
			}
			const FSeaBattleState::FGunOf& Mounted = Known->second;
			const int32 CrewFile = Mounted.Shape.File.empty() ? -1 : S.Crew.FileFor(Ship.Nation);
			if (CrewFile < 0)
			{
				continue;
			}
			auto Kit = S.GunBlocks.find(CrewFile);
			if (Kit == S.GunBlocks.end())
			{
				Kit = S.GunBlocks.emplace(CrewFile, DeckCrew::BlocksOf(S.Crew, CrewFile)).first;
			}
			if (!Kit->second.bGun)
			{
				continue;
			}
			const DeckCrew::Told Said = DeckCrew::TellOf(Ship);
			DeckCrew::Watch& Crewed = S.GunWatches[Ship.Key];
			GunCues.clear();
			DeckCrew::Step(Crewed, Said, Mounted.Shape, Mounted.Mount, Kit->second, Now, &GunCues);
			if (!bFiles || !Crewed.bManned || !Ship.bShown || DistanceTo(Ship.At.X, Ship.At.Y, 0.0) > LiveReach + Ship.Long * 0.5)
			{
				continue;
			}
			FGunManned Manned;
			Manned.ShipKey = Ship.Key;
			Manned.CrewFile = CrewFile;
			Manned.CrewDress = DressFor(S, Ship.Nation, CrewFile, 0);
			Manned.Mounted = &Mounted;
			Manned.Kit = &Kit->second;
			Manned.Said = Said;
			Manned.bRack = !Crewed.bHanding;
			const int32 GunAt = static_cast<int32>(GunsManned.size());
			FVector CueAt[DeckCrew::Roles] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };
			bool bCueAt[DeckCrew::Roles] = { false, false, false, false };
			for (int32 Who = 0; Who < DeckCrew::Roles; ++Who)
			{
				const DeckCrew::Shown& Is = Crewed.Men[Who];
				if (!DeckCrew::Drawn(Is, Ship))
				{
					continue;
				}
				FVector FeetAt = FVector::ZeroVector;
				FQuat HerTurn = FQuat::Identity;
				if (!Stage->GunDeck(Ship.Key, Mounted.Part, Is.At.Root.Fore, Is.At.Root.Right, Is.At.Root.Up, FeetAt, HerTurn))
				{
					break;
				}
				const FVector BoardAt = BoardOf(FeetAt);
				const double Seen = DistanceTo(BoardAt.X, BoardAt.Y, BoardAt.Z);
				const double HandPixels = 0.45 * PerRadian / Seen;
				if (Seen > LiveReach || HandPixels < 0.5)
				{
					continue;
				}
				// (his own feet on her deck, his own turn hers and then his facing: she heels and pitches, and he with her)
				FGunHand Gunner;
				Gunner.Mine = -(Ship.Key * DeckCrew::Roles + Who + 1);
				Gunner.GunAt = GunAt;
				Gunner.Who = Who;
				Gunner.bAtGun = Crewed.Hands[Who].Now == DeckCrew::Phase::AtGun;
				Gunner.Is = Is;
				Gunner.Feet = FeetAt;
				Gunner.Turn = HerTurn * FQuat(FVector::UpVector, FMath::DegreesToRadians(Is.At.Facing));
				Gunner.Pixels = HandPixels;
				GunHands.push_back(Gunner);
				// (her hatch open while a man of hers is up: a boat's only, a merchant has none by her gun)
				Manned.bHatchOpen = Mounted.Shape.bBoat;
				CueAt[Who] = FeetAt + Gunner.Turn.RotateVector(FVector(0.4, 0.0, 1.0) * UnitsPerMetre);
				bCueAt[Who] = true;
			}
			const DeckCrew::GunShape& Fitted = Mounted.Shape;
			if (Fitted.bBoat && Manned.bHatchOpen)
			{
				// (her own hatch, where her crew come up and the passer stands: the one opening drawn, on her middle line)
				const DeckCrew::Spot HatchSpot = DeckCrew::HatchOf(Fitted, Mounted.Mount);
				FVector HatchWorld = FVector::ZeroVector;
				FQuat HatchTurn = FQuat::Identity;
				Manned.bHatchOpen = Stage->GunDeck(Ship.Key, Mounted.Part, HatchSpot.Fore, HatchSpot.Right, HatchSpot.Up, HatchWorld, HatchTurn);
				Manned.Hatch = FTransform(HatchTurn, HatchWorld);
				Manned.HatchAway = FVector2D(HatchSpot.Fore >= 0.0 ? 1.0 : -1.0, 0.0);
			}
			for (const DeckCrew::Cue& Heard : GunCues)
			{
				const int32 Whose = static_cast<int32>(Heard.Who);
				if (Whose >= 0 && Whose < DeckCrew::Roles && bCueAt[Whose])
				{
					Sound(FName(UTF8_TO_TCHAR(Heard.Name.c_str())), CueAt[Whose], TEXT("body"), 1.0f, false);
				}
			}
			GunsManned.push_back(Manned);
		}
	}
	TSet<int32> LiveKeys, SlotsDrawn;
	if (bFiles)
	{
		TArray<TPair<double, int32>> Nearest;
		for (const SeaShow::ManOut& Man : Men)
		{
			const double Pixels = 0.45 * PerRadian / DistanceTo(Man.At.X, Man.At.Y, Man.At.Z);
			if (Pixels >= FMath::Max(LiveLeast, DetailFrom))
			{
				Nearest.Add(TPair<double, int32>(Pixels, Man.Key));
			}
		}
		// (and a gun's crew, as near: keyed below 0)
		for (const FGunHand& Gunner : GunHands)
		{
			if (Gunner.Pixels >= FMath::Max(LiveLeast, DetailFrom))
			{
				Nearest.Add(TPair<double, int32>(Gunner.Pixels, Gunner.Mine));
			}
		}
		Nearest.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key > B.Key; });
		for (int32 K = 0; K < Nearest.Num() && K < LiveMost; ++K)
		{
			LiveKeys.Add(Nearest[K].Value);
		}
		for (int32 Slot = 0; Slot < SailorLiveOf.Num(); ++Slot)
		{
			if (SailorLiveOf[Slot].Man != 0 && !LiveKeys.Contains(SailorLiveOf[Slot].Man))
			{
				SailorLiveOf[Slot].Man = 0;
			}
		}
		for (TPair<int32, FBankedSailor>& One : SailorBanked)
		{
			One.Value.bSeen = false;
		}
	}
	int32 Builds = TicksSinceCut == 0 ? SailorBuildsAtCut : SailorBuilds, LiveNew = LiveNewMost;
	static const FColor OarWood(150, 128, 92);
	// (an oar: its loom from its handle out through its rowlock, its blade the last 0.55 m, flat to the water)
	auto DrawOar = [&NearMesh](const FVector& From, const FVector& Tip)
	{
		NearMesh.Rod(From, Tip, 0.03, 4, OarWood);
		const FVector Flat = (Tip - From).GetSafeNormal();
		const FVector Side = (Flat ^ FVector::UpVector).GetSafeNormal() * 0.08;
		NearMesh.Quad(Tip - Flat * 0.55 - Side, Tip - Side, Tip + Side, Tip - Flat * 0.55 + Side, FVector::UpVector, OarWood);
	};
	// (each boat's coxswain: the man of her farthest row aft, where that is aft of her oarsmen (her rows 0 and 1 of twelve),
	// the lower numbered of its two; he sits at her tiller facing forward. By row, not by place: the two of a row sit level
	// with each other, and a hair's difference in their places as she turns must not change which of them steers. The
	// stern sheets are his: the other men of those rows sit forward instead, on her boards in places no man of hers has,
	// from her row next to her bow aft to her oarsmen (her row at her very bow, where she is narrowest, last), the lowest
	// numbered man first)
	struct FCoxOf
	{
		int32 Row = 0;
		int32 Key = 0;
		uint32 Taken = 0; // her places (row * 2, and one more on her starboard side) with a man in them
		TArray<int32> Others;
	};
	auto BoatRow = [](const SeaShow::FloatOut& F, const SeaShow::ManOut& Man)
	{
		const double Along = (Man.At.X - F.At.X) * FMath::Sin(F.Heading) + (Man.At.Y - F.At.Y) * FMath::Cos(F.Heading);
		return FMath::RoundToInt32((Along / F.Long + 0.38) / 0.76 * 11.0);
	};
	auto BoatSide = [](const SeaShow::FloatOut& F, const SeaShow::ManOut& Man)
	{
		return (Man.At.X - F.At.X) * FMath::Cos(F.Heading) - (Man.At.Y - F.At.Y) * FMath::Sin(F.Heading) >= 0.0 ? 1 : 0;
	};
	TMap<int32, FCoxOf> Coxswains;
	for (const SeaShow::ManOut& Man : Men)
	{
		const int32* Index = Man.Doing == Swim::Boat && Man.On >= 0 ? FloatAt.Find(Man.On) : nullptr;
		if (!Index)
		{
			continue;
		}
		const SeaShow::FloatOut& F = Floats[*Index];
		const int32 Row = BoatRow(F, Man);
		FCoxOf& Cox = Coxswains.FindOrAdd(F.Key);
		if (Cox.Taken == 0 || Row < Cox.Row || (Row == Cox.Row && Man.Key < Cox.Key))
		{
			Cox.Row = Row;
			Cox.Key = Man.Key;
		}
		if (Row >= 0 && Row <= 11)
		{
			Cox.Taken |= 1u << (Row * 2 + BoatSide(F, Man));
		}
	}
	for (const SeaShow::ManOut& Man : Men)
	{
		const int32* Index = Man.Doing == Swim::Boat && Man.On >= 0 ? FloatAt.Find(Man.On) : nullptr;
		FCoxOf* Cox = Index ? Coxswains.Find(Floats[*Index].Key) : nullptr;
		if (Cox && Cox->Row <= 1 && Cox->Key != Man.Key && BoatRow(Floats[*Index], Man) <= 1)
		{
			Cox->Others.Add(Man.Key);
		}
	}
	TMap<int32, int32> SeatedForward; // a man of her stern rows other than her coxswain: the place (row * 2 + side) he sits in
	static const int32 Places[] = { 20, 21, 18, 19, 16, 17, 14, 15, 22, 23 };
	const int32 PlaceCount = UE_ARRAY_COUNT(Places);
	for (TPair<int32, FCoxOf>& Boat : Coxswains)
	{
		Boat.Value.Others.Sort();
		int32 Next = 0;
		for (const int32 Key : Boat.Value.Others)
		{
			while (Next < PlaceCount && (Boat.Value.Taken & (1u << Places[Next])))
			{
				++Next;
			}
			if (Next >= PlaceCount)
			{
				break;
			}
			SeatedForward.Add(Key, Places[Next]);
			Boat.Value.Taken |= 1u << Places[Next];
		}
	}
	int32 Detailed = 0;
	for (const SeaShow::ManOut& Man : Men)
	{
		const double D = DistanceTo(Man.At.X, Man.At.Y, Man.At.Z);
		const double Pixels = 0.45 * PerRadian / D;
		if (Pixels < 0.5)
		{
			continue;
		}
		// (a struck boat's crew on her casing moves with her every frame, near or far, as a man going over does)
		const bool bLive = D < LiveReach || Pixels >= DetailFrom || Man.Doing == Swim::Jump || Man.Doing == Swim::Climb || Man.Doing == Swim::Boat || OnHerCasing(Man.Doing);
		const bool bInFar = !bLive && bFarDue;
		if (!bLive && !bFarDue && FarMen.Contains(Man.Key))
		{
			continue;
		}
		FSailorPaint Paint = SailorPaint(KitNation(Man.Nation), Man.Look, bWarm);
		const float* OilShare = OilOnMen.Find(Man.Key);
		if (OilShare)
		{
			Paint = SeaParts::Oiled(Paint, *OilShare);
		}
		// (on a float: its ride on the swell; in a boat, whether he is one of her ten oarsmen, on his thwart, pulling while
		// she is under way and resting on his oar while not; the stand-in's oar)
		double Lift = 0.0, Pull = 0.0;
		FVector Handle = FVector::ZeroVector, StandInTip = FVector::ZeroVector;
		bool bRowing = false;
		SeaSailors::Aboard Where;
		FWreckFrame BoatFrame;
		double BoatU = 0.0, BoatAlong = 0.0, BoatAcross = 0.0, BoatLong = 0.0, BoatBeam = 0.0;
		bool bOarsman = false, bInBoat = false, bCoxswain = false;
		double SeatAlong = 0.0, SeatAcross = 0.0;
		if ((Man.Doing == Swim::Raft || Man.Doing == Swim::Boat || Man.Doing == Swim::Cling) && Man.On >= 0)
		{
			if (const int32* Index = FloatAt.Find(Man.On))
			{
				const SeaShow::FloatOut& F = Floats[*Index];
				const FVector Ride = FloatRideOf(F, Now);
				Lift = Ride.X;
				Where.On = &F;
				Where.Ride = Lift;
				if (Man.Doing == Swim::Cling)
				{
					// (holding on at her side: her rise there, her bow lifting and her right side going down with the swell as
					// the scene draws her, so his hands stay on her line or her tube)
					const FWreckFrame Tilted = WreckFrame(FVector(F.At.X, F.At.Y, Ride.X), F.Heading, Ride.Y, Ride.Z);
					const FWreckFrame Untilted = WreckFrame(FVector(F.At.X, F.At.Y, Ride.X), F.Heading);
					const FVector Off(Man.At.X - F.At.X, Man.At.Y - F.At.Y, 0.0);
					Where.Ride = Tilted.At(FVector::DotProduct(Off, Untilted.X), FVector::DotProduct(Off, Untilted.Y), 0.0).Z;
				}
				if (Man.Doing == Swim::Boat)
				{
					// (under way: her own way through the water under oars, the show's; a boat only drifting down the wind
					// is not rowed)
					const bool bUnderWay = F.Way > 0.0;
					const FWreckFrame Fr = WreckFrame(FVector(F.At.X, F.At.Y, Ride.X), F.Heading);
					const FVector Off(Man.At.X - F.At.X, Man.At.Y - F.At.Y, 0.0);
					const double Along = FVector::DotProduct(Off, Fr.X), Across = FVector::DotProduct(Off, Fr.Y);
					const double U = FMath::Clamp(Along / (0.5 * F.Long), -0.95, 0.95);
					const int32 Row = BoatRow(F, Man);
					bInBoat = true;
					if (const FCoxOf* Cox = Coxswains.Find(F.Key))
					{
						bCoxswain = Cox->Row <= 1 && Cox->Key == Man.Key;
					}
					SeatAlong = Along;
					SeatAcross = Across;
					if (const int32* Place = SeatedForward.Find(Man.Key))
					{
						SeatAlong = (-0.38 + 0.76 * (*Place / 2) / 11.0) * F.Long;
						SeatAcross = (*Place % 2 ? 0.25 : -0.25) * F.Beam;
					}
					BoatFrame = Fr;
					BoatU = U;
					BoatAlong = Along;
					BoatAcross = Across;
					BoatLong = F.Long;
					BoatBeam = F.Beam;
					if (Row >= 2 && Row <= 6)
					{
						bOarsman = true;
						Where.bRowing = true;
						Where.bStarboard = Across >= 0.0;
						// (her crew pull together, on her own stroke; at rest, his hands at his chest, the blade clear)
						Where.Stroke = static_cast<float>(bUnderWay ? Now + Hash01(F.Key * 0.37) * 2.4 : 1.15);
					}
					if (Row >= 2 && Row <= 6 && bUnderWay)
					{
						bRowing = true;
						const double Hand = Across >= 0.0 ? 1.0 : -1.0;
						Pull = Now * 2.4 + Hash01(F.Key * 0.37) * 6.0;
						const FVector Lock = Fr.At(Along, Hand * BoatHalf(U, F.Beam), BoatSheer(U) + 0.04);
						// (the oar swept fore and aft about square to her side: at the catch, Pull a quarter round, he leans
						// toward her stern and the blade is forward; through the pull, cos Pull below nought, the blade is in
						// the water and goes aft as he leans back; coming forward it is clear of the water)
						const double Sweep = 0.5 * FMath::Sin(Pull);
						const FVector Out = (Fr.Y * (Hand * FMath::Cos(Sweep)) + Fr.X * FMath::Sin(Sweep)).GetSafeNormal();
						const double Blade = -0.47 + 0.15 * FMath::Cos(Pull);
						StandInTip = Lock + Out * 2.6 + FVector(0.0, 0.0, Blade);
						Handle = Lock - Out * 0.75 + FVector(0.0, 0.0, 0.08);
					}
				}
			}
		}
		const bool bDetail = Pixels >= DetailFrom && Detailed < MostDetailedMen && (bLive || !bFarDue);
		Detailed += bDetail ? 1 : 0;
		bool bByFile = false;
		const int32 File = bDetail && bFiles ? S.Crew.FileFor(Man.Nation) : -1;
		if (File >= 0)
		{
			const double* Landed = MenLanded.Find(Man.Key);
			SeaSailors::Placed P = S.Crew.Place(File, Man, Where, Now, Landed ? *Landed : -1.0);
			if (P.What == SeaSailors::Act::Row && bOarsman)
			{
				// (across her so his rowlock is on her gunwale)
				const double Want = (BoatAcross >= 0.0 ? 1.0 : -1.0) * FMath::Max(0.1, BoatHalf(BoatU, BoatBeam) - SeaSailors::LockOut);
				const FVector Shift = BoatFrame.Y * (Want - BoatAcross);
				P.X += Shift.X;
				P.Y += Shift.Y;
			}
			else if (bCoxswain && P.What != SeaSailors::Act::Row)
			{
				// (her coxswain on her boards at her tiller, on her middle line facing forward, his boots toward her oarsmen)
				const FVector Shift = BoatFrame.X * (-0.5 * BoatLong + CoxAt - BoatAlong) + BoatFrame.Y * (-BoatAcross);
				P.X += Shift.X;
				P.Y += Shift.Y;
				P.Heading = FMath::UnwindRadians(P.Heading + UE_DOUBLE_PI);
			}
			else if (bInBoat && P.What != SeaSailors::Act::Row)
			{
				// (one sitting on her boards, facing aft, his knees up: his back 0.27 m forward of his hips, his boots 0.59 m
				// aft of them, his elbows and knees 0.32 m to either side; all of him inside her sides and her ends, where she
				// narrows toward them; a man of her stern rows other than her coxswain, in his place forward)
				const double Half = 0.5 * BoatLong;
				const double Along = FMath::Clamp(SeatAlong, -Half + 1.05, Half - 0.7);
				const double Room = FMath::Min(BoatHalf((Along + 0.27) / Half, BoatBeam), BoatHalf((Along - 0.59) / Half, BoatBeam)) - 0.34;
				const double Across = FMath::Clamp(SeatAcross, -FMath::Max(0.0, Room), FMath::Max(0.0, Room));
				const FVector Shift = BoatFrame.X * (Along - BoatAlong) + BoatFrame.Y * (Across - BoatAcross);
				P.X += Shift.X;
				P.Y += Shift.Y;
			}
			// (about one man in seven in his file's other dress: an officer's or chief's khaki, the later grey-green)
			const int32 Dress = DressFor(S, Man.Nation, File, ((Man.Look >> 16) & 255u) < 38u ? 1 : 0);
			const bool bOiled = OilShare && *OilShare >= 0.4f;
			const FVector Feet = World(P.X, P.Y, P.Z);
			const FRotator Turn(0.0, FMath::RadiansToDegrees(P.Heading), 0.0);
			float Shown = P.Time;
			if (LiveKeys.Contains(Man.Key))
			{
				int32 Slot = INDEX_NONE;
				for (int32 K = 0; K < SailorLiveOf.Num() && Slot == INDEX_NONE; ++K)
				{
					Slot = SailorLiveOf[K].Man == Man.Key ? K : INDEX_NONE;
				}
				for (int32 K = 0; K < SailorLiveOf.Num() && Slot == INDEX_NONE; ++K)
				{
					Slot = SailorLiveOf[K].Man == 0 && !SlotsDrawn.Contains(K) ? K : INDEX_NONE;
				}
				if (Slot == INDEX_NONE && SailorLive.Num() < LiveMost)
				{
					UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, FName(*FString::Printf(TEXT("SeaSailorLive%d"), SailorLive.Num())));
					Mesh->SetupAttachment(Root);
					Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Mesh->SetCastShadow(true);
					Mesh->bUseAsyncCooking = false;
					Mesh->RegisterComponent();
					Slot = SailorLive.Add(Mesh);
					SailorLiveOf.AddDefaulted();
				}
				const LandMen::Lod Level = Pixels >= NearLodFrom ? LandMen::Near : LandMen::Mid;
				const int64 Made = SailorKey(File, Dress, 0, 0, static_cast<int32>(Level), P.bMirror, bOiled);
				const bool bAfresh = Slot == INDEX_NONE || SailorLiveOf[Slot].Man != Man.Key || SailorLiveOf[Slot].Made != Made;
				if (Slot != INDEX_NONE && (!bAfresh || LiveNew > 0))
				{
					FLiveSailor& Shows = SailorLiveOf[Slot];
					UProceduralMeshComponent* Mesh = SailorLive[Slot];
					std::vector<float> At, Facing;
					const int32 Count = S.Crew.Skin(File, P.What, P.Time, Level, At, Facing);
					const double Hand = P.bMirror ? -1.0 : 1.0;
					TArray<FVector> Points, Normals;
					Points.Reset(Count);
					Normals.Reset(Count);
					for (int32 I = 0; I < Count; ++I)
					{
						const size_t K = static_cast<size_t>(I) * 3;
						Points.Add(FVector(At[K] * UnitsPerMetre, Hand * At[K + 1] * UnitsPerMetre, At[K + 2] * UnitsPerMetre));
						Normals.Add(FVector(Facing[K], Hand * Facing[K + 1], Facing[K + 2]));
					}
					if (bAfresh)
					{
						// (a new man or a new level, dress or hand: his triangles and colours with his points)
						--LiveNew;
						const std::vector<uint32_t>& Tris = S.Crew.Triangles(File, Level);
						TArray<int32> Corners;
						Corners.Reset(static_cast<int32>(Tris.size()));
						for (size_t K = 0; K + 2 < Tris.size(); K += 3)
						{
							Corners.Append({ static_cast<int32>(Tris[K]), static_cast<int32>(Tris[P.bMirror ? K + 2 : K + 1]), static_cast<int32>(Tris[P.bMirror ? K + 1 : K + 2]) });
						}
						std::vector<uint8_t> Rgb;
						S.Crew.Paint(File, Level, S.Dresses[static_cast<size_t>(Dress)], bOiled ? 0.8 : 0.0, Rgb);
						TArray<FColor> Colours;
						Colours.Reset(Count);
						for (int32 I = 0; I < Count; ++I)
						{
							const size_t K = static_cast<size_t>(I) * 3;
							Colours.Add(FColor(Rgb[K], Rgb[K + 1], Rgb[K + 2], 255));
						}
						Mesh->ClearAllMeshSections();
						Mesh->CreateMeshSection(0, Points, Corners, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
						Mesh->SetMaterial(0, WreckInk);
						Shows.Man = Man.Key;
						Shows.Made = Made;
					}
					else
					{
						Mesh->UpdateMeshSection(0, Points, Normals, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>());
					}
					Mesh->SetRelativeLocationAndRotation(Feet, Turn);
					Mesh->SetVisibility(true);
					SlotsDrawn.Add(Slot);
					bByFile = true;
				}
			}
			if (!bByFile)
			{
				const int32 Level = Pixels >= MidLodFrom ? LandMen::Mid : LandMen::Far;
				const int32 Pose = S.Crew.PoseOf(File, P.What, P.Time);
				const int64 Key = SailorKey(File, Dress, static_cast<int32>(P.What), Pose, Level, P.bMirror, bOiled);
				int32 Copy = SailorCopy(Key, File, Dress, static_cast<int32>(P.What), Pose, Level, P.bMirror, bOiled, Builds);
				int32 ShownPose = Pose;
				if (Copy == -1)
				{
					// (this frame's builds spent, as after a cut: the nearest pose of his act already made, at his level or
					// the other, rather than a stand-in)
					const int32 Poses = SeaSailors::PosesOf(P.What);
					const bool bLoop = P.What != SeaSailors::Act::Enter;
					for (int32 Step = 1; Copy < 0 && Step < Poses; ++Step)
					{
						for (const int32 Sign : { -1, 1 })
						{
							int32 Other = Pose + Sign * Step;
							Other = bLoop ? ((Other % Poses) + Poses) % Poses : Other;
							for (const int32 Lv : { Level, Level == LandMen::Mid ? static_cast<int32>(LandMen::Far) : static_cast<int32>(LandMen::Mid) })
							{
								const int32* Made = Copy < 0 && Other >= 0 && Other < Poses ? SailorCopyOf.Find(SailorKey(File, Dress, static_cast<int32>(P.What), Other, Lv, P.bMirror, bOiled)) : nullptr;
								if (Made && *Made >= 0)
								{
									Copy = *Made;
									ShownPose = Other;
								}
							}
						}
					}
					if (Copy < 0)
					{
						const int32 OtherLevel = Level == LandMen::Mid ? LandMen::Far : LandMen::Mid;
						const int32* Made = SailorCopyOf.Find(SailorKey(File, Dress, static_cast<int32>(P.What), Pose, OtherLevel, P.bMirror, bOiled));
						Copy = Made && *Made >= 0 ? *Made : Copy;
					}
				}
				if (Copy >= 0)
				{
					Shown = S.Crew.PoseTime(File, P.What, ShownPose);
					FBankedSailor* Had = SailorBanked.Find(Man.Key);
					if (!Had)
					{
						Had = &SailorBanked.Add(Man.Key, FBankedSailor());
					}
					if (Had->Copy != Copy)
					{
						// (into the copies of his new pose: a slot put away there before, or a new one)
						SailorAway(*Had);
						TArray<int32>& Free = SailorFree[Copy];
						const FTransform Put(Turn, Feet);
						if (Free.Num() > 0)
						{
							Had->Slot = Free.Pop();
							SailorCopies[Copy]->UpdateInstanceTransform(Had->Slot, Put, false, false, true);
						}
						else
						{
							Had->Slot = SailorCopies[Copy]->AddInstance(Put, false);
						}
						Had->Copy = Copy;
						Had->At = Feet;
						Had->Yaw = Turn.Yaw;
						SailorDirty[Copy] = 1;
					}
					else if (FVector::DistSquared(Had->At, Feet) > 1.0 || FMath::Abs(Had->Yaw - Turn.Yaw) > 0.2)
					{
						// (moved a centimetre or more, or turned: put where he is now)
						SailorCopies[Copy]->UpdateInstanceTransform(Had->Slot, FTransform(Turn, Feet), false, false, true);
						Had->At = Feet;
						Had->Yaw = Turn.Yaw;
						SailorDirty[Copy] = 1;
					}
					Had->bSeen = true;
					bByFile = true;
				}
			}
			if (bByFile && P.What == SeaSailors::Act::Row)
			{
				// (his oar: from his hands' track through the clip's rowlock, out to its blade at the water)
				LandMen::Vec3 Grip, Lock;
				if (S.Crew.Oar(File, Shown, P.bMirror, Grip, Lock))
				{
					const FVector Fwd(FMath::Sin(P.Heading), FMath::Cos(P.Heading), 0.0), Rt(FMath::Cos(P.Heading), -FMath::Sin(P.Heading), 0.0);
					const FVector Root3(P.X, P.Y, P.Z);
					const FVector GripAt = Root3 + Fwd * Grip.X + Rt * Grip.Y + FVector(0.0, 0.0, Grip.Z);
					const FVector LockAt = Root3 + Fwd * Lock.X + Rt * Lock.Y + FVector(0.0, 0.0, Lock.Z);
					const FVector Way = (LockAt - GripAt).GetSafeNormal();
					DrawOar(GripAt - Way * 0.12, LockAt + Way * OarOutboard);
				}
			}
		}
		if (!bByFile)
		{
			if (bRowing)
			{
				DrawOar(Handle, StandInTip);
			}
			DrawSailor(bInFar ? FarMesh : NearMesh, Man, Paint, bDetail, Now, Lift, bRowing ? &Handle : nullptr, Pull);
			if (bInFar)
			{
				FarMen.Add(Man.Key);
			}
		}
	}
	// A gun's crew at it (gathered above: GunHands, at their guns GunsManned): each man drawn as the men in the water are
	// (the nearest skinned live; the rest copies of the pose bank's men, the nearest pose already made where this frame's
	// builds are spent; no stand-in), as his transform lays him on her deck; and what he works with, in his own axes at
	// the time in his clip he is drawn at: the layer's and the trainer's handwheels at their hands, turning as they turn
	// them, and their sights at their eyes; the round in the loader's hands (slid home along the gun's own bore as he rams
	// it) and in the passer's; a boat's hatch open while her crew are up, and the passer's scuttle while he is in it. Each
	// round is drawn while its man works, as nothing while it is not in his hands, so the near mesh keeps its shape.
	if (bFiles)
	{
		const int32 LayerRole = static_cast<int32>(DeckCrew::Role::Layer), TrainerRole = static_cast<int32>(DeckCrew::Role::Trainer);
		const int32 LoaderRole = static_cast<int32>(DeckCrew::Role::Loader), PasserRole = static_cast<int32>(DeckCrew::Role::Passer);
		for (const FGunHand& Gunner : GunHands)
		{
			const FGunManned& Manned = GunsManned[static_cast<size_t>(Gunner.GunAt)];
			const DeckCrew::Shown& Is = Gunner.Is;
			const int32 CrewFile = Manned.CrewFile, CrewDress = Manned.CrewDress;
			bool bDrawnByFile = false;
			float ShownTime = Is.Time;
			if (LiveKeys.Contains(Gunner.Mine))
			{
				int32 LiveSlot = INDEX_NONE;
				for (int32 Each = 0; Each < SailorLiveOf.Num() && LiveSlot == INDEX_NONE; ++Each)
				{
					LiveSlot = SailorLiveOf[Each].Man == Gunner.Mine ? Each : INDEX_NONE;
				}
				for (int32 Each = 0; Each < SailorLiveOf.Num() && LiveSlot == INDEX_NONE; ++Each)
				{
					LiveSlot = SailorLiveOf[Each].Man == 0 && !SlotsDrawn.Contains(Each) ? Each : INDEX_NONE;
				}
				if (LiveSlot == INDEX_NONE && SailorLive.Num() < LiveMost)
				{
					UProceduralMeshComponent* LiveMesh = NewObject<UProceduralMeshComponent>(this, FName(*FString::Printf(TEXT("SeaSailorLive%d"), SailorLive.Num())));
					LiveMesh->SetupAttachment(Root);
					LiveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					LiveMesh->SetCastShadow(true);
					LiveMesh->bUseAsyncCooking = false;
					LiveMesh->RegisterComponent();
					LiveSlot = SailorLive.Add(LiveMesh);
					SailorLiveOf.AddDefaulted();
				}
				const LandMen::Lod LiveLevel = Gunner.Pixels >= NearLodFrom ? LandMen::Near : LandMen::Mid;
				const int64 LiveMade = SailorKey(CrewFile, CrewDress, 0, 0, static_cast<int32>(LiveLevel), false, false);
				const bool bLiveAfresh = LiveSlot == INDEX_NONE || SailorLiveOf[LiveSlot].Man != Gunner.Mine || SailorLiveOf[LiveSlot].Made != LiveMade;
				if (LiveSlot != INDEX_NONE && (!bLiveAfresh || LiveNew > 0))
				{
					FLiveSailor& LiveShows = SailorLiveOf[LiveSlot];
					UProceduralMeshComponent* LiveMesh = SailorLive[LiveSlot];
					std::vector<float> SkinAt, SkinFacing;
					const int32 SkinCount = S.Crew.Skin(CrewFile, Is.What, Is.Time, LiveLevel, SkinAt, SkinFacing);
					TArray<FVector> LivePoints, LiveNormals;
					LivePoints.Reset(SkinCount);
					LiveNormals.Reset(SkinCount);
					for (int32 Vert = 0; Vert < SkinCount; ++Vert)
					{
						const size_t At3 = static_cast<size_t>(Vert) * 3;
						LivePoints.Add(FVector(SkinAt[At3], SkinAt[At3 + 1], SkinAt[At3 + 2]) * UnitsPerMetre);
						LiveNormals.Add(FVector(SkinFacing[At3], SkinFacing[At3 + 1], SkinFacing[At3 + 2]));
					}
					if (bLiveAfresh)
					{
						// (a new man or a new level: his triangles and colours with his points)
						--LiveNew;
						const std::vector<uint32_t>& LiveTris = S.Crew.Triangles(CrewFile, LiveLevel);
						TArray<int32> LiveCorners;
						LiveCorners.Reset(static_cast<int32>(LiveTris.size()));
						for (const uint32_t Corner : LiveTris)
						{
							LiveCorners.Add(static_cast<int32>(Corner));
						}
						std::vector<uint8_t> LiveRgb;
						S.Crew.Paint(CrewFile, LiveLevel, S.Dresses[static_cast<size_t>(CrewDress)], 0.0, LiveRgb);
						TArray<FColor> LiveColours;
						LiveColours.Reset(SkinCount);
						for (int32 Vert = 0; Vert < SkinCount; ++Vert)
						{
							const size_t At3 = static_cast<size_t>(Vert) * 3;
							LiveColours.Add(FColor(LiveRgb[At3], LiveRgb[At3 + 1], LiveRgb[At3 + 2], 255));
						}
						LiveMesh->ClearAllMeshSections();
						LiveMesh->CreateMeshSection(0, LivePoints, LiveCorners, LiveNormals, TArray<FVector2D>(), LiveColours, TArray<FProcMeshTangent>(), false);
						LiveMesh->SetMaterial(0, WreckInk);
						LiveShows.Man = Gunner.Mine;
						LiveShows.Made = LiveMade;
					}
					else
					{
						LiveMesh->UpdateMeshSection(0, LivePoints, LiveNormals, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>());
					}
					LiveMesh->SetRelativeLocationAndRotation(Gunner.Feet, Gunner.Turn);
					LiveMesh->SetVisibility(true);
					SlotsDrawn.Add(LiveSlot);
					bDrawnByFile = true;
				}
			}
			if (!bDrawnByFile)
			{
				const int32 WhatIndex = static_cast<int32>(Is.What);
				const int32 BankLevel = Gunner.Pixels >= MidLodFrom ? LandMen::Mid : LandMen::Far;
				const int32 BankOtherLevel = BankLevel == LandMen::Mid ? static_cast<int32>(LandMen::Far) : static_cast<int32>(LandMen::Mid);
				const int32 BankPose = S.Crew.PoseOf(CrewFile, Is.What, Is.Time);
				int32 BankCopy = SailorCopy(SailorKey(CrewFile, CrewDress, WhatIndex, BankPose, BankLevel, false, false), CrewFile, CrewDress, WhatIndex, BankPose, BankLevel, false, false, Builds);
				int32 BankShown = BankPose;
				if (BankCopy == -1)
				{
					// (this frame's builds spent, as after a cut: the nearest pose of his act already made, at his level or
					// the other)
					const int32 BankPoses = SeaSailors::PosesOf(Is.What);
					for (int32 Apart = 0; BankCopy < 0 && Apart < BankPoses; ++Apart)
					{
						for (const int32 BankWay : { -1, 1 })
						{
							const int32 NextPose = ((BankPose + BankWay * Apart) % BankPoses + BankPoses) % BankPoses;
							for (const int32 BankLv : { BankLevel, BankOtherLevel })
							{
								const int32* MadeCopy = BankCopy < 0 ? SailorCopyOf.Find(SailorKey(CrewFile, CrewDress, WhatIndex, NextPose, BankLv, false, false)) : nullptr;
								if (MadeCopy && *MadeCopy >= 0)
								{
									BankCopy = *MadeCopy;
									BankShown = NextPose;
								}
							}
						}
					}
				}
				if (BankCopy >= 0)
				{
					ShownTime = S.Crew.PoseTime(CrewFile, Is.What, BankShown);
					const FTransform BankPut(Gunner.Turn, Gunner.Feet);
					FBankedSailor* BankHad = SailorBanked.Find(Gunner.Mine);
					if (!BankHad)
					{
						BankHad = &SailorBanked.Add(Gunner.Mine, FBankedSailor());
					}
					if (BankHad->Copy != BankCopy)
					{
						// (into the copies of his new pose: a slot put away there before, or a new one)
						SailorAway(*BankHad);
						TArray<int32>& BankFree = SailorFree[BankCopy];
						if (BankFree.Num() > 0)
						{
							BankHad->Slot = BankFree.Pop();
							SailorCopies[BankCopy]->UpdateInstanceTransform(BankHad->Slot, BankPut, false, false, true);
						}
						else
						{
							BankHad->Slot = SailorCopies[BankCopy]->AddInstance(BankPut, false);
						}
						BankHad->Copy = BankCopy;
					}
					else
					{
						// (she heels and pitches under him, and the gun trains: put where he is every frame)
						SailorCopies[BankCopy]->UpdateInstanceTransform(BankHad->Slot, BankPut, false, false, true);
					}
					BankHad->At = Gunner.Feet;
					BankHad->Yaw = BankPut.Rotator().Yaw;
					BankHad->bSeen = true;
					SailorDirty[BankCopy] = 1;
					bDrawnByFile = true;
				}
			}
			// (what he works with: only where he is drawn, and big enough to see it, a wheel a pixel or more across)
			if (!bDrawnByFile || Gunner.Pixels < DetailFrom)
			{
				continue;
			}
			const FTransform ManAt(Gunner.Turn, Gunner.Feet);
			const DeckCrew::GunShape& Fitted = Manned.Mounted->Shape;
			const DeckCrew::Blocks& GunKit = *Manned.Kit;
			if ((Gunner.Who == LayerRole || Gunner.Who == TrainerRole) && Gunner.bAtGun)
			{
				// (the layer at the gun's left, it on his right; the trainer at its right; the wheel held still while he
				// crouches)
				const bool bLayer = Gunner.Who == LayerRole;
				const SeaSailors::Wheel& Handwheel = GunKit.Wheels[bLayer ? 0 : 1];
				const float Turned = Is.bAtWork ? SeaSailors::TurnAt(Handwheel, GunKit.Long[Gunner.Who], ShownTime) : 0.f;
				DrawGunWheel(NearMesh, ManAt, UnitsPerMetre, Handwheel, Turned, Fitted.ShaftAhead, Fitted.ShaftIn, bLayer);
				DrawGunSight(NearMesh, ManAt, UnitsPerMetre, Handwheel, Fitted.ShaftAhead, Fitted.ShaftIn, bLayer);
			}
			else if (Gunner.Who == LoaderRole && Is.bAtWork)
			{
				// (the gun's chamber's mouth and its bore's way in his own axes, as the stage trains and lifts it)
				double BoreWay[3] = { 1.0, 0.0, 0.0 };
				const DeckCrew::Spot MouthHer = DeckCrew::BreechOf(Fitted, GunKit, Manned.Mounted->Mount, Manned.Said.Train, Manned.Said.Elevation, BoreWay);
				const DeckCrew::Spot MouthHis = DeckCrew::ToHim(Is.At, MouthHer);
				DeckCrew::Post FacingOnly = Is.At;
				FacingOnly.Root = DeckCrew::Spot();
				const DeckCrew::Spot BoreHis = DeckCrew::ToHim(FacingOnly, DeckCrew::Spot{ BoreWay[0], BoreWay[1], BoreWay[2] });
				const LandMen::Vec3 MouthAt = { static_cast<float>(MouthHis.Fore), static_cast<float>(MouthHis.Right), static_cast<float>(MouthHis.Up) };
				const LandMen::Vec3 BoreAt = { static_cast<float>(BoreHis.Fore), static_cast<float>(BoreHis.Right), static_cast<float>(BoreHis.Up) };
				SeaSailors::RoundAt InHands;
				if (!DeckCrew::LoaderRound(GunKit, ShownTime, Fitted.bBoat, MouthAt, BoreAt, InHands))
				{
					InHands.bShown = false;
				}
				DrawGunRound(NearMesh, ManAt, UnitsPerMetre, InHands, GunKit.Loading.Long, GunKit.Loading.Radius);
				// (no passer handing it up: the next round waiting on a ready-use rack at his left until he takes it, where
				// the passer's hands would hold it; drawn as nothing once he has it, so the mesh keeps its shape)
				SeaSailors::RoundAt Racked;
				if (!Manned.bRack || !DeckCrew::RackRound(GunKit, ShownTime, Racked))
				{
					Racked.bShown = false;
				}
				DrawGunRound(NearMesh, ManAt, UnitsPerMetre, Racked, GunKit.Loading.Long, GunKit.Loading.Radius);
				if (Manned.bRack)
				{
					DrawGunRack(NearMesh, ManAt, UnitsPerMetre, GunKit);
				}
			}
			else if (Gunner.Who == PasserRole && Is.bAtWork)
			{
				SeaSailors::RoundAt Handed;
				if (!SeaSailors::RoundAlong(GunKit.Passing, GunKit.Long[PasserRole], ShownTime, Handed))
				{
					Handed.bShown = false;
				}
				DrawGunRound(NearMesh, ManAt, UnitsPerMetre, Handed, GunKit.Passing.Long, GunKit.Passing.Radius);
			}
		}
		for (const FGunManned& Manned : GunsManned)
		{
			if (Manned.bHatchOpen && Manned.Mounted != nullptr && Manned.Mounted->Shape.bBoat)
			{
				DrawGunHatch(NearMesh, Manned.Hatch, UnitsPerMetre, Manned.HatchAway);
			}
		}
	}
	if (bFiles)
	{
		for (int32 Slot = 0; Slot < SailorLive.Num(); ++Slot)
		{
			if (!SlotsDrawn.Contains(Slot))
			{
				SailorLive[Slot]->SetVisibility(false);
				SailorLiveOf[Slot].Man = 0;
			}
		}
		TArray<int32> Gone;
		for (TPair<int32, FBankedSailor>& One : SailorBanked)
		{
			if (!One.Value.bSeen)
			{
				SailorAway(One.Value);
				Gone.Add(One.Key);
			}
		}
		for (const int32 Key : Gone)
		{
			SailorBanked.Remove(Key);
		}
		for (int32 Copy = 0; Copy < SailorCopies.Num(); ++Copy)
		{
			if (SailorDirty[Copy])
			{
				// (a copy whose every slot is put away hidden, no draw of its shrunken men; shown again as one is used: the
				// review of 24 November, the gun crews' poses passed through a copy at a time)
				const bool bAnyIn = SailorFree[Copy].Num() < SailorCopies[Copy]->GetInstanceCount();
				if (SailorCopies[Copy]->IsVisible() != bAnyIn)
				{
					SailorCopies[Copy]->SetVisibility(bAnyIn);
				}
				SailorCopies[Copy]->MarkRenderStateDirty();
				SailorDirty[Copy] = 0;
			}
		}
	}
	WrecksMostDrawn = FMath::Max(WrecksMostDrawn, NearMesh.V.Num() + (bFarDue ? FarMesh.V.Num() : 0));

	// (the near mesh made afresh only when its shape changed: while the same points and triangles hold, only where they
	// are, which way they face and their paint are sent, the pre-check's note of 22 November on its cost every frame)
	if (NearMesh.V.Num() == 0)
	{
		WreckNear->ClearMeshSection(0);
		WreckNearShape.Reset();
	}
	else if (NearMesh.T.Num() == WreckNearShape.Num() - 1 && WreckNearShape.Num() > 0 && WreckNearShape[0] == NearMesh.V.Num()
		&& FMemory::Memcmp(NearMesh.T.GetData(), WreckNearShape.GetData() + 1, sizeof(int32) * NearMesh.T.Num()) == 0)
	{
		WreckNear->UpdateMeshSection(0, NearMesh.V, NearMesh.N, TArray<FVector2D>(), NearMesh.C, TArray<FProcMeshTangent>());
	}
	else
	{
		WreckNear->CreateMeshSection(0, NearMesh.V, NearMesh.T, NearMesh.N, TArray<FVector2D>(), NearMesh.C, TArray<FProcMeshTangent>(), false);
		WreckNear->SetMaterial(0, WreckInk);
		WreckNearShape.Reset(NearMesh.T.Num() + 1);
		WreckNearShape.Add(NearMesh.V.Num());
		WreckNearShape.Append(NearMesh.T);
	}
	if (bFarDue)
	{
		if (FarMesh.V.Num() == 0)
		{
			WreckFar->ClearMeshSection(0);
		}
		else
		{
			WreckFar->CreateMeshSection(0, FarMesh.V, FarMesh.T, FarMesh.N, TArray<FVector2D>(), FarMesh.C, TArray<FProcMeshTangent>(), false);
			WreckFar->SetMaterial(0, WreckInk);
		}
	}
}

int32 ASeaBattle::SailorCopy(int64 Key, int32 File, int32 Dress, int32 What, int32 Pose, int32 Lod, bool bMirror, bool bOiled, int32& Builds)
{
	if (const int32* Found = SailorCopyOf.Find(Key))
	{
		return *Found;
	}
	if (Builds <= 0 || !State.IsValid() || !WreckInk || Dress < 0 || Dress >= static_cast<int32>(State->Dresses.size()))
	{
		return -1;
	}
	--Builds;
	FSeaBattleState& S = *State;
	const SeaSailors::Act A = static_cast<SeaSailors::Act>(What);
	const LandMen::Lod Level = static_cast<LandMen::Lod>(Lod);
	std::vector<float> At, Facing;
	std::vector<uint8_t> Rgb;
	S.Crew.Skin(File, A, S.Crew.PoseTime(File, A, Pose), Level, At, Facing);
	S.Crew.Paint(File, Level, S.Dresses[static_cast<size_t>(Dress)], bOiled ? 0.8 : 0.0, Rgb);
	const FString Name = FString::Printf(TEXT("SeaSailor_%lld"), static_cast<long long>(Key));
	UStaticMesh* Mesh = SeaParts::SailorMesh(this, FName(*(Name + TEXT("_Mesh"))), At, Facing, Rgb, S.Crew.Triangles(File, Level), bMirror, WreckInk, UnitsPerMetre);
	if (!Mesh)
	{
		SailorCopyOf.Add(Key, -2);
		return -2;
	}
	UInstancedStaticMeshComponent* Copies = NewObject<UInstancedStaticMeshComponent>(this, FName(*Name));
	Copies->SetupAttachment(Root);
	Copies->SetStaticMesh(Mesh);
	Copies->SetMaterial(0, WreckInk);
	Copies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Copies->SetCanEverAffectNavigation(false);
	// (the banked men are past the nearest few: no shadows, as the ground scene's bank)
	Copies->SetCastShadow(false);
	Copies->RegisterComponent();
	const int32 Index = SailorCopies.Add(Copies);
	SailorMeshes.Add(Mesh);
	SailorFree.AddDefaulted();
	SailorDirty.Add(0);
	SailorCopyOf.Add(Key, Index);
	return Index;
}

void ASeaBattle::SailorAway(FBankedSailor& Man)
{
	if (Man.Copy >= 0 && Man.Copy < SailorCopies.Num())
	{
		// (shrunk to nothing where he was, its slot kept for the next man into that pose)
		SailorCopies[Man.Copy]->UpdateInstanceTransform(Man.Slot, FTransform(FQuat::Identity, Man.At, FVector(1e-3)), false, false, true);
		SailorFree[Man.Copy].Add(Man.Slot);
		SailorDirty[Man.Copy] = 1;
	}
	Man.Copy = -1;
	Man.Slot = -1;
}

void ASeaBattle::RunTrails()
{
	FSeaBattleState& S = *State;
	FBattleEffects* Kit = Effects.Get();
	if (!Kit)
	{
		return;
	}
	const double Now = S.BattleT;
	// Every few seconds, the ships whose funnels smoke: those afloat and under her own steam nearest the eye (a burning
	// ship's own smoke stands for it). A plume a funnel on the nearest, one a ship on the next (2 and 1); a ship keeps
	// what she has while she stays a little inside the count, so plumes are not stopped and started as ships pass.
	// (chosen again at once when the camera cuts, and the ships the shot is of first of all: the hero destroyer of the
	// funnels picture went without, Samar r2e frame 01)
	if (bFunnels && (Now - FunnelsChosenAt >= 4.0 || ShotNow != FunnelsShot))
	{
		FunnelsChosenAt = Now;
		FunnelsShot = ShotNow;
		int32 Hero = -1, Second = -1;
		if (Shots.IsValidIndex(ShotNow))
		{
			Hero = Shots[ShotNow].Subject;
			Second = Shots[ShotNow].Kind == ESeaShot::Funnels ? Shots[ShotNow].Other : -1;
		}
		TArray<TPair<double, int32>> ByNear;
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			if (!Ship.bShown || Ship.bBurning || (Ship.Doing != SeaShow::Fate::Line && Ship.Doing != SeaShow::Fate::Limping))
			{
				continue;
			}
			const double Away = Ship.Key == Hero || Ship.Key == Second ? 0.0 : FVector::Dist(World(Ship.At.X, Ship.At.Y, 0.0), EyeAt) / UnitsPerMetre;
			if (Away < SeaParts::FunnelReach)
			{
				ByNear.Add(TPair<double, int32>(Away, Ship.Key));
			}
		}
		ByNear.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
		const TMap<int32, uint8> Had = MoveTemp(FunnelsChosen);
		FunnelsChosen.Reset();
		for (int32 K = 0; K < ByNear.Num(); ++K)
		{
			const uint8* Was = Had.Find(ByNear[K].Value);
			if (K < SeaParts::NearFunnels || (Was && *Was == 2 && K < SeaParts::NearFunnels + 1))
			{
				FunnelsChosen.Add(ByNear[K].Value, 2);
			}
			else if (K < SeaParts::FunnelShips || (Was && K < SeaParts::FunnelShips + 2))
			{
				FunnelsChosen.Add(ByNear[K].Value, 1);
			}
		}
	}
	TSet<int32> Fires, Funnels;
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		const double Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
		const FVector2D At(Ship.At.X, Ship.At.Y), Bow = SeaParts::Ahead(Ship.Heading);
		const FVector Velocity = WorldVector(Bow.X * Ship.Speed, Bow.Y * Ship.Speed, 0.0);
		// L4: a ship out of the fight burning as she limps away: flames on her deck, black smoke streaming off them.
		if (Ship.bBurning && Ship.Doing != SeaShow::Fate::Sinking)
		{
			const int32 Id = SeaParts::FireIds + Ship.Key;
			const FVector2D Seat = At + Bow * (Long * 0.08);
			const FVector Spot = World(Seat.X, Seat.Y, FMath::Min(Ship.Beam * 0.4, 8.0));
			if (FireTrails.Contains(Id))
			{
				Kit->TrailMove(Now, Id, Spot, Velocity);
			}
			else
			{
				Kit->TrailStart(Now, Id, Spot, Velocity, TEXT("fire"), static_cast<float>(Ship.Beam));
				FireTrails.Add(Id);
			}
			Fires.Add(Id);
		}
		// Her funnels' smoke (the kit's "funnel" kind): from each funnel's top as the stage finds it on her model, or,
		// a ship farther off, one plume from her after funnel as wide as all of hers; every frame, also when she lies
		// stopped (she smokes on, straight up). Power: her speed over her flank speed, as her wake has it; the kit
		// darkens it toward full power and gives a black surge when it rises fast.
		const uint8* Tier = FunnelsChosen.Find(Ship.Key);
		if (bFunnels && Tier && Ship.bShown && !Ship.bBurning && (Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping))
		{
			TArray<FVector4> Tops;
			if (!Stage || !Stage->FunnelTops(Ship.Key, Tops))
			{
				// (no funnels found on her model: one, where her looks put a funnel's steam)
				const FVector2D Stack = At + Bow * (Long * 0.03);
				const FVector Spot = World(Stack.X, Stack.Y, Ship.Beam * 0.9);
				Tops.Add(FVector4(Spot.X, Spot.Y, Spot.Z, FMath::Clamp(Ship.Beam * 0.28, 1.0, 14.0)));
			}
			const bool bOne = *Tier == 1 && Tops.Num() > 1;
			if (bOne)
			{
				// (her after funnel: the one farthest astern along her heading, in the world's axes)
				const FVector Ahead3 = WorldVector(Bow.X, Bow.Y, 0.0).GetSafeNormal();
				int32 Aft = 0;
				for (int32 K = 1; K < Tops.Num(); ++K)
				{
					if (FVector::DotProduct(FVector(Tops[K]), Ahead3) < FVector::DotProduct(FVector(Tops[Aft]), Ahead3))
					{
						Aft = K;
					}
				}
				const FVector4 After = Tops[Aft];
				const double Span = After.W * FMath::Sqrt(static_cast<double>(Tops.Num()));
				Tops.Reset();
				Tops.Add(FVector4(After.X, After.Y, After.Z, FMath::Min(Span, 14.0)));
			}
			const float Power = static_cast<float>(FMath::Clamp(Ship.Wake, 0.0, 1.0));
			for (int32 K = 0; K < Tops.Num() && K < 3; ++K)
			{
				const int32 Id = SeaParts::FunnelIds + Ship.Key * 4 + (bOne ? 3 : K);
				const FVector Spot(Tops[K].X, Tops[K].Y, Tops[K].Z);
				if (FunnelTrails.Contains(Id))
				{
					SeaParts::FunnelMove(Kit, Now, Id, Spot, Velocity, Power);
				}
				else
				{
					Kit->TrailStart(Now, Id, Spot, Velocity, TEXT("funnel"), static_cast<float>(Tops[K].W));
					FunnelTrails.Add(Id);
				}
				Funnels.Add(Id);
			}
		}
	}
	for (auto It = FireTrails.CreateIterator(); It; ++It)
	{
		if (!Fires.Contains(*It))
		{
			Kit->TrailStop(*It);
			It.RemoveCurrent();
		}
	}
	for (auto It = FunnelTrails.CreateIterator(); It; ++It)
	{
		if (!Funnels.Contains(*It))
		{
			Kit->TrailStop(*It);
			It.RemoveCurrent();
		}
	}
	// A ship broken in two (the art director, Samar r2e frame 08): white steam puffing from the gap, three to five
	// seconds each, for the first half minute, and black smoke pouring from it until her halves are under (the kit's
	// funnel smoke at its blackest: a surge at full power). Her boil, rings and oil are DrawMarks'.
	if (Stage)
	{
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			FVector Cut = FVector::ZeroVector;
			double When = 0.0;
			if (Ship.Doing != SeaShow::Fate::Sinking || Breaks.Contains(Ship.Key) || !Stage->BreakOf(Ship.Key, Cut, When))
			{
				continue;
			}
			FBreakUp& Made = Breaks.Add(Ship.Key);
			const FVector Board = BoardOf(Cut);
			Made.At = FVector2D(Board.X, Board.Y);
			Made.T = FMath::Min(When, Now);
			Made.Heading = Ship.Heading;
			Made.Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
			Made.Beam = Ship.Beam;
			Made.NextSteam = Made.T;
		}
	}
	for (TPair<int32, FBreakUp>& Pair : Breaks)
	{
		FBreakUp& B = Pair.Value;
		const double Age = Now - B.T;
		const FVector Gap = World(B.At.X, B.At.Y, 1.5);
		const int32 Id = SeaParts::BreakIds + Pair.Key;
		if (Age < 30.0 && Now >= B.NextSteam)
		{
			// (a new puff each time, under its own number: one number called again starts its puff over)
			const double Seconds = 3.0 + 2.0 * SeaParts::Hash01(Pair.Key * 5.0 + B.Puffs);
			SeaParts::SteamPuff(Kit, SeaParts::BreakSteamIds + Pair.Key * 8 + (B.Puffs % 8), Gap, FVector::UpVector, static_cast<float>(B.Beam * 0.45), static_cast<float>(Seconds));
			++B.Puffs;
			B.NextSteam = Now + Seconds * 0.8;
		}
		if (Age < 40.0)
		{
			if (B.bSmoke)
			{
				SeaParts::FunnelMove(Kit, Now, Id, Gap, FVector::ZeroVector, 1.0f);
			}
			else if (bFunnels)
			{
				// (from a fifth of full power, so the rise to full on the next frame reads as the kit's black surge)
				Kit->TrailStart(Now, Id, Gap, FVector::ZeroVector, TEXT("funnel"), static_cast<float>(FMath::Clamp(B.Beam * 0.5, 1.0, 14.0)));
				SeaParts::FunnelMove(Kit, Now, Id, Gap, FVector::ZeroVector, 0.2f);
				B.bSmoke = true;
				BreakTrails.Add(Id);
			}
		}
		else if (B.bSmoke)
		{
			Kit->TrailStop(Id);
			BreakTrails.Remove(Id);
			B.bSmoke = false;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// The camera: the action's moments, found by playing it through once, and where the eye stands for each.

void ASeaBattle::PlanShots()
{
	FSeaBattleState& S = *State;
	Shots.Reset();
	// (the same show as the scene's: the same seed, the same views, handed over the same way)
	SeaShow::Show Trial(S.Rec.Seed ? S.Rec.Seed : 239);
	Trial.SetField(S.Rec.Board);
	// (the player's sight of the enemy's boats as the scene's: a boat not yet found is no shot's subject)
	Trial.SetMine(S.Show.MineSide());
	const double Mid = S.Rec.Board.Size * 0.5;
	Trial.Eye = SeaShow::Vec(Mid, Mid - 3000.0, 900.0);
	// U1 hook (under water): the director weighs this same trial for the moments to go under the sea (Battle/SeaUnderCore),
	// whom it may go under beside the trial's own (SetMine above: ShipOut::bShown, as the surface draws)
	SeaUnder::Director UnderPlan;
	UnderPlan.Begin(S.Rec.Board, Under ? Under->UnheldClear() : SeaUnder::UnheldClearBy());
	size_t Given = 0;
	auto TakeNext = [&Trial, &S, &Given, &UnderPlan]()
	{
		const SeaFeed::Frame& F = S.Rec.Views[Given++];
		Trial.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
		UnderPlan.Told(F.Head.T, F.Pieces, F.Events); // U1 hook (under water): its events and the escorts' echoes (the pings)
	};
	TakeNext();
	FShot Fleets, Line, Broadside, Fall, Struck, Run, Strike, Down, Burning, Funnels;
	// (the fall of shot: the salvos of 180 mm and up lately fired, and the columns they threw about each ship)
	struct FSalvoSeen
	{
		double T = 0.0, Size = 0.0;
		int32 Side = -1;
		FVector2D Aim = FVector2D::ZeroVector;
	};
	struct FColumnSeen
	{
		double T = 0.0;
		int32 On = -1;
	};
	TArray<FSalvoSeen> SalvosSeen;
	TArray<FColumnSeen> ColumnsSeen;
	for (FShot* Each : { &Fleets, &Line, &Broadside, &Fall, &Struck, &Run, &Strike, &Down, &Burning, &Funnels })
	{
		Each->At = -1.0;
	}
	Fleets.Kind = ESeaShot::Fleets;
	Line.Kind = ESeaShot::Line;
	Broadside.Kind = ESeaShot::Broadside;
	Fall.Kind = ESeaShot::Rangefinder;
	Struck.Kind = ESeaShot::Hit;
	Run.Kind = ESeaShot::Tracks;
	Strike.Kind = ESeaShot::Strike;
	Down.Kind = ESeaShot::Sinking;
	Burning.Kind = ESeaShot::Burning;
	Funnels.Kind = ESeaShot::Funnels;
	double Contact = -1.0;
	bool bBigGuns = false;
	auto Rank = [](const std::string& Class) { return Class == "bb" ? 4 : Class == "cv" ? 3 : Class == "ca" ? 2 : 1; };
	// THE WATER'S CHECK SHEET ("-TheaterSeaWaterSheet", in place of the battle's shots; the art director's fleet water
	// targets, 21 November 23:44Z): a minute and a half in, the open sea from 28 m and from 60 m, looking away from the
	// sun and toward it, with the ships 1 to 3 km off; then ten seconds at two pictures a second of the smallest ship and
	// the biggest lying nearest her, side by side, to read their roll and pitch from the masthead.
	static const bool bWaterSheet = FParse::Param(FCommandLine::Get(), TEXT("TheaterSeaWaterSheet"));
	const double SheetT = FMath::Min(S.BeginT + 90.0, FMath::Max(S.BeginT + 5.0, S.EndT - 30.0));
	FShot Ride;
	Ride.Kind = ESeaShot::Water;
	Ride.At = -1.0;
	// (both lines: from the guns' first exchange, where each side's ships stand each second, and whether both sides have
	// fired in the half minute before)
	struct FLineShip
	{
		int32 Key = -1, Side = -1;
		FVector2D P = FVector2D::ZeroVector;
		bool bLine = false, bBig = false;
	};
	struct FLinesSeen
	{
		double T = 0.0;
		bool bDuel = false;
		TArray<FLineShip> Ships;
	};
	TArray<FLinesSeen> LinesSeen;
	double LastSalvoAt[2] = { -1e9, -1e9 };
	int32 Step = 0;
	for (double T = S.BeginT; T <= S.EndT + 1e-6; T += 0.25, ++Step)
	{
		while (Given < S.Rec.Views.size() && S.Rec.Views[Given - 1].Head.T <= T + 1e-9)
		{
			TakeNext();
		}
		Trial.Advance(T);
		if (Contact < 0.0 && Trial.ContactAt() >= 0.0)
		{
			Contact = Trial.ContactAt();
		}
		if (bWaterSheet && Ride.At < 0.0 && T >= SheetT)
		{
			// (the smallest ship afloat in a line, and the biggest nearest her: a destroyer and a battleship, as near as the
			// battle has them)
			const SeaShow::ShipOut* Small = nullptr;
			for (const SeaShow::ShipOut& Ship : Trial.Ships())
			{
				if (Ship.Doing == SeaShow::Fate::Line && Ship.bShown && (!Small || Ship.Long < Small->Long))
				{
					Small = &Ship;
				}
			}
			const SeaShow::ShipOut* Big = nullptr;
			double Best = TNumericLimits<double>::Max();
			for (const SeaShow::ShipOut& Ship : Trial.Ships())
			{
				const double Away = Small ? FVector2D::Distance(FVector2D(Ship.At.X, Ship.At.Y), FVector2D(Small->At.X, Small->At.Y)) : 0.0;
				if (Small && Ship.Doing == SeaShow::Fate::Line && Ship.bShown && Ship.Long >= Small->Long + 40.0 && Away - Ship.Long * 10.0 < Best)
				{
					Best = Away - Ship.Long * 10.0;
					Big = &Ship;
				}
			}
			Ride.At = T;
			Ride.Subject = Small ? Small->Key : -1;
			Ride.Other = Big ? Big->Key : -1;
		}
		// The fleets: the attacker's biggest column, by its leader; a column led by a boat the player has not found is passed
		// over, and with none of the attacker's left, the defender's.
		if (Fleets.Subject < 0)
		{
			bool bKeptBack = false;
			for (int32 Pass = 0; Pass < 2 && Fleets.Subject < 0 && (Pass == 0 || bKeptBack); ++Pass)
			{
				int32 Best = 0;
				for (const SeaShow::ShipOut& Ship : Trial.Ships())
				{
					if (Ship.Side == Pass && Ship.Slot == 0 && Rank(Ship.Class) > Best)
					{
						if (!Ship.bShown)
						{
							bKeptBack = true;
							continue;
						}
						Best = Rank(Ship.Class);
						Fleets.Subject = Ship.Key;
						Fleets.Other = Ship.Piece;
					}
				}
			}
		}
		// Their funnels (the art director, 21 November 19:45Z): a big ship and a destroyer of one side both under way at
		// 20 knots or more, within a kilometre of each other, side on; the first, from twenty seconds in.
		if (Funnels.At < 0.0 && T >= S.BeginT + 20.0)
		{
			for (const SeaShow::ShipOut& Big : Trial.Ships())
			{
				if (Funnels.At >= 0.0)
				{
					break;
				}
				if (Big.Doing != SeaShow::Fate::Line || Big.Speed < 10.0 || Rank(Big.Class) < 2)
				{
					continue;
				}
				for (const SeaShow::ShipOut& Small : Trial.Ships())
				{
					if (Small.Doing != SeaShow::Fate::Line || Small.Speed < 10.0 || Small.Class != "dd" || Small.Side != Big.Side ||
					    FVector2D(Small.At.X - Big.At.X, Small.At.Y - Big.At.Y).Size() > 1000.0)
					{
						continue;
					}
					Funnels.At = T;
					Funnels.Subject = Big.Key;
					Funnels.Other = Small.Key;
					break;
				}
			}
		}
		for (const SeaShow::Fx& E : Trial.Effects)
		{
			const FVector2D At(E.At.X, E.At.Y);
			switch (E.Kind)
			{
			case SeaShow::FxKind::Salvo:
				if (E.Side == 0 || E.Side == 1)
				{
					LastSalvoAt[E.Side] = T;
				}
				if (E.Size >= 180.0)
				{
					FSalvoSeen& Seen = SalvosSeen.AddDefaulted_GetRef();
					Seen.T = T;
					Seen.Size = E.Size;
					Seen.Side = E.Side;
					Seen.Aim = FVector2D(E.Aim.X, E.Aim.Y);
				}
				// (a battleship's first salvo, else the first of any; its fall of shot is looked for again after it)
				if (Broadside.At < 0.0 || (!bBigGuns && E.What == "bb"))
				{
					Fall.At = -1.0;
					const SeaShow::ShipOut* From = Trial.Ship(E.Key);
					Broadside.At = T;
					Broadside.Subject = E.Key;
					Broadside.Other = From ? From->Aim : -1;
					Broadside.Spot = FVector2D(E.Aim.X, E.Aim.Y);
					bBigGuns = E.What == "bb";
				}
				break;
			case SeaShow::FxKind::Line:
				// (the attacker's battle line as it forms, else the first line formed or turned: the ship plan's R1)
				if (Line.At < 0.0 || (Line.Other != 0 && E.Side == 0))
				{
					Line.At = T;
					Line.Subject = E.Key;
					Line.Other = E.Side;
					Line.Way = FVector2D(E.Vel.X, E.Vel.Y);
				}
				break;
			case SeaShow::FxKind::Splash:
				// The fall of shot (the art director, Samar r2e frame 06): a straddle, three or more columns of 180 mm and up
				// about one ship within three seconds, from salvos the other side fired at her in the last half minute;
				// none, and there is no such picture.
				if (Fall.At < 0.0 && Broadside.At >= 0.0 && T >= Broadside.At + 6.0 && E.Size >= 180.0)
				{
					const SeaShow::ShipOut* About = nullptr;
					double Best = TNumericLimits<double>::Max();
					for (const SeaShow::ShipOut& Ship : Trial.Ships())
					{
						const double Away = FVector2D::Distance(FVector2D(Ship.At.X, Ship.At.Y), At);
						if ((Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping) && Ship.bShown && Away <= Ship.Long * 0.5 + 300.0 && Away < Best)
						{
							Best = Away;
							About = &Ship;
						}
					}
					bool bAimed = false;
					for (const FSalvoSeen& Seen : SalvosSeen)
					{
						bAimed = bAimed || (About && Seen.Side != About->Side && T - Seen.T <= 30.0 && FVector2D::Distance(Seen.Aim, FVector2D(About->At.X, About->At.Y)) <= 600.0);
					}
					if (About && bAimed)
					{
						FColumnSeen& Seen = ColumnsSeen.AddDefaulted_GetRef();
						Seen.T = T;
						Seen.On = About->Key;
						int32 Count = 0;
						for (const FColumnSeen& Column : ColumnsSeen)
						{
							Count += Column.On == About->Key && T - Column.T <= 3.0 ? 1 : 0;
						}
						if (Count >= 3)
						{
							Fall.At = T;
							Fall.Subject = About->Key;
							Fall.Spot = FVector2D(About->At.X, About->At.Y);
						}
					}
				}
				break;
			case SeaShow::FxKind::ShellHit:
				if (Struck.At < 0.0)
				{
					Struck.At = T;
					Struck.Subject = E.Key;
				}
				break;
			case SeaShow::FxKind::TrackStart:
				if (Run.At < 0.0)
				{
					Run.At = T;
					Run.Spot = At;
					Run.Way = FVector2D(E.Vel.X, E.Vel.Y).GetSafeNormal();
					// (fired from a boat the player has not found: her place is no shot's, so the tracks are seen as his lookout
					// sees them, from the nearest ship of his they run toward, ShotEye; Other 1 says so)
					const SeaShow::ShipOut* Firer = nullptr;
					for (const SeaShow::ShipOut& Ship : Trial.Ships())
					{
						if (!Ship.bShown && FVector2D::Distance(FVector2D(Ship.At.X, Ship.At.Y), At) <= Ship.Long * 0.5 + 60.0)
						{
							Firer = &Ship;
						}
					}
					if (Firer)
					{
						Run.Other = 1;
						double Nearest = TNumericLimits<double>::Max();
						for (const SeaShow::ShipOut& Ship : Trial.Ships())
						{
							const FVector2D Off = FVector2D(Ship.At.X, Ship.At.Y) - At;
							if (Ship.bShown && Ship.Side != Firer->Side && Ship.Doing == SeaShow::Fate::Line && FVector2D::DotProduct(Off, Run.Way) > 0.0 && Off.Size() < Nearest)
							{
								Nearest = Off.Size();
								Run.Subject = Ship.Key;
							}
						}
					}
				}
				break;
			case SeaShow::FxKind::BombHit:
			case SeaShow::FxKind::NearMiss:
				if (Strike.At < 0.0)
				{
					Strike.At = T;
					Strike.Subject = E.Kind == SeaShow::FxKind::BombHit ? E.Key : -1;
					Strike.Spot = At;
				}
				break;
			case SeaShow::FxKind::Sink:
				if (Down.At < 0.0)
				{
					Down.At = T;
					Down.Subject = E.Key;
				}
				break;
			case SeaShow::FxKind::LeaveLine:
				Burning.At = T;
				Burning.Subject = E.Key;
				break;
			case SeaShow::FxKind::OilFire:
				if (Burning.Subject < 0)
				{
					Burning.At = T;
					Burning.Spot = At;
				}
				break;
			default:
				break;
			}
		}
		UnderPlan.Step(T, Trial); // U1 hook (under water): the step's effects read before they go
		Trial.Effects.clear();
		const bool bDuel = T - LastSalvoAt[0] <= 30.0 && T - LastSalvoAt[1] <= 30.0;
		if (Step % 4 == 0 && (bDuel || LinesSeen.Num() > 0))
		{
			FLinesSeen& Seen = LinesSeen.AddDefaulted_GetRef();
			Seen.T = T;
			Seen.bDuel = bDuel;
			for (const SeaShow::ShipOut& Ship : Trial.Ships())
			{
				if ((Ship.Side == 0 || Ship.Side == 1) && Ship.bShown && (Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping))
				{
					FLineShip& One = Seen.Ships.AddDefaulted_GetRef();
					One.Key = Ship.Key;
					One.Side = Ship.Side;
					One.P = FVector2D(Ship.At.X, Ship.At.Y);
					One.bLine = Ship.LineAt >= 0;
					One.bBig = Rank(Ship.Class) >= 2 || Ship.Class == "cl";
				}
			}
		}
		// (the eye the cloud test gives it: south of the middle of every piece)
		double X = 0.0, Y = 0.0;
		int32 Count = 0;
		for (const auto& Pair : Trial.Pieces())
		{
			X += Pair.second.X;
			Y += Pair.second.Y;
			++Count;
		}
		if (Count > 0)
		{
			Trial.Eye = SeaShow::Vec(X / Count, Y / Count - 3000.0, 900.0);
		}
	}
	// When each is taken: the fleets before the guns meet, a battleship's first broadside, its fall of shot a second after the
	// columns rise, a hit as it bursts, the tracks once they have run, a strike as it dives, a ship well into her end, the
	// whole battle at its height, and its end.
	auto Add = [this](FShot Shot, const TCHAR* Name, double At)
	{
		Shot.Name = Name;
		Shot.At = At;
		Shots.Add(Shot);
	};
	Add(Fleets, TEXT("the-fleets"), Contact >= 0.0 ? FMath::Max(S.BeginT + 12.0, Contact - 40.0) : S.BeginT + 30.0);
	if (Line.At >= 0.0)
	{
		// (twenty seconds on, the columns round onto the new course in line ahead; kept 8 s clear of the broadside, so a
		// watched cut holds each)
		double LineAt = Line.At + 20.0;
		if (Broadside.At >= 0.0 && FMath::Abs(LineAt - Broadside.At) < 8.0 && Broadside.At - 8.0 >= Line.At + 10.0)
		{
			LineAt = Broadside.At - 8.0;
		}
		Add(Line, TEXT("the-line"), LineAt);
	}
	if (Broadside.At >= 0.0)
	{
		Add(Broadside, TEXT("broadside"), Broadside.At + 0.2);
	}
	if (Fall.At >= 0.0)
	{
		// (a second and a half after the last column falls: all of them standing, the first near their height)
		Add(Fall, TEXT("fall-of-shot"), Fall.At + 1.5);
	}
	if (Struck.At >= 0.0)
	{
		Add(Struck, TEXT("a-hit"), Struck.At + 0.5);
	}
	if (Run.At >= 0.0)
	{
		Add(Run, TEXT("torpedo-tracks"), Run.At + 20.0);
	}
	if (Strike.At >= 0.0)
	{
		Add(Strike, TEXT("the-strike"), Strike.At - 2.5);
	}
	if (Down.At >= 0.0)
	{
		Add(Down, TEXT("going-down"), Down.At + 20.0);
	}
	if (Funnels.At >= 0.0)
	{
		Add(Funnels, TEXT("funnels"), Funnels.At + 2.0);
	}
	FShot Whole;
	Whole.Kind = ESeaShot::Overview;
	Add(Whole, TEXT("the-battle"), Contact >= 0.0 ? Contact + (S.EndT - Contact) * 0.45 : (S.BeginT + S.EndT) * 0.5);
	if (Burning.At >= 0.0)
	{
		Add(Burning, TEXT("the-end"), S.EndT - 8.0);
	}
	else
	{
		Add(Whole, TEXT("the-end"), S.EndT - 8.0);
	}
	// BOTH LINES IN ONE PICTURE (the camera director's CD2, 22 November, camera-director/CD2-tips-air-sea.md): once in the
	// gun duel, a held wide with both sides in it, the way a war film shows how big the fight is. Each side's line (her
	// ships in a battle line; else her big ships; else all she has afloat) is weighed from places nearest first, the eye
	// riding with the player's own ships: the first place that keeps 85 in the hundred of them inside the picture (a
	// margin in from its edges for the eye's sway), at every second of the 7 it must hold, is taken; the earliest such
	// moment from 15 s into the duel, clear of the other shots (none due in its 7 s, none in the 3 s before it cuts in).
	{
		static constexpr double Least = 7.0, Most = 20.0, Holds = 0.85, Edge = 0.92, Tall = 9.0 / 16.0;
		const int32 Mine = SeaFeed::SideOf(S.Rec.Views.front().Head.Me) == 1 ? 1 : 0;
		int32 FirstDuel = INDEX_NONE;
		for (int32 K = 0; K < LinesSeen.Num() && FirstDuel == INDEX_NONE; ++K)
		{
			FirstDuel = LinesSeen[K].bDuel ? K : INDEX_NONE;
		}
		auto Where = [](const FLinesSeen& Seen, int32 Key, FVector2D& Out) -> bool
		{
			for (const FLineShip& One : Seen.Ships)
			{
				if (One.Key == Key)
				{
					Out = One.P;
					return true;
				}
			}
			return false;
		};
		// (the picture's measure from an eye looking along F: across and up over the distance ahead; false behind it)
		auto Across = [](const FVector& Pos, const FVector& F, const FVector2D& P, double& OutX, double& OutY) -> bool
		{
			const FVector R = FVector::CrossProduct(F, FVector::UpVector).GetSafeNormal();
			const FVector U = FVector::CrossProduct(R, F);
			const FVector D = FVector(P.X, P.Y, 8.0) - Pos;
			const double Z = FVector::DotProduct(D, F);
			if (Z < 1.0)
			{
				return false;
			}
			OutX = FVector::DotProduct(D, R) / Z;
			OutY = FVector::DotProduct(D, U) / Z;
			return true;
		};
		// (the share of the ships, and the least of each side's, in the picture at one second: the eye ridden on by then)
		auto Share = [&](const FLinesSeen& Seen, const TArray<int32>& Keys, const TArray<int32>& Sides, const FVector& Pos, const FVector& F, double Lens) -> double
		{
			const double HalfX = Edge * FMath::Tan(FMath::DegreesToRadians(Lens * 0.5)), HalfY = HalfX * Tall;
			int32 In[2] = { 0, 0 }, Of[2] = { 0, 0 };
			for (int32 K = 0; K < Keys.Num(); ++K)
			{
				FVector2D P;
				double X = 0.0, Y = 0.0;
				if (!Where(Seen, Keys[K], P))
				{
					continue;
				}
				++Of[Sides[K]];
				In[Sides[K]] += Across(Pos, F, P, X, Y) && FMath::Abs(X) <= HalfX && FMath::Abs(Y) <= HalfY ? 1 : 0;
			}
			if (Of[0] == 0 || Of[1] == 0)
			{
				return 0.0;
			}
			const double Each = FMath::Min(In[0] / static_cast<double>(Of[0]), In[1] / static_cast<double>(Of[1]));
			return Each < 0.5 ? 0.0 : (In[0] + In[1]) / static_cast<double>(Of[0] + Of[1]);
		};
		FShot Lines;
		Lines.Kind = ESeaShot::BothLines;
		double LinesShare = -1.0;
		int32 LinesFrom = INDEX_NONE, LinesShips[2] = { 0, 0 };
		// (bounded, the pre-check's note of 22 November: a long duel with no good place would try 504 placements each
		// clear second. A moment whose best is under 60 in the hundred steps on 5 s, and 3,000 placements in all at the
		// most, about six moments searched to the end)
		static constexpr int32 MostTries = 3000;
		int32 Tried = 0, Stride = 1;
		for (int32 I = FirstDuel == INDEX_NONE ? LinesSeen.Num() : FirstDuel + 15; I + static_cast<int32>(Least) < LinesSeen.Num() && LinesShare < Holds && Tried < MostTries; I += Stride)
		{
			Stride = 1;
			const FLinesSeen& Seen = LinesSeen[I];
			const double At = Seen.T + SeaParts::CutBefore;
			bool bClear = Seen.bDuel;
			for (const FShot& Other : Shots)
			{
				bClear = bClear && !(Other.At > At - 7.0 && Other.At < At + Least);
			}
			if (!bClear)
			{
				continue;
			}
			// (each side's line, its middle, and the player's own ships' way over the 7 s)
			TArray<int32> Keys, Sides;
			FVector2D Middle[2] = { FVector2D::ZeroVector, FVector2D::ZeroVector };
			int32 Of[2] = { 0, 0 };
			for (int32 Side = 0; Side < 2; ++Side)
			{
				bool bAnyLine = false, bAnyBig = false;
				for (const FLineShip& One : Seen.Ships)
				{
					bAnyLine = bAnyLine || (One.Side == Side && One.bLine);
					bAnyBig = bAnyBig || (One.Side == Side && One.bBig);
				}
				for (const FLineShip& One : Seen.Ships)
				{
					if (One.Side == Side && (bAnyLine ? One.bLine : !bAnyBig || One.bBig))
					{
						Keys.Add(One.Key);
						Sides.Add(Side);
						Middle[Side] += One.P;
						++Of[Side];
					}
				}
			}
			if (Of[0] == 0 || Of[1] == 0)
			{
				continue;
			}
			Middle[0] /= static_cast<double>(Of[0]);
			Middle[1] /= static_cast<double>(Of[1]);
			const FLinesSeen& Then = LinesSeen[I + static_cast<int32>(Least)];
			FVector2D Way = FVector2D::ZeroVector;
			int32 Riding = 0;
			for (int32 K = 0; K < Keys.Num(); ++K)
			{
				FVector2D From, To;
				if (Sides[K] == Mine && Where(Seen, Keys[K], From) && Where(Then, Keys[K], To))
				{
					Way += (To - From) / Least;
					++Riding;
				}
			}
			Way = Riding > 0 ? Way / static_cast<double>(Riding) : FVector2D::ZeroVector;
			const FVector2D OverTo = (Middle[1 - Mine] - Middle[Mine]).GetSafeNormal();
			if (OverTo.IsNearlyZero())
			{
				continue;
			}
			const FVector2D Between = (Middle[0] + Middle[1]) * 0.5;
			// (the ways it may look, each from behind what it stands off: over the player's own line toward theirs, then round
			// to either hand, her line in the near water and theirs beyond; and astern of both lines, between them, looking
			// up the gap)
			TArray<FVector2D> Looks, Offs;
			for (const double Turn : { 0.0, 35.0, -35.0, 70.0, -70.0 })
			{
				Looks.Add(OverTo.GetRotated(Turn));
				Offs.Add(Middle[Mine]);
			}
			if (Way.Size() > 1.0)
			{
				Looks.Add(Way.GetSafeNormal());
				Offs.Add(Between);
			}
			double HereBest = -1.0;
			for (int32 Farther = 0; Farther < 14 && LinesShare < Holds && Tried < MostTries; ++Farther)
			{
				// (300 m back, then a quarter farther each step: 5.5 km at the most)
				const double Back = 300.0 * FMath::Pow(1.25, Farther);
				for (const double Up : { 300.0, 450.0, 600.0 })
				{
					for (int32 L = 0; L < Looks.Num(); ++L)
					{
						const FVector2D& Look = Looks[L];
						for (const double Lens : { 50.0, 60.0 })
						{
							const FVector2D Ground = Offs[L] - Look * Back;
							const FVector EyeP(Ground.X, Ground.Y, Up);
							// (turned to the middle of where they stand across the picture, twice over; then tipped down to put the
							// horizon on the picture's upper third, the sea and both lines below it)
							FVector F = FVector(Look, 0.0).GetSafeNormal();
							for (int32 Again = 0; Again < 2; ++Again)
							{
								TArray<double> Xs;
								for (int32 K = 0; K < Keys.Num(); ++K)
								{
									FVector2D P;
									double X = 0.0, Y = 0.0;
									if (Where(Seen, Keys[K], P) && Across(EyeP, F, P, X, Y))
									{
										Xs.Add(X);
									}
								}
								if (Xs.Num() == 0)
								{
									break;
								}
								Xs.Sort();
								const int32 Lo = Xs.Num() * 3 / 100, Hi = Xs.Num() - 1 - Lo;
								F = (F + FVector::CrossProduct(F, FVector::UpVector).GetSafeNormal() * ((Xs[Lo] + Xs[Hi]) * 0.5)).GetSafeNormal();
							}
							const double Tip = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Lens * 0.5)) * Tall / 3.0);
							F = FVector(F.X * FMath::Cos(Tip), F.Y * FMath::Cos(Tip), -FMath::Sin(Tip));
							// (the least share over the 7 s, the eye riding on with the player's ships: followed to the end while it
							// still beats the best so far, so the best's is its true least)
							++Tried;
							double Least7 = 1.0;
							for (int32 J = 0; J <= static_cast<int32>(Least) && Least7 > LinesShare; ++J)
							{
								Least7 = FMath::Min(Least7, Share(LinesSeen[I + J], Keys, Sides, EyeP + FVector(Way * static_cast<double>(J), 0.0), F, Lens));
							}
							HereBest = FMath::Max(HereBest, Least7);
							if (Least7 > LinesShare)
							{
								LinesShare = Least7;
								LinesFrom = I;
								LinesShips[0] = Of[0];
								LinesShips[1] = Of[1];
								Lines.At = At;
								Lines.EyeFrom = EyeP;
								Lines.LookAt = EyeP + F * 1000.0;
								Lines.Way = Way;
								Lines.Lens = Lens;
								// (held on past the 7 s while four in five of them stay in it, to 20 s at the most)
								Lines.Hold = Least;
								for (int32 J = static_cast<int32>(Least) + 1; J <= static_cast<int32>(Most) && I + J < LinesSeen.Num(); ++J)
								{
									if (Share(LinesSeen[I + J], Keys, Sides, EyeP + FVector(Way * static_cast<double>(J), 0.0), F, Lens) < 0.8)
									{
										break;
									}
									Lines.Hold = J;
								}
							}
							if (LinesShare >= Holds)
							{
								break;
							}
						}
						if (LinesShare >= Holds)
						{
							break;
						}
					}
					if (LinesShare >= Holds)
					{
						break;
					}
				}
			}
			Stride = HereBest < 0.6 ? 5 : 1;
		}
		if (LinesFrom != INDEX_NONE && LinesShare >= 0.6)
		{
			Add(Lines, TEXT("both-lines"), Lines.At);
			UE_LOG(LogTheater, Display, TEXT("Sea battle: both lines from %.0f s, %d and %d ships, %.0f in the hundred in the picture at the least over 7 s, held %.0f s; the eye %.0f m up, a %.0f degree lens."),
				Lines.At - SeaParts::CutBefore, LinesShips[0], LinesShips[1], 100.0 * LinesShare, Lines.Hold, Lines.EyeFrom.Z, Lines.Lens);
		}
	}
	// U1 hook (under water): the director's windows under the sea, each a shot (its window in Way.X) that cuts in as it
	// opens, and a second picture as its first charge bursts; clear of the shots above, but the whole battle from above (a
	// filler) gives way, moved to after it. A fleet action has no boats and so no windows. The windows are always planned,
	// so that the switch can be turned on while the battle plays; with it off none of them becomes a shot.
	if (Under)
	{
		std::vector<double> Busy;
		for (const FShot& Taken : Shots)
		{
			if (Taken.Name != TEXT("the-battle"))
			{
				Busy.push_back(Taken.At - SeaParts::CutBefore);
			}
		}
		// (the month's scene plays beats, each at its own pace; watched and in a picture run there are none)
		std::vector<SeaUnder::Director::Beat> Paces;
		if (bMonthScene)
		{
			for (const SeaFeed::Beat& Window : S.Rec.Beats)
			{
				Paces.push_back({ Window.From, Window.To, Window.Screen });
			}
			if (Paces.empty())
			{
				// (a scene with no beats plays the whole battle at the one pace: ASeaBattle::RunMonthScene's FastestWindow)
				Paces.push_back({ S.BeginT, S.EndT, (S.EndT - S.BeginT) / FMath::Clamp((S.EndT - S.BeginT) / 40.0, 1.0, SeaUnder::Director::FastestBeat) });
			}
		}
		std::vector<SeaUnder::Window> UnderWindows = UnderPlan.Finish(Busy, Paces, S.BeginT, S.EndT);
		if (Under->IsOn() && !ShotFolder.IsEmpty())
		{
			// (a picture run takes the windows as its own pictures: named by their window, so no two shots share a name)
			for (int32 K = 0; K < static_cast<int32>(UnderWindows.size()); ++K)
			{
				const SeaUnder::Window& W = UnderWindows[K];
				const FString Named = FString::Printf(TEXT("%s-%d"), UTF8_TO_TCHAR(W.Name.c_str()), K + 1);
				FShot Dived;
				Dived.Kind = ESeaShot::Under;
				Dived.Subject = W.Boat;
				Dived.Other = W.Escort;
				Dived.Spot = FVector2D(W.Aim.X, W.Aim.Y);
				Dived.Way = FVector2D(static_cast<double>(K), 0.0);
				Add(Dived, *Named, W.From + SeaParts::CutBefore);
				if (W.FirstBurst > W.From + SeaParts::CutBefore + 1.0)
				{
					Add(Dived, *(Named + TEXT("-burst")), W.FirstBurst + 0.15); // (the flash and the globe at its widest)
				}
				for (FShot& Filler : Shots)
				{
					const double Cut = Filler.At - SeaParts::CutBefore;
					if (Filler.Name == TEXT("the-battle") && Cut >= W.From - 3.0 && Cut <= W.Until + 3.0)
					{
						Filler.At = W.Until + SeaParts::CutBefore + 6.0;
					}
				}
			}
		}
		Under->TakeWindows(MoveTemp(UnderWindows), UnderPlan.Sighted());
	}
	if (bWaterSheet)
	{
		Shots.Reset();
		static const TCHAR* const Names[4] = { TEXT("water-28m-away-from-sun"), TEXT("water-28m-toward-sun"), TEXT("water-60m-away-from-sun"), TEXT("water-60m-toward-sun") };
		for (int32 K = 0; K < 4; ++K)
		{
			FShot Sea;
			Sea.Kind = ESeaShot::Water;
			Sea.Spot = FVector2D(K < 2 ? 28.0 : 60.0, K % 2 ? 1.0 : 0.0); // (its height, and whether toward the sun)
			Add(Sea, Names[K], SheetT + K * 1.0);
		}
		for (int32 K = 0; K < 20; ++K)
		{
			Ride.Spot = FVector2D(-1.0, 0.0); // (the pair, side on)
			Add(Ride, *FString::Printf(TEXT("ride-%02d"), K), SheetT + 6.0 + K * 0.5);
		}
	}
	Shots.RemoveAll([&S](const FShot& Shot) { return Shot.At < S.BeginT + 2.0; });
	Shots.StableSort([](const FShot& A, const FShot& B) { return A.At < B.At; });
	for (int32 K = 0; K < Shots.Num(); ++K)
	{
		Shots[K].At = FMath::Min(K > 0 ? FMath::Max(Shots[K].At, Shots[K - 1].At + (bWaterSheet ? 0.5 : 1.0)) : Shots[K].At, S.EndT + 4.0);
		Shots[K].Name = FString::Printf(TEXT("%02d-%s"), K + 1, *Shots[K].Name);
		UE_LOG(LogTheater, Display, TEXT("Sea battle: shot %s at %.1f s (ship %d)."), *Shots[K].Name, Shots[K].At, Shots[K].Subject);
	}
}

bool ASeaBattle::ShotEye(const FShot& Shot, FVector& OutEye, FVector& OutLook, double& OutLens)
{
	using SeaParts::Ahead;
	using SeaParts::Starboard;
	FSeaBattleState& S = *State;
	auto Find = [this, &S](int32 Which, FVector2D& OutP, double& OutHeading, double& OutLong) -> bool
	{
		if (Which < 0)
		{
			return false;
		}
		if (const SeaShow::ShipOut* Ship = S.Show.Ship(Which))
		{
			// (an enemy boat the player has not found is no shot's: neither where she is nor where she was last seen)
			if (!Ship->bShown)
			{
				return false;
			}
			OutP = FVector2D(Ship->At.X, Ship->At.Y);
			OutHeading = Ship->Heading;
			OutLong = DrawnLong.Contains(Which) ? DrawnLong[Which] : Ship->Long;
			return true;
		}
		// (a ship the show no longer draws, gone down or out of the fight: where she was last)
		if (const FVector4* Was = LastSeen.Find(Which))
		{
			OutP = FVector2D(Was->X, Was->Y);
			OutHeading = Was->Z;
			OutLong = Was->W;
			return true;
		}
		return false;
	};
	auto HerSide = [&S](int32 Which)
	{
		const SeaShow::ShipOut* Ship = S.Show.Ship(Which);
		return Ship ? Ship->Side : -1;
	};
	auto Centre = [&S](int32 Which, FVector2D& Out) -> bool
	{
		FVector2D Sum = FVector2D::ZeroVector;
		int32 Count = 0;
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			if (Ship.Side == Which && Ship.Doing == SeaShow::Fate::Line && Ship.bShown)
			{
				Sum += FVector2D(Ship.At.X, Ship.At.Y);
				++Count;
			}
		}
		if (Count == 0)
		{
			return false;
		}
		Out = Sum / static_cast<double>(Count);
		return true;
	};
	FVector2D P = FVector2D::ZeroVector;
	double Heading = 0.0, Long = 150.0;
	switch (Shot.Kind)
	{
	case ESeaShot::Fleets:
	{
		// Behind the column's quarter, looking up its line as it zig-zags (never a column the player has not found).
		const SeaShow::ShipOut* Leader = S.Show.Ship(Shot.Subject);
		if (Leader && !Leader->bShown)
		{
			return false;
		}
		SeaShow::Vec Middle;
		double Face = 0.0;
		if (S.Show.PieceMiddle(Shot.Other, Middle, Face))
		{
			P = FVector2D(Middle.X, Middle.Y);
			Heading = FMath::RadiansToDegrees(Face);
		}
		else if (!Find(Shot.Subject, P, Heading, Long))
		{
			return false;
		}
		const FVector2D From = P - Ahead(Heading) * 700.0 + Starboard(Heading) * 420.0, To = P + Ahead(Heading) * 250.0;
		OutEye = World(From.X, From.Y, 55.0);
		OutLook = World(To.X, To.Y, 0.0);
		OutLens = 45.0;
		return true;
	}
	case ESeaShot::Line:
	{
		// Ahead of the line's guide on its engaged side, low, looking back down the line as it steams in line ahead, its
		// guns trained out past the camera.
		const int32 Hers = Shot.Other;
		bool bGuide = Find(Shot.Subject, P, Heading, Long);
		FVector2D Sum = FVector2D::ZeroVector;
		int32 Count = 0;
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			if (Ship.Side != Hers || Ship.LineAt < 0 || !Ship.bShown)
			{
				continue;
			}
			Sum += FVector2D(Ship.At.X, Ship.At.Y);
			++Count;
			if (!bGuide && Ship.LineAt == 0)
			{
				// (the guide's leading ship gone: another of the guide's column)
				P = FVector2D(Ship.At.X, Ship.At.Y);
				Heading = Ship.Heading;
				Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
				bGuide = true;
			}
		}
		if (!bGuide)
		{
			return false;
		}
		const FVector2D Course = Ahead(Heading);
		// (the line's own axis, from its middle up to its guide: the course once it is in line ahead, a bearing line while
		// its rows are still coming round; the course where the line is one column)
		const FVector2D Middle = Count > 0 ? Sum / static_cast<double>(Count) : P - Course * 800.0;
		const double Spread = FVector2D::Distance(P, Middle);
		const FVector2D Axis = Spread > 2.0 * Long ? (P - Middle) / Spread : Course;
		FVector2D Out(Axis.Y, -Axis.X);
		FVector2D Foe;
		if (Hers >= 0 && Centre(1 - Hers, Foe) && FVector2D::DotProduct(Foe - P, Out) < 0.0)
		{
			Out = -Out;
		}
		// (out ahead of the guide along the axis and a little to the engaged side, looking a third of the way down the
		// line: the guide near the frame's edge, the line going away behind her)
		const FVector2D From = P + Axis * (Long * 5.0) + Out * (Long * 2.0);
		const FVector2D Down = P - Axis * (FMath::Max(Spread, 600.0) * 0.35);
		OutEye = World(From.X, From.Y, 20.0);
		OutLook = World(Down.X, Down.Y, 8.0);
		OutLens = 40.0;
		return true;
	}
	case ESeaShot::Broadside:
	{
		// On her far side from her target, low, looking over her toward the enemy: her guns against the sky.
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			return false;
		}
		FVector2D Target = Shot.Spot;
		double TargetHeading = 0.0, TargetLong = 0.0;
		if (!Find(Shot.Other, Target, TargetHeading, TargetLong))
		{
			const int32 Hers = HerSide(Shot.Subject);
			if (Hers < 0 || !Centre(1 - Hers, Target))
			{
				Target = Shot.Spot;
			}
		}
		const FVector2D To = (Target - P).GetSafeNormal();
		const FVector2D From = P - To * (Long * 0.95) - Ahead(Heading) * (Long * 0.55);
		const FVector2D Over = P + To * 2000.0;
		OutEye = World(From.X, From.Y, 24.0);
		OutLook = World(Over.X, Over.Y, 12.0);
		OutLens = 55.0;
		return true;
	}
	case ESeaShot::Rangefinder:
	{
		// From the firing side, 28 m up through a 19 degree lens (the art director, Samar r2e frame 06): the straddled
		// ship a third of the frame wide, as near as that puts the eye (never nearer than 700 m nor farther than 2.6 km),
		// looking a little over her waterline so the columns' 40 to 60 m stand in the frame.
		int32 Target = HerSide(Shot.Subject);
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			P = Shot.Spot;
			double Best = TNumericLimits<double>::Max();
			for (const SeaShow::ShipOut& Ship : S.Show.Ships())
			{
				const double Away = FVector2D::Distance(FVector2D(Ship.At.X, Ship.At.Y), P);
				if (Ship.bShown && Away < Best)
				{
					Best = Away;
					Target = Ship.Side;
				}
			}
		}
		FVector2D Firing = P + FVector2D(0.0, -3000.0);
		if (Target >= 0)
		{
			Centre(1 - Target, Firing);
		}
		FVector2D Way = (Firing - P).GetSafeNormal();
		Way = Way.IsNearlyZero() ? FVector2D(0.0, -1.0) : Way;
		const double Lens = 19.0;
		const double HalfTan = FMath::Tan(FMath::DegreesToRadians(Lens * 0.5));
		// (how wide she shows from there: her length across the line of sight, her beam along it)
		const double Side = FMath::Abs(FVector2D::DotProduct(Ahead(Heading), FVector2D(Way.Y, -Way.X)));
		const double Shows = Long * Side + 0.12 * Long * FMath::Sqrt(FMath::Max(0.0, 1.0 - Side * Side));
		const double Away = FMath::Clamp(3.0 * Shows / (2.0 * HalfTan), 700.0, 2600.0);
		const FVector2D From = P + Way * Away;
		OutEye = World(From.X, From.Y, 28.0);
		OutLook = World(P.X, P.Y, 22.0);
		OutLens = Lens;
		return true;
	}
	case ESeaShot::Hit:
	{
		// Off her side toward the enemy, where the shells came from.
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			return false;
		}
		FVector2D Toward = Starboard(Heading);
		FVector2D Foe;
		const int32 Hers = HerSide(Shot.Subject);
		if (Hers >= 0 && Centre(1 - Hers, Foe) && FVector2D::DotProduct(Foe - P, Toward) < 0.0)
		{
			Toward = -Toward;
		}
		const FVector2D From = P + Toward * (Long * 1.7) - Ahead(Heading) * (Long * 0.5);
		OutEye = World(From.X, From.Y, 30.0);
		OutLook = World(P.X, P.Y, 8.0);
		OutLens = 40.0;
		return true;
	}
	case ESeaShot::Tracks:
	{
		// Above and behind where the salvo was fired, looking down its tracks.
		const FVector2D Way = Shot.Way.IsNearlyZero() ? FVector2D(0.0, 1.0) : Shot.Way.GetSafeNormal();
		const FVector2D Aside(Way.Y, -Way.X);
		if (Shot.Other == 1)
		{
			// Fired from a boat the player has not found (PlanShots): no eye over where she is, but his lookout's, on the
			// bridge wing of the ship the tracks run toward on the side they come from, just outboard of her hull, looking
			// back up them as they come; with no such ship, high over the tracks ahead, looking back along them. Neither
			// eye is ever inside steel.
			const FVector2D Coming = Shot.Spot + Way * 700.0;
			if (Find(Shot.Subject, P, Heading, Long))
			{
				const SeaShow::ShipOut* Hers = S.Show.Ship(Shot.Subject);
				const double HalfBeam = 0.5 * (Hers ? Hers->Beam : Long * 0.13);
				const FVector2D Wing = FVector2D::DotProduct(Starboard(Heading), Shot.Spot - P) >= 0.0 ? Starboard(Heading) : -Starboard(Heading);
				const FVector2D Lookout = P + Ahead(Heading) * (Long * 0.12) + Wing * (HalfBeam + 2.0);
				const FVector2D Toward = FVector2D::Distance(Lookout, Shot.Spot) > 900.0 ? Coming : Shot.Spot;
				OutEye = World(Lookout.X, Lookout.Y, 14.0);
				OutLook = World(Toward.X, Toward.Y, 0.0);
				OutLens = 40.0;
				return true;
			}
			const FVector2D High = Shot.Spot + Way * 1500.0 + Aside * 150.0;
			OutEye = World(High.X, High.Y, 260.0);
			OutLook = World(Coming.X, Coming.Y, 0.0);
			OutLens = 50.0;
			return true;
		}
		const FVector2D From = Shot.Spot - Way * 350.0 + Aside * 150.0, To = Shot.Spot + Way * 700.0;
		OutEye = World(From.X, From.Y, 260.0);
		OutLook = World(To.X, To.Y, 0.0);
		OutLens = 50.0;
		return true;
	}
	case ESeaShot::Strike:
	{
		// Low, across her from the far side of her attackers: she low in the picture and the bombers coming down above
		// her, the lens as wide as both need. (The first Mac frames, 21 November: looking up into the air over her beam,
		// the dive was out of the picture and she was cut off at its foot.)
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			P = Shot.Spot;
			Heading = 0.0;
		}
		const int32 Hers = HerSide(Shot.Subject);
		FVector Sum = FVector::ZeroVector;
		int32 Diving = 0;
		for (const SeaShow::PlaneOut& Plane : S.Show.Planes())
		{
			if (!Plane.bShown || Plane.bFalling || Plane.Role == "cover" || (Hers >= 0 && Plane.Side == Hers))
			{
				continue;
			}
			if (FVector2D(Plane.At.X - P.X, Plane.At.Y - P.Y).Size() > 4000.0)
			{
				continue;
			}
			Sum += FVector(Plane.At.X, Plane.At.Y, Plane.At.Z);
			++Diving;
		}
		const double EyeUp = 18.0;
		if (Diving == 0)
		{
			const FVector2D From = P + Starboard(Heading) * 650.0 - Ahead(Heading) * 250.0;
			OutEye = World(From.X, From.Y, EyeUp);
			OutLook = World(P.X, P.Y, 90.0);
			OutLens = 55.0;
			return true;
		}
		const FVector Them = Sum / static_cast<double>(Diving);
		FVector2D Toward(Them.X - P.X, Them.Y - P.Y);
		Toward = Toward.Size() < 1.0 ? Starboard(Heading) : Toward.GetSafeNormal();
		const FVector2D Aside(-Toward.Y, Toward.X);
		const FVector2D From = P - Toward * FMath::Max(500.0, Long * 2.2) + Aside * 160.0;
		// (the angles up from the eye to her waterline and to them: the picture's middle between the two, its height
		// both and a margin, at the pictures' 1.6 to 1)
		const double ToHer = FMath::Atan2(4.0 - EyeUp, FVector2D::Distance(From, P));
		const double ToThem = FMath::Atan2(Them.Z - EyeUp, FVector2D::Distance(From, FVector2D(Them.X, Them.Y)));
		const double Tall = FMath::Clamp((ToThem - ToHer) * 1.35 + FMath::DegreesToRadians(6.0), FMath::DegreesToRadians(18.0), FMath::DegreesToRadians(60.0));
		OutLens = FMath::Clamp(FMath::RadiansToDegrees(2.0 * FMath::Atan(FMath::Tan(Tall * 0.5) * 1.6)), 40.0, 85.0);
		const double Up = 0.5 * (ToHer + ToThem);
		const FVector2D Look = ((P + FVector2D(Them.X, Them.Y)) * 0.5 - From).GetSafeNormal();
		OutEye = World(From.X, From.Y, EyeUp);
		OutLook = World(From.X + Look.X * 1000.0 * FMath::Cos(Up), From.Y + Look.Y * 1000.0 * FMath::Cos(Up), EyeUp + 1000.0 * FMath::Sin(Up));
		return true;
	}
	case ESeaShot::Sinking:
	{
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			return false;
		}
		const FVector2D From = P + Starboard(Heading) * (Long * 2.6) + Ahead(Heading) * (Long * 0.9);
		OutEye = World(From.X, From.Y, 28.0);
		OutLook = World(P.X, P.Y, 2.0);
		OutLens = 38.0;
		return true;
	}
	case ESeaShot::Burning:
	{
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			P = Shot.Spot;
		}
		const FVector2D From = P + FVector2D(700.0, -350.0);
		OutEye = World(From.X, From.Y, 70.0);
		OutLook = World(P.X, P.Y, 40.0);
		OutLens = 45.0;
		return true;
	}
	case ESeaShot::Funnels:
	{
		// Side on to the big ship from the destroyer's side of her, low, far enough out for both and their smoke astern.
		FVector2D Other;
		double OtherHeading = 0.0, OtherLong = 0.0;
		if (!Find(Shot.Subject, P, Heading, Long) || !Find(Shot.Other, Other, OtherHeading, OtherLong))
		{
			return false;
		}
		const FVector2D Middle = (P + Other) * 0.5;
		const double Way = FVector2D::DotProduct(Other - P, Starboard(Heading)) >= 0.0 ? 1.0 : -1.0;
		const double Apart = FVector2D::Distance(P, Other);
		const FVector2D From = Middle + Starboard(Heading) * (Way * FMath::Max(650.0, Apart * 1.2 + Long)) - Ahead(Heading) * (Long * 0.4);
		OutEye = World(From.X, From.Y, 16.0);
		OutLook = World(Middle.X, Middle.Y, 22.0);
		OutLens = 50.0;
		return true;
	}
	case ESeaShot::Water:
	{
		// The water's check sheet (PlanShots): the eye 28 or 60 m up, 2 km short of the ships' middle, looking over them
		// away from the sun or toward it through a 50 degree lens, a little down, the horizon a little above the frame's
		// middle; or the pair side by side, from 1.2 km off on the sun's side, 30 m up.
		const FVector Lit = Stage ? Stage->TowardSun() : FVector(1.0, 0.0, 0.5);
		FVector2D Sun = FVector2D(Lit.Y, Lit.X).GetSafeNormal();
		Sun = Sun.IsNearlyZero() ? FVector2D(0.0, 1.0) : Sun;
		// (how far the nearest ship afloat lies from a point: the eye is never put among them)
		auto Clear = [&S](const FVector2D& At)
		{
			double Nearest = TNumericLimits<double>::Max();
			for (const SeaShow::ShipOut& Ship : S.Show.Ships())
			{
				if (Ship.Doing != SeaShow::Fate::Sinking && Ship.bShown)
				{
					Nearest = FMath::Min(Nearest, FVector2D::Distance(FVector2D(Ship.At.X, Ship.At.Y), At));
				}
			}
			return Nearest;
		};
		if (Shot.Spot.X < 0.0)
		{
			FVector2D A, B;
			double HeadingA = 0.0, HeadingB = 0.0, LongA = 100.0, LongB = 200.0;
			if (!Find(Shot.Subject, A, HeadingA, LongA))
			{
				return false;
			}
			if (!Find(Shot.Other, B, HeadingB, LongB))
			{
				B = A;
				LongB = LongA;
			}
			const FVector2D Middle = (A + B) * 0.5;
			FVector2D Abeam = FVector2D(Ahead(HeadingB).Y, -Ahead(HeadingB).X);
			Abeam = FVector2D::DotProduct(Abeam, Sun) < 0.0 ? -Abeam : Abeam;
			const double Span = FVector2D::Distance(A, B) + 0.5 * (LongA + LongB) + 120.0;
			// (on her sunlit side 1.2 to 1.8 km off where no other ship lies within 600 m of the eye, else her shaded side,
			// else wherever the nearest ship lies farthest)
			FVector2D From = Middle + Abeam * 1200.0;
			double Off = 1200.0, Roomiest = -1.0;
			bool bFound = false;
			for (int32 Try = 0; Try < 6 && !bFound; ++Try)
			{
				const double Far = 1200.0 + 300.0 * (Try % 3);
				const FVector2D Here = Middle + (Try < 3 ? Abeam : -Abeam) * Far;
				const double Room = Clear(Here);
				bFound = Room >= 600.0;
				if (bFound || Room > Roomiest)
				{
					Roomiest = Room;
					From = Here;
					Off = Far;
				}
			}
			OutEye = World(From.X, From.Y, 30.0);
			OutLook = World(Middle.X, Middle.Y, 12.0);
			OutLens = FMath::Clamp(FMath::RadiansToDegrees(2.0 * FMath::Atan(0.5 * Span / Off)), 15.0, 55.0);
			return true;
		}
		FVector2D Sum = FVector2D::ZeroVector;
		int32 Count = 0;
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			if (Ship.Doing == SeaShow::Fate::Line && Ship.bShown)
			{
				Sum += FVector2D(Ship.At.X, Ship.At.Y);
				++Count;
			}
		}
		const FVector2D Middle = Count ? Sum / static_cast<double>(Count) : FVector2D(S.Rec.Board.Size * 0.5, S.Rec.Board.Size * 0.5);
		const FVector2D Way = Shot.Spot.Y > 0.5 ? Sun : -Sun;
		// (2 km short of the ships' middle, and farther back until the nearest ship is a kilometre off)
		FVector2D From = Middle - Way * 2000.0;
		for (double Back = 2000.0; Back <= 6000.0 && Clear(From) < 1000.0; Back += 250.0)
		{
			From = Middle - Way * Back;
		}
		OutEye = World(From.X, From.Y, Shot.Spot.X);
		OutLook = World(Middle.X, Middle.Y, 0.0);
		OutLens = 50.0;
		return true;
	}
	case ESeaShot::Under:
	{
		// U1 hook (under water): the director's window under the sea (Battle/SeaUnder): beside the boat at her depth or
		// under her, then on the surface over the pattern; past that, or with the view off or let go, false, so AimEye
		// shows the whole battle as BothLines does past its hold.
		return Under && Under->EyeFor(static_cast<int32>(Shot.Way.X), S.Show, S.BattleT, OutEye, OutLook, OutLens);
	}
	case ESeaShot::BothLines:
	{
		// Both lines in one picture: the eye PlanShots weighed for it, riding on with the player's own ships; past the time
		// it was weighed to hold, the whole battle from above.
		const double Since = FMath::Max(0.0, S.BattleT - (Shot.At - SeaParts::CutBefore));
		if (Since > Shot.Hold)
		{
			return false;
		}
		const FVector Ride(Shot.Way * Since, 0.0);
		const FVector From = Shot.EyeFrom + Ride, To = Shot.LookAt + Ride;
		OutEye = World(From.X, From.Y, From.Z);
		OutLook = World(To.X, To.Y, To.Z);
		OutLens = Shot.Lens;
		return true;
	}
	case ESeaShot::Overview:
	default:
	{
		// High behind the attacker's ships and off to one side, both sides in the picture.
		FVector2D A, B;
		const bool bA = Centre(0, A), bB = Centre(1, B);
		if (!bA && !bB)
		{
			A = B = FVector2D(S.Rec.Board.Size * 0.5, S.Rec.Board.Size * 0.5);
		}
		else if (!bA)
		{
			A = B;
		}
		else if (!bB)
		{
			B = A;
		}
		const FVector2D Middle = (A + B) * 0.5;
		FVector2D Way = (B - A).GetSafeNormal();
		if (Way.IsNearlyZero())
		{
			Way = FVector2D(0.0, 1.0);
		}
		const FVector2D Aside(Way.Y, -Way.X);
		const FVector2D From = A - Way * 2200.0 + Aside * 1500.0;
		OutEye = World(From.X, From.Y, 1300.0);
		OutLook = World(Middle.X, Middle.Y, 0.0);
		OutLens = 55.0;
		return true;
	}
	}
}

void ASeaBattle::AimEye()
{
	if (!Eye || !State.IsValid())
	{
		return;
	}
	FSeaBattleState& S = *State;
	// A picture run stands on its next picture from the moment the last was taken; watched, the camera cuts to each
	// shot a few seconds before its moment.
	int32 Want = -1;
	if (!ShotFolder.IsEmpty())
	{
		Want = FMath::Min(NextPicture, Shots.Num() - 1);
	}
	else
	{
		for (int32 K = 0; K < Shots.Num(); ++K)
		{
			if (S.BattleT >= Shots[K].At - SeaParts::CutBefore)
			{
				Want = K;
			}
		}
		Want = Want < 0 && Shots.Num() > 0 ? 0 : Want;
	}
	FShot Whole;
	Whole.Kind = ESeaShot::Overview;
	FVector From = FVector::ZeroVector, To = FVector::ForwardVector;
	double Lens = 50.0;
	// U1 hook (under water): the window standing in for the shots now (the player's own dive, the director's window
	// watched or in the month's scene; a picture run plays them as its own pictures instead).
	const double WantCut = Shots.IsValidIndex(Want) ? Shots[Want].At - SeaParts::CutBefore : S.EndT + 1e9;
	const int32 Standing = Under ? Under->StandsIn(S.BattleT, WantCut, !ShotFolder.IsEmpty()) : INDEX_NONE;
	bool bUnderEye = Standing != INDEX_NONE && Under->EyeFor(Standing, S.Show, S.BattleT, From, To, Lens);
	if (!bUnderEye && (!Shots.IsValidIndex(Want) || !ShotEye(Shots[Want], From, To, Lens)))
	{
		ShotEye(Whole, From, To, Lens);
	}
	// (the under shots of a picture run go through ShotEye: its eye is under the water too)
	bUnderEye = bUnderEye || (Shots.IsValidIndex(Want) && Shots[Want].Kind == ESeaShot::Under && From.Z < 0.0);
	// (under the water the eye goes below the sea; everywhere else at least 3 m above it) U1 hook (under water)
	if (!(bUnderEye && From.Z < 0.0))
	{
		From.Z = FMath::Max(From.Z, 3.0 * UnitsPerMetre);
	}
	const int32 WasWindow = UnderWindow;
	UnderWindow = bUnderEye ? (Standing != INDEX_NONE ? Standing : static_cast<int32>(Shots[Want].Way.X)) : INDEX_NONE; // U1 hook
	// A cut: a new shot whose eye is not where the last left it, the first, or after a skip. (The ride strip's pictures
	// follow on from each other, the eye carried with her: no cut, the history kept.) U1 hook (under water): through the
	// sea's face, and from one window to another, always a clean cut.
	const FVector Looking = (To - From).GetSafeNormal();
	const bool bMoved = FVector::Dist(From, EyeAt) > 25.0 * UnitsPerMetre || FMath::Abs(Lens - EyeLens) > 2.0 ||
		FVector::DotProduct(Looking, EyeFacing.Vector()) < FMath::Cos(FMath::DegreesToRadians(3.0));
	if (bEyeCut || (Want != ShotNow && bMoved) || ((From.Z < 0.0) != (EyeAt.Z < 0.0)) || (UnderWindow != WasWindow && bMoved))
	{
		bEyeCut = false;
		TicksSinceCut = 0;
		UWorld* Here = GetWorld();
		if (APlayerController* Controller = Here ? Here->GetFirstPlayerController() : nullptr)
		{
			if (Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
			}
		}
	}
	else
	{
		TicksSinceCut = FMath::Min(TicksSinceCut + 1, 1000000);
	}
	EyeAt = From;
	EyeFacing = Looking.Rotation();
	EyeLens = Lens;
	ShotNow = Want;
	// U1 hook (under water): a burst's shock in the eye, on the camera only (never in EyeAt, so no cut comes of it)
	Eye->SetActorLocationAndRotation(EyeAt + (Under ? Under->ShakeAt(S.BattleT) : FVector::ZeroVector), EyeFacing);
	Eye->GetCameraComponent()->SetFieldOfView(static_cast<float>(EyeLens));
}

void ASeaBattle::TakePictures()
{
	if (ShotFolder.IsEmpty() || !State.IsValid())
	{
		return;
	}
	FSeaBattleState& S = *State;
	if (!Shots.IsValidIndex(NextPicture))
	{
		if (QuitAt < 0.0)
		{
			QuitAt = RealT + 1.5;
		}
		else if (RealT >= QuitAt)
		{
			Quit(TEXT("the pictures are taken"));
		}
		return;
	}
	FShot& Shot = Shots[NextPicture];
	if (PictureAt < 0.0 && S.BattleT >= Shot.At && TicksSinceCut >= SeaParts::SettleTicks)
	{
		const FString Path = FPaths::Combine(ShotFolder, RecordKey + TEXT("-") + Shot.Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, true, false); // (with the screen: the bar is in the picture)
		if (FilmCritic::On()) { FilmCritic::Still(Shot.Name); } // film critic: the still's camera (and a strip after it)
		// (and the funnels smoking, the shot's own ship's among them or not: the funnels picture of Samar r2e had none on
		// its hero)
		const uint8* Hers = Shot.Subject >= 0 ? FunnelsChosen.Find(Shot.Subject) : nullptr;
		UE_LOG(LogTheater, Display, TEXT("Sea battle: picture %s at %.1f s (the eye %.0f m up, a %.0f degree lens); %d funnel plumes, %d ships, the subject's %s."), *Shot.Name, S.BattleT,
			EyeAt.Z / UnitsPerMetre, EyeLens, FunnelTrails.Num(), FunnelsChosen.Num(), Hers ? (*Hers == 2 ? TEXT("from each funnel") : TEXT("one plume")) : TEXT("none"));
		PictureAt = RealT;
		++PicturesTaken;
		bLastFrameApart = true;
		if (Shot.Kind == ESeaShot::Water && Shot.Spot.X < 0.0)
		{
			// (the ride strip: each ship's pose as the show gives it, to read the frame against; a ship's pitch plus is by
			// the show's own reckoning, her roll and pitch in degrees, her heave in metres, plus up)
			const double U = S.Show.WindSpeed();
			for (const int32 Key : { Shot.Subject, Shot.Other })
			{
				if (const SeaShow::ShipOut* Ship = Key >= 0 ? S.Show.Ship(Key) : nullptr)
				{
					UE_LOG(LogTheater, Display, TEXT("Sea battle: ride %s, ship %d (%s, %.0f m, %.1f m/s, heading %.0f): heel %+.2f, pitch %+.2f degrees, heave %+.2f m; the wind %.1f m/s, Beaufort %.1f."),
						*Shot.Name, Key, *SeaParts::Wide(Ship->Class), Ship->Long, Ship->Speed, Ship->Heading, Ship->Heel, Ship->Pitch, Ship->Heave, U, FMath::Pow(U / 0.836, 2.0 / 3.0));
				}
			}
		}
	}
	else if (PictureAt >= 0.0 && RealT >= PictureAt + 0.5)
	{
		// (half a second for the picture to be written before the camera moves on)
		Shot.bTaken = true;
		++NextPicture;
		PictureAt = -1.0;
	}
}

void ASeaBattle::SayFrameTimes()
{
	if (bFrameTimesSaid || FrameTimes.Num() == 0)
	{
		return;
	}
	bFrameTimesSaid = true;
	TArray<float> Sorted = FrameTimes;
	Sorted.Sort();
	double Sum = 0.0;
	for (const float T : Sorted)
	{
		Sum += T;
	}
	auto At = [&Sorted](double Share) { return 1000.0 * Sorted[FMath::Clamp(FMath::FloorToInt32(Share * (Sorted.Num() - 1) + 0.5), 0, Sorted.Num() - 1)]; };
	const FIntPoint Size = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport ? GEngine->GameViewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
	UE_LOG(LogTheater, Display, TEXT("Sea battle: frame times over %d frames (%.0f s, %dx%d): mean %.1f ms (%.0f a second), middle %.1f, 95th in the hundred %.1f, 99th %.1f, worst %.1f; %d frames apart (skips ahead, %d pictures)."),
		Sorted.Num(), Sum, Size.X, Size.Y, 1000.0 * Sum / Sorted.Num(), Sum > 0.0 ? Sorted.Num() / Sum : 0.0, At(0.5), At(0.95), At(0.99), At(1.0), FramesLeftOut, PicturesTaken);
	if (Under)
	{
		Under->SayFrameTimes(); // U1 hook (under water): the frames under the water, said apart
	}
	if (State.IsValid())
	{
		const SeaShow::Checks& Kept = State->Show.Checked();
		UE_LOG(LogTheater, Display, TEXT("Sea battle: men in the water %d of %d shown (%.0f%%), %d at once at the most; %d rafts, %d boats, %d floats in all, %d slicks; the wreck meshes %d points at the most."),
			Kept.MenKept, Kept.MenReal, 100.0 * State->Show.MenShare(), Kept.MostMen, Kept.Rafts, Kept.Boats, Kept.Floats, Kept.Slicks, WrecksMostDrawn);
	}
}

void ASeaBattle::Quit(const FString& Why)
{
	SayFrameTimes();
	UE_LOG(LogTheater, Display, TEXT("Sea battle: %s; quitting."), *Why);
	SetActorTickEnabled(false);
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
	FPlatformMisc::RequestExit(false);
}

// ---------------------------------------------------------------------------------------------------------------
// The sound: the battle pack's takes, in the mix's distance families, each heard after its flash.

USoundBase* ASeaBattle::SoundOf(const FName& Name, bool bLoop)
{
	if (TObjectPtr<USoundBase>* Found = Sounds.Find(Name))
	{
		return Found->Get();
	}
	if (SoundsMissing.Contains(Name))
	{
		return nullptr;
	}
	const FString Text = Name.ToString();
	USoundBase* Made = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Campaign/Sounds/Real/%s.%s"), *Text, *Text), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Made)
	{
		SoundsMissing.Add(Name);
		UE_LOG(LogTheater, Display, TEXT("Sea battle: the sound %s is not in /Game/Campaign/Sounds/Real (Tools/build_sounds.sh brings it in)"), *Text);
		return nullptr;
	}
	if (bLoop)
	{
		if (USoundWave* Wave = Cast<USoundWave>(Made))
		{
			Wave->bLooping = true;
		}
	}
	Sounds.Add(Name, Made);
	return Made;
}

USoundAttenuation* ASeaBattle::FamilyOf(const FName& Name)
{
	if (TObjectPtr<USoundAttenuation>* Found = Families.Find(Name))
	{
		return Found->Get();
	}
	const TSharedPtr<FJsonObject>* Distance = nullptr;
	const TSharedPtr<FJsonObject>* Family = nullptr;
	if (!Mix.IsValid() || !Mix->TryGetObjectField(TEXT("distance"), Distance) || !(*Distance)->TryGetObjectField(Name.ToString(), Family))
	{
		return nullptr;
	}
	// A distance family of the mix: full loudness to `full` metres, nothing past `silent`, the top taken off with
	// distance by its lpf points (as the air battle reads it).
	double Full = 50.0, Silent = 1500.0;
	(*Family)->TryGetNumberField(TEXT("full"), Full);
	(*Family)->TryGetNumberField(TEXT("silent"), Silent);
	USoundAttenuation* Made = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& A = Made->Attenuation;
	A.bAttenuate = true;
	A.bSpatialize = true;
	A.AttenuationShape = EAttenuationShape::Sphere;
	A.AttenuationShapeExtents = FVector(Full * UnitsPerMetre, 0.0, 0.0);
	A.FalloffDistance = static_cast<float>(FMath::Max(1.0, Silent - Full) * UnitsPerMetre);
	A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	const TArray<TSharedPtr<FJsonValue>>* Lpf = nullptr;
	if ((*Family)->TryGetArrayField(TEXT("lpf"), Lpf) && Lpf->Num() >= 2)
	{
		const TArray<TSharedPtr<FJsonValue>>& Near = (*Lpf)[1]->AsArray();
		const TArray<TSharedPtr<FJsonValue>>& Far = Lpf->Last()->AsArray();
		if (Near.Num() == 2 && Far.Num() == 2)
		{
			A.bAttenuateWithLPF = true;
			A.LPFRadiusMin = static_cast<float>(Near[0]->AsNumber() * UnitsPerMetre);
			A.LPFRadiusMax = static_cast<float>(Far[0]->AsNumber() * UnitsPerMetre);
			A.LPFFrequencyAtMin = static_cast<float>(Near[1]->AsNumber());
			A.LPFFrequencyAtMax = static_cast<float>(Far[1]->AsNumber());
		}
	}
	Families.Add(Name, Made);
	return Made;
}

UAudioComponent* ASeaBattle::Sound(const FName& Name, const FVector& Where, const FName& Family, float Volume, bool bFlashFirst)
{
	if (bSkipping || Name.IsNone())
	{
		return nullptr;
	}
	if (bFlashFirst && Eye)
	{
		// (flash, then bang: sound crosses the water at 343 m a second; never held more than 8 s)
		const double Metres = FVector::Dist(Where, Eye->GetActorLocation()) / UnitsPerMetre;
		if (Metres > 150.0)
		{
			if (Later.Num() < 64)
			{
				FLater& Held = Later.AddDefaulted_GetRef();
				Held.At = RealT + FMath::Min(Metres / 343.0, 8.0) / Speed;
				Held.Name = Name;
				Held.Where = Where;
				Held.Family = Family;
				Held.Volume = Volume;
			}
			return nullptr;
		}
	}
	USoundBase* Played = SoundOf(Name);
	if (!Played)
	{
		return nullptr;
	}
	UAudioComponent* Heard = UGameplayStatics::SpawnSoundAtLocation(this, Played, Where, FRotator::ZeroRotator, Volume, 1.0f, 0.0f, FamilyOf(Family));
	if (Under)
	{
		Under->Muffle(Heard); // U1 hook (under water): a sound from above the sea dulled under it
	}
	return Heard;
}

void ASeaBattle::GunFired(int32 Key, int32 Slot, const FVector& At)
{
	// Her light guns heard (mix.json air.gunsByCalibre, as the air scene hears a ship's): one battery for each ship and
	// kind of gun (ShipGuns::HeardKey), its takes laid end to end while any of its mounts fires, each only the mix's
	// part of its recording (the firing, without the range's echo), the next due as that part ends.
	const int32 Heard = ShipGuns::HeardKey(Key, Slot);
	GunsFiring.Add(Key, Heard);
	const double* Due = GunHeardAt.Find(Heard);
	if (Due && RealT < *Due)
	{
		return;
	}
	FString Take;
	double Cut = 0.0;
	if (Slot == 1 || Slot == 2)
	{
		Take = SeaParts::MixTake(Mix, TEXT("air.gunsByCalibre.cannon.take"));
		Cut = SeaParts::MixNumber(Mix, TEXT("air.gunsByCalibre.cannon.part"), 0.0);
	}
	else
	{
		Take = SeaParts::MixTake(Mix, TEXT("air.gunsByCalibre.heavy.handHeld"));
		if (!Take.IsEmpty())
		{
			Cut = SeaParts::MixNumber(Mix, FString(TEXT("air.gunsByCalibre.heavy.partByTake.")) + Take, 0.0);
		}
	}
	USoundBase* Wave = Take.IsEmpty() ? nullptr : SoundOf(FName(*Take));
	double Again = 0.9;
	if (Wave)
	{
		const double Whole = Wave->GetDuration();
		Again = Cut > 0.0 ? Cut : (Whole > 0.0 && Whole < 10.0 ? Whole : 0.9);
	}
	Again = FMath::Clamp(Again - 0.02, FMath::Max(0.3, SeaParts::MixNumber(Mix, TEXT("air.sameTakeGap"), 0.15)), 2.0);
	GunHeardAt.Add(Heard, RealT + Again);
	UAudioComponent* Played = Wave ? Sound(FName(*Take), At, TEXT("air_guns"), 1.0f, false) : nullptr;
	if (!Played)
	{
		return;
	}
	if (Cut > 0.0)
	{
		// (only the firing: faded where the mix's part ends)
		TWeakObjectPtr<UAudioComponent> Weak(Played);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
		{
			if (Weak.IsValid())
			{
				Weak->FadeOut(0.08f, 0.0f);
			}
		}), static_cast<float>(Cut), false);
	}
	GunTakes.Add(Heard, Played);
}

void ASeaBattle::GunStopped(int32 Key)
{
	if (Effects)
	{
		Effects->AirGunStop(Key);
	}
	int32 Heard = 0;
	if (!GunsFiring.RemoveAndCopyValue(Key, Heard))
	{
		return;
	}
	for (const TPair<int32, int32>& Other : GunsFiring)
	{
		if (Other.Value == Heard)
		{
			return;
		}
	}
	// (the battery's last mount silent: a take still playing faded at once, not left to run on after the guns stop)
	GunHeardAt.Remove(Heard);
	TWeakObjectPtr<UAudioComponent> Take;
	if (GunTakes.RemoveAndCopyValue(Heard, Take) && Take.IsValid())
	{
		Take->FadeOut(0.08f, 0.0f);
	}
}

void ASeaBattle::Beds()
{
	// The sound director's sea beds (mix.json sea.beds): the sea by day always, the battle's own bed from contact.
	if (!DayBed)
	{
		if (USoundBase* Day = SoundOf(TEXT("sea_day_loop"), true))
		{
			DayBed = UGameplayStatics::SpawnSound2D(this, Day, SeaParts::FromDb(-10.0), 1.0f, 0.0f, nullptr, false, false);
		}
	}
	if (!bBattleBedUp && State.IsValid() && State->Show.ContactAt() >= 0.0 && State->BattleT >= State->Show.ContactAt())
	{
		bBattleBedUp = true;
		if (USoundBase* Fight = SoundOf(TEXT("sea_battle_loop"), true))
		{
			BattleBed = UGameplayStatics::SpawnSound2D(this, Fight, SeaParts::FromDb(-6.0), 1.0f, 0.0f, nullptr, false, false);
			if (BattleBed)
			{
				BattleBed->FadeIn(4.0f, SeaParts::FromDb(-6.0));
			}
		}
	}
	if (Under && State.IsValid())
	{
		// U1 hook (under water): the beds pressed down under the sea and given back to their own levels, the day's whole
		// and the battle's at its fade-in's -6 dB above
		Under->Beds(DayBed.Get(), 1.0f, BattleBed.Get(), SeaParts::FromDb(-6.0), State->Show);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Each frame.

void ASeaBattle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* Here = GetWorld();
	if (!PendingReplay.IsEmpty())
	{
		if (Here && Here->GetTimeSeconds() > 2.0)
		{
			const FString Asked = PendingReplay;
			PendingReplay.Empty();
			if (!BeginReplay(Asked))
			{
				FString Folder;
				if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattleShots="), Folder))
				{
					Quit(TEXT("the recorded action could not be played"));
				}
				SetActorTickEnabled(false);
			}
		}
		return;
	}
	if (!bBegun || !State.IsValid() || !Stage)
	{
		return;
	}
	FSeaBattleState& S = *State;
	const double Dt = FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, 0.1);
	// (the frame before this one, timed on the real clock: the battle's own frames, from its second second on; a gap
	// over half a minute is the game held, not a frame)
	const double TickClock = FPlatformTime::Seconds();
	const double Took = LastTickClock >= 0.0 ? TickClock - LastTickClock : static_cast<double>(DeltaSeconds);
	LastTickClock = TickClock;
	if (bLastFrameApart)
	{
		++FramesLeftOut;
	}
	else if (RealT >= 1.0 && Took < 30.0 && FrameTimes.Num() < 1000000)
	{
		FrameTimes.Add(static_cast<float>(Took));
		if (Under)
		{
			Under->Took(Took); // U1 hook (under water): the frames under the water timed apart too
		}
	}
	bLastFrameApart = false;
	RealT += Dt;
	// A picture run skips the quiet stretch before its next picture, keeping what lasts (ships gone down, oil burning).
	bool bSkippedOn = false;
	if (!ShotFolder.IsEmpty() && Shots.IsValidIndex(NextPicture) && PictureAt < 0.0)
	{
		const double To = FMath::Min(Shots[NextPicture].At - 20.0, S.EndT);
		if (To - S.BattleT > 15.0)
		{
			SkipTo(To);
			bSkippedOn = true;
			UE_LOG(LogTheater, Display, TEXT("Sea battle: on to %.0f s, 20 s before %s."), S.BattleT, *Shots[NextPicture].Name);
		}
	}
	// The month's scene: its windows in turn, at each one's pace, cut between; its caption; its end, or a key's.
	if (bMonthScene)
	{
		if (bMonthEnding || (MonthEndsAt >= 0.0 && RealT >= MonthEndsAt))
		{
			EndMonthScene(); // (the scene is taken down: nothing of it is touched after)
			return;
		}
		bSkippedOn = RunMonthScene() || bSkippedOn;
	}
	// U1 hook (under water): under the sea no faster than one and a half times, so the charges are seen to sink (a
	// picture run and the month's scene keep their own pace: their windows are timed)
	const double Pace = Under ? Under->PaceCap(Speed, bMonthScene || !ShotFolder.IsEmpty()) : Speed;
	StepTo(FMath::Min(S.BattleT + Dt * Pace, S.EndT + 12.0), false);
	MoveKnot(Dt, bSkippedOn);
	for (int32 K = FlakLater.Num() - 1; K >= 0; --K)
	{
		if (FlakLater[K].At <= S.BattleT)
		{
			if (Effects)
			{
				Effects->AirFlak(S.BattleT, FlakLater[K].Where, FlakLater[K].Kind);
			}
			// (the burst heard: the mix's air.flak burst, after its flash, the first of any closer together than its
			// soundEvery; the picture keeps them all)
			if (RealT - FlakHeardAt >= SeaParts::MixNumber(Mix, TEXT("air.flak.soundEvery"), 0.6))
			{
				FlakHeardAt = RealT;
				const FString Bang = SeaParts::MixTake(Mix, TEXT("air.flak.burst"), 0);
				const FString Family = SeaParts::MixTake(Mix, TEXT("air.flak.burstFamily"));
				Sound(FName(Bang.IsEmpty() ? TEXT("flak_burst_01") : *Bang), FlakLater[K].Where, FName(Family.IsEmpty() ? TEXT("explosions") : *Family), 0.8f);
			}
			FlakLater.RemoveAtSwap(K);
		}
	}
	Stage->Advance(S.BattleT);
	DrawPlanes();
	RunTrails();
	DrawMarks();
	AimEye();
	// U1 hook (under water): the water round the eye once it is under the sea's face (its murk and light, the silt, the
	// charges sinking, the sounds) and the U key; through the face, the lamps said again this same frame.
	if (Under)
	{
		Under->Tick(S.Show, S.BattleT, EyeAt, EyeFacing, UnderWindow, bSkipping || bSkippedOn, !bMonthScene && ShotFolder.IsEmpty());
		if (Under->Crossed())
		{
			RoomCheckedAt = -10.0;
		}
	}
	DrawWrecks();
	if (Effects && Eye)
	{
		int32 ViewW = 1600, ViewH = 1000;
		if (APlayerController* Controller = Here ? Here->GetFirstPlayerController() : nullptr)
		{
			Controller->GetViewportSize(ViewW, ViewH);
		}
		Effects->Tick(S.BattleT, Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW, FLinearColor::White, ViewH);
	}
	for (int32 K = Later.Num() - 1; K >= 0; --K)
	{
		if (Later[K].At <= RealT)
		{
			const FLater Due = Later[K];
			Later.RemoveAtSwap(K);
			Sound(Due.Name, Due.Where, Due.Family, Due.Volume, false);
		}
	}
	Beds();
	if (RealT - RoomCheckedAt >= 1.0)
	{
		RoomCheckedAt = RealT;
		LightTheBattle();
	}
	// film critic: the camera as drawn this frame (its shake on), its shot and whether it cut
	if (FilmCritic::On() && Eye) { FilmCritic::Sea(this, *Eye, UnitsPerMetre, DeltaSeconds, S.BattleT, TicksSinceCut == 0, Shots.IsValidIndex(ShotNow) ? Shots[ShotNow].Name : FString(), Shots.IsValidIndex(ShotNow) ? Shots[ShotNow].Subject : -1); }
	TakePictures();
	if (!ShotFolder.IsEmpty() && RealT > 1500.0)
	{
		Quit(TEXT("the picture run took more than 25 minutes"));
	}
}

void ASeaBattle::SkipTo(double To)
{
	if (!State.IsValid())
	{
		return;
	}
	FSeaBattleState& S = *State;
	bSkipping = true;
	for (double T = S.BattleT + 1.0; T < To; T += 1.0)
	{
		StepTo(T, true);
	}
	StepTo(To, true);
	bSkipping = false;
	if (Effects)
	{
		TGuardValue<bool> Water(bOnWater, true);
		for (const FOilLater& Oil : OilLater)
		{
			Effects->OilFire(Oil.Id, World(Oil.At.X, Oil.At.Y, 0.0), static_cast<float>(Oil.Reach), Oil.Kind, Oil.Until, static_cast<float>(Oil.Since + S.BattleT - Oil.LitAt));
		}
	}
	OilLater.Reset();
	FlakLater.Reset();
	Later.Reset();
	bEyeCut = true;
	bLastFrameApart = true;
	if (Under)
	{
		Under->Skipped(); // U1 hook (under water): nothing of what was skipped is heard under the water
	}
}

// ---------------------------------------------------------------------------------------------------------------
// The month's submarine scene (D1, ruling 241): a staged recording played inside the campaign, then closed.

namespace SeaParts
{
	/** The month's scene: the caption's seconds on the screen after the last window, its fade in and out, the most a
	 * window is played faster than the battle (the show is stepped a frame at a time), and the longest the scene is
	 * let run (the form's 30 to 45 s, and the caption, with room). */
	constexpr double CaptionHold = 6.0, CaptionFade = 0.8, FastestWindow = 6.0, LongestScene = 90.0;
}

void ASeaBattle::UnderKey(bool bShift)
{
	// U1 hook (under water): the month scene's screen takes every key; U and Shift+U it hands over here instead of
	// skipping the scene (a modifier alone it lets by).
	if (Under && State.IsValid())
	{
		Under->PressU(bShift, State->Show, State->BattleT, EyeAt);
	}
}

bool ASeaBattle::RunMonthScene()
{
	using namespace SeaParts;
	FSeaBattleState& S = *State;
	const std::vector<SeaFeed::Beat>& Beats = S.Rec.Beats;
	bool bCut = false;
	if (!Beats.empty())
	{
		const int32 Windows = static_cast<int32>(Beats.size());
		while (BeatAt < Windows && S.BattleT >= Beats[BeatAt].To - 1e-6)
		{
			++BeatAt;
		}
		if (BeatAt < Windows)
		{
			const SeaFeed::Beat& Window = Beats[BeatAt];
			// (between windows nothing the scene needs happens: a cut to the next)
			if (Window.From - S.BattleT > 1.0 && Window.From < S.EndT)
			{
				SkipTo(Window.From);
				bCut = true;
			}
			Speed = FMath::Clamp((Window.To - Window.From) / Window.Screen, 0.25, FastestWindow);
			if (BeatAt == Windows - 1 && CaptionAt < 0.0)
			{
				CaptionAt = RealT;
			}
		}
		else
		{
			Speed = 1.0;
			if (CaptionAt < 0.0)
			{
				CaptionAt = RealT;
			}
			if (MonthEndsAt < 0.0)
			{
				MonthEndsAt = RealT + CaptionHold;
			}
		}
	}
	else
	{
		// (a recording without windows: played whole in about 40 s of screen, the caption over its last 8)
		Speed = FMath::Clamp((S.EndT - S.BeginT) / 40.0, 1.0, FastestWindow);
		if (CaptionAt < 0.0 && S.BattleT >= S.EndT - 8.0 * Speed)
		{
			CaptionAt = RealT;
		}
		if (MonthEndsAt < 0.0 && S.BattleT >= S.EndT)
		{
			MonthEndsAt = RealT + CaptionHold;
		}
	}
	if (MonthEndsAt < 0.0 && (S.BattleT >= S.EndT + 10.0 || RealT >= LongestScene))
	{
		MonthEndsAt = RealT + (CaptionAt >= 0.0 ? 2.0 : 0.0);
	}
	return bCut;
}

bool ASeaBattle::GetCaption(TArray<FString>& OutLines, float& OutShown) const
{
	if (!bMonthScene || CaptionAt < 0.0 || Caption.Num() == 0)
	{
		return false;
	}
	double Shown = FMath::Clamp((RealT - CaptionAt) / SeaParts::CaptionFade, 0.0, 1.0);
	if (MonthEndsAt >= 0.0)
	{
		Shown = FMath::Min(Shown, FMath::Clamp((MonthEndsAt - RealT) / SeaParts::CaptionFade, 0.0, 1.0));
	}
	OutLines = Caption;
	OutShown = static_cast<float>(Shown);
	return true;
}

void ASeaBattle::EndMonthScene()
{
	if (!bMonthScene)
	{
		return;
	}
	UE_LOG(LogTheater, Display, TEXT("Sea battle: the month's scene %s, after %.0f s on the screen."), MonthEndsAt >= 0.0 && RealT >= MonthEndsAt ? TEXT("ended") : TEXT("skipped"), RealT);
	// (the screen, the table, the music and the campaign's own eye given back; EndPlay gives back the room's light and
	// the frame. The keys and the pointer go to a guard over the map first, with no frame between, which gives them
	// back to the campaign once the key or click that ended the scene is let go)
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (bOwnInput && Controller)
	{
		bOwnInput = false;
		SSeaKeyGuard::Raise(Controller);
	}
	HideScreen();
	if (Controller)
	{
		Controller->SetViewTarget(Controller->GetPawn());
	}
	Destroy();
}

void ASeaBattle::EndPlay(const EEndPlayReason::Type Reason)
{
	SayFrameTimes();
	if (Under) // U1 hook (under water): the haze, the lens and the lamps given back before the room is
	{
		Under->End();
		Under.Reset();
	}
	HideScreen();
	for (UAudioComponent* Bed : { DayBed.Get(), BattleBed.Get() })
	{
		if (Bed)
		{
			Bed->Stop();
		}
	}
	if (Reason == EEndPlayReason::Destroyed)
	{
		GiveBackTheRoom(); // (taken down in the game: the table comes back as it was)
	}
	if (bTablePutAway)
	{
		bTablePutAway = false;
		if (ACampaignMapActor* Map = Cast<ACampaignMapActor>(UGameplayStatics::GetActorOfClass(this, ACampaignMapActor::StaticClass())))
		{
			Map->SetPutAway(false);
		}
	}
	if (bMonthScene)
	{
		bMonthScene = false;
		if (ACampaignGameMode* Campaign = Cast<ACampaignGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			Campaign->DuckForScene(false);
		}
	}
	// (the stage first: its looks play on this scene's kit)
	if (Stage)
	{
		Stage->Destroy();
		Stage = nullptr;
	}
	if (Effects)
	{
		for (const TPair<int32, int32>& Firing : GunsFiring)
		{
			Effects->AirGunStop(Firing.Key);
		}
		Effects->End();
		Effects.Reset();
	}
	GunsFiring.Reset();
	GunHeardAt.Reset();
	GunTakes.Reset();
	if (Eye)
	{
		Eye->Destroy();
		Eye = nullptr;
	}
	State.Reset();
	Super::EndPlay(Reason);
}

bool ASeaBattle::GetBar(double& OutKnot, UTexture2D*& OutMine, UTexture2D*& OutTheirs) const
{
	if (!bBegun || !bBarGiven || !State.IsValid())
	{
		return false;
	}
	OutKnot = BarKnot;
	OutMine = MyFlag.Get();
	OutTheirs = TheirFlag.Get();
	return true;
}

void ASeaBattle::MoveKnot(double RealDt, bool bAtOnce)
{
	if (!State.IsValid())
	{
		return;
	}
	const FSeaBattleState& S = *State;
	// The view the picture has reached (never one ahead of it: the bar follows what is seen).
	size_t Upto = FMath::Min(S.Taken, S.Rec.Views.size());
	while (Upto > 1 && S.Rec.Views[Upto - 1].Head.T > S.BattleT + 1e-9)
	{
		--Upto;
	}
	const double Want = Upto > 0 ? SeaFeed::KnotOf(S.Rec.Views[Upto - 1].Head) : -1.0;
	bBarGiven = Want >= 0.0;
	if (!bBarGiven)
	{
		return;
	}
	// (a lag on the screen's own clock, not the battle's: at any pace the knot takes about a second and a half to arrive)
	constexpr double Lag = 0.5;
	BarKnot = bAtOnce ? Want : BarKnot + (Want - BarKnot) * (1.0 - FMath::Exp(-FMath::Max(RealDt, 0.0) / Lag));
}

void ASeaBattle::ShowScreen()
{
	if (!GEngine || !GEngine->GameViewport || Screen.IsValid())
	{
		return;
	}
	Screen = SNew(SSeaBattleScreen).Battle(this);
	// A quarter larger than Unreal's own game-screen size, as the air screen's (the art director on build 1734), so the
	// two battles' bars stand the same size; "-TheaterSeaTextScale=" tunes it. The screen takes no input: the scene's
	// own controls stay as they are.
	float Lettering = 1.25f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaTextScale="), Lettering);
	Lettering = FMath::Clamp(Lettering, 0.75f, 2.0f);
	ScreenFrame = SNew(SDPIScaler).DPIScale(Lettering)[Screen.ToSharedRef()];
	GEngine->GameViewport->AddViewportWidgetContent(ScreenFrame.ToSharedRef(), 20);
}

void ASeaBattle::HideScreen()
{
	if (ScreenFrame.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ScreenFrame.ToSharedRef());
	}
	ScreenFrame.Reset();
	Screen.Reset();
	if (bOwnInput)
	{
		bOwnInput = false;
		if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			// (the campaign's own mode, as its controller sets it: the pointer in sight while a drag turns the table, and
			// never locked in)
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Controller->SetInputMode(Mode);
		}
	}
}
