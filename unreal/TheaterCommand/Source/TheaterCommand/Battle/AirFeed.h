#pragma once

#include "Battle/AirShow.h"
#include "Battle/AirSky.h"

#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * The air battle's words read into the show: the bridge's JSON (TC.airStart's sky, each TC.battleView, TC.battleEnd's
 * after card) or a recorded battle (Content/Data/AirBattles/<key>.json, written by the rules repo's
 * tools/air-record), read the same way, in plain C++ so the cloud test reads exactly what the game reads.
 *
 * Nothing here decides anything: it only reads (air 3d-handoff.md, sections 1, 2, 4, 6, 8 and 9).
 */
namespace AirFeed
{
	/** A JSON value, small and plain (no exceptions, no allocation tricks: the views are a few kilobytes each). */
	struct Json
	{
		enum class Type { Null, Bool, Number, String, Array, Object };
		Type Kind = Type::Null;
		bool Flag = false;
		double Value = 0.0;
		std::string Text;
		std::vector<Json> Items;
		/** An object's fields: names and values side by side, in the order written. */
		std::vector<std::string> Keys;
		std::vector<Json> Values;

		/** The field by name, or a shared null. */
		const Json& operator[](const char* Key) const;
		const Json& operator[](const std::string& Key) const { return (*this)[Key.c_str()]; }
		/** The item by index, or a shared null. */
		const Json& At(size_t Index) const;
		size_t Count() const { return Kind == Type::Array ? Items.size() : Kind == Type::Object ? Values.size() : 0; }
		bool IsNull() const { return Kind == Type::Null; }
		bool IsNumber() const { return Kind == Type::Number; }
		bool IsString() const { return Kind == Type::String; }
		bool IsArray() const { return Kind == Type::Array; }
		bool IsObject() const { return Kind == Type::Object; }
		double Num(double Default = 0.0) const { return Kind == Type::Number ? Value : Kind == Type::Bool ? (Flag ? 1.0 : 0.0) : Default; }
		int Int(int Default = 0) const;
		bool Truth(bool Default = false) const { return Kind == Type::Bool ? Flag : Kind == Type::Number ? Value != 0.0 : Default; }
		const std::string& Str() const;
		std::string Str(const char* Default) const { return Kind == Type::String ? Text : std::string(Default); }
	};

	/** Reads JSON text; false (and why) on a fault. */
	bool Parse(const std::string& Source, Json& OutValue, std::string& OutError);

	/** What arrives once, at the start (TC.airStart(...).sky): the board, the place, the loadouts. */
	struct Sky
	{
		double W = 100000.0, H = 100000.0, Scale = 2.0;
		double TargetX = 50000.0, TargetY = 70000.0, TargetR = 6000.0, FlakR = 12000.0;
		std::vector<double> Bands;
		std::string Title, Date;
		std::string AttNat, DefNat, AttAdj, DefAdj;
		bool bAttHuman = true, bDefHuman = false;
		/** place: its kind (harbour, fleet, city, sky), its name, where the raiders fly from, the month, the night. */
		std::string PlaceKind, PlaceName, From, Terrain, StageKey;
		/** The fleet the raid is after (place.ships.law.fleets[0].name: "Pacific Fleet", "Mobile Force"), or empty. */
		std::string FleetName;
		/** The set piece's stage, turned onto the board (place.ships.stage: its aim in stage metres, its turn). */
		double StageAimX = 0.0, StageAimY = 0.0, StageTurn = 0.0;
		bool bNight = false;
		int Year = 1939, Month = 8;
		double CardChance = 0.0; // place.chance, the raid card's (the raider's)
		std::map<std::string, AirShow::Loadout> Guns; // "<nat>:ftr" and "<nat>:bmr"
		std::map<std::string, std::string> Models;    // "<nat>:ftr" and "<nat>:bmr", the place's main models
		/** The land under the board (place.ground.land): rings of board points. */
		std::vector<std::vector<std::pair<double, double>>> Land;
		std::vector<std::vector<std::pair<double, double>>> TargetOutline;
		/** The board's great rivers (place.ground.rivers): lines of board points. */
		std::vector<std::vector<std::pair<double, double>>> Rivers;
		/** True north on the turned board (place.ground.north, a unit vector [x, y]); none given: the board's own up. */
		double NorthX = 0.0, NorthY = 1.0;
		bool bNorthGiven = false;
		/** The place's latitude in degrees north (place.lat), for the sun over a raid with no stage; none given: NaN. */
		double Latitude = std::numeric_limits<double>::quiet_NaN();
		/** The day's vapour-trail height, metres (TrailHeight: from the place's weather zone and month, never a draw). */
		double TrailM = 0.0;
		/** The day's sky (place.cloud): a deck or scattered cumulus, its base and top; absent, cumulus. */
		AirSky::Day Cloud;
	};
	bool ReadSky(const Json& J, Sky& Out);
	/** The day's vapour-trail height, metres, by the weather zone (place.cloud.zone), the month (from 0) and the
	 * latitude (NaN: not given). */
	double TrailHeight(const std::string& Zone, int Month, double Latitude);
	/** Hands the board and the loadouts to the show. */
	void Apply(const Sky& S, AirShow::Show& Show);
	/** Where and for what battle its sky is laid: the battle's seed, the target, the raid's bands (of the raider's
	 * bombers in the first view: the lowest above 2,000 m, and the highest at 2,000 m or lower; the middle band if it
	 * has none) and whether it is a strike at ships. The caller adds the sun and the ground. */
	AirSky::Placing PlacingOf(const Sky& S, const std::vector<AirShow::Group>& First, uint64_t Seed);

