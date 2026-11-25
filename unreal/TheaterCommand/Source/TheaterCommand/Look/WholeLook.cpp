#include "Look/WholeLook.h"

#include "TheaterCommand.h"
#include "CoreGlobals.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkyLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RenderTimer.h" // (GRenderThreadTime, as TheaterSpeed reads it: the in-battle timing)
#include "RHI.h" // (RHIGetGPUFrameCycles: the in-battle timing)
#include "UObject/UObjectIterator.h"

namespace
{
	// ---- The switches (WholeLook.h) ----------------------------------------------------------------------------------

	TAutoConsoleVariable<int32> CVarCrisp(TEXT("tc.Look.Crisp"), 0,
		TEXT("Release 2, the whole picture: 1 sharpens the picture after the upscaler and reads the ground's pictures with sixteen-fold filtering at a slant (Look/WholeLook.h)."));
	TAutoConsoleVariable<float> CVarCrispSharpen(TEXT("tc.Look.CrispSharpen"), 0.4f,
		TEXT("How hard tc.Look.Crisp sharpens (the tonemapper's sharpening, 0 to 1; the art director: start 0.4, sweep 0.25 / 0.4 / 0.6, 0.6 the ceiling until a pan clip shows no rim)."));
	TAutoConsoleVariable<int32> CVarCrispFiltering(TEXT("tc.Look.CrispFiltering"), 16,
		TEXT("The slanted filtering tc.Look.Crisp asks for (r.MaxAnisotropy while it is on; 1 to 16)."));
	TAutoConsoleVariable<int32> CVarDepth(TEXT("tc.Look.Depth"), 0,
		TEXT("Release 2, the whole picture: 1 lets the soft shade where things meet (ambient occlusion) reach as far as a battle is seen, not the engine's 80 m (Look/WholeLook.h)."));
	TAutoConsoleVariable<float> CVarDepthStrength(TEXT("tc.Look.DepthStrength"), 0.65f,
		TEXT("tc.Look.Depth: how dark the shade is (the engine's own is 0.5; the art director: start 0.65, sweep 0.45 / 0.85)."));
	TAutoConsoleVariable<float> CVarDepthReach(TEXT("tc.Look.DepthReach"), 1500.0f,
		TEXT("tc.Look.Depth: metres from the eye where the shade has wholly faded (it is whole to half of it; the engine's own is 80)."));
	TAutoConsoleVariable<float> CVarDepthRadius(TEXT("tc.Look.DepthRadius"), 2.0f,
		TEXT("tc.Look.Depth: how far round a thing its shade reaches, in metres in the world (the art director: contact scale, about 2 m; the engine's own grows with the distance)."));
	TAutoConsoleVariable<float> CVarDepthQuality(TEXT("tc.Look.DepthQuality"), 70.0f,
		TEXT("tc.Look.Depth: the shade's quality, 0 to 100 (the engine's own is 50; above about 75 it reads a third, wider ring)."));
	TAutoConsoleVariable<float> CVarDepthClosed(TEXT("tc.Look.DepthClosed"), 0.4f,
		TEXT("tc.Look.Depth: the shade's strength under a closed canopy, the jungle (the art director: 0.4 to start; canopy shade is never black)."));
	TAutoConsoleVariable<int32> CVarBounce(TEXT("tc.Look.Bounce"), 0,
		TEXT("Release 2, the whole picture: 1 lays every ground's own colour under the ground battle's sky light, as snow's is, for the light it gives back to what faces it (Look/WholeLook.h). Read as a battle opens."));
	TAutoConsoleVariable<float> CVarBounceStrength(TEXT("tc.Look.BounceStrength"), 1.0f,
		TEXT("tc.Look.Bounce: the field's mean paint, as light, times this (0 to 4; snow's own is 2; the art director's sweep 0.6 / 1.0 / 1.6)."));
	TAutoConsoleVariable<float> CVarBounceClosed(TEXT("tc.Look.BounceClosed"), 0.5f,
		TEXT("tc.Look.Bounce: its share under a closed canopy, the jungle (the art director: half to start)."));
	TAutoConsoleVariable<int32> CVarBounceHue(TEXT("tc.Look.BounceHue"), 1,
		TEXT("tc.Look.Bounce: 1 pulls the bounce's hue inside the art director's box (red/green 0.90 to 1.15, blue/green 0.80 to 1.05) with its value kept; 0 leaves the field's own hue."));
	TAutoConsoleVariable<int32> CVarWorn(TEXT("tc.Look.Worn"), 0,
		TEXT("Release 2, the whole picture: 1 breaks the ground battle's vehicles' paint in patches and lays it flat (rougher), so a hull is not one even coat (Look/WholeLook.h). Read as a battle opens."));
	TAutoConsoleVariable<float> CVarWornStrength(TEXT("tc.Look.WornStrength"), 1.0f,
		TEXT("tc.Look.Worn: how strongly the paint is broken (the surface's own Wear, 0 to 2; 1 the map's pieces')."));
	TAutoConsoleVariable<float> CVarWornRepeat(TEXT("tc.Look.WornRepeat"), 2.5f,
		TEXT("tc.Look.Worn: metres over which the patches' pattern repeats on a vehicle (its patches some tenths of a metre across)."));
	TAutoConsoleVariable<float> CVarWornRoughness(TEXT("tc.Look.WornRoughness"), 0.85f,
		TEXT("tc.Look.Worn: the vehicles' paint roughness (the battle's own is 0.6; the art director: army paint 0.8 to 0.9)."));
	TAutoConsoleVariable<float> CVarWornRepeat2(TEXT("tc.Look.WornRepeat2"), 6.1f,
		TEXT("tc.Look.Worn: metres over which the second, larger layer of patches repeats (no multiple of tc.Look.WornRepeat, so no patch comes round on one hull; the art director: about 6 m)."));
	TAutoConsoleVariable<float> CVarWornMix(TEXT("tc.Look.WornMix"), 0.5f,
		TEXT("tc.Look.Worn: the second layer's share of the patches (0 the first alone, 1 the second alone; the blend is lifted so it keeps one layer's contrast)."));
	TAutoConsoleVariable<float> CVarWornSheen(TEXT("tc.Look.WornSheen"), 1.3f,
		TEXT("tc.Look.Worn: how far the roughness moves with the patches (the art director: dark, oily patches about 0.7, pale, dusty ones about 0.95; 0 keeps one roughness)."));

