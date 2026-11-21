#pragma once

#include "Battle/LandField.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * The ground battle's shell holes, in plain C++ so the cloud test lays the same ones the game draws (Brian on the first
 * stills, 21 November: "the graphics for the terrain need a lot of work"; a shell hole was a flat dark disc on the
 * grass). A fresh hole reads as torn earth: a raised ragged rim of thrown soil, drier and lighter at its crest; a
 * darker, damp hollow; rays of thrown soil running out over the grass; clods and torn turf scattered round it; brown
 * earth colours, never a black disc. Sized by the round: a mortar bomb, a 75 or an 88's high explosive, a 105. Every
 * hole stays for the whole battle (the art director's ruling).
 *
 * The ground is drawn opaque and nothing may sink under it, so a hole is laid over the ground, its floor a few
 * centimetres up: what reads as depth is the rim standing round it, the dark hollow and its walls' shading from dark at
 * the foot to dry soil at the crest. Colours are 8-bit sRGB vertex colours for the game's M_MapSurface (made linear
 * there), as LandField's are; the meshes are LandField::Mesh, in the field's frame (metres, x east, y north, z up),
 * counter-clockwise seen from above.
 *
 * Cost: a 105's hole is some 270 points and 480 triangles, a mortar's about 200 and 340; a few hundred holes by the
 * end of a battle are some 100,000 points. They are kept in chunks of PerChunk holes, each chunk one section of the
 * game's mesh, so a new hole lays only its own points and the game sends one chunk again.
 */
namespace LandCrater
{
	using LandField::Rgb8;

	/** How big a hole a round leaves. */
	struct Size
	{
		/** The hole across its rim's crest, metres (0: no hole). */
		double Across = 0.0;
		/** How high the rim's crest stands over the ground, metres. */
		double Rim = 0.0;
		/** How far the thrown soil reaches, as a share of the hole's radius (its rays a little further). */
		double Reach = 1.9;
		/** The clods thrown round it, and the biggest's size, metres. */
		int Clods = 0;
		double Clod = 0.1;
		/** Points round each ring (fewer for the small holes). */
		int Sides = 20;
	};

	/**
	 * The hole a round leaves, by the effects kit's name for it (LandPlay::Effect::Name) and its calibre: "grenade" a
	 * scuff half a metre across; "light" (a 37 to 57 mm high explosive) 0.9 m; "mortar" (the 81 mm bomb) 1.3 m; "gun75"
	 * a tank's or anti-tank gun's high explosive, 1.5 m for a 75 and 1.8 m for an 88 (by CalibreMm); "howitzer" or
	 * "field" (the 105) 2.8 m; "heavy" (a 150) 4 m. Any other name: by the calibre, a 75's from nothing given.
	 */
	Size SizeOf(const std::string& Name, double CalibreMm);

	/** The soil a hole is torn in (8-bit sRGB): its damp hollow, its wall, its dry crest, the soil thrown out, the
	 * thrown soil's far thin edge over the grass, and the clods (and torn turf's green). */
	struct Soil
	{
		Rgb8 Hollow, Wall, Crest, Thrown, Edge, Clod, Turf;
	};
	/** By the effects kit's ground word: "earth" (the bocage's brown loam), "woods" (darker, with leaf mould), "mud",
	 * "town" (grey with stone and plaster), "sand", "snow" (brown-black earth thrown on the white). Any other: earth. */
	Soil SoilOf(const std::string& Ground);

	/** The drawn ground's height at a place (the field's x and y, metres). */
	using HeightFn = std::function<double(double X, double Y)>;

	/**
	 * Lays one hole at X, Y into Out (appended), draped on the ground by Height; Seed makes it its own (the same seed,
	 * the same hole). Lift: metres more over the ground for this hole's floor, so two holes laid over each other do not
	 * flicker (Field gives each hole its own few millimetres). Nothing for a size of no width.
	 */
	void Lay(double X, double Y, const Size& S, const Soil& Paint, uint64_t Seed, const HeightFn& Height, double Lift, LandField::Mesh& Out);

	/** All of a battle's holes, kept for the whole battle, in chunks for the game's mesh sections. */
	class Field
	{
	public:
		static constexpr int PerChunk = 24;
		/** Beyond this many, no more are laid (never reached by a battle of the dry run's size; a guard). */
		static constexpr int Most = 2400;

		void Begin(HeightFn InHeight, uint64_t InSeed);
		/** A round landed at X, Y on Ground (the effects kit's word): its hole laid, by its name and calibre (SizeOf).
		 * False when it leaves none (a name of no size, water, the guard reached). */
		bool Add(double X, double Y, const std::string& Name, double CalibreMm, const std::string& Ground);
		/** The chunks a hole was laid in since the last call: each to be sent to the game's mesh again, whole. */
		std::vector<int> TakeChanged();
		int Chunks() const { return static_cast<int>(Meshes.size()); }
		const LandField::Mesh& Chunk(int K) const { return Meshes[static_cast<size_t>(K)]; }
		int Count() const { return static_cast<int>(Holes.size()); }
		/** Whether a point is in a hole or on its rim (the grass and corn laid round the eye leave it bare). */
		bool Bare(double X, double Y) const;

		struct Hole
		{
			double X = 0.0, Y = 0.0;
			/** Its radius at its rim's crest, and how high its crest stands. */
			double R = 0.0, Rim = 0.0;
		};
		const std::vector<Hole>& All() const { return Holes; }

	private:
		HeightFn Height;
		uint64_t Seed = 1;
		std::vector<Hole> Holes;
		std::vector<LandField::Mesh> Meshes;
		std::vector<int> Changed;
		/** The holes by 8 m squares of the field. */
		std::unordered_map<int64_t, std::vector<int>> Squares;
		static int64_t SquareOf(int I, int J);
	};
}
