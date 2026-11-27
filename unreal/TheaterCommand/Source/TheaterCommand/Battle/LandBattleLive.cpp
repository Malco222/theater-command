// The ground battle fought live (release 2; Brian, 22 November 2026: "i want the ground battles to be playable. im the
// general who controls their deployment and who to attack and move on a map with tactics"; "i also want the game play to
// be smooth, fun, and realistic").
//
// ALandBattle's part that feeds the scene from the rules' own thread (LandLive). The scene is the one that plays a
// recording: here the recording only grows. The rules open the battle and hand over its header (the recording with
// nothing fought yet), and the scene opens on it as on a whole one; then each frame the pieces they have fought are laid
// into the recording (LandFeed::AppendDelta) and into each core that keeps its own copy (the fire, the men, the tanks,
// the camera), and the battle's clock is held from ever passing what the rules have decided. The general's orders go the
// other way, to the rules' thread, which gives them between its steps.
//
// Like a printing house a page ahead of its reader: the screen only ever reads pages that are printed, a little behind
// the press, and if the press is slow the reader waits on the last line printed rather than guessing the next.

#include "Battle/LandBattle.h"

#include "Battle/LandBattleState.h"
#include "Battle/LandFell.h"
#include "Battle/LandLive.h"
#include "Battle/LandMen.h"
#include "Battle/LandVat.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "TheaterCommand.h"

#include <string>
#include <utility>
#include <vector>

namespace
{
	FString WideOf(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	std::string NarrowOf(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		return std::string(Utf8.Get(), static_cast<size_t>(Utf8.Length()));
	}

	/** The screen's middle note is said in capitals, as every other. */
	std::string NoteOf(const FString& Text)
	{
		// ("block" never goes on the screen: units, the UI director's word, 22 November 18:26Z)
		return NarrowOf(Text.ToUpper().Replace(TEXT("BLOCKS"), TEXT("UNITS"), ESearchCase::CaseSensitive).Replace(TEXT("BLOCK"), TEXT("UNIT"), ESearchCase::CaseSensitive));
	}

	/** How far behind what the rules have fought the screen may come, battle seconds: a piece is up to half a second,
	 * the men sample up to 0.2 s behind the frame, and a moving man's last point is provisional until the next piece. */
	constexpr double LiveMargin = 1.0;

	/** How far ahead of the screen the rules are kept, battle seconds at life's pace and more at a higher speed (the
	 * camera thread, 22 November: its cut on action wants 2.2 real seconds of lead and a tank's knock-out 5.2: "2.5 battle
	 * s at 1x, 5 at 2x, 10 at 4x"). */
	constexpr double LiveLead = 2.5;

	/** Milliseconds of one frame the pieces may take while the screen waits on the rules (PumpLive): past it, the rest wait
	 * for the next frame, so a backlog of pieces is laid over a few frames and never holds the picture. */
	constexpr double LivePumpMs = 12.0;

	/** The campaign's own land battles fought live from their tickets (ALandBattle::CampaignLiveOn): on (Brian, 22 November 21:21Z). */
	TAutoConsoleVariable<int32> CampaignLiveSwitch(TEXT("TheaterLandCampaignLive"), 1,
		TEXT("1: the attack card offers FIGHT IT YOURSELF, the campaign's land battle fought live in the land rules from its ticket (release 2). 0: SEND THEM IN only."));
}

bool ALandBattle::WantsLive(FString& OutKey)
{
	return FParse::Value(FCommandLine::Get(), TEXT("TheaterLandLive="), OutKey) && !OutKey.IsEmpty();
}

void ALandBattle::LiveWhenReady(const FString& InKey)
{
	FString Side = TEXT("att");
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandSide="), Side);
	Side = Side.ToLower();
	LiveMe = Side == TEXT("def") ? TEXT("def") : Side == TEXT("none") || Side == TEXT("film") ? FString() : TEXT("att");
	LiveSeed = 0;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandSeed="), LiveSeed);
	bLiveDeploy = !LiveMe.IsEmpty() && !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoDeploy"));
	PendingLive = InKey;
	SetActorTickEnabled(true);
}

void ALandBattle::LiveWhenReady(const FString& InKey, const FString& Me, int32 Seed, bool bDeploy)
{
	LiveMe = Me == TEXT("def") ? TEXT("def") : Me == TEXT("att") ? TEXT("att") : FString();
	LiveSeed = Seed;
	bLiveDeploy = bDeploy && !LiveMe.IsEmpty();
	PendingLive = InKey;
	SetActorTickEnabled(true);
}

bool ALandBattle::WantsLiveTicket(FString& OutLookJson)
{
	FString File;
	if (!FParse::Value(FCommandLine::Get(), TEXT("TheaterLandTicket="), File) || File.IsEmpty())
	{
		return false;
	}
	if (!FFileHelper::LoadFileToString(OutLookJson, *File) || !OutLookJson.TrimStart().StartsWith(TEXT("{")))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle (live): -TheaterLandTicket=%s is not a ticket (JSON) this game can read"), *File);
		OutLookJson.Empty();
		return false;
	}
	return true;
}

