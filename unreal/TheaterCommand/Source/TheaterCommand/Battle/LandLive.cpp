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
#include "Release2Ship.h"
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
		// (release 1.5: Stretcher bearers start off, the release 1.5 lead 23 Nov 05:37Z under Brian's rule that a new rule ships on only
		// if it plays clean. The pre-check's land_live found a light case the rules stand back up still lying on screen: the live
		// feed never takes his fall back, and the 3D has no man getting up. -TheaterLandRuleBearers turns them on for a run.)
		{ TEXT("bearers"), TEXT("Stretcher bearers"), TEXT("Bearers carry the wounded back, and the lightly hurt return to the line later in the battle. Off: no one comes back."), TEXT("TheaterLandNoBearers"), false, TEXT("TheaterLandRuleBearers") },
		{ TEXT("patrol"), TEXT("Night patrol"), TEXT("While you place your army, send a unit on foot out the night before to find hidden posts; it may be caught. Needs Forest ambushes. Off: no patrols."), TEXT("TheaterLandNoPatrol"), true, nullptr },
		// (looks only, release 2: Brian, 23 November 01:11Z, "Do all of these"; the 3D game draws them once its own trial
		// passes, and nothing in the fight reads them)
		// (Captured tanks, Old ruts and Scarred ground start off, the lead 23 Nov 01:40Z and 01:45Z: a drawn look waits for the art director's
		// stills and 45 fps on the Mac, then its row flips to on in one line; -TheaterLandRuleCaptured, -TheaterLandRuleOldRuts and
		// -TheaterLandRuleScars turn one on for a run. Worn uniforms stayed on in release 2 (nothing draws them until the 3D's own trial word); in 1.5 they start off too. The 3D's
		// -TheaterLandWorn=N and -TheaterLandOldRuts=N stand-ins are other flags.)
		{ TEXT("captured"), TEXT("Captured tanks"), TEXT("An army that lately beat one with tanks fields a few of them in its own paint. Off: every tank is its own side's."), TEXT("TheaterLandNoCaptured"), false, TEXT("TheaterLandRuleCaptured") },
		// (release 1.5: Worn uniforms start off with the other looks, the release 1.5 lead 23 Nov 06:38Z; -TheaterLandRuleWorn
		// turns them on for a run. The 3D's -TheaterLandWorn=N stand-in is another flag.)
		{ TEXT("worn"), TEXT("Worn uniforms"), TEXT("Men long in the line without a rest wear faded, patched kit. Off: every army is turned out fresh."), TEXT("TheaterLandNoWorn"), false, TEXT("TheaterLandRuleWorn") },
		{ TEXT("mud"), TEXT("Old ruts"), TEXT("A field fought over in the mud keeps its ruts for the next battle there. Off: the ground dries out between battles."), TEXT("TheaterLandNoOldRuts"), false, TEXT("TheaterLandRuleOldRuts") },
		// (Shell blasts, release 2: Brian, 23 November 05:04Z, "explosions on the ground should be able to take out multiple
		// people if they are close to it". The drill's, looks only: the rules' counts are untouched. Off until a Mac
		// off/on line and the art director pass; -TheaterLandRuleBlast turns it on for a run.)
		{ TEXT("escort"), TEXT("Infantry with tanks"), TEXT("Riflemen near a friendly tank walk behind it for cover as it advances, and step out of the way of their own tanks and lorries. An enemy tank can run a man over. Off: men and vehicles move on their own."), TEXT("TheaterLandNoEscort"), false, TEXT("TheaterLandRuleEscort") },
		// (Squad life, release 2: Brian, 25 November 13:40Z, "i dont want all the soldiers of a squad doing the exact same
		// thing at the same time. I want them to work together and look real as individuals apart of a squad". The drill's,
		// looks only: nothing in the fight reads it, and a battle ends the same with it on or off (tests/land-squad-life). It
		// brings the drill book on with the men drawn as sections (no platoon crowds). Off until a Mac off/on line and the
		// art director pass it; -TheaterLandRuleSquadLife turns it on for a run.)
		{ TEXT("life"), TEXT("Squads of individuals"), TEXT("Each soldier does his own part of his squad's job in his own time: pairs rush in turn while their mates cover them, men react a moment apart, stand, kneel or lie as their cover suits, and each watches his own arc. Off: a squad moves and fires more as one."), TEXT("TheaterLandNoSquadLife"), false, TEXT("TheaterLandRuleSquadLife") },
		// (Heads up, hatches down, release 2: Brian, 25 November 15:11Z, "do those 3 ideas": tank commanders ride with heads
		// out for a better view, then button up when bullets fly. The drill's, looks only: each commander closes his
		// hatch in his own time, and a battle ends the same with it on or off (tests/land-hatches). What a closed-up tank
		// can see is not touched: that part changes results and waits for the design director's ruling under a switch of
		// its own. Off until a Mac off/on line and the art director pass it; -TheaterLandRuleHatches turns it on for a run.)
		{ TEXT("hatches"), TEXT("Heads up, hatches down"), TEXT("Each tank commander rides with his head out of the hatch for the view, and drops inside in his own time when rounds come at his tank or the enemy gets close: green crews early and for long, veterans late and briefly. Off: a platoon's tanks all close up together when the enemy is within 120 metres."), TEXT("TheaterLandNoHatches"), false, TEXT("TheaterLandRuleHatches") },
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
		// (Men in lorries, release 1.5: Brian, 23 November 14:10Z, "troops are stuck under the truck", and 15:50Z "yes" to the
		// ride and the hop out in 1.5 if it passes the pre-check and his Mac in time. The drill seats each lorried man on
		// her benches and has them get down one at a time (sim.trucks); the 3D seats a man dressed as each on her bench,
		// her tilt stowed, and walks him to the tailboard and down.
		// (release 1.5, 24 November: ON. The Mac held 62-64 fps while the men got down (Poznan, 88-145 s, 58-68 the whole
		// battle) on d40cc95d. The art director judges the close stills (-TheaterLandLorryPan) on the launch candidate;
		// should they fail, this row goes back to false. -TheaterLandNoTrucks turns it off for a run.)
		{ TEXT("trucks"), TEXT("Men in lorries"), TEXT("Lorried infantry ride on the benches in the back of their lorries, the canvas rolled away, and get down over the tailboard one at a time. Off: they ride out of sight under the canvas and all get down at once."), TEXT("TheaterLandNoTrucks"), true, TEXT("TheaterLandRuleTrucks") },
		{ TEXT("scars"), TEXT("Scarred ground"), TEXT("A field fought over before keeps the craters of those battles, weathered by the months since. Off: every field is fresh."), TEXT("TheaterLandNoScars"), false, TEXT("TheaterLandRuleScars") },
		// (release 1.5, Brian 23 Nov 06:35Z "yes 1 and 2 for 1.5": the campaign's weather word on the field, off until the art
		// director's stills and 45 fps pass it; -TheaterLandRuleWetMud turns it on for a run. The 3D's -TheaterLandWeatherWord=
		// stand-in is another flag.)
		{ TEXT("wet"), TEXT("Wet mud"), TEXT("The month's weather reaches the field: deep mud in the thaw in the east, snow in winter, rain under a closed sky. Off: the ground is dry and the sky fair, whatever the month."), TEXT("TheaterLandNoWetMud"), false, TEXT("TheaterLandRuleWetMud") },
		// (release 1.6, the air battles: Brian, 23 November 06:52Z, the campaign's air fights opened as 3D air battles. FLY IT
		// YOURSELF on the raid card of a bombing raid by day or a fight for the sky. Off until the playtester passes it (ruling
		// 217) and the Mac holds 45 fps, then this row flips to on in one line; -TheaterAirCampaignLive turns it on for a run.
		// The campaign reads it (AAirBattle::CampaignLiveOn); it is never sent to the land rules.)
		// (release 1.5, 24 November 03:42Z: ON. The playtester passed it on 4351110c, the Mac held 49-60 fps in flight (Part B on
		// 6c676061) and the art director passed every raid's stills on 8913978d. -TheaterAirNoCampaignLive turns it off for a run.)
		{ TEXT("flyLive"), TEXT("Fly raids yourself"), TEXT("On a bombing raid by day or a fight for the sky, FLY IT YOURSELF takes you into the battle in the air, where you give your groups their orders. Off: your crews fly every raid."), TEXT("TheaterAirNoCampaignLive"), true, TEXT("TheaterAirCampaignLive") },
		// (release 2, the air battles over an army: Brian, 24 November 2026, "if a bomber group is bombing troops instead of a
		// factory or town and they get intercepted than i want that playable". FLY IT YOURSELF on the raid card of a raid on
		// troops: the 3D stands the army's column on its road under the planes (its tanks, lorries, guns and men), and every
		// bomb and strafing pass the rules count shows on the piece it struck. Off until the playtester and the Mac pass it,
		// then this row flips to on in one line; -TheaterAirRuleTroops turns it on for a run, -TheaterAirNoRuleTroops off.
		// The campaign reads it (AAirBattle::CampaignLiveOn(TEXT("ground"))); it is never sent to the land rules.)
		{ TEXT("flyTroops"), TEXT("Fly raids on troops yourself"), TEXT("On a raid on an army's column, FLY IT YOURSELF takes you into the battle in the air over its road, where you watch the bombs and strafing passes land on its tanks, lorries, guns and men and give your groups their orders. Off: your crews fly every raid on troops."), TEXT("TheaterAirNoRuleTroops"), false, TEXT("TheaterAirRuleTroops") },
		// (Bomber nose art, release 2: the modeller's 200 paintings on the American bombers in the air battles, Brian, 25
		// November 2026 01:30Z, "american only". Looks only: nothing in the battle reads it. Off until the Mac holds 45 fps
		// with it and the art director has passed it, then this row flips to on in one line; -TheaterAirNoseArt turns it
		// on for a run, -TheaterAirNoNoseArt off. The air battle reads it (AAirBattle::DrawNoseArt); it is never sent to the
		// land rules.)
		{ TEXT("noseArt"), TEXT("Bomber nose art"), TEXT("American bombers in the air battles wear painted nose art, each crew's own picture and name. Off: every bomber's nose is bare."), TEXT("TheaterAirNoNoseArt"), false, TEXT("TheaterAirNoseArt") },
		// (Earned colours, Brian 25 November 03:27Z: "unique colorings or markings like the tuskegee airmen". Off until the
		// art director has scored the Mac's stills (the historian checked the palette twice); -TheaterAirUnitColours turns it
		// on for a run, -TheaterAirNoUnitColours off. The air battle reads it (Battle/AirBattleUnitColours.cpp); never sent to the
		// land rules.)
		{ TEXT("unitColours"), TEXT("Earned colours"), TEXT("Air groups that have flown hard wear colours of their own: first the spinner, then a band round the nose, then the tail. Off: every aircraft wears its plain paint."), TEXT("TheaterAirNoUnitColours"), false, TEXT("TheaterAirUnitColours") },
		// (Old wrecks, Brian 23 November 20:09Z: starts off as Scarred ground does, until the effects thread's wrecks pass the
		// art director and 45 fps; -TheaterLandRuleOldWrecks turns it on for a run. The 3D's -TheaterLandOldWrecks=N and
		// -TheaterLandOldWreckMonths=M stand-ins are other flags.)
		{ TEXT("wrecks"), TEXT("Old wrecks"), TEXT("A field fought over before keeps the burnt-out tanks and armoured vehicles those battles left on it. Off: the field is cleared between battles."), TEXT("TheaterLandNoOldWrecks"), false, TEXT("TheaterLandRuleOldWrecks") },
		// (Release 2, the scramble offer: Brian, 24 November 2026 22:21Z, "i want every air battle playable". In the enemy's
		// month the campaign stops where an enemy raid on his ground meets his fighters and offers SCRAMBLE: taken, he flies
		// the defence in the battle in the air (AAirBattle, his side 'def'), and the month runs on with what the battle
		// decided. The month's stop is the warplanes thread's (air-battles/every-kind/CONTRACT.md section 7); until it lands,
		// -TheaterAirScrambleDemo has the bridge's stand-in offer one raid after the month, booking nothing. Off until that
		// stop lands, the Mac holds 45 fps, the art director and a playtest pass it (ruling 217); -TheaterAirRuleDefend turns
		// it on for a run. The campaign reads it (AAirBattle::DefendOn), as flyLive; it is never sent to the land rules.)
		{ TEXT("flyDefend"), TEXT("Scramble your fighters"), TEXT("When an enemy raid on your ground meets your fighters, the month stops on a SCRAMBLE card: take it and you fly your fighters against the raid in the battle in the air. Off: your staff meets every enemy raid."), TEXT("TheaterAirNoRuleDefend"), false, TEXT("TheaterAirRuleDefend") },
		// (Japan's own tank and gun, release 2: the Japan and Pacific thread's finding of 25 November, that Japan fought
		// in a borrowed light tank and a German howitzer. Looks only, a campaign battle's only, off until the art
		// director and a Mac reading pass them; -TheaterLandRuleJapan turns it on for a run.)
		{ TEXT("japan"), TEXT("Japan's own tank and gun"), TEXT("Japanese units are drawn with their own Type 97 medium tank and 75 mm field gun instead of the generic light tank and a German howitzer. Campaign battles only. Off: the generic models."), TEXT("TheaterLandNoJapan"), false, TEXT("TheaterLandRuleJapan") },
		// (Island ground, release 2, looks only: a campaign battle on an island of the campaign's table is drawn as that
		// island, beach and palms or jungle under its own sun, where the campaign's ticket names its look and its place.
		// Off until the island look has its Mac still and the art director's pass; -TheaterLandRuleIsland turns it on
		// for a run. The campaign side's keys, ticket.look and ticket.latLon, are the Japan and Pacific thread's.)
		{ TEXT("island"), TEXT("Island ground"), TEXT("A battle on a Pacific island is drawn as that island: beach, reef and palms, or jungle on the big islands, under its own sun. Campaign battles only. Off: it is drawn on the same ground as any other battle."), TEXT("TheaterLandNoIsland"), false, TEXT("TheaterLandRuleIsland") },
		// (Every infantry force with its own teams, release 2: Brian, 25 November 14:14Z, "Where are the machine gun teams
		// and mortar teams in the infantry squad", and his "Both" at 14:42Z. The rules' own-teams.ts: each infantry,
		// mountain or lorried corps of a campaign battle gives a small share of itself to a machine-gun and a mortar
		// battalion of its own, so nothing is added to a side. It is NOT looks only: it changes what fights, so it stays
		// off until the design director's ruling and the fairness tables pass it; -TheaterLandRuleTeams turns it on for
		// a run. The weapons themselves are drawn on the 3D's own trial word, teamguns. In the joined build this row
		// goes after the squad life, hatches and tank machine gun rows.)
		{ TEXT("teams"), TEXT("Machine gun and mortar teams"), TEXT("Each infantry, mountain and motorized corps fields its own machine gun teams and mortar teams behind its line, firing in support. They are part of the corps, not extra men. Campaign battles only. Off: a battle has only the teams the campaign built."), TEXT("TheaterLandNoTeams"), false, TEXT("TheaterLandRuleTeams") },
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
	// (Release 2 assembly; Brian, 25 November 2026: "turn them on automatically") A row the release 2 ship list ships on
	// starts on too, until he switches it off on the card (Release2Ship.h; -TheaterR2Off leaves the rows as before it).
	bool bOn = RuleAt(Index).bOnAtFirst || Release2Ship::Ships(Release2Ship::EKind::LandRule, RuleAt(Index).Key);
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
			return (RuleOn(R) || (Rule.OnFlag && FParse::Param(FCommandLine::Get(), Rule.OnFlag))) && !FParse::Param(FCommandLine::Get(), Rule.OffFlag);
		}
	}
	return false;
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
		const FString RuleKey(LandRuleTable[R].Key);
		if (RuleKey.StartsWith(TEXT("fly"), ESearchCase::CaseSensitive) || RuleKey.Equals(TEXT("noseArt"), ESearchCase::CaseSensitive) || RuleKey.Equals(TEXT("unitColours"), ESearchCase::CaseSensitive))
		{
			continue; // (the air battles' own switches, on the same card: flyLive, flyTroops, flyDefend, noseArt, unitColours and those to come; the campaign and the air battle read them, the land rules never do)
		}
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
