#include "Battle/LandBattle.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Async/ParallelFor.h"

#include "Battle/AirSky.h"
#include "Battle/LandBattleScreen.h"
#include "Battle/LandBattleState.h"
#include "Battle/LandCarpet.h"
#include "Battle/LandCrowd.h" // TK1
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
#include "Components/AudioComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShaderCompiler.h"
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
	static void Grade(FPostProcessSettings& Lens, const LandFeed::Sky* Sky = nullptr)
	{
		BattleLook::Grade(Lens);
		if (FParse::Param(FCommandLine::Get(), TEXT("TheaterNoBattleGrade")))
		{
			return;
		}
		// (and half a stop down: the art director on the third cut's field sheets, 22 November: the stop brings hay,
		// wheat, barley and meadow into 120-165 and keeps the contrast's shadows, where a softer contrast flattens them)
		double Darks = 0.45, Middles = 0.48, Red = 1.0, Blue = 1.02, Stops = -0.5;
		if (Sky && Sky->bGrade)
		{
			Darks = Sky->Grade[0];
			Middles = Sky->Grade[1];
			Red = Sky->Grade[2];
			Blue = Sky->Grade[3];
		}
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandGrade="), Asked))
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
		static bool bSaid = false;
		if (!bSaid)
		{
			bSaid = true;
			UE_LOG(LogTheater, Display, TEXT("Land battle look: the ground battle's grade over the battles' own: colour kept in the darks %.2f, the middle tones %.2f; red %.2f and blue %.2f; %.2f stops"),
				Darks, Middles, Red, Blue, Stops);
		}
	}

	static std::string Narrow(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		return std::string(Utf8.Get(), static_cast<size_t>(Utf8.Length()));
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
	 * engine takes it, its colours taken toward the burnt colour by Soot. */
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
				(*T)[I] = static_cast<int32>(M.Triangles[static_cast<size_t>(I)]);
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
				if (!S.Men->PlaceOf(Man, X, Y, Z, Facing))
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
	std::string Error;
	if (!LandFeed::ReadRecording(S.RecText, S.Rec, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: %s: %s"), *File, *LandParts::Wide(Error));
		return Fail();
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
		// (the same field the cloud test lays: LandSight::FieldOf; the hedges the men and vehicles cross opened there)
		LandField::Field Lay = LandSight::FieldOf(Rec);
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
		S.Field = std::make_unique<LandField::Ground>(Lay);
		S.Sight.Lay(S.Field.get());
	}
	const double FieldSeconds = LandParts::Seconds() - Took;
	const LandField::Ground* FieldPtr = S.Field.get();

	// The fire's world: the ground drawn, its word for the kit, the shooters' muzzles and where a round struck a hull.
	LandPlay::World PlayWorld;
	PlayWorld.Height = [FieldPtr](double X, double Y) { return FieldPtr->HeightAt(X, Y); };
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
			const double Ground = FieldPtr->At(X, Y).Z;
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
		// (the production lead's A/B: the whole mass's wide, the dearest shot until the men are instanced, turned off)
		if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoMassWide")))
		{
			Set.MassFrom = 0.0;
		}
		S.EyeWorld = EyeWorld;
		S.Eye.Begin(Rec, EyeWorld, Set);
	}
	S.Desk.Begin(Rec, 0);
	S.Mix.bThudFromClips = S.Men != nullptr;
	// A bang crosses the ground in life's seconds (Far / 343), not the battle's: the mix's flash-to-bang reads Life.
	S.Mix.Life = S.Life;

	// The effects kit: shells, guns, tracer, fires, the camera's jolt; the ground under a point is the ground drawn.
	Effects = MakeUnique<FBattleEffects>();
	if (!Effects->Begin(Root, UnitsPerMetre, [this, FieldPtr](const FVector& Point)
		{
			return FVector(Point.X, Point.Y, FieldPtr->At(Point.Y / UnitsPerMetre, Point.X / UnitsPerMetre).Z * UnitsPerMetre);
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
		Effects->SetCraters(false);
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
	// And the tracks' marks, on the same ground and paint (none across a dug hole's bowl).
	S.Ruts.Begin([FieldPtr](double X, double Y) { return FieldPtr->HeightAt(X, Y); }, [FieldPtr](double X, double Y) { return FieldPtr->ColourAt(X, Y); },
		LandCrater::Ruts::KindOn(*FieldPtr));
	{
		const LandCrater::Field* DugHoles = &S.Holes;
		S.Ruts.DugAt = [DugHoles](double X, double Y) { return DugHoles->InCut(X, Y); };
	}
	S.Ruts.Lit = GroundLit; // (each strip lit as the ground under it: a flat stripe no more)

	Took = LandParts::Seconds();
	LayField();
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
		LandParts::Grade(Lens->PostProcessSettings, &Rec.Weather);
		// The grain of the film stock, a little (the art director's to tune: "-TheaterLandGrain=0.3").
		float Grain = 0.25f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandGrain="), Grain);
		Lens->PostProcessSettings.bOverride_FilmGrainIntensity = true;
		Lens->PostProcessSettings.FilmGrainIntensity = FMath::Clamp(Grain, 0.0f, 1.0f);
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
	auto Lay = [&](const LandField::Mesh& M, UMaterialInstanceDynamic* Ink, bool bShadow) -> UProceduralMeshComponent*
	{
		if (M.Empty())
		{
			return nullptr;
		}
		LandParts::FromField(M, U, P, N, C, T);
		UProceduralMeshComponent* Mesh = LandParts::NewMesh(this, Root, FName(*FString::Printf(TEXT("LandGround%d"), Made++)), bShadow);
		Mesh->CreateMeshSection(0, P, T, N, NoUv, C, NoTangents, false);
		Mesh->SetMaterial(0, Ink);
		FieldMeshes.Add(Mesh);
		return Mesh;
	};
	auto AddTile = [&](const LandField::Tile& Tile)
	{
		FTileDrawn& D = Tiles.AddDefaulted_GetRef();
		D.Box = FBox2D(FVector2D(Tile.X0, Tile.Y0), FVector2D(Tile.X1, Tile.Y1));
		D.Near = Lay(Tile.Near, GroundInk, true);
		D.Far = Lay(Tile.Far, GroundInk, true);
		D.Built = Lay(Tile.Built, ThingInk, true);
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
		AddTile(Tile);
	}
	for (const LandField::Tile& Tile : G.Ring())
	{
		AddTile(Tile);
	}
	LayThings();
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
	double SaidShrubFar = 0.0, SaidTreeFar = 0.0;
	int32 Instances = 0, Meshes = 0;
	const double Began = LandParts::Seconds();
	TArray<FTransform> Places;
	for (int32 Kind = 0; Kind < Kinds; ++Kind)
	{
		const LandField::Base Base = static_cast<LandField::Base>(Kind);
		const bool bClutter = LandParts::IsClutter(Kind);
		const bool bShrub = Base == LandField::Base::ShrubA || Base == LandField::Base::ShrubB || Base == LandField::Base::ShrubC || Base == LandField::Base::Haycock;
		const bool bTree = Base == LandField::Base::Oak || Base == LandField::Base::Elm || Base == LandField::Base::Ash || Base == LandField::Base::Beech
			|| Base == LandField::Base::Pollard || Base == LandField::Base::Apple || Base == LandField::Base::Birch;
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
		}
		for (int32 Shade = 0; Shade < Shades; ++Shade)
		{
			// (a thing's shades are its brightness thirds alone, 1, 4 and 7; a clutter kind not in the sample, the middle)
			if (bClutter ? (Them.empty() && Shade != 4) : Number[Shade] == 0)
			{
				continue;
			}
			const FVector3f Tint = Number[Shade] > 0 ? Sum[Shade] / static_cast<float>(Number[Shade]) : Mean;
			const FString Name = FString::Printf(TEXT("LandThing_%s_%d"), *LandParts::Wide(Shape.Name), Shade);
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
			else if ((bShrub && ShrubsTo > 0.0f) || (bTree && TreesTo > 0.0f))
			{
				Copies->SetCullDistances(0, FMath::RoundToInt32((bTree ? TreesTo : ShrubsTo) * U));
			}
			Copies->RegisterComponent();
			if (bClutter)
			{
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
			for (const LandField::Placed* One : Them)
			{
				if (ShadeOf(Kind, One->Tint.R, One->Tint.G, One->Tint.B) == Shade)
				{
					Places.Add(LandParts::PlacedAt(*One, U));
				}
			}
			Copies->AddInstances(Places, false, true);
			Instances += Places.Num();
			ThingCopies.Add(Copies);
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
}

void ALandBattle::ShowField(const FVector& EyeWorld, double Battle)
{
	(void)Battle;
	const FVector2D EyeAt(EyeWorld.Y / UnitsPerMetre, EyeWorld.X / UnitsPerMetre);
	// The ground's level by the eye's distance to each tile (the things choose their own, by instancing).
	for (FTileDrawn& Tile : Tiles)
	{
		const double Far = FMath::Sqrt(Tile.Box.ComputeSquaredDistanceToPoint(EyeAt));
		const bool bNear = Far < Tile.FarFrom;
		if (bNear != Tile.bNearShown && Tile.Near.IsValid() && Tile.Far.IsValid())
		{
			Tile.bNearShown = bNear;
			// (once dug in, its parts stand in for its near mesh, which stays hidden)
			Tile.Near->SetVisibility(bNear && Tile.NearParts.IsEmpty());
			for (const TWeakObjectPtr<UProceduralMeshComponent>& Part : Tile.NearParts)
			{
				if (Part.IsValid())
				{
					Part->SetVisibility(bNear);
				}
			}
			Tile.Far->SetVisibility(!bNear);
		}
	}
	// The grass round the eye and round what it looks at: its squares coming into reach laid, those leaving it given
	// back, every frame (nothing to do while the eye stays within its squares).
	const FVector2D LookAt(State->Pose.Look.X, State->Pose.Look.Y);
	LayClutter(EyeAt, LookAt, State->Pose.bCut, EyeWorld.Z / UnitsPerMetre - GroundZ(EyeAt.X, EyeAt.Y));
}

void ALandBattle::LayClutter(const FVector2D& EyeAt, const FVector2D& LookAt, bool bCut, double EyeHigh)
{
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
			// (the places given back off the mesh's end, last first: nothing after them moves)
			TArray<int32> Gone;
			Gone.Reserve(Had[K] - Now[K]);
			for (int32 Slot = Had[K] - 1; Slot >= Now[K]; --Slot)
			{
				Gone.Add(Slot);
			}
			Mesh->RemoveInstances(Gone);
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
		return State->Field->At(X, Y).Z;
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
	FString ArmiesText;
	std::string Error;
	if (FFileHelper::LoadFileToString(ArmiesText, *(FPaths::ProjectContentDir() / TEXT("Data/LandBattles/armies.json"))))
	{
		if (!Crowd->ReadArmies(LandParts::Narrow(ArmiesText), Error))
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle: armies.json: %s"), *LandParts::Wide(Error));
		}
	}
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
	if (Crowd->PoseBankOn())
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: the pose bank: %d poses, %d sets of men alike out there, baked in %.0f ms"), Crowd->BankPoses(), Crowd->BankKeys(),
			Crowd->PoseBankMs());
	}
	// (six hundred men: a man away from the eye posed every few frames, his place, shots and falls every frame)
	Crowd->SetThrift(true);
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
			if (!Vat->Make(*Crowd))
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
	// (the banked men's meshes made as first needed: at most four a frame; a man whose mesh waits is skinned meanwhile)
	int32 Builds = 4;
	// (a man's mesh made again for a new level of detail rebuilds its whole draw: at most MenRebuildsMax a frame, the
	// battle-scale plan's stopgap; the rest keep their level a frame or more, a new man waits to be drawn)
	int32 Rebuilds = S.MenRebuildsMax;
	// (no man drawn under some 2 of the picture's lines, GS3, but in the camera's wide of the whole army, where the far
	// men are its crowd)
	const bool bMass = S.Eye.MassShot();
	for (int32 Man = 0; Man < Crowd.Count(); ++Man)
	{
		FManDrawn& D = MenDrawn[Man];
		const bool bShown = Crowd.Visible(Man) && Crowd.Fade(Man) >= 0.5f && !HiddenMan(Man) && (bMass || Crowd.SeenDistance(Man) <= S.MenCullFrom);
		double X = 0.0, Y = 0.0, Z = 0.0;
		float Facing = 0.0f;
		Crowd.PlaceOf(Man, X, Y, Z, Facing);
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
	TArray<FVector> P, N;
	TArray<FColor> C;
	TArray<int32> T;
	for (const FSkinJob& Job : Jobs)
	{
		const int32 Man = Job.Man;
		const LandMen::Lod Level = Job.Level;
		const int32 Points = Job.Points;
		FManDrawn& D = MenDrawn[Man];
		const float* Skinned = SkinAt.GetData() + Job.Offset;
		const float* Faced = SkinFacing.GetData() + Job.Offset;
		P.SetNumUninitialized(Points);
		N.SetNumUninitialized(Points);
		for (int32 I = 0; I < Points; ++I)
		{
			P[I] = FVector(Skinned[I * 3 + 1] * UnitsPerMetre, Skinned[I * 3] * UnitsPerMetre, Skinned[I * 3 + 2] * UnitsPerMetre);
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
	const LandKit::GroundFn GroundAt = [FieldPtr](double X, double Y, double& OutZ, LandKit::V3& OutNormal)
	{
		const LandField::Surface Under = FieldPtr->At(X, Y);
		OutZ = Under.Z;
		OutNormal = LandKit::V3(Under.NX, Under.NY, Under.NZ);
	};
	auto Armour = std::make_unique<LandKit::Battle>();
	Armour->Life = S.Life;
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
		Mesh->SetWorldLocationAndRotation(World(Place.At.X, Place.At.Y, Place.At.Z), FQuat(Place.Q.X, Place.Q.Y, Place.Q.Z, Place.Q.W));
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
			Copies->SetCullDistances(0, FMath::RoundToInt32(800.0 * UnitsPerMetre));
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
		const FVector At = World(V.Root.At.X, V.Root.At.Y, V.Root.At.Z);
		const FQuat Q(V.Root.Q.X, V.Root.Q.Y, V.Root.Q.Z, V.Root.Q.W);
		if (bShown == D.bShown && (!bShown || (At.Equals(D.At, 1.0) && Q.Equals(D.Q, 1e-4))))
		{
			continue;
		}
		D.bShown = bShown;
		D.At = At;
		D.Q = Q;
		ShadeCopies[D.Copy]->UpdateInstanceTransform(D.Slot, FTransform(Q, At, FVector(bShown ? 1.0 : 1e-3)), true, false, true);
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
		if (Crowd.Visible(Man) && Crowd.Fade(Man) >= 0.5f && Crowd.SeenDistance(Man) < 150.0f && !HiddenMan(Man) && !Crowd.IsRider(Man) && Crowd.PlaceOf(Man, X, Y, Z, Facing))
		{
			Kind = Crowd.Frozen(Man) ? 2 : Crowd.PlayingOf(Man).Lie > 0.5f ? 1 : 0;
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
		const FVector At = World(X, Y, Z);
		// (his heading is clockwise from north, the field's y: in Unreal's frame, x north and y east, that is a yaw)
		const float Yaw = FMath::RadiansToDegrees(Facing);
		if (At.Equals(D.At, 1.0) && (Kind != 1 || FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, D.Yaw)) < 1.0f))
		{
			continue;
		}
		D.At = At;
		D.Yaw = Yaw;
		FVector Up = FVector::UpVector;
		if (S.Field)
		{
			const LandField::Surface Under = S.Field->At(X, Y);
			Up = FVector(Under.NY, Under.NX, Under.NZ).GetSafeNormal(1e-6, FVector::UpVector);
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
	double HazeStart = 0.0, HazeDensity = 0.0069, HazeFalloff = 0.1, HazeMost = 0.98;
	LandLook::Asked(TEXT("TheaterLandHaze="), { &HazeStart, &HazeDensity, &HazeFalloff, &HazeMost });
	HazeFalloff = FMath::Max(0.0, HazeFalloff);
	// (a warm grey-blue on a fair day, the summer haze over the bocage, not the sea's blue; greyer under overcast)
	const FLinearColor HazeInk = FMath::Lerp(FLinearColor(0.62f, 0.66f, 0.70f), FLinearColor(0.50f, 0.52f, 0.54f), static_cast<float>(Day.Overcast));
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
			const FLinearColor Field = FLinearColor::FromSRGBColor(FColor(118, 116, 72)) * static_cast<float>(FMath::Clamp(Below, 0.0, 1.0));
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
					Sky->bLowerHemisphereIsBlack = false;
					Sky->LowerHemisphereColor = Field;
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
	double SkyBright = 1.3, SkyAir = 1.0, SkyDust = 0.6, HazeOver = 1.0;
	LandLook::Asked(TEXT("TheaterLandSky="), { &SkyBright, &SkyAir, &SkyDust, &HazeOver });
	const float SkyShine = static_cast<float>(FMath::Max(0.0, SkyBright) * (1.0 - 0.45 * Day.Overcast));
	TArray<USkyAtmosphereComponent*> Atmospheres;
	GetComponents(Atmospheres);
	for (USkyAtmosphereComponent* Atmosphere : Atmospheres)
	{
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
			LandParts::Grade(Over->Settings, State.IsValid() ? &State->Rec.Weather : nullptr);
			Day.Grade = Over;
		}
	}
	LightTheBattle();
	LaySky();
	UE_LOG(LogTheater, Display, TEXT("Land battle look: %s, cover %.2f (overcast %.2f), clouds from %.0f m; the sun %.0f degrees up; key %.2f, the sky's fill %.2f at %.0f K and the sky light %.2f, shaded open ground %.2f of sunlit in light before the grade; the haze from %.0f m, %.4f at the ground (%.0f m), falloff %.3f, at most %.2f; the sky %.2f bright, air %.2f, dust %.2f, haze over it %.2f (%d skies)"),
		Day.bDeck ? TEXT("a deck") : TEXT("cumulus"), Day.Cover, Day.Overcast, Day.Base, FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(TowardSun.Z, -1.0, 1.0))),
		Day.Key, Day.Fill, Day.FillKelvin, Day.SkyLight, Day.Shade, HazeStart, HazeDensity, GroundMean, HazeFalloff, HazeMost, SkyShine, SkyAir, SkyDust, HazeOver, Atmospheres.Num());
}

void ALandBattle::LightTheBattle()
{
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
	// The first turn, to an eye over the field's middle; DrawSky turns them as the battle's eye moves.
	const AirSky::Vec Drift = Day.Clouds.DriftAt(State->Battle);
	const double MidX = 0.5 * Rec.Ground.W, MidY = 0.5 * Rec.Ground.H;
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
	// The clouds carried by the wind on the battle's clock, and turned to the eye as far as it has moved (the near room
	// as often as its puffs' turn shows, the far room when a far puff's would: AirSky::Rooms). On this thread: from the
	// ground the nearest puff is 900 m off, so a near turn comes every few metres the eye moves, about a millisecond.
	// (Called before this frame's camera is aimed: the turn is the last frame's eye, a fraction of a degree.)
	if (!SkyDay.IsValid() || !State.IsValid() || !Eye)
	{
		return;
	}
	FLandSky& Day = *SkyDay;
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
		for (UProceduralMeshComponent* Mesh : { SkyDay->Far.Get(), SkyDay->Near.Get() })
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
		Map->SetActorHiddenInGame(false);
		Map->SetPutAway(false);
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

	// The clock, against life: four times life until the armies are about to meet (the approach, as the first two cuts
	// played it), then life; held while the after card is up. The battle's own clock runs Life times faster than life,
	// so the fight is played at 1 / Life of it (Brian, 21 November: "the tanks should be moving", as tanks do).
	// (a live battle: first what the rules have fought since the last frame; its contact unknown until it is made)
	if (Live.IsValid())
	{
		PumpLive();
	}
	const double Contact = S.Rec.Contact >= 0.0 ? S.Rec.Contact : Live.IsValid() && !S.Rec.bOver ? 1e9 : 0.0;
	if (S.bOwnSpeed)
	{
		S.Speed = S.Battle < Contact - 10.0 ? 2.0 * S.Life : 1.0;
	}
	S.SpeedNow = S.bCard ? 0.0 : S.Speed;
	const double From = S.Battle;
	S.Battle = FMath::Min(S.Rec.End + 8.0, S.Battle + Real * S.SpeedNow / S.Life);
	if (Live.IsValid())
	{
		S.Battle = LiveClock(From, S.Battle, Real);
	}
	S.Lived += Real * S.SpeedNow;
	S.Real += Real;

	// The cores, in order: the tanks and the men posed first (their muzzles and hulls are where the rounds start and
	// strike), then the fire, then the camera, then the words.
	double Mark = LandParts::Seconds();
	if (S.Armour)
	{
		S.Armour->Update(S.Battle);
	}
	ArmourMs += (LandParts::Seconds() - Mark) * 1000.0;
	Mark = LandParts::Seconds();
	if (S.Men)
	{
		// (the men's detail by how big the lens draws them, the usual 50 deg lens as one: a long lens's far men drawn as
		// the near ones they look; the battle-scale plan's GS3)
		const double Zoom = FMath::Tan(FMath::DegreesToRadians(25.0)) / FMath::Max(0.02, FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(S.Pose.Fov, 2.0, 120.0) * 0.5)));
		S.Men->SetEye(S.Pose.Eye.X, S.Pose.Eye.Y, S.Pose.Eye.Z, Zoom);
		// (the tank commanders on their tanks as just posed)
		RideTanks();
		S.Men->Update(S.Battle);
	}
	MenMs += (LandParts::Seconds() - Mark) * 1000.0;
	Mark = LandParts::Seconds();
	S.Fx.clear();
	S.Noises.clear();
	S.Losses.clear();
	S.Takes.clear();
	S.Play.Step(From, S.Battle, S.Fx, S.Noises, S.Losses);
	if (bShotHeld)
	{
		S.Eye.Hold(true);
	}
	LandParts::PlaceMovers(S);
	S.Pose = S.Eye.Update(S.Battle, S.Real, Real);
	if (Live.IsValid())
	{
		GeneralView(Real);
	}
	S.Words = S.Desk.Update(S.Battle, Real);
	CoresMs += (LandParts::Seconds() - Mark) * 1000.0;

	// The picture: the camera, the ground round it, the men and the tanks.
	AimEye(static_cast<float>(Real));
	if (Live.IsValid())
	{
		ShowGeneral();
	}
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
	ShowField(EyeWorld, S.Battle);
	FieldMs += (LandParts::Seconds() - Mark) * 1000.0;
	Mark = LandParts::Seconds();
	const int32 InPictureWas = MenInPicture;
	DrawMen(EyeWorld);
	DrawMenMs += (LandParts::Seconds() - Mark) * 1000.0;
	MostInPicture = FMath::Max(MostInPicture, MenInPicture - InPictureWas);
	Mark = LandParts::Seconds();
	DrawArmour(EyeWorld);
	DrawArmourMs += (LandParts::Seconds() - Mark) * 1000.0;
	DrawManShades();

	// What happens: the fire, the tanks' own beats, the men's; then the effects kit, and its jolt on the hand-held eye.
	PlayEffects();
	// (the shell holes' meshes are the field's: their time is counted in FieldMs)
	Mark = LandParts::Seconds();
	MakeCraters();
	MakeRuts();
	FieldMs += (LandParts::Seconds() - Mark) * 1000.0;
	PlayArmourEvents();
	PlayMenEvents();
	if (Effects && Eye)
	{
		int32 ViewW = 1600, ViewH = 900;
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D Size;
			GEngine->GameViewport->GetViewportSize(Size);
			ViewW = FMath::Max(1, static_cast<int32>(Size.X));
			ViewH = FMath::Max(1, static_cast<int32>(Size.Y));
		}
		Mark = LandParts::Seconds();
		const FBattleJolt Jolt = Effects->Tick(S.Lived, Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW, FLinearColor::White, ViewH);
		EffectsMs += (LandParts::Seconds() - Mark) * 1000.0;
		Eye->SetActorLocationAndRotation(Eye->GetActorLocation() + Jolt.Offset, Eye->GetActorRotation() + FRotator(0.0, 0.0, Jolt.RollDegrees));
	}
	Mark = LandParts::Seconds();
	PlaySound(Real);
	SoundMs += (LandParts::Seconds() - Mark) * 1000.0;

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
	TakeShots();

	// The log, every 10 s: the frames, the slowest, and what each part cost.
	const double FrameMs = (LandParts::Seconds() - FrameBegan) * 1000.0;
	SlowestFrame = FMath::Max(SlowestFrame, FrameMs);
	// (the card's own time for its last whole frame: what an A/B of the field's or the men's draw moves)
	GpuMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	++LogFrames;
	if (RealT - LogAt >= 10.0)
	{
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
		UE_LOG(LogTheater, Display, TEXT("Land battle: a frame's drawing: men %.2f ms, tanks %.2f, effects %.2f, sound %.2f; the card %.2f ms; men in the picture %.0f a frame (the most %d), meshes made again %.1f a frame, held at the old level %.1f; the crowd (SO1) %s: mid %.0f a frame, far %.0f; the mass shot's least size %.0f lines: %.0f men grown a frame, the most x%.1f; the ground's clutter %d places for %d laid (GS7)"),
			DrawMenMs / LogFrames, DrawArmourMs / LogFrames, EffectsMs / LogFrames, SoundMs / LogFrames, GpuMs / LogFrames, static_cast<double>(MenInPicture) / LogFrames, MostInPicture,
			static_cast<double>(MenRebuilt) / LogFrames, static_cast<double>(RebuildsPut) / LogFrames, Vat.IsValid() ? TEXT("on") : TEXT("off"),
			static_cast<double>(VatMid) / LogFrames, static_cast<double>(VatFar) / LogFrames, MassMinPx, static_cast<double>(MassGrown) / LogFrames, MostGrow, ClutterPlaces, ClutterLaid);
		LogAt = RealT;
		LogFrames = 0;
		SlowestFrame = CoresMs = MenMs = ArmourMs = FieldMs = 0.0;
		DrawMenMs = DrawArmourMs = EffectsMs = SoundMs = GpuMs = 0.0;
		MenInPicture = MenRebuilt = RebuildsPut = MostInPicture = 0;
		VatMid = VatFar = MassGrown = 0;
		MostGrow = 1.0;
	}
}

