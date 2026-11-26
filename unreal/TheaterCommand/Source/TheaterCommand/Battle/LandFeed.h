#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

/**
 * A recorded ground battle read into plain terms (Content/Data/LandBattles/<key>.json, written by
 * Tools/land_record/record.ts from the rules' own field battle with the squad brain on). Plain C++, so that the cloud
 * test reads exactly what the game reads. Nothing here decides anything: it only reads, and samples the tracks
 * between their kept points as the contract says (the bridge's "The men", the land 3D handoff section 9).
 *
 * The field's frame: x east, y north, metres, from the field's south-west corner; z up from the field's datum. A
 * facing is radians, 0 north, clockwise (a block's face), in (-pi, pi]: turned the short way round.
 */
namespace LandFeed
{
	/** 0 the attacker, 1 the defender, -1 neither. */
	int SideOf(const std::string& Side);

	struct Block
	{
		int Id = 0;
		int Side = -1;
		std::string Nat, Kind, Name, Short, Parent;
		/** The vehicle or gun model (Sherman, Tiger, Howitzer105...), empty for foot. */
		std::string Model;
		/** The army whose soldier file dresses its men, and the file standing in until that army's own exists. */
		std::string Army, StandIn;
		/** A support team: "mg" or "mor" (empty for every other block). */
		std::string Team;
		std::string Cal;
		int Figures0 = 0;
		/** The real men it stands for at the start (a live battle's header; 0 where the recording does not say). */
		int Men0 = 0;
		double W = 0.0, D = 0.0;
		/** The corps it belongs to (a live battle's header; 0 where not said), and whether that corps comes up from next door
		 * later in the battle (neighbours join the fight, release 2): its block is in the header from the start, in the
		 * views only once it has walked on, and till then in the views' Coming. */
		int Corps = 0;
		bool bJoins = false;
		/** Tank crews' names, one per tank in the block's model order (empty strings for a plain tank). */
		std::vector<std::string> CrewNames;
	};

	/** A kept point of a track: lerp between them. */
	struct Point
	{
		double T = 0.0, X = 0.0, Y = 0.0, F = 0.0;
	};

	/** What changed: the pose, the cover, where he fires (while he fires), hidden in ambush, closed up. */
	struct Change
	{
		double T = 0.0;
		std::string Pose, Cover;
		bool bAim = false;
		double AimX = 0.0, AimY = 0.0;
		bool bHidden = false;
		bool bButtoned = false;
	};

	/** A vehicle's scar: on its face (front, left, right, rear, top) at U across it (-1 its left edge to 1) and H up it
	 * (0 the ground to 1 the top), of a kind (gouge, hole, scorch, pocks), shown from battle time From. */
	struct Scar
	{
		std::string Face, Kind;
		double U = 0.0, H = 0.0, From = 0.0;
	};

