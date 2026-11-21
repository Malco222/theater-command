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
	};

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

	/** A whole recorded action (tools/sea-record). */
	struct Recording
	{
		std::string Key;
		uint64_t Seed = 0;
		double Step = 1.0;
		SeaShow::Field Board;
		std::vector<Frame> Views;
		After Card;
	};
	bool ReadRecording(const std::string& Source, Recording& Out, std::string& OutError);

	/** 0 the attacker, 1 the defender, -1 neither. */
	int SideOf(const std::string& Side);
}
