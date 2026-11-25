#include "Effects/WeatherFall.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "TheaterCommand.h"

#include <string>

namespace
{
	const TCHAR* const FallMaterialPath = TEXT("/Game/Campaign/Materials/M_LandFall.M_LandFall");

	FLinearColor Four(const float (&V)[4])
	{
		return FLinearColor(V[0], V[1], V[2], V[3]);
	}
}

FWeatherFall::FWeatherFall()
	: Recipe(WeatherFall::Recipe::Standing())
{
}

FWeatherFall::~FWeatherFall()
{
	// (the meshes go with their owner: none is destroyed here, where the owner may be going itself)
	Layers.Reset();
}

bool FWeatherFall::LoadRecipe(const FString& FileName)
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("Effects") / FileName;
	Recipe = WeatherFall::Recipe::Standing();
	bBook = Book.Load(Path);
	if (!bBook)
	{
		UE_LOG(LogTheater, Warning, TEXT("Rain and snow: the recipe is missing or unreadable (%s); the standing numbers"), *Path);
		return false;
	}
	// (the layers are the recipe's "fall": "fall.rain.near.density", "fall.least_px")
	Recipe.ReadOver([this](const char* Key, double& Out)
	{
		const std::string Full = std::string("fall.") + Key;
		return Book.Find(Full.c_str(), Out);
	});
	return true;
}

bool FWeatherFall::Number(const char* Path, double& Out) const
{
	return bBook && Path && Book.Find(Path, Out);
}

bool FWeatherFall::Setup(USceneComponent* Parent, double InUnitsPerMetre, uint64 InSeed)
{
	if (!bBook)
	{
		LoadRecipe();
	}
	if (!Parent || !Parent->GetOwner() || InUnitsPerMetre <= 0.0)
	{
		UE_LOG(LogTheater, Warning, TEXT("Rain and snow: no parent to hang on, or no scale; nothing will fall"));
		return false;
	}
	UMaterialInterface* Found = LoadObject<UMaterialInterface>(nullptr, FallMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Found)
	{
		UE_LOG(LogTheater, Warning, TEXT("Rain and snow: M_LandFall is missing (run Tools/build_content.sh); nothing will fall (the haze, the light and the wet ground are laid as asked)"));
		return false;
	}
	Material = Found;
	Holder = Parent;
	UnitsPerMetre = InUnitsPerMetre;
	Seed = InSeed;
	bReady = true;
	if (Kind != WeatherFall::Kind::None && Made != Kind)
	{
		Make();
	}
	return true;
}

void FWeatherFall::SetFall(WeatherFall::Kind InKind, double InStrength)
{
	Kind = InKind;
	Strength = FMath::Clamp(InStrength, 0.0, 1.0);
	if (bReady && Kind != WeatherFall::Kind::None && Made != Kind)
	{
		Make();
	}
}

void FWeatherFall::SetWind(double WorldX, double WorldY)
{
	WindX = FMath::IsFinite(WorldX) ? WorldX : 0.0;
	WindY = FMath::IsFinite(WorldY) ? WorldY : 0.0;
}

void FWeatherFall::SetInk(const FLinearColor& Linear)
{
	InkNow = Linear;
	for (FShownLayer& Each : Layers)
	{
		if (UMaterialInstanceDynamic* Ink = Each.Ink.Get())
		{
			Ink->SetVectorParameterValue(TEXT("Ink"), InkNow);
		}
	}
}

void FWeatherFall::SetLight(double Share)
{
	Light = FMath::Clamp(FMath::IsFinite(Share) ? Share : 1.0, 0.0, 4.0);
}

