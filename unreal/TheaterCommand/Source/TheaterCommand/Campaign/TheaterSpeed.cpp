#include "Campaign/TheaterSpeed.h"

#include "TheaterCommand.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "RenderTimer.h"
#include "RHI.h"
#include "UObject/UObjectIterator.h"

CSV_DEFINE_CATEGORY(Theater, true);

namespace
{
	TAutoConsoleVariable<int32> CVarSpeedLog(
		TEXT("tc.Speed.Log"), 0,
		TEXT("1 writes the speed log every five seconds: frames, the map's jobs, the rules' calls (as -TheaterSpeedLog)."));
	TAutoConsoleVariable<int32> CVarTownSquares(
		TEXT("tc.Speed.TownSquares"), 1,
		TEXT("1 cuts the towns into squares of country, so that only those in view are drawn; 0 builds them in one piece, as before."));
	TAutoConsoleVariable<int32> CVarTownShadows(
		TEXT("tc.Speed.TownShadows"), 1,
		TEXT("(for measuring only: it changes the picture) 0 stops the near towns throwing shadows."));
	TAutoConsoleVariable<int32> CVarSmokeTrim(
		TEXT("tc.Speed.SmokeTrim"), 1,
		TEXT("1 sends only as much of the smoke as is in use to the graphics chip each frame; 0 sends all of it, as before."));
	TAutoConsoleVariable<int32> CVarRulesMemo(
		TEXT("tc.Speed.RulesMemo"), 1,
		TEXT("1 answers the odds and the like from the last asking while nothing has changed; 0 asks the rules every time."));
	TAutoConsoleVariable<int32> CVarTrainShadows(
		TEXT("tc.Speed.TrainShadows"), 1,
		TEXT("(for measuring only: it changes the picture) 0 stops the trains throwing shadows."));

	bool bLogging = false;

	struct FSlot
	{
		FString Name;
		uint64 Frame = 0; // this frame's time so far
		uint64 Total = 0; // over the summary's five seconds
		uint64 Worst = 0; // the most in any one frame of them
		int32 Calls = 0;
	};
	TArray<FSlot>& Slots()
	{
		static TArray<FSlot> Each;
		return Each;
	}

	struct FRulesTally
	{
		int32 Calls = 0;
		int32 FromMemo = 0;
		double Total = 0.0;
		double Worst = 0.0;
	};
	TMap<FString, FRulesTally>& RulesTallies()
	{
		static TMap<FString, FRulesTally> Each;
		return Each;
	}

	double WindowClock = 0.0;
	int32 WindowFrames = 0;
	int32 WindowSlow = 0;
	double WindowWorst = 0.0;
	double WindowGame = 0.0, WindowDraw = 0.0, WindowChip = 0.0;
	int32 Summaries = 0;

	double Ms(uint64 Cycles)
	{
		return FPlatformTime::ToMilliseconds64(Cycles);
	}

