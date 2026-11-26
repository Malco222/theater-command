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
	// (each tip: what the rule does, then "Off:" what changes; the UI director, 22 November 20:56Z. Smoke is off until he
	// switches it on: the tactics thread's paired measure, 21:29Z, has an attacking staff's own screens blunting its own
	// assault on plains, forest and city, so it stays his choice until that is mended.)
	const FLandLive::FRule LandRuleTable[] = {
		{ TEXT("cutters"), TEXT("Hedge cutters"), TEXT("Your tanks cut a gap through a hedge where you pick (from July 1944). Off: no cutting; tanks cross hedges as they always have."), TEXT("TheaterLandNoCutters"), true, nullptr },
		{ TEXT("smoke"), TEXT("Smoke on call"), TEXT("Your men on foot lay a smoke screen along a line you drag, and both staffs lay their own. Off: no smoke screens."), TEXT("TheaterLandNoSmoke"), false, TEXT("TheaterLandSmoke") },
		{ TEXT("hide"), TEXT("Forest ambushes"), TEXT("Units in woods stay hidden from the enemy until found or firing. Off: woods hide no one."), TEXT("TheaterLandNoHide"), true, nullptr },
		{ TEXT("relief"), TEXT("Hold till relieved"), TEXT("A unit digs in to wait for a corps coming up; if help comes late, it breaks sooner. Off: no hold order; help simply arrives."), TEXT("TheaterLandNoRelief"), true, nullptr },
		{ TEXT("rider"), TEXT("Despatch riders"), TEXT("A corps coming up sends a rider ahead; click him for its news. Off: help arrives with no word ahead."), TEXT("TheaterLandNoRider"), true, nullptr },
		// (Brian, 22 November 17:33Z, "make it where we can toggle it off", and 21:21Z, each starting on)
		{ TEXT("phones"), TEXT("Field telephones"), TEXT("The defence's posts are wired to their headquarters, so orders reach them quickly until shellfire cuts a line. Off: no telephone lines."), TEXT("TheaterLandNoPhones"), true, nullptr },
		{ TEXT("bearers"), TEXT("Stretcher bearers"), TEXT("Bearers carry the wounded back, and the lightly hurt return to the line later in the battle. Off: no one comes back."), TEXT("TheaterLandNoBearers"), true, nullptr },
		{ TEXT("patrol"), TEXT("Night patrol"), TEXT("While you place your army, send a unit on foot out the night before to find hidden posts; it may be caught. Needs Forest ambushes. Off: no patrols."), TEXT("TheaterLandNoPatrol"), true, nullptr },
		// (looks only, release 2: Brian, 23 November 01:11Z, "Do all of these"; the 3D game draws them once its own trial
		// passes, and nothing in the fight reads them)
		// (Captured tanks, Old ruts and Scarred ground start off, the lead 23 Nov 01:40Z and 01:45Z: a drawn look waits for the art director's
		// stills and 45 fps on the Mac, then its row flips to on in one line; -TheaterLandRuleCaptured, -TheaterLandRuleOldRuts and
		// -TheaterLandRuleScars turn one on for a run. Worn uniforms stay on: nothing draws them until the 3D's own trial word. The 3D's
		// -TheaterLandWorn=N and -TheaterLandOldRuts=N stand-ins are other flags.)
		{ TEXT("captured"), TEXT("Captured tanks"), TEXT("An army that lately beat one with tanks fields a few of them in its own paint. Off: every tank is its own side's."), TEXT("TheaterLandNoCaptured"), false, TEXT("TheaterLandRuleCaptured") },
		{ TEXT("worn"), TEXT("Worn uniforms"), TEXT("Men long in the line without a rest wear faded, patched kit. Off: every army is turned out fresh."), TEXT("TheaterLandNoWorn"), true, nullptr },
		{ TEXT("mud"), TEXT("Old ruts"), TEXT("A field fought over in the mud keeps its ruts for the next battle there. Off: the ground dries out between battles."), TEXT("TheaterLandNoOldRuts"), false, TEXT("TheaterLandRuleOldRuts") },
		// (Shell blasts, release 2: Brian, 23 November 05:04Z, "explosions on the ground should be able to take out multiple
		// people if they are close to it". The drill's, looks only: the rules' counts are untouched. Off until a Mac
		// off/on line and the art director pass; -TheaterLandRuleBlast turns it on for a run.)
		{ TEXT("escort"), TEXT("Infantry with tanks"), TEXT("Riflemen near a friendly tank walk behind it for cover as it advances, and step out of the way of their own tanks and lorries. An enemy tank can run a man over. Off: men and vehicles move on their own."), TEXT("TheaterLandNoEscort"), false, TEXT("TheaterLandRuleEscort") },
		// (release 2: Brian, 23 November 07:01Z, yes to half-tracks carrying armoured infantry; looks only, starting off
		// until the Mac holds 45 fps with it and the art director has passed it; -TheaterLandRuleMech turns it on for a run)
		{ TEXT("mech"), TEXT("Armoured infantry"), TEXT("An armoured unit's riflemen ride behind its tanks in their army's half-tracks or lorries, and get down to fight near the enemy. Off: the tanks go alone."), TEXT("TheaterLandNoMech"), false, TEXT("TheaterLandRuleMech") },
		// (Self-propelled guns and tank destroyers, release 2: Brian, 23 November 07:01Z, yes to "a tank destroyer and a
		// self-propelled gun for each major army"; looks only, a campaign battle's only, off until the Mac and the art
		// director pass them)
		{ TEXT("spg"), TEXT("Self-propelled guns"), TEXT("Some of an army's field guns are drawn as its self-propelled howitzers (the American M7, the British Sexton, the German Hummel, the Soviet SU-76), driving with the battery instead of towed. Campaign battles only. Off: every gun is towed."), TEXT("TheaterLandNoSpg"), false, TEXT("TheaterLandRuleSpg") },
		{ TEXT("td"), TEXT("Tank destroyers"), TEXT("One vehicle in some armoured units is drawn as its army's tank destroyer or assault gun (the M10, the SU-85, the Semovente), fighting as its unit does. Campaign battles only. Off: the tanks alone."), TEXT("TheaterLandNoTd"), false, TEXT("TheaterLandRuleTd") },
		// (Armoured cars and AA vehicles, release 2: the same yes, item 3; looks only, a campaign battle's only, off until the
		// Mac and the art director pass them)
		{ TEXT("cars"), TEXT("Armoured cars"), TEXT("Some units bring their army's armoured cars (the German Sd.Kfz. 222, the American M8, the rest a generic car), out ahead on the flanks until the enemy is near, then back behind the unit. Campaign battles only. Off: none drawn."), TEXT("TheaterLandNoCars"), false, TEXT("TheaterLandRuleCars") },
		{ TEXT("aa"), TEXT("AA vehicles"), TEXT("Some units and batteries bring their army's anti-aircraft vehicles (the American M16, the German flak half-track, the rest a flak lorry), kept behind them. Campaign battles only. Off: none drawn."), TEXT("TheaterLandNoAa"), false, TEXT("TheaterLandRuleAa") },
		{ TEXT("light"), TEXT("Jeeps and motorcycles"), TEXT("Units bring their army's jeeps, field cars and motorcycles (the Willys jeep, the Kubelwagen, the BMW with its sidecar and more), riding out ahead with the screen or parked behind, despatch riders coming and going. Campaign battles only. Off: none drawn."), TEXT("TheaterLandNoLight"), false, TEXT("TheaterLandRuleLight") },
		{ TEXT("blast"), TEXT("Shell blasts"), TEXT("One shell can take out several men standing close to where it bursts; men lying down or in cover are hit less often. Off: each man falls to a shell of his own."), TEXT("TheaterLandNoBlast"), false, TEXT("TheaterLandRuleBlast") },
		{ TEXT("scars"), TEXT("Scarred ground"), TEXT("A field fought over before keeps the craters of those battles, weathered by the months since. Off: every field is fresh."), TEXT("TheaterLandNoScars"), false, TEXT("TheaterLandRuleScars") },
		// (Japan's own tank and gun, release 2: the Japan and Pacific thread's finding of 25 November, that Japan fought
		// in a borrowed light tank and a German howitzer. Looks only, a campaign battle's only, off until the art
		// director and a Mac reading pass them; -TheaterLandRuleJapan turns it on for a run.)
		{ TEXT("japan"), TEXT("Japan's own tank and gun"), TEXT("Japanese units are drawn with their own Type 97 medium tank and 75 mm field gun instead of the generic light tank and a German howitzer. Campaign battles only. Off: the generic models."), TEXT("TheaterLandNoJapan"), false, TEXT("TheaterLandRuleJapan") },
		// (Island ground, release 2, looks only: a campaign battle on an island of the campaign's table is drawn as that
		// island, beach and palms or jungle under its own sun, where the campaign's ticket names its look and its place.
		// Off until the island look has its Mac still and the art director's pass; -TheaterLandRuleIsland turns it on
		// for a run. The campaign side's keys, ticket.look and ticket.latLon, are the Japan and Pacific thread's.)
		{ TEXT("island"), TEXT("Island ground"), TEXT("A battle on a Pacific island is drawn as that island: beach, reef and palms, or jungle on the big islands, under its own sun. Campaign battles only. Off: it is drawn on the same ground as any other battle."), TEXT("TheaterLandNoIsland"), false, TEXT("TheaterLandRuleIsland") },
		// (Attackers stay where they stop to fight, release 2: the art director's number of 26 November 2026 00:50Z and
		// the design director's rulings of 01:02Z and 01:28Z. Off, a block on an attack order asks afresh every tick
		// whether it is well in reach, with no memory: its target hovering at that edge, it stands a tick and wheels to
		// what it fights, marches a tick and wheels back to its way, and rocks on the spot (counted in today's rules at
		// its gate: the note land-battles/facing-gate-1126.md). Ruled a fault, for every block on an attack order. On, a
		// block that has stood to fight its target stands while the target is within its hold, the stand's own figure,
		// and closes again only beyond it; and by the same rule, "what it chose it keeps while that still holds", it
		// keeps its way over a river, its place beside a friend and the place it was sent to (the rules' sim.standRule
		// and sim.standKeeps). It is NOT looks only: it moves every battle, so it has this row of its own and its own
		// fairness gate, and stays off until that gate, the Mac and the art director pass it; -TheaterLandRuleStand
		// turns it on for a run. The two rows below work only with it on (LandRuleNeeds, under this table; the bridge
		// and live.ts read all three). Key and flags mine; name, line and the rows' order the UI director's of 01:24Z.)
		{ TEXT("stand"), TEXT("Attackers stay where they stop to fight"), TEXT("A unit sent to attack closes to good range, stops and fights from there, and moves up again only when its enemy has drawn well back. Off: with its enemy at the edge of its range, it stops, turns to fight, turns back and steps on, over and over, rocking on the spot, as before. This changes how battles turn out. A change takes effect from the next battle."), TEXT("TheaterLandNoStand"), false, TEXT("TheaterLandRuleStand") },
		// (Units face the fire that hurts most, release 2: the design director's ruling of 25 November 21:54Z on the
		// fairness gate's trace. Off, any blow on a block's front holds it to its facing, at any range, however light
		// and whatever its cover: ruled a fault. On, a block is held only by the enemy whose blows cost it most after
		// its cover, and turns to meet another that costs it more (the rules' sim.faceRule). It is NOT looks only: it
		// moves every battle, guns hold too, so it has this row of its own and its own fairness gate, and stays off
		// until that gate and the design director pass it; -TheaterLandRuleFacing turns it on for a run. "Support
		// teams in battle" below does nothing without it (the same ruling: the bridge and live.ts read both). Its key,
		// name and line are the UI director's of 25 November 22:14Z, and "A change takes effect from the next battle." on
		// both rows theirs of 22:30Z: a battle reads its rows once, as it begins.)
		{ TEXT("facing"), TEXT("Units face the fire that hurts most"), TEXT("A unit faces the enemy that hurts it most, once its cover is counted, and turns to meet another only when that one hurts it more. Fire that truly hurts still holds a unit to its facing; a far-off gun that does it little harm no longer keeps it facing the wrong way while infantry comes in at its side. Off: any enemy striking a unit's front, however lightly and from however far, keeps it from turning to meet another at its side, as before. This changes how battles turn out. A change takes effect from the next battle. It works only with \u201CAttackers stay where they stop to fight\u201D on."), TEXT("TheaterLandNoFacing"), false, TEXT("TheaterLandRuleFacing") },
		// (Every infantry force with its own teams, release 2: Brian, 25 November 14:14Z, "Where are the machine gun teams
		// and mortar teams in the infantry squad", and his "Both" at 14:42Z. The rules' own-teams.ts: each infantry,
		// mountain or lorried corps of a campaign battle gives a small share of itself to a machine-gun and a mortar
		// battalion of its own, so nothing is added to a side. It is NOT looks only: it changes what fights, so it stays
		// off until the design director's ruling and the fairness tables pass it; -TheaterLandRuleTeams turns it on for
		// a run. The weapons themselves are drawn on the 3D's own trial word, teamguns. In the joined build this row
		// goes after the squad life, hatches and tank machine gun rows. Its name and line are the UI director's of
		// 25 November 17:58Z (the campaign's row is "Machine gun and mortar brigades", the look's "Gun crews at work").
		// Since the design director's ruling of 17:52Z the same switch turns on the rules' sim.teamsRule in every
		// battle, the custom battles too: a team's fire shakes by its own harm, and a defence's line stays in its
		// places while its teams fire. So the line says "into battle", not "campaign battles".)
		{ TEXT("teams"), TEXT("Support teams in battle"), TEXT("Every infantry corps takes its own machine gun and mortar teams into battle, drawn from its own men, not extra ones, and they fire in support from behind its line. This changes how battles turn out. A change takes effect from the next battle. It works only with \u201CAttackers stay where they stop to fight\u201D and \u201CUnits face the fire that hurts most\u201D on."), TEXT("TheaterLandNoTeams"), false, TEXT("TheaterLandRuleTeams") },
		// (Drawing up while he places his army, release 2: Brian, 26 November 2026 03:45Z, "i should be able to move my
		// units in a formation before the battle begins during the deployed phase". A control of the setup step and no
		// battle rule (the design director, 03:50Z): several units dragged as one, a line drawn on his ground, and LINE,
		// TWO LINES, WEDGE, COLUMN, each place by the rules' own land:place, all of them or none. Off until the Mac and
		// the art director pass it; -TheaterLandRuleDrawUp turns it on for a run. Name and line the UI director's of
		// 03:51Z. The rules do not read the key: the screen does (LandBattleCommand.cpp, LandBattleScreen.cpp).)
		{ TEXT("drawup"), TEXT("Draw up units together before battle"), TEXT("Before the battle begins you can take several units in hand, drag them as one, and draw them up in a line, two lines, a wedge or a column. Off: you place them one at a time, as before. Only how you place them: no result changes."), TEXT("TheaterLandNoDrawUp"), false, TEXT("TheaterLandRuleDrawUp") },
		// (Groups on the number keys, release 2: Brian, 26 November 2026 03:51Z, "you should be able to group units so
		// you can hit the #1 and the units tied to it are all selected". Cmd on a Mac, Ctrl elsewhere, with a number
		// ties those in hand to it; the number takes them in hand again, with Shift adds them, twice glides the view to
		// them. The clock's pace moves from the numbers to the minus and plus keys while the row is on. A control and
		// no battle rule. Off until the Mac and the art director pass it; -TheaterLandRuleGroups turns it on for a run.
		// Name and line the UI director's of 03:53Z. The rules do not read the key: the screen does.)
		{ TEXT("groups"), TEXT("Groups on the number keys"), TEXT("Tie the units in hand to a number with Cmd and that number (Ctrl on Windows); press the number to take them in hand again, twice to look at them. The clock's pace moves to \u2212 and +. Off: the numbers set the pace, as before. Only how you give orders: no result changes."), TEXT("TheaterLandNoGroups"), false, TEXT("TheaterLandRuleGroups") },
	};
	const TCHAR* const RulesSection = TEXT("TheaterCommand.LandRules");

	/**
	 * Rows that work only with other rows on, each with what it needs in order from the top of its chain (the design
	 * director, 26 November 2026 01:02Z; the UI director's marks of 01:24Z: grey the row, never switch another behind
	 * his back). A row lacking one of these is greyed on the card, cannot be pressed and acts as off; its own setting
	 * is kept, and comes back as he left it when what it needs is on again.
	 */
	struct FRuleNeed
	{
		const TCHAR* Key;
		const TCHAR* Needs[2];
	};
	const FRuleNeed LandRuleNeeds[] =
	{
		{ TEXT("facing"), { TEXT("stand"), nullptr } },
		{ TEXT("teams"), { TEXT("stand"), TEXT("facing") } },
	};
	const FRuleNeed* NeedsOf(const TCHAR* Key)
	{
		for (const FRuleNeed& Need : LandRuleNeeds)
		{
			if (FCString::Strcmp(Need.Key, Key) == 0)
			{
				return &Need;
			}
		}
		return nullptr;
	}
	/** The greyed row last pressed, and when: the card says for a few seconds what it works only with. */
	int32 RefusedRow = INDEX_NONE;
	double RefusedAt = 0.0;
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
	bool bOn = RuleAt(Index).bOnAtFirst;
	if (GConfig)
	{
		GConfig->GetBool(RulesSection, RuleAt(Index).Key, bOn, GGameUserSettingsIni);
	}
	return bOn;
}