void FWeatherFall::Make()
{
	// (a kind's meshes, made once: the other kind's taken away first)
	for (FShownLayer& Each : Layers)
	{
		if (UProceduralMeshComponent* Mesh = Each.Mesh.Get())
		{
			Mesh->DestroyComponent();
		}
	}
	Layers.Reset();
	Made = WeatherFall::Kind::None;
	USceneComponent* Parent = Holder.Get();
	if (!Parent || !Parent->GetOwner())
	{
		return;
	}
	UMaterialInterface* Parented = Material.Get();
	if (!Parented)
	{
		Parented = LoadObject<UMaterialInterface>(nullptr, FallMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		Material = Parented;
	}
	if (!Parented)
	{
		UE_LOG(LogTheater, Warning, TEXT("Rain and snow: M_LandFall has gone; nothing will fall"));
		bReady = false;
		return;
	}
	AActor* Owner = Parent->GetOwner();
	const double U = UnitsPerMetre;
	const double Step = Recipe.StepOf(Kind);
	int32 LayerIndex = 0;
	for (const WeatherFall::Layer& Layer : Recipe.Of(Kind))
	{
		const WeatherFall::Mesh Drops = WeatherFall::Make(Layer, Step, Recipe.MostDrops, Seed * 0x9E3779B97F4A7C15ull + static_cast<uint64>(LayerIndex) * 0x632BE59BD9B4E019ull + static_cast<uint64>(Kind));
		++LayerIndex;
		const int32 Count = static_cast<int32>(Drops.Points.size() / 3);
		if (Drops.DropCount <= 0 || Count <= 0)
		{
			continue;
		}
		// (each point at its drop's home, in world units, in the mesh's own frame, which is the world's turned not at all)
		TArray<FVector> Points;
		TArray<FVector2D> Corners, Falls, Sizes;
		Points.SetNumUninitialized(Count);
		Corners.SetNumUninitialized(Count);
		Falls.SetNumUninitialized(Count);
		Sizes.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const size_t P = static_cast<size_t>(Index);
			Points[Index] = FVector(Drops.Points[P * 3], Drops.Points[P * 3 + 1], Drops.Points[P * 3 + 2]) * U;
			Corners[Index] = FVector2D(Drops.Corners[P * 2], Drops.Corners[P * 2 + 1]);
			Falls[Index] = FVector2D(Drops.Drops[P * 4], Drops.Drops[P * 4 + 1]);
			Sizes[Index] = FVector2D(Drops.Drops[P * 4 + 2], Drops.Drops[P * 4 + 3]);
		}
		TArray<int32> Triangles;
		Triangles.Reserve(static_cast<int32>(Drops.Triangles.size()));
		for (const int Index : Drops.Triangles)
		{
			Triangles.Add(Index);
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Count);
		const FString Name = FString::Printf(TEXT("WeatherFall_%s_%s"), ANSI_TO_TCHAR(WeatherFall::NameOf(Kind)), ANSI_TO_TCHAR(Layer.Name.c_str()));
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Owner, MakeUniqueObjectName(Owner, UProceduralMeshComponent::StaticClass(), FName(*Name)));
		Mesh->bUseAsyncCooking = false;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetupAttachment(Parent);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		// (near layers over the smoke and the dust, far ones under them)
		Mesh->SetTranslucentSortPriority(Layer.Order);
		Mesh->RegisterComponent();
		Mesh->SetWorldTransform(FTransform::Identity);
		UMaterialInstanceDynamic* Ink = UMaterialInstanceDynamic::Create(Parented, Mesh);
		Ink->SetScalarParameterValue(TEXT("UnitsPerMetre"), static_cast<float>(U));
		Ink->SetVectorParameterValue(TEXT("Ink"), InkNow);
		Mesh->SetMaterial(0, Ink);
		Mesh->CreateMeshSection(0, Points, Triangles, Normals, Corners, Falls, Sizes, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		Mesh->SetVisibility(false);
		FShownLayer& Shown = Layers.AddDefaulted_GetRef();
		Shown.Layer = Layer;
		Shown.Mesh = Mesh;
		Shown.Ink = Ink;
		Shown.Drops = Drops.DropCount;
	}
	Made = Layers.Num() > 0 ? Kind : WeatherFall::Kind::None;
	UE_LOG(LogTheater, Display, TEXT("Rain and snow: %s"), *Said());
}

