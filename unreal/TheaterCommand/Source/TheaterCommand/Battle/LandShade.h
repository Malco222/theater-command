// Theater Command - the ground battle's footprint shade (release 2; the art director on cut 4, fix 3).
//
// Under a tank's belly the ground sees almost none of the sky and none of the bounce off the lit field, so it is dark
// whatever the sun does; out past her tracks it comes back to the open ground's light within a metre or so. The sun's
// shadow gives only part of that, and on the side toward the sun none of it, so a tank stood on the field "as on a
// mat". This lays that shade as a flat mesh over the ground (model axes, as her root stands), the share of the light
// left at each point in its colour, which the scene draws multiplying what is behind it (M_LandShade, build_content.py):
// deepest under the belly, a little lighter at the outline, back to nothing over about a metre outside, the outer edge
// wavering so that no straight line shows. The same for a gun (lighter), a lorry, a standing man (a small round blob)
// and a prone one (his body's length).
//
// No engine in here: Tools/land_field's test builds it with the ground and checks it.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace LandShade
{
	/** What is laid: an outline on the ground (a rounded box, model axes x forward, y right, metres from the root) and
	 * the shade's shares of light, as the eye would measure them (the material makes them linear). */
	struct Shape
	{
		double LoX = -3.0, LoY = -1.6, HiX = 3.0, HiY = 1.6;
		/** The outline's corners' radius (no more than half the box's shorter side). */
		double Corner = 0.6;
		/** How far inside the outline the shade comes to its deepest (no more than the corner's radius). */
		double Deep = 0.6;
		/** The share of light left at its deepest, at the outline, and (always) 1 at its outer edge. */
		double Belly = 0.35, Edge = 0.5;
		/** How far outside the outline it is back to none: varied by up to Wander of it along the outline. */
		double Feather = 1.0, Wander = 0.2;
		/** How high over the ground (model z 0) it is laid. */
		double Lift = 0.06;
		/** The outline's pieces: no straight piece longer than Step, Arc pieces a corner. */
		double Step = 0.7;
		int Arc = 5;
		/** For the outer edge's waver. */
		uint32_t Seed = 1;
	};

	/** The shade laid: points (x, y, z a point, model axes, metres), the share of light left at each (0 to 1), and
	 * triangles (three points' numbers each). Every triangle turns the same way seen from above, none overlapping. */
	struct Mesh
	{
		std::vector<float> Points;
		std::vector<float> Light;
		std::vector<uint32_t> Triangles;

		size_t Count() const { return Light.size(); }
		void Clear()
		{
			Points.clear();
			Light.clear();
			Triangles.clear();
		}
	};

	inline double Smooth(double T)
	{
		T = std::clamp(T, 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/** The outline's sizes as laid: the box's half sides, the corner's radius and the depth, kept sensible. */
	struct Fitted
	{
		double CX = 0.0, CY = 0.0, AX = 0.0, AY = 0.0, R = 0.0, Deep = 0.0;
	};

	inline Fitted Fit(const Shape& S)
	{
		Fitted F;
		F.CX = 0.5 * (S.LoX + S.HiX);
		F.CY = 0.5 * (S.LoY + S.HiY);
		const double HX = std::max(0.01, 0.5 * std::fabs(S.HiX - S.LoX)), HY = std::max(0.01, 0.5 * std::fabs(S.HiY - S.LoY));
		F.R = std::clamp(S.Corner, 0.005, std::min(HX, HY));
		F.AX = HX - F.R;
		F.AY = HY - F.R;
		F.Deep = std::clamp(S.Deep, 0.0, F.R);
		return F;
	}

	/** How far a point (model x, y) is outside the outline (under 0: inside). */
	inline double Outside(const Fitted& F, double X, double Y)
	{
		const double QX = std::fabs(X - F.CX) - F.AX, QY = std::fabs(Y - F.CY) - F.AY;
		return std::hypot(std::max(QX, 0.0), std::max(QY, 0.0)) + std::min(std::max(QX, QY), 0.0) - F.R;
	}

	/** The share of light left at D outside the outline (under 0: inside), with the feather there F. */
	inline double LightAt(const Shape& S, double Deep, double D, double Feather)
	{
		if (D <= 0.0)
		{
			return Deep > 1e-9 ? S.Belly + (S.Edge - S.Belly) * Smooth((D + Deep) / Deep) : S.Edge;
		}
		return S.Edge + (1.0 - S.Edge) * Smooth(D / std::max(1e-6, Feather));
	}

	/**
	 * Lays the shade (Out emptied first). Rings round the outline's core (the box less its corners' radius): four
	 * inside, from Deep in to the outline, and five outside to the feather's edge, its distance wavering along the
	 * outline (two waves round it, from the seed); the core filled from its middle at the deepest share.
	 */
	inline void Make(const Shape& S, Mesh& Out)
	{
		Out.Clear();
		const Fitted F = Fit(S);
		// The ring's places round the core: each a point on the core's edge and the way out from it, with its distance
		// along the outline (for the waver). Corners counter-clockwise from the front right, each its arc (its last
		// point left to the side after it), then the side to the next corner.
		struct Way
		{
			double X, Y, NX, NY, Along;
		};
		std::vector<Way> Ring;
		const double Pi = 3.14159265358979323846;
		const double CornerX[4] = { F.AX, -F.AX, -F.AX, F.AX }, CornerY[4] = { F.AY, F.AY, -F.AY, -F.AY };
		const int Arc = std::max(1, S.Arc);
		double Along = 0.0;
		for (int K = 0; K < 4; ++K)
		{
			const double From = K * 0.5 * Pi;
			for (int I = 0; I < Arc; ++I)
			{
				const double A = From + 0.5 * Pi * I / Arc;
				Ring.push_back({ F.CX + CornerX[K], F.CY + CornerY[K], std::cos(A), std::sin(A), Along });
				Along += F.R * 0.5 * Pi / Arc;
			}
			const int Next = (K + 1) % 4;
			const double SX = CornerX[Next] - CornerX[K], SY = CornerY[Next] - CornerY[K];
			const double Long = std::hypot(SX, SY);
			if (Long < 1e-9)
			{
				continue;
			}
			const double A = From + 0.5 * Pi;
			const int Pieces = std::max(1, static_cast<int>(std::ceil(Long / std::max(0.05, S.Step))));
			for (int M = 0; M < Pieces; ++M)
			{
				const double T = static_cast<double>(M) / Pieces;
				Ring.push_back({ F.CX + CornerX[K] + SX * T, F.CY + CornerY[K] + SY * T, std::cos(A), std::sin(A), Along });
				Along += Long / Pieces;
			}
		}
		const double Round = std::max(1e-6, Along);
		// The waver: two waves round the outline, about 3 m and 1.6 m long, their phases from the seed.
		uint32_t Rng = S.Seed * 2654435761u + 0x9e3779b9u;
		auto Rand = [&Rng]() { Rng = Rng * 1664525u + 1013904223u; return (Rng >> 8) / 16777216.0; };
		const double P1 = 2.0 * Pi * Rand(), P2 = 2.0 * Pi * Rand();
		const double N1 = std::max(1.0, std::round(Round / 3.0)), N2 = std::max(2.0, std::round(Round / 1.6));
		auto FeatherAt = [&](double At) {
			const double W = 0.6 * std::sin(2.0 * Pi * N1 * At / Round + P1) + 0.4 * std::sin(2.0 * Pi * N2 * At / Round + P2);
			return std::max(0.05, S.Feather * (1.0 + S.Wander * W));
		};
		// The rings, inside out: four from Deep in to the outline, five out to the feather's edge.
		const double Inside[4] = { 0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0 };
		const double Outward[5] = { 0.12, 0.3, 0.5, 0.72, 1.0 };
		const int Rings = 9;
		const size_t N = Ring.size();
		Out.Points.reserve((N * Rings + 1) * 3);
		Out.Light.reserve(N * Rings + 1);
		auto Put = [&Out, &S](double X, double Y, double Light) {
			Out.Points.push_back(static_cast<float>(X));
			Out.Points.push_back(static_cast<float>(Y));
			Out.Points.push_back(static_cast<float>(S.Lift));
			Out.Light.push_back(static_cast<float>(std::clamp(Light, 0.0, 1.0)));
		};
		// (the core's middle, at the deepest share)
		Put(F.CX, F.CY, S.Belly);
		for (int J = 0; J < Rings; ++J)
		{
			for (const Way& W : Ring)
			{
				// (how far outside the outline this ring is here)
				const double Feather = FeatherAt(W.Along);
				const double Off = J < 4 ? -F.Deep * (1.0 - Inside[J]) : Outward[J - 4] * Feather;
				const double Reach = F.R + Off;
				Put(W.X + W.NX * Reach, W.Y + W.NY * Reach, J == Rings - 1 ? 1.0 : LightAt(S, F.Deep, Off, Feather));
			}
		}
		// The core's fill: a fan from its middle to the first ring (the core's edge pushed out by R - Deep, convex).
		const uint32_t First = 1;
		for (size_t K = 0; K < N; ++K)
		{
			const uint32_t A = First + static_cast<uint32_t>(K), B = First + static_cast<uint32_t>((K + 1) % N);
			Out.Triangles.insert(Out.Triangles.end(), { 0u, A, B });
		}
		// Each ring to the next.
		for (int J = 0; J + 1 < Rings; ++J)
		{
			const uint32_t In = First + static_cast<uint32_t>(J * N), Outer = First + static_cast<uint32_t>((J + 1) * N);
			for (size_t K = 0; K < N; ++K)
			{
				const uint32_t K0 = static_cast<uint32_t>(K), K1 = static_cast<uint32_t>((K + 1) % N);
				Out.Triangles.insert(Out.Triangles.end(), { In + K0, Outer + K0, Outer + K1 });
				Out.Triangles.insert(Out.Triangles.end(), { In + K0, Outer + K1, In + K1 });
			}
		}
	}

	/** The shapes the battle lays. Box: the thing's outline on the ground (model axes). */
	inline Shape Tank(double LoX, double LoY, double HiX, double HiY)
	{
		Shape S;
		S.LoX = LoX;
		S.LoY = LoY;
		S.HiX = HiX;
		S.HiY = HiY;
		return S;
	}

	/** A lorry: more sky under her (high on her wheels), lighter. */
	inline Shape Lorry(double LoX, double LoY, double HiX, double HiY)
	{
		Shape S = Tank(LoX, LoY, HiX, HiY);
		S.Belly = 0.45;
		S.Edge = 0.6;
		S.Feather = 0.9;
		return S;
	}

	/** A gun: its shield, wheels and trails, about 0.6 at most. */
	inline Shape Gun(double LoX, double LoY, double HiX, double HiY)
	{
		Shape S = Tank(LoX, LoY, HiX, HiY);
		S.Corner = 0.4;
		S.Deep = 0.4;
		S.Belly = 0.6;
		S.Edge = 0.7;
		S.Feather = 0.6;
		return S;
	}

	/** A standing man: a round blob 0.6 m across at 0.7, soft to nothing 0.3 m beyond. */
	inline Shape Standing()
	{
		Shape S;
		S.LoX = -0.3;
		S.LoY = -0.3;
		S.HiX = 0.3;
		S.HiY = 0.3;
		S.Corner = 0.3;
		S.Deep = 0.3;
		S.Belly = 0.7;
		S.Edge = 0.82;
		S.Feather = 0.3;
		S.Wander = 0.1;
		S.Lift = 0.03;
		S.Arc = 3;
		return S;
	}

	/** A prone man: his body's shape, 1.8 m by 0.5, at 0.75 (his head toward x, his root at his hips). */
	inline Shape Prone()
	{
		Shape S;
		S.LoX = -0.95;
		S.LoY = -0.25;
		S.HiX = 0.85;
		S.HiY = 0.25;
		S.Corner = 0.25;
		S.Deep = 0.2;
		S.Belly = 0.75;
		S.Edge = 0.85;
		S.Feather = 0.25;
		S.Wander = 0.1;
		S.Lift = 0.03;
		S.Arc = 3;
		return S;
	}

	/** A body at rest: which way he fell is not known here, so a round shade a metre across at 0.75. */
	inline Shape Body()
	{
		Shape S = Standing();
		S.LoX = -0.5;
		S.LoY = -0.5;
		S.HiX = 0.5;
		S.HiY = 0.5;
		S.Corner = 0.5;
		S.Deep = 0.4;
		S.Belly = 0.75;
		S.Edge = 0.85;
		S.Feather = 0.35;
		return S;
	}
}