	/** What is drawn: every part of the world that is registered and shown, how many throw shadows, and the largest meshes. */
	void Census(UWorld* World)
	{
		struct FEntry
		{
			FString Name;
			int32 Points = 0;
			int32 Triangles = 0;
			int32 Sections = 0;
			bool bShadow = false;
		};
		TArray<FEntry> Meshes;
		int32 Parts = 0, Shown = 0, Shadowed = 0, Instanced = 0, Instances = 0;
		int64 Points = 0, Triangles = 0, ShadowTriangles = 0;
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			UPrimitiveComponent* Part = *It;
			if (!Part || Part->GetWorld() != World || !Part->IsRegistered())
			{
				continue;
			}
			++Parts;
			if (!Part->IsVisible())
			{
				continue;
			}
			++Shown;
			const bool bShadow = Part->CastShadow != 0;
			Shadowed += bShadow ? 1 : 0;
			if (UProceduralMeshComponent* Mesh = Cast<UProceduralMeshComponent>(Part))
			{
				FEntry Entry;
				Entry.Name = Mesh->GetOwner() ? FString::Printf(TEXT("%s.%s"), *Mesh->GetOwner()->GetName(), *Mesh->GetName()) : Mesh->GetName();
				Entry.bShadow = bShadow;
				for (int32 Index = 0; Index < Mesh->GetNumSections(); ++Index)
				{
					const FProcMeshSection* Section = Mesh->GetProcMeshSection(Index);
					if (Section && Section->bSectionVisible)
					{
						Entry.Points += Section->ProcVertexBuffer.Num();
						Entry.Triangles += Section->ProcIndexBuffer.Num() / 3;
						++Entry.Sections;
					}
				}
				Points += Entry.Points;
				Triangles += Entry.Triangles;
				ShadowTriangles += bShadow ? Entry.Triangles : 0;
				if (Entry.Points > 0)
				{
					Meshes.Add(MoveTemp(Entry));
				}
			}
			else if (const UInstancedStaticMeshComponent* Copies = Cast<UInstancedStaticMeshComponent>(Part))
			{
				++Instanced;
				Instances += Copies->GetInstanceCount();
			}
		}
		Meshes.Sort([](const FEntry& A, const FEntry& B) { return A.Points > B.Points; });
		UE_LOG(LogTheater, Display, TEXT("Speed census: %d parts registered, %d shown, %d of them throwing shadows; procedural meshes shown: %lld points, %lld triangles (%lld throwing shadows); %d instanced parts with %d copies"),
			Parts, Shown, Shadowed, Points, Triangles, ShadowTriangles, Instanced, Instances);
		for (int32 Index = 0; Index < FMath::Min(Meshes.Num(), 16); ++Index)
		{
			const FEntry& Each = Meshes[Index];
			UE_LOG(LogTheater, Display, TEXT("Speed census:   %s: %d points, %d triangles, %d section%s%s"),
				*Each.Name, Each.Points, Each.Triangles, Each.Sections, Each.Sections == 1 ? TEXT("") : TEXT("s"), Each.bShadow ? TEXT(", throws shadows") : TEXT(""));
		}
	}
}

namespace TheaterSpeed
{
	bool IsLogging()
	{
		return bLogging;
	}

	bool TownSquares()
	{
		return CVarTownSquares.GetValueOnGameThread() != 0;
	}

	bool TownShadows()
	{
		return CVarTownShadows.GetValueOnGameThread() != 0;
	}

	bool SmokeTrim()
	{
		return CVarSmokeTrim.GetValueOnGameThread() != 0;
	}

	bool RulesMemo()
	{
		return CVarRulesMemo.GetValueOnGameThread() != 0;
	}

	bool TrainShadows()
	{
		return CVarTrainShadows.GetValueOnGameThread() != 0;
	}

	int32 SlotFor(const TCHAR* Name)
	{
		TArray<FSlot>& Each = Slots();
		const int32 Found = Each.IndexOfByPredicate([Name](const FSlot& Slot) { return Slot.Name == Name; });
		if (Found != INDEX_NONE)
		{
			return Found;
		}
		FSlot& Slot = Each.AddDefaulted_GetRef();
		Slot.Name = Name;
		return Each.Num() - 1;
	}

	void AddTime(int32 Slot, uint64 Cycles)
	{
		TArray<FSlot>& Each = Slots();
		if (Each.IsValidIndex(Slot))
		{
			Each[Slot].Frame += Cycles;
			++Each[Slot].Calls;
		}
	}

	void NoteRulesCall(const FString& Function, double Seconds, bool bFromMemo)
	{
		if (!bLogging)
		{
			return;
		}
		FRulesTally& Tally = RulesTallies().FindOrAdd(Function);
		++Tally.Calls;
		Tally.FromMemo += bFromMemo ? 1 : 0;
		Tally.Total += Seconds * 1000.0;
		Tally.Worst = FMath::Max(Tally.Worst, Seconds * 1000.0);
		if (Seconds > 0.015)
		{
			UE_LOG(LogTheater, Display, TEXT("Speed: the rules took %.1f ms over %s"), Seconds * 1000.0, *Function);
		}
	}

