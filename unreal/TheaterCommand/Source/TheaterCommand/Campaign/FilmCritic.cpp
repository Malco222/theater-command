#include "Campaign/FilmCritic.h"

#include "Battle/AirBattle.h"
#include "Battle/AirBattleState.h"
#include "Battle/LandBattle.h"
#include "Battle/LandBattleState.h"
#include "Battle/LandScape.h"
#include "Campaign/ArmyPiece.h"
#include "Campaign/FilmCriticLine.h"
#include "TheaterCommand.h"
#include "Async/Async.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CoreGlobals.h" // (GFrameCounter, GStartTime)
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FrameGrabber.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "Slate/SceneViewport.h"
#include "UnrealClient.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

// The film critic's facts (development only, -FilmCritic): see FilmCritic.h and Tools/film_critic/FACTS-BUILD-NOTES.md.
// Everything here runs only behind FilmCritic::On(); its state is made on the first fact and never freed.

namespace FilmCriticParts
{
	using FCriticLine = FilmCriticLine::Line;

	/** At most this many strip frames a run (INTERFACE.md section 1), and JPEGs being written at once. */
	static constexpr int32 MostStripFrames = 1500;
	static constexpr int32 MostWriting = 16;
	/** The strips' width (INTERFACE.md section 1). */
	static constexpr int32 StripWide = 960;

	static double NoValue()
	{
		return std::numeric_limits<double>::quiet_NaN();
	}

	/** A camera as it is drawn: Unreal's world axes, in metres (the scene's units a metre); its turn in degrees. */
	struct FCriticCam
	{
		bool bValid = false;
		uint64 Frame = 0;
		double T = 0.0;
		std::string Scene;
		bool bBt = false;
		double Bt = 0.0;
		FVector Eye = FVector::ZeroVector;
		FVector EyeUnits = FVector::ZeroVector;
		FRotator Turn = FRotator::ZeroRotator;
		double Fov = 90.0;
		double DtMs = 0.0;
		bool bCut = false;
		std::string Why, Subject;
		bool bSubjectAt = false;
		FVector SubjectAt = FVector::ZeroVector;
		std::string Left, Special;
		double Curtain = 0.0, Bars = 0.0;
	};

	/** What a camera's picture holds: a point (Unreal units) in front of the eye and inside its lens's frame. */
	struct FCriticLens
	{
		FVector At = FVector::ZeroVector;
		FVector Ahead = FVector::ForwardVector, Across = FVector::RightVector, Upward = FVector::UpVector;
		double TanAcross = 1.0, TanUp = 1.0;

