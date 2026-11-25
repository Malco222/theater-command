#include "Campaign/CampaignSession.h"

#include "Campaign/CampaignMapData.h"
#include "Campaign/TheaterSpeed.h"
#include "Rules/RulesHost.h"
#include "TheaterCommand.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <atomic>

static void ReadSorties(const TArray<TSharedPtr<FJsonValue>>& List, TArray<FSortie>& Out);
static void ReadSinkings(const TArray<TSharedPtr<FJsonValue>>& List, TArray<FSinking>& Out);

namespace
{
	FString IdList(const TArray<int32>& Ids)
	{
		return FString::JoinBy(Ids, TEXT(","), [](int32 Id) { return FString::FromInt(Id); });
	}

	/** (Speed, b414 mark 1: see FCampaignSession::MoveAside.) */
	TAutoConsoleVariable<int32> CVarRulesAside(
		TEXT("tc.Speed.RulesAside"), 1,
		TEXT("1 (default): a march, a landing and an attack are answered by the rules on a thread of their own, with the questions the army in hand asks next, and the press waits for nothing. 0: answered in the frame of the press, as before."),
		ECVF_Default);
}

FCampaignSession::FCampaignSession() = default;

FCampaignSession::~FCampaignSession()
{
	// (the rules' thread uses the engine, which goes with the session)
	if (AsideRun.IsValid())
	{
		AsideRun.Wait();
	}
	if (SceneAskRun.IsValid())
	{
		SceneAskRun.Wait();
	}
}

namespace
{
	/** Text as a JSON string, quotes and all, for handing to the rules as an argument. */
	FString JsonQuote(const FString& Text)
	{
		FString Out;
		Out.Reserve(Text.Len() + Text.Len() / 8 + 2);
		Out.AppendChar(TEXT('"'));
		for (const TCHAR Ch : Text)
		{
			switch (Ch)
			{
			case TEXT('"'): Out += TEXT("\\\""); break;
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('\n'): Out += TEXT("\\n"); break;
			case TEXT('\r'): Out += TEXT("\\r"); break;
			case TEXT('\t'): Out += TEXT("\\t"); break;
			default:
				if (Ch < 0x20)
				{
					Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Ch));
				}
				else
				{
					Out.AppendChar(Ch);
				}
			}
		}
		Out.AppendChar(TEXT('"'));
		return Out;
	}

	/** (One voice after a press.) The army in hand's name and its corps in the field, as the rules' last two arguments: nothing where no name is given. */
	FString ArmyArgs(const FString& Army, const TArray<int32>& ArmyIds)
	{
		return Army.IsEmpty() ? FString() : FString::Printf(TEXT(",%s,[%s]"), *JsonQuote(Army), *IdList(ArmyIds));
	}

	/**
	 * (Speed, b414 mark 1.) The arguments of the questions the army in hand asks after an order, made one way for the
	 * question and for the order that asks it ahead: an answer kept ahead is found by them, to the character.
	 */
	FString RegionArgs(int32 RegionId)
	{
		return FString::Printf(TEXT("[%d]"), RegionId);
	}
	FString ReachArgs(const TArray<int32>& CorpsIds)
	{
		return FString::Printf(TEXT("[[%s]]"), *IdList(CorpsIds));
	}
	FString RailShowArgs(const TArray<int32>& CorpsIds, const TArray<int32>& March, const FString& Army, const TArray<int32>& ArmyIds)
	{
		return FString::Printf(TEXT("[[%s],[%s]%s]"), *IdList(CorpsIds), *IdList(March), *ArmyArgs(Army, ArmyIds));
	}
}

bool FCampaignSession::Prepare(FString& OutError)
{
	DropAside(); // (before the engine it was asked of goes)
	bReady = false;
	bRunning = false;
	Host = MakeUnique<FRulesHost>();
	if (!Host->IsAvailable())
	{
		OutError = TEXT("This build has no engine to run the rules on");
		return false;
	}

	// The real rules carry their own data. The stand-in is handed the same three files the map is drawn from.
	const FString RulesFolder = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules"));
	const FString RealRules = FPaths::Combine(RulesFolder, TEXT("campaign-rules.js"));
	const bool bReal = IFileManager::Get().FileExists(*RealRules);
	if (!bReal)
	{
		FString Map, Nations, Setup;
		if (!FFileHelper::LoadFileToString(Map, *FCampaignMapData::ResolveDataFile(TEXT("campaign-map.json"), TEXT("src/generated")))
			|| !FFileHelper::LoadFileToString(Nations, *FCampaignMapData::ResolveDataFile(TEXT("nations.json"), TEXT("data/campaign")))
			|| !FFileHelper::LoadFileToString(Setup, *FCampaignMapData::ResolveDataFile(TEXT("setup-1939.json"), TEXT("data/campaign"))))
		{
			OutError = TEXT("Could not read the campaign data for the rules");
			return false;
		}
		const FString Data = FString::Printf(TEXT("globalThis.__data = { map: %s, nations: %s, setup: %s };"), *Map, *Nations, *Setup);
		if (!Host->Evaluate(Data, TEXT("campaign data"), OutError))
		{
			return false;
		}
	}
#if !UE_BUILD_SHIPPING
	// R2 bargains hook: -TheaterR2Deals opens new grand campaigns with a neutral's bargains and the volunteer legions (development only)
	if (bReal && FParse::Param(FCommandLine::Get(), TEXT("TheaterR2Deals")) && !Host->Evaluate(TEXT("globalThis.TC_BARGAINS = 1; globalThis.TC_LEGIONS = 1;"), TEXT("release 2 deals"), OutError))
	{
		return false;
	}
	// (Release 2) -TheaterR2Names and -TheaterR2Columns open new grand campaigns with earned nicknames and with supply
	// columns, for a picture of each (development only; a player switches them under OPTIONS)
	if (bReal && FParse::Param(FCommandLine::Get(), TEXT("TheaterR2Names")) && !Host->Evaluate(TEXT("globalThis.TC_NICKNAMES = 1;"), TEXT("release 2 names"), OutError))
	{
		return false;
	}
	if (bReal && FParse::Param(FCommandLine::Get(), TEXT("TheaterR2Columns")) && !Host->Evaluate(TEXT("globalThis.TC_COLUMNS = 1;"), TEXT("release 2 columns"), OutError))
	{
		return false;
	}
	// (Release 2, the marked army) -TheaterR2Marked opens them with the armies that take his towns marked
	if (bReal && FParse::Param(FCommandLine::Get(), TEXT("TheaterR2Marked")) && !Host->Evaluate(TEXT("globalThis.TC_MARKED = 1;"), TEXT("release 2 marked army"), OutError))
	{
		return false;
	}
	// (Release 2, new units ready) -TheaterR2Ready opens his month with an infantry corps on order at his capital, due next month (for the pictures)
	if (bReal && FParse::Param(FCommandLine::Get(), TEXT("TheaterR2Ready")) && !Host->Evaluate(TEXT("globalThis.TC_READY_DEMO = 1;"), TEXT("release 2 new units ready"), OutError))
	{
		return false;
	}
	// -TheaterR2Supply opens them with the columns and all three supply orders: the lorries moved, wagons, dumps
	if (bReal && FParse::Param(FCommandLine::Get(), TEXT("TheaterR2Supply")) && !Host->Evaluate(TEXT("globalThis.TC_COLUMNS = 1; globalThis.TC_LORRYSHIFT = 1; globalThis.TC_WAGONS = 1; globalThis.TC_DUMPS = 1;"), TEXT("release 2 supply"), OutError))
	{
		return false;
	}
	// (Release 2, the whole world) -TheaterFar beside -TheaterWorld: the rules play the far side, Asia, the Indies and
	// the Pacific, on the whole world's chart (src/campaign/stage.ts reads TC_FAR when its bundle loads), as the table
	// draws it (FCampaignMapData::FarSwitch). With stage.json's "far" on, the bundle is built with it and needs nothing.
	if (bReal && FParse::Param(FCommandLine::Get(), TEXT("TheaterWorld")) && FParse::Param(FCommandLine::Get(), TEXT("TheaterFar"))
		&& !Host->Evaluate(TEXT("globalThis.TC_FAR = 1;"), TEXT("release 2 the far side"), OutError))
	{
		return false;
	}
#endif
	// (Release 2, THE HOME FRONT; tc.UI.HomeFront, as -TheaterHomeFront.) The board's door on the top bar and its chip
	// under END THE MONTH are the rules' to name (the view's rooms and its to-do list), and they name them only when
	// asked here. In a shipped build too: the ship list turns the board on by its launch word.
	{
		const IConsoleVariable* HomeFront = IConsoleManager::Get().FindConsoleVariable(TEXT("tc.UI.HomeFront"));
		if (bReal && (FParse::Param(FCommandLine::Get(), TEXT("TheaterHomeFront")) || (HomeFront && HomeFront->GetInt() != 0))
			&& !Host->Evaluate(TEXT("globalThis.TC_HOME_FRONT = 1;"), TEXT("release 2 the home front"), OutError))
		{
			return false;
		}
	}
	if (!Host->LoadScript(bReal ? RealRules : FPaths::Combine(RulesFolder, TEXT("stand-in-rules.js")), OutError))
	{
		return false;
	}
	bReady = true;
	ReadNotebook();
	return true;
}

TArray<FStartGroup> FCampaignSession::Starts()
{
	TArray<FStartGroup> Out;
	FString Json, Error;
	TSharedPtr<FJsonObject> Object;
	Settle();
	RulesMemo.Reset(); // (a new war, or one read in: nothing asked of the old one holds)
	if (!bReady || !Host->Call(TEXT("starts"), TEXT("[]"), Json, Error)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
	{
		// Rules that do not offer a choice (the stand-in): one way in.
		FStartGroup& Group = Out.AddDefaulted_GetRef();
		Group.Title = TEXT("September 1939");
		FStartSide& Side = Group.Sides.AddDefaulted_GetRef();
		Side.Nation = TEXT("GER");
		Side.Name = TEXT("Germany");
		return Out;
	}
	Years.Reset();
	const TArray<TSharedPtr<FJsonValue>>* YearValues = nullptr;
	if (Object->TryGetArrayField(TEXT("years"), YearValues))
	{
		for (const TSharedPtr<FJsonValue>& YearValue : *YearValues)
		{
			const TSharedPtr<FJsonObject> YearObject = YearValue->AsObject();
			FGrandYear& Year = Years.AddDefaulted_GetRef();
			YearObject->TryGetStringField(TEXT("year"), Year.Year);
			YearObject->TryGetStringField(TEXT("title"), Year.Title);
			YearObject->TryGetStringField(TEXT("note"), Year.Note);
			YearObject->TryGetNumberField(TEXT("from"), Year.From);
		}
	}
	Levels.Reset();
	Object->TryGetStringField(TEXT("level"), DefaultLevel);
	const TArray<TSharedPtr<FJsonValue>>* LevelValues = nullptr;
	if (Object->TryGetArrayField(TEXT("levels"), LevelValues))
	{
		for (const TSharedPtr<FJsonValue>& LevelValue : *LevelValues)
		{
			const TSharedPtr<FJsonObject> LevelObject = LevelValue->AsObject();
			FGrandLevel& Level = Levels.AddDefaulted_GetRef();
			LevelObject->TryGetStringField(TEXT("id"), Level.Id);
			LevelObject->TryGetStringField(TEXT("name"), Level.Name);
			LevelObject->TryGetStringField(TEXT("blurb"), Level.Blurb);
			LevelObject->TryGetStringArrayField(TEXT("lines"), Level.Lines);
		}
	}
	// (rules version 66) How deep it is played.
	Depths.Reset();
	Object->TryGetStringField(TEXT("depth"), DefaultDepth);
	const TArray<TSharedPtr<FJsonValue>>* DepthValues = nullptr;
	if (Object->TryGetArrayField(TEXT("depths"), DepthValues))
	{
		for (const TSharedPtr<FJsonValue>& DepthValue : *DepthValues)
		{
			const TSharedPtr<FJsonObject> DepthObject = DepthValue->AsObject();
			FGrandLevel& Depth = Depths.AddDefaulted_GetRef();
			DepthObject->TryGetStringField(TEXT("id"), Depth.Id);
			DepthObject->TryGetStringField(TEXT("name"), Depth.Name);
			DepthObject->TryGetStringField(TEXT("under"), Depth.Under);
			DepthObject->TryGetStringField(TEXT("blurb"), Depth.Blurb);
		}
	}
	// (rules version 70) The history dial's stops.
	Histories.Reset();
	Object->TryGetNumberField(TEXT("history"), DefaultHistory);
	const TArray<TSharedPtr<FJsonValue>>* StopValues = nullptr;
	if (Object->TryGetArrayField(TEXT("histories"), StopValues))
	{
		for (const TSharedPtr<FJsonValue>& StopValue : *StopValues)
		{
			const TSharedPtr<FJsonObject> StopObject = StopValue->AsObject();
			FGrandLevel& Stop = Histories.AddDefaulted_GetRef();
			StopObject->TryGetNumberField(TEXT("stop"), Stop.Stop);
			StopObject->TryGetStringField(TEXT("id"), Stop.Id);
			StopObject->TryGetStringField(TEXT("name"), Stop.Name);
			StopObject->TryGetStringField(TEXT("under"), Stop.Under);
			StopObject->TryGetStringField(TEXT("blurb"), Stop.Blurb);
			StopObject->TryGetStringArrayField(TEXT("lines"), Stop.Lines);
		}
	}
	for (const TSharedPtr<FJsonValue>& GroupValue : Object->GetArrayField(TEXT("groups")))
	{
		const TSharedPtr<FJsonObject> GroupObject = GroupValue->AsObject();
		FStartGroup& Group = Out.AddDefaulted_GetRef();
		GroupObject->TryGetStringField(TEXT("title"), Group.Title);
		GroupObject->TryGetNumberField(TEXT("from"), Group.From);
		GroupObject->TryGetStringField(TEXT("view"), Group.ViewMode);
		for (const TSharedPtr<FJsonValue>& SideValue : GroupObject->GetArrayField(TEXT("sides")))
		{
			const TSharedPtr<FJsonObject> SideObject = SideValue->AsObject();
			FStartSide& Side = Group.Sides.AddDefaulted_GetRef();
			SideObject->TryGetStringField(TEXT("nat"), Side.Nation); // null to watch
			SideObject->TryGetStringField(TEXT("name"), Side.Name);
			SideObject->TryGetStringField(TEXT("blurb"), Side.Blurb);
			SideObject->TryGetStringArrayField(TEXT("aims"), Side.Aims);
		}
	}
	return Out;
}

namespace
{
	/** The arguments of TC.grand: the month, the dice, and the wider world where the table has it. */
	FString GrandArgs(int32 From, int32 Seed)
	{
		return FString::Printf(TEXT("[%d,%d,%s]"), From, Seed, FParse::Param(FCommandLine::Get(), TEXT("TheaterWorld")) ? TEXT("true") : TEXT("false"));
	}

	/** TC.grand's answer read: who can be led. */
	TArray<FGrandNation> ReadGrandNations(const TSharedPtr<FJsonObject>& Object)
	{
		TArray<FGrandNation> Out;
		const TArray<TSharedPtr<FJsonValue>>* Listed = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(TEXT("nations"), Listed))
		{
			return Out;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Listed)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			if (!Entry.IsValid())
			{
				continue;
			}
			FGrandNation& Nation = Out.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Nation.Nation);
			Entry->TryGetStringField(TEXT("name"), Nation.Name);
			Entry->TryGetStringField(TEXT("blurb"), Nation.Blurb);
			Entry->TryGetStringField(TEXT("standing"), Nation.Standing);
			Entry->TryGetBoolField(TEXT("major"), Nation.bMajor);
			Entry->TryGetBoolField(TEXT("small"), Nation.bSmall);
			Entry->TryGetStringArrayField(TEXT("abilities"), Nation.Abilities);
			Entry->TryGetNumberField(TEXT("corps"), Nation.Corps);
			Entry->TryGetNumberField(TEXT("regions"), Nation.Regions);
		}
		return Out;
	}
}

TArray<FGrandNation> FCampaignSession::GrandNations(int32 From, int32 Seed)
{
	TArray<FGrandNation> Out;
	FString Json, Error;
	TSharedPtr<FJsonObject> Object;
	const double Began = FPlatformTime::Seconds();
	Settle();
	RulesMemo.Reset(); // (a new war, or one read in: nothing asked of the old one holds)
	if (!bReady || !Host->Call(TEXT("grand"), GrandArgs(From, Seed), Json, Error)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
	{
		UE_LOG(LogTheater, Warning, TEXT("The rules could not say who can be led from month %d: %s"), From, *Error);
		return Out;
	}
	Out = ReadGrandNations(Object);
	UE_LOG(LogTheater, Display, TEXT("Grand campaign from month %d: %d countries can be led (worked out in %.0f ms)"), From, Out.Num(), (FPlatformTime::Seconds() - Began) * 1000.0);
	return Out;
}

void FCampaignSession::GrandNationsAside(int32 From, int32 Seed, bool bAside, TFunction<void(TArray<FGrandNation>)>&& InThen)
{
	if (!bAside || CVarRulesAside.GetValueOnGameThread() == 0 || !bReady || !Host.IsValid())
	{
		InThen(GrandNations(From, Seed)); // (at once, as before)
		return;
	}
	const double Began = FPlatformTime::Seconds();
	Order(TEXT("grand"), GrandArgs(From, Seed), true, nullptr,
		[From, Began, Then = MoveTemp(InThen)](const TSharedPtr<FJsonObject>& Answer)
		{
			TArray<FGrandNation> Listed = ReadGrandNations(Answer);
			if (Answer.IsValid())
			{
				UE_LOG(LogTheater, Display, TEXT("Grand campaign from month %d: %d countries can be led (worked out beside the game in %.0f ms)"), From, Listed.Num(), (FPlatformTime::Seconds() - Began) * 1000.0);
			}
			else
			{
				UE_LOG(LogTheater, Warning, TEXT("The rules could not say who can be led from month %d"), From);
			}
			Then(MoveTemp(Listed));
		});
}

bool FCampaignSession::Begin(const FString& Human, int32 From, const FString& ViewMode, int32 Seed, FString& OutError, const FString& Level, const FString& Depth, int32 History, bool bFirstCommand)
{
	bRunning = false;
	if (!bReady && !Prepare(OutError))
	{
		return false;
	}
	// (rules version 74: with -TheaterWorld the grand campaign is played on the wider chart, which the table then has laid out)
	const FString Arguments = FString::Printf(TEXT("[{\"human\":%s,\"from\":%d,\"view\":%s,\"seed\":%d%s%s%s%s%s}]"),
		Human.IsEmpty() ? TEXT("null") : *JsonQuote(Human), From, *JsonQuote(ViewMode), Seed,
		FParse::Param(FCommandLine::Get(), TEXT("TheaterWorld")) ? TEXT(",\"world\":true") : TEXT(""),
		Level.IsEmpty() ? TEXT("") : *(TEXT(",\"level\":") + JsonQuote(Level)),
		Depth.IsEmpty() ? TEXT("") : *(TEXT(",\"depth\":") + JsonQuote(Depth)),
		History > 0 ? *FString::Printf(TEXT(",\"history\":%d"), History) : TEXT(""),
		bFirstCommand ? TEXT(",\"first\":true") : TEXT(""));
	FString ViewJson;
	DropAside();
	RulesMemo.Reset(); // (a new war, or one read in: nothing asked of the old one holds)
	TellSubScene(); // (D1: before the start, which already plays the opening month's war at sea)
	const double Began = FPlatformTime::Seconds();
	if (!Host->Call(TEXT("start"), Arguments, ViewJson, OutError))
	{
		return false;
	}
	// (Speed: the same call takes about 0.25 to 0.6 s in a fast JavaScript engine on a laptop, and two or three times
	// that where the engine cannot compile the rules as it runs them. This line says which the game has.)
	UE_LOG(LogTheater, Display, TEXT("Rules: the campaign was set up in %.0f ms (%d characters of picture)"), (FPlatformTime::Seconds() - Began) * 1000.0, ViewJson.Len());
	TSharedPtr<FJsonObject> Object;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ViewJson), Object) || !Object.IsValid())
	{
		OutError = TEXT("The rules did not return a picture of the war");
		return false;
	}
	View = ParseView(Object);
	bRunning = true;
	++WarsBegun;
	UE_LOG(LogTheater, Display, TEXT("Campaign started on the %s rules: %s, %d corps, the player leads %s"),
		*View.Rules, *View.Date, View.Corps.Num(), View.Human.IsEmpty() ? TEXT("nobody") : *View.Human);
	OnViewChanged.Broadcast();
	return true;
}

FString FCampaignSession::Save()
{
	FString Json, Error;
	Settle(); // (an order still being answered is in the war that is saved)
	if (!bRunning || !Host->Call(TEXT("save"), TEXT("[]"), Json, Error))
	{
		return FString();
	}
	TSharedPtr<FJsonObject> Object;
	FString Text;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) && Object.IsValid())
	{
		Object->TryGetStringField(TEXT("text"), Text);
	}
	return Text;
}

FString FCampaignSession::DescribeSave(const FString& SaveText)
{
	FString Json, Error;
	TSharedPtr<FJsonObject> Object;
	Settle();
	RulesMemo.Reset(); // (a new war, or one read in: nothing asked of the old one holds)
	if (!bReady || SaveText.IsEmpty() || !Host->Call(TEXT("peek"), FString::Printf(TEXT("[%s]"), *JsonQuote(SaveText)), Json, Error)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
	{
		return FString();
	}
	FString Name, Date;
	bool bFreePlay = false;
	Object->TryGetStringField(TEXT("name"), Name);
	Object->TryGetStringField(TEXT("date"), Date);
	Object->TryGetBoolField(TEXT("open"), bFreePlay);
	const FString Who = Name.IsEmpty() ? FString(bFreePlay ? TEXT("Watching a free-play war") : TEXT("Watching history")) : Name;
	return FString::Printf(TEXT("%s, %s%s"), *Who, *Date, !Name.IsEmpty() && bFreePlay ? TEXT(", free play") : TEXT(""));
}

bool FCampaignSession::Load(const FString& SaveText)
{
	FString Json, Error;
	TSharedPtr<FJsonObject> Object;
	DropAside();
	RulesMemo.Reset(); // (a new war, or one read in: nothing asked of the old one holds)
	if (!bReady || SaveText.IsEmpty() || !Host->Call(TEXT("load"), FString::Printf(TEXT("[%s]"), *JsonQuote(SaveText)), Json, Error)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
	{
		if (!Error.IsEmpty())
		{
			UE_LOG(LogTheater, Error, TEXT("Rules: %s"), *Error);
		}
		return false;
	}
	View = ParseView(Object);
	bRunning = true;
	++WarsBegun;
	TellSubScene(); // (D1: the setting is the viewer's, outside the save: told again after every load)
	UE_LOG(LogTheater, Display, TEXT("Campaign continued: %s, the player leads %s"), *View.Date, View.Human.IsEmpty() ? TEXT("nobody") : *View.Human);
	OnViewChanged.Broadcast();
	return true;
}

namespace
{
	/** The questions that change nothing in the war: see CallRules. */
	bool IsQuestion(const FString& Function)
	{
		// (Speed, b414 mark 1: "stands", "province" and "railShow" too, which the army in hand asks after every order
		// and the order given aside asks ahead. Each was checked against the rules as the six were. And "garrisonWhy",
		// whyNotOrderGarrison's sentence, which the tray asks before those three when a town's guard shares it: asked as
		// an order it threw away the three answers kept ahead. And "gateLeft", gateLeft's line of what a gate's holder
		// leaves behind, which the tray asks before the province's card: in build 1590 it threw away the province kept
		// ahead after every order, 2 to 14 ms of the frame after.)
		static const TSet<FString> Questions = { TEXT("previewAttack"), TEXT("takeOutlook"), TEXT("reach"), TEXT("marchPlan"), TEXT("railTip"), TEXT("lead"),
			TEXT("stands"), TEXT("province"), TEXT("railShow"), TEXT("garrisonWhy"), TEXT("gateLeft"),
			TEXT("bargainsLook") }; // R2 bargains hook: the map's look at the bargains (changes nothing; Campaign/BargainCalls.cpp)
		return Questions.Contains(Function);
	}

	/**
	 * (Rules 84.3.3) FIRST COMMAND's calls: they read the war and change only the player's notebook, never the war
	 * (checked: the saved game is the same before and after each), so they neither forget the questions kept nor are
	 * kept themselves. Each answers for the notebook as it stands, which they change.
	 */
	bool IsBesideTheWar(const FString& Function)
	{
		static const TSet<FString> Beside = { TEXT("lessonNow"), TEXT("lessonDeed"), TEXT("lessonAct"), TEXT("notebook"), TEXT("guide") };
		return Beside.Contains(Function);
	}

	/**
	 * (Release 2, tc.Speed.SaveAside) The war written out (TC.save, the war as JSON text) changes nothing in it: given
	 * aside it keeps the questions kept, as Save at once always has (it calls the rules without CallRules).
	 */
	bool IsWrittenOut(const FString& Function)
	{
		return Function == TEXT("save");
	}

	/**
	 * (Release 2, tc.Speed.NationsAside) Asked of the rules with or without a war in hand: TC.grand plays a history of
	 * its own (historyTo) and reads no war being played, so it may be given aside on the opening screen before any war.
	 */
	bool IsBeforeAnyWar(const FString& Function)
	{
		return Function == TEXT("grand");
	}
}

bool FCampaignSession::CallBattle(const FString& Function, const FString& ArgumentsJson, FString& OutJson, FString& OutError)
{
	Settle(); // (an order still being answered on the rules' thread comes first)
	if (!bRunning)
	{
		OutError = TEXT("no war is running");
		return false;
	}
	THEATER_SPEED_SCOPE(Rules);
	RulesMemo.Reset();
	return Host->Call(Function, ArgumentsJson, OutJson, OutError);
}

bool FCampaignSession::CallRules(const FString& Function, const FString& ArgumentsJson, TSharedPtr<FJsonObject>& OutObject)
{
	Settle(); // (an order still being answered on the rules' thread comes first: see MoveAside)
	if (!bRunning)
	{
		return false;
	}
	THEATER_SPEED_SCOPE(Rules);
	// (Speed, 17 November 2026; ruling 206(a), "the odds may be kept if they are what the card would show".) The
	// questions that change nothing in the war (the odds of an attack, what a march will meet, where an army can reach
	// and by which road, what the railway can do) are answered from the last asking while nothing has been asked since
	// that could change it: the hover tip asks them again each time the pointer comes back over a province. Every other
	// call may change the war, and forgets them all. (Each of the six was checked against the rules: asked twice it
	// answers the same, and it leaves the saved game as it found it. "lead", why an army may not go alone, is asked by
	// the same hover tip just before the odds: were it not here, it would forget the odds on every hover.)
	// "tc.Speed.RulesMemo 0" always asks.
	const bool bQuestion = IsQuestion(Function);
	if (!bQuestion && !IsBesideTheWar(Function))
	{
		RulesMemo.Reset();
	}
	const FString Key = bQuestion && TheaterSpeed::RulesMemo() ? Function + TEXT("|") + ArgumentsJson : FString();
	const double Began = FPlatformTime::Seconds();
	FString ResultJson, Error;
	const FString* Known = Key.IsEmpty() ? nullptr : RulesMemo.Find(Key);
	if (Known)
	{
		ResultJson = *Known;
	}
	else
	{
		if (!Host->Call(Function, ArgumentsJson, ResultJson, Error))
		{
			UE_LOG(LogTheater, Error, TEXT("Rules: %s"), *Error);
			return false;
		}
		if (!Key.IsEmpty())
		{
			if (RulesMemo.Num() >= 256)
			{
				RulesMemo.Reset(); // (a long month of looking about: start again rather than keep it all)
			}
			RulesMemo.Add(Key, ResultJson);
		}
	}
	TheaterSpeed::NoteRulesCall(Function, FPlatformTime::Seconds() - Began, Known != nullptr);
	THEATER_SPEED_SCOPE(RulesJson); // (reading the answer: a picture of the war is some 80,000 characters)
	return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResultJson), OutObject) && OutObject.IsValid();
}

namespace
{
	FOrderLine ReadOrder(const TSharedPtr<FJsonObject>& Object)
	{
		FOrderLine Line;
		Object->TryGetNumberField(TEXT("id"), Line.Id);
		Object->TryGetStringField(TEXT("name"), Line.Name);
		Object->TryGetStringField(TEXT("where"), Line.Where);
		Object->TryGetStringField(TEXT("due"), Line.Due);
		Object->TryGetNumberField(TEXT("months"), Line.Months);
		Object->TryGetNumberField(TEXT("cost"), Line.Cost);
		Object->TryGetBoolField(TEXT("canCancel"), Line.bCanCancel);
		Object->TryGetStringField(TEXT("sign"), Line.Sign);
		Object->TryGetStringField(TEXT("words"), Line.Words);
		Object->TryGetBoolField(TEXT("star"), Line.bStar);
		Object->TryGetNumberField(TEXT("share"), Line.Share);
		return Line;
	}

	TArray<FOrderLine> ReadOrders(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		TArray<FOrderLine> Out;
		const TArray<TSharedPtr<FJsonValue>>* Orders = nullptr;
		if (Object->TryGetArrayField(Field, Orders))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Orders)
			{
				Out.Add(ReadOrder(Value->AsObject()));
			}
		}
		return Out;
	}

	/** (Release 2) The city panel's AT A GLANCE page as the rules give it (the bridge's glanceShelves). */
	FCityShelves ReadShelves(const TSharedPtr<FJsonObject>& Object)
	{
		FCityShelves Out;
		if (!Object.IsValid())
		{
			return Out;
		}
		Out.bValid = true;
		Object->TryGetBoolField(TEXT("own"), Out.bOwn);
		Object->TryGetBoolField(TEXT("spied"), Out.bSpied);
		Object->TryGetBoolField(TEXT("agent"), Out.bAgent);
		Object->TryGetNumberField(TEXT("ago"), Out.Ago);
		Object->TryGetStringField(TEXT("garrHead"), Out.GarrisonHead);
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Object->TryGetArrayField(TEXT("garrison"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				if (!Entry.IsValid())
				{
					continue;
				}
				FGlanceCard& Card = Out.Garrison.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("kind"), Card.Kind);
				Entry->TryGetStringField(TEXT("sign"), Card.Sign);
				Entry->TryGetStringField(TEXT("name"), Card.Name);
				Entry->TryGetStringField(TEXT("line"), Card.Line);
				Entry->TryGetNumberField(TEXT("share"), Card.Share);
				Entry->TryGetNumberField(TEXT("id"), Card.Id);
			}
		}
		List = nullptr;
		if (Object->TryGetArrayField(TEXT("orders"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				if (!Entry.IsValid())
				{
					continue;
				}
				FGlanceOrder& Order = Out.Orders.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("sign"), Order.Sign);
				Entry->TryGetBoolField(TEXT("star"), Order.bStar);
				Entry->TryGetStringField(TEXT("name"), Order.Name);
				Entry->TryGetStringField(TEXT("line"), Order.Line);
				Entry->TryGetNumberField(TEXT("share"), Order.Share);
				Entry->TryGetNumberField(TEXT("months"), Order.Months);
				Entry->TryGetNumberField(TEXT("id"), Order.Id);
				Entry->TryGetBoolField(TEXT("fresh"), Order.bFresh);
			}
		}
		List = nullptr;
		if (Object->TryGetArrayField(TEXT("arriving"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				if (!Entry.IsValid())
				{
					continue;
				}
				FGlanceArrival& Arrival = Out.Arriving.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("name"), Arrival.Name);
				Entry->TryGetStringField(TEXT("line"), Arrival.Line);
				Entry->TryGetStringField(TEXT("when"), Arrival.When);
			}
		}
		Object->TryGetStringField(TEXT("buildHead"), Out.BuildHead);
		List = nullptr;
		if (Object->TryGetArrayField(TEXT("tiles"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				if (!Entry.IsValid())
				{
					continue;
				}
				FGlanceTile& Tile = Out.Tiles.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("kind"), Tile.Kind);
				Entry->TryGetStringField(TEXT("sign"), Tile.Sign);
				Entry->TryGetStringField(TEXT("name"), Tile.Name);
				Entry->TryGetStringField(TEXT("line"), Tile.Line);
				Entry->TryGetNumberField(TEXT("share"), Tile.Share);
				Entry->TryGetNumberField(TEXT("months"), Tile.Months);
				Entry->TryGetNumberField(TEXT("cost"), Tile.Cost);
				Entry->TryGetStringField(TEXT("key"), Tile.Key);
				Entry->TryGetStringField(TEXT("cause"), Tile.Cause);
				Entry->TryGetStringField(TEXT("does"), Tile.Does);
			}
		}
		List = nullptr;
		if (Object->TryGetArrayField(TEXT("rows"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				if (!Entry.IsValid())
				{
					continue;
				}
				FGlanceRow& Row = Out.Rows.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("name"), Row.Name);
				Entry->TryGetStringField(TEXT("line"), Row.Line);
				Entry->TryGetStringField(TEXT("sign"), Row.Sign);
				Entry->TryGetNumberField(TEXT("share"), Row.Share);
			}
		}
		Object->TryGetNumberField(TEXT("agents"), Out.Agents);
		Object->TryGetBoolField(TEXT("onGuard"), Out.bOnGuard);
		Object->TryGetStringField(TEXT("rail"), Out.Rail);
		// (Release 2) AIR DEFENCE, and CAN BUILD HERE.
		const TSharedPtr<FJsonObject>* AirObject = nullptr;
		if (Object->TryGetObjectField(TEXT("air"), AirObject) && AirObject && (*AirObject).IsValid())
		{
			const TSharedPtr<FJsonObject>& Air = *AirObject;
			Out.Air.bValid = true;
			Air->TryGetNumberField(TEXT("level"), Out.Air.Level);
			Air->TryGetNumberField(TEXT("heavy"), Out.Air.Heavy);
			Air->TryGetNumberField(TEXT("light"), Out.Air.Light);
			Air->TryGetNumberField(TEXT("stop"), Out.Air.Stop);
			Air->TryGetNumberField(TEXT("going"), Out.Air.Going);
			Air->TryGetBoolField(TEXT("watch"), Out.Air.bWatch);
			Air->TryGetNumberField(TEXT("fighters"), Out.Air.Fighters);
			Air->TryGetStringField(TEXT("foe"), Out.Air.Foe);
			Air->TryGetStringField(TEXT("sky"), Out.Air.Sky);
			Air->TryGetStringField(TEXT("skyBy"), Out.Air.SkyBy);
			const TSharedPtr<FJsonObject>* AddObject = nullptr;
			if (Air->TryGetObjectField(TEXT("add"), AddObject) && AddObject && (*AddObject).IsValid())
			{
				Out.Air.bCanAdd = true;
				(*AddObject)->TryGetStringField(TEXT("key"), Out.Air.AddKey);
				(*AddObject)->TryGetStringField(TEXT("size"), Out.Air.AddSize);
				(*AddObject)->TryGetStringField(TEXT("why"), Out.Air.AddWhy);
				(*AddObject)->TryGetNumberField(TEXT("cost"), Out.Air.AddCost);
				(*AddObject)->TryGetNumberField(TEXT("months"), Out.Air.AddMonths);
			}
		}
		List = nullptr;
		if (Object->TryGetArrayField(TEXT("can"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				if (!Entry.IsValid())
				{
					continue;
				}
				FGlanceCan& Can = Out.Can.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("key"), Can.Key);
				Entry->TryGetStringField(TEXT("group"), Can.Group);
				Entry->TryGetStringField(TEXT("sign"), Can.Sign);
				Entry->TryGetStringField(TEXT("label"), Can.Label);
				Entry->TryGetStringField(TEXT("size"), Can.Size);
				Entry->TryGetBoolField(TEXT("star"), Can.bStar);
				Entry->TryGetNumberField(TEXT("cost"), Can.Cost);
				Entry->TryGetNumberField(TEXT("months"), Can.Months);
				Entry->TryGetStringField(TEXT("why"), Can.Why);
			}
		}
		return Out;
	}

	/** Honours as the rules list them: a title and the two colours of a ribbon. */
	TArray<FHonourRibbon> ReadRibbons(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		TArray<FHonourRibbon> Out;
		const TArray<TSharedPtr<FJsonValue>>* Honours = nullptr;
		if (Object->TryGetArrayField(Field, Honours))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Honours)
			{
				const TSharedPtr<FJsonObject> Honour = Value->AsObject();
				FHonourRibbon Ribbon;
				Honour->TryGetStringField(TEXT("title"), Ribbon.Title);
				TArray<FString> Colours;
				if (Honour->TryGetStringArrayField(TEXT("ribbon"), Colours) && Colours.Num() >= 2)
				{
					Ribbon.First = FColor::FromHex(Colours[0]);
					Ribbon.Second = FColor::FromHex(Colours[1]);
				}
				Out.Add(MoveTemp(Ribbon));
			}
		}
		return Out;
	}
}