void ALandBattle::AimEye(float DeltaSeconds)
{
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
		Grade.bOverride_DepthOfFieldFocalDistance = true;
		Grade.bOverride_DepthOfFieldFstop = true;
		Grade.DepthOfFieldFocalDistance = static_cast<float>((To - From).Size());
		Grade.DepthOfFieldFstop = S.Pose.Shot == LandEye::Kind::Watch || S.Pose.Shot == LandEye::Kind::Track ? 2.8f : 11.0f;
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
			const float Seconds = Cal < 70.0 ? 60.0f : Cal < 85.0 ? 90.0f : 150.0f;
			Effects->SmokeScreen(At, Across, Seconds, Name.IsEmpty() ? FString(TEXT("canister")) : Name, -1);
			break;
		}
		}
	}
}

void ALandBattle::MakeCraters()
{
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
	}
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
	}
}

void ALandBattle::MakeRuts()
{
	FLandBattleState& S = *State;
	if (!S.Field || !S.Armour)
	{
		return;
	}
	// Each tank, lorry and towed gun that moves on the ground leaves her tracks' marks behind her (the middles of her
	// tracks are her dust sockets, at her tail): pressed grass, torn stubble or plough, and in standing corn the corn
	// between her tracks laid flat. A knocked-out tank, or one gone from the field, lays none; seen to jump (a seek), her
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
		if (S.Ruts.Drive(V.Index, V.DustLeft.X, V.DustLeft.Y, V.DustRight.X, V.DustRight.Y, Width))
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
	}
}

