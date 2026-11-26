#include "Battle/AirBattle.h"

#include "AudioMixerBlueprintLibrary.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"

#include "Battle/AirBattleScreen.h"
#include "Battle/AirBattleState.h"
#include "Battle/AirGround.h"
#include "Battle/AirLight.h"
#include "Battle/AirLand.h"
#include "Battle/AirScrew.h"
#include "Battle/AirStage.h"
#include "Battle/LandLive.h"
#include "Battle/ShipGuns.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignBattleLook.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignGameMode.h"
#include "Campaign/CampaignHUD.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignSession.h"
#include "Campaign/CampaignSounds.h"
#include "Campaign/CampaignTextures.h"
#include "Campaign/CampaignUnitTable.h"
#include "Campaign/FilmCritic.h" // film critic: the development facts (-FilmCritic)
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Texture2D.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/Material.h"
#include "RenderingThread.h"
#include "CoreGlobals.h" // (GGameThreadTime: the opening held, WarmTick)
#include "RenderTimer.h" // (GRenderThreadTime, as TheaterSpeed reads it)
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "AudioDevice.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "StaticMeshAttributes.h"
#include "SubmixEffects/AudioMixerSubmixEffectDynamicsProcessor.h"
#include "TheaterCommand.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SDPIScaler.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <memory>

/** The ground's pictures as they are painted aside (AAirBattle::PaintLand): the painter, its three pictures and each
 * one's smaller copies, ready for CampaignTextures::CreateFromLevels on the game thread. */
struct FAirLandMade
{
	std::unique_ptr<AirLand::Painter> Painter;
	std::vector<AirLand::Frame> Frames;
	TArray<CampaignTextures::FLevels> Levels;
	double Seconds = 0.0;
};

/** (1.5.1) The opening held (AAirBattle::WarmUp, WarmTick). */
namespace AirParts
{
	/** How long the opening is held at least and at most (s); the frames in a row that must come quick, and how quick
	 * (ms, the frame and both threads). The stalls it waits out were 600 to 1,200 ms; a flying frame on the Mac 20 to 40. */
	static constexpr double WarmLeast = 0.8;
	static constexpr double WarmMost = 8.0;
	static constexpr int32 WarmQuickWanted = 8;
	static constexpr double WarmQuickMs = 80.0;
	/** The black's easing off as the flyover begins (s). */
	static constexpr double CurtainFade = 0.5;
}

namespace AirParts
{
	/** GS6: the clouds' table (AirSky::Chip, its first Rows rows, four floats a texel) sent to its float picture; the copy
	 * is the renderer's until it has read it. */
	static void SendTable(UTexture2D* Picture, const std::vector<float>& Cells, int32 Rows)
	{
		if (!Picture || !Picture->GetResource() || Rows <= 0)
		{
			return;
		}
		const int32 Wide = Picture->GetSizeX();
		Rows = FMath::Min(Rows, Picture->GetSizeY());
		const size_t Floats = static_cast<size_t>(Wide) * static_cast<size_t>(Rows) * 4;
		if (Cells.size() < Floats)
		{
			return;
		}
		float* Copy = static_cast<float*>(FMemory::Malloc(Floats * sizeof(float)));
		FMemory::Memcpy(Copy, Cells.data(), Floats * sizeof(float));
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Wide, Rows);
		Picture->UpdateTextureRegions(0, 1, Region, Wide * 4 * sizeof(float), 4 * sizeof(float), reinterpret_cast<uint8*>(Copy),
			[](uint8* Data, const FUpdateTextureRegion2D* Regions)
			{
				FMemory::Free(Data);
				delete Regions;
			});
	}

	/** GS6: a painter of the clouds' shadows alone, for a sky the chip draws: it owns none of the clouds (its part takes
	 * none), so it lays only their shadows, every one the far room lays, point for point (the cloud test), and turns as
	 * loosely as the far room. */
	static void ShadowsAlone(AirSky::Painter& Shade, double Ink)
	{
		Shade = AirSky::Painter();
		AirSky::Part None;
		None.bAll = false;
		None.bNear = false;
		None.Split = 1e18;
		Shade.SetPart(None);
		Shade.SetRoom(1);
		Shade.SetInk(Ink);
		Shade.SetLoose(0.0005, 0.04);
	}

	/** An aircraft casts its shadow only within ShadowFarM of the eye (AS1 of the production lead's battle-scale plan,
	 * 22 November: past a kilometre its shadow is under a pixel on its own skin and kilometres below it on the ground,
	 * yet every one that moves redraws the shadow maps each frame); one on the ground keeps it out to ShadowGroundM,
	 * for parked aircraft stand on their shadows in the strategy view. A tenth more before one already casting stops.
	 * "-TheaterPlaneShadowM=1500" tries another reach in the air (the ground's scaled with it; 0: none cast). */
	constexpr double ShadowFarM = 1000.0, ShadowGroundM = 3000.0;
	static double ShadowScale()
	{
		static const double Scale = []
		{
			double Metres = ShadowFarM;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterPlaneShadowM="), Metres);
			const double Asked = FMath::Clamp(Metres, 0.0, 100000.0) / ShadowFarM;
			if (Asked != 1.0)
			{
				UE_LOG(LogTheater, Display, TEXT("Air battle: aircraft cast shadows within %.0f m (%.0f m on the ground)."), ShadowFarM * Asked, ShadowGroundM * Asked);
			}
			return Asked;
		}();
		return Scale;
	}

	static FString Wide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	/** The sheet of cloud shapes (AirSky::MakeFragments, some 190 ms of work): made once a run on the engine's pool of
	 * threads, begun as the first battle opens (Open) and waited for when its sky is laid, most of it done by then. */
	struct FSheetMaking
	{
		TFuture<void> Done;
		std::vector<uint8_t> Rgba;
		bool bBegun = false;
	};
	static FSheetMaking& SheetMaking()
	{
		static FSheetMaking Making;
		return Making;
	}
	static void BeginSheet()
	{
		FSheetMaking& Making = SheetMaking();
		if (!Making.bBegun)
		{
			Making.bBegun = true;
			Making.Done = Async(EAsyncExecution::ThreadPool, [&Making]() { AirSky::MakeFragments(Making.Rgba); });
		}
	}
	static const std::vector<uint8_t>& WaitSheet()
	{
		BeginSheet();
		FSheetMaking& Making = SheetMaking();
		if (Making.Done.IsValid())
		{
			Making.Done.Wait();
		}
		return Making.Rgba;
	}

	/** The painter's four numbers a point (AirSky::Painter::SetSprites) as the mesh's two texture coordinates: the
	 * point's place on the sheet, and how much of the sun is behind the eye and ahead. Count points; any the painter
	 * gave none, at the sheet's corner (no cloud there). */
	static void SheetPlaces(const std::vector<float>& From, TArray<FVector2D>& Places, TArray<FVector2D>& Ways, int32 Count)
	{
		Places.SetNumUninitialized(Count);
		Ways.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const size_t At = static_cast<size_t>(Index) * 4;
			if (At + 3 < From.size())
			{
				Places[Index] = FVector2D(From[At], From[At + 1]);
				Ways[Index] = FVector2D(From[At + 2], From[At + 3]);
			}
			else
			{
				Places[Index] = FVector2D::ZeroVector;
				Ways[Index] = FVector2D::ZeroVector;
			}
		}
	}

	/** The effects kit's nation rows (us, gb, de, it, jp, su, agreed with the effects thread 17:34Z) from a
	 * recording's nation (USA, UK, GER, ITA, JAP, SOV): empty for any other, which the kit draws in its default. */
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

	static std::string Narrow(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		return std::string(Utf8.Get(), static_cast<size_t>(Utf8.Length()));
	}

	/**
	 * The effects kit's calls that came after unreal a7f7f7a (the effects thread, kit heads d359a18 and 1295edc): made
	 * where the kit has them, so the scene builds on either kit. With the newer kit, an air scene draws its guns, hits
	 * and flames out to 4 km, and a burning aircraft's flame is sized to its span.
	 */
	template <typename TKit>
	static void UseAirDistance(TKit* Kit)
	{
		if constexpr (requires(TKit* K) { K->UseAirDistance(); })
		{
			Kit->UseAirDistance();
		}
	}

	template <typename TKit>
	static void TrailStart(TKit* Kit, double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres)
	{
		if constexpr (requires(TKit* K) { K->TrailStart(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f); })
		{
			Kit->TrailStart(Now, Id, At, Velocity, Kind, SpanMetres);
		}
		else
		{
			Kit->TrailStart(Now, Id, At, Velocity, Kind);
		}
	}

	/** A gun firing, with its aircraft's nation where the kit takes it (the art director's G2, 15:38Z: Allied streams
	 * red-orange, German and Italian pale yellow-white, Japanese near white, Soviet green; the effects thread's call,
	 * the nation last, as KitNation names it: us, gb, de, it, jp, su). An older kit draws its one colour. */
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

	/** Whether the effects kit's recipe (Content/Effects/battle-fx.json) turns on its far forms, game.air_far: a trail
	 * past 1.2 km in fewer, longer pieces (trails_on), a gun past 1.5 km as a dash a burst (guns_on). Read once. */
	static double FarSwitch(const TCHAR* Key)
	{
		static const TSharedPtr<FJsonObject> Far = []
		{
			FString Text;
			TSharedPtr<FJsonObject> Recipe;
			const TSharedPtr<FJsonObject>* Game = nullptr;
			const TSharedPtr<FJsonObject>* Out = nullptr;
			if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Effects/battle-fx.json"))) &&
				FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) && Recipe.IsValid() &&
				Recipe->TryGetObjectField(TEXT("game"), Game) && (*Game)->TryGetObjectField(TEXT("air_far"), Out))
			{
				return *Out;
			}
			return TSharedPtr<FJsonObject>();
		}();
		double Value = 0.0;
		return Far.IsValid() && Far->TryGetNumberField(Key, Value) ? Value : 0.0;
	}
	static bool FarTrailsOn() { return FarSwitch(TEXT("trails_on")) >= 0.5; }
	static bool FarGunsOn() { return FarSwitch(TEXT("guns_on")) >= 0.5; }

	/** The kit from 1295edc (TrailStart with a span) keeps every flame on a full screen and sheds the farthest smoke
	 * first, so the scene may light 24 trails at once on it; 10 on an older kit (the pre-check, 06:08Z). */
	template <typename TKit>
	static constexpr bool HasTrailSpan(const TKit*)
	{
		return requires(TKit* K) { K->TrailStart(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f); };
	}

	/** A bomb's burst where the flight core lands it (the effects kit's BombStick, 01f2dfc7: inside the kit's draw range
	 * the ordinary burst ShellOfSize lays, beyond it a cheaper far form, cross-fading between, so a far raid's carpets are
	 * seen). One bomb to a call, on the core's own spot at its own landing: Count 1, no length, OverSeconds 0, laid at the
	 * kit's next Tick and its id free the frame after (the effects thread, 07:00Z). An older kit lays the near burst. */
	template <typename TKit>
	static void BombBurst(TKit* Kit, double Now, int32 Id, const FVector& At, const FString& Size, const FString& Ground)
	{
		if constexpr (requires(TKit* K) { K->BombStick(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, 0, FString(), FString(), 0.0f); })
		{
			Kit->BombStick(Now, Id, At, At, 1, Size, Ground, 0.0f);
		}
		else
		{
			Kit->ShellOfSize(At, Size, Ground);
		}
	}

	/** The table's put-away (hook lines for CampaignMapActor in Battle/AIR-SCENE.md): while it is set, every piece stays
	 * off the table whatever the hover asks, and the table is busy, so the hover falls quiet. False where the map has
	 * none yet; the scene then keeps the pieces hidden itself. */
	template <typename TMap>
	static bool PutAway(TMap* Map, bool bAway)
	{
		if constexpr (requires(TMap* M) { M->SetPutAway(true); })
		{
			if (Map)
			{
				Map->SetPutAway(bAway);
				return true;
			}
		}
		return false;
	}

	/** The air battle's haze colour (the fog's inscattering, set by HideTheTable; the haze band takes it too). */
	static const FLinearColor AirHazeInk(0.55f, 0.66f, 0.78f);

	/** The haze's colour over the sea: bluer and a little dimmer than AirHazeInk, so that it reads pale blue and not
	 * white (the art director on the Mac's release 2 Midway 01 and Oahu 02, 19:32Z: the haze there read 220 to 225
	 * with blue over red by 5, and filled the top 40% of the picture; she asks blue over red by 10 to 20, white only at
	 * the horizon, where the haze band stands). Bluer again, 22 November: on 2b177386's frames the horizon read
	 * (215,218,225), "only +10 blue", where pale blue is about (196,212,230); the haze band over the sea takes it too.
	 * Bluer and greener again on a1acb552's frames: at 0.40, 0.53, 0.76 (with the air's dust at 0.6 over the sea) the
	 * horizon read (193,201,212), the art director asking blue +18 and green +10, the fade's span as it is (22 November
	 * 08:13Z); about 145 levels a unit of the ink there, as the last step moved it.
	 * "-TheaterSeaHazeInk=0.42,0.60,0.88" tries another. */
	static FLinearColor SeaHazeInk()
	{
		double R = 0.42, G = 0.60, B = 0.88;
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaHazeInk="), Asked, false))
		{
			TArray<FString> Parts;
			Asked.ParseIntoArray(Parts, TEXT(","), false);
			double* const Into[] = { &R, &G, &B };
			for (int32 K = 0; K < Parts.Num() && K < 3; ++K)
			{
				if (!Parts[K].TrimStartAndEnd().IsEmpty())
				{
					*Into[K] = FMath::Clamp(FCString::Atod(*Parts[K]), 0.0, 4.0);
				}
			}
		}
		return FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B));
	}

	/** The sky behind the fight as the SCREEN shows it (pixel / 255), for the effects kit's SetSky: old flak and aircraft
	 * smoke pales into it as it thins (the effects kit's 2a200875; without it, into a mid grey). Measured on the Mac's
	 * release 2 frames at the fights' heights: over Bremen's deck 219, 224, 230. Over the sea those frames read 220,
	 * 221, 224, a milky haze the art director asked to go pale blue, blue over red by 10 to 20 (SeaHazeInk since r2c),
	 * so the sea's is that: 202, 209, 221. Under a deck the day greys toward 172, 175, 180 by how far under its base the
	 * eye is and how closed it is (Grey, 0 to 1); by night 32, 36, 48, under a deck 22, 23, 26.
	 * "-TheaterSmokeSky=200,208,220" (0 to 255) sets it outright. */
	static FLinearColor SkyBehind(bool bNight, bool bSea, double ToGrey)
	{
		static const FLinearColor Asked = []
		{
			FString Said;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSmokeSky="), Said, false) && Said.ParseIntoArray(Parts, TEXT(","), false) == 3)
			{
				return FLinearColor(FMath::Clamp(FCString::Atof(*Parts[0]), 0.0f, 255.0f) / 255.0f, FMath::Clamp(FCString::Atof(*Parts[1]), 0.0f, 255.0f) / 255.0f,
					FMath::Clamp(FCString::Atof(*Parts[2]), 0.0f, 255.0f) / 255.0f);
			}
			return FLinearColor(-1.0f, -1.0f, -1.0f);
		}();
		if (Asked.R >= 0.0f)
		{
			return Asked;
		}
		const float Grey = static_cast<float>(FMath::Clamp(ToGrey, 0.0, 1.0));
		const FLinearColor Clear = bNight ? FLinearColor(32.0f / 255.0f, 36.0f / 255.0f, 48.0f / 255.0f)
			: bSea ? FLinearColor(202.0f / 255.0f, 209.0f / 255.0f, 221.0f / 255.0f) : FLinearColor(219.0f / 255.0f, 224.0f / 255.0f, 230.0f / 255.0f);
		const FLinearColor Overcast = bNight ? FLinearColor(22.0f / 255.0f, 23.0f / 255.0f, 26.0f / 255.0f) : FLinearColor(172.0f / 255.0f, 175.0f / 255.0f, 180.0f / 255.0f);
		return FMath::Lerp(Clear, Overcast, Grey);
	}

	/** A haze's colour, by the name this engine gives it. */
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

	/** The kit's falling debris (kit head 44af184): whether this kit has it. */
	template <typename TKit>
	static constexpr bool HasAirFall(const TKit*)
	{
		return requires(TKit* K) { K->AirFall(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f, 0.0); };
	}

	/** A lost aircraft's debris row: once as it falls (Kind explode, burn or spin; DeckZ the world height where its
	 * pieces leave sight), then every frame of a burning or spinning fall, then when the model is gone. */
	template <typename TKit>
	static void AirFall(TKit* Kit, double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres, double DeckZ)
	{
		if constexpr (requires(TKit* K) { K->AirFall(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f, 0.0); })
		{
			Kit->AirFall(Now, Id, At, Velocity, Kind, SpanMetres, DeckZ);
		}
	}

	template <typename TKit>
	static void AirFallMove(TKit* Kit, double Now, int32 Id, const FVector& At, const FVector& Velocity)
	{
		if constexpr (requires(TKit* K) { K->AirFallMove(0.0, 0, FVector::ZeroVector, FVector::ZeroVector); })
		{
			Kit->AirFallMove(Now, Id, At, Velocity);
		}
	}

	template <typename TKit>
	static void AirFallStop(TKit* Kit, int32 Id)
	{
		if constexpr (requires(TKit* K) { K->AirFallStop(0); })
		{
			Kit->AirFallStop(Id);
		}
	}

	/** The way the campaign hides its own frame while a battle is up (CampaignHUD's ShowFrame, a hook line the 3D
	 * builder adds: see Battle/AIR-SCENE.md). Without it the frame stays under the battle's screen. */
	template <typename THud>
	static void ShowFrame(THud* Hud, bool bShow)
	{
		if constexpr (requires(THud* H) { H->ShowFrame(true); })
		{
			if (Hud)
			{
				Hud->ShowFrame(bShow);
			}
		}
	}

	/** A number from the mix, at a path of fields ("air.flak.soundEvery"), or Default. */
	static double MixNumber(const TSharedPtr<FJsonObject>& Mix, const FString& Path, double Default)
	{
		TArray<FString> Steps;
		Path.ParseIntoArray(Steps, TEXT("."));
		TSharedPtr<FJsonObject> At = Mix;
		for (int32 K = 0; At.IsValid() && K < Steps.Num(); ++K)
		{
			if (K == Steps.Num() - 1)
			{
				double Value = Default;
				return At->TryGetNumberField(Steps[K], Value) ? Value : Default;
			}
			const TSharedPtr<FJsonObject>* Next = nullptr;
			At = At->TryGetObjectField(Steps[K], Next) ? *Next : nullptr;
		}
		return Default;
	}

	/** A true/false from the mix, at a path of fields ("buses.master.limiterAnalogDetector"), or Default. */
	static bool MixFlag(const TSharedPtr<FJsonObject>& Mix, const FString& Path, bool Default)
	{
		TArray<FString> Steps;
		Path.ParseIntoArray(Steps, TEXT("."));
		TSharedPtr<FJsonObject> At = Mix;
		for (int32 K = 0; At.IsValid() && K < Steps.Num(); ++K)
		{
			if (K == Steps.Num() - 1)
			{
				bool Value = Default;
				return At->TryGetBoolField(Steps[K], Value) ? Value : Default;
			}
			const TSharedPtr<FJsonObject>* Next = nullptr;
			At = At->TryGetObjectField(Steps[K], Next) ? *Next : nullptr;
		}
		return Default;
	}

	/** How many engines an aircraft of this model has (mix.json air.engineLost: its drone drops as they die). */
	static int32 EnginesOf(const std::string& Model, bool bFighter)
	{
		// (as many as the flight core gives the aircraft's shape, AirShow::ShapeOf, so that its dead engines, numbered on
		// that shape, are counted out of the same: a B-24 or an unnamed twin flies on what the shape gives it)
		static std::map<std::pair<std::string, bool>, int32> Known;
		const std::pair<std::string, bool> Which(Model, bFighter);
		auto Found = Known.find(Which);
		if (Found == Known.end())
		{
			Found = Known.emplace(Which, FMath::Max(1, static_cast<int32>(AirShow::ShapeOf(Model, bFighter).Engines.size()))).first;
		}
		return Found->second;
	}

	/** An aircraft's engines still turning, as a share of its drone's energy: 10 log10(left / engines) dB, as a gain. */
	static float LiveEngines(const std::string& Model, bool bFighter, const std::vector<int>& Dead, int32& OutLeft, int32& OutAll)
	{
		OutAll = EnginesOf(Model, bFighter);
		OutLeft = FMath::Max(0, OutAll - static_cast<int32>(Dead.size()));
		return FMath::Sqrt(static_cast<float>(OutLeft) / static_cast<float>(FMath::Max(1, OutAll)));
	}

	static TSharedPtr<FJsonObject> MixObject(const TSharedPtr<FJsonObject>& Mix, const FString& Path)
	{
		TArray<FString> Steps;
		Path.ParseIntoArray(Steps, TEXT("."));
		TSharedPtr<FJsonObject> At = Mix;
		for (const FString& Step : Steps)
		{
			const TSharedPtr<FJsonObject>* Next = nullptr;
			if (!At.IsValid() || !At->TryGetObjectField(Step, Next))
			{
				return nullptr;
			}
			At = *Next;
		}
		return At;
	}

	/** A string, or one of a list of strings (picked by Pick), at a field of an object of the mix. */
	static FString MixTake(const TSharedPtr<FJsonObject>& Object, const FString& Field, int32 Pick = 0)
	{
		if (!Object.IsValid())
		{
			return FString();
		}
		FString One;
		if (Object->TryGetStringField(Field, One))
		{
			return One;
		}
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Object->TryGetArrayField(Field, List) && List->Num() > 0)
		{
			return (*List)[FMath::Abs(Pick) % List->Num()]->AsString();
		}
		return FString();
	}

	static double Smooth(double Real, double Seconds)
	{
		return 1.0 - FMath::Exp(-Real / FMath::Max(0.01, Seconds));
	}

	/** A cut, told to the renderer: this frame starts afresh, so the last shot's sharpening, light and exposure are not
	 * carried into the new one (the camera and film director's note of 22 November: at each cut the upscaler's history,
	 * the bounced light and the eye's adaptation smeared cloud, flak smoke and light for a frame or a few). */
	static void TellCut(UWorld* Here)
	{
		if (FilmCritic::On()) { FilmCritic::NoteCut(); } // film critic: the frame's camera line says it is a cut
		if (APlayerController* Controller = Here ? Here->GetFirstPlayerController() : nullptr)
		{
			if (Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
			}
		}
	}

	// The paint of the small things: a canopy's silk, an airman, torn skin, a bomb's casing.
	static const FLinearColor Silk(0.93f, 0.92f, 0.88f);
	static const FLinearColor Airman(0.16f, 0.14f, 0.11f);
	static const FLinearColor Skin(0.48f, 0.49f, 0.47f);
	static const FLinearColor Scorched(0.07f, 0.065f, 0.06f);
	static const FLinearColor Casing(0.12f, 0.13f, 0.12f);
}

AAirBattle::AAirBattle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

namespace
{
	/** A ship's name (else her class) by her key, and whether she is a battleship or a carrier: the guns fire back
	 * picture's ship and its log line. */
	void NoteShip(TMap<int32, FString>& Names, TSet<int32>& Capitals, const AirShow::Ship& Ship)
	{
		if (!Ship.Name.empty() || !Names.Contains(Ship.Id))
		{
			Names.Add(Ship.Id, AirParts::Wide(Ship.Name.empty() ? "a " + Ship.Class : Ship.Name));
		}
		if (Ship.Class == "bb" || Ship.Class == "bc" || Ship.Class == "cv" || Ship.Class == "cvl")
		{
			Capitals.Add(Ship.Id);
		}
	}
}

bool AAirBattle::WantsReplay(FString& OutKey)
{
	return FParse::Value(FCommandLine::Get(), TEXT("TheaterAirBattle="), OutKey) && !OutKey.IsEmpty();
}

// (1.5.1; Brian, 24 November, on the film of a raid flown from the campaign: "why is the plane shaking in the test
// video?") The flight core's smooth flying is on (AirShow::Show::SetSmooth: the roll eased in and out, a formation's
// velocity read over a battle second). "-TheaterAirOldFlying" flies every air battle as 1.5 flew it, for a film of the
// one beside the other.
static bool AirOldFlying()
{
	static const bool bAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterAirOldFlying"));
	return bAsked;
}

void AAirBattle::ReplayWhenReady(const FString& Key)
{
	PendingReplay = Key;
	SetActorTickEnabled(true);
}

bool AAirBattle::BeginReplay(const FString& Key)
{
	FParse::Value(FCommandLine::Get(), TEXT("TheaterAirBattleShots="), ShotFolder);
	// (the builder's film of a picture run: "-TheaterAirBattleRideHold=40" stays on each ride 40 s after its picture)
	float Hold = 0.5f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterAirBattleRideHold="), Hold);
	RideHold = FMath::Clamp(static_cast<double>(Hold), 0.5, 300.0);
	const FString File = FPaths::ProjectContentDir() / TEXT("Data/AirBattles") / Key + TEXT(".json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: no recorded battle %s (is Content/Data/AirBattles staged with the build?)"), *File);
		if (!ShotFolder.IsEmpty())
		{
			QuitAt = 0.0;
			SetActorTickEnabled(true);
		}
		return false;
	}
	SkyFinish(false); // (a turn of the clouds painted aside on a battle begun before: done before its state goes)
	State = MakeShared<FAirBattleState>();
	std::string Error;
	if (!AirFeed::ReadRecording(AirParts::Narrow(Text), State->Recording, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: %s: %s"), *File, *AirParts::Wide(Error));
		State.Reset();
		if (!ShotFolder.IsEmpty())
		{
			QuitAt = 0.0;
			SetActorTickEnabled(true);
		}
		return false;
	}
	FAirBattleState& S = *State;
	S.bReplay = true;
	S.Board = S.Recording.Board;
	::UnitTable::SetMonth(S.Board.Year, S.Board.Month + 1); // (its aircraft wear the paint of its month; the feed's months count from 0)
	S.Show = AirShow::Show(S.Recording.Seed ? S.Recording.Seed : 238);
	AirFeed::Apply(S.Board, S.Show);
	S.Show.SetSmooth(!AirOldFlying());
	S.SkyPlace = AirFeed::PlacingOf(S.Board, S.Recording.Views.front().Groups, S.Recording.Seed);
	S.Desk.Begin(S.Board, S.Recording.Views.front().Head.Me);
	S.Card = S.Desk.CardOf(S.Recording.Card);
	S.BattleT = S.Recording.Views.front().Head.T;
	return Open(Key);
}

bool AAirBattle::BeginLive(FCampaignSession* InSession, const FString& Kind, int32 Target, const TArray<int32>& Ids)
{
	TArray<FString> Numbers;
	for (const int32 Id : Ids)
	{
		Numbers.Add(FString::FromInt(Id));
	}
	return BeginLiveCall(InSession, TEXT("airStart"), FString::Printf(TEXT("[\"%s\",%d,[%s]]"), *Kind, Target, *FString::Join(Numbers, TEXT(","))));
}

bool AAirBattle::BeginLiveCall(FCampaignSession* InSession, const FString& Function, const FString& ArgumentsJson)
{
	Session = InSession;
	if (!Session)
	{
		return false;
	}
	FString Reply, Error;
	if (!Session->CallBattle(Function, ArgumentsJson, Reply, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: TC.%s failed: %s"), *Function, *Error);
		return false;
	}
	if (Reply.IsEmpty() || Reply == TEXT("null"))
	{
		// (a battle of your own its rules would not open: the setup page says why, from TC.customWhy)
		UE_LOG(LogTheater, Warning, TEXT("Air battle: TC.%s opened no battle"), *Function);
		return false;
	}
	SkyFinish(false); // (a turn of the clouds painted aside on a battle begun before: done before its state goes)
	State = MakeShared<FAirBattleState>();
	FAirBattleState& S = *State;
	AirFeed::Json Opened;
	std::string Why;
	AirFeed::Frame First;
	if (!AirFeed::Parse(AirParts::Narrow(Reply), Opened, Why) || !AirFeed::ReadSky(Opened["sky"], S.Board) || !AirFeed::ReadView(Opened["view"], S.Board, First))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: TC.airStart's answer could not be read (%s)"), *AirParts::Wide(Why));
		State.Reset();
		return false;
	}
	S.bReplay = false;
	::UnitTable::SetMonth(S.Board.Year, S.Board.Month + 1); // (its aircraft wear the paint of its month; the feed's months count from 0)
	AirFeed::Apply(S.Board, S.Show);
	S.Show.SetSmooth(!AirOldFlying());
	S.SkyPlace = AirFeed::PlacingOf(S.Board, First.Groups, 0);
	S.Desk.Begin(S.Board, First.Head.Me);
	S.Show.Take(First.Head.T, First.Groups, First.Events, First.Ships);
	for (const AirShow::Ship& Ship : First.Ships)
	{
		// (a ship's light guns' tracers in her navy's colours: their keys name her as an aircraft's name it)
		GunNations.Add(ShipGuns::OwnerOf(Ship.Id), AirParts::KitNation(Ship.Nation));
		NoteShip(ShipNames, CapitalShips, Ship);
	}
	S.Desk.Take(First);
	S.BattleT = S.FedT = First.Head.T;
	// (the bridge's battle opens paused: it is run here a battle second at a time, ahead of what is shown)
	FString Ignored;
	Session->CallBattle(TEXT("battleSpeed"), TEXT("[1]"), Ignored, Error);
	if (!Open(TEXT("live")))
	{
		return false;
	}
	if (Stage && !First.ShipRows.empty())
	{
		TArray<FStageShip> Ships;
		for (const AirFeed::ShipRow& Row : First.ShipRows)
		{
			FStageShip& Ship = Ships.AddDefaulted_GetRef();
			Ship.Id = Row.Id;
			Ship.Class = AirParts::Wide(Row.Class);
			Ship.Nation = AirParts::Wide(Row.Nation);
			Ship.Name = AirParts::Wide(Row.Name);
			Ship.At = FVector2D(Row.X, Row.Y);
			Ship.Face = Row.Face;
			Ship.bDock = Row.bDock;
		}
		Stage->SetShips(Ships);
	}
	return true;
}

bool AAirBattle::Open(const FString& Why)
{
	UWorld* Here = GetWorld();
	if (!Here || !State.IsValid())
	{
		return false;
	}
	FAirBattleState& S = *State;
	// (the sheet of cloud shapes begun on the pool now, for LaySky; once a run)
	AirParts::BeginSheet();
	// The mix (the sound director's mix.json, carried in Content/Data/Sound): the scene reads its numbers, never copies them.
	{
		FString MixText;
		if (FFileHelper::LoadFileToString(MixText, *(FPaths::ProjectContentDir() / TEXT("Data/Sound/mix.json"))))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MixText), Mix);
		}
		if (!Mix.IsValid())
		{
			UE_LOG(LogTheater, Warning, TEXT("Air battle: Content/Data/Sound/mix.json is missing: the battle is silent."));
		}
		// The master limiter (mix.json buses.master): stacked guns, flak and passes never past the ceiling. On the
		// master while the battle runs, so whatever records the game's sound records it limited; taken off at EndPlay.
		USubmixEffectDynamicsProcessorPreset* MadeLimiter = nullptr;
		if (Mix.IsValid() && !Limiter)
		{
			USubmixEffectDynamicsProcessorPreset* Made = NewObject<USubmixEffectDynamicsProcessorPreset>(this);
			FSubmixEffectDynamicsProcessorSettings Settings;
			Settings.DynamicsProcessorType = ESubmixEffectDynamicsProcessorType::Limiter;
			Settings.PeakMode = ESubmixEffectDynamicsPeakMode::Peak;
			// (both channels limited by the louder: a gun hard to one side never passes the ceiling in that ear)
			Settings.LinkMode = ESubmixEffectDynamicsChannelLinkMode::Peak;
			Settings.ThresholdDb = static_cast<float>(AirParts::MixNumber(Mix, TEXT("buses.master.limiterCeilingDb"), -1.5));
			// (the attack near instant, 0.02 ms, about one sample: a gun crack's first peak lasts about 0.1 ms, and with a
			// 1 ms attack the detector hardly rose before it had passed, so the Mac's first runs with this limiter really on
			// still reached 0.00 dBFS whatever the look-ahead. With the attack instant, the look-ahead only has to hold the
			// gain down until the peak leaves the delay: the envelope's release over it is the one leak left, 40 x look-ahead
			// / release dB with the digital detector, so 1 ms and 500 ms leak under 0.1 dB. A digital 500 ms release is
			// the analog ~110 ms curve. Modelled in sound/limiter-model/; the ground scene's limiter takes the same numbers)
			Settings.bAnalogMode = AirParts::MixFlag(Mix, TEXT("buses.master.limiterAnalogDetector"), false);
			Settings.LookAheadMsec = static_cast<float>(AirParts::MixNumber(Mix, TEXT("buses.master.limiterLookAheadMs"), 1.0));
			Settings.AttackTimeMsec = static_cast<float>(AirParts::MixNumber(Mix, TEXT("buses.master.limiterAttackMs"), 0.02));
			Settings.ReleaseTimeMsec = static_cast<float>(AirParts::MixNumber(Mix, TEXT("buses.master.limiterReleaseMs"), 500.0));
			Settings.InputGainDb = 0.0f;
			Settings.OutputGainDb = 0.0f;
			Settings.KneeBandwidthDb = 0.0f; // (a hard ceiling: the default 10 dB knee began pressing 5 dB under it)
			// (the preset's own Settings as well as SetSettings: SetSettings fills only the copy the effects read, and
			// a preset's first effect refills that copy from Settings (EFFECT_PRESET_METHODS' Init), so with SetSettings
			// alone the master most likely ran the default dynamics settings, not this limiter: the Bremen chase film
			// passed the ceiling 28 times, for up to 96 ms. Set again once the effect is made, whichever comes first)
			Made->Settings = Settings;
			Made->SetSettings(Settings);
			Limiter = Made;
			UAudioMixerBlueprintLibrary::AddMasterSubmixEffect(this, Limiter);
			Made->SetSettings(Settings);
			MadeLimiter = Made;
		}
		// (how many voices the engine plays at once: DefaultEngine.ini's audio quality level and AudioMaxChannels. The Mac
		// run's log line shows whether it is the 128 asked for, or still the engine's 32)
		if (Here)
		{
			FAudioDeviceHandle Device = Here->GetAudioDevice();
			if (Device.IsValid())
			{
				UE_LOG(LogTheater, Display, TEXT("Air battle sound: the engine plays up to %d voices at once."), Device->GetMaxChannels());
				if (MadeLimiter)
				{
					// (read back from the copy the effects read, GetSettings, not from the numbers asked for: if the master
					// still ran the engine's default dynamics, this line says compressor, -6 dBFS, a 10 dB knee)
					const FSubmixEffectDynamicsProcessorSettings Running = MadeLimiter->GetSettings();
					const bool bIsLimiter = Running.DynamicsProcessorType == ESubmixEffectDynamicsProcessorType::Limiter;
					UE_LOG(LogTheater, Display, TEXT("Air battle sound: master %s on, read back from the effect's settings: ceiling %.1f dBFS, knee %.1f dB, look-ahead %.2f ms, attack %.3f ms, release %.0f ms, %s detector."),
						bIsLimiter ? TEXT("limiter") : TEXT("COMPRESSOR (not the limiter asked for)"), Running.ThresholdDb, Running.KneeBandwidthDb, Running.LookAheadMsec,
						Running.AttackTimeMsec, Running.ReleaseTimeMsec, Running.bAnalogMode ? TEXT("analog") : TEXT("digital"));
				}
			}
			else
			{
				UE_LOG(LogTheater, Warning, TEXT("Air battle sound: the world has no audio device, so no master limiter and no sound."));
			}
		}
	}
	// The paint: the campaign's own surface, coloured by each point's colour, as the stage's ships are.
	{
		UMaterialInterface* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_MapSurface.M_MapSurface"));
		if (!Surface)
		{
			Surface = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		auto Ink = [this, Surface](float Roughness, float Specular)
		{
			UMaterialInstanceDynamic* Made = UMaterialInstanceDynamic::Create(Surface, this);
			Made->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			Made->SetScalarParameterValue(TEXT("Specular"), Specular);
			Made->SetScalarParameterValue(TEXT("Grain"), 0.0f);
			Made->SetScalarParameterValue(TEXT("Tooth"), 0.0f);
			return Made;
		};
		// (the aircraft's paint flat, as service paint was: at 0.5 rough and 0.45 specular a low sun ahead lit an olive
		// drab P-40's near wing sand, (198,180,161), with a pale sheen farther out (the art director on 2b177386's Pearl
		// 05, 22 November 04:16Z: Warhawk.json is 4F5236, "matte #555A47"). At 0.78 and 0.3 the sky's sheen at a grazing
		// look still lit the P-40's upper wing sand, (199,177,152), and the Devastator's navy blue-grey (566A78) a neutral
		// (140,140,133): the art director on a1acb552's frames, 22 November 08:13Z, "try roughness 0.9 and specular 0.15
		// for flat paints". "-TheaterPlaneInk=0.9,0.15" tries another, roughness then specular)
		double PaintRough = 0.9, PaintSpec = 0.15;
		{
			FString Asked;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterPlaneInk="), Asked, false))
			{
				TArray<FString> Parts;
				Asked.ParseIntoArray(Parts, TEXT(","), false);
				double* const Into[] = { &PaintRough, &PaintSpec };
				for (int32 K = 0; K < Parts.Num() && K < 2; ++K)
				{
					if (!Parts[K].TrimStartAndEnd().IsEmpty())
					{
						*Into[K] = FMath::Clamp(FCString::Atod(*Parts[K]), 0.0, 1.0);
					}
				}
			}
			UE_LOG(LogTheater, Display, TEXT("Air battle: the aircraft's paint %.2f rough, %.2f specular."), PaintRough, PaintSpec);
		}
		PlaneInk = Ink(static_cast<float>(PaintRough), static_cast<float>(PaintSpec));
		PaintRoughness = static_cast<float>(PaintRough);
		PaintSpecular = static_cast<float>(PaintSpec);
		PlainInk = Ink(0.85f, 0.2f);
		if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight")))
		{
			DiscInk = UMaterialInstanceDynamic::Create(Glow, this);
			DiscInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
		}
		float Turns = 1.9f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterScrewTurns="), Turns);
		ScrewTurns = FMath::Clamp(static_cast<double>(Turns), 0.0, 30.0);
	}
	// The effects: the kit's own, drawn out to the air distance, on the air clock (its mesh, the one it hangs on Root,
	// drawn behind the clouds while the eye is under them: DrawSky).
	const TArray<TObjectPtr<USceneComponent>> BeforeAir = Root->GetAttachChildren();
	Effects = MakeUnique<FBattleEffects>();
	if (!Effects->Begin(Root, UnitsPerMetre, [this](const FVector& Point)
		{
			return FVector(Point.X, Point.Y, Stage && Stage->IsRaised() ? Stage->GroundZ(Point) : 0.0);
		}))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: the effects kit did not begin (its recipe or material is missing): no guns, smoke or flak are drawn."));
		Effects.Reset();
	}
	else
	{
		for (const TObjectPtr<USceneComponent>& Child : Root->GetAttachChildren())
		{
			UProceduralMeshComponent* Found = Cast<UProceduralMeshComponent>(Child.Get());
			if (Found && !BeforeAir.Contains(Child))
			{
				AirFxMesh = Found;
			}
		}
		AirParts::UseAirDistance(Effects.Get());
		// (with the kit's far trails on, game.air_far.trails_on: past 1.2 km a trail is some 40 pieces, not 128, so the scene
		// may light 60 at once, the battle-scale plan's room at 150 aircraft; the art director passed them, 19:25Z 22 November.
		// -TheaterTrailRoom= tries another, 10 to 120)
		{
			const bool bFar = AirParts::FarTrailsOn();
			int32 Room = !AirParts::HasTrailSpan(Effects.Get()) ? 10 : bFar ? 60 : 24;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterTrailRoom="), Room);
			S.Trails.Most = FMath::Clamp(Room, 10, 120);
			UE_LOG(LogTheater, Display, TEXT("Air battle: room for %d trails at once (the kit's far trails %s, far guns %s)."), S.Trails.Most,
				bFar ? TEXT("on") : TEXT("off"), AirParts::FarGunsOn() ? TEXT("on") : TEXT("off"));
		}
		Effects->SetWind(WorldVector(AirSky::Wind.X, AirSky::Wind.Y, 0.0) * (1.0 / 100.0));
		// The ground's own kit (bomb bursts, crashes, splashes, fires, the stage's oil), so the clouds can lie over it
		// while the air fight lies over them: its mesh is the one it hangs on Root.
		TArray<TObjectPtr<USceneComponent>> Before = Root->GetAttachChildren();
		GroundFx = MakeUnique<FBattleEffects>();
		if (GroundFx->Begin(Root, UnitsPerMetre, [this](const FVector& Point)
			{
				return FVector(Point.X, Point.Y, Stage && Stage->IsRaised() ? Stage->GroundZ(Point) : 0.0);
			}))
		{
			AirParts::UseAirDistance(GroundFx.Get());
			GroundFx->SetWind(WorldVector(AirSky::Wind.X, AirSky::Wind.Y, 0.0) * (1.0 / 100.0));
			for (const TObjectPtr<USceneComponent>& Child : Root->GetAttachChildren())
			{
				UProceduralMeshComponent* Found = Cast<UProceduralMeshComponent>(Child.Get());
				if (Found && !Before.Contains(Child))
				{
					GroundFxMesh = Found;
				}
			}
		}
		else
		{
			GroundFx.Reset();
		}
	}
	Raise();
	// The flight core's ground: the stage's hills; with no stage the board's own land, as LayGround lays it (the
	// pre-check on round 14: Bremen's bombs had burst as water columns); else the sea.
	if (Stage && Stage->IsRaised())
	{
		TWeakObjectPtr<AAirStage> Under(Stage);
		const double U = UnitsPerMetre;
		S.Show.Ground = [Under, U](double X, double Y)
		{
			return Under.IsValid() ? FMath::Max(0.0, Under->GroundZ(FVector(Y * U, X * U, 0.0)) / U) : 0.0;
		};
	}
	else
	{
		const std::shared_ptr<const AirGround::BoardLand> Land = std::make_shared<const AirGround::BoardLand>(S.Board.Land);
		if (!Land->IsEmpty())
		{
			S.Show.Ground = [Land](double X, double Y) { return Land->At(X, Y); };
			S.SkyPlace.GroundTop = AirGround::BoardLand::LandHigh;
		}
	}
	LaySky();
	LayZenith();
	LayHazeBand();
	KeyWarmth = S.Board.StageKey.empty() && !S.Board.Land.empty() ? 6500.0f : 4900.0f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterKeyWarmth="), KeyWarmth);
	KeyWarmth = FMath::Clamp(KeyWarmth, 1700.0f, 12000.0f);
	Small = NewObject<UProceduralMeshComponent>(this, TEXT("AirSmallThings"));
	Small->SetupAttachment(Root);
	Small->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Small->SetCastShadow(false);
	Small->RegisterComponent();
	HideTheTable();
	LayDeckShade();
	LayDeckSheet();
	LayCirrus();
	LayRainShafts();
	Eye = Here->SpawnActor<ACameraActor>();
	if (Eye)
	{
		Eye->GetCameraComponent()->SetFieldOfView(55.0f);
		// (the whole window, whatever its shape: a camera actor keeps 16:9 and bars the rest)
		Eye->GetCameraComponent()->bConstrainAspectRatio = false;
		Eye->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Eye->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
		// (the 3D builder, 19 November 2026: the art director's battle grade, on battle cameras only)
		BattleLook::Grade(Eye->GetCameraComponent()->PostProcessSettings);
		// (a raid over land with no stage: the grade's warmth, tuned on Pearl Harbor's blue sea and sky, turned the fields
		// under Bremen's low winter sun khaki, then desert tan (the coordinator's reading, 21:51Z; the art director on the
		// film of build 1786, 23:27Z: sat 0.40, blue under red by 43, against the ground's own neutral grey-olive). Over
		// land it is neutral, in sun and under a deck alike (the ground thread's numbers, 23:29Z: at 6,500 K sunlit ground
		// reads blue under red by 8 and shade over it by 11, both in her bands; blue 0.97 in sun turned the old town's
		// brick roofs to 24 under, the first thing to read orange): "-TheaterLandGrade=1.0,0.97" (red, blue) tries others)
		if (State.IsValid() && State->Board.StageKey.empty() && !State->Board.Land.empty())
		{
			double Red = 1.0, Blue = 1.0;
			FString Asked;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandGrade="), Asked, false))
			{
				FString A, B;
				if (Asked.Split(TEXT(","), &A, &B))
				{
					Red = A.IsEmpty() ? Red : FCString::Atod(*A);
					Blue = B.IsEmpty() ? Blue : FCString::Atod(*B);
				}
				else if (!Asked.IsEmpty())
				{
					Red = FCString::Atod(*Asked);
				}
			}
			FPostProcessSettings& Lens = Eye->GetCameraComponent()->PostProcessSettings;
			Lens.bOverride_ColorGain = true;
			Lens.ColorGain = FVector4(FMath::Clamp(Red, 0.5, 1.5), 1.0, FMath::Clamp(Blue, 0.5, 1.5), 1.0);
			UE_LOG(LogTheater, Display, TEXT("Air battle: over land, the grade's warmth eased to red %.2f, blue %.2f."), Red, Blue);
		}
	}
	{
		// The scene's own drawing size, for timing (build 1734: on Brian's 2881 by 1786 window the scene is drawn at the
		// campaign map's budget, some 1077 by 668, and filled out 2.7 times each way by Unreal's upscaler, which is the
		// softness of the Midway ships picture; the words are drawn at every pixel, which is why they stay sharp).
		// "-TheaterAirSize=85" draws the scene at 85 hundredths of the window's points each way while the battle
		// stands; without it the map's budget holds. The size it had comes back when the scene ends.
		int32 AirSize = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterAirSize="), AirSize) && AirSize >= 40 && AirSize <= 100)
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
			{
				KeptScreenPercent = Variable->GetFloat();
				Variable->Set(static_cast<float>(AirSize), ECVF_SetByCode);
				UE_LOG(LogTheater, Display, TEXT("Air battle: the scene drawn at %d hundredths of the window's points (the map's %.0f)"), AirSize, KeptScreenPercent);
			}
		}
	}
	// The camera starts behind his own raid, looking along its way.
	for (const auto& Pair : S.Show.Groups())
	{
		if (Pair.second.Side == S.Desk.MySideIndex() && !Pair.second.bFighter)
		{
			Yaw = FMath::RadiansToDegrees(Pair.second.Face);
			break;
		}
	}
	// (the eye stood on its opening place before it is the view: spawned at the world's middle, the frame drawn before
	// the battle's first tick was drawn from there, at the stage's sea level; the first tick then cuts to the flyover)
	AimEye(0.0);
	SkyLowerOnce();
	if (APlayerController* Controller = Here->GetFirstPlayerController())
	{
		if (Eye)
		{
			Controller->SetViewTarget(Eye);
			AirParts::TellCut(Here);
		}
		AirParts::ShowFrame(Cast<ACampaignHUD>(Controller->GetHUD()), false);
	}
	// The picture run: the art director's frames, then quit.
	if (!ShotFolder.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*ShotFolder, true);
		Shots = {
			{ TEXT("01-approach"), -12.0, false, false, false },   // (seconds of real time before the formations meet)
			{ TEXT("02-the-meeting"), 6.0, false, false, false },  // (after they meet)
			{ TEXT("03-ride-a-fighter"), 22.0, true, false, false },
			{ TEXT("04-the-thick-of-it"), 45.0, false, false, false },
			{ TEXT("05-ride-again"), 70.0, true, false, false, true }, // (riding the other side)
			{ TEXT("05c-chase-view"), 95.0, true, false, false, false, true }, // (the chase view, its plate and a step)
			{ TEXT("06-the-ships"), 5.0, false, true, false },     // (after the first ship goes down)
			{ TEXT("06b-guns-fire-back"), 2.5, false, false, false, false, false, true }, // (after the first attack run on a ship begins)
			{ TEXT("07-the-fight-goes-on"), 120.0, false, false, false },
			{ TEXT("09-strike-marks"), 105.0, true, false, false, false, false, false, true }, // (riding the aircraft struck the most)
			{ TEXT("10-dead-engine"), 85.0, true, false, false, false, false, false, false, true }, // (riding an aircraft with an engine out)
			{ TEXT("11-parachutes"), 3.0, false, false, false, false, false, false, false, false, true }, // (after three canopies first hang together)
			{ TEXT("12-ace-close"), 30.0, true, false, false, false, false, false, false, false, false, true }, // (an ace's marks, 8 m off them)
			{ TEXT("12b-ace-chase"), 36.0, true, false, false, false, true, false, false, false, false, true }, // (the chase view behind the ace)
		};
	}
	// (1.5.1) The opening held behind its black frame while the scene just made is taken in (WarmUp, WarmTick).
	if (!NoCurtain())
	{
		WarmUp();
	}
	ShowScreen();
	bBegun = true;
	SetActorTickEnabled(true);
	UE_LOG(LogTheater, Display, TEXT("Air battle: %s, %s (%s), %d views%s."), *Why, *AirParts::Wide(S.Board.Title), *AirParts::Wide(S.Board.Date),
		S.bReplay ? static_cast<int32>(S.Recording.Views.size()) : 1, ShotFolder.IsEmpty() ? TEXT("") : *(TEXT(", pictures into ") + ShotFolder));
	return true;
}

void AAirBattle::Raise()
{
	FAirBattleState& S = *State;
	if (!S.Board.StageKey.empty())
	{
		Stage = GetWorld()->SpawnActor<AAirStage>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Stage && Stage->Raise(AirParts::Wide(S.Board.StageKey), FVector2D(S.Board.StageAimX, S.Board.StageAimY), S.Board.StageTurn,
			FVector2D(S.Board.TargetX, S.Board.TargetY), UnitsPerMetre, GroundFx ? GroundFx.Get() : Effects.Get()))
		{
			if (Effects)
			{
				Effects->SetSun(Stage->TowardSun());
			}
			if (GroundFx)
			{
				GroundFx->SetSun(Stage->TowardSun());
			}
			// The ships as the battle lays them (the first view that carries them).
			if (S.bReplay)
			{
				for (const AirFeed::Frame& View : S.Recording.Views)
				{
					if (View.ShipRows.empty())
					{
						continue;
					}
					TArray<FStageShip> Ships;
					for (const AirFeed::ShipRow& Row : View.ShipRows)
					{
						FStageShip& Ship = Ships.AddDefaulted_GetRef();
						Ship.Id = Row.Id;
						Ship.Class = AirParts::Wide(Row.Class);
						Ship.Nation = AirParts::Wide(Row.Nation);
						Ship.Name = AirParts::Wide(Row.Name);
						Ship.At = FVector2D(Row.X, Row.Y);
						Ship.Face = Row.Face;
						Ship.bDock = Row.bDock;
					}
					Stage->SetShips(Ships);
					break;
				}
			}
			return;
		}
		UE_LOG(LogTheater, Warning, TEXT("Air battle: the stage %s could not be raised; the plain sea stands in."), *AirParts::Wide(S.Board.StageKey));
		if (Stage)
		{
			Stage->Destroy();
			Stage = nullptr;
		}
	}
	LayGround();
	// (the hour's sun: the stage's, the place's, or mid-morning from the south-east, as the campaign's daylight raids)
	if (Effects)
	{
		Effects->SetSun(TowardSun());
	}
	if (GroundFx)
	{
		GroundFx->SetSun(TowardSun());
	}
}

FVector AAirBattle::TowardSun() const
{
	if (Stage && Stage->IsRaised())
	{
		return Stage->TowardSun();
	}
	if (!State.IsValid())
	{
		return FRotator(30.0, 150.0, 0.0).Vector();
	}
	const FAirBattleState& S = *State;
	if (S.Board.StageKey.empty() && std::isfinite(S.Board.Latitude))
	{
		// (a raid over a place with no stage: its sun at half past noon in the middle of the month, never under 4 degrees
		// up; Bremen in December some 13 degrees up, a little west of south, the low winter light of the art director's
		// round 18d)
		double X = 0.0, Y = 0.0, Z = 0.0;
		AirLand::SunAt(S.Board.Latitude, S.Board.Month + 1, 12.5, S.Board.NorthX, S.Board.NorthY, X, Y, Z);
		const FVector Way = FVector(Y, X, FMath::Max(Z, 0.07)).GetSafeNormal();
		if (!Way.IsNearlyZero())
		{
			return Way;
		}
	}
	return FRotator(30.0, 150.0, 0.0).Vector();
}

void AAirBattle::LayGround()
{
	// A battle with no stage: the sea far past the board, the board's land laid flat on it (place.ground.land, rings of
	// board points, LandHigh over the sea: the flight core's ground there is the same, AirGround::BoardLand), and the
	// target's outline. The land is cut into triangles by ear clipping.
	FAirBattleState& S = *State;
	const double U = UnitsPerMetre;
	auto Sheet = [this](const TCHAR* Name, const TArray<FVector>& Points, const TArray<int32>& Triangles, const FColor& Paint)
	{
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->RegisterComponent();
		TArray<FVector> Normals;
		TArray<FColor> Colors;
		Normals.Init(FVector::UpVector, Points.Num());
		Colors.Init(Paint, Points.Num());
		Mesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(0, PlainInk);
		Ground.Add(Mesh);
	};
	// (each triangle the way round that faces up, as FMiniatureMesh lays them)
	auto Up = [](const TArray<FVector>& V, int32 A, int32 B, int32 C, TArray<int32>& Out)
	{
		const FVector Facing = (V[B] - V[C]) ^ (V[A] - V[C]);
		Out.Append(Facing.Z >= 0.0 ? TArray<int32>{ A, B, C } : TArray<int32>{ A, C, B });
	};
	{
		const double Far = 300000.0 * U;
		const FVector Middle = World(S.Board.TargetX, S.Board.TargetY, 0.0);
		const TArray<FVector> V = { Middle + FVector(-Far, -Far, 0.0), Middle + FVector(Far, -Far, 0.0), Middle + FVector(Far, Far, 0.0), Middle + FVector(-Far, Far, 0.0) };
		TArray<int32> T;
		Up(V, 0, 1, 2, T);
		Up(V, 0, 2, 3, T);
		Sheet(TEXT("AirSea"), V, T, FColor(38, 62, 78));
	}
	int32 Ring = 0;
	for (const std::vector<std::pair<double, double>>& Land : S.Board.Land)
	{
		if (Land.size() < 3)
		{
			continue;
		}
		TArray<FVector> V;
		TArray<FVector2D> P;
		for (const std::pair<double, double>& Point : Land)
		{
			V.Add(World(Point.first, Point.second, AirGround::BoardLand::LandHigh));
			P.Add(FVector2D(Point.first, Point.second));
		}
		// Ear clipping: the ring's corners taken off one at a time where the triangle they make holds no other corner.
		double Area = 0.0;
		for (int32 K = 0; K < P.Num(); ++K)
		{
			const FVector2D& A = P[K];
			const FVector2D& B = P[(K + 1) % P.Num()];
			Area += A.X * B.Y - B.X * A.Y;
		}
		const double Turn = Area >= 0.0 ? 1.0 : -1.0;
		TArray<int32> Left;
		for (int32 K = 0; K < P.Num(); ++K)
		{
			Left.Add(K);
		}
		TArray<int32> T;
		int32 Guard = 0;
		while (Left.Num() > 3 && Guard++ < 100000)
		{
			bool bCut = false;
			for (int32 K = 0; K < Left.Num(); ++K)
			{
				const int32 IA = Left[(K + Left.Num() - 1) % Left.Num()], IB = Left[K], IC = Left[(K + 1) % Left.Num()];
				const FVector2D A = P[IA], B = P[IB], C = P[IC];
				const double Cross = ((B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X)) * Turn;
				if (Cross <= 0.0)
				{
					continue;
				}
				bool bInside = false;
				for (const int32 IO : Left)
				{
					if (IO == IA || IO == IB || IO == IC)
					{
						continue;
					}
					const FVector2D O = P[IO];
					const double D1 = ((B.X - A.X) * (O.Y - A.Y) - (B.Y - A.Y) * (O.X - A.X)) * Turn;
					const double D2 = ((C.X - B.X) * (O.Y - B.Y) - (C.Y - B.Y) * (O.X - B.X)) * Turn;
					const double D3 = ((A.X - C.X) * (O.Y - C.Y) - (A.Y - C.Y) * (O.X - C.X)) * Turn;
					if (D1 >= 0.0 && D2 >= 0.0 && D3 >= 0.0)
					{
						bInside = true;
						break;
					}
				}
				if (bInside)
				{
					continue;
				}
				Up(V, IA, IB, IC, T);
				Left.RemoveAt(K);
				bCut = true;
				break;
			}
			if (!bCut)
			{
				break;
			}
		}
		if (Left.Num() == 3)
		{
			Up(V, Left[0], Left[1], Left[2], T);
		}
		Sheet(*FString::Printf(TEXT("AirLand_%d"), Ring++), V, T, FColor(92, 98, 70));
	}
	// The ground's pictures, for a raid over land with no set piece: northern Europe's country (not a desert's, nor a
	// Japanese-held target's, where the plain sheets stay; nor a set piece's that could not be raised).
	if (S.Board.StageKey.empty() && !S.Board.Land.empty() && S.Board.Terrain != "desert" && S.Board.DefNat != "JAP" && S.Board.DefNat != "JPN")
	{
		PaintLand();
	}
}

/** -TheaterAirLoudPaint: the ground's paint spread over every core at the frame's priority, as before 1.5.1. */
static bool LoudPaint()
{
	static const bool bAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterAirLoudPaint"));
	return bAsked;
}

void AAirBattle::PaintLand()
{
	// The ground's pictures (Battle/AirLand): the board's land and rivers, the month, true north on the board, the
	// target's town plan from the book (Content/Data/AirGround/towns.json; a town not in it is made up), the scene's
	// sun for the baked shadows. Painted on the pool's threads; the plain sheets stand until LayLand lays them.
	const FAirBattleState& S = *State;
	AirLand::Place P;
	P.AimX = S.Board.TargetX;
	P.AimY = S.Board.TargetY;
	P.NorthX = S.Board.NorthX;
	P.NorthY = S.Board.NorthY;
	P.Terrain = S.Board.Terrain.empty() ? std::string("plains") : S.Board.Terrain;
	P.Name = S.Board.PlaceName;
	P.Month = S.Board.Month + 1; // (the feed's months count from 0)
	P.Seed = S.Recording.Seed ? S.Recording.Seed : 238;
	const FVector Sun = TowardSun();
	P.SunX = Sun.Y;
	P.SunY = Sun.X;
	P.SunZ = Sun.Z;
	P.Land = S.Board.Land;
	P.Rivers = S.Board.Rivers;
	FString Book;
	FFileHelper::LoadFileToString(Book, *(FPaths::ProjectContentDir() / TEXT("Data/AirGround/towns.json")));
	const std::string Plans = TCHAR_TO_UTF8(*Book);
	if (!S.Board.bNorthGiven)
	{
		UE_LOG(LogTheater, Log, TEXT("Air battle ground: the recording gives no true north (place.ground.north); the town plan is laid with north up the board."));
	}
	LandMade = MakeShared<FAirLandMade, ESPMode::ThreadSafe>();
	LandAsked = FPlatformTime::Seconds();
	// The paint's loops at the background's priority, so that they never stand before the frame's own jobs. Spread over
	// every core at the frame's priority, they did: the render thread waited behind them for its scene's own jobs, a
	// second at a time, until the paint was done (the Mac's hitch dump on 3e098f1f, 24 Nov 2026 23:44Z, hitch 7:
	// UpdatePrimitive's wait for its tasks 1005 ms, 8.9 s into the flyover, ending as the ground was painted). The same
	// ground, perhaps a little later. -TheaterAirLoudPaint: spread at the frame's priority, as before.
	const EParallelForFlags Loops = LoudPaint() ? EParallelForFlags::None : EParallelForFlags::BackgroundPriority;
	LandPainting = Async(EAsyncExecution::ThreadPool, [Made = LandMade, P, Plans, Loops]()
	{
		const double Began = FPlatformTime::Seconds();
		Made->Painter = std::make_unique<AirLand::Painter>(P, Plans);
		Made->Frames = AirLand::Frames(P);
		Made->Levels.SetNum(static_cast<int32>(Made->Frames.size()));
		for (int32 K = 0; K < Made->Levels.Num(); ++K)
		{
			const AirLand::Frame& F = Made->Frames[static_cast<size_t>(K)];
			std::vector<uint8_t> Rgba(static_cast<size_t>(F.W) * F.H * 4);
			std::vector<float> Heights(static_cast<size_t>(F.W) * F.H);
			// (in bands of 64 rows, every band on a thread of its own; the shadows when every row's heights are in)
			const int32 Bands = (F.H + 63) / 64;
			const AirLand::Painter& Painter = *Made->Painter;
			ParallelFor(Bands, [&](int32 B) { Painter.PaintRows(F, B * 64, (B + 1) * 64, Rgba.data(), Heights.data()); }, Loops);
			if (AirLand::Painter::Shades(F))
			{
				ParallelFor(Bands, [&](int32 B) { Painter.ShadeRows(F, B * 64, (B + 1) * 64, Rgba.data(), Heights.data()); }, Loops);
			}
			TArray<FColor> Pixels;
			Pixels.SetNumUninitialized(F.W * F.H);
			for (int32 I = 0; I < Pixels.Num(); ++I)
			{
				const uint8_t* C = &Rgba[static_cast<size_t>(I) * 4];
				Pixels[I] = FColor(C[0], C[1], C[2], 255);
			}
			CampaignTextures::MakeLevels(F.W, F.H, Pixels, true, Made->Levels[K]);
		}
		Made->Seconds = FPlatformTime::Seconds() - Began;
	});
	// (a picture run waits for the ground, so that every picture has it)
	if (!ShotFolder.IsEmpty())
	{
		LandPainting.Wait();
		LandPainting = TFuture<void>();
		LayLand();
	}
}

void AAirBattle::LayLand()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_LayLand);
	if (!LandMade.IsValid() || !LandMade->Painter || LandMade->Levels.Num() != static_cast<int32>(LandMade->Frames.size()))
	{
		return;
	}
	FAirLandMade& Made = *LandMade;
	const FAirBattleState& S = *State;
	UMaterialInterface* Cloth = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Flag.M_Flag"));
	if (!Cloth)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle ground: M_Flag is missing; the plain ground stays."));
		LandMade.Reset();
		return;
	}
	// (the cloth of a standard, its picture the ground, with no light of its own: lit by the sun as the land is)
	auto InkOf = [this, Cloth](UTexture2D* Picture)
	{
		UMaterialInstanceDynamic* Ink = UMaterialInstanceDynamic::Create(Cloth, this);
		Ink->SetTextureParameterValue(TEXT("Flag"), Picture);
		Ink->SetScalarParameterValue(TEXT("Glow"), 0.0f);
		Ink->SetScalarParameterValue(TEXT("Fade"), 0.0f);
		Ink->SetScalarParameterValue(TEXT("Drain"), 0.0f);
		Ink->SetScalarParameterValue(TEXT("Veiled"), 0.0f);
		Ink->SetScalarParameterValue(TEXT("Roughness"), 0.92f);
		LandInks.Add(Ink);
		return Ink;
	};
	const AirGround::BoardLand Heights(S.Board.Land);
	auto High = [&Heights](double X, double Y) { return Heights.IsEmpty() ? AirGround::BoardLand::LandHigh : Heights.At(X, Y); };
	auto Up = [](const TArray<FVector>& V, int32 A, int32 B, int32 C, TArray<int32>& Out)
	{
		const FVector Facing = (V[B] - V[C]) ^ (V[A] - V[C]);
		Out.Append(Facing.Z >= 0.0 ? TArray<int32>{ A, B, C } : TArray<int32>{ A, C, B });
	};
	auto Sheet = [this](const FString& Name, const TArray<FVector>& Points, const TArray<int32>& Triangles, const TArray<FVector2D>& Uv,
	                    const TArray<FColor>& Colours, UMaterialInterface* Ink)
	{
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, *Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->RegisterComponent();
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Points.Num());
		Mesh->CreateMeshSection(0, Points, Triangles, Normals, Uv, Colours, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(0, Ink);
		return Mesh;
	};
	TArray<TObjectPtr<UProceduralMeshComponent>> Laid;
	// Each picture a grid of flat cells (the coast's 2 m step kept at the cells' corners), less the cells a finer picture
	// covers: the pictures are laid out so that a finer one's edges fall on the coarser one's cells.
	const double Cells[3] = { 512.0, 2048.0, 8192.0 };
	for (int32 K = 0; K < Made.Levels.Num(); ++K)
	{
		const AirLand::Frame& F = Made.Frames[static_cast<size_t>(K)];
		// (read through the anisotropic filter: the ground is seen at a slant, and plain trilinear smeared it across the
		// view; build 1782's Mac stills, the aim-point flyover worst, its town at some 8 degrees)
		UTexture2D* Picture = CampaignTextures::CreateFromLevels(Made.Levels[K], false, MakeUniqueObjectName(this, UTexture2D::StaticClass(), *FString::Printf(TEXT("AirLandPicture_%d"), K)), false, true);
		if (!Picture)
		{
			continue;
		}
		LandPictures.Add(Picture);
		const double Cell = Cells[FMath::Min(K, 2)];
		const int32 NX = FMath::Max(1, FMath::RoundToInt((F.X1() - F.X0) / Cell)), NY = FMath::Max(1, FMath::RoundToInt((F.Y1() - F.Y0) / Cell));
		TArray<FVector> V;
		TArray<FVector2D> Uv;
		TArray<FColor> Colours;
		for (int32 J = 0; J <= NY; ++J)
		{
			for (int32 I = 0; I <= NX; ++I)
			{
				const double X = F.X0 + (F.X1() - F.X0) * I / NX, Y = F.Y0 + (F.Y1() - F.Y0) * J / NY;
				V.Add(World(X, Y, High(X, Y)));
				Uv.Add(FVector2D((X - F.X0) / (F.X1() - F.X0), (F.Y1() - Y) / (F.Y1() - F.Y0)));
				Colours.Add(FColor::White);
			}
		}
		const AirLand::Frame* Finer = K > 0 ? &Made.Frames[static_cast<size_t>(K - 1)] : nullptr;
		TArray<int32> T;
		for (int32 J = 0; J < NY; ++J)
		{
			for (int32 I = 0; I < NX; ++I)
			{
				const double CX = F.X0 + (F.X1() - F.X0) * (I + 0.5) / NX, CY = F.Y0 + (F.Y1() - F.Y0) * (J + 0.5) / NY;
				if (Finer && Finer->Holds(CX, CY))
				{
					continue;
				}
				const int32 A = J * (NX + 1) + I;
				Up(V, A, A + 1, A + NX + 2, T);
				Up(V, A, A + NX + 2, A + NX + 1, T);
			}
		}
		Laid.Add(Sheet(FString::Printf(TEXT("AirLandSheet_%d"), K), V, T, Uv, Colours, InkOf(Picture)));
	}
	// Past the far picture to the sea sheet's rim: the colour of the far picture's edge, carried out (each point of the
	// edge the mean of the texels by it, the rim the mean of its whole side), on a picture of plain white.
	if (Made.Levels.Num() == 3 && LandPictures.Num() == 3)
	{
		const AirLand::Frame& F = Made.Frames[2];
		const TArray<FColor>& Px = Made.Levels[2].Each[0];
		auto Linear = [](uint8 C) { const double V = C / 255.0; return V <= 0.04045 ? V / 12.92 : FMath::Pow((V + 0.055) / 1.055, 2.4); };
		// (the mean of a box of texels, as linear light, by the pixel's column and row)
		auto Mean = [&](int32 I0, int32 J0, int32 I1, int32 J1, double Out[3])
		{
			Out[0] = Out[1] = Out[2] = 0.0;
			int32 N = 0;
			for (int32 J = FMath::Max(0, J0); J <= FMath::Min(F.H - 1, J1); ++J)
			{
				for (int32 I = FMath::Max(0, I0); I <= FMath::Min(F.W - 1, I1); ++I)
				{
					const FColor& C = Px[J * F.W + I];
					Out[0] += Linear(C.R);
					Out[1] += Linear(C.G);
					Out[2] += Linear(C.B);
					++N;
				}
			}
			for (int32 C = 0; C < 3; ++C)
			{
				Out[C] /= FMath::Max(1, N);
			}
		};
		auto Colour = [](const double L[3]) { return FColor(static_cast<uint8>(FMath::Clamp(L[0] * 255.0 + 0.5, 0.0, 255.0)), static_cast<uint8>(FMath::Clamp(L[1] * 255.0 + 0.5, 0.0, 255.0)), static_cast<uint8>(FMath::Clamp(L[2] * 255.0 + 0.5, 0.0, 255.0)), 255); };
		TArray<FColor> White;
		White.Init(FColor::White, 16);
		UTexture2D* Plain = CampaignTextures::Create(4, 4, White, true, false, MakeUniqueObjectName(this, UTexture2D::StaticClass(), TEXT("AirLandPlain")));
		const double Rim = 300000.0;
		const int32 Steps = 24;
		TArray<FVector> V;
		TArray<FVector2D> Uv;
		TArray<FColor> Colours;
		TArray<int32> T;
		// (the four sides, each from its corner to the next going round: its points along the edge, and out at the rim)
		const double Corners[5][2] = { { F.X0, F.Y0 }, { F.X1(), F.Y0 }, { F.X1(), F.Y1() }, { F.X0, F.Y1() }, { F.X0, F.Y0 } };
		const double MX = (F.X0 + F.X1()) * 0.5, MY = (F.Y0 + F.Y1()) * 0.5;
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const double AX = Corners[Side][0], AY = Corners[Side][1], BX = Corners[Side + 1][0], BY = Corners[Side + 1][1];
			double Whole[3];
			auto TexelOf = [&F](double X, double Y, int32& I, int32& J) { I = FMath::Clamp(static_cast<int32>((X - F.X0) / F.Step), 0, F.W - 1); J = FMath::Clamp(static_cast<int32>((F.Y1() - Y) / F.Step), 0, F.H - 1); };
			{
				int32 IA, JA, IB, JB;
				TexelOf(AX, AY, IA, JA);
				TexelOf(BX, BY, IB, JB);
				Mean(FMath::Min(IA, IB) - 8, FMath::Min(JA, JB) - 8, FMath::Max(IA, IB) + 8, FMath::Max(JA, JB) + 8, Whole);
			}
			const int32 First = V.Num();
			for (int32 Q = 0; Q <= Steps; ++Q)
			{
				const double X = AX + (BX - AX) * Q / Steps, Y = AY + (BY - AY) * Q / Steps;
				int32 I, J;
				TexelOf(X, Y, I, J);
				double Near[3];
				Mean(I - 24, J - 24, I + 24, J + 24, Near);
				V.Add(World(X, Y, High(X, Y)));
				Uv.Add(FVector2D(0.5, 0.5));
				Colours.Add(Colour(Near));
				// (out from the picture's middle through the edge's point, to the rim's square)
				const double DX = X - MX, DY = Y - MY;
				const double Reach = Rim / FMath::Max(FMath::Abs(DX), FMath::Abs(DY));
				V.Add(World(MX + DX * Reach, MY + DY * Reach, 0.0));
				Uv.Add(FVector2D(0.5, 0.5));
				Colours.Add(Colour(Whole));
			}
			for (int32 Q = 0; Q < Steps; ++Q)
			{
				const int32 A = First + Q * 2;
				Up(V, A, A + 2, A + 3, T);
				Up(V, A, A + 3, A + 1, T);
			}
		}
		if (Plain)
		{
			LandPictures.Add(Plain);
			Laid.Add(Sheet(TEXT("AirLandRim"), V, T, Uv, Colours, InkOf(Plain)));
		}
	}
	// The plain sheets out of sight; the pictures' in their place.
	for (UProceduralMeshComponent* Old : Ground)
	{
		if (Old)
		{
			Old->SetVisibility(false);
		}
	}
	Ground.Append(Laid);
	const AirLand::Tally& Count = Made.Painter->Laid();
	UE_LOG(LogTheater, Log, TEXT("Air battle ground: %s (plan %s), %d villages, %d country roads, %d bridges, %d spires%s; painted aside in %.2f s (%s), laid %.2f s after it was asked for."),
		*AirParts::Wide(S.Board.PlaceName), *AirParts::Wide(Count.Plan), Count.Villages, Count.Links, Count.Bridges, Count.Spires, Count.bCoast ? TEXT(", the coast") : TEXT(""),
		Made.Seconds, LoudPaint() ? TEXT("its loops at the frame's priority: -TheaterAirLoudPaint") : TEXT("its loops at the background's priority"),
		FPlatformTime::Seconds() - LandAsked);
	// (the pictures are on the drawing thread now: the copies here are let go, all but the near picture's own texels
	// and the painter, for the craters)
	if (Made.Levels.Num() > 0 && Made.Levels[0].Each.Num() > 0 && LandPictures.Num() > 0)
	{
		LandNear = MoveTemp(Made.Levels[0].Each[0]);
	}
	Made.Levels.Empty();
}

void AAirBattle::ScarLand(double X, double Y, double Kilograms)
{
	// (the pictures first: while the pool still paints, it is writing the painter and the frames)
	if (LandPictures.Num() == 0 || !LandMade.IsValid() || !LandMade->Painter || LandMade->Frames.empty())
	{
		return;
	}
	const AirLand::Frame& F = LandMade->Frames[0];
	if (!F.Holds(X, Y) || LandNear.Num() != F.W * F.H)
	{
		return;
	}
	int Box[4] = { 1, 0, 0, 0 };
	LandMade->Painter->Scar(F, X, Y, Kilograms, reinterpret_cast<uint8_t*>(LandNear.GetData()), true, Box);
	if (Box[0] <= Box[2] && Box[1] <= Box[3])
	{
		// (the picture and its three smaller copies, down to 64 m a pixel: above that a crater is lost in its pixel)
		CampaignTextures::UpdateBoxAll(LandPictures[0], LandNear, Box[0], Box[1], Box[2], Box[3], 3);
	}
}

void AAirBattle::HideTheTable()
{
	// The table and its armies are put out of sight; the air and the light are the battle's, by day. What the table
	// had is kept first, and put back by ShowTheTable (the pre-check, 06:08Z: the lamp's turn and strength and the
	// haze stayed the battle's after CONTINUE, since the room sets some of them only once).
	if (bTableHidden)
	{
		return;
	}
	bTableHidden = true;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (ACampaignMapActor* Map = Cast<ACampaignMapActor>(*It))
		{
			Table = Map;
			AirParts::PutAway(Map, true);
		}
		if (It->IsA<ACampaignMapActor>() || It->IsA<AArmyPiece>())
		{
			if (It->IsA<AArmyPiece>() && !It->IsHidden())
			{
				KeptPieces.Add(*It);
			}
			It->SetActorHiddenInGame(true);
		}
		if (ACampaignEnvironment* Environment = Cast<ACampaignEnvironment>(*It))
		{
			KeptRooms.Add(Environment);
			Environment->SetActorTickEnabled(false);
			// Haze from 2 km (the art director, 19 November 04:40Z): a clear sky over the sea, the far formations soft.
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
				Kept.Colour = AirParts::HazeColour(Fog, FLinearColor(0.80f, 0.84f, 0.88f));
				// (thick low down and thin high up: looking down from bomber height the sea stays dark under the bright
				// cloud floor, the art director's 19 November 12:59Z; the horizon and the low air far off soft. The sky's
				// pictures, Tools/air_show/air_sky_picture.cpp, take the same numbers; in the game the battle look then
				// sets its own haze from these, CampaignBattleLook.cpp's AirHaze, which the pictures do not take)
				Fog->SetStartDistance(2000.0f * static_cast<float>(UnitsPerMetre));
				Fog->SetFogDensity(0.00045f);
				Fog->SetFogHeightFalloff(0.006f);
				Fog->SetFogMaxOpacity(0.8f);
				// (the 3D builder, 19 November 2026: the art director's ruling on the haze, thin under the eye. Over land
				// with no stage, a haze that thickens with distance: about her h1 over the near 10 km, so the town stays
				// darker than the fields, and her h2.5 by 25 to 30 km, the far ground pale blue-grey and meeting the sky
				// with no edge; from 7.5 km, 2.6 times as thick, by the haze's own reckoning as the ground's pictures
				// take it, Tools/air_show/air_land_picture.cpp. The art director, 20 November 20:09Z, round 18d)
				// (Over the sea it began 15 km off at twice as thick, and from bomber height it hid the sea from 15 or
				// 20 km on, the top 40% of the picture a white wall. From 30 km at 1.2 times, the sea's own face reads
				// out to 35 or 40 km, about a quarter of the way down from the horizon, and the haze still closes well
				// short of the sea's edge, 200 km off; the art director on the Mac's release 2 check, 19:32Z)
				const bool bOverLand = State.IsValid() && State->Board.StageKey.empty() && !State->Board.Land.empty();
				Fog->SetFogInscatteringColor(bOverLand ? AirParts::AirHazeInk : AirParts::SeaHazeInk());
				BattleLook::AirHaze(*Fog, UnitsPerMetre, bOverLand ? 7500.0 : 30000.0, bOverLand ? 2.6 : 1.2);
			}
			// The lamps: the key turned to the battle's sun, every lamp at its day strength and white (a battle opened
			// in the table's night kept the moon's), the sky's light the day's.
			TArray<UDirectionalLightComponent*> Lights;
			Environment->GetComponents(Lights);
			UDirectionalLightComponent* Key = nullptr;
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
				Kept.bAirByHeight = Light->bPerPixelAtmosphereTransmittance != 0;
				Kept.LightFunction = Light->LightFunctionMaterial;
				Kept.LightFunctionFade = Light->LightFunctionFadeDistance;
				Kept.LightFunctionDisabled = Light->DisabledBrightness;
				if (Light->GetFName() == TEXT("KeyLight") || (!Key && Lights.Num() == 1))
				{
					Key = Light;
				}
			}
			if (!Key)
			{
				for (UDirectionalLightComponent* Light : Lights)
				{
					if (!Key || Light->Intensity > Key->Intensity)
					{
						Key = Light;
					}
				}
			}
			if (Key)
			{
				BattleKey = Key;
				const FVector Sun = TowardSun();
				Key->SetWorldRotation((-Sun).Rotation());
				// (the 3D builder, 19 November 2026: a sky behind the battle, lit by its sun; taken away in ShowTheTable)
				BattleLook::AirSkyBegin(*this, Key);
				TheSun(*Key);
			}
			TArray<USkyLightComponent*> Skies;
			Environment->GetComponents(Skies);
			for (USkyLightComponent* Sky : Skies)
			{
				FKeptSky& Kept = KeptSkies.AddDefaulted_GetRef();
				Kept.Sky = Sky;
				Kept.Intensity = Sky->Intensity;
				Kept.Colour = Sky->GetLightColor();
				Kept.bLowerBlack = Sky->bLowerHemisphereIsBlack;
				Kept.LowerColour = Sky->LowerHemisphereColor;
			}
		}
	}
	LightTheBattle();
}

void AAirBattle::TheSun(UDirectionalLightComponent& Key)
{
	// The sun a hard small disc (the art director on build 1750's Midway 05, 19 November 2026: a core of 25 pixels
	// or less, the glow back to the sky's colour within about 150). The table's lamp is a sun a degree and a half
	// wide, kept soft for the table's shadows, and the sky drew all of its light into that disc: some two thousand
	// times the picture's white, which the picture's bloom spread into a glow 700 pixels wide. The bloom spreads a
	// share of what the disc holds, however wide the disc is drawn, so here the disc holds six thousandths of it
	// (still near a hundred times the white, so a hard white disc, its glow under a hundredth of the white 150 pixels
	// out) and is drawn the true sun's half degree wide (eleven pixels across the middle of the close camera's 62
	// degrees). And the air's haze throws the sun's light forward at 0.75: the engine's 0.8 made a bright ring some
	// twelve degrees wide round the sun (before the disc held its six thousandths), and at a half the glow spread so
	// broad that, with the sun just out of the picture, it washed half the sky white (the art director on build 1760's
	// Midway 05: real haze is 0.75 to 0.8, which keeps the bright patch round the sun and the rest of the sky its
	// blue-grey). Game-wide curve and bloom stay as they are.
	// "-TheaterSun=0.53,0.006,0.75" (the disc's width in degrees, the share of the light it holds, how steeply forward
	// the haze throws the sun's light); each one given takes the place of its number here.
	double DiscWide = 0.53, DiscShare = 0.006, HazeForward = 0.75;
	FString SunAsked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSun="), SunAsked, false))
	{
		TArray<FString> Parts;
		SunAsked.ParseIntoArray(Parts, TEXT(","), false);
		double* const Into[] = { &DiscWide, &DiscShare, &HazeForward };
		for (int32 K = 0; K < Parts.Num() && K < 3; ++K)
		{
			if (!Parts[K].TrimStartAndEnd().IsEmpty())
			{
				*Into[K] = FCString::Atod(*Parts[K]);
			}
		}
	}
	DiscWide = FMath::Clamp(DiscWide, 0.1, 5.0);
	DiscShare = FMath::Clamp(DiscShare, 0.0, 1.0);
	HazeForward = FMath::Clamp(HazeForward, 0.0, 0.999);
	Key.LightSourceAngle = static_cast<float>(DiscWide);
	Key.AtmosphereSunDiskColorScale = FLinearColor(static_cast<float>(DiscShare), static_cast<float>(DiscShare), static_cast<float>(DiscShare));
	// The sun's light through the air above each point, not through the whole of it as at sea level. The engine gives
	// every surface the sun as it reaches the ground, and at Bremen's 13 degrees that light has crossed some four
	// thicknesses of the air and the ozone over it: about 1.45 as much red as green and 0.6 as much blue, which turned
	// the B-17's olive drab (hue 63) to a tan of hue 40 in sun while its white star, near the top of the scale, hid
	// the cast (the art director on the Mac's release 2 film, 19:32Z; the paint itself is hue-neutral, M_MapSurface).
	// A bomber 7.5 km up has about four tenths of the air over it: some 1.25 red to green and 0.9 blue. The ground and
	// the sea, at the bottom of the air, keep the light they had. "-TheaterAirByHeight=0" lights all as before.
	static const bool bAirByHeight = []
	{
		int32 Asked = 1;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterAirByHeight="), Asked);
		return Asked != 0;
	}();
	Key.bPerPixelAtmosphereTransmittance = bAirByHeight;
	Key.MarkRenderStateDirty();
	// (The battle's sky is the battle look's, made on this actor in AirSkyBegin and taken away with it.)
	TArray<USkyAtmosphereComponent*> Atmospheres;
	GetComponents(Atmospheres);
	// (and over the sea the air's dust thinner, a clear trade-wind day's: the battle look's white dust, the same over
	// the sea as over Bremen's land, made the horizon read (215,218,225), "only +10 blue", where pale blue is about
	// (196,212,230), the art director on 2b177386's frames, 22 November 04:16Z. "-TheaterSeaDust=0.6" tries another
	// share; once to each sky the battle look makes, by its tag)
	double SeaDust = 0.6;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaDust="), SeaDust);
	SeaDust = FMath::Clamp(SeaDust, 0.0, 2.0);
	const bool bSkyOverSea = !(State.IsValid() && State->Board.StageKey.empty() && !State->Board.Land.empty());
	static const FName SeaDustTag(TEXT("TheaterSeaDust"));
	for (USkyAtmosphereComponent* Atmosphere : Atmospheres)
	{
		Atmosphere->SetMieAnisotropy(static_cast<float>(HazeForward));
		if (bSkyOverSea && !Atmosphere->ComponentTags.Contains(SeaDustTag))
		{
			Atmosphere->ComponentTags.Add(SeaDustTag);
			Atmosphere->SetMieScatteringScale(Atmosphere->MieScatteringScale * static_cast<float>(SeaDust));
		}
	}
	UE_LOG(LogTheater, Display, TEXT("Air battle: the sun a disc %.2f degrees wide holding %.4f of its light, the haze throwing it forward at %.2f (%d skies); its light through the air %s; the air's dust %s"),
		DiscWide, DiscShare, HazeForward, Atmospheres.Num(), bAirByHeight ? TEXT("over each point") : TEXT("as at sea level"),
		bSkyOverSea ? *FString::Printf(TEXT("%.2f of the battle look's over the sea"), SeaDust) : TEXT("the battle look's over the land"));
}

bool AAirBattle::SkyEveryStep()
{
	static const bool bAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterAirSkyRecapture"));
	return bAsked;
}

void AAirBattle::SkyLowerOnce()
{
	// The sky light's lower half (the light thrown up from under the eye) laid once for the whole raid, here inside
	// the press's own long frame, for the bounce under the raid's first eye. A capture of the sky light holds the draw
	// thread 0.65 to 0.9 s on the Mac (the 1.5 launch candidate's cold Rhineland raid: three captures in its first 9 s,
	// at the press, over the cumulus tops and at the cut to the target, and a draw stall of 0.8 to 0.9 s after each,
	// the game thread idle; a second raid in the same session the same). So the capture is run now and waited for,
	// and the opening card holds a moment longer instead of the first seconds of flight freezing. After this the
	// light from below moves the UnderLight alone, which has carried the bellies since 22 November.
	// "-TheaterAirSkyRecapture": the old way, a capture at every step of 0.08 and none waited for here.
	// "-TheaterAirSkyNoCapture": no capture at all this raid, the sky light's lower half left as the table's (to weigh
	// the look against the press's wait on the Mac).
	if (SkyEveryStep() || bSkyLowerLaid)
	{
		return;
	}
	bSkyLowerLaid = true;
	static const bool bNone = FParse::Param(FCommandLine::Get(), TEXT("TheaterAirSkyNoCapture"));
	if (bNone)
	{
		UE_LOG(LogTheater, Display, TEXT("Air battle: the sky light's lower half left as the table's, no capture this raid (-TheaterAirSkyNoCapture); the light from below moves only the lamp under the aircraft."));
		return;
	}
	if (Eye)
	{
		WeatherAt(Eye->GetActorLocation());
	}
	const float B = static_cast<float>(WeatherBounce);
	int32 Laid = 0;
	for (const FKeptSky& Kept : KeptSkies)
	{
		if (USkyLightComponent* Sky = Kept.Sky.Get())
		{
			Sky->bLowerHemisphereIsBlack = true;
			Sky->SetLowerHemisphereColor(FLinearColor(0.82f * B, 0.84f * B, 0.88f * B, 1.0f));
			Sky->RecaptureSky();
			++Laid;
		}
	}
	BounceShown = WeatherBounce;
	double Waited = 0.0;
	if (Laid > 0 && GetWorld())
	{
		const double Began = FPlatformTime::Seconds();
		USkyLightComponent::UpdateSkyCaptureContents(GetWorld());
		FlushRenderingCommands();
		Waited = 1000.0 * (FPlatformTime::Seconds() - Began);
	}
	UE_LOG(LogTheater, Display, TEXT("Air battle: the sky light's lower half laid once, at the press, for the first eye's light from below %.2f (%d skies); its capture waited for in %.0f ms inside the press. After this the light from below moves only the lamp under the aircraft (-TheaterAirSkyRecapture captures the sky again at every step of 0.08)."),
		B, Laid, Waited);
}

void AAirBattle::LightTheBattle()
{
	// The battle's day: every lamp white, the key (turned to the battle's sun) at the table's day strength and warmth
	// and the others at the fill's, the sky's light the day's; both as the day's weather has them where the eye is. Said again every frame where anything differs: the room
	// can still light itself for the hour under the battle (a showpiece's picture retaken, a portrait, an answer
	// changing the season), and the battle's light must stand (the review, 06:50Z).
	for (const FKeptLamp& Kept : KeptLamps)
	{
		UDirectionalLightComponent* Light = Kept.Lamp.Get();
		if (!Light)
		{
			continue;
		}
		const bool bKey = Light == BattleKey.Get();
		// (the day's weather where the eye is: under a deck the sun's light falls with its cover, WeatherAt)
		const float Want = bKey ? 7.0f * static_cast<float>(WeatherSun) : 0.75f;
		if (!Light->GetLightColor().Equals(FLinearColor::White, 0.002f))
		{
			Light->SetLightColor(FLinearColor::White);
		}
		if (!FMath::IsNearlyEqual(Light->Intensity, Want, 0.001f))
		{
			Light->SetIntensity(Want);
		}
		// (over land with no stage the sun white, 6,500 K, and the sky's air alone warming it by its height: the table's
		// 4,900 K (red over blue 1.64), the grade's gain (1.24) and the air's own reddening at a low sun (1.2 to 1.3)
		// multiplied to 2.5 and turned Bremen's grey-olive winter fields desert tan (the art director on the film of build
		// 1786, 23:27Z: sat 0.40, blue under red by 43; the ground thread's trace, 23:22Z). "-TheaterKeyWarmth=6500"
		// sets the key's temperature for any battle; KeyWarmth, set as the battle opens)
		if (bKey && !FMath::IsNearlyEqual(Light->Temperature, KeyWarmth, 1.0f))
		{
			Light->SetTemperature(KeyWarmth);
		}
	}
	for (const FKeptSky& Kept : KeptSkies)
	{
		USkyLightComponent* Sky = Kept.Sky.Get();
		if (!Sky)
		{
			continue;
		}
		if (!Sky->GetLightColor().Equals(FLinearColor::White, 0.002f))
		{
			Sky->SetLightColor(FLinearColor::White);
		}
		const float SkyWant = 0.72f * static_cast<float>(WeatherSky);
		if (!FMath::IsNearlyEqual(Sky->Intensity, SkyWant, 0.001f))
		{
			Sky->SetIntensity(SkyWant);
		}
		// (the light from below, moved only when it has moved enough to see: each change has the sky light captured
		// again, so in steps of 0.08, about seven captures in a climb through an overcast's tops; the pre-check, 00:55Z.
		// Only under "-TheaterAirSkyRecapture" now: each capture held the Mac's draw thread 0.65 to 0.9 s, raid after
		// raid. Without it the lower half is laid once, inside the press, by SkyLowerOnce, and the light from below
		// moves the UnderLight alone; the 1.5 launch candidate's cold Rhineland raid, 24 November)
		if (SkyEveryStep() && FMath::Abs(WeatherBounce - BounceShown) > 0.08)
		{
			const float B = static_cast<float>(WeatherBounce);
			Sky->bLowerHemisphereIsBlack = true;
			Sky->SetLowerHemisphereColor(FLinearColor(0.82f * B, 0.84f * B, 0.88f * B, 1.0f));
			Sky->RecaptureSky();
		}
	}
	if (FMath::Abs(WeatherBounce - BounceShown) > 0.08)
	{
		BounceShown = WeatherBounce;
	}
	// The light thrown up on the aircraft (UnderLight): over the sea under the cloud (WeatherBounce at its least, 0.06)
	// 0.23 of a blue-grey, the bellies 40 to 60; over a whole sunlit deck (Bremen's, about 0.77) 0.67 of a near white,
	// 80 to 110; between, by how much of the view down is sunlit cloud. Over land with no stage a grey-green. None by
	// night. Measured against the key's 7 on a sunlit side: the art director's targets on a1acb552's frames.
	static const float UnderScale = []
	{
		float Asked = 1.0f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterUnderLight="), Asked);
		return FMath::Clamp(Asked, 0.0f, 4.0f);
	}();
	if (!UnderLight && bTableHidden && UnderScale > 0.0f && Root)
	{
		UDirectionalLightComponent* Made = NewObject<UDirectionalLightComponent>(this, TEXT("UnderLight"));
		Made->SetupAttachment(Root);
		Made->SetMobility(EComponentMobility::Movable);
		Made->SetCastShadows(false);
		Made->SetAtmosphereSunLight(false);
		Made->bAffectTranslucentLighting = false;
		Made->SetSpecularScale(0.0f);
		Made->SetVolumetricScatteringIntensity(0.0f);
		// (the aircraft only: the sea, the ground, the ships and the smoke keep their own light)
		Made->LightingChannels.bChannel0 = false;
		Made->LightingChannels.bChannel1 = true;
		Made->SetIntensity(0.0f);
		Made->RegisterComponent();
		// (shining straight up: it lights only faces turned down)
		Made->SetWorldRotation(FRotator(90.0, 0.0, 0.0));
		UnderLight = Made;
	}
	if (UDirectionalLightComponent* Under = UnderLight.Get())
	{
		const bool bNight = State.IsValid() && State->Board.bNight;
		const bool bOverLand = State.IsValid() && State->Board.StageKey.empty() && !State->Board.Land.empty();
		const double Cloud = FMath::Clamp((WeatherBounce - 0.06) / 0.71, 0.0, 1.0);
		const FLinearColor Low = bOverLand ? FLinearColor(0.80f, 0.84f, 0.74f) : FLinearColor(0.62f, 0.80f, 1.0f);
		const FLinearColor Colour = FMath::Lerp(Low, FLinearColor(0.95f, 0.97f, 1.0f), static_cast<float>(Cloud));
		const float Want = bNight ? 0.0f : UnderScale * static_cast<float>(0.23 + 0.62 * FMath::Max(0.0, WeatherBounce - 0.06));
		if (!FMath::IsNearlyEqual(Under->Intensity, Want, 0.005f))
		{
			Under->SetIntensity(Want);
		}
		if (!Under->GetLightColor().Equals(Colour, 0.004f))
		{
			Under->SetLightColor(Colour);
		}
		if (UnderLogged < 0.0f || FMath::Abs(Want - UnderLogged) > 0.05f)
		{
			UnderLogged = Want;
			UE_LOG(LogTheater, Display, TEXT("Air battle: the light from below %.2f, (%.2f,%.2f,%.2f), over %s (the view down %.2f bright)."),
				Want, Colour.R, Colour.G, Colour.B, bOverLand ? TEXT("land") : TEXT("sea or stage"), WeatherBounce);
		}
	}
}

void AAirBattle::KeepTheTableHidden()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_KeepTheTableHidden);
	// (the campaign's controller still ticks under the battle, before it: when the province under the pointer changes,
	// the map unfolds its crowds and stands every piece not folded into one, at the world's origin where the battle
	// stands. Until the map's put-away hook lines are in, each such piece is put away again before the frame is drawn)
	if (!bTableHidden)
	{
		return;
	}
	for (TActorIterator<AArmyPiece> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden())
		{
			It->SetActorHiddenInGame(true);
		}
	}
	if (Table.IsValid() && !Table->IsHidden())
	{
		Table->SetActorHiddenInGame(true);
	}
	LightTheBattle();
}

void AAirBattle::ShowTheTable()
{
	// The table as it was: its haze, its lamps, its own tick, and its pieces stood again with the crowds folded up.
	if (!bTableHidden)
	{
		return;
	}
	bTableHidden = false;
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
	BattleLook::AirSkyEnd(); // (the 3D builder, 19 November 2026: the battle's sky goes with the battle)
	if (UnderLight)
	{
		UnderLight->DestroyComponent();
		UnderLight = nullptr;
		UnderLogged = -1.0f;
	}
	for (const FKeptLamp& Kept : KeptLamps)
	{
		if (UDirectionalLightComponent* Light = Kept.Lamp.Get())
		{
			Light->SetRelativeRotation(Kept.Rotation);
			Light->SetIntensity(Kept.Intensity);
			Light->SetLightColor(Kept.Colour);
			if (!FMath::IsNearlyEqual(Light->Temperature, Kept.Temperature, 1.0f))
			{
				Light->SetTemperature(Kept.Temperature);
			}
			if (Light->LightFunctionMaterial != Kept.LightFunction.Get())
			{
				// (the deck's shade goes with the battle)
				Light->SetLightFunctionMaterial(Kept.LightFunction.Get());
				Light->SetLightFunctionFadeDistance(Kept.LightFunctionFade);
				Light->SetLightFunctionDisabledBrightness(Kept.LightFunctionDisabled);
			}
			if (!FMath::IsNearlyEqual(Light->LightSourceAngle, Kept.SourceAngle) || !Light->AtmosphereSunDiskColorScale.Equals(Kept.SunDisc)
				|| (Light->bPerPixelAtmosphereTransmittance != 0) != Kept.bAirByHeight)
			{
				Light->LightSourceAngle = Kept.SourceAngle;
				Light->AtmosphereSunDiskColorScale = Kept.SunDisc;
				Light->bPerPixelAtmosphereTransmittance = Kept.bAirByHeight;
				Light->MarkRenderStateDirty();
			}
		}
	}
	for (const FKeptSky& Kept : KeptSkies)
	{
		if (USkyLightComponent* Sky = Kept.Sky.Get())
		{
			Sky->SetIntensity(Kept.Intensity);
			Sky->SetLightColor(Kept.Colour);
			if (Sky->bLowerHemisphereIsBlack != Kept.bLowerBlack || !Sky->LowerHemisphereColor.Equals(Kept.LowerColour))
			{
				Sky->bLowerHemisphereIsBlack = Kept.bLowerBlack;
				Sky->SetLowerHemisphereColor(Kept.LowerColour);
				Sky->RecaptureSky();
			}
		}
	}
	for (const TWeakObjectPtr<ACampaignEnvironment>& Room : KeptRooms)
	{
		if (Room.IsValid())
		{
			Room->SetActorTickEnabled(true);
		}
	}
	if (ACampaignMapActor* Map = Table.Get())
	{
		Map->SetActorHiddenInGame(false);
		// (the map's own way of standing its pieces again, which folds the crowds up: its put-away where the hook lines
		// are in, else its bare-ground switch off and on)
		if (!AirParts::PutAway(Map, false))
		{
			Map->SetPiecesShown(false);
			Map->SetPiecesShown(true);
		}
	}
	else
	{
		for (const TWeakObjectPtr<AActor>& Piece : KeptPieces)
		{
			if (Piece.IsValid())
			{
				Piece->SetActorHiddenInGame(false);
			}
		}
	}
	BattleKey.Reset();
	KeptHazes.Reset();
	KeptLamps.Reset();
	KeptSkies.Reset();
	KeptRooms.Reset();
	KeptPieces.Reset();
}

void AAirBattle::LaySky()
{
	// The day's clouds (AirSky, the art director's sky at bomber height): laid once on a square that repeats round the
	// eye, so there are clouds wherever the fight goes, lifted over the stage's hills where they are drawn; once they
	// are drawn, the flight core is told where the deck is, so that falls begun above it pass out of sight in it (where
	// a tile of it is drawn, and nowhere else: with no clouds drawn, every fall goes on down).
	SkyFinish(false); // (a turn painted aside reads the field laid again here)
	FAirBattleState& S = *State;
	S.Show.CloudTop = 0.0;
	S.Show.DeckAt = nullptr;
	S.Show.DeckReach = 0.0;
	const FVector Sun = TowardSun();
	S.SkyPlace.Sun = AirSky::Vec(Sun.Y, Sun.X, Sun.Z);
	S.SkyPlace.Ground = S.Show.Ground;
	// ("-TheaterWeather=0.2" lays the day's cloud over that share of the sky, "-TheaterWeather=deck,0.95" a deck of it:
	// each kind of day for a picture, the battle's own height and seed kept)
	FString Weather;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterWeather="), Weather, false))
	{
		FString Kind, Share = Weather;
		if (Weather.Split(TEXT(","), &Kind, &Share))
		{
			S.Board.Cloud.bDeck = Kind.TrimStartAndEnd() == TEXT("deck");
		}
		S.Board.Cloud.Cover = FMath::Clamp(FCString::Atod(*Share), 0.0, 1.0);
		S.Board.Cloud.Word = "asked";
	}
	S.Sky = AirSky::Lay(S.Board.Cloud, S.SkyPlace);
	// (the painter's own field and ground, for DrawSky's turns on the pool's threads: nothing they read is the frame's)
	S.SkyAside = S.Sky;
	// (the stage's ground copied for the pool; the board's own land reads nothing it changes, so it is handed on as it is)
	S.SkyAside.Ground = Stage && Stage->IsRaised() ? (S.Show.Ground ? Stage->GroundAside() : std::function<double(double, double)>()) : S.Show.Ground;
	const FString Moved = S.Sky.MovedWhy.empty() ? FString() : FString::Printf(TEXT(", moved %.0f m %s"), S.Sky.Moved, *AirParts::Wide(S.Sky.MovedWhy));
	UE_LOG(LogTheater, Display, TEXT("Air battle: the sky, %s%s from %.0f to %.0f m%s, %s (%.2f of it laid); %d clouds of %d puffs, %.0f%% of the sky round the target."),
		S.Board.Cloud.bGiven ? TEXT("") : TEXT("(none given) "), S.Sky.Sky.bDeck ? TEXT("a deck") : TEXT("scattered cumulus"), S.Sky.Base, S.Sky.Top,
		*Moved, S.Board.Cloud.Word.empty() ? TEXT("no word") : *AirParts::Wide(S.Board.Cloud.Word), S.Sky.Sky.CoverLaid(), static_cast<int32>(S.Sky.Clouds.size()),
		static_cast<int32>(S.Sky.Puffs.size()), S.Sky.Cover * 100.0);
	if (S.Sky.Puffs.empty())
	{
		return;
	}
	UMaterialInterface* Highlight = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Highlight)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: M_Highlight is missing (run Tools/build_content.sh): no clouds are drawn, and falls go on down."));
		return;
	}
	// The puffs drawn from the sheet of cloud shapes (the art director's brief, item 4: torn fragments of our own in
	// place of soft discs) by M_Cloud, which reads the sheet ("Sheet") by the points' first texture coordinate and the
	// sun's way on them by their second (Tools/build_content.py); without M_Cloud (the content not built again yet), the
	// soft discs on M_Highlight, as before. The shadows on the sea stay soft discs on M_Highlight.
	UMaterialInterface* CloudMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Cloud.M_Cloud"));
	if (CloudMaterial && !CloudSheet)
	{
		const std::vector<uint8_t>& Rgba = AirParts::WaitSheet();
		const int32 Side = AirSky::FragmentCells * AirSky::FragmentSize;
		if (Rgba.size() == static_cast<size_t>(Side) * static_cast<size_t>(Side) * 4)
		{
			TArray<FColor> Pixels;
			Pixels.SetNumUninitialized(Side * Side);
			for (int32 Index = 0; Index < Side * Side; ++Index)
			{
				const size_t C = static_cast<size_t>(Index) * 4;
				Pixels[Index] = FColor(Rgba[C], Rgba[C + 1], Rgba[C + 2], Rgba[C + 3]);
			}
			// (plain numbers, not colours: twice each is the share of the cloud's light; the edge held, not repeated)
			// (a name of its own each battle: a second battle in one run would otherwise make its sheet over the first,
			// still uncollected, and the engine destroys that one in place on the game thread; the pre-check, 22:11Z)
			CloudSheet = CampaignTextures::Create(Side, Side, Pixels, false, false, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("AirCloudSheet")));
		}
	}
	// (the material asked first: with no M_Cloud no sheet is made, and its missing must still be said; the pre-check, 22:11Z)
	if (!CloudMaterial)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: M_Cloud is missing (run Tools/build_content.sh): the clouds are soft discs, not the sheet's shapes."));
	}
	else if (!CloudSheet)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: the sheet of cloud shapes was not made: the clouds are soft discs."));
		CloudMaterial = nullptr;
	}
	bCloudShapes = CloudMaterial != nullptr;
	S.SkyRooms.SetSprites(bCloudShapes);
	Clouds = NewObject<UProceduralMeshComponent>(this, TEXT("AirClouds"));
	Clouds->bUseAsyncCooking = false;
	Clouds->SetupAttachment(Root);
	Clouds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Clouds->SetCastShadow(false);
	Clouds->RegisterComponent();
	CloudInk = UMaterialInstanceDynamic::Create(bCloudShapes ? CloudMaterial : Highlight, this);
	if (bCloudShapes)
	{
		CloudInk->SetTextureParameterValue(TEXT("Sheet"), CloudSheet);
	}
	CloudShadowInk = UMaterialInstanceDynamic::Create(Highlight, this);
	// (the clouds' sunlit tops a third brighter than their paint: on build 1732, through the battle grade, they read 226
	// to 231 at the 99.5th percentile of the picture, the art director's mark 238 to 245 with no more than about one
	// per cent of it at 250 or over (16:21Z). The paint's top, AirSky.cpp's SunSide, is already 252, so the lift is the
	// ink's; the painter paints the shaded sides, the bases and the shadows that much darker, so that they stand where
	// they were (her 16:31Z). "-TheaterCloudBright=1.2" tries another)
	float CloudBright = 1.3f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterCloudBright="), CloudBright);
	CloudBright = FMath::Clamp(CloudBright, 0.5f, 3.0f);
	CloudInk->SetScalarParameterValue(TEXT("Brightness"), CloudBright);
	CloudShadowInk->SetScalarParameterValue(TEXT("Brightness"), CloudBright);
	S.SkyRooms.SetInk(CloudBright);
	CloudShadows = NewObject<UProceduralMeshComponent>(this, TEXT("AirCloudShadows"));
	CloudShadows->bUseAsyncCooking = false;
	CloudShadows->SetupAttachment(Root);
	CloudShadows->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CloudShadows->SetCastShadow(false);
	// (under everything see-through, whichever way DrawSky turns the rest: the air kit is at 0 or -30, the clouds at -10
	// or -20, the ground's kit at -20 or -10)
	CloudShadows->SetTranslucentSortPriority(-40);
	CloudShadows->RegisterComponent();
	// (the near clouds' own mesh, drawn after the far ones: DrawSky sets its order with theirs)
	NearClouds = NewObject<UProceduralMeshComponent>(this, TEXT("AirNearClouds"));
	NearClouds->bUseAsyncCooking = false;
	NearClouds->SetupAttachment(Root);
	NearClouds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NearClouds->SetCastShadow(false);
	NearClouds->RegisterComponent();
	SkyOrder = 0;
	SkyNearTurns = 0;
	SkyBothTurns = 0;
	SkyAtOnce = 0;
	SkyLate = 0;
	SkyPaintSeconds = 0.0;
	SkySendSeconds = 0.0;
	SkySlowest = 0.0;
	SkyClockAt = -1.0;
	SkyEyeWas = FVector(0.0, 0.0, -1e9);
	SkyStepWas = 0.0;
	bSkyAtOnce = FParse::Param(FCommandLine::Get(), TEXT("TheaterSkyAtOnce"));
	SkyLays = 0;
	SkyShadePaints = 0;
	SkyLayBytes = 0.0;
	// The first turn of the puffs, to an eye behind the raid; after that DrawSky turns them as the eye moves.
	const AirSky::Vec Drift = S.Sky.DriftAt(S.Show.AirNow());
	const AirSky::Rgb Light = S.Board.bNight ? AirSky::Rgb(0.12f, 0.13f, 0.17f) : AirSky::Rgb(1.0f, 1.0f, 1.0f);
	const AirSky::Vec FirstEye(S.Board.TargetX, S.Board.TargetY - 6000.0, 5000.0);

	// GS6 (the speed gate's clouds on the graphics chip): the same puffs, turned to the eye where the points are placed,
	// every frame, by M_CloudTurned (Battle/AirSkyChip.inl, the painter's own arithmetic, checked point by point against
	// it by Tools/air_show/air_sky_chip_test). This thread lays a table of the puffs, far to near, in a float picture,
	// and lays it again only when that order has gone stale (a fraction of a millisecond, a few times a second in a
	// fast chase); one mesh, Clouds, made once, reads it. No points are turned, filled or sent as the eye moves, on any
	// thread, and no turn shows late. The shadows on the sea are painted here, alone, as loosely as the far room painted
	// them (a quarter of a millisecond). "-TheaterSkyChip=0" turns them in the two rooms, as before; so does a missing
	// M_CloudTurned (Tools/build_content.sh) or soft discs. "-TheaterSkyChipSort=" the order's staleness, a share of the
	// nearest drawn puff's distance (0.025).
	bSkyChip = false;
	S.SkyChip.reset();
	CloudTable = nullptr;
	int32 ChipAsked = 1;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSkyChip="), ChipAsked);
	UMaterialInterface* TurnedMaterial = bCloudShapes && ChipAsked != 0
		? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_CloudTurned.M_CloudTurned")) : nullptr;
	if (bCloudShapes && ChipAsked != 0 && !TurnedMaterial)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: M_CloudTurned is missing (run Tools/build_content.sh): the clouds are turned in the two rooms, as before."));
	}
	UMaterialInstanceDynamic* TurnedInk = nullptr;
	if (TurnedMaterial)
	{
		std::shared_ptr<AirSky::Chip> Laid = std::make_shared<AirSky::Chip>();
		double Sort = AirSky::Chip::DefaultOrderTurn;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterSkyChipSort="), Sort);
		Laid->SetOrderTurn(Sort);
		Laid->SetInk(CloudBright);
		const int32 Wide = AirSky::Chip::Wide, High = Laid->Rows();
		UTexture2D* Picture = UTexture2D::CreateTransient(Wide, High, PF_A32B32G32R32F, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("AirCloudTable")));
		if (Picture && Picture->GetPlatformData() && Picture->GetPlatformData()->Mips.Num() > 0)
		{
			Picture->SRGB = false;
			Picture->Filter = TF_Nearest;
			Picture->AddressX = TA_Clamp;
			Picture->AddressY = TA_Clamp;
			Picture->NeverStream = true;
			Laid->Lay(S.SkyAside, FirstEye, Drift, S.SkyPlace.Sun, Light);
			{
				FTexture2DMipMap& Mip = Picture->GetPlatformData()->Mips[0];
				const int64 Bytes = static_cast<int64>(Wide) * High * 4 * static_cast<int64>(sizeof(float));
				Mip.BulkData.Lock(LOCK_READ_WRITE);
				void* Into = Mip.BulkData.Realloc(Bytes);
				FMemory::Memzero(Into, Bytes);
				FMemory::Memcpy(Into, Laid->Table().data(), FMath::Min<int64>(Bytes, static_cast<int64>(Laid->Table().size() * sizeof(float))));
				Mip.BulkData.Unlock();
			}
			Picture->UpdateResource();
			TurnedInk = UMaterialInstanceDynamic::Create(TurnedMaterial, this);
			TurnedInk->SetTextureParameterValue(TEXT("Sheet"), CloudSheet);
			TurnedInk->SetTextureParameterValue(TEXT("Table"), Picture);
			TurnedInk->SetScalarParameterValue(TEXT("Brightness"), CloudBright);
			TurnedInk->SetScalarParameterValue(TEXT("UnitsPerMetre"), static_cast<float>(UnitsPerMetre));
			CloudTable = Picture;
			S.SkyChip = Laid;
			bSkyChip = true;
			SkyLays = 1;
			SkyLayBytes = static_cast<double>(Laid->RowsUsed()) * Wide * 16.0;
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Air battle: the clouds' table could not be made: the clouds are turned in the two rooms, as before."));
		}
	}
	if (bSkyChip)
	{
		// (the shadows alone, into the rooms' own store for them, which the shadows' mesh is made from and filled from)
		AirParts::ShadowsAlone(S.SkyShade, CloudBright);
		std::vector<AirSky::Vec> NoPoints;
		std::vector<uint8_t> NoRgba;
		S.SkyShade.Paint(S.SkyAside, FirstEye, Drift, S.SkyPlace.Sun, Light, NoPoints, NoRgba, &S.SkyRooms.Shadows, &S.SkyRooms.ShadowRgba, nullptr);
		SkyLine = 0.0;
	}
	else
	{
		S.SkyRooms.SetSweep(AirSky::Vec(0.0, 0.0, 0.0)); // (a sky laid again keeps no sweep from the last one's turns)
		S.SkyRooms.PaintBoth(S.SkyAside, FirstEye, Drift, S.SkyPlace.Sun, Light);
		SkyLine = S.SkyRooms.Line();
	}
	// Each room's mesh, its slots fixed (each keeps its triangles, only its points move as the eye does).
	auto Make = [this, &Drift](UProceduralMeshComponent* Mesh, const AirSky::Painter& Brush, int32 Room, const std::vector<AirSky::Vec>& From, const std::vector<uint8_t>& FromRgba,
		TArray<FVector>& Points, TArray<FColor>& Colours, const std::vector<float>* FromUvs, TArray<FVector2D>* Places, TArray<FVector2D>* Ways, UMaterialInterface* Ink)
	{
		const std::vector<int> Made = Brush.Triangles(Room);
		TArray<int32> Triangles;
		Triangles.Reserve(static_cast<int32>(Made.size()));
		for (const int Index : Made)
		{
			Triangles.Add(Index);
		}
		Points.SetNum(static_cast<int32>(From.size()));
		Colours.SetNum(static_cast<int32>(From.size()));
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const AirSky::Vec& P = From[static_cast<size_t>(Index)];
			Points[Index] = World(P.X, P.Y, P.Z);
			const size_t C = static_cast<size_t>(Index) * 4;
			Colours[Index] = FColor(FromRgba[C], FromRgba[C + 1], FromRgba[C + 2], FromRgba[C + 3]); // (linear already, as the kit's)
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Points.Num());
		Mesh->SetRelativeLocation(WorldVector(Drift.X, Drift.Y, 0.0));
		if (Places && Ways && FromUvs)
		{
			// (the shapes: each point's place on the sheet, and the sun's way on it, behind the eye and ahead)
			AirParts::SheetPlaces(*FromUvs, *Places, *Ways, Points.Num());
			Mesh->CreateMeshSection(0, Points, Triangles, Normals, *Places, *Ways, TArray<FVector2D>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		}
		else
		{
			Mesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		}
		Mesh->SetMaterial(0, Ink);
	};
	AirSky::Rooms& SkyRooms = S.SkyRooms;
	const bool bShapes = bCloudShapes;
	if (bSkyChip && TurnedInk)
	{
		// GS6: the chip's one mesh: every slot's points with the painter's triangles, each point's place round its puff
		// and its slot's texel in the table. The chip places every point, so they are made at the mesh's own middle (two
		// a centimetre off, to give it a size), and its bounds are stretched over every cloud it can draw (200 km round:
		// the reach is 20 km, and the eye and the wind's carry stay well inside): it is never left out of the picture,
		// and if the material ever fails the points stay where they are made, where nothing is seen.
		const AirSky::Chip& Laid = *S.SkyChip;
		std::vector<float> RingPlaces, SlotPlaces;
		Laid.PointPlaces(RingPlaces, SlotPlaces);
		const int32 Count = static_cast<int32>(RingPlaces.size() / 2);
		TArray<FVector> Points;
		Points.Init(FVector::ZeroVector, Count);
		TArray<FVector2D> Rings, Slots;
		Rings.SetNum(Count);
		Slots.SetNum(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const size_t P = static_cast<size_t>(Index) * 2;
			Rings[Index] = FVector2D(RingPlaces[P], RingPlaces[P + 1]);
			Slots[Index] = FVector2D(SlotPlaces[P], SlotPlaces[P + 1]);
		}
		if (Count > 1)
		{
			Points[0] = FVector(-1.0, -1.0, -1.0);
			Points[Count - 1] = FVector(1.0, 1.0, 1.0);
		}
		const std::vector<int> Made = Laid.Triangles();
		TArray<int32> Triangles;
		Triangles.Reserve(static_cast<int32>(Made.size()));
		for (const int Index : Made)
		{
			Triangles.Add(Index);
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Count);
		Clouds->SetBoundsScale(2.0e7f);
		Clouds->SetRelativeLocation(WorldVector(Drift.X, Drift.Y, 0.0));
		Clouds->CreateMeshSection(0, Points, Triangles, Normals, Rings, Slots, TArray<FVector2D>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		Clouds->SetMaterial(0, TurnedInk);
		CloudInk = TurnedInk;
		CloudPoints.Reset();
		CloudColours.Reset();
		NearCloudPoints.Reset();
		NearCloudColours.Reset();
		UE_LOG(LogTheater, Display, TEXT("Air battle: the clouds turned to the eye on the graphics chip (M_CloudTurned: %d puffs' room in one mesh made once, %d laid, %d left out; -TheaterSkyChip=0 turns them in the two rooms)."),
			Laid.Room(), Laid.Laid(), Laid.Dropped());
	}
	else
	{
		Make(Clouds, SkyRooms.Far, SkyRooms.Far.RoomFor(S.SkyAside), SkyRooms.FarPoints, SkyRooms.FarRgba, CloudPoints, CloudColours, bShapes ? &SkyRooms.FarUvs : nullptr,
			bShapes ? &CloudPlaces : nullptr, bShapes ? &CloudWays : nullptr, CloudInk);
		Make(NearClouds, SkyRooms.Near, SkyRooms.NearRoom(), SkyRooms.NearPoints, SkyRooms.NearRgba, NearCloudPoints, NearCloudColours, bShapes ? &SkyRooms.NearUvs : nullptr,
			bShapes ? &NearCloudPlaces : nullptr, bShapes ? &NearCloudWays : nullptr, CloudInk);
	}
	Make(CloudShadows, SkyRooms.Far, AirSky::Painter::MostShadows, SkyRooms.Shadows, SkyRooms.ShadowRgba, ShadowPoints, ShadowColours, nullptr, nullptr, nullptr, CloudShadowInk);
	// (drawn: now the deck is where falls pass out of sight; the field outlives the flight core's hold on it, both
	// being the battle state's)
	if (S.Sky.DeckTop > 0.0)
	{
		const AirSky::Field* Laid = &S.Sky;
		S.Show.CloudTop = S.Sky.DeckTop;
		S.Show.DeckAt = [Laid](double X, double Y)
		{
			AirShow::DeckSpan Here;
			Here.Hide = Laid->DeckAt(X, Y, &Here.Foot);
			return Here;
		};
		// (drawn, more than half, within five sixths of the clouds' reach from the eye: past it, falls go on down)
		S.Show.DeckReach = AirSky::Reach * 5.0 / 6.0;
		// (no tile's foot under the laid base: under it, under the deck for good)
		S.Show.DeckBase = S.Sky.Base;
	}
}

void AAirBattle::LayZenith()
{
	FAirBattleState& S = *State;
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterZenith="), Asked, false))
	{
		TArray<FString> Parts;
		Asked.ParseIntoArray(Parts, TEXT(","), false);
		double* const Into[] = { &ZenithStrength, &ZenithFrom };
		for (int32 K = 0; K < Parts.Num() && K < 2; ++K)
		{
			if (!Parts[K].TrimStartAndEnd().IsEmpty())
			{
				*Into[K] = FCString::Atod(*Parts[K]);
			}
		}
	}
	ZenithStrength = FMath::Clamp(ZenithStrength, 0.0, 12.0);
	ZenithFrom = FMath::Clamp(ZenithFrom, 0.0, 80.0);
	// (by night the sky is the night's; with the battle's own sky left out, the engine's is not lifted)
	if (S.Board.bNight || ZenithStrength <= 0.0 || FParse::Param(FCommandLine::Get(), TEXT("TheaterNoAirSky")))
	{
		return;
	}
	UMaterialInterface* GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Glow.M_Glow"));
	if (!GlowMaterial)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: M_Glow is missing (run Tools/build_content.sh): the sky looking up is the engine's alone."));
		return;
	}
	// A bowl 9 km round the eye, inside the haze's start (15 km), so no haze lies on it, and past every cloud and
	// aircraft near enough to matter: rings from ZenithFrom up, 48 a ring. Its colour is the lift's tint, its alpha
	// how much of it: none at ZenithFrom, all of it at the zenith, rising as the sky's own blue thins (the eye's path
	// through the air up there is shortest). The tint from the battle's grade worked back (contrast 1.42 about 0.18,
	// red 1.08 and blue 0.87, colour kept 0.55 to 0.72): to bring the zenith from (22, 27, 37) to about (55, 105, 185)
	// it wants about 0.01, 0.09 and 0.35 more light in red, green and blue, before the grade.
	// (The art director on build 1780, 20 November 2026: at 0.3 from 25 degrees the overhead read 43 to 50 at 7,500 m,
	// still a dusk; her mark stands at 90 to 110. Worked back through the grade from those two readings, 29 with none
	// and about 46 at 0.3, the mark wants some four times the light: 1.3, from 12 degrees so the blue begins lower.
	// The Mac's picture run tries 0.9, 1.3 and 1.8 for her to pick from.)
	// (Build 1786's sweep, 20 November 23:00Z: over Bremen at 7,500 m the sky 40 to 50 degrees up read (40, 41, 62) at
	// 0.9 and (42, 47, 78) at 1.8, the lift moving it some six levels of green and sixteen of blue for each 0.9, about a
	// sixth of what the working back promised; and the mark is a sky blue with near twice as much green as the tint
	// had. So the tint greener, (12, 110, 255), and 6 at 7,500 m: by those steps about (50, 100, 150) there, some 90 to
	// 95 graded; the Mac's picture run tries 4, 6 and 9.)
	const double Radius = 9000.0 * UnitsPerMetre;
	const int32 Around = 48;
	const double Rings[] = { 0.0, 0.08, 0.18, 0.3, 0.45, 0.6, 0.75, 0.88, 1.0 };
	const int32 RingCount = UE_ARRAY_COUNT(Rings);
	const double Low = FMath::DegreesToRadians(ZenithFrom);
	// (the art director, 23:06Z: strongest straight up, fading to nothing at the foot so the horizon keeps its pale
	// band, as the rings already lay it; and a little weaker on the sun's side, the sky paler toward the sun as a real
	// one is: up to a third less looking straight at it, nothing off across the sun's way or away from it)
	const FVector SunWay = TowardSun().GetSafeNormal();
	TArray<FVector> Points;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colours;
	for (int32 R = 0; R < RingCount; ++R)
	{
		const double Up = Low + (UE_DOUBLE_HALF_PI - Low) * Rings[R];
		const double Rise = (FMath::Sin(Up) - FMath::Sin(Low)) / FMath::Max(1e-6, 1.0 - FMath::Sin(Low));
		const uint8 Share = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(255.0 * FMath::Pow(FMath::Clamp(Rise, 0.0, 1.0), 1.5)), 0, 255));
		const int32 Count = R == RingCount - 1 ? 1 : Around;
		for (int32 K = 0; K < Count; ++K)
		{
			const double Round = 2.0 * UE_DOUBLE_PI * K / Around;
			const FVector Way(FMath::Cos(Up) * FMath::Cos(Round), FMath::Cos(Up) * FMath::Sin(Round), FMath::Sin(Up));
			const double Toward = FMath::Max(0.0, FVector::DotProduct(Way, SunWay));
			const uint8 Here = static_cast<uint8>(FMath::RoundToInt32(Share * (1.0 - 0.33 * Toward * Toward)));
			Points.Add(Way * Radius);
			Normals.Add(-Way);
			Colours.Add(FColor(12, 110, 255, Here));
		}
	}
	for (int32 R = 0; R + 1 < RingCount; ++R)
	{
		const int32 Here = R * Around;
		const int32 Next = (R + 1) * Around;
		for (int32 K = 0; K < Around; ++K)
		{
			const int32 K2 = (K + 1) % Around;
			if (R + 1 == RingCount - 1)
			{
				Triangles.Append({ Here + K, Here + K2, Next });
			}
			else
			{
				Triangles.Append({ Here + K, Here + K2, Next + K2, Here + K, Next + K2, Next + K });
			}
		}
	}
	Zenith = NewObject<UProceduralMeshComponent>(this, TEXT("AirZenith"));
	Zenith->bUseAsyncCooking = false;
	Zenith->SetupAttachment(Root);
	Zenith->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Zenith->SetCastShadow(false);
	// (behind every other see-through thing: the cloud shadows are at -40, the clouds and kits from -30 up)
	Zenith->SetTranslucentSortPriority(-50);
	Zenith->RegisterComponent();
	Zenith->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
	ZenithInk = UMaterialInstanceDynamic::Create(GlowMaterial, this);
	ZenithInk->SetScalarParameterValue(TEXT("Strength"), 0.0f);
	Zenith->SetMaterial(0, ZenithInk);
	ZenithShown = -1.0;
	UE_LOG(LogTheater, Display, TEXT("Air battle: the sky looking up lifted toward a deep blue, %.2f at 7,500 m, from %.0f degrees up."), ZenithStrength, ZenithFrom);
}

void AAirBattle::ZenithAt(const FVector& EyeWorld)
{
	if (!Zenith || !ZenithInk)
	{
		return;
	}
	Zenith->SetWorldLocation(EyeWorld);
	// (the air over the eye thins as it climbs, the engine's sky looking up darkening with it: the low haze keeps the
	// sky light below about 1,000 m (it thins by half every 1,200 m or so), so the lift is none there, rising evenly to
	// all of ZenithStrength at a B-17's 7,500 m, a little more higher still)
	const double High = FMath::Max(0.0, EyeWorld.Z / UnitsPerMetre);
	const double Want = ZenithStrength * FMath::Clamp((High - 1000.0) / 6500.0, 0.0, 1.2);
	if (FMath::Abs(Want - ZenithShown) > 0.002)
	{
		ZenithShown = Want;
		ZenithInk->SetScalarParameterValue(TEXT("Strength"), static_cast<float>(Want));
	}
}

void AAirBattle::LayHazeBand()
{
	FAirBattleState& S = *State;
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterHazeBand="), Asked, false))
	{
		TArray<FString> Parts;
		Asked.ParseIntoArray(Parts, TEXT(","), false);
		double* const Into[] = { &HazeBandStrength, &HazeBandFrom, &HazeBandFull };
		for (int32 K = 0; K < Parts.Num() && K < 3; ++K)
		{
			if (!Parts[K].TrimStartAndEnd().IsEmpty())
			{
				*Into[K] = FCString::Atod(*Parts[K]);
			}
		}
	}
	HazeBandStrength = FMath::Clamp(HazeBandStrength, 0.0, 3.0);
	HazeBandFrom = FMath::Max(0.0, HazeBandFrom);
	HazeBandFull = FMath::Max(HazeBandFrom + 1.0, HazeBandFull);
	// (the sea's square: the stage's, 200 km each way from the board's target (AirStage::BuildSea), or with no stage
	// the scene's own, 300 km (LayGround); both centred on the target)
	SeaMiddle = World(S.Board.TargetX, S.Board.TargetY, 0.0);
	SeaHalf = (Stage && Stage->IsRaised() ? 200000.0 : 300000.0) * UnitsPerMetre;
	if (S.Board.bNight || HazeBandStrength <= 0.0)
	{
		return;
	}
	UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Ink)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: M_Highlight is missing (run Tools/build_content.sh): no haze band over the horizon."));
		return;
	}
	HazeBand = NewObject<UProceduralMeshComponent>(this, TEXT("AirHazeBand"));
	HazeBand->bUseAsyncCooking = false;
	HazeBand->SetupAttachment(Root);
	HazeBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HazeBand->SetCastShadow(false);
	// (behind every other see-through thing but the zenith's bowl, -50: the cloud shadows are at -40, the clouds and
	// kits from -30 up, so a cloud far out still draws over it)
	HazeBand->SetTranslucentSortPriority(-48);
	HazeBand->RegisterComponent();
	HazeBandInk = UMaterialInstanceDynamic::Create(Ink, this);
	HazeBandInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
	// (over the sea the band is the sea's haze, bluer: the art director on 2b177386's frames, 22 November 04:16Z, "haze is
	// still milky", the band in the land's colour over Midway and Pearl)
	const bool bBandOverLand = S.Board.StageKey.empty() && !S.Board.Land.empty();
	HazeBandInk->SetVectorParameterValue(TEXT("Tint"), (bBandOverLand ? AirParts::AirHazeInk : AirParts::SeaHazeInk()) * static_cast<float>(HazeBandStrength));
	HazeBandInk->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
	HazeBandLaidAt = FVector(1e30, 1e30, 1e30);
	HazeBandShown = -1.0;
	UE_LOG(LogTheater, Display, TEXT("Air battle: a haze band over the sea's far edge (the sea %.0f km each way), %.2f of the haze's brightness, from an eye %.0f m up, all of it from %.0f m."),
		SeaHalf / UnitsPerMetre / 1000.0, HazeBandStrength, HazeBandFrom, HazeBandFull);
}

void AAirBattle::LayDeckShade()
{
	bDeckShade = false;
	UDirectionalLightComponent* Key = BattleKey.Get();
	if (!State.IsValid() || !Key || !State->Sky.Sky.bDeck || State->Board.bNight || FParse::Param(FCommandLine::Get(), TEXT("TheaterNoDeckShade")))
	{
		return;
	}
	UMaterialInterface* ShadeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_DeckShade.M_DeckShade"));
	if (!ShadeMaterial)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: M_DeckShade is missing (run Tools/build_content.sh): under the deck the sun is dimmed by the eye's height alone."));
		return;
	}
	const AirSky::DeckShade Shade = AirSky::ShadeOf(State->Sky);
	if (Shade.Side <= 0)
	{
		return;
	}
	// (the picture laid on the world's own axes: the scene's x is the board's y (World), so each row of the picture
	// is a column of the shade's)
	const int32 Side = Shade.Side;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Side * Side);
	for (int32 Row = 0; Row < Side; ++Row)
	{
		for (int32 Col = 0; Col < Side; ++Col)
		{
			const uint8 Light = Shade.Light[static_cast<size_t>(Col) * static_cast<size_t>(Side) + static_cast<size_t>(Row)];
			Pixels[Row * Side + Col] = FColor(Light, Light, Light, 255);
		}
	}
	DeckShadeSheet = CampaignTextures::Create(Side, Side, Pixels, false, true, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("AirDeckShade")));
	if (!DeckShadeSheet)
	{
		return;
	}
	const double U = UnitsPerMetre;
	const FTransform& Here = GetActorTransform();
	const FVector Corner = Here.TransformPosition(World(Shade.Origin.X, Shade.Origin.Y, 0.0));
	const FVector Middle = Here.TransformPosition(World(0.0, 0.0, Shade.Z));
	const FVector Tops = Here.TransformPosition(World(0.0, 0.0, Shade.Top));
	FVector SunWay = TowardSun().GetSafeNormal();
	SunWay.Z = FMath::Max(0.05, SunWay.Z);
	DeckShadeInk = UMaterialInstanceDynamic::Create(ShadeMaterial, this);
	DeckShadeInk->SetTextureParameterValue(TEXT("Shade"), DeckShadeSheet);
	DeckShadeInk->SetVectorParameterValue(TEXT("SunWay"), FLinearColor(static_cast<float>(SunWay.X), static_cast<float>(SunWay.Y), static_cast<float>(SunWay.Z), 0.0f));
	DeckShadeInk->SetVectorParameterValue(TEXT("Origin"), FLinearColor(static_cast<float>(Corner.X), static_cast<float>(Corner.Y), 0.0f, 0.0f));
	DeckShadeInk->SetScalarParameterValue(TEXT("Period"), static_cast<float>(Shade.Period * U));
	DeckShadeInk->SetScalarParameterValue(TEXT("DeckZ"), static_cast<float>(Middle.Z));
	DeckShadeInk->SetScalarParameterValue(TEXT("DeckTop"), static_cast<float>(Tops.Z));
	DeckShadeInk->SetScalarParameterValue(TEXT("Fade"), static_cast<float>(100.0 * U));
	Key->SetLightFunctionMaterial(DeckShadeInk);
	// (drawn to the haze's far end and past it; where it is not drawn, the deck's mean)
	Key->SetLightFunctionFadeDistance(static_cast<float>(400000.0 * U));
	Key->SetLightFunctionDisabledBrightness(static_cast<float>(Shade.Mean()));
	bDeckShade = true;
	UE_LOG(LogTheater, Display, TEXT("Air battle: the deck's shade on the sun, %d squares a side over %.0f km, %.2f of the sun through a tile, %.2f on the whole; read at %.0f m, none over %.0f m."),
		Side, Shade.Period / 1000.0, Shade.Through, Shade.Mean(), Shade.Z, Shade.Top);
}

void AAirBattle::LayDeckSheet()
{
	if (!State.IsValid() || !State->Sky.Sky.bDeck || State->Board.bNight || State->Sky.Clouds.empty() || FParse::Param(FCommandLine::Get(), TEXT("TheaterNoDeckSheet")))
	{
		return;
	}
	UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Ink)
	{
		return;
	}
	const AirSky::Field& Sky = State->Sky;
	// (how solid the far sheet is: an overcast's whole, a broken deck's two thirds or so, so a pale strip can stay at
	// the horizon under it; "-TheaterDeckSheet=0.65" sets it for the day's cover of 0.77)
	const double Cover = Sky.Sky.CoverLaid();
	double Solid = 0.4 + 0.6 * FMath::SmoothStep(0.6, 1.0, Cover);
	double Asked = -1.0;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterDeckSheet="), Asked) && Asked >= 0.0)
	{
		Solid = FMath::Clamp(Asked, 0.0, 1.0);
	}
	DeckSheetZ = 0.5 * (Sky.Base + Sky.Top);
	// (an annulus round the eye at the deck's middle: none within 14 km, where the drawn tiles still stand, all of it
	// from 20 km, where they have thinned into the haze (AirSky::Reach), out past the sea's far edge; rings close
	// enough near in for the haze to lie on it smoothly)
	const double U = UnitsPerMetre;
	const double Rings[] = { 14000.0, 17000.0, 20000.0, 26000.0, 35000.0, 50000.0, 75000.0, 115000.0, 180000.0, 300000.0, 600000.0 };
	const int32 RingCount = UE_ARRAY_COUNT(Rings);
	const int32 Around = 72;
	TArray<FVector> Points;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colours;
	for (int32 R = 0; R < RingCount; ++R)
	{
		const double In = FMath::SmoothStep(14000.0, 20000.0, Rings[R]);
		const uint8 Share = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(255.0 * Solid * In), 0, 255));
		for (int32 K = 0; K < Around; ++K)
		{
			const double Round = 2.0 * UE_DOUBLE_PI * K / Around;
			Points.Add(FVector(FMath::Cos(Round), FMath::Sin(Round), 0.0) * (Rings[R] * U));
			Normals.Add(FVector::UpVector);
			Colours.Add(FColor(255, 255, 255, Share));
		}
	}
	for (int32 R = 0; R + 1 < RingCount; ++R)
	{
		for (int32 K = 0; K < Around; ++K)
		{
			const int32 A = R * Around + K, B = R * Around + (K + 1) % Around, C = (R + 1) * Around + (K + 1) % Around, D = (R + 1) * Around + K;
			// (both ways round: seen from under the deck as well as from over it)
			Triangles.Append({ A, B, C, A, C, D, A, C, B, A, D, C });
		}
	}
	DeckSheet = NewObject<UProceduralMeshComponent>(this, TEXT("AirDeckSheet"));
	DeckSheet->bUseAsyncCooking = false;
	DeckSheet->SetupAttachment(Root);
	DeckSheet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DeckSheet->SetCastShadow(false);
	// (over the haze band, -48, under every cloud, -30 up: the drawn tiles stand over it where they reach)
	DeckSheet->SetTranslucentSortPriority(-45);
	DeckSheet->RegisterComponent();
	DeckSheet->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
	DeckSheetInk = UMaterialInstanceDynamic::Create(Ink, this);
	// (as bright as the clouds' own ink, LaySky's "-TheaterCloudBright")
	float CloudBright = 1.3f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterCloudBright="), CloudBright);
	DeckSheetInk->SetScalarParameterValue(TEXT("Brightness"), CloudBright);
	DeckSheetInk->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
	DeckSheet->SetMaterial(0, DeckSheetInk);
	DeckSheetShown = -2.0;
	UE_LOG(LogTheater, Display, TEXT("Air battle: the deck carried on to the horizon as one sheet at %.0f m, %.2f solid (cover %.2f)."), DeckSheetZ, Solid, Cover);
}

void AAirBattle::LayCirrus()
{
	if (!State.IsValid() || State->Sky.Sky.bDeck || State->Board.bNight || FParse::Param(FCommandLine::Get(), TEXT("TheaterNoCirrus")))
	{
		return;
	}
	const AirSky::Field& Sky = State->Sky;
	// (a clear day's whole, a fair day's a little, none from a scattered day's 0.39 up; "-TheaterCirrus=1" asks for it
	// whatever the day)
	double Strength = 1.0 - FMath::SmoothStep(0.05, 0.32, Sky.Sky.CoverLaid());
	float Asked = -1.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterCirrus="), Asked) && Asked >= 0.0f)
	{
		Strength = FMath::Clamp(static_cast<double>(Asked), 0.0, 2.0);
	}
	if (Strength < 0.02)
	{
		return;
	}
	UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Ink)
	{
		return;
	}
	// Bundles of thin threads 8 to 10 km up, each along the wind aloft (backed a little from the low wind), the threads
	// fanning out and hooking one way at their downwind ends (mares' tails), soft at their sides and ends. Laid round the
	// target out to 45 km, the same every time for the day.
	FRandomStream Roll(1786 + static_cast<int32>(Sky.Base));
	const double Way = Sky.WindWay + 0.35;
	TArray<FVector> Points;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colours;
	const int32 Bundles = FMath::RoundToInt32(8.0 + 10.0 * FMath::Min(Strength, 1.0));
	for (int32 Bundle = 0; Bundle < Bundles; ++Bundle)
	{
		const double Turn = Way + Roll.FRandRange(-0.2f, 0.2f);
		const FVector2D Along(FMath::Cos(Turn), FMath::Sin(Turn));
		const FVector2D Across(-Along.Y, Along.X);
		const double Range = Roll.FRandRange(3000.0f, 45000.0f), Bear = Roll.FRandRange(0.0f, 6.2831853f);
		const FVector2D Middle(Sky.TargetX + Range * FMath::Cos(Bear), Sky.TargetY + Range * FMath::Sin(Bear));
		const double Z = Roll.FRandRange(8000.0f, 10000.0f);
		const double Long = Roll.FRandRange(5000.0f, 16000.0f), Wide = Roll.FRandRange(600.0f, 2200.0f);
		const double Hook = Roll.FRandRange(-0.25f, 0.25f) * Wide;
		const int32 Threads = Roll.RandRange(3, 7);
		for (int32 Thread = 0; Thread < Threads; ++Thread)
		{
			const double Off = Roll.FRandRange(-0.5f, 0.5f) * Wide;
			const double Length = Long * Roll.FRandRange(0.45f, 1.0f);
			const double Start = Roll.FRandRange(-0.5f, 0.5f) * (Long - Length);
			const double Thick = Roll.FRandRange(70.0f, 260.0f);
			const double Peak = FMath::Min(1.0, Strength * Roll.FRandRange(0.18f, 0.42f));
			const int32 Steps = 12;
			const int32 First = Points.Num();
			for (int32 K = 0; K <= Steps; ++K)
			{
				const double Share = static_cast<double>(K) / Steps;
				const double Down = Start + (Share - 0.5) * Length;
				const double Bend = Hook * FMath::Square(FMath::Max(0.0, Share - 0.6) / 0.4);
				const double Fade = FMath::Pow(FMath::Sin(UE_DOUBLE_PI * Share), 0.6) * (0.65 + 0.35 * Roll.FRand());
				const uint8 Alpha = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(255.0 * Peak * Fade), 0, 255));
				const double Spread = Thick * (0.6 + 0.6 * Share);
				for (int32 Side = -1; Side <= 1; ++Side)
				{
					const FVector2D P = Middle + Along * Down + Across * (Off + Bend + Side * Spread);
					Points.Add(World(P.X, P.Y, Z));
					Normals.Add(FVector::UpVector);
					Colours.Add(FColor(255, 255, 255, Side == 0 ? Alpha : 0));
				}
			}
			for (int32 K = 0; K < Steps; ++K)
			{
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const int32 A = First + K * 3 + Side, B = A + 1, C = A + 4, D = A + 3;
					// (both ways round: seen from under it, and from over it on the way up)
					Triangles.Append({ A, B, C, A, C, D, A, C, B, A, D, C });
				}
			}
		}
	}
	Cirrus = NewObject<UProceduralMeshComponent>(this, TEXT("AirCirrus"));
	Cirrus->bUseAsyncCooking = false;
	Cirrus->SetupAttachment(Root);
	Cirrus->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cirrus->SetCastShadow(false);
	// (high over everything: drawn after the haze band, -48, before every cloud and the deck's far sheet)
	Cirrus->SetTranslucentSortPriority(-47);
	Cirrus->RegisterComponent();
	Cirrus->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
	CirrusInk = UMaterialInstanceDynamic::Create(Ink, this);
	CirrusInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
	CirrusInk->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
	CirrusInk->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.93f, 0.95f, 1.0f, 1.0f));
	Cirrus->SetMaterial(0, CirrusInk);
	UE_LOG(LogTheater, Display, TEXT("Air battle: %d bundles of cirrus 8 to 10 km up, %.2f strong (cover %.2f)."), Bundles, Strength, Sky.Sky.CoverLaid());
}

void AAirBattle::LayRainShafts()
{
	if (!State.IsValid() || State->Sky.Sky.bDeck || State->Board.bNight || State->Sky.Clouds.empty() || FParse::Param(FCommandLine::Get(), TEXT("TheaterNoRain")))
	{
		return;
	}
	const AirSky::Field& Sky = State->Sky;
	// (a heavy day's: none at 0.45 of cover, three at 0.55 and over; "-TheaterRain=1" asks for three whatever the day)
	double Rain = FMath::SmoothStep(0.45, 0.55, Sky.Sky.CoverLaid());
	float Asked = -1.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterRain="), Asked) && Asked >= 0.0f)
	{
		Rain = FMath::Clamp(static_cast<double>(Asked), 0.0, 1.0);
	}
	const int32 Most = FMath::RoundToInt32(3.0 * Rain);
	if (Most <= 0)
	{
		return;
	}
	UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Ink)
	{
		return;
	}
	// Under the biggest clouds shown whole, 7 km and more from the target (its view stays clear), 5 km apart at least.
	TArray<TPair<double, int32>> Big;
	for (int32 Index = 0; Index < static_cast<int32>(Sky.Clouds.size()); ++Index)
	{
		const AirSky::Cloud& C = Sky.Clouds[static_cast<size_t>(Index)];
		if (C.bScrap)
		{
			continue;
		}
		const AirSky::Placed Here = Sky.Place(Index, Sky.TargetX, Sky.TargetY, AirSky::Vec(0.0, 0.0, 0.0));
		const double X = C.At.X + Here.Shift.X, Y = C.At.Y + Here.Shift.Y;
		if (Here.Show < 0.9 || Here.Squash < 0.9 || FMath::Square(X - Sky.TargetX) + FMath::Square(Y - Sky.TargetY) < FMath::Square(7000.0))
		{
			continue;
		}
		Big.Add(TPair<double, int32>(C.W * C.H, Index));
	}
	Big.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key > B.Key; });
	FRandomStream Roll(238 + static_cast<int32>(Sky.Base));
	TArray<FVector2D> Taken;
	TArray<FVector> Points;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colours;
	const FVector2D Lee = FVector2D(AirSky::Wind.X, AirSky::Wind.Y).GetSafeNormal();
	// (down its height: its head inside the base, thickest a little under it, thinning to a mist at the ground; across
	// each of its four crossed sheets, none at the edges)
	const double Downs[] = { 0.0, 0.12, 0.45, 0.8, 1.0 };
	const double Thicks[] = { 0.55, 1.0, 0.9, 0.65, 0.2 };
	const double Acrosses[] = { -1.0, -0.5, 0.0, 0.5, 1.0 };
	const double Weights[] = { 0.0, 0.6, 1.0, 0.6, 0.0 };
	const double Peak = 0.16 * Rain;
	for (const TPair<double, int32>& Each : Big)
	{
		if (Taken.Num() >= Most)
		{
			break;
		}
		const AirSky::Cloud& C = Sky.Clouds[static_cast<size_t>(Each.Value)];
		const AirSky::Placed Here = Sky.Place(Each.Value, Sky.TargetX, Sky.TargetY, AirSky::Vec(0.0, 0.0, 0.0));
		const FVector2D Mid(C.At.X + Here.Shift.X, C.At.Y + Here.Shift.Y);
		bool bNear = false;
		for (const FVector2D& Other : Taken)
		{
			bNear = bNear || FVector2D::Distance(Mid, Other) < 5000.0;
		}
		if (bNear)
		{
			continue;
		}
		Taken.Add(Mid);
		const double Top = C.At.Z + Here.Lift + 100.0;
		const double Foot = Sky.Ground ? FMath::Max(0.0, Sky.Ground(Mid.X, Mid.Y)) : 0.0;
		const double Radius = FMath::Clamp(0.28 * C.W, 500.0, 2200.0);
		// (the rain lags behind its cloud as it falls into the slower air under it)
		const FVector2D Lag = -Lee * (0.22 * (Top - Foot));
		const double Turned = Roll.FRandRange(0.0f, 1.5707963f);
		for (int32 Sheet = 0; Sheet < 4; ++Sheet)
		{
			const double Turn = Turned + Sheet * UE_DOUBLE_PI / 4.0;
			const FVector2D Side(FMath::Cos(Turn), FMath::Sin(Turn));
			const int32 First = Points.Num();
			for (int32 Row = 0; Row < 5; ++Row)
			{
				const double Down = Downs[Row];
				const FVector2D Centre = Mid + Lag * Down;
				const double Width = Radius * (1.0 + 0.25 * Down);
				for (int32 Col = 0; Col < 5; ++Col)
				{
					const FVector2D P = Centre + Side * (Acrosses[Col] * Width);
					Points.Add(World(P.X, P.Y, Top - Down * (Top - Foot)));
					Normals.Add(FVector::UpVector);
					Colours.Add(FColor(255, 255, 255, static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(255.0 * Peak * Thicks[Row] * Weights[Col]), 0, 255))));
				}
			}
			for (int32 Row = 0; Row < 4; ++Row)
			{
				for (int32 Col = 0; Col < 4; ++Col)
				{
					const int32 A = First + Row * 5 + Col, B = A + 1, Cc = A + 6, D = A + 5;
					Triangles.Append({ A, B, Cc, A, Cc, D, A, Cc, B, A, D, Cc });
				}
			}
		}
	}
	if (Points.Num() == 0)
	{
		return;
	}
	RainShafts = NewObject<UProceduralMeshComponent>(this, TEXT("AirRainShafts"));
	RainShafts->bUseAsyncCooking = false;
	RainShafts->SetupAttachment(Root);
	RainShafts->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RainShafts->SetCastShadow(false);
	RainShafts->SetTranslucentSortPriority(-12);
	RainShafts->RegisterComponent();
	RainShafts->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
	RainInk = UMaterialInstanceDynamic::Create(Ink, this);
	RainInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
	RainInk->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
	RainInk->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.42f, 0.44f, 0.47f, 1.0f));
	RainShafts->SetMaterial(0, RainInk);
	UE_LOG(LogTheater, Display, TEXT("Air battle: %d rain shafts under the heaviest clouds (cover %.2f)."), Taken.Num(), Sky.Sky.CoverLaid());
}

void AAirBattle::DeckSheetAt(const FVector& EyeWorld)
{
	if (!DeckSheet || !DeckSheetInk || !State.IsValid())
	{
		return;
	}
	const AirSky::Field& Sky = State->Sky;
	const FVector Floor = GetActorTransform().TransformPosition(World(0.0, 0.0, DeckSheetZ));
	DeckSheet->SetWorldLocation(FVector(EyeWorld.X, EyeWorld.Y, Floor.Z));
	// (its colour by where the eye is: over the tops, the deck's top far off, pale and a little shaded; under the base,
	// the base's grey, darker on a closed day; between, the one into the other)
	const double High = EyeWorld.Z / UnitsPerMetre;
	const double Over = FMath::Clamp((High - Sky.Base) / FMath::Max(1.0, Sky.Top - Sky.Base), 0.0, 1.0);
	if (FMath::Abs(Over - DeckSheetShown) < 0.01)
	{
		return;
	}
	DeckSheetShown = Over;
	const double Closed = FMath::SmoothStep(0.85, 1.0, Sky.Sky.CoverLaid());
	const AirSky::Rgb Top = AirSky::SunSide.Mix(AirSky::ShadeSide, 0.18);
	const AirSky::Rgb Base = AirSky::BaseSide * (1.0 - 0.15 * Closed);
	const AirSky::Rgb Here = Base.Mix(Top, Over);
	DeckSheetInk->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Here.R, Here.G, Here.B, 1.0f));
}

void AAirBattle::WeatherAt(const FVector& EyeWorld)
{
	// (how far under the deck the eye is, 0 at its top to 1 at its base: going down through it, the day greys)
	if (!State.IsValid())
	{
		return;
	}
	const AirSky::Field& Sky = State->Sky;
	const double High = EyeWorld.Z / UnitsPerMetre;
	const double Under = Sky.Sky.bDeck && Sky.Top > Sky.Base ? FMath::Clamp((Sky.Top - High) / (Sky.Top - Sky.Base), 0.0, 1.0) : 0.0;
	const AirSky::Lighting Day = AirSky::LightOf(Sky.Sky, Under);
	// (with the deck's shade on the key, the sun is taken away point by point under the deck, the eye's own height
	// no matter: the aircraft over it lit whole, the sea under it in shade but where a hole lets the sun through)
	WeatherSun = bDeckShade ? 1.0 : Day.Sun;
	WeatherSky = Day.Sky;
	// (the light from below: over the cloud tops, the share of the view down that is sunlit cloud, lit as the sun's
	// height lights it; under the base the dark sea or ground; "-TheaterBounce=1" scales it)
	static const double BounceStrength = []
	{
		float Asked = 1.0f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterBounce="), Asked);
		return FMath::Clamp(static_cast<double>(Asked), 0.0, 4.0);
	}();
	const double OverTops = FMath::Clamp((High - Sky.Top) / 300.0, 0.0, 1.0);
	// (how bright a whole sunlit deck throws it, against the sky light's own: the sky's light on the tops and the sun's
	// by its height. A deck under a low sun still reads near white from above, 220 to 245 on Bremen's frames, and the
	// bellies over it read 38 to 41, about a third of the light they want, so a winter sun of 13 degrees gives 1 where
	// it gave 0.34; a high sun at most 1.4; the art director on the Mac's release 2 check, 19:32Z)
	const double SunUp = FMath::Clamp(0.55 + 2.0 * TowardSun().GetSafeNormal().Z, 0.6, 1.4);
	const double FromBelow = 0.06;
	// (none by night: a deck under the moon throws back next to nothing; the key under a deck is dimmed by night as by
	// day, an overcast night darker than a clear one)
	WeatherBounce = State->Board.bNight ? 0.0 : BounceStrength * (FromBelow + (SunUp * Sky.Sky.CoverLaid() - FromBelow) * OverTops);
	// The sky behind the smoke, for it to pale into (the effects kit's SetSky): the air kit's at the eye's height (greyer
	// as it goes down through a deck), the ground kit's under the deck, if there is one. Sent again only when it moves
	// by more than 3 of 255.
	const bool bSea = !State->Board.StageKey.empty();
	const double Closed = Sky.Sky.bDeck ? Sky.Sky.CoverLaid() : 0.0;
	auto Send = [](FBattleEffects* Kit, const FLinearColor& Behind, FLinearColor& Sent, const TCHAR* Whose)
	{
		const float Moved = FMath::Max3(FMath::Abs(Behind.R - Sent.R), FMath::Abs(Behind.G - Sent.G), FMath::Abs(Behind.B - Sent.B));
		if (!Kit || Moved <= 3.0f / 255.0f)
		{
			return;
		}
		if (Sent.R < 0.0f || Moved > 20.0f / 255.0f)
		{
			UE_LOG(LogTheater, Display, TEXT("Air battle: the %s smoke pales into the sky behind it, on screen %.0f, %.0f, %.0f."), Whose,
				255.0f * Behind.R, 255.0f * Behind.G, 255.0f * Behind.B);
		}
		Kit->SetSky(Behind);
		Sent = Behind;
	};
	Send(Effects.Get(), AirParts::SkyBehind(State->Board.bNight, bSea, Under * Closed), SmokeSkySent, TEXT("air's"));
	Send(GroundFx.Get(), AirParts::SkyBehind(State->Board.bNight, bSea, Closed), GroundSkySent, TEXT("ground's"));
}

void AAirBattle::HazeBandAt(const FVector& EyeWorld)
{
	if (!HazeBand || !HazeBandInk)
	{
		return;
	}
	const double U = UnitsPerMetre;
	const double High = EyeWorld.Z / U;
	const double Want = FMath::Clamp((High - HazeBandFrom) / (HazeBandFull - HazeBandFrom), 0.0, 1.0);
	if (FMath::Abs(Want - HazeBandShown) > 0.004)
	{
		HazeBandShown = Want;
		HazeBandInk->SetScalarParameterValue(TEXT("Opacity"), static_cast<float>(Want));
		HazeBand->SetVisibility(Want > 0.0);
	}
	if (Want <= 0.0)
	{
		return;
	}
	// (laid again when the eye has gone 250 m along or 100 m up or down: the edge's angle under the eye moves little)
	const FVector Moved = EyeWorld - HazeBandLaidAt;
	if (FVector2D(Moved.X, Moved.Y).Size() < 250.0 * U && FMath::Abs(Moved.Z) < 100.0 * U)
	{
		return;
	}
	HazeBandLaidAt = EyeWorld;
	HazeBand->SetWorldLocation(FVector(EyeWorld.X, EyeWorld.Y, 0.0));
	// Column by column round the eye: how far the sea's edge lies that way, and so how far under the level it shows
	// (Edge). The band stands at 0.6 of that distance, from the sea (Foot, under the edge, none of it there) up through
	// the edge (all of it from just under it to just over it) and thinning to none 7 degrees above it.
	struct FRow
	{
		double Over;  // degrees over the edge's angle (the first two rows are placed by the foot instead)
		uint8 Share;  // how much of the band
	};
	static const FRow Rows[] = { { 0.0, 0 }, { 0.0, 150 }, { -0.1, 255 }, { 0.4, 255 }, { 1.2, 180 }, { 2.2, 108 }, { 3.4, 52 }, { 5.0, 16 }, { 7.0, 0 } };
	const int32 RowCount = UE_ARRAY_COUNT(Rows);
	const int32 Around = 64;
	const double Ex = EyeWorld.X - SeaMiddle.X, Ey = EyeWorld.Y - SeaMiddle.Y;
	TArray<FVector> Points;
	TArray<FColor> Colours;
	Points.Reserve(Around * RowCount);
	Colours.Reserve(Around * RowCount);
	for (int32 K = 0; K < Around; ++K)
	{
		const double Round = 2.0 * UE_DOUBLE_PI * K / Around;
		const double Dx = FMath::Cos(Round), Dy = FMath::Sin(Round);
		// (the way out of the square from the eye: the nearer of its two sides that way)
		double Out = 1e18;
		if (FMath::Abs(Dx) > 1e-9)
		{
			Out = FMath::Min(Out, ((Dx > 0.0 ? SeaHalf : -SeaHalf) - Ex) / Dx);
		}
		if (FMath::Abs(Dy) > 1e-9)
		{
			Out = FMath::Min(Out, ((Dy > 0.0 ? SeaHalf : -SeaHalf) - Ey) / Dy);
		}
		const double Far = FMath::Max(Out, 20000.0 * U) / U; // metres
		const double Stand = 0.6 * Far;
		const double Edge = -FMath::Atan2(FMath::Max(High, 1.0), Far);
		const double Foot = -FMath::Atan2(FMath::Max(High, 1.0), Stand);
		for (int32 R = 0; R < RowCount; ++R)
		{
			double Angle = Edge + FMath::DegreesToRadians(Rows[R].Over);
			if (R == 0)
			{
				Angle = Foot;
			}
			else if (R == 1)
			{
				Angle = 0.5 * (Foot + Edge + FMath::DegreesToRadians(Rows[2].Over));
			}
			const double Z = FMath::Max(0.0, High + Stand * FMath::Tan(Angle));
			Points.Add(FVector(Dx * Stand * U, Dy * Stand * U, Z * U));
			Colours.Add(FColor(255, 255, 255, Rows[R].Share));
		}
	}
	if (HazeBand->GetNumSections() == 0)
	{
		TArray<int32> Triangles;
		Triangles.Reserve(Around * (RowCount - 1) * 12);
		for (int32 K = 0; K < Around; ++K)
		{
			const int32 K2 = (K + 1) % Around;
			for (int32 R = 0; R + 1 < RowCount; ++R)
			{
				const int32 A = K * RowCount + R, B = K2 * RowCount + R, C = K2 * RowCount + R + 1, D = K * RowCount + R + 1;
				// (both ways round: the band is seen from inside, whichever way the engine takes as its face)
				Triangles.Append({ A, B, C, A, C, D, A, C, B, A, D, C });
			}
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Points.Num());
		HazeBand->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		HazeBand->SetMaterial(0, HazeBandInk);
	}
	else
	{
		HazeBand->UpdateMeshSection(0, Points, TArray<FVector>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
	}
}

void AAirBattle::DrawSky()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_DrawSky);
	if (!Clouds || !Eye || !State.IsValid())
	{
		return;
	}
	FAirBattleState& S = *State;
	// The wind's carry (the battle's wind, as the effects kit is given it), on the air clock, so cumulus drift as the
	// smoke does (a deck stays where it was laid).
	const AirSky::Vec Drift = S.Sky.DriftAt(S.Show.AirNow());
	Clouds->SetRelativeLocation(WorldVector(Drift.X, Drift.Y, 0.0));
	if (NearClouds)
	{
		NearClouds->SetRelativeLocation(WorldVector(Drift.X, Drift.Y, 0.0));
	}
	if (CloudShadows)
	{
		CloudShadows->SetRelativeLocation(WorldVector(Drift.X, Drift.Y, 0.0));
	}
	if (RainShafts)
	{
		RainShafts->SetRelativeLocation(WorldVector(Drift.X, Drift.Y, 0.0));
	}
	const FVector At = Eye->GetActorLocation() / UnitsPerMetre;
	const AirSky::Vec Seen(At.Y, At.X, At.Z);
	// See-through things are drawn one after another, not by depth. Seen from over the clouds' base, the ground's bursts
	// and fires lie under every cloud between, and the air fight over them; seen from under, the air fight (all of it
	// over the base: what is under it is the ground kit's) lies behind every cloud between, a gap showing it, and the
	// ground's over both (the pre-check, 19 November 13:07Z: tracer, flak and smoke showed in front of the clouds that
	// hid their aircraft). The base under the eye is the one the kits are split at (KitAt), with 50 m either side
	// before the order turns, so it does not flicker.
	const double UnderEye = S.Show.Ground ? S.Show.Ground(Seen.X, Seen.Y) : 0.0;
	const double EyeBase = FMath::Max(S.Sky.Base, UnderEye + 300.0) + (SkyOrder == 1 ? -50.0 : SkyOrder == 2 ? 50.0 : 0.0);
	const int32 Want = Seen.Z > EyeBase ? 1 : 2;
	if (Want != SkyOrder)
	{
		SkyOrder = Want;
		Clouds->SetTranslucentSortPriority(Want == 1 ? -10 : -20);
		// (the near clouds just after the far ones, before the kit drawn over them)
		if (NearClouds)
		{
			NearClouds->SetTranslucentSortPriority(Want == 1 ? -5 : -15);
		}
		if (GroundFxMesh.IsValid())
		{
			GroundFxMesh->SetTranslucentSortPriority(Want == 1 ? -20 : -10);
		}
		if (AirFxMesh.IsValid())
		{
			AirFxMesh->SetTranslucentSortPriority(Want == 1 ? 0 : -30);
		}
		// (a rain shaft is under the base, as the ground's kit: under the clouds seen from over them, over them from under)
		if (RainShafts)
		{
			RainShafts->SetTranslucentSortPriority(Want == 1 ? -21 : -12);
		}
	}
	// Turned to the eye as far as it has moved: the near room as often as its puffs' turn shows, the far room (most of
	// the points) and the shadows only when the eye has moved far enough for a far puff's turn to show (AirSky::Rooms).
	// Before, all of it, 132,000 points, was sent 9 to 16 times a second: the builder's Mac run of 19 November, Midway's
	// stutter.
	// And painted aside (round 11): this thread asks the rooms what the eye's move needs (Wants); the painting, and the
	// points made over for the engine (SkyFill), are done on the engine's pool of threads while the frame goes on, and
	// the frame they are done this thread sends them (SkySend), all it does of a turn now. A far turn painted here was
	// most of Midway's slowest frame each five seconds (build 1758: 24 ms at the middle, 35 at the ninetieth). A turn
	// so shows a frame or a few late, a steady lag of the eye's move (2.5 m a frame at 150 m/s); when the eye jumps more
	// than SkyCut in a frame, and SkyCutJump times its last frame's move (a cut, not a glide), the sky is turned at once,
	// here, as before, the cut hiding it.
	// "-TheaterSkyAtOnce" turns it here always.
	const AirSky::Rgb Light = S.Board.bNight ? AirSky::Rgb(0.12f, 0.13f, 0.17f) : AirSky::Rgb(1.0f, 1.0f, 1.0f);
	const AirSky::Vec Sun = S.SkyPlace.Sun;
	const FVector EyeNow(Seen.X, Seen.Y, Seen.Z);
	const double Step = FVector::Dist(EyeNow, SkyEyeWas);
	const bool bCut = Step > SkyCut && (Step > SkyCutJump * SkyStepWas || Step > SkyCutFar);
	if (bCut)
	{
		// (every cut the sky sees is told to the renderer too: the flyover's shots, the pictures, the strategy camera's jumps)
		AirParts::TellCut(GetWorld());
	}
	// (a turn painted aside stands on the picture while the eye goes on a few frames: the painters fade a puff by the
	// lens along that way (AirSky's SetSweep), so that in a glide of hundreds of metres a frame none shows as a disc
	// across the lens. The way's length: SkySweep frames of the lesser of this frame's step and the last one's, so one
	// long frame does not stretch it; none at a cut. The pre-check, round 12)
	const FVector Ahead = bCut ? FVector::ZeroVector : (EyeNow - SkyEyeWas).GetSafeNormal() * (FMath::Min(Step, SkyStepWas) * SkySweep);
	SkyEyeWas = EyeNow;
	SkyStepWas = Step;
	double ThisFrame = 0.0; // (what the sky cost this thread this frame)
	if (bSkyChip && S.SkyChip)
	{
		// GS6: turned on the graphics chip, for the eye as it is each frame (no turn painted aside, none late, no sweep):
		// here only the table laid again when the puffs' order has gone stale, and the shadows painted alone when the
		// far room would have painted them.
		AirSky::Chip& Laid = *S.SkyChip;
		UTexture2D* Picture = CloudTable.Get();
		if (Picture && Laid.Wants(Seen, Drift))
		{
			const double Began = FPlatformTime::Seconds();
			Laid.Lay(S.SkyAside, Seen, Drift, Sun, Light);
			AirParts::SendTable(Picture, Laid.Table(), Laid.RowsUsed());
			++SkyLays;
			SkyLayBytes += static_cast<double>(FMath::Min(Laid.RowsUsed(), Picture->GetSizeY())) * AirSky::Chip::Wide * 16.0;
			const double Took = FPlatformTime::Seconds() - Began;
			SkyPaintSeconds += Took;
			ThisFrame += Took;
		}
		if (CloudShadows && S.SkyShade.Stale(Seen, Drift))
		{
			const double Began = FPlatformTime::Seconds();
			std::vector<AirSky::Vec> NoPoints;
			std::vector<uint8_t> NoRgba;
			AirSky::Rooms& SkyRooms = S.SkyRooms;
			S.SkyShade.Paint(S.SkyAside, Seen, Drift, Sun, Light, NoPoints, NoRgba, &SkyRooms.Shadows, &SkyRooms.ShadowRgba, nullptr);
			const int32 Shown = FMath::Min3(ShadowPoints.Num(), ShadowColours.Num(), static_cast<int32>(FMath::Min(SkyRooms.Shadows.size(), SkyRooms.ShadowRgba.size() / 4)));
			for (int32 Index = 0; Index < Shown; ++Index)
			{
				const AirSky::Vec& P = SkyRooms.Shadows[static_cast<size_t>(Index)];
				ShadowPoints[Index] = World(P.X, P.Y, P.Z);
				const size_t C = static_cast<size_t>(Index) * 4;
				ShadowColours[Index] = FColor(SkyRooms.ShadowRgba[C], SkyRooms.ShadowRgba[C + 1], SkyRooms.ShadowRgba[C + 2], SkyRooms.ShadowRgba[C + 3]);
			}
			if (ShadowPoints.Num() > 0)
			{
				CloudShadows->UpdateMeshSection(0, ShadowPoints, TArray<FVector>(), TArray<FVector2D>(), ShadowColours, TArray<FProcMeshTangent>());
			}
			++SkyShadePaints;
			const double Took = FPlatformTime::Seconds() - Began;
			SkySendSeconds += Took;
			ThisFrame += Took;
		}
		SkySlowest = FMath::Max(SkySlowest, ThisFrame);
		// (the sky's clock, in the log every 5 s: the table's lays and what they cost this thread and sent, the shadows'
		// paints and what they cost, the most the sky cost this thread in a frame; the chip's own share is the GPU's)
		const double Done = FPlatformTime::Seconds();
		if (SkyClockAt < 0.0)
		{
			SkyClockAt = Done;
		}
		else if (Done - SkyClockAt >= 5.0)
		{
			UE_LOG(LogTheater, Log, TEXT("Air battle sky, %.1f s, on the graphics chip: %d table lays, %.2f ms a lay, %.2f MB sent; %d shadow paints, %.2f ms a paint; the slowest %.2f ms (this thread's most in a frame); %d puffs laid, the nearest %.0f m off, laid again past %.1f m."),
				Done - SkyClockAt, SkyLays, SkyLays > 0 ? SkyPaintSeconds * 1000.0 / SkyLays : 0.0, SkyLayBytes / 1.0e6, SkyShadePaints,
				SkyShadePaints > 0 ? SkySendSeconds * 1000.0 / SkyShadePaints : 0.0, SkySlowest * 1000.0, Laid.Laid(), Laid.Nearest(), Laid.Slack());
			SkyLays = 0;
			SkyShadePaints = 0;
			SkyLayBytes = 0.0;
			SkyPaintSeconds = 0.0;
			SkySendSeconds = 0.0;
			SkySlowest = 0.0;
			SkyClockAt = Done;
		}
		return;
	}
	auto Count = [this](int32 What, double PaintSeconds, double SendSeconds)
	{
		(What == static_cast<int32>(AirSky::Rooms::Sent::Both) ? SkyBothTurns : SkyNearTurns) += 1;
		SkyPaintSeconds += PaintSeconds;
		SkySendSeconds += SendSeconds;
	};
	// (a turn painted aside and done: sent; one still painting waits for a later frame, but at a cut its painting is
	// finished here and sent, so that every turn painted is sent, and the rooms' line and their meshes stay one)
	if (SkyTurning.IsValid())
	{
		if (SkyTurning.IsReady() || bCut)
		{
			const double Began = FPlatformTime::Seconds();
			SkyTurning.Wait();
			SkyTurning = TFuture<void>();
			const double Waited = FPlatformTime::Seconds() - Began;
			const double Sending = SkySend(SkyTurningWhat);
			ThisFrame += Waited + Sending;
			Count(SkyTurningWhat, SkyTurningSeconds, Sending);
		}
		else
		{
			++SkyLate;
		}
	}
	if (!SkyTurning.IsValid())
	{
		AirSky::Rooms& SkyRooms = S.SkyRooms;
		const AirSky::Rooms::Sent What = SkyRooms.Wants(Seen, Drift);
		const int32 WhatNumber = static_cast<int32>(What);
		if (What != AirSky::Rooms::Sent::None && (bSkyAtOnce || bCut))
		{
			const double Began = FPlatformTime::Seconds();
			SkyRooms.SetSweep(AirSky::Vec(0.0, 0.0, 0.0));
			SkyRooms.Paint(What, S.SkyAside, Seen, Drift, Sun, Light);
			SkyFill(S, WhatNumber);
			const double Painted = FPlatformTime::Seconds() - Began;
			const double Sending = SkySend(WhatNumber);
			ThisFrame += Painted + Sending;
			Count(WhatNumber, Painted, Sending);
			++SkyAtOnce;
		}
		else if (What != AirSky::Rooms::Sent::None)
		{
			// (the state is held for the turn's length; the actor's points it writes are waited for in EndPlay and LaySky)
			SkyTurningWhat = WhatNumber;
			SkyRooms.SetSweep(AirSky::Vec(Ahead.X, Ahead.Y, Ahead.Z));
			SkyTurning = Async(EAsyncExecution::ThreadPool, [this, Keep = State, What, WhatNumber, Seen, Drift, Sun, Light]()
			{
				const double Began = FPlatformTime::Seconds();
				Keep->SkyRooms.Paint(What, Keep->SkyAside, Seen, Drift, Sun, Light);
				SkyFill(*Keep, WhatNumber);
				SkyTurningSeconds = FPlatformTime::Seconds() - Began;
			});
		}
	}
	SkySlowest = FMath::Max(SkySlowest, ThisFrame);
	// (the sky's clock, in the log every 5 s: how often each room turned, what painting a turn cost (on the pool's
	// thread, or here when turned at once) and what sending it cost this thread, the most it cost this thread in a frame,
	// how many turns were painted at once, and how many frames a turn was still painting; the render thread's share of
	// a send is not in it)
	const double Done = FPlatformTime::Seconds();
	if (SkyClockAt < 0.0)
	{
		SkyClockAt = Done;
	}
	else if (Done - SkyClockAt >= 5.0)
	{
		const int32 Turns = FMath::Max(1, SkyNearTurns + SkyBothTurns);
		UE_LOG(LogTheater, Log, TEXT("Air battle sky, %.1f s: %d near turns, %d far; painting %.2f ms a turn (%s), sending %.2f ms, the slowest %.2f ms (this thread's most in a frame); %d at once, %d frames a turn still painting; the rooms' line %.0f m."),
			Done - SkyClockAt, SkyNearTurns, SkyBothTurns, SkyPaintSeconds * 1000.0 / Turns, bSkyAtOnce ? TEXT("here") : TEXT("aside"), SkySendSeconds * 1000.0 / Turns,
			SkySlowest * 1000.0, SkyAtOnce, SkyLate, SkyLine);
		SkyNearTurns = 0;
		SkyBothTurns = 0;
		SkyAtOnce = 0;
		SkyLate = 0;
		SkyPaintSeconds = 0.0;
		SkySendSeconds = 0.0;
		SkySlowest = 0.0;
		SkyClockAt = Done;
	}
}

void AAirBattle::SkyFill(FAirBattleState& S, int32 What)
{
	// (on whichever thread painted the turn: the points into the engine's frame, the colours as they are, linear already)
	auto Fill = [this](const std::vector<AirSky::Vec>& From, const std::vector<uint8_t>& FromRgba, TArray<FVector>& Points, TArray<FColor>& Colours)
	{
		const int32 Count = FMath::Min3(Points.Num(), Colours.Num(), static_cast<int32>(FMath::Min(From.size(), FromRgba.size() / 4)));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const AirSky::Vec& P = From[static_cast<size_t>(Index)];
			Points[Index] = World(P.X, P.Y, P.Z);
			const size_t C = static_cast<size_t>(Index) * 4;
			Colours[Index] = FColor(FromRgba[C], FromRgba[C + 1], FromRgba[C + 2], FromRgba[C + 3]);
		}
	};
	AirSky::Rooms& SkyRooms = S.SkyRooms;
	if (What == static_cast<int32>(AirSky::Rooms::Sent::None))
	{
		return;
	}
	Fill(SkyRooms.NearPoints, SkyRooms.NearRgba, NearCloudPoints, NearCloudColours);
	if (bCloudShapes)
	{
		AirParts::SheetPlaces(SkyRooms.NearUvs, NearCloudPlaces, NearCloudWays, NearCloudPoints.Num());
	}
	if (What == static_cast<int32>(AirSky::Rooms::Sent::Both))
	{
		Fill(SkyRooms.FarPoints, SkyRooms.FarRgba, CloudPoints, CloudColours);
		if (bCloudShapes)
		{
			AirParts::SheetPlaces(SkyRooms.FarUvs, CloudPlaces, CloudWays, CloudPoints.Num());
		}
		Fill(SkyRooms.Shadows, SkyRooms.ShadowRgba, ShadowPoints, ShadowColours);
	}
}

double AAirBattle::SkySend(int32 What)
{
	// (this thread only, the turn painted and filled: each room's points and colours to its mesh)
	const double Began = FPlatformTime::Seconds();
	auto Send = [](UProceduralMeshComponent* Mesh, const TArray<FVector>& Points, const TArray<FColor>& Colours, const TArray<FVector2D>* Places, const TArray<FVector2D>* Ways)
	{
		if (Mesh && Points.Num() > 0)
		{
			if (Places && Ways && Places->Num() == Points.Num() && Ways->Num() == Points.Num())
			{
				Mesh->UpdateMeshSection(0, Points, TArray<FVector>(), *Places, *Ways, TArray<FVector2D>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
			}
			else
			{
				Mesh->UpdateMeshSection(0, Points, TArray<FVector>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
			}
		}
	};
	if (What != static_cast<int32>(AirSky::Rooms::Sent::None))
	{
		Send(NearClouds, NearCloudPoints, NearCloudColours, bCloudShapes ? &NearCloudPlaces : nullptr, bCloudShapes ? &NearCloudWays : nullptr);
	}
	if (What == static_cast<int32>(AirSky::Rooms::Sent::Both))
	{
		Send(Clouds, CloudPoints, CloudColours, bCloudShapes ? &CloudPlaces : nullptr, bCloudShapes ? &CloudWays : nullptr);
		Send(CloudShadows, ShadowPoints, ShadowColours, nullptr, nullptr);
	}
	// (the rooms are this thread's again: the line, for the log)
	if (State.IsValid())
	{
		SkyLine = State->SkyRooms.Line();
	}
	return FPlatformTime::Seconds() - Began;
}

void AAirBattle::SkyFinish(bool bSend)
{
	// (a turn painted aside writes this actor's points and reads the stage's ground: finished before either goes)
	if (SkyTurning.IsValid())
	{
		SkyTurning.Wait();
		SkyTurning = TFuture<void>();
		if (bSend)
		{
			SkySend(SkyTurningWhat);
		}
	}
}

FBattleEffects* AAirBattle::KitAt(const FVector& WorldAt, const FBattleEffects* InNow) const
{
	FBattleEffects* const Air = Effects ? Effects.Get() : GroundFx.Get();
	if (!GroundFx || !Clouds || !State.IsValid())
	{
		return Air;
	}
	// (under the clouds' base as drawn there, with the clouds drawn: the ground's kit, drawn under them when the eye is
	// over them; a cloud is lifted to 300 m over the ground under it)
	const FAirBattleState& S = *State;
	const double X = WorldAt.Y / UnitsPerMetre, Y = WorldAt.X / UnitsPerMetre, Z = WorldAt.Z / UnitsPerMetre;
	const double Floor = S.Show.Ground ? S.Show.Ground(X, Y) : 0.0;
	const double Base = FMath::Max(S.Sky.Base, Floor + 300.0);
	const double Margin = InNow == GroundFx.Get() ? 50.0 : InNow == Air ? -50.0 : 0.0;
	return Z < Base + Margin ? GroundFx.Get() : Air;
}

FBattleEffects* AAirBattle::GroundKit() const
{
	if (GroundFx && Clouds)
	{
		return GroundFx.Get();
	}
	return Effects ? Effects.Get() : GroundFx.Get();
}

void AAirBattle::NoteFuelBall(const FVector& At, double AcrossMetres)
{
	FFuelBallNear& Ball = BallsNear.AddDefaulted_GetRef();
	Ball.At = At;
	Ball.Reach = 2.0 * AcrossMetres * UnitsPerMetre;
	Ball.Born = State.IsValid() ? State->Show.AirNow() : 0.0;
}

double AAirBattle::IntoFuelBall(const FVector& At, double Extra) const
{
	double Most = -1e12;
	for (const FFuelBallNear& Ball : BallsNear)
	{
		Most = FMath::Max(Most, Ball.Reach + Extra - FVector::Dist(At, Ball.At));
	}
	return Most;
}

FVector AAirBattle::World(double X, double Y, double Z) const
{
	return FVector(Y * UnitsPerMetre, X * UnitsPerMetre, Z * UnitsPerMetre);
}

FVector AAirBattle::WorldVector(double X, double Y, double Z) const
{
	return FVector(Y * UnitsPerMetre, X * UnitsPerMetre, Z * UnitsPerMetre);
}

void AAirBattle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!PendingReplay.IsEmpty())
	{
		if (GetWorld() && GetWorld()->GetTimeSeconds() > 2.0)
		{
			const FString Key = PendingReplay;
			PendingReplay.Empty();
			BeginReplay(Key);
		}
		return;
	}
	KeepTheTableHidden();
	if (LandPainting.IsValid() && LandPainting.IsReady())
	{
		LandPainting.Wait();
		LandPainting = TFuture<void>();
		LayLand();
	}
	// (1.5.1: nothing moves while the opening is held behind its black frame, WarmTick)
	const bool bHeld = bBegun && State.IsValid() && State->bWarming;
	const double Real = bHeld ? 0.0 : FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, 0.1);
	RealT += Real;
	if (QuitAt >= 0.0 && RealT >= QuitAt)
	{
		QuitAt = -1.0;
		UE_LOG(LogTheater, Display, TEXT("Air battle: %d pictures taken; quitting."), ShotsTaken);
		if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			UKismetSystemLibrary::QuitGame(this, Controller, EQuitPreference::Quit, false);
		}
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (!bBegun || !State.IsValid())
	{
		return;
	}
	FAirBattleState& S = *State;
	// (1.5.1) The opening held: the frames watched while the black stands; then the black eased off over the flyover's
	// first half second (at once where no flyover runs).
	if (S.bWarming)
	{
		WarmTick(DeltaSeconds);
	}
	else if (S.Curtain > 0.0)
	{
		S.Curtain = S.bFlying ? FMath::Max(0.0, S.Curtain - FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, 0.1) / AirParts::CurtainFade) : 0.0;
	}
	// The screen's clock: x1 is real time; the run-up and the run home at x10; held while a question stands or the card is up.
	S.Clock.bQuestion = S.Question != 0 || S.bCard;
	S.Clock.bFlyover = S.bFlying;
	// (and at the player's pace while the strike is on and 5 s after, so the bombs and the dives are seen at the
	// battle's own pace: the flight core's Show::StrikeOn, the realism audit's 4.2)
	S.Clock.bStrike = S.Show.StrikeOn();
	const double Run = S.Clock.Step(Real, S.Desk.Head(), S.Board.Scale, S.ClockTag);
	const double End = S.bReplay ? S.Recording.Views.back().Head.T : 1e18;
	S.BattleT = FMath::Min(End, S.BattleT + Run);
	Feed();
	if (!S.bReplay)
	{
		S.BattleT = FMath::Min(S.BattleT, S.FedT);
	}
	// (release 1.5: never a raid with no way out) The rules failing every tick for 2 s: the raid stops where it stands and
	// the note says so; 4 s on, the scene closes as CONTINUE closes it, with nothing booked (WayOut). Wall-clock seconds,
	// so a slow failing call cannot stretch them. A raid whose end has come keeps its after card and CONTINUE.
	if (!S.bReplay && !S.bOver)
	{
		const double Now = FPlatformTime::Seconds();
		if (BrokenAt < 0.0 && TickFailures >= 3 && TickFailingSince >= 0.0 && Now - TickFailingSince > 2.0)
		{
			BrokenAt = Now;
			UE_LOG(LogTheater, Warning, TEXT("Air battle (live): TC.battleTick failed %d times in a row over %.1f s: the raid is stopped"),
				TickFailures, Now - TickFailingSince);
		}
		if (BrokenAt >= 0.0)
		{
			S.Note = "THE RAID STOPPED HERE: ITS RULES FAILED. NOTHING IS BOOKED; BACK TO THE MAP IN A MOMENT";
			S.NoteAge = -1.0; // (held, and put back each frame: no other note takes its place)
			if (Now - BrokenAt > 4.0)
			{
				BrokenAt = -1.0;
				WayOut(TEXT("its rules failed mid-raid"));
				return; // (the scene is destroyed: nothing more of this frame is run)
			}
		}
	}
	if (Eye)
	{
		const FVector At = Eye->GetActorLocation() / UnitsPerMetre;
		S.Show.Eye = AirShow::Vec(At.Y, At.X, At.Z);
	}
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_ShowAdvance);
		S.Show.Advance(S.BattleT);
	}
	if (!S.bFlyTried && !S.Show.Groups().empty())
	{
		// The flyover before the battle, once its first groups stand ("-TheaterNoFlyover" goes straight to the battle).
		S.bFlyTried = true;
		if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterNoFlyover")) && S.Fly.Begin(S.Show.Groups(), S.Board.TargetX, S.Board.TargetY, S.Board.PlaceKind == "harbour"))
		{
			S.bFlying = true;
			S.FlyT = 0.0;
			S.GlideT = -1.0;
			S.FlyBars = 1.0;
			UE_LOG(LogTheater, Display, TEXT("Air battle: the flyover, %.1f s: the raid (group %d), the target, %s"), S.Fly.Length(), S.Fly.RaidGroup(),
				S.Fly.GuardGroup() >= 0 ? TEXT("the guard") : TEXT("no guard up"));
		}
	}
	S.Desk.Reach(S.Show.Shown());
	PlayEffects();
	S.Desk.Write(Real, S.Show.Groups(), S.ClockTag, S.Words);
	if (MetRealT < 0.0 && S.Desk.Head().Contact >= 0.0)
	{
		MetRealT = RealT;
	}
	// (1.5.1: every aircraft model of the raid made under the opening's black, once its first groups stand)
	if (S.bWarming && !bWarmShapes && !S.Show.Planes().empty())
	{
		WarmShapes();
	}
	// (the eye first: a cut to a new aircraft must draw it at its near shape on the very frame of the cut)
	AimEye(Real);
	Draw(Real);
	// The effects kits' own frames in two halves (the production lead's FX2, the kit's TickBegin and TickEnd): each
	// kit's Draw on a worker while the sky is painted, begun once the frame's shells, fires and trails are given and the
	// eye is set, ended once the sky is done. What is asked of a kit between (the sky behind the smoke, WeatherAt) is
	// held and done at TickEnd, first seen the next frame, as after a Tick. "-TheaterFxOneTick": both on this thread
	// after the sky, as before.
	static const bool bFxOneTick = FParse::Param(FCommandLine::Get(), TEXT("TheaterFxOneTick"));
	const bool bFxHalves = Effects && Eye && !bFxOneTick;
	int32 ViewW = 1600;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		ViewW = FMath::Max(1, static_cast<int32>(Size.X));
	}
	FBattleJolt Jolt;
	if (bFxHalves)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_EffectsBegin);
		Jolt = Effects->TickBegin(S.Show.AirNow(), Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW);
		if (GroundFx)
		{
			const FBattleJolt Under = GroundFx->TickBegin(S.Show.AirNow(), Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW);
			Jolt.Offset += Under.Offset;
			Jolt.RollDegrees += Under.RollDegrees;
		}
	}
	DrawSky();
	if (Eye)
	{
		ZenithAt(Eye->GetActorLocation());
		HazeBandAt(Eye->GetActorLocation());
		WeatherAt(Eye->GetActorLocation());
		DeckSheetAt(Eye->GetActorLocation());
	}
	if (Effects && Eye)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_EffectsTick);
		if (bFxHalves)
		{
			Effects->TickEnd();
			if (GroundFx)
			{
				GroundFx->TickEnd();
			}
		}
		else
		{
			Jolt = Effects->Tick(S.Show.AirNow(), Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW);
			if (GroundFx)
			{
				const FBattleJolt Under = GroundFx->Tick(S.Show.AirNow(), Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW);
				Jolt.Offset += Under.Offset;
				Jolt.RollDegrees += Under.RollDegrees;
			}
		}
		if (S.bClose)
		{
			// (the braced camera's jolt, in the close view only: the overview stays still)
			Eye->SetActorLocationAndRotation(Eye->GetActorLocation() + Jolt.Offset, Eye->GetActorRotation() + FRotator(0.0, 0.0, Jolt.RollDegrees));
		}
	}
	// film critic: the camera as drawn this frame (its jolt on), its shot and the aircraft in frame
	if (FilmCritic::On() && Eye) { FilmCritic::Air(this, S, *Eye, UnitsPerMetre, DeltaSeconds, ShipsLook >= 0 ? TEXT("overview-ships") : GunsLook >= 0 ? TEXT("overview-guns") : bChutesLook ? TEXT("overview-chutes") : TEXT("overview")); }
	if (Stage)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_StageAdvance);
		Stage->Advance(S.Show.AirNow());
	}
	Play(Real);
	PlaceTags();
	if (S.NoteAge >= 0.0 && !S.Note.empty())
	{
		S.NoteAge += Real;
		if (S.NoteAge > 4.0)
		{
			S.Note.clear();
		}
	}
	// The battle's end: the over line shown, then the after card a few seconds on.
	if (S.Desk.Head().bOver && S.Show.Shown() >= S.Desk.Head().T - 1e-6)
	{
		if (!S.bOver)
		{
			S.bOver = true;
			S.OverRealT = 0.0;
			if (S.bReplay)
			{
				S.Desk.SettleFrom(S.Recording.Card); // (a set piece's end as its recorded card books it, as a live raid's)
			}
			if (!S.bReplay && Session)
			{
				FString Reply, Error;
				AirFeed::Json Ended;
				std::string Why;
				AirFeed::After After;
				// (release 1.5: an answer of null is the rules saying the battle is not over: nothing to book, as a failure;
				// "-TheaterAirRulesFailEnd" fails it on purpose, for the Mac's check of the card that says so)
				const bool bFailEnd = FParse::Param(FCommandLine::Get(), TEXT("TheaterAirRulesFailEnd"));
				if (bFailEnd)
				{
					Error = TEXT("failed on purpose (-TheaterAirRulesFailEnd)");
				}
				const bool bEnded = !bFailEnd && Session->CallBattle(TEXT("battleEnd"), TEXT("[]"), Reply, Error) && !Reply.TrimStartAndEnd().Equals(TEXT("null"));
				if (bEnded)
				{
					EndReply = Reply; // (release 1.6: for HandBack at CONTINUE, the raid booked as the rules answered it)
				}
				else
				{
					UE_LOG(LogTheater, Warning, TEXT("Air battle: TC.battleEnd failed: %s"), Error.IsEmpty() ? TEXT("it answered null") : *Error);
				}
				if (bEnded && AirFeed::Parse(AirParts::Narrow(Reply), Ended, Why) && AirFeed::ReadAfter(Ended["after"], After))
				{
					S.Card = S.Desk.CardOf(After);
					// (release 1.5, the playtester's finding 2: the bar and the music end on the side the card books, not on
					// the view's stamp, which a raid handed to the staff or an unopposed sky fight can contradict)
					S.Desk.SettleFrom(After);
					if (S.Desk.Winner() != S.Desk.Head().Winner)
					{
						UE_LOG(LogTheater, Display, TEXT("Air battle: the after card (%s) books the other side from the view's stamp (%s): the bar and the music follow the card"),
							*AirParts::Wide(After.Level), *AirParts::Wide(S.Desk.Head().Winner.empty() ? std::string("none") : S.Desk.Head().Winner));
					}
				}
				else
				{
					// (release 1.5: an end the rules failed, or answered with no card the scene can read: the after card all
					// the same, saying so, with CONTINUE on it as ever; handed back, a failed end books nothing)
					S.Card = AirDesk::Card();
					S.Card.Kicker = "THE RAID";
					S.Card.Level = bEnded ? "OVER" : "NOT BOOKED";
					S.Card.Title = bEnded ? "The raid is over" : "The raid's end could not be booked";
					S.Card.Lines.push_back(bEnded ? "What came of it is on the map." : "Its rules failed at its end, so nothing is booked.");
					if (!bEnded)
					{
						S.Card.Lines.push_back("FLY THE RAID on its card still sends your crews.");
					}
				}
			}
		}
		S.OverRealT += Real;
		if (S.OverRealT > 4.0)
		{
			S.bCard = true;
		}
	}
	TakeShots(Real);
}

void AAirBattle::Feed()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_Feed);
	FAirBattleState& S = *State;
	auto Take = [this, &S](const AirFeed::Frame& View)
	{
		S.Show.Take(View.Head.T, View.Groups, View.Events, View.Ships);
		for (const AirShow::Ship& Ship : View.Ships)
		{
			GunNations.Add(ShipGuns::OwnerOf(Ship.Id), AirParts::KitNation(Ship.Nation));
			NoteShip(ShipNames, CapitalShips, Ship);
		}
		S.Desk.Take(View);
		S.FedT = View.Head.T;
	};
	if (S.bReplay)
	{
		while (S.NextView < S.Recording.Views.size() && S.Recording.Views[S.NextView].Head.T <= S.BattleT + 1.0 + 1e-9)
		{
			Take(S.Recording.Views[S.NextView++]);
		}
		return;
	}
	// Live: the rules run a battle second ahead of what is shown, so the flight core always has the next view in hand.
	if (!Session || S.Desk.Head().bOver)
	{
		return;
	}
	const double Owed = S.BattleT + 1.0 - S.FedT;
	if (Owed <= 1e-6)
	{
		return;
	}
	// (release 1.5: a tick the rules fail, or answer with no view the scene can read, is counted; a run of them is the
	// raid's way out, in Tick. Logged at the first and every 60th, not every frame.)
	auto Failed = [this](const FString& What)
	{
		if (TickFailures++ == 0)
		{
			TickFailingSince = FPlatformTime::Seconds();
		}
		if (TickFailures == 1 || TickFailures % 60 == 0)
		{
			UE_LOG(LogTheater, Warning, TEXT("Air battle: TC.battleTick failed (%d in a row): %s"), TickFailures, *What);
		}
	};
	// (release 1.5, for the Mac's check of the way out: "-TheaterAirRulesFailAt=40" fails every tick from battle second 40)
	static const double FailAt = []
	{
		double At = -1.0;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterAirRulesFailAt="), At);
		return At;
	}();
	if (FailAt >= 0.0 && S.FedT >= FailAt)
	{
		Failed(TEXT("failed on purpose (-TheaterAirRulesFailAt)"));
		return;
	}
	FString Reply, Error;
	if (!Session->CallBattle(TEXT("battleTick"), FString::Printf(TEXT("[%.4f]"), Owed), Reply, Error))
	{
		Failed(Error);
		return;
	}
	AirFeed::Json Ticked;
	std::string Why;
	AirFeed::Frame View;
	if (!AirFeed::Parse(AirParts::Narrow(Reply), Ticked, Why) || !AirFeed::ReadView(Ticked, S.Board, View))
	{
		Failed(TEXT("its view could not be read ") + AirParts::Wide(Why));
		return;
	}
	TickFailures = 0;
	TickFailingSince = -1.0;
	if (View.Head.T > S.FedT + 1e-9)
	{
		Take(View);
	}
}

FString AAirBattle::ModelFor(const FString& Model, const FString& Nation, bool bFighter) const
{
	// The battle's model names (the unit table's, and the carrier aircraft's), as the miniature kit knows them: a
	// sculpted model of the name where Content/Models has one, else the nation's plain fighter or bomber in its paint.
	static const TMap<FString, FString> Known = {
		{ TEXT("B5N"), TEXT("Kate") }, { TEXT("D3A"), TEXT("Val") }, { TEXT("SBD"), TEXT("Dauntless") }, { TEXT("TBD"), TEXT("Devastator") },
		{ TEXT("TBF"), TEXT("Avenger") }, { TEXT("F4F"), TEXT("Wildcat") }, { TEXT("G4M"), TEXT("Betty") }, { TEXT("A6M"), TEXT("Zero") },
		{ TEXT("P40"), TEXT("Warhawk") }, { TEXT("B-17"), TEXT("B17") }, { TEXT("P-40"), TEXT("Warhawk") },
		// (the three fighters the modeller had made that the battle could not find by the names a record gives them)
		{ TEXT("F6F"), TEXT("Hellcat") }, { TEXT("Me 262"), TEXT("Me262") }, { TEXT("C.202"), TEXT("C202") }, { TEXT("MC.202"), TEXT("C202") },
		// (the land bombers: Japan's two, and Italy's, her rebuilt file in since 23 November)
		{ TEXT("G3M"), TEXT("Nell") }, { TEXT("SM.79"), TEXT("SM79") },
		// (the modeller's four of 23 November, by the marks the rules give them: the rules' own names are the files')
		{ TEXT("Pe-2"), TEXT("Pe2") }, { TEXT("LeO 451"), TEXT("LeO451") }, { TEXT("D.520"), TEXT("D520") },
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
	const FString Out = !Name.IsEmpty() && Probe.Vehicle(Name + TEXT(":") + Nation, FVector::ZeroVector, 1.0, 0.0, false)
		? Name + TEXT(":") + Nation : (bFighter ? TEXT("Fighter:") : TEXT("Bomber:")) + Nation;
	Found.Add(Asked, Out);
	return Out;
}

namespace AirParts
{
	/** An aircraft's shape, built once at its true size with its middle at its own origin; each aircraft has its own
	 * mesh of it. */
	struct FShape
	{
		TArray<FVector> Points;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FColor> Colors;
		/** Its airscrews, their blades taken out of the shape to turn as discs of their own (in the shape's frame). */
		struct FScrewAt
		{
			FVector Hub = FVector::ZeroVector;
			double Radius = 0.0;
			int32 Blades = 3;
		};
		TArray<FScrewAt> Screws;
		/** Its bomb bay (the modeller's batch of 21 November: the file's near.part and parts): each door's triangles,
		 * taken out of the near shape to turn about its hinge (Pivot, Axis, at most Most degrees), and the load racked
		 * in the bay, hidden once her stick begins to fall. In the shape's frame. */
		struct FPiece
		{
			TArray<FVector> Points, Normals;
			TArray<int32> Triangles;
			TArray<FColor> Colors;
			FVector Pivot = FVector::ZeroVector, Axis = FVector::ForwardVector;
			double Most = 85.0;
		};
		TArray<FPiece> Doors;
		FPiece Racked;
		/** Its kill slots, for an ace's victories (the file's tally.kills; AirBattleTally.cpp): read when an ace first
		 * needs them, from its file (Kind) as its points were brought in (Middle, Fit). */
		FAirTallySide Kills;
		bool bKillsRead = false;
		FString Kind;
		FVector Middle = FVector::ZeroVector;
		double Fit = 1.0;
	};

	static TMap<FString, FShape>& Shapes()
	{
		static TMap<FString, FShape> Kept;
		return Kept;
	}

	static FShape ShapeOf(const FMiniatureMesh& Made, const FVector& Middle, double Fit)
	{
		FShape Fresh;
		Fresh.Points.Reserve(Made.Vertices.Num());
		for (const FVector& Point : Made.Vertices)
		{
			Fresh.Points.Add((Point - Middle) * Fit);
		}
		Fresh.Triangles = Made.Triangles;
		Fresh.Normals = Made.Normals;
		Fresh.Colors = Made.Colors;
		return Fresh;
	}

	/**
	 * A sculpted aircraft's airscrews (Battle/AirScrew), found by the paints its file gives the points the model was made
	 * from, in order: their blades are taken out of its near shape, which turns them as discs. Read wrong (its points
	 * not in the file's order, a screw that is not one) and the blades stay where they are, standing still.
	 */
	static void TakeScrews(FShape& Shape, const FMiniatureMesh& Made, const FString& Kind, const FVector& Middle, double Fit, double U)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Models") / (Kind + TEXT(".json")))))
		{
			return;
		}
		const std::string FileText(TCHAR_TO_UTF8(*Text));
		const std::vector<int> Paints = AirScrew::NearPaints(FileText);
		const AirScrew::PaintsAre Are = AirScrew::PaintKinds(FileText);
		if (Paints.empty() || static_cast<int32>(Paints.size()) > Made.Vertices.Num() || Shape.Triangles.Num() != Made.Triangles.Num())
		{
			return;
		}
		std::vector<AirScrew::Vec3> Points;
		std::vector<bool> Blade(Paints.size(), false);
		Points.reserve(Paints.size());
		for (size_t K = 0; K < Paints.size(); ++K)
		{
			const FVector& P = Made.Vertices[static_cast<int32>(K)];
			Points.push_back({ P.X, P.Y, P.Z });
			Blade[K] = Are.IsBlade(Paints[K]);
		}
		const std::vector<int> Triangles(Made.Triangles.GetData(), Made.Triangles.GetData() + Made.Triangles.Num());
		const AirScrew::Found Found = AirScrew::Find(Points, Blade, Triangles);
		if (Found.Screws.empty())
		{
			return;
		}
		for (const AirScrew::Screw& S : Found.Screws)
		{
			const double Metres = S.Radius / U;
			if (FMath::Abs(S.Axis.X) < 0.98 || Metres < 0.9 || Metres > 2.4 || S.Blades < 2 || S.Blades > 4)
			{
				return;
			}
		}
		TArray<int32> Kept;
		Kept.Reserve(Shape.Triangles.Num());
		// (the blades go, and with them the model's own painted blur disc where it has one: the turning disc stands
		// in for both, and the blur would draw solid here)
		auto IsBlur = [&](int32 Point) { return Point >= 0 && static_cast<size_t>(Point) < Paints.size() && Are.IsBlur(Paints[static_cast<size_t>(Point)]); };
		for (int32 T = 0; T + 2 < Shape.Triangles.Num(); T += 3)
		{
			const bool bBlur = IsBlur(Made.Triangles[T]) && IsBlur(Made.Triangles[T + 1]) && IsBlur(Made.Triangles[T + 2]);
			if (!Found.BladeTriangle[static_cast<size_t>(T / 3)] && !bBlur)
			{
				Kept.Append({ Shape.Triangles[T], Shape.Triangles[T + 1], Shape.Triangles[T + 2] });
			}
		}
		Shape.Triangles = MoveTemp(Kept);
		for (const AirScrew::Screw& S : Found.Screws)
		{
			FShape::FScrewAt At;
			At.Hub = (FVector(S.Hub.X, S.Hub.Y, S.Hub.Z) - Middle) * Fit;
			At.Radius = S.Radius * Fit;
			At.Blades = S.Blades;
			Shape.Screws.Add(At);
		}
	}

	/**
	 * A bomber's bay (Battle/AirShow's doors, Brian 21 November 05:44Z "add working doors in release 1"): the parts its
	 * file names bay.port and bay.starboard (kind turn) and bombs, read point by point from near.part, are taken out of
	 * the near shape into pieces of their own; the rest stays. Read wrong (no parts, a count that does not match, not
	 * two doors) and the shape is left as it was, its bay drawn shut.
	 */
	/**
	 * A model's kill slots (its file's tally.kills, the modeller's: each [x, y, z, nx, ny, nz] in the file's axes, x
	 * forward and y to the right), brought into the shape's frame as its points were; with the paint under each (the
	 * paint with the most of her skin about it, at its own colour) and whether that paint is light; and, where the file has them, the
	 * ace's name line (tally.name) and the rudder's bars (tally.rudder). Nothing where the file has none.
	 */
	/** A list of whole numbers in a model file's near mesh ("near":{"blend":[...]}), read from the text as
	 * AirScrew::NearPaints reads its paints; empty where the near mesh has none (the far mesh's is never taken). */
	static std::vector<int> NearNumbers(const std::string& FileText, const char* Name)
	{
		std::vector<int> Out;
		const size_t Near = FileText.find("\"near\":{");
		if (Near == std::string::npos)
		{
			return Out;
		}
		const size_t Far = FileText.find("\"far\":{", Near);
		const size_t At = FileText.find(std::string("\"") + Name + "\":[", Near);
		if (At == std::string::npos || (Far != std::string::npos && At > Far))
		{
			return Out;
		}
		const size_t Open = FileText.find('[', At);
		const size_t Close = FileText.find(']', Open);
		if (Close == std::string::npos)
		{
			return Out;
		}
		const char* Read = FileText.c_str() + Open + 1;
		const char* End = FileText.c_str() + Close;
		while (Read < End)
		{
			char* Next = nullptr;
			const long Value = std::strtol(Read, &Next, 10);
			if (Next == Read)
			{
				++Read;
				continue;
			}
			Out.push_back(static_cast<int>(Value));
			Read = Next;
		}
		return Out;
	}

	static void TakeTally(FShape& Shape, const FString& Kind, const FVector& Middle, double Fit, double U)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Models") / (Kind + TEXT(".json")))))
		{
			return;
		}
		// (only the tally's own object is read: the file is megabytes of points)
		const int32 Key = Text.Find(TEXT("\"tally\""));
		const int32 Open = Key == INDEX_NONE ? INDEX_NONE : Text.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Key);
		if (Open == INDEX_NONE)
		{
			return;
		}
		int32 Depth = 0, Close = INDEX_NONE;
		for (int32 K = Open; K < Text.Len(); ++K)
		{
			const TCHAR C = Text[K];
			Depth += C == TEXT('{') ? 1 : C == TEXT('}') ? -1 : 0;
			if (Depth == 0)
			{
				Close = K;
				break;
			}
		}
		TSharedPtr<FJsonObject> Tally;
		if (Close == INDEX_NONE || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text.Mid(Open, Close - Open + 1)), Tally) || !Tally.IsValid())
		{
			return;
		}
		FAirTallySide& Side = Shape.Kills;
		Tally->TryGetStringField(TEXT("habit"), Side.Habit);
		Side.Scale = U * Fit;
		// (a slot as the file gives it, [x, y, z, nx, ny, nz]: the file has x forward and y to the right, the shape its
		// nose toward -X and its right hand at -Y; and its x, for how far aft it is)
		auto TakeSlot = [U, &Middle, Fit](const TSharedPtr<FJsonValue>& Slot, FVector& At, FVector& Out, double& X)
		{
			const TArray<TSharedPtr<FJsonValue>>* Six = nullptr;
			if (!Slot.IsValid() || !Slot->TryGetArray(Six) || Six->Num() < 6)
			{
				return false;
			}
			auto Of = [Six](int32 K) { return (*Six)[K]->AsNumber(); };
			At = (FVector(-Of(0), -Of(1), Of(2)) * U - Middle) * Fit;
			Out = FVector(-Of(3), -Of(4), Of(5)).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
			X = Of(0);
			return true;
		};
		// The paint under a point: the colour with the most of her skin about it, each of her triangles that faces the
		// same way and comes within 2 cm counting for its area, less the farther it lies. Her skin is a few large
		// triangles and her rivets and seams many small points, so the nearest point alone was a rivet's or a seam's
		// under a third of the slots (the 16 fighters of 22 November: 0 of 748 off her skin this way). Where none is
		// that close, the nearest point that faces the same way (not a stripe's edge on the other side of a seam).
		// The vote is by the file's paint of each point where the file gives them (the points the shape was made from,
		// in order), and the paint that wins is read at its own colour: a point of it with neither a mottle's soft blend
		// nor soot, the colour most such points have. So a name lying partly on a sprayed mottle (the C.202's smoke
		// rings) takes her base coat's ink, as the modeller's ACES.md and pictures do.
		const std::string FileText(TCHAR_TO_UTF8(*Text));
		const std::vector<int> FilePaints = AirScrew::NearPaints(FileText);
		const std::vector<int> FileBlends = NearNumbers(FileText, "blend");
		const std::vector<int> FileSoot = NearNumbers(FileText, "soot");
		const int32 Painted = FMath::Min(static_cast<int32>(FilePaints.size()), Shape.Colors.Num());
		TMap<int32, FColor> PlainOf;
		{
			TMap<int32, TMap<uint32, int32>> Seen;
			for (int32 P = 0; P < Painted; ++P)
			{
				const bool bBlended = P < static_cast<int32>(FileBlends.size()) && FileBlends[static_cast<size_t>(P)] > 0;
				const bool bSooted = P < static_cast<int32>(FileSoot.size()) && FileSoot[static_cast<size_t>(P)] > 0;
				if (!bBlended && !bSooted)
				{
					FColor Plain = Shape.Colors[P];
					Plain.A = 255;
					++Seen.FindOrAdd(FilePaints[static_cast<size_t>(P)]).FindOrAdd(Plain.ToPackedARGB());
				}
			}
			for (const TPair<int32, TMap<uint32, int32>>& Each : Seen)
			{
				int32 Commonest = 0;
				for (const TPair<uint32, int32>& Shade : Each.Value)
				{
					if (Shade.Value > Commonest)
					{
						Commonest = Shade.Value;
						PlainOf.Add(Each.Key, FColor(Shade.Key));
					}
				}
			}
		}
		const double SkinReach = 0.02 * U * Fit;
		auto PaintUnder = [&Shape, SkinReach, &FilePaints, Painted, &PlainOf](const FVector& At, const FVector& Out)
		{
			const int32 Known = FMath::Min3(Shape.Points.Num(), Shape.Colors.Num(), Shape.Normals.Num());
			TMap<uint32, double> Votes;
			TMap<int32, double> PaintVotes;
			for (int32 T = 0; T + 2 < Shape.Triangles.Num(); T += 3)
			{
				const int32 I0 = Shape.Triangles[T], I1 = Shape.Triangles[T + 1], I2 = Shape.Triangles[T + 2];
				if (I0 < 0 || I1 < 0 || I2 < 0 || I0 >= Known || I1 >= Known || I2 >= Known)
				{
					continue;
				}
				const FVector& A = Shape.Points[I0];
				const FVector& B = Shape.Points[I1];
				const FVector& C = Shape.Points[I2];
				// (a quick look first: within reach of the triangle's box)
				if (At.X < FMath::Min3(A.X, B.X, C.X) - SkinReach || At.X > FMath::Max3(A.X, B.X, C.X) + SkinReach
					|| At.Y < FMath::Min3(A.Y, B.Y, C.Y) - SkinReach || At.Y > FMath::Max3(A.Y, B.Y, C.Y) + SkinReach
					|| At.Z < FMath::Min3(A.Z, B.Z, C.Z) - SkinReach || At.Z > FMath::Max3(A.Z, B.Z, C.Z) + SkinReach)
				{
					continue;
				}
				// (its facing from its points' own, whichever way round its corners run)
				if (FVector::DotProduct((Shape.Normals[I0] + Shape.Normals[I1] + Shape.Normals[I2]).GetSafeNormal(), Out) < 0.5)
				{
					continue;
				}
				const double Away = FVector::Dist(FMath::ClosestPointOnTriangleToPoint(At, A, B, C), At);
				if (Away > SkinReach)
				{
					continue;
				}
				const double Weight = FVector::CrossProduct(B - A, C - A).Size() / 6.0 * FMath::Square(1.0 - Away / SkinReach);
				for (const int32 I : { I0, I1, I2 })
				{
					FColor Paint = Shape.Colors[I];
					Paint.A = 255;
					Votes.FindOrAdd(Paint.ToPackedARGB()) += Weight;
					if (I < Painted)
					{
						PaintVotes.FindOrAdd(FilePaints[static_cast<size_t>(I)]) += Weight;
					}
				}
			}
			int32 Won = INDEX_NONE;
			double WonBy = 0.0;
			for (const TPair<int32, double>& Vote : PaintVotes)
			{
				if (Vote.Value > WonBy)
				{
					WonBy = Vote.Value;
					Won = Vote.Key;
				}
			}
			if (const FColor* Plain = Won == INDEX_NONE ? nullptr : PlainOf.Find(Won))
			{
				return *Plain;
			}
			FColor Under(96, 96, 96, 255);
			double Most = 0.0;
			for (const TPair<uint32, double>& Vote : Votes)
			{
				if (Vote.Value > Most)
				{
					Most = Vote.Value;
					Under = FColor(Vote.Key);
				}
			}
			if (Most <= 0.0)
			{
				double Best = TNumericLimits<double>::Max();
				for (int32 P = 0; P < Known; ++P)
				{
					if (FVector::DotProduct(Shape.Normals[P], Out) < 0.5)
					{
						continue;
					}
					const double D = FVector::DistSquared(Shape.Points[P], At);
					if (D < Best)
					{
						Best = D;
						Under = Shape.Colors[P];
					}
				}
			}
			Under.A = 255;
			return Under;
		};
		// His name's line (the modeller's tally.name: ACES.md), and the paint under its middle for its ink.
		const TSharedPtr<FJsonObject>* NameAt = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
		if (Tally->TryGetObjectField(TEXT("name"), NameAt) && (*NameAt)->TryGetArrayField(TEXT("line"), Points) && Points->Num() >= 2)
		{
			double Front = 0.0;
			for (const TSharedPtr<FJsonValue>& Point : *Points)
			{
				FVector At, Out;
				double X = 0.0;
				if (!TakeSlot(Point, At, Out, X))
				{
					continue;
				}
				if (Side.Line.Num() == 0)
				{
					Front = X;
				}
				Side.Line.Add(At);
				Side.LineOut.Add(Out);
				Side.LineAft.Add(Front - X);
			}
			Side.Cap = (*NameAt)->HasField(TEXT("cap")) ? (*NameAt)->GetNumberField(TEXT("cap")) : 0.05;
			Side.CapMin = (*NameAt)->HasField(TEXT("cap_min")) ? (*NameAt)->GetNumberField(TEXT("cap_min")) : 0.035;
			Side.Long = (*NameAt)->HasField(TEXT("long")) ? (*NameAt)->GetNumberField(TEXT("long")) : (Side.LineAft.Num() > 0 ? Side.LineAft.Last() : 0.0);
			if (Side.Line.Num() >= 2)
			{
				const int32 Mid = Side.Line.Num() / 2;
				Side.NameSkin = PaintUnder(Side.Line[Mid], Side.LineOut[Mid]);
			}
			else
			{
				Side.Line.Reset();
				Side.LineOut.Reset();
				Side.LineAft.Reset();
			}
		}
		// The rudder's bars (Germany's habit: tally.rudder, a white bar a kill; the scene's rudders do not turn).
		const TSharedPtr<FJsonObject>* RudderAt = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* BarSlots = nullptr;
		if (Tally->TryGetObjectField(TEXT("rudder"), RudderAt) && (*RudderAt)->TryGetArrayField(TEXT("slots"), BarSlots))
		{
			Side.BarRadius = ((*RudderAt)->HasField(TEXT("radius")) ? (*RudderAt)->GetNumberField(TEXT("radius")) : 0.045) * U * Fit;
			for (const TSharedPtr<FJsonValue>& Slot : *BarSlots)
			{
				FVector At, Out;
				double X = 0.0;
				if (TakeSlot(Slot, At, Out, X))
				{
					Side.Bars.Add(At);
					Side.BarsOut.Add(Out);
					Side.BarsSkin.Add(PaintUnder(At, Out));
				}
			}
		}
		const TSharedPtr<FJsonObject>* KillsAt = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
		if (!Tally->TryGetObjectField(TEXT("kills"), KillsAt) || !(*KillsAt)->TryGetArrayField(TEXT("slots"), Slots))
		{
			return;
		}
		Side.Radius = (*KillsAt)->HasField(TEXT("radius")) ? (*KillsAt)->GetNumberField(TEXT("radius")) * U * Fit : 0.045 * U * Fit;
		if ((*KillsAt)->HasField(TEXT("per_row")))
		{
			Side.PerRow = FMath::Clamp(static_cast<int32>((*KillsAt)->GetNumberField(TEXT("per_row"))), 1, 20);
		}
		for (const TSharedPtr<FJsonValue>& Slot : *Slots)
		{
			FVector At, Out;
			double X = 0.0;
			if (TakeSlot(Slot, At, Out, X))
			{
				Side.At.Add(At);
				Side.Out.Add(Out);
			}
		}
		// (the painter's hand, where the file gives one: kills.hand, or the tally's hand.kills)
		const TArray<TSharedPtr<FJsonValue>>* Hand = nullptr;
		const TSharedPtr<FJsonObject>* Hands = nullptr;
		if (!(*KillsAt)->TryGetArrayField(TEXT("hand"), Hand) && Tally->TryGetObjectField(TEXT("hand"), Hands))
		{
			(*Hands)->TryGetArrayField(TEXT("kills"), Hand);
		}
		if (Hand)
		{
			for (const TSharedPtr<FJsonValue>& One : *Hand)
			{
				const TArray<TSharedPtr<FJsonValue>>* Four = nullptr;
				if (One.IsValid() && One->TryGetArray(Four) && Four->Num() >= 4)
				{
					Side.Hand.Add(FVector4((*Four)[0]->AsNumber(), (*Four)[1]->AsNumber(), (*Four)[2]->AsNumber() * U * Fit, (*Four)[3]->AsNumber() * U * Fit));
				}
			}
		}
		// The paint under each slot, and the panel's lightness from them all (the sheet's light set at L* 50 or more).
		double Sum[3] = { 0.0, 0.0, 0.0 };
		for (int32 K = 0; K < Side.At.Num(); ++K)
		{
			const FColor Under = PaintUnder(Side.At[K], Side.Out[K]);
			Side.Skin.Add(Under);
			const FLinearColor Lin(Under);
			Sum[0] += Lin.R;
			Sum[1] += Lin.G;
			Sum[2] += Lin.B;
		}
		if (Side.At.Num() > 0)
		{
			const double Y = (0.2126 * Sum[0] + 0.7152 * Sum[1] + 0.0722 * Sum[2]) / Side.At.Num();
			const double L = Y > 216.0 / 24389.0 ? 116.0 * FMath::Pow(Y, 1.0 / 3.0) - 16.0 : 24389.0 / 27.0 * Y;
			Side.bLight = L >= 50.0;
		}
	}

	static void TakeBay(FShape& Shape, const FMiniatureMesh& Made, const FString& Kind, const FVector& Middle, double Fit, double U)
	{
		FString Text;
		TSharedPtr<FJsonObject> File;
		const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
		const TSharedPtr<FJsonObject>* Near = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* PartOf = nullptr;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Models") / (Kind + TEXT(".json"))))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), File) || !File.IsValid()
			|| !File->TryGetArrayField(TEXT("parts"), Parts) || !File->TryGetObjectField(TEXT("near"), Near) || !(*Near)->TryGetArrayField(TEXT("part"), PartOf)
			|| PartOf->Num() == 0 || PartOf->Num() > Made.Vertices.Num() || Shape.Points.Num() != Made.Vertices.Num())
		{
			return;
		}
		// (the file has x forward and y to the right; the shape the nose toward -X, the right hand at -Y, a metre U, about Middle, Fit)
		auto Place = [&](const TArray<TSharedPtr<FJsonValue>>& Xyz) { return (FVector(-Xyz[0]->AsNumber(), -Xyz[1]->AsNumber(), Xyz[2]->AsNumber()) * U - Middle) * Fit; };
		TMap<int32, int32> PieceOfPart; // the file's part: 0 and 1 the doors, 2 the racked load
		TArray<FShape::FPiece> Pieces;
		Pieces.SetNum(3);
		int32 DoorsFound = 0;
		for (int32 K = 0; K < Parts->Num(); ++K)
		{
			const TSharedPtr<FJsonObject> Part = (*Parts)[K]->AsObject();
			FString Name, PartKind;
			if (!Part.IsValid() || !Part->TryGetStringField(TEXT("name"), Name))
			{
				continue;
			}
			Part->TryGetStringField(TEXT("kind"), PartKind);
			const TArray<TSharedPtr<FJsonValue>> *Pivot = nullptr, *Axis = nullptr, *Limits = nullptr;
			if ((Name == TEXT("bay.port") || Name == TEXT("bay.starboard")) && PartKind == TEXT("turn") && DoorsFound < 2
				&& Part->TryGetArrayField(TEXT("pivot"), Pivot) && Pivot->Num() == 3 && Part->TryGetArrayField(TEXT("axis"), Axis) && Axis->Num() == 3)
			{
				FShape::FPiece& Door = Pieces[DoorsFound];
				Door.Pivot = Place(*Pivot);
				Door.Axis = FVector(-(*Axis)[0]->AsNumber(), -(*Axis)[1]->AsNumber(), (*Axis)[2]->AsNumber()).GetSafeNormal();
				if (Part->TryGetArrayField(TEXT("limits"), Limits) && Limits->Num() == 2)
				{
					Door.Most = FMath::Clamp((*Limits)[1]->AsNumber(), 0.0, 120.0);
				}
				PieceOfPart.Add(K, DoorsFound++);
			}
			else if (Name == TEXT("bombs"))
			{
				PieceOfPart.Add(K, 2);
			}
		}
		if (DoorsFound != 2)
		{
			return;
		}
		TArray<int32> Part;
		Part.Reserve(PartOf->Num());
		for (const TSharedPtr<FJsonValue>& Value : *PartOf)
		{
			Part.Add(static_cast<int32>(Value->AsNumber()));
		}
		TArray<TMap<int32, int32>> Remap;
		Remap.SetNum(3);
		TArray<int32> Kept;
		Kept.Reserve(Shape.Triangles.Num());
		for (int32 T = 0; T + 2 < Shape.Triangles.Num(); T += 3)
		{
			const int32 A = Shape.Triangles[T];
			const int32 First = A < Part.Num() ? Part[A] : 0;
			const int32* Piece = PieceOfPart.Find(First);
			bool bOne = Piece != nullptr;
			for (int32 C = 1; C < 3 && bOne; ++C)
			{
				const int32 Corner = Shape.Triangles[T + C];
				bOne = (Corner < Part.Num() ? Part[Corner] : 0) == First;
			}
			if (!bOne)
			{
				Kept.Append({ Shape.Triangles[T], Shape.Triangles[T + 1], Shape.Triangles[T + 2] });
				continue;
			}
			FShape::FPiece& Into = Pieces[*Piece];
			for (int32 C = 0; C < 3; ++C)
			{
				const int32 Corner = Shape.Triangles[T + C];
				int32* Was = Remap[*Piece].Find(Corner);
				if (!Was)
				{
					Was = &Remap[*Piece].Add(Corner, Into.Points.Num());
					Into.Points.Add(Shape.Points[Corner]);
					Into.Normals.Add(Shape.Normals.IsValidIndex(Corner) ? Shape.Normals[Corner] : FVector::UpVector);
					Into.Colors.Add(Shape.Colors.IsValidIndex(Corner) ? Shape.Colors[Corner] : FColor::White);
				}
				Into.Triangles.Add(*Was);
			}
		}
		if (Pieces[0].Triangles.Num() == 0 || Pieces[1].Triangles.Num() == 0)
		{
			return;
		}
		Shape.Triangles = MoveTemp(Kept);
		Shape.Doors = { MoveTemp(Pieces[0]), MoveTemp(Pieces[1]) };
		Shape.Racked = MoveTemp(Pieces[2]);
	}

	/** A bay door turned Degrees about its hinge (plus opens): its points and facings, for the mesh's section. */
	static void TurnDoor(const FShape::FPiece& Door, double Degrees, TArray<FVector>& Points, TArray<FVector>& Normals)
	{
		const FQuat Turn(Door.Axis, FMath::DegreesToRadians(Degrees));
		Points.SetNum(Door.Points.Num());
		Normals.SetNum(Door.Normals.Num());
		for (int32 K = 0; K < Door.Points.Num(); ++K)
		{
			Points[K] = Door.Pivot + Turn.RotateVector(Door.Points[K] - Door.Pivot);
		}
		for (int32 K = 0; K < Door.Normals.Num(); ++K)
		{
			Normals[K] = Turn.RotateVector(Door.Normals[K]);
		}
	}

	/**
	 * An airscrew turning, as a film's camera sees it: a see-through dark disc in its own Y-Z plane (the shaft along X),
	 * clear at the boss and soft at the tips, darker where each blade sweeps, so that it is seen to turn. Both faces.
	 */
	static void DiscOf(double Radius, int32 Blades, TArray<FVector>& Points, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FColor>& Colours)
	{
		constexpr int32 Round = 48;
		const double Rings[4] = { 0.12, 0.5, 0.9, 1.0 };
		for (int32 Ring = 0; Ring < 4; ++Ring)
		{
			for (int32 K = 0; K < Round; ++K)
			{
				const double A = UE_DOUBLE_TWO_PI * K / Round;
				// (the sweep of each blade: a dark smear behind it, fading round to the next)
				const double Swept = FMath::Pow(0.5 + 0.5 * FMath::Cos(Blades * A), 5.0);
				const double Alpha = Ring == 0 || Ring == 3 ? 0.0 : (Ring == 1 ? 46.0 + 80.0 * Swept : 34.0 + 64.0 * Swept);
				Points.Add(FVector(0.0, FMath::Sin(A), FMath::Cos(A)) * Radius * Rings[Ring]);
				Normals.Add(FVector(1.0, 0.0, 0.0));
				Colours.Add(FColor(30, 30, 32, static_cast<uint8>(FMath::Clamp(Alpha, 0.0, 255.0))));
			}
		}
		for (int32 Ring = 0; Ring < 3; ++Ring)
		{
			for (int32 K = 0; K < Round; ++K)
			{
				const int32 A = Ring * Round + K, B = Ring * Round + (K + 1) % Round, C = A + Round, D = B + Round;
				Triangles.Append({ A, B, D, A, D, C });
				Triangles.Append({ A, D, B, A, C, D });
			}
		}
	}

	/**
	 * An airscrew stopped, its blades feathered (turned edge on to the wind, so the dead engine is not dragged round):
	 * each blade a strip from the boss to the tip, its chord along the shaft, with a thin face across it so it reads from
	 * ahead as well as from the side. In the disc's frame (the shaft along X); both faces; near black.
	 */
	static void BladesOf(double Radius, int32 Blades, TArray<FVector>& Points, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FColor>& Colours)
	{
		const double Steps[4] = { 0.12, 0.45, 0.8, 1.0 };
		const double Chords[4] = { 0.15, 0.16, 0.12, 0.07 };
		for (int32 Blade = 0; Blade < Blades; ++Blade)
		{
			const double A = UE_DOUBLE_TWO_PI * Blade / FMath::Max(1, Blades);
			const FVector Along(0.0, FMath::Sin(A), FMath::Cos(A));
			const FVector Across(0.0, FMath::Cos(A), -FMath::Sin(A));
			// (the chord along the shaft, then the thin face in the disc's plane)
			for (int32 Face = 0; Face < 2; ++Face)
			{
				const FVector Wide = Face == 0 ? FVector(1.0, 0.0, 0.0) : Across * 0.3;
				const FVector Facing = Face == 0 ? Across : FVector(1.0, 0.0, 0.0);
				const int32 First = Points.Num();
				for (int32 K = 0; K < 4; ++K)
				{
					for (int32 Side = 0; Side < 2; ++Side)
					{
						Points.Add(Along * Radius * Steps[K] + Wide * Radius * Chords[K] * (Side == 0 ? -0.5 : 0.5));
						Normals.Add(Facing);
						Colours.Add(FColor(38, 38, 40, 255));
					}
				}
				for (int32 K = 0; K < 3; ++K)
				{
					const int32 P0 = First + K * 2, P1 = P0 + 1, P2 = P0 + 2, P3 = P0 + 3;
					Triangles.Append({ P0, P1, P3, P0, P3, P2 });
					Triangles.Append({ P0, P3, P1, P0, P2, P3 });
				}
			}
		}
	}

	/**
	 * A stand-in's coat in the paint its navy flew (the art director's note on the first Mac run: three orange-yellow
	 * aircraft over Oahu, whose side could not be told). The plain machines wear their country's colour on the map; the
	 * carrier aircraft of 1941-42 wore the Japanese navy's grey, or the American navy's blue-grey. Every point whose
	 * colour is the coat's, or the coat's shaded, is painted the same shade of the new one; marks, glass, metal and the
	 * undersides keep theirs.
	 */
	static void Recoat(TArray<FColor>& Colors, const FColor& From, const FColor& To)
	{
		for (FColor& C : Colors)
		{
			const double R = C.R / FMath::Max(1.0, static_cast<double>(From.R));
			const double G = C.G / FMath::Max(1.0, static_cast<double>(From.G));
			const double B = C.B / FMath::Max(1.0, static_cast<double>(From.B));
			const double Mean = (R + G + B) / 3.0;
			if (Mean < 0.4 || Mean > 1.3 || FMath::Abs(R - Mean) > 0.07 || FMath::Abs(G - Mean) > 0.07 || FMath::Abs(B - Mean) > 0.07)
			{
				continue;
			}
			auto Shade = [Mean](uint8 Channel) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Channel * Mean), 0, 255)); };
			C = FColor(Shade(To.R), Shade(To.G), Shade(To.B), C.A);
		}
	}

	/** The navy paint a stand-in wears, by its nation and model; false if it keeps its own. */
	static bool NavyCoat(const FString& Nation, const FString& Model, FColor& Out)
	{
		if (Nation == TEXT("JAP") || Nation == TEXT("JPN"))
		{
			Out = FColor::FromHex(TEXT("A9B19F")); // (the Japanese navy's grey, as the battle models' Zero, Kate and Val)
			return true;
		}
		// (the recordings name them either way: "F4F" or "Wildcat", "SBD" or "Dauntless")
		static const TSet<FString> Carried = { TEXT("F4F"), TEXT("Wildcat"), TEXT("SBD"), TEXT("Dauntless"), TEXT("TBD"), TEXT("Devastator"), TEXT("TBF"), TEXT("Avenger") };
		if (Nation == TEXT("USA") && Carried.Contains(Model))
		{
			Out = FColor::FromHex(TEXT("566A78")); // (the American navy's blue-grey, as the battle models' Dauntless)
			return true;
		}
		return false;
	}

	/** A colour as AirLight's paint number, and back. */
	static uint32_t ToneOf(const FColor& C)
	{
		return (static_cast<uint32_t>(C.R) << 24) | (static_cast<uint32_t>(C.G) << 16) | (static_cast<uint32_t>(C.B) << 8) | C.A;
	}
	static FColor ColourOf(uint32_t Tone)
	{
		return FColor(static_cast<uint8>(Tone >> 24), static_cast<uint8>(Tone >> 16), static_cast<uint8>(Tone >> 8), static_cast<uint8>(Tone));
	}

	/**
	 * An aircraft's light shape (AS1; Battle/AirLight), made from its far shape the first time it is asked for and kept
	 * beside it ("<shape>|light"): null where its far shape is light enough as it is (AirLight::LightMost), or it has none.
	 */
	static const FShape* LightOf(const FString& ShapeKey)
	{
		TMap<FString, FShape>& Kept = Shapes();
		const FString Key = ShapeKey + TEXT("|light");
		if (const FShape* Have = Kept.Find(Key))
		{
			return Have->Triangles.Num() > 0 ? Have : nullptr;
		}
		FShape Light;
		if (const FShape* Far = Kept.Find(ShapeKey + TEXT("|far")))
		{
			const int32 Most = AirLight::LightMost(Far->Triangles.Num() / 3);
			if (Most > 0 && Far->Colors.Num() == Far->Points.Num())
			{
				AirLight::Shape In;
				In.Points.reserve(static_cast<size_t>(Far->Points.Num()) * 3);
				for (const FVector& Point : Far->Points)
				{
					In.Points.insert(In.Points.end(), { Point.X, Point.Y, Point.Z });
				}
				In.Triangles.assign(Far->Triangles.GetData(), Far->Triangles.GetData() + Far->Triangles.Num());
				In.Tones.reserve(static_cast<size_t>(Far->Colors.Num()));
				for (const FColor& Colour : Far->Colors)
				{
					In.Tones.push_back(ToneOf(Colour));
				}
				const double Began = FPlatformTime::Seconds();
				const AirLight::Made Made = AirLight::Lighten(In, Most);
				// (one it could not take down to what was asked is one whose far shape it cannot read, and it comes out
				// torn (the rebuilt SM.79's of 23 November, 1,524 of 1,500, her fuselage gone to spikes): none is made,
				// and her copies past LightFromM keep her far shape)
				const int32 Count = Made.Kept > Most ? 0 : static_cast<int32>(Made.Tones.size());
				if (Count == 0)
				{
					UE_LOG(LogTheater, Warning, TEXT("Air battle: %s's light shape left unmade (%d triangles kept of %d asked): its copies past %.0f m keep its far shape."),
						*ShapeKey, Made.Kept, Most, AirLight::LightFromM);
				}
				Light.Points.Reserve(Count);
				Light.Normals.Reserve(Count);
				Light.Colors.Reserve(Count);
				for (int32 K = 0; K < Count; ++K)
				{
					const size_t I = static_cast<size_t>(K) * 3;
					Light.Points.Add(FVector(Made.Points[I], Made.Points[I + 1], Made.Points[I + 2]));
					Light.Normals.Add(FVector(Made.Normals[I], Made.Normals[I + 1], Made.Normals[I + 2]));
					Light.Colors.Add(ColourOf(Made.Tones[static_cast<size_t>(K)]));
				}
				if (Count > 0)
				{
					Light.Triangles.Append(Made.Triangles.data(), static_cast<int32>(Made.Triangles.size()));
					UE_LOG(LogTheater, Display, TEXT("Air battle: %s's light shape for its copies past %.0f m, %d triangles from its far shape's %d (%d asked), in %.1f ms."),
						*ShapeKey, AirLight::LightFromM, Made.Kept, Far->Triangles.Num() / 3, Most, (FPlatformTime::Seconds() - Began) * 1000.0);
				}
			}
		}
		// (kept empty where there is none, so that it is not looked for again)
		Kept.Add(Key, MoveTemp(Light));
		const FShape* Laid = Kept.Find(Key);
		return Laid && Laid->Triangles.Num() > 0 ? Laid : nullptr;
	}

	/**
	 * A shape as a mesh the engine keeps (AS1: the aircraft's copies): its points, facings and colours (sRGB bytes, as
	 * the procedural meshes carry them; the material makes them linear), all on Ink. Null for a shape with no triangles.
	 * Built as the ground battle builds its crowds' (LandVat), fast, with no collision.
	 */
	static UStaticMesh* StillOf(UObject* Owner, const FShape& Shape, UMaterialInterface* Ink)
	{
		const int32 Count = Shape.Points.Num();
		if (!Owner || !Ink || Count == 0 || Shape.Triangles.Num() < 3 || Shape.Colors.Num() != Count)
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
		D.ReserveNewTriangles(Shape.Triangles.Num() / 3);
		TArray<FVertexInstanceID> Corners;
		Corners.SetNum(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const FVertexID Vertex = D.CreateVertex();
			Positions[Vertex] = FVector3f(Shape.Points[I]);
			const FVertexInstanceID Corner = D.CreateVertexInstance(Vertex);
			const FVector3f Normal = Shape.Normals.IsValidIndex(I) ? FVector3f(Shape.Normals[I]).GetSafeNormal(1e-6f, FVector3f::UpVector) : FVector3f::UpVector;
			Normals[Corner] = Normal;
			// (no normal map: any tangent across the facing will do)
			const FVector3f Across = FMath::Abs(Normal.Z) < 0.9f ? FVector3f::UpVector : FVector3f::ForwardVector;
			Tangents[Corner] = FVector3f::CrossProduct(Across, Normal).GetSafeNormal(1e-6f, FVector3f::ForwardVector);
			Signs[Corner] = 1.0f;
			Colours[Corner] = FVector4f(FLinearColor(Shape.Colors[I]));
			Uvs.Set(Corner, 0, FVector2f::ZeroVector);
			Corners[I] = Corner;
		}
		int32 Laid = 0;
		for (int32 T = 0; T + 2 < Shape.Triangles.Num(); T += 3)
		{
			const int32 A = Shape.Triangles[T], B = Shape.Triangles[T + 1], C = Shape.Triangles[T + 2];
			if (!Corners.IsValidIndex(A) || !Corners.IsValidIndex(B) || !Corners.IsValidIndex(C) || A == B || B == C || A == C)
			{
				continue;
			}
			const FVertexInstanceID Three[3] = { Corners[A], Corners[B], Corners[C] };
			D.CreateTriangle(Group, TArrayView<const FVertexInstanceID>(Three, 3));
			++Laid;
		}
		if (Laid == 0)
		{
			return nullptr;
		}
		UStaticMesh* Made = NewObject<UStaticMesh>(Owner, MakeUniqueObjectName(Owner, UStaticMesh::StaticClass(), FName(TEXT("AirPlaneStill"))));
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
}

FString AAirBattle::ShapeFor(const FString& Model, const FString& Nation, bool bFighter, double Span, bool bTorpedo)
{
	// A sculpted model has two shapes: its far one (a few thousand points) for every aircraft, and its near one (tens
	// of thousands) made only for those that come near the eye (PlaneNear): two hundred aircraft at the near shape were
	// millions of points. A stand-in is sized to the battle's span, so each span is a shape of its own (a Kate is not a
	// Val's size), and wears its navy's paint.
	const FString Name = ModelFor(Model, Nation, bFighter);
	const bool bStandIn = Name.StartsWith(TEXT("Fighter:")) || Name.StartsWith(TEXT("Bomber:"));
	// (a model with paint by date is a shape of its own in each of its schemes: a battle of 1944 does not take the shape
	// painted for one of 1943)
	const int32 Scheme = FMiniatureMesh::SchemeOf(Name);
	// (and one of a torpedo group a shape of its own: a model with both loads in its file carries its torpedo there,
	// its bombs anywhere else; FMiniatureMesh::bTorpedoLoad)
	const FString ShapeKey = (bStandIn ? FString::Printf(TEXT("%s@%.1f|%s"), *Name, Span, *Model) : Name) + (Scheme > 0 ? FString::Printf(TEXT("|scheme%d"), Scheme) : FString())
		+ (bTorpedo ? TEXT("|torpedo") : TEXT(""));
	TMap<FString, AirParts::FShape>& Shapes = AirParts::Shapes();
	if (!Shapes.Contains(ShapeKey))
	{
		FMiniatureMesh::bTorpedoLoad = bTorpedo;
		FMiniatureMesh Made;
		Made.Vehicle(Name, FVector::ZeroVector, UnitsPerMetre, 0.0, true);
		AirParts::FShape Near;
		if (Made.Vertices.Num() > 0)
		{
			// (to the battle's span: a stand-in's own size is the plain machine's, not this one's)
			const FBox Box(Made.Vertices);
			const FVector Middle = Box.GetCenter();
			const double Across = FMath::Max(1.0, Box.GetSize().Y);
			const double Fit = bStandIn ? Span * UnitsPerMetre / Across : 1.0;
			Near = AirParts::ShapeOf(Made, Middle, Fit);
			FString Kind, Whose;
			Name.Split(TEXT(":"), &Kind, &Whose);
			if (DiscInk)
			{
				AirParts::TakeScrews(Near, Made, Kind, Middle, Fit, UnitsPerMetre);
			}
			FColor Navy;
			const bool bNavy = bStandIn && AirParts::NavyCoat(Nation, Model, Navy);
			if (bNavy)
			{
				AirParts::Recoat(Near.Colors, FMiniatureMesh::VehicleColour(Nation).ToFColor(true), Navy);
			}
			// (its bay's doors and racked load, lifted out to turn and to go: after its coat, so that they wear it)
			AirParts::TakeBay(Near, Made, Kind, Middle, Fit, UnitsPerMetre);
			// (its kill slots are read from its file when an ace first needs them, as its points were brought in)
			Near.Kind = Kind;
			Near.Middle = Middle;
			Near.Fit = Fit;
			// (the stand-ins too: the plain Fighter and Bomber models have far parts, and they are half of each battle)
			FMiniatureMesh Far;
			if (Far.Sculpted(Kind, Whose, FVector::ZeroVector, UnitsPerMetre, 0.0, true) && Far.Vertices.Num() > 0 && Far.Vertices.Num() < Made.Vertices.Num())
			{
				// (about the same middle and the same size as the near shape's, so that the change from one to the
				// other does not jump)
				AirParts::FShape Away = AirParts::ShapeOf(Far, Middle, Fit);
				if (bNavy)
				{
					AirParts::Recoat(Away.Colors, FMiniatureMesh::VehicleColour(Nation).ToFColor(true), Navy);
				}
				Shapes.Add(ShapeKey + TEXT("|far"), MoveTemp(Away));
			}
		}
		FMiniatureMesh::bTorpedoLoad = false;
		Shapes.Add(ShapeKey, MoveTemp(Near));
	}
	return ShapeKey;
}

UProceduralMeshComponent* AAirBattle::PlaneMesh(int32 Key, const FString& Model, const FString& Nation, bool bFighter, double Span, bool bTorpedo)
{
	TArray<FPlaneMade::FMark> Struck; // (an aircraft made again keeps the strikes it took, laid again on its new skin)
	if (TObjectPtr<UProceduralMeshComponent>* Found = Planes.Find(Key))
	{
		const FPlaneMade* Was = PlaneMade.Find(Key);
		if (*Found && Was && Was->Model == Model && Was->Span == Span)
		{
			return Found->Get();
		}
		// (made before a view named its model, the place's main one standing in: made again as its own)
		if (*Found)
		{
			(*Found)->DestroyComponent();
		}
		if (FPlaneMade* Old = PlaneMade.Find(Key))
		{
			for (const TWeakObjectPtr<UProceduralMeshComponent>& Disc : Old->Discs)
			{
				if (Disc.IsValid())
				{
					Disc->DestroyComponent();
				}
			}
			LiftMarks(*Old);
			if (Old->Tally.IsValid())
			{
				Old->Tally->DestroyComponent();
			}
			Struck = MoveTemp(Old->Marks);
			for (FPlaneMade::FMark& Mark : Struck)
			{
				Mark.bOnSkin = false;
			}
		}
		Planes.Remove(Key);
		PlaneMade.Remove(Key);
	}
	const FString ShapeKey = ShapeFor(Model, Nation, bFighter, Span, bTorpedo);
	TMap<FString, AirParts::FShape>& Shapes = AirParts::Shapes();
	// (her bay's racked load, for where her stick's bombs leave her: the flight core's BaysOf, in metres forward, to the
	// right and up from the point she is drawn about; the shape has her nose toward -X and her right hand at -Y)
	const AirParts::FShape* Bayed = Shapes.Find(ShapeKey);
	if (State && Bayed && Bayed->Doors.Num() == 2 && Bayed->Racked.Points.Num() > 0)
	{
		const FBox Load(Bayed->Racked.Points);
		AirShow::Show::BayPlace Bay;
		Bay.Fore = -Load.Min.X / UnitsPerMetre;
		Bay.Aft = -Load.Max.X / UnitsPerMetre;
		Bay.Up = Load.GetCenter().Z / UnitsPerMetre;
		Bay.Half = FMath::Clamp(Load.GetExtent().Y / UnitsPerMetre * 0.5, 0.1, 0.4);
		State->Show.BaysOf[AirParts::Narrow(Model)] = Bay;
	}
	const AirParts::FShape* Far = Shapes.Find(ShapeKey + TEXT("|far"));
	const AirParts::FShape* First = Far ? Far : Shapes.Find(ShapeKey);
	// (a name of its own: one made again for its own model must not take the name of the one just taken down)
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("AirPlane_%d"), Key)));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	// (lit by the light thrown up from under the eye, UnderLight, as well as the day's)
	Mesh->LightingChannels.bChannel1 = true;
	Mesh->bUseAsyncCooking = false;
	Mesh->RegisterComponent();
	if (First && First->Points.Num() > 0)
	{
		Mesh->CreateMeshSection(0, First->Points, First->Triangles, First->Normals, TArray<FVector2D>(), First->Colors, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(0, PlaneInk);
	}
	Planes.Add(Key, Mesh);
	FPlaneMade Made;
	Made.Model = Model;
	Made.Span = Span;
	Made.Shape = ShapeKey;
	Made.bTwoShapes = Far != nullptr;
	Made.Phase = static_cast<double>(GetTypeHash(Key) % 360u);
	Made.Marks = MoveTemp(Struck);
	PlaneMade.Add(Key, Made);
	return Mesh;
}

void AAirBattle::PlaneNear(int32 Key, UProceduralMeshComponent* Mesh, bool bNear)
{
	FPlaneMade* Made = PlaneMade.Find(Key);
	if (!Mesh || !Made || !Made->bTwoShapes || Made->bNear == bNear)
	{
		return;
	}
	if (bNear && !Made->bNearMade)
	{
		const AirParts::FShape* Near = AirParts::Shapes().Find(Made->Shape);
		if (!Near || Near->Points.Num() == 0)
		{
			return;
		}
		Mesh->CreateMeshSection(1, Near->Points, Near->Triangles, Near->Normals, TArray<FVector2D>(), Near->Colors, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(1, PlaneInk);
		Made->bNearMade = true;
		// (its bay: the two doors, sections 2 and 3, turned by Draw as the flight core opens them; the racked load, 4)
		for (int32 K = 0; K < Near->Doors.Num(); ++K)
		{
			const AirParts::FShape::FPiece& Door = Near->Doors[K];
			Mesh->CreateMeshSection(2 + K, Door.Points, Door.Triangles, Door.Normals, TArray<FVector2D>(), Door.Colors, TArray<FProcMeshTangent>(), false);
			Mesh->SetMaterial(2 + K, PlaneInk);
		}
		if (Near->Doors.Num() == 2 && Near->Racked.Triangles.Num() > 0)
		{
			Mesh->CreateMeshSection(4, Near->Racked.Points, Near->Racked.Triangles, Near->Racked.Normals, TArray<FVector2D>(), Near->Racked.Colors, TArray<FProcMeshTangent>(), false);
			Mesh->SetMaterial(4, PlaneInk);
		}
		Made->bBay = Near->Doors.Num() == 2;
		Made->BayShown = 0.0;
		// (its airscrews, each a disc of its own on the aircraft at its hub, turned about its shaft as it flies)
		for (const AirParts::FShape::FScrewAt& Screw : Near->Screws)
		{
			UProceduralMeshComponent* Disc = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("AirScrew_%d"), Key)));
			Disc->SetupAttachment(Mesh);
			Disc->SetMobility(EComponentMobility::Movable);
			Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Disc->SetCastShadow(false);
			Disc->bUseAsyncCooking = false;
			Disc->RegisterComponent();
			Disc->SetRelativeLocation(Screw.Hub);
			TArray<FVector> Points, Normals;
			TArray<int32> Triangles;
			TArray<FColor> Colours;
			AirParts::DiscOf(Screw.Radius, Screw.Blades, Points, Triangles, Normals, Colours);
			Disc->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
			Disc->SetMaterial(0, DiscInk);
			// (and its blades standing still, shown in place of the turning disc once its engine is dead and feathered)
			Points.Reset();
			Normals.Reset();
			Triangles.Reset();
			Colours.Reset();
			AirParts::BladesOf(Screw.Radius, Screw.Blades, Points, Triangles, Normals, Colours);
			Disc->CreateMeshSection(1, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
			Disc->SetMaterial(1, PlainInk);
			Disc->SetMeshSectionVisible(1, false);
			Made->Discs.Add(Disc);
		}
	}
	Mesh->SetMeshSectionVisible(0, !bNear);
	Mesh->SetMeshSectionVisible(1, bNear);
	if (Made->bBay)
	{
		Mesh->SetMeshSectionVisible(2, bNear);
		Mesh->SetMeshSectionVisible(3, bNear);
		if (Mesh->GetNumSections() > 4)
		{
			Mesh->SetMeshSectionVisible(4, bNear && !Made->bBombsGone);
		}
	}
	Made->bNear = bNear;
	ShowDiscs(*Made, bNear);
}

void AAirBattle::TakeDiscEngines(FPlaneMade& Made, const AirShow::Shape& Shape)
{
	// Each disc's engine (from 1, as the model's engine.N the flight core names its dead by): the engine whose place
	// across the span is nearest the disc's hub's (engine.1 the port outer; the shape's frame has its right hand toward
	// -Y). A single engine is engine 1.
	const double SpanUnits = FMath::Max(1.0, Made.Span * UnitsPerMetre);
	Made.DiscEngine.Init(1, Made.Discs.Num());
	Made.DiscStill.Init(false, Made.Discs.Num());
	for (int32 K = 0; K < Made.Discs.Num(); ++K)
	{
		const UProceduralMeshComponent* Disc = Made.Discs[K].Get();
		if (!Disc || Shape.Engines.size() < 2)
		{
			continue;
		}
		const double Across = -Disc->GetRelativeLocation().Y / SpanUnits;
		double Nearest = TNumericLimits<double>::Max();
		for (size_t N = 0; N < Shape.Engines.size(); ++N)
		{
			const double Off = FMath::Abs(Shape.Engines[N].Y / FMath::Max(1.0, Shape.Span) - Across);
			if (Off < Nearest)
			{
				Nearest = Off;
				Made.DiscEngine[K] = static_cast<int32>(N) + 1;
			}
		}
	}
}

void AAirBattle::ShowDiscs(FPlaneMade& Made, bool bShow)
{
	for (const TWeakObjectPtr<UProceduralMeshComponent>& Disc : Made.Discs)
	{
		if (Disc.IsValid() && Disc->IsVisible() != bShow)
		{
			Disc->SetVisibility(bShow);
		}
	}
}

const FAirTallySide* AAirBattle::TallyOfShape(const FString& Shape)
{
	AirParts::FShape* Found = AirParts::Shapes().Find(Shape);
	if (Found && !Found->bKillsRead)
	{
		Found->bKillsRead = true;
		// (a stand-in sized to the battle's span has its slots sized with it)
		if (!Found->Kind.IsEmpty())
		{
			AirParts::TakeTally(*Found, Found->Kind, Found->Middle, Found->Fit, UnitsPerMetre);
		}
	}
	return Found ? &Found->Kills : nullptr;
}

UStaticMesh* AAirBattle::PlaneStill(const FString& Shape, bool bLight)
{
	const FString Key = Shape + (bLight ? TEXT("|light") : TEXT("|far"));
	if (const TObjectPtr<UStaticMesh>* Have = PlaneStills.Find(Key))
	{
		return Have->Get();
	}
	const AirParts::FShape* From = bLight ? AirParts::LightOf(Shape) : AirParts::Shapes().Find(Key);
	UStaticMesh* Made = From ? AirParts::StillOf(this, *From, PlaneInk) : nullptr;
	// (null kept too: a shape with no mesh is drawn on its own mesh, and not tried again)
	PlaneStills.Add(Key, Made);
	return Made;
}

AAirBattle::FPlaneCopies* AAirBattle::CopiesFor(const FString& Key, bool bShadow, UStaticMesh* Mesh, UMaterialInterface* Ink)
{
	if (FPlaneCopies* Have = PlaneCopies.Find(Key); Have && Have->Copies.IsValid())
	{
		return Have;
	}
	if (!Mesh || !Ink)
	{
		return nullptr;
	}
	FPlaneCopies& Group = PlaneCopies.FindOrAdd(Key);
	UInstancedStaticMeshComponent* Copies = NewObject<UInstancedStaticMeshComponent>(this, MakeUniqueObjectName(this, UInstancedStaticMeshComponent::StaticClass(), FName(TEXT("AirPlaneCopies"))));
	Copies->SetupAttachment(Root);
	Copies->SetStaticMesh(Mesh);
	Copies->SetMaterial(0, Ink);
	Copies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Copies->SetCanEverAffectNavigation(false);
	// (a shadow within ShadowFarM, as the aircraft's own meshes; lit by the light thrown up from under the eye, too)
	Copies->SetCastShadow(bShadow);
	Copies->LightingChannels.bChannel1 = true;
	// (places removed off the end only, as the ground battle's crowds; each place's place a frame before kept, for the
	// motion: set before the component is registered, and never on a static one, which the engine forbids)
	Copies->SetRemoveSwap();
	Copies->SetMobility(EComponentMobility::Movable);
	Copies->SetHasPerInstancePrevTransforms(true);
	Copies->SetVisibility(false);
	Copies->RegisterComponent();
	PlaneKeep.Add(Copies);
	Group.Copies = Copies;
	return &Group;
}

AAirBattle::FPlaneCopies* AAirBattle::BombCopies(bool bTorpedo)
{
	TObjectPtr<UStaticMesh>& Still = bTorpedo ? TorpedoStill : BombStill;
	if (!Still)
	{
		// (the rod each was laid as: 2 m by 0.4 for a bomb, 5.5 m by 0.56 for a torpedo, six sides, in its casing)
		const double U = UnitsPerMetre;
		const double Long = (bTorpedo ? 5.5 : 2.0) * U;
		FMiniatureMesh Made;
		Made.Rod(FVector(-Long * 0.5, 0.0, 0.0), FVector(Long * 0.5, 0.0, 0.0), (bTorpedo ? 0.28 : 0.2) * U, AirParts::Casing, 6);
		AirParts::FShape Shape;
		Shape.Points = Made.Vertices;
		Shape.Triangles = Made.Triangles;
		Shape.Normals = Made.Normals;
		Shape.Colors = Made.Colors;
		Still = AirParts::StillOf(this, Shape, PlainInk);
		if (!Still)
		{
			return nullptr;
		}
	}
	return CopiesFor(bTorpedo ? TEXT("torpedo") : TEXT("bomb"), false, Still, PlainInk);
}

void AAirBattle::SendCopies()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_SendCopies);
	CopiesMeshes = 0;
	for (TPair<FString, FPlaneCopies>& Pair : PlaneCopies)
	{
		FPlaneCopies& Group = Pair.Value;
		UInstancedStaticMeshComponent* Copies = Group.Copies.Get();
		const int32 Want = Group.At.Num();
		if (Copies)
		{
			// (as many places as aircraft: the last ones removed, last first, or new ones added at the end, which the
			// engine gives their place now as their place a frame before; then every place sent with where its aircraft
			// is and where it was, whichever place it had the frame before)
			const int32 Have = Copies->GetInstanceCount();
			if (Have > Want)
			{
				TArray<int32> Gone;
				Gone.Reserve(Have - Want);
				for (int32 Slot = Have - 1; Slot >= Want; --Slot)
				{
					Gone.Add(Slot);
				}
				Copies->RemoveInstances(Gone, true);
			}
			else if (Have < Want)
			{
				const TArray<FTransform> Added(Group.At.GetData() + Have, Want - Have);
				Copies->AddInstances(Added, false, false);
			}
			if (Want > 0)
			{
				Copies->BatchUpdateInstancesTransforms(0, Group.At, Group.Before, false, false, false);
				Copies->MarkRenderInstancesDirty();
				++CopiesMeshes;
			}
			if (Copies->IsVisible() != (Want > 0))
			{
				Copies->SetVisibility(Want > 0);
			}
		}
		Group.Keys.Reset();
		Group.At.Reset();
		Group.Before.Reset();
	}
}

// ---- (1.5.1) The opening held ----------------------------------------------------------------------------------
// The Mac's cold Rhineland raids of 24 November (build 3e098f1f; p1 twice in one run, p2 with the sky captured at every
// step, p4 with no capture at all): every raid, the draw thread took 0.6 to 1.1 s a frame for the two frames after the
// press while the game thread idled, the flyover running and the aircraft jumping through them; the first raid of a run
// stalled once more (0.7 to 1.2 s) as the land's picture first went down, and the game thread 258 ms as the defending
// fighters' model was first built mid-flight. The sky light's capture was not it (p4). So the raid now opens behind a
// black frame with its title, the clock held, while the scene just made is drawn and taken in, with a sheet of each
// kind first drawn later set before the eye and every aircraft model of the raid made; the black lifts when the frames
// come quick again, and the flyover begins from its first second.

bool AAirBattle::NoCurtain()
{
	static const bool bAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterAirNoCurtain"));
	return bAsked;
}

void AAirBattle::WarmUp()
{
	FAirBattleState& S = *State;
	S.bWarming = true;
	S.Curtain = 1.0;
	WarmBegan = -1.0;
	WarmQuick = 0;
	WarmFrames = 0;
	WarmSlowest = 0.0;
	bWarmShapes = false;
	// A sheet of each kind the raid first draws only later, its shaders made ready under the black: the land's paint
	// (LayLand's sheets, once the pool has painted them; the stall that came as they went down was the paint's own loops
	// crowding the render thread's jobs, now at the background's priority: PaintLand),
	// and an aircraft on its own mesh (PlaneMesh: those near the eye, first seen at the flyover's later shots). Each as
	// its own kind is made, the same material, the same points' parts and the same shadow; set before the eye each frame
	// while the black holds (WarmTick), and taken down with it (WarmDone).
	auto Quad = [this](const TCHAR* Name, UMaterialInterface* Ink, bool bUv, bool bPlane)
	{
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), FName(Name)));
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (bPlane)
		{
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetCastShadow(true);
			Mesh->LightingChannels.bChannel1 = true;
			Mesh->bUseAsyncCooking = false;
		}
		else
		{
			Mesh->SetCastShadow(false);
		}
		Mesh->RegisterComponent();
		const double Half = 3.0 * UnitsPerMetre;
		const TArray<FVector> Points = { FVector(0.0, -Half, -Half), FVector(0.0, Half, -Half), FVector(0.0, Half, Half), FVector(0.0, -Half, Half) };
		// (both faces, so that it shows whichever way it is turned)
		const TArray<int32> Triangles = { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 };
		TArray<FVector> Normals;
		Normals.Init(bPlane ? FVector::ForwardVector : FVector::UpVector, Points.Num());
		TArray<FColor> Colours;
		Colours.Init(FColor::White, Points.Num());
		TArray<FVector2D> Uv;
		if (bUv)
		{
			Uv = { FVector2D(0.0, 1.0), FVector2D(1.0, 1.0), FVector2D(1.0, 0.0), FVector2D(0.0, 0.0) };
		}
		Mesh->CreateMeshSection(0, Points, Triangles, Normals, Uv, Colours, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(0, Ink);
		WarmShelf.Add(Mesh);
	};
	if (LandMade.IsValid())
	{
		UMaterialInterface* Cloth = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Flag.M_Flag"));
		TArray<FColor> White;
		White.Init(FColor::White, 16);
		UTexture2D* Plain = Cloth ? CampaignTextures::Create(4, 4, White, true, false, MakeUniqueObjectName(this, UTexture2D::StaticClass(), TEXT("AirWarmPicture"))) : nullptr;
		if (Plain)
		{
			// (LayLand's InkOf, as it is)
			UMaterialInstanceDynamic* Ink = UMaterialInstanceDynamic::Create(Cloth, this);
			Ink->SetTextureParameterValue(TEXT("Flag"), Plain);
			Ink->SetScalarParameterValue(TEXT("Glow"), 0.0f);
			Ink->SetScalarParameterValue(TEXT("Fade"), 0.0f);
			Ink->SetScalarParameterValue(TEXT("Drain"), 0.0f);
			Ink->SetScalarParameterValue(TEXT("Veiled"), 0.0f);
			Ink->SetScalarParameterValue(TEXT("Roughness"), 0.92f);
			WarmKeep.Add(Plain);
			WarmKeep.Add(Ink);
			Quad(TEXT("AirWarmLand"), Ink, true, false);
		}
	}
	if (PlaneInk)
	{
		Quad(TEXT("AirWarmPlane"), PlaneInk.Get(), false, true);
	}
}

void AAirBattle::WarmShapes()
{
	bWarmShapes = true;
	const FAirBattleState& S = *State;
	const double Began = FPlatformTime::Seconds();
	TSet<FString> Done;
	const int32 Before = AirParts::Shapes().Num();
	auto Ready = [&](const FString& Model, const FString& Nation, bool bFighter, double Span, bool bTorpedo)
	{
		const FString ShapeKey = ShapeFor(Model, Nation, bFighter, Span, bTorpedo);
		if (Done.Contains(ShapeKey))
		{
			return;
		}
		Done.Add(ShapeKey);
		// (and its copies' meshes, far and light, as Draw asks for them: PlaneStill keeps them for the raid)
		if (AirParts::Shapes().Contains(ShapeKey + TEXT("|far")))
		{
			PlaneStill(ShapeKey, false);
			PlaneStill(ShapeKey, true);
		}
	};
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		const auto Flown = S.Show.Groups().find(P.Group);
		const bool bTorpedo = !P.bFighter && Flown != S.Show.Groups().end() && Flown->second.Strike == "torpedo";
		Ready(AirParts::Wide(P.Model), AirParts::Wide(P.Nation), P.bFighter, P.Span, bTorpedo);
	}
	// The place's main models of both sides, as the board names them ("<nat>:ftr", "<nat>:bmr"): a side's fighters
	// that are not yet in the air (the Bf 109s over the Rhineland, first seen 18 s in) are made here too. A stand-in is
	// made to its group's own span when it first shows, as before.
	for (const auto& Pair : S.Board.Models)
	{
		FString Nat, Kind;
		if (Pair.second.empty() || !AirParts::Wide(Pair.first).Split(TEXT(":"), &Nat, &Kind))
		{
			continue;
		}
		const bool bFighter = Kind == TEXT("ftr");
		const FString Model = AirParts::Wide(Pair.second);
		const FString Name = ModelFor(Model, Nat, bFighter);
		if (Name.StartsWith(TEXT("Fighter:")) || Name.StartsWith(TEXT("Bomber:")))
		{
			continue;
		}
		Ready(Model, Nat, bFighter, 12.0, false);
	}
	UE_LOG(LogTheater, Display, TEXT("Air battle: the raid's %d aircraft shapes made ready under the opening's black (%d near and far shapes new this run), with their copies' meshes, in %.0f ms."),
		Done.Num(), AirParts::Shapes().Num() - Before, 1000.0 * (FPlatformTime::Seconds() - Began));
}

void AAirBattle::WarmTick(float DeltaSeconds)
{
	const double Now = FPlatformTime::Seconds();
	if (WarmBegan < 0.0)
	{
		WarmBegan = Now;
	}
	++WarmFrames;
	// (the frame just ended as the speed log reads it; the draw thread's time is its last frame's, which holds the black
	// a frame longer at most)
	const double FrameMs = 1000.0 * static_cast<double>(DeltaSeconds);
	const double GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const double DrawMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	WarmSlowest = FMath::Max(WarmSlowest, FMath::Max3(FrameMs, GameMs, DrawMs));
	const bool bQuick = FrameMs < AirParts::WarmQuickMs && GameMs < AirParts::WarmQuickMs && DrawMs < AirParts::WarmQuickMs;
	WarmQuick = bQuick ? WarmQuick + 1 : 0;
	if (Eye)
	{
		// (60 m before the eye, side by side, turned to it)
		const FVector At = Eye->GetActorLocation();
		const FVector Ahead = Eye->GetActorForwardVector();
		const FVector Right = Eye->GetActorRightVector();
		for (int32 K = 0; K < WarmShelf.Num(); ++K)
		{
			if (UProceduralMeshComponent* Mesh = WarmShelf[K].Get())
			{
				const double Across = (K - 0.5 * (WarmShelf.Num() - 1)) * 8.0 * UnitsPerMetre;
				Mesh->SetWorldLocationAndRotation(At + Ahead * (60.0 * UnitsPerMetre) + Right * Across, Eye->GetActorRotation());
			}
		}
	}
	const double Held = Now - WarmBegan;
	if (Held >= AirParts::WarmMost)
	{
		WarmDone(TEXT("held its longest"));
	}
	else if (WarmQuick >= AirParts::WarmQuickWanted && Held >= AirParts::WarmLeast)
	{
		WarmDone(TEXT("the frames quick again"));
	}
}

void AAirBattle::WarmDone(const TCHAR* Why)
{
	if (!State.IsValid() || !State->bWarming)
	{
		return;
	}
	State->bWarming = false;
	for (const TObjectPtr<UProceduralMeshComponent>& Mesh : WarmShelf)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	WarmShelf.Empty();
	WarmKeep.Empty();
	UE_LOG(LogTheater, Display, TEXT("Air battle: the opening held %.2f s behind its black frame (%d frames, the slowest %.0f ms; %s); the flyover begins. (-TheaterAirNoCurtain: the old opening)"),
		WarmBegan >= 0.0 ? FPlatformTime::Seconds() - WarmBegan : 0.0, WarmFrames, WarmSlowest, Why);
}

void AAirBattle::Draw(double Real)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_Draw);
	FAirBattleState& S = *State;
	// Every aircraft where the flight core has it: its nose along its path, banked into its turn. The models are built
	// nose toward -X with the right hand at -Y, so each is turned half round before its heading, climb and bank.
	const FQuat HalfRound = FRotator(0.0, 180.0, 0.0).Quaternion();
	const double U = UnitsPerMetre;
	const FVector EyeAt = Eye ? Eye->GetActorLocation() : FVector::ZeroVector;
	// AS1: past the near shape, each aircraft a copy in its shape's instanced mesh (PlaneCopies), its light shape past
	// LightFromM through the battle's own 55 degree lens (a narrower lens, enlarging, moves it out as far as it enlarges).
	static const bool bCopiesOn = []
	{
		int32 On = 1;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterPlaneCopies="), On);
		UE_LOG(LogTheater, Display, TEXT("Air battle: aircraft past their near shape drawn as copies, one mesh a shape (AS1): %s."), On != 0 ? TEXT("on") : TEXT("off, each on its own mesh"));
		return On != 0;
	}();
	static const double LightFromM = []
	{
		double Metres = AirLight::LightFromM;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterPlaneLightM="), Metres);
		Metres = FMath::Clamp(Metres, 0.0, 1.0e6);
		UE_LOG(LogTheater, Display, TEXT("Air battle: the copies' light shapes past %.0f m through the battle's 55 degree lens."), Metres);
		return Metres;
	}();
	const double Lens = Eye && Eye->GetCameraComponent() ? FMath::Clamp(static_cast<double>(Eye->GetCameraComponent()->FieldOfView), 5.0, 120.0) : 55.0;
	const double Enlarged = FMath::Tan(FMath::DegreesToRadians(55.0 * 0.5)) / FMath::Tan(FMath::DegreesToRadians(Lens * 0.5));
	CopiesDrawn = 0;
	TSet<int32> Seen;
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		Seen.Add(P.Key);
		// (laid each frame, so an aircraft's nation is always this battle's: the pre-check on round 16)
		GunNations.Add(P.Key, AirParts::KitNation(P.Nation));
		TObjectPtr<UProceduralMeshComponent>* Have = Planes.Find(P.Key);
		if (!P.bShown)
		{
			if (Have && *Have)
			{
				(*Have)->SetVisibility(false);
			}
			if (FPlaneMade* Hidden = PlaneMade.Find(P.Key))
			{
				ShowDiscs(*Hidden, false);
				// (out of the picture: no copy, and no move from where it was when it comes back)
				Hidden->bCopy = false;
				Hidden->bWas = false;
			}
			continue;
		}
		const auto Flown = S.Show.Groups().find(P.Group);
		const bool bTorpedo = !P.bFighter && Flown != S.Show.Groups().end() && Flown->second.Strike == "torpedo";
		UProceduralMeshComponent* Mesh = PlaneMesh(P.Key, AirParts::Wide(P.Model), AirParts::Wide(P.Nation), P.bFighter, P.Span, bTorpedo);
		if (!Mesh)
		{
			continue;
		}
		const FQuat Attitude = FRotator(P.Pitch, P.Heading, P.Bank).Quaternion() * HalfRound;
		const FVector Where = World(P.At.X, P.At.Y, P.At.Z);
		const double Away = FVector::Dist(Where, EyeAt) / U;
		const FTransform Now(Attitude, Where);
		FPlaneMade* Lit = PlaneMade.Find(P.Key);
		if (Lit)
		{
			const double Reach = (P.bOnGround ? AirParts::ShadowGroundM : AirParts::ShadowFarM) * AirParts::ShadowScale() * (Lit->bShadow ? 1.1 : 1.0);
			const bool bShadow = Away < Reach;
			if (bShadow != Lit->bShadow)
			{
				Mesh->SetCastShadow(bShadow);
				Lit->bShadow = bShadow;
			}
		}
		// The near shape within 300 m of the eye (and for the ridden aircraft), the far one past 450 m.
		const bool bRidden = S.bClose && S.Subject.Plane == P.Key;
		// (held on the near shape while its bay doors are open: the far shape has no bay, the art director, 06:48Z)
		const bool bNearShape = Lit && Lit->bTwoShapes && (bRidden || Away < (Lit->bNear ? 450.0 : 300.0) || (Lit->bNear && Lit->bBay && P.Bay > 0.0));
		if (bCopiesOn && Lit && Lit->bTwoShapes && !bNearShape)
		{
			const bool bLightWanted = Away * Enlarged > LightFromM * (Lit->bCopy && Lit->bLight ? 0.9 : 1.0);
			UStaticMesh* Still = bLightWanted ? PlaneStill(Lit->Shape, true) : nullptr;
			const bool bLight = Still != nullptr;
			if (!Still)
			{
				Still = PlaneStill(Lit->Shape, false);
			}
			const FString CopiesKey = Lit->Shape + (bLight ? TEXT("|light") : TEXT("|far")) + (Lit->bShadow ? TEXT("|shadow") : TEXT("|plain"));
			if (FPlaneCopies* Group = Still ? CopiesFor(CopiesKey, Lit->bShadow, Still, PlaneInk) : nullptr)
			{
				if (!Lit->bCopy)
				{
					// (from its own mesh: its near shape put by and the mesh hidden, its airscrews with it)
					PlaneNear(P.Key, Mesh, false);
					Mesh->SetVisibility(false);
					ShowDiscs(*Lit, false);
					Lit->bCopy = true;
				}
				Lit->bLight = bLight;
				Group->Keys.Add(P.Key);
				Group->At.Add(Now);
				Group->Before.Add(Lit->bWas ? Lit->Was : Now);
				Lit->Was = Now;
				Lit->bWas = true;
				++CopiesDrawn;
				continue;
			}
		}
		// On its own mesh: moved before it is shown, so that one coming back from a copy is not drawn a frame where it
		// was when it became one.
		Mesh->SetWorldLocationAndRotation(Where, Attitude);
		Mesh->SetVisibility(true);
		if (Lit)
		{
			Lit->bCopy = false;
			Lit->bLight = false;
			Lit->Was = Now;
			Lit->bWas = true;
			if (Lit->bTwoShapes)
			{
				PlaneNear(P.Key, Mesh, bNearShape);
			}
		}
		// Its bay doors as the flight core has them, while it is near (turned only when they have moved), and its racked
		// load gone once its stick begins to fall.
		if (FPlaneMade* Bay = PlaneMade.Find(P.Key); Bay && Bay->bBay)
		{
			if (Bay->bNear && FMath::Abs(P.Bay - Bay->BayShown) > 0.004)
			{
				if (const AirParts::FShape* Near = AirParts::Shapes().Find(Bay->Shape); Near && Near->Doors.Num() == 2)
				{
					for (int32 K = 0; K < 2; ++K)
					{
						TArray<FVector> Points, Normals;
						AirParts::TurnDoor(Near->Doors[K], FMath::Clamp(P.Bay, 0.0, 1.0) * FMath::Min(80.0, Near->Doors[K].Most), Points, Normals);
						Mesh->UpdateMeshSection(2 + K, Points, Normals, TArray<FVector2D>(), Near->Doors[K].Colors, TArray<FProcMeshTangent>());
					}
					Bay->BayShown = P.Bay;
				}
			}
			if (P.bBombsGone != Bay->bBombsGone)
			{
				Bay->bBombsGone = P.bBombsGone;
				if (Mesh->GetNumSections() > 4)
				{
					Mesh->SetMeshSectionVisible(4, Bay->bNear && !Bay->bBombsGone);
				}
			}
		}
		// Its airscrews turning while it is near: a falling aircraft's windmill slower. A dead engine's airscrew (the
		// audit of 22 November: they all turned on) windmills slower, or once feathered stands still, its blades edge on
		// to the wind.
		if (FPlaneMade* Turning = PlaneMade.Find(P.Key); Turning && Turning->bNear && Turning->Discs.Num() > 0)
		{
			ShowDiscs(*Turning, true); // (shown again with an aircraft that was out of sight while near)
			if (Turning->DiscEngine.Num() != Turning->Discs.Num())
			{
				TakeDiscEngines(*Turning, AirShow::ShapeOf(P.Model, P.bFighter));
			}
			const double Turns = P.bFalling ? ScrewTurns * 0.3 : ScrewTurns;
			const double Angle = FMath::Fmod(RealT * Turns * 360.0 + Turning->Phase, 360.0);
			for (int32 K = 0; K < Turning->Discs.Num(); ++K)
			{
				UProceduralMeshComponent* Disc = Turning->Discs[K].Get();
				if (!Disc)
				{
					continue;
				}
				const bool bDead = std::find(P.DeadEngines.begin(), P.DeadEngines.end(), Turning->DiscEngine[K]) != P.DeadEngines.end();
				const bool bStill = bDead && P.bFeathered;
				if (Turning->DiscStill[K] != bStill && Disc->GetNumSections() > 1)
				{
					Turning->DiscStill[K] = bStill;
					Disc->SetMeshSectionVisible(0, !bStill);
					Disc->SetMeshSectionVisible(1, bStill);
				}
				const double Own = bStill ? Turning->Phase : bDead ? FMath::Fmod(RealT * Turns * 0.45 * 360.0 + Turning->Phase, 360.0) : Angle;
				Disc->SetRelativeRotation(FRotator(0.0, 0.0, Own + K * 23.0));
			}
		}
	}
	for (TPair<int32, TObjectPtr<UProceduralMeshComponent>>& Pair : Planes)
	{
		if (!Seen.Contains(Pair.Key) && Pair.Value)
		{
			Pair.Value->SetVisibility(false);
			if (FPlaneMade* Gone = PlaneMade.Find(Pair.Key))
			{
				ShowDiscs(*Gone, false);
				Gone->bCopy = false;
				Gone->bWas = false;
			}
		}
	}
	// Every ten seconds a line of how many copies (for the Mac's readings).
	if (bCopiesOn && RealT >= CopiesLogAt)
	{
		CopiesLogAt = RealT + 10.0;
		int32 Own = 0;
		for (const TPair<int32, TObjectPtr<UProceduralMeshComponent>>& Pair : Planes)
		{
			Own += Pair.Value && Pair.Value->IsVisible() ? 1 : 0;
		}
		UE_LOG(LogTheater, Display, TEXT("Air battle: %d aircraft drawn as copies, %d on their own meshes, and %d bombs falling as copies, in %d meshes the frame before (lens %.0f degrees)."),
			CopiesDrawn, Own, BombsDrawnWas, CopiesMeshes, Lens);
	}
	// The strikes on those near the eye, laid on their skins; taken up from the rest.
	DrawMarks(EyeAt);
	DrawTally(EyeAt);
	// The small things, laid afresh each frame in one mesh: a parachute's canopy and its airman, a torn piece of a
	// wreck tumbling; a bomb or a torpedo falling a copy of its own mesh (AS2), sent with the aircraft's copies.
	FMiniatureMesh Things;
	const double AirNowDrawn = S.Show.AirNow();
	const double AirStep = BombsAirWas >= 0.0 ? AirNowDrawn - BombsAirWas : 0.0;
	BombsAirWas = AirNowDrawn;
	BombsDrawn = 0;
	// (a real crew gets out, up to ten from a B-17, so three lost together hang thirty canopies: every one is drawn, the
	// audit of 22 November; past 600 m from the eye with fewer sides and no rigging, past 2.5 km as a plain canopy and
	// its man, and only past the nearest MostChutes, the farthest first, are any left out)
	constexpr int32 MostChutes = 240;
	double ChuteFar = TNumericLimits<double>::Max();
	{
		TArray<double> Aways;
		for (const AirShow::Thing& T : S.Show.Things())
		{
			if (T.Kind == AirShow::ThingKind::Chute)
			{
				Aways.Add(FVector::Dist(World(T.At.X, T.At.Y, T.At.Z), EyeAt) / U);
			}
		}
		if (Aways.Num() > MostChutes)
		{
			Aways.Sort();
			ChuteFar = FMath::Max(600.0, Aways[MostChutes - 1]);
		}
	}
	for (const AirShow::Thing& T : S.Show.Things())
	{
		const FVector At = World(T.At.X, T.At.Y, T.At.Z);
		switch (T.Kind)
		{
		case AirShow::ThingKind::Chute:
		{
			const double Away = FVector::Dist(At, EyeAt) / U;
			if (Away > ChuteFar)
			{
				break;
			}
			const int32 Sides = Away < 600.0 ? 14 : Away < 2500.0 ? 8 : 5;
			if (T.bOpen)
			{
				Things.Pillar(At + FVector(0.0, 0.0, 5.2 * U), 3.6 * U, 0.9 * U, 1.7 * U, AirParts::Silk, Sides);
				if (Away < 600.0)
				{
					for (const double Side : { -1.0, 1.0 })
					{
						Things.Rod(At + FVector(0.0, 0.0, 1.6 * U), At + FVector(Side * 3.2 * U, 0.0, 5.2 * U), 0.03 * U, AirParts::Silk, 3);
					}
				}
			}
			else
			{
				Things.Limb(At + FVector(0.0, 0.0, 1.6 * U), At + FVector(0.0, 0.0, 4.0 * U), 0.15 * U, 0.35 * U, AirParts::Silk, FMath::Min(6, Sides));
			}
			Things.Limb(At, At + FVector(0.0, 0.0, 1.7 * U), 0.22 * U, 0.2 * U, AirParts::Airman, FMath::Min(6, Sides));
			break;
		}
		case AirShow::ThingKind::Piece:
		{
			const double Turn = FMath::RadiansToDegrees(T.Spin * T.Age) + T.Key * 37.0;
			const double Side = FMath::Max(0.3, T.Size);
			Things.Box(At, FVector(Side * U, Side * 0.6 * U, 0.08 * U), (T.Key % 3 == 0) ? AirParts::Scorched : AirParts::Skin, Turn, Turn * 0.7);
			break;
		}
		case AirShow::ThingKind::Bomb:
		{
			const FVector Velocity = WorldVector(T.Vel.X, T.Vel.Y, T.Vel.Z);
			const FVector Way = Velocity.GetSafeNormal();
			if (FPlaneCopies* Falling = bCopiesOn ? BombCopies(T.bTorpedo) : nullptr)
			{
				// (AS2: a copy of the bomb's mesh, along its fall; where it was a frame before from its velocity and the
				// air time the frame drew on, none on the first frame or a clock run back)
				const FTransform Here(FRotationMatrix::MakeFromX(Way.IsNearlyZero() ? FVector(0.0, 0.0, -1.0) : Way).ToQuat(), At);
				FTransform There = Here;
				if (AirStep > 0.0 && AirStep < 1.0)
				{
					There.SetLocation(At - Velocity * AirStep);
				}
				Falling->Keys.Add(T.Key);
				Falling->At.Add(Here);
				Falling->Before.Add(There);
				++BombsDrawn;
				break;
			}
			const double Long = (T.bTorpedo ? 5.5 : 2.0) * U;
			Things.Rod(At - Way * Long * 0.5, At + Way * Long * 0.5, (T.bTorpedo ? 0.28 : 0.2) * U, AirParts::Casing, 6);
			break;
		}
		}
	}
	// The copies' places sent, once for all of them: the aircraft's and the bombs'.
	SendCopies();
	BombsDrawnWas = BombsDrawn;
	Small->ClearAllMeshSections();
	if (Things.Vertices.Num() > 0)
	{
		Small->CreateMeshSection(0, Things.Vertices, Things.Triangles, Things.Normals, TArray<FVector2D>(), Things.Colors, TArray<FProcMeshTangent>(), false);
		Small->SetMaterial(0, PlainInk);
	}
	// The blasts' torn flames: streaming on with what is left of the aircraft for a moment, then out.
	for (int32 K = Torn.Num() - 1; K >= 0; --K)
	{
		FTorn& Flame = Torn[K];
		Flame.At += Flame.Velocity * Real;
		Flame.Velocity *= FMath::Max(0.0, 1.0 - Real * 0.8);
		if (Flame.Kit)
		{
			Flame.Kit->TrailMove(S.Show.AirNow(), Flame.Key, Flame.At, Flame.Velocity);
		}
		if (RealT >= Flame.Until)
		{
			if (Flame.Kit)
			{
				Flame.Kit->TrailStop(Flame.Key);
			}
			Torn.RemoveAt(K);
		}
	}
}

void AAirBattle::PlayEffects()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_PlayEffects);
	FAirBattleState& S = *State;
	const double Now = S.Show.AirNow();
	const double U = UnitsPerMetre;
	const FVector Listener = Eye ? Eye->GetActorLocation() : FVector::ZeroVector;
	// (rule 13 holds the eye off a fuel ball for its first 2 s on the air clock, the life the kit gives it; a replay's
	// clock run back drops them too)
	BallsNear.RemoveAll([Now](const FFuelBallNear& Ball) { return Now - Ball.Born > 2.0 || Now < Ball.Born - 1.0; });
	// A fighter's fixed guns of one kind fire together, and are heard as one take: one recording laid once a barrel on
	// the same frame adds up in step (four barrels +12 dB, eight +18, and locked together, into the limiter), where real
	// guns firing out of step add up as their power, 10 log10 N. So each aircraft's fixed guns of a kind share one key
	// (below every real gun's key), and its take plays at the square root of how many of them fire this frame.
	auto WingKey = [](int32 Key, const std::string& What)
	{
		return -1 - ((Key / 64) * 4 + (What == "cannon20" ? 0 : What == "hmg" ? 1 : 2));
	};
	TMap<int32, int32> WingBarrels;
	for (const AirShow::Fx& E : S.Show.Effects)
	{
		if (E.Kind == AirShow::FxKind::Gun && E.Text == "fixed" && !ShipGuns::IsStream(E.Key))
		{
			++WingBarrels.FindOrAdd(WingKey(E.Key, E.What));
		}
	}
	// A gunner's twin mount (a turret's two guns side by side in one opening, laid and fired together) is one take too,
	// by the same rule: two takes laid on the same frame added in step, +6 dB and locked (the Bremen r2d film, 22 Nov:
	// a B-17 box's gunners at the ridden Fw 190 held the limiter down for 10 s, twin lays 1-3 dB over what one take can
	// reach). The mount is found by where its guns are, not by how the show numbers them: guns of one aircraft, of one
	// kind and mount, within a metre of each other this frame. It is heard under its lowest gun's key, moved out of the
	// guns' own range (MountKey), or under the key it is already heard under, at the square root of how many of the guns
	// heard under that key fire this frame.
	auto MountKey = [](int32 Key) { return -(1 << 30) - Key; };
	TMap<int32, int32> MountOf;
	TMap<int32, int32> MountGuns;
	{
		TArray<const AirShow::Fx*> Loose;
		for (const AirShow::Fx& E : S.Show.Effects)
		{
			if (E.Kind == AirShow::FxKind::Gun && E.Text != "fixed" && !ShipGuns::IsStream(E.Key))
			{
				Loose.Add(&E);
			}
		}
		TArray<int32> Lowest;
		Lowest.SetNum(Loose.Num());
		for (int32 I = 0; I < Loose.Num(); ++I)
		{
			Lowest[I] = I;
			const AirShow::Fx& A = *Loose[I];
			for (int32 J = 0; J < I; ++J)
			{
				const AirShow::Fx& B = *Loose[J];
				const double Dx = A.At.X - B.At.X, Dy = A.At.Y - B.At.Y, Dz = A.At.Z - B.At.Z;
				if (A.Key / 64 == B.Key / 64 && A.What == B.What && A.Text == B.Text && Dx * Dx + Dy * Dy + Dz * Dz < 1.0)
				{
					Lowest[I] = Lowest[J];
					break;
				}
			}
		}
		TMap<int32, int32> LowestKey;
		for (int32 I = 0; I < Loose.Num(); ++I)
		{
			if (int32* Low = LowestKey.Find(Lowest[I]))
			{
				*Low = FMath::Min(*Low, Loose[I]->Key);
			}
			else
			{
				LowestKey.Add(Lowest[I], Loose[I]->Key);
			}
		}
		// (a mount one of whose guns is already heard keeps that gun's key, so guns that come in on different frames,
		// as when the show's nearest-guns cut drops one for a frame, still make one take)
		TMap<int32, int32> HeardUnder;
		for (int32 I = 0; I < Loose.Num(); ++I)
		{
			if (const int32* Was = WingOf.Find(Loose[I]->Key))
			{
				if (!HeardUnder.Contains(Lowest[I]))
				{
					HeardUnder.Add(Lowest[I], *Was);
				}
			}
		}
		for (int32 I = 0; I < Loose.Num(); ++I)
		{
			const int32 Shared = HeardUnder.Contains(Lowest[I]) ? HeardUnder.FindRef(Lowest[I]) : MountKey(LowestKey.FindRef(Lowest[I]));
			MountOf.Add(Loose[I]->Key, Shared);
			++MountGuns.FindOrAdd(Shared);
		}
	}
	for (const AirShow::Fx& E : S.Show.Effects)
	{
		const FVector At = World(E.At.X, E.At.Y, E.At.Z);
		switch (E.Kind)
		{
		case AirShow::FxKind::Gun:
		{
			// The first attack run on a ship (the guns-fire-back picture): her light guns laid on an attacker within
			// GunsReach of her, a battleship or a carrier where the battle has one (Pearl Harbor's first was a destroyer
			// in a nest, and the picture is for Battleship Row). He is the aircraft nearest where they are laid (their
			// lead puts him a little short of it).
			if (GunsRealT < 0.0 && ShipGuns::IsStream(E.Key) && !S.bFlying
				&& (CapitalShips.Num() == 0 || CapitalShips.Contains((E.Key - ShipGuns::StreamBase) / 64)))
			{
				const AirShow::PlaneOut* On = nullptr;
				double Nearest = 400.0;
				for (const AirShow::PlaneOut& P : S.Show.Planes())
				{
					const double Off = (P.At - E.Aim).Length();
					if (P.bShown && !P.bOnGround && !P.bFalling && Off < Nearest)
					{
						Nearest = Off;
						On = &P;
					}
				}
				const double Range = On ? (On->At - E.At).Length() : GunsReach + 1.0;
				if (On && Range <= GunsReach)
				{
					GunsRealT = RealT;
					GunsShip = (E.Key - ShipGuns::StreamBase) / 64;
					GunsOn = On->Key;
					GunsPlane = World(On->At.X, On->At.Y, On->At.Z);
					const FString* Name = ShipNames.Find(GunsShip);
					UE_LOG(LogTheater, Display, TEXT("Air battle: guns fire back picture on %s, her light guns on a %s %.0f m off, %.0f m up"),
						Name ? **Name : TEXT("a ship"), *AirParts::Wide(On->Model), Range, On->At.Z);
				}
			}
			// (in the kit for the muzzle's height, kept while the gun fires unless it passes the clouds' base)
			FBattleEffects* const* Was = GunKits.Find(E.Key);
			FBattleEffects* const Before = Was ? *Was : nullptr;
			FBattleEffects* const Kit = KitAt(At, Before);
			if (Before && Before != Kit)
			{
				Before->AirGunStop(E.Key);
			}
			if (Kit)
			{
				GunKits.Add(E.Key, Kit);
				const FString* Nation = GunNations.Find(E.Key / 64); // (the gun's key names its aircraft: Key * 64 + gun)
				AirParts::AirGun(Kit, Now, E.Key, At, World(E.Aim.X, E.Aim.Y, E.Aim.Z), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), AirParts::Wide(E.What), static_cast<float>(E.Rate),
				                 static_cast<float>(E.Mv * U), Nation ? *Nation : FString());
			}
			// The burst heard (mix.json air.gunsByCalibre): takes laid end to end while the trigger is down, each the
			// mix's part of its recording (the firing, without the range's echo), the next one due as that part ends.
			// Every gun seen firing is heard (Brian, 21 November 16:21Z: "when ever there is gun fire make sure the sound
			// of the guns happens. that didnt happen with the german plane"): the ridden aircraft's own guns at the eye,
			// unplaced (air.closeView.ownGuns), the rest at the muzzle and carried along with it while they play.
			const bool bOwnGuns = S.bClose && E.Key / 64 == S.Subject.Plane;
			// (a ship's light guns of one kind are heard as one battery, under one key: ShipGuns::HeardKey; laid where
			// its mount fires, and left there, as the ship hardly moves while a take plays)
			const bool bShip = ShipGuns::IsStream(E.Key);
			const bool bWing = !bShip && E.Text == "fixed";
			// (a gunner's mount: under the key it was first heard under while it fires, so its take is cut only with
			// the last of its guns, as a wing's)
			const bool bMount = !bShip && !bWing;
			const int32* HeardAs = bMount ? WingOf.Find(E.Key) : nullptr;
			const int32 SoundKey = bShip ? ShipGuns::HeardKey(E.Key, ShipGuns::KindSlot(E.What)) : bWing ? WingKey(E.Key, E.What)
				: HeardAs ? *HeardAs : MountOf.Contains(E.Key) ? MountOf.FindRef(E.Key) : E.Key;
			if ((bWing || bMount) && !WingOf.Contains(E.Key))
			{
				WingOf.Add(E.Key, SoundKey);
				++WingLive.FindOrAdd(SoundKey);
			}
			const double* Due = GunHeardAt.Find(SoundKey);
			if (!Due)
			{
				++GunBursts;
			}
			if (!Due || RealT >= *Due)
			{
				const TSharedPtr<FJsonObject> Calibre = AirParts::MixObject(Mix, TEXT("air.gunsByCalibre"));
				FString Take;
				float Cut = 0.0f;
				if (E.What == "cannon20" || E.What == "aa25" || E.What == "aa40")
				{
					const TSharedPtr<FJsonObject> Cannon = AirParts::MixObject(Calibre, TEXT("cannon"));
					Take = AirParts::MixTake(Cannon, TEXT("take"));
					const TArray<TSharedPtr<FJsonValue>>* Part = nullptr;
					if (Cannon.IsValid() && Cannon->TryGetArrayField(TEXT("part"), Part) && Part->Num() == 2)
					{
						Cut = static_cast<float>((*Part)[1]->AsNumber());
					}
				}
				else if (E.What == "hmg")
				{
					const TSharedPtr<FJsonObject> Heavy = AirParts::MixObject(Calibre, TEXT("heavy"));
					Take = AirParts::MixTake(Heavy, E.Text == "fixed" ? TEXT("wing") : E.Text == "turret" ? TEXT("turret") : TEXT("handHeld"));
					// (partByTake: each heavy take's own stretch of firing, mix v3)
					const TSharedPtr<FJsonObject> Parts = AirParts::MixObject(Heavy, TEXT("partByTake"));
					const TArray<TSharedPtr<FJsonValue>>* Part = nullptr;
					if (Parts.IsValid() && !Take.IsEmpty() && Parts->TryGetArrayField(Take, Part) && Part->Num() == 2)
					{
						Cut = static_cast<float>((*Part)[1]->AsNumber());
					}
				}
				else
				{
					Take = AirParts::MixTake(AirParts::MixObject(Calibre, E.Text == "hand" ? TEXT("handHeldRifle") : TEXT("rifle")), TEXT("takes"), E.Key);
				}
				// The next take is due as this one's part ends (a hair early, so the bursts run on without a hole), or
				// at its end where the mix plays it whole; never sooner than the mix's gap after itself.
				double Again = 0.9;
				USoundBase* const Recording = Take.IsEmpty() ? nullptr : SoundOf(*Take);
				if (Recording)
				{
					const double Whole = Recording->GetDuration();
					Again = Cut > 0.0f ? Cut : (Whole > 0.0 && Whole < 10.0 ? Whole : 0.9);
				}
				Again = FMath::Clamp(Again - 0.02, FMath::Max(0.3, AirParts::MixNumber(Mix, TEXT("air.sameTakeGap"), 0.15)), 2.0);
				GunHeardAt.Add(SoundKey, RealT + Again);
				// (a wing's barrels as their power: the square root of how many fire now. The engine's voices: guns near
				// the eye before the far ones, so a near gun is never the one dropped; given before the take begins)
				const float Together = bWing ? FMath::Sqrt(static_cast<float>(FMath::Max(1, WingBarrels.FindRef(SoundKey))))
					: bMount ? FMath::Sqrt(static_cast<float>(FMath::Max(1, MountGuns.FindRef(SoundKey)))) : 1.0f;
				const float Voice = bOwnGuns ? 4.0f : FVector::Dist(At, Listener) < 300.0 * U ? 3.0f : 1.0f;
				UAudioComponent* Played = nullptr;
				if (Recording)
				{
					Played = bOwnGuns ? SoundAtEye(*Take, Together, Cut, false, Voice) : Sound(*Take, At, TEXT("air_guns"), Together, 0.0f, Cut, 0.0f, Voice);
				}
				if (Played)
				{
					++GunTakesLaid;
					// (its takes still playing kept, the finished ones let go)
					TArray<TWeakObjectPtr<UAudioComponent>> Had;
					GunTakes.MultiFind(SoundKey, Had);
					GunTakes.Remove(SoundKey);
					for (const TWeakObjectPtr<UAudioComponent>& Take_ : Had)
					{
						if (Take_.IsValid() && Take_->IsPlaying())
						{
							GunTakes.Add(SoundKey, Take_);
						}
					}
					GunTakes.Add(SoundKey, Played);
				}
				else if (Recording)
				{
					++GunTakesFar; // (past its family's silent distance: nothing to hear)
				}
				else
				{
					++GunTakesLost;
				}
			}
			else if (!bOwnGuns && !bShip)
			{
				// (a take laid at the muzzle goes along with the aircraft while it plays: a fighter covers 100 m in one)
				TArray<TWeakObjectPtr<UAudioComponent>> Playing;
				GunTakes.MultiFind(SoundKey, Playing);
				for (const TWeakObjectPtr<UAudioComponent>& Take_ : Playing)
				{
					if (Take_.IsValid() && Take_->IsPlaying() && Take_->GetAttachParent() == nullptr)
					{
						Take_->SetWorldLocation(At);
					}
				}
			}
			// Rounds passing the eye (air.roundsPassing): a gun not the ridden aircraft's whose line of fire passes
			// within the mix's `within` of the eye, more than 30 m out from its muzzle, cracks there now and then.
			if (!bOwnGuns && Eye)
			{
				const FVector Aim = World(E.Aim.X, E.Aim.Y, E.Aim.Z);
				const FVector Line = Aim - At;
				const double Long = Line.Size();
				if (Long > 1.0)
				{
					// (the rounds fly on past the guns' convergence, where the line is aimed, to air.roundsPassing.reach)
					const double Flies = FMath::Max(Long, AirParts::MixNumber(Mix, TEXT("air.roundsPassing.reach"), 1000.0) * U);
					const double Along = FMath::Clamp(FVector::DotProduct(Listener - At, Line / Long), 0.0, Flies);
					const FVector Nearest = At + Line / Long * Along;
					const double Within = AirParts::MixNumber(Mix, TEXT("air.roundsPassing.within"), 40.0) * U;
					// (by the aircraft, not the barrel: its converging guns' lines pass together, and crack as one)
					const int32 Shooter = E.Key / 64;
					const double* Cracked = CrackHeardAt.Find(Shooter);
					if (Along > 30.0 * U && FVector::Dist(Nearest, Listener) < Within && (!Cracked || RealT >= *Cracked))
					{
						CrackHeardAt.Add(Shooter, RealT + FMath::FRandRange(0.12, 0.3));
						const FString Crack = AirParts::MixTake(AirParts::MixObject(Mix, TEXT("air.roundsPassing")), TEXT("takes"), FMath::RandRange(0, 9));
						const FName CrackTake(Crack.IsEmpty() ? TEXT("bullet_crack") : *Crack);
						// (the same take never again inside air.sameTakeGap, whoever fires it: closer, two of them comb)
						const double* Began = TakeStartedAt.Find(CrackTake);
						if (!Began || RealT - *Began >= AirParts::MixNumber(Mix, TEXT("air.sameTakeGap"), 0.15))
						{
							if (Sound(*CrackTake.ToString(), Nearest, TEXT("air_guns"), 1.0f, 0.0f, 0.0f, 0.0f, 3.0f))
							{
								TakeStartedAt.Add(CrackTake, RealT);
								++CracksHeard;
							}
						}
					}
				}
			}
			break;
		}
		case AirShow::FxKind::GunStop:
		{
			FBattleEffects* Kit = nullptr;
			if (GunKits.RemoveAndCopyValue(E.Key, Kit) && Kit)
			{
				Kit->AirGunStop(E.Key);
			}
			GunHeardAt.Remove(E.Key);
			// (a barrel of a wing, or a gun of a gunner's mount: the take goes on while any of its guns still fires, and
			// stops with the last)
			int32 StopKey = E.Key;
			int32 Wing = 0;
			if (WingOf.RemoveAndCopyValue(E.Key, Wing))
			{
				int32& Live = WingLive.FindOrAdd(Wing);
				Live = FMath::Max(0, Live - 1);
				if (Live > 0)
				{
					break;
				}
				WingLive.Remove(Wing);
				GunHeardAt.Remove(Wing);
				StopKey = Wing;
			}
			// The burst heard stops with the gun (Brian 11-20 14:20Z: "after the bomber crew bails out, they can't keep
			// shooting"): a take still playing is faded out at once, not left to run to its end after the trigger is up.
			TArray<TWeakObjectPtr<UAudioComponent>> Playing;
			GunTakes.MultiFind(StopKey, Playing);
			for (const TWeakObjectPtr<UAudioComponent>& Take : Playing)
			{
				if (Take.IsValid())
				{
					Take->FadeOut(0.08f, 0.0f);
				}
			}
			GunTakes.Remove(StopKey);
			break;
		}
		case AirShow::FxKind::Hit:
		{
			MarkStrike(E); // (kept on the aircraft it struck, and laid on its skin while it is near: AirBattleMarks.cpp)
			if (FBattleEffects* Kit = KitAt(At))
			{
				Kit->AirHit(Now, At, WorldVector(E.Normal.X, E.Normal.Y, E.Normal.Z).GetSafeNormal(), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), AirParts::Wide(E.What));
			}
			// Rounds striking a plane, heard near the eye (mix.json air.roundsHit: within `within` metres, no closer together
			// than `every`). A strike on the ridden aircraft is heard at the eye, unplaced, as its own guns are. Until the
			// mix names its takes (round seven's hits, when Brian has marked them) this is silent.
			const TSharedPtr<FJsonObject> Hits = AirParts::MixObject(Mix, TEXT("air.roundsHit"));
			if (Hits.IsValid() && (HitHeardAt < 0.0 || RealT - HitHeardAt >= AirParts::MixNumber(Mix, TEXT("air.roundsHit.every"), 0.08))
			    && FVector::Dist(At, Listener) < AirParts::MixNumber(Mix, TEXT("air.roundsHit.within"), 60.0) * U)
			{
				const FString Take = AirParts::MixTake(Hits, TEXT("takes"), FMath::RandRange(0, 11));
				if (!Take.IsEmpty())
				{
					HitHeardAt = RealT;
					bool bOwn = false;
					if (S.bClose)
					{
						for (const AirShow::PlaneOut& P : S.Show.Planes())
						{
							if (P.Key == S.Subject.Plane)
							{
								bOwn = FVector::Dist(At, World(P.At.X, P.At.Y, P.At.Z)) < P.Span * 0.75 * U;
								break;
							}
						}
					}
					if (bOwn)
					{
						SoundAtEye(*Take, 1.0f, 0.0f, false, 4.0f);
					}
					else
					{
						Sound(*Take, At, TEXT("air_guns"), 1.0f, 0.0f, 0.0f, 0.0f, 3.0f);
					}
				}
			}
			break;
		}
		case AirShow::FxKind::TrailStart:
		{
			// (the scene's room for trails, S.Trails: 24 on the newer kit, 10 on an older; a burning aircraft's own
			// trail first, taking the place of the oldest piece's when the room is full)
			// (in the kit for its height where it starts: over the clouds' base the air's, under it the ground's)
			int PutOut = 0;
			FBattleEffects* const Kit = KitAt(At);
			if (Kit && S.Trails.Start(E.Key, E.Text == "piece" || E.What == "streak", PutOut))
			{
				if (PutOut != 0)
				{
					FTrailOn Out;
					if (TrailKits.RemoveAndCopyValue(PutOut, Out) && Out.Kit)
					{
						Out.Kit->TrailStop(PutOut);
					}
				}
				FTrailOn On;
				On.Kit = Kit;
				On.Kind = AirParts::Wide(E.What);
				On.Span = static_cast<float>(E.Size);
				On.KindT = Now;
				if (const FTrailOn* Was = TrailKits.Find(E.Key))
				{
					// (lit again while it burns: its kind's own start kept, the same kind going on at its full look; in
					// the kit it is already in, with the base's 50 m either side)
					On.Kit = KitAt(At, Was->Kit);
					if (Was->Kind == On.Kind)
					{
						On.KindT = Was->KindT;
					}
					if (Was->Kit && Was->Kit != On.Kit)
					{
						Was->Kit->TrailStop(E.Key);
					}
				}
				TrailKits.Add(E.Key, On);
				AirParts::TrailStart(On.Kit, On.KindT, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), On.Kind, On.Span);
				if (On.KindT < Now)
				{
					On.Kit->TrailMove(Now, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z));
				}
			}
			break;
		}
		case AirShow::FxKind::TrailMove:
			if (S.Trails.On(E.Key))
			{
				if (FTrailOn* On = TrailKits.Find(E.Key))
				{
					const FVector Velocity = WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z);
					FBattleEffects* const Kit = KitAt(At, On->Kit);
					if (Kit && Kit != On->Kit)
					{
						// (passing the clouds' base: it goes on in the other kit, and what it has left in the air fades
						// where it is; started there as of when its kind began, so it carries on at its full look and
						// does not light again, and laid on from here)
						if (On->Kit)
						{
							On->Kit->TrailStop(E.Key);
						}
						On->Kit = Kit;
						AirParts::TrailStart(Kit, On->KindT, E.Key, At, Velocity, On->Kind, On->Span);
						Kit->TrailMove(Now, E.Key, At, Velocity);
					}
					else if (On->Kit)
					{
						On->Kit->TrailMove(Now, E.Key, At, Velocity);
					}
				}
			}
			break;
		case AirShow::FxKind::TrailStop:
		{
			FTrailOn On;
			const bool bKept = TrailKits.RemoveAndCopyValue(E.Key, On);
			if (S.Trails.Stop(E.Key) && bKept && On.Kit)
			{
				On.Kit->TrailStop(E.Key);
			}
			break;
		}
		case AirShow::FxKind::ShipGun:
		{
			// A ship's heavy gun fired (ShipGuns: its shell's burst, or its splash ahead of a torpedo plane, follows when
			// it gets there). The flash and smoke along the bore at her deck (muzzle.ship_aa, a main battery's gun
			// muzzle.ship_main: no dust ring, as the guns stand over the sea), and the boom: the mix's flak firing take,
			// after the flash by the distance (none within 150 m, at most 3 s), no closer together than its soundEvery.
			if (FBattleEffects* Kit = GroundKit())
			{
				const FVector Bore = (World(E.Aim.X, E.Aim.Y, E.Aim.Z) - At).GetSafeNormal();
				Kit->Muzzle(At, Bore.IsNearlyZero() ? FVector::UpVector : Bore, E.Size >= 200.0 ? TEXT("ship_main") : TEXT("ship_aa"), TEXT("water"));
			}
			const TSharedPtr<FJsonObject> Flak = AirParts::MixObject(Mix, TEXT("air.flak"));
			const double Every = AirParts::MixNumber(Mix, TEXT("air.flak.soundEvery"), 0.6);
			if (RealT - ShipGunHeardAt >= Every)
			{
				ShipGunHeardAt = RealT;
				const double Metres = FVector::Dist(At, Listener) / U;
				const float After = Metres < 150.0 ? 0.0f : static_cast<float>(FMath::Min(3.0, Metres / 343.0));
				const FString Boom = AirParts::MixTake(Flak, TEXT("fire"), 0);
				FString Family;
				if (!Flak.IsValid() || !Flak->TryGetStringField(TEXT("fireFamily"), Family) || Family.IsEmpty())
				{
					Family = TEXT("big_guns");
				}
				Sound(Boom.IsEmpty() ? TEXT("flak_heavy_fire_01") : *Boom, At, *Family, 1.0f, After);
			}
			break;
		}
		case AirShow::FxKind::Flak:
		{
			if (FBattleEffects* Kit = KitAt(At))
			{
				Kit->AirFlak(Now, At, AirParts::Wide(E.What));
			}
			// Flak heard when it is seen (handoff 10, gap 6): the burst at its point, after the flash by the distance
			// over the speed of sound (none within 150 m, at most 3 s), no closer together than the mix's soundEvery.
			const double Every = AirParts::MixNumber(Mix, TEXT("air.flak.soundEvery"), 0.6);
			const double Metres = FVector::Dist(At, Listener) / U;
			// (a burst close to the eye, mix.json air.closeView.closeFlak: a real close blast in place of the far burst,
			// whatever the view; within 20 m a big moment by big.alsoWhenCloserThan. It is always heard: soundEvery keeps
			// only the bursts farther off from crowding, never the one beside the eye)
			const TSharedPtr<FJsonObject> Close = AirParts::MixObject(Mix, TEXT("air.closeView.closeFlak"));
			FString CloseTake;
			const bool bCloseBurst = Close.IsValid() && Metres < AirParts::MixNumber(Mix, TEXT("air.closeView.closeFlak.within"), 100.0)
				&& Close->TryGetStringField(TEXT("take"), CloseTake) && SoundOf(*CloseTake);
			if (bCloseBurst || RealT - FlakHeardAt >= Every)
			{
				FlakHeardAt = RealT;
				const float After = Metres < 150.0 ? 0.0f : static_cast<float>(FMath::Min(3.0, Metres / 343.0));
				const FString Burst = bCloseBurst ? CloseTake : AirParts::MixTake(AirParts::MixObject(Mix, TEXT("air.flak")), TEXT("burst"), static_cast<int32>(RealT * 7.0));
				Sound(Burst.IsEmpty() ? TEXT("flak_burst_01") : *Burst, At, TEXT("explosions"), 1.0f, After);
			}
			break;
		}
		case AirShow::FxKind::Fall:
			// The debris row (the art director, 04:40Z; the effects kit's AirFall): blown apart, every piece at once
			// round the kit's short torn fireball; burning or spinning, pieces shed over the fall. Only while the
			// pieces already falling leave the kit room (S.Debris): past that the blast below keeps the older flame.
			// (in the kit for the height where the fall begins, as its trail)
			// A pilot hit or a glide down goes down whole: no pieces shed (the air thread's ways down, Text the way).
			if (E.Text == "pilot" || E.Text == "glide")
			{
				break;
			}
			if (FBattleEffects* const Kit = KitAt(At))
			{
				if (AirParts::HasAirFall(Kit) && S.Debris.Take(Now, E))
				{
					AirParts::AirFall(Kit, Now, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), AirParts::Wide(E.What), static_cast<float>(E.Size), World(0.0, 0.0, E.Deck).Z);
					if (E.What == "explode")
					{
						BlastsTaken.Add(E.Key);
					}
					else
					{
						FallsOn.Add(E.Key, Kit);
					}
				}
			}
			break;
		case AirShow::FxKind::FallMove:
			if (FBattleEffects* const* Kit = FallsOn.Find(E.Key))
			{
				if (E.Deck < 0.0)
				{
					// (down a hole and under the deck: the row, told the deck, sheds no more)
					AirParts::AirFallStop(*Kit, E.Key);
					FallsOn.Remove(E.Key);
				}
				else
				{
					AirParts::AirFallMove(*Kit, Now, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z));
				}
			}
			break;
		case AirShow::FxKind::FallStop:
		{
			FBattleEffects* Kit = nullptr;
			if (FallsOn.RemoveAndCopyValue(E.Key, Kit) && Kit)
			{
				AirParts::AirFallStop(Kit, E.Key);
			}
			break;
		}
		case AirShow::FxKind::Blast:
		{
			// Blown apart (the art director, 03:14Z): a short torn flame streaming on with the wreck, never a ball, the
			// sparks and scraps of the hits; the pieces are the flight core's. Where the kit's AirFall took it, its
			// torn fireball and debris row are the blast, and this older flame is not laid over them.
			if (BlastsTaken.Remove(E.Key) > 0)
			{
				break;
			}
			if (FBattleEffects* const Kit = KitAt(At))
			{
				const FVector Velocity = WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z);
				for (int32 K = 0; K < 4; ++K)
				{
					Kit->AirHit(Now, At + FMath::VRand() * E.Size * 0.2 * U, FMath::VRand(), Velocity, TEXT("cannon20"));
				}
				FTorn& Flame = Torn.AddDefaulted_GetRef();
				Flame.Key = 1 << 29 | (TornNext++ & 0xFFFF);
				Flame.At = At;
				Flame.Velocity = Velocity * 0.6;
				Flame.Until = RealT + 0.9;
				Flame.Kit = Kit;
				AirParts::TrailStart(Kit, Now, Flame.Key, At, Flame.Velocity, TEXT("fire"), static_cast<float>(E.Size));
			}
			break;
		}
		case AirShow::FxKind::Splash:
			if (FBattleEffects* Kit = GroundKit())
			{
				// (a ditching, a glide on to the sea under control: a skip and spray, never a bomb's column)
				Kit->ShellOfSize(At, E.What == "ditch" ? TEXT("light") : E.Size >= 8.0 ? TEXT("bomb250") : E.Size >= 3.0 ? TEXT("light") : TEXT("grenade"), TEXT("water"));
			}
			break;
		case AirShow::FxKind::Crash:
			if (FBattleEffects* Kit = GroundKit())
			{
				// (a belly landing: dust thrown up, no fire)
				const bool bBelly = E.What == "belly";
				Kit->ShellOfSize(At, !bBelly && E.Size >= 8.0 ? TEXT("bomb250") : TEXT("grenade"), TEXT("earth"));
				// (an aircraft flown into the ground, not a piece of one nor a belly landing: its fuel goes up, a rolling
				// ball 15 to 25 m across by its size, a fighter's the least and a heavy's the most, then the fire burns on.
				// The effects kit's FuelBall, the art director's head D; its velocity when the flight core gives one)
				if (E.What == "plane")
				{
					const double Across = FMath::Clamp(E.Size * 0.8, 15.0, 25.0);
					Kit->FuelBall(At, static_cast<float>(Across), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z));
					NoteFuelBall(At, Across);
				}
				if (!bBelly && E.Size >= 8.0)
				{
					Kit->Burn(At, static_cast<float>(E.Size * 0.6), 90.0f);
				}
			}
			break;
		case AirShow::FxKind::BombHit:
			if (FBattleEffects* Kit = GroundKit())
			{
				AirParts::BombBurst(Kit, Now, 1 << 28 | (BombNext++ & 0xFFFFF), At, E.Size >= 500.0 ? TEXT("bomb500") : TEXT("bomb250"), E.What == "water" ? TEXT("water") : TEXT("earth"));
			}
			if (E.What != "water")
			{
				ScarLand(E.At.X, E.At.Y, E.Size > 0.0 ? E.Size : 250.0);
				// (on or beside an oil tank of the stage: it goes up, once, its ball kept for rule 13)
				if (Stage && Stage->IsRaised())
				{
					TArray<TPair<FVector, float>> Balls;
					if (Stage->BombOnOil(At, Balls) > 0)
					{
						for (const TPair<FVector, float>& Ball : Balls)
						{
							NoteFuelBall(Ball.Key, Ball.Value);
						}
					}
				}
			}
			{
				// Heard where it is seen (the mix's impacts; Brian 21 November 05:45Z, "you see and hear drop to the
				// ground"): a stick's first bomb plays bomb_stick_01 from its first blast (0.85 s into the take), so that
				// the take's blasts walk on as the stick's bombs land, one stick take at a time unless a nearer stick
				// lands; a bomb alone, and any bomb within 150 m, a crump of its own taken by the distance (impacts near,
				// middle, far). Each after its flash by the distance over the speed of sound (none within 150 m, at most 3 s).
				const double Metres = FVector::Dist(At, Listener) / U;
				const float After = Metres < 150.0 ? 0.0f : static_cast<float>(FMath::Min(3.0, Metres / 343.0));
				if (E.Text == "stick" && (RealT - StickHeardAt >= 1.5 || Metres < StickHeardMetres * 0.5))
				{
					StickHeardAt = RealT;
					StickHeardMetres = Metres;
					Sound(TEXT("bomb_stick_01"), At, TEXT("explosions"), 1.0f, After, 0.0f, 0.85f);
				}
				else if ((E.Text == "one" || Metres < 150.0) && RealT - CrumpHeardAt >= 0.25)
				{
					CrumpHeardAt = RealT;
					const TCHAR* Band = Metres < 60.0 ? TEXT("impacts.near") : Metres < 150.0 ? TEXT("impacts.middle") : TEXT("impacts.far");
					const FString Take = AirParts::MixTake(AirParts::MixObject(Mix, Band), TEXT("takes"), static_cast<int32>(RealT * 7.0) + E.Key);
					Sound(Take.IsEmpty() ? TEXT("explosion_far_01") : *Take, At, TEXT("explosions"), 1.0f, After);
				}
			}
			break;
		case AirShow::FxKind::Sound:
		{
			// The event's cue at its point (handoff section 3), a far take for a blast past the explosions' farTakesFrom.
			FString Name = AirParts::Wide(E.What);
			FName Family = TEXT("aircraft");
			// (a falling bomb's rush near the eye, the mix's impacts.incoming.bomb, timed by the flight core to end at
			// its landing: only at the battle's own pace, for at x10 the bomb is down long before the take is out. The
			// clock now holds the player's pace while the strike is on, before the formations meet and on the way home
			// too, so it is muted only when the last step ran fast)
			if (Name == TEXT("bomb_release") && (S.Clock.Speed != 1.0 || S.Clock.Fast() || S.bFlying))
			{
				break;
			}
			if (Name == TEXT("plane_explode") || Name == TEXT("plane_down") || Name == TEXT("bomb_stick_01") || Name == TEXT("bomb_release"))
			{
				Family = TEXT("explosions");
				const double From = AirParts::MixNumber(Mix, TEXT("distance.explosions.farTakesFrom"), 150.0);
				if (Name == TEXT("plane_explode") && FVector::Dist(At, Listener) / U > From)
				{
					Name = (E.Key + static_cast<int32>(RealT)) % 2 ? TEXT("explosion_far_01") : TEXT("explosion_far_02");
				}
			}
			else if (Name == TEXT("parachute_open"))
			{
				Family = TEXT("body");
			}
			Sound(*Name, At, *Family.ToString());
			break;
		}
		case AirShow::FxKind::Dispatch:
			S.Desk.Say(E.What, E.Text, E.Side, E.Key);
			break;
		case AirShow::FxKind::ShipHit:
			if (Stage)
			{
				Stage->Hit(E.Key, AirParts::Wide(E.What), Now);
			}
			break;
		case AirShow::FxKind::ShipSunk:
			if (Stage)
			{
				Stage->Sink(E.Key, AirParts::Wide(E.What), E.Size >= 1.0, Now);
			}
			if (ShipRealT < 0.0)
			{
				ShipRealT = RealT;
				FirstSunk = E.Key;
			}
			break;
		case AirShow::FxKind::Smoke:
			if (FBattleEffects* Kit = GroundKit())
			{
				Kit->Burn(At, 40.0f, 900.0f);
			}
			break;
		}
	}
	S.Show.Effects.clear();
}

void AAirBattle::AimEye(double Real)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_AimEye);
	if (!Eye)
	{
		return;
	}
	FAirBattleState& S = *State;
	const double U = UnitsPerMetre;
	UCameraComponent* Lens = Eye->GetCameraComponent();
	// The flyover before the battle: its shots, each eye kept 60 m over the ground; then the glide (below, after the
	// strategy camera has found its place).
	if (S.bFlying && S.GlideT < 0.0)
	{
		S.FlyT += Real;
		if (S.FlyT < S.Fly.Length())
		{
			const AirDesk::Flyover::Shot Shot = S.Fly.At(S.FlyT, S.Show.Groups(), S.Show.Planes());
			FVector FlyEye = World(Shot.Eye.X, Shot.Eye.Y, Shot.Eye.Z);
			const FVector FlyLook = World(Shot.Look.X, Shot.Look.Y, Shot.Look.Z);
			if (Stage && Stage->IsRaised())
			{
				FlyEye.Z = FMath::Max(FlyEye.Z, Stage->GroundZ(FlyEye) + 60.0 * U);
			}
			FlyEye.Z = FMath::Max(FlyEye.Z, 40.0 * U);
			Lens->SetFieldOfView(static_cast<float>(AirDesk::Flyover::Lens));
			Lens->PostProcessSettings.MotionBlurAmount = 0.0f;
			S.GlideEye = FlyEye;
			S.GlideFacing = (FlyLook - FlyEye).Rotation();
			Eye->SetActorLocationAndRotation(S.GlideEye, S.GlideFacing);
			return;
		}
		S.GlideT = 0.0;
		S.GlideLong = 2.5;
		bLookSet = false;
	}
	// The close view (handoff 10, gap 1): it rides a plane of the group in hand, or with none in hand the busiest fighter
	// near the middle of the picture; a fall, a straggle or a limp cuts to the next, never a swoop. (Not while the
	// flyover's glide runs, which is at this function's end: a ride asked for during the flyover, a C pressed while the
	// battle loaded or a ride picture come due, starts when the glide is done. The cloud pre-check's catch, 17:05Z.)
	if (S.bClose && !S.bFlying)
	{
		const std::vector<AirShow::PlaneOut>& Out = S.Show.Planes();
		auto Find = [&Out](int Key) -> const AirShow::PlaneOut*
		{
			for (const AirShow::PlaneOut& P : Out)
			{
				if (P.Key == Key)
				{
					return &P;
				}
			}
			return nullptr;
		};
		const AirShow::PlaneOut* Rider = Find(S.Subject.Plane);
		S.Subject.Since += Real * S.Board.Scale;
		const bool bLost = !Rider || !Rider->bShown || Rider->bFalling || Rider->bOnGround;
		// (a squadron he chose to ride is kept to: the picture changes only at his step, or as its aircraft go down)
		const bool bStale = S.Hand.empty() && S.RideGroup < 0 && S.Subject.Since > 10.0;
		// (the camera rules' 13: the eye within twice a fuel ball's width in its first 2 s is cut on its flash, to an
		// aircraft whose eye stands clear of every such ball: a fighter's eye is 23 m from it, a heavy's two spans)
		const bool bBall = !CloseEye.IsZero() && BallsNear.Num() > 0 && IntoFuelBall(CloseEye) > 0.0;
		auto RideClear = [this](const AirShow::PlaneOut& P)
		{
			return BallsNear.Num() == 0 || IntoFuelBall(World(P.At.X, P.At.Y, P.At.Z), (P.bFighter ? 23.0 : 2.0 * FMath::Max(12.0, P.Span)) * UnitsPerMetre) <= 0.0;
		};
		if (bBall)
		{
			++BallCuts;
			UE_LOG(LogTheater, Display, TEXT("Air battle: the close view cut on a fuel ball's flash, %.0f m inside its reach (rule 13, cut %d)."), IntoFuelBall(CloseEye) / U, BallCuts);
		}
		if (bLost || bStale || bBall)
		{
			AirDesk::Subject Next;
			// The squadron he rides (the chase view), else the group in hand: the next of its aircraft along its line
			// from the one lost, so that a fall steps on to its wingman, not back to its leader.
			const int Squadron = S.RideGroup >= 0 ? S.RideGroup : !S.Hand.empty() ? S.Hand.front() : -1;
			if (Squadron >= 0)
			{
				const int Key = AirDesk::RideStep(Out, Squadron, S.Subject.Plane, 1);
				const AirShow::PlaneOut* Then = Key >= 0 ? Find(Key) : nullptr;
				if (Key >= 0 && Key != S.Subject.Plane && (!Then || RideClear(*Then)))
				{
					Next.Plane = Key;
					Next.Other = Then ? Then->Target : -1;
				}
				else if (Key < 0)
				{
					S.RideGroup = -1; // (none of the squadron left up: the view picks the fight again)
				}
			}
			if (Next.Plane < 0)
			{
				// (his own side's aircraft first: the picture rides with his men; the other side's when his have none up,
				// or when a picture run's ride asks for theirs)
				std::vector<AirShow::PlaneOut> Ours;
				for (const AirShow::PlaneOut& P : Out)
				{
					if ((P.Side == S.Desk.MySideIndex()) != bRideOther && RideClear(P))
					{
						Ours.push_back(P);
					}
				}
				// (a fighter closing on what it fights, so that is in the picture too; on a bomber, one coming at it
				// head-on most of all: Brian on the Bremen film, "where are the german fighters?")
				Next = AirDesk::ChooseRide(Ours, Out, S.Show.Eye, bLost ? -1 : S.Subject.Plane);
				if (Next.Plane < 0 && !bLost && (Rider->Side == S.Desk.MySideIndex()) != bRideOther)
				{
					S.Subject.Since = 0.0; // (his only aircraft up: the picture stays with it)
				}
				else if (Next.Plane < 0)
				{
					Next = AirDesk::Choose(Out, std::vector<int>(), S.Show.Eye, bLost ? -1 : S.Subject.Plane);
				}
			}
			if (Next.Plane >= 0)
			{
				S.Subject = Next;
				Rider = Find(Next.Plane);
				CloseEye = FVector::ZeroVector; // (a cut: no swoop from the last)
			}
		}
		if (Rider)
		{
			// (the art director's rules from the first Mac run: the ridden aircraft always in the picture and seen from
			// behind, the horizon never rolled past 15 degrees, little blur)
			const FQuat Attitude = FRotator(Rider->Pitch, Rider->Heading, Rider->Bank).Quaternion();
			const FVector Ahead = Attitude.GetForwardVector();
			const FVector Body = World(Rider->At.X, Rider->At.Y, Rider->At.Z);
			const AirShow::PlaneOut* Foe = Find(Rider->Target >= 0 ? Rider->Target : S.Subject.Other);
			// The way the eye trails: the aircraft's path, eased over a third of a second so that a jink or a snap roll
			// does not whip the picture round. The eye is set at its place from the aircraft each frame (never eased
			// toward it), so a fast aircraft does not draw away from it, nor a turning one swing past it.
			const bool bCut = CloseEye.IsZero();
			if (bCut)
			{
				// (a cut from one aircraft to the next can be shorter than the sky's cut, SkyCut: in a formation, 30 m)
				AirParts::TellCut(GetWorld());
			}
			CloseAhead = bCut || CloseAhead.IsNearlyZero() ? Ahead : (CloseAhead + (Ahead - CloseAhead) * AirParts::Smooth(Real, 0.33)).GetSafeNormal();
			FVector Flat(CloseAhead.X, CloseAhead.Y, 0.0);
			Flat = Flat.IsNearlyZero() ? FVector(Ahead.X, Ahead.Y, 0.0).GetSafeNormal() : Flat.GetSafeNormal();
			const FVector FlatRight = FVector::CrossProduct(FVector::UpVector, Flat).GetSafeNormal();
			FVector Want, Look, AceMarks, AceOut;
			if (Rider->Key == AceEyeKey && AceMarksAt(Rider->Key, AceMarks, AceOut))
			{
				// The ace's close picture (the art director, 18:31Z 22 November: "one close frame"): 8 m off his record
				// (his name and first row of marks, or a German's name and rudder bars), square to it, two metres above
				// and a metre behind, looking at it: some 20 px a mark on the Mac's screen. (A little above, as the camera
				// rules frame him: the modeller's check, 19:45Z, found a mid-set wing (the Wildcat's) or a resting
				// propeller (the Night Fighter's) hides a few slots only from dead level.)
				Want = AceMarks + AceOut * 8.0 * U + FVector::UpVector * 2.0 * U - CloseAhead * 1.0 * U;
				Look = AceMarks;
			}
			else if (Rider->bFighter)
			{
				// A fighter from 22 m behind along its path and 6 m above it, looking 80 m past it: the aircraft a little
				// under the middle of the picture, its back, its fin and the tops of its wings toward the eye.
				Want = Body - CloseAhead * 22.0 * U + FVector::UpVector * 6.0 * U;
				Look = Body + CloseAhead * 80.0 * U;
			}
			else
			{
				// A bomber from behind its tail and above it, a little out on the side the attack comes from, looking
				// a little past its nose: the whole bomber in the lower middle of the picture, both wingtips inside it,
				// the sky its attackers come through beyond it.
				const FVector Toward = Foe ? (World(Foe->At.X, Foe->At.Y, Foe->At.Z) - Body).GetSafeNormal() : FlatRight;
				double Hand = FVector::DotProduct(Toward, FlatRight) >= 0.0 ? 1.0 : -1.0;
				// (the strike-marks picture's eye on the side its strikes are on, and under it when they are mostly
				// underneath: from behind on the other side they were hidden, the Mac's r2f run)
				bool bUnder = false;
				if (Rider->Key == MarksEyeKey)
				{
					MarksSide(Rider->Key, Hand, bUnder);
				}
				// (the dead-engine picture's on the side of the dead engine)
				if (Rider->Key == ScrewEyeKey)
				{
					Hand = ScrewEyeHand;
				}
				const double Size = FMath::Max(12.0, Rider->Span);
				Want = Body - CloseAhead * Size * 1.8 * U + FlatRight * Hand * Size * 0.55 * U + FVector::UpVector * Size * (bUnder ? -0.3 : 0.45) * U;
				Look = Body + CloseAhead * Size * 1.2 * U;
			}
			// (rule 13 with no aircraft clear to cut to: the eye stands off the ball, along the line from it to where the
			// eye would be, at its reach and a little more, until the ball's 2 s are out: a cut in place, and one back)
			bool bOffBall = false;
			for (const FFuelBallNear& Ball : BallsNear)
			{
				const FVector Off = Want - Ball.At;
				if (Off.Size() < Ball.Reach)
				{
					Want = Ball.At + (Off.IsNearlyZero() ? -CloseAhead : Off.GetSafeNormal()) * (Ball.Reach * 1.05);
					bOffBall = true;
				}
			}
			const bool bJumped = bOffBall != bCloseOffBall;
			bCloseOffBall = bOffBall;
			if (bJumped && !bCut)
			{
				AirParts::TellCut(GetWorld());
			}
			CloseEye = Want;
			FRotator Facing = (Look - CloseEye).Rotation();
			Facing.Roll = FMath::Clamp(Rider->Bank * 0.25, -15.0, 15.0);
			CloseFacing = bCut || bJumped ? Facing : FMath::RInterpTo(CloseFacing, Facing, static_cast<float>(Real), 6.0f);
			Eye->SetActorLocationAndRotation(CloseEye, CloseFacing);
			Lens->SetFieldOfView(62.0f);
			Lens->PostProcessSettings.bOverride_MotionBlurAmount = true;
			Lens->PostProcessSettings.MotionBlurAmount = 0.12f;
			return;
		}
		S.bClose = false;
	}
	Lens->SetFieldOfView(55.0f);
	Lens->PostProcessSettings.MotionBlurAmount = 0.0f;
	// The strategy camera: on the group in hand, else the fight, else his own raid; turned by dragging, drawn in and out by the wheel.
	AirShow::Vec Focus(S.Board.TargetX, S.Board.TargetY, 3000.0);
	int Count = 0;
	AirShow::Vec Sum;
	std::vector<int> Picked;
	for (const auto& Pair : S.Show.Groups())
	{
		const AirShow::Group& G = Pair.second;
		const bool bInHand = std::find(S.Hand.begin(), S.Hand.end(), G.Id) != S.Hand.end();
		if (G.Doing == AirShow::Act::Down || G.Doing == AirShow::Act::Idle)
		{
			continue;
		}
		if (!S.Hand.empty() ? bInHand : G.Doing == AirShow::Act::Fight)
		{
			Sum = Sum + AirShow::Vec(G.X, G.Y, G.Alt);
			Picked.push_back(G.Id);
			++Count;
		}
	}
	if (Count == 0)
	{
		for (const auto& Pair : S.Show.Groups())
		{
			const AirShow::Group& G = Pair.second;
			if (G.Side == S.Desk.MySideIndex() && !G.bFighter && G.Doing != AirShow::Act::Down)
			{
				Sum = Sum + AirShow::Vec(G.X, G.Y, G.Alt);
				Picked.push_back(G.Id);
				++Count;
			}
		}
	}
	// (on the thickest crowd of their aircraft, not the middle of their groups, which kilometres apart was empty sky)
	AirShow::Vec Crowd;
	// (and framed by the kind of aircraft in it, at the player's own zoom (Distance over its first 800 m): its middle
	// aircraft about 70 pixels across at 1920 for heavies, 45 for the rest, through the 55 degree lens; the look on its
	// aircraft, not on empty sky between them: on the middle of those within 350 m of the crowd's middle, else on the
	// one nearest it. The art director on a1acb552's frames, 22 November 08:13Z: Bremen's B-17s 145 to 290 pixels,
	// Pearl 01's and Midway 04's 10 to 25 pixel specks off the middle of the picture)
	double CrowdSpan = 0.0;
	if (AirDesk::CrowdOf(S.Show.Planes(), Picked, 1200.0, FocusAnchor, Crowd))
	{
		Focus = Crowd;
		std::vector<double> Spans;
		const AirShow::PlaneOut* Nearest = nullptr;
		double NearestFar2 = 1e18;
		AirShow::Vec Tight;
		int TightCount = 0;
		for (const AirShow::PlaneOut& P : S.Show.Planes())
		{
			if (!P.bShown || P.bFalling || P.bOnGround || std::find(Picked.begin(), Picked.end(), P.Group) == Picked.end())
			{
				continue;
			}
			const AirShow::Vec Off = P.At - Crowd;
			const double Far2 = Off.Dot(Off);
			if (Far2 > 1200.0 * 1200.0)
			{
				continue;
			}
			Spans.push_back(P.Span > 1.0 ? P.Span : 12.0);
			if (Far2 < NearestFar2)
			{
				NearestFar2 = Far2;
				Nearest = &P;
			}
			if (Far2 < 350.0 * 350.0)
			{
				Tight = Tight + P.At;
				++TightCount;
			}
		}
		if (!Spans.empty())
		{
			std::nth_element(Spans.begin(), Spans.begin() + Spans.size() / 2, Spans.end());
			CrowdSpan = Spans[Spans.size() / 2];
		}
		if (TightCount >= 2)
		{
			Focus = Tight * (1.0 / TightCount);
		}
		else if (Nearest)
		{
			Focus = Nearest->At;
		}
	}
	else if (Count > 0)
	{
		Focus = Sum * (1.0 / Count);
	}
	FVector Wanted = World(Focus.X, Focus.Y, Focus.Z);
	const double TypeAway = CrowdSpan > 0.0 ? FMath::Clamp(CrowdSpan * 1920.0 / ((CrowdSpan >= 25.0 ? 70.0 : 45.0) * 2.0 * FMath::Tan(FMath::DegreesToRadians(27.5))), 250.0, 1500.0) : 800.0;
	double Away = Distance * TypeAway / 800.0, Down = Pitch, Heading = Yaw;
	StrategyAway = Away;
	StrategySpan = CrowdSpan;
	StrategyGroups = static_cast<int32>(Picked.size());
	// "The ships" picture: the overview cut to the ship that went down, from 1.6 km off and above, her neighbours and
	// the water round her in the frame (the art director's note: the first run framed the hills, no ship in sight).
	FVector ShipWorld;
	const bool bOnShip = ShipsLook >= 0 && Stage && Stage->IsRaised() && Stage->ShipAt(ShipsLook, ShipWorld);
	if (bOnShip)
	{
		Wanted = ShipWorld;
		Away = 1600.0;
		Down = -24.0;
		// At sea (the art director on build 1734, Midway 06: the carrier a small soft shape in a teal field): she fills
		// about a third of the picture's width, through a longer lens (35 degrees) from low enough that her smoke stands
		// up above her; the look a little over her waterline, so that she sits under the middle with her column over her.
		// She is seen from upwind (the art director on build 1760: from downwind her column leaned at the eye, its
		// nearest puffs huge and soft): the look 55 degrees off the wind's way, so her smoke streams away and across the
		// picture, of the two such eyes the one nearer her beam (she shows more of her length), and on a near tie her
		// sunlit side. In harbour the wide picture stands (Pearl Harbor 06: its columns across the harbour are what it is
		// for), its eye upwind and clear of them (below).
		double Long = 0.0;
		FVector Bow;
		bool bHarbour = true;
		if (Stage->ShipLie(ShipsLook, Long, Bow, bHarbour) && !bHarbour)
		{
			const double LensDegrees = 35.0;
			const FVector Abeam = FVector::CrossProduct(FVector::UpVector, Bow).GetSafeNormal();
			FVector Sun = Stage->TowardSun();
			Sun.Z = 0.0;
			Sun = Sun.GetSafeNormal();
			FVector Downwind = WorldVector(AirSky::Wind.X, AirSky::Wind.Y, 0.0).GetSafeNormal();
			if (Downwind.IsNearlyZero())
			{
				Downwind = -Abeam;
			}
			FVector Out = Abeam;
			double Best = -1e9;
			for (const double Turn : { 55.0, -55.0 })
			{
				// (the eye's way from her: upwind, turned Turn degrees, so the look runs that far off the wind's way)
				const FVector Way = (-Downwind).RotateAngleAxis(Turn, FVector::UpVector);
				const double Score = FMath::Abs(FVector::DotProduct(Way, Abeam)) + 0.25 * FMath::Max(0.0, FVector::DotProduct(Way, Sun));
				if (Score > Best)
				{
					Best = Score;
					Out = Way;
				}
			}
			// (seen at an angle off her length she shows that much of it, never less than a third; the frame is
			// 2 tan(lens/2) of the way wide)
			const double Shown = FMath::Max(FVector::CrossProduct(Out, Bow.GetSafeNormal()).Size(), 0.33) * Long;
			Away = FMath::Clamp(3.0 * Shown / (2.0 * FMath::Tan(FMath::DegreesToRadians(LensDegrees * 0.5))), 450.0, 1600.0);
			// (9 degrees down: the frame's top edge a little over the horizon, so her column stands against the sky and
			// not the sea; at 13 the top lay 2 to 3 degrees under it. The pre-check and the art director, 18:47Z)
			Down = -9.0;
			Heading = (-Out).Rotation().Yaw;
			Wanted = ShipWorld + FVector::UpVector * (0.12 * Long * U);
			Lens->SetFieldOfView(static_cast<float>(LensDegrees));
		}
		else if (Long > 0.0)
		{
			// In harbour (the art director on builds 1766 and 1770, Pearl Harbor 06: the eye stood inside a ship's
			// smoke column, the frame brown-black murk with almost no ship). The eye stands upwind of the row, 55 degrees
			// off the wind, so the columns lean away from it; at least 300 m clear of every column that stands or may
			// stand over the ships struck, sunk or moored beside her, from the picture's start to its end (chosen once,
			// held); and where the columns' spread leaves no such eye at 1.6 km, back and up until there is one. The
			// smoke is never thinned for it: the oil columns stay out of the near eye's fade (the effects rule).
			if (ShipsEyeFor != ShipsLook)
			{
				ShipsEyeFor = ShipsLook;
				const FVector Abeam = FVector::CrossProduct(FVector::UpVector, Bow).GetSafeNormal();
				FVector Sun = Stage->TowardSun();
				Sun.Z = 0.0;
				Sun = Sun.GetSafeNormal();
				FVector Downwind = WorldVector(AirSky::Wind.X, AirSky::Wind.Y, 0.0).GetSafeNormal();
				if (Downwind.IsNearlyZero())
				{
					Downwind = -Abeam;
				}
				TArray<FVector> Feet;
				Stage->SmokeFeet(ShipsLook, 800.0, Feet);
				// (a column: from its foot up 1.2 km, leaning downwind as it rises; its width grows with its height)
				auto Clear = [&Feet, &Downwind, U](const FVector& Eye_) -> double
				{
					double Least = 1e12;
					for (const FVector& Foot : Feet)
					{
						const FVector Top = Foot + (FVector::UpVector + Downwind * 0.8) * (1200.0 * U);
						const FVector Nearest = FMath::ClosestPointOnSegment(Eye_, Foot, Top);
						const double Up = (Nearest.Z - Foot.Z) / U;
						Least = FMath::Min(Least, FVector::Dist(Eye_, Nearest) / U - 0.12 * Up);
					}
					return Least;
				};
				// (the ways the eye may stand, upwind of the row and off the wind, best first: the one that shows her
				// broadside and is lit from behind the eye, then the others, nearer the wind and farther off it)
				TArray<TPair<double, FVector>> Ways;
				for (const double Turn : { 55.0, -55.0, 35.0, -35.0, 75.0, -75.0 })
				{
					const FVector Way = (-Downwind).RotateAngleAxis(Turn, FVector::UpVector);
					const double Score = FMath::Abs(FVector::DotProduct(Way, Abeam)) + 0.25 * FMath::Max(0.0, FVector::DotProduct(Way, Sun)) - 0.004 * (FMath::Abs(Turn) - 55.0) * (FMath::Abs(Turn) - 55.0) / 20.0;
					Ways.Add(TPair<double, FVector>(Score, Way));
				}
				Ways.Sort([](const TPair<double, FVector>& A, const TPair<double, FVector>& B) { return A.Key > B.Key; });
				// (clear of every column by 700 m, or by 0.35 of the eye's way to her, whichever is more: at 300 m on build
				// 1778 a near column out of focus still filled 45 per cent of the picture, the art director 17:54Z: 60 per
				// cent of it clear, no column inside the near eye's fade. The nearest eye that is, any of the ways)
				ShipsEyeHeading = (-Ways[0].Value).Rotation().Yaw;
				ShipsEyeAway = 1600.0;
				ShipsEyeDown = -24.0;
				bool bFound = false;
				for (int32 Step = 0; Step <= 16 && !bFound; ++Step)
				{
					const double Away_ = 1600.0 + Step * 250.0, Down_ = -24.0 - Step * 1.25;
					for (const TPair<double, FVector>& Way : Ways)
					{
						const double Heading_ = (-Way.Value).Rotation().Yaw;
						const FVector Eye_ = ShipWorld - FRotator(Down_, Heading_, 0.0).Vector() * Away_ * U;
						if (Clear(Eye_) >= FMath::Max(700.0, 0.35 * Away_))
						{
							ShipsEyeHeading = Heading_;
							ShipsEyeAway = Away_;
							ShipsEyeDown = Down_;
							bFound = true;
							break;
						}
					}
				}
				if (!bFound)
				{
					// (none clear: the farthest and steepest of the best way)
					ShipsEyeAway = 1600.0 + 16 * 250.0;
					ShipsEyeDown = -24.0 - 16 * 1.25;
				}
				UE_LOG(LogTheater, Display, TEXT("Air battle: the ships picture in harbour, %d columns %s: %.0f m off, %.1f degrees down, heading %.0f"), Feet.Num(),
					bFound ? TEXT("kept clear") : TEXT("not all kept clear"), ShipsEyeAway, -ShipsEyeDown, ShipsEyeHeading);
			}
			Away = ShipsEyeAway;
			Down = ShipsEyeDown;
			Heading = ShipsEyeHeading;
		}
	}
	// "The guns fire back" picture (item 14; the Mac builder on r2d: no picture showed a ship's guns at work, the ships
	// picture coming after the raid or on a carrier already ablaze): the ship of the first attack run on a ship, seen
	// side-on to the line from her to the aircraft her guns are on, so that her tracers and the heavy bursts climb across
	// the picture toward him. The look is at two fifths of the way from her to him, 4 degrees down: she sits low in the
	// frame with the sea round her, he high against the sky, the horizon between. Its way is chosen once (of the two
	// side-on ways the one upwind, so her smoke leans away, and with the sun behind the eye) and held to its end.
	FVector GunShip;
	const bool bOnGuns = !bOnShip && GunsLook >= 0 && Stage && Stage->IsRaised() && Stage->ShipAt(GunsLook, GunShip);
	if (bOnGuns)
	{
		for (const AirShow::PlaneOut& P : S.Show.Planes())
		{
			if (P.Key == GunsOn && P.bShown)
			{
				GunsPlane = World(P.At.X, P.At.Y, P.At.Z);
				break;
			}
		}
		const FVector Line = GunsPlane - GunShip;
		const FVector Across(Line.X, Line.Y, 0.0);
		if (GunsEyeFor != GunsLook)
		{
			GunsEyeFor = GunsLook;
			FVector Along = Across.GetSafeNormal();
			if (Along.IsNearlyZero())
			{
				// (he is straight over her: side-on to her length)
				double Long = 0.0;
				FVector Bow;
				bool bHarbour = true;
				Along = Stage->ShipLie(GunsLook, Long, Bow, bHarbour) ? Bow.GetSafeNormal() : FRotator(0.0, Yaw, 0.0).Vector();
			}
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal();
			FVector Sun = Stage->TowardSun();
			Sun.Z = 0.0;
			Sun = Sun.GetSafeNormal();
			const FVector Upwind = -WorldVector(AirSky::Wind.X, AirSky::Wind.Y, 0.0).GetSafeNormal();
			const FVector Way = FVector::DotProduct(Side, Upwind) + FVector::DotProduct(Side, Sun) >= 0.0 ? Side : -Side;
			GunsEyeHeading = (-Way).Rotation().Yaw;
			UE_LOG(LogTheater, Display, TEXT("Air battle: the guns fire back picture, heading %.0f, the aircraft %.0f m along and %.0f m up from her"), GunsEyeHeading, Across.Size() / U, Line.Z / U);
		}
		// (both in the frame with an eighth to spare: at 55 degrees it is 2 tan 27.5 of the way wide, and at 16:9
		// 2 tan 16.3 of it high; never nearer than 350 m, never farther than 2.6 km. With a quarter to spare, and 450 m
		// at least, both were small: "frame both tighter", the art director on 2b177386's 06b, 22 November 04:16Z)
		const double Wide = Across.Size() / U, High = FMath::Abs(Line.Z) / U;
		Away = FMath::Clamp(FMath::Max(0.68 * Wide / FMath::Tan(FMath::DegreesToRadians(27.5)), 0.68 * High / FMath::Tan(FMath::DegreesToRadians(16.3))), 350.0, 2600.0);
		Wanted = GunShip + Line * 0.4;
		Down = -4.0;
		Heading = GunsEyeHeading;
	}
	// The parachutes picture (the art director on a1acb552's frames, item 8): the open canopies that hang together,
	// followed down, from 300 m off and a little under them, so they stand against the sky with the sea or the cloud
	// under them; the middle of those within 1.5 km of where they were last.
	const bool bOnChutes = bChutesLook && !bOnShip && !bOnGuns;
	if (bOnChutes)
	{
		FVector ChuteSum = FVector::ZeroVector;
		int32 ChuteCount = 0;
		for (const AirShow::Thing& T : S.Show.Things())
		{
			const FVector ChuteAt = World(T.At.X, T.At.Y, T.At.Z);
			if (T.Kind == AirShow::ThingKind::Chute && T.bOpen && FVector::Dist(ChuteAt, ChutesWorld) < 1500.0 * U)
			{
				ChuteSum += ChuteAt;
				++ChuteCount;
			}
		}
		if (ChuteCount > 0)
		{
			ChutesWorld = ChuteSum / ChuteCount;
		}
		Wanted = ChutesWorld;
		Away = 300.0;
		Down = 4.0;
	}
	// (the look carried with the crowd as it moves, and only a jump to another crowd eased: eased all the way, at the
	// clock's x10 the look trailed the raid by a kilometre and more, and the strategy pictures before the meeting showed
	// empty sky, the art director on 2b177386's Pearl 01, Midway 01 and Bremen 01, 22 November 04:16Z)
	if (bLookSet && !bOnShip && !bOnGuns && !bOnChutes)
	{
		const FVector Moved = Wanted - LookWanted;
		if (Moved.Size() < 150.0 * U)
		{
			LookAt += Moved;
		}
		LookAt = FMath::Lerp(LookAt, Wanted, AirParts::Smooth(Real, 1.2));
	}
	else
	{
		LookAt = Wanted;
	}
	LookWanted = Wanted;
	bLookSet = true;
	const FRotator Facing(Down, Heading, 0.0);
	auto EyeFrom = [this, &Facing, U](const FVector& Look_, double Away_)
	{
		FVector From_ = Look_ - Facing.Vector() * Away_ * U;
		if (Stage && Stage->IsRaised())
		{
			From_.Z = FMath::Max(From_.Z, Stage->GroundZ(From_) + 40.0 * U);
		}
		From_.Z = FMath::Max(From_.Z, 40.0 * U);
		return From_;
	};
	FVector From = EyeFrom(LookAt, Away);
	// (the camera rules' 13: an eye within twice a fuel ball's width in its first 2 s is cut on its flash, here drawn
	// back along its own line until it is clear, the look kept. In the strategy view the player's zoom is drawn back
	// with it, so the eye stays there after the ball; the pictures' own framings come back when the ball is out)
	if (BallsNear.Num() > 0 && !(S.bFlying && S.GlideT >= 0.0) && IntoFuelBall(From) > 0.0)
	{
		const double Was = Away;
		for (int32 Step = 0; Step < 16 && IntoFuelBall(From) > 0.0; ++Step)
		{
			Away *= 1.25;
			From = EyeFrom(LookAt, Away);
		}
		if (!bOnShip && !bOnGuns && !bOnChutes)
		{
			Distance = FMath::Clamp(Distance * Away / FMath::Max(Was, 1.0), 120.0, 20000.0);
		}
		AirParts::TellCut(GetWorld());
		++BallCuts;
		UE_LOG(LogTheater, Display, TEXT("Air battle: the overview drawn back off a fuel ball's flash, %.0f m off to %.0f (rule 13, cut %d)."), Was, Away, BallCuts);
	}
	Eye->SetActorLocationAndRotation(From, (LookAt - From).Rotation());
	if (S.bFlying && S.GlideT >= 0.0)
	{
		// The glide from the flyover's last eye into the strategy view, eased in and out; the film's frame draws back.
		S.GlideT += Real;
		const double Into = FMath::Clamp(S.GlideT / FMath::Max(S.GlideLong, 0.1), 0.0, 1.0);
		const double Eased = Into * Into * (3.0 - 2.0 * Into);
		S.FlyBars = 1.0 - Eased;
		if (Into >= 1.0)
		{
			S.bFlying = false;
			S.FlyBars = 0.0;
			S.FlyEndRealT = RealT;
		}
		else
		{
			const FQuat Toward = FQuat::Slerp(S.GlideFacing.Quaternion(), (LookAt - From).Rotation().Quaternion(), Eased);
			Eye->SetActorLocationAndRotation(FMath::Lerp(S.GlideEye, From, Eased), Toward.Rotator());
			Lens->SetFieldOfView(static_cast<float>(FMath::Lerp(AirDesk::Flyover::Lens, 55.0, Eased)));
		}
	}
}

void AAirBattle::Orbit(const FVector2D& DeltaPixels)
{
	Yaw += DeltaPixels.X * 0.25;
	Pitch = FMath::Clamp(Pitch - DeltaPixels.Y * 0.2, -80.0, 10.0);
}

UTexture2D* AAirBattle::FlagPicture(const FString& Tag)
{
	ACampaignMapActor* Map = Table.Get();
	if (!Map && GetWorld())
	{
		TActorIterator<ACampaignMapActor> It(GetWorld());
		Map = It ? *It : nullptr;
	}
	return Map && !Tag.IsEmpty() ? Map->GetFlagTexture(Tag) : nullptr;
}

void AAirBattle::Zoom(float Steps)
{
	Distance = FMath::Clamp(Distance * FMath::Pow(0.85, static_cast<double>(Steps)), 120.0, 20000.0);
}

void AAirBattle::PlaceTags()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_PlaceTags);
	FAirBattleState& S = *State;
	S.TagAt.clear();
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!Controller)
	{
		return;
	}
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		S.ViewportPixels = Size;
	}
	// Each tag's place over its formation. Groups never stack (b384(d)): the screen, which knows how wide each tag is
	// drawn, gathers tags that would touch into one block (SAirBattleScreen::OnPaint).
	for (const AirDesk::Tag& T : S.Words.Tags)
	{
		AirShow::Vec Middle;
		double Face = 0.0;
		if (T.bGone || !S.Show.Middle(T.Id, Middle, Face))
		{
			continue;
		}
		FVector2D OnScreen;
		if (Controller->ProjectWorldLocationToScreen(World(Middle.X, Middle.Y, Middle.Z + 80.0), OnScreen, false)
			&& OnScreen.X >= 0.0 && OnScreen.Y >= 0.0 && OnScreen.X <= S.ViewportPixels.X && OnScreen.Y <= S.ViewportPixels.Y)
		{
			S.TagAt[T.Id] = OnScreen;
		}
	}
	// The ridden aircraft's box on the screen (the Mac's release 2 check: the dispatches covered it in Bremen, Midway
	// and Oahu 03 and 05, and a group's tag sat on it in Oahu 05): the corners of its wings, nose, tail and fin, as
	// it flies.
	S.bRiddenOn = false;
	if (S.bClose)
	{
		for (const AirShow::PlaneOut& P : S.Show.Planes())
		{
			if (P.Key != S.Subject.Plane || !P.bShown)
			{
				continue;
			}
			const FQuat Attitude = FRotator(P.Pitch, P.Heading, P.Bank).Quaternion();
			const FVector Body = World(P.At.X, P.At.Y, P.At.Z);
			const double Half = 0.5 * FMath::Max(P.Span, 8.0) * UnitsPerMetre;
			const FVector Ways[3] = { Attitude.GetForwardVector() * Half * 0.85, Attitude.GetRightVector() * Half, Attitude.GetUpVector() * Half * 0.3 };
			FVector2D Min(1e9, 1e9), Max(-1e9, -1e9);
			int32 Seen = 0;
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				const FVector At = Body + Ways[0] * ((Corner & 1) ? 1.0 : -1.0) + Ways[1] * ((Corner & 2) ? 1.0 : -1.0) + Ways[2] * ((Corner & 4) ? 1.0 : -1.0);
				FVector2D OnScreen;
				if (Controller->ProjectWorldLocationToScreen(At, OnScreen, false))
				{
					Min = FVector2D(FMath::Min(Min.X, OnScreen.X), FMath::Min(Min.Y, OnScreen.Y));
					Max = FVector2D(FMath::Max(Max.X, OnScreen.X), FMath::Max(Max.Y, OnScreen.Y));
					++Seen;
				}
			}
			if (Seen == 8)
			{
				S.RiddenMin = FVector2D(FMath::Max(Min.X, 0.0), FMath::Max(Min.Y, 0.0));
				S.RiddenMax = FVector2D(FMath::Min(Max.X, S.ViewportPixels.X), FMath::Min(Max.Y, S.ViewportPixels.Y));
				S.bRiddenOn = S.RiddenMax.X > S.RiddenMin.X && S.RiddenMax.Y > S.RiddenMin.Y;
			}
			break;
		}
	}
}

int32 AAirBattle::TagUnder(const FVector2D& Pixel) const
{
	const FAirBattleState& S = *State;
	// A tag as drawn (a block's last line is its first tag's); else the formation nearest the click.
	for (auto Box = S.TagBoxes.rbegin(); Box != S.TagBoxes.rend(); ++Box)
	{
		if (S.TagAt.count(Box->Id) && Pixel.X >= Box->Min.X && Pixel.X <= Box->Max.X && Pixel.Y >= Box->Min.Y && Pixel.Y <= Box->Max.Y)
		{
			return Box->Id;
		}
	}
	int32 Best = -1;
	double BestD = 60.0;
	for (const auto& Pair : S.TagAt)
	{
		const double D = FVector2D::Distance(Pair.second, Pixel);
		if (D < BestD)
		{
			BestD = D;
			Best = Pair.first;
		}
	}
	return Best;
}

bool AAirBattle::BoardUnder(const FVector2D& Pixel, double Height, FVector2D& OutBoard) const
{
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	FVector From, Way;
	if (!Controller || !Controller->DeprojectScreenPositionToWorld(static_cast<float>(Pixel.X), static_cast<float>(Pixel.Y), From, Way) || FMath::Abs(Way.Z) < 1e-4)
	{
		return false;
	}
	const double Along = (Height * UnitsPerMetre - From.Z) / Way.Z;
	if (Along <= 0.0)
	{
		return false;
	}
	const FVector Hit = From + Way * Along;
	OutBoard = FVector2D(Hit.Y / UnitsPerMetre, Hit.X / UnitsPerMetre);
	return true;
}

void AAirBattle::PickAt(const FVector2D& Pixel, bool bAdd)
{
	FAirBattleState& S = *State;
	const int32 Id = TagUnder(Pixel);
	const AirDesk::Tag* Tag = Id >= 0 ? S.TagOf(Id) : nullptr;
	if (!bAdd)
	{
		S.Hand.clear();
	}
	if (Tag && Tag->bMine && std::find(S.Hand.begin(), S.Hand.end(), Id) == S.Hand.end())
	{
		S.Hand.push_back(Id);
	}
	// (in the close view a click on a tag rides that squadron, his or the enemy's: the chase view's switch)
	if (Tag && S.bClose && !S.bFlying && Id != S.RideGroup)
	{
		const int Key = AirDesk::RideStep(S.Show.Planes(), Id, -1, 0);
		if (Key >= 0)
		{
			RideOn(Id, Key);
		}
	}
}

void AAirBattle::RideAt(const FVector2D& Pixel)
{
	FAirBattleState& S = *State;
	if (S.bCard || S.bFlying)
	{
		return;
	}
	const int32 Id = TagUnder(Pixel);
	if (Id < 0 || !S.TagOf(Id))
	{
		return;
	}
	const int Key = AirDesk::RideStep(S.Show.Planes(), Id, -1, 0);
	if (Key < 0)
	{
		S.Note = "That squadron has no aircraft in the air to ride.";
		S.NoteAge = 0.0;
		return;
	}
	RideOn(Id, Key);
}

void AAirBattle::RideOn(int32 Group, int32 Key)
{
	FAirBattleState& S = *State;
	S.bClose = true;
	S.RideGroup = Group;
	S.Subject = AirDesk::Subject();
	S.Subject.Plane = Key;
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		if (P.Key == Key)
		{
			S.Subject.Other = P.Target;
		}
	}
	CloseEye = FVector::ZeroVector; // (a cut to the aircraft, never a swoop across the sky)
}

void AAirBattle::OrderAt(const FVector2D& Pixel)
{
	FAirBattleState& S = *State;
	if (S.Hand.empty())
	{
		return;
	}
	const int32 Id = TagUnder(Pixel);
	const AirDesk::Tag* Tag = Id >= 0 ? S.TagOf(Id) : nullptr;
	if (Tag && !Tag->bMine)
	{
		Order(AirParts::Wide(S.Desk.Attack(S.Hand, Id)));
		return;
	}
	AirShow::Vec Middle;
	double Face = 0.0;
	const double Height = S.Show.Middle(S.Hand.front(), Middle, Face) ? Middle.Z : 4500.0;
	FVector2D Board;
	if (BoardUnder(Pixel, Height, Board))
	{
		Order(AirParts::Wide(S.Desk.Move(S.Hand, Board.X, Board.Y)));
	}
}

bool AAirBattle::Order(const FString& Command)
{
	FAirBattleState& S = *State;
	if (Command.IsEmpty())
	{
		return false;
	}
	if (S.bReplay || !Session)
	{
		S.Note = "A recorded battle: it plays as it was fought.";
		S.NoteAge = 0.0;
		return false;
	}
	FString Reply, Error;
	if (!Session->CallBattle(TEXT("battleOrder"), TEXT("[") + Command + TEXT("]"), Reply, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: TC.battleOrder failed: %s"), *Error);
		return false;
	}
	if (!Reply.Contains(TEXT("true")))
	{
		S.Note = "That order cannot be given now.";
		S.NoteAge = 0.0;
		return false;
	}
	CampaignSounds::Play(this, TEXT("stamp"));
	return true;
}

void AAirBattle::Press(const FString& Button)
{
	FAirBattleState& S = *State;
	if (Button == TEXT("skip"))
	{
		// (a click or a key during the flyover: its shots end and a short glide takes the eye to the battle)
		if (S.bFlying && S.GlideT < 0.0)
		{
			S.GlideT = 0.0;
			S.GlideLong = 0.8;
			bLookSet = false;
		}
		return;
	}
	if (S.bCard)
	{
		if (Button == TEXT("continue"))
		{
			Finish();
		}
		return;
	}
	if (Button == TEXT("ride"))
	{
		S.bClose = !S.bClose;
		bRideOther = bRideOther && S.bClose; // (a ride taken off ends a ride of the other side's; the next is his own)
		S.Subject = AirDesk::Subject();
		S.RideGroup = -1;
		CloseEye = FVector::ZeroVector;
		return;
	}
	if (Button.StartsWith(TEXT("ride:")))
	{
		// The chase view's steps (Brian, 21 November 05:52Z: "you can then move from one plane to the next"): along the
		// squadron ridden, or to the next squadron's leader; the squadron stepped to is his choice from then on.
		if (!S.bClose || S.bFlying)
		{
			return;
		}
		const std::vector<AirShow::PlaneOut>& Out = S.Show.Planes();
		const int Squadron = S.RideGroup >= 0 ? S.RideGroup : S.RiddenGroup();
		const bool bAircraft = Button == TEXT("ride:next") || Button == TEXT("ride:prev");
		const int Way = Button == TEXT("ride:prev") || Button == TEXT("ride:up") ? -1 : 1;
		if (bAircraft && Squadron >= 0)
		{
			const int Key = AirDesk::RideStep(Out, Squadron, S.Subject.Plane, Way);
			if (Key >= 0)
			{
				RideOn(Squadron, Key);
			}
		}
		else if (!bAircraft)
		{
			const int Next = AirDesk::RideSquadronStep(S.Words.Tags, Out, Squadron, Way);
			const int Key = Next >= 0 ? AirDesk::RideStep(Out, Next, -1, 0) : -1;
			if (Key >= 0)
			{
				RideOn(Next, Key);
			}
		}
		return;
	}
	if (S.Question != 0)
	{
		return;
	}
	const std::vector<int>& Hand = S.Hand;
	if (Button == TEXT("hold")) { Order(AirParts::Wide(S.Desk.Hold(Hand))); }
	else if (Button == TEXT("breakoff")) { Order(AirParts::Wide(S.Desk.BreakOff(Hand))); }
	else if (Button == TEXT("breakoffall")) { S.Question = 2; }
	else if (Button == TEXT("staff")) { S.Question = 1; }
	else if (Button == TEXT("hunt")) { Order(AirParts::Wide(S.Desk.Hunt(Hand))); }
	else if (Button == TEXT("jettison")) { Order(AirParts::Wide(S.Desk.Jettison(Hand))); }
	else if (Button == TEXT("low") || Button == TEXT("middle") || Button == TEXT("high"))
	{
		Order(AirParts::Wide(S.Desk.Band(Hand, Button == TEXT("low") ? 0 : Button == TEXT("middle") ? 1 : 2)));
	}
	else if (Button == TEXT("close") || Button == TEXT("ahead"))
	{
		// STAY CLOSE / RANGE AHEAD: over the nearest of his own bomber boxes.
		AirShow::Vec Middle;
		double Face = 0.0;
		int Box = -1;
		double Best = 1e18;
		if (!Hand.empty() && S.Show.Middle(Hand.front(), Middle, Face))
		{
			for (const int Id : S.MineOf(false))
			{
				AirShow::Vec Other;
				double OtherFace = 0.0;
				if (S.Show.Middle(Id, Other, OtherFace))
				{
					const double D = (Other.X - Middle.X) * (Other.X - Middle.X) + (Other.Y - Middle.Y) * (Other.Y - Middle.Y);
					if (D < Best)
					{
						Best = D;
						Box = Id;
					}
				}
			}
		}
		Order(AirParts::Wide(S.Desk.Escort(Hand, Box, Button == TEXT("close"))));
	}
}

void AAirBattle::SetSpeed(double Speed)
{
	FAirBattleState& S = *State;
	S.Clock.Speed = Speed;
}

void AAirBattle::Answer(bool bYes)
{
	FAirBattleState& S = *State;
	const int Asked = S.Question;
	S.Question = 0;
	if (!bYes || Asked == 0)
	{
		return; // (no question standing: nothing to answer)
	}
	Order(AirParts::Wide(Asked == 1 ? S.Desk.HandOver() : S.Desk.BreakOffAll()));
}

void AAirBattle::Finish()
{
	// CONTINUE: the scene is taken down, the table shown again; the campaign's own after-battle steps are the HUD's.
	// (release 1.6: a raid he flew himself is handed back once the table is up, with what the rules booked)
	TFunction<void(const FString&)> Hand = MoveTemp(HandBack);
	HandBack = nullptr;
	const FString Handed = EndReply;
	HideScreen();
	if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		AirParts::ShowFrame(Cast<ACampaignHUD>(Controller->GetHUD()), true);
		Controller->SetViewTarget(Controller->GetPawn());
		AirParts::TellCut(GetWorld());
	}
	ShowTheTable();
	if (Hand)
	{
		Hand(Handed); // (the campaign takes the raid's answer and the map shows what came of it)
	}
	Destroy();
}

void AAirBattle::WayOut(const TCHAR* Why)
{
	UE_LOG(LogTheater, Warning, TEXT("Air battle (live): %s: the raid closes with no result%s"), Why,
		HandBack ? TEXT(", the campaign's raid card back as he left it") : TEXT(""));
	EndReply.Empty(); // (TC.battleEnd was never reached: nothing is booked)
	Finish(); // (as CONTINUE: the screen down, the table up, an empty result handed back, and the scene destroyed)
}

bool AAirBattle::CampaignLiveOn()
{
	return FLandLive::RuleNow(TEXT("flyLive"));
}

void AAirBattle::EndPlay(const EEndPlayReason::Type Reason)
{
	SkyFinish(false); // (a turn of the clouds painted aside: done before the stage and the meshes it touches go)
	HideScreen();
	if (KeptScreenPercent > 0.0f)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
		{
			Variable->Set(KeptScreenPercent, ECVF_SetByCode);
		}
		KeptScreenPercent = -1.0f;
	}
	if (Reason == EEndPlayReason::Destroyed)
	{
		ShowTheTable(); // (taken down without CONTINUE: the table still comes back as it was)
	}
	for (TPair<int32, TObjectPtr<UAudioComponent>>& Pair : Engines)
	{
		if (Pair.Value)
		{
			Pair.Value->Stop();
		}
	}
	if (OwnEngine)
	{
		OwnEngine->Stop();
		OwnEngine = nullptr;
	}
	if (AirBed)
	{
		AirBed->Stop();
		AirBed = nullptr;
	}
	if (Limiter)
	{
		UAudioMixerBlueprintLibrary::RemoveMasterSubmixEffect(this, Limiter);
		Limiter = nullptr;
	}
	if (bScoreBegun)
	{
		if (ACampaignGameMode* Mode = GetWorld() ? Cast<ACampaignGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
		{
			Mode->StopBattleScore();
		}
	}
	if (Effects)
	{
		Effects->End();
		Effects.Reset();
	}
	if (Stage)
	{
		Stage->Destroy();
		Stage = nullptr;
	}
	if (GroundFx)
	{
		GroundFx->End();
		GroundFx.Reset();
	}
	GroundFxMesh.Reset();
	AirFxMesh.Reset();
	if (Eye)
	{
		Eye->Destroy();
		Eye = nullptr;
	}
	if (Reason == EEndPlayReason::Destroyed && HandBack)
	{
		// (release 1.6: his raid taken down without CONTINUE is handed back all the same, with what the rules booked at
		// its end where it reached one, or nothing: the map is never left waiting; the pre-check on 0c0f1aee)
		TFunction<void(const FString&)> Hand = MoveTemp(HandBack);
		HandBack = nullptr;
		Hand(EndReply);
	}
	Super::EndPlay(Reason);
}

void AAirBattle::ShowScreen()
{
	if (!GEngine || !GEngine->GameViewport || Screen.IsValid())
	{
		return;
	}
	Screen = SNew(SAirBattleScreen).Battle(this);
	// The words and the name tags a quarter larger than Unreal's own game-screen size, which already grows with the
	// window's height (the art director on build 1734: they were small at Brian's window; 1.3 at 1440 high and 1.24 at
	// his, so one number between). Everything on the screen grows together, so the dark plates still fit their words.
	// "-TheaterAirTextScale=" tunes it.
	float Lettering = 1.25f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterAirTextScale="), Lettering);
	Lettering = FMath::Clamp(Lettering, 0.75f, 2.0f);
	ScreenFrame = SNew(SDPIScaler).DPIScale(Lettering)[Screen.ToSharedRef()];
	GEngine->GameViewport->AddViewportWidgetContent(ScreenFrame.ToSharedRef(), 20);
	if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Screen);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(Mode);
		Controller->bShowMouseCursor = ShotFolder.IsEmpty();
		bOwnInput = true;
	}
}

void AAirBattle::HideScreen()
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
			// (the campaign's own mode: the pointer stays in sight while a drag turns the table, and is never locked in)
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Controller->SetInputMode(Mode);
		}
	}
}

// ---- Sound -----------------------------------------------------------------------------------------------------

USoundBase* AAirBattle::SoundOf(const TCHAR* Name, bool bLoop)
{
	const FName Key(Name);
	if (TObjectPtr<USoundBase>* Found = Sounds.Find(Key))
	{
		// (a loop asked for by a take first loaded to play once: looped from now on)
		if (bLoop)
		{
			if (USoundWave* Wave = Cast<USoundWave>(Found->Get()))
			{
				Wave->bLooping = true;
			}
		}
		return Found->Get();
	}
	if (SoundsMissing.Contains(Key))
	{
		return nullptr;
	}
	// The battle pack's recordings, brought in by Tools/build_sounds.sh from Sounds/Real to /Game/Campaign/Sounds/Real.
	USoundBase* Made = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Campaign/Sounds/Real/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Made)
	{
		SoundsMissing.Add(Key);
		UE_LOG(LogTheater, Display, TEXT("Air battle: the sound %s is not in /Game/Campaign/Sounds/Real (Tools/build_sounds.sh brings it in)"), Name);
		return nullptr;
	}
	if (bLoop)
	{
		if (USoundWave* Wave = Cast<USoundWave>(Made))
		{
			Wave->bLooping = true;
		}
	}
	Sounds.Add(Key, Made);
	return Made;
}

USoundAttenuation* AAirBattle::FamilyOf(const TCHAR* Name)
{
	const FName Key(Name);
	if (TObjectPtr<USoundAttenuation>* Found = Families.Find(Key))
	{
		return Found->Get();
	}
	// A distance family of the mix (mix.json distance): full loudness to `full` metres, nothing past `silent`, the top
	// taken off with distance by its lpf points.
	const TSharedPtr<FJsonObject> Family = AirParts::MixObject(Mix, FString(TEXT("distance.")) + Name);
	if (!Family.IsValid())
	{
		return nullptr;
	}
	double Full = 50.0, Silent = 1500.0;
	Family->TryGetNumberField(TEXT("full"), Full);
	Family->TryGetNumberField(TEXT("silent"), Silent);
	USoundAttenuation* Made = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& A = Made->Attenuation;
	A.bAttenuate = true;
	A.bSpatialize = true;
	A.AttenuationShape = EAttenuationShape::Sphere;
	A.AttenuationShapeExtents = FVector(Full * UnitsPerMetre, 0.0, 0.0);
	A.FalloffDistance = static_cast<float>(FMath::Max(1.0, Silent - Full) * UnitsPerMetre);
	A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	const TArray<TSharedPtr<FJsonValue>>* Lpf = nullptr;
	if (Family->TryGetArrayField(TEXT("lpf"), Lpf) && Lpf->Num() >= 2)
	{
		// (the first point is the near one: full brightness out to it. Taking the second as the near point had every
		// gun inside 600 m cut at 4 kHz and every engine inside 3 km at 3.5 kHz, their crack and edge gone; the sound
		// director, 21 November, on the Bremen film)
		const TArray<TSharedPtr<FJsonValue>>& Near = (*Lpf)[0]->AsArray();
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
	Families.Add(Key, Made);
	return Made;
}

UAudioComponent* AAirBattle::Lay(USoundBase* Played, const FVector* Where, USoundAttenuation* Family, float Volume, float From, float Priority, float PitchBy, bool bAutoDestroy)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_Lay);
	// (made, given its voice priority, and only then begun: UAudioComponent::Play hands the priority to the sound the
	// engine runs, so a priority set after the spawn, as the release 2 air patch first did, reached nothing)
	if (Where && Family && Eye)
	{
		// (past its family's silent distance from the eye, the listener: not made at all, as the engine's own spawn did not)
		const double Reach = Family->Attenuation.AttenuationShapeExtents.X + Family->Attenuation.FalloffDistance;
		if (FVector::DistSquared(*Where, Eye->GetActorLocation()) > Reach * Reach)
		{
			return nullptr;
		}
	}
	UAudioComponent* Player = UGameplayStatics::CreateSound2D(this, Played, Volume, PitchBy, 0.0f, nullptr, false, bAutoDestroy);
	if (!Player)
	{
		return nullptr;
	}
	Player->bIsUISound = false; // (CreateSound2D makes an interface sound, which would play on through a pause)
	if (Where)
	{
		Player->bAllowSpatialization = true;
		Player->AttenuationSettings = Family;
		Player->SetWorldLocation(*Where);
	}
	if (Priority > 0.0f)
	{
		Player->bOverridePriority = true;
		Player->Priority = Priority;
	}
	Player->Play(From);
	return Player;
}

UAudioComponent* AAirBattle::Sound(const TCHAR* Name, const FVector& Where, const TCHAR* Family, float Volume, float After, float Cut, float From, float Priority)
{
	if (After > 0.0f)
	{
		Later.Add({ RealT + After, FName(Name), Where, FName(Family), Volume, Cut, From });
		return nullptr;
	}
	USoundBase* Played = SoundOf(Name);
	if (!Played)
	{
		return nullptr;
	}
	// (laid on at the battle's level now: louder riding an aircraft, and as the strategy camera draws back. A big moment
	// plays at its full level and dips the rest; anything else laid on while one is dipping is laid on dipped)
	const bool bBig = BigMoment(Name, Where);
	UAudioComponent* Player = Lay(Played, &Where, FamilyOf(Family), Volume * MasterGain * (bBig ? 1.0f : DuckCloseGain), From, bBig ? FMath::Max(Priority, 3.5f) : Priority);
	if (Player && Cut > 0.0f)
	{
		// (only the first part of the take: the rest is the ground's echo, which the sky does not have)
		TWeakObjectPtr<UAudioComponent> Weak(Player);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
		{
			if (Weak.IsValid())
			{
				Weak->FadeOut(0.08f, 0.0f);
			}
		}), Cut, false);
	}
	return Player;
}

bool AAirBattle::BigMoment(const TCHAR* Name, const FVector& Where)
{
	// A big moment (mix.json big): one of big.names, or a take nearer than its big.alsoWhenCloserThan. Within
	// air.bigDuck.within of the eye it dips the rest (attack, hold, release); farther off it plays but dips nothing.
	const TSharedPtr<FJsonObject> Big = AirParts::MixObject(Mix, TEXT("big"));
	if (!Big.IsValid())
	{
		return false;
	}
	const double Metres = Eye ? FVector::Dist(Where, Eye->GetActorLocation()) / UnitsPerMetre : 1e9;
	bool bBig = false;
	const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
	if (Big->TryGetArrayField(TEXT("names"), Names))
	{
		for (const TSharedPtr<FJsonValue>& One : *Names)
		{
			if (One.IsValid() && One->AsString() == Name)
			{
				bBig = true;
				break;
			}
		}
	}
	if (!bBig)
	{
		const TSharedPtr<FJsonObject> Near = AirParts::MixObject(Big, TEXT("alsoWhenCloserThan"));
		double Closer = 0.0;
		bBig = Near.IsValid() && Near->TryGetNumberField(FString(Name), Closer) && Metres < Closer;
	}
	if (bBig && Metres <= AirParts::MixNumber(Mix, TEXT("air.bigDuck.within"), 1000.0))
	{
		// (a second big moment while one is dipping carries on from where the dip stands, never back up and down again)
		const double Attack = AirParts::MixNumber(Mix, TEXT("big.attackMs"), 50.0) / 1000.0;
		DuckFrom = RealT - Attack * DuckLevel;
	}
	return bBig;
}

UAudioComponent* AAirBattle::SoundAtEye(const TCHAR* Name, float Volume, float Cut, bool bLoop, float Priority)
{
	USoundBase* Played = SoundOf(Name, bLoop);
	if (!Played || !Eye)
	{
		return nullptr;
	}
	// (unplaced, with no distance rule: heard as the mix sets it wherever the eye goes)
	UAudioComponent* Player = Lay(Played, nullptr, nullptr, Volume * MasterGain, 0.0f, Priority);
	if (Player)
	{
		if (Cut > 0.0f)
		{
			TWeakObjectPtr<UAudioComponent> Weak(Player);
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
			{
				if (Weak.IsValid())
				{
					Weak->FadeOut(0.08f, 0.0f);
				}
			}), Cut, false);
		}
	}
	return Player;
}

void AAirBattle::Play(double Real)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_Play);
	(void)Real;
	FAirBattleState& S = *State;
	// The sounds held for their moment (a flak burst heard after its flash).
	for (int32 K = Later.Num() - 1; K >= 0; --K)
	{
		if (RealT >= Later[K].At)
		{
			const FLater Due = Later[K];
			Later.RemoveAt(K);
			Sound(*Due.Name.ToString(), Due.Where, *Due.Family.ToString(), Due.Volume, 0.0f, Due.Cut, Due.From);
		}
	}
	EngineSounds();
	const FVector Listener = Eye ? Eye->GetActorLocation() : FVector::ZeroVector;
	// The flak guns on the ground while a box is in their ring (mix.json air.flak: a gun every `every` seconds).
	const TSharedPtr<FJsonObject> Flak = AirParts::MixObject(Mix, TEXT("air.flak"));
	bool bInRing = false;
	for (const auto& Pair : S.Show.Groups())
	{
		if (!Pair.second.Flak.empty())
		{
			bInRing = true;
			break;
		}
	}
	if (bInRing && Flak.IsValid() && RealT >= FlakGunsAt)
	{
		double Low = 0.8, High = 2.5;
		const TArray<TSharedPtr<FJsonValue>>* Every = nullptr;
		if (Flak->TryGetArrayField(TEXT("every"), Every) && Every->Num() == 2)
		{
			Low = (*Every)[0]->AsNumber();
			High = (*Every)[1]->AsNumber();
		}
		FlakGunsAt = RealT + FMath::FRandRange(Low, High);
		const FString Gun = AirParts::MixTake(Flak, TEXT("fire"), FMath::RandRange(0, 9));
		FString FireFamily = TEXT("big_guns");
		Flak->TryGetStringField(TEXT("fireFamily"), FireFamily);
		const FVector Battery = World(S.Board.TargetX + FMath::FRandRange(-3000.0, 3000.0), S.Board.TargetY + FMath::FRandRange(-3000.0, 3000.0), 10.0);
		Sound(*Gun, Battery, *FireFamily, 1.0f);
	}
	// A fighter group passing within 300 m of the listener: one pass take for the group, the nearest plane's (mix.json air.passes).
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		if (!P.bFighter || !P.bShown || P.bFalling)
		{
			continue;
		}
		const FVector At = World(P.At.X, P.At.Y, P.At.Z);
		if (FVector::Dist(At, Listener) > 300.0 * UnitsPerMetre)
		{
			continue;
		}
		const double* Heard = PassHeardAt.Find(P.Group);
		if (Heard && RealT - *Heard < 6.0)
		{
			continue;
		}
		PassHeardAt.Add(P.Group, RealT);
		// (the plane's own engine passing, by its model: mix.json air.passByPlane, else fighter_pass; +1.5 dB, air.passes.
		// The ridden aircraft's group passes nobody: its engines are at the eye.)
		if (S.bClose && P.Group == S.RiddenGroup())
		{
			continue;
		}
		FString Pass;
		if (const TSharedPtr<FJsonObject> ByModel = AirParts::MixObject(Mix, TEXT("air.passByPlane")))
		{
			if (!ByModel->TryGetStringField(AirParts::Wide(P.Model), Pass))
			{
				ByModel->TryGetStringField(TEXT("*"), Pass);
			}
		}
		if (Pass.IsEmpty() || !SoundOf(*Pass))
		{
			Pass = TEXT("fighter_pass");
		}
		Sound(*Pass, At, TEXT("aircraft"), 1.19f, 0.0f, 0.0f, 0.0f, 2.0f);
	}
	AirLevels(Real);
	// The guns' tally (the Mac run's log: every gun seen firing heard).
	if (RealT >= GunTallyAt)
	{
		GunTallyAt = RealT + 20.0;
		UE_LOG(LogTheater, Display, TEXT("Air battle sound: %d bursts begun, %d takes laid on, %d takes that could not play, %d out of earshot, %d rounds heard passing; level %+.1f dB%s; air intensity %.2f, music %+.0f dB."),
			GunBursts, GunTakesLaid, GunTakesLost, GunTakesFar, CracksHeard, 20.0 * FMath::LogX(10.0, FMath::Max(1e-4, static_cast<double>(MasterGain))), S.bClose ? TEXT(" (riding)") : TEXT(""),
			AirIntensity, ScoreShownDb);
	}
}

void AAirBattle::AirLevels(double Real)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_AirLevels);
	FAirBattleState& S = *State;
	const FVector Listener = Eye ? Eye->GetActorLocation() : FVector::ZeroVector;
	// Big moments (mix.json big; BigMoment): each layer dips by its big.duckDb at the full of it, over attack, hold, release.
	DuckLevel = 0.0;
	if (DuckFrom >= 0.0)
	{
		const double Attack = FMath::Max(1e-3, AirParts::MixNumber(Mix, TEXT("big.attackMs"), 50.0) / 1000.0);
		const double Hold = AirParts::MixNumber(Mix, TEXT("big.holdMs"), 800.0) / 1000.0;
		const double Release = FMath::Max(1e-3, AirParts::MixNumber(Mix, TEXT("big.releaseMs"), 1500.0) / 1000.0);
		const double T = RealT - DuckFrom;
		DuckLevel = T < Attack ? FMath::Max(0.0, T / Attack) : T < Attack + Hold ? 1.0 : FMath::Max(0.0, 1.0 - (T - Attack - Hold) / Release);
	}
	auto Dipped = [this](const TCHAR* Layer, double Default)
	{
		return static_cast<float>(FMath::Pow(10.0, AirParts::MixNumber(Mix, FString(TEXT("big.duckDb.")) + Layer, Default) * DuckLevel / 20.0));
	};
	DuckBedGain = Dipped(TEXT("bed"), -6.0);
	DuckLoopsGain = Dipped(TEXT("loops"), -4.0);
	DuckCloseGain = Dipped(TEXT("close"), -4.0);
	DuckMusicDb = AirParts::MixNumber(Mix, TEXT("big.duckDb.music"), -6.0) * DuckLevel;
	// The battle's end (mix.json music.end): the battle sounds come down under the stinger, stingerDucksAllDb over
	// battleSoundsFade seconds from the over line.
	if (S.bOver)
	{
		const float Under = static_cast<float>(FMath::Pow(10.0, AirParts::MixNumber(Mix, TEXT("music.end.stingerDucksAllDb"), -10.0) / 20.0));
		const double Over = FMath::Max(0.1, AirParts::MixNumber(Mix, TEXT("music.end.battleSoundsFade"), 4.0));
		EndGain = FMath::Max(Under, EndGain - static_cast<float>((1.0 - Under) * Real / Over));
	}
	// The battle's level: riding an aircraft, the close view's (air.closeView.masterDb); else by how far the strategy
	// camera stands from the fight (air.masterDbByCameraDistance, metres to dB, straight lines between).
	double Db = 0.0;
	if (S.bClose)
	{
		Db = AirParts::MixNumber(Mix, TEXT("air.closeView.masterDb"), 3.0);
	}
	else
	{
		double Nearest = 1e12;
		for (const auto& Pair : S.Show.Groups())
		{
			AirShow::Vec Middle;
			double Face = 0.0;
			if (Pair.second.Doing != AirShow::Act::Down && Pair.second.Doing != AirShow::Act::Idle && S.Show.Middle(Pair.first, Middle, Face))
			{
				Nearest = FMath::Min(Nearest, FVector::Dist(World(Middle.X, Middle.Y, Middle.Z), Listener) / UnitsPerMetre);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* DbTable = nullptr;
		if (Nearest < 1e11 && AirParts::MixObject(Mix, TEXT("air")).IsValid() && AirParts::MixObject(Mix, TEXT("air"))->TryGetArrayField(TEXT("masterDbByCameraDistance"), DbTable) && DbTable->Num() >= 2)
		{
			for (int32 K = 0; K < DbTable->Num(); ++K)
			{
				const TArray<TSharedPtr<FJsonValue>>& Row = (*DbTable)[K]->AsArray();
				const double M = Row[0]->AsNumber(), D = Row[1]->AsNumber();
				if (K == 0 && Nearest <= M)
				{
					Db = D;
					break;
				}
				if (K == DbTable->Num() - 1)
				{
					Db = D;
					break;
				}
				const TArray<TSharedPtr<FJsonValue>>& Next = (*DbTable)[K + 1]->AsArray();
				const double M2 = Next[0]->AsNumber(), D2 = Next[1]->AsNumber();
				if (Nearest <= M2)
				{
					Db = D + (D2 - D) * (Nearest - M) / FMath::Max(1.0, M2 - M);
					break;
				}
			}
		}
	}
	// (eased by the clock, about a second to settle as the mix has it, so a cut does not jump the whole battle's level,
	// whatever the frame rate)
	const float Want = static_cast<float>(FMath::Pow(10.0, Db / 20.0)) * EndGain;
	MasterGain += (Want - MasterGain) * static_cast<float>(1.0 - FMath::Exp(-FMath::Clamp(Real, 0.0, 0.25) / 0.33));

	// The ridden aircraft's own engines (air.closeView.bed): its air.loops take, unplaced at the eye, gainDb over
	// the rest. A bomber's heard through its airframe (lpfHz); a fighter's, ridden from behind it, open. A bomber
	// recorded from inside (bed.interior) plays that recording, open, at interiorGainDb instead.
	int32 Ridden = -1;
	FString OwnLoop;
	bool bOwnBomber = false;
	bool bOwnInterior = false;
	float OwnLive = 1.0f;
	if (S.bClose)
	{
		for (const AirShow::PlaneOut& P : S.Show.Planes())
		{
			if (P.Key == S.Subject.Plane && !P.bFalling && P.bShown)
			{
				Ridden = P.Key;
				bOwnBomber = !P.bFighter;
				const FString RiddenModel = AirParts::Wide(P.Model);
				OwnLoop = EngineLoop(RiddenModel);
				// (inside a B-17: two real recordings from inside one in flight, Brian's Goods of 22 Nov; a model not
				// listed, or no take of its list yet in the game, keeps its drone through the filter)
				if (bOwnBomber)
				{
					const TCHAR* Own = RiddenModel == TEXT("B17") ? TEXT("B-17") : RiddenModel == TEXT("B24") ? TEXT("B-24") : nullptr;
					const TSharedPtr<FJsonObject> Inside = AirParts::MixObject(Mix, TEXT("air.closeView.bed.interior"));
					// (one take or a list: from a list, one per ridden bomber by its key, so two bombers ridden in turn
					// are heard from inside two aircraft; a take not yet built into the game gives way to the next)
					const TArray<TSharedPtr<FJsonValue>>* InsideTakes = nullptr;
					const int32 InsideCount = Own && Inside.IsValid() && Inside->TryGetArrayField(Own, InsideTakes) ? InsideTakes->Num() : 1;
					for (int32 InsideTry = 0; Own && InsideTry < InsideCount; ++InsideTry)
					{
						const FString InsideTake = AirParts::MixTake(Inside, Own, P.Key + InsideTry);
						if (!InsideTake.IsEmpty() && SoundOf(*InsideTake, true))
						{
							OwnLoop = InsideTake;
							bOwnInterior = true;
							break;
						}
					}
				}
				// (its dead engines: the drone down by 10 log10(left / engines), air.engineLost; none left, a whisper)
				int32 Left = 0, All = 1;
				OwnLive = FMath::Max(0.3f, AirParts::LiveEngines(P.Model, P.bFighter, P.DeadEngines, Left, All));
				break;
			}
		}
	}
	const float BedGain = static_cast<float>(FMath::Pow(10.0, (bOwnInterior ? AirParts::MixNumber(Mix, TEXT("air.closeView.bed.interiorGainDb"), 2.0)
		: AirParts::MixNumber(Mix, TEXT("air.closeView.bed.gainDb"), 6.0)) / 20.0));
	if (Ridden != OwnEngineFor)
	{
		if (OwnEngine)
		{
			OwnEngine->FadeOut(0.4f, 0.0f);
			OwnEngine = nullptr;
		}
		OwnEngineFor = Ridden;
		if (Ridden >= 0 && !OwnLoop.IsEmpty())
		{
			OwnEngine = SoundAtEye(*OwnLoop, BedGain, 0.0f, true, 4.0f);
			if (OwnEngine)
			{
				if (bOwnBomber && !bOwnInterior)
				{
					OwnEngine->SetLowPassFilterEnabled(true);
					OwnEngine->SetLowPassFilterFrequency(static_cast<float>(AirParts::MixNumber(Mix, TEXT("air.closeView.bed.lpfHz"), 200.0)));
				}
			}
		}
	}
	else if (OwnEngine)
	{
		OwnEngine->SetVolumeMultiplier(BedGain * MasterGain * DuckBedGain * OwnLive);
	}

	// The far air bed (beds.list's air row: air_battle_far_loop), unplaced, by the share of the battle's groups in a
	// fight (air.intensity), eased over a few seconds: in from 0 at `in` to full at `full`, at its gainDb.
	int32 Groups = 0, Fighting = 0;
	for (const auto& Pair : S.Show.Groups())
	{
		if (Pair.second.Doing == AirShow::Act::Down || Pair.second.Doing == AirShow::Act::Idle)
		{
			continue;
		}
		++Groups;
		Fighting += Pair.second.Doing == AirShow::Act::Fight ? 1 : 0;
	}
	const double Share = Groups > 0 ? static_cast<double>(Fighting) / Groups : 0.0;
	// (rising over intensity.rise seconds, falling over intensity.fall: a fight calms slowly, as in a film)
	const double Tau = FMath::Max(0.05, AirParts::MixNumber(Mix, Share > AirIntensity ? TEXT("intensity.rise") : TEXT("intensity.fall"), Share > AirIntensity ? 1.5 : 10.0));
	AirIntensity += (Share - AirIntensity) * (1.0 - FMath::Exp(-FMath::Clamp(Real, 0.0, 0.25) / Tau));
	double In = 0.02, Full = 0.15, BedDb = -12.0;
	FString BedTake = TEXT("air_battle_far_loop");
	const TArray<TSharedPtr<FJsonValue>>* Beds = nullptr;
	if (const TSharedPtr<FJsonObject> BedsAt = AirParts::MixObject(Mix, TEXT("beds")); BedsAt.IsValid() && BedsAt->TryGetArrayField(TEXT("list"), Beds))
	{
		for (const TSharedPtr<FJsonValue>& Row : *Beds)
		{
			const TSharedPtr<FJsonObject>* Bed = nullptr;
			bool bAir = false;
			if (Row->TryGetObject(Bed) && (*Bed)->TryGetBoolField(TEXT("air"), bAir) && bAir)
			{
				(*Bed)->TryGetStringField(TEXT("name"), BedTake);
				(*Bed)->TryGetNumberField(TEXT("in"), In);
				(*Bed)->TryGetNumberField(TEXT("full"), Full);
				(*Bed)->TryGetNumberField(TEXT("gainDb"), BedDb);
				break;
			}
		}
	}
	const float BedLevel = static_cast<float>(FMath::Clamp((AirIntensity - In) / FMath::Max(0.01, Full - In), 0.0, 1.0) * FMath::Pow(10.0, BedDb / 20.0));
	if (!AirBed && BedLevel > 0.001f)
	{
		// (begun at its level, never at nothing: a sound begun silent can be set aside by the engine and not come back)
		AirBed = SoundAtEye(*BedTake, BedLevel, 0.0f, true, 2.0f);
	}
	if (AirBed)
	{
		AirBed->SetVolumeMultiplier(FMath::Max(0.0001f, BedLevel * MasterGain * DuckBedGain));
	}
	BattleScore(Real);
}

void AAirBattle::BattleScore(double Real)
{
	// The battle's music (mix.json music; Brian's stamps of 19 November): Zero Hour until first contact, crossfading into
	// the battle's piece at onContact.db; down to thickOfIt.db while the air battle's meter is over its mark, back to
	// lull.db when it has stayed under the lull's mark long enough; nearBreak.db while either side is three quarters of
	// the way to breaking off; dipped by a big moment; at the end the stinger, The Field Is Ours or The Line Gives Way.
	// (The map's moods wait while it plays: ACampaignGameMode::BeginBattleScore.)
	FAirBattleState& S = *State;
	ACampaignGameMode* Mode = GetWorld() ? Cast<ACampaignGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	if (!Mode || !Mix.IsValid())
	{
		return;
	}
	if (!bScoreBegun)
	{
		bScoreBegun = true;
		ScoreWantDb = ScoreShownDb = AirParts::MixNumber(Mix, TEXT("music.beforeContact.db"), 0.0);
		Mode->BeginBattleScore();
	}
	if (!bScoreContact && S.Desk.Head().Contact >= 0.0)
	{
		bScoreContact = true;
		ScoreWantDb = ScoreShownDb = AirParts::MixNumber(Mix, TEXT("music.onContact.db"), -8.0);
		Mode->BattleScoreContact(static_cast<float>(ScoreShownDb + DuckMusicDb), static_cast<float>(AirParts::MixNumber(Mix, TEXT("music.onContact.fromBeforeContact.crossfade"), 3.0)));
	}
	if (bScoreContact && !S.bOver)
	{
		double Want = ScoreWantDb, Fade = 3.0;
		double LullUnder = 0.3, LullFor = 15.0;
		const TArray<TSharedPtr<FJsonValue>>* Under = nullptr;
		if (const TSharedPtr<FJsonObject> Lull = AirParts::MixObject(Mix, TEXT("music.lull")); Lull.IsValid() && Lull->TryGetArrayField(TEXT("whenIntensityUnderFor"), Under) && Under->Num() == 2)
		{
			LullUnder = (*Under)[0]->AsNumber();
			LullFor = (*Under)[1]->AsNumber();
		}
		if (AirIntensity > AirParts::MixNumber(Mix, TEXT("music.thickOfIt.whenIntensityOver"), 0.6))
		{
			Want = AirParts::MixNumber(Mix, TEXT("music.thickOfIt.db"), -18.0);
			Fade = AirParts::MixNumber(Mix, TEXT("music.thickOfIt.fade"), 4.0);
			LullSince = -1.0;
		}
		else if (AirIntensity < LullUnder)
		{
			LullSince = LullSince < 0.0 ? RealT : LullSince;
			if (RealT - LullSince >= LullFor)
			{
				Want = AirParts::MixNumber(Mix, TEXT("music.lull.db"), -8.0);
				Fade = AirParts::MixNumber(Mix, TEXT("music.lull.fade"), 6.0);
			}
		}
		else
		{
			LullSince = -1.0;
		}
		// (the swing of the battle: either side three quarters of the way to breaking off)
		if (FMath::Max(S.Words.MyBar, S.Words.TheirBar) >= 0.75)
		{
			Want = AirParts::MixNumber(Mix, TEXT("music.nearBreak.db"), -4.0);
			Fade = AirParts::MixNumber(Mix, TEXT("music.nearBreak.fade"), 3.0);
		}
		if (FMath::Abs(Want - ScoreWantDb) > 0.01)
		{
			ScoreRampDbPerS = FMath::Abs(Want - ScoreShownDb) / FMath::Max(0.1, Fade);
			ScoreWantDb = Want;
		}
	}
	// (the level eased toward its mark at the rule's own fade, in straight dB)
	const double Step = ScoreRampDbPerS * FMath::Clamp(Real, 0.0, 0.25);
	ScoreShownDb = ScoreShownDb < ScoreWantDb ? FMath::Min(ScoreWantDb, ScoreShownDb + Step) : FMath::Max(ScoreWantDb, ScoreShownDb - Step);
	if (!S.bOver)
	{
		Mode->SetBattleScoreLevel(static_cast<float>(ScoreShownDb + DuckMusicDb), 0.1f);
	}
	if (S.bOver && !bScoreEnded && S.OverRealT >= AirParts::MixNumber(Mix, TEXT("music.end.stingerAfter"), 1.0))
	{
		bScoreEnded = true;
		const int32 Won = AirFeed::SideOf(S.Desk.Winner()); // (release 1.5: the after card's side once it is read)
		Mode->EndBattleScore(Won < 0 ? -1 : Won == S.Desk.MySideIndex() ? 1 : 0);
	}
}

FString AAirBattle::EngineLoop(const FString& Model)
{
	// An aircraft's own engines by its model (mix.json air.loops): the B-17 and the B-24 their own drones, the Stuka
	// its own; other four-engined and twin-engined bombers theirs; every fighter, and the carriers' single-engined
	// bombers, which have no recording of their own, the fighter's. A take not yet in the game gives way to its kind's.
	const TSharedPtr<FJsonObject> Loops = AirParts::MixObject(Mix, TEXT("air.loops"));
	if (!Loops.IsValid())
	{
		return FString();
	}
	const bool bFour = Model == TEXT("Lancaster") || Model == TEXT("Halifax") || Model == TEXT("B24");
	// (the SM.79's three engines among the twins, as mix.json's note has her; the plain bomber in any nation's paint,
	// "Bomber:UK", and the rules' new bombers, 23 November, before or after their models are in)
	const bool bTwin = Model == TEXT("He111") || Model == TEXT("Betty") || Model == TEXT("G4M") || Model == TEXT("Nell") || Model == TEXT("G3M")
		|| Model == TEXT("SM79") || Model == TEXT("SM.79") || Model == TEXT("Wellington") || Model == TEXT("Pe2") || Model == TEXT("Pe-2")
		|| Model == TEXT("LeO451") || Model == TEXT("LeO 451") || Model.StartsWith(TEXT("Bomber"));
	const TCHAR* Kind = bFour ? TEXT("fourEngined") : bTwin ? TEXT("twinEngined") : TEXT("fighter");
	const TCHAR* Own = Model == TEXT("B17") ? TEXT("B-17") : Model == TEXT("B24") ? TEXT("B-24") : Model == TEXT("Stuka") ? TEXT("Ju 87") : nullptr;
	FString Loop;
	if (Own && Loops->TryGetStringField(Own, Loop) && SoundOf(*Loop, true))
	{
		return Loop;
	}
	Loop.Reset();
	Loops->TryGetStringField(Kind, Loop);
	return Loop;
}

void AAirBattle::EngineSounds()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_EngineSounds);
	FAirBattleState& S = *State;
	const TSharedPtr<FJsonObject> Loops = AirParts::MixObject(Mix, TEXT("air.loops"));
	if (!Loops.IsValid())
	{
		return;
	}
	// Each formation's own engines at its middle, by its model (mix.json air.loops, SKY.cues.engines): the B-17 its
	// own drone; four-engined and twin-engined bombers theirs; every fighter, and the carriers' single-engined
	// bombers, which have no recording of their own, the fighter's.
	// (its drone as its engines stand, air.engineLost: 10 log10 of the engines turning over the most it has flown with,
	// so a box that loses aircraft and engines grows thinner; a formation with none left, a whisper)
	TMap<int32, int32> Turning;
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		if (P.bShown && !P.bFalling)
		{
			int32 Left = 0, All = 1;
			AirParts::LiveEngines(P.Model, P.bFighter, P.DeadEngines, Left, All);
			Turning.FindOrAdd(P.Group) += Left;
		}
	}
	for (const auto& Pair : S.Show.Groups())
	{
		const AirShow::Group& G = Pair.second;
		TObjectPtr<UAudioComponent>* Have = Engines.Find(G.Id);
		AirShow::Vec Middle;
		double Face = 0.0;
		const bool bFlying = G.Doing != AirShow::Act::Down && G.Doing != AirShow::Act::Idle && S.Show.Middle(G.Id, Middle, Face);
		if (!bFlying)
		{
			if (Have && *Have)
			{
				(*Have)->FadeOut(2.0f, 0.0f);
				Engines.Remove(G.Id);
				EngineKind.Remove(G.Id);
			}
			continue;
		}
		const FString Loop = EngineLoop(AirParts::Wide(G.Model));
		if (Have)
		{
			const FName* Was = EngineKind.Find(G.Id);
			if (!Was || *Was != FName(*Loop))
			{
				// (a group first seen with the place's stand-in model, its own named since: its own engines)
				if (*Have)
				{
					(*Have)->FadeOut(1.0f, 0.0f);
				}
				Engines.Remove(G.Id);
				EngineKind.Remove(G.Id);
				Have = nullptr;
			}
		}
		if (!Have)
		{
			USoundBase* Played = Loop.IsEmpty() ? nullptr : SoundOf(*Loop, true);
			if (!Played)
			{
				EngineKind.Add(G.Id, FName(*Loop));
				Engines.Add(G.Id, nullptr);
				continue;
			}
			// (in the mix's family for formation loops, air.loopsFamily: heard out to 14 km, where the aircraft family is
			// silent at 6. Past it, not begun, and tried again as the formation comes nearer: before, a formation that
			// began out of earshot stayed silent all battle)
			FString LoopsFamily = TEXT("air_far");
			if (const TSharedPtr<FJsonObject> Air = AirParts::MixObject(Mix, TEXT("air")))
			{
				Air->TryGetStringField(TEXT("loopsFamily"), LoopsFamily);
			}
			USoundAttenuation* Family = FamilyOf(*LoopsFamily);
			const FVector Where = World(Middle.X, Middle.Y, Middle.Z);
			const float Loud = static_cast<float>(FMath::Pow(10.0, AirParts::MixNumber(Mix, TEXT("air.groupLoopDb"), 0.0) / 20.0)) * MasterGain * DuckLoopsGain;
			UAudioComponent* Player = Lay(Played, &Where, Family ? Family : FamilyOf(TEXT("aircraft")), Loud, 0.0f, 2.0f,
				G.bFighter ? 1.0f : 0.97f + 0.06f * static_cast<float>(G.Id % 3) / 2.0f, false);
			if (Player)
			{
				EngineKind.Add(G.Id, FName(*Loop));
				Engines.Add(G.Id, Player);
			}
			continue;
		}
		if (*Have)
		{
			(*Have)->SetWorldLocation(World(Middle.X, Middle.Y, Middle.Z));
			const int32 Spinning = Turning.FindRef(G.Id);
			int32& Most = GroupEnginesMost.FindOrAdd(G.Id);
			Most = FMath::Max(Most, Spinning);
			const float Live = Most > 0 ? FMath::Max(0.3f, FMath::Sqrt(static_cast<float>(Spinning) / static_cast<float>(Most))) : 1.0f;
			(*Have)->SetVolumeMultiplier(static_cast<float>(FMath::Pow(10.0, AirParts::MixNumber(Mix, TEXT("air.groupLoopDb"), 0.0) / 20.0)) * MasterGain * DuckLoopsGain * Live);
		}
	}
}

// ---- The picture run -------------------------------------------------------------------------------------------

void AAirBattle::StrategyShotLog(const FString& Name) const
{
	// (what the strategy picture framed: the art director measures its aircraft; this says where they stood from the
	// look, so a picture of empty sky can be told from a look on aircraft hidden in cloud or too small to see)
	if (!State.IsValid() || !Eye)
	{
		return;
	}
	const double U = UnitsPerMetre;
	int32 Shown = 0, Within = 0;
	double Nearest = 1e18;
	for (const AirShow::PlaneOut& P : State->Show.Planes())
	{
		if (!P.bShown || P.bFalling || P.bOnGround)
		{
			continue;
		}
		++Shown;
		const double Far = FVector::Dist(World(P.At.X, P.At.Y, P.At.Z), LookAt) / U;
		Nearest = FMath::Min(Nearest, Far);
		Within += Far < 600.0 ? 1 : 0;
	}
	const double Pixels = StrategySpan > 0.0 ? StrategySpan * 1920.0 / (2.0 * FMath::Tan(FMath::DegreesToRadians(27.5)) * FMath::Max(1.0, StrategyAway)) : 0.0;
	UE_LOG(LogTheater, Display, TEXT("Air battle: strategy picture %s: %d groups framed, %.0f m off for a middle span of %.1f m (about %.0f pixels across at 1920); of %d aircraft shown the nearest %.0f m from the look, %d within 600 m of it; the eye %.0f m up."),
		*Name, StrategyGroups, StrategyAway, StrategySpan, Pixels, Shown, Nearest > 1e17 ? -1.0 : Nearest, Within, Eye->GetActorLocation().Z / U);
}

void AAirBattle::TakeShots(double Real)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AirBattle_TakeShots);
	(void)Real;
	if (ShotFolder.IsEmpty() || QuitAt >= 0.0)
	{
		return;
	}
	FAirBattleState& S = *State;
	// The flyover's own pictures, one in each of its shots, on its own clock.
	if (S.bFlying && S.GlideT < 0.0)
	{
		const double RaidSeconds = AirDesk::Flyover::RaidLong, TargetSeconds = AirDesk::Flyover::TargetLong;
		const struct { const TCHAR* Name; double At; bool bGuard; } FlyShots[] = {
			{ TEXT("00-flyover-raid"), 3.0, false }, { TEXT("00-flyover-target"), RaidSeconds + 2.5, false }, { TEXT("00-flyover-guard"), RaidSeconds + TargetSeconds + 2.0, true } };
		for (int32 K = 0; K < 3; ++K)
		{
			if ((S.FlyPictures & (1 << K)) == 0 && S.FlyT >= FlyShots[K].At && (!FlyShots[K].bGuard || S.Fly.GuardGroup() >= 0))
			{
				S.FlyPictures |= 1 << K;
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, AirParts::Wide(S.Recording.Key) + TEXT("-") + FlyShots[K].Name + TEXT(".png")), true, false);
				if (FilmCritic::On()) { FilmCritic::Still(FlyShots[K].Name); } // film critic: the still's camera (and a strip after it)
				++ShotsTaken;
			}
		}
	}
	// (the parachutes picture's moment: three or more open canopies shown within 1.5 km of one of them)
	if (ChutesRealT < 0.0)
	{
		TArray<FVector> Open;
		for (const AirShow::Thing& T : S.Show.Things())
		{
			if (T.Kind == AirShow::ThingKind::Chute && T.bOpen)
			{
				Open.Add(World(T.At.X, T.At.Y, T.At.Z));
			}
		}
		for (const FVector& One : Open)
		{
			FVector Sum = FVector::ZeroVector;
			int32 Count = 0;
			for (const FVector& Other : Open)
			{
				if (FVector::Dist(One, Other) < 1500.0 * UnitsPerMetre)
				{
					Sum += Other;
					++Count;
				}
			}
			if (Count >= 3)
			{
				ChutesRealT = RealT;
				ChutesWorld = Sum / Count;
				break;
			}
		}
	}
	for (FShot& Shot : Shots)
	{
		if (Shot.bTaken)
		{
			continue;
		}
		const double Base = Shot.bGuns ? GunsRealT : Shot.bShips ? ShipRealT : Shot.bChutes ? ChutesRealT : MetRealT;
		if (Base < 0.0)
		{
			// (the approach is taken before the formations meet: the clock's x10 brings them together in some 20 s;
			// after the flyover, when there is one)
			if (Shot.At < 0.0 && !Shot.bShips && !Shot.bGuns && !S.bFlying && RealT >= FMath::Max(8.0, S.FlyEndRealT + 4.0))
			{
				Shot.bTaken = true;
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, AirParts::Wide(S.Recording.Key) + TEXT("-") + Shot.Name + TEXT(".png")), true, false);
				if (FilmCritic::On()) { FilmCritic::Still(Shot.Name); } // film critic: the still's camera (and a strip after it)
				++ShotsTaken;
				StrategyShotLog(Shot.Name);
			}
			continue;
		}
		const double Due = Base + Shot.At;
		if (((Shot.bClose && ShipsLook >= 0) || (!Shot.bGuns && GunsLook >= 0) || (Shot.bGuns && ShipsLook >= 0) || (!Shot.bChutes && bChutesLook)
			|| (Shot.bChutes && (ShipsLook >= 0 || GunsLook >= 0))) && RealT >= Due - 1.5)
		{
			// (the ships picture holds the eye: a ride picture due now waits until two seconds after it; the guns fire
			// back picture holds it from every other, and waits for the ships picture)
			Shot.At = RealT + 2.0 - Base;
			continue;
		}
		if (Shot.bScrew && ScrewEyeKey == INDEX_NONE && RealT >= Due - 1.5)
		{
			// (an aircraft flying with an engine out, a bomber before a fighter: its airscrew windmilling, or stopped
			// edge-on when feathered; none yet, it waits, and after 400 real seconds of none there is no picture)
			const AirShow::PlaneOut* Pick = nullptr;
			for (const AirShow::PlaneOut& P : S.Show.Planes())
			{
				if (P.bShown && !P.bFalling && !P.bOnGround && !P.DeadEngines.empty() && (!Pick || (Pick->bFighter && !P.bFighter)))
				{
					Pick = &P;
				}
			}
			if (!Pick)
			{
				if (RealT > Base + 400.0)
				{
					Shot.bTaken = true;
					UE_LOG(LogTheater, Display, TEXT("Air battle: dead engine picture: no aircraft flew on with an engine out, so none taken."));
				}
				else
				{
					Shot.At = RealT + 2.0 - Base;
				}
				continue;
			}
			int32 Left = 0, All = 0;
			AirParts::LiveEngines(Pick->Model, Pick->bFighter, Pick->DeadEngines, Left, All);
			// (engines numbered from the left, as the B-17's 1 to 4 are: the first dead one's side)
			ScrewEyeKey = Pick->Key;
			ScrewEyeHand = All > 1 && Pick->DeadEngines.front() * 2 <= All ? -1.0 : 1.0;
			RideOn(Pick->Group, Pick->Key);
			CloseOffAt = -1.0; // (a ride held from an earlier picture must not take this one off before it is taken)
			UE_LOG(LogTheater, Display, TEXT("Air battle: dead engine picture on %s (key %d): engine %d of %d out, its airscrew %s; the eye on its %s."),
				*AirParts::Wide(Pick->Model), Pick->Key, Pick->DeadEngines.front(), All, Pick->bFeathered ? TEXT("stopped") : TEXT("windmilling"), ScrewEyeHand > 0.0 ? TEXT("right") : TEXT("left"));
		}
		if (Shot.bChutes && !bChutesLook && RealT >= Due - 2.0)
		{
			if (S.bClose)
			{
				Press(TEXT("ride"));
				CloseOffAt = -1.0;
			}
			bChutesLook = true;
			bLookSet = false; // (a cut to them)
		}
		if (Shot.bClose && Shot.bAce && !S.bClose && RealT >= Due - 1.5)
		{
			// (an ace's lead still flying, the first ace's; with none, as in a battle with no ace and no test ace, no
			// pictures: the art director's two frames of his marks, close and from the chase view)
			int32 AceGroup = INDEX_NONE, AceKey = INDEX_NONE;
			for (const TPair<int32, int32>& Lead : AceLead)
			{
				for (const AirShow::PlaneOut& P : S.Show.Planes())
				{
					if (AceKey == INDEX_NONE && P.Key == Lead.Value && P.bShown && !P.bFalling && !P.bOnGround)
					{
						AceGroup = Lead.Key;
						AceKey = P.Key;
					}
				}
			}
			if (AceKey == INDEX_NONE)
			{
				Shot.bTaken = true;
				UE_LOG(LogTheater, Display, TEXT("Air battle: ace picture %s: no ace's aircraft flying, so none taken."), *Shot.Name);
				continue;
			}
			RideOn(AceGroup, AceKey);
			AceEyeKey = Shot.bChase ? INDEX_NONE : AceKey;
			CloseOffAt = -1.0;
			UE_LOG(LogTheater, Display, TEXT("Air battle: ace picture %s on key %d (group %d), %s."), *Shot.Name, AceKey, AceGroup,
				Shot.bChase ? TEXT("the chase view behind him") : TEXT("the eye 8 m off his marks"));
		}
		else if (Shot.bClose && !S.bClose && RealT >= Due - 1.5)
		{
			bRideOther = Shot.bOther;
			int Squadron = -1;
			if (Shot.bChase)
			{
				// (as he would take it: a double click on the tag of his first fighter squadron up, else his first,
				// then one step on along it, so the plate reads its second aircraft)
				for (const int Id : AirDesk::RideSquadrons(S.Words.Tags, S.Show.Planes()))
				{
					const AirDesk::Tag* Tag = S.TagOf(Id);
					if (Tag && Tag->bMine && (Squadron < 0 || (Tag->bFighter && !S.TagOf(Squadron)->bFighter)))
					{
						Squadron = Id;
					}
				}
			}
			const int Leader = Squadron >= 0 ? AirDesk::RideStep(S.Show.Planes(), Squadron, -1, 0) : -1;
			const int32 Marked = Shot.bMarks ? MostMarked() : INDEX_NONE;
			if (Marked != INDEX_NONE)
			{
				for (const AirShow::PlaneOut& P : S.Show.Planes())
				{
					if (P.Key == Marked)
					{
						RideOn(P.Group, P.Key);
						MarksEyeKey = P.Key;
						UE_LOG(LogTheater, Display, TEXT("Air battle: strike marks picture on %s (key %d): %d strikes on it, %d laid near the eye, %d kept in all."),
							*AirParts::Wide(P.Model), P.Key, PlaneMade.Contains(P.Key) ? PlaneMade[P.Key].Marks.Num() : 0, MarkDecals, MarkStrikes);
					}
				}
			}
			else if (Leader >= 0)
			{
				RideOn(Squadron, Leader);
				Press(TEXT("ride:next"));
			}
			else
			{
				Press(TEXT("ride"));
			}
		}
		if (Shot.bShips && ShipsLook < 0 && GunsLook < 0 && FirstSunk >= 0 && RealT >= Due - 2.0)
		{
			if (S.bClose)
			{
				Press(TEXT("ride"));
				CloseOffAt = -1.0; // (the ride is off: its old time must not take the next ride off a tick after it begins)
			}
			ShipsLook = FirstSunk;
		}
		if (Shot.bGuns && GunsLook < 0 && ShipsLook < 0 && GunsShip >= 0 && RealT >= Due - 2.0)
		{
			if (S.bClose)
			{
				Press(TEXT("ride"));
				CloseOffAt = -1.0;
			}
			GunsLook = GunsShip;
		}
		if (RealT >= Due)
		{
			Shot.bTaken = true;
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, AirParts::Wide(S.Recording.Key) + TEXT("-") + Shot.Name + TEXT(".png")), true, false);
			if (FilmCritic::On()) { FilmCritic::Still(Shot.Name); } // film critic: the still's camera (and a strip after it)
			++ShotsTaken;
			if (!Shot.bClose && !Shot.bShips && !Shot.bGuns && !Shot.bChutes)
			{
				StrategyShotLog(Shot.Name);
			}
			if (Shot.bClose)
			{
				CloseOffAt = RealT + RideHold;
			}
			if (Shot.bShips)
			{
				ShipsLookOffAt = RealT + 0.5;
			}
			if (Shot.bGuns)
			{
				GunsLookOffAt = RealT + 0.5;
			}
			if (Shot.bChutes)
			{
				ChutesLookOffAt = RealT + 0.5;
				int32 Hung = 0;
				for (const AirShow::Thing& T : S.Show.Things())
				{
					if (T.Kind == AirShow::ThingKind::Chute && T.bOpen && FVector::Dist(World(T.At.X, T.At.Y, T.At.Z), ChutesWorld) < 1500.0 * UnitsPerMetre)
					{
						++Hung;
					}
				}
				const APlayerCameraManager* Cam = GetWorld() && GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->PlayerCameraManager : nullptr;
				UE_LOG(LogTheater, Display, TEXT("Air battle: parachutes picture: %d open canopies within 1.5 km of the look, the eye %.0f m from their middle."),
					Hung, Cam ? FVector::Dist(Cam->GetCameraLocation(), ChutesWorld) / UnitsPerMetre : -1.0);
			}
		}
	}
	if (ChutesLookOffAt >= 0.0 && RealT >= ChutesLookOffAt)
	{
		ChutesLookOffAt = -1.0;
		bChutesLook = false;
		bLookSet = false; // (a cut back to the fight)
	}
	if (GunsLookOffAt >= 0.0 && RealT >= GunsLookOffAt)
	{
		GunsLookOffAt = -1.0;
		GunsLook = -1;
		GunsEyeFor = -1;
		bLookSet = false; // (a cut back to the fight)
	}
	if (ShipsLookOffAt >= 0.0 && RealT >= ShipsLookOffAt)
	{
		ShipsLookOffAt = -1.0;
		ShipsLook = -1;
		ShipsEyeFor = -1;
		bLookSet = false; // (a cut back to the fight)
	}
	if (CloseOffAt >= 0.0 && RealT >= CloseOffAt)
	{
		CloseOffAt = -1.0;
		MarksEyeKey = INDEX_NONE;
		ScrewEyeKey = INDEX_NONE;
		AceEyeKey = INDEX_NONE;
		if (S.bClose)
		{
			Press(TEXT("ride"));
		}
	}
	// The after card, then quit (or once the battle has run long past the last picture).
	if (S.bCard && !bCardShot)
	{
		bCardShot = true;
		CardShotAt = RealT + 1.0;
	}
	if (bCardShot && CardShotAt >= 0.0 && RealT >= CardShotAt)
	{
		CardShotAt = -1.0;
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, AirParts::Wide(S.Recording.Key) + TEXT("-08-after-card.png")), true, false);
		if (FilmCritic::On()) { FilmCritic::Still(TEXT("08-after-card")); } // film critic: the still's camera
		++ShotsTaken;
		QuitAt = RealT + 2.0;
	}
	if (RealT > 1500.0)
	{
		QuitAt = RealT + 1.0;
	}
}