		void Set(const FVector& InAt, const FRotator& InTurn, double FovDegrees, double Aspect)
		{
			At = InAt;
			const FQuat Facing = InTurn.Quaternion();
			Ahead = Facing.GetForwardVector();
			Across = Facing.GetRightVector();
			Upward = Facing.GetUpVector();
			TanAcross = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FovDegrees, 1.0, 170.0) * 0.5));
			TanUp = TanAcross / FMath::Max(0.1, Aspect);
		}
		bool Sees(const FVector& Point) const
		{
			const FVector Off = Point - At;
			const double Along = FVector::DotProduct(Off, Ahead);
			if (Along <= 1.0)
			{
				return false;
			}
			return FMath::Abs(FVector::DotProduct(Off, Across)) <= Along * TanAcross && FMath::Abs(FVector::DotProduct(Off, Upward)) <= Along * TanUp;
		}
	};

	/** Each side's aircraft this frame (side 0 the attack, 1 the defence): in frame, the nearest in frame and of all. */
	struct FCriticSide
	{
		int32 InFrame = 0;
		double Nearest = std::numeric_limits<double>::quiet_NaN();
		double NearestAll = std::numeric_limits<double>::quiet_NaN();
	};
	struct FCriticPlanes
	{
		bool bValid = false;
		uint64 Frame = 0;
		FCriticSide Sides[2];
	};

	/** What has been written once for a battle (its look, its end, its opening strip) and when its next lines are due. */
	struct FCriticBattle
	{
		TWeakObjectPtr<const AActor> Battle;
		bool bLook = false, bEnd = false, bOpening = false;
		double NextMen = 0.0, NextVehicles = 0.0, NextPlanes = 0.0;
	};

	/** A still asked for, waiting for the camera of its frame. */
	struct FCriticStill
	{
		FString Name, File;
		uint64 Frame = 0;
	};

	/** A strip frame asked of the grabber, waiting for its pixels. */
	struct FCriticAsk
	{
		int32 Seq = 0;
		double T = 0.0;
		uint64 Frame = 0;
		bool bBt = false;
		double Bt = 0.0;
		FString File, Of;
	};

	/** Rides with each frame asked of the grabber, so a frame read back is matched to its own ask, never to the next
	 * one in line when a frame was dropped (the default behaviour is kept: the frame is stored for GetCapturedFrames). */
	struct FCriticFramePayload : public IFramePayload
	{
		explicit FCriticFramePayload(int32 InSeq) : Seq(InSeq) {}
		int32 Seq = 0;
	};

	/** The strip being taken now. */
	struct FCriticStrip
	{
		bool bOn = false;
		FString Tag, Of;
		double Until = 0.0, NextAt = 0.0;
		bool bUntilStill = false;
	};

	struct FCriticState
	{
		bool bBegun = false;
		FString Folder, Path;
		std::string RunScene;
		bool bSteps = false;
		bool bStrips = false;
		double StripFps = 0.0, StripSeconds = 0.0;
		std::string Buffer;
		double LastFlushT = 0.0;
		// (the frame before this one, whose picture the camera manager holds at the start of this frame)
		bool bHaveLast = false;
		double LastT = 0.0;
		uint64 LastFrame = 0;
		double LastDtMs = 0.0;
		bool bCutNoted = false;
		uint64 CutFrame = 0;
		FCriticCam LastBattleCam;
		FCriticCam LastMapCam;
		double LastMapLineT = -1.0e9;
		TArray<FCriticStill> Stills;
		FCriticPlanes Planes;
		TArray<FCriticBattle> Battles;
		double NextFleetT = 0.0;
		TMap<int32, FVector2D> LastCourse;
		std::set<std::string> MarksSeen;
		// The strips.
		TSharedPtr<FFrameGrabber> Grab;
		bool bGrabTried = false;
		FCriticStrip Strip;
		TArray<FCriticAsk> Asks;
		TMap<FString, int32> NextIndex;
		int32 StripAsks = 0;
		int32 NextSeq = 0;
		int32 Dropped = 0;
		std::atomic<int32> Writing{ 0 };
	};

	/** The one state, made on first use and never freed (nothing runs at the program's static teardown). */
	static FCriticState& State()
	{
		static FCriticState* Kept = new FCriticState();
		return *Kept;
	}

	static double NowT()
	{
		return FPlatformTime::Seconds() - GStartTime;
	}

	static std::string Utf8(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	/** A line's start: t, k, f and, in a battle, bt. */
	static FCriticLine NewLine(const char* Kind, double T, uint64 Frame, bool bBt, double Bt)
	{
		FCriticLine Made(T, Kind);
		Made.Whole("f", static_cast<long long>(Frame));
		if (bBt)
		{
			Made.Num("bt", Bt, 3);
		}
		return Made;
	}

	static void FlushLines(FCriticState& St)
	{
		if (St.Buffer.empty() || St.Path.IsEmpty())
		{
			return;
		}
		FFileHelper::SaveStringToFile(FString(UTF8_TO_TCHAR(St.Buffer.c_str())), *St.Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		St.Buffer.clear();
	}

	/** A line kept; the file written at most once a second. */
	static void AddLine(FCriticState& St, const std::string& Text)
	{
		St.Buffer += Text;
		const double T = NowT();
		if (T - St.LastFlushT >= 1.0)
		{
			St.LastFlushT = T;
			FlushLines(St);
		}
	}

	static bool ViewSize(double& OutW, double& OutH)
	{
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D Size(0.0, 0.0);
			GEngine->GameViewport->GetViewportSize(Size);
			if (Size.X > 0.0 && Size.Y > 0.0)
			{
				OutW = Size.X;
				OutH = Size.Y;
				return true;
			}
		}
		return false;
	}

	static double AspectNow()
	{
		double W = 0.0, H = 0.0;
		return ViewSize(W, H) ? W / H : 1.6;
	}

	static void CameraFields(FCriticLine& L, const FCriticCam& C)
	{
		L.Str("scene", C.Scene);
		L.Vec3("eye", C.Eye.X, C.Eye.Y, C.Eye.Z, 3);
		L.Vec3("rot", C.Turn.Pitch, C.Turn.Yaw, C.Turn.Roll, 3);
		L.Num("fov", C.Fov, 3);
		L.Num("dt", C.DtMs, 2);
		L.Flag("cut", C.bCut);
		if (!C.Why.empty())
		{
			L.Str("why", C.Why);
		}
		if (!C.Subject.empty())
		{
			if (C.bSubjectAt)
			{
				L.NamedAt("subject", C.Subject, C.SubjectAt.X, C.SubjectAt.Y, C.SubjectAt.Z, 3);
			}
			else
			{
				L.Str("subject", C.Subject);
			}
		}
		if (!C.Left.empty())
		{
			L.Str("left", C.Left);
		}
		if (!C.Special.empty())
		{
			L.Str("special", C.Special);
		}
		if (C.Curtain > 0.0)
		{
			L.Num("curtain", C.Curtain, 3);
		}
		if (C.Bars > 0.0)
		{
			L.Num("bars", C.Bars, 3);
		}
	}

	/** A battle's camera actor as it stands now: its lens's place and turn (the jolt or shake already on it). */
	static void FromEye(const ACameraActor& Eye, double UnitsPerMetre, float DeltaSeconds, FCriticCam& C)
	{
		const UCameraComponent* Lens = Eye.GetCameraComponent();
		C.EyeUnits = Lens ? Lens->GetComponentLocation() : Eye.GetActorLocation();
		C.Turn = Lens ? Lens->GetComponentRotation() : Eye.GetActorRotation();
		C.Fov = Lens ? static_cast<double>(Lens->FieldOfView) : 90.0;
		C.Eye = C.EyeUnits / UnitsPerMetre;
		C.DtMs = static_cast<double>(DeltaSeconds) * 1000.0;
	}

	static void WritePlanes(FCriticState& St, const FCriticCam& C, const std::string& StillName)
	{
		const FCriticPlanes& P = St.Planes;
		const std::vector<std::pair<std::string, double>> InFrame = {
			{ "attack", static_cast<double>(P.Sides[0].InFrame) }, { "defend", static_cast<double>(P.Sides[1].InFrame) } };
		const std::vector<std::pair<std::string, double>> Nearest = { { "attack", P.Sides[0].Nearest }, { "defend", P.Sides[1].Nearest } };
		const std::vector<std::pair<std::string, double>> NearestAll = { { "attack", P.Sides[0].NearestAll }, { "defend", P.Sides[1].NearestAll } };
		FCriticLine L = NewLine("planes", C.T, C.Frame, C.bBt, C.Bt);
		L.Str("shot", C.Why).Pairs("inframe", InFrame, 0).Pairs("nearest_m", Nearest, 1).Pairs("nearest_all_m", NearestAll, 1);
		if (!StillName.empty())
		{
			L.Str("still", StillName);
		}
		AddLine(St, L.Done());
	}

	static void WriteStill(FCriticState& St, const FCriticStill& Picture, const FCriticCam& C)
	{
		FCriticLine L = NewLine("still", C.T, C.Frame, C.bBt, C.Bt);
		L.Str("name", Utf8(Picture.Name)).Str("file", Utf8(Picture.File));
		CameraFields(L, C);
		AddLine(St, L.Done());
		// (an air still: the aircraft in its frame too)
		if (C.Scene == "air" && St.Planes.bValid && St.Planes.Frame == C.Frame)
		{
			WritePlanes(St, C, Utf8(Picture.Name));
		}
	}

	/** A battle's camera line just written: the stills asked for this frame (before it) take it. */
	static void BattleCamWritten(FCriticState& St, const FCriticCam& C)
	{
		St.LastBattleCam = C;
		St.LastBattleCam.bValid = true;
		for (const FCriticStill& Picture : St.Stills)
		{
			WriteStill(St, Picture, C);
		}
		St.Stills.Reset();
	}

	/** The fleets on the screen now, each a line (At: "sail", or the still they were seen at). */
	static void WriteFleets(FCriticState& St, APlayerController& Player, const FCriticCam& C, const std::string& At)
	{
		UWorld* Here = Player.GetWorld();
		if (!Here)
		{
			return;
		}
		int32 ViewW = 0, ViewH = 0;
		Player.GetViewportSize(ViewW, ViewH);
		for (TActorIterator<AArmyPiece> It(Here); It; ++It)
		{
			const AArmyPiece* Piece = *It;
			if (!Piece || !Piece->IsFleet() || Piece->IsHidden())
			{
				continue;
			}
			FVector2D Middle(0.0, 0.0);
			if (!Player.ProjectWorldLocationToScreen(Piece->GetActorLocation(), Middle, false) || Middle.X < 0.0 || Middle.Y < 0.0 ||
				Middle.X > static_cast<double>(ViewW) || Middle.Y > static_cast<double>(ViewH))
			{
				continue;
			}
			FString Name;
			int32 Wake = 0;
			double Progress = -1.0;
			FVector Course = FVector::ZeroVector, Bow = FVector::ZeroVector, BowAt = FVector::ZeroVector, TailAt = FVector::ZeroVector;
			bool bTail = false;
			if (!Piece->FilmFleet(Name, Wake, Progress, Course, Bow, BowAt, TailAt, bTail))
			{
				continue;
			}
			const int32 Id = Piece->GetForceId();
			const bool bSailing = Piece->IsSailing();
			FCriticLine L = NewLine("fleet", C.T, C.Frame, false, 0.0);
			L.Whole("id", Id).Str("name", Utf8(Name)).Str("nation", Utf8(Piece->GetNationTag())).Flag("sailing", bSailing);
			if (bSailing && Progress >= 0.0)
			{
				L.Num("progress", Progress, 3);
			}
			else
			{
				L.Nothing("progress");
			}
			const FVector2D Way(Course.X, Course.Y);
			if (bSailing && Way.SizeSquared() > 1.0e-8)
			{
				const FVector2D Unit = Way.GetSafeNormal();
				L.Vec2("course", Unit.X, Unit.Y, 4);
				St.LastCourse.Add(Id, Unit);
			}
			else
			{
				L.Nothing("course");
				if (const FVector2D* Last = St.LastCourse.Find(Id))
				{
					L.Vec2("last_course", Last->X, Last->Y, 4);
				}
			}
			const FVector2D BowWay = FVector2D(Bow.X, Bow.Y).GetSafeNormal();
			L.Vec2("bow", BowWay.X, BowWay.Y, 4);
			L.Whole("wake", Wake);
			L.Num("camyaw", C.Turn.Yaw, 3);
			FVector2D BowPx(0.0, 0.0), TailPx(0.0, 0.0);
			if (Player.ProjectWorldLocationToScreen(BowAt, BowPx, false))
			{
				L.Vec2("bowpx", BowPx.X, BowPx.Y, 1);
			}
			else
			{
				L.Nothing("bowpx");
			}
			if (bTail && Player.ProjectWorldLocationToScreen(TailAt, TailPx, false))
			{
				L.Vec2("tailpx", TailPx.X, TailPx.Y, 1);
			}
			else
			{
				L.Nothing("tailpx");
			}
			L.Str("at", At);
			AddLine(St, L.Done());
		}
	}

	static bool AnyFleetSailing(APlayerController& Player)
	{
		UWorld* Here = Player.GetWorld();
		if (!Here)
		{
			return false;
		}
		for (TActorIterator<AArmyPiece> It(Here); It; ++It)
		{
			if (*It && It->IsFleet() && It->IsSailing())
			{
				return true;
			}
		}
		return false;
	}

	static FCriticBattle& BattleOf(FCriticState& St, const AActor* Battle)
	{
		for (FCriticBattle& Each : St.Battles)
		{
			if (Each.Battle.IsValid() && Each.Battle.Get() == Battle)
			{
				return Each;
			}
		}
		if (St.Battles.Num() >= 8)
		{
			St.Battles.RemoveAt(0);
		}
		FCriticBattle& Made = St.Battles.AddDefaulted_GetRef();
		Made.Battle = Battle;
		return Made;
	}

	/** Makes the strips' frame grabber, once (the land battle's dark reading's way: the game's own viewport). */
	static void EnsureGrab(FCriticState& St)
	{
		if (St.Grab.IsValid())
		{
			return;
		}
		const bool bFirst = !St.bGrabTried;
		St.bGrabTried = true;
		UGameEngine* Game = Cast<UGameEngine>(GEngine);
		if (!Game || !Game->SceneViewport.IsValid())
		{
			if (bFirst)
			{
				UE_LOG(LogTheater, Display, TEXT("Film critic: no game viewport to take film strips from"));
			}
			return;
		}
		const FIntPoint Size = Game->SceneViewport->GetSizeXY();
		if (Size.X <= 0 || Size.Y <= 0)
		{
			return;
		}
		int32 High = FMath::RoundToInt32(static_cast<double>(StripWide) * static_cast<double>(Size.Y) / static_cast<double>(Size.X));
		High = FMath::Max(2, High - (High % 2));
		// (the JPEG writer's module, loaded here on the game thread: the strips are written on the thread pool)
		(void)FModuleManager::Get().LoadModule(TEXT("ImageWrapper"));
		St.Grab = MakeShared<FFrameGrabber>(Game->SceneViewport.ToSharedRef(), FIntPoint(StripWide, High), PF_B8G8R8A8, 6);
		St.Grab->StartCapturingFrames();
		UE_LOG(LogTheater, Display, TEXT("Film critic: film strips at %d x %d, %.1f a second for %.1f s"), StripWide, High, St.StripFps, St.StripSeconds);
	}

	static void StopGrab(FCriticState& St)
	{
		if (St.Grab.IsValid())
		{
			// (the frames asked on the render thread are asked before the grabber goes)
			FlushRenderingCommands();
			St.Grab->StopCapturingFrames();
			St.Grab->Shutdown();
			St.Grab.Reset();
		}
		St.Asks.Reset();
		St.Strip.bOn = false;
	}

	static FString SafeTag(const FString& Name)
	{
		FString Out;
		for (const TCHAR Ch : Name)
		{
			const bool bPlain = (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z')) || (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('-') || Ch == TEXT('_') || Ch == TEXT('.');
			Out.AppendChar(bPlain ? Ch : TEXT('-'));
		}
		return Out.IsEmpty() ? FString(TEXT("strip")) : Out;
	}

	/** A strip begins (a new one takes the place of one still running): its frames strip-<Tag>-<NNNN>.jpg, "of" Of. */
	static void StartStrip(FCriticState& St, const FString& Tag, const FString& Of, bool bUntilStill)
	{
		if (!St.bStrips || St.StripAsks >= MostStripFrames)
		{
			return;
		}
		const double T = NowT();
		St.Strip.bOn = true;
		St.Strip.Tag = SafeTag(Tag);
		St.Strip.Of = Of;
		St.Strip.Until = T + St.StripSeconds;
		St.Strip.NextAt = T;
		St.Strip.bUntilStill = bUntilStill;
		EnsureGrab(St);
	}

	/** The strips' work of a frame: what the grabber has read back is written, and this frame asked for when due. */
	static void StripFrame(FCriticState& St, double T, uint64 Frame)
	{
		if (!St.Grab.IsValid())
		{
			return;
		}
		TArray<FCapturedFrameData> Frames = St.Grab->GetCapturedFrames();
		for (FCapturedFrameData& Got : Frames)
		{
			const FCriticFramePayload* Pay = static_cast<const FCriticFramePayload*>(Got.Payload.Get());
			if (!Pay)
			{
				++St.Dropped;
				continue;
			}
			// (asks older than this frame were never read back: let them go, so each JPEG keeps its own ask's name)
			while (St.Asks.Num() > 0 && St.Asks[0].Seq < Pay->Seq)
			{
				St.Asks.RemoveAt(0);
				++St.Dropped;
			}
			if (St.Asks.Num() == 0 || St.Asks[0].Seq != Pay->Seq)
			{
				++St.Dropped;
				continue;
			}
			const FCriticAsk Ask = St.Asks[0];
			St.Asks.RemoveAt(0);
			const int32 GotW = Got.BufferSize.X, GotH = Got.BufferSize.Y;
			if (GotW <= 0 || GotH <= 0 || Got.ColorBuffer.Num() < GotW * GotH || St.Writing.load() >= MostWriting)
			{
				++St.Dropped;
				continue;
			}
			const FString Path = FPaths::Combine(St.Folder, Ask.File);
			std::atomic<int32>* Count = &St.Writing;
			Count->fetch_add(1);
			(void)Async(EAsyncExecution::ThreadPool, [Pixels = MoveTemp(Got.ColorBuffer), GotW, GotH, Path, Count]() mutable
			{
				FImageUtils::SaveImageByExtension(*Path, FImageView(Pixels.GetData(), GotW, GotH));
				Count->fetch_sub(1);
			});
			FCriticLine L = NewLine("strip", Ask.T, Ask.Frame, Ask.bBt, Ask.Bt);
			L.Str("file", Utf8(Ask.File)).Str("of", Utf8(Ask.Of)).Whole("w", GotW).Whole("h", GotH);
			if (St.Dropped > 0)
			{
				L.Whole("dropped", St.Dropped);
			}
			AddLine(St, L.Done());
		}
		if (!St.Strip.bOn || !St.Grab->IsCapturingFrames())
		{
			return;
		}
		if (T > St.Strip.Until || St.StripAsks >= MostStripFrames)
		{
			St.Strip.bOn = false;
			return;
		}
		const double Every = 1.0 / FMath::Max(0.1, St.StripFps);
		if (T + 0.004 < St.Strip.NextAt)
		{
			return;
		}
		St.Strip.NextAt = FMath::Max(St.Strip.NextAt + Every, T + 0.5 * Every);
		int32& Index = St.NextIndex.FindOrAdd(St.Strip.Tag);
		FCriticAsk Ask;
		Ask.Seq = ++St.NextSeq;
		Ask.T = T;
		Ask.Frame = Frame;
		Ask.bBt = St.LastBattleCam.bValid && St.LastBattleCam.Frame + 1 >= Frame;
		Ask.Bt = Ask.bBt ? St.LastBattleCam.Bt : 0.0;
		Ask.File = FString::Printf(TEXT("strip-%s-%04d.jpg"), *St.Strip.Tag, Index);
		Ask.Of = St.Strip.Of;
		++Index;
		++St.StripAsks;
		// (frames asked and never read back, the window not drawn: the oldest let go, so the asks keep step with the frames)
		if (St.Asks.Num() >= 64)
		{
			St.Asks.RemoveAt(0);
			++St.Dropped;
		}
		St.Asks.Add(Ask);
		// (asked on the game thread before this frame's window is drawn, as the land battle's dark reading asks: the frame
		// read back is this one, a frame or two later)
		St.Grab->CaptureThisFrame(FFramePayloadPtr(new FCriticFramePayload(Ask.Seq)));
	}

	static void EnsureBegun(FCriticState& St)
	{
		if (St.bBegun)
		{
			return;
		}
		St.bBegun = true;
		const TCHAR* Cmd = FCommandLine::Get();
		St.Folder = FilmCritic::Folder();
		IFileManager::Get().MakeDirectory(*St.Folder, true);
		St.Path = FPaths::Combine(St.Folder, TEXT("critic-facts.jsonl"));
		// (a fresh file for this run)
		FFileHelper::SaveStringToFile(FString(), *St.Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString Value, Key;
		St.bSteps = FParse::Value(Cmd, TEXT("TheaterSteps="), Value, false);
		if (St.bSteps)
		{
			St.RunScene = "map";
			Key = FPaths::GetBaseFilename(Value);
		}
		else if (FParse::Value(Cmd, TEXT("TheaterAirBattle="), Value, false))
		{
			St.RunScene = "air";
			Key = Value;
		}
		else if (FParse::Value(Cmd, TEXT("TheaterLandBattle="), Value, false))
		{
			St.RunScene = "land";
			Key = Value;
		}
		else if (FParse::Value(Cmd, TEXT("TheaterSeaBattle="), Value, false))
		{
			St.RunScene = "sea";
			Key = Value;
		}
		else if (FParse::Value(Cmd, TEXT("TheaterLandScript="), Value, false))
		{
			St.RunScene = "land";
		}
		else
		{
			St.RunScene = "map";
		}
		St.bStrips = FilmCritic::StripAsked(St.StripFps, St.StripSeconds);
		double WindowW = 0.0, WindowH = 0.0;
		bool bWindow = ViewSize(WindowW, WindowH);
		FString Asked, AskedW, AskedH;
		if (!bWindow && FParse::Value(Cmd, TEXT("TheaterWindow="), Asked) && Asked.Split(TEXT("x"), &AskedW, &AskedH))
		{
			WindowW = static_cast<double>(FCString::Atoi(*AskedW));
			WindowH = static_cast<double>(FCString::Atoi(*AskedH));
			bWindow = WindowW > 0.0 && WindowH > 0.0;
		}
		const double T = NowT();
		FCriticLine L = NewLine("run", T, GFrameCounter, false, 0.0);
		L.Str("scene", St.RunScene).StrOrNull("key", Utf8(Key));
		if (bWindow)
		{
			L.Vec2("window", WindowW, WindowH, 0);
		}
		else
		{
			L.Nothing("window");
		}
		if (St.bStrips)
		{
			L.Vec2("strip", St.StripFps, St.StripSeconds, 3);
		}
		else
		{
			L.Nothing("strip");
		}
		L.Flag("steps", St.bSteps).Whole("v", 1);
		St.Buffer += L.Done();
		St.LastFlushT = T;
		FlushLines(St);
		UE_LOG(LogTheater, Display, TEXT("Film critic: facts to %s (a %s run%s)"), *St.Path, UTF8_TO_TCHAR(St.RunScene.c_str()), St.bStrips ? TEXT(", with film strips") : TEXT(""));
	}

	static double Ranked(const std::vector<double>& Sorted, double Share)
	{
		if (Sorted.empty())
		{
			return NoValue();
		}
		const double Want = std::ceil(Share * static_cast<double>(Sorted.size()));
		const size_t Index = Want < 1.0 ? 0 : std::min(Sorted.size() - 1, static_cast<size_t>(Want) - 1);
		return Sorted[Index];
	}

	static void WriteMen(FCriticState& St, const FLandBattleState& S, const FCriticLens& Lens, double UnitsPerMetre, double T, uint64 Frame)
	{
		const LandMen::Crowd& Folk = *S.Men;
		const double Life = S.Life > 0.0 ? S.Life : 1.0;
		int32 InSight = 0, Moving = 0, AllMoving = 0, Capped = 0;
		std::vector<double> Runs;
		std::set<std::pair<std::string, long>> Rates;
		std::set<std::string> Clips;
		for (int Man = 0; Man < Folk.Count(); ++Man)
		{
			if (!Folk.Visible(Man) || Folk.Fade(Man) < 0.5f)
			{
				continue;
			}
			const LandMen::Playing Now = Folk.PlayingOf(Man);
			// (life's metres a second: the battle's clock runs Life times life's)
			const double Pace = static_cast<double>(Now.Speed) / Life;
			const bool bMoving = Pace > 0.3;
			AllMoving += bMoving ? 1 : 0;
			double X = 0.0, Y = 0.0, Z = 0.0;
			float Facing = 0.0f;
			Folk.PlaceAsPosed(Man, X, Y, Z, Facing);
			if (!Lens.Sees(FVector(Y * UnitsPerMetre, X * UnitsPerMetre, (Z + 0.9) * UnitsPerMetre)))
			{
				continue;
			}
			++InSight;
			if (!bMoving)
			{
				continue;
			}
			++Moving;
			Capped += Now.bCapped ? 1 : 0;
			if (Now.bGait)
			{
				Rates.insert(std::make_pair(Now.Clip, std::lround(static_cast<double>(Now.Rate) / 0.05)));
				Clips.insert(Now.Clip);
				if (Pace >= 2.2)
				{
					Runs.push_back(Pace);
				}
			}
		}
		std::sort(Runs.begin(), Runs.end());
		std::vector<std::string> ClipList;
		for (const std::string& Each : Clips)
		{
			if (ClipList.size() >= 16)
			{
				break;
			}
			ClipList.push_back(Each);
		}
		FCriticLine L = NewLine("men", T, Frame, true, S.Battle);
		L.Whole("moving", Moving).Num("run_p10", Ranked(Runs, 0.1), 2).Num("run_p50", Ranked(Runs, 0.5), 2).Num("run_p90", Ranked(Runs, 0.9), 2)
			.Whole("rates", static_cast<long long>(Rates.size())).Whole("capped", Capped).Whole("insight", InSight).Whole("all_moving", AllMoving)
			.Whole("runners", static_cast<long long>(Runs.size())).Num("pace", S.SpeedNow, 2).Strs("clips", ClipList);
		AddLine(St, L.Done());
	}

	static void WriteVehicles(FCriticState& St, const FLandBattleState& S, const FCriticLens& Lens, double UnitsPerMetre, double T, uint64 Frame)
	{
		const LandKit::Battle& Kit = *S.Armour;
		const double Life = S.Life > 0.0 ? S.Life : 1.0;
		for (const LandKit::Vehicle& V : Kit.Vehicles())
		{
			if (V.Fade <= 0.0)
			{
				continue;
			}
			if (!Lens.Sees(FVector(V.Root.At.Y * UnitsPerMetre, V.Root.At.X * UnitsPerMetre, (V.Root.At.Z + 1.5) * UnitsPerMetre)))
			{
				continue;
			}
			const LandKit::Model& Shape = V.Look();
			double Heading = std::fmod(FMath::RadiansToDegrees(V.Heading), 360.0);
			Heading = Heading < 0.0 ? Heading + 360.0 : Heading;
			FCriticLine L = NewLine("vehicle", T, Frame, true, S.Battle);
			L.Whole("id", V.Index).Str("type", Shape.Name).Str("nation", V.Nation).Str("maker", Shape.Nation).Str("kind", Shape.Kind)
				.Num("heading", Heading, 2).Num("speed", V.Speed / Life, 2).Num("turret", V.TurretDeg, 2).Flag("moving", V.bMoving).Flag("dead", V.bDead);
			AddLine(St, L.Done());
			// (once a model and nation a run: its markings)
			const std::string Seen = Shape.Name + "|" + V.Nation + "|" + std::to_string(Kit.Year) + "-" + std::to_string(Kit.Month);
			if (St.MarksSeen.insert(Seen).second)
			{
				FCriticLine M = NewLine("marks", T, Frame, true, S.Battle);
				M.Str("kind", Shape.Kind).Str("type", Shape.Name).Str("nation", V.Nation).Whole("year", Kit.Year).Whole("month", Kit.Month)
					.Whole("marks", static_cast<long long>(Shape.Painted.size())).Flag("on", Kit.bMarks);
				AddLine(St, M.Done());
			}
		}
	}

	static void WriteLook(FCriticState& St, const FLandBattleState& S, double T, uint64 Frame)
	{
		const char* Word = S.Field->LookWord();
		const std::string LookName = Word ? std::string(Word) : std::string();
		// (the field as the game lays it from the recording, for its month and the trees' leaves)
		const LandField::Field Laid = LandSight::FieldOf(S.Rec);
		const bool bLeafless = LandScape::LeaflessOf(Laid, LandScape::LookOf(LookName));
		FCriticLine L = NewLine("look", T, Frame, true, S.Battle);
		L.StrOrNull("look", LookName).Whole("month", Laid.Month);
		if (S.Armour)
		{
			L.Whole("year", S.Armour->Year);
		}
		else
		{
			L.Nothing("year");
		}
		L.Num("lat", S.Rec.Weather.Lat, 3).Num("lon", S.Rec.Weather.Lon, 3).Num("snow", S.Field->SnowLying(), 3).Flag("leafless", bLeafless)
			.Num("hour", S.Rec.Weather.Hour, 2).StrOrNull("weather", S.Rec.WeatherWord).StrOrNull("terrain", Laid.Terrain).StrOrNull("date", S.Rec.Date);
		AddLine(St, L.Done());
	}

	static void WriteLandEnd(FCriticState& St, const FLandBattleState& S, double T, uint64 Frame)
	{
		const LandFeed::Over& Result = S.Rec.Result;
		FCriticLine L = NewLine("end", T, Frame, true, S.Battle);
		L.StrOrNull("how", Result.How).StrOrNull("broke", Result.Broke);
		// (the recording's over.seconds, the battle second its result came: for a side that broke, its break)
		if (Result.Broke.empty())
		{
			L.Nothing("broke_at");
		}
		else
		{
			L.Num("broke_at", Result.Seconds, 2);
		}
		L.Num("ended_at", S.Rec.End, 2).Num("card_at", T, 3).Nothing("offered_continue").StrOrNull("winner", Result.Winner).StrOrNull("level", Result.Level)
			.Num("result_s", Result.Seconds, 2);
		AddLine(St, L.Done());
	}
}

bool FilmCritic::On()
{
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("FilmCritic"));
	return bOn;
}

bool FilmCritic::StripAsked(double& OutFps, double& OutSeconds)
{
	FString Value, Fps, Seconds;
	if (!FParse::Value(FCommandLine::Get(), TEXT("FilmCriticStrip="), Value, false) || !Value.Split(TEXT(","), &Fps, &Seconds))
	{
		return false;
	}
	const double AskedFps = FCString::Atod(*Fps), AskedSeconds = FCString::Atod(*Seconds);
	if (!(AskedFps > 0.0) || !(AskedSeconds > 0.0))
	{
		return false;
	}
	OutFps = FMath::Min(AskedFps, 120.0);
	OutSeconds = FMath::Min(AskedSeconds, 600.0);
	return true;
}

FString FilmCritic::Folder()
{
	static const TCHAR* const Flags[] = { TEXT("TheaterShots="), TEXT("TheaterLandBattleShots="), TEXT("TheaterAirBattleShots="), TEXT("TheaterSeaBattleShots="), TEXT("TheaterLandScript=") };
	for (const TCHAR* Flag : Flags)
	{
		FString Value;
		if (FParse::Value(FCommandLine::Get(), Flag, Value, false) && !Value.IsEmpty())
		{
			return Value;
		}
	}
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FilmCritic"));
}

void FilmCritic::NoteCut()
{
	FilmCriticParts::FCriticState& St = FilmCriticParts::State();
	St.bCutNoted = true;
	St.CutFrame = GFrameCounter;
}

void FilmCritic::Frame(APlayerController& Player, float DeltaSeconds)
{
	using namespace FilmCriticParts;
	FCriticState& St = State();
	EnsureBegun(St);
	const double T = NowT();
	const uint64 Now = GFrameCounter;
	// What the last frame showed: the camera manager holds its view (taken after every actor's tick) until this frame's
	// own is taken at its end. A battle's frames are its own lines (Air, Land, Sea); these are the map's, and the stills
	// no battle camera took.
	const bool bBattleNear = St.LastBattleCam.bValid && St.LastBattleCam.Frame + 2 >= Now;
	if (St.bHaveLast && Player.PlayerCameraManager)
	{
		FCriticCam C;
		C.bValid = true;
		C.Frame = St.LastFrame;
		C.T = St.LastT;
		C.DtMs = St.LastDtMs;
		C.EyeUnits = Player.PlayerCameraManager->GetCameraLocation();
		C.Eye = C.EyeUnits / 100.0;
		C.Turn = Player.PlayerCameraManager->GetCameraRotation();
		C.Fov = static_cast<double>(Player.PlayerCameraManager->GetFOVAngle());
		C.bCut = St.bCutNoted && St.CutFrame == St.LastFrame;
		C.Scene = bBattleNear ? St.LastBattleCam.Scene : std::string("map");
		C.bBt = bBattleNear && St.LastBattleCam.bBt;
		C.Bt = St.LastBattleCam.Bt;
		TArray<FCriticStill> Waiting;
		for (int32 K = St.Stills.Num() - 1; K >= 0; --K)
		{
			if (St.Stills[K].Frame < Now)
			{
				Waiting.Insert(St.Stills[K], 0);
				St.Stills.RemoveAt(K);
			}
		}
		for (const FCriticStill& Picture : Waiting)
		{
			WriteStill(St, Picture, C);
			if (!bBattleNear)
			{
				WriteFleets(St, Player, C, Utf8(Picture.Name));
			}
		}
		if (St.RunScene == "map" && !bBattleNear)
		{
			const bool bMoved = !St.LastMapCam.bValid || !C.Eye.Equals(St.LastMapCam.Eye, 0.0005) || !C.Turn.Equals(St.LastMapCam.Turn, 0.0005) ||
				FMath::Abs(C.Fov - St.LastMapCam.Fov) > 0.0005;
			if (bMoved || T - St.LastMapLineT >= 1.0)
			{
				FCriticLine L = NewLine("cam", C.T, C.Frame, false, 0.0);
				CameraFields(L, C);
				AddLine(St, L.Done());
				St.LastMapCam = C;
				St.LastMapLineT = T;
			}
			if (T >= St.NextFleetT)
			{
				St.NextFleetT = T + 0.5;
				if (AnyFleetSailing(Player))
				{
					WriteFleets(St, Player, C, std::string("sail"));
				}
			}
		}
	}
	if (St.bStrips)
	{
		// (the grabber made a few seconds in, before the first strip is due, so making it does not fall in a strip)
		if (!St.bGrabTried && T > 5.0)
		{
			EnsureGrab(St);
		}
		StripFrame(St, T, Now);
	}
	St.bHaveLast = true;
	St.LastT = T;
	St.LastFrame = Now;
	St.LastDtMs = static_cast<double>(DeltaSeconds) * 1000.0;
	if (T - St.LastFlushT >= 1.0)
	{
		St.LastFlushT = T;
		FlushLines(St);
	}
}

void FilmCritic::Shutdown()
{
	using namespace FilmCriticParts;
	FCriticState& St = State();
	if (!St.bBegun)
	{
		return;
	}
	StopGrab(St);
	// (the strip frames still being written, a few seconds at most)
	const double Began = FPlatformTime::Seconds();
	while (St.Writing.load() > 0 && FPlatformTime::Seconds() - Began < 3.0)
	{
		FPlatformProcess::Sleep(0.01f);
	}
	FlushLines(St);
	St.LastFlushT = NowT();
}

void FilmCritic::Air(const AActor* Battle, const FAirBattleState& S, const ACameraActor& Eye, double UnitsPerMetre, float DeltaSeconds, const TCHAR* Overview)
{
	using namespace FilmCriticParts;
	FCriticState& St = State();
	EnsureBegun(St);
	const double T = NowT();
	const uint64 Now = GFrameCounter;
	const double U = UnitsPerMetre > 0.0 ? UnitsPerMetre : 100.0;
	FCriticBattle& Seen = BattleOf(St, Battle);
	FCriticCam C;
	C.bValid = true;
	FromEye(Eye, U, DeltaSeconds, C);
	C.Frame = Now;
	C.T = T;
	C.Scene = "air";
	C.bBt = true;
	C.Bt = S.BattleT;
	C.bCut = St.bCutNoted && St.CutFrame == Now;
	C.Curtain = S.Curtain;
	C.Bars = S.FlyBars;
	// The shot, as the scene's AimEye chose it this frame: the flyover's three shots by their clock, its glide, the close
	// view, the strategy view.
	int32 GroupShown = -1;
	if (S.bFlying && S.GlideT < 0.0)
	{
		const double RaidEnd = AirDesk::Flyover::RaidLong;
		const double TargetEnd = RaidEnd + AirDesk::Flyover::TargetLong;
		if (S.FlyT < RaidEnd)
		{
			C.Why = "flyover-raid";
			GroupShown = S.Fly.RaidGroup();
		}
		else if (S.FlyT < TargetEnd)
		{
			C.Why = "flyover-target";
			C.Subject = "target";
		}
		else
		{
			C.Why = "flyover-guard";
			GroupShown = S.Fly.GuardGroup();
		}
	}
	else if (S.bFlying)
	{
		C.Why = "glide";
	}
	else if (S.bClose)
	{
		C.Why = "close";
	}
	else
	{
		C.Why = Overview ? Utf8(FString(Overview)) : std::string("overview");
	}
	if (GroupShown >= 0)
	{
		C.Subject = "group " + std::to_string(GroupShown);
		const std::map<int, AirShow::Group>& Groups = S.Show.Groups();
		const auto Found = Groups.find(GroupShown);
		if (Found != Groups.end())
		{
			// (the group's own place, as the flyover's shot frames it: show metres X east, Y north, in Unreal's axes)
			C.bSubjectAt = true;
			C.SubjectAt = FVector(Found->second.Y, Found->second.X, Found->second.Alt);
		}
	}
	// The aircraft in frame, each side's (side 0 the raid's, 1 the defence's), and the ridden one's place.
	FCriticLens Lens;
	Lens.Set(C.EyeUnits, C.Turn, C.Fov, AspectNow());
	FCriticPlanes Tally;
	Tally.bValid = true;
	Tally.Frame = Now;
	const bool bRiding = C.Why == "close";
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		const FVector At(P.At.Y * U, P.At.X * U, P.At.Z * U);
		if (bRiding && P.Key == S.Subject.Plane)
		{
			C.Subject = "plane " + std::to_string(P.Key);
			C.bSubjectAt = true;
			C.SubjectAt = At / U;
		}
		if (!P.bShown || P.bOnGround || P.Side < 0 || P.Side > 1)
		{
			continue;
		}
		FCriticSide& Side = Tally.Sides[P.Side];
		const double Metres = FVector::Dist(At, C.EyeUnits) / U;
		if (!(Side.NearestAll <= Metres))
		{
			Side.NearestAll = Metres;
		}
		if (Lens.Sees(At))
		{
			++Side.InFrame;
			if (!(Side.Nearest <= Metres))
			{
				Side.Nearest = Metres;
			}
		}
	}
	St.Planes = Tally;
	FCriticLine L = NewLine("cam", T, Now, true, S.BattleT);
	CameraFields(L, C);
	AddLine(St, L.Done());
	BattleCamWritten(St, C);
	// (the aircraft each 0.25 s while the opening plays: the flyover and its glide, once the black has lifted)
	if (S.bFlying && !S.bWarming && T >= Seen.NextPlanes)
	{
		Seen.NextPlanes = T + 0.25;
		WritePlanes(St, C, std::string());
	}
	// (the opening's strip, a battle scene run's: from the first frame drawn once the black has lifted)
	if (!Seen.bOpening && !S.bWarming && S.bFlyTried)
	{
		Seen.bOpening = true;
		if (!St.bSteps)
		{
			StartStrip(St, TEXT("opening"), TEXT("opening"), false);
		}
	}
	if (!Seen.bEnd && S.bCard)
	{
		Seen.bEnd = true;
		FCriticLine E = NewLine("end", T, Now, true, S.BattleT);
		E.StrOrNull("how", S.Card.Level).Nothing("broke_at").Num("ended_at", S.BattleT, 2).Num("card_at", T, 3).Nothing("offered_continue")
			.StrOrNull("title", S.Card.Title);
		AddLine(St, E.Done());
	}
}