	/** One man, tank, gun or lorry of a block, for the whole battle. */
	struct Track
	{
		int Block = 0;
		int K = 0;
		/** A follower of key man K in his section (the drill's sections: 1 to n-1), 0 the key man himself (the
		 * recording's j, absent for a key man). The man is (Block, K, J). */
		int J = 0;
		/** officer, rifle, mg, at, sapper, holder, fo, hmg, mortar; tank, gun, lorry; and a section's followers' leader
		 * (a rifle section's key man), loader (an MG's or a tube's number two, the ammunition bearers), crew (a gun's). */
		std::string Role;
		std::vector<Point> At;
		std::vector<Change> Ch;
		/** The battle time of each of his shots that the contract stamps (a tank's or a gun's round, an anti-tank
		 * round, a grenade). */
		std::vector<double> Shots;
		/** The battle time he fell (or she was knocked out), -1 never. */
		double Fell = -1.0;
		/** The battle time he left the field with his block, -1 never: carried on at his last speed and facing and
		 * faded over a second. */
		double Out = -1.0;
		std::vector<Scar> Marks;
		/** The spotter pair's number (fo), 0 for anyone else. */
		int Fo = 0;
		/** Another track drawn in the same place for the whole battle (the recording's two batteries share one
		 * spotter pair's walk): drawn once, this one left out. */
		bool bTwin = false;
		/** A live battle (release 2): At's last point is where he stands now, provisional; the next delta replaces it
		 * (AppendDelta). Never set on a recording read whole. */
		bool bTail = false;
		/** A wreck laid where it was hit (LandFeed's reading: the rules move a knocked-out vehicle to its formation
		 * place): every point from PinFrom on is at PinX, PinY. */
		bool bPinned = false;
		double PinFrom = 0.0, PinX = 0.0, PinY = 0.0;
		/** The platoon cam's marks (release 2, the tactics thread's -cam recordings): the battle seconds he is one of the
		 * squad leading his side's assault (Lead, the recording's ld), in the base of fire or the moving group (Element,
		 * e: bMove for "move"), and his radio calls (Calls, cl), each with the spot it brings the fire on where the
		 * recording gives one (bSpot). All empty in a recording without them. */
		struct Span
		{
			double From = 0.0, To = 0.0;
			bool bMove = false;
		};
		struct Call
		{
			double T = 0.0, X = 0.0, Y = 0.0;
			bool bSpot = false;
		};
		std::vector<Span> Lead, Element;
		std::vector<Call> Calls;

		// Release 2's field telephones and stretcher bearers (Brian, 22 November 17:33Z; the tactics thread's rules,
		// each behind its player's switch: absent, he is as before).
		/** A lineman ("ln": he carries the cable reel and mends the wires) or a stretcher bearer ("br"). */
		bool bLineman = false, bBearer = false;
		/** Struck and not killed ("wd"): he goes down and lies alive from then, -1 never. */
		double Wounded = -1.0;
		/** Carried on a stretcher from From to To by two bearers ("cy": [[from, to, by1, by2]]), their tracks' indexes
		 * (-1 where the rules named no one found). */
		struct Carry
		{
			double From = 0.0, To = 0.0;
			int By1 = -1, By2 = -1;
		};
		std::vector<Carry> Carried;
		/** He reached the aid post and leaves the field ("gone"); he walks back to the line ("back"). -1 never. */
		double Gone = -1.0, Back = -1.0;

		bool IsVehicle() const { return Role == "tank" || Role == "gun" || Role == "lorry"; }
		bool IsMan() const { return !IsVehicle(); }
		double Start() const { return At.empty() ? 0.0 : At.front().T; }
		double End() const { return At.empty() ? 0.0 : At.back().T; }
	};

	enum class Result : uint8_t { Miss, Glance, Worn, Kill };

	/** One round, once: fired at T by model K of block By (-1: a shooter the player has not seen), J of his section
	 * (0 the key man), with weapon W at block On; lands at X, Y at battle time L with result R; Ok the model it struck
	 * and Oj the man of his section (0 the key man), on its face Fc at U across and H up (glance, worn, kill). */
	struct Round
	{
		int N = 0;
		double T = 0.0, L = 0.0;
		int By = -1, K = -1, J = 0;
		std::string W;
		int On = -1, Ok = -1, Oj = 0;
		double X = 0.0, Y = 0.0;
		Result R = Result::Miss;
		std::string Fc;
		double U = 0.0, H = 0.0;
		/** A tank's machine-gun round: its gun, as the rules name it ("coax", "hull" or "roof"; empty where they do not). */
		std::string G;
		/** It struck the tank's commander up in his hatch (the rules' cm: 1): he drops inside, and she fights buttoned up. */
		bool bCm = false;
		/** Shell blasts (release 2): a man struck by the burst of the round before it, or of the 'shell' event that burst
		 * there (the rules' m: 1). No shot and no burst of its own: the one burst is drawn once. */
		bool bMate = false;
		/** A smoke round the player called (release 2, the general's smoke on call): how long its screen hangs once it
		 * lands, battle seconds, as the rules' sight block holds it (the rules' "hang"); -1 where the round does not say
		 * (the calibre's own time). */
		double Hang = -1.0;
		/** A sniper's one aimed round (release 2's sniper teams, the rules' sn: 1), and whether he still lies hidden as he
		 * fires it (hd: 1): heard as a sniper's, and while hidden from where it arrives, never from his spot. */
		bool bSn = false, bHd = false;
	};

