#pragma once

#include <cmath>

/**
 * (Release 2, the campaign map; Brian, 26 November 2026: "the wake of the boats when moving don't follow the direction
 * they came from".) A fleet that lies on the heading she came in on.
 *
 * Before this every piece on the table was turned to face whoever looks at it, fleets too: a fleet's model has its bows
 * toward its own +Y, the right of the screen, and her wake lies straight astern, so at rest every fleet's wake streamed
 * off to the left of the screen whichever way she had come, and swung round with the eye. With tc.Map.FleetHeading her
 * ships and her wake keep the heading she arrived on, in the world, and only her standard (the pole, the colours, the
 * plate and its figures) is turned to the eye: it is turned back, about the piece's own upright, by as much as she is
 * turned away from the table.
 *
 * Plain arithmetic with no engine in it, so that the cloud can check it (Tools/fleet_heading/run.sh). The piece
 * (AArmyPiece::LieHeaded, TurnStandard) and the map (ACampaignMapActor::ShowFleets) use it.
 *
 * Angles are in degrees. A yaw is the engine's own: of a way over the map it is the angle from east round toward south
 * (the map's east is the world's +Y and its south the world's -X: a quarter turn, the same hand).
 */
namespace TheaterFleetHeading
{
	inline constexpr double Pi = 3.14159265358979323846;
	inline double ToDegrees(double Radians) { return Radians * (180.0 / Pi); }
	inline double ToRadians(double Degrees) { return Degrees * (Pi / 180.0); }

	/** An angle brought into (-180, 180]. */
	inline double Wrap(double Degrees)
	{
		double Wrapped = std::fmod(Degrees, 360.0);
		if (Wrapped > 180.0)
		{
			Wrapped -= 360.0;
		}
		else if (Wrapped <= -180.0)
		{
			Wrapped += 360.0;
		}
		return Wrapped;
	}

	/** A way shorter than this, in map units, has no direction worth the name. */
	inline constexpr double Shortest = 1.0;

	/** A way over the map: so far east and so far south, where it is known at all. */
	struct FWay
	{
		double East = 0.0, South = 0.0;
		bool bKnown = false;
		bool Leads() const { return bKnown && East * East + South * South >= Shortest * Shortest; }
	};

	/** The yaw of a fleet that sails that way over the map. */
	inline double YawOfWay(double East, double South) { return ToDegrees(std::atan2(South, East)); }

	/** The same of a heading in the world, as the sail reckons it (AArmyPiece::Tick): the world's +Y is the map's east, its -X the map's south. */
	inline double YawOfWorld(double WorldX, double WorldY) { return ToDegrees(std::atan2(-WorldX, WorldY)); }

	/** A fleet's bows in the world at a yaw: her model has its bows toward its own +Y. */
	inline void Bows(double Yaw, double& OutX, double& OutY)
	{
		OutX = -std::sin(ToRadians(Yaw));
		OutY = std::cos(ToRadians(Yaw));
	}

	/** A point turned about the upright, as the engine turns one by a yaw. */
	inline void Turn(double ByDegrees, double X, double Y, double& OutX, double& OutY)
	{
		const double C = std::cos(ToRadians(ByDegrees)), S = std::sin(ToRadians(ByDegrees));
		OutX = X * C - Y * S;
		OutY = X * S + Y * C;
	}

	/**
	 * What her standard is turned by, about the piece's own upright, so that it faces the eye while she lies on her own
	 * heading. Its foot is turned by the same about the piece's middle: so the standard stands where it stood on the
	 * screen, and reads as it read, whatever way her ships lie.
	 */
	inline double StandardTurn(double EyeYaw, double OwnYaw) { return Wrap(EyeYaw - OwnYaw); }

	/**
	 * Under the old sail, corner to corner, a fleet has her bows along her way for this share of it and then comes
	 * round to the table. One that lies headed has them along her way to the end.
	 */
	inline constexpr double RoundsToTableFrom = 0.84;
	inline bool HeadsHerWay(bool bLiesHeaded, double Progress) { return bLiesHeaded || Progress < RoundsToTableFrom; }

	/** For a fleet nobody has seen come from anywhere: a heading of her own, the same every time, no two alike side by side. */
	inline double YawOfId(int Id) { return Wrap(static_cast<double>(Id % 360) * 137.5); }

	/**
	 * The heading a fleet lies on when she has not been seen to sail in: the way she moved, where she changed her place
	 * and was simply found there; else out from her home port; else out of the sea her harbour lies on; else her own.
	 */
	inline double CameYaw(const FWay& Moved, const FWay& FromBase, const FWay& FromHome, int Id)
	{
		if (Moved.Leads())
		{
			return YawOfWay(Moved.East, Moved.South);
		}
		if (FromBase.Leads())
		{
			return YawOfWay(FromBase.East, FromBase.South);
		}
		if (FromHome.Leads())
		{
			return YawOfWay(FromHome.East, FromHome.South);
		}
		return YawOfId(Id);
	}
}
