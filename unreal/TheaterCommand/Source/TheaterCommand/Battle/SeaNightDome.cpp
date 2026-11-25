#include "Battle/SeaNightDome.h"

#include "Campaign/TheaterStillMesh.h"
#include "Components/SceneComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "TheaterCommand.h"
#include "UnrealClient.h"
#include <cmath>

namespace SeaNightDomeParts
{
	/** The moon's dark side is left out under this share of her full face's light while the sky is still light. */
	constexpr double DarkCut = 0.08;
	/** The stars' light is stored over this much (the core's brightest, 1.5), so 1 is the top of a vertex's colour. */
	constexpr double StarTop = 1.5;
}

bool FSeaNightDome::Open(const FRoom& InRoom, const SeaNightSky::When& InWhen, double InStarStrength)
{
	Close();
	Room = InRoom;
	When = InWhen;
	See = SeaNightSky::Seeing();
	StarStrength = FMath::Clamp(InStarStrength, 0.0, 8.0);
	if (!Room.Owner || !Room.Root || StarStrength <= 0.0 || When.SunUp > OpensUnder)
	{
		return false;
	}
	UMaterialInterface* NightPaint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_NightSky.M_NightSky"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!NightPaint)
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: M_NightSky is missing (run Tools/build_content.sh): no stars or moon in the night sky."));
		return false;
	}
	// (one still mesh: drawn as a stored model is, its orders kept, only carried after the eye; no shadow, nothing to
	// collide with; in the world's frame, so the eye's place is its own)
	UTheaterStillMesh* Made = UTheaterStillMesh::Make(Room.Owner, TEXT("SeaNightDome"));
	Made->bUseAsyncCooking = false;
	Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Made->SetCastShadow(false);
	Made->SetupAttachment(Room.Root);
	Made->SetUsingAbsoluteLocation(true);
	Made->SetUsingAbsoluteRotation(true);
	Made->SetUsingAbsoluteScale(true);
	Made->RegisterComponent();
	Made->SetWorldTransform(FTransform::Identity);
	UMaterialInstanceDynamic* Stars = UMaterialInstanceDynamic::Create(NightPaint, Made);
	UMaterialInstanceDynamic* Moon = UMaterialInstanceDynamic::Create(NightPaint, Made);
	Made->SetMaterial(0, Stars);
	Made->SetMaterial(1, Moon);
	Dome = Made;
	StarPaint = Stars;
	MoonPaint = Moon;
	bOn = true;
	bSaid = false;
	Paint();
	return true;
}

void FSeaNightDome::SetSeeing(double Cover, double StarShare)
{
	See.Cover = FMath::Clamp(Cover, 0.0, 1.0);
	See.StarShare = StarShare;
}

void FSeaNightDome::SetExposure(double InBiasStops)
{
	BiasStops = InBiasStops;
	Paint();
}

void FSeaNightDome::Paint()
{
	// (a star or the moon as bright on the screen whatever the exposure: the bias lifts the scene, so it is taken back off
	// here; the stars' colours stored over StarTop, the moon's over 1)
	const double Back = FMath::Pow(2.0, -BiasStops);
	if (UMaterialInstanceDynamic* Stars = StarPaint.Get())
	{
		Stars->SetScalarParameterValue(TEXT("Strength"), static_cast<float>(SeaNightDomeParts::StarTop * StarScreen * StarStrength * Back));
	}
	if (UMaterialInstanceDynamic* Moon = MoonPaint.Get())
	{
		Moon->SetScalarParameterValue(TEXT("Strength"), static_cast<float>(MoonScreen * Back));
	}
}

double FSeaNightDome::InternalWidth(UWorld* World)
{
	double Wide = 1600.0;
	if (World)
	{
		if (UGameViewportClient* Client = World->GetGameViewport())
		{
			if (Client->Viewport && Client->Viewport->GetSizeXY().X > 0)
			{
				Wide = Client->Viewport->GetSizeXY().X;
			}
		}
	}
	double Percent = 100.0;
	if (IConsoleVariable* Screen = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		const double Asked = Screen->GetFloat();
		Percent = Asked > 0.0 ? Asked : 100.0;
	}
	return FMath::Max(64.0, Wide * Percent / 100.0);
}

