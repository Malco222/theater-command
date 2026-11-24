#include "Campaign/EnemyMoves.h"

#include <algorithm>
#include <cmath>

namespace TheaterEnemyMoves
{
	namespace
	{
		// (File-local helpers, each named for this file: the module is built one file at a time, and a helper of the
		// same name in another file must never be the one a reader finds.)
		constexpr double MovesPi = 3.14159265358979323846;

		double MovesDistance(const FPoint& One, const FPoint& Other)
		{
			return std::hypot(Other.X - One.X, Other.Y - One.Y);
		}

		FPoint MovesLerp(const FPoint& One, const FPoint& Other, double Share)
		{
			return FPoint{ One.X + (Other.X - One.X) * Share, One.Y + (Other.Y - One.Y) * Share };
		}

		/** The point a share of the way along a curve that bows out from the straight line by Bow at its middle (a quadratic). */
		FPoint MovesCurve(const FPoint& From, const FPoint& Bend, const FPoint& To, double Share)
		{
			const double Rest = 1.0 - Share;
			return FPoint{ Rest * Rest * From.X + 2.0 * Rest * Share * Bend.X + Share * Share * To.X, Rest * Rest * From.Y + 2.0 * Rest * Share * Bend.Y + Share * Share * To.Y };
		}

		FInk MovesInk(unsigned char R, unsigned char G, unsigned char B, unsigned char A = 255)
		{
			FInk Ink;
			Ink.R = R;
			Ink.G = G;
			Ink.B = B;
			Ink.A = A;
			return Ink;
		}

		// The inks. (Drafts for the art director: a deep red for an army at war with him, amber for one at peace, a
		// brighter red for an attack, brass for a place that held, cream for his own corps falling back, and the dark
		// edge every one of them sits on so that it reads on any ground.)
		const FInk WarInk = MovesInk(176, 30, 26);
		const FInk PeaceInk = MovesInk(226, 150, 38);
		const FInk AttackInk = MovesInk(214, 38, 24);
		const FInk HeldInk = MovesInk(224, 184, 114);
		const FInk OwnInk = MovesInk(238, 230, 208);
		const FInk EdgeInk = MovesInk(22, 16, 12, 215);

		FInk MovesFaded(FInk Ink, double Share)
		{
			Ink.A = static_cast<unsigned char>(std::clamp(Ink.A * Share, 0.0, 255.0));
			return Ink;
		}

		// How high each layer lies over the ground (map units): the edges under the inks, the marks over the arrows.
		constexpr double EdgeLift = 0.44, InkLift = 0.5, MarkEdgeLift = 0.54, MarkLift = 0.6;
		// The arrow's shape, in map units (neighbouring provinces' middles lie about 35 to 110 apart, 65 in the middle).
		constexpr double StartGap = 3.0; // the arrow begins a little off the place it came from
		constexpr double EndGap = 4.5; // and its point stops short of the army it came to, so the piece stays clear
		constexpr double HeadLong = 6.0, HeadWide = 3.4; // the arrowhead, from its base to its point, and half across
		constexpr double EdgeOver = 0.55; // how far the dark edge shows round the ink
		constexpr double MarkRadius = 6.2; // the battle mark's ring
		constexpr int CurveSteps = 24;

		/** Half the width of an arrow's shaft: more corps, a little wider; an attack wider still; his own retreat narrower. */
		double MovesHalf(const FArrow& Arrow)
		{
			const double Size = std::min(std::max(Arrow.Corps, 1), 6) - 1;
			switch (Arrow.Kind)
			{
			case EKind::Attack:
				return 1.35 + 0.1 * Size;
			case EKind::Fell:
				return 0.8;
			default:
				return (Arrow.bAway ? 0.75 : 1.05) + 0.09 * Size;
			}
		}

		FInk MovesInkOf(const FArrow& Arrow)
		{
			switch (Arrow.Kind)
			{
			case EKind::Attack:
				return AttackInk;
			case EKind::Fell:
				return OwnInk;
			default:
				return MovesFaded(Arrow.bWar ? WarInk : PeaceInk, Arrow.bAway ? 0.6 : 1.0);
			}
		}

		void MovesTriangle(FLayer& Out, const FPoint& A, const FPoint& B, const FPoint& C, double Lift, const FInk& Ink)
		{
			FTriangle Face;
			Face.A = A;
			Face.B = B;
			Face.C = C;
			Face.Lift = Lift;
			Face.Ink = Ink;
			Out.Triangles.push_back(Face);
		}

		void MovesStroke(FLayer& Out, std::vector<FPoint> Points, double Half, double Lift, const FInk& Ink, bool bClosed = false)
		{
			if (Points.size() < 2)
			{
				return;
			}
			FStroke Stroke;
			Stroke.Points = std::move(Points);
			Stroke.Half = Half;
			Stroke.Lift = Lift;
			Stroke.Ink = Ink;
			Stroke.bClosed = bClosed;
			Out.Strokes.push_back(std::move(Stroke));
		}