bool ALandBattle::CampaignLiveOn()
{
	return CampaignLiveSwitch.GetValueOnGameThread() != 0 && !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoCampaignLive"));
}

void ALandBattle::LiveTicketWhenReady(const FString& LookJson)
{
	// (its side and setup by the set piece's own switches)
	LiveWhenReady(TEXT("field"));
	PendingTicket = LookJson;
}

void ALandBattle::LiveTicketWhenReady(const FString& LookJson, const FString& Me, bool bDeploy)
{
	LiveWhenReady(TEXT("field"), Me, 0, bDeploy);
	PendingTicket = LookJson;
}

bool ALandBattle::LiveResult(FString& OutJson) const
{
	return Live.IsValid() && Live->Result(OutJson);
}

bool ALandBattle::BeginLive(const FString& InKey, int32 Seed, const FString& Me, bool bDeploy)
{
	return BeginLiveFrom(InKey, Seed, Me, bDeploy, FString());
}

bool ALandBattle::BeginLiveTicket(const FString& LookJson, const FString& Me, bool bDeploy)
{
	return BeginLiveFrom(TEXT("field"), 0, Me, bDeploy, LookJson);
}

bool ALandBattle::BeginLiveFrom(const FString& InKey, int32 Seed, const FString& Me, bool bDeploy, const FString& LookJson)
{
	if (Live.IsValid() || State.IsValid())
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle (live): %s not begun: a battle is already open in this scene"), *InKey);
		return false;
	}
	Key = InKey;
	LiveMe = Me;
	bLiveDeploy = bDeploy && !Me.IsEmpty();
	TSharedPtr<FLandLive> Made = MakeShared<FLandLive>();
	if (LookJson.IsEmpty() ? !Made->Start(InKey, Seed, Me, bLiveDeploy) : !Made->StartTicket(LookJson, Me, bLiveDeploy))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle (live): %s not begun: the rules' thread could not start"), *InKey);
		return false;
	}
	Live = Made;
	// (the general's walk pressed in code for a builder with no hand on the mouse: LandBattleCommand.cpp)
	ScriptFolder.Empty();
	ScriptStep = 0;
	ScriptAt = RealT;
	ScriptUsed.Reset();
	ScriptTaken.Reset();
	if (!Me.IsEmpty() && FParse::Value(FCommandLine::Get(), TEXT("TheaterLandScript="), ScriptFolder) && !ScriptFolder.IsEmpty())
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle (script): his steps will be pressed in code, stills into %s"), *ScriptFolder);
	}
	// (the lorry pan, the Mac's stills of the men in lorries: "-TheaterLandLorryPan=<battle s>[,<seconds>[,<block>]]",
	// read past its commas; LorryPan in LandBattle.cpp)
	LorryPanAt = -1.0;
	LorryPanVehicle = -1;
	LorryPanStills = 0;
	LorryPanTries = 12;
	FString LorryPanAsked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandLorryPan="), LorryPanAsked, false))
	{
		TArray<FString> Parts;
		LorryPanAsked.ParseIntoArray(Parts, TEXT(","), false);
		LorryPanAt = Parts.Num() > 0 && !Parts[0].TrimStartAndEnd().IsEmpty() ? FMath::Max(0.0, FCString::Atod(*Parts[0])) : 80.0;
		if (Parts.Num() > 1 && !Parts[1].TrimStartAndEnd().IsEmpty())
		{
			LorryPanFor = FMath::Clamp(FCString::Atod(*Parts[1]), 4.0, 180.0);
		}
		if (Parts.Num() > 2 && !Parts[2].TrimStartAndEnd().IsEmpty())
		{
			LorryPanBlock = FCString::Atoi(*Parts[2]);
		}
		UE_LOG(LogTheater, Display, TEXT("Land battle (live): the lorry pan asked: from %.0f s for %.0f s, beside a lorry with men aboard%s"),
			LorryPanAt, LorryPanFor, LorryPanBlock >= 0 ? *FString::Printf(TEXT(" of block %d"), LorryPanBlock) : TEXT(""));
	}
	// The crash locator (LandFell): on for the walk, or by -TheaterLandFell=<folder> for any live battle; a crash then
	// leaves fell-over.txt (where it fell, its stack as library and offset) and fell-over-ring.txt (the frame's last
	// parts, kept as it goes) in that folder.
	FString FellFolder = ScriptFolder;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterLandFell="), FellFolder);
	if (!FellFolder.IsEmpty() && !LandFell::Armed())
	{
		IFileManager::Get().MakeDirectory(*FellFolder, true);
		const FString Report = FPaths::ConvertRelativePathToFull(FPaths::Combine(FellFolder, TEXT("fell-over.txt")));
		const FString Ring = FPaths::ConvertRelativePathToFull(FPaths::Combine(FellFolder, TEXT("fell-over-ring.txt")));
		const bool bOn = LandFell::Arm(TCHAR_TO_UTF8(*Report), TCHAR_TO_UTF8(*Ring));
		UE_LOG(LogTheater, Display, TEXT("Land battle (live): the crash locator %s: %s"), bOn ? TEXT("on") : TEXT("not on this machine"), *Report);
	}
	bLiveWaiting = true;
	bLiveBroken = false;
	LiveBrokenAt = -1.0;
	LiveWaited = 0.0;
	LiveHeld = 0.0;
	SetActorTickEnabled(true);
	UE_LOG(LogTheater, Display, TEXT("Land battle (live): %s: the rules are opening it, the player %s%s"), *InKey,
		Me.IsEmpty() ? TEXT("watching") : *Me, bLiveDeploy ? TEXT(", deploying first") : TEXT(""));
	return true;
}

