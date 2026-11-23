#include "Battle/AirLight.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <queue>
#include <unordered_map>

namespace AirLight
{
	namespace
	{
		struct V3
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
		};
		V3 operator+(const V3& A, const V3& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
		V3 operator-(const V3& A, const V3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
		V3 operator*(const V3& A, double K) { return { A.X * K, A.Y * K, A.Z * K }; }
		double Dot(const V3& A, const V3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		V3 Cross(const V3& A, const V3& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		double Length(const V3& A) { return std::sqrt(Dot(A, A)); }
		V3 Unit(const V3& A)
		{
			const double L = Length(A);
			return L > 0.0 ? A * (1.0 / L) : V3{};
		}
		V3 PointAt(const std::vector<double>& Flat, size_t K) { return { Flat[K * 3], Flat[K * 3 + 1], Flat[K * 3 + 2] }; }

		/** A sum of squared distances to planes, as the upper half of a symmetric 4 by 4 matrix: aa ab ac ad bb bc bd cc
		 * cd dd. */
		struct Quadric
		{
			double Q[10] = {};

			void Plane(const V3& Facing, double D, double Weight)
			{
				const double A = Facing.X, B = Facing.Y, C = Facing.Z;
				Q[0] += Weight * A * A;
				Q[1] += Weight * A * B;
				Q[2] += Weight * A * C;
				Q[3] += Weight * A * D;
				Q[4] += Weight * B * B;
				Q[5] += Weight * B * C;
				Q[6] += Weight * B * D;
				Q[7] += Weight * C * C;
				Q[8] += Weight * C * D;
				Q[9] += Weight * D * D;
			}

			void Join(const Quadric& Other)
			{
				for (int K = 0; K < 10; ++K)
				{
					Q[K] += Other.Q[K];
				}
			}

			double Error(const V3& At) const
			{
				const double X = At.X, Y = At.Y, Z = At.Z;
				return Q[0] * X * X + 2.0 * Q[1] * X * Y + 2.0 * Q[2] * X * Z + 2.0 * Q[3] * X + Q[4] * Y * Y + 2.0 * Q[5] * Y * Z + 2.0 * Q[6] * Y
					+ Q[7] * Z * Z + 2.0 * Q[8] * Z + Q[9];
			}

			/** The point of least error, where the planes settle it (false where they leave a line or a plane free). */
			bool Best(V3& Out) const
			{
				const double A = Q[0], B = Q[1], C = Q[2], E = Q[4], F = Q[5], I = Q[7];
				const double C00 = E * I - F * F, C01 = C * F - B * I, C02 = B * F - C * E;
				const double C11 = A * I - C * C, C12 = B * C - A * F, C22 = A * E - B * B;
				const double Det = A * C00 + B * C01 + C * C02;
				const double Scale = std::fabs(A) + std::fabs(E) + std::fabs(I);
				if (!(Scale > 0.0) || std::fabs(Det) < 1e-7 * Scale * Scale * Scale)
				{
					return false;
				}
				const double R0 = -Q[3], R1 = -Q[6], R2 = -Q[8];
				Out = { (C00 * R0 + C01 * R1 + C02 * R2) / Det, (C01 * R0 + C11 * R1 + C12 * R2) / Det, (C02 * R0 + C12 * R1 + C22 * R2) / Det };
				return std::isfinite(Out.X) && std::isfinite(Out.Y) && std::isfinite(Out.Z);
			}
		};

		struct Face
		{
			std::array<int, 3> V{};
			uint32_t Tone = 0;
			bool bDead = false;

			bool Has(int Point) const { return V[0] == Point || V[1] == Point || V[2] == Point; }
		};

		struct PlaceKey
		{
			long long X = 0, Y = 0, Z = 0;
			bool operator==(const PlaceKey& O) const { return X == O.X && Y == O.Y && Z == O.Z; }
		};
		struct PlaceHash
		{
			size_t operator()(const PlaceKey& K) const
			{
				const size_t H = std::hash<long long>()(K.X);
				const size_t H2 = std::hash<long long>()(K.Y) * 1000003u;
				const size_t H3 = std::hash<long long>()(K.Z) * 998244353u;
				return H ^ (H2 + 0x9e3779b97f4a7c15ull + (H << 6) + (H >> 2)) ^ H3;
			}
		};

		/** The point of triangle A B C nearest Pt (Ericson, Real-Time Collision Detection, 5.1.5). */
		V3 Nearest(const V3& Pt, const V3& A, const V3& B, const V3& C)
		{
			const V3 AB = B - A, AC = C - A, AP = Pt - A;
			const double D1 = Dot(AB, AP), D2 = Dot(AC, AP);
			if (D1 <= 0.0 && D2 <= 0.0)
			{
				return A;
			}
			const V3 BP = Pt - B;
			const double D3 = Dot(AB, BP), D4 = Dot(AC, BP);
			if (D3 >= 0.0 && D4 <= D3)
			{
				return B;
			}
			const double VC = D1 * D4 - D3 * D2;
			if (VC <= 0.0 && D1 >= 0.0 && D3 <= 0.0 && D1 - D3 > 0.0)
			{
				return A + AB * (D1 / (D1 - D3));
			}
			const V3 CP = Pt - C;
			const double D5 = Dot(AB, CP), D6 = Dot(AC, CP);
			if (D6 >= 0.0 && D5 <= D6)
			{
				return C;
			}
			const double VB = D5 * D2 - D1 * D6;
			if (VB <= 0.0 && D2 >= 0.0 && D6 <= 0.0 && D2 - D6 > 0.0)
			{
				return A + AC * (D2 / (D2 - D6));
			}
			const double VA = D3 * D6 - D5 * D4;
			if (VA <= 0.0 && D4 - D3 >= 0.0 && D5 - D6 >= 0.0 && (D4 - D3) + (D5 - D6) > 0.0)
			{
				return B + (C - B) * ((D4 - D3) / ((D4 - D3) + (D5 - D6)));
			}
			const double Sum = VA + VB + VC;
			if (!(std::fabs(Sum) > 0.0))
			{
				// (a triangle with no area: the nearest of its corners)
				const double DA = Dot(Pt - A, Pt - A), DB = Dot(Pt - B, Pt - B), DC = Dot(Pt - C, Pt - C);
				return DA <= DB && DA <= DC ? A : DB <= DC ? B : C;
			}
			return A + AB * (VB / Sum) + AC * (VC / Sum);
		}
	}

	Made Lighten(const Shape& In, int MostTriangles, double CreaseDegrees)
	{
		Made Out;
		const size_t Given = In.Points.size() / 3;
		if (Given == 0 || In.Triangles.size() < 3)
		{
			return Out;
		}
		const bool bTones = In.Tones.size() == Given;
		// Points at one place joined (to a grain far below anything drawn: a ten-millionth of the shape's size).
		V3 Lo{ std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max() };
		V3 Hi{ -Lo.X, -Lo.Y, -Lo.Z };
		for (size_t K = 0; K < Given; ++K)
		{
			const V3 At = PointAt(In.Points, K);
			Lo = { std::min(Lo.X, At.X), std::min(Lo.Y, At.Y), std::min(Lo.Z, At.Z) };
			Hi = { std::max(Hi.X, At.X), std::max(Hi.Y, At.Y), std::max(Hi.Z, At.Z) };
		}
		const double Grain = std::max(Length(Hi - Lo) * 1e-7, 1e-12);
		std::unordered_map<PlaceKey, int, PlaceHash> Joined;
		Joined.reserve(Given);
		std::vector<int> Into(Given, -1);
		std::vector<V3> P;
		P.reserve(Given);
		for (size_t K = 0; K < Given; ++K)
		{
			const V3 At = PointAt(In.Points, K);
			const PlaceKey Key{ std::llround(At.X / Grain), std::llround(At.Y / Grain), std::llround(At.Z / Grain) };
			const auto Found = Joined.find(Key);
			if (Found != Joined.end())
			{
				Into[K] = Found->second;
			}
			else
			{
				Into[K] = static_cast<int>(P.size());
				Joined.emplace(Key, Into[K]);
				P.push_back(At);
			}
		}
		std::vector<Face> F;
		F.reserve(In.Triangles.size() / 3);
		for (size_t T = 0; T + 2 < In.Triangles.size(); T += 3)
		{
			const int I0 = In.Triangles[T], I1 = In.Triangles[T + 1], I2 = In.Triangles[T + 2];
			if (I0 < 0 || I1 < 0 || I2 < 0 || static_cast<size_t>(I0) >= Given || static_cast<size_t>(I1) >= Given || static_cast<size_t>(I2) >= Given)
			{
				continue;
			}
			Face Made3;
			Made3.V = { Into[static_cast<size_t>(I0)], Into[static_cast<size_t>(I1)], Into[static_cast<size_t>(I2)] };
			if (Made3.V[0] == Made3.V[1] || Made3.V[1] == Made3.V[2] || Made3.V[0] == Made3.V[2])
			{
				continue;
			}
			if (bTones)
			{
				const uint32_t T0 = In.Tones[static_cast<size_t>(I0)], T1 = In.Tones[static_cast<size_t>(I1)], T2 = In.Tones[static_cast<size_t>(I2)];
				Made3.Tone = (T0 == T1 || T0 == T2) ? T0 : (T1 == T2 ? T1 : T0);
			}
			F.push_back(Made3);
		}
		Out.From = static_cast<int>(F.size());
		const size_t N = P.size();
		std::vector<std::vector<int>> Around(N);
		for (size_t K = 0; K < F.size(); ++K)
		{
			for (const int V : F[K].V)
			{
				Around[static_cast<size_t>(V)].push_back(static_cast<int>(K));
			}
		}
		auto FacingOf = [&P](const Face& Of) { return Cross(P[static_cast<size_t>(Of.V[1])] - P[static_cast<size_t>(Of.V[0])], P[static_cast<size_t>(Of.V[2])] - P[static_cast<size_t>(Of.V[0])]); };

		// Each point's planes: its triangles', weighed by their areas; and upright planes on the open rims and the paints'
		// lines, weighed by the edge's length squared (a rim's eight times a triangle's own, a paint line's twice).
		std::vector<Quadric> Qs(N);
		for (const Face& Of : F)
		{
			const V3 Twice = FacingOf(Of);
			const double Area2 = Length(Twice);
			if (!(Area2 > 0.0))
			{
				continue;
			}
			const V3 Way = Twice * (1.0 / Area2);
			const double D = -Dot(Way, P[static_cast<size_t>(Of.V[0])]);
			for (const int V : Of.V)
			{
				Qs[static_cast<size_t>(V)].Plane(Way, D, Area2 * 0.5);
			}
		}
		std::unordered_map<unsigned long long, std::array<int, 3>> Edges; // (up to two faces kept, and how many met)
		Edges.reserve(F.size() * 2);
		auto EdgeKey = [](int A, int B) { return (static_cast<unsigned long long>(static_cast<unsigned>(std::min(A, B))) << 32) | static_cast<unsigned>(std::max(A, B)); };
		for (size_t K = 0; K < F.size(); ++K)
		{
			for (int C = 0; C < 3; ++C)
			{
				auto& Met = Edges.try_emplace(EdgeKey(F[K].V[static_cast<size_t>(C)], F[K].V[static_cast<size_t>((C + 1) % 3)]), std::array<int, 3>{ -1, -1, 0 }).first->second;
				if (Met[2] < 2)
				{
					Met[static_cast<size_t>(Met[2])] = static_cast<int>(K);
				}
				++Met[2];
			}
		}
		constexpr double RimWeight = 8.0, PaintLineWeight = 2.0;
		std::vector<char> bRim(N, 0);
		for (const auto& [Key, Met] : Edges)
		{
			const bool bOpen = Met[2] == 1;
			const bool bPaintLine = Met[2] == 2 && F[static_cast<size_t>(Met[0])].Tone != F[static_cast<size_t>(Met[1])].Tone;
			if (!bOpen && !bPaintLine)
			{
				continue;
			}
			const int A = static_cast<int>(Key >> 32), B = static_cast<int>(Key & 0xffffffffull);
			if (bOpen)
			{
				bRim[static_cast<size_t>(A)] = bRim[static_cast<size_t>(B)] = 1;
			}
			const V3 Along = P[static_cast<size_t>(B)] - P[static_cast<size_t>(A)];
			const double Long2 = Dot(Along, Along);
			for (int Side = 0; Side < (bOpen ? 1 : 2); ++Side)
			{
				const V3 Up = Unit(Cross(Along, Unit(FacingOf(F[static_cast<size_t>(Met[static_cast<size_t>(Side)])]))));
				if (Length(Up) < 0.5)
				{
					continue;
				}
				const double D = -Dot(Up, P[static_cast<size_t>(A)]);
				const double Weight = (bOpen ? RimWeight : PaintLineWeight) * Long2;
				Qs[static_cast<size_t>(A)].Plane(Up, D, Weight);
				Qs[static_cast<size_t>(B)].Plane(Up, D, Weight);
			}
		}

		// The edges offered, cheapest first; an offer is good while both its ends are as they were when it was made.
		struct Offer
		{
			double Cost = 0.0;
			int A = 0, B = 0;
			unsigned VerA = 0, VerB = 0;
			V3 At;
		};
		auto Dearer = [](const Offer& L, const Offer& R) { return L.Cost > R.Cost; };
		std::priority_queue<Offer, std::vector<Offer>, decltype(Dearer)> Heap(Dearer);
		std::vector<unsigned> Ver(N, 0u);
		std::vector<char> Alive(N, 1);
		auto Offered = [&](int A, int B)
		{
			Offer O;
			O.A = A;
			O.B = B;
			O.VerA = Ver[static_cast<size_t>(A)];
			O.VerB = Ver[static_cast<size_t>(B)];
			Quadric Q = Qs[static_cast<size_t>(A)];
			Q.Join(Qs[static_cast<size_t>(B)]);
			const V3 PA = P[static_cast<size_t>(A)], PB = P[static_cast<size_t>(B)];
			const V3 Mid = (PA + PB) * 0.5;
			const double Long = Length(PB - PA);
			V3 Best;
			if (Q.Best(Best) && Length(Best - Mid) <= 2.0 * Long + Grain)
			{
				O.At = Best;
				O.Cost = Q.Error(Best);
			}
			else
			{
				// (the planes leave the point free along a line or a plane, or settle it far off: the better end, or the
				// middle)
				const double EA = Q.Error(PA), EB = Q.Error(PB), EM = Q.Error(Mid);
				O.At = EA <= EB && EA <= EM ? PA : (EB <= EM ? PB : Mid);
				O.Cost = std::min(EA, std::min(EB, EM));
			}
			// (on flat ground, where every join costs nothing, the shortest edges first)
			O.Cost = std::max(0.0, O.Cost) + Long * Long * Long * Long * 1e-6;
			Heap.push(O);
		};
		for (const auto& Pair : Edges)
		{
			Offered(static_cast<int>(Pair.first >> 32), static_cast<int>(Pair.first & 0xffffffffull));
		}

		std::vector<int> RingA, RingB, Common;
		auto Ring = [&](int V, std::vector<int>& Into3)
		{
			Into3.clear();
			for (const int K : Around[static_cast<size_t>(V)])
			{
				const Face& Of = F[static_cast<size_t>(K)];
				if (Of.bDead)
				{
					continue;
				}
				for (const int W : Of.V)
				{
					if (W != V)
					{
						Into3.push_back(W);
					}
				}
			}
			std::sort(Into3.begin(), Into3.end());
			Into3.erase(std::unique(Into3.begin(), Into3.end()), Into3.end());
		};
		int Live = static_cast<int>(F.size());
		const int Want = std::max(1, MostTriangles);
		while (Live > Want && !Heap.empty())
		{
			const Offer O = Heap.top();
			Heap.pop();
			const size_t UA = static_cast<size_t>(O.A), UB = static_cast<size_t>(O.B);
			if (!Alive[UA] || !Alive[UB] || Ver[UA] != O.VerA || Ver[UB] != O.VerB)
			{
				continue;
			}
			int OnEdge = 0;
			for (const int K : Around[UA])
			{
				OnEdge += !F[static_cast<size_t>(K)].bDead && F[static_cast<size_t>(K)].Has(O.B) ? 1 : 0;
			}
			if (OnEdge == 0 || OnEdge > 2)
			{
				continue;
			}
			// (no tear: the two ends share no neighbour but the triangles' third corners; and an edge across the surface
			// whose two ends both lie on rims would pinch it)
			Ring(O.A, RingA);
			Ring(O.B, RingB);
			Common.clear();
			std::set_intersection(RingA.begin(), RingA.end(), RingB.begin(), RingB.end(), std::back_inserter(Common));
			if (static_cast<int>(Common.size()) != OnEdge || (OnEdge == 2 && bRim[UA] && bRim[UB]))
			{
				continue;
			}
			// (no triangle turned over, nor squashed flat)
			bool bFine = true;
			for (int Side = 0; Side < 2 && bFine; ++Side)
			{
				const int Moved = Side == 0 ? O.A : O.B, Other = Side == 0 ? O.B : O.A;
				for (const int K : Around[static_cast<size_t>(Moved)])
				{
					const Face& Of = F[static_cast<size_t>(K)];
					if (Of.bDead || Of.Has(Other))
					{
						continue;
					}
					V3 C3[3];
					for (int C = 0; C < 3; ++C)
					{
						C3[C] = Of.V[static_cast<size_t>(C)] == Moved ? O.At : P[static_cast<size_t>(Of.V[static_cast<size_t>(C)])];
					}
					const V3 Was = FacingOf(Of);
					const V3 Now = Cross(C3[1] - C3[0], C3[2] - C3[0]);
					const double LNow = Length(Now), LWas = Length(Was);
					if (!(LNow > 1e-12 * std::max(1.0, LWas)) || Dot(Was, Now) < 0.2 * LWas * LNow)
					{
						bFine = false;
						break;
					}
				}
			}
			if (!bFine)
			{
				continue;
			}
			// The join: B's triangles go to A, the edge's own ones go.
			const int Keep = O.A, Gone = O.B;
			P[UA] = O.At;
			Qs[UA].Join(Qs[UB]);
			bRim[UA] = static_cast<char>(bRim[UA] || bRim[UB]);
			Alive[UB] = 0;
			for (const int K : Around[UB])
			{
				Face& Of = F[static_cast<size_t>(K)];
				if (Of.bDead)
				{
					continue;
				}
				if (Of.Has(Keep))
				{
					Of.bDead = true;
					--Live;
					continue;
				}
				for (int& V : Of.V)
				{
					V = V == Gone ? Keep : V;
				}
				Around[UA].push_back(K);
			}
			Around[UB].clear();
			std::vector<int>& Mine = Around[UA];
			Mine.erase(std::remove_if(Mine.begin(), Mine.end(), [&F](int K) { return F[static_cast<size_t>(K)].bDead; }), Mine.end());
			std::sort(Mine.begin(), Mine.end());
			Mine.erase(std::unique(Mine.begin(), Mine.end()), Mine.end());
			++Ver[UA];
			++Ver[UB];
			Ring(Keep, RingA);
			for (const int W : RingA)
			{
				Offered(Keep, W);
			}
		}

		// Laid out: each corner faced as the triangles round its point that bend less than the crease from its own.
		const double Pi = 3.14159265358979323846;
		const double CosCrease = std::cos(std::clamp(CreaseDegrees, 0.0, 180.0) * Pi / 180.0);
		std::vector<V3> Twice(F.size()), Way(F.size());
		for (size_t K = 0; K < F.size(); ++K)
		{
			if (!F[K].bDead)
			{
				Twice[K] = FacingOf(F[K]);
				Way[K] = Unit(Twice[K]);
			}
		}
		struct CornerKey
		{
			int Point = 0;
			long long NX = 0, NY = 0, NZ = 0;
			uint32_t Tone = 0;
			bool operator==(const CornerKey& O) const { return Point == O.Point && NX == O.NX && NY == O.NY && NZ == O.NZ && Tone == O.Tone; }
		};
		struct CornerHash
		{
			size_t operator()(const CornerKey& K) const
			{
				size_t H = std::hash<int>()(K.Point);
				for (const long long V : { K.NX, K.NY, K.NZ, static_cast<long long>(K.Tone) })
				{
					H ^= std::hash<long long>()(V) + 0x9e3779b97f4a7c15ull + (H << 6) + (H >> 2);
				}
				return H;
			}
		};
		std::unordered_map<CornerKey, int, CornerHash> Corners;
		for (size_t K = 0; K < F.size(); ++K)
		{
			const Face& Of = F[K];
			if (Of.bDead || !(Length(Twice[K]) > 0.0))
			{
				continue;
			}
			for (const int V : Of.V)
			{
				V3 Sum;
				for (const int G : Around[static_cast<size_t>(V)])
				{
					if (!F[static_cast<size_t>(G)].bDead && Dot(Way[K], Way[static_cast<size_t>(G)]) >= CosCrease)
					{
						Sum = Sum + Twice[static_cast<size_t>(G)];
					}
				}
				V3 Facing = Unit(Sum);
				if (!(Length(Facing) > 0.5))
				{
					Facing = Way[K];
				}
				const CornerKey Key{ V, std::llround(Facing.X * 1e5), std::llround(Facing.Y * 1e5), std::llround(Facing.Z * 1e5), Of.Tone };
				const auto Found = Corners.find(Key);
				int Index = 0;
				if (Found != Corners.end())
				{
					Index = Found->second;
				}
				else
				{
					Index = static_cast<int>(Out.Tones.size());
					Corners.emplace(Key, Index);
					const V3& At = P[static_cast<size_t>(V)];
					Out.Points.insert(Out.Points.end(), { At.X, At.Y, At.Z });
					Out.Normals.insert(Out.Normals.end(), { Facing.X, Facing.Y, Facing.Z });
					Out.Tones.push_back(Of.Tone);
				}
				Out.Triangles.push_back(Index);
			}
			++Out.Kept;
		}
		return Out;
	}

	Apart Distance(const std::vector<double>& APoints, const std::vector<double>& BPoints, const std::vector<int>& BTriangles)
	{
		Apart Out;
		const size_t Count = APoints.size() / 3, BCount = BPoints.size() / 3;
		if (Count == 0 || BTriangles.size() < 3)
		{
			return Out;
		}
		double Sum = 0.0;
		for (size_t K = 0; K < Count; ++K)
		{
			const V3 Pt = PointAt(APoints, K);
			double Least = std::numeric_limits<double>::max();
			for (size_t T = 0; T + 2 < BTriangles.size(); T += 3)
			{
				const int I0 = BTriangles[T], I1 = BTriangles[T + 1], I2 = BTriangles[T + 2];
				if (I0 < 0 || I1 < 0 || I2 < 0 || static_cast<size_t>(I0) >= BCount || static_cast<size_t>(I1) >= BCount || static_cast<size_t>(I2) >= BCount)
				{
					continue;
				}
				const V3 Near = Nearest(Pt, PointAt(BPoints, static_cast<size_t>(I0)), PointAt(BPoints, static_cast<size_t>(I1)), PointAt(BPoints, static_cast<size_t>(I2)));
				Least = std::min(Least, Dot(Pt - Near, Pt - Near));
			}
			const double D = std::sqrt(Least);
			Out.Most = std::max(Out.Most, D);
			Sum += D;
		}
		Out.Mean = Sum / static_cast<double>(Count);
		return Out;
	}
}
