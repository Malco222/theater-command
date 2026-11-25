#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

/**
 * Armies with weight on the campaign map (release 2; Brian, 25 November 2026: "I want to keep upleveling the graphics
 * and smoothness of how they move and act to make it feel as real as possible", "can we do that for the campaign map
 * too?", and "I want this to visually wow people on how smooth and realistic and detailed grand it looks").
 *
 * An army piece was picked up, carried and set down along straight lines from one province to the next, rising and
 * falling on one even arc and stopping dead where it stood: it moved as a drawing moves. This is what a heavy thing in
 * a hand does instead:
 *
 *   The road (Round). The hand does not carry a piece to the middle of each province and turn there: it cuts each
 *   corner on a curve. The curve never leaves the corner it rounds, so the piece still passes through every
 *   province it is said to cross.
 *
 *   The carry (Lifted, FromHand). It is taken up before it goes, carried level, brought over its place, and set
 *   down the last of the way: quicker at the end than at the start, because it is let go. The lift and the drop
 *   take about a quarter of a second each however long the march.
 *
 *   The hang (Swing, Lean). A piece is held by its top and is heavy at its foot. As the hand gathers speed the foot
 *   hangs back and the top leans the way it goes; as the hand slows the foot swings on under it; round a bend it
 *   swings wide. A weight on a spring, damped: it swings once past where it will hang, and is still.
 *
 *   The set-down (Kick, and the same spring made stiff). It lands on one edge of its base and rocks once to the other.
 *
 *   The dust (Skirt, Billow). A low ring runs out along the table from under the base as it lands, and a slower
 *   cloud rises behind it: more of both for a bigger army, white and short-lived on snow, pale and lingering on sand.
 *
 * Plain C++, no engine, so that it can be built and checked alone (Tools/army_carry/run.sh). The piece (AArmyPiece)
 * carries itself by this; the map (ACampaignMapActor::UpdateSmoke) draws the dust. Angles are in radians, time in
 * seconds, lengths in whatever the caller measures the table in.
 */
namespace TheaterArmyCarry
{
	inline constexpr double Degree = 0.017453292519943295;

