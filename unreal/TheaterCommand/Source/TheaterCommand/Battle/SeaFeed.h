#pragma once

#include "Battle/AirFeed.h"
#include "Battle/SeaShow.h"

#include <map>
#include <string>
#include <vector>

/**
 * The fleet action's words read into the show: the bridge's JSON (TC.seaStart's field, each TC.battleView, TC.battleEnd)
 * or a recorded action (Content/Data/SeaBattles/<key>.json, written by tools/sea-record on the rules branch), read the
 * same way, in plain C++ so the cloud test reads exactly what the game reads. The JSON reader is the air feed's.
 *
 * A view may leave out what it has said before: a recording sends a piece's still fields (kind, name, nat, figures0, w;
 * extra.cls, ships0, fleet) the first time it shows, and its zones only where they changed. The reader keeps the last it
 * was told, so the live feed (which says everything every time) and a recording read alike.
 *
 * Nothing here decides anything: it only reads.
 */
namespace SeaFeed
{
	using Json = AirFeed::Json;

	/** The top of a view: the clock, the bars, whether it is over. */
	struct Top
	{
		double T = 0.0, Limit = 1500.0, Speed = 1.0;
		double BalanceAtt = 0.5;
		double BreakingAtt = 0.0, BreakingDef = 0.0;
		double Contact = -1.0;
		std::string Me = "att";
		bool bOver = false;
		std::string Winner, Level;
		std::vector<std::string> OverLines;
		/** The tug-of-war bar's look fields (the naval thread, 21 November): each side's lostShare, -1 where the view has
		 * none (a recording made before them), and the side that has broken off ("att" or "def"), empty before. */
		double LostAtt = -1.0, LostDef = -1.0;
		std::string Broken;
	};

	/**
	 * The tug-of-war bar's knot for a view, from the player's flag (the view's "me"): 0 to 1, 0.5 level, past the
	 * middle he is winning; -1 if the view has no lostShare figures. It is the exchange the action's own result reads
	 * (SeaSim.leader at the time limit): x = (the defender's lost + 0.05) / (the attacker's lost + 0.05), the knot
	 * x / (1 + x) for the attacker and 1 / (1 + x) for the defender. A side that has broken off has lost whatever the
	 * exchange says, so from the break the knot stands on the other side's half, at least SettleBy past the middle; at
	 * the end, on the half of the winner the result names, even one that had broken off (the air thread's rule and the
	 * UI director's ruling, 21 November).
	 */
	double KnotOf(const Top& H);
	constexpr double SettleBy = 0.1;

	/**
	 * The shelling scene's own (the shore guns plan, sea-scene/shore-guns/plan.md R1.8; Rules/shore-scene.ts writes them):
	 * what a staged shelling adds to a recording, read only under -TheaterSeaShore=1 (bShore below) and only where the
	 * recording has it. Off, or in any other recording, all of it stays empty and every recording reads exactly as before;
	 * on, the show's own pieces, events and zones are read as before too. Nothing is drawn from it yet (R2 on): the show
	 * never sees it. Bearings here are degrees clockwise from north; places are board metres.
	 */
	/** Where a shell or a battery's round fell: x, y and its kind ('on', 'sea', 'beach', 'over'; a battery's 'short', 'over', 'straddle', 'hit'). */
	struct ShoreFall
	{
		double X = 0.0, Y = 0.0;
		std::string Kind;
	};
	/** A division's ground ashore: its middle, length along its face, width, and shelter ('open', 'dug', 'fort'). */
	struct ShoreStrip
	{
		double X = 0.0, Y = 0.0, Len = 0.0, Wide = 0.0, Face = 0.0;
		std::string Kind;
	};
	/** A battery of the coast's fort: where it stands, the way it faces and its bore (mm). */
	struct ShoreBattery
	{
		double X = 0.0, Y = 0.0, Face = 0.0, Mm = 0.0;
	};
	/** `field.coast`: the coast the fleet stands off. */
	struct Coast
	{
		bool bHas = false;
		/** The way the sea lies from the waterfront, and the waterfront. */
		double Bearing = 0.0, AtX = 0.0, AtY = 0.0;
		/** The land, as closed rings of board metres (at most ShoreRings rings of ShorePoints points in all). */
		std::vector<std::vector<std::pair<double, double>>> Shore;
		std::string Form, Relief, Kit, Land, Ground, Whose;
		double Rise = 0.0, Shelf = 0.0;
		/** The chief place, where drawn: its name, size, look, whether a port, its middle and reach, its quay, damage (0 to 1). */
		bool bTown = false, bPort = false, bQuay = false, bSkyline = false;
		std::string TownName, TownSize, TownLook;
		double TownX = 0.0, TownY = 0.0, TownR = 0.0, QuayX = 0.0, QuayY = 0.0, QuayFace = 0.0, QuayLen = 0.0, Damage = 0.0;
		/** What was shelled ('troops', 'town', 'city'), its middle and reach, and the troops' strips. */
		std::string TargetKind;
		double TargetX = 0.0, TargetY = 0.0, TargetR = 0.0;
		std::vector<ShoreStrip> Strips;
		/** The fort: its level, whether it fired, its batteries. */
		int FortLevel = 0;
		bool bFortFired = false;
		std::vector<ShoreBattery> Batteries;
		/** The spotting aircraft ('none', 'floatplane', 'carrier'), their nation and where from. */
		std::string AirSpot, AirNat, AirFrom;
	};
	inline constexpr size_t ShoreRings = 4096, ShorePoints = 200000;
	/** A shelling ship's guns, as her piece's extra says them: her main bore (75 to 460 mm) and how many. */
	struct ShoreGuns
	{
		int Unit = -1;
		double MainMm = 0.0;
		int Guns = 0;
	};
	/** One of a shelling's own events: sea:shore (a salvo at the coast), sea:battery (a battery's round), sea:fire (a fire
	 * ashore or on a deck), sea:spot (the spotting aircraft over the target). */
	struct ShoreEvent
	{
		double T = 0.0;
		std::string What, Kind, Nat;
		int Unit = -1, Ship = 0, N = 0, Bat = -1, On = -1;
		double Lands = -1.0, Mm = 0.0, Train = 0.0, X = 0.0, Y = 0.0, R = 0.0, Until = -1.0, Alt = 0.0;
		/** A salvo's shells by where they fell: on target, short in the water, short on the beach, over. */
		int LayOn = 0, LaySea = 0, LayBeach = 0, LayOver = 0;
		bool bDud = false;
		std::vector<ShoreFall> Falls;
	};
	/** A shot a staged window cuts to (`beats[].shots`): when (battle seconds), which shot, on which piece, at which spot. */
	struct Shot
	{
		double At = 0.0;
		std::string Kind;
		int Subject = -1;
		bool bSpot = false;
		double SpotX = 0.0, SpotY = 0.0;
	};

