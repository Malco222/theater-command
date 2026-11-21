#include "Campaign/CampaignPhotographer.h"
#include "Campaign/ArmyPiece.h"

#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
#include "Campaign/CampaignStepPlayer.h"
#include "TheaterCommand.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderTimer.h"
#include "RHI.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

ACampaignPhotographer::ACampaignPhotographer()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ACampaignPhotographer::Begin(const FString& InFolder)
{
	Folder = FPaths::ConvertRelativePathToFull(InFolder);
	IFileManager::Get().MakeDirectory(*Folder, true);

	auto Add = [this](const TCHAR* Name, const TCHAR* Region, float Zoom, float Yaw = 0.0f, float Tilt = 0.0f) -> FShot&
	{
		FShot& Shot = Shots.AddDefaulted_GetRef();
		Shot.Name = Name;
		Shot.Region = Region;
		Shot.Zoom = Zoom;
		Shot.Yaw = Yaw;
		Shot.Tilt = Tilt;
		return Shot;
	};
	Shots.Reset();
	// The opening screen, as the game starts: the grand campaign, with a country chosen; a later year; the scenarios.
	{
		FShot& Shot = Add(TEXT("00-opening-screen"), TEXT("Frankfurt"), 0.56f);
		Shot.bOpening = true;
		Shot.OpeningTab = 0;
		Shot.OpeningYear = 0;
		Shot.OpeningNation = TEXT("GER");
	}
	{
		FShot& Shot = Add(TEXT("00b-opening-1941"), TEXT("Frankfurt"), 0.56f);
		Shot.bOpening = true;
		Shot.OpeningYear = 2;
		Shot.OpeningNation = TEXT("SOV");
	}
	{
		FShot& Shot = Add(TEXT("00c-opening-scenarios"), TEXT("Frankfurt"), 0.56f);
		Shot.bOpening = true;
		Shot.OpeningTab = 1;
	}
	// The table.
	Add(TEXT("01-whole-map"), TEXT(""), 0.97f);
	Add(TEXT("02-opening-view"), TEXT("Frankfurt"), 0.56f);
	Add(TEXT("03-poland-close"), TEXT("Warsaw"), 0.22f, -18.0f);
	Add(TEXT("04-alps"), TEXT("Tyrol"), 0.2f, 25.0f, 4.0f);
	Add(TEXT("05-paris-picked"), TEXT("Paris"), 0.36f).Select = TEXT("Paris");
	Add(TEXT("06-mediterranean"), TEXT("Sicily"), 0.5f).Select = TEXT("Malta");
	Add(TEXT("07-eastern-frontier"), TEXT("Lwow"), 0.42f, 35.0f);
	Add(TEXT("08-channel-low"), TEXT("Calais"), 0.12f, -30.0f, 6.0f);
	// The game: an army in hand, an attack, its result, a march, and the month after.
	{
		FShot& Shot = Add(TEXT("09-army-in-hand"), TEXT("Berlin"), 0.4f);
		Shot.Select = Shot.Army = TEXT("Farther Pomerania");
		Shot.Route = TEXT("Hamburg");
	}
	{
		// A click on ground the army can reach only offers it: the tray asks for the word.
		FShot& Shot = Add(TEXT("09b-clicked-once"), TEXT("Berlin"), 0.4f);
		Shot.Select = Shot.Army = TEXT("Hamburg");
		Shot.Click = TEXT("Mecklenburg");
	}
	{
		// The second click on the same ground sends it.
		FShot& Shot = Add(TEXT("09c-clicked-twice"), TEXT("Berlin"), 0.4f);
		Shot.bKeep = true;
		Shot.Click = TEXT("Mecklenburg");
	}
	{
		// And a click on the army's own ground puts it down.
		FShot& Shot = Add(TEXT("09d-put-down"), TEXT("Berlin"), 0.4f);
		Shot.bKeep = true;
		Shot.Click = TEXT("Mecklenburg");
	}
	{
		// The pointer resting on ground the army in hand could attack: the tip gives the odds before anything is ordered.
		FShot& Shot = Add(TEXT("09d-tip-odds"), TEXT("Pomorze"), 0.34f);
		Shot.Army = TEXT("Farther Pomerania");
		Shot.Route = Shot.Hover = TEXT("Pomorze");
	}
	{
		// And resting on ground it could simply march to.
		FShot& Shot = Add(TEXT("09e-tip-march"), TEXT("Berlin"), 0.4f);
		Shot.Army = TEXT("Farther Pomerania");
		Shot.Route = Shot.Hover = TEXT("Hamburg");
	}
	{
		// What stands round an army's base says what it is about: entrenched behind sandbags and wire, in camp by
		// its fire, supplied by its lorries, or cut off; and one that is still digging.
		FShot& Shot = Add(TEXT("09f-armies-at-a-glance"), TEXT("Grenzmark"), 0.22f);
		Shot.Dress = TEXT("Farther Pomerania:2:0:1;Grenzmark:0:1:1;Pomorze:1:0:2;Poznan:2:1:1");
	}
	{
		FShot& Shot = Add(TEXT("09g-at-a-glance-wide"), TEXT("Poznan"), 0.4f);
		Shot.Dress = TEXT("Farther Pomerania:2:0:1;Grenzmark:0:1:1;Pomorze:1:0:2;Poznan:2:1:1;Lower Silesia:2:0:1;Lodz:0:1:2");
	}
	{
		FShot& Shot = Add(TEXT("10-face-off"), TEXT("Pomorze"), 0.34f);
		Shot.Army = TEXT("Farther Pomerania");
		Shot.Attack = TEXT("Pomorze");
	}
	{
		FShot& Shot = Add(TEXT("11-battle-result"), TEXT("Pomorze"), 0.34f);
		Shot.bKeep = Shot.bConfirm = true;
	}
	{
		FShot& Shot = Add(TEXT("12-ground-taken"), TEXT("Pomorze"), 0.34f);
		Shot.bKeep = Shot.bClose = true;
	}
	// The scars the battle left at Pomorze, close to; and the scars of battles of each size and age, side by side.
	Add(TEXT("12b-scars-of-the-battle"), TEXT("Pomorze"), 0.2f);
	{
		FShot& Shot = Add(TEXT("12c-scars-by-size-and-age"), TEXT("Lodz"), 0.36f);
		Shot.Scars = TEXT("Poznan:0:3;Lodz:0:2;Kielce:0:1;Mlawa:1:3;Polish Silesia:2:3;Lublin:3:3");
	}
	{
		FShot& Shot = Add(TEXT("13-army-on-the-march"), TEXT("Berlin"), 0.4f);
		Shot.Army = TEXT("Neumark");
		Shot.March = TEXT("Magdeburg");
	}
	{
		// The flag picked up by hand and carried: held over ground it can reach, then let go there.
		// (The Hamburg garrison has marched by now, so this is the corps in the Eifel.)
		FShot& Shot = Add(TEXT("13b-flag-in-the-hand"), TEXT("Rhineland"), 0.3f);
		Shot.Army = TEXT("Eifel");
		Shot.Carry = TEXT("Rhineland");
	}
	{
		FShot& Shot = Add(TEXT("13c-flag-set-down"), TEXT("Rhineland"), 0.3f);
		Shot.bKeep = true;
		Shot.Drop = TEXT("Rhineland");
	}
	{
		// An army on a frontier at peace: the ground across it is amber, because going there means war.
		FShot& Shot = Add(TEXT("13d-neutral-frontier"), TEXT("Gelderland"), 0.45f);
		Shot.Army = TEXT("Westphalia");
		Shot.Carry = TEXT("Gelderland");
	}
	{
		// The flag is set down there: the game says what would follow, and asks before any war begins.
		FShot& Shot = Add(TEXT("13d2-declare-war"), TEXT("Gelderland"), 0.45f);
		Shot.bKeep = true;
		Shot.Drop = TEXT("Gelderland");
	}
	{
		// The answer is yes: war is declared, and the army goes on against the place it was sent to.
		FShot& Shot = Add(TEXT("13e-war-declared"), TEXT("Gelderland"), 0.45f);
		Shot.bKeep = true;
		Shot.bWar = true;
	}
	{
		FShot& Shot = Add(TEXT("14-next-month"), TEXT("Warsaw"), 0.5f);
		Shot.Months = 1;
	}
	Add(TEXT("14b-menu-with-save"), TEXT("Warsaw"), 0.5f).bMenu = true;
	{
		// The options page of the menu, with two of the choices changed.
		FShot& Shot = Add(TEXT("14b2-options"), TEXT("Warsaw"), 0.5f);
		Shot.bMenu = true;
		Shot.OpeningTab = 2;
		Shot.Options = TEXT("narrator,loud");
	}
	Add(TEXT("14e-guide"), TEXT("Warsaw"), 0.5f).bGuide = true;
	Add(TEXT("14e2-lesson-guide"), TEXT("Warsaw"), 0.5f).bLessonGuide = true;
	{
		// The opening flight: its title, an enemy's frontier, and the objective. Then put away.
		FShot& Title = Add(TEXT("14j-flight-title"), TEXT("Berlin"), 0.5f);
		Title.Flight = -1;
		Title.Wait = 1.0f;
		FShot& Foe = Add(TEXT("14k-flight-enemy"), TEXT("Berlin"), 0.5f);
		Foe.Flight = 1;
		Foe.Wait = 3.0f;
		FShot& Aim = Add(TEXT("14l-flight-objective"), TEXT("Berlin"), 0.5f);
		Aim.Flight = 5;
		Aim.Wait = 3.0f;
		Add(TEXT("14m-after-flight"), TEXT("Berlin"), 0.5f).bDismiss = true;
	}
	{
		FShot& Shot = Add(TEXT("14h-newsreel"), TEXT("Lodz"), 0.36f);
		Shot.bNewsreel = true;
		Shot.Wait = 1.5f;
	}
	Add(TEXT("14i-after-newsreel"), TEXT("Warsaw"), 0.5f);
	Add(TEXT("14f-nation-fallen"), TEXT("Warsaw"), 0.5f).bAnnounce = true;
	Add(TEXT("14g-after"), TEXT("Warsaw"), 0.5f).bDismiss = true;
	{
		// A province of one's own: what it holds, what can be raised there, and something put on order.
		FShot& Shot = Add(TEXT("14c-province"), TEXT("Berlin"), 0.4f);
		Shot.Select = TEXT("Berlin");
		Shot.Order = TEXT("mtn");
	}
	{
		FShot& Shot = Add(TEXT("14d-by-rail"), TEXT("Saxony"), 0.5f);
		Shot.Army = TEXT("Rhineland");
		Shot.bRail = true;
	}
	{
		// Attack again, then end the month and catch the other nations' month being played out.
		FShot& Shot = Add(TEXT("15-attack-again"), TEXT("Pomorze"), 0.34f);
		Shot.Army = TEXT("Grenzmark");
		Shot.Attack = TEXT("Pomorze");
	}
	{
		FShot& Shot = Add(TEXT("16-watching-the-others"), TEXT("Pomorze"), 0.45f);
		Shot.bKeep = Shot.bConfirm = Shot.bClose = Shot.bWatch = true;
	}

	// "-TheaterStart=4,0" begins from another scenario on the opening screen (here the fifth, its first side) and
	// takes the pictures that show what units have made of themselves by then.
	FString Start;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterStart="), Start, false)) // false: the comma is part of the value
	{
		FString Group, Side;
		if (Start.Split(TEXT(","), &Group, &Side))
		{
			StartGroup = FCString::Atoi(*Group);
			StartSide = FCString::Atoi(*Side);
		}
		Shots.Reset();
		Add(TEXT("20-honours-on-the-standards"), TEXT("Masuria"), 0.2f, -10.0f, 4.0f).Army = TEXT("Masuria");
		Add(TEXT("21-army-with-stories"), TEXT("Masuria"), 0.36f).Army = TEXT("Masuria");
		{
			FShot& Shot = Add(TEXT("22-service-record"), TEXT("Masuria"), 0.36f);
			Shot.Army = TEXT("Masuria");
			Shot.bRecord = true;
		}
		Add(TEXT("23-roll-of-honour"), TEXT("Masuria"), 0.36f).bRoll = true;
	}

	// "-TheaterGrand=SOV,2" begins the grand campaign as a country (here the Soviet Union) in one of its years
	// (here the third on the list, January 1941), and looks at the whole table and then at that country's capital.
	FString Grand;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterGrand="), Grand, false))
	{
		FString Year;
		if (Grand.Split(TEXT(","), &GrandNation, &Year))
		{
			GrandYear = FCString::Atoi(*Year);
		}
		else
		{
			GrandNation = Grand;
		}
		Shots.Reset();
		Add(TEXT("30-grand-whole-map"), TEXT(""), 0.97f);
		Add(TEXT("31-grand-capital"), TEXT("@capital"), 0.45f).Select = TEXT("@capital");
		{
			FShot& Shot = Add(TEXT("31a-province-recruit"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.ProvinceTab = 1;
		}
		{
			FShot& Shot = Add(TEXT("31a2-province-works"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.ProvinceTab = 2;
		}
		{
			// The adviser's first steps, with an army picked up so the first is ticked.
			FShot& Shot = Add(TEXT("31t-adviser"), TEXT("@capital"), 0.45f);
			Shot.bAdviser = true;
			Shot.Select = Shot.Army = TEXT("Upper Silesia");
		}
		if (GrandNation == TEXT("GER"))
		{
			// A joint army: the Slovak corps beside the German one asked to march with it.
			FShot& Shot = Add(TEXT("31u-joint-army"), TEXT("Moravia"), 0.4f);
			Shot.bJoin = true;
		}
		if (GrandNation == TEXT("GER"))
		{
			// Armies that are not the player's to move, looked at: an enemy's that nobody has identified, and a garrison of his own.
			Add(TEXT("31v-enemy-army-looked-at"), TEXT("Lodz"), 0.4f).Look = TEXT("Lodz");
			Add(TEXT("31w-own-garrison-looked-at"), TEXT("Hamburg"), 0.4f).Look = TEXT("Hamburg");
		}
		Add(TEXT("31m-forces"), TEXT("@capital"), 0.45f).Room = TEXT("forces");
		// An enemy capital picked: the prize for taking it. And the wealth view of the whole map.
		Add(TEXT("31p-prize"), TEXT("Warsaw"), 0.4f).Select = TEXT("Warsaw");
		Add(TEXT("31q-wealth-view"), TEXT(""), 0.97f).bWealth = true;
		Add(TEXT("31r-wealth-close"), TEXT("Ruhr"), 0.55f).bWealth = true;
		Add(TEXT("31s-friend-and-foe"), TEXT(""), 0.97f).bSides = true;
		Add(TEXT("31o-objectives"), TEXT("@capital"), 0.45f).Room = TEXT("goals");
		{
			// An army in hand: a card for each corps, its divisions by name, its morale and its rest.
			FShot& Shot = Add(TEXT("31n-army-cards"), TEXT("Upper Silesia"), 0.4f);
			Shot.Select = Shot.Army = TEXT("Upper Silesia");
		}
		Add(TEXT("31b-ledger"), TEXT("@capital"), 0.45f).bLedger = true;
		Add(TEXT("31e-diplomacy"), TEXT("@capital"), 0.45f).bDiplomacy = true;
		Add(TEXT("31f-research"), TEXT("@capital"), 0.45f).bResearch = true;
		Add(TEXT("31g-generals"), TEXT("@capital"), 0.45f).bGenerals = true;
		{
			// The airfield at the capital, then a raid planned from it and ordered: the aircraft are on their way.
			FShot& Shot = Add(TEXT("31i-airfield"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("airfield");
		}
		{
			// One air group's orders: what it does between raids, and the raids it can fly.
			FShot& Shot = Add(TEXT("31j-air-group"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("airfield");
			Shot.RoomAct = TEXT("@row");
		}
		{
			FShot& Shot = Add(TEXT("31j2-raid-targets"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("airfield");
			Shot.RoomAct = TEXT("open:raid:bomb");
		}
		{
			// The raid planned: who flies it, who could join, what rises to meet it.
			FShot& Shot = Add(TEXT("31j3-raid-plan"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("airfield");
			Shot.RoomAct = TEXT("open:raid:bomb|@row");
		}
		{
			// The raid flown: the groups are on their way from their airfields.
			FShot& Shot = Add(TEXT("31k-raid-flying"), TEXT("@capital"), 0.7f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("airfield");
			Shot.RoomAct = TEXT("open:raid:bomb|@row|@top");
			Shot.Wait = 2.6f;
		}
		{
			// (Rules version 72) The crossings of a defended sea: each shore, what bars it, why, and what would help.
			Add(TEXT("35d-sea-crossings"), TEXT("@capital"), 0.5f).Room = TEXT("crossings");
		}
		{
			// One of them named as the plan.
			FShot& Shot = Add(TEXT("35e-crossing-planned"), TEXT("@capital"), 0.5f);
			Shot.Room = TEXT("crossings");
			Shot.RoomAct = TEXT("@row");
		}
		{
			// The raid from close by: the bombers and their escort half way out, as near as the player can go.
			FShot& Shot = Add(TEXT("31k2-raid-close"), TEXT("@capital"), 0.0f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("airfield");
			Shot.RoomAct = TEXT("open:raid:bomb|@row|@top");
			Shot.bCloseRoom = true;
			Shot.RaidAt = 0.22f; // (half way out, before the defender's fighters are in among them)
			FParse::Value(FCommandLine::Get(), TEXT("TheaterRaidAt="), Shot.RaidAt); // ("-TheaterRaidAt=0.45" catches it over its target, where the guns are)
			Shot.Tilt = -25.0f; // (looked down on more steeply than the closest view does of itself, so that no army's flag stands in front)
			// ("-TheaterRaidTilt=0 -TheaterRaidTurn=40" set the eye's tilt and heading instead: for a flag in front of a beam)
			FParse::Value(FCommandLine::Get(), TEXT("TheaterRaidTilt="), Shot.Tilt);
			FParse::Value(FCommandLine::Get(), TEXT("TheaterRaidTurn="), Shot.Yaw);
		}
		{
			// The same raid from the top, where a burst of flak is a mark of smoke and no more.
			FShot& Shot = Add(TEXT("31k3-raid-top"), TEXT("@capital"), 0.85f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("airfield");
			Shot.RoomAct = TEXT("open:raid:bomb|@row|@top");
			Shot.bCloseRoom = true;
			Shot.RaidAt = 0.22f;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterRaidAt="), Shot.RaidAt);
		}
		{
			// A raid on troops, caught over them.
			FShot& Shot = Add(TEXT("31l-raid-met"), TEXT("@capital"), 0.7f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("airfield");
			Shot.RoomAct = TEXT("open:raid:ground|@row|@top");
			Shot.Wait = 5.2f;
		}
		{
			// Towns you build up (version 52): the province panel shows the capital's plots, and its town plan.
			FShot& Shot = Add(TEXT("38a-town-plan"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("town:0");
		}
		{
			// A market hall ordered there, then the panel again with the plot taken.
			FShot& Shot = Add(TEXT("38b-building-ordered"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("town:0");
			Shot.RoomAct = TEXT("civic:0:market");
		}
		{
			FShot& Shot = Add(TEXT("38c-plots-on-the-panel"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
		}
		// (the market hall ordered two pictures ago is seen going up at the town's edge)
		Add(TEXT("38c2-builders-at-work"), TEXT("@capital"), 0.24f, 0.0f);
		{
			// The buildings themselves on the table: every kind at its first level, at its third, and a mixture.
			Add(TEXT("38g-buildings-first-level"), TEXT("@capital"), 0.3f).Civic = 1;
			Add(TEXT("38h-buildings-third-level"), TEXT("@capital"), 0.3f).Civic = 3;
			FShot& Mixed = Add(TEXT("38i-buildings-going-up"), TEXT("@capital"), 0.24f, 30.0f);
			Mixed.Civic = 4;
			Add(TEXT("38j-buildings-at-a-town"), TEXT("Saxony"), 0.26f).Civic = 2;
		}
		{
			// The town's bar of pictures: the capital with its market going up; four months on, the market built and
			// the choices for a free plot; a second building ordered from them; and the raising of the first.
			Add(TEXT("38k-town-bar"), TEXT("@capital"), 0.45f).Select = TEXT("@capital");
			FShot& Later = Add(TEXT("38l-town-bar-choices"), TEXT("@capital"), 0.45f);
			Later.Months = 4;
			Later.Select = TEXT("@capital");
			Later.TownPlot = 1;
			FShot& Ordered = Add(TEXT("38m-town-bar-ordered"), TEXT("@capital"), 0.45f);
			Ordered.Select = TEXT("@capital");
			Ordered.TownPlot = 1;
			Ordered.TownOrder = TEXT("school");
			FShot& Raise = Add(TEXT("38n-town-bar-raise"), TEXT("@capital"), 0.45f);
			Raise.Select = TEXT("@capital");
			Raise.TownPlot = 0;
		}
		{
			FShot& Shot = Add(TEXT("38d-towns-to-build-up"), TEXT("@capital"), 0.45f);
			Shot.Room = TEXT("towns");
		}
		{
			// Trade agreements, and the routes drawn in gold on the table behind.
			FShot& Shot = Add(TEXT("38e-trade"), TEXT("@capital"), 0.62f);
			Shot.Room = TEXT("trade");
		}
		{
			FShot& Shot = Add(TEXT("38f-trade-routes"), TEXT("@capital"), 0.72f);
		}
		{
			// The government and its war cabinet (version 57): trust, the directive, the four offices, the decrees.
			FShot& Shot = Add(TEXT("43a-the-cabinet"), TEXT("@capital"), 0.62f);
			Shot.Room = TEXT("cabinet");
		}
		{
			FShot& Shot = Add(TEXT("43b-after-the-cabinet"), TEXT("@capital"), 0.62f);
		}
		{
			// The options page in a grand campaign, where the war cabinet can be switched off (version 59).
			FShot& Shot = Add(TEXT("43c-options-with-the-cabinet"), TEXT("@capital"), 0.62f);
			Shot.bMenu = true;
			Shot.OpeningTab = 2;
		}
		{
			FShot& Shot = Add(TEXT("43d-after-the-options"), TEXT("@capital"), 0.62f);
			Shot.bDismiss = true;
		}
		{
			// The secret service, with the capital selected and an agent recruited.
			FShot& Shot = Add(TEXT("31h-secret-service"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.Room = TEXT("spies");
			Shot.RoomAct = TEXT("spy:recruit");
		}
		{
			FShot& Shot = Add(TEXT("31c-arsenal"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.bArsenal = true;
		}
		{
			FShot& Shot = Add(TEXT("31d-arsenal-ordered"), TEXT("@capital"), 0.45f);
			Shot.Select = TEXT("@capital");
			Shot.bArsenal = true;
			Shot.Order = TEXT("corps:art");
		}
		// A province on a river with a railway through it: what its panel says, and a bridge blown.
		Add(TEXT("33-river-province"), TEXT("Neumark"), 0.3f).Select = TEXT("Neumark");
		{
			FShot& Shot = Add(TEXT("33b-bridge-blown"), TEXT("Neumark"), 0.3f);
			Shot.Select = TEXT("Neumark");
			Shot.Order = TEXT("work:blow:24");
		}
		{
			FShot& Shot = Add(TEXT("33c-railway-cut"), TEXT("Neumark"), 0.12f);
			Shot.Select = TEXT("Neumark");
			Shot.Order = TEXT("work:cutrail");
		}
		// A fleet and an air fleet, each with the orders it can be given; and one given.
		Add(TEXT("34-fleet-orders"), TEXT("@fleet4"), 0.25f).Force = TEXT("fleet:5");
		// A fleet picked up like an army: its ships as cards, the seas it can reach ringed; and then sent to one.
		Add(TEXT("34d-fleet-in-hand"), TEXT("@capital"), 0.5f).bFleet = true;
		Add(TEXT("34e-fleet-in-hand-wide"), TEXT("@capital"), 0.8f).bFleet = true;
		if (GrandNation == TEXT("UK"))
		{
			// The navy as pieces: the fleet's room of orders, the question before a coast is shelled, and the shelling.
			{
				FShot& Shot = Add(TEXT("34g-fleet-orders-room"), TEXT("@capital"), 0.5f);
				Shot.bFleet = true;
				Shot.FleetDo = TEXT("orders");
			}
			{
				FShot& Shot = Add(TEXT("34h-shell-question"), TEXT("@capital"), 0.5f);
				Shot.bFleet = true;
				Shot.FleetDo = TEXT("shell");
			}
			{
				FShot& Shot = Add(TEXT("34i-coast-shelled"), TEXT("@capital"), 0.5f);
				Shot.bFleet = true;
				Shot.FleetDo = TEXT("fire");
				Shot.Wait = 1.2f;
			}
		}
		{
			FShot& Shot = Add(TEXT("34f-fleet-sent"), TEXT("@capital"), 0.6f);
			Shot.bFleet = true;
			Shot.bFleetSails = true;
			Shot.Wait = 1.5f;
		}
		{
			FShot& Shot = Add(TEXT("34b-fleet-sails"), TEXT("@fleet4"), 0.25f);
			Shot.Force = TEXT("fleet:5");
			Shot.Order = TEXT("fleet:5:6:patrol");
		}
		{
			FShot& Shot = Add(TEXT("34c-air-orders"), TEXT("Berlin"), 0.25f);
			Shot.Force = TEXT("air:1");
			Shot.Order = TEXT("air:1:bomb:38");
		}
		if (GrandNation == TEXT("GER"))
		{
			// Cities and sieges (version 54): an attack that wins the country but not the town, the province's
			// panel with how the siege stands and the order to storm it, and the smoke over the town.
			{
				FShot& Shot = Add(TEXT("41a-attack-on-a-town"), TEXT("Danzig Corridor"), 0.3f);
				Shot.Army = TEXT("Farther Pomerania");
				Shot.Attack = TEXT("Danzig Corridor");
			}
			{
				FShot& Shot = Add(TEXT("41b-the-town-holds"), TEXT("Danzig Corridor"), 0.3f);
				Shot.bKeep = true;
				Shot.bConfirm = true;
			}
			{
				FShot& Shot = Add(TEXT("41c-siege-orders"), TEXT("Danzig Corridor"), 0.26f);
				Shot.bClose = true;
				Shot.Select = TEXT("Danzig Corridor");
				Shot.ProvinceTab = 2;
				Shot.Wait = 2.0f;
			}
			{
				FShot& Shot = Add(TEXT("41d-siege-panel"), TEXT("Danzig Corridor"), 0.2f);
				Shot.Select = TEXT("Danzig Corridor");
				Shot.Wait = 2.0f;
			}
			{
				// Prizes outside the town (version 56): the places of a province in its panel, the orders on its
				// works tab, and the marks on the map: a fire where one is wrecked, a pennant where a besieger holds one.
				FShot& Shot = Add(TEXT("42a-places-of-their-own"), TEXT("@capital"), 0.4f);
				Shot.Select = TEXT("@capital");
				Shot.Wait = 1.5f;
			}
			{
				FShot& Shot = Add(TEXT("42b-wreck-and-scorch"), TEXT("@capital"), 0.4f);
				Shot.Select = TEXT("@capital");
				Shot.ProvinceTab = 2;
				Shot.Wait = 1.5f;
			}
			{
				FShot& Shot = Add(TEXT("42c-prizes-on-the-map"), TEXT("Danzig Corridor"), 0.22f);
				Shot.Prizes = TEXT("Danzig Corridor:GER:1;Pomorze:GER:0;Grenzmark:-:1");
				Shot.Wait = 2.0f;
			}
		}
		{
			// The title card that opens a year: four months on from September 1939. Then put away.
			FShot& Shot = Add(TEXT("40a-year-card"), TEXT("@capital"), 0.5f);
			Shot.Months = 4;
			Shot.bYearCard = true;
			Add(TEXT("40b-after-year-card"), TEXT("@capital"), 0.5f).bDismiss = true;
		}
		if (GrandNation == TEXT("UK"))
		{
			// Carriers and the fog at sea (version 53): a fleet with carriers in hand, the room of its aircraft, a
			// search flown from the ships, and an enemy force known only from a report.
			Add(TEXT("39a-carrier-fleet-in-hand"), TEXT("@capital"), 0.62f).bFleet = true;
			Add(TEXT("39b-carrier-aircraft"), TEXT("@capital"), 0.62f).Room = TEXT("wings:1");
			Add(TEXT("39c-wing-orders"), TEXT("@capital"), 0.62f).Room = TEXT("group:53");
			{
				FShot& Shot = Add(TEXT("39d-search-flown"), TEXT("@fleet0"), 0.5f);
				Shot.Room = TEXT("search:53");
				Shot.RoomAct = TEXT("search:53:9");
				Shot.Wait = 2.2f;
			}
			{
				FShot& Shot = Add(TEXT("39e-fleet-only-reported"), TEXT("@report"), 0.3f);
				Shot.Force = TEXT("fleet:6");
			}
		}
		if (GrandNation == TEXT("UK"))
		{
			// An army sent by sea: where it can go, the question before a landing, and the landing made.
			// ("-TheaterLanding=Calais:9:Kent" pictures another shore, after that many months have been played, with
			// the army of another province: the landing card's amber verdicts come only later in a war, and a great
			// army is more than the ships can carry.)
			// (Since rules 85 Hamburg is no beach from London in September 1939: the pictures showed "They cannot get
			// there this month." in place of the landing card. The one beach the rules offer then is Schleswig-Holstein.)
			FString LandAt = TEXT("Schleswig-Holstein"), LandFrom = TEXT("London");
			int32 LandMonths = 0;
			{
				FString LandingAsked;
				TArray<FString> LandingParts;
				if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLanding="), LandingAsked, false) && LandingAsked.ParseIntoArray(LandingParts, TEXT(":")) > 0)
				{
					LandAt = LandingParts[0];
					LandMonths = LandingParts.Num() > 1 ? FMath::Clamp(FCString::Atoi(*LandingParts[1]), 0, 80) : 0;
					LandFrom = LandingParts.Num() > 2 ? LandingParts[2] : LandAt == TEXT("@amber") ? FString(TEXT("@amber")) : LandFrom;
				}
			}
			{
				FShot& Shot = Add(TEXT("35-by-sea"), *LandAt, 0.62f);
				Shot.Army = LandFrom;
				Shot.bSea = true;
				Shot.Months = LandMonths;
				Shot.bDismiss = LandMonths > 0; // (a nation fallen in those months is told on a screen of its own)
			}
			{
				FShot& Shot = Add(TEXT("35b-landing-asked"), *LandAt, 0.5f);
				Shot.bKeep = true;
				Shot.Attack = LandAt;
			}
			{
				FShot& Shot = Add(TEXT("35c-landing-made"), *LandAt, 0.4f);
				Shot.bKeep = true;
				Shot.bWar = true;
			}

		}
		if (GrandNation == TEXT("SWE"))
		{
			// A small country's own path: asked about founding the League with Denmark, the League founded, and its ground on the map.
			{
				FShot& Shot = Add(TEXT("38-path-asked"), TEXT("@capital"), 0.45f);
				Shot.bDiplomacy = true;
				Shot.Path = TEXT("found:DEN");
			}
			{
				FShot& Shot = Add(TEXT("38b-league-founded"), TEXT("@capital"), 0.45f);
				Shot.bDiplomacy = true;
				Shot.Path = TEXT("found:DEN");
				Shot.PathPresses = 2;
			}
			Add(TEXT("38c-league-on-the-map"), TEXT("Jutland"), 0.8f);
		}
		if (GrandNation == TEXT("GER"))
		{
			// A neutral frontier in the grand campaign: the ground across it in amber, and the question with its surprise.
			{
				FShot& Shot = Add(TEXT("36-neutral-frontier"), TEXT("Gelderland"), 0.45f);
				Shot.Army = TEXT("Westphalia");
				Shot.Carry = TEXT("Gelderland");
			}
			{
				FShot& Shot = Add(TEXT("36b-war-asked"), TEXT("Gelderland"), 0.45f);
				Shot.bKeep = true;
				Shot.Drop = TEXT("Gelderland");
			}
		}
		{
			// (Rules version 66) How deep the player plays: the staff's room; research handed to the staff from it; and
			// the design offices with the bar that says who runs them. (The month that follows is then ended with a
			// desk in the staff's hands, so that the staff's own month is run in every check.)
			Add(TEXT("44a-your-staff"), TEXT("@capital"), 0.62f).Room = TEXT("staff");
			FShot& Handed = Add(TEXT("44b-research-handed-over"), TEXT("@capital"), 0.62f);
			Handed.Room = TEXT("staff");
			Handed.RoomAct = TEXT("desk:research:staff");
			Add(TEXT("44c-research-with-the-staff"), TEXT("@capital"), 0.45f).bResearch = true;
		}
		{
			// "-TheaterMonths=24" plays that many months before this picture: a long run, to see that nothing breaks.
			int32 Long = 1;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterMonths="), Long);
			Add(TEXT("32-grand-next-month"), TEXT("@capital"), 0.6f).Months = FMath::Clamp(Long, 1, 80);
			Add(TEXT("32b-whole-map-later"), TEXT(""), 0.97f); // the whole table as it stands after those months: the season, the weather
		}
		{
			// The month ended and caught while the others' moves are shown: the strip of flags, and the march.
			FShot& Shot = Add(TEXT("37-month-replay"), TEXT("@capital"), 0.5f);
			Shot.bKeep = Shot.bWatch = true;
			Shot.Wait = -2.35f; // taken early, while the replay is still going
		}
	}

	// "-TheaterWorksAt=Hamburg" takes two pictures of that region with a full set of works standing in it.
	FString WorksAt;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterWorksAt="), WorksAt, false))
	{
		Shots.Reset();
		Add(TEXT("40-works"), *WorksAt, 0.16f).bWorks = true;
		Add(TEXT("40b-works-close"), *WorksAt, 0.1f, 200.0f, 8.0f).bWorks = true;
	}

	// "-TheaterLook=Ardennes:0.1:20:4;Tobruk:0.2" takes one picture of each place named instead: a region,
	// then how far out (0 close, 1 far), which way the view is turned, and how far it is tipped.
	FString Look;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLook="), Look, false))
	{
		Shots.Reset();
		TArray<FString> Places;
		Look.ParseIntoArray(Places, TEXT(";"));
		for (const FString& Place : Places)
		{
			TArray<FString> Parts;
			Place.ParseIntoArray(Parts, TEXT(":"));
			if (Parts.Num() > 0)
			{
				const float Zoom = Parts.IsValidIndex(1) ? FCString::Atof(*Parts[1]) : 0.3f;
				auto& Asked = Add(*FString::Printf(TEXT("look%d-%s-%s"), Shots.Num() + 1, *Parts[0].Replace(TEXT(" "), TEXT("-")).Replace(TEXT("@"), TEXT("")), Parts.IsValidIndex(1) ? *Parts[1] : TEXT("0.3")), *Parts[0], Zoom,
					Parts.IsValidIndex(2) ? FCString::Atof(*Parts[2]) : 0.0f, Parts.IsValidIndex(3) ? FCString::Atof(*Parts[3]) : 0.0f);
				Asked.Lens = Parts.IsValidIndex(4) ? FCString::Atof(*Parts[4]) : 0.0f; // a fifth part is the width of the view in degrees: small for a long lens
				if (Parts.IsValidIndex(5))
				{
					// a sixth part is a province whose army is picked up first ("keep" leaves the army of the picture before
					// in hand, "fleet" picks up the player's largest fleet)
					if (Parts[5] == TEXT("keep"))
					{
						Asked.bKeep = true;
					}
					else if (Parts[5].StartsWith(TEXT("fleet")))
					{
						Asked.bFleet = true; // ("fleet" the largest; "fleet0" the first of the player's own, and so on)
						Asked.FleetPick = Parts[5].Len() > 5 ? FCString::Atoi(*Parts[5].Mid(5)) : -1;
					}
					else
					{
						Asked.Army = Parts[5];
					}
					if (Parts.IsValidIndex(6) && Parts[6] != TEXT("-"))
					{
						Asked.Route = Parts[6]; // and a seventh is a province the road is shown to, as when the pointer rests on it
					}
					if (Parts.IsValidIndex(7) && Parts[7] != TEXT("-"))
					{
						Asked.March = Parts[7]; // and an eighth is a province the army is ordered to just before the picture
					}
				}
			}
		}
	}

	// "-TheaterReview" takes the art director's fixed views instead, so that each build can be held against his
	// target pictures: central Europe from high up, Germany and Poland at working height, the Alps and northern
	// Italy, Berlin close, and an army piece close. Run it by day, and again with "-TheaterNight".
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterReview")))
	{
		Shots.Reset();
		Add(TEXT("review-1-far"), TEXT("@700,979"), 0.803f, 0.0f, 20.9f).Lens = 34.1f;
		Add(TEXT("review-2-working"), TEXT("@623,747"), 0.70f);
		Add(TEXT("review-3-alps"), TEXT("@642,1034"), 0.611f, 0.0f, 0.7f).Lens = 36.5f;
		Add(TEXT("review-4-berlin"), TEXT("Berlin"), 0.3f);
		Add(TEXT("review-5-army"), TEXT("Grenzmark"), 0.12f, -20.0f, 4.0f);
	}

	// "-TheaterLookMonths=1" plays that many months before the first "-TheaterLook" picture: ground changes hands, sieges go on.
	int32 LookMonths = 0;
	if (!Look.IsEmpty() && FParse::Value(FCommandLine::Get(), TEXT("TheaterLookMonths="), LookMonths) && Shots.Num() > 0)
	{
		Shots[0].Months = FMath::Clamp(LookMonths, 0, 80);
		Shots[0].bDismiss = true; // (a nation fallen in those months is told on a screen of its own, which would cover the picture)
	}
	// "-TheaterLookLater=6" plays that many months more before the second "-TheaterLook" picture: what was ordered
	// before the first has come by then.
	int32 LookLater = 0;
	if (!Look.IsEmpty() && FParse::Value(FCommandLine::Get(), TEXT("TheaterLookLater="), LookLater) && Shots.Num() > 1)
	{
		Shots[1].Months = FMath::Clamp(LookLater, 0, 80);
		Shots[1].bDismiss = true;
	}

	// "-TheaterRoom=plan:bomb:12:5.6" opens that room of the rules' own over the first "-TheaterLook" picture.
	FString RoomAsked;
	if (!Look.IsEmpty() && FParse::Value(FCommandLine::Get(), TEXT("TheaterRoom="), RoomAsked, false) && Shots.Num() > 0)
	{
		Shots[0].Room = RoomAsked;
		// (and "-TheaterRoomAct=@top" presses something in it: "@top" flies the raid on a raid card)
		FParse::Value(FCommandLine::Get(), TEXT("TheaterRoomAct="), Shots[0].RoomAct, false);
	}

	// "-TheaterSelect=Ruhr" picks that province for the first "-TheaterLook" picture (its card is shown), and
	// "-TheaterHover=Ruhr" rests the pointer on one (its tip is shown).
	if (!Look.IsEmpty() && Shots.Num() > 0)
	{
		FParse::Value(FCommandLine::Get(), TEXT("TheaterSelect="), Shots[0].Select, false);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterHover="), Shots[0].Hover, false);
		// ("-TheaterLookWait=30" waits that many seconds more before the first picture: for something that takes time, as a train's run)
		float LookWait = 0.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLookWait="), LookWait))
		{
			Shots[0].Wait += FMath::Clamp(LookWait, 0.0f, 120.0f);
		}
		// ("-TheaterArmy=Ruhr" picks up the army there, and "-TheaterRoute=Rhineland" draws the road to a place as the
		// pointer would. "-TheaterSpend=Ruhr>Rhineland>Ruhr" first marches the army in the first place through the
		// others in turn, to use up its moves: for pictures of an army with none left.)
		FParse::Value(FCommandLine::Get(), TEXT("TheaterArmy="), Shots[0].Army, false);
		// ("-TheaterOrderTo=Mecklenburg" gives the army in hand that order: for the card after a march is kept.)
		FParse::Value(FCommandLine::Get(), TEXT("TheaterOrderTo="), Shots[0].Attack, false);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterRoute="), Shots[0].Route, false);
	}

	// "-TheaterMoment=Warsaw" takes four stills of a moment at the front over that region, as when its capital
	// falls: as it starts, half way down, at its lowest, and with the eye handed back.
	FString MomentAt;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterMoment="), MomentAt, false))
	{
		Shots.Reset();
		static const TCHAR* Names[4] = { TEXT("moment-1-start"), TEXT("moment-2-half-way-down"), TEXT("moment-3-lowest"), TEXT("moment-4-handed-back") };
		static const float Waits[4] = { -0.5f, -0.2f, 1.6f, 4.9f };
		for (int32 K = 0; K < 4; ++K)
		{
			FShot& Shot = Add(Names[K], TEXT("@623,747"), 0.70f);
			Shot.Moment = MomentAt;
			Shot.Wait = Waits[K];
		}
	}

	// "-TheaterOnly=13b,13c" takes only the pictures whose names begin with one of these.
	FString Only;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterOnly="), Only, false))
	{
		TArray<FString> Wanted;
		Only.ParseIntoArray(Wanted, TEXT(","));
		Shots.RemoveAll([&Wanted](const FShot& Shot)
		{
			return !Wanted.ContainsByPredicate([&Shot](const FString& Prefix) { return Shot.Name.StartsWith(Prefix); });
		});
	}

	// "-TheaterQuick" takes two pictures only, for trying a setting out.
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterQuick")))
	{
		const TArray<FShot> All = Shots;
		if (All.Num() > 8)
		{
			Shots = { All[2], All[4], All[8] };
		}
	}
	bStarted = true;
	UE_LOG(LogTheater, Display, TEXT("Photographer: %d pictures to take, into %s"), Shots.Num(), *Folder);
	if (Shots.Num() == 0)
	{
		// Nothing was asked for that exists: leave, instead of sitting there for ever.
		bStarted = false;
		UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
		FPlatformMisc::RequestExit(false); // (the polite way has been seen not to end a run that never began)
	}
}

void ACampaignPhotographer::Arrange(const FShot& Given)
{
	ACampaignPlayerController* Controller = Cast<ACampaignPlayerController>(GetWorld()->GetFirstPlayerController());
	ACampaignCameraPawn* Eye = Controller ? Cast<ACampaignCameraPawn>(Controller->GetPawn()) : nullptr;
	ACampaignMapActor* Table = Controller ? Controller->GetMap() : nullptr;
	if (!Eye || !Table)
	{
		return;
	}
	FShot Shot = Given; // (a picture that looks for its own subject fills in where it found it: see "@amber" below)
	// The first picture after the opening screen begins a campaign through it, as Germany in September 1939.
	if (!Shot.bOpening && !Controller->GetSession())
	{
		// "-TheaterContinue" picks up the campaign saved for pictures instead (a copy of a real save put there
		// by hand), to see a war that has been played as its player sees it.
		if (FParse::Param(FCommandLine::Get(), TEXT("TheaterContinue")))
		{
			Controller->ContinueCampaign();
		}
		else if (GrandNation.IsEmpty())
		{
			Controller->BeginCampaign(StartGroup, StartSide);
		}
		else
		{
			Controller->ShowOpeningTab(0);
			Controller->PickGrandYear(GrandYear, true);
			Controller->PickGrandNation(GrandNation);
			Controller->BeginGrandCampaign(false);
		}
	}
	// ("-TheaterLanding=@amber:9" pictures a landing whose card is amber. Which army and which shore that is depends on
	// how the war has gone, so the photographer looks for it: the months asked for are played, then the player's armies
	// are gone through for a shore they could land on with an amber verdict, a month more at a time until one is found.
	// The pictures that follow use the same army and the same shore.)
	if (Shot.Army == TEXT("@amber") && Controller->GetSession())
	{
		for (int32 i = 0; i < Shot.Months; ++i)
		{
			Controller->EndMonth();
		}
		Shot.Months = 0;
		Shot.bDismiss = true;
		AmberFrom.Reset();
		AmberTo.Reset();
		int32 Played = 0, Met[4] = { 0, 0, 0, 0 }; // red, amber, good, none
		for (; Played < 16 && AmberTo.IsEmpty(); ++Played)
		{
			if (Played > 0)
			{
				Controller->EndMonth();
			}
			Controller->DismissAnnouncement();
			Controller->EndNewsreel();
			FCampaignSession* Session = Controller->GetSession();
			const FCampaignView& View = Session->GetView();
			TMap<int32, TArray<int32>> Armies;
			for (const FViewCorps& Unit : View.Corps)
			{
				if (Unit.Nation == View.Human)
				{
					Armies.FindOrAdd(Unit.Region).Add(Unit.Id);
				}
			}
			for (const TPair<int32, TArray<int32>>& Army : Armies)
			{
				TArray<int32> Ports, Beaches;
				Session->SeaReach(Army.Value, Ports, Beaches);
				for (const int32 Beach : Beaches)
				{
					if (!AmberTo.IsEmpty() || !Table->GetData().Regions.IsValidIndex(Beach) || !Table->GetData().Regions.IsValidIndex(Army.Key))
					{
						continue;
					}
					const FString Tone = Session->PlanLanding(Army.Value, Beach).Tone;
					++Met[Tone == TEXT("red") ? 0 : Tone == TEXT("amber") ? 1 : Tone == TEXT("good") ? 2 : 3];
					if (Tone == TEXT("amber"))
					{
						AmberFrom = Table->GetData().Regions[Army.Key].Name;
						AmberTo = Table->GetData().Regions[Beach].Name;
					}
				}
			}
		}
		UE_LOG(LogTheater, Display, TEXT("Photographer: an amber landing %s (verdicts met: %d red, %d amber, %d good, %d with none)"),
			AmberTo.IsEmpty() ? TEXT("was not found") : *FString::Printf(TEXT("from %s to %s, %d months on"), *AmberFrom, *AmberTo, Played - 1), Met[0], Met[1], Met[2], Met[3]);
		Shot.Army = AmberFrom;
	}
	if (Shot.Region == TEXT("@amber"))
	{
		Shot.Region = AmberTo;
	}
	if (Shot.Attack == TEXT("@amber"))
	{
		Shot.Attack = AmberTo;
	}
	const FCampaignMapData& Data = Table->GetData();
	FVector2D At(Data.Width * 0.5, Data.Height * 0.5);
	if (const int32* Id = Data.RegionByName.Find(Shot.Region))
	{
		At = Data.Regions[*Id].Center;
	}
	else if (Shot.Region == TEXT("@capital") && Controller->GetSession())
	{
		for (const FCampaignRegion& Region : Data.Regions)
		{
			if (Region.bCapital && Region.Owner == Controller->GetSession()->GetView().Human)
			{
				At = Region.Center;
			}
		}
	}
	// "@parade" is the middle of the board the models are set out on for inspection; "@parade2" is the third model
	// on it. For these the view goes much closer than the game allows, with everything sharp and nothing over it.
	if (Shot.Region.StartsWith(TEXT("@parade")) && Table->HasParade())
	{
		const FString Which = Shot.Region.Mid(7);
		At = Table->ParadeSpot(Which.IsEmpty() ? INDEX_NONE : FCString::Atoi(*Which));
		Eye->MinDistance = 300.0f;
		Eye->MiniatureStrength = 1.0f;
		Eye->FocusHeight = Table->ParadeHeight();
		Eye->FlattestPitch = 0.0f;
		Eye->SetBounds(FVector2D(1.0e7, 1.0e7)); // the board may run off the edge of the map
		Eye->SetLens(Shot.Lens > 0.0f ? Shot.Lens : 42.0f);
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveAllViewportWidgets();
		}
	}
	if (Shot.Region.StartsWith(TEXT("@")) && Shot.Region.Contains(TEXT(",")))
	{
		// "@700,979": a point of the map itself, in map units.
		FString X, Y;
		Shot.Region.Mid(1).Split(TEXT(","), &X, &Y);
		At = FVector2D(FCString::Atod(*X), FCString::Atod(*Y));
	}
	if (Shot.Region == TEXT("@report") && Controller->GetSession())
	{
		// The first fleet known only from a report.
		for (const FViewFleet& Fleet : Controller->GetSession()->GetView().Fleets)
		{
			if (Fleet.bReport)
			{
				At = Table->FleetPlace(Fleet.Id);
				break;
			}
		}
	}
	if (Shot.Region.StartsWith(TEXT("@fleet")))
	{
		At = Table->FleetSpot(FCString::Atoi(*Shot.Region.Mid(6)));
	}
	if (Shot.Region.StartsWith(TEXT("@army ")))
	{
		// "@army Grenzmark": the first army piece standing in that province, looked at itself (not the province's middle).
		if (const int32* Where = Table->GetData().RegionByName.Find(Shot.Region.Mid(6)))
		{
			for (const TObjectPtr<AArmyPiece>& Piece : Table->GetPieces())
			{
				if (Piece && Piece->GetRegionId() == *Where && !Piece->IsHidden())
				{
					At = Table->WorldToMap(Piece->GetRestLocation());
					break;
				}
			}
		}
	}
	if (Shot.Region.StartsWith(TEXT("@train")))
	{
		// "@train" is a train running in the open just now; "@train3" one made up under level 3 guns (two flak wagons).
		FVector2D Running;
		Table->ForgetTrain();
		if (Table->TrainSpot(FCString::Atoi(*Shot.Region.Mid(6)), Running))
		{
			At = Running;
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Photographer: no such train runs in the open just now"));
		}
	}
	Eye->FocusHeight = Shot.Region.StartsWith(TEXT("@parade")) ? Eye->FocusHeight : 0.0f;
	// ("-TheaterLookUp=12,204": for this run's looks the eye may tip up to 12 degrees above level, and it turns about
	// a point 204 world units over the ground. For the effects kit's flak burst, which hangs 60 m up and is to be seen
	// from below against the sky. A picture switch only: the game's own eye is never flatter than 22 degrees below
	// level and looks at the ground.)
	{
		FString Asked;
		TArray<FString> Parts;
		if (!Shot.Region.StartsWith(TEXT("@parade")) && FParse::Value(FCommandLine::Get(), TEXT("TheaterLookUp="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 2)
		{
			Eye->FlattestPitch = FCString::Atof(*Parts[0]);
			Eye->FocusHeight = FCString::Atof(*Parts[1]);
		}
	}
	Table->HoldRaids(Shot.RaidAt);
	Eye->SetHeading(Shot.Yaw, Shot.Tilt);
	Eye->SetTopZoom(FMath::Clamp(Shot.Zoom, 1.0f, 1.45f)); // (a picture may be taken from higher than the player can go: the whole world in one)
	Eye->GoTo(Table->MapToWorld(At.X, At.Y), Shot.Zoom, true);

	Controller->SetScripted(true);
	Table->SetHoveredRegion(INDEX_NONE);
	if (Shot.bOpening)
	{
		if (Shot.OpeningTab >= 0)
		{
			Controller->ShowOpeningTab(Shot.OpeningTab);
		}
		if (Shot.OpeningYear >= 0)
		{
			Controller->PickGrandYear(Shot.OpeningYear, true);
		}
		if (!Shot.OpeningNation.IsEmpty())
		{
			Controller->PickGrandNation(Shot.OpeningNation);
		}
		// ("-TheaterAskBegin": the word to begin is given, so that the question put before a saved campaign is replaced shows)
		if (FParse::Param(FCommandLine::Get(), TEXT("TheaterAskBegin")))
		{
			Controller->AskToBegin(1);
		}
		return;
	}
	if (Shot.bMenu)
	{
		Controller->OpenMenu();
		if (Shot.OpeningTab >= 0)
		{
			Controller->ShowOpeningTab(Shot.OpeningTab);
		}
		TArray<FString> Presses;
		Shot.Options.ParseIntoArray(Presses, TEXT(","));
		for (const FString& Press : Presses)
		{
			Controller->PressOption(Press);
		}
		return;
	}
	Controller->CloseMenu();
	Controller->CloseDocument();
	Controller->CloseLessonGuide(); // (84.3.3: the Guide's list, left up by 14e2, is put away before the next picture)
	Controller->CloseLedger();
	Controller->CloseDiplomacy();
	Controller->CloseResearch();
	Controller->CloseGenerals();
	Controller->ClosePage();
	Controller->CloseArsenal();
	if (!Shot.bKeep)
	{
		Controller->CloseWarPrompt();
		Controller->CloseFaceOff();
		Controller->PutDownArmy();
	}
	if (Shot.bWar)
	{
		Controller->ConfirmWar();
	}
	if (Shot.bConfirm)
	{
		Controller->ConfirmAttack();
	}
	if (Shot.bClose)
	{
		Controller->CloseFaceOff();
	}
	// ("-TheaterActs=0:spy:recruit;1:Thames>spy:send:1" presses actions as the months asked of a picture go by: each
	// is "<months gone>:<action>" or "<months gone>:<province>><action>", for an action that wants a province in mind.
	// For pictures of things that take some months to bring about, such as an agent at work in another country's city.)
	static TArray<TPair<int32, FString>> ActsAsked;
	static bool bActsRead = false;
	if (!bActsRead)
	{
		bActsRead = true;
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterActs="), Asked, false))
		{
			TArray<FString> Each;
			Asked.ParseIntoArray(Each, TEXT(";"));
			for (const FString& One : Each)
			{
				FString When, What;
				if (One.Split(TEXT(":"), &When, &What))
				{
					ActsAsked.Emplace(FCString::Atoi(*When), What);
				}
			}
		}
	}
	auto PressAsked = [&](int32 MonthsGone)
	{
		for (TPair<int32, FString>& Asked : ActsAsked)
		{
			if (Asked.Key != MonthsGone)
			{
				continue;
			}
			Asked.Key = -1; // (once only)
			FString Where, What = Asked.Value;
			if (Asked.Value.Split(TEXT(">"), &Where, &What))
			{
				if (const int32* Region = Table->GetData().RegionByName.Find(Where))
				{
					Controller->SelectRegion(*Region);
				}
			}
			Controller->OpenPage(TEXT("spies"));
			Controller->PressPage(What);
			Controller->ClosePage();
			Controller->SelectRegion(INDEX_NONE);
		}
	};
	for (int32 i = 0; i < Shot.Months; ++i)
	{
		PressAsked(i);
		Controller->EndMonth();
	}
	if (Shot.Months > 0)
	{
		PressAsked(Shot.Months);
	}
	// ("-TheaterRecon=Poznan;Lodz" flies a reconnaissance flight over those provinces before the first picture: the
	// armies there are identified.)
	{
		static bool bReconFlown = false;
		FString ReconAsked;
		if (!bReconFlown && Controller->GetSession() && FParse::Value(FCommandLine::Get(), TEXT("TheaterRecon="), ReconAsked, false))
		{
			bReconFlown = true;
			TArray<FString> ReconPlaces;
			ReconAsked.ParseIntoArray(ReconPlaces, TEXT(";"));
			for (const FString& ReconPlace : ReconPlaces)
			{
				if (const int32* ReconRegion = Table->GetData().RegionByName.Find(ReconPlace))
				{
					const FString ReconSaid = Controller->StageRecon(*ReconRegion);
					UE_LOG(LogTheater, Display, TEXT("Photographer: reconnaissance over %s: %s"), *ReconPlace, ReconSaid.IsEmpty() ? TEXT("no flight could go") : *ReconSaid);
				}
			}
		}
	}
	// ("-TheaterCutAt=Neumark:bombed;Lodz:agent:report" cuts those railways before the first picture, each with its
	// cause; a third part, "report" or "seen", only writes down that the player knows of a cut there.)
	{
		static bool bCutsStaged = false;
		FString CutsAsked;
		if (!bCutsStaged && Controller->GetSession() && FParse::Value(FCommandLine::Get(), TEXT("TheaterCutAt="), CutsAsked, false))
		{
			bCutsStaged = true;
			TArray<FString> CutPlaces;
			CutsAsked.ParseIntoArray(CutPlaces, TEXT(";"));
			for (const FString& CutPlace : CutPlaces)
			{
				TArray<FString> CutParts;
				CutPlace.ParseIntoArray(CutParts, TEXT(":"));
				if (CutParts.Num() >= 1)
				{
					if (const int32* CutRegion = Table->GetData().RegionByName.Find(CutParts[0]))
					{
						Controller->StageCut(*CutRegion, CutParts.IsValidIndex(1) ? CutParts[1] : FString(TEXT("bombed")), CutParts.IsValidIndex(2) ? CutParts[2] : FString(TEXT("now")));
					}
				}
			}
		}
	}
	// ("-TheaterAgentAt=Paris;Saxony;Brussels:books;Champagne:road" sets an agent of the player's at work in each of
	// those provinces before the first picture: watching abroad or on guard at home, or at the work named, or still on
	// the road there. "-TheaterAgentCaught=Lille" leaves the mark of one caught there this month. No start has an agent.)
	{
		static bool bAgentsStaged = false;
		FString AgentsAsked, CaughtAsked;
		if (!bAgentsStaged && Controller->GetSession())
		{
			bAgentsStaged = true;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterAgentAt="), AgentsAsked, false))
			{
				TArray<FString> AgentPlaces;
				AgentsAsked.ParseIntoArray(AgentPlaces, TEXT(";"));
				for (const FString& AgentPlace : AgentPlaces)
				{
					FString AgentWhere = AgentPlace, AgentWork;
					AgentPlace.Split(TEXT(":"), &AgentWhere, &AgentWork);
					if (const int32* AgentRegion = Table->GetData().RegionByName.Find(AgentWhere))
					{
						Controller->StageAgent(*AgentRegion, AgentWork);
					}
				}
			}
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterAgentCaught="), CaughtAsked, false))
			{
				TArray<FString> CaughtPlaces;
				CaughtAsked.ParseIntoArray(CaughtPlaces, TEXT(";"));
				for (const FString& CaughtPlace : CaughtPlaces)
				{
					if (const int32* CaughtRegion = Table->GetData().RegionByName.Find(CaughtPlace))
					{
						Controller->StageCaught(*CaughtRegion);
					}
				}
			}
		}
	}
	if (Shot.Region.StartsWith(TEXT("@agent")))
	{
		// "@agent0" is the first of the agents' pieces on the table (after them come the marks of those caught): the
		// eye goes to where it stands. (Here, and not with the other places: the agents are stood just above.)
		FVector2D AgentStands;
		if (Table->GetAgentSpot(FCString::Atoi(*Shot.Region.Mid(6)), AgentStands))
		{
			Eye->GoTo(Table->MapToWorld(AgentStands.X, AgentStands.Y), Shot.Zoom, true);
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Photographer: no such agent stands on the table"));
		}
	}
	// ("-TheaterGunsAt=Polish Silesia:2;Krakow:1" gives those places anti-aircraft guns of that level before the first
	// picture: for the raid card's defences, since no start has a place with guns.)
	{
		static bool bGunsStaged = false;
		FString GunsAsked;
		if (!bGunsStaged && Controller->GetSession() && FParse::Value(FCommandLine::Get(), TEXT("TheaterGunsAt="), GunsAsked, false))
		{
			bGunsStaged = true;
			TArray<FString> StagedPlaces;
			GunsAsked.ParseIntoArray(StagedPlaces, TEXT(";"));
			for (const FString& Staged : StagedPlaces)
			{
				FString StagedWhere, StagedLevel;
				if (Staged.Split(TEXT(":"), &StagedWhere, &StagedLevel))
				{
					if (const int32* StagedRegion = Table->GetData().RegionByName.Find(StagedWhere))
					{
						Controller->StageGuns(*StagedRegion, FCString::Atoi(*StagedLevel));
					}
				}
			}
		}
	}
	// ("-TheaterOrders=Ruhr:corps:inf+corps:inf+corps:arm;Hamburg:ship:dd" places orders from the Arsenal before the
	// first picture, each list with its province in mind, after any months the first picture asks to be played (so
	// that there is money for them): for pictures of what the cities have on order.)
	{
		static bool bOrdersPlaced = false;
		FString OrdersAsked;
		if (!bOrdersPlaced && Controller->GetSession() && FParse::Value(FCommandLine::Get(), TEXT("TheaterOrders="), OrdersAsked, false))
		{
			bOrdersPlaced = true;
			TArray<FString> Places;
			OrdersAsked.ParseIntoArray(Places, TEXT(";"));
			for (const FString& Place : Places)
			{
				FString Where, Keys;
				TArray<FString> Each;
				if (!Place.Split(TEXT(":"), &Where, &Keys) || Keys.ParseIntoArray(Each, TEXT("+")) == 0)
				{
					continue;
				}
				if (const int32* Region = Table->GetData().RegionByName.Find(Where))
				{
					Controller->SelectRegion(*Region);
					Controller->OpenArsenal();
					for (const FString& Key : Each)
					{
						Controller->OrderFromArsenal(Key);
					}
					Controller->CloseArsenal();
				}
			}
			Controller->SelectRegion(INDEX_NONE);
		}
	}
	if (Shot.bWatch)
	{
		Controller->PutDownArmy();
		Controller->EndMonthAndWatch();
		return; // the view now follows the action
	}
	{
		static bool bSpentAsked = false;
		FString Spend;
		if (!bSpentAsked && Controller->GetSession() && FParse::Value(FCommandLine::Get(), TEXT("TheaterSpend="), Spend, false))
		{
			bSpentAsked = true;
			TArray<FString> Places;
			Spend.ParseIntoArray(Places, TEXT(">"));
			for (int32 K = 0; K + 1 < Places.Num(); ++K)
			{
				const int32* From = Data.RegionByName.Find(Places[K]);
				const int32* To = Data.RegionByName.Find(Places[K + 1]);
				if (From && To && Controller->PickUpArmyIn(*From))
				{
					const bool bSpentBefore = Controller->IsHandSpent();
					if (!bSpentBefore)
					{
						Controller->OrderTo(*To);
					}
					UE_LOG(LogTheater, Display, TEXT("Photographer: %s to %s, %s"), *Places[K], *Places[K + 1], bSpentBefore ? TEXT("no moves left: not ordered") : TEXT("ordered"));
					Controller->PutDownArmy();
				}
			}
		}
	}
	if (!Shot.bKeep)
	{
		const int32* Pick = Data.RegionByName.Find(Shot.Select);
		int32 Picked = Pick ? *Pick : INDEX_NONE;
		if (Shot.Select == TEXT("@capital") && Controller->GetSession())
		{
			for (const FCampaignRegion& Region : Data.Regions)
			{
				if (Region.bCapital && Region.Owner == Controller->GetSession()->GetView().Human)
				{
					Picked = Region.Id;
				}
			}
		}
		Controller->SelectRegion(Picked);
	}
	if (const int32* Army = Data.RegionByName.Find(Shot.Army))
	{
		if (Shot.Carry.IsEmpty())
		{
			Controller->PickUpArmyIn(*Army);
		}
		else if (const int32* Over = Data.RegionByName.Find(Shot.Carry))
		{
			const bool bHeld = Controller->HoldArmyIn(*Army);
			Controller->CarryHeldOver(*Over);
			UE_LOG(LogTheater, Display, TEXT("Photographer: flag in %s %s, carried over %s"), *Shot.Army, bHeld ? TEXT("taken in hand") : TEXT("NOT taken"), *Shot.Carry);
		}
	}
	{
		// ("-TheaterAgentHand=Paris" takes the agent standing there in hand for every picture: his card is shown, and
		// with "-TheaterHover=Champagne" the ground he would watch from there is edged.)
		static FString AgentHand;
		static bool bAgentHandAsked = false;
		if (!bAgentHandAsked)
		{
			bAgentHandAsked = true;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterAgentHand="), AgentHand, false);
		}
		if (const int32* HandRegion = AgentHand.IsEmpty() ? nullptr : Data.RegionByName.Find(AgentHand))
		{
			const bool bAgentTaken = Controller->PickUpAgentIn(*HandRegion);
			UE_LOG(LogTheater, Display, TEXT("Photographer: the agent at %s %s"), *AgentHand, bAgentTaken ? TEXT("is taken in hand") : TEXT("could NOT be taken in hand"));
		}
	}
	if (const int32* Click = Data.RegionByName.Find(Shot.Click))
	{
		Controller->ClickRegion(*Click);
	}
	if (const int32* Drop = Data.RegionByName.Find(Shot.Drop))
	{
		Controller->DropHeldOver(*Drop);
	}
	if (const int32* Route = Data.RegionByName.Find(Shot.Route))
	{
		Controller->ShowRouteTo(*Route);
	}
	{
		// ("-TheaterHover=airfield:Norfolk" rests it on that province's airfield, for the airfield's own box)
		const bool bOnField = Shot.Hover.StartsWith(TEXT("airfield:"));
		const int32* Rest = Shot.Hover.IsEmpty() ? nullptr : Data.RegionByName.Find(bOnField ? Shot.Hover.Mid(9) : Shot.Hover);
		Controller->StageHover(Rest ? *Rest : INDEX_NONE, bOnField);
		if (Rest)
		{
			Table->SetHoveredRegion(*Rest); // (as the game does for the real pointer)
		}
	}
	if (Shot.bSea)
	{
		Controller->ToggleSea();
	}
	if (const int32* Target = Data.RegionByName.Find(Shot.Attack))
	{
		Controller->OrderTo(*Target);
	}
	Controller->SetProvinceTab(Shot.ProvinceTab);
	// ("-TheaterHoverGate=Panama" rests the pointer on that gate's mark, for the picture of its box.)
	{
		FString OnGate;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterHoverGate="), OnGate, false);
		Controller->StageGate(OnGate);
	}
	// ("-TheaterFleetNamed=Baltic" takes the player's fleet of that name in hand, and "-TheaterHoverSea=Skagerrak"
	// rests the pointer on that sea: for the pictures of a fleet's reach through a gate, and of a sea it is refused.)
	{
		FString FleetNamed, OnSea;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFleetNamed="), FleetNamed, false) && Controller->GetSession())
		{
			const FCampaignView& Seen = Controller->GetSession()->GetView();
			for (const FViewFleet& Fleet : Seen.Fleets)
			{
				if (Fleet.Nation == Seen.Human && Fleet.Name.StartsWith(FleetNamed))
				{
					Controller->PickUpFleet(Fleet.Id);
					break;
				}
			}
		}
		FParse::Value(FCommandLine::Get(), TEXT("TheaterHoverSea="), OnSea, false);
		Controller->StageSea(OnSea);
	}
	// ("-TheaterStandAll" tells every army that waits to stand this month, for the picture of the corner when none
	// waits; "-TheaterEndAsk" puts the question at the month's end.)
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterStandAll")))
	{
		for (int32 Guard = 0; Guard < 400 && Controller->ArmiesYetToMove() > 0; ++Guard)
		{
			Controller->PutDownArmy();
			Controller->NextArmy();
			if (Controller->IsStandingThisMonth())
			{
				break;
			}
			Controller->StandThisMonth();
		}
		Controller->PutDownArmy();
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterEndAsk")))
	{
		Controller->ShowEndAsk();
	}
	{
		// The view of the map the picture asks for: as it is, by wealth, or by friend and foe.
		const int32 Wanted = Shot.bWealth ? 1 : Shot.bSides ? 2 : 0;
		for (int32 Step = 0; Step < 3 && (Table->IsWealthShown() ? 1 : Table->AreSidesShown() ? 2 : 0) != Wanted; ++Step)
		{
			Controller->ToggleWealth();
		}
	}
	if (Shot.bAdviser)
	{
		Controller->ShowAdviser();
	}
	else if (Controller->GetAdviser().bOn)
	{
		Controller->DismissAdviser();
	}
	if (Shot.bNewsreel)
	{
		TArray<FNewsCard> Cards;
		FNewsCard& Card = Cards.AddDefaulted_GetRef();
		Card.Tag = TEXT("GER");
		Card.Title = TEXT("THE GERMAN ARMY ON THE MARCH");
		Card.Text = TEXT("German forces took Pomorze and Lodz, and were thrown back at Poznan.");
		for (const FCampaignRegion& Region : Table->GetData().Regions)
		{
			if (Region.Name == TEXT("Lodz"))
			{
				Card.Region = Region.Id;
			}
		}
		Controller->PlayNewsreel(TEXT("September 1939"), Cards);
		Controller->NextNewsCard();
	}
	if (Shot.Flight >= -1)
	{
		Controller->PlayBriefing();
		for (int32 Card = 0; Card <= Shot.Flight; ++Card)
		{
			Controller->NextNewsCard();
		}
	}
	if (Shot.bYearCard)
	{
		Controller->PlayYearCard(Controller->GetLastYear());
	}
	if (!Shot.Look.IsEmpty() && Controller->GetSession())
	{
		if (const int32* Place = Data.RegionByName.Find(Shot.Look))
		{
			Controller->SelectRegion(*Place);
			for (const FViewCorps& Unit : Controller->GetSession()->GetView().Corps)
			{
				if (Unit.Region == *Place)
				{
					Controller->LookAtArmy(*Place, Unit.Nation);
					break;
				}
			}
		}
	}
	if (Shot.bJoin && Controller->GetSession())
	{
		// The first province where an ally's corps could join: its army picked up, and the allies asked.
		TArray<int32> Places;
		Controller->GetSession()->GetView().Joins.GetKeys(Places);
		if (Places.Num() > 0)
		{
			const int32 Place = Places[0];
			Controller->SelectRegion(Place);
			if (Controller->PickUpArmyIn(Place))
			{
				Controller->AskAlliesToJoin();
				const FVector2D Spot = Table->GetData().Regions[Place].Center;
				Eye->GoTo(Table->MapToWorld(Spot.X, Spot.Y), Shot.Zoom, true);
			}
		}
	}
	if (Shot.bFleet && Controller->GetSession())
	{
		const FCampaignView& Now = Controller->GetSession()->GetView();
		const FViewFleet* Best = nullptr;
		int32 Own = 0;
		for (const FViewFleet& Fleet : Now.Fleets)
		{
			if (Fleet.Nation == Now.Human && Shot.FleetPick >= 0)
			{
				if (Own++ == Shot.FleetPick)
				{
					Best = &Fleet;
				}
			}
			else if (Fleet.Nation == Now.Human && Fleet.Reach.Num() > 1 && (!Best || Fleet.Ships() > Best->Ships()))
			{
				Best = &Fleet;
			}
		}
		if (Best)
		{
			const int32 Id = Best->Id;
			const TArray<int32> Seas = Best->Reach;
			const int32 From = Best->Zone;
			Controller->PickUpFleet(Id);
			if (Shot.bFleetSails)
			{
				int32 Far = INDEX_NONE;
				double Most = -1.0;
				for (int32 Sea : Seas)
				{
					if (Table->GetData().Seas.IsValidIndex(Sea) && Table->GetData().Seas.IsValidIndex(From))
					{
						const double Away = FVector2D::Distance(Table->GetData().Seas[Sea].Center, Table->GetData().Seas[From].Center);
						if (Away > Most)
						{
							Most = Away;
							Far = Sea;
						}
					}
				}
				Controller->OrderFleetTo(Far);
			}
			const FVector2D Spot = Table->FleetPlace(Id);
			Eye->GoTo(Table->MapToWorld(Spot.X, Spot.Y), Shot.Zoom, true);
			if (Shot.FleetDo == TEXT("orders"))
			{
				Controller->OpenFleetOrders();
			}
			else if (!Shot.FleetDo.IsEmpty() && Controller->GetFleetHand().Coasts.Num() > 0)
			{
				// The first coast its guns can reach: the question, and then (for "fire") the shelling itself.
				const int32 Coast = Controller->GetFleetHand().Coasts[0];
				Controller->OpenPage(FString::Printf(TEXT("shell:%d:%d"), Id, Coast));
				if (Shot.FleetDo == TEXT("fire"))
				{
					const FOfficePage& Asked = Controller->GetPage();
					if (Asked.Sections.Num() > 0 && Asked.Sections[0].Rows.Num() > 0 && Asked.Sections[0].Rows[0].Buttons.Num() > 0)
					{
						Controller->PressPage(Asked.Sections[0].Rows[0].Buttons[0].Act);
					}
				}
			}
		}
	}
	if (!Shot.Force.IsEmpty())
	{
		FString Kind, Number;
		if (Shot.Force.Split(TEXT(":"), &Kind, &Number))
		{
			Controller->ShowForce(Kind == TEXT("fleet"), FCString::Atoi(*Number));
		}
	}
	if (!Shot.Order.IsEmpty() && !Shot.bArsenal)
	{
		Controller->ProvinceAction(Shot.Order);
	}
	if (Shot.bRail)
	{
		Controller->ToggleRail();
	}
	if (Shot.bRecord)
	{
		// The corps of the army in hand that has the most to tell.
		int32 Best = INDEX_NONE, Most = -1;
		for (const FTrayCard& Card : Controller->GetArmyTray().Cards)
		{
			if (Card.Honours.Num() > Most)
			{
				Most = Card.Honours.Num();
				Best = Card.CorpsId;
			}
		}
		if (Best != INDEX_NONE)
		{
			Controller->OpenRecord(Best);
		}
	}
	if (Shot.bRoll)
	{
		Controller->OpenRoll();
	}
	if (!Shot.Room.IsEmpty())
	{
		// (a page that is asked for is not pictured under a country's fall notice: in play the notices are read first)
		for (int32 Guard = 0; Guard < 24 && Controller->GetAnnouncement(); ++Guard)
		{
			Controller->DismissAnnouncement();
		}
		Controller->OpenPage(Shot.Room);
		// The presses of a shot, one after another: an act by name, "@row" for the first button of a row that can
		// be pressed, "@top" for the first button across the top.
		TArray<FString> Presses;
		Shot.RoomAct.ParseIntoArray(Presses, TEXT("|"));
		for (const FString& Press : Presses)
		{
			FString Act = Press;
			const FOfficePage& Now = Controller->GetPage();
			if (Press == TEXT("@top"))
			{
				// (a raid being planned is a card with no buttons across its top: its one button is the one that flies it)
				Act = Now.Card.bValid ? Now.Card.Fly.Act : Now.Top.Num() > 0 ? Now.Top[0].Act : FString();
			}
			else if (Press == TEXT("@row"))
			{
				Act.Reset();
				for (const FPageSection& Section : Now.Sections)
				{
					for (const FPageRow& Row : Section.Rows)
					{
						if (Act.IsEmpty() && Row.Buttons.Num() > 0 && Row.Buttons[0].bOk)
						{
							Act = Row.Buttons[0].Act;
						}
					}
				}
			}
			if (!Act.IsEmpty())
			{
				Controller->PressPage(Act);
			}
		}
		if (Shot.bCloseRoom)
		{
			Controller->ClosePage();
			if (Shot.RaidAt >= 0.0f)
			{
				Controller->SelectRegion(INDEX_NONE); // (the province's panels are put away too: the picture is of the sky)
			}
		}
	}
	if (Shot.bDismiss)
	{
		Controller->DismissAnnouncement();
		Controller->EndNewsreel();
	}
	if (Shot.bAnnounce)
	{
		FAnnouncement Told;
		Told.Tag = TEXT("POL");
		Told.Title = TEXT("POLAND HAS FALLEN");
		Told.Text = TEXT("Poland asked Germany for terms in the fourth week of September 1939. Its armies have laid down their arms, and its 17 home provinces are under foreign flags.");
		Told.Kicker = TEXT("Eliminated \u00B7 September 1939");
		Told.More = TEXT("Poland can come back. If a member of an alliance at war with Germany takes Warsaw, Poland is liberated and returns to the war on that side.");
		Told.FactLabels = { TEXT("Fell to"), TEXT("Home provinces"), TEXT("Week of the war") };
		Told.FactValues = { TEXT("Germany"), TEXT("17"), TEXT("4") };
		Told.bGood = true;
		Controller->Announce(Told);
	}
	if (Shot.bGuide)
	{
		// (Rules 84.3.3: in a war he leads GUIDE now opens the list of the ten lessons; this picture stays the typed page.)
		Controller->OpenHowToPlay();
	}
	if (Shot.bLessonGuide)
	{
		Controller->OpenGuide();
	}
	if (Shot.bWorks)
	{
		// For looking at the models: every kind of works in the region the picture is of, as if they had been built.
		TArray<FIntVector4> Works;
		Works.Init(FIntVector4(0, 0, 0, 0), Data.Regions.Num());
		if (const int32* Here = Data.RegionByName.Find(Shot.Region))
		{
			Works[*Here] = FIntVector4(3, 3, 3, 2);
		}
		Table->ShowBuildings(Works);
	}
	if (!Shot.Prizes.IsEmpty())
	{
		TArray<FPrizeMark> Marks;
		TArray<FString> Each;
		Shot.Prizes.ParseIntoArray(Each, TEXT(";"));
		for (const FString& One : Each)
		{
			TArray<FString> Part;
			One.ParseIntoArray(Part, TEXT(":"));
			const int32* Where = Part.Num() >= 3 ? Data.RegionByName.Find(Part[0]) : nullptr;
			if (Where)
			{
				FPrizeMark& Mark = Marks.AddDefaulted_GetRef();
				Mark.Region = *Where;
				Mark.By = Part[1] == TEXT("-") ? FString() : Part[1];
				Mark.bBurning = FCString::Atoi(*Part[2]) != 0;
			}
		}
		Table->ShowPrizes(Marks);
	}
	if (!Shot.Scars.IsEmpty())
	{
		TArray<int32> Left;
		TArray<FString> Each;
		Shot.Scars.ParseIntoArray(Each, TEXT(";"));
		for (const FString& One : Each)
		{
			TArray<FString> Part;
			One.ParseIntoArray(Part, TEXT(":"));
			const int32* Where = Part.Num() >= 3 ? Data.RegionByName.Find(Part[0]) : nullptr;
			if (Where)
			{
				Left.Append({ *Where, FCString::Atoi(*Part[1]), FCString::Atoi(*Part[2]) });
			}
		}
		Table->ShowScars(Left);
	}
	if (Shot.TownPlot >= 0)
	{
		Controller->PickTownPlot(Shot.TownPlot);
	}
	if (!Shot.TownOrder.IsEmpty())
	{
		Controller->OrderTownBuilding(Shot.TownOrder);
	}
	if (!Shot.Dress.IsEmpty() && Controller->GetSession())
	{
		// For looking at what stands round an army's base: each army named is shown in the state given.
		TArray<FString> Each;
		Shot.Dress.ParseIntoArray(Each, TEXT(";"));
		for (const FString& One : Each)
		{
			TArray<FString> Part;
			One.ParseIntoArray(Part, TEXT(":"));
			const int32* Where = Part.Num() >= 4 ? Data.RegionByName.Find(Part[0]) : nullptr;
			if (!Where)
			{
				continue;
			}
			for (const FViewCorps& Unit : Controller->GetSession()->GetView().Corps)
			{
				if (Unit.Region == *Where)
				{
					const FCampaignNation* Nation = Data.FindNation(Unit.Nation);
					AArmyPiece* Piece = Table->FindPiece(*Where, Unit.Nation);
					if (Piece && Nation)
					{
						Piece->DressAs(Table, *Nation, FCString::Atoi(*Part[1]), FCString::Atoi(*Part[2]) != 0, FCString::Atoi(*Part[3]));
					}
					break;
				}
			}
		}
	}
	if (Shot.Civic > 0)
	{
		// For looking at the models: every kind of town building beside the town the picture is of.
		int32 Here = INDEX_NONE;
		if (const int32* Named = Data.RegionByName.Find(Shot.Region))
		{
			Here = *Named;
		}
		else
		{
			for (const FCampaignRegion& Region : Data.Regions)
			{
				if (Region.bCapital && Controller->GetSession() && Region.Owner == Controller->GetSession()->GetView().Human)
				{
					Here = Region.Id;
				}
			}
		}
		TArray<int32> Stood;
		for (int32 Line = 0; Line < 6 && Here != INDEX_NONE; ++Line)
		{
			const int32 Level = Shot.Civic >= 4 ? (Line % 3) + 1 : Shot.Civic;
			Stood.Append({ Here, Line, Shot.Civic >= 4 && Line == 4 ? 0 : Level, Shot.Civic >= 4 && Line % 2 == 0 ? 1 : 0 });
		}
		Table->ShowCivic(Stood);
	}
	if (Shot.bGenerals)
	{
		Controller->OpenGenerals();
		for (const FVacantCorps& Corps : Controller->GetGenerals().Vacant)
		{
			if (Corps.BestId != INDEX_NONE)
			{
				Controller->AssignGeneral(Corps.BestId, Corps.Corps);
				break;
			}
		}
	}
	if (Shot.bResearch)
	{
		// ("-TheaterChart=mobile:radar" chooses a card of the research chart and rests the pointer on another)
		{
			FString Staged;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterChart="), Staged, false))
			{
				if (!Staged.Split(TEXT(":"), &Controller->StagedChartChoice, &Controller->StagedChartPoint))
				{
					Controller->StagedChartChoice = Staged;
				}
			}
		}
		Controller->OpenResearch();
		if (Controller->GetResearch().bTree)
		{
			Controller->StartTech(TEXT("mediums")); // the grand campaign: a team is put to work on a technology
		}
		else
		{
			Controller->SetResearchTeams(TEXT("infantry"), 0);
			Controller->SetResearchTeams(TEXT("armour"), 2);
		}
	}
	if (Shot.bDiplomacy)
	{
		Controller->OpenDiplomacy();
		if (!Shot.Path.IsEmpty())
		{
			FString Id = Shot.Path, Partner;
			Shot.Path.Split(TEXT(":"), &Id, &Partner);
			for (int32 Press = 0; Press < Shot.PathPresses; ++Press)
			{
				Controller->PressPath(Id, Partner);
			}
		}
	}
	if (Shot.bLedger)
	{
		Controller->OpenLedger();
	}
	if (Shot.bArsenal)
	{
		Controller->OpenArsenal();
		if (!Shot.Order.IsEmpty())
		{
			Controller->OrderFromArsenal(Shot.Order);
		}
	}
}

void ACampaignPhotographer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bStarted)
	{
		return;
	}

	// Wait for the materials to finish compiling the first time the game is run.
	if (Current < 0)
	{
		WarmUp += DeltaSeconds;
		const int32 Remaining = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
		if (FMath::FloorToInt32(WarmUp / 5.0f) != FMath::FloorToInt32((WarmUp - DeltaSeconds) / 5.0f))
		{
			UE_LOG(LogTheater, Display, TEXT("Photographer: waiting, %d shader jobs left after %.0f s"), Remaining, WarmUp);
		}
		if (WarmUp < 4.0f || (Remaining > 0 && WarmUp < 240.0f))
		{
			return;
		}
		Current = 0;
		Clock = 0.0f;
		SheetWait = 0.0f;
		Frames = 0;
		bTaken = false;
		// ("-TheaterSteps=<file>": the playtester's step file is played in place of the pictures. The game is left as
		// a player finds it, on its opening screen: the file's own first step begins the campaign.)
		{
			FString Asked;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterTime="), Asked, false))
			{
				TArray<FString> Sets;
				Asked.ParseIntoArray(Sets, TEXT("|"), true);
				for (const FString& Each : Sets)
				{
					FTimeSet& Set = TimeSets.AddDefaulted_GetRef();
					FString Vars;
					if (!Each.Split(TEXT(":"), &Set.Name, &Vars))
					{
						Set.Name = Each;
					}
					TArray<FString> Pairs;
					Vars.ParseIntoArray(Pairs, TEXT(","), true);
					for (const FString& Pair : Pairs)
					{
						FString Key, Value;
						if (Pair.Split(TEXT("="), &Key, &Value))
						{
							// ("size" is the share of the window the map is drawn at, in hundredths each way)
							Set.Vars.Emplace(Key == TEXT("size") ? FString(TEXT("r.ScreenPercentage")) : Key, Value);
						}
					}
				}
				FParse::Value(FCommandLine::Get(), TEXT("TheaterTimeRounds="), TimeRounds);
				TimeRounds = FMath::Clamp(TimeRounds, 1, 8);
				UE_LOG(LogTheater, Display, TEXT("Timing: %d settings, %d rounds"), TimeSets.Num(), TimeRounds);
			}
		}
		FString StepFile;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSteps="), StepFile, false))
		{
			Steps = MakeShared<FStepPlayer>();
			if (!Steps->Load(StepFile, Folder))
			{
				Steps.Reset();
				bStarted = false;
				UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
			}
			return;
		}
		Arrange(Shots[0]);
		return;
	}
	if (Steps.IsValid())
	{
		ACampaignPlayerController* Hand = Cast<ACampaignPlayerController>(GetWorld()->GetFirstPlayerController());
		if (!Hand || !Steps->Tick(*Hand, DeltaSeconds))
		{
			Steps.Reset();
			bStarted = false;
			UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
		}
		return;
	}

	// (The whole-world map: the fine ground under the eye is modelled on another thread, two sheets at a time. A
	// picture taken before it stood showed the coarse sheet, with no lesser river, lake or clearing on it: so the
	// Niger, the prairie's creeks and the taiga by night were sent to the art director with no water in them. The
	// picture waits for the ground, up to two minutes.)
	if (!bTaken && Clock >= 1.5f && SheetWait < 120.0f)
	{
		const ACampaignPlayerController* Player = Cast<ACampaignPlayerController>(GetWorld()->GetFirstPlayerController());
		if (Player && Player->GetMap() && Player->GetMap()->AreFineSheetsComing())
		{
			SheetWait += DeltaSeconds;
			return;
		}
	}

	Clock += DeltaSeconds;
	++Frames;

	// (A look at a train goes with the train while the picture waits.)
	if (!bTaken && Shots.IsValidIndex(Current) && Shots[Current].Region.StartsWith(TEXT("@train")))
	{
		ACampaignPlayerController* Rider = Cast<ACampaignPlayerController>(GetWorld()->GetFirstPlayerController());
		ACampaignCameraPawn* RiderEye = Rider ? Cast<ACampaignCameraPawn>(Rider->GetPawn()) : nullptr;
		FVector2D Running;
		if (RiderEye && Rider->GetMap() && Rider->GetMap()->TrainSpot(FCString::Atoi(*Shots[Current].Region.Mid(6)), Running))
		{
			RiderEye->GoTo(Rider->GetMap()->MapToWorld(Running.X, Running.Y), Shots[Current].Zoom, true);
		}
	}

	// An army ordered off just before the picture is caught in the air.
	if (!bMarched && Clock >= 2.45f && !Shots[Current].March.IsEmpty())
	{
		bMarched = true;
		ACampaignPlayerController* Controller = Cast<ACampaignPlayerController>(GetWorld()->GetFirstPlayerController());
		const int32* To = Controller && Controller->GetMap() ? Controller->GetMap()->GetData().RegionByName.Find(Shots[Current].March) : nullptr;
		if (To)
		{
			Controller->OrderTo(*To);
		}
	}

	// A moment at the front begun just before the picture: the eye is caught on its way down, at the bottom, or back.
	if (!bMarched && Clock >= 2.45f && !Shots[Current].Moment.IsEmpty())
	{
		bMarched = true;
		ACampaignPlayerController* Controller = Cast<ACampaignPlayerController>(GetWorld()->GetFirstPlayerController());
		const int32* At = Controller && Controller->GetMap() ? Controller->GetMap()->GetData().RegionByName.Find(Shots[Current].Moment) : nullptr;
		if (At)
		{
			Controller->GetMap()->AddBattleSmoke(*At);
			Controller->StartMoment(*At);
		}
	}

	// A raid held in the air for its picture: the eye stays on it, at the height it flies.
	if (!bTaken && Shots[Current].RaidAt >= 0.0f)
	{
		ACampaignPlayerController* Controller = Cast<ACampaignPlayerController>(GetWorld()->GetFirstPlayerController());
		ACampaignCameraPawn* Eye = Controller ? Cast<ACampaignCameraPawn>(Controller->GetPawn()) : nullptr;
		FVector2D Spot;
		float High = 0.0f;
		// ("-TheaterRaidFixed" keeps the eye over the place being raided, so that stills a second apart are framed alike)
		static const bool bRaidFixed = FParse::Param(FCommandLine::Get(), TEXT("TheaterRaidFixed"));
		if (Eye && Controller->GetMap() && (bRaidFixed ? Controller->GetMap()->FirstRaidTarget(Spot, High) : Controller->GetMap()->FirstFlight(Spot, High)))
		{
			Eye->FocusHeight = High;
			Eye->GoTo(Controller->GetMap()->MapToWorld(Spot.X, Spot.Y), Shots[Current].Zoom, true);
		}
	}

	// ("-TheaterTime=": the view is held and the settings are tried against one another in place of the pictures.)
	if (!bTaken && TimeSets.Num() > 0 && Clock >= 3.0f + Shots[Current].Wait)
	{
		if (!TickTiming(DeltaSeconds))
		{
			bStarted = false;
			UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
		}
		return;
	}

	// Give the picture three seconds to settle, then take it.
	if (!bTaken && Clock >= 3.0f + Shots[Current].Wait)
	{
		const FShot& Shot = Shots[Current];
		const FString File = FPaths::Combine(Folder, Shot.Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		FIntPoint View(0, 0);
		if (const APlayerController* Controller = GetWorld()->GetFirstPlayerController())
		{
			Controller->GetViewportSize(View.X, View.Y);
		}
		UE_LOG(LogTheater, Display, TEXT("Photographer: %s at %.1f frames a second (%dx%d; game %.1f ms, draw %.1f ms, graphics chip %.1f ms)"),
			*Shot.Name, Frames / Clock, View.X, View.Y,
			FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
		bTaken = true;
		// ("-TheaterClip=24,0.25": the first picture is followed by so many more of the same view, so many seconds
		// apart, named after it, to be put together as a short clip by Tools/clip.swift.)
		if (Current == 0)
		{
			FString ClipAsked, ClipCount, ClipGap;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterClip="), ClipAsked, false))
			{
				if (!ClipAsked.Split(TEXT(","), &ClipCount, &ClipGap))
				{
					ClipCount = ClipAsked;
				}
				ClipFrames = FMath::Clamp(FCString::Atoi(*ClipCount), 0, 120);
				ClipEvery = ClipGap.IsEmpty() ? 0.25f : FMath::Clamp(FCString::Atof(*ClipGap), 0.05f, 5.0f);
				ClipTaken = 0;
				ClipAt = Clock + ClipEvery;
			}
		}
	}
	if (bTaken && Current == 0 && ClipTaken < ClipFrames && Clock >= ClipAt)
	{
		++ClipTaken;
		ClipAt += ClipEvery;
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, FString::Printf(TEXT("%s-clip-%02d.png"), *Shots[0].Name, ClipTaken)), true, false);
		if (ClipTaken == ClipFrames)
		{
			ClipAt = Clock + 1.0f; // (and a second for the last of them to be written)
		}
		return;
	}
	if (bTaken && Current == 0 && ClipFrames > 0 && (ClipTaken < ClipFrames || Clock < ClipAt))
	{
		return;
	}

	// A second later, move on.
	if (bTaken && Clock >= 4.0f + Shots[Current].Wait)
	{
		++Current;
		if (!Shots.IsValidIndex(Current))
		{
			bStarted = false;
			UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
			return;
		}
		Clock = 0.0f;
		SheetWait = 0.0f;
		Frames = 0;
		bTaken = false;
		bMarched = false;
		Arrange(Shots[Current]);
	}
}

void ACampaignPhotographer::ApplyTimeSet(int32 Index)
{
	// Everything touched so far goes back to how it stood, then this trial's own settings are laid over it.
	for (const TPair<FString, FString>& Was : TimeDefaults)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Was.Key))
		{
			Variable->Set(*Was.Value, ECVF_SetByConsole);
		}
	}
	if (!TimeSets.IsValidIndex(Index))
	{
		return;
	}
	for (const TPair<FString, FString>& Each : TimeSets[Index].Vars)
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Each.Key);
		if (!Variable)
		{
			UE_LOG(LogTheater, Warning, TEXT("Timing: no such setting: %s"), *Each.Key);
			continue;
		}
		if (!TimeDefaults.Contains(Each.Key))
		{
			TimeDefaults.Add(Each.Key, Variable->GetString());
		}
		Variable->Set(*Each.Value, ECVF_SetByConsole);
	}
}

bool ACampaignPhotographer::TickTiming(float DeltaSeconds)
{
	const float Settle = 2.0f, Measure = 4.0f;
	if (TimeAt < 0)
	{
		TimeAt = 0;
		TimeRound = 0;
		TimeClock = 0.0f;
		ApplyTimeSet(0);
		return true;
	}
	TimeClock += DeltaSeconds;
	{
		// ("-TheaterTimePan": the eye is carried round a small loop while the settings are timed, as a hand drags the map.
		// The same loop for every setting: it is timed by the run's own clock, begun afresh with each.)
		static const bool bPan = FParse::Param(FCommandLine::Get(), TEXT("TheaterTimePan"));
		if (bPan)
		{
			if (const APlayerController* Player = GetWorld()->GetFirstPlayerController())
			{
				if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(Player->GetPawn()))
				{
					Eye->Pan(FVector2D(FMath::Sin(TimeClock * 1.05f), 0.6f * FMath::Cos(TimeClock * 1.05f)), DeltaSeconds);
				}
			}
		}
	}
	if (TimeClock >= Settle)
	{
		++TimeFrames;
		TimeSum += DeltaSeconds;
		TimeGame += FPlatformTime::ToMilliseconds(GGameThreadTime);
		TimeDraw += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		TimeChip += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		TimeSlow += DeltaSeconds > 0.0334f ? 1 : 0;
	}
	// (one picture of each setting, in the first round, for the eye: the same view under each)
	if (TimeRound == 0 && !bTimePictured && TimeClock >= Settle + Measure - 0.5f)
	{
		bTimePictured = true;
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, TEXT("time-") + TimeSets[TimeAt].Name + TEXT(".png")), true, false);
	}
	if (TimeClock < Settle + Measure)
	{
		return true;
	}
	FTimeSet& Set = TimeSets[TimeAt];
	const int32 Count = FMath::Max(TimeFrames, 1);
	Set.Fps.Add(TimeSum > 0.0f ? Count / TimeSum : 0.0f);
	Set.Game.Add(static_cast<float>(TimeGame / Count));
	Set.Draw.Add(static_cast<float>(TimeDraw / Count));
	Set.Chip.Add(static_cast<float>(TimeChip / Count));
	Set.Slow.Add(TimeSlow);
	UE_LOG(LogTheater, Display, TEXT("Timing: round %d, %s: %.1f frames a second (game %.1f ms, draw %.1f ms, graphics chip %.1f ms), %d of %d frames over 33 ms"),
		TimeRound + 1, *Set.Name, Set.Fps.Last(), Set.Game.Last(), Set.Draw.Last(), Set.Chip.Last(), TimeSlow, Count);
	TimeFrames = TimeSlow = 0;
	TimeSum = 0.0f;
	TimeGame = TimeDraw = TimeChip = 0.0;
	TimeClock = 0.0f;
	bTimePictured = false;
	if (++TimeAt >= TimeSets.Num())
	{
		TimeAt = 0;
		if (++TimeRound >= TimeRounds)
		{
			// The middle value of each setting over the rounds: what the comparison is read from.
			auto Middle = [](TArray<float> Values) { Values.Sort(); return Values.Num() > 0 ? Values[Values.Num() / 2] : 0.0f; };
			for (const FTimeSet& Each : TimeSets)
			{
				UE_LOG(LogTheater, Display, TEXT("Timing: MIDDLE OF %d ROUNDS, %s: %.1f frames a second (game %.1f ms, draw %.1f ms, graphics chip %.1f ms)"),
					TimeRounds, *Each.Name, Middle(Each.Fps), Middle(Each.Game), Middle(Each.Draw), Middle(Each.Chip));
			}
			ApplyTimeSet(INDEX_NONE);
			return false;
		}
	}
	ApplyTimeSet(TimeAt);
	return true;
}
