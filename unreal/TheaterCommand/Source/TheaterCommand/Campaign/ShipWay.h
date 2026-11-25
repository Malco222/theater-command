#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

/**
 * A fleet under way on the campaign map (release 2; Brian, 25 November 2026: "I want to keep upleveling the graphics
 * and smoothness of how they move and act to make it feel as real as possible", "can we do that for the campaign map
 * too?", and "I want this to visually wow people on how smooth and realistic and detailed grand it looks").
 *
 * Ships that sail (24 November) took a fleet from one sea to the next along the map's way by water: she spun on the
 * spot to her first heading, ran corner to corner in straight lines at a piece's ease, spun back to face the table, and
 * her wake swung rigidly behind her like a tail. This is what a ship does instead:
 *
 *   The course (Plan). She cannot turn where she lies: she swings out of her berth on her turning circle, runs
 *   straight, sweeps round each corner of the way on a wide circle, and comes round on a last circle to lie as the
 *   table wants her. Arcs and straights, each straight a tangent to the circles at its ends, so her heading never
 *   jumps. A corner of the way is always a point of land the way bends round, so the land is inside the turn: her
 *   circle there is laid behind the corner and passes a berth outside it, to seaward. Every point is asked of the
 *   water: a circle that would put her ashore is halved, three times, and then given up, and where a circle is
 *   given up she slows, turns where she lies and goes on, as she did before. With every circle given up the course
 *   is the way exactly as the map found it.
 *
 *   Her way (Time). A ship is heavy. She gathers way slowly and carries it in, and her head swings no faster than
 *   her helm allows: so she slows for a tight turn, creeps round her own circle as she leaves and comes in, and
 *   where she must turn where she lies she all but stops to do it. (At the map's speeds, six of her own lengths a
 *   second, a rounded corner alone is still a snap: it is the helm that makes it a turn.) No voyage keeps the
 *   table waiting: past half as long again as was asked, her helm is made quicker.
 *
 *   Her wake (LookAstern, Trail). Foam stays on the water where she made it. Astern of her it lies along the water
 *   her sterns have crossed, not along her keel: on a turn her sterns swing wide, and the wake lies where they swung.
 *
 * Plain C++, no engine, so that it can be built and checked alone (Tools/ship_way/run.sh). The map
 * (ACampaignMapActor::ShowFleets) hands in the way and asks the water; the piece (AArmyPiece::SailCourse) sails it.
 * Lengths are in whatever unit the way is given in, but for Seconds, which is told map units.
 */
namespace TheaterShipWay
{
	struct FSpot
	{
		double X = 0.0;
		double Y = 0.0;
	};

	inline FSpot operator+(FSpot A, FSpot B) { return FSpot{ A.X + B.X, A.Y + B.Y }; }
	inline FSpot operator-(FSpot A, FSpot B) { return FSpot{ A.X - B.X, A.Y - B.Y }; }
	inline FSpot operator*(FSpot A, double K) { return FSpot{ A.X * K, A.Y * K }; }
	inline double Dot(FSpot A, FSpot B) { return A.X * B.X + A.Y * B.Y; }
	inline double Cross(FSpot A, FSpot B) { return A.X * B.Y - A.Y * B.X; }
	inline double Size(FSpot A) { return std::sqrt(Dot(A, A)); }
	/** A quarter turn from A toward the side this file calls the left. (Which side that is on the table does not matter: every circle is built and gone round by the same rule.) */
	inline FSpot Left(FSpot A) { return FSpot{ -A.Y, A.X }; }
	inline FSpot Unit(FSpot A, FSpot Else) { const double Long = Size(A); return Long > 1.0e-9 ? A * (1.0 / Long) : Else; }
	inline double Clamp(double V, double Low, double High) { return V < Low ? Low : V > High ? High : V; }
	inline double Least(double A, double B) { return A < B ? A : B; }
	inline constexpr double WholeTurn = 6.283185307179586;
	/** Of two radii of turn at one point, the one that binds her: the tighter, where both are turns (nought is a straight). */
	inline double Tighter(double A, double B) { return A <= 0.0 ? B : B <= 0.0 ? A : Least(A, B); }