	struct Event
	{
		double T = 0.0;
		std::string What, Side, Text, Cue, Cal, Ground;
		int Unit = -1, On = -1, By = -1;
		double X = 0.0, Y = 0.0, Reach = 0.0;
		bool bArmour = false;
		bool bWhere = false;
	};

	/**
	 * Effects (R2): one of the rules' notes of the fighters on call (air support, src/battle/land/sim.ts), read from an
	 * event's extra block where the event is one of them: "land:air-in" (a leg of a strike over the start of its run:
	 * Run the run, Models, Alt, Nat, Year, Heading), "land:strafe" (its guns down the strip: Run from and to, Width,
	 * Secs battle seconds), "land:bomb-drop" (a bomb let go: At the battle second it bursts, To where), "air:lost" (a
	 * plane of it down: Kind explode, burn or spin, Chutes, Alt). Side 0 the attacker's fighters, 1 the defender's.
	 * Read for LandAir (-TheaterLandAirSupport); nothing else reads them.
	 */
	struct AirNote
	{
		double T = 0.0;
		std::string What, Nat, Kind;
		int Side = -1, Flight = -1, Models = 4, Year = 1944, Chutes = 0;
		double X = 0.0, Y = 0.0, Alt = 0.0, Heading = 0.0, Secs = 0.0, Width = 60.0, At = -1.0;
		double Run[4] = { 0.0, 0.0, 0.0, 0.0 };
		double To[2] = { 0.0, 0.0 };
	};

	/** A block's phase from a battle time on: column, deploy, bound, startline, fight, hold, rest, pinned, fallback,
	 * flee, rally, gone, battery, limbered. */
	struct Phase
	{
		double T = 0.0;
		std::string Ph;
	};

	/** A block's stand in a view, for the camera and the desk. */
	struct Unit
	{
		int Id = 0;
		int Side = -1;
		double X = 0.0, Y = 0.0, Face = 0.0;
		double Str = 1.0, Mor = 1.0;
		int Figures = 0;
		std::string State, Act, Cover;
		int Engaged = -1;

		// A live battle's (release 2; src/battle/land/live.ts landUnitOut): what the general's screen reads.
		/** An enemy block the player has not met: where it stands and whether it moves, nothing that names or counts it. */
		bool bKnown = true;
		/** The player has taken it in hand (his staff fights the rest of his side). */
		bool bTaken = false;
		/** Hidden from the other side (a block in woods since the fight began, not yet found: Brian's ambushes, 22
		 * November 04:18Z). The player's own: his forest mark; the enemy's: not drawn at all. */
		bool bHidden = false;
		/** Seconds till his order on the air reaches it (radio), -1 none on the air. */
		double OrderIn = -1.0;
		/** The real men it stands for now (its start's men by its strength now), -1 where not said (an enemy not met). */
		int Men = -1;
		/** Found by its own fire but not yet located (the historian's check, 22 November: its men not yet plain, the fire
		 * back at it halved for a while). */
		bool bVague = false;
		/** Firing into cover at a point to flush out what hides there (reconnaissance by fire), and the point. */
		bool bSearch = false;
		double SearchX = 0.0, SearchY = 0.0;
		/** His own blocks' tools (Brian, 22 November 14:19Z; the rules say, on his own blocks only): a Sherman with hedge
		 * cutters that can punch a gap at a hedge point he picks; and the smoke a section can still lay (-1 none said). */
		bool bCutter = false;
		int Smoke = -1;
		/** Hold until relieved (release 2; Brian, 22 November 12:48Z; his own only): told to wait for a corps coming up from
		 * next door; the battle second help can reach it; help never came (its men break sooner); no longer waiting (his
		 * re-order or giving ground). Dug in where it stands while waiting and not free. */
		bool bWaits = false, bWaitLate = false, bWaitFree = false;
		double WaitDue = -1.0;
		/** Its order: kind (move, attack, hold, withdraw...), the point it goes to, the block it attacks (-1 none) and its
		 * path's corners. Empty kind: no order. */
		std::string OrderKind;
		double OrderX = 0.0, OrderY = 0.0;
		int OrderTarget = -1;
		std::vector<std::pair<double, double>> OrderPath;
	};