	/** A side's bar under HOW NEAR TO BREAKING OFF, and why (ammo or nerve). */
	struct Near
	{
		double At = 0.0;
		std::string Why;
	};

	/** The top of a view: the clock, the bars, the aim slip, the names. */
	struct Top
	{
		double T = 0.0, Limit = 0.0, Speed = 1.0;
		double BalanceAtt = 0.5;
		double BreakingAtt = 0.0, BreakingDef = 0.0;
		double Contact = -1.0;      // the battle time of the first contact (-1 none yet)
		std::string Me = "att";
		bool bOver = false;
		std::string Winner, Level;
		std::vector<std::string> OverLines;
		/** The sky block (land's diff): the staff's chance, the aim slip, the near lines. */
		double Chance = 0.0;
		bool bFired = false, bWinding = false;
		Near NearAtt, NearDef;
		int LostAtt = 0, LostDef = 0, UpAtt = 0, UpDef = 0;
		double Worth = 0.0, Ratio = 1.0;
		bool bCut = false, bRuined = false;
		/** sim.called(u): only names new or changed since the last view (the screen keeps them). */
		std::map<int, std::string> Called;
	};

	/** A ship on the board as the view gives it (its name and class for a label; the show gets AirShow::Ship). */
	struct ShipRow
	{
		int Id = 0;
		std::string Name, Class, Nation, Berth;
		double X = 0.0, Y = 0.0, Face = 0.0;
		int Hits = 0;
		bool bSunk = false;
		bool bDock = false; // in a dry dock, on the stage's land
		std::string SunkHow;
	};

	/** A ship's hull as she is drawn: her length overall and her beam, metres. */
	struct HullSize
	{
		double Long = 0.0, Beam = 0.0;
	};

	/**
	 * Her real size: by her name where the record names her (her 1941-42 figures, her anti-torpedo bulges where she had
	 * them by then), else her class's in her navy. The kit's ship models are one per class and nation (the American
	 * battleship is an Iowa, 270 m), so the scene stretches each to this: on Battleship Row the Iowas had overrun each
	 * other (Brian on the film of build 1768, 20 November). The flight test lays the same hulls.
	 */
	HullSize HullOf(const std::string& Class, const std::string& Name, const std::string& Nation);

	/** A unit's words for the screen (the show has the rest). */
	struct Label
	{
		int Id = 0;
		int Side = 0;
		std::string Name, State, Act, Job, Order;
		int N = 0, N0 = 0, Figures = 0;
		double Ammo = 1.0, Fuel = 1.0;
		int Band = 1;
	};

	struct Frame
	{
		Top Head;
		std::vector<AirShow::Group> Groups;
		std::vector<AirShow::Event> Events;
		std::vector<AirShow::Ship> Ships;
		std::vector<ShipRow> ShipRows;
		std::vector<Label> Labels;
	};
	/** One TC.battleView() (or one recorded view) read against the sky. */
	bool ReadView(const Json& J, const Sky& S, Frame& Out);

	/** The after card (TC.battleEnd().after, or the recording's after). */
	struct AfterRow
	{
		std::string Name, Side, Kind, Words;
		int Flew = 0, Lost = 0;
	};
	struct After
	{
		std::vector<std::string> Lines;
		std::string Level, Reckoning;
		std::vector<AfterRow> Groups;
		int LostAttFtr = 0, LostAttBmr = 0, LostDefFtr = 0, LostDefBmr = 0;
	};
	bool ReadAfter(const Json& J, After& Out);

	/** A whole recorded battle (tools/air-record). */
	struct Recording
	{
		std::string Key;
		uint64_t Seed = 0;
		Sky Board;
		std::vector<Frame> Views;
		After Card;
	};
	bool ReadRecording(const std::string& Source, Recording& Out, std::string& OutError);

	/** 0 the raider, 1 the defender, -1 neither. */
	int SideOf(const std::string& Side);
	AirShow::Act ActOf(const std::string& Act);
}