void FSeaNightDome::Lay(double FovDeg, const FVector& Eye)
{
	UProceduralMeshComponent* Mesh = Dome.Get();
	if (!bOn || !Mesh)
	{
		return;
	}
	const double Start = FPlatformTime::Seconds();
	SeaNightSky::Lens L;
	L.FovDeg = FovDeg;
	L.ViewPx = InternalWidth(Room.Owner ? Room.Owner->GetWorld() : nullptr);
	SeaNightSky::Seeing Seen = See;
	Seen.Strength = 1.0; // (the switch scales the stars' light through the paint, so a vertex's colour keeps its range)
	Sky = SeaNightSky::Lay(When, L, Seen);
	const double Radius = DomeMetres * Room.UnitsPerMetre;
	const SeaNightSky::Mesh M = SeaNightSky::BuildMesh(Sky, Radius, 64, 8, When.SunUp < DarkUnder ? 0.0 : SeaNightDomeParts::DarkCut);

	// The board's frame (east, north, up) to the world's (north, east, up), as ASeaBattle::World turns it; each face
	// wound toward the eye by the kit's rule (the paint is two-sided, so this only keeps the rule).
	const int32 Points = static_cast<int32>(M.V.size() / 3);
	// (the stars and the Milky Way's specks share the first section: the same paint and the same strength)
	const int32 StarPoints = (M.StarQuads + M.GlowQuads) * 4, StarIndices = (M.StarQuads + M.GlowQuads) * 6;
	TArray<FVector> Places[2], Normals[2];
	TArray<FColor> Colours[2];
	TArray<int32> Triangles[2];
	for (int32 P = 0; P < Points; ++P)
	{
		const int32 Part = P < StarPoints ? 0 : 1;
		const FVector At(M.V[P * 3 + 1], M.V[P * 3], M.V[P * 3 + 2]);
		const double Top = Part == 0 ? SeaNightDomeParts::StarTop : 1.0;
		const FLinearColor Light(static_cast<float>(M.C[P * 4] / Top), static_cast<float>(M.C[P * 4 + 1] / Top), static_cast<float>(M.C[P * 4 + 2] / Top), 1.0f);
		// (sRGB in the vertex, for the faint stars' sake: M_NightSky raises it to 2.2 again, as M_UnderSea does)
		FColor Stored = Light.ToFColor(true);
		Stored.A = M.C[P * 4 + 3] > 0.5f ? 255 : 0;
		Places[Part].Add(At);
		Normals[Part].Add(-At.GetSafeNormal());
		Colours[Part].Add(Stored);
	}
	for (int32 K = 0; K + 2 < static_cast<int32>(M.Triangles.size()); K += 3)
	{
		const int32 Part = K < StarIndices ? 0 : 1;
		const int32 Less = Part == 0 ? 0 : StarPoints;
		const int32 A = M.Triangles[K] - Less, B = M.Triangles[K + 1] - Less, C = M.Triangles[K + 2] - Less;
		const TArray<FVector>& Pl = Places[Part];
		const FVector Facing = (Pl[B] - Pl[C]) ^ (Pl[A] - Pl[C]);
		const FVector Mid = (Pl[A] + Pl[B] + Pl[C]) / 3.0;
		if ((Facing | -Mid) >= 0.0)
		{
			Triangles[Part].Append({ A, B, C });
		}
		else
		{
			Triangles[Part].Append({ A, C, B });
		}
	}
	for (int32 Part = 0; Part < 2; ++Part)
	{
		if (Triangles[Part].Num() > 0)
		{
			Mesh->CreateMeshSection(Part, Places[Part], Triangles[Part], Normals[Part], TArray<FVector2D>(), Colours[Part], TArray<FProcMeshTangent>(), false);
		}
		else
		{
			Mesh->ClearMeshSection(Part);
		}
	}
	Centre = Eye;
	Mesh->SetWorldLocation(Eye);
	LayMs = (FPlatformTime::Seconds() - Start) * 1000.0;
	if (!bSaid)
	{
		bSaid = true;
		UE_LOG(LogTheater, Display,
			TEXT("Sea battle: the night sky over %.1f%s %.1f%s, %d-%02d-%02d at %.1f h local (the sun %.0f deg down): %d stars to magnitude %.1f (%d under the haze, %d under cloud, %d behind the moon), the Milky Way at %.0f%% in %d specks; the moon %s; laid in %.2f ms for a lens %.0f deg across %.0f pixels."),
			FMath::Abs(When.Lat), When.Lat >= 0.0 ? TEXT("N") : TEXT("S"), FMath::Abs(When.Lon), When.Lon >= 0.0 ? TEXT("E") : TEXT("W"),
			When.Year, When.Month, When.Day, Sky.SolarHour, -When.SunUp, StarsLaid(), Sky.LimitMag, Sky.BelowHaze, Sky.HiddenByCloud, Sky.HiddenByMoon, 100.0 * Sky.MilkyShare, GlowLaid(),
			Sky.bMoonUp ? *FString::Printf(TEXT("%.0f deg up from %.0f, %.0f%% lit"), When.MoonUp, When.MoonFrom, 100.0 * When.MoonLit) : TEXT("down"),
			LayMs, FovDeg, L.ViewPx);
	}
}

void FSeaNightDome::Follow(const FVector& Eye)
{
	UProceduralMeshComponent* Mesh = Dome.Get();
	if (!bOn || !Mesh)
	{
		return;
	}
	// (at infinity: carried with the eye, but only once it has moved far enough to matter, 25 m against 400 km being a
	// two-hundredth of a degree)
	if (FVector::DistSquared(Eye, Centre) > FMath::Square(FollowMetres * Room.UnitsPerMetre))
	{
		Centre = Eye;
		Mesh->SetWorldLocation(Eye);
	}
}

void FSeaNightDome::Close()
{
	if (UProceduralMeshComponent* Mesh = Dome.Get())
	{
		Mesh->DestroyComponent();
	}
	Dome.Reset();
	StarPaint.Reset();
	MoonPaint.Reset();
	Sky = SeaNightSky::Sky();
	bOn = false;
}