void FilmCritic::Land(const AActor* Battle, const FLandBattleState& S, const ACameraActor& Eye, double UnitsPerMetre, float DeltaSeconds)
{
	using namespace FilmCriticParts;
	FCriticState& St = State();
	EnsureBegun(St);
	const double T = NowT();
	const uint64 Now = GFrameCounter;
	const double U = UnitsPerMetre > 0.0 ? UnitsPerMetre : 100.0;
	FCriticBattle& Seen = BattleOf(St, Battle);
	FCriticCam C;
	C.bValid = true;
	FromEye(Eye, U, DeltaSeconds, C);
	C.Frame = Now;
	C.T = T;
	C.Scene = "land";
	C.bBt = true;
	C.Bt = S.Battle;
	C.bCut = S.Pose.bCut;
	const bool bNight = S.NightNow.bNight;
	if (bNight)
	{
		C.Why = "night";
	}
	else if (S.General.bOn && !S.General.bFilm)
	{
		C.Why = "general";
	}
	else
	{
		C.Why = LandEye::NameOf(S.Pose.Shot);
		if (S.Pose.Sp != LandEye::Special::None)
		{
			C.Special = LandEye::NameOf(S.Pose.Sp);
		}
		// (the shot's subject: a recording's track, where it stands now)
		const int Subject = S.Pose.Subject;
		if (Subject >= 0 && Subject < static_cast<int>(S.Rec.Tracks.size()))
		{
			const LandFeed::Track& Followed = S.Rec.Tracks[static_cast<size_t>(Subject)];
			C.Subject = "track " + std::to_string(Subject);
			if (S.EyeWorld.Height)
			{
				const LandFeed::Spot Here = LandFeed::SpotAt(Followed, S.Battle);
				const double Ground = S.EyeWorld.Height(Here.X, Here.Y);
				C.bSubjectAt = true;
				C.SubjectAt = FVector(Here.Y, Here.X, Ground + (Followed.IsMan() ? 1.0 : 1.5));
			}
		}
	}
	if (C.bCut)
	{
		C.Left = LandEye::NameOf(S.Eye.LastWhy());
	}
	FCriticLine L = NewLine("cam", T, Now, true, S.Battle);
	CameraFields(L, C);
	AddLine(St, L.Done());
	BattleCamWritten(St, C);
	FCriticLens Lens;
	Lens.Set(C.EyeUnits, C.Turn, C.Fov, AspectNow());
	if (S.Men && T >= Seen.NextMen)
	{
		Seen.NextMen = T + 1.0;
		WriteMen(St, S, Lens, U, T, Now);
	}
	if (S.Armour && T >= Seen.NextVehicles)
	{
		Seen.NextVehicles = T + 0.5;
		WriteVehicles(St, S, Lens, U, T, Now);
	}
	if (!Seen.bLook && S.Field)
	{
		Seen.bLook = true;
		WriteLook(St, S, T, Now);
	}
	// (the opening's strip, a battle scene run's: from the first frame of the battle's own film, after any night)
	if (!Seen.bOpening && !bNight)
	{
		Seen.bOpening = true;
		if (!St.bSteps)
		{
			StartStrip(St, TEXT("opening"), TEXT("opening"), false);
		}
	}
	if (!Seen.bEnd && S.bCard)
	{
		Seen.bEnd = true;
		WriteLandEnd(St, S, T, Now);
	}
}

