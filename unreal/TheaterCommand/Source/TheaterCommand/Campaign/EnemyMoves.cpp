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
		// (his allies' battles near him: a blue arrow where his side attacked, a steel ring round the place)
		const FInk AllyInk = MovesInk(70, 118, 184);
		const FInk AllyRingInk = MovesInk(175, 195, 225); // (art director, 24 Nov, round 2b: lifted from 150,172,200, which sat on the French grey)

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
		// (art director, 24 Nov) His own corps' fall-back, where it shares its road with an attack or a move, keeps to the
		// other side of it, and this far off its line, so that both read.
		constexpr double FellAside = 2.6;
		// (art director, 24 Nov) A soft contact shadow under every arrow and mark, so that it lies on the table and not in
		// the light: two dark ribbons a little down and to the side of it, under its edge.
		constexpr double ShadowLift = 0.42;
		const FPoint ShadowShift{ 0.7, 0.7 };

		/** Half the width of an arrow's shaft: more corps, a little wider; an attack wider still; his own retreat narrower. */
		double MovesHalf(const FArrow& Arrow)
		{
			const double Size = std::min(std::max(Arrow.Corps, 1), 6) - 1;
			switch (Arrow.Kind)
			{
			case EKind::Attack:
				return Arrow.bAlly ? 1.05 + 0.09 * Size : 1.35 + 0.1 * Size;
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
				return Arrow.bAlly && Arrow.bOurs ? AllyInk : AttackInk;
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

		/** The soft shadow under a ribbon of this half-width (its dark edge's), faded with it by Share. */
		void MovesShadow(FLayer& Out, const std::vector<FPoint>& Points, double Half, double Share, bool bClosed = false)
		{
			std::vector<FPoint> Moved;
			Moved.reserve(Points.size());
			for (const FPoint& Point : Points)
			{
				Moved.push_back(FPoint{ Point.X + ShadowShift.X, Point.Y + ShadowShift.Y });
			}
			MovesStroke(Out, Moved, Half + 0.9, ShadowLift, MovesInk(0, 0, 0, static_cast<unsigned char>(34 * Share)), bClosed);
			MovesStroke(Out, std::move(Moved), Half + 0.35, ShadowLift + 0.005, MovesInk(0, 0, 0, static_cast<unsigned char>(46 * Share)), bClosed);
		}

		/** The shadow of a triangle (an arrowhead), a little down and to the side. */
		void MovesShadowTriangle(FLayer& Out, const FTriangle& Face, double Share)
		{
			FTriangle Shade = Face;
			for (FPoint* Corner : { &Shade.A, &Shade.B, &Shade.C })
			{
				Corner->X += ShadowShift.X;
				Corner->Y += ShadowShift.Y;
			}
			Shade.Lift = ShadowLift;
			Shade.Ink = MovesInk(0, 0, 0, static_cast<unsigned char>(70 * Share));
			Out.Triangles.push_back(Shade);
		}

		/**
		 * One sabre for the battle mark (art director, 24 Nov: crossed sabres are the plain map mark for a battle; an X in
		 * a ring reads as "no"). Its hilt at Hilt, its point Long further along Along, its blade bowing a little to Bow's
		 * side and narrowing to the point; a guard across it and a grip below. Laid as triangles: the dark edge in Under,
		 * the ink in Over.
		 */
		void MovesSabre(const FPoint& Hilt, const FPoint& Along, const FPoint& Bow, double Long, const FInk& Ink, double Show, FDrawing& Out)
		{
			const FPoint Side{ -Along.Y, Along.X };
			const double Grip = Long * 0.2;
			const FPoint Guard{ Hilt.X + Along.X * Grip, Hilt.Y + Along.Y * Grip };
			const FPoint Point{ Hilt.X + Along.X * Long, Hilt.Y + Along.Y * Long };
			const FPoint Bent{ (Guard.X + Point.X) * 0.5 + Bow.X * Long * 0.09, (Guard.Y + Point.Y) * 0.5 + Bow.Y * Long * 0.09 };
			constexpr int Steps = 10;
			auto Blade = [&](double Wider, double Longer, FLayer& Layer, double Lift, const FInk& Colour)
			{
				std::vector<FPoint> Left, Right;
				for (int K = 0; K <= Steps; ++K)
				{
					const double Share = static_cast<double>(K) / Steps;
					const FPoint At = MovesCurve(Guard, Bent, Point, Share);
					const FPoint Ahead = MovesCurve(Guard, Bent, Point, std::min(Share + 0.05, 1.0));
					const FPoint Behind = MovesCurve(Guard, Bent, Point, std::max(Share - 0.05, 0.0));
					double Dx = Ahead.X - Behind.X, Dy = Ahead.Y - Behind.Y;
					const double Dl = std::max(std::hypot(Dx, Dy), 1e-9);
					Dx /= Dl;
					Dy /= Dl;
					const double Wide = 0.9 * (1.0 - 0.8 * Share * Share) + Wider;
					Left.push_back(FPoint{ At.X - Dy * Wide, At.Y + Dx * Wide });
					Right.push_back(FPoint{ At.X + Dy * Wide, At.Y - Dx * Wide });
				}
				const FPoint Tip{ Point.X + Along.X * Longer, Point.Y + Along.Y * Longer };
				for (size_t K = 0; K + 1 < Left.size(); ++K)
				{
					MovesTriangle(Layer, Left[K], Right[K], Right[K + 1], Lift, Colour);
					MovesTriangle(Layer, Left[K], Right[K + 1], Left[K + 1], Lift, Colour);
				}
				MovesTriangle(Layer, Left.back(), Right.back(), Tip, Lift, Colour);
			};
			Blade(EdgeOver, EdgeOver * 1.4, Out.Under, MarkEdgeLift, MovesFaded(EdgeInk, Show));
			Blade(0.0, 0.0, Out.Over, MarkLift, MovesFaded(Ink, Show));
			const double GuardHalf = Long * 0.19;
			const std::vector<FPoint> Across{ FPoint{ Guard.X - Side.X * GuardHalf, Guard.Y - Side.Y * GuardHalf }, FPoint{ Guard.X + Side.X * GuardHalf, Guard.Y + Side.Y * GuardHalf } };
			const std::vector<FPoint> Handle{ Hilt, Guard };
			MovesStroke(Out.Under, Across, 0.5 + EdgeOver, MarkEdgeLift, MovesFaded(EdgeInk, Show));
			MovesStroke(Out.Under, Handle, 0.5 + EdgeOver, MarkEdgeLift, MovesFaded(EdgeInk, Show));
			MovesStroke(Out.Over, Across, 0.5, MarkLift, MovesFaded(Ink, Show));
			MovesStroke(Out.Over, Handle, 0.5, MarkLift, MovesFaded(Ink, Show));
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
		 * The battle mark where his troops were attacked: a ring with two crossed sabres in it, as a situation map marks
		 * a fight. Red where the place was lost (and the ring filled faintly), brass where it held, a brass ring with red
		 * sabres where the field was lost and the town holds. Show grows it in from a little larger, so that it is seen to land.
		 */
		void MovesMark(const FArrow& Arrow, double Show, FDrawing& Out)
		{
			if (Show <= 0.0)
			{
				return;
			}
			const double Radius = MarkRadius * (Arrow.bAlly ? 0.82 : 1.0) * (1.0 + 0.35 * (1.0 - Show));
			// (an allies' battle: a steel ring; its sabres in the colour of whoever won the place, steel where it held)
			const FInk Winner = Arrow.bAlly && Arrow.bOurs ? AllyInk : AttackInk;
			const FInk Ring = Arrow.bAlly ? AllyRingInk : Arrow.bTaken ? AttackInk : HeldInk;
			const FInk Blades = Arrow.bTaken || Arrow.bSiege ? Winner : Arrow.bAlly ? AllyRingInk : HeldInk;
			MovesShadow(Out.Under, MovesRing(Arrow.To, Radius, 28), 0.75 + EdgeOver, Show, true);
			MovesStroke(Out.Under, MovesRing(Arrow.To, Radius, 28), 0.75 + EdgeOver, MarkEdgeLift, MovesFaded(EdgeInk, Show), true);
			MovesStroke(Out.Over, MovesRing(Arrow.To, Radius, 28), 0.75, MarkLift, MovesFaded(Ring, Show), true);
			// (two sabres, hilts low, points high, crossing at the ring's middle; each bows outward, as a pair is drawn)
			const double Long = Radius * 1.36;
			const double Root = std::sqrt(0.5);
			const FPoint UpRight{ Root, -Root }, UpLeft{ -Root, -Root };
			MovesSabre(FPoint{ Arrow.To.X - UpRight.X * Long * 0.5, Arrow.To.Y - UpRight.Y * Long * 0.5 }, UpRight, FPoint{ Root, Root }, Long, Blades, Show, Out);
			MovesSabre(FPoint{ Arrow.To.X - UpLeft.X * Long * 0.5, Arrow.To.Y - UpLeft.Y * Long * 0.5 }, UpLeft, FPoint{ -Root, Root }, Long, Blades, Show, Out);
			if (Arrow.bTaken)
			{
				// (a place lost: the ring is filled faintly, a fan of triangles from its middle)
				const std::vector<FPoint> Rim = MovesRing(Arrow.To, Radius - 0.4, 28);
				for (size_t K = 0; K < Rim.size(); ++K)
				{
					MovesTriangle(Out.Under, Arrow.To, Rim[K], Rim[(K + 1) % Rim.size()], MarkEdgeLift - 0.02, MovesFaded(Winner, 0.2 * Show));
				}
			}
		}
	}

	double Grown(double Seconds)
	{
		const double Share = std::clamp(Seconds / GrowSeconds, 0.0, 1.0);
		return 1.0 - (1.0 - Share) * (1.0 - Share) * (1.0 - Share); // (fast at first, settling at its end)
	}

	std::vector<FArrow> Fold(const std::vector<FArrow>& Arrows)
	{
		// (how bad an arrow is, for which of two on one road is kept)
		auto Rank = [](const FArrow& Arrow)
		{
			if (Arrow.Kind == EKind::Attack)
			{
				return Arrow.bAlly ? 3 : 4;
			}
			return Arrow.bAway ? 1 : 2;
		};
		auto Foldable = [](const FArrow& Arrow) { return Arrow.bFrom && Arrow.Kind != EKind::Fell; };
		std::vector<FArrow> Kept;
		Kept.reserve(Arrows.size());
		for (const FArrow& Arrow : Arrows)
		{
			bool bFolded = false;
			if (Foldable(Arrow))
			{
				for (FArrow& Earlier : Kept)
				{
					if (!Foldable(Earlier) || MovesDistance(Earlier.From, Arrow.From) >= 1.0 || MovesDistance(Earlier.To, Arrow.To) >= 1.0)
					{
						continue;
					}
					const bool bBothMoves = Earlier.Kind == EKind::Move && Arrow.Kind == EKind::Move;
					const int Corps = bBothMoves ? Earlier.Corps + Arrow.Corps : std::max(Earlier.Corps, Arrow.Corps);
					const bool bWar = Earlier.bWar || Arrow.bWar;
					if (Rank(Arrow) > Rank(Earlier))
					{
						Earlier = Arrow;
					}
					Earlier.Corps = Corps;
					if (bBothMoves)
					{
						Earlier.bWar = bWar;
					}
					bFolded = true;
					break;
				}
			}
			if (!bFolded)
			{
				Kept.push_back(Arrow);
			}
		}
		return Kept;
	}

	void Lay(const std::vector<FArrow>& Given, double Grow, FDrawing& Out)
	{
		// (art director, 24 Nov, round 2b: one arrow for each road and direction)
		const std::vector<FArrow> Arrows = Fold(Given);
		Out.Clear();
		const double Share = std::clamp(Grow, 0.0, 1.0);
		if (Share <= 0.0)
		{
			return;
		}
		// (each arrow bows a little, the next one the other way, so two roads between the same places do not lie on one
		// another; his own fall-back, where an attack or a move runs the same road, bows away from it and keeps aside)
		// (art director, 24 Nov: his corps' fall-back from a place that was attacked starts at the battle ring's rim, not
		// inside it among the sabres)
		std::vector<double> Gaps(Arrows.size(), StartGap);
		for (size_t K = 0; K < Arrows.size(); ++K)
		{
			for (const FArrow& Other : Arrows)
			{
				if (Arrows[K].Kind == EKind::Fell && Other.Kind == EKind::Attack && MovesDistance(Other.To, Arrows[K].From) < 1.0)
				{
					Gaps[K] = std::max(Gaps[K], MarkRadius * (Other.bAlly ? 0.82 : 1.0) + 1.2);
				}
			}
		}
		auto HasShaft = [&Gaps, &Arrows](size_t K) { return Arrows[K].bFrom && MovesDistance(Arrows[K].From, Arrows[K].To) > Gaps[K] + EndGap + HeadLong; };
		std::vector<double> Bows(Arrows.size(), 0.0);
		int Bend = 0;
		for (size_t K = 0; K < Arrows.size(); ++K)
		{
			Bows[K] = HasShaft(K) ? ((Bend++ % 2) == 0 ? 1.0 : -1.0) : 0.0;
		}
		// (round 2b: one road the same way is folded into one arrow above; one road both ways keeps both arrows, and as
		// each bows to its own side of its way, the same sign puts them on either side of the road)
		for (size_t K = 0; K < Arrows.size(); ++K)
		{
			for (size_t J = 0; J < K && Bows[K] != 0.0; ++J)
			{
				if (Bows[J] != 0.0 && Arrows[J].Kind != EKind::Fell && Arrows[K].Kind != EKind::Fell && MovesDistance(Arrows[J].From, Arrows[K].To) < 1.0 && MovesDistance(Arrows[J].To, Arrows[K].From) < 1.0)
				{
					Bows[K] = Bows[J];
					break;
				}
			}
		}
		std::vector<bool> Aside(Arrows.size(), false);
		for (size_t K = 0; K < Arrows.size(); ++K)
		{
			if (Arrows[K].Kind != EKind::Fell || Bows[K] == 0.0)
			{
				continue;
			}
			for (size_t J = 0; J < Arrows.size(); ++J)
			{
				if (J != K && Arrows[J].Kind != EKind::Fell && Bows[J] != 0.0 && MovesDistance(Arrows[J].From, Arrows[K].From) < 1.0 && MovesDistance(Arrows[J].To, Arrows[K].To) < 1.0)
				{
					Bows[K] = -Bows[J];
					Aside[K] = true;
					break;
				}
			}
		}
		for (size_t Index = 0; Index < Arrows.size(); ++Index)
		{
			const FArrow& Arrow = Arrows[Index];
			const bool bShaft = Bows[Index] != 0.0;
			if (bShaft)
			{
				const double Long = MovesDistance(Arrow.From, Arrow.To);
				const FPoint Along{ (Arrow.To.X - Arrow.From.X) / Long, (Arrow.To.Y - Arrow.From.Y) / Long };
				const FPoint Side{ -Along.Y, Along.X };
				const double Bow = Long * 0.12 * Bows[Index];
				const double Off = Aside[Index] ? FellAside * Bows[Index] : 0.0;
				const FPoint Start{ Arrow.From.X + Along.X * Gaps[Index] + Side.X * Off, Arrow.From.Y + Along.Y * Gaps[Index] + Side.Y * Off };
				const FPoint End{ Arrow.To.X - Along.X * EndGap + Side.X * Off, Arrow.To.Y - Along.Y * EndGap + Side.Y * Off };
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
							MovesShadow(Out.Under, { Line[K - 1], Line[K] }, Half + EdgeOver, 1.0);
							MovesStroke(Out.Under, { Line[K - 1], Line[K] }, Half + EdgeOver, EdgeLift, EdgeInk);
							MovesStroke(Out.Over, { Line[K - 1], Line[K] }, Half, InkLift, Ink);
						}
					}
				}
				else
				{
					MovesShadow(Out.Under, Line, Half + EdgeOver, Arrow.bAway ? 0.6 : 1.0);
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
				MovesShadowTriangle(Out.Under, Edge, Arrow.bAway ? 0.6 : 1.0);
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

	int Worst(const std::vector<FArrow>& Arrows)
	{
		for (size_t K = 0; K < Arrows.size(); ++K)
		{
			if (Arrows[K].Kind == EKind::Attack && !Arrows[K].bAlly)
			{
				return static_cast<int>(K);
			}
		}
		for (size_t K = 0; K < Arrows.size(); ++K)
		{
			if (Arrows[K].Kind == EKind::Move && !Arrows[K].bAway)
			{
				return static_cast<int>(K);
			}
		}
		return Arrows.empty() ? -1 : 0;
	}

	FPingLook Ping(EPing State, double Seconds, int Order)
	{
		FPingLook Look;
		switch (State)
		{
		case EPing::Seen:
			Look.Dot = 2.0;
			Look.DotAlpha = 0.45;
			break;
		case EPing::Now:
			Look.Dot = 3.0;
			Look.DotAlpha = 1.0;
			Look.Ring = 7.5;
			Look.RingAlpha = 1.0;
			Look.RingThick = 2.0;
			Look.bBrassRing = true;
			break;
		case EPing::ToCome:
		{
			// (each a third of a swell after the one before, so that the eye reads them in their order)
			double Phase = std::fmod(Seconds - 0.45 * std::max(Order, 0), PingSeconds);
			Phase = (Phase < 0.0 ? Phase + PingSeconds : Phase) / PingSeconds;
			const double Rest = 1.0 - Phase;
			Look.Dot = 2.6;
			Look.DotAlpha = 1.0;
			Look.Ring = 3.0 + 9.0 * (1.0 - Rest * Rest);
			Look.RingAlpha = 0.95 * Rest * std::sqrt(Rest);
			Look.RingThick = 1.6;
			break;
		}
		}
		return Look;
	}

	FInk PingInk(const FArrow& Arrow)
	{
		if (Arrow.bAlly)
		{
			return AllyRingInk;
		}
		switch (Arrow.Kind)
		{
		case EKind::Attack:
			return AttackInk;
		case EKind::Fell:
			return OwnInk;
		case EKind::Move:
			break;
		}
		return Arrow.bWar ? WarInk : PeaceInk;
	}
}
