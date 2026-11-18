#include "Campaign/TheaterSpeed.h"

#include "TheaterCommand.h"
#include "CoreGlobals.h" // (IsInGameThread)
#include "Campaign/TheaterStillMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/ScopeRWLock.h" // (FRWLock)
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "RenderTimer.h"
#include "RHI.h"
#include "UObject/UObjectGlobals.h" // (FCoreUObjectDelegates: the engine's clearing out of what is no longer used)
#include "UObject/UObjectIterator.h"
#include <atomic>

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
	TAutoConsoleVariable<int32> CVarOneEye(
		TEXT("tc.Speed.OneEye"), 1,
		TEXT("1 works out the eye's view once for all the names and plates laid over the map; 0 works it out again for every point, as before."));
	TAutoConsoleVariable<int32> CVarStillMeshes(
		TEXT("tc.Speed.StillMeshes"), 1,
		TEXT("1 draws the hand-built models that are made once and left alone (the ground, towns, trees, the lines on the map) through Unreal's kept drawing orders; 0 makes their drawing orders again every frame, as before."));
	TAutoConsoleVariable<int32> CVarHoldersLock(
		TEXT("tc.Speed.HoldersLock"), 1,
		TEXT("1 lets ground change hands without waiting for the world's fine sheets still being modelled (a sheet in the making has the change painted in before it is shown); 0 waits for them, as before."));
	TAutoConsoleVariable<int32> CVarKeptHeights(
		TEXT("tc.Speed.KeptHeights"), 1,
		TEXT("1 keeps the points the frontiers, the front line and the enemy's reach are laid on, until a fine sheet stands up or goes over them; 0 works every point out again each time, as before."));
	TAutoConsoleVariable<int32> CVarDressSteps(
		TEXT("tc.Speed.DressSteps"), 1,
		TEXT("1 dresses a fine sheet a little at a time (about 8 ms a frame, one sheet at a time); 0 dresses every ready sheet whole in one frame, as before."));
	TAutoConsoleVariable<int32> CVarRegionGrid(
		TEXT("tc.Speed.RegionGrid"), 1,
		TEXT("1 finds the province at a place among those whose bounds reach into its square of the map; 0 looks through every province, as before. The same province either way."));
	TAutoConsoleVariable<int32> CVarRiverInks(
		TEXT("tc.Speed.RiverInks"), 1,
		TEXT("1 darkens or lightens the rivers' inks where the haze has moved; 0 draws every river again, as before."));
	TAutoConsoleVariable<int32> CVarFrontWeights(
		TEXT("tc.Speed.FrontWeights"), 1,
		TEXT("1 draws the weight of the front line the eye is at when the front changes, and the other weight a frame after; 0 draws both at once, as before."));
	TAutoConsoleVariable<int32> CVarBordersKept(
		TEXT("tc.Speed.BordersKept"), 1,
		TEXT("1 keeps the frontier lines' points from one drawing to the next and hands only the kinds of line that changed to the graphics chip again; 0 lays and hands over every line each time, as before. The same lines either way."));
	TAutoConsoleVariable<int32> CVarPictureOnce(
		TEXT("tc.Speed.PictureOnce"), 1,
		TEXT("1 makes a picture read without smoothing that way from the first; 0 makes it smoothed and then again unsmoothed, waiting for the drawing thread, as before. The same picture either way."));
	TAutoConsoleVariable<int32> CVarFogAside(
		TEXT("tc.Speed.FogAside"), 1,
		TEXT("1 lays the haze beyond the map on another thread when it moves and puts it on the table a picture a frame after; 0 lays it all in the frame of the press, as before."));
	TAutoConsoleVariable<int32> CVarFineAside(
		TEXT("tc.Speed.FineAside"), 1,
		TEXT("1 makes the smaller copies of a fine sheet's pictures on the thread that models it; 0 in the frame that dresses it, as before."));

	TAutoConsoleVariable<int32> CVarSheetShelf(
		TEXT("tc.Speed.SheetShelf"), 6,
		TEXT("How many fine sheets of the world the eye has left are kept on a shelf (their modelling kept, their tiles taken down), to be stood up again without being modelled again; 0 drops them whole, as before. The same picture either way."));
	TAutoConsoleVariable<int32> CVarTileTime(
		TEXT("tc.Speed.TileTime"), 4,
		TEXT("The ms of tiles of ground stood up in one frame, over every sheet together, after which the rest wait for the next frame (one is always stood up); 0 stands up five a sheet whatever they take, as before."));
	TAutoConsoleVariable<int32> CVarQuietModel(
		TEXT("tc.Speed.QuietModel"), 0,
		TEXT("(for measuring first) 1 models each fine sheet on its one thread and leaves the other cores to the frame: the fine ground comes later; 2 spreads its loops as before but at the background's priority; 0 spreads them over every spare core, as before. The same ground whichever."));

	FRWLock HoldersGuard; // (tc.Speed.HoldersLock: see TheaterSpeed::FHoldersRead)

	thread_local int32 QuietHere = 0; // (tc.Speed.QuietModel: this thread is modelling a fine sheet quietly, and how)

	// (What was made, counted on whichever thread made it; read and cleared on the game thread.)
	std::atomic<int32> MadeNow[static_cast<int32>(TheaterSpeed::EMade::Count)];
	int32 MadeFrame[static_cast<int32>(TheaterSpeed::EMade::Count)] = {}; // (the frame just ended)
	int64 MadeWindow[static_cast<int32>(TheaterSpeed::EMade::Count)] = {}; // (the summary's five seconds)
	int32 SheetsModelling = 0, SheetsStanding = 0, SheetsShelved = 0; // (as the map last said)
	int32 WindowModelling = 0; // (frames of the five seconds in which a sheet was being modelled)
	int32 WindowSlowModelling = 0; // (...and how many of the frames over 33 ms those were, not beside the step player's look)
	uint64 GarbageBegan = 0; // (when the engine began clearing out what is no longer used, while logging)

	/** The line of what was made: "5 tiles of ground stood up, 7 still models handed over (29575 points), ...". */
	template <typename TCount>
	FString MadeLine(const TCount* Made)
	{
		static const TCHAR* const One[] = {
			TEXT("tile of ground stood up"), TEXT("still model handed to the draw thread"), TEXT(""), TEXT("piece's shadow switched"),
			TEXT("fine sheet's modelling ended"), TEXT("fine sheet stood down to the shelf"), TEXT("fine sheet stood up from the shelf"), TEXT("fine sheet dropped whole") };
		static const TCHAR* const Many[] = {
			TEXT("tiles of ground stood up"), TEXT("still models handed to the draw thread"), TEXT(""), TEXT("pieces' shadows switched"),
			TEXT("fine sheets' modelling ended"), TEXT("fine sheets stood down to the shelf"), TEXT("fine sheets stood up from the shelf"), TEXT("fine sheets dropped whole") };
		FString Line;
		for (int32 Index = 0; Index < static_cast<int32>(TheaterSpeed::EMade::Count); ++Index)
		{
			if (Index == static_cast<int32>(TheaterSpeed::EMade::StillPoints) || Made[Index] <= 0)
			{
				continue;
			}
			Line += FString::Printf(TEXT("%s%lld %s"), Line.IsEmpty() ? TEXT("") : TEXT(", "), static_cast<long long>(Made[Index]), Made[Index] == 1 ? One[Index] : Many[Index]);
			if (Index == static_cast<int32>(TheaterSpeed::EMade::StillModels))
			{
				Line += FString::Printf(TEXT(" (%lld points)"), static_cast<long long>(Made[static_cast<int32>(TheaterSpeed::EMade::StillPoints)]));
			}
		}
		return Line;
	}

	bool bLogging = false;
	uint64 SlateBegan = 0; // (when Slate's tick began this frame, while logging)

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
	int32 WindowSlowBeside = 0; // (of those, the ones in or just after a frame the step player looked at the game)
	bool bLookedBefore = false; // (the step player worked in the frame before this one)
	double WindowWorst = 0.0;
	double WindowGame = 0.0, WindowDraw = 0.0, WindowChip = 0.0;
	double WindowRHI = 0.0; // (the RHI thread's, where there is one: none, and its work is the draw thread's)
	int32 Summaries = 0;

	double Ms(uint64 Cycles)
	{
		return FPlatformTime::ToMilliseconds64(Cycles);
	}

	/**
	 * What is drawn: every part of the world that is registered and shown, how many throw shadows, and the largest
	 * meshes. Then what was on screen in that moment (drawn in the last quarter of a second), kind by kind: a kind is
	 * the owner's class and the part's name without its number ("CampaignMapActor.LandTile", "ArmyPiece.Cloth"). Each
	 * section of a hand-built model is one drawing order per pass; a still one's are kept from frame to frame, the others'
	 * are made again every frame.
	 */
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
		struct FKind
		{
			int32 Parts = 0;
			int32 Sections = 0;
			int32 Shadowed = 0;
			int64 Triangles = 0;
			bool bStill = false;
			bool bMesh = false;
		};
		TArray<FEntry> Meshes;
		TMap<FString, FKind> Kinds;
		int32 Parts = 0, Shown = 0, Shadowed = 0, Instanced = 0, Instances = 0;
		int64 Points = 0, Triangles = 0, ShadowTriangles = 0;
		int32 OnScreen = 0, OnScreenShadowed = 0, OnScreenMeshes = 0, OnScreenStill = 0, OnScreenSections = 0, StillSections = 0, DrawnOffScreen = 0;
		const double Now = World->GetTimeSeconds();
		const bool bStillOn = TheaterSpeed::StillMeshes();
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
			const bool bOnScreen = Now - Part->GetLastRenderTimeOnScreen() <= 0.25;
			DrawnOffScreen += !bOnScreen && Now - Part->GetLastRenderTime() <= 0.25 ? 1 : 0;
			FKind* Kind = nullptr;
			if (bOnScreen)
			{
				++OnScreen;
				OnScreenShadowed += bShadow ? 1 : 0;
				const FString Key = FString::Printf(TEXT("%s.%s"), Part->GetOwner() ? *Part->GetOwner()->GetClass()->GetName() : TEXT("-"), *Part->GetFName().GetPlainNameString());
				Kind = &Kinds.FindOrAdd(Key);
				++Kind->Parts;
				Kind->Shadowed += bShadow ? 1 : 0;
			}
			if (UProceduralMeshComponent* Mesh = Cast<UProceduralMeshComponent>(Part))
			{
				FEntry Entry;
				Entry.Name = Mesh->GetOwner() ? FString::Printf(TEXT("%s.%s"), *Mesh->GetOwner()->GetName(), *Mesh->GetName()) : Mesh->GetName();
				Entry.bShadow = bShadow;
				for (int32 Index = 0; Index < Mesh->GetNumSections(); ++Index)
				{
					const FProcMeshSection* Section = Mesh->GetProcMeshSection(Index);
					if (Section && Section->bSectionVisible && Section->ProcIndexBuffer.Num() > 0)
					{
						Entry.Points += Section->ProcVertexBuffer.Num();
						Entry.Triangles += Section->ProcIndexBuffer.Num() / 3;
						++Entry.Sections;
					}
				}
				Points += Entry.Points;
				Triangles += Entry.Triangles;
				ShadowTriangles += bShadow ? Entry.Triangles : 0;
				if (Kind)
				{
					const bool bStill = bStillOn && Mesh->IsA<UTheaterStillMesh>();
					Kind->bMesh = true;
					Kind->bStill = bStill;
					Kind->Sections += Entry.Sections;
					Kind->Triangles += Entry.Triangles;
					++OnScreenMeshes;
					OnScreenStill += bStill ? 1 : 0;
					OnScreenSections += Entry.Sections;
					StillSections += bStill ? Entry.Sections : 0;
				}
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
		UE_LOG(LogTheater, Display, TEXT("Speed census, on screen: %d parts (%d throwing shadows); %d hand-built with %d sections, %d of them still with %d sections; %d more drawn only off screen (into a shadow)"),
			OnScreen, OnScreenShadowed, OnScreenMeshes, OnScreenSections, OnScreenStill, StillSections, DrawnOffScreen);
		TArray<TPair<FString, FKind>> ByKind;
		for (const TPair<FString, FKind>& Each : Kinds)
		{
			ByKind.Emplace(Each.Key, Each.Value);
		}
		ByKind.Sort([](const TPair<FString, FKind>& A, const TPair<FString, FKind>& B)
		{
			return A.Value.Sections != B.Value.Sections ? A.Value.Sections > B.Value.Sections : A.Value.Parts > B.Value.Parts;
		});
		FString Line;
		for (int32 Index = 0; Index < FMath::Min(ByKind.Num(), 24); ++Index)
		{
			const FKind& Each = ByKind[Index].Value;
			Line += FString::Printf(TEXT("%s%s %d"), Line.IsEmpty() ? TEXT("") : TEXT("; "), *ByKind[Index].Key, Each.Parts);
			if (Each.bMesh)
			{
				Line += FString::Printf(TEXT(" (%d sections, %lldk triangles%s)"), Each.Sections, (Each.Triangles + 500) / 1000, Each.bStill ? TEXT(", still") : TEXT(""));
			}
			Line += Each.Shadowed > 0 ? FString::Printf(TEXT(" [%d throw shadows]"), Each.Shadowed) : FString();
		}
		UE_LOG(LogTheater, Display, TEXT("Speed census, on screen by kind: %s"), *Line);
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

	bool OneEye()
	{
		return CVarOneEye.GetValueOnGameThread() != 0;
	}

	bool FineAside()
	{
		return CVarFineAside.GetValueOnGameThread() != 0;
	}

	bool StillMeshes()
	{
		// (read where a mesh's proxy is made, which may be off the game thread)
		return CVarStillMeshes.GetValueOnAnyThread() != 0;
	}

	bool HoldersLock()
	{
		return CVarHoldersLock.GetValueOnGameThread() != 0;
	}

	bool KeptHeights()
	{
		return CVarKeptHeights.GetValueOnGameThread() != 0;
	}

	bool DressSteps()
	{
		return CVarDressSteps.GetValueOnGameThread() != 0;
	}

	bool FogAside()
	{
		return CVarFogAside.GetValueOnGameThread() != 0;
	}

	bool RegionGrid()
	{
		return CVarRegionGrid.GetValueOnAnyThread() != 0; // (asked on the sheets' threads too)
	}

	bool RiverInks()
	{
		return CVarRiverInks.GetValueOnGameThread() != 0;
	}

	bool FrontWeights()
	{
		return CVarFrontWeights.GetValueOnGameThread() != 0;
	}

	bool PictureOnce()
	{
		return CVarPictureOnce.GetValueOnGameThread() != 0;
	}

	bool BordersKept()
	{
		return CVarBordersKept.GetValueOnGameThread() != 0;
	}

	int32 SheetShelf()
	{
		return FMath::Clamp(CVarSheetShelf.GetValueOnGameThread(), 0, 24);
	}

	double TileTime()
	{
		return FMath::Clamp(CVarTileTime.GetValueOnGameThread(), 0, 1000);
	}

	int32 QuietModel()
	{
		return FMath::Clamp(CVarQuietModel.GetValueOnGameThread(), 0, 2);
	}

	FQuietModelling::FQuietModelling(int32 How)
		: Was(QuietHere)
	{
		QuietHere = How;
	}

	FQuietModelling::~FQuietModelling()
	{
		QuietHere = Was;
	}

	EParallelForFlags ModelLoop()
	{
		return QuietHere == 1 ? EParallelForFlags::ForceSingleThread : QuietHere == 2 ? EParallelForFlags::BackgroundPriority : EParallelForFlags::None;
	}

	void NoteMade(EMade What, int32 HowMany)
	{
		if (bLogging && What < EMade::Count)
		{
			MadeNow[static_cast<int32>(What)].fetch_add(HowMany, std::memory_order_relaxed);
		}
	}

	void NoteSheets(int32 Modelling, int32 Standing, int32 Shelved)
	{
		SheetsModelling = Modelling;
		SheetsStanding = Standing;
		SheetsShelved = Shelved;
	}

	FHoldersRead::FHoldersRead()
		: bHeld(!IsInGameThread())
	{
		if (bHeld)
		{
			HoldersGuard.ReadLock();
		}
	}

	void FHoldersRead::Release()
	{
		if (bHeld)
		{
			HoldersGuard.ReadUnlock();
			bHeld = false;
		}
	}

	FHoldersChange::FHoldersChange(bool bNeeded)
		: bHeld(bNeeded)
	{
		if (bHeld)
		{
			HoldersGuard.WriteLock();
		}
	}

	void FHoldersChange::Release()
	{
		if (bHeld)
		{
			HoldersGuard.WriteUnlock();
			bHeld = false;
		}
	}

	FEye::FEye(const APlayerController* InPlayer)
		: Player(InPlayer)
	{
		// (As UGameplayStatics::ProjectWorldToScreen does it, once.)
		if (!Player || !OneEye())
		{
			return;
		}
		const ULocalPlayer* const Local = Player->GetLocalPlayer();
		if (!Local || !Local->ViewportClient || !Local->ViewportClient->Viewport)
		{
			return;
		}
		FSceneViewProjectionData Projection;
		if (Local->GetProjectionData(Local->ViewportClient->Viewport, Projection))
		{
			ViewProjection = Projection.ComputeViewProjectionMatrix();
			Rect = Projection.GetConstrainedViewRect();
			bOwn = true;
		}
	}

	bool FEye::Project(const FVector& World, FVector2D& OutScreen) const
	{
		if (!bOwn)
		{
			return Player && Player->ProjectWorldLocationToScreen(World, OutScreen, false);
		}
		// (The controller's own way then lets PostProcessWorldToScreen have its say; the campaign's controller leaves
		// it as the engine has it, which says yes and changes nothing.)
		return FSceneView::ProjectWorldToScreen(World, Rect, ViewProjection, OutScreen);
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

	void ApplyCommandLineSets()
	{
		// ("-TheaterSet=tc.Speed.SheetShelf=0,tc.Speed.QuietModel=1": console variables set from the command line, once,
		// for a run of the step player with one of the switches the other way. -TheaterTime tries settings against
		// each other with the eye held in one place, which never takes a fine sheet down or brings new ground up.
		// Done as the table begins to be laid (ACampaignMapActor::BuildMap), so that a switch read only in the laying
		// is set in time: the pre-checker's note on 0108ac6, where it was done at the first frame, after the laying.)
		static bool bSetAsked = false;
		if (!bSetAsked)
		{
			bSetAsked = true;
			FString Asked;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSet="), Asked, false))
			{
				TArray<FString> Each;
				Asked.ParseIntoArray(Each, TEXT(","), true);
				for (const FString& One : Each)
				{
					FString Name, Value;
					if (!One.Split(TEXT("="), &Name, &Value))
					{
						continue;
					}
					Name.TrimStartAndEndInline();
					Value.TrimStartAndEndInline();
					if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name))
					{
						Variable->Set(*Value, ECVF_SetByConsole);
						UE_LOG(LogTheater, Display, TEXT("Speed: %s set to %s (-TheaterSet)"), *Name, *Value);
					}
					else
					{
						UE_LOG(LogTheater, Warning, TEXT("Speed: -TheaterSet names no console variable called %s"), *Name);
					}
				}
			}
		}
	}

	void NoteFrame(float DeltaSeconds, UWorld* World)
	{
		ApplyCommandLineSets(); // (in case no table was laid first: it is done once)
		// (a change of tc.Speed.StillMeshes is seen at once: every still mesh's proxy is made again, of the kind now asked for)
		{
			static int32 StillWas = -1;
			const int32 StillNow = CVarStillMeshes.GetValueOnGameThread();
			if (StillWas != -1 && StillNow != StillWas)
			{
				UTheaterStillMesh::RemakeAll();
			}
			StillWas = StillNow;
		}
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
			WindowClock = WindowWorst = WindowGame = WindowDraw = WindowChip = WindowRHI = 0.0;
			WindowFrames = WindowSlow = WindowSlowBeside = 0;
			for (int32 Index = 0; Index < static_cast<int32>(EMade::Count); ++Index)
			{
				MadeNow[Index].store(0, std::memory_order_relaxed);
				MadeFrame[Index] = 0;
				MadeWindow[Index] = 0;
			}
			WindowModelling = WindowSlowModelling = 0;
			return;
		}
		// (The second speed detective's, 18 November 2026. The engine clears out what is no longer used about once a
		// minute, in one go, and with the editor's program under the game that is every object the editor has: a frame
		// of 45 ms with no job of ours in it, twice in the builder's sweep of build 1634, 61 s apart. Timed as a job.)
		static bool bGarbageHooked = false;
		if (!bGarbageHooked)
		{
			bGarbageHooked = true;
			const int32 GarbageSlot = SlotFor(TEXT("EngineClearOut"));
			FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddLambda([]()
			{
				GarbageBegan = bLogging && IsInGameThread() ? FPlatformTime::Cycles64() : 0;
			});
			FCoreUObjectDelegates::GetPostGarbageCollect().AddLambda([GarbageSlot]()
			{
				if (GarbageBegan != 0 && IsInGameThread())
				{
					AddTime(GarbageSlot, FPlatformTime::Cycles64() - GarbageBegan);
				}
				GarbageBegan = 0;
			});
		}
		// What was made in the frame just ended.
		bool bAnyMade = false;
		for (int32 Index = 0; Index < static_cast<int32>(EMade::Count); ++Index)
		{
			MadeFrame[Index] = MadeNow[Index].exchange(0, std::memory_order_relaxed);
			MadeWindow[Index] += MadeFrame[Index];
			bAnyMade |= MadeFrame[Index] > 0;
		}
		// (Slate's tick, its laying out and painting of every panel, is timed from its own start and end.)
		static bool bSlateHooked = false;
		if (!bSlateHooked && FSlateApplication::IsInitialized())
		{
			bSlateHooked = true;
			const int32 SlateSlot = SlotFor(TEXT("Slate"));
			FSlateApplication::Get().OnPreTick().AddLambda([](float)
			{
				SlateBegan = bLogging ? FPlatformTime::Cycles64() : 0;
			});
			FSlateApplication::Get().OnPostTick().AddLambda([SlateSlot](float)
			{
				if (SlateBegan != 0)
				{
					AddTime(SlateSlot, FPlatformTime::Cycles64() - SlateBegan);
					SlateBegan = 0;
				}
			});
		}
		// A long frame is named at once: every job that took a share of it, the longest first. (The jobs are counted
		// from one start of the player's tick to the next, which is the frame just ended give or take its edges: a
		// press's work can fall in the line before or the line after.)
		// (Speed, 17 November, the design director's b414: every frame over 33 ms is named, not only those over 100 ms
		// or with a job over 40, and a frame in which the step player looked at the game, or the one after it, where its
		// picture is taken, is marked: its time is the harness's, which a player does not pay.)
		static const int32 LookSlot = SlotFor(TEXT("HarnessLook"));
		const bool bLookedNow = Each.IsValidIndex(LookSlot) && Each[LookSlot].Frame > 0;
		const bool bBeside = bLookedNow || bLookedBefore;
		bLookedBefore = bLookedNow;
		{
			uint64 Longest = 0;
			for (const FSlot& Slot : Each)
			{
				Longest = FMath::Max(Longest, Slot.Frame);
			}
			if (DeltaSeconds > 0.0334f || Ms(Longest) > 20.0)
			{
				TArray<const FSlot*> Heavy;
				for (const FSlot& Slot : Each)
				{
					if (Ms(Slot.Frame) >= 0.5)
					{
						Heavy.Add(&Slot);
					}
				}
				Heavy.Sort([](const FSlot& A, const FSlot& B) { return A.Frame > B.Frame; });
				FString Jobs;
				for (const FSlot* Slot : Heavy)
				{
					Jobs += FString::Printf(TEXT("%s%s %.1f"), Jobs.IsEmpty() ? TEXT("") : TEXT(", "), *Slot->Name, Ms(Slot->Frame));
				}
				UE_LOG(LogTheater, Display, TEXT("Speed: a long frame, %.0f ms (game %.1f, draw %.1f, graphics chip %.1f ms)%s; its jobs in ms (inner jobs counted in their outer ones too): %s"),
					DeltaSeconds * 1000.0, FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),
					bBeside ? TEXT(" [beside the step player's look]") : TEXT(""), Jobs.IsEmpty() ? TEXT("none of the timed jobs") : *Jobs);
				// (A line of its own, so that the line above reads as it did: what was made in that frame, which the
				// draw thread then has to take in, and whether new ground was being modelled on the other cores.)
				if (DeltaSeconds > 0.0334f && (bAnyMade || SheetsModelling > 0))
				{
					const FString Made = MadeLine(MadeFrame);
					UE_LOG(LogTheater, Display, TEXT("Speed: made in that frame: %s%s%d fine sheet%s being modelled on other threads"),
						*Made, Made.IsEmpty() ? TEXT("") : TEXT("; "), SheetsModelling, SheetsModelling == 1 ? TEXT("") : TEXT("s"));
				}
			}
		}
		WindowModelling += SheetsModelling > 0 ? 1 : 0;
		WindowSlowModelling += SheetsModelling > 0 && DeltaSeconds > 0.0334f && !bBeside ? 1 : 0;
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
		WindowSlowBeside += DeltaSeconds > 0.0334f && bBeside ? 1 : 0;
		WindowWorst = FMath::Max(WindowWorst, static_cast<double>(DeltaSeconds));
		WindowGame += FPlatformTime::ToMilliseconds(GGameThreadTime);
		WindowDraw += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		WindowRHI += FPlatformTime::ToMilliseconds(GRHIThreadTime);
		WindowChip += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		if (WindowClock < 5.0 || WindowFrames < 2)
		{
			return;
		}

		const double Frames = WindowFrames;
		UE_LOG(LogTheater, Display, TEXT("Speed: %.1f s, %.1f frames a second (worst frame %.1f ms; %d of %d over 33 ms, %d of those beside the step player's look); game %.1f ms, draw %.1f ms, graphics chip %.1f ms"),
			WindowClock, Frames / WindowClock, WindowWorst * 1000.0, WindowSlow, WindowFrames, WindowSlowBeside, WindowGame / Frames, WindowDraw / Frames, WindowChip / Frames);
		// (Speed, 18 November 2026: a line of its own, so that the line above reads as it did. Is the draw thread's time
		// its own, or the RHI's too? With no RHI thread, the RHI's work is done on the draw thread and counted there.)
		UE_LOG(LogTheater, Display, TEXT("Speed: RHI thread %.1f ms a frame%s"), WindowRHI / Frames,
			GRHIThreadTime == 0 && WindowRHI <= 0.0 ? TEXT(" (none: its work is in the draw thread's time)") : TEXT(""));
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
		{
			// (What was made in those five seconds; how the fine sheets stand; and how the long frames fell: in the frames
			// while new ground was being modelled, or in the others. A line of its own, as the RHI thread's is.)
			const FString Made = MadeLine(MadeWindow);
			UE_LOG(LogTheater, Display, TEXT("Speed: made: %s; fine sheets now: %d standing, %d on the shelf, %d being modelled; new ground was being modelled in %d of the %d frames, and %d of the %d over 33 ms not beside the step player's look fell in those; the game holds %.0f MB of memory"),
				Made.IsEmpty() ? TEXT("nothing") : *Made, SheetsStanding, SheetsShelved, SheetsModelling, WindowModelling, WindowFrames, WindowSlowModelling, WindowSlow - WindowSlowBeside,
				static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0));
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
		WindowClock = WindowWorst = WindowGame = WindowDraw = WindowChip = WindowRHI = 0.0;
		WindowFrames = WindowSlow = WindowSlowBeside = 0;
		for (int32 Index = 0; Index < static_cast<int32>(EMade::Count); ++Index)
		{
			MadeWindow[Index] = 0;
		}
		WindowModelling = WindowSlowModelling = 0;
	}
}