void FilmCritic::Sea(const AActor* Battle, const ACameraActor& Eye, double UnitsPerMetre, float DeltaSeconds, double BattleSeconds, bool bCut, const FString& Shot, int32 Subject)
{
	using namespace FilmCriticParts;
	FCriticState& St = State();
	EnsureBegun(St);
	const double T = NowT();
	const uint64 Now = GFrameCounter;
	const double U = UnitsPerMetre > 0.0 ? UnitsPerMetre : 100.0;
	FCriticBattle& Seen = BattleOf(St, Battle);
	FCriticCam C;
	C.bValid = true;
	FromEye(Eye, U, DeltaSeconds, C);
	C.Frame = Now;
	C.T = T;
	C.Scene = "sea";
	C.bBt = true;
	C.Bt = BattleSeconds;
	C.bCut = bCut;
	C.Why = Shot.IsEmpty() ? std::string("overview") : Utf8(Shot);
	if (Subject >= 0)
	{
		C.Subject = "ship " + std::to_string(Subject);
	}
	FCriticLine L = NewLine("cam", T, Now, true, BattleSeconds);
	CameraFields(L, C);
	AddLine(St, L.Done());
	BattleCamWritten(St, C);
	if (!Seen.bOpening)
	{
		Seen.bOpening = true;
		if (!St.bSteps)
		{
			StartStrip(St, TEXT("opening"), TEXT("opening"), false);
		}
	}
}