FCampaignView FCampaignSession::ParseView(const TSharedPtr<FJsonObject>& Object)
{
	THEATER_SPEED_SCOPE(ParseView);
	return ReadView(Object);
}

FCampaignView FCampaignSession::ReadView(const TSharedPtr<FJsonObject>& Object)
{
	FCampaignView Out;
	Object->TryGetStringField(TEXT("rules"), Out.Rules);
	Out.Turn = Object->GetIntegerField(TEXT("turn"));
	Object->TryGetStringField(TEXT("date"), Out.Date);
	Object->TryGetStringField(TEXT("human"), Out.Human); // null once the player's nation has fallen
	Object->TryGetStringField(TEXT("current"), Out.Current);
	Object->TryGetBoolField(TEXT("humanUp"), Out.bHumanUp);
	Object->TryGetBoolField(TEXT("atWar"), Out.bAtWar);
	Object->TryGetBoolField(TEXT("eve"), Out.bEve);
	Object->TryGetBoolField(TEXT("evePlan"), Out.bEvePlan);
	Object->TryGetBoolField(TEXT("autoDig"), Out.bAutoDig);
	{
		double Full = 3.0;
		Object->TryGetNumberField(TEXT("dugFull"), Full);
		Out.DugFull = FMath::Max(1, FMath::RoundToInt32(Full));
	}
	{
		const TSharedPtr<FJsonObject>* Weather = nullptr;
		if (Object->TryGetObjectField(TEXT("weather"), Weather))
		{
			(*Weather)->TryGetBoolField(TEXT("mud"), Out.bMud);
			(*Weather)->TryGetBoolField(TEXT("winter"), Out.bWinter);
			(*Weather)->TryGetStringField(TEXT("text"), Out.WeatherText);
			const TArray<TSharedPtr<FJsonValue>>* Ground = nullptr;
			if ((*Weather)->TryGetArrayField(TEXT("ground"), Ground))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Ground)
				{
					Out.WeatherGround.Add(FMath::RoundToInt32(Value->AsNumber()));
				}
			}
		}
	}
	Object->TryGetBoolField(TEXT("open"), Out.bOpen);
	Object->TryGetStringField(TEXT("level"), Out.Level);
	{
		// (rules version 66) How deep the player plays, and the six desks.
		const TSharedPtr<FJsonObject>* Command = nullptr;
		if (Object->TryGetObjectField(TEXT("command"), Command))
		{
			(*Command)->TryGetStringField(TEXT("name"), Out.CommandName);
			(*Command)->TryGetStringField(TEXT("under"), Out.CommandUnder);
			(*Command)->TryGetNumberField(TEXT("staffed"), Out.Staffed);
			const TArray<TSharedPtr<FJsonValue>>* DeskValues = nullptr;
			if ((*Command)->TryGetArrayField(TEXT("desks"), DeskValues))
			{
				for (const TSharedPtr<FJsonValue>& DeskValue : *DeskValues)
				{
					const TSharedPtr<FJsonObject> DeskObject = DeskValue->AsObject();
					if (!DeskObject.IsValid())
					{
						continue;
					}
					FViewDesk& Desk = Out.Desks.AddDefaulted_GetRef();
					DeskObject->TryGetStringField(TEXT("id"), Desk.Id);
					DeskObject->TryGetStringField(TEXT("name"), Desk.Name);
					DeskObject->TryGetBoolField(TEXT("staff"), Desk.bStaff);
					DeskObject->TryGetStringField(TEXT("line"), Desk.Line);
					DeskObject->TryGetStringField(TEXT("label"), Desk.Label);
					DeskObject->TryGetStringField(TEXT("act"), Desk.Act);
					DeskObject->TryGetStringField(TEXT("tip"), Desk.Tip);
				}
			}
		}
	}
	Object->TryGetStringField(TEXT("fallen"), Out.Fallen); // null unless the player's nation has been beaten
	Object->TryGetStringArrayField(TEXT("declare"), Out.Declarable);
	Object->TryGetStringArrayField(TEXT("league"), Out.League);
	Object->TryGetNumberField(TEXT("idle"), Out.IdleTeams);
	{
		const TArray<TSharedPtr<FJsonValue>>* Things = nullptr;
		if (Object->TryGetArrayField(TEXT("todo"), Things))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Things)
			{
				const TSharedPtr<FJsonObject>* Thing = nullptr;
				if (Value->TryGetObject(Thing))
				{
					auto& Line = Out.ToDo.AddDefaulted_GetRef();
					(*Thing)->TryGetStringField(TEXT("k"), Line.Screen);
					(*Thing)->TryGetStringField(TEXT("t"), Line.Text);
					(*Thing)->TryGetStringField(TEXT("tip"), Line.Tip);
					(*Thing)->TryGetNumberField(TEXT("tone"), Line.Tone);
				}
			}
		}
		// (Rules 84.2.6; ruling 211(c).) The alarm that names the cure, as the rules read it.
		const TArray<TSharedPtr<FJsonValue>>* Alarms = nullptr;
		if (Object->TryGetArrayField(TEXT("alarms"), Alarms))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Alarms)
			{
				const TSharedPtr<FJsonObject>* Raised = nullptr;
				if (Value->TryGetObject(Raised))
				{
					FHomeAlarm& Alarm = Out.Alarms.AddDefaulted_GetRef();
					(*Raised)->TryGetStringField(TEXT("case"), Alarm.Case);
					(*Raised)->TryGetNumberField(TEXT("region"), Alarm.Region);
					(*Raised)->TryGetStringField(TEXT("by"), Alarm.By);
					(*Raised)->TryGetStringArrayField(TEXT("lines"), Alarm.Lines);
					(*Raised)->TryGetNumberField(TEXT("help"), Alarm.HelpRegion);
					(*Raised)->TryGetNumberField(TEXT("to"), Alarm.SendTo);
					(*Raised)->TryGetStringField(TEXT("hand"), Alarm.Hand);
					const TArray<TSharedPtr<FJsonValue>>* Pick = nullptr;
					if ((*Raised)->TryGetArrayField(TEXT("pick"), Pick))
					{
						for (const TSharedPtr<FJsonValue>& Id : *Pick)
						{
							Alarm.HelpCorps.Add(FMath::RoundToInt32(Id->AsNumber()));
						}
					}
				}
			}
		}
		// (Rules 84.2.6.) His own guns left alone round an enemy town, as the rules read it.
		const TArray<TSharedPtr<FJsonValue>>* Lone = nullptr;
		if (Object->TryGetArrayField(TEXT("alone"), Lone))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Lone)
			{
				const TSharedPtr<FJsonObject>* Row = nullptr;
				if (Value->TryGetObject(Row))
				{
					FGunsAlone& Left = Out.Alone.AddDefaulted_GetRef();
					(*Row)->TryGetNumberField(TEXT("region"), Left.Region);
					(*Row)->TryGetStringField(TEXT("what"), Left.What);
					(*Row)->TryGetBoolField(TEXT("red"), Left.bRed);
					(*Row)->TryGetStringField(TEXT("line"), Left.Line);
					(*Row)->TryGetNumberField(TEXT("from"), Left.From);
					const TArray<TSharedPtr<FJsonValue>>* Help = nullptr;
					if ((*Row)->TryGetArrayField(TEXT("help"), Help))
					{
						for (const TSharedPtr<FJsonValue>& Id : *Help)
						{
							Left.HelpCorps.Add(FMath::RoundToInt32(Id->AsNumber()));
						}
					}
				}
			}
		}
	}
	{
		const TArray<TSharedPtr<FJsonValue>>* Civic = nullptr;
		if (Object->TryGetArrayField(TEXT("civic"), Civic))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Civic)
			{
				Out.Civic.Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
		Object->TryGetStringArrayField(TEXT("dark"), Out.Blackout);
		Object->TryGetStringArrayField(TEXT("wars"), Out.Wars);
		const TArray<TSharedPtr<FJsonValue>>* Wrecks = nullptr;
		if (Object->TryGetArrayField(TEXT("wrecks"), Wrecks))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Wrecks)
			{
				const TSharedPtr<FJsonObject>* Wreck = nullptr;
				if (Value->TryGetObject(Wreck))
				{
					FWreckMark& Mark = Out.Wrecks.AddDefaulted_GetRef();
					(*Wreck)->TryGetNumberField(TEXT("zone"), Mark.Zone);
					(*Wreck)->TryGetStringField(TEXT("nat"), Mark.Nation);
					(*Wreck)->TryGetStringField(TEXT("cls"), Mark.Class);
					(*Wreck)->TryGetNumberField(TEXT("turn"), Mark.Turn);
					(*Wreck)->TryGetNumberField(TEXT("seed"), Mark.Seed);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Chart = nullptr;
		if (Object->TryGetObjectField(TEXT("chart"), Chart))
		{
			const TArray<TSharedPtr<FJsonValue>>* Levels = nullptr;
			if ((*Chart)->TryGetArrayField(TEXT("seas"), Levels))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Levels)
				{
					Out.SeaSearch.Add(Value.IsValid() && !Value->IsNull() ? FMath::RoundToInt32(Value->AsNumber()) : 3);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Old = nullptr;
			if ((*Chart)->TryGetArrayField(TEXT("old"), Old))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Old)
				{
					const TSharedPtr<FJsonObject>* Seen = nullptr;
					if (Value->TryGetObject(Seen))
					{
						FOldSighting& Sighting = Out.OldSightings.AddDefaulted_GetRef();
						(*Seen)->TryGetNumberField(TEXT("zone"), Sighting.Zone);
						(*Seen)->TryGetStringField(TEXT("nat"), Sighting.Nation);
						(*Seen)->TryGetNumberField(TEXT("months"), Sighting.Months);
					}
				}
			}
		}
		Object->TryGetStringField(TEXT("lift"), Out.LiftLine);
		const TSharedPtr<FJsonObject>* Dial = nullptr;
		if (Object->TryGetObjectField(TEXT("dial"), Dial))
		{
			(*Dial)->TryGetNumberField(TEXT("stop"), Out.DialStop);
			(*Dial)->TryGetStringField(TEXT("name"), Out.DialName);
			(*Dial)->TryGetStringField(TEXT("under"), Out.DialUnder);
		}
		const TArray<TSharedPtr<FJsonValue>>* Arms = nullptr;
		if (Object->TryGetArrayField(TEXT("arms"), Arms))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Arms)
			{
				const TSharedPtr<FJsonObject>* Arm = nullptr;
				if (Value->TryGetObject(Arm))
				{
					FArmNote& Note = Out.Arms.AddDefaulted_GetRef();
					(*Arm)->TryGetStringField(TEXT("kind"), Note.Kind);
					(*Arm)->TryGetStringField(TEXT("label"), Note.Label);
					(*Arm)->TryGetNumberField(TEXT("atk"), Note.Attack);
					(*Arm)->TryGetNumberField(TEXT("def"), Note.Defence);
					(*Arm)->TryGetStringField(TEXT("role"), Note.Role);
					(*Arm)->TryGetStringField(TEXT("good"), Note.Good);
					(*Arm)->TryGetStringField(TEXT("poor"), Note.Poor);
				}
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Cold = nullptr;
		if (Object->TryGetArrayField(TEXT("cold"), Cold))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Cold)
			{
				Out.Cold.Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Fronts = nullptr;
		if (Object->TryGetArrayField(TEXT("fronts"), Fronts))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Fronts)
			{
				Out.Fronts.Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Prizes = nullptr;
		if (Object->TryGetArrayField(TEXT("prizes"), Prizes))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Prizes)
			{
				const TSharedPtr<FJsonObject>* Mark = nullptr;
				if (Value->TryGetObject(Mark))
				{
					FPrizeMark& Out1 = Out.Prizes.AddDefaulted_GetRef();
					double Region = -1.0, Burning = 0.0;
					(*Mark)->TryGetNumberField(TEXT("region"), Region);
					(*Mark)->TryGetNumberField(TEXT("burning"), Burning);
					(*Mark)->TryGetStringField(TEXT("by"), Out1.By);
					Out1.Region = FMath::RoundToInt32(Region);
					Out1.bBurning = Burning > 0.5;
				}
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Bombed = nullptr;
		if (Object->TryGetArrayField(TEXT("bombed"), Bombed))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Bombed)
			{
				Out.Bombed.Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Sieges = nullptr;
		if (Object->TryGetArrayField(TEXT("sieges"), Sieges))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Sieges)
			{
				Out.Sieges.Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Scars = nullptr;
		if (Object->TryGetArrayField(TEXT("scars"), Scars))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Scars)
			{
				Out.Scars.Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
	}
	Object->TryGetStringArrayField(TEXT("alerts"), Out.Alerts);
	const TArray<TSharedPtr<FJsonValue>>* Pages = nullptr;
	if (Object->TryGetArrayField(TEXT("pages"), Pages))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Pages)
		{
			FString Id, Label;
			Value->AsObject()->TryGetStringField(TEXT("id"), Id);
			Value->AsObject()->TryGetStringField(TEXT("label"), Label);
			Out.PageIds.Add(Id);
			Out.PageLabels.Add(Label);
		}
	}
	{
		const TSharedPtr<FJsonObject>* Men = nullptr;
		if (Object->TryGetObjectField(TEXT("men"), Men)) // null where the rules keep no reserve of men
		{
			Out.bMen = true;
			double Share = 0.0;
			(*Men)->TryGetNumberField(TEXT("share"), Share);
			Out.Men.Stock = static_cast<float>(Share);
			Out.Men.Cap = 1.0f;
			(*Men)->TryGetBoolField(TEXT("short"), Out.Men.bShort);
			(*Men)->TryGetStringField(TEXT("word"), Out.Men.Word);
			(*Men)->TryGetStringField(TEXT("tip"), Out.Men.Tip);
		}
		const TSharedPtr<FJsonObject>* Cabinet = nullptr;
		if (Object->TryGetObjectField(TEXT("cabinet"), Cabinet))
		{
			(*Cabinet)->TryGetBoolField(TEXT("can"), Out.bCabinetCan);
			Out.Allied.Reset();
			Object->TryGetStringArrayField(TEXT("allied"), Out.Allied);
			(*Cabinet)->TryGetBoolField(TEXT("off"), Out.bCabinetOff);
			(*Cabinet)->TryGetBoolField(TEXT("secure"), Out.bOfficeSecure);
		}
		const TSharedPtr<FJsonObject>* Seal = nullptr;
		if (Object->TryGetObjectField(TEXT("seal"), Seal))
		{
			double Region = -1.0, Left = 0.0;
			(*Seal)->TryGetNumberField(TEXT("region"), Region);
			(*Seal)->TryGetNumberField(TEXT("left"), Left);
			Out.SealRegion = FMath::RoundToInt32(Region);
			Out.SealLeft = FMath::RoundToInt32(Left);
		}
		const TSharedPtr<FJsonObject>* Vow = nullptr;
		if (Object->TryGetObjectField(TEXT("vow"), Vow))
		{
			double Region = -1.0, Left = 0.0;
			(*Vow)->TryGetNumberField(TEXT("region"), Region);
			(*Vow)->TryGetNumberField(TEXT("left"), Left);
			Out.VowRegion = FMath::RoundToInt32(Region);
			Out.VowLeft = FMath::RoundToInt32(Left);
		}
	}
	const TSharedPtr<FJsonObject>* Stores = nullptr;
	if (Object->TryGetObjectField(TEXT("stores"), Stores)) // null for a small country and in a scenario
	{
		Out.bStores = true;
		for (const TPair<const TCHAR*, FViewStock*>& Good : { TPair<const TCHAR*, FViewStock*>(TEXT("oil"), &Out.Oil), TPair<const TCHAR*, FViewStock*>(TEXT("sup"), &Out.Supplies) })
		{
			const TSharedPtr<FJsonObject>* Book = nullptr;
			if ((*Stores)->TryGetObjectField(Good.Key, Book))
			{
				double Stock = 0.0, Cap = 0.0, Net = 0.0;
				(*Book)->TryGetNumberField(TEXT("stock"), Stock);
				(*Book)->TryGetNumberField(TEXT("cap"), Cap);
				(*Book)->TryGetNumberField(TEXT("net"), Net);
				Good.Value->Stock = static_cast<float>(Stock);
				Good.Value->Cap = static_cast<float>(Cap);
				Good.Value->Net = static_cast<float>(Net);
				(*Book)->TryGetNumberField(TEXT("months"), Good.Value->Months);
				(*Book)->TryGetBoolField(TEXT("short"), Good.Value->bShort);
				(*Book)->TryGetStringField(TEXT("tip"), Good.Value->Tip);
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Lanes = nullptr;
	if (Object->TryGetArrayField(TEXT("lanes"), Lanes))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Lanes)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FViewLane& Lane = Out.Lanes.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("state"), Lane.State);
			double Flow = 1.0;
			Entry->TryGetNumberField(TEXT("flow"), Flow);
			Lane.Flow = static_cast<float>(Flow);
			const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
			if (Entry->TryGetArrayField(TEXT("path"), Points))
			{
				for (int32 K = 0; K + 1 < Points->Num(); K += 2)
				{
					Lane.Path.Add(FVector2D((*Points)[K]->AsNumber(), (*Points)[K + 1]->AsNumber()));
				}
			}
		}
	}
	// (Release 2, columns.ts) The month's supply columns: their roads as map points, and where each was caught (four
	// numbers a place: x, y, 1 aircraft or 2 partisans, lorries lost).
	const TArray<TSharedPtr<FJsonValue>>* Columns = nullptr;
	if (Object->TryGetArrayField(TEXT("columns"), Columns))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Columns)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Entry.IsValid())
			{
				continue;
			}
			FViewColumn& Column = Out.Columns.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Column.Nation);
			Entry->TryGetStringField(TEXT("side"), Column.Side);
			Entry->TryGetNumberField(TEXT("lorries"), Column.Lorries);
			Entry->TryGetNumberField(TEXT("lost"), Column.Lost);
			double Share = 1.0;
			Entry->TryGetNumberField(TEXT("share"), Share);
			Column.Share = static_cast<float>(Share);
			const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
			if (Entry->TryGetArrayField(TEXT("path"), Points))
			{
				for (int32 K = 0; K + 1 < Points->Num(); K += 2)
				{
					Column.Path.Add(FVector2D((*Points)[K]->AsNumber(), (*Points)[K + 1]->AsNumber()));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Hits = nullptr;
			if (Entry->TryGetArrayField(TEXT("hits"), Hits))
			{
				for (int32 K = 0; K + 3 < Hits->Num(); K += 4)
				{
					FViewColumnHit& Hit = Column.Hits.AddDefaulted_GetRef();
					Hit.At = FVector2D((*Hits)[K]->AsNumber(), (*Hits)[K + 1]->AsNumber());
					Hit.bByAir = FMath::RoundToInt32((*Hits)[K + 2]->AsNumber()) != 2;
					Hit.Lost = FMath::RoundToInt32((*Hits)[K + 3]->AsNumber());
				}
			}
			// (Release 2, logistics.ts) Absent where those switches are off.
			Entry->TryGetNumberField(TEXT("horse"), Column.Horse);
			Entry->TryGetBoolField(TEXT("fill"), Column.bFill);
			Entry->TryGetBoolField(TEXT("fromDump"), Column.bFromDump);
		}
	}
	// (Release 2, logistics.ts) The forward dumps in sight.
	const TArray<TSharedPtr<FJsonValue>>* Dumps = nullptr;
	if (Object->TryGetArrayField(TEXT("dumps"), Dumps))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Dumps)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Entry.IsValid())
			{
				continue;
			}
			FViewDump& Dump = Out.Dumps.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Dump.Nation);
			Entry->TryGetStringField(TEXT("side"), Dump.Side);
			double X = 0.0, Y = 0.0;
			Entry->TryGetNumberField(TEXT("x"), X);
			Entry->TryGetNumberField(TEXT("y"), Y);
			Dump.At = FVector2D(X, Y);
			Entry->TryGetNumberField(TEXT("region"), Dump.Region);
			Entry->TryGetNumberField(TEXT("stock"), Dump.Stock);
			Entry->TryGetBoolField(TEXT("filling"), Dump.bFilling);
		}
	}
	Object->TryGetBoolField(TEXT("lorryShiftSet"), Out.bLorryShiftSet);
	Object->TryGetBoolField(TEXT("wagonsSet"), Out.bWagonsSet);
	Object->TryGetBoolField(TEXT("dumpsSet"), Out.bDumpsSet);
	// (Release 2, nicknames.ts) Whether earned names are shown on the flags; and the options page's two switches.
	Object->TryGetBoolField(TEXT("nickFlags"), Out.bNickFlags);
	Object->TryGetBoolField(TEXT("airNames"), Out.bAirNames); // (release 2, airnames.ts)
	Object->TryGetBoolField(TEXT("markedOn"), Out.bMarkedOn); // (release 2, the marked army)
	Object->TryGetBoolField(TEXT("fleetAttack"), Out.bFleetAttack); // (release 2, fleetattack.ts)
	Object->TryGetBoolField(TEXT("breakUp"), Out.bBreakUp); // (release 2, breakup.ts)
	Object->TryGetBoolField(TEXT("taskForce"), Out.bTaskForce); // (release 2, taskforce.ts)
	Object->TryGetBoolField(TEXT("fleetWhy"), Out.bFleetWhy); // (release 2, fleetwhy.ts)
	Object->TryGetBoolField(TEXT("rearguard"), Out.bRearguard); // (release 2, rearguard.ts)
	Object->TryGetBoolField(TEXT("rearBridges"), Out.bRearBridges); // (release 2, rearguard.ts)
	Object->TryGetBoolField(TEXT("shadow"), Out.bShadowRule); // (release 2, shadow.ts)
	Object->TryGetBoolField(TEXT("voyageLog"), Out.bVoyageLog); // (release 2, voyage.ts)
	Object->TryGetBoolField(TEXT("sailors"), Out.bSailors); // (release 2, the sea field's sailor rules: battle/sea/ticket.ts)
	Object->TryGetBoolField(TEXT("ownWater"), Out.bOwnWater); // (release 2, the sea field's own water: battle/sea/ticket.ts)
	Object->TryGetBoolField(TEXT("seaMines"), Out.bSeaMines); // (release 2, mines.ts)
	Object->TryGetBoolField(TEXT("heavySeas"), Out.bHeavySeas); // (release 2, heavyseas.ts)
	Object->TryGetBoolField(TEXT("shadowRelay"), Out.bShadowRelay); // (release 2, shadow.ts)
	Object->TryGetBoolField(TEXT("seekOnce"), Out.bSeekOnce); // (release 2, navy.ts)
	Object->TryGetBoolField(TEXT("atomBomb"), Out.bAtomBomb); // (release 2, atom.ts)
	Object->TryGetBoolField(TEXT("carrierStrike"), Out.bCarrierStrike); // (release 2, harbourstrike.ts)
	Object->TryGetBoolField(TEXT("wingByDate"), Out.bWingByDate); // (release 2, carriers.ts)
	Object->TryGetBoolField(TEXT("settledTowns"), Out.bSettledTowns); // (release 2, buildings.ts)
	Object->TryGetBoolField(TEXT("r2Switches"), Out.bR2Switches);
	Object->TryGetBoolField(TEXT("columnsOn"), Out.bColumnsOn);
	Object->TryGetBoolField(TEXT("columnsCan"), Out.bColumnsCan);
	// (Release 2, the spies thread of 24 November 2026) The secret war's one switch, and whether it can still be thrown.
	Object->TryGetBoolField(TEXT("secretWar"), Out.bSecretWar);
	Object->TryGetBoolField(TEXT("secretWarCan"), Out.bSecretWarCan);
	const TSharedPtr<FJsonObject>* Designs = nullptr;
	if (Object->TryGetObjectField(TEXT("designs"), Designs))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*Designs)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Years = nullptr;
			if (Entry.Value.IsValid() && Entry.Value->TryGetArray(Years) && Years->Num() >= 3)
			{
				Out.DesignYears.Add(Entry.Key, FIntVector(static_cast<int32>((*Years)[0]->AsNumber()), static_cast<int32>((*Years)[1]->AsNumber()), static_cast<int32>((*Years)[2]->AsNumber())));
			}
		}
	}
	{
		// (Rulings 153 and 159.) His agents and what each one's card says; and where one was caught this month.
		const TArray<TSharedPtr<FJsonValue>> *AgentsTold = nullptr, *MarksTold = nullptr;
		if (Object->TryGetArrayField(TEXT("agents"), AgentsTold))
		{
			for (const TSharedPtr<FJsonValue>& Value : *AgentsTold)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				if (!Entry.IsValid())
				{
					continue;
				}
				FViewAgent& Agent = Out.Agents.AddDefaulted_GetRef();
				Entry->TryGetNumberField(TEXT("id"), Agent.Id);
				Entry->TryGetNumberField(TEXT("region"), Agent.Region);
				Entry->TryGetStringField(TEXT("name"), Agent.Name);
				Entry->TryGetStringField(TEXT("rank"), Agent.Rank);
				Entry->TryGetStringField(TEXT("toward"), Agent.Toward);
				Entry->TryGetNumberField(TEXT("skill"), Agent.Skill);
				Entry->TryGetNumberField(TEXT("ranks"), Agent.Ranks);
				Entry->TryGetNumberField(TEXT("xp"), Agent.Xp);
				Entry->TryGetNumberField(TEXT("next"), Agent.Next);
				Entry->TryGetStringField(TEXT("where"), Agent.Where);
				Entry->TryGetStringField(TEXT("host"), Agent.Host);
				Entry->TryGetStringField(TEXT("hostName"), Agent.HostName);
				Entry->TryGetBoolField(TEXT("abroad"), Agent.bAbroad);
				Entry->TryGetBoolField(TEXT("atWork"), Agent.bAtWork);
				Entry->TryGetNumberField(TEXT("wait"), Agent.Wait);
				Entry->TryGetNumberField(TEXT("caught"), Agent.Caught);
				Entry->TryGetNumberField(TEXT("rest"), Agent.Rest);
				Entry->TryGetStringField(TEXT("mission"), Agent.Mission);
				Entry->TryGetStringField(TEXT("doing"), Agent.Doing);
				Entry->TryGetStringField(TEXT("status"), Agent.Status);
				Entry->TryGetStringField(TEXT("stuck"), Agent.Stuck);
				Entry->TryGetStringField(TEXT("home"), Agent.Home);
				Entry->TryGetStringField(TEXT("will"), Agent.Will);
				const TArray<TSharedPtr<FJsonValue>> *WorkTold = nullptr, *SeesTold = nullptr;
				if (Entry->TryGetArrayField(TEXT("work"), WorkTold))
				{
					for (const TSharedPtr<FJsonValue>& Each : *WorkTold)
					{
						const TSharedPtr<FJsonObject> Row = Each->AsObject();
						if (Row.IsValid())
						{
							FAgentWork& Work = Agent.Work.AddDefaulted_GetRef();
							Row->TryGetStringField(TEXT("id"), Work.Id);
							Row->TryGetStringField(TEXT("label"), Work.Label);
							Row->TryGetStringField(TEXT("text"), Work.Text);
							Row->TryGetStringField(TEXT("why"), Work.Why);
							Row->TryGetStringField(TEXT("act"), Work.Act);
							Row->TryGetBoolField(TEXT("ok"), Work.bOk);
							Row->TryGetBoolField(TEXT("now"), Work.bNow);
							Row->TryGetNumberField(TEXT("caught"), Work.Caught);
							Row->TryGetNumberField(TEXT("success"), Work.Success);
						}
					}
				}
				if (Entry->TryGetArrayField(TEXT("sees"), SeesTold))
				{
					for (const TSharedPtr<FJsonValue>& Each : *SeesTold)
					{
						const TSharedPtr<FJsonObject> Row = Each->AsObject();
						if (Row.IsValid())
						{
							FAgentSight& Sight = Agent.Sees.AddDefaulted_GetRef();
							Row->TryGetNumberField(TEXT("region"), Sight.Region);
							Row->TryGetStringField(TEXT("name"), Sight.Name);
							Row->TryGetStringField(TEXT("nat"), Sight.Nation);
							Row->TryGetNumberField(TEXT("corps"), Sight.Corps);
							Row->TryGetNumberField(TEXT("divs"), Sight.Divisions);
						}
					}
				}
			}
		}
		if (Object->TryGetArrayField(TEXT("agentMarks"), MarksTold))
		{
			for (const TSharedPtr<FJsonValue>& Value : *MarksTold)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				if (Entry.IsValid())
				{
					FAgentMark& Mark = Out.AgentMarks.AddDefaulted_GetRef();
					Entry->TryGetNumberField(TEXT("region"), Mark.Region);
					Entry->TryGetStringField(TEXT("text"), Mark.Text);
				}
			}
		}
		// (Release 2, the spies thread of 24 November 2026) The enemy agents his hunters uncovered, and the SPIES door's number.
		const TArray<TSharedPtr<FJsonValue>>* FoesTold = nullptr;
		if (Object->TryGetArrayField(TEXT("foeAgents"), FoesTold))
		{
			for (const TSharedPtr<FJsonValue>& Value : *FoesTold)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				if (Entry.IsValid())
				{
					FViewFoeAgent& Foe = Out.FoeAgents.AddDefaulted_GetRef();
					Entry->TryGetNumberField(TEXT("id"), Foe.Id);
					Entry->TryGetNumberField(TEXT("region"), Foe.Region);
					Entry->TryGetStringField(TEXT("nat"), Foe.Nat);
					Entry->TryGetBoolField(TEXT("watch"), Foe.bWatch);
					Entry->TryGetStringField(TEXT("text"), Foe.Text);
				}
			}
		}
		Object->TryGetNumberField(TEXT("spiesWaiting"), Out.SpiesWaiting);
	}
	{
		const TArray<TSharedPtr<FJsonValue>>* CutsTold = nullptr;
		if (Object->TryGetArrayField(TEXT("cuts"), CutsTold))
		{
			for (const TSharedPtr<FJsonValue>& Value : *CutsTold)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				if (!Entry.IsValid())
				{
					continue;
				}
				FViewCut& Cut = Out.Cuts.AddDefaulted_GetRef();
				Entry->TryGetNumberField(TEXT("region"), Cut.Region);
				Entry->TryGetStringField(TEXT("how"), Cut.How);
				Entry->TryGetStringField(TEXT("head"), Cut.Head);
				Entry->TryGetStringField(TEXT("line"), Cut.Line);
				Entry->TryGetStringField(TEXT("short"), Cut.Short);
			}
		}
	}
	for (const TPair<const TCHAR*, TArray<int32>*>& List : { TPair<const TCHAR*, TArray<int32>*>(TEXT("down"), &Out.BridgesDown), TPair<const TCHAR*, TArray<int32>*>(TEXT("cut"), &Out.RailsCut) })
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object->TryGetArrayField(List.Key, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				List.Value->Add(static_cast<int32>(Value->AsNumber()));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Fleets = nullptr;
	if (Object->TryGetArrayField(TEXT("fleets"), Fleets))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Fleets)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FViewFleet& Fleet = Out.Fleets.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("id"), Fleet.Id);
			Entry->TryGetStringField(TEXT("nat"), Fleet.Nation);
			Entry->TryGetStringField(TEXT("name"), Fleet.Name);
			Entry->TryGetNumberField(TEXT("zone"), Fleet.Zone);
			Entry->TryGetNumberField(TEXT("base"), Fleet.Base);
			Entry->TryGetStringField(TEXT("port"), Fleet.Port);
			Entry->TryGetStringField(TEXT("stance"), Fleet.Stance);
			Entry->TryGetNumberField(TEXT("bb"), Fleet.Battleships);
			Entry->TryGetNumberField(TEXT("cv"), Fleet.Carriers);
			Entry->TryGetNumberField(TEXT("ca"), Fleet.Cruisers);
			Entry->TryGetNumberField(TEXT("dd"), Fleet.Destroyers);
			Entry->TryGetNumberField(TEXT("ss"), Fleet.Submarines);
			Entry->TryGetNumberField(TEXT("home"), Fleet.Home);
			Entry->TryGetBoolField(TEXT("report"), Fleet.bReport);
			Entry->TryGetStringField(TEXT("size"), Fleet.SizeWord);
			Entry->TryGetNumberField(TEXT("wingFtr"), Fleet.WingFighters);
			Entry->TryGetNumberField(TEXT("wingStrike"), Fleet.WingStrike);
			const TArray<TSharedPtr<FJsonValue>>* Ports = nullptr;
			if (Entry->TryGetArrayField(TEXT("bases"), Ports))
			{
				for (const TSharedPtr<FJsonValue>& Port : *Ports)
				{
					Fleet.Bases.Add(FMath::RoundToInt32(Port->AsNumber()));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Seas = nullptr;
			if (Entry->TryGetArrayField(TEXT("reach"), Seas))
			{
				for (const TSharedPtr<FJsonValue>& Sea : *Seas)
				{
					Fleet.Reach.Add(FMath::RoundToInt32(Sea->AsNumber()));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Barred = nullptr;
			if (Entry->TryGetArrayField(TEXT("barred"), Barred))
			{
				for (int32 K = 0; K + 1 < Barred->Num(); K += 2)
				{
					Fleet.Barred.Add(FMath::RoundToInt32((*Barred)[K]->AsNumber()), FMath::RoundToInt32((*Barred)[K + 1]->AsNumber()));
				}
			}
		}
	}
	// (Rulings 88, 101 and 102.) The round of armies, as the rules count it.
	const TSharedPtr<FJsonObject>* Round = nullptr;
	if (Object->TryGetObjectField(TEXT("round"), Round) && Round && Round->IsValid())
	{
		(*Round)->TryGetNumberField(TEXT("bright"), Out.Round.Bright);
		(*Round)->TryGetNumberField(TEXT("quiet"), Out.Round.Quiet);
		(*Round)->TryGetNumberField(TEXT("front"), Out.Round.Front);
		(*Round)->TryGetNumberField(TEXT("all"), Out.Round.All);
		(*Round)->TryGetNumberField(TEXT("idle"), Out.Round.Idle);
		(*Round)->TryGetStringField(TEXT("words"), Out.Round.Words);
		(*Round)->TryGetStringField(TEXT("question"), Out.Round.Question);
	}
	// (Ruling 86.) The sea gates.
	const TArray<TSharedPtr<FJsonValue>>* Gates = nullptr;
	if (Object->TryGetArrayField(TEXT("gates"), Gates))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Gates)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FViewGate& Gate = Out.Gates.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("id"), Gate.Id);
			Entry->TryGetStringField(TEXT("name"), Gate.Name);
			double X = 0.0, Y = 0.0;
			Entry->TryGetNumberField(TEXT("x"), X);
			Entry->TryGetNumberField(TEXT("y"), Y);
			Gate.At = FVector2D(X, Y);
			Entry->TryGetStringField(TEXT("holder"), Gate.Holder);
			Entry->TryGetBoolField(TEXT("open"), Gate.bOpen);
			Entry->TryGetStringField(TEXT("lead"), Gate.Lead);
			Entry->TryGetStringField(TEXT("mark"), Gate.Mark);
			Entry->TryGetStringField(TEXT("rest"), Gate.Rest);
			Entry->TryGetStringField(TEXT("shut"), Gate.Shut);
			Entry->TryGetStringField(TEXT("order"), Gate.Order);
			Entry->TryGetStringField(TEXT("orderLine"), Gate.OrderLine);
			Entry->TryGetStringField(TEXT("shutLine"), Gate.ShutLine);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* AirFleets = nullptr;
	if (Object->TryGetArrayField(TEXT("air"), AirFleets))
	{
		for (const TSharedPtr<FJsonValue>& Value : *AirFleets)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FViewAirFleet& Fleet = Out.AirFleets.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("id"), Fleet.Id);
			Entry->TryGetStringField(TEXT("nat"), Fleet.Nation);
			Entry->TryGetStringField(TEXT("name"), Fleet.Name);
			Entry->TryGetNumberField(TEXT("base"), Fleet.Base);
			Entry->TryGetNumberField(TEXT("ftr"), Fleet.Fighters);
			Entry->TryGetNumberField(TEXT("bmr"), Fleet.Bombers);
			Entry->TryGetStringField(TEXT("mission"), Fleet.Mission);
		}
	}
	// The airfields (the grand campaign). There the aircraft belong to their airfield, so the table shows one
	// piece for each airfield with aircraft on it instead of one for each group.
	// (Version 64) Where two worn corps could be folded into one, and fieldworks standing empty.
	const TArray<TSharedPtr<FJsonValue>>* Folds = nullptr;
	if (Object->TryGetArrayField(TEXT("folds"), Folds))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Folds)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			int32 Region = INDEX_NONE;
			TArray<FString> Names;
			if (Entry.IsValid() && Entry->TryGetNumberField(TEXT("region"), Region))
			{
				Entry->TryGetStringArrayField(TEXT("names"), Names);
				Out.Folds.Add(Region, FString::Join(Names, TEXT(" and ")));
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* CityOrders = nullptr;
	if (Object->TryGetArrayField(TEXT("cityOrders"), CityOrders))
	{
		for (const TSharedPtr<FJsonValue>& Value : *CityOrders)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FCityOrders& City = Out.CityOrders.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("region"), City.Region);
			Entry->TryGetStringField(TEXT("sign"), City.Sign);
			Entry->TryGetBoolField(TEXT("star"), City.bStar);
			Entry->TryGetNumberField(TEXT("count"), City.Count);
			Entry->TryGetBoolField(TEXT("plus"), City.bPlus);
			Entry->TryGetNumberField(TEXT("months"), City.Months);
			Entry->TryGetNumberField(TEXT("total"), City.Total);
			Entry->TryGetStringField(TEXT("head"), City.Head);
			Entry->TryGetBoolField(TEXT("spied"), City.bSpied);
			Entry->TryGetStringArrayField(TEXT("lines"), City.Lines);
		}
	}
	// (Release 2) Each town's plate under its name: garrison, on order, going up, damaged.
	const TArray<TSharedPtr<FJsonValue>>* Glance = nullptr;
	if (Object->TryGetArrayField(TEXT("glance"), Glance))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Glance)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			if (!Entry.IsValid())
			{
				continue;
			}
			FCityGlance& Plate = Out.Glance.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("region"), Plate.Region);
			Entry->TryGetBoolField(TEXT("own"), Plate.bOwn);
			Entry->TryGetBoolField(TEXT("spied"), Plate.bSpied);
			Entry->TryGetNumberField(TEXT("ago"), Plate.Ago);
			Entry->TryGetNumberField(TEXT("garr"), Plate.Garrison);
			Entry->TryGetBoolField(TEXT("guard"), Plate.bGuardOnly);
			Entry->TryGetBoolField(TEXT("quiet"), Plate.bQuiet);
			Entry->TryGetStringField(TEXT("garrFig"), Plate.GarrisonFigure);
			Entry->TryGetStringField(TEXT("garrWords"), Plate.GarrisonWords);
			Entry->TryGetStringField(TEXT("orderFig"), Plate.OrderFigure);
			Entry->TryGetStringField(TEXT("orderWords"), Plate.OrderWords);
			Entry->TryGetNumberField(TEXT("up"), Plate.UpMonths);
			Entry->TryGetStringField(TEXT("upFig"), Plate.UpFigure);
			Entry->TryGetStringField(TEXT("upWords"), Plate.UpWords);
			Entry->TryGetNumberField(TEXT("dmg"), Plate.Damage);
			Entry->TryGetStringField(TEXT("dmgFig"), Plate.DamageFigure);
			Entry->TryGetStringField(TEXT("dmgWords"), Plate.DamageWords);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Going = nullptr;
	if (Object->TryGetArrayField(TEXT("going"), Going))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Going)
		{
			Out.Going.Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Wrecked = nullptr;
	if (Object->TryGetArrayField(TEXT("wrecked"), Wrecked))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Wrecked)
		{
			Out.Wrecked.Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* WorksLeft = nullptr;
	if (Object->TryGetArrayField(TEXT("worksLeft"), WorksLeft))
	{
		for (const TSharedPtr<FJsonValue>& Value : *WorksLeft)
		{
			Out.WorksLeft.Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Joins = nullptr;
	if (Object->TryGetArrayField(TEXT("joins"), Joins))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Joins)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			int32 Region = INDEX_NONE, Count = 0;
			TArray<FString> Whose;
			if (Entry.IsValid() && Entry->TryGetNumberField(TEXT("region"), Region) && Entry->TryGetNumberField(TEXT("n"), Count))
			{
				Entry->TryGetStringArrayField(TEXT("nats"), Whose);
				Out.Joins.Add(Region, FString::Printf(TEXT("%d %s"), Count, *FString::Join(Whose, TEXT(" and "))));
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Unseen = nullptr;
	if (Object->TryGetArrayField(TEXT("unseen"), Unseen))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Unseen)
		{
			Out.Unseen.Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Glimpsed = nullptr;
	if (Object->TryGetArrayField(TEXT("glimpsed"), Glimpsed))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Glimpsed)
		{
			Out.Glimpsed.Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Free = nullptr;
	if (Object->TryGetArrayField(TEXT("free"), Free))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Free)
		{
			int32 Capital = INDEX_NONE;
			if (Value->AsObject().IsValid() && Value->AsObject()->TryGetNumberField(TEXT("region"), Capital))
			{
				Out.FreeCapitals.Add(Capital);
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Airfields = nullptr;
	if (Object->TryGetArrayField(TEXT("airfields"), Airfields))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Airfields)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FViewAirfield& Field = Out.Airfields.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("region"), Field.Region);
			Entry->TryGetStringField(TEXT("nat"), Field.Nation);
			Entry->TryGetStringField(TEXT("side"), Field.Side);
			Entry->TryGetStringField(TEXT("word"), Field.Word);
			Entry->TryGetNumberField(TEXT("size"), Field.Size);
			Entry->TryGetNumberField(TEXT("holds"), Field.Holds);
			Entry->TryGetNumberField(TEXT("groups"), Field.Groups);
			Entry->TryGetNumberField(TEXT("aircraft"), Field.Aircraft);
			Entry->TryGetBoolField(TEXT("crowded"), Field.bCrowded);
			Entry->TryGetBoolField(TEXT("shut"), Field.bShut);
			Entry->TryGetBoolField(TEXT("mend"), Field.bMending);
			Entry->TryGetStringField(TEXT("tier"), Field.Tier);
		}
	}
	Out.BasedGroups.Reset();
	if (Out.Airfields.Num() > 0)
	{
		Out.BasedGroups = Out.AirFleets; // (for the airfield's card: each group based there, by name)
		TArray<FViewAirFleet> Stood;
		for (const FViewAirFleet& Group : Out.AirFleets)
		{
			FViewAirFleet* Same = Stood.FindByPredicate([&Group](const FViewAirFleet& Other) { return Other.Base == Group.Base && Other.Nation == Group.Nation; });
			if (Same)
			{
				Same->Fighters += Group.Fighters;
				Same->Bombers += Group.Bombers;
				++Same->Groups;
			}
			else
			{
				Stood.Add(Group);
			}
		}
		Out.AirFleets = MoveTemp(Stood);
		// (Brian, 24 November 2026: "when i have a unit on an allied airfield there is no way to know its there".) On a
		// field another country holds, the player's own groups are counted apart, for the map to show them as his.
		for (FViewAirfield& Field : Out.Airfields)
		{
			if (Out.Human.IsEmpty() || Field.Nation == Out.Human || Field.Groups <= 0)
			{
				continue;
			}
			for (const FViewAirFleet& Group : Out.AirFleets)
			{
				if (Group.Base == Field.Region && Group.Nation == Out.Human)
				{
					Field.YourGroups += Group.Groups;
					Field.YourAircraft += Group.Fighters + Group.Bombers;
					Field.YourNation = Out.Human;
				}
			}
		}
	}

	{
		// (the ownership paint: the sieges, for the besieger's colour on the field)
		Out.SiegePaint.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Painted = nullptr;
		if (Object->TryGetArrayField(TEXT("siegePaint"), Painted))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Painted)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				if (!Value->TryGetObject(Entry))
				{
					continue;
				}
				FCampaignView::FSiegePaint& Siege = Out.SiegePaint.AddDefaulted_GetRef();
				(*Entry)->TryGetNumberField(TEXT("r"), Siege.Region);
				(*Entry)->TryGetStringField(TEXT("by"), Siege.By);
				(*Entry)->TryGetNumberField(TEXT("p"), Siege.Progress);
				int32 Closed = 0;
				(*Entry)->TryGetNumberField(TEXT("closed"), Closed);
				Siege.bClosed = Closed != 0;
				const TArray<TSharedPtr<FJsonValue>>* Sides = nullptr;
				if ((*Entry)->TryGetArrayField(TEXT("from"), Sides))
				{
					for (const TSharedPtr<FJsonValue>& Side : *Sides)
					{
						Siege.From.Add(FMath::RoundToInt32(Side->AsNumber()));
					}
				}
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
	if (Object->TryGetArrayField(TEXT("regions"), Regions))
	{
		Out.RegionHolder.SetNum(Regions->Num());
		Out.RegionOwner.SetNum(Regions->Num());
		Out.RegionStanding.SetNum(Regions->Num());
		Out.RegionWorks.Init(FIntVector4(0, 0, 0, 0), Regions->Num());
		for (const TSharedPtr<FJsonValue>& Value : *Regions)
		{
			const TSharedPtr<FJsonObject> Region = Value->AsObject();
			const int32 Id = Region->GetIntegerField(TEXT("id"));
			if (Out.RegionHolder.IsValidIndex(Id))
			{
				Out.RegionHolder[Id] = Region->GetStringField(TEXT("ctrl"));
				Region->TryGetStringField(TEXT("own"), Out.RegionOwner[Id]);
				Region->TryGetStringField(TEXT("stand"), Out.RegionStanding[Id]);
				const TArray<TSharedPtr<FJsonValue>>* Built = nullptr;
				if (Region->TryGetArrayField(TEXT("w"), Built) && Built->Num() >= 4)
				{
					Out.RegionWorks[Id] = FIntVector4(static_cast<int32>((*Built)[0]->AsNumber()), static_cast<int32>((*Built)[1]->AsNumber()), static_cast<int32>((*Built)[2]->AsNumber()), static_cast<int32>((*Built)[3]->AsNumber()));
				}
			}
		}
	}

	const TSharedPtr<FJsonObject>* Treasury = nullptr;
	if (Object->TryGetObjectField(TEXT("treasury"), Treasury)) // null when nobody is being led
	{
		Out.Treasury.bValid = true;
		(*Treasury)->TryGetNumberField(TEXT("pp"), Out.Treasury.Production);
		(*Treasury)->TryGetNumberField(TEXT("income"), Out.Treasury.Income);
		(*Treasury)->TryGetNumberField(TEXT("pc"), Out.Treasury.Political);
		Out.Treasury.Orders = ReadOrders(*Treasury, TEXT("orders"));
		(*Treasury)->TryGetBoolField(TEXT("budget"), Out.Treasury.bBudget);
		(*Treasury)->TryGetNumberField(TEXT("money"), Out.Treasury.Money);
		(*Treasury)->TryGetNumberField(TEXT("gross"), Out.Treasury.Gross);
		(*Treasury)->TryGetNumberField(TEXT("upkeep"), Out.Treasury.Upkeep);
		(*Treasury)->TryGetNumberField(TEXT("net"), Out.Treasury.Net);
		(*Treasury)->TryGetNumberField(TEXT("debt"), Out.Treasury.Debt);
	}

	const TArray<TSharedPtr<FJsonValue>>* Corps = nullptr;
	if (Object->TryGetArrayField(TEXT("corps"), Corps))
	{
		Out.Corps.Reserve(Corps->Num());
		for (const TSharedPtr<FJsonValue>& Value : *Corps)
		{
			const TSharedPtr<FJsonObject> Unit = Value->AsObject();
			FViewCorps C;
			C.Id = Unit->GetIntegerField(TEXT("id"));
			C.Nation = Unit->GetStringField(TEXT("nat"));
			C.Name = Unit->GetStringField(TEXT("name"));
			C.Kind = Unit->GetStringField(TEXT("kind"));
			Unit->TryGetStringField(TEXT("elite"), C.Elite);
			Unit->TryGetBoolField(TEXT("dim"), C.bDim);
			Unit->TryGetStringField(TEXT("gen"), C.General); // null when nobody is named
			Unit->TryGetStringField(TEXT("posture"), C.Posture);
			Unit->TryGetBoolField(TEXT("dug"), C.bDug);
			{
				double Months = 0.0;
				Unit->TryGetNumberField(TEXT("dugFor"), Months);
				C.DugFor = FMath::RoundToInt32(Months);
				double Works = -1.0;
				Unit->TryGetNumberField(TEXT("works"), Works);
				C.Works = static_cast<float>(Works);
				// (where months are counted the works are finished when they have had them all, and attacks wear them
				// down again; where they are not, to be dug in is to have finished)
				C.bWorksDone = C.bDug && (Works >= 0.0 ? Works >= Out.DugFull - 0.05 : (C.DugFor == 0 || C.DugFor >= Out.DugFull));
			}
			Unit->TryGetBoolField(TEXT("supplied"), C.bSupplied);
			Unit->TryGetBoolField(TEXT("town"), C.bInTown);
			C.Divisions = Unit->GetIntegerField(TEXT("divs"));
			C.Strength = static_cast<float>(Unit->GetNumberField(TEXT("str")));
			// Rules that do not count heads (the stand-in) are taken at a full division's strength.
			if (!Unit->TryGetNumberField(TEXT("men"), C.Men))
			{
				C.Men = FMath::RoundToInt32(C.Divisions * C.Strength * 15000.0f);
			}
			C.Cohesion = static_cast<float>(Unit->GetNumberField(TEXT("org")));
			C.Region = Unit->GetIntegerField(TEXT("reg"));
			C.Moves = Unit->GetIntegerField(TEXT("ap"));
			C.MovesMax = Unit->GetIntegerField(TEXT("apMax"));
			{
				int32 Marching = INDEX_NONE;
				if (Unit->TryGetNumberField(TEXT("marchTo"), Marching) && Marching >= 0)
				{
					C.MarchTo = Marching;
					Unit->TryGetNumberField(TEXT("marchMonths"), C.MarchMonths);
					Unit->TryGetBoolField(TEXT("marchNext"), C.bMarchNext);
				}
			}
			Unit->TryGetBoolField(TEXT("canOrder"), C.bCanOrder);
			Unit->TryGetStringField(TEXT("noOrders"), C.NoOrders);
			Unit->TryGetStringField(TEXT("garrison"), C.Garrison);
			Unit->TryGetNumberField(TEXT("xp"), C.Experience);
			Unit->TryGetStringField(TEXT("grade"), C.Grade);
			Unit->TryGetBoolField(TEXT("lean"), C.bLean);
			// (Release 2: the battle schools.) Only rules that have them say 'school' at all.
			C.bSchoolKnown = Unit->TryGetBoolField(TEXT("school"), C.bSchool);
			Unit->TryGetStringField(TEXT("schoolTo"), C.SchoolTo);
			Unit->TryGetStringField(TEXT("schoolWhy"), C.SchoolWhy);
			// (Release 2, breakup.ts) Sent only for his own corps, and only where breaking corps up is on.
			C.bBreakKnown = Unit->TryGetStringField(TEXT("breakWhy"), C.BreakWhy);
			{
				double Rejoin = -1.0;
				Unit->TryGetNumberField(TEXT("rejoinId"), Rejoin);
				C.RejoinId = Rejoin >= 0.0 ? static_cast<int32>(Rejoin) : INDEX_NONE;
				Unit->TryGetStringField(TEXT("rejoinName"), C.RejoinName);
			}
			// (Release 2, rearguard.ts) Sent only for his own corps, and only where rearguards are on.
			C.bRearKnown = Unit->TryGetStringField(TEXT("rearWhy"), C.RearWhy);
			Unit->TryGetBoolField(TEXT("rear"), C.bRear);
			Unit->TryGetStringField(TEXT("ally"), C.Ally);
			Unit->TryGetStringField(TEXT("vol"), C.Vol); // R2 bargains hook: a volunteer legion's home (sent only when set)
			Unit->TryGetBoolField(TEXT("near"), C.bNear);
			Unit->TryGetNumberField(TEXT("stale"), C.Stale);
			{
				// (Version 84) the weapons it carries, and whether it is digging with no works to show yet
				const TArray<TSharedPtr<FJsonValue>>* Carried = nullptr;
				if (Unit->TryGetArrayField(TEXT("carry"), Carried))
				{
					for (const TSharedPtr<FJsonValue>& Each : *Carried)
					{
						C.Carry.Add(FMath::RoundToInt32(Each->AsNumber()));
					}
				}
				Unit->TryGetBoolField(TEXT("digging"), C.bDigging);
			}
			Unit->TryGetStringField(TEXT("nick"), C.Nickname); // null until it has earned one
			Unit->TryGetStringField(TEXT("mark"), C.Marked); // (release 2, the marked army) absent unless it took one of his towns
			Unit->TryGetBoolField(TEXT("ready"), C.bReady); // (release 2, new units ready) absent unless handed over as his month opened
			C.Honours = ReadRibbons(Unit, TEXT("honours"));
			{
				double Morale = -1.0, Rest = -1.0;
				Unit->TryGetNumberField(TEXT("mor"), Morale);
				Unit->TryGetNumberField(TEXT("rest"), Rest);
				C.Morale = static_cast<float>(Morale);
				C.Rest = static_cast<float>(Rest);
				Unit->TryGetStringField(TEXT("morWord"), C.MoraleWord);
				Unit->TryGetStringField(TEXT("restWord"), C.RestWord);
				Unit->TryGetStringField(TEXT("trend"), C.Trend);
				Unit->TryGetStringField(TEXT("moodLine"), C.MoodLine);
				Unit->TryGetStringField(TEXT("restLine"), C.RestLine);
				Unit->TryGetStringField(TEXT("service"), C.Service);
				Unit->TryGetBoolField(TEXT("resting"), C.bResting);
				Unit->TryGetStringArrayField(TEXT("why"), C.Why);
				const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
				if (Unit->TryGetArrayField(TEXT("dv"), Parts))
				{
					for (const TSharedPtr<FJsonValue>& PartValue : *Parts)
					{
						const TSharedPtr<FJsonObject> Part = PartValue->AsObject();
						if (!Part.IsValid())
						{
							continue;
						}
						FViewDivision& Division = C.Parts.AddDefaulted_GetRef();
						Part->TryGetStringField(TEXT("name"), Division.Name);
						Part->TryGetStringField(TEXT("full"), Division.Full);
						Part->TryGetStringField(TEXT("kind"), Division.Kind);
						Part->TryGetStringField(TEXT("grade"), Division.Grade);
						double Left = 1.0;
						Part->TryGetNumberField(TEXT("str"), Left);
						Division.Strength = static_cast<float>(Left);
						Part->TryGetNumberField(TEXT("men"), Division.Men);
						Part->TryGetNumberField(TEXT("stripes"), Division.Stripes);
						Part->TryGetBoolField(TEXT("hard"), Division.bHardened);
					}
				}
			}
			Out.Corps.Add(MoveTemp(C));
		}
	}
	return Out;
}