	/** The engine's world units in a metre (the battles are laid at 100, as the engine's own measure is). */
	constexpr float UnitsPerMetre = 100.0f;

	/** A ground battle is laid (FieldLaid to FieldGone): the far shade is the ground battle's alone, the only scene that
	 * draws ambient occlusion (the map's trims keep it off; with -TheaterOldLight they do not, and the map, the air and the
	 * sea would take a 1,500 m shade they were never judged with). */
	bool bFieldOn = false;

	/** The ground battle is fought under a closed canopy (FieldLaid). */
	bool bClosedField = false;

	// ---- What was there before -----------------------------------------------------------------------------------------

	/** An engine setting a look changes, and what it was before the first change, to be put back when the look goes off. */
	struct FKept
	{
		const TCHAR* Name;
		FString Was;
		bool bKept = false;
	};

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

	// ---- Crisp: the filtering (the sharpening is on the lens, below) ---------------------------------------------------

	int32 FilteringLaid = -1; // (-1: none laid)

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

	void KeepFiltering()
	{
		const bool bOn = CVarCrisp.GetValueOnGameThread() != 0;
		const int32 Filtering = FMath::Clamp(CVarCrispFiltering.GetValueOnGameThread(), 1, 16);
		if (bOn)
		{
			// (first laid once the game's world is up: by then the engine has laid the player's own quality settings, the
			// filtering among them, so the value kept to put back is the player's and not the engine's bare default. The
			// ground battle's field pictures and the air battle's ground are made later, with each battle)
			if (Filtering == FilteringLaid || (!KeptFiltering.bKept && !PlayWorld()))
			{
				return;
			}
			if (!Lay(KeptFiltering, FString::FromInt(Filtering)))
			{
				return; // (the engine's setting is not there yet: tried again on the next turn of the clock)
			}
			FilteringLaid = Filtering;
			const IConsoleVariable* Laid = IConsoleManager::Get().FindConsoleVariable(KeptFiltering.Name);
			if (Laid && Laid->GetInt() != Filtering)
			{
				// (a setting made at a higher rank, the console's, as the photographer's timing rounds lay theirs, holds)
				UE_LOG(LogTheater, Warning, TEXT("Whole look: the crisp picture's filtering asked %d-fold but %s holds at %d (set at a higher rank, by the console or a timing round)"),
					Filtering, KeptFiltering.Name, Laid->GetInt());
				return;
			}
			// (the engine makes the samplers of the pictures already made again as the setting changes, by its own callback on
			// r.MaxAnisotropy: the ground battle's field pictures and the air battle's ground take it at once)
			UE_LOG(LogTheater, Display, TEXT("Whole look: the crisp picture's filtering ON: the ground's pictures filtered %d-fold at a slant (it was %s); tc.Look.Crisp 0 puts it back"),
				Filtering, *KeptFiltering.Was);
		}
		else if (KeptFiltering.bKept)
		{
			// (put back whatever was laid, however far the laying got)
			PutBack(KeptFiltering);
			FilteringLaid = -1;
			UE_LOG(LogTheater, Display, TEXT("Whole look: the crisp picture's filtering off: as it was"));
		}
	}

	// ---- The whole look's lens: the sharpening and the far shade ------------------------------------------------------

	TWeakObjectPtr<APostProcessVolume> Lens;
	FString LensSaid;

	/** The plates the game paints with its own cameras (the map's portraits and showpiece) are not sharpened: a camera's own
	 * lens lies over the world's volumes, so each scene capture in the world is told no sharpening, the engine's own value.
	 * (The battle's corner film copies the battle eye's lens onto its capture every frame, so it takes the battle's
	 * sharpening, as the battle's own picture; it is held off in release 2.) */
	void KeepPlatesPlain(UWorld* World)
	{
		for (TObjectIterator<USceneCaptureComponent2D> It; It; ++It)
		{
			USceneCaptureComponent2D* Capture = *It;
			if (!Capture || Capture->GetWorld() != World || Capture->PostProcessSettings.bOverride_Sharpen)
			{
				continue;
			}
			Capture->PostProcessSettings.bOverride_Sharpen = true;
			Capture->PostProcessSettings.Sharpen = 0.0f;
		}
	}