		/** A ring of so many points round Middle. */
		std::vector<FPoint> MovesRing(const FPoint& Middle, double Radius, int Count)
		{
			std::vector<FPoint> Ring;
			for (int K = 0; K < Count; ++K)
			{
				const double Turn = 2.0 * MovesPi * K / Count;
				Ring.push_back(FPoint{ Middle.X + std::cos(Turn) * Radius, Middle.Y + std::sin(Turn) * Radius });
			}
			return Ring;
		}

		/**
		 * The battle mark where his troops were attacked: a ring with two crossed strokes in it, as a situation map marks
		 * a fight. Red where the place was lost (and the ring filled faintly), brass where it held, red and brass where
		 * the field was lost and the town holds. Show grows it in from a little larger, so that it is seen to land.
		 */
		void MovesMark(const FArrow& Arrow, double Show, FDrawing& Out)
		{
			if (Show <= 0.0)
			{
				return;
			}
			const double Radius = MarkRadius * (1.0 + 0.35 * (1.0 - Show));
			const FInk Ring = Arrow.bTaken ? AttackInk : HeldInk;
			const FInk Cross = Arrow.bTaken || Arrow.bSiege ? AttackInk : HeldInk;
			MovesStroke(Out.Under, MovesRing(Arrow.To, Radius, 28), 0.75 + EdgeOver, MarkEdgeLift, MovesFaded(EdgeInk, Show), true);
			MovesStroke(Out.Over, MovesRing(Arrow.To, Radius, 28), 0.75, MarkLift, MovesFaded(Ring, Show), true);
			const double Arm = Radius * 0.62;
			for (const double Turn : { MovesPi * 0.25, MovesPi * 0.75 })
			{
				const FPoint Off{ std::cos(Turn) * Arm, std::sin(Turn) * Arm };
				std::vector<FPoint> Line{ FPoint{ Arrow.To.X - Off.X, Arrow.To.Y - Off.Y }, FPoint{ Arrow.To.X + Off.X, Arrow.To.Y + Off.Y } };
				MovesStroke(Out.Under, Line, 0.6 + EdgeOver, MarkEdgeLift, MovesFaded(EdgeInk, Show));
				MovesStroke(Out.Over, Line, 0.6, MarkLift, MovesFaded(Cross, Show));
			}
			if (Arrow.bTaken)
			{
				// (a place lost: the ring is filled faintly, a fan of triangles from its middle)
				const std::vector<FPoint> Rim = MovesRing(Arrow.To, Radius - 0.4, 28);
				for (size_t K = 0; K < Rim.size(); ++K)
				{
					MovesTriangle(Out.Under, Arrow.To, Rim[K], Rim[(K + 1) % Rim.size()], MarkEdgeLift - 0.02, MovesFaded(AttackInk, 0.28 * Show));
				}
			}
		}
	}

	double Grown(double Seconds)
	{
		const double Share = std::clamp(Seconds / GrowSeconds, 0.0, 1.0);
		return 1.0 - (1.0 - Share) * (1.0 - Share) * (1.0 - Share); // (fast at first, settling at its end)
	}