bool ALandBattle::TickLiveOpening(float DeltaSeconds)
{
	if (!PendingLive.IsEmpty())
	{
		// (two seconds in, as a recording: the table and its light are up, so that they are put away after)
		if (GetWorld() && GetWorld()->GetTimeSeconds() > 2.0)
		{
			const FString Asked = PendingLive, Look = PendingTicket;
			PendingLive.Empty();
			PendingTicket.Empty();
			if (!BeginLiveFrom(Asked, LiveSeed, LiveMe, bLiveDeploy, Look))
			{
				LiveWayOut(TEXT("its rules could not be started"));
			}
		}
		return true;
	}
	if (!bLiveWaiting || !Live.IsValid())
	{
		return false;
	}
	LiveWaited += FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, 0.1);
	FString Why;
	if (Live->Failed(Why) || LiveWaited > 90.0)
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle (live): %s did not open: %s"), *Key, Why.IsEmpty() ? TEXT("the rules did not answer in 90 s") : *Why);
		bLiveWaiting = false;
		CloseLive();
		LiveWayOut(TEXT("it did not open"));
		return true;
	}
	std::string Header;
	if (!Live->TakeHeader(Header))
	{
		return true;
	}
	bLiveWaiting = false;
	const double Began = FPlatformTime::Seconds();
	State = MakeShared<FLandBattleState>();
	FLandBattleState& S = *State;
	S.RecText = std::move(Header);
	std::string Error;
	if (!LandFeed::ReadRecording(S.RecText, S.Rec, Error) || !S.Rec.bLive)
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle (live): %s: the rules' header is unreadable: %s"), *Key, Error.empty() ? TEXT("not a live battle") : *WideOf(Error));
		State.Reset();
		CloseLive();
		LiveWayOut(TEXT("its header could not be read"));
		return true;
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle (live): %s header read in %.0f ms: \"%s\", %s, %d blocks%s"), *Key, (FPlatformTime::Seconds() - Began) * 1000.0,
		*WideOf(S.Rec.Title), *WideOf(S.Rec.Date), static_cast<int32>(S.Rec.Blocks.size()), S.Rec.bSetup ? TEXT(", the setup step open") : TEXT(""));
	if (!Open())
	{
		State.Reset();
		CloseLive();
		LiveWayOut(TEXT("its field could not be laid"));
		return true;
	}
	// (the general sets his own pace, at life's to begin with, the keys 1 to 3 faster; a film keeps the scene's own,
	// fast until the armies are about to meet)
	if (S.Rec.Me >= 0)
	{
		S.bOwnSpeed = false;
		S.Speed = 1.0;
	}
	// (his clipboard: the blocks' names and his side, from the header)
	S.General.Clipboard.Begin(S.Rec);
	// The field as laid: its hedge lines and gateways to the rules before the clock runs (the historian's check, 22
	// November: a block on a hedge line hides as in woods, and the men on foot go through the gates). The wood's edges
	// are the woods' own, and the far strips lie off the field.
	{
		static const TCHAR* const KindWord[] = { TEXT("hedgerow"), TEXT("bank"), nullptr, TEXT("garden"), TEXT("wall"), nullptr };
		FString Hedges = TEXT("["), Gates = TEXT("[");
		int32 Lines = 0, Ways = 0;
		if (S.Field)
		{
			for (const LandField::HedgeLine& H : S.Field->Hedges())
			{
				const TCHAR* Word = H.Kind >= 0 && H.Kind < 6 ? KindWord[H.Kind] : nullptr;
				if (!Word || H.Points.size() < 2) continue;
				Hedges += FString::Printf(TEXT("%s{\"kind\":\"%s\",\"points\":["), Lines ? TEXT(",") : TEXT(""), Word);
				for (size_t P = 0; P < H.Points.size(); ++P)
				{
					Hedges += FString::Printf(TEXT("%s[%.1f,%.1f]"), P ? TEXT(",") : TEXT(""), H.Points[P].first, H.Points[P].second);
				}
				Hedges += TEXT("]}");
				++Lines;
			}
			for (const LandField::HedgeOpening& O : S.Field->Openings())
			{
				if (O.Kind != 0) continue;
				Gates += FString::Printf(TEXT("%s{\"x\":%.1f,\"y\":%.1f,\"yaw\":%.4f,\"width\":%.1f,\"kind\":\"gate\"}"), Ways ? TEXT(",") : TEXT(""), O.X, O.Y, O.Yaw, O.Width);
				++Ways;
			}
		}
		Hedges += TEXT("]");
		Gates += TEXT("]");
		Live->Field(Hedges, Gates);
		UE_LOG(LogTheater, Display, TEXT("Land battle (live): %d hedge lines and %d gateways handed to the rules (%d KB)"), Lines, Ways, (Hedges.Len() + Gates.Len()) / 1024);
	}
	return true;
}

