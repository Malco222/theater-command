#pragma once

#include <cmath>

/**
 * (Release 2, a fleet under way, second part; Brian, 25 November 2026: "I want to keep upleveling the graphics and
 * smoothness of how they move and act to make it feel as real as possible".) How a ship carries herself on the
 * campaign map: she heels outward as she turns, with a ship's lag and one small roll back, and she rides the swell
 * where the eye is near enough to see it. Plain arithmetic with no engine in it, so that the cloud can check it
 * (Tools/ship_ride/run.sh). The piece (AArmyPiece) gives each ship of a fleet a small mesh of her own, about her own
 * middle on the water, and sets her by this every frame; her wake stays flat on the sea.
 *
 * Angles are in radians, lengths in whatever the caller measures her in, time in seconds. A ship's own frame is flat
 * on the water: the way her bows point (a unit), and her port side at a quarter turn from it, (Ahead.Y, -Ahead.X),
 * which is the left of a ship in a world where a turn to starboard is a turn from +X toward +Y.
 */
namespace TheaterShipRide
{
	inline constexpr double Degree = 0.017453292519943295;
	inline constexpr double TwoPi = 6.283185307179586;

	/** What kind of ship she is: how far she heels, how quick she is to it, and how she rides a swell. */
	struct FKind
	{
		double Heel;      // the most she heels in a hard turn at full speed
		double Quick;     // her roll's own rate, radians a second: how quick she is to heel and to come back
		double Damp;      // how soon her roll dies (under 1: she rolls back past where she will lie, once)
		double Pitch;     // how far her bows rise and fall in the swell
		double Roll;      // how far she rolls in it
		double PitchTime; // seconds from one rise of her bows to the next
		double RollTime;  // and from one roll to port to the next
		double Heave;     // how far she rises and falls bodily, as a share of her beam
	};

	/**
	 * 0 a battleship, 1 a carrier, 2 a cruiser, 3 a destroyer, 4 a submarine on the surface (anything else rides as a
	 * cruiser). The big ships are stiff and slow: a battleship leans five degrees and takes her time; a destroyer lies
	 * over nine and snaps back; a submarine, with a metre and a half of deck above the sea, leans four. (Each is held to what her own deck can take: Tools/ship_ride checks that no lee rail goes under.)
	 */
	inline FKind KindOf(int Class)
	{
		switch (Class)
		{
		case 0: return FKind{ 5.0 * Degree, 3.4, 0.62, 0.40 * Degree, 0.7 * Degree, 6.1, 7.6, 0.012 };
		case 1: return FKind{ 4.0 * Degree, 3.0, 0.66, 0.35 * Degree, 0.6 * Degree, 6.6, 8.3, 0.012 };
		case 3: return FKind{ 9.0 * Degree, 5.8, 0.50, 0.50 * Degree, 1.6 * Degree, 3.7, 4.3, 0.025 };
		case 4: return FKind{ 4.0 * Degree, 5.0, 0.58, 0.35 * Degree, 1.0 * Degree, 3.9, 4.7, 0.015 };
		default: return FKind{ 7.0 * Degree, 4.4, 0.55, 0.50 * Degree, 1.0 * Degree, 4.8, 5.6, 0.020 };
		}
	}

	/**
	 * The heel her turn asks of her: outward, so positive (to port) for a turn to starboard (Turn over nothing, in
	 * radians a second). It grows with how hard she turns, near its most by about a radian and a half a second, and
	 * with her speed (Pace: 0 lying still, 1 at her full speed): a ship swung round where she lies does not heel.
	 */
	inline double HeelWanted(const FKind& Kind, double Turn, double Pace)
	{
		const double Way = Pace < 0.0 ? 0.0 : Pace > 1.0 ? 1.0 : Pace;
		return Kind.Heel * std::tanh(Turn / 0.9) * Way;
	}

	/** Her heel as it stands: the angle, and how fast it is changing. */
	struct FHeel
	{
		double Angle = 0.0;
		double Rate = 0.0;
	};