	/** What is asked: the way by water as the map found it, and how she lies at each end of it. */
	struct FAsk
	{
		std::vector<FSpot> Way; // corner to corner, the first point where she lies and the last where she is going
		FSpot Bows{ 0.0, 1.0 }; // the way her bows point as she leaves
		FSpot Rest{ 0.0, 1.0 }; // and the way they must point when she has come to rest
		double Turn = 0.0; // the radius of her turning circle, leaving and coming in (nought: she turns where she lies)
		double Sweep = 0.0; // the radius she takes a corner of the way on, out at sea (nought: corners as they are)
		double Berth = 0.0; // how far outside a corner that circle passes
		double Step = 1.0; // how closely the water is asked along the course
		double Stride = 4.0; // the most between one point of the course and the next
	};

	/** The course she sails. */
	struct FCourse
	{
		std::vector<FSpot> Points; // close-set: no more than a stride apart, nor four degrees of a turn
		std::vector<FSpot> Heads; // her heading at each point
		std::vector<double> Radii; // the radius of the turn each point is on; nought on a straight
		// How far she has come at each point. A turn made where she lies moves her nowhere, yet counts for a little
		// here (as a circle too small to see would), so that it is a part of the course like any other: her head
		// swings through it in its turn, and takes its time.
		std::vector<double> Marks;
		double Length = 0.0; // the last of the marks
		int Arcs = 0; // the circles kept
		int Corners = 0; // the turns of more than a degree she makes where she lies (a circle given up, or none asked)
		double Tightest = 0.0; // the least radius among the circles kept (nought with none)
	};

	/** One circle of the course: her turning circle at an end of it, or the sweep round a corner of the way. */
	struct FRing
	{
		FSpot Mid; // the corner; at an end, where she lies
		FSpot Head; // (an end) her bows there
		FSpot Inward; // (a corner) toward the inside of the way's turn there, where the land is
		double Full = 0.0; // the radius asked
		double Clear = 0.0; // (a corner) the berth asked
		int Notch = 0; // how far it has been made less: 0 whole, 1 a half, 2 a quarter, 3 an eighth, 4 given up
		int Side = 1; // 1 gone round with its middle on her left, -1 on her right
		bool bEnd = false;
		double Radius() const { return Notch >= 4 ? 0.0 : Full / static_cast<double>(1 << Notch); }
		/** (a corner) How far outside the corner the circle passes: the berth asked while there is room, less as the circle is made less, and at the last through the corner itself. */
		double Off() const { return Notch >= 3 ? 0.0 : Least(Clear, Radius() * (Notch == 2 ? 0.5 : 1.0)); }
		FSpot Centre() const { return bEnd ? Mid + Left(Head) * (Radius() * Side) : Mid + Inward * (Radius() - Off()); }
		/** Where she is on the circle when her heading is Heading. */
		FSpot On(FSpot Heading) const { return Centre() - Left(Heading) * (Radius() * Side); }
		void Less() { Notch = Notch < 4 ? Notch + 1 : 4; }
	};

	/** Her heading along the straight that leaves circle A and comes onto circle B, each gone round its own way. False where there is none. */
	inline bool Tangent(const FRing& A, const FRing& B, FSpot& OutHead)
	{
		const FSpot Between = B.Centre() - A.Centre();
		const double Apart = Size(Between);
		// (with N a quarter turn left of her heading, she is at Centre - N * Side * Radius on each circle, and the
		// straight between those two points lies along her heading only where N . Between is this)
		const double Lean = B.Side * B.Radius() - A.Side * A.Radius();
		if (Apart < 1.0e-9 || std::fabs(Lean) > Apart * (1.0 - 1.0e-9))
		{
			return false;
		}
		const FSpot Along = Between * (1.0 / Apart);
		const double C = Lean / Apart, S = std::sqrt(1.0 - C * C);
		OutHead = Along * S - Left(Along) * C;
		return true;
	}

	/** How far round a circle she goes from one heading to another, its own way; with the circle given up, the size of the turn she makes where she lies. */
	inline double Swept(const FRing& Ring, FSpot In, FSpot Out)
	{
		const double Turn = std::atan2(Cross(In, Out), Dot(In, Out));
		if (Ring.Radius() <= 0.0)
		{
			return std::fabs(Turn);
		}
		double Round = Ring.Side * Turn;
		if (Round < 0.0)
		{
			Round += WholeTurn;
		}
		return Round > WholeTurn - 1.0e-7 ? 0.0 : Round;
	}

