#include "Battle/AirGround.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace AirGround
{
	namespace
	{
		uint64_t Mix(uint64_t H)
		{
			H ^= H >> 33;
			H *= 0xFF51AFD7ED558CCDull;
			H ^= H >> 33;
			H *= 0xC4CEB9FE1A85EC53ull;
			H ^= H >> 33;
			return H;
		}

		double Hashed(int64_t I, int64_t J, uint64_t Seed)
		{
			const uint64_t H = Mix(static_cast<uint64_t>(I) * 0x9E3779B97F4A7C15ull ^ Mix(static_cast<uint64_t>(J) + Seed * 0x632BE59BD9B4E019ull));
			return static_cast<double>(H >> 11) / 9007199254740992.0;
		}

		double Ease(double T)
		{
			return T * T * (3.0 - 2.0 * T);
		}

		/** Smooth value noise, 0 to 1, its lattice Scale metres apart. */
		double Noise(double X, double Y, double Scale, uint64_t Seed)
		{
			const double U = X / Scale, V = Y / Scale;
			const double FU = std::floor(U), FV = std::floor(V);
			const int64_t IU = static_cast<int64_t>(FU), IV = static_cast<int64_t>(FV);
			const double A = Ease(U - FU), B = Ease(V - FV);
			const double N00 = Hashed(IU, IV, Seed), N10 = Hashed(IU + 1, IV, Seed), N01 = Hashed(IU, IV + 1, Seed), N11 = Hashed(IU + 1, IV + 1, Seed);
			return (N00 * (1.0 - A) + N10 * A) * (1.0 - B) + (N01 * (1.0 - A) + N11 * A) * B;
		}

		double Step(double Low, double High, double X)
		{
			return Ease(std::clamp((X - Low) / (High - Low), 0.0, 1.0));
		}

		uint8_t Byte(double V)
		{
			return static_cast<uint8_t>(std::clamp(std::lround(V), 0L, 255L));
		}

		Rgb8 Blend(const Rgb8& A, const Rgb8& B, double T)
		{
			Rgb8 C;
			C.R = Byte(A.R + (B.R - A.R) * T);
			C.G = Byte(A.G + (B.G - A.G) * T);
			C.B = Byte(A.B + (B.B - A.B) * T);
			return C;
		}

		Rgb8 Shaded(const Rgb8& A, double By)
		{
			Rgb8 C;
			C.R = Byte(A.R * By);
			C.G = Byte(A.G * By);
			C.B = Byte(A.B * By);
			return C;
		}

		/** A place to a tenth of a metre, as the stage file gives it (decimetres): one key for one place, whichever mesh. */
		int64_t PlaceKey(double X, double Y)
		{
			const int64_t XD = std::llround(X * 10.0), YD = std::llround(Y * 10.0);
			return static_cast<int64_t>((static_cast<uint64_t>(XD) << 32) ^ (static_cast<uint64_t>(YD) & 0xFFFFFFFFull));
		}

		struct Segment
		{
			double X0, Y0, X1, Y1;
		};

		/** The mesh's edges used by one triangle only: its rim (on the land, the coast). Points at one place are one. */
		std::vector<Segment> Rim(const std::vector<double>& PX, const std::vector<double>& PY, const std::vector<int>& PT)
		{
			std::vector<std::pair<int64_t, int64_t>> Edges;
			std::vector<std::pair<int, int>> Ends;
			Edges.reserve(PT.size());
			for (size_t K = 0; K + 2 < PT.size(); K += 3)
			{
				for (int E = 0; E < 3; ++E)
				{
					const int A = PT[K + E], B = PT[K + (E + 1) % 3];
					const int64_t KA = PlaceKey(PX[A], PY[A]), KB = PlaceKey(PX[B], PY[B]);
					if (KA != KB)
					{
						Edges.push_back(KA < KB ? std::make_pair(KA, KB) : std::make_pair(KB, KA));
						Ends.push_back(KA < KB ? std::make_pair(A, B) : std::make_pair(B, A));
					}
				}
			}
			std::vector<size_t> Order(Edges.size());
			for (size_t K = 0; K < Order.size(); ++K)
			{
				Order[K] = K;
			}
			std::sort(Order.begin(), Order.end(), [&Edges](size_t L, size_t R) { return Edges[L] < Edges[R]; });
			std::vector<Segment> Out;
			for (size_t K = 0; K < Order.size();)
			{
				size_t Run = K + 1;
				while (Run < Order.size() && Edges[Order[Run]] == Edges[Order[K]])
				{
					++Run;
				}
				if (Run - K == 1)
				{
					const std::pair<int, int>& End = Ends[Order[K]];
					Out.push_back({ PX[End.first], PY[End.first], PX[End.second], PY[End.second] });
				}
				K = Run;
			}
			return Out;
		}

		/** Segments filed by square, for "how far to the nearest" within a reach. */
		struct Filed
		{
			double Cell = 40.0;
			std::vector<Segment> Lines;
			std::unordered_map<int64_t, std::vector<int>> Squares;

			static int64_t SquareKey(int64_t I, int64_t J)
			{
				return static_cast<int64_t>((static_cast<uint64_t>(I) << 32) ^ (static_cast<uint64_t>(J) & 0xFFFFFFFFull));
			}

			void File(const std::vector<Segment>& InLines)
			{
				Lines = InLines;
				for (int K = 0; K < static_cast<int>(Lines.size()); ++K)
				{
					const Segment& S = Lines[K];
					const int64_t I0 = static_cast<int64_t>(std::floor(std::min(S.X0, S.X1) / Cell)), I1 = static_cast<int64_t>(std::floor(std::max(S.X0, S.X1) / Cell));
					const int64_t J0 = static_cast<int64_t>(std::floor(std::min(S.Y0, S.Y1) / Cell)), J1 = static_cast<int64_t>(std::floor(std::max(S.Y0, S.Y1) / Cell));
					for (int64_t I = I0; I <= I1 && I - I0 < 64; ++I)
					{
						for (int64_t J = J0; J <= J1 && J - J0 < 64; ++J)
						{
							Squares[SquareKey(I, J)].push_back(K);
						}
					}
				}
			}

			/** The distance to the nearest line within Reach metres, or Reach if none is that near. */
			double Nearest(double X, double Y, double Reach) const
			{
				double Best = Reach;
				const int64_t I0 = static_cast<int64_t>(std::floor((X - Reach) / Cell)), I1 = static_cast<int64_t>(std::floor((X + Reach) / Cell));
				const int64_t J0 = static_cast<int64_t>(std::floor((Y - Reach) / Cell)), J1 = static_cast<int64_t>(std::floor((Y + Reach) / Cell));
				for (int64_t I = I0; I <= I1; ++I)
				{
					for (int64_t J = J0; J <= J1; ++J)
					{
						const auto Found = Squares.find(SquareKey(I, J));
						if (Found == Squares.end())
						{
							continue;
						}
						for (const int K : Found->second)
						{
							const Segment& S = Lines[K];
							const double DX = S.X1 - S.X0, DY = S.Y1 - S.Y0, LL = DX * DX + DY * DY;
							const double T = LL > 0.0 ? std::clamp(((X - S.X0) * DX + (Y - S.Y0) * DY) / LL, 0.0, 1.0) : 0.0;
							Best = std::min(Best, std::hypot(X - (S.X0 + DX * T), Y - (S.Y0 + DY * T)));
						}
					}
				}
				return Best;
			}
		};

		/**
		 * The part's triangles near the focus split until no edge there is longer than Edge: a point at an edge's middle,
		 * one for the edge (keyed by its ends' places), so both triangles on it share it and no crack opens. Its height
		 * is the edge's, so the ground keeps its shape; its paint is one end's, chosen by its place, so a boundary between
		 * two paints runs ragged at the finer size and not as a straight band; but a point between the sand and anything
		 * else (Yield) takes the other paint, since sand belongs only at the water's edge. Returns the points laid.
		 */
		int Refine(Part& P, const Focus& Near, double Edge, int Yield)
		{
			if (Near.Radius <= 0.0 || Edge <= 0.0 || P.Paints.size() != P.X.size() || P.Y.size() != P.X.size() || P.Z.size() != P.X.size())
			{
				return 0;
			}
			int Added = 0;
			for (int Pass = 0; Pass < 6; ++Pass)
			{
				std::unordered_map<uint64_t, int> Middles;
				auto Middle = [&](int A, int B)
				{
					const size_t AS = static_cast<size_t>(A), BS = static_cast<size_t>(B);
					const double MX = 0.5 * (P.X[AS] + P.X[BS]), MY = 0.5 * (P.Y[AS] + P.Y[BS]);
					if (std::hypot(P.X[BS] - P.X[AS], P.Y[BS] - P.Y[AS]) <= Edge || std::hypot(MX - Near.X, MY - Near.Y) > Near.Radius)
					{
						return -1;
					}
					const int64_t KA = PlaceKey(P.X[AS], P.Y[AS]), KB = PlaceKey(P.X[BS], P.Y[BS]);
					const uint64_t Key = Mix(static_cast<uint64_t>(std::min(KA, KB)) * 31u) ^ static_cast<uint64_t>(std::max(KA, KB));
					const auto Found = Middles.find(Key);
					if (Found != Middles.end())
					{
						return Found->second;
					}
					const int Made = static_cast<int>(P.X.size());
					const int PaintA = P.Paints[AS], PaintB = P.Paints[BS];
					P.X.push_back(MX);
					P.Y.push_back(MY);
					P.Z.push_back(0.5 * (P.Z[AS] + P.Z[BS]));
					P.Paints.push_back(PaintA == PaintB || PaintB == Yield ? PaintA : PaintA == Yield ? PaintB : Hashed(std::llround(MX * 10.0), std::llround(MY * 10.0), 17) < 0.5 ? PaintA : PaintB);
					Middles.emplace(Key, Made);
					++Added;
					return Made;
				};
				std::vector<int> Next;
				Next.reserve(P.Tris.size() * 2);
				bool bAny = false;
				for (size_t K = 0; K + 2 < P.Tris.size(); K += 3)
				{
					int V[3] = { P.Tris[K], P.Tris[K + 1], P.Tris[K + 2] };
					int M[3] = { Middle(V[0], V[1]), Middle(V[1], V[2]), Middle(V[2], V[0]) };
					const int Split = static_cast<int>(M[0] >= 0) + static_cast<int>(M[1] >= 0) + static_cast<int>(M[2] >= 0);
					if (Split == 0)
					{
						Next.insert(Next.end(), { V[0], V[1], V[2] });
						continue;
					}
					bAny = true;
					if (Split == 3)
					{
						Next.insert(Next.end(), { V[0], M[0], M[2], M[0], V[1], M[1], M[2], M[1], V[2], M[0], M[1], M[2] });
						continue;
					}
					// (turned, the winding kept, so that the one split edge, or the one edge not split, is in a known place)
					for (int Turn = 0; Turn < 3 && !(Split == 1 ? M[0] >= 0 : M[2] < 0); ++Turn)
					{
						std::rotate(V, V + 1, V + 3);
						std::rotate(M, M + 1, M + 3);
					}
					if (Split == 1)
					{
						Next.insert(Next.end(), { V[0], M[0], V[2], M[0], V[1], V[2] });
						continue;
					}
					Next.insert(Next.end(), { M[0], V[1], M[1] });
					auto Apart = [&P](int I, int J) { return std::hypot(P.X[static_cast<size_t>(J)] - P.X[static_cast<size_t>(I)], P.Y[static_cast<size_t>(J)] - P.Y[static_cast<size_t>(I)]); };
					if (Apart(V[0], M[1]) <= Apart(M[0], V[2]))
					{
						Next.insert(Next.end(), { V[0], M[0], M[1], V[0], M[1], V[2] });
					}
					else
					{
						Next.insert(Next.end(), { V[0], M[0], V[2], M[0], M[1], V[2] });
					}
				}
				P.Tris = std::move(Next);
				if (!bAny)
				{
					break;
				}
			}
			return Added;
		}
	}

	size_t OpenEdges(const std::vector<double>& X, const std::vector<double>& Y, const std::vector<int>& Tris)
	{
		return Rim(X, Y, Tris).size();
	}

	double RimLength(const std::vector<double>& X, const std::vector<double>& Y, const std::vector<int>& Tris)
	{
		double Sum = 0.0;
		for (const Segment& S : Rim(X, Y, Tris))
		{
			Sum += std::hypot(S.X1 - S.X0, S.Y1 - S.Y0);
		}
		return Sum;
	}

	Rgb8 Worn(const Rgb8& Base, double X, double Y)
	{
		// (repairs and pours some 40 m across a few per cent apart, about 8 luma either way on 140; oil and standing
		// water here and there a stain some 15 m across, up to about 10 darker: the art director's 6 to 10)
		const double Patch = 0.06 * (2.0 * Noise(X, Y, 40.0, 21) - 1.0);
		const double Stain = 0.07 * Step(0.7, 0.92, Noise(X + 700.0, Y - 300.0, 16.0, 22));
		return Shaded(Base, 1.0 + Patch - Stain);
	}

	std::vector<Panel> Paving(double X, double Y, double Long, double Wide, double Heading, const Rgb8& Base, double Cell)
	{
		std::vector<Panel> Panels;
		if (!(Long > 0.0) || !(Wide > 0.0) || !(Cell > 0.0))
		{
			return Panels;
		}
		const int Along = std::max(1, static_cast<int>(std::lround(Long / Cell))), Across = std::max(1, static_cast<int>(std::lround(Wide / Cell)));
		const double H = Heading * 3.14159265358979323846 / 180.0;
		const double Ux = std::sin(H), Uy = std::cos(H), Vx = Uy, Vy = -Ux;
		Panels.reserve(static_cast<size_t>(Along) * static_cast<size_t>(Across));
		for (int I = 0; I < Along; ++I)
		{
			for (int J = 0; J < Across; ++J)
			{
				const double A = (I + 0.5) * Long / Along - Long * 0.5, B = (J + 0.5) * Wide / Across - Wide * 0.5;
				Panel Slab;
				Slab.X = X + Ux * A + Vx * B;
				Slab.Y = Y + Uy * A + Vy * B;
				Slab.Long = Long / Along;
				Slab.Wide = Wide / Across;
				// (each slab its own shade, up to 3 per cent either way, on the concrete as it wears where it lies)
				const double Own = 0.03 * (2.0 * Hashed(std::llround(Slab.X * 10.0), std::llround(Slab.Y * 10.0), 23) - 1.0);
				Slab.Colour = Shaded(Worn(Base, Slab.X, Slab.Y), 1.0 + Own);
				Panels.push_back(Slab);
			}
		}
		return Panels;
	}

	double Luma(const Rgb8& C)
	{
		return 0.299 * C.R + 0.587 * C.G + 0.114 * C.B;
	}

	std::vector<Drawn> Paint(const std::vector<Part>& Given, const std::vector<std::string>& Names, const std::vector<Rgb8>& Palette, const Rgb8& Sea, Tally* Out, const Focus& Near)
	{
		Tally Count;
		auto Named = [&Names](const char* Name)
		{
			for (size_t K = 0; K < Names.size(); ++K)
			{
				if (Names[K] == Name)
				{
					return static_cast<int>(K);
				}
			}
			return -1;
		};
		auto ColourOf = [&Palette](int Index)
		{
			return Index >= 0 && Index < static_cast<int>(Palette.size()) ? Palette[static_cast<size_t>(Index)] : Rgb8();
		};
		const int Sand = Named("sand"), Cane = Named("cane"), Earth = Named("pineapple"), Forest = Named("forest"), Runway = Named("runway"), Lagoon = Named("lagoon"), Scrub = Named("scrub"),
		          Concrete = Named("concrete");
		// (the land and the shallows split finer near the battle first: each point is then painted for its own place)
		std::vector<Part> Parts(Given);
		for (Part& P : Parts)
		{
			if (P.Name == "land" || P.Name == "shallows")
			{
				Count.Refined += Refine(P, Near, P.Name == "land" ? Near.LandEdge : Near.WaterEdge, Sand);
			}
		}
		for (const Part& P : Parts)
		{
			Count.bLagoon = Count.bLagoon || (Lagoon >= 0 && std::find(P.Paints.begin(), P.Paints.end(), Lagoon) != P.Paints.end());
		}
		std::vector<Segment> LandLines, ReefLines;
		for (const Part& P : Parts)
		{
			if (P.Name == "land" || P.Name == "reef")
			{
				std::vector<Segment>& Into = P.Name == "land" ? LandLines : ReefLines;
				const std::vector<Segment> Edges = Rim(P.X, P.Y, P.Tris);
				Into.insert(Into.end(), Edges.begin(), Edges.end());
			}
		}
		Filed LandRim, ReefRims;
		LandRim.File(LandLines);
		ReefRims.File(ReefLines);
		// The shallows' rim in two: the coast (on the land's rim, or on a reef's edge) and the band's open-sea edge. A point
		// of the shallows fades toward the sea by how far it lies from the one toward the other.
		Filed WaterCoast, WaterOuter;
		WaterCoast.Cell = WaterOuter.Cell = 80.0;
		{
			std::vector<Segment> CoastLines, OuterLines;
			auto OnCoast = [&LandRim, &ReefRims](double X, double Y) { return LandRim.Nearest(X, Y, 2.0) < 1.5 || ReefRims.Nearest(X, Y, 6.0) < 6.0; };
			for (const Part& P : Parts)
			{
				if (P.Name != "shallows")
				{
					continue;
				}
				for (const Segment& S : Rim(P.X, P.Y, P.Tris))
				{
					(OnCoast(S.X0, S.Y0) && OnCoast(S.X1, S.Y1) ? CoastLines : OuterLines).push_back(S);
				}
			}
			WaterCoast.File(CoastLines);
			WaterOuter.File(OuterLines);
		}
		// The piers, quays and seawalls (the works' outlines, every edge of their triangles as seen from above): a berth
		// was dredged, so the deep water runs right up to them and the shallows' pale band does not.
		Filed Walls;
		{
			std::vector<Segment> WallLines;
			for (const Part& P : Parts)
			{
				if (P.Name != "works")
				{
					continue;
				}
				for (size_t K = 0; K + 2 < P.Tris.size(); K += 3)
				{
					const int A = P.Tris[K], B = P.Tris[K + 1], C = P.Tris[K + 2];
					if (A < 0 || B < 0 || C < 0 || static_cast<size_t>(std::max({ A, B, C })) >= std::min(P.X.size(), P.Y.size()))
					{
						continue;
					}
					WallLines.push_back({ P.X[A], P.Y[A], P.X[B], P.Y[B] });
					WallLines.push_back({ P.X[B], P.Y[B], P.X[C], P.Y[C] });
					WallLines.push_back({ P.X[C], P.Y[C], P.X[A], P.Y[A] });
				}
			}
			Walls.File(WallLines);
		}

		std::vector<Drawn> All;
		All.reserve(Parts.size());
		for (const Part& P : Parts)
		{
			Drawn D;
			D.Name = P.Name;
			D.X = P.X;
			D.Y = P.Y;
			D.Z = P.Z;
			D.Tris = P.Tris;
			D.Colours.reserve(P.X.size());
			for (size_t K = 0; K < P.X.size(); ++K)
			{
				D.Colours.push_back(ColourOf(K < P.Paints.size() ? P.Paints[K] : -1));
			}
			const size_t Points = std::min(P.X.size(), P.Paints.size());
			if (P.Name == "land")
			{
				// The grass: broad patches some per cent lighter or darker, and here and there turned toward the red
				// earth of the pineapple fields' paint, in patches some hundreds of metres across; the forest a little
				// varied too. (One colour for one place, whichever point carries it, so no seam shows.)
				const Rgb8 Inland = Cane >= 0 ? ColourOf(Cane) : Scrub >= 0 ? ColourOf(Scrub) : ColourOf(Sand);
				// (the grass at a place, as a cane point there is painted)
				auto Grass = [&](double X, double Y)
				{
					Rgb8 C = Shaded(Inland, 0.82 + 0.36 * (0.6 * Noise(X, Y, 380.0, 12) + 0.4 * Noise(X, Y, 140.0, 14)));
					const double Red = Earth >= 0 ? 0.6 * Step(0.68, 0.9, Noise(X + 5000.0, Y - 3000.0, 480.0, 11)) : 0.0;
					if (Red > 0.02)
					{
						C = Blend(C, ColourOf(Earth), Red);
						++Count.Earth;
					}
					return C;
				};
				for (size_t K = 0; K < Points; ++K)
				{
					const double X = P.X[K], Y = P.Y[K];
					if (P.Paints[K] == Cane)
					{
						D.Colours[K] = Grass(X, Y);
					}
					else if (P.Paints[K] == Forest)
					{
						D.Colours[K] = Shaded(D.Colours[K], 0.9 + 0.2 * Noise(X, Y, 520.0, 13));
					}
					else if (P.Paints[K] == Sand && !Count.bLagoon && Cane >= 0 && WaterOuter.Nearest(X, Y, 140.0) < 130.0)
					{
						// (a shore in sheltered water, where the shallows' band is narrow, as in the harbour's lochs: mud
						// and grass to the water, not a beach; a thin sand edge there read as a drawn outline. The sand
						// points back from the water there too, which showed as a lattice of paler triangles in the coves)
						// (at the water's edge a little of the mud's colour left; back from it, even on a spit as low as the
						// coast, the grass as the grass is: a fifth of sand there showed the split points as a lattice)
						D.Colours[K] = P.Z[K] <= 0.45 && LandRim.Nearest(X, Y, 4.0) < 3.0 ? Blend(D.Colours[K], Grass(X, Y), 0.8) : Grass(X, Y);
						++Count.Sheltered;
					}
					else if (P.Paints[K] == Sand)
					{
						// (the sand at the water's edge only: a point back from it, where the coast's ramp has lifted it,
						// is mostly the grass behind, so a beach is a band along the water and not a fan to the next point.
						// Where no lagoon is, also by how far it lies from the coast, past 8 m: a low sand spit split fine
						// showed its points as a lattice of paler triangles. The grass as the grass is painted there.)
						const double Lifted = std::clamp((P.Z[K] - 0.4) / 1.5, 0.0, 1.0);
						const double Inward = Count.bLagoon ? 0.0 : std::clamp((LandRim.Nearest(X, Y, 60.0) - 8.0) / 30.0, 0.0, 1.0);
						const double Back = std::max(Lifted, Inward);
						if (Back > 0.0)
						{
							D.Colours[K] = Blend(D.Colours[K], Count.bLagoon || Cane < 0 ? Inland : Grass(X, Y), 0.85 * Back); // (Midway's sand as it was)
						}
					}
				}
				// The beach. Sand belongs at the water's edge (a point of the coast itself, 0.4 m up). An edge from such a
				// point to one back from the water, longer than 20 m, takes a point 10 m along it painted nearly as the
				// far end, so the sand fades out in 10 m and not across the whole triangle (one point an edge, shared by
				// both triangles on it, so no crack opens). A triangle with all three corners at the water's edge whose
				// middle lies well back from it takes a point there, painted from the sand toward the grass by how far
				// back it lies (6 m sand, 36 m and more mostly grass). So the sand runs along the coast as a band and no
				// straight edge of a block, nor a fan to the next point inland, shows.
				if (Sand >= 0)
				{
					Filed Coast;
					Coast.File(Rim(P.X, P.Y, P.Tris));
					auto Shore = [&P, Sand](int I) { return P.Paints[static_cast<size_t>(I)] == Sand && P.Z[static_cast<size_t>(I)] <= 0.45; };
					std::unordered_map<uint64_t, int> EdgePoints;
					// (the point 10 m in from the shore end W along the edge to the inland end I: one for the edge)
					auto EdgePoint = [&](int W, int I)
					{
						const uint64_t Key = Mix(static_cast<uint64_t>(PlaceKey(P.X[static_cast<size_t>(W)], P.Y[static_cast<size_t>(W)])) * 31u) ^ static_cast<uint64_t>(PlaceKey(P.X[static_cast<size_t>(I)], P.Y[static_cast<size_t>(I)]));
						const auto Found = EdgePoints.find(Key);
						if (Found != EdgePoints.end())
						{
							return Found->second;
						}
						const size_t WS = static_cast<size_t>(W), IS = static_cast<size_t>(I);
						const double T = 10.0 / std::hypot(P.X[IS] - P.X[WS], P.Y[IS] - P.Y[WS]);
						const int Made = static_cast<int>(D.X.size());
						D.X.push_back(P.X[WS] + (P.X[IS] - P.X[WS]) * T);
						D.Y.push_back(P.Y[WS] + (P.Y[IS] - P.Y[WS]) * T);
						D.Z.push_back(P.Z[WS] + (P.Z[IS] - P.Z[WS]) * T);
						D.Colours.push_back(Blend(D.Colours[IS], D.Colours[WS], 0.15));
						EdgePoints.emplace(Key, Made);
						++Count.SandSplit;
						return Made;
					};
					auto Long = [&P](int I, int J) { return std::hypot(P.X[static_cast<size_t>(J)] - P.X[static_cast<size_t>(I)], P.Y[static_cast<size_t>(J)] - P.Y[static_cast<size_t>(I)]) > 20.0; };
					const size_t Laid = P.Tris.size();
					std::vector<int> Kept;
					Kept.reserve(Laid + Laid / 4);
					for (size_t K = 0; K + 2 < Laid; K += 3)
					{
						int V[3] = { P.Tris[K], P.Tris[K + 1], P.Tris[K + 2] };
						const int Shores = static_cast<int>(Shore(V[0])) + static_cast<int>(Shore(V[1])) + static_cast<int>(Shore(V[2]));
						if (Shores == 1 || Shores == 2)
						{
							// (turned so the odd corner is first: the one shore point among two inland, or the one inland
							// point among two on the shore)
							while (Shore(V[0]) != (Shores == 1))
							{
								std::rotate(V, V + 1, V + 3);
							}
							const int A = V[0], B = V[1], C = V[2];
							if (Shores == 1 && Long(A, B) && Long(A, C))
							{
								const int SB = EdgePoint(A, B), SC = EdgePoint(A, C);
								Kept.insert(Kept.end(), { A, SB, SC, SB, B, C, SB, C, SC });
								continue;
							}
							if (Shores == 2 && Long(B, A) && Long(C, A))
							{
								const int SB = EdgePoint(B, A), SC = EdgePoint(C, A);
								Kept.insert(Kept.end(), { B, C, SC, B, SC, SB, SB, SC, A });
								continue;
							}
							if (Shores == 1 && (Long(A, B) || Long(A, C)))
							{
								const bool bOnB = Long(A, B);
								const int S = bOnB ? EdgePoint(A, B) : EdgePoint(A, C);
								Kept.insert(Kept.end(), bOnB ? std::initializer_list<int>{ A, S, C, S, B, C } : std::initializer_list<int>{ A, B, S, S, B, C });
								continue;
							}
							if (Shores == 2 && (Long(B, A) || Long(C, A)))
							{
								const bool bOnB = Long(B, A);
								const int S = bOnB ? EdgePoint(B, A) : EdgePoint(C, A);
								Kept.insert(Kept.end(), bOnB ? std::initializer_list<int>{ B, C, S, S, C, A } : std::initializer_list<int>{ B, C, S, B, S, A });
								continue;
							}
							Kept.insert(Kept.end(), { A, B, C });
							continue;
						}
						const int A = V[0], B = V[1], C = V[2];
						const double Area = 0.5 * std::fabs((P.X[B] - P.X[A]) * (P.Y[C] - P.Y[A]) - (P.X[C] - P.X[A]) * (P.Y[B] - P.Y[A]));
						const double MX = (P.X[A] + P.X[B] + P.X[C]) / 3.0, MY = (P.Y[A] + P.Y[B] + P.Y[C]) / 3.0;
						const double Back = Shores == 3 && Area > 400.0 ? Coast.Nearest(MX, MY, 60.0) : 0.0;
						const double Green = 0.85 * std::clamp((Back - 6.0) / 30.0, 0.0, 1.0);
						if (Green < 0.08)
						{
							Kept.insert(Kept.end(), { A, B, C });
							continue;
						}
						const int M = static_cast<int>(D.X.size());
						D.X.push_back(MX);
						D.Y.push_back(MY);
						D.Z.push_back((P.Z[A] + P.Z[B] + P.Z[C]) / 3.0);
						// (from the corners' own colours: on a sheltered shore they are already the grass's, and a sand
						// middle would show there as a pale star)
						const Rgb8 &CA = D.Colours[static_cast<size_t>(A)], &CB = D.Colours[static_cast<size_t>(B)], &CC = D.Colours[static_cast<size_t>(C)];
						const Rgb8 Middle = { Byte((CA.R + CB.R + CC.R) / 3.0), Byte((CA.G + CB.G + CC.G) / 3.0), Byte((CA.B + CB.B + CC.B) / 3.0) };
						D.Colours.push_back(Blend(Middle, Inland, Green));
						Kept.insert(Kept.end(), { A, B, M, B, C, M, C, A, M });
						++Count.SandSplit;
					}
					D.Tris = std::move(Kept);
				}
			}
			else if (P.Name == "shallows" && !Count.bLagoon)
			{
				// The shallows darken into the deep water: a point on the coast (where the land has one) or on a reef's
				// edge keeps the shallows' paint; one on the open-sea side takes the sea's. Each triangle runs from one
				// to the other, so the band fades across its own width.
				for (size_t K = 0; K < Points; ++K)
				{
					const double X = P.X[K], Y = P.Y[K];
					const double Coast = WaterCoast.Nearest(X, Y, 240.0), Open = WaterOuter.Nearest(X, Y, 240.0);
					const double Seaward = Coast + Open > 0.0 ? Coast / (Coast + Open) : 0.0;
					// (the art director on the second picture, 18:47Z: by a quay, a pier or a seawall the deep sea's paint
					// up to the wall, the band coming back over 40 m beside it; along the natural shores the band's width
					// wanders, so that it reads as shoal water and not as a glow drawn round the land)
					const double Wall = Walls.Lines.empty() ? 1.0 : 1.0 - Ease(std::clamp((Walls.Nearest(X, Y, 60.0) - 10.0) / 40.0, 0.0, 1.0));
					const double Wander = 0.55 + Noise(X, Y, 260.0, 31);
					const double Fade = std::max(Ease(std::pow(std::clamp(Seaward, 0.0, 1.0), Wander)), Wall);
					if (Wall > 0.5)
					{
						++Count.Walled;
					}
					if (Fade < 0.02)
					{
						++Count.ShoreKept;
						continue;
					}
					D.Colours[K] = Blend(D.Colours[K], Sea, Fade);
					++Count.ShoreFaded;
				}
			}
			else if (P.Name == "works")
			{
				for (size_t K = 0; K < Points; ++K)
				{
					if (Runway >= 0 && P.Paints[K] == Runway)
					{
						D.Z[K] += 0.8;
						++Count.RunwayRaised;
					}
					else if (Concrete >= 0 && P.Paints[K] == Concrete)
					{
						D.Colours[K] = Worn(D.Colours[K], P.X[K], P.Y[K]);
						++Count.Worn;
					}
				}
			}
			All.push_back(std::move(D));
		}
		if (Out)
		{
			*Out = Count;
		}
		return All;
	}
}