	void KeepLens()
	{
		const bool bCrisp = CVarCrisp.GetValueOnGameThread() != 0;
		const bool bDepth = CVarDepth.GetValueOnGameThread() != 0 && bFieldOn;
		UWorld* World = PlayWorld();
		APostProcessVolume* Volume = Lens.Get();
		if (Volume && Volume->GetWorld() != World)
		{
			Volume = nullptr; // (an old world's: it goes with that world)
		}
		if (!bCrisp && !bDepth)
		{
			if (Volume && Volume->bEnabled)
			{
				Volume->bEnabled = false;
				UE_LOG(LogTheater, Display, TEXT("Whole look: the lens off: no sharpening, ambient occlusion as the engine has it (it fades out by 80 m)"));
			}
			LensSaid.Reset();
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
			// camera's lens sets, so they never meet. The world sorts its volumes by their priority as each registers, and
			// this one registered inside the spawn at the engine's 0: registered again at 1, so it lies where it says)
			Volume->bUnbound = true;
			Volume->Priority = 1.0f;
			Volume->BlendWeight = 1.0f;
			Volume->UnregisterAllComponents();
			Volume->RegisterAllComponents();
			Lens = Volume;
			LensSaid.Reset();
		}
		Volume->bEnabled = true;
		FPostProcessSettings& Settings = Volume->Settings;

		// The sharpening, the tonemapper's own (it runs after the temporal upscale, before the window's last enlargement)
		const float Sharpen = FMath::Clamp(CVarCrispSharpen.GetValueOnGameThread(), 0.0f, 1.0f);
		Settings.bOverride_Sharpen = bCrisp;
		Settings.Sharpen = bCrisp ? Sharpen : 0.0f;
		if (bCrisp)
		{
			KeepPlatesPlain(World);
		}

		// The far shade: whole out to half the reach, then fading to nothing at the reach; its radius in the world, so a
		// man at 150 m has a shade no wider than he is (the engine's own radius is a share of the view and grows far off)
		const float Strength = FMath::Clamp(bClosedField ? CVarDepthClosed.GetValueOnGameThread() : CVarDepthStrength.GetValueOnGameThread(), 0.0f, 1.0f);
		const float Reach = FMath::Clamp(CVarDepthReach.GetValueOnGameThread(), 80.0f, 5000.0f);
		const float Radius = FMath::Clamp(CVarDepthRadius.GetValueOnGameThread(), 0.25f, 20.0f);
		const float Quality = FMath::Clamp(CVarDepthQuality.GetValueOnGameThread(), 0.0f, 100.0f);
		Settings.bOverride_AmbientOcclusionIntensity = bDepth;
		Settings.AmbientOcclusionIntensity = Strength;
		Settings.bOverride_AmbientOcclusionFadeDistance = bDepth;
		Settings.AmbientOcclusionFadeDistance = Reach * UnitsPerMetre;
		Settings.bOverride_AmbientOcclusionFadeRadius = bDepth;
		Settings.AmbientOcclusionFadeRadius = 0.5f * Reach * UnitsPerMetre;
		Settings.bOverride_AmbientOcclusionRadius = bDepth;
		Settings.AmbientOcclusionRadius = Radius * UnitsPerMetre;
		Settings.bOverride_AmbientOcclusionRadiusInWS = bDepth;
		Settings.AmbientOcclusionRadiusInWS = 1;
		Settings.bOverride_AmbientOcclusionQuality = bDepth;
		Settings.AmbientOcclusionQuality = Quality;

		const FString Said = FString::Printf(TEXT("sharpening %s; far shade %s"),
			bCrisp ? *FString::Printf(TEXT("%.2f"), Sharpen) : TEXT("off"),
			bDepth ? *FString::Printf(TEXT("%.2f strong%s, whole to %.0f m and gone at %.0f m, %.1f m round a thing, quality %.0f"), Strength,
				bClosedField ? TEXT(" (a closed canopy)") : TEXT(""), 0.5f * Reach, Reach, Radius, Quality) : CVarDepth.GetValueOnGameThread() != 0 ? TEXT("waiting for a ground battle") : TEXT("off"));
		if (Said != LensSaid)
		{
			LensSaid = Said;
			UE_LOG(LogTheater, Display, TEXT("Whole look: the lens ON: %s (the engine's own: no sharpening; shade 0.50, 30 to 80 m, its radius a share of the view, quality 50)"), *Said);
			// (the engine's own setting, when anything has set it to 0 or more, overrides every volume's and camera's
			// sharpening: the picture then is sharpened at that, not at this)
			const IConsoleVariable* Engine = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Tonemapper.Sharpen"));
			if (bCrisp && Engine && Engine->GetFloat() >= 0.0f)
			{
				UE_LOG(LogTheater, Warning, TEXT("Whole look: r.Tonemapper.Sharpen is set to %.2f, which overrides the crisp picture's %.2f (set by the console, an ini or a timing round; -1 lets the look's own number hold)"),
					Engine->GetFloat(), Sharpen);
			}
		}
	}

	// ---- "-TheaterLookTime=": the looks timed against one another inside one ground battle ----------------------------
	//
	// The map's timing rounds (the photographer's "-TheaterTime=") hold one view; a ground battle cannot be held, and its
	// cost swings from one shot to the next and as the fanless Mac warms (the look reel's R1: two windows five seconds
	// apart differ by about 3 ms a frame, and the runs off differed from one another by 1 to 7 ms in the same battle
	// seconds). So here the baseline (the first setting) and the others take turns every few seconds through the whole
	// battle, the baseline between every two others (A B A C A D A B ...), and each other window is read against the mean
	// of the baseline's windows on either side of it: the battle's swing and the warming fall on both alike. Frames longer
	// than a tenth of a second (a still being written, a field being baked) are counted and left out.
	//   -TheaterLookTime=off:tc.Look.Crisp=0,tc.Look.Depth=0|crisp:tc.Look.Crisp=1|depth:tc.Look.Depth=1
	//   -TheaterLookTimeSettle= (seconds, 0.5), -TheaterLookTimeMeasure= (seconds, 2)
	// Between two windows every setting any of them names goes back to what the launch made it, so each names only its
	// own. It begins 3 s after the field is laid, reports every 24 windows and as the field goes ("Whole look timing:").

	struct FLookTimeSet
	{
		FString Name;
		TArray<TPair<FString, FString>> Vars;
	};

	/** One window timed: which setting, and its readings (Frames 0: none to time). */
	struct FLookTimeWindow
	{
		int32 Set = 0;
		int32 Frames = 0;
		float Fps = 0.0f, Game = 0.0f, Draw = 0.0f, Chip = 0.0f;
	};

	TArray<FLookTimeSet> LookTimeSets;
	TArray<FLookTimeWindow> LookTimeWindows;
	TMap<FString, FString> LookTimeKept; // (each setting's value before the first window laid it: the launch's own)
	FTSTicker::FDelegateHandle LookTimeClock;
	float LookTimeSettle = 0.5f, LookTimeMeasure = 2.0f, LookTimeWait = 0.0f, LookTimeIn = 0.0f;
	int32 LookTimeSlot = -1, LookTimeAt = INDEX_NONE, LookTimeFrames = 0, LookTimeLong = 0;
	double LookTimeSum = 0.0, LookTimeGame = 0.0, LookTimeDraw = 0.0, LookTimeChip = 0.0;
	bool bLookTimeDone = false;

	/** The setting timed in a slot: the baseline in every even one, the others in turn in the odd ones. */
	int32 LookTimeSetOf(int32 Slot)
	{
		return Slot % 2 == 0 ? 0 : 1 + (Slot / 2) % (LookTimeSets.Num() - 1);
	}