void FCampaignSession::TakeView(const TSharedPtr<FJsonObject>& Holder)
{
	const TSharedPtr<FJsonObject>* NewView = nullptr;
	if (Holder.IsValid() && Holder->TryGetObjectField(TEXT("view"), NewView))
	{
		View = ParseView(*NewView);
		OnViewChanged.Broadcast();
	}
}

namespace
{
	TArray<FCorpsMove> ReadMoves(const TSharedPtr<FJsonObject>& Object)
	{
		TArray<FCorpsMove> Out;
		const TArray<TSharedPtr<FJsonValue>>* Moved = nullptr;
		if (Object->TryGetArrayField(TEXT("moved"), Moved))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Moved)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FCorpsMove M;
				M.CorpsId = Entry->GetIntegerField(TEXT("id"));
				for (const TSharedPtr<FJsonValue>& Step : Entry->GetArrayField(TEXT("path")))
				{
					M.Path.Add(FMath::RoundToInt32(Step->AsNumber()));
				}
				Out.Add(MoveTemp(M));
			}
		}
		return Out;
	}

	TArray<FBattleUnit> ReadUnits(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		TArray<FBattleUnit> Out;
		const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
		if (Object->TryGetArrayField(Field, Units))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Units)
			{
				const TSharedPtr<FJsonObject> Unit = Value->AsObject();
				FBattleUnit U;
				Unit->TryGetStringField(TEXT("nat"), U.Nation);
				Unit->TryGetStringField(TEXT("name"), U.Name);
				Unit->TryGetStringField(TEXT("kind"), U.Kind);
				Unit->TryGetStringField(TEXT("gen"), U.General);
				Unit->TryGetStringField(TEXT("fate"), U.Fate);
				Unit->TryGetNumberField(TEXT("divs"), U.Divisions);
				Unit->TryGetNumberField(TEXT("men"), U.Men);
				Unit->TryGetNumberField(TEXT("lost"), U.Lost);
				Out.Add(MoveTemp(U));
			}
		}
		return Out;
	}
}

TArray<FReachRegion> FCampaignSession::Reach(const TArray<int32>& CorpsIds, TArray<int32>* OutDanger, TMap<int32, int32>* OutFar, TArray<int32>* OutStanding, bool* OutSpent)
{
	TArray<FReachRegion> Out;
	if (OutSpent)
	{
		*OutSpent = false;
	}
	if (OutDanger)
	{
		OutDanger->Reset();
	}
	if (OutFar)
	{
		OutFar->Reset();
	}
	if (OutStanding)
	{
		OutStanding->Reset();
	}
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("reach"), ReachArgs(CorpsIds), Object))
	{
		return Out;
	}
	const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
	if (Object->TryGetArrayField(TEXT("regions"), Regions))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Regions)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FReachRegion R;
			R.Region = Entry->GetIntegerField(TEXT("id"));
			R.Cost = Entry->GetIntegerField(TEXT("cost"));
			R.Previous = Entry->GetIntegerField(TEXT("prev"));
			Entry->TryGetBoolField(TEXT("attack"), R.bAttack);
			Entry->TryGetBoolField(TEXT("take"), R.bTake);
			Entry->TryGetStringField(TEXT("war"), R.War);
			Out.Add(R);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Danger = nullptr;
	if (OutDanger && Object->TryGetArrayField(TEXT("danger"), Danger))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Danger)
		{
			OutDanger->Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	// (release 1) Friendly ground beyond this month's reach, each province with about how many months the march
	// would take; and the road still ahead of a corps already on a standing march.
	const TArray<TSharedPtr<FJsonValue>>* Far = nullptr;
	if (OutFar && Object->TryGetArrayField(TEXT("far"), Far))
	{
		for (int32 K = 0; K + 1 < Far->Num(); K += 2)
		{
			OutFar->Add(FMath::RoundToInt32((*Far)[K]->AsNumber()), FMath::RoundToInt32((*Far)[K + 1]->AsNumber()));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Standing = nullptr;
	if (OutStanding && Object->TryGetArrayField(TEXT("standing"), Standing))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Standing)
		{
			OutStanding->Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	if (OutSpent)
	{
		Object->TryGetBoolField(TEXT("spent"), *OutSpent);
	}
	return Out;
}

int32 FCampaignSession::NextArmy(TArray<int32>& InOutSeen, const FVector2D& From)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("nextArmy"), FString::Printf(TEXT("[[%s],%.1f,%.1f]"), *IdList(InOutSeen), From.X, From.Y), Object))
	{
		return INDEX_NONE;
	}
	int32 Region = INDEX_NONE;
	Object->TryGetNumberField(TEXT("reg"), Region);
	const TArray<TSharedPtr<FJsonValue>>* Seen = nullptr;
	if (Object->TryGetArrayField(TEXT("seen"), Seen))
	{
		InOutSeen.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *Seen)
		{
			InOutSeen.Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	return Region < 0 ? INDEX_NONE : Region;
}

FString FCampaignSession::Stand(int32 RegionId, bool bOn)
{
	FString Now;
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("stand"), FString::Printf(TEXT("[%d,%s]"), RegionId, bOn ? TEXT("true") : TEXT("false")), Object))
	{
		Object->TryGetStringField(TEXT("stands"), Now);
		TakeView(Object);
	}
	return Now;
}

void FCampaignSession::StageCut(int32 RegionId, const FString& Cause, const FString& How)
{
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("stageCut"), FString::Printf(TEXT("[%d,\"%s\",\"%s\"]"), RegionId, *Cause, *How), Object))
	{
		TakeView(Object);
	}
}

void FCampaignSession::StageGuns(int32 RegionId, int32 Level)
{
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("stageGuns"), FString::Printf(TEXT("[%d,%d]"), RegionId, Level), Object))
	{
		TakeView(Object);
	}
}

int32 FCampaignSession::StageAgent(int32 RegionId, const FString& Mission)
{
	int32 Id = INDEX_NONE;
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("stageAgent"), FString::Printf(TEXT("[%d,%s]"), RegionId, *JsonQuote(Mission)), Object))
	{
		Object->TryGetNumberField(TEXT("id"), Id);
		TakeView(Object);
	}
	return Id;
}

void FCampaignSession::StageCaught(int32 RegionId)
{
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("stageCaught"), FString::Printf(TEXT("[%d]"), RegionId), Object))
	{
		TakeView(Object);
	}
}

FString FCampaignSession::WhyNotSendAgent(int32 AgentId, int32 RegionId)
{
	FString Why;
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("agentWhy"), FString::Printf(TEXT("[%d,%d]"), AgentId, RegionId), Object))
	{
		Object->TryGetStringField(TEXT("why"), Why);
	}
	return Why;
}

FString FCampaignSession::StageRecon(int32 RegionId)
{
	FString Said;
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("stageRecon"), FString::Printf(TEXT("[%d]"), RegionId), Object))
	{
		Object->TryGetStringField(TEXT("said"), Said);
		TakeView(Object);
	}
	return Said;
}

FString FCampaignSession::Stands(int32 RegionId)
{
	FString Now;
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("stands"), RegionArgs(RegionId), Object))
	{
		Object->TryGetStringField(TEXT("stands"), Now);
	}
	return Now;
}

FMarchPlan FCampaignSession::PlanMarch(const TArray<int32>& CorpsIds, int32 RegionId, const FString& Army, const TArray<int32>& ArmyIds)
{
	FMarchPlan Plan;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("marchPlan"), FString::Printf(TEXT("[[%s],%d%s]"), *IdList(CorpsIds), RegionId, *ArmyArgs(Army, ArmyIds)), Object))
	{
		return Plan;
	}
	Object->TryGetBoolField(TEXT("ok"), Plan.bOk);
	Object->TryGetBoolField(TEXT("spent"), Plan.bSpent);
	Object->TryGetBoolField(TEXT("now"), Plan.bNow);
	Object->TryGetNumberField(TEXT("months"), Plan.Months);
	Object->TryGetStringField(TEXT("line"), Plan.Line);
	const TArray<TSharedPtr<FJsonValue>>* Road = nullptr;
	if (Object->TryGetArrayField(TEXT("road"), Road))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Road)
		{
			Plan.Road.Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	return Plan;
}

TArray<FCorpsMove> FCampaignSession::Move(const TArray<int32>& CorpsIds, int32 RegionId, FString* OutNote, const FString& Army, const TArray<int32>& ArmyIds)
{
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("move"), FString::Printf(TEXT("[[%s],%d%s]"), *IdList(CorpsIds), RegionId, *ArmyArgs(Army, ArmyIds)), Object))
	{
		return TArray<FCorpsMove>();
	}
	TArray<FCorpsMove> Out = ReadMoves(Object);
	if (OutNote)
	{
		Object->TryGetStringField(TEXT("note"), *OutNote);
	}
	TakeView(Object);
	return Out;
}