	/** One laying of the course, with the two end circles on the sides given: the circles are made less until it is all on water. What it cost comes back. */
	template <typename FWet>
	inline double Lay(const std::vector<FSpot>& Way, FSpot Bows, FSpot Rest, const FAsk& Ask, int LeaveSide, int ArriveSide, FWet& Wet, FCourse& Out)
	{
		const size_t Count = Way.size();
		const double Step = Ask.Step > 1.0e-6 ? Ask.Step : 1.0, Stride = Ask.Stride > Step ? Ask.Stride : Step;
		std::vector<FRing> Rings(Count);
		double Whole = 0.0;
		for (size_t K = 1; K < Count; ++K)
		{
			Whole += Size(Way[K] - Way[K - 1]);
		}
		for (size_t K = 0; K < Count; ++K)
		{
			FRing& Ring = Rings[K];
			Ring.Mid = Way[K];
			if (K == 0 || K + 1 == Count)
			{
				Ring.bEnd = true;
				Ring.Head = K == 0 ? Bows : Rest;
				Ring.Side = K == 0 ? LeaveSide : ArriveSide;
				Ring.Full = Clamp(Ask.Turn, 0.0, 0.3 * Whole); // (a short hop is not two great loops)
			}
			else
			{
				const FSpot In = Unit(Way[K] - Way[K - 1], Bows), On = Unit(Way[K + 1] - Way[K], Bows);
				const double Bend = std::atan2(Cross(In, On), Dot(In, On));
				Ring.Side = Bend < 0.0 ? -1 : 1;
				Ring.Inward = Unit(On - In, Left(In) * Ring.Side);
				Ring.Clear = Ask.Berth > 0.0 ? Ask.Berth : 0.0;
				// (no wider than lets her be on it and off it again within the legs that meet there: a circle that
				// touches both legs leaves each this far from the corner)
				const double Reach = std::tan(std::fabs(Bend) * 0.5);
				const double Short = Least(Size(Way[K] - Way[K - 1]), Size(Way[K + 1] - Way[K]));
				Ring.Full = std::fabs(Bend) < 0.02 ? 0.0 : Clamp(Ask.Sweep, 0.0, 0.45 * Short / (Reach > 1.0e-6 ? Reach : 1.0e-6));
			}
			Ring.Notch = Ring.Full > 0.0 ? 0 : 4;
		}
		std::vector<FSpot> Legs(Count - 1); // her heading on the straight after each circle
		const FSpot First = Way.front(), Last = Way.back();
		auto Dry = [&](FSpot P)
		{
			// (where she lies and where she is going are hers already: the map lays a fleet close in under a coast)
			return Size(P - First) > Step && Size(P - Last) > Step && !Wet(P);
		};
		for (size_t Pass = 0; Pass < 5 * Count + 4; ++Pass)
		{
			bool bLess = false;
			for (size_t K = 0; K + 1 < Count && !bLess; ++K)
			{
				if (!Tangent(Rings[K], Rings[K + 1], Legs[K]))
				{
					FRing& Wide = Rings[K].Radius() >= Rings[K + 1].Radius() ? Rings[K] : Rings[K + 1];
					if (Wide.Radius() <= 0.0)
					{
						Legs[K] = Unit(Way[K + 1] - Way[K], Bows); // (cannot be: two points of the way are never one)
						continue;
					}
					Wide.Less();
					bLess = true;
				}
			}
			for (size_t K = 0; K < Count && !bLess; ++K)
			{
				FRing& Ring = Rings[K];
				if (Ring.Radius() <= 0.0)
				{
					continue;
				}
				const FSpot In = K == 0 ? Bows : Legs[K - 1], On = K + 1 == Count ? Rest : Legs[K];
				const double Round = Swept(Ring, In, On);
				// (more than a little over half a turn round a corner, or nearly all the way round at an end: the circle
				// is the wrong side of the straights it should join, and is too big for where it is)
				if (Round > (Ring.bEnd ? 5.4 : 3.5))
				{
					Ring.Less();
					bLess = true;
					break;
				}
				const double Start = std::atan2(In.Y, In.X);
				const int Asks = static_cast<int>(std::ceil(Ring.Radius() * Round / Step));
				for (int A = 1; A < Asks; ++A)
				{
					const double Angle = Start + Ring.Side * Round * A / Asks;
					if (Dry(Ring.On(FSpot{ std::cos(Angle), std::sin(Angle) })))
					{
						Ring.Less();
						bLess = true;
						break;
					}
				}
			}
			for (size_t K = 0; K + 1 < Count && !bLess; ++K)
			{
				FRing& A = Rings[K];
				FRing& B = Rings[K + 1];
				if (A.Radius() <= 0.0 && B.Radius() <= 0.0)
				{
					continue; // (the way's own straight, which the map has asked already)
				}
				const FSpot From = A.On(Legs[K]), To = B.On(Legs[K]);
				const int Asks = static_cast<int>(std::ceil(Size(To - From) / Step));
				for (int At = 0; At <= Asks; ++At)
				{
					if (Dry(From + (To - From) * (Asks > 0 ? static_cast<double>(At) / Asks : 0.0)))
					{
						(A.Radius() >= B.Radius() ? A : B).Less();
						bLess = true;
						break;
					}
				}
			}
			if (!bLess)
			{
				break;
			}
		}

		// Laid: its points, and what it cost. (A circle made less costs twice the turn it no longer carries, so that of
		// two sides she takes the one with room for the whole circle, and of two with room, the shorter.)
		Out = FCourse();
		double Cost = 0.0;
		auto Add = [&Out](FSpot P, FSpot H, double Radius, double Swung)
		{
			if (!Out.Points.empty())
			{
				if (Size(P - Out.Points.back()) < 1.0e-7 && Size(H - Out.Heads.back()) < 1.0e-7)
				{
					Out.Radii.back() = Tighter(Radius, Out.Radii.back());
					return;
				}
				Out.Length += Size(P - Out.Points.back()) + Swung;
			}
			Out.Points.push_back(P);
			Out.Heads.push_back(H);
			Out.Radii.push_back(Radius);
			Out.Marks.push_back(Out.Length);
		};
		auto Straight = [&](FSpot To, FSpot H)
		{
			const FSpot From = Out.Points.back();
			const int Cuts = static_cast<int>(std::ceil(Size(To - From) / Stride));
			for (int At = 1; At <= Cuts; ++At)
			{
				Add(At == Cuts ? To : From + (To - From) * (static_cast<double>(At) / Cuts), H, 0.0, 0.0);
			}
		};
		auto Arc = [&](FSpot Centre, double Radius, int Side, FSpot In, FSpot On, double Round)
		{
			Out.Radii.back() = Tighter(Radius, Out.Radii.back()); // (where she comes onto the circle is on it)
			const double Start = std::atan2(In.Y, In.X);
			int Cuts = static_cast<int>(std::ceil(Round / 0.07)); // (a point every four degrees or so)
			const int ByStride = static_cast<int>(std::ceil(Radius * Round / Stride));
			Cuts = (Cuts > ByStride ? Cuts : ByStride) + 1;
			for (int A = 1; A <= Cuts; ++A)
			{
				const double Angle = Start + Side * Round * A / Cuts;
				const FSpot H = A == Cuts ? On : FSpot{ std::cos(Angle), std::sin(Angle) };
				Add(Centre - Left(H) * (Radius * Side), H, Radius, 0.0);
			}
		};
		// A turn made where she lies, the short way round: she stays where she is, and her head swings. It counts in
		// the marks as a circle of radius Tiny would, and is given that radius, so that she all but stops for it and
		// her helm sets how long it takes.
		const double Tiny = Step * 0.04;
		auto Pivot = [&](FSpot In, FSpot On)
		{
			const FSpot Lies = Out.Points.back();
			const double Turn = std::atan2(Cross(In, On), Dot(In, On)), Start = std::atan2(In.Y, In.X);
			const int Cuts = static_cast<int>(std::ceil(std::fabs(Turn) / 0.07)) + 1;
			Out.Radii.back() = Tighter(Tiny, Out.Radii.back());
			for (int A = 1; A <= Cuts; ++A)
			{
				const double Angle = Start + Turn * A / Cuts;
				Add(Lies, A == Cuts ? On : FSpot{ std::cos(Angle), std::sin(Angle) }, Tiny, Tiny * std::fabs(Turn) / Cuts);
			}
		};
		Add(First, Bows, 0.0, 0.0);
		for (size_t K = 0; K < Count; ++K)
		{
			const FRing& Ring = Rings[K];
			const FSpot In = K == 0 ? Bows : Legs[K - 1], On = K + 1 == Count ? Rest : Legs[K];
			const double Round = Swept(Ring, In, On);
			Cost += 2.0 * (Ring.Full - Ring.Radius()) * Round;
			if (Ring.Radius() > 0.0)
			{
				if (Round > 1.0e-6)
				{
					++Out.Arcs;
					Out.Tightest = Out.Tightest <= 0.0 || Ring.Radius() < Out.Tightest ? Ring.Radius() : Out.Tightest;
					Arc(Ring.Centre(), Ring.Radius(), Ring.Side, In, On, Round);
				}
			}
			else if (Round > 0.02)
			{
				++Out.Corners;
				Pivot(In, On);
			}
			if (K + 1 < Count)
			{
				Straight(Rings[K + 1].On(Legs[K]), Legs[K]);
			}
		}
		// (she is there already, to the last place of the arithmetic)
		Out.Points.back() = Last;
		Out.Heads.back() = Rest;
		return Out.Length + Cost;
	}