	/** Every setting any window has laid put back to the launch's own, then the setting's laid (none: INDEX_NONE). */
	void LayLookTimeSet(int32 Index)
	{
		for (const TPair<FString, FString>& Was : LookTimeKept)
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Was.Key))
			{
				Variable->Set(*Was.Value, ECVF_SetByConsole);
			}
		}
		if (LookTimeSets.IsValidIndex(Index))
		{
			for (const TPair<FString, FString>& Each : LookTimeSets[Index].Vars)
			{
				IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Each.Key);
				if (!Variable)
				{
					UE_LOG(LogTheater, Warning, TEXT("Whole look timing: no such setting: %s (in %s)"), *Each.Key, *LookTimeSets[Index].Name);
					continue;
				}
				if (!LookTimeKept.Contains(Each.Key))
				{
					LookTimeKept.Add(Each.Key, Variable->GetString());
				}
				Variable->Set(*Each.Value, ECVF_SetByConsole);
			}
		}
		// (at once, not on the clock's next turn: the settle starts from the setting as laid)
		KeepFiltering();
		KeepLens();
	}

	float LookTimeShare(TArray<float> Values, float Share) // (the value a share of the way up the sorted values; 0.5 the middle)
	{
		Values.Sort();
		if (Values.Num() == 0)
		{
			return 0.0f;
		}
		const float At = Share * static_cast<float>(Values.Num() - 1);
		const int32 Low = FMath::FloorToInt(At);
		const int32 High = FMath::Min(Low + 1, Values.Num() - 1);
		return FMath::Lerp(Values[Low], Values[High], At - static_cast<float>(Low));
	}

	/** The mean of some values and its standard error (the spread of the values over the root of their count). */
	void LookTimeMean(const TArray<float>& Values, float& OutMean, float& OutError)
	{
		OutMean = OutError = 0.0f;
		const int32 Num = Values.Num();
		if (Num == 0)
		{
			return;
		}
		double Sum = 0.0, Squares = 0.0;
		for (const float Each : Values)
		{
			Sum += Each;
		}
		const double Mean = Sum / Num;
		for (const float Each : Values)
		{
			Squares += (Each - Mean) * (Each - Mean);
		}
		OutMean = static_cast<float>(Mean);
		OutError = Num > 1 ? static_cast<float>(FMath::Sqrt(Squares / (Num - 1)) / FMath::Sqrt(static_cast<double>(Num))) : 0.0f;
	}

	/** Each setting's middle reading; and each other one's difference from the baseline, each of its windows against the
	 * mean of the baseline's windows on either side of it: the middle of them and their middle half, and their mean and
	 * its standard error. */
	void ReportLookTime(bool bLast)
	{
		const int32 Windows = LookTimeWindows.Num();
		for (int32 Index = 0; Index < LookTimeSets.Num(); ++Index)
		{
			TArray<float> Chip, Fps, Game, Draw, ChipDiff, FpsDiff;
			for (int32 At = 0; At < Windows; ++At)
			{
				const FLookTimeWindow& Here = LookTimeWindows[At];
				if (Here.Set != Index || Here.Frames == 0)
				{
					continue;
				}
				Chip.Add(Here.Chip);
				Fps.Add(Here.Fps);
				Game.Add(Here.Game);
				Draw.Add(Here.Draw);
				if (Index != 0 && At > 0 && At + 1 < Windows)
				{
					const FLookTimeWindow& Before = LookTimeWindows[At - 1];
					const FLookTimeWindow& After = LookTimeWindows[At + 1];
					if (Before.Set == 0 && After.Set == 0 && Before.Frames > 0 && After.Frames > 0)
					{
						ChipDiff.Add(Here.Chip - 0.5f * (Before.Chip + After.Chip));
						FpsDiff.Add(Here.Fps - 0.5f * (Before.Fps + After.Fps));
					}
				}
			}
			float Mean = 0.0f, Error = 0.0f;
			LookTimeMean(ChipDiff, Mean, Error);
			const FString Against = Index == 0 ? FString(TEXT("the baseline"))
				: FString::Printf(TEXT("against %s on either side: graphics chip %+.2f ms a frame (the middle; the middle half %+.2f to %+.2f), mean %+.2f give or take %.2f, from %d windows; %+.1f frames a second (the middle)"),
					*LookTimeSets[0].Name, LookTimeShare(ChipDiff, 0.5f), LookTimeShare(ChipDiff, 0.25f), LookTimeShare(ChipDiff, 0.75f), Mean, Error,
					ChipDiff.Num(), LookTimeShare(FpsDiff, 0.5f));
			UE_LOG(LogTheater, Display, TEXT("Whole look timing: %s%d windows, %s: graphics chip %.2f ms a frame, %.1f frames a second, game %.2f ms, draw %.2f ms (the middle of its %d); %s"),
				bLast ? TEXT("THE WHOLE BATTLE, ") : TEXT("so far, "), Windows, *LookTimeSets[Index].Name, LookTimeShare(Chip, 0.5f), LookTimeShare(Fps, 0.5f),
				LookTimeShare(Game, 0.5f), LookTimeShare(Draw, 0.5f), Chip.Num(), *Against);
		}
	}

	/** The timing's end: the last report, and every setting back to the launch's own. */
	void EndLookTime()
	{
		if (bLookTimeDone || LookTimeSlot < 0)
		{
			return;
		}
		bLookTimeDone = true;
		ReportLookTime(true);
		LayLookTimeSet(INDEX_NONE);
	}

	bool TickLookTime(float DeltaSeconds)
	{
		if (bLookTimeDone)
		{
			LookTimeClock.Reset();
			return false;
		}
		if (!bFieldOn)
		{
			return true; // (FieldGone ends it; until a field is laid, nothing)
		}
		if (LookTimeSlot < 0)
		{
			LookTimeWait += DeltaSeconds;
			if (LookTimeWait < 3.0f)
			{
				return true;
			}
			LookTimeSlot = 0;
			LookTimeAt = LookTimeSetOf(0);
			LayLookTimeSet(LookTimeAt);
			UE_LOG(LogTheater, Display, TEXT("Whole look timing: begun, %d settings (the first, %s, the baseline, between every two others), each settled %.1f s and timed %.1f s, until the battle ends"),
				LookTimeSets.Num(), *LookTimeSets[0].Name, LookTimeSettle, LookTimeMeasure);
			return true;
		}
		LookTimeIn += DeltaSeconds;
		if (LookTimeIn > LookTimeSettle)
		{
			if (DeltaSeconds > 0.1f)
			{
				++LookTimeLong; // (a still being written, a field being baked: counted, left out)
			}
			else
			{
				++LookTimeFrames;
				LookTimeSum += DeltaSeconds;
				LookTimeGame += FPlatformTime::ToMilliseconds(GGameThreadTime);
				LookTimeDraw += FPlatformTime::ToMilliseconds(GRenderThreadTime);
				LookTimeChip += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
			}
		}
		if (LookTimeIn < LookTimeSettle + LookTimeMeasure)
		{
			return true;
		}
		FLookTimeWindow& Window = LookTimeWindows.AddDefaulted_GetRef();
		Window.Set = LookTimeAt;
		if (LookTimeFrames > 0 && LookTimeSum > 0.0)
		{
			Window.Frames = LookTimeFrames;
			Window.Fps = static_cast<float>(LookTimeFrames / LookTimeSum);
			Window.Game = static_cast<float>(LookTimeGame / LookTimeFrames);
			Window.Draw = static_cast<float>(LookTimeDraw / LookTimeFrames);
			Window.Chip = static_cast<float>(LookTimeChip / LookTimeFrames);
		}
		UE_LOG(LogTheater, Display, TEXT("Whole look timing: window %d, %s: %.1f frames a second (game %.2f ms, draw %.2f ms, graphics chip %.2f ms), %d frames timed, %d longer than 0.1 s left out"),
			LookTimeWindows.Num(), *LookTimeSets[LookTimeAt].Name, Window.Fps, Window.Game, Window.Draw, Window.Chip, LookTimeFrames, LookTimeLong);
		LookTimeFrames = LookTimeLong = 0;
		LookTimeSum = LookTimeGame = LookTimeDraw = LookTimeChip = 0.0;
		LookTimeIn = 0.0f;
		if (LookTimeWindows.Num() % 24 == 0)
		{
			ReportLookTime(false);
		}
		LookTimeAt = LookTimeSetOf(++LookTimeSlot);
		LayLookTimeSet(LookTimeAt);
		return true;
	}

	/** "-TheaterLookTime=name:setting=value,setting=value|name:...", read as the game starts. */
	void StartLookTime()
	{
		FString Asked;
		if (!FParse::Value(FCommandLine::Get(), TEXT("TheaterLookTime="), Asked, false))
		{
			return;
		}
		TArray<FString> Sets;
		Asked.ParseIntoArray(Sets, TEXT("|"), true);
		for (const FString& Each : Sets)
		{
			FLookTimeSet Set;
			FString Vars;
			if (!Each.Split(TEXT(":"), &Set.Name, &Vars))
			{
				Set.Name = Each;
			}
			TArray<FString> Pairs;
			Vars.ParseIntoArray(Pairs, TEXT(","), true);
			for (const FString& Pair : Pairs)
			{
				FString Key, Value;
				if (Pair.Split(TEXT("="), &Key, &Value))
				{
					Set.Vars.Emplace(Key.TrimStartAndEnd(), Value.TrimStartAndEnd());
				}
			}
			LookTimeSets.Add(MoveTemp(Set));
		}
		if (LookTimeSets.Num() < 2)
		{
			UE_LOG(LogTheater, Warning, TEXT("Whole look timing: %d settings asked; a baseline and one or more others are timed against it (none timed)"), LookTimeSets.Num());
			LookTimeSets.Reset();
			return;
		}
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLookTimeSettle="), LookTimeSettle);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterLookTimeMeasure="), LookTimeMeasure);
		LookTimeSettle = FMath::Clamp(LookTimeSettle, 0.25f, 10.0f);
		LookTimeMeasure = FMath::Clamp(LookTimeMeasure, 1.0f, 30.0f);
		LookTimeClock = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickLookTime), 0.0f);
		UE_LOG(LogTheater, Display, TEXT("Whole look timing: %d settings read; they take turns once a ground battle's field is laid"), LookTimeSets.Num());
	}

	// ---- The clock -----------------------------------------------------------------------------------------------------

	FTSTicker::FDelegateHandle Clock;

	bool Turn(float DeltaSeconds)
	{
		KeepFiltering();
		KeepLens();
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

	/** "-TheaterLookBounceStrength=1.6" and the like: a look's tuning set from the launch line, for the Mac's sweeps. */
	template <typename T>
	void Tuned(const TCHAR* Word, TAutoConsoleVariable<T>& Variable)
	{
		T Value{};
		if (FParse::Value(FCommandLine::Get(), Word, Value))
		{
			Variable->Set(Value, ECVF_SetByCode);
		}
	}

	// ---- The bounce held through a night before the attack (BounceAtDawn to Dawn) --------------------------------------

	TArray<TWeakObjectPtr<USkyLightComponent>> DawnSkies;
	FLinearColor DawnBounce(0.0f, 0.0f, 0.0f, 1.0f);

	// ---- The bounce's colour -----------------------------------------------------------------------------------------

	float ToPicture(float Light) // (linear light to the sRGB a picture keeps)
	{
		Light = FMath::Max(Light, 0.0f);
		return Light <= 0.0031308f ? 12.92f * Light : 1.055f * FMath::Pow(Light, 1.0f / 2.4f) - 0.055f;
	}

	float ToLight(float Picture)
	{
		Picture = FMath::Max(Picture, 0.0f);
		return Picture <= 0.04045f ? Picture / 12.92f : FMath::Pow((Picture + 0.055f) / 1.055f, 2.4f);
	}

	float ValueOf(const FLinearColor& Light)
	{
		return 0.2126f * Light.R + 0.7152f * Light.G + 0.0722f * Light.B;
	}

	/** The art director's box (25 Nov 04:46Z): the hue pulled inside red/green 0.90 to 1.15 and blue/green 0.80 to 1.05,
	 * read as a picture keeps it (her swatches), and the value kept: Normandy's 110,135,96 becomes about 118,132,105, the
	 * Mediterranean's 108,88,63 about 102,89,71; the desert, the steppe and the Russian summer are inside already. */
	FLinearColor Boxed(const FLinearColor& Light, float RedLeast = 0.90f, float RedMost = 1.15f, float BlueLeast = 0.80f, float BlueMost = 1.05f)
	{
		const float R = ToPicture(Light.R), G = ToPicture(Light.G), B = ToPicture(Light.B);
		if (G <= UE_KINDA_SMALL_NUMBER)
		{
			return Light; // (a black ground gives nothing back to pull)
		}
		FLinearColor Out(ToLight(G * FMath::Clamp(R / G, RedLeast, RedMost)), ToLight(G), ToLight(G * FMath::Clamp(B / G, BlueLeast, BlueMost)), 1.0f);
		const float Was = ValueOf(Light), Now = ValueOf(Out);
		if (Now > UE_KINDA_SMALL_NUMBER)
		{
			Out *= Was / Now;
		}
		Out.A = 1.0f;
		return Out;
	}
}