void ALandBattle::PlayArmourEvents()
{
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
		if (!Player || !Player->IsPlaying())
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
		if (!Recent[K].Player.IsValid() || RealT - Recent[K].Started > 2.0)
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
}

void ALandBattle::StartTakes()
{
	FLandBattleState& S = *State;
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
				Moving.Add({ Player, T.Track });
			}
			Recent.Add({ Player, RealT });
			if (ShockLeft > 0.0)
			{
				Player->SetLowPassFilterEnabled(true);
				Player->SetLowPassFilterFrequency(static_cast<float>(ShockLpf));
			}
		}
	}
	S.Later.resize(Kept);
}

UAudioComponent* ALandBattle::StartTake(const TCHAR* Name, const FVector& Where, bool bSpatial, const TCHAR* Family, float Volume, float Pitch, float From, float Cut, float CutFade, float FadeIn)
{
	USoundBase* Played = SoundOf(FName(Name));
	if (!Played)
	{
		return nullptr;
	}
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
		const FName Cue = bFire ? FName(TEXT("fire_loop")) : V.bDead || V.Fade < 0.5 || V.EngineCue.empty() ? NAME_None : FName(LandParts::Wide(S.Mix.EngineTake(V.Look().Name, V.EngineCue)));
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
	return LandParts::MixNumber(Mix, TEXT("buses.") + Bus + TEXT(".gainDb"), 0.0);
}