	/**
	 * A hedge cutter's cut (release 2; Brian, 22 November 14:19Z; the tactics thread's rules, "cuts"): T0 she starts
	 * to push into the bank, T1 it gives way and the gap is open from then on (men use it from T1); Unit her track
	 * (Tracks' index, -1 where not given); X, Y the gap's middle on the bank's line; Yaw the way she drives through
	 * (radians from east); Width the gap, bank end to bank end (metres).
	 */
	struct Cut
	{
		double T0 = 0.0, T1 = 0.0, X = 0.0, Y = 0.0, Yaw = 0.0, Width = 4.0;
		int Unit = -1;
	};

	/**
	 * Release 2's field telephones (Brian, 22 November 17:33Z; the tactics thread's "wires", behind the player's phones
	 * switch): a line from a headquarters' post (block Hq) to a company (block Unit) of side Side, laid along Pts (metres,
	 * the field's); Id its own number, as the cuts and mends name it.
	 */
	struct Wire
	{
		int Id = 0, Side = 1, Hq = -1, Unit = -1;
		std::vector<std::pair<double, double>> Pts;
	};
	/** A wire broken at T at X, Y ("wireCuts"), By what: "shell" or "patrol". */
	struct WireCut
	{
		double T = 0.0, X = 0.0, Y = 0.0;
		int Wire = 0;
		std::string By;
	};
	/** A wire mended at T ("wireMends") by its lineman (Man, his track's index; -1 none named): out from the post at T0
	 * (walking in 'reel'), at the cut X, Y from T1, splicing from then to T (-1 where the recording does not say). */
	struct WireMend
	{
		double T = 0.0, T0 = -1.0, T1 = -1.0, X = 0.0, Y = 0.0;
		int Wire = 0, Man = -1;
	};
	/** An order held by a cut wire ("held"): due at Due, it went at Went (by a runner; -1 not yet), to block Unit; T when
	 * the rules held it. */
	struct HeldOrder
	{
		double T = 0.0, Due = 0.0, Went = -1.0;
		int Unit = -1;
	};
	/** A side's aid post ("aid"), where the stretcher bearers carry the wounded. */
	struct AidPost
	{
		int Side = 0;
		double X = 0.0, Y = 0.0;
	};
	/**
	 * Release 2's night patrol before the attack (the tactics thread's "patrol", behind the player's patrol switch;
	 * settled by the rules at 0 s, a block's matter: no man of the battle's own goes): Count men of side Side sent from
	 * block Unit (dressed as its soldiers), out between T0 and T1 (before 0 s) along Route ([t, x, y], times before 0:
	 * out from the block to the post found or where it was caught, and back); the hidden post found at Found (bFound,
	 * FoundKind its kind); Alerted when the enemy heard it (-1 never), and the flares sent up ([t, x, y] each). bOn
	 * where a recording carries one.
	 */
	struct Patrol
	{
		struct Point
		{
			double T = 0.0, X = 0.0, Y = 0.0;
		};
		bool bOn = false, bFound = false;
		int Side = 0, Unit = -1, Count = 0;
		double T0 = 0.0, T1 = 0.0;
		double FoundX = 0.0, FoundY = 0.0;
		std::string FoundKind;
		double Alerted = -1.0;
		std::vector<Point> Route, Flares;
	};

