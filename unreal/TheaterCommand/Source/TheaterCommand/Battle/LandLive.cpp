#include "Battle/LandLive.h"

#include "TheaterCommand.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Rules/RulesHost.h"

#include <cstdlib>
#include <cstring>
#include <utility>

namespace
{
	/** A string as a JSON string, quoted and escaped. */
	FString Quoted(const FString& Text)
	{
		FString Out = TEXT("\"");
		for (const TCHAR C : Text)
		{
			if (C == TEXT('"') || C == TEXT('\\')) { Out.AppendChar(TEXT('\\')); Out.AppendChar(C); }
			else if (C < 32) Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(C));
			else Out.AppendChar(C);
		}
		Out.AppendChar(TEXT('"'));
		return Out;
	}

	/** A field's text from a small JSON answer ({"ok":false,"why":"..."}), without reading the whole (plain words only). */
	FString FieldText(const FString& Json, const TCHAR* Name)
	{
		const FString Opening = FString::Printf(TEXT("\"%s\":\""), Name);
		const int32 At = Json.Find(Opening);
		if (At == INDEX_NONE) return FString();
		FString Out;
		for (int32 I = At + Opening.Len(); I < Json.Len() && Json[I] != TEXT('"'); ++I)
		{
			if (Json[I] == TEXT('\\') && I + 1 < Json.Len()) ++I;
			Out.AppendChar(Json[I]);
		}
		return Out;
	}
}

FLandLive::FLandLive()
{
	Wake = FPlatformProcess::GetSynchEventFromPool(false);
}

FLandLive::~FLandLive()
{
	Close();
	if (Wake)
	{
		FPlatformProcess::ReturnSynchEventToPool(Wake);
		Wake = nullptr;
	}
}

namespace
{
	/** The player's battle rules, in the order the practice card lists them (FLandLive::FRule). */
	// (each tip: what the rule does, then "Off:" what changes; the UI director, 22 November 20:56Z)
	const FLandLive::FRule LandRuleTable[] = {
		{ TEXT("cutters"), TEXT("Hedge cutters"), TEXT("Your tanks cut a gap through a hedge where you pick (from July 1944). Off: no cutting; tanks cross hedges as they always have."), TEXT("TheaterLandNoCutters") },
		{ TEXT("smoke"), TEXT("Smoke on call"), TEXT("Your men on foot lay a smoke screen along a line you drag. Off: no smoke screens on call."), TEXT("TheaterLandNoSmoke") },
		{ TEXT("hide"), TEXT("Forest ambushes"), TEXT("Units in woods stay hidden from the enemy until found or firing. Off: woods hide no one."), TEXT("TheaterLandNoHide") },
		{ TEXT("relief"), TEXT("Hold till relieved"), TEXT("A unit digs in to wait for a corps coming up; if help comes late, it breaks sooner. Off: no hold order; help simply arrives."), TEXT("TheaterLandNoRelief") },
		{ TEXT("rider"), TEXT("Despatch riders"), TEXT("A corps coming up sends a rider ahead; click him for its news. Off: help arrives with no word ahead."), TEXT("TheaterLandNoRider") },
	};
	const TCHAR* const RulesSection = TEXT("TheaterCommand.LandRules");
}

int32 FLandLive::RuleCount()
{
	return UE_ARRAY_COUNT(LandRuleTable);
}

const FLandLive::FRule& FLandLive::RuleAt(int32 Index)
{
	return LandRuleTable[FMath::Clamp(Index, 0, RuleCount() - 1)];
}

bool FLandLive::RuleOn(int32 Index)
{
	bool bOn = true;
	if (GConfig)
	{
		GConfig->GetBool(RulesSection, RuleAt(Index).Key, bOn, GGameUserSettingsIni);
	}
	return bOn;
}