void FilmCritic::Still(const FString& Name)
{
	using namespace FilmCriticParts;
	FCriticState& St = State();
	EnsureBegun(St);
	FCriticStill Asked;
	Asked.Name = Name;
	Asked.File = FPaths::GetCleanFilename(FScreenshotRequest::GetFilename());
	if (Asked.File.IsEmpty())
	{
		Asked.File = Name + TEXT(".png");
	}
	Asked.Frame = GFrameCounter;
	// (asked after this frame's battle camera was written: that camera is the picture's; else the next camera written,
	// a battle's later this frame or the camera manager's at the next frame)
	if (St.LastBattleCam.bValid && St.LastBattleCam.Frame == Asked.Frame)
	{
		WriteStill(St, Asked, St.LastBattleCam);
	}
	else
	{
		St.Stills.Add(Asked);
	}
	if (!St.bSteps)
	{
		// (a battle scene run: a strip after each named still)
		StartStrip(St, Name, Name, false);
	}
	else if (St.Strip.bOn && St.Strip.bUntilStill && St.Strip.Of == Name)
	{
		// (a step run: a wait step's strip ends with the wait, at its step's picture)
		St.Strip.bOn = false;
	}
}

void FilmCritic::StepActed(int32 Number, const FString& Verb, bool bReplay)
{
	using namespace FilmCriticParts;
	FCriticState& St = State();
	EnsureBegun(St);
	if (bReplay || !St.bStrips)
	{
		return;
	}
	const bool bWait = Verb == TEXT("air-wait") || Verb == TEXT("land-wait");
	if (!bWait && Verb != TEXT("press") && Verb != TEXT("order-sea"))
	{
		return;
	}
	StartStrip(St, FString::Printf(TEXT("%03d"), Number), FString::Printf(TEXT("step-%03d"), Number), bWait);
}