	struct FSpot
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
	};

	inline FSpot operator+(FSpot A, FSpot B) { return FSpot{ A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
	inline FSpot operator-(FSpot A, FSpot B) { return FSpot{ A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
	inline FSpot operator*(FSpot A, double K) { return FSpot{ A.X * K, A.Y * K, A.Z * K }; }
	inline double Dot(FSpot A, FSpot B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	inline double Size(FSpot A) { return std::sqrt(Dot(A, A)); }
	inline double Clamp(double V, double Low, double High) { return V < Low ? Low : V > High ? High : V; }
	/** An ease with no speed at either end. */
	inline double Smooth(double S)
	{
		const double T = Clamp(S, 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}
	/** And one that also gathers and loses its speed gently: what is carried by it is never jerked. */
	inline double Smoother(double S)
	{
		const double T = Clamp(S, 0.0, 1.0);
		return T * T * T * (T * (6.0 * T - 15.0) + 10.0);
	}

	/**
	 * The road with its corners rounded. Each corner is cut by a share (Much, 0 to 1) of half the shorter of the two
	 * legs that meet there, and by no more than Most; the curve between runs from the one leg to the other inside the
	 * corner it replaces, leaving each leg along that leg. The ends are kept exactly. A road of two points, or one with
	 * nothing to round, comes back as it was given (but for points that repeat the one before, which are dropped).
	 */
	inline void Round(const std::vector<FSpot>& Road, double Much, double Most, std::vector<FSpot>& Out)
	{
		Out.clear();
		std::vector<FSpot> Way;
		for (const FSpot& Point : Road)
		{
			if (Way.empty() || Size(Point - Way.back()) > 1.0e-6)
			{
				Way.push_back(Point);
			}
		}
		if (Way.size() < 3 || Much <= 0.0 || Most <= 0.0)
		{
			Out = Road.size() < 2 || Way.size() < 2 ? Road : Way;
			return;
		}
		const double Share = 0.5 * Clamp(Much, 0.0, 1.0);
		auto Put = [&Out](FSpot Point)
		{
			if (Out.empty() || Size(Point - Out.back()) > 1.0e-6)
			{
				Out.push_back(Point);
			}
		};
		Put(Way.front());
		for (std::size_t Corner = 1; Corner + 1 < Way.size(); ++Corner)
		{
			const FSpot In = Way[Corner] - Way[Corner - 1], On = Way[Corner + 1] - Way[Corner];
			const double LongIn = Size(In), LongOn = Size(On);
			const double Shorter = LongIn < LongOn ? LongIn : LongOn;
			const double Cut = Share * Shorter < Most ? Share * Shorter : Most;
			const FSpot AlongIn = In * (1.0 / LongIn), AlongOn = On * (1.0 / LongOn);
			const double Turn = std::acos(Clamp(Dot(AlongIn, AlongOn), -1.0, 1.0));
			if (Cut < 1.0e-6 || Turn < 2.0 * Degree)
			{
				Put(Way[Corner]);
				continue;
			}
			const FSpot From = Way[Corner] - AlongIn * Cut, To = Way[Corner] + AlongOn * Cut;
			const int Steps = static_cast<int>(Clamp(std::ceil(Turn / (8.0 * Degree)), 3.0, 16.0));
			for (int Step = 0; Step <= Steps; ++Step)
			{
				const double T = static_cast<double>(Step) / Steps;
				Put(From * ((1.0 - T) * (1.0 - T)) + Way[Corner] * (2.0 * T * (1.0 - T)) + To * (T * T));
			}
		}
		Put(Way.back());
	}

	/** Where the hand has the piece at one moment: how far along its road (0 to 1) and how high off the table (0 to 1 of its lift). */
	struct FHeld
	{
		double Along = 0.0;
		double High = 0.0;
	};

	/** The shares of a march's time it spends being taken up and being set down: about a quarter of a second each. */
	inline double UpShare(double Seconds) { return Clamp(0.24 / (Seconds > 1.0e-3 ? Seconds : 1.0e-3), 0.10, 0.20); }
	inline double DownShare(double Seconds) { return Clamp(0.27 / (Seconds > 1.0e-3 ? Seconds : 1.0e-3), 0.11, 0.24); }

	/**
	 * A piece lifted, carried and set down in Seconds, at a share (Progress) of that time. It is clear of the table
	 * before it goes; it is over its place before it lands, and comes down the last of the way straight; and it
	 * lands moving, not eased: it is let go.
	 */
	inline FHeld Lifted(double Progress, double Seconds)
	{
		const double P = Clamp(Progress, 0.0, 1.0);
		const double Up = UpShare(Seconds), Down = DownShare(Seconds);
		const double Leaves = 0.3 * Up, Over = 1.0 - 0.55 * Down;
		FHeld Held;
		Held.Along = Smoother((P - Leaves) / (Over - Leaves));
		if (P < Up)
		{
			Held.High = Smooth(P / Up);
		}
		else if (P <= 1.0 - Down)
		{
			Held.High = 1.0;
		}
		else
		{
			const double Fallen = Clamp((P - (1.0 - Down)) / Down, 0.0, 1.0);
			Held.High = 1.0 - Fallen * Fallen;
		}
		return Held;
	}

	/**
	 * A piece set down from the hand that holds it: brought over its place quickly, then let down, faster as it
	 * goes. High is 1 at the height it was held at and 0 on its place.
	 */
	inline FHeld FromHand(double Progress)
	{
		const double P = Clamp(Progress, 0.0, 1.0);
		const double Short = 1.0 - Clamp(P / 0.6, 0.0, 1.0);
		const double Fallen = Clamp((P - 0.18) / 0.82, 0.0, 1.0);
		FHeld Held;
		Held.Along = 1.0 - Short * Short * Short;
		Held.High = 1.0 - Fallen * Fallen;
		return Held;
	}

	/**
	 * How the piece hangs from the hand: how far its foot lies behind the point straight under the hand (X, Y: the
	 * hand less the foot, flat on the table) and how fast that is changing; how fast the hand itself was last moving,
	 * and the push it has lately given the piece (so that a hand that starts, stops or turns between one frame and
	 * the next is felt, and felt as a push and not as a blow).
	 */
	struct FHang
	{
		double X = 0.0, Y = 0.0;
		double RateX = 0.0, RateY = 0.0;
		double HandRateX = 0.0, HandRateY = 0.0;
		double PushX = 0.0, PushY = 0.0;
	};

	/** How quick the hang is to answer (radians a second) and how soon it dies (under 1: it swings past once). */
	struct FSpring
	{
		double Quick;
		double Damp;
	};
	/**
	 * In the air it swings like a weight on a short cord. As it is brought down the hand steadies it, so that it lands
	 * all but upright. On the table it rocks, quick and soon over.
	 */
	inline constexpr FSpring InTheAir{ 10.5, 0.42 };
	inline constexpr FSpring ComingDown{ 20.0, 0.9 };
	inline constexpr FSpring OnTheTable{ 30.0, 0.34 };

	/** The length a piece hangs on, in the piece's own unit (a map unit at the size pieces are drawn), and the most it ever leans. */
	inline constexpr double HangsOn = 60.0;
	inline constexpr double MostLean = 8.0 * Degree;
	/**
	 * A hand is not a machine: what it does to the piece comes on over a few hundredths of a second (Soft), and it
	 * never throws the piece harder than Throw (radians a second, each second, of its hang). Pieces are carried
	 * across the table at several provinces a second, and without these a corner of the road would whip the piece
	 * from one side to the other between two frames.
	 */
	inline constexpr double Soft = 0.06;
	inline constexpr double Throw = 21.5;

	/**
	 * The hang over a stretch of time, From to To (seconds, on any clock), with the hand where Where(time) says it is
	 * (an FSpot; only X and Y are read), for a piece that hangs on Reach. Steady however long the frame: it is taken
	 * in steps of no more than a 240th of a second, and of a frame of over a quarter of a second only the last
	 * quarter is counted.
	 */
	template <typename FWhere>
	inline void Swing(FHang& Hang, FWhere&& Where, double From, double To, const FSpring& Spring, double Reach)
	{
		double Start = From;
		if (To - Start > 0.25)
		{
			Start = To - 0.25;
		}
		const double Span = To - Start;
		const int Steps = static_cast<int>(std::ceil(Span * 240.0));
		if (Steps <= 0)
		{
			return;
		}
		const double Each = Span / Steps;
		FSpot Was = Where(Start);
		const double Pull = Spring.Quick * Spring.Quick, Drag = 2.0 * Spring.Damp * Spring.Quick;
		const double Hardest = Throw * (Reach > 0.0 ? Reach : 0.0), Blend = Each / (Soft + Each);
		for (int Step = 1; Step <= Steps; ++Step)
		{
			const FSpot Here = Where(Start + Each * Step);
			const double HandRateX = (Here.X - Was.X) / Each, HandRateY = (Here.Y - Was.Y) / Each;
			Hang.PushX += ((HandRateX - Hang.HandRateX) / Each - Hang.PushX) * Blend;
			Hang.PushY += ((HandRateY - Hang.HandRateY) / Each - Hang.PushY) * Blend;
			const double Hard = std::sqrt(Hang.PushX * Hang.PushX + Hang.PushY * Hang.PushY);
			const double Held = Hard > Hardest && Hard > 0.0 ? Hardest / Hard : 1.0;
			Hang.RateX += (Hang.PushX * Held - Pull * Hang.X - Drag * Hang.RateX) * Each;
			Hang.RateY += (Hang.PushY * Held - Pull * Hang.Y - Drag * Hang.RateY) * Each;
			Hang.X += Hang.RateX * Each;
			Hang.Y += Hang.RateY * Each;
			Hang.HandRateX = HandRateX;
			Hang.HandRateY = HandRateY;
			Was = Here;
		}
	}

	/**
	 * The hang of a piece lifted, carried and set down in Seconds, over a stretch of that time: swinging free until
	 * the hand begins to bring it down, and steadied from there.
	 */
	template <typename FWhere>
	inline void Carried(FHang& Hang, FWhere&& Where, double From, double To, double Seconds, double Reach)
	{
		const double Comes = (1.0 - DownShare(Seconds)) * Seconds;
		if (From < Comes && To > Comes)
		{
			Swing(Hang, Where, From, Comes, InTheAir, Reach);
			Swing(Hang, Where, Comes, To, ComingDown, Reach);
		}
		else
		{
			Swing(Hang, Where, From, To, To > Comes ? ComingDown : InTheAir, Reach);
		}
	}

	/** How far the top of the piece leans, and which way on the table: an angle along X and one along Y. */
	struct FLean
	{
		double X = 0.0;
		double Y = 0.0;
	};

	/**
	 * The lean a hang shows. Reach is the length the piece hangs on (the further its foot is behind, the more it
	 * leans: one part in Reach is a radian, while the lean is small), and Most the lean it never passes however
	 * hard the hand is thrown about.
	 */
	inline FLean Lean(const FHang& Hang, double Reach, double Most)
	{
		const double Behind = std::sqrt(Hang.X * Hang.X + Hang.Y * Hang.Y);
		if (Behind < 1.0e-9 || Reach <= 0.0 || Most <= 0.0)
		{
			return FLean{};
		}
		const double Angle = Most * std::tanh(Behind / (Reach * Most));
		return FLean{ Hang.X / Behind * Angle, Hang.Y / Behind * Angle };
	}

	/** True when the hang has as good as gone and the hand is still: nothing left for the eye to see settle. */
	inline bool Still(const FHang& Hang, double Reach)
	{
		if (Reach <= 0.0)
		{
			return true;
		}
		const double Behind = std::sqrt(Hang.X * Hang.X + Hang.Y * Hang.Y) / Reach;
		const double Rate = std::sqrt(Hang.RateX * Hang.RateX + Hang.RateY * Hang.RateY) / Reach;
		const double Hand = std::sqrt(Hang.HandRateX * Hang.HandRateX + Hang.HandRateY * Hang.HandRateY) / Reach;
		const double Push = std::sqrt(Hang.PushX * Hang.PushX + Hang.PushY * Hang.PushY) / Reach;
		return Behind < 0.03 * Degree && Rate < 0.5 * Degree && Hand < 0.5 * Degree && Push < 5.0 * Degree;
	}

	/** How hard a piece is tipped as it lands (radians a second), before the size of the army is counted. */
	inline constexpr double Landing = 1.7;

	/** The knock of landing: the top is thrown on the way (AlongX, AlongY: any length; nothing, and it is thrown along +X). */
	inline void Kick(FHang& Hang, double AlongX, double AlongY, double Rate, double Reach)
	{
		const double Long = std::sqrt(AlongX * AlongX + AlongY * AlongY);
		const double X = Long > 1.0e-9 ? AlongX / Long : 1.0, Y = Long > 1.0e-9 ? AlongY / Long : 0.0;
		Hang.RateX += X * Rate * Reach;
		Hang.RateY += Y * Rate * Reach;
		Hang.HandRateX = 0.0;
		Hang.HandRateY = 0.0;
		Hang.PushX = 0.0;
		Hang.PushY = 0.0;
	}

	/** How the piece is set for a lean: the way its pole points (a unit) and how far it is lifted so that the low edge of its base stays on the table. */
	struct FSet
	{
		double UpX = 0.0, UpY = 0.0, UpZ = 1.0;
		double Rise = 0.0;
	};

	inline FSet Set(const FLean& Leans, double BaseRadius)
	{
		const double Angle = std::sqrt(Leans.X * Leans.X + Leans.Y * Leans.Y);
		FSet Out;
		if (Angle < 1.0e-9)
		{
			return Out;
		}
		const double Sine = std::sin(Angle);
		Out.UpX = Leans.X / Angle * Sine;
		Out.UpY = Leans.Y / Angle * Sine;
		Out.UpZ = std::cos(Angle);
		Out.Rise = (BaseRadius > 0.0 ? BaseRadius : 0.0) * Sine;
		return Out;
	}

	/** How much of an army lands: 1 for three corps, a little less for one, a third more for seven and over. */
	inline double Heft(int Corps) { return Clamp(0.73 + 0.09 * Corps, 0.82, 1.36); }

	/** One ring of the dust at one age: how far out from the middle its puffs are, how high, how big, and how thick (0 to 1). Lengths in the smoke's own unit. */
	struct FDust
	{
		double Out = 0.0;
		double High = 0.0;
		double Size = 0.0;
		double Thick = 0.0;
	};

	/** What it lands on: 0 earth, 1 snow, 2 sand. */
	inline constexpr double SkirtLasts = 0.6;
	inline double BillowLasts(int Ground) { return Ground == 1 ? 1.5 : Ground == 2 ? 2.2 : 1.9; }

	/** The low ring that runs out along the table from under the base: quick at first and soon gone. */
	inline FDust Skirt(double Age, double Much)
	{
		FDust Dust;
		if (Age < 0.0 || Age >= SkirtLasts)
		{
			return Dust;
		}
		const double A = Age / SkirtLasts;
		const double Run = 1.0 - (1.0 - A) * (1.0 - A) * (1.0 - A);
		Dust.Out = (1.9 + 4.6 * Run) * Much;
		Dust.High = 0.22 + 0.2 * Run;
		Dust.Size = (0.5 + 0.8 * Run) * Much;
		Dust.Thick = 0.6 * Smooth(A / 0.1) * std::pow(1.0 - A, 1.2);
		return Dust;
	}

	/** The cloud that rises behind it and thins away: lower and sooner gone on snow, longer in the air on sand. */
	inline FDust Billow(double Age, double Much, int Ground)
	{
		FDust Dust;
		const double Lasts = BillowLasts(Ground);
		if (Age < 0.0 || Age >= Lasts)
		{
			return Dust;
		}
		const double A = Age / Lasts;
		const double Tall = Ground == 1 ? 0.7 : 1.0;
		Dust.Out = (2.3 + 3.2 * A) * Much;
		Dust.High = (0.3 + 1.5 * A * Tall) * Much;
		Dust.Size = (0.8 + 1.7 * A) * Much;
		Dust.Thick = (Ground == 1 ? 0.4 : 0.46) * Smooth(A / 0.12) * std::pow(1.0 - A, 1.5);
		return Dust;
	}
}