void ALandBattle::PumpLive()
{
	if (!Live.IsValid() || !State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;

	// The rules' answers to the general: a refusal said in the middle of the screen, in the rules' own plain words.
	TArray<FLandLive::FAnswer> Said;
	Live->Answers(Said);
	for (const FLandLive::FAnswer& A : Said)
	{
		// (the field's hedges are logged by the rules' thread with their count: "the field's hedges handed to the rules")
		if (A.Kind != TEXT("hedges"))
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle (live): %s %s %s%s%s"), *A.Kind, *A.Asked, A.bOk ? TEXT("taken") : TEXT("refused"),
				A.Why.IsEmpty() ? TEXT("") : TEXT(": "), *A.Why);
		}
		// (ALL FALL BACK refused by the rules: the button comes back, and the note says why)
		if (!A.bOk && A.Kind == TEXT("order") && A.Asked.Contains(TEXT("land:fallBack")))
		{
			S.General.bAllBack = false;
		}
		if (!A.bOk && A.Kind != TEXT("hedges"))
		{
			S.Note = NoteOf(!A.Why.IsEmpty() ? A.Why : A.Kind == TEXT("place") ? FString(TEXT("The block cannot stand there")) : FString(TEXT("The order was not taken")));
			S.NoteAge = 0.0;
		}
	}
	FString Why;
	if (!bLiveBroken && Live->Failed(Why))
	{
		// (the rules stopped: the battle shown up to what they fought, and then held; the log says why)
		bLiveBroken = true;
		LiveBrokenAt = RealT;
		S.Note = "THE BATTLE STOPPED HERE: ITS RULES FAILED. NOTHING IS BOOKED; BACK TO THE MAP IN A MOMENT";
		S.NoteAge = -1.0;
	}

	// The pieces fought: one a frame (each some milliseconds to read and lay), and more when the screen waits on them (the
	// opening, the setup step's placings, the rules catching up), up to LivePumpMs of the frame: the rest the next frame.
	// (release 1.5, the Mac's speed gate on 2dcc018b, 23 November: every live battle held its picture for 200 to 550 ms
	// every few seconds; a frame that took every waiting piece at once was one way. "-TheaterLandPumpAll": every waiting
	// piece in one frame, as before)
	static const bool bPumpAll = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandPumpAll"));
	const bool bWaitedOn = !S.Rec.bOver && S.Battle >= S.Rec.Fought - LiveMargin - 1e-6;
	const double PumpFrom = FPlatformTime::Seconds();
	std::string Delta;
	int32 Taken = 0;
	while ((Taken == 0 || (bWaitedOn && (bPumpAll || (FPlatformTime::Seconds() - PumpFrom) * 1000.0 < LivePumpMs))) && Live->TakeOne(Delta))
	{
		++Taken;
		// (release 1.5, the Mac's speed gate on 2dcc018b, 23 November: on every battlefield fought live a piece took 200 to
		// 550 ms of one frame, the picture held every few seconds, where the cloud's reading of a whole piece was some 40 ms.
		// Each step of a piece timed, and a piece over 50 ms said in the log with its parts, so the Mac's next run names the
		// step to mend. Timing only: nothing here changes what is laid)
		const double PieceAt = FPlatformTime::Seconds();
		double StepAt = PieceAt;
		double MsRead = 0.0, MsPlay = 0.0, MsMen = 0.0, MsCrowd = 0.0, MsTanks = 0.0, MsMap = 0.0;
		auto Lap = [&StepAt](double& Ms)
		{
			const double Now = FPlatformTime::Seconds();
			Ms += (Now - StepAt) * 1000.0;
			StepAt = Now;
		};
		const int32 PieceKb = static_cast<int32>(Delta.size() / 1024);
		const int32 VehiclesBefore = S.Armour ? static_cast<int32>(S.Armour->Vehicles().size()) : 0;
		LandFeed::Grown G;
		std::string Error;
		LandFell::Phase("live: a piece read", Taken);
		if (!LandFeed::AppendDelta(Delta, S.Rec, G, Error))
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle (live): a piece at %.1f s not read: %s"), S.Rec.Fought, *WideOf(Error));
			continue;
		}
		Lap(MsRead);
		S.Play.Extend(G);
		Lap(MsPlay);
		LandFell::Phase("live: the men grown", G.bReset ? 1 : 0);
		if (S.Men && !S.Men->Extend(Delta, Error))
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle (live): the men not grown at %.1f s: %s"), G.T, *WideOf(Error));
		}
		Lap(MsMen);
		// (the men's crowd, SO1: the body sets and helmets these men bring made now, before they are drawn; a set whose mid
		// part cannot be made has every mid man skinned, as MakeMen does)
		if (Vat && S.Men)
		{
			LandFell::Phase("live: the men's crowd grown");
			Vat->Grow(*S.Men, G.bReset);
			if (Vat->MidMissing())
			{
				S.Men->SetPoseBankMid(false);
			}
		}
		Lap(MsCrowd);
		// (a reset made her vehicles again: kept as they are drawn if they are the same vehicles in the same order)
		std::vector<LandKit::TrackWords> WordsBefore;
		if (G.bReset && S.Armour)
		{
			for (const LandKit::Vehicle& V : S.Armour->Vehicles())
			{
				WordsBefore.push_back(V.Words());
			}
		}
		LandFell::Phase("live: the tanks grown", G.bReset ? 1 : 0);
		if (S.Armour && !S.Armour->Extend(Delta, Error))
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle (live): the tanks not grown at %.1f s: %s"), G.T, *WideOf(Error));
		}
		bool bVehiclesKept = true;
		if (G.bReset && S.Armour)
		{
			const std::vector<LandKit::Vehicle>& Now = S.Armour->Vehicles();
			bVehiclesKept = Now.size() == WordsBefore.size();
			for (size_t V = 0; bVehiclesKept && V < Now.size(); ++V)
			{
				const LandKit::TrackWords& W = Now[V].Words();
				bVehiclesKept = W.Block == WordsBefore[V].Block && W.K == WordsBefore[V].K && W.J == WordsBefore[V].J;
			}
		}
		Lap(MsTanks);
		LandFell::Phase("live: mapped to the drawn", bVehiclesKept ? 0 : 1);
		MapLive(static_cast<int32>(G.NewTracksFrom), VehiclesBefore, G.bReset, bVehiclesKept);
		// (the old wrecks, release 2: any one this piece's men or machines are to come near cleared away before they are
		// drawn there; nothing where none was laid)
		GuardOldWrecks(G.bReset);
		Lap(MsMap);
		const double PieceMs = (StepAt - PieceAt) * 1000.0;
		if (PieceMs > 50.0)
		{
			const int32 VehiclesNow = S.Armour ? static_cast<int32>(S.Armour->Vehicles().size()) : 0;
			UE_LOG(LogTheater, Display, TEXT("Land battle (live): a slow piece, %.0f ms at %.1f s (%d KB%s, piece %d of this frame; %d men, %d tracks, %d vehicles, %d new): read %.0f, play %.0f, men %.0f, crowd %.0f, tanks %.0f, mapped and bodies made %.0f"),
				PieceMs, G.T, PieceKb, G.bReset ? (bVehiclesKept ? TEXT(", a reset, vehicles kept") : TEXT(", a reset, vehicles made again")) : TEXT(""), Taken,
				S.Men ? S.Men->Count() : 0, static_cast<int32>(S.Rec.Tracks.size()), VehiclesNow, FMath::Max(0, VehiclesNow - VehiclesBefore),
				MsRead, MsPlay, MsMen, MsCrowd, MsTanks, MsMap);
		}
		if (G.bBegun)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle (live): the setup step closed; the clock runs"));
		}
		if (G.bOver)
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle (live): over at %.1f s; the rules' slowest piece %.0f ms"), S.Rec.End, Live->MostMs());
			// (the result as the campaign takes it back, TC.battleSettleLand: in the log for a campaign battle's test run)
			FString Result;
			if (Live->Result(Result))
			{
				UE_LOG(LogTheater, Display, TEXT("Land battle (live): the result for the campaign (%d bytes): %s"), Result.Len(), *Result.Left(400));
			}
		}
	}
	// (several pieces in one frame, the screen waiting on them: said with their count when they took over 50 ms together,
	// so the Mac's run tells a backlog from one heavy piece)
	const double FrameMs = (FPlatformTime::Seconds() - PumpFrom) * 1000.0;
	if (Taken > 1 && FrameMs > 50.0)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle (live): %d pieces in one frame, %.0f ms (the screen waiting on the rules at %.1f s, %.1f s fought)%s"),
			Taken, FrameMs, S.Battle, S.Rec.Fought, bPumpAll ? TEXT(", every waiting piece taken: -TheaterLandPumpAll") : TEXT(""));
	}
}