	/** A crater the field keeps (view zones, kind crater). */
	struct Crater
	{
		double X = 0.0, Y = 0.0, R = 0.0, At = 0.0;
		std::string Cal;
	};

	/**
	 * A block of a corps coming up from next door, not yet on the field (neighbours join the fight, release 2; the rules'
	 * view().coming): his own side's, and the enemy's his staff knows of. Its corps walks on together at At (battle
	 * seconds) at (X, Y) on the field's Edge (south, north, west, east); Arrive the card's word for when (early, midway,
	 * late; empty where not said).
	 */
	struct Arrival
	{
		int Id = 0;
		int Side = -1;
		int Corps = 0;
		std::string Kind, Edge, Arrive;
		double At = 0.0, X = 0.0, Y = 0.0;
	};

	/**
	 * The rider ahead (release 2; Brian, 22 November 12:48Z: "a courier arrives first; clicking him shows the relief's
	 * strength and arrival time"): his own corps coming up from next door sends a despatch rider on a motorcycle first. He
	 * rides from its edge to the middle of his army (there: the battle second he reaches it) and stands there until his
	 * corps comes up (comes), then leaves the views. Says: the relief's strength and times in words for a click. Looks and
	 * words only: nothing in the fight reads him.
	 */
	struct Rider
	{
		int Corps = 0, Side = -1;
		double X = 0.0, Y = 0.0, Face = 0.0, Speed = 0.0, There = 0.0, Comes = 0.0;
		/** When his staff fights it, the battle second it reaches his line (-1 where it waits for his orders). */
		double Line = -1.0;
		std::string Edge, Name, Kind, Says;
		int Men = 0;
		double Str = 1.0;
	};

	/** One battle second's view: the bars and the blocks' stands. */
	struct View
	{
		double T = 0.0;
		double BalanceAtt = 0.5, BalanceDef = 0.5;
		double BreakingAtt = 0.0, BreakingDef = 0.0;
		double Contact = -1.0;
		std::vector<Unit> Units;
		/** The blocks of corps still coming up from next door that the player may know of (empty: none). */
		std::vector<Arrival> Coming;
		/** His own corps' despatch riders riding in ahead of them (empty: none). */
		std::vector<Rider> Riders;
	};

	struct Field
	{
		double W = 0.0, H = 0.0, Cell = 25.0;
		int Cols = 0, Rows = 0;
		std::vector<std::string> Grounds;
		/** Per cell, row 0 the south edge: an index into Grounds, the height in metres, the storeys of its buildings. */
		std::vector<uint8_t> Ground;
		std::vector<float> Height;
		std::vector<uint8_t> Storeys;
		double TownX = 0.0, TownY = 0.0, TownR = 0.0;
		std::string Region, PlaceName, Terrain, TownName, TownSize;
		/** field.look and field.snow, where the recording names them (LandField::Field::Look, Snow): the look of the land
		 * ("desert", "med", "jungle", "island", "farmland", "bocage"; empty: by the place) and the snow lying (0 to 1;
		 * less than 0: by the place and the month). */
		std::string Look;
		double Snow = -1.0;
		/** The deployment zones [x0, y0, x1, y1] of the attacker and the defender. */
		double ZoneAtt[4] = { 0, 0, 0, 0 };
		double ZoneDef[4] = { 0, 0, 0, 0 };

		/**
		 * field.steppe, where the recording lays the open steppe (Tools/land_record, openSteppe; LandField::SteppeLay says
		 * what each is): the farms' fields (a point turned by -Turn about CX, CY falls in a column of Xs and a row of Ys
		 * moved by that column's Shift, Patch each field's crop digit row by row), each cell's crop digit (Crop, as Ground), the balka's and the
		 * railway's lines, the rise the defence holds.
		 */
		struct SteppeLay
		{
			bool bOn = false;
			double Turn = 0.0, CX = 0.0, CY = 0.0;
			std::vector<double> Xs, Ys, Shift;
			std::string Patch, Crop;
			std::vector<std::pair<double, double>> Balka, Rail;
			double BalkaDeep = 0.0, RailBank = 0.0;
			double HillX = 0.0, HillY = 0.0, HillR = 0.0, HillRise = 0.0;
		};
		SteppeLay Steppe;

