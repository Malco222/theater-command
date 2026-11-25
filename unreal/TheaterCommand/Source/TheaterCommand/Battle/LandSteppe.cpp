#include "Battle/LandSteppe.h"

#include "Battle/LandJson.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace LandSteppe
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		double Clamp01(double X) { return std::clamp(X, 0.0, 1.0); }
		double Smooth(double E0, double E1, double X)
		{
			const double T = Clamp01((X - E0) / (E1 - E0));
			return T * T * (3.0 - 2.0 * T);
		}
		Tone Rgb(double R, double G, double B) { return Tone{ R, G, B }; }
		Tone Mix(const Tone& A, const Tone& B, double T)
		{
			T = Clamp01(T);
			return Tone{ A.R + (B.R - A.R) * T, A.G + (B.G - A.G) * T, A.B + (B.B - A.B) * T };
		}
		Tone Vary(const Tone& C, double K) { return Tone{ C.R * K, C.G * K, C.B * K }; }

		// ------------------------------------------------------------------ the steppe's colours (the art director's to check)
		// July 1943 on the forest-steppe south-west of Prokhorovka, at half past eight: wheat ripening, gold with green in it;
		// winter rye ripe and standing chest high, paler and greyer; pasture and hay meadow gone buff in the heat, feather
		// grass silvering where the ground is poorest; fallow black earth (chernozem) under weeds. A step darker than the kit's
		// tile colours (its wheat CCB46A, its steppe grass B0A474), as the second cut's stills asked of the bocage's pale crops
		// (open ground lit 120 to 160, the art director, 21 November).
		const Tone Wheat = Rgb(170, 148, 82);
		const Tone WheatGreen = Rgb(132, 134, 72);
		const Tone Rye = Rgb(162, 146, 104);
		const Tone RyeGrey = Rgb(138, 132, 104);
		const Tone Grass = Rgb(116, 120, 70);
		const Tone Buff = Rgb(150, 140, 94);
		const Tone Feather = Rgb(164, 160, 126);
		const Tone Soil = Rgb(68, 58, 46);       // black earth turned, damp
		const Tone DrySoil = Rgb(98, 88, 70);    // the black fallow's harrowed top, dried grey-brown in July
		const Tone Weeds = Rgb(96, 102, 62);
		const Tone Strip = Rgb(116, 114, 74);    // the strip where two fields meet: dusty grass and weeds, buff more than green
		const Tone DryGrass = Rgb(128, 124, 80);
		const Tone HollowGreen = Rgb(86, 98, 52);
		const Tone Ballast = Rgb(108, 102, 94);
		const Tone Cinders = Rgb(70, 66, 62);

		/** A tiny mesh writer (the ground's own lives in LandField): points with normals and colours, triangles counter-clockwise from the front. */
		struct Writer
		{
			LandField::Mesh& M;
			explicit Writer(LandField::Mesh& InM) : M(InM) {}
			uint32_t Vert(double X, double Y, double Z, double NX, double NY, double NZ, const Tone& C)
			{
				M.Positions.insert(M.Positions.end(), { static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z) });
				M.Normals.insert(M.Normals.end(), { static_cast<float>(NX), static_cast<float>(NY), static_cast<float>(NZ) });
				LandField::Rgb8 B;
				B.R = static_cast<uint8_t>(std::clamp(std::lround(C.R), 0L, 255L));
				B.G = static_cast<uint8_t>(std::clamp(std::lround(C.G), 0L, 255L));
				B.B = static_cast<uint8_t>(std::clamp(std::lround(C.B), 0L, 255L));
				M.Colours.push_back(B);
				return static_cast<uint32_t>(M.Colours.size() - 1);
			}
			/** A flat quad (its corners in order round it) turned to face (TX, TY, TZ). */
			void Quad(const double* A, const double* B, const double* C, const double* D, const Tone& Col, double TX, double TY, double TZ)
			{
				const double UX = B[0] - A[0], UY = B[1] - A[1], UZ = B[2] - A[2], VX = C[0] - A[0], VY = C[1] - A[1], VZ = C[2] - A[2];
				double NX = UY * VZ - UZ * VY, NY = UZ * VX - UX * VZ, NZ = UX * VY - UY * VX;
				const double L = std::sqrt(NX * NX + NY * NY + NZ * NZ);
				if (L < 1e-12) return;
				NX /= L;
				NY /= L;
				NZ /= L;
				const bool bFlip = NX * TX + NY * TY + NZ * TZ < 0.0;
				if (bFlip)
				{
					NX = -NX;
					NY = -NY;
					NZ = -NZ;
				}
				const uint32_t IA = Vert(A[0], A[1], A[2], NX, NY, NZ, Col), IB = Vert(B[0], B[1], B[2], NX, NY, NZ, Col), IC = Vert(C[0], C[1], C[2], NX, NY, NZ, Col), ID = Vert(D[0], D[1], D[2], NX, NY, NZ, Col);
				if (bFlip) M.Triangles.insert(M.Triangles.end(), { IA, ID, IC, IA, IC, IB });
				else M.Triangles.insert(M.Triangles.end(), { IA, IB, IC, IA, IC, ID });
			}
			/** A flat quad with a colour at each corner, turned to face (TX, TY, TZ). */
			void Quad4(const double* A, const double* B, const double* C, const double* D, const Tone& CA, const Tone& CB, const Tone& CC, const Tone& CD, double TX, double TY, double TZ)
			{
				const double UX = C[0] - A[0], UY = C[1] - A[1], UZ = C[2] - A[2], VX = D[0] - B[0], VY = D[1] - B[1], VZ = D[2] - B[2];
				double NX = UY * VZ - UZ * VY, NY = UZ * VX - UX * VZ, NZ = UX * VY - UY * VX;
				const double L = std::sqrt(NX * NX + NY * NY + NZ * NZ);
				if (L < 1e-12) return;
				NX /= L;
				NY /= L;
				NZ /= L;
				const bool bFlip = NX * TX + NY * TY + NZ * TZ < 0.0;
				if (bFlip)
				{
					NX = -NX;
					NY = -NY;
					NZ = -NZ;
				}
				const uint32_t IA = Vert(A[0], A[1], A[2], NX, NY, NZ, CA), IB = Vert(B[0], B[1], B[2], NX, NY, NZ, CB), IC = Vert(C[0], C[1], C[2], NX, NY, NZ, CC), ID = Vert(D[0], D[1], D[2], NX, NY, NZ, CD);
				if (bFlip) M.Triangles.insert(M.Triangles.end(), { IA, ID, IC, IA, IC, IB });
				else M.Triangles.insert(M.Triangles.end(), { IA, IB, IC, IA, IC, ID });
			}
			/** A box: its middle, three half-extent vectors; its bottom left off. */
			void Box(const double O[3], const double AX[3], const double AY[3], const double AZ[3], const Tone& Col, const Tone* Top = nullptr)
			{
				double P[8][3];
				for (int K = 0; K < 8; ++K)
				{
					const double SX = (K & 1) ? 1.0 : -1.0, SY = (K & 2) ? 1.0 : -1.0, SZ = (K & 4) ? 1.0 : -1.0;
					for (int D = 0; D < 3; ++D) P[K][D] = O[D] + SX * AX[D] + SY * AY[D] + SZ * AZ[D];
				}
				Quad(P[0], P[1], P[5], P[4], Col, -AY[0], -AY[1], -AY[2]);
				Quad(P[2], P[3], P[7], P[6], Col, AY[0], AY[1], AY[2]);
				Quad(P[0], P[2], P[6], P[4], Col, -AX[0], -AX[1], -AX[2]);
				Quad(P[1], P[3], P[7], P[5], Col, AX[0], AX[1], AX[2]);
				Quad(P[4], P[5], P[7], P[6], Top ? *Top : Col, AZ[0], AZ[1], AZ[2]);
			}
		};

		uint64_t Mix64(uint64_t H)
		{
			H ^= H >> 33;
			H *= 0xFF51AFD7ED558CCDull;
			H ^= H >> 33;
			H *= 0xC4CEB9FE1A85EC53ull;
			H ^= H >> 33;
			return H;
		}
		double Unit(uint64_t H) { return static_cast<double>(Mix64(H) >> 11) * (1.0 / 9007199254740992.0); }

		/** A colour from "RRGGBB". */
		Tone Hex(const std::string& S)
		{
			if (S.size() < 6) return Rgb(128, 128, 128);
			const unsigned long V = std::strtoul(S.substr(S.size() - 6).c_str(), nullptr, 16);
			return Rgb(static_cast<double>((V >> 16) & 255), static_cast<double>((V >> 8) & 255), static_cast<double>(V & 255));
		}

		/** One level of a kit model into a mesh, mirrored into the ground's frame and wound as its facings say. */
		bool ReadLevel(const LandJson::Json& L, const std::vector<Tone>& Colours, LandField::Mesh& Out, std::string& OutError)
		{
			const std::vector<double> P = LandJson::Numbers(L["points"]), F = LandJson::Numbers(L["facings"]), Paint = LandJson::Numbers(L["paints"]), T = LandJson::Numbers(L["triangles"]);
			const size_t N = Paint.size();
			if (N == 0 || P.size() != 3 * N || F.size() != 3 * N || T.size() % 3 != 0)
			{
				OutError = "the kit model's points, facings, paints and triangles do not agree";
				return false;
			}
			Writer W(Out);
			const uint32_t Base0 = static_cast<uint32_t>(Out.Colours.size());
			for (size_t K = 0; K < N; ++K)
			{
				const size_t C = static_cast<size_t>(std::max(0.0, Paint[K]));
				const Tone Col = C < Colours.size() ? Colours[C] : Rgb(128, 128, 128);
				W.Vert(P[3 * K], -P[3 * K + 1], P[3 * K + 2], F[3 * K], -F[3 * K + 1], F[3 * K + 2], Col);
			}
			for (size_t K = 0; K + 2 < T.size(); K += 3)
			{
				uint32_t A = static_cast<uint32_t>(T[K]), B = static_cast<uint32_t>(T[K + 1]), C = static_cast<uint32_t>(T[K + 2]);
				if (A >= N || B >= N || C >= N)
				{
					OutError = "a kit model's triangle names a point it does not have";
					return false;
				}
				// (counter-clockwise seen from the side the points' facings say is the front)
				auto At = [&Out, Base0](uint32_t I, int D) { return static_cast<double>(Out.Positions[(Base0 + I) * 3 + static_cast<uint32_t>(D)]); };
				auto Nm = [&Out, Base0](uint32_t I, int D) { return static_cast<double>(Out.Normals[(Base0 + I) * 3 + static_cast<uint32_t>(D)]); };
				const double UX = At(B, 0) - At(A, 0), UY = At(B, 1) - At(A, 1), UZ = At(B, 2) - At(A, 2);
				const double VX = At(C, 0) - At(A, 0), VY = At(C, 1) - At(A, 1), VZ = At(C, 2) - At(A, 2);
				const double CX = UY * VZ - UZ * VY, CY = UZ * VX - UX * VZ, CZ = UX * VY - UY * VX;
				const double Face = CX * (Nm(A, 0) + Nm(B, 0) + Nm(C, 0)) + CY * (Nm(A, 1) + Nm(B, 1) + Nm(C, 1)) + CZ * (Nm(A, 2) + Nm(B, 2) + Nm(C, 2));
				if (Face < 0.0) std::swap(B, C);
				Out.Triangles.insert(Out.Triangles.end(), { Base0 + A, Base0 + B, Base0 + C });
			}
			return true;
		}
	}

	// ====================================================================== the plan

	Plan::Plan(const LandField::SteppeLay& InLay) : Lay(InLay)
	{
		Cos.push_back(std::cos(Lay.Turn));
		Sin.push_back(std::sin(Lay.Turn));
		Cols = Lay.Xs.size() >= 2 ? static_cast<int>(Lay.Xs.size()) - 1 : 0;
		Rows = Lay.Ys.size() >= 2 ? static_cast<int>(Lay.Ys.size()) - 1 : 0;
		if (static_cast<size_t>(Cols) * static_cast<size_t>(Rows) > Lay.Patch.size()) Cols = Rows = 0;
		// The railway every 10 m of its northing, from the lay's points (it runs south to north), smoothed a little.
		std::vector<std::pair<double, double>> L = Lay.Rail;
		std::sort(L.begin(), L.end(), [](const std::pair<double, double>& A, const std::pair<double, double>& B) { return A.second < B.second; });
		if (L.size() >= 2)
		{
			for (double Y = L.front().second; Y <= L.back().second + 1e-6; Y += 10.0)
			{
				size_t K = 1;
				while (K + 1 < L.size() && L[K].second < Y) ++K;
				const double Span = L[K].second - L[K - 1].second;
				const double T = Span > 1e-9 ? Clamp01((Y - L[K - 1].second) / Span) : 0.0;
				Rail.push_back({ L[K - 1].first + (L[K].first - L[K - 1].first) * T, Y });
			}
			for (int Pass = 0; Pass < 3; ++Pass)
			{
				std::vector<std::pair<double, double>> S = Rail;
				for (size_t K = 1; K + 1 < Rail.size(); ++K) S[K].first = 0.25 * Rail[K - 1].first + 0.5 * Rail[K].first + 0.25 * Rail[K + 1].first;
				Rail = std::move(S);
			}
		}
	}

	int Plan::FieldAt(double X, double Y, int& OutCrop, double* OutGap) const
	{
		const double C = Cos[0], S = Sin[0];
		const double U = Lay.CX + (X - Lay.CX) * C + (Y - Lay.CY) * S;
		double V = Lay.CY - (X - Lay.CX) * S + (Y - Lay.CY) * C;
		int Col = -1, Row = -1;
		if (Cols > 0 && Rows > 0)
		{
			Col = static_cast<int>(std::upper_bound(Lay.Xs.begin(), Lay.Xs.end(), U) - Lay.Xs.begin()) - 1;
			// (each column's fields moved north by its own shift, so no edge runs on unbroken from one column to the next)
			if (Col >= 0 && static_cast<size_t>(Col) < Lay.Shift.size()) V -= Lay.Shift[static_cast<size_t>(Col)];
			Row = static_cast<int>(std::upper_bound(Lay.Ys.begin(), Lay.Ys.end(), V) - Lay.Ys.begin()) - 1;
		}
		if (Col < 0 || Row < 0 || Col >= Cols || Row >= Rows)
		{
			// (beyond the fields laid: pasture, in squares of a kilometre and a half)
			OutCrop = CropPasture;
			const int64_t I = static_cast<int64_t>(std::floor(U / 1500.0)), J = static_cast<int64_t>(std::floor(V / 1500.0));
			if (OutGap) *OutGap = std::min({ U - I * 1500.0, (I + 1) * 1500.0 - U, V - J * 1500.0, (J + 1) * 1500.0 - V });
			return 900000 + static_cast<int>((static_cast<uint64_t>(I * 7919 + J * 104729) & 0xFFFFull) % 99000ull);
		}
		const int Id = Row * Cols + Col;
		OutCrop = std::clamp(static_cast<int>(Lay.Patch[static_cast<size_t>(Id)]), 1, 4);
		if (OutGap)
		{
			const double X0 = Lay.Xs[static_cast<size_t>(Col)], X1 = Lay.Xs[static_cast<size_t>(Col) + 1], Y0 = Lay.Ys[static_cast<size_t>(Row)], Y1 = Lay.Ys[static_cast<size_t>(Row) + 1];
			*OutGap = std::min({ U - X0, X1 - U, V - Y0, Y1 - V });
		}
		return Id;
	}

	double Plan::RailX(double Y) const
	{
		if (Rail.size() < 2) return -1e9;
		if (Y <= Rail.front().second)
		{
			const double Slope = (Rail[1].first - Rail[0].first) / (Rail[1].second - Rail[0].second);
			return Rail.front().first + (Y - Rail.front().second) * Slope;
		}
		if (Y >= Rail.back().second)
		{
			const size_t E = Rail.size() - 1;
			const double Slope = (Rail[E].first - Rail[E - 1].first) / (Rail[E].second - Rail[E - 1].second);
			return Rail.back().first + (Y - Rail.back().second) * Slope;
		}
		const double F = (Y - Rail.front().second) / 10.0;
		const size_t K = std::min(Rail.size() - 2, static_cast<size_t>(F));
		const double T = Clamp01(F - static_cast<double>(K));
		return Rail[K].first + (Rail[K + 1].first - Rail[K].first) * T;
	}

	void Plan::RailWay(double Y, double& OutX, double& OutY) const
	{
		const double DX = RailX(Y + 5.0) - RailX(Y - 5.0), DY = 10.0;
		const double L = std::sqrt(DX * DX + DY * DY);
		OutX = DX / L;
		OutY = DY / L;
	}

	double Plan::RailDistance(double X, double Y) const
	{
		if (Rail.size() < 2) return 1e9;
		const double Gap = std::fabs(X - RailX(Y));
		if (Gap > 30.0) return Gap; // (far off it, the eastward gap is near enough, and cheaper)
		double WX = 0.0, WY = 1.0;
		RailWay(Y, WX, WY);
		// (the line runs near enough north that its distance is the eastward gap times its way's northward part)
		return Gap * WY;
	}

	double Plan::BankRise(double X, double Y) const
	{
		if (Rail.size() < 2 || Lay.RailBank <= 0.0) return 0.0;
		const double Top = 3.4, Side = 1.5 * Lay.RailBank;
		const double D = RailDistance(X, Y);
		if (D <= Top) return Lay.RailBank;
		if (D >= Top + Side) return 0.0;
		const double U = (D - Top) / Side;
		return Lay.RailBank * 0.5 * (1.0 + std::cos(Pi * U));
	}

	// ====================================================================== colours

	Tone FieldColour(int Crop, double N1, double N2, double Broad, double Lands, double Streak, double Edge)
	{
		Tone C;
		switch (Crop)
		{
		case CropWheat:
			C = Mix(Wheat, WheatGreen, 0.3 * Smooth(0.15, 0.6, N2 + 0.6 * N1));
			C = Vary(C, 1.0 + 0.07 * Lands + 0.07 * Streak + 0.09 * N1);
			break;
		case CropRye:
			C = Mix(Rye, RyeGrey, 0.35 * Smooth(0.1, 0.6, N1));
			C = Vary(C, 1.0 + 0.05 * Lands + 0.09 * Streak + 0.08 * N1);
			break;
		case CropFallow:
			// (black fallow, ploughed in spring and harrowed since: its dry top grey-brown, darker where the harrow left
			// it rough, a few weeds coming)
			C = Mix(DrySoil, Soil, 0.25 + 0.2 * Smooth(-0.3, 0.5, N1));
			C = Mix(C, Weeds, 0.18 * Smooth(0.3, 0.8, N2 + 0.4 * N1));
			C = Vary(C, 1.0 + 0.05 * Lands + 0.05 * N2);
			break;
		default:
			C = Mix(Grass, Buff, 0.35 + 0.3 * N1);
			C = Mix(C, Feather, 0.25 * Smooth(0.45, 0.8, N2));
			C = Vary(C, 1.0 + 0.06 * N2);
			break;
		}
		// The strip where two fields meet: grass and weeds a metre or two wide, no hedge.
		return Mix(Vary(C, 1.0 + 0.04 * N2 + 0.05 * Broad), StripTone(N2, Broad), StripShare(Edge));
	}

	Tone StripTone(double N2, double Broad) { return Vary(Strip, 1.0 + 0.04 * N2 + 0.05 * Broad); }
	double StripShare(double Edge) { return Smooth(3.4, 1.0, Edge); }

	Tone FarFieldColour(int Crop, double N1, double Broad, double Gap)
	{
		Tone C = Crop == CropWheat ? Mix(Wheat, WheatGreen, 0.15) : Crop == CropRye ? Mix(Rye, RyeGrey, 0.15) : Crop == CropFallow ? Mix(DrySoil, Soil, 0.3) : Mix(Grass, Buff, 0.35);
		C = Vary(C, 1.0 + 0.07 * N1 + 0.05 * Broad);
		return Mix(C, Strip, 0.3 * Smooth(9.0, 1.0, Gap));
	}

	Tone RoughColour(double N1, double N2)
	{
		const Tone C = Mix(DryGrass, HollowGreen, 0.4 * Smooth(-0.2, 0.6, N1));
		return Vary(C, 1.0 + 0.08 * N2);
	}

	Tone BankColour(double D, double N1, double N2, const Tone& Under)
	{
		// (continuous across: the ballast's cinders, its edge into the bank's dry grass, the grass into the land's own)
		const Tone Bed = Vary(Mix(Ballast, Cinders, 0.35 * Smooth(0.0, 0.6, N2) + 0.2 * Smooth(1.6, 2.4, D)), 1.0 + 0.05 * N1);
		if (D < 2.4) return Bed;
		const Tone Side = RoughColour(N1, N2);
		if (D < 3.6) return Mix(Bed, Side, Smooth(2.4, 3.6, D));
		return Mix(Side, Under, Smooth(6.0, 9.5, D));
	}

	Tone BlackEarth() { return Soil; }
	Tone RoadDust() { return Rgb(112, 100, 84); }
	Tone RoadRut() { return Rgb(86, 76, 64); }

	// ====================================================================== the east kit

	const char* KitName(LandField::Base B)
	{
		using LandField::Base;
		switch (B)
		{
		case Base::Khata: return "Khata";
		case Base::Chimney: return "BurntChimney";
		case Base::Pole: return "TelegraphPole";
		case Base::Wattle: return "WattleFence";
		case Base::Sweep: return "WellSweep";
		case Base::Haystack: return "Haystack";
		case Base::Birch: return "Birch";
		default: return "";
		}
	}

	bool ReadKit(const std::string& Text, double FarFrom, LandField::BaseMesh& Out, std::string& OutError)
	{
		LandJson::Json Root;
		if (!LandJson::Parse(Text, Root, OutError)) return false;
		std::vector<Tone> Colours;
		for (size_t K = 0; K < Root["colours"].Count(); ++K) Colours.push_back(Hex(Root["colours"].At(K).Str("808080")));
		LandField::Mesh Near, Mid, Far;
		if (!ReadLevel(Root["near"], Colours, Near, OutError)) return false;
		if (Root["far"].IsObject() && !ReadLevel(Root["far"], Colours, Far, OutError)) return false;
		// (its middle level too where the file has one: the game draws it from MidFrom, GS9 by class)
		if (Root["mid"].IsObject() && !ReadLevel(Root["mid"], Colours, Mid, OutError)) return false;
		Out.Near = std::move(Near);
		Out.Mid = std::move(Mid);
		Out.Far = std::move(Far);
		Out.FarFrom = static_cast<float>(FarFrom);
		return true;
	}

	void PlainKit(LandField::Base B, LandField::BaseMesh& Out)
	{
		using LandField::Base;
		Writer W(Out.Near);
		const Tone White = Rgb(214, 208, 192), Straw = Rgb(140, 120, 84), Wood = Rgb(96, 80, 62), Hay = Rgb(156, 136, 88), Leaf = Rgb(96, 118, 58), Bark = Rgb(208, 204, 196);
		auto Box = [&W](double X, double Y, double Z, double HX, double HY, double HZ, const Tone& C) {
			const double O[3] = { X, Y, Z }, AX[3] = { HX, 0, 0 }, AY[3] = { 0, HY, 0 }, AZ[3] = { 0, 0, HZ };
			W.Box(O, AX, AY, AZ, C);
		};
		switch (B)
		{
		case Base::Khata:
			Box(0, 0, 1.2, 4.4, 3.0, 1.2, White);
			Box(0, 0, 3.0, 4.8, 3.4, 0.6, Straw);
			break;
		case Base::Chimney:
			Box(0, 0, 1.3, 0.6, 0.6, 1.3, Rgb(120, 110, 100));
			Box(0, 0, 0.1, 4.0, 3.0, 0.1, Rgb(46, 42, 38));
			break;
		case Base::Pole:
			Box(0, 0, 3.6, 0.1, 0.1, 3.6, Wood);
			Box(0, 0, 6.8, 0.06, 0.7, 0.05, Wood);
			break;
		case Base::Wattle: Box(0, 0, 0.7, 2.0, 0.06, 0.7, Rgb(112, 92, 64)); break;
		case Base::Sweep:
			Box(0, 0, 1.6, 0.12, 0.12, 1.6, Wood);
			Box(0, 0, 3.8, 2.4, 0.06, 0.06, Wood);
			break;
		case Base::Haystack: Box(0, 0, 1.6, 1.6, 1.6, 1.6, Hay); break;
		case Base::Birch:
			Box(0, 0, 4.0, 0.15, 0.15, 4.0, Bark);
			Box(0, 0, 10.0, 2.6, 2.6, 4.5, Leaf);
			break;
		default: break;
		}
		Out.FarFrom = 160.0f;
	}

	// ====================================================================== the railway

	void LayRail(const Plan& P, double Y0, double Y1, bool bFar, const std::function<double(double, double)>& Ground, const std::function<Tone(double, double)>& Paint, const std::function<LandField::Mesh*(double, double)>& Target, double& OutKm)
	{
		OutKm = 0.0;
		if (!P.HasRail() || Y1 <= Y0) return;
		const Tone Sleeper = Rgb(66, 54, 44), RailSide = Rgb(84, 68, 56), RailTop = Rgb(128, 122, 114);
		// The bank's section, right to left of the way: its feet tucked under the land, its sides, its top's edges (the
		// ballast covers its middle). The ballast's bed: a low ridge 3.2 m across its crest and 4.6 at its foot.
		static const double NearAcross[] = { 8.6, 7.9, 6.9, 5.9, 4.9, 4.1, 3.4, 2.1, -2.1, -3.4, -4.1, -4.9, -5.9, -6.9, -7.9, -8.6 };
		static const double FarAcross[] = { 8.6, 6.4, 4.6, 3.4, 2.1, -2.1, -3.4, -4.6, -6.4, -8.6 };
		const double* Across = bFar ? FarAcross : NearAcross;
		const int NA = bFar ? static_cast<int>(std::size(FarAcross)) : static_cast<int>(std::size(NearAcross));
		const double Step = bFar ? 16.0 : 4.0, Crest = 1.6, Foot = 2.3, Deep = 0.32;
		for (double Y = Y0; Y < Y1 - 1e-6; Y += Step)
		{
			const double YB = std::min(Y1, Y + Step);
			double WXA = 0, WYA = 1, WXB = 0, WYB = 1;
			P.RailWay(Y, WXA, WYA);
			P.RailWay(YB, WXB, WYB);
			const double XA = P.RailX(Y), XB = P.RailX(YB);
			LandField::Mesh* M = Target(0.5 * (XA + XB), 0.5 * (Y + YB));
			if (!M) continue;
			Writer W(*M);
			// (a point Off metres right of the way at a step's end; its height on the bank, the feet sunk under the land)
			auto At = [&](bool bB, double Off, double Up, double* Out) {
				const double X = bB ? XB : XA, Yy = bB ? YB : Y, WX = bB ? WXB : WXA, WY = bB ? WYB : WYA;
				Out[0] = X + WY * Off;
				Out[1] = Yy - WX * Off;
				Out[2] = Ground(Out[0], Out[1]) + Up - (std::fabs(Off) > 8.2 ? 0.12 : 0.0);
			};
			// The bank: its grass and its cinders, painted as the ground is.
			for (int K = 0; K + 1 < NA; ++K)
			{
				if (std::fabs(Across[K]) < 2.2 && std::fabs(Across[K + 1]) < 2.2) continue;
				double A0[3], A1[3], B0[3], B1[3];
				At(false, Across[K], 0.0, A0);
				At(false, Across[K + 1], 0.0, A1);
				At(true, Across[K], 0.0, B0);
				At(true, Across[K + 1], 0.0, B1);
				W.Quad4(A0, B0, B1, A1, Paint(A0[0], A0[1]), Paint(B0[0], B0[1]), Paint(B1[0], B1[1]), Paint(A1[0], A1[1]), 0.0, 0.0, 1.0);
			}
			const double Tone1 = 0.94 + 0.12 * Unit(static_cast<uint64_t>(std::llround(Y * 10.0)) ^ 0xBA11);
			const double Off4[4] = { Foot, Crest, -Crest, -Foot }, Up4[4] = { -0.06, bFar ? 0.6 * Deep : Deep, bFar ? 0.6 * Deep : Deep, -0.06 };
			double PA[4][3], PB[4][3];
			for (int K = 0; K < 4; ++K)
			{
				At(false, Off4[K], Up4[K], PA[K]);
				At(true, Off4[K], Up4[K], PB[K]);
			}
			W.Quad(PA[0], PB[0], PB[1], PA[1], Vary(Mix(Ballast, Cinders, 0.25), Tone1), WYA, -WXA, 1.0);
			W.Quad(PA[1], PB[1], PB[2], PA[2], Vary(Mix(Ballast, Cinders, 0.45), Tone1), 0.0, 0.0, 1.0);
			W.Quad(PA[2], PB[2], PB[3], PA[3], Vary(Mix(Ballast, Cinders, 0.25), Tone1), -WYA, WXA, 1.0);
			if (bFar) continue;
			// The rails: two steel bars on the sleepers, 1.524 m apart, each 0.07 m across its head and 0.15 high.
			const double ZA = 0.5 * (PA[1][2] + PA[2][2]), ZB = 0.5 * (PB[1][2] + PB[2][2]);
			for (int Side = -1; Side <= 1; Side += 2)
			{
				const double G = 0.762 * Side;
				const double MX = 0.5 * (XA + XB) + 0.5 * (WYA + WYB) * G, MY = 0.5 * (Y + YB) - 0.5 * (WXA + WXB) * G, MZ = 0.5 * (ZA + ZB) + 0.15 + 0.075;
				const double Half = 0.5 * std::hypot(XB - XA, YB - Y);
				const double O[3] = { MX, MY, MZ }, AX[3] = { 0.5 * (WXA + WXB) * Half, 0.5 * (WYA + WYB) * Half, 0.5 * (ZB - ZA) }, AY[3] = { 0.5 * (WYA + WYB) * 0.035, -0.5 * (WXA + WXB) * 0.035, 0 }, AZ[3] = { 0, 0, 0.075 };
				W.Box(O, AX, AY, AZ, RailSide, &RailTop);
			}
			// The sleepers, every 0.55 m: 2.75 m long, 0.25 wide, 0.15 deep, creosoted and weathered.
			for (double S = std::ceil(Y / 0.55) * 0.55; S < YB; S += 0.55)
			{
				const double T = (S - Y) / (YB - Y);
				const double X = XA + (XB - XA) * T, Z = ZA + (ZB - ZA) * T + 0.075;
				const double WX = WXA + (WXB - WXA) * T, WY = WYA + (WYB - WYA) * T;
				const double O[3] = { X, S, Z }, AX[3] = { WY * 1.375, -WX * 1.375, 0 }, AY[3] = { WX * 0.125, WY * 0.125, 0 }, AZ[3] = { 0, 0, 0.075 };
				const Tone C = Vary(Sleeper, 0.85 + 0.3 * Unit(static_cast<uint64_t>(std::llround(S * 100.0)) ^ 0x51EE));
				W.Box(O, AX, AY, AZ, C);
			}
			OutKm += std::hypot(XB - XA, YB - Y) / 1000.0;
		}
	}

	void PolesAlongRail(const Plan& P, double Y0, double Y1, double Step, double Off, std::vector<std::pair<double, double>>& OutAt, std::vector<double>& OutYaw)
	{
		if (!P.HasRail()) return;
		for (double Y = std::ceil(Y0 / Step) * Step; Y <= Y1; Y += Step)
		{
			double WX = 0.0, WY = 1.0;
			P.RailWay(Y, WX, WY);
			// (to the west of the line: the way turned left)
			OutAt.push_back({ P.RailX(Y) - WY * Off, Y + WX * Off });
			OutYaw.push_back(std::atan2(WY, WX));
		}
	}
}