void ALandBattle::MapLive(int32 TracksFrom, int32 VehiclesFrom, bool bReset, bool bVehiclesKept)
{
	FLandBattleState& S = *State;
	const size_t Tracks = S.Rec.Tracks.size();
	const size_t From = bReset ? 0 : static_cast<size_t>(FMath::Max(0, TracksFrom));
	if (bReset)
	{
		S.ManOf.assign(Tracks, -1);
		S.VehicleOf.assign(Tracks, -1);
	}
	else
	{
		S.ManOf.resize(Tracks, -1);
		S.VehicleOf.resize(Tracks, -1);
	}

	// The men: each new track's man in the crowd, and back.
	if (S.Men)
	{
		const int Count = S.Men->Count();
		if (bReset)
		{
			S.TrackOfMan.assign(static_cast<size_t>(Count), -1);
		}
		else
		{
			S.TrackOfMan.resize(static_cast<size_t>(Count), -1);
		}
		for (size_t K = From; K < Tracks; ++K)
		{
			const LandFeed::Track& T = S.Rec.Tracks[K];
			if (!T.IsMan())
			{
				continue;
			}
			const int Man = S.Men->Find(T.Block, T.K, T.J);
			S.ManOf[K] = Man;
			if (Man >= 0 && Man < Count)
			{
				S.TrackOfMan[static_cast<size_t>(Man)] = static_cast<int>(K);
			}
		}
		// (placed again: every man drawn afresh, at his level, the next time he is in the picture)
		if (bReset)
		{
			for (FManDrawn& D : MenDrawn)
			{
				D.Lod = -1;
				D.Stamp = 0xFFFFFFFFu;
				if (D.Mesh.IsValid())
				{
					D.Mesh->SetVisibility(false);
				}
			}
		}
		MenDrawn.SetNum(FMath::Max(MenDrawn.Num(), Count));
	}

	// The vehicles: each new one's track and bodies; after a reset that changed them, every body made again.
	if (S.Armour)
	{
		const std::vector<LandKit::Vehicle>& All = S.Armour->Vehicles();
		int32 NewFrom = bReset ? static_cast<int32>(All.size()) : FMath::Max(0, VehiclesFrom);
		if (bReset && !bVehiclesKept)
		{
			for (FBodyDrawn& D : BodiesDrawn)
			{
				if (UProceduralMeshComponent* Mesh = D.Mesh.Get())
				{
					BodyMeshes.Remove(Mesh);
					Mesh->DestroyComponent();
				}
			}
			BodiesDrawn.Reset();
			++BodyRound;
			NewFrom = 0;
		}
		if (bReset)
		{
			S.TrackOfVehicle.assign(All.size(), -1);
		}
		else
		{
			S.TrackOfVehicle.resize(All.size(), -1);
		}
		for (size_t V = bReset ? 0 : static_cast<size_t>(NewFrom); V < All.size(); ++V)
		{
			const LandKit::TrackWords& W = All[V].Words();
			const int Track = S.Rec.TrackIndexOf(W.Block, W.K, W.J);
			S.TrackOfVehicle[V] = Track;
			if (Track >= 0 && static_cast<size_t>(Track) < Tracks)
			{
				S.VehicleOf[static_cast<size_t>(Track)] = static_cast<int>(V);
			}
		}
		if (NewFrom < static_cast<int32>(All.size()))
		{
			MakeBodies(NewFrom);
		}
		// The riders: after a reset the crowd's men were all made again, its riders gone with them, so the tank
		// commanders and the light cars' and motorcycles' men are seated afresh (never a stale number posing another
		// man); else the new light cars' and motorcycles' men.
		if (bReset)
		{
			S.Seated.clear();
			S.SeatedDone.clear();
			MakeRiders();
			SeatLight(0);
		}
		else
		{
			SeatLight(FMath::Max(0, VehiclesFrom));
		}
	}
}