	/**
	 * The course for a way. Wet is asked of a point (an FSpot) and answers whether a ship may be there. The course
	 * begins and ends where the way does, begins on Bows and ends on Rest, and is on water wherever the way was.
	 */
	template <typename FWet>
	inline FCourse Plan(const FAsk& Ask, FWet&& Wet)
	{
		std::vector<FSpot> Way;
		for (const FSpot& P : Ask.Way)
		{
			if (Way.empty() || Size(P - Way.back()) > 1.0e-6)
			{
				Way.push_back(P);
			}
		}
		FCourse Best;
		if (Way.size() < 2)
		{
			Best.Points = Way;
			Best.Heads.assign(Way.size(), Unit(Ask.Rest, FSpot{ 0.0, 1.0 }));
			Best.Radii.assign(Way.size(), 0.0);
			Best.Marks.assign(Way.size(), 0.0);
			return Best;
		}
		const FSpot Bows = Unit(Ask.Bows, Unit(Way[1] - Way[0], FSpot{ 0.0, 1.0 }));
		const FSpot Rest = Unit(Ask.Rest, Unit(Way.back() - Way[Way.size() - 2], FSpot{ 0.0, 1.0 }));
		double Cheapest = -1.0;
		for (int Leave = 1; Leave >= -1; Leave -= 2)
		{
			for (int Arrive = 1; Arrive >= -1; Arrive -= 2)
			{
				FCourse Tried;
				const double Cost = Lay(Way, Bows, Rest, Ask, Leave, Arrive, Wet, Tried);
				if (Cheapest < 0.0 || Cost < Cheapest - 1.0e-9)
				{
					Cheapest = Cost;
					Best = Tried;
				}
				if (Ask.Turn <= 0.0)
				{
					return Best; // (no circles at the ends: the sides are all one)
				}
			}
		}
		return Best;
	}