namespace
{
	/** The lines of a Battle Ticket, from a forecast or from a battle's report. */
	TArray<FTicketLine> ReadTicket(const TSharedPtr<FJsonObject>& Object)
	{
		TArray<FTicketLine> Out;
		const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
		if (Object.IsValid() && Object->TryGetArrayField(TEXT("lines"), Lines))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Lines)
			{
				const TSharedPtr<FJsonObject> Line = Value->AsObject();
				if (!Line.IsValid())
				{
					continue;
				}
				FTicketLine L;
				Line->TryGetStringField(TEXT("label"), L.Label);
				Line->TryGetStringField(TEXT("value"), L.Value);
				Line->TryGetStringField(TEXT("effect"), L.Effect);
				Out.Add(MoveTemp(L));
			}
		}
		return Out;
	}

	/** A list of corps by their numbers. */
	void ReadIds(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<int32>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object->TryGetArrayField(Field, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				Out.Add(static_cast<int32>(Value->AsNumber()));
			}
		}
	}
}

namespace
{
	/** An attack's forecast as the rules give it: previewAttack's, and takeOutlook's where a town's guard will fight. */
	FAttackPreview ReadPreview(const TSharedPtr<FJsonObject>& Object, int32 RegionId);
	/** A march on a town or the storm of one, as the rules give it (takeOutlook, stormLook): its words, and its forecasts where there is a fight. */
	FAttackPreview ReadMarch(const TSharedPtr<FJsonObject>& Object, int32 RegionId);
}

FAttackPreview FCampaignSession::PreviewAttack(const TArray<int32>& CorpsIds, int32 RegionId, bool bOpening, bool bGlance)
{
	TSharedPtr<FJsonObject> Object;
	const FString Arguments = bGlance
		? FString::Printf(TEXT("[[%s],%d,%s,true]"), *IdList(CorpsIds), RegionId, bOpening ? TEXT("true") : TEXT("false"))
		: FString::Printf(TEXT("[[%s],%d,%s]"), *IdList(CorpsIds), RegionId, bOpening ? TEXT("true") : TEXT("false"));
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("previewAttack"), Arguments, Object))
	{
		return FAttackPreview(); // also what comes back when the rules answer null: there is nobody there to attack
	}
	return ReadPreview(Object, RegionId);
}

FAttackPreview FCampaignSession::PreviewTake(const TArray<int32>& CorpsIds, int32 RegionId)
{
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("takeOutlook"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		return FAttackPreview(); // (the rules answer null: a plain march, or a field battle)
	}
	return ReadMarch(Object, RegionId);
}

FAttackPreview FCampaignSession::PreviewStorm(int32 RegionId)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("stormLook"), FString::Printf(TEXT("[%d]"), RegionId), Object))
	{
		return FAttackPreview(); // (the rules answer null: no army of the player's stands round a town there)
	}
	FString Refused;
	if (Object->TryGetStringField(TEXT("refused"), Refused) && !Refused.IsEmpty())
	{
		FAttackPreview Out;
		Out.Region = RegionId;
		Out.Refused = Refused; // (one assault a town a month, and the like: the rules' own sentence)
		return Out;
	}
	return ReadMarch(Object, RegionId);
}

FAttackPreview FCampaignSession::PreviewSally(int32 RegionId)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("sallyLook"), FString::Printf(TEXT("[%d]"), RegionId), Object))
	{
		return FAttackPreview(); // (the rules answer null: no garrison of the player's is shut in there)
	}
	FString Refused;
	if (Object->TryGetStringField(TEXT("refused"), Refused) && !Refused.IsEmpty())
	{
		FAttackPreview Out;
		Out.Region = RegionId;
		Out.Refused = Refused; // (one sortie a town a month, and the like: the rules' own sentence)
		return Out;
	}
	FAttackPreview Out = ReadPreview(Object, RegionId);
	Object->TryGetStringField(TEXT("town"), Out.Town);
	return Out;
}

FAttackResult FCampaignSession::Sally(int32 RegionId)
{
	return Fight(TEXT("sally"), FString::Printf(TEXT("[%d]"), RegionId));
}

FAttackResult FCampaignSession::Storm(int32 RegionId)
{
	return Fight(TEXT("storm"), FString::Printf(TEXT("[%d]"), RegionId));
}

namespace
{
FAttackPreview ReadMarch(const TSharedPtr<FJsonObject>& Object, int32 RegionId)
{
	bool bForecast = false;
	Object->TryGetBoolField(TEXT("forecast"), bForecast);
	// (where the guard will fight, the assault as the army arrives reads as an attack's forecast does)
	FAttackPreview Out = bForecast ? ReadPreview(Object, RegionId) : FAttackPreview();
	Out.bValid = true;
	Out.Region = RegionId;
	Out.bForecast = bForecast;
	Object->TryGetStringField(TEXT("meets"), Out.Meets);
	Object->TryGetStringField(TEXT("line"), Out.MarchLine);
	Object->TryGetStringField(TEXT("town"), Out.Town);
	Object->TryGetStringField(TEXT("forces"), Out.Forces);
	Object->TryGetStringField(TEXT("order"), Out.OrderWord);
	Object->TryGetStringField(TEXT("guardName"), Out.GuardName);
	Object->TryGetStringField(TEXT("guardLine"), Out.GuardLine);
	Object->TryGetStringField(TEXT("guardWho"), Out.GuardWho);
	Object->TryGetStringField(TEXT("favours"), Out.Favours);
	Object->TryGetStringField(TEXT("nowSays"), Out.NowSays);
	Object->TryGetStringField(TEXT("laterSays"), Out.LaterSays);
	Object->TryGetStringField(TEXT("prize"), Out.Prize);
	const TSharedPtr<FJsonObject>* Later = nullptr;
	if (Object->TryGetObjectField(TEXT("prepared"), Later))
	{
		double Odds = 0.0, Low = 0.0, High = 0.0;
		(*Later)->TryGetNumberField(TEXT("odds"), Odds);
		Low = High = Odds;
		(*Later)->TryGetNumberField(TEXT("lo"), Low);
		(*Later)->TryGetNumberField(TEXT("hi"), High);
		(*Later)->TryGetBoolField(TEXT("exact"), Out.bLaterExact);
		(*Later)->TryGetStringField(TEXT("chanceWords"), Out.LaterChanceWords);
		Out.bLater = true;
		Out.LaterOdds = static_cast<float>(Odds);
		Out.LaterOddsLow = static_cast<float>(Low);
		Out.LaterOddsHigh = static_cast<float>(High);
	}
	Object->TryGetStringField(TEXT("nowLabel"), Out.NowLabel);
	Object->TryGetStringField(TEXT("laterLabel"), Out.LaterLabel);
	Object->TryGetStringField(TEXT("waitSays"), Out.WaitSays);
	return Out;
}

FAttackPreview ReadPreview(const TSharedPtr<FJsonObject>& Object, int32 RegionId)
{
	FAttackPreview Out;
	Out.Region = RegionId;
	if (Object->TryGetStringField(TEXT("refused"), Out.Refused) && !Out.Refused.IsEmpty())
	{
		return Out; // the troops there stand on a neutral's ground: no attack until war is declared on it
	}
	Out.bValid = true;
	Object->TryGetStringField(TEXT("alone"), Out.Alone); // (the pick cannot go: the card shows it, and says why)
	{
		double Even = 1.2;
		Object->TryGetNumberField(TEXT("even"), Even);
		Out.Even = static_cast<float>(Even);
	}
	Out.Attack = static_cast<float>(Object->GetNumberField(TEXT("attack")));
	Out.Defence = static_cast<float>(Object->GetNumberField(TEXT("defence")));
	Out.Odds = static_cast<float>(Object->GetNumberField(TEXT("odds")));
	Object->TryGetStringField(TEXT("prize"), Out.Prize);
	{
		double Low = Out.Odds, High = Out.Odds;
		Object->TryGetBoolField(TEXT("exact"), Out.bExact);
		Object->TryGetNumberField(TEXT("lo"), Low);
		Object->TryGetNumberField(TEXT("hi"), High);
		Out.OddsLow = static_cast<float>(Low);
		Out.OddsHigh = static_cast<float>(High);
		Object->TryGetStringField(TEXT("note"), Out.Note);
		Object->TryGetStringArrayField(TEXT("allies"), Out.Allies);
	}
	{
		bool bHasChance = false;
		if (Object->TryGetBoolField(TEXT("hasChance"), bHasChance) && bHasChance)
		{
			Object->TryGetNumberField(TEXT("chancePct"), Out.ChancePct);
			Object->TryGetNumberField(TEXT("chancePctLo"), Out.ChancePctLow);
			Object->TryGetNumberField(TEXT("chancePctHi"), Out.ChancePctHigh);
		}
		Object->TryGetStringField(TEXT("mass"), Out.Mass);
		ReadIds(Object, TEXT("massIds"), Out.MassIds);
		// (Version 63) the chance in words, the bill before the order, and how the card opened
		Object->TryGetStringField(TEXT("chanceWords"), Out.ChanceWords);
		Object->TryGetStringField(TEXT("tone"), Out.VerdictTone);
		Object->TryGetStringField(TEXT("verdict"), Out.VerdictWord);
		Object->TryGetStringArrayField(TEXT("cost"), Out.Cost);
		Object->TryGetStringField(TEXT("opened"), Out.Opened);
		ReadIds(Object, TEXT("ids"), Out.Ids);
		ReadIds(Object, TEXT("pickedIds"), Out.PickedIds);
		{
			// (Version 84) WHAT YOU FACE
			const TArray<TSharedPtr<FJsonValue>>* Faced = nullptr;
			if (Object->TryGetArrayField(TEXT("face"), Faced))
			{
				for (const TSharedPtr<FJsonValue>& Each : *Faced)
				{
					const TSharedPtr<FJsonObject> Entry = Each->AsObject();
					if (Entry.IsValid())
					{
						FFaceLine& Line = Out.Face.AddDefaulted_GetRef();
						Entry->TryGetStringField(TEXT("sign"), Line.Sign);
						Entry->TryGetStringField(TEXT("text"), Line.Text);
						Entry->TryGetBoolField(TEXT("unknown"), Line.bUnknown);
					}
				}
			}
		}
		// (Version 64) soften the line first
		const TSharedPtr<FJsonObject>* Soften = nullptr;
		if (Object->TryGetObjectField(TEXT("soften"), Soften))
		{
			Out.bSoften = true;
			(*Soften)->TryGetStringField(TEXT("head"), Out.SoftenHead);
			(*Soften)->TryGetStringField(TEXT("done"), Out.SoftenDone);
			const TSharedPtr<FJsonObject>* Probe = nullptr;
			if ((*Soften)->TryGetObjectField(TEXT("probe"), Probe))
			{
				(*Probe)->TryGetStringField(TEXT("label"), Out.ProbeLabel);
				(*Probe)->TryGetStringField(TEXT("text"), Out.ProbeText);
				(*Probe)->TryGetBoolField(TEXT("ok"), Out.bProbeOk);
				(*Probe)->TryGetNumberField(TEXT("corps"), Out.ProbeCorps);
			}
			const TSharedPtr<FJsonObject>* Bomb = nullptr;
			if ((*Soften)->TryGetObjectField(TEXT("bomb"), Bomb))
			{
				Out.bBomb = true;
				(*Bomb)->TryGetStringField(TEXT("text"), Out.BombText);
				(*Bomb)->TryGetStringField(TEXT("open"), Out.BombOpen);
				(*Bomb)->TryGetBoolField(TEXT("ok"), Out.bBombOk);
			}
		}
	}
	Out.Lines = ReadTicket(Object);
	Out.Attackers = ReadUnits(Object, TEXT("attackers"));
	Out.Defenders = ReadUnits(Object, TEXT("defenders"));
	return Out;
}
}

FAttackResult FCampaignSession::Attack(const TArray<int32>& CorpsIds, int32 RegionId)
{
	return CorpsIds.Num() == 0 ? FAttackResult() : Fight(TEXT("attack"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId));
}

FString FCampaignSession::GarrisonWhy(int32 RegionId)
{
	FString Why;
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("garrisonWhy"), FString::Printf(TEXT("[%d]"), RegionId), Object))
	{
		Object->TryGetStringField(TEXT("why"), Why);
	}
	return Why;
}

FString FCampaignSession::WhyNotLead(const TArray<int32>& CorpsIds, int32 RegionId)
{
	FString Why;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("lead"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		Object->TryGetStringField(TEXT("why"), Why);
	}
	return Why;
}

FString FCampaignSession::WhyNotBySea(const TArray<int32>& CorpsIds, int32 RegionId)
{
	FString Why;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("bySeaWhy"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		Object->TryGetStringField(TEXT("why"), Why);
	}
	return Why;
}

FApproachPlan FCampaignSession::PlanApproach(const TArray<int32>& CorpsIds, int32 RegionId)
{
	FApproachPlan Plan;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("approach"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		return Plan;
	}
	Object->TryGetBoolField(TEXT("ok"), Plan.bOk);
	Object->TryGetStringField(TEXT("name"), Plan.Name);
	Object->TryGetStringField(TEXT("none"), Plan.None);
	Object->TryGetStringField(TEXT("from"), Plan.From);
	Object->TryGetStringField(TEXT("ours"), Plan.Ours);
	Object->TryGetStringField(TEXT("foe"), Plan.Foe);
	Object->TryGetStringField(TEXT("foeName"), Plan.FoeName);
	Object->TryGetStringField(TEXT("foeLine"), Plan.FoeLine);
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (Object->TryGetArrayField(TEXT("rows"), Rows))
	{
		for (const TSharedPtr<FJsonValue>& Each : *Rows)
		{
			const TSharedPtr<FJsonObject> Row = Each->AsObject();
			if (!Row.IsValid())
			{
				continue;
			}
			FApproachRow& Way = Plan.Rows.AddDefaulted_GetRef();
			Row->TryGetNumberField(TEXT("id"), Way.Region);
			Row->TryGetStringField(TEXT("name"), Way.Name);
			Row->TryGetNumberField(TEXT("cost"), Way.Cost);
			Row->TryGetNumberField(TEXT("left"), Way.Left);
			Row->TryGetBoolField(TEXT("attack"), Way.bAttack);
			Row->TryGetStringField(TEXT("arrive"), Way.Arrive);
			{
				// (The UI director, check 19: the ground by its label, "open country", never the rules' key "plains".)
				Row->TryGetStringField(TEXT("river"), Way.River);
				Row->TryGetStringField(TEXT("riverWords"), Way.RiverWords);
				Row->TryGetStringField(TEXT("ground"), Way.Ground);
				Way.How = Way.RiverWords + TEXT(" \u00B7 ") + FCampaignMapData::TerrainLabel(Way.Ground).ToLower();
			}
			Row->TryGetStringField(TEXT("says"), Way.Says);
			const TArray<TSharedPtr<FJsonValue>>* Road = nullptr;
			if (Row->TryGetArrayField(TEXT("road"), Road))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Road)
				{
					Way.Road.Add(FMath::RoundToInt32(Value->AsNumber()));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Going = nullptr;
			if (Row->TryGetArrayField(TEXT("going"), Going))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Going)
				{
					Way.Going.Add(FMath::RoundToInt32(Value->AsNumber()));
				}
			}
		}
	}
	Plan.bOk = Plan.bOk && Plan.Rows.Num() > 0;
	return Plan;
}

FGateAsk FCampaignSession::AskGate(int32 GateId)
{
	FGateAsk Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("gateAsk"), FString::Printf(TEXT("[%d]"), GateId), Object))
	{
		return Out;
	}
	Object->TryGetBoolField(TEXT("ok"), Out.bOk);
	Object->TryGetStringField(TEXT("why"), Out.Why);
	Object->TryGetStringField(TEXT("order"), Out.Order);
	Object->TryGetStringField(TEXT("label"), Out.Label);
	Object->TryGetStringField(TEXT("question"), Out.Question);
	Object->TryGetStringField(TEXT("lead"), Out.Lead);
	Object->TryGetStringField(TEXT("heading"), Out.Heading);
	Object->TryGetStringField(TEXT("total"), Out.Total);
	Object->TryGetStringField(TEXT("none"), Out.None);
	Object->TryGetStringField(TEXT("yes"), Out.Yes);
	Object->TryGetStringField(TEXT("no"), Out.No);
	Object->TryGetStringField(TEXT("holder"), Out.Holder);
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (Object->TryGetArrayField(TEXT("rows"), Rows))
	{
		for (const TSharedPtr<FJsonValue>& Each : *Rows)
		{
			const TSharedPtr<FJsonObject> Row = Each->AsObject();
			if (Row.IsValid())
			{
				FString Name, What, Loss;
				Row->TryGetStringField(TEXT("name"), Name);
				Row->TryGetStringField(TEXT("what"), What);
				Row->TryGetStringField(TEXT("loss"), Loss);
				Out.Rows.Add(FString::Printf(TEXT("%s: %s  %s"), *Name, *What, *Loss).TrimEnd());
			}
		}
	}
	Object->TryGetStringField(TEXT("resentHeading"), Out.ResentHeading);
	Object->TryGetStringField(TEXT("resentNote"), Out.ResentNote);
	Object->TryGetStringArrayField(TEXT("resent"), Out.Resent);
	return Out;
}

FString FCampaignSession::GateLeft(const TArray<int32>& CorpsIds)
{
	TSharedPtr<FJsonObject> Object;
	FString Line;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("gateLeft"), FString::Printf(TEXT("[[%s]]"), *IdList(CorpsIds)), Object))
	{
		Object->TryGetStringField(TEXT("line"), Line);
	}
	return Line;
}

bool FCampaignSession::OrderGate(int32 GateId, bool bShut, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("gateOrder"), FString::Printf(TEXT("[%d,%s]"), GateId, bShut ? TEXT("true") : TEXT("false")), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}

FAttackResult FCampaignSession::Take(const TArray<int32>& CorpsIds, int32 RegionId, bool bAssault)
{
	return CorpsIds.Num() == 0 ? FAttackResult() : Fight(TEXT("take"), FString::Printf(TEXT("[[%s],%d,%s]"), *IdList(CorpsIds), RegionId, bAssault ? TEXT("true") : TEXT("false")));
}

namespace
{
	/** An order that may end in a battle, as the rules answer it (on either thread: it reads only the answer). */
	FAttackResult ReadFight(const TSharedPtr<FJsonObject>& Object)
	{
		FAttackResult Out;
		const TSharedPtr<FJsonObject>* Report = nullptr;
		if (Object->TryGetObjectField(TEXT("report"), Report)) // null when the rules would not let the attack go in
		{
			Out.bFought = true;
			(*Report)->TryGetStringField(TEXT("level"), Out.Level);
			(*Report)->TryGetBoolField(TEXT("taken"), Out.bTaken);
			(*Report)->TryGetBoolField(TEXT("yielded"), Out.bYielded);
			(*Report)->TryGetNumberField(TEXT("attLost"), Out.AttackerLost);
			(*Report)->TryGetNumberField(TEXT("defLost"), Out.DefenderLost);
			(*Report)->TryGetNumberField(TEXT("prisoners"), Out.Prisoners);
			Out.Attackers = ReadUnits(*Report, TEXT("attackers"));
			Out.Defenders = ReadUnits(*Report, TEXT("defenders"));
			Out.Lines = ReadTicket(*Report);
			double Odds = 0.0;
			(*Report)->TryGetNumberField(TEXT("odds"), Odds);
			Out.Odds = static_cast<float>(Odds);
		}
		Object->TryGetStringField(TEXT("price"), Out.Price);
		{
			// (Version 84) who paid, and to what
			const TSharedPtr<FJsonObject>* Paid = nullptr;
			if (Object->TryGetObjectField(TEXT("paid"), Paid))
			{
				(*Paid)->TryGetStringField(TEXT("text"), Out.Paid);
				(*Paid)->TryGetStringField(TEXT("sign"), Out.PaidSign);
			}
		}
		Object->TryGetStringField(TEXT("note"), Out.Note);
		Object->TryGetBoolField(TEXT("siege"), Out.bSiege);
		Object->TryGetBoolField(TEXT("fell"), Out.bFell); // (rules 84.1: a march to take a place)
		Object->TryGetStringField(TEXT("stands"), Out.Stands);
		Out.Moved = ReadMoves(Object);
		return Out;
	}
}

FAttackResult FCampaignSession::Fight(const FString& Function, const FString& ArgumentsJson)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(Function, ArgumentsJson, Object))
	{
		return FAttackResult();
	}
	FAttackResult Out = ReadFight(Object);
	TakeView(Object);
	return Out;
}

FAttackResult FCampaignSession::SettleLandLive(const TArray<int32>& CorpsIds, int32 RegionId, const FString& ResultJson, double TicketSeed)
{
	return Fight(TEXT("battleSettleLand"), FString::Printf(TEXT("[[%s],%d,%s,%.0f]"), *IdList(CorpsIds), RegionId, *ResultJson, TicketSeed));
}

FString FCampaignSession::StepDigest(int32 RegionId, const TArray<int32>& CorpsIds, int32 FleetId, int64& OutStamp, FString& OutHolder)
{
	TSharedPtr<FJsonObject> Object;
	FString Line;
	if (CallRules(TEXT("digest"), FString::Printf(TEXT("[%d,[%s],%d]"), RegionId, *IdList(CorpsIds), FleetId), Object))
	{
		double Stamp = 0.0;
		Object->TryGetNumberField(TEXT("stamp"), Stamp);
		OutStamp = static_cast<int64>(Stamp);
		Object->TryGetStringField(TEXT("holder"), OutHolder);
		Object->TryGetStringField(TEXT("text"), Line);
	}
	return Line;
}

bool FCampaignSession::StepFind(const FString& Name, int32& OutRegion, int32& OutSea, int32& OutCorps, int32& OutCorpsRegion, int32& OutFleet, int32& OutGroup)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("find"), FString::Printf(TEXT("[%s]"), *JsonQuote(Name)), Object))
	{
		return false;
	}
	Object->TryGetNumberField(TEXT("region"), OutRegion);
	Object->TryGetNumberField(TEXT("sea"), OutSea);
	Object->TryGetNumberField(TEXT("corps"), OutCorps);
	Object->TryGetNumberField(TEXT("corpsRegion"), OutCorpsRegion);
	Object->TryGetNumberField(TEXT("fleet"), OutFleet);
	Object->TryGetNumberField(TEXT("group"), OutGroup);
	return true;
}

FMonthResult FCampaignSession::EndMonth()
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("endMonth"), TEXT("[]"), Object))
	{
		return FMonthResult();
	}
	FMonthResult Out = ReadMonth(Object, true);
	TakeView(Object);
	return Out;
}

void FCampaignSession::EndMonthAside(bool bAside, TFunction<void(FMonthResult&&)>&& InThen)
{
	if (!bAside || !TheaterSpeed::MonthAside() || CVarRulesAside.GetValueOnGameThread() == 0 || !bRunning || !Host.IsValid())
	{
		InThen(EndMonth()); // (at once, as before)
		return;
	}
	// (the month played and its answer read on the rules' thread; the picture of the war is taken by Settle, then Then runs)
	const TSharedRef<FMonthResult, ESPMode::ThreadSafe> Read = MakeShared<FMonthResult, ESPMode::ThreadSafe>();
	Order(TEXT("endMonth"), TEXT("[]"), true,
		[Read](const TSharedPtr<FJsonObject>& Answer)
		{
			*Read = ReadMonth(Answer, false);
			return FHandNext();
		},
		[Read, Then = MoveTemp(InThen)](const TSharedPtr<FJsonObject>& Answer)
		{
			Then(Answer.IsValid() ? MoveTemp(*Read) : FMonthResult());
		});
}

FMonthResult FCampaignSession::ReadMonth(const TSharedPtr<FJsonObject>& Object, bool bGameThread)
{
	FMonthResult Out;
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (Object->TryGetArrayField(TEXT("steps"), Steps))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Steps)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FMonthStep Step;
			Entry->TryGetStringField(TEXT("nat"), Step.Nation);
			Entry->TryGetStringField(TEXT("note"), Step.Note);
			Entry->TryGetBoolField(TEXT("mine"), Step.bMine);
			Entry->TryGetBoolField(TEXT("moved"), Step.bMarchOnly);
			// ---- (release 2, the enemy's month near you: Rules/near-moves.ts)
			Entry->TryGetBoolField(TEXT("near"), Step.bNear);
			Entry->TryGetStringField(TEXT("nearNote"), Step.NearNote);
			Entry->TryGetBoolField(TEXT("nearAllies"), Step.bNearAllies);
			Entry->TryGetStringField(TEXT("allyNote"), Step.AllyNote);
			const TArray<TSharedPtr<FJsonValue>>* Arrows = nullptr;
			if (Entry->TryGetArrayField(TEXT("arrows"), Arrows))
			{
				for (const TSharedPtr<FJsonValue>& Drawing : *Arrows)
				{
					const TSharedPtr<FJsonObject>* Arrow = nullptr;
					if (!Drawing.IsValid() || !Drawing->TryGetObject(Arrow))
					{
						continue;
					}
					FStepArrow Drawn;
					double Number = 0.0;
					Drawn.From = (*Arrow)->TryGetNumberField(TEXT("from"), Number) ? FMath::RoundToInt32(Number) : INDEX_NONE;
					Drawn.To = (*Arrow)->TryGetNumberField(TEXT("to"), Number) ? FMath::RoundToInt32(Number) : INDEX_NONE;
					(*Arrow)->TryGetStringField(TEXT("nat"), Drawn.Nation);
					FString Kind;
					(*Arrow)->TryGetStringField(TEXT("kind"), Kind);
					Drawn.Kind = Kind == TEXT("attack") ? 1 : Kind == TEXT("fell") ? 2 : 0;
					(*Arrow)->TryGetBoolField(TEXT("war"), Drawn.bWar);
					(*Arrow)->TryGetBoolField(TEXT("away"), Drawn.bAway);
					(*Arrow)->TryGetBoolField(TEXT("taken"), Drawn.bTaken);
					(*Arrow)->TryGetBoolField(TEXT("siege"), Drawn.bSiege);
					(*Arrow)->TryGetBoolField(TEXT("yielded"), Drawn.bYielded);
					(*Arrow)->TryGetBoolField(TEXT("ally"), Drawn.bAlly);
					(*Arrow)->TryGetBoolField(TEXT("ours"), Drawn.bOurs);
					Drawn.Corps = (*Arrow)->TryGetNumberField(TEXT("corps"), Number) ? FMath::RoundToInt32(Number) : 1;
					Drawn.Divs = (*Arrow)->TryGetNumberField(TEXT("divs"), Number) ? FMath::RoundToInt32(Number) : -1; // (null: only glimpsed)
					if (Drawn.To >= 0) // (an arrow to nowhere is not drawn)
					{
						Step.Arrows.Add(MoveTemp(Drawn));
					}
				}
			}
			// ---- (end release 2)
			const TArray<TSharedPtr<FJsonValue>>* Shelled = nullptr;
			if (Entry->TryGetArrayField(TEXT("shelled"), Shelled))
			{
				for (const TSharedPtr<FJsonValue>& Coast : *Shelled)
				{
					Step.Shelled.AddUnique(FMath::RoundToInt32(Coast->AsNumber()));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* SeaFights = nullptr;
			if (Entry->TryGetArrayField(TEXT("seas"), SeaFights))
			{
				for (const TSharedPtr<FJsonValue>& Sea : *SeaFights)
				{
					Step.SeaFights.AddUnique(FMath::RoundToInt32(Sea->AsNumber()));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Sunk = nullptr;
			if (Entry->TryGetArrayField(TEXT("sunk"), Sunk))
			{
				ReadSinkings(*Sunk, Step.Sinkings);
			}
			const TArray<TSharedPtr<FJsonValue>>* Raids = nullptr;
			if (Entry->TryGetArrayField(TEXT("raids"), Raids))
			{
				ReadSorties(*Raids, Step.Raids);
			}
			const TArray<TSharedPtr<FJsonValue>>* Battles = nullptr;
			if (Entry->TryGetArrayField(TEXT("battles"), Battles))
			{
				for (const TSharedPtr<FJsonValue>& Region : *Battles)
				{
					Step.Battles.AddUnique(FMath::RoundToInt32(Region->AsNumber()));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Focus = nullptr;
			if (Entry->TryGetArrayField(TEXT("focus"), Focus))
			{
				for (const TSharedPtr<FJsonValue>& Region : *Focus)
				{
					Step.Focus.Add(FMath::RoundToInt32(Region->AsNumber()));
				}
			}
			const TSharedPtr<FJsonObject>* StepView = nullptr;
			if (Entry->TryGetObjectField(TEXT("view"), StepView))
			{
				Step.View = bGameThread ? ParseView(*StepView) : ReadView(*StepView);
			}
			Out.Steps.Add(MoveTemp(Step));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	if (Object->TryGetArrayField(TEXT("events"), Events))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Events)
		{
			FString Text;
			if (Value->AsObject()->TryGetStringField(TEXT("text"), Text))
			{
				Out.Headlines.Add(Text);
			}
		}
	}
	Object->TryGetStringField(TEXT("fallen"), Out.Fallen);
	// (Release 2; Brian, 24 November 2026 18:30Z) The units handed over to him as his month opened.
	const TArray<TSharedPtr<FJsonValue>>* ReadyList = nullptr;
	if (Object->TryGetArrayField(TEXT("ready"), ReadyList))
	{
		for (const TSharedPtr<FJsonValue>& Value : *ReadyList)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			if (!Entry.IsValid())
			{
				continue;
			}
			FReadyUnit& Unit = Out.Ready.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("what"), Unit.What);
			Entry->TryGetStringField(TEXT("text"), Unit.Text);
			Entry->TryGetStringField(TEXT("name"), Unit.Name);
			double Number = 0.0;
			if (Entry->TryGetNumberField(TEXT("region"), Number) && Number >= 0.0)
			{
				Unit.Region = FMath::RoundToInt32(Number);
			}
			if (Entry->TryGetNumberField(TEXT("corps"), Number) && Number >= 0.0)
			{
				Unit.Corps = FMath::RoundToInt32(Number);
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Announce = nullptr;
	if (Object->TryGetArrayField(TEXT("announce"), Announce))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Announce)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FAnnouncement& Told = Out.Announcements.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Told.Tag);
			Entry->TryGetStringField(TEXT("title"), Told.Title);
			Entry->TryGetStringField(TEXT("text"), Told.Text);
			Entry->TryGetBoolField(TEXT("good"), Told.bGood);
			Entry->TryGetStringField(TEXT("kicker"), Told.Kicker);
			Entry->TryGetBoolField(TEXT("ending"), Told.bEnding);
			Entry->TryGetStringField(TEXT("more"), Told.More);
			Entry->TryGetNumberField(TEXT("region"), Told.Region);
			const TArray<TSharedPtr<FJsonValue>>* Facts = nullptr;
			if (Entry->TryGetArrayField(TEXT("facts"), Facts))
			{
				for (const TSharedPtr<FJsonValue>& Fact : *Facts)
				{
					const TSharedPtr<FJsonObject> Pair = Fact->AsObject();
					if (Pair.IsValid())
					{
						Told.FactLabels.Add(Pair->GetStringField(TEXT("label")));
						Told.FactValues.Add(Pair->GetStringField(TEXT("value")));
					}
				}
			}
		}
	}
	const TSharedPtr<FJsonObject>* Reel = nullptr;
	if (Object->TryGetObjectField(TEXT("newsreel"), Reel))
	{
		(*Reel)->TryGetStringField(TEXT("date"), Out.ReelDate);
		const TArray<TSharedPtr<FJsonValue>>* Cards = nullptr;
		if ((*Reel)->TryGetArrayField(TEXT("cards"), Cards))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Cards)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FNewsCard& Card = Out.Reel.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("nat"), Card.Tag);
				Entry->TryGetStringField(TEXT("title"), Card.Title);
				Entry->TryGetStringField(TEXT("text"), Card.Text);
				Entry->TryGetNumberField(TEXT("region"), Card.Region);
			}
		}
	}
	const TSharedPtr<FJsonObject>* Year = nullptr;
	if (Object->TryGetObjectField(TEXT("year"), Year))
	{
		Out.Year.bValid = (*Year)->TryGetStringField(TEXT("year"), Out.Year.Year);
		(*Year)->TryGetStringField(TEXT("title"), Out.Year.Title);
		(*Year)->TryGetStringField(TEXT("kicker"), Out.Year.Kicker);
		(*Year)->TryGetStringArrayField(TEXT("lines"), Out.Year.Lines);
	}
	return Out;
}

TArray<FNewsCard> FCampaignSession::Briefing(FString& OutNation, FString& OutDate)
{
	TArray<FNewsCard> Out;
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (!CallRules(TEXT("briefing"), TEXT("[]"), Object) || !Object->TryGetBoolField(TEXT("ok"), bOk) || !bOk)
	{
		return Out;
	}
	Object->TryGetStringField(TEXT("nation"), OutNation);
	Object->TryGetStringField(TEXT("date"), OutDate);
	const TArray<TSharedPtr<FJsonValue>>* Cards = nullptr;
	if (Object->TryGetArrayField(TEXT("cards"), Cards))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Cards)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			if (!Entry.IsValid())
			{
				continue;
			}
			FNewsCard& Card = Out.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("title"), Card.Title);
			Entry->TryGetStringField(TEXT("text"), Card.Text);
			Entry->TryGetNumberField(TEXT("region"), Card.Region);
			double Zoom = 0.36, Turn = 0.0;
			Entry->TryGetNumberField(TEXT("zoom"), Zoom);
			Entry->TryGetNumberField(TEXT("turn"), Turn);
			Card.Zoom = static_cast<float>(Zoom);
			Card.Turn = static_cast<float>(Turn);
		}
	}
	return Out;
}

