#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Campaign/CampaignView.h"
#include "Campaign/EdgeNews.h" // (Release 2, news at the map's edge: merge by hand)
#include "CampaignPlayerController.generated.h"

class FCampaignMapData;

class ACampaignMapActor;
class ACampaignCameraPawn;
class AArmyPiece;
class FCampaignSession;
class FJsonObject;

/** What the panel shows about the territory the player has clicked. Worked out once, when the selection changes. */
struct FRegionPanelInfo
{
	bool bVisible = false;
	FText Name;
	FText OwnerLine;
	FLinearColor OwnerColor = FLinearColor::Gray;
	FString OwnerTag; // whose flag to show
	FText Details;
	FText Forces;
	// A country at peace with the player that he could go to war with: its tag, and a word about it.
	FString DeclareTag;
	FText DiplomacyLine;
	// What the player can do here, on home ground: raise corps, build works, and what is already forming.
	FText Holdings;
	FText Prize; // what taking the place is worth, or what holding it pays, in a sentence
	FText PrizeTip; // and in detail
	bool bPrizeToTake = false; // it is not the player's yet
	// The town's building plots (the grand campaign), each as what stands on it or "Free plot".
	TArray<FString> Plots;
	bool bPlotsMine = false;
	int32 PlotsFree = 0, PlotsCanBuild = 0;
	FText ProvinceNote;
	FText SiegeTitle, SiegeText; // the town as a fortress, and how its siege stands
	bool bSiegeOwn = false; // a town of the player's own, besieged: the head is in red (its stores run out by a date); any other siege head is in the heads' brass
	FText LeftWorksTitle, LeftWorksText; // fieldworks standing empty here, and who can use them
	FText PocketTitle, PocketText; // a province cut off from the rest of its country: what its troops live on, and for how long
	bool bPocketSpent = false;
	FText PlacesTitle, PlacesText; // the places of the province that are prizes of their own, and scorched earth
	FText DirectiveText; // a directive of the war cabinet that names this province
	TArray<FProvinceOption> Options;
	/** The heading over those: "PROVINCE", or "ORDERS" for a fleet or an air fleet. */
	FText OptionsTitle = FText::FromString(TEXT("PROVINCE"));
	TArray<FOrderLine> Forming;
	FString FormingHead; // the total of what is on order here, with its breakdown
	FCityShelves Glance; // (release 2) the AT A GLANCE page: garrison, on order here, building and repairs
	// (Release 2; the art director's airfield spec of 24 November, item 3.) The province's airfield as a row of the panel,
	// "Airfield  \u00B7  3 air groups based here": a click opens it. Empty where the province has none on the table.
	FText FieldRow;
	// (Release 2, the town card, tc.UI.TownCard.) The province's figures as tiles: what it pays a month (-1 where the
	// rules give no figure), its industry, its worth and its works' level; and the details line without them.
	int32 TileYield = -1, TileIndustry = 0, TileWorth = 0, TileWorks = 0;
	FText DetailsShort;
	int32 Revision = 0;
};

/** One corps of a picked-up army, as a card in the tray. */
struct FTrayCard
{
	int32 CorpsId = INDEX_NONE;
	FText Name;
	FText Kind;
	int32 Divisions = 0;
	float Strength = 1.0f;
	int32 Men = 0; // (for the bigger card)
	int32 Moves = 0;
	int32 MovesMax = 0;
	FString Marching; // (release 1) "Marching to X, about N months", where it is on a standing march
	bool bPicked = false; // going with the army, or left behind
	bool bCanOrder = false;
	FText Nickname; // empty until the corps has earned one
	FString Marked; // (release 2, the marked army) an enemy corps that took one of his towns: that town
	bool bNew = false; // (release 2, new units ready) his corps handed over as this month opened
	FString NoOrders; // (ruling 176(c)) why it never takes orders, in the rules' sentence
	FText Grade; // "Veteran, 65"
	FString KindId; // inf, arm, art, mot, cav, mtn, eng, fort: for the picture on each division's card
	FString Ally; // an ally's corps marching with the army: its own country's tag
	bool bLook = false; // a card of an army only being looked at: it cannot be given orders from here
	bool bUnknown = false; // a corps in view that nobody has identified
	int32 Stale = 0; // known from a report this many months old
	float Morale = 1.0f; // 0..1: how well it holds together
	// Where the rules keep morale and rest (the grand campaign): both, with their words and why; and each division.
	bool bMood = false;
	float Rest = 1.0f;
	FString MoraleWord, RestWord, Trend, MoodTip, Service;
	bool bResting = false;
	TArray<FViewDivision> Parts;
	TArray<int32> Carry; // (Version 84) its weapons: anti-tank level, machine guns, mortars, snipers; empty where the rule counts none
	bool bDigging = false; // (Version 84) digging, with no works to show yet
	int32 Experience = 0; // 0 to 100
	TArray<FHonourRibbon> Honours;
	// (The code checker, 17 November.) A corps of the town's garrison while the army in the field is in hand: it cannot
	// march with them until it takes the field, so its card takes no click, and says why in the rules' sentence.
	bool bInTown = false;
	FString Apart;
	// ---- (Release 2: the battle schools.) Its button on the card (Campaign/BattleSchool): SEND TO SCHOOL, or BRING BACK
	// while it is there; no label, no button (release 1's rules, a history scenario, an ally's corps, fortress troops).
	FString SchoolLabel;
	FString SchoolTip; // unlit: the rules' reason, word for word
	bool bSchoolLit = false;
	bool bAtSchool = false; // at the schools: it takes no orders, but its card still takes the press of BRING BACK
	// ---- (end of the battle schools)
	// (Release 2, breakup.ts) Its division tiles can be picked and split off (bCanBreak), or why not; the corps it could rejoin.
	bool bCanBreak = false;
	FString BreakWhy;
	int32 RejoinId = INDEX_NONE;
	FString RejoinName;
	// (Release 2, rearguard.ts) It can be ordered to hold as a rearguard now (bCanRear), or it holds as one (bRear).
	bool bCanRear = false;
	bool bRear = false;
};

/** One ship of the fleet in hand, as a card. */
struct FShipCard
{
	FString Model; // "Battleship:UK"
	FText Name; // "Battleship"
	FText Role; // what it is for, in a line
	FText Brief; // and in three words, for the face of the card
	int32 Count = 1; // how many ships this card stands for, when the fleet has too many to show each
	// (Release 2; Brian 23 November 2026 22:23Z: "select which units I want to move one place and ... another".) Its class
	// (bb, cv, ca, dd, ss), its place in the tray, and whether it is left behind: a click on the card says. Only the lit
	// ships sail; those left behind stay where they are as a squadron of their own once the order is given.
	FString Class;
	int32 Index = 0;
	bool bStaying = false;
};

/** The adviser's first steps: shown at the start of a campaign, each ticked as the player does it. */
struct FAdviser
{
	bool bOn = false;
	bool bDone[5] = { false, false, false, false, false }; // an army picked up, sent, a province looked at, an airfield or fleet opened, the month ended
	int32 Revision = 0;
};

/**
 * (Brian's memorial card, 19 November 2026.) The plate for a battleship or a carrier lost at sea: where, and for
 * each ship her name and a line on who she was and the men lost with her. Three ships at most; the rest are counted.
 */
struct FMemorialShow
{
	struct FRow
	{
		FString Name;
		FString Line;
	};
	FString Heading;
	TArray<FRow> Rows;
	FString More;
	double From = 0.0;
	double Until = 0.0;
};

/** The newsreel being shown: the month it is of, its stories, and which is on the screen (-1 for the title). */
struct FNewsreelShow
{
	bool bOn = false;
	int32 Kind = 0; // 0 the month's newsreel, in black and white; 1 the opening flight, the situation told in colour; 2 a year's title card
	FYearCard Year; // for a title card
	FString Date;
	TArray<FNewsCard> Cards;
	int32 Index = -1;
	double Until = 0.0; // when the story on the screen gives way to the next
};

/**
 * (Release 2, the enemy's month near you, round 2; Brian, 24 November 2026 18:01Z: corner pings.) A stop of the watch
 * near him, marked on the small map in the corner: where (a share of the small map's picture, as its marks are), its
 * ink, and whether it has been seen, is on screen or is still to come (TheaterEnemyMoves::EPing).
 */
struct FWatchPing
{
	FVector2D Share = FVector2D::ZeroVector;
	FColor Ink = FColor::White;
	uint8 State = 2; // 0 seen, 1 on screen, 2 to come
	int32 Order = 0; // which stop it is, from 0
};

/** The army the player has picked up: its general, and its corps as cards. */
struct FArmyTray
{
	bool bVisible = false;
	FText Title;
	FText Subtitle;
	FLinearColor Color = FLinearColor::Gray;
	FString Tag; // whose flag to show
	TArray<FTrayCard> Cards;
	bool bFleet = false; // it is a fleet in hand, not an army: its ships are the cards
	bool bLook = false; // somebody's army being looked at, not one in hand: its corps as cards, and no orders
	TArray<FShipCard> Ships;
	FString FleetStance; // patrol, raid or port: what the fleet is doing now
	FString FleetNext; // patrol, raid or hunt: what it will do in the sea it is sent to next
	bool bFleetGrand = false; // the navy as pieces: it can hunt, escort, shell, split and join
	int32 FleetAircraft = 0; // the aircraft its carriers bring, if it has carriers
	FText FleetWarning; // its heavy ships are thinly screened, or its guns have spoken
	// (1.5.2) Why its orders at sea are refused (held where this month's battle left it, or cut off beyond a shut strait), and
	// why its way home is (held, or cut off with no friendly harbour on its side): each dims its buttons, with this as the tip.
	// FleetHomeTip is the way home's own tip where it is cut off with a harbour on its side to make for. Empty when none applies.
	FString FleetSeaWhy, FleetHomeWhy, FleetHomeTip;
	FString Posture; // the standing order the picked corps are under: stand, dig or back; empty if they differ
	bool bByRail = false; // the next order sends them by rail
	bool bBySea = false; // or by sea: to a friendly port, or ashore on an enemy coast
	// (Ruling 118; the art director's railway brief, section 2a.) The one line that says what the trains will do for
	// the army this month, in the rules' words; and the opening of it that takes the railway's colour.
	bool bRailCan = false;
	FString RailStrong, RailLine;
	bool bLean = false; // the picked corps take no drafts
	FText JoinButton; // "ASK 1 SLOVAK CORPS TO JOIN": allied corps beside the army that could march with it. Empty when there are none
	FText FoldButton, FoldTip; // (Version 64) two corps of the army are worn thin and could be folded into one. Empty when there are none
	// (Brian, 18 November 2026: "troops should be able to garrison a city", and of an army beaten into its town, "im not
	// able to attack them again".) The town's orders for the army in hand, as plain buttons on its own tray: until
	// then they stood only under ORDERS on the province's card, and a right-click on the town.
	FText StormButton, StormTip; // "STORM PANAMA CITY": the army stands round that town and may assault it. Empty when it may not
	FString StormTown; // ("Storm Panama City": the same in the rules' own capitals, for the tray's line)
	bool bStormFavoured = false; // the staff's forecast favours the assault: the one case in which the button is lit
	FText GarrisonButton, GarrisonTip; // "GARRISON NEW YORK": corps in hand may go into the town of the province they stand in
	FText FieldButton; // "TAKE THE FIELD": corps in hand are inside a town and may leave it
	TArray<FString> GarrisonActions, FieldActions; // the province's own orders that do it, one for each corps
	FText PendingButton; // "MARCH TO LODZ": the place clicked once, waiting for the word. Empty when there is none
	FText Hint; // what to do next, in a line
	FText HintLead; // (ruling 87) and what is said before it, in brass: "No moves left this month."
	// (Release 2: splitting a stack.) How many of the army's corps, or of the fleet's ships, go with the next order, of
	// how many could: "3 OF 4 CORPS GOING". GoingOf 0 or 1: nothing to split, and no line is shown.
	int32 Going = 0;
	int32 GoingOf = 0;
	int32 Revision = 0; // goes up whenever the cards change, so the screen knows to redraw them
};

/** One choice on the opening screen. */
struct FOpeningSide
{
	FText Name;
	FText Blurb;
	FText Aims;
	FLinearColor Color = FLinearColor::Gray;
	FString Tag; // whose flag to show; empty to watch
	bool bWatch = false;
};

/** A day the grand campaign can be taken over on. */
struct FOpeningYear
{
	FText Year;
	FText Title;
	FText Note;
	int32 From = 1;
};

/** A country that can be led in the grand campaign. */
struct FOpeningNation
{
	FString Tag;
	FText Name;
	FText Blurb;
	FText Standing; // "At peace", or who it is at war with
	FText Strength; // "30 corps, 36 provinces"
	bool bMajor = false;
	bool bSmall = false; // a small country whose path is its own to choose
	TArray<FString> Abilities; // what the country does in its own way, a line each
};

struct FOpeningGroup
{
	FText Title;
	TArray<FOpeningSide> Sides;
};

/**
 * (Release 2) A battle of your own (Brian, 24 November 18:36Z): one land or air battle, his side against the computer's,
 * each army bought inside one treasury from the campaign's own price list (Rules/custom-bridge.ts). The page as the
 * opening screen's fourth door draws it.
 */
struct FCustomChip
{
	FString Id;
	FText Name;
	FText Tip;
	bool bOn = false;
	bool bLocked = false; // shown, not offered (the sea, with Tip saying why)
};

/** One line of the price list: a corps or wing, or the defence's fieldworks or guns ("works"), and how many each side has. */
struct FCustomBuyRow
{
	FString Kind;
	FText Name;
	FText Tip;
	int32 Cost = 0; // one more of it; for "works", the next level's price
	int32 Mine = 0;
	int32 Theirs = 0;
	int32 Most = 0; // works: the most levels (0: no limit but the treasury and the room)
	bool bDefenceOnly = false;
};

/** One side of the battle on the page. */
struct FCustomSideView
{
	FString Nat;
	FText Name;
	FLinearColor Color = FLinearColor::Gray;
	FText Kit; // the year its kit is from: "1942 to 1943"
	FText KitTip;
	FText Training; // "Seasoned"
	FText TrainingTip;
	int32 Spent = 0;
	int32 Budget = 0;
	FText Room; // "4 of the 5 corps the ground has room for"
	bool bComputerPicks = false; // its army is the computer's own pick, made again as the battle changes
	bool bAttacks = false;
};

struct FCustomPage
{
	bool bReady = false; // the rules' offer is in
	TArray<FCustomChip> Domains; // land, air, sea
	TArray<FCustomChip> Places; // the battlefields of the kind chosen, only those that passed the art director and the Mac
	TArray<FCustomChip> Sizes; // the treasury
	TArray<FCustomChip> Roles; // attack, defend
	TArray<FCustomChip> Kinds; // in the air: a bombing raid or a fight for the sky
	FText PlaceBlurb;
	FCustomSideView Mine;
	FCustomSideView Theirs;
	TArray<FCustomBuyRow> Buys;
	FText Problem; // why it cannot be fought as it stands (TC.customCheck), empty when it can
	bool bCanFight = false;
	FText Last; // how the last battle of his own ended, shown when he is back at the door
};

/** (Release 2) A battle of your own: what he has chosen so far, as the rules' spec is made from it (CustomSpec). */
struct FCustomState
{
	TSharedPtr<FJsonObject> Offer; // TC.customOffer, read once
	FString Domain = TEXT("land");
	FString Place, AirPlace;
	FString Size = TEXT("battle");
	FString Role = TEXT("att");
	FString Kind = TEXT("bomb");
	int32 Nat[2] = { 0, 1 }; // his and the computer's, as places in the offer's list of nations
	int32 Kit[2] = { 3, 3 };
	int32 Train[2] = { 2, 2 }; // 1 raw to 4 veteran
	TMap<FString, int32> Buys[2];
	int32 Works[2] = { 0, 0 };
	bool bTheyPick = true;
	bool bPicked = false; // his own army has been made at least once
	FText Last;
	bool bOpen = false; // a battle of his own is being fought
};

