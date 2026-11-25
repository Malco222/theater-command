// Bullet hits' pictures for Brian and the art director, painted from Battle/LandMen's own skinning (the crowd as the
// scene plays it, its looks and colours) by a small orthographic painter: a depth buffer, one light, flat faces. Not
// the game's renderer.
//   strip_<case>.ppm   one German rifleman killed by a rifle round (from the front, from his left, running), at 0,
//                      0.05, 0.1, 0.2, 0.4 and 1.0 s of life after the round, seen square to the shot: hits off (today)
//                      and the jolt on (the new), each panel <case>_<off|on>_<k>.ppm
//   dense_<off|on>.ppm a dense moment of a recorded battle seen from above at a slant, hits off and every part on,
//                      with dense_<off|on>.txt: each fallen man's clip, rate and start and where he is in the picture
// Usage: land_hits_picture <soldiers folder> <armies.json> <out folder> [battle.json]
// land_hits_pictures.py lays them out with their labels.
#include "Battle/LandMen.h"
#include "land_json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace LandMen;

namespace
{
	std::string ReadFile(const std::string& P)
	{
		std::ifstream F(P, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	struct D3
	{
		double X = 0, Y = 0, Z = 0;
	};
	D3 operator-(D3 A, D3 B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
	D3 operator*(D3 A, double K) { return { A.X * K, A.Y * K, A.Z * K }; }
	double Dot(D3 A, D3 B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	D3 Cross(D3 A, D3 B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
	D3 Unit(D3 A)
	{
		const double L = std::sqrt(Dot(A, A));
		return L > 1e-12 ? A * (1.0 / L) : D3 { 0, 0, 1 };
	}

	/** An orthographic eye: looking along Look, Up up, Scale pixels a metre, centred on At. */
	struct Canvas
	{
		int W = 0, H = 0;
		std::vector<float> Depth;
		std::vector<unsigned char> Rgb;
		D3 At, Look, Right, Upv;
		double Scale = 100.0;
		void Begin(int Wd, int Ht, D3 Centre, D3 LookDir, D3 UpHint, double PxPerM, unsigned char R, unsigned char G, unsigned char B)
		{
			W = Wd;
			H = Ht;
			Depth.assign(static_cast<size_t>(W) * static_cast<size_t>(H), 1e30f);
			Rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3, 0);
			for (size_t I = 0; I < static_cast<size_t>(W) * static_cast<size_t>(H); ++I) { Rgb[I * 3] = R; Rgb[I * 3 + 1] = G; Rgb[I * 3 + 2] = B; }
			At = Centre;
			Look = Unit(LookDir);
			Right = Unit(Cross(Look, UpHint));
			Upv = Cross(Right, Look);
			Scale = PxPerM;
		}
		void Project(D3 P, double& Sx, double& Sy, double& Sz) const
		{
			const D3 D = P - At;
			Sx = W * 0.5 + Dot(D, Right) * Scale;
			Sy = H * 0.5 - Dot(D, Upv) * Scale;
			Sz = Dot(D, Look);
		}
		void Tri(D3 A, D3 B, D3 C, double Cr, double Cg, double Cb)
		{
			const D3 N = Unit(Cross(B - A, C - A));
			const D3 Light = Unit(D3 { -0.4, -0.6, 0.8 });
			// (two-sided: his own axes are left-handed, so the winding is the mirror's; light either face)
			const double Lm = 0.35 + 0.65 * std::fabs(Dot(N, Light));
			double Ax, Ay, Az, Bx, By, Bz, Cx, Cy, Cz;
			Project(A, Ax, Ay, Az);
			Project(B, Bx, By, Bz);
			Project(C, Cx, Cy, Cz);
			const int X0 = std::max(0, static_cast<int>(std::floor(std::min({ Ax, Bx, Cx })))), X1 = std::min(W - 1, static_cast<int>(std::ceil(std::max({ Ax, Bx, Cx }))));
			const int Y0 = std::max(0, static_cast<int>(std::floor(std::min({ Ay, By, Cy })))), Y1 = std::min(H - 1, static_cast<int>(std::ceil(std::max({ Ay, By, Cy }))));
			const double Area = (Bx - Ax) * (Cy - Ay) - (By - Ay) * (Cx - Ax);
			if (std::fabs(Area) < 1e-12) return;
			for (int Y = Y0; Y <= Y1; ++Y)
			{
				for (int X = X0; X <= X1; ++X)
				{
					const double Px = X + 0.5, Py = Y + 0.5;
					const double W0 = ((Bx - Px) * (Cy - Py) - (By - Py) * (Cx - Px)) / Area;
					const double W1 = ((Cx - Px) * (Ay - Py) - (Cy - Py) * (Ax - Px)) / Area;
					const double W2 = 1.0 - W0 - W1;
					if (W0 < 0 || W1 < 0 || W2 < 0) continue;
					const float Z = static_cast<float>(W0 * Az + W1 * Bz + W2 * Cz);
					const size_t I = static_cast<size_t>(Y) * static_cast<size_t>(W) + static_cast<size_t>(X);
					if (Z >= Depth[I]) continue;
					Depth[I] = Z;
					Rgb[I * 3] = static_cast<unsigned char>(std::min(255.0, Cr * Lm));
					Rgb[I * 3 + 1] = static_cast<unsigned char>(std::min(255.0, Cg * Lm));
					Rgb[I * 3 + 2] = static_cast<unsigned char>(std::min(255.0, Cb * Lm));
				}
			}
		}
		void Save(const std::string& Path) const
		{
			FILE* F = std::fopen(Path.c_str(), "wb");
			if (!F) return;
			std::fprintf(F, "P6\n%d %d\n255\n", W, H);
			std::fwrite(Rgb.data(), 1, Rgb.size(), F);
			std::fclose(F);
		}
	};

	void PaintMan(Crowd& C, int Man, Canvas& Cv)
	{
		if (!C.Visible(Man)) return;
		const Lod L = Near;
		const Look& Lk = C.LookOf(Man, L);
		const int N = C.PointsOf(Man, L);
		std::vector<float> At(static_cast<size_t>(N) * 3), Fa(static_cast<size_t>(N) * 3);
		C.Skin(Man, L, At.data(), Fa.data());
		auto P = [&](uint32_t K) { return D3 { At[K * 3], At[K * 3 + 1], At[K * 3 + 2] }; };
		for (size_t T = 0; T + 2 < Lk.Triangles.size(); T += 3)
		{
			const uint32_t A = Lk.Triangles[T], B = Lk.Triangles[T + 1], Cc = Lk.Triangles[T + 2];
			const double R = (Lk.Rgb[A * 3] + Lk.Rgb[B * 3] + Lk.Rgb[Cc * 3]) / 3.0;
			const double G = (Lk.Rgb[A * 3 + 1] + Lk.Rgb[B * 3 + 1] + Lk.Rgb[Cc * 3 + 1]) / 3.0;
			const double Bl = (Lk.Rgb[A * 3 + 2] + Lk.Rgb[B * 3 + 2] + Lk.Rgb[Cc * 3 + 2]) / 3.0;
			Cv.Tri(P(A), P(B), P(Cc), R, G, Bl);
		}
	}

	void PaintGround(Canvas& Cv, double Cx, double Cy, double Half)
	{
		// (a flat field, in squares a metre wide, two greens)
		for (int I = -static_cast<int>(Half); I < static_cast<int>(Half); ++I)
		{
			for (int J = -static_cast<int>(Half); J < static_cast<int>(Half); ++J)
			{
				const double X = std::floor(Cx) + I, Y = std::floor(Cy) + J;
				const bool bDark = ((I + J) & 1) != 0;
				const double G = bDark ? 118 : 128;
				Cv.Tri({ X, Y, 0 }, { X + 1, Y, 0 }, { X + 1, Y + 1, 0 }, G * 0.95, G, G * 0.7);
				Cv.Tri({ X, Y, 0 }, { X + 1, Y + 1, 0 }, { X, Y + 1, 0 }, G * 0.95, G, G * 0.7);
			}
		}
	}

	struct Files
	{
		std::map<std::string, std::shared_ptr<Soldier>> ByArmy;
		std::string Armies;
	};

	void Fill(Crowd& C, const Files& Fs)
	{
		C.SetLife(2.0);
		for (const auto& KV : Fs.ByArmy) C.AddSoldier(KV.first, KV.second);
		std::string Err;
		C.ReadArmies(Fs.Armies, Err);
	}

	std::string Strip(double SX, double SY, double Go)
	{
		const double Hit = 5.0, F = 0.0;
		const double Face = std::atan2(0.0 - SX, 0.0 - SY);
		std::ostringstream R;
		R.precision(10);
		R << "{\"seed\": " << (std::getenv("LAND_HITS_SEED") ? std::atoi(std::getenv("LAND_HITS_SEED")) : 42) << ", \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"}], \"men\": ["
		  << "{\"b\": 2, \"k\": 1, \"r\": \"rifle\", \"at\": [[0, " << -std::sin(F) * Go * Hit << ", " << -std::cos(F) * Go * Hit << ", " << F << "], [40, "
		  << std::sin(F) * Go * 35.0 << ", " << std::cos(F) * Go * 35.0 << ", " << F << "]], \"ch\": [[0, \"" << (Go > 0.0 ? "advance" : "fire_stand")
		  << "\", \"none\", 0, 0, 0, 0]], \"d\": " << Hit << "},"
		  << "{\"b\": 1, \"k\": 1, \"r\": \"rifle\", \"at\": [[0, " << SX << ", " << SY << ", " << Face << "], [40, " << SX << ", " << SY << ", " << Face
		  << "]], \"ch\": [[0, \"fire_prone\", \"none\", 0, 0, 0, 0]]}],"
		  << "\"rounds\": [{\"t\": 5, \"l\": 5, \"w\": \"rifle\", \"by\": 1, \"k\": 1, \"on\": 2, \"ok\": 1, \"x\": 0, \"y\": 0, \"r\": \"kill\"}]}";
		return R.str();
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4) { std::printf("usage: land_hits_picture <soldiers folder> <armies.json> <out folder> [battle.json]\n"); return 2; }
	Files Fs;
	{
		DIR* Dir = opendir(Argv[1]);
		std::vector<std::string> Names;
		while (Dir)
		{
			dirent* E = readdir(Dir);
			if (!E) break;
			const std::string N = E->d_name;
			if (N.rfind("soldier_", 0) == 0 && N.size() > 13 && N.substr(N.size() - 5) == ".json" && N.find("_gore") == std::string::npos) Names.push_back(N);
		}
		if (Dir) closedir(Dir);
		for (const std::string& N : Names)
		{
			auto S = std::make_shared<Soldier>();
			std::string Err;
			if (S->Load(ReadFile(std::string(Argv[1]) + "/" + N), Err)) Fs.ByArmy[N.substr(8, N.size() - 13)] = S;
		}
		Fs.Armies = ReadFile(Argv[2]);
	}
	const std::string Out = Argv[3];
	// ---- The strips.
	struct Case { const char* Name; double SX, SY, Go; };
	const Case Cases[] = { { "front", 0.0, 150.0, 0.0 }, { "left", -150.0, 0.0, 0.0 }, { "running", 0.0, 150.0, 2.5 } };
	const double Times[] = { 0.0, 0.05, 0.1, 0.2, 0.4, 1.0 };
	for (const Case& Cs : Cases)
	{
		const std::string Rec = Strip(Cs.SX, Cs.SY, Cs.Go);
		D3 Centres[6];
		for (int On = 0; On < 2; ++On)
		{
			for (int K = 0; K < 6; ++K)
			{
				Crowd C;
				Fill(C, Fs);
				Crowd::Hits H;
				H.bJolt = On != 0;
				C.SetHits(H);
				std::string Err;
				C.Build(Rec, [](double, double) { return LandMen::Ground(); }, Err);
				C.SetEye(0, -30, 2);
				const double T = 5.0 + Times[K] / 2.0; // (life seconds: the game plays the fight at half the battle's clock)
				for (int F = 0; F / 30.0 < T - 1e-9; ++F) C.Update(F / 30.0);
				C.Update(T);
				const int Man = C.Find(2, 1, 0);
				double X = 0, Y = 0, Z = 0;
				float Fa = 0.f;
				C.PlaceOf(Man, X, Y, Z, Fa);
				// (centred on his pelvis as today has it, the same for both rows)
				if (On == 0)
				{
					const Soldier* S = C.SoldierOf(Man);
					const int Pe = S->BoneIndex("pelvis");
					const Vec3 P = C.FramesOf(Man)[Pe].Point(S->Bones[static_cast<size_t>(Pe)].Head);
					Centres[K] = D3 { P.X, P.Y, std::max(0.55, std::min(0.95, static_cast<double>(P.Z) + 0.1)) };
				}
				X = Centres[K].X;
				Y = Centres[K].Y;
				// (seen square to the shot, from his right side of it: the jolt across the picture)
				const D3 Shot = Unit(D3 { 0.0 - Cs.SX, 0.0 - Cs.SY, 0.0 });
				const D3 Look = Unit(D3 { Shot.Y, -Shot.X, -0.15 });
				Canvas Cv;
				Cv.Begin(300, 340, D3 { X, Y, Centres[K].Z }, Look, D3 { 0, 0, 1 }, 140.0, 196, 206, 214);
				PaintGround(Cv, X, Y, 4.0);
				PaintMan(C, Man, Cv);
				Cv.Save(Out + "/" + Cs.Name + "_" + (On ? "on" : "off") + "_" + std::to_string(K) + ".ppm");
				if (K == 0 && On == 0) std::printf("strip %s: fall %s\n", Cs.Name, C.Info(Man).FallClip.c_str());
			}
		}
	}
	// ---- The dense moment.
	if (Argc > 4)
	{
		const std::string RecText = ReadFile(Argv[4]);
		LandJson::Value Rec;
		LandJson::Parse(RecText, Rec);
		std::vector<float> Hts;
		const auto& Fd = Rec["field"];
		const int Cols = Fd["cols"].Int(120), Rows = Fd["rows"].Int(80);
		const double Cell = Fd["cell"].Num(25.0);
		for (size_t I = 0; I < Fd["height"].Count(); ++I) Hts.push_back(static_cast<float>(Fd["height"].At(I).Num()));
		const GroundFn GroundAt = [&](double X, double Y) {
			const double FX = X / Cell - 0.5, FY = Y / Cell - 0.5;
			const int C0 = static_cast<int>(std::floor(FX)), R0 = static_cast<int>(std::floor(FY));
			auto At = [&](int Cc, int Rr) { Cc = std::max(0, std::min(Cols - 1, Cc)); Rr = std::max(0, std::min(Rows - 1, Rr)); return Hts.empty() ? 0.0 : static_cast<double>(Hts[static_cast<size_t>(Rr) * static_cast<size_t>(Cols) + static_cast<size_t>(Cc)]); };
			const double U = FX - C0, V = FY - R0;
			LandMen::Ground G;
			G.Height = static_cast<float>(At(C0, R0) * (1 - U) * (1 - V) + At(C0 + 1, R0) * U * (1 - V) + At(C0, R0 + 1) * (1 - U) * V + At(C0 + 1, R0 + 1) * U * V);
			return G;
		};
		// Where the most men fall within 15 m and 8 s of one another, read from the crowd off.
		double BestT = 0, BestX = 0, BestY = 0;
		{
			Crowd C;
			Fill(C, Fs);
			std::string Err;
			C.Build(RecText, GroundAt, Err);
			C.SetThrift(true);
			double Last = 0;
			for (int I = 0; I < C.Count(); ++I) Last = std::max(Last, C.Info(I).Dead);
			std::vector<double> Xs(static_cast<size_t>(C.Count())), Ys(static_cast<size_t>(C.Count()));
			for (int F = 0; F / 30.0 <= Last + 1.0; ++F)
			{
				C.Update(F / 30.0);
				for (int I = 0; I < C.Count(); ++I)
				{
					if (C.Info(I).FallStarted >= 0 && Xs[static_cast<size_t>(I)] == 0)
					{
						double Z = 0;
						float Fa = 0;
						C.PlaceOf(I, Xs[static_cast<size_t>(I)], Ys[static_cast<size_t>(I)], Z, Fa);
					}
				}
			}
			int Most = 0;
			for (int I = 0; I < C.Count(); ++I)
			{
				const double Ti = C.Info(I).FallStarted;
				if (Ti < 0) continue;
				int N = 0;
				for (int J = 0; J < C.Count(); ++J)
				{
					const double Tj = C.Info(J).FallStarted;
					if (Tj >= 0 && Tj <= Ti && Ti - Tj <= 8.0 && std::hypot(Xs[static_cast<size_t>(I)] - Xs[static_cast<size_t>(J)], Ys[static_cast<size_t>(I)] - Ys[static_cast<size_t>(J)]) <= 15.0) ++N;
				}
				if (N > Most) { Most = N; BestT = Ti; BestX = Xs[static_cast<size_t>(I)]; BestY = Ys[static_cast<size_t>(I)]; }
			}
			std::printf("dense: %d men fell within 15 m and 8 s, the last at %.1f s, at %.1f, %.1f\n", Most, BestT, BestX, BestY);
		}
		for (int On = 0; On < 2; ++On)
		{
			Crowd C;
			Fill(C, Fs);
			if (On)
			{
				Crowd::Hits H;
				H.bJolt = H.bVariety = H.bWounded = H.bFlinch = true;
				C.SetHits(H);
			}
			std::string Err;
			C.Build(RecText, GroundAt, Err);
			C.SetThrift(true);
			C.SetEye(BestX, BestY - 20, 15);
			const double T = BestT + 3.0;
			for (int F = 0; F / 30.0 < T; ++F) C.Update(F / 30.0);
			C.Update(T);
			const double Gz = GroundAt(BestX, BestY).Height;
			Canvas Cv;
			Cv.Begin(1100, 800, D3 { BestX, BestY, Gz }, D3 { 0.0, 0.55, -1.0 }, D3 { 0, 1, 0 }, 26.0, 150, 160, 120);
			// (the ground under them: their own heights, a grid of half-metre squares)
			for (int I = -40; I < 40; ++I)
			{
				for (int J = -32; J < 32; ++J)
				{
					const double X = std::floor(BestX) + I * 0.5, Y = std::floor(BestY) + J * 0.5;
					auto H = [&](double A, double B) { return static_cast<double>(GroundAt(A, B).Height); };
					const double G = ((I / 2 + J / 2) & 1) ? 120 : 128;
					Cv.Tri({ X, Y, H(X, Y) }, { X + 0.5, Y, H(X + 0.5, Y) }, { X + 0.5, Y + 0.5, H(X + 0.5, Y + 0.5) }, G * 0.95, G, G * 0.72);
					Cv.Tri({ X, Y, H(X, Y) }, { X + 0.5, Y + 0.5, H(X + 0.5, Y + 0.5) }, { X, Y + 0.5, H(X, Y + 0.5) }, G * 0.95, G, G * 0.72);
				}
			}
			std::ofstream Txt(Out + "/dense_" + (On ? "on" : "off") + ".txt");
			for (int I = 0; I < C.Count(); ++I)
			{
				double X = 0, Y = 0, Z = 0;
				float Fa = 0;
				if (!C.Visible(I) || !C.PlaceOf(I, X, Y, Z, Fa)) continue;
				if (std::fabs(X - BestX) > 21 || std::fabs(Y - BestY) > 17) continue;
				PaintMan(C, I, Cv);
				const ManInfo& In = C.Info(I);
				if (In.FallStarted < 0 && In.WoundStarted < 0) continue;
				double Sx, Sy, Sz;
				Cv.Project(D3 { X, Y, Z }, Sx, Sy, Sz);
				Txt << (In.FallStarted >= 0 ? In.FallClip : In.WoundClip) << " " << In.FallRate << " " << In.FallSkip << " " << Sx << " " << Sy << " " << (In.FallStarted >= 0 ? In.FallStarted : In.WoundStarted)
					<< " " << X << " " << Y << "\n";
			}
			Cv.Save(Out + "/dense_" + (On ? "on" : "off") + ".ppm");
		}
		std::printf("dense moment at %.1f s\n", BestT + 3.0);
	}
	return 0;
}