FDocumentPage FCampaignSession::Record(int32 CorpsId)
{
	FDocumentPage Page;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("record"), FString::Printf(TEXT("[%d]"), CorpsId), Object))
	{
		return Page; // also what the stand-in rules come to: they keep no records
	}
	FString Name, Nick, What, Where, Raised, Grade;
	int32 Experience = 0;
	Object->TryGetStringField(TEXT("name"), Name);
	Object->TryGetStringField(TEXT("nick"), Nick);
	Object->TryGetStringField(TEXT("what"), What);
	Object->TryGetStringField(TEXT("where"), Where);
	Object->TryGetStringField(TEXT("raised"), Raised);
	Object->TryGetStringField(TEXT("grade"), Grade);
	Object->TryGetNumberField(TEXT("xp"), Experience);

	Page.bValid = true;
	Page.Title = Nick.IsEmpty() ? Name : FString::Printf(TEXT("%s, \u201C%s\u201D"), *Name, *Nick);
	Page.Subtitle = FString::Printf(TEXT("%s\n%s\n%s\n%s, experience %d of 100"), *What, *Where, *Raised, *Grade, Experience);
	Page.Ribbons = ReadRibbons(Object, TEXT("honours"));

	TArray<FString> Notes;
	if (Object->TryGetStringArrayField(TEXT("notes"), Notes) && Notes.Num() > 0)
	{
		Page.Sections.Add({ TEXT("Ties"), FString::Join(Notes, TEXT("\n\n")) });
	}

	TArray<FString> Honours;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Object->TryGetArrayField(TEXT("honours"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Honour = Value->AsObject();
			FString Title, Kind, Citation, Date;
			Honour->TryGetStringField(TEXT("title"), Title);
			Honour->TryGetStringField(TEXT("name"), Kind);
			Honour->TryGetStringField(TEXT("citation"), Citation);
			Honour->TryGetStringField(TEXT("date"), Date);
			Honours.Add(FString::Printf(TEXT("%s  \u00B7  %s, %s\n%s"), *Title, *Kind, *Date, *Citation));
		}
	}
	Page.Sections.Add({ TEXT("Honours"), Honours.Num() > 0 ? FString::Join(Honours, TEXT("\n\n")) : FString(TEXT("None yet.")) });

	TArray<FString> Diary;
	if (Object->TryGetArrayField(TEXT("diary"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Line = Value->AsObject();
			FString Date, Text;
			Line->TryGetStringField(TEXT("date"), Date);
			Line->TryGetStringField(TEXT("text"), Text);
			Diary.Add(FString::Printf(TEXT("%s:  %s"), *Date, *Text));
		}
	}
	Page.Sections.Add({ TEXT("Diary, newest first"), Diary.Num() > 0 ? FString::Join(Diary, TEXT("\n")) : FString(TEXT("Nothing written yet.")) });
	return Page;
}

FDocumentPage FCampaignSession::Roll()
{
	FDocumentPage Page;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("roll"), TEXT("[]"), Object))
	{
		return Page;
	}
	Page.bValid = true;
	Page.Title = TEXT("The roll of honour");

	TArray<FString> Awards;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Object->TryGetArrayField(TEXT("awards"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Award = Value->AsObject();
			FString Title, Unit, Citation, Date;
			Award->TryGetStringField(TEXT("title"), Title);
			Award->TryGetStringField(TEXT("unit"), Unit);
			Award->TryGetStringField(TEXT("citation"), Citation);
			Award->TryGetStringField(TEXT("date"), Date);
			Awards.Add(FString::Printf(TEXT("%s  \u00B7  %s, %s\n%s"), *Unit, *Title, *Date, *Citation));
		}
	}
	Page.Subtitle = FString::Printf(TEXT("%d honour%s won so far. The newest come first."), Awards.Num(), Awards.Num() == 1 ? TEXT("") : TEXT("s"));
	Page.Sections.Add({ TEXT("Honours"), Awards.Num() > 0 ? FString::Join(Awards, TEXT("\n\n")) : FString(TEXT("No unit has been honoured yet. Take a capital, break a line, hold out under siege.")) });

	TArray<FString> Fallen;
	if (Object->TryGetArrayField(TEXT("fallen"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Unit = Value->AsObject();
			FString Name, Date, Where, How, By, General;
			Unit->TryGetStringField(TEXT("name"), Name);
			Unit->TryGetStringField(TEXT("date"), Date);
			Unit->TryGetStringField(TEXT("where"), Where);
			Unit->TryGetStringField(TEXT("how"), How);
			Unit->TryGetStringField(TEXT("by"), By);
			Unit->TryGetStringField(TEXT("general"), General);
			FString Line = FString::Printf(TEXT("%s  \u00B7  %s at %s, %s, by %s forces"), *Name, *How, *Where, *Date, *By);
			if (!General.IsEmpty())
			{
				Line += FString::Printf(TEXT("  \u00B7  %s was with it at the end"), *General);
			}
			TArray<FString> Carried;
			if (Unit->TryGetStringArrayField(TEXT("honours"), Carried) && Carried.Num() > 0)
			{
				Line += FString::Printf(TEXT("\nIt carried: %s"), *FString::Join(Carried, TEXT(", ")));
			}
			Fallen.Add(Line);
		}
	}
	Page.Sections.Add({ TEXT("The memorial wall"), Fallen.Num() > 0 ? FString::Join(Fallen, TEXT("\n\n")) : FString(TEXT("No corps has been lost.")) });
	return Page;
}

void FCampaignSession::SetStance(const TArray<int32>& CorpsIds, const FString& Posture)
{
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("stance"), FString::Printf(TEXT("[[%s],\"%s\"]"), *IdList(CorpsIds), *Posture), Object))
	{
		View = ParseView(Object); // this call answers with the picture itself
		OnViewChanged.Broadcast();
	}
}

TArray<int32> FCampaignSession::RailReach(const TArray<int32>& CorpsIds)
{
	TArray<int32> Out;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("railReach"), FString::Printf(TEXT("[[%s]]"), *IdList(CorpsIds)), Object))
	{
		const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
		if (Object->TryGetArrayField(TEXT("regions"), Regions))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Regions)
			{
				Out.Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
	}
	return Out;
}

TArray<FCorpsMove> FCampaignSession::Rail(const TArray<int32>& CorpsIds, int32 RegionId, FString* OutSaid)
{
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("rail"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		return TArray<FCorpsMove>();
	}
	TArray<FCorpsMove> Out = ReadMoves(Object);
	if (OutSaid)
	{
		Object->TryGetStringField(TEXT("said"), *OutSaid);
	}
	TakeView(Object);
	return Out;
}

FRailShow FCampaignSession::RailShow(const TArray<int32>& CorpsIds, const TArray<int32>& March, const FString& Army, const TArray<int32>& ArmyIds)
{
	FRailShow Out;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("railShow"), RailShowArgs(CorpsIds, March, Army, ArmyIds), Object))
	{
		return Out;
	}
	Object->TryGetBoolField(TEXT("can"), Out.bCan);
	Object->TryGetStringField(TEXT("strong"), Out.Strong);
	Object->TryGetStringField(TEXT("line"), Out.Line);
	auto Numbers = [&Object](const TCHAR* Name, TArray<int32>& Into)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object->TryGetArrayField(Name, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				Into.Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
	};
	auto Pairs = [&Object](const TCHAR* Name, TArray<FIntPoint>& Into)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object->TryGetArrayField(Name, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				const TArray<TSharedPtr<FJsonValue>>* Two = nullptr;
				if (Value.IsValid() && Value->TryGetArray(Two) && Two->Num() >= 2)
				{
					Into.Emplace(FMath::RoundToInt32((*Two)[0]->AsNumber()), FMath::RoundToInt32((*Two)[1]->AsNumber()));
				}
			}
		}
	};
	Pairs(TEXT("tracks"), Out.Light.Tracks);
	Numbers(TEXT("dots"), Out.Light.Dots);
	Numbers(TEXT("tint"), Out.Light.Tint);
	Pairs(TEXT("bars"), Out.Light.Bars);
	Numbers(TEXT("ringed"), Out.Light.Ringed);
	return Out;
}

FRailTip FCampaignSession::RailTip(const TArray<int32>& CorpsIds, int32 RegionId)
{
	FRailTip Out;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("railTip"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		Object->TryGetStringField(TEXT("ride"), Out.Ride);
		Object->TryGetStringField(TEXT("relay"), Out.Relay);
		Object->TryGetStringField(TEXT("no"), Out.No);
		Object->TryGetStringField(TEXT("strong"), Out.Strong);
		Object->TryGetBoolField(TEXT("cutHere"), Out.bCutHere);
	}
	return Out;
}

void FCampaignSession::SeaReach(const TArray<int32>& CorpsIds, TArray<int32>& OutPorts, TArray<int32>& OutBeaches)
{
	OutPorts.Reset();
	OutBeaches.Reset();
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("seaReach"), FString::Printf(TEXT("[[%s]]"), *IdList(CorpsIds)), Object))
	{
		return;
	}
	for (const TPair<const TCHAR*, TArray<int32>*>& List : { TPair<const TCHAR*, TArray<int32>*>(TEXT("ports"), &OutPorts), TPair<const TCHAR*, TArray<int32>*>(TEXT("beaches"), &OutBeaches) })
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object->TryGetArrayField(List.Key, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				List.Value->Add(FMath::RoundToInt32(Value->AsNumber()));
			}
		}
	}
}

TArray<FCorpsMove> FCampaignSession::Ship(const TArray<int32>& CorpsIds, int32 RegionId)
{
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() == 0 || !CallRules(TEXT("ship"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		return TArray<FCorpsMove>();
	}
	TArray<FCorpsMove> Out = ReadMoves(Object);
	TakeView(Object);
	return Out;
}

FWarPlan FCampaignSession::PlanWar(const FString& Nation, int32 RegionId)
{
	FWarPlan Plan;
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("warPlan"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Nation), RegionId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), Plan.bOk);
		Object->TryGetStringField(TEXT("name"), Plan.Name);
		Object->TryGetStringField(TEXT("lead"), Plan.Lead);
		Object->TryGetStringField(TEXT("friends"), Plan.Friends);
		Object->TryGetStringArrayField(TEXT("lines"), Plan.Lines);
	}
	return Plan;
}

FLandingPlan FCampaignSession::PlanLanding(const TArray<int32>& CorpsIds, int32 RegionId)
{
	FLandingPlan Plan;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("landingPlan"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), Plan.bOk);
		Object->TryGetStringField(TEXT("why"), Plan.Why);
		Object->TryGetStringField(TEXT("name"), Plan.Name);
		Object->TryGetStringArrayField(TEXT("lines"), Plan.Lines);
		Object->TryGetStringField(TEXT("warn"), Plan.Warn);
		Object->TryGetStringField(TEXT("verdict"), Plan.Verdict);
		Object->TryGetStringField(TEXT("tone"), Plan.Tone);
		Object->TryGetStringField(TEXT("under"), Plan.Under);
		Object->TryGetStringField(TEXT("underTone"), Plan.UnderTone);
		const TSharedPtr<FJsonObject>* Ashore = nullptr;
		if (Object->TryGetObjectField(TEXT("ashore"), Ashore) && Ashore && Ashore->IsValid())
		{
			(*Ashore)->TryGetStringField(TEXT("head"), Plan.AshoreHead);
			(*Ashore)->TryGetStringField(TEXT("line"), Plan.AshoreLine);
			(*Ashore)->TryGetStringField(TEXT("tone"), Plan.AshoreTone);
		}
	}
	return Plan;
}

namespace
{
	/** A landing as the rules answer it (on either thread). */
	FLandingResult ReadLanding(const TSharedPtr<FJsonObject>& Object)
	{
		FLandingResult Result;
		Object->TryGetBoolField(TEXT("ok"), Result.bOk);
		Object->TryGetBoolField(TEXT("taken"), Result.bTaken);
		Object->TryGetStringField(TEXT("note"), Result.Note);
		Result.Moved = ReadMoves(Object);
		return Result;
	}
}

FLandingResult FCampaignSession::Land(const TArray<int32>& CorpsIds, int32 RegionId)
{
	FLandingResult Result;
	TSharedPtr<FJsonObject> Object;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("landOn"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), Object))
	{
		Result = ReadLanding(Object);
		TakeView(Object);
	}
	return Result;
}

FProvinceInfo FCampaignSession::Province(int32 RegionId)
{
	FProvinceInfo Info;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("province"), RegionArgs(RegionId), Object))
	{
		return Info; // the stand-in rules have no provinces to manage
	}
	Info.bValid = true;
	Info.Region = RegionId;
	Object->TryGetBoolField(TEXT("home"), Info.bHome);
	{
		const TSharedPtr<FJsonObject>* Plots = nullptr;
		if (Object->TryGetObjectField(TEXT("plots"), Plots))
		{
			Info.bHasPlots = true;
			(*Plots)->TryGetBoolField(TEXT("mine"), Info.bPlotsMine);
			(*Plots)->TryGetNumberField(TEXT("free"), Info.PlotsFree);
			(*Plots)->TryGetNumberField(TEXT("can"), Info.PlotsCanBuild);
			(*Plots)->TryGetStringArrayField(TEXT("list"), Info.Plots);
			(*Plots)->TryGetStringField(TEXT("size"), Info.TownKind);
			const TArray<TSharedPtr<FJsonValue>>* Each = nullptr;
			if ((*Plots)->TryGetArrayField(TEXT("plots"), Each))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Each)
				{
					const TSharedPtr<FJsonObject> Entry = Value->AsObject();
					if (!Entry.IsValid())
					{
						continue;
					}
					FTownPlot& Plot = Info.TownPlots.AddDefaulted_GetRef();
					Entry->TryGetStringField(TEXT("line"), Plot.Line);
					Entry->TryGetNumberField(TEXT("no"), Plot.No);
					Entry->TryGetStringField(TEXT("name"), Plot.Name);
					Entry->TryGetStringField(TEXT("label"), Plot.Label);
					Entry->TryGetNumberField(TEXT("level"), Plot.Level);
					Entry->TryGetBoolField(TEXT("building"), Plot.bBuilding);
					Entry->TryGetStringField(TEXT("due"), Plot.Due);
					Entry->TryGetStringField(TEXT("does"), Plot.Does);
				}
			}
			if ((*Plots)->TryGetArrayField(TEXT("offers"), Each))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Each)
				{
					const TSharedPtr<FJsonObject> Entry = Value->AsObject();
					if (!Entry.IsValid())
					{
						continue;
					}
					FTownOffer& Offer = Info.TownOffers.AddDefaulted_GetRef();
					Entry->TryGetStringField(TEXT("line"), Offer.Line);
					Entry->TryGetNumberField(TEXT("no"), Offer.No);
					Entry->TryGetStringField(TEXT("name"), Offer.Name);
					Entry->TryGetStringField(TEXT("label"), Offer.Label);
					Entry->TryGetStringField(TEXT("effect"), Offer.Effect);
					Entry->TryGetStringField(TEXT("blurb"), Offer.Blurb);
					Entry->TryGetStringField(TEXT("why"), Offer.Why);
					Entry->TryGetNumberField(TEXT("level"), Offer.Level);
					Entry->TryGetNumberField(TEXT("has"), Offer.Has);
					Entry->TryGetNumberField(TEXT("cost"), Offer.Cost);
					Entry->TryGetNumberField(TEXT("months"), Offer.Months);
				}
			}
		}
	}
	Object->TryGetStringField(TEXT("raiseNote"), Info.Note);
	{
		const TSharedPtr<FJsonObject>* Siege = nullptr;
		if (Object->TryGetObjectField(TEXT("siege"), Siege))
		{
			(*Siege)->TryGetStringField(TEXT("by"), Info.SiegeBy);
			(*Siege)->TryGetBoolField(TEXT("own"), Info.bSiegeOwn);
			(*Siege)->TryGetStringArrayField(TEXT("lines"), Info.SiegeLines);
		}
		const TSharedPtr<FJsonObject>* LeftWorks = nullptr;
		if (Object->TryGetObjectField(TEXT("leftWorks"), LeftWorks))
		{
			(*LeftWorks)->TryGetStringField(TEXT("title"), Info.LeftWorksTitle);
			(*LeftWorks)->TryGetStringField(TEXT("text"), Info.LeftWorksText);
		}
		const TSharedPtr<FJsonObject>* Pocket = nullptr;
		if (Object->TryGetObjectField(TEXT("pocket"), Pocket))
		{
			(*Pocket)->TryGetStringField(TEXT("title"), Info.PocketTitle);
			(*Pocket)->TryGetStringField(TEXT("text"), Info.PocketText);
			(*Pocket)->TryGetBoolField(TEXT("spent"), Info.bPocketSpent);
		}
	}
	Object->TryGetStringField(TEXT("directive"), Info.DirectiveLine);
	{
		const TSharedPtr<FJsonObject>* Places = nullptr;
		if (Object->TryGetObjectField(TEXT("places"), Places))
		{
			(*Places)->TryGetStringField(TEXT("head"), Info.PlacesHead);
			(*Places)->TryGetStringArrayField(TEXT("lines"), Info.PlacesLines);
		}
	}
	{
		const TSharedPtr<FJsonObject>* Town = nullptr;
		if (Object->TryGetObjectField(TEXT("town"), Town))
		{
			(*Town)->TryGetStringField(TEXT("name"), Info.Town);
			(*Town)->TryGetStringField(TEXT("size"), Info.TownSize);
			(*Town)->TryGetNumberField(TEXT("worth"), Info.TownWorth);
		}
		const TSharedPtr<FJsonObject>* Prize = nullptr;
		if (Object->TryGetObjectField(TEXT("prize"), Prize))
		{
			(*Prize)->TryGetStringField(TEXT("kind"), Info.PrizeKind);
			(*Prize)->TryGetStringField(TEXT("line"), Info.PrizeLine);
			(*Prize)->TryGetStringArrayField(TEXT("items"), Info.PrizeItems);
		}
	}
	{
		// A fallen country's capital: what taking it would mean.
		FString Fallen;
		if (Object->TryGetStringField(TEXT("fallenNote"), Fallen) && !Fallen.IsEmpty())
		{
			Info.Note = Info.Note.IsEmpty() ? Fallen : Fallen + TEXT("\n") + Info.Note;
		}
	}

	int32 Industry = 0, Works = 0;
	bool bHarbour = false;
	Object->TryGetNumberField(TEXT("industry"), Industry);
	Object->TryGetNumberField(TEXT("works"), Works);
	Object->TryGetBoolField(TEXT("harbour"), bHarbour);
	TArray<FString> Holdings;
	Holdings.Add(Industry > 0 ? FString::Printf(TEXT("Industry %d"), Industry) : FString(TEXT("No industry")));
	Holdings.Add(Works > 0 ? FString::Printf(TEXT("Fortified, level %d"), Works) : FString(TEXT("No works")));
	Holdings.Add(bHarbour ? TEXT("Harbour") : TEXT("No harbour"));
	Info.Holdings = FString::Join(Holdings, TEXT("   \u00B7   "));
	// What kind of country it is for an army, its rivers and bridges, its railway.
	TArray<FString> Country;
	if (Object->TryGetStringArrayField(TEXT("country"), Country) && Country.Num() > 0)
	{
		Info.Holdings += TEXT("\n") + FString::Join(Country, TEXT("\n"));
	}

	// What the province gives whoever holds it each month.
	const TSharedPtr<FJsonObject>* Yield = nullptr;
	if (Object->TryGetObjectField(TEXT("yield"), Yield))
	{
		int32 Land = 0, Port = 0, Oil = 0, Factories = 0, Capital = 0, Total = 0;
		(*Yield)->TryGetNumberField(TEXT("land"), Land);
		(*Yield)->TryGetNumberField(TEXT("port"), Port);
		(*Yield)->TryGetNumberField(TEXT("oil"), Oil);
		(*Yield)->TryGetNumberField(TEXT("fac"), Factories);
		(*Yield)->TryGetNumberField(TEXT("capital"), Capital);
		(*Yield)->TryGetNumberField(TEXT("total"), Total);
		Info.YieldTotal = Total;
		TArray<FString> Parts;
		auto Part = [&Parts](const TCHAR* What, int32 Amount)
		{
			if (Amount > 0)
			{
				Parts.Add(FString::Printf(TEXT("%s %s"), What, *FText::AsNumber(Amount).ToString()));
			}
		};
		Part(TEXT("factories"), Factories);
		Part(TEXT("land"), Land);
		Part(TEXT("harbour"), Port);
		Part(TEXT("oil"), Oil);
		Part(TEXT("seat of government"), Capital);
		Info.Yield = Total > 0
			? FString::Printf(TEXT("%s a month%s"), *FText::AsNumber(Total).ToString(), Parts.Num() > 1 ? *FString::Printf(TEXT(" (%s)"), *FString::Join(Parts, TEXT(", "))) : TEXT(""))
			: FString(TEXT("nothing"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Raise = nullptr;
	if (Object->TryGetArrayField(TEXT("raise"), Raise))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Raise)
		{
			const TSharedPtr<FJsonObject> Spec = Value->AsObject();
			FProvinceOption Option;
			int32 Cost = 0, Months = 0, Divisions = 0, Keep = 0;
			Spec->TryGetNumberField(TEXT("keep"), Keep);
			Spec->TryGetStringField(TEXT("kind"), Option.Action);
			Spec->TryGetStringField(TEXT("label"), Option.Label);
			Spec->TryGetNumberField(TEXT("cost"), Cost);
			Spec->TryGetNumberField(TEXT("months"), Months);
			Spec->TryGetNumberField(TEXT("divs"), Divisions);
			Spec->TryGetBoolField(TEXT("ok"), Option.bEnabled);
			Spec->TryGetStringField(TEXT("why"), Option.Why);
			Option.Cost = Cost;
			Option.Months = Months;
			Option.Divisions = Divisions;
			Option.Keep = Keep;
			Option.Label = FString::Printf(TEXT("Raise: %s"), *Option.Label);
			Option.Detail = FString::Printf(TEXT("%d divisions  \u00B7  costs %s  \u00B7  ready in %d months"), Divisions, *FText::AsNumber(Cost).ToString(), Months);
			if (Keep > 0)
			{
				Option.Detail += FString::Printf(TEXT("  \u00B7  then %s a month to keep"), *FText::AsNumber(Keep).ToString());
			}
			Info.Options.Add(MoveTemp(Option));
		}
	}
	const TSharedPtr<FJsonObject>* Fort = nullptr;
	if (Object->TryGetObjectField(TEXT("fort"), Fort))
	{
		FProvinceOption Option;
		int32 Level = 0, Cost = 0, Months = 0;
		(*Fort)->TryGetNumberField(TEXT("level"), Level);
		(*Fort)->TryGetNumberField(TEXT("cost"), Cost);
		(*Fort)->TryGetNumberField(TEXT("months"), Months);
		(*Fort)->TryGetBoolField(TEXT("ok"), Option.bEnabled);
		(*Fort)->TryGetStringField(TEXT("why"), Option.Why);
		Option.Action = TEXT("fort");
		Option.Label = FString::Printf(TEXT("Build: fortifications, level %d"), Level);
		Option.Detail = FString::Printf(TEXT("Costs %s  \u00B7  finished in %d months"), *FText::AsNumber(Cost).ToString(), Months);
		Info.Options.Add(MoveTemp(Option));
	}
	// What can be done to the country itself: bridges, the railway, engineers' work.
	const TArray<TSharedPtr<FJsonValue>>* Jobs = nullptr;
	if (Object->TryGetArrayField(TEXT("jobs"), Jobs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Jobs)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FProvinceOption Option;
			FString Key;
			Entry->TryGetStringField(TEXT("key"), Key);
			Option.Action = TEXT("work:") + Key;
			Entry->TryGetStringField(TEXT("label"), Option.Label);
			Entry->TryGetStringField(TEXT("detail"), Option.Detail);
			Entry->TryGetStringField(TEXT("why"), Option.Why);
			Entry->TryGetBoolField(TEXT("ok"), Option.bEnabled);
			Info.Options.Add(MoveTemp(Option));
		}
	}
	Info.Forming = ReadOrders(Object, TEXT("forming"));
	Object->TryGetStringField(TEXT("formingHead"), Info.FormingHead);
	const TSharedPtr<FJsonObject>* GlanceObject = nullptr;
	if (Object->TryGetObjectField(TEXT("glance"), GlanceObject) && GlanceObject && GlanceObject->IsValid())
	{
		Info.Glance = ReadShelves(*GlanceObject);
	}
	return Info;
}

bool FCampaignSession::OrderInProvince(const FString& Action, int32 RegionId, FString& OutNote, TArray<FSortie>* OutSorties)
{
	TSharedPtr<FJsonObject> Object;
	if (Action.StartsWith(TEXT("work:")))
	{
		// A work on the country itself: a bridge blown, the railway torn up, engineers set to a task.
		if (!CallRules(TEXT("work"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Action.Mid(5)), RegionId), Object))
		{
			return false;
		}
		bool bDone = false;
		Object->TryGetBoolField(TEXT("ok"), bDone);
		Object->TryGetStringField(TEXT("note"), OutNote);
		const TArray<TSharedPtr<FJsonValue>>* Flies = nullptr;
		if (OutSorties && Object->TryGetArrayField(TEXT("flies"), Flies))
		{
			ReadSorties(*Flies, *OutSorties);
		}
		TakeView(Object);
		return bDone;
	}
	const bool bFort = Action == TEXT("fort");
	const FString Arguments = bFort ? FString::Printf(TEXT("[%d]"), RegionId) : FString::Printf(TEXT("[\"%s\",%d]"), *Action, RegionId);
	if (!CallRules(bFort ? TEXT("fortify") : TEXT("raise"), Arguments, Object))
	{
		return false;
	}
	bool bOk = false;
	FString Name, Due;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("name"), Name);
	Object->TryGetStringField(TEXT("due"), Due);
	TakeView(Object);
	OutNote = bOk ? FString::Printf(TEXT("%s ordered. Due %s."), *Name, *Due) : FString(TEXT("That cannot be ordered now."));
	return bOk;
}

FForcePanel FCampaignSession::Force(const FString& Kind, int32 Id)
{
	FForcePanel Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("force"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Kind), Id), Object))
	{
		return Out;
	}
	Out.bValid = Object->TryGetStringField(TEXT("name"), Out.Name);
	Object->TryGetStringField(TEXT("nat"), Out.Nation);
	Object->TryGetBoolField(TEXT("mine"), Out.bMine);
	Object->TryGetStringArrayField(TEXT("lines"), Out.Lines);
	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (Object->TryGetArrayField(TEXT("options"), Options))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Options)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FProvinceOption Option;
			Entry->TryGetStringField(TEXT("key"), Option.Action);
			Entry->TryGetStringField(TEXT("label"), Option.Label);
			Entry->TryGetStringField(TEXT("detail"), Option.Detail);
			Entry->TryGetStringField(TEXT("why"), Option.Why);
			Entry->TryGetBoolField(TEXT("ok"), Option.bEnabled);
			Out.Options.Add(MoveTemp(Option));
		}
	}
	return Out;
}

bool FCampaignSession::OrderForce(const FString& Key, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("forceOrder"), FString::Printf(TEXT("[%s]"), *JsonQuote(Key)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::JoinArmy(int32 RegionId, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("join"), FString::Printf(TEXT("[%d]"), RegionId), Object) && Object.IsValid())
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("note"), OutNote);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::SendBack(int32 CorpsId, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("sendBack"), FString::Printf(TEXT("[%d]"), CorpsId), Object) && Object.IsValid())
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("note"), OutNote);
		TakeView(Object);
	}
	return bOk;
}

int32 FCampaignSession::SeaAt(const FVector2D& MapPoint)
{
	TSharedPtr<FJsonObject> Object;
	int32 Sea = INDEX_NONE;
	if (CallRules(TEXT("seaAt"), FString::Printf(TEXT("[%.1f,%.1f]"), MapPoint.X, MapPoint.Y), Object) && Object.IsValid())
	{
		Object->TryGetNumberField(TEXT("sea"), Sea);
	}
	return Sea < 0 ? INDEX_NONE : Sea;
}

TArray<FTownInfo> FCampaignSession::Towns()
{
	TArray<FTownInfo> Out;
	TSharedPtr<FJsonObject> Object;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!CallRules(TEXT("towns"), TEXT("[]"), Object) || !Object.IsValid() || !Object->TryGetArrayField(TEXT("towns"), List))
	{
		return Out;
	}
	for (const TSharedPtr<FJsonValue>& Value : *List)
	{
		const TSharedPtr<FJsonObject> Entry = Value->AsObject();
		if (!Entry.IsValid())
		{
			continue;
		}
		FTownInfo& Town = Out.AddDefaulted_GetRef();
		Entry->TryGetNumberField(TEXT("region"), Town.Region);
		Entry->TryGetStringField(TEXT("name"), Town.Name);
		Entry->TryGetStringField(TEXT("province"), Town.Province);
		Entry->TryGetStringField(TEXT("size"), Town.Size);
		Entry->TryGetNumberField(TEXT("worth"), Town.Worth);
		Entry->TryGetNumberField(TEXT("rank"), Town.Rank);
	}
	return Out;
}

FLandFeatures FCampaignSession::Land()
{
	FLandFeatures Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("land"), TEXT("[]"), Object))
	{
		return Out;
	}
	auto Numbers = [](const TSharedPtr<FJsonValue>& Value)
	{
		TArray<double> List;
		for (const TSharedPtr<FJsonValue>& Entry : Value->AsArray())
		{
			List.Add(Entry->AsNumber());
		}
		return List;
	};
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object->TryGetArrayField(TEXT("rivers"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FLandRiver& River = Out.Rivers.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("name"), River.Name);
			Entry->TryGetStringField(TEXT("into"), River.Into);
			const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
			if (Entry->TryGetArrayField(TEXT("pts"), Points))
			{
				for (const TSharedPtr<FJsonValue>& Point : *Points)
				{
					const TArray<double> XY = Numbers(Point);
					if (XY.Num() >= 2)
					{
						River.Course.Add(FVector2D(XY[0], XY[1]));
					}
				}
			}
		}
	}
	if (Object->TryGetArrayField(TEXT("crossings"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TArray<double> N = Numbers(Value);
			if (N.Num() >= 5)
			{
				FLandCrossing& Crossing = Out.Crossings.AddDefaulted_GetRef();
				Crossing.A = static_cast<int32>(N[0]);
				Crossing.B = static_cast<int32>(N[1]);
				Crossing.River = static_cast<int32>(N[2]);
				Crossing.At = FVector2D(N[3], N[4]);
			}
		}
	}
	if (Object->TryGetArrayField(TEXT("rails"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TArray<double> N = Numbers(Value);
			if (N.Num() >= 4)
			{
				FLandRail& Rail = Out.Rails.AddDefaulted_GetRef();
				Rail.A = static_cast<int32>(N[0]);
				Rail.B = static_cast<int32>(N[1]);
				Rail.At = FVector2D(N[2], N[3]);
			}
		}
	}
	if (Object->TryGetArrayField(TEXT("steep"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			Out.Steep.Add(static_cast<int32>(Value->AsNumber()));
		}
	}
	return Out;
}

FArsenal FCampaignSession::Arsenal(int32 RegionId)
{
	FArsenal Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("arsenal"), FString::Printf(TEXT("[%d]"), RegionId), Object))
	{
		return Out;
	}
	Out.bValid = true;
	Object->TryGetBoolField(TEXT("budget"), Out.bBudget);
	Object->TryGetNumberField(TEXT("money"), Out.Money);
	Object->TryGetStringField(TEXT("region"), Out.Region);
	const TArray<TSharedPtr<FJsonValue>>* Offers = nullptr;
	if (Object->TryGetArrayField(TEXT("offers"), Offers))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Offers)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FArsenalOffer& Offer = Out.Offers.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("key"), Offer.Key);
			Entry->TryGetStringField(TEXT("group"), Offer.Group);
			Entry->TryGetStringField(TEXT("kind"), Offer.Kind);
			Entry->TryGetStringField(TEXT("label"), Offer.Label);
			Entry->TryGetStringField(TEXT("blurb"), Offer.Blurb);
			Entry->TryGetStringField(TEXT("size"), Offer.Size);
			Entry->TryGetStringField(TEXT("note"), Offer.Note);
			Entry->TryGetStringField(TEXT("why"), Offer.Why); // null when it can be ordered
			Entry->TryGetNumberField(TEXT("cost"), Offer.Cost);
			Entry->TryGetNumberField(TEXT("upkeep"), Offer.Upkeep);
			Entry->TryGetNumberField(TEXT("months"), Offer.Months);
			Entry->TryGetBoolField(TEXT("poor"), Offer.bPoor);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Orders = nullptr;
	if (Object->TryGetArrayField(TEXT("orders"), Orders))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Orders)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FLedgerLine& Line = Out.Orders.AddDefaulted_GetRef();
			FString Name, Where, Due;
			Entry->TryGetStringField(TEXT("name"), Name);
			Entry->TryGetStringField(TEXT("where"), Where);
			Entry->TryGetStringField(TEXT("due"), Due);
			Entry->TryGetNumberField(TEXT("cost"), Line.Amount);
			Line.Label = Where.IsEmpty() ? FString::Printf(TEXT("%s, due %s"), *Name, *Due) : FString::Printf(TEXT("%s, %s, due %s"), *Name, *Where, *Due);
		}
	}
	return Out;
}

bool FCampaignSession::OrderArsenal(const FString& Key, int32 RegionId, FString& OutName)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("order"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Key), RegionId), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("name"), OutName);
	TakeView(Object);
	return bOk;
}