bool FLandLive::RuleNow(const TCHAR* Key)
{
	for (int32 R = 0; R < RuleCount(); ++R)
	{
		const FRule& Rule = LandRuleTable[R];
		if (FCString::Strcmp(Rule.Key, Key) == 0)
		{
			if (!((RuleOn(R) || (Rule.OnFlag && FParse::Param(FCommandLine::Get(), Rule.OnFlag))) && !FParse::Param(FCommandLine::Get(), Rule.OffFlag)))
			{
				return false;
			}
			// A row that works only with others on is off for the battle while any of them is (LandRuleNeeds).
			if (const FRuleNeed* Need = NeedsOf(Key))
			{
				for (const TCHAR* Needed : Need->Needs)
				{
					if (Needed && !RuleNow(Needed))
					{
						return false;
					}
				}
			}
			return true;
		}
	}
	return false;
}

int32 FLandLive::RuleLacks(int32 Index)
{
	const FRuleNeed* Need = NeedsOf(RuleAt(Index).Key);
	if (!Need)
	{
		return INDEX_NONE;
	}
	for (const TCHAR* Needed : Need->Needs)
	{
		for (int32 R = 0; Needed && R < RuleCount(); ++R)
		{
			if (FCString::Strcmp(LandRuleTable[R].Key, Needed) == 0 && !RuleOn(R))
			{
				return R;
			}
		}
	}
	return INDEX_NONE;
}

