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
	}

	size_t OpenEdges(const std::vector<double>& X, const std::vector<double>& Y, const std::vector<int>& Tris)
	{
		return Rim(X, Y, Tris).size();
	}

	double Luma(const Rgb8& C)
	{
		return 0.299 * C.R + 0.587 * C.G + 0.114 * C.B;
	}

	std::vector<Drawn> Paint(const std::vector<Part>& Parts, const std::vector<std::string>& Names, const std::vector<Rgb8>& Palette, const Rgb8& Sea, Tally* Out)
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
		const int Sand = Named("sand"), Cane = Named("cane"), Earth = Named("pineapple"), Forest = Named("forest"), Runway = Named("runway"), Lagoon = Named("lagoon"), Scrub = Named("scrub");
		for (const Part& P : Parts)
		{
			Count.bLagoon = Count.bLagoon || (Lagoon >= 0 && std::find(P.Paints.begin(), P.Paints.end(), Lagoon) != P.Paints.end());
		}
		std::unordered_set<int64_t> LandPlaces;
		Filed ReefRims;
		for (const Part& P : Parts)
		{
			if (P.Name == "land")
			{
				for (size_t K = 0; K < P.X.size(); ++K)
				{
					LandPlaces.insert(PlaceKey(P.X[K], P.Y[K]));
				}
			}
			else if (P.Name == "reef")
			{
				ReefRims.File(Rim(P.X, P.Y, P.Tris));
			}
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
				for (size_t K = 0; K < Points; ++K)
				{
					const double X = P.X[K], Y = P.Y[K];
					if (P.Paints[K] == Cane)
					{
						Rgb8 C = Shaded(D.Colours[K], 0.82 + 0.36 * (0.6 * Noise(X, Y, 380.0, 12) + 0.4 * Noise(X, Y, 140.0, 14)));
						const double Red = Earth >= 0 ? 0.6 * Step(0.68, 0.9, Noise(X + 5000.0, Y - 3000.0, 480.0, 11)) : 0.0;
						if (Red > 0.02)
						{
							C = Blend(C, ColourOf(Earth), Red);
							++Count.Earth;
						}
						D.Colours[K] = C;
					}
					else if (P.Paints[K] == Forest)
					{
						D.Colours[K] = Shaded(D.Colours[K], 0.9 + 0.2 * Noise(X, Y, 520.0, 13));
					}
					else if (P.Paints[K] == Sand && P.Z[K] > 0.45)
					{
						// (the sand at the water's edge only: a point back from it, where the coast's ramp has lifted it,
						// is mostly the grass behind, so a beach is a band along the water and not a fan to the next point)
						D.Colours[K] = Blend(D.Colours[K], Inland, 0.85 * std::clamp((P.Z[K] - 0.4) / 1.5, 0.0, 1.0));
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
						D.Colours.push_back(Blend(ColourOf(Sand), Inland, Green));
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
					const bool bCoast = LandPlaces.count(PlaceKey(P.X[K], P.Y[K])) > 0 || ReefRims.Nearest(P.X[K], P.Y[K], 6.0) < 6.0;
					if (bCoast)
					{
						++Count.ShoreKept;
					}
					else
					{
						D.Colours[K] = Sea;
						++Count.ShoreFaded;
					}
				}
			}
			else if (P.Name == "works" && Runway >= 0)
			{
				for (size_t K = 0; K < Points; ++K)
				{
					if (P.Paints[K] == Runway)
					{
						D.Z[K] += 0.8;
						++Count.RunwayRaised;
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
