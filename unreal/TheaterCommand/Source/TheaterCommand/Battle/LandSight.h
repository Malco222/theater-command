#pragma once

#include "Battle/LandFeed.h"
#include "Battle/LandField.h"
#include "Battle/LandPlay.h"

#include <cstdint>
#include <vector>

/**
 * What stands where, for the ground battle's camera: whether a point is inside something the scene draws (a tank, a
 * gun, a man, a tree's trunk or crown, a hedge's shrubs, a house, a stone wall). Plain C++, so the cloud test asks the
 * same questions of the same field the game draws.
 *
 * The art director on the first film (21 November, 19:27Z): a fifth of it was over 40 per cent black, the camera pressed
 * on a German turret for 17 s, under a tree's crown for 10 s, on a dark hedge for 8, under the ground, inside a GI. The
 * camera knew only the ground and the banks; now it can ask about everything, and LandEye turns away any shot whose
 * lens is within 1.5 m of a thing (0.9 m of a man) or whose picture is more than a third filled by something within a
 * few metres of the lens.
 *
 * The field's own things (trees, shrubs, gates, carts, the houses and walls) are laid once; the men and the vehicles
 * move, so the scene (or the test) tells it where they are before each frame's questions.
 */
namespace LandSight
{
	using LandPlay::Vec;

	enum class Hit : uint8_t { None, Man, Thing };

	/** A man as a rod from his feet to the top of his head, 0.3 m round (lying: both ends near the ground). */
	struct Man
	{
		int Track = -1;
		Vec Feet, Top;
	};

	/** A tank, a gun or a lorry: its hull a box in its own axes (X forward, Y right, Z up from its root, as LandKit's
	 * model frame), turned by its heading; its turret a box as it stands facing ahead, turned about the box's own middle
	 * by the turret's way; and its barrel a rod from the turret's middle out to the muzzle. */
	struct Machine
	{
		int Track = -1;
		Vec Root;
		double Heading = 0.0, Turret = 0.0; // radians, 0 north, clockwise (Turret: where the turret faces on the field)
		Vec HullLo = Vec(-3.0, -1.5, 0.0), HullHi = Vec(3.0, 1.5, 1.9);
		bool bTurret = false;
		Vec TurretLo, TurretHi;
		bool bMuzzle = false;
		Vec Muzzle;
		/** A plain one by its size (the cloud test's, from the recording alone): a tank's turret over its middle and a
		 * barrel half its length beyond its nose. */
		static Machine Plain(int Track, const Vec& Root, double Heading, double Turret, double Long, double Wide, double High, bool bTurret);
	};

	/** The rules' recording's field as LandField takes it (the same the scene lays): the ways the men and vehicles go
	 * open the hedges they cross. */
	LandField::Field FieldOf(const LandFeed::Recording& Rec);
	/** The month (1 to 12) a recording's date names; July where none. */
	int MonthOf(const std::string& Date);

	class Room
	{
	public:
		/** Lays the field's own things in a grid of 10 m squares (once; about 20 ms for the dry run's 175,000). */
		void Lay(const LandField::Ground* InGround);
		/** The movers, again before each frame's questions. */
		void ClearMovers();
		void Add(const Man& M);
		void Add(const Machine& M);

		/** What a point is inside, if anything (never the ground: the camera asks the ground's height for itself).
		 * Except: a track left out (the man or the tank a shot is on). */
		Hit At(const Vec& P, int Except) const;

		size_t Things() const { return Kept.size(); }

	private:
		struct Thing
		{
			float X = 0.0f, Y = 0.0f, Z = 0.0f;
			/** A crown or a shrub: an upright ellipsoid about (X, Y, Mid) with radii Across and Half; a trunk under it,
			 * Trunk round, from Z to Mid (0: none). */
			float Mid = 0.0f, Across = 0.0f, Half = 0.0f, Trunk = 0.0f;
		};
		const LandField::Ground* Ground = nullptr;
		std::vector<Thing> Kept;
		std::vector<uint32_t> First, Order; // the grid: First[cell] .. First[cell + 1] in Order
		double X0 = 0.0, Y0 = 0.0, Cell = 10.0;
		int Cols = 0, Rows = 0;
		std::vector<Man> Men;
		std::vector<Machine> Machines;
	};
}