	/**
	 * A frame's worth of her roll toward the heel wanted: a weight on a spring, damped. Steady however long the frame
	 * (it is taken in steps of no more than a 240th of a second, and a frame of over a quarter of a second counts as
	 * a quarter).
	 */
	inline void Lean(FHeel& Heel, double Wanted, double Seconds, const FKind& Kind)
	{
		const double Span = Seconds < 0.0 ? 0.0 : Seconds > 0.25 ? 0.25 : Seconds;
		const int Steps = static_cast<int>(std::ceil(Span * 240.0));
		if (Steps <= 0)
		{
			return;
		}
		const double Each = Span / Steps;
		for (int K = 0; K < Steps; ++K)
		{
			const double Pull = Kind.Quick * Kind.Quick * (Wanted - Heel.Angle) - 2.0 * Kind.Damp * Kind.Quick * Heel.Rate;
			Heel.Rate += Pull * Each;
			Heel.Angle += Heel.Rate * Each;
		}
	}

	/** True when her heel has as good as gone: nothing left for the eye to see settle. */
	inline bool Upright(const FHeel& Heel)
	{
		return std::fabs(Heel.Angle) < 0.02 * Degree && std::fabs(Heel.Rate) < 0.05 * Degree;
	}

	/** How she lies in the swell at one moment. */
	struct FSea
	{
		double Pitch = 0.0; // her bows down (positive) or up
		double Roll = 0.0;  // to port (positive) or to starboard
		double Heave = 0.0; // up (positive) or down, as a share of her beam
	};

	/**
	 * The swell at a time: two swells of unlike length for each of her motions, so that she never repeats herself in
	 * the time anyone watches. Phase tells ships apart (any number: two ships with unlike phases never move as one).
	 * Much is how much of it she shows, 0 to 1: it comes on and dies away as the eye comes near and leaves.
	 */
	inline FSea Swell(const FKind& Kind, double Time, double Phase, double Much)
	{
		const double Show = Much < 0.0 ? 0.0 : Much > 1.0 ? 1.0 : Much;
		FSea Sea;
		Sea.Pitch = Kind.Pitch * Show * (0.72 * std::sin(TwoPi * Time / Kind.PitchTime + Phase) + 0.28 * std::sin(TwoPi * Time / (Kind.PitchTime * 1.63) + 2.1 * Phase + 0.7));
		Sea.Roll = Kind.Roll * Show * (0.74 * std::sin(TwoPi * Time / Kind.RollTime + 1.7 * Phase + 1.0) + 0.26 * std::sin(TwoPi * Time / (Kind.RollTime * 0.61) + Phase + 2.3));
		Sea.Heave = Kind.Heave * Show * std::sin(TwoPi * Time / (Kind.PitchTime * 1.27) + 0.6 * Phase + 2.0);
		return Sea;
	}

	/** How she is set on the water: the way her masts point (a unit, in her own flat frame and up) and how far her middle is moved up. */
	struct FSet
	{
		double UpX = 0.0, UpY = 0.0, UpZ = 1.0;
		double Rise = 0.0; // (never above nothing: she is let down as she rides, so that her bottom does not show)
	};

	/** How much of what would lift clear of the sea she is let down by (the rest shows as a ship's bilge and forefoot do: a hand's breadth on the table). */
	inline constexpr double Let = 0.3;

	/**
	 * Her set for a roll (heel and swell together, to port positive), a pitch (bows down positive) and a heave (a
	 * share of her beam), for a ship Long by Beam whose bows point along (AheadX, AheadY). Her hull is built from the
	 * waterline up and has nothing under it, so she is let down a little as she rides: by a share (Let) of the
	 * most the swell lifts her ends and her side, all the time it shows (Much), so that she does not bob twice a
	 * pitch; and by the same share of what her heel lifts her high side.
	 */
	inline FSet Set(const FKind& Kind, double Long, double Beam, double AheadX, double AheadY, double Roll, const FSea& Sea, double Much)
	{
		const double Show = Much < 0.0 ? 0.0 : Much > 1.0 ? 1.0 : Much;
		const double Lean = Roll + Sea.Roll;
		const double PortX = AheadY, PortY = -AheadX;
		const double X = PortX * std::tan(Lean) + AheadX * std::tan(Sea.Pitch);
		const double Y = PortY * std::tan(Lean) + AheadY * std::tan(Sea.Pitch);
		const double Size = std::sqrt(X * X + Y * Y + 1.0);
		FSet Out;
		Out.UpX = X / Size;
		Out.UpY = Y / Size;
		Out.UpZ = 1.0 / Size;
		const double Draught = Show * Let * (0.5 * Long * std::sin(Kind.Pitch) + 0.5 * Beam * std::sin(Kind.Roll) + Kind.Heave * Beam);
		Out.Rise = Sea.Heave * Beam - Draught - Let * 0.5 * Beam * std::fabs(std::sin(Roll));
		return Out;
	}
}
