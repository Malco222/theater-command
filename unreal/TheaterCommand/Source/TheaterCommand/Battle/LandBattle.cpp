#include "Battle/LandBattle.h"
#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Async/ParallelFor.h"

#include "Battle/AirSky.h"
#include "Battle/LandBattleScreen.h"
#include "Battle/LandBattleState.h"
#include "Battle/LandBench.h"
#include "Battle/LandCarpet.h"
#include "Battle/LandFell.h"
#include "Battle/LandLive.h"
#include "Battle/LandCrowd.h" // TK1
#include "Battle/LandScape.h"
#include "Battle/LandShade.h"
#include "Battle/LandVat.h" // SO1
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignBattleLook.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignFlags.h"
#include "Campaign/CampaignHUD.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignTextures.h"
#include "Campaign/TheaterSpeed.h"
#include "Components/AudioComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "FrameGrabber.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShaderCompiler.h"
#include "Slate/SceneViewport.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "RHI.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "StaticMeshAttributes.h"
#include "SubmixEffects/AudioMixerSubmixEffectDynamicsProcessor.h"
#include "StaticMeshResources.h"
#include "TheaterCommand.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SDPIScaler.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>


// The scene's own tick split by name for the gate (the production lead, 22 Nov 18:41Z: 24 ms a frame of the lit base's
// ALandBattle::Tick had no name under it): each block in an Insights trace (cpu) and in the speed log's and the frame
// recorder's "Theater" columns.
#define LAND_SCOPE(Name) TRACE_CPUPROFILER_EVENT_SCOPE(Land_##Name); THEATER_SPEED_SCOPE(Land##Name)

namespace LandParts
{
	static FString Wide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	/**
	 * The ground battle's grade: the battles' own (BattleLook::Grade, which the air battle keeps as it is), then cooler
	 * and drained further for a war film's daylight. The art director on the second cut's stills (21 November 21:36Z,
	 * her first item, "one switch, every shot: the biggest lift"): with red 1.08 and blue 0.87 the frames measured
	 * saturation 0.30 to 0.48 and blue under red by 37 to 62, wheat orange-gold and grass lime; she asks red 1.00, blue
	 * 1.02, the middle tones' colour 0.45 to 0.50 and the darks' 0.45, for saturation 0.15 to 0.25 and blue under red
	 * by 15 to 0. A recording may set its own (sky.grade: the steppe's July morning is warmer, its wheat and dust gold
	 * under the haze); "-TheaterLandGrade=darks,middles,red,blue,stops" tries other numbers over either (leave one
	 * empty to keep it); "-TheaterNoBattleGrade" leaves both off.
	 */
	static void Grade(FPostProcessSettings& Lens, const LandFeed::Sky* Sky = nullptr, const LandScape::Air* Look = nullptr)
	{
		BattleLook::Grade(Lens);
		if (FParse::Param(FCommandLine::Get(), TEXT("TheaterNoBattleGrade")))
		{
			return;
		}
		// (and half a stop down: the art director on the third cut's field sheets, 22 November: the stop brings hay,
		// wheat, barley and meadow into 120-165 and keeps the contrast's shadows, where a softer contrast flattens them)
		double Darks = 0.45, Middles = 0.48, Red = 1.0, Blue = 1.02, Stops = -0.5;
		// (a ground other than the bocage carries its own grade, the art director's per theatre (theatres-sheet-rulings-
		// 1122.md item 1): its contrast, stop and toe always, its colour where the recording gives none)
		double Contrast = -1.0, Toe = -1.0;
		if (Look && Look->bSet)
		{
			Darks = Look->Grade[0];
			Middles = Look->Grade[1];
			Red = Look->Grade[2];
			Blue = Look->Grade[3];
			Stops = Look->Stops;
			Contrast = Look->Contrast;
			Toe = Look->Toe;
		}
		if (Sky && Sky->bGrade)
		{
			Darks = Sky->Grade[0];
			Middles = Sky->Grade[1];
			Red = Sky->Grade[2];
			Blue = Sky->Grade[3];
		}
		FString Asked;
		// (read past its commas, as LandLook::Asked reads its own: the engine's reader stops at the first one unless told
		// not to, so only the darks were ever taken; the effects thread's find on cut 5)
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandGrade="), Asked, false))
		{
			TArray<FString> Parts;
			Asked.ParseIntoArray(Parts, TEXT(","), false);
			double* Into[] = { &Darks, &Middles, &Red, &Blue, &Stops };
			for (int32 K = 0; K < Parts.Num() && K < 5; ++K)
			{
				if (!Parts[K].TrimStartAndEnd().IsEmpty())
				{
					*Into[K] = FCString::Atod(*Parts[K]);
				}
			}
		}
		Lens.ColorSaturationShadows = FVector4(Darks, Darks, Darks, 1.0);
		Lens.ColorSaturationMidtones = FVector4(Middles, Middles, Middles, 1.0);
		Lens.ColorGain = FVector4(Red, 1.0, Blue, 1.0);
		Lens.bOverride_AutoExposureBias = true;
		Lens.AutoExposureBias = static_cast<float>(Stops);
		if (Contrast > 0.0)
		{
			Lens.bOverride_ColorContrast = true;
			Lens.ColorContrast = FVector4(Contrast, Contrast, Contrast, 1.0);
		}
		if (Toe > 0.0)
		{
			Lens.bOverride_FilmToe = true;
			Lens.FilmToe = static_cast<float>(Toe);
		}
		static bool bSaid = false;
		if (!bSaid)
		{
			bSaid = true;
			UE_LOG(LogTheater, Display, TEXT("Land battle look: the ground battle's grade over the battles' own: colour kept in the darks %.2f, the middle tones %.2f; red %.2f and blue %.2f; %.2f stops; contrast %s, toe %s"),
				Darks, Middles, Red, Blue, Stops, Contrast > 0.0 ? *FString::Printf(TEXT("%.2f (the ground's)"), Contrast) : TEXT("the battles' own"),
				Toe > 0.0 ? *FString::Printf(TEXT("%.2f (the ground's)"), Toe) : TEXT("Unreal's own"));
		}
	}

	static std::string Narrow(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		return std::string(Utf8.Get(), static_cast<size_t>(Utf8.Length()));
	}

	/** The looks on trial ("-TheaterLandTrial=church,walltop,..." or "=all"), taken loosely as the weather word is:
	 * "Church, Walltop" reads as "church,walltop". Empty: none asked. */
	static std::string TrialWords()
	{
		FString Trial;
		if (!FParse::Value(FCommandLine::Get(), TEXT("TheaterLandTrial="), Trial, false))
		{
			return std::string();
		}
		return Narrow(Trial.Replace(TEXT(" "), TEXT("")).Replace(TEXT("\t"), TEXT("")).ToLower());
	}

	/** Whether that look is on trial (LandField::Field::Tried's reading of the words). */
	static bool OnTrial(const char* Word)
	{
		LandField::Field F;
		F.Trial = TrialWords();
		return !F.Trial.empty() && F.Tried(Word);
	}

	static double Seconds()
	{
		return FPlatformTime::Seconds();
	}

	/** A number from the mix, at a path of fields ("buses.loops.gainDb"), or Default. */
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

	static float Gain(double Db)
	{
		return static_cast<float>(FMath::Pow(10.0, Db / 20.0));
	}

	/** The haze's colour as this engine names it. */
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

	/** The month of a recording's date ("July 1944"), 7 if none is named. */
	static int MonthOf(const std::string& Date)
	{
		static const char* Names[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
		std::string Low;
		for (const char Ch : Date)
		{
			Low += static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
		}
		for (int K = 0; K < 12; ++K)
		{
			if (Low.find(Names[K]) != std::string::npos)
			{
				return K + 1;
			}
		}
		return 7;
	}

	/**
	 * The way toward the sun in the field's frame (x east, y north, z up) for a place, a month (the middle of it) and a
	 * local solar hour: the sun's declination for the day, its hour angle, its height and its bearing. Good to a degree
	 * or two, which is all a picture asks.
	 */
	static FVector SunWay(double Lat, int Month, double Hour)
	{
		const double Day = (Month - 1) * 30.4 + 15.0;
		const double Decl = FMath::DegreesToRadians(23.44) * FMath::Sin(2.0 * PI * (284.0 + Day) / 365.0);
		const double Phi = FMath::DegreesToRadians(Lat);
		const double H = FMath::DegreesToRadians(15.0 * (Hour - 12.0));
		const double SinAlt = FMath::Sin(Phi) * FMath::Sin(Decl) + FMath::Cos(Phi) * FMath::Cos(Decl) * FMath::Cos(H);
		const double Alt = FMath::Asin(FMath::Clamp(SinAlt, -1.0, 1.0));
		// (bearing from north, clockwise: east in the morning, west after noon)
		const double CosAz = (FMath::Sin(Decl) - FMath::Sin(Alt) * FMath::Sin(Phi)) / FMath::Max(1e-6, FMath::Cos(Alt) * FMath::Cos(Phi));
		double Az = FMath::Acos(FMath::Clamp(CosAz, -1.0, 1.0));
		if (H > 0.0)
		{
			Az = 2.0 * PI - Az;
		}
		// (never under 8 degrees: a battle by day keeps a sun over the hedges)
		const double Up = FMath::Max(Alt, FMath::DegreesToRadians(8.0));
		return FVector(FMath::Sin(Az) * FMath::Cos(Up), FMath::Cos(Az) * FMath::Cos(Up), FMath::Sin(Up));
	}

	/** A mesh of the ground's (field frame, metres) as the engine takes it: the field's x and y swapped (x east is
	 * Unreal's Y), which keeps each triangle facing the same way. */
	static void FromField(const LandField::Mesh& M, double U, TArray<FVector>& P, TArray<FVector>& N, TArray<FColor>& C, TArray<int32>& T)
	{
		const int32 Count = static_cast<int32>(M.Points());
		P.SetNumUninitialized(Count);
		N.SetNumUninitialized(Count);
		C.SetNumUninitialized(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const float* At = &M.Positions[static_cast<size_t>(I) * 3];
			const float* Way = &M.Normals[static_cast<size_t>(I) * 3];
			P[I] = FVector(At[1] * U, At[0] * U, At[2] * U);
			N[I] = FVector(Way[1], Way[0], Way[2]);
			const LandField::Rgb8& Paint = M.Colours[static_cast<size_t>(I)];
			C[I] = FColor(Paint.R, Paint.G, Paint.B, 255);
		}
		T.SetNumUninitialized(static_cast<int32>(M.Triangles.size()));
		for (int32 I = 0; I < T.Num(); ++I)
		{
			T[I] = static_cast<int32>(M.Triangles[static_cast<size_t>(I)]);
		}
	}

	/** One of the ground's things (LandField's base meshes: a hedge shrub, a tree, a gate, a tuft of grass) as a mesh of
	 * its own for instancing: its near level the first level of detail, its far (where it has one) the second, from the
	 * distance the field gives on (FarFrom). Points as FromField takes them (the field's x and y swapped into Unreal's
	 * frame, which keeps each triangle facing the same way); colours as sRGB bytes, the base's paint by the shade's tint
	 * (128 as built). */
	/** A thing's level of detail past its near one (GS9): its mesh, from how many metres off at the usual 50 deg lens,
	 * and whether it casts a shadow there. */
	struct FThingLevel
	{
		const LandField::Mesh* Mesh = nullptr;
		double From = 0.0;
		bool bShadow = true;
	};

	/** The share of the screen's height a thing of this bounds radius (Unreal's units) fills Metres off at the usual 50
	 * deg vertical lens: what Unreal's level of detail goes by (ComputeBoundsScreenSize: the projection's larger scale,
	 * the vertical one, over the distance, times two). */
	static float ScreenAt(double Radius, double Metres, double U)
	{
		const double Lens = 1.0 / FMath::Tan(FMath::DegreesToRadians(25.0));
		return static_cast<float>(Lens * Radius / FMath::Max(1.0, Metres * U));
	}

	static UStaticMesh* ThingMesh(UObject* Owner, const FName& Name, const LandField::BaseMesh& Base, const FVector3f& Tint, UMaterialInterface* Ink, UMaterialInterface* LeafInk, double U,
		const TArray<FThingLevel>& Further)
	{
		TArray<const LandField::Mesh*> Levels;
		Levels.Add(&Base.Near);
		for (const FThingLevel& Level : Further)
		{
			Levels.Add(Level.Mesh);
		}
		// The near level's leaves (a hedge shrub's, a tree's): on their cards with the leaves' material, its second
		// section; without that material, the solid sprigs in the first, on the paint.
		const bool bCards = LeafInk && !Base.Cards.Empty() && Base.Cards.Uvs.size() == 2 * Base.Cards.Points();
		const LandField::Mesh* Leaves = bCards ? &Base.Cards : Base.Sprigs.Empty() ? nullptr : &Base.Sprigs;
		TArray<FMeshDescription> Descriptions;
		Descriptions.SetNum(Levels.Num());
		for (int32 L = 0; L < Descriptions.Num(); ++L)
		{
			FMeshDescription& D = Descriptions[L];
			FStaticMeshAttributes Attributes(D);
			Attributes.Register();
			auto Positions = Attributes.GetVertexPositions();
			auto Normals = Attributes.GetVertexInstanceNormals();
			auto Tangents = Attributes.GetVertexInstanceTangents();
			auto Signs = Attributes.GetVertexInstanceBinormalSigns();
			auto Colours = Attributes.GetVertexInstanceColors();
			auto Uvs = Attributes.GetVertexInstanceUVs();
			Uvs.SetNumChannels(1);
			// (the near level: its body on the paint, then its leaves, on their cards in a section of their own or the
			// sprigs in the body's)
			const LandField::Mesh* Parts[2] = { Levels[L], L == 0 ? Leaves : nullptr };
			const FPolygonGroupID Group = D.CreatePolygonGroup();
			Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Ink");
			FPolygonGroupID LeafGroup = Group;
			if (L == 0 && bCards)
			{
				LeafGroup = D.CreatePolygonGroup();
				Attributes.GetPolygonGroupMaterialSlotNames()[LeafGroup] = TEXT("Leaves");
			}
			for (int32 Part = 0; Part < 2; ++Part)
			{
				if (!Parts[Part])
				{
					continue;
				}
				const LandField::Mesh& M = *Parts[Part];
				const bool bOnCards = Part == 1 && bCards;
				const int32 Count = static_cast<int32>(M.Points());
				D.ReserveNewVertices(Count);
				D.ReserveNewVertexInstances(Count);
				D.ReserveNewTriangles(static_cast<int32>(M.Tris()));
				TArray<FVertexInstanceID> Corners;
				Corners.SetNum(Count);
				for (int32 I = 0; I < Count; ++I)
				{
					const size_t K = static_cast<size_t>(I);
					const float* At = &M.Positions[K * 3];
					const float* Way = &M.Normals[K * 3];
					const FVertexID Vertex = D.CreateVertex();
					Positions[Vertex] = FVector3f(At[1] * U, At[0] * U, At[2] * U);
					const FVertexInstanceID Corner = D.CreateVertexInstance(Vertex);
					const FVector3f Normal = FVector3f(Way[1], Way[0], Way[2]).GetSafeNormal(1e-6f, FVector3f::UpVector);
					Normals[Corner] = Normal;
					// (no normal map: any tangent across the normal will do)
					const FVector3f Across = FMath::Abs(Normal.Z) < 0.9f ? FVector3f::UpVector : FVector3f::ForwardVector;
					Tangents[Corner] = FVector3f::CrossProduct(Across, Normal).GetSafeNormal(1e-6f, FVector3f::ForwardVector);
					Signs[Corner] = 1.0f;
					const LandField::Rgb8& Paint = M.Colours[K];
					const FColor Shaded(static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Paint.R * Tint.X), 0, 255)),
						static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Paint.G * Tint.Y), 0, 255)),
						static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Paint.B * Tint.Z), 0, 255)), 255);
					// (the mesh keeps its colours as sRGB bytes, as the surface material reads them: a colour handed over
					// linear is turned back to these same bytes)
					Colours[Corner] = FVector4f(FLinearColor(Shaded));
					Uvs.Set(Corner, 0, bOnCards ? FVector2f(M.Uvs[K * 2], M.Uvs[K * 2 + 1]) : FVector2f::ZeroVector);
					Corners[I] = Corner;
				}
				for (size_t K = 0; K + 2 < M.Triangles.size(); K += 3)
				{
					const FVertexInstanceID Three[3] = { Corners[static_cast<int32>(M.Triangles[K])], Corners[static_cast<int32>(M.Triangles[K + 1])],
						Corners[static_cast<int32>(M.Triangles[K + 2])] };
					D.CreateTriangle(bOnCards ? LeafGroup : Group, TArrayView<const FVertexInstanceID>(Three, 3));
				}
			}
		}
		UStaticMesh* Made = NewObject<UStaticMesh>(Owner, Name);
		Made->GetStaticMaterials().Add(FStaticMaterial(Ink, TEXT("Ink")));
		if (bCards)
		{
			Made->GetStaticMaterials().Add(FStaticMaterial(LeafInk, TEXT("Leaves")));
		}
		TArray<const FMeshDescription*> Levelled;
		for (const FMeshDescription& D : Descriptions)
		{
			Levelled.Add(&D);
		}
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Made->BuildFromMeshDescriptions(Levelled, Params);
		// Each further level from its distance: a level is chosen by the share of the screen the mesh's bounds fill (a long
		// lens keeps the nearer level further out). Until GS9 the far level's share was worked out for a 90 degree lens,
		// so at the usual 50 deg it fell at 2.14 times the field's FarFrom; the callers keep that unless told otherwise.
		// A level with no shadow has every section's shadow off: the instances' proxies read it as they are made.
		FStaticMeshRenderData* Drawn = Made->GetRenderData();
		if (Drawn)
		{
			const double Reach = FMath::Max(1.0, static_cast<double>(Made->GetBounds().SphereRadius));
			float Last = 1e9f;
			for (int32 L = 1; L < Drawn->LODResources.Num() && L <= Further.Num() && L < MAX_STATIC_MESH_LODS; ++L)
			{
				const float Share = FMath::Min(ScreenAt(Reach, Further[L - 1].From, U), Last * 0.95f);
				Drawn->ScreenSize[L].Default = Share;
				Last = Share;
				if (!Further[L - 1].bShadow)
				{
					for (FStaticMeshSection& Section : Drawn->LODResources[L].Sections)
					{
						Section.bCastShadow = false;
					}
				}
			}
		}
		return Made;
	}

	/** A footprint shade (LandShade, model axes: x forward, y right, z up, as Unreal's own) as a mesh of its own on the
	 * shade's ink, for instancing: the share of light left at each point its colour's bytes (the material reads them
	 * as the eye measures and makes them linear), facing up. */
	static UStaticMesh* ShadeMesh(UObject* Owner, const FName& Name, const LandShade::Mesh& M, UMaterialInterface* Ink, double U)
	{
		if (!Ink || M.Count() == 0 || M.Triangles.empty())
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
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Shade");
		const int32 Count = static_cast<int32>(M.Count());
		D.ReserveNewVertices(Count);
		D.ReserveNewVertexInstances(Count);
		D.ReserveNewTriangles(static_cast<int32>(M.Triangles.size() / 3));
		TArray<FVertexInstanceID> Corners;
		Corners.SetNum(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const size_t K = static_cast<size_t>(I);
			const FVertexID Vertex = D.CreateVertex();
			Positions[Vertex] = FVector3f(M.Points[K * 3] * U, M.Points[K * 3 + 1] * U, M.Points[K * 3 + 2] * U);
			const FVertexInstanceID Corner = D.CreateVertexInstance(Vertex);
			Normals[Corner] = FVector3f::UpVector;
			Tangents[Corner] = FVector3f::ForwardVector;
			Signs[Corner] = 1.0f;
			const uint8 Left = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(M.Light[K] * 255.0f), 0, 255));
			// (kept as these same bytes: see ThingMesh)
			Colours[Corner] = FVector4f(FLinearColor(FColor(Left, Left, Left, 255)));
			Uvs.Set(Corner, 0, FVector2f::ZeroVector);
			Corners[I] = Corner;
		}
		for (size_t K = 0; K + 2 < M.Triangles.size(); K += 3)
		{
			const FVertexInstanceID Three[3] = { Corners[static_cast<int32>(M.Triangles[K])], Corners[static_cast<int32>(M.Triangles[K + 1])],
				Corners[static_cast<int32>(M.Triangles[K + 2])] };
			D.CreateTriangle(Group, TArrayView<const FVertexInstanceID>(Three, 3));
		}
		UStaticMesh* Made = NewObject<UStaticMesh>(Owner, Name);
		Made->GetStaticMaterials().Add(FStaticMaterial(Ink, TEXT("Shade")));
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

	/** A ground tile's level (GS8: LandField's Tile Near, Far, Built, or a tile of the far ring) as a static mesh of its
	 * own, so the engine draws it from its cached draw commands: as a procedural mesh it was gathered again on the draw
	 * thread every frame, in every pass, shadows too. Points and normals as FromField makes them; colours as sRGB bytes
	 * (see ThingMesh); no texture coordinates, as before; the tangent along Unreal's x laid flat on the normal, as the
	 * procedural mesh's own (it gave none, so x). */
	static UStaticMesh* GroundMesh(UObject* Owner, const FName& Name, const LandField::Mesh& M, UMaterialInterface* Ink, double U)
	{
		if (!Ink || M.Empty())
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
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Ground");
		const int32 Count = static_cast<int32>(M.Points());
		D.ReserveNewVertices(Count);
		D.ReserveNewVertexInstances(Count);
		D.ReserveNewTriangles(static_cast<int32>(M.Tris()));
		TArray<FVertexInstanceID> Corners;
		Corners.SetNum(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const size_t K = static_cast<size_t>(I);
			const float* At = &M.Positions[K * 3];
			const float* Way = &M.Normals[K * 3];
			const FVertexID Vertex = D.CreateVertex();
			Positions[Vertex] = FVector3f(At[1] * U, At[0] * U, At[2] * U);
			const FVertexInstanceID Corner = D.CreateVertexInstance(Vertex);
			const FVector3f Normal = FVector3f(Way[1], Way[0], Way[2]).GetSafeNormal(1e-6f, FVector3f::UpVector);
			Normals[Corner] = Normal;
			FVector3f Along = FVector3f::ForwardVector - Normal * FVector3f::DotProduct(FVector3f::ForwardVector, Normal);
			if (!Along.Normalize(1e-6f))
			{
				Along = FVector3f::RightVector;
			}
			Tangents[Corner] = Along;
			Signs[Corner] = 1.0f;
			const LandField::Rgb8& Paint = M.Colours[K];
			Colours[Corner] = FVector4f(FLinearColor(FColor(Paint.R, Paint.G, Paint.B, 255)));
			Uvs.Set(Corner, 0, FVector2f::ZeroVector);
			Corners[I] = Corner;
		}
		for (size_t K = 0; K + 2 < M.Triangles.size(); K += 3)
		{
			const FVertexInstanceID Three[3] = { Corners[static_cast<int32>(M.Triangles[K])], Corners[static_cast<int32>(M.Triangles[K + 1])],
				Corners[static_cast<int32>(M.Triangles[K + 2])] };
			D.CreateTriangle(Group, TArrayView<const FVertexInstanceID>(Three, 3));
		}
		UStaticMesh* Made = NewObject<UStaticMesh>(Owner, Name);
		Made->GetStaticMaterials().Add(FStaticMaterial(Ink, TEXT("Ground")));
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

	/**
	 * Which of a battle model's triangle corners goes to the engine at place I (LandKit's meshes: the tanks, guns, lorries,
	 * cars and props): each triangle's last two swapped. The model files wind a triangle's corners counter-clockwise seen
	 * from its front in their own axes; the engine draws the side it sees them clockwise from (the campaign's own reader
	 * turns them so, FMiniatureMesh::Face), and the files' axes go to the engine's with no mirror on the way. Handed over
	 * as they stood, every vehicle was drawn inside out: from outside the inner side of her far walls, and whatever stood
	 * inside her, her commander's legs and his hatch's floor, drawn over the walls in front of him (Brian, 23 November
	 * 14:45Z to 15:05Z: "some of the tanks you can see through the back of them", "every tank appears to be see
	 * through", his Tiger's commander seen whole through her turret). The men and the field are wound for the engine
	 * already (their mirror on the way, LandMen::WorldOf and World's swap) and are not turned.
	 */
	static constexpr int32 KitFront(int32 I)
	{
		return I % 3 == 1 ? I + 1 : I % 3 == 2 ? I - 1 : I;
	}

	/** A far man of the soldier pose bank (SO1: one baked set in one pose, LandMen::Crowd::BankMesh and BankLook) as a
	 * mesh of its own on the men's ink, for instancing: his own axes (x forward, y to his right, z up: the same hand as
	 * Unreal's) as the mesh's, metres to Unreal's units; colours as sRGB bytes (see ThingMesh). Null where the points
	 * and the look do not agree. */
	static UStaticMesh* BankManMesh(UObject* Owner, const FName& Name, const std::vector<float>& At, const std::vector<float>& Facing, const LandMen::Look& Paint,
		UMaterialInterface* Ink, double U)
	{
		const int32 Count = Paint.Points;
		if (!Ink || Count <= 0 || At.size() != static_cast<size_t>(Count) * 3 || Facing.size() != At.size() || Paint.Triangles.empty())
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
		D.ReserveNewTriangles(static_cast<int32>(Paint.Triangles.size() / 3));
		TArray<FVertexInstanceID> Corners;
		Corners.SetNum(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const size_t K = static_cast<size_t>(I) * 3;
			const FVertexID Vertex = D.CreateVertex();
			Positions[Vertex] = FVector3f(At[K] * U, At[K + 1] * U, At[K + 2] * U);
			const FVertexInstanceID Corner = D.CreateVertexInstance(Vertex);
			const FVector3f Normal = FVector3f(Facing[K], Facing[K + 1], Facing[K + 2]).GetSafeNormal(1e-6f, FVector3f::UpVector);
			Normals[Corner] = Normal;
			const FVector3f Across = FMath::Abs(Normal.Z) < 0.9f ? FVector3f::UpVector : FVector3f::ForwardVector;
			Tangents[Corner] = FVector3f::CrossProduct(Across, Normal).GetSafeNormal(1e-6f, FVector3f::ForwardVector);
			Signs[Corner] = 1.0f;
			const FColor Shade = K + 2 < Paint.Rgb.size() ? FColor(Paint.Rgb[K], Paint.Rgb[K + 1], Paint.Rgb[K + 2], 255) : FColor(90, 90, 80, 255);
			Colours[Corner] = FVector4f(FLinearColor(Shade));
			Uvs.Set(Corner, 0, FVector2f::ZeroVector);
			Corners[I] = Corner;
		}
		for (size_t K = 0; K + 2 < Paint.Triangles.size(); K += 3)
		{
			const uint32_t A = Paint.Triangles[K], B = Paint.Triangles[K + 1], C = Paint.Triangles[K + 2];
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

	/** A battle model's bodies at rest (a prop: the field telephone, the cable reel, the stretcher; LandKit's shape) as
	 * one mesh on Ink for instancing: its own axes (x forward, y right, z up, Unreal's hand), each body's points from its
	 * pivot, its triangles turned for the engine (KitFront); Far its plain level, else its fine one. Null where the model
	 * has no points. */
	static UStaticMesh* PropMesh(UObject* Owner, const FName& Name, const LandKit::Model& Prop, bool bFar, UMaterialInterface* Ink, double U)
	{
		std::vector<float> At, Facing;
		LandMen::Look Paint;
		for (const LandKit::Body& Body : Prop.Bodies)
		{
			const LandKit::Mesh& M = bFar ? Body.Far : Body.Near;
			const size_t Count = M.Points.size() / 3;
			if (Count == 0 || M.Normals.size() != M.Points.size() || M.Colours.size() < Count * 4)
			{
				continue;
			}
			const uint32_t First = static_cast<uint32_t>(Paint.Points);
			for (size_t K = 0; K < Count; ++K)
			{
				At.push_back(M.Points[K * 3] + static_cast<float>(Body.Pivot.X));
				At.push_back(M.Points[K * 3 + 1] + static_cast<float>(Body.Pivot.Y));
				At.push_back(M.Points[K * 3 + 2] + static_cast<float>(Body.Pivot.Z));
				Facing.insert(Facing.end(), { M.Normals[K * 3], M.Normals[K * 3 + 1], M.Normals[K * 3 + 2] });
				Paint.Rgb.insert(Paint.Rgb.end(), { M.Colours[K * 4], M.Colours[K * 4 + 1], M.Colours[K * 4 + 2] });
			}
			// (each triangle's last two corners swapped, as FromBody's: KitFront)
			for (size_t K = 0; K < M.Triangles.size(); ++K)
			{
				Paint.Triangles.push_back(First + M.Triangles[static_cast<size_t>(KitFront(static_cast<int32>(K)))]);
			}
			Paint.Points += static_cast<int>(Count);
		}
		return BankManMesh(Owner, Name, At, Facing, Paint, Ink, U);
	}

	/** Where a banked man stands in Unreal's frame, from his WorldFrame (field = R * own + T, R row-major, the field's x
	 * east and y north): Unreal's x is the field's y and its y the field's x, so his own axes go to Unreal by R with its
	 * first two rows swapped. R is a mirror (his axes are left-handed, the field's right-handed) and the swap another,
	 * so together a plain turn: his mesh needs no flip. (Checked against the soldiers' bank test: det(R) -1, R
	 * orthonormal to 2e-7, the mesh so placed within 3.6 mm of his banked skin, his face's moved points.) */
	static FTransform BankedAt(const LandMen::Xform& W, double U)
	{
		const FVector XAxis(W.R[3], W.R[0], W.R[6]);
		const FVector YAxis(W.R[4], W.R[1], W.R[7]);
		const FVector ZAxis(W.R[5], W.R[2], W.R[8]);
		const FVector Origin(W.T[1] * U, W.T[0] * U, W.T[2] * U);
		return FTransform(FMatrix(XAxis, YAxis, ZAxis, Origin));
	}

	/** Where an instance of the ground's things stands, in Unreal's frame: the field's turn (counter-clockwise from x
	 * east toward y north) is the opposite way round in the swapped frame. */
	static FTransform PlacedAt(const LandField::Placed& One, double U)
	{
		return FTransform(FRotator(0.0, -FMath::RadiansToDegrees(static_cast<double>(One.Yaw)), 0.0), FVector(One.Y * U, One.X * U, One.Z * U),
			FVector(One.Scale, One.Scale, One.ScaleZ));
	}

	/** Whether a base is clutter (laid round the eye, not across the field). */
	static bool IsClutter(int32 Base)
	{
		return Base >= static_cast<int32>(LandField::Base::Tuft) && Base < static_cast<int32>(LandField::Base::Count);
	}

	/** The corn near the lens (the art director's cut 4, fix 4): its bundles cleared within StalkClear metres of the eye
	 * (along the ground), where each clump is drawn as its own stalks (LandField's Stalks, on M_LandStalks), while the
	 * eye is under StalkEyeLow metres over the ground; again over StalkEyeHigh, the bundles back and the stalks hidden
	 * (the stalks reach only 5.5 m from the lens: from higher, a clearing would be a bald ring in the corn). */
	static constexpr double StalkClear = 3.5, StalkEyeLow = 2.5, StalkEyeHigh = 3.0;

	/** A body of a battle model (model axes, metres from its pivot: x forward, y right, z up, as Unreal's own) as the
	 * engine takes it, its colours taken toward the burnt colour by Soot. Each triangle's last two corners are swapped
	 * (KitFront). */
	static void FromBody(const LandKit::Mesh& M, const uint8_t* Burnt, double Soot, double U, TArray<FVector>& P, TArray<FVector>& N, TArray<FColor>& C, TArray<int32>* T)
	{
		const int32 Count = static_cast<int32>(M.Count());
		P.SetNumUninitialized(Count);
		N.SetNumUninitialized(Count);
		C.SetNumUninitialized(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const size_t K = static_cast<size_t>(I);
			P[I] = FVector(M.Points[K * 3], M.Points[K * 3 + 1], M.Points[K * 3 + 2]) * U;
			N[I] = FVector(M.Normals[K * 3], M.Normals[K * 3 + 1], M.Normals[K * 3 + 2]);
			const double S = Soot > 0.0 && K < M.Soot.size() ? FMath::Clamp(M.Soot[K] * Soot, 0.0, 1.0) : 0.0;
			auto Mixed = [S](uint8_t From, uint8_t To) { return static_cast<uint8>(FMath::RoundToInt(From + (To - From) * S)); };
			C[I] = FColor(Mixed(M.Colours[K * 4], Burnt[0]), Mixed(M.Colours[K * 4 + 1], Burnt[1]), Mixed(M.Colours[K * 4 + 2], Burnt[2]), M.Colours[K * 4 + 3]);
		}
		if (T)
		{
			T->SetNumUninitialized(static_cast<int32>(M.Triangles.size()));
			for (int32 I = 0; I < T->Num(); ++I)
			{
				(*T)[I] = static_cast<int32>(M.Triangles[static_cast<size_t>(KitFront(I))]);
			}
		}
	}

	/** Whether a field point is in the camera's picture (its vertical lens, the window's shape), with a margin. */
	static bool InPicture(const LandEye::Pose& Pose, double Aspect, const LandPlay::Vec& At, double Margin)
	{
		const LandPlay::Vec Fwd = (Pose.Look - Pose.Eye).Unit();
		const LandPlay::Vec To = At - Pose.Eye;
		const double Depth = To.Dot(Fwd);
		if (Depth < 0.5)
		{
			return To.Length() < 3.0;
		}
		const LandPlay::Vec Right = LandPlay::Vec(Fwd.Y, -Fwd.X, 0.0).Unit();
		const LandPlay::Vec Up(Right.Y * Fwd.Z - Right.Z * Fwd.Y, Right.Z * Fwd.X - Right.X * Fwd.Z, Right.X * Fwd.Y - Right.Y * Fwd.X);
		const double TanV = FMath::Tan(FMath::DegreesToRadians(Pose.Fov * 0.5)) * Margin;
		const double TanH = TanV * Aspect;
		return FMath::Abs(To.Dot(Right)) / Depth <= TanH && FMath::Abs(To.Dot(Up)) / Depth <= TanV;
	}

	static UProceduralMeshComponent* NewMesh(AActor* Owner, USceneComponent* Parent, const FName& Name, bool bShadow)
	{
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Owner, Name);
		Mesh->SetupAttachment(Parent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(bShadow);
		Mesh->RegisterComponent();
		return Mesh;
	}

	/** A man's head top and chest on the field now, from his skinned bones (the soldier file's "head" and "chest"). */
	static bool ManPoints(FLandBattleState& S, int Man, LandPlay::Vec& OutHead, LandPlay::Vec& OutChest)
	{
		if (!S.Men || Man < 0 || Man >= S.Men->Count())
		{
			return false;
		}
		const LandMen::Soldier* Sol = S.Men->SoldierOf(Man);
		const LandMen::Xform* Frames = S.Men->FramesOf(Man);
		if (!Sol || !Frames)
		{
			return false;
		}
		std::pair<int, int> Bones(-1, -1);
		bool bKnown = false;
		for (const auto& Known : S.SightBones)
		{
			if (Known.first == Sol)
			{
				Bones = Known.second;
				bKnown = true;
				break;
			}
		}
		if (!bKnown)
		{
			Bones = std::make_pair(Sol->BoneIndex("head"), Sol->BoneIndex("chest"));
			S.SightBones.emplace_back(Sol, Bones);
		}
		const int Count = static_cast<int>(Sol->Bones.size());
		if (Bones.first < 0 || Bones.first >= Count || Bones.second < 0 || Bones.second >= Count)
		{
			return false;
		}
		const LandMen::Vec3 Head = Frames[Bones.first].Point(Sol->Bones[static_cast<size_t>(Bones.first)].Head);
		const LandMen::Vec3 Chest = Frames[Bones.second].Point(Sol->Bones[static_cast<size_t>(Bones.second)].Head);
		// (the head bone starts at the neck: its top is some 0.25 m on, the way the neck runs)
		const LandPlay::Vec Neck(Head.X, Head.Y, Head.Z), Breast(Chest.X, Chest.Y, Chest.Z);
		const LandPlay::Vec Up = Neck - Breast;
		OutHead = Neck + (Up.Length() > 1e-4 ? Up.Unit() : LandPlay::Vec(0.0, 0.0, 1.0)) * 0.25;
		OutChest = Breast;
		return true;
	}

	/** The men and the vehicles where the cores have them now, for the camera's questions (LandSight). */
	static void PlaceMovers(FLandBattleState& S)
	{
		LAND_SCOPE(PlaceMovers);
		S.Sight.ClearMovers();
		if (S.Men)
		{
			for (int Man = 0; Man < S.Men->Count(); ++Man)
			{
				if (!S.Men->Visible(Man) || static_cast<size_t>(Man) >= S.TrackOfMan.size() || S.Men->IsRider(Man))
				{
					continue;
				}
				double X = 0.0, Y = 0.0, Z = 0.0;
				float Facing = 0.0f;
				// (his height as last posed: no ground asked for each of two thousand men a frame)
				if (!S.Men->PlaceAsPosed(Man, X, Y, Z, Facing))
				{
					continue;
				}
				LandSight::Man M;
				M.Track = S.TrackOfMan[static_cast<size_t>(Man)];
				LandPlay::Vec Head, Chest;
				if (ManPoints(S, Man, Head, Chest))
				{
					// (from his feet, or from wherever the lower end of him lies, to the top of his head)
					const LandPlay::Vec Root(X, Y, Z + 0.25);
					M.Feet = Head.Z - Z > 1.0 ? Root : Chest + (Chest - Head) * 1.6;
					M.Top = Head;
				}
				else
				{
					M.Feet = LandPlay::Vec(X, Y, Z + 0.25);
					M.Top = LandPlay::Vec(X, Y, Z + 1.75);
				}
				S.Sight.Add(M);
			}
		}
		if (S.Armour)
		{
			const std::vector<LandKit::Vehicle>& All = S.Armour->Vehicles();
			for (size_t I = 0; I < All.size(); ++I)
			{
				const LandKit::Vehicle& V = All[I];
				if (V.Fade <= 0.0 || I >= S.TrackOfVehicle.size())
				{
					continue;
				}
				const LandKit::Model& Look = V.Look();
				LandSight::Machine M;
				M.Track = S.TrackOfVehicle[I];
				M.Root = LandPlay::Vec(V.Root.At.X, V.Root.At.Y, V.Root.At.Z);
				M.Heading = V.Heading;
				M.Turret = V.Heading + FMath::DegreesToRadians(V.TurretDeg);
				M.HullLo = LandPlay::Vec(Look.HullLo.X, Look.HullLo.Y, Look.HullLo.Z);
				M.HullHi = LandPlay::Vec(Look.HullHi.X, Look.HullHi.Y, Look.HullHi.Z);
				M.bTurret = Look.Turret >= 0 && !(V.bDead && V.bBrewUp);
				M.TurretLo = LandPlay::Vec(Look.TurretLo.X, Look.TurretLo.Y, Look.TurretLo.Z);
				M.TurretHi = LandPlay::Vec(Look.TurretHi.X, Look.TurretHi.Y, Look.TurretHi.Z);
				LandKit::V3 Muzzle, Way;
				if (V.SocketNow("muzzle", Muzzle, Way))
				{
					M.bMuzzle = true;
					M.Muzzle = LandPlay::Vec(Muzzle.X, Muzzle.Y, Muzzle.Z);
				}
				S.Sight.Add(M);
			}
		}
	}
}

ALandBattle::ALandBattle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

bool ALandBattle::WantsReplay(FString& OutKey)
{
	return FParse::Value(FCommandLine::Get(), TEXT("TheaterLandBattle="), OutKey) && !OutKey.IsEmpty();
}

void ALandBattle::ReplayWhenReady(const FString& InKey)
{
	PendingReplay = InKey;
	SetActorTickEnabled(true);
}

bool ALandBattle::BeginReplay(const FString& InKey)
{
	Key = InKey;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandBattleShots="), ShotFolder);
	// (the walk pan, the Mac recipe's look 7: "-TheaterLandWalkPan=<battle s>[,<seconds>]", read past its comma; or the
	// tank pan, "-TheaterLandTankPan=" the same, the far tanks' motion vectors: a tank driving past the hand-over)
	FString Pan;
	const bool bWalk = FParse::Value(FCommandLine::Get(), TEXT("TheaterLandWalkPan="), Pan, false);
	bTankPan = !bWalk && FParse::Value(FCommandLine::Get(), TEXT("TheaterLandTankPan="), Pan, false);
	if (bWalk || bTankPan)
	{
		TArray<FString> Parts;
		Pan.ParseIntoArray(Parts, TEXT(","), false);
		if (Parts.Num() > 0 && !Parts[0].TrimStartAndEnd().IsEmpty())
		{
			WalkPanAt = FMath::Max(0.0, FCString::Atod(*Parts[0]));
		}
		if (Parts.Num() > 1 && !Parts[1].TrimStartAndEnd().IsEmpty())
		{
			WalkPanFor = FMath::Clamp(FCString::Atod(*Parts[1]), 4.0, 120.0);
		}
	}
	const FString File = FPaths::ProjectContentDir() / TEXT("Data/LandBattles") / Key + TEXT(".json");
	FString Text;
	auto Fail = [this]()
	{
		State.Reset();
		if (!ShotFolder.IsEmpty())
		{
			QuitAt = 0.0;
			SetActorTickEnabled(true);
		}
		return false;
	};
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: no recorded battle %s (is Content/Data/LandBattles staged with the build?)"), *File);
		return Fail();
	}
	const double Began = LandParts::Seconds();
	State = MakeShared<FLandBattleState>();
	FLandBattleState& S = *State;
	S.RecText = LandParts::Narrow(Text);
	// The night patrol's men (release 2, the player's patrol switch): a recording that carries a patrol has its men
	// written into the text as rows, which the crowd and the feed both read (LandFeed::AddPatrolMen).
	int32 PatrolMen = LandFeed::AddPatrolMen(S.RecText);
	std::string Error;
	if (!LandFeed::ReadRecording(S.RecText, S.Rec, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: %s: %s"), *File, *LandParts::Wide(Error));
		return Fail();
	}
	// (a stand-in patrol for a recording without one, "-TheaterLandPatrol[=found,caught]": found and caught when bare)
	{
		FString Asked;
		const bool bValue = FParse::Value(FCommandLine::Get(), TEXT("TheaterLandPatrol="), Asked, false);
		if ((bValue || FParse::Param(FCommandLine::Get(), TEXT("TheaterLandPatrol"))) && !S.Rec.Night.bOn)
		{
			TArray<FString> Parts;
			Asked.ParseIntoArray(Parts, TEXT(","), false);
			const bool bFound = Parts.Num() < 1 || Parts[0].TrimStartAndEnd().IsEmpty() || FCString::Atoi(*Parts[0]) != 0;
			const bool bCaught = Parts.Num() < 2 || Parts[1].TrimStartAndEnd().IsEmpty() || FCString::Atoi(*Parts[1]) != 0;
			std::string Why;
			PatrolMen = LandNight::AddStandInPatrol(S.RecText, S.Rec, bFound, bCaught, Why);
			if (PatrolMen == 0)
			{
				UE_LOG(LogTheater, Warning, TEXT("Land battle: no stand-in patrol (%s)"), Why.empty() ? TEXT("no foot block on a side, or the two closer than 200 m") : *LandParts::Wide(Why));
				// (the text may have been changed before the reading failed: read it again as it was)
				if (!Why.empty())
				{
					S.RecText = LandParts::Narrow(Text);
					LandFeed::AddPatrolMen(S.RecText);
					if (!LandFeed::ReadRecording(S.RecText, S.Rec, Error))
					{
						UE_LOG(LogTheater, Warning, TEXT("Land battle: %s: %s"), *File, *LandParts::Wide(Error));
						return Fail();
					}
				}
			}
		}
	}
	if (S.Rec.Night.bOn)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: the night patrol: block %d, %d men (%d written in), out %.0f s to %.0f s, %s, %d flares%s"),
			S.Rec.Night.Unit, S.Rec.Night.Count, PatrolMen, S.Rec.Night.T0, S.Rec.Night.T1, S.Rec.Night.bFound ? TEXT("the post found") : TEXT("nothing found"),
			static_cast<int32>(S.Rec.Night.Flares.size()), S.Rec.Night.Alerted >= 0.0 ? TEXT(", the defence alerted") : TEXT(""));
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle: %s read in %.0f ms: \"%s\", %s, %d tracks, %d rounds, %.0f s"), *Key, (LandParts::Seconds() - Began) * 1000.0,
		*LandParts::Wide(S.Rec.Title), *LandParts::Wide(S.Rec.Date), static_cast<int32>(S.Rec.Tracks.size()), static_cast<int32>(S.Rec.Rounds.size()), S.Rec.End);
	return Open();
}

void ALandBattle::LoadInks()
{
	// Vertex colours on the campaign's own surface (sRGB in, made linear there), as the air battle's aircraft: the
	// ground with the surface's fine grain, the rest plain.
	UMaterialInterface* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_MapSurface.M_MapSurface"));
	if (!Surface)
	{
		Surface = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	auto Ink = [this, Surface](float Roughness, float Specular, float Grain)
	{
		UMaterialInstanceDynamic* Made = UMaterialInstanceDynamic::Create(Surface, this);
		Made->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		Made->SetScalarParameterValue(TEXT("Specular"), Specular);
		Made->SetScalarParameterValue(TEXT("Grain"), Grain);
		Made->SetScalarParameterValue(TEXT("Tooth"), 0.0f);
		return Made;
	};
	// The ground tiles (near, far and the ring) on the battle's own ground surface, M_LandSurface (build_content.py):
	// the same vertex colours with a sward's or a soil's grain, the lie of the field and a fine lean over them. Without
	// it (build_content.py not run since it came) the ground is on the campaign's surface as before, said once.
	UMaterialInterface* LandSurface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_LandSurface.M_LandSurface"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (LandSurface)
	{
		GroundInk = UMaterialInstanceDynamic::Create(LandSurface, this);
		GroundInk->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
		GroundInk->SetScalarParameterValue(TEXT("Specular"), 0.15f);
	}
	else
	{
		static bool bToldNoLandSurface = false;
		if (!bToldNoLandSurface)
		{
			bToldNoLandSurface = true;
			UE_LOG(LogTheater, Warning, TEXT("Land battle: /Game/Campaign/Materials/M_LandSurface is missing (run Tools/build_content.sh): the ground is drawn on M_MapSurface."));
		}
		GroundInk = Ink(0.95f, 0.15f, 0.14f);
	}
	ThingInk = Ink(0.9f, 0.2f, 0.06f);
	// The hedges' and trees' leaves on their cards (M_LandLeaves, build_content.py): without it, their solid sprigs.
	UMaterialInterface* Leaves = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_LandLeaves.M_LandLeaves"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	LeafInk = Leaves ? UMaterialInstanceDynamic::Create(Leaves, this) : nullptr;
	if (!Leaves)
	{
		static bool bToldNoLeaves = false;
		if (!bToldNoLeaves)
		{
			bToldNoLeaves = true;
			UE_LOG(LogTheater, Warning, TEXT("Land battle: /Game/Campaign/Materials/M_LandLeaves is missing (run Tools/build_content.sh): the hedges and trees carry their solid sprigs."));
		}
	}
	// The corn's stalks near a low lens (M_LandStalks, build_content.py; the art director's cut 4, fix 4): without it, the
	// bundles to the lens, said once. Shown from NearFrom + NearSpan of depth past the lens, gone from FarTo along the
	// ground, where the bundles (cleared within LandParts::StalkClear) take over.
	UMaterialInterface* Stalks = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_LandStalks.M_LandStalks"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	StalkInk = Stalks ? UMaterialInstanceDynamic::Create(Stalks, this) : nullptr;
	if (StalkInk)
	{
		StalkInk->SetScalarParameterValue(TEXT("NearFrom"), 0.6f);
		StalkInk->SetScalarParameterValue(TEXT("NearSpan"), 0.3f);
		StalkInk->SetScalarParameterValue(TEXT("FarTo"), static_cast<float>(LandParts::StalkClear + 1.5));
		StalkInk->SetScalarParameterValue(TEXT("FarSpan"), 1.5f);
	}
	else
	{
		static bool bToldNoStalks = false;
		if (!bToldNoStalks)
		{
			bToldNoStalks = true;
			UE_LOG(LogTheater, Warning, TEXT("Land battle: /Game/Campaign/Materials/M_LandStalks is missing (run Tools/build_content.sh): the corn keeps its bundles to the lens."));
		}
	}
	ManInk = Ink(0.85f, 0.2f, 0.0f);
	SteelInk = Ink(0.6f, 0.35f, 0.0f);
	// The footprint shade under the tanks, guns and lorries (M_LandShade, build_content.py; the art director's cut 4,
	// fix 3): without it, none, said once. "-TheaterLandFootShade=0.6" takes it part way (0 none, 1 as ruled).
	UMaterialInterface* Shade = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_LandShade.M_LandShade"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	ShadeInk = Shade ? UMaterialInstanceDynamic::Create(Shade, this) : nullptr;
	if (ShadeInk)
	{
		float FootShade = 1.0f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandFootShade="), FootShade);
		ShadeInk->SetScalarParameterValue(TEXT("Strength"), FMath::Clamp(FootShade, 0.0f, 1.0f));
	}
	else
	{
		static bool bToldNoShade = false;
		if (!bToldNoShade)
		{
			bToldNoShade = true;
			UE_LOG(LogTheater, Warning, TEXT("Land battle: /Game/Campaign/Materials/M_LandShade is missing (run Tools/build_content.sh): no footprint shade under the tanks."));
		}
	}
	// (the surfaces' flat lift on every face turned up to the sky, 0.26 on the map, lit the ground under a tank as
	// much as the open field, sun or shade: the battle's light now gives the shade its own (the contact shade, the
	// ambient occlusion), so only a little of it. The graphics audit, 22 November. "-TheaterLandTopLight=0.08")
	float TopLight = 0.08f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandTopLight="), TopLight);
	TopLight = FMath::Clamp(TopLight, 0.0f, 1.0f);
	for (UMaterialInstanceDynamic* Made : { GroundInk.Get(), ThingInk.Get(), ManInk.Get(), SteelInk.Get() })
	{
		if (Made)
		{
			Made->SetScalarParameterValue(TEXT("TopLight"), TopLight);
		}
	}
}

bool ALandBattle::Open()
{
	UWorld* Here = GetWorld();
	if (!Here || !State.IsValid())
	{
		return false;
	}
	FLandBattleState& S = *State;
	const LandFeed::Recording& Rec = S.Rec;
	LoadInks();

	// The mix: read here as a tree for the distance families and the buses, and by the sound core as text.
	FString MixText;
	if (FFileHelper::LoadFileToString(MixText, *(FPaths::ProjectContentDir() / TEXT("Data/Sound/mix.json"))))
	{
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MixText), Mix);
	}
	std::string Error;
	if (!Mix.IsValid() || !S.Mix.Begin(LandParts::Narrow(MixText), Rec.Seed ? Rec.Seed : 1, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: Content/Data/Sound/mix.json is missing or unreadable (%s): the battle is silent."), *LandParts::Wide(Error));
		Mix.Reset();
	}
	// The one-shot takes' players, pooled (land.voices): the warm ones made now, before the battle, so no take spawns
	// and destroys a player of its own mid-fight (the production study of 22 Nov, section 5, item 9). Off with
	// land.voices.pool false or -TheaterLandVoicesSpawned.
	for (const TObjectPtr<UAudioComponent>& Player : VoicePlayers)
	{
		if (Player)
		{
			Player->Stop();
		}
	}
	bPoolVoices = Mix.IsValid() && S.Mix.bVoicesPooled && !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandVoicesSpawned"));
	BusDbs.Reset();
	EngineLoopNames.Reset();
	VoicesUnheard = 0;
	if (bPoolVoices)
	{
		S.Voices.Size(S.Mix.VoicesWarm, S.Mix.VoicesMost);
		while (VoicePlayers.Num() < S.Voices.Count())
		{
			VoicePlayers.Add(MakeVoice());
		}
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle sound: the takes' players %s"), bPoolVoices
		? *FString::Printf(TEXT("pooled, %d made before the battle, at most %d (land.voices)"), S.Voices.Count(), S.Mix.VoicesMost)
		: TEXT("spawned one for each take (land.voices.pool false or -TheaterLandVoicesSpawned)"));
	// The master limiter (mix.json buses.master): stacked shots, blasts and the lift down among the men never past the
	// ceiling. On the master while the battle runs, so whatever records the game's sound records it limited; taken off
	// at EndPlay. (As the air battle's.)
	if (Mix.IsValid() && !Limiter)
	{
		// One stage, its attack near instant (the sound director's model of the engine's limiter, sound/limiter-model):
		// a gun crack's first peak lasts about 0.1 ms, so a 1 ms attack barely moves before it has passed, whatever the
		// look-ahead, and a second stage with the same attack misses the same cracks (Bremen stayed at +4.4 dBFS). With
		// the attack at 0.02 ms, 1 ms of look-ahead and a digital 500 ms release (the curve of an analog ~110 ms) the
		// model held -1.42 dBFS on Bremen and -1.41 on the Normandy cut raised 6 dB, the loudness within a fraction of
		// a LU. Every number from mix.json buses.master.
		const float Ceiling = static_cast<float>(LandParts::MixNumber(Mix, TEXT("buses.master.limiterCeilingDb"), -1.5));
		const float Release = static_cast<float>(LandParts::MixNumber(Mix, TEXT("buses.master.limiterReleaseMs"), 500.0));
		const float Attack = static_cast<float>(LandParts::MixNumber(Mix, TEXT("buses.master.limiterAttackMs"), 0.02));
		const float LookAhead = static_cast<float>(LandParts::MixNumber(Mix, TEXT("buses.master.limiterLookAheadMs"), 1.0));
		bool bAnalog = false;
		if (const TSharedPtr<FJsonObject> Master = LandParts::MixObject(Mix, TEXT("buses.master")))
		{
			Master->TryGetBoolField(TEXT("limiterAnalogDetector"), bAnalog);
		}
		USubmixEffectDynamicsProcessorPreset* Made = NewObject<USubmixEffectDynamicsProcessorPreset>(this);
		FSubmixEffectDynamicsProcessorSettings Settings;
		Settings.DynamicsProcessorType = ESubmixEffectDynamicsProcessorType::Limiter;
		Settings.PeakMode = ESubmixEffectDynamicsPeakMode::Peak;
		// (both channels limited by the louder: a shot hard to one side never passes the ceiling in that ear)
		Settings.LinkMode = ESubmixEffectDynamicsChannelLinkMode::Peak;
		Settings.ThresholdDb = Ceiling;
		Settings.KneeBandwidthDb = 0.0f;
		Settings.bAnalogMode = bAnalog;
		Settings.LookAheadMsec = LookAhead;
		Settings.AttackTimeMsec = Attack;
		Settings.ReleaseTimeMsec = Release;
		Settings.InputGainDb = 0.0f;
		Settings.OutputGainDb = 0.0f;
		// (the preset's own Settings too, not only SetSettings: SetSettings fills the copy the effect reads, and the
		// first effect made from the preset refills that copy from Settings, which were still the defaults, so a default
		// compressor ran in the limiter's place and the second cut peaked at 0 dBFS; set again once it is on the master)
		Made->Settings = Settings;
		Made->SetSettings(Settings);
		Limiter = Made;
		UAudioMixerBlueprintLibrary::AddMasterSubmixEffect(this, Made);
		Made->SetSettings(Settings);
	}
	{
		TWeakObjectPtr<ALandBattle> Weak(this);
		S.Mix.Length = [Weak](const std::string& Name)
		{
			if (!Weak.IsValid())
			{
				return 1.5;
			}
			const FName Take(LandParts::Wide(Name));
			if (const float* Known = Weak->TakeSeconds.Find(Take))
			{
				return static_cast<double>(*Known);
			}
			USoundBase* Sound = Weak->SoundOf(Take);
			const float Long = Sound ? Sound->GetDuration() : 1.5f;
			Weak->TakeSeconds.Add(Take, Long > 0.0f && Long < 600.0f ? Long : 1.5f);
			return static_cast<double>(Weak->TakeSeconds[Take]);
		};
	}

	// The ground (LandField): the bocage, the lanes, the village, out to the haze.
	double Took = LandParts::Seconds();
	{
		// (the hedge cutters' test switch until the rules' cuts come, "-TheaterLandCutters=N": the first N crossings of a
		// bank by the attacker's tanks made cuts, LandSight::CutsFromCrossings, from the field laid once without them;
		// never over the recording's own cuts, nor on the steppe, which has no banks)
		int32 TestCuts = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandCutters="), TestCuts) && TestCuts > 0 && S.Rec.Cuts.empty())
		{
			const LandField::Field First = LandSight::FieldOf(Rec);
			if (!First.Steppe.bOn)
			{
				const LandField::Ground Plain(First);
				S.Rec.Cuts = LandSight::CutsFromCrossings(Rec, Plain, TestCuts);
				UE_LOG(LogTheater, Log, TEXT("Land battle: -TheaterLandCutters=%d: %d cuts made from the tanks' crossings (the field laid once more for them)"),
					TestCuts, static_cast<int32>(S.Rec.Cuts.size()));
			}
		}
		// (the field telephones' test switch until the rules' wires come, "-TheaterLandPhones": the defender's lines from
		// his headquarters to his officers and mortars, cut by the rounds that land beside them, LandSight::PhonesFromBattle)
		if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLandPhones")) && S.Rec.Wires.empty())
		{
			LandSight::PhonesFromBattle(S.Rec);
			UE_LOG(LogTheater, Log, TEXT("Land battle: -TheaterLandPhones: %d wires, %d cuts, %d mends made from the battle"), static_cast<int32>(S.Rec.Wires.size()),
				static_cast<int32>(S.Rec.WireCuts.size()), static_cast<int32>(S.Rec.WireMends.size()));
		}
		// (the same field the cloud test lays: LandSight::FieldOf; the hedges the men and vehicles cross opened there,
		// and laid whole where a hedge cutter will tear it)
		LandField::Field Lay = LandSight::FieldOf(Rec);
		// (the campaign's weather word over the place, the recording's "weather": "mud" the thaw's mud over open ground;
		// "-TheaterLandWeatherWord=mud" stands in for it on any field, for the Mac's off/on line,
		// "-TheaterLandWeatherWord=clear" lays none. Not "-TheaterLandWeather=", the day's cover, deck and base, which
		// FParse would read "mud" from as a clear sky)
		{
			FString Word;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandWeatherWord="), Word))
			{
				Lay.Weather = LandParts::Narrow(Word.TrimStartAndEnd().ToLower());
			}
			if (!Lay.Weather.empty())
			{
				UE_LOG(LogTheater, Display, TEXT("Land battle: the campaign's weather over the field: %s%s"), *LandParts::Wide(Lay.Weather),
					Lay.Weather == "mud" ? TEXT(" (open ground the thaw's mud: the holes' soil, the tracks' ruts, the thrown mud)") : TEXT(""));
			}
		}
		// (the looks on trial, each behind its switch until the art director passes it and the Mac holds its frame rate:
		// "-TheaterLandTrial=church,walltop,spruce,edge,kunai" or "=all"; LandField::Field::Trial)
		{
			// (taken loosely, as the weather word is: "Church, Walltop" reads as "church,walltop")
			Lay.Trial = LandParts::TrialWords();
			if (!Lay.Trial.empty())
			{
				UE_LOG(LogTheater, Display, TEXT("Land battle: -TheaterLandTrial=%s: these looks on trial are ON"), *LandParts::Wide(Lay.Trial));
			}
		}
		// (release 1.5's mend: a river's water drawn 2 m under its banks, not cut down to sea level through hills;
		// "-TheaterLandOldRiverBed" draws it as before)
		Lay.bOldRiverBed = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandOldRiverBed"));
		if (Lay.bOldRiverBed)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: -TheaterLandOldRiverBed: the rivers drawn at the rules' sea level, as before 1.5's mend"));
		}
		if (Lay.Steppe.bOn)
		{
			// The steppe's cottages, ruins, poles, wattle, sweeps, ricks and birches: the east kit's models
			// (Content/Models/East, from battle-models/east); a model not found is drawn plainly, and said.
			const FString Kit = FPaths::ProjectContentDir() / TEXT("Models/East");
			for (const TCHAR* Name : { TEXT("Khata"), TEXT("BurntChimney"), TEXT("TelegraphPole"), TEXT("WattleFence"), TEXT("WellSweep"), TEXT("Haystack"), TEXT("Birch") })
			{
				FString Text;
				if (FFileHelper::LoadFileToString(Text, *(Kit / (FString(Name) + TEXT(".json")))))
				{
					Lay.Steppe.Kit[LandParts::Narrow(FString(Name))] = LandParts::Narrow(Text);
				}
				else
				{
					UE_LOG(LogTheater, Warning, TEXT("Land battle: the east kit's %s is not in Content/Models/East; drawn plainly"), Name);
				}
			}
		}
		else
		{
			// Every other ground's look (LandScape: the desert, the Mediterranean, the jungle, the islands, the farmland):
			// its region's kit (Content/Models/<its folder>, from battle-models/<folder>/models); a model not found is drawn
			// plainly, and said.
			std::string Folder;
			std::vector<std::string> Names;
			if (LandScape::KitWanted(Lay, Folder, Names))
			{
				const FString Kit = FPaths::ProjectContentDir() / TEXT("Models") / LandParts::Wide(Folder);
				for (const std::string& Name : Names)
				{
					FString Text;
					if (FFileHelper::LoadFileToString(Text, *(Kit / (LandParts::Wide(Name) + TEXT(".json")))))
					{
						Lay.Kit[Name] = LandParts::Narrow(Text);
					}
					else
					{
						UE_LOG(LogTheater, Warning, TEXT("Land battle: the %s kit's %s is not in Content/Models/%s; drawn plainly"), *LandParts::Wide(Folder), *LandParts::Wide(Name), *LandParts::Wide(Folder));
					}
				}
			}
		}
		S.Field = std::make_unique<LandField::Ground>(Lay);
		S.Sight.Lay(S.Field.get());
	}
	const double FieldSeconds = LandParts::Seconds() - Took;
	const LandField::Ground* FieldPtr = S.Field.get();

	// The fire's world: the ground drawn, its word for the kit, the shooters' muzzles and where a round struck a hull.
	LandPlay::World PlayWorld;
	// (a round landing in a hedge cutter's gap once it is torn lands on the gap's floor, not on the bank that was there)
	const LandCrater::Field* GapsPtr = &S.Holes;
	const int32* GapsTorn = &CutsTorn;
	PlayWorld.Height = [FieldPtr, GapsPtr, GapsTorn](double X, double Y)
	{
		double Floor = 0.0;
		return *GapsTorn > 0 && GapsPtr->BreachFloor(X, Y, Floor) ? Floor : FieldPtr->HeightAt(X, Y);
	};
	PlayWorld.GroundWord = [FieldPtr](double X, double Y) { return std::string(FieldPtr->At(X, Y).Fx); };

	// Life's seconds against the battle's (the fight at half the battle's clock): before the men and the tanks are made,
	// whose clips are planned by it.
	{
		double Life = S.Life;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandLife="), Life);
		S.Life = FMath::Clamp(Life, 0.5, 4.0);
	}

	// The men (LandMen) and the tanks (LandKit), each reading the recording for itself.
	Took = LandParts::Seconds();
	MakeMen();
	const double MenSeconds = LandParts::Seconds() - Took;
	Took = LandParts::Seconds();
	MakeArmour();
	const double ArmourSeconds = LandParts::Seconds() - Took;
	{
		TWeakPtr<FLandBattleState> Weak(State);
		PlayWorld.Muzzle = [Weak](int Track, double T, const std::string& Socket, LandPlay::Vec& OutAt, LandPlay::Vec& OutFacing)
		{
			const TSharedPtr<FLandBattleState> Pinned = Weak.Pin();
			if (!Pinned.IsValid() || Track < 0)
			{
				return false;
			}
			FLandBattleState& St = *Pinned;
			const size_t K = static_cast<size_t>(Track);
			if (K < St.VehicleOf.size() && St.VehicleOf[K] >= 0 && St.Armour)
			{
				const LandKit::Vehicle& V = St.Armour->Vehicles()[static_cast<size_t>(St.VehicleOf[K])];
				LandKit::V3 At, Facing;
				// Her machine guns: the hull gunner's and the coaxial at their sockets; the commander's on the roof at a
				// roof_mg socket where the model has one, else on its pintle over his hatch's rim (the commander socket is
				// a quarter metre under it). A model short of one fires the next: its one machine gun where it has only
				// that (an 'mg' socket: the StuG, the M10, the M7, the Semovente, the M8), the coaxial, then the main gun's place.
				bool bFound = false;
				if (Socket == "roof_mg")
				{
					bFound = V.SocketAt("roof_mg", T, At, Facing);
					if (!bFound && V.SocketAt("commander", T, At, Facing))
					{
						At = LandKit::V3{ At.X + Facing.X * 0.3, At.Y + Facing.Y * 0.3, At.Z + 0.6 };
						bFound = true;
					}
				}
				else if (!Socket.empty())
				{
					bFound = V.SocketAt(Socket, T, At, Facing);
				}
				if (!bFound && !Socket.empty() && Socket != "mg")
				{
					bFound = V.SocketAt("mg", T, At, Facing);
				}
				if (!bFound && !Socket.empty())
				{
					bFound = V.SocketAt("coax", T, At, Facing);
				}
				if (!bFound)
				{
					bFound = V.SocketAt("muzzle", T, At, Facing);
				}
				if (bFound)
				{
					OutAt = LandPlay::Vec(At.X, At.Y, At.Z);
					OutFacing = LandPlay::Vec(Facing.X, Facing.Y, Facing.Z);
					return true;
				}
				return false;
			}
			if (K < St.ManOf.size() && St.ManOf[K] >= 0 && St.Men)
			{
				LandMen::Vec3 At, Dir;
				if (St.Men->Muzzle(St.ManOf[K], At, Dir))
				{
					OutAt = LandPlay::Vec(At.X, At.Y, At.Z);
					OutFacing = LandPlay::Vec(Dir.X, Dir.Y, Dir.Z);
					return true;
				}
			}
			return false;
		};
		PlayWorld.Skin = [Weak](int Track, int Round, double T, const std::string& Face, double U, double H, LandPlay::Vec& OutAt, LandPlay::Vec& OutNormal)
		{
			(void)T;
			(void)Face;
			(void)U;
			(void)H;
			const TSharedPtr<FLandBattleState> Pinned = Weak.Pin();
			if (!Pinned.IsValid() || Track < 0 || !Pinned->Armour)
			{
				return false;
			}
			FLandBattleState& St = *Pinned;
			const size_t K = static_cast<size_t>(Track);
			if (K >= St.VehicleOf.size() || St.VehicleOf[K] < 0)
			{
				return false;
			}
			// (the tank core has already cast the rules' face, across and up onto her hull or turret for that round)
			const LandKit::Vehicle& V = St.Armour->Vehicles()[static_cast<size_t>(St.VehicleOf[K])];
			LandKit::V3 At, Normal;
			if (!V.StruckNow(Round, At, Normal))
			{
				return false;
			}
			OutAt = LandPlay::Vec(At.X, At.Y, At.Z);
			OutNormal = LandPlay::Vec(Normal.X, Normal.Y, Normal.Z);
			return true;
		};
	}
	{
		// (a roof gun only where her model draws one this month and for her nation: the tank modeller's swaps)
		TWeakPtr<FLandBattleState> Weak(State);
		PlayWorld.RoofGun = [Weak](int Track)
		{
			const TSharedPtr<FLandBattleState> Pinned = Weak.Pin();
			if (!Pinned.IsValid() || !Pinned->Armour || Track < 0 || static_cast<size_t>(Track) >= Pinned->VehicleOf.size() || Pinned->VehicleOf[static_cast<size_t>(Track)] < 0)
			{
				return true;
			}
			return Pinned->Armour->Vehicles()[static_cast<size_t>(Pinned->VehicleOf[static_cast<size_t>(Track)])].Look().RoofGun();
		};
	}
	S.Play.Begin(Rec, PlayWorld);
	MakeRiders();

	// The camera's world: the ground drawn, where it can stand, what it can see through, where each man and tank is.
	{
		LandEye::World EyeWorld;
		EyeWorld.Height = PlayWorld.Height;
		EyeWorld.Free = [FieldPtr](const LandPlay::Vec& At)
		{
			LandField::Drawn What2 = LandField::Drawn::Pasture;
			const double Z2 = FieldPtr->HeightAt(At.X, At.Y, &What2);
			if (What2 == LandField::Drawn::Floor || What2 == LandField::Drawn::Wall)
			{
				return false;
			}
			// (on a bank the hedge's shrubs stand to some 3 m: the eye is in them below that)
			return !(What2 == LandField::Drawn::Bank && At.Z < Z2 + 3.2);
		};
		EyeWorld.Open = [FieldPtr](const LandPlay::Vec& From, const LandPlay::Vec& To)
		{
			// Every 2 m along the line, what stands there: a bank and its hedge 3 m, a wall 2, a house 7, a wood 14.
			const LandPlay::Vec Way = To - From;
			const double Long = Way.Length();
			const int Steps = FMath::Clamp(static_cast<int>(Long / 2.0), 2, 200);
			for (int K = 1; K < Steps; ++K)
			{
				const double F = static_cast<double>(K) / Steps;
				if (F * Long < 1.5 || (1.0 - F) * Long < 2.5)
				{
					continue; // (what stands at the eye's own feet, or round the subject, is not in the way)
				}
				const LandPlay::Vec At = From + Way * F;
				LandField::Drawn What2 = LandField::Drawn::Pasture;
				const double Z2 = FieldPtr->HeightAt(At.X, At.Y, &What2);
				double Top = Z2;
				switch (What2)
				{
				case LandField::Drawn::Bank: Top += 3.0; break;
				case LandField::Drawn::Wall: Top += 2.0; break;
				case LandField::Drawn::Floor: Top += 7.0; break;
				case LandField::Drawn::Woods: Top += 14.0; break;
				default: break;
				}
				if (At.Z < Top)
				{
					return false;
				}
			}
			return true;
		};
		// (the standing corn or tall grass round a place: the tallest the field lays within 0.7 m, from its own clutter,
		// the same stalks it draws; the camera keeps a lens near the ground over their ears)
		EyeWorld.Crop = [FieldPtr](double X, double Y)
		{
			std::vector<LandField::Placed> Near;
			FieldPtr->Clutter(X - 0.7, Y - 0.7, X + 0.7, Y + 0.7, Near);
			const double Ground = FieldPtr->HeightAt(X, Y); // (At's Z, a fifth of its cost: no normal or word asked)
			double Top = 0.0;
			for (const LandField::Placed& One : Near)
			{
				if (One.Kind != LandField::Base::Wheat && One.Kind != LandField::Base::TallGrass)
				{
					continue;
				}
				const double High = static_cast<double>(One.Z) + static_cast<double>(FieldPtr->BaseOf(One.Kind).Height) * static_cast<double>(One.ScaleZ);
				Top = FMath::Max(Top, High - Ground);
			}
			return Top;
		};
		TWeakPtr<FLandBattleState> Weak(State);
		EyeWorld.Where = [Weak](int Track, double T, LandPlay::Vec& OutAt, double& OutFacing)
		{
			(void)T;
			const TSharedPtr<FLandBattleState> Pinned = Weak.Pin();
			if (!Pinned.IsValid() || Track < 0)
			{
				return false;
			}
			FLandBattleState& St = *Pinned;
			const size_t K = static_cast<size_t>(Track);
			if (K < St.VehicleOf.size() && St.VehicleOf[K] >= 0 && St.Armour)
			{
				const LandKit::Vehicle& V = St.Armour->Vehicles()[static_cast<size_t>(St.VehicleOf[K])];
				OutAt = LandPlay::Vec(V.Root.At.X, V.Root.At.Y, V.Root.At.Z);
				OutFacing = V.Heading + FMath::DegreesToRadians(V.TurretDeg);
				return true;
			}
			if (K < St.ManOf.size() && St.ManOf[K] >= 0 && St.Men)
			{
				double X = 0.0, Y = 0.0, Z = 0.0;
				float Facing = 0.0f;
				if (St.Men->PlaceOf(St.ManOf[K], X, Y, Z, Facing))
				{
					OutAt = LandPlay::Vec(X, Y, Z);
					OutFacing = Facing;
					return true;
				}
			}
			return false;
		};
		// (what a point is inside, for the art director's rules on the lens: the field's things and the men and vehicles
		// placed this frame; where a man's body is, so his fall is followed to the ground)
		EyeWorld.Solid = [Weak](const LandPlay::Vec& At, int Except)
		{
			const TSharedPtr<FLandBattleState> Pinned = Weak.Pin();
			return Pinned.IsValid() ? static_cast<int>(Pinned->Sight.At(At, Except)) : 0;
		};
		EyeWorld.Body = [Weak](int Track, double T, LandPlay::Vec& OutAt)
		{
			(void)T;
			const TSharedPtr<FLandBattleState> Pinned = Weak.Pin();
			if (!Pinned.IsValid() || Track < 0)
			{
				return false;
			}
			FLandBattleState& St = *Pinned;
			const size_t K = static_cast<size_t>(Track);
			if (K < St.VehicleOf.size() && St.VehicleOf[K] >= 0 && St.Armour)
			{
				const LandKit::Vehicle& V = St.Armour->Vehicles()[static_cast<size_t>(St.VehicleOf[K])];
				OutAt = LandPlay::Vec(V.Root.At.X, V.Root.At.Y, V.Root.At.Z + 1.3);
				return true;
			}
			if (K < St.ManOf.size() && St.ManOf[K] >= 0 && St.Men)
			{
				LandPlay::Vec Head, Chest;
				if (LandParts::ManPoints(St, St.ManOf[K], Head, Chest))
				{
					OutAt = Chest;
					return true;
				}
			}
			return false;
		};
		// (an enemy unit in a wood the player has not found: the film never shows it, so it never gives the ambush away;
		// the general's screen keeps the list, General.Hides)
		EyeWorld.Hidden = [Weak](int Block)
		{
			const TSharedPtr<FLandBattleState> Pinned = Weak.Pin();
			return Pinned.IsValid() && Pinned->General.Hides(Block);
		};
		LandEye::Settings Set;
		Set.Own = 0;
		Set.Seed = Rec.Seed ? Rec.Seed : 1;
		float Hand = 1.0f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandHand="), Hand);
		Set.Hand = FMath::Clamp(static_cast<double>(Hand), 0.0, 3.0);
		// (the film's shallow focus, E9: OFF until the Mac's A/B holds 45 fps and the art director passes its stills, the
		// production lead's rule for every new look, 22 Nov 23:28Z; "-TheaterLandFocusLive=1" turns it on. Off, the camera
		// asks nothing for it a frame)
		{
			int32 FocusLive = 0;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterLandFocusLive="), FocusLive);
			bFocusLive = FocusLive != 0;
			if (!bFocusLive)
			{
				Set.FocusFar = 0.0;
			}
		}
		// (the production lead's A/B: the whole mass's wide, the dearest shot until the men are instanced, turned off)
		if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoMassWide")))
		{
			Set.MassFrom = 0.0;
		}
		// (the dark rule's reading of the drawn frame, when the game has its viewport to read)
		StartDarkRead();
		if (DarkGrab.IsValid())
		{
			TWeakObjectPtr<ALandBattle> Self(this);
			EyeWorld.DarkNow = [Self]() { return Self.IsValid() ? Self->DarkShare : -1.0; };
		}
		S.EyeWorld = EyeWorld;
		S.Eye.Begin(Rec, EyeWorld, Set);
	}
	// The night before the attack, where the recording carries the patrol (LandNight).
	StartNight();
	S.Desk.Begin(Rec, 0);
	S.Mix.bThudFromClips = S.Men != nullptr;
	// A bang crosses the ground in life's seconds (Far / 343), not the battle's: the mix's flash-to-bang reads Life.
	S.Mix.Life = S.Life;

	// The effects kit: shells, guns, tracer, fires, the camera's jolt; the ground under a point is the ground drawn.
	Effects = MakeUnique<FBattleEffects>();
	if (!Effects->Begin(Root, UnitsPerMetre, [this, FieldPtr](const FVector& Point)
		{
			return FVector(Point.X, Point.Y, FieldPtr->HeightAt(Point.Y / UnitsPerMetre, Point.X / UnitsPerMetre) * UnitsPerMetre);
		}))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: the effects kit did not begin (its recipe or material is missing): no guns, shells or smoke are drawn."));
		Effects.Reset();
	}
	TowardSun = [&]()
	{
		const FVector Way = LandParts::SunWay(Rec.Weather.Lat, LandParts::MonthOf(Rec.Date), Rec.Weather.Hour);
		return WorldWay(Way.X, Way.Y, Way.Z).GetSafeNormal();
	}();
	if (Effects)
	{
		// (the camera stands among the bursts: smoke and dust fade as they come within a few metres in front of the
		// lens, and nothing is laid at or behind it; the kit's flat painted craters give way to the torn earth below)
		Effects->SetLensFade(1.0f, 5.0f, 0.3f);
		// (the art director, 22 November: the holes are the torn earth's alone, never the kit's marks over them too;
		// -TheaterLandKitScars brings the kit's back for the A/B)
		const bool bKitScars = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandKitScars"));
		Effects->SetCraters(bKitScars);
		UE_LOG(LogTheater, Display, TEXT("Land battle: the effects kit's painted crater marks %s"),
			bKitScars ? TEXT("ON (-TheaterLandKitScars: drawn over the torn-earth holes)") : TEXT("off: the torn-earth holes alone"));
		Effects->SetSun(TowardSun);
		// (a summer morning's breeze from the west-south-west, some 3 m a second: the smoke leans east over the field;
		// or the recording's own wind)
		Effects->SetWind(Rec.Weather.bWind ? WorldWay(Rec.Weather.WindX, Rec.Weather.WindY, 0.0) : WorldWay(2.6, 1.2, 0.0));
	}

	// The shell holes (LandCrater): torn earth laid on the ground drawn, for the whole battle, the thrown soil lit as
	// the ground round it is (by the near ground's own normals: lit by its own slope, a hole's edge drew a ruled polygon
	// in a low sun; the art director's cut 4, shot 07). (Their heights, and the tracks', by HeightAt, At's height alone
	// at a fifth of its cost, now that each point asks the ground's normal as well.)
	S.Holes.Begin([FieldPtr](double X, double Y) { return FieldPtr->HeightAt(X, Y); }, Rec.Seed ? Rec.Seed : 1,
		[FieldPtr](double X, double Y) { return FieldPtr->ColourAt(X, Y); });
	const LandCrater::UpFn GroundLit = [FieldPtr](double X, double Y, double& NX, double& NY, double& NZ) { FieldPtr->DrawnUp(X, Y, NX, NY, NZ); };
	S.Holes.Lit = GroundLit;
	// (the thrown soil laid on the ground as drawn, its flat triangles, so its thin edge never sinks under them)
	S.Holes.Drawn = [FieldPtr](double X, double Y) { return FieldPtr->DrawnZ(X, Y); };
	// (-TheaterLandCraterOldEdge: the crater's edge as before the art director's 23 November seam, for the A/B)
	S.Holes.bOldEdge = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandCraterOldEdge"));
	// (Brian, 22 November: "explosions should create craters": a mortar bomb's or a shell's hole dug into open ground,
	// the ground drawn cut away over its bowl; "-TheaterLandShallowHoles" lays them all over the ground as before)
	if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterLandShallowHoles")))
	{
		// (nor within some metres of a tree, a gate, a well or a cart standing in the open: it would hang over the bowl)
		TSharedRef<TSet<int64>> Things = MakeShared<TSet<int64>>();
		const auto SquareKey = [](int32 I, int32 J) { return (static_cast<int64>(I) << 32) ^ static_cast<int64>(static_cast<uint32>(J)); };
		for (int32 Kind = 0; Kind < static_cast<int32>(LandField::Base::Tuft); ++Kind)
		{
			for (const LandField::Placed& One : FieldPtr->Instances(static_cast<LandField::Base>(Kind)))
			{
				for (int32 J = FMath::FloorToInt32((One.Y - 2.5) / 2.0); J <= FMath::FloorToInt32((One.Y + 2.5) / 2.0); ++J)
				{
					for (int32 I = FMath::FloorToInt32((One.X - 2.5) / 2.0); I <= FMath::FloorToInt32((One.X + 2.5) / 2.0); ++I)
					{
						Things->Add(SquareKey(I, J));
					}
				}
			}
		}
		S.Holes.Dig = [Open = LandCrater::Field::DigOn(*FieldPtr), Things, SquareKey](double X, double Y)
		{
			return Open(X, Y) && !Things->Contains(SquareKey(FMath::FloorToInt32(X / 2.0), FMath::FloorToInt32(Y / 2.0)));
		};
	}
	else
	{
		S.Holes.Dig = [](double, double) { return false; };
	}
	S.Dug.Begin(&FieldPtr->Tiles());
	if (Rec.Ground.Steppe.bOn)
	{
		S.Holes.SetEarth("blackearth"); // (the steppe's chernozem, not the bocage's loam)
	}
	else if (*FieldPtr->EarthWord())
	{
		S.Holes.SetEarth(FieldPtr->EarthWord()); // (a region's own: the desert's sand, Italy's red earth, ...)
	}
	// And the tracks' marks, on the same ground and paint (none across a dug hole's bowl).
	S.Ruts.Begin([FieldPtr](double X, double Y) { return FieldPtr->HeightAt(X, Y); }, [FieldPtr](double X, double Y) { return FieldPtr->ColourAt(X, Y); },
		LandCrater::Ruts::KindOn(*FieldPtr));
	S.Ruts.Snow = FieldPtr->SnowLying();
	{
		const LandCrater::Field* DugHoles = &S.Holes;
		S.Ruts.DugAt = [DugHoles](double X, double Y) { return DugHoles->InCut(X, Y); };
	}
	S.Ruts.Lit = GroundLit; // (each strip lit as the ground under it: a flat stripe no more)

	Took = LandParts::Seconds();
	LayField();
	LayScars();
	LayOldRuts();
	const double LaySeconds = LandParts::Seconds() - Took;
	MakeFlags();
	HideTheTable();

	Eye = Here->SpawnActor<ACameraActor>();
	if (Eye)
	{
		UCameraComponent* Lens = Eye->GetCameraComponent();
		Lens->SetFieldOfView(70.0f);
		Lens->bConstrainAspectRatio = false;
		// (a narrow shutter: the staccato, crisp look of a war film's battle, not a smear)
		Lens->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Lens->PostProcessSettings.MotionBlurAmount = 0.0f;
		// (the recording's grade where it gives one, over the look's own where its ground is not the bocage's: LandScape)
		const LandScape::Air LookAir = LandScape::AirOf(LandScape::LookOf(FieldPtr->LookWord()), FieldPtr->SnowLying());
		LandParts::Grade(Lens->PostProcessSettings, &Rec.Weather, &LookAir);
		// The grain of the film stock, a little (the art director's to tune: "-TheaterLandGrain=0.3").
		float Grain = 0.25f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandGrain="), Grain);
		Lens->PostProcessSettings.bOverride_FilmGrainIntensity = true;
		Lens->PostProcessSettings.FilmGrainIntensity = FMath::Clamp(Grain, 0.0f, 1.0f);
	}
	// The effects kit under the land battle's grade (the effects thread's dbebfe02, for cut 6): its colours written for
	// the grade (the clods earth brown, the yellow tracer yellow); fire keeps its orange through the grade (M_FireSpread
	// on the eye's lens); a flame or a lying piece meets the ground and the hulls softly (0.1 m, the art director's
	// 14:16Z ruling). "-TheaterLandFireSpread=0" and "-TheaterLandSoftContact=0" leave those passes off for the A/B.
	if (Effects && Eye)
	{
		Effects->SetLandGrade(true);
		Effects->SetFromAbove(true); // (inert while game.from_above.on is 0; the Mac flips it with -TheaterFxSet=game.from_above.on=1)
		int32 FireSpread = 1, SoftContact = 1;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandFireSpread="), FireSpread);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandSoftContact="), SoftContact);
		const bool bSpread = FireSpread != 0 && Effects->SetFireSpread(Eye->GetCameraComponent()->PostProcessSettings);
		const bool bSoft = SoftContact != 0 && Effects->SetSoftContact(0.1f);
		UE_LOG(LogTheater, Display, TEXT("Land battle: the effects kit's land grade on; fire spread %s; soft contact %s"),
			bSpread ? TEXT("on") : FireSpread == 0 ? TEXT("off (-TheaterLandFireSpread=0)") : TEXT("off (no M_FireSpread: rebuild TC_REBUILD=M_BattlePuffs,M_FireSpread)"),
			bSoft ? TEXT("on, 0.1 m") : SoftContact == 0 ? TEXT("off (-TheaterLandSoftContact=0)") : TEXT("off (no M_BattlePuffsSoft: rebuild TC_REBUILD=M_BattlePuffsSoft)"));
	}
	// The shade where things meet the ground, for the battle only (the graphics audit, 22 November): the map's speed
	// trims switched off the soft shade in corners and under things, the contact shadows, and the finer sun shadows for
	// the whole game, and a battle never had them back, so men and tanks floated on the field. Put back here, and the
	// map's own again when the battle ends. "-TheaterLandMapLight" keeps the map's trims in the battle.
	if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterLandMapLight")))
	{
		static const TPair<const TCHAR*, const TCHAR*> BattleLight[] = {
			{ TEXT("r.AmbientOcclusionLevels"), TEXT("-1") },
			{ TEXT("r.ContactShadows"), TEXT("1") },
			{ TEXT("sg.ShadowQuality"), TEXT("3") },
		};
		for (const TPair<const TCHAR*, const TCHAR*>& One : BattleLight)
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(One.Key))
			{
				KeptTrims.Add(TPair<FString, FString>(One.Key, Variable->GetString()));
				Variable->Set(One.Value, ECVF_SetByCode);
			}
		}
		// (the sun's contact shadow a world length, 0.3 m: enough to ground a track or a boot, too short to black out a
		// hedge, where at 0.05 of the screen every leaf's ray to the sun met another leaf. The art director on cut 4, fix
		// 1. "-TheaterLandContact=metres"; 0 keeps the screen's 0.05)
		if (BattleKey.IsValid())
		{
			float Contact = 0.3f;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterLandContact="), Contact);
			KeptContactShadow = BattleKey->ContactShadowLength;
			bKeptContactInWorld = BattleKey->ContactShadowLengthInWS;
			BattleKey->ContactShadowLengthInWS = Contact > 0.0f;
			BattleKey->ContactShadowLength = Contact > 0.0f ? FMath::Clamp(Contact, 0.02f, 3.0f) * static_cast<float>(UnitsPerMetre) : 0.05f;
			BattleKey->MarkRenderStateDirty();
		}
	}
	// The film's shallow focus (the graphics audit's E9, Brian's D3): depth of field drawn only while the camera holds a
	// close follow shot (LandEye::Pose::Focus), the map's trims keeping it off everywhere else; put back as it ends.
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DepthOfFieldQuality")))
		{
			FocusQualityKept = Variable->GetInt();
			FocusQualityNow = FocusQualityKept;
			KeptTrims.Add(TPair<FString, FString>(TEXT("r.DepthOfFieldQuality"), Variable->GetString()));
		}
	}
	// No occlusion queries while the battle is on (release 1.5's 45 fps gate; the Mac's diagnostic sitting, 23 November
	// 06:20Z, Normandy seed 11). The render thread's visibility pass took 15 to 18 ms of a 21 ms frame, much of it asking
	// which of the battle's thousands of men, props and hedges were hidden; with the queries off the Mac ran about 4 fps
	// faster (48 and 49 at 60 s against 43 and 45), and nothing on screen changes: a query only spares drawing what is
	// hidden. The map's own setting again as the battle ends (KeptTrims). "-TheaterLandOcclusion=1" keeps the queries.
	{
		int32 Occlusion = 0;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandOcclusion="), Occlusion);
		IConsoleVariable* Variable = Occlusion == 0 ? IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowOcclusionQueries")) : nullptr;
		if (Variable)
		{
			KeptTrims.Add(TPair<FString, FString>(TEXT("r.AllowOcclusionQueries"), Variable->GetString()));
			Variable->Set(TEXT("0"), ECVF_SetByCode);
		}
		UE_LOG(LogTheater, Display, TEXT("Land battle: occlusion queries %s"), Variable ? TEXT("off (-TheaterLandOcclusion=1 keeps them)") : TEXT("as the map has them"));
	}
	int32 LandSize = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandSize="), LandSize) && LandSize >= 40 && LandSize <= 100)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
		{
			KeptScreenPercent = Variable->GetFloat();
			Variable->Set(static_cast<float>(LandSize), ECVF_SetByCode);
		}
	}
	// (the eye stood on the opening shot before it is the view: spawned at the world's middle, its first frame was
	// drawn from there)
	if (Eye)
	{
		S.Pose = S.Eye.Current();
		AimEye(0.0f);
	}
	if (APlayerController* Controller = Here->GetFirstPlayerController())
	{
		if (Eye)
		{
			Controller->SetViewTarget(Eye);
			if (Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
			}
		}
		if (ACampaignHUD* Hud = Cast<ACampaignHUD>(Controller->GetHUD()))
		{
			Hud->ShowFrame(false);
		}
	}

	// The picture run: the art director's frames, each asked of the camera a little before it is taken.
	if (!ShotFolder.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*ShotFolder, true);
		// (the houses the village still can frame, found once: 20 to 50 ms, the picture run only; the camera director)
		State->Eye.FindHouses();
		const double Contact = Rec.Contact >= 0.0 ? Rec.Contact : 30.0;
		auto Kind = [](LandEye::Kind K) { return static_cast<int32>(K); };
		// (a man of the attack who stands through the moment, for the shots among the men)
		auto Standing = [&Rec](int Side, double At, bool bVehicle) -> int32
		{
			for (int32 K = 0; K < static_cast<int32>(Rec.Tracks.size()); ++K)
			{
				const LandFeed::Track& T = Rec.Tracks[static_cast<size_t>(K)];
				const LandFeed::Block* B = Rec.BlockOf(T.Block);
				if (!B || B->Side != Side || T.bTwin || T.IsVehicle() != bVehicle || T.Start() > At || (T.Fell >= 0.0 && T.Fell < At + 4.0) || (T.Out >= 0.0 && T.Out < At + 4.0))
				{
					continue;
				}
				if (!bVehicle && (T.Role == "mortar" || T.Role == "fo"))
				{
					continue;
				}
				return K;
			}
			return -1;
		};
		Shots.Add({ TEXT("01-opening"), 4.0, -1, -1 });
		Shots.Add({ TEXT("02-the-advance"), FMath::Max(12.0, Contact - 14.0), Kind(LandEye::Kind::Wide), -1 });
		Shots.Add({ TEXT("03-among-the-men"), Contact + 12.0, Kind(LandEye::Kind::Shoulder), Standing(0, Contact + 10.0, false) });
		Shots.Add({ TEXT("04-in-the-grass"), Contact + 45.0, Kind(LandEye::Kind::Low), Standing(0, Contact + 43.0, false) });
		// (the first tank of the attack to fire, a moment after her shot)
		double FirstTankShot = -1.0;
		int32 FirstTank = -1;
		for (int32 K = 0; K < static_cast<int32>(Rec.Tracks.size()); ++K)
		{
			const LandFeed::Track& T = Rec.Tracks[static_cast<size_t>(K)];
			if (T.Role == "tank" && !T.Shots.empty() && (FirstTankShot < 0.0 || T.Shots.front() < FirstTankShot))
			{
				FirstTankShot = T.Shots.front();
				FirstTank = K;
			}
		}
		if (FirstTank >= 0)
		{
			Shots.Add({ TEXT("05-the-tanks"), FirstTankShot + 0.25, Kind(LandEye::Kind::Tank), FirstTank });
		}
		// (the first man to fall, a moment after; and the first tank knocked out)
		int32 FirstFall = -1, FirstKill = -1;
		for (int32 K = 0; K < static_cast<int32>(Rec.Tracks.size()); ++K)
		{
			const LandFeed::Track& T = Rec.Tracks[static_cast<size_t>(K)];
			if (T.Fell < 0.0 || T.bTwin)
			{
				continue;
			}
			int32& Into = T.IsVehicle() ? FirstKill : FirstFall;
			if (Into < 0 || T.Fell < Rec.Tracks[static_cast<size_t>(Into)].Fell)
			{
				Into = K;
			}
		}
		if (FirstFall >= 0)
		{
			Shots.Add({ TEXT("06-a-man-falls"), Rec.Tracks[static_cast<size_t>(FirstFall)].Fell + 0.8, Kind(LandEye::Kind::Watch), FirstFall });
		}
		if (FirstKill >= 0)
		{
			Shots.Add({ TEXT("07-a-tank-knocked-out"), Rec.Tracks[static_cast<size_t>(FirstKill)].Fell + 2.0, Kind(LandEye::Kind::Watch), FirstKill });
		}
		Shots.Add({ TEXT("08-the-village"), Rec.End * 0.6, Kind(LandEye::Kind::Watch), Standing(1, Rec.End * 0.6 - 2.0, false) });
		// (the fallen: low in the grass by a man who fell, late in the fight)
		int32 Fallen = -1;
		for (int32 K = 0; K < static_cast<int32>(Rec.Tracks.size()); ++K)
		{
			const LandFeed::Track& T = Rec.Tracks[static_cast<size_t>(K)];
			if (T.IsMan() && T.Fell >= 0.0 && T.Fell < Rec.End * 0.75 && !T.bTwin)
			{
				Fallen = K;
			}
		}
		if (Fallen >= 0)
		{
			Shots.Add({ TEXT("09-the-fallen"), Rec.End * 0.82, Kind(LandEye::Kind::Low), Fallen });
		}
		// (the guns open: the machine guns' long fire after contact, watched by the gunner who fires most of it, tracer
		// across the field; the production lead's cut 6 ask. The after card stays the tenth)
		double GunsAt = -1.0;
		int GunsTrack = -1;
		if (LandFeed::GunsOpen(Rec, GunsAt, GunsTrack))
		{
			Shots.Add({ TEXT("11-the-guns-open"), GunsAt, GunsTrack >= 0 ? Kind(LandEye::Kind::Watch) : Kind(LandEye::Kind::Wide), GunsTrack });
		}
		Shots.Sort([](const FShot& A, const FShot& B) { return A.At < B.At; });
	}

	ShowScreen();
	bBegun = true;
	SetActorTickEnabled(true);
	UE_LOG(LogTheater, Display, TEXT("Land battle: %s opened: the ground built in %.2f s and laid in %.2f s, the men in %.2f s (%d), the tanks in %.2f s (%d)%s"),
		*Key, FieldSeconds, LaySeconds, MenSeconds, S.Men ? S.Men->Count() : 0, ArmourSeconds, S.Armour ? static_cast<int32>(S.Armour->Vehicles().size()) : 0,
		ShotFolder.IsEmpty() ? TEXT("") : *(TEXT(", pictures into ") + ShotFolder));
	if (S.Field)
	{
		const LandField::Tally& L = S.Field->Laid();
		UE_LOG(LogTheater, Display, TEXT("Land battle: the ground: %d parcels, %.1f km of hedges, %d lanes, %d houses (%d damaged)%s, %d instances; triangles near %d, far %d, built %d, ring %d"),
			L.Parcels, L.HedgeKm, L.Lanes, L.Houses, L.Damaged, L.bChurch ? TEXT(", a church") : TEXT(""), static_cast<int32>(L.Instances),
			static_cast<int32>(L.NearTris), static_cast<int32>(L.FarTris), static_cast<int32>(L.BuiltTris), static_cast<int32>(L.RingTris));
	}
	return true;
}

// ---- The ground ------------------------------------------------------------------------------------------------

void ALandBattle::LayField()
{
	FLandBattleState& S = *State;
	if (!S.Field)
	{
		return;
	}
	const LandField::Ground& G = *S.Field;
	const double U = UnitsPerMetre;
	TArray<FVector> P, N;
	TArray<FColor> C;
	TArray<int32> T;
	const TArray<FVector2D> NoUv;
	const TArray<FProcMeshTangent> NoTangents;
	int32 Made = 0;
	// GS8: each tile's level a static mesh of its own, drawn from the engine's cached draw commands (a procedural mesh is
	// gathered again on the draw thread every frame, in every pass); -TheaterLandTilesProcedural draws them as before.
	const bool bStaticTiles = !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandTilesProcedural"));
	// (and the torn ground's pieces baked into static meshes once they settle, SettleTorn)
	bTornBakes = !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandTornProcedural"));
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandTornSettle="), TornSettleAfter);
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandTornBakeMs="), TornBakeMs);
	TornSettleAfter = FMath::Clamp(TornSettleAfter, 0.5, 600.0);
	TornBakeMs = FMath::Clamp(TornBakeMs, 0.05, 50.0);
	const double LaidFrom = LandParts::Seconds();
	int32 StaticMade = 0, Points = 0;
	auto Lay = [&](const LandField::Mesh& M, UMaterialInstanceDynamic* Ink, bool bShadow) -> UMeshComponent*
	{
		if (M.Empty())
		{
			return nullptr;
		}
		const FString Name = FString::Printf(TEXT("LandGround%d"), Made++);
		Points += static_cast<int32>(M.Points());
		if (bStaticTiles)
		{
			if (UStaticMesh* Shape = LandParts::GroundMesh(this, FName(*(Name + TEXT("_Mesh"))), M, Ink, U))
			{
				UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, FName(*Name));
				Mesh->SetupAttachment(Root);
				Mesh->SetStaticMesh(Shape);
				Mesh->SetMaterial(0, Ink);
				Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Mesh->SetCanEverAffectNavigation(false);
				Mesh->SetCastShadow(bShadow);
				Mesh->RegisterComponent();
				GroundKept.Add(Shape);
				GroundKept.Add(Mesh);
				++StaticMade;
				return Mesh;
			}
		}
		LandParts::FromField(M, U, P, N, C, T);
		UProceduralMeshComponent* Mesh = LandParts::NewMesh(this, Root, FName(*Name), bShadow);
		Mesh->CreateMeshSection(0, P, T, N, NoUv, C, NoTangents, false);
		Mesh->SetMaterial(0, Ink);
		FieldMeshes.Add(Mesh);
		return Mesh;
	};
	auto AddTile = [&](const LandField::Tile& Tile, bool bRing)
	{
		FTileDrawn& D = Tiles.AddDefaulted_GetRef();
		D.Box = FBox2D(FVector2D(Tile.X0, Tile.Y0), FVector2D(Tile.X1, Tile.Y1));
		// (GS8: the far ring's tiles, 1 km and more off, cast no shadow: the shadows' far pages hold the field's own)
		D.Near = Lay(Tile.Near, GroundInk, !bRing);
		D.Far = Lay(Tile.Far, GroundInk, !bRing);
		D.Built = Lay(Tile.Built, ThingInk, !bRing);
		// (the near ground within 700 m, as the field's own painter switches, the far beyond: a tile of the ring has
		// only its one level)
		D.FarFrom = Tile.Far.Empty() ? 1e12 : 700.0;
		if (D.Far.IsValid())
		{
			D.Far->SetVisibility(true);
		}
		if (D.Near.IsValid())
		{
			D.Near->SetVisibility(D.FarFrom > 1e11);
		}
	};
	for (const LandField::Tile& Tile : G.Tiles())
	{
		AddTile(Tile, false);
	}
	for (const LandField::Tile& Tile : G.Ring())
	{
		AddTile(Tile, true);
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle: the ground (GS8): %d meshes, %d of them static (cached draw), %d points, %d ring tiles with no shadow, in %.2f s%s; torn ground %s"), Made, StaticMade,
		Points, static_cast<int32>(G.Ring().size()), LandParts::Seconds() - LaidFrom, bStaticTiles ? TEXT("") : TEXT(" (-TheaterLandTilesProcedural: procedural, as before)"),
		bTornBakes ? *FString::Printf(TEXT("baked static after %.1f s unchanged, within %.2f ms a frame"), TornSettleAfter, TornBakeMs) : TEXT("procedural (-TheaterLandTornProcedural)"));
	LayThings();
	LayWires();
	LayStretchers();
}

int32 ALandBattle::ShadeOf(int32 Base, uint8 R, uint8 G, uint8 B) const
{
	if (!ShadeEdges.IsValidIndex(Base))
	{
		return 4;
	}
	const float Bright = static_cast<float>(R) + static_cast<float>(G) + static_cast<float>(B);
	const int32 Third = Bright < ShadeEdges[Base].X ? 0 : Bright < ShadeEdges[Base].Y ? 1 : 2;
	const int32 Warm = Base * 3 + Third;
	if (!WarmEdges.IsValidIndex(Warm))
	{
		return Third * 3 + 1;
	}
	const float Warmth = static_cast<float>(R) - static_cast<float>(G);
	return Third * 3 + (Warmth < WarmEdges[Warm].X ? 0 : Warmth < WarmEdges[Warm].Y ? 1 : 2);
}

void ALandBattle::LayThings()
{
	const LandField::Ground& G = *State->Field;
	const double U = UnitsPerMetre;
	constexpr int32 Shades = 9;
	const int32 Kinds = static_cast<int32>(LandField::Base::Count);
	ShadeEdges.Init(FVector2f(-1.0f, 1e9f), Kinds);
	WarmEdges.Init(FVector2f(-1e9f, 1e9f), Kinds * 3);
	ClutterReach.Init(1e9f, Kinds);
	ClutterCopyOf.Init(-1, Kinds * Shades);
	ClutterTwin.Reset();
	bStalksMade = false;
	// The clutter's shades from a sample of it (it is laid round the eye later): nine squares 120 m across, laid once
	// in three rows of three over the field, so that a crop or a grass that is not in the field's middle has its own
	// tints too (one square in the middle left the corn of an open wheat map at its built colour everywhere).
	std::vector<LandField::Placed> Sample;
	if (!G.Tiles().empty())
	{
		double X0 = 1e12, Y0 = 1e12, X1 = -1e12, Y1 = -1e12;
		for (const LandField::Tile& Tile : G.Tiles())
		{
			X0 = FMath::Min(X0, Tile.X0);
			Y0 = FMath::Min(Y0, Tile.Y0);
			X1 = FMath::Max(X1, Tile.X1);
			Y1 = FMath::Max(Y1, Tile.Y1);
		}
		std::vector<LandField::Placed> Square;
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Col = 0; Col < 3; ++Col)
			{
				const double Mx = FMath::Lerp(X0, X1, (1.0 + 2.0 * Col) / 6.0), My = FMath::Lerp(Y0, Y1, (1.0 + 2.0 * Row) / 6.0);
				Square.clear();
				G.Clutter(Mx - 60.0, My - 60.0, Mx + 60.0, My + 60.0, Square);
				Sample.insert(Sample.end(), Square.begin(), Square.end());
			}
		}
		// (and a small square every 150 m over the whole field, so a grass that grows only somewhere the nine miss, the
		// state farm's at Prokhorovka, is sampled too: unsampled, it drew in its built grey. The pre-check, 22 November)
		for (double Sy = Y0 + 75.0; Sy < Y1; Sy += 150.0)
		{
			for (double Sx = X0 + 75.0; Sx < X1; Sx += 150.0)
			{
				Square.clear();
				G.Clutter(Sx - 10.0, Sy - 10.0, Sx + 10.0, Sy + 10.0, Square);
				Sample.insert(Sample.end(), Square.begin(), Square.end());
			}
		}
	}
	// The field's own draw (the battle-scale plan's GS9): the hedge shrubs and haycocks drawn plain from ShrubFarFrom
	// metres (as the build before it: the field's FarFrom at a 90 deg lens, 2.14 times it at the usual 50), their shadows
	// to ShrubShadowTo, none past ShrubsTo; the trees plain from TreeFarFrom, as crowns of some 40 triangles with no
	// shadow from CrownFrom, none past TreesTo. Each "-TheaterLand<name>=" metres; 0 leaves that step out.
	const double OldLens = 1.0 / FMath::Tan(FMath::DegreesToRadians(25.0));
	float ShrubFarFrom = -1.0f, TreeFarFrom = -1.0f, ShrubShadowTo = 250.0f, CrownFrom = 400.0f, ShrubsTo = 500.0f, TreesTo = 1200.0f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandShrubFarFrom="), ShrubFarFrom);
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandTreeFarFrom="), TreeFarFrom);
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandShrubShadowTo="), ShrubShadowTo);
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandCrownFrom="), CrownFrom);
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandShrubsTo="), ShrubsTo);
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandTreesTo="), TreesTo);
	// (and the corn's bundles cast their shadow only within CornShadowTo metres of the eye: the squares nearer than that
	// lay them in a twin mesh that casts one, the rest in one that casts none; LandCarpet's ShadowTo. 0: everywhere)
	float CornShadowTo = 25.0f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandCornShadowTo="), CornShadowTo);
	// A region's ground (GS9 by class, the frame budget of 22 November, production/theatre-budgets-1122): its kit drawn
	// by its class (LandField::BaseMesh::Class), each class at the budget's distances, its dense woods a step or more down
	// the budget's ladder of nearer ones (LandScape::Region::DrawStep). The bocage and the steppe (no region: -1) keep the
	// distances above exactly; a "-TheaterLand<name>=" still wins.
	const int32 Step = G.DrawStep();
	struct FRung
	{
		float TreePlain, Crown, Trees, ShrubPlain, Shrubs;
	};
	static const FRung Ladder[5] = { { 300.0f, 400.0f, 1200.0f, 128.0f, 500.0f }, { 200.0f, 300.0f, 1200.0f, 100.0f, 400.0f }, { 150.0f, 250.0f, 1000.0f, 80.0f, 300.0f },
		{ 120.0f, 200.0f, 800.0f, 60.0f, 250.0f }, { 80.0f, 150.0f, 600.0f, 50.0f, 200.0f } };
	float RungTreePlain = 0.0f, RungShrubPlain = 0.0f;
	if (Step >= 0)
	{
		const FRung& Rung = Ladder[FMath::Clamp(Step, 0, 4)];
		auto Said = [](const TCHAR* Key) {
			float Value = 0.0f;
			return FParse::Value(FCommandLine::Get(), Key, Value);
		};
		if (!Said(TEXT("TheaterLandCrownFrom=")))
		{
			CrownFrom = Rung.Crown;
		}
		if (!Said(TEXT("TheaterLandTreesTo=")))
		{
			TreesTo = Rung.Trees;
		}
		if (!Said(TEXT("TheaterLandShrubsTo=")))
		{
			ShrubsTo = Rung.Shrubs;
		}
		if (!Said(TEXT("TheaterLandShrubShadowTo=")))
		{
			ShrubShadowTo = FMath::Min(ShrubShadowTo, ShrubsTo);
		}
		RungTreePlain = TreeFarFrom > 0.0f ? 0.0f : Rung.TreePlain;
		RungShrubPlain = ShrubFarFrom > 0.0f ? 0.0f : Rung.ShrubPlain;
	}
	// The bocage (no region, and not the steppe), release 1.5's 45 fps gate: the ladder's third rung as its own draw. The
	// Mac's A7 on 849c1bc1 drew the old distances at 37 frames a second at 40 s and 60 s of battle, this rung at 46 and 43,
	// bound by the graphics chip and the draw thread, not by the men; the art director passed its look (24 Nov). Its
	// hedges' shadows reach 150 m, not the rung's 120: the shadowed foot of the far hedge across a field is what boxes the
	// field in. "-TheaterLandBocageRung=0..4" draws another rung (-1: the distances above); each "-TheaterLand<name>="
	// still wins.
	if (Step < 0 && !State->Rec.Ground.Steppe.bOn)
	{
		int32 BocageRung = 2;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandBocageRung="), BocageRung);
		if (BocageRung >= 0)
		{
			const FRung& Rung = Ladder[FMath::Clamp(BocageRung, 0, 4)];
			auto Said = [](const TCHAR* Key) {
				float Value = 0.0f;
				return FParse::Value(FCommandLine::Get(), Key, Value);
			};
			if (!Said(TEXT("TheaterLandShrubFarFrom=")))
			{
				ShrubFarFrom = Rung.ShrubPlain;
			}
			if (!Said(TEXT("TheaterLandTreeFarFrom=")))
			{
				TreeFarFrom = Rung.TreePlain;
			}
			if (!Said(TEXT("TheaterLandCrownFrom=")))
			{
				CrownFrom = Rung.Crown;
			}
			if (!Said(TEXT("TheaterLandTreesTo=")))
			{
				TreesTo = Rung.Trees;
			}
			if (!Said(TEXT("TheaterLandShrubsTo=")))
			{
				ShrubsTo = Rung.Shrubs;
			}
			if (!Said(TEXT("TheaterLandShrubShadowTo=")))
			{
				ShrubShadowTo = FMath::Min(150.0f, ShrubsTo);
			}
			UE_LOG(LogTheater, Display, TEXT("Land battle: the bocage drawn at the ladder's rung %d (-TheaterLandBocageRung=; -1 the old distances)"), FMath::Clamp(BocageRung, 0, 4));
		}
	}
	double SaidShrubFar = 0.0, SaidTreeFar = 0.0;
	int32 Instances = 0, Meshes = 0;
	const double Began = LandParts::Seconds();
	TArray<FTransform> Places;
	// The hedge cutters' gaps (the recording's cuts): each one's bank where she will tear it (its line's heading and
	// foot), and the hedge's shrubs standing in the gap or a little past its ends laid in copies of their own, not the
	// field's, so that shaking and throwing them down sends only those few places (the field's copies hold tens of
	// thousands, over the whole field). No cuts, nothing changes.
	struct FCutBank
	{
		double X = 0.0, Y = 0.0, Yaw = 0.0, Half = 2.0, Foot = 1.4;
	};
	TArray<FCutBank> CutBanks;
	for (const LandFeed::Cut& Cut : State->Rec.Cuts)
	{
		LandField::BankPlace BP;
		FCutBank& Bank = CutBanks.AddDefaulted_GetRef();
		Bank.Half = 0.5 * Cut.Width;
		if (G.NearestBank(Cut.X, Cut.Y, 6.0, BP) && std::hypot(BP.X - Cut.X, BP.Y - Cut.Y) <= 2.5)
		{
			Bank.X = BP.X;
			Bank.Y = BP.Y;
			Bank.Yaw = BP.Yaw;
			Bank.Foot = BP.Foot;
		}
		else
		{
			Bank.Half = -1.0; // (no bank there: nothing to tear, MakeBreaches says so)
		}
	}
	CutStage.Init(0, CutBanks.Num());
	CutsTorn = 0;
	CutShrubs.Reset();
	const auto CutAt = [&CutBanks](const LandField::Placed& One) -> int32
	{
		for (int32 K = 0; K < CutBanks.Num(); ++K)
		{
			const FCutBank& Bank = CutBanks[K];
			if (Bank.Half <= 0.0)
			{
				continue;
			}
			const double Dx = One.X - Bank.X, Dy = One.Y - Bank.Y;
			const double Along = Dx * std::cos(Bank.Yaw) + Dy * std::sin(Bank.Yaw), Across = -Dx * std::sin(Bank.Yaw) + Dy * std::cos(Bank.Yaw);
			if (std::fabs(Along) <= Bank.Half + 2.0 && std::fabs(Across) <= Bank.Foot + 1.0)
			{
				return K;
			}
		}
		return -1;
	};
	TArray<FTransform> CutPlaces;
	TArray<FCutShrub> CutFound;
	for (int32 Kind = 0; Kind < Kinds; ++Kind)
	{
		const LandField::Base Base = static_cast<LandField::Base>(Kind);
		const bool bClutter = LandParts::IsClutter(Kind);
		// (and a region's kit by its class: an olive, a cypress, a palm a tree; a bush or a boulder a shrub; an outcrop, a
		// pole or a tower big; a building a house. LandScape's KitLook or LandField sets it)
		using FDraw = LandField::BaseMesh::Draw;
		const FDraw Class = G.BaseOf(Base).Class;
		const bool bShrub = Base == LandField::Base::ShrubA || Base == LandField::Base::ShrubB || Base == LandField::Base::ShrubC || Base == LandField::Base::Haycock
			|| Class == FDraw::Shrub;
		const bool bTree = Base == LandField::Base::Oak || Base == LandField::Base::Elm || Base == LandField::Base::Ash || Base == LandField::Base::Beech
			|| Base == LandField::Base::Pollard || Base == LandField::Base::Apple || Base == LandField::Base::Birch || G.BaseOf(Base).bTree || Class == FDraw::Tree;
		const bool bBig = Class == FDraw::Big, bHouse = Class == FDraw::House;
		std::vector<const LandField::Placed*> Them;
		if (bClutter)
		{
			for (const LandField::Placed& One : Sample)
			{
				if (One.Kind == Base)
				{
					Them.push_back(&One);
				}
			}
		}
		else
		{
			for (const LandField::Placed& One : G.Instances(Base))
			{
				Them.push_back(&One);
			}
			if (Them.empty())
			{
				continue;
			}
		}
		// The kind's tints split by brightness into thirds, and the clutter's each third again by warmth (ShadeOf); each
		// shade painted with its own mean tint.
		std::vector<float> Bright;
		Bright.reserve(Them.size());
		for (const LandField::Placed* One : Them)
		{
			Bright.push_back(static_cast<float>(One->Tint.R) + One->Tint.G + One->Tint.B);
		}
		std::sort(Bright.begin(), Bright.end());
		if (!Bright.empty())
		{
			ShadeEdges[Kind] = FVector2f(Bright[Bright.size() / 3], Bright[Bright.size() * 2 / 3]);
		}
		if (bClutter)
		{
			std::vector<float> Warmth[3];
			for (const LandField::Placed* One : Them)
			{
				Warmth[ShadeOf(Kind, One->Tint.R, One->Tint.G, One->Tint.B) / 3].push_back(static_cast<float>(One->Tint.R) - One->Tint.G);
			}
			for (int32 Third = 0; Third < 3; ++Third)
			{
				std::vector<float>& W = Warmth[Third];
				std::sort(W.begin(), W.end());
				WarmEdges[Kind * 3 + Third] = W.empty() ? FVector2f(-1e9f, 1e9f) : FVector2f(W[W.size() / 3], W[W.size() * 2 / 3]);
			}
		}
		FVector3f Sum[Shades];
		int32 Number[Shades];
		for (int32 Shade = 0; Shade < Shades; ++Shade)
		{
			Sum[Shade] = FVector3f::ZeroVector;
			Number[Shade] = 0;
		}
		FVector3f All = FVector3f::ZeroVector;
		for (const LandField::Placed* One : Them)
		{
			const int32 Shade = ShadeOf(Kind, One->Tint.R, One->Tint.G, One->Tint.B);
			const FVector3f Tint(One->Tint.R / 128.0f, One->Tint.G / 128.0f, One->Tint.B / 128.0f);
			Sum[Shade] += Tint;
			++Number[Shade];
			All += Tint;
		}
		const FVector3f Mean = Them.empty() ? FVector3f(1.0f, 1.0f, 1.0f) : All / static_cast<float>(Them.size());
		const LandField::BaseMesh& Shape = G.BaseOf(Base);
		// (the corn's stalks only on their own material: without it, none, and the bundles go to the lens)
		const bool bStalks = Base == LandField::Base::Stalks;
		if (Shape.Near.Empty() || (bStalks && !StalkInk))
		{
			continue;
		}
		if (bClutter && Shape.Far.Empty())
		{
			ClutterReach[Kind] = Shape.FarFrom + 12.0f;
		}
		// Its further levels (GS9): the far one where the field has it, a shrub's again with no shadow, a tree's crown.
		TArray<LandParts::FThingLevel> Further;
		LandField::Mesh Crown;
		if (!bClutter)
		{
			double Plain = static_cast<double>(Shape.FarFrom) * OldLens;
			if (bTree && TreeFarFrom > 0.0f)
			{
				Plain = TreeFarFrom;
			}
			else if (bShrub && ShrubFarFrom > 0.0f)
			{
				Plain = ShrubFarFrom;
			}
			// (a region's ground: no plain level further off than its rung's, a house's from 300 m at most)
			if ((bTree || bBig) && RungTreePlain > 0.0f)
			{
				Plain = FMath::Min(Plain, static_cast<double>(RungTreePlain));
			}
			else if (bShrub && RungShrubPlain > 0.0f)
			{
				Plain = FMath::Min(Plain, static_cast<double>(RungShrubPlain));
			}
			else if (bHouse && Step >= 0)
			{
				Plain = FMath::Min(Plain, 300.0);
			}
			// (a region's kit: its file's middle level from MidFrom, where it has one and the plain level is well beyond)
			if (Step >= 0 && !Shape.Mid.Empty() && !Shape.Far.Empty() && Shape.MidFrom * 1.2 < Plain)
			{
				Further.Add({ &Shape.Mid, static_cast<double>(Shape.MidFrom), true });
			}
			if (!Shape.Far.Empty())
			{
				Further.Add({ &Shape.Far, Plain, true });
				if (bTree)
				{
					SaidTreeFar = Plain;
				}
				else if (bShrub)
				{
					SaidShrubFar = Plain;
				}
			}
			const LandField::Mesh& Last = Shape.Far.Empty() ? Shape.Near : Shape.Far;
			if (bShrub && ShrubShadowTo > 0.0f && (ShrubsTo <= 0.0f || ShrubShadowTo < ShrubsTo))
			{
				Further.Add({ &Last, FMath::Max(static_cast<double>(ShrubShadowTo), Plain * 1.2), false });
			}
			if (bTree && CrownFrom > 0.0f && (TreesTo <= 0.0f || CrownFrom < TreesTo))
			{
				Crown = LandField::CrownOf(Last);
				if (!Crown.Empty())
				{
					Further.Add({ &Crown, FMath::Max(static_cast<double>(CrownFrom), Plain * 1.2), false });
				}
			}
			// (a big thing casts no shadow past the trees' crowns, a house none past 1,200 m)
			if (bBig && CrownFrom > 0.0f && (TreesTo <= 0.0f || CrownFrom < TreesTo))
			{
				Further.Add({ &Last, FMath::Max(static_cast<double>(CrownFrom), Plain * 1.2), false });
			}
			else if (bHouse)
			{
				Further.Add({ &Last, FMath::Max(1200.0, Plain * 1.2), false });
			}
		}
		for (int32 Shade = 0; Shade < Shades; ++Shade)
		{
			// (a thing's shades are its brightness thirds alone, 1, 4 and 7; a clutter kind not in the sample, the middle)
			if (bClutter ? (Them.empty() && Shade != 4) : Number[Shade] == 0)
			{
				continue;
			}
			const FVector3f Tint = Number[Shade] > 0 ? Sum[Shade] / static_cast<float>(Number[Shade]) : Mean;
			// (named by its kind's number as well as its shape's name. A region may give several of its kit's slots one
			// name: the jungle's boulder, fallen log, edge curtain and thicket are all "Boulder", an island's tall palm
			// and its stump both "CoconutPalmTall". NewObject under a name its owner already holds makes the same object
			// again where it stood: the first kind's mesh and its copies were gone from under the tree Unreal was still
			// building for them, and the field fell over as it opened, "InstanceReorderTable.Num() ==
			// PerInstanceSMData.Num()" in ApplyBuildTree)
			const FString Name = FString::Printf(TEXT("LandThing_%s_%d_%d"), *LandParts::Wide(Shape.Name), Kind, Shade);
			UMaterialInterface* Paint = bStalks ? StalkInk.Get() : ThingInk.Get();
			UStaticMesh* Mesh = LandParts::ThingMesh(this, FName(*(Name + TEXT("_Mesh"))), Shape, Tint, Paint, bStalks ? nullptr : LeafInk.Get(), U, Further);
			if (!Mesh)
			{
				continue;
			}
			ThingMeshes.Add(Mesh);
			++Meshes;
			// The field's things by hierarchical instancing (each cluster of them takes its own level of detail); the
			// clutter, laid again and again round the eye, by plain instancing, unshadowed.
			UInstancedStaticMeshComponent* Copies = bClutter ? NewObject<UInstancedStaticMeshComponent>(this, FName(*Name))
															 : NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(*Name));
			Copies->SetupAttachment(Root);
			Copies->SetStaticMesh(Mesh);
			Copies->SetMaterial(0, Paint);
			if (LeafInk && Mesh->GetStaticMaterials().Num() > 1)
			{
				Copies->SetMaterial(1, LeafInk);
			}
			Copies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Copies->SetCanEverAffectNavigation(false);
			const bool bCornTwin = bClutter && Base == LandField::Base::Wheat && CornShadowTo > 0.0f;
			Copies->SetCastShadow((!bClutter || Base == LandField::Base::Wheat || bStalks) && !bCornTwin);
			// (a thing with no far level is not drawn past where the field gives it up: gates, the well, the cart, the
			// clutter)
			if (Shape.Far.Empty())
			{
				Copies->SetCullDistances(0, FMath::RoundToInt32(Shape.FarFrom * U));
			}
			// (GS9: a hedge shrub or a haycock not drawn past ShrubsTo, a tree past TreesTo: some two lines of the picture)
			else if ((bShrub && ShrubsTo > 0.0f) || ((bTree || bBig) && TreesTo > 0.0f))
			{
				Copies->SetCullDistances(0, FMath::RoundToInt32((bTree || bBig ? TreesTo : ShrubsTo) * U));
			}
			Copies->RegisterComponent();
			if (bClutter)
			{
				// (GS7: its places removed off the end only; swap-removal keeps its arrays from shrinking and growing again)
				Copies->SetRemoveSwap();
				const int32 Index = ClutterCopies.Add(Copies);
				ClutterCopyOf[Kind * Shades + Shade] = Index;
				if (bCornTwin)
				{
					// (GS9: the same corn in a mesh that casts its shadow, for the squares near the eye)
					UInstancedStaticMeshComponent* Shadowed = NewObject<UInstancedStaticMeshComponent>(this, FName(*(Name + TEXT("_Shadowed"))));
					Shadowed->SetupAttachment(Root);
					Shadowed->SetStaticMesh(Mesh);
					Shadowed->SetMaterial(0, Paint);
					if (LeafInk && Mesh->GetStaticMaterials().Num() > 1)
					{
						Shadowed->SetMaterial(1, LeafInk);
					}
					Shadowed->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Shadowed->SetCanEverAffectNavigation(false);
					Shadowed->SetCastShadow(true);
					if (Shape.Far.Empty())
					{
						Shadowed->SetCullDistances(0, FMath::RoundToInt32(Shape.FarFrom * U));
					}
					Shadowed->RegisterComponent();
					Shadowed->SetRemoveSwap();
					const int32 TwinIndex = ClutterCopies.Add(Shadowed);
					while (ClutterTwin.Num() <= TwinIndex)
					{
						ClutterTwin.Add(-1);
					}
					ClutterTwin[Index] = TwinIndex;
				}
				if (bStalks)
				{
					// (hidden until the eye comes down into the corn)
					Copies->SetVisibility(false);
					bStalksMade = true;
				}
				continue;
			}
			Places.Reset();
			CutPlaces.Reset();
			CutFound.Reset();
			const bool bHedgeShrub = CutBanks.Num() > 0 && (Base == LandField::Base::ShrubA || Base == LandField::Base::ShrubB || Base == LandField::Base::ShrubC
				|| Base == LandField::Base::Bramble);
			for (const LandField::Placed* One : Them)
			{
				if (ShadeOf(Kind, One->Tint.R, One->Tint.G, One->Tint.B) == Shade)
				{
					const int32 Cut = bHedgeShrub ? CutAt(*One) : -1;
					if (Cut < 0)
					{
						Places.Add(LandParts::PlacedAt(*One, U));
						continue;
					}
					FCutShrub& Found = CutFound.AddDefaulted_GetRef();
					Found.Cut = Cut;
					Found.Index = CutPlaces.Num();
					Found.X = One->X;
					Found.Y = One->Y;
					Found.Z = One->Z;
					Found.Yaw = One->Yaw;
					Found.Scale = One->Scale;
					Found.ScaleZ = One->ScaleZ;
					CutPlaces.Add(LandParts::PlacedAt(*One, U));
				}
			}
			Copies->AddInstances(Places, false, true);
			Instances += Places.Num();
			ThingCopies.Add(Copies);
			if (CutPlaces.Num() > 0)
			{
				// (the gap's own few: plain instancing, otherwise drawn as the field's copies are)
				UInstancedStaticMeshComponent* Own = NewObject<UInstancedStaticMeshComponent>(this, FName(*(Name + TEXT("_Cut"))));
				Own->SetupAttachment(Root);
				Own->SetStaticMesh(Mesh);
				Own->SetMaterial(0, Paint);
				if (LeafInk && Mesh->GetStaticMaterials().Num() > 1)
				{
					Own->SetMaterial(1, LeafInk);
				}
				Own->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Own->SetCanEverAffectNavigation(false);
				Own->SetCastShadow(true);
				if (Shape.Far.Empty())
				{
					Own->SetCullDistances(0, FMath::RoundToInt32(Shape.FarFrom * U));
				}
				else if (bShrub && ShrubsTo > 0.0f)
				{
					Own->SetCullDistances(0, FMath::RoundToInt32(ShrubsTo * U));
				}
				Own->RegisterComponent();
				Own->AddInstances(CutPlaces, false, true);
				Instances += CutPlaces.Num();
				ThingCopies.Add(Own);
				for (FCutShrub& Found : CutFound)
				{
					Found.Copies = Own;
					CutShrubs.Add(Found);
				}
			}
		}
	}
	// The clutter's squares (LandCarpet): each thing's mesh by its kind and shade, how near the eye a kind is laid, and
	// whether a thing still stands (not in a shell hole, not flattened by a tank).
	Carpet = MakeShared<LandCarpet::Squares>();
	// (GS7: each mesh kept packed, a place given back filled from its end and the last removed, so nothing hidden is
	// drawn; "-TheaterLandClutterShrunk" keeps the old way, given-back places shrunk to nothing where they stood)
	Carpet->bCompact = !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandClutterShrunk"));
	Carpet->Reach.assign(static_cast<size_t>(Kinds), 1e9);
	for (int32 Kind = 0; Kind < Kinds; ++Kind)
	{
		Carpet->Reach[static_cast<size_t>(Kind)] = static_cast<double>(ClutterReach[Kind]);
	}
	Carpet->CopyOf = [this](const LandField::Placed& One) -> int32_t
	{
		const int32 Kind = static_cast<int32>(One.Kind);
		const int32 Slot = Kind * Shades + ShadeOf(Kind, One.Tint.R, One.Tint.G, One.Tint.B);
		return ClutterCopyOf.IsValidIndex(Slot) ? ClutterCopyOf[Slot] : -1;
	};
	Carpet->Stands = [this](const LandField::Placed& One) -> bool
	{
		return !State.IsValid() || (!State->Holes.Bare(One.X, One.Y) && !State->Ruts.Flat(One.X, One.Y));
	};
	// (the corn's clearing round the lens: none until the eye is low, LayClutter)
	Carpet->Clear.assign(static_cast<size_t>(Kinds), 0.0);
	// (the corn's shadows only near the eye, GS9: its squares within CornShadowTo laid in its meshes' shadowed twins)
	Carpet->ShadowTo.assign(static_cast<size_t>(Kinds), 0.0);
	Carpet->Twin.assign(static_cast<size_t>(ClutterCopies.Num()), -1);
	for (int32 K = 0; K < ClutterTwin.Num() && K < ClutterCopies.Num(); ++K)
	{
		Carpet->Twin[static_cast<size_t>(K)] = ClutterTwin[K];
	}
	if (CornShadowTo > 0.0f)
	{
		Carpet->ShadowTo[static_cast<size_t>(LandField::Base::Wheat)] = CornShadowTo;
	}
	bStalksShown = false;
	UE_LOG(LogTheater, Display, TEXT("Land battle: the ground's things: %d meshes, %d instances, made in %.2f s"), Meshes, Instances, LandParts::Seconds() - Began);
	UE_LOG(LogTheater, Display, TEXT("Land battle: the field's draw (GS9): hedge shrubs and haycocks plain from %.0f m, their shadows to %.0f m, none past %.0f m; trees plain from %.0f m, 42-triangle crowns with no shadow from %.0f m, none past %.0f m; the corn's shadows within %.0f m (0: that step off); the clutter's given-back places %s"),
		SaidShrubFar, ShrubShadowTo, ShrubsTo, SaidTreeFar, CrownFrom, TreesTo, CornShadowTo,
		Carpet->bCompact ? TEXT("removed, each mesh kept packed (GS7)") : TEXT("kept shrunk to nothing (-TheaterLandClutterShrunk)"));
	if (Step >= 0)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: a region's ground, step %d of the frame budget's ladder: its kit by class, trees plain from %.0f m at most, shrubs from %.0f m, houses from 300 m and never culled"),
			Step, RungTreePlain, RungShrubPlain);
	}
}

void ALandBattle::ShowField(const FVector& EyeWorld, double Battle)
{
	LAND_SCOPE(ShowField);
	(void)Battle;
	const FVector2D EyeAt(EyeWorld.Y / UnitsPerMetre, EyeWorld.X / UnitsPerMetre);
	// The ground's level by the eye's distance to each tile (the things choose their own, by instancing).
	for (int32 TileIndex = 0; TileIndex < Tiles.Num(); ++TileIndex)
	{
		FTileDrawn& Tile = Tiles[TileIndex];
		const double Far = FMath::Sqrt(Tile.Box.ComputeSquaredDistanceToPoint(EyeAt));
		const bool bNear = Far < Tile.FarFrom;
		if (bNear != Tile.bNearShown && Tile.Near.IsValid() && Tile.Far.IsValid())
		{
			Tile.bNearShown = bNear;
			// (once dug in, its parts stand in for its near mesh, which stays hidden; a part settled is drawn by its baked
			// mesh, GS8)
			Tile.Near->SetVisibility(bNear && Tile.NearParts.IsEmpty());
			for (int32 Part = 0; Part < Tile.NearParts.Num(); ++Part)
			{
				if (Tile.NearParts[Part].IsValid())
				{
					Tile.NearParts[Part]->SetVisibility(bNear);
				}
				if (const FTornPiece* Piece = TornPieces.Find(TornKey(2, TileIndex, Part)))
				{
					if (UStaticMeshComponent* Baked = Piece->Baked.Get())
					{
						Baked->SetVisibility(bNear);
					}
				}
			}
			Tile.Far->SetVisibility(!bNear);
		}
	}
	// The grass round the eye and round what it looks at: its squares coming into reach laid, those leaving it given
	// back, every frame (nothing to do while the eye stays within its squares).
	const FVector2D LookAt(State->Pose.Look.X, State->Pose.Look.Y);
	LandFell::Phase("ground: the clutter");
	LayClutter(EyeAt, LookAt, State->Pose.bCut, EyeWorld.Z / UnitsPerMetre - GroundZ(EyeAt.X, EyeAt.Y));
}

void ALandBattle::LayClutter(const FVector2D& EyeAt, const FVector2D& LookAt, bool bCut, double EyeHigh)
{
	LAND_SCOPE(Clutter);
	if (ClutterCopies.Num() == 0 || !State->Field || !Carpet.IsValid())
	{
		return;
	}
	// The corn stalk by stalk round a low lens, its bundles cleared there (the art director's cut 4, fix 4); from higher
	// the bundles again and no stalks (the carpet gives the cleared bundles back in this update).
	if (bStalksMade)
	{
		const bool bLow = bStalksShown ? EyeHigh < LandParts::StalkEyeHigh : EyeHigh < LandParts::StalkEyeLow;
		if (bLow != bStalksShown)
		{
			bStalksShown = bLow;
			const int32 Kind = static_cast<int32>(LandField::Base::Stalks);
			const int32 KindShades = ClutterCopyOf.Num() / static_cast<int32>(LandField::Base::Count);
			for (int32 Shade = 0; Shade < KindShades; ++Shade)
			{
				const int32 Slot = Kind * KindShades + Shade;
				if (ClutterCopyOf.IsValidIndex(Slot) && ClutterCopies.IsValidIndex(ClutterCopyOf[Slot]) && ClutterCopies[ClutterCopyOf[Slot]])
				{
					ClutterCopies[ClutterCopyOf[Slot]]->SetVisibility(bLow);
				}
			}
			const size_t Wheat = static_cast<size_t>(LandField::Base::Wheat);
			if (Wheat < Carpet->Clear.size())
			{
				Carpet->Clear[Wheat] = bLow ? LandParts::StalkClear : 0.0;
			}
		}
	}
	const int32 Copies = ClutterCopies.Num();
	// The squares brought up to date: within 20 m of the eye or what it looks at made now (32 m on a cut), four more a
	// frame, nearest first (the old way made the whole 90 m box again every 12 m: 2.1 to 2.9 ms on the Mac each time).
	std::vector<LandCarpet::Op> Ops;
	LandFell::Phase("ground: the carpet's squares");
	Carpet->Update(*State->Field, EyeAt.X, EyeAt.Y, LookAt.X, LookAt.Y, bCut, 4, RealT, Ops);
	if (Ops.empty())
	{
		return;
	}
	// Each mesh brought to what the squares hold now. A place's last change in this update is the one sent (a place may
	// be given back, filled from its mesh's end and taken again within one update); places past the squares' count are
	// removed from the mesh's end, new ones added at it, in order. (Kept packed, GS7, a place given back is filled with
	// its mesh's last thing and the last removed, so the mesh draws only what is laid; with -TheaterLandClutterShrunk it
	// stays, shrunk to nothing where it stood, for the next thing, as before.)
	LandFell::Phase("ground: the clutter's meshes", static_cast<int>(Ops.size()));
	const double U = UnitsPerMetre;
	size_t From = 0;
	for (size_t N = Ops.size(); N-- > 0;)
	{
		if (Ops[N].Copy < 0)
		{
			// (a cut far away: every mesh emptied whole, its places counted again from 0; what came before it is moot)
			for (int32 K = 0; K < Copies; ++K)
			{
				if (UInstancedStaticMeshComponent* Mesh = ClutterCopies[K])
				{
					Mesh->ClearInstances();
				}
			}
			From = N + 1;
			break;
		}
	}
	TArray<int32> Had, Now, Filled;
	TArray<TArray<FTransform>> Added;
	TArray<bool> Touched;
	Had.SetNumZeroed(Copies);
	Now.SetNumZeroed(Copies);
	Filled.SetNumZeroed(Copies);
	Added.SetNum(Copies);
	Touched.Init(false, Copies);
	const FTransform Nothing(FQuat::Identity, FVector(EyeAt.Y * U, EyeAt.X * U, GroundZ(EyeAt.X, EyeAt.Y) * U), FVector(1e-3));
	for (int32 K = 0; K < Copies; ++K)
	{
		if (UInstancedStaticMeshComponent* Mesh = ClutterCopies[K])
		{
			Had[K] = Mesh->GetInstanceCount();
			Now[K] = Carpet->Count(K);
			Added[K].Init(Nothing, FMath::Max(0, Now[K] - Had[K]));
		}
	}
	TSet<uint64> Seen;
	Seen.Reserve(static_cast<int32>(Ops.size() - From));
	for (size_t N = Ops.size(); N-- > From;)
	{
		const LandCarpet::Op& O = Ops[N];
		if (O.bPop || O.Slot < 0 || !ClutterCopies.IsValidIndex(O.Copy) || !ClutterCopies[O.Copy])
		{
			continue;
		}
		bool bSeen = false;
		Seen.Add((static_cast<uint64>(static_cast<uint32>(O.Copy)) << 32) | static_cast<uint32>(O.Slot), &bSeen);
		if (bSeen || O.Slot >= Now[O.Copy])
		{
			continue; // (changed again later in this update, or removed by its end)
		}
		FTransform Where = LandParts::PlacedAt(O.Thing, U);
		if (O.bHide)
		{
			Where.SetScale3D(FVector(1e-3));
		}
		if (O.Slot < Had[O.Copy])
		{
			ClutterCopies[O.Copy]->UpdateInstanceTransform(O.Slot, Where, true, false, true);
			Touched[O.Copy] = true;
		}
		else
		{
			Added[O.Copy][O.Slot - Had[O.Copy]] = Where;
			++Filled[O.Copy];
		}
	}
	for (int32 K = 0; K < Copies; ++K)
	{
		UInstancedStaticMeshComponent* Mesh = ClutterCopies[K];
		if (!Mesh)
		{
			continue;
		}
		if (Now[K] < Had[K])
		{
			// (the places given back off the mesh's end, last first: nothing after them moves. In UE 5.8 a removal of
			// the last place is a change sent with the instances, not the mesh made again: pre-check, 22 November)
			TArray<int32> Gone;
			Gone.Reserve(Had[K] - Now[K]);
			for (int32 Slot = Had[K] - 1; Slot >= Now[K]; --Slot)
			{
				Gone.Add(Slot);
			}
			Mesh->RemoveInstances(Gone, true);
			Touched[K] = true;
		}
		if (Added[K].Num() > 0 && Filled[K] == Added[K].Num())
		{
			Mesh->AddInstances(Added[K], false, true);
		}
		else if (Touched[K])
		{
			// (the moved places are already queued, each UpdateInstanceTransform; this only says so: the instances sent
			// at the frame's end, not the whole mesh made again as MarkRenderStateDirty would. Pre-check on UE 5.8's
			// InstanceDataManager, 22 November)
			Mesh->MarkRenderInstancesDirty();
		}
		// (out of step with the squares, which should never be: the mesh built again whole from what they hold)
		if (Mesh->GetInstanceCount() != Carpet->Count(K) || Filled[K] != Added[K].Num())
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle: clutter mesh %d had %d places for the squares' %d (%d of %d new ones given); built again whole"), K,
				Mesh->GetInstanceCount(), Carpet->Count(K), Filled[K], Added[K].Num());
			TArray<FTransform> All;
			All.Init(Nothing, Carpet->Count(K));
			Carpet->Each(K, [&All, U](int32_t Slot, const LandField::Placed& One)
			{
				if (All.IsValidIndex(Slot))
				{
					All[Slot] = LandParts::PlacedAt(One, U);
				}
			});
			Mesh->ClearInstances();
			Mesh->AddInstances(All, false, true);
		}
	}
}

double ALandBattle::GroundZ(double X, double Y) const
{
	if (State.IsValid() && State->Field)
	{
		return State->Field->HeightAt(X, Y);
	}
	return State.IsValid() ? State->Rec.Ground.HeightAt(X, Y) : 0.0;
}

FString ALandBattle::GroundWordAt(double X, double Y) const
{
	if (State.IsValid() && State->Field)
	{
		return FString(State->Field->At(X, Y).Fx);
	}
	return TEXT("earth");
}

// ---- The men ---------------------------------------------------------------------------------------------------

void ALandBattle::MakeMen()
{
	FLandBattleState& S = *State;
	const LandField::Ground* FieldPtr = S.Field.get();
	if (!FieldPtr)
	{
		return;
	}
	auto Crowd = std::make_unique<LandMen::Crowd>();
	Crowd->SetLife(S.Life);
	// Uniforms that age (release 2, on trial: "-TheaterLandTrial=worn"): each side's cloth faded by its months in the line,
	// the recording's sides.<side>.worn (the campaign's); "-TheaterLandNoWorn" off whatever the trial (the player's switch
	// on the Battle rules card); "-TheaterLandWorn=12" stands in for both sides' (the Mac's off/on line).
	{
		double Months = -1.0;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandWorn="), Months);
		const bool bWorn = LandParts::OnTrial("worn") && !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoWorn"));
		Crowd->SetWorn(bWorn, Months);
		if (bWorn)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: uniforms that age ON (%s)"), Months >= 0.0 ? *FString::Printf(TEXT("%.0f months in the line, both sides"), Months) : TEXT("each side's months in the line from the recording"));
		}
	}
	// Run styles (release 2, on trial: "-TheaterLandTrial=runs"; Brian's 23 Nov "not all players are running exactly the
	// same at the same time"): each man runs and advances as his own one of his file's four runners, so a squad on the
	// move is not one man copied; "-TheaterLandNoRunStyles" off whatever the trial (the player's switch).
	{
		const bool bRuns = LandParts::OnTrial("runs") && !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoRunStyles"));
		Crowd->SetRunStyles(bRuns);
		if (bRuns)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: run styles ON (each man his own runner)"));
		}
	}
	// The armies' colours and stand-ins (armies.json), read first: an army with no soldier file of its own is drawn with
	// the file its row names, else the Pole's (LandMen's MakeMan; never a side left blank).
	{
		FString ArmiesText;
		std::string ArmiesError;
		if (FFileHelper::LoadFileToString(ArmiesText, *(FPaths::ProjectContentDir() / TEXT("Data/LandBattles/armies.json")))
			&& !Crowd->ReadArmies(LandParts::Narrow(ArmiesText), ArmiesError))
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle: armies.json: %s"), *LandParts::Wide(ArmiesError));
		}
	}
	// The soldier files (Content/Models/Soldiers, the soldiers thread's): each army's own, or the one standing in.
	const FString Folder = FPaths::ProjectContentDir() / TEXT("Models/Soldiers");
	TSet<FString> Armies;
	for (const LandFeed::SideInfo& Side : S.Rec.Sides)
	{
		Armies.Add(LandParts::Wide(Side.Nat));
		if (!Side.StandIn.empty())
		{
			Armies.Add(LandParts::Wide(Side.StandIn));
		}
		if (!Side.Army.empty())
		{
			Armies.Add(LandParts::Wide(Side.Army));
		}
		// (an army with no file of its own: its row's stand-in, and the Pole's body behind it)
		for (const std::string& Own : { Side.Nat, Side.Army })
		{
			if (Own.empty() || FPaths::FileExists(Folder / (TEXT("soldier_") + LandParts::Wide(Own) + TEXT(".json"))))
			{
				continue;
			}
			const std::string Row = Crowd->StandInOf(Own);
			if (!Row.empty())
			{
				Armies.Add(LandParts::Wide(Row));
			}
			Armies.Add(TEXT("POL"));
		}
	}
	// (each army's dress for the front and the season where it is made, read only where the battle needs it: the
	// Italian's sahariana in the desert, the Soviet's winter dress and snow suit from November to March; LandMen picks)
	{
		const std::string& Date = S.Rec.Date;
		static const char* const Winter[] = { "January", "February", "March", "November", "December" };
		bool bWinter = false;
		for (const char* W : Winter)
		{
			bWinter = bWinter || Date.find(W) != std::string::npos;
		}
		const bool bDesert = S.Rec.Weather.Lat < 36.0 && S.Rec.Weather.Lon > -18.0 && S.Rec.Weather.Lon < 40.0;
		for (const FString& Army : TArray<FString>(Armies.Array()))
		{
			if (bDesert)
			{
				Armies.Add(Army + TEXT("_desert"));
			}
			if (bWinter)
			{
				// (the snow suit only where there is no winter dress: LandMen picks the winter dress first)
				const FString Dress = Army + TEXT("_winter");
				Armies.Add(FPaths::FileExists(Folder / (TEXT("soldier_") + Dress + TEXT(".json"))) ? Dress : Army + TEXT("_snow"));
			}
		}
	}
	// (and each army's files for its other men where it has them: the American's BAR gunner, squad leader, bazooka
	// gunner and his loader, soldier_USA_bar, _nco, _bazooka and _loader; a file not made yet is passed over below)
	// (and the tank commanders' files, soldier_USA_tanker and soldier_GER_panzer: ridden on their tanks, MakeRiders)
	static const TCHAR* const OtherMen[] = { TEXT("_bar"), TEXT("_nco"), TEXT("_smg"), TEXT("_bazooka"), TEXT("_loader"), TEXT("_radio"), TEXT("_tanker"), TEXT("_panzer") };
	for (const FString& Army : TArray<FString>(Armies.Array()))
	{
		for (const TCHAR* Suffix : OtherMen)
		{
			Armies.Add(Army + Suffix);
		}
	}
	int32 Files = 0;
	for (const FString& Army : Armies)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(Folder / (TEXT("soldier_") + Army + TEXT(".json")))))
		{
			continue;
		}
		auto One = std::make_shared<LandMen::Soldier>();
		std::string Error;
		if (!One->Load(LandParts::Narrow(Text), Error))
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle: soldier_%s.json: %s"), *Army, *LandParts::Wide(Error));
			continue;
		}
		Crowd->AddSoldier(LandParts::Narrow(Army), One);
		++Files;
	}
	std::string Error;
	// (the men's ground is the field's, and the shell holes' own face where one is torn: their floors and rims stand a
	// little over the drawn ground, and a body laid across a rim lies on it, not sunk in it)
	const LandCrater::Field* HolesPtr = &S.Holes;
	const LandMen::GroundFn GroundAt = [FieldPtr, HolesPtr](double X, double Y)
	{
		const LandField::Surface Under = FieldPtr->At(X, Y);
		LandMen::Ground Out;
		Out.Height = static_cast<float>(Under.Z);
		double Top = 0.0;
		// (inside a dug hole's cut the ground is gone: he stands in its bowl, under the ground's height)
		if (HolesPtr->TopAt(X, Y, Top) && (Top > Under.Z || HolesPtr->InCut(X, Y)))
		{
			Out.Height = static_cast<float>(Top);
		}
		Out.Normal = LandMen::Vec3{ static_cast<float>(Under.NX), static_cast<float>(Under.NY), static_cast<float>(Under.NZ) };
		return Out;
	};
	// The pose bank for far men (the true-scale plan's SO1; Content/Models/Soldiers/pose_bank.json, the soldiers
	// thread's): a far man shows the baked pose nearest to where his clip has got, drawn as an instance of his set's
	// mesh in that pose (DrawMen), not skinned; his play is unchanged. "-TheaterLandNoPoseBank" skins every man as
	// before.
	if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoPoseBank")))
	{
		FString BankText;
		if (!FFileHelper::LoadFileToString(BankText, *(Folder / TEXT("pose_bank.json"))))
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: no Content/Models/Soldiers/pose_bank.json: every man is skinned."));
		}
		else if (!Crowd->ReadPoseBank(LandParts::Narrow(BankText), Error))
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle: pose_bank.json: %s (every man is skinned)"), *LandParts::Wide(Error));
		}
	}
	if (Files == 0 || !Crowd->Build(S.RecText, GroundAt, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: the men could not be made (%d soldier files; %s): none are drawn."), Files, *LandParts::Wide(Error));
		return;
	}
	// (an army drawn with another's body for want of its own file: one line each, the stand-in named)
	for (const std::string& Line : Crowd->StandIns())
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: stand-in: %s"), *LandParts::Wide(Line));
	}
	if (Crowd->PoseBankOn())
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: the pose bank: %d poses, %d sets of men alike out there, baked in %.0f ms"), Crowd->BankPoses(), Crowd->BankKeys(),
			Crowd->PoseBankMs());
	}
	// (six hundred men: a man away from the eye posed every few frames, his place, shots and falls every frame)
	Crowd->SetThrift(true);
	// (the men stepped over the cores, a stretch of them each: the same men to the bit as on one; the frame-rate lane,
	// 22 November. -TheaterLandMenOneCore steps them on the game thread alone, for the A/B)
	if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterLandMenOneCore")))
	{
		Crowd->SetRun([](int Count, const std::function<void(int)>& Job)
		{
			ParallelFor(Count, [&Job](int32 K) { Job(K); });
		});
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle: the men stepped %s"),
		FParse::Param(FCommandLine::Get(), TEXT("TheaterLandMenOneCore")) ? TEXT("on the game thread alone (-TheaterLandMenOneCore)") : TEXT("over the cores"));
	// The levels of detail by the man's share of the picture (the battle-scale plan's GS3), at the usual 50 deg lens and
	// 665 lines tall: a 1.8 m man over NearPx lines (150) drawn Near, Mid to ShadowTo metres (80, where his shadow
	// ends), Far beyond; under CullPx lines (2) not drawn but in the wide mass shot. "-TheaterLandNearPx=",
	// "-TheaterLandShadowTo=", "-TheaterLandCullPx=" and "-TheaterLandRebuilds=" (a frame's most meshes made again).
	{
		float NearPx = 150.0f, ShadowTo = 80.0f, CullPx = 2.0f;
		int32 Rebuilds = 50;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandNearPx="), NearPx);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandShadowTo="), ShadowTo);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandCullPx="), CullPx);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandRebuilds="), Rebuilds);
		const double Lines = 1.8 * 665.0 / (2.0 * FMath::Tan(FMath::DegreesToRadians(25.0)));
		Crowd->SetLevels(Lines / FMath::Max(1.0f, NearPx), ShadowTo);
		S.MenCullFrom = CullPx > 0.0f ? Lines / CullPx : 1e12;
		S.MenLines = Lines;
		S.MenRebuildsMax = FMath::Clamp(Rebuilds, 1, 100000);
		UE_LOG(LogTheater, Display, TEXT("Land battle: the men drawn Near within %.1f m, Mid to %.0f m (their shadows end there), Far beyond, none past %.0f m but in the mass shot; at most %d meshes made again a frame"),
			Lines / FMath::Max(1.0f, NearPx), FMath::Max(Lines / FMath::Max(1.0f, NearPx), static_cast<double>(ShadowTo)), S.MenCullFrom, S.MenRebuildsMax);
	}
	// SO1's draw (Battle/LandVat): the mid and far men as one crowd of posed instances on M_LandCrowd (build_content.py),
	// a component for each body set and level, every pose in one picture; mid men banked too (SetPoseBankMid), so
	// nobody past the near level is skinned. Without M_LandCrowd, or with "-TheaterLandNoVat", far men are drawn a
	// mesh a set and pose and mid men skinned, as before; "-TheaterLandNoVatMid" keeps mid men skinned. In the wide
	// mass shot a far man is drawn no smaller than "-TheaterLandMassMinPx=" (2) of the picture's lines.
	{
		float MinPx = 2.0f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandMassMinPx="), MinPx);
		MassMinPx = FMath::Clamp(static_cast<double>(MinPx), 0.0, 50.0);
		// (and in the general's own view, "-TheaterLandGeneralMinPx=" (4): about two by four of the picture's points)
		float GeneralPx = 4.0f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandGeneralMinPx="), GeneralPx);
		GeneralMinPx = FMath::Clamp(static_cast<double>(GeneralPx), 0.0, 50.0);
	}
	if (Crowd->PoseBankOn() && !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoVat")))
	{
		UMaterialInterface* CrowdInk = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_LandCrowd.M_LandCrowd"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!CrowdInk || !ManInk)
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle: /Game/Campaign/Materials/M_LandCrowd is missing (run Tools/build_content.sh): far men are drawn a mesh a pose, mid men skinned."));
		}
		else
		{
			const bool bMid = !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoVatMid"));
			Crowd->SetPoseBankMid(bMid);
			Vat = MakeShared<FLandVat>(this, Root.Get(), CrowdInk, ManInk.Get(), UnitsPerMetre, VatKept);
			// (a live battle's header has no men yet: its sets are made as its pieces bring them, FLandVat::Grow from the
			// live pieces; the 40d928a6 Mac run found SO1 never made live)
			const bool bLiveEmpty = S.Rec.bLive && Crowd->Count() == 0;
			if (bLiveEmpty)
			{
				Vat->Make(*Crowd);
				UE_LOG(LogTheater, Display, TEXT("Land battle (live): the men's crowd (SO1) is made as the men come"));
			}
			else if (!Vat->Make(*Crowd))
			{
				Crowd->SetPoseBankMid(false);
				Vat.Reset();
				UE_LOG(LogTheater, Warning, TEXT("Land battle: the men's crowd (SO1) could not be made: far men are drawn a mesh a pose, mid men skinned."));
			}
			else if (bMid && !Vat->HasAll(static_cast<int32>(LandMen::Mid)))
			{
				// (a set missing at mid would show the far mesh there: every mid man skinned instead)
				Crowd->SetPoseBankMid(false);
				UE_LOG(LogTheater, Warning, TEXT("Land battle: a body set could not be made at the mid level: mid men are skinned as before."));
			}
		}
	}
	S.Men = std::move(Crowd);
	S.ManOf.assign(S.Rec.Tracks.size(), -1);
	S.TrackOfMan.assign(static_cast<size_t>(S.Men->Count()), -1);
	for (size_t K = 0; K < S.Rec.Tracks.size(); ++K)
	{
		const LandFeed::Track& T = S.Rec.Tracks[K];
		if (!T.IsMan())
		{
			continue;
		}
		const int Man = S.Men->Find(T.Block, T.K, T.J);
		S.ManOf[K] = Man;
		if (Man >= 0 && Man < S.Men->Count())
		{
			S.TrackOfMan[static_cast<size_t>(Man)] = static_cast<int>(K);
		}
	}
	MenDrawn.SetNum(S.Men->Count());
}

void ALandBattle::DrawMen(const FVector& EyeWorld)
{
	LAND_SCOPE(DrawMen);
	FLandBattleState& S = *State;
	if (!S.Men)
	{
		return;
	}
	LandMen::Crowd& Crowd = *S.Men;
	double Aspect = 16.0 / 9.0;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		Aspect = Size.Y > 0.0 ? Size.X / Size.Y : Aspect;
	}
	const LandPlay::Vec EyeAt(EyeWorld.Y / UnitsPerMetre, EyeWorld.X / UnitsPerMetre, EyeWorld.Z / UnitsPerMetre);
	// First who is drawn, and who must be skinned again (in the picture or close by, and moved since he was last
	// drawn); then the skins, spread over the cores (the drill's sections: six hundred men); then the meshes, here.
	struct FSkinJob
	{
		int32 Man = 0;
		LandMen::Lod Level = LandMen::Far;
		int32 Points = 0;
		int32 Offset = 0;
	};
	TArray<FSkinJob> Jobs;
	int32 Floats = 0;
	LandFell::Phase("men: who is drawn", Crowd.Count());
	// (the banked men's meshes made as first needed: at most four a frame; a man whose mesh waits is skinned meanwhile)
	int32 Builds = 4;
	// (a man's mesh made again for a new level of detail rebuilds its whole draw: at most MenRebuildsMax a frame, the
	// battle-scale plan's stopgap; the rest keep their level a frame or more, a new man waits to be drawn)
	int32 Rebuilds = S.MenRebuildsMax;
	// (no man drawn under some 2 of the picture's lines, GS3, but in the camera's wide of the whole army, where the far
	// men are its crowd)
	const bool bMass = S.Eye.MassShot();
	// (the general's own view, high over the field: every man drawn, the far ones no smaller than GeneralMinPx of the
	// picture's lines and at most 2.5 times their size, so a section reads as a dark dash and a platoon as a row of them;
	// the art director, 22 November 18:29Z: "the men are not on screen at all" at his height, culled under two lines)
	const bool bHigh = S.General.bOn && !S.General.bFilm;
	for (int32 Man = 0; Man < Crowd.Count(); ++Man)
	{
		FManDrawn& D = MenDrawn[Man];
		const bool bShown = Crowd.Visible(Man) && Crowd.Fade(Man) >= 0.5f && !HiddenMan(Man) && (bMass || bHigh || Crowd.SeenDistance(Man) <= S.MenCullFrom);
		double X = 0.0, Y = 0.0, Z = 0.0;
		float Facing = 0.0f;
		Crowd.PlaceAsPosed(Man, X, Y, Z, Facing); // (his height as last posed: no ground asked a man a frame)
		// (skinned only when in the picture, or close by: a man out of the picture keeps the pose he had)
		const LandPlay::Vec At(X, Y, Z + 0.9);
		const bool bLooked = (At - EyeAt).Length() < 6.0 || LandParts::InPicture(S.Pose, Aspect, At, 1.15);
		if (bShown && bLooked)
		{
			++MenInPicture;
		}
		if (!bShown || !bLooked)
		{
			if (!bShown)
			{
				if (D.Mesh.IsValid())
				{
					D.Mesh->SetVisibility(false);
				}
				BankOut(D);
				if (Vat.IsValid())
				{
					Vat->Out(Man);
				}
			}
			continue;
		}
		const int32 Pose = Crowd.BankPose(Man);
		// Banked mid or far (SO1's draw, Battle/LandVat): an instance of his body set at his level, between the bank's
		// two poses his clip is between, placed by his frame; his helmet another, by its own. Nothing skinned, nothing of
		// his own sent but his place and his numbers. In the wide mass shot a far man is grown about his feet to stand
		// at least MassMinPx of the picture's lines (GS3's measure), so the army reads as men, not as nothing.
		if (Pose >= 0 && Vat.IsValid())
		{
			const int32 Level = Crowd.BankLevel(Man);
			FTransform Body = LandParts::BankedAt(Crowd.WorldFrame(Man), UnitsPerMetre);
			double Grow = 1.0;
			if (bMass && Level == static_cast<int32>(LandMen::Far) && MassMinPx > 0.0)
			{
				Grow = FMath::Max(1.0, MassMinPx * Crowd.SeenDistance(Man) / FMath::Max(1.0, S.MenLines));
			}
			else if (bHigh && Level == static_cast<int32>(LandMen::Far) && GeneralMinPx > 0.0)
			{
				Grow = FMath::Clamp(GeneralMinPx * Crowd.SeenDistance(Man) / FMath::Max(1.0, S.MenLines), 1.0, 2.5);
			}
			FTransform Helmet;
			LandMen::Xform Worn;
			const bool bHelmet = Crowd.HelmetOf(Man) >= 0 && Crowd.HelmetFrame(Man, Worn);
			if (bHelmet)
			{
				Helmet = LandParts::BankedAt(Worn, UnitsPerMetre);
			}
			if (Grow > 1.0)
			{
				const FVector Feet = Body.GetLocation();
				Body.SetScale3D(Body.GetScale3D() * Grow);
				if (bHelmet)
				{
					Helmet.SetLocation(Feet + (Helmet.GetLocation() - Feet) * Grow);
					Helmet.SetScale3D(Helmet.GetScale3D() * Grow);
				}
			}
			if (Vat->Place(Crowd, Man, Body, bHelmet ? &Helmet : nullptr))
			{
				BankOut(D);
				if (D.Mesh.IsValid())
				{
					D.Mesh->SetVisibility(false);
				}
				// (skinned afresh when he leaves the bank)
				D.Stamp = 0xFFFFFFFFu;
				if (Level == static_cast<int32>(LandMen::Mid))
				{
					++VatMid;
				}
				else
				{
					++VatFar;
				}
				if (Grow > 1.0)
				{
					++MassGrown;
					MostGrow = FMath::Max(MostGrow, Grow);
				}
				continue;
			}
		}
		// Banked far, the crowd not made (SO1 before its draw): an instance of his set's mesh in the bank's pose, put
		// where his frame is (again only when it moved or the pose changed); nothing skinned, no mesh of his own sent.
		if (Pose >= 0)
		{
			const int32 Copy = BankCopyFor(Crowd.BankKey(Man), Pose, Builds);
			if (Copy >= 0)
			{
				if (Vat.IsValid())
				{
					Vat->Out(Man);
				}
				const LandMen::Xform& W = Crowd.WorldFrame(Man);
				float Frame[12];
				FMemory::Memcpy(Frame, W.R, sizeof(W.R));
				FMemory::Memcpy(Frame + 9, W.T, sizeof(W.T));
				const bool bMoved = D.BankCopy != Copy || FMemory::Memcmp(Frame, D.BankFrame, sizeof(Frame)) != 0;
				if (bMoved)
				{
					const FTransform Where = LandParts::BankedAt(W, UnitsPerMetre);
					UInstancedStaticMeshComponent* Copies = BankCopies[Copy];
					if (D.BankCopy != Copy)
					{
						BankOut(D);
						TArray<int32>& Free = BankFree[Copy];
						if (Free.Num() > 0)
						{
							D.BankSlot = Free.Pop(EAllowShrinking::No);
							Copies->UpdateInstanceTransform(D.BankSlot, Where, false, false, true);
						}
						else
						{
							D.BankSlot = Copies->AddInstance(Where, false);
						}
						D.BankCopy = Copy;
						++BankLive[Copy];
					}
					else
					{
						Copies->UpdateInstanceTransform(D.BankSlot, Where, false, false, true);
					}
					BankDirty[Copy] = 1;
					FMemory::Memcpy(D.BankFrame, Frame, sizeof(Frame));
				}
				if (D.Mesh.IsValid())
				{
					D.Mesh->SetVisibility(false);
				}
				// (skinned afresh when he leaves the bank)
				D.Stamp = 0xFFFFFFFFu;
				continue;
			}
		}
		LandMen::Lod Level = Crowd.LodOf(Man);
		if (!D.Mesh.IsValid() || D.Lod != static_cast<int32>(Level))
		{
			if (Rebuilds <= 0)
			{
				// (over the frame's rebuilds: he keeps the level his mesh has, or waits to be drawn; a man just out of the
				// bank with no mesh yet keeps his bank copy where it last stood meanwhile, not drawn nowhere: the pre-check
				// on 76562a10)
				if (!D.Mesh.IsValid() || D.Lod < 0)
				{
					continue;
				}
				Level = static_cast<LandMen::Lod>(D.Lod);
				++RebuildsPut;
			}
			else
			{
				--Rebuilds;
				++MenRebuilt;
			}
		}
		BankOut(D);
		if (Vat.IsValid())
		{
			Vat->Out(Man);
		}
		// (banked but his set's mesh not made yet: his stamp holds while only his place moves, so he is skinned each frame)
		if (D.Mesh.IsValid() && D.Lod == static_cast<int32>(Level) && D.Stamp == Crowd.Stamp(Man) && Pose < 0)
		{
			D.Mesh->SetVisibility(true);
			continue;
		}
		FSkinJob& Job = Jobs.AddDefaulted_GetRef();
		Job.Man = Man;
		Job.Level = Level;
		Job.Points = Crowd.PointsOf(Man, Level);
		Job.Offset = Floats;
		Floats += Job.Points * 3;
	}
	LandFell::Phase("men: the bank sent", BankCopies.Num());
	// (the gate's names, nested to the function's end: DrawMen alone is the choice above, MenSend the sends, MenSkin the
	// skinning, MenMeshes the near men's meshes)
	LAND_SCOPE(MenSend);
	// (the banked instances moved this frame sent at the frame's end, only those; a mesh holding no one now hidden)
	for (int32 K = 0; K < BankCopies.Num(); ++K)
	{
		if (BankDirty[K] && BankCopies[K])
		{
			BankCopies[K]->SetVisibility(BankLive[K] > 0);
			BankCopies[K]->MarkRenderInstancesDirty();
		}
		BankDirty[K] = 0;
	}
	if (Vat.IsValid())
	{
		Vat->Send();
	}
	if (Jobs.Num() == 0)
	{
		return;
	}
	LandFell::Phase("men: skinned", Jobs.Num());
	LAND_SCOPE(MenSkin);
	SkinAt.SetNumUninitialized(Floats);
	SkinFacing.SetNumUninitialized(Floats);
	// (a body's first skin at a level lays him on the ground through the field: those here, one at a time; the rest
	// read only their own man and the crowd's fixed tables, so may go at once)
	TArray<int32> Spread;
	Spread.Reserve(Jobs.Num());
	for (int32 J = 0; J < Jobs.Num(); ++J)
	{
		const FSkinJob& Job = Jobs[J];
		if (Crowd.SkinReadsGround(Job.Man, Job.Level))
		{
			Crowd.Skin(Job.Man, Job.Level, SkinAt.GetData() + Job.Offset, SkinFacing.GetData() + Job.Offset);
		}
		else
		{
			Spread.Add(J);
		}
	}
	ParallelFor(Spread.Num(), [this, &Crowd, &Jobs, &Spread](int32 K)
	{
		const FSkinJob& Job = Jobs[Spread[K]];
		Crowd.Skin(Job.Man, Job.Level, SkinAt.GetData() + Job.Offset, SkinFacing.GetData() + Job.Offset);
	});
	LandFell::Phase("men: their meshes sent", Jobs.Num());
	LAND_SCOPE(MenMeshes);
	TArray<FVector> P, N;
	TArray<FColor> C;
	TArray<int32> T;
	// (a near man's points laid about his own place and his mesh moved there each frame, so its move is his motion
	// vector and the temporal anti-aliasing keeps him whole in a shot that follows him: with his mesh left at the field's
	// corner and only its points moved the engine saw him as standing still, threw his history away every frame and drew
	// him from the low resolution, his back and pack in blocks (the art director's stills, 36 s, 23 November). The mid and
	// far men, small in the picture, stay laid in the field's frame. -TheaterLandMenFieldFrame lays the near ones so too,
	// for the A/B.)
	static const bool bFieldFrame = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandMenFieldFrame"));
	for (const FSkinJob& Job : Jobs)
	{
		const int32 Man = Job.Man;
		const LandMen::Lod Level = Job.Level;
		const int32 Points = Job.Points;
		FManDrawn& D = MenDrawn[Man];
		const float* Skinned = SkinAt.GetData() + Job.Offset;
		const float* Faced = SkinFacing.GetData() + Job.Offset;
		FVector Origin = FVector::ZeroVector;
		if (Level == LandMen::Near && !bFieldFrame)
		{
			double Ox = 0.0, Oy = 0.0, Oz = 0.0;
			float Of = 0.0f;
			Crowd.PlaceAsPosed(Man, Ox, Oy, Oz, Of);
			Origin = FVector(Oy * UnitsPerMetre, Ox * UnitsPerMetre, Oz * UnitsPerMetre);
		}
		P.SetNumUninitialized(Points);
		N.SetNumUninitialized(Points);
		for (int32 I = 0; I < Points; ++I)
		{
			P[I] = FVector(Skinned[I * 3 + 1] * UnitsPerMetre, Skinned[I * 3] * UnitsPerMetre, Skinned[I * 3 + 2] * UnitsPerMetre) - Origin;
			N[I] = FVector(Faced[I * 3 + 1], Faced[I * 3], Faced[I * 3 + 2]);
		}
		if (!D.Mesh.IsValid())
		{
			UProceduralMeshComponent* Mesh = LandParts::NewMesh(this, Root, FName(*FString::Printf(TEXT("LandMan%d"), Man)), true);
			Mesh->SetMaterial(0, ManInk);
			MenMeshes.Add(Mesh);
			D.Mesh = Mesh;
		}
		if (D.Lod != static_cast<int32>(Level))
		{
			// A new level: its own triangles and colours (the same all battle, read once), then the skin. His shadow goes
			// with the level, switched only as his draw is made again anyway (GS3): near and mid men cast one, within some
			// 80 m as the picture sees him (the Mid level's end); a far man's would be a smudge of a few pixels, and every
			// moving one makes the shadow maps draw their pages under him again.
			const bool bShadow = Level != LandMen::Far;
			if (bShadow != D.bShadow)
			{
				D.Mesh->SetCastShadow(bShadow);
				D.bShadow = bShadow;
			}
			const LandMen::Look& Paint = Crowd.LookOf(Man, Level);
			T.SetNumUninitialized(static_cast<int32>(Paint.Triangles.size()));
			for (int32 I = 0; I < T.Num(); ++I)
			{
				T[I] = static_cast<int32>(Paint.Triangles[static_cast<size_t>(I)]);
			}
			C.SetNumUninitialized(Points);
			for (int32 I = 0; I < Points; ++I)
			{
				const size_t K = static_cast<size_t>(I) * 3;
				C[I] = K + 2 < Paint.Rgb.size() ? FColor(Paint.Rgb[K], Paint.Rgb[K + 1], Paint.Rgb[K + 2], 255) : FColor(90, 90, 80, 255);
			}
			D.Mesh->ClearAllMeshSections();
			D.Mesh->CreateMeshSection(0, P, T, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
			D.Lod = static_cast<int32>(Level);
		}
		else
		{
			D.Mesh->UpdateMeshSection(0, P, N, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>());
		}
		if (!D.Mesh->GetRelativeLocation().Equals(Origin, 0.01))
		{
			D.Mesh->SetRelativeLocation(Origin);
		}
		D.Stamp = Crowd.Stamp(Man);
		D.Mesh->SetVisibility(true);
	}
}

int32 ALandBattle::BankCopyFor(int32 Set, int32 Pose, int32& Builds)
{
	const LandMen::Crowd* Crowd = State.IsValid() ? State->Men.get() : nullptr;
	if (!Crowd || Set < 0 || Pose < 0 || !ManInk)
	{
		return -2;
	}
	const int32 Id = Set * Crowd->BankPoses() + Pose;
	if (const int32* Found = BankCopyOf.Find(Id))
	{
		return *Found;
	}
	if (Builds <= 0)
	{
		return -1;
	}
	--Builds;
	LandFell::Phase("men: a banked pose's mesh made", Id);
	const double Began = LandParts::Seconds();
	std::vector<float> At, Facing;
	const FString Name = FString::Printf(TEXT("LandBankMan_%d_%d"), Set, Pose);
	UStaticMesh* Mesh = Crowd->BankMesh(Set, Pose, At, Facing) ? LandParts::BankManMesh(this, FName(*(Name + TEXT("_Mesh"))), At, Facing, Crowd->BankLook(Set), ManInk, UnitsPerMetre) : nullptr;
	if (!Mesh)
	{
		BankCopyOf.Add(Id, -2);
		return -2;
	}
	UInstancedStaticMeshComponent* Copies = NewObject<UInstancedStaticMeshComponent>(this, FName(*Name));
	Copies->SetupAttachment(Root);
	Copies->SetStaticMesh(Mesh);
	Copies->SetMaterial(0, ManInk);
	Copies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Copies->SetCanEverAffectNavigation(false);
	// (far: no man's shadow past about 80 m as seen, GS3; the bank's men are 120 m and more)
	Copies->SetCastShadow(false);
	Copies->RegisterComponent();
	const int32 Index = BankCopies.Add(Copies);
	BankMeshes.Add(Mesh);
	BankFree.AddDefaulted();
	BankDirty.Add(0);
	BankLive.Add(0);
	BankCopyOf.Add(Id, Index);
	++BankMade;
	BankMadeMs += (LandParts::Seconds() - Began) * 1000.0;
	if (BankMade == 50 || BankMade == 200 || BankMade == 500)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: %d banked men's meshes made, %.2f ms each"), BankMade, BankMadeMs / BankMade);
	}
	return Index;
}

void ALandBattle::BankOut(FManDrawn& D)
{
	if (D.BankCopy >= 0 && D.BankSlot >= 0 && BankCopies.IsValidIndex(D.BankCopy) && BankCopies[D.BankCopy])
	{
		const FVector Stood(D.BankFrame[10] * UnitsPerMetre, D.BankFrame[9] * UnitsPerMetre, D.BankFrame[11] * UnitsPerMetre);
		BankCopies[D.BankCopy]->UpdateInstanceTransform(D.BankSlot, FTransform(FQuat::Identity, Stood, FVector(1e-3)), false, false, true);
		BankFree[D.BankCopy].Add(D.BankSlot);
		BankDirty[D.BankCopy] = 1;
		BankLive[D.BankCopy] = FMath::Max(0, BankLive[D.BankCopy] - 1);
	}
	D.BankCopy = -1;
	D.BankSlot = -1;
}

// ---- The tanks, guns and lorries -------------------------------------------------------------------------------

void ALandBattle::MakeArmour()
{
	FLandBattleState& S = *State;
	const LandField::Ground* FieldPtr = S.Field.get();
	if (!FieldPtr)
	{
		return;
	}
	{
		FString FatesText;
		std::string Error;
		if (!FFileHelper::LoadFileToString(FatesText, *(FPaths::ProjectContentDir() / TEXT("Data/Battle/fates.json"))) || !S.FatesTable.Load(LandParts::Narrow(FatesText), Error))
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: Content/Data/Battle/fates.json not read (%s): one kill in four brews up."), *LandParts::Wide(Error));
		}
	}
	const FString Folder = FPaths::ProjectContentDir() / TEXT("Models/Battle");
	const LandKit::Battle::ReadFn Read = [Folder](const std::string& FileName, std::string& OutText)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(Folder / LandParts::Wide(FileName))))
		{
			return false;
		}
		OutText = LandParts::Narrow(Text);
		return true;
	};
	// (in a hedge cutter's gap, once torn, she drives on its torn floor: the bank she tore is gone from under her; its
	// slope from the floor half a metre either way, level where that is past the gap's cut. Read only once a bank is
	// torn: before that, and with no cuts, the field's ground alone, as ever)
	const LandCrater::Field* BreachesPtr = &S.Holes;
	const int32* TornPtr = &CutsTorn;
	const LandKit::GroundFn GroundAt = [FieldPtr, BreachesPtr, TornPtr](double X, double Y, double& OutZ, LandKit::V3& OutNormal)
	{
		const LandField::Surface Under = FieldPtr->At(X, Y);
		OutZ = Under.Z;
		OutNormal = LandKit::V3(Under.NX, Under.NY, Under.NZ);
		double Floor = 0.0;
		if (*TornPtr > 0 && BreachesPtr->BreachFloor(X, Y, Floor))
		{
			const auto FloorOr = [BreachesPtr, Floor](double PX, double PY)
			{
				double Z = Floor;
				return BreachesPtr->BreachFloor(PX, PY, Z) ? Z : Floor;
			};
			constexpr double Step = 0.5;
			const double Dx = (FloorOr(X + Step, Y) - FloorOr(X - Step, Y)) / (2.0 * Step);
			const double Dy = (FloorOr(X, Y + Step) - FloorOr(X, Y - Step)) / (2.0 * Step);
			const double Long = std::sqrt(Dx * Dx + Dy * Dy + 1.0);
			OutZ = Floor;
			OutNormal = LandKit::V3(-Dx / Long, -Dy / Long, 1.0 / Long);
		}
	};
	auto Armour = std::make_unique<LandKit::Battle>();
	Armour->Life = S.Life;
	// Men in lorries (the Battle rules card's row, a live battle's: the rules seat the men): the lorries' tilts stowed,
	// so the men on their benches are seen (RideLorries)
	S.bTrucks = S.Rec.bLive && FLandLive::RuleNow(TEXT("trucks"));
	Armour->bOpenLorries = S.bTrucks;
	if (S.bTrucks)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: men in lorries ON: seated on the benches, the tilts stowed, getting down one at a time"));
	}
	// Captured kit (release 2, on trial: "-TheaterLandTrial=captured"): a block the recording marks captured (the
	// campaign's, blocks[].captured) in her captor's coat over her maker's; "-TheaterLandNoCaptured" off whatever the
	// trial (the player's switch on the Battle rules card); "-TheaterLandCapturedTest" every other attacker tank block drawn
	// as the defender's tank, taken from them (the Mac's off/on line).
	Armour->bCapturedLook = LandParts::OnTrial("captured") && !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoCaptured"));
	Armour->bCapturedTest = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandCapturedTest"));
	if (Armour->bCapturedLook || Armour->bCapturedTest)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: captured kit ON%s"), Armour->bCapturedTest ? TEXT(" (test: every other attacker tank block taken from the defender)") : TEXT(""));
	}
	std::string Error;
	{
		// How a pierced tank ends by the face struck (the modeller's measured odds: a rear hit on a Tiger or a Panther
		// blows her up more often, Brian's ask of 22 November).
		FString PerformanceText;
		std::string Why;
		if (!FFileHelper::LoadFileToString(PerformanceText, *(FPaths::ProjectContentDir() / TEXT("Data/Battle/performance-land.json")))
			|| !LandKit::ReadFireOdds(LandParts::Narrow(PerformanceText), Armour->Fires, Why))
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: Content/Data/Battle/performance-land.json not read (%s): kills look by the fates table alone."), *LandParts::Wide(Why));
		}
	}
	const bool bFates = !S.FatesTable.Ways().empty();
	if (!Armour->Load(S.RecText, Read, GroundAt, bFates ? &S.FatesTable : nullptr, S.Rec.Seed ? S.Rec.Seed : 1, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: the tanks could not be made (%s): none are drawn."), *LandParts::Wide(Error));
		return;
	}
	// (a vehicle drawn as the nearest generic for want of her own model: one line each, the stand-in named)
	for (const std::string& Line : Armour->StandIns())
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: stand-in: %s"), *LandParts::Wide(Line));
	}
	S.Armour = std::move(Armour);
	S.VehicleOf.assign(S.Rec.Tracks.size(), -1);
	S.TrackOfVehicle.assign(S.Armour->Vehicles().size(), -1);
	for (size_t V = 0; V < S.Armour->Vehicles().size(); ++V)
	{
		const LandKit::TrackWords& W = S.Armour->Vehicles()[V].Words();
		const int Track = S.Rec.TrackIndexOf(W.Block, W.K, W.J);
		S.TrackOfVehicle[V] = Track;
		if (Track >= 0)
		{
			S.VehicleOf[static_cast<size_t>(Track)] = static_cast<int>(V);
		}
	}
	MakeBodies(0);
}

void ALandBattle::MakeRiders()
{
	FLandBattleState& S = *State;
	using FRider = FLandBattleState::FRider;
	S.Riders.clear();
	S.RoofManned.assign(S.Armour ? S.Armour->Vehicles().size() : 0, 1);
	if (!S.Men || !S.Armour || FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoCommanders")))
	{
		return;
	}
	static const char* const ClipNames[FRider::Kinds] = { "commander_hatch", "commander_roof_mg", "commander_roof_mg_burst", "die_hatch_slump", "die_hatch_drop" };
	const std::vector<LandKit::Vehicle>& All = S.Armour->Vehicles();
	// (each vehicle's rider by her track's words, for her rounds below)
	std::map<std::tuple<int, int, int>, size_t> ByWords;
	for (size_t V = 0; V < All.size(); ++V)
	{
		const LandKit::Vehicle& Veh = All[V];
		const LandKit::Model& Look = Veh.Look();
		if (Look.Kind != "tank" || Look.SocketIndex("commander") < 0 || V >= S.TrackOfVehicle.size() || S.TrackOfVehicle[V] < 0)
		{
			continue;
		}
		const std::string File = Veh.Nation == "USA" ? "USA_tanker" : Veh.Nation == "GER" ? "GER_panzer" : std::string();
		const LandMen::Soldier* Crew = File.empty() ? nullptr : S.Men->SoldierFile(File);
		const LandKit::TrackWords& W = Veh.Words();
		const LandFeed::Block* B = S.Rec.BlockOf(W.Block);
		if (!Crew || !B)
		{
			continue;
		}
		// (his clips' places name her by her model's file: Sherman, PanzerIV, Tiger; a tank they do not name has no
		// commander drawn yet)
		FRider R;
		R.Vehicle = static_cast<int>(V);
		bool bAll = true;
		for (int K = 0; K < FRider::Kinds && bAll; ++K)
		{
			R.Clips[K] = Crew->ClipIndex(ClipNames[K]);
			const LandMen::Clip* C = R.Clips[K] >= 0 ? &Crew->Clips[static_cast<size_t>(R.Clips[K])] : nullptr;
			const LandMen::Clip::Seat* Seat = C ? C->SeatOn(B->Model) : nullptr;
			bAll = Seat && Seat->Socket == "commander";
			if (!bAll)
			{
				break;
			}
			R.Offsets[K] = Seat->Offset;
			R.Long[K] = C->Long;
			for (const LandMen::ClipEvent& E : C->Events)
			{
				if (K == FRider::Drop && E.Fx == "hide")
				{
					R.DropHide = E.T;
				}
			}
		}
		if (!bAll)
		{
			continue;
		}
		const uint64_t Seed = (S.Rec.Seed ? S.Rec.Seed : 1) * 0x9E3779B97F4A7C15ull ^ (static_cast<uint64_t>(V) + 1) * 0xC2B2AE3D27D4EB4Full;
		R.Phase = static_cast<float>((Seed >> 20) % 1000) * 0.01f;
		// His end up in the hatch: her kill while he was up (by the rules' own hatch: not buttoned then). Thrown with
		// her turret when she blows up; else he slumps over the rim or drops inside, by her own draw.
		const LandFeed::Track& Tr = S.Rec.Tracks[static_cast<size_t>(S.TrackOfVehicle[V])];
		if (Veh.Died >= 0.0)
		{
			const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Veh.Died - 0.05);
			if (C && !C->bButtoned)
			{
				R.Killed = Veh.Died;
				R.Dies = Veh.bBrewUp ? static_cast<int>(FRider::Kinds) : ((Seed >> 41) & 1) ? static_cast<int>(FRider::Slump) : static_cast<int>(FRider::Drop);
			}
		}
		R.Man = S.Men->AddRider(File, Veh.Nation, Seed);
		if (R.Man < 0)
		{
			continue;
		}
		ByWords[std::make_tuple(W.Block, W.K, W.J)] = S.Riders.size();
		S.Riders.push_back(R);
	}
	// Her roof gun's bursts (the replay's own pick of her gun for each burst, TankMg), and a round that struck him up
	// in his hatch (the rules' cm: he drops inside; she fights on buttoned up).
	int Bursts = 0, Struck = 0;
	for (const LandFeed::Round& Rd : S.Rec.Rounds)
	{
		if (Rd.W == "mg")
		{
			const auto Found = ByWords.find(std::make_tuple(Rd.By, Rd.K, Rd.J));
			if (Found != ByWords.end())
			{
				FRider& R = S.Riders[Found->second];
				const int Track = S.TrackOfVehicle[static_cast<size_t>(R.Vehicle)];
				if (S.Play.TankMg(Track, Rd.T, Rd.N, LandPlay::Vec(Rd.X, Rd.Y, 0.0), Rd.G) == "roof_mg" && (R.Killed < 0.0 || Rd.T < R.Killed))
				{
					R.Bursts.push_back(Rd.T);
					++Bursts;
				}
			}
		}
		if (Rd.bCm)
		{
			const auto Found = ByWords.find(std::make_tuple(Rd.On, Rd.Ok, Rd.Oj));
			if (Found != ByWords.end())
			{
				FRider& R = S.Riders[Found->second];
				const double At = Rd.T + FMath::Max(0.0, Rd.L);
				if (R.Killed < 0.0 || At < R.Killed)
				{
					R.Killed = At;
					R.Dies = FRider::Drop;
					++Struck;
				}
			}
		}
	}
	for (FRider& R : S.Riders)
	{
		std::sort(R.Bursts.begin(), R.Bursts.end());
		while (!R.Bursts.empty() && R.Killed >= 0.0 && R.Bursts.back() >= R.Killed)
		{
			R.Bursts.pop_back();
		}
	}
	MenDrawn.SetNum(S.Men->Count());
	int Killed = 0;
	for (const FRider& R : S.Riders)
	{
		Killed += R.Killed >= 0.0;
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle: %d tank commanders up in their hatches (soldier_USA_tanker, soldier_GER_panzer): %d roof-gun bursts, %d killed up there (%d by a round at him)"),
		static_cast<int32>(S.Riders.size()), Bursts, Killed, Struck);
}

void ALandBattle::RideTanks()
{
	LAND_SCOPE(RideTanks);
	FLandBattleState& S = *State;
	using FRider = FLandBattleState::FRider;
	if (!S.Men || !S.Armour || S.Riders.empty())
	{
		return;
	}
	const std::vector<LandKit::Vehicle>& All = S.Armour->Vehicles();
	if (S.RoofManned.size() < All.size())
	{
		S.RoofManned.resize(All.size(), 1);
	}
	const double Now = S.Battle;
	const double Life = S.Life;
	for (FRider& R : S.Riders)
	{
		if (R.Man < 0 || !S.Men->IsRider(R.Man) || R.Vehicle < 0 || static_cast<size_t>(R.Vehicle) >= All.size())
		{
			continue;
		}
		const LandKit::Vehicle& V = All[static_cast<size_t>(R.Vehicle)];
		S.RoofManned[static_cast<size_t>(R.Vehicle)] = 0;
		int Want = -1;
		float Time = 0.f;
		double Rise = 0.0;
		float Blend = 0.35f;
		if (R.Killed >= 0.0 && Now >= R.Killed)
		{
			// Killed up there: his end played from the moment, where he stood (not seen if he was down inside, or
			// thrown with her turret; gone from sight once a drop has taken him under the rim).
			const float Since = static_cast<float>((Now - R.Killed) * Life);
			Rise = R.Rise;
			if (R.Dies != FRider::Kinds && Rise > 0.03 && !(R.Dies == FRider::Drop && Since >= R.DropHide))
			{
				Want = R.Dies;
				Time = Since;
				Blend = 0.12f;
			}
		}
		else
		{
			// Up as her lid swings open, from 40% open, and down again as it shuts.
			const double Open = FMath::Clamp((V.HatchOpen - 0.4) / 0.55, 0.0, 1.0);
			Rise = Open * Open * (3.0 - 2.0 * Open);
			const double BurstSpan = R.Long[FRider::Burst] / FMath::Max(0.25, Life);
			const auto Next = std::lower_bound(R.Bursts.begin(), R.Bursts.end(), Now + 0.02);
			const double Last = Next != R.Bursts.begin() ? *(Next - 1) : -1e9;
			const double Coming = Next != R.Bursts.end() ? *Next : 1e9;
			// (her roof gun firing, or about to, while the lid still swings open: he is up at it at once, so no burst
			// comes from a gun nobody mans, or from a German cradle not drawn; the pre-check's note on 174524b6)
			if (V.HatchOpen > 0.15 && (Now - Last < BurstSpan || Coming - Now < 0.5))
			{
				Rise = 1.0;
			}
			R.Rise = static_cast<float>(Rise);
			if (Rise > 0.03)
			{
				// At the roof gun from some seconds before her bursts to some after, firing each burst; else looking
				// about with his glasses.
				if (Now - Last < BurstSpan)
				{
					Want = FRider::Burst;
					Time = static_cast<float>((Now - Last) * Life);
					Blend = 0.1f;
				}
				else if (Now - Last < 5.0 || Coming - Now < 3.0)
				{
					Want = FRider::Roof;
					Time = static_cast<float>(Now * Life) + R.Phase;
				}
				else
				{
					Want = FRider::Hatch;
					Time = static_cast<float>(Now * Life) + R.Phase;
				}
			}
		}
		LandMen::Crowd::Ride Ride;
		LandKit::V3 At, Facing;
		if (Want < 0 || R.Clips[Want] < 0 || V.Fade < 0.5 || HiddenVehicle(R.Vehicle) || !V.SocketNow("commander", At, Facing))
		{
			// (out of sight: what he played is forgotten, so he comes back up as he is, unblended)
			R.Now = R.Was = -1;
			S.Men->SetRide(R.Man, Ride);
			continue;
		}
		if (Want != R.Now)
		{
			R.Was = R.Now;
			R.WasTime = R.NowTime;
			R.SwitchedAt = Now;
			R.Now = Want;
		}
		R.NowTime = Time;
		S.RoofManned[static_cast<size_t>(R.Vehicle)] = Want == FRider::Roof || Want == FRider::Burst;
		// (the clip left, blended out: a loop runs on, a one-shot holds its end)
		const double Since = Now - R.SwitchedAt;
		const float W = R.Was < 0 || Since < 0.0 ? 1.f : static_cast<float>(FMath::Clamp(Since * Life / Blend, 0.0, 1.0));
		LandMen::Vec3 Off = R.Offsets[Want];
		if (W < 1.f)
		{
			const LandMen::Vec3& From = R.Offsets[R.Was];
			Off = LandMen::Vec3{ From.X + (Off.X - From.X) * W, From.Y + (Off.Y - From.Y) * W, From.Z + (Off.Z - From.Z) * W };
			Ride.FromClip = R.Clips[R.Was];
			Ride.FromTime = R.WasTime + static_cast<float>(Since * Life);
			Ride.W = W;
		}
		// His seat: her commander socket, its facing (her turret's) along the turret roof, her hull's up; his root the
		// socket moved by the clip's offset, and lower by what he has not yet risen.
		const LandKit::V3 Up = V.Root.Way(LandKit::V3(0.0, 0.0, 1.0)).Unit();
		const LandKit::V3 Forward = (Facing - Up * Facing.Dot(Up)).Unit(LandKit::V3(0.0, 1.0, 0.0));
		const LandKit::V3 Right = Forward.Cross(Up);
		const LandKit::V3 Feet = At + Forward * Off.X + Right * Off.Y + Up * (Off.Z - (1.0 - Rise) * 1.1);
		float* M = Ride.Seat.R;
		M[0] = static_cast<float>(Forward.X); M[1] = static_cast<float>(Right.X); M[2] = static_cast<float>(Up.X);
		M[3] = static_cast<float>(Forward.Y); M[4] = static_cast<float>(Right.Y); M[5] = static_cast<float>(Up.Y);
		M[6] = static_cast<float>(Forward.Z); M[7] = static_cast<float>(Right.Z); M[8] = static_cast<float>(Up.Z);
		Ride.Seat.T[0] = static_cast<float>(Feet.X);
		Ride.Seat.T[1] = static_cast<float>(Feet.Y);
		Ride.Seat.T[2] = static_cast<float>(Feet.Z);
		Ride.bShown = true;
		Ride.Clip = R.Clips[Want];
		Ride.Time = Time;
		S.Men->SetRide(R.Man, Ride);
	}
}

void ALandBattle::RideLorries()
{
	LAND_SCOPE(RideLorries);
	FLandBattleState& S = *State;
	if (!S.bTrucks || !S.Men || !S.Armour)
	{
		return;
	}
	std::vector<LandKit::Vehicle>& All = S.Armour->Vehicles();
	// (her by her block and her track's k; a lorry with benches, a way down and the men's clips' places on her)
	std::map<std::pair<int, int>, int> LorryOf;
	for (size_t V = 0; V < All.size(); ++V)
	{
		const LandKit::Model& Look = All[V].Look();
		if (All[V].Words().Role == "lorry" && Look.SocketIndex("rider.1") >= 0 && Look.SocketIndex("step") >= 0)
		{
			LorryOf[{ All[V].Words().Block, All[V].Words().K }] = static_cast<int>(V);
		}
	}
	const int Men = S.Men->Count();
	if (S.LorryRider.size() < static_cast<size_t>(Men))
	{
		S.LorryRider.resize(static_cast<size_t>(Men), -1);
	}
	const double Now = S.Battle;
	const double Life = S.Life;
	bool bAdded = false;
	auto Hide = [&S](int Rider)
	{
		if (Rider >= 0)
		{
			S.Men->SetRide(Rider, LandMen::Crowd::Ride());
		}
	};
	for (int Man = 0; Man < Men; ++Man)
	{
		if (S.Men->IsRider(Man))
		{
			continue;
		}
		int& Rider = S.LorryRider[static_cast<size_t>(Man)];
		int Block = -1, SeatOn = -1, Seat = -1;
		double Down = -1.0;
		const auto Her = S.Men->AboardOf(Man, Block, SeatOn, Seat, Down) ? LorryOf.find({ Block, SeatOn }) : LorryOf.end();
		const int Socket = LandBench::SocketOf(Seat);
		if (Her == LorryOf.end() || Socket <= 0)
		{
			Hide(Rider);
			continue;
		}
		LandKit::Vehicle& V = All[static_cast<size_t>(Her->second)];
		// (her way down thrown open as her first man starts down)
		if (Down >= 0.0)
		{
			V.OpenBoard(Now - Down);
		}
		const double Near = FVector::Dist2D(FVector(S.Pose.Eye.X, S.Pose.Eye.Y, 0.0), FVector(V.Root.At.X, V.Root.At.Y, 0.0));
		if (V.Fade < 0.5 || HiddenVehicle(Her->second) || Near > 160.0)
		{
			Hide(Rider);
			continue;
		}
		if (Rider < 0)
		{
			Rider = S.Men->AddRiderLike(Man);
			bAdded = bAdded || Rider >= 0;
			if (Rider < 0)
			{
				continue;
			}
		}
		const LandMen::Soldier* Sol = S.Men->SoldierOf(Rider);
		LandMen::Crowd::Ride Ride;
		if (!Sol || !LandBench::RideFor(V, *Sol, Seat, Down, Now, Life, Man, Ride))
		{
			Hide(Rider);
			continue;
		}
		S.Men->SetRide(Rider, Ride);
	}
	if (bAdded)
	{
		MenDrawn.SetNum(S.Men->Count());
	}
}

void ALandBattle::MakeBodies(int32 FromVehicle)
{
	FLandBattleState& S = *State;
	if (!S.Armour)
	{
		return;
	}
	// One mesh a body: its near level as section 0 and its far as section 1, posed each frame. (A live battle makes a
	// vehicle's as she first comes, and all again in a new round of names if the setup step changed her vehicles.)
	TArray<FVector> P, N;
	TArray<FColor> C;
	TArray<int32> T;
	// TK1: past the hand-over a vehicle is drawn as instances of her model's merged far pieces (Battle/LandCrowd): each
	// is enlisted there, and a body it merges makes no far section of its own
	if (!FarCrowd.IsValid())
	{
		FarCrowd = MakeShared<FLandCrowd>(this, Root.Get(), SteelInk.Get(), UnitsPerMetre, [this](double X, double Y, double Z) { return World(X, Y, Z); }, CrowdKept); // TK1
	}
	for (size_t V = static_cast<size_t>(FMath::Max(0, FromVehicle)); V < S.Armour->Vehicles().size(); ++V)
	{
		const LandKit::Vehicle& Vehicle = S.Armour->Vehicles()[V];
		const LandKit::Model& Model = Vehicle.Look();
		const bool bEnlisted = FarCrowd->Enlist(static_cast<int32>(V), Model); // TK1
		for (size_t B = 0; B < Model.Bodies.size(); ++B)
		{
			const LandKit::Body& Body = Model.Bodies[B];
			if (Body.Near.Triangles.empty() && Body.Far.Triangles.empty())
			{
				continue;
			}
			const bool bFarSection = !Body.Far.Triangles.empty() && !(bEnlisted && FarCrowd->TakesFar(static_cast<int32>(V), static_cast<int32>(B))); // TK1
			const FString Name = BodyRound == 0 ? FString::Printf(TEXT("LandBody%d_%d"), static_cast<int32>(V), static_cast<int32>(B))
				: FString::Printf(TEXT("LandBody%d_%d_%d"), static_cast<int32>(V), static_cast<int32>(B), BodyRound);
			UProceduralMeshComponent* Mesh = LandParts::NewMesh(this, Root, FName(*Name), true);
			if (!Body.Near.Triangles.empty())
			{
				LandParts::FromBody(Body.Near, Model.BurntColour, 0.0, UnitsPerMetre, P, N, C, &T);
				Mesh->CreateMeshSection(0, P, T, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
				Mesh->SetMaterial(0, SteelInk);
			}
			if (bFarSection) // TK1
			{
				LandParts::FromBody(Body.Far, Model.BurntColour, 0.0, UnitsPerMetre, P, N, C, &T);
				Mesh->CreateMeshSection(1, P, T, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
				Mesh->SetMaterial(1, SteelInk);
				Mesh->SetMeshSectionVisible(1, Body.Near.Triangles.empty());
			}
			BodyMeshes.Add(Mesh);
			FBodyDrawn& D = BodiesDrawn.AddDefaulted_GetRef();
			D.Mesh = Mesh;
			D.Vehicle = static_cast<int32>(V);
			D.Body = static_cast<int32>(B);
			D.bTrack = Body.bTrack;
			D.bNear = !Body.Near.Triangles.empty();
			// (a German roof gun, the MG 34 in its cradle on the cupola's rail, is drawn only while her commander mans
			// it: the crews' hatch and death clips are made without it; the Sherman's .50 stays on its post)
			D.bWhileManned = LandKit::ShownWhileManned(Model, Body);
			D.bFarSection = bFarSection; // TK1
			D.LinksRun = Body.Side < 0 ? Vehicle.RunLeft : Vehicle.RunRight; // TK3: links as made, posed once she runs
		}
	}
}

void ALandBattle::DrawArmour(const FVector& EyeWorld)
{
	LAND_SCOPE(DrawArmour);
	FLandBattleState& S = *State;
	if (!S.Armour)
	{
		return;
	}
	const LandPlay::Vec EyeAt(EyeWorld.Y / UnitsPerMetre, EyeWorld.X / UnitsPerMetre, EyeWorld.Z / UnitsPerMetre);
	std::vector<LandKit::Vehicle>& All = S.Armour->Vehicles();
	TArray<FVector> P, N;
	TArray<FColor> C;
	// TK1: the far vehicles put as their pieces' instances (and who is in the picture, for TK3), before their bodies
	if (FarCrowd.IsValid())
	{
		// (the general's own view, high over the field: a far vehicle grown to stand at least 14 by 7 of the men's lines,
		// her barrel drawn at least a line wide, her far ink's sky shine taken down; the art director, 22 November 18:27Z,
		// "tanks read as grey-blue chips". FLandCrowd's "-TheaterLandGeneralTankPx=", "-TheaterLandGeneralBarrelPx=" and
		// "-TheaterLandGeneralTankInk=")
		FarCrowd->SetHigh(S.General.bOn && !S.General.bFilm, S.MenLines / 1.8);
		FarCrowd->Draw(All, S.Pose, EyeAt, [this](int32 K) { return HiddenVehicle(K); }); // TK1
	}
	// (the wrecks' soot steps spread over frames: at most this many points sent a frame, a body shown again sent at once)
	int32 SootRoom = 40000; // TK1 (soot)
	for (FBodyDrawn& D : BodiesDrawn)
	{
		UProceduralMeshComponent* Mesh = D.Mesh.Get();
		if (!Mesh)
		{
			continue;
		}
		const LandKit::Vehicle& V = All[static_cast<size_t>(D.Vehicle)];
		const LandKit::Model& Model = V.Look();
		const LandKit::Body& Body = Model.Bodies[static_cast<size_t>(D.Body)];
		const bool bManned = !D.bWhileManned || static_cast<size_t>(D.Vehicle) >= S.RoofManned.size() || S.RoofManned[static_cast<size_t>(D.Vehicle)] != 0;
		const bool bShown = V.Fade >= 0.5 && !HiddenVehicle(D.Vehicle) && bManned;
		Mesh->SetVisibility(bShown);
		if (!bShown)
		{
			continue;
		}
		// TK1: far and drawn by her instances: this body's sections hidden (no rebuild) and the body not moved; shown
		// again at its level the frame she comes back
		const bool bHeld = FarCrowd.IsValid() && FarCrowd->Holds(D.Vehicle, D.Body); // TK1
		const bool bBack = D.bHeld && !bHeld;
		if (bHeld != D.bHeld)
		{
			D.bHeld = bHeld;
			if (!Body.Near.Triangles.empty())
			{
				Mesh->SetMeshSectionVisible(0, !bHeld && D.bNear);
			}
			if (D.bFarSection)
			{
				Mesh->SetMeshSectionVisible(1, !bHeld && !D.bNear);
			}
		}
		if (bHeld)
		{
			continue;
		}
		const LandKit::Place& Place = V.Bodies()[static_cast<size_t>(D.Body)];
		// (in the general's view a far vehicle her own meshes draw is grown as her pieces would grow her, about the ground
		// under her middle: a gun laid high, a manned roof gun's cradle)
		const double Grow = FarCrowd.IsValid() ? FarCrowd->GrowOf(D.Vehicle) : 1.0;
		if (Grow != 1.0 || !Mesh->GetRelativeScale3D().Equals(FVector::OneVector))
		{
			const FVector Foot = World(V.Root.At.X, V.Root.At.Y, V.Root.At.Z);
			Mesh->SetWorldTransform(FTransform(FQuat(Place.Q.X, Place.Q.Y, Place.Q.Z, Place.Q.W), Foot + (World(Place.At.X, Place.At.Y, Place.At.Z) - Foot) * Grow, FVector(Grow)));
		}
		else
		{
			Mesh->SetWorldLocationAndRotation(World(Place.At.X, Place.At.Y, Place.At.Z), FQuat(Place.Q.X, Place.Q.Y, Place.Q.Z, Place.Q.W));
		}
		// The near level within 120 m (a long lens's Watch shot at 140 m keeps it), the far beyond (TK1: where it has a far
		// section of its own; a body merged into her far pieces keeps its near level while her own meshes draw her).
		const double Far = (LandPlay::Vec(V.Root.At.X, V.Root.At.Y, V.Root.At.Z) - EyeAt).Length();
		const bool bNear = (Far < 140.0 && !Body.Near.Triangles.empty()) || !D.bFarSection; // TK1
		if (bNear != D.bNear && !Body.Near.Triangles.empty() && D.bFarSection) // TK1
		{
			D.bNear = bNear;
			Mesh->SetMeshSectionVisible(0, bNear);
			Mesh->SetMeshSectionVisible(1, !bNear);
		}
		// A track's links, posed for her run while she is near, and (TK3) only while she is in the picture: out of it
		// she keeps the links she had, and is posed at once as she comes back into it, moving or not.
		const double LinksRun = Body.Side < 0 ? V.RunLeft : V.RunRight; // TK3
		if (D.bTrack && D.bNear && !Body.Near.Triangles.empty() && LinksRun != D.LinksRun // TK3
			&& (!FarCrowd.IsValid() || FarCrowd->LinksDue(D.Vehicle, Mesh->WasRecentlyRendered(0.1f)))) // TK3
		{
			D.LinksRun = LinksRun; // TK3
			const int32 Points = static_cast<int32>(Body.Near.Count());
			LinkPoints.SetNumUninitialized(Points * 3);
			V.PoseLinks(Body.Side, LinkPoints.GetData());
			P.SetNumUninitialized(Points);
			for (int32 I = 0; I < Points; ++I)
			{
				P[I] = FVector(LinkPoints[I * 3], LinkPoints[I * 3 + 1], LinkPoints[I * 3 + 2]) * UnitsPerMetre;
			}
			Mesh->UpdateMeshSection(0, P, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>());
		}
		// The burning blackens her, a step at a time (spread over frames by SootRoom: a whole Sherman's step is some 89,000
		// points; a body coming back from her instances is brought up to date at once).
		if ((V.Soot - D.SootShown > 0.05 || (V.Soot >= 1.0 && D.SootShown < 1.0)) && (SootRoom > 0 || bBack)) // TK1 (soot)
		{
			D.SootShown = V.Soot;
			for (int32 Section = 0; Section < 2; ++Section)
			{
				const LandKit::Mesh& M = Section == 0 ? Body.Near : Body.Far;
				if (M.Triangles.empty() || (Section == 0 && D.bTrack && V.bMoving) || (Section == 1 && !D.bFarSection)) // TK1
				{
					continue;
				}
				LandParts::FromBody(M, Model.BurntColour, V.Soot, UnitsPerMetre, P, N, C, nullptr);
				Mesh->UpdateMeshSection(Section, P, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>());
				SootRoom -= P.Num(); // TK1 (soot)
				if (Section == 0 && D.bTrack)
				{
					D.LinksRun = -1e300; // TK3: the links were sent at rest with the colours: posed again when next in the picture
				}
			}
		}
	}
	DrawShades();
	// Dust behind each vehicle on the move.
	if (Effects)
	{
		for (size_t K = 0; K < All.size(); ++K)
		{
			const LandKit::Vehicle& V = All[K];
			if (!V.bMoving || V.bDead || V.Fade < 0.5)
			{
				continue;
			}
			// (the effects kit runs on life's clock: a battle second's metres are Life life seconds')
			const FVector Velocity = WorldWay(V.Velocity.X, V.Velocity.Y, V.Velocity.Z) * (UnitsPerMetre / S.Life);
			const bool bTracked = V.Look().Kind == "tank";
			const FString Ground = GroundWordAt(V.Root.At.X, V.Root.At.Y);
			Effects->VehicleDust(static_cast<int32>(K) * 2, World(V.DustLeft.X, V.DustLeft.Y, V.DustLeft.Z), Velocity, Ground, bTracked);
			Effects->VehicleDust(static_cast<int32>(K) * 2 + 1, World(V.DustRight.X, V.DustRight.Y, V.DustRight.Z), Velocity, Ground, bTracked);
		}
	}
}

int32 ALandBattle::ShadeCopyFor(const LandKit::Model& Model)
{
	const FString ModelKey = LandParts::Wide(Model.Name);
	if (const int32* Found = ShadeCopyByName.Find(ModelKey))
	{
		return *Found;
	}
	// Her outline on the ground: a tank's or a lorry's hull with its running gear (not her turret or her gun beyond
	// her); a gun's from her wheels back to her spades (not her barrel, high over the ground); her length and width
	// about her middle when the model gives neither.
	double LoX = -0.5 * Model.Long, LoY = -0.5 * Model.Wide, HiX = 0.5 * Model.Long, HiY = 0.5 * Model.Wide;
	if (Model.Kind == "gun")
	{
		LoX = LoY = 1e9;
		HiX = HiY = -1e9;
		for (const LandKit::V3& Corner : Model.Foot)
		{
			LoX = FMath::Min(LoX, Corner.X - 0.3);
			LoY = FMath::Min(LoY, Corner.Y - 0.3);
			HiX = FMath::Max(HiX, Corner.X + 0.3);
			HiY = FMath::Max(HiY, Corner.Y + 0.3);
		}
	}
	else if (Model.HullLo.X < Model.HullHi.X && Model.HullLo.Y < Model.HullHi.Y)
	{
		LoX = Model.HullLo.X;
		LoY = Model.HullLo.Y;
		HiX = Model.HullHi.X;
		HiY = Model.HullHi.Y;
	}
	int32 Index = -1;
	if (ShadeInk && HiX - LoX > 0.2 && HiY - LoY > 0.2)
	{
		LandShade::Shape Shape = Model.Kind == "gun" ? LandShade::Gun(LoX, LoY, HiX, HiY)
			: Model.Kind == "carrier" ? LandShade::Lorry(LoX, LoY, HiX, HiY) : LandShade::Tank(LoX, LoY, HiX, HiY);
		Shape.Seed = GetTypeHash(ModelKey);
		LandShade::Mesh Laid;
		LandShade::Make(Shape, Laid);
		// (by number: a model's name may hold what an object's name may not, a bracket or a full stop)
		const FString Name = FString::Printf(TEXT("LandShade%d"), ShadeCopies.Num());
		if (UStaticMesh* Mesh = LandParts::ShadeMesh(this, FName(*(Name + TEXT("_Mesh"))), Laid, ShadeInk, UnitsPerMetre))
		{
			ShadeMeshes.Add(Mesh);
			UInstancedStaticMeshComponent* Copies = NewObject<UInstancedStaticMeshComponent>(this, FName(*Name));
			Copies->SetupAttachment(Root);
			Copies->SetStaticMesh(Mesh);
			Copies->SetMaterial(0, ShadeInk);
			Copies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Copies->SetCanEverAffectNavigation(false);
			Copies->SetCastShadow(false);
			Copies->bReceivesDecals = false;
			// (drawn before the smoke and fire, which must not be darkened where they pass in front of it; gone past
			// 800 m, where a tank is a few pixels)
			Copies->SetTranslucentSortPriority(-100);
			Copies->SetCullDistances(0, FMath::RoundToInt32((bShadesHigh ? 3000.0 : 800.0) * UnitsPerMetre));
			Copies->RegisterComponent();
			Index = ShadeCopies.Add(Copies);
			ShadeFree.SetNum(ShadeCopies.Num());
		}
	}
	ShadeCopyByName.Add(ModelKey, Index);
	return Index;
}

void ALandBattle::DrawShades()
{
	FLandBattleState& S = *State;
	if (!S.Armour || !ShadeInk)
	{
		return;
	}
	// Each vehicle's shade under her root (the ground under her middle, tilted as she stands), shown while she is, a
	// wreck too; moved only when she has moved a centimetre or turned. A shade given back is shrunk to nothing and used
	// again for the next of its kind.
	const std::vector<LandKit::Vehicle>& All = S.Armour->Vehicles();
	const int32 Count = static_cast<int32>(All.size());
	TArray<bool> Touched;
	Touched.Init(false, ShadeCopies.Num());
	auto GiveBack = [this, &Touched](FShadeDrawn& D)
	{
		if (ShadeCopies.IsValidIndex(D.Copy) && ShadeCopies[D.Copy] && D.Slot >= 0)
		{
			ShadeCopies[D.Copy]->UpdateInstanceTransform(D.Slot, FTransform(FQuat::Identity, D.At, FVector(1e-3)), true, false, true);
			ShadeFree[D.Copy].Add(D.Slot);
			if (Touched.IsValidIndex(D.Copy))
			{
				Touched[D.Copy] = true;
			}
		}
		D = FShadeDrawn();
	};
	for (int32 K = Count; K < ShadesDrawn.Num(); ++K)
	{
		GiveBack(ShadesDrawn[K]);
	}
	ShadesDrawn.SetNum(Count);
	// The general's view (the art director, 22 November 18:27Z: "give each tank a soft cast shadow on the ground, which
	// doubles its footprint and gives the hull its shape"): her shade grown with her (FLandCrowd::GrowOf), laid 1.4
	// times as big and moved away from the sun by half the shadow two thirds of her height throws (never more than 0.6
	// of her length), along the ground she stands on: from his height it is her footprint and her cast shadow in one;
	// and seen to 3 km, as far as his eye looks over the field (800 m, where a tank is a few pixels, out of it).
	const bool bHigh = S.General.bOn && !S.General.bFilm;
	if (bHigh != bShadesHigh)
	{
		bShadesHigh = bHigh;
		for (UInstancedStaticMeshComponent* Copies : ShadeCopies)
		{
			if (Copies)
			{
				Copies->SetCullDistances(0, FMath::RoundToInt32((bHigh ? 3000.0 : 800.0) * UnitsPerMetre));
			}
		}
	}
	FVector Away(-TowardSun.X, -TowardSun.Y, 0.0);
	const double SunFlat = Away.Size();
	Away = SunFlat > 1e-3 ? Away / SunFlat : FVector::ZeroVector;
	// (the shadow's length a metre of height: never past 3, the low sun's)
	const double ThrowPerMetre = FMath::Min(3.0, SunFlat / FMath::Max(0.05, static_cast<double>(TowardSun.Z)));
	for (int32 K = 0; K < Count; ++K)
	{
		const LandKit::Vehicle& V = All[static_cast<size_t>(K)];
		FShadeDrawn& D = ShadesDrawn[K];
		const LandKit::Model& Model = V.Look();
		if (D.Model != &Model)
		{
			GiveBack(D);
			D.Model = &Model;
			D.Copy = ShadeCopyFor(Model);
			Touched.SetNum(ShadeCopies.Num());
			if (!ShadeCopies.IsValidIndex(D.Copy) || !ShadeCopies[D.Copy])
			{
				D.Copy = -1;
				continue;
			}
			const FTransform Hidden(FQuat::Identity, FVector::ZeroVector, FVector(1e-3));
			if (ShadeFree[D.Copy].Num() > 0)
			{
				D.Slot = ShadeFree[D.Copy].Pop(EAllowShrinking::No);
			}
			else
			{
				D.Slot = ShadeCopies[D.Copy]->AddInstance(Hidden, true);
			}
			D.bShown = false;
		}
		if (D.Copy < 0 || D.Slot < 0 || !ShadeCopies[D.Copy])
		{
			continue;
		}
		const bool bShown = V.Fade >= 0.5 && !HiddenVehicle(K);
		FVector At = World(V.Root.At.X, V.Root.At.Y, V.Root.At.Z);
		const FQuat Q(V.Root.Q.X, V.Root.Q.Y, V.Root.Q.Z, V.Root.Q.W);
		double Size = FarCrowd.IsValid() ? FarCrowd->GrowOf(K) : 1.0;
		if (bHigh && bShown)
		{
			// (away from the sun along the ground under her: the way taken into her own frame, kept level in it)
			FVector Along = Q.UnrotateVector(Away);
			Along.Z = 0.0;
			const double Throw = FMath::Min(0.6 * FMath::Max(Model.Long, 1.0), 0.67 * FMath::Max(Model.High, 1.0) * ThrowPerMetre) * Size;
			At += Q.RotateVector(Along.GetSafeNormal()) * (0.5 * Throw * UnitsPerMetre);
			Size *= 1.4;
		}
		if (bShown == D.bShown && (!bShown || (At.Equals(D.At, 1.0) && Q.Equals(D.Q, 1e-4) && FMath::IsNearlyEqual(Size, D.Size, 0.01))))
		{
			continue;
		}
		D.bShown = bShown;
		D.At = At;
		D.Q = Q;
		D.Size = Size;
		ShadeCopies[D.Copy]->UpdateInstanceTransform(D.Slot, FTransform(Q, At, FVector(bShown ? Size : 1e-3)), true, false, true);
		Touched[D.Copy] = true;
	}
	for (int32 C = 0; C < Touched.Num(); ++C)
	{
		if (Touched[C] && ShadeCopies.IsValidIndex(C) && ShadeCopies[C])
		{
			ShadeCopies[C]->MarkRenderInstancesDirty();
		}
	}
}

void ALandBattle::DrawManShades()
{
	LAND_SCOPE(DrawManShades);
	FLandBattleState& S = *State;
	if (!S.Men || !ShadeInk)
	{
		return;
	}
	const FTransform Hidden(FQuat::Identity, FVector::ZeroVector, FVector(1e-3));
	if (!bManShadesMade)
	{
		bManShadesMade = true;
		const LandShade::Shape Shapes[3] = { LandShade::Standing(), LandShade::Prone(), LandShade::Body() };
		for (int32 K = 0; K < 3; ++K)
		{
			LandShade::Shape Shape = Shapes[K];
			Shape.Seed = 7u + static_cast<uint32>(K);
			LandShade::Mesh Laid;
			LandShade::Make(Shape, Laid);
			UStaticMesh* Mesh = LandParts::ShadeMesh(this, FName(*FString::Printf(TEXT("LandManShade%d_Mesh"), K)), Laid, ShadeInk, UnitsPerMetre);
			if (!Mesh)
			{
				ManShadeCopies.Reset();
				return;
			}
			ShadeMeshes.Add(Mesh);
			UInstancedStaticMeshComponent* Copies = NewObject<UInstancedStaticMeshComponent>(this, FName(*FString::Printf(TEXT("LandManShade%d"), K)));
			Copies->SetupAttachment(Root);
			Copies->SetStaticMesh(Mesh);
			Copies->SetMaterial(0, ShadeInk);
			Copies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Copies->SetCanEverAffectNavigation(false);
			Copies->SetCastShadow(false);
			Copies->bReceivesDecals = false;
			Copies->SetTranslucentSortPriority(-100);
			Copies->RegisterComponent();
			ManShadeCopies.Add(Copies);
		}
		ManShadeFree.SetNum(3);
	}
	if (ManShadeCopies.Num() != 3)
	{
		return;
	}
	// A man's shade while he is drawn and within 150 m as the picture sees him (past that it is a pixel or two): a
	// blob while he stands, kneels or runs, his body's length while he lies (along his heading, his root at his hips),
	// a wider blob once he lies dead; tilted to the ground under him, moved only when he has moved a centimetre or (lying)
	// turned a degree. A shade given back is shrunk to nothing and used again for the next man of that kind.
	LandMen::Crowd& Crowd = *S.Men;
	const int32 Count = Crowd.Count();
	bool Touched[3] = { false, false, false };
	// (fewer men than shades after a live battle's reset: the rest given back)
	for (int32 Man = Count; Man < ManShades.Num(); ++Man)
	{
		FManShade& D = ManShades[Man];
		if (D.Kind >= 0 && D.Slot >= 0)
		{
			ManShadeCopies[D.Kind]->UpdateInstanceTransform(D.Slot, Hidden, true, false, true);
			ManShadeFree[D.Kind].Add(D.Slot);
			Touched[D.Kind] = true;
		}
	}
	ManShades.SetNum(Count);
	for (int32 Man = 0; Man < Count; ++Man)
	{
		FManShade& D = ManShades[Man];
		int32 Kind = -1;
		double X = 0.0, Y = 0.0, Z = 0.0;
		float Facing = 0.0f;
		// (his place as last posed: a living man's height is the ground's under him then, so nothing of the ground is
		// asked for the some thousand men near enough each frame; the frame-rate lane, 22 November)
		if (Crowd.Visible(Man) && Crowd.Fade(Man) >= 0.5f && Crowd.SeenDistance(Man) < 150.0f && !HiddenMan(Man) && !Crowd.IsRider(Man) && Crowd.PlaceAsPosed(Man, X, Y, Z, Facing))
		{
			Kind = Crowd.Frozen(Man) ? 2 : Crowd.LieOf(Man) > 0.5f ? 1 : 0;
		}
		if (Kind != D.Kind)
		{
			if (D.Kind >= 0 && D.Slot >= 0)
			{
				ManShadeCopies[D.Kind]->UpdateInstanceTransform(D.Slot, Hidden, true, false, true);
				ManShadeFree[D.Kind].Add(D.Slot);
				Touched[D.Kind] = true;
			}
			D = FManShade();
			if (Kind < 0)
			{
				continue;
			}
			D.Kind = Kind;
			D.Slot = ManShadeFree[Kind].Num() > 0 ? ManShadeFree[Kind].Pop(EAllowShrinking::No) : ManShadeCopies[Kind]->AddInstance(Hidden, true);
			D.At = FVector(1e30);
		}
		if (Kind < 0)
		{
			continue;
		}
		// (his heading is clockwise from north, the field's y: in Unreal's frame, x north and y east, that is a yaw)
		const float Yaw = FMath::RadiansToDegrees(Facing);
		const FVector Gate(X, Y, Z);
		if (Gate.Equals(D.Gate, 0.01) && (Kind != 1 || FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, D.Yaw)) < 1.0f))
		{
			continue;
		}
		D.Gate = Gate;
		// (a body, or a man falling, by the ground at his root as it is now: his last posing read it under his pivot)
		if (Kind == 2 || Crowd.IsFalling(Man))
		{
			Crowd.PlaceOf(Man, X, Y, Z, Facing);
		}
		const FVector At = World(X, Y, Z);
		if (At.Equals(D.At, 1.0) && (Kind != 1 || FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, D.Yaw)) < 1.0f))
		{
			continue;
		}
		D.At = At;
		D.Yaw = Yaw;
		// (the ground's up read again once he is half a metre on from where it was last read, and for a body)
		FVector Up = D.Up;
		if (S.Field && (Kind == 2 || FMath::Square(X - D.UpAt.X) + FMath::Square(Y - D.UpAt.Y) >= 0.25))
		{
			D.UpAt = FVector(X, Y, 0.0);
			const LandField::Surface Under = S.Field->At(X, Y);
			Up = FVector(Under.NY, Under.NX, Under.NZ).GetSafeNormal(1e-6, FVector::UpVector);
			D.Up = Up;
		}
		const FQuat Q = FQuat::FindBetweenNormals(FVector::UpVector, Up) * FQuat(FVector::UpVector, static_cast<double>(Facing));
		ManShadeCopies[Kind]->UpdateInstanceTransform(D.Slot, FTransform(Q, At, FVector::OneVector), true, false, true);
		Touched[Kind] = true;
	}
	for (int32 K = 0; K < 3; ++K)
	{
		if (Touched[K])
		{
			ManShadeCopies[K]->MarkRenderInstancesDirty();
		}
	}
}

// ---- The day over the field: its weather, light, sky, clouds and haze ------------------------------------------

/**
 * The day over the field (the art director's notes on cut 1, 21 November 2026). The recording's sky block is the
 * battle's one weather draw, clear to overcast (Tools/land_record: cover, cloud base, cumulus or a deck), with the
 * place and the hour for the sun; from it come the light (LightTheBattle says it every frame), the clouds (the air
 * battle's own AirSky, LaySky and DrawSky), the sky (the battle look's, AirSkyBegin, set for the ground) and the haze.
 */
struct FLandSky
{
	// The day.
	double Cover = 0.3, Base = 900.0;
	bool bDeck = false;
	/** 0 a fair day, 1 overcast: the light softer and flatter, the shadows soft, no sun's disc, the haze greyer. */
	double Overcast = 0.0;
	// The light it gives (strengths in the engine's lux; the fill a lamp that throws no shadow).
	float Key = 4.0f, Fill = 1.5f, SkyLight = 0.8f, FillKelvin = 7500.0f, KeyAngle = 0.53f, Disc = 0.006f;
	FRotator FillFacing = FRotator::ZeroRotator;
	/** Shaded open ground's light against sunlit, before the grade (the share asked, -TheaterLandFill). */
	double Shade = 0.36;
	// The clouds: laid once, turned to the eye as it moves (the air battle's two rooms), carried by the wind.
	AirSky::Field Clouds;
	AirSky::Rooms Rooms;
	AirSky::Vec Sun;
	AirSky::Rgb Light = AirSky::Rgb(1.0f, 1.0f, 1.0f);
	bool bShapes = false;
	TWeakObjectPtr<UProceduralMeshComponent> Far, Near;
	TArray<FVector> FarPoints, NearPoints;
	TArray<FColor> FarColours, NearColours;
	TArray<FVector2D> FarPlaces, FarWays, NearPlaces, NearWays;
	FVector DriftShown = FVector(1e30, 0.0, 0.0);
	int32 NearTurns = 0, BothTurns = 0;
	double TurnSeconds = 0.0, TurnsSaidAt = 0.0;
	// GS6: or turned on the graphics chip (LaySky): the chip's table of the puffs, far to near, the one mesh it is read
	// by (made once) and the float picture it is sent in; how often it was laid again, what that cost this thread and
	// what it sent.
	TUniquePtr<AirSky::Chip> Chip;
	TWeakObjectPtr<UProceduralMeshComponent> Turned;
	TWeakObjectPtr<UTexture2D> Table;
	int32 Lays = 0;
	double LaySeconds = 0.0, LayBytes = 0.0;
	bool bDroppedSaid = false;
	// The grade laid over the whole world while the battle stands, under the eye's own (KeepTheLook), and its check.
	TWeakObjectPtr<APostProcessVolume> Grade;
	bool bLookSaid = false, bTargetSaid = false;
};

namespace LandLook
{
	/** The numbers of a switch such as "-TheaterLandFill=0.4,,7000": each one given takes the place of the one kept. */
	static void Asked(const TCHAR* Switch, TArray<double*> Kept)
	{
		FString Text;
		if (!FParse::Value(FCommandLine::Get(), Switch, Text, false))
		{
			return;
		}
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), false);
		for (int32 K = 0; K < Parts.Num() && K < Kept.Num(); ++K)
		{
			if (!Parts[K].TrimStartAndEnd().IsEmpty())
			{
				*Kept[K] = FCString::Atod(*Parts[K]);
			}
		}
	}

	/** A room's points as the painter laid them (field metres) into the engine's frame, with their colours (linear
	 * already) and, drawn from the sheet of shapes, each point's place on it and the sun's way on it (four numbers a
	 * point, as the air battle's SheetPlaces). At most as many as the mesh was made with. */
	static void FillRoom(const std::vector<AirSky::Vec>& From, const std::vector<uint8_t>& FromRgba, const std::vector<float>* FromUvs, double U,
		TArray<FVector>& Points, TArray<FColor>& Colours, TArray<FVector2D>* Places, TArray<FVector2D>* Ways)
	{
		const int32 Count = FMath::Min3(Points.Num(), Colours.Num(), static_cast<int32>(FMath::Min(From.size(), FromRgba.size() / 4)));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const AirSky::Vec& P = From[static_cast<size_t>(Index)];
			Points[Index] = FVector(P.Y * U, P.X * U, P.Z * U);
			const size_t C = static_cast<size_t>(Index) * 4;
			Colours[Index] = FColor(FromRgba[C], FromRgba[C + 1], FromRgba[C + 2], FromRgba[C + 3]);
		}
		if (FromUvs && Places && Ways)
		{
			for (int32 Index = 0; Index < Count && Index < Places->Num() && Index < Ways->Num(); ++Index)
			{
				const size_t At = static_cast<size_t>(Index) * 4;
				if (At + 3 < FromUvs->size())
				{
					(*Places)[Index] = FVector2D((*FromUvs)[At], (*FromUvs)[At + 1]);
					(*Ways)[Index] = FVector2D((*FromUvs)[At + 2], (*FromUvs)[At + 3]);
				}
				else
				{
					(*Places)[Index] = FVector2D::ZeroVector;
					(*Ways)[Index] = FVector2D::ZeroVector;
				}
			}
		}
	}

	/** GS6: the chip's table (its first Rows rows, four floats a texel) sent to its picture; the copy is the renderer's
	 * until it has read it. */
	static void SendTable(UTexture2D* Table, const std::vector<float>& Cells, int32 Rows)
	{
		if (!Table || !Table->GetResource() || Rows <= 0)
		{
			return;
		}
		const int32 Wide = Table->GetSizeX();
		Rows = FMath::Min(Rows, Table->GetSizeY());
		const size_t Floats = static_cast<size_t>(Wide) * static_cast<size_t>(Rows) * 4;
		if (Cells.size() < Floats)
		{
			return;
		}
		float* Copy = static_cast<float*>(FMemory::Malloc(Floats * sizeof(float)));
		FMemory::Memcpy(Copy, Cells.data(), Floats * sizeof(float));
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Wide, Rows);
		Table->UpdateTextureRegions(0, 1, Region, Wide * 4 * sizeof(float), 4 * sizeof(float), reinterpret_cast<uint8*>(Copy),
			[](uint8* Data, const FUpdateTextureRegion2D* Regions)
			{
				FMemory::Free(Data);
				delete Regions;
			});
	}
}

// ---- The table under the battle (as the air battle keeps it) ---------------------------------------------------

void ALandBattle::HideTheTable()
{
	if (bTableHidden)
	{
		return;
	}
	bTableHidden = true;
	SkyDay = MakeShared<FLandSky>();
	FLandSky& Day = *SkyDay;

	// The day: the recording's sky block, the battle's one weather draw (clear to overcast). "-TheaterLandWeather=
	// cover,deck,base" tries another day without a new recording (cover 0 to 1; deck 1 for a deck, 0 for cumulus; the
	// cloud base in metres): "-TheaterLandWeather=0.95,1" is an overcast Normandy morning.
	double Hour = 10.0;
	if (State.IsValid())
	{
		const LandFeed::Sky& Drawn = State->Rec.Weather;
		Day.Cover = Drawn.Cover;
		Day.Base = Drawn.Base;
		Day.bDeck = Drawn.Kind == "deck";
		Hour = Drawn.Hour;
	}
	{
		double DeckAsked = Day.bDeck ? 1.0 : 0.0;
		LandLook::Asked(TEXT("TheaterLandWeather="), { &Day.Cover, &DeckAsked, &Day.Base });
		Day.Cover = FMath::Clamp(Day.Cover, 0.0, 1.0);
		Day.Base = FMath::Clamp(Day.Base, 300.0, 3000.0);
		Day.bDeck = DeckAsked >= 0.5 || Day.Cover >= 0.8;
	}
	// (scattered cumulus up to about half the sky is a fair day; past it the light dulls, and a deck is overcast)
	Day.Overcast = Day.bDeck ? 1.0 : FMath::SmoothStep(0.55, 0.85, Day.Cover);
	// The sun from the place, the month and the hour (Open turned it for the recording's hour; "-TheaterLandHour=" tries
	// another, local sun time).
	float HourAsked = static_cast<float>(Hour);
	if (State.IsValid() && FParse::Value(FCommandLine::Get(), TEXT("TheaterLandHour="), HourAsked))
	{
		const FVector Way = LandParts::SunWay(State->Rec.Weather.Lat, LandParts::MonthOf(State->Rec.Date), FMath::Clamp(static_cast<double>(HourAsked), 5.0, 20.0));
		TowardSun = WorldWay(Way.X, Way.Y, Way.Z).GetSafeNormal();
		if (Effects)
		{
			Effects->SetSun(TowardSun);
		}
	}

	// The light (the art director on cut 1, note 2: shaded ground at 0.4 to 0.55 of sunlit with a slight blue, the
	// crowns' shade about half their lit side; the shadows had crushed to black, sunlit grass 139 and shaded 24). The
	// table's fill lamp, a weak cool light from a fixed side of the room, is turned into the sky's light on the field:
	// from 60 degrees up on the side away from the sun, so it lights the shaded ground and the crowns' shaded sides,
	// throwing no shadow; with the sky light's own soft fill. Light is shared between them so that sunlit ground keeps
	// what it had on cut 1 (the key at 7 less 2.5 for each whole of cover, and the old fill), now Shade of it in every
	// shadow: 0.36 of it in light makes about 0.45 of its brightness through the grade's contrast and the film curve.
	// Overcast, 0.62 of it, the sun's disc gone and its shadows soft.
	// "-TheaterLandFill=shade,overcast shade,kelvin,height" tries others (shaded ground's share of sunlit in light on a
	// fair day and an overcast one, the fill's warmth in kelvin (the sun is 5200), and how high it stands in degrees).
	{
		double ShadeFair = 0.36, ShadeDull = 0.62, Kelvin = 7500.0, FillUpDeg = 60.0;
		LandLook::Asked(TEXT("TheaterLandFill="), { &ShadeFair, &ShadeDull, &Kelvin, &FillUpDeg });
		// (the shade's share carried by the sky light, the rest by the fill: the art director on cut 4, fix 2. The fill
		// throws no shadow and comes from 60 degrees up, so a hull's side or a hedge's face turned across the sun got half
		// of it or none and went black, and the ground under a hull got as much as the open field; the sky's light comes
		// from the whole dome and the shade under things (ambient occlusion) darkens it. "-TheaterLandSkyShare=0.15" is
		// about cut 4's split)
		double SkyShare = 0.6;
		LandLook::Asked(TEXT("TheaterLandSkyShare="), { &SkyShare });
		SkyShare = FMath::Clamp(SkyShare, 0.0, 0.95);
		Day.Shade = FMath::Clamp(FMath::Lerp(ShadeFair, ShadeDull, Day.Overcast), 0.02, 0.95);
		const double SunUp = FMath::Max(TowardSun.Z, FMath::Sin(FMath::DegreesToRadians(8.0)));
		const double Whole = (7.0 - 2.5 * Day.Cover) * SunUp + 0.5;
		Day.Key = static_cast<float>((1.0 - Day.Shade) * Whole / SunUp);
		// (the sky light's own share on open ground, from the table's daylight cubemap: about 0.15 of its strength, as
		// cut 1's shade measured; SkyShare of the shade from it, never less than cut 4's; the fill gives the rest)
		Day.SkyLight = static_cast<float>(FMath::Max(0.72 + 0.25 * Day.Cover, SkyShare * Day.Shade * Whole / 0.15));
		const double FillUp = FMath::DegreesToRadians(FMath::Clamp(FillUpDeg, 15.0, 90.0));
		Day.Fill = static_cast<float>(FMath::Max(0.05, (Day.Shade * Whole - 0.15 * Day.SkyLight) / FMath::Sin(FillUp)));
		FVector Away = FVector(-TowardSun.X, -TowardSun.Y, 0.0).GetSafeNormal();
		if (Away.IsNearlyZero())
		{
			Away = FVector(1.0, 0.0, 0.0);
		}
		const FVector ToFill = Away * FMath::Cos(FillUp) + FVector::UpVector * FMath::Sin(FillUp);
		Day.FillFacing = (-ToFill).Rotation();
		Day.FillKelvin = static_cast<float>(FMath::Clamp(Kelvin, 1700.0, 20000.0));
		// (a hard small sun on a fair day, as the air battle's; overcast, a broad soft one and no disc)
		Day.KeyAngle = static_cast<float>(FMath::Lerp(0.53, 6.0, Day.Overcast));
		Day.Disc = static_cast<float>(0.006 * (1.0 - Day.Overcast));
	}

	// The haze at the ground (note 4: the tree line 400 m off was as black as the near crowns, the field a diorama).
	// A low layer, as a summer morning's: lifted toward the horizon's colour about 17 per cent at 400 m, 38 at a
	// kilometre, 76 at 3 km, 91 at 5 km where the far ring ends, and the most it lifts (0.98) at the skyline; thin
	// overhead, so the sky over the eye keeps its blue (the layer's density halves every 100 m up, about 7 per cent
	// looking straight up). The
	// engine thins light by two to the power of minus the density in thousandths of a centimetre times the way, so
	// 0.0069 lifts 1 - 2^-0.69 = 38 per cent at a kilometre. The opening aerial (170 m up) sees about 25 per cent at a
	// kilometre and 57 at three, a little more than cut 1's 15 and 45.
	// "-TheaterLandHaze=start metres,density,falloff,most" tries other numbers (the density at the field's ground).
	// (a ground not the bocage's has its own air, LandScape's: the desert's pale warm dust, the jungle's thick damp
	// haze, the winter's cold grey-blue; the bocage's and the steppe's are these numbers as they were)
	LandScape::Air LookAir;
	if (State.IsValid() && State->Field)
	{
		LookAir = LandScape::AirOf(LandScape::LookOf(State->Field->LookWord()), State->Field->SnowLying());
	}
	double HazeStart = 0.0, HazeDensity = LookAir.bSet ? LookAir.HazeDensity : 0.0069, HazeFalloff = 0.1, HazeMost = 0.98;
	LandLook::Asked(TEXT("TheaterLandHaze="), { &HazeStart, &HazeDensity, &HazeFalloff, &HazeMost });
	HazeFalloff = FMath::Max(0.0, HazeFalloff);
	// (a warm grey-blue on a fair day, the summer haze over the bocage, not the sea's blue; greyer under overcast)
	const FLinearColor HazeFair = LookAir.bSet ? FLinearColor(static_cast<float>(LookAir.HazeInk[0]), static_cast<float>(LookAir.HazeInk[1]), static_cast<float>(LookAir.HazeInk[2])) : FLinearColor(0.62f, 0.66f, 0.70f);
	const FLinearColor HazeDull = LookAir.bSet ? FLinearColor(static_cast<float>(LookAir.HazeOvercast[0]), static_cast<float>(LookAir.HazeOvercast[1]), static_cast<float>(LookAir.HazeOvercast[2])) : FLinearColor(0.50f, 0.52f, 0.54f);
	const FLinearColor HazeInk = FMath::Lerp(HazeFair, HazeDull, static_cast<float>(Day.Overcast));
	// (old smoke pales into the air behind it, the effects kit's slow smoke: the horizon's haze as the screen shows it,
	// its sRGB pixel; the haze's own colour stands for it, the stills' horizon reading within a few levels of it)
	if (Effects)
	{
		const FColor HazePixel = HazeInk.ToFColor(true);
		Effects->SetSky(FLinearColor(HazePixel.R / 255.0f, HazePixel.G / 255.0f, HazePixel.B / 255.0f, 1.0f));
	}
	double GroundMean = 0.0;
	if (State.IsValid() && !State->Rec.Ground.Height.empty())
	{
		for (const float H : State->Rec.Ground.Height)
		{
			GroundMean += H;
		}
		GroundMean /= static_cast<double>(State->Rec.Ground.Height.size());
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (ACampaignMapActor* Map = Cast<ACampaignMapActor>(*It))
		{
			Table = Map;
			Map->SetPutAway(true);
			// (its per-frame update held while the battle is up, the gate's MapUpdate; -TheaterLandMapUpdate=1 keeps it
			// running, as before, for the A/B)
			int32 MapUpdate = 0;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterLandMapUpdate="), MapUpdate);
			Map->HoldUpdates(MapUpdate == 0);
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
				Kept.Colour = LandParts::HazeColour(Fog, FLinearColor(0.80f, 0.84f, 0.88f));
				// (the density asked is the field's, at its mean ground: the haze's own height is where its lamp stands)
				const double Below = FMath::Clamp(HazeFalloff / 1000.0 * (GroundMean * UnitsPerMetre - Fog->GetComponentLocation().Z), -20.0, 20.0);
				Fog->SetStartDistance(static_cast<float>(FMath::Max(0.0, HazeStart) * UnitsPerMetre));
				Fog->SetFogDensity(static_cast<float>(FMath::Max(0.0, HazeDensity) * FMath::Pow(2.0, Below)));
				Fog->SetFogHeightFalloff(static_cast<float>(HazeFalloff));
				Fog->SetFogMaxOpacity(static_cast<float>(FMath::Clamp(HazeMost, 0.0, 1.0)));
				Fog->SetFogInscatteringColor(HazeInk);
			}
			TArray<UDirectionalLightComponent*> Lights;
			Environment->GetComponents(Lights);
			UDirectionalLightComponent* KeyLight = nullptr;
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
				if (Light->GetFName() == TEXT("KeyLight") || (!KeyLight && Lights.Num() == 1))
				{
					KeyLight = Light;
				}
			}
			if (!KeyLight)
			{
				for (UDirectionalLightComponent* Light : Lights)
				{
					if (!KeyLight || Light->Intensity > KeyLight->Intensity)
					{
						KeyLight = Light;
					}
				}
			}
			if (KeyLight)
			{
				BattleKey = KeyLight;
				KeyLight->SetWorldRotation((-TowardSun).Rotation());
				BattleLook::AirSkyBegin(*this, KeyLight);
				// (the sun a small hard disc, as the air battle's: the art director's ruling on build 1750; overcast, soft)
				KeyLight->LightSourceAngle = Day.KeyAngle;
				KeyLight->AtmosphereSunDiskColorScale = FLinearColor(Day.Disc, Day.Disc, Day.Disc);
				KeyLight->MarkRenderStateDirty();
			}
			TArray<USkyLightComponent*> Skies;
			Environment->GetComponents(Skies);
			// (the sky's lower half the field's own colour, dimmed, not black: the bounce off the lit ground that a hull's
			// side and a hedge's underside catch. The art director on cut 4, fix 2. "-TheaterLandSkyBelow=0" keeps it black)
			double Below = 0.3;
			LandLook::Asked(TEXT("TheaterLandSkyBelow="), { &Below });
			// (on snow the bounce is the snow's, white and strong: lit snow is near as bright as the sky over it, and lights
			// whatever faces it. The art director on Valdai, 24 Nov: a hull's shaded side read black, 18-22 against snow at
			// 165, where the snow lifts it to 55-75. "-TheaterLandSnowBelow=" its strength at full snow, 2 (the Mac at
			// Valdai on d40cc95d, the hull's shaded side: 0.85 gave 38, 1.5 52, 2 62, open snow 159 throughout); the snow
			// lying, 0 to 1, takes it from the field's colour to the snow's)
			const double Snowed = State.IsValid() && State->Field ? FMath::Clamp(State->Field->SnowLying(), 0.0, 1.0) : 0.0;
			double SnowBelow = 2.0;
			LandLook::Asked(TEXT("TheaterLandSnowBelow="), { &SnowBelow });
			const FLinearColor Bare = FLinearColor::FromSRGBColor(FColor(118, 116, 72)) * static_cast<float>(FMath::Clamp(Below, 0.0, 1.0));
			const FLinearColor Snowy = FLinearColor::FromSRGBColor(FColor(214, 220, 232)) * static_cast<float>(FMath::Clamp(SnowBelow, 0.0, 4.0));
			const FLinearColor Field = FMath::Lerp(Bare, Snowy, static_cast<float>(Snowed));
			// (the engine lays this colour over the sky's lower half only when the lower half is a solid colour, which
			// the flag the engine still calls "black" says: with it off, as it was, the colour was ignored and the lower
			// half was the table's cubemap's dark ground, so the hull's shaded side stayed 18-22 on the Mac at Valdai,
			// 9ebf1fcc. Turned on where snow lies as the campaign names a snowy field (0.25 and more, FieldLookAt's
			// "+snow"); elsewhere the lower half stays the cubemap's, as every other look passed with. Its alpha is set
			// to 1, as the air battle's light from below (AirBattle.cpp): the scalings above carry into the alpha (0.78 at
			// full snow, past 1 above 1.15) and the engine may weigh the colour by it. "-TheaterLandSnowBelowOld" keeps
			// the cubemap's on snow too, to set the two side by side)
			const bool bSolidBelow = Snowed >= 0.25 && !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandSnowBelowOld"));
			UE_LOG(LogTheater, Display, TEXT("Land battle look: the sky's lower half (the ground's bounce) %.3f, %.3f, %.3f (the snow lying %.2f; snow's strength %.2f, -TheaterLandSnowBelow=), %s"), Field.R, Field.G, Field.B, Snowed, SnowBelow,
				bSolidBelow ? TEXT("laid as a solid colour") : Snowed >= 0.25 ? TEXT("the cubemap's own (-TheaterLandSnowBelowOld)") : TEXT("the cubemap's own (little or no snow)"));
			for (USkyLightComponent* Sky : Skies)
			{
				FKeptSky& Kept = KeptSkies.AddDefaulted_GetRef();
				Kept.Sky = Sky;
				Kept.Intensity = Sky->Intensity;
				Kept.Colour = Sky->GetLightColor();
				Kept.bLowerBlack = Sky->bLowerHemisphereIsBlack;
				Kept.Lower = Sky->LowerHemisphereColor;
				if (Below > 0.0)
				{
					Sky->bLowerHemisphereIsBlack = bSolidBelow;
					Sky->SetLowerHemisphereColor(FLinearColor(Field.R, Field.G, Field.B, 1.0f));
					Sky->RecaptureSky();
				}
			}
		}
	}
	// (the fill's share between the lamps that are not the sun: the table has one)
	int32 FillLamps = 0;
	for (const FKeptLamp& Kept : KeptLamps)
	{
		FillLamps += Kept.Lamp.Get() != BattleKey.Get() ? 1 : 0;
	}
	Day.Fill /= static_cast<float>(FMath::Max(1, FillLamps));

	// The sky (note 3: a blank pale grey in every still). The battle look's air and sky (AirSkyBegin), set for the
	// ground: the haze laid wholly over the sky, since it lies low now and thins upward (a pale horizon, the blue kept
	// overhead), less dust in the air than the air battle's (the haze is the fog's work, and dust whitens the blue), the
	// sky's light the air battle's 1.3 (it is lit by the key, which gives the shade its share now: the sky stands at
	// about two thirds of cut 1's), and darker under cloud. "-TheaterLandSky=bright,air,dust,haze over the sky" tries
	// others (each against the engine's own numbers, as -TheaterAirSky's).
	double SkyBright = 1.3, SkyAir = 1.0, SkyDust = LookAir.bSet ? LookAir.SkyDust : 0.6, HazeOver = 1.0;
	LandLook::Asked(TEXT("TheaterLandSky="), { &SkyBright, &SkyAir, &SkyDust, &HazeOver });
	const float SkyShine = static_cast<float>(FMath::Max(0.0, SkyBright) * (1.0 - 0.45 * Day.Overcast));
	// The sky overhead lifted as the sun climbs (the art director on 849c1bc1: Baghdad's zenith 50,59,74 with the sun 61
	// degrees up, "a night storm over Iraq", and Voronezh's 63,71,85 at 52; it should be a pale hazy blue, about
	// 120,145,185, near white at the horizon). The field's light grows with the sun's height (Whole, in Day's lamps) while
	// the sky the engine draws from the key does not, so under a high sun the field outshone its own sky. The sky's own
	// pixels only (SkyLuminanceFactor: not the haze over the far ground, and not the sky light, which is the table's
	// cubemap): as it was to a sun 25 degrees up, rising to SkyLift times at 60 and over. Only the looks that ask it (the
	// dry steppe's and the farmland's, LandScape's Air); the others' skies keep 1, as they were. "-TheaterLandSkyLift=1"
	// leaves the sky as it was.
	double SkyLift = LookAir.SkyLift;
	LandLook::Asked(TEXT("TheaterLandSkyLift="), { &SkyLift });
	const double SunDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(TowardSun.Z, -1.0, 1.0)));
	const float Lift = static_cast<float>(1.0 + (FMath::Max(1.0, SkyLift) - 1.0) * FMath::SmoothStep(25.0, 60.0, SunDeg));
	TArray<USkyAtmosphereComponent*> Atmospheres;
	GetComponents(Atmospheres);
	for (USkyAtmosphereComponent* Atmosphere : Atmospheres)
	{
		Atmosphere->SetSkyLuminanceFactor(FLinearColor(Lift, Lift, Lift));
		Atmosphere->SetMieAnisotropy(0.75f);
		Atmosphere->SetRayleighScatteringScale(static_cast<float>(0.0331 * FMath::Max(0.0, SkyAir)));
		Atmosphere->SetMieScatteringScale(static_cast<float>(0.003996 * FMath::Max(0.0, SkyDust)));
		Atmosphere->SetHeightFogContribution(static_cast<float>(FMath::Clamp(HazeOver, 0.0, 1.0)));
		Atmosphere->SetSkyAndAerialPerspectiveLuminanceFactor(FLinearColor(SkyShine, SkyShine, SkyShine));
	}
	// The battle grade laid over the whole world too, while the battle stands (note 1): the same BattleLook::Grade as
	// the eye's own, unbound, under it. Where the eye's lens is drawn through, its own grade stands over this one and
	// nothing changes; if anything ever draws the picture past the eye's lens, it is graded still. Gone with the table
	// shown again. ("-TheaterNoBattleGrade" leaves both off.)
	if (UWorld* Here = GetWorld())
	{
		if (APostProcessVolume* Over = Here->SpawnActor<APostProcessVolume>())
		{
			Over->bUnbound = true;
			Over->bEnabled = true;
			Over->Priority = 0.0f;
			Over->BlendWeight = 1.0f;
			LandParts::Grade(Over->Settings, State.IsValid() ? &State->Rec.Weather : nullptr, &LookAir);
			Day.Grade = Over;
		}
	}
	LightTheBattle();
	LaySky();
	UE_LOG(LogTheater, Display, TEXT("Land battle look: %s, cover %.2f (overcast %.2f), clouds from %.0f m; the sun %.0f degrees up; key %.2f, the sky's fill %.2f at %.0f K and the sky light %.2f, shaded open ground %.2f of sunlit in light before the grade; the haze from %.0f m, %.4f at the ground (%.0f m), falloff %.3f, at most %.2f; the sky %.2f bright (its own pixels %.2f, for the sun's height), air %.2f, dust %.2f, haze over it %.2f (%d skies)"),
		Day.bDeck ? TEXT("a deck") : TEXT("cumulus"), Day.Cover, Day.Overcast, Day.Base, FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(TowardSun.Z, -1.0, 1.0))),
		Day.Key, Day.Fill, Day.FillKelvin, Day.SkyLight, Day.Shade, HazeStart, HazeDensity, GroundMean, HazeFalloff, HazeMost, SkyShine, Lift, SkyAir, SkyDust, HazeOver, Atmospheres.Num());
}

void ALandBattle::LightTheBattle()
{
	LAND_SCOPE(Light);
	// The battle's day, said again wherever the room has changed it: every lamp white, the key turned to the sun at the
	// day's strength and warmth, the other lamps the sky's fill (turned to the sky away from the sun, cool, throwing no
	// shadow), and the sky light the day's (HideTheTable works them out).
	const FLandSky* Day = SkyDay.Get();
	if (!Day)
	{
		return;
	}
	for (const FKeptLamp& Kept : KeptLamps)
	{
		UDirectionalLightComponent* Light = Kept.Lamp.Get();
		if (!Light)
		{
			continue;
		}
		const bool bKey = Light == BattleKey.Get();
		const float Want = bKey ? Day->Key : Day->Fill;
		if (!Light->GetLightColor().Equals(FLinearColor::White, 0.002f))
		{
			Light->SetLightColor(FLinearColor::White);
		}
		if (!FMath::IsNearlyEqual(Light->Intensity, Want, 0.001f))
		{
			Light->SetIntensity(Want);
		}
		const float Warmth = bKey ? 5200.0f : Day->FillKelvin;
		if (!FMath::IsNearlyEqual(Light->Temperature, Warmth, 1.0f))
		{
			Light->SetTemperature(Warmth);
		}
		if (!bKey && !Light->GetComponentRotation().Equals(Day->FillFacing, 0.05f))
		{
			Light->SetWorldRotation(Day->FillFacing);
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
		if (!FMath::IsNearlyEqual(Sky->Intensity, Day->SkyLight, 0.001f))
		{
			Sky->SetIntensity(Day->SkyLight);
		}
	}
}

void ALandBattle::LaySky()
{
	// The day's clouds (note 3: a fair day's cumulus at the day's base): the air battle's own sky (AirSky), laid once on
	// its square that repeats round the eye, drawn out to 20 km and thinning into the haze, each cloud lit by the
	// battle's sun, carried by the wind; a deck when the day is overcast. The clearing the air battle keeps over its
	// target is put 35 km off, past every cloud drawn round the field. Seen from under them, so drawn before the
	// battle's own smoke and fire (theirs at 0; the clouds at -20 and -15). No cloud shadows on the field: the air
	// battle's lie flat on the sea, and would lie through the hedges here.
	// "-TheaterLandNoClouds" leaves them out; "-TheaterCloudBright=" is the air battle's (1.3).
	if (!SkyDay.IsValid() || !State.IsValid())
	{
		return;
	}
	FLandSky& Day = *SkyDay;
	const LandFeed::Recording& Rec = State->Rec;
	Day.Sun = AirSky::Vec(TowardSun.Y, TowardSun.X, TowardSun.Z);
	Day.Light = Day.bDeck ? AirSky::Rgb(0.82f, 0.83f, 0.86f) : AirSky::Rgb(1.0f, 1.0f, 1.0f);
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoClouds")) || Day.Cover < 0.05)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle look: no clouds (%s)."), Day.Cover < 0.05 ? TEXT("a clear day") : TEXT("-TheaterLandNoClouds"));
		return;
	}
	AirSky::Day Drawn;
	Drawn.bGiven = true;
	Drawn.bDeck = Day.bDeck;
	Drawn.Base = Day.Base;
	// (a summer morning's cumulus some 1.1 km deep over its base, as a fair day's in Normandy; a deck 600 m)
	Drawn.Top = Day.Base + (Day.bDeck ? 600.0 : 1100.0);
	AirSky::Placing Place;
	Place.Seed = Rec.Seed ? Rec.Seed : 238;
	Place.TargetX = 0.5 * Rec.Ground.W + 25000.0;
	Place.TargetY = 0.5 * Rec.Ground.H + 25000.0;
	Place.Band = 0.0;
	Place.LowBand = 0.0;
	Place.bShips = false;
	Place.Sun = Day.Sun;
	Day.Clouds = AirSky::Lay(Drawn, Place);
	if (Day.Clouds.Puffs.empty())
	{
		return;
	}
	UMaterialInterface* Highlight = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	UMaterialInterface* CloudMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Cloud.M_Cloud"));
	UTexture2D* Sheet = nullptr;
	if (CloudMaterial)
	{
		// (the sheet of cloud shapes, some 190 ms of work, made here once: the air battle's own torn fragments)
		std::vector<uint8_t> Rgba;
		AirSky::MakeFragments(Rgba);
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
			Sheet = CampaignTextures::Create(Side, Side, Pixels, false, false, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("LandCloudSheet")));
		}
	}
	Day.bShapes = CloudMaterial && Sheet;
	if (!Day.bShapes && !Highlight)
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle look: M_Cloud and M_Highlight are missing (run Tools/build_content.sh): no clouds are drawn."));
		return;
	}
	if (!Day.bShapes)
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle look: M_Cloud or its sheet is missing (run Tools/build_content.sh): the clouds are soft discs."));
	}
	Day.Rooms.SetSprites(Day.bShapes);
	UMaterialInstanceDynamic* CloudInk = UMaterialInstanceDynamic::Create(Day.bShapes ? CloudMaterial : Highlight, this);
	if (Day.bShapes)
	{
		CloudInk->SetTextureParameterValue(TEXT("Sheet"), Sheet);
	}
	float CloudBright = 1.3f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterCloudBright="), CloudBright);
	CloudBright = FMath::Clamp(CloudBright, 0.5f, 3.0f);
	CloudInk->SetScalarParameterValue(TEXT("Brightness"), CloudBright);
	Day.Rooms.SetInk(CloudBright);
	const AirSky::Vec Drift = Day.Clouds.DriftAt(State->Battle);
	const double MidX = 0.5 * Rec.Ground.W, MidY = 0.5 * Rec.Ground.H;

	// GS6 (the speed gate's clouds on the graphics chip): the same puffs, turned to the eye where the points are placed,
	// every frame, by M_CloudTurned (Battle/AirSkyChip.inl, the painter's own arithmetic, checked point by point against
	// it by Tools/air_show/air_sky_chip_test). This thread lays a table of the puffs, far to near, in a float picture,
	// and lays it again only when that order has gone stale (a few times a second as the eye flies, now and then as it
	// walks); one mesh, made once, reads it. Nothing is turned or sent from here as the eye moves. "-TheaterSkyChip=0"
	// turns them here as before (the two rooms below); "-TheaterSkyChipSort=" the order's staleness, a share of the
	// nearest drawn puff's distance (0.025). Without M_CloudTurned (Tools/build_content.sh) they are turned here.
	int32 ChipAsked = 1;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSkyChip="), ChipAsked);
	UMaterialInterface* TurnedMaterial = Day.bShapes && ChipAsked != 0
		? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_CloudTurned.M_CloudTurned")) : nullptr;
	if (Day.bShapes && ChipAsked != 0 && !TurnedMaterial)
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle look: M_CloudTurned is missing (run Tools/build_content.sh): the clouds are turned on the game's thread, as before."));
	}
	if (TurnedMaterial)
	{
		TUniquePtr<AirSky::Chip> Chip = MakeUnique<AirSky::Chip>();
		double Sort = AirSky::Chip::DefaultOrderTurn;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterSkyChipSort="), Sort);
		Chip->SetOrderTurn(Sort);
		Chip->SetInk(CloudBright);
		const int32 Wide = AirSky::Chip::Wide, High = Chip->Rows();
		UTexture2D* ChipTable = UTexture2D::CreateTransient(Wide, High, PF_A32B32G32R32F, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("LandCloudTable")));
		if (ChipTable && ChipTable->GetPlatformData() && ChipTable->GetPlatformData()->Mips.Num() > 0)
		{
			ChipTable->SRGB = false;
			ChipTable->Filter = TF_Nearest;
			ChipTable->AddressX = TA_Clamp;
			ChipTable->AddressY = TA_Clamp;
			ChipTable->NeverStream = true;
			// The first lay, for an eye over the field's middle, in the picture as it is made.
			Chip->Lay(Day.Clouds, AirSky::Vec(MidX, MidY, GroundZ(MidX, MidY) + 2.0), Drift, Day.Sun, Day.Light);
			{
				FTexture2DMipMap& Mip = ChipTable->GetPlatformData()->Mips[0];
				const int64 Bytes = static_cast<int64>(Wide) * High * 4 * static_cast<int64>(sizeof(float));
				Mip.BulkData.Lock(LOCK_READ_WRITE);
				void* Out = Mip.BulkData.Realloc(Bytes);
				FMemory::Memzero(Out, Bytes);
				FMemory::Memcpy(Out, Chip->Table().data(), FMath::Min<int64>(Bytes, static_cast<int64>(Chip->Table().size() * sizeof(float))));
				Mip.BulkData.Unlock();
			}
			ChipTable->UpdateResource();
			UMaterialInstanceDynamic* TurnedInk = UMaterialInstanceDynamic::Create(TurnedMaterial, this);
			TurnedInk->SetTextureParameterValue(TEXT("Sheet"), Sheet);
			TurnedInk->SetTextureParameterValue(TEXT("Table"), ChipTable);
			TurnedInk->SetScalarParameterValue(TEXT("Brightness"), CloudBright);
			TurnedInk->SetScalarParameterValue(TEXT("UnitsPerMetre"), static_cast<float>(UnitsPerMetre));
			// The mesh: every slot's points with the painter's triangles, each point's place round its puff and its slot's
			// texel in the table. The chip places every point, so they are made at the mesh's own middle (two a centimetre
			// off, to give it a size), and its bounds are stretched over every cloud it can draw (200 km round, a little
			// past the drawn 20 km and the wind's carry over a long battle): it is never left out of the picture, and if
			// the material ever fails the points stay where they are made, where nothing is seen.
			std::vector<float> RingPlaces, SlotPlaces;
			Chip->PointPlaces(RingPlaces, SlotPlaces);
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
			const std::vector<int> Made = Chip->Triangles();
			TArray<int32> Triangles;
			Triangles.Reserve(static_cast<int32>(Made.size()));
			for (const int Index : Made)
			{
				Triangles.Add(Index);
			}
			TArray<FVector> Normals;
			Normals.Init(FVector::UpVector, Count);
			UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, TEXT("LandCloudsTurned"));
			Mesh->bUseAsyncCooking = false;
			Mesh->SetupAttachment(Root);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false);
			Mesh->SetTranslucentSortPriority(-20);
			Mesh->SetBoundsScale(2.0e7f);
			Mesh->RegisterComponent();
			const FVector Carried = WorldWay(Drift.X, Drift.Y, 0.0) * UnitsPerMetre;
			Mesh->SetRelativeLocation(Carried);
			Mesh->CreateMeshSection(0, Points, Triangles, Normals, Rings, Slots, TArray<FVector2D>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
			Mesh->SetMaterial(0, TurnedInk);
			Day.Turned = Mesh;
			Day.Table = ChipTable;
			Day.DriftShown = Carried;
			Day.Lays = 1;
			Day.LayBytes = static_cast<double>(Chip->RowsUsed()) * Wide * 16.0;
			UE_LOG(LogTheater, Display, TEXT("Land battle look: the sky, %s from %.0f to %.0f m, %d clouds of %d puffs, %.0f%% of the sky, drawn as the sheet's shapes, turned to the eye on the graphics chip (M_CloudTurned: %d puffs' room in one mesh made once, %d laid, %d left out; -TheaterSkyChip=0 turns them on the game's thread)."),
				Day.bDeck ? TEXT("a deck") : TEXT("scattered cumulus"), Day.Clouds.Base, Day.Clouds.Top, static_cast<int32>(Day.Clouds.Clouds.size()),
				static_cast<int32>(Day.Clouds.Puffs.size()), Day.Clouds.Cover * 100.0, Chip->Room(), Chip->Laid(), Chip->Dropped());
			Day.Chip = MoveTemp(Chip);
			return;
		}
		UE_LOG(LogTheater, Warning, TEXT("Land battle look: the clouds' table could not be made: the clouds are turned on the game's thread, as before."));
	}

	// The first turn, to an eye over the field's middle; DrawSky turns them as the battle's eye moves.
	Day.Rooms.PaintBoth(Day.Clouds, AirSky::Vec(MidX, MidY, GroundZ(MidX, MidY) + 2.0), Drift, Day.Sun, Day.Light);
	const double U = UnitsPerMetre;
	const FVector Carried = WorldWay(Drift.X, Drift.Y, 0.0) * U;
	auto Make = [this, &Day, Carried, U, CloudInk](const TCHAR* Name, int32 Order, const AirSky::Painter& Brush, int32 Room, const std::vector<AirSky::Vec>& Laid,
		const std::vector<uint8_t>& LaidRgba, const std::vector<float>& LaidUvs, TArray<FVector>& Points, TArray<FColor>& Colours, TArray<FVector2D>& Places, TArray<FVector2D>& Ways)
	{
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, Name);
		Mesh->bUseAsyncCooking = false;
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetTranslucentSortPriority(Order);
		Mesh->RegisterComponent();
		const std::vector<int> Made = Brush.Triangles(Room);
		TArray<int32> Triangles;
		Triangles.Reserve(static_cast<int32>(Made.size()));
		for (const int Index : Made)
		{
			Triangles.Add(Index);
		}
		const int32 Count = static_cast<int32>(Laid.size());
		Points.SetNum(Count);
		Colours.SetNum(Count);
		Places.SetNum(Count);
		Ways.SetNum(Count);
		LandLook::FillRoom(Laid, LaidRgba, Day.bShapes ? &LaidUvs : nullptr, U, Points, Colours, &Places, &Ways);
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Count);
		Mesh->SetRelativeLocation(Carried);
		if (Day.bShapes)
		{
			Mesh->CreateMeshSection(0, Points, Triangles, Normals, Places, Ways, TArray<FVector2D>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		}
		else
		{
			Mesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		}
		Mesh->SetMaterial(0, CloudInk);
		return Mesh;
	};
	AirSky::Rooms& Two = Day.Rooms;
	Day.Far = Make(TEXT("LandClouds"), -20, Two.Far, Two.Far.RoomFor(Day.Clouds), Two.FarPoints, Two.FarRgba, Two.FarUvs, Day.FarPoints, Day.FarColours, Day.FarPlaces, Day.FarWays);
	Day.Near = Make(TEXT("LandNearClouds"), -15, Two.Near, Two.NearRoom(), Two.NearPoints, Two.NearRgba, Two.NearUvs, Day.NearPoints, Day.NearColours, Day.NearPlaces, Day.NearWays);
	Day.DriftShown = Carried;
	UE_LOG(LogTheater, Display, TEXT("Land battle look: the sky, %s from %.0f to %.0f m, %d clouds of %d puffs, %.0f%% of the sky, drawn as %s."),
		Day.bDeck ? TEXT("a deck") : TEXT("scattered cumulus"), Day.Clouds.Base, Day.Clouds.Top, static_cast<int32>(Day.Clouds.Clouds.size()),
		static_cast<int32>(Day.Clouds.Puffs.size()), Day.Clouds.Cover * 100.0, Day.bShapes ? TEXT("the sheet's shapes") : TEXT("soft discs"));
}

void ALandBattle::DrawSky()
{
	LAND_SCOPE(Sky);
	// The clouds carried by the wind on the battle's clock, and turned to the eye as far as it has moved (the near room
	// as often as its puffs' turn shows, the far room when a far puff's would: AirSky::Rooms). On this thread: from the
	// ground the nearest puff is 900 m off, so a near turn comes every few metres the eye moves, about a millisecond.
	// (Called before this frame's camera is aimed: the turn is the last frame's eye, a fraction of a degree.)
	if (!SkyDay.IsValid() || !State.IsValid() || !Eye)
	{
		return;
	}
	FLandSky& Day = *SkyDay;
	if (Day.Chip.IsValid())
	{
		// GS6: turned on the graphics chip; here only the wind's carry and, when the puffs' order has gone stale, the
		// table laid again and sent (a few hundred kilobytes, no points).
		UProceduralMeshComponent* TurnedMesh = Day.Turned.Get();
		UTexture2D* ChipTable = Day.Table.Get();
		if (!TurnedMesh || !ChipTable)
		{
			return;
		}
		const AirSky::Vec Drift = Day.Clouds.DriftAt(State->Battle);
		const FVector Carried = WorldWay(Drift.X, Drift.Y, 0.0) * UnitsPerMetre;
		if (!Carried.Equals(Day.DriftShown, 1.0))
		{
			Day.DriftShown = Carried;
			TurnedMesh->SetRelativeLocation(Carried);
		}
		const FVector At = Eye->GetActorLocation() / UnitsPerMetre;
		const AirSky::Vec Seen(At.Y, At.X, At.Z);
		AirSky::Chip& Chip = *Day.Chip;
		if (Chip.Wants(Seen, Drift))
		{
			const double Began = LandParts::Seconds();
			Chip.Lay(Day.Clouds, Seen, Drift, Day.Sun, Day.Light);
			LandLook::SendTable(ChipTable, Chip.Table(), Chip.RowsUsed());
			++Day.Lays;
			Day.LayBytes += static_cast<double>(FMath::Min(Chip.RowsUsed(), ChipTable->GetSizeY())) * AirSky::Chip::Wide * 16.0;
			Day.LaySeconds += LandParts::Seconds() - Began;
			if (Chip.Dropped() > 0 && !Day.bDroppedSaid)
			{
				Day.bDroppedSaid = true;
				UE_LOG(LogTheater, Warning, TEXT("Land battle sky: the chip's table had no room for %d of the farthest puffs (room %d; said once)."), Chip.Dropped(), Chip.Room());
			}
		}
		// (the sky's clock in the log every 30 s: how often the table was laid, what a lay cost this thread, what it sent)
		if (RealT - Day.TurnsSaidAt >= 30.0)
		{
			UE_LOG(LogTheater, Log, TEXT("Land battle sky, %.0f s, on the graphics chip: %d table lays, %.2f ms a lay, %.2f MB sent; %d puffs laid, the nearest %.0f m off, laid again past %.1f m"),
				RealT - Day.TurnsSaidAt, Day.Lays, Day.Lays > 0 ? Day.LaySeconds * 1000.0 / Day.Lays : 0.0, Day.LayBytes / 1.0e6, Chip.Laid(), Chip.Nearest(), Chip.Slack());
			Day.TurnsSaidAt = RealT;
			Day.Lays = 0;
			Day.LaySeconds = 0.0;
			Day.LayBytes = 0.0;
		}
		return;
	}
	UProceduralMeshComponent* FarMesh = Day.Far.Get();
	UProceduralMeshComponent* NearMesh = Day.Near.Get();
	if (!FarMesh || !NearMesh)
	{
		return;
	}
	const AirSky::Vec Drift = Day.Clouds.DriftAt(State->Battle);
	const FVector Carried = WorldWay(Drift.X, Drift.Y, 0.0) * UnitsPerMetre;
	if (!Carried.Equals(Day.DriftShown, 1.0))
	{
		Day.DriftShown = Carried;
		FarMesh->SetRelativeLocation(Carried);
		NearMesh->SetRelativeLocation(Carried);
	}
	const FVector At = Eye->GetActorLocation() / UnitsPerMetre;
	const AirSky::Vec Seen(At.Y, At.X, At.Z);
	const double Began = LandParts::Seconds();
	const AirSky::Rooms::Sent What = Day.Rooms.Turn(Day.Clouds, Seen, Drift, Day.Sun, Day.Light);
	if (What != AirSky::Rooms::Sent::None)
	{
		AirSky::Rooms& Two = Day.Rooms;
		LandLook::FillRoom(Two.NearPoints, Two.NearRgba, Day.bShapes ? &Two.NearUvs : nullptr, UnitsPerMetre, Day.NearPoints, Day.NearColours, &Day.NearPlaces, &Day.NearWays);
		if (Day.bShapes)
		{
			NearMesh->UpdateMeshSection(0, Day.NearPoints, TArray<FVector>(), Day.NearPlaces, Day.NearWays, TArray<FVector2D>(), TArray<FVector2D>(), Day.NearColours, TArray<FProcMeshTangent>());
		}
		else
		{
			NearMesh->UpdateMeshSection(0, Day.NearPoints, TArray<FVector>(), TArray<FVector2D>(), Day.NearColours, TArray<FProcMeshTangent>());
		}
		if (What == AirSky::Rooms::Sent::Both)
		{
			LandLook::FillRoom(Two.FarPoints, Two.FarRgba, Day.bShapes ? &Two.FarUvs : nullptr, UnitsPerMetre, Day.FarPoints, Day.FarColours, &Day.FarPlaces, &Day.FarWays);
			if (Day.bShapes)
			{
				FarMesh->UpdateMeshSection(0, Day.FarPoints, TArray<FVector>(), Day.FarPlaces, Day.FarWays, TArray<FVector2D>(), TArray<FVector2D>(), Day.FarColours, TArray<FProcMeshTangent>());
			}
			else
			{
				FarMesh->UpdateMeshSection(0, Day.FarPoints, TArray<FVector>(), TArray<FVector2D>(), Day.FarColours, TArray<FProcMeshTangent>());
			}
			++Day.BothTurns;
		}
		else
		{
			++Day.NearTurns;
		}
		Day.TurnSeconds += LandParts::Seconds() - Began;
	}
	// (the sky's clock in the log every 30 s: how often each room turned and what a turn cost this thread)
	if (RealT - Day.TurnsSaidAt >= 30.0)
	{
		const int32 Turns = FMath::Max(1, Day.NearTurns + Day.BothTurns);
		UE_LOG(LogTheater, Log, TEXT("Land battle sky, %.0f s: %d near turns, %d far; %.2f ms a turn"), RealT - Day.TurnsSaidAt, Day.NearTurns, Day.BothTurns, Day.TurnSeconds * 1000.0 / Turns);
		Day.TurnsSaidAt = RealT;
		Day.NearTurns = 0;
		Day.BothTurns = 0;
		Day.TurnSeconds = 0.0;
	}
}

void ALandBattle::KeepTheLook()
{
	LAND_SCOPE(Look);
	// The battle grade (note 1: the art director measured the stills as ungraded, saturation 0.26 to 0.46 and blue
	// under red by 20 to 70, against the air battle's graded 0.08 to 0.18 and +3 to +9). The grade is written into the
	// battle's eye when it is made (Open), with every one of its overrides, and the shallow focus (AimEye) writes only
	// the focus's own; the eye is the view target. So the picture is drawn through the eye, and the grade is on it,
	// unless something under the battle takes the view: it is put back here, every frame, and said once (and the same
	// grade lies over the whole world under it while the battle stands: HideTheTable). A line in the log, three seconds
	// in, says what the picture is drawn through and what its lens carries, for the next Mac run.
	if (!SkyDay.IsValid() || !Eye)
	{
		return;
	}
	FLandSky& Day = *SkyDay;
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!Controller)
	{
		return;
	}
	if (Controller->GetViewTarget() != Eye.Get())
	{
		if (!Day.bTargetSaid)
		{
			Day.bTargetSaid = true;
			const AActor* Was = Controller->GetViewTarget();
			UE_LOG(LogTheater, Warning, TEXT("Land battle look: the picture was being drawn through %s, not the battle's eye, so without its grade: the eye is put back (said once)."),
				Was ? *Was->GetName() : TEXT("nothing"));
		}
		Controller->SetViewTarget(Eye);
	}
	if (!Day.bLookSaid && RealT > 3.0)
	{
		Day.bLookSaid = true;
		const UCameraComponent* Lens = Eye->GetCameraComponent();
		const FPostProcessSettings& On = Lens->PostProcessSettings;
		const bool bGraded = On.bOverride_ColorContrast && On.bOverride_ColorSaturationShadows && On.bOverride_ColorSaturationMidtones && On.bOverride_ColorSaturationHighlights
			&& On.bOverride_ColorGain && On.bOverride_AutoExposureMethod && On.AutoExposureMethod == AEM_Manual;
		const AActor* Through = Controller->GetViewTarget();
		UE_LOG(LogTheater, Display, TEXT("Land battle look: the picture is drawn through %s (%s); its lens, at weight %.2f, %s: contrast %.2f, colour kept in the darks %.2f, the middle tones %.2f, the brights %.2f, red %.2f, blue %.2f, exposure %s %+.2f stops; shallow focus %s (its own overrides only); the grade over the world %s"),
			Through ? *Through->GetName() : TEXT("nothing"), Through == Eye.Get() ? TEXT("the battle's eye") : TEXT("NOT the battle's eye"), Lens->PostProcessBlendWeight,
			bGraded ? TEXT("carries the battle grade") : FParse::Param(FCommandLine::Get(), TEXT("TheaterNoBattleGrade")) ? TEXT("has no grade (-TheaterNoBattleGrade)") : TEXT("DOES NOT carry the battle grade"),
			On.ColorContrast.X, On.ColorSaturationShadows.X, On.ColorSaturationMidtones.X, On.ColorSaturationHighlights.X, On.ColorGain.X, On.ColorGain.Z,
			On.AutoExposureMethod == AEM_Manual ? TEXT("fixed at") : TEXT("left to adapt, bias"), On.AutoExposureBias,
			On.bOverride_DepthOfFieldFocalDistance ? TEXT("on") : TEXT("off"), Day.Grade.IsValid() ? TEXT("stands") : TEXT("is missing"));
	}
}

void ALandBattle::KeepTheTableHidden()
{
	LAND_SCOPE(TableHidden);
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
	KeepTheLook();
	DrawSky();
}

void ALandBattle::ShowTheTable()
{
	if (!bTableHidden)
	{
		return;
	}
	bTableHidden = false;
	if (SkyDay.IsValid())
	{
		if (APostProcessVolume* Over = SkyDay->Grade.Get())
		{
			Over->Destroy();
		}
		SkyDay->Grade.Reset();
		for (UProceduralMeshComponent* Mesh : { SkyDay->Far.Get(), SkyDay->Near.Get(), SkyDay->Turned.Get() })
		{
			if (Mesh)
			{
				Mesh->SetVisibility(false);
			}
		}
	}
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
			if (!FMath::IsNearlyEqual(Light->Temperature, Kept.Temperature, 1.0f))
			{
				Light->SetTemperature(Kept.Temperature);
			}
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
			if (Sky->bLowerHemisphereIsBlack != Kept.bLowerBlack || !Sky->LowerHemisphereColor.Equals(Kept.Lower))
			{
				Sky->bLowerHemisphereIsBlack = Kept.bLowerBlack;
				Sky->LowerHemisphereColor = Kept.Lower;
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
		// (its held update run once, with the pieces back, before it is shown: nothing on it stale)
		Map->SetPutAway(false);
		Map->HoldUpdates(false);
		Map->SetActorHiddenInGame(false);
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

void ALandBattle::MakeFlags()
{
	// The bar's two ends carry the two nations' flags, the game's own (Brian, 14:58Z: flags, never names).
	const FLandBattleState& S = *State;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		TArray<FLinearColor> Pixels;
		const FString Tag = LandParts::Wide(S.Rec.Sides[Side].Nat);
		CampaignFlags::Paint(Tag, Side == 0 ? FLinearColor(0.35f, 0.4f, 0.3f) : FLinearColor(0.4f, 0.4f, 0.42f), Pixels);
		UTexture2D* Made = CampaignFlags::MakeTexture(Pixels, MakeUniqueObjectName(this, UTexture2D::StaticClass(), *FString::Printf(TEXT("LandFlag_%s"), *Tag)));
		(Side == 0 ? FlagAttack : FlagDefence) = Made;
	}
}

UTexture2D* ALandBattle::FlagOf(int32 Side) const
{
	return Side == 0 ? FlagAttack.Get() : FlagDefence.Get();
}

// ---- Each frame ------------------------------------------------------------------------------------------------

void ALandBattle::Tick(float DeltaSeconds)
{
	LAND_SCOPE(Tick);
	Super::Tick(DeltaSeconds);
	if (!PendingReplay.IsEmpty())
	{
		if (GetWorld() && GetWorld()->GetTimeSeconds() > 2.0)
		{
			const FString Asked = PendingReplay;
			PendingReplay.Empty();
			BeginReplay(Asked);
		}
		return;
	}
	if (TickLiveOpening(DeltaSeconds))
	{
		return;
	}
	KeepTheTableHidden();
	const double Real = FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, 0.1);
	RealT += Real;
	if (QuitAt >= 0.0 && RealT >= QuitAt)
	{
		QuitAt = -1.0;
		UE_LOG(LogTheater, Display, TEXT("Land battle: %d pictures taken; quitting."), ShotsTaken);
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
	// The battle's clock waits, the first time the game is run on a machine, for the materials' shaders: while one
	// compiles Unreal draws its stand-in grey, and the second cut's opening picture was of grey fields (the builder,
	// 21 November). A packaged game has none to wait for. While it waits the screen says so in the middle note (the
	// pre-check, 21 November: up to four minutes of a still picture, and only the log said why).
	if (!bShadersReady)
	{
		const int32 Remaining = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
		if (Remaining > 0 && ShaderWait < 240.0)
		{
			if (FMath::FloorToInt32(ShaderWait / 5.0) != FMath::FloorToInt32((ShaderWait + Real) / 5.0))
			{
				UE_LOG(LogTheater, Display, TEXT("Land battle: waiting for the materials, %d shader jobs left after %.0f s"), Remaining, ShaderWait + Real);
			}
			ShaderWait += Real;
			State->Note = "GETTING THE BATTLEFIELD READY, FIRST RUN ONLY (" + std::to_string(Remaining) + " LEFT)";
			State->NoteAge = -1.0;
			return;
		}
		bShadersReady = true;
		if (ShaderWait > 0.0)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: the materials ready after %.1f s (%d shader jobs left)"), ShaderWait, Remaining);
			State->Note.clear();
			State->NoteAge = -1.0;
		}
	}
	FLandBattleState& S = *State;
	const double FrameBegan = LandParts::Seconds();
	// The night before the attack (LandNight): while its shots play, the battle's clock waits at its start; the men and
	// the tanks are posed at the night's own time, the camera is the night's, and no rules, words or fire run. Its light
	// rises over the battle's opening after (Dark and Black, on the lens: NightLook).
	if (!S.Night.Done())
	{
		S.NightNow = S.Night.Update(Real);
	}
	else if (S.NightNow.bNight || S.NightNow.Dark > 0.0 || S.NightNow.Black > 0.0 || !S.NightNow.Flares.empty())
	{
		S.NightNow = LandNight::Frame();
	}
	const bool bNight = S.NightNow.bNight;
	const double Shown = bNight ? S.NightNow.Battle : S.Battle;

	// The clock, against life: four times life until the armies are about to meet (the approach, as the first two cuts
	// played it), then life; held while the after card is up. The battle's own clock runs Life times faster than life,
	// so the fight is played at 1 / Life of it (Brian, 21 November: "the tanks should be moving", as tanks do).
	// (a live battle: first what the rules have fought since the last frame; its contact unknown until it is made)
	if (Live.IsValid())
	{
		LandFell::Phase("tick: the rules' pieces taken");
		LAND_SCOPE(PumpLive);
		PumpLive();
	}
	if (bLiveBroken && !S.Rec.bOver && LiveBrokenAt >= 0.0 && RealT - LiveBrokenAt > 6.0)
	{
		// (release 1.5: the rules stopped mid-fight and the note has been up six seconds; the scene is destroyed here,
		// so nothing more of this frame is run. A battle whose end had already come keeps its after card and CONTINUE.)
		LiveBrokenAt = -1.0;
		LiveWayOut(TEXT("its rules stopped mid-fight"));
		return;
	}
	LandFell::Phase("tick: the clock");
	const double Contact = S.Rec.Contact >= 0.0 ? S.Rec.Contact : Live.IsValid() && !S.Rec.bOver ? 1e9 : 0.0;
	if (S.bOwnSpeed)
	{
		S.Speed = S.Battle < Contact - 10.0 ? 2.0 * S.Life : 1.0;
	}
	S.SpeedNow = S.bCard || bNight ? 0.0 : S.Speed;
	const double From = S.Battle;
	S.Battle = FMath::Min(S.Rec.End + 8.0, S.Battle + Real * S.SpeedNow / S.Life);
	if (Live.IsValid())
	{
		S.Battle = LiveClock(From, S.Battle, Real);
	}
	S.Lived += Real * S.SpeedNow;
	S.Real += bNight ? 0.0 : Real;

	// The cores, in order: the tanks and the men posed first (their muzzles and hulls are where the rounds start and
	// strike), then the fire, then the camera, then the words.
	double Mark = LandParts::Seconds();
	LandFell::Phase("tick: the tanks posed");
	if (S.Armour)
	{
		LAND_SCOPE(Armour);
		S.Armour->Update(Shown);
	}
	ArmourMs += (LandParts::Seconds() - Mark) * 1000.0;
	Mark = LandParts::Seconds();
	LandFell::Phase("tick: the men posed");
	if (S.Men)
	{
		LAND_SCOPE(Men);
		// (the men's detail by how big the lens draws them, the usual 50 deg lens as one: a long lens's far men drawn as
		// the near ones they look; the battle-scale plan's GS3)
		const double Zoom = FMath::Tan(FMath::DegreesToRadians(25.0)) / FMath::Max(0.02, FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(S.Pose.Fov, 2.0, 120.0) * 0.5)));
		S.Men->SetEye(S.Pose.Eye.X, S.Pose.Eye.Y, S.Pose.Eye.Z, Zoom);
		// (the tank commanders on their tanks as just posed, and the men in the lorries)
		RideTanks();
		RideLorries();
		S.Men->Update(Shown);
	}
	MenMs += (LandParts::Seconds() - Mark) * 1000.0;
	Mark = LandParts::Seconds();
	S.Fx.clear();
	S.Noises.clear();
	S.Losses.clear();
	S.Takes.clear();
	LandFell::Phase("tick: the fire stepped");
	if (!bNight)
	{
		LAND_SCOPE(Rules);
		S.Play.Step(From, S.Battle, S.Fx, S.Noises, S.Losses);
	}
	if (bShotHeld)
	{
		S.Eye.Hold(true);
	}
	LandFell::Phase("tick: the movers placed");
	LandParts::PlaceMovers(S);
	LandFell::Phase("tick: the film's eye");
	{
		LAND_SCOPE(Camera);
		// (the camera's own ms, for its 10 s line)
		const double EyeMark = LandParts::Seconds();
		const long StepsWere = S.Eye.Steps();
		S.Pose = bNight ? S.NightNow.Pose : S.Eye.Update(S.Battle, S.Real, Real);
		const double EyeFrameMs = (LandParts::Seconds() - EyeMark) * 1000.0;
		EyeMs += EyeFrameMs;
		EyeMostMs = FMath::Max(EyeMostMs, EyeFrameMs);
		EyeStepsMost = FMath::Max(EyeStepsMost, static_cast<int64>(S.Eye.Steps() - StepsWere));
		EyeCuts += S.Pose.bCut ? 1 : 0;
		// (the gap since the last frame began, against how many frames since the last cut)
		const double GapMs = Real * 1000.0;
		if (GapMs > LongestGapMs)
		{
			LongestGapMs = GapMs;
			LongestGapSinceCut = S.Pose.bCut ? 0 : FMath::Min(FramesSinceCut + 1, 1000);
		}
		FramesSinceCut = S.Pose.bCut ? 0 : FMath::Min(FramesSinceCut + 1, 1000);
	}
	if (Live.IsValid())
	{
		LandFell::Phase("tick: the general's view");
		LAND_SCOPE(GeneralView);
		const double GenMark = LandParts::Seconds();
		GeneralView(Real);
		const double GenFrameMs = (LandParts::Seconds() - GenMark) * 1000.0;
		GenEyeMs += GenFrameMs;
		GenEyeMostMs = FMath::Max(GenEyeMostMs, GenFrameMs);
		GeneralViewMs += GenFrameMs;
	}
	WalkPan(Real);
	LorryPan(Real);
	ReadDark(Real);
	LandFell::Phase("tick: the dispatches");
	if (!bNight)
	{
		LAND_SCOPE(Desk);
		S.Words = S.Desk.Update(S.Battle, Real);
	}
	NightLook();
	CoresMs += (LandParts::Seconds() - Mark) * 1000.0;

	// The picture: the camera, the ground round it, the men and the tanks.
	LandFell::Phase("tick: the lens aimed");
	AimEye(static_cast<float>(Real));
	if (Live.IsValid())
	{
		LandFell::Phase("tick: the general's screen");
		const double ScreenBegan = LandParts::Seconds();
		LAND_SCOPE(ShowGeneral);
		ShowGeneral();
		GeneralScreenMs += (LandParts::Seconds() - ScreenBegan) * 1000.0;
		LandFell::Phase("tick: the walk", ScriptStep);
		TickScript();
	}
	LandFell::Phase("tick: a cut told");
	// A cut is told to the renderer, so this frame starts afresh: the last shot's smoke, fog, light and sharpening are
	// not carried over a frame into the new one (the film check's smoke popping in a frame after a cut).
	if (S.Pose.bCut)
	{
		if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
			}
		}
	}
	const FVector EyeWorld = Eye ? Eye->GetActorLocation() : World(S.Pose.Eye.X, S.Pose.Eye.Y, S.Pose.Eye.Z);
	Mark = LandParts::Seconds();
	LandFell::Phase("tick: the ground shown");
	ShowField(EyeWorld, S.Battle);
	FieldMs += (LandParts::Seconds() - Mark) * 1000.0;
	LandFell::Phase("tick: the effects played");
	// What happens first: the frame's fire (the shells, guns and smoke rounds, told the kit before its frame begins, so
	// they are drawn this frame), then the shell holes and the tracks dug and settled (the field's: their time is counted
	// in FieldMs).
	PlayEffects();
	Mark = LandParts::Seconds();
	LandFell::Phase("tick: the breaches");
	MakeBreaches();
	LandFell::Phase("tick: the craters");
	MakeCraters();
	LandFell::Phase("tick: the wires");
	MakeWires();
	LandFell::Phase("tick: the stretchers");
	MakeStretchers();
	MakeReels();
	LandFell::Phase("tick: the ruts");
	MakeRuts();
	SettleTorn();
	FieldMs += (LandParts::Seconds() - Mark) * 1000.0;

	// The effects kit's frame in two halves (the production lead's FX2): TickBegin starts the kit's drawing on a worker
	// and gives the hand-held eye its jolt at once; the men and the tanks are drawn meanwhile; TickEnd waits for the
	// worker and sends the smoke. In between, the kit asks the ground under a point from the worker (the field's At,
	// read-only: nothing here changes it), and what the scene asks of the kit (a tank's dust, a cook-off, a fire) is
	// held and done at TickEnd, first drawn the next frame. EffectsMs is the two halves' own time on this thread.
	bool bEffectsBegun = false;
	LandFell::Phase("tick: the effects kit begun");
	if (Effects && Eye)
	{
		LAND_SCOPE(EffectsBegin);
		int32 ViewW = 1600, ViewH = 900;
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D Size;
			GEngine->GameViewport->GetViewportSize(Size);
			ViewW = FMath::Max(1, static_cast<int32>(Size.X));
			ViewH = FMath::Max(1, static_cast<int32>(Size.Y));
		}
		Mark = LandParts::Seconds();
		const FBattleJolt Jolt = Effects->TickBegin(S.Lived, Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW, FLinearColor::White, ViewH);
		EffectsMs += (LandParts::Seconds() - Mark) * 1000.0;
		Eye->SetActorLocationAndRotation(Eye->GetActorLocation() + Jolt.Offset, Eye->GetActorRotation() + FRotator(0.0, 0.0, Jolt.RollDegrees));
		bEffectsBegun = true;
	}
	Mark = LandParts::Seconds();
	const int32 InPictureWas = MenInPicture;
	LandFell::Phase("tick: the men drawn");
	DrawMen(EyeWorld);
	DrawMenMs += (LandParts::Seconds() - Mark) * 1000.0;
	MostInPicture = FMath::Max(MostInPicture, MenInPicture - InPictureWas);
	Mark = LandParts::Seconds();
	LandFell::Phase("tick: the tanks drawn");
	DrawArmour(EyeWorld);
	DrawArmourMs += (LandParts::Seconds() - Mark) * 1000.0;
	LandFell::Phase("tick: the men's shades");
	DrawManShades();
	// (the pool's players stopped since the last frame free their voices before this frame's first take, the tanks'
	// own beats below, as the cloud test frees them; once a frame)
	FreeVoices();
	// (the tanks' own beats and the men's: cook-offs, wreck fires, falls)
	LandFell::Phase("tick: the tanks' beats");
	PlayArmourEvents();
	LandFell::Phase("tick: the men's beats");
	PlayMenEvents();
	if (bEffectsBegun)
	{
		LAND_SCOPE(EffectsEnd);
		Mark = LandParts::Seconds();
		LandFell::Phase("tick: the effects kit ended");
		Effects->TickEnd();
		EffectsMs += (LandParts::Seconds() - Mark) * 1000.0;
		// (the smoke's puffs drawn and the room asked, the most in the log's 10 s)
		if (Effects->PuffsDrawn() >= PuffsMost)
		{
			PuffsMost = Effects->PuffsDrawn();
			PuffsMostAsked = Effects->PuffsAsked();
		}
	}
	Mark = LandParts::Seconds();
	LandFell::Phase("tick: the sound");
	PlaySound(Real);
	SoundMs += (LandParts::Seconds() - Mark) * 1000.0;
	LandFell::Phase("tick: the end and the notes");

	// The battle's end: the last words shown, then the after card a few seconds on.
	if (S.Words.bOver && S.Battle >= S.Rec.End)
	{
		if (!S.bOver)
		{
			S.bOver = true;
			S.OverReal = 0.0;
		}
		S.OverReal += Real;
		if (S.OverReal > 4.0)
		{
			S.bCard = true;
		}
	}
	if (S.NoteAge >= 0.0)
	{
		S.NoteAge += Real;
		if (S.NoteAge > 2.5)
		{
			S.NoteAge = -1.0;
			S.Note.clear();
		}
	}
	LandFell::Phase("tick: the pictures taken");
	TakeShots();
	LandFell::Phase("tick: the 10 s log");

	// The log, every 10 s: the frames, the slowest, and what each part cost.
	const double FrameMs = (LandParts::Seconds() - FrameBegan) * 1000.0;
	SlowestFrame = FMath::Max(SlowestFrame, FrameMs);
	// (the card's own time for its last whole frame: what an A/B of the field's or the men's draw moves)
	GpuMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	++LogFrames;
	if (RealT - LogAt >= 10.0)
	{
		LAND_SCOPE(Log10);
		const double Span = FMath::Max(0.001, RealT - LogAt);
		UE_LOG(LogTheater, Display, TEXT("Land battle: %.0f s of battle, %s shot; %.0f frames a second, the scene's slowest frame %.1f ms; a frame's cores %.2f ms, men %.2f, tanks %.2f, ground %.2f; shots heard %d, takes %d (dropped %d, on the screen %d), cracks %d, falls %d"),
			S.Battle, ANSI_TO_TCHAR(LandEye::NameOf(S.Pose.Shot)), LogFrames / Span, SlowestFrame, CoresMs / LogFrames, MenMs / LogFrames, ArmourMs / LogFrames, FieldMs / LogFrames,
			S.Mix.ShotsHeard, S.Mix.ShotTakes, S.Mix.ShotsDropped, S.Mix.ShotsOnScreenDropped, S.Mix.Cracks, S.Mix.Falls);
		// (the drawing's own parts, the battle-scale plan's speed log: what the Mac's frame goes on)
		// (and the card's frame, ms: the art director's A/B of -TheaterLandTreeFarFrom=80 against the default reads here;
		// SO1: men drawn as the crowd's instances, mid and far, a frame, and in the wide mass shot how many far men were
		// grown to MassMinPx of the picture's lines and the most, x times his size)
		// (GS7: the ground's clutter places the meshes hold now against the things laid in them; kept packed, the same)
		int32 ClutterPlaces = 0;
		for (const TObjectPtr<UInstancedStaticMeshComponent>& Mesh : ClutterCopies)
		{
			ClutterPlaces += Mesh ? Mesh->GetInstanceCount() : 0;
		}
		const int32 ClutterLaid = Carpet.IsValid() ? static_cast<int32>(Carpet->Laid()) : 0;
		UE_LOG(LogTheater, Display, TEXT("Land battle: a frame's drawing: men %.2f ms, tanks %.2f, effects %.2f, sound %.2f; the card %.2f ms; men in the picture %.0f a frame (the most %d), meshes made again %.1f a frame, held at the old level %.1f; the crowd (SO1) %s: mid %.0f a frame, far %.0f; the mass shot's least size %.0f lines: %.0f men grown a frame, the most x%.1f; the ground's clutter %d places for %d laid (GS7); the torn ground (GS8) %d sections sent, %d baked static, %d procedural now, the slowest frame's baking %.2f ms; the smoke's puffs (FX2) the most %d drawn, of %d asked"),
			DrawMenMs / LogFrames, DrawArmourMs / LogFrames, EffectsMs / LogFrames, SoundMs / LogFrames, GpuMs / LogFrames, static_cast<double>(MenInPicture) / LogFrames, MostInPicture,
			static_cast<double>(MenRebuilt) / LogFrames, static_cast<double>(RebuildsPut) / LogFrames, Vat.IsValid() ? TEXT("on") : TEXT("off"),
			static_cast<double>(VatMid) / LogFrames, static_cast<double>(VatFar) / LogFrames, MassMinPx, static_cast<double>(MassGrown) / LogFrames, MostGrow, ClutterPlaces, ClutterLaid,
			TornSent, TornBakedCount, TornHot.Num(), TornBakeMostMs, PuffsMost, PuffsMostAsked);
		// (the camera's line, CD0: what the camera's choice costs, and whether the span's longest gap between frames
		// came on a shot change: 0 or 1 frames after a cut, with the camera's cut frames cheap, is the renderer's start
		// afresh, not the choice)
		const FString GapWhen = LongestGapSinceCut >= 1000 ? FString(TEXT("no cut before it in the span")) : FString::Printf(TEXT("%d frames after a cut"), LongestGapSinceCut);
		UE_LOG(LogTheater, Display, TEXT("Land battle: the camera %.3f ms a frame, the slowest %.2f ms, the most ray steps in a frame %lld (%d cuts); the general's view %.3f ms a frame, the slowest %.2f; the longest gap between frames %.1f ms, %s"),
			EyeMs / LogFrames, EyeMostMs, static_cast<long long>(EyeStepsMost), EyeCuts, GenEyeMs / LogFrames, GenEyeMostMs, LongestGapMs, *GapWhen);
		EyeMs = EyeMostMs = GenEyeMs = GenEyeMostMs = LongestGapMs = 0.0;
		EyeStepsMost = 0;
		EyeCuts = 0;
		LongestGapSinceCut = 1000;
		// (a live battle: the general's own parts, and the rules' step on their thread with how far ahead of the picture
		// they have fought; the production lead's frame-time read, 22 November)
		if (Live.IsValid())
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle (live): a frame's general's view %.2f ms, his screen %.2f ms; the rules' last step %.0f ms, the slowest %.0f ms, fought to %.1f s, %.1f s ahead of the picture%s"),
				GeneralViewMs / LogFrames, GeneralScreenMs / LogFrames, Live->LastMs(), Live->MostMs(), Live->Fought(), Live->Fought() - S.Battle,
				S.Rec.bSetup ? TEXT(" (the setup step: the clock stands)") : TEXT(""));
		}
		GeneralViewMs = GeneralScreenMs = 0.0;
		LogAt = RealT;
		LogFrames = 0;
		SlowestFrame = CoresMs = MenMs = ArmourMs = FieldMs = 0.0;
		DrawMenMs = DrawArmourMs = EffectsMs = SoundMs = GpuMs = 0.0;
		MenInPicture = MenRebuilt = RebuildsPut = MostInPicture = 0;
		VatMid = VatFar = MassGrown = 0;
		MostGrow = 1.0;
		TornSent = TornBakedCount = 0;
		TornBakeMostMs = 0.0;
		PuffsMost = PuffsMostAsked = 0;
	}
	LandFell::Phase("tick: done");
}

void ALandBattle::WalkPan(double Real)
{
	LAND_SCOPE(WalkPan);
	if (WalkPanAt < 0.0 || !State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	if (S.Battle < WalkPanAt)
	{
		return;
	}
	const TCHAR* PanName = bTankPan ? TEXT("the tank pan (far tanks' motion vectors)") : TEXT("the walk pan (look 7)");
	if (!bWalkPanOn)
	{
		// (the block walking most steadily over the pan, or the tank driving most steadily, and the eye off its flank:
		// none, asked again 10 s on)
		S.Pan = bTankPan ? LandWalkPan::ChooseTank(S.Rec, S.EyeWorld, WalkPanAt, WalkPanFor) : LandWalkPan::Choose(S.Rec, S.EyeWorld, WalkPanAt, WalkPanFor);
		if (S.Pan.Block < 0)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: %s: no %s from %.0f s%s"), PanName, bTankPan ? TEXT("tank driving") : TEXT("block walking"), WalkPanAt,
				WalkPanTries > 0 ? TEXT("; asked again 10 s on") : TEXT("; none taken"));
			WalkPanAt = WalkPanTries-- > 0 ? WalkPanAt + 10.0 : -1.0;
			return;
		}
		LandPlay::Vec Mid;
		LandWalkPan::Middle(S.Rec, S.EyeWorld, S.Pan, S.Battle, Mid);
		WalkPanLook = FVector(Mid.X, Mid.Y, Mid.Z);
		bWalkPanOn = true;
		WalkPanStillAt = RealT + 0.5;
		WalkPanStills = 0;
		const double Up = S.EyeWorld.Height ? S.Pan.Eye.Z - S.EyeWorld.Height(S.Pan.Eye.X, S.Pan.Eye.Y) : S.Pan.Eye.Z;
		if (bTankPan)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: %s from %.0f s for %.0f s: the tank of track %d (block %d) driving %.1f m/s; the eye %.1f m up, %.0f m off her at the start, %.0f at the middle, %.0f at the end%s"),
				PanName, WalkPanAt, WalkPanFor, S.Pan.Men.empty() ? -1 : S.Pan.Men[0], S.Pan.Block, S.Pan.Speed, Up, S.Pan.FromStart, S.Pan.FromMiddle, S.Pan.FromEnd,
				S.Pan.bOpen ? TEXT("") : TEXT(" (no open line found: a bank or hedge may stand between)"));
		}
		else
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: %s from %.0f s for %.0f s: block %d, %d men walking %.1f m/s; the eye %.1f m up, %.0f m off their middle at the start, %.0f at the middle, %.0f at the end%s"),
				PanName, WalkPanAt, WalkPanFor, S.Pan.Block, static_cast<int32>(S.Pan.Men.size()), S.Pan.Speed, Up, S.Pan.FromStart, S.Pan.FromMiddle, S.Pan.FromEnd,
				S.Pan.bOpen ? TEXT("") : TEXT(" (no open line found: a bank or hedge may stand between)"));
		}
		S.Pose.bCut = true;
	}
	if (S.Battle >= WalkPanAt + WalkPanFor)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: %s done at %.0f s: %d stills"), PanName, S.Battle, WalkPanStills);
		WalkPanAt = -1.0;
		bWalkPanOn = false;
		// (the film's own shot again, laid round as on a cut)
		S.Pose.bCut = true;
		return;
	}
	// The eye still; its look turned with the block's middle, smoothed over a second and a half (a slow pan).
	LandPlay::Vec Mid;
	if (LandWalkPan::Middle(S.Rec, S.EyeWorld, S.Pan, S.Battle, Mid))
	{
		const double Ease = 1.0 - FMath::Exp(-FMath::Max(0.0, Real) / 1.5);
		WalkPanLook = FMath::Lerp(WalkPanLook, FVector(Mid.X, Mid.Y, Mid.Z), Ease);
	}
	S.Pose.Eye = S.Pan.Eye;
	S.Pose.Look = LandPlay::Vec(WalkPanLook.X, WalkPanLook.Y, WalkPanLook.Z);
	// (the tank pan on a long lens: a tank 190 m off some 40 m of the picture's width, about a sixth of it, so a smear
	// along her drive is plain to see)
	S.Pose.Fov = bTankPan ? 12.0 : 26.0;
	S.Pose.Roll = 0.0;
	// (sharp through: the low shot's stop, not the watch shot's shallow focus)
	S.Pose.Shot = LandEye::Kind::Low;
	S.Pose.Subject = -1;
	if (RealT >= WalkPanStillAt)
	{
		WalkPanStillAt = RealT + 1.0;
		const FString Folder = ShotFolder.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), bTankPan ? TEXT("LandTankPan") : TEXT("LandWalkPan")) : ShotFolder;
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, FString::Printf(TEXT("%s-%s-%02d.png"), *Key, bTankPan ? TEXT("tankpan") : TEXT("walkpan"), WalkPanStills)), true, false);
		++WalkPanStills;
	}
}

void ALandBattle::LorryPan(double Real)
{
	LAND_SCOPE(LorryPan);
	if (LorryPanAt < 0.0 || !State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	if (S.Battle < LorryPanAt || !S.Armour || !S.Men)
	{
		return;
	}
	std::vector<LandKit::Vehicle>& All = S.Armour->Vehicles();
	bool bSnap = false;
	if (LorryPanVehicle < 0)
	{
		// (the drawn lorry with the most men seated on her now, of the block asked for if one was, as RideLorries finds
		// a man's lorry: by her block and her track's k; none, asked again 5 s on)
		std::map<std::pair<int, int>, int> Aboard;
		const int Men = S.Men->Count();
		for (int Man = 0; Man < Men; ++Man)
		{
			int Block = -1, SeatOn = -1, Seat = -1;
			double Down = -1.0;
			if (!S.Men->IsRider(Man) && S.Men->AboardOf(Man, Block, SeatOn, Seat, Down) && Down < 0.0 && (LorryPanBlock < 0 || Block == LorryPanBlock))
			{
				++Aboard[{ Block, SeatOn }];
			}
		}
		int Best = -1, BestMen = 0;
		for (size_t V = 0; V < All.size(); ++V)
		{
			const LandKit::Vehicle& Her = All[V];
			if (Her.Words().Role != "lorry" || Her.Look().SocketIndex("rider.1") < 0 || Her.Fade < 0.5 || HiddenVehicle(static_cast<int32>(V)))
			{
				continue;
			}
			const auto Found = Aboard.find({ Her.Words().Block, Her.Words().K });
			if (Found != Aboard.end() && Found->second > BestMen)
			{
				Best = static_cast<int>(V);
				BestMen = Found->second;
			}
		}
		if (Best < 0)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle: the lorry pan: no drawn lorry with men seated on her at %.0f s%s"), S.Battle,
				LorryPanTries > 0 ? TEXT("; asked again 5 s on") : TEXT("; none taken"));
			LorryPanAt = LorryPanTries-- > 0 ? S.Battle + 5.0 : -1.0;
			return;
		}
		LorryPanVehicle = Best;
		LorryPanHeading = All[static_cast<size_t>(Best)].Heading;
		LorryPanStillAt = RealT + 0.5;
		LorryPanStills = 0;
		// (the side of her with the ground free for the eye, her right first)
		LorryPanSide = 1;
		if (S.EyeWorld.Free)
		{
			const LandKit::Vehicle& Her = All[static_cast<size_t>(Best)];
			const double Sn = std::sin(Her.Heading), Cs = std::cos(Her.Heading);
			const double Rx = Her.Root.At.X - Sn * 12.0 + Cs * 7.0, Ry = Her.Root.At.Y - Cs * 12.0 - Sn * 7.0;
			const double Rz = (S.EyeWorld.Height ? S.EyeWorld.Height(Rx, Ry) : Her.Root.At.Z) + 3.5;
			if (!S.EyeWorld.Free(LandPlay::Vec(Rx, Ry, Rz)))
			{
				LorryPanSide = -1;
			}
		}
		bSnap = true;
		UE_LOG(LogTheater, Display, TEXT("Land battle: the lorry pan from %.0f s for %.0f s: the lorry of block %d, k %d (vehicle %d), %d men seated on her, at %.0f, %.0f; the eye off her %s rear quarter"),
			S.Battle, LorryPanFor, All[static_cast<size_t>(Best)].Words().Block, All[static_cast<size_t>(Best)].Words().K, Best, BestMen,
			All[static_cast<size_t>(Best)].Root.At.X, All[static_cast<size_t>(Best)].Root.At.Y, LorryPanSide > 0 ? TEXT("right") : TEXT("left"));
		// (its length from the moment she is found)
		LorryPanAt = S.Battle;
	}
	if (S.Battle >= LorryPanAt + LorryPanFor || static_cast<size_t>(LorryPanVehicle) >= All.size())
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: the lorry pan done at %.0f s: %d stills"), S.Battle, LorryPanStills);
		LorryPanAt = -1.0;
		LorryPanVehicle = -1;
		// (the film's own shot again, laid round as on a cut)
		S.Pose.bCut = true;
		return;
	}
	const LandKit::Vehicle& Her = All[static_cast<size_t>(LorryPanVehicle)];
	// Her heading eased over about a second (a lorry turning on a lane does not swing the eye round at once); the eye
	// off her rear quarter, looking into her bed a metre behind her middle, at a man's chest over her floor.
	const double Turn = FMath::UnwindRadians(Her.Heading - LorryPanHeading);
	LorryPanHeading += bSnap ? Turn : Turn * (1.0 - FMath::Exp(-FMath::Max(0.0, Real) / 1.0));
	const double Sn = std::sin(LorryPanHeading), Cs = std::cos(LorryPanHeading);
	const double Aside = 7.0 * static_cast<double>(LorryPanSide);
	const double Ex = Her.Root.At.X - Sn * 12.0 + Cs * Aside, Ey = Her.Root.At.Y - Cs * 12.0 - Sn * Aside;
	const double Ground = S.EyeWorld.Height ? S.EyeWorld.Height(Ex, Ey) : Her.Root.At.Z;
	S.Pose.Eye = LandPlay::Vec(Ex, Ey, FMath::Max(Ground, Her.Root.At.Z) + 3.5);
	S.Pose.Look = LandPlay::Vec(Her.Root.At.X - Sn * 1.0, Her.Root.At.Y - Cs * 1.0, Her.Root.At.Z + 1.6);
	S.Pose.Fov = 40.0;
	S.Pose.Roll = 0.0;
	// (sharp through: the low shot's stop, not the watch shot's shallow focus)
	S.Pose.Shot = LandEye::Kind::Low;
	S.Pose.Subject = -1;
	S.Pose.Focus = 0.0;
	S.Pose.bCut = bSnap;
	if (RealT >= LorryPanStillAt)
	{
		LorryPanStillAt = RealT + 1.0;
		const FString Folder = ShotFolder.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LandLorryPan")) : ShotFolder;
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, FString::Printf(TEXT("%s-lorrypan-%02d.png"), *Key, LorryPanStills)), true, false);
		UE_LOG(LogTheater, Display, TEXT("Land battle: the lorry pan's still %02d at %.1f s"), LorryPanStills, S.Battle);
		++LorryPanStills;
	}
}

void ALandBattle::StartDarkRead()
{
	StopDarkRead();
	DarkShare = -1.0;
	DarkReads = 0;
	DarkAsks = 0;
	DarkBacks = 0;
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoDarkRead")))
	{
		return;
	}
	// (the game's own viewport: a packaged game or -game; none in the editor's play, where the rule is not asked)
	UGameEngine* Game = Cast<UGameEngine>(GEngine);
	if (!Game || !Game->SceneViewport.IsValid())
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: the dark reading off (no game viewport to read): the camera's dark rule not asked"));
		return;
	}
	DarkGrab = MakeShared<FFrameGrabber>(Game->SceneViewport.ToSharedRef(), FIntPoint(64, 36), PF_B8G8R8A8, 3);
	DarkGrab->StartCapturingFrames();
	DarkClock = 0.0;
	DarkAskAt = 0.0;
	DarkGotAt = 0.0;
	UE_LOG(LogTheater, Display, TEXT("Land battle: the dark reading on: the drawn frame at 64 x 36 four times a second, its share under L 25 for the camera's dark rule"));
}

void ALandBattle::ReadDark(double Real)
{
	LAND_SCOPE(ReadDark);
	if (!DarkGrab.IsValid())
	{
		return;
	}
	// (the reading's own clock, from this frame's length. Until 25 November the asks and the staleness below were
	// measured against the frame's length itself, at most a tenth of a second: one frame was asked for as the battle
	// began, the next ask's time was never reached, and the camera's dark rule saw nothing all battle.)
	DarkClock += Real;
	const double Now = DarkClock;
	// Ask for a copy of the frame now and then; take what has come back (a frame or two later), the newest.
	if (Now >= DarkAskAt && DarkGrab->IsCapturingFrames())
	{
		DarkAskAt = Now + 0.25;
		DarkGrab->CaptureThisFrame(FFramePayloadPtr());
		++DarkAsks;
	}
	// (no frame read for six tenths of a second: the last reading is stale, so the rule is told there is none;
	// pre-check's note. Before the frames are taken, so that it holds on every way out below: a frame come back empty
	// too. And under the rule's own quarter second and half second together (LandEye: a shot's reading counts from
	// its first quarter second and must hold DarkHold), so that a reading of the last shot, the frames stopped, can
	// never by itself have the next one left or the run of dark cuts started again. It was a second, 25 November.)
	if (DarkShare >= 0.0 && Now - DarkGotAt > 0.6)
	{
		DarkShare = -1.0;
	}
	TArray<FCapturedFrameData> Frames = DarkGrab->GetCapturedFrames();
	if (Frames.Num() == 0)
	{
		return;
	}
	DarkBacks += Frames.Num();
	// (a result card up: the whole picture dimmed under it, so no reading till it goes; nor while the night's light is
	// on the picture, the night before the attack and its dawn; nor while the general's own view is the picture drawn
	// (his eye begun and the film not given the camera): the film goes on being directed behind it, for its corner
	// and for his return to it, and a dark reading of HIS view, a wood from above, would have the film leave shots
	// that are not dark at all; nor while a drag has the lens looking aside from the shot (AimEye; more than a degree
	// either way): what he turns the lens to, a hedge, a crown, an open field, is not the shot's picture, and the
	// film would leave the shot he is looking about in and bar its place, or take the field for the light come back.
	// Until the reading had a clock of its own, 25 November, nothing was read and none of this could show.
	// These are asked as the frame comes back, a frame or two after it was drawn: one frame already on its way at
	// a change is read as the new picture's. One reading cannot act alone: the rule wants half a second of dark to
	// leave a shot and half a second of light to start its run again.)
	if (FMath::Abs(LookAside.X) > 1.0 || FMath::Abs(LookAside.Y) > 1.0
		|| (State.IsValid() && (State->bCard || (State->bGeneralEye && !State->General.bFilm) || State->NightNow.Dark > 0.0 || State->NightNow.Black > 0.0)))
	{
		DarkShare = -1.0;
		return;
	}
	const FCapturedFrameData& Last = Frames.Last();
	const TArray<FColor>& Pixels = Last.ColorBuffer;
	const int32 Wd = Last.BufferSize.X, Ht = Last.BufferSize.Y;
	if (Pixels.Num() == 0 || Wd <= 0 || Ht <= 0 || Pixels.Num() < Wd * Ht)
	{
		return;
	}
	// The top bar's rows left out (the art director's 30% is read on the picture under the words, not the leather bar).
	const int32 Skip = Screen.IsValid() ? FMath::Clamp(FMath::CeilToInt32(Screen->TopShare() * Ht), 0, Ht - 1) : 0;
	// L 25 of 255: the picture's own brightness, as the art director reads a still (Rec. 709 weights on its sRGB values).
	int32 Dark = 0;
	for (int32 Y = Skip; Y < Ht; ++Y)
	{
		for (int32 X = 0; X < Wd; ++X)
		{
			const FColor& P = Pixels[Y * Wd + X];
			if (0.2126f * P.R + 0.7152f * P.G + 0.0722f * P.B < 25.0f)
			{
				++Dark;
			}
		}
	}
	DarkShare = static_cast<double>(Dark) / static_cast<double>((Ht - Skip) * Wd);
	// (only a frame that was read keeps the reading fresh: one that comes back empty leaves the last to go stale)
	DarkGotAt = Now;
	++DarkReads;
}

void ALandBattle::StopDarkRead()
{
	if (DarkGrab.IsValid())
	{
		DarkGrab->StopCapturingFrames();
		DarkGrab->Shutdown();
		DarkGrab.Reset();
	}
	DarkShare = -1.0;
}

void ALandBattle::StartNight()
{
	// The night before the attack: a recording's patrol (its men written in at the load), played before the battle's
	// own opening; recordings only (a live battle's night is the rules' to fight).
	if (!State.IsValid() || Live.IsValid() || !State->Rec.Night.bOn)
	{
		return;
	}
	FLandBattleState& S = *State;
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoNight")))
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: the night before the attack left out (-TheaterLandNoNight)"));
		return;
	}
	float Candela = 1.3e6f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandFlare="), Candela);
	FlareCandela = FMath::Clamp(static_cast<double>(Candela), 0.0, 1e8) * FMath::Square(UnitsPerMetre / 100.0); // (candelas at 100 units a metre)
	if (!S.Night.Begin(S.Rec, S.EyeWorld.Height, S.EyeWorld.Free, S.Battle))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: the night before the attack not played (no patrol men in the recording, or no route)"));
		return;
	}
	FString ShotList;
	for (const LandNight::Shot& Shot : S.Night.Shots())
	{
		ShotList += FString::Printf(TEXT("%s%s from %.0f s, %.1f real s"), ShotList.IsEmpty() ? TEXT("") : TEXT("; "), ANSI_TO_TCHAR(LandNight::NameOf(Shot.Kind)), Shot.From, Shot.Real);
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle: the night before the attack: %d of the patrol's men; %s; then the dawn over the battle (a key or a click skips to it); flares %.0f cd"),
		static_cast<int32>(S.Night.Men().size()), *ShotList, FlareCandela);
	S.NightNow = S.Night.Update(0.0);
}

void ALandBattle::NightLook()
{
	// The night's look on the eye's lens over the day's grade (the day's values kept the first time and given back once
	// the light has risen): the exposure 4.6 stops down, the colour drained to a third and cooled, as a film shoots day
	// for night; the picture's dip to black between the night and the battle on the lens's gain. Then the flares.
	if (!State.IsValid() || !Eye)
	{
		return;
	}
	const LandNight::Frame& F = State->NightNow;
	FPostProcessSettings& Lens = Eye->GetCameraComponent()->PostProcessSettings;
	const bool bWant = F.bNight || F.Dark > 0.0 || F.Black > 0.0;
	if (bWant && !NightLens.bKept)
	{
		NightLens.Stops = Lens.AutoExposureBias;
		NightLens.Darks = Lens.ColorSaturationShadows;
		NightLens.Middles = Lens.ColorSaturationMidtones;
		NightLens.Gain = Lens.ColorGain;
		NightLens.bKept = true;
	}
	if (bWant)
	{
		const double Dark = FMath::Clamp(F.Dark, 0.0, 1.0), Up = 1.0 - FMath::Clamp(F.Black, 0.0, 1.0);
		const double Drain = FMath::Lerp(1.0, 0.35, Dark);
		Lens.bOverride_AutoExposureBias = true;
		Lens.AutoExposureBias = static_cast<float>(NightLens.Stops - 4.6 * Dark);
		Lens.bOverride_ColorSaturationShadows = true;
		Lens.ColorSaturationShadows = FVector4(NightLens.Darks.X * Drain, NightLens.Darks.Y * Drain, NightLens.Darks.Z * Drain, NightLens.Darks.W);
		Lens.bOverride_ColorSaturationMidtones = true;
		Lens.ColorSaturationMidtones = FVector4(NightLens.Middles.X * Drain, NightLens.Middles.Y * Drain, NightLens.Middles.Z * Drain, NightLens.Middles.W);
		Lens.bOverride_ColorGain = true;
		Lens.ColorGain = FVector4(NightLens.Gain.X * FMath::Lerp(1.0, 0.8, Dark) * Up, NightLens.Gain.Y * FMath::Lerp(1.0, 0.92, Dark) * Up,
			NightLens.Gain.Z * FMath::Lerp(1.0, 1.2, Dark) * Up, NightLens.Gain.W);
		NightLens.bOn = true;
	}
	else if (NightLens.bOn)
	{
		Lens.AutoExposureBias = NightLens.Stops;
		Lens.ColorSaturationShadows = NightLens.Darks;
		Lens.ColorSaturationMidtones = NightLens.Middles;
		Lens.ColorGain = NightLens.Gain;
		NightLens.bOn = false;
		UE_LOG(LogTheater, Display, TEXT("Land battle: the night's light has risen: the day's grade back on the lens"));
	}
	// The flares: one lamp and one glow each, by the patrol's flare number; the rest put out.
	int32 Used = 0;
	for (const LandNight::Flare& Each : F.Flares)
	{
		if (Each.Light <= 0.0)
		{
			continue;
		}
		FlareAt(Used++, World(Each.At.X, Each.At.Y, Each.At.Z), Each.Light, Each.bRising);
	}
	for (int32 K = Used; K < FlareLamps.Num(); ++K)
	{
		if (UPointLightComponent* Lamp = FlareLamps[K].Get())
		{
			Lamp->SetVisibility(false);
		}
		if (K < FlareGlows.Num())
		{
			if (UProceduralMeshComponent* Glow = FlareGlows[K].Get())
			{
				Glow->SetVisibility(false);
			}
		}
	}
}

void ALandBattle::FlareAt(int32 Index, const FVector& At, double Light, bool bRising)
{
	// A flare's lamp (its swing throws the men's and the hedges' shadows about: the first two cast them) and its glow, a
	// small ball of warm white on M_Highlight, bright enough to burn through the night's exposure.
	while (FlareLamps.Num() <= Index)
	{
		UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
		Lamp->SetMobility(EComponentMobility::Movable);
		Lamp->SetupAttachment(Root);
		Lamp->SetUsingAbsoluteLocation(true);
		Lamp->IntensityUnits = ELightUnits::Candelas;
		Lamp->bUseInverseSquaredFalloff = true;
		Lamp->SetCastShadows(FlareLamps.Num() < 2);
		Lamp->SetLightColor(FLinearColor(1.0f, 0.95f, 0.8f));
		Lamp->SetAttenuationRadius(static_cast<float>(450.0 * UnitsPerMetre));
		Lamp->SetSourceRadius(static_cast<float>(0.3 * UnitsPerMetre));
		Lamp->SetIntensity(0.0f);
		Lamp->RegisterComponent();
		FlareLamps.Add(Lamp);
		UProceduralMeshComponent* Glow = NewObject<UProceduralMeshComponent>(this);
		Glow->bUseAsyncCooking = false;
		Glow->SetupAttachment(Root);
		Glow->SetUsingAbsoluteLocation(true);
		Glow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Glow->SetCastShadow(false);
		Glow->RegisterComponent();
		{
			// (a ball of a metre: an octahedron cut once, its points pushed out to the sphere)
			TArray<FVector> Points = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0), FVector(0, 0, 1), FVector(0, 0, -1) };
			const int32 Faces[8][3] = { { 0, 2, 4 }, { 2, 1, 4 }, { 1, 3, 4 }, { 3, 0, 4 }, { 2, 0, 5 }, { 1, 2, 5 }, { 3, 1, 5 }, { 0, 3, 5 } };
			TArray<int32> Triangles;
			for (const auto& Face : Faces)
			{
				const int32 A = Face[0], B = Face[1], C = Face[2];
				const int32 Ab = Points.Add((Points[A] + Points[B]).GetSafeNormal());
				const int32 Bc = Points.Add((Points[B] + Points[C]).GetSafeNormal());
				const int32 Ca = Points.Add((Points[C] + Points[A]).GetSafeNormal());
				Triangles.Append({ A, Ca, Ab, B, Ab, Bc, C, Bc, Ca, Ab, Ca, Bc });
			}
			// (each face both ways round: seen from outside whichever way the faces are wound)
			const int32 OneWay = Triangles.Num();
			for (int32 K = 0; K < OneWay; K += 3)
			{
				Triangles.Append({ Triangles[K], Triangles[K + 2], Triangles[K + 1] });
			}
			TArray<FVector> Scaled, Normals;
			TArray<FColor> Colours;
			for (const FVector& P : Points)
			{
				Scaled.Add(P * UnitsPerMetre);
				Normals.Add(P);
				Colours.Add(FColor(255, 244, 214, 255));
			}
			Glow->CreateMeshSection(0, Scaled, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		}
		if (!FlareInk.IsValid())
		{
			if (UMaterialInterface* Highlight = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight")))
			{
				FlareInk = UMaterialInstanceDynamic::Create(Highlight, this);
			}
		}
		if (FlareInk.IsValid())
		{
			Glow->SetMaterial(0, FlareInk.Get());
		}
		FlareGlows.Add(Glow);
	}
	const double Shine = FMath::Clamp(Light, 0.0, 1.5);
	if (UPointLightComponent* Lamp = FlareLamps[Index].Get())
	{
		Lamp->SetWorldLocation(At);
		Lamp->SetIntensity(static_cast<float>(FlareCandela * (bRising ? 0.0 : Shine)));
		Lamp->SetVisibility(!bRising && Shine > 0.0);
	}
	if (UProceduralMeshComponent* Glow = FlareGlows[Index].Get())
	{
		// (2.5 m across burning, a spark of 0.6 m climbing; the night's 4.6 stops given back, and a little over)
		Glow->SetWorldLocation(At);
		Glow->SetWorldScale3D(FVector(bRising ? 0.3 : 1.25));
		if (UMaterialInstanceDynamic* Ink = FlareInk.Get())
		{
			Ink->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(bRising ? 6.0 : 40.0 * Shine));
			Ink->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
		}
		Glow->SetVisibility(Shine > 0.0);
	}
}

void ALandBattle::AimEye(float DeltaSeconds)
{
	LAND_SCOPE(AimEye);
	FLandBattleState& S = *State;
	if (!Eye)
	{
		return;
	}
	// A drag looks a little aside; let go, the eye comes back to the shot over a second.
	if (RealT - LookAsideAt > 0.3)
	{
		LookAside *= FMath::Exp(-DeltaSeconds / 0.35f);
	}
	const FVector From = World(S.Pose.Eye.X, S.Pose.Eye.Y, S.Pose.Eye.Z);
	const FVector To = World(S.Pose.Look.X, S.Pose.Look.Y, S.Pose.Look.Z);
	FRotator Facing = (To - From).Rotation();
	Facing.Yaw += LookAside.X;
	Facing.Pitch = FMath::Clamp(Facing.Pitch + LookAside.Y, -80.0, 80.0);
	Facing.Roll = S.Pose.Roll;
	Eye->SetActorLocationAndRotation(From, Facing);
	// The pose's lens is the vertical one: Unreal's is the horizontal, by the window's shape.
	double Aspect = 16.0 / 9.0;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		Aspect = Size.Y > 0.0 ? Size.X / Size.Y : Aspect;
	}
	const double Horizontal = 2.0 * FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(S.Pose.Fov * 0.5)) * Aspect));
	UCameraComponent* Lens = Eye->GetCameraComponent();
	Lens->SetFieldOfView(static_cast<float>(FMath::Clamp(Horizontal, 5.0, 120.0)));
	// The long lens's shallow focus on its subject; among the men all of it sharp.
	FPostProcessSettings& Grade = Lens->PostProcessSettings;
	if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoFocus")))
	{
		// (the camera's close follow shots name the distance held sharp, pulled after the man; the quality is switched
		// only as such a shot starts or ends, never inside one, and on while the shaders are first made, so the pass is
		// ready before the first close shot asks for it)
		const bool bShallow = bFocusLive && S.Pose.Focus > 0.0;
		const int32 Want = bShallow || (bFocusLive && !bShadersReady) ? FMath::Max(1, FocusQualityKept) : FocusQualityKept;
		if (FocusQualityKept >= 0 && Want != FocusQualityNow)
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DepthOfFieldQuality")))
			{
				Variable->Set(Want, ECVF_SetByCode);
				FocusQualityNow = Want;
			}
		}
		Grade.bOverride_DepthOfFieldFocalDistance = true;
		Grade.bOverride_DepthOfFieldFstop = true;
		Grade.DepthOfFieldFocalDistance = static_cast<float>(bShallow ? S.Pose.Focus * UnitsPerMetre : (To - From).Size());
		Grade.DepthOfFieldFstop = bShallow ? 2.8f : 11.0f;
	}
	// The ear: the mix's listener stands over what the eye looks at; down among the men it is the eye.
	if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		const double Ground = GroundZ(S.Pose.Look.X, S.Pose.Look.Y);
		const double High = S.Pose.Eye.Z - GroundZ(S.Pose.Eye.X, S.Pose.Eye.Y);
		FVector Ear = World(S.Pose.Look.X, S.Pose.Look.Y, Ground + S.Mix.EarHeightFactor() * FMath::Max(0.0, High));
		if (High < 4.0)
		{
			Ear = From;
		}
		Controller->SetAudioListenerOverride(nullptr, Ear, Facing);
	}
}

void ALandBattle::Look(const FVector2D& DeltaPixels)
{
	LookAside.X = FMath::Clamp(LookAside.X + DeltaPixels.X * 0.12, -50.0, 50.0);
	LookAside.Y = FMath::Clamp(LookAside.Y - DeltaPixels.Y * 0.12, -30.0, 30.0);
	LookAsideAt = RealT;
}

void ALandBattle::SetSpeed(double Speed)
{
	if (!State.IsValid())
	{
		return;
	}
	State->bOwnSpeed = false;
	State->Speed = FMath::Clamp(Speed, 0.0, 4.0);
}

void ALandBattle::Press(const FString& Button)
{
	if (!State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	if (Button == TEXT("continue"))
	{
		if (S.bCard)
		{
			Finish();
		}
	}
	else if (Button == TEXT("cut"))
	{
		bShotHeld = false;
		S.Eye.Hold(false);
		S.Eye.CutNow();
	}
	else if (Button == TEXT("begin"))
	{
		BeginFight();
	}
	else if (Button == TEXT("night"))
	{
		// (a key or a click while the night plays: straight to the dip and the dawn)
		if (S.NightNow.bNight)
		{
			S.Night.Skip();
			UE_LOG(LogTheater, Display, TEXT("Land battle: the night skipped to the dawn"));
		}
	}
	else if (Button == TEXT("hold"))
	{
		bShotHeld = !bShotHeld;
		S.Eye.Hold(bShotHeld);
		S.Note = bShotHeld ? "THIS SHOT HELD (H LETS IT GO)" : "THE CAMERA CUTS AGAIN";
		S.NoteAge = 0.0;
	}
}

// ---- What happens ----------------------------------------------------------------------------------------------

void ALandBattle::PlayEffects()
{
	LAND_SCOPE(PlayEffects);
	FLandBattleState& S = *State;
	if (!Effects)
	{
		return;
	}
	for (const LandPlay::Effect& E : S.Fx)
	{
		const FVector At = World(E.At.X, E.At.Y, E.At.Z);
		const FString Name = LandParts::Wide(E.Name);
		const FString Ground = E.Ground.empty() ? FString(TEXT("earth")) : LandParts::Wide(E.Ground);
		switch (E.Kind)
		{
		case LandPlay::FxKind::Muzzle:
			Effects->Muzzle(At, WorldWay(E.Way.X, E.Way.Y, E.Way.Z).GetSafeNormal(), Name, Ground);
			break;
		case LandPlay::FxKind::Shots:
			Effects->Shots(E.Gun, At, World(E.To.X, E.To.Y, E.To.Z), Name, E.Rounds, static_cast<float>(E.PerSecond), Ground); // (rounds a life second, on the kit's life clock: the mix cuts the burst alike)
			break;
		case LandPlay::FxKind::Shell:
			Effects->Shell(At, Name, Ground, E.bArmour);
			break;
		case LandPlay::FxKind::Size:
			Effects->ShellOfSize(At, Name, Ground, E.bArmour);
			break;
		case LandPlay::FxKind::Skip:
			Effects->Skip(At, WorldWay(E.Way.X, E.Way.Y, E.Way.Z).GetSafeNormal(), Ground);
			break;
		case LandPlay::FxKind::Glance:
			Effects->Glance(At, WorldWay(E.Way.X, E.Way.Y, E.Way.Z).GetSafeNormal(), WorldWay(E.Normal.X, E.Normal.Y, E.Normal.Z).GetSafeNormal(), static_cast<float>(E.CalibreMm), Ground);
			break;
		case LandPlay::FxKind::Plate:
			Effects->ShellOfSize(At, Name.IsEmpty() ? FString(TEXT("gun75")) : Name, Ground, true);
			break;
		case LandPlay::FxKind::Screen:
		{
			// A smoke round's screen, as long and as wide as its bomb gave (the kit's life clock is the seen world's: a
			// light mortar's bomb about a minute, 18 m; the 81 mm's a minute and a half, 24 m; a field gun's shell two
			// and a half minutes, 30 m). No hole: MakeCraters takes shells only.
			const double Cal = E.CalibreMm;
			const float Across = Cal < 70.0 ? 18.0f : Cal < 85.0 ? 24.0f : 30.0f;
			// (a screen the player called hangs as long as the rules' sight block holds it: their hang, in battle seconds,
			// on the kit's life clock; the general's thread, 22 November 20:22Z)
			const float Seconds = E.Hang > 0.0 ? static_cast<float>(E.Hang * FMath::Max(0.1, S.Life))
				: Cal < 70.0 ? 60.0f : Cal < 85.0 ? 90.0f : 150.0f;
			Effects->SmokeScreen(At, Across, Seconds, Name.IsEmpty() ? FString(TEXT("canister")) : Name, -1);
			break;
		}
		}
	}
}

void ALandBattle::MakeBreaches()
{
	LAND_SCOPE(Breaches);
	FLandBattleState& S = *State;
	// (a live battle's cuts come with its deltas, after the field and its things were laid: their banks are torn as
	// any other's; their shrubs, laid in the field's own copies, are left standing for now)
	if (CutStage.Num() < static_cast<int32>(S.Rec.Cuts.size()))
	{
		CutStage.SetNumZeroed(static_cast<int32>(S.Rec.Cuts.size()));
	}
	if (!S.Field || CutStage.Num() == 0)
	{
		return;
	}
	const LandField::Ground& G = *S.Field;
	const LandField::Ground* FieldPtr = S.Field.get();
	// (a shrub's own draw, the same every run: from the cut and its place in its copies)
	const auto Draw = [](int32 Cut, int32 Index, uint32 Salt)
	{
		uint32 H = static_cast<uint32>(Cut + 1) * 0x9E3779B1u ^ static_cast<uint32>(Index + 1) * 0x85EBCA77u ^ Salt * 0xC2B2AE3Du;
		H ^= H >> 15;
		H *= 0x2C1B3C6Du;
		H ^= H >> 12;
		H *= 0x297A2D39u;
		H ^= H >> 15;
		return static_cast<double>(H) / 4294967296.0;
	};
	TSet<UInstancedStaticMeshComponent*> Touched;
	const int32 Cuts = FMath::Min(CutStage.Num(), static_cast<int32>(S.Rec.Cuts.size()));
	for (int32 K = 0; K < Cuts; ++K)
	{
		const LandFeed::Cut& Cut = S.Rec.Cuts[static_cast<size_t>(K)];
		const uint8 Was = CutStage[K];
		const uint8 Now = S.Battle >= Cut.T1 ? 2 : (S.Battle >= Cut.T0 ? 1 : 0);
		if (Now <= Was)
		{
			continue;
		}
		CutStage[K] = Now;
		LandField::BankPlace BP;
		if (!G.NearestBank(Cut.X, Cut.Y, 6.0, BP) || std::hypot(BP.X - Cut.X, BP.Y - Cut.Y) > 2.5)
		{
			if (Now == 2)
			{
				UE_LOG(LogTheater, Warning, TEXT("Land battle: hedge cutter %d at %.0f, %.0f (battle %.1f s): no bank within 2.5 m, nothing torn"), K, Cut.X, Cut.Y, S.Battle);
			}
			continue;
		}
		// The bank's frame: along its line, and the side she drives out to (her heading against the bank's left).
		const double Ca = std::cos(BP.Yaw), Sa = std::sin(BP.Yaw);
		const double Left = -Sa * std::cos(Cut.Yaw) + Ca * std::sin(Cut.Yaw);
		const int Through = Left >= 0.0 ? 1 : -1;
		const double OutX = -Sa * Through, OutY = Ca * Through; // (the way she drives out, level)
		const double Half = 0.5 * Cut.Width;
		const auto GroundUnder = [&S, FieldPtr](double X, double Y)
		{
			double Z = FieldPtr->HeightAt(X, Y), Top = 0.0;
			if (S.Holes.TopAt(X, Y, Top) && (Top > Z || S.Holes.InCut(X, Y)))
			{
				Z = Top;
			}
			return Z;
		};
		int32 Thrown = 0, Crushed = 0, Leaning = 0;
		bool bTorn = false;
		if (Now == 2)
		{
			// The bank torn through (its earth down to the gap's floor, the spoil heaped the side she drove out to;
			// MakeCraters sends it with the holes, and cuts the ground drawn over it).
			LandCrater::Breach B;
			B.X = BP.X;
			B.Y = BP.Y;
			B.Yaw = BP.Yaw;
			B.Half = Half;
			B.Foot = BP.Foot;
			B.Through = Through;
			bTorn = S.Holes.AddBreach(B, [FieldPtr](double X, double Y) { return FieldPtr->BankRise(X, Y); }, "earth");
			if (bTorn)
			{
				++CutsTorn;
				const double Out = S.Holes.All().back().Out;
				// (bodies lying on the bank laid again on what is left of it; the grass over it and its spoil cleared)
				if (S.Men)
				{
					S.Men->Unsettle(BP.X, BP.Y, Out);
				}
				if (Carpet.IsValid())
				{
					Carpet->Mark(BP.X, BP.Y, Out + 0.5);
				}
				// (the earth she bursts out with: three low spurts thrown ahead of her blade across the gap)
				if (Effects)
				{
					for (const double Along : { -0.6 * Half, 0.0, 0.6 * Half })
					{
						const double X = BP.X + Ca * Along + OutX * (BP.Foot + 0.3), Y = BP.Y + Sa * Along + OutY * (BP.Foot + 0.3);
						Effects->Skip(World(X, Y, GroundUnder(X, Y) + 0.2), WorldWay(OutX, OutY, 0.35).GetSafeNormal(), TEXT("earth"));
					}
				}
			}
		}
		// The hedge's shrubs at the gap: shaken toward the way she drives as she pushes into the bank; when it gives,
		// those in the gap thrown down ahead of her blade (some crushed under her and the spoil, not seen again), those
		// at its ends left leaning out of it, torn.
		for (const FCutShrub& Shrub : CutShrubs)
		{
			UInstancedStaticMeshComponent* Copies = Shrub.Copies.Get();
			if (Shrub.Cut != K || !Copies)
			{
				continue;
			}
			const double Dx = Shrub.X - BP.X, Dy = Shrub.Y - BP.Y;
			const double Along = Dx * Ca + Dy * Sa;
			const bool bInGap = std::fabs(Along) <= Half + 0.4;
			const FQuat Turn = FRotator(0.0, -FMath::RadiansToDegrees(Shrub.Yaw), 0.0).Quaternion();
			const FVector Size(Shrub.Scale, Shrub.Scale, Shrub.ScaleZ);
			// (a tilt toward a level field way Wx, Wy, about the shrub's foot: in the engine's frame, x and y swapped)
			const auto Tilt = [](double Wx, double Wy, double Degrees)
			{
				const FVector Toward = FVector(Wy, Wx, 0.0).GetSafeNormal();
				return FQuat(FVector::UpVector ^ Toward, FMath::DegreesToRadians(Degrees));
			};
			FTransform Where;
			if (Now == 1)
			{
				if (!bInGap)
				{
					continue;
				}
				Where = FTransform(Tilt(OutX, OutY, 8.0 + 8.0 * Draw(K, Shrub.Index, 1)) * Turn, World(Shrub.X, Shrub.Y, Shrub.Z), Size);
				++Leaning;
			}
			else if (!bTorn)
			{
				continue;
			}
			else if (bInGap)
			{
				if (Draw(K, Shrub.Index, 2) < 0.25)
				{
					Where = FTransform(Turn, World(Shrub.X, Shrub.Y, Shrub.Z), FVector(1e-3));
					++Crushed;
				}
				else
				{
					// (thrown 1.5 to 3.5 m ahead, a little along the bank, and laid nearly flat, its crown ahead)
					const double Push = 1.5 + 2.0 * Draw(K, Shrub.Index, 3), Aside = 1.2 * (Draw(K, Shrub.Index, 4) - 0.5);
					const double X = Shrub.X + OutX * Push + Ca * Aside, Y = Shrub.Y + OutY * Push + Sa * Aside;
					const double Spin = 40.0 * (Draw(K, Shrub.Index, 5) - 0.5);
					Where = FTransform(Tilt(OutX, OutY, 70.0 + 18.0 * Draw(K, Shrub.Index, 6)) * FRotator(0.0, Spin, 0.0).Quaternion() * Turn,
						World(X, Y, GroundUnder(X, Y) - 0.15), Size);
					++Thrown;
				}
			}
			else
			{
				// (at the gap's ends: leaning out of it and a little ahead, its roots torn on the gap's side)
				const double Side = Along >= 0.0 ? 1.0 : -1.0;
				Where = FTransform(Tilt(Ca * Side + 0.4 * OutX, Sa * Side + 0.4 * OutY, 15.0 + 20.0 * Draw(K, Shrub.Index, 7)) * Turn,
					World(Shrub.X, Shrub.Y, GroundUnder(Shrub.X, Shrub.Y) - 0.05), Size);
				++Leaning;
			}
			Copies->UpdateInstanceTransform(Shrub.Index, Where, true, false, true);
			Touched.Add(Copies);
		}
		if (Now == 2)
		{
			UE_LOG(LogTheater, Log, TEXT("Land battle: hedge cutter %d (track %d) tore the bank at %.0f, %.0f, battle %.1f s: %s; %d shrubs thrown down, %d crushed, %d left leaning; %d of %d torn"),
				K, Cut.Unit, BP.X, BP.Y, S.Battle, bTorn ? TEXT("the gap torn") : TEXT("NOT torn (no room left for holes, or no bank's rise)"), Thrown, Crushed, Leaning, CutsTorn, Cuts);
		}
	}
	// (the moved places sent at the frame's end with the instances, as the clutter's are, not the mesh made again)
	for (UInstancedStaticMeshComponent* Copies : Touched)
	{
		Copies->MarkRenderInstancesDirty();
	}
}

FTransform ALandBattle::WireAt(double X0, double Y0, double X1, double Y1) const
{
	// (on the ground as it is now: a hole's face or a breach's floor where one is torn under it, a little over it)
	const FLandBattleState& S = *State;
	const auto Under = [&S](double X, double Y)
	{
		double Z = S.Field ? S.Field->HeightAt(X, Y) : 0.0, Top = 0.0;
		if (S.Holes.TopAt(X, Y, Top) && (Top > Z || S.Holes.InCut(X, Y)))
		{
			Z = Top;
		}
		return Z + 0.012;
	};
	const FVector A = World(X0, Y0, Under(X0, Y0)), B = World(X1, Y1, Under(X1, Y1));
	const FVector Way = B - A;
	const double Long = Way.Size() / UnitsPerMetre;
	return FTransform(FRotationMatrix::MakeFromX(Way.IsNearlyZero() ? FVector::ForwardVector : Way).ToQuat(), A, FVector(FMath::Max(Long, 0.01), 1.0, 1.0));
}

void ALandBattle::LayWires()
{
	FLandBattleState& S = *State;
	WireLengths.Reset();
	WireStates.Reset();
	NextWireCut = NextWireMend = WireHolesSeen = 0;
	if (S.Rec.Wires.empty() || !S.Field || !ThingInk)
	{
		return;
	}
	// One piece of wire a metre long along its own x (the art director's brief: dark, L 25 to 35, about a pixel wide
	// from 10 to 30 m and gone by 60): a flat-bottomed three-sided strand 14 mm across, lying on the ground.
	{
		std::vector<float> At, Facing;
		LandMen::Look Paint;
		const float W = 0.007f, H = 0.010f;
		const float Ring[3][2] = { { -W, 0.0f }, { W, 0.0f }, { 0.0f, H } };
		for (int End = 0; End < 2; ++End)
		{
			for (int K = 0; K < 3; ++K)
			{
				At.insert(At.end(), { static_cast<float>(End), Ring[K][0], Ring[K][1] });
				const float Ny = Ring[K][0] == 0.0f ? 0.0f : Ring[K][0] / W, Nz = Ring[K][0] == 0.0f ? 1.0f : 0.35f;
				Facing.insert(Facing.end(), { 0.0f, Ny, Nz });
				Paint.Rgb.insert(Paint.Rgb.end(), { 68, 64, 56 });
			}
		}
		Paint.Points = 6;
		// (the two sloping sides, both ways round: the strand is drawn from either side)
		Paint.Triangles = { 0, 3, 5, 0, 5, 2, 1, 2, 5, 1, 5, 4, 0, 2, 5, 0, 5, 3, 1, 5, 2, 1, 4, 5 };
		UStaticMesh* Piece = LandParts::BankManMesh(this, TEXT("LandWirePiece"), At, Facing, Paint, ThingInk, UnitsPerMetre);
		if (!Piece)
		{
			return;
		}
		PropMeshes.Add(Piece);
		WireCopies = NewObject<UInstancedStaticMeshComponent>(this, TEXT("LandWires"));
		WireCopies->SetupAttachment(Root);
		WireCopies->SetStaticMesh(Piece);
		WireCopies->SetMaterial(0, ThingInk);
		WireCopies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WireCopies->SetCanEverAffectNavigation(false);
		WireCopies->SetCastShadow(false);
		WireCopies->SetCullDistances(FMath::RoundToInt32(45.0 * UnitsPerMetre), FMath::RoundToInt32(60.0 * UnitsPerMetre));
		WireCopies->RegisterComponent();
	}
	// Each wire in lengths of about a metre along its points.
	TArray<FTransform> Places;
	for (const LandFeed::Wire& W : S.Rec.Wires)
	{
		FWireState& State0 = WireStates.AddDefaulted_GetRef();
		State0.Id = W.Id;
		State0.From = WireLengths.Num();
		for (size_t I = 0; I + 1 < W.Pts.size(); ++I)
		{
			const double X0 = W.Pts[I].first, Y0 = W.Pts[I].second, X1 = W.Pts[I + 1].first, Y1 = W.Pts[I + 1].second;
			const int32 Pieces = FMath::Max(1, FMath::CeilToInt32(std::hypot(X1 - X0, Y1 - Y0)));
			for (int32 P = 0; P < Pieces; ++P)
			{
				const double F0 = static_cast<double>(P) / Pieces, F1 = static_cast<double>(P + 1) / Pieces;
				FWireLength& L = WireLengths.AddDefaulted_GetRef();
				L.Wire = WireStates.Num() - 1;
				L.X0 = X0 + (X1 - X0) * F0;
				L.Y0 = Y0 + (Y1 - Y0) * F0;
				L.X1 = X0 + (X1 - X0) * F1;
				L.Y1 = Y0 + (Y1 - Y0) * F1;
				Places.Add(WireAt(L.X0, L.Y0, L.X1, L.Y1));
			}
		}
		State0.Count = WireLengths.Num() - State0.From;
	}
	WireCopies->AddInstances(Places, false, true);
	// The field telephones at each wire's two ends, a little to its side, turned along it (Content/Models/Battle's
	// FieldPhone, the tank modeller's; without it the wires alone, said).
	int32 Phones = 0;
	FString PhoneText;
	LandKit::Model Phone;
	std::string Why;
	if (FFileHelper::LoadFileToString(PhoneText, *(FPaths::ProjectContentDir() / TEXT("Models/Battle/FieldPhone.json"))) && Phone.Load(LandParts::Narrow(PhoneText), LandKit::Dress(), Why))
	{
		UStaticMesh* Made = LandParts::PropMesh(this, TEXT("LandFieldPhone"), Phone, false, ThingInk, UnitsPerMetre);
		if (Made)
		{
			PropMeshes.Add(Made);
			PhoneCopies = NewObject<UInstancedStaticMeshComponent>(this, TEXT("LandFieldPhones"));
			PhoneCopies->SetupAttachment(Root);
			PhoneCopies->SetStaticMesh(Made);
			PhoneCopies->SetMaterial(0, ThingInk);
			PhoneCopies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PhoneCopies->SetCanEverAffectNavigation(false);
			PhoneCopies->SetCullDistances(FMath::RoundToInt32(120.0 * UnitsPerMetre), FMath::RoundToInt32(150.0 * UnitsPerMetre));
			PhoneCopies->RegisterComponent();
			Places.Reset();
			for (const LandFeed::Wire& W : S.Rec.Wires)
			{
				for (int End = 0; End < 2; ++End)
				{
					const auto& P = End == 0 ? W.Pts.front() : W.Pts.back();
					const auto& Q = End == 0 ? W.Pts[1] : W.Pts[W.Pts.size() - 2];
					const double Way = std::atan2(Q.second - P.second, Q.first - P.first);
					const double X = P.first - std::sin(Way) * 0.4, Y = P.second + std::cos(Way) * 0.4;
					// (its own x along the wire: the model's x forward is the field's heading Way, Unreal's yaw 90 less it)
					Places.Add(FTransform(FRotator(0.0, 90.0 - FMath::RadiansToDegrees(Way), 0.0), World(X, Y, S.Field->HeightAt(X, Y)), FVector::OneVector));
				}
			}
			PhoneCopies->AddInstances(Places, false, true);
			Phones = Places.Num();
		}
	}
	else
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: Content/Models/Battle/FieldPhone.json not read (%s): the wires are laid without their telephones"), *LandParts::Wide(Why));
	}
	UE_LOG(LogTheater, Log, TEXT("Land battle: field telephones: %d wires in %d lengths, %d telephones; %d cuts and %d mends to come"), WireStates.Num(), WireLengths.Num(), Phones,
		static_cast<int32>(S.Rec.WireCuts.size()), static_cast<int32>(S.Rec.WireMends.size()));
}

void ALandBattle::MakeWires()
{
	LAND_SCOPE(Wires);
	FLandBattleState& S = *State;
	UInstancedStaticMeshComponent* Copies = WireCopies.Get();
	if (!Copies || WireStates.Num() == 0)
	{
		return;
	}
	bool bTouched = false;
	const auto StateOf = [this](int32 Id) -> FWireState*
	{
		for (FWireState& W : WireStates)
		{
			if (W.Id == Id)
			{
				return &W;
			}
		}
		return nullptr;
	};
	const FTransform Gone(FQuat::Identity, FVector::ZeroVector, FVector(1e-3));
	// A wire cut: its lengths within 0.4 m of the break hidden, and its two ends left curled back from the gap, lifted a
	// little off the ground (the art director's two curled ends).
	while (NextWireCut < static_cast<int32>(S.Rec.WireCuts.size()) && S.Rec.WireCuts[static_cast<size_t>(NextWireCut)].T <= S.Battle)
	{
		const LandFeed::WireCut& Cut = S.Rec.WireCuts[static_cast<size_t>(NextWireCut++)];
		FWireState* W = StateOf(Cut.Wire);
		if (!W || W->bCut)
		{
			continue;
		}
		W->bCut = true;
		W->Hidden.Reset();
		double WayX = 1.0, WayY = 0.0;
		for (int32 K = W->From; K < W->From + W->Count; ++K)
		{
			FWireLength& L = WireLengths[K];
			const double Dx = L.X1 - L.X0, Dy = L.Y1 - L.Y0, Len2 = FMath::Max(1e-9, Dx * Dx + Dy * Dy);
			const double F = FMath::Clamp(((Cut.X - L.X0) * Dx + (Cut.Y - L.Y0) * Dy) / Len2, 0.0, 1.0);
			if (std::hypot(L.X0 + Dx * F - Cut.X, L.Y0 + Dy * F - Cut.Y) <= 0.4)
			{
				L.bHidden = true;
				W->Hidden.Add(K);
				Copies->UpdateInstanceTransform(K, Gone, true, false, true);
				const double Len = std::sqrt(Len2);
				WayX = Dx / Len;
				WayY = Dy / Len;
			}
		}
		// (each end 0.45 m from the break, a 0.35 m curl back toward it, turned aside and lifted)
		int32 Ends[2] = { -1, -1 };
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const double Sign = Side == 0 ? -1.0 : 1.0;
			const double EX = Cut.X + WayX * 0.45 * Sign, EY = Cut.Y + WayY * 0.45 * Sign;
			const double Turn = (Side == 0 ? 40.0 : -55.0) * PI / 180.0;
			const double BackX = -WayX * Sign, BackY = -WayY * Sign;
			const double CX = BackX * std::cos(Turn) - BackY * std::sin(Turn), CY = BackX * std::sin(Turn) + BackY * std::cos(Turn);
			const FTransform Flat = WireAt(EX, EY, EX + CX * 0.35, EY + CY * 0.35);
			const FVector From = Flat.GetLocation();
			const FVector To = From + Flat.GetRotation().GetForwardVector() * Flat.GetScale3D().X * UnitsPerMetre + FVector(0.0, 0.0, 0.10 * UnitsPerMetre);
			const FTransform Curl(FRotationMatrix::MakeFromX(To - From).ToQuat(), From, FVector((To - From).Size() / UnitsPerMetre, 1.0, 1.0));
			Ends[Side] = Copies->AddInstance(Curl, true);
			(Side == 0 ? W->CurlAAt : W->CurlBAt) = Curl;
		}
		W->CurlA = Ends[0];
		W->CurlB = Ends[1];
		W->CutX = Cut.X;
		W->CutY = Cut.Y;
		W->WayX = WayX;
		W->WayY = WayY;
		bTouched = true;
	}
	// A wire mended: its lengths laid again whole (on the ground as it is now), its curled ends gone.
	while (NextWireMend < static_cast<int32>(S.Rec.WireMends.size()) && S.Rec.WireMends[static_cast<size_t>(NextWireMend)].T <= S.Battle)
	{
		const LandFeed::WireMend& Mend = S.Rec.WireMends[static_cast<size_t>(NextWireMend++)];
		FWireState* W = StateOf(Mend.Wire);
		if (!W || !W->bCut)
		{
			continue;
		}
		for (const int32 K : W->Hidden)
		{
			FWireLength& L = WireLengths[K];
			L.bHidden = false;
			Copies->UpdateInstanceTransform(K, WireAt(L.X0, L.Y0, L.X1, L.Y1), true, false, true);
		}
		for (const int32 Curl : { W->CurlA, W->CurlB })
		{
			if (Curl >= 0)
			{
				Copies->UpdateInstanceTransform(Curl, Gone, true, false, true);
			}
		}
		W->Hidden.Reset();
		W->CurlA = W->CurlB = -1;
		W->bCut = false;
		W->bSpliced = false;
		bTouched = true;
	}
	// The lengths a new hole was torn under laid again on its face, not left in the air over its bowl.
	const std::vector<LandCrater::Field::Hole>& Holes = S.Holes.All();
	for (; WireHolesSeen < static_cast<int32>(Holes.size()); ++WireHolesSeen)
	{
		const LandCrater::Field::Hole& H = Holes[static_cast<size_t>(WireHolesSeen)];
		const double Reach = H.Out + 1.0;
		for (int32 K = 0; K < WireLengths.Num(); ++K)
		{
			const FWireLength& L = WireLengths[K];
			if (L.bHidden || std::hypot(0.5 * (L.X0 + L.X1) - H.X, 0.5 * (L.Y0 + L.Y1) - H.Y) > Reach)
			{
				continue;
			}
			Copies->UpdateInstanceTransform(K, WireAt(L.X0, L.Y0, L.X1, L.Y1), true, false, true);
			bTouched = true;
		}
	}
	if (bTouched)
	{
		Copies->MarkRenderInstancesDirty();
	}
}

void ALandBattle::LayStretchers()
{
	FLandBattleState& S = *State;
	CarrySpans.Reset();
	if (!S.Field || !ThingInk)
	{
		return;
	}
	for (size_t K = 0; K < S.Rec.Tracks.size(); ++K)
	{
		for (const LandFeed::Track::Carry& C : S.Rec.Tracks[K].Carried)
		{
			FCarrySpan& Span = CarrySpans.AddDefaulted_GetRef();
			Span.Carried = static_cast<int32>(K);
			Span.By1 = C.By1;
			Span.By2 = C.By2;
			Span.From = C.From;
			Span.To = C.To;
		}
	}
	if (CarrySpans.Num() == 0 && S.Rec.Aid.empty())
	{
		return;
	}
	// The tank modeller's stretcher (Content/Models/Battle's Stretcher: head end +x, on its feet with its origin on the
	// ground under its middle, the carried man's place "lie" 0.11 m up).
	FString Text;
	LandKit::Model Prop;
	std::string Why;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Models/Battle/Stretcher.json"))) || !Prop.Load(LandParts::Narrow(Text), LandKit::Dress(), Why))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: Content/Models/Battle/Stretcher.json not read (%s): %d carries shown by their men alone"), *LandParts::Wide(Why), CarrySpans.Num());
		CarrySpans.Reset();
		return;
	}
	UStaticMesh* Made = LandParts::PropMesh(this, TEXT("LandStretcher"), Prop, false, ThingInk, UnitsPerMetre);
	if (!Made)
	{
		CarrySpans.Reset();
		return;
	}
	PropMeshes.Add(Made);
	StretcherCopies = NewObject<UInstancedStaticMeshComponent>(this, TEXT("LandStretchers"));
	StretcherCopies->SetupAttachment(Root);
	StretcherCopies->SetStaticMesh(Made);
	StretcherCopies->SetMaterial(0, ThingInk);
	StretcherCopies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StretcherCopies->SetCanEverAffectNavigation(false);
	StretcherCopies->SetCullDistances(FMath::RoundToInt32(150.0 * UnitsPerMetre), FMath::RoundToInt32(200.0 * UnitsPerMetre));
	StretcherCopies->RegisterComponent();
	// Each carry's copy put away until its carry starts; two stretchers laid on the ground by each aid post, a little
	// askew, waiting.
	const FTransform Gone(FQuat::Identity, FVector::ZeroVector, FVector(1e-3));
	TArray<FTransform> Places;
	for (FCarrySpan& Span : CarrySpans)
	{
		Span.Copy = Places.Num();
		Places.Add(Gone);
	}
	for (const LandFeed::AidPost& A : S.Rec.Aid)
	{
		for (int32 K = 0; K < 2; ++K)
		{
			const double X = A.X + (K == 0 ? -0.9 : 0.9), Y = A.Y + (K == 0 ? 0.3 : -0.2);
			Places.Add(FTransform(FRotator(0.0, 15.0 + 9.0 * K, 0.0), World(X, Y, GroundZ(X, Y)), FVector::OneVector));
		}
	}
	StretcherCopies->AddInstances(Places, false, true);
	UE_LOG(LogTheater, Log, TEXT("Land battle: stretcher bearers: %d carries, %d aid posts (%d stretchers laid by them)"), CarrySpans.Num(), static_cast<int32>(S.Rec.Aid.size()),
		2 * static_cast<int32>(S.Rec.Aid.size()));
}

void ALandBattle::MakeStretchers()
{
	LAND_SCOPE(Stretchers);
	FLandBattleState& S = *State;
	UInstancedStaticMeshComponent* Copies = StretcherCopies.Get();
	if (!Copies || CarrySpans.Num() == 0)
	{
		return;
	}
	const FTransform Gone(FQuat::Identity, FVector::ZeroVector, FVector(1e-3));
	bool bTouched = false;
	for (FCarrySpan& Span : CarrySpans)
	{
		if (S.Battle < Span.From || S.Battle > Span.To || Span.Carried < 0 || Span.Carried >= static_cast<int32>(S.Rec.Tracks.size()))
		{
			if (Span.bShown)
			{
				Copies->UpdateInstanceTransform(Span.Copy, Gone, true, false, true);
				Span.bShown = false;
				bTouched = true;
			}
			continue;
		}
		// Under the man carried where he is drawn (his root at the stretcher's `lie`, the soldiers' carried_wounded), at
		// the height he is lifted to (the bearers' fists, LandMen's CarriedLift, raised over the first second), level.
		double X = 0.0, Y = 0.0, Z = 0.0;
		float F = 0.f;
		const size_t T = static_cast<size_t>(Span.Carried);
		if (!(T < S.ManOf.size() && S.ManOf[T] >= 0 && S.Men && S.Men->PlaceOf(S.ManOf[T], X, Y, Z, F)))
		{
			const LandFeed::Spot Spot = LandFeed::SpotAt(S.Rec.Tracks[T], S.Battle);
			X = Spot.X;
			Y = Spot.Y;
			Z = GroundZ(X, Y);
			F = static_cast<float>(Spot.F);
		}
		const double Lift = static_cast<double>(LandMen::CarriedLift) * FMath::Clamp(S.Battle - Span.From, 0.0, 1.0);
		// (carried head first: the stretcher's head end, +x, his way, the front bearer's; his root at its `lie`, 0.11 m
		// over its root; a heading clockwise from north is Unreal's yaw)
		const FTransform At(FRotator(0.0, FMath::RadiansToDegrees(static_cast<double>(F)), 0.0), World(X, Y, Z + FMath::Max(0.0, Lift - 0.11)), FVector::OneVector);
		Copies->UpdateInstanceTransform(Span.Copy, At, true, false, true);
		Span.bShown = true;
		bTouched = true;
	}
	if (bTouched)
	{
		Copies->MarkRenderInstancesDirty();
	}
}

void ALandBattle::MakeReels()
{
	LAND_SCOPE(Reels);
	FLandBattleState& S = *State;
	if (!S.Men || !S.Field)
	{
		return;
	}
	const FTransform Gone(FQuat::Identity, FVector::ZeroVector, FVector(1e-3));
	UInstancedStaticMeshComponent* Copies = ReelCopies.Get();
	bool bTouched = false;
	// The linemen met so far (a live battle's come with its pieces; its reset makes every track again, from 0).
	if (ReelTracksSeen > static_cast<int32>(S.Rec.Tracks.size()))
	{
		for (const FReelMan& R : ReelMen)
		{
			if (R.Copy >= 0)
			{
				if (Copies && R.bShown)
				{
					Copies->UpdateInstanceTransform(R.Copy, Gone, true, false, true);
					bTouched = true;
				}
				FreeReels.Add(R.Copy);
			}
		}
		ReelMen.Reset();
		ReelTracksSeen = 0;
	}
	for (; ReelTracksSeen < static_cast<int32>(S.Rec.Tracks.size()); ++ReelTracksSeen)
	{
		if (S.Rec.Tracks[static_cast<size_t>(ReelTracksSeen)].bLineman)
		{
			ReelMen.AddDefaulted_GetRef().Track = ReelTracksSeen;
		}
	}
	if (ReelMen.Num() == 0)
	{
		if (bTouched && Copies)
		{
			Copies->MarkRenderInstancesDirty();
		}
		return;
	}
	// The tank modeller's reel (Content/Models/Battle's CableReel: x forward, its root on the ground under it, its two
	// handles 0.56 m up), read the first time a lineman is met.
	if (!Copies && !bReelTried && ThingInk)
	{
		bReelTried = true;
		FString Text;
		LandKit::Model Prop;
		std::string Why;
		UStaticMesh* Made = nullptr;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Models/Battle/CableReel.json"))) && Prop.Load(LandParts::Narrow(Text), LandKit::Dress(), Why))
		{
			Made = LandParts::PropMesh(this, TEXT("LandCableReel"), Prop, false, ThingInk, UnitsPerMetre);
		}
		if (!Made)
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle: Content/Models/Battle/CableReel.json not read (%s): the linemen walk without their reels"), *LandParts::Wide(Why));
			return;
		}
		PropMeshes.Add(Made);
		ReelCopies = NewObject<UInstancedStaticMeshComponent>(this, TEXT("LandCableReels"));
		ReelCopies->SetupAttachment(Root);
		ReelCopies->SetStaticMesh(Made);
		ReelCopies->SetMaterial(0, ThingInk);
		ReelCopies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ReelCopies->SetCanEverAffectNavigation(false);
		ReelCopies->SetCullDistances(FMath::RoundToInt32(100.0 * UnitsPerMetre), FMath::RoundToInt32(140.0 * UnitsPerMetre));
		ReelCopies->RegisterComponent();
		Copies = ReelCopies.Get();
		UE_LOG(LogTheater, Log, TEXT("Land battle: the linemen's cable reels made (%d linemen so far)"), ReelMen.Num());
	}
	if (!Copies)
	{
		return;
	}
	// (a length of wire from one place to another, as the wire's own pieces are drawn: along x, a metre long)
	const auto Strand = [this](const FVector& From, const FVector& To)
	{
		const FVector D = To - From;
		return FTransform(FRotationMatrix::MakeFromX(D.IsNearlyZero() ? FVector::ForwardVector : D).ToQuat(), From, FVector(FMath::Max(1e-3, D.Size() / UnitsPerMetre), 1.0, 1.0));
	};
	for (FWireState& W : WireStates)
	{
		W.bSpliced = false;
	}
	UInstancedStaticMeshComponent* Wires = WireCopies.Get();
	std::vector<float> Keys;
	for (FReelMan& R : ReelMen)
	{
		if (R.Copy < 0)
		{
			R.Copy = FreeReels.Num() > 0 ? FreeReels.Pop(EAllowShrinking::No) : Copies->AddInstance(Gone, true);
		}
		const size_t T = static_cast<size_t>(R.Track);
		const int32 Man = T < S.ManOf.size() ? S.ManOf[T] : -1;
		bool bShow = false;
		FTransform At = Gone;
		if (Man >= 0 && Man < S.Men->Count() && S.Men->Visible(Man) && !S.Men->IsFalling(Man))
		{
			const LandMen::Xform& Frame = S.Men->WorldFrame(Man);
			const LandMen::Vec3 Ahead = Frame.Turn(LandMen::Vec3{ 1.f, 0.f, 0.f });
			// (his heading, clockwise from north, is Unreal's yaw; the reel faces as he does)
			const double Yaw = FMath::RadiansToDegrees(std::atan2(static_cast<double>(Ahead.X), static_cast<double>(Ahead.Y)));
			if (S.Men->PropAt(Man, "reel", Keys) && Keys.size() >= 3)
			{
				// Walking his line out: the reel's root where his clip carries it (his fists on its two handles).
				const LandMen::Vec3 P = Frame.Point(LandMen::Vec3{ Keys[0], Keys[1], Keys[2] });
				At = FTransform(FRotator(0.0, Yaw, 0.0), World(P.X, P.Y, P.Z), FVector::OneVector);
				bShow = true;
			}
			else if (S.Men->PropAt(Man, "hands", Keys) && Keys.size() >= 6)
			{
				// Splicing: the reel set down by his right knee, turned a little out; the cut wire's two ends in his fists.
				const LandMen::Vec3 P = Frame.Point(LandMen::Vec3{ 0.15f, 0.45f, 0.f });
				At = FTransform(FRotator(0.0, Yaw + 25.0, 0.0), World(P.X, P.Y, GroundZ(P.X, P.Y)), FVector::OneVector);
				bShow = true;
				FWireState* Near = nullptr;
				double Best = 4.0;
				for (FWireState& W : WireStates)
				{
					const double D = std::hypot(W.CutX - P.X, W.CutY - P.Y);
					if (W.bCut && !W.bSpliced && W.CurlA >= 0 && W.CurlB >= 0 && D < Best)
					{
						Best = D;
						Near = &W;
					}
				}
				if (Near && Wires)
				{
					const LandMen::Vec3 L = Frame.Point(LandMen::Vec3{ Keys[0], Keys[1], Keys[2] });
					const LandMen::Vec3 Rt = Frame.Point(LandMen::Vec3{ Keys[3], Keys[4], Keys[5] });
					// (each end from where it lies, 0.45 m along the wire from the break, up into the fist nearer it)
					const double AX = Near->CutX - Near->WayX * 0.45, AY = Near->CutY - Near->WayY * 0.45;
					const double BX = Near->CutX + Near->WayX * 0.45, BY = Near->CutY + Near->WayY * 0.45;
					const bool bLeftToA = std::hypot(L.X - AX, L.Y - AY) + std::hypot(Rt.X - BX, Rt.Y - BY) <= std::hypot(L.X - BX, L.Y - BY) + std::hypot(Rt.X - AX, Rt.Y - AY);
					const LandMen::Vec3& FistA = bLeftToA ? L : Rt;
					const LandMen::Vec3& FistB = bLeftToA ? Rt : L;
					const FVector GroundA = World(AX, AY, GroundZ(AX, AY) + 0.01), GroundB = World(BX, BY, GroundZ(BX, BY) + 0.01);
					Wires->UpdateInstanceTransform(Near->CurlA, Strand(GroundA, World(FistA.X, FistA.Y, FistA.Z)), true, false, true);
					Wires->UpdateInstanceTransform(Near->CurlB, Strand(GroundB, World(FistB.X, FistB.Y, FistB.Z)), true, false, true);
					Near->bSpliced = true;
					SplicedWires.AddUnique(Near->Id);
				}
			}
		}
		if (bShow)
		{
			R.LastAt = At;
			R.ShownAt = S.Battle;
		}
		else if (R.bShown && Man >= 0 && S.Battle - R.ShownAt < 2.0)
		{
			// (between his clips, kneeling down or getting up, a moment with neither: the reel left where it was, on the
			// ground, not gone and back)
			FVector Where = R.LastAt.GetLocation();
			const double FX = Where.Y / UnitsPerMetre, FY = Where.X / UnitsPerMetre;
			At = R.LastAt;
			At.SetLocation(World(FX, FY, GroundZ(FX, FY)));
			bShow = true;
		}
		if (bShow || R.bShown)
		{
			Copies->UpdateInstanceTransform(R.Copy, At, true, false, true);
			R.bShown = bShow;
			bTouched = true;
		}
	}
	// (a wire's ends let go of, its splice not done: curled again where they lay)
	if (Wires)
	{
		bool bWiresTouched = SplicedWires.Num() > 0;
		for (int32 K = SplicedWires.Num() - 1; K >= 0; --K)
		{
			FWireState* W = nullptr;
			for (FWireState& Each : WireStates)
			{
				if (Each.Id == SplicedWires[K])
				{
					W = &Each;
					break;
				}
			}
			if (W && W->bSpliced)
			{
				continue;
			}
			if (W && W->bCut && W->CurlA >= 0 && W->CurlB >= 0)
			{
				Wires->UpdateInstanceTransform(W->CurlA, W->CurlAAt, true, false, true);
				Wires->UpdateInstanceTransform(W->CurlB, W->CurlBAt, true, false, true);
			}
			SplicedWires.RemoveAtSwap(K);
		}
		if (bWiresTouched)
		{
			Wires->MarkRenderInstancesDirty();
		}
	}
	if (bTouched)
	{
		Copies->MarkRenderInstancesDirty();
	}
}

void ALandBattle::MakeCraters()
{
	LAND_SCOPE(Craters);
	FLandBattleState& S = *State;
	if (!S.Field)
	{
		return;
	}
	// Every shell, bomb and grenade that lands in the ground leaves its hole, for the whole battle; none on armour,
	// in a house, on a wall's line or in water.
	const LandField::Ground& G = *S.Field;
	for (const LandPlay::Effect& E : S.Fx)
	{
		if ((E.Kind != LandPlay::FxKind::Shell && E.Kind != LandPlay::FxKind::Size) || E.bArmour)
		{
			continue;
		}
		const LandField::Surface Under = G.At(E.At.X, E.At.Y);
		if (Under.What == LandField::Drawn::Floor || Under.What == LandField::Drawn::Wall || Under.What == LandField::Drawn::Water)
		{
			continue;
		}
		if (!S.Holes.Add(E.At.X, E.At.Y, E.Name, E.CalibreMm, std::string(Under.Fx)))
		{
			continue;
		}
		// (a hole torn where bodies already lie: they are laid again on its face, on its rim and not sunk in it)
		if (S.Men)
		{
			S.Men->Unsettle(E.At.X, E.At.Y, S.Holes.All().back().Out);
		}
		// (the grass squares it tears laid again without what it blew away, each at most once a second, from what the
		// square already holds: a barrage where the camera looks no longer lays all the grass again, LandCarpet)
		if (Carpet.IsValid())
		{
			const LandCrater::Field::Hole& Torn = S.Holes.All().back();
			Carpet->Mark(E.At.X, E.At.Y, FMath::Max(Torn.Out, 1.2 * Torn.R) + 0.5); // (its bare earth: 1.2 of its radius, Field::Bare)
		}
	}
	// Each section with a new hole in it sent again whole, on the ground's own paint. One mesh a 64 m square of the field
	// (LandCrater's patches): the engine copies every section of a mesh again when one changes, and redraws the shadows
	// cached under its bounds, so a new hole costs only its own square, never the whole field's hedges and trees.
	// The dug holes' cuts: the ground drawn and the tracks' marks cut over their bowls (the older holes are cut by Add).
	for (const std::vector<double>& Cut : S.Holes.TakeCuts())
	{
		S.Dug.Cut(Cut);
		S.Ruts.Cut(Cut);
	}
	SendDug();
	const std::vector<LandCrater::Field::Place> Changed = S.Holes.TakeChanged();
	if (Changed.empty())
	{
		return;
	}
	TArray<FVector> P, N;
	TArray<FColor> C;
	TArray<int32> T;
	for (const LandCrater::Field::Place& Where : Changed)
	{
		// (a patch is numbered when its first hole is laid: a new one is always the next mesh)
		while (CraterMeshes.Num() <= Where.Patch)
		{
			CraterMeshes.Add(LandParts::NewMesh(this, Root, FName(*FString::Printf(TEXT("LandCraters%d"), CraterMeshes.Num())), true));
		}
		UProceduralMeshComponent* Mesh = CraterMeshes[Where.Patch];
		if (!Mesh)
		{
			continue;
		}
		LandParts::FromField(S.Holes.Section(Where), UnitsPerMetre, P, N, C, T);
		Mesh->CreateMeshSection(Where.Section, P, T, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(Where.Section, GroundInk);
		TornChanged(0, Where.Patch, Where.Section);
	}
}

void ALandBattle::LayScars()
{
	// Scarred ground (release 2; the ideas Brian took at 21:21Z, "Do all of these": a field fought over keeps its craters
	// next time; looks only, the design director's ruling): the recording's "scars" {count, months} where the player's
	// switch has it on; -TheaterLandScars=N (battles before) and -TheaterLandScarMonths=M stand in for it on any field,
	// for the Mac's off/on line; -TheaterLandNoScars lays none.
	FLandBattleState& S = *State;
	if (!S.Field)
	{
		return;
	}
	int32 Battles = S.Rec.ScarCount;
	double Months = S.Rec.ScarMonths;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandScars="), Battles);
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandScarMonths="), Months);
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoScars")))
	{
		Battles = 0;
	}
	if (Battles <= 0)
	{
		return;
	}
	std::vector<std::pair<double, double>> Fought;
	for (const LandFeed::Track& T : S.Rec.Tracks)
	{
		for (size_t K = 0; K < T.At.size(); K += 4)
		{
			Fought.emplace_back(T.At[K].X, T.At[K].Y);
		}
	}
	const LandField::Ground* FieldPtr = S.Field.get();
	const LandCrater::Field* HolesPtr = &S.Holes;
	const double Took = LandParts::Seconds();
	const int Laid = LandCrater::LayScars(S.Holes, Fought, Battles, Months, S.Rec.Seed ? S.Rec.Seed : 1,
		[FieldPtr, HolesPtr](double X, double Y) -> std::string
		{
			// (open ground only: a floor, a wall, water or marsh none; a lane or a yard mended since, none)
			const LandField::Surface Under = FieldPtr->At(X, Y);
			if (Under.What == LandField::Drawn::Floor || Under.What == LandField::Drawn::Wall || Under.What == LandField::Drawn::Water || Under.What == LandField::Drawn::Marsh)
			{
				return std::string();
			}
			if (HolesPtr->Dig && !HolesPtr->Dig(X, Y))
			{
				return std::string();
			}
			return std::string(Under.Fx);
		},
		// (the old lines they lie along: the hedges, walls and lanes, the lanes' junctions, the village's edge)
		LandCrater::OldLinesOf(*FieldPtr, S.Rec.Ground.TownX, S.Rec.Ground.TownY, S.Rec.Ground.TownR));
	// (their meshes and the ground's cuts made now, before the first frame; the grass and corn already laid round the
	// eye laid again without what stands over an old bowl)
	MakeCraters();
	if (Carpet.IsValid())
	{
		for (const LandCrater::Field::Hole& H : S.Holes.All())
		{
			Carpet->Mark(H.X, H.Y, FMath::Max(H.Out, 1.2 * H.R) + 0.5);
		}
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle: scarred ground: %d old holes of %d battle(s) fought here before, the last %.0f months ago, laid in %.2f s (-TheaterLandNoScars: none)"),
		Laid, Battles, Months, LandParts::Seconds() - Took);
}

void ALandBattle::LayOldRuts()
{
	// Mud memory (release 2; Brian 23 November, "Do all of these": a spring battle's ruts stay on that field for the next
	// battle there; looks only): the recording's "oldRuts" {count, months}; -TheaterLandOldRuts=N (the battles fought here
	// in mud before) and -TheaterLandOldRutMonths=M stand in for it on any field, for the Mac's off/on line;
	// -TheaterLandNoOldRuts lays none.
	FLandBattleState& S = *State;
	if (!S.Field)
	{
		return;
	}
	// (the campaign writes it only while the player's Battle rules row "Old ruts" is on, and that row starts off until
	// the art director passes the look and the Mac holds 45 fps: the production lead's call, 23 November 01:40Z)
	int32 Battles = S.Rec.OldRutCount;
	double Months = S.Rec.OldRutMonths;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandOldRuts="), Battles);
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandOldRutMonths="), Months);
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoOldRuts")))
	{
		Battles = 0;
	}
	if (Battles <= 0)
	{
		return;
	}
	// The fought-over ground (as the scars read it), and the way the battle's machines went: their tracks' start to
	// end, as a line (a heading and its reverse alike), the longer the more.
	std::vector<std::pair<double, double>> Fought;
	double Sx = 0.0, Sy = 0.0;
	for (const LandFeed::Track& T : S.Rec.Tracks)
	{
		for (size_t K = 0; K < T.At.size(); K += 4)
		{
			Fought.emplace_back(T.At[K].X, T.At[K].Y);
		}
		if (T.IsVehicle() && T.At.size() >= 2)
		{
			const double Dx = T.At.back().X - T.At.front().X, Dy = T.At.back().Y - T.At.front().Y, Long = FMath::Sqrt(Dx * Dx + Dy * Dy);
			if (Long > 20.0)
			{
				const double A = 2.0 * FMath::Atan2(Dy, Dx);
				Sx += Long * FMath::Cos(A);
				Sy += Long * FMath::Sin(A);
			}
		}
	}
	const double Heading = Sx * Sx + Sy * Sy > 0.0 ? 0.5 * FMath::Atan2(Sy, Sx) : std::nan("");
	std::vector<std::pair<double, double>> Gates;
	for (const LandField::HedgeOpening& O : S.Field->Openings())
	{
		Gates.emplace_back(O.X, O.Y);
	}
	// (puddled when the ground is in mud now: the campaign's word, or the Mac's stand-in for it)
	FString Word(UTF8_TO_TCHAR(S.Rec.WeatherWord.c_str()));
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandWeatherWord="), Word);
	const bool bWet = Word.TrimStartAndEnd().ToLower() == TEXT("mud");
	const double Took = LandParts::Seconds();
	const uint64_t Seed = S.Rec.Seed ? S.Rec.Seed : 1;
	// (where a column may drive: a field, a pasture, rough ground, the trodden field works; not through a hedge's bank, a lane, a wall, a yard, a
	// wood or water, pre-check's 01:36Z: seven ways had crossed a bank away from any gateway twelve times)
	const LandField::Ground* FieldPtr = S.Field.get();
	const auto Open = [FieldPtr](double X, double Y)
	{
		LandField::Drawn What = LandField::Drawn::Pasture;
		FieldPtr->HeightAt(X, Y, &What);
		return What == LandField::Drawn::Pasture || What == LandField::Drawn::Crop || What == LandField::Drawn::Rough || What == LandField::Drawn::Works || What == LandField::Drawn::Beyond;
	};
	const std::vector<std::vector<std::pair<double, double>>> Ways = LandCrater::OldWays(Fought, Battles, Seed, Gates, Heading, Open);
	const int Laid = S.Ruts.LayOld(Ways, Months, bWet, Seed);
	// (the grass laid again without what stands in a bare rut: dried mud or puddled; grassed over, it stands)
	if (Carpet.IsValid() && (bWet || Months < 3.0))
	{
		for (const std::vector<std::pair<double, double>>& Way : Ways)
		{
			for (size_t K = 0; K < Way.size(); K += 2)
			{
				Carpet->Mark(Way[K].first, Way[K].second, 8.0);
			}
		}
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle: mud memory: %d pieces of old ruts along %d ways, %d battle(s) fought here in mud before, the last %.0f months ago%s, laid in %.2f s (-TheaterLandNoOldRuts: none)"),
		Laid, static_cast<int32>(Ways.size()), Battles, Months, bWet ? TEXT(", puddled") : TEXT(""), LandParts::Seconds() - Took);
}

void ALandBattle::SendDug()
{
	FLandBattleState& S = *State;
	TArray<FVector> P, N;
	TArray<FColor> C;
	TArray<int32> T;
	// A tile first dug in: its near ground drawn by parts from now on (each a 62 m square: a new hole sends only the
	// part or two it is cut in, and redraws only their shadows), the tile's own near mesh hidden for good.
	for (const int TileIndex : S.Dug.TakeSplit())
	{
		if (!Tiles.IsValidIndex(TileIndex))
		{
			continue;
		}
		FTileDrawn& Tile = Tiles[TileIndex];
		const bool bShow = Tile.bNearShown || !Tile.Far.IsValid();
		Tile.NearParts.SetNum(LandCrater::Dug::Split * LandCrater::Dug::Split);
		for (int32 Part = 0; Part < Tile.NearParts.Num(); ++Part)
		{
			const LandField::Mesh& M = S.Dug.PartOf({ TileIndex, Part });
			if (M.Empty())
			{
				continue;
			}
			LandParts::FromField(M, UnitsPerMetre, P, N, C, T);
			UProceduralMeshComponent* Mesh = LandParts::NewMesh(this, Root, FName(*FString::Printf(TEXT("LandDug%d_%d"), TileIndex, Part)), true);
			Mesh->CreateMeshSection(0, P, T, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
			Mesh->SetMaterial(0, GroundInk);
			Mesh->SetVisibility(bShow);
			FieldMeshes.Add(Mesh);
			Tile.NearParts[Part] = Mesh;
			TornChanged(2, TileIndex, Part);
		}
		if (Tile.Near.IsValid())
		{
			Tile.Near->SetVisibility(false);
		}
	}
	for (const LandCrater::Dug::Place& Where : S.Dug.TakeChanged())
	{
		if (!Tiles.IsValidIndex(Where.Tile) || !Tiles[Where.Tile].NearParts.IsValidIndex(Where.Part))
		{
			continue;
		}
		UProceduralMeshComponent* Mesh = Tiles[Where.Tile].NearParts[Where.Part].Get();
		if (!Mesh)
		{
			continue;
		}
		LandParts::FromField(S.Dug.PartOf(Where), UnitsPerMetre, P, N, C, T);
		Mesh->CreateMeshSection(0, P, T, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
		TornChanged(2, Where.Tile, Where.Part);
	}
}

void ALandBattle::MakeRuts()
{
	LAND_SCOPE(Ruts);
	FLandBattleState& S = *State;
	if (!S.Field || !S.Armour)
	{
		return;
	}
	// Each tank, lorry and towed gun that moves on the ground leaves her tracks' marks behind her (laid from the front of
	// her tracks' ground, under her: Vehicle::TreadLeft): pressed grass, torn stubble or plough, and in standing corn the
	// corn between her tracks laid flat. A knocked-out tank, or one gone from the field, lays none; seen to jump (a seek), her
	// marks start afresh.
	for (const LandKit::Vehicle& V : S.Armour->Vehicles())
	{
		if (V.bDead || V.Fade < 0.5)
		{
			S.Ruts.Lift(V.Index);
			continue;
		}
		if (!V.bMoving)
		{
			continue;
		}
		const double Width = LandCrater::Ruts::TrackWidthOf(V.Look().Name, V.Look().Kind);
		if (S.Ruts.Drive(V.Index, V.TreadLeft.X, V.TreadLeft.Y, V.TreadRight.X, V.TreadRight.Y, Width))
		{
			// (the grass and corn squares under her laid again, without what she flattened, LandCarpet)
			if (Carpet.IsValid())
			{
				Carpet->Mark(V.Root.At.X, V.Root.At.Y, 5.0);
			}
		}
	}
	// The patches' sections with new pieces sent again whole, on the ground's own paint, four times a second at most.
	if (RealT - RutsSentAt < 0.25)
	{
		return;
	}
	const std::vector<LandCrater::Ruts::Place> Changed = S.Ruts.TakeChanged();
	if (Changed.empty())
	{
		return;
	}
	RutsSentAt = RealT;
	TArray<FVector> P, N;
	TArray<FColor> C;
	TArray<int32> T;
	for (const LandCrater::Ruts::Place& Where : Changed)
	{
		while (RutMeshes.Num() <= Where.Patch)
		{
			RutMeshes.Add(LandParts::NewMesh(this, Root, FName(*FString::Printf(TEXT("LandRuts%d"), RutMeshes.Num())), true));
		}
		UProceduralMeshComponent* Mesh = RutMeshes[Where.Patch];
		if (!Mesh)
		{
			continue;
		}
		LandParts::FromField(S.Ruts.Section(Where), UnitsPerMetre, P, N, C, T);
		Mesh->CreateMeshSection(Where.Section, P, T, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(Where.Section, GroundInk);
		TornChanged(1, Where.Patch, Where.Section);
	}
}

void ALandBattle::TornChanged(int32 Kind, int32 A, int32 B)
{
	++TornSent;
	if (!bTornBakes)
	{
		return;
	}
	const int64 PieceKey = TornKey(Kind, A, B);
	FTornPiece& Piece = TornPieces.FindOrAdd(PieceKey);
	Piece.Kind = Kind;
	Piece.A = A;
	Piece.B = B;
	Piece.ChangedAt = RealT;
	if (UStaticMeshComponent* Was = Piece.Baked.Get())
	{
		// (its baked mesh let go: its procedural section, sent just now, draws it again until it settles)
		TornBaked.RemoveSingleSwap(Was);
		Was->DestroyComponent();
	}
	Piece.Baked.Reset();
	if (!Piece.bHot)
	{
		Piece.bHot = true;
		TornHot.Add(PieceKey);
	}
}

void ALandBattle::SettleTorn()
{
	LAND_SCOPE(SettleTorn);
	if (!bTornBakes || TornHot.Num() == 0 || !State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	// Each piece unchanged TornSettleAfter seconds baked into a static mesh of its own, drawn from cached draw commands,
	// and its procedural section emptied; within TornBakeMs a frame (the production lead's guard: a hitch is worse than
	// a few more procedural pieces), the rest left for the frames after.
	const double Began = LandParts::Seconds();
	for (int32 I = 0; I < TornHot.Num() && (LandParts::Seconds() - Began) * 1000.0 < TornBakeMs;)
	{
		const int64 PieceKey = TornHot[I];
		FTornPiece* Piece = TornPieces.Find(PieceKey);
		if (!Piece || !Piece->bHot)
		{
			TornHot.RemoveAtSwap(I);
			continue;
		}
		if (RealT - Piece->ChangedAt < TornSettleAfter)
		{
			++I;
			continue;
		}
		const LandField::Mesh* M = nullptr;
		UProceduralMeshComponent* Proc = nullptr;
		bool bShown = true;
		if (Piece->Kind == 0 && Piece->A >= 0 && Piece->A < S.Holes.Patches() && Piece->B >= 0
			&& Piece->B < static_cast<int32>(S.Holes.PatchAt(Piece->A).Sections.size()) && CraterMeshes.IsValidIndex(Piece->A))
		{
			M = &S.Holes.Section({ Piece->A, Piece->B });
			Proc = CraterMeshes[Piece->A];
		}
		else if (Piece->Kind == 1 && Piece->A >= 0 && Piece->A < S.Ruts.Patches() && Piece->B >= 0
			&& Piece->B < static_cast<int32>(S.Ruts.PatchAt(Piece->A).Sections.size()) && RutMeshes.IsValidIndex(Piece->A))
		{
			M = &S.Ruts.Section({ Piece->A, Piece->B });
			Proc = RutMeshes[Piece->A];
		}
		else if (Piece->Kind == 2 && Tiles.IsValidIndex(Piece->A) && Tiles[Piece->A].NearParts.IsValidIndex(Piece->B) && S.Dug.IsSplit(Piece->A))
		{
			M = &S.Dug.PartOf({ Piece->A, Piece->B });
			Proc = Tiles[Piece->A].NearParts[Piece->B].Get();
			bShown = Tiles[Piece->A].bNearShown || !Tiles[Piece->A].Far.IsValid();
		}
		Piece->bHot = false;
		TornHot.RemoveAtSwap(I);
		if (!M || !Proc)
		{
			continue;
		}
		if (!M->Empty())
		{
			const FString Name = FString::Printf(TEXT("LandTorn%d_%d_%d"), Piece->Kind, Piece->A, Piece->B);
			UStaticMesh* Shape = LandParts::GroundMesh(this, MakeUniqueObjectName(this, UStaticMesh::StaticClass(), FName(*(Name + TEXT("_Mesh")))), *M, GroundInk,
				UnitsPerMetre);
			if (!Shape)
			{
				continue; // (left procedural)
			}
			UStaticMeshComponent* Baked = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(*Name)));
			Baked->SetupAttachment(Root);
			Baked->SetStaticMesh(Shape);
			Baked->SetMaterial(0, GroundInk);
			Baked->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Baked->SetCanEverAffectNavigation(false);
			Baked->SetCastShadow(true);
			Baked->SetVisibility(bShown);
			Baked->RegisterComponent();
			TornBaked.Add(Baked);
			Piece->Baked = Baked;
		}
		// (its procedural section emptied: the baked mesh draws it now)
		if (Piece->Kind == 2)
		{
			Proc->ClearAllMeshSections();
		}
		else
		{
			Proc->ClearMeshSection(Piece->B);
		}
		++TornBakedCount;
	}
	TornBakeMostMs = FMath::Max(TornBakeMostMs, (LandParts::Seconds() - Began) * 1000.0);
}

void ALandBattle::PlayArmourEvents()
{
	LAND_SCOPE(ArmourEvents);
	FLandBattleState& S = *State;
	if (!S.Armour)
	{
		return;
	}
	// The tanks' own beats: what the fire (LandPlay) does not already draw and sound. Her gun's flash and its shot are
	// the round's, the strike's sparks the round's landing, the spent case the mix's: those are left out here.
	static TSet<FString> Unknown;
	for (const LandKit::Event& E : S.Armour->Events)
	{
		FString Fx = LandParts::Wide(E.Fx);
		const FVector At = World(E.At.X, E.At.Y, E.At.Z);
		const FVector Up = WorldWay(E.Facing.X, E.Facing.Y, E.Facing.Z).GetSafeNormal(0.01, FVector::UpVector);
		// (the round's: her gun's flash and blast dust, the strike's sparks and burst, small arms' rattle; the mix's: the
		// spent case; her moving dust is drawn from her each frame; the crew and the gun's fittings make no effect)
		if (Fx.Contains(TEXT("muzzle")) || Fx.Contains(TEXT("spark")) || Fx.Contains(TEXT("rattle")) || Fx.Contains(TEXT("case")) || Fx == TEXT("hit")
			|| Fx.StartsWith(TEXT("hit_")) || Fx.Contains(TEXT("dust")) || Fx.Contains(TEXT("exhaust")) || Fx.Contains(TEXT("engine"))
			|| Fx.Contains(TEXT("brake")) || Fx.Contains(TEXT("bail")) || Fx.Contains(TEXT("breech")) || Fx.Contains(TEXT("trail")) || Fx.Contains(TEXT("hook"))
			|| Fx.Contains(TEXT("board")) || Fx == TEXT("hatch_open") || Fx.Contains(TEXT("turret_lands")))
		{
			Fx.Reset();
		}
		// Her fuel going up: a rolling ball of burning fuel over her engine deck, once a vehicle, as a tank brews up (her
		// ammunition's blast) or a lorry is knocked out (its fire starting). Some 8 m across for a tank (the art
		// director's ruling), 6 for a lorry; it rolls on as she was moving. Her own fire burns on after it as before.
		if (Effects && E.Vehicle >= 0 && static_cast<size_t>(E.Vehicle) < S.Armour->Vehicles().size()
			&& (Fx == TEXT("magazine_blast") || (Fx == TEXT("fire_start") && S.Armour->Vehicles()[static_cast<size_t>(E.Vehicle)].Look().Kind == "carrier")))
		{
			const LandKit::Vehicle& V = S.Armour->Vehicles()[static_cast<size_t>(E.Vehicle)];
			LandKit::V3 Deck, DeckWay;
			const FVector BallAt = V.SocketNow("engine", Deck, DeckWay) ? World(Deck.X, Deck.Y, Deck.Z) : At;
			const FVector Rolling = WorldWay(V.Velocity.X, V.Velocity.Y, V.Velocity.Z) * (UnitsPerMetre / S.Life);
			Effects->FuelBall(BallAt, V.Look().Kind == "carrier" ? 6.0f : 8.0f, Rolling);
		}
		if (Effects && !Fx.IsEmpty())
		{
			if (Fx.Contains(TEXT("brew")) || Fx.Contains(TEXT("cook")) || Fx.Contains(TEXT("magazine")) || Fx.Contains(TEXT("flame_jet")))
			{
				// (a brew-up: the ammunition goes, flame roaring from the hatches; the turret thrown is the model's. The jet
				// 2.5 s: the art director's burning-tank ruling gives a cook-off's jet 1 to 3 s)
				Effects->CookOff(At, Up, 1.0f, 8, 5.0f, 2.5f, -1.0f, E.Key);
			}
			else if (Fx.Contains(TEXT("knocked")) || Fx.Contains(TEXT("knock_out")) || Fx.Contains(TEXT("hatch_blown")))
			{
				Effects->CookOff(At, Up, 0.8f, 3, 2.5f, 0.0f, -1.0f, E.Key);
			}
			else if (Fx.Contains(TEXT("fire")) || Fx.Contains(TEXT("burn")))
			{
				const float Seconds = static_cast<float>(FMath::Max(5.0, ((E.bLoop ? E.Until : S.Rec.End + 8.0) - E.T) * S.Life));
				Effects->Burn(At, 2.4f, Seconds, E.Key);
			}
			else if (Fx.Contains(TEXT("smoke_column")))
			{
				Effects->Burn(At, 3.2f, static_cast<float>(FMath::Max(10.0, (S.Rec.End + 8.0 - E.T) * S.Life)), E.Key + 200000);
			}
			else if (Fx.Contains(TEXT("smoke")))
			{
				// (a burst of smoke as she is struck; a wreck's smoke goes on until its loop ends)
				const float Seconds = E.bLoop ? static_cast<float>(FMath::Max(5.0, (E.Until - E.T) * S.Life)) : 4.0f;
				Effects->Burn(At, E.bLoop ? 1.2f : 0.8f, Seconds, E.Key + 100000);
			}
			else if (!Unknown.Contains(Fx))
			{
				Unknown.Add(Fx);
				UE_LOG(LogTheater, Display, TEXT("Land battle: the tanks' effect %s is not drawn"), *Fx);
			}
		}
		// Her own sounds that the mix does not make: hatches, the engine starting, brakes.
		if (!E.Cue.empty() && !E.bLoop)
		{
			const FString Cue = LandParts::Wide(E.Cue);
			if (Cue.Contains(TEXT("hatch")) || Cue.Contains(TEXT("start")) || Cue.Contains(TEXT("brake")) || Cue.Contains(TEXT("squeal")) || Cue.Contains(TEXT("breech")))
			{
				// (land.bodyCues may name another take for it, or none: field_gun_breech has no recording, and the
				// gun's case already clangs out after its shot)
				const std::string Take = S.Mix.CueTake(E.Cue);
				if (!Take.empty())
				{
					StartTake(*LandParts::Wide(Take), At, true, TEXT("engines"), LandParts::Gain(BusDb(TEXT("loops"))), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
				}
			}
		}
	}
	S.Armour->Events.clear();
}

void ALandBattle::PlayMenEvents()
{
	LAND_SCOPE(MenEvents);
	FLandBattleState& S = *State;
	if (!S.Men)
	{
		return;
	}
	// The men's clips' own sounds (footsteps, a knee down, kit, a bolt, a body landing), heard through the mix. Their
	// shots are the rounds', heard already.
	LandSound::Listener Ear;
	Ear.CameraHeight = S.Pose.Eye.Z - GroundZ(S.Pose.Eye.X, S.Pose.Eye.Y);
	Ear.At = Ear.CameraHeight < 4.0 ? S.Pose.Eye : LandPlay::Vec(S.Pose.Look.X, S.Pose.Look.Y, GroundZ(S.Pose.Look.X, S.Pose.Look.Y) + S.Mix.EarHeightFactor() * Ear.CameraHeight);
	for (const LandMen::Event& E : S.Men->Events())
	{
		if (E.Cue.empty() || E.Fx == "muzzle_flash" || E.Fx == "mortar")
		{
			continue;
		}
		S.Mix.HearBody(E.Cue, E.Army, LandPlay::Vec(E.At.X, E.At.Y, E.At.Z), S.Real, Ear, S.Takes);
	}
}

// ---- The sound -------------------------------------------------------------------------------------------------

void ALandBattle::PlaySound(double RealDt)
{
	LAND_SCOPE(Sound);
	FLandBattleState& S = *State;
	if (!Mix.IsValid())
	{
		return;
	}
	double Aspect = 16.0 / 9.0;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		Aspect = Size.Y > 0.0 ? Size.X / Size.Y : Aspect;
	}
	const LandEye::Pose Pose = S.Pose;
	S.Mix.OnScreen = [Pose, Aspect](const LandPlay::Vec& At) { return LandParts::InPicture(Pose, Aspect, At, 1.1); };
	LandSound::Listener Ear;
	Ear.CameraHeight = S.Pose.Eye.Z - GroundZ(S.Pose.Eye.X, S.Pose.Eye.Y);
	Ear.At = LandPlay::Vec(S.Pose.Look.X, S.Pose.Look.Y, GroundZ(S.Pose.Look.X, S.Pose.Look.Y) + S.Mix.EarHeightFactor() * FMath::Max(0.0, Ear.CameraHeight));
	if (Ear.CameraHeight < 4.0)
	{
		Ear.At = S.Pose.Eye;
	}
	Ear.Yaw = LandFeed::FacingTo(S.Pose.Eye.X, S.Pose.Eye.Y, S.Pose.Look.X, S.Pose.Look.Y);
	for (const LandPlay::Noise& N : S.Noises)
	{
		S.Mix.Hear(N, S.Real, S.Battle, FMath::Max(0.25 / S.Life, S.SpeedNow / S.Life), Ear, S.Takes);
	}
	// The beds by how much of the battle is fighting round the ear, and the ground the ear stands on.
	int32 Fighting = 0, Blocks = 0;
	if (const LandFeed::View* V = S.Rec.ViewAt(S.Battle))
	{
		for (const LandFeed::Unit& U : V->Units)
		{
			++Blocks;
			if (U.Act == "fight" && FMath::Sqrt(FMath::Square(U.X - Ear.At.X) + FMath::Square(U.Y - Ear.At.Y)) < 600.0)
			{
				++Fighting;
			}
		}
	}
	std::string Cover = "open";
	if (S.Field)
	{
		switch (S.Field->At(Ear.At.X, Ear.At.Y).What)
		{
		case LandField::Drawn::Woods: Cover = "woods"; break;
		case LandField::Drawn::Village: case LandField::Drawn::Yard: case LandField::Drawn::Floor: case LandField::Drawn::Wall: Cover = "buildings"; break;
		case LandField::Drawn::Water: Cover = "water"; break;
		default: break;
		}
	}
	const double Contact = S.Rec.Contact >= 0.0 ? S.Rec.Contact : 0.0;
	S.Mix.Tick(S.Real, RealDt, Ear, Fighting, Blocks > 0 ? static_cast<double>(Fighting) / Blocks : 0.0, S.Battle >= Contact, Cover, S.Beds);
	LiftDb = S.Mix.CloseDb(Ear.CameraHeight);
	StartTakes();
	if (RealT - SoundLogAt >= 20.0)
	{
		SoundLogAt = RealT;
		LogSound();
	}

	// The beds: one loop each, at the level the mix says.
	const float BedGain = LandParts::Gain(BusDb(TEXT("bed")));
	for (const LandSound::Bed& Bed : S.Beds)
	{
		const FName Name(LandParts::Wide(Bed.Name));
		TObjectPtr<UAudioComponent>* Found = BedPlayers.Find(Name);
		UAudioComponent* Player = Found ? Found->Get() : nullptr;
		if (Bed.bSilent)
		{
			if (Player && Player->IsPlaying())
			{
				Player->FadeOut(2.0f, 0.0f);
			}
			continue;
		}
		const float Volume = LandParts::Gain(Bed.Db) * BedGain;
		if (!Player)
		{
			USoundBase* Loop = SoundOf(Name, true);
			if (!Loop)
			{
				continue;
			}
			Player = UGameplayStatics::SpawnSound2D(this, Loop, Volume, 1.0f, 0.0f, nullptr, false, false);
			if (!Player)
			{
				continue;
			}
			BedPlayers.Add(Name, Player);
			// (a fade's level is a share of the player's own volume, already Volume: fading to Volume again played every
			// bed at twice its dB down, battle_loop's -4 as -8)
			Player->FadeIn(2.0f, 1.0f);
		}
		else if (!Player->IsPlaying())
		{
			Player->SetVolumeMultiplier(Volume);
			Player->FadeIn(2.0f, 1.0f);
		}
		else
		{
			Player->SetVolumeMultiplier(Volume);
		}
	}
	PlayEngines();

	// Takes that move with their shooter.
	for (int32 K = Moving.Num() - 1; K >= 0; --K)
	{
		UAudioComponent* Player = Moving[K].Player.Get();
		if (!Player || !Player->IsPlaying() || !StillHeld(Moving[K].Voice, Moving[K].Use))
		{
			Moving.RemoveAtSwap(K);
			continue;
		}
		const int32 Track = Moving[K].Track;
		LandPlay::Vec At;
		double Facing = 0.0;
		const size_t T = static_cast<size_t>(Track);
		if (T < S.VehicleOf.size() && S.VehicleOf[T] >= 0 && S.Armour)
		{
			const LandKit::Vehicle& V = S.Armour->Vehicles()[static_cast<size_t>(S.VehicleOf[T])];
			At = LandPlay::Vec(V.Root.At.X, V.Root.At.Y, V.Root.At.Z + 1.5);
		}
		else if (T < S.ManOf.size() && S.ManOf[T] >= 0 && S.Men)
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
			float F = 0.0f;
			S.Men->PlaceOf(S.ManOf[T], X, Y, Z, F);
			At = LandPlay::Vec(X, Y, Z + 1.2);
		}
		else if (T < S.Rec.Tracks.size())
		{
			const LandFeed::Spot Spot = LandFeed::SpotAt(S.Rec.Tracks[T], S.Battle);
			At = LandPlay::Vec(Spot.X, Spot.Y, GroundZ(Spot.X, Spot.Y) + 1.2);
			Facing = Spot.F;
		}
		(void)Facing;
		Player->SetWorldLocation(World(At.X, At.Y, At.Z));
	}

	// The ears dulled by a blast close by: down fast, held, then back.
	double Lpf = 0.0, Down = 0.0, Hold = 0.0, Back = 0.0, Db = 0.0;
	if (S.Mix.TakeShellShock(Lpf, Down, Hold, Back, Db))
	{
		ShockAt = RealT;
		ShockLpf = Lpf;
		ShockDown = Down;
		ShockHold = Hold;
		ShockBack = Back;
		ShockDb = Db;
	}
	const double Since = RealT - ShockAt;
	const double Whole = ShockDown + ShockHold + ShockBack;
	for (int32 K = Recent.Num() - 1; K >= 0; --K)
	{
		if (!Recent[K].Player.IsValid() || RealT - Recent[K].Started > 2.0 || !StillHeld(Recent[K].Voice, Recent[K].Use))
		{
			Recent.RemoveAtSwap(K);
		}
	}
	if (Since >= 0.0 && Since <= Whole + 0.1)
	{
		// The low-pass, from the open ear to the mix's and back, on a log scale.
		double Share = 0.0;
		if (Since < ShockDown)
		{
			Share = Since / FMath::Max(0.01, ShockDown);
		}
		else if (Since < ShockDown + ShockHold)
		{
			Share = 1.0;
		}
		else
		{
			Share = 1.0 - FMath::Clamp((Since - ShockDown - ShockHold) / FMath::Max(0.01, ShockBack), 0.0, 1.0);
		}
		const float Hz = static_cast<float>(FMath::Exp(FMath::Lerp(FMath::Loge(20000.0), FMath::Loge(FMath::Max(100.0, ShockLpf)), Share)));
		const bool bOn = Share > 0.001;
		auto Dull = [bOn, Hz](UAudioComponent* Player)
		{
			if (Player)
			{
				Player->SetLowPassFilterEnabled(bOn);
				Player->SetLowPassFilterFrequency(Hz);
			}
		};
		for (const FRecent& One : Recent)
		{
			Dull(One.Player.Get());
		}
		for (TPair<FName, TObjectPtr<UAudioComponent>>& Pair : BedPlayers)
		{
			Dull(Pair.Value.Get());
		}
		for (TPair<int32, TObjectPtr<UAudioComponent>>& Pair : Engines)
		{
			Dull(Pair.Value.Get());
		}
	}
}

void ALandBattle::LogSound() const
{
	// The sound's tally for the Mac run's log (the ground battle sound designer reads it): every shot heard against the
	// takes it started, none on the screen dropped, none heard late; the land rows at work.
	if (!State.IsValid())
	{
		return;
	}
	const FLandBattleState& S = *State;
	UE_LOG(LogTheater, Display, TEXT("Land battle sound: %d shots heard, %d takes started, %d dropped (%d on the screen), %d out of hearing; %d cracks (%d runs), %d rounds struck near the ear (%d whined off), %d bursts cut to their rounds, %d Garand clips (%d reloads), %d shouts, %d shells heard coming in; held at most %.2f s, %d on time at another pitch; lift down among the men %+.1f dB; meter %.2f"),
		S.Mix.ShotsHeard, S.Mix.ShotTakes, S.Mix.ShotsDropped, S.Mix.ShotsOnScreenDropped, S.Mix.ShotsOutOfHearing, S.Mix.Cracks, S.Mix.CrackRuns, S.Mix.GroundStrikes,
		S.Mix.Whines, S.Mix.BurstsCut, S.Mix.Clips, S.Mix.Reloads, S.Mix.Shouts, S.Mix.Incoming, S.Mix.MostDelay, S.Mix.Repitched, LiftDb, S.Mix.Meter());
	if (bPoolVoices)
	{
		// (land.voices: a take that gave way to another means land.voices.most is too few; players made during the
		// battle, past the warm ones, each cost a spawn)
		UE_LOG(LogTheater, Display, TEXT("Land battle sound players: %d takes on %d pooled players (%d made during the battle, at most %d), %d sounding now, the most at once %d; %d gave way to a new take; %d cuts faded; %d short takes out of hearing where they start, no voice (as the spawn); engine names worked out %d times for %d asked"),
			S.Voices.Taken, S.Voices.Count(), S.Voices.MadeLate, S.Mix.VoicesMost, S.Voices.BusyNow, S.Voices.MostBusy, S.Voices.Stolen, S.Voices.Faded, VoicesUnheard, S.EngineNames.Built, S.EngineNames.Asked);
	}
}

void ALandBattle::StartTakes()
{
	FLandBattleState& S = *State;
	// (the pool's stopped players were freed at the top of the frame's sound, before the tanks' own beats)
	// This frame's takes: each now, or held for its flash-to-bang and the 0.15 s rule.
	for (const LandSound::Take& T : S.Takes)
	{
		if (T.Delay > 0.0)
		{
			S.Later.emplace_back(RealT + T.Delay, T);
		}
		else
		{
			S.Later.emplace_back(RealT, T);
		}
	}
	S.Takes.clear();
	const double ShockLeft = ShockDown + ShockHold + ShockBack - (RealT - ShockAt);
	size_t Kept = 0;
	for (size_t K = 0; K < S.Later.size(); ++K)
	{
		if (S.Later[K].first > RealT + 1e-6)
		{
			if (Kept != K)
			{
				S.Later[Kept] = std::move(S.Later[K]);
			}
			++Kept;
			continue;
		}
		const LandSound::Take& T = S.Later[K].second;
		const float Volume = LandParts::Gain(T.Db + BusDb(LandParts::Wide(T.Bus)));
		UAudioComponent* Player = StartTake(*LandParts::Wide(T.Name), World(T.At.X, T.At.Y, T.At.Z), T.bSpatial, *LandParts::Wide(T.Family), Volume,
			static_cast<float>(T.Pitch), static_cast<float>(T.From), static_cast<float>(T.Cut), static_cast<float>(T.CutFade), static_cast<float>(T.FadeIn));
		if (Player)
		{
			if (T.Track >= 0)
			{
				Moving.Add({ Player, T.Track, LastVoice, LastVoiceUse });
			}
			Recent.Add({ Player, RealT, LastVoice, LastVoiceUse });
			if (ShockLeft > 0.0)
			{
				Player->SetLowPassFilterEnabled(true);
				Player->SetLowPassFilterFrequency(static_cast<float>(ShockLpf));
			}
		}
	}
	S.Later.resize(Kept);
	// (and the cuts due now fade out, each on the frame it falls due: one timer a take before)
	FadeVoices();
}

UAudioComponent* ALandBattle::StartTake(const TCHAR* Name, const FVector& Where, bool bSpatial, const TCHAR* Family, float Volume, float Pitch, float From, float Cut, float CutFade, float FadeIn)
{
	LastVoice = -1;
	LastVoiceUse = 0;
	const FName Take(Name);
	USoundBase* Played = SoundOf(Take);
	if (!Played)
	{
		return nullptr;
	}
	if (bPoolVoices && State.IsValid())
	{
		return StartPooled(Played, Take, Where, bSpatial, Family, Volume, Pitch, From, Cut, CutFade, FadeIn);
	}
	// (land.voices off: a player spawned for this take alone, gone when it ends, its cut faded by a timer)
	UAudioComponent* Player = bSpatial
		? UGameplayStatics::SpawnSoundAtLocation(this, Played, Where, FRotator::ZeroRotator, Volume, Pitch, From, FamilyOf(FName(Family)))
		: UGameplayStatics::SpawnSound2D(this, Played, Volume, Pitch, From);
	if (!Player)
	{
		return nullptr;
	}
	if (FadeIn > 0.0f)
	{
		// (a share of the volume it was spawned at: 1 is that volume)
		Player->FadeIn(FadeIn, 1.0f, From);
	}
	if (Cut > 0.0f)
	{
		TWeakObjectPtr<UAudioComponent> Weak(Player);
		const float Fade = FMath::Max(0.02f, CutFade);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Fade]()
		{
			if (Weak.IsValid())
			{
				Weak->FadeOut(Fade, 0.0f);
			}
		}), Cut, false);
	}
	return Player;
}

UAudioComponent* ALandBattle::StartPooled(USoundBase* Played, const FName& Take, const FVector& Where, bool bSpatial, const TCHAR* Family, float Volume, float Pitch, float From, float Cut, float CutFade, float FadeIn)
{
	FLandBattleState& S = *State;
	USoundAttenuation* const Atten = bSpatial ? FamilyOf(FName(Family)) : nullptr;
	// (UGameplayStatics' spawn makes no player for a short take, a second or less, that is out of hearing where it
	// starts: FAudioDevice::CreateComponent asks the device as here. The pool takes no voice for one either, as the old
	// path had no player: the same takes start either way)
	if (bSpatial && Played->GetDuration() <= 1.0f)
	{
		UWorld* const Here = GetWorld();
		if (FAudioDevice* const Device = Here ? Here->GetAudioDeviceRaw() : nullptr)
		{
			const FSoundAttenuationSettings* const Reach = Atten ? &Atten->Attenuation : Played->GetAttenuationSettingsToApply();
			float MaxDistance = 0.0f, FocusFactor = 0.0f;
			Device->GetMaxDistanceAndFocusFactor(Played, Here, Where, Reach, MaxDistance, FocusFactor);
			if (!Device->SoundIsAudible(Played, Here, Where, Reach, MaxDistance, FocusFactor))
			{
				++VoicesUnheard;
				return nullptr;
			}
		}
	}
	// A voice from the pool (LandSound::Voices): a free one, or a new one up to land.voices.most, or with every one
	// sounding the one nearest its end. Its player is set up as UGameplayStatics' spawn would set up a new one.
	const float* Known = TakeSeconds.Find(Take);
	const double Long = Known ? static_cast<double>(*Known) : static_cast<double>(FMath::Max(0.0f, Played->GetDuration()));
	const double Fade = Cut > 0.0f ? static_cast<double>(FMath::Max(0.02f, CutFade)) : 0.0;
	bool bMade = false;
	const int32 Voice = S.Voices.Take(RealT, FMath::Max(0.0, Long - From), Pitch, Cut, Fade, bMade);
	while (VoicePlayers.Num() <= Voice)
	{
		VoicePlayers.Add(MakeVoice());
	}
	UAudioComponent* Player = VoicePlayers[Voice].Get();
	if (!Player)
	{
		Player = MakeVoice();
		VoicePlayers[Voice] = Player;
	}
	if (Player->IsPlaying())
	{
		// (given way: every voice was sounding, and this one was nearest its end)
		Player->Stop();
	}
	Player->SetSound(Played);
	// (SpawnSoundAtLocation: the family's attenuation, spatialised; SpawnSound2D: neither, a UI sound)
	Player->AttenuationSettings = Atten;
	Player->bAllowSpatialization = bSpatial;
	Player->bIsUISound = !bSpatial;
	Player->SetVolumeMultiplier(Volume);
	Player->SetPitchMultiplier(Pitch);
	// (a voice keeps its last take's dulling: StartTakes dulls it again while a blast's is on)
	Player->SetLowPassFilterEnabled(false);
	if (bSpatial)
	{
		Player->SetWorldLocation(Where);
	}
	if (FadeIn > 0.0f)
	{
		Player->FadeIn(FadeIn, 1.0f, From);
	}
	else
	{
		Player->Play(From);
	}
	LastVoice = Voice;
	LastVoiceUse = S.Voices.UseOf(Voice);
	return Player;
}

UAudioComponent* ALandBattle::MakeVoice()
{
	// One of the pool's players: made once, never destroyed with its take, kept until the scene ends.
	UAudioComponent* Player = NewObject<UAudioComponent>(this, NAME_None, RF_Transient);
	Player->bAutoActivate = false;
	Player->bAutoDestroy = false;
	Player->bStopWhenOwnerDestroyed = true;
	Player->RegisterComponent();
	return Player;
}

void ALandBattle::FreeVoices()
{
	if (!bPoolVoices || !State.IsValid())
	{
		return;
	}
	LandSound::Voices& Pool = State->Voices;
	const int32 Count = FMath::Min(VoicePlayers.Num(), Pool.Count());
	for (int32 K = 0; K < Count; ++K)
	{
		if (Pool.At(K).bBusy)
		{
			const UAudioComponent* Player = VoicePlayers[K].Get();
			if (!Player || !Player->IsPlaying())
			{
				Pool.Free(K);
			}
		}
	}
}

void ALandBattle::FadeVoices()
{
	if (!bPoolVoices || !State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	S.VoiceFades.clear();
	S.Voices.Due(RealT, S.VoiceFades);
	for (const int Voice : S.VoiceFades)
	{
		if (Voice < VoicePlayers.Num())
		{
			if (UAudioComponent* Player = VoicePlayers[Voice].Get())
			{
				Player->FadeOut(static_cast<float>(S.Voices.FadeOf(Voice)), 0.0f);
			}
		}
	}
}

bool ALandBattle::StillHeld(int32 Voice, int32 Use) const
{
	return Voice < 0 || (State.IsValid() && State->Voices.Holds(Voice, Use));
}

void ALandBattle::PlayEngines()
{
	FLandBattleState& S = *State;
	if (!S.Armour)
	{
		return;
	}
	// Each vehicle's engine: the loop her core says (idling, driving), or her model's own for it (land.engines), on the
	// loops' bus, moving with her; gone with her.
	// Louder down among the men (land.closeView), as every other world sound.
	const float Volume = LandParts::Gain(BusDb(TEXT("loops")) + LiftDb);
	const std::vector<LandKit::Vehicle>& All = S.Armour->Vehicles();
	for (int32 K = 0; K < static_cast<int32>(All.size()); ++K)
	{
		const LandKit::Vehicle& V = All[static_cast<size_t>(K)];
		// (a wreck burning crackles where her engine ran)
		const bool bFire = V.bDead && V.bBurning && V.Fade >= 0.5;
		// (her loop's name worked out only when her cue changes, LandSound::EngineNames: an FName a vehicle a frame before)
		static const FName FireLoop(TEXT("fire_loop"));
		const int32 Named = bFire || V.bDead || V.Fade < 0.5 ? -1 : S.EngineNames.Of(S.Mix, K, V.Look().Name, V.EngineCue);
		while (EngineLoopNames.Num() < static_cast<int32>(S.EngineNames.Names().size()))
		{
			EngineLoopNames.Add(FName(LandParts::Wide(S.EngineNames.Names()[static_cast<size_t>(EngineLoopNames.Num())])));
		}
		const FName Cue = bFire ? FireLoop : Named >= 0 ? EngineLoopNames[Named] : FName(NAME_None);
		TObjectPtr<UAudioComponent>* Found = Engines.Find(K);
		UAudioComponent* Player = Found ? Found->Get() : nullptr;
		const FName* Was = EngineCue.Find(K);
		if (Player && (!Was || *Was != Cue))
		{
			Player->FadeOut(0.6f, 0.0f);
			Engines.Remove(K);
			EngineCue.Remove(K);
			Player = nullptr;
		}
		if (Cue.IsNone())
		{
			continue;
		}
		const FVector At = World(V.Root.At.X, V.Root.At.Y, V.Root.At.Z + 1.2);
		if (!Player)
		{
			USoundBase* Loop = SoundOf(Cue, true);
			if (!Loop)
			{
				continue;
			}
			Player = UGameplayStatics::SpawnSoundAtLocation(this, Loop, At, FRotator::ZeroRotator, Volume, 1.0f, 0.0f, FamilyOf(bFire ? TEXT("fire") : TEXT("engines")), nullptr, false);
			if (!Player)
			{
				continue;
			}
			// (a share of the volume it was spawned at: fading to Volume again played every engine at twice the loops
			// bus's -6 dB)
			Player->FadeIn(0.6f, 1.0f);
			Engines.Add(K, Player);
			EngineCue.Add(K, Cue);
		}
		else
		{
			Player->SetVolumeMultiplier(Volume);
		}
		Player->SetWorldLocation(At);
	}
}

USoundBase* ALandBattle::SoundOf(const FName& Name, bool bLoop)
{
	if (TObjectPtr<USoundBase>* Found = Sounds.Find(Name))
	{
		return Found->Get();
	}
	if (SoundsMissing.Contains(Name))
	{
		return nullptr;
	}
	// The battle pack's recordings, brought in by Tools/build_sounds.sh from Sounds/Real to /Game/Campaign/Sounds/Real.
	const FString Text = Name.ToString();
	USoundBase* Made = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Campaign/Sounds/Real/%s.%s"), *Text, *Text), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Made)
	{
		SoundsMissing.Add(Name);
		UE_LOG(LogTheater, Display, TEXT("Land battle: the sound %s is not in /Game/Campaign/Sounds/Real (Tools/build_sounds.sh brings it in)"), *Text);
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

USoundAttenuation* ALandBattle::FamilyOf(const FName& Name)
{
	if (TObjectPtr<USoundAttenuation>* Found = Families.Find(Name))
	{
		return Found->Get();
	}
	// A distance family of the mix: full loudness to `full` metres, nothing past `silent`, the top taken off with
	// distance by its low-pass points (the first of them near, as the director asked of the field).
	const TSharedPtr<FJsonObject> Family = LandParts::MixObject(Mix, TEXT("distance.") + Name.ToString());
	if (!Family.IsValid())
	{
		return nullptr;
	}
	double Full = 25.0, Silent = 700.0;
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
	Families.Add(Name, Made);
	return Made;
}

double ALandBattle::BusDb(const FString& Bus) const
{
	// (read from the mix's tree once a bus: it was a path through the tree for every take)
	if (const double* Known = BusDbs.Find(Bus))
	{
		return *Known;
	}
	return BusDbs.Add(Bus, LandParts::MixNumber(Mix, TEXT("buses.") + Bus + TEXT(".gainDb"), 0.0));
}

// ---- The picture run -------------------------------------------------------------------------------------------

void ALandBattle::TakeShots()
{
	LAND_SCOPE(TakeShots);
	if (ShotFolder.IsEmpty() || QuitAt >= 0.0)
	{
		return;
	}
	FLandBattleState& S = *State;
	// (a still asked only once the one before it is taken: Sedan asked 05 before 06 was taken, and the second ask
	// overrode the first. The shots are sorted by their time; the camera director, 22 November)
	bool bBeforeTaken = true;
	for (FShot& Shot : Shots)
	{
		if (Shot.bTaken)
		{
			continue;
		}
		// (each asked of the camera two battle seconds before it is taken, so that the cut has settled; the village's
		// asks for a house in the frame)
		if (!Shot.bAsked && Shot.Kind >= 0 && S.Battle >= Shot.At - 2.0 && bBeforeTaken)
		{
			Shot.bAsked = true;
			S.Eye.Ask(static_cast<LandEye::Kind>(Shot.Kind), Shot.Subject, Shot.Name == TEXT("08-the-village"));
		}
		if (S.Battle >= Shot.At)
		{
			Shot.bTaken = true;
			// (a still whose asked shot found no fit place and was placed anyway, one the film's own rules would have
			// turned away, says so in its name: the camera director's AskedUnfit, for the art director's read)
			const bool bUnfit = Shot.Kind >= 0 && S.Eye.AskedUnfit();
			// (and the village's still says so when no house is in it)
			const bool bNoHouse = Shot.Name == TEXT("08-the-village") && !S.Eye.HouseInShot();
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, Key + TEXT("-") + Shot.Name + (bNoHouse ? TEXT("-no-house") : TEXT("")) + (bUnfit ? TEXT("-unfit") : TEXT("")) + TEXT(".png")), true, false);
			++ShotsTaken;
			UE_LOG(LogTheater, Display, TEXT("Land battle: picture %s at %.1f s, %s shot on %d%s"), *Shot.Name, S.Battle, ANSI_TO_TCHAR(LandEye::NameOf(S.Pose.Shot)), S.Pose.Subject,
				bUnfit ? TEXT(" (no fit place found for it: placed anyway; the film would have turned it away)") : TEXT(""));
		}
		bBeforeTaken = Shot.bTaken;
	}
	if (S.bCard && !bCardShot)
	{
		bCardShot = true;
		CardShotAt = RealT + 1.0;
	}
	if (bCardShot && CardShotAt >= 0.0 && RealT >= CardShotAt)
	{
		CardShotAt = -1.0;
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, Key + TEXT("-10-after-card.png")), true, false);
		++ShotsTaken;
		QuitAt = RealT + 2.0;
	}
	if (RealT > 1200.0)
	{
		QuitAt = RealT + 1.0;
	}
}

// ---- The screen and the end ------------------------------------------------------------------------------------

void ALandBattle::ShowScreen()
{
	if (!GEngine || !GEngine->GameViewport || Screen.IsValid())
	{
		return;
	}
	Screen = SNew(SLandBattleScreen).Battle(this);
	// (the words a quarter larger than the game screen's own size, as the air battle's: "-TheaterLandTextScale=")
	float Lettering = 1.25f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandTextScale="), Lettering);
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

void ALandBattle::HideScreen()
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
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Controller->SetInputMode(Mode);
		}
	}
}

void ALandBattle::Finish()
{
	// CONTINUE: the scene taken down and the table shown again.
	// (a campaign battle he fought himself: its result taken now, while the rules' thread is still open, for after)
	TFunction<void(const FString&)> Hand = MoveTemp(HandBack);
	HandBack = nullptr;
	FString Handed;
	if (Hand && !LiveResult(Handed))
	{
		Handed.Empty();
	}
	HideScreen();
	if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (ACampaignHUD* Hud = Cast<ACampaignHUD>(Controller->GetHUD()))
		{
			Hud->ShowFrame(true);
		}
		Controller->ClearAudioListenerOverride();
		Controller->SetViewTarget(Controller->GetPawn());
	}
	ShowTheTable();
	if (Hand)
	{
		Hand(Handed); // (the campaign books it and its attack card shows the result)
	}
	Destroy();
}

void ALandBattle::EndPlay(const EEndPlayReason::Type Reason)
{
	StopDarkRead();
	CloseLive();
	HideScreen();
	// (the map's own light trims again, and its sun's contact shadow)
	for (const TPair<FString, FString>& Kept : KeptTrims)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Kept.Key))
		{
			Variable->Set(*Kept.Value, ECVF_SetByCode);
		}
	}
	KeptTrims.Reset();
	if (KeptContactShadow >= 0.0f && BattleKey.IsValid())
	{
		BattleKey->ContactShadowLength = KeptContactShadow;
		BattleKey->ContactShadowLengthInWS = bKeptContactInWorld;
		BattleKey->MarkRenderStateDirty();
	}
	KeptContactShadow = -1.0f;
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
		ShowTheTable();
	}
	if (State.IsValid())
	{
		const FLandBattleState& S = *State;
		UE_LOG(LogTheater, Display, TEXT("Land battle: ended at %.0f s: %d rounds played, %d blasts paired; shots heard %d, takes %d, out of hearing %d, dropped %d (on the screen %d); cracks %d, landings %d, falls %d; %d cuts (%d left for a dark picture; the picture read %d times, %d frames back of %d asked)"),
			S.Battle, S.Play.RoundsPlayed(), S.Play.BlastsPaired(), S.Mix.ShotsHeard, S.Mix.ShotTakes, S.Mix.ShotsOutOfHearing, S.Mix.ShotsDropped, S.Mix.ShotsOnScreenDropped,
			S.Mix.Cracks, S.Mix.Landings, S.Mix.Falls, S.Eye.Cuts(), S.Eye.DarkCuts(), DarkReads, DarkBacks, DarkAsks);
		if (DarkAsks >= 8 && DarkBacks * 2 < DarkAsks)
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle: the dark reading was all but blind: %d frames back of %d asked, so the camera's dark rule had little or nothing to go on"), DarkBacks, DarkAsks);
		}
		LogSound();
	}
	if (Limiter)
	{
		UAudioMixerBlueprintLibrary::RemoveMasterSubmixEffect(this, Limiter);
		Limiter = nullptr;
	}
	for (TPair<FName, TObjectPtr<UAudioComponent>>& Pair : BedPlayers)
	{
		if (Pair.Value)
		{
			Pair.Value->Stop();
		}
	}
	for (TPair<int32, TObjectPtr<UAudioComponent>>& Pair : Engines)
	{
		if (Pair.Value)
		{
			Pair.Value->Stop();
		}
	}
	for (const TObjectPtr<UAudioComponent>& Player : VoicePlayers)
	{
		if (Player)
		{
			Player->Stop();
		}
	}
	if (Effects)
	{
		Effects->End();
		Effects.Reset();
	}
	if (Eye)
	{
		Eye->Destroy();
		Eye = nullptr;
	}
	Super::EndPlay(Reason);
}
