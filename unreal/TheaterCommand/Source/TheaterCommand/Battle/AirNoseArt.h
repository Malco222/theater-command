#pragma once

#include "Battle/AirFeed.h"
#include "Battle/AirShow.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

/**
 * The bombers' nose art (release 2; Brian, 25 November 2026 01:30Z: "american only"): each American bomber's painting
 * on her nose, one of the modeller's 200 (the air thread's list, names.json; Content/Data/NoseArt, packed by
 * Tools/pack_nose_art.py), on the panel laid on her model's skin (panels.json). Which painting each aircraft wears: where
 * the battle names her group's corps (its view's extra.corps) and the airStart board's place.bombers gives that corps'
 * seats (campaign/bombers.ts, old hands first), her seat's, the first of the seats her model stands for (the model in
 * slot k seats k * Per on, as the show's gunners name a bomber's slot); everywhere else (the rules do not name the corps
 * yet; the Bremen film, the test battles), the list's own rule (names.json's assign): each group's paintings shuffled
 * from the battle's seed and its id, given in turn, none shared in the battle while any is unused, and about one in five
 * a bare nose (a new replacement). An aircraft keeps hers for the whole battle, but for this: a bomber the battle names
 * as she falls, or falls out of her box (the loss's extra.bomber, the dispatch's "That was <name>"), wears that bomber's
 * painting from then, whichever model the show took for her, and the one of her group that wore it takes hers in turn.
 *
 * Engine-free, so the cloud test reads exactly what the game reads (Tools/air_show/air_nose_art_test.cpp). Nothing here
 * decides anything: the rules say who flies each bomber, and nothing drawn reads back.
 */
namespace AirNoseArt
{
	/** One number of the list (index.json): its name, whether it is painted, and its picture's file. */
	struct Entry
	{
		int Number = 0;
		std::string Name;
		std::string File;
		bool bPainted = false;
	};

	/** The list (Content/Data/NoseArt/index.json). */
	struct Index
	{
		std::vector<Entry> Art;          // by number, rising
		double BareChance = 0.2;         // the chance of a bare nose where the battle does not say who flies her
		double WideM = 2.2, HighM = 1.35; // a picture's size on her skin, metres (the painting fitted inside)
		int W = 0, H = 0;                // a picture's size, pixels
		/** A number's entry, or null. */
		const Entry* Of(int Number) const;
		/** The painted numbers, rising. */
		std::vector<int> Painted() const;
	};
	bool ReadIndex(const std::string& Text, Index& Out, std::string& Why);

	/** A point of a panel, in the model file's frame (metres, x forward, y right, z up): where it is, her skin's facing
	 * there, and its place on the picture (u from its left edge, v down from its top). */
	struct Point
	{
		double At[3] = { 0.0, 0.0, 0.0 };
		double Facing[3] = { 0.0, 0.0, 1.0 };
		double U = 0.0, V = 0.0;
	};

	/** One side's panel: its points (the grid, rows from the picture's top), its triangles (three points each, every one
	 * facing out as the engine draws a face: AirBattleTally.cpp's rule, which a turn of the frame does not change), and
	 * its middle, facing, up and right (the way u grows: the reader's right seen from outside). */
	struct Side
	{
		std::vector<Point> Points;
		std::vector<int> Triangles;
		double Middle[3] = { 0.0, 0.0, 0.0 };
		double Normal[3] = { 0.0, 0.0, 1.0 };
		double Up[3] = { 0.0, 0.0, 1.0 };
		double Right[3] = { 1.0, 0.0, 0.0 };
	};

	/** A model's nose panels (panels.json's models.<Kind>), port and starboard. */
	struct Panel
	{
		int Cols = 0, Rows = 0;
		double WideM = 2.2, HighM = 1.35, LiftM = 0.0;
		unsigned Skin = 0x4F5236u; // the paint it lies on (0xRRGGBB)
		Side Port, Starboard;
	};
	/** The panel of a model (Kind as its file is named: "B17"); false (and why) where it has none or it is not whole. */
	bool ReadPanel(const std::string& Text, const std::string& Kind, Panel& Out, std::string& Why);

