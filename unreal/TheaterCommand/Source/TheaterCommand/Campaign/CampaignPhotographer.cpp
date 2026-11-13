#include "Campaign/CampaignPhotographer.h"
#include "Campaign/ArmyPiece.h"

#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
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
			{
				FShot& Shot = Add(TEXT("35-by-sea"), TEXT("Hamburg"), 0.62f);
				Shot.Army = TEXT("London");
				Shot.bSea = true;
			}
			{
				FShot& Shot = Add(TEXT("35b-landing-asked"), TEXT("Hamburg"), 0.5f);
				Shot.bKeep = true;
				Shot.Attack = TEXT("Hamburg");
			}
			{
				FShot& Shot = Add(TEXT("35c-landing-made"), TEXT("Hamburg"), 0.4f);
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
				Add(*FString::Printf(TEXT("look%d-%s-%s"), Shots.Num() + 1, *Parts[0].Replace(TEXT(" "), TEXT("-")).Replace(TEXT("@"), TEXT("")), Parts.IsValidIndex(1) ? *Parts[1] : TEXT("0.3")), *Parts[0], Zoom,
					Parts.IsValidIndex(2) ? FCString::Atof(*Parts[2]) : 0.0f, Parts.IsValidIndex(3) ? FCString::Atof(*Parts[3]) : 0.0f)
					.Lens = Parts.IsValidIndex(4) ? FCString::Atof(*Parts[4]) : 0.0f; // a fifth part is the width of the view in degrees: small for a long lens
			}
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

void ACampaignPhotographer::Arrange(const FShot& Shot)
{
	ACampaignPlayerController* Controller = Cast<ACampaignPlayerController>(GetWorld()->GetFirstPlayerController());
	ACampaignCameraPawn* Eye = Controller ? Cast<ACampaignCameraPawn>(Controller->GetPawn()) : nullptr;
	ACampaignMapActor* Table = Controller ? Controller->GetMap() : nullptr;
	if (!Eye || !Table)
	{
		return;
	}
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
	Eye->SetHeading(Shot.Yaw, Shot.Tilt);
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
	for (int32 i = 0; i < Shot.Months; ++i)
	{
		Controller->EndMonth();
	}
	if (Shot.bWatch)
	{
		Controller->PutDownArmy();
		Controller->EndMonthAndWatch();
		return; // the view now follows the action
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
		const int32* Rest = Shot.Hover.IsEmpty() ? nullptr : Data.RegionByName.Find(Shot.Hover);
		Controller->StageHover(Rest ? *Rest : INDEX_NONE);
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
		for (const FViewFleet& Fleet : Now.Fleets)
		{
			if (Fleet.Nation == Now.Human && Fleet.Reach.Num() > 1 && (!Best || Fleet.Ships() > Best->Ships()))
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
				Act = Now.Top.Num() > 0 ? Now.Top[0].Act : FString();
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
		Frames = 0;
		bTaken = false;
		Arrange(Shots[0]);
		return;
	}

	Clock += DeltaSeconds;
	++Frames;

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
		Frames = 0;
		bTaken = false;
		bMarched = false;
		Arrange(Shots[Current]);
	}
}