FLedger FCampaignSession::Ledger()
{
	FLedger Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("ledger"), TEXT("[]"), Object))
	{
		return Out;
	}
	Out.bValid = true;
	Object->TryGetBoolField(TEXT("budget"), Out.bBudget);
	Object->TryGetStringField(TEXT("nation"), Out.Nation);
	Object->TryGetStringField(TEXT("date"), Out.Date);
	Object->TryGetNumberField(TEXT("money"), Out.Money);
	Object->TryGetNumberField(TEXT("gross"), Out.Gross);
	Object->TryGetNumberField(TEXT("upkeepTotal"), Out.UpkeepTotal);
	Object->TryGetNumberField(TEXT("net"), Out.Net);
	Object->TryGetNumberField(TEXT("debt"), Out.Debt);
	Object->TryGetNumberField(TEXT("debtFloor"), Out.DebtFloor);
	Object->TryGetNumberField(TEXT("onOrder"), Out.OnOrder);
	auto Lines = [&Object](const TCHAR* Field, TArray<FLedgerLine>& Into)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object->TryGetArrayField(Field, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FLedgerLine& Line = Into.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("label"), Line.Label);
				Entry->TryGetNumberField(TEXT("amount"), Line.Amount);
				Entry->TryGetNumberField(TEXT("count"), Line.Count);
			}
		}
	};
	Lines(TEXT("income"), Out.Income);
	Lines(TEXT("changes"), Out.Changes);
	Lines(TEXT("upkeep"), Out.Upkeep);
	const TArray<TSharedPtr<FJsonValue>>* Provinces = nullptr;
	if (Object->TryGetArrayField(TEXT("provinces"), Provinces))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Provinces)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FLedgerProvince& Province = Out.Provinces.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("id"), Province.Region);
			Entry->TryGetStringField(TEXT("name"), Province.Name);
			Entry->TryGetBoolField(TEXT("home"), Province.bHome);
			Entry->TryGetNumberField(TEXT("land"), Province.Land);
			Entry->TryGetNumberField(TEXT("port"), Province.Port);
			Entry->TryGetNumberField(TEXT("oil"), Province.Oil);
			Entry->TryGetNumberField(TEXT("fac"), Province.Factories);
			Entry->TryGetNumberField(TEXT("capital"), Province.Capital);
			Entry->TryGetNumberField(TEXT("total"), Province.Total);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Orders = nullptr;
	if (Object->TryGetArrayField(TEXT("orders"), Orders))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Orders)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FLedgerLine& Line = Out.Orders.AddDefaulted_GetRef();
			FString Name, Where, Due;
			Entry->TryGetStringField(TEXT("name"), Name);
			Entry->TryGetStringField(TEXT("where"), Where);
			Entry->TryGetStringField(TEXT("due"), Due);
			Entry->TryGetNumberField(TEXT("cost"), Line.Amount);
			Line.Label = FString::Printf(TEXT("%s, %s, due %s"), *Name, *Where, *Due);
		}
	}
	return Out;
}

FDiplomacy FCampaignSession::Diplomacy()
{
	FDiplomacy Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("diplomacy"), TEXT("[]"), Object) || !Object.IsValid())
	{
		return Out;
	}
	Out.bValid = Object->TryGetStringField(TEXT("nation"), Out.Nation);
	Object->TryGetBoolField(TEXT("up"), Out.bUp);
	Object->TryGetStringField(TEXT("date"), Out.Date);
	Object->TryGetStringField(TEXT("bloc"), Out.Bloc);
	Object->TryGetStringField(TEXT("wind"), Out.Wind);
	const TSharedPtr<FJsonObject>* Claims = nullptr;
	if (Object->TryGetObjectField(TEXT("claims"), Claims))
	{
		(*Claims)->TryGetStringField(TEXT("lead"), Out.ClaimsLead);
		const TArray<TSharedPtr<FJsonValue>>* ClaimRows = nullptr;
		if ((*Claims)->TryGetArrayField(TEXT("rows"), ClaimRows))
		{
			for (const TSharedPtr<FJsonValue>& Value : *ClaimRows)
			{
				const TSharedPtr<FJsonObject>* Claim = nullptr;
				if (Value->TryGetObject(Claim))
				{
					FDiplomacy::FClaim& Row = Out.Claims.AddDefaulted_GetRef();
					(*Claim)->TryGetStringField(TEXT("title"), Row.Title);
					(*Claim)->TryGetStringField(TEXT("from"), Row.From);
					(*Claim)->TryGetStringField(TEXT("tag"), Row.Tag);
					(*Claim)->TryGetNumberField(TEXT("state"), Row.State);
					(*Claim)->TryGetNumberField(TEXT("region"), Row.Region);
					(*Claim)->TryGetBoolField(TEXT("onYou"), Row.bOnYou);
				}
			}
		}
	}
	Object->TryGetStringField(TEXT("help"), Out.Help);
	Object->TryGetNumberField(TEXT("capital"), Out.Capital);
	Object->TryGetNumberField(TEXT("income"), Out.Income);
	Object->TryGetNumberField(TEXT("cap"), Out.Cap);
	Object->TryGetNumberField(TEXT("partners"), Out.Partners);
	Object->TryGetNumberField(TEXT("cost"), Out.Cost);
	const TArray<TSharedPtr<FJsonValue>>* Powers = nullptr;
	if (Object->TryGetArrayField(TEXT("powers"), Powers))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Powers)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FDiplomacyPower& Power = Out.Powers.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Power.Tag);
			Entry->TryGetStringField(TEXT("name"), Power.Name);
			Entry->TryGetStringField(TEXT("side"), Power.Side);
			Entry->TryGetStringField(TEXT("note"), Power.Note);
			Entry->TryGetNumberField(TEXT("divs"), Power.Divisions);
			double Will = 0.0;
			Entry->TryGetNumberField(TEXT("will"), Will);
			Power.Will = static_cast<float>(Will);
			Entry->TryGetBoolField(TEXT("alive"), Power.bAlive);
			Entry->TryGetBoolField(TEXT("war"), Power.bWar);
			const TSharedPtr<FJsonObject>* Staff = nullptr;
			if (Entry->TryGetObjectField(TEXT("staff"), Staff))
			{
				(*Staff)->TryGetStringField(TEXT("label"), Power.StaffLabel);
				(*Staff)->TryGetStringField(TEXT("line"), Power.StaffLine);
				(*Staff)->TryGetStringField(TEXT("says"), Power.StaffSays);
				(*Staff)->TryGetBoolField(TEXT("threat"), Power.bThreat);
				(*Staff)->TryGetNumberField(TEXT("target"), Power.ThreatRegion);
				(*Staff)->TryGetStringField(TEXT("targetName"), Power.ThreatName);
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (Object->TryGetArrayField(TEXT("rows"), Rows))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Rows)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FDiplomacyRow& Row = Out.Rows.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Row.Tag);
			Entry->TryGetStringField(TEXT("name"), Row.Name);
			Entry->TryGetStringField(TEXT("label"), Row.Label);
			Entry->TryGetStringField(TEXT("word"), Row.Word);
			Entry->TryGetStringField(TEXT("why"), Row.Why);
			Entry->TryGetStringField(TEXT("friends"), Row.Friends);
			Entry->TryGetNumberField(TEXT("opinion"), Row.Opinion);
			Entry->TryGetNumberField(TEXT("divs"), Row.Divisions);
			Entry->TryGetBoolField(TEXT("court"), Row.bCourt);
			Entry->TryGetBoolField(TEXT("canCourt"), Row.bCanCourt);
			Entry->TryGetBoolField(TEXT("trades"), Row.bTrades);
			Entry->TryGetBoolField(TEXT("war"), Row.bWar);
		}
	}
	const TSharedPtr<FJsonObject>* League = nullptr;
	if (Object->TryGetObjectField(TEXT("league"), League))
	{
		(*League)->TryGetBoolField(TEXT("founded"), Out.bLeague);
		(*League)->TryGetStringField(TEXT("full"), Out.LeagueName);
		(*League)->TryGetStringField(TEXT("since"), Out.LeagueSince);
		const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
		if ((*League)->TryGetArrayField(TEXT("members"), Members))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Members)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FDiplomacyPower& Member = Out.Members.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("nat"), Member.Tag);
				Entry->TryGetStringField(TEXT("name"), Member.Name);
				Entry->TryGetStringField(TEXT("note"), Member.Note);
				Entry->TryGetNumberField(TEXT("divs"), Member.Divisions);
				Entry->TryGetBoolField(TEXT("war"), Member.bWar);
				bool bYou = false;
				Entry->TryGetBoolField(TEXT("you"), bYou);
				Member.Side = bYou ? TEXT("own") : TEXT("other");
			}
		}
	}
	const TSharedPtr<FJsonObject>* Path = nullptr;
	if (Object->TryGetObjectField(TEXT("path"), Path))
	{
		(*Path)->TryGetStringField(TEXT("lead"), Out.PathLead);
		(*Path)->TryGetNumberField(TEXT("inviteCost"), Out.InviteCost);
		const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
		if ((*Path)->TryGetArrayField(TEXT("options"), Options))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Options)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FPathOption& Option = Out.Paths.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("id"), Option.Id);
				Entry->TryGetStringField(TEXT("title"), Option.Title);
				Entry->TryGetStringField(TEXT("text"), Option.Text);
				Entry->TryGetStringField(TEXT("why"), Option.Why);
				Entry->TryGetBoolField(TEXT("ok"), Option.bOk);
				Entry->TryGetBoolField(TEXT("now"), Option.bNow);
				Entry->TryGetBoolField(TEXT("final"), Option.bFinal);
			}
		}
		for (const TPair<const TCHAR*, TArray<FLeagueAnswer>*>& List : { TPair<const TCHAR*, TArray<FLeagueAnswer>*>(TEXT("partners"), &Out.Founders), TPair<const TCHAR*, TArray<FLeagueAnswer>*>(TEXT("invites"), &Out.Invites) })
		{
			const TArray<TSharedPtr<FJsonValue>>* Answers = nullptr;
			if ((*Path)->TryGetArrayField(List.Key, Answers))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Answers)
				{
					const TSharedPtr<FJsonObject> Entry = Value->AsObject();
					FLeagueAnswer& Answer = List.Value->AddDefaulted_GetRef();
					Entry->TryGetStringField(TEXT("nat"), Answer.Tag);
					Entry->TryGetStringField(TEXT("name"), Answer.Name);
					Entry->TryGetStringField(TEXT("why"), Answer.Why);
					Entry->TryGetBoolField(TEXT("ok"), Answer.bOk);
					Answer.bCan = Answer.bOk;
					Entry->TryGetBoolField(TEXT("can"), Answer.bCan);
				}
			}
		}
	}
	return Out;
}

TArray<FString> FCampaignSession::Abilities(bool& bOutActive)
{
	TArray<FString> Lines;
	bOutActive = false;
	TSharedPtr<FJsonObject> Object;
	if (CallRules(TEXT("abilities"), TEXT("[]"), Object) && Object.IsValid())
	{
		Object->TryGetBoolField(TEXT("active"), bOutActive);
		Object->TryGetStringArrayField(TEXT("lines"), Lines);
	}
	return Lines;
}

namespace
{
	void ReadButtons(const TSharedPtr<FJsonObject>& From, const TCHAR* Field, TArray<FPageButton>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Buttons = nullptr;
		if (From->TryGetArrayField(Field, Buttons))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Buttons)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FPageButton& Button = Out.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("label"), Button.Label);
				Entry->TryGetStringField(TEXT("act"), Button.Act);
				Entry->TryGetStringField(TEXT("tip"), Button.Tip);
				Entry->TryGetBoolField(TEXT("ok"), Button.bOk);
				Entry->TryGetBoolField(TEXT("danger"), Button.bDanger);
				Entry->TryGetBoolField(TEXT("safe"), Button.bSafe); // (release 2, formations offered: GO BACK on a question asked in place)
			}
		}
	}
}

namespace
{
	/** (Release 2, the home front board.) A building on the board, as the bridge lists it. */
	void ReadHomeBuild(const TSharedPtr<FJsonObject>& From, FHomeBuild& Out)
	{
		From->TryGetStringField(TEXT("line"), Out.Line);
		From->TryGetStringField(TEXT("name"), Out.Name);
		From->TryGetStringField(TEXT("label"), Out.Label);
		From->TryGetStringField(TEXT("effect"), Out.Effect);
		From->TryGetStringField(TEXT("why"), Out.Why);
		From->TryGetStringField(TEXT("act"), Out.Act);
		From->TryGetNumberField(TEXT("cost"), Out.Cost);
		From->TryGetNumberField(TEXT("months"), Out.Months);
		From->TryGetBoolField(TEXT("ok"), Out.bOk);
	}

	/** (Release 2.) The home front board: the bridge's homeBoard, field for field. */
	void ReadHomeBoard(const TSharedPtr<FJsonObject>& Told, FHomeBoard& Board)
	{
		Board.bValid = true;
		Told->TryGetStringField(TEXT("nation"), Board.Nation);
		Told->TryGetStringField(TEXT("date"), Board.Date);
		Told->TryGetBoolField(TEXT("up"), Board.bUp);
		Told->TryGetNumberField(TEXT("money"), Board.Money);
		Told->TryGetNumberField(TEXT("net"), Board.Net);
		const TArray<TSharedPtr<FJsonValue>>* Tiles = nullptr;
		if (Told->TryGetArrayField(TEXT("tiles"), Tiles))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Tiles)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				if (Value->TryGetObject(Entry))
				{
					FHomeTile& Tile = Board.Tiles.AddDefaulted_GetRef();
					(*Entry)->TryGetStringField(TEXT("k"), Tile.Kind);
					(*Entry)->TryGetStringField(TEXT("label"), Tile.Label);
					(*Entry)->TryGetStringField(TEXT("figure"), Tile.Figure);
					(*Entry)->TryGetStringField(TEXT("sub"), Tile.Sub);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Counts = nullptr;
		if (Told->TryGetObjectField(TEXT("counts"), Counts) && Counts && Counts->IsValid())
		{
			(*Counts)->TryGetNumberField(TEXT("ready"), Board.Ready);
			(*Counts)->TryGetNumberField(TEXT("going"), Board.Going);
			(*Counts)->TryGetNumberField(TEXT("all"), Board.All);
		}
		const TArray<TSharedPtr<FJsonValue>>* Towns = nullptr;
		if (Told->TryGetArrayField(TEXT("towns"), Towns))
		{
			Board.Towns.Reserve(Towns->Num());
			for (const TSharedPtr<FJsonValue>& Value : *Towns)
			{
				const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
				if (!Value->TryGetObject(EntryPtr))
				{
					continue;
				}
				const TSharedPtr<FJsonObject>& Entry = *EntryPtr;
				FHomeTown& Town = Board.Towns.AddDefaulted_GetRef();
				Entry->TryGetNumberField(TEXT("region"), Town.Region);
				Entry->TryGetStringField(TEXT("town"), Town.Town);
				Entry->TryGetStringField(TEXT("province"), Town.Province);
				Entry->TryGetStringField(TEXT("size"), Town.Size);
				Entry->TryGetStringField(TEXT("state"), Town.State);
				Entry->TryGetStringField(TEXT("note"), Town.Note);
				Entry->TryGetBoolField(TEXT("own"), Town.bOwn);
				Entry->TryGetNumberField(TEXT("pays"), Town.Pays);
				const TArray<TSharedPtr<FJsonValue>>* Plots = nullptr;
				if (Entry->TryGetArrayField(TEXT("plots"), Plots))
				{
					for (const TSharedPtr<FJsonValue>& PlotValue : *Plots)
					{
						const TSharedPtr<FJsonObject>* PlotEntry = nullptr;
						if (PlotValue->TryGetObject(PlotEntry))
						{
							FHomePlot& Plot = Town.Plots.AddDefaulted_GetRef();
							(*PlotEntry)->TryGetStringField(TEXT("s"), Plot.State);
							(*PlotEntry)->TryGetStringField(TEXT("line"), Plot.Line);
							(*PlotEntry)->TryGetStringField(TEXT("name"), Plot.Name);
							(*PlotEntry)->TryGetNumberField(TEXT("level"), Plot.Level);
							(*PlotEntry)->TryGetNumberField(TEXT("months"), Plot.Months);
						}
					}
				}
				const TSharedPtr<FJsonObject>* Best = nullptr;
				if (Entry->TryGetObjectField(TEXT("best"), Best) && Best && Best->IsValid())
				{
					Town.bBest = true;
					ReadHomeBuild(*Best, Town.Best);
				}
				const TArray<TSharedPtr<FJsonValue>>* More = nullptr;
				if (Entry->TryGetArrayField(TEXT("more"), More))
				{
					for (const TSharedPtr<FJsonValue>& MoreValue : *More)
					{
						const TSharedPtr<FJsonObject>* MoreEntry = nullptr;
						if (MoreValue->TryGetObject(MoreEntry))
						{
							ReadHomeBuild(*MoreEntry, Town.More.AddDefaulted_GetRef());
						}
					}
				}
				const TSharedPtr<FJsonObject>* Going = nullptr;
				if (Entry->TryGetObjectField(TEXT("going"), Going) && Going && Going->IsValid())
				{
					Town.bGoing = true;
					(*Going)->TryGetNumberField(TEXT("id"), Town.GoingId);
					(*Going)->TryGetNumberField(TEXT("months"), Town.GoingMonths);
					(*Going)->TryGetStringField(TEXT("name"), Town.GoingName);
					(*Going)->TryGetStringField(TEXT("due"), Town.GoingDue);
					(*Going)->TryGetStringField(TEXT("act"), Town.GoingAct);
					(*Going)->TryGetBoolField(TEXT("cancel"), Town.bGoingCancel);
				}
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Raise = nullptr;
		if (Told->TryGetArrayField(TEXT("raise"), Raise))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Raise)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				if (Value->TryGetObject(Entry))
				{
					FHomeRaise& Offer = Board.Raise.AddDefaulted_GetRef();
					(*Entry)->TryGetStringField(TEXT("key"), Offer.Key);
					(*Entry)->TryGetStringField(TEXT("group"), Offer.Group);
					(*Entry)->TryGetStringField(TEXT("label"), Offer.Label);
					(*Entry)->TryGetStringField(TEXT("where"), Offer.Where);
					(*Entry)->TryGetStringField(TEXT("note"), Offer.Note);
					(*Entry)->TryGetStringField(TEXT("why"), Offer.Why);
					(*Entry)->TryGetStringField(TEXT("act"), Offer.Act);
					(*Entry)->TryGetNumberField(TEXT("cost"), Offer.Cost);
					(*Entry)->TryGetNumberField(TEXT("months"), Offer.Months);
					(*Entry)->TryGetBoolField(TEXT("ok"), Offer.bOk);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Again = nullptr;
		if (Told->TryGetObjectField(TEXT("again"), Again) && Again && Again->IsValid())
		{
			Board.bAgain = true;
			(*Again)->TryGetBoolField(TEXT("ok"), Board.bAgainOk);
			(*Again)->TryGetStringField(TEXT("when"), Board.AgainWhen);
			(*Again)->TryGetStringField(TEXT("text"), Board.AgainText);
			(*Again)->TryGetStringField(TEXT("left"), Board.AgainLeft);
			(*Again)->TryGetStringField(TEXT("why"), Board.AgainWhy);
			(*Again)->TryGetStringField(TEXT("act"), Board.AgainAct);
			(*Again)->TryGetNumberField(TEXT("cost"), Board.AgainCost);
		}
		const TArray<TSharedPtr<FJsonValue>>* Orders = nullptr;
		if (Told->TryGetArrayField(TEXT("orders"), Orders))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Orders)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				if (Value->TryGetObject(Entry))
				{
					FHomeOrder& Order = Board.Orders.AddDefaulted_GetRef();
					(*Entry)->TryGetNumberField(TEXT("id"), Order.Id);
					(*Entry)->TryGetNumberField(TEXT("region"), Order.Region);
					(*Entry)->TryGetStringField(TEXT("what"), Order.What);
					(*Entry)->TryGetStringField(TEXT("name"), Order.Name);
					(*Entry)->TryGetStringField(TEXT("where"), Order.Where);
					(*Entry)->TryGetStringField(TEXT("due"), Order.Due);
					(*Entry)->TryGetStringField(TEXT("act"), Order.Act);
					(*Entry)->TryGetNumberField(TEXT("months"), Order.Months);
					(*Entry)->TryGetNumberField(TEXT("cost"), Order.Cost);
					(*Entry)->TryGetBoolField(TEXT("cancel"), Order.bCancel);
					(*Entry)->TryGetNumberField(TEXT("share"), Order.Share);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Staff = nullptr;
		if (Told->TryGetObjectField(TEXT("staff"), Staff) && Staff && Staff->IsValid())
		{
			Board.bStaff = true;
			(*Staff)->TryGetBoolField(TEXT("on"), Board.bStaffOn);
			(*Staff)->TryGetStringField(TEXT("line"), Board.StaffLine);
			(*Staff)->TryGetStringField(TEXT("label"), Board.StaffLabel);
			(*Staff)->TryGetStringField(TEXT("act"), Board.StaffAct);
			(*Staff)->TryGetStringField(TEXT("tip"), Board.StaffTip);
			(*Staff)->TryGetNumberField(TEXT("cap"), Board.StaffCap);
			const TArray<TSharedPtr<FJsonValue>>* Caps = nullptr;
			if ((*Staff)->TryGetArrayField(TEXT("caps"), Caps))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Caps)
				{
					const TSharedPtr<FJsonObject>* Entry = nullptr;
					int32 Share = 0;
					if (Value->TryGetObject(Entry) && (*Entry)->TryGetNumberField(TEXT("n"), Share))
					{
						Board.StaffCaps.Add(Share);
					}
				}
			}
			(*Staff)->TryGetStringField(TEXT("head"), Board.StaffHead);
			const TArray<TSharedPtr<FJsonValue>>* Desks = nullptr;
			if ((*Staff)->TryGetArrayField(TEXT("desks"), Desks))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Desks)
				{
					const TSharedPtr<FJsonObject>* Entry = nullptr;
					if (!Value->TryGetObject(Entry))
					{
						continue;
					}
					FHomeDesk& Half = Board.StaffDesks.AddDefaulted_GetRef();
					(*Entry)->TryGetStringField(TEXT("name"), Half.Name);
					(*Entry)->TryGetBoolField(TEXT("on"), Half.bOn);
					(*Entry)->TryGetStringField(TEXT("act"), Half.Act);
					(*Entry)->TryGetStringField(TEXT("tip"), Half.Tip);
				}
			}
		}
	}
}

FOfficePage FCampaignSession::Page(const FString& Id, int32 RegionId)
{
	FOfficePage Out;
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (!CallRules(TEXT("page"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Id), RegionId), Object) || !Object.IsValid() || !Object->TryGetBoolField(TEXT("ok"), bOk) || !bOk)
	{
		return Out;
	}
	Out.bValid = true;
	Out.Id = Id;
	Object->TryGetStringField(TEXT("title"), Out.Title);
	Object->TryGetStringField(TEXT("headline"), Out.Headline);
	Object->TryGetStringField(TEXT("note"), Out.Note);
	ReadButtons(Object, TEXT("top"), Out.Top);
	// (Ruling 54.) A raid being planned comes with its card: the target, the groups and the forecast, as the rules reckon them.
	// (Ruling 64.) The month's report comes as one card.
	const TSharedPtr<FJsonObject>* MonthObject = nullptr;
	if (Object->TryGetObjectField(TEXT("month"), MonthObject) && MonthObject && MonthObject->IsValid())
	{
		const TSharedPtr<FJsonObject>& Told = *MonthObject;
		FMonthCard& Month = Out.Month;
		Month.bValid = true;
		Told->TryGetBoolField(TEXT("opening"), Month.bOpening);
		Told->TryGetStringField(TEXT("date"), Month.Date);
		Told->TryGetStringField(TEXT("none"), Month.None);
		Told->TryGetStringField(TEXT("foot"), Month.Foot);
		Told->TryGetStringField(TEXT("count"), Month.Count);
		Told->TryGetStringField(TEXT("rest"), Month.Rest);
		// (Release 2, formations offered.) The card's one row for them, where the rules send one: { text, amber, act }.
		const TSharedPtr<FJsonObject>* OffersRow = nullptr;
		if (Told->TryGetObjectField(TEXT("offersRow"), OffersRow) && OffersRow && OffersRow->IsValid())
		{
			(*OffersRow)->TryGetStringField(TEXT("text"), Month.OffersText);
			(*OffersRow)->TryGetBoolField(TEXT("amber"), Month.bOffersAmber);
			(*OffersRow)->TryGetStringField(TEXT("act"), Month.OffersAct);
		}
		auto ReadFigures = [](const TSharedPtr<FJsonObject>& From, TArray<FMonthFigure>& Into)
		{
			const TArray<TSharedPtr<FJsonValue>>* Figures = nullptr;
			if (From->TryGetArrayField(TEXT("figures"), Figures))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Figures)
				{
					const TSharedPtr<FJsonObject> Each = Value->AsObject();
					FMonthFigure& Figure = Into.AddDefaulted_GetRef();
					Each->TryGetStringField(TEXT("value"), Figure.Value);
					Each->TryGetStringField(TEXT("label"), Figure.Label);
				}
			}
		};
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Told->TryGetArrayField(TEXT("bands"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Each = Value->AsObject();
				FMonthBand& Band = Month.Bands.AddDefaulted_GetRef();
				Each->TryGetStringField(TEXT("kind"), Band.Kind);
				Each->TryGetStringArrayField(TEXT("flags"), Band.Flags);
				Each->TryGetStringField(TEXT("text"), Band.Text);
				Each->TryGetStringField(TEXT("tip"), Band.Tip);
				Each->TryGetNumberField(TEXT("week"), Band.Week);
				Each->TryGetStringField(TEXT("when"), Band.When);
				Each->TryGetNumberField(TEXT("region"), Band.Region);
				Each->TryGetStringField(TEXT("title"), Band.Title);
				Each->TryGetStringField(TEXT("sub"), Band.Sub);
				Each->TryGetStringField(TEXT("note"), Band.Note);
				Each->TryGetBoolField(TEXT("good"), Band.bGood);
				Each->TryGetBoolField(TEXT("spent"), Band.bSpent);
				Each->TryGetBoolField(TEXT("alarm"), Band.bAlarm);
			}
		}
		if (Told->TryGetArrayField(TEXT("score"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Each = Value->AsObject();
				FMonthScore& Box = Month.Score.AddDefaulted_GetRef();
				Each->TryGetStringField(TEXT("label"), Box.Label);
				Each->TryGetStringField(TEXT("sign"), Box.Sign);
				Each->TryGetStringField(TEXT("figure"), Box.Figure);
				Each->TryGetStringField(TEXT("sub"), Box.Sub);
				Each->TryGetNumberField(TEXT("tone"), Box.Tone);
			}
		}
		const TSharedPtr<FJsonObject>* LeadObject = nullptr;
		if (Told->TryGetObjectField(TEXT("lead"), LeadObject) && LeadObject && LeadObject->IsValid())
		{
			const TSharedPtr<FJsonObject>& Each = *LeadObject;
			FMonthLead& Lead = Month.Lead;
			Lead.bValid = true;
			Each->TryGetStringField(TEXT("tag"), Lead.Tag);
			Each->TryGetStringField(TEXT("stamp"), Lead.Stamp);
			Each->TryGetNumberField(TEXT("tone"), Lead.Tone);
			Each->TryGetStringField(TEXT("headline"), Lead.Headline);
			Each->TryGetStringField(TEXT("a"), Lead.A);
			Each->TryGetStringField(TEXT("aName"), Lead.AName);
			Each->TryGetStringField(TEXT("b"), Lead.B);
			Each->TryGetStringField(TEXT("bName"), Lead.BName);
			Each->TryGetStringField(TEXT("picture"), Lead.Picture);
			Each->TryGetStringField(TEXT("line"), Lead.Line);
			Each->TryGetNumberField(TEXT("region"), Lead.Region);
			Each->TryGetNumberField(TEXT("report"), Lead.Report);
			ReadFigures(Each, Lead.Figures);
		}
		if (Told->TryGetArrayField(TEXT("rows"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Each = Value->AsObject();
				FMonthRow& Row = Month.Rows.AddDefaulted_GetRef();
				Each->TryGetStringField(TEXT("picture"), Row.Picture);
				Each->TryGetStringField(TEXT("sign"), Row.Sign);
				Each->TryGetStringField(TEXT("place"), Row.Place);
				Each->TryGetStringField(TEXT("tag"), Row.Tag);
				Each->TryGetNumberField(TEXT("tone"), Row.Tone);
				Each->TryGetStringField(TEXT("a"), Row.A);
				Each->TryGetStringField(TEXT("b"), Row.B);
				Each->TryGetStringField(TEXT("join"), Row.JoinWord);
				Each->TryGetStringField(TEXT("joinName"), Row.JoinName);
				Each->TryGetStringField(TEXT("text"), Row.Text);
				Each->TryGetNumberField(TEXT("region"), Row.Region);
				Each->TryGetNumberField(TEXT("sea"), Row.Sea);
				ReadFigures(Each, Row.Figures);
			}
		}
		if (Told->TryGetArrayField(TEXT("home"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Each = Value->AsObject();
				FMonthHome& Line = Month.Home.AddDefaulted_GetRef();
				Each->TryGetStringField(TEXT("sign"), Line.Sign);
				Each->TryGetStringField(TEXT("count"), Line.Count);
				Each->TryGetStringField(TEXT("text"), Line.Text);
				Each->TryGetStringField(TEXT("where"), Line.Where);
				Each->TryGetBoolField(TEXT("grey"), Line.bGrey);
				Each->TryGetStringField(TEXT("tip"), Line.Tip);
			}
		}
		if (Told->TryGetArrayField(TEXT("staff"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Each = Value->AsObject();
				FMonthStaff& Line = Month.Staff.AddDefaulted_GetRef();
				Each->TryGetStringField(TEXT("text"), Line.Text);
				Each->TryGetStringField(TEXT("act"), Line.Act);
				Each->TryGetBoolField(TEXT("ok"), Line.bOk);
				Each->TryGetStringField(TEXT("tip"), Line.Tip);
			}
		}
		if (Told->TryGetArrayField(TEXT("elsewhere"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject> Each = Value->AsObject();
				FMonthElse& Line = Month.Elsewhere.AddDefaulted_GetRef();
				Each->TryGetStringField(TEXT("nat"), Line.Tag);
				Each->TryGetStringField(TEXT("text"), Line.Text);
				Each->TryGetStringField(TEXT("tip"), Line.Tip);
			}
		}
	}
	const TSharedPtr<FJsonObject>* CardObject = nullptr;
	if (Object->TryGetObjectField(TEXT("card"), CardObject) && CardObject && CardObject->IsValid())
	{
		const TSharedPtr<FJsonObject>& Told = *CardObject;
		FRaidCard& Card = Out.Card;
		Card.bValid = true;
		Told->TryGetStringField(TEXT("kind"), Card.Kind);
		Told->TryGetStringField(TEXT("kindLabel"), Card.KindLabel);
		Told->TryGetStringField(TEXT("why"), Card.Why);
		Told->TryGetStringField(TEXT("what"), Card.What);
		Told->TryGetStringField(TEXT("effect"), Card.Effect);
		Told->TryGetStringField(TEXT("effects"), Card.Effects);
		Told->TryGetStringField(TEXT("from"), Card.From);
		Told->TryGetStringField(TEXT("holderName"), Card.HolderName);
		Told->TryGetStringField(TEXT("allAct"), Card.AllAct);
		Told->TryGetStringField(TEXT("otherAct"), Card.OtherAct);
		Told->TryGetNumberField(TEXT("inReach"), Card.InReach);
		const TArray<TSharedPtr<FJsonValue>>* Hits = nullptr;
		if (Told->TryGetArrayField(TEXT("hits"), Hits))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Hits)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FString HitNumber, HitWords;
				Entry->TryGetStringField(TEXT("n"), HitNumber);
				Entry->TryGetStringField(TEXT("words"), HitWords);
				Card.Hits.Emplace(HitNumber, HitWords);
			}
		}
		Told->TryGetStringField(TEXT("verdict"), Card.Verdict);
		Told->TryGetStringField(TEXT("massed"), Card.Massed);
		Told->TryGetStringField(TEXT("offPatrol"), Card.OffPatrol);
		Told->TryGetStringField(TEXT("ticket"), Card.Ticket);
		Told->TryGetNumberField(TEXT("success"), Card.Success);
		Told->TryGetNumberField(TEXT("bombers"), Card.Bombers);
		Told->TryGetNumberField(TEXT("bomberLoss"), Card.BomberLoss);
		Told->TryGetNumberField(TEXT("bomberShare"), Card.BomberShare);
		Told->TryGetNumberField(TEXT("fighters"), Card.Fighters);
		Told->TryGetNumberField(TEXT("fighterLoss"), Card.FighterLoss);
		Told->TryGetNumberField(TEXT("foeLoss"), Card.FoeLoss);
		Told->TryGetStringField(TEXT("gunsWords"), Card.GunsWords);
		Told->TryGetStringField(TEXT("toGuns"), Card.ToGuns);
		Told->TryGetStringField(TEXT("types"), Card.Types);
		Told->TryGetStringField(TEXT("typesEdge"), Card.TypesEdge);
		Told->TryGetStringField(TEXT("pilots"), Card.Pilots);
		Told->TryGetStringField(TEXT("typesTip"), Card.TypesTip);
		// (Ruling 107(d).) A bombing raid by day and by night.
		const TSharedPtr<FJsonObject>* WaysObject = nullptr;
		if (Told->TryGetObjectField(TEXT("ways"), WaysObject) && WaysObject && WaysObject->IsValid())
		{
			(*WaysObject)->TryGetStringField(TEXT("label"), Card.WaysLabel);
			(*WaysObject)->TryGetStringField(TEXT("foes"), Card.WaysFoes);
			(*WaysObject)->TryGetStringField(TEXT("foesWords"), Card.WaysFoesWords);
			(*WaysObject)->TryGetNumberField(TEXT("bombers"), Card.WaysBombers);
			(*WaysObject)->TryGetNumberField(TEXT("guns"), Card.WaysGuns);
			(*WaysObject)->TryGetStringField(TEXT("gunsFigure"), Card.WaysGunsFigure);
			(*WaysObject)->TryGetStringField(TEXT("gunsNoun"), Card.WaysGunsNoun);
			(*WaysObject)->TryGetStringField(TEXT("gunsTail"), Card.WaysGunsTail);
			(*WaysObject)->TryGetStringField(TEXT("gunsTip"), Card.WaysGunsTip);
			for (const TCHAR* Which : { TEXT("day"), TEXT("night") })
			{
				const TSharedPtr<FJsonObject>* WayObject = nullptr;
				if (!(*WaysObject)->TryGetObjectField(Which, WayObject) || !WayObject || !WayObject->IsValid())
				{
					continue;
				}
				FRaidWay& Way = Card.Ways.AddDefaulted_GetRef();
				(*WayObject)->TryGetStringField(TEXT("label"), Way.Label);
				(*WayObject)->TryGetStringField(TEXT("why"), Way.Why);
				(*WayObject)->TryGetStringField(TEXT("verdict"), Way.Verdict);
				(*WayObject)->TryGetStringField(TEXT("lost"), Way.Lost);
				(*WayObject)->TryGetStringField(TEXT("fightersLost"), Way.FightersLost);
				(*WayObject)->TryGetStringField(TEXT("foot"), Way.Foot);
				(*WayObject)->TryGetStringField(TEXT("toGuns"), Way.ToGuns);
				(*WayObject)->TryGetStringField(TEXT("gunsWords"), Way.GunsWords);
				(*WayObject)->TryGetStringField(TEXT("stopNote"), Way.StopNote);
				(*WayObject)->TryGetStringField(TEXT("act"), Way.Act);
				(*WayObject)->TryGetStringField(TEXT("tip"), Way.Tip);
				(*WayObject)->TryGetNumberField(TEXT("success"), Way.Success);
				(*WayObject)->TryGetNumberField(TEXT("lostShare"), Way.LostShare);
				(*WayObject)->TryGetNumberField(TEXT("fighterShare"), Way.FighterShare);
				(*WayObject)->TryGetBoolField(TEXT("night"), Way.bNight);
				(*WayObject)->TryGetBoolField(TEXT("chosen"), Way.bChosen);
				const TArray<TSharedPtr<FJsonValue>>* WayHits = nullptr;
				if ((*WayObject)->TryGetArrayField(TEXT("hits"), WayHits))
				{
					for (const TSharedPtr<FJsonValue>& Value : *WayHits)
					{
						const TSharedPtr<FJsonObject> Entry = Value->AsObject();
						FString HitNumber, HitWords;
						bool bHitDim = false;
						Entry->TryGetStringField(TEXT("n"), HitNumber);
						Entry->TryGetStringField(TEXT("words"), HitWords);
						Entry->TryGetBoolField(TEXT("dim"), bHitDim);
						Way.Hits.Emplace(HitNumber, HitWords);
						Way.HitDim.Add(bHitDim);
					}
				}
			}
		}
		auto ReadStop = [](const TSharedPtr<FJsonObject>& Entry, FRaidStop& Stop)
		{
			Entry->TryGetNumberField(TEXT("id"), Stop.Id);
			Entry->TryGetStringField(TEXT("name"), Stop.Name);
			Entry->TryGetStringField(TEXT("sort"), Stop.Sort);
			Entry->TryGetStringField(TEXT("holder"), Stop.Holder);
			Entry->TryGetStringField(TEXT("act"), Stop.Act);
			Entry->TryGetStringField(TEXT("picture"), Stop.Picture);
			Entry->TryGetStringField(TEXT("title"), Stop.Title);
			Entry->TryGetStringField(TEXT("who"), Stop.Who);
			Entry->TryGetNumberField(TEXT("km"), Stop.Km);
			Entry->TryGetNumberField(TEXT("guns"), Stop.Guns);
			Entry->TryGetBoolField(TEXT("now"), Stop.bNow);
		};
		const TSharedPtr<FJsonObject>* TargetObject = nullptr;
		if (Told->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
		{
			ReadStop(*TargetObject, Card.Target);
		}
		const TArray<TSharedPtr<FJsonValue>>* Strip = nullptr;
		if (Told->TryGetArrayField(TEXT("strip"), Strip))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Strip)
			{
				ReadStop(Value->AsObject(), Card.Strip.AddDefaulted_GetRef());
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Seats = nullptr;
		if (Told->TryGetArrayField(TEXT("seats"), Seats))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Seats)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FRaidSeat& Seat = Card.Seats.AddDefaulted_GetRef();
				Entry->TryGetNumberField(TEXT("id"), Seat.Id);
				Entry->TryGetStringField(TEXT("name"), Seat.Name);
				Entry->TryGetStringField(TEXT("kind"), Seat.Kind);
				Entry->TryGetStringField(TEXT("nat"), Seat.Nation);
				Entry->TryGetStringField(TEXT("base"), Seat.Base);
				Entry->TryGetStringField(TEXT("why"), Seat.Why);
				Entry->TryGetStringField(TEXT("note"), Seat.Note);
				Entry->TryGetStringField(TEXT("act"), Seat.Act);
				Entry->TryGetNumberField(TEXT("aircraft"), Seat.Aircraft);
				Entry->TryGetNumberField(TEXT("km"), Seat.Km);
				Entry->TryGetNumberField(TEXT("ready"), Seat.Ready);
				Entry->TryGetBoolField(TEXT("picked"), Seat.bPicked);
				Entry->TryGetBoolField(TEXT("locked"), Seat.bLocked);
				Entry->TryGetBoolField(TEXT("lead"), Seat.bLead);
				Entry->TryGetBoolField(TEXT("afloat"), Seat.bAfloat);
				Entry->TryGetBoolField(TEXT("stays"), Seat.bStays);
				Entry->TryGetBoolField(TEXT("dayOnly"), Seat.bDayOnly);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Meets = nullptr;
		if (Told->TryGetArrayField(TEXT("meets"), Meets))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Meets)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FRaidMeet& Meet = Card.Meets.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("nat"), Meet.Tag);
				Entry->TryGetStringField(TEXT("name"), Meet.Name);
				Entry->TryGetStringField(TEXT("base"), Meet.Base);
				Entry->TryGetNumberField(TEXT("ftr"), Meet.Fighters);
				Entry->TryGetNumberField(TEXT("able"), Meet.Able);
			}
		}
		const TSharedPtr<FJsonObject>* FlyObject = nullptr;
		if (Told->TryGetObjectField(TEXT("fly"), FlyObject) && FlyObject && FlyObject->IsValid())
		{
			(*FlyObject)->TryGetStringField(TEXT("label"), Card.Fly.Label);
			(*FlyObject)->TryGetStringField(TEXT("act"), Card.Fly.Act);
			(*FlyObject)->TryGetStringField(TEXT("tip"), Card.Fly.Tip);
			(*FlyObject)->TryGetBoolField(TEXT("ok"), Card.Fly.bOk);
		}
		// (Release 2, atom.ts) A raid that carries the atomic bomb, and its BACK.
		Told->TryGetBoolField(TEXT("bomb"), Card.bBomb);
		const TSharedPtr<FJsonObject>* BackObject = nullptr;
		if (Told->TryGetObjectField(TEXT("back"), BackObject) && BackObject && BackObject->IsValid())
		{
			(*BackObject)->TryGetStringField(TEXT("label"), Card.Back.Label);
			(*BackObject)->TryGetStringField(TEXT("act"), Card.Back.Act);
			(*BackObject)->TryGetStringField(TEXT("tip"), Card.Back.Tip);
			(*BackObject)->TryGetBoolField(TEXT("ok"), Card.Back.bOk);
		}
		Card.bBomb = Card.bBomb && !Card.Back.Act.IsEmpty();
	}
	// (Release 2, the home front board.) The room comes with its board: every town, what can be raised, what is on order.
	const TSharedPtr<FJsonObject>* HomeObject = nullptr;
	if (Object->TryGetObjectField(TEXT("home"), HomeObject) && HomeObject && HomeObject->IsValid())
	{
		ReadHomeBoard(*HomeObject, Out.Home);
	}
	const TArray<TSharedPtr<FJsonValue>>* Sections = nullptr;
	if (Object->TryGetArrayField(TEXT("sections"), Sections))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Sections)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FPageSection& Section = Out.Sections.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("heading"), Section.Heading);
			Entry->TryGetStringField(TEXT("text"), Section.Text);
			const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
			if (Entry->TryGetArrayField(TEXT("rows"), Rows))
			{
				for (const TSharedPtr<FJsonValue>& RowValue : *Rows)
				{
					const TSharedPtr<FJsonObject> RowEntry = RowValue->AsObject();
					FPageRow& Row = Section.Rows.AddDefaulted_GetRef();
					RowEntry->TryGetStringField(TEXT("nat"), Row.Tag);
					RowEntry->TryGetStringField(TEXT("title"), Row.Title);
					RowEntry->TryGetStringArrayField(TEXT("lines"), Row.Lines);
					ReadButtons(RowEntry, TEXT("buttons"), Row.Buttons);
					RowEntry->TryGetNumberField(TEXT("meter"), Row.Meter);
					RowEntry->TryGetStringField(TEXT("alarm"), Row.Alarm);
					RowEntry->TryGetStringField(TEXT("picture"), Row.Picture);
					RowEntry->TryGetStringArrayField(TEXT("tones"), Row.Tones); // (release 2, formations offered: a line plain, muted or amber)
				}
			}
		}
	}
	return Out;
}

