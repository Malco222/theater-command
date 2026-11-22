// The men's crowd's poses' picture (SO1's draw: Battle/LandVatPack.h, what Battle/LandVat hands M_LandCrowd), checked
// against the bank it is packed from, every body set at mid and far, every pose the file has, every point:
//   - the layout: a column a point, two rows a pose (the way from rest, then the facing), the rest the first pose the
//     file has;
//   - the material's lookup: each point's number as the half float its mesh carries, read at ((n + 0.5) / width,
//     (row + 0.5) / height) with the nearest texel in 32-bit floats, lands on its own column and row;
//   - the halfs' rounding: rest + the packed way against the bank's point (mm, and lines of the usual 50 deg lens's
//     665-line picture at the nearest mid man, GS3's 8.6 m), and the packed facing against the bank's (degrees);
//   - the bounds: every pose's points inside the rest's box grown as LandVat grows the mesh's.
// Usage: land_vat_pack_test <recording.json> <soldiers folder> <armies.json> <pose_bank.json>
#include "land_json.h"
#include "Battle/LandMen.h"
#include "Battle/LandVatPack.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace LandMen;

namespace
{
	std::string ReadFile(const std::string& Path)
	{
		std::ifstream F(Path, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	struct Field
	{
		int Cols = 1, Rows = 1;
		double Cell = 25.0;
		std::vector<float> H;
		float At(int C, int R) const
		{
			C = std::max(0, std::min(Cols - 1, C));
			R = std::max(0, std::min(Rows - 1, R));
			return H[static_cast<size_t>(R) * static_cast<size_t>(Cols) + static_cast<size_t>(C)];
		}
		Ground operator()(double X, double Y) const
		{
			const double FX = X / Cell - 0.5, FY = Y / Cell - 0.5;
			const int C0 = static_cast<int>(std::floor(FX)), R0 = static_cast<int>(std::floor(FY));
			const double U = FX - C0, V = FY - R0;
			const double H00 = At(C0, R0), H10 = At(C0 + 1, R0), H01 = At(C0, R0 + 1), H11 = At(C0 + 1, R0 + 1);
			Ground G;
			G.Height = static_cast<float>(H00 * (1 - U) * (1 - V) + H10 * U * (1 - V) + H01 * (1 - U) * V + H11 * U * V);
			G.Normal = { 0.f, 0.f, 1.f };
			return G;
		}
	};
}

int main(int Argc, char** Argv)
{
	if (Argc < 5)
	{
		std::fprintf(stderr, "usage: land_vat_pack_test <recording.json> <soldiers folder> <armies.json> <pose_bank.json>\n");
		return 2;
	}
	int Fails = 0;
	auto Fail = [&](const char* Fmt, auto... Args) {
		++Fails;
		if (Fails <= 40)
		{
			std::printf("  FAIL: ");
			if constexpr (sizeof...(Args) == 0) std::fputs(Fmt, stdout);
			else std::printf(Fmt, Args...);
			std::printf("\n");
		}
	};
	const std::string RecText = ReadFile(Argv[1]);
	LandJson::Value Rec;
	std::string Err;
	if (RecText.empty() || !LandJson::Parse(RecText, Rec)) { std::printf("cannot read %s\n", Argv[1]); return 1; }
	Crowd V;
	V.SetLife(2.0);
	for (const char* Army : { "GER", "POL", "USA", "USA_bar", "USA_nco", "USA_bazooka", "USA_loader", "USA_radio", "SOV" })
	{
		const std::string Path = std::string(Argv[2]) + "/soldier_" + Army + ".json";
		const std::string Text = ReadFile(Path);
		if (Text.empty() && (std::string(Army).rfind("USA", 0) == 0 || std::string(Army) == "SOV")) continue;
		auto S = std::make_shared<Soldier>();
		if (Text.empty() || !S->Load(Text, Err)) { std::printf("cannot read %s: %s\n", Path.c_str(), Err.c_str()); return 1; }
		V.AddSoldier(Army, S);
	}
	if (!V.ReadArmies(ReadFile(Argv[3]), Err)) { std::printf("cannot read %s: %s\n", Argv[3], Err.c_str()); return 1; }
	if (!V.ReadPoseBank(ReadFile(Argv[4]), Err)) { std::printf("cannot read the pose bank %s: %s\n", Argv[4], Err.c_str()); return 1; }
	Field Fd;
	{
		const LandJson::Value& F = Rec["field"];
		Fd.Cols = F["cols"].Int(120);
		Fd.Rows = F["rows"].Int(80);
		Fd.Cell = F["cell"].Num(25.0);
		for (size_t I = 0; I < F["height"].Count(); ++I) Fd.H.push_back(static_cast<float>(F["height"].At(I).Num()));
		if (Fd.H.size() != static_cast<size_t>(Fd.Cols) * static_cast<size_t>(Fd.Rows)) { std::printf("field heights do not fit its cells\n"); return 1; }
	}
	if (!V.Build(RecText, [&Fd](double X, double Y) { return Fd(X, Y); }, Err)) { std::printf("cannot build the crowd: %s\n", Err.c_str()); return 1; }
	V.SetPoseBankMid(true);
	const int Poses = V.BankPoses();
	std::set<int> Keys;
	for (int I = 0; I < V.Count(); ++I) if (V.BodyKey(I) >= 0) Keys.insert(V.BodyKey(I));
	if (Keys.empty()) Fail("no body sets");
	const double U = 100.0;
	// (lines a metre at the nearest mid man, GS3: a 1.8 m man over 1283.5 / D lines at D metres)
	const double Lines = 1.8 * 665.0 / (2.0 * std::tan(25.0 * 3.14159265358979 / 180.0));
	const double MidFrom = Lines / 150.0;
	const double LinesPerM = Lines / 1.8 / MidFrom;
	double WorstMm = 0.0, WorstDeg = 0.0, Bytes = 0.0;
	long Points = 0, Missed = 0, OutOfBox = 0;
	for (const int Key : Keys)
	{
		for (const Lod Lv : { Mid, Far })
		{
			std::vector<float> At, Fa;
			const int Pts = V.BankBodyPoints(Key, Lv);
			if (!V.BankBody(Key, Lv, At, Fa)) { Fail("set %d has no %s buffer", Key, Lv == Mid ? "mid" : "far"); continue; }
			std::vector<bool> Has(static_cast<size_t>(Poses));
			int First = -1;
			for (int K = 0; K < Poses; ++K)
			{
				Has[static_cast<size_t>(K)] = V.BankHas(Key, K);
				if (Has[static_cast<size_t>(K)] && First < 0) First = K;
			}
			LandVatPack::Packed P;
			if (!LandVatPack::Pack(At, Fa, Has, Pts, Poses, U, P)) { Fail("set %d %s: not packed (%d points)", Key, Lv == Mid ? "mid" : "far", Pts); continue; }
			if (P.Width != Pts || P.Height != 2 * Poses || P.Rest != First || P.Texels.size() != static_cast<size_t>(Pts) * static_cast<size_t>(2 * Poses) * 4)
				Fail("set %d %s: picture %dx%d, rest %d (points %d, poses %d, first %d)", Key, Lv == Mid ? "mid" : "far", P.Width, P.Height, P.Rest, Pts, Poses, First);
			Bytes += static_cast<double>(P.Texels.size()) * 2.0;
			const size_t Each = static_cast<size_t>(Pts) * 3;
			const float* Rest = At.data() + static_cast<size_t>(P.Rest) * Each;
			float Lo[3], Hi[3];
			for (int A = 0; A < 3; ++A)
			{
				Lo[A] = P.RestLo[A] - std::max(0.f, P.RestLo[A] - P.AllLo[A]) - 1.f;
				Hi[A] = P.RestHi[A] + std::max(0.f, P.AllHi[A] - P.RestHi[A]) + 1.f;
			}
			for (int K = 0; K < Poses; ++K)
			{
				if (!Has[static_cast<size_t>(K)]) continue;
				for (int Pt = 0; Pt < Pts; ++Pt)
				{
					int Col = -1, Row = -1;
					const uint16_t Number = LandVatPack::Half(static_cast<float>(Pt));
					if (LandVatPack::FromHalf(Number) != static_cast<float>(Pt)) { ++Missed; continue; }
					if (!LandVatPack::Read(P, Number, static_cast<float>(K), Col, Row) || Col != Pt || Row != K) { ++Missed; continue; }
					int Col2 = -1, Row2 = -1;
					if (!LandVatPack::Read(P, Number, static_cast<float>(Poses + K), Col2, Row2) || Col2 != Pt || Row2 != Poses + K) { ++Missed; continue; }
					const size_t T = (static_cast<size_t>(Row) * static_cast<size_t>(P.Width) + static_cast<size_t>(Col)) * 4;
					const size_t TF = (static_cast<size_t>(Row2) * static_cast<size_t>(P.Width) + static_cast<size_t>(Col2)) * 4;
					const size_t Q = static_cast<size_t>(K) * Each + static_cast<size_t>(Pt) * 3;
					double E2 = 0.0, Dot = 0.0, Ln = 0.0, Lf = 0.0;
					for (int A = 0; A < 3; ++A)
					{
						const double Drawn = static_cast<double>(Rest[static_cast<size_t>(Pt) * 3 + static_cast<size_t>(A)]) * U + LandVatPack::FromHalf(P.Texels[T + static_cast<size_t>(A)]);
						const double Truth = static_cast<double>(At[Q + static_cast<size_t>(A)]) * U;
						E2 += (Drawn - Truth) * (Drawn - Truth);
						if (Truth < Lo[A] || Truth > Hi[A]) ++OutOfBox;
						const double F = LandVatPack::FromHalf(P.Texels[TF + static_cast<size_t>(A)]);
						Dot += F * Fa[Q + static_cast<size_t>(A)];
						Ln += F * F;
						Lf += static_cast<double>(Fa[Q + static_cast<size_t>(A)]) * Fa[Q + static_cast<size_t>(A)];
					}
					WorstMm = std::max(WorstMm, std::sqrt(E2) / U * 1000.0);
					if (Lf > 1e-12)
					{
						const double C = std::max(-1.0, std::min(1.0, Dot / std::sqrt(std::max(1e-12, Ln * Lf))));
						WorstDeg = std::max(WorstDeg, std::acos(C) * 180.0 / 3.14159265358979);
					}
					++Points;
				}
			}
		}
	}
	std::printf("%s: %zu body sets at mid and far, %d poses; %ld points checked; pictures %.1f MB as halfs; worst point %.2f mm (%.3f lines at %.1f m), worst facing %.2f deg; lookups missed %ld, outside the bounds %ld\n",
		Argv[1], Keys.size(), Poses, Points, Bytes / 1048576.0, WorstMm, WorstMm / 1000.0 * LinesPerM, MidFrom, WorstDeg, Missed, OutOfBox);
	if (Missed > 0) Fail("%ld points' lookups land on another texel", Missed);
	if (OutOfBox > 0) Fail("%ld points outside the grown bounds", OutOfBox);
	if (WorstMm > 2.0) Fail("a point is %.2f mm off (over 2)", WorstMm);
	if (WorstDeg > 0.5) Fail("a facing is %.2f deg off (over 0.5)", WorstDeg);
	if (Points == 0) Fail("nothing checked");
	// The half floats themselves, both ways, against the plain cases and every whole number a point may be.
	for (const float X : { 0.f, 1.f, -1.f, 0.5f, 2047.f, 2048.f, 65504.f, 1e-5f, -3.25f })
	{
		const float Y = LandVatPack::FromHalf(LandVatPack::Half(X));
		if (std::fabs(Y - X) > std::fabs(X) * 1e-3f + 1e-7f) Fail("half of %g came back %g", static_cast<double>(X), static_cast<double>(Y));
	}
	for (int N = 0; N <= LandVatPack::MostPoints; ++N)
		if (LandVatPack::FromHalf(LandVatPack::Half(static_cast<float>(N))) != static_cast<float>(N)) { Fail("point number %d is not a whole half", N); break; }
	std::printf(Fails ? "FAILED (%d)\n" : "ALL PASSED\n", Fails);
	return Fails ? 1 : 0;
}
