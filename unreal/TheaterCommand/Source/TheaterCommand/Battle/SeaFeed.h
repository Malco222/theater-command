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

	struct Frame
	{
		Top Head;
		std::vector<SeaShow::Piece> Pieces;
		std::vector<SeaShow::Event> Events;
		std::vector<SeaShow::Zone> Zones;
		bool bZones = false; // the view carried its zones (else they are as before)
	};

	/** What a reader keeps between views: each piece's still fields as last told. */
	struct Memory
	{
		std::map<int, SeaShow::Piece> Still;
	};

	/** TC.seaStart's field (and a recording's): the board, the navies, the look's own inputs. */
	bool ReadField(const Json& J, SeaShow::Field& Out);
	/** One TC.battleView() (or one recorded view). */
	bool ReadView(const Json& J, Memory& Kept, Frame& Out);

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
	};
	bool ReadRecording(const std::string& Source, Recording& Out, std::string& OutError);

	/** Whether the action was fought with the naval rules' "Friendly torpedoes" switch on, as far as what was read says it:
	 * its field's ownWater (SeaShow::Field::bOwnWater), or a view carrying the torpedo words' fields (heldFor, clearFrom:
	 * the rules write them only with the switch on). The torpedo words' gate (SeaWords::Write): off, nothing shows. A
	 * recording made with the switch on whose views never carry a field and whose field says nothing of it reads as off. */
	bool FriendlyTorpedoes(const Recording& Rec);

	/** 0 the attacker, 1 the defender, -1 neither. */
	int SideOf(const std::string& Side);
}
