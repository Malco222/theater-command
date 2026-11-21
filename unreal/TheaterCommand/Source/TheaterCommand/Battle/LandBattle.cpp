#include "Battle/LandBattle.h"
#include "AudioMixerBlueprintLibrary.h"

#include "Battle/LandBattleScreen.h"
#include "Battle/LandBattleState.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignBattleLook.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignFlags.h"
#include "Campaign/CampaignHUD.h"
#include "Campaign/CampaignMapActor.h"
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
#include "SubmixEffects/AudioMixerSubmixEffectDynamicsProcessor.h"
#include "StaticMeshResources.h"
#include "TheaterCommand.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SDPIScaler.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace LandParts
{
	static FString Wide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
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
	static UStaticMesh* ThingMesh(UObject* Owner, const FName& Name, const LandField::BaseMesh& Base, const FVector3f& Tint, UMaterialInterface* Ink, double U)
	{
		const LandField::Mesh* Levels[2] = { &Base.Near, Base.Far.Empty() ? nullptr : &Base.Far };
		TArray<FMeshDescription> Descriptions;
		Descriptions.SetNum(Levels[1] ? 2 : 1);
		for (int32 L = 0; L < Descriptions.Num(); ++L)
		{
			const LandField::Mesh& M = *Levels[L];
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
			const int32 Count = static_cast<int32>(M.Points());
			D.ReserveNewVertices(Count);
			D.ReserveNewVertexInstances(Count);
			D.ReserveNewTriangles(static_cast<int32>(M.Tris()));
			const FPolygonGroupID Group = D.CreatePolygonGroup();
			Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Ink");
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
				Uvs.Set(Corner, 0, FVector2f::ZeroVector);
				Corners[I] = Corner;
			}
			for (size_t K = 0; K + 2 < M.Triangles.size(); K += 3)
			{
				const FVertexInstanceID Three[3] = { Corners[static_cast<int32>(M.Triangles[K])], Corners[static_cast<int32>(M.Triangles[K + 1])],
					Corners[static_cast<int32>(M.Triangles[K + 2])] };
				D.CreateTriangle(Group, TArrayView<const FVertexInstanceID>(Three, 3));
			}
		}
		UStaticMesh* Made = NewObject<UStaticMesh>(Owner, Name);
		Made->GetStaticMaterials().Add(FStaticMaterial(Ink, TEXT("Ink")));
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
		// The far level from where the field puts it: a level is chosen by the share of the screen the mesh's bounds
		// fill, which at a 90 degree lens is its radius over the distance (a long lens keeps the near level further).
		FStaticMeshRenderData* Drawn = Made->GetRenderData();
		if (Levels[1] && Drawn && Drawn->LODResources.Num() > 1)
		{
			const double Reach = FMath::Max(1.0, static_cast<double>(Made->GetBounds().SphereRadius));
			Drawn->ScreenSize[1].Default = static_cast<float>(Reach / (FMath::Max(1.0f, Base.FarFrom) * U));
		}
		return Made;
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
	ManInk = Ink(0.85f, 0.2f, 0.0f);
	SteelInk = Ink(0.6f, 0.35f, 0.0f);
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
		USubmixEffectDynamicsProcessorPreset* Made = NewObject<USubmixEffectDynamicsProcessorPreset>(this);
		FSubmixEffectDynamicsProcessorSettings Settings;
		Settings.DynamicsProcessorType = ESubmixEffectDynamicsProcessorType::Limiter;
		Settings.PeakMode = ESubmixEffectDynamicsPeakMode::Peak;
		Settings.ThresholdDb = static_cast<float>(LandParts::MixNumber(Mix, TEXT("buses.master.limiterCeilingDb"), -1.5));
		Settings.LookAheadMsec = 3.0f;
		Settings.AttackTimeMsec = 1.0f;
		Settings.ReleaseTimeMsec = static_cast<float>(LandParts::MixNumber(Mix, TEXT("buses.master.limiterReleaseMs"), 120.0));
		Settings.InputGainDb = 0.0f;
		Settings.OutputGainDb = 0.0f;
		Made->SetSettings(Settings);
		Limiter = Made;
		UAudioMixerBlueprintLibrary::AddMasterSubmixEffect(this, Limiter);
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
		LandField::Field F;
		const LandFeed::Field& G = Rec.Ground;
		F.Cols = G.Cols;
		F.Rows = G.Rows;
		F.Cell = G.Cell;
		F.Grounds = G.Grounds;
		F.Ground.assign(G.Ground.begin(), G.Ground.end());
		F.Height.assign(G.Height.begin(), G.Height.end());
		F.Storeys.assign(G.Storeys.begin(), G.Storeys.end());
		F.bTown = G.TownR > 0.0;
		F.TownX = G.TownX;
		F.TownY = G.TownY;
		F.TownR = G.TownR;
		F.Terrain = G.Terrain.empty() ? std::string("plains") : G.Terrain;
		F.Month = LandParts::MonthOf(Rec.Date);
		F.Seed = Rec.Seed ? Rec.Seed : 1;
		// (where the men and the vehicles go: a hedge they cross is opened there)
		for (const LandFeed::Track& T : Rec.Tracks)
		{
			if (T.bTwin || T.At.size() < 2)
			{
				continue;
			}
			LandField::Field::Way& W = F.Ways.emplace_back();
			W.HalfWidth = T.Role == "tank" ? 1.8 : T.Role == "gun" ? 1.4 : T.Role == "lorry" ? 1.3 : 0.6;
			for (const LandFeed::Point& P : T.At)
			{
				W.Points.emplace_back(P.X, P.Y);
			}
		}
		S.Field = std::make_unique<LandField::Ground>(F);
	}
	const double FieldSeconds = LandParts::Seconds() - Took;
	const LandField::Ground* FieldPtr = S.Field.get();

	// The fire's world: the ground drawn, its word for the kit, the shooters' muzzles and where a round struck a hull.
	LandPlay::World PlayWorld;
	PlayWorld.Height = [FieldPtr](double X, double Y) { return FieldPtr->At(X, Y).Z; };
	PlayWorld.GroundWord = [FieldPtr](double X, double Y) { return std::string(FieldPtr->At(X, Y).Fx); };

	// The men (LandMen) and the tanks (LandKit), each reading the recording for itself.
	Took = LandParts::Seconds();
	MakeMen();
	const double MenSeconds = LandParts::Seconds() - Took;
	Took = LandParts::Seconds();
	MakeArmour();
	const double ArmourSeconds = LandParts::Seconds() - Took;
	{
		TWeakPtr<FLandBattleState> Weak(State);
		PlayWorld.Muzzle = [Weak](int Track, double T, LandPlay::Vec& OutAt, LandPlay::Vec& OutFacing)
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
				if (V.SocketAt("muzzle", T, At, Facing))
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
	S.Play.Begin(Rec, PlayWorld);

	// The camera's world: the ground drawn, where it can stand, what it can see through, where each man and tank is.
	{
		LandEye::World EyeWorld;
		EyeWorld.Height = PlayWorld.Height;
		EyeWorld.Free = [FieldPtr](const LandPlay::Vec& At)
		{
			const LandField::Surface Here2 = FieldPtr->At(At.X, At.Y);
			if (Here2.What == LandField::Drawn::Floor || Here2.What == LandField::Drawn::Wall)
			{
				return false;
			}
			// (on a bank the hedge's shrubs stand to some 3 m: the eye is in them below that)
			return !(Here2.What == LandField::Drawn::Bank && At.Z < Here2.Z + 3.2);
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
				const LandField::Surface Here2 = FieldPtr->At(At.X, At.Y);
				double Top = Here2.Z;
				switch (Here2.What)
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
		LandEye::Settings Set;
		Set.Own = 0;
		Set.Seed = Rec.Seed ? Rec.Seed : 1;
		float Hand = 1.0f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandHand="), Hand);
		Set.Hand = FMath::Clamp(static_cast<double>(Hand), 0.0, 3.0);
		S.Eye.Begin(Rec, EyeWorld, Set);
	}
	S.Desk.Begin(Rec, 0);
	S.Mix.bThudFromClips = S.Men != nullptr;

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
		Effects->SetSun(TowardSun);
		// (a summer morning's breeze from the west-south-west, some 3 m a second: the smoke leans east over the field)
		Effects->SetWind(WorldWay(2.6, 1.2, 0.0));
	}

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
		BattleLook::Grade(Lens->PostProcessSettings);
		// The grain of the film stock, a little (the art director's to tune: "-TheaterLandGrain=0.3").
		float Grain = 0.25f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLandGrain="), Grain);
		Lens->PostProcessSettings.bOverride_FilmGrainIntensity = true;
		Lens->PostProcessSettings.FilmGrainIntensity = FMath::Clamp(Grain, 0.0f, 1.0f);
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
	if (APlayerController* Controller = Here->GetFirstPlayerController())
	{
		if (Eye)
		{
			Controller->SetViewTarget(Eye);
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
		return 1;
	}
	const float Bright = static_cast<float>(R) + static_cast<float>(G) + static_cast<float>(B);
	return Bright < ShadeEdges[Base].X ? 0 : Bright < ShadeEdges[Base].Y ? 1 : 2;
}

void ALandBattle::LayThings()
{
	const LandField::Ground& G = *State->Field;
	const double U = UnitsPerMetre;
	constexpr int32 Shades = 3;
	const int32 Kinds = static_cast<int32>(LandField::Base::Count);
	ShadeEdges.SetNum(Kinds);
	ClutterCopyOf.Init(-1, Kinds * Shades);
	// The clutter's shades from a sample of it, laid once over the field's middle (it is laid round the eye later).
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
		const double Mx = (X0 + X1) * 0.5, My = (Y0 + Y1) * 0.5;
		G.Clutter(Mx - 120.0, My - 120.0, Mx + 120.0, My + 120.0, Sample);
	}
	int32 Instances = 0, Meshes = 0;
	const double Began = LandParts::Seconds();
	TArray<FTransform> Places;
	for (int32 Kind = 0; Kind < Kinds; ++Kind)
	{
		const LandField::Base Base = static_cast<LandField::Base>(Kind);
		const bool bClutter = LandParts::IsClutter(Kind);
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
		// Three shades of the kind's tints, split by brightness into thirds; each shade painted with its own mean tint.
		std::vector<float> Bright;
		Bright.reserve(Them.size());
		for (const LandField::Placed* One : Them)
		{
			Bright.push_back(static_cast<float>(One->Tint.R) + One->Tint.G + One->Tint.B);
		}
		std::sort(Bright.begin(), Bright.end());
		ShadeEdges[Kind] = Bright.empty() ? FVector2f(-1.0f, 1e9f) : FVector2f(Bright[Bright.size() / 3], Bright[Bright.size() * 2 / 3]);
		FVector3f Sum[Shades] = { FVector3f::ZeroVector, FVector3f::ZeroVector, FVector3f::ZeroVector };
		int32 Number[Shades] = { 0, 0, 0 };
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
		if (Shape.Near.Empty())
		{
			continue;
		}
		for (int32 Shade = 0; Shade < Shades; ++Shade)
		{
			if (!bClutter && Number[Shade] == 0)
			{
				continue;
			}
			const FVector3f Tint = Number[Shade] > 0 ? Sum[Shade] / static_cast<float>(Number[Shade]) : Mean;
			const FString Name = FString::Printf(TEXT("LandThing_%s_%d"), *LandParts::Wide(Shape.Name), Shade);
			UStaticMesh* Mesh = LandParts::ThingMesh(this, FName(*(Name + TEXT("_Mesh"))), Shape, Tint, ThingInk, U);
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
			Copies->SetMaterial(0, ThingInk);
			Copies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Copies->SetCanEverAffectNavigation(false);
			Copies->SetCastShadow(!bClutter || Base == LandField::Base::Wheat);
			// (a thing with no far level is not drawn past where the field gives it up: gates, the well, the cart, the
			// clutter)
			if (Shape.Far.Empty())
			{
				Copies->SetCullDistances(0, FMath::RoundToInt32(Shape.FarFrom * U));
			}
			Copies->RegisterComponent();
			if (bClutter)
			{
				ClutterCopyOf[Kind * Shades + Shade] = ClutterCopies.Add(Copies);
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
	UE_LOG(LogTheater, Display, TEXT("Land battle: the ground's things: %d meshes, %d instances, made in %.2f s"), Meshes, Instances, LandParts::Seconds() - Began);
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
			Tile.Near->SetVisibility(bNear);
			Tile.Far->SetVisibility(!bNear);
		}
	}
	// The grass round the eye and round what it looks at, again when either has moved some metres.
	const FVector2D LookAt(State->Pose.Look.X, State->Pose.Look.Y);
	if (FVector2D::Distance(EyeAt, ClutterEyeAt) > 12.0 || FVector2D::Distance(LookAt, ClutterLookAt) > 12.0)
	{
		LayClutter(EyeAt, LookAt);
	}
}

void ALandBattle::LayClutter(const FVector2D& EyeAt, const FVector2D& LookAt)
{
	ClutterEyeAt = EyeAt;
	ClutterLookAt = LookAt;
	if (ClutterCopies.Num() == 0 || !State->Field)
	{
		return;
	}
	const LandField::Ground& G = *State->Field;
	std::vector<LandField::Placed> Things;
	G.Clutter(EyeAt.X - 45.0, EyeAt.Y - 45.0, EyeAt.X + 45.0, EyeAt.Y + 45.0, Things);
	// (round what the eye looks at too, when that is not where it stands: the long lens's men in the grass)
	if (FVector2D::Distance(EyeAt, LookAt) > 40.0)
	{
		std::vector<LandField::Placed> Round;
		G.Clutter(LookAt.X - 25.0, LookAt.Y - 25.0, LookAt.X + 25.0, LookAt.Y + 25.0, Round);
		for (const LandField::Placed& One : Round)
		{
			if (FMath::Abs(One.X - EyeAt.X) > 45.0 || FMath::Abs(One.Y - EyeAt.Y) > 45.0)
			{
				Things.push_back(One);
			}
		}
	}
	constexpr int32 Shades = 3;
	TArray<TArray<FTransform>> ByCopy;
	ByCopy.SetNum(ClutterCopies.Num());
	for (const LandField::Placed& One : Things)
	{
		const int32 Kind = static_cast<int32>(One.Kind);
		const int32 Slot = Kind * Shades + ShadeOf(Kind, One.Tint.R, One.Tint.G, One.Tint.B);
		const int32 Copy = ClutterCopyOf.IsValidIndex(Slot) ? ClutterCopyOf[Slot] : -1;
		if (Copy >= 0)
		{
			ByCopy[Copy].Add(LandParts::PlacedAt(One, UnitsPerMetre));
		}
	}
	for (int32 K = 0; K < ClutterCopies.Num(); ++K)
	{
		if (UInstancedStaticMeshComponent* Copies = ClutterCopies[K])
		{
			Copies->ClearInstances();
			if (ByCopy[K].Num() > 0)
			{
				Copies->AddInstances(ByCopy[K], false, true);
			}
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
	// (and each army's gunner's file where it has one: the American's BAR, soldier_USA_bar)
	for (const FString& Army : TArray<FString>(Armies.Array()))
	{
		Armies.Add(Army + TEXT("_bar"));
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
	const LandMen::GroundFn GroundAt = [FieldPtr](double X, double Y)
	{
		const LandField::Surface Under = FieldPtr->At(X, Y);
		LandMen::Ground Out;
		Out.Height = static_cast<float>(Under.Z);
		Out.Normal = LandMen::Vec3{ static_cast<float>(Under.NX), static_cast<float>(Under.NY), static_cast<float>(Under.NZ) };
		return Out;
	};
	if (Files == 0 || !Crowd->Build(S.RecText, GroundAt, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: the men could not be made (%d soldier files; %s): none are drawn."), Files, *LandParts::Wide(Error));
		return;
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
		const int Man = S.Men->Find(T.Block, T.K);
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
	TArray<FVector> P, N;
	TArray<FColor> C;
	TArray<int32> T;
	for (int32 Man = 0; Man < Crowd.Count(); ++Man)
	{
		FManDrawn& D = MenDrawn[Man];
		const bool bShown = Crowd.Visible(Man) && Crowd.Fade(Man) >= 0.5f;
		double X = 0.0, Y = 0.0, Z = 0.0;
		float Facing = 0.0f;
		Crowd.PlaceOf(Man, X, Y, Z, Facing);
		// (skinned only when in the picture, or close by: a man out of the picture keeps the pose he had)
		const LandPlay::Vec At(X, Y, Z + 0.9);
		const bool bLooked = (At - EyeAt).Length() < 6.0 || LandParts::InPicture(S.Pose, Aspect, At, 1.15);
		if (!bShown || !bLooked)
		{
			if (D.Mesh.IsValid() && !bShown)
			{
				D.Mesh->SetVisibility(false);
			}
			continue;
		}
		const LandMen::Lod Level = Crowd.LodOf(Man);
		const uint32 Stamp = Crowd.Stamp(Man);
		if (D.Mesh.IsValid() && D.Lod == static_cast<int32>(Level) && D.Stamp == Stamp)
		{
			D.Mesh->SetVisibility(true);
			continue;
		}
		const int32 Points = Crowd.PointsOf(Man, Level);
		SkinAt.SetNumUninitialized(Points * 3);
		SkinFacing.SetNumUninitialized(Points * 3);
		Crowd.Skin(Man, Level, SkinAt.GetData(), SkinFacing.GetData());
		P.SetNumUninitialized(Points);
		N.SetNumUninitialized(Points);
		for (int32 I = 0; I < Points; ++I)
		{
			P[I] = FVector(SkinAt[I * 3 + 1] * UnitsPerMetre, SkinAt[I * 3] * UnitsPerMetre, SkinAt[I * 3 + 2] * UnitsPerMetre);
			N[I] = FVector(SkinFacing[I * 3 + 1], SkinFacing[I * 3], SkinFacing[I * 3 + 2]);
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
			// A new level: its own triangles and colours (the same all battle, read once), then the skin.
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
		D.Stamp = Stamp;
		D.Mesh->SetVisibility(true);
	}
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
	std::string Error;
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
		const int Track = S.Rec.TrackIndexOf(W.Block, W.K);
		S.TrackOfVehicle[V] = Track;
		if (Track >= 0)
		{
			S.VehicleOf[static_cast<size_t>(Track)] = static_cast<int>(V);
		}
	}
	// One mesh a body: its near level as section 0 and its far as section 1, posed each frame.
	TArray<FVector> P, N;
	TArray<FColor> C;
	TArray<int32> T;
	for (size_t V = 0; V < S.Armour->Vehicles().size(); ++V)
	{
		const LandKit::Vehicle& Vehicle = S.Armour->Vehicles()[V];
		const LandKit::Model& Model = Vehicle.Look();
		for (size_t B = 0; B < Model.Bodies.size(); ++B)
		{
			const LandKit::Body& Body = Model.Bodies[B];
			if (Body.Near.Triangles.empty() && Body.Far.Triangles.empty())
			{
				continue;
			}
			UProceduralMeshComponent* Mesh = LandParts::NewMesh(this, Root, FName(*FString::Printf(TEXT("LandBody%d_%d"), static_cast<int32>(V), static_cast<int32>(B))), true);
			if (!Body.Near.Triangles.empty())
			{
				LandParts::FromBody(Body.Near, Model.BurntColour, 0.0, UnitsPerMetre, P, N, C, &T);
				Mesh->CreateMeshSection(0, P, T, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
				Mesh->SetMaterial(0, SteelInk);
			}
			if (!Body.Far.Triangles.empty())
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
		const bool bShown = V.Fade >= 0.5;
		Mesh->SetVisibility(bShown);
		if (!bShown)
		{
			continue;
		}
		const LandKit::Place& Place = V.Bodies()[static_cast<size_t>(D.Body)];
		Mesh->SetWorldLocationAndRotation(World(Place.At.X, Place.At.Y, Place.At.Z), FQuat(Place.Q.X, Place.Q.Y, Place.Q.Z, Place.Q.W));
		// The near level within 120 m (a long lens's Watch shot at 140 m keeps it), the far beyond.
		const double Far = (LandPlay::Vec(V.Root.At.X, V.Root.At.Y, V.Root.At.Z) - EyeAt).Length();
		const bool bNear = (Far < 140.0 && !Body.Near.Triangles.empty()) || Body.Far.Triangles.empty();
		if (bNear != D.bNear && !Body.Near.Triangles.empty() && !Body.Far.Triangles.empty())
		{
			D.bNear = bNear;
			Mesh->SetMeshSectionVisible(0, bNear);
			Mesh->SetMeshSectionVisible(1, !bNear);
		}
		// A track's links, posed for her run while she is near and moving.
		if (D.bTrack && D.bNear && V.bMoving && !Body.Near.Triangles.empty())
		{
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
		// The burning blackens her, a step at a time.
		if (V.Soot - D.SootShown > 0.05 || (V.Soot >= 1.0 && D.SootShown < 1.0))
		{
			D.SootShown = V.Soot;
			for (int32 Section = 0; Section < 2; ++Section)
			{
				const LandKit::Mesh& M = Section == 0 ? Body.Near : Body.Far;
				if (M.Triangles.empty() || (Section == 0 && D.bTrack && V.bMoving))
				{
					continue;
				}
				LandParts::FromBody(M, Model.BurntColour, V.Soot, UnitsPerMetre, P, N, C, nullptr);
				Mesh->UpdateMeshSection(Section, P, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>());
			}
		}
	}
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
			const FVector Velocity = WorldWay(V.Velocity.X, V.Velocity.Y, V.Velocity.Z) * UnitsPerMetre;
			const bool bTracked = V.Look().Kind == "tank";
			const FString Ground = GroundWordAt(V.Root.At.X, V.Root.At.Y);
			Effects->VehicleDust(static_cast<int32>(K) * 2, World(V.DustLeft.X, V.DustLeft.Y, V.DustLeft.Z), Velocity, Ground, bTracked);
			Effects->VehicleDust(static_cast<int32>(K) * 2 + 1, World(V.DustRight.X, V.DustRight.Y, V.DustRight.Z), Velocity, Ground, bTracked);
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
	// The haze of a summer morning in the bocage: clear near, the far hedgerows soft from a few hundred metres, the
	// horizon gone at three or four kilometres (the art director's field look: 2 to 4 km sight on land).
	// "-TheaterLandHaze=start metres,density,falloff,most" tries other numbers.
	double HazeStart = 250.0, HazeDensity = 0.0035, HazeFalloff = 0.02, HazeMost = 0.9;
	{
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandHaze="), Asked, false))
		{
			TArray<FString> Parts;
			Asked.ParseIntoArray(Parts, TEXT(","), false);
			double* const Into[] = { &HazeStart, &HazeDensity, &HazeFalloff, &HazeMost };
			for (int32 K = 0; K < Parts.Num() && K < 4; ++K)
			{
				if (!Parts[K].TrimStartAndEnd().IsEmpty())
				{
					*Into[K] = FCString::Atod(*Parts[K]);
				}
			}
		}
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
				Fog->SetStartDistance(static_cast<float>(HazeStart * UnitsPerMetre));
				Fog->SetFogDensity(static_cast<float>(HazeDensity));
				Fog->SetFogHeightFalloff(static_cast<float>(HazeFalloff));
				Fog->SetFogMaxOpacity(static_cast<float>(HazeMost));
				// (a warm grey-blue: the summer haze over the bocage, not the sea's blue)
				Fog->SetFogInscatteringColor(FLinearColor(0.62f, 0.66f, 0.70f));
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
				// (the sun a small hard disc, as the air battle's: the art director's ruling on build 1750)
				KeyLight->LightSourceAngle = 0.53f;
				KeyLight->AtmosphereSunDiskColorScale = FLinearColor(0.006f, 0.006f, 0.006f);
				KeyLight->MarkRenderStateDirty();
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
	}
	TArray<USkyAtmosphereComponent*> Atmospheres;
	GetComponents(Atmospheres);
	for (USkyAtmosphereComponent* Atmosphere : Atmospheres)
	{
		Atmosphere->SetMieAnisotropy(0.75f);
	}
	LightTheBattle();
}

void ALandBattle::LightTheBattle()
{
	// The battle's day, said again wherever the room has changed it: every lamp white, the key at the day's strength
	// and warmth and the others at the fill's, the sky's light the day's. A morning under broken cumulus: the key a
	// little under the air battle's, the sky's fill a little over.
	const double Cover = State.IsValid() ? FMath::Clamp(State->Rec.Weather.Cover, 0.0, 1.0) : 0.3;
	const float KeyWant = static_cast<float>(7.0 - 2.5 * Cover);
	const float SkyWant = static_cast<float>(0.72 + 0.25 * Cover);
	for (const FKeptLamp& Kept : KeptLamps)
	{
		UDirectionalLightComponent* Light = Kept.Lamp.Get();
		if (!Light)
		{
			continue;
		}
		const bool bKey = Light == BattleKey.Get();
		const float Want = bKey ? KeyWant : 0.75f;
		if (!Light->GetLightColor().Equals(FLinearColor::White, 0.002f))
		{
			Light->SetLightColor(FLinearColor::White);
		}
		if (!FMath::IsNearlyEqual(Light->Intensity, Want, 0.001f))
		{
			Light->SetIntensity(Want);
		}
		if (bKey && !FMath::IsNearlyEqual(Light->Temperature, 5200.0f, 1.0f))
		{
			Light->SetTemperature(5200.0f);
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
		if (!FMath::IsNearlyEqual(Sky->Intensity, SkyWant, 0.001f))
		{
			Sky->SetIntensity(SkyWant);
		}
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
}

void ALandBattle::ShowTheTable()
{
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
	FLandBattleState& S = *State;
	const double FrameBegan = LandParts::Seconds();

	// The clock: twice life until the armies are about to meet, then life; held while the after card is up.
	const double Contact = S.Rec.Contact >= 0.0 ? S.Rec.Contact : 0.0;
	if (S.bOwnSpeed)
	{
		S.Speed = S.Battle < Contact - 10.0 ? 2.0 : 1.0;
	}
	S.SpeedNow = S.bCard ? 0.0 : S.Speed;
	const double From = S.Battle;
	S.Battle = FMath::Min(S.Rec.End + 8.0, S.Battle + Real * S.SpeedNow);
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
		S.Men->SetEye(S.Pose.Eye.X, S.Pose.Eye.Y, S.Pose.Eye.Z);
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
	S.Pose = S.Eye.Update(S.Battle, S.Real, Real);
	S.Words = S.Desk.Update(S.Battle, Real);
	CoresMs += (LandParts::Seconds() - Mark) * 1000.0;

	// The picture: the camera, the ground round it, the men and the tanks.
	AimEye(static_cast<float>(Real));
	const FVector EyeWorld = Eye ? Eye->GetActorLocation() : World(S.Pose.Eye.X, S.Pose.Eye.Y, S.Pose.Eye.Z);
	Mark = LandParts::Seconds();
	ShowField(EyeWorld, S.Battle);
	FieldMs += (LandParts::Seconds() - Mark) * 1000.0;
	DrawMen(EyeWorld);
	DrawArmour(EyeWorld);

	// What happens: the fire, the tanks' own beats, the men's; then the effects kit, and its jolt on the hand-held eye.
	PlayEffects();
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
		const FBattleJolt Jolt = Effects->Tick(S.Battle, Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW, FLinearColor::White, ViewH);
		Eye->SetActorLocationAndRotation(Eye->GetActorLocation() + Jolt.Offset, Eye->GetActorRotation() + FRotator(0.0, 0.0, Jolt.RollDegrees));
	}
	PlaySound(Real);

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
	++LogFrames;
	if (RealT - LogAt >= 10.0)
	{
		const double Span = FMath::Max(0.001, RealT - LogAt);
		UE_LOG(LogTheater, Display, TEXT("Land battle: %.0f s of battle, %s shot; %.0f frames a second, the scene's slowest frame %.1f ms; a frame's cores %.2f ms, men %.2f, tanks %.2f, ground %.2f; shots heard %d, takes %d (dropped %d, on the screen %d), cracks %d, falls %d"),
			S.Battle, ANSI_TO_TCHAR(LandEye::NameOf(S.Pose.Shot)), LogFrames / Span, SlowestFrame, CoresMs / LogFrames, MenMs / LogFrames, ArmourMs / LogFrames, FieldMs / LogFrames,
			S.Mix.ShotsHeard, S.Mix.ShotTakes, S.Mix.ShotsDropped, S.Mix.ShotsOnScreenDropped, S.Mix.Cracks, S.Mix.Falls);
		LogAt = RealT;
		LogFrames = 0;
		SlowestFrame = CoresMs = MenMs = ArmourMs = FieldMs = 0.0;
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
		Grade.DepthOfFieldFstop = S.Pose.Shot == LandEye::Kind::Watch ? 2.8f : 11.0f;
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
			Effects->Shots(E.Gun, At, World(E.To.X, E.To.Y, E.To.Z), Name, E.Rounds, static_cast<float>(E.PerSecond), Ground);
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
		}
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
		if (Effects && !Fx.IsEmpty())
		{
			if (Fx.Contains(TEXT("brew")) || Fx.Contains(TEXT("cook")) || Fx.Contains(TEXT("magazine")) || Fx.Contains(TEXT("flame_jet")))
			{
				// (a brew-up: the ammunition goes, flame roaring from the hatches; the turret thrown is the model's)
				Effects->CookOff(At, Up, 1.0f, 8, 5.0f, 4.0f, -1.0f, E.Key);
			}
			else if (Fx.Contains(TEXT("knocked")) || Fx.Contains(TEXT("knock_out")) || Fx.Contains(TEXT("hatch_blown")))
			{
				Effects->CookOff(At, Up, 0.8f, 3, 2.5f, 0.0f, -1.0f, E.Key);
			}
			else if (Fx.Contains(TEXT("fire")) || Fx.Contains(TEXT("burn")))
			{
				const float Seconds = static_cast<float>(FMath::Max(5.0, (E.bLoop ? E.Until : S.Rec.End + 8.0) - E.T));
				Effects->Burn(At, 2.4f, Seconds, E.Key);
			}
			else if (Fx.Contains(TEXT("smoke_column")))
			{
				Effects->Burn(At, 3.2f, static_cast<float>(FMath::Max(10.0, S.Rec.End + 8.0 - E.T)), E.Key + 200000);
			}
			else if (Fx.Contains(TEXT("smoke")))
			{
				// (a burst of smoke as she is struck; a wreck's smoke goes on until its loop ends)
				const float Seconds = E.bLoop ? static_cast<float>(FMath::Max(5.0, E.Until - E.T)) : 4.0f;
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
				StartTake(*Cue, At, true, TEXT("engines"), LandParts::Gain(BusDb(TEXT("loops"))), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
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
		S.Mix.Hear(N, S.Real, S.Battle, FMath::Max(0.25, S.SpeedNow), Ear, S.Takes);
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
	// Each vehicle's engine: the loop her core says (idling, driving), on the loops' bus, moving with her; gone with her.
	// Louder down among the men (land.closeView), as every other world sound.
	const float Volume = LandParts::Gain(BusDb(TEXT("loops")) + LiftDb);
	const std::vector<LandKit::Vehicle>& All = S.Armour->Vehicles();
	for (int32 K = 0; K < static_cast<int32>(All.size()); ++K)
	{
		const LandKit::Vehicle& V = All[static_cast<size_t>(K)];
		// (a wreck burning crackles where her engine ran)
		const bool bFire = V.bDead && V.bBurning && V.Fade >= 0.5;
		const FName Cue = bFire ? FName(TEXT("fire_loop")) : V.bDead || V.Fade < 0.5 || V.EngineCue.empty() ? NAME_None : FName(LandParts::Wide(V.EngineCue));
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
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, Key + TEXT("-") + Shot.Name + TEXT(".png")), true, false);
			++ShotsTaken;
			UE_LOG(LogTheater, Display, TEXT("Land battle: picture %s at %.1f s, %s shot on %d"), *Shot.Name, S.Battle, ANSI_TO_TCHAR(LandEye::NameOf(S.Pose.Shot)), S.Pose.Subject);
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