	// ---- Her way: how she goes through her time.

	inline constexpr double Gather = 0.3; // the share of the time asked she would take to work up to full speed
	inline constexpr double Lose = 0.42; // and the share over which she would lose it all

	/** The time asked for a course of this length in map units: slower than a piece carried by hand, and slower than she sailed before. (Her turns may make it longer: see Time.) */
	inline double Seconds(double MapLength)
	{
		return Clamp(2.2 + MapLength / 240.0, 2.8, 6.5);
	}

	/** Her times over a course: where she is, and how she heads, at each tick of her time. */
	struct FTimes
	{
		double Tick = 0.02; // the time from one entry to the next
		std::vector<double> Ran; // how far along the course she is at each tick, from nought to its length
		std::vector<FSpot> At; // where she is at each tick
		std::vector<FSpot> Head; // and how she heads there (eased: see Time)
		double Seconds = 0.0; // her whole time
		double Full = 0.0; // the full speed she works up to, where she has the sea room
		double Fastest = 0.0; // the fastest she in fact goes
		double Swing = 0.0; // the fastest her head in fact swings, in radians a second
		double Helm = 0.0; // the helm she was in fact given (more than was asked only where her turns would have kept her too long)
	};

	inline constexpr double Longest = 1.5; // her time is never more than this many times the time asked