// ---- The picture run -------------------------------------------------------------------------------------------

void ALandBattle::TakeShots()
{
	if (ShotFolder.IsEmpty() || QuitAt >= 0.0)
	{
		return;
	}
	FLandBattleState& S = *State;
	for (FShot& Shot : Shots)
	{
		if (Shot.bTaken)
		{
			continue;
		}
		// (each asked of the camera two battle seconds before it is taken, so that the cut has settled)
		if (!Shot.bAsked && Shot.Kind >= 0 && S.Battle >= Shot.At - 2.0)
		{
			Shot.bAsked = true;
			S.Eye.Ask(static_cast<LandEye::Kind>(Shot.Kind), Shot.Subject);
		}
		if (S.Battle >= Shot.At)
		{
			Shot.bTaken = true;
			// (a still whose asked shot found no fit place and was placed anyway, one the film's own rules would have
			// turned away, says so in its name: the camera director's AskedUnfit, for the art director's read)
			const bool bUnfit = Shot.Kind >= 0 && S.Eye.AskedUnfit();
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, Key + TEXT("-") + Shot.Name + (bUnfit ? TEXT("-unfit") : TEXT("")) + TEXT(".png")), true, false);
			++ShotsTaken;
			UE_LOG(LogTheater, Display, TEXT("Land battle: picture %s at %.1f s, %s shot on %d%s"), *Shot.Name, S.Battle, ANSI_TO_TCHAR(LandEye::NameOf(S.Pose.Shot)), S.Pose.Subject,
				bUnfit ? TEXT(" (no fit place found for it: placed anyway; the film would have turned it away)") : TEXT(""));
		}
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
	Destroy();
}

void ALandBattle::EndPlay(const EEndPlayReason::Type Reason)
{
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
		UE_LOG(LogTheater, Display, TEXT("Land battle: ended at %.0f s: %d rounds played, %d blasts paired; shots heard %d, takes %d, out of hearing %d, dropped %d (on the screen %d); cracks %d, landings %d, falls %d; %d cuts"),
			S.Battle, S.Play.RoundsPlayed(), S.Play.BlastsPaired(), S.Mix.ShotsHeard, S.Mix.ShotTakes, S.Mix.ShotsOutOfHearing, S.Mix.ShotsDropped, S.Mix.ShotsOnScreenDropped,
			S.Mix.Cracks, S.Mix.Landings, S.Mix.Falls, S.Eye.Cuts());
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