/** The ships seen to go down, as the bridge lists them under 'sunk' (a month's fights at sea, or a fleet's attack just fought). */
static void ReadSinkings(const TArray<TSharedPtr<FJsonValue>>& List, TArray<FSinking>& Out)
{
	for (const TSharedPtr<FJsonValue>& WentDown : List)
	{
		const TSharedPtr<FJsonObject>* Lost = nullptr;
		if (WentDown->TryGetObject(Lost))
		{
			FSinking& Sinking = Out.AddDefaulted_GetRef();
			(*Lost)->TryGetNumberField(TEXT("zone"), Sinking.Zone);
			(*Lost)->TryGetNumberField(TEXT("region"), Sinking.Region);
			(*Lost)->TryGetStringField(TEXT("nat"), Sinking.Nation);
			(*Lost)->TryGetStringField(TEXT("cls"), Sinking.Class);
			(*Lost)->TryGetNumberField(TEXT("n"), Sinking.Count);
			// (Brian's memorial card) The battleships and carriers among them, by name.
			const TArray<TSharedPtr<FJsonValue>>* Named = nullptr;
			if ((*Lost)->TryGetArrayField(TEXT("ships"), Named))
			{
				for (const TSharedPtr<FJsonValue>& One : *Named)
				{
					const TSharedPtr<FJsonObject>* Hers = nullptr;
					if (One->TryGetObject(Hers))
					{
						FLostHull& Hull = Sinking.Ships.AddDefaulted_GetRef();
						(*Hers)->TryGetStringField(TEXT("name"), Hull.Name); // (null where the record has run out: left empty)
						(*Hers)->TryGetStringField(TEXT("cls"), Hull.Class);
						(*Hers)->TryGetStringField(TEXT("type"), Hull.Type);
						(*Hers)->TryGetNumberField(TEXT("crew"), Hull.Crew);
						(*Hers)->TryGetNumberField(TEXT("lost"), Hull.Lost);
						(*Hers)->TryGetBoolField(TEXT("pair"), Hull.bPair);
					}
				}
			}
		}
	}
}

/** The formations of a raid, as the rules list them. */
static void ReadSorties(const TArray<TSharedPtr<FJsonValue>>& List, TArray<FSortie>& Out)
{
	for (const TSharedPtr<FJsonValue>& Value : List)
	{
		const TSharedPtr<FJsonObject> Fly = Value->AsObject();
		if (!Fly.IsValid())
		{
			continue;
		}
		FSortie& Sortie = Out.AddDefaulted_GetRef();
		Sortie.bValid = true;
		Fly->TryGetNumberField(TEXT("from"), Sortie.From);
		Fly->TryGetNumberField(TEXT("region"), Sortie.Region);
		Fly->TryGetStringField(TEXT("nat"), Sortie.Nation);
		Fly->TryGetBoolField(TEXT("bombers"), Sortie.bBombers);
		Fly->TryGetBoolField(TEXT("escort"), Sortie.bEscort);
		Fly->TryGetNumberField(TEXT("aa"), Sortie.Flak);
		{
			// (Release 2, the flak brief, 2c.) "light": 1 on a bomber flight over a target with no built guns.
			int32 LightGuns = 0;
			Fly->TryGetNumberField(TEXT("light"), LightGuns);
			Sortie.bLightFlak = LightGuns > 0;
		}
		Fly->TryGetStringField(TEXT("meet"), Sortie.Meet);
		Fly->TryGetNumberField(TEXT("meetFrom"), Sortie.MeetFrom);
		Fly->TryGetBoolField(TEXT("night"), Sortie.bNight);
		double Loss = 0.0, X = 0.0, Y = 0.0;
		Fly->TryGetNumberField(TEXT("loss"), Loss);
		Sortie.Loss = static_cast<float>(Loss);
		Fly->TryGetNumberField(TEXT("x"), X);
		Fly->TryGetNumberField(TEXT("y"), Y);
		Sortie.To = FVector2D(X, Y);
		double FromX = 0.0, FromY = 0.0;
		if (Fly->TryGetNumberField(TEXT("fromX"), FromX) && Fly->TryGetNumberField(TEXT("fromY"), FromY))
		{
			Sortie.bFromSea = true;
			Sortie.FromPoint = FVector2D(FromX, FromY);
			Fly->TryGetNumberField(TEXT("fleet"), Sortie.FromFleet);
		}
	}
}

FFleetHand FCampaignSession::FleetInHand(int32 FleetId)
{
	FFleetHand Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("fleetInHand"), FString::Printf(TEXT("[%d]"), FleetId), Object) || !Object.IsValid() || !Object->HasField(TEXT("ok")))
	{
		return Out;
	}
	Out.bValid = true;
	Object->TryGetBoolField(TEXT("grand"), Out.bGrand);
	Object->TryGetStringField(TEXT("doing"), Out.Doing);
	Object->TryGetStringField(TEXT("admiral"), Out.Admiral);
	Object->TryGetStringField(TEXT("screen"), Out.Screen);
	Object->TryGetBoolField(TEXT("shelled"), Out.bShelled);
	Object->TryGetNumberField(TEXT("guns"), Out.Guns);
	// (Release 2, fleetattack.ts) The enemy fleets it can be dropped on to attack now, and why it stays put once it has.
	Object->TryGetBoolField(TEXT("fleetAttack"), Out.bFleetAttack);
	Object->TryGetStringField(TEXT("held"), Out.Held);
	const TArray<TSharedPtr<FJsonValue>>* Attack = nullptr;
	if (Object->TryGetArrayField(TEXT("attack"), Attack))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Attack)
		{
			Out.Attack.Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	double Ready = 1.0;
	Object->TryGetNumberField(TEXT("ready"), Ready);
	Out.Ready = static_cast<float>(Ready);
	const TArray<TSharedPtr<FJsonValue>>* Cards = nullptr;
	if (Object->TryGetArrayField(TEXT("cards"), Cards))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Cards)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			if (!Entry.IsValid())
			{
				continue;
			}
			FFleetShipClass& Class = Out.Classes.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("cls"), Class.Class);
			Entry->TryGetStringField(TEXT("label"), Class.Label);
			Entry->TryGetStringField(TEXT("role"), Class.Role);
			Entry->TryGetStringField(TEXT("brief"), Class.Brief);
			Entry->TryGetNumberField(TEXT("count"), Class.Count);
			Entry->TryGetNumberField(TEXT("lost"), Class.Lost);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Seas = nullptr;
	if (Object->TryGetArrayField(TEXT("seas"), Seas))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Seas)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			if (!Entry.IsValid())
			{
				continue;
			}
			FFleetSea& Sea = Out.Seas.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("zone"), Sea.Zone);
			Entry->TryGetNumberField(TEXT("surface"), Sea.Surface);
			Entry->TryGetNumberField(TEXT("subs"), Sea.Subs);
			Entry->TryGetStringField(TEXT("meet"), Sea.Meet);
			double Hunt = 0.0, Torpedo = 0.0;
			Entry->TryGetNumberField(TEXT("hunt"), Hunt);
			Entry->TryGetNumberField(TEXT("torpedo"), Torpedo);
			Entry->TryGetNumberField(TEXT("reported"), Sea.Reported);
			Entry->TryGetStringField(TEXT("search"), Sea.Search);
			Sea.Hunt = static_cast<float>(Hunt);
			Sea.Torpedo = static_cast<float>(Torpedo);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Coasts = nullptr;
	if (Object->TryGetArrayField(TEXT("coasts"), Coasts))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Coasts)
		{
			Out.Coasts.Add(FMath::RoundToInt32(Value->AsNumber()));
		}
	}
	return Out;
}

bool FCampaignSession::Act(const FString& Action, int32 RegionId, FString& OutNote, TArray<FSortie>* OutSorties, FString* OutOpen, FActExtras* OutExtras)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("act"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Action), RegionId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("note"), OutNote);
		if (OutOpen)
		{
			Object->TryGetStringField(TEXT("open"), *OutOpen);
		}
		if (OutExtras)
		{
			Object->TryGetNumberField(TEXT("shell"), OutExtras->Shell);
			Object->TryGetNumberField(TEXT("made"), OutExtras->Made);
			Object->TryGetNumberField(TEXT("joined"), OutExtras->Joined);
			// (Release 2, fleetattack.ts) A fleet sent at the enemy: the sea it was fought in, what came of it, and the ships that went down.
			Object->TryGetNumberField(TEXT("seaFight"), OutExtras->SeaFight);
			Object->TryGetStringField(TEXT("outcome"), OutExtras->Outcome);
			const TArray<TSharedPtr<FJsonValue>>* Sunk = nullptr;
			if (Object->TryGetArrayField(TEXT("sunk"), Sunk))
			{
				ReadSinkings(*Sunk, OutExtras->Sinkings);
			}
			if (OutExtras->SeaFight < 0) { OutExtras->SeaFight = INDEX_NONE; }
			if (OutExtras->Shell < 0) { OutExtras->Shell = INDEX_NONE; }
			if (OutExtras->Made < 0) { OutExtras->Made = INDEX_NONE; }
			if (OutExtras->Joined < 0) { OutExtras->Joined = INDEX_NONE; }
		}
		const TArray<TSharedPtr<FJsonValue>>* Flies = nullptr;
		if (OutSorties && Object->TryGetArrayField(TEXT("flies"), Flies))
		{
			ReadSorties(*Flies, *OutSorties);
		}
		TakeView(Object);
	}
	return bOk;
}

FGeneralStaff FCampaignSession::Generals()
{
	FGeneralStaff Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("generals"), TEXT("[]"), Object) || !Object.IsValid())
	{
		return Out;
	}
	Out.bValid = Object->TryGetStringField(TEXT("nation"), Out.Nation);
	Object->TryGetStringField(TEXT("date"), Out.Date);
	Object->TryGetStringField(TEXT("note"), Out.Note);
	Object->TryGetBoolField(TEXT("up"), Out.bUp);
	Object->TryGetNumberField(TEXT("capital"), Out.Capital);
	Object->TryGetNumberField(TEXT("promoteCost"), Out.PromoteCost);
	Object->TryGetNumberField(TEXT("commanding"), Out.Commanding);
	Object->TryGetNumberField(TEXT("waiting"), Out.Waiting);
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Object->TryGetArrayField(TEXT("generals"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FGeneralEntry& General = Out.Generals.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("id"), General.Id);
			Entry->TryGetStringField(TEXT("name"), General.Name);
			Entry->TryGetStringField(TEXT("rank"), General.Rank);
			Entry->TryGetStringField(TEXT("where"), General.Where);
			Entry->TryGetStringField(TEXT("record"), General.Record);
			Entry->TryGetNumberField(TEXT("level"), General.Level);
			Entry->TryGetNumberField(TEXT("points"), General.Points);
			double Progress = 0.0;
			Entry->TryGetNumberField(TEXT("progress"), Progress);
			General.Progress = static_cast<float>(Progress);
			Entry->TryGetBoolField(TEXT("commands"), General.bCommands);
			Entry->TryGetBoolField(TEXT("waiting"), General.bWaiting);
			Entry->TryGetBoolField(TEXT("relieve"), General.bRelieve);
			Entry->TryGetBoolField(TEXT("sack"), General.bSack);
			Entry->TryGetBoolField(TEXT("popular"), General.bPopular);
			const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
			if (Entry->TryGetArrayField(TEXT("skills"), Skills))
			{
				for (const TSharedPtr<FJsonValue>& SkillValue : *Skills)
				{
					const TSharedPtr<FJsonObject> SkillEntry = SkillValue->AsObject();
					FGeneralSkill& Skill = General.Skills.AddDefaulted_GetRef();
					SkillEntry->TryGetStringField(TEXT("id"), Skill.Id);
					SkillEntry->TryGetStringField(TEXT("label"), Skill.Label);
					SkillEntry->TryGetStringField(TEXT("blurb"), Skill.Blurb);
					SkillEntry->TryGetNumberField(TEXT("value"), Skill.Value);
					SkillEntry->TryGetNumberField(TEXT("max"), Skill.Max);
					SkillEntry->TryGetBoolField(TEXT("more"), Skill.bMore);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Traits = nullptr;
			if (Entry->TryGetArrayField(TEXT("traits"), Traits))
			{
				for (const TSharedPtr<FJsonValue>& TraitValue : *Traits)
				{
					const TSharedPtr<FJsonObject> TraitEntry = TraitValue->AsObject();
					FString Label, Effect;
					bool bGood = true;
					TraitEntry->TryGetStringField(TEXT("label"), Label);
					TraitEntry->TryGetStringField(TEXT("effect"), Effect);
					TraitEntry->TryGetBoolField(TEXT("good"), bGood);
					General.Traits.Add(Effect.IsEmpty() ? Label : Label + TEXT(": ") + Effect);
					General.TraitGood.Add(bGood);
				}
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Vacant = nullptr;
	if (Object->TryGetArrayField(TEXT("vacant"), Vacant))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Vacant)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FVacantCorps& Corps = Out.Vacant.AddDefaulted_GetRef();
			Entry->TryGetNumberField(TEXT("corps"), Corps.Corps);
			Entry->TryGetStringField(TEXT("name"), Corps.Name);
			Entry->TryGetStringField(TEXT("line"), Corps.Line);
			Entry->TryGetBoolField(TEXT("promote"), Corps.bPromote);
			Entry->TryGetStringField(TEXT("whyPromote"), Corps.WhyPromote);
			const TSharedPtr<FJsonObject>* Best = nullptr;
			if (Entry->TryGetObjectField(TEXT("best"), Best))
			{
				FString Rank, Name;
				int32 Level = 0;
				(*Best)->TryGetNumberField(TEXT("id"), Corps.BestId);
				(*Best)->TryGetStringField(TEXT("rank"), Rank);
				(*Best)->TryGetStringField(TEXT("name"), Name);
				(*Best)->TryGetNumberField(TEXT("level"), Level);
				Corps.BestName = FString::Printf(TEXT("%s, level %d"), *Name, Level);
			}
		}
	}
	return Out;
}

bool FCampaignSession::AssignGeneral(int32 GeneralId, int32 CorpsId)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("assign"), FString::Printf(TEXT("[%d,%d]"), GeneralId, CorpsId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::PromoteOfficer(int32 CorpsId, FString& OutName)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("promote"), FString::Printf(TEXT("[%d]"), CorpsId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("name"), OutName);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::SackGeneral(int32 GeneralId, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("sack"), FString::Printf(TEXT("[%d]"), GeneralId), Object) && Object.IsValid())
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("note"), OutNote);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::SetDrafts(const TArray<int32>& CorpsIds, bool bTake, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("drafts"), FString::Printf(TEXT("[[%s],%s]"), *IdList(CorpsIds), bTake ? TEXT("true") : TEXT("false")), Object) && Object.IsValid())
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("note"), OutNote);
		TakeView(Object);
	}
	return bOk;
}

// ---- (Release 2: the battle schools.) TC.school([ids], go): release 1's rules have no such call, and its button never shows there.
bool FCampaignSession::SendToSchool(const TArray<int32>& CorpsIds, bool bGo, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CorpsIds.Num() > 0 && CallRules(TEXT("school"), FString::Printf(TEXT("[[%s],%s]"), *IdList(CorpsIds), bGo ? TEXT("true") : TEXT("false")), Object) && Object.IsValid())
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("note"), OutNote);
		TakeView(Object);
	}
	return bOk;
}
// ---- (end of the battle schools)

bool FCampaignSession::PlaceByPlan(FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("evePlanGo"), TEXT("[]"), Object) && Object.IsValid())
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("note"), OutNote);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::RelieveGeneral(int32 GeneralId)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("relieve"), FString::Printf(TEXT("[%d]"), GeneralId), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::SpendSkill(int32 GeneralId, const FString& Skill)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("skill"), FString::Printf(TEXT("[%d,%s]"), GeneralId, *JsonQuote(Skill)), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::StartTech(const FString& Id, FString& OutWhy)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("techStart"), FString::Printf(TEXT("[%s]"), *JsonQuote(Id)), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		Object->TryGetStringField(TEXT("why"), OutWhy);
		TakeView(Object);
	}
	return bOk;
}

bool FCampaignSession::StopTech(const FString& Id)
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	if (CallRules(TEXT("techStop"), FString::Printf(TEXT("[%s]"), *JsonQuote(Id)), Object))
	{
		Object->TryGetBoolField(TEXT("ok"), bOk);
		TakeView(Object);
	}
	return bOk;
}

FResearch FCampaignSession::Research()
{
	FResearch Out;
	TSharedPtr<FJsonObject> Object;
	// The grand campaign has a technology tree; a historical scenario keeps history's clock and its design teams.
	bool bTree = false;
	if (CallRules(TEXT("tech"), TEXT("[]"), Object) && Object.IsValid() && Object->TryGetBoolField(TEXT("ok"), bTree) && bTree)
	{
		Out.bValid = Out.bTree = Out.bOwn = true;
		Object->TryGetStringField(TEXT("nation"), Out.Nation);
		Object->TryGetStringField(TEXT("date"), Out.Date);
		Object->TryGetBoolField(TEXT("up"), Out.bUp);
		Object->TryGetNumberField(TEXT("teams"), Out.Teams);
		Object->TryGetNumberField(TEXT("busy"), Out.Busy);
		Object->TryGetNumberField(TEXT("idle"), Out.Free);
		Object->TryGetNumberField(TEXT("done"), Out.Done);
		Object->TryGetNumberField(TEXT("total"), Out.Total);
		Object->TryGetStringArrayField(TEXT("eras"), Out.Eras);
		Out.Note = TEXT("Each research team works on one technology at a time, and a team with nothing to do stays idle until you give it work. A technology begun before its time takes longer. Rest the mouse on one to read what it is.");
		const TArray<TSharedPtr<FJsonValue>>* Branches = nullptr;
		if (Object->TryGetArrayField(TEXT("branches"), Branches))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Branches)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FTechBranch& Branch = Out.Branches.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("id"), Branch.Id);
				Entry->TryGetStringField(TEXT("label"), Branch.Label);
				Entry->TryGetStringField(TEXT("text"), Branch.Text);
				Entry->TryGetNumberField(TEXT("done"), Branch.Done);
				Entry->TryGetNumberField(TEXT("total"), Branch.Total);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if (Object->TryGetArrayField(TEXT("nodes"), Nodes))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Nodes)
			{
				const TSharedPtr<FJsonObject> Entry = Value->AsObject();
				FTechNode& Node = Out.Nodes.AddDefaulted_GetRef();
				Entry->TryGetStringField(TEXT("id"), Node.Id);
				Entry->TryGetStringField(TEXT("branch"), Node.Branch);
				Entry->TryGetNumberField(TEXT("tier"), Node.Tier);
				Entry->TryGetStringField(TEXT("name"), Node.Name);
				Entry->TryGetStringField(TEXT("text"), Node.Text);
				Entry->TryGetStringField(TEXT("rule"), Node.Rule);
				Entry->TryGetStringField(TEXT("state"), Node.State);
				Entry->TryGetStringField(TEXT("why"), Node.Why);
				Entry->TryGetNumberField(TEXT("months"), Node.Months);
				Entry->TryGetNumberField(TEXT("left"), Node.Left);
				Entry->TryGetNumberField(TEXT("of"), Node.Of);
				Entry->TryGetBoolField(TEXT("can"), Node.bCan);
				Entry->TryGetBoolField(TEXT("stop"), Node.bStop);
				TArray<FString> Needs;
				Entry->TryGetStringArrayField(TEXT("needs"), Needs);
				Node.Needs = FString::Join(Needs, TEXT(", "));
				Entry->TryGetNumberField(TEXT("year"), Node.Year);
				Entry->TryGetNumberField(TEXT("base"), Node.Base);
				Entry->TryGetStringField(TEXT("tag"), Node.Tag);
				const TArray<TSharedPtr<FJsonValue>>* All = nullptr;
				if (Entry->TryGetArrayField(TEXT("all"), All))
				{
					for (const TSharedPtr<FJsonValue>& Each : *All)
					{
						const TSharedPtr<FJsonObject> Need = Each->AsObject();
						if (Need.IsValid())
						{
							bool bDone = false;
							Need->TryGetBoolField(TEXT("done"), bDone);
							Node.NeedIds.Add(Need->GetStringField(TEXT("id")));
							Node.NeedNames.Add(Need->GetStringField(TEXT("name")));
							Node.NeedDone.Add(bDone);
						}
					}
				}
			}
		}
		return Out;
	}
	Object.Reset();
	if (!CallRules(TEXT("research"), TEXT("[]"), Object) || !Object.IsValid())
	{
		return Out;
	}
	Out.bValid = Object->TryGetStringField(TEXT("nation"), Out.Nation);
	Object->TryGetStringField(TEXT("date"), Out.Date);
	Object->TryGetBoolField(TEXT("up"), Out.bUp);
	Object->TryGetBoolField(TEXT("own"), Out.bOwn);
	Object->TryGetStringField(TEXT("doctrine"), Out.Doctrine);
	Object->TryGetStringField(TEXT("note"), Out.Note);
	Object->TryGetNumberField(TEXT("teams"), Out.Teams);
	Object->TryGetNumberField(TEXT("free"), Out.Free);
	Object->TryGetNumberField(TEXT("money"), Out.Money);
	const TSharedPtr<FJsonObject>* Bureau = nullptr;
	if (Object->TryGetObjectField(TEXT("bureau"), Bureau))
	{
		Out.bBureau = true;
		(*Bureau)->TryGetNumberField(TEXT("cost"), Out.BureauCost);
		(*Bureau)->TryGetBoolField(TEXT("can"), Out.bBureauCan);
		(*Bureau)->TryGetStringField(TEXT("why"), Out.BureauWhy);
	}
	const TArray<TSharedPtr<FJsonValue>>* Fields = nullptr;
	if (Object->TryGetArrayField(TEXT("fields"), Fields))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Fields)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FResearchField& Field = Out.Fields.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("id"), Field.Id);
			Entry->TryGetStringField(TEXT("label"), Field.Label);
			Entry->TryGetStringField(TEXT("pace"), Field.Pace);
			Entry->TryGetStringField(TEXT("now"), Field.Now);
			Entry->TryGetStringField(TEXT("next"), Field.Next);
			Entry->TryGetStringField(TEXT("effect"), Field.Effect);
			Entry->TryGetStringField(TEXT("work"), Field.Work);
			Entry->TryGetNumberField(TEXT("ahead"), Field.Ahead);
			Entry->TryGetNumberField(TEXT("teams"), Field.Teams);
			Entry->TryGetNumberField(TEXT("max"), Field.Max);
			Entry->TryGetBoolField(TEXT("good"), Field.bGood);
			Entry->TryGetBoolField(TEXT("more"), Field.bMore);
			Entry->TryGetBoolField(TEXT("fewer"), Field.bFewer);
			double Fill = 0.0, Today = 0.0;
			Entry->TryGetNumberField(TEXT("fill"), Fill);
			Entry->TryGetNumberField(TEXT("today"), Today);
			Field.Fill = static_cast<float>(Fill);
			Field.Today = static_cast<float>(Today);
			const TArray<TSharedPtr<FJsonValue>>* Marks = nullptr;
			if (Entry->TryGetArrayField(TEXT("marks"), Marks))
			{
				for (const TSharedPtr<FJsonValue>& MarkValue : *Marks)
				{
					const TSharedPtr<FJsonObject> MarkEntry = MarkValue->AsObject();
					FResearchMark& Mark = Field.Marks.AddDefaulted_GetRef();
					double At = 0.0;
					MarkEntry->TryGetNumberField(TEXT("at"), At);
					Mark.At = static_cast<float>(At);
					MarkEntry->TryGetBoolField(TEXT("got"), Mark.bGot);
					MarkEntry->TryGetStringField(TEXT("name"), Mark.Name);
				}
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Others = nullptr;
	if (Object->TryGetArrayField(TEXT("others"), Others))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Others)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			FResearchPower& Power = Out.Others.AddDefaulted_GetRef();
			Entry->TryGetStringField(TEXT("nat"), Power.Tag);
			Entry->TryGetStringField(TEXT("name"), Power.Name);
			Entry->TryGetStringField(TEXT("doctrine"), Power.Doctrine);
			const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
			if (Entry->TryGetArrayField(TEXT("lines"), Lines))
			{
				for (const TSharedPtr<FJsonValue>& LineValue : *Lines)
				{
					const TSharedPtr<FJsonObject> Line = LineValue->AsObject();
					FString Label, Design;
					Line->TryGetStringField(TEXT("label"), Label);
					Line->TryGetStringField(TEXT("name"), Design);
					Power.Labels.Add(Label);
					Power.Designs.Add(Design);
				}
			}
		}
	}
	return Out;
}