	void Lay(const std::vector<FArrow>& Arrows, double Grow, FDrawing& Out)
	{
		Out.Clear();
		const double Share = std::clamp(Grow, 0.0, 1.0);
		if (Share <= 0.0)
		{
			return;
		}
		int Bend = 0;
		for (const FArrow& Arrow : Arrows)
		{
			const bool bShaft = Arrow.bFrom && MovesDistance(Arrow.From, Arrow.To) > StartGap + EndGap + HeadLong;
			if (bShaft)
			{
				const double Long = MovesDistance(Arrow.From, Arrow.To);
				const FPoint Along{ (Arrow.To.X - Arrow.From.X) / Long, (Arrow.To.Y - Arrow.From.Y) / Long };
				const FPoint Side{ -Along.Y, Along.X };
				// (each arrow bows a little, the next one the other way, so two roads between the same places do not lie on one another)
				const double Bow = Long * 0.12 * ((Bend++ % 2) == 0 ? 1.0 : -1.0);
				const FPoint Start{ Arrow.From.X + Along.X * StartGap, Arrow.From.Y + Along.Y * StartGap };
				const FPoint End{ Arrow.To.X - Along.X * EndGap, Arrow.To.Y - Along.Y * EndGap };
				const FPoint Middle = MovesLerp(Start, End, 0.5);
				const FPoint Bent{ Middle.X + Side.X * Bow, Middle.Y + Side.Y * Bow };
				// The shaft runs to where the head's base will be; the head is laid at the curve's end as it grows.
				const double Reached = Share;
				std::vector<FPoint> Line;
				for (int K = 0; K <= CurveSteps; ++K)
				{
					Line.push_back(MovesCurve(Start, Bent, End, Reached * K / CurveSteps));
				}
				const FPoint Tip = Line.back();
				const FPoint Before = MovesCurve(Start, Bent, End, std::max(Reached - 0.04, 0.0));
				double Dx = Tip.X - Before.X, Dy = Tip.Y - Before.Y;
				const double Dl = std::hypot(Dx, Dy);
				if (Dl < 1e-9)
				{
					Dx = Along.X;
					Dy = Along.Y;
				}
				else
				{
					Dx /= Dl;
					Dy /= Dl;
				}
				// (the shaft stops where the head's base begins, so the two never lie on one another)
				const FPoint Base{ Tip.X - Dx * HeadLong, Tip.Y - Dy * HeadLong };
				while (Line.size() > 2 && MovesDistance(Line[Line.size() - 2], Tip) < HeadLong)
				{
					Line.pop_back();
				}
				Line.back() = Base;
				const double Half = MovesHalf(Arrow);
				const FInk Ink = MovesInkOf(Arrow);
				if (Arrow.Kind == EKind::Fell)
				{
					// His own corps falling back: dashed, as a retreat is drawn.
					double Run = 0.0;
					for (size_t K = 1; K < Line.size(); ++K)
					{
						const double Step = MovesDistance(Line[K - 1], Line[K]);
						const bool bDrawn = std::fmod(Run, 5.0) < 3.0;
						Run += Step;
						if (bDrawn)
						{
							MovesStroke(Out.Under, { Line[K - 1], Line[K] }, Half + EdgeOver, EdgeLift, EdgeInk);
							MovesStroke(Out.Over, { Line[K - 1], Line[K] }, Half, InkLift, Ink);
						}
					}
				}
				else
				{
					MovesStroke(Out.Under, Line, Half + EdgeOver, EdgeLift, MovesFaded(EdgeInk, Arrow.bAway ? 0.6 : 1.0));
					MovesStroke(Out.Over, Line, Half, InkLift, Ink);
				}
				const double Wide = HeadWide + (Half - 1.0) * 0.8;
				const FPoint Left{ Base.X - Dy * Wide, Base.Y + Dx * Wide }, Right{ Base.X + Dy * Wide, Base.Y - Dx * Wide };
				const double Over = EdgeOver * 1.6;
				FTriangle Edge;
				Edge.A = FPoint{ Tip.X + Dx * Over, Tip.Y + Dy * Over };
				Edge.B = FPoint{ Left.X - Dy * Over - Dx * Over * 0.5, Left.Y + Dx * Over - Dy * Over * 0.5 };
				Edge.C = FPoint{ Right.X + Dy * Over - Dx * Over * 0.5, Right.Y - Dx * Over - Dy * Over * 0.5 };
				Edge.Lift = EdgeLift;
				Edge.Ink = MovesFaded(EdgeInk, Arrow.bAway ? 0.6 : 1.0);
				Out.Under.Triangles.push_back(Edge);
				FTriangle Head;
				Head.A = Tip;
				Head.B = Left;
				Head.C = Right;
				Head.Lift = InkLift;
				Head.Ink = Ink;
				Out.Over.Triangles.push_back(Head);
			}
			if (Arrow.Kind == EKind::Attack)
			{
				// The mark lands as the arrow arrives (at once where there is no arrow to wait for).
				MovesMark(Arrow, bShaft ? std::clamp((Share - 0.8) / 0.2, 0.0, 1.0) : Share, Out);
			}
		}
	}

	FFrame Frame(const std::vector<FPoint>& Points)
	{
		FFrame Out;
		if (Points.empty())
		{
			return Out;
		}
		double MinX = Points[0].X, MaxX = Points[0].X, MinY = Points[0].Y, MaxY = Points[0].Y;
		for (const FPoint& Point : Points)
		{
			MinX = std::min(MinX, Point.X);
			MaxX = std::max(MaxX, Point.X);
			MinY = std::min(MinY, Point.Y);
			MaxY = std::max(MaxY, Point.Y);
		}
		Out.Middle = FPoint{ (MinX + MaxX) * 0.5, (MinY + MaxY) * 0.5 };
		for (const FPoint& Point : Points)
		{
			Out.Reach = std::max(Out.Reach, MovesDistance(Out.Middle, Point));
		}
		Out.bValid = true;
		return Out;
	}

	double ZoomFor(double Reach, double UnitScale, double MinDistance, double MaxDistance)
	{
		if (!(MinDistance > 0.0) || !(MaxDistance > MinDistance))
		{
			return 0.5;
		}
		// (the eye looks down at about 50 degrees here; the screen's height holds about 0.64 of the distance at the
		// focus, and the farthest point is kept within the middle seven tenths of it, with a province's room round it)
		const double Room = (std::max(Reach, 0.0) + 40.0) * UnitScale;
		const double Distance = Room / (0.32 * 0.7);
		const double Zoom = std::log(std::max(Distance, MinDistance) / MinDistance) / std::log(MaxDistance / MinDistance);
		return std::clamp(Zoom, 0.40, 0.66);
	}

	double Pause(int Attacks, int Lost, int Moves, double Raids, double Sinkings)
	{
		double Hold = 0.0;
		if (Attacks > 0)
		{
			Hold = 2.6 + 0.8 * std::min(Attacks - 1, 3) + (Lost > 0 ? 0.6 : 0.0);
		}
		else if (Moves > 0)
		{
			Hold = 2.0 + 0.15 * std::min(Moves - 1, 4);
		}
		else
		{
			Hold = 1.6;
		}
		return std::max({ Hold, Raids, Sinkings });
	}
}