/** The opening screen: where a campaign can begin and as whom, and the campaign saved on this computer if there is one. */
struct FOpening
{
	bool bVisible = false;
	bool bCanReturn = false; // a campaign is under way behind the screen, so it can simply be closed

	// The grand campaign: any country, taken over on one of a few days, and free to make its own history after.
	int32 Tab = 0; // 0: the grand campaign, 1: the historical scenarios, 2: the options, 3: the saved wars (release 1.5.1), 4: a battle of your own (release 2)
	TArray<FOpeningYear> Years;
	int32 YearIndex = 0;
	TArray<FOpeningNation> Nations;
	int32 NationIndex = INDEX_NONE; // the country chosen, if one is
	FText GrandNote; // a word about who is missing from the list, if anyone is
	// How hard the war is to be: the levels, easiest first, and the one chosen.
	TArray<FGrandLevel> Levels;
	int32 LevelIndex = INDEX_NONE;
	// (rules version 66) How deep the campaign is to be played: the depths on offer and the one picked.
	TArray<FGrandLevel> Depths;
	int32 DepthIndex = INDEX_NONE;
	TArray<FGrandLevel> Histories; // (rules version 70) the history dial's five stops, and the one picked
	int32 HistoryIndex = INDEX_NONE;
	bool bWorking = false; // history is being played up to the day chosen; the list of countries follows
	FText Laying; // (tc.Speed.WarSpread) a war is being begun and laid under the opening's plate: the plate's words, else empty

	// The scenarios: single campaigns in which the other nations keep to history's plan.
	TArray<FOpeningGroup> Groups;
	FText SavedLine; // empty when there is nothing to continue
	// (Ruling 130.) Nothing replaces a saved campaign without first asking and naming what would be lost. The
	// beginning that waits on the player's answer: 0 none; 1 the grand campaign, led; 2 the grand campaign,
	// watched; 3 a scenario (its group and side); 4 FIRST COMMAND; 5 a saved war, loaded (AskLoad, release 1.5.1).
	int32 AskKind = 0;
	int32 AskGroup = 0, AskSide = 0;
	// (Release 1.5.1; Brian, 24 November 2026: "yes add save button so i can save multiple games".) The wars kept with
	// SAVE, the newest first (the page of saved wars, Tab 3); the one just saved (its row is marked); the one waiting on
	// ruling 130's question before it is loaded; the one whose DELETE asks on its row first; and the page's word on what
	// was done ("Saved: Germany, July 1940.") or what went wrong.
	TArray<FSavedWar> SavedWars;
	FString JustSaved, AskLoad, AskDelete;
	FText SavesNote;
	// (Release 2; the 1.5.1 pre-check: each SAVE is a whole copy.) What the saved wars take on the disk, and the room
	// left on it (below nothing where the computer does not say), for the page's line under its heading.
	int64 SavesBytes = 0, FreeBytes = -1;
	// (Rules 84.3.3; ruling 141) FIRST COMMAND's button on the title band: its words, and whether the ten are done.
	FLessonDoor Door;
	// (Release 2) A battle of your own, the fourth door.
	FCustomPage Custom;
	int32 Revision = 0;
};

/** One letter of a name lettered across the map, as the screen should draw it this frame. */
struct FMapLetter
{
	TCHAR Char = TEXT(' ');
	FVector2D At = FVector2D::ZeroVector; // the middle of the letter on the screen
	FVector2D Along = FVector2D::ZeroVector; // on the screen: the way the word runs, as long as a capital is high
	FVector2D Up = FVector2D::ZeroVector; // and the way the letter's top points, the same length on the map
	float Alpha = 1.0f;
	bool bSea = false;
	bool bInk = false; // (a sea's name on a sea the chart veils: in dark chart ink, or it is lost on the pale veil)
	FString Whole; // a province's name, written whole here instead of a single letter
	bool bShadowed = false; // (a province's name that lies over roofs: a soft dark shadow under it)
};

/** A town's name plate as the screen should draw it this frame. */
struct FTownPlate
{
	FVector2D At = FVector2D::ZeroVector; // on the screen, in its own points: the middle of the plate's top edge
	FString Name;
	FString Line; // empty from far off
	FLinearColor Colour = FLinearColor::Gray; // of whoever holds the place
	bool bCapital = false;
	bool bFar = false; // seen from far off: a small plate
	bool bAirfield = false;
	bool bHarbour = false; // ships in harbour: an anchor and how many
	bool bCrowd = false; // a crowded province folded up: how many more armies stand behind the one flag shown // not a town at all: the badge of an airfield with air groups based on it (Name is how many)
	bool bSeal = false; // not a town either: the seal on the town a directive of the war cabinet names (Name is the months left)
	bool bVow = false; // with bSeal: it is the player's own promise to the cabinet, in wax red, not the cabinet's directive in brass
	int32 Rank = 0;
	// (Rulings 46 and 57.) What the place has on order, for its badge: drawn directly above its name plate, or by
	// itself (the name left empty) where the plate is not shown at this height.
	int32 Region = INDEX_NONE;
	bool bOrders = false;
	FString Sign;
	bool bStar = false;
	bool bSpied = false; // (an agent's report of another country's city: a dashed frame)
	int32 OrderCount = 0;
	bool bPlus = false;
	int32 Months = 0;
	int32 Stage = 0; // 0 the whole badge, 1 without the count and the plus, 2 the sign alone
	// (Release 2, Brian's city at a glance.) The plate under the name: up to four marks, left to right, the garrison,
	// the order badge above (moved down into it), what is going up and what is damaged. Close up the whole plate;
	// from the middle distance the signs and the months alone; from far off four dots in fixed places.
	bool bGlance = false;
	int32 GlanceStage = 0; // 0 close, 1 middle, 2 far (the dots)
	bool bGlanceSpied = false; // an enemy's town as his intelligence gives it: a dashed brass frame and the report's age
	int32 GlanceAgo = -1;
	FString GarrisonFigure; // "3", "guard", or empty where the mark is not shown
	int32 UpMonths = -1; // the months until the next work is finished, -1 for no mark
	int32 Damage = 0; // percent wrecked, 0 for no mark
	bool bGlanceOrders = false; // units on order here (the order cell itself is the badge's fields above; this is its far dot)
	bool bPicked = false; // the selected city: the plate's lit edge
	// (Ruling 86.) Not a town: a sea gate's mark. Its holder's flag and a lamp, green or red; with Stage 0 its name
	// too, and with Stage 2 the lamp alone.
	bool bGate = false;
	bool bGateOpen = true;
	int32 Gate = INDEX_NONE;
	FString Flag;
};

/** (Release 2, the city at a glance; the art director's spec, section 3.) One line of the paper card over a town: a mark in words. */
struct FHoverGlanceLine
{
	FString Sign; // the UI kit's sign (defence, scaffold, wrench, hourglass) or an order badge's ("inf", "bmr")
	bool bOrderSign = false; // an order badge's sign, half as wide again as it is high
	FString Figure; // "3 div", "1 mo", "35%"
	FString Words; // "Third Army and the guard"
	bool bAlarm = false; // the damaged line, in the alarm's red
};

/** (Release 2) The paper card over the town under the pointer, where it wears a plate: its name, what it is, a line a mark. */
struct FHoverGlance
{
	bool bValid = false;
	FString Title; // the town's name
	FString Kind; // "your city", "enemy city · seen 2 mo ago"
	TArray<FHoverGlanceLine> Lines;
	FString Foot; // "click: open the city · right-click: send the army in hand here"
};

/** The bar of a town's building plots, shown when one of the player's provinces is picked and nothing is in hand. */
struct FTownBar
{
	int32 Region = INDEX_NONE;
	FString Tag; // whose town it is
	bool bMine = false;
	FText Title, Sub;
	TArray<FTownPlot> Plots;
	TArray<FTownOffer> Offers;
	FString Best; // (release 2, build from the map) the line the staff would put up here next, or empty
	FString Queued; // (release 2, a queue for a town) what is queued here, in a line, or empty
	int32 Picked = INDEX_NONE; // the plot whose choices are laid out above the bar
	int32 Revision = 0;
};

/** One of the game's standing choices, as the options page shows it. They are remembered from one day to the next. */
struct FGameOption
{
	FString Id;
	FText Name;
	FText Blurb;
	FText State; // what its button reads
	bool bOn = false; // lit
};

/** The question put to the player before he goes to war with a country he is at peace with. */
struct FWarPrompt
{
	bool bVisible = false;
	FString Tag; // the country war would be declared on
	FString Name; // its name as it reads in a sentence
	int32 Region = INDEX_NONE; // where the army in hand was being sent, to carry on there once war is declared
	FText Title;
	FText Body;
	FText Warning; // (rules version 73) set out in red under the body: before a landing, that the sea behind the beach could be closed
	// (rules version 74) Before a landing: the headline that speaks for the whole venture, how it is to be read
	// (0 plainly, 1 as good news, 2 in amber, 3 in red), and the line under it.
	FText Verdict, VerdictUnder;
	int32 VerdictTone = 0, UnderTone = 3;
	// (rules version 83) Before a landing: the test of strength, as the card's last note (0 plainly where the staff
	// would expect to hold, 2 in amber where it is an even thing, 3 in red where they would not); and the card's
	// closing line after it.
	FText Ashore, Last;
	int32 AshoreTone = 0;
	bool bFromOffice = false; // asked from the foreign office: go back there afterwards
	// The same question is put before a landing from the sea: then this is true, and the button says so.
	bool bLanding = false;
	FText Confirm = FText::FromString(TEXT("DECLARE WAR"));
	// (Rules 84.1; rulings 176(e) and 178(d).) The same card is the gate's question for its holder: the order to shut
	// it or open it again, with the answer that changes nothing in the other button's words.
	bool bGate = false, bGateShut = false;
	int32 Gate = INDEX_NONE;
	FText Cancel = FText::FromString(TEXT("NOT NOW"));
	// (The design director, 17 November 2026; Brian the same day: "if an enemy army is out of a city i couldnt attack
	// them".) The same card plans the march up to an enemy army that is not next door, and the attack, as one order:
	// the ways up as rows (the game's pick first), the one chosen, and the enemy's province.
	bool bApproach = false;
	// (The design director, 17 November.) And it asks before a garrison's sortie, which was fought at a press with no
	// way back: the odds stand on the card, and it can be called off.
	bool bSortie = false;
	/** True where the card is an order he asked for himself: Enter gives it, Esc calls it off, and the buttons wear their key caps. */
	bool IsOrder() const { return bApproach || bSortie; }
	/**
	 * (The UI director's 84.3 check, item 6: one rule for every card.) The question of war itself: its safe answer
	 * (NOT NOW) is lit and takes Enter and Esc, and the declaration takes a click. The landing and the gate keep their
	 * order lit, as before.
	 */
	bool IsWarQuestion() const { return !IsOrder() && !bLanding && !bGate; }
	int32 ApproachTo = INDEX_NONE;
	TArray<FApproachRow> Ways;
	int32 Way = 0;
	FText WaysHead;
	// (Rules 84.5; the UI director's sheet 34, "The road before the order".) The march-up card in the landing card's
	// parts: the kicker and date, the two sides with THE ROAD between them, the headline (bright where he can attack
	// this month, amber where the attack waits), the sentence under it, and the note over the rows.
	FString Kicker; // "MARCH UP AND ATTACK  ·  JANUARY 1941"
	FString OursTag, OursName, OursLine; // the hand: its flag, the name the tray gives it, and its numbers
	FString FoeName, FoeLine; // the enemy as far as the fog shows ("Round Pittsburgh"; its flag is Tag)
	FString From; // where the road starts
	FString Road; // "from New York to Susquehanna, beside the enemy"
	FText Headline; // the row's own sentence: "3 moves left on arrival: you can attack this month"
	bool bHeadAmber = false; // the attack waits for next month
	FText Under; // the staff's pick and what Enter does, in one or two sentences
	FText WaysNote; // "press a row to go another way", or "the only one in reach this month"
};

/**
 * (The UI director, check 19.) One corps on a side of the battle card: its name, and its figures ("infantry, 2
 * divisions · 14,599 men", after the battle "infantry, 2 divisions · lost 1,240, fell back"). The card sets the two on
 * one line where every corps of the side fits its column, and on two lines each where any does not.
 */
struct FRosterLine
{
	FString Name;
	FString Figures; // empty for a corps not identified
};

/** The two armies drawn up against each other before an attack and, once it is fought, what the battle came to. */
struct FFaceOff
{
	bool bVisible = false;
	bool bResolved = false;
	int32 Region = INDEX_NONE;
	/**
	 * (Release 1.5, Brian 23 November 05:18Z) Whether FIGHT IT YOURSELF is offered on this card, worked out once when it is
	 * first asked (WeighFieldOffer): -1 not yet, 0 no, 1 yes. Offered only where the battlefield's look has passed its Mac
	 * still, 45 fps and the art director (Content/Rules/fight-yourself-looks.txt) and the field can fight this attack;
	 * everywhere else the card offers SEND THEM IN only, with no button and no failed press.
	 */
	int32 FieldOffer = -1;
	/** The battlefield's look as the land battle will pick it: "bocage", "farmland+snow", "desert", ... */
	FString FieldLook;
	/** The corps FieldOffer was weighed for: a change of pick on the open card weighs it again. */
	TArray<int32> FieldPicks;
	FText Title;
	FText OurName;
	FText TheirName;
	FLinearColor OurColor = FLinearColor::Gray;
	FLinearColor TheirColor = FLinearColor::Gray;
	FString OurTag; // whose flags to show
	FString TheirTag;
	FText OurGeneral;
	FText TheirGeneral;
	FText OurStrength;
	FText TheirStrength;
	FText OurUnits; // the corps a line each, as plain text; where a roster is empty (a town's guard) the card shows this sentence instead
	FText TheirUnits;
	TArray<FRosterLine> OurRoster, TheirRoster; // (check 19) the corps as the card sets them
	float Balance = 0.5f; // the attackers' share of the strength on the field
	FText OddsLine;
	FText Prize; // what taking the place is worth
	FText Mass; // where more corps stand in reach than were picked: what sending everyone would come to
	TArray<int32> MassIds; // ...and the corps that would go
	FText Opened; // (Version 63) the staff has brought in more than were picked: said in a sentence
	bool bWidened = false; // more corps go in than the player picked: one button goes back to his pick
	FText Cost; // what the fight is likely to cost each side in men, if it carries and if it is thrown back
	// (Version 64) Soften the line first: a probe by one corps, or the bombers on the fieldworks.
	bool bSoften = false;
	FText SoftenHead, SoftenDone, ProbeLabel, ProbeText, BombText;
	bool bProbeOk = false, bBomb = false, bBombOk = false;
	int32 ProbeCorps = INDEX_NONE;
	FString BombOpen;
	TArray<FFaceLine> Face; // (Version 84) WHAT YOU FACE, before the order
	FString Paid, PaidSign; // (Version 84) who paid and to what, on the result
	FText Ticket;
	FText TicketKey; // the lines of the ticket that matter most: a long ticket opens folded to these
	int32 TicketLines = 0, TicketHidden = 0;
	FText ResultTitle;
	FText ResultDetail;
	// (Rules 84.1; rulings 175(c), 177, 178; the UI director's sheet 21.) The card before a march on a town held by
	// its guard alone: two forecasts and two orders. The first forecast is the card's own (the assault as the army
	// arrives); these are its heading and its sentence, and the second (the same assault made ready, a month later).
	bool bMarch = false;
	// (Rulings 182 and 183.) The same card for a town the army already stands round: SEND THEM IN and KEEP UP THE
	// SIEGE. bArrival: it opened by itself as the army arrived, and a card that opens by itself takes no Enter.
	bool bStorm = false, bArrival = false;
	// (The design director, 17 November 2026: no press fights a battle whose odds have not been shown.) The garrison's
	// sortie against the army round its town, on the attack card: SALLY OUT in SEND THEM IN's place.
	bool bSally = false;
	// (The UI director, 17 November.) The verdict reads "A long shot" or worse: SEND THEM IN is then a plain button
	// that takes a click only, and the light and Enter go to CALL IT OFF.
	bool bLongShot = false;
	bool bPending = false; // (tc.Speed.CardAside) the card stands and its forecast is still being worked out on the rules' thread
	// (The UI director, 17 November.) The card shows exactly what was picked. Where that pick cannot go (guns or
	// engineers alone, ruling 211(a)) this is the rules' sentence: it stands at the foot, there is no forecast, SEND
	// THEM IN is not lit and takes no key, and CALL IT OFF is lit with Enter and Esc.
	FText Alone;
	/** The light and Enter are on CALL IT OFF: the verdict is a long shot or worse, or the pick cannot go at all. */
	bool CallOffLit() const { return bLongShot || !Alone.IsEmpty(); }
	bool bFavourSiege = false; // which order the forecast favours: that button is the lit one
	FText NowLabel, NowSays, LaterLabel, LaterOdds, LaterSays;
	float LaterBalance = 0.5f;
};