void FWeatherFall::Tick(const FVector& EyeWorld, double FovDegrees, double ViewWidthPx, double Seconds, bool bRunning)
{
	if (Layers.Num() == 0)
	{
		return;
	}
	const bool bShow = IsFalling() && Made == Kind;
	if (bRunning && FMath::IsFinite(Seconds))
	{
		Clock += FMath::Clamp(Seconds, 0.0, 0.25);
	}
	const double Lens = FMath::DegreesToRadians(FMath::Clamp(FMath::IsFinite(FovDegrees) ? FovDegrees : 90.0, 1.0, 170.0));
	const double PixelAngle = Lens / FMath::Max(ViewWidthPx, 64.0);
	const double U = UnitsPerMetre;
	for (FShownLayer& Each : Layers)
	{
		UProceduralMeshComponent* Mesh = Each.Mesh.Get();
		UMaterialInstanceDynamic* Ink = Each.Ink.Get();
		if (!Mesh || !Ink)
		{
			continue;
		}
		if (!bShow)
		{
			Mesh->SetVisibility(false);
			continue;
		}
		// The mesh set down on its own grid of whole boxes nearest the eye: the chip wraps each drop about the eye by
		// whole boxes, so where the mesh is laid never moves a drop in the world.
		const double Side = Each.Layer.Box * U;
		const FVector Grid(FMath::RoundToDouble(EyeWorld.X / Side) * Side, FMath::RoundToDouble(EyeWorld.Y / Side) * Side, FMath::RoundToDouble(EyeWorld.Z / Side) * Side);
		if (!Grid.Equals(Each.Placed, 0.5))
		{
			Mesh->SetWorldLocation(Grid);
			Each.Placed = Grid;
		}
		const WeatherFall::Params P = WeatherFall::Now(Recipe, Each.Layer, Kind, Strength, WindX, WindY, Clock, PixelAngle);
		Ink->SetVectorParameterValue(TEXT("Box"), Four(P.Box));
		Ink->SetVectorParameterValue(TEXT("Move"), Four(P.Move));
		Ink->SetVectorParameterValue(TEXT("Look"), Four(P.Look));
		Ink->SetVectorParameterValue(TEXT("Lens"), Four(P.Lens));
		Ink->SetScalarParameterValue(TEXT("Opacity"), P.Opacity);
		Ink->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(P.Tone * Light));
		Mesh->SetVisibility(true);
	}
}

void FWeatherFall::Clear()
{
	for (FShownLayer& Each : Layers)
	{
		if (UProceduralMeshComponent* Mesh = Each.Mesh.Get())
		{
			Mesh->DestroyComponent();
		}
	}
	Layers.Reset();
	Made = WeatherFall::Kind::None;
}

FString FWeatherFall::Said() const
{
	if (Layers.Num() == 0)
	{
		return FString::Printf(TEXT("%s at %.2f, no layers made%s"), ANSI_TO_TCHAR(WeatherFall::NameOf(Kind)), Strength, bReady ? TEXT("") : TEXT(" (M_LandFall missing)"));
	}
	FString Out = FString::Printf(TEXT("%s at %.2f, wind %.1f, %.1f m/s, the drops wrapped round the eye on the graphics chip (M_LandFall):"),
		ANSI_TO_TCHAR(WeatherFall::NameOf(Made)), Strength, WindX, WindY);
	int32 All = 0;
	for (const FShownLayer& Each : Layers)
	{
		const int32 Showing = FMath::RoundToInt(Each.Drops * Strength);
		All += Showing;
		Out += FString::Printf(TEXT(" %s %.0f m box, %d of %d drops;"), ANSI_TO_TCHAR(Each.Layer.Name.c_str()), Each.Layer.Box, Showing, Each.Drops);
	}
	Out += FString::Printf(TEXT(" %d showing in all"), All);
	return Out;
}