bool FLandLive::RuleLit(int32 Index)
{
	return RuleOn(Index) && RuleLacks(Index) == INDEX_NONE;
}

bool FLandLive::RulePressed(int32 Index)
{
	return Index == RefusedRow && FPlatformTime::Seconds() - RefusedAt < 6.0;
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

void FLandLive::FlipRule(int32 Index)
{
	if (Index < 0 || Index >= RuleCount())
	{
		return;
	}
	// A greyed row cannot be pressed: nothing is switched, its own setting is kept, and the card says why.
	const int32 Lacks = RuleLacks(Index);
	if (Lacks != INDEX_NONE)
	{
		RefusedRow = Index;
		RefusedAt = FPlatformTime::Seconds();
		UE_LOG(LogTheater, Display, TEXT("Battle rules card: %s is greyed and was not switched: it works only with %s on (its own setting is kept: %s)"), RuleAt(Index).Label,
			RuleAt(Lacks).Label, RuleOn(Index) ? TEXT("ON") : TEXT("OFF"));
		return;
	}
	const bool bWas = RuleOn(Index);
	SetRule(Index, !bWas);
	const bool bNow = RuleOn(Index);
	UE_LOG(LogTheater, Display, TEXT("Battle rules card: %s switched %s -> %s (saved; it reads %s)%s"), RuleAt(Index).Label, bWas ? TEXT("ON") : TEXT("OFF"),
		bNow ? TEXT("ON") : TEXT("OFF"), bNow ? TEXT("ON") : TEXT("OFF"), bNow == bWas ? TEXT(": THE SWITCH DID NOT TURN") : TEXT(""));
}

void FLandLive::FlipFromCommandLine()
{
	static bool bDone = false;
	FString Keys;
	if (bDone || !FParse::Value(FCommandLine::Get(), TEXT("TheaterRulesFlip="), Keys, false))
	{
		return;
	}
	bDone = true;
	TArray<FString> Named;
	Keys.ParseIntoArray(Named, TEXT(","), true);
	for (const FString& Key : Named)
	{
		int32 Found = INDEX_NONE;
		for (int32 R = 0; R < RuleCount(); ++R)
		{
			if (Key.Equals(RuleAt(R).Key, ESearchCase::IgnoreCase))
			{
				Found = R;
			}
		}
		if (Found == INDEX_NONE)
		{
			UE_LOG(LogTheater, Warning, TEXT("Battle rules card: no switch called \"%s\" to click"), *Key);
			continue;
		}
		FlipRule(Found);
	}
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
	// as he left it on the Battle rules card (or as it starts), or for one run on the command line)
	Switches.Reset();
	for (int32 R = 0; R < RuleCount(); ++R)
	{
		const bool bOn = RuleNow(LandRuleTable[R].Key);
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

void FLandLive::PlaceMany(const FString& ListJson)
{
	Requests.Enqueue({ TEXT("place"), TEXT("landPlaceMany"), TEXT("[") + ListJson + TEXT("]"), TEXT("several as one") });
	if (Wake) Wake->Trigger();
}

void FLandLive::DrawUp(const FString& ArgumentsJson, const FString& Kind)
{
	Requests.Enqueue({ TEXT("drawup"), TEXT("landDrawUp"), ArgumentsJson, Kind });
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
			else
			{
				// (a yes with a word to it: a draw-up that left some where they are dug in, or stood them a way off)
				A.Note = FieldText(Reply, TEXT("note"));
			}
			// (a block placed stands its men again: the next piece starts them afresh; begun, the clock runs)
			if (A.bOk && (Asked.Kind == TEXT("place") || Asked.Kind == TEXT("drawup"))) bNeedPiece = true;
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
