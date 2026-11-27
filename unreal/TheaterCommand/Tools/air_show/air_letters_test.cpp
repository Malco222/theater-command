// The bomber markings' test (Battle/AirLetters; the Battle rules card's "Bomber markings", row flyBomberMarks; Brian,
// 25 November 2026, of the approved studio pictures: "Build it into the game"). The data file
// (Content/Data/AirMarks/b17-fine-marks.json, Tools/air_marks/make_b17_marks.py): read as the game reads it, whole for its
// bundle, made on the finer B-17 the game holds (her file's sha256), every triangle wound as her own file's, every fifth
// point of each level 3 to 40 mm off that level's skin of her (the fin's and side's along the point's facing, the wing's
// straight down), the letter F reading the right way round on both faces of her fin and on her wing seen from above, the
// wing's triangle apex forward, nothing laid over her star and bar. Then who wears which letters: each recording played
// through the game's own show and the book taking its aircraft every step, every third, every seventh from the fifth,
// and with every aircraft hidden: the same wears each time; every American B-17 given hers, one letter a group and none
// shared, codes not shared between groups, a group's (code, letter) never twice; the 303rd's C, VK and A; the same wears
// with the battle's date moved; and the show's own record the same with the book taking and without. No word of month or
// year in Battle/AirLetters.
// Usage: air_letters_test <b17-fine-marks.json> <Content/Models> <Source/TheaterCommand> [recording.json ...]
#include "Battle/AirFeed.h"
#include "Battle/AirLetters.h"
#include "Battle/AirShow.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
	int Bad = 0;

	void Check(bool bOk, const char* What, const std::string& Detail = std::string())
	{
		std::printf("  %s  %s%s%s\n", bOk ? "ok  " : "FAIL", What, Detail.empty() ? "" : ": ", Detail.c_str());
		if (!bOk) ++Bad;
	}

	bool ReadText(const std::string& Path, std::string& Out)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In) return false;
		Out.assign(std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>());
		return true;
	}

	/** SHA-256 (FIPS 180-4), written out, for her file's fingerprint. */
	std::string Sha256(const std::string& Data)
	{
		static const uint32_t K[64] = {
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
			0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
			0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
			0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
			0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
			0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
			0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
		uint32_t H[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
		std::string M = Data;
		const uint64_t Bits = static_cast<uint64_t>(Data.size()) * 8u;
		M.push_back(static_cast<char>(0x80));
		while (M.size() % 64 != 56) M.push_back('\0');
		for (int I = 7; I >= 0; --I) M.push_back(static_cast<char>((Bits >> (8 * I)) & 0xFFu));
		auto Rot = [](uint32_t X, int N) { return (X >> N) | (X << (32 - N)); };
		for (size_t Block = 0; Block < M.size(); Block += 64)
		{
			uint32_t W[64];
			for (int I = 0; I < 16; ++I)
			{
				W[I] = 0;
				for (int B = 0; B < 4; ++B) W[I] = (W[I] << 8) | static_cast<unsigned char>(M[Block + 4 * I + B]);
			}
			for (int I = 16; I < 64; ++I)
			{
				const uint32_t S0 = Rot(W[I - 15], 7) ^ Rot(W[I - 15], 18) ^ (W[I - 15] >> 3);
				const uint32_t S1 = Rot(W[I - 2], 17) ^ Rot(W[I - 2], 19) ^ (W[I - 2] >> 10);
				W[I] = W[I - 16] + S0 + W[I - 7] + S1;
			}
			uint32_t A = H[0], B = H[1], C = H[2], D = H[3], E = H[4], F = H[5], G = H[6], Hh = H[7];
			for (int I = 0; I < 64; ++I)
			{
				const uint32_t T1 = Hh + (Rot(E, 6) ^ Rot(E, 11) ^ Rot(E, 25)) + ((E & F) ^ (~E & G)) + K[I] + W[I];
				const uint32_t T2 = (Rot(A, 2) ^ Rot(A, 13) ^ Rot(A, 22)) + ((A & B) ^ (A & C) ^ (B & C));
				Hh = G; G = F; F = E; E = D + T1; D = C; C = B; B = A; A = T1 + T2;
			}
			H[0] += A; H[1] += B; H[2] += C; H[3] += D; H[4] += E; H[5] += F; H[6] += G; H[7] += Hh;
		}
		std::string Out;
		char Hex[9];
		for (const uint32_t V : H)
		{
			std::snprintf(Hex, sizeof Hex, "%08x", V);
			Out += Hex;
		}
		return Out;
	}

	struct V3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};
	V3 Sub(const V3& A, const V3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
	double Dot(const V3& A, const V3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	V3 Cross(const V3& A, const V3& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
	V3 PointOf(const std::vector<double>& P, int I) { return { P[3 * I], P[3 * I + 1], P[3 * I + 2] }; }

	/** One level of her skin, as triangles. */
	struct Skin
	{
		std::vector<V3> A, B, C;
	};
	bool ReadSkin(const AirFeed::Json& Level, Skin& Out)
	{
		const std::vector<AirFeed::Json>& P = Level["points"].Items;
		const std::vector<AirFeed::Json>& T = Level["triangles"].Items;
		if (P.empty() || T.empty()) return false;
		std::vector<V3> Pts(P.size() / 3);
		for (size_t I = 0; I < Pts.size(); ++I) Pts[I] = { P[3 * I].Num(), P[3 * I + 1].Num(), P[3 * I + 2].Num() };
		for (size_t I = 0; I + 2 < T.size(); I += 3)
		{
			Out.A.push_back(Pts[static_cast<size_t>(T[I].Int())]);
			Out.B.push_back(Pts[static_cast<size_t>(T[I + 1].Int())]);
			Out.C.push_back(Pts[static_cast<size_t>(T[I + 2].Int())]);
		}
		return true;
	}

	/** The skin's triangles within a box (Pad metres round it). */
	std::vector<size_t> Within(const Skin& S, const V3& Lo, const V3& Hi, double Pad)
	{
		std::vector<size_t> Out;
		for (size_t I = 0; I < S.A.size(); ++I)
		{
			const V3& A = S.A[I];
			const V3& B = S.B[I];
			const V3& C = S.C[I];
			if (std::max({ A.X, B.X, C.X }) < Lo.X - Pad || std::min({ A.X, B.X, C.X }) > Hi.X + Pad) continue;
			if (std::max({ A.Y, B.Y, C.Y }) < Lo.Y - Pad || std::min({ A.Y, B.Y, C.Y }) > Hi.Y + Pad) continue;
			if (std::max({ A.Z, B.Z, C.Z }) < Lo.Z - Pad || std::min({ A.Z, B.Z, C.Z }) > Hi.Z + Pad) continue;
			Out.push_back(I);
		}
		return Out;
	}

	/** Where along O + t D the ray meets triangle I (Moller-Trumbore), or false. */
	bool Meet(const Skin& S, size_t I, const V3& O, const V3& D, double& T)
	{
		const V3 E1 = Sub(S.B[I], S.A[I]), E2 = Sub(S.C[I], S.A[I]);
		const V3 P = Cross(D, E2);
		const double Det = Dot(E1, P);
		if (std::fabs(Det) < 1e-12) return false;
		const double Inv = 1.0 / Det;
		const V3 Q0 = Sub(O, S.A[I]);
		const double U = Dot(Q0, P) * Inv;
		if (U < -1e-9 || U > 1.0 + 1e-9) return false;
		const V3 Q = Cross(Q0, E1);
		const double V = Dot(D, Q) * Inv;
		if (V < -1e-9 || U + V > 1.0 + 1e-9) return false;
		T = Dot(E2, Q) * Inv;
		return true;
	}

	/** The nearest point of triangle I to P (Ericson's Real-Time Collision Detection, 5.1.5). */
	V3 Nearest(const Skin& S, size_t I, const V3& P)
	{
		const V3& A = S.A[I];
		const V3& B = S.B[I];
		const V3& C = S.C[I];
		auto At = [](const V3& O, const V3& U, double K) { return V3{ O.X + U.X * K, O.Y + U.Y * K, O.Z + U.Z * K }; };
		const V3 Ab = Sub(B, A), Ac = Sub(C, A), Ap = Sub(P, A);
		const double D1 = Dot(Ab, Ap), D2 = Dot(Ac, Ap);
		if (D1 <= 0.0 && D2 <= 0.0) return A;
		const V3 Bp = Sub(P, B);
		const double D3 = Dot(Ab, Bp), D4 = Dot(Ac, Bp);
		if (D3 >= 0.0 && D4 <= D3) return B;
		const double Vc = D1 * D4 - D3 * D2;
		if (Vc <= 0.0 && D1 >= 0.0 && D3 <= 0.0) return At(A, Ab, D1 / (D1 - D3));
		const V3 Cp = Sub(P, C);
		const double D5 = Dot(Ab, Cp), D6 = Dot(Ac, Cp);
		if (D6 >= 0.0 && D5 <= D6) return C;
		const double Vb = D5 * D2 - D1 * D6;
		if (Vb <= 0.0 && D2 >= 0.0 && D6 <= 0.0) return At(A, Ac, D2 / (D2 - D6));
		const double Va = D3 * D6 - D5 * D4;
		if (Va <= 0.0 && (D4 - D3) >= 0.0 && (D5 - D6) >= 0.0) return At(B, Sub(C, B), (D4 - D3) / ((D4 - D3) + (D5 - D6)));
		const double Den = 1.0 / (Va + Vb + Vc);
		const V3 Q = At(A, Ab, Vb * Den);
		return At(Q, Ac, Vc * Den);
	}

	/** A point's gap to her skin: its distance to the nearest of the triangles given, less than nothing where it lies
	 * behind that triangle's face (her file's way: Cross(B - C, A - C) points into her). */
	double GapOf(const Skin& S, const std::vector<size_t>& Near, const V3& P, bool& bFound)
	{
		double Best = 1e9;
		size_t Which = 0;
		bFound = false;
		for (const size_t K : Near)
		{
			const V3 Q = Sub(P, Nearest(S, K, P));
			const double D = Dot(Q, Q);
			if (D < Best)
			{
				Best = D;
				Which = K;
				bFound = true;
			}
		}
		if (!bFound) return 0.0;
		const V3 Into = Cross(Sub(S.B[Which], S.C[Which]), Sub(S.A[Which], S.C[Which]));
		const double Dist = std::sqrt(Best);
		return Dot(Sub(P, Nearest(S, Which, P)), Into) > 0.0 ? -Dist : Dist;
	}

	/** A glyph's ink's middle against its box's middle, in its own (right, up) as seen: a letter F leans left and up. */
	bool ReadsRight(const AirLetters::Level& L, int RightAxis, double RightSign, int UpAxis, int FaceAxis, double FaceSign, std::string& Detail)
	{
		double Area = 0.0, Cx = 0.0, Cy = 0.0;
		double Lo[2] = { 1e9, 1e9 }, Hi[2] = { -1e9, -1e9 };
		for (size_t T = 0; T + 2 < L.Triangles.size(); T += 3)
		{
			const int I[3] = { L.Triangles[T], L.Triangles[T + 1], L.Triangles[T + 2] };
			bool bFace = true;
			double X[3], Y[3];
			for (int K = 0; K < 3; ++K)
			{
				bFace = bFace && L.Facings[3 * I[K] + FaceAxis] * FaceSign > 0.7;
				X[K] = L.Points[3 * I[K] + RightAxis] * RightSign;
				Y[K] = L.Points[3 * I[K] + UpAxis];
			}
			if (!bFace) continue;
			const double A = std::fabs((X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0])) * 0.5;
			Area += A;
			Cx += A * (X[0] + X[1] + X[2]) / 3.0;
			Cy += A * (Y[0] + Y[1] + Y[2]) / 3.0;
			for (int K = 0; K < 3; ++K)
			{
				Lo[0] = std::min(Lo[0], X[K]); Hi[0] = std::max(Hi[0], X[K]);
				Lo[1] = std::min(Lo[1], Y[K]); Hi[1] = std::max(Hi[1], Y[K]);
			}
		}
		if (Area <= 0.0) return false;
		const double Dx = Cx / Area - 0.5 * (Lo[0] + Hi[0]), Dy = Cy / Area - 0.5 * (Lo[1] + Hi[1]);
		char Line[96];
		std::snprintf(Line, sizeof Line, "leans %.0f mm right, %.0f mm up", Dx * 1000.0, Dy * 1000.0);
		Detail = Line;
		return Dx < 0.0 && Dy > 0.0;
	}

	using WearMap = std::map<int, AirLetters::Wear>;

	/** A recording played through the show: the book taking its aircraft at every Every-th step from From (its aircraft
	 * all hidden where bHide), and a hash of the show's own record of every step (its aircraft and its effects). */
	struct Played
	{
		WearMap Worn;
		uint64_t ShowHash = 1469598103934665603ull;
		std::map<int, int> GroupOf;
		std::set<int> Wearing; // every key the show gave that wears them
	};
	void Mix(uint64_t& H, uint64_t V)
	{
		for (int I = 0; I < 8; ++I)
		{
			H ^= (V >> (8 * I)) & 0xFFu;
			H *= 1099511628211ull;
		}
	}
	Played Play(const AirFeed::Recording& Rec, const AirLetters::Sheet& Sheet, bool bBook, int Every, int From, bool bHide, double MostT)
	{
		Played Out;
		AirShow::Show Show(238);
		AirFeed::Apply(Rec.Board, Show);
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const AirFeed::Frame& F = Rec.Views[Taken++];
			Show.Take(F.Head.T, F.Groups, F.Events, F.Ships);
		};
		TakeNext();
		AirLetters::Book Book;
		if (bBook) Book.Begin(Sheet, Show.Groups());
		double T = Rec.Views.front().Head.T;
		const double EndT = std::min(Rec.Views.back().Head.T + 30.0, Rec.Views.front().Head.T + MostT);
		int Step = 0;
		while (T < EndT - 1e-9)
		{
			T = std::min(EndT, T + 0.25);
			while (Taken < Rec.Views.size() && Rec.Views[Taken].Head.T <= T + 1e-9) TakeNext();
			Show.Advance(T);
			Mix(Out.ShowHash, Show.Effects.size());
			Show.Effects.clear();
			for (const AirShow::PlaneOut& P : Show.Planes())
			{
				Mix(Out.ShowHash, static_cast<uint64_t>(P.Key));
				Mix(Out.ShowHash, static_cast<uint64_t>(std::llround(P.At.X * 100.0)));
				Mix(Out.ShowHash, static_cast<uint64_t>(std::llround(P.At.Y * 100.0)));
				Mix(Out.ShowHash, static_cast<uint64_t>(std::llround(P.At.Z * 100.0)));
				Mix(Out.ShowHash, (P.bShown ? 1u : 0u) | (P.bFalling ? 2u : 0u));
				if (AirLetters::Wears(P))
				{
					Out.Wearing.insert(P.Key);
					Out.GroupOf[P.Key] = P.Group;
				}
			}
			if (bBook && Step >= From && (Step - From) % Every == 0)
			{
				if (bHide)
				{
					std::vector<AirShow::PlaneOut> Hidden = Show.Planes();
					for (AirShow::PlaneOut& P : Hidden) P.bShown = false;
					Book.Take(Hidden);
				}
				else
				{
					Book.Take(Show.Planes());
				}
			}
			++Step;
		}
		if (bBook)
		{
			for (const int Key : Out.Wearing)
			{
				if (const AirLetters::Wear* W = Book.Of(Key)) Out.Worn[Key] = *W;
			}
		}
		return Out;
	}

	std::string WearText(const AirLetters::Wear& W)
	{
		return std::string(1, W.Group ? W.Group : '-') + " " + (W.Code.empty() ? "--" : W.Code) + " " + std::string(1, W.Letter ? W.Letter : '-');
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::printf("usage: air_letters_test <b17-fine-marks.json> <Content/Models> <Source/TheaterCommand> [recording.json ...]\n");
		return 2;
	}
	const std::string MarksPath = Argv[1], Models = Argv[2], Src = Argv[3];
	std::string Text, Why;
	char Line[256];

	std::printf("The markings' sheet (%s)\n", MarksPath.c_str());
	AirLetters::Sheet Sheet;
	const bool bRead = ReadText(MarksPath, Text) && Sheet.Read(Text, Why);
	Check(bRead, "read as the game reads it, whole for its bundle", Why);
	if (!bRead)
	{
		std::printf("%d failed\n", Bad);
		return 1;
	}
	std::snprintf(Line, sizeof Line, "bundle %d, %zu parts, %.2f MB", Sheet.Bundle, Sheet.Parts.size(), Text.size() / 1.0e6);
	Check(Sheet.Bundle >= 1 && Text.size() < 8u * 1000u * 1000u, "its bundle and size", Line);
	std::string ModelText;
	Check(ReadText(Models + "/B17Fine.json", ModelText), "her file read");
	const std::string Sha = Sha256(ModelText);
	Check(Sha == Sheet.ModelSha && Sha.compare(0, 12, "5e1f39002423") == 0, "made on the finer B-17 the game holds", Sha.substr(0, 12));
	Check(Sheet.InkOf("white") == 0xDCD9CCu && Sheet.InkOf("black") == 0x17171Au, "her group's white and its letter's black, the approved picture's", "");
	if (Sheet.Bundle >= 2)
	{
		Check(Sheet.InkOf("yellow") == 0xC99A35u && Sheet.InkOf("code") == 0xB9BAB4u, "her letter's yellow and her codes' light grey, the approved picture's");
	}

	AirFeed::Json Model;
	Check(AirFeed::Parse(ModelText, Model, Why), "her file parsed", Why);
	Skin Skins[2];
	Check(ReadSkin(Model["near"], Skins[0]) && ReadSkin(Model["far"], Skins[1]), "her near and far skins read");
	ModelText.clear();

	// Wound as her file's, and clear of her skin.
	int Tris = 0, Wrong = 0, Points = 0, Out = 0, Missed = 0, DecalPoints = 0, DecalOut = 0, DecalWide = 0;
	double Least = 1e9, Most = -1e9, DecalLeast = 1e9, DecalMost = -1e9;
	std::string Worst;
	for (const auto& [Name, Part] : Sheet.Parts)
	{
		for (int Lv = 0; Lv < 2; ++Lv)
		{
			const AirLetters::Level& L = Lv == 0 ? Part.Near : Part.Far;
			if (L.Empty()) continue;
			for (size_t T = 0; T + 2 < L.Triangles.size(); T += 3)
			{
				const V3 A = PointOf(L.Points, L.Triangles[T]), B = PointOf(L.Points, L.Triangles[T + 1]), C = PointOf(L.Points, L.Triangles[T + 2]);
				const V3 F = Cross(Sub(B, C), Sub(A, C));
				V3 N;
				for (int K = 0; K < 3; ++K)
				{
					const V3 Fk = PointOf(L.Facings, L.Triangles[T + K]);
					N = { N.X + Fk.X, N.Y + Fk.Y, N.Z + Fk.Z };
				}
				++Tris;
				// (her own file's way: Cross(B - C, A - C) into her; a sliver too thin to face either way not counted)
				if (Dot(F, N) > 1e-9) ++Wrong;
			}
			const bool bWing = Name.compare(0, 5, "wing.") == 0;
			// (a side letter or code at her far level is laid as a decal seen straight from her side, the tool's decal_side:
			// measured straight in from her side, as it was laid; her far skin's thin edges make the nearest face no measure)
			const bool bSideDecal = Lv == 1 && (Name.compare(0, 12, "letter.side.") == 0 || Name.compare(0, 5, "code.") == 0);
			V3 Lo{ 1e9, 1e9, 1e9 }, Hi{ -1e9, -1e9, -1e9 };
			const int Count = static_cast<int>(L.Points.size() / 3);
			for (int I = 0; I < Count; ++I)
			{
				const V3 P = PointOf(L.Points, I);
				Lo = { std::min(Lo.X, P.X), std::min(Lo.Y, P.Y), std::min(Lo.Z, P.Z) };
				Hi = { std::max(Hi.X, P.X), std::max(Hi.Y, P.Y), std::max(Hi.Z, P.Z) };
			}
			const Skin& S = Skins[Lv];
			const std::vector<size_t> Near = Within(S, Lo, Hi, bWing ? 0.6 : 0.25);
			for (int I = 0; I < Count; I += 5)
			{
				const V3 P = PointOf(L.Points, I);
				// (the wing's straight down, as the tool laid it; the rest its distance to her skin, her nearest face)
				double Gap = 0.0;
				bool bFound = false;
				if (bWing || bSideDecal)
				{
					const V3 Way = bWing ? V3{ 0.0, 0.0, 1.0 } : V3{ 0.0, P.Y > 0.0 ? 1.0 : -1.0, 0.0 };
					double Best = 1e9;
					for (const size_t K : Near)
					{
						double T = 0.0;
						if (Meet(S, K, P, Way, T) && std::fabs(T) < std::fabs(Best) && std::fabs(T) < 0.2) Best = T;
					}
					bFound = Best < 1e8;
					Gap = -Best;
				}
				else
				{
					Gap = GapOf(S, Near, P, bFound);
				}
				++Points;
				if (!bFound)
				{
					++Missed;
					continue;
				}
				if (bSideDecal)
				{
					// (her far side's decals: never under her; off her by at most 40 mm but where a tall letter's tail or
					// top reaches round her curve, which a decal seen straight from her side stands off, at most 125 mm)
					++DecalPoints;
					DecalLeast = std::min(DecalLeast, Gap);
					DecalMost = std::max(DecalMost, Gap);
					if (Gap > 0.040) ++DecalWide;
					if (Gap < 0.002 || Gap > 0.125) ++DecalOut;
					continue;
				}
				Least = std::min(Least, Gap);
				Most = std::max(Most, Gap);
				if (Gap < 0.003 || Gap > 0.040)
				{
					++Out;
					if (Worst.empty())
					{
						std::snprintf(Line, sizeof Line, "%s %s point %d: %.1f mm", Name.c_str(), Lv == 0 ? "near" : "far", I, Gap * 1000.0);
						Worst = Line;
					}
				}
			}
		}
	}
	std::snprintf(Line, sizeof Line, "%d of %d triangles the other way", Wrong, Tris);
	// (the livery's own glyphs carry a handful wound the other way, as her file does: 0.0 of a hundredth there)
	Check(Wrong * 1000 <= Tris, "wound as her own file's (all but a thousandth)", Line);
	std::snprintf(Line, sizeof Line, "%d points tried, %d off, %d found no skin within 20 cm; %.1f to %.1f mm %s", Points, Out, Missed, Least * 1000.0, Most * 1000.0, Worst.c_str());
	Check(Out == 0 && Missed * 50 <= Points, "every fifth point 3 to 40 mm off its own level's skin (the nearest of her; the wing's straight down)", Line);
	std::snprintf(Line, sizeof Line, "%d points tried, %d off, %d over 40 mm; %.1f to %.1f mm", DecalPoints, DecalOut, DecalWide, DecalLeast * 1000.0, DecalMost * 1000.0);
	Check(Sheet.Bundle < 2 || (DecalPoints > 0 && DecalOut == 0 && DecalWide * 100 <= DecalPoints),
		"her far side's letters and codes, every fifth point, straight in from her side: never under her, 2 to 125 mm off, over 40 mm one in a hundred at most", Line);

	// Reading the right way round, and the wing's triangle apex forward.
	for (int Lv = 0; Lv < 2; ++Lv)
	{
		const bool bNear = Lv == 0;
		const AirLetters::Level* Fin = Sheet.LevelOf("fin.T.F", bNear);
		const AirLetters::Level* Wing = Sheet.LevelOf("wing.T.F", bNear);
		std::string D1, D2, D3;
		// (her starboard face seen from her right: right is forward, +x; her port face from her left: right is aft;
		// her wing from above, her nose up the picture: right is +y, her right hand, and up is forward)
		const bool bS = Fin && ReadsRight(*Fin, 0, 1.0, 2, 1, 1.0, D1);
		const bool bP = Fin && ReadsRight(*Fin, 0, -1.0, 2, 1, -1.0, D2);
		const bool bW = Wing && ReadsRight(*Wing, 1, 1.0, 0, 2, 1.0, D3);
		Check(bS && bP && bW, bNear ? "near: the letter F reads the right way round on both faces of her fin and on her wing from above" : "far: the letter F reads the right way round on both faces of her fin and on her wing from above",
			D1 + "; " + D2 + "; " + D3);
		if (Sheet.Bundle >= 2)
		{
			// (her own letter on her fin and side, both faces, and her squadron's codes, each on its own side)
			std::string Said;
			bool bAll = true;
			const char* Faces[][2] = { { "letter.fin.F", "both" }, { "letter.side.F", "both" }, { "code.port.1.F", "port" }, { "code.port.2.F", "port" },
				{ "code.starboard.1.F", "starboard" }, { "code.starboard.2.F", "starboard" } };
			for (const auto& Face : Faces)
			{
				const AirLetters::Level* L = Sheet.LevelOf(Face[0], bNear);
				std::string D;
				const bool bStarboard = std::strcmp(Face[1], "port") != 0;
				const bool bPort = std::strcmp(Face[1], "starboard") != 0;
				const bool bOk = L && (!bStarboard || ReadsRight(*L, 0, 1.0, 2, 1, 1.0, D)) && (!bPort || ReadsRight(*L, 0, -1.0, 2, 1, -1.0, D));
				if (!bOk)
				{
					bAll = false;
					Said += std::string(Face[0]) + " " + D + "; ";
				}
			}
			Check(bAll, bNear ? "near: the letter F reads the right way round as her own letter and in her codes, each side" : "far: the letter F reads the right way round as her own letter and in her codes, each side", Said);
		}
		const AirLetters::Level* Tri = Sheet.LevelOf("wing.T", bNear);
		if (Tri)
		{
			double X0 = 1e9, X1 = -1e9;
			for (size_t I = 0; I < Tri->Points.size(); I += 3) { X0 = std::min(X0, Tri->Points[I]); X1 = std::max(X1, Tri->Points[I]); }
			double WideFore = 0.0, WideAft = 0.0;
			double Y[2][2] = { { 1e9, -1e9 }, { 1e9, -1e9 } };
			for (size_t I = 0; I < Tri->Points.size(); I += 3)
			{
				const double X = Tri->Points[I], Yv = Tri->Points[I + 1];
				const int Band = X > X1 - 0.1 * (X1 - X0) ? 0 : X < X0 + 0.1 * (X1 - X0) ? 1 : -1;
				if (Band < 0) continue;
				Y[Band][0] = std::min(Y[Band][0], Yv);
				Y[Band][1] = std::max(Y[Band][1], Yv);
			}
			WideFore = Y[0][1] - Y[0][0];
			WideAft = Y[1][1] - Y[1][0];
			std::snprintf(Line, sizeof Line, "%.2f m wide at its forward tenth, %.2f at its aft; y %.2f to %.2f", WideFore, WideAft, Y[1][0], Y[1][1]);
			Check(WideFore < 0.25 * WideAft && Y[1][0] > 0.5, bNear ? "near: the wing's triangle on her right wing, apex forward" : "far: the wing's triangle on her right wing, apex forward", Line);
		}
	}

	// Nothing laid over her star and bar (her file's marks: a side's 4 radii long, a wing's on its own face).
	{
		int Crossed = 0;
		std::string First;
		for (const AirFeed::Json& M : Model["marks"].Items)
		{
			if (M["tag"].Str("") != "USA-BAR") continue;
			const double Mx = M["at"].At(0).Num(), My = M["at"].At(1).Num(), R = M["radius"].Num();
			const double Fz = M["facing"].At(2).Num();
			const bool bBoth = M["both"].Truth(false);
			for (const auto& [Name, Part] : Sheet.Parts)
			{
				const AirLetters::Level& L = Part.Far.Empty() ? Part.Near : Part.Far;
				for (size_t I = 0; I < L.Points.size(); I += 3)
				{
					const double X = L.Points[I], Y = L.Points[I + 1], Fy = L.Facings[I + 1], Fzz = L.Facings[I + 2];
					bool bOn = false;
					if (std::fabs(Fz) > 0.7)
					{
						// (a wing's: the same face of the same wing, within the bars' reach across the span)
						bOn = Fzz * Fz > 0.5 && std::fabs(Y - My) < 2.0 * R && std::fabs(X - Mx) < R;
					}
					else
					{
						// (a side's: either side where it is worn on both, along her within 2 radii of its middle)
						const bool bSide = bBoth ? std::fabs(Fy) > 0.5 : Fy * M["facing"].At(1).Num() > 0.5;
						bOn = bSide && std::fabs(X - Mx) < 2.0 * R && std::fabs(L.Points[I + 2] - M["at"].At(2).Num()) < R;
					}
					if (bOn)
					{
						++Crossed;
						if (First.empty()) First = Name;
					}
				}
			}
		}
		Check(Crossed == 0, "nothing laid over her star and bar", First);
	}
	Skins[0] = Skin();
	Skins[1] = Skin();

	// Who wears which letters.
	for (int A = 4; A < Argc; ++A)
	{
		AirFeed::Recording Rec;
		if (!ReadText(Argv[A], Text) || !AirFeed::ReadRecording(Text, Rec, Why) || Rec.Views.empty())
		{
			Check(false, "a recording read", std::string(Argv[A]) + ": " + Why);
			continue;
		}
		std::printf("%s\n", Rec.Key.c_str());
		const double MostT = 600.0;
		const Played Each = Play(Rec, Sheet, true, 1, 0, false, MostT);
		const Played Third = Play(Rec, Sheet, true, 3, 0, false, MostT);
		const Played Seventh = Play(Rec, Sheet, true, 7, 5, false, MostT);
		const Played Hidden = Play(Rec, Sheet, true, 1, 0, true, MostT);
		const Played Bare = Play(Rec, Sheet, false, 1, 0, false, MostT);
		std::snprintf(Line, sizeof Line, "%zu American B-17s; %zu given at every step, %zu every third, %zu every seventh from the fifth, %zu all hidden",
			Each.Wearing.size(), Each.Worn.size(), Third.Worn.size(), Seventh.Worn.size(), Hidden.Worn.size());
		Check(Each.Worn == Third.Worn && Each.Worn == Seventh.Worn && Each.Worn == Hidden.Worn, "the same wears at any cadence, shown or not", Line);
		Check(Each.Worn.size() == Each.Wearing.size(), "every American B-17 the show gave wears hers", Line);
		Check(Each.ShowHash == Bare.ShowHash, "the show's own record the same with the book taking and without");

		// One letter a group, none shared; codes not shared between groups; (code, letter) never twice in a group.
		std::map<int, char> LetterOfGroup;
		std::map<int, std::set<std::string>> CodesOfGroup;
		std::map<int, std::set<std::string>> PairsOfGroup;
		bool bOneLetter = true, bPairOnce = true;
		for (const auto& [Key, W] : Each.Worn)
		{
			const int G = Each.GroupOf.at(Key);
			const auto Had = LetterOfGroup.find(G);
			if (Had != LetterOfGroup.end() && Had->second != W.Group) bOneLetter = false;
			LetterOfGroup[G] = W.Group;
			CodesOfGroup[G].insert(W.Code);
			bPairOnce = PairsOfGroup[G].insert(W.Code + W.Letter).second && bPairOnce;
		}
		std::set<char> Letters;
		std::set<std::string> Codes;
		bool bCodesApart = true;
		for (const auto& [G, L] : LetterOfGroup)
		{
			Letters.insert(L);
			for (const std::string& C : CodesOfGroup[G]) bCodesApart = Codes.insert(C).second && bCodesApart;
		}
		std::snprintf(Line, sizeof Line, "%zu groups, %zu letters", LetterOfGroup.size(), Letters.size());
		Check(bOneLetter && Letters.size() == LetterOfGroup.size(), "one letter a group, none shared", Line);
		Check(bCodesApart, "no squadron's code in two groups");
		bool bFew = true;
		for (const auto& [G, Pairs] : PairsOfGroup) bFew = bFew && Pairs.size() <= 96;
		Check(bPairOnce || !bFew, "no two of a group with the same code and letter (to 96 a group)");
		for (const auto& [G, L] : LetterOfGroup)
		{
			int Lowest = -1;
			for (const auto& [Key, W] : Each.Worn)
			{
				if (Each.GroupOf.at(Key) == G && (Lowest < 0 || Key < Lowest)) Lowest = Key;
			}
			std::printf("    group %d: %c, codes", G, L);
			for (const std::string& C : CodesOfGroup[G]) std::printf(" %s", C.c_str());
			std::printf("; its first: %s\n", Lowest >= 0 ? WearText(Each.Worn.at(Lowest)).c_str() : "-");
		}
		// The 303rd, as history painted her.
		AirShow::Show Probe(238);
		AirFeed::Apply(Rec.Board, Probe);
		Probe.Take(Rec.Views.front().Head.T, Rec.Views.front().Groups, Rec.Views.front().Events, Rec.Views.front().Ships);
		for (const auto& [Id, G] : Probe.Groups())
		{
			if (G.Name != "303rd Bomb Group" || !AirLetters::GroupWears(G)) continue;
			int Lowest = -1;
			for (const auto& [Key, W] : Each.Worn)
			{
				if (Each.GroupOf.at(Key) == Id && (Lowest < 0 || Key < Lowest)) Lowest = Key;
			}
			const std::string Got = Lowest >= 0 ? WearText(Each.Worn.at(Lowest)) : std::string("none");
			Check(Got == "C VK A", "the 303rd Bomb Group's first bomber wears C, VK and A", Got);
		}
		// Never by date.
		for (const int When : { 194303, 194406 })
		{
			AirFeed::Recording Moved = Rec;
			Moved.Board.Year = When / 100;
			Moved.Board.Month = When % 100;
			const Played Again = Play(Moved, Sheet, true, 1, 0, false, 120.0);
			const Played Short = Play(Rec, Sheet, true, 1, 0, false, 120.0);
			std::snprintf(Line, sizeof Line, "%d-%02d", When / 100, When % 100);
			Check(Again.Worn == Short.Worn, "the same wears with the battle's date moved", Line);
		}
	}

	// Thirty made-up American B-17 groups (more than 24 letters), the 303rd among them, numbered two ways: the same
	// letters by name whichever way, the 303rd's C, the first 24 groups' letters all different, no code in two groups, and
	// no two of a group's 30 bombers with the same code and letter.
	{
		std::map<std::string, char> ByName[2];
		std::map<std::string, std::vector<std::string>> CodesByName;
		bool bPairsOnce = true;
		for (int Way = 0; Way < 2; ++Way)
		{
			std::map<int, AirShow::Group> Groups;
			std::vector<AirShow::PlaneOut> Planes;
			for (int K = 0; K < 30; ++K)
			{
				AirShow::Group G;
				G.Id = Way == 0 ? K + 1 : 30 - K;
				G.Name = K == 7 ? std::string("303rd Bomb Group") : std::to_string(400 + K) + "th Bomb Group";
				G.bFighter = false;
				G.Nation = "USA";
				G.Model = K % 2 == 0 ? "B17" : "B-17";
				Groups[G.Id] = G;
				for (int N = 0; N < 30; ++N)
				{
					AirShow::PlaneOut P;
					P.Key = 1000 * G.Id + N;
					P.Group = G.Id;
					P.bFighter = false;
					P.Nation = "USA";
					P.Model = G.Model;
					Planes.push_back(P);
				}
			}
			// (and a German group and an American fighter group, which wear none)
			AirShow::Group Other;
			Other.Id = 99;
			Other.Name = "Jagdgeschwader 1";
			Other.Nation = "GER";
			Groups[99] = Other;
			AirLetters::Book Book;
			Book.Begin(Sheet, Groups);
			std::reverse(Planes.begin(), Planes.end());
			Book.Take(Planes);
			for (const auto& [Id, G] : Groups)
			{
				ByName[Way][G.Name] = Book.LetterOf(Id);
				if (Way == 0) CodesByName[G.Name] = Book.CodesOf(Id);
				std::set<std::string> Pairs;
				for (int N = 0; N < 30 && Id != 99; ++N)
				{
					const AirLetters::Wear* W = Book.Of(1000 * Id + N);
					bPairsOnce = W && Pairs.insert(W->Code + W->Letter).second && bPairsOnce;
				}
			}
		}
		Check(ByName[0] == ByName[1], "the same letters by name however the battle numbers its groups");
		Check(ByName[0]["303rd Bomb Group"] == 'C' && ByName[0]["Jagdgeschwader 1"] == 0, "the 303rd's C; a German group none");
		std::set<char> Distinct;
		for (const auto& [Name, L] : ByName[0])
		{
			if (L != 0) Distinct.insert(L);
		}
		std::set<std::string> Codes;
		bool bApart = true;
		for (const auto& [Name, Cs] : CodesByName)
		{
			for (const std::string& C : Cs) bApart = Codes.insert(C).second && bApart;
		}
		std::snprintf(Line, sizeof Line, "%zu letters among 30 groups", Distinct.size());
		Check(Distinct.size() == 24, "every one of the 24 letters given before any is given twice", Line);
		Check(bApart && Codes.size() == 120, "no squadron's code in two groups (120 codes)");
		Check(bPairsOnce, "no two of a group's 30 bombers with the same code and letter");
	}

	// No word of the date in the book.
	for (const char* File : { "/Battle/AirLetters.h", "/Battle/AirLetters.cpp" })
	{
		std::string Src_;
		const bool bHave = ReadText(Src + File, Src_);
		std::string Lower = Src_;
		std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
		Check(bHave && Lower.find("month") == std::string::npos && Lower.find("year") == std::string::npos, "no month or year in it", File);
	}

	// The Unreal side from its source. These count lines of text: they show the guards are there, not how they behave,
	// and a harmless reformat breaks them (then mend the text here). The proof is the Mac's run with the row off against
	// the base: the same log, the same pictures.
	{
		auto Count = [](const std::string& In, const std::string& What)
		{
			int N = 0;
			for (size_t At = In.find(What); At != std::string::npos; At = In.find(What, At + What.size())) ++N;
			return N;
		};
		std::string Battle, Rules, State, Scene, Book;
		const bool bSource = ReadText(Src + "/Battle/AirBattle.cpp", Battle) && ReadText(Src + "/Battle/LandLive.cpp", Rules)
			&& ReadText(Src + "/Battle/AirBattleState.h", State) && ReadText(Src + "/Battle/AirBattleLetters.cpp", Scene)
			&& ReadText(Src + "/Battle/AirLetters.cpp", Book);
		Check(bSource, "the source read");
		if (bSource)
		{
			Check(Count(Rules, "{ TEXT(\"flyBomberMarks\"), TEXT(\"Bomber markings\")") == 1 && Count(Rules, "TEXT(\"TheaterAirNoBomberMarks\"), false, TEXT(\"TheaterAirBomberMarks\") }") == 1,
				"the row on the card, off unless asked (-TheaterAirBomberMarks), its key beginning \"fly\"");
			Check(Count(Scene, "FLandLive::RuleNow(TEXT(\"flyBomberMarks\"))") == 1 && Count(Battle, "flyBomberMarks\"))") == 0 && Count(State, "int LettersOn = -1;") == 1,
				"read once a battle, in AirBattleLetters.cpp alone, into the battle's own state");
			Check(Count(Scene, "S.LettersOn = S.B17FineOn == 1 && FLandLive::RuleNow(") == 1, "read only while the finer B-17 flies");
			// (every line of AirBattle.cpp that places, takes or makes a mark guarded by the row)
			int Lines = 0, Guarded = 0;
			size_t From = 0;
			while (From < Battle.size())
			{
				const size_t End = std::min(Battle.find('\n', From), Battle.size());
				const std::string Line_ = Battle.substr(From, End - From);
				if (Line_.find("AddMarks(") != std::string::npos || Line_.find("S.Letters.") != std::string::npos || Line_.find("WarmMarks()") != std::string::npos)
				{
					++Lines;
					Guarded += Line_.find("LettersOn == 1") != std::string::npos ? 1 : 0;
				}
				From = End + 1;
			}
			std::snprintf(Line, sizeof Line, "%d of %d lines", Guarded, Lines);
			Check(Lines == 4 && Guarded == Lines, "in AirBattle.cpp every mark placed, taken or made on a line guarded by the row", Line);
			Check(Count(Battle, "const double MarksPpm = LettersOnNow() ? MarksPixelsPerMetre(Lens) : 0.0;") == 1
					&& Count(Battle, "if (!State.IsValid() || State->LettersOn != 1 || !ShapeFrameOf(Shape, Kind, Middle, Fit))") == 1,
				"the frame's pixels reckoned and a mark mesh made only with the row on");
			std::string Miniatures;
			const bool bMini = ReadText(Src + "/Campaign/CampaignMiniatures.cpp", Miniatures);
			Check(bMini && Count(Battle, "FMiniatureMesh::bNoPlaneNumber = bLetters;") == 1 && Count(Battle, "FMiniatureMesh::bNoPlaneNumber = false;") == 1
					&& Count(Battle, "FMiniatureMesh::bNoPlaneNumber") == 2 && Count(Battle, "&& LettersOnNow() && State->LettersSheet.Bundle >= 2;") == 1
					&& Count(Battle, "+ (bLetters ? TEXT(\"|letters\") : TEXT(\"\"))") == 1
					&& Count(Miniatures, "if (!bNoPlaneNumber)") == 1 && Count(Miniatures, "+ (bNoPlaneNumber ? TEXT(\"|nonumber\") : TEXT(\"\"));") == 1,
				"her \"#\" number dropped only for a finer B-17 shape made with the row on at bundle 2, under a key of its own (off: the keys of before)");
			Check(Count(Book, "PlaneMade") == 0 && Count(Book, "Random") == 0 && Count(Book, "Seed") == 0,
				"the book never reads the scene's aircraft or a random draw");
		}
	}

	if (Bad == 0) std::printf("air letters: all good\n");
	else std::printf("air letters: %d failed\n", Bad);
	return Bad == 0 ? 0 : 1;
}