bool FCampaignSession::SetTeams(const FString& Field, int32 Teams)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("teams"), FString::Printf(TEXT("[%s,%d]"), *JsonQuote(Field), Teams), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::FoundBureau()
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("bureau"), TEXT("[]"), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::TakePath(const FString& Id, const FString& Partner, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("path"), FString::Printf(TEXT("[%s,%s]"), *JsonQuote(Id), *JsonQuote(Partner)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::InviteToLeague(const FString& Nation, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("invite"), FString::Printf(TEXT("[%s]"), *JsonQuote(Nation)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::Court(const FString& Nation, FString& OutNote)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("court"), FString::Printf(TEXT("[%s]"), *JsonQuote(Nation)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	Object->TryGetStringField(TEXT("note"), OutNote);
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::Declare(const FString& Nation, FString* Said)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("declare"), FString::Printf(TEXT("[%s]"), *JsonQuote(Nation)), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	if (Said)
	{
		Object->TryGetStringField(TEXT("note"), *Said); // (rules 84.2.5) what the declaration brought at once: Britain's answer
	}
	TakeView(Object);
	return bOk;
}

bool FCampaignSession::CancelOrder(int32 OrderId)
{
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("cancel"), FString::Printf(TEXT("[%d]"), OrderId), Object))
	{
		return false;
	}
	bool bOk = false;
	Object->TryGetBoolField(TEXT("ok"), bOk);
	TakeView(Object);
	return bOk;
}

// (Speed, the design director's mark 1 for build 414: a press's own work under a tenth of a second. See MoveAside.)

struct FCampaignSession::FAside
{
	FString Function, Arguments;
	/** Run on the rules' thread: what the army in hand will ask after the order. */
	TFunction<FHandNext(const TSharedPtr<FJsonObject>&)> Guess;
	/** Run on the game thread, with the answer. */
	TFunction<void(const TSharedPtr<FJsonObject>&)> Then;

	// What came back, written on the rules' thread and read on the game thread once bDone is set.
	bool bOk = false;
	FString Error;
	double Seconds = 0.0;
	TSharedPtr<FJsonObject> Answer;
	bool bView = false;
	FCampaignView View;
	struct FKept
	{
		FString Function, Arguments, Json;
		double Seconds = 0.0;
	};
	TArray<FKept> Kept;
	std::atomic<bool> bDone{ false };
};

/** (D1) The month's scene asked for beside the war: see SubSceneAside. */
struct FCampaignSession::FSceneAsk
{
	FString Arguments;
	/** Run on the game thread (Deliver), with the answer. */
	TFunction<void(int32 NextMark, const FString& Recording)> Then;
	// What came back, written on the rules' thread and read on the game thread once bDone is set.
	bool bOk = false;
	FString Error, Recording;
	int32 Next = -1;
	double Seconds = 0.0;
	std::atomic<bool> bDone{ false };
};

void FCampaignSession::Order(const FString& Function, const FString& ArgumentsJson, bool bAside, TFunction<FHandNext(const TSharedPtr<FJsonObject>&)>&& InGuess, TFunction<void(const TSharedPtr<FJsonObject>&)>&& InThen)
{
	// (one order at a time: the controller takes no order while one is being answered, and an earlier one is taken first)
	Deliver(true);
	if (!bAside || CVarRulesAside.GetValueOnGameThread() == 0 || (!bRunning && !IsBeforeAnyWar(Function)) || !Host.IsValid())
	{
		// At once, as before: the order, its picture of the war, then its Then.
		TSharedPtr<FJsonObject> Object;
		if (!CallRules(Function, ArgumentsJson, Object))
		{
			InThen(nullptr);
			return;
		}
		if (InGuess)
		{
			InGuess(Object); // (it reads the answer for Then; nothing is asked ahead)
		}
		TakeView(Object);
		InThen(Object);
		return;
	}
	if (!IsQuestion(Function) && !IsWrittenOut(Function))
	{
		RulesMemo.Reset(); // (an order: nothing asked before it holds; a question asked aside, PreviewAside's, changes nothing, nor does the war written out)
	}
	const TSharedRef<FAside, ESPMode::ThreadSafe> Job = MakeShared<FAside, ESPMode::ThreadSafe>();
	Job->Function = Function;
	Job->Arguments = ArgumentsJson;
	Job->Guess = MoveTemp(InGuess);
	Job->Then = MoveTemp(InThen);
	Aside = Job;
	FRulesHost* Rules = Host.Get();
	// (A thread of its own, not the pool, where the world's fine sheets are modelled: the answer never waits behind them.
	// Its stack is as deep as a program's first thread, for the rules run inside the game with -TheaterRulesInside.)
	AsideRun = AsyncThread([Rules, Job]()
	{
		const double Began = FPlatformTime::Seconds();
		FString Json;
		Job->bOk = Rules->Call(Job->Function, Job->Arguments, Json, Job->Error);
		Job->Seconds = FPlatformTime::Seconds() - Began;
		// (the rules' null, a question with nobody there to ask about: no answer, as CallRules gives it, and no fault)
		const bool bNull = Job->bOk && Json.Len() <= 8 && Json.TrimStartAndEnd() == TEXT("null");
		if (Job->bOk && !bNull && !(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Job->Answer) && Job->Answer.IsValid()))
		{
			Job->bOk = false;
			Job->Error = Job->Function + TEXT(": the answer could not be read");
		}
		if (Job->bOk && Job->Answer.IsValid())
		{
			const TSharedPtr<FJsonObject>* NewView = nullptr;
			if (Job->Answer->TryGetObjectField(TEXT("view"), NewView))
			{
				Job->View = ReadView(*NewView);
				Job->bView = true;
			}
			const FHandNext Next = Job->Guess ? Job->Guess(Job->Answer) : FHandNext();
			auto Ask = [Rules, &Job](const FString& Question, const FString& Asked) -> FString
			{
				const double At = FPlatformTime::Seconds();
				FString Said, Wrong;
				if (!Rules->Call(Question, Asked, Said, Wrong))
				{
					return FString(); // (the hand asks it again itself, and says what went wrong)
				}
				FAside::FKept& Kept = Job->Kept.AddDefaulted_GetRef();
				Kept.Function = Question;
				Kept.Arguments = Asked;
				Kept.Json = Said;
				Kept.Seconds = FPlatformTime::Seconds() - At;
				return Said;
			};
			if (Job->bView && Next.Region != INDEX_NONE)
			{
				// The army in hand as the controller's RefreshOrders will make it from this picture: its corps there that
				// can be ordered, its general, and its corps in the field (the town's garrison where nobody stands outside).
				const FCampaignView& Seen = Job->View;
				bool bAny = false;
				FString General;
				TArray<int32> Field, InTown;
				for (const FViewCorps& Unit : Seen.Corps)
				{
					if (Unit.Region != Next.Region || Unit.Nation != Seen.Human)
					{
						continue;
					}
					bAny = true;
					if (General.IsEmpty() && !Unit.General.IsEmpty())
					{
						General = Unit.General;
					}
					if (Unit.bCanOrder)
					{
						(Unit.bInTown ? InTown : Field).Add(Unit.Id);
					}
				}
				if (bAny)
				{
					TArray<int32> Ids;
					for (const int32 Id : Next.Ids)
					{
						if (Field.Contains(Id) || InTown.Contains(Id))
						{
							Ids.Add(Id);
						}
					}
					if (Field.Num() == 0)
					{
						Field = InTown;
					}
					const FString Army = General.IsEmpty() ? FString::Printf(TEXT("the %s army"), *Next.Adjective) : FString::Printf(TEXT("%s's army"), *General);
					Ask(TEXT("stands"), RegionArgs(Next.Region));
					if (Next.bPlain && Ids.Num() > 0)
					{
						const FString ReachJson = Ask(TEXT("reach"), ReachArgs(Ids));
						TSharedPtr<FJsonObject> Read;
						if (!ReachJson.IsEmpty() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ReachJson), Read) && Read.IsValid())
						{
							// (the ground it walks to, as RefreshOrders hands it to the railway's question)
							TArray<int32> Walked;
							const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
							if (Read->TryGetArrayField(TEXT("regions"), Regions))
							{
								for (const TSharedPtr<FJsonValue>& Value : *Regions)
								{
									const TSharedPtr<FJsonObject> Entry = Value->AsObject();
									bool bAttack = false;
									FString War;
									if (!Entry.IsValid())
									{
										continue;
									}
									Entry->TryGetBoolField(TEXT("attack"), bAttack);
									Entry->TryGetStringField(TEXT("war"), War);
									if (!bAttack && War.IsEmpty())
									{
										Walked.Add(static_cast<int32>(Entry->GetIntegerField(TEXT("id"))));
									}
								}
							}
							Ask(TEXT("railShow"), RailShowArgs(Ids, Walked, Army, Field));
						}
					}
				}
			}
			if (Next.Selected != INDEX_NONE)
			{
				Ask(TEXT("province"), RegionArgs(Next.Selected));
			}
		}
		Job->bDone.store(true, std::memory_order_release);
	}, 8 * 1024 * 1024);
}

void FCampaignSession::Settle()
{
	WaitSceneAsk(); // (the month's scene being asked for comes first: the rules answer one thing at a time)
	if (!Aside.IsValid())
	{
		return;
	}
	if (!Aside->bDone.load(std::memory_order_acquire))
	{
		THEATER_SPEED_SCOPE(RulesWait); // (something asked the rules while the order was still being answered)
		AsideRun.Wait();
	}
	const TSharedPtr<FAside, ESPMode::ThreadSafe> Job = MoveTemp(Aside);
	Aside.Reset();
	AsideRun = TFuture<void>();
	TheaterSpeed::NoteRulesCall(Job->Function, Job->Seconds, false);
	if (!Job->bOk)
	{
		UE_LOG(LogTheater, Error, TEXT("Rules: %s"), *Job->Error);
	}
	if (Job->bView)
	{
		View = MoveTemp(Job->View);
		OnViewChanged.Broadcast();
	}
	if (!IsQuestion(Job->Function) && !IsWrittenOut(Job->Function))
	{
		RulesMemo.Reset(); // (the order changed the war; what was asked ahead after it holds)
	}
	for (FAside::FKept& Kept : Job->Kept)
	{
		TheaterSpeed::NoteRulesCall(Kept.Function, Kept.Seconds, false);
		if (TheaterSpeed::RulesMemo() && IsQuestion(Kept.Function))
		{
			RulesMemo.Add(Kept.Function + TEXT("|") + Kept.Arguments, MoveTemp(Kept.Json));
		}
	}
	const TSharedPtr<FJsonObject> Answer = Job->bOk ? Job->Answer : TSharedPtr<FJsonObject>();
	AsideThen = [Then = MoveTemp(Job->Then), Answer]()
	{
		if (Then)
		{
			Then(Answer);
		}
	};
}

void FCampaignSession::Deliver(bool bWait)
{
	if (Aside.IsValid() && (bWait || Aside->bDone.load(std::memory_order_acquire)))
	{
		THEATER_SPEED_SCOPE(RulesAnswer);
		Settle();
	}
	if (AsideThen)
	{
		THEATER_SPEED_SCOPE(RulesAnswerThen);
		const TFunction<void()> Run = MoveTemp(AsideThen);
		AsideThen = nullptr;
		Run();
	}
	// (D1) The month's scene asked for, once it is in.
	if (SceneAsk.IsValid() && (bWait || SceneAsk->bDone.load(std::memory_order_acquire)))
	{
		WaitSceneAsk();
		const TSharedPtr<FSceneAsk, ESPMode::ThreadSafe> Job = MoveTemp(SceneAsk);
		SceneAsk.Reset();
		SceneAskRun = TFuture<void>();
		TheaterSpeed::NoteRulesCall(TEXT("subScene"), Job->Seconds, false);
		if (!Job->bOk)
		{
			UE_LOG(LogTheater, Warning, TEXT("Sea scene: the month's scene could not be asked for (%s)"), *Job->Error);
		}
		if (Job->Then)
		{
			Job->Then(Job->Next, Job->Recording);
		}
	}
}

void FCampaignSession::DropAside()
{
	Settle();
	AsideThen = nullptr;
	SceneAsk.Reset(); // (its Then unrun: the war it was asked of is going)
	SceneAskRun = TFuture<void>();
}

void FCampaignSession::WaitSceneAsk()
{
	if (SceneAsk.IsValid() && !SceneAsk->bDone.load(std::memory_order_acquire))
	{
		THEATER_SPEED_SCOPE(RulesWait);
		SceneAskRun.Wait();
	}
}

// ---- (D1, ruling 241) The month's submarine scene ---------------------------------------------------------------

FString FCampaignSession::SubSceneFile()
{
	// (a run that takes pictures or plays the playtester's steps keeps its own, as it keeps its own notebook)
	FString Run;
	const bool bRun = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Run, false) || FParse::Value(FCommandLine::Get(), TEXT("TheaterSteps="), Run, false);
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Campaign"), bRun ? TEXT("subscene-runs.json") : TEXT("subscene.json"));
}

FString FCampaignSession::GetSubScene() const
{
	if (!SubScene.IsEmpty())
	{
		return SubScene;
	}
	auto Known = [](const FString& Word) { return Word == TEXT("all") || Word == TEXT("mine") || Word == TEXT("never") || Word == TEXT("off"); };
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSubScene="), Asked) && Known(Asked.ToLower()))
	{
		SubScene = Asked.ToLower();
		return SubScene;
	}
	// (a run that is not about the scene leaves it off, so that its saves and steps are as they were)
	FString Run;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Run, false) || FParse::Value(FCommandLine::Get(), TEXT("TheaterSteps="), Run, false))
	{
		SubScene = TEXT("off");
		return SubScene;
	}
	FString Text, Kept;
	TSharedPtr<FJsonObject> Object;
	SubScene = FFileHelper::LoadFileToString(Text, *SubSceneFile()) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) && Object.IsValid()
		&& Object->TryGetStringField(TEXT("view"), Kept) && Known(Kept) ? Kept : FString(TEXT("all"));
	return SubScene;
}

void FCampaignSession::SetSubScene(const FString& Setting)
{
	const FString Word = Setting.ToLower();
	if (Word != TEXT("all") && Word != TEXT("mine") && Word != TEXT("never") && Word != TEXT("off"))
	{
		return;
	}
	SubScene = Word;
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"view\":\"%s\"}\n"), *Word), *SubSceneFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	Settle(); // (an order still being answered is answered first: the rules are told on this thread)
	TellSubScene();
}

void FCampaignSession::TellSubScene()
{
	if (!Host.IsValid())
	{
		return;
	}
	// (the record is kept for every setting but "off": "never" plays nothing, but a change back finds this month's record)
	FString Json, Error;
	bNoSubScene = !Host->Call(TEXT("subSceneOn"), GetSubScene() == TEXT("off") ? TEXT("[false]") : TEXT("[true]"), Json, Error);
	if (bNoSubScene)
	{
		UE_LOG(LogTheater, Display, TEXT("Sea scene: these rules keep no record of the month at sea (%s)"), *Error); // (the stand-in rules, or rules before D1)
	}
}

void FCampaignSession::SubSceneAside(int32 Mark, TFunction<void(int32 NextMark, const FString& Recording)>&& Then)
{
	const FString Setting = GetSubScene();
	if (Setting == TEXT("off") || Setting == TEXT("never") || !bRunning || bNoSubScene)
	{
		Then(Mark, FString());
		return;
	}
	// (an order still being answered, or a scene asked before and not yet taken, first: the rules answer one thing at a
	// time, and each Then in its turn)
	Deliver(true);
	if (!bRunning || !Host.IsValid())
	{
		Then(Mark, FString());
		return;
	}
	const TSharedRef<FSceneAsk, ESPMode::ThreadSafe> Job = MakeShared<FSceneAsk, ESPMode::ThreadSafe>();
	Job->Arguments = FString::Printf(TEXT("[%s,%s]"), Mark < 0 ? TEXT("null") : *FString::FromInt(Mark), *JsonQuote(Setting));
	Job->Next = Mark;
	Job->Then = MoveTemp(Then);
	SceneAsk = Job;
	FRulesHost* Rules = Host.Get();
	// (a thread of its own, as an order's: the answer is read there too, a recording of a few hundred kilobytes)
	SceneAskRun = AsyncThread([Rules, Job]()
	{
		const double Began = FPlatformTime::Seconds();
		FString Json;
		Job->bOk = Rules->Call(TEXT("subScene"), Job->Arguments, Json, Job->Error);
		TSharedPtr<FJsonObject> Answer;
		if (Job->bOk && !(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Answer) && Answer.IsValid()))
		{
			Job->bOk = false;
			Job->Error = TEXT("subScene: the answer could not be read");
		}
		if (Job->bOk)
		{
			double Number = 0.0;
			if (Answer->TryGetNumberField(TEXT("mark"), Number) && Number >= 0.0)
			{
				Job->Next = static_cast<int32>(Number);
			}
			Answer->TryGetStringField(TEXT("recording"), Job->Recording); // (null where the month holds no scene for him)
		}
		Job->Seconds = FPlatformTime::Seconds() - Began;
		Job->bDone.store(true, std::memory_order_release);
	}, 8 * 1024 * 1024);
}

void FCampaignSession::MoveAside(const TArray<int32>& CorpsIds, int32 RegionId, const FString& Army, const TArray<int32>& ArmyIds, bool bAside,
	TFunction<FHandNext(const FMoveAnswer&)>&& InGuess, TFunction<void(const FMoveAnswer&)>&& InThen)
{
	if (CorpsIds.Num() == 0)
	{
		InThen(FMoveAnswer());
		return;
	}
	// (read once, on whichever thread has the answer first; Then is run after, on the game thread)
	const TSharedRef<FMoveAnswer, ESPMode::ThreadSafe> Read = MakeShared<FMoveAnswer, ESPMode::ThreadSafe>();
	Order(TEXT("move"), FString::Printf(TEXT("[[%s],%d%s]"), *IdList(CorpsIds), RegionId, *ArmyArgs(Army, ArmyIds)), bAside,
		[Read, Guess = MoveTemp(InGuess)](const TSharedPtr<FJsonObject>& Answer)
		{
			Read->Moved = ReadMoves(Answer);
			Answer->TryGetStringField(TEXT("note"), Read->Note);
			return Guess ? Guess(*Read) : FHandNext();
		},
		[Read, Then = MoveTemp(InThen)](const TSharedPtr<FJsonObject>& Answer)
		{
			Then(Answer.IsValid() ? *Read : FMoveAnswer());
		});
}

void FCampaignSession::LandAside(const TArray<int32>& CorpsIds, int32 RegionId, bool bAside, TFunction<FHandNext(const FLandingResult&)>&& InGuess, TFunction<void(const FLandingResult&)>&& InThen)
{
	if (CorpsIds.Num() == 0)
	{
		InThen(FLandingResult());
		return;
	}
	const TSharedRef<FLandingResult, ESPMode::ThreadSafe> Read = MakeShared<FLandingResult, ESPMode::ThreadSafe>();
	Order(TEXT("landOn"), FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId), bAside,
		[Read, Guess = MoveTemp(InGuess)](const TSharedPtr<FJsonObject>& Answer)
		{
			*Read = ReadLanding(Answer);
			return Guess ? Guess(*Read) : FHandNext();
		},
		[Read, Then = MoveTemp(InThen)](const TSharedPtr<FJsonObject>& Answer)
		{
			Then(Answer.IsValid() ? *Read : FLandingResult());
		});
}

void FCampaignSession::PreviewAside(const TArray<int32>& CorpsIds, int32 RegionId, bool bOpening, bool bAside, TFunction<void(const FAttackPreview&)>&& InThen)
{
	if (CorpsIds.Num() == 0)
	{
		InThen(FAttackPreview());
		return;
	}
	// (the same arguments as PreviewAttack sends, so that at once it is the same question, kept as that one is)
	Order(TEXT("previewAttack"), FString::Printf(TEXT("[[%s],%d,%s]"), *IdList(CorpsIds), RegionId, bOpening ? TEXT("true") : TEXT("false")), bAside, nullptr,
		[RegionId, Then = MoveTemp(InThen)](const TSharedPtr<FJsonObject>& Answer)
		{
			Then(Answer.IsValid() ? ReadPreview(Answer, RegionId) : FAttackPreview());
		});
}

void FCampaignSession::SaveAside(bool bAside, TFunction<void(FString)>&& InThen)
{
	if (!bAside || CVarRulesAside.GetValueOnGameThread() == 0 || !bRunning || !Host.IsValid())
	{
		InThen(Save()); // (at once, as before)
		return;
	}
	// (the war as JSON text, and the answer that carries it read, on the rules' thread: by August 1944 some 2.3 MB)
	Order(TEXT("save"), TEXT("[]"), true, nullptr,
		[Then = MoveTemp(InThen)](const TSharedPtr<FJsonObject>& Answer)
		{
			FString Written;
			if (Answer.IsValid())
			{
				Answer->TryGetStringField(TEXT("text"), Written);
			}
			Then(MoveTemp(Written));
		});
}

void FCampaignSession::FightAside(const FString& Function, const TArray<int32>& CorpsIds, int32 RegionId, bool bAssault, bool bAside,
	TFunction<FHandNext(const FAttackResult&)>&& InGuess, TFunction<void(const FAttackResult&)>&& InThen)
{
	// (the same arguments as Attack, Take, Storm and Sally send)
	const bool bByProvince = Function == TEXT("storm") || Function == TEXT("sally");
	if (!bByProvince && CorpsIds.Num() == 0)
	{
		InThen(FAttackResult());
		return;
	}
	const FString Arguments = bByProvince ? FString::Printf(TEXT("[%d]"), RegionId)
		: Function == TEXT("take") ? FString::Printf(TEXT("[[%s],%d,%s]"), *IdList(CorpsIds), RegionId, bAssault ? TEXT("true") : TEXT("false"))
		: FString::Printf(TEXT("[[%s],%d]"), *IdList(CorpsIds), RegionId);
	const TSharedRef<FAttackResult, ESPMode::ThreadSafe> Read = MakeShared<FAttackResult, ESPMode::ThreadSafe>();
	Order(Function, Arguments, bAside,
		[Read, Guess = MoveTemp(InGuess)](const TSharedPtr<FJsonObject>& Answer)
		{
			*Read = ReadFight(Answer);
			return Guess ? Guess(*Read) : FHandNext();
		},
		[Read, Then = MoveTemp(InThen)](const TSharedPtr<FJsonObject>& Answer)
		{
			Then(Answer.IsValid() ? *Read : FAttackResult());
		});
}

// ---- (Rules 84.3.3) FIRST COMMAND ------------------------------------------------------------------------------

FString FCampaignSession::NotebookFile()
{
	// (A run that takes pictures or plays the playtester's steps keeps its own, as it keeps its own campaign and options:
	// the player's own notebook is never written by a run.)
	FString Run;
	const bool bRun = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Run, false) || FParse::Value(FCommandLine::Get(), TEXT("TheaterSteps="), Run, false);
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Campaign"), bRun ? TEXT("lessons-runs.json") : TEXT("lessons.json"));
}

void FCampaignSession::ReadNotebook()
{
	// What he has learnt, kept with the player and not with a save; a missing or unreadable file is a new player's.
	// A run keeps the notes off, so that no picture or step of a run that is not about them finds one on the desk: a
	// run that starts FIRST COMMAND turns them on as any start of it does, -TheaterLessons starts a run as a new player,
	// and -TheaterKeepLessons keeps what the last run left.
	FString Run, Text;
	const bool bRun = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Run, false) || FParse::Value(FCommandLine::Get(), TEXT("TheaterSteps="), Run, false);
	if (bRun && !FParse::Param(FCommandLine::Get(), TEXT("TheaterKeepLessons")))
	{
		Text = FParse::Param(FCommandLine::Get(), TEXT("TheaterLessons")) ? TEXT("{}") : TEXT("{\"on\":false}");
	}
	else if (!FFileHelper::LoadFileToString(Text, *NotebookFile()))
	{
		Text = TEXT("{}");
	}
	// (The code checker, 18 November: the file is read and written back as the reader understood it, so that nothing
	// after its closing brace can reach the rules' arguments and fail the call there.)
	TSharedPtr<FJsonObject> Kept;
	FString Clean;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Clean);
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Kept) || !Kept.IsValid() || !FJsonSerializer::Serialize(Kept.ToSharedRef(), Writer))
	{
		Clean = TEXT("{}");
	}
	FString Json, Error;
	Settle(); // (an order or a month's scene still being answered on another thread first: the rules answer one thing at a time)
	if (Host.IsValid() && !Host->Call(TEXT("notebook"), FString::Printf(TEXT("[%s]"), *Clean), Json, Error))
	{
		UE_LOG(LogTheater, Display, TEXT("FIRST COMMAND: these rules keep no staff notes (%s)"), *Error); // (the stand-in rules, or rules before 84.3.3)
	}
}

void FCampaignSession::KeepNotebook(const TSharedPtr<FJsonObject>& Answer)
{
	const TSharedPtr<FJsonObject>* Book = nullptr;
	if (!Answer.IsValid() || !Answer->TryGetObjectField(TEXT("book"), Book) || !Book || !Book->IsValid())
	{
		return;
	}
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (FJsonSerializer::Serialize(Book->ToSharedRef(), Writer))
	{
		FFileHelper::SaveStringToFile(Text + TEXT("\n"), *NotebookFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

namespace
{
	FLessonNote ReadLessonNote(const TSharedPtr<FJsonObject>& Object)
	{
		FLessonNote Note;
		double Number = 0.0;
		Object->TryGetStringField(TEXT("kind"), Note.Kind);
		Object->TryGetStringField(TEXT("id"), Note.Id);
		if (Object->TryGetNumberField(TEXT("n"), Number)) { Note.N = static_cast<int32>(Number); }
		if (Object->TryGetNumberField(TEXT("of"), Number)) { Note.Of = static_cast<int32>(Number); }
		Object->TryGetStringField(TEXT("head"), Note.Head);
		Object->TryGetStringField(TEXT("text"), Note.Text);
		Object->TryGetStringField(TEXT("deed"), Note.Deed);
		Object->TryGetStringField(TEXT("does"), Note.Does);
		Object->TryGetStringField(TEXT("due"), Note.Due);
		if (Object->TryGetNumberField(TEXT("region"), Number) && Number >= 0.0) { Note.Region = static_cast<int32>(Number); }
		return Note;
	}

	FString LessonMomentArgs(bool bCard, const TArray<int32>& InHand, bool bOpen)
	{
		return FString::Printf(TEXT("{\"card\":%s,\"inHand\":[%s],\"open\":%s}"), bCard ? TEXT("true") : TEXT("false"), *IdList(InHand), bOpen ? TEXT("true") : TEXT("false"));
	}
}

bool FCampaignSession::ReadLessonDesk(const TSharedPtr<FJsonObject>& Object, FLessonDesk& Out)
{
	Out = FLessonDesk();
	if (!Object.IsValid())
	{
		return false;
	}
	KeepNotebook(Object);
	Out.bValid = true;
	Object->TryGetBoolField(TEXT("on"), Out.bOn);
	Object->TryGetBoolField(TEXT("first"), Out.bFirst);
	const TSharedPtr<FJsonObject>* Note = nullptr;
	if (Object->TryGetObjectField(TEXT("note"), Note) && Note && Note->IsValid())
	{
		Out.bNote = true;
		Out.Note = ReadLessonNote(*Note);
	}
	const TSharedPtr<FJsonObject>* Laid = nullptr;
	if (Object->TryGetObjectField(TEXT("laid"), Laid) && Laid && Laid->IsValid())
	{
		Out.bLaid = true;
		Out.Laid = ReadLessonNote(*Laid);
	}
	Object->TryGetStringArrayField(TEXT("stamped"), Out.Stamped);
	double Number = 0.0;
	if (Object->TryGetNumberField(TEXT("waiting"), Number)) { Out.Waiting = static_cast<int32>(Number); }
	if (Object->TryGetNumberField(TEXT("done"), Number)) { Out.Done = static_cast<int32>(Number); }
	return true;
}

bool FCampaignSession::LessonNow(bool bCard, const TArray<int32>& InHand, FLessonDesk& Out)
{
	TSharedPtr<FJsonObject> Object;
	return CallRules(TEXT("lessonNow"), FString::Printf(TEXT("[%s]"), *LessonMomentArgs(bCard, InHand, bLessonPageOpen)), Object) && ReadLessonDesk(Object, Out);
}

bool FCampaignSession::LessonDeed(const FString& Deed, bool bCard, FLessonDesk& Out)
{
	TSharedPtr<FJsonObject> Object;
	return CallRules(TEXT("lessonDeed"), FString::Printf(TEXT("[%s,%s]"), *JsonQuote(Deed), *LessonMomentArgs(bCard, TArray<int32>(), bLessonPageOpen)), Object) && ReadLessonDesk(Object, Out);
}

bool FCampaignSession::LessonAct(const FString& Action, bool bCard, const TArray<int32>& InHand, FLessonDesk& Out)
{
	TSharedPtr<FJsonObject> Object;
	return CallRules(TEXT("lessonAct"), FString::Printf(TEXT("[%s,%s]"), *JsonQuote(Action), *LessonMomentArgs(bCard, InHand, bLessonPageOpen)), Object) && ReadLessonDesk(Object, Out);
}

FLessonGuide FCampaignSession::LessonGuide(bool bCard, const TArray<int32>& InHand)
{
	FLessonGuide Out;
	TSharedPtr<FJsonObject> Object;
	if (!CallRules(TEXT("guide"), FString::Printf(TEXT("[%s]"), *LessonMomentArgs(bCard, InHand, bLessonPageOpen)), Object) || !Object.IsValid())
	{
		return Out;
	}
	Out.bValid = true;
	Object->TryGetBoolField(TEXT("first"), Out.bFirst);
	Object->TryGetStringField(TEXT("label"), Out.Label);
	Object->TryGetStringField(TEXT("head"), Out.Head);
	Object->TryGetStringField(TEXT("state"), Out.State);
	Object->TryGetStringField(TEXT("setting"), Out.Setting);
	Object->TryGetBoolField(TEXT("on"), Out.bOn);
	double Number = 0.0;
	if (Object->TryGetNumberField(TEXT("waiting"), Number)) { Out.Waiting = static_cast<int32>(Number); }
	const TArray<TSharedPtr<FJsonValue>>* Groups = nullptr;
	if (Object->TryGetArrayField(TEXT("groups"), Groups))
	{
		for (const TSharedPtr<FJsonValue>& GroupValue : *Groups)
		{
			const TSharedPtr<FJsonObject> Group = GroupValue.IsValid() ? GroupValue->AsObject() : nullptr;
			if (!Group.IsValid())
			{
				continue;
			}
			FString Label;
			Group->TryGetStringField(TEXT("label"), Label);
			Out.GroupLabels.Add(Label);
			TArray<FLessonRow>& Rows = Out.Groups.AddDefaulted_GetRef();
			const TArray<TSharedPtr<FJsonValue>>* RowValues = nullptr;
			if (Group->TryGetArrayField(TEXT("rows"), RowValues))
			{
				for (const TSharedPtr<FJsonValue>& RowValue : *RowValues)
				{
					const TSharedPtr<FJsonObject> Entry = RowValue.IsValid() ? RowValue->AsObject() : nullptr;
					if (!Entry.IsValid())
					{
						continue;
					}
					FLessonRow& Row = Rows.AddDefaulted_GetRef();
					Entry->TryGetStringField(TEXT("id"), Row.Id);
					if (Entry->TryGetNumberField(TEXT("n"), Number)) { Row.N = static_cast<int32>(Number); }
					Entry->TryGetStringField(TEXT("head"), Row.Head);
					Entry->TryGetStringField(TEXT("state"), Row.State);
					Entry->TryGetBoolField(TEXT("lit"), Row.bLit);
				}
			}
		}
	}
	return Out;
}

FLessonDoor FCampaignSession::LessonDoor()
{
	// (Asked on the door, before any campaign is under way: straight to the rules, which need no war for it. An order or
	// a month's scene still being answered on another thread first, should the door be opened from a war: the rules
	// answer one thing at a time.)
	Settle();
	FLessonDoor Out;
	FString Json, Error;
	TSharedPtr<FJsonObject> Object;
	if (!bReady || !Host.IsValid() || !Host->Call(TEXT("notebook"), TEXT("[]"), Json, Error)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
	{
		return Out;
	}
	const TSharedPtr<FJsonObject>* Door = nullptr;
	const TSharedPtr<FJsonObject>* Campaign = nullptr;
	if (!Object->TryGetObjectField(TEXT("door"), Door) || !Door || !Door->IsValid() || !Object->TryGetObjectField(TEXT("campaign"), Campaign) || !Campaign || !Campaign->IsValid())
	{
		return Out;
	}
	Out.bValid = true;
	(*Door)->TryGetStringField(TEXT("label"), Out.Label);
	(*Door)->TryGetStringField(TEXT("line"), Out.Line);
	(*Door)->TryGetBoolField(TEXT("done"), Out.bAllDone);
	(*Campaign)->TryGetStringField(TEXT("nat"), Out.Nation);
	double From = 1.0;
	if ((*Campaign)->TryGetNumberField(TEXT("from"), From)) { Out.From = static_cast<int32>(From); }
	(*Campaign)->TryGetStringField(TEXT("level"), Out.Level);
	(*Campaign)->TryGetStringField(TEXT("depth"), Out.Depth);
	return Out;
}

bool FCampaignSession::OpenOpeningCard()
{
	TSharedPtr<FJsonObject> Object;
	bool bOk = false;
	return CallRules(TEXT("openingCard"), TEXT("[]"), Object) && Object.IsValid() && Object->TryGetBoolField(TEXT("ok"), bOk) && bOk;
}
