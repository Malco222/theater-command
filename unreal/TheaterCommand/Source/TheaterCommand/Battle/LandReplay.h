#pragma once

// The battle's replay map (release 2; Brian's yes, 28 November 2026 00:09Z): after a land battle, the after card offers
// REPLAY MAP, and the whole battle plays again in 20 real seconds on a map seen from straight above, north up: the
// woods, the town and the water as a staff map draws them, every unit as a block of its side's ink, and behind each
// one that moved the way it went, with an arrowhead ahead of it. At the end it holds: the battle's story as arrows.
//
// Like the sports pages' diagram of the winning play: the same field, the same players, drawn from above so you see in
// a glance what you could not see from the touchline.
//
// Plain C++, no engine: it reads the battle's own recording (LandFeed::Recording, the views the rules sent, one a
// second of battle), so every block's place on the map is where the rules had it. The scene keeps the map and the
// player (LandBattleReplay.cpp), the screen draws them (LandBattleScreen.cpp, PaintReplay). Behind the ship list's row
// replaymap (-TheaterReplayMap), off until the Mac and the UI director pass it.

#include "Battle/LandFeed.h"

#include <string>
#include <utility>
#include <vector>

namespace LandReplay
{
	/** A patch of ground the map shades: woods, buildings, wet ground (water and marsh). */
	enum class Look
	{
		Woods,
		Town,
		Wet
	};

	/** A rectangle of the field's cells of one look, in field metres (x east, y north, from the south-west corner). */
	struct Patch
	{
		double X0 = 0.0, Y0 = 0.0, X1 = 0.0, Y1 = 0.0;
		Look Kind = Look::Woods;
	};

	/** One block at one view of the battle. */
	struct Mark
	{
		double T = 0.0, X = 0.0, Y = 0.0, Face = 0.0;
		std::string State;
		/** His side knew it then (his own; an enemy his side had met: live.ts landUnitOut's known; a film knows all). */
		bool bSeen = true;
	};

	/** A point of a block's way, kept where it had gone far enough from the last. */
	struct Step
	{
		double T = 0.0, X = 0.0, Y = 0.0;
	};

	/** One block over the whole battle. */
	struct Piece
	{
		int Id = 0, Side = -1;
		/** Its kind (inf, arm, art, ...; empty where his side never met it) and its ground: frontage and depth, metres. */
		std::string Kind;
		double W = 150.0, D = 40.0;
		std::vector<Mark> Marks;
		/** Its way, thinned: the first mark, every mark at least WayStep metres from the last kept, the last mark. */
		std::vector<Step> Way;
	};

	/** The map: the field, the ground's patches, the town, every block, the battle's span. */
	struct Map
	{
		double W = 0.0, H = 0.0;
		/** The battle time of the first view and of the last: the replay's span. */
		double T0 = 0.0, T1 = 0.0;
		/** The side he commanded (0 the attacker, 1 the defender), -1 a film. */
		int Me = -1;
		std::string Title, TownName;
		double TownX = 0.0, TownY = 0.0, TownR = 0.0;
		/** The two sides' adjectives (American, German, ...), for a film's key. */
		std::string Adj[2];
		std::vector<Patch> Patches;
		std::vector<Piece> Pieces;
	};

	/** A block's way keeps a point once it is this far from the last kept (metres). */
	constexpr double WayStep = 20.0;
	/** A block whose way is shorter than this has not moved: no arrow (metres). */
	constexpr double LeastMove = 60.0;

	/** The ground word's look ("woods", "park": woods; "buildings", "suburb", "industry", "works", "bunker": town;
	 * "water", "marsh": wet), false for the rest (open, field, road, rough, ...). */
	bool LookOf(const std::string& Ground, Look& Out);

	/** The map from the battle's recording; false (and why) where it has no field or no views to replay. */
	bool Build(const LandFeed::Recording& Rec, Map& Out, std::string& OutWhy);

	/** Where a block stands at a battle time, as the map draws it. */
	struct Pose
	{
		/** On the field yet (before its first view: not). */
		bool bThere = false;
		double X = 0.0, Y = 0.0, Face = 0.0;
		bool bSeen = true;
		/** Its state's word then: gone (lost), off the field (withdrawn), running (routing). */
		bool bGone = false, bOff = false, bRouting = false;
	};

	/** The block's pose at a battle time: between its two views, the place and facing in proportion; the state and what
	 * his side knew from the earlier. */
	Pose PoseAt(const Piece& P, double T);

	/** The block's way up to a battle time: its kept points before it, and its place then last. */
	std::vector<std::pair<double, double>> WayTo(const Piece& P, double T);

	/** The length of a way (metres). */
	double Length(const std::vector<std::pair<double, double>>& Way);

	/** The four corners of a block's ground at a pose (front left, front right, back right, back left), field metres. */
	std::vector<std::pair<double, double>> Corners(const Piece& P, const Pose& At);

	/** The field laid into a box on the screen, north up, as large as fits, centred. */
	struct Fit
	{
		double Scale = 1.0, X0 = 0.0, Y0 = 0.0, W = 0.0, H = 0.0;
		/** The field (W by H metres) into the box (its left, top, width, height in screen units). */
		void Into(double FieldW, double FieldH, double BoxX, double BoxY, double BoxW, double BoxH);
		/** A field point (metres) on the screen. */
		std::pair<double, double> On(double X, double Y) const;
	};

	/** The 20 real seconds: a second on the battle's first view, then the battle at an even pace to its last view, which
	 * holds when it is done. */
	class Player
	{
	public:
		static constexpr double Length = 20.0;
		/** The first view held this long before the blocks move (real seconds, inside the 20). */
		static constexpr double Lead = 1.0;

		void Open();
		void Again();
		void Close();
		/** A frame's real seconds. */
		void Tick(double Real);

		bool IsOpen() const { return bOpen; }
		bool IsDone() const { return Age >= Length; }
		/** Real seconds since it began (at most Length). */
		double Played() const { return Age; }
		/** The battle time shown now. */
		double BattleAt(const Map& M) const;

	private:
		bool bOpen = false;
		double Age = 0.0;
	};

	/** A battle time as the map's clock shows it: minutes and seconds ("4:05"). */
	std::string Clock(double Seconds);
}