void WholeLook::Start()
{
	if (IsRunningCommandlet())
	{
		return; // (a cook or a tool: no picture to look at)
	}
	Asked(TEXT("TheaterLookCrisp"), CVarCrisp);
	Asked(TEXT("TheaterLookDepth"), CVarDepth);
	Asked(TEXT("TheaterLookBounce"), CVarBounce);
	Tuned(TEXT("TheaterLookCrispSharpen="), CVarCrispSharpen);
	Tuned(TEXT("TheaterLookCrispFiltering="), CVarCrispFiltering);
	Tuned(TEXT("TheaterLookDepthStrength="), CVarDepthStrength);
	Tuned(TEXT("TheaterLookDepthReach="), CVarDepthReach);
	Tuned(TEXT("TheaterLookDepthRadius="), CVarDepthRadius);
	Tuned(TEXT("TheaterLookDepthQuality="), CVarDepthQuality);
	Tuned(TEXT("TheaterLookDepthClosed="), CVarDepthClosed);
	Tuned(TEXT("TheaterLookBounceStrength="), CVarBounceStrength);
	Tuned(TEXT("TheaterLookBounceClosed="), CVarBounceClosed);
	Asked(TEXT("TheaterLookWorn"), CVarWorn);
	Tuned(TEXT("TheaterLookWornStrength="), CVarWornStrength);
	Tuned(TEXT("TheaterLookWornRepeat="), CVarWornRepeat);
	Tuned(TEXT("TheaterLookWornRoughness="), CVarWornRoughness);
	Tuned(TEXT("TheaterLookWornRepeat2="), CVarWornRepeat2);
	Tuned(TEXT("TheaterLookWornMix="), CVarWornMix);
	Tuned(TEXT("TheaterLookWornSheen="), CVarWornSheen);
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterLookBounceUnclamped")))
	{
		CVarBounceHue->Set(0, ECVF_SetByCode);
	}
	StartLookTime();
	// (the filtering and the lens both wait for the game's world: the clock lays them)
	if (!Clock.IsValid())
	{
		Clock = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Turn), 0.25f);
	}
}