	/** (For Time.) How long the course takes at a full speed, and her speed at each point of it: never over full speed, nor faster round a turn than her helm allows, nor gathered or lost faster than a ship can. */
	inline double Took(const std::vector<double>& Marks, const std::vector<double>& Radii, double Full, double Asked, double Helm, std::vector<double>& Speeds)
	{
		const size_t Count = Marks.size();
		const double Gain = Full / (Gather * Asked), Loss = Full / (Lose * Asked);
		Speeds.assign(Count, 0.0);
		for (size_t K = 1; K + 1 < Count; ++K)
		{
			Speeds[K] = Radii[K] > 0.0 ? Least(Full, Helm * Radii[K]) : Full;
		}
		for (size_t K = 1; K < Count; ++K)
		{
			Speeds[K] = Least(Speeds[K], std::sqrt(Speeds[K - 1] * Speeds[K - 1] + 2.0 * Gain * (Marks[K] - Marks[K - 1])));
		}
		for (size_t K = Count - 1; K-- > 0;)
		{
			Speeds[K] = Least(Speeds[K], std::sqrt(Speeds[K + 1] * Speeds[K + 1] + 2.0 * Loss * (Marks[K + 1] - Marks[K])));
		}
		double Time = 0.0;
		for (size_t K = 1; K < Count; ++K)
		{
			const double Both = Speeds[K - 1] + Speeds[K];
			Time += Both > 1.0e-12 ? 2.0 * (Marks[K] - Marks[K - 1]) / Both : 0.0;
		}
		return Time;
	}

	/** How far her wake has come back to lie straight astern of her, a share P of the way through her time: nought until the last seventh of it. */
	inline double Straighten(double P)
	{
		const double X = Clamp((P - 0.86) / 0.14, 0.0, 1.0);
		return X * X * (3.0 - 2.0 * X);
	}

	// ---- Her wake: the water astern of her, along the course she has run.

	/**
	 * Where she is a distance At along a course given as points, her heading at each, and the distance run at each.
	 * Before its start she is straight astern of where she lay; past its end, straight on. (TPoint and THead are
	 * anything with an X and a Y: the piece hands in its own points.)
	 */
	template <typename TPoint, typename THead>
	inline void Along(const TPoint* Points, const THead* Heads, const double* Marks, int Count, double At, FSpot& OutAt, FSpot& OutHead)
	{
		if (Count < 1)
		{
			OutAt = FSpot{};
			OutHead = FSpot{ 0.0, 1.0 };
			return;
		}
		auto P = [&](int K) { return FSpot{ static_cast<double>(Points[K].X), static_cast<double>(Points[K].Y) }; };
		auto H = [&](int K) { return FSpot{ static_cast<double>(Heads[K].X), static_cast<double>(Heads[K].Y) }; };
		if (At <= Marks[0] || Count == 1)
		{
			OutHead = H(0);
			OutAt = P(0) + OutHead * (At - Marks[0]);
			return;
		}
		if (At >= Marks[Count - 1])
		{
			OutHead = H(Count - 1);
			OutAt = P(Count - 1) + OutHead * (At - Marks[Count - 1]);
			return;
		}
		int Low = 1, High = Count - 1; // (the first point at or past At)
		while (Low < High)
		{
			const int Mid = (Low + High) / 2;
			if (Marks[Mid] < At)
			{
				Low = Mid + 1;
			}
			else
			{
				High = Mid;
			}
		}
		const double Span = Marks[Low] - Marks[Low - 1];
		const double F = Span > 1.0e-9 ? (At - Marks[Low - 1]) / Span : 1.0;
		OutAt = P(Low - 1) + (P(Low) - P(Low - 1)) * F;
		OutHead = Unit(H(Low - 1) + (H(Low) - H(Low - 1)) * F, H(Low));
	}

	/** The water astern of her, station by station from where she is now: where each lies, and her heading when she was there. */
	struct FAstern
	{
		double Each = 1.0; // the distance between stations
		std::vector<FSpot> At, Head;
	};

	template <typename TPoint, typename THead>
	inline void LookAstern(const TPoint* Points, const THead* Heads, const double* Marks, int Count, double Ran, double Reach, int Stations, FAstern& Out)
	{
		const size_t Many = static_cast<size_t>(Stations < 2 ? 2 : Stations);
		Out.Each = (Reach > 1.0e-6 ? Reach : 1.0e-6) / static_cast<double>(Many - 1);
		Out.At.resize(Many);
		Out.Head.resize(Many);
		for (size_t K = 0; K < Many; ++K)
		{
			Along(Points, Heads, Marks, Count, Ran - static_cast<double>(K) * Out.Each, Out.At[K], Out.Head[K]);
		}
	}