/**
 * The player's hands on the campaign table: moving the view, pointing at
 * territories, picking up armies and giving them their orders.
 */
UCLASS()
class THEATERCOMMAND_API ACampaignPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACampaignPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	const FRegionPanelInfo& GetRegionPanel() const { return RegionPanel; }
	const FArmyTray& GetArmyTray() const { return Tray; }
	/**
	 * (Release 2: the map gets the screen; Brian, 22 November 2026.) True from the player's second month on: the frame
	 * folds the date card to a strip, the chief of staff's list to a line, and the army tray away while nothing is in
	 * hand. A campaign begun here keeps everything open through its first month, so that a new player sees it all; one
	 * picked up from a save is folded from the start, since its player has played it before.
	 */
	bool IsFrameFolded() const;
	/**
	 * (Release 2.) Whether the army tray stands on the screen: while an army or a fleet of his is in hand, and, once the
	 * frame folds, an army only looked at while its ground is still the one picked on the map.
	 */
	bool IsTrayShown() const;
	const FFaceOff& GetFaceOff() const { return FaceOff; }
	const FOpening& GetOpening() const { return Opening; }
	const FWarPrompt& GetWarPrompt() const { return WarPrompt; }

	/**
	 * Asks the player whether to declare war on the country that holds a territory. Returns false, asking
	 * nothing, if that country is not one he is at peace with and could go to war with this month.
	 */
	bool OfferWar(int32 RegionId, bool bMarching = true);
	/** The same question, about a country by its tag. Region is where the army in hand goes on to afterwards, or INDEX_NONE. */
	bool OfferWarOn(const FString& Tag, int32 Region);

	/** The same question about the country whose territory is selected, asked from the territory panel. */
	void OfferWarOnSelected();

	/** Declares the war that was asked about, and sends the army in hand on if it was on its way there. */
	void ConfirmWar();
	/** (Ruling 178(d).) The gate's question for its holder, opened by a click on the gate's plate. Anyone else is told that only the holder can give the order. */
	void OpenGateBox(int32 GateId);
	void CloseWarPrompt();

	/** A page shown over the table: a unit's service record or the roll of honour. Revision goes up when it changes. */
	const FDocumentPage& GetDocument() const { return Document; }
	int32 GetDocumentRevision() const { return DocumentRevision; }
	void OpenRecord(int32 CorpsId);
	void OpenRoll();
	void CloseDocument();

	/** Starts the scenario chosen on the opening screen. */
	void BeginCampaign(int32 GroupIndex, int32 SideIndex);

	/** The opening screen's pages: 0 the grand campaign, 1 the historical scenarios, 2 the options, 3 a battle of your own. */
	void ShowOpeningTab(int32 Tab);
	/**
	 * (Release 2) A press on the page of a battle of your own: "domain:land", "place:<id>", "size:<id>", "role:att",
	 * "kind:bomb", "nat:me:1" (the next nation; -1 the one before; "them" the computer's side), "kit:me:1", "train:them:-1",
	 * "buy:me:arm:1" (one more; -1 one fewer; "works" the fieldworks or the guns), "pick:me" (the computer chooses his army
	 * for him, once), "pick:them" (the computer chooses its own, or lets him choose it), "clear:me", "fight".
	 */
	void PressCustom(const FString& What);
	/** The options page: the game's standing choices, and a press on one of them. */
	TArray<FGameOption> GetOptions() const;
	void PressOption(const FString& Id);

	/**
	 * Chooses the day the grand campaign is taken over on, and works out who can be led from it. For a later
	 * year that means playing history up to it, which takes a few seconds: the screen says so first, unless
	 * bAtOnce asks for the answer before returning (scripts do).
	 */
	void PickGrandYear(int32 Index, bool bAtOnce = false);

	/** Chooses a country for the grand campaign, by its place in the list or by its tag. */
	void PickGrandNation(int32 Index);
	void PickGrandNation(const FString& Tag);

	/** Starts the grand campaign as the country chosen, or with nobody leading. */
	void BeginGrandCampaign(bool bWatch);

	/** Picks up the campaign saved on this computer. */
	void ContinueCampaign();
	/**
	 * (Release 1.5.1; Brian, 24 November 2026: "yes add save button so i can save multiple games".) SAVE on the menu
	 * over a war: a copy of it is kept as a saved war of its own, and the page of saved wars opens on it. LOAD (the
	 * page). A row's LOAD: where the campaign kept by itself would be written over by it and is not kept as a saved war
	 * too, ruling 130's question comes first, with a third answer that keeps it and then loads (AnswerLoadKeeping). A
	 * row's DELETE asks on the row first; KEEP IT is its lit answer, and Enter's and Esc's.
	 */
	void SaveWar();
	void ShowSavedWars();
	void AskToLoadWar(const FString& Id);
	void AnswerLoadKeeping();
	void AskToDeleteWar(const FString& Id);
	void AnswerDeleteWar(bool bDelete);

	/**
	 * (Ruling 130.) Begins a new war from the opening screen: at once where no campaign is saved on this computer,
	 * and otherwise only after the player has been asked, with the saved campaign named. Kind is 1 for the grand
	 * campaign led, 2 for it watched, 3 for a scenario, 4 for FIRST COMMAND.
	 */
	void AskToBegin(int32 Kind, int32 GroupIndex = 0, int32 SideIndex = 0);
	/** The answer to that question: begin the new war and let the saved one go, or keep it. */
	void AnswerBegin(bool bBegin);
	/** (The UI director's 84.3 check, item 6.) The door's BEGIN AS ... is up: Enter presses it, and it shows ENTER. */
	bool CanBeginByKey() const { return Opening.bVisible && Opening.AskKind == 0 && Opening.Tab == 0 && Opening.Years.Num() > 0 && Opening.Nations.IsValidIndex(Opening.NationIndex); }

	/** Saves the campaign and shows the opening screen over it. */
	void OpenMenu();

	/** Goes back from the opening screen to the campaign under way. */
	void CloseMenu();

	/** The month's news, shown for a while after the month turns. */
	FText GetDispatches() const;

	/** True while the other nations' months are being played out in front of the player. */
	bool IsWatching() const { return bWatching; }
	// ---- (release 2, the enemy's month near you; Brian, 24 November 2026)
	/** True while the watch is of the others' months near his ground only (the option "Enemy moves near you"). */
	bool IsWatchingNear() const { return bWatching && bWatchNear; }
	/** The heading over the watch's sentence while it is of the moves near him ("MOVES NEAR YOU · GERMANY · 1 OF 3"); empty otherwise. */
	FString GetWatchHeading() const;
	/** Whose month the watch is showing, for the flag beside the heading (his own, under READY FOR ORDERS). */
	const FString& GetWatchNation() const { return CanShowReady() ? ReadyNation : WatchNation; }
	/** (round 2, Brian 18:01Z) TAKE ME THERE: the watch ends at the stop on screen, the eye stays on it and his nearest army is put in his hand (G). */
	void TakeMeThere();
	bool CanTakeMeThere() const { return bWatching && bWatchNear && bTakeMeThere && TakeRegion != INDEX_NONE; }
	/**
	 * (Release 2, new units ready; Brian, 24 November 2026 18:30Z: "let me know when a unit has been delivered".) As his
	 * month opens, a brass READY FOR ORDERS plate names the units handed over to him; SHOW ME (or G) takes the eye to
	 * each in turn and puts a new army in his hand. The plate is said once nothing else holds the screen.
	 */
	void ShowReady();
	bool CanShowReady() const { return bReadyNotice && !bWatching && !IsFightingLive() && ReadyUnits.Num() > 0 && ReadySaid >= 0 && ReadySaid == NoticesSaid && FPlatformTime::Seconds() < NoticeUntil; }
	const TArray<FReadyUnit>& GetReadyUnits() const { return ReadyUnits; }
	/**
	 * (Release 2, tc.UI.SeaWaits; Brian's card, 25 November 2026: "waits behind a button".) In a month whose enemy's moves
	 * near him were shown, the month's scene at sea (AMonthSeaScene) does not follow the tour by itself: it waits behind
	 * THE MONTH AT SEA, a press away, until the month ends. Other months play it as before.
	 */
	static bool IsSeaWaitsStyle();
	bool HoldsMonthSea() const { return IsSeaWaitsStyle() && bNearShownThisMonth; }
	/** The scene at sea is held and waiting (set by AMonthSeaScene; false once it has played or its month has gone). */
	void OfferMonthSea(bool bOffer) { bSeaOffered = bOffer; if (!bOffer) { bSeaAsked = false; } }
	bool IsMonthSeaOffered() const { return bSeaOffered && !bWatching; }
	/** THE MONTH AT SEA pressed: the held scene plays once the table is free. */
	void AskMonthSea() { if (bSeaOffered) { bSeaAsked = true; } }
	bool TakeMonthSeaAsk() { const bool bWas = bSeaAsked; bSeaAsked = false; return bWas; }
	/** Says the READY plate now (the month's end does, once the screen is free; a picture asks for it at once). */
	void SayReady();
	/** G: TAKE ME THERE while the watch offers it, else SHOW ME while the READY plate is up. */
	void PressGo();
	// ---- (end release 2)
	ACampaignMapActor* GetMap() const { return Map.Get(); }
	FCampaignSession* GetSession() const;
	/** (release 1) What an arm is, for the bigger unit card; null where the rules give none. */
	const FArmNote* FindArm(const FString& Kind) const;

	FText GetDateText() const;
	FText GetRulesNote() const;
	/** A line of news for the player, shown for a few seconds. Empty when there is none. */
	FText GetNotice() const;
	/** (Brian's memorial card) The plate for the capital ships just lost, and how far it is shown (0 none, 1 whole: it fades in and out). */
	const FMemorialShow& GetMemorial() const { return Memorial; }
	float GetMemorialShown() const;
	/** Names the battleships and carriers among these sinkings on the plate; bHold keeps it up (for a picture). */
	void Remember(const TArray<FSinking>& Sinkings, bool bHold = false);

	/** Selects a territory, or none with INDEX_NONE. */
	void SelectRegion(int32 RegionId);

	/** Picks up the player's army in a region, if there is one that can be given orders. */
	bool PickUpArmyIn(int32 RegionId);
	void PutDownArmy();

	/** Leaves a corps of the picked-up army behind, or takes it along again. */
	void ToggleCorps(int32 CorpsId);

	/**
	 * (Release 2, breakup.ts; Brian 24 November 2026 18:02Z: "see each individual unit ... so I can break it up".) A
	 * division's tile on a corps' card picked to be split off, or put back; SPLIT OFF makes the picked divisions a corps
	 * of their own where the army stands (it stays behind when the army moves); REJOIN puts a part back.
	 */
	void ToggleDivision(int32 CorpsId, int32 Slot);
	bool IsDivisionPicked(int32 CorpsId, int32 Slot) const { const TSet<int32>* Set = PickedDivisions.Find(CorpsId); return Set && Set->Contains(Slot); }
	int32 DivisionsPicked(int32 CorpsId) const { const TSet<int32>* Set = PickedDivisions.Find(CorpsId); return Set ? Set->Num() : 0; }
	void SplitOffDivisions(int32 CorpsId);
	void RejoinCorps(int32 CorpsId, int32 IntoId);
	/** (Release 2, rearguard.ts) Orders one of his corps to hold as a rearguard until his next month, or stands it down. */
	void HoldAsRearguard(int32 CorpsId, bool bHold);

	/** The standing order for the army in hand on the day the enemy comes: "stand", "dig" or "back". */
	void SetStance(const FString& Posture);

	/** Switches between marching and going by rail for the army in hand. */
	void ToggleRail();

	/** Orders something in the selected territory: a kind of corps to raise, or "fort". */
	void ProvinceAction(const FString& Action);

	/** Switches the army in hand between marching and going by sea. */
	void ToggleSea();

	/** On the opening screen: chooses how hard the grand campaign is to be. */
	void PickLevel(int32 Index);
	/** (rules version 66) How deep the campaign is to be played, picked as it is set up. */
	void PickDepth(int32 Index);
	void PickHistory(int32 Index);
	/** The depth the campaign stands at and how many desks the staff runs, for a line under the date: empty outside the grand campaign. */
	FText GetDepthLine() const;
	/** The staff's room: the depths, the six desks and what the staff may spend. */
	void OpenStaff();
	/** One of the six desks, by its name in the rules ("research", "build", "diplomacy", "spies", "air", "navy"), or null. */
	const FViewDesk* FindDesk(const FString& Id) const;
	/** The button on a desk's bar: hands the desk to the staff, or takes it back, and reads the open screen again. */
	void PressDesk(const FString& Id);

	/** Shows a fleet or an air fleet in the panel, with the orders it can be given. */
	void ShowForce(bool bFleet, int32 Id);

	/** Takes back an order placed this month. */
	void CancelOrder(int32 OrderId);

	/** The treasury in a line, for the date plate. */
	FText GetTreasuryText() const;

	/** What comes in, goes out and is left each month, in a line under it; empty in the scenarios, where nothing goes out. */
	FText GetBudgetText() const;

	/** True when the treasury is in debt, so the plate can show it in red. */
	bool IsInDebt() const;

	/** The ledger: the player's books, shown over the table. Revision goes up when they change. */
	const FLedger& GetLedger() const { return Ledger; }
	bool IsLedgerOpen() const { return bLedgerOpen; }
	FString GetLedgerNationTag() const;
	void OpenLedger();
	void CloseLedger();

	/** GUIDE, or H: in a war he leads, the Guide's list of the ten lessons (rules 84.3.3); otherwise, or from that
	 * list, How to play, as a page over the table (the Guide's second page, ruling 147). The same key puts either away. */
	void OpenGuide();
	/** How to play: the typed page of rules. */
	void OpenHowToPlay();
	/** (Ruling 141) The door's FIRST COMMAND: the campaign the rules name for it (Germany, from September 1939, at Cadet
	 * and Field command), marked as FIRST COMMAND's. Through AskToBegin(4) where a campaign is saved. */
	void BeginFirstCommand();

	/** A room the rules set out themselves: the secret service, the last act. */
	const FOfficePage& GetPage() const { return Page; }
	bool IsPageOpen() const { return bPageOpen; }
	void OpenPage(const FString& Id);
	void ClosePage();
	void PressPage(const FString& Action);
	/** The rooms open to the player now, for the buttons that lead to them: ids and labels, in step. */
	void GetPageLinks(TArray<FString>& OutIds, TArray<FString>& OutLabels) const;
	/**
	 * (Release 2; Brian, 25 November 2026: "I want the UI and user experience to feel seamless and easy to manage the
	 * cities, recruitment, ect".) THE HOME FRONT: one board for every town, raising forces and everything on order,
	 * under tc.UI.HomeFront (off until the art director passes the Mac's stills; -TheaterHomeFront turns it on for a
	 * run). It is the rules' own room "home", laid out as a board. Off, no door leads to it and the game is as it was.
	 */
	static bool IsHomeFrontStyle();
	/** (Release 2, tc.UI.Ease) The rooms and the province panel ease in and out instead of appearing and vanishing at once. */
	static bool IsEaseStyle();
	void OpenHomeFront();
	bool IsHomeFrontOpen() const { return bPageOpen && Page.Home.bValid && IsHomeFrontStyle(); }
	/** Which towns the board lists: 0 those that can take a building, 1 those building, 2 every town. */
	int32 GetHomeFilter() const { return HomeFilter; }
	void SetHomeFilter(int32 Filter);
	/** The town whose other buildings are unfolded under its row, or none: a second press folds them away. */
	int32 GetHomeUnfolded() const { return HomeUnfolded; }
	void UnfoldHomeTown(int32 Region);
	/** A town built in from the board keeps its row while the board stands, so the list does not jump under the hand. */
	bool IsHomeKept(int32 Region) const { return HomeKept.Contains(Region); }
	void BuildFromHome(int32 Region, const FString& Act);
	/** How many towns the board lists at most (a great power has near two hundred), and the button that lists more. */
	int32 GetHomeRows() const { return HomeRows; }
	void ShowMoreHomeTowns();
	/** How many times the board has been opened: its lists start from their heads each time. */
	int32 GetHomeOpenings() const { return HomeOpenings; }
	/** A town's own plan from the board: its card on the map where the town card is shown, and otherwise its room. */
	void OpenTownFromHome(int32 Region);
	// (Release 2, news at the map's edge; ruling 131: merge by hand) The month's seals on the map's edge, and a press of
	// one: the month's card opened at its row (EdgeNews.cpp).
	const FEdgeNewsBoard& GetEdgeNews() const { return EdgeNews; }
	void OpenEdgeSeal(int32 Seal);

	/** The generals: who commands what, who is waiting, and the corps that have nobody. */
	const FGeneralStaff& GetGenerals() const { return Staff; }
	bool IsGeneralsOpen() const { return bGeneralsOpen; }
	void OpenGenerals();
	void CloseGenerals();
	void AssignGeneral(int32 GeneralId, int32 CorpsId);
	void PromoteOfficer(int32 CorpsId);
	void RelieveGeneral(int32 GeneralId);
	/** Dismisses a general for good: asked twice, since it cannot be undone. Says which general is being asked about. */
	void SackGeneral(int32 GeneralId);
	int32 GetSackAsked() const { return SackAsked; }
	/** The army in hand takes drafts, or keeps its veterans together and takes none. */
	void ToggleDrafts();
	/** (Release 2: the battle schools.) A corps card's SEND TO SCHOOL, or its BRING BACK while the corps is there. */
	void SchoolCorps(int32 CorpsId);
	/** On the eve of war: the army is placed as the staff advises. */
	void PlaceByPlan();
	bool HasEvePlan() const;
	/** The honours won, and what can be made of each at home. */
	void OpenHeroes();
	void SpendSkill(int32 GeneralId, const FString& Skill);

	/** The design offices: the teams at work on each branch of the armed forces. */
	const FResearch& GetResearch() const { return Research; }
	bool IsResearchOpen() const { return bResearchOpen; }
	void OpenResearch();
	void CloseResearch();
	void SetResearchTeams(const FString& Field, int32 Teams);
	/** (For the photographer.) On the research chart: the card chosen, and the card the pointer rests on. */
	FString StagedChartChoice, StagedChartPoint;
	void StartTech(const FString& Id);
	void StopTech(const FString& Id);
	/** Research teams of the player's with nothing to do, for the badge on the button. */
	int32 GetIdleTeams() const;
	/** (Release 2, the spies thread of 24 November 2026.) The number on the SPIES door: enemy agents uncovered and at large, and his agents newly at work this month. */
	int32 GetSpiesWaiting() const;
	/** What is still worth doing before the month is ended: the line at this place in the list, or empty. And a press on it. */
	FString GetToDoText(int32 Index) const;
	FString GetToDoTip(int32 Index) const;
	void PressToDo(int32 Index);
	/** (Rules 84.2.6; ruling 211(c).) The alarm that names the cure: the row shown first in the tray, or none; how many
	 * there are and which of them is shown (from 1), for its "1 of 3"; and a press on it, which puts the nearest help in
	 * hand, centres the town and goes on to the next. */
	const FHomeAlarm* GetAlarm() const;
	int32 GetAlarmCount() const;
	int32 GetAlarmPlace() const;
	void PressAlarm();
	/** (Rules 84.2.6.) His own guns left alone round an enemy town: the row at this place, or none; and a press, which centres the town. */
	const FGunsAlone* GetGunsAlone(int32 Index) const;
	void PressGunsAlone(int32 Index);
	/** A line about the province under the pointer, for a tip that follows the mouse; empty when a screen is open. */
	FText GetHoverTip() const { return HoverTip; }
	/**
	 * (Rulings 86 and 87.) What the box says under that, where some of it is marked in a colour of its own: "<Brass>No
	 * moves left this month.</>" and "<Red>Shut to your ships:</>". The marks are the box's own; nothing else is read as one.
	 */
	FText GetHoverTipMarked() const { return HoverTipMarked; }
	/** (Release 2) The paper card of the city at a glance over the town under the pointer; not valid where there is none. */
	const FHoverGlance& GetHoverGlance() const { return HoverGlance; }
	/**
	 * (Release 2; the art director's airfield spec of 24 November, item 2.) The four corners of the airfield a click
	 * would open, in the window's own pixels, for its brass outline; empty where there is none.
	 */
	void GetFieldOutline(TArray<FVector2D>& Out) const;
	/** (The same spec, item 3.) The province panel's airfield row: opens the picked province's airfield. */
	void OpenSelectedAirfield();
	/** A country's name as it is said in a sentence: "the United States", "the Soviet Union", "the Netherlands"; "Germany". */
	static FString WithThe(const FString& Name) { return (Name.StartsWith(TEXT("United")) || Name.StartsWith(TEXT("Soviet")) || Name.StartsWith(TEXT("Netherlands"))) ? TEXT("the ") + Name : Name; }
	/** The same at the head of a sentence: "The United States has fallen." (84.2.6) */
	static FString HeadThe(const FString& Name) { const FString Said = WithThe(Name); return Said.Left(1).ToUpper() + Said.Mid(1); }
	/** A line made safe to stand inside marked text. */
	static FString PlainInMarked(const FString& Text) { return Text.Replace(TEXT("&"), TEXT("&amp;")).Replace(TEXT("<"), TEXT("&lt;")).Replace(TEXT(">"), TEXT("&gt;")); }
	/**
	 * (Ruling 87.) The slip on the top edge of the army's card: shown whenever an army with no moves left is in hand,
	 * until GOT IT is pressed or one march has been kept for such an army. After that, never.
	 */
	bool IsMarchSlipShown() const;
	void DismissMarchSlip();
	bool IsHandSpent() const { return bHandSpent; }
	/** For the photographer: the question at the month's end, as it would be asked now. */
	void ShowEndAsk();
	/** Where the pointer is, in the window's own pixels. */
	FVector2D GetPointer() const { return (StagedHover != INDEX_NONE || StagedGate != INDEX_NONE || StagedSea != INDEX_NONE) ? StagedPointer : LastMouse; }
	// For the photographer, who has no mouse: the tip as it would read with the pointer resting on this region.
	void StageHover(int32 RegionId, bool bOnItsAirfield = false) { StagedHover = RegionId; bStagedField = bOnItsAirfield; }
	/** (Ruling 86.) For the photographer: the pointer resting on the mark of the gate whose name begins so. */
	void StageGate(const FString& NameBegins);
	/** For the photographer: with a fleet in hand, the pointer resting on the sea whose name begins so. */
	void StageSea(const FString& NameBegins);
	int32 StagedSea = INDEX_NONE;
	/** (Ruling 86.) Where the gates' marks were last drawn, in the window's own pixels: told by the screen as it paints them. */
	void NoteGateMarks(const TArray<TPair<int32, FBox2D>>& Marks, float PixelsToAUnit = 1.0f) { GateMarks = Marks; GateMarkScale = FMath::Max(PixelsToAUnit, 0.01f); }
	/**
	 * (The UI director's HUD read, 22 November: mend 1.) Where the airfields' badges were last drawn, each with its
	 * airfield's province, in the window's own pixels: told by the screen as it paints them. And the province whose
	 * badge is under the pointer, or none: the pointer on a badge points at its airfield.
	 */
	void NoteFieldBadges(const TArray<TPair<int32, FBox2D>>& Badges) { FieldBadges = Badges; }
	/**
	 * (The UI director's read of run 7, 22 November: mend 3.) Where the towns' plates were last drawn, each with its
	 * province, in the screen's own units: told by the screen as it paints them. And the plate of the town whose paper
	 * card is on the pointer, for the card to stand clear of; false where no card shows or its plate is not drawn.
	 */
	void NoteTownPlateBoxes(const TArray<TPair<int32, FBox2D>>& Boxes) { TownPlateBoxes = Boxes; }
	bool GetGlancePlate(FBox2D& Out) const
	{
		if (!HoverGlance.bValid || HoverTipRegion == INDEX_NONE)
		{
			return false;
		}
		for (const TPair<int32, FBox2D>& Each : TownPlateBoxes)
		{
			if (Each.Key == HoverTipRegion)
			{
				Out = Each.Value;
				return true;
			}
		}
		return false;
	}
	int32 FieldBadgeUnder(const FVector2D& Pointer) const
	{
		for (const TPair<int32, FBox2D>& Badge : FieldBadges)
		{
			if (Badge.Value.IsInside(Pointer))
			{
				return Badge.Key;
			}
		}
		return INDEX_NONE;
	}
	/**
	 * Where the pointer's box should stand clear of: the mark of the gate it explains, in the screen's own units (the
	 * units the mark was painted in, whatever the window's pixels are). False when it explains no mark.
	 */
	bool GetTipAnchor(FBox2D& Out) const
	{
		for (const TPair<int32, FBox2D>& Mark : GateMarks)
		{
			if (HoverGate != INDEX_NONE && Mark.Key == HoverGate)
			{
				Out = FBox2D(Mark.Value.Min / GateMarkScale, Mark.Value.Max / GateMarkScale);
				return true;
			}
		}
		return false;
	}
	float GateMarkScale = 1.0f;
	/** Shows the model of one of the corps in hand large and turning beside the cards. */
	void ShowCorpsModel(int32 CorpsId);
	FText GetShowTitle() const { return ShowTitle; }
	/** The great moment being told now, or null: a nation eliminated or liberated, on a screen of its own. */
	const FAnnouncement* GetAnnouncement() const { return Announcements.Num() > 0 ? &Announcements[0] : nullptr; }
	void DismissAnnouncement();
	/** Tells a great moment on a screen of its own, after any already waiting. */
	void Announce(const FAnnouncement& Told) { Announcements.Add(Told); }
	/** Which part of the province panel is open: 0 the province, 1 what can be raised there, 2 the works. */
	int32 GetProvinceTab() const { return ProvinceTab; }
	void SetProvinceTab(int32 Tab) { ProvinceTab = Tab; RegionPanel.Revision = ++PanelRevision; }
	/** Opens the town plan of the province picked on the map: its plots and what could be built on them. */
	void OpenTownPlan();
	/** The town's plots as a bar of pictures; whether it is to be shown; a click on a plot; and an order from its choices. */
	const FTownBar& GetTownBar() const { return TownBar; }
	bool IsTownBarShown() const;
	/**
	 * (Release 2; Brian, 24 November 2026: "lets improve these graphics and UI. its very underwhelming".) The new
	 * province panel and the town card, under tc.UI.TownCard (off until the art director passes the Mac stills;
	 * -TheaterTownCard turns it on for a run). With it on, the town's plots are a card the player opens from the
	 * province panel, not a bar that stands whenever the town is picked; Esc or its close puts it away.
	 */
	static bool IsTownCardStyle();
	void OpenTownCard();
	void CloseTownCard();
	bool IsTownCardOpen() const { return bTownCardOpen && IsTownBarShown(); }
	/** Where the armies' colours and their strength plates are on the screen, in its own pixels: the towns' name plates step aside from them. */
	void GetColoursOnScreen(TArray<FBox2D>& Out) const;
	/** Whether the reminder of the keys stands open: through a new player's first month, while the ? key is held, or kept open by a click on its mark. */
	bool IsHelpShown() const;
	/** A click on the mark: keep the reminder open, or fold it away (and with it the first month's showing). */
	void ToggleHelp();
	void PickTownPlot(int32 Index);
	void OrderTownBuilding(const FString& Line);
	/** (Release 2, a queue for a town) Gives one of the town's own acts for the town in view ("queue:<region>:<line>"), and says what came of it. */
	void ActOnTown(const FString& Act);
	/** The small map in the corner: its picture, the marks on it, and a jump to a place on it. */
	UTexture2D* GetMinimapTexture() const;
	/** Armies as (x, y, whose) with x and y from 0 to 1 and whose 0 for the player's, 1 for a friend's, 2 for an enemy's, 3 for others; and the part of the map on screen. */
	void GetMinimapMarks(TArray<FVector>& OutDots, FBox2D& OutLooking) const;
	/** Whether the small map shows the whole world, which it does wherever the world is attached. */
	bool IsMinimapWorld() const;
	/** Where the eye looks, as a share of the way round the whole world's small map (0 at its west edge, 1 at its east). */
	double GetMinimapCentre() const;
	/** (Release 2, round 2: corner pings) While the watch near him runs, its stops on the small map; none otherwise or with the option off. */
	void GetWatchPings(TArray<FWatchPing>& OutPings) const;
	/** The name plates of the towns in view, the ones that matter most first. */
	void GetTownPlates(TArray<FTownPlate>& Out) const;
	/** The names of the countries and the seas, letter by letter, lying on the map. */
	void GetMapLetters(TArray<FMapLetter>& Out) const;
	/** How far into a moonlit night the table is, 0 to 1: the lettering on the map dims with it. */
	float GetMoonlight() const;
	/** How far the table has gone over to the staff's paper map (the top of the zoom): 0 to 1. Names are lettered on it in ink. */
	float GetPaper() const;
	/** True where whoever stands a month digs in unasked (the grand campaign): the tray then offers stand, or fall back. */
	bool IsAutoDig() const;
	/** The reserve of men, where the rules keep one (the grand campaign); null otherwise. */
	const FViewStock* GetMen() const;
	void JumpToMapShare(const FVector2D& Share);
	bool IsMapShown() const;
	/** A fleet in hand: picked up like an army, sent to a sea with a right-click, its ships shown as cards. */
	bool PickUpFleet(int32 FleetId);
	void PutDownFleet();
	bool HasFleetInHand() const { return FleetInHand != INDEX_NONE; }
	/** What the fleet in hand does where it is sent: "patrol" or "raid". Changes what it is doing now if it is at sea. */
	void SetFleetStance(const FString& Stance);
	void FleetToHarbour();
	const FFleetHand& GetFleetHand() const { return FleetHand; }
	/** Opens the room with everything the fleet in hand could be told to do: seas, lanes to escort, coasts to shell, splitting and joining. */
	void OpenFleetOrders();
	/** The room of the aircraft the fleet in hand carries: search a sea, strike a fleet, raid a harbour. */
	void OpenFleetWings();
	/** Moves the fleet in hand to a new home port. */
	void FleetToNewBase(int32 RegionId);
	/** Sends the fleet in hand to a sea, to do there what it has been told (patrol or raid). */
	void OrderFleetTo(int32 Zone);
	/**
	 * (Release 2, fleetattack.ts) The fleet in hand set down on an enemy fleet: the attack card, with the odds first, as
	 * an army's face-off. False where fleet attacks are off, and the caller does what it did before.
	 */
	bool AttackFleetWith(int32 Target);
	/** (Release 2: splitting a stack.) A ship's card in the fleet tray clicked: it is left behind, or comes along again. */
	void ToggleShip(int32 Index);
	/** (Release 2: splitting a stack.) ALL, NONE or HALF of the army's corps, or of the fleet's ships, go with the next order: 1, 0 or 2. */
	void PickGoing(int32 Mode);
	/**
	 * (Release 2: splitting a stack.) The ships left behind are split off from the fleet in hand as a squadron of their
	 * own, where it is and under its order, so that only the lit ships go. True where the order may go ahead (nothing
	 * was left behind, or the split was made); false where every ship was left behind or the split was refused.
	 */
	bool SplitOffStaying(const FString& ForCard = FString());
	/**
	 * (Release 2; the pre-check's read of 24 November) A split made for a card (the attack's odds, the shelling's
	 * question) is taken back if the card is closed without its order: the ships left behind rejoin the fleet.
	 */
	void RejoinHeldBack();
	void ShowShipModel(const FString& Model, const FText& Name);
	/** Picks up the next of the player's armies that can still move this month, and goes to it. */
	void NextArmy();
	/**
	 * (Release 2, tc.UI.NextAll; the UI director's seamless plan, fix 4: one NEXT.) Once every army has its orders, Tab
	 * goes on round the month's list, the rows under END THE MONTH, opening each one's screen in turn, each once, and
	 * then says the month is done. The list never holds the month, as the rear never does. Off: Tab as before.
	 * -TheaterNextAll turns it on for a run.
	 */
	static bool IsNextAllStyle();
	/** (tc.UI.NextAll) The row of the month's list Tab opens next, or INDEX_NONE where it has been round them all or there are none. */
	int32 NextToDoIndex() const;
	/** How many of the player's armies still wait for an order this month: the count on the NEXT ARMY button. */
	int32 ArmiesYetToMove() const;
	/**
	 * (Ruling 101.) How many armies at the front have not been touched this month and have no order: the count on
	 * NEXT ARMY, and what keeps it bright and END THE MONTH dark. The rest that can still move (ArmiesYetToMove less
	 * this) are the quieter number: Tab still goes to them after the front, and they never hold the month.
	 */
	int32 ArmiesIdleAtFront() const;
	/** (Ruling 101.) The quieter number: untouched armies in the rear. Tab goes to them after the front, and they never hold the month. */
	int32 ArmiesInRear() const;
	/** True when the armies on the disc are all newly raised and none stands at the front: the line under NEXT ARMY then says so. */
	bool IsRoundNewlyRaised() const;
	/** (Rulings 111(f) and 116.) The bright count in the rules' own words, and the question's headline; empty where the rules give none. */
	FString GetRoundWords() const;
	FString GetRoundQuestion() const;
	/** True where the disc counts both armies at the front and armies newly raised. */
	bool IsRoundMixed() const;
	/** (For the photographer only.) Cuts a province's railway through the rules and lays the table out again. */
	void StageCut(int32 RegionId, const FString& Cause, const FString& How);
	/** (For the photographer only.) Gives a province anti-aircraft guns, or flies a reconnaissance flight over it, and lays the table out again. */
	void StageGuns(int32 RegionId, int32 Level);
	FString StageRecon(int32 RegionId);
	/** (For the photographer only.) Sets an agent of his at work in a province ("road": still on his way; or a mission's id), or marks one caught there, and lays the table out again. */
	void StageAgent(int32 RegionId, const FString& Mission);
	void StageCaught(int32 RegionId);

	// ---- (Rulings 153, 163 and 169.) The agent in hand: picked up like an army, sent with a right-click or by dragging him.
	bool HasAgentInHand() const { return AgentInHand != INDEX_NONE; }
	/** The agent in hand as the rules describe him, for his card; null when none is. */
	const FViewAgent* GetAgentInHand() const;
	/** The foot of his card: how he is sent, or the rules' own reason why he cannot be just now. */
	FString GetAgentHint() const;
	/** Where the agent in hand stands on the screen, as a box round him with six pixels to spare: the pointer's box hangs clear of it. False with none in hand. */
	bool GetAgentInHandBox(FBox2D& OutBox) const;
	bool PickUpAgent(int32 AgentId);
	/** Takes in hand the agent standing in a province (the photographer's way in). */
	bool PickUpAgentIn(int32 RegionId);
	void PutDownAgent();
	/** Sends the agent in hand to a province: a month on the road. Where the rules refuse, their reason is said and he stays in hand. */
	void SendAgentTo(int32 RegionId);
	/** Gives the agent in hand his work where he stands, by the rules' own order. */
	void SetAgentWork(const FString& Act);
	/** The army in hand is told to stand this month, or the order is taken back: it leaves the round until the next month. */
	void StandThisMonth();
	bool IsStandingThisMonth() const;
	/** The question at the month's end, put once a month when armies at the front have had no orders. */
	struct FEndAsk
	{
		bool bVisible = false;
		int32 Count = 0;
		int32 Raised = 0; // how many of them are armies raised this month, not at the front
		double Since = 0.0; // (it takes no key for its first half second)
	};
	const FEndAsk& GetEndAsk() const { return EndAsk; }
	void AnswerEndAsk(bool bShowMe);
	bool IsEndAskWanted() const { return bAskAtMonthsEnd; }
	void ToggleEndAskWanted();
	/**
	 * (Release 2, the scramble offer; Brian, 24 November 2026 22:21Z: "i want every air battle playable".) The month
	 * stopped where an enemy raid on his ground meets his fighters: the card that offers SCRAMBLE (he flies his fighters
	 * against it in the battle in the air) or LEAVE IT TO THE STAFF, with the tick "Settle the rest of this month's raids".
	 * Behind the Battle rules card's "Scramble your fighters" (AAirBattle::DefendOn, off by default). While it or its
	 * battle is up, no order and no END THE MONTH, as while his own raid is open.
	 */
	struct FScrambleCard
	{
		bool bVisible = false;
		FScrambleOffer Offer;
		bool bRest = false; // the tick: the rest of this month's raids are settled by his staff
		double Since = 0.0; // (it takes no press in its first half second: a doubled END THE MONTH never answers it)
	};
	const FScrambleCard& GetScramble() const { return Scramble; }
	/** SCRAMBLE (bFly) or LEAVE IT TO THE STAFF. */
	void AnswerScramble(bool bFly);
	void ToggleScrambleRest();
	/** The card's title ("A German raid on Kent") and its words: who is coming, about how many, what he has to meet them. */
	FString ScrambleTitle() const;
	FString ScrambleBody() const;
	/** The list of everything the player commands, each with a way to go to it (L). */
	void OpenForces();
	/** The reserve of men as a sum, and the player's say over the call-up (version 62). */
	void OpenMen() { OpenPage(TEXT("men")); }
	/** Looks at an army that is not in hand (an enemy's, an ally's, or one of the player's that cannot move): its corps are dealt as cards, as far as they are known. */
	void LookAtArmy(int32 RegionId, const FString& Nation);
	void StopLooking();
	/** Joint armies: asks the allied corps beside the army in hand to march with it; sends one of them home. */
	void AskAlliesToJoin();
	/** A town order from the army's own tray: 0 storms the town it besieges (its card first), 1 sends the corps in hand into the town, 2 brings them out. */
	void TrayTownOrder(int32 Which);
	// (the score: whose wars were counted last, how many, and in which month, to hear one of them end)
	FString StormFavourKey; // (the forecast is asked once for a hand, a place and a month)
	bool bStormFavourKept = false;
	FString MusicWarsOf;
	int32 MusicWars = 0, MusicWarsTurn = 0;
	void SendBackCorps(int32 CorpsId);
	/** The nation the player leads; empty when nobody is led. */
	FString GetPlayerTag() const;
	/** Says in the notice line why something pressed cannot be done. */
	void SayWhy(const FString& Why);
	/** R2 bargains hook: says a note in the notice line with no sound of its own (the caller has played the order's one). */
	void SayNote(const FString& Note) { Notify(Note, 6.0); }
	/** The weather in the east this month, for the date plate; empty in clear weather. */
	FText GetWeatherText() const;
	// ---- (Rules 84.3.3) FIRST COMMAND: the staff's notes ------------------------------------------------------
	/** The note on the desk as the screen shows it (held while its DONE stamp stands), if one is and the screen is free for it. */
	bool HasLessonNote() const;
	const FLessonNote& GetLessonNote() const { return LessonNote; }
	/** A key for the note as it came: a new key is a new arrival, which slides in and chooses its home. */
	int32 GetLessonArrival() const { return LessonArrival; }
	/** Seconds since the note came (it slides in over 220 ms), and since its DONE stamp landed (negative while none stands). */
	double GetLessonAge() const;
	double GetLessonStampAge() const;
	/** Whether the note's DONE stamp stands on it now (2 s at most, or until a click sends it). */
	bool IsLessonStamped() const { return LessonStampUntil > 0.0 && LessonStampAt >= 0.0; }
	/** The note just gone, fading over 110 ms: its words, and how far it has faded (0 to 1). False once it has gone. */
	bool GetLessonLeaving(FLessonNote& Out, float& Faded) const;
	/** The note leaving was stamped DONE, and leaves with its stamp. */
	bool IsLessonGoneStamped() const { return bLessonGoneStamped; }
	/** (The art director's brief, section 10.) Lesson 10 is laid across the open arsenal, and its strip keeps its room until the room closes. */
	bool IsArsenalStrip() const { return bArsenalStrip && bArsenalOpen; }
	/** The screen found no room for the note this frame (a room with no margin): it waits unseen, and a stamp owed waits with it. */
	void SetLessonNoteHidden(bool bHidden) { bLessonNoteHidden = bHidden; }
	/** Lesson 1, laid across the month card that is open now. */
	bool IsLessonLaid() const { return bLessonLaid && bPageOpen && Page.Month.bValid; }
	const FLessonNote& GetLaidLesson() const { return LessonLaid; }
	/** The notes put away with LATER, waiting in the GUIDE (its count). */
	int32 GetLessonsWaiting() const { return Lessons.Waiting; }
	/** (Rules 84.3.3) The Guide's first page, the ten lessons, is open, and what it lists. */
	bool IsLessonGuideOpen() const { return bLessonGuideOpen; }
	const FLessonGuide& GetLessonGuide() const { return LessonGuidePage; }
	int32 GetLessonGuideRevision() const { return LessonGuideRevision; }
	/** A press on the Guide's list: "back:<id>" (a row waiting for him: its note comes back, and the Guide closes on
	 * it), "again:<id>" (SHOW ME AGAIN), "on" or "off" (the setting). */
	void PressGuide(const FString& Action);
	void CloseLessonGuide();
	/** The note's own words to press: "later", "close" (the closing note), "tip" (a tip put away), "send" (a stamped note sent now). */
	void PressLesson(const FString& Action);
	/** (Ruling 147(a).) What the note at its home must never cover, in pixels of the screen: the army in hand, and the flag it points at. */
	void GetLessonKeepClear(TArray<FBox2D>& Out) const;
	/** (The brief, section 3.) What the note's pointer frames now, worked out once a frame: a button or a card's line by its
	 * name, or a place on the map, which the pointer frames on the ground under the pieces. */
	const FLessonAim& GetLessonAim() const { return LessonAim; }
	/** Where the note stands on the screen, in the viewport's pixels: a place pointed at keeps out from under it. */
	void SetLessonNoteOnScreen(const FBox2D& Box) { LessonNoteOnScreen = Box; }
	/** Whether a veil (a card or a room over the map) is up, which the note stands beside as a slip. */
	bool IsLessonBesideCard() const { return bPageOpen || Document.bValid || bArsenalOpen || bLedgerOpen || bGeneralsOpen || bResearchOpen || bDiplomacyOpen || FaceOff.bVisible; }

	/** The adviser's first steps, and the way to send him away. */
	const FAdviser& GetAdviser() const { return Adviser; }
	void DismissAdviser() { Adviser.bOn = false; ++Adviser.Revision; }
	void ShowAdviser() { Adviser.bOn = true; ++Adviser.Revision; }
	/** The newsreel: the month's great stories flown over in black and white, each read out. Space skips it; K turns newsreels off or on. */
	const FNewsreelShow& GetNewsreel() const { return Newsreel; }
	void PlayNewsreel(const FString& Date, const TArray<FNewsCard>& Cards, int32 Kind = 0);
	/** The opening flight: the situation as the campaign begins, told stop by stop over the map. Space skips it. */
	void PlayBriefing();
	/** The title card that opens a year. A click or Space goes on. */
	void PlayYearCard(const FYearCard& Year);
	const FYearCard& GetLastYear() const { return LastYear; }
	FText GetArmySaid() const { return FPlatformTime::Seconds() < ArmySaidUntil ? ArmySaid : FText::GetEmpty(); }
	void NextNewsCard();
	void EndNewsreel();
	void ToggleNewsreels();
	/** (Ruling 203.) The month's newsreel, played from the month's card: the card comes back when it is over. */
	bool HasLastReel() const { return LastReel.Num() > 0 && !Newsreel.bOn; }
	void PlayLastReel();
	/** Day and night in the room: running, held at day, or held at night (T). */
	void ToggleDayNight();
	/** Begins a moment over a province: the eye goes down to it, stays about three seconds and is handed back. Any key or click ends it sooner. */
	void StartMoment(int32 RegionId);
	void EndMoment();
	/** The F key: puts the frame away for a clean picture of the table, or brings it back. */
	void ToggleFrame();
	bool IsFrameHidden() const;
	bool IsFrameHintShown() const { return FPlatformTime::Seconds() < FrameHintUntil; }
	/** The wealth view of the map: provinces tinted by what they pay (Y). */
	void ToggleWealth();
	bool IsWealthShown() const;
	/** What winning means: the campaign's objectives, each thing they ask for ticked or not (O). */
	void OpenGoals();
	/** What every nation did last month, each with a way to go and look (R). */
	void OpenReports();
	/** Looks at the player's capital (Home, or C). */
	void GoToCapital();
	/** While the month is replayed: the nations whose moves are shown, in order, and which is on the table now. */
	void GetWatchOrder(TArray<FString>& OutTags, int32& OutNow) const;
	/** Whether every nation's move is watched at the end of a month, or only what touches the player. */
	void ToggleWatchAll();
	/** The player's oil or supplies, or null where his nation keeps no stocks (a small country, a scenario, nobody led). */
	const FViewStock* GetStock(bool bOil) const;
	void FoundResearchBureau();

	/** The foreign office: the powers at war, the countries at peace, courting them and declaring war. */
	const FDiplomacy& GetDiplomacy() const { return Diplomacy; }
	bool IsDiplomacyOpen() const { return bDiplomacyOpen; }
	void OpenDiplomacy();
	void CloseDiplomacy();
	void CourtNation(const FString& Tag);
	/** A small country's path. The first press asks; the second, on the same path, takes it. Founding the League names the partner. */
	void PressPath(const FString& Id, const FString& Partner = FString());
	const FString& GetPathAsked() const { return PathAsked; }
	void InviteToLeague(const FString& Tag);
	/** Asked from the foreign office: the question about declaring war on a country, with no army on its way. */
	void AskWarOn(const FString& Tag);

	/** The arsenal: everything that can be ordered, with the territory that is selected in mind. */
	const FArsenal& GetArsenal() const { return Arsenal; }
	bool IsArsenalOpen() const { return bArsenalOpen; }
	void OpenArsenal();
	void CloseArsenal();
	void OrderFromArsenal(const FString& Key);
	/** (Release 2, the city at a glance) Orders from the arsenal in the town the panel shows: CAN BUILD HERE and ADD GUNS. */
	void OrderHere(const FString& Key);

	/** Closes the ledger and looks at one of its provinces on the table. */
	void ShowProvince(int32 RegionId);

	/** Shows the road the picked-up army would take to a region. */
	void ShowRouteTo(int32 RegionId);

	/**
	 * Orders the picked-up army to a region. CarriedFrom is where the player's hand set the piece down, when it
	 * was dragged there: the piece then settles straight onto its place.
	 */
	void OrderTo(int32 RegionId, const FVector* CarriedFrom = nullptr);

	/** A click on a territory, as the mouse makes it: for scripts. */
	void ClickRegion(int32 RegionId) { PointedField = INDEX_NONE; ClickOnTable(nullptr, RegionId); } // (a step's click on a province is never the airfield the real pointer happens to rest by)
	// ---- For the playtester's click-through (CampaignStepPlayer: a development tool) ----
	/** What the player has in front of him: the card or page on top, the words he is shown, and what is in hand. */
	void DescribeScreen(FString& OutOpen, FString& OutWords, FString& OutHand) const;
	/** A right-click with whatever is in hand, on a province or (Sea given) on a sea: the order the mouse gives. */
	void RightClickOn(int32 RegionId, int32 Sea, bool bOnTheTable);
	/**
	 * A right-click on the figure of the army this corps stands in, aimed as the mouse aims it: along the line from the
	 * eye through its colours. Says which province the order went to, and which province's ground lay behind the pointer.
	 */
	bool RightClickOnFigure(int32 CorpsId, int32& OutAimed, int32& OutGround);
	/** On the approach order's card: another way up is picked. */
	void PickApproach(int32 Index);
	/** True while a card, a page or a screen lies over the table and its clicks do not reach the map. */
	bool IsTableBusy() const;
	/**
	 * (Speed, b414 mark 1.) True from an order the rules answer on a thread of their own (a march, a landing, an attack)
	 * until its answer is on the table, a frame or a few later: the map, the keys and the attack card take no other
	 * order meanwhile.
	 */
	bool IsAnswering() const;
	/** True while the game plays something through by itself: the other countries' month, a newsreel, an announcement. */
	bool IsBusyForSteps() const;
	/** Answers what a month's end puts in the way, as a player in a hurry would: the question, the newsreel, the announcements, the replay. */
	void StepThroughMonth();
	bool RestoreCampaign(const FString& SaveText);
	/** Begins a grand campaign as this country from this month, with these dice and this level, as the opening screen would. */
	bool BeginGrandFor(const FString& Tag, int32 FromTurn, int32 Seed, const FString& LevelWord);
	const TArray<int32>& StepPicked() const { return PickedCorps; }
	int32 StepFleet() const { return FleetInHand; }
	/** How many sentences the paper line has been given so far: a sentence said again is still an answer. */
	int32 StepNotices() const { return NoticesSaid; }
	int32 NoticesSaid = 0;
	void PressEscape() { EscapeKey(); }
	void PressTab() { TabKey(); } // (the code checker's pre-check: a replay's Tab goes through the key guard, as a player's does)
	void PressSpace() { SpaceKey(); }
	/** Enter as the key gives it: the lit button of the card that is up, or the map's own END THE MONTH under the key guard. */
	void PressEnter() { EnterKey(); }
	/** What the game is showing by itself just now, for the screens a month's end brings: the question, the replay, a newsreel, an announcement, or the page on top. */
	FString StepState() const;
	/** A click on a province's airfield, as the mouse gives it. */
	void ClickAirfield(int32 RegionId) { PointedField = RegionId; ClickOnTable(nullptr, RegionId); }
	/** A click on the plate of the gate whose name begins so, as the mouse gives it with nothing in hand. False where there is no such gate. */
	bool ClickGate(const FString& NameBegins);

	/** Sends the army in hand to the place that was clicked once and is waiting for the word. */
	void ConfirmPending();

	/** Sends the attack shown on the face-off in. */
	void ConfirmAttack();
	/**
	 * (release 2; Brian, 22 November 21:21Z, asked "fight it yourself?" before each campaign battle) FIGHT IT YOURSELF on
	 * the attack card, beside SEND THEM IN, which lets his generals decide: the battle the card shows (TC.battleLook) is
	 * fought by him as the general in the 3D field, and when he presses CONTINUE on its after card the campaign books its
	 * result (TC.battleSettleLand) and the card shows it, as SEND THEM IN's does. Off with -TheaterLandNoCampaignLive.
	 */
	bool CanFightLive() const;
	void FightLive();
	/**
	 * (Release 1.6; Brian, 23 November 06:52Z: the campaign's air fights open as 3D air battles) FLY IT YOURSELF on the raid
	 * card, beside FLY THE RAID BY DAY, on a bombing raid by day and a fight for the sky only: the raid the card shows, with
	 * the groups picked, is flown by him in the 3D sky (TC.airStart), and at CONTINUE on its after card the campaign takes
	 * what the rules booked (TC.battleEnd books the raid as FLY THE RAID does, with the battle's losses) and the map shows
	 * it. Behind the Battle rules card's "Fly raids yourself" (AAirBattle::CampaignLiveOn), off until it passes; offered
	 * only where TC.airLook has a ticket for the raid as picked.
	 */
	bool CanFlyLive() const;
	void FlyLive();
	/** (Release 1.5) The look of the land a battle for this region is fought on, this month, as LandScape::Pick lays it,
	 * with "+snow" where snow lies (LandScape::SnowOf a quarter or more). */
	static FString FieldLookAt(const FCampaignMapData& Data, int32 RegionId, int32 Turn);
	/** (Release 1.5) Whether a look has passed (Content/Rules/fight-yourself-looks.txt, one look a line; bocage alone
	 * where the file is missing). -TheaterFightEveryLook offers every look (the Mac's per-theatre runs). */
	static bool FieldLookPassed(const FString& Look);
	/** True while his own battle is open: the map takes no order, key or month's end until it is handed back. */
	bool IsFightingLive() const { return LiveBattle.IsValid() || LiveRaid.IsValid(); }
	/** (Rules 84.1.) On the card before a march on a guarded town: SEND THEM IN (the assault as they arrive) or LAY SIEGE FIRST. */
	void ConfirmMarch(bool bAssault);
	/** On the attack card: pick every corps that can reach the place, and show the attack again as it would then be. */
	void SendEveryone();
	/** (Version 63) Back to the corps that were in hand when the attack was ordered, where the staff brought in more. */
	void SendOnlyPicked();
	/** (Version 64) One corps feels out the line before the attack: the card is read again with what it found. */
	void ProbeLine();
	/** (Version 64) The raid planner, on this line: the attack is ordered again when the bombers are back. */
	void BombLine();
	/** (Version 64) The card that folds two worn corps of the army in hand into one. */
	void OpenFold();
	/** A long Battle Ticket opens folded to the lines that matter most: this unfolds it, or folds it again. */
	void ToggleTicket() { bTicketAll = !bTicketAll; }
	bool IsTicketAll() const { return bTicketAll; }

	/** Calls the attack off, or closes the result of one that has been fought. */
	void CloseFaceOff();

	void EndMonth();

	/** Ends the month and plays the other nations' months out one at a time, even with a script driving. */
	void EndMonthAndWatch();

	/** Cuts short the watching of the other nations' month. */
	void SkipWatching();
	/** (Ruling 64.) The month's replay, shown again from the month's card. Nothing is told twice: the card comes back when it is over. */
	void WatchAgain();
	bool CanWatchAgain() const { return !bWatching && !bMonthTurning && Month.Steps.Num() > 0; }

	/**
	 * The hand on a flag, as three steps a script can take too: take hold of the player's army in a region,
	 * hang it over another, and let go of it there.
	 */
	bool HoldArmyIn(int32 RegionId);
	void CarryHeldOver(int32 RegionId);
	void DropHeldOver(int32 RegionId);

	/** While a script is driving (the development photographer), the pointer does not light or route anything. */
	void SetScripted(bool bInScripted) { bScripted = bInScripted; }
	/** (The UI director's 84.3 check, item 3.) Where the screen's panels stand, in the viewport's own pixels, as the HUD last laid them out. */
	void SetFrameOnScreen(TArray<FBox2D>&& Boxes) { FrameOnScreen = MoveTemp(Boxes); }