double ALandBattle::LiveClock(double From, double To, double Real)
{
	FLandBattleState& S = *State;
	if (!S.Rec.bOver)
	{
		// Never past what the rules have decided, less the margin: the clock waits there for them (forward only).
		const double Cap = FMath::Max(From, S.Rec.Fought - LiveMargin);
		if (To > Cap + 1e-9)
		{
			To = Cap;
			LiveHeld += Real;
		}
		else
		{
			LiveHeld = 0.0;
		}
		// (said only once the wait is felt, and never in the setup step, whose clock stands still by design)
		static const std::string Waiting = "HOLDING A MOMENT WHILE THE FIGHT IS WORKED OUT";
		if (LiveHeld > 1.5 && !S.Rec.bSetup && !bLiveBroken && S.Note.empty())
		{
			S.Note = Waiting;
			S.NoteAge = -1.0;
		}
		else if (LiveHeld == 0.0 && S.Note == Waiting)
		{
			S.Note.clear();
			S.NoteAge = -1.0;
		}
	}
	if (Live.IsValid())
	{
		Live->Want(To, LiveLead * FMath::Max(1.0, S.SpeedNow));
	}
	return To;
}

void ALandBattle::OrderLive(const FString& CommandJson)
{
	if (!Live.IsValid() || !State.IsValid())
	{
		return;
	}
	// (given now: the battle time shown, so the radio's delay runs from the moment he gave it, not from where the rules
	// have reached, a couple of seconds on)
	FString Command = CommandJson.TrimStartAndEnd();
	if (!Command.Contains(TEXT("\"at\"")) && Command.EndsWith(TEXT("}")))
	{
		Command = Command.LeftChop(1) + FString::Printf(TEXT(",\"at\":%.2f}"), State->Battle);
	}
	Live->Order(Command);
	++State->FieldOrders; // (field notes: an order given)
}

void ALandBattle::PlaceLive(int32 Block, double X, double Y, double Face)
{
	if (Live.IsValid())
	{
		Live->Place(Block, X, Y, Face);
		if (State.IsValid())
		{
			++State->FieldPlaced; // (field notes: a unit set down)
		}
	}
}

void ALandBattle::BeginFight()
{
	if (Live.IsValid() && State.IsValid() && State->Rec.bSetup)
	{
		Live->Begin();
	}
}

void ALandBattle::LiveWayOut(const TCHAR* Why)
{
	UE_LOG(LogTheater, Warning, TEXT("Land battle (live): %s: %s; the field closes with no result%s"), *Key, Why,
		HandBack ? TEXT(", the campaign's attack card back as he left it") : TEXT(""));
	Finish(); // (as CONTINUE: the screen down, the table up, an empty result handed back, and the scene destroyed)
}

void ALandBattle::CloseLive()
{
	if (Live.IsValid())
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle (live): closed at %.1f s fought; the rules' last piece %.0f ms, the slowest %.0f ms"), Live->Fought(), Live->LastMs(), Live->MostMs());
		LandFell::Disarm();
		Live->Close();
		Live.Reset();
	}
	bLiveWaiting = false;
	PendingLive.Empty();
}
