#include "Look/WholeLook.h"

#include "TheaterCommand.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	// ---- The switches (WholeLook.h) ----------------------------------------------------------------------------------

	TAutoConsoleVariable<int32> CVarCrisp(TEXT("tc.Look.Crisp"), 0,
		TEXT("Release 2, the whole picture: 1 sharpens the picture after the upscaler and reads the ground's pictures with sixteen-fold filtering at a slant (Look/WholeLook.h)."));
	TAutoConsoleVariable<float> CVarCrispSharpen(TEXT("tc.Look.CrispSharpen"), 0.6f,
		TEXT("How hard tc.Look.Crisp sharpens (r.Tonemapper.Sharpen while it is on; 0 to 2, 0.6 to start)."));
	TAutoConsoleVariable<int32> CVarCrispFiltering(TEXT("tc.Look.CrispFiltering"), 16,
		TEXT("The slanted filtering tc.Look.Crisp asks for (r.MaxAnisotropy while it is on; 1 to 16)."));
	TAutoConsoleVariable<int32> CVarDepth(TEXT("tc.Look.Depth"), 0,
		TEXT("Release 2, the whole picture: 1 lets the soft shade where things meet (ambient occlusion) reach as far as a battle is seen, not the engine's 80 m (Look/WholeLook.h)."));
	TAutoConsoleVariable<float> CVarDepthStrength(TEXT("tc.Look.DepthStrength"), 0.65f,
		TEXT("tc.Look.Depth: how dark the shade is (the engine's own is 0.5)."));
	TAutoConsoleVariable<float> CVarDepthReach(TEXT("tc.Look.DepthReach"), 1500.0f,
		TEXT("tc.Look.Depth: metres from the eye where the shade has wholly faded (it is whole to half of it; the engine's own is 80)."));
	TAutoConsoleVariable<float> CVarDepthQuality(TEXT("tc.Look.DepthQuality"), 70.0f,
		TEXT("tc.Look.Depth: the shade's quality, 0 to 100 (the engine's own is 50; above about 75 it reads a third, wider ring)."));
	TAutoConsoleVariable<int32> CVarBounce(TEXT("tc.Look.Bounce"), 0,
		TEXT("Release 2, the whole picture: 1 lays every ground's own colour under the ground battle's sky light, as snow's is, for the light it gives back to what faces it (Look/WholeLook.h). Read as a battle opens."));
	TAutoConsoleVariable<float> CVarBounceStrength(TEXT("tc.Look.BounceStrength"), 1.0f,
		TEXT("tc.Look.Bounce: the field's mean paint, as light, times this (0 to 4; snow's own is 2)."));

	/** The engine's world units in a metre (the battles are laid at 100, as the engine's own measure is). */
	constexpr float UnitsPerMetre = 100.0f;

	// ---- What was there before -----------------------------------------------------------------------------------------

	/** An engine setting a look changes, and what it was before the first change, to be put back when the look goes off. */
	struct FKept
	{
		const TCHAR* Name;
		FString Was;
		bool bKept = false;
	};

	FKept KeptSharpen{ TEXT("r.Tonemapper.Sharpen") };
	FKept KeptFiltering{ TEXT("r.MaxAnisotropy") };

	/** The setting given this value, what it was kept first. False if the engine has no such setting (yet). */
	bool Lay(FKept& Kept, const FString& Value)
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Kept.Name);
		if (!Variable)
		{
			return false;
		}
		if (!Kept.bKept)
		{
			Kept.Was = Variable->GetString();
			Kept.bKept = true;
		}
		Variable->Set(*Value, ECVF_SetByCode);
		return true;
	}

	void PutBack(FKept& Kept)
	{
		if (!Kept.bKept)
		{
			return;
		}
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Kept.Name))
		{
			Variable->Set(*Kept.Was, ECVF_SetByCode);
		}
		Kept.bKept = false;
	}

	// ---- Crisp ---------------------------------------------------------------------------------------------------------

	bool bCrispLaid = false;
	float SharpenLaid = -1.0f;
	int32 FilteringLaid = -1;

	void KeepCrisp()
	{
		const bool bOn = CVarCrisp.GetValueOnGameThread() != 0;
		const float Sharpen = FMath::Clamp(CVarCrispSharpen.GetValueOnGameThread(), 0.0f, 2.0f);
		const int32 Filtering = FMath::Clamp(CVarCrispFiltering.GetValueOnGameThread(), 1, 16);
		if (bOn == bCrispLaid && (!bOn || (FMath::IsNearlyEqual(Sharpen, SharpenLaid) && Filtering == FilteringLaid)))
		{
			return;
		}
		if (bOn)
		{
			const bool bSharpened = Lay(KeptSharpen, FString::SanitizeFloat(Sharpen));
			const bool bFiltered = Lay(KeptFiltering, FString::FromInt(Filtering));
			if (!bSharpened || !bFiltered)
			{
				return; // (the renderer's settings are not there yet: tried again on the next turn of the clock)
			}
			UE_LOG(LogTheater, Display, TEXT("Whole look: the crisp picture ON: sharpened %.2f after the upscaler (it was %s), the ground's pictures filtered %d-fold at a slant (it was %s); tc.Look.Crisp 0 puts both back"),
				Sharpen, *KeptSharpen.Was, Filtering, *KeptFiltering.Was);
		}
		else
		{
			PutBack(KeptSharpen);
			PutBack(KeptFiltering);
			UE_LOG(LogTheater, Display, TEXT("Whole look: the crisp picture off: the sharpening and the filtering as they were"));
		}
		bCrispLaid = bOn;
		SharpenLaid = Sharpen;
		FilteringLaid = Filtering;
	}

	// ---- Depth ---------------------------------------------------------------------------------------------------------

	TWeakObjectPtr<APostProcessVolume> DepthVolume;
	float StrengthSaid = -1.0f, ReachSaid = -1.0f, QualitySaid = -1.0f;
	bool bDepthSaid = false;

	/** The world the game is being played in (none while it loads). */
	UWorld* PlayWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	void KeepDepth()
	{
		const bool bOn = CVarDepth.GetValueOnGameThread() != 0;
		UWorld* World = PlayWorld();
		APostProcessVolume* Volume = DepthVolume.Get();
		if (Volume && Volume->GetWorld() != World)
		{
			Volume = nullptr; // (an old world's: it goes with that world)
		}
		if (!bOn)
		{
			if (Volume && Volume->bEnabled)
			{
				Volume->bEnabled = false;
				UE_LOG(LogTheater, Display, TEXT("Whole look: the far shade off: ambient occlusion as the engine has it (it fades out by 80 m)"));
			}
			bDepthSaid = false;
			return;
		}
		if (!World || World->bIsTearingDown || !World->HasBegunPlay())
		{
			return;
		}
		if (!Volume)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transient;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Volume = World->SpawnActor<APostProcessVolume>(Params);
			if (!Volume)
			{
				return;
			}
			// (over the whole world, above the ground battle's grade volume, which is at 0; it sets nothing that one or a
			// camera's lens sets, so the two never meet)
			Volume->bUnbound = true;
			Volume->Priority = 1.0f;
			Volume->BlendWeight = 1.0f;
			DepthVolume = Volume;
			bDepthSaid = false;
		}
		Volume->bEnabled = true;
		const float Strength = FMath::Clamp(CVarDepthStrength.GetValueOnGameThread(), 0.0f, 1.0f);
		const float Reach = FMath::Clamp(CVarDepthReach.GetValueOnGameThread(), 80.0f, 5000.0f);
		const float Quality = FMath::Clamp(CVarDepthQuality.GetValueOnGameThread(), 0.0f, 100.0f);
		FPostProcessSettings& Shade = Volume->Settings;
		Shade.bOverride_AmbientOcclusionIntensity = true;
		Shade.AmbientOcclusionIntensity = Strength;
		// (whole out to half the reach, then fading to nothing at the reach)
		Shade.bOverride_AmbientOcclusionFadeDistance = true;
		Shade.AmbientOcclusionFadeDistance = Reach * UnitsPerMetre;
		Shade.bOverride_AmbientOcclusionFadeRadius = true;
		Shade.AmbientOcclusionFadeRadius = 0.5f * Reach * UnitsPerMetre;
		Shade.bOverride_AmbientOcclusionQuality = true;
		Shade.AmbientOcclusionQuality = Quality;
		if (!bDepthSaid || !FMath::IsNearlyEqual(Strength, StrengthSaid) || !FMath::IsNearlyEqual(Reach, ReachSaid) || !FMath::IsNearlyEqual(Quality, QualitySaid))
		{
			bDepthSaid = true;
			StrengthSaid = Strength;
			ReachSaid = Reach;
			QualitySaid = Quality;
			UE_LOG(LogTheater, Display, TEXT("Whole look: the far shade ON: ambient occlusion %.2f strong, whole to %.0f m and gone at %.0f m, quality %.0f (the engine's own: 0.50, 30 and 80 m, 50); it shows where a scene draws it (the ground battle)"),
				Strength, 0.5f * Reach, Reach, Quality);
		}
	}

	// ---- The clock -----------------------------------------------------------------------------------------------------

	FTSTicker::FDelegateHandle Clock;

	bool Turn(float DeltaSeconds)
	{
		KeepCrisp();
		KeepDepth();
		return true;
	}

	/** "-TheaterLookCrisp" (or "=0", "=1") sets its variable, as the ship list's row would. */
	void Asked(const TCHAR* Word, TAutoConsoleVariable<int32>& Variable)
	{
		int32 Value = 0;
		if (FParse::Value(FCommandLine::Get(), *(FString(Word) + TEXT("=")), Value))
		{
			Variable->Set(Value != 0 ? 1 : 0, ECVF_SetByCode);
		}
		else if (FParse::Param(FCommandLine::Get(), Word))
		{
			Variable->Set(1, ECVF_SetByCode);
		}
	}
}

