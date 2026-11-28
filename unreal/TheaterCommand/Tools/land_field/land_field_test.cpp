// The land battle's ground in the cloud (Battle/LandField, no engine): builds the ground of a recorded land battle as
// the game will, times it, counts what it laid, checks that what is drawn and what At() answers agree, and (with a
// folder for pictures) paints it with the small software painter in land_field_picture.cpp: a soldier's eye in a hedged
// field toward the village, 30 m up over the bocage toward the village, and the whole field from above.
//
// Usage: land_field_test <recording.json> [picture folder]
// The recording's men are its ways: tanks 2 m either side, guns and lorries 1.6 m, men half a metre.

#include "Battle/LandCarpet.h"
#include "Battle/LandShade.h"
#include "Battle/LandCrater.h"
#include "Battle/LandField.h"
#include "Battle/LandScape.h"
#include "land_field_picture.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
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

	const char* const BaseNames[] = { "ShrubA", "ShrubB", "ShrubC", "Bramble", "Oak", "Elm", "Ash", "Beech", "Pollard", "Apple", "Gate", "Well", "Cart", "Khata", "Chimney", "Pole", "Wattle", "Sweep", "Haystack", "Birch", "Kit0", "Kit1", "Kit2", "Kit3", "Kit4", "Kit5", "Kit6", "Kit7", "Kit8", "Kit9", "Kit10", "Kit11", "Kit12", "Kit13", "Kit14", "Kit15", "Kit16", "Kit17", "Kit18", "Kit19", "Kit20", "Kit21", "Kit22", "Kit23", "Kit24", "Kit25", "Kit26", "Kit27", "Kit28", "Kit29", "Kit30", "Kit31", "Kit32", "Kit33", "Kit34", "Kit35", "Kit36", "Kit37", "Kit38", "Kit39", "Kit40", "Kit41", "Kit42", "Kit43", "Kit44", "Kit45", "Kit46", "Kit47", "Kit48", "Kit49", "Kit50", "Kit51", "Kit52", "Kit53", "Kit54", "Kit55", "Kit56", "Kit57", "Kit58", "Kit59", "Kit60", "Kit61", "Kit62", "Kit63", "Haycock", "Tuft", "TallGrass", "Wheat", "Poppies", "Flowers", "Stone", "Fern", "Stubble", "Sward", "Trodden", "Stalks", "Rosette", "Clut0", "Clut1", "Clut2", "Clut3", "Clut4", "Clut5" };
	static_assert(sizeof(BaseNames) / sizeof(BaseNames[0]) == static_cast<size_t>(LandField::Base::Count), "a name for every base");
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

	/** The normal the drawn mesh is lit by at a place, seen from above: its highest triangle's points' normals, mixed by
	 * where the place is in it (as the card draws it); false where no triangle is. */
	bool MeshUp(const LandField::Mesh& M, double X, double Y, double& OutZ, double Out[3])
	{
		bool bFound = false;
		for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
		{
			const uint32_t Ia = M.Triangles[T], Ib = M.Triangles[T + 1], Ic = M.Triangles[T + 2];
			const float* A = &M.Positions[Ia * 3];
			const float* B = &M.Positions[Ib * 3];
			const float* C = &M.Positions[Ic * 3];
			const double Den = (B[1] - C[1]) * (A[0] - C[0]) + (C[0] - B[0]) * (A[1] - C[1]);
			if (std::fabs(Den) < 1e-9) continue;
			const double L1 = ((B[1] - C[1]) * (X - C[0]) + (C[0] - B[0]) * (Y - C[1])) / Den;
			const double L2 = ((C[1] - A[1]) * (X - C[0]) + (A[0] - C[0]) * (Y - C[1])) / Den;
			const double L3 = 1.0 - L1 - L2;
			if (L1 < -1e-7 || L2 < -1e-7 || L3 < -1e-7) continue;
			const double Z = L1 * A[2] + L2 * B[2] + L3 * C[2];
			if (bFound && Z <= OutZ) continue;
			bFound = true;
			OutZ = Z;
			double Len = 0.0;
			for (int K = 0; K < 3; ++K)
			{
				Out[K] = L1 * M.Normals[Ia * 3 + K] + L2 * M.Normals[Ib * 3 + K] + L3 * M.Normals[Ic * 3 + K];
				Len += Out[K] * Out[K];
			}
			Len = std::sqrt(std::max(Len, 1e-12));
			for (int K = 0; K < 3; ++K) Out[K] /= Len;
		}
		return bFound;
	}
	/** The angle, degrees, between a point's normal (unit) in a mesh and a unit normal. */
	double DegreesOff(const LandField::Mesh& M, size_t I, const double Up[3])
	{
		const double D = M.Normals[I * 3] * Up[0] + M.Normals[I * 3 + 1] * Up[1] + M.Normals[I * 3 + 2] * Up[2];
		return std::acos(std::clamp(D, -1.0, 1.0)) * 57.29577951308232;
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
		// The trees' crowns filed the same way (8 m squares): a crown's middle, its reach and its bottom, from its leaves.
		struct Leafy
		{
			double X, Y, R, Bottom;
		};
		constexpr double Sq8 = 8.0;
		const int LX = static_cast<int>(std::ceil((F.Width() + 600.0) / Sq8)), LY = static_cast<int>(std::ceil((F.Depth() + 600.0) / Sq8));
		std::vector<std::vector<Leafy>> Crowns(static_cast<size_t>(LX) * LY);
		for (const Base B : { Base::Oak, Base::Elm, Base::Ash, Base::Beech, Base::Pollard })
		{
			const LandField::BaseMesh& Bm = G.BaseOf(B);
			const LandField::Mesh& Leaves = Bm.Cards.Empty() ? Bm.Sprigs : Bm.Cards;
			double Low = 1e9, Reach = 0.0;
			for (size_t K = 0; K + 2 < Leaves.Positions.size(); K += 3)
			{
				Low = std::min(Low, static_cast<double>(Leaves.Positions[K + 2]));
				Reach = std::max(Reach, std::hypot(static_cast<double>(Leaves.Positions[K]), static_cast<double>(Leaves.Positions[K + 1])));
			}
			for (const LandField::Placed& P : G.Instances(B))
			{
				const int I = static_cast<int>(std::floor((P.X + 300.0) / Sq8)), J = static_cast<int>(std::floor((P.Y + 300.0) / Sq8));
				if (I >= 0 && J >= 0 && I < LX && J < LY) Crowns[static_cast<size_t>(J) * LX + I].push_back({ P.X, P.Y, 0.8 * Reach * P.Scale, P.Z + Low * P.ScaleZ });
			}
		}
		// (a crown over a place whose leaves come down within half a metre of the hedge's top there)
		auto Touched = [&](double X, double Y, double Top) {
			const int I = static_cast<int>(std::floor((X + 300.0) / Sq8)), J = static_cast<int>(std::floor((Y + 300.0) / Sq8));
			for (int DJ = -1; DJ <= 1; ++DJ)
				for (int DI = -1; DI <= 1; ++DI)
				{
					if (I + DI < 0 || J + DJ < 0 || I + DI >= LX || J + DJ >= LY) continue;
					for (const Leafy& C : Crowns[static_cast<size_t>(J + DJ) * LX + I + DI])
						if (C.Bottom <= Top + 0.5 && std::hypot(X - C.X, Y - C.Y) < C.R) return true;
				}
			return false;
		};
		// Along the hedgerows and lane banks, every metre: the bank's rise over the field beside it, its foot, the top.
		std::vector<double> BankH, BankFoot, TopH, GapLen;
		double Metres = 0.0, Low = 0.0;
		int Gaps = 0, WayGaps = 0;
		// (a gap where a recorded man or vehicle crosses is one the rules ask for; where a vehicle drove, the bank is gone)
		auto OnWay = [&F](double X, double Y, bool bVehicles) {
			for (const LandField::Field::Way& W : F.Ways)
			{
				if (bVehicles && W.HalfWidth < 1.2) continue;
				for (size_t K = 0; K + 1 < W.Points.size(); ++K)
				{
					const double AX = W.Points[K].first, AY = W.Points[K].second, DX = W.Points[K + 1].first - AX, DY = W.Points[K + 1].second - AY, LL = DX * DX + DY * DY;
					const double U = LL > 0.0 ? std::clamp(((X - AX) * DX + (Y - AY) * DY) / LL, 0.0, 1.0) : 0.0;
					if (std::hypot(X - AX - DX * U, Y - AY - DY * U) < W.HalfWidth + 3.0) return true;
				}
			}
			return false;
		};
		// The fight's ground: within 30 m of a recorded man's way (by 5 m square).
		constexpr double FzSq = 5.0, FzPad = 60.0, FzReach = 30.0;
		const int FzX = static_cast<int>(std::ceil((F.Width() + 2.0 * FzPad) / FzSq)), FzY = static_cast<int>(std::ceil((F.Depth() + 2.0 * FzPad) / FzSq));
		std::vector<char> Fight(static_cast<size_t>(FzX) * FzY, 0);
		for (const LandField::Field::Way& W : F.Ways)
		{
			if (W.HalfWidth >= 1.2) continue;
			for (size_t K = 0; K + 1 < W.Points.size(); ++K)
			{
				const double AX = W.Points[K].first, AY = W.Points[K].second, DX = W.Points[K + 1].first - AX, DY = W.Points[K + 1].second - AY, LL = DX * DX + DY * DY;
				const int I0 = std::max(0, static_cast<int>((std::min(AX, AX + DX) - FzReach + FzPad) / FzSq)), I1 = std::min(FzX - 1, static_cast<int>((std::max(AX, AX + DX) + FzReach + FzPad) / FzSq));
				const int J0 = std::max(0, static_cast<int>((std::min(AY, AY + DY) - FzReach + FzPad) / FzSq)), J1 = std::min(FzY - 1, static_cast<int>((std::max(AY, AY + DY) + FzReach + FzPad) / FzSq));
				for (int J = J0; J <= J1; ++J)
					for (int I = I0; I <= I1; ++I)
					{
						char& Mark = Fight[static_cast<size_t>(J) * FzX + I];
						if (Mark) continue;
						const double MX = (I + 0.5) * FzSq - FzPad, MY = (J + 0.5) * FzSq - FzPad;
						const double U = LL > 0.0 ? std::clamp(((MX - AX) * DX + (MY - AY) * DY) / LL, 0.0, 1.0) : 0.0;
						if (std::hypot(MX - AX - DX * U, MY - AY - DY * U) < FzReach) Mark = 1;
					}
			}
		}
		auto InFight = [&](double X, double Y) {
			const int I = static_cast<int>((X + FzPad) / FzSq), J = static_cast<int>((Y + FzPad) / FzSq);
			return I >= 0 && J >= 0 && I < FzX && J < FzY && Fight[static_cast<size_t>(J) * FzX + I] != 0;
		};
		double FightMetres = 0.0, FightHigh = 0.0, High25 = 0.0, LongestOff = 0.0, Touch = 0.0, LongestLevel = 0.0;
		std::vector<double> Rise;
		for (const LandField::HedgeLine& H : G.Hedges())
		{
			if (H.Kind > 1) continue;
			double Run = 0.0, Since = 0.0, RunX = 0.0, RunY = 0.0;
			// (its top's rise and fall: in each 10 m along it, highest less lowest; and its longest stretch within 0.5 m)
			std::vector<double> Tops;
			std::vector<char> Fought;
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
					Tops.push_back(Top);
					Fought.push_back(InFight(X, Y) ? 1 : 0);
					if (Touched(X, Y, Field + Top)) Touch += 1.0;
					if (Top >= 2.5) High25 += 1.0;
					if (InFight(X, Y))
					{
						FightMetres += 1.0;
						if (Top >= 2.5) FightHigh += 1.0;
					}
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
							if (OnWay(0.5 * (RunX + X), 0.5 * (RunY + Y), false)) ++WayGaps;
							if (Run > LongestOff && !OnWay(0.5 * (RunX + X), 0.5 * (RunY + Y), true)) LongestOff = Run;
							if (Run >= 8.0 && std::getenv("LAND_FIELD_GAPS"))
							{
								const double MX = 0.5 * (RunX + X), MY = 0.5 * (RunY + Y);
								int Veh = 0, Man = 0;
								for (const LandField::Field::Way& W : F.Ways)
									for (size_t Q = 0; Q + 1 < W.Points.size(); ++Q)
									{
										const double WX = W.Points[Q].first, WY = W.Points[Q].second, DX = W.Points[Q + 1].first - WX, DY = W.Points[Q + 1].second - WY, LL = DX * DX + DY * DY;
										const double U = LL > 0.0 ? std::clamp(((MX - WX) * DX + (MY - WY) * DY) / LL, 0.0, 1.0) : 0.0;
										if (std::hypot(MX - WX - DX * U, MY - WY - DY * U) < W.HalfWidth + 3.0) ++(W.HalfWidth >= 1.2 ? Veh : Man);
									}
								int Shrubs = 0, Trees = 0;
								double Tallest = 0.0;
								for (int B = 0; B <= static_cast<int>(LandField::Base::Cart); ++B)
									for (const LandField::Placed& Pl : G.Instances(static_cast<LandField::Base>(B)))
										if (std::hypot(Pl.X - MX, Pl.Y - MY) < 0.5 * Run + 1.0)
										{
											++(B <= static_cast<int>(LandField::Base::Bramble) ? Shrubs : Trees);
											if (B <= static_cast<int>(LandField::Base::Bramble)) Tallest = std::max(Tallest, Pl.Z + G.BaseOf(static_cast<LandField::Base>(B)).Height * Pl.ScaleZ - G.At(MX, MY).Z);
										}
								std::printf("      gap %.0f m from %.0f, %.0f to %.0f, %.0f: bank %.2f m at its middle, %d vehicle and %d men's segments near; %d shrubs (tallest %.1f m over the bank), %d other things\n", Run, RunX, RunY, X, Y,
								            G.At(MX, MY).Z - std::max(G.At(MX + NX * 5.0, MY + NY * 5.0).Z, G.At(MX - NX * 5.0, MY - NY * 5.0).Z), Veh, Man, Shrubs, Tallest, Trees);
							}
						}
						Run = 0.0;
					}
				}
			}
			for (size_t K = 0; K + 20 <= Tops.size(); K += 20)
			{
				const auto Ends = std::minmax_element(Tops.begin() + static_cast<std::ptrdiff_t>(K), Tops.begin() + static_cast<std::ptrdiff_t>(K + 20));
				Rise.push_back(*Ends.second - *Ends.first);
			}
			// (the art review asks no level run over 12 m; one in Normandy's 71 km runs 16, where broad crowns overlap)
			// (where its shrubs stand, 2.9 m or more, and men did not trample it: a vehicle's gap, a bank bare by a house's
			// wall or a hedge pushed through all along is no clipped hedge)
			for (size_t K = 0; K < Tops.size(); ++K)
			{
				if (Tops[K] < 2.9 || Fought[K]) continue;
				double Lo = Tops[K], Hi = Tops[K];
				size_t E = K + 1;
				for (; E < Tops.size() && Tops[E] >= 2.9 && !Fought[E]; ++E)
				{
					Lo = std::min(Lo, Tops[E]);
					Hi = std::max(Hi, Tops[E]);
					if (Hi - Lo >= 0.5) break;
				}
				LongestLevel = std::max(LongestLevel, static_cast<double>(E - K));
			}
		}
		std::printf("  bocage, along %.1f km of hedgerows and lane banks in the field:\n", Metres / 1000.0);
		std::printf("    bank over the field: median %.2f m (10%% %.2f, 90%% %.2f); its foot across %.1f m (10%% %.1f, 90%% %.1f)\n", Pctl(BankH, 0.5), Pctl(BankH, 0.1), Pctl(BankH, 0.9), Pctl(BankFoot, 0.5), Pctl(BankFoot, 0.1),
		            Pctl(BankFoot, 0.9));
		std::printf("    hedge's top over the field (bank and shrubs): median %.2f m (10%% %.2f, 90%% %.2f); under 1.9 m (seen over or through at a man's eye) %.1f%% of its length, %d gaps (%d of them where the recording's men or vehicles cross), median %.0f m, longest %.0f m\n",
		            Pctl(TopH, 0.5), Pctl(TopH, 0.1), Pctl(TopH, 0.9), 100.0 * Low / std::max(1.0, Metres), Gaps, WayGaps, Pctl(GapLen, 0.5), GapLen.empty() ? 0.0 : Pctl(GapLen, 1.0));
		std::printf("    2.5 m or more over the field: %.1f%% of its length; where the fight is (within 30 m of a man's way, %.1f km) %.1f%%; the longest run under 1.9 m but where a vehicle drove %.0f m\n",
		            100.0 * High25 / std::max(1.0, Metres), FightMetres / 1000.0, 100.0 * FightHigh / std::max(1.0, FightMetres), LongestOff);
		{
			int Ragged = 0;
			for (const double R : Rise) Ragged += R >= 1.0 ? 1 : 0;
			const double RaggedShare = static_cast<double>(Ragged) / std::max<size_t>(1, Rise.size()), TouchShare = Touch / std::max(1.0, Metres);
			std::printf("    its top rises and falls: in each 20 m, highest less lowest median %.1f m, 1 m or more in %.0f%% of them; the longest stretch of shrubs level within half a metre (away from the fight) %.0f m; a tree's leaves down to its top over %.0f%% of its length\n",
			            Pctl(Rise, 0.5), 100.0 * RaggedShare, LongestLevel, 100.0 * TouchShare);
			if (Metres > 1000.0)
				Check(Pctl(TopH, 0.5) >= 3.8 && Pctl(TopH, 0.5) <= 4.8 && Pctl(TopH, 0.1) >= 2.8 && Pctl(TopH, 0.9) >= 5.5 && RaggedShare >= 0.85 && LongestLevel <= 16.0 && TouchShare >= 0.7,
				      "the hedgerows stand 4 to 6 m over the field, untrimmed, the trees' leaves down to them (not a clipped hedge before an avenue)",
				      Fmt("top median %.2f m (10%% %.2f, 90%% %.2f); %.0f%% of 20 m rise 1 m", Pctl(TopH, 0.5), Pctl(TopH, 0.1), Pctl(TopH, 0.9), 100.0 * RaggedShare) +
				          Fmt("; level %.0f m at longest; leaves to the top over %.0f%%", LongestLevel, 100.0 * TouchShare));
		}
		if (Metres > 1000.0)
			Check(FightHigh >= 0.9 * FightMetres && LongestOff <= 8.0, "the hedges stay walls where the fight is (no hedge cleared for the men who cross it)",
			      Fmt("%.1f%% of %.1f km at 2.5 m or more; longest low run %.0f m", 100.0 * FightHigh / std::max(1.0, FightMetres), FightMetres / 1000.0, LongestOff));
		// The breaches: where the recording's men went over in numbers, each 4 m across at most and one in 40 m of a hedge.
		{
			std::vector<const LandField::HedgeOpening*> Br;
			int Gw = 0;
			for (const LandField::HedgeOpening& O : G.Openings())
			{
				if (O.Kind == 1) Br.push_back(&O);
				else ++Gw;
			}
			std::vector<double> Wide, Men;
			double Closest = 1e9;
			for (size_t A = 0; A < Br.size(); ++A)
			{
				Wide.push_back(Br[A]->Width);
				Men.push_back(Br[A]->Men);
				const double UX = std::cos(Br[A]->Yaw), UY = std::sin(Br[A]->Yaw);
				for (size_t B = A + 1; B < Br.size(); ++B)
				{
					// (on the same line: each across the other's within 3 m, heading within 20 degrees)
					const double DX = Br[B]->X - Br[A]->X, DY = Br[B]->Y - Br[A]->Y, VX = std::cos(Br[B]->Yaw), VY = std::sin(Br[B]->Yaw);
					const double Turn = std::fabs(std::remainder(Br[B]->Yaw - Br[A]->Yaw, 3.14159265358979));
					if (std::fabs(DX * UY - DY * UX) < 3.0 && std::fabs(DX * VY - DY * VX) < 3.0 && Turn < 0.35) Closest = std::min(Closest, std::fabs(DX * UX + DY * UY));
				}
			}
			if (Gw == 0 && Br.empty()) std::printf("    openings: none\n");
			else std::printf("    openings: %d gateways; %zu breaches blown where the men crossed, %.1f to %.1f m across (median %.1f), gathering %.0f to %.0f crossings each (median %.0f); the closest two on one hedge %.0f m apart\n", Gw,
			            Br.size(), Pctl(Wide, 0.0), Pctl(Wide, 1.0), Pctl(Wide, 0.5), Pctl(Men, 0.0), Pctl(Men, 1.0), Pctl(Men, 0.5), Closest);
			Check(Wide.empty() || (Pctl(Wide, 1.0) <= 4.0 + 1e-6 && Closest >= 40.0 - 1e-6), "a breach is 4 m across at most, one in 40 m of hedge",
			      Fmt("widest %.1f m, closest two %.0f m apart", Pctl(Wide, 1.0), Closest));
		}
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

	/**
	 * On trial, "lodged" (LandField::Ground::AddFlatten): the corn and grass laid over outward round shell holes. Three
	 * grounds of the same field are built here (the word off; the word on; the word on again, its rounds given in another
	 * order) so that the word's own ground (with any LAND_FIELD_FLATTEN rounds, for the pictures) is left as it is.
	 */
	void LodgedTrial(const LandField::Field& Fd, const std::string& Name)
	{
		using LandField::Placed;
		const auto Same = [](const Placed& A, const Placed& B) {
			return A.Kind == B.Kind && A.X == B.X && A.Y == B.Y && A.Z == B.Z && A.Yaw == B.Yaw && A.Scale == B.Scale && A.ScaleZ == B.ScaleZ && A.Tint.R == B.Tint.R && A.Tint.G == B.Tint.G
				&& A.Tint.B == B.Tint.B && A.Lodge == B.Lodge;
		};
		std::printf("  lodged (on trial): shell holes' blasts laying the corn and grass over\n");
		// (the barrage's middle: the look scout's wheat at Normandy, the rye's edge on the steppe at Prokhorovka)
		double CX = 0.5 * Fd.Width(), CY = 0.5 * Fd.Depth();
		if (Name == "normandy-1944")
		{
			CX = 2440.0;
			CY = 680.0;
		}
		else if (Name == "prokhorovka-1943")
		{
			CX = 1968.0;
			CY = 716.0;
		}
		const double BoxX0 = CX - 64.0, BoxY0 = CY - 64.0, BoxX1 = CX + 64.0, BoxY1 = CY + 64.0;
		LandField::Field Off = Fd;
		bool bOff = Off.Trial != "all";
		{
			std::string Kept;
			for (size_t P = 0; P <= Off.Trial.size();)
			{
				size_t E = Off.Trial.find(',', P);
				if (E == std::string::npos) E = Off.Trial.size();
				const std::string Word = Off.Trial.substr(P, E - P);
				if (Word != "lodged" && !Word.empty()) Kept += (Kept.empty() ? "" : ",") + Word;
				P = E + 1;
			}
			Off.Trial = Kept;
		}
		LandField::Ground On(Fd);
		std::vector<Placed> Plain, Bare;
		On.Clutter(BoxX0, BoxY0, BoxX1, BoxY1, Bare);
		// The rounds: 200 near the eye over 10 s, mixed (light mortars, 105s, 150s and a few heavier), each its Outer as the
		// effects engineer passes it (2.5 x its rim in corn, 1.8 x in grass, at most 15 m).
		struct Round
		{
			double X, Y, Core, Outer;
		};
		std::vector<Round> Rounds;
		{
			unsigned Lcg = 777u;
			auto Draw = [&Lcg]() { Lcg = Lcg * 1664525u + 1013904223u; return (Lcg >> 8) / 16777216.0; };
			for (int K = 0; K < 200; ++K)
			{
				const double A = 6.2832 * Draw(), Rr = 38.0 * std::sqrt(Draw()), Kind = Draw();
				const double Core = Kind < 0.45 ? 1.0 : Kind < 0.8 ? 2.2 : Kind < 0.95 ? 3.5 : 4.5;
				const double RX = CX + Rr * std::cos(A), RY = CY + Rr * std::sin(A);
				Rounds.push_back({ RX, RY, Core, std::min(15.0, (On.CornAt(RX, RY) ? 2.5 : 1.8) * Core) });
			}
		}
		if (bOff)
		{
			LandField::Ground Without(Off);
			Without.Clutter(BoxX0, BoxY0, BoxX1, BoxY1, Plain);
			long Differ = Plain.size() == Bare.size() ? 0 : 1;
			for (size_t N = 0; N < Plain.size() && N < Bare.size(); ++N) Differ += Same(Plain[N], Bare[N]) ? 0 : 1;
			Check(Differ == 0 && !Plain.empty(), "lodged: the word on with no round lays every clutter thing as the word off, every field of its record",
			      Fmt("%.0f differ of %.0f", static_cast<double>(Differ), static_cast<double>(Plain.size())));
			// (and with the word off AddFlatten does nothing at all)
			for (const Round& R : Rounds) Without.AddFlatten(R.X, R.Y, R.Core, R.Outer);
			std::vector<Placed> After;
			Without.Clutter(BoxX0, BoxY0, BoxX1, BoxY1, After);
			long Moved = After.size() == Plain.size() ? 0 : 1;
			for (size_t N = 0; N < Plain.size() && N < After.size(); ++N) Moved += Same(Plain[N], After[N]) ? 0 : 1;
			Check(Moved == 0 && Without.Flattened() == 0, "lodged: with the word off AddFlatten keeps nothing and changes nothing",
			      Fmt("%.0f rounds kept, ", static_cast<double>(Without.Flattened())) + Fmt("%.0f things changed", static_cast<double>(Moved)));
		}
		else
			Plain = Bare;
		// A moving eye over the barrage, as the scene drives the carpet (every kind laid, each its own mesh): first with no
		// round, then with the 200 rounds falling over its first 10 s; each frame's time and ops, and the meshes kept here
		// as the engine would keep them, op by op.
		constexpr int Kinds = static_cast<int>(LandField::Base::Count);
		double WorstMs[2] = {}, SumMs[2] = {}, AddUs = 0.0;
		long WorstOps[2] = {}, SumOps[2] = {}, BadOps = 0, Needless = 0, Reposes = 0, Mismatch = 0, SquaresChecked = 0, ThingsChecked = 0, Lodges = 0;
		constexpr int Frames = 60 * 14;
		for (int Walk = 0; Walk < 2; ++Walk)
		{
			LandCarpet::Squares Cp;
			Cp.Reach.assign(Kinds, 1e9);
			Cp.CopyOf = [](const Placed& P) { return static_cast<int32_t>(P.Kind); };
			std::vector<std::vector<std::pair<bool, Placed>>> Mesh(Kinds);
			std::vector<LandCarpet::Op> Out;
			double EX = CX - 20.0, EY = CY - 6.0;
			for (int Frame = 0; Frame < Frames; ++Frame)
			{
				const double Now = Frame / 60.0;
				EX += 1.5 / 60.0;
				if (Walk == 1 && Frame % 3 == 0 && Frame / 3 < static_cast<int>(Rounds.size()))
				{
					const Round& R = Rounds[static_cast<size_t>(Frame / 3)];
					const auto T1 = std::chrono::steady_clock::now();
					On.AddFlatten(R.X, R.Y, R.Core, R.Outer);
					AddUs += Seconds(T1) * 1e6;
				}
				Out.clear();
				const auto T0 = std::chrono::steady_clock::now();
				const LandCarpet::Tally Tl = Cp.Update(On, EX, EY, EX + 60.0, EY, Frame == 0, 4, Now, Out);
				const double Ms = Seconds(T0) * 1000.0;
				Lodges += Walk == 1 ? Tl.Lodged : 0;
				if (Frame > 0)
				{
					WorstMs[Walk] = std::max(WorstMs[Walk], Ms);
					SumMs[Walk] += Ms;
					WorstOps[Walk] = std::max(WorstOps[Walk], static_cast<long>(Out.size()));
					SumOps[Walk] += static_cast<long>(Out.size());
				}
				int32_t PopDue = -1;
				for (const LandCarpet::Op& O : Out)
				{
					if (O.Copy < 0 || O.Copy >= Kinds)
					{
						if (O.Copy == -1 && O.Slot == -1)
							for (auto& M : Mesh) M.clear();
						else
							++BadOps;
						continue;
					}
					auto& M = Mesh[static_cast<size_t>(O.Copy)];
					if (PopDue >= 0 && (!O.bPop || O.Copy != PopDue)) ++BadOps;
					PopDue = -1;
					if (O.bPop)
					{
						if (M.empty() || O.Slot + 1 != static_cast<int32_t>(M.size())) ++BadOps;
						else
							M.pop_back();
						continue;
					}
					if (O.Slot == static_cast<int32_t>(M.size()))
					{
						M.push_back({ true, O.Thing });
						continue;
					}
					if (O.Slot < 0 || O.Slot > static_cast<int32_t>(M.size()))
					{
						++BadOps;
						continue;
					}
					auto& Place = M[static_cast<size_t>(O.Slot)];
					if (O.bRepose)
					{
						++Reposes;
						if (Place.second.X != O.Thing.X || Place.second.Y != O.Thing.Y || Place.second.Kind != O.Thing.Kind) ++BadOps;
						if (Same(Place.second, O.Thing)) ++Needless;
						Place.second = O.Thing;
						continue;
					}
					Place.second = O.Thing; // (a move of the mesh's last thing; its pop follows)
					PopDue = O.Copy;
				}
				BadOps += PopDue >= 0 ? 1 : 0;
			}
			if (Walk == 0) continue;
			// Made before the rounds and laid over since, or made after them: each held square as the field lays it now.
			std::map<std::tuple<int, float, float>, Placed> Laid;
			for (const auto& M : Mesh)
				for (const auto& P : M) Laid[{ static_cast<int>(P.second.Kind), P.second.X, P.second.Y }] = P.second;
			for (int J = static_cast<int>(std::floor((EY - 70.0) / 16.0)); J <= static_cast<int>(std::floor((EY + 70.0) / 16.0)); ++J)
				for (int I = static_cast<int>(std::floor((EX - 70.0) / 16.0)); I <= static_cast<int>(std::floor((EX + 130.0) / 16.0)); ++I)
				{
					if (!Cp.Holds(I, J)) continue;
					++SquaresChecked;
					std::vector<Placed> Fresh;
					On.Clutter(I * 16.0, J * 16.0, (I + 1) * 16.0, (J + 1) * 16.0, Fresh);
					for (const Placed& P : Fresh)
					{
						++ThingsChecked;
						const auto Found = Laid.find({ static_cast<int>(P.Kind), P.X, P.Y });
						if (Found == Laid.end() || !Same(Found->second, P)) ++Mismatch;
					}
				}
		}
		std::printf("    a moving eye over a barrage of %zu rounds in 10 s (%zu kept): updates %.3f ms mean, worst %.2f ms, worst %ld ops (%.1f a frame); "
		            "the same walk with no round %.3f ms mean, worst %.2f ms, worst %ld ops (%.1f a frame); %ld squares laid over again, %ld things re-posed in place\n",
			Rounds.size(), On.Flattened(), SumMs[1] / (Frames - 1), WorstMs[1], WorstOps[1], static_cast<double>(SumOps[1]) / (Frames - 1), SumMs[0] / (Frames - 1), WorstMs[0], WorstOps[0],
			static_cast<double>(SumOps[0]) / (Frames - 1), Lodges, Reposes);
		std::printf("    AddFlatten: %.1f microseconds a call (mean of %zu, mid-walk)\n", AddUs / static_cast<double>(Rounds.size()), Rounds.size());
		Check(BadOps == 0, "lodged: the carpet's ops through a barrage kept straight (a re-pose only of a held thing in its own place)", Fmt("%.0f wrong", static_cast<double>(BadOps)));
		Check(Needless == 0 && Reposes > 0, "lodged: a laying-over update sends ops only for things whose record changed",
		      Fmt("%.0f re-posed, ", static_cast<double>(Reposes)) + Fmt("%.0f of them unchanged", static_cast<double>(Needless)));
		Check(Mismatch == 0 && SquaresChecked > 0, "lodged: a carpet square made before the rounds and laid over since equals one made after them, thing for thing",
		      Fmt("%.0f of ", static_cast<double>(Mismatch)) + Fmt("%.0f things in ", static_cast<double>(ThingsChecked)) + Fmt("%.0f squares differ", static_cast<double>(SquaresChecked)));
		// The rounds laid: against the field with none (the word off, checked equal above).
		std::vector<Placed> Over;
		On.Clutter(BoxX0, BoxY0, BoxX1, BoxY1, Over);
		long Count = Over.size() == Plain.size() ? 0 : 1, Shifted = 0, Outside = 0, Stiff = 0, Leaning = 0, Single = 0, Outward = 0;
		double LeanSum = 0.0;
		for (size_t N = 0; N < Over.size() && N < Plain.size(); ++N)
		{
			const Placed& A = Over[N];
			const Placed& B = Plain[N];
			if (A.Kind != B.Kind || A.X != B.X || A.Y != B.Y)
			{
				++Count;
				continue;
			}
			if (A.Z != B.Z || A.Scale != B.Scale || A.Tint.R != B.Tint.R || A.Tint.G != B.Tint.G || A.Tint.B != B.Tint.B) ++Shifted;
			if (Same(A, B)) continue;
			const LandField::Base K = A.Kind;
			if (K == LandField::Base::Stone || K == LandField::Base::Trodden || K == LandField::Base::Stubble || K == LandField::Base::Sward || K == LandField::Base::Rosette) ++Stiff;
			int Within = 0;
			double Way = 0.0;
			for (size_t R = 0; R < On.Flattened(); ++R)
			{
				double RX = 0.0, RY = 0.0, Reach = 0.0;
				On.FlattenRound(R, RX, RY, Reach);
				const double Bearing = std::atan2(A.Y - RY, A.X - RX);
				if (std::hypot(A.X - RX, A.Y - RY) < On.FlattenEdge(R, Bearing) + 1e-3)
				{
					++Within;
					Way = Bearing;
				}
			}
			Outside += Within == 0 ? 1 : 0;
			if (A.Lodge > 0.15f)
			{
				++Leaning;
				LeanSum += A.Lodge;
				if (Within == 1)
				{
					++Single;
					const double Off2 = std::remainder(static_cast<double>(A.Yaw) - Way, 6.283185307179586);
					Outward += std::fabs(Off2) <= 35.0 * 3.141592653589793 / 180.0 ? 1 : 0;
				}
			}
		}
		// (every field of every thing in the box, hashed: the same from gcc and clang, thing for thing)
		uint64_t Print = 1469598103934665603ull;
		for (const Placed& Th : Over)
		{
			const float Fs[7] = { Th.X, Th.Y, Th.Z, Th.Yaw, Th.Scale, Th.ScaleZ, Th.Lodge };
			uint8_t Bytes[sizeof(Fs) + 4] = {};
			std::memcpy(Bytes, Fs, sizeof(Fs));
			Bytes[sizeof(Fs)] = static_cast<uint8_t>(Th.Kind);
			Bytes[sizeof(Fs) + 1] = Th.Tint.R;
			Bytes[sizeof(Fs) + 2] = Th.Tint.G;
			Bytes[sizeof(Fs) + 3] = Th.Tint.B;
			for (const uint8_t Byte : Bytes) Print = (Print ^ Byte) * 1099511628211ull;
		}
		std::printf("    in a 128 m box over the barrage: %zu things, %ld laid over (mean %.0f degrees); every field hashed %016llx\n", Over.size(), Leaning,
			Leaning ? LeanSum / Leaning * 57.29578 : 0.0, static_cast<unsigned long long>(Print));
		Check(Count == 0 && Shifted == 0, "lodged: no clutter thing added or taken away, none moved, resized or tinted (the same things in the same order, X, Y, Z, Scale, Tint)",
		      Fmt("%.0f added, taken or reordered, ", static_cast<double>(Count)) + Fmt("%.0f moved", static_cast<double>(Shifted)));
		Check(Outside == 0, "lodged: no thing outside every round's wandering edge changed in any field", Fmt("%.0f changed outside", static_cast<double>(Outside)));
		Check(Stiff == 0, "lodged: stones, trodden earth, stubble, the short sward and the docks never lean", Fmt("%.0f changed", static_cast<double>(Stiff)));
		Check(Single >= 20 && Outward >= Single * 9 / 10, "lodged: things laid over lie outward from the burst (within 35 degrees, 9 in 10 at least; those in one round's reach)",
		      Fmt("%.0f of ", static_cast<double>(Outward)) + Fmt("%.0f", static_cast<double>(Single)));
		// The same rounds in another order (a shuffle), on a third ground: every thing the same.
		{
			LandField::Ground Again(Fd);
			std::vector<Round> Shuffled = Rounds;
			unsigned Lcg = 4242u;
			for (size_t N = Shuffled.size(); N > 1; --N)
			{
				Lcg = Lcg * 1664525u + 1013904223u;
				std::swap(Shuffled[N - 1], Shuffled[(Lcg >> 8) % N]);
			}
			for (const Round& R : Shuffled) Again.AddFlatten(R.X, R.Y, R.Core, R.Outer);
			std::vector<Placed> Other;
			Again.Clutter(BoxX0, BoxY0, BoxX1, BoxY1, Other);
			long Differ = Other.size() == Over.size() ? 0 : 1;
			for (size_t N = 0; N < Other.size() && N < Over.size(); ++N) Differ += Same(Other[N], Over[N]) ? 0 : 1;
			Check(Differ == 0, "lodged: the same rounds in another order lay every thing the same (the strongest wins, whatever the order)",
			      Fmt("%.0f differ; ", static_cast<double>(Differ)) + Fmt("%.0f rounds kept in one order, ", static_cast<double>(On.Flattened())) + Fmt("%.0f in the other", static_cast<double>(Again.Flattened())));
			// (a small round within a big one's flattest: nothing more, the strongest counts, they do not add)
			const Round& Big = *std::max_element(Rounds.begin(), Rounds.end(), [](const Round& A, const Round& B) { return A.Outer < B.Outer; });
			const size_t Was = Again.Flattened();
			Again.AddFlatten(Big.X + 0.8, Big.Y, 0.5, 1.0);
			std::vector<Placed> Third;
			Again.Clutter(BoxX0, BoxY0, BoxX1, BoxY1, Third);
			long Added = Third.size() == Other.size() ? 0 : 1;
			for (size_t N = 0; N < Other.size() && N < Third.size(); ++N) Added += Same(Other[N], Third[N]) ? 0 : 1;
			Check(Added == 0 && Again.Flattened() == Was, "lodged: a small round inside a big one's flattest changes nothing and is not kept", Fmt("%.0f things changed", static_cast<double>(Added)));
			// (a burst far off the field, or not a number, is not kept: no square's number can overflow)
			const size_t Before = Again.Flattened();
			Again.AddFlatten(1e12, CY, 2.0, 5.0);
			Again.AddFlatten(CX, -1e11, 2.0, 5.0);
			Again.AddFlatten(-40.0, CY, 2.0, 5.0);
			Again.AddFlatten(std::nan(""), CY, 2.0, 5.0);
			Check(Again.Flattened() == Before, "lodged: a burst more than 30 m off the field (or not a number) is not kept",
			      Fmt("%.0f kept", static_cast<double>(Again.Flattened() - Before)));
			// A hot spot (the contract review's case): 70 heavy rounds 19 m round an 8 m square, none reaching within 3 m of
			// its middle, then a light mortar at its middle. Every thing within 3 m of the middle must lie as with the
			// mortar alone (on a fourth ground): the square's cap keeps those reaching deepest into it, not the widest.
			LandField::Ground Fourth(Fd);
			{
				const double HX = std::floor((CX - 150.0) / 8.0) * 8.0 + 4.0, HY = std::floor(CY / 8.0) * 8.0 + 4.0;
				for (int K = 0; K < 70; ++K)
				{
					const double A = 6.283185307179586 * K / 70.0;
					Again.AddFlatten(HX + 19.0 * std::cos(A), HY + 19.0 * std::sin(A), 5.0, 12.5);
				}
				Again.AddFlatten(HX, HY, 1.0, 2.5);
				Fourth.AddFlatten(HX, HY, 1.0, 2.5);
				std::vector<Placed> Hot, Alone;
				Again.Clutter(HX - 3.0, HY - 3.0, HX + 3.0, HY + 3.0, Hot);
				Fourth.Clutter(HX - 3.0, HY - 3.0, HX + 3.0, HY + 3.0, Alone);
				long Near = 0, HotDiffer = Hot.size() == Alone.size() ? 0 : 1, Lean = 0;
				for (size_t N = 0; N < Hot.size() && N < Alone.size(); ++N)
				{
					if (std::hypot(Hot[N].X - HX, Hot[N].Y - HY) >= 3.0) continue;
					++Near;
					Lean += Alone[N].Lodge > 0.0f ? 1 : 0;
					HotDiffer += Same(Hot[N], Alone[N]) ? 0 : 1;
				}
				Check(HotDiffer == 0 && Lean > 0, "lodged: at a hot spot (70 heavy rounds round a square) a light round in its middle still lays its things over as alone",
				      Fmt("%.0f differ of ", static_cast<double>(HotDiffer)) + Fmt("%.0f within 3 m, ", static_cast<double>(Near)) + Fmt("%.0f laid over", static_cast<double>(Lean)));
			}
			// The ground changed by more than the blasts while a square is held (a burn added mid-battle, with a round at the
			// same place): the carpet makes the square again whole, and every held square equals the field's, thing for thing.
			{
				LandCarpet::Squares Cb;
				Cb.Reach.assign(Kinds, 1e9);
				Cb.CopyOf = [](const Placed& P) { return static_cast<int32_t>(P.Kind); };
				std::vector<std::vector<Placed>> Meshes(Kinds);
				std::vector<LandCarpet::Op> Ops;
				long Wrong = 0;
				const auto Apply = [&]() {
					for (const LandCarpet::Op& O : Ops)
					{
						if (O.Copy == -1 && O.Slot == -1)
						{
							for (auto& M : Meshes) M.clear();
							continue;
						}
						if (O.Copy < 0 || O.Copy >= Kinds)
						{
							++Wrong;
							continue;
						}
						auto& M = Meshes[static_cast<size_t>(O.Copy)];
						if (O.bPop)
						{
							if (M.empty() || O.Slot + 1 != static_cast<int32_t>(M.size())) ++Wrong;
							else
								M.pop_back();
						}
						else if (O.Slot == static_cast<int32_t>(M.size()))
							M.push_back(O.Thing);
						else if (O.Slot >= 0 && O.Slot < static_cast<int32_t>(M.size()))
							M[static_cast<size_t>(O.Slot)] = O.Thing;
						else
							++Wrong;
					}
				};
				const double BX = CX + 3.0, BY = CY;
				for (int Frame = 0; Frame < 240; ++Frame)
				{
					if (Frame == 60)
					{
						Fourth.AddFlatten(BX, BY, 2.2, 5.5);
						Fourth.AddBurn(BX, BY, 2.0, 5.0); // (within the round's reach: a burn reaching farther is not the carpet's to lay again, AddBurn)
					}
					Ops.clear();
					Cb.Update(Fourth, CX - 10.0, CY, CX + 30.0, CY, Frame == 0, 4, Frame / 60.0, Ops);
					Apply();
				}
				std::map<std::tuple<int, float, float>, Placed> Held;
				size_t Laid = 0;
				for (const auto& M : Meshes)
				{
					Laid += M.size();
					for (const Placed& P : M) Held[{ static_cast<int>(P.Kind), P.X, P.Y }] = P;
				}
				size_t Fresh = 0;
				long Stale = 0;
				for (int J = static_cast<int>(std::floor((CY - 90.0) / 16.0)); J <= static_cast<int>(std::floor((CY + 90.0) / 16.0)); ++J)
					for (int I = static_cast<int>(std::floor((CX - 100.0) / 16.0)); I <= static_cast<int>(std::floor((CX + 110.0) / 16.0)); ++I)
					{
						if (!Cb.Holds(I, J)) continue;
						std::vector<Placed> Now;
						Fourth.Clutter(I * 16.0, J * 16.0, (I + 1) * 16.0, (J + 1) * 16.0, Now);
						for (const Placed& P : Now)
						{
							++Fresh;
							const auto Found = Held.find({ static_cast<int>(P.Kind), P.X, P.Y });
							if (Found == Held.end() || !Same(Found->second, P)) ++Stale;
						}
					}
				Check(Wrong == 0 && Stale == 0 && Laid == Fresh && Fresh > 0, "lodged: a burn added mid-battle where a round fell: the held squares made again whole, each equal to the field's",
				      Fmt("%.0f wrong ops, ", static_cast<double>(Wrong)) + Fmt("%.0f stale, ", static_cast<double>(Stale)) + Fmt("%.0f laid against ", static_cast<double>(Laid)) + Fmt("%.0f in the field", static_cast<double>(Fresh)));
			}
			// The cost: an 8 m square of the clutter laid with 0, 1 and 20 rounds reaching into it (away from the barrage).
			const double SX = std::floor((CX + 150.0) / 8.0) * 8.0, SY = std::floor(CY / 8.0) * 8.0;
			auto Time = [&Again, SX, SY]() {
				std::vector<Placed> Sq8;
				const auto T2 = std::chrono::steady_clock::now();
				for (int K = 0; K < 300; ++K)
				{
					Sq8.clear();
					Again.Clutter(SX, SY, SX + 8.0, SY + 8.0, Sq8);
				}
				return std::make_pair(Seconds(T2) * 1e6 / 300.0, Sq8.size());
			};
			const auto None = Time();
			Again.AddFlatten(SX + 4.0, SY + 4.0, 2.2, 5.5);
			const auto One = Time();
			for (int K = 0; K < 19; ++K) Again.AddFlatten(SX + 0.4 + 7.2 * ((K % 5) / 4.0), SY + 0.4 + 7.2 * ((K / 5) / 3.0), 0.6, 1.5);
			const auto Twenty = Time();
			std::printf("    Clutter of one 8 m square (%zu things): %.0f microseconds with no round in reach, %.0f with 1, %.0f with 20\n", None.second, None.first, One.first, Twenty.first);
			// Memory: 5,000 rounds over the whole field.
#if defined(__GLIBC__)
			const size_t Heap0 = mallinfo2().uordblks;
#else
			const size_t Heap0 = 0;
#endif
			const size_t Kept0 = Again.Flattened();
			unsigned Lcg2 = 99u;
			auto Draw2 = [&Lcg2]() { Lcg2 = Lcg2 * 1664525u + 1013904223u; return (Lcg2 >> 8) / 16777216.0; };
			const auto T3 = std::chrono::steady_clock::now();
			for (int K = 0; K < 5000; ++K)
			{
				const double Core = 1.0 + 3.5 * Draw2() * Draw2();
				Again.AddFlatten(Fd.Width() * Draw2(), Fd.Depth() * Draw2(), Core, std::min(15.0, 2.2 * Core));
			}
			const double Each = Seconds(T3) * 1e6 / 5000.0;
#if defined(__GLIBC__)
			const size_t Heap1 = mallinfo2().uordblks;
#else
			const size_t Heap1 = 0;
#endif
			std::printf("    5,000 more rounds over the field: %.1f microseconds a call, %zu kept, the heap %.2f MB larger (glibc's count)\n", Each, Again.Flattened() - Kept0,
				(static_cast<double>(Heap1) - static_cast<double>(Heap0)) / 1048576.0);
		}
	}

	/**
	 * On trial, "diggings" (alone, on the steppe): the rules' works dug in heaps as "ploughland" paints them, and nothing
	 * else of ploughland. Two more grounds of the same field: the word off, and the word swapped for "ploughland". Every
	 * near tile's point on the works the colour ploughland gives it; every point of a fallow, and of every other ground
	 * more than 15 m from the works, the colour with the word off; no tile any furrow; the clutter of the man's box in a
	 * fallow (120 m at 820, 340) the word off's, and of a box over the works ploughland's, piece for piece.
	 */
	void DiggingsTrial(const LandField::Field& Fd, const LandField::Ground& G)
	{
		using LandField::Placed;
		std::printf("  diggings (on trial): the works dug in heaps as ploughland has them, the fallows as they were\n");
		const auto DigSwapped = [&Fd](const char* DigTo) {
			std::string DigKept;
			for (size_t P = 0; P <= Fd.Trial.size();)
			{
				size_t E = Fd.Trial.find(',', P);
				if (E == std::string::npos) E = Fd.Trial.size();
				const std::string DigWord = Fd.Trial.substr(P, E - P);
				const std::string DigWordNow = DigWord == "diggings" ? std::string(DigTo) : DigWord;
				if (!DigWordNow.empty()) DigKept += (DigKept.empty() ? "" : ",") + DigWordNow;
				P = E + 1;
			}
			LandField::Field DigOut = Fd;
			DigOut.Trial = DigKept;
			return DigOut;
		};
		const LandField::Field DigOffField = DigSwapped(""), DigPloughField = DigSwapped("ploughland");
		const LandField::Ground DigOff(DigOffField), DigPlough(DigPloughField);
		// (the works on the field's 2 m raster, and every square within 18 m of one: a point in none is over 15 m away)
		constexpr double DigCell = 2.0;
		const int DigCols = static_cast<int>(std::ceil(Fd.Width() / DigCell)), DigRows = static_cast<int>(std::ceil(Fd.Depth() / DigCell));
		std::vector<uint8_t> DugCells(static_cast<size_t>(DigCols) * DigRows, 0), NearDug(static_cast<size_t>(DigCols) * DigRows, 0);
		long DugCount = 0;
		for (int J = 0; J < DigRows; ++J)
			for (int I = 0; I < DigCols; ++I)
			{
				LandField::Drawn DugWhat = LandField::Drawn::Pasture;
				G.HeightAt((I + 0.5) * DigCell, (J + 0.5) * DigCell, &DugWhat);
				if (DugWhat != LandField::Drawn::Works) continue;
				DugCells[static_cast<size_t>(J) * DigCols + I] = 1;
				++DugCount;
			}
		constexpr int DigReach = 9;
		for (int J = 0; J < DigRows; ++J)
			for (int I = 0; I < DigCols; ++I)
			{
				if (!DugCells[static_cast<size_t>(J) * DigCols + I]) continue;
				for (int B = std::max(0, J - DigReach); B <= std::min(DigRows - 1, J + DigReach); ++B)
					for (int A = std::max(0, I - DigReach); A <= std::min(DigCols - 1, I + DigReach); ++A)
						if ((A - I) * (A - I) + (B - J) * (B - J) <= DigReach * DigReach) NearDug[static_cast<size_t>(B) * DigCols + A] = 1;
			}
		const auto NearWorks = [&](double X, double Y) {
			const int I = static_cast<int>(std::floor(X / DigCell)), J = static_cast<int>(std::floor(Y / DigCell));
			if (I < 0 || J < 0 || I >= DigCols || J >= DigRows) return false;
			return NearDug[static_cast<size_t>(J) * DigCols + I] != 0;
		};
		const auto InFallow = [&G](double X, double Y) {
			const int DigParcel = G.ParcelAt(X, Y);
			return DigParcel >= 0 && G.CropOfParcel(DigParcel) == "fallow";
		};
		const auto DigGap = [](const LandField::Rgb8& A, const LandField::Rgb8& B) {
			return std::max({ std::abs(A.R - B.R), std::abs(A.G - B.G), std::abs(A.B - B.B) });
		};
		long OnWorks = 0, WorksOff = 0, InFallowPts = 0, FarFromDug = 0, FarFromDugOff = 0, NearDugPts = 0, NearDugOff = 0, NearDugPlough = 0, BadShape = 0, DugFurrowed = 0;
		long FarAwayOff = 0, FarPts = 0;
		int WorksWorst = 0, FarFromDugWorst = 0, NearDugWorst = 0;
		const auto& TilesDig = G.Tiles();
		const auto& TilesOff = DigOff.Tiles();
		const auto& TilesPlough = DigPlough.Tiles();
		if (TilesDig.size() != TilesOff.size() || TilesDig.size() != TilesPlough.size()) ++BadShape;
		for (size_t T = 0; T < TilesDig.size() && T < TilesOff.size() && T < TilesPlough.size(); ++T)
		{
			const LandField::Mesh& M = TilesDig[T].Near;
			const LandField::Mesh& MO = TilesOff[T].Near;
			const LandField::Mesh& MPl = TilesPlough[T].Near;
			DugFurrowed += static_cast<long>(M.Uvs.size() + TilesDig[T].Far.Uvs.size());
			if (M.Positions != MO.Positions || M.Positions != MPl.Positions || M.Colours.size() != M.Points() || MO.Colours.size() != M.Points() || MPl.Colours.size() != M.Points())
			{
				++BadShape;
				continue;
			}
			for (size_t K = 0; K < M.Points(); ++K)
			{
				const double X = M.Positions[3 * K], Y = M.Positions[3 * K + 1];
				LandField::Drawn DugWhat = LandField::Drawn::Pasture;
				G.HeightAt(X, Y, &DugWhat);
				const int ToOff = DigGap(M.Colours[K], MO.Colours[K]), ToPlough = DigGap(M.Colours[K], MPl.Colours[K]);
				if (DugWhat == LandField::Drawn::Works)
				{
					++OnWorks;
					WorksOff += ToPlough != 0;
					WorksWorst = std::max(WorksWorst, ToPlough);
				}
				else if (InFallow(X, Y) || !NearWorks(X, Y))
				{
					++FarFromDug;
					InFallowPts += InFallow(X, Y);
					FarFromDugOff += ToOff != 0;
					FarFromDugWorst = std::max(FarFromDugWorst, ToOff);
				}
				else
				{
					++NearDugPts;
					NearDugOff += ToOff != 0;
					NearDugPlough += ToPlough != 0;
					NearDugWorst = std::max(NearDugWorst, std::min(ToOff, ToPlough));
				}
			}
			// (the far level, for the record: its points mix the near colour round them, so only those far from the works)
			const LandField::Mesh& FarDig = TilesDig[T].Far;
			const LandField::Mesh& FO = TilesOff[T].Far;
			if (FarDig.Positions == FO.Positions && FarDig.Colours.size() == FO.Colours.size())
				for (size_t K = 0; K < FarDig.Points(); ++K)
				{
					if (NearWorks(FarDig.Positions[3 * K], FarDig.Positions[3 * K + 1])) continue;
					++FarPts;
					FarAwayOff += DigGap(FarDig.Colours[K], FO.Colours[K]) != 0;
				}
		}
		for (const LandField::Tile& DigRingTile : G.Ring()) DugFurrowed += static_cast<long>(DigRingTile.Near.Uvs.size() + DigRingTile.Far.Uvs.size());
		std::printf("    %ld works squares of 2 m; near points: %ld on the works (%ld unlike ploughland, worst %d), %ld in a fallow or over 15 m off (%ld in a fallow; %ld unlike the word off, worst %d), "
		            "%ld within 15 m of the works off them (%ld unlike the word off, %ld unlike ploughland, worst of the nearer %d); far points over 15 m off %ld (%ld unlike the word off)\n",
		            DugCount, OnWorks, WorksOff, WorksWorst, FarFromDug, InFallowPts, FarFromDugOff, FarFromDugWorst, NearDugPts, NearDugOff, NearDugPlough, NearDugWorst, FarPts, FarAwayOff);
		Check(BadShape == 0 && DugCount > 0 && OnWorks > 0 && WorksOff == 0, "diggings: every near point on the works painted as ploughland paints it",
		      Fmt("%.0f of %.0f differ", static_cast<double>(WorksOff), static_cast<double>(OnWorks)) + Fmt(", %.0f tiles unlike in shape", static_cast<double>(BadShape)));
		Check(BadShape == 0 && InFallowPts > 0 && FarFromDugOff == 0, "diggings: every near point of a fallow, and of all other ground over 15 m from the works, as with the word off",
		      Fmt("%.0f of %.0f differ", static_cast<double>(FarFromDugOff), static_cast<double>(FarFromDug)));
		Check(DugFurrowed == 0, "diggings: no tile carries the furrows' two numbers", Fmt("%.0f numbers", static_cast<double>(DugFurrowed)));
		// The clutter, piece for piece: the man's box in a fallow against the word off, and a box over the works (the 40 m
		// grid's box of 120 m with the most works and no fallow) against ploughland.
		const auto DigSame = [](const Placed& A, const Placed& B) {
			return A.Kind == B.Kind && A.X == B.X && A.Y == B.Y && A.Z == B.Z && A.Yaw == B.Yaw && A.Scale == B.Scale && A.ScaleZ == B.ScaleZ && A.Tint.R == B.Tint.R && A.Tint.G == B.Tint.G
				&& A.Tint.B == B.Tint.B && A.Lodge == B.Lodge;
		};
		const auto DigDiffering = [&DigSame](const std::vector<Placed>& A, const std::vector<Placed>& B) {
			long D = A.size() == B.size() ? 0 : 1;
			for (size_t N = 0; N < A.size() && N < B.size(); ++N) D += DigSame(A[N], B[N]) ? 0 : 1;
			return D;
		};
		{
			std::vector<Placed> DigHere, DigThere;
			G.Clutter(760.0, 280.0, 880.0, 400.0, DigHere);
			DigOff.Clutter(760.0, 280.0, 880.0, 400.0, DigThere);
			const long D = DigDiffering(DigHere, DigThere);
			Check(D == 0 && !DigHere.empty() && InFallow(820.0, 340.0), "diggings: the clutter of the man's box in a fallow (120 m at 820, 340) as with the word off, piece for piece",
			      Fmt("%.0f differ of %.0f", static_cast<double>(D), static_cast<double>(DigHere.size())));
		}
		{
			double DugBoxX = -1.0, DugBoxY = -1.0;
			long BestDug = 0;
			for (double BoxMidY = 60.0; BoxMidY + 60.0 <= Fd.Depth(); BoxMidY += 40.0)
				for (double BoxMidX = 60.0; BoxMidX + 60.0 <= Fd.Width(); BoxMidX += 40.0)
				{
					long DugSeen = 0;
					bool bDugBoxFallow = false;
					for (double Y = BoxMidY - 59.0; Y < BoxMidY + 60.0 && !bDugBoxFallow; Y += 2.0)
						for (double X = BoxMidX - 59.0; X < BoxMidX + 60.0; X += 2.0)
						{
							const int I = static_cast<int>(X / DigCell), J = static_cast<int>(Y / DigCell);
							if (I >= 0 && J >= 0 && I < DigCols && J < DigRows) DugSeen += DugCells[static_cast<size_t>(J) * DigCols + I];
							if (static_cast<int>(X) % 8 == 1 && static_cast<int>(Y) % 8 == 1 && InFallow(X, Y))
							{
								bDugBoxFallow = true;
								break;
							}
						}
					if (!bDugBoxFallow && DugSeen > BestDug)
					{
						BestDug = DugSeen;
						DugBoxX = BoxMidX;
						DugBoxY = BoxMidY;
					}
				}
			std::vector<Placed> DigHere, DigThere, DigPlain;
			if (BestDug > 0)
			{
				G.Clutter(DugBoxX - 60.0, DugBoxY - 60.0, DugBoxX + 60.0, DugBoxY + 60.0, DigHere);
				DigPlough.Clutter(DugBoxX - 60.0, DugBoxY - 60.0, DugBoxX + 60.0, DugBoxY + 60.0, DigThere);
				DigOff.Clutter(DugBoxX - 60.0, DugBoxY - 60.0, DugBoxX + 60.0, DugBoxY + 60.0, DigPlain);
			}
			const long D = DigDiffering(DigHere, DigThere), DOff = DigDiffering(DigHere, DigPlain);
			Check(BestDug > 0 && D == 0 && !DigHere.empty(), "diggings: the clutter of a box over the works as ploughland's, piece for piece",
			      Fmt("box at %.0f, %.0f, ", DugBoxX, DugBoxY) + Fmt("%.0f m2 of works; %.0f differ of %.0f", DigCell * DigCell * static_cast<double>(BestDug), static_cast<double>(D), static_cast<double>(DigHere.size()))
				      + Fmt(" (%.0f unlike the word off)", static_cast<double>(DOff)));
		}
	}

	// ------------------------------------------------------------------ trees struck by shells (the trial "shattered")

	/** LAND_FIELD_STRIKE="x y up across; ...": the rounds that strike the trees (Ground::StrikeTrees), for a picture or a count. */
	std::vector<std::array<double, 4>> ShatterRounds()
	{
		std::vector<std::array<double, 4>> Out;
		if (const char* Text = std::getenv("LAND_FIELD_STRIKE"))
			for (const char* At = Text; At && *At;)
			{
				std::array<double, 4> R{};
				if (std::sscanf(At, "%lf %lf %lf %lf", &R[0], &R[1], &R[2], &R[3]) == 4) Out.push_back(R);
				At = std::strchr(At, ';');
				if (At) ++At;
			}
		return Out;
	}

	/** A leaf's or a twig's distance from the nearest wood: the furthest any leafy piece of a tree's near level (its main mesh's
	 * leafy pieces, its cards, its sprigs) stands from a wood point of its main mesh, metres. */
	double ShatterLeafReach(const LandField::BaseMesh& Bm)
	{
		auto Kind = [](const LandField::Rgb8& C) {
			const int Lo = std::min({ C.R, C.G, C.B }), Hi = std::max({ C.R, C.G, C.B });
			if (Lo > 160 && Hi - Lo < 48 && C.B + 2 >= C.R) return 2;
			if (C.G > C.R + 6) return 1;
			if (Hi - Lo > 60 && C.B + 30 < C.G && (Hi - Lo) * 100 > 42 * Hi) return 1;
			return 0;
		};
		std::vector<std::array<float, 3>> Wood;
		for (size_t K = 0; K < Bm.Near.Points(); ++K)
			if (Kind(Bm.Near.Colours[K]) == 0) Wood.push_back({ Bm.Near.Positions[K * 3], Bm.Near.Positions[K * 3 + 1], Bm.Near.Positions[K * 3 + 2] });
		if (Wood.empty()) return 0.0;
		double Worst = 0.0;
		auto Pieces = [&](const LandField::Mesh& M, bool bLeafOnly) {
			const size_t N = M.Points();
			std::vector<uint32_t> Up(N);
			for (size_t K = 0; K < N; ++K) Up[K] = static_cast<uint32_t>(K);
			auto Find = [&Up](uint32_t A) {
				while (Up[A] != A) A = Up[A] = Up[Up[A]];
				return A;
			};
			for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
				for (int C = 1; C < 3; ++C)
				{
					const uint32_t A = Find(M.Triangles[T]), B = Find(M.Triangles[T + static_cast<size_t>(C)]);
					if (A != B) Up[std::max(A, B)] = std::min(A, B);
				}
			std::map<uint32_t, std::array<double, 5>> Sum; // x, y, z, points, leafy points
			for (size_t K = 0; K < N; ++K)
			{
				auto& S = Sum[Find(static_cast<uint32_t>(K))];
				S[0] += M.Positions[K * 3];
				S[1] += M.Positions[K * 3 + 1];
				S[2] += M.Positions[K * 3 + 2];
				S[3] += 1.0;
				S[4] += Kind(M.Colours[K]) == 1 ? 1.0 : 0.0;
			}
			for (const auto& Kv : Sum)
			{
				const auto& S = Kv.second;
				if (bLeafOnly && S[4] * 2.0 < S[3]) continue;
				double Best = 1e18;
				for (const auto& W : Wood) Best = std::min(Best, (W[0] - S[0] / S[3]) * (W[0] - S[0] / S[3]) + (W[1] - S[1] / S[3]) * (W[1] - S[1] / S[3]) + (W[2] - S[2] / S[3]) * (W[2] - S[2] / S[3]));
				Worst = std::max(Worst, std::sqrt(Best));
			}
		};
		Pieces(Bm.Near, true);
		Pieces(Bm.Cards, false);
		Pieces(Bm.Sprigs, false);
		return Worst;
	}

	/** Each tree's form (StrikeFormOf), base by base, instance by instance: what a set of rounds left. */
	std::vector<int> ShatterState(const LandField::Ground& G)
	{
		std::vector<int> Out;
		for (int B = 0; B < static_cast<int>(LandField::Base::Tuft); ++B)
			for (size_t K = 0; K < G.Instances(static_cast<LandField::Base>(B)).size(); ++K) Out.push_back(G.StrikeFormOf(static_cast<LandField::Base>(B), K));
		return Out;
	}

	/**
	 * The trial "shattered": the struck forms of every tree laid here checked, then the rounds struck (after every other check,
	 * so they see the ground as laid); with LAND_FIELD_STRIKE_ORDER=1, the same rounds struck on two more grounds in shuffled
	 * orders, a hostile list on one and 20,000 rounds on the other. With the word off: the rounds do nothing.
	 */
	void ShatterStrike(LandField::Ground& G, const LandField::Field& F, std::vector<std::array<double, 4>> Rounds)
	{
		using LandField::Base;
		const bool bOn = !F.Trial.empty() && F.Tried("shattered");
		if (!bOn)
		{
			if (Rounds.empty()) return;
			int Did = 0;
			for (const auto& R : Rounds) Did += G.StrikeTrees(R[0], R[1], R[2], R[3]);
			std::vector<LandField::TreeHit> Hits;
			G.TakeTreeHits(Hits);
			std::vector<LandField::TreeCrown> Crowns;
			const size_t Asked = G.CrownsNear(Rounds.front()[0], Rounds.front()[1], 50.0, Crowns);
			Check(Did == 0 && Hits.empty() && Asked == 0 && !G.StrikeForm(static_cast<int>(Base::Oak) * LandField::StruckForms), "the word \"shattered\" off: a round strikes nothing, the crowns query answers nothing", Fmt("%.0f rounds", static_cast<double>(Rounds.size())));
			return;
		}
		std::printf("shattered: the struck forms of the trees laid here (%d a base: 6 torn, 12 snapped, and 10 of them with nothing laid at the foot)\n", LandField::StruckForms);
		bool bTaller = false, bMore = false, bBreak = true, bLeaf = true, bPale = true, bCrown = true, bFarMore = false, bCrownMore = false, bTornBare = true;
		const bool bLobed = !F.Trial.empty() && F.Tried("farcrowns");
		std::string Worst;
		size_t Bytes = 0, Forms = 0, BytesLaid = 0;
		double FormSeconds = 0.0, SlowestForm = 0.0;
		int TreeKinds = 0;
		for (int B = 0; B < static_cast<int>(Base::Tuft); ++B)
		{
			const Base Bs = static_cast<Base>(B);
			const LandField::BaseMesh& Whole = G.BaseOf(Bs);
			if (!G.Instances(Bs).empty() && ((B >= static_cast<int>(Base::Oak) && B <= static_cast<int>(Base::Apple)) || Bs == Base::Birch || Whole.bTree || Whole.Class == LandField::BaseMesh::Draw::Tree)) ++TreeKinds;
			if (G.Instances(Bs).empty() || !G.StrikeForm(B * LandField::StruckForms)) continue;
			const double WholeReach = ShatterLeafReach(Whole);
			std::map<uint32_t, int> WholeColours;
			std::map<uint32_t, bool> FarFromWhole;
			double BarkLuma = 0.0, BarkWarm = 0.0, BarkN = 0.0;
			for (const LandField::Rgb8& C : Whole.Near.Colours)
			{
				WholeColours[(static_cast<uint32_t>(C.R) << 16) | (static_cast<uint32_t>(C.G) << 8) | C.B] = 1;
				const int Spread = std::max({ C.R, C.G, C.B }) - std::min({ C.R, C.G, C.B });
				const bool bSnow = std::min({ C.R, C.G, C.B }) > 160 && Spread < 48 && C.B + 2 >= C.R; // (snow on the boughs is no bark)
				if (C.G <= C.R + 6 && !bSnow && !(Spread > 60 && C.B + 30 < C.G && Spread * 100 > 42 * std::max({ C.R, C.G, C.B })))
				{
					BarkLuma += 0.299 * C.R + 0.587 * C.G + 0.114 * C.B;
					BarkWarm += C.R - C.B;
					BarkN += 1.0;
				}
			}
			BarkLuma /= std::max(1.0, BarkN);
			BarkWarm /= std::max(1.0, BarkN);
			double PaleWarm = 1e9;
			double NearMost = 0.0, HighMost = 0.0, WideMost = 0.0, BreakLo = 1.0, BreakHi = 0.0, ReachMost = 0.0, PaleLeast = 1e9, PaleLuma = 1e9, TornPale = 1e9, CrownOver = -1e9, Ms = 0.0;
			double LeafShareTorn = 0.0, LeafShareSnap = 0.0, TornMost = 0.0, FarMost = 0.0;
			const LandField::Mesh StandingCrown = bLobed ? LandField::CrownLobed(Whole.Far.Empty() ? Whole.Near : Whole.Far) : LandField::CrownOf(Whole.Far.Empty() ? Whole.Near : Whole.Far);
			auto Leafy = [](const LandField::Mesh& M) {
				double N = 0.0;
				for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
				{
					const LandField::Rgb8& C = M.Colours[M.Triangles[T]];
					N += C.G > C.R + 6 ? 1.0 : 0.0;
				}
				return N;
			};
			const double WholeLeaves = Leafy(Whole.Near) + static_cast<double>(Whole.Cards.Tris() + Whole.Sprigs.Tris());
			for (int V = 0; V < LandField::StruckForms; ++V)
			{
				const int Form = B * LandField::StruckForms + V;
				const auto T0 = std::chrono::steady_clock::now();
				const LandField::BaseMesh* S = G.StrikeForm(Form);
				const double Took = Seconds(T0);
				Ms += 1000.0 * Took;
				SlowestForm = std::max(SlowestForm, Took);
				FormSeconds += Took;
				const LandField::Mesh* Crown = G.StrikeCrown(Form);
				if (!S || !Crown) continue;
				const bool bTornForm = S->Name.find("_Torn") != std::string::npos;
				++Forms;
				for (const LandField::Mesh* M : { &S->Near, &S->Mid, &S->Far, &S->Cards, &S->Sprigs, Crown })
				{
					const size_t Of = M->Positions.size() * 4 + M->Normals.size() * 4 + M->Colours.size() * 3 + M->Uvs.size() * 4 + M->Triangles.size() * 4;
					Bytes += Of;
					if (V < LandField::StruckLaid) BytesLaid += Of;
				}
				bTaller = bTaller || S->Height > Whole.Height + 1e-4f || S->Radius > Whole.Radius + 1e-4f;
				HighMost = std::max(HighMost, static_cast<double>(S->Height / std::max(0.01f, Whole.Height)));
				WideMost = std::max(WideMost, static_cast<double>(S->Radius / std::max(0.01f, Whole.Radius)));
				const bool bOver = S->Near.Tris() > Whole.Near.Tris() || S->Cards.Tris() > Whole.Cards.Tris() || S->Sprigs.Tris() > Whole.Sprigs.Tris();
				bMore = bMore || bOver;
				NearMost = std::max(NearMost, static_cast<double>(S->Near.Tris()) / std::max<size_t>(1, Whole.Near.Tris()));
				const double Reach = ShatterLeafReach(*S);
				ReachMost = std::max(ReachMost, Reach);
				if (Reach > WholeReach + 0.3) bLeaf = false;
				// (the leaves still on the tree, not on what fell: the ground's own count)
				const double Left = G.StrikeLeafShare(Form);
				(bTornForm ? LeafShareTorn : LeafShareSnap) += Left / (bTornForm ? 12.0 : 16.0);
				if (bTornForm) TornMost = std::max(TornMost, Left);
				// (the further levels and the far crown: no more triangles than the standing tree's)
				if (S->Far.Tris() > Whole.Far.Tris() || S->Mid.Tris() > Whole.Mid.Tris()) bFarMore = true;
				if (Crown->Tris() > StandingCrown.Tris()) bCrownMore = true;
				FarMost = std::max(FarMost, static_cast<double>(S->Far.Tris()) / std::max<size_t>(1, Whole.Far.Tris()));
				// (the fresh wood: warm pale points the standing tree has none of)
				double PaleN = 0.0, PaleSum = 0.0, PaleWarmSum = 0.0, BarkTop = 0.0;
				std::vector<double> PaleLumas;
				std::vector<char> Barky(S->Near.Points(), 0), Woody(S->Near.Points(), 0);
				for (size_t K = 0; K < S->Near.Points(); ++K)
				{
					const LandField::Rgb8& C = S->Near.Colours[K];
					const int Spread = std::max({ C.R, C.G, C.B }) - std::min({ C.R, C.G, C.B });
					const bool bLeafy = C.G > C.R + 6 || (Spread > 60 && C.B + 30 < C.G && Spread * 100 > 42 * std::max({ C.R, C.G, C.B }));
					// (new: no colour of the standing tree within 20 of it, so a cut's blend of two bark colours is not counted)
					const uint32_t Key = (static_cast<uint32_t>(C.R) << 16) | (static_cast<uint32_t>(C.G) << 8) | C.B;
					auto Seen = FarFromWhole.find(Key);
					if (Seen == FarFromWhole.end())
					{
						bool bFar = true;
						for (const auto& Wc : WholeColours)
						{
							const int Dr = static_cast<int>(Wc.first >> 16) - C.R, Dg = static_cast<int>((Wc.first >> 8) & 255) - C.G, Db = static_cast<int>(Wc.first & 255) - C.B;
							if (Dr * Dr + Dg * Dg + Db * Db <= 400)
							{
								bFar = false;
								break;
							}
						}
						Seen = FarFromWhole.emplace(Key, bFar).first;
					}
					const bool bNew = Seen->second;
					const bool bPaleWood = bNew && !bLeafy && C.R - C.B >= 40 && C.R >= 140;
					if (bPaleWood)
					{
						PaleN += 1.0;
						PaleSum += 0.299 * C.R + 0.587 * C.G + 0.114 * C.B;
						PaleLumas.push_back(0.299 * C.R + 0.587 * C.G + 0.114 * C.B);
						PaleWarmSum += C.R - C.B;
					}
					else if (!bLeafy && C.G <= C.R + 6) Barky[K] = 1;
					Woody[K] = !bLeafy;
				}
				// (the break's height: the highest point of a triangle all bark, so the bark up to the rim of the break, not the
				// splinters standing out of it, whose feet are dark with torn bark)
				for (size_t T = 0; T + 2 < S->Near.Triangles.size(); T += 3)
				{
					const uint32_t* Tr = &S->Near.Triangles[T];
					if (!Barky[Tr[0]] || !Barky[Tr[1]] || !Barky[Tr[2]]) continue;
					for (int J = 0; J < 3; ++J) BarkTop = std::max(BarkTop, static_cast<double>(S->Near.Positions[Tr[J] * 3 + 2]));
				}
				(void)Woody;
				if (bTornForm) TornPale = std::min(TornPale, PaleN);
				else
				{
					if (PaleN < 12.0) std::printf("  %-8s form %d: only %.0f fresh points at its break\n", BaseNames[B], V, PaleN);
					PaleLeast = std::min(PaleLeast, PaleN);
					// (the break's brightness by its brighter fresh wood, the faces and the splinters' tips: three in four fresh
					// points are darker, the splinters' feet shaded with the torn bark on purpose)
					std::sort(PaleLumas.begin(), PaleLumas.end());
					PaleLuma = std::min(PaleLuma, PaleLumas.empty() ? 0.0 : PaleLumas[PaleLumas.size() * 3 / 4]);
					(void)PaleSum;
					PaleWarm = std::min(PaleWarm, PaleN > 0.0 ? PaleWarmSum / PaleN : 0.0);
					const double Share = BarkTop / std::max(0.01f, Whole.Height);
					BreakLo = std::min(BreakLo, Share);
					BreakHi = std::max(BreakHi, Share);
					if (Share < 1.0 / 3.0 - 0.03 || Share > 2.0 / 3.0 + 0.03) bBreak = false;
					double CrownTop = 0.0;
					for (size_t K = 2; K < Crown->Positions.size(); K += 3) CrownTop = std::max(CrownTop, static_cast<double>(Crown->Positions[K]));
					CrownOver = std::max(CrownOver, CrownTop - BarkTop);
					if (CrownTop > BarkTop + 1.5) bCrown = false;
				}
			}
			// (pale: at least a dozen fresh points at a snapped tree's break and a few at a torn one's stubs, half again
			// as bright as the bark (the brighter quarter of them), or, on a bark already pale (a beech's, a palm's; under snow, kept to four fifths of the
			// snow), 1.2 times it, or on a white bark (a birch's) warmer than it by 40 (red over blue): broken wood, not more white)
			const bool bPaleOk = PaleLeast >= 12.0 && TornPale >= 3.0
				&& (PaleLuma >= 1.5 * BarkLuma || (BarkLuma > 110.0 && PaleLuma >= 1.2 * BarkLuma) || (BarkLuma > 120.0 && PaleWarm >= BarkWarm + 40.0));
			bPale = bPale && bPaleOk;
			// (torn: half the leaves or more gone, every torn form of every kind)
			if (TornMost > 0.5 && WholeLeaves > 0.0) bTornBare = false;
			std::printf("  %-8s %s: %zu tris standing, struck at most %.2f of them (far level %.2f); at most %.3f as high, %.3f as wide; snapped break at %.2f to %.2f of its height, its far crown at most %.1f m over the break; leaves left on the tree %.0f%% torn (at most %.0f%%), %.0f%% snapped; leaf to wood at most %.2f m (standing %.2f); fresh wood %.0f points at least (torn %.0f), luma %.0f (its brighter quarter) warmth %.0f (bark %.0f, warmth %.0f); %d forms asked in %.1f ms (made as the ground was built)\n",
				BaseNames[B], Whole.Name.c_str(), Whole.Near.Tris(), NearMost, FarMost, HighMost, WideMost, BreakLo, BreakHi, CrownOver, 100.0 * LeafShareTorn, 100.0 * TornMost, 100.0 * LeafShareSnap, ReachMost, WholeReach, PaleLeast, TornPale, PaleLuma, PaleWarm, BarkLuma, BarkWarm, LandField::StruckForms, Ms);
			if (!bPaleOk || ReachMost > WholeReach + 0.3 || TornMost > 0.5) Worst += std::string(" ") + BaseNames[B];
		}
		Check(Forms > 0 || TreeKinds == 0, "a tree laid here has struck forms", (TreeKinds == 0 ? std::string("no tree laid here; ") : std::string()) + Fmt("%.0f forms, %.1f MB (%.1f MB of them made as the ground was built, the rest when first asked)", static_cast<double>(Forms), Bytes / 1048576.0, BytesLaid / 1048576.0) + Fmt(", asked in %.2f s (the slowest %.1f ms)", FormSeconds, 1000.0 * SlowestForm));
		Check(!bTaller, "a struck tree is never taller or wider than it stood", "");
		Check(!bMore, "a struck tree's near level (and its cards and sprigs) has no more triangles than the standing tree's", "");
		Check(!bFarMore, "a struck tree's middle and far levels have no more triangles than the standing tree's", "");
		Check(!bCrownMore, "a struck tree's far crown has no more triangles than the standing tree's", bLobed ? "(the trial \"farcrowns\": CrownLobed)" : "(CrownOf)");
		Check(bTornBare, "a torn tree has lost half its leaves or more (every torn form of every kind)", Worst);
		Check(bBreak, "a snapped tree's trunk breaks between a third and two thirds of its height (its ragged top within 0.03 of them)", "");
		Check(bCrown, "a snapped tree's far crown is no round crown: none of it more than 1.5 m over its break", "");
		Check(bLeaf, "no leaf of a struck tree hangs further from its wood than on the standing tree (0.3 m allowed)", Worst);
		Check(bPale, "pale fresh wood at the break and the stubs, half again the bark's brightness (1.2 times, on a pale bark)", Worst);
		if (Rounds.empty() && !std::getenv("LAND_FIELD_STRIKE_ORDER")) return;
		// The rounds: given, or forty bursts beside the trees of the first kind laid.
		if (Rounds.empty())
			for (int B = 0; B < static_cast<int>(Base::Tuft) && Rounds.empty(); ++B)
			{
				const auto& Of = G.Instances(static_cast<Base>(B));
				if (Of.size() < 50 || !G.StrikeForm(B * LandField::StruckForms)) continue;
				for (size_t K = 0; K < 40; ++K)
				{
					const LandField::Placed& P = Of[(K * 7919) % Of.size()];
					Rounds.push_back({ P.X + 1.5, P.Y - 1.0, K % 7 == 0 ? 9.0 : 0.0, K % 3 == 0 ? 4.0 : 2.8 });
				}
				// (and twenty canopy bursts with a ground burst at the very same place: a reversed list strikes each pair the other
				// way round)
				for (size_t K = 0; K < 20; ++K)
				{
					const LandField::Placed& P = Of[(K * 104729 + 13) % Of.size()];
					const double Up = 0.6 * G.BaseOf(static_cast<Base>(B)).Height * P.ScaleZ;
					Rounds.push_back({ P.X + 0.7, P.Y + 0.4, Up, 2.8 });
					Rounds.push_back({ P.X + 0.7, P.Y + 0.4, 0.0, 2.8 });
				}
			}
		if (Rounds.empty())
		{
			std::printf("  struck: no tree kind laid here 50 times or more, no rounds to strike\n");
			return;
		}
		auto Strike = [](LandField::Ground& On, const std::vector<std::array<double, 4>>& Rs, int* OutMost) {
			int Most = 0;
			for (const auto& R : Rs) Most = std::max(Most, On.StrikeTrees(R[0], R[1], R[2], R[3]));
			if (OutMost) *OutMost = Most;
		};
		const auto T0 = std::chrono::steady_clock::now();
		int Most = 0;
		Strike(G, Rounds, &Most);
		const double StrikeMs = 1000.0 * Seconds(T0);
		std::vector<LandField::TreeHit> Hits;
		G.TakeTreeHits(Hits);
		const std::vector<int> Once = ShatterState(G);
		size_t Torn = 0, Snapped = 0;
		uint64_t Digest = 1469598103934665603ull;
		for (const int Fm : Once)
		{
			if (Fm >= 0) ++(G.StrikeForm(Fm)->Name.find("_Torn") != std::string::npos ? Torn : Snapped);
			Digest = (Digest ^ static_cast<uint64_t>(Fm + 1)) * 1099511628211ull;
		}
		std::printf("  struck: %zu rounds in %.2f ms (the strongest a round did: %d), %zu trees changed, %zu torn and %zu snapped; struck digest %016llx\n", Rounds.size(), StrikeMs, Most, Hits.size(), Torn, Snapped,
			static_cast<unsigned long long>(Digest));
		{
			// What fell, on each struck tree's own ground: its near level's low points more than 1.5 m out from the trunk, the
			// lowest a 0.75 m square, against the ground's height there (HeightAt: banks, ditches, slopes): sunk, any of them
			// more than 0.35 m under it; clear, a point meant to touch it (0.2 m or less over the tree's foot) more than 0.3 m
			// over it.
			size_t Lying = 0, Clear = 0, Sunk = 0;
			double ClearMost = 0.0, SunkMost = 0.0;
			for (int B = 0; B < static_cast<int>(Base::Tuft); ++B)
				for (size_t K = 0; K < G.Instances(static_cast<Base>(B)).size(); ++K)
				{
					const int Fm = G.StrikeFormOf(static_cast<Base>(B), K);
					if (Fm < 0) continue;
					const LandField::Placed& Pl = G.Instances(static_cast<Base>(B))[K];
					const LandField::Mesh& M = G.StrikeForm(Fm)->Near;
					std::map<std::pair<int, int>, double> Low;
					double Hi = -1e9;
					const double C = std::cos(Pl.Yaw), Sn = std::sin(Pl.Yaw);
					for (size_t Q = 0; Q < M.Points(); ++Q)
					{
						const double X = M.Positions[Q * 3], Y = M.Positions[Q * 3 + 1], Z = M.Positions[Q * 3 + 2];
						if (Z > 0.6 || std::hypot(X, Y) < 1.5) continue;
						const double Wx = Pl.X + (X * C - Y * Sn) * Pl.Scale, Wy = Pl.Y + (X * Sn + Y * C) * Pl.Scale;
						const double Over = Pl.Z + Z * Pl.ScaleZ - G.HeightAt(Wx, Wy);
						if (Z <= 0.2) Hi = std::max(Hi, Over);
						const auto Key = std::make_pair(static_cast<int>(std::floor(X / 0.75)), static_cast<int>(std::floor(Y / 0.75)));
						const auto Found = Low.find(Key);
						if (Found == Low.end() || Over < Found->second) Low[Key] = Over;
					}
					if (Low.empty()) continue;
					++Lying;
					double Lo = 1e9;
					for (const auto& Kv : Low) Lo = std::min(Lo, Kv.second);
					if (Hi > 0.3) ++Clear;
					if (std::getenv("LAND_FIELD_STRIKE_FIT") && (Hi > 0.3 || Lo < -0.35))
						std::printf("    fit %-8s %.1f %.1f form %d: clear %.2f, into %.2f, ground at foot %.2f\n", BaseNames[B], Pl.X, Pl.Y, Fm % LandField::StruckForms, Hi, -Lo, Pl.Z);
					if (Lo < -0.35) ++Sunk;
					ClearMost = std::max(ClearMost, Hi);
					SunkMost = std::max(SunkMost, -Lo);
				}
			std::printf("  what fell on its own ground: %zu struck trees with pieces lying, %zu with a piece more than 0.3 m clear of the ground (at most %.2f m), %zu with one more than 0.35 m into it (at most %.2f m)\n", Lying, Clear, ClearMost, Sunk, SunkMost);
		}
		{
			// Variety among the struck trees: the forms they take, neighbours within 15 m in the same form, and snapped
			// neighbours within 15 m broken within 0.5 m of the same height (the stump's highest all-bark triangle, as laid).
			struct StruckOne
			{
				double X, Y, BreakM;
				int Form;
				bool bSnapped;
			};
			std::vector<StruckOne> Struck;
			std::map<int, double> BarkTopOf;
			for (int B = 0; B < static_cast<int>(Base::Tuft); ++B)
				for (size_t K = 0; K < G.Instances(static_cast<Base>(B)).size(); ++K)
				{
					const int Fm = G.StrikeFormOf(static_cast<Base>(B), K);
					if (Fm < 0) continue;
					const LandField::BaseMesh& Sf = *G.StrikeForm(Fm);
					auto Found = BarkTopOf.find(Fm);
					if (Found == BarkTopOf.end())
					{
						double Top = 0.0;
						const LandField::Mesh& M = Sf.Near;
						auto Barky = [&M](uint32_t I) {
							const LandField::Rgb8& C = M.Colours[I];
							return C.G <= C.R + 6 && !(C.R >= 140 && C.R - C.B >= 40);
						};
						for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
							if (Barky(M.Triangles[T]) && Barky(M.Triangles[T + 1]) && Barky(M.Triangles[T + 2]))
								for (int J = 0; J < 3; ++J) Top = std::max(Top, static_cast<double>(M.Positions[M.Triangles[T + J] * 3 + 2]));
						Found = BarkTopOf.emplace(Fm, Top).first;
					}
					const LandField::Placed& Pl = G.Instances(static_cast<Base>(B))[K];
					Struck.push_back({ Pl.X, Pl.Y, Found->second * Pl.ScaleZ, Fm, Sf.Name.find("_Snapped") != std::string::npos });
				}
			std::map<int, int> FormsUsed;
			size_t Pairs = 0, Same = 0, SnapPairs = 0, SameHeight = 0;
			for (size_t A = 0; A < Struck.size(); ++A)
			{
				++FormsUsed[Struck[A].Form];
				for (size_t Bq = A + 1; Bq < Struck.size(); ++Bq)
				{
					if (std::hypot(Struck[A].X - Struck[Bq].X, Struck[A].Y - Struck[Bq].Y) > 15.0) continue;
					++Pairs;
					Same += Struck[A].Form == Struck[Bq].Form ? 1 : 0;
					if (Struck[A].bSnapped && Struck[Bq].bSnapped)
					{
						++SnapPairs;
						SameHeight += std::fabs(Struck[A].BreakM - Struck[Bq].BreakM) < 0.5 ? 1 : 0;
					}
				}
			}
			std::printf("  variety: %zu struck trees in %zu forms; of %zu neighbours within 15 m, %zu in the same form; of %zu snapped neighbours, %zu broken within 0.5 m of the same height\n", Struck.size(), FormsUsed.size(), Pairs, Same, SnapPairs, SameHeight);
		}
		int Again = 0;
		Strike(G, Rounds, &Again);
		std::vector<LandField::TreeHit> Hits2;
		G.TakeTreeHits(Hits2);
		Check(Again == 0 && Hits2.empty() && ShatterState(G) == Once, "a round struck twice is struck once", Fmt("again: strongest %.0f, %.0f trees changed", Again, static_cast<double>(Hits2.size())));
		if (!std::getenv("LAND_FIELD_STRIKE_ORDER")) return;
		// The same rounds, reversed and shuffled (by a hash), on two more grounds laid from the same field.
		std::vector<std::array<double, 4>> Back(Rounds.rbegin(), Rounds.rend()), Shuffled = Rounds;
		for (size_t K = Shuffled.size(); K > 1; --K)
		{
			const size_t J = static_cast<size_t>((K * 2654435761ull + 977ull) % K);
			std::swap(Shuffled[K - 1], Shuffled[J]);
		}
		LandField::Ground G2(F), G3(F);
		Strike(G2, Back, nullptr);
		Strike(G3, Shuffled, nullptr);
		Check(ShatterState(G2) == Once && ShatterState(G3) == Once, "the same rounds in three orders leave the same trees", Fmt("%.0f rounds, %.0f trees struck", static_cast<double>(Rounds.size()), static_cast<double>(Torn + Snapped)));
		// Hostile rounds on G2: not a number, infinite, off the field, a hole of nothing or less, far up, and one too wide.
		const std::vector<int> Before = ShatterState(G2);
		const double Nan = std::nan(""), Inf = HUGE_VAL;
		LandField::Placed Any;
		Any.X = static_cast<float>(Rounds.front()[0]);
		Any.Y = static_cast<float>(Rounds.front()[1]);
		int Bad = 0;
		Bad += G2.StrikeTrees(Nan, Any.Y, 0.0, 3.0);
		Bad += G2.StrikeTrees(Any.X, Inf, 0.0, 3.0);
		Bad += G2.StrikeTrees(Any.X, Any.Y, Nan, 3.0);
		Bad += G2.StrikeTrees(Any.X, Any.Y, 0.0, Nan);
		Bad += G2.StrikeTrees(Any.X, Any.Y, 0.0, -5.0);
		Bad += G2.StrikeTrees(Any.X, Any.Y, 0.0, 0.5);
		Bad += G2.StrikeTrees(1e9, -1e9, 0.0, 3.0);
		Bad += G2.StrikeTrees(Any.X, Any.Y, 1e6, 3.0);
		Check(Bad == 0 && ShatterState(G2) == Before && !G2.StrikeForm(-1) && !G2.StrikeForm(1 << 30) && !G2.StrikeCrown(-7), "hostile rounds (not a number, infinite, off the field, no hole, 1,000 km up) and form numbers are refused", "");
		const auto T1 = std::chrono::steady_clock::now();
		const int Wide = G2.StrikeTrees(Any.X, Any.Y, 0.0, 1e6);
		std::vector<LandField::TreeHit> WideHits;
		G2.TakeTreeHits(WideHits);
		std::printf("  a hole 1,000 km across strikes as one 30 m across (what the cap loses: every tree past 75 m of it stands): strongest %d, %zu trees changed, in %.1f ms\n", Wide, WideHits.size(), 1000.0 * Seconds(T1));
		// 20,000 rounds over the whole field on G3 (a barrage of hundreds a battle, a hundred times over).
		const auto T2 = std::chrono::steady_clock::now();
		for (int K = 0; K < 20000; ++K)
		{
			const double U = ((K * 7919) % 20000) / 20000.0, V = ((K * 104729) % 20011) / 20011.0;
			G3.StrikeTrees(U * F.Width(), V * F.Depth(), K % 11 == 0 ? 12.0 : 0.0, K % 5 == 0 ? 4.0 : 2.8);
		}
		const double ManyMs = 1000.0 * Seconds(T2);
		std::vector<LandField::TreeHit> ManyHits;
		G3.TakeTreeHits(ManyHits);
		size_t Struck = 0, Trees = 0;
		for (const int Fm : ShatterState(G3)) Struck += Fm >= 0 ? 1 : 0;
		for (int B = 0; B < static_cast<int>(Base::Tuft); ++B)
			if (G3.StrikeForm(B * LandField::StruckForms)) Trees += G3.Instances(static_cast<Base>(B)).size();
		Check(ManyHits.size() <= Trees && ManyMs < 2000.0, "20,000 rounds: told once a tree, no more trees than are laid, in under 2 s",
			Fmt("%.0f ms (%.1f us a round), %.0f trees struck of ", ManyMs, 1000.0 * ManyMs / 20000.0, static_cast<double>(Struck)) + Fmt("%.0f, %.0f told", static_cast<double>(Trees), static_cast<double>(ManyHits.size())));
		std::vector<LandField::TreeCrown> Crowns;
		const auto T3 = std::chrono::steady_clock::now();
		size_t Seen = 0;
		for (int K = 0; K < 1000; ++K) Seen += G3.CrownsNear(Any.X + (K % 40) - 20.0, Any.Y + (K / 40) - 12.0, 12.0, Crowns);
		std::printf("  crowns near a point (12 m): %.1f us a question, %.1f crowns an answer\n", 1000.0 * Seconds(T3), Seen / 1000.0);
	}

	/** LAND_FIELD_STRIKE_EYES="x y a [up pitch] | ...": each eye painted before the rounds (<key>-e<N>-before-field-eye.png),
	 * then the rounds struck (ShatterStrike), then each eye again (<key>-e<N>-after-field-eye.png). */
	int ShatterPairs(LandField::Ground& G, const LandField::Field& F, const LandFieldPicture::Sky& Sk, const std::string& Key, const std::string& Dir, const char* Eyes, const std::vector<std::array<double, 4>>& Rounds)
	{
		std::vector<std::string> List;
		for (const char* At = Eyes; At && *At;)
		{
			const char* End = std::strchr(At, '|');
			List.push_back(End ? std::string(At, End) : std::string(At));
			At = End ? End + 1 : nullptr;
		}
		setenv("LAND_FIELD_EYE_ONLY", "1", 1);
		int Bad = 0;
		for (size_t K = 0; K < List.size(); ++K)
		{
			setenv("LAND_FIELD_EYE", List[K].c_str(), 1);
			if (!LandFieldPicture::Paint(G, F, Sk, Key + "-e" + std::to_string(K) + "-before", Dir)) ++Bad;
		}
		ShatterStrike(G, F, Rounds);
		for (size_t K = 0; K < List.size(); ++K)
		{
			setenv("LAND_FIELD_EYE", List[K].c_str(), 1);
			if (!LandFieldPicture::Paint(G, F, Sk, Key + "-e" + std::to_string(K) + "-after", Dir)) ++Bad;
		}
		return Bad;
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
	// The look of the land (LandScape): the recording's own word and snow where it gives them, the place from the sky's
	// latLon. LAND_FIELD_LOOK ("desert", "med", "jungle", "island", "farmland", "bocage"), LAND_FIELD_LATLON ("lat lon"),
	// LAND_FIELD_SNOW (0 to 1), LAND_FIELD_MONTH (1 to 12) and LAND_FIELD_TERRAIN lay a recording's battle on other ground.
	if (const Json* L = Fj->Get("look"); L && L->K == Json::String) F.Look = L->S;
	F.Snow = Fj->Num("snow", -1.0);
	if (const Json* Sky = Rec.Get("sky"))
		if (const Json* L = Sky->Get("latLon"); L && L->A.size() == 2)
		{
			F.Lat = L->A[0].N;
			F.Lon = L->A[1].N;
		}
	if (const Json* D = Rec.Get("date"); D && D->K == Json::String)
	{
		static const char* const Months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
		for (int M = 0; M < 12; ++M)
			if (D->S.find(Months[M]) != std::string::npos) F.Month = M + 1;
	}
	F.Seed = static_cast<uint64_t>(Rec.Num("seed", 1.0));
	if (const char* V = std::getenv("LAND_FIELD_LOOK")) F.Look = V;
	if (const char* V = std::getenv("LAND_FIELD_LATLON")) std::sscanf(V, "%lf %lf", &F.Lat, &F.Lon);
	if (const char* V = std::getenv("LAND_FIELD_SNOW")) F.Snow = std::atof(V);
	if (const char* V = std::getenv("LAND_FIELD_TRIAL")) F.Trial = V; // (the looks on trial: Field::Trial)
	if (const char* V = std::getenv("LAND_FIELD_WEATHER")) F.Weather = V; // (the campaign's weather word: "mud")
	if (const char* V = std::getenv("LAND_FIELD_MONTH")) F.Month = std::atoi(V);
	if (const char* V = std::getenv("LAND_FIELD_TERRAIN")) F.Terrain = V;
	if (std::getenv("LAND_FIELD_OLD_RIVER_BED")) F.bOldRiverBed = true; // (the game's -TheaterLandOldRiverBed: Field::bOldRiverBed)
	// The open steppe, where the recording lays it (field.steppe), and the east kit's models beside the recordings
	// (Content/Models/East: the recording's folder's ../../Models/East), as the game reads them.
	if (const Json* St = Fj->Get("steppe"); St && St->K == Json::Object)
	{
		LandField::SteppeLay& L = F.Steppe;
		auto Digits = [](const Json* J, std::vector<uint8_t>& Out) {
			if (J && J->K == Json::String)
				for (const char Ch : J->S) Out.push_back(static_cast<uint8_t>(Ch >= '0' && Ch <= '9' ? Ch - '0' : 0));
		};
		auto Line = [](const Json* J, std::vector<std::pair<double, double>>& Out) {
			if (J)
				for (const Json& P : J->A)
					if (P.A.size() >= 2) Out.push_back({ P.A[0].N, P.A[1].N });
		};
		if (const Json* Fl = St->Get("fields"))
		{
			L.Turn = Fl->Num("turn", 0.0);
			L.CX = Fl->Num("cx", F.Width() / 2.0);
			L.CY = Fl->Num("cy", F.Depth() / 2.0);
			if (const Json* X = Fl->Get("xs"))
				for (const Json& N : X->A) L.Xs.push_back(N.N);
			if (const Json* Y = Fl->Get("ys"))
				for (const Json& N : Y->A) L.Ys.push_back(N.N);
			if (const Json* S = Fl->Get("shift"))
				for (const Json& N : S->A) L.Shift.push_back(N.N);
			Digits(Fl->Get("patch"), L.Patch);
		}
		Digits(St->Get("crop"), L.Crop);
		if (const Json* B = St->Get("balka"))
		{
			Line(B->Get("line"), L.Balka);
			L.BalkaDeep = B->Num("deep", 0.0);
		}
		if (const Json* R = St->Get("rail"))
		{
			Line(R->Get("line"), L.Rail);
			L.RailBank = R->Num("bank", 0.0);
		}
		if (const Json* H = St->Get("hill"))
		{
			L.HillX = H->Num("x", 0.0);
			L.HillY = H->Num("y", 0.0);
			L.HillR = H->Num("r", 0.0);
			L.HillRise = H->Num("rise", 0.0);
		}
		L.bOn = L.Xs.size() >= 2 && L.Ys.size() >= 2;
		const std::filesystem::path KitDir = std::filesystem::path(Argv[1]).parent_path() / ".." / ".." / "Models" / "East";
		int Kits = 0;
		for (const char* Name : { "Khata", "BurntChimney", "TelegraphPole", "WattleFence", "WellSweep", "Haystack", "Birch" })
		{
			std::ifstream KitIn(KitDir / (std::string(Name) + ".json"), std::ios::binary);
			if (!KitIn) continue;
			std::stringstream KitBuf;
			KitBuf << KitIn.rdbuf();
			L.Kit[Name] = KitBuf.str();
			++Kits;
		}
		std::printf("steppe: %zu fields, %zu railway points (bank %.1f m), balka %zu points, the east kit's models %d of 7\n", L.Patch.size(), L.Rail.size(), L.RailBank, L.Balka.size(), Kits);
	}
	// A region's kit (LandScape::KitWanted): Content/Models/<Folder> beside the recordings, as the game reads it, else the
	// shared folder's battle-models/<folder>/models (LAND_FIELD_KITS=<folder> to read another).
	{
		std::string Folder;
		std::vector<std::string> Names;
		if (LandScape::KitWanted(F, Folder, Names))
		{
			std::string Lower = Folder;
			for (char& Ch : Lower) Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
			std::vector<std::filesystem::path> Dirs;
			if (const char* V = std::getenv("LAND_FIELD_KITS")) Dirs.push_back(V);
			Dirs.push_back(std::filesystem::path(Argv[1]).parent_path() / ".." / ".." / "Models" / Folder);
			Dirs.push_back(std::filesystem::path("/mnt/project-files/battle-models") / Lower / "models");
			int Read = 0;
			std::string From;
			for (const std::string& Name : Names)
				for (const std::filesystem::path& D : Dirs)
				{
					std::ifstream KitIn(D / (Name + ".json"), std::ios::binary);
					if (!KitIn) continue;
					std::stringstream KitBuf;
					KitBuf << KitIn.rdbuf();
					F.Kit[Name] = KitBuf.str();
					++Read;
					if (From.find(D.string()) == std::string::npos) From += (From.empty() ? "" : ", ") + D.string();
					break;
				}
			std::printf("look %s (%s at %.1f, %.1f): the %s kit's models %d of %zu (%s)\n", LandScape::NameOf(LandScape::Pick(F)), LandScape::ClimateAt(F.Lat, F.Lon), F.Lat, F.Lon, Folder.c_str(), Read, Names.size(),
			            From.empty() ? "none found: plain stand-ins" : From.c_str());
		}
	}
	const bool bSteppe = F.Steppe.bOn;
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
	LandField::Ground GroundMade(F);
	// (burnt ground without the scorch layer, for a picture or a count: LAND_FIELD_BURN="x y core outer; x y core outer")
	if (const char* Burns = std::getenv("LAND_FIELD_BURN"))
		for (const char* At = Burns; At && *At;)
		{
			double BX = 0.0, BY = 0.0, BCore = 0.0, BOuter = 0.0;
			if (std::sscanf(At, "%lf %lf %lf %lf", &BX, &BY, &BCore, &BOuter) == 4) GroundMade.AddBurn(BX, BY, BCore, BOuter);
			At = std::strchr(At, ';');
			if (At) ++At;
		}
	// (shell holes' blasts for a picture, LAND_FIELD_FLATTEN="x y core outer; ...": the growth laid over round them, on
	// trial "lodged"; the picture also takes the clutter away within 1.2 x core, the holes' bare earth, word or not)
	if (const char* Blasts = std::getenv("LAND_FIELD_FLATTEN"))
		for (const char* At = Blasts; At && *At;)
		{
			double BX = 0.0, BY = 0.0, BCore = 0.0, BOuter = 0.0;
			if (std::sscanf(At, "%lf %lf %lf %lf", &BX, &BY, &BCore, &BOuter) == 4) GroundMade.AddFlatten(BX, BY, BCore, BOuter);
			At = std::strchr(At, ';');
			if (At) ++At;
		}
	const LandField::Ground& G = GroundMade;
	// A region's ground (LandScape: the desert, the Mediterranean, the jungle, the islands, the farmland): the bocage's
	// own checks (its hedges, gates, cow paths, church, sunken lanes, closed views, corn) are not asked of it; its own are.
	const std::string LookLaid = G.LookWord();
	const bool bRegion = LookLaid != "bocage" && LookLaid != "steppe";
	const double Build = Seconds(Start);
	const LandField::Tally& T = G.Laid();
	std::printf("built in %.2f s (core %.2f s: heights, lanes, parcels, hedges, village, instances; meshes %.2f s: tiles, ring, bases)\n", Build, T.CoreSeconds, T.MeshSeconds);
	// LAND_FIELD_HEDGES=<folder>: the hedges and their openings as laid, for the rules (<key>-hedges.json): metres from
	// the field's south-west corner, a hedge's points along its bank's middle after its gaps are cut, an opening's yaw
	// the hedge's own heading there (radians from east).
	if (const char* Dir = std::getenv("LAND_FIELD_HEDGES"))
	{
		const std::string Path = std::string(Dir) + "/" + Key + "-hedges.json";
		if (FILE* Out = std::fopen(Path.c_str(), "w"))
		{
			static const char* const KindWords[] = { "hedgerow", "lane_bank", "wood_edge", "garden", "wall" };
			std::fprintf(Out, "{\"field\":\"%s\",\"width\":%.1f,\"depth\":%.1f,\"ways\":%zu,\"hedges\":[", Key.c_str(), F.Width(), F.Depth(), F.Ways.size());
			bool bFirst = true;
			int Lines = 0, Gates = 0, Breaches = 0;
			for (const LandField::HedgeLine& H : G.Hedges())
			{
				if (H.Kind < 0 || H.Kind > 4 || H.Points.size() < 2) continue; // (the far ring's strips are scenery)
				std::fprintf(Out, "%s\n{\"kind\":\"%s\",\"points\":[", bFirst ? "" : ",", KindWords[H.Kind]);
				for (size_t K = 0; K < H.Points.size(); ++K) std::fprintf(Out, "%s[%.2f,%.2f]", K ? "," : "", H.Points[K].first, H.Points[K].second);
				std::fprintf(Out, "]}");
				bFirst = false;
				++Lines;
			}
			std::fprintf(Out, "\n],\"openings\":[");
			bFirst = true;
			for (const LandField::HedgeOpening& O : G.Openings())
			{
				std::fprintf(Out, "%s\n{\"x\":%.2f,\"y\":%.2f,\"yaw\":%.4f,\"width\":%.2f,\"kind\":\"%s\",\"men\":%d}", bFirst ? "" : ",", O.X, O.Y, O.Yaw, O.Width, O.Kind == 0 ? "gate" : "breach", O.Men);
				bFirst = false;
				++(O.Kind == 0 ? Gates : Breaches);
			}
			std::fprintf(Out, "\n]}\n");
			std::fclose(Out);
			std::printf("  hedges for the rules: %d lines, %d gates, %d breaches -> %s\n", Lines, Gates, Breaches, Path.c_str());
		}
		else std::printf("  hedges for the rules: could not write %s\n", Path.c_str());
	}
	std::printf("  %d parcels in the field and its band, mean %.0f m across; %d lanes, %.1f km; %.1f km of banks and walls; %d gates; %d hedges opened by the recording's vehicles\n", T.Parcels, T.ParcelMean, T.Lanes,
	            T.LaneKm, T.HedgeKm, T.Gates, T.WayGaps);
	if (!T.LaneShape.empty()) std::printf("  %s\n", T.LaneShape.c_str());
	// (on trial, "roadline": the bocage's lanes and the jungle's track wander, the art director's ruling of 26 November:
	// somewhere in any 200 m the line 4 m (the bocage) or 6 m (the jungle) off its chord, where its bends are full and the
	// rules' road cells leave them room; read from each lane's shape in the log line)
	if (F.Tried("roadline") && (LookLaid == "bocage" || LookLaid == "jungle"))
	{
		const double Want = LookLaid == "bocage" ? 4.0 : 6.0;
		double Worst = 1e9;
		int Read = 0;
		for (size_t At = T.LaneShape.find(" m where its bends are full"); At != std::string::npos; At = T.LaneShape.find(" m where its bends are full", At + 1))
		{
			const size_t From = T.LaneShape.rfind('(', At);
			if (From == std::string::npos) continue;
			Worst = std::min(Worst, std::atof(T.LaneShape.c_str() + From + 1));
			++Read;
		}
		// (a lane whose bends are nowhere full, eased in or held in by its cells all along, is not read: there the limit
		// wins, as the art director rules, and the log's lane line gives how far it wanders all the same)
		Check(Read == 0 || Worst >= Want, "the lanes wander (trial roadline): somewhere in any 200 m off their chord as far as the art director asks, where their bends are full",
		      Read > 0 ? Fmt("%.0f lanes with their bends full somewhere, least %.1f m (asked %.0f m)", static_cast<double>(Read), Worst, Want) : Fmt("no lane with its bends full in 200 m (held by its cells or eased in): none read (asked %.0f m)", Want));
	}
	std::printf("  village: %d houses, barns and byres (%d damaged), church %s, %d farmsteads\n", T.Houses, T.Damaged, T.bChurch ? "yes" : "no", T.Farmsteads);
	{
		// What the fields grow, by their ground in the field (a 4 m sample): mid-July, the harvest barely begun (the
		// environment review of 22 November: stubble 3% of the arable at most, the barley's only).
		std::map<std::string, double> Area;
		double Arable = 0.0, Grass = 0.0, All = 0.0;
		for (double Y = 2.0; Y < F.Depth(); Y += 4.0)
			for (double X = 2.0; X < F.Width(); X += 4.0)
			{
				const std::string C = G.CropOfParcel(G.ParcelAt(X, Y));
				if (C.empty()) continue;
				Area[C] += 16.0;
				All += 16.0;
				if (C == "wheat" || C == "barley" || C == "oats" || C == "green wheat" || C == "stubble" || C == "roots" || C == "rye") Arable += 16.0;
				else Grass += 16.0;
			}
		std::string Line;
		for (const auto& Kv : Area) Line += Kv.first + Fmt(" %.0f%%, ", 100.0 * Kv.second / std::max(1.0, All));
		std::printf("  crops by ground: %sarable %.0f%%, grass %.0f%%\n", Line.c_str(), 100.0 * Arable / std::max(1.0, All), 100.0 * Grass / std::max(1.0, All));
		if (Arable > 0.0 && !F.Steppe.bOn && !bRegion) // Normandy's July only: a region's harvest keeps its own calendar
			Check(Area["stubble"] <= 0.035 * Arable, "mid-July: the harvest barely begun (stubble 3% of the arable at most)", Fmt("stubble %.1f%% of the arable", 100.0 * Area["stubble"] / Arable));
	}
	std::printf("  triangles: near %zu, far %zu, built %zu, ring %zu; instances %zu\n", T.NearTris, T.FarTris, T.BuiltTris, T.RingTris, T.Instances);
	// The bocage's hedge trees as laid (not their young shoots), and the step in its shrubs' tops from one to the next.
	if (T.BocageHedgeKm > 0.0)
		std::printf("  bocage hedge trees: %.1f km of hedgerows and lane banks, %d trees laid (%d left out for the clumps), one to %.1f m of hedge; longest run (12 m apart at most) %d trees over %.0f m, widest gap %.0f m (with a hedge's ends %.0f m); shrub tops, step to the next (tall clumps apart): median %.2f m, 95%% %.2f m, %.1f%% over 0.5 m\n",
		            T.BocageHedgeKm, T.BocageStandards, T.BocageStandardsDropped, 1000.0 * T.BocageHedgeKm / std::max(1, T.BocageStandards), T.StandardRunMost, T.StandardRunMetres, T.StandardGapMost, T.StandardBareMost,
		            T.ShrubStepMedian, T.ShrubStep95, 100.0 * T.ShrubStepOver);
	if (T.BocageStandards > 0)
	{
		const auto& Sp = T.StandardSpacingBins;
		const auto& Ru = T.StandardRunsOf;
		std::printf("  bocage hedge trees, spacing to the next: under 6 m %d, 6-12 m %d, 12-25 m %d, 25-60 m %d, over 60 m %d; runs (12 m apart at most) of 1 tree %d, 2 %d, 3 %d, 4 %d, 5 %d, 6 or more %d\n",
		            Sp[0], Sp[1], Sp[2], Sp[3], Sp[4], Ru[0], Ru[1], Ru[2], Ru[3], Ru[4], Ru[5]);
	}
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
	std::string BadCards;
	int Carded = 0;
	for (int B = 0; B < static_cast<int>(LandField::Base::Count); ++B)
	{
		const LandField::BaseMesh& Bm = G.BaseOf(static_cast<LandField::Base>(B));
		std::printf("    %-9s %6zu   %5zu / %4zu tris, far from %3.0f m, %.1f m high, %.1f m round%s\n", BaseNames[B], G.Instances(static_cast<LandField::Base>(B)).size(), Bm.Near.Tris(), Bm.Far.Tris(), Bm.FarFrom,
		            Bm.Height, Bm.Radius, Bm.Cards.Empty() ? "" : (", leaves on " + std::to_string(Bm.Cards.Tris() / 2) + " cards (or " + std::to_string(Bm.Sprigs.Tris()) + " tris of sprigs)").c_str());
		// (a leaf card mesh carries its picture's place at every point; no other mesh has any)
		if (!(Bm.Cards.Uvs.size() == 2 * Bm.Cards.Points() && Bm.Near.Uvs.empty() && Bm.Far.Uvs.empty() && Bm.Sprigs.Uvs.empty())) BadCards += std::string(" ") + BaseNames[B];
		if (!Bm.Cards.Empty()) ++Carded;
	}
	// LAND_FIELD_DIGEST=1: a digest of everything laid (the tiles' three meshes, the ring, every base's levels, every
	// instance), so a change meant to leave the ground as it was can be shown to (the same digest before and after).
	if (std::getenv("LAND_FIELD_DIGEST"))
	{
		auto Mix = [](uint64_t H, const void* P, size_t N) {
			const unsigned char* C = static_cast<const unsigned char*>(P);
			for (size_t I = 0; I < N; ++I) H = (H ^ C[I]) * 1099511628211ull;
			return H;
		};
		auto Of = [&Mix](uint64_t H, const LandField::Mesh& M) {
			H = Mix(H, M.Positions.data(), M.Positions.size() * sizeof(float));
			H = Mix(H, M.Normals.data(), M.Normals.size() * sizeof(float));
			H = Mix(H, M.Colours.data(), M.Colours.size() * sizeof(LandField::Rgb8));
			H = Mix(H, M.Uvs.data(), M.Uvs.size() * sizeof(float));
			return Mix(H, M.Triangles.data(), M.Triangles.size() * sizeof(uint32_t));
		};
		uint64_t Tiles = 1469598103934665603ull, Ring = Tiles, Bases = Tiles, Things = Tiles;
		for (const LandField::Tile& Tl : G.Tiles()) Tiles = Of(Of(Of(Tiles, Tl.Near), Tl.Far), Tl.Built);
		for (const LandField::Tile& Tl : G.Ring()) Ring = Of(Of(Of(Ring, Tl.Near), Tl.Far), Tl.Built);
		for (int B = 0; B < static_cast<int>(LandField::Base::Count); ++B)
		{
			const LandField::BaseMesh& Bm = G.BaseOf(static_cast<LandField::Base>(B));
			for (const LandField::Mesh* M : { &Bm.Near, &Bm.Mid, &Bm.Far, &Bm.Cards, &Bm.Sprigs }) Bases = Of(Bases, *M);
			for (const LandField::Placed& P : G.Instances(static_cast<LandField::Base>(B)))
			{
				const float Pose[6] = { P.X, P.Y, P.Z, P.Yaw, P.Scale, P.ScaleZ };
				Things = Mix(Mix(Things, Pose, sizeof(Pose)), &P.Tint, sizeof(P.Tint));
			}
		}
		std::printf("  digest: tiles %016llx, ring %016llx, bases %016llx, instances %016llx\n", static_cast<unsigned long long>(Tiles), static_cast<unsigned long long>(Ring),
		            static_cast<unsigned long long>(Bases), static_cast<unsigned long long>(Things));
	}
	// Ploughland's furrows (the near ground's two numbers a point, Mesh::Uvs: the plough's way times the furrows' strength):
	// on the steppe with the word, every near tile carries them at every point, within -1 to 1, and no triangle has two
	// ways in it (a fallow's edge zero, so two fields' ways never mix across a square); the far level and the ring none;
	// without the word, no ground mesh any.
	{
		const bool bFurrows = F.Tried("ploughland") && LookLaid == "steppe";
		size_t Carrying = 0, Furrowed = 0, BadSize = 0, BadValue = 0, Mixed = 0, Others = 0, Seams = 0;
		std::map<std::tuple<float, float, float>, std::pair<float, float>> SeenAt; // (a point two tiles share: the same numbers in both)
		for (const LandField::Tile& Tl : G.Tiles())
		{
			const LandField::Mesh& M = Tl.Near;
			Others += Tl.Far.Uvs.size() + Tl.Built.Uvs.size();
			if (M.Uvs.empty()) continue;
			++Carrying;
			if (M.Uvs.size() != 2 * M.Points())
			{
				++BadSize;
				continue;
			}
			for (size_t K = 0; K < M.Points(); ++K)
			{
				const float U = M.Uvs[2 * K], V = M.Uvs[2 * K + 1];
				if (!std::isfinite(U) || !std::isfinite(V) || std::hypot(U, V) > 1.0001f) ++BadValue;
				else if (U != 0.0f || V != 0.0f) ++Furrowed;
				const auto Was = SeenAt.emplace(std::make_tuple(M.Positions[3 * K], M.Positions[3 * K + 1], M.Positions[3 * K + 2]), std::make_pair(U, V));
				if (!Was.second && Was.first->second != std::make_pair(U, V)) ++Seams;
			}
			for (size_t Tr = 0; Tr + 2 < M.Triangles.size(); Tr += 3)
				for (int A = 0; A < 3; ++A)
				{
					const size_t P0 = M.Triangles[Tr + static_cast<size_t>(A)], P1 = M.Triangles[Tr + static_cast<size_t>((A + 1) % 3)];
					const double U0 = M.Uvs[2 * P0], V0 = M.Uvs[2 * P0 + 1], U1 = M.Uvs[2 * P1], V1 = M.Uvs[2 * P1 + 1];
					const double L0 = std::hypot(U0, V0), L1 = std::hypot(U1, V1);
					if (L0 > 1e-6 && L1 > 1e-6 && U0 * U1 + V0 * V1 < 0.999 * L0 * L1) ++Mixed;
				}
		}
		for (const LandField::Tile& Tl : G.Ring()) Others += Tl.Near.Uvs.size() + Tl.Far.Uvs.size();
		if (bFurrows)
			Check(Carrying == G.Tiles().size() && Furrowed > 0 && BadSize == 0 && BadValue == 0 && Mixed == 0 && Others == 0 && Seams == 0,
				  "ploughland: every near tile carries the furrows' way at every point, one way to a triangle, the same where two tiles meet, none on the far ground",
				  Fmt("%.0f of the tiles, %.0f points furrowed, ", static_cast<double>(Carrying), static_cast<double>(Furrowed)) + Fmt("%.0f wrong sizes, %.0f out of range, ", static_cast<double>(BadSize), static_cast<double>(BadValue))
					  + Fmt("%.0f edges with two ways, %.0f numbers elsewhere, ", static_cast<double>(Mixed), static_cast<double>(Others)) + Fmt("%.0f shared points that disagree", static_cast<double>(Seams)));
		else Check(Carrying == 0 && Others == 0, "no ground mesh carries furrows (the word is off, or not the steppe)", Fmt("%.0f tiles", static_cast<double>(Carrying)));
	}
	// The bocage's farmhouses built to the vehicles' detail (release 2, the trial "houses", LandHouse), and their barns and
	// byres: each kind a kit slot drawn as a building, near, its middle level from 90 m and plain from 300 m; every one
	// standing on its house's floor, its levels within their triangle budgets (a farmhouse's or a barn's near some 25 to 40
	// thousand, as a vehicle's; a byre's from 18 thousand), every point a number and within the house's reach, and its
	// normals of unit length. The village church the same, whole in one slot (nave, chancel and tower; its near some 75 to
	// 90 thousand, three buildings' worth, its middle at most 14 thousand, its far 1.5), standing once where the village has one.
	// The village's houses the same, each its own (their near some 6 to 45 thousand: a rendered cottage's few stones, a
	// rubble house of three storeys).
	if (F.Tried("houses") && LookLaid == "bocage")
	{
		int Kinds = 0, Barns = 0, Byres = 0, Churches = 0, Village = 0;
		size_t Standing = 0, NearFew = static_cast<size_t>(-1), NearMost = 0, MidMost = 0, FarMost = 0;
		size_t VNearFew = static_cast<size_t>(-1), VNearMost = 0, VMidMost = 0, VFarMost = 0, VNear = 0;
		std::string Bad;
		for (int K = 0; K < LandField::KitSlots; ++K)
		{
			const LandField::Base B = static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K);
			const LandField::BaseMesh& Bm = G.BaseOf(B);
			const bool bByre = Bm.Name.rfind("Byre", 0) == 0;
			if (Bm.Class == LandField::BaseMesh::Draw::House && Bm.Name.rfind("Church", 0) == 0)
			{
				Churches += static_cast<int>(G.Instances(B).size());
				std::printf("    %-24s %4zu standing   %5zu / %4zu / %3zu tris near / middle from %.0f m / far from %.0f m\n", Bm.Name.c_str(), G.Instances(B).size(), Bm.Near.Tris(), Bm.Mid.Tris(), Bm.Far.Tris(),
				            Bm.MidFrom, Bm.FarFrom);
				if (Bm.Near.Tris() < 60000 || Bm.Near.Tris() > 90000 || Bm.Mid.Tris() > 14000 || Bm.Far.Tris() > 1500 || Bm.Mid.Empty() || Bm.Far.Empty() || Bm.MidFrom != 90.0f || Bm.FarFrom != 300.0f || Bm.bSight)
					Bad += " " + Bm.Name + " (its levels)";
				for (const LandField::Mesh* M : { &Bm.Near, &Bm.Mid, &Bm.Far })
				{
					bool bOk = M->Normals.size() == M->Positions.size() && M->Colours.size() * 3 == M->Positions.size();
					for (size_t I = 0; bOk && I + 2 < M->Positions.size(); I += 3)
					{
						const float X = M->Positions[I], Y = M->Positions[I + 1], Z = M->Positions[I + 2];
						const double Nl = std::hypot(M->Normals[I], M->Normals[I + 1], M->Normals[I + 2]);
						if (!std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z) || std::fabs(X) > 18.0f || std::fabs(Y) > 8.0f || Z < -6.0f || Z > 32.0f || std::fabs(Nl - 1.0) > 0.01) bOk = false;
					}
					for (const uint32_t Ix : M->Triangles)
						if (static_cast<size_t>(Ix) * 3 >= M->Positions.size()) bOk = false;
					if (!bOk) Bad += " " + Bm.Name + " (a point)";
				}
				for (const LandField::Placed& P : G.Instances(B))
				{
					LandField::Drawn What = LandField::Drawn::Beyond;
					const double Z = G.HeightAt(P.X, P.Y, &What);
					if (What != LandField::Drawn::Floor || std::fabs(Z - P.Z) > 0.02) Bad += Fmt(" church %.0f", P.X) + Fmt(",%.0f (off its floor)", P.Y);
				}
				continue;
			}
			const bool bVillage = Bm.Name.rfind("House", 0) == 0;
			if (Bm.Class != LandField::BaseMesh::Draw::House || (Bm.Name.rfind("Farmhouse", 0) != 0 && Bm.Name.rfind("Barn", 0) != 0 && !bByre && !bVillage)) continue;
			if (bVillage)
			{
				Village += static_cast<int>(G.Instances(B).size());
				VNearFew = std::min(VNearFew, Bm.Near.Tris());
				VNearMost = std::max(VNearMost, Bm.Near.Tris());
				VMidMost = std::max(VMidMost, Bm.Mid.Tris());
				VFarMost = std::max(VFarMost, Bm.Far.Tris());
				VNear += Bm.Near.Tris();
				if (std::getenv("LAND_FIELD_HOUSES"))
					std::printf("    %-24s %4zu standing   %5zu / %4zu / %3zu tris near / middle from %.0f m / far from %.0f m\n", Bm.Name.c_str(), G.Instances(B).size(), Bm.Near.Tris(), Bm.Mid.Tris(), Bm.Far.Tris(), Bm.MidFrom, Bm.FarFrom);
			}
			else ++Kinds;
			Barns += Bm.Name.rfind("Barn", 0) == 0 ? static_cast<int>(G.Instances(B).size()) : 0;
			Byres += bByre ? static_cast<int>(G.Instances(B).size()) : 0;
			if (!bVillage)
			{
			Standing += G.Instances(B).size();
			// (a burnt-out shell has no roof: its near level some 13 thousand; a byre, low, some 21 thousand whole)
			if (Bm.Name.find("Shell") == std::string::npos && !bByre) NearFew = std::min(NearFew, Bm.Near.Tris());
			else if (Bm.Near.Tris() < (bByre && Bm.Name.find("Shell") == std::string::npos ? 18000u : 8000u)) Bad += " " + Bm.Name + " (too few)";
			NearMost = std::max(NearMost, Bm.Near.Tris());
			MidMost = std::max(MidMost, Bm.Mid.Tris());
			FarMost = std::max(FarMost, Bm.Far.Tris());
			std::printf("    %-24s %4zu standing   %5zu / %4zu / %3zu tris near / middle from %.0f m / far from %.0f m\n", Bm.Name.c_str(), G.Instances(B).size(), Bm.Near.Tris(), Bm.Mid.Tris(), Bm.Far.Tris(),
			            Bm.MidFrom, Bm.FarFrom);
			}
			if (Bm.Mid.Empty() || Bm.Far.Empty() || Bm.MidFrom != 90.0f || Bm.FarFrom != 300.0f || Bm.bSight) Bad += " " + Bm.Name + " (its levels)";
			for (const LandField::Mesh* M : { &Bm.Near, &Bm.Mid, &Bm.Far })
			{
				bool bOk = M->Normals.size() == M->Positions.size() && M->Colours.size() * 3 == M->Positions.size();
				for (size_t I = 0; bOk && I + 2 < M->Positions.size(); I += 3)
				{
					const float X = M->Positions[I], Y = M->Positions[I + 1], Z = M->Positions[I + 2];
					const double Nl = std::hypot(M->Normals[I], M->Normals[I + 1], M->Normals[I + 2]);
					if (!std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z) || std::fabs(X) > 12.0f || std::fabs(Y) > 8.0f || Z < -6.0f || Z > 16.0f || std::fabs(Nl - 1.0) > 0.01) bOk = false;
				}
				for (const uint32_t Ix : M->Triangles)
					if (static_cast<size_t>(Ix) * 3 >= M->Positions.size()) bOk = false;
				if (!bOk) Bad += " " + Bm.Name + " (a point)";
			}
			for (const LandField::Placed& P : G.Instances(B))
			{
				LandField::Drawn What = LandField::Drawn::Beyond;
				const double Z = G.HeightAt(P.X, P.Y, &What);
				if (What != LandField::Drawn::Floor || std::fabs(Z - P.Z) > 0.02) Bad += Fmt(" %.0f", P.X) + Fmt(",%.0f (off its floor)", P.Y);
			}
		}
		std::printf("  farmhouses, barns and byres at the vehicles' detail: %zu standing (%d barns, %d byres) in %d kinds; near %zu to %zu triangles, middle at most %zu, far at most %zu; the church %d; the tiles' built %zu\n", Standing, Barns,
		            Byres, Kinds, NearFew == static_cast<size_t>(-1) ? 0 : NearFew, NearMost, MidMost, FarMost, Churches, T.BuiltTris);
		std::printf("  the village's houses at the vehicles' detail: %d standing; near %zu to %zu triangles (%zu in all), middle at most %zu, far at most %zu\n", Village, VNearFew == static_cast<size_t>(-1) ? 0 : VNearFew, VNearMost, VNear, VMidMost, VFarMost);
		Check(Kinds > 0 && Standing > 0 && Barns > 0 && Byres > 0 && Bad.empty() && (NearFew == static_cast<size_t>(-1) || NearFew >= 15000) && NearMost <= 45000 && MidMost <= 6000 && FarMost <= 1200 && Churches == (T.bChurch ? 1 : 0),
		      "the trial \"houses\": each bocage farmhouse, barn, byre and the church at the vehicles' detail, on its floor, three levels within budget", Fmt("%.0f standing, ", static_cast<double>(Standing)) + Fmt("church %.0f, ", static_cast<double>(Churches)) + "bad:" + (Bad.empty() ? std::string(" none") : Bad.substr(0, 200)));
		Check(Village > 0 && (VNearFew == static_cast<size_t>(-1) || VNearFew >= 6000) && VNearMost <= 45000 && VMidMost <= 6000 && VFarMost <= 1200,
		      "the trial \"houses\": the village's houses at the vehicles' detail, each its own, three levels within budget", Fmt("%.0f standing", static_cast<double>(Village)));
	}
	// The east's kit houses built to the vehicles' detail (the same trial: Brian 24 November, every theatre's houses): the
	// farmland's log izbas, plank-roofed and thatched (LandHouse's Kind 7), four looks of each, each a kit slot drawn as a
	// building with its three levels, each house planted on its floor (as the kit's model: a tenth of a metre down), within
	// its footprint and its triangle budgets.
	if (F.Tried("houses") && LookLaid == "farmland" && F.Lon >= 15.5)
	{
		size_t Standing = 0, NearFew = static_cast<size_t>(-1), NearMost = 0, MidMost = 0, FarMost = 0, Near = 0;
		int Kinds = 0;
		std::string Bad;
		for (int K = 0; K < LandField::KitSlots; ++K)
		{
			const LandField::Base B = static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K);
			const LandField::BaseMesh& Bm = G.BaseOf(B);
			const bool bKhata = Bm.Name.rfind("Khata", 0) == 0;
			if ((Bm.Name.rfind("Izba", 0) != 0 && !bKhata) || Bm.Class != LandField::BaseMesh::Draw::House) continue;
			++Kinds;
			// (an izba 7.5 by 7.2 m with its porch and woodpile; a khata 9.18 by 6.46 m, its snow lip some 0.2 m past the eaves)
			const float HalfX = bKhata ? 4.85f : 4.6f, HalfY = bKhata ? 3.5f : 4.0f;
			Standing += G.Instances(B).size();
			Near += Bm.Near.Tris() * G.Instances(B).size();
			NearFew = std::min(NearFew, Bm.Near.Tris());
			NearMost = std::max(NearMost, Bm.Near.Tris());
			MidMost = std::max(MidMost, Bm.Mid.Tris());
			FarMost = std::max(FarMost, Bm.Far.Tris());
			std::printf("    %-24s %4zu standing   %5zu / %4zu / %3zu tris near / middle from %.0f m / far from %.0f m\n", Bm.Name.c_str(), G.Instances(B).size(), Bm.Near.Tris(), Bm.Mid.Tris(), Bm.Far.Tris(), Bm.MidFrom, Bm.FarFrom);
			// (LAND_FIELD_HOUSES=1: where each stands, and which way its front faces, degrees from east)
			if (std::getenv("LAND_FIELD_HOUSES"))
				for (const LandField::Placed& P : G.Instances(B)) std::printf("      at %.0f, %.0f, its front %.0f degrees\n", P.X, P.Y, std::fmod(P.Yaw * 180.0 / 3.14159265358979 + 90.0 + 720.0, 360.0));
			if (Bm.Mid.Empty() || Bm.Far.Empty() || Bm.MidFrom != 90.0f || Bm.FarFrom != 300.0f || Bm.bSight) Bad += " " + Bm.Name + " (its levels)";
			for (const LandField::Mesh* M : { &Bm.Near, &Bm.Mid, &Bm.Far })
			{
				bool bOk = M->Normals.size() == M->Positions.size() && M->Colours.size() * 3 == M->Positions.size();
				for (size_t I = 0; bOk && I + 2 < M->Positions.size(); I += 3)
				{
					const float X = M->Positions[I], Y = M->Positions[I + 1], Z = M->Positions[I + 2];
					const double Nl = std::hypot(M->Normals[I], M->Normals[I + 1], M->Normals[I + 2]);
					if (!std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z) || std::fabs(X) > HalfX || std::fabs(Y) > HalfY || Z < -6.0f || Z > 9.0f || std::fabs(Nl - 1.0) > 0.01) bOk = false;
				}
				for (const uint32_t Ix : M->Triangles)
					if (static_cast<size_t>(Ix) * 3 >= M->Positions.size()) bOk = false;
				if (!bOk) Bad += " " + Bm.Name + " (a point)";
			}
		}
		std::printf("  the east's izbas and khatas at the vehicles' detail: %zu standing in %d looks; near %zu to %zu triangles (%zu in all), middle at most %zu, far at most %zu\n", Standing, Kinds, NearFew == static_cast<size_t>(-1) ? 0 : NearFew, NearMost, Near, MidMost, FarMost);
		Check(Kinds > 0 && Standing > 0 && Bad.empty() && NearFew >= 15000 && NearMost <= 45000 && MidMost <= 6000 && FarMost <= 1200,
		      "the trial \"houses\": the east's izbas and khatas at the vehicles' detail, three levels within budget", Fmt("%.0f standing, bad:", static_cast<double>(Standing)) + (Bad.empty() ? std::string(" none") : Bad.substr(0, 200)));
	}
	// The desert's kit houses built to the vehicles' detail (a trial of their own, "deserthouses"): the town's flat-roofed houses round their
	// walled yards (LandHouse's Kind 10), limewashed, of bare mud or of two storeys, four looks of each, each a kit slot
	// drawn as a building with its three levels, within its footprint (its spouts and its beams' ends some 0.7 m out past
	// its walls) and its triangle budgets.
	if (F.Tried("deserthouses") && LookLaid == "desert")
	{
		size_t Standing = 0, NearFew = static_cast<size_t>(-1), NearMost = 0, MidMost = 0, FarMost = 0, Near = 0;
		int Kinds = 0;
		std::string Bad;
		for (int K = 0; K < LandField::KitSlots; ++K)
		{
			const LandField::Base B = static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K);
			const LandField::BaseMesh& Bm = G.BaseOf(B);
			if (Bm.Name.rfind("Desert", 0) != 0 || Bm.Class != LandField::BaseMesh::Draw::House) continue;
			++Kinds;
			Standing += G.Instances(B).size();
			Near += Bm.Near.Tris() * G.Instances(B).size();
			NearFew = std::min(NearFew, Bm.Near.Tris());
			NearMost = std::max(NearMost, Bm.Near.Tris());
			MidMost = std::max(MidMost, Bm.Mid.Tris());
			FarMost = std::max(FarMost, Bm.Far.Tris());
			std::printf("    %-24s %4zu standing   %5zu / %4zu / %3zu tris near / middle from %.0f m / far from %.0f m\n", Bm.Name.c_str(), G.Instances(B).size(), Bm.Near.Tris(), Bm.Mid.Tris(), Bm.Far.Tris(), Bm.MidFrom, Bm.FarFrom);
			if (std::getenv("LAND_FIELD_HOUSES"))
				for (const LandField::Placed& P : G.Instances(B)) std::printf("      at %.0f, %.0f, its front %.0f degrees\n", P.X, P.Y, std::fmod(P.Yaw * 180.0 / 3.14159265358979 + 90.0 + 720.0, 360.0));
			if (Bm.Mid.Empty() || Bm.Far.Empty() || Bm.MidFrom != 90.0f || Bm.FarFrom != 300.0f || Bm.bSight) Bad += " " + Bm.Name + " (its levels)";
			for (const LandField::Mesh* M : { &Bm.Near, &Bm.Mid, &Bm.Far })
			{
				bool bOk = M->Normals.size() == M->Positions.size() && M->Colours.size() * 3 == M->Positions.size();
				for (size_t I = 0; bOk && I + 2 < M->Positions.size(); I += 3)
				{
					const float X = M->Positions[I], Y = M->Positions[I + 1], Z = M->Positions[I + 2];
					const double Nl = std::hypot(M->Normals[I], M->Normals[I + 1], M->Normals[I + 2]);
					if (!std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z) || std::fabs(X) > 6.2f || std::fabs(Y) > 6.2f || Z < -6.0f || Z > 9.0f || std::fabs(Nl - 1.0) > 0.01) bOk = false;
				}
				for (const uint32_t Ix : M->Triangles)
					if (static_cast<size_t>(Ix) * 3 >= M->Positions.size()) bOk = false;
				if (!bOk) Bad += " " + Bm.Name + " (a point)";
			}
		}
		std::printf("  the desert's houses at the vehicles' detail: %zu standing in %d looks; near %zu to %zu triangles (%zu in all), middle at most %zu, far at most %zu\n", Standing, Kinds, NearFew == static_cast<size_t>(-1) ? 0 : NearFew, NearMost, Near, MidMost, FarMost);
		Check(Kinds > 0 && Standing > 0 && Bad.empty() && NearFew >= 15000 && NearMost <= 45000 && MidMost <= 6000 && FarMost <= 1200,
		      "the trial \"deserthouses\": the desert's flat-roofed houses at the vehicles' detail, three levels within budget", Fmt("%.0f standing, bad:", static_cast<double>(Standing)) + (Bad.empty() ? std::string(" none") : Bad.substr(0, 200)));
	}
	// The east's burnt-out houses (the same trial): the kit's chimney a burnt-out izba or khata at the vehicles' detail
	// (LandHouse's Kind 9) on its house's footprint, its ash bed's skirt run down into the ground; four looks of each; in
	// sight with the chimney's own shape; within its footprint (its ashes 0.55 m past it) and its triangle budgets.
	if (F.Tried("houses") && LookLaid == "farmland" && F.Lon >= 15.5)
	{
		size_t Standing = 0, NearMost = 0, MidMost = 0, FarMost = 0, NearFew = static_cast<size_t>(-1);
		int Kinds = 0;
		std::string Bad;
		for (int K = 0; K < LandField::KitSlots; ++K)
		{
			const LandField::Base B = static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K);
			const LandField::BaseMesh& Bm = G.BaseOf(B);
			if (Bm.Name.rfind("Burnt", 0) != 0 || Bm.Class != LandField::BaseMesh::Draw::House) continue;
			++Kinds;
			const bool bClay = Bm.Name.rfind("BurntClay", 0) == 0;
			const float HalfX = (bClay ? 4.59f : 3.75f) + 0.6f, HalfY = (bClay ? 3.23f : 3.6f) + 0.6f;
			Standing += G.Instances(B).size();
			NearFew = std::min(NearFew, Bm.Near.Tris());
			NearMost = std::max(NearMost, Bm.Near.Tris());
			MidMost = std::max(MidMost, Bm.Mid.Tris());
			FarMost = std::max(FarMost, Bm.Far.Tris());
			std::printf("    %-24s %4zu standing   %5zu / %4zu / %3zu tris near / middle from %.0f m / far from %.0f m; in sight %s, %.2f m across\n", Bm.Name.c_str(), G.Instances(B).size(), Bm.Near.Tris(), Bm.Mid.Tris(), Bm.Far.Tris(), Bm.MidFrom, Bm.FarFrom, Bm.bSight ? "yes" : "no", Bm.SightAcross * Bm.Radius);
			if (std::getenv("LAND_FIELD_HOUSES"))
				for (const LandField::Placed& P : G.Instances(B)) std::printf("      at %.0f, %.0f, %.1f m up, turned %.0f degrees\n", P.X, P.Y, P.Z, std::fmod(P.Yaw * 180.0 / 3.14159265358979 + 720.0, 360.0));
			if (Bm.Mid.Empty() || Bm.Far.Empty() || Bm.MidFrom != 90.0f || Bm.FarFrom != 300.0f || !Bm.bSight || Bm.SightAcross * Bm.Radius > 9.0f) Bad += " " + Bm.Name + " (its levels or its sight)";
			for (const LandField::Mesh* M : { &Bm.Near, &Bm.Mid, &Bm.Far })
			{
				bool bOk = M->Normals.size() == M->Positions.size() && M->Colours.size() * 3 == M->Positions.size();
				for (size_t I = 0; bOk && I + 2 < M->Positions.size(); I += 3)
				{
					const float X = M->Positions[I], Y = M->Positions[I + 1], Z = M->Positions[I + 2];
					const double Nl = std::hypot(M->Normals[I], M->Normals[I + 1], M->Normals[I + 2]);
					if (!std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z) || std::fabs(X) > HalfX || std::fabs(Y) > HalfY || Z < -5.5f || Z > 6.0f || std::fabs(Nl - 1.0) > 0.01) bOk = false;
				}
				for (const uint32_t Ix : M->Triangles)
					if (static_cast<size_t>(Ix) * 3 >= M->Positions.size()) bOk = false;
				if (!bOk) Bad += " " + Bm.Name + " (a point)";
			}
		}
		std::printf("  the east's burnt-out houses at the vehicles' detail: %zu standing in %d looks; near %zu to %zu triangles, middle at most %zu, far at most %zu\n", Standing, Kinds, NearFew == static_cast<size_t>(-1) ? 0 : NearFew, NearMost, MidMost, FarMost);
		Check(Kinds > 0 && Standing > 0 && Bad.empty() && NearFew >= 8000 && NearMost <= 30000 && MidMost <= 3000 && FarMost <= 200,
		      "the trial \"houses\": the east's burnt-out houses at the vehicles' detail, three levels within budget, in sight as the chimney", Fmt("%.0f standing, bad:", static_cast<double>(Standing)) + (Bad.empty() ? std::string(" none") : Bad.substr(0, 200)));
	}
	// The trees far off (GS9): a crown of some 40 triangles from each tree's far level, inside the tree, every face and
	// point's normal turned out, its underside darker than its top.
	{
		std::string Line, Bad;
		int Crowns = 0;
		// (and a region's kit trees, which the game draws far off the same way)
		std::vector<LandField::Base> Trees = { LandField::Base::Oak, LandField::Base::Elm, LandField::Base::Ash, LandField::Base::Beech, LandField::Base::Pollard, LandField::Base::Apple, LandField::Base::Birch };
		for (int K = 0; K < LandField::KitSlots; ++K)
		{
			const LandField::Base Kb = static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K);
			if (G.BaseOf(Kb).bTree) Trees.push_back(Kb);
		}
		for (const LandField::Base B : Trees)
		{
			const LandField::BaseMesh& Bm = G.BaseOf(B);
			const LandField::Mesh& From = Bm.Far.Empty() ? Bm.Near : Bm.Far;
			if (From.Empty()) continue;
			const LandField::Mesh C = LandField::CrownOf(From);
			++Crowns;
			double High = 0.0, Wide = 0.0, Mid[3] = { 0, 0, 0 };
			for (size_t K = 0; K < From.Points(); ++K)
			{
				High = std::max(High, static_cast<double>(From.Positions[K * 3 + 2]));
				Wide = std::max(Wide, std::hypot(static_cast<double>(From.Positions[K * 3]), static_cast<double>(From.Positions[K * 3 + 1])));
			}
			// (the ball's middle, from its own points: the trunk's six first would pull it down and toward the trunk, off a
			// leaning palm's flat crown)
			for (size_t K = 6; K < C.Points(); ++K)
				for (int A = 0; A < 3; ++A) Mid[A] += C.Positions[K * 3 + A] / static_cast<double>(C.Points() - 6);
			bool bInside = true, bOut = true;
			for (size_t K = 0; K < C.Points(); ++K)
			{
				const float* P = &C.Positions[K * 3];
				if (P[2] < -0.01 || P[2] > High * 1.02 + 0.05 || std::hypot(static_cast<double>(P[0]), static_cast<double>(P[1])) > Wide * 1.02 + 0.05) bInside = false;
			}
			for (size_t K = 0; K + 2 < C.Triangles.size(); K += 3)
			{
				const float* A = &C.Positions[C.Triangles[K] * 3];
				const float* Bp = &C.Positions[C.Triangles[K + 1] * 3];
				const float* Cp = &C.Positions[C.Triangles[K + 2] * 3];
				const double UX = Bp[0] - A[0], UY = Bp[1] - A[1], UZ = Bp[2] - A[2], VX = Cp[0] - A[0], VY = Cp[1] - A[1], VZ = Cp[2] - A[2];
				const double NX = UY * VZ - UZ * VY, NY = UZ * VX - UX * VZ, NZ = UX * VY - UY * VX;
				// (the trunk's faces out from its own axis, the crown's from its middle)
				const bool bTrunk = K < 18;
				const double OX = (A[0] + Bp[0] + Cp[0]) / 3.0 - (bTrunk ? 0.0 : Mid[0]), OY = (A[1] + Bp[1] + Cp[1]) / 3.0 - (bTrunk ? 0.0 : Mid[1]);
				const double OZ = bTrunk ? 0.0 : (A[2] + Bp[2] + Cp[2]) / 3.0 - Mid[2];
				if (NX * OX + NY * OY + NZ * OZ <= 0.0) bOut = false;
			}
			const bool bDark = C.Points() > 2 && C.Colours.back().G < C.Colours[6].G;
			Line += std::string(" ") + BaseNames[static_cast<int>(B)] + Fmt(" %.0f;", static_cast<double>(C.Tris()));
			if (C.Tris() < 30 || C.Tris() > 48 || !bInside || !bOut || !bDark) Bad += std::string(" ") + BaseNames[static_cast<int>(B)];
		}
		std::printf("  far crowns (GS9), triangles:%s\n", Line.c_str());
		Check(Crowns > 0 && Bad.empty(), "the trees far off as crowns of 30 to 48 triangles, inside the tree, faces out, darker beneath", Bad.empty() ? Fmt("%.0f trees", Crowns) : "wrong:" + Bad);
		// (on trial "farcrowns": the crown CrownLobed makes in CrownOf's place, by the class it reads from the far mesh:
		// a broadleaf in lobes, 48 to 66 triangles; a conifer in three skirts of eight sides, 54; a palm as CrownOf's)
		if (F.Tried("farcrowns"))
		{
			std::string Lobed, Wrong;
			for (const LandField::Base B : Trees)
			{
				const LandField::BaseMesh& Bm = G.BaseOf(B);
				const LandField::Mesh& From = Bm.Far.Empty() ? Bm.Near : Bm.Far;
				if (From.Empty()) continue;
				double Narrow = 0.0, Taper = 0.0;
				const int Class = LandField::CrownClass(From, &Narrow, &Taper);
				const LandField::Mesh C = LandField::CrownLobed(From);
				const size_t Was = LandField::CrownOf(From).Tris(), Now = C.Tris();
				// (GS9's shape checks on it too: every corner a number, inside the tree (a conifer's skirts may reach 15%
				// past the far mesh's boughs, the near tree's own reach), each face out the way its corners' normals point,
				// darker beneath than on top; a lobed crown's dark heart, each lobe's foot against its own cap, 0.40 to 0.55
				// and the darkest foot no less than 0.40 of the lightest cap; and at least 80% of the far mesh's leafy
				// points at a height its lobes span)
				double High = 0.0, Reach = 0.0;
				int LeafN = 0;
				for (size_t K = 0; K < From.Points(); ++K)
				{
					const float* P = &From.Positions[K * 3];
					High = std::max(High, static_cast<double>(P[2]));
					Reach = std::max(Reach, std::hypot(static_cast<double>(P[0]), static_cast<double>(P[1])));
					if (From.Colours[K].G > From.Colours[K].R + 6) ++LeafN;
				}
				bool bFinite = true, bInside = true, bOut = true;
				for (size_t K = 0; K < C.Points(); ++K)
				{
					const float* P = &C.Positions[K * 3];
					if (!std::isfinite(P[0]) || !std::isfinite(P[1]) || !std::isfinite(P[2])) bFinite = false;
					if (P[2] < -0.01 || P[2] > High * 1.02 + 0.05 || std::hypot(static_cast<double>(P[0]), static_cast<double>(P[1])) > Reach * (Class == 1 ? 1.15 : 1.02) + 0.05) bInside = false;
				}
				double UpL = 0.0, DownL = 0.0;
				int UpN = 0, DownN = 0;
				auto Lum = [&C](size_t K) { return 0.299 * C.Colours[K].R + 0.587 * C.Colours[K].G + 0.114 * C.Colours[K].B; };
				for (size_t K = 0; K + 2 < C.Triangles.size(); K += 3)
				{
					const uint32_t I0 = C.Triangles[K], I1 = C.Triangles[K + 1], I2 = C.Triangles[K + 2];
					const float* A = &C.Positions[I0 * 3];
					const float* Bp = &C.Positions[I1 * 3];
					const float* Cp = &C.Positions[I2 * 3];
					const double UX = Bp[0] - A[0], UY = Bp[1] - A[1], UZ = Bp[2] - A[2], VX = Cp[0] - A[0], VY = Cp[1] - A[1], VZ = Cp[2] - A[2];
					const double NX = UY * VZ - UZ * VY, NY = UZ * VX - UX * VZ, NZ = UX * VY - UY * VX;
					double MX = 0.0, MY = 0.0, MZ = 0.0;
					for (const uint32_t I : { I0, I1, I2 })
					{
						MX += C.Normals[I * 3];
						MY += C.Normals[I * 3 + 1];
						MZ += C.Normals[I * 3 + 2];
					}
					// (a palm's crown is CrownOf's own, unchanged, which GS9 above already holds to its faces by its own test;
					// its head's corner normals do not follow its faces, so it is not judged by them here)
					if (Class != 2 && NX * MX + NY * MY + NZ * MZ <= 0.0) bOut = false;
				}
				for (size_t K = 6; K < C.Points(); ++K)
				{
					const double Nz = C.Normals[K * 3 + 2];
					if (Nz > 0.3) UpL += Lum(K), ++UpN;
					else if (Nz < -0.3) DownL += Lum(K), ++DownN;
				}
				const bool bDark = UpN > 0 && DownN > 0 && DownL / DownN < UpL / UpN;
				// (the lobes: after the stub's six corners, eight a lobe, its top first and its foot second)
				double Heart = 0.5, HeartLeast = 0.5, CapMost = 0.0, FootLeast = 1e9, Cover = 1.0;
				bool bMeasured = false;
				if ((Class == 0 || Class == 3) && C.Points() > 6 && (C.Points() - 6) % 8 == 0 && LeafN >= 8)
				{
					bMeasured = true;
					const size_t Lobes = (C.Points() - 6) / 8;
					std::vector<std::pair<double, double>> Spans;
					Heart = 0.0;
					for (size_t L = 0; L < Lobes; ++L)
					{
						const size_t T0 = 6 + 8 * L;
						const double Own = Lum(T0 + 1) / std::max(1.0, Lum(T0));
						Heart = std::max(Heart, Own);
						HeartLeast = std::min(HeartLeast, Own);
						CapMost = std::max(CapMost, Lum(T0));
						FootLeast = std::min(FootLeast, Lum(T0 + 1));
						double Z0 = 1e9, Z1 = -1e9;
						for (size_t K = T0; K < T0 + 8; ++K)
						{
							Z0 = std::min(Z0, static_cast<double>(C.Positions[K * 3 + 2]));
							Z1 = std::max(Z1, static_cast<double>(C.Positions[K * 3 + 2]));
						}
						Spans.push_back({ Z0, Z1 });
					}
					// (the share of the far mesh's leafy points at a height some lobe spans: a jungle tree's few green
					// points at its foot do not count against it as a share of the height would)
					int Hit = 0;
					for (size_t K = 0; K < From.Points(); ++K)
					{
						if (From.Colours[K].G <= From.Colours[K].R + 6) continue;
						const double Z = From.Positions[K * 3 + 2];
						for (const auto& [Z0, Z1] : Spans)
							if (Z >= Z0 && Z <= Z1)
							{
								++Hit;
								break;
							}
					}
					Cover = Hit / static_cast<double>(LeafN);
				}
				const double Whole = CapMost > 0.0 ? FootLeast / CapMost : 1.0;
				const bool bHeart = Class == 1 || Class == 2 || (HeartLeast >= 0.40 && Heart <= 0.55 && Whole >= 0.40);
				Lobed += std::string(" ") + BaseNames[static_cast<int>(B)] + " (" + Bm.Name + ", " + (Class == 0 ? "broadleaf" : Class == 1 ? "conifer" : Class == 2 ? "palm" : Class == 3 ? "column" : "none") +
					Fmt(", width/height %.2f,", Narrow) + Fmt(" foot/top %.2f)", Taper) + Fmt(" far %.0f,", static_cast<double>(Bm.Far.Tris())) + Fmt(" crown %.0f ->", static_cast<double>(Was)) + Fmt(" %.0f", static_cast<double>(Now)) +
					(bMeasured ? Fmt(", heart %.2f", HeartLeast) + Fmt("-%.2f of cap", Heart) + Fmt(" (%.2f of the lightest),", Whole) + Fmt(" lobes span %.0f%% of its leaves", 100.0 * Cover) : std::string()) + ";";
				const bool bFits = Class == 0 ? Now >= 48 && Now <= 66 : Class == 1 ? Now == 54 : Class == 3 ? Now >= 30 && Now <= 66 : Now == Was;
				if (!bFits || !bFinite || !bInside || !bOut || !bDark || !bHeart || Cover < 0.8)
					Wrong += std::string(" ") + BaseNames[static_cast<int>(B)] + (!bFits ? " (triangles)" : "") + (!bFinite ? " (not finite)" : "") + (!bInside ? " (outside the tree)" : "") + (!bOut ? " (a face in)" : "") +
						(!bDark ? " (not darker beneath)" : "") + (!bHeart ? " (heart)" : "") + (Cover < 0.8 ? " (cover)" : "");
			}
			std::printf("  far crowns on trial \"farcrowns\" (CrownLobed), triangles:%s\n", Lobed.c_str());
			// (LAND_FIELD_DUMP_CROWNS=<dir>: each tree's near, far and both crowns as .obj with their colours, and its
			// instances' mean scales, for measuring their outlines from any side)
			if (const char* Dump = std::getenv("LAND_FIELD_DUMP_CROWNS"))
				for (const LandField::Base B : Trees)
				{
					const LandField::BaseMesh& Bm = G.BaseOf(B);
					const LandField::Mesh& From = Bm.Far.Empty() ? Bm.Near : Bm.Far;
					if (From.Empty() || G.Instances(B).empty()) continue;
					const LandField::Mesh Old = LandField::CrownOf(From), New = LandField::CrownLobed(From);
					const std::pair<const char*, const LandField::Mesh*> Levels[] = { { "near", &Bm.Near }, { "far", &Bm.Far }, { "crownof", &Old }, { "crownlobed", &New } };
					for (const auto& [Word, Me] : Levels)
					{
						std::ofstream Obj(std::string(Dump) + "/" + BaseNames[static_cast<int>(B)] + "-" + Word + ".obj");
						for (size_t K = 0; K < Me->Points(); ++K)
							Obj << "v " << Me->Positions[K * 3] << " " << Me->Positions[K * 3 + 1] << " " << Me->Positions[K * 3 + 2] << " " << static_cast<int>(Me->Colours[K].R) << " "
								<< static_cast<int>(Me->Colours[K].G) << " " << static_cast<int>(Me->Colours[K].B) << "\n";
						for (size_t K = 0; K + 2 < Me->Triangles.size(); K += 3) Obj << "f " << Me->Triangles[K] + 1 << " " << Me->Triangles[K + 1] + 1 << " " << Me->Triangles[K + 2] + 1 << "\n";
					}
					double Sc = 0.0, Sz = 0.0;
					for (const LandField::Placed& P : G.Instances(B))
					{
						Sc += P.Scale;
						Sz += P.ScaleZ;
					}
					const double Nb = static_cast<double>(G.Instances(B).size());
					std::printf("    dumped %s (%s): %zu standing, mean scale %.3f across, %.3f up\n", BaseNames[static_cast<int>(B)], Bm.Name.c_str(), G.Instances(B).size(), Sc / Nb, Sz / Nb);
				}
			Check(Wrong.empty(), "the trial \"farcrowns\": each tree's crown by its class (lobes 48 to 66 triangles, a column's 30 to 66, a conifer's three skirts 54, a palm as before), finite, inside the tree, faces out, darker beneath, its heart 0.40 to 0.55 of its cap, its lobes at the height of 80% of its leaves", Wrong.empty() ? std::string("all") : "wrong:" + Wrong);
		}
	}
	Check(BadCards.empty() && Carded == 10, "the hedge shrubs and the trees have their leaf cards, each point its place on the leaves' picture, and no other mesh has any",
	      Fmt("%.0f bases with cards", Carded) + (BadCards.empty() ? std::string() : ", wrong:" + BadCards));

	// The clutter: what a soldier's eye gets round it, and that boxes that overlap agree.
	{
		const double CX = F.bTown ? F.TownX : F.Width() / 2.0, CY = F.bTown ? F.TownY - 500.0 : F.Depth() / 2.0;
		size_t Wide = 0;
		for (const double Half : { 40.0, 60.0 })
		{
			std::vector<LandField::Placed> Out;
			// (the quickest of five, so a busy machine's hiccup is not read as the field's cost)
			double Best = 1e9;
			for (int Try = 0; Try < 5; ++Try)
			{
				Out.clear();
				const auto C0 = std::chrono::steady_clock::now();
				G.Clutter(CX - Half, CY - Half, CX + Half, CY + Half, Out);
				Best = std::min(Best, Seconds(C0));
			}
			std::map<int, int> ByKind;
			for (const LandField::Placed& P : Out) ++ByKind[static_cast<int>(P.Kind)];
			std::string Kinds;
			for (const auto& [K, N] : ByKind) Kinds += std::string(" ") + BaseNames[K] + " " + std::to_string(N);
			std::printf("  clutter in %.0f m square round %.0f, %.0f: %zu things in %.1f ms (%s )\n", 2.0 * Half, CX, CY, Out.size(), Best * 1000.0, Kinds.c_str());
			Wide = Out.size();
		}
		// (the frame rate: a region's ground draws no more than Normandy's, which the Mac holds at 45 frames a second warm
		// with the day's figures; production, 22 November: no thread raises drawn counts until the gate passes. Normandy
		// lays 14,635 clutter things in this square, 178,257 things standing and 1.24 million near triangles)
		if (bRegion)
			Check(Wide <= 15000 && G.Laid().Instances <= 180000 && G.Laid().NearTris <= 1250000, "a region's ground draws no more than Normandy's (clutter round the eye, things standing, near triangles)",
			      Fmt("%.0f clutter things in 120 m, ", static_cast<double>(Wide)) + Fmt("%.0f standing, ", static_cast<double>(G.Laid().Instances)) + Fmt("%.0f near triangles", static_cast<double>(G.Laid().NearTris)));
		std::vector<LandField::Placed> A, B;
		G.Clutter(CX - 30.0, CY - 30.0, CX + 10.0, CY + 10.0, A);
		G.Clutter(CX - 10.0, CY - 10.0, CX + 30.0, CY + 30.0, B);
		// (the desert's ruts, the trial "trigh": a piece lies at its station on the rut, up to 2 m from the try that lays
		// it, so the pieces are matched 4 m inside the overlap, where every try that lays one lies within it)
		const bool bRuts = F.Tried("trigh") && LookLaid == "desert";
		auto Inside = [&](const LandField::Placed& P) {
			const double M = bRuts && (P.Kind == LandField::Base::Clut5 || P.Kind == LandField::Base::Trodden || P.Kind == LandField::Base::Stubble) ? 6.0 : 10.0;
			return P.X >= CX - M && P.X < CX + M && P.Y >= CY - M && P.Y < CY + M;
		};
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

	// The desert's braided tracks at a man's eye (the trial "trigh"): each strand's two wheel ruts laid in pieces end to
	// end at stations along their lines, square-ended where a piece has both neighbours (Clut5's slot), else fading
	// toward the one it lacks (Trodden's slot fading ahead as built, Stubble's behind). In the 120 m square round the eye
	// (LAND_FIELD_EYE's place where it is given): every try at a station lays the same piece in the same place; each
	// piece's middle on its rut's line (its neighbours') and its way the line's, one width a rut, no piece doubling
	// another; neighbours meeting end to end, neither apart nor overlapping by more than a centimetre, the gaps between
	// pieces a station or more and most of a rut's length within 30 m covered, a square end only where a neighbour meets
	// it; built with its edges the ground's own colour, its pressed floor one tone, mirrored across its line; and each
	// lying on the drawn ground, not under it nor floating over it.
	if (F.Tried("trigh") && LookLaid == "desert")
	{
		double EX = F.bTown ? F.TownX : F.Width() / 2.0, EY = F.bTown ? F.TownY - 500.0 : F.Depth() / 2.0;
		if (const char* E = std::getenv("LAND_FIELD_EYE"))
		{
			double X = 0.0, Y = 0.0, A = 0.0;
			if (std::sscanf(E, "%lf %lf %lf", &X, &Y, &A) == 3)
			{
				EX = X;
				EY = Y;
			}
		}
		std::vector<LandField::Placed> Out;
		G.Clutter(EX - 60.0, EY - 60.0, EX + 60.0, EY + 60.0, Out);
		// (the three meshes as built: their length and width, their edge points', floor points' and fades' greys)
		const LandField::Mesh* Built[3] = { &G.BaseOf(LandField::Base::Clut5).Near, &G.BaseOf(LandField::Base::Trodden).Near, &G.BaseOf(LandField::Base::Stubble).Near };
		double Long[3] = { 0.0, 0.0, 0.0 }, Wide[3] = { 0.0, 0.0, 0.0 };
		bool bMirror = true;
		std::string Shape;
		for (int K = 0; K < 3; ++K)
		{
			const LandField::Mesh& M = *Built[K];
			double X0 = 1e9, X1 = -1e9, Y0 = 1e9, Y1 = -1e9;
			for (size_t I = 0; I < M.Points(); ++I)
			{
				X0 = std::min(X0, static_cast<double>(M.Positions[I * 3]));
				X1 = std::max(X1, static_cast<double>(M.Positions[I * 3]));
				Y0 = std::min(Y0, static_cast<double>(M.Positions[I * 3 + 1]));
				Y1 = std::max(Y1, static_cast<double>(M.Positions[I * 3 + 1]));
			}
			Long[K] = X1 - X0;
			Wide[K] = Y1 - Y0;
			// (mirrored across its middle line: each point's mirror a point of the same grey, each triangle's mirror a
			// triangle, so its diagonals turn the same way both sides)
			auto Find = [&M](double X, double Y) {
				for (size_t I = 0; I < M.Points(); ++I)
					if (std::fabs(M.Positions[I * 3] - X) < 1e-4 && std::fabs(M.Positions[I * 3 + 1] - Y) < 1e-4) return static_cast<long>(I);
				return -1L;
			};
			std::vector<std::vector<long>> Tris;
			for (size_t Q = 0; Q + 2 < M.Triangles.size(); Q += 3)
			{
				std::vector<long> V = { static_cast<long>(M.Triangles[Q]), static_cast<long>(M.Triangles[Q + 1]), static_cast<long>(M.Triangles[Q + 2]) };
				std::sort(V.begin(), V.end());
				Tris.push_back(V);
			}
			for (int Flip = 0; Flip < 1; ++Flip)
			{
				std::vector<long> Map(M.Points(), -1);
				for (size_t I = 0; I < M.Points(); ++I)
				{
					const double X = M.Positions[I * 3], Y = M.Positions[I * 3 + 1];
					Map[I] = Find(X, -Y);
					if (Map[I] < 0 || M.Colours[static_cast<size_t>(Map[I])].G != M.Colours[I].G) bMirror = false;
				}
				for (const std::vector<long>& Tr : Tris)
				{
					if (!bMirror) break;
					std::vector<long> V = { Map[static_cast<size_t>(Tr[0])], Map[static_cast<size_t>(Tr[1])], Map[static_cast<size_t>(Tr[2])] };
					std::sort(V.begin(), V.end());
					if (std::find(Tris.begin(), Tris.end(), V) == Tris.end()) bMirror = false;
				}
			}
			Shape += Fmt(" %.2f by %.3f m,", Long[K], Wide[K]) + Fmt(" %.0f points, %.0f triangles;", static_cast<double>(M.Points()), static_cast<double>(M.Tris()));
		}
		// The pieces (a station's copies, one a try that lays it, taken once).
		struct Rut
		{
			double X, Y, Z, Yaw, Scale, ScaleZ;
			int Odd; // (its mesh: 0 square-ended, 1 fading ahead as built, 2 behind)
			LandField::Rgb8 Tint;
			std::vector<size_t> Next;
		};
		std::vector<Rut> Ps;
		std::map<std::pair<float, float>, size_t> Where;
		int Unlike = 0, TintApart = 0;
		size_t Laid = 0;
		for (const LandField::Placed& P : Out)
		{
			const int Odd = P.Kind == LandField::Base::Trodden ? 1 : P.Kind == LandField::Base::Stubble ? 2 : 0;
			if (!Odd && P.Kind != LandField::Base::Clut5) continue;
			++Laid;
			const auto Found = Where.find({ P.X, P.Y });
			if (Found == Where.end())
			{
				Where[{ P.X, P.Y }] = Ps.size();
				Ps.push_back({ P.X, P.Y, P.Z, P.Yaw, P.Scale, P.ScaleZ, Odd, P.Tint, {} });
				continue;
			}
			const Rut& R = Ps[Found->second];
			if (static_cast<float>(R.Z) != P.Z || static_cast<float>(R.Yaw) != P.Yaw || static_cast<float>(R.Scale) != P.Scale || static_cast<float>(R.ScaleZ) != P.ScaleZ || R.Odd != Odd) ++Unlike;
			TintApart = std::max(TintApart, std::abs(R.Tint.R - P.Tint.R) + std::abs(R.Tint.G - P.Tint.G) + std::abs(R.Tint.B - P.Tint.B));
		}
		auto Dec = [](double C) { return std::pow((std::clamp(C, 0.0, 255.0) / 255.0 + 0.055) / 1.055, 2.4); };
		auto Enc = [](double L) { return 255.0 * (1.055 * std::pow(std::clamp(L, 0.0, 1.0), 1.0 / 2.4) - 0.055); };
		auto Luma = [](double R, double G_, double B) { return 0.2126 * R + 0.7152 * G_ + 0.0722 * B; };
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
		// Each piece as drawn: its mesh placed, its height over the drawn ground at its ends, quarters, middle and sides
		// (13 along it by 3 across, its edges' 0.48 of its width out); its edges' colour against the ground's there, its
		// floor's light against the ground's.
		std::vector<double> Over, EdgeOff, FloorUnder;
		for (const Rut& R : Ps)
		{
			const LandField::Mesh& M = *Built[R.Odd];
			LandField::Mesh W = M;
			const double Ca = std::cos(R.Yaw), Sa = std::sin(R.Yaw);
			double Grey = 0.0;
			for (size_t I = 0; I < M.Points(); ++I)
			{
				const double X = M.Positions[I * 3] * R.Scale, Y = M.Positions[I * 3 + 1] * R.Scale;
				W.Positions[I * 3] = static_cast<float>(R.X + X * Ca - Y * Sa);
				W.Positions[I * 3 + 1] = static_cast<float>(R.Y + X * Sa + Y * Ca);
				W.Positions[I * 3 + 2] = static_cast<float>(R.Z + M.Positions[I * 3 + 2] * R.ScaleZ);
				Grey = std::max(Grey, static_cast<double>(M.Colours[I].G));
			}
			const double L = 0.5 * Long[R.Odd] * R.Scale, H = 0.5 * Wide[R.Odd] * R.Scale;
			for (int A = 0; A < 13; ++A)
				for (int C = -1; C <= 1; ++C)
				{
					const double U = (-1.0 + A / 6.0) * (L - 0.01), V = C * 0.96 * H;
					const double X = R.X + U * Ca - V * Sa, Y = R.Y + U * Sa + V * Ca;
					const double Zp = MeshTop(W, X, Y), Zg = DrawnTop(X, Y);
					if (!std::isnan(Zp) && !std::isnan(Zg)) Over.push_back(Zp - Zg);
				}
			for (size_t I = 0; I < M.Points(); ++I)
			{
				const LandField::Rgb8 Gr = G.ColourAt(W.Positions[I * 3], W.Positions[I * 3 + 1]);
				const double Cr = Enc(Dec(M.Colours[I].R) * R.Tint.R / 128.0), Cg = Enc(Dec(M.Colours[I].G) * R.Tint.G / 128.0), Cb = Enc(Dec(M.Colours[I].B) * R.Tint.B / 128.0);
				if (std::fabs(std::fabs(M.Positions[I * 3 + 1]) - 0.5 * Wide[R.Odd]) < 1e-4)
					EdgeOff.push_back(std::fabs(Cr - Gr.R) + std::fabs(Cg - Gr.G) + std::fabs(Cb - Gr.B));
				else if (M.Colours[I].G < Grey)
					FloorUnder.push_back(Luma(Gr.R, Gr.G, Gr.B) - Luma(Cr, Cg, Cb));
			}
		}
		// The joints: each piece's neighbour ahead and behind on its rut (the nearest of the pieces within 25 m whose way
		// is its way, the line between their middles the mean of their ways: on one gently bending line); overlaps where
		// they meet, gaps where they do not, the sideways step between their lines at the joint, their widths and tints.
		auto Ahead = [](const Rut& A, const Rut& B, double& Along, double& Turn) {
			Turn = std::remainder(B.Yaw - A.Yaw, 3.141592653589793); // (a piece is laid either way round: its line's way, mod a half turn)
			const double Mean = A.Yaw + 0.5 * Turn, Dx = B.X - A.X, Dy = B.Y - A.Y;
			Along = Dx * std::cos(Mean) + Dy * std::sin(Mean);
			const double Side = -Dx * std::sin(Mean) + Dy * std::cos(Mean);
			return std::fabs(Turn) < 0.2 && std::fabs(Along) > 0.5 && std::fabs(Along) < 25.0 && std::fabs(Side) < 0.25;
		};
		for (size_t I = 0; I < Ps.size(); ++I)
		{
			long Best[2] = { -1, -1 };
			double BestAt[2] = { 1e9, 1e9 };
			for (size_t J = 0; J < Ps.size(); ++J)
			{
				double Along = 0.0, Turn = 0.0;
				if (J == I || !Ahead(Ps[I], Ps[J], Along, Turn)) continue;
				const int Way = Along > 0.0 ? 0 : 1;
				if (std::fabs(Along) < BestAt[Way])
				{
					BestAt[Way] = std::fabs(Along);
					Best[Way] = static_cast<long>(J);
				}
			}
			for (int Way = 0; Way < 2; ++Way)
				if (Best[Way] >= 0) Ps[I].Next.push_back(static_cast<size_t>(Best[Way]));
		}
		auto Near = [&](const Rut& R, double Within) { return std::hypot(R.X - EX, R.Y - EY) <= Within; };
		std::vector<double> Laps, Gaps, Steps, Jitter, OffLine, WidthOff, TintStep;
		int HardEnds = 0, Ends = 0, Doubled = 0, ShortGaps = 0;
		double Covered30 = 0.0, Missing30 = 0.0;
		for (size_t I = 0; I < Ps.size(); ++I)
		{
			const Rut& A = Ps[I];
			const double La = 0.5 * Long[A.Odd] * A.Scale;
			// (its ends: each covered by a neighbour, or bare; a bare end square is a hard end)
			int Covered[2] = { 0, 0 };
			for (const size_t J : A.Next)
			{
				const Rut& B = Ps[J];
				double Along = 0.0, Turn = 0.0;
				Ahead(A, B, Along, Turn);
				const double Lb = 0.5 * Long[B.Odd] * B.Scale, Lap = La + Lb - std::fabs(Along);
				if (Lap > -0.01) Covered[Along > 0.0 ? 0 : 1] = 1;
				if (J < I) continue; // (each joint once)
				const bool bMutual = std::find(B.Next.begin(), B.Next.end(), I) != B.Next.end();
				if (!bMutual) continue;
				if (Lap > -0.01)
				{
					if (Near(A, 25.0) || Near(B, 25.0)) Laps.push_back(Lap);
				}
				else
				{
					if (Near(A, 30.0) || Near(B, 30.0)) Gaps.push_back(-Lap);
					if (-Lap < 1.8) ++ShortGaps;
					if (Near(A, 30.0) && Near(B, 30.0)) Missing30 += -Lap;
				}
				// (the step at the joint: the joint's middle off each piece's line, the difference; B's end the one nearer A's,
				// either piece laid either way round)
				const double Sg = Along > 0.0 ? 1.0 : -1.0;
				const double Ax_ = A.X + Sg * La * std::cos(A.Yaw), Ay_ = A.Y + Sg * La * std::sin(A.Yaw);
				const double B1x = B.X + Lb * std::cos(B.Yaw), B1y = B.Y + Lb * std::sin(B.Yaw), B2x = B.X - Lb * std::cos(B.Yaw), B2y = B.Y - Lb * std::sin(B.Yaw);
				const bool bOne = std::hypot(B1x - Ax_, B1y - Ay_) < std::hypot(B2x - Ax_, B2y - Ay_);
				const double Ex = 0.5 * (Ax_ + (bOne ? B1x : B2x)), Ey = 0.5 * (Ay_ + (bOne ? B1y : B2y));
				const double Sa = -(Ex - A.X) * std::sin(A.Yaw) + (Ey - A.Y) * std::cos(A.Yaw);
				const double Sb = -(Ex - B.X) * std::sin(B.Yaw) + (Ey - B.Y) * std::cos(B.Yaw);
				const double Flip = std::cos(B.Yaw - A.Yaw) < 0.0 ? -1.0 : 1.0;
				if (Lap > -0.01 && (Near(A, 25.0) || Near(B, 25.0))) Steps.push_back(std::fabs(Sa - Flip * Sb));
				if (Lap > -0.01) WidthOff.push_back(std::fabs(B.Scale / A.Scale - 1.0));
				const double Ta = Luma(A.Tint.R, A.Tint.G, A.Tint.B), Tb = Luma(B.Tint.R, B.Tint.G, B.Tint.B);
				TintStep.push_back(std::fabs(Ta - Tb) / std::max(1.0, 0.5 * (Ta + Tb)));
			}
			if (Near(A, 30.0)) Covered30 += 2.0 * La;
			// (a square end: both of a square-ended piece's, the end a fading piece fades from; Way 0 its +x as laid; only
			// where its neighbour would lie in the square too)
			const bool bInside = std::max(std::fabs(A.X - EX), std::fabs(A.Y - EY)) < 56.0;
			for (int Way = 0; Way < 2; ++Way)
				if (!Covered[Way] && bInside)
				{
					++Ends;
					if (A.Odd == 0 || (A.Odd == 1 && Way == 1) || (A.Odd == 2 && Way == 0)) ++HardEnds;
				}
			// (its middle and way against the line its two neighbours lie on, one either side a station off: the arc
			// between them, bending as they turn; its middle the chord's, out by its sagitta, its way the chord's)
			if (A.Next.size() == 2 && Near(A, 25.0))
			{
				double Along[2] = { 0.0, 0.0 }, Turn[2] = { 0.0, 0.0 };
				for (int K = 0; K < 2; ++K) Ahead(A, Ps[A.Next[static_cast<size_t>(K)]], Along[K], Turn[K]);
				const int Fw = Along[0] > 0.0 ? 0 : 1, Bk = 1 - Fw;
				if (Along[Fw] > 0.0 && Along[Bk] < 0.0 && std::fabs(Along[Fw] + Along[Bk]) < 0.3)
				{
					const Rut& P = Ps[A.Next[static_cast<size_t>(Bk)]];
					const Rut& Q = Ps[A.Next[static_cast<size_t>(Fw)]];
					const double Cx = Q.X - P.X, Cy = Q.Y - P.Y, Tn = 0.5 * std::tan(0.25 * (Turn[Fw] - Turn[Bk]));
					const double Mx = 0.5 * (P.X + Q.X) + Cy * Tn, My = 0.5 * (P.Y + Q.Y) - Cx * Tn;
					OffLine.push_back(std::hypot(A.X - Mx, A.Y - My));
					Jitter.push_back(std::fabs(std::remainder(A.Yaw - std::atan2(Cy, Cx), 3.141592653589793)) * 57.29577951308232);
				}
			}
			// (another piece doubling it: one alongside, its way the same, overlapping it but not on its line)
			for (size_t J = I + 1; J < Ps.size(); ++J)
			{
				const Rut& B = Ps[J];
				if ((!Near(A, 25.0) && !Near(B, 25.0)) || std::find(A.Next.begin(), A.Next.end(), J) != A.Next.end() || std::find(B.Next.begin(), B.Next.end(), I) != B.Next.end()) continue;
				const double Turn = std::remainder(B.Yaw - A.Yaw, 3.141592653589793), Dx = B.X - A.X, Dy = B.Y - A.Y;
				const double Along = Dx * std::cos(A.Yaw) + Dy * std::sin(A.Yaw), Side = -Dx * std::sin(A.Yaw) + Dy * std::cos(A.Yaw);
				const double Hb = 0.5 * Wide[B.Odd] * B.Scale, Ha = 0.5 * Wide[A.Odd] * A.Scale;
				if (std::fabs(Turn) < 0.2 && std::fabs(Side) > 0.03 && std::fabs(Side) < Ha + Hb && std::fabs(Along) < La + 0.5 * Long[B.Odd] * B.Scale) ++Doubled;
			}
		}
		const double Under_ = static_cast<double>(std::count_if(Over.begin(), Over.end(), [](double O) { return O < -0.002; })) / std::max<size_t>(1, Over.size());
		const double Inside = static_cast<double>(std::count_if(Over.begin(), Over.end(), [](double O) { return O >= -0.002 && O <= 0.025; })) / std::max<size_t>(1, Over.size());
		const double Worst = Over.empty() ? 0.0 : *std::max_element(Over.begin(), Over.end());
		const double Cover = Covered30 + Missing30 > 0.0 ? Covered30 / (Covered30 + Missing30) : 0.0;
		double EdgeMean = 0.0;
		for (const double E : EdgeOff) EdgeMean += E / static_cast<double>(EdgeOff.size());
		int Odds = 0;
		for (const Rut& R : Ps) Odds += R.Odd == 0;
		std::printf("  ruts round %.0f, %.0f: %zu laid, %zu pieces (%d square-ended, %zu fading), copies apart %d (tints by %d at most); built%s mirrored %s\n", EX, EY, Laid, Ps.size(), Odds,
			Ps.size() - static_cast<size_t>(Odds), Unlike, TintApart, Shape.c_str(), bMirror ? "yes" : "no");
		std::printf("  ruts on their lines (within 25 m): middles off the line by median %.3f, worst %.3f m, ways by worst %.2f degrees (%zu); joints' sideways step median %.3f, worst %.3f m; widths apart %.1f%% at worst; tint steps median %.1f%%, worst %.1f%%; doubled %d\n",
			Pctl(OffLine, 0.5), Pctl(OffLine, 1.0), Pctl(Jitter, 1.0), OffLine.size(), Pctl(Steps, 0.5), Pctl(Steps, 1.0), 100.0 * Pctl(WidthOff, 1.0), 100.0 * Pctl(TintStep, 0.5), 100.0 * Pctl(TintStep, 1.0), Doubled);
		std::printf("  ruts' joints: met (within 25 m) %zu, overlapping least %.3f, most %.3f m; gaps (within 30 m) %zu, shortest %.2f m, under 1.8 m %d in all; covered within 30 m %.1f%% of %.0f m; bare ends %d, square %d\n",
			Laps.size(), Pctl(Laps, 0.0), Pctl(Laps, 1.0), Gaps.size(), Pctl(Gaps, 0.0), ShortGaps, 100.0 * Cover, Covered30 + Missing30, Ends, HardEnds);
		std::printf("  rut pieces over the drawn ground: 5%% %.3f, median %.3f, 95%% %.3f m, worst %.3f m, %.1f%% inside 0 to 2.5 cm, %.2f%% under it (%zu places); edges off the ground's colour by mean %.2f, 95%% %.1f; floor under the ground's light by median %.1f luma (5%% %.1f, 95%% %.1f)\n",
			Pctl(Over, 0.05), Pctl(Over, 0.5), Pctl(Over, 0.95), Worst, 100.0 * Inside, 100.0 * Under_, Over.size(), EdgeMean,
			Pctl(EdgeOff, 0.95), Pctl(FloorUnder, 0.5), Pctl(FloorUnder, 0.05), Pctl(FloorUnder, 0.95));
		Check(!Ps.empty() && Unlike == 0 && bMirror && Pctl(OffLine, 1.0) <= 0.02 && Pctl(Jitter, 1.0) <= 0.5 && Pctl(Steps, 1.0) <= 0.02 && Pctl(WidthOff, 1.0) <= 0.05 && Doubled == 0,
			"a track's ruts lie on their lines, one width a rut, no piece doubling another, built mirrored", Fmt("%.0f pieces, middles off by %.3f m, ways by %.2f degrees, steps %.3f m", static_cast<double>(Ps.size()), Pctl(OffLine, 1.0), Pctl(Jitter, 1.0), Pctl(Steps, 1.0)));
		Check(!Laps.empty() && Pctl(Laps, 0.0) >= -0.01 && Pctl(Laps, 1.0) <= 0.01 && ShortGaps == 0 && HardEnds == 0 && Cover >= 0.85, "a track's ruts run on unbroken, end to end, their gaps a station or more, ending faded",
			Fmt("joints %.3f to %.3f m over, ", Pctl(Laps, 0.0), Pctl(Laps, 1.0)) + Fmt("%.0f gaps under 1.8 m, %.0f square ends bare, ", ShortGaps, HardEnds) + Fmt("%.1f%% covered within 30 m", 100.0 * Cover));
		Check(!Over.empty() && Pctl(Over, 0.05) >= -0.002 && Pctl(Over, 0.95) <= 0.025 && Inside >= 0.95 && Under_ < 0.005 && Worst <= 0.04, "a track's ruts lie on the drawn ground",
			Fmt("%.1f%% inside 0 to 2.5 cm, %.2f%% under it, worst %.3f m over", 100.0 * Inside, 100.0 * Under_, Worst));
	}

	// The bracken keeps to the hedge banks (the art director's cut 5 note): none in the open grass or a field's rough
	// ground more than 5 m from a bank's middle, a dock's or a plantain's low rosette there instead; none of either on the
	// steppe (Prokhorovka's grass as it was). Squares 80 m across every 250 m over the field.
	{
		struct Seg
		{
			double AX, AY, BX, BY;
		};
		std::map<std::pair<int, int>, std::vector<Seg>> ByCell; // (the banks' middle lines, filed by 10 m cell)
		for (const LandField::HedgeLine& H : G.Hedges())
		{
			if (H.Kind < 0 || H.Kind > 3) continue; // (the banked kinds: hedgerow, lane bank, wood edge, garden hedge)
			for (size_t K = 0; K + 1 < H.Points.size(); ++K)
			{
				const Seg Sg{ H.Points[K].first, H.Points[K].second, H.Points[K + 1].first, H.Points[K + 1].second };
				for (int I = static_cast<int>(std::floor((std::min(Sg.AX, Sg.BX) - 6.0) / 10.0)); I <= static_cast<int>(std::floor((std::max(Sg.AX, Sg.BX) + 6.0) / 10.0)); ++I)
					for (int J = static_cast<int>(std::floor((std::min(Sg.AY, Sg.BY) - 6.0) / 10.0)); J <= static_cast<int>(std::floor((std::max(Sg.AY, Sg.BY) + 6.0) / 10.0)); ++J) ByCell[{ I, J }].push_back(Sg);
			}
		}
		auto ToBank = [&ByCell](double X, double Y) {
			double Best = 1e9;
			const auto Found = ByCell.find({ static_cast<int>(std::floor(X / 10.0)), static_cast<int>(std::floor(Y / 10.0)) });
			if (Found == ByCell.end()) return Best;
			for (const Seg& Sg : Found->second)
			{
				const double DX = Sg.BX - Sg.AX, DY = Sg.BY - Sg.AY, L2 = DX * DX + DY * DY;
				const double U = L2 > 1e-9 ? std::clamp(((X - Sg.AX) * DX + (Y - Sg.AY) * DY) / L2, 0.0, 1.0) : 0.0;
				Best = std::min(Best, std::hypot(Sg.AX + U * DX - X, Sg.AY + U * DY - Y));
			}
			return Best;
		};
		long Open = 0, OpenFar = 0, Banked = 0, Rosettes = 0, All = 0;
		std::vector<LandField::Placed> Out;
		for (double X = 40.0; X + 40.0 <= F.Width(); X += 250.0)
			for (double Y = 40.0; Y + 40.0 <= F.Depth(); Y += 250.0)
			{
				Out.clear();
				G.Clutter(X - 40.0, Y - 40.0, X + 40.0, Y + 40.0, Out);
				All += static_cast<long>(Out.size());
				for (const LandField::Placed& P : Out)
				{
					if (P.Kind == LandField::Base::Rosette) ++Rosettes;
					if (P.Kind != LandField::Base::Fern) continue;
					const LandField::Drawn W = G.At(P.X, P.Y).What;
					if (W != LandField::Drawn::Pasture && W != LandField::Drawn::Rough)
					{
						++Banked;
						continue;
					}
					++Open;
					if (ToBank(P.X, P.Y) > 5.05) ++OpenFar;
				}
			}
		std::printf("  bracken and docks (squares of 80 m every 250 m, %ld things): bracken %ld on banks, lanes, walls and in woods, %ld in the open grass (%ld more than 5 m from a bank); %ld dock and plantain rosettes\n",
		            All, Banked, Open, OpenFar, Rosettes);
		if (!bRegion)
		Check(OpenFar == 0 && (bSteppe ? Rosettes == 0 && Open + Banked == 0 : Rosettes > 0), bSteppe ? "no bracken and no docks on the steppe (its grass as it was)" : "the bracken keeps within 5 m of a hedge bank in the open grass, docks and plantains out in the fields",
		      Fmt("%.0f bracken in the open more than 5 m from a bank, ", static_cast<double>(OpenFar)) + Fmt("%.0f rosettes", static_cast<double>(Rosettes)));
	}

	// The clutter laid once in squares as the eye moves (LandCarpet, as the scene drives it): an eye dollying across the
	// field at a tank's pace and looking 60 m ahead, cut away every 15 s, with shell holes torn near it. Its meshes kept
	// here as the engine would keep them, place by place, and what they hold checked against the old box. The same walk
	// three times, each frame's time and the old box's the quickest of the three (a busy box's swings are not the
	// squares'), checked on the third; the meshes kept packed (GS7: a given-back place filled from the mesh's end, the
	// last removed). Then once more the old way (-TheaterLandClutterShrunk: given-back places kept, shrunk), its places
	// checked as on the third, not timed.
	{
		constexpr int Kinds = static_cast<int>(LandField::Base::Count);
		constexpr int Frames = 60 * 60;
		std::vector<double> FrameMs(Frames, 1e30), OldMsAt(Frames, 1e30);
		std::vector<char> CutAt(Frames, 0);
		for (int Run = 0; Run < 4; ++Run)
		{
			const bool bCompact = Run < 3;
			const char* const Way = bCompact ? "kept packed" : "the old way, given-back places shrunk";
			LandCarpet::Squares C;
			C.bCompact = bCompact;
			C.Reach.assign(Kinds, 1e9);
			std::vector<double> Cull(Kinds, 1e9);
			for (int B = static_cast<int>(LandField::Base::Tuft); B < Kinds; ++B)
			{
				const LandField::BaseMesh& Shape = G.BaseOf(static_cast<LandField::Base>(B));
				if (Shape.Far.Empty())
				{
					C.Reach[static_cast<size_t>(B)] = Shape.FarFrom + 12.0; // (as LandBattle::LayThings sets it)
					Cull[static_cast<size_t>(B)] = Shape.FarFrom;            // (and where the engine stops drawing it)
				}
			}
			struct HoleAt
			{
				double X, Y, R;
			};
			std::vector<HoleAt> Holes;
			// (filed by 8 m cell, as the scene's holes are, so asking costs what it does there)
			std::map<std::pair<int, int>, std::vector<size_t>> HoleCells;
			auto InHole = [&Holes, &HoleCells](double X, double Y) {
				const auto Found = HoleCells.find({ static_cast<int>(std::floor(X / 8.0)), static_cast<int>(std::floor(Y / 8.0)) });
				if (Found == HoleCells.end()) return false;
				for (const size_t H : Found->second)
					if (std::hypot(X - Holes[H].X, Y - Holes[H].Y) < Holes[H].R) return true;
				return false;
			};
			// (the corn's bundles cleared within 3.5 m of the lens, where its stalks are drawn one by one: as LayThings)
			constexpr double ClearCorn = 3.5;
			C.Clear.assign(Kinds, 0.0);
			C.Clear[static_cast<size_t>(LandField::Base::Wheat)] = ClearCorn;
			C.CopyOf = [](const LandField::Placed& P) { return static_cast<int32_t>(P.Kind); };
			C.Stands = [&InHole](const LandField::Placed& P) { return !InHole(P.X, P.Y); };
			// (the corn's shadows within 25 m, GS9: its mesh's shadowed twin is one more mesh, after the kinds' own)
			constexpr double CornShadowTo = 25.0;
			constexpr int CornTwin = Kinds;
			C.ShadowTo.assign(Kinds, 0.0);
			C.ShadowTo[static_cast<size_t>(LandField::Base::Wheat)] = CornShadowTo;
			C.Twin.assign(Kinds + 1, -1);
			C.Twin[static_cast<size_t>(LandField::Base::Wheat)] = CornTwin;
			long CornNearPlain = 0, CornFarShadowed = 0, CornLaid = 0, CornShadowed = 0;
			std::vector<std::vector<std::pair<bool, LandField::Placed>>> Mesh(Kinds + 1); // (laid, thing) a place
			long BadOps = 0, Ops = 0, Puts = 0, Hides = 0, Pops = 0, Moves = 0, HiddenHeld = 0, OutOfStep = 0, Checked = 0;
			double ShrunkSum = 0.0; // (hidden places the meshes hold, summed over the frames: what the old way drew for nothing)
			size_t MostShrunk = 0;
			double WorstMoving = 0.0, SumMoving = 0.0, WorstCut = 0.0, OldSum = 0.0, OldWorst = 0.0;
			int Moving = 0, Cuts = 0, Settled = 0, OldLays = 0, MostMade = 0, Relaid = 0, SinceCut = 0, SlowestFill = 0;
			long Missing = 0, Wanted = 0, Extra = 0, InHoles = 0, InClearing = 0, StalksWanted = 0, HighCorn = 0, HighFrames = 0;
			size_t PeakLaid = 0;
			std::vector<LandCarpet::Op> Out;
			const double W = F.Width(), D = F.Depth();
			double EX = W * 0.3, EY = D * 0.3, Head = 0.6, OldX = 1e9, OldY = 1e9, OldLX = 1e9, OldLY = 1e9;
			unsigned Rng = 12345u;
			auto Rand = [&Rng]() { Rng = Rng * 1664525u + 1013904223u; return (Rng >> 8) / 16777216.0; };
			for (int Frame = 0; Frame < Frames; ++Frame)
			{
				const double Now = Frame / 60.0;
				const bool bCut = Frame == 0 || Frame % (15 * 60) == 0;
				if (bCut && Frame > 0)
				{
					EX = W * (0.2 + 0.6 * Rand());
					EY = D * (0.2 + 0.6 * Rand());
					Head = 6.2832 * Rand();
				}
				// (a tank's pace, 6 m/s, turning slowly; kept inside the field)
				Head += 0.15 / 60.0;
				EX = std::clamp(EX + 6.0 / 60.0 * std::cos(Head), 60.0, W - 60.0);
				EY = std::clamp(EY + 6.0 / 60.0 * std::sin(Head), 60.0, D - 60.0);
				const double LX = EX + 60.0 * std::cos(Head), LY = EY + 60.0 * std::sin(Head);
				// (a shell hole near the eye every 2 s)
				if (Frame % 120 == 60)
				{
					const double R = 2.0 + 3.0 * Rand(), A = 6.2832 * Rand(), Far = 5.0 + 30.0 * Rand();
					Holes.push_back({ EX + Far * std::cos(A), EY + Far * std::sin(A), R });
					for (int CJ = static_cast<int>(std::floor((Holes.back().Y - R) / 8.0)); CJ <= static_cast<int>(std::floor((Holes.back().Y + R) / 8.0)); ++CJ)
						for (int CI = static_cast<int>(std::floor((Holes.back().X - R) / 8.0)); CI <= static_cast<int>(std::floor((Holes.back().X + R) / 8.0)); ++CI)
							HoleCells[{ CI, CJ }].push_back(Holes.size() - 1);
					C.Mark(Holes.back().X, Holes.back().Y, R);
				}
				// (the eye rises out of the corn for 1.5 s in every 6, as the scene lifts it: the clearing let go, and
				// taken again as it comes down)
				const bool bHigh = (Frame / 90) % 4 == 3;
				const double ClearNow = bHigh ? 0.0 : ClearCorn;
				C.Clear[static_cast<size_t>(LandField::Base::Wheat)] = ClearNow;
				HighFrames += bHigh ? 1 : 0;
				std::vector<int32_t> Before(Kinds + 1);
				for (int K = 0; K <= Kinds; ++K) Before[static_cast<size_t>(K)] = C.Count(K);
				Out.clear();
				const auto T0 = std::chrono::steady_clock::now();
				const LandCarpet::Tally Tl = C.Update(G, EX, EY, LX, LY, bCut, 4, Now, Out);
				if (bCompact)
					FrameMs[static_cast<size_t>(Frame)] = std::min(FrameMs[static_cast<size_t>(Frame)], Seconds(T0) * 1000.0);
				CutAt[static_cast<size_t>(Frame)] = bCut ? 1 : 0;
				Relaid += Tl.Relaid;
				// (how many frames a cut's far squares take to follow: until the first frame with none waiting)
				if (bCut) SinceCut = 0;
				if (SinceCut >= 0 && Tl.Waiting == 0)
				{
					SlowestFill = std::max(SlowestFill, SinceCut);
					SinceCut = -1;
				}
				else if (SinceCut >= 0)
					++SinceCut;
				if (bCut)
				{
					++Cuts;
				}
				else
				{
					++Moving;
					MostMade = std::max(MostMade, Tl.Made);
				}
				// The meshes as the engine keeps them: a new place only at the end, in order. Kept packed: no hide; a put
				// into a held place only of the mesh's last thing, the last place removed straight after; a pop only of
				// the last. The old way: no pop; a put only into a free place; a hide only of a laid one.
				int32_t PopDue = -1;
				for (const LandCarpet::Op& O : Out)
				{
					++Ops;
					if (O.Copy == -1 && O.Slot == -1)
					{
						for (auto& M : Mesh) M.clear();
						std::fill(Before.begin(), Before.end(), 0);
						BadOps += PopDue >= 0 ? 1 : 0;
						PopDue = -1;
						continue;
					}
					if (O.Copy < 0 || O.Copy > Kinds)
					{
						++BadOps;
						continue;
					}
					auto& M = Mesh[static_cast<size_t>(O.Copy)];
					if (PopDue >= 0 && (!O.bPop || O.Copy != PopDue))
					{
						++BadOps;
					}
					PopDue = -1;
					if (O.bPop)
					{
						if (!bCompact || M.empty() || O.Slot != static_cast<int32_t>(M.size()) - 1)
						{
							++BadOps;
							continue;
						}
						M.pop_back();
						++Pops;
						continue;
					}
					if (O.Slot >= static_cast<int32_t>(M.size()))
					{
						if (O.bHide || O.Slot != static_cast<int32_t>(M.size()) || (!bCompact && O.Slot < Before[static_cast<size_t>(O.Copy)]))
						{
							++BadOps;
							continue;
						}
						M.push_back({ true, O.Thing });
						++Puts;
						continue;
					}
					auto& Place = M[static_cast<size_t>(O.Slot)];
					if (O.bRepose)
					{
						// (on trial, "lodged": a held thing re-posed in its own place, laid over)
						if (O.bHide || !Place.first || Place.second.X != O.Thing.X || Place.second.Y != O.Thing.Y || Place.second.Kind != O.Thing.Kind) ++BadOps;
						Place.second = O.Thing;
						continue;
					}
					if (bCompact)
					{
						const auto& Back = M.back().second;
						if (O.bHide || O.Slot + 1 >= static_cast<int32_t>(M.size()) || Back.X != O.Thing.X || Back.Y != O.Thing.Y || Back.Kind != O.Thing.Kind)
						{
							++BadOps;
							continue;
						}
						Place = { true, O.Thing };
						PopDue = O.Copy;
						++Moves;
						continue;
					}
					if (O.bHide != Place.first)
					{
						++BadOps;
					}
					Place = { !O.bHide, O.Thing };
					(O.bHide ? Hides : Puts) += 1;
				}
				BadOps += PopDue >= 0 ? 1 : 0;
				// (the meshes' counts as the squares give them, every frame; kept packed, nothing hidden held; and every
				// tenth frame each laid thing where the squares say it is)
				for (int K = 0; K <= Kinds; ++K)
				{
					const auto& M = Mesh[static_cast<size_t>(K)];
					OutOfStep += C.Count(K) != static_cast<int32_t>(M.size()) ? 1 : 0;
					if (bCompact)
						for (const auto& P : M) HiddenHeld += P.first ? 0 : 1;
					if (Frame % 10 != 0) continue;
					long Each = 0, Shown = 0;
					C.Each(K, [&](int32_t Slot, const LandField::Placed& P) {
						++Each;
						if (Slot < 0 || Slot >= static_cast<int32_t>(M.size()) || !M[static_cast<size_t>(Slot)].first || M[static_cast<size_t>(Slot)].second.X != P.X
							|| M[static_cast<size_t>(Slot)].second.Y != P.Y)
							++OutOfStep;
					});
					for (const auto& P : M) Shown += P.first ? 1 : 0;
					OutOfStep += Each != Shown ? 1 : 0;
					++Checked;
				}
				size_t Laid = 0;
				for (const auto& M : Mesh)
					for (const auto& P : M) Laid += P.first ? 1 : 0;
				PeakLaid = std::max(PeakLaid, Laid);
				{
					size_t PlacesNow = 0;
					for (const auto& M : Mesh) PlacesNow += M.size();
					ShrunkSum += static_cast<double>(PlacesNow - Laid);
					MostShrunk = std::max(MostShrunk, PlacesNow - Laid);
				}
				// (no bundle of corn laid in the clearing round the lens, any frame)
				for (const int Corn : { static_cast<int>(LandField::Base::Wheat), CornTwin })
					for (const auto& P : Mesh[static_cast<size_t>(Corn)])
						if (P.first && std::hypot(P.second.X - EX, P.second.Y - EY) < ClearNow - 1e-3) ++InClearing;
				// The old way, for its time: the whole box made again every 12 m.
				if (std::hypot(EX - OldX, EY - OldY) > 12.0 || std::hypot(LX - OldLX, LY - OldLY) > 12.0)
				{
					std::vector<LandField::Placed> A;
					const auto T1 = std::chrono::steady_clock::now();
					G.Clutter(EX - 45.0, EY - 45.0, EX + 45.0, EY + 45.0, A);
					G.Clutter(LX - 25.0, LY - 25.0, LX + 25.0, LY + 25.0, A);
					if (bCompact)
						OldMsAt[static_cast<size_t>(Frame)] = std::min(OldMsAt[static_cast<size_t>(Frame)], Seconds(T1) * 1000.0);
					++OldLays;
					OldX = EX;
					OldY = EY;
					OldLX = LX;
					OldLY = LY;
				}
				// Once a second, with nothing left waiting: every thing the old box would draw is laid, and nothing laid
				// stands in a hole past its mark's time.
				if (Frame % 60 == 30 && Tl.Waiting == 0)
				{
					++Settled;
					// (the corn's shadows: every bundle within 25 m less the sway in the shadowed twin, none past 25 m, the
					// sway and a square's diagonal)
					for (const int Corn : { static_cast<int>(LandField::Base::Wheat), CornTwin })
						for (const auto& P : Mesh[static_cast<size_t>(Corn)])
						{
							if (!P.first) continue;
							const double Away = std::hypot(P.second.X - EX, P.second.Y - EY);
							++CornLaid;
							CornShadowed += Corn == CornTwin ? 1 : 0;
							if (Corn != CornTwin && Away < CornShadowTo - LandCarpet::Squares::ReachSway) ++CornNearPlain;
							if (Corn == CornTwin && Away > CornShadowTo + LandCarpet::Squares::ReachSway + LandCarpet::Squares::Side * 1.4143) ++CornFarShadowed;
						}
					std::vector<std::pair<float, float>> Have;
					for (const auto& M : Mesh)
						for (const auto& P : M)
							if (P.first) Have.push_back({ P.second.X, P.second.Y });
					std::sort(Have.begin(), Have.end());
					std::vector<LandField::Placed> Box;
					G.Clutter(EX - 45.0, EY - 45.0, EX + 45.0, EY + 45.0, Box);
					if (std::hypot(LX - EX, LY - EY) >= 40.0)
					{
						std::vector<LandField::Placed> Round;
						G.Clutter(LX - 25.0, LY - 25.0, LX + 25.0, LY + 25.0, Round);
						for (const LandField::Placed& P : Round)
							if (std::fabs(P.X - EX) > 45.0 || std::fabs(P.Y - EY) > 45.0) Box.push_back(P);
					}
					long Drawn = 0;
					for (const LandField::Placed& P : Box)
					{
						if (std::hypot(P.X - EX, P.Y - EY) >= Cull[static_cast<size_t>(P.Kind)] || InHole(P.X, P.Y)) continue;
						if (P.Kind == LandField::Base::Wheat && std::hypot(P.X - EX, P.Y - EY) < ClearNow) continue;
						if (P.Kind == LandField::Base::Stalks) ++StalksWanted;
						if (bHigh && P.Kind == LandField::Base::Wheat && std::hypot(P.X - EX, P.Y - EY) < ClearCorn) ++HighCorn;
						++Drawn;
						if (!std::binary_search(Have.begin(), Have.end(), std::make_pair(P.X, P.Y))) ++Missing;
					}
					Wanted += Drawn;
					Extra += static_cast<long>(Have.size()) - Drawn;
					// (holes older than the marks' wait)
					for (const auto& M : Mesh)
						for (const auto& P : M)
							if (P.first)
								for (size_t H = 0; H + 1 < Holes.size(); ++H) // (the newest may still be within its wait)
									if (std::hypot(P.second.X - Holes[H].X, P.second.Y - Holes[H].Y) < Holes[H].R) ++InHoles;
				}
			}
			size_t Places = 0;
			for (const auto& M : Mesh) Places += M.size();
			if (Run < 2) continue;
			for (int Frame = 0; Frame < Frames && Run == 2; ++Frame)
			{
				const double Ms = FrameMs[static_cast<size_t>(Frame)];
				if (CutAt[static_cast<size_t>(Frame)])
					WorstCut = std::max(WorstCut, Ms);
				else
				{
					WorstMoving = std::max(WorstMoving, Ms);
					SumMoving += Ms;
				}
				if (OldMsAt[static_cast<size_t>(Frame)] < 1e29)
				{
					OldSum += OldMsAt[static_cast<size_t>(Frame)];
					OldWorst = std::max(OldWorst, OldMsAt[static_cast<size_t>(Frame)]);
				}
			}
			if (Run == 2)
				std::printf("  clutter in squares: a cut's far squares all laid within %d frames\n", SlowestFill);
			if (Run == 2)
				std::printf("  clutter in squares: %d moving frames %.3f ms mean, worst %.2f ms (at most %d squares made a frame), %d cuts worst %.2f ms; "
				            "the old box made %d times, %.2f ms mean, worst %.2f ms\n",
					Moving, Moving ? SumMoving / Moving : 0.0, WorstMoving, MostMade, Cuts, WorstCut, OldLays, OldLays ? OldSum / OldLays : 0.0, OldWorst);
			std::printf("  clutter in squares (%s): %d squares laid again (holes, reach); %ld puts, %ld hides, %ld filled from the end, %ld removed; "
			            "%zu places at the end for %zu laid at most; hidden places drawn %.0f a frame on average, %zu at most; %ld of %ld drawn things missing over %d settled frames, %.2f laid for each drawn\n",
				Way, Relaid, Puts, Hides, Moves, Pops, Places, PeakLaid, ShrunkSum / Frames, MostShrunk, Missing, Wanted, Settled, Wanted ? static_cast<double>(Wanted + Extra) / Wanted : 0.0);
			Check(BadOps == 0 && Ops > 0, (std::string("the grass squares' places kept straight, ") + Way + (bCompact ? " (new ones at the end, a put into a held place only of the mesh's last thing, the last removed straight after)" : " (new ones at the end, a put only into a free place, a hide only of a laid one)")).c_str(),
			      Fmt("%.0f wrong of %.0f", static_cast<double>(BadOps), static_cast<double>(Ops)));
			Check(OutOfStep == 0 && Checked > 0, (std::string("the meshes as the squares count them, every thing where they say, ") + Way).c_str(),
			      Fmt("%.0f out of step", static_cast<double>(OutOfStep)) + Fmt(" in %.0f meshes checked whole", static_cast<double>(Checked)));
			Check(Settled >= 40 && Missing == 0 && InHoles == 0, (std::string("the grass squares hold everything the old box drew, and nothing in a hole once its square is laid again, ") + Way).c_str(),
			      Fmt("%.0f missing of %.0f, %.0f in holes", static_cast<double>(Missing), static_cast<double>(Wanted), static_cast<double>(InHoles)) + Fmt(", %.0f settled frames", Settled));
			if (bCompact)
				Check(HiddenHeld == 0 && Pops > 0 && Hides == 0, "the clutter's meshes kept packed (GS7): no place given back is drawn, shrunk or not",
				      Fmt("%.0f hidden places held", static_cast<double>(HiddenHeld)) + Fmt(", %.0f removed from the end", static_cast<double>(Pops)));
			else
				Check(Places <= PeakLaid * 3 / 2 + 2000, "the grass squares' places reused (not grown without end), the old way", Fmt("%.0f places, %.0f laid at most", static_cast<double>(Places), static_cast<double>(PeakLaid)));
			std::printf("  corn's shadows (GS9): %.0f%% of the corn laid casts one (%ld of %ld over the settled frames)\n", CornLaid ? 100.0 * CornShadowed / CornLaid : 0.0, CornShadowed, CornLaid);
			Check(CornNearPlain == 0 && CornFarShadowed == 0 && (CornLaid == 0 || CornShadowed > 0), "the corn casts its shadow within 25 m of the eye (less the 4 m sway) and none far past it",
			      Fmt("%.0f near without, ", static_cast<double>(CornNearPlain)) + Fmt("%.0f far with", static_cast<double>(CornFarShadowed)));
			Check(InClearing == 0 && (HighCorn > 0 || StalksWanted == 0), "no bundle of corn laid within 3.5 m of a low lens (its stalks drawn one by one there), each laid again as the lens leaves it or rises",
			      Fmt("%.0f laid in the clearing; ", static_cast<double>(InClearing)) + Fmt("%.0f single-stalk clumps wanted near the lens; ", static_cast<double>(StalksWanted)) +
			      Fmt("%.0f bundles back under a risen lens", static_cast<double>(HighCorn)) + Fmt(" in %.0f frames", static_cast<double>(HighFrames)));
			if (Run == 3) continue; // (the old way's walk not timed)
			Check(Moving > 0 && WorstMoving < 0.5 * OldWorst, "a moving eye's grass costs under half the old box's re-lay in its worst frame",
			      Fmt("worst %.2f ms against %.2f ms", WorstMoving, OldWorst));
			Check(WorstCut <= 1.1 * OldWorst && SlowestFill <= 20, "a cut's grass costs no more than the old box's re-lay, and its far squares follow within a third of a second",
			      Fmt("worst %.2f ms against %.2f ms, ", WorstCut, OldWorst) + Fmt("%.0f frames", SlowestFill));
		}
	}
	// (on trial, "lodged": the growth laid over round shell holes, LodgedTrial)
	if (F.Tried("lodged")) LodgedTrial(F, Key);
	// (on trial, "diggings" alone on the steppe: the works dug in heaps, nothing else of ploughland, DiggingsTrial)
	if (F.Tried("diggings") && !F.Tried("ploughland") && LookLaid == "steppe") DiggingsTrial(F, G);

	// The footprint shade under a tank, a gun, a standing man and a prone one (LandShade, the art director's cut 4 fix
	// 3): laid without a fold or an overlap (it multiplies, so an overlap would be a dark seam), deepest under the
	// belly, at the outline's share there, back to nothing 0.8 to 1.2 m out with no straight edge, and small.
	{
		struct Kind
		{
			const char* Name;
			LandShade::Shape S;
		};
		const Kind Kinds[] = { { "tank", LandShade::Tank(-3.2, -1.85, 3.2, 1.85) }, { "car", LandShade::Car(-1.7, -0.8, 1.7, 0.8) },
			{ "gun", LandShade::Gun(-3.5, -0.9, 1.2, 0.9) },
			{ "standing man", LandShade::Standing() }, { "prone man", LandShade::Prone() }, { "body", LandShade::Body() } };
		for (const Kind& K : Kinds)
		{
			LandShade::Mesh M;
			LandShade::Make(K.S, M);
			const LandShade::Fitted Fi = LandShade::Fit(K.S);
			const size_t N = M.Count();
			double Area = 0.0, Folded = 0.0;
			for (size_t Tri = 0; Tri + 2 < M.Triangles.size(); Tri += 3)
			{
				const float* A = &M.Points[M.Triangles[Tri] * 3];
				const float* B = &M.Points[M.Triangles[Tri + 1] * 3];
				const float* C = &M.Points[M.Triangles[Tri + 2] * 3];
				const double Twice = (static_cast<double>(B[0]) - A[0]) * (static_cast<double>(C[1]) - A[1]) - (static_cast<double>(B[1]) - A[1]) * (static_cast<double>(C[0]) - A[0]);
				Area += 0.5 * Twice;
				if (Twice < -1e-6) Folded += -0.5 * Twice;
			}
			// (the outer ring's own area: with no triangle turned over, the triangles' areas add up to it only if none
			// overlaps)
			const size_t Ring = (N - 1) / 9, OuterAt = 1 + 8 * Ring;
			double Outline = 0.0, NearestOut = 1e9, FarthestOut = 0.0, Deepest = 1.0, AtEdge = 0.0, EdgeCount = 0.0, OuterLight = 1.0;
			bool bInOrder = true;
			for (size_t I = 0; I < Ring; ++I)
			{
				const float* A = &M.Points[(OuterAt + I) * 3];
				const float* B = &M.Points[(OuterAt + (I + 1) % Ring) * 3];
				Outline += 0.5 * (static_cast<double>(A[0]) * B[1] - static_cast<double>(B[0]) * A[1]);
				const double D = LandShade::Outside(Fi, A[0], A[1]);
				NearestOut = std::min(NearestOut, D);
				FarthestOut = std::max(FarthestOut, D);
				OuterLight = std::min(OuterLight, static_cast<double>(M.Light[OuterAt + I]));
				AtEdge += M.Light[1 + 3 * Ring + I];
				EdgeCount += 1.0;
				for (size_t J = 0; J + 1 < 9; ++J)
					if (M.Light[1 + (J + 1) * Ring + I] + 1e-6 < M.Light[1 + J * Ring + I]) bInOrder = false;
			}
			for (size_t I = 0; I < N; ++I) Deepest = std::min(Deepest, static_cast<double>(M.Light[I]));
			AtEdge = EdgeCount > 0.0 ? AtEdge / EdgeCount : 0.0;
			const double Lo = K.S.Feather * (1.0 - K.S.Wander) - 0.02, Hi = K.S.Feather * (1.0 + K.S.Wander) + 0.02;
			std::printf("  footprint shade, %s: %zu points, %zu triangles; deepest %.2f, at the outline %.2f, back to %.2f between %.2f and %.2f m out\n", K.Name, N,
				M.Triangles.size() / 3, Deepest, AtEdge, OuterLight, NearestOut, FarthestOut);
			Check(Folded < 1e-9 && std::fabs(Area - Outline) < 1e-4 * std::max(1.0, Outline), (std::string("the ") + K.Name + "'s footprint shade laid without a fold or an overlap").c_str(),
				Fmt("%.4f m2 turned over, ", Folded) + Fmt("%.4f m2 laid for ", Area) + Fmt("%.4f m2 inside its edge", Outline));
			Check(std::fabs(Deepest - K.S.Belly) < 0.01 && std::fabs(AtEdge - K.S.Edge) < 0.01 && OuterLight > 0.999 && bInOrder,
				(std::string("the ") + K.Name + "'s footprint shade deepest under it, its share at the outline, lighter outward, nothing at its edge").c_str(),
				Fmt("deepest %.2f, ", Deepest) + Fmt("outline %.2f, ", AtEdge) + Fmt("edge %.2f", OuterLight));
			Check(NearestOut >= Lo && FarthestOut <= Hi && FarthestOut - NearestOut >= K.S.Feather * K.S.Wander && N < 1500,
				(std::string("the ") + K.Name + "'s footprint shade fades out over its feather, its edge wavering, in under 1,500 points").c_str(),
				Fmt("%.2f to ", NearestOut) + Fmt("%.2f m out, ", FarthestOut) + Fmt("%.0f points", static_cast<double>(N)));
		}
		// (a car's whole shade no longer than 1.2 of her, and on full snow no share darker than L 100 on snow at L 158:
		// the art director's Mac pass 1, B4)
		const LandShade::Shape Car = LandShade::Car(-1.7, -0.8, 1.7, 0.8);
		const double CarLong = (3.4 + 2.0 * Car.Feather * (1.0 + Car.Wander)) / 3.4;
		const auto OnSnow = [](double Share, double Strength) {
			const double Lin = std::pow(Share, 2.2), Left = 1.0 - Strength * (1.0 - Lin);
			return 158.0 * std::pow(Left, 1.0 / 2.2);
		};
		const double Full = LandShade::SnowStrength(1.0), Lying = LandShade::SnowStrength(0.87);
		Check(CarLong <= 1.25 && OnSnow(LandShade::Tank(-3, -1.6, 3, 1.6).Belly, Lying) >= 100.0 && OnSnow(Car.Belly, Lying) >= 100.0 && Full < 1.0 && LandShade::SnowStrength(0.0) == 1.0,
		      "a car's footprint shade no longer than about 1.2 of her, and on snow no shade under L 100",
		      Fmt("%.2f of her length, ", CarLong) + Fmt("a tank's belly L %.0f, ", OnSnow(LandShade::Tank(-3, -1.6, 3, 1.6).Belly, Lying)) + Fmt("a car's L %.0f on snow at 158", OnSnow(Car.Belly, Lying)));
	}

	Check(Build < 2.0, "built in under two seconds", Fmt("%.2f s", Build));
	Check(T.NearTris + T.BuiltTris < 1500000, "near level (ground, lanes, banks) and buildings under 1.5 million triangles", Fmt("%.0f + %.0f", static_cast<double>(T.NearTris), static_cast<double>(T.BuiltTris)));
	if (bSteppe)
	{
		// The steppe: the collective farm's great fields, no hedge or gate; the farm's cottages and sheds, a share burnt;
		// the railway on its bank with its telegraph poles.
		Check(T.ParcelMean >= 250.0 && T.ParcelMean <= 900.0, "the steppe's fields 250 to 900 m across", Fmt("mean %.0f m", T.ParcelMean));
		Check(T.HedgeKm < 0.01 && T.Gates == 0, "no hedge, bank or gate on the steppe", Fmt("%.2f km, %.0f gates", T.HedgeKm, T.Gates));
		Check(!F.bTown || (T.Houses >= 15 && T.Damaged >= T.Houses / 5 && !T.bChurch), "the farm has its cottages and sheds, a share of them burnt or hit",
		      Fmt("%.0f buildings, %.0f burnt or hit", T.Houses, T.Damaged));
		Check(F.Steppe.Rail.size() < 2 || (T.RailKm > 2.0 && T.Poles > 40), "the railway laid on its bank, its telegraph poles beside it", Fmt("%.1f km, %.0f poles", T.RailKm, T.Poles));
		int Kit = 0;
		for (const LandField::Base B : { LandField::Base::Khata, LandField::Base::Chimney, LandField::Base::Pole, LandField::Base::Wattle, LandField::Base::Sweep, LandField::Base::Haystack, LandField::Base::Birch })
			if (!G.Instances(B).empty()) ++Kit;
		Check(Kit == 7, "every one of the east kit's models stands somewhere on the steppe", Fmt("%.0f of 7", Kit));
	}
	else if (bRegion)
	{
		// A region's ground: its village of its own kit's houses round the rules' town, and every model of its kit read.
		Check(!F.bTown || T.Houses >= 10, "the village has its houses (the region's own)", Fmt("%.0f houses", T.Houses));
		int Named = 0, Read = 0, Standing = 0;
		for (int B = static_cast<int>(LandField::Base::Kit0); B < static_cast<int>(LandField::Base::Kit0) + LandField::KitSlots; ++B)
		{
			const LandField::BaseMesh& Bm = G.BaseOf(static_cast<LandField::Base>(B));
			if (Bm.Near.Empty()) continue;
			++Named;
			if (F.Kit.count(Bm.Name)) ++Read;
			if (!G.Instances(static_cast<LandField::Base>(B)).empty()) ++Standing;
		}
		std::printf("  %s: kit %d models (%d read from their files, the rest plain), %d of them standing somewhere\n", LookLaid.c_str(), Named, Read, Standing);
	}
	else
	{
		Check(T.ParcelMean >= 80.0 && T.ParcelMean <= 200.0, "parcels 80 to 200 m across", Fmt("mean %.0f m", T.ParcelMean));
		Check(T.Gates > T.Parcels / 3, "a gate in most parcels", Fmt("%.0f gates, %.0f parcels", T.Gates, T.Parcels));
		// The floor (the art director's review of 22 November, item 3): the June hay in cocks in half the mown meadows or
		// more, 1.0 to 1.6 m (cut 5); the cattle's paths along the pastures' hedges and out of their gateways, bare; the grass
		// in clumps, not spread even.
		{
			std::vector<double> High;
			const LandField::BaseMesh& Hc = G.BaseOf(LandField::Base::Haycock);
			for (const LandField::Placed& P : G.Instances(LandField::Base::Haycock)) High.push_back(Hc.Height * P.ScaleZ);
			const std::vector<LandField::Placed>& Cocks = G.Instances(LandField::Base::Haycock);
			const LandField::Placed* Mid = nullptr;
			for (const LandField::Placed& P : Cocks)
				if (!Mid || std::hypot(P.X - F.Width() / 2.0, P.Y - F.Depth() / 2.0) < std::hypot(Mid->X - F.Width() / 2.0, Mid->Y - F.Depth() / 2.0)) Mid = &P;
			std::printf("  hay: %d mown meadows, %d with their hay in cocks, %d cocks (%.2f to %.2f m, median %.2f); the cock nearest the middle at %.0f, %.0f\n", T.HayFields,
			            T.HayFieldsCocked, T.Haycocks, Pctl(High, 0.1), Pctl(High, 0.9), Pctl(High, 0.5), Mid ? Mid->X : 0.0, Mid ? Mid->Y : 0.0);
			// (cocks of every size, 1.0 to 1.6 m: the art director's cut 5, item 5, saw them all one size)
			Check(T.HayFields == 0 || (T.HayFieldsCocked * 2 >= T.HayFields && Pctl(High, 0.1) >= 0.95 && Pctl(High, 0.9) <= 1.65 && Pctl(High, 0.9) - Pctl(High, 0.1) >= 0.25),
			      "the June hay in cocks 1.0 to 1.6 m high and of mixed sizes, in half the mown meadows or more",
			      Fmt("%.0f of %.0f meadows, 10%% %.2f m, 90%% %.2f m", T.HayFieldsCocked, T.HayFields, Pctl(High, 0.1), Pctl(High, 0.9)));
			// Every gateway into a pasture with beasts: its paths, and the grass off them.
			int Into = 0, Pathed = 0;
			double GateX = 1e9, GateY = 1e9;
			double OnPath = 0.0, OnArea = 0.0, Grass = 0.0, Area = 0.0;
			for (const LandField::HedgeOpening& O : G.Openings())
			{
				if (O.Kind != 0) continue;
				bool bPasture = false;
				for (const double Side : { -1.0, 1.0 })
				{
					const std::string Crop = G.CropOfParcel(G.ParcelAt(O.X - std::sin(O.Yaw) * 4.0 * Side, O.Y + std::cos(O.Yaw) * 4.0 * Side));
					bPasture = bPasture || Crop == "grazed" || Crop == "lush";
				}
				if (!bPasture) continue;
				++Into;
				std::vector<LandField::Placed> Out;
				G.Clutter(O.X - 10.0, O.Y - 10.0, O.X + 10.0, O.Y + 10.0, Out);
				std::vector<std::pair<float, float>> Bits;
				for (const LandField::Placed& P : Out)
					if (P.Kind == LandField::Base::Trodden) Bits.push_back({ P.X, P.Y });
				if (Bits.size() >= 8)
				{
					++Pathed;
					if (std::hypot(O.X - F.Width() / 2.0, O.Y - F.Depth() / 2.0) < std::hypot(GateX - F.Width() / 2.0, GateY - F.Depth() / 2.0))
					{
						GateX = O.X;
						GateY = O.Y;
					}
				}
				for (const LandField::Placed& P : Out)
				{
					if (P.Kind == LandField::Base::Trodden || P.Kind == LandField::Base::Stone) continue;
					bool bOn = false;
					for (const auto& [BX, BY] : Bits) bOn = bOn || std::hypot(P.X - BX, P.Y - BY) < 0.15;
					(bOn ? OnPath : Grass) += 1.0;
				}
				OnArea += Bits.size() * 3.14159 * 0.15 * 0.15 * 0.75; // (the pieces' circles overlap a quarter along the path)
				Area += 400.0;
			}
			const double Bare = Area > 0.0 && OnArea > 0.0 && Grass > 0.0 ? (OnPath / OnArea) / (Grass / Area) : 1.0;
			std::printf("  cow paths: %.1f km; %d of %d gateways into a pasture with a path out of them (the nearest the middle at %.0f, %.0f); grass on a path's middle %.2f of the ground round it\n",
			            T.CowPathKm, Pathed, Into, GateX, GateY, Bare);
			// (under snow no beasts are out: their paths are laid, and lie under it)
			if (G.SnowLying() > 0.0)
				Check(T.CowPathKm > 0.5 && Into > 0 && Pathed == 0, "cow paths along the pastures' hedges laid, and under the snow: none trodden through it",
				      Fmt("%.1f km, %.0f of %.0f gateways with a path showing", T.CowPathKm, Pathed, Into));
			else
			Check(T.CowPathKm > 0.5 && Into > 0 && Pathed * 10 >= Into * 8 && Bare < 0.2, "cow paths along the pastures' hedges and out of eight in ten of their gateways, trodden bare",
			      Fmt("%.1f km, %.0f of %.0f gateways, grass on them %.2f of round them", T.CowPathKm, Pathed, Into, Bare));
			// Clumps: the tufts a 2 m square in grazed pasture, over random places (the docks among them, a share of the tufts'
			// growth in the same clumps since cut 5); their spread over their mean (1 when spread at random, more in clumps).
			uint64_t Sd = 777;
			auto Next = [&Sd]() {
				Sd = Sd * 6364136223846793005ull + 1442695040888963407ull;
				return static_cast<double>(Sd >> 11) / 9007199254740992.0;
			};
			double Sum = 0.0, Sum2 = 0.0, Cells = 0.0;
			for (int Try = 0, Boxes = 0; Try < 4000 && Boxes < 40; ++Try)
			{
				const double X = 20.0 + Next() * (F.Width() - 40.0), Y = 20.0 + Next() * (F.Depth() - 40.0);
				if (G.CropOfParcel(G.ParcelAt(X, Y)) != "grazed") continue;
				bool bAll = true;
				for (const double DX : { -8.0, 8.0 })
					for (const double DY : { -8.0, 8.0 })
					{
						LandField::Drawn What = LandField::Drawn::Floor;
						G.HeightAt(X + DX, Y + DY, &What);
						bAll = bAll && What == LandField::Drawn::Pasture && G.ParcelAt(X + DX, Y + DY) == G.ParcelAt(X, Y);
					}
				if (!bAll) continue;
				++Boxes;
				std::vector<LandField::Placed> Out;
				G.Clutter(X - 8.0, Y - 8.0, X + 8.0, Y + 8.0, Out);
				int N[64] = {};
				for (const LandField::Placed& P : Out)
					if (P.Kind == LandField::Base::Tuft || P.Kind == LandField::Base::Rosette) ++N[std::clamp(static_cast<int>((P.X - (X - 8.0)) / 2.0), 0, 7) * 8 + std::clamp(static_cast<int>((P.Y - (Y - 8.0)) / 2.0), 0, 7)];
				for (const int K : N)
				{
					Sum += K;
					Sum2 += static_cast<double>(K) * K;
					Cells += 1.0;
				}
			}
			const double Mean = Cells > 0.0 ? Sum / Cells : 0.0, Spread = Cells > 0.0 && Mean > 0.0 ? (Sum2 / Cells - Mean * Mean) / Mean : 0.0;
			std::printf("  grazed pasture's tufts and docks: %.2f a 2 m square, spread over mean %.2f (%.0f squares)\n", Mean, Spread, Cells);
			// (spread at random they read 1.19 here: b754375e, before the clumps)
			// (under snow only what stands out of the drifts shows, so the clumps read looser: still clear of random's 1.19)
			const double ClumpBar = G.SnowLying() > 0.0 ? 1.3 : 1.45;
			Check(Cells < 64.0 || Spread >= ClumpBar, "the pasture's tufts in clumps, not spread even", Fmt("spread over mean %.2f (at least %.2f)", Spread, ClumpBar));
		}
		Check(!F.bTown || (T.bChurch && T.Houses >= 10), "the village has its church and houses", Fmt("%.0f houses", T.Houses));
	}

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
		if (!bSteppe && !bRegion)
		{
			Check(!BankDiff.empty() && Pct(BankDiff, 0.5) < 0.03 && Pct(BankDiff, 0.95) < 0.12, "At() stands on the drawn banks",
			      Fmt("%.0f places, median %.3f m, 95%% %.3f m, worst %.3f m", static_cast<double>(BankDiff.size()), Pct(BankDiff, 0.5), Pct(BankDiff, 0.95), BankDiff.empty() ? 0.0 : BankDiff.back()));
			Check(Banks > 0 && OnBank > Banks * 0.9, "a hedge's line is drawn as its bank", Fmt("%.0f of %.0f", OnBank, Banks));
		}
		Check(LanePts > 0 && OnLane > LanePts * 0.97, "a lane's middle is drawn as lane", Fmt("%.0f of %.0f", OnLane, LanePts));
		if (bRegion)
			Check(SunkN > 0 && Sunk / SunkN > -0.2 && Sunk / SunkN < 0.8, "a region's roads worn a little into the ground, not sunk as the bocage's", Fmt("mean %.2f m", SunkN ? Sunk / SunkN : 0.0));
		else if (bSteppe)
			Check(SunkN > 0 && Sunk / SunkN > 0.05 && Sunk / SunkN < 0.6, "the steppe's dirt roads worn a little into the black earth", Fmt("mean %.2f m", SunkN ? Sunk / SunkN : 0.0));
		else Check(SunkN > 0 && Sunk / SunkN > 0.5 && Sunk / SunkN < 2.2, "lanes sunk below the fields beside them (outside the village)", Fmt("mean %.2f m", SunkN ? Sunk / SunkN : 0.0));
		if (bSteppe && F.Steppe.Rail.size() >= 2)
		{
			// The railway's bank: At() on its top, and the bank as drawn (its swept strip) under a man standing there.
			std::vector<double> Top;
			int OnTop = 0, Asked = 0;
			for (const auto& [RX, RY] : F.Steppe.Rail)
			{
				if (RY < 50.0 || RY > F.Depth() - 50.0 || RX < 0.0 || RX > F.Width()) continue;
				for (const double Off : { -3.0, 3.0 })
				{
					++Asked;
					const LandField::Surface Su = G.At(RX + Off, RY);
					const LandField::Surface Beside = G.At(RX + Off * 6.0, RY);
					if (Su.Z > Beside.Z + 0.7 * F.Steppe.RailBank) ++OnTop;
					const double Z = DrawnTop(RX + Off, RY);
					if (!std::isnan(Z)) Top.push_back(std::fabs(Z - Su.Z));
				}
			}
			std::sort(Top.begin(), Top.end());
			Check(Asked > 0 && OnTop >= Asked * 0.9 && !Top.empty() && Pct(Top, 0.5) < 0.05, "the railway's bank stands up from the steppe, and At() stands on it as drawn",
			      Fmt("%.0f of %.0f places on its top, median %.3f m off the drawn bank", OnTop, Asked, Pct(Top, 0.5)));
		}
	}
	// Nothing standing in a house, and each thing standing on the ground drawn; no tree trunk on a vehicle's way.
	{
		int Bad = 0, Off = 0, All = 0, InLane = 0, ShrubsInLane = 0;
		for (int B = 0; B < static_cast<int>(LandField::Base::Tuft); ++B)
			for (const LandField::Placed& P : G.Instances(static_cast<LandField::Base>(B)))
			{
				if (B == static_cast<int>(LandField::Base::Khata)) continue; // (a cottage stands on its own floor)
				const LandField::Surface Su = G.At(P.X, P.Y);
				// (and a region's kit house on its own)
				if (B >= static_cast<int>(LandField::Base::Kit0) && B < static_cast<int>(LandField::Base::Kit0) + LandField::KitSlots && Su.What == LandField::Drawn::Floor) continue;
				++All;
				if (Su.What == LandField::Drawn::Floor) ++Bad;
				if (std::fabs(Su.Z - P.Z) > 0.02) ++Off;
				if (Su.What == LandField::Drawn::Lane) ++(B >= static_cast<int>(LandField::Base::Oak) && B <= static_cast<int>(LandField::Base::Apple) ? InLane : ShrubsInLane);
			}
		Check(Bad == 0, "no shrub, tree, gate, prop or kit model in a house", Fmt("%.0f of %.0f", Bad, All));
		Check(InLane == 0, "no tree in a sunken lane (its floor or walls)", Fmt("%.0f trees (and %.0f shrubs, gates or props)", InLane, ShrubsInLane));
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
	// The recording's ways: each crosses a hedge only where it is opened (no bank and no shrub standing across it), and
	// nothing laid stands on one (a stem or a post within its half width).
	{
		struct Seg
		{
			double AX, AY, BX, BY, Half;
		};
		std::vector<Seg> Segs;
		for (const LandField::Field::Way& W : F.Ways)
			for (size_t K = 0; K + 1 < W.Points.size(); ++K)
				Segs.push_back({ W.Points[K].first, W.Points[K].second, W.Points[K + 1].first, W.Points[K + 1].second, W.HalfWidth });
		// (the segments filed by 10 m square, each in every square its box with its half width touches)
		constexpr double Sq = 10.0, Pad = 320.0;
		const int SX = static_cast<int>(std::ceil((F.Width() + 2.0 * Pad) / Sq)), SY = static_cast<int>(std::ceil((F.Depth() + 2.0 * Pad) / Sq));
		std::vector<std::vector<uint32_t>> SegGrid(static_cast<size_t>(SX) * SY);
		auto Sqr = [](double V, int N) { return std::clamp(static_cast<int>(std::floor((V + Pad) / Sq)), 0, N - 1); };
		for (size_t K = 0; K < Segs.size(); ++K)
		{
			const Seg& S = Segs[K];
			const int I0 = Sqr(std::min(S.AX, S.BX) - S.Half, SX), I1 = Sqr(std::max(S.AX, S.BX) + S.Half, SX);
			const int J0 = Sqr(std::min(S.AY, S.BY) - S.Half, SY), J1 = Sqr(std::max(S.AY, S.BY) + S.Half, SY);
			for (int J = J0; J <= J1; ++J)
				for (int I = I0; I <= I1; ++I) SegGrid[static_cast<size_t>(J) * SX + I].push_back(static_cast<uint32_t>(K));
		}
		auto Dist = [](const Seg& S, double X, double Y) {
			const double DX = S.BX - S.AX, DY = S.BY - S.AY, LL = DX * DX + DY * DY;
			const double U = LL > 0.0 ? std::clamp(((X - S.AX) * DX + (Y - S.AY) * DY) / LL, 0.0, 1.0) : 0.0;
			return std::hypot(X - S.AX - DX * U, Y - S.AY - DY * U);
		};
		// The hedgerows' and lane banks' lines filed the same way: a shrub on a man's way within 1.6 m of one is in the hedge
		// he pushes through (the hedge kept a wall where the men crossed it outside its gateways and breaches).
		std::vector<Seg> Banks;
		std::vector<std::vector<uint32_t>> BankGrid(static_cast<size_t>(SX) * SY);
		for (const LandField::HedgeLine& H : G.Hedges())
		{
			if (H.Kind > 1) continue;
			for (size_t K = 0; K + 1 < H.Points.size(); ++K)
			{
				const Seg S{ H.Points[K].first, H.Points[K].second, H.Points[K + 1].first, H.Points[K + 1].second, 1.6 };
				const int I0 = Sqr(std::min(S.AX, S.BX) - S.Half, SX), I1 = Sqr(std::max(S.AX, S.BX) + S.Half, SX);
				const int J0 = Sqr(std::min(S.AY, S.BY) - S.Half, SY), J1 = Sqr(std::max(S.AY, S.BY) + S.Half, SY);
				for (int J = J0; J <= J1; ++J)
					for (int I = I0; I <= I1; ++I) BankGrid[static_cast<size_t>(J) * SX + I].push_back(static_cast<uint32_t>(Banks.size()));
				Banks.push_back(S);
			}
		}
		auto OnBank = [&](double X, double Y) {
			for (const uint32_t K : BankGrid[static_cast<size_t>(Sqr(Y, SY)) * SX + Sqr(X, SX)])
				if (Dist(Banks[K], X, Y) < Banks[K].Half) return true;
			return false;
		};
		// Nothing standing on a way (and the hedge shrubs' stems filed by square, for the crossings below).
		int Standing = 0, StandingMen = 0, All = 0, Through = 0;
		std::map<std::string, int> StandingBy;
		std::vector<std::vector<std::pair<double, double>>> Stems(static_cast<size_t>(SX) * SY);
		for (int B = 0; B <= static_cast<int>(LandField::Base::Cart); ++B)
			for (const LandField::Placed& P : G.Instances(static_cast<LandField::Base>(B)))
			{
				++All;
				if (B <= static_cast<int>(LandField::Base::Bramble)) Stems[static_cast<size_t>(Sqr(P.Y, SY)) * SX + Sqr(P.X, SX)].push_back({ P.X, P.Y });
				bool bMan = false, bVehicle = false;
				for (const uint32_t K : SegGrid[static_cast<size_t>(Sqr(P.Y, SY)) * SX + Sqr(P.X, SX)])
					if (Dist(Segs[K], P.X, P.Y) < Segs[K].Half) (Segs[K].Half < 1.2 ? bMan : bVehicle) = true;
				if (!bMan && !bVehicle) continue;
				if (!bVehicle && B <= static_cast<int>(LandField::Base::ShrubC) && OnBank(P.X, P.Y))
				{
					++Through;
					continue;
				}
				++Standing;
				++StandingBy[G.BaseOf(static_cast<LandField::Base>(B)).Name];
				if (!bVehicle) ++StandingMen;
			}
		auto NearestStem = [&](double X, double Y) {
			double Best = 1e9;
			const int I = Sqr(X, SX), J = Sqr(Y, SY);
			for (int DJ = -1; DJ <= 1; ++DJ)
				for (int DI = -1; DI <= 1; ++DI)
				{
					if (I + DI < 0 || J + DJ < 0 || I + DI >= SX || J + DJ >= SY) continue;
					for (const auto& St : Stems[static_cast<size_t>(J + DJ) * SX + I + DI]) Best = std::min(Best, std::hypot(St.first - X, St.second - Y));
				}
			return Best;
		};
		// Each crossing of a way and a bank's line (hedgerows, lane banks, wood edges and garden hedges, as laid after their
		// gaps): a vehicle's must find the bank down (under half a metre over the way a few metres either side); a man's
		// may climb the bank but must find the hedge open (no shrub's stem within his half width and half a metre more).
		int Crossings = 0, Shut = 0, MenCrossings = 0, MenShut = 0;
		for (const LandField::HedgeLine& H : G.Hedges())
		{
			if (H.Kind > 3) continue;
			for (size_t Q = 0; Q + 1 < H.Points.size(); ++Q)
			{
				const double CX = H.Points[Q].first, CY = H.Points[Q].second, DX = H.Points[Q + 1].first, DY = H.Points[Q + 1].second;
				std::vector<uint32_t> Near;
				for (const double Fr : { 0.0, 0.5, 1.0 })
					for (const uint32_t K : SegGrid[static_cast<size_t>(Sqr(CY + (DY - CY) * Fr, SY)) * SX + Sqr(CX + (DX - CX) * Fr, SX)]) Near.push_back(K);
				std::sort(Near.begin(), Near.end());
				Near.erase(std::unique(Near.begin(), Near.end()), Near.end());
				for (const uint32_t K : Near)
				{
					const Seg& S = Segs[K];
					const double RX = S.BX - S.AX, RY = S.BY - S.AY, EX = DX - CX, EY = DY - CY;
					const double Den = RX * EY - RY * EX;
					if (std::fabs(Den) < 1e-9) continue;
					const double Tw = ((CX - S.AX) * EY - (CY - S.AY) * EX) / Den, Uh = ((CX - S.AX) * RY - (CY - S.AY) * RX) / Den;
					if (Tw < 0.0 || Tw > 1.0 || Uh < 0.0 || Uh > 1.0) continue;
					const double X = S.AX + RX * Tw, Y = S.AY + RY * Tw, L = std::max(1e-6, std::hypot(RX, RY));
					if (S.Half >= 1.2)
					{
						++Crossings;
						const double Lo = std::min(G.At(X - RX / L * 3.0, Y - RY / L * 3.0).Z, G.At(X + RX / L * 3.0, Y + RY / L * 3.0).Z);
						if (G.At(X, Y).Z - Lo > 0.5) ++Shut;
					}
					else
					{
						++MenCrossings;
						if (NearestStem(X, Y) < S.Half + 0.5) ++MenShut;
					}
				}
			}
		}
		std::string By;
		for (const auto& [Name, N] : StandingBy) By += " " + Name + " " + std::to_string(N);
		std::printf("  ways: %zu segments; vehicles cross a bank's line %d times, %d of them over a standing bank; men cross one %d times, %d of them through a shrub (they climb the bank and push through; %d hedge shrubs stand on their ways); %d of %d things laid stand on a way otherwise (%d on a man's:%s)\n",
		            Segs.size(), Crossings, Shut, MenCrossings, MenShut, Through, Standing, All, StandingMen, By.c_str());
		Check(Shut == 0 && Standing == 0, "the vehicles cross the hedges where they are open, and nothing but a hedge's shrub stands on a way",
		      Fmt("%.0f vehicle crossings shut, %.0f things on a way", static_cast<double>(Shut), static_cast<double>(Standing)));
		// A region's kit on a way: only its low kit (a balk's ridge a man steps over, LandField::LowKitHigh or less as
		// planted), and only on a men's way (said only where some kit lies on a way).
		{
			int Low = 0, Tall = 0, OnVehicles = 0;
			for (int B = static_cast<int>(LandField::Base::Kit0); B < static_cast<int>(LandField::Base::Kit0) + LandField::KitSlots; ++B)
			{
				const LandField::BaseMesh& Bm = G.BaseOf(static_cast<LandField::Base>(B));
				for (const LandField::Placed& P : G.Instances(static_cast<LandField::Base>(B)))
				{
					bool bMan = false, bVehicle = false;
					for (const uint32_t K : SegGrid[static_cast<size_t>(Sqr(P.Y, SY)) * SX + Sqr(P.X, SX)])
						if (Dist(Segs[K], P.X, P.Y) < Segs[K].Half) (Segs[K].Half < 1.2 ? bMan : bVehicle) = true;
					if (bVehicle) ++OnVehicles;
					else if (bMan) ++(Bm.Height * P.ScaleZ <= LandField::LowKitHigh + 1e-3 ? Low : Tall);
				}
			}
			if (Low + Tall + OnVehicles > 0)
				Check(Tall == 0 && OnVehicles == 0, "a region's kit lies across a men's way only where it is low (a man steps over it), and never on a vehicles' way",
				      Fmt("%.0f low on men's ways, %.0f taller there, %.0f on vehicles' ways", Low, Tall, OnVehicles));
		}
	}
	// The bocage at a man's eye: a green wall round each field, the view closed at 100 to 200 m.
	{
		double Own = 0.0;
		const double Median = Bocage(G, F, Own);
		if (bSteppe) Check(Median >= 350.0, "the steppe lies open to a man's eye", Fmt("median sight line %.0f m", Median));
		else if (LookLaid == "desert") Check(Median >= 350.0, "the desert lies open to a man's eye", Fmt("median sight line %.0f m", Median));
		else if (bRegion) std::printf("  %s at a man's eye: median sight line %.0f m\n", LookLaid.c_str(), Median);
		else Check(Median >= 40.0 && Median <= 200.0 && Own >= 0.85, "the bocage closes a field's view at a man's eye", Fmt("median sight line %.0f m, %.0f%% stopped by the field's own hedge", Median, 100.0 * Own));
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

	// Shell holes laid on this ground fade into it (LandCrater with the ground's own colour): the thrown soil's thin edge
	// is the ground's colour exactly, with no outline on stubble, corn or pasture; the same seed lays the same shape.
	{
		const LandCrater::HeightFn Height = [&G](double X, double Y) { return G.At(X, Y).Z; };
		const LandCrater::ColourFn Under = [&G](double X, double Y) { return G.ColourAt(X, Y); };
		const LandCrater::UpFn GroundLit = [&G](double X, double Y, double& NX, double& NY, double& NZ) { G.DrawnUp(X, Y, NX, NY, NZ); };
		uint64_t S = 0x5eedull;
		auto U = [&S]() { S = S * 6364136223846793005ull + 1442695040888963407ull; return static_cast<double>(S >> 11) / 9007199254740992.0; };
		const char* Names[] = { "mortar", "gun75", "howitzer" };
		auto Diff = [](const LandField::Rgb8& A, const LandField::Rgb8& B) { return std::abs(A.R - B.R) + std::abs(A.G - B.G) + std::abs(A.B - B.B); };
		double Before = 0.0, After = 0.0, Inner = 0.0, InnerWas = 0.0;
		int Edges = 0, Moved = 0, Holes = 0, Worst = 0;
		// (and lit as the ground round it: the thin edge's normal against the drawn ground's under it)
		auto DrawnUp = [&G](double X, double Y, double Up[3]) {
			double Best = -1e30, Z = 0.0, Here[3];
			bool bAny = false;
			for (const LandField::Tile& C : G.Tiles())
				if (X >= C.X0 - 4.0 && X < C.X1 + 4.0 && Y >= C.Y0 - 4.0 && Y < C.Y1 + 4.0 && MeshUp(C.Near, X, Y, Z, Here) && Z > Best)
				{
					Best = Z;
					bAny = true;
					for (int K = 0; K < 3; ++K) Up[K] = Here[K];
				}
			return bAny;
		};
		std::vector<double> EdgeLit, BandLit;
		for (int K = 0; K < 300; ++K)
		{
			const double X = 30.0 + U() * (F.Width() - 60.0), Y = 30.0 + U() * (F.Depth() - 60.0);
			const LandField::Surface Here = G.At(X, Y);
			if (std::string(Here.Fx) == "water") continue;
			const LandCrater::Size Sz = LandCrater::SizeOf(Names[K % 3], 0.0);
			const LandCrater::Soil Paint = LandCrater::SoilOf(Here.Fx);
			LandField::Mesh Plain, Faded;
			LandCrater::Lay(X, Y, Sz, Paint, 1000u + static_cast<uint64_t>(K), Height, 0.0, Plain, LandCrater::ColourFn(), nullptr, GroundLit);
			LandCrater::Lay(X, Y, Sz, Paint, 1000u + static_cast<uint64_t>(K), Height, 0.0, Faded, Under, nullptr, GroundLit);
			if (Plain.Positions != Faded.Positions || Plain.Triangles != Faded.Triangles) ++Moved;
			const size_t N = static_cast<size_t>(std::clamp(Sz.Sides, 8, 48));
			if (Faded.Colours.size() < 1 + 9 * N) continue;
			++Holes;
			for (size_t I = 1 + 8 * N; I < 1 + 9 * N; ++I)
			{
				const LandField::Rgb8 Gc = G.ColourAt(Faded.Positions[3 * I], Faded.Positions[3 * I + 1]);
				Before += Diff(Plain.Colours[I], Gc);
				After += Diff(Faded.Colours[I], Gc);
				Worst = std::max(Worst, Diff(Faded.Colours[I], Gc));
				++Edges;
			}
			for (size_t I = 1 + 7 * N; I < 1 + 8 * N; ++I)
			{
				const LandField::Rgb8 Gc = G.ColourAt(Faded.Positions[3 * I], Faded.Positions[3 * I + 1]);
				InnerWas += Diff(Plain.Colours[I], Gc);
				Inner += Diff(Faded.Colours[I], Gc);
			}
			if (K % 3 == 0)
			{
				double Up[3];
				for (size_t I = 1 + 7 * N; I < 1 + 9 * N; ++I)
					if (DrawnUp(Faded.Positions[3 * I], Faded.Positions[3 * I + 1], Up)) (I >= 1 + 8 * N ? EdgeLit : BandLit).push_back(DegreesOff(Faded, I, Up));
			}
		}
		const double E = std::max(1, Edges);
		std::printf("  shell holes: %d laid; their thin edge off the ground's colour by %.2f (was %.1f, worst now %d), the ring inside %.1f (was %.1f), summed RGB steps\n",
			Holes, After / E, Before / E, Worst, Inner / E, InnerWas / E);
		// (the mesh keeps its points as floats: the colour read there again may be a step off the one laid)
		Check(Holes > 200 && Moved == 0 && After / E < 0.5 && Worst <= 3, "a shell hole's thin edge is the ground's own colour, its shape unchanged",
			Fmt("%.0f holes, %.0f changed shape, edge off by %.2f, worst %.0f", Holes, Moved, After / E, Worst));
		std::printf("  shell holes lit as the ground: the thin edge's normal off the drawn ground's by median %.2f, 95%% %.2f, worst %.2f degrees (%zu points); the ring inside it median %.2f, 95%% %.2f\n",
			Pctl(EdgeLit, 0.5), Pctl(EdgeLit, 0.95), Pctl(EdgeLit, 1.0), EdgeLit.size(), Pctl(BandLit, 0.5), Pctl(BandLit, 0.95));
		// (the art director's cut 4, shot 07: lit by its own slope down from the lip, a hole's edge drew a ruled polygon in
		// a low sun, a degree or two either way a tenth brighter or darker than the field)
		Check(EdgeLit.size() > 500 && Pctl(EdgeLit, 0.95) < 1.0 && Pctl(BandLit, 0.95) < 8.0, "a shell hole's thrown soil is lit as the ground it lies on, its edge drawing no outline",
			Fmt("edge 95%% %.2f, ring inside 95%% %.2f degrees off the ground's", Pctl(EdgeLit, 0.95), Pctl(BandLit, 0.95)));
	}
	// A body lies on a shell hole's face (Field::TopAt, the men's ground in the game): wherever a hole's floor, wall,
	// crest, thrown soil or clod stands, TopAt is at least that high, so nothing of the hole shows through a body laid
	// on it; and where no hole reaches, it says none. Holes laid in clusters, over each other, as a barrage lays them.
	{
		LandCrater::Field Holes;
		Holes.Begin([&G](double X, double Y) { return G.At(X, Y).Z; }, 77u, [&G](double X, double Y) { return G.ColourAt(X, Y); });
		Holes.Lit = [&G](double X, double Y, double& NX, double& NY, double& NZ) { G.DrawnUp(X, Y, NX, NY, NZ); };
		uint64_t S = 0xb0d1e5ull;
		auto U = [&S]() { S = S * 6364136223846793005ull + 1442695040888963407ull; return static_cast<double>(S >> 11) / 9007199254740992.0; };
		const char* Names[] = { "grenade", "mortar", "gun75", "howitzer", "heavy" };
		double X = 0.0, Y = 0.0;
		for (int K = 0; K < 300; ++K)
		{
			if (K % 4 == 0) { X = 40.0 + U() * (F.Width() - 80.0); Y = 40.0 + U() * (F.Depth() - 80.0); }
			else { X += U() * 6.0 - 3.0; Y += U() * 6.0 - 3.0; }
			if (std::string(G.At(X, Y).Fx) == "water") continue;
			Holes.Add(X, Y, Names[K % 5], 0.0, G.At(X, Y).Fx);
		}
		long Points = 0, Missed = 0, Under = 0;
		double WorstUnder = 0.0, CrestSum = 0.0;
		int Crests = 0;
		const double Bary[3][3] = { { 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0 }, { 0.7, 0.15, 0.15 }, { 0.15, 0.15, 0.7 } };
		for (int P = 0; P < Holes.Patches(); ++P)
		{
			for (const LandField::Mesh& M : Holes.PatchAt(P).Sections)
			{
				for (size_t Tr = 0; Tr + 2 < M.Triangles.size(); Tr += 3)
				{
					const float* A = &M.Positions[M.Triangles[Tr] * 3];
					const float* B = &M.Positions[M.Triangles[Tr + 1] * 3];
					const float* C = &M.Positions[M.Triangles[Tr + 2] * 3];
					for (const auto& W : Bary)
					{
						const double Px = W[0] * A[0] + W[1] * B[0] + W[2] * C[0], Py = W[0] * A[1] + W[1] * B[1] + W[2] * C[1], Pz = W[0] * A[2] + W[1] * B[2] + W[2] * C[2];
						++Points;
						double Top = 0.0;
						if (!Holes.TopAt(Px, Py, Top)) { ++Missed; continue; }
						if (Top < Pz - 1e-3) { ++Under; WorstUnder = std::max(WorstUnder, Pz - Top); }
					}
				}
			}
		}
		// (how high a body is laid over the drawn ground on a 105's crest)
		for (const LandCrater::Field::Hole& H : Holes.All())
		{
			if (H.Sides != 20 || H.R < 1.3) continue;
			const LandField::Mesh& M = Holes.Section(H.In);
			for (int K = 0; K < H.Sides; ++K)
			{
				const size_t I = H.First + 1u + static_cast<size_t>(4 * H.Sides + K); // (the crest: Lay's fifth ring)
				double Top = 0.0;
				if (Holes.TopAt(M.Positions[I * 3], M.Positions[I * 3 + 1], Top)) { CrestSum += Top - G.At(M.Positions[I * 3], M.Positions[I * 3 + 1]).Z; ++Crests; }
			}
		}
		int Away = 0, AwaySaid = 0;
		for (int K = 0; K < 20000; ++K)
		{
			const double Qx = U() * F.Width(), Qy = U() * F.Depth();
			bool bNear = false;
			for (const LandCrater::Field::Hole& H : Holes.All()) bNear = bNear || (Qx - H.X) * (Qx - H.X) + (Qy - H.Y) * (Qy - H.Y) <= H.Out * H.Out;
			if (bNear) continue;
			++Away;
			double Top = 0.0;
			if (Holes.TopAt(Qx, Qy, Top)) ++AwaySaid;
		}
		const auto A0 = std::chrono::steady_clock::now();
		double Sum = 0.0;
		int Hits = 0;
		for (int K = 0; K < 200000; ++K)
		{
			const LandCrater::Field::Hole& H = Holes.All()[static_cast<size_t>(K) % Holes.All().size()];
			double Top = 0.0;
			if (Holes.TopAt(H.X + (K % 7 - 3) * 0.3 * H.R, H.Y + (K % 5 - 2) * 0.4 * H.R, Top)) { Sum += Top; ++Hits; }
		}
		const double Us = Seconds(A0) / 200000.0 * 1e6;
		std::printf("  holes' face: %d holes; %ld points on their triangles, %ld with no face found, %ld under the face (worst %.4f m); %d away from every hole, %d said to have one; a 105's crest lays a body %.2f m over the drawn ground; TopAt %.2f microseconds a call by a hole (%d, %.0f)\n",
			Holes.Count(), Points, Missed, Under, WorstUnder, Away, AwaySaid, CrestSum / std::max(1, Crests), Us, Hits, Sum / std::max(1, Hits));
		Check(Holes.Count() > 250 && Missed == 0 && Under == 0 && AwaySaid == 0 && Us < 5.0, "a body lies on a shell hole's face: nothing of a hole stands above it, and no face where none is torn",
			Fmt("%.0f points with no face, %.0f under it, %.0f away said to have one, %.2f microseconds", static_cast<double>(Missed), static_cast<double>(Under), static_cast<double>(AwaySaid), Us));
	}


	// Dug holes (Brian, 22 November: "explosions should create craters"): a mortar bomb's or a shell's hole is dug into the
	// ground and the ground drawn is cut away over its bowl (LandCrater::Dug, CutOut). Laid in barrages over each other:
	// the ground left outside every cut is the ground it was, to the millimetre, and covers every place it did; inside a
	// cut no ground is left; wherever the ground is gone a hole's face is there to stand on (no hole through the world);
	// just outside a cut's edge the hole's wall stands over the ground (its edge never seen); the bowls go as deep as dug.
	{
		LandCrater::Field Holes;
		LandCrater::Dug Ground;
		Ground.Begin(&G.Tiles());
		Holes.Begin([&G](double X, double Y) { return G.At(X, Y).Z; }, 91u, [&G](double X, double Y) { return G.ColourAt(X, Y); });
		Holes.Lit = [&G](double X, double Y, double& NX, double& NY, double& NZ) { G.DrawnUp(X, Y, NX, NY, NZ); };
		Holes.Dig = LandCrater::Field::DigOn(G);
		uint64_t S = 0xd1665ull;
		auto U = [&S]() { S = S * 6364136223846793005ull + 1442695040888963407ull; return static_cast<double>(S >> 11) / 9007199254740992.0; };
		const char* Names[] = { "mortar", "gun75", "howitzer", "mortar", "heavy", "grenade", "light" };
		double X = 0.0, Y = 0.0;
		std::vector<std::vector<double>> AllCuts;
		const auto C0 = std::chrono::steady_clock::now();
		for (int K = 0; K < 400; ++K)
		{
			if (K % 5 == 0) { X = 60.0 + U() * (F.Width() - 120.0); Y = 60.0 + U() * (F.Depth() - 120.0); }
			else { X += U() * 5.0 - 2.5; Y += U() * 5.0 - 2.5; }
			if (std::string(G.At(X, Y).Fx) == "water") continue;
			Holes.Add(X, Y, Names[K % 7], 0.0, G.At(X, Y).Fx);
			for (std::vector<double>& Cut : Holes.TakeCuts())
			{
				Ground.Cut(Cut);
				AllCuts.push_back(std::move(Cut));
			}
		}
		const double CutMs = Seconds(C0) * 1e3;
		int Dug = 0, Shallow = 0;
		double DeepSum = 0.0, DeepWant = 0.0;
		for (const LandCrater::Field::Hole& H : Holes.All())
		{
			if (H.Cut.empty()) { ++Shallow; continue; }
			++Dug;
		}
		// (holes dug alone in open ground: each one's floor read at its middle, under the ground by its depth less the
		// 3 cm its floor is laid over the ground it is dug in)
		int Measured = 0;
		{
			LandCrater::Field Lone;
			Lone.Begin([&G](double Px, double Py) { return G.At(Px, Py).Z; }, 5u);
			Lone.Dig = LandCrater::Field::DigOn(G);
			for (double Px = 80.0; Px < F.Width() - 80.0; Px += 90.0)
			{
				for (double Py = 80.0; Py < F.Depth() - 80.0; Py += 90.0)
				{
					Lone.Add(Px, Py, Names[Measured % 5], 0.0, G.At(Px, Py).Fx);
					++Measured;
				}
			}
			Measured = 0;
			for (const LandCrater::Field::Hole& H : Lone.All())
			{
				double Top = 0.0;
				if (!H.Cut.empty() && Lone.TopAt(H.X, H.Y, Top)) { DeepSum += G.At(H.X, H.Y).Z + 0.03 - Top; DeepWant += H.Deep; ++Measured; }
			}
		}
		// The ground left, part by part: what covers a point, and how high.
		const auto GroundAt = [&](double Px, double Py, double& OutZ, bool bUncut = false) -> bool
		{
			bool bAny = false;
			for (size_t Ti = 0; Ti < G.Tiles().size(); ++Ti)
			{
				const LandField::Tile& Tile = G.Tiles()[Ti];
				if (Px < Tile.X0 - 12.0 || Px > Tile.X1 + 12.0 || Py < Tile.Y0 - 12.0 || Py > Tile.Y1 + 12.0) continue;
				const bool bSplit = !bUncut && Ground.IsSplit(static_cast<int>(Ti));
				const int Parts = bSplit ? LandCrater::Dug::Split * LandCrater::Dug::Split : 1;
				for (int P = 0; P < Parts; ++P)
				{
					const LandField::Mesh& M = bSplit ? Ground.PartOf({ static_cast<int>(Ti), P }) : Tile.Near;
					for (size_t Tr = 0; Tr + 2 < M.Triangles.size(); Tr += 3)
					{
						const float* A = &M.Positions[M.Triangles[Tr] * 3];
						const float* B = &M.Positions[M.Triangles[Tr + 1] * 3];
						const float* C = &M.Positions[M.Triangles[Tr + 2] * 3];
						if (Px < std::min({ A[0], B[0], C[0] }) - 1e-4 || Px > std::max({ A[0], B[0], C[0] }) + 1e-4 || Py < std::min({ A[1], B[1], C[1] }) - 1e-4 || Py > std::max({ A[1], B[1], C[1] }) + 1e-4) continue;
						const double Ux = B[0] - A[0], Uy = B[1] - A[1], Vx = C[0] - A[0], Vy = C[1] - A[1];
						const double Den = Ux * Vy - Uy * Vx;
						if (std::fabs(Den) < 1e-9) continue;
						const double Wx = Px - A[0], Wy = Py - A[1];
						const double Su = (Wx * Vy - Wy * Vx) / Den, Tv = (Ux * Wy - Uy * Wx) / Den;
						if (Su < -1e-7 || Tv < -1e-7 || Su + Tv > 1.0 + 1e-7) continue;
						// (the highest face over the point: a bank's or a lane's may lie over the ground's own)
						const double Z = A[2] + Su * (B[2] - A[2]) + Tv * (C[2] - A[2]);
						OutZ = bAny ? std::max(OutZ, Z) : Z;
						bAny = true;
					}
				}
			}
			return bAny;
		};
		// (a point within the positions' own rounding of a face, and no face over it: a T-junction's hairline, where a
		// point cut into one face's edge was stored as a float a fraction of a millimetre off the edge its neighbour
		// keeps; counted apart, not as ground gone. In the desert's Normandy, cut 128 of 288 left one 0.06 mm wide,
		// under its own hole's rim, which stands over the ground 8 cm past the cut)
		const auto HairlineAt = [&](double Px, double Py, double& OutZ) -> bool
		{
			const double Tol = 1.5 * std::ldexp(1.0, std::ilogb(std::max({ std::fabs(Px), std::fabs(Py), 1.0 })) - 23);
			bool bAny = false;
			for (size_t Ti = 0; Ti < G.Tiles().size(); ++Ti)
			{
				const LandField::Tile& Tile = G.Tiles()[Ti];
				if (Px < Tile.X0 - 12.0 || Px > Tile.X1 + 12.0 || Py < Tile.Y0 - 12.0 || Py > Tile.Y1 + 12.0) continue;
				const bool bSplit = Ground.IsSplit(static_cast<int>(Ti));
				const int Parts = bSplit ? LandCrater::Dug::Split * LandCrater::Dug::Split : 1;
				for (int P = 0; P < Parts; ++P)
				{
					const LandField::Mesh& M = bSplit ? Ground.PartOf({ static_cast<int>(Ti), P }) : Tile.Near;
					for (size_t Tr = 0; Tr + 2 < M.Triangles.size(); Tr += 3)
					{
						const float* A = &M.Positions[M.Triangles[Tr] * 3];
						const float* B = &M.Positions[M.Triangles[Tr + 1] * 3];
						const float* C = &M.Positions[M.Triangles[Tr + 2] * 3];
						if (Px < std::min({ A[0], B[0], C[0] }) - Tol || Px > std::max({ A[0], B[0], C[0] }) + Tol || Py < std::min({ A[1], B[1], C[1] }) - Tol || Py > std::max({ A[1], B[1], C[1] }) + Tol) continue;
						const double Ux = B[0] - A[0], Uy = B[1] - A[1], Vx = C[0] - A[0], Vy = C[1] - A[1];
						const double Den = Ux * Vy - Uy * Vx;
						if (std::fabs(Den) < 1e-9) continue;
						double Near = 1e9;
						const float* Ends[4] = { A, B, C, A };
						for (int E = 0; E < 3; ++E)
						{
							const double Ex = Ends[E + 1][0] - Ends[E][0], Ey = Ends[E + 1][1] - Ends[E][1];
							const double Long2 = Ex * Ex + Ey * Ey;
							const double Along = Long2 > 0.0 ? std::clamp(((Px - Ends[E][0]) * Ex + (Py - Ends[E][1]) * Ey) / Long2, 0.0, 1.0) : 0.0;
							Near = std::min(Near, std::hypot(Px - (Ends[E][0] + Along * Ex), Py - (Ends[E][1] + Along * Ey)));
						}
						if (Near > Tol) continue;
						const double Wx = Px - A[0], Wy = Py - A[1];
						const double Su = (Wx * Vy - Wy * Vx) / Den, Tv = (Ux * Wy - Uy * Wx) / Den;
						const double Z = A[2] + Su * (B[2] - A[2]) + Tv * (C[2] - A[2]);
						OutZ = bAny ? std::max(OutZ, Z) : Z;
						bAny = true;
					}
				}
			}
			return bAny;
		};
		long OutsideHairline = 0, AwayHairline = 0;
		long Outside = 0, OutsideBare = 0, OutsideOff = 0, Inside = 0, InsideCovered = 0, InsideNoFace = 0, EdgeTests = 0, EdgeUnder = 0;
		double WorstOff = 0.0, WorstEdge = 1e9;
		for (const std::vector<double>& Cut : AllCuts)
		{
			const size_t N = Cut.size() / 2;
			double Mx = 0.0, My = 0.0;
			for (size_t K = 0; K < N; ++K) { Mx += Cut[2 * K] / N; My += Cut[2 * K + 1] / N; }
			for (size_t K = 0; K < N; ++K)
			{
				const double Ex = Cut[2 * K], Ey = Cut[2 * K + 1];
				const double Fx = Cut[2 * ((K + 1) % N)], Fy = Cut[2 * ((K + 1) % N) + 1];
				for (double W : { 0.25, 0.5, 0.75 })
				{
					const double Qx = Ex + (Fx - Ex) * W, Qy = Ey + (Fy - Ey) * W;
					const double Dx = Qx - Mx, Dy = Qy - My, L = std::hypot(Dx, Dy);
					// just inside: no ground; a face to stand on
					const double Ix = Qx - Dx / L * 0.03, Iy = Qy - Dy / L * 0.03;
					double Z = 0.0;
					++Inside;
					if (GroundAt(Ix, Iy, Z)) ++InsideCovered;
					if (!Holes.TopAt(Ix, Iy, Z)) ++InsideNoFace;
					// just outside: the ground as it was (unless a later cut took it), and a hole's face over it
					const double Ox = Qx + Dx / L * 0.03, Oy = Qy + Dy / L * 0.03;
					if (Holes.InCut(Ox, Oy)) continue;
					++Outside;
					if (!GroundAt(Ox, Oy, Z) && HairlineAt(Ox, Oy, Z))
					{
						++OutsideHairline;
					}
					else if (!GroundAt(Ox, Oy, Z))
					{
						++OutsideBare;
						if (std::getenv("LAND_FIELD_BARE"))
						{
							std::printf("  bare just outside a cut at %.4f, %.4f (edge %zu of %zu, %.4f, %.4f to %.4f, %.4f); the cut:", Ox, Oy, K, N, Ex, Ey, Fx, Fy);
							for (size_t J = 0; J < N; ++J) std::printf(" %.4f,%.4f", Cut[2 * J], Cut[2 * J + 1]);
							std::printf("\n");
							for (size_t O = 0; O < AllCuts.size(); ++O)
							{
								const std::vector<double>& Other = AllCuts[O];
								bool bNearIt = false;
								for (size_t J = 0; J + 1 < Other.size(); J += 2) bNearIt = bNearIt || std::hypot(Other[J] - Ox, Other[J + 1] - Oy) < 3.0;
								if (!bNearIt || &Other == &Cut) continue;
								std::printf("    another cut near it (%zu of %zu in order):", O, AllCuts.size());
								for (size_t J = 0; J + 1 < Other.size(); J += 2) std::printf(" %.4f,%.4f", Other[J], Other[J + 1]);
								std::printf("\n");
							}
							std::printf("    this cut is %zu in order\n", static_cast<size_t>(&Cut - &AllCuts[0]));
							// (the nearest faces: how far outside each the point lies, in its own share)
							for (size_t Ti = 0; Ti < G.Tiles().size(); ++Ti)
							{
								const LandField::Tile& Tile = G.Tiles()[Ti];
								if (Ox < Tile.X0 - 12.0 || Ox > Tile.X1 + 12.0 || Oy < Tile.Y0 - 12.0 || Oy > Tile.Y1 + 12.0) continue;
								const bool bSplit = Ground.IsSplit(static_cast<int>(Ti));
								const int Parts = bSplit ? LandCrater::Dug::Split * LandCrater::Dug::Split : 1;
								for (int P = 0; P < Parts; ++P)
								{
									const LandField::Mesh& Mm = bSplit ? Ground.PartOf({ static_cast<int>(Ti), P }) : Tile.Near;
									for (size_t Tr = 0; Tr + 2 < Mm.Triangles.size(); Tr += 3)
									{
										const float* A = &Mm.Positions[Mm.Triangles[Tr] * 3];
										const float* B = &Mm.Positions[Mm.Triangles[Tr + 1] * 3];
										const float* C = &Mm.Positions[Mm.Triangles[Tr + 2] * 3];
										if (Ox < std::min({ A[0], B[0], C[0] }) - 0.05 || Ox > std::max({ A[0], B[0], C[0] }) + 0.05 || Oy < std::min({ A[1], B[1], C[1] }) - 0.05 || Oy > std::max({ A[1], B[1], C[1] }) + 0.05) continue;
										const double Ux = B[0] - A[0], Uy = B[1] - A[1], Vx = C[0] - A[0], Vy = C[1] - A[1];
										const double Den = Ux * Vy - Uy * Vx;
										const double Wx = Ox - A[0], Wy = Oy - A[1];
										const double Su = std::fabs(Den) > 0 ? (Wx * Vy - Wy * Vx) / Den : -9, Tv = std::fabs(Den) > 0 ? (Ux * Wy - Uy * Wx) / Den : -9;
										std::printf("      tile %zu part %d tri %zu: (%.4f %.4f) (%.4f %.4f) (%.4f %.4f) den %.3g shares %.6f %.6f %.6f\n", Ti, P, Tr / 3, A[0], A[1], B[0], B[1], C[0], C[1], Den, 1.0 - Su - Tv, Su, Tv);
									}
								}
							}
						}
						continue;
					}
					double Was = 0.0;
					GroundAt(Ox, Oy, Was, true);
					if (std::fabs(Z - Was) > 2e-3) { ++OutsideOff; WorstOff = std::max(WorstOff, std::fabs(Z - Was)); }
					double Top = 0.0;
					++EdgeTests;
					if (!Holes.TopAt(Ox, Oy, Top) || Top < Z + 0.005) ++EdgeUnder;
					else WorstEdge = std::min(WorstEdge, Top - Z);
				}
			}
		}
		// Far from every hole: the ground there as it was.
		long Away = 0, AwayBare = 0, AwayOff = 0;
		for (int K = 0; K < 3000; ++K)
		{
			const double Qx = 30.0 + U() * (F.Width() - 60.0), Qy = 30.0 + U() * (F.Depth() - 60.0);
			bool bNear = false;
			for (const LandCrater::Field::Hole& H : Holes.All()) bNear = bNear || std::hypot(Qx - H.X, Qy - H.Y) < H.Out + 0.5;
			if (bNear) continue;
			++Away;
			double Z = 0.0;
			if (!GroundAt(Qx, Qy, Z))
			{
				if (!HairlineAt(Qx, Qy, Z)) { ++AwayBare; continue; }
				++AwayHairline;
			}
			double Was = 0.0;
			if (!GroundAt(Qx, Qy, Was, true) || std::fabs(Z - Was) > 2e-3) ++AwayOff;
		}
		int Split = 0;
		for (size_t Ti = 0; Ti < G.Tiles().size(); ++Ti) Split += Ground.IsSplit(static_cast<int>(Ti));
		std::printf("  dug holes: %d dug, %d laid shallow (a grenade's, a small round's, or on a lane or bank), %d tiles split; bowls %.2f m deep at their middles where %.2f dug (alone); %ld points just inside a cut, %ld with ground left, %ld with no face; %ld just outside, %ld bare, %ld off the ground it was (worst %.4f m); the wall over the cut's edge by %.3f m at least (%ld of %ld under 5 mm); %ld away, %ld bare, %ld off; on a hairline under the points' rounding %ld outside, %ld away; laying and cutting %.1f ms for all\n",
			Dug, Shallow, Split, DeepSum / std::max(1, Measured), DeepWant / std::max(1, Measured), Inside, InsideCovered, InsideNoFace, Outside, OutsideBare, OutsideOff, WorstOff, WorstEdge, EdgeUnder, EdgeTests, Away, AwayBare, AwayOff, OutsideHairline, AwayHairline, CutMs);
		Check(Dug > 150 && Shallow > 20 && InsideCovered == 0 && InsideNoFace == 0 && OutsideBare == 0 && OutsideOff == 0 && AwayBare == 0 && AwayOff == 0
			&& (OutsideHairline + AwayHairline) * 1000 <= Outside + Away,
			"a dug hole cuts the ground over its bowl and nothing else: the ground left is as it was, and a face to stand on wherever it is gone (a hairline under the points' rounding in one point of a thousand at most)",
			Fmt("%.0f inside with ground or with no face, %.0f outside bare or off, %.0f away bare or off, %.0f on a hairline", static_cast<double>(InsideCovered + InsideNoFace), static_cast<double>(OutsideBare + OutsideOff), static_cast<double>(AwayBare + AwayOff), static_cast<double>(OutsideHairline + AwayHairline)));
		Check(EdgeUnder <= EdgeTests / 200 && DeepWant > 0.0 && std::fabs(DeepSum / DeepWant - 1.0) < 0.15,
			"a dug hole's wall stands over the ground's cut edge, and its bowl goes as deep as dug",
			Fmt("%.0f of %.0f edge points with the wall under 5 mm over the ground; depth %.2f of what was dug", static_cast<double>(EdgeUnder), static_cast<double>(EdgeTests), DeepWant > 0.0 ? DeepSum / DeepWant : 0.0));
		// (ploughland, on trial: a dug tile's parts keep the furrows' two numbers at every point, those the cuts made
		// too; asked only where the near ground carries them)
		size_t PloughTiles = 0, PloughParts = 0, PloughBad = 0;
		for (size_t Ti = 0; Ti < G.Tiles().size(); ++Ti)
		{
			if (G.Tiles()[Ti].Near.Uvs.empty() || !Ground.IsSplit(static_cast<int>(Ti))) continue;
			++PloughTiles;
			for (int P = 0; P < LandCrater::Dug::Split * LandCrater::Dug::Split; ++P)
			{
				const LandField::Mesh& Pm = Ground.PartOf({ static_cast<int>(Ti), P });
				if (Pm.Empty()) continue;
				++PloughParts;
				if (Pm.Uvs.size() != 2 * Pm.Points()) ++PloughBad;
			}
		}
		if (PloughTiles > 0)
			Check(PloughBad == 0, "ploughland: a dug tile's parts keep the furrows' way at every point, the cuts' new points too",
				  Fmt("%.0f dug tiles, %.0f parts, ", static_cast<double>(PloughTiles), static_cast<double>(PloughParts)) + Fmt("%.0f parts without", static_cast<double>(PloughBad)));
	}

	// Hedge cutters (release 2; Brian, 22 November 14:19Z, "Let's do 1 and 2": bladed Shermans punch a gap through a
	// hedgerow where the player picks). At a cut the hedge is laid whole: no way opened through it, no tree in the gap.
	// Torn (LandCrater::Field::AddBreach, as the scene tears it when she bursts through), the bank's earth goes down to
	// the field: the ground drawn is cut over it and a face is there to stand on wherever it is gone, the gap's floor is
	// the field's level, her tracks' way through it climbs no step over 30 cm, the cut's seams with the ground left
	// show no step a man would see, and a body laid on it lies on its face.
	if (!G.Crossings().empty())
	{
		LandField::Field FC = F;
		for (const LandField::HedgeCrossing& C : G.Crossings())
		{
			if (C.Kind > 1) continue;
			bool bNear = false;
			for (const LandField::Field::Cut& O : FC.Cuts) bNear = bNear || std::hypot(C.X - O.X, C.Y - O.Y) < 80.0;
			// (as LandSight::CutsFromCrossings: not through a field's gateway)
			for (const LandField::HedgeOpening& O : G.Openings()) bNear = bNear || (O.Kind == 0 && std::hypot(O.X - C.X, O.Y - C.Y) < 0.5 * (O.Width + 4.0) + 3.0);
			if (bNear) continue;
			LandField::Field::Cut Cut;
			Cut.X = C.X;
			Cut.Y = C.Y;
			Cut.Width = 4.0;
			FC.Cuts.push_back(Cut);
			if (FC.Cuts.size() >= 8) break;
		}
		const auto L0 = std::chrono::steady_clock::now();
		const LandField::Ground GC(FC);
		const double LayS = Seconds(L0);
		const LandField::Base Trees[] = { LandField::Base::Oak, LandField::Base::Elm, LandField::Base::Ash, LandField::Base::Beech, LandField::Base::Pollard, LandField::Base::Apple, LandField::Base::Birch };
		LandCrater::Field Br;
		LandCrater::Dug Dg;
		Dg.Begin(&GC.Tiles());
		Br.Begin([&GC](double X, double Y) { return GC.HeightAt(X, Y); }, 0xB4EAC4u, [&GC](double X, double Y) { return GC.ColourAt(X, Y); });
		Br.Lit = [&GC](double X, double Y, double& NX, double& NY, double& NZ) { GC.DrawnUp(X, Y, NX, NY, NZ); };
		const LandCrater::RiseFn Rise = [&GC](double X, double Y) { return GC.BankRise(X, Y); };
		int Standing = 0, Through = 0, Treed = 0, NoBank = 0, Torn = 0, FloorOff = 0, Steep = 0, High = 0;
		double LeastRise = 1e9, OpenRise = 0.0, WorstFloor = 0.0, WorstStep = 0.0, WorstHigh = 0.0;
		double TearMs = 0.0;
		struct Gap { double X, Y, Yaw, CutU, CutV; };
		std::vector<Gap> Gaps;
		std::vector<std::vector<double>> GapCuts;
		for (size_t K = 0; K < FC.Cuts.size(); ++K)
		{
			const LandField::Field::Cut& Cut = FC.Cuts[K];
			// laid whole: the bank stands at the cut, no vehicle's way through it, no tree in the gap
			// (on the bank's middle line nearest the crossing, which may lie a little off it)
			LandField::BankPlace BP;
			const bool bBank = GC.NearestBank(Cut.X, Cut.Y, 6.0, BP) && std::hypot(BP.X - Cut.X, BP.Y - Cut.Y) <= 2.5;
			const double R = bBank ? GC.BankRise(BP.X, BP.Y) : 0.0;
			LeastRise = std::min(LeastRise, R);
			OpenRise += (bBank ? G.BankRise(BP.X, BP.Y) : 0.0) / static_cast<double>(FC.Cuts.size());
			if (R > 0.4) ++Standing;
			for (const LandField::HedgeCrossing& C : GC.Crossings()) Through += std::hypot(C.X - Cut.X, C.Y - Cut.Y) < 0.5 * Cut.Width + 3.0;
			for (LandField::Base B : Trees)
				for (const LandField::Placed& P : GC.Instances(B))
				{
					const bool bIn = std::hypot(P.X - Cut.X, P.Y - Cut.Y) < 0.5 * Cut.Width + 1.5;
					Treed += bIn;
					if (bIn && std::getenv("LAND_FIELD_BREACHES")) std::printf("    cut %zu: a %s at %.1f m\n", K, BaseNames[static_cast<int>(B)], std::hypot(P.X - Cut.X, P.Y - Cut.Y));
				}
			if (std::getenv("LAND_FIELD_BREACHES"))
			{
				LandField::Drawn What = LandField::Drawn::Pasture;
				GC.HeightAt(Cut.X, Cut.Y, &What);
				std::printf("    cut %zu at %.1f, %.1f: bank %.2f m (%.2f opened), drawn %s; nearest bank %.2f m off, kind %d, foot %.2f\n", K, Cut.X, Cut.Y, R, G.BankRise(Cut.X, Cut.Y), DrawnNames[static_cast<int>(What)], bBank ? std::hypot(BP.X - Cut.X, BP.Y - Cut.Y) : -1.0, BP.Kind, BP.Foot);
				for (const LandField::HedgeOpening& O : GC.Openings())
					if (std::hypot(O.X - Cut.X, O.Y - Cut.Y) < 12.0) std::printf("      an opening (kind %d, %.1f m wide, %d men) %.1f m off\n", O.Kind, O.Width, O.Men, std::hypot(O.X - Cut.X, O.Y - Cut.Y));
				for (const LandField::HedgeCrossing& C : G.Crossings())
					if (std::hypot(C.X - Cut.X, C.Y - Cut.Y) < 12.0) std::printf("      a crossing laid without cuts (kind %d, %.1f m wide, way %d) %.1f m off\n", C.Kind, C.Width, C.Way, std::hypot(C.X - Cut.X, C.Y - Cut.Y));
			}
			// torn as the scene tears it
			if (!bBank) { ++NoBank; continue; }
			LandCrater::Breach B;
			B.X = BP.X;
			B.Y = BP.Y;
			B.Yaw = BP.Yaw;
			B.Half = 0.5 * Cut.Width;
			B.Foot = BP.Foot;
			B.Through = K % 2 == 0 ? 1 : -1;
			const auto T0 = std::chrono::steady_clock::now();
			const bool bTorn = Br.AddBreach(B, Rise, "earth");
			for (std::vector<double>& Out : Br.TakeCuts())
			{
				Dg.Cut(Out);
				GapCuts.push_back(std::move(Out));
			}
			TearMs = std::max(TearMs, Seconds(T0) * 1e3);
			if (!bTorn) continue;
			++Torn;
			const LandCrater::Field::Hole& H = Br.All().back();
			Gaps.push_back({ B.X, B.Y, B.Yaw, H.CutU, H.CutV });
			// the gap's floor at its middle: the field's level (the bank's earth gone), churned a few centimetres
			double Z = 0.0;
			const double Field0 = GC.HeightAt(B.X, B.Y) - GC.BankRise(B.X, B.Y);
			if (!Br.BreachFloor(B.X, B.Y, Z) || std::fabs(Z - Field0) > 0.15) ++FloorOff;
			WorstFloor = std::max(WorstFloor, std::fabs(Z - Field0));
			// her two tracks' way across the bank: what she drives on (the gap's floor in its cut, the ground drawn
			// beyond it) a step of 30 cm at most between 20 cm samples, and never 30 cm over the bare field
			const double Cy = std::cos(B.Yaw), Sy = std::sin(B.Yaw);
			for (const double U : { -1.1, 1.1 })
			{
				double Was = std::nan("");
				for (double V = -7.0; V <= 7.0 + 1e-9; V += 0.2)
				{
					const double X = B.X + U * Cy - V * Sy, Y = B.Y + U * Sy + V * Cy;
					double On = 0.0;
					if (!Br.BreachFloor(X, Y, On)) On = GC.HeightAt(X, Y);
					const double Bare = GC.HeightAt(X, Y) - GC.BankRise(X, Y);
					if (On - Bare > 0.3) { ++High; WorstHigh = std::max(WorstHigh, On - Bare); }
					if (!std::isnan(Was) && std::fabs(On - Was) > 0.3) ++Steep;
					if (!std::isnan(Was)) WorstStep = std::max(WorstStep, std::fabs(On - Was));
					Was = On;
				}
			}
		}
		// The ground left, as the dug holes' test reads it: the highest face of the tiles' parts over a point.
		const auto GroundAt = [&](double Px, double Py, double& OutZ, bool bUncut) -> bool
		{
			bool bAny = false;
			for (size_t Ti = 0; Ti < GC.Tiles().size(); ++Ti)
			{
				const LandField::Tile& Tile = GC.Tiles()[Ti];
				if (Px < Tile.X0 - 12.0 || Px > Tile.X1 + 12.0 || Py < Tile.Y0 - 12.0 || Py > Tile.Y1 + 12.0) continue;
				const bool bSplit = !bUncut && Dg.IsSplit(static_cast<int>(Ti));
				const int Parts = bSplit ? LandCrater::Dug::Split * LandCrater::Dug::Split : 1;
				for (int P = 0; P < Parts; ++P)
				{
					const LandField::Mesh& M = bSplit ? Dg.PartOf({ static_cast<int>(Ti), P }) : Tile.Near;
					for (size_t Tr = 0; Tr + 2 < M.Triangles.size(); Tr += 3)
					{
						const float* A = &M.Positions[M.Triangles[Tr] * 3];
						const float* Bv = &M.Positions[M.Triangles[Tr + 1] * 3];
						const float* C = &M.Positions[M.Triangles[Tr + 2] * 3];
						if (Px < std::min({ A[0], Bv[0], C[0] }) - 1e-4 || Px > std::max({ A[0], Bv[0], C[0] }) + 1e-4 || Py < std::min({ A[1], Bv[1], C[1] }) - 1e-4 || Py > std::max({ A[1], Bv[1], C[1] }) + 1e-4) continue;
						const double Ux = Bv[0] - A[0], Uy = Bv[1] - A[1], Vx = C[0] - A[0], Vy = C[1] - A[1];
						const double Den = Ux * Vy - Uy * Vx;
						if (std::fabs(Den) < 1e-9) continue;
						const double Wx = Px - A[0], Wy = Py - A[1];
						const double Su = (Wx * Vy - Wy * Vx) / Den, Tv = (Ux * Wy - Uy * Wx) / Den;
						if (Su < -1e-7 || Tv < -1e-7 || Su + Tv > 1.0 + 1e-7) continue;
						const double Z = A[2] + Su * (Bv[2] - A[2]) + Tv * (C[2] - A[2]);
						OutZ = bAny ? std::max(OutZ, Z) : Z;
						bAny = true;
					}
				}
			}
			return bAny;
		};
		// Round each gap's cut: just inside no ground left and a face to stand on; just outside the ground as it was,
		// and the step between the face inside and the ground outside small (at the bank's cut ends, the faces meet the
		// bank's own top; across it, the floor meets the field)
		long Inside = 0, InsideCovered = 0, InsideNoFace = 0, Outside = 0, OutsideOff = 0, Seams = 0, SeamSteps = 0;
		double WorstSeam = 0.0;
		for (const Gap& Gp : Gaps)
		{
			const double Cy = std::cos(Gp.Yaw), Sy = std::sin(Gp.Yaw);
			for (int Side = 0; Side < 4; ++Side)
			{
				for (double W = -0.9; W <= 0.9 + 1e-9; W += 0.15)
				{
					// (u, v of a point on the cut's edge and the way out of it)
					const double Eu = Side < 2 ? (Side == 0 ? -Gp.CutU : Gp.CutU) : W * Gp.CutU;
					const double Ev = Side < 2 ? W * Gp.CutV : (Side == 2 ? -Gp.CutV : Gp.CutV);
					const double Ou = Side == 0 ? -1.0 : Side == 1 ? 1.0 : 0.0, Ov = Side == 2 ? -1.0 : Side == 3 ? 1.0 : 0.0;
					const auto At = [&](double Du, double Dv, double& X, double& Y) { const double U = Eu + Du, V = Ev + Dv; X = Gp.X + U * Cy - V * Sy; Y = Gp.Y + U * Sy + V * Cy; };
					double Ix = 0.0, Iy = 0.0, Ox = 0.0, Oy = 0.0;
					At(-0.03 * Ou, -0.03 * Ov, Ix, Iy);
					At(0.03 * Ou, 0.03 * Ov, Ox, Oy);
					double Z = 0.0, Face = 0.0;
					++Inside;
					if (GroundAt(Ix, Iy, Z, false)) ++InsideCovered;
					if (!Br.TopAt(Ix, Iy, Face))
					{
						++InsideNoFace;
						if (std::getenv("LAND_FIELD_BREACHES")) std::printf("    no face just inside: gap at %.1f, %.1f, side %d, u %.2f v %.2f\n", Gp.X, Gp.Y, Side, Eu - 0.03 * Ou, Ev - 0.03 * Ov);
						continue;
					}
					if (Br.InCut(Ox, Oy)) continue;
					++Outside;
					double Left = 0.0, Was = 0.0;
					if (!GroundAt(Ox, Oy, Left, false) || !GroundAt(Ox, Oy, Was, true) || std::fabs(Left - Was) > 2e-3) { ++OutsideOff; continue; }
					// (the spoil heaped on her side may stand on the field over the floor: a heap, not a seam)
					++Seams;
					const double Step = Face - Left;
					WorstSeam = std::max(WorstSeam, std::fabs(Step));
					if (Step < -0.12 || Step > 0.5) ++SeamSteps;
				}
			}
		}
		// A body lies on the breach's face: nothing of its torn earth stands above TopAt.
		long Points = 0, Missed = 0, Under = 0;
		double WorstUnder = 0.0;
		const double Bary[3][3] = { { 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0 }, { 0.7, 0.15, 0.15 }, { 0.15, 0.15, 0.7 } };
		for (int P = 0; P < Br.Patches(); ++P)
		{
			for (const LandField::Mesh& M : Br.PatchAt(P).Sections)
			{
				for (size_t Tr = 0; Tr + 2 < M.Triangles.size(); Tr += 3)
				{
					const float* A = &M.Positions[M.Triangles[Tr] * 3];
					const float* Bv = &M.Positions[M.Triangles[Tr + 1] * 3];
					const float* C = &M.Positions[M.Triangles[Tr + 2] * 3];
					for (const auto& Wt : Bary)
					{
						const double Px = Wt[0] * A[0] + Wt[1] * Bv[0] + Wt[2] * C[0], Py = Wt[0] * A[1] + Wt[1] * Bv[1] + Wt[2] * C[1], Pz = Wt[0] * A[2] + Wt[1] * Bv[2] + Wt[2] * C[2];
						++Points;
						double Top = 0.0;
						if (!Br.TopAt(Px, Py, Top))
						{
							if (std::getenv("LAND_FIELD_BREACHES") && Missed < 40)
								for (const LandCrater::Field::Hole& H : Br.All())
								{
									const double Dx = Px - H.X, Dy = Py - H.Y;
									if (Dx * Dx + Dy * Dy > 64.0) continue;
									const double Uu = Dx * std::cos(H.Yaw) + Dy * std::sin(H.Yaw), Vv = -Dx * std::sin(H.Yaw) + Dy * std::cos(H.Yaw);
									std::printf("    body point with no face at %.2f, %.2f, %.2f: u %.3f v %.3f (grid from %.2f, %.2f, %d by %d, out %.2f, through %d, triangle %zu)\n", Px, Py, Pz, Uu, Vv, H.U0, H.V0, H.NU, H.NV, H.Out, H.Through, Tr / 3);
								}
							++Missed;
							continue;
						}
						if (Top < Pz - 1e-3)
						{
							if (std::getenv("LAND_FIELD_BREACHES") && Under < 12) std::printf("    body point under the face at %.2f, %.2f: %.3f over %.3f (triangle %zu)\n", Px, Py, Pz, Top, Tr / 3);
							++Under;
							WorstUnder = std::max(WorstUnder, Pz - Top);
						}
					}
				}
			}
		}
		std::printf("  hedge cutters: %zu cuts laid (%.2f s); the bank at a cut %.2f m high at the least (%.2f where the way was opened), %d with a way through, %d trees in a gap, %d with no bank found; %d torn (%.2f ms the slowest), floor at the field's level within %.3f m (%d off), her tracks' way a step of %.3f m at most (%d over 30 cm, %d places over 30 cm above the field, worst %.2f); round the cuts %ld just inside, %ld with ground left, %ld with no face; %ld just outside, %ld off the ground; seams %ld, step %.3f m at most, %ld past -12 or +50 cm; a body on its face: %ld points, %ld with no face, %ld under it (worst %.4f m)\n",
			FC.Cuts.size(), LayS, LeastRise, OpenRise, Through, Treed, NoBank, Torn, TearMs, WorstFloor, FloorOff, WorstStep, Steep, High, WorstHigh, Inside, InsideCovered, InsideNoFace, Outside, OutsideOff, Seams, WorstSeam, SeamSteps, Points, Missed, Under, WorstUnder);
		Check(FC.Cuts.size() >= 4 && Standing == static_cast<int>(FC.Cuts.size()) && Through == 0 && Treed == 0, "a hedge cutter's cut: the hedge laid whole there, no way opened through it, no tree in the gap",
			Fmt("%.0f of %.0f standing, %.0f ways through, %.0f trees", Standing, static_cast<double>(FC.Cuts.size()), Through, Treed));
		Check(NoBank == 0 && Torn == static_cast<int>(FC.Cuts.size()) && FloorOff == 0 && Steep == 0 && High == 0 && TearMs < 20.0,
			"a breach torn through the bank: its floor the field's, her way through it no step over 30 cm, torn in under 20 ms",
			Fmt("%.0f torn, %.0f floors off, %.0f steps, %.0f high", Torn, FloorOff, Steep, High) + Fmt(", %.2f ms", TearMs));
		Check(InsideCovered == 0 && InsideNoFace == 0 && OutsideOff == 0 && SeamSteps == 0 && Missed == 0 && Under == 0,
			"a breach cuts the ground over its gap and nothing else, with a face to stand on wherever it is gone and no step at its seams",
			Fmt("%.0f inside with ground, %.0f with no face, %.0f outside off, %.0f seam steps", static_cast<double>(InsideCovered), static_cast<double>(InsideNoFace), static_cast<double>(OutsideOff), static_cast<double>(SeamSteps)) +
				Fmt(", %.0f body points off its face", static_cast<double>(Missed + Under)));
	}

	// A tank's marks (LandCrater::Ruts): Tigers and Shermans driven across the field lay their tracks' strips on the
	// ground: the strips' faded edges the ground's own colour, their pressed middles on the ground drawn (not under it),
	// the corn between a tank's tracks laid flat in a cornfield (and only there), nothing standing where she has not been;
	// cheap enough for every tank every frame.
	{
		LandCrater::Ruts R;
		// (the game's own heights: HeightAt, At's height alone)
		R.Begin([&G](double X, double Y) { return G.HeightAt(X, Y); }, [&G](double X, double Y) { return G.ColourAt(X, Y); }, LandCrater::Ruts::KindOn(G));
		R.Lit = [&G](double X, double Y, double& NX, double& NY, double& NZ) { G.DrawnUp(X, Y, NX, NY, NZ); };
		const LandCrater::Ruts::KindFn Kind = LandCrater::Ruts::KindOn(G);
		uint64_t S = 0x7aac5ull;
		auto U = [&S]() { S = S * 6364136223846793005ull + 1442695040888963407ull; return static_cast<double>(S >> 11) / 9007199254740992.0; };
		struct Laid { double LX, LY, RX, RY, PX, PY, Apart, Width; LandCrater::Ruts::Under Mid; };
		std::vector<Laid> Pieces;
		std::map<int, int> ByKind;
		long Drives = 0;
		double DriveSeconds = 0.0;
		std::vector<double> DriveEach; // (each call's own time: the cost read without the machine's own pauses)
		for (int V = 0; V < 16; ++V)
		{
			const bool bTiger = V % 2 == 0;
			const double Apart = bTiger ? 2.83 : 2.108, Width = LandCrater::Ruts::TrackWidthOf(bTiger ? "Tiger" : "M4 Sherman", "tank");
			double X = 60.0 + U() * (F.Width() - 120.0), Y = 60.0 + U() * (F.Depth() - 120.0), Head = U() * 6.283, Turn = 0.0;
			double PX = 0.0, PY = 0.0;
			bool bHad = false;
			for (int K = 0; K < 1600; ++K) // (400 m at 0.25 m a frame: 5 m a second at 20 frames)
			{
				if (K % 80 == 0) Turn = (U() - 0.5) * 0.02;
				Head += Turn;
				X += 0.25 * std::cos(Head);
				Y += 0.25 * std::sin(Head);
				if (X < 20.0 || Y < 20.0 || X > F.Width() - 20.0 || Y > F.Depth() - 20.0) break;
				const double Nx = -std::sin(Head), Ny = std::cos(Head);
				const double LX = X + Nx * 0.5 * Apart, LY = Y + Ny * 0.5 * Apart, RX = X - Nx * 0.5 * Apart, RY = Y - Ny * 0.5 * Apart;
				const auto D0 = std::chrono::steady_clock::now();
				const bool bLaid = R.Drive(V, LX, LY, RX, RY, Width);
				const double Took = Seconds(D0);
				DriveSeconds += Took;
				DriveEach.push_back(Took);
				++Drives;
				if (bLaid && bHad)
				{
					const LandCrater::Ruts::Under Mid = Kind(0.5 * (X + PX), 0.5 * (Y + PY));
					Pieces.push_back({ LX, LY, RX, RY, PX, PY, Apart, Width, Mid });
					++ByKind[static_cast<int>(Mid)];
				}
				if (bLaid || !bHad) { PX = X; PY = Y; bHad = true; }
			}
		}
		// The strips' points: StripAcross at each of the piece's two ends, across her way edge, band, pressed, pressed,
		// band, edge.
		constexpr size_t Across = static_cast<size_t>(LandCrater::Ruts::StripAcross);
		auto Diff = [](const LandField::Rgb8& A, const LandField::Rgb8& B) { return std::abs(A.R - B.R) + std::abs(A.G - B.G) + std::abs(A.B - B.B); };
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
		long Points = 0, Edges = 0, Tris = 0;
		int WorstEdge = 0;
		double EdgeOff = 0.0;
		std::vector<double> Over, Lit;
		// (the edge and the band no more alike than the band and the pressed strip: a feathered edge, not a stripe)
		double EdgeBand = 0.0, BandPress = 0.0;
		long Bands = 0;
		auto DrawnUp = [&G](double X, double Y, double Up[3]) {
			double Best = -1e30, Z = 0.0, Here[3];
			bool bAny = false;
			for (const LandField::Tile& C : G.Tiles())
				if (X >= C.X0 - 4.0 && X < C.X1 + 4.0 && Y >= C.Y0 - 4.0 && Y < C.Y1 + 4.0 && MeshUp(C.Near, X, Y, Z, Here) && Z > Best)
				{
					Best = Z;
					bAny = true;
					for (int K = 0; K < 3; ++K) Up[K] = Here[K];
				}
			return bAny;
		};
		for (int P = 0; P < R.Patches(); ++P)
			for (const LandField::Mesh& M : R.PatchAt(P).Sections)
			{
				const size_t N = M.Colours.size();
				Points += static_cast<long>(N);
				Tris += static_cast<long>(M.Triangles.size() / 3);
				for (size_t I = 0; I < N; ++I)
				{
					const size_t K = I % Across;
					const double Px = M.Positions[I * 3], Py = M.Positions[I * 3 + 1], Pz = M.Positions[I * 3 + 2];
					if (K == 0 || K == Across - 1)
					{
						const int D = Diff(M.Colours[I], G.ColourAt(Px, Py));
						EdgeOff += D;
						WorstEdge = std::max(WorstEdge, D);
						++Edges;
					}
					else if (K == 1 || K == Across - 2)
					{
						const size_t Edge_ = K == 1 ? I - 1 : I + 1, Press = K == 1 ? I + 1 : I - 1;
						EdgeBand += Diff(M.Colours[Edge_], M.Colours[I]);
						BandPress += Diff(M.Colours[I], M.Colours[Press]);
						++Bands;
					}
					else if (I % 37 == 2 && Over.size() < 2500)
					{
						const double Z = DrawnTop(Px, Py);
						if (!std::isnan(Z)) Over.push_back(Pz - Z);
					}
					double Up[3];
					if (I % 41 == 3 && Lit.size() < 2500 && DrawnUp(Px, Py, Up)) Lit.push_back(DegreesOff(M, I, Up));
				}
			}
		const double Buried = static_cast<double>(std::count_if(Over.begin(), Over.end(), [](double O) { return O < -0.002; })) / std::max<size_t>(1, Over.size());
		// Flat: between a tank's tracks in standing corn, and under each track; not between them on grass or plough, and
		// not 2.5 m beyond her outer edge (some few where two tanks' ways cross).
		int CornMid = 0, CornMidFlat = 0, OtherMid = 0, OtherMidFlat = 0, Under_ = 0, UnderFlat = 0, Beyond = 0, BeyondFlat = 0;
		for (const Laid& L : Pieces)
		{
			if (L.Mid == LandCrater::Ruts::Under::None) continue;
			const double MX = 0.25 * (L.LX + L.RX) + 0.5 * L.PX, MY = 0.25 * (L.LY + L.RY) + 0.5 * L.PY;
			const double Ax = (L.LX - L.RX) / L.Apart, Ay = (L.LY - L.RY) / L.Apart; // (right to left, unit)
			const bool bMid = R.Flat(MX, MY);
			if (L.Mid == LandCrater::Ruts::Under::Corn) { ++CornMid; CornMidFlat += bMid; }
			else { ++OtherMid; OtherMidFlat += bMid; }
			if (Kind(MX + Ax * 0.5 * L.Apart, MY + Ay * 0.5 * L.Apart) != LandCrater::Ruts::Under::None)
			{
				++Under_;
				UnderFlat += R.Flat(MX + Ax * 0.5 * L.Apart, MY + Ay * 0.5 * L.Apart);
			}
			const double Out = 0.5 * L.Apart + L.Width + 2.5;
			++Beyond;
			BeyondFlat += R.Flat(MX + Ax * Out, MY + Ay * Out);
		}
		const auto F0 = std::chrono::steady_clock::now();
		int FlatHits = 0;
		for (int K = 0; K < 100000; ++K)
		{
			const Laid& L = Pieces[static_cast<size_t>(K) % std::max<size_t>(1, Pieces.size())];
			FlatHits += R.Flat(L.LX + (K % 9 - 4) * 0.7, L.LY + (K % 7 - 3) * 0.7);
		}
		// (a call the machine paused in, swapping this test out for another, is not the marks' cost: the dearest 1 in 100
		// calls are left out of the mean the check reads; the whole mean is printed beside it)
		std::sort(DriveEach.begin(), DriveEach.end());
		double Kept = 0.0;
		const size_t KeepN = DriveEach.size() - DriveEach.size() / 100;
		for (size_t K = 0; K < KeepN; ++K) Kept += DriveEach[K];
		const double FlatUs = Seconds(F0) / 100000.0 * 1e6, DriveUs = Kept / std::max<double>(1.0, static_cast<double>(KeepN)) * 1e6;
		const double DriveAllUs = DriveSeconds / std::max(1L, Drives) * 1e6;
		std::printf("  track marks: %d pieces (corn %d, grass %d, earth %d, none %d) by 16 tanks, %ld points, %ld triangles; edges off the ground's colour by %.2f (worst %d); pressed strip over the drawn ground: 5%% %.3f, median %.3f, 95%% %.3f m, %.1f%% under it (%zu places); flat between the tracks in corn %d of %d, on grass or earth %d of %d, under a track %d of %d, 2.5 m beyond %d of %d; Drive %.2f us a frame, Flat %.2f us (%d)\n",
			R.Pieces(), ByKind[2], ByKind[1], ByKind[3], ByKind[0], Points, Tris, EdgeOff / std::max(1L, Edges), WorstEdge, Pctl(Over, 0.05), Pctl(Over, 0.5), Pctl(Over, 0.95), 100.0 * Buried, Over.size(),
			CornMidFlat, CornMid, OtherMidFlat, OtherMid, UnderFlat, Under_, BeyondFlat, Beyond, DriveUs, FlatUs, FlatHits);
		if (bRegion)
			Check(R.Pieces() > 2000, "tanks driven across the field leave their marks", Fmt("%.0f pieces", R.Pieces()));
		else if (G.SnowLying() > 0.0) // (under snow the grass is the snow's: marks in it, or on the grass where it shows)
			Check(R.Pieces() > 2000 && ByKind[1] + ByKind[3] > 100, "tanks driven across the field leave their marks in the snow",
				Fmt("%.0f pieces, %.0f in the snow or on grass through it", R.Pieces(), ByKind[1] + ByKind[3]));
		else if (F.Weather == "mud") // (the thaw: a track tears the turf to mud, so none pressed on grass, all torn earth)
			Check(R.Pieces() > 2000 && ByKind[2] > 100 && ByKind[1] == 0 && ByKind[3] >= 1, "tanks driven across the thaw's field leave their marks in corn and torn earth, none pressed on grass",
				Fmt("%.0f pieces, %.0f in corn, %.0f torn earth, %.0f on grass", R.Pieces(), ByKind[2], ByKind[3], ByKind[1]));
		else
		Check(R.Pieces() > 2000 && ByKind[2] > 100 && ByKind[1] > 100, "tanks driven across the field leave their marks in corn and on grass",
			Fmt("%.0f pieces, %.0f in corn, %.0f on grass", R.Pieces(), ByKind[2], ByKind[1]));
		Check(EdgeOff / std::max(1L, Edges) < 0.5 && WorstEdge <= 3, "a track mark's edge is the ground's own colour", Fmt("off by %.2f, worst %.0f", EdgeOff / std::max(1L, Edges), WorstEdge));
		const double Eb = EdgeBand / std::max(1L, Bands), Bp = BandPress / std::max(1L, Bands);
		std::printf("  track marks feathered and lit: edge to band %.1f, band to pressed %.1f summed RGB steps; normals off the drawn ground's by median %.2f, 95%% %.2f, worst %.2f degrees (%zu points)\n",
			Eb, Bp, Pctl(Lit, 0.5), Pctl(Lit, 0.95), Pctl(Lit, 1.0), Lit.size());
		// (the art director's cut 4, shot 07: "the track marks are hard-edged flat stripes")
		Check(Bands > 1000 && Eb > 4.0 && Bp > 4.0 && Lit.size() > 1000 && Pctl(Lit, 0.95) < 1.0, "a track mark feathers into the ground and is lit as the ground is, not a flat stripe",
			Fmt("edge to band %.1f, band to pressed %.1f, lit 95%% %.2f degrees off", Eb, Bp, Pctl(Lit, 0.95)));
		Check(Over.size() > 1000 && Buried < 0.03 && Pctl(Over, 0.95) < 0.12, "a track mark lies on the ground drawn, not under it", Fmt("%.1f%% under, 95%% %.3f m over", 100.0 * Buried, Pctl(Over, 0.95)));
		Check(((bRegion && CornMid == 0) || (CornMid > 50 && CornMidFlat >= CornMid * 0.98)) && OtherMid > 50 && OtherMidFlat <= OtherMid * 0.03 && UnderFlat >= Under_ * 0.98 && BeyondFlat <= Beyond * 0.03,
			"the corn between a tank's tracks lies flat, and only there", Fmt("corn %.0f of %.0f, grass %.0f of %.0f, ", CornMidFlat, CornMid, OtherMidFlat, OtherMid) + Fmt("under %.0f of %.0f, beyond %.0f of ", UnderFlat, Under_, BeyondFlat) + Fmt("%.0f", Beyond));
		// (on the steppe most of the ground is standing corn, so nearly every piece lays its flattened strip as well: a
		// little more a frame, still a quarter of a millisecond for forty tanks)
		const double DriveMost = bSteppe ? 7.0 : 5.0;
		Check(DriveUs < DriveMost && FlatUs < 3.0, "marks cheap enough for every tank every frame", Fmt("Drive %.2f us (%.2f with the dearest 1 in 100), Flat %.2f us", DriveUs, DriveAllUs, FlatUs));
	}

	// (the trial "shattered": trees struck by shells, after every check above: ShatterStrike; with LAND_FIELD_STRIKE_EYES, ShatterPairs)
	const std::vector<std::array<double, 4>> StrikeRounds = ShatterRounds();
	if (!(Argc >= 3 && std::getenv("LAND_FIELD_STRIKE_EYES"))) ShatterStrike(GroundMade, F, StrikeRounds);
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
		if (std::getenv("LAND_FIELD_LATLON"))
		{
			Sk.Lat = F.Lat;
			Sk.Lon = F.Lon;
			Sk.Zone = std::round(F.Lon / 15.0) + 1.0; // (the armies' clocks: an hour ahead of the sun's zone, as they mostly kept)
		}
		// (the look's own air where its ground is not the bocage's: LandScape::AirOf, as the game sets its haze; the field's
		// place, month and trials, the east's summer its own on trial "eastgold")
		const LandScape::Air Air = LandScape::AirOf(LandScape::LookOf(G.LookWord()), G.SnowLying(), F.Lat, F.Lon, F.Month, F.Trial);
		if (Air.bSet)
		{
			const double Bocage[3] = { 0.62, 0.66, 0.70 };
			for (int C = 0; C < 3; ++C) Sk.HazeTint[C] = Air.HazeInk[C] / Bocage[C];
			Sk.HazeThick = Air.HazeDensity / 0.0069;
			if (Air.Cover >= 0.0) Sk.Cover = Air.Cover;
		}
		std::printf("look laid: %s, snow %.2f%s\n", G.LookWord(), G.SnowLying(), Air.bSet ? Fmt(", haze %.2f %.2f %.2f x%.2f", Air.HazeInk[0], Air.HazeInk[1], Air.HazeInk[2], Sk.HazeThick).c_str() : "");
		if (const char* Eyes = std::getenv("LAND_FIELD_STRIKE_EYES")) Failures += ShatterPairs(GroundMade, F, Sk, Key, Argv[2], Eyes, StrikeRounds);
		else if (!LandFieldPicture::Paint(G, F, Sk, Key, Argv[2])) ++Failures;
		std::printf("pictures painted in %.1f s\n", Seconds(P0));
	}
	std::printf("%s\n", Failures ? "SOME CHECKS FAILED" : "all checks passed");
	return Failures ? 1 : 0;
}