		/** The cell's ground word at a point (clamped to the field). */
		const std::string& GroundAt(double X, double Y) const;
		/** The cells' heights, bilinear, at a point (clamped). The ground core draws the finer ground; this is the
		 * rules' own. */
		double HeightAt(double X, double Y) const;
	};

	struct Sky
	{
		double Cover = 0.0, Base = 1000.0;
		std::string Kind = "cumulus";
		double Lat = 49.0, Lon = 0.0;
		double Hour = 10.0;
		/** sky.wind, where the recording sets it: the wind's way and strength (metres a second, east and north) for the
		 * smoke and dust. */
		bool bWind = false;
		double WindX = 0.0, WindY = 0.0;
		/** sky.grade, where the recording sets it: the battle camera's grade (darks, middles, red, blue), as
		 * LandBattle's -TheaterLandGrade takes it. */
		bool bGrade = false;
		double Grade[4] = { 0.45, 0.48, 1.0, 1.02 };
	};

	struct SideInfo
	{
		std::string Nat, Adj, Colour, Army, StandIn;
	};

	struct Over
	{
		std::string Winner, How, Level, Broke;
		double Seconds = 0.0;
		std::vector<std::string> Lines;
		double LostAtt = 0.0, LostDef = 0.0;
	};

	struct Recording
	{
		std::string Key, Title, Date;
		uint64_t Seed = 0;
		double Dt = 0.1;
		int Turn = 0;
		Sky Weather;
		/** The campaign's weather over the place in its own word (the battle ticket's weather: "clear", "rain", "snow",
		 * "mud"; the root's "weather"), empty where it keeps none: "mud" lays the thaw's mud (LandField::Field::Weather). */
		std::string WeatherWord;
		SideInfo Sides[2];
		Field Ground;
		double Limit = 0.0, Contact = -1.0, End = 0.0;
		std::vector<Block> Blocks;
		/** Each block's phases, by block (PhasesOf). */
		std::vector<std::pair<int, std::vector<Phase>>> Phases;
		std::vector<View> Views;
		std::vector<Event> Events;
		/** Effects (R2): the fighters on call's notes (AirNote), in the events' order. */
		std::vector<AirNote> Air;
		std::vector<Round> Rounds;
		std::vector<Track> Tracks;
		std::vector<Crater> Craters;
		/** The hedge cutters' cuts, by T1 (none where the rules' cutters switch is off). */
		std::vector<Cut> Cuts;
		/** Release 2's field telephones, aid posts and night patrol (none where their switches are off): the wires, their
		 * cuts and mends by time, the orders a cut held, the aid posts and the patrol. */
		std::vector<Wire> Wires;
		std::vector<WireCut> WireCuts;
		std::vector<WireMend> WireMends;
		std::vector<HeldOrder> Held;
		std::vector<AidPost> Aid;
		Patrol Night;
		/**
		 * Scarred ground (release 2, Brian 21:21Z: "a field fought over keeps its craters next time"; the design
		 * director: looks only, no draw reads it): root "scars" {"count": battles fought here before, "months": since
		 * the last}. Count 0 where none is given (the player's switch off, or a fresh field).
		 */
		int ScarCount = 0;
		double ScarMonths = 0.0;
		/**
		 * Mud memory (release 2, Brian 23 November, "Do all of these": a spring battle's ruts stay on that field for
		 * the next battle there; looks only, as the scars): root "oldRuts" {"count": the battles fought here in mud
		 * before, "months": since the last}. Count 0 where none is given.
		 */
		int OldRutCount = 0;
		double OldRutMonths = 0.0;
		/**
		 * Old wrecks (release 2, Brian 23 November 20:09Z: the burnt-out wrecks of a battle stay on the field for the
		 * next battle there; looks only, as the scars): root "oldWrecks" {"count": the armoured vehicles burnt out here
		 * before (60 at most), "months": since the last of those battles, "nats": {"GER": n, ...} whose they were,
		 * "per": the real armoured vehicles each tank model drawn in this battle stands for}. Count 0 where none is
		 * given (none burnt here, or the player's "Old wrecks" row off); the nations in the order written, each with a
		 * count over 0 (none: the scene takes the battle's two sides). Per 1 where it is missing (no tank drawn), not
		 * a number or under 1; at most 1000. The scene lays Count / Per of them (at least one), so a wreck stands for
		 * as many real machines as a tank in the battle does (Brian's battle-scale rule).
		 */
		int OldWreckCount = 0;
		double OldWreckMonths = 0.0;
		std::vector<std::pair<std::string, int>> OldWreckNats;
		double OldWreckPer = 1.0;
		Over Result;