	/**
	 * A point of her wake, in her own frame (X abeam, Y toward her bows, as her model is built, Scale of the course's
	 * units to one of hers): where it lies once the wake is laid along the water astern. A point no more than Stiff
	 * astern of her middle is hers and stays (her ships, and the water at their sterns: a piece is a formation, and
	 * turns as one). One further astern is water left by the point of her that far abeam and Stiff astern, as she lay
	 * when she was as far back along her course as it is astern of that: so on a turn, where her sterns swing wide of
	 * her middle, the wake lies where they swung. Head is the way her bows point now. Settle brings it back toward
	 * straight astern (1: where it was).
	 */
	inline void Trail(const FAstern& Astern, FSpot Head, double Scale, double Settle, double Stiff, double LocalX, double LocalY, double& OutX, double& OutY)
	{
		OutX = LocalX;
		OutY = LocalY;
		Stiff = Stiff > 0.0 ? Stiff : 0.0;
		if (LocalY >= -Stiff || Scale <= 0.0 || Astern.At.size() < 2 || Settle >= 1.0)
		{
			return;
		}
		const size_t Last = Astern.At.size() - 1;
		const double Back = Clamp((-LocalY - Stiff) * Scale / Astern.Each, 0.0, static_cast<double>(Last));
		const size_t K = Back >= static_cast<double>(Last) ? Last - 1 : static_cast<size_t>(Back);
		const double F = Back - static_cast<double>(K);
		const FSpot There = Astern.At[K] + (Astern.At[K + 1] - Astern.At[K]) * F;
		const FSpot Then = Unit(Astern.Head[K] + (Astern.Head[K + 1] - Astern.Head[K]) * F, Head);
		// (her frame's +X, abeam, lies a quarter turn from her bows the other way from Left)
		const FSpot Lies = There - Left(Then) * (LocalX * Scale) - Then * (Stiff * Scale) - Astern.At[0];
		const double BentX = -Dot(Lies, Left(Head)) / Scale, BentY = Dot(Lies, Head) / Scale;
		const double Keep = Clamp(Settle, 0.0, 1.0);
		OutX = BentX + (LocalX - BentX) * Keep;
		OutY = BentY + (LocalY - BentY) * Keep;
	}

	// ---- Her way, laid out in time.