	struct Frame
	{
		Top Head;
		std::vector<SeaShow::Piece> Pieces;
		std::vector<SeaShow::Event> Events;
		std::vector<SeaShow::Zone> Zones;
		bool bZones = false; // the view carried its zones (else they are as before)
		/** The shelling scene's (under -TheaterSeaShore=1 only): the shelling ships' guns this view told, and its own events. */
		std::vector<ShoreGuns> Guns;
		std::vector<ShoreEvent> Shore;
	};

	/** What a reader keeps between views: each piece's still fields as last told. */
	struct Memory
	{
		std::map<int, SeaShow::Piece> Still;
	};

	/** TC.seaStart's field (and a recording's): the board, the navies, the look's own inputs. */
	bool ReadField(const Json& J, SeaShow::Field& Out);
	/** One TC.battleView() (or one recorded view); bShore (-TheaterSeaShore=1) reads the shelling scene's own too. */
	bool ReadView(const Json& J, Memory& Kept, Frame& Out, bool bShore = false);
	/** A shelling's `field.coast` (read under -TheaterSeaShore=1 only); false where the field has none. */
	bool ReadCoast(const Json& J, Coast& Out);

	/** The after card (TC.battleEnd(), or the recording's after). */
	struct After
	{
		std::vector<std::string> Lines;
		std::string Level, Winner;
		double Seconds = 0.0;
	};
	bool ReadAfter(const Json& J, After& Out);

	/** One of a staged scene's windows of battle time (the D1 stager's `field.staged.beats`): what it shows, from and to
	 * (battle seconds), and the seconds of screen it is given, so it plays at (To - From) / Screen. */
	struct Beat
	{
		std::string What;
		double From = 0.0, To = 0.0, Screen = 0.0;
		/** The shots it cuts to, in time (a shelling's, under -TheaterSeaShore=1 only; else empty). */
		std::vector<Shot> Shots;
	};

	/** A whole recorded action (tools/sea-record). */
	struct Recording
	{
		std::string Key;
		uint64_t Seed = 0;
		double Step = 1.0;
		SeaShow::Field Board;
		std::vector<Frame> Views;
		After Card;
		/** A staged scene's windows, in order, each a window of at least a tenth of a second given a tenth of a second
		 * of screen or more (empty for a fought action, which is played whole). Read only: the show never sees them. */
		std::vector<Beat> Beats;
		/** A shelling's coast (under -TheaterSeaShore=1 only; else bHas false). */
		Coast Shore;
	};
	/** bShore (-TheaterSeaShore=1): a shelling's own keys read too, where the recording has them; off, they are not read. */
	bool ReadRecording(const std::string& Source, Recording& Out, std::string& OutError, bool bShore = false);

	/** 0 the attacker, 1 the defender, -1 neither. */
	int SideOf(const std::string& Side);
}