		// A live battle (release 2: the general's, fought as it is shown; src/battle/land/live.ts). Read from the header
		// with nothing fought yet, and grown by AppendDelta.
		bool bLive = false;
		/** Battle time the rules have reached: nothing at or before it will change. End is that until the battle is over. */
		double Fought = 0.0;
		bool bOver = false;
		/** The side the player commands (0 the attacker, 1 the defender, -1 none: a film), and the setup step's room
		 * [x0, y0, x1, y1] where he may stand his blocks before the clock runs (bSetup while it is open). */
		int Me = -1;
		bool bSetup = false;
		double Room[4] = { 0, 0, 0, 0 };
		/**
		 * After the break (release 2, the battle rule `pursue`; the view rows' "after", src/battle/land/live.ts): 0 none
		 * (before the break, the rule off, or the battle ended at the break), 1 the player who won is asked, PURSUE or
		 * END, and the rules fight nothing more until he answers, 2 the winner fights on. AfterBroke and AfterWinner: the
		 * side that broke and the side that holds the field (0 the attacker, 1 the defender; -1 none). AfterAt: the
		 * break's battle second (never before it). AfterBrokeName and AfterWinnerName: those two armies as the rules name
		 * them ("The German army"), for the screen's line. As the last view read has them.
		 */
		int After = 0;
		int AfterBroke = -1, AfterWinner = -1;
		double AfterAt = 0.0;
		std::string AfterBrokeName, AfterWinnerName;
		/** The craters' numbers already read (the views repeat them). */
		std::set<int> CraterIds;

		const Block* BlockOf(int Id) const;
		/** The track of model K of block B (follower J of his section, 0 the key man), or null. */
		const Track* TrackOf(int B, int K, int J = 0) const;
		int TrackIndexOf(int B, int K, int J = 0) const;
		/** Whether any track is a follower (the drill's sections). */
		bool HasSections() const;
		const std::vector<Phase>* PhasesOf(int B) const;
		/** A block's phase at a battle time ("" before its first). */
		const std::string& PhaseAt(int B, double T) const;
		/** The view at or just before a battle time (the first before it). */
		const View* ViewAt(double T) const;
	};

	bool ReadRecording(const std::string& Source, Recording& Out, std::string& OutError);

	/** The night patrol's men (AddPatrolMen) are numbered from PatrolK in their block (their K): no man of the rules' is. */
	constexpr int PatrolK = 100000;
	/** How long a flare holds the patrol flat (battle seconds: some 28 of life, a parachute flare's burning), the metres
	 * between its men in file, and how near the post found they crawl. */
	constexpr double PatrolFlareBurns = 14.0, PatrolGap = 5.0, PatrolCrawlNear = 70.0;
	/**
	 * Release 2's night patrol drawn (the tactics thread's patrol root, behind its player's switch): its men (Count, at
	 * most 16) added to the recording's text as men of its unit (block Unit, K from PatrolK, the first its leader, the rest
	 * riflemen), so the feed and the crowd both read them and they are dressed as the unit's soldiers. They go out in file
	 * along the route, PatrolGap metres apart (each the leader's way a little later, a little to one side of it), creep,
	 * crawl within PatrolCrawlNear of the post found, kneel where the way halts, go flat a breath after each flare bursts
	 * and lie still while it burns (the way held that long), come home to their unit, and are out as the battle opens (a
	 * leaving time under 0 reads as never everywhere: home before then, they stand among their unit till it). Run on
	 * the text before ReadRecording and the crowd's Build. The men added (0: no patrol, or no way to walk).
	 */
	int AddPatrolMen(std::string& Text);
	/** Whether a track is one of the night patrol's men. */
	inline bool IsPatrolMan(const Track& T) { return T.K >= PatrolK; }