	/**
	 * Her times over a course: Asked is the time wanted (Seconds), Helm the fastest her head may swing, in radians a
	 * second. She makes the time asked where her turns let her, by her full speed (never more than two and a half
	 * times her mean); where they do not, she takes what the turns take, up to Longest times the time asked, and past
	 * that her helm is made quicker (to eight times what was asked, and no more) so that no voyage keeps the table
	 * waiting.
	 *
	 * Her head is eased: at each tick it is the mean of the course's heading over a fifth of a second about it, so
	 * that a turn comes on and goes off, and does not begin or end at a point. For that her whole time is a fifth of
	 * a second longer, and she lies still for the first and last tenth of it. (It is her head that is eased, not how
	 * far she has run: to smooth the run would hurry her through the turn she slowed for.)
	 */
	inline FTimes Time(const FCourse& Course, double Asked, double Helm)
	{
		FTimes Out;
		const size_t Count = Course.Points.size();
		std::vector<double> Marks = Course.Marks;
		if (Marks.size() != Count)
		{
			// (a course made by hand, with no marks of its own: the way come from point to point)
			Marks.assign(Count, 0.0);
			for (size_t K = 1; K < Count; ++K)
			{
				Marks[K] = Marks[K - 1] + Size(Course.Points[K] - Course.Points[K - 1]);
			}
		}
		const double Long = Count > 0 ? Marks.back() : 0.0;
		Asked = Asked > 0.1 ? Asked : 0.1;
		Helm = Helm > 0.05 ? Helm : 0.05;
		Out.Helm = Helm;
		if (Count < 2 || Long < 1.0e-9)
		{
			// (nowhere to go: she lies where she lies, as the table wants her)
			const FSpot Lies = Count > 0 ? Course.Points.back() : FSpot{};
			const FSpot Heads = Count > 0 ? Course.Heads.back() : FSpot{ 0.0, 1.0 };
			Out.Ran = { 0.0, Long };
			Out.At = { Lies, Lies };
			Out.Head = { Heads, Heads };
			Out.Seconds = Out.Tick;
			return Out;
		}
		std::vector<double> Speeds;
		double Low = Long / Asked, High = 2.5 * Long / Asked;
		double Whole = Took(Marks, Course.Radii, High, Asked, Helm, Speeds);
		if (Whole < Asked)
		{
			for (int Halving = 0; Halving < 40; ++Halving)
			{
				const double Mid = 0.5 * (Low + High);
				(Took(Marks, Course.Radii, Mid, Asked, Helm, Speeds) > Asked ? Low : High) = Mid;
			}
			Whole = Took(Marks, Course.Radii, High, Asked, Helm, Speeds);
		}
		else if (Whole > Longest * Asked)
		{
			double Slow = Helm, Quick = 8.0 * Helm;
			for (int Halving = 0; Halving < 30; ++Halving)
			{
				const double Mid = 0.5 * (Slow + Quick);
				(Took(Marks, Course.Radii, High, Asked, Mid, Speeds) > Longest * Asked ? Slow : Quick) = Mid;
			}
			Out.Helm = Quick;
			Whole = Took(Marks, Course.Radii, High, Asked, Quick, Speeds);
		}
		Out.Full = High;
		// How far she has run at each tick, her speed changing evenly between one point of the course and the next;
		// and with it where she is, and the course's own heading there.
		const size_t Ticks = static_cast<size_t>(std::ceil(Whole / Out.Tick)) + 1;
		std::vector<double> Run(Ticks + 1, 0.0);
		std::vector<FSpot> Lies(Ticks + 1), Heads(Ticks + 1);
		{
			size_t K = 1;
			double Before = 0.0; // the time at point K - 1
			for (size_t J = 0; J <= Ticks; ++J)
			{
				const double T = static_cast<double>(J) * Out.Tick;
				double Span = 0.0;
				while (K + 1 < Count)
				{
					const double Both = Speeds[K - 1] + Speeds[K];
					Span = Both > 1.0e-12 ? 2.0 * (Marks[K] - Marks[K - 1]) / Both : 0.0;
					if (T <= Before + Span)
					{
						break;
					}
					Before += Span;
					++K;
				}
				const double Both = Speeds[K - 1] + Speeds[K];
				Span = Both > 1.0e-12 ? 2.0 * (Marks[K] - Marks[K - 1]) / Both : 0.0;
				const double In = Clamp(T - Before, 0.0, Span);
				const double Quicken = Span > 1.0e-12 ? (Speeds[K] - Speeds[K - 1]) / Span : 0.0;
				Run[J] = T >= Whole || J == Ticks ? Long : Clamp(Marks[K - 1] + Speeds[K - 1] * In + 0.5 * Quicken * In * In, Marks[K - 1], Marks[K]);
				if (J > 0 && Run[J] < Run[J - 1])
				{
					Run[J] = Run[J - 1];
				}
				Along(Course.Points.data(), Course.Heads.data(), Marks.data(), static_cast<int>(Count), Run[J], Lies[J], Heads[J]);
			}
		}
		Heads.front() = Course.Heads.front();
		Heads.back() = Course.Heads.back();
		Lies.back() = Course.Points.back();
		// Eased. (Entry I is tick I - Ease of her run: before it she lies as she lay, after it as she is to lie.)
		const long Ease = 5, Last = static_cast<long>(Ticks);
		const size_t Many = Ticks + 1 + 2 * static_cast<size_t>(Ease);
		Out.Ran.resize(Many);
		Out.At.resize(Many);
		Out.Head.resize(Many);
		auto Tick = [Last](long J) { return static_cast<size_t>(J < 0 ? 0 : J > Last ? Last : J); };
		for (size_t I = 0; I < Many; ++I)
		{
			const long J = static_cast<long>(I) - Ease;
			Out.Ran[I] = Run[Tick(J)];
			Out.At[I] = Lies[Tick(J)];
			FSpot Sum;
			for (long D = -Ease; D <= Ease; ++D)
			{
				Sum = Sum + Heads[Tick(J + D)];
			}
			Out.Head[I] = Unit(Sum, Heads[Tick(J)]);
			if (I > 0)
			{
				const double Went = (Out.Ran[I] - Out.Ran[I - 1]) / Out.Tick;
				const double Swung = std::fabs(std::atan2(Cross(Out.Head[I - 1], Out.Head[I]), Dot(Out.Head[I - 1], Out.Head[I]))) / Out.Tick;
				Out.Fastest = Went > Out.Fastest ? Went : Out.Fastest;
				Out.Swing = Swung > Out.Swing ? Swung : Out.Swing;
			}
		}
		Out.Seconds = static_cast<double>(Many - 1) * Out.Tick;
		return Out;
	}
}