void FLandLive::SetRule(int32 Index, bool bOn)
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetBool(RulesSection, RuleAt(Index).Key, bOn, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

bool FLandLive::Start(const FString& InKey, int32 InSeed, const FString& InMe, bool bInSetup)
{
	if (Thread)
	{
		return false;
	}
	Key = InKey;
	Seed = InSeed;
	Me = InMe;
	bSetup = bInSetup;
	// (the battle rules the player may switch off, read here on the game thread, where his settings are written: each on
	// unless he switched it off on the practice card, or for one run on the command line)
	Switches.Reset();
	for (int32 R = 0; R < RuleCount(); ++R)
	{
		const bool bOn = RuleOn(R) && !FParse::Param(FCommandLine::Get(), LandRuleTable[R].OffFlag);
		Switches += FString::Printf(TEXT(",\"%s\":%s"), LandRuleTable[R].Key, bOn ? TEXT("true") : TEXT("false"));
	}
	Thread = FRunnableThread::Create(this, TEXT("TheaterLandRules"), 0, TPri_Normal);
	return Thread != nullptr;
}

bool FLandLive::StartTicket(const FString& LookJson, const FString& InMe, bool bInSetup)
{
	if (Thread || !LookJson.TrimStart().StartsWith(TEXT("{")))
	{
		return false;
	}
	Ticket = LookJson;
	return Start(TEXT("field"), 0, InMe, bInSetup);
}

bool FLandLive::Result(FString& OutJson) const
{
	FScopeLock Hold(&HeaderGate);
	if (ResultJson.IsEmpty())
	{
		return false;
	}
	OutJson = ResultJson;
	return true;
}

bool FLandLive::TakeHeader(std::string& OutHeader)
{
	FScopeLock Hold(&HeaderGate);
	if (!bHeaderReady || bHeaderTaken)
	{
		return false;
	}
	bHeaderTaken = true;
	OutHeader = std::move(HeaderText);
	HeaderText.clear();
	return true;
}

bool FLandLive::Failed(FString& OutWhy) const
{
	FScopeLock Hold(&HeaderGate);
	OutWhy = Why;
	return bFailed;
}

void FLandLive::Fail(const FString& InWhy)
{
	FScopeLock Hold(&HeaderGate);
	bFailed = true;
	Why = InWhy;
	UE_LOG(LogTheater, Warning, TEXT("Land battle (live): %s"), *InWhy);
}

void FLandLive::Want(double Shown, double Ahead)
{
	WantT.store(Shown);
	AheadT.store(FMath::Max(0.5, Ahead));
	if (Wake && Shown + Ahead > FoughtT.load())
	{
		Wake->Trigger();
	}
}

bool FLandLive::TakeOne(std::string& OutDelta)
{
	return Deltas.Dequeue(OutDelta);
}

void FLandLive::Order(const FString& CommandJson)
{
	Requests.Enqueue({ TEXT("order"), TEXT("landOrder"), TEXT("[") + CommandJson + TEXT("]"), CommandJson });
	if (Wake) Wake->Trigger();
}

void FLandLive::Place(int32 Id, double X, double Y, double Face)
{
	Requests.Enqueue({ TEXT("place"), TEXT("landPlace"), FString::Printf(TEXT("[%d,%.2f,%.2f,%.4f]"), Id, X, Y, Face), FString::FromInt(Id) });
	if (Wake) Wake->Trigger();
}

void FLandLive::Begin()
{
	Requests.Enqueue({ TEXT("begin"), TEXT("landBegin"), TEXT("[]"), FString() });
	if (Wake) Wake->Trigger();
}

void FLandLive::Field(const FString& HedgesJson, const FString& GatesJson)
{
	Requests.Enqueue({ TEXT("hedges"), TEXT("landHedges"), TEXT("[") + HedgesJson + TEXT(",") + GatesJson + TEXT("]"), FString() });
	if (Wake) Wake->Trigger();
}

void FLandLive::Answers(TArray<FAnswer>& Out)
{
	FAnswer A;
	while (Replies.Dequeue(A))
	{
		Out.Add(MoveTemp(A));
	}
}

double FLandLive::TimeOf(const std::string& Delta, bool& bOutLast)
{
	// {"t":12.3,"end":12.3,... : the rules write t first, and end straight after it on the battle's last delta.
	bOutLast = false;
	if (Delta.compare(0, 5, "{\"t\":") != 0)
	{
		return -1.0;
	}
	const char* At = Delta.c_str() + 5;
	char* Past = nullptr;
	const double T = std::strtod(At, &Past);
	if (Past == At)
	{
		return -1.0;
	}
	bOutLast = std::strncmp(Past, ",\"end\":", 7) == 0;
	return T;
}

void FLandLive::Stop()
{
	bStopping.store(true);
	if (Wake) Wake->Trigger();
}

void FLandLive::Close()
{
	if (!Thread)
	{
		return;
	}
	Stop();
	Thread->WaitForCompletion();
	delete Thread;
	Thread = nullptr;
}

uint32 FLandLive::Run()
{
	// The rules engine is made, used and let go on this thread alone.
	TUniquePtr<FRulesHost> Host = MakeUnique<FRulesHost>();
	if (!Host->IsAvailable())
	{
		Fail(TEXT("this build has no engine to run the land rules on"));
		return 1;
	}
	FString Error;
	const FString Bundle = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules"), TEXT("land-rules.js"));
	if (!Host->LoadScript(Bundle, Error))
	{
		Fail(FString::Printf(TEXT("the land rules (Content/Rules/land-rules.js) did not load: %s. Run Tools/build_land_rules.sh."), *Error));
		return 1;
	}
	// (each rifle figure drawn as its whole platoon, about 2,000 men in Normandy, only when asked: the Mac draws that many
	// at 8 to 20 frames a second until the crowd is skinned on the graphics chip (the Mac builds thread, 22 November
	// 06:36Z); sections, about 700, are smooth. Brian's rule: as many as the Mac draws smoothly, the ratio said)
	const bool bPlatoons = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandPlatoons"));
	// (the player's battle rules, read as the battle was asked for (Start); the bocage, tanks without cutters crossing
	// hedges only at gaps, only when asked while the tactics thread finishes it)
	const bool bBocage = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandBocage"));
	const FString Options = FString::Printf(TEXT("{\"me\":%s,\"setup\":%s,\"platoons\":%s,\"bocage\":%s%s}"), Me.IsEmpty() ? TEXT("null") : *Quoted(Me),
		bSetup ? TEXT("true") : TEXT("false"), bPlatoons ? TEXT("true") : TEXT("false"), bBocage ? TEXT("true") : TEXT("false"), *Switches);
	UE_LOG(LogTheater, Display, TEXT("Land battle (live): the battle's rules %s"), *Options);
	// (a campaign battle from its ticket: the campaign's look answer as it stands, the same options beside it)
	const bool bTicket = !Ticket.IsEmpty();
	const FString Arguments = bTicket ? FString::Printf(TEXT("[%s,%s]"), *Ticket, *Options)
		: FString::Printf(TEXT("[%s,%s,%s]"), *Quoted(Key), Seed > 0 ? *FString::FromInt(Seed) : TEXT("null"), *Options);
	FString Header;
	const double Opened = FPlatformTime::Seconds();
	if (!Host->Call(bTicket ? TEXT("landStartTicket") : TEXT("landStart"), Arguments, Header, Error) || !Header.StartsWith(TEXT("{")))
	{
		Fail(bTicket ? FString::Printf(TEXT("TC.landStartTicket (a campaign battle, %d KB of ticket) failed: %s"), Ticket.Len() / 1024, *Error)
			: FString::Printf(TEXT("TC.landStart(%s) failed: %s"), *Key, *Error));
		return 1;
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle (live): %s opened in %.0f ms (%d KB), the player %s%s"), *Key, (FPlatformTime::Seconds() - Opened) * 1000.0,
		Header.Len() / 1024, Me.IsEmpty() ? TEXT("watching") : *Me, bSetup ? TEXT(", deploying first") : TEXT(""));
	{
		const FTCHARToUTF8 Narrow(*Header);
		FScopeLock Hold(&HeaderGate);
		HeaderText.assign(Narrow.Get(), static_cast<size_t>(Narrow.Length()));
		bHeaderReady = true;
	}

	// The first piece is the men as they stand (every track first seen), fought or not; then the rules are kept ahead.
	// The clock waits for the 3D field's hedges (Field), or 20 s.
	bool bNeedPiece = true;
	bool bDeploying = bSetup;
	const double HeaderAt = FPlatformTime::Seconds();
	while (!bStopping.load())
	{
		bool bDidSomething = false;
		FRequest Asked;
		while (Requests.Dequeue(Asked))
		{
			FString Reply;
			FAnswer A;
			A.Kind = Asked.Kind;
			A.Asked = Asked.Asked;
			A.bOk = Host->Call(Asked.Function, Asked.Arguments, Reply, Error) && Reply.Contains(TEXT("\"ok\":true"));
			if (!A.bOk)
			{
				A.Why = FieldText(Reply, TEXT("why"));
				if (A.Why.IsEmpty() && !Error.IsEmpty()) A.Why = Error;
			}
			// (a block placed stands its men again: the next piece starts them afresh; begun, the clock runs)
			if (A.bOk && Asked.Kind == TEXT("place")) bNeedPiece = true;
			if (A.bOk && Asked.Kind == TEXT("begin")) { bDeploying = false; bNeedPiece = true; }
			// (the hedges in, the blocks on their lines hide: their men stand again; given or not, the clock may run)
			if (Asked.Kind == TEXT("hedges"))
			{
				bFieldIn.store(true);
				if (A.bOk) bNeedPiece = true;
				UE_LOG(LogTheater, Display, TEXT("Land battle (live): the field's hedges %s the rules (%s)"), A.bOk ? TEXT("handed to") : TEXT("not taken by"), *Reply.Left(80));
			}
			Replies.Enqueue(MoveTemp(A));
			bDidSomething = true;
		}
		const double Target = WantT.load() + AheadT.load();
		const double Now = FoughtT.load();
		const bool bFieldWait = !bFieldIn.load() && FPlatformTime::Seconds() - HeaderAt < 20.0;
		const bool bBehind = !bDeploying && !bFieldWait && !bOver.load() && Now + 1e-6 < Target;
		if (bNeedPiece || bBehind)
		{
			// At most half a battle second a call, so orders given meanwhile wait no longer than that.
			const double To = bBehind ? FMath::Min(Target, Now + 0.5) : Now;
			FString Answer;
			const double Began = FPlatformTime::Seconds();
			if (!Host->Call(TEXT("landTo"), FString::Printf(TEXT("[%.3f,50]"), To), Answer, Error))
			{
				Fail(FString::Printf(TEXT("TC.landTo failed at %.1f s: %s"), Now, *Error));
				break;
			}
			const double Ms = (FPlatformTime::Seconds() - Began) * 1000.0;
			LastStepMs.store(Ms);
			if (Ms > MostStepMs.load()) MostStepMs.store(Ms);
			const FTCHARToUTF8 Narrow(*Answer);
			std::string Delta(Narrow.Get(), static_cast<size_t>(Narrow.Length()));
			bool bLast = false;
			const double T = TimeOf(Delta, bLast);
			if (T < 0.0)
			{
				Fail(TEXT("TC.landTo's answer is not a delta"));
				break;
			}
			if (bLast)
			{
				// (the result as the campaign takes it, kept for its hand-back, TC.battleSettleLand: before the last delta
				// goes, so the game thread that reads the end finds it)
				FString Over;
				if (Host->Call(TEXT("landResult"), TEXT("[]"), Over, Error))
				{
					FScopeLock Hold(&HeaderGate);
					ResultJson = Over;
				}
			}
			Deltas.Enqueue(MoveTemp(Delta));
			FoughtT.store(T);
			if (bLast) bOver.store(true);
			bNeedPiece = false;
			bDidSomething = true;
		}
		if (!bDidSomething && Wake)
		{
			Wake->Wait(FTimespan::FromMilliseconds(5.0));
		}
	}
	FString Ignored;
	Host->Call(TEXT("landEnd"), TEXT("[]"), Ignored, Error);
	Host.Reset();
	return 0;
}