private:
	// (Release 1.5.1) The saved wars read afresh for the menu; and one laid on the table.
	void ReadSavedWars();
	void LoadWar(const FString& Id);
	void WheelUp();
	void WheelDown();
	void Escape();

	/** Where on the table the pointer is, allowing for the height of the ground. */
	bool PointerOnTable(FVector& OutWorld) const;

	void RefreshRegionPanel(int32 RegionId);
	void RefreshOrders();
	void Notify(const FString& Text, double Seconds = 5.0);
	void OpenFaceOff(int32 RegionId, bool bOpening = false, const FAttackPreview* Given = nullptr);
	/** (tc.Speed.CardAside) OpenFaceOff(RegionId, true) with its forecast worked out on the rules' thread: the card stands at once and fills when it comes. */
	void OpenFaceOffAside(int32 RegionId);
	/**
	 * (Rules 84.1.) The army in hand is ordered onto enemy ground with no army in its field. Where the town's guard
	 * will fight, opens the attack card and returns true. Otherwise returns false (nothing is risked: no card and no
	 * yes, ruling 178), with the rules' sentence for what the march meets in OutSays where they have one.
	 */
	bool OpenMarchCard(int32 RegionId, FString& OutSays, FString* OutMeets = nullptr);
	/**
	 * (Ruling 182: no storm is fought without its card.) The assault on the town of a province the player's army
	 * stands round: opens the card and returns true. Where the rules refuse a storm this month their sentence is
	 * said; where there is nothing to storm, bSayWhy says so. bOnArrival: the army has just marched up to a town
	 * its reports could not see into, and the card opens by itself only where its guard alone holds it.
	 */
	bool OpenStormCard(int32 RegionId, bool bSayWhy, bool bOnArrival = false);
	/** The march's and the storm's card over the attack card as it stands: the guard as the defender, the two blocks' headings and sentences. */
	void DressSiegeCard(const FAttackPreview& Preview, int32 RegionId);
	/** The battle the card asked about is fought and its result shown: an attack, or the assault a march ends in. */
	void ResolveAttack();
	bool bTicketAll = false;
	/** bPictureFirst (an order's answer): the ground and the armies now, the rest of the table a frame after (tc.Speed.PictureFirst). */
	void ShowWar(const FCampaignView& View, const TArray<FCorpsMove>& Marches, const FVector* CarriedFrom = nullptr, bool bPictureFirst = false);
	void ShowWarRest(const FCampaignView& View);
	/**
	 * (Speed, b414 mark 1.) What follows an order's answer, a step a frame (tc.Speed.PictureFirst): the ground and the
	 * armies, then the rest of the table, then the army in hand's orders and the province's card. At once where pictures
	 * are being taken, or with the switch off.
	 */
	TArray<TFunction<void()>> Afterwards;
	void Later(TFunction<void()>&& Step);
	/**
	 * (Speed, b414 mark 2.) The last of an order's answer, a frame each: the army in hand's orders (RefreshOrders), then
	 * Then (the province's card and the paper line), then the enemy's reach. Build 1590 had all three in one frame of 35 to
	 * 79 ms.
	 */
	void LaterOrders(TFunction<void()>&& Then);
	/** Set while LaterOrders' RefreshOrders runs: the enemy's reach is then shown a step later. */
	bool bOrdersInSteps = false;
	/** The enemy's reach so put off, until shown: any RefreshOrders before then shows it first, as it would have been. */
	TOptional<TArray<int32>> PendingEnemyReach;
	/** Everything still to come of an order's answer, now: before a campaign is put away. */
	void FinishAnswer();
	/** The nation's adjective as the army in hand's tray names it, for the rules' thread, which reads no map. */
	FString HandAdjective() const;
	/** What follows an order's answer, on the game thread: what OrderTo, ConfirmWar, ConfirmMarch and ResolveAttack did once the rules had answered. */
	void AfterMarch(int32 RegionId, const TArray<FCorpsMove>& Marches, FString Said, const FString& MarchSays, const FString& MarchMeets, const TOptional<FVector>& Carried);
	void AfterFarMarch(const TArray<FCorpsMove>& Marches, const FString& Said, bool bWasSpent, const TOptional<FVector>& Carried);
	void AfterLanding(int32 RegionId, const FLandingResult& Result);
	void AfterSiege(int32 RegionId, const FAttackResult& Result);
	void AfterFight(int32 RegionId, bool bMarch, bool bStorm, bool bSally, const FAttackResult& Result);
	/** His own battle handed back (FightLive): its result booked by the campaign, then shown on the attack card. */
	void LiveHandBack(int32 RegionId, const TArray<int32>& CorpsIds, double TicketSeed, const FString& ResultJson);
	/** The field of his own battle while it is open (FightLive). */
	TWeakObjectPtr<class ALandBattle> LiveBattle;
	/** (Release 1.6) His own raid handed back (FlyLive): TC.battleEnd's answer taken as the raid's act, the map brought up to date. */
	void AirHandBack(const FString& Kind, int32 Target, const FString& EndJson);
	/** (Release 1.6) The sky of his own raid while it is open (FlyLive). */
	TWeakObjectPtr<class AAirBattle> LiveRaid;
	FCustomState Custom;
	void LoadCustomOffer();
	/** The spec for the rules (CustomSpec), the computer's army in it if bWithTheirs and it picks its own. */
	FString CustomSpecJson(bool bWithTheirs) const;
	/** The page made again from the choices and the rules' check of them (TC.customArmy, TC.customCheck). */
	void RefreshCustom();
	void FightCustom();
	/** A battle of his own handed back (its scene's CONTINUE): the door again, its outcome in a line. */
	void CustomHandBack(const FString& Domain, const FString& ResultJson);
	/** (Release 1.6) FLY IT YOURSELF's offer, weighed once for the fly act it was asked of (TC.airLook): 1 offered, 0 not, -1 not yet. */
	mutable FString FlyOfferAct;
	mutable int32 FlyOffer = -1;
	/** (Release 1.5) The raid card his raid was flown from (its page and its fly act), for the card to come back as he left
	 * it when the raid closes with no result (AirHandBack). */
	FString FlyLivePage;
	FString FlyLiveAct;
	bool CanPickUp(const AArmyPiece* Piece) const;
	void ClickOnTable(AArmyPiece* Piece, int32 RegionId);
	bool Hold(AArmyPiece* Piece);
	void SetPending(int32 RegionId);
	void DropArmy(int32 RegionId, bool bOnTable);
	void WatchNextStep();
	/** (Release 2, tc.Speed.MonthAside) The others' months are in (Month): the watch, or the month's card. bWatchAsked: END THE MONTH AND WATCH was pressed. */
	void MonthPlayed(bool bWatchAsked);
	void FinishMonth();
	void SpaceBar();
	void ToggleMusic();
	void ToggleEffects();
	int32 ShownForce = INDEX_NONE;
	bool bShownFleet = false;

	void RefreshOpening(bool bShow);
	void EnterCampaign();
	class ACampaignGameMode* GetMode() const;
	const FReachRegion* FindReach(int32 RegionId) const;

	TWeakObjectPtr<ACampaignMapActor> Map;
	FRegionPanelInfo RegionPanel;
	FArmyTray Tray;

	/** The army in hand: the region it stands in and the corps that will go when it is ordered. */
	int32 ArmyRegion = INDEX_NONE;
	TArray<int32> PickedCorps;
	TArray<int32> PickedBeforeMass; // the corps that were in hand before "send everyone" was pressed on the attack card
	TArray<FReachRegion> Reach;
	TSet<int32> EnemyReach; // with an army in hand: the regions the enemy he can see could strike next month
	/** (release 1) A view of the map that has been kept, to come back to: where it looks, from how far, turned and tipped how. */
	struct FKeptView
	{
		bool bKept = false;
		FVector Focus = FVector::ZeroVector;
		float Zoom = 0.5f, Yaw = 0.0f, Tilt = 0.0f;
	};
	FKeptView KeptViews[4]; // under the keys 6, 7, 8 and 9
	bool bKeptViewsRead = false;
	bool bChartView = false; // (release 1) the admiral's chart, as one of the map's views; it is also shown while a fleet is in hand
	void ReadKeptViews();
	void KeepView(int32 Slot);
	void GoToKeptView(int32 Slot);
	TMap<int32, int32> FarMonths; // and friendly ground beyond this month's reach: about how many months the march there would take
	TSet<int32> RailBeyond; // (rulings 110, 112, 118) and the provinces beyond its march that a train would take it to this month
	// (Ruling 87.) Every corps in hand has used its moves: a right-click on friendly ground is kept for next month.
	bool bHandSpent = false;
	bool bMarchSlipDone = false; // (kept with the options) GOT IT has been pressed, or one march kept for a spent army
	// (One voice after a press; the UI director's brief, 17 November.) An order has been given to the army in hand since
	// it was picked up: the slip stays put away, and the tray's foot no longer speaks of next month.
	bool bHandOrdered = false;
	FString HandArmy; // the army in hand as its tray's head names it, for the rules' sentences: "the American army", "Heinz Guderian's army"
	TArray<int32> HandArmyIds; // and the corps it has in the field there
	bool bNightTold = false; // (ruling 205(c)) this campaign's first night has said that it is night, and which key holds the hour
	FMarchPlan KeptPlan; // the rules' plan for a march to the place last asked about
	int32 KeptPlanRegion = -2;
	const FMarchPlan& PlanFor(int32 RegionId);
	/** Where a line from the eye meets the table. */
	bool RayOnTable(const FVector& Origin, const FVector& Direction, FVector& OutWorld) const;
	/** The province an order goes to: that of the army whose figure the pointer is on, with an army in hand, or else the ground behind the pointer. */
	int32 AimedOf(const AArmyPiece* Piece, int32 Ground) const;
	/** Where every corps in hand is the garrison of its town: the sentence that says so and how it can be moved. Empty otherwise. */
	FString GarrisonSays() const;
	/**
	 * (Release 2, Brian, 24 November 2026: "if a unit is dug in and i click them to attack and move I want them to over
	 * ride their orders insted of me having to press a button".) True where the garrison in hand may come out of its
	 * town (TAKE THE FIELD is offered for every corps in hand) and could reach that place from the open country this
	 * month (the rules give a garrison's reach from there, all its corps out together): an order to it is given as to
	 * an army in the field, and the rules bring them out inside the order's own call, only if the order goes.
	 */
	bool ComesOutTo(int32 RegionId) const;
	/** True where the garrison in hand may come out of its town and could go somewhere this month once out. */
	bool ComesOut() const;
	/**
	 * The march up to an enemy army that is not next door and the attack, planned as one order and shown before
	 * anything is carried out. True where the card opened or the rules' sentence was said; false where the rules have
	 * no plan to offer (no enemy army seen there, or it is next door already).
	 */
	/** The sortie's card: the attack card under the sortie's title. False where the rules will not have a sortie (their reason is said). */
	bool OpenSortieCard(int32 RegionId);
	bool OpenApproach(int32 RegionId, bool bSayNone);
	void DressApproach();
	void CarryOutApproach(const FWarPrompt& Asked);
	double WarPromptSince = 0.0; // (an order's card takes no key in its first half second)
	bool bSortieAsked = false; // (the sortie's question has been answered yes: the order goes through)
	/** Where a garrison's own order stands, said after the rules' sentence so that no hint sends him to a press that does nothing. */
	FString GarrisonWhere() const;
	/** Why the army in hand cannot attack the enemy army seen at a place, in a sentence that says what to do; empty where none is seen there. */
	FString WhyNoAttackOn(int32 RegionId) const;
	FText HoverTipMarked;
	FHoverGlance HoverGlance; // (release 2) the paper card over a town with a plate
	TArray<int32> StandingRoad; // and the road still ahead of it, if it is on a standing march
	int32 RouteTarget = INDEX_NONE;
	int32 PendingTarget = INDEX_NONE; // ground clicked once with an army in hand: a second click, or the button, sends it there
	bool bHeldWasInHand = false; // the flag pressed was already the army in hand, so a plain click on it puts it down
	TWeakObjectPtr<AArmyPiece> RingedPiece;
	TWeakObjectPtr<AArmyPiece> HoverPiece; // the piece the pointer rests on, if it can be picked up
	TWeakObjectPtr<AArmyPiece> HeldPiece; // the piece the left button went down on
	// (Release 2; Brian 23 November 2026 22:23Z) One of the player's fleets the left button went down on: carried like a
	// flag, and let go over a sea it sails there, or over an enemy fleet it attacks it (the attack card first). And
	// whether it was the fleet in hand already, so a plain click on it puts it down.
	TWeakObjectPtr<AArmyPiece> HeldFleetPiece;
	bool bFleetWasInHand = false;

	FFaceOff FaceOff;
	/** Works out FaceOff.FieldOffer, once a card and pick (CanFightLive). */
	void WeighFieldOffer();
	FWarPrompt WarPrompt;
	FLedger Ledger;
	bool bLedgerOpen = false;
	FDiplomacy Diplomacy;
	bool bDiplomacyOpen = false;
	FResearch Research;
	bool bResearchOpen = false;
	FGeneralStaff Staff;
	bool bGeneralsOpen = false;
	FOfficePage Page;
	bool bPageOpen = false;
	// (Release 2, THE HOME FRONT) The board's own state, kept while it stands: the list shown, the town unfolded, and the
	// towns built in since it was opened.
	int32 HomeFilter = 0, HomeUnfolded = INDEX_NONE, HomeRows = 40, HomeOpenings = 0;
	TSet<int32> HomeKept;
	// (Release 2, tc.UI.Ease: Esc steps back one screen at a time.) The pages this one was reached from by their own
	// doors, the nearest last; and the town whose card was opened from THE HOME FRONT, whose Esc goes back to the board.
	TArray<FString> PageTrail;
	int32 TownFromHomeRegion = INDEX_NONE;
	/** (tc.UI.Ease) Esc on a page: the page it was reached from, if there is one. */
	bool StepBackPage();
	/** (tc.UI.Ease) THE HOME FRONT again as it was left: the same towns listed, the same rows unfolded and kept. */
	void ReturnToHomeFront();
	void RefreshGenerals(bool bArmiesChanged);
	FString PathAsked; // the path waiting for the player's yes, with its partner: "found:DEN"
	FArsenal Arsenal;
	bool bArsenalOpen = false;
	FOpening Opening;
	FDocumentPage Document;
	int32 DocumentRevision = 0;
	TArray<FStartGroup> StartGroups;
	TArray<FGrandNation> GrandNations;
	int32 GrandSeed = 1939; // the dice for the campaign about to be chosen
	int32 WorkingFrames = 0;
	void LoadGrandNations();
	/**
	 * (Release 2, tc.Speed.NationsAside; the UI director's bar: every press answers within a tenth of a second.) The
	 * list of countries for the year and the dice now chosen, worked out on the rules' own thread while the screen says
	 * so (Opening.bWorking), and laid when it is in; a list asked while one is out is asked once that one is in. At
	 * once, as LoadGrandNations, where the switch is off or in a run that takes pictures.
	 */
	void AskGrandNations();
	/** The list of countries laid on the opening screen; the country chosen stays chosen where it is still there. */
	void TakeGrandNations(TArray<FGrandNation>&& Listed);
	bool bNationsAsked = false; // (tc.Speed.NationsAside) a list of countries is being worked out on the rules' thread
	/**
	 * (Release 2, tc.Speed.WarSpread; the UI director's bar: every press answers within a tenth of a second.) A war
	 * begun or continued from the opening screen: at once, as before, or with the plate drawn first, the war begun
	 * (Start) a few frames on, laid a step a frame under the plate, and entered when the last step is down (StepWar,
	 * from PlayerTick). bFresh: a new campaign (bFreshCampaign); bBugle: the bugle as it is entered.
	 */
	void BeginWar(TFunction<bool(class ACampaignGameMode&)>&& Start, bool bFresh, bool bBugle);
	void StepWar();
	TFunction<bool(class ACampaignGameMode&)> WarDue;
	int32 WarDueFrames = 0;
	bool bEnterWhenLaid = false;
	bool bWarFresh = false;
	bool bWarBugle = false;
	static int32 NewSeed();

	FString NoticeText;
	double NoticeUntil = 0.0;
	FMemorialShow Memorial;
	FString DispatchText;
	double DispatchUntil = 0.0;

	// The other nations' month, being played out one nation at a time.
	bool bWatching = false;
	// ---- (release 2, the enemy's month near you) The option (kept with the options; on unless he turns it off), and the
	// watch of only the months that came near him: how many there are, how many have been shown, whose is up, and the
	// arrows of the one up as they grow (from ArrowsSince).
	bool bEnemyMoves = true;
	bool bAlliesNear = true; // (round 2, Brian 18:01Z) his allies' battles near him stop the watch too
	bool bAlliesThisWatch = true; // (the switch as it stood when the month was ended: the watch's stops and pings keep to it)
	bool bTakeMeThere = true; // (round 2) the TAKE ME THERE button on each stop (OPTIONS)
	int32 TakeRegion = INDEX_NONE; // the stop's place: where its worst arrow ends
	bool bCornerPings = true; // (round 2) the watch's stops pulse on the small map in the corner (OPTIONS)
	// (Release 2, new units ready; Brian 24 Nov 18:30Z) The option (on unless he turns it off), the units handed over as
	// this month opened, the next SHOW ME goes to, whether the plate is still to be said, and the notice that said it.
	bool bReadyNotice = true;
	TArray<FReadyUnit> ReadyUnits;
	int32 ReadyNext = 0;
	bool bReadyDue = false;
	int32 ReadySaid = -1;
	FString ReadyNation;
	FString ReadyHeading;
	TArray<int32> NearStopPlaces; // each stop's place, in the watch's order (INDEX_NONE where it has nothing to draw), set as the month ends
	TArray<FColor> NearStopInks;
	bool bWatchNear = false;
	int32 NearCount = 0, NearShown = 0;
	bool bNearShownThisMonth = false; // (release 2, tc.UI.SeaWaits) this month opened with the enemy's moves near him shown
	bool bSeaOffered = false, bSeaAsked = false; // (release 2, tc.UI.SeaWaits) the month's scene at sea held, and asked for
	FString WatchNation;
	TArray<FStepArrow> ArrowsShown;
	double ArrowsSince = 0.0;
	bool bArrowsGrowing = false;
	/** A step the watch near him stops at: another country's month that came near his ground or touched him. */
	bool IsNearStep(const FMonthStep& Step) const;
	/** Puts away the step's arrows. */
	void ClearStepArrows();
	// ---- (end release 2)
	bool bMonthCardDue = false; // the month's card is to open once the replay, the newsreel and the great moments are over
	// (Release 2, news at the map's edge; ruling 131: merge by hand) Where bMonthCardDue is for a month just resolved, its
	// rows are laid as seals on the map's edge instead of the card opening by itself, unless the option "Month report:
	// open by itself" is on (kept with the options; off by default). Nothing here is saved: the seals are laid again
	// from the month's card.
	FEdgeNewsBoard EdgeNews;
	bool bEdgeNewsDue = false;
	bool bMonthReportOpens = false;
	void LayEdgeNews(const FMonthCard* Shown);
	// (Brian, 24 November 2026: a raid "didnt't tell me the results of the battle".) The report of the raid his crews
	// have just been sent on ('airreport:<id>', which the rules name as the page that comes next): it opens once the
	// aircraft have been seen to fly the raid, in the month it was flown in.
	FString RaidCardDue;
	double RaidCardAt = 0.0;
	int32 RaidCardTurn = 0;
	bool bRewatching = false; // the month is being watched again from its card
	bool bHandStands = false; // (ruling 88) the army in hand has been told to stand this month: the rules keep the order, with the saved game
	TArray<int32> RoundSeen; // the provinces the round of armies has been to since it last began
	TArray<FString> ToDoSeen; // (release 2, tc.UI.NextAll) the screens of the month's list Tab has opened since its round last began
	/** (tc.UI.NextAll) Tab past the armies: opens the next row of the month's list, or says the round is done. False where it did nothing. */
	bool StepToDo();
	FEndAsk EndAsk;
	int32 EndAskedTurn = -1; // the month the question was last put: it is put once a month
	bool bAskAtMonthsEnd = true; // (kept with the options)
	FMonthResult Month;
	// ---- (Release 2, the scramble offer; Brian, 24 November 2026 22:21Z) The card up; the month's answer in hand while
	// it waits (shown where the next answer is not a month's: the stand-in's, or a failure); how many scrambles this month
	// has stopped on (a cap, so a month is never a queue of cards); the raid being flown; the card's tick as last answered
	// (every later scramble this month is left to the staff); whether the month was ended to be watched (V's END THE MONTH
	// AND WATCH), kept for the month's rest; and the OPTIONS setting, "every", "big" or "never" (kept with the options).
	FScrambleCard Scramble;
	FMonthResult ScrambleMonth;
	int32 ScrambleRound = 0;
	FString ScrambleFlown;
	bool bScrambleRest = false;
	bool bScrambleWatch = false;
	FString ScrambleMode = TEXT("big");
	// (The reviewer, 25 November: the note a scramble's answer carries, TC.battleEnd's or TC.scrambleLeave's, was said
	// under the map and at once replaced by the month's own "<date>. Your orders." there, which has the one line. It is
	// kept here and said with that line (FinishMonth); with the switch off it is always empty.)
	FString ScrambleNote;
	/** Everything of a scramble put away: the card, the month it held, this month's count, the tick, the raid flown, the
	 * note. For a war left or loaded (EnterCampaign, RestoreCampaign): none of it belongs to the next. */
	void ForgetScramble();
	/** The month's answer in Month taken: a scramble is offered (or left at once), or the month's watch and card follow as ever. */
	void MonthAnswered();
	/** The scramble in Month offered on its card, or left to the staff at once (the switch off, a run that takes pictures,
	 * Scramble offers NEVER, the card's tick, enough cards this month) and the month's next answer taken: true either way. False only where the
	 * month has stopped too many times: its scramble is dropped, and the month in hand runs on. */
	bool OfferScramble();
	/** TC.scrambleLeave(Id, { rest }), its answer taken as the month's next. */
	void LeaveScramble(const FString& Id, bool bRest);
	/** The scramble's battle handed back at CONTINUE: TC.battleEnd's answer, or empty from the way out (then left to the staff). */
	void ScrambleHandBack(const FString& EndJson);
	/** The month's next answer after a scramble taken through the same handler as TC.endMonth's (MonthAnswered); where it
	 * is not a month's (or is empty: the call failed), the month in hand is shown, and the log says so. */
	void TakeScrambleAnswer(const FString& Json);
	int32 WatchIndex = 0;
	double WatchNext = 0.0;
	FVector WatchReturnFocus = FVector::ZeroVector;
	float WatchReturnZoom = 0.5f;

	bool bLeftHeld = false;
	bool bDragging = false;
	bool bRightHeld = false;
	bool bTurning = false;
	bool bHaveLastMouse = false;
	FText HoverTip;
	TArray<FAnnouncement> Announcements;
	FText ShowTitle;
	int32 ShowCorps = INDEX_NONE;
	bool bToldOfEvening = false; // the first dusk has been explained
	int32 LastClickRegion = INDEX_NONE; // for a double click: where the last plain click fell, and when
	double LastClickAt = 0.0;
	bool bPointerOverPanel = false; // the pointer is on one of the screen's panels, not on the map
	bool bButtonDown = false; // the mouse was down at the last look
	// (The UI director's 84.3 check, item 3.) Nothing silent under a panel.
	TArray<FBox2D> FrameOnScreen; // the panels, in the viewport's pixels
	int32 FrameReachFor = INDEX_NONE; // an army just picked up, whose reach is to be framed clear of the panels once the view is still
	double FrameReachFrom = 0.0, FrameReachUntil = 0.0;
	FVector LastCameraSpot = FVector::ZeroVector;
	bool bSlateRightDown = false; // the right button, as the screen itself saw it at the last look
	bool bRightPressOnPanel = false; // and it went down on a panel
	bool bRightGameOnPanel = false; // the map saw the press, but it was on a panel: no order goes through the panel
	bool bPanelRightClick = false; // a right-click on a panel, to be answered with a line
	FVector2D RightPanelPress = FVector2D::ZeroVector;
	/** An army has been picked up by a hand (a press on its flag, or Tab): its reach is framed clear of the panels soon. */
	void FrameReachSoon();
	/** Frames the army in hand and the ground it can reach in the largest stretch of the screen no panel covers. */
	void FrameReach();
	const void* HoveredButton = nullptr; // the button the pointer is on, to tick once as it crosses onto one
	double BattleMusicUntil = 0.0; // the march plays until then
	int32 SackAsked = INDEX_NONE; // the general whose dismissal has been asked once
	// (Rules 84.3.3) FIRST COMMAND. The rules' last answer; the note on screen and when it came; the stamp on it; the
	// note just gone; lesson 1 laid on the month card now open; and what is owed to the rules once no order is being
	// answered (the code checker's note on T1: every lesson call waits out an order in flight, so none is made then).
	FLessonDesk Lessons;
	FLessonNote LessonNote, LessonGone, LessonLaid;
	bool bLessonNote = false;
	bool bLessonLaid = false;
	int32 LessonArrival = 0;
	double LessonCameAt = 0.0, LessonGoneAt = -1.0, LessonStampAt = -1.0, LessonStampUntil = 0.0;
	bool bLessonsDue = false;
	bool bLessonStampOwed = false; // the note's deed is done, and its stamp waits for the screen to be his again
	bool bLessonGoneStamped = false;
	bool bLessonNoteHidden = false;
	bool bArsenalStrip = false; // lesson 10's strip is laid across the arsenal, and keeps its room
	bool bArsenalStripPutAway = false; // he put the note in the strip away himself: the room closes up until it is next opened
	uint32 LessonMomentKey = 0;
	TArray<TPair<FString, bool>> LessonDeedsOwed; // a deed only the screen sees, and whether the month card was open
	TArray<FString> LessonActsOwed;
	bool bOpeningCardOwed = false; // trap 1: the card lesson 1 lies on, owed once the opening flight is over
	bool LessonCardOpen() const { return bPageOpen && Page.Month.bValid; }
	bool IsLessonQuiet() const;
	void AskLessons();
	void TakeLessons(const FLessonDesk& Desk);
	void ShowDeskNote(double Now, bool bWasStamped = false);
	void LessonPageChanged();
	void ResetLessons();
	// (The pointer.) What it frames this frame; where the note stands; the ground pointer as last laid (its frame and
	// the eye it was laid for), so that it is laid again only when either has moved.
	FLessonAim LessonAim;
	FBox2D LessonNoteOnScreen = FBox2D(ForceInit);
	FString GroundAimLaid;
	void AimLesson();
	// (The Guide's list.) Open, what it lists, and the page under it, which it gives way to if another is opened.
	bool bLessonGuideOpen = false;
	FLessonGuide LessonGuidePage;
	int32 LessonGuideRevision = 0;
	FString GuideOverPage;
	bool OpenLessonGuide();
	void RefreshLessonGuide();
	// (The brief: the notes "read aloud by the built-in voice when the voice is on", the deed line last.) The note
	// last read, by its arrival (and the arrival the screen has had a tick to place); and whether lesson 1, laid on the
	// card that is open, has been read.
	int32 LessonSpokenArrival = -1;
	int32 LessonPlacedArrival = -1;
	bool bLessonLaidSpoken = false;
	void SpeakLesson(const FLessonNote& Note);
	bool PieceBoxOnScreen(const AArmyPiece* Piece, FBox2D& Out) const;
	bool GroundSpotOnScreen(int32 Region, FVector2D& Out, FVector& Foot) const;
	void LayGroundPointer(const FLessonAim& Aim);
	FAdviser Adviser;
	void AdviserSaw(int32 Step);
	bool bBusyWithScreens() const { return bWatching || Newsreel.bOn || Opening.bVisible || Announcements.Num() > 0 || bScripted; }
	FNewsreelShow Newsreel;
	bool bNewsreels = true; // K turns them off
	// (Ruling 203.) Off, as it stands unless he asks: the month's card comes first, and the other countries' moves,
	// the newsreel and the great moments are a press away on it. On: they play before the card, as they used to.
	bool bShowsFirst = false;
	TArray<FNewsCard> LastReel; // the month's newsreel, kept for the card's own button
	FString LastReelDate;
	double FaceOffSince = 0.0; // when the attack card opened: it takes no key for its first half second
	// The standing choices, kept in a file of their own so they outlast the campaign.
	void LoadOptions();
	void SaveOptions() const;
	static FString OptionsFile();
	bool bOptionsRead = false;
	bool bNarrator = true; // the voice that reads the newsreels
	bool bBriefings = true; // the opening flight when a campaign begins, and the title card of each year
	bool bFrameHidden = false; // the frame is put away for a clean picture (the F key)
	double FrameHintUntil = 0.0;
	bool bMoments = true; // the eye drops to the front for a few seconds when a capital falls or a great battle is decided
	bool bShipsSail = true; // (the 1.5 patch: on unless switched off in OPTIONS) a fleet that moves sails there on the map (ACampaignMapActor::SetShipsSail)
	bool bWakes = false; // (release 2) an enemy fleet seen to move leaves a fading wake (ACampaignMapActor::SetWakes)
	bool bMomentDue = false; // such a thing has just happened: the moment begins when its result has been read
	bool bMomentOn = false;
	double MomentUntil = 0.0;
	double MomentGlideUntil = 0.0; // the eye keeps its slow travel until it is back where it was
	FVector MomentHome = FVector::ZeroVector; // where the eye was looking, and from how high, before the moment
	float MomentHomeZoom = 0.5f;
	bool bHelpSeen = false; // the player has ended a month, or folded the keys away: they are not shown unasked again
	bool bHelpPinned = false; // the reminder of the keys is kept open by a click on its mark
	bool bFreshCampaign = false; // a campaign has just been begun, not continued
	int32 SeatTurn = INDEX_NONE; // (release 2) the month a campaign begun here opened on: the frame stays open through it
	int32 TurnLaid = -1; // the month the table's holders were last laid for, and for whom: ink spreads only from one month to the next
	FString HumanLaid;
	FYearCard LastYear; // the last year's card, to show again
	// What an army answers as it is picked up, in its own language, for a moment.
	FText ArmySaid;
	double ArmySaidUntil = 0.0;
	double BriefingAt = 0.0; // when the opening flight is to begin, a moment after the table is laid
	// A newsreel that waits for a year's title card to go by.
	FString WaitingReelDate;
	TArray<FNewsCard> WaitingReel;
	bool bAdviserWanted = true; // the staff officer of the opening months
	bool bEdgeScroll = true; // the pointer resting at the window's edge slides the table
	float EdgeRest = 0.0f; // how long it has rested there
	TArray<FAnnouncement> HeldAnnouncements; // waiting for the newsreel to end
	FVector ReelReturnFocus = FVector::ZeroVector;
	float ReelReturnZoom = 0.5f, ReelReturnYaw = 0.0f;
	FProcHandle Narrator; // the voice reading the story
	void Narrate(const FString& Words);
	void HushNarrator();
	// The game's voice (the computer's own, for a start): the newsreels and the opening flight read aloud, the
	// title card of a year, the adviser's steps, and an army's answer in its own language when it is picked up.
	// One line is spoken at a time; the others wait their turn.
	struct FVoiceLine
	{
		FString Words, Voice;
		int32 Rate = 180;
	};
	TArray<FVoiceLine> VoiceQueue;
	int32 VoiceCount = 0;
	double AnsweredAt = 0.0; // when an army last answered aloud: they do not chatter
	bool bAdviserSpoke = false; // the adviser has said his first word in this campaign
	/** Cut: 0 waits its turn; 1 is said now and the rest wait behind it; 2 is said now and the rest are forgotten. */
	void Speak(const FString& Words, const FString& Voice, int32 Rate, int32 Cut);
	void PumpVoice();
	void StopSpeaking();
	bool IsSpeaking();
	void SpeakAdviser();
	int32 ProvinceTab = 3; // (release 2) 3 AT A GLANCE, which a newly picked town opens on; 1 ORDER; 2 BUILD; 0 PROVINCE
	int32 FleetInHand = INDEX_NONE; // the fleet picked up, by its id
	int32 AgentInHand = INDEX_NONE; // (ruling 153) the agent picked up, by his id
	bool bAgentHeld = false, bAgentWasInHand = false; // the left button went down on an agent's piece; and he was in hand already, so a plain click puts him down
	FString FleetNext = TEXT("patrol");
	mutable FVector2D SeaAskedAt = FVector2D(-1.0e6, -1.0e6); // where the rules were last asked which sea lies under the pointer
	mutable int32 SeaAsked = INDEX_NONE;
	int32 LookRegion = INDEX_NONE; // where the army being looked at stands, and whose it is
	FString LookNation;
	void LayOutLook();
	FFleetHand FleetHand; // the fleet in hand as the rules describe it
	TSet<int32> FleetStaying; // (release 2) the fleet tray's cards left behind, by their place in the tray
	TMap<int32, TSet<int32>> PickedDivisions; // (release 2, breakup.ts) by corps: the division tiles picked to be split off
	int32 HeldBackSquadron = INDEX_NONE, HeldBackFrom = INDEX_NONE; // (release 2) a split made for a card not yet answered
	FString HeldBackCard; // that card's id
	FString FleetStayingFor; // and the fleet they were picked on, with its ships: picked afresh when either changes
	int32 FleetHoverSea = INDEX_NONE; // the sea the pointer is nearest, while a fleet is in hand
	FVector2D PointerMap = FVector2D::ZeroVector; // where the pointer is on the map
	bool bPointerOnWater = false;
	void LayOutFleet();
	int32 SeaNear(const FVector2D& MapPoint) const;
	int32 PointedField = INDEX_NONE; // the province whose airfield the pointer is over
	/**
	 * (Brian, 24 November 2026: "its hard to click on an airfield"; the art director's spec, item 1.) The airfield whose
	 * ring, 28 of the screen's units across round the middle of its runway, the pointer is in, at any height of the eye;
	 * Ground is the province under the pointer, whose town wins where its ring overlaps and its middle is nearer.
	 */
	int32 FieldInRing(const FVector2D& Mouse, int32 Ground);
	bool bPointerInOverlap = false; // the pointer is where a town's ring and an airfield's overlap
	bool bOverlapSwapped = false; // and a click there has turned it to the other, until the pointer moves off
	FVector2D OverlapClickAt = FVector2D(-1.0e6, -1.0e6);
	TArray<FVector> FieldOutline; // (item 2) the airfield a click would open, its corners on the table
	int32 HoverTipField = INDEX_NONE;
	int32 StagedHover = INDEX_NONE;
	bool bStagedField = false; // (for the photographer: the staged pointer rests on that province's airfield)
	// (Ruling 86.) The gates' marks as last drawn; the gate under the pointer, and what its box says.
	TArray<TPair<int32, FBox2D>> GateMarks;
	TArray<TPair<int32, FBox2D>> FieldBadges; // (the UI director's HUD read, 22 November: mend 1) the airfields' badges as last drawn, by province
	TArray<TPair<int32, FBox2D>> TownPlateBoxes; // (the UI director's read of run 7, 22 November: mend 3) the towns' plates as last drawn, by province
	int32 StagedGate = INDEX_NONE;
	int32 HoverGate = INDEX_NONE;
	// (The code checker, 17 November.) The padlock under the pointer when the left button went down: the hover is blanked
	// while the button is held, so the click is judged on this, and on the padlock still being under the pointer as it is let go.
	int32 PressGate = INDEX_NONE;
	int32 AlarmShown = 0; // (rules 84.2.6) which of the alarm's rows the tray shows: each press goes on to the next
	FText GateTip, GateTipMarked;
	int32 GateUnder(const FVector2D& Pointer) const;
	FVector2D StagedPointer = FVector2D::ZeroVector;
	int32 HoverTipHand = INDEX_NONE; // what was in hand, and how it was to travel, when the tip was written
	int32 HoverTipRegion = INDEX_NONE;
	bool bTipHushed = false; // (84.3 check, item 4) Tab moved the view: no hover box until the pointer moves
	FVector2D TipHushedAt = FVector2D::ZeroVector; // where the pointer stood when it was hushed
	bool bWatchAll = true; // watch every nation's move at the month's end; V switches to only what touches the player
	TMap<int32, int32> WatchStood; // where each corps stood at the last step watched, to see who has marched
	void OpenScreen1();
	void OpenScreen2();
	void OpenScreen3();
	void OpenScreen4();
	void OpenScreen5();
	bool bScripted = false;
	// (Rules 84.3.1; the second playtester's check of 84.3.) The keys as they are bound: each notes when it put a card
	// down, and the map's own Enter waits the key guard's half second after that. A card put down by the mouse leaves
	// the map's Enter free.
	void EnterKey();
	void EscapeKey();
	void SpaceKey();
	/** (Rules 84.3.2.) Tab, NEXT ARMY's key: on the question before the month's end it is SHOW ME, and it tells the guard. */
	void TabKey();
	bool IsCardUp() const { return IsTableBusy() || EndAsk.bVisible; }
	double KeyShutAt = -1000.0;
	double DoorSince = 0.0; // when the door last came up: it takes no Enter in its first half second
	bool bEnterByKey = false; // (EndMonth was called by the Enter key: on the question before the month's end it is SHOW ME)
	double AtomBackAt = -1000.0; // (release 2, atom.ts) BACK given by Enter or Esc on a bomb raid's card: a doubled Enter ends no month
	bool bAlwaysWatch = false;
	/** (Release 2, tc.Speed.MonthAside) The others' months are being played on the rules' thread: END THE MONTH waits for them. */
	bool bMonthTurning = false;
	bool bByRail = false;
	bool bBySea = false;
	int32 PanelRevision = 0;
	FTownBar TownBar;
	bool bTownCardOpen = false; // (tc.UI.TownCard) the town card stands open over the map
	int32 SelectedRegion = INDEX_NONE;
	int32 SizedForWidth = 0;
	int32 SizedForHeight = 0;
	FVector2D PressPosition = FVector2D::ZeroVector;
	FVector2D RightPressPosition = FVector2D::ZeroVector;
	FVector2D LastMouse = FVector2D::ZeroVector;
};