	void NoteFrame(float DeltaSeconds, UWorld* World)
	{
		static const bool bAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterSpeedLog"));
		const bool bWas = bLogging;
		bLogging = bAsked || CVarSpeedLog.GetValueOnGameThread() != 0;
		TArray<FSlot>& Each = Slots();
		if (!bLogging || !bWas)
		{
			// (nothing is counted while the log is off, and a new five seconds begins when it comes on)
			for (FSlot& Slot : Each)
			{
				Slot = FSlot{ Slot.Name };
			}
			RulesTallies().Reset();
			WindowClock = WindowWorst = WindowGame = WindowDraw = WindowChip = 0.0;
			WindowFrames = WindowSlow = 0;
			return;
		}
		// The frame just ended: its time, and each job's share of it.
		for (FSlot& Slot : Each)
		{
			Slot.Total += Slot.Frame;
			Slot.Worst = FMath::Max(Slot.Worst, Slot.Frame);
			Slot.Frame = 0;
		}
		WindowClock += DeltaSeconds;
		++WindowFrames;
		WindowSlow += DeltaSeconds > 0.0334f ? 1 : 0;
		WindowWorst = FMath::Max(WindowWorst, static_cast<double>(DeltaSeconds));
		WindowGame += FPlatformTime::ToMilliseconds(GGameThreadTime);
		WindowDraw += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		WindowChip += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		if (WindowClock < 5.0 || WindowFrames < 2)
		{
			return;
		}

		const double Frames = WindowFrames;
		UE_LOG(LogTheater, Display, TEXT("Speed: %.1f s, %.1f frames a second (worst frame %.1f ms; %d of %d over 33 ms); game %.1f ms, draw %.1f ms, graphics chip %.1f ms"),
			WindowClock, Frames / WindowClock, WindowWorst * 1000.0, WindowSlow, WindowFrames, WindowGame / Frames, WindowDraw / Frames, WindowChip / Frames);
		TArray<const FSlot*> Busy;
		for (const FSlot& Slot : Each)
		{
			if (Slot.Calls > 0)
			{
				Busy.Add(&Slot);
			}
		}
		Busy.Sort([](const FSlot& A, const FSlot& B) { return A.Total > B.Total; });
		FString Line;
		for (const FSlot* Slot : Busy)
		{
			Line += FString::Printf(TEXT("%s%s %.2f (worst %.1f)"), Line.IsEmpty() ? TEXT("") : TEXT(", "), *Slot->Name, Ms(Slot->Total) / Frames, Ms(Slot->Worst));
		}
		if (!Line.IsEmpty())
		{
			UE_LOG(LogTheater, Display, TEXT("Speed: ms a frame (jobs inside others are counted in both): %s"), *Line);
		}
		FString Rules;
		for (const TPair<FString, FRulesTally>& Tally : RulesTallies())
		{
			Rules += FString::Printf(TEXT("%s%s %d (%d from memo) %.1f ms each, worst %.1f"), Rules.IsEmpty() ? TEXT("") : TEXT("; "), *Tally.Key, Tally.Value.Calls, Tally.Value.FromMemo,
				Tally.Value.Total / FMath::Max(Tally.Value.Calls, 1), Tally.Value.Worst);
		}
		if (!Rules.IsEmpty())
		{
			UE_LOG(LogTheater, Display, TEXT("Speed: rules calls: %s"), *Rules);
		}
		if (Summaries++ % 6 == 0 && World)
		{
			Census(World);
		}
		for (FSlot& Slot : Each)
		{
			Slot = FSlot{ Slot.Name };
		}
		RulesTallies().Reset();
		WindowClock = WindowWorst = WindowGame = WindowDraw = WindowChip = 0.0;
		WindowFrames = WindowSlow = 0;
	}
}