	/**
	 * What one live delta added (release 2), as places in the Recording, for the cores that extend what they built from
	 * it rather than build again: the tracks are by index, the rest from where each list stood before.
	 */
	struct Grown
	{
		/** The men were placed again (the setup step, the player moving a block): every track, view and phase began
		 * again; build afresh. */
		bool bReset = false;
		/** Battle time the rules have reached. */
		double T = 0.0;
		/** Tracks [NewTracksFrom, Tracks.size()) are new. */
		size_t NewTracksFrom = 0;
		/** Tracks with new kept points or a new provisional last point; with new changes; new shots; a fall; an out
		 * set or taken back; new scars. Each once, in order. */
		std::vector<int> Moved, Changed, Shot, Fell, Left, Marked;
		/** Tracks a wound, a carry, a going or a coming back was given in this delta (release 2's bearers). */
		std::vector<int> Hurt;
		/** Where the rounds, events, views and craters stood before: the new are from there on. */
		size_t RoundsFrom = 0, EventsFrom = 0, ViewsFrom = 0, CratersFrom = 0;
		/** Blocks with new phases. */
		std::vector<int> Phased;
		/** The contact was first made in this delta. */
		bool bContact = false;
		/** The battle ended in this delta (Result and End are set). */
		bool bOver = false;
		/** The setup step closed in this delta. */
		bool bBegun = false;
	};

	/**
	 * Adds a live delta (src/battle/land/live.ts LiveDelta) to a Recording read from a live header: the tracks' new kept
	 * points, changes, shots, falls, outs and scars, each moving man's provisional last point (Track::bTail), the rounds,
	 * events, views (and their craters and crews' names), phases, the contact and the end. A battle grown delta by delta
	 * is the battle read whole, once over. False (and why) on a fault; the Recording is then as it was up to the fault.
	 */
	bool AppendDelta(const std::string& Source, Recording& InOut, Grown& Out, std::string& OutError);

	/** Where a track is at a battle time: lerped between its kept points, the facing turned the short way, and its speed
	 * over the ground there (metres a battle second). Before its first point it stands at its first; after its last at
	 * its last (a man who left goes on at his last speed and facing: OutDrift). */
	struct Spot
	{
		double X = 0.0, Y = 0.0, F = 0.0;
		double V = 0.0;
		/** The way he goes over the ground (radians as a facing; his facing where he stands still). */
		double Way = 0.0;
	};
	Spot SpotAt(const Track& T, double Time);
	/** The change in force at a battle time (null before the first). */
	const Change* ChangeAt(const Track& T, double Time);
	/** A facing turned the short way from A toward B by a share. */
	double TurnToward(double A, double B, double Share);
	/** A facing put into (-pi, pi]. */
	double Wrap(double F);
	/**
	 * The machine guns' long fire as the armies meet (the first minute from contact): the 5 battle seconds with the most machine-gun
	 * rounds flown over 150 m, OutAt their middle, and the gunner who fired the most of them and stands through it
	 * (OutTrack, a track index): the moment tracer crosses a field, for the picture run's still. False with none.
	 */
	bool GunsOpen(const Recording& Rec, double& OutAt, int& OutTrack);
	/** The facing from one field point toward another (0 north, clockwise). */
	double FacingTo(double FromX, double FromY, double ToX, double ToY);
}