void WholeLook::Start()
{
	Asked(TEXT("TheaterLookCrisp"), CVarCrisp);
	Asked(TEXT("TheaterLookDepth"), CVarDepth);
	Asked(TEXT("TheaterLookBounce"), CVarBounce);
	// (the sharpening and the filtering at once, before the game's own pictures are made, so that every one of them is read
	// with the filtering asked; the far shade waits for a world to lay its volume in)
	KeepCrisp();
	if (!Clock.IsValid())
	{
		Clock = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Turn), 0.25f);
	}
}

void WholeLook::Stop()
{
	if (Clock.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Clock);
		Clock.Reset();
	}
}

bool WholeLook::BounceOn()
{
	return CVarBounce.GetValueOnGameThread() != 0;
}

float WholeLook::BounceStrength()
{
	return FMath::Clamp(CVarBounceStrength.GetValueOnGameThread(), 0.0f, 4.0f);
}

FLinearColor WholeLook::GroundMean(TFunctionRef<FColor(double X, double Y)> ColourAt, double Wide, double Deep)
{
	// (the paint is sRGB, as a picture keeps it; light adds in linear, so each is made linear before the mean)
	constexpr int32 Across = 24;
	FLinearColor Sum(0.0f, 0.0f, 0.0f, 0.0f);
	int32 Count = 0;
	for (int32 Row = 0; Row < Across; ++Row)
	{
		for (int32 Col = 0; Col < Across; ++Col)
		{
			const double X = (Col + 0.5) / Across * Wide, Y = (Row + 0.5) / Across * Deep;
			Sum += FLinearColor::FromSRGBColor(ColourAt(X, Y));
			++Count;
		}
	}
	return Count > 0 ? FLinearColor(Sum.R / Count, Sum.G / Count, Sum.B / Count, 1.0f) : FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
}
