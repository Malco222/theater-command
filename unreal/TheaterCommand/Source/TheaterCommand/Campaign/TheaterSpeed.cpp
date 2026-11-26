#include "Campaign/TheaterSpeed.h"

#include "TheaterCommand.h"
#include "Async/Async.h" // (AsyncThread: a fine sheet modelled on a thread of its own, tc.Speed.QuietModel)
#include "CoreGlobals.h" // (IsInGameThread)
#include "Campaign/TheaterStillMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h" // (GEngine->ForceGarbageCollection: tc.Speed.TidyAside)
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h" // (FRWLock)
#include "Misc/App.h" // (FApp::IsUnattended: a test's run keeps the engine's clear-out every minute, tc.Speed.TidyAside)
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "RenderTimer.h"
#include "RHI.h"
#include "UObject/UObjectGlobals.h" // (FCoreUObjectDelegates: the engine's clearing out of what is no longer used)
#include "UObject/UObjectIterator.h"
#include <atomic>
#if PLATFORM_MAC
#include <sys/resource.h> // (setpriority, PRIO_DARWIN_THREAD, PRIO_DARWIN_BG: tc.Speed.QuietModel 4; getrusage: the machine's line)
#include <mach/mach.h> // (host_processor_info: how busy the chip's cores are, every program's work together)
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/vm_map.h>
#include <notify.h> // (notify_register_check, notify_get_state: the system's own word on how hot it is)
#endif

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
		TEXT("tc.Speed.SheetShelf"), 12,
		TEXT("How many fine sheets of the world the eye has left are kept on a shelf (their modelling kept, their tiles taken down), to be stood up again without being modelled again; 0 drops them whole, as before. The same picture either way."));
	TAutoConsoleVariable<int32> CVarTileTime(
		TEXT("tc.Speed.TileTime"), 4,
		TEXT("The ms of tiles of ground stood up in one frame, over every sheet together, after which the rest wait for the next frame (one is always stood up); 0 stands up five a sheet whatever they take, as before."));
	TAutoConsoleVariable<int32> CVarQuietModel(
		TEXT("tc.Speed.QuietModel"), 0,
		TEXT("(for measuring first) How a fine sheet is modelled. 0: on the engine's pool with its loops spread over every spare core, as before. 1: its loops on its one pool thread. 2: its loops spread at the background's priority. 3: on a thread of its own at the lowest priority, its loops on that thread. 4: as 3, and on a Mac that thread is marked as background work, which the system keeps to the efficiency cores. The same ground whichever; it comes later the quieter it is made."));
	TAutoConsoleVariable<int32> CVarFontRoom(
		TEXT("tc.Speed.FontRoom"), 1,
		TEXT("1 gives Unreal eight pages for the letters it has cut, where it had one and threw every letter away when a second was wanted; 0 leaves it at one, as before. Read as the table is laid. The same letters either way."));
	TAutoConsoleVariable<int32> CVarStandOne(
		TEXT("tc.Speed.StandOne"), 1,
		TEXT("1 stands one ready fine sheet a frame, the next the frame after; 0 stands every ready one in the same frame, as before. The same picture either way."));
	TAutoConsoleVariable<int32> CVarShowSpread(
		TEXT("tc.Speed.ShowSpread"), 80000,
		TEXT("How many points of the towns' and the trees' squares are newly shown in one frame when the eye crosses a height at which they change (the nearest squares first, one always), the rest the frames after; 0 shows every square in the same frame, as before. The same picture either way, a few frames later."));
	TAutoConsoleVariable<int32> CVarMonthSpread(
		TEXT("tc.Speed.MonthSpread"), 1,
		TEXT("1 spreads the month's turn: the season's masks sent only when the winter changes, snow's paint only in the box it moved in, and the rest of the table and the save a frame and two after; 0 does it all in the month's frame, as before. The same picture either way."));
	TAutoConsoleVariable<int32> CVarSidesKept(
		TEXT("tc.Speed.SidesKept"), 1,
		TEXT("1 lays the sides view once on a mesh of its own, keeps it while it is put away and recolours it where the sides changed; 0 lays it afresh on every press, as before. The same picture either way."));
	TAutoConsoleVariable<float> CVarSidesFade(
		TEXT("tc.Speed.SidesFade"), 0.18f,
		TEXT("(with tc.Speed.SidesKept) The seconds the sides view takes to come up and go away, its wash fading in and out, so that the step is never a jump (the art director: covered by a 0.15 to 0.2 s cross-fade); 0 shows and hides it at once, as before. A long frame counts as a thirtieth of a second, so a stall never eats the fade."));
	TAutoConsoleVariable<float> CVarTreesSpread(
		TEXT("tc.Speed.TreesSpread"), 6.0f,
		TEXT("The ms a frame the woods may take to turn for the month (autumn, winter, spring): the squares in sight first, then the nearest, a few a frame; 0 plants every square again in the month's frame, as before (SeasonTrees, about 155 ms in run 8). The same woods either way, the far ones a moment later."));
	TAutoConsoleVariable<int32> CVarCardAside(
		TEXT("tc.Speed.CardAside"), 1,
		TEXT("1 stands the attack card the moment the army is ordered at enemy ground and fills its forecast a frame or a few later, worked out on the rules' own thread (it took 150 ms of the press's frame); 0 works the forecast out in the press's frame, as before. The same card either way."));
	TAutoConsoleVariable<int32> CVarWarSpread(
		TEXT("tc.Speed.WarSpread"), 1,
		TEXT("1 begins a war from the opening screen under a plate: the rules start it a few frames after the press, and the table is laid a step a frame before the war is entered (map run 8: 1.2 to 1.4 s of laying in a 2.2 to 2.5 s frame); 0 does it all in the press's frame, as before. The same table either way."));
	TAutoConsoleVariable<int32> CVarSaveAside(
		TEXT("tc.Speed.SaveAside"), 1,
		TEXT("1 writes the month's save out on the rules' own thread and onto the disk on a worker thread, a frame or a few after the month (some 2.3 MB of text by August 1944); 0 writes it in the month's frames, as before. The same file either way."));
	TAutoConsoleVariable<int32> CVarNationsAside(
		TEXT("tc.Speed.NationsAside"), 1,
		TEXT("1 works the opening screen's list of countries out on the rules' own thread while the screen says so (a later year is history played up to it: 0.7 to 1 s for 1941 in a cloud run); 0 works it out in the press's frame, as before. The same list either way."));
	TAutoConsoleVariable<int32> CVarMonthAside(
		TEXT("tc.Speed.MonthAside"), 0,
		TEXT("1 plays the other nations' months and reads their answer on the rules' own thread when he ends his, so the table's frames go on meanwhile (1.0 to 1.7 s on the whole-world map in a cloud run, 0.35 to 0.8 s on Europe and the Americas), and the watch or the month's card follows once they are in; 0 plays them in the press's frame, as before. A run that takes pictures plays them at once. The same month either way."));
	TAutoConsoleVariable<int32> CVarPicturesAside(
		TEXT("tc.Speed.PicturesAside"), 1,
		TEXT("1 stands a card at once and takes its unit pictures not yet taken one a frame after it, each laid in its place (the arsenal or a province's raise list opened for the first time took them all in the press's frame); 0 takes them all in the press's frame, as before. A run that takes pictures takes them at once. The same pictures either way, a few frames later."));
	TAutoConsoleVariable<int32> CVarTidyAside(
		TEXT("tc.Speed.TidyAside"), 1,
		TEXT("1 has Unreal clear out what is no longer used every five minutes, where it did every 61 s in one frame of 40 to 63 ms, and at once behind any pause of a quarter of a second that the game already has (a load, a month's end); 0 leaves it every 61 s, as before. A test's run (-unattended) keeps its 61 s and has the second half only. Read as the table is laid. The same picture either way."));

	TAutoConsoleVariable<int32> CVarFrameCap(
		TEXT("tc.Speed.FrameCap"), 0,
		TEXT("0 leaves the frames free, as before (a cool Mac draws 80 to 97 pictures a second for a screen that shows 60, and the chip's work for the rest is only heat). 20 to 240 holds the game to that many frames a second (t.MaxFPS, set by code: a value set by hand or by -TheaterSet stays); -1 has each frame wait for the screen's refresh (r.VSync 1). Seen at once; set back to 0, both are put back as they stood. The same picture either way. A capped game sleeps between frames, and macOS stretches the sleeps when the screen is dark or the window hidden: time it on a lit screen, the window in front."));

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
	double TidiedAt = 0.0; // (tc.Speed.TidyAside: when the engine last cleared out, or was last asked to, by the machine's clock)
	double FrameSeenAt = 0.0; // (...and when the last frame was counted, by the same clock: a pause is told by the clock, not by the game's time)
	double TidyWeighedAt = 0.0; // (...when the game's memory was last read for it, once a second)
	double TidyLowMB = 0.0; // (...and the least the game has held since the engine last cleared out)
	bool bTidyOwed = false; // (...and the game holds half a gigabyte over that least: the next pause is taken, whenever the last clear-out was)

	// (The machine's own state, for the log's line on it: see ReadMachine.)
	struct FMachineReading
	{
		double Clock = 0.0; // (the machine's clock at the reading)
		double GameSeconds = 0.0; // (the seconds of a core's time this program has had since it began, all its threads together)
		uint64 BusyTicks = 0; // (every core's ticks at work since the machine started, every program's)
		uint64 AllTicks = 0; // (...and at work or idle)
		int32 Cores = 0;
		bool bValid = false;
	};
	FMachineReading MachineWas; // (as it stood at the last five seconds' end)
	FCriticalSection KindsGuard;
	TMap<FName, FIntPoint> KindsNow; // (still models handed over since the last frame's count, by kind: how many, and their points)

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
	double WindowIdle = 0.0, WindowOverslept = 0.0; // (ms the engine waited to keep to a cap, and by how much its waits overran what it asked)
	int32 WindowInFront = 0; // (frames in which the game's window was the one in front)
	int32 CapInForce = 0; // (tc.Speed.FrameCap: what this file last put in force: 0 nothing, N frames a second, -1 the screen's refresh)
	bool bCapSetMax = false, bCapSetSync = false; // (...whether it set t.MaxFPS or r.VSync itself, and so has them to put back)
	float CapMaxWas = 0.0f; // (...t.MaxFPS as it stood before)
	int32 CapSyncWas = 0; // (...and r.VSync)
	int32 Summaries = 0;
	// (the last five seconds the log closed, its frames a second and worst frame, for a reader that prints them beside
	// its own counts: TheaterSpeed::LastWindow, the air battle's timing eye)
	double LastWindowClock = 0.0, LastWindowRate = 0.0, LastWindowWorstMs = 0.0;

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
		return FMath::Clamp(CVarQuietModel.GetValueOnGameThread(), 0, 4);
	}

	bool StandOne()
	{
		return CVarStandOne.GetValueOnGameThread() != 0;
	}

	int32 ShowSpread()
	{
		return FMath::Clamp(CVarShowSpread.GetValueOnGameThread(), 0, 100000000);
	}

	bool MonthSpread()
	{
		return CVarMonthSpread.GetValueOnGameThread() != 0;
	}

	bool SidesKept()
	{
		return CVarSidesKept.GetValueOnGameThread() != 0;
	}

	bool WarSpread()
	{
		return CVarWarSpread.GetValueOnGameThread() != 0;
	}

	bool CardAside()
	{
		return CVarCardAside.GetValueOnGameThread() != 0;
	}

	bool SaveAside()
	{
		return CVarSaveAside.GetValueOnGameThread() != 0;
	}

	bool NationsAside()
	{
		return CVarNationsAside.GetValueOnGameThread() != 0;
	}

	bool MonthAside()
	{
		return CVarMonthAside.GetValueOnGameThread() != 0;
	}

	bool PicturesAside()
	{
		return CVarPicturesAside.GetValueOnGameThread() != 0;
	}

	bool PictureRun()
	{
		const TCHAR* Line = FCommandLine::Get();
		return FCString::Strifind(Line, TEXT("TheaterShots=")) != nullptr && FCString::Strifind(Line, TEXT("TheaterSteps=")) == nullptr;
	}

	double TreesSpread()
	{
		return FMath::Clamp(static_cast<double>(CVarTreesSpread.GetValueOnGameThread()), 0.0, 1000.0);
	}

	float SidesFade()
	{
		return FMath::Clamp(CVarSidesFade.GetValueOnGameThread(), 0.0f, 2.0f);
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
		// (1, 3 and 4 keep the loop to the thread it is on; 2 spreads it at the background's priority)
		return QuietHere == 2 ? EParallelForFlags::BackgroundPriority : QuietHere != 0 ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None;
	}

	TFuture<void> ModelAside(int32 How, TUniqueFunction<void()> Work)
	{
		if (How >= 3)
		{
			// A thread of its own for this one sheet, at the lowest priority the engine has, so that the frame's own
			// threads are before it for the fast cores. (Its stack is given room: the engine's pool threads, where the
			// modelling has always run, are not small.) With 4, on a Mac, the thread also tells the system that it is
			// background work, which Apple's chips keep to their efficiency cores. Its loops run on it alone (ModelLoop).
			return AsyncThread([How, Mine = MoveTemp(Work)]()
			{
#if PLATFORM_MAC
				if (How == 4)
				{
					setpriority(PRIO_DARWIN_THREAD, 0, PRIO_DARWIN_BG);
				}
#endif
				Mine();
			}, 8 * 1024 * 1024, TPri_Lowest);
		}
		return Async(EAsyncExecution::ThreadPool, MoveTemp(Work)); // (as before)
	}

	void NoteMade(EMade What, int32 HowMany)
	{
		if (bLogging && What < EMade::Count)
		{
			MadeNow[static_cast<int32>(What)].fetch_add(HowMany, std::memory_order_relaxed);
		}
	}

	void NoteStillKind(FName Kind, int32 Points)
	{
		if (bLogging)
		{
			FScopeLock Lock(&KindsGuard);
			FIntPoint& Count = KindsNow.FindOrAdd(Kind, FIntPoint::ZeroValue);
			++Count.X;
			Count.Y += Points;
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

	// (Release 2, the far side: where the eye looks along the world's +Y and how far round the world is; see RoundWorld.
	// The game's own thread only.)
	static double SeamEyeAlong = 0.0, SeamSpan = 0.0;

	void RoundWorld(double EyeAlong, double Span)
	{
		SeamEyeAlong = EyeAlong;
		SeamSpan = FMath::Max(Span, 0.0);
	}

	bool FEye::Project(const FVector& World, FVector2D& OutScreen) const
	{
		// (the point in the turn of the world nearest the eye, where the world goes round; as given where it does not)
		const FVector Seen = SeamSpan > 0.0 ? FVector(World.X, World.Y + FMath::RoundToDouble((SeamEyeAlong - World.Y) / SeamSpan) * SeamSpan, World.Z) : World;
		if (!bOwn)
		{
			return Player && Player->ProjectWorldLocationToScreen(Seen, OutScreen, false);
		}
		// (The controller's own way then lets PostProcessWorldToScreen have its say; the campaign's controller leaves
		// it as the engine has it, which says yes and changes nothing.)
		return FSceneView::ProjectWorldToScreen(Seen, Rect, ViewProjection, OutScreen);
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

	// (Speed: tc.Speed.TidyAside, the second half.) The clear-out is a frame of 40 to 63 ms. Behind a pause the game
	// already has (a frame of over a quarter of a second: a load, a month's end) it is not seen, and the engine then
	// counts its five minutes from there. So after such a frame it is asked for at once, for the frame that follows,
	// unless it was done or asked for in the last half minute: a month's end is several long frames, and one clear-out
	// serves them all. It only sets the engine's own flag (UEngine::ForceGarbageCollection, no full purge): the engine
	// does it in its own place in the next frame, as it does when its clock runs out.
	// (Mended after the builder's capture 3 of 18 November, build 1688. A tenth of a second took in the game's own
	// stutters, 100 and 140 ms, and made each of them 40 ms longer: a quarter of a second takes the loads and the
	// months' ends and leaves those. And the half minute does not hold when a clear-out is owed, see TidyWhenHeavy:
	// a second load inside the half minute left its 900 MB to be cleared out in the open.)
	static void TidyBehind(double FrameTook)
	{
		if (FrameTook > 0.25 && CVarTidyAside.GetValueOnGameThread() != 0 && GEngine && (bTidyOwed || FPlatformTime::Seconds() - TidiedAt > 30.0))
		{
			TidiedAt = FPlatformTime::Seconds(); // (asked: not asked again for half a minute, whatever comes of it, unless one is owed again)
			bTidyOwed = false;
			GEngine->ForceGarbageCollection(false);
			if (bLogging)
			{
				UE_LOG(LogTheater, Display, TEXT("Speed: the engine's clear-out is asked for behind a pause of %.0f ms (tc.Speed.TidyAside)"), FrameTook * 1000.0);
			}
		}
	}

	// (...and its bound.) What waits to be cleared out is held in memory until it is. In the builder's capture 2 of
	// 18 November (build 1684, run F) a journey that stood 6,900 tiles and handed 8,440 models over in three minutes
	// with no clear-out held 600 MB more at its end than the same journey cleared every minute: about 200 MB a
	// minute, at a pace no player keeps. So the wait is not left to the clock alone. The game's memory is read once
	// a second, and when it stands half a gigabyte over the least it has been since the last clear-out, one is OWED:
	// the next pause is taken for it whenever the last was. Only at two gigabytes over is it asked for in the open.
	// (Mended after capture 3, run I, where it was asked for in the open at half a gigabyte. Four loads in half a
	// minute left 935 MB, so a list of loads was cleared out every half minute, wherever the eye was, in frames of
	// 97 and 138 ms on a slowed Mac: more often than Unreal's own minute and for longer. The memory a load brings
	// is not waste either, and the first clear-out of every run was for the world itself, 1,300 MB of it.)
	static void TidyWhenHeavy(double Now)
	{
		if (Now - TidyWeighedAt < 1.0)
		{
			return;
		}
		TidyWeighedAt = Now;
		const double HoldsMB = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
		if (TidyLowMB <= 0.0 || HoldsMB < TidyLowMB)
		{
			TidyLowMB = HoldsMB;
			return;
		}
		if (CVarTidyAside.GetValueOnGameThread() == 0 || !GEngine)
		{
			return;
		}
		const double OverMB = HoldsMB - TidyLowMB;
		if (OverMB > 2000.0 && Now - TidiedAt > 30.0)
		{
			TidiedAt = Now;
			bTidyOwed = false;
			GEngine->ForceGarbageCollection(false);
			if (bLogging)
			{
				UE_LOG(LogTheater, Display, TEXT("Speed: the engine's clear-out is asked for with the game holding %.0f MB, %.0f over its least since the last (tc.Speed.TidyAside)"), HoldsMB, OverMB);
			}
		}
		else if (OverMB > 500.0 && !bTidyOwed)
		{
			bTidyOwed = true;
			if (bLogging)
			{
				UE_LOG(LogTheater, Display, TEXT("Speed: the engine's clear-out is owed, the game holding %.0f MB, %.0f over its least since the last: the next pause is taken for it (tc.Speed.TidyAside)"), HoldsMB, OverMB);
			}
		}
	}

	// (The second speed detective's, 18 November 2026, after capture 3: four runs on build 1688 in which the same
	// work on the same view took the graphics chip 12 ms at the first run's start and 21 to 25 ms from its second
	// minute to the fourth run's end, and the same load's SetLand 224, 293, 386 and 367 ms. The Mac has no fan and
	// slows its own chip when it is hot, which is the likeliest reading, or another program was busy: the log could
	// not say which.
	// So each five seconds the log has a line on the machine: the system's own word on its heat, and how many of the
	// chip's cores were kept busy by the game and by everything else. Read on a Mac alone, with no cost to a frame.)
	static FMachineReading ReadMachine()
	{
		FMachineReading Seen;
		Seen.Clock = FPlatformTime::Seconds();
#if PLATFORM_MAC
		struct rusage Usage;
		if (getrusage(RUSAGE_SELF, &Usage) == 0)
		{
			Seen.GameSeconds = static_cast<double>(Usage.ru_utime.tv_sec) + static_cast<double>(Usage.ru_utime.tv_usec) * 1e-6
				+ static_cast<double>(Usage.ru_stime.tv_sec) + static_cast<double>(Usage.ru_stime.tv_usec) * 1e-6;
		}
		static const host_t Host = mach_host_self();
		natural_t CoreCount = 0;
		processor_info_array_t Info = nullptr;
		mach_msg_type_number_t InfoCount = 0;
		if (host_processor_info(Host, PROCESSOR_CPU_LOAD_INFO, &CoreCount, &Info, &InfoCount) == KERN_SUCCESS && Info != nullptr)
		{
			const processor_cpu_load_info_data_t* Load = reinterpret_cast<const processor_cpu_load_info_data_t*>(Info);
			for (natural_t Core = 0; Core < CoreCount; ++Core)
			{
				const uint64 AtWork = static_cast<uint64>(Load[Core].cpu_ticks[CPU_STATE_USER]) + static_cast<uint64>(Load[Core].cpu_ticks[CPU_STATE_SYSTEM])
					+ static_cast<uint64>(Load[Core].cpu_ticks[CPU_STATE_NICE]);
				Seen.BusyTicks += AtWork;
				Seen.AllTicks += AtWork + static_cast<uint64>(Load[Core].cpu_ticks[CPU_STATE_IDLE]);
			}
			Seen.Cores = static_cast<int32>(CoreCount);
			Seen.bValid = CoreCount > 0;
			vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(Info), static_cast<vm_size_t>(InfoCount) * sizeof(integer_t));
		}
#endif
		return Seen;
	}

	/** The system's own word on its heat: 0 nominal, 1 moderate, 2 heavy, 3 trapping, 4 sleeping; -1 where it is not told. */
	static int32 ReadHeat()
	{
#if PLATFORM_MAC
		// (the level the system's own heat keeper publishes, which needs no leave to read; the coarser "thermal state" a
		// program is offered calls both moderate and heavy "fair")
		static int Token = 0;
		static const bool bRegistered = notify_register_check("com.apple.system.thermalpressurelevel", &Token) == NOTIFY_STATUS_OK;
		uint64_t Level = 0;
		if (bRegistered && notify_get_state(Token, &Level) == NOTIFY_STATUS_OK)
		{
			return static_cast<int32>(Level);
		}
#endif
		return -1;
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
			// (Speed: tc.Speed.FontRoom, after the command line's sets so that one of them can turn it off.) Unreal keeps
			// the letters it has cut on pages 1024 points square and, with one page allowed, throws every letter away
			// when a second page is wanted ("FontCache flush requested. Reason: Large atlases out of space; 2/1
			// Textures"), then cuts them all again and lays every panel out afresh: a 25 to 45 ms frame about ten times
			// in a two-minute journey over the world, in the builder's capture of 18 November (build 1680). The names
			// lettered on the map at their two largest sizes do not fit one page with everything else. Eight pages are
			// allowed here (8 MB at most). The pre-checker read the engine: the number is read each time a page is
			// added (FontCache.cpp 30 to 55, TextureAtlas.cpp 446 to 475), so setting it here is in time.
			if (CVarFontRoom.GetValueOnGameThread() != 0)
			{
				static const TCHAR* const PagesName = TEXT("Slate.MaxFontAtlasPagesBeforeFlush");
				if (IConsoleVariable* Pages = IConsoleManager::Get().FindConsoleVariable(PagesName))
				{
					if (Pages->GetInt() < 8)
					{
						Pages->Set(8, ECVF_SetByCode);
						UE_LOG(LogTheater, Display, TEXT("Speed: %s set to %d (tc.Speed.FontRoom)"), PagesName, Pages->GetInt());
					}
				}
				else
				{
					UE_LOG(LogTheater, Warning, TEXT("Speed: tc.Speed.FontRoom found no console variable called %s"), PagesName);
				}
			}
			// (Speed: tc.Speed.TidyAside, the first half.) Every 61.1 s exactly Unreal clears out what is no longer used,
			// all in one frame, and under the editor's program that is every object the editor has as well as the game's:
			// a frame of 40 to 63 ms wherever the eye is and whatever the player is doing, eleven times in the five runs
			// of the builder's capture of 18 November (build 1680; "EngineClearOut" in the long-frame line). A hundred and
			// eighty stutters in a three-hour evening. It is asked for every five minutes here. What waits longer to be
			// cleared is what a journey drops: at most the pictures of the fine sheets dropped from the shelf in those
			// minutes, 15 to 20 MB each. The second half is in NoteFrame.
			// (Not in a test's run, "-unattended": there the clear-out every minute is what finds an object held too
			// loosely, as it found the effects kit's materials on 18 November, and a two-minute run with the wait at
			// five minutes would never clear out at all. A test keeps its minute, and by the second half clears out
			// behind its loads as well, which is more often than before. A timing run that wants an evening's wait
			// asks for it: -TheaterSet=gc.TimeBetweenPurgingPendingKillObjects=300.)
			if (CVarTidyAside.GetValueOnGameThread() != 0 && !FApp::IsUnattended())
			{
				static const TCHAR* const EveryName = TEXT("gc.TimeBetweenPurgingPendingKillObjects");
				const float Every = 300.0f;
				if (IConsoleVariable* Between = IConsoleManager::Get().FindConsoleVariable(EveryName))
				{
					if (Between->GetFloat() < Every)
					{
						Between->Set(Every, ECVF_SetByCode);
						UE_LOG(LogTheater, Display, TEXT("Speed: %s set to %.0f (tc.Speed.TidyAside)"), EveryName, Between->GetFloat());
					}
				}
				else
				{
					UE_LOG(LogTheater, Warning, TEXT("Speed: tc.Speed.TidyAside found no console variable called %s"), EveryName);
				}
			}
		}
	}

	/**
	 * tc.Speed.FrameCap: puts the cap asked for in force when the switch changes, and takes it off again. With the switch
	 * at 0 from the start nothing is touched. (Capture 4, 19 November: nothing in the game's code or settings limited its
	 * frames, and run Q's cap was asked for by -TheaterSet=t.MaxFPS=60. Set here by code, a value set by hand or by
	 * -TheaterSet outranks it and stays: what is read back says which.)
	 */
	static void ApplyFrameCap()
	{
		const int32 Asked = CVarFrameCap.GetValueOnGameThread();
		const int32 Want = Asked < 0 ? -1 : Asked == 0 ? 0 : FMath::Clamp(Asked, 20, 240);
		if (Want == CapInForce)
		{
			return;
		}
		static const TCHAR* const MaxName = TEXT("t.MaxFPS");
		static const TCHAR* const SyncName = TEXT("r.VSync");
		IConsoleVariable* MaxFPS = IConsoleManager::Get().FindConsoleVariable(MaxName);
		IConsoleVariable* Sync = IConsoleManager::Get().FindConsoleVariable(SyncName);
		// What was put in force before goes back to how it stood (a value set by hand since stays: it outranks code).
		if (bCapSetMax && MaxFPS)
		{
			MaxFPS->Set(CapMaxWas, ECVF_SetByCode);
			UE_LOG(LogTheater, Display, TEXT("Speed: %s put back to %.0f (tc.Speed.FrameCap)"), MaxName, MaxFPS->GetFloat());
		}
		if (bCapSetSync && Sync)
		{
			Sync->Set(CapSyncWas, ECVF_SetByCode);
			UE_LOG(LogTheater, Display, TEXT("Speed: %s put back to %d (tc.Speed.FrameCap)"), SyncName, Sync->GetInt());
		}
		bCapSetMax = bCapSetSync = false;
		CapInForce = Want;
		if (Want > 0)
		{
			if (!MaxFPS)
			{
				UE_LOG(LogTheater, Warning, TEXT("Speed: tc.Speed.FrameCap found no console variable called %s"), MaxName);
				return;
			}
			CapMaxWas = MaxFPS->GetFloat();
			MaxFPS->Set(static_cast<float>(Want), ECVF_SetByCode);
			bCapSetMax = FMath::IsNearlyEqual(MaxFPS->GetFloat(), static_cast<float>(Want), 0.5f);
			UE_LOG(LogTheater, Display, TEXT("Speed: %s %s %.0f%s (tc.Speed.FrameCap)"), MaxName, bCapSetMax ? TEXT("set to") : TEXT("left at"), MaxFPS->GetFloat(),
				bCapSetMax ? TEXT("") : TEXT(", set by hand or by -TheaterSet"));
		}
		else if (Want < 0)
		{
			if (!Sync)
			{
				UE_LOG(LogTheater, Warning, TEXT("Speed: tc.Speed.FrameCap found no console variable called %s"), SyncName);
				return;
			}
			CapSyncWas = Sync->GetInt();
			Sync->Set(1, ECVF_SetByCode);
			bCapSetSync = Sync->GetInt() == 1;
			UE_LOG(LogTheater, Display, TEXT("Speed: %s %s %d%s (tc.Speed.FrameCap)"), SyncName, bCapSetSync ? TEXT("set to") : TEXT("left at"), Sync->GetInt(),
				bCapSetSync ? TEXT("") : TEXT(", set by hand or by -TheaterSet"));
		}
	}

	bool LastWindow(double& Seconds, double& FramesASecond, double& WorstMs)
	{
		Seconds = LastWindowClock;
		FramesASecond = LastWindowRate;
		WorstMs = LastWindowWorstMs;
		return LastWindowClock > 0.0;
	}

	void NoteFrame(float DeltaSeconds, UWorld* World)
	{
		ApplyCommandLineSets(); // (in case no table was laid first: it is done once)
		ApplyFrameCap(); // (after the command line's sets, so that -TheaterSet=tc.Speed.FrameCap=60 is in force from the first frame)
		// (Speed: tc.Speed.TidyAside, the second half. The frame just ended by the machine's own clock, which the game's
		// speed and its pauses do not stretch; and when the engine last cleared out, whoever asked.)
		const double SeenNow = FPlatformTime::Seconds();
		const double FrameTook = FrameSeenAt > 0.0 ? SeenNow - FrameSeenAt : 0.0;
		FrameSeenAt = SeenNow;
		{
			static bool bTidyHooked = false;
			if (!bTidyHooked)
			{
				bTidyHooked = true;
				FCoreUObjectDelegates::GetPostGarbageCollect().AddLambda([]()
				{
					if (IsInGameThread())
					{
						TidiedAt = FPlatformTime::Seconds();
						TidyLowMB = 0.0; // (the least is found afresh: what was cleared goes over the frames that follow)
						bTidyOwed = false;
					}
				});
			}
			TidyWhenHeavy(SeenNow);
		}
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
			WindowIdle = WindowOverslept = 0.0;
			WindowInFront = 0;
			for (int32 Index = 0; Index < static_cast<int32>(EMade::Count); ++Index)
			{
				MadeNow[Index].store(0, std::memory_order_relaxed);
				MadeFrame[Index] = 0;
				MadeWindow[Index] = 0;
			}
			WindowModelling = WindowSlowModelling = 0;
			MachineWas = FMachineReading();
			TidyBehind(FrameTook); // (with no log there is no telling the step player's looks apart: a test's pause serves as well)
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
		TMap<FName, FIntPoint> KindsFrame; // (...and the still models among it, by kind)
		{
			FScopeLock Lock(&KindsGuard);
			KindsFrame = MoveTemp(KindsNow);
			KindsNow.Reset();
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
		// (A pause that is the step player's own look is not one a player has: with the log on, where the looks are
		// known, the clear-out is not hidden behind them, so that a measured run tells what an evening's play will.)
		TidyBehind(bBeside ? 0.0 : FrameTook);
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
				// (And which still models those were: in the capture of 18 November a jump of the eye handed 130 models
				// of half a million points to the draw thread in one frame, 35 to 56 ms, and the log could not say whose.)
				if (DeltaSeconds > 0.0334f && KindsFrame.Num() > 0)
				{
					KindsFrame.ValueSort([](const FIntPoint& A, const FIntPoint& B) { return A.Y > B.Y; });
					FString Kinds;
					int32 Shown = 0;
					for (const TPair<FName, FIntPoint>& Kind : KindsFrame)
					{
						if (++Shown > 6)
						{
							break;
						}
						Kinds += FString::Printf(TEXT("%s%s %d (%d points)"), Kinds.IsEmpty() ? TEXT("") : TEXT(", "), *Kind.Key.ToString(), Kind.Value.X, Kind.Value.Y);
					}
					UE_LOG(LogTheater, Display, TEXT("Speed: still models of that frame by kind, the most points first: %s"), *Kinds);
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
		{
			// (the engine's own wait to keep to a cap, in the frame just ended, and by how much the system's sleep overran it)
			const double Idle = FApp::GetIdleTime();
			WindowIdle += Idle * 1000.0;
			WindowOverslept += Idle > 0.0 ? FMath::Max(FApp::GetIdleTimeOvershoot(), 0.0) * 1000.0 : 0.0;
			WindowInFront += FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive() ? 1 : 0;
		}
		if (WindowClock < 5.0 || WindowFrames < 2)
		{
			return;
		}

		const double Frames = WindowFrames;
		LastWindowClock = WindowClock;
		LastWindowRate = Frames / WindowClock;
		LastWindowWorstMs = WindowWorst * 1000.0;
		UE_LOG(LogTheater, Display, TEXT("Speed: %.1f s, %.1f frames a second (worst frame %.1f ms; %d of %d over 33 ms, %d of those beside the step player's look); game %.1f ms, draw %.1f ms, graphics chip %.1f ms"),
			WindowClock, Frames / WindowClock, WindowWorst * 1000.0, WindowSlow, WindowFrames, WindowSlowBeside, WindowGame / Frames, WindowDraw / Frames, WindowChip / Frames);
		// (Speed, 18 November 2026: a line of its own, so that the line above reads as it did. Is the draw thread's time
		// its own, or the RHI's too? With no RHI thread, the RHI's work is done on the draw thread and counted there.)
		UE_LOG(LogTheater, Display, TEXT("Speed: RHI thread %.1f ms a frame%s"), WindowRHI / Frames,
			GRHIThreadTime == 0 && WindowRHI <= 0.0 ? TEXT(" (none: its work is in the draw thread's time)") : TEXT(""));
		{
			// (The machine's line, a line of its own as the RHI thread's is: see ReadMachine. Nothing where neither is told.)
			const FMachineReading Machine = ReadMachine();
			const int32 Heat = ReadHeat();
			static const TCHAR* const HeatNames[] = { TEXT("nominal"), TEXT("moderate"), TEXT("heavy"), TEXT("trapping"), TEXT("sleeping") };
			const FString HeatWord = Heat < 0 ? FString(TEXT("not told"))
				: Heat < static_cast<int32>(UE_ARRAY_COUNT(HeatNames)) ? FString::Printf(TEXT("%s (%d of 4)"), HeatNames[Heat], Heat)
				: FString::Printf(TEXT("%d"), Heat);
			if (Machine.bValid && MachineWas.bValid && Machine.AllTicks > MachineWas.AllTicks && Machine.BusyTicks >= MachineWas.BusyTicks && Machine.Clock > MachineWas.Clock)
			{
				const double BusyCores = static_cast<double>(Machine.Cores) * static_cast<double>(Machine.BusyTicks - MachineWas.BusyTicks) / static_cast<double>(Machine.AllTicks - MachineWas.AllTicks);
				const double GameCores = FMath::Max(Machine.GameSeconds - MachineWas.GameSeconds, 0.0) / (Machine.Clock - MachineWas.Clock);
				UE_LOG(LogTheater, Display, TEXT("Speed: the machine: heat %s; of its %d cores the game kept %.1f busy and other programs %.1f"),
					*HeatWord, Machine.Cores, GameCores, FMath::Max(BusyCores - GameCores, 0.0));
			}
			else if (Heat >= 0)
			{
				UE_LOG(LogTheater, Display, TEXT("Speed: the machine: heat %s"), *HeatWord);
			}
			MachineWas = Machine;
		}
		{
			// (Speed: tc.Speed.FrameCap. The held frames' line, a line of its own as the machine's is. A capped game sleeps
			// between frames, and macOS stretches those sleeps when the screen is dark or the window hidden: capture 4's
			// run Q fell to 21 to 26 frames a second at nominal heat with 0.7 cores busy. This says whether a run had the
			// lit screen and the window in front that a timing with a cap wants.)
			const IConsoleVariable* MaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
			const IConsoleVariable* Sync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			const float MaxNow = MaxFPS ? MaxFPS->GetFloat() : 0.0f;
			const int32 SyncNow = Sync ? Sync->GetInt() : 0;
			const FString Cap = CapInForce > 0 ? FString::Printf(TEXT("%d a second"), CapInForce) : CapInForce < 0 ? FString(TEXT("the screen's refresh")) : FString(TEXT("none"));
			UE_LOG(LogTheater, Display, TEXT("Speed: held frames: tc.Speed.FrameCap %s (t.MaxFPS %.0f, r.VSync %d); the engine waited %.1f ms a frame to keep to it, its waits overran by %.1f; the game's window in front in %d of %d frames"),
				*Cap, MaxNow, SyncNow, WindowIdle / Frames, WindowOverslept / Frames, WindowInFront, WindowFrames);
			const double Rate = Frames / WindowClock;
			if (MaxNow > 0.0f && Rate < 0.8 * MaxNow && WindowIdle / Frames > 5.0)
			{
				UE_LOG(LogTheater, Warning, TEXT("Speed: %.1f frames a second under a cap of %.0f while the engine waited %.1f ms a frame: held longer than the cap asks. Is the screen dark or the game's window hidden? A timing with a cap wants a lit screen and the window in front."),
					Rate, MaxNow, WindowIdle / Frames);
			}
		}
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
		WindowIdle = WindowOverslept = 0.0;
		WindowInFront = 0;
		for (int32 Index = 0; Index < static_cast<int32>(EMade::Count); ++Index)
		{
			MadeWindow[Index] = 0;
		}
		WindowModelling = WindowSlowModelling = 0;
	}
}
