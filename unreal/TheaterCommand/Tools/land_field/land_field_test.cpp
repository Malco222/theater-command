// The land battle's ground in the cloud (Battle/LandField, no engine): builds the ground of a recorded land battle as
// the game will, times it, counts what it laid, checks that what is drawn and what At() answers agree, and (with a
// folder for pictures) paints it with the small software painter in land_field_picture.cpp: a soldier's eye in a hedged
// field toward the village, 30 m up over the bocage toward the village, and the whole field from above.
//
// Usage: land_field_test <recording.json> [picture folder]
// The recording's men are its ways: tanks 2 m either side, guns and lorries 1.6 m, men half a metre.

#include "Battle/LandField.h"
#include "land_field_picture.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	// ------------------------------------------------------------------ a small JSON reader (the recording only)

	struct Json
	{
		enum Kind
		{
			Null,
			Bool,
			Number,
			String,
			Array,
			Object
		} K = Null;
		double N = 0.0;
		bool B = false;
		std::string S;
		std::vector<Json> A;
		std::vector<std::string> Names; // an object's keys, and beside them its values (a vector of pairs of an incomplete type fails under clang)
		std::vector<Json> Values;

		const Json* Get(const std::string& Key) const
		{
			for (size_t I = 0; I < Names.size(); ++I)
				if (Names[I] == Key) return &Values[I];
			return nullptr;
		}
		double Num(const std::string& Key, double Else) const
		{
			const Json* J = Get(Key);
			return J && J->K == Number ? J->N : Else;
		}
	};

	struct Reader
	{
		const std::string& T;
		size_t P = 0;
		void Space()
		{
			while (P < T.size() && (T[P] == ' ' || T[P] == '\n' || T[P] == '\r' || T[P] == '\t')) ++P;
		}
		bool Value(Json& Out)
		{
			Space();
			if (P >= T.size()) return false;
			const char C = T[P];
			if (C == '{')
			{
				Out.K = Json::Object;
				++P;
				Space();
				if (P < T.size() && T[P] == '}')
				{
					++P;
					return true;
				}
				for (;;)
				{
					Json Key;
					Space();
					if (!Text(Key.S)) return false;
					Space();
					if (P >= T.size() || T[P] != ':') return false;
					++P;
					Out.Names.push_back(Key.S);
					Out.Values.emplace_back();
					if (!Value(Out.Values.back())) return false;
					Space();
					if (P < T.size() && T[P] == ',')
					{
						++P;
						continue;
					}
					if (P < T.size() && T[P] == '}')
					{
						++P;
						return true;
					}
					return false;
				}
			}
			if (C == '[')
			{
				Out.K = Json::Array;
				++P;
				Space();
				if (P < T.size() && T[P] == ']')
				{
					++P;
					return true;
				}
				for (;;)
				{
					Out.A.emplace_back();
					if (!Value(Out.A.back())) return false;
					Space();
					if (P < T.size() && T[P] == ',')
					{
						++P;
						continue;
					}
					if (P < T.size() && T[P] == ']')
					{
						++P;
						return true;
					}
					return false;
				}
			}
			if (C == '"')
			{
				Out.K = Json::String;
				return Text(Out.S);
			}
			if (T.compare(P, 4, "true") == 0)
			{
				Out.K = Json::Bool;
				Out.B = true;
				P += 4;
				return true;
			}
			if (T.compare(P, 5, "false") == 0)
			{
				Out.K = Json::Bool;
				P += 5;
				return true;
			}
			if (T.compare(P, 4, "null") == 0)
			{
				P += 4;
				return true;
			}
			char* End = nullptr;
			Out.N = std::strtod(T.c_str() + P, &End);
			if (End == T.c_str() + P) return false;
			Out.K = Json::Number;
			P = static_cast<size_t>(End - T.c_str());
			return true;
		}
		bool Text(std::string& Out)
		{
			if (P >= T.size() || T[P] != '"') return false;
			++P;
			while (P < T.size() && T[P] != '"')
			{
				if (T[P] == '\\' && P + 1 < T.size())
				{
					++P;
					const char E = T[P];
					Out += E == 'n' ? '\n' : E == 't' ? '\t' : E == 'u' ? '?' : E;
					if (E == 'u') P += 4;
					++P;
					continue;
				}
				Out += T[P++];
			}
			++P;
			return true;
		}
	};

	double Seconds(std::chrono::steady_clock::time_point Since)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - Since).count();
	}

	int Failures = 0;
	void Check(bool bOk, const char* What, const std::string& Detail)
	{
		std::printf("  %s  %s%s%s\n", bOk ? "ok  " : "FAIL", What, Detail.empty() ? "" : ": ", Detail.c_str());
		if (!bOk) ++Failures;
	}
	std::string Fmt(const char* F, double A = 0.0, double B = 0.0, double C = 0.0, double D = 0.0)
	{
		char Buf[256];
		std::snprintf(Buf, sizeof(Buf), F, A, B, C, D);
		return Buf;
	}

	const char* const BaseNames[] = { "ShrubA", "ShrubB", "ShrubC", "Bramble", "Oak", "Elm", "Ash", "Beech", "Pollard", "Apple", "Gate", "Well", "Cart", "Tuft", "TallGrass", "Wheat", "Poppies", "Flowers", "Stone", "Fern" };
	const char* const DrawnNames[] = { "pasture", "crop", "woods", "rough", "works", "village", "yard", "lane", "bank", "floor", "wall", "water", "marsh", "beyond" };

	/** The highest triangle of a mesh over a place, seen from above (NaN where none). */
	double MeshTop(const LandField::Mesh& M, double X, double Y)
	{
		double Best = std::nan("");
		for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
		{
			const float* A = &M.Positions[M.Triangles[T] * 3];
			const float* B = &M.Positions[M.Triangles[T + 1] * 3];
			const float* C = &M.Positions[M.Triangles[T + 2] * 3];
			const double Den = (B[1] - C[1]) * (A[0] - C[0]) + (C[0] - B[0]) * (A[1] - C[1]);
			if (std::fabs(Den) < 1e-9) continue;
			const double L1 = ((B[1] - C[1]) * (X - C[0]) + (C[0] - B[0]) * (Y - C[1])) / Den;
			const double L2 = ((C[1] - A[1]) * (X - C[0]) + (A[0] - C[0]) * (Y - C[1])) / Den;
			const double L3 = 1.0 - L1 - L2;
			if (L1 < -1e-7 || L2 < -1e-7 || L3 < -1e-7) continue;
			const double Z = L1 * A[2] + L2 * B[2] + L3 * C[2];
			if (std::isnan(Best) || Z > Best) Best = Z;
		}
		return Best;
	}

	double Pctl(std::vector<double> V, double Q)
	{
		if (V.empty()) return 0.0;
		std::sort(V.begin(), V.end());
		return V[std::min(V.size() - 1, static_cast<size_t>(Q * static_cast<double>(V.size())))];
	}

	/**
	 * The bocage as a man in it meets it: the banks' height and foot across the hedge lines, the hedge's top over the
	 * field (bank and shrubs) and where it is low enough to see through at eye height, the hedgerow trees' spacing and
	 * height, and sight lines from the middles of fields at a man's eye (1.7 m) in 16 directions, each stopped by the
	 * ground, a bank or a shrub's crown (the shrubs taken as solid from the ground to their top within 0.8 of their
	 * radius). Returns the sight lines' median, and the share of them stopped by the field's own hedge.
	 */
	double Bocage(const LandField::Ground& G, const LandField::Field& F, double& OwnShare)
	{
		using LandField::Base;
		// The shrubs filed by 4 m square: a crown's middle, its reach and its top.
		struct Crown
		{
			double X, Y, R, Top;
		};
		constexpr double Sq4 = 4.0;
		const int CX = static_cast<int>(std::ceil((F.Width() + 600.0) / Sq4)), CY = static_cast<int>(std::ceil((F.Depth() + 600.0) / Sq4));
		std::vector<std::vector<Crown>> Grid(static_cast<size_t>(CX) * CY);
		auto Cell = [&](double X, double Y) -> std::vector<Crown>* {
			const int I = static_cast<int>(std::floor((X + 300.0) / Sq4)), J = static_cast<int>(std::floor((Y + 300.0) / Sq4));
			if (I < 0 || J < 0 || I >= CX || J >= CY) return nullptr;
			return &Grid[static_cast<size_t>(J) * CX + I];
		};
		for (const Base B : { Base::ShrubA, Base::ShrubB, Base::ShrubC, Base::Bramble })
		{
			const LandField::BaseMesh& Bm = G.BaseOf(B);
			for (const LandField::Placed& P : G.Instances(B))
				if (std::vector<Crown>* C = Cell(P.X, P.Y)) C->push_back({ P.X, P.Y, 0.8 * Bm.Radius * P.Scale, P.Z + Bm.Height * P.ScaleZ });
		}
		// The top of what stands at a place: the ground drawn (banks and all), or a shrub's crown over it.
		auto Wall = [&](double X, double Y) {
			double Z = G.At(X, Y).Z;
			for (int DJ = -1; DJ <= 1; ++DJ)
				for (int DI = -1; DI <= 1; ++DI)
					if (const std::vector<Crown>* C = Cell(X + DI * Sq4, Y + DJ * Sq4))
						for (const Crown& Cr : *C)
							if (std::hypot(X - Cr.X, Y - Cr.Y) < Cr.R) Z = std::max(Z, Cr.Top);
			return Z;
		};
		auto In = [&F](double X, double Y) { return X > 20.0 && Y > 20.0 && X < F.Width() - 20.0 && Y < F.Depth() - 20.0; };
		// Along the hedgerows and lane banks, every metre: the bank's rise over the field beside it, its foot, the top.
		std::vector<double> BankH, BankFoot, TopH, GapLen;
		double Metres = 0.0, Low = 0.0;
		int Gaps = 0, WayGaps = 0;
		// (a gap where a recorded man or vehicle crosses is one the rules ask for)
		auto OnWay = [&F](double X, double Y) {
			for (const LandField::Field::Way& W : F.Ways)
				for (size_t K = 0; K + 1 < W.Points.size(); ++K)
				{
					const double AX = W.Points[K].first, AY = W.Points[K].second, DX = W.Points[K + 1].first - AX, DY = W.Points[K + 1].second - AY, LL = DX * DX + DY * DY;
					const double U = LL > 0.0 ? std::clamp(((X - AX) * DX + (Y - AY) * DY) / LL, 0.0, 1.0) : 0.0;
					if (std::hypot(X - AX - DX * U, Y - AY - DY * U) < W.HalfWidth + 3.0) return true;
				}
			return false;
		};
		for (const LandField::HedgeLine& H : G.Hedges())
		{
			if (H.Kind > 1) continue;
			double Run = 0.0, Since = 0.0, RunX = 0.0, RunY = 0.0;
			for (size_t K = 0; K + 1 < H.Points.size(); ++K)
			{
				const double AX = H.Points[K].first, AY = H.Points[K].second, BX = H.Points[K + 1].first, BY = H.Points[K + 1].second;
				const double L = std::hypot(BX - AX, BY - AY);
				if (L < 1e-6) continue;
				const double NX = -(BY - AY) / L, NY = (BX - AX) / L;
				for (double S = 0.0; S < L; S += 1.0)
				{
					const double X = AX + (BX - AX) * S / L, Y = AY + (BY - AY) * S / L;
					if (!In(X, Y)) continue;
					// (the field beside it: the higher side, 5 m out, so a lane's bank is measured from its field)
					const double Field = std::max(G.At(X + NX * 5.0, Y + NY * 5.0).Z, G.At(X - NX * 5.0, Y - NY * 5.0).Z);
					const double Top = Wall(X, Y) - Field;
					Metres += 1.0;
					Since += 1.0;
					TopH.push_back(Top);
					if (Since >= 5.0)
					{
						Since = 0.0;
						BankH.push_back(G.At(X, Y).Z - Field);
						double Foot = 0.0;
						for (int Side = -1; Side <= 1; Side += 2)
							for (double D = 0.25; D < 4.0; D += 0.25)
								if (G.At(X + NX * D * Side, Y + NY * D * Side).Z - Field < 0.1)
								{
									Foot += D;
									break;
								}
						BankFoot.push_back(Foot);
					}
					if (Top < 1.9)
					{
						Low += 1.0;
						if (Run == 0.0)
						{
							RunX = X;
							RunY = Y;
						}
						Run += 1.0;
					}
					else if (Run > 0.0)
					{
						if (Run >= 1.0)
						{
							++Gaps;
							GapLen.push_back(Run);
							if (OnWay(0.5 * (RunX + X), 0.5 * (RunY + Y))) ++WayGaps;
						}
						Run = 0.0;
					}
				}
			}
		}
		std::printf("  bocage, along %.1f km of hedgerows and lane banks in the field:\n", Metres / 1000.0);
		std::printf("    bank over the field: median %.2f m (10%% %.2f, 90%% %.2f); its foot across %.1f m (10%% %.1f, 90%% %.1f)\n", Pctl(BankH, 0.5), Pctl(BankH, 0.1), Pctl(BankH, 0.9), Pctl(BankFoot, 0.5), Pctl(BankFoot, 0.1),
		            Pctl(BankFoot, 0.9));
		std::printf("    hedge's top over the field (bank and shrubs): median %.2f m (10%% %.2f, 90%% %.2f); under 1.9 m (seen over or through at a man's eye) %.1f%% of its length, %d gaps (%d of them where the recording's men or vehicles cross), median %.0f m, longest %.0f m\n",
		            Pctl(TopH, 0.5), Pctl(TopH, 0.1), Pctl(TopH, 0.9), 100.0 * Low / std::max(1.0, Metres), Gaps, WayGaps, Pctl(GapLen, 0.5), GapLen.empty() ? 0.0 : Pctl(GapLen, 1.0));
		// The gateways: from the hedge's end at each gate's hinge to the next hedge's end across the gap, how wide it is
		// open (bank and shrubs under a metre over the gateway's ground).
		{
			std::vector<std::pair<double, double>> Ends;
			std::vector<size_t> EndLine;
			for (size_t L = 0; L < G.Hedges().size(); ++L)
			{
				const LandField::HedgeLine& H = G.Hedges()[L];
				if (H.Kind > 1 || H.Points.size() < 2) continue;
				for (const auto& E : { H.Points.front(), H.Points.back() })
				{
					Ends.push_back(E);
					EndLine.push_back(L);
				}
			}
			std::vector<double> Open;
			auto Lp = [](double A, double B, double T) { return A + (B - A) * T; };
			size_t InField = 0;
			for (const LandField::Placed& P : G.Instances(Base::Gate))
			{
				if (!In(P.X, P.Y)) continue;
				++InField;
				size_t A = Ends.size();
				double DA = 3.0;
				for (size_t K = 0; K < Ends.size(); ++K)
				{
					const double D = std::hypot(Ends[K].first - P.X, Ends[K].second - P.Y);
					if (D < DA)
					{
						DA = D;
						A = K;
					}
				}
				if (A == Ends.size()) continue;
				size_t B = Ends.size();
				double DB = 9.0;
				for (size_t K = 0; K < Ends.size(); ++K)
				{
					if (EndLine[K] == EndLine[A]) continue;
					const double D = std::hypot(Ends[K].first - Ends[A].first, Ends[K].second - Ends[A].second);
					if (D < DB)
					{
						DB = D;
						B = K;
					}
				}
				if (B == Ends.size()) continue;
				const double AX = Ends[A].first, AY = Ends[A].second, BX = Ends[B].first, BY = Ends[B].second;
				double Floor = 1e9;
				for (double T = 0.0; T <= 1.0; T += 0.05) Floor = std::min(Floor, G.At(Lp(AX, BX, T), Lp(AY, BY, T)).Z);
				double Best = 0.0, Cur = 0.0;
				const double Step = 0.25 / std::max(0.25, DB);
				for (double T = 0.0; T <= 1.0; T += Step)
				{
					if (Wall(Lp(AX, BX, T), Lp(AY, BY, T)) < Floor + 1.0) Cur += 0.25;
					else Cur = 0.0;
					Best = std::max(Best, Cur);
				}
				Open.push_back(Best);
			}
			std::printf("    gateways: %zu gates in the field, %zu measured across their gap: open %.1f m median (10%% %.1f, 90%% %.1f)\n", InField, Open.size(), Pctl(Open, 0.5), Pctl(Open, 0.1), Pctl(Open, 0.9));
		}
		// The hedgerow trees: within 2.5 m of a hedge line, their spacing along it and their height over the ground.
		{
			std::vector<double> Spacing, High;
			int Trees = 0, Pollards = 0;
			std::vector<std::pair<double, double>> Pts;
			std::vector<std::pair<double, int>> Along;
			std::vector<std::vector<std::pair<double, double>>> TreeGrid(static_cast<size_t>(CX) * CY);
			for (const Base B : { Base::Oak, Base::Elm, Base::Ash, Base::Beech, Base::Pollard })
			{
				const LandField::BaseMesh& Bm = G.BaseOf(B);
				for (const LandField::Placed& P : G.Instances(B))
				{
					const int I = static_cast<int>(std::floor((P.X + 300.0) / Sq4)), J = static_cast<int>(std::floor((P.Y + 300.0) / Sq4));
					if (I < 0 || J < 0 || I >= CX || J >= CY) continue;
					TreeGrid[static_cast<size_t>(J) * CX + I].push_back({ P.X, P.Y });
					if (!In(P.X, P.Y)) continue;
					// (a hedge's tree stands on or by its bank)
					const double Field = std::min({ G.At(P.X + 5.0, P.Y).Z, G.At(P.X - 5.0, P.Y).Z, G.At(P.X, P.Y + 5.0).Z, G.At(P.X, P.Y - 5.0).Z });
					if (P.Z - Field < 0.3) continue;
					++Trees;
					if (B == Base::Pollard) ++Pollards;
					High.push_back(P.Z + Bm.Height * P.ScaleZ - Field);
				}
			}
			for (const LandField::HedgeLine& H : G.Hedges())
			{
				if (H.Kind > 1) continue;
				double S0 = 0.0;
				std::vector<double> At;
				for (size_t K = 0; K + 1 < H.Points.size(); ++K)
				{
					const double AX = H.Points[K].first, AY = H.Points[K].second, BX = H.Points[K + 1].first, BY = H.Points[K + 1].second;
					const double L = std::hypot(BX - AX, BY - AY);
					if (L < 1e-6) continue;
					const int I = static_cast<int>(std::floor((0.5 * (AX + BX) + 300.0) / Sq4)), J = static_cast<int>(std::floor((0.5 * (AY + BY) + 300.0) / Sq4));
					for (int DJ = -2; DJ <= 2; ++DJ)
						for (int DI = -2; DI <= 2; ++DI)
						{
							if (I + DI < 0 || J + DJ < 0 || I + DI >= CX || J + DJ >= CY) continue;
							for (const auto& T : TreeGrid[static_cast<size_t>(J + DJ) * CX + I + DI])
							{
								const double U = ((T.first - AX) * (BX - AX) + (T.second - AY) * (BY - AY)) / (L * L);
								if (U < 0.0 || U >= 1.0) continue;
								const double D = std::fabs((T.first - AX) * (BY - AY) - (T.second - AY) * (BX - AX)) / L;
								if (D < 2.5) At.push_back(S0 + U * L);
							}
						}
					S0 += L;
				}
				if (!In(H.Points[H.Points.size() / 2].first, H.Points[H.Points.size() / 2].second) || S0 < 40.0) continue;
				std::sort(At.begin(), At.end());
				At.erase(std::unique(At.begin(), At.end(), [](double A, double B) { return std::fabs(A - B) < 0.5; }), At.end());
				for (size_t K = 1; K < At.size(); ++K) Spacing.push_back(At[K] - At[K - 1]);
				if (At.empty()) Spacing.push_back(S0);
			}
			std::printf("    hedgerow trees: %d on banks in the field (%d pollards), %.1f m to the top over the field median (10%% %.1f, 90%% %.1f); spaced along their hedge median %.1f m (25%% %.1f, 75%% %.1f, 90%% %.1f)\n", Trees, Pollards,
			            Pctl(High, 0.5), Pctl(High, 0.1), Pctl(High, 0.9), Pctl(Spacing, 0.5), Pctl(Spacing, 0.25), Pctl(Spacing, 0.75), Pctl(Spacing, 0.9));
		}
		// Sight lines at a man's eye from the middles of fields.
		std::vector<double> Sight, Across;
		int Fields = 0, Own = 0;
		{
			std::map<int, std::pair<std::pair<double, double>, int>> Mid;
			for (double Y = 25.0; Y < F.Depth() - 25.0; Y += 10.0)
				for (double X = 25.0; X < F.Width() - 25.0; X += 10.0)
				{
					const int P = G.ParcelAt(X, Y);
					if (P < 0) continue;
					auto& M = Mid[P];
					M.first.first += X;
					M.first.second += Y;
					++M.second;
				}
			int Index = 0;
			for (const auto& [P, M] : Mid)
			{
				if (M.second < 60) continue; // 6000 square metres and more
				if (Index++ % 6 != 0) continue;
				const double X = M.first.first / M.second, Y = M.first.second / M.second;
				const LandField::Surface Su = G.At(X, Y);
				if (G.ParcelAt(X, Y) != P || (Su.What != LandField::Drawn::Pasture && Su.What != LandField::Drawn::Crop)) continue;
				++Fields;
				const double Eye = Su.Z + 1.7;
				for (int K = 0; K < 16; ++K)
				{
					const double A = K * 2.0 * 3.14159265358979 / 16.0;
					double D = 1.0, Leaves = 1e9;
					for (; D < 600.0; D += 1.0)
					{
						const double QX = X + std::cos(A) * D, QY = Y + std::sin(A) * D;
						if (Leaves > 1e8 && G.ParcelAt(QX, QY) != P) Leaves = D;
						if (Wall(QX, QY) > Eye) break;
					}
					Sight.push_back(D);
					if (D <= Leaves + 8.0) ++Own;
				}
				// (and across the field, from its hedge on one side to its hedge on the other: what a man at its edge sees)
				for (int K = 0; K < 8; ++K) Across.push_back(Sight[Sight.size() - 16 + static_cast<size_t>(K)] + Sight[Sight.size() - 8 + static_cast<size_t>(K)]);
			}
		}
		const double Rays = std::max<double>(1.0, static_cast<double>(Sight.size()));
		std::printf("    sight at a man's eye (1.7 m) from %d field middles, 16 ways each: median %.0f m (10%% %.0f, 25%% %.0f, 75%% %.0f, 90%% %.0f), %.0f%% past 250 m; %.0f%% stopped by the field's own hedge\n", Fields, Pctl(Sight, 0.5), Pctl(Sight, 0.1),
		            Pctl(Sight, 0.25), Pctl(Sight, 0.75), Pctl(Sight, 0.9), 100.0 * static_cast<double>(std::count_if(Sight.begin(), Sight.end(), [](double D) { return D > 250.0; })) / Rays, 100.0 * Own / Rays);
		std::printf("    across those fields, hedge to hedge: median %.0f m (10%% %.0f, 25%% %.0f, 75%% %.0f, 90%% %.0f)\n", Pctl(Across, 0.5), Pctl(Across, 0.1), Pctl(Across, 0.25), Pctl(Across, 0.75), Pctl(Across, 0.9));
		OwnShare = Own / Rays;
		return Pctl(Sight, 0.5);
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: land_field_test <recording.json> [picture folder]\n");
		return 2;
	}
	std::ifstream In(Argv[1], std::ios::binary);
	std::stringstream Buf;
	Buf << In.rdbuf();
	const std::string Text = Buf.str();
	Json Rec;
	Reader Rd{ Text };
	if (Text.empty() || !Rd.Value(Rec) || Rec.K != Json::Object)
	{
		std::fprintf(stderr, "cannot read %s\n", Argv[1]);
		return 2;
	}
	const Json* Fj = Rec.Get("field");
	if (!Fj)
	{
		std::fprintf(stderr, "%s has no field\n", Argv[1]);
		return 2;
	}
	LandField::Field F;
	F.Cols = static_cast<int>(Fj->Num("cols", 0));
	F.Rows = static_cast<int>(Fj->Num("rows", 0));
	F.Cell = Fj->Num("cell", 25.0);
	if (const Json* G = Fj->Get("grounds"))
		for (const Json& S : G->A) F.Grounds.push_back(S.S);
	if (const Json* G = Fj->Get("ground"))
		for (const Json& N : G->A) F.Ground.push_back(static_cast<int>(N.N));
	if (const Json* G = Fj->Get("height"))
		for (const Json& N : G->A) F.Height.push_back(N.N);
	if (const Json* G = Fj->Get("storeys"))
		for (const Json& N : G->A) F.Storeys.push_back(static_cast<int>(N.N));
	if (const Json* T = Fj->Get("town"); T && T->K == Json::Object)
	{
		F.bTown = true;
		F.TownX = T->Num("x", 0.0);
		F.TownY = T->Num("y", 0.0);
		F.TownR = T->Num("r", 150.0);
	}
	if (const Json* P = Fj->Get("place"))
		if (const Json* T = P->Get("terrain"); T && T->K == Json::String) F.Terrain = T->S;
	if (const Json* D = Rec.Get("date"); D && D->K == Json::String)
	{
		static const char* const Months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
		for (int M = 0; M < 12; ++M)
			if (D->S.find(Months[M]) != std::string::npos) F.Month = M + 1;
	}
	F.Seed = static_cast<uint64_t>(Rec.Num("seed", 1.0));
	std::string Key = "battle";
	if (const Json* K = Rec.Get("key"); K && K->K == Json::String) Key = K->S;
	int Vehicles = 0, Men = 0;
	if (const Json* Ms = Rec.Get("men"))
		for (const Json& M : Ms->A)
		{
			const Json* R = M.Get("r");
			const std::string Role = R ? R->S : "";
			LandField::Field::Way W;
			W.HalfWidth = Role == "tank" ? 2.0 : (Role == "gun" || Role == "lorry") ? 1.6 : 0.5;
			if (const Json* At = M.Get("at"))
				for (const Json& P : At->A)
				{
					if (P.A.size() < 3) continue;
					const double X = P.A[1].N, Y = P.A[2].N;
					if (!W.Points.empty() && std::hypot(X - W.Points.back().first, Y - W.Points.back().second) < 0.75) continue;
					W.Points.push_back({ X, Y });
				}
			if (W.Points.size() < 2) continue;
			(W.HalfWidth >= 1.2 ? Vehicles : Men) += 1;
			F.Ways.push_back(std::move(W));
		}
	std::printf("%s: field %d x %d cells of %.0f m (%.0f x %.0f m), %zu ways (%d vehicles, %d men), town %s\n", Key.c_str(), F.Cols, F.Rows, F.Cell, F.Width(), F.Depth(), F.Ways.size(), Vehicles, Men,
	            F.bTown ? Fmt("at %.0f, %.0f r %.0f m", F.TownX, F.TownY, F.TownR).c_str() : "none");

	const auto Start = std::chrono::steady_clock::now();
	const LandField::Ground G(F);
	const double Build = Seconds(Start);
	const LandField::Tally& T = G.Laid();
	std::printf("built in %.2f s (core %.2f s: heights, lanes, parcels, hedges, village, instances; meshes %.2f s: tiles, ring, bases)\n", Build, T.CoreSeconds, T.MeshSeconds);
	std::printf("  %d parcels in the field and its band, mean %.0f m across; %d lanes, %.1f km; %.1f km of banks and walls; %d gates; %d hedges opened by the recording's vehicles\n", T.Parcels, T.ParcelMean, T.Lanes,
	            T.LaneKm, T.HedgeKm, T.Gates, T.WayGaps);
	std::printf("  village: %d houses, barns and byres (%d damaged), church %s, %d farmsteads\n", T.Houses, T.Damaged, T.bChurch ? "yes" : "no", T.Farmsteads);
	std::printf("  triangles: near %zu, far %zu, built %zu, ring %zu; instances %zu\n", T.NearTris, T.FarTris, T.BuiltTris, T.RingTris, T.Instances);
	size_t NearMax = 0, FarMax = 0, BuiltMax = 0, NearSum = 0, FarSum = 0;
	for (const LandField::Tile& Tl : G.Tiles())
	{
		NearMax = std::max(NearMax, Tl.Near.Tris());
		FarMax = std::max(FarMax, Tl.Far.Tris());
		BuiltMax = std::max(BuiltMax, Tl.Built.Tris());
		NearSum += Tl.Near.Tris();
		FarSum += Tl.Far.Tris();
	}
	const size_t NT = std::max<size_t>(1, G.Tiles().size());
	std::printf("  per tile (%zu tiles of 250 m): near max %zu mean %zu, far max %zu mean %zu, built max %zu\n", G.Tiles().size(), NearMax, NearSum / NT, FarMax, FarSum / NT, BuiltMax);
	size_t RingMax = 0;
	for (const LandField::Tile& Tl : G.Ring()) RingMax = std::max(RingMax, Tl.Near.Tris());
	std::printf("  ring: %zu tiles, max %zu triangles\n", G.Ring().size(), RingMax);
	std::printf("  instances and base meshes (near / far triangles, far from):\n");
	for (int B = 0; B < static_cast<int>(LandField::Base::Count); ++B)
	{
		const LandField::BaseMesh& Bm = G.BaseOf(static_cast<LandField::Base>(B));
		std::printf("    %-9s %6zu   %5zu / %4zu tris, far from %3.0f m, %.1f m high, %.1f m round\n", BaseNames[B], G.Instances(static_cast<LandField::Base>(B)).size(), Bm.Near.Tris(), Bm.Far.Tris(), Bm.FarFrom,
		            Bm.Height, Bm.Radius);
	}

	// The clutter: what a soldier's eye gets round it, and that boxes that overlap agree.
	{
		const double CX = F.bTown ? F.TownX : F.Width() / 2.0, CY = F.bTown ? F.TownY - 500.0 : F.Depth() / 2.0;
		for (const double Half : { 40.0, 60.0 })
		{
			std::vector<LandField::Placed> Out;
			const auto C0 = std::chrono::steady_clock::now();
			G.Clutter(CX - Half, CY - Half, CX + Half, CY + Half, Out);
			std::map<int, int> ByKind;
			for (const LandField::Placed& P : Out) ++ByKind[static_cast<int>(P.Kind)];
			std::string Kinds;
			for (const auto& [K, N] : ByKind) Kinds += std::string(" ") + BaseNames[K] + " " + std::to_string(N);
			std::printf("  clutter in %.0f m square round %.0f, %.0f: %zu things in %.1f ms (%s )\n", 2.0 * Half, CX, CY, Out.size(), Seconds(C0) * 1000.0, Kinds.c_str());
		}
		std::vector<LandField::Placed> A, B;
		G.Clutter(CX - 30.0, CY - 30.0, CX + 10.0, CY + 10.0, A);
		G.Clutter(CX - 10.0, CY - 10.0, CX + 30.0, CY + 30.0, B);
		auto Inside = [&](const LandField::Placed& P) { return P.X >= CX - 10.0 && P.X < CX + 10.0 && P.Y >= CY - 10.0 && P.Y < CY + 10.0; };
		std::vector<std::pair<float, float>> PA, PB;
		for (const LandField::Placed& P : A)
			if (Inside(P)) PA.push_back({ P.X, P.Y });
		for (const LandField::Placed& P : B)
			if (Inside(P)) PB.push_back({ P.X, P.Y });
		std::sort(PA.begin(), PA.end());
		std::sort(PB.begin(), PB.end());
		std::printf("checks\n");
		Check(PA == PB && !PA.empty(), "clutter boxes that overlap lay the same things", Fmt("%.0f and %.0f in the overlap", static_cast<double>(PA.size()), static_cast<double>(PB.size())));
	}

	Check(Build < 2.0, "built in under two seconds", Fmt("%.2f s", Build));
	Check(T.NearTris + T.BuiltTris < 1500000, "near level (ground, lanes, banks) and buildings under 1.5 million triangles", Fmt("%.0f + %.0f", static_cast<double>(T.NearTris), static_cast<double>(T.BuiltTris)));
	Check(T.ParcelMean >= 80.0 && T.ParcelMean <= 200.0, "parcels 80 to 200 m across", Fmt("mean %.0f m", T.ParcelMean));
	Check(T.Gates > T.Parcels / 3, "a gate in most parcels", Fmt("%.0f gates, %.0f parcels", T.Gates, T.Parcels));
	Check(!F.bTown || (T.bChurch && T.Houses >= 10), "the village has its church and houses", Fmt("%.0f houses", T.Houses));

	// What is drawn against what At() says: the highest triangle of the tile's near level over random places.
	{
		uint64_t S = 12345;
		auto Next = [&S]() {
			S = S * 6364136223846793005ull + 1442695040888963407ull;
			return static_cast<double>(S >> 11) / 9007199254740992.0;
		};
		// (a lane's or a bank's strip near a tile's edge may be filed with the tile next door: every tile near is asked)
		auto DrawnTop = [&G](double X, double Y) {
			double Z = std::nan("");
			for (const LandField::Tile& C : G.Tiles())
				if (X >= C.X0 - 4.0 && X < C.X1 + 4.0 && Y >= C.Y0 - 4.0 && Y < C.Y1 + 4.0)
				{
					const double Q = MeshTop(C.Near, X, Y);
					if (!std::isnan(Q) && (std::isnan(Z) || Q > Z)) Z = Q;
				}
			return Z;
		};
		std::vector<double> Diff;
		std::map<int, std::pair<int, double>> ByDrawn;
		int Missed = 0;
		for (int K = 0; K < 600; ++K)
		{
			const double X = Next() * F.Width(), Y = Next() * F.Depth();
			const LandField::Surface Su = G.At(X, Y);
			if (Su.What == LandField::Drawn::Floor) continue; // inside a house: its floor is in Built
			const double Z = DrawnTop(X, Y);
			if (std::isnan(Z))
			{
				++Missed;
				continue;
			}
			const double D = std::fabs(Z - Su.Z);
			Diff.push_back(D);
			auto& E = ByDrawn[static_cast<int>(Su.What)];
			++E.first;
			E.second = std::max(E.second, D);
		}
		// And along the banks and lanes, where it matters most (a man stands on the bank, a tank in the lane).
		int Banks = 0, OnBank = 0;
		std::vector<double> BankDiff;
		for (const LandField::HedgeLine& H : G.Hedges())
		{
			if (H.Kind > 2) continue;
			for (size_t K = 0; K < H.Points.size(); K += 3)
			{
				const double X = H.Points[K].first, Y = H.Points[K].second;
				if (X < 0.0 || Y < 0.0 || X > F.Width() || Y > F.Depth()) continue;
				++Banks;
				const LandField::Surface Su = G.At(X, Y);
				if (Su.What == LandField::Drawn::Bank) ++OnBank;
				if (Banks % 5 != 0) continue;
				const double Z = DrawnTop(X, Y);
				if (!std::isnan(Z)) BankDiff.push_back(std::fabs(Z - Su.Z));
			}
		}
		int LanePts = 0, OnLane = 0;
		double Sunk = 0.0;
		int SunkN = 0;
		for (const auto& L : G.Lanes())
			for (size_t K = 2; K + 2 < L.size(); K += 5)
			{
				const double X = L[K].first, Y = L[K].second;
				if (X < 0.0 || Y < 0.0 || X > F.Width() || Y > F.Depth()) continue;
				++LanePts;
				const LandField::Surface Su = G.At(X, Y);
				if (Su.What == LandField::Drawn::Lane) ++OnLane;
				if (F.bTown && std::hypot(X - F.TownX, Y - F.TownY) < F.TownR * 1.3) continue;
				const double DX = L[K + 1].first - L[K - 1].first, DY = L[K + 1].second - L[K - 1].second, DL = std::hypot(DX, DY);
				if (DL < 1e-6) continue;
				double Field = -1e9;
				for (int Side = -1; Side <= 1; Side += 2)
				{
					const LandField::Surface O = G.At(X - DY / DL * Side * 9.0, Y + DX / DL * Side * 9.0);
					if (O.What == LandField::Drawn::Crop || O.What == LandField::Drawn::Pasture) Field = std::max(Field, O.Z);
				}
				if (Field > -1e8)
				{
					Sunk += Field - Su.Z;
					++SunkN;
				}
			}
		std::sort(Diff.begin(), Diff.end());
		std::sort(BankDiff.begin(), BankDiff.end());
		auto Pct = [](const std::vector<double>& V, double Q) { return V.empty() ? 0.0 : V[std::min(V.size() - 1, static_cast<size_t>(Q * V.size()))]; };
		Check(!Diff.empty() && Pct(Diff, 0.5) < 0.03 && Pct(Diff, 0.95) < 0.12 && Missed == 0, "At() stands on the drawn ground (600 random places)",
		      Fmt("median %.3f m, 95%% %.3f m, worst %.3f m", Pct(Diff, 0.5), Pct(Diff, 0.95), Diff.empty() ? 0.0 : Diff.back()) + Fmt(", %.0f with no ground", Missed));
		std::string Kinds;
		for (const auto& [K, E] : ByDrawn) Kinds += std::string(" ") + DrawnNames[K] + " " + std::to_string(E.first) + Fmt(" (worst %.2f)", E.second);
		std::printf("        by what is drawn:%s\n", Kinds.c_str());
		Check(!BankDiff.empty() && Pct(BankDiff, 0.5) < 0.03 && Pct(BankDiff, 0.95) < 0.12, "At() stands on the drawn banks",
		      Fmt("%.0f places, median %.3f m, 95%% %.3f m, worst %.3f m", static_cast<double>(BankDiff.size()), Pct(BankDiff, 0.5), Pct(BankDiff, 0.95), BankDiff.empty() ? 0.0 : BankDiff.back()));
		Check(Banks > 0 && OnBank > Banks * 0.9, "a hedge's line is drawn as its bank", Fmt("%.0f of %.0f", OnBank, Banks));
		Check(LanePts > 0 && OnLane > LanePts * 0.97, "a lane's middle is drawn as lane", Fmt("%.0f of %.0f", OnLane, LanePts));
		Check(SunkN > 0 && Sunk / SunkN > 0.5 && Sunk / SunkN < 2.2, "lanes sunk below the fields beside them (outside the village)", Fmt("mean %.2f m", SunkN ? Sunk / SunkN : 0.0));
	}
	// Nothing standing in a house, and each thing standing on the ground drawn; no tree trunk on a vehicle's way.
	{
		int Bad = 0, Off = 0, All = 0;
		for (int B = 0; B <= static_cast<int>(LandField::Base::Cart); ++B)
			for (const LandField::Placed& P : G.Instances(static_cast<LandField::Base>(B)))
			{
				++All;
				const LandField::Surface Su = G.At(P.X, P.Y);
				if (Su.What == LandField::Drawn::Floor) ++Bad;
				if (std::fabs(Su.Z - P.Z) > 0.02) ++Off;
			}
		Check(Bad == 0, "no shrub, tree, gate or prop in a house", Fmt("%.0f of %.0f", Bad, All));
		Check(Off == 0, "each stands where At() puts the ground", Fmt("%.0f of %.0f off by more than 2 cm", Off, All));
		int OnWay = 0;
		for (const LandField::Field::Way& W : F.Ways)
		{
			if (W.HalfWidth < 1.2) continue;
			for (int B = static_cast<int>(LandField::Base::Oak); B <= static_cast<int>(LandField::Base::Apple); ++B)
				for (const LandField::Placed& P : G.Instances(static_cast<LandField::Base>(B)))
					for (size_t K = 0; K + 1 < W.Points.size(); K += 1)
					{
						const double AX = W.Points[K].first, AY = W.Points[K].second, BX = W.Points[K + 1].first, BY = W.Points[K + 1].second;
						const double DX = BX - AX, DY = BY - AY, LL = DX * DX + DY * DY;
						const double U = LL > 0 ? std::clamp(((P.X - AX) * DX + (P.Y - AY) * DY) / LL, 0.0, 1.0) : 0.0;
						if (std::hypot(P.X - (AX + DX * U), P.Y - (AY + DY * U)) < W.HalfWidth) ++OnWay;
					}
		}
		Check(OnWay == 0, "no tree on a vehicle's way", Fmt("%.0f", OnWay));
	}
	// The bocage at a man's eye: a green wall round each field, the view closed at 100 to 200 m.
	{
		double Own = 0.0;
		const double Median = Bocage(G, F, Own);
		Check(Median >= 40.0 && Median <= 200.0 && Own >= 0.85, "the bocage closes a field's view at a man's eye", Fmt("median sight line %.0f m, %.0f%% stopped by the field's own hedge", Median, 100.0 * Own));
	}
	// The effects kit's words, as a shell would read them.
	{
		std::map<std::string, int> Words;
		for (int J = 0; J < 60; ++J)
			for (int I = 0; I < 90; ++I) ++Words[G.At((I + 0.5) * F.Width() / 90.0, (J + 0.5) * F.Depth() / 60.0).Fx];
		std::string Ws;
		for (const auto& [W, N] : Words) Ws += " " + W + " " + std::to_string(N);
		std::printf("  ground words over the field (90 x 60 places):%s\n", Ws.c_str());
		const auto A0 = std::chrono::steady_clock::now();
		double Sum = 0.0;
		for (int K = 0; K < 20000; ++K) Sum += G.At(100.0 + K * 0.137, 300.0 + K * 0.071).Z;
		std::printf("  At(): %.2f microseconds a call (%.1f)\n", Seconds(A0) / 20000.0 * 1e6, Sum / 20000.0);
	}

	if (Argc >= 3)
	{
		const auto P0 = std::chrono::steady_clock::now();
		LandFieldPicture::Sky Sk;
		if (const Json* S = Rec.Get("sky"))
		{
			Sk.Hour = S->Num("hour", Sk.Hour);
			Sk.Cover = S->Num("cover", Sk.Cover);
			Sk.Base = S->Num("base", Sk.Base);
			if (const Json* L = S->Get("latLon"); L && L->A.size() == 2)
			{
				Sk.Lat = L->A[0].N;
				Sk.Lon = L->A[1].N;
			}
		}
		Sk.Day = 15 + static_cast<int>((F.Month - 1) * 30.44); // the middle of the battle's month
		if (!LandFieldPicture::Paint(G, F, Sk, Key, Argv[2])) ++Failures;
		std::printf("pictures painted in %.1f s\n", Seconds(P0));
	}
	std::printf("%s\n", Failures ? "SOME CHECKS FAILED" : "all checks passed");
	return Failures ? 1 : 0;
}