void WholeLook::Stop()
{
	// (the clocks only: the engine is going, and its settings and pictures with it)
	if (Clock.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Clock);
		Clock.Reset();
	}
	if (LookTimeClock.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LookTimeClock);
		LookTimeClock.Reset();
	}
}

bool WholeLook::BounceOn()
{
	return CVarBounce.GetValueOnGameThread() != 0;
}

FLinearColor WholeLook::GroundMean(TFunctionRef<FColor(double X, double Y)> ColourAt, double Wide, double Deep)
{
	// (the paint is sRGB, as a picture keeps it; light adds in linear, so each is made linear before the mean. A point
	// given alpha 0 is not counted: water, which mirrors the sky and throws no diffuse light back, the art director)
	constexpr int32 Across = 24;
	FLinearColor Sum(0.0f, 0.0f, 0.0f, 0.0f);
	int32 Count = 0;
	for (int32 Row = 0; Row < Across; ++Row)
	{
		for (int32 Col = 0; Col < Across; ++Col)
		{
			const double X = (Col + 0.5) / Across * Wide, Y = (Row + 0.5) / Across * Deep;
			const FColor Paint = ColourAt(X, Y);
			if (Paint.A == 0)
			{
				continue;
			}
			Sum += FLinearColor::FromSRGBColor(FColor(Paint.R, Paint.G, Paint.B, 255));
			++Count;
		}
	}
	if (Count < Across * Across)
	{
		UE_LOG(LogTheater, Display, TEXT("Whole look: the ground's bounce: %d of the field's %d points are water, not counted"), Across * Across - Count, Across * Across);
	}
	return Count > 0 ? FLinearColor(Sum.R / Count, Sum.G / Count, Sum.B / Count, 1.0f) : FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

FLinearColor WholeLook::BounceLight(const FLinearColor& Mean, bool bClosedCanopy, double SunHeight)
{
	const bool bHue = CVarBounceHue.GetValueOnGameThread() != 0;
	const float Strength = FMath::Clamp(CVarBounceStrength.GetValueOnGameThread(), 0.0f, 4.0f);
	const float Closed = bClosedCanopy ? FMath::Clamp(CVarBounceClosed.GetValueOnGameThread(), 0.0f, 1.0f) : 1.0f;
	// (the bounce follows the sun: none with it under the horizon or at deep dusk, whole from 15 degrees up; a daylight fill
	// under a low sun kills the evening, the art director)
	const float Sun = static_cast<float>(FMath::Clamp(SunHeight / FMath::Sin(FMath::DegreesToRadians(15.0)), 0.0, 1.0));
	FLinearColor Out = (bHue ? Boxed(Mean) : Mean) * (Strength * Closed * Sun);
	Out.A = 1.0f;
	UE_LOG(LogTheater, Display, TEXT("Whole look: the ground's bounce: the field's mean paint %.3f, %.3f, %.3f (linear), %s, times %.2f strength%s, the sun's share %.2f: %.3f, %.3f, %.3f"),
		Mean.R, Mean.G, Mean.B, bHue ? TEXT("its hue pulled inside the art director's box") : TEXT("its own hue (unclamped)"), Strength,
		bClosedCanopy ? *FString::Printf(TEXT(", times %.2f under a closed canopy"), Closed) : TEXT(""), Sun, Out.R, Out.G, Out.B);
	return Out;
}

double WholeLook::SunHeight(double Lat, int32 Month, double Hour)
{
	// (as LandBattle's SunWay reckons it, the middle of the month, without its floor of 8 degrees)
	const double Day = (Month - 1) * 30.4 + 15.0;
	const double Decl = FMath::DegreesToRadians(23.44) * FMath::Sin(2.0 * UE_DOUBLE_PI * (284.0 + Day) / 365.0);
	const double Phi = FMath::DegreesToRadians(Lat);
	const double H = FMath::DegreesToRadians(15.0 * (Hour - 12.0));
	return FMath::Clamp(FMath::Sin(Phi) * FMath::Sin(Decl) + FMath::Cos(Phi) * FMath::Cos(Decl) * FMath::Cos(H), -1.0, 1.0);
}

void WholeLook::BounceAtDawn(const TArray<USkyLightComponent*>& Skies, const FLinearColor& Bounce)
{
	DawnSkies.Reset();
	for (USkyLightComponent* Sky : Skies)
	{
		if (Sky)
		{
			DawnSkies.Add(Sky);
		}
	}
	DawnBounce = FLinearColor(Bounce.R, Bounce.G, Bounce.B, 1.0f);
	UE_LOG(LogTheater, Display, TEXT("Whole look: the ground's bounce waits for the dawn: the night before the attack keeps the sky's lower half as shipped (%d sky lights)"), DawnSkies.Num());
}

void WholeLook::Dawn()
{
	if (DawnSkies.Num() == 0)
	{
		return;
	}
	int32 Laid = 0;
	for (const TWeakObjectPtr<USkyLightComponent>& Each : DawnSkies)
	{
		if (USkyLightComponent* Sky = Each.Get())
		{
			// (as the ground battle lays it by day: the lower half a solid colour, the flag the engine still calls "black")
			Sky->bLowerHemisphereIsBlack = true;
			Sky->SetLowerHemisphereColor(DawnBounce);
			Sky->RecaptureSky();
			++Laid;
		}
	}
	DawnSkies.Reset();
	UE_LOG(LogTheater, Display, TEXT("Whole look: the ground's bounce laid at the dawn: %.3f, %.3f, %.3f under %d sky lights"), DawnBounce.R, DawnBounce.G, DawnBounce.B, Laid);
}

namespace
{
	// ---- Worn: the vehicles' paint and the men's sheen ------------------------------------------------------------------

	/** The battle now opening wears its vehicles' paint (WornOpen: the switch on and M_LandWorn built). */
	bool bWornBattle = false;

	/** M_LandWorn (WornOpen looks for it again each battle until it is found). */
	TWeakObjectPtr<UMaterialInterface> WornParent;

	/** Each vehicle's own worn ink, by her number in the battle (VehicleInk), for the battle WornOpen opened. */
	TMap<int32, TWeakObjectPtr<UMaterialInstanceDynamic>> WornInks;

	/** The men's worn parents (MenParent): the near men's and the crowd's; whether each was looked for and not found
	 * (build_content.py not run: not looked for again, said once); a found one the collector freed between battles, when
	 * nothing held it, is loaded again (the 3D pre-check, 25 Nov 12:28Z). */
	TWeakObjectPtr<UMaterialInterface> MenParents[2];
	bool bMenMissing[2] = { false, false };

	/** A material of the campaign's, quietly (nullptr when build_content.py has not made it). */
	UMaterialInterface* Campaign(const TCHAR* Path)
	{
		return LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** The worn paint's settings on an ink made on M_LandWorn (its other settings already copied from the steel), the
	 * patches shifted by Seed. */
	void LayWorn(UMaterialInstanceDynamic* Ink, uint32 Seed)
	{
		const float Strength = FMath::Clamp(CVarWornStrength.GetValueOnGameThread(), 0.0f, 2.0f);
		const float Repeat = FMath::Clamp(CVarWornRepeat.GetValueOnGameThread(), 0.2f, 20.0f);
		const float Repeat2 = FMath::Clamp(CVarWornRepeat2.GetValueOnGameThread(), 0.2f, 40.0f);
		const float Mix = FMath::Clamp(CVarWornMix.GetValueOnGameThread(), 0.0f, 1.0f);
		// (two patterns blended half and half are flatter than either: their swings partly cancel, by the root of the sum
		// of the shares' squares. The wear is lifted by as much, so strength 1 keeps one layer's contrast, the art
		// director's 10 of 255 on a sunlit panel)
		const float Lift = 1.0f / FMath::Sqrt(FMath::Square(Mix) + FMath::Square(1.0f - Mix));
		FRandomStream Stream(static_cast<int32>(Seed));
		const float A = Stream.FRand(), B = Stream.FRand(), C = Stream.FRand(), D = Stream.FRand();
		Ink->SetScalarParameterValue(TEXT("Wear"), Strength * Lift);
		Ink->SetScalarParameterValue(TEXT("BlotTiling"), 1.0f / (Repeat * UnitsPerMetre));
		Ink->SetScalarParameterValue(TEXT("Blot2Tiling"), 1.0f / (Repeat2 * UnitsPerMetre));
		Ink->SetScalarParameterValue(TEXT("Blot2Mix"), Mix);
		Ink->SetVectorParameterValue(TEXT("BlotShift"), FLinearColor(A, B, C, D));
		Ink->SetScalarParameterValue(TEXT("Roughness"), FMath::Clamp(CVarWornRoughness.GetValueOnGameThread(), 0.0f, 1.0f));
		Ink->SetScalarParameterValue(TEXT("WearRough"), FMath::Clamp(CVarWornSheen.GetValueOnGameThread(), 0.0f, 4.0f));
	}
}

bool WholeLook::WornOn()
{
	return CVarWorn.GetValueOnGameThread() != 0;
}

FLinearColor WholeLook::FieldDust(const FLinearColor& Mean)
{
	// (dirt is earth, never grass: red over green 1.00 to 1.25, blue over green 0.70 to 0.90, the soldiers' earth box of
	// 25 Nov 12:22Z, one box for the hulls' lower third and the men's boots and knees)
	FLinearColor Out = Boxed(Mean, 1.00f, 1.25f, 0.70f, 0.90f);
	Out.A = 1.0f;
	return Out;
}

void WholeLook::WornOpen(UMaterialInstanceDynamic* Steel)
{
	WornInks.Reset();
	bWornBattle = false;
	if (!WornOn())
	{
		return;
	}
	if (!WornParent.IsValid())
	{
		WornParent = Campaign(TEXT("/Game/Campaign/Materials/M_LandWorn.M_LandWorn"));
	}
	if (!WornParent.IsValid())
	{
		static bool bToldNoWorn = false;
		if (!bToldNoWorn)
		{
			bToldNoWorn = true;
			UE_LOG(LogTheater, Warning, TEXT("Whole look: worn is on but /Game/Campaign/Materials/M_LandWorn is missing (run Tools/build_content.sh): the vehicles keep the even coat."));
		}
		return;
	}
	bWornBattle = true;
	const float Strength = FMath::Clamp(CVarWornStrength.GetValueOnGameThread(), 0.0f, 2.0f);
	const float Mix = FMath::Clamp(CVarWornMix.GetValueOnGameThread(), 0.0f, 1.0f);
	const float Roughness = FMath::Clamp(CVarWornRoughness.GetValueOnGameThread(), 0.0f, 1.0f);
	const float Sheen = FMath::Clamp(CVarWornSheen.GetValueOnGameThread(), 0.0f, 4.0f);
	const float Was = Steel ? Steel->K2_GetScalarParameterValue(TEXT("Roughness")) : 0.0f;
	// (the mottle's swing at strength 1 runs from about 0.9 to 1.1 of the paint: the roughness moves by its own swing
	// times the sheen, and is held inside 0 to 1)
	const float Swing = 0.1f * Strength * Sheen;
	UE_LOG(LogTheater, Display, TEXT("Whole look: worn ON for this battle: the vehicles' paint broken in patches, two layers repeating every %.1f and %.1f m (the second's share %.2f), wear %.2f, each vehicle her own shift of them; roughness %.2f (the plain steel's %.2f), about %.2f on the dark patches and %.2f on the pale; the planes over the field and the old wrecks keep the plain steel"),
		FMath::Clamp(CVarWornRepeat.GetValueOnGameThread(), 0.2f, 20.0f), FMath::Clamp(CVarWornRepeat2.GetValueOnGameThread(), 0.2f, 40.0f), Mix, Strength, Roughness, Was,
		FMath::Clamp(Roughness - Swing, 0.0f, 1.0f), FMath::Clamp(Roughness + Swing, 0.0f, 1.0f));
}

UMaterialInterface* WholeLook::VehicleInk(UMaterialInstanceDynamic* Steel, int32 Vehicle, UObject* Outer)
{
	UMaterialInterface* Worn = WornParent.Get();
	if (!bWornBattle || !Steel || !Outer || !Worn)
	{
		return Steel;
	}
	if (const TWeakObjectPtr<UMaterialInstanceDynamic>* Made = WornInks.Find(Vehicle))
	{
		if (UMaterialInstanceDynamic* Ink = Made->Get())
		{
			return Ink;
		}
	}
	UMaterialInstanceDynamic* Ink = UMaterialInstanceDynamic::Create(Worn, Outer);
	if (!Ink)
	{
		return Steel;
	}
	// (the steel's own settings, as LoadInks gave them, then the worn paint's; the shift seeded by her number, so a troop
	// of one model in line wears as many stains as it has tanks, and the same ones every time the battle is shown)
	Ink->CopyParameterOverrides(Steel);
	LayWorn(Ink, HashCombine(GetTypeHash(Vehicle), 0x9E3779B9u));
	WornInks.Add(Vehicle, Ink);
	return Ink;
}

UMaterialInterface* WholeLook::FarParent()
{
	return bWornBattle ? WornParent.Get() : nullptr;
}

void WholeLook::WearFar(UMaterialInstanceDynamic* FarInk, const FString& Model)
{
	if (!bWornBattle || !FarInk || !WornParent.IsValid() || FarInk->Parent != WornParent.Get())
	{
		return;
	}
	LayWorn(FarInk, HashCombine(GetTypeHash(Model), 0x85EBCA6Bu));
}

UMaterialInterface* WholeLook::MenParent(bool bCrowd)
{
	if (!WornOn())
	{
		return nullptr;
	}
	const int32 Which = bCrowd ? 1 : 0;
	if (!MenParents[Which].IsValid() && !bMenMissing[Which])
	{
		MenParents[Which] = Campaign(bCrowd ? TEXT("/Game/Campaign/Materials/M_LandCrowdWorn.M_LandCrowdWorn") : TEXT("/Game/Campaign/Materials/M_LandManWorn.M_LandManWorn"));
		if (!MenParents[Which].IsValid())
		{
			bMenMissing[Which] = true;
			UE_LOG(LogTheater, Warning, TEXT("Whole look: worn is on but /Game/Campaign/Materials/%s is missing (run Tools/build_content.sh): the men keep one roughness, and their paint's roughness is not written."),
				bCrowd ? TEXT("M_LandCrowdWorn") : TEXT("M_LandManWorn"));
		}
	}
	return MenParents[Which].Get();
}

bool WholeLook::MenRough()
{
	return MenParent(false) && MenParent(true);
}

UMaterialInstanceDynamic* WholeLook::MenInk(UMaterialInstanceDynamic* Made, UObject* Outer)
{
	UMaterialInterface* Worn = MenRough() ? MenParent(false) : nullptr;
	if (!Made || !Outer || !Worn)
	{
		return Made;
	}
	UMaterialInstanceDynamic* Ink = UMaterialInstanceDynamic::Create(Worn, Outer);
	if (!Ink)
	{
		return Made;
	}
	Ink->CopyParameterOverrides(Made);
	UE_LOG(LogTheater, Display, TEXT("Whole look: worn ON for the men: each paint's roughness read from its points' alpha (written by the men's looks), a point left whole keeping %.2f"),
		Made->K2_GetScalarParameterValue(TEXT("Roughness")));
	return Ink;
}

void WholeLook::FieldLaid(bool bClosedCanopy)
{
	bFieldOn = true;
	bClosedField = bClosedCanopy;
	KeepLens(); // (at once, not on the clock's next turn)
}

void WholeLook::FieldGone()
{
	EndLookTime(); // (the timing's last report, its settings put back, while the field's looks still stand)
	bFieldOn = false;
	bClosedField = false;
	DawnSkies.Reset();
	WornInks.Reset();
	KeepLens();
}
