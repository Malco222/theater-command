#pragma once

#include "Battle/AirShow.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/**
 * The finer B-17's bomber markings (the Battle rules card's "Bomber markings", row flyBomberMarks; Brian, 25 November
 * 2026, of the approved studio pictures: "Build it into the game"): her group's white triangle with its black letter on
 * her fin and her upper starboard wing, on every finer B-17, ridden, near and far; then (bundle 2) her squadron's two
 * letters and her own letter on her side, and her letter in yellow on her fin. Each mark a small vertex-painted mesh in
 * her model file's frame (Content/Data/AirMarks/b17-fine-marks.json, made by Tools/air_marks/make_b17_marks.py from the
 * model thread's livery file), drawn over her as an instanced copy with her own place (AirBattleLetters.cpp).
 *
 * Which letters each bomber wears is decided here, from the show's aircraft alone (never from the scene, never from a
 * random draw of the battle, never from the date): the same battle wears the same letters on every run, whatever the
 * frames. Engine-free, so the cloud test reads exactly what the game reads (Tools/air_show/air_letters_test.cpp).
 * Nothing here decides anything the rules count, and nothing drawn reads back.
 */
namespace AirLetters
{
	/** The 24 letters a group, squadron or plane is given: A to Z without I and O. */
	extern const char* const L24;

	/** One level of a part: its points and their facings (metres, her file's frame: x forward, y right, z up), its
	 * triangles (three points each, wound as her own file's), and its ink's name. */
	struct Level
	{
		std::vector<double> Points, Facings;
		std::vector<int> Triangles;
		std::string Ink;
		bool Empty() const { return Triangles.empty(); }
	};
	struct Part
	{
		Level Near, Far;
	};
	/** A group the history names: its triangle's letter and its four squadrons' codes. */
	struct KnownGroup
	{
		char Letter = 0;
		std::vector<std::string> Codes;
	};

	/** The data file (b17-fine-marks.json). */
	struct Sheet
	{
		int Bundle = 0;
		std::string ModelSha;
		std::map<std::string, Part> Parts;
		std::map<std::string, uint32_t> Inks; // 0xRRGGBB by ink name
		std::map<std::string, KnownGroup> Known;
		/** Reads it; false (and why) where it is not whole: every part a bundle names, at both levels. */
		bool Read(const std::string& Text, std::string& Why);
		/** A part's level, or null where the sheet has none. */
		const Level* LevelOf(const std::string& Name, bool bNear) const;
		/** An ink's colour (0xRRGGBB), white where unknown. */
		uint32_t InkOf(const std::string& Name) const;
	};

	/** What one bomber wears: her group's letter, her squadron's two letters, her own letter (0 or empty: none). */
	struct Wear
	{
		char Group = 0;
		std::string Code;
		char Letter = 0;
		bool operator==(const Wear& O) const { return Group == O.Group && Code == O.Code && Letter == O.Letter; }
	};

	/** FNV-1a, 64 bits, written out (the same on every machine and compiler, never std::hash). */
	uint64_t Fnv(const std::string& Text);

	/** Whether an aircraft wears them: an American B-17 (her group's or her own model "B17" or "B-17"), not a fighter. */
	bool Wears(const AirShow::PlaneOut& P);
	bool GroupWears(const AirShow::Group& G);
	/** A group's identity for its letters: its name, else "id:<Id>". */
	std::string IdentityOf(const AirShow::Group& G);

	/** The mark meshes a wear puts on her, by bundle: 1 "unit.<G>"; 2 adds "code.<XY>" and "letter.<L>". */
	std::vector<std::string> MeshesOf(const Wear& W, int Bundle);
	/** The sheet's parts in a mark mesh: "unit.C" -> fin.T, fin.T.C, wing.T, wing.T.C. */
	std::vector<std::string> PartsOf(const std::string& Mesh);
	/** A mesh's size for its reach on the screen, metres: the triangle 1.74, letters and codes 0.8. */
	double SizeOf(const std::string& Mesh);
	/** Whether a mark of that size is drawn: SizeM * PixelsPerMetreAt1 / AwayM at least MinPx (0.9 of it while it was
	 * drawn the frame before, so that it does not flicker at its edge). */
	bool Shown(double SizeM, double PixelsPerMetreAt1, double AwayM, double MinPx, bool bWas);

	/** Each bomber's wear in one battle. */
	class Book
	{
	public:
		/** A battle's start: every group that wears them lettered here, once, from the full set of the show's groups
		 * (the history's known groups first, then the rest in their identities' order). */
		void Begin(const Sheet& S, const std::map<int, AirShow::Group>& Groups);
		/** A frame's aircraft: each new one that wears them is given hers (her group's letter and codes; her squadron and
		 * letter from her place in her group, her key's order among her group's). A wear once given never changes. */
		void Take(const std::vector<AirShow::PlaneOut>& Planes);
		/** An aircraft's wear, or null (she wears none, or has not been seen). */
		const Wear* Of(int Key) const;
		int Given() const { return static_cast<int>(Worn.size()); }
		bool Begun() const { return bBegun; }
		/** A group's letter (0 where it has none) and its four codes. */
		char LetterOf(int Group) const;
		std::vector<std::string> CodesOf(int Group) const;
		/** Every mark mesh worn so far, for the scene to make before the flyover (WarmShapes). */
		std::vector<std::string> MeshesWorn(int Bundle) const;

	private:
		struct Lettered
		{
			char Letter = 0;
			std::vector<std::string> Codes;
			int Made = 0;
		};
		void LetterOne(int Id, const std::string& Identity);
		bool bBegun = false;
		std::map<std::string, KnownGroup> Known;
		std::map<int, Lettered> Groups;
		std::map<char, int> LettersUsed;
		std::map<std::string, int> CodesUsed;
		std::map<int, Wear> Worn;
	};
}