	/** The ways of a triangle of a side, as the engine draws a face (AirBattleTally.cpp's FTallyPart::Add): its facing
	 * Cross(B - C, A - C). */
	void FacingOf(const Point& A, const Point& B, const Point& C, double Out[3]);

	/** Whether a painting is worn on both sides of her nose: the brief's "A few aircraft carry it on both sides", one
	 * number in eight (so a crew's aircraft carries it the same every mission). */
	bool BothSides(int Number);

	/** Whether an aircraft may wear nose art: an American bomber (Brian: "american only") whose model has a panel. */
	bool Wears(const std::string& Nation, bool bFighter, const std::string& Model, const std::function<bool(const std::string&)>& HasPanel);

	/** How a group's paintings are told: from its seats, its corps named by the battle (extra.corps); else dealt from the
	 * battle's seed. Never by the order of its groups: the battle numbers them in its ticket's order, the campaign's own
	 * order of its air groups (a newer one can stand ahead of older ones), and place.bombers is keyed by corps. */
	enum class Told { Dealt, ByCorps };

	/** Each bomber's painting in one battle. */
	class Book
	{
	public:
		/** A battle's start: the list, the battle's seed (AirSky::Placing's), and the board's seats (AirFeed::Sky::Bombers). */
		void Begin(const Index& List, uint64_t Seed, const std::map<int, AirFeed::Sky::BomberGroup>& Bombers);
		/** A frame's aircraft, in the show's order (keys rising, as Show::Planes gives them): each new one that may wear
		 * nose art is given her painting, the next of her group's (Per from her group, extra.per); then each the battle
		 * has newly named (PlaneOut::NamedArt) wears that bomber's painting. Groups are the show's (Show::Groups). */
		void Take(const std::map<int, AirShow::Group>& Groups, const std::vector<AirShow::PlaneOut>& Planes, const std::function<bool(const std::string&)>& HasPanel);
		/** An aircraft's painting: its number, 0 a bare nose, -1 none (she wears none, or has not been seen). */
		int ArtOf(int Key) const;
		/** How a group's paintings were told (Dealt for a group not seen). */
		Told HowTold(int Group) const;
		/** The aircraft given a painting so far, and how many of them bare. */
		int Given() const { return static_cast<int>(Art.size()); }
		int Bare() const;
		bool Begun() const { return bBegun; }
		/** The aircraft whose painting the last Take changed after she was first given one: a bomber the battle named as
		 * she fell or fell out of her box, and the one that handed that painting over to her. The scene paints them again. */
		const std::vector<int>& Repainted() const { return Changed; }

	private:
		struct Turn
		{
			std::vector<int> Order;   // the group's shuffle of the painted numbers
			size_t Next = 0;
			std::set<int> Own;        // the numbers its aircraft have had (cleared when its list runs out)
		};
		void Pair(const std::map<int, AirShow::Group>& Groups, const std::function<bool(const std::string&)>& HasPanel);
		int Deal(int Group, int Ordinal, int Per);
		/** A number as worn: itself where the list has it painted, else 0 (a bare nose). */
		int Usable(int Number) const;
		/** She wears the painting the battle named her by (P.NamedArt), the one that wore it taking hers. */
		void Rename(const AirShow::PlaneOut& P, const std::vector<AirShow::PlaneOut>& Planes);

		Index List;
		std::vector<int> Painted;
		uint64_t Seed = 0;
		std::map<int, AirFeed::Sky::BomberGroup> Bombers;
		bool bBegun = false;
		bool bPaired = false;
		std::map<int, int> CorpsOf; // a group's corps in Bombers, where it flies with its seats
		std::map<int, Told> How;
		std::map<int, Turn> Turns;
		std::map<int, int> Made;   // each group's aircraft given so far
		std::map<int, int> Art;    // each aircraft's painting (0 bare)
		std::set<int> Used;        // the numbers given in this battle
		std::map<int, int> Named;  // each aircraft's PlaneOut::NamedArt as last taken
		std::vector<int> Changed;  // the aircraft the last Take painted again
	};
}
