// The land field's pictures: a software painter for the ground the game is given (Battle/LandField), so the cloud can
// see the ground at a soldier's eye, from 30 m up and from above without the engine. Triangles are rasterised with a
// depth buffer into a buffer twice the picture's size each way (its samples averaged down at the end), lit per sample by
// the July morning sun from the recording's place and hour, with soft shadows from two shadow maps (one fine round the
// eye, one wide), a sky lit by the sun and the hemisphere, and the distance haze of a summer morning (some 3.5 km to
// lose the land). Vertex colours are taken as the paint (sRGB) and made linear; a filmic curve brings the light back
// to the picture. What it paints is the game's meshes and instances only: no texture, no grass the game does not lay.

#include "land_field_picture.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace LandFieldPicture
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr float Empty = std::numeric_limits<float>::infinity();

		struct V3
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
			V3 operator+(const V3& O) const { return { X + O.X, Y + O.Y, Z + O.Z }; }
			V3 operator-(const V3& O) const { return { X - O.X, Y - O.Y, Z - O.Z }; }
			V3 operator*(double S) const { return { X * S, Y * S, Z * S }; }
		};
		double Dot(const V3& A, const V3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		V3 Cross(const V3& A, const V3& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		V3 Unit(const V3& A)
		{
			const double L = std::sqrt(Dot(A, A));
			return L > 1e-12 ? A * (1.0 / L) : V3{ 0, 0, 1 };
		}

		double Linear(double C255)
		{
			const double C = C255 / 255.0;
			return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
		}
		double ToPicture(double L)
		{
			L = std::clamp(L, 0.0, 1.0);
			return 255.0 * (L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(L, 1.0 / 2.4) - 0.055);
		}
		/** The filmic curve (Narkowicz's fit of ACES): the sunlit ground's highlights roll off instead of clipping. */
		double Filmic(double X) { return std::clamp(X * (2.51 * X + 0.03) / (X * (2.43 * X + 0.59) + 0.14), 0.0, 1.0); }

		float LinearOf[256];

		// ------------------------------------------------------------------ noise (the sky's clouds)

		uint64_t Mix(uint64_t H)
		{
			H ^= H >> 33;
			H *= 0xFF51AFD7ED558CCDull;
			H ^= H >> 33;
			H *= 0xC4CEB9FE1A85EC53ull;
			H ^= H >> 33;
			return H;
		}
		double Lattice(int64_t I, int64_t J, uint64_t Seed) { return static_cast<double>(Mix(static_cast<uint64_t>(I) * 0x9E3779B97F4A7C15ull ^ Mix(static_cast<uint64_t>(J) + Seed)) >> 11) / 9007199254740992.0; }
		double Noise(double X, double Y, uint64_t Seed)
		{
			const double FX = std::floor(X), FY = std::floor(Y);
			const int64_t I = static_cast<int64_t>(FX), J = static_cast<int64_t>(FY);
			double U = X - FX, V = Y - FY;
			U = U * U * (3.0 - 2.0 * U);
			V = V * V * (3.0 - 2.0 * V);
			const double A = Lattice(I, J, Seed), B = Lattice(I + 1, J, Seed), C = Lattice(I, J + 1, Seed), D = Lattice(I + 1, J + 1, Seed);
			return (A + (B - A) * U) * (1.0 - V) + (C + (D - C) * U) * V;
		}

		// ------------------------------------------------------------------ views

		/** A camera: perspective (Tan: half the picture's width as a tangent) or straight along Fwd (Half: half its width, metres). */
		struct View
		{
			V3 Eye, Fwd, Right, Up;
			double Tan = 0.6, Half = 100.0, Near = 0.05;
			bool bOrtho = false;
			int W = 0, H = 0;

			V3 ToView(const V3& P) const
			{
				const V3 Q = P - Eye;
				return { Dot(Q, Right), Dot(Q, Up), Dot(Q, Fwd) };
			}
			void ToScreen(double X, double Y, double Z, double& SX, double& SY) const
			{
				const double K = bOrtho ? 1.0 / Half : 1.0 / (Z * Tan);
				SX = 0.5 * W * (1.0 + X * K);
				SY = 0.5 * H - 0.5 * W * Y * K;
			}
			V3 FromSample(double SX, double SY, double Z) const
			{
				const double U = (SX - 0.5 * W) / (0.5 * W), V = (0.5 * H - SY) / (0.5 * W);
				if (bOrtho) return Eye + Right * (U * Half) + Up * (V * Half) + Fwd * Z;
				return Eye + Fwd * Z + Right * (U * Tan * Z) + Up * (V * Tan * Z);
			}
			V3 RayOf(double SX, double SY) const
			{
				const double U = (SX - 0.5 * W) / (0.5 * W), V = (0.5 * H - SY) / (0.5 * W);
				return Unit(Fwd + Right * (U * Tan) + Up * (V * Tan));
			}
			/** Whether a ball might be seen. */
			bool Sees(const V3& C, double R) const
			{
				const V3 Q = ToView(C);
				const double Aspect = static_cast<double>(H) / W;
				if (bOrtho) return std::fabs(Q.X) <= Half + R && std::fabs(Q.Y) <= Half * Aspect + R;
				if (Q.Z < Near - R) return false;
				const double Slack = R * std::sqrt(1.0 + Tan * Tan);
				return std::fabs(Q.X) <= Q.Z * Tan + Slack && std::fabs(Q.Y) <= Q.Z * Tan * Aspect + Slack;
			}
		};

		View Looking(const V3& Eye, double Yaw, double Pitch, double FovDeg, int W, int H)
		{
			View V;
			V.Eye = Eye;
			V.Fwd = { std::cos(Pitch) * std::cos(Yaw), std::cos(Pitch) * std::sin(Yaw), std::sin(Pitch) };
			V.Right = Unit(Cross(V.Fwd, { 0, 0, 1 }));
			V.Up = Cross(V.Right, V.Fwd);
			V.Tan = std::tan(0.5 * FovDeg * Pi / 180.0);
			V.W = W;
			V.H = H;
			return V;
		}

		/** The view from the sun over a square of ground (for a shadow map). */
		View FromSun(const V3& Sun, const V3& Centre, double Size, int N)
		{
			View V;
			V.bOrtho = true;
			V.Fwd = Sun * -1.0;
			V.Right = Unit(Cross(V.Fwd, { 0, 0, 1 }));
			V.Up = Cross(V.Right, V.Fwd);
			V.Eye = Centre + Sun * 3000.0;
			V.Half = 0.5 * Size;
			V.W = V.H = N;
			return V;
		}

		// ------------------------------------------------------------------ what is drawn

		enum Material : uint8_t
		{
			MatGround,
			MatLeaves,
			MatBlades,
			MatSoil // a ground tile under the game's ground surface (LAND_FIELD_SURFACE): its paint kept as sRGB, made up in Shade
		};

		/**
		 * The leaves' picture (Tools/land_look/T_LandLeaves.png, read once through ImageMagick), as the game's leaf cards
		 * carry it: RGB linear numbers (0.5 as painted), A the leaves. Empty where it cannot be read: the cards are then not
		 * drawn, and the sprigs that stand in for them where the game has no leaves' material are.
		 */
		struct Leaves
		{
			int W = 0, H = 0;
			std::vector<uint8_t> Px; // RGBA
			bool Ok() const { return W > 0 && H > 0; }
			const uint8_t* At(double U, double V) const
			{
				const int X = std::clamp(static_cast<int>(std::floor(U * W)), 0, W - 1), Y = std::clamp(static_cast<int>(std::floor(V * H)), 0, H - 1);
				return &Px[(static_cast<size_t>(Y) * W + X) * 4];
			}
		};

		const Leaves& LeafPicture()
		{
			static const Leaves L = [] {
				Leaves Out;
				std::string Path;
				if (const char* Env = std::getenv("LAND_LEAVES")) Path = Env;
				else
				{
					Path = __FILE__;
					const size_t Cut = Path.rfind("land_field/");
					Path = Cut == std::string::npos ? std::string("Tools/land_look/T_LandLeaves.png") : Path.substr(0, Cut) + "land_look/T_LandLeaves.png";
				}
				const std::string Cmd = "convert '" + Path + "' -depth 8 RGBA:- 2>/dev/null";
				FILE* P = popen(Cmd.c_str(), "r");
				if (!P) return Out;
				std::vector<uint8_t> Buf;
				uint8_t Chunk[65536];
				size_t Got;
				while ((Got = std::fread(Chunk, 1, sizeof(Chunk), P)) > 0) Buf.insert(Buf.end(), Chunk, Chunk + Got);
				pclose(P);
				const size_t Side = static_cast<size_t>(std::lround(std::sqrt(static_cast<double>(Buf.size() / 4))));
				if (Side > 0 && Side * Side * 4 == Buf.size())
				{
					Out.W = Out.H = static_cast<int>(Side);
					Out.Px = std::move(Buf);
				}
				std::printf("  leaves' picture %s: %s\n", Path.c_str(), Out.Ok() ? (std::to_string(Out.W) + " px").c_str() : "not read (the sprigs are drawn)");
				return Out;
			}();
			return L;
		}

		/** The air over the field (Tools/land_look/T_LandCloudWind.png, make_sky_textures.py): red the clouds, green the
		 * gusts, as the game's materials read it (M_LandCloudShade, wind_sheen in build_content.py). */
		const Leaves& AirPicture()
		{
			static const Leaves L = [] {
				Leaves Out;
				std::string Path = __FILE__;
				const size_t Cut = Path.rfind("land_field/");
				Path = Cut == std::string::npos ? std::string("Tools/land_look/T_LandCloudWind.png") : Path.substr(0, Cut) + "land_look/T_LandCloudWind.png";
				const std::string Cmd = "convert '" + Path + "' -depth 8 RGBA:- 2>/dev/null";
				FILE* P = popen(Cmd.c_str(), "r");
				if (!P) return Out;
				std::vector<uint8_t> Buf;
				uint8_t Chunk[65536];
				size_t Got;
				while ((Got = std::fread(Chunk, 1, sizeof(Chunk), P)) > 0) Buf.insert(Buf.end(), Chunk, Chunk + Got);
				pclose(P);
				const size_t Side = static_cast<size_t>(std::lround(std::sqrt(static_cast<double>(Buf.size() / 4))));
				if (Side > 0 && Side * Side * 4 == Buf.size())
				{
					Out.W = Out.H = static_cast<int>(Side);
					Out.Px = std::move(Buf);
				}
				return Out;
			}();
			return L;
		}

		/** One channel of the air's picture at (U, V) in repeats, wrapping, read between its pixels as a material does. */
		double AirAt(double U, double V, int Channel)
		{
			const Leaves& A = AirPicture();
			if (!A.Ok()) return 0.5;
			const double X = (U - std::floor(U)) * A.W - 0.5, Y = (V - std::floor(V)) * A.H - 0.5;
			const int X0 = static_cast<int>(std::floor(X)), Y0 = static_cast<int>(std::floor(Y));
			const double FX = X - X0, FY = Y - Y0;
			auto Px = [&](int I, int J) { return A.Px[(static_cast<size_t>((J % A.H + A.H) % A.H) * A.W + (I % A.W + A.W) % A.W) * 4 + Channel] / 255.0; };
			return (Px(X0, Y0) * (1 - FX) + Px(X0 + 1, Y0) * FX) * (1 - FY) + (Px(X0, Y0 + 1) * (1 - FX) + Px(X0 + 1, Y0 + 1) * FX) * FY;
		}

		/** The game's switches for the wind and the clouds' shadows, as the painter shows them in a still (release 2,
		 * 23 November): LAND_FIELD_CLOUD_SHADE=dark (-TheaterLandCloudShadows: the share of the sun left under a cloud) and
		 * LAND_FIELD_SHEEN=s (the gusts' sheen over the grass, the game's 0.06 at a breeze; ripe grain 11 of its 6), each at the battle's start (the clouds not yet carried, the gusts where they stand). */
		struct Air
		{
			double Dark = -1.0, Sheen = 0.0;
		};
		const Air& AirAsked()
		{
			static const Air A = [] {
				Air Out;
				if (const char* E = std::getenv("LAND_FIELD_CLOUD_SHADE")) Out.Dark = std::clamp(std::atof(E), 0.0, 1.0);
				if (const char* E = std::getenv("LAND_FIELD_SHEEN")) Out.Sheen = std::clamp(std::atof(E), 0.0, 0.5);
				if (Out.Dark >= 0.0 || Out.Sheen > 0.0)
					std::printf("  the air (the game's wind and clouds' shadows): clouds' shadows %s, gusts' sheen %.2f%s\n", Out.Dark >= 0.0 ? ("leave " + std::to_string(Out.Dark).substr(0, 4) + " of the sun").c_str() : "off",
					            Out.Sheen, AirPicture().Ok() ? "" : " (T_LandCloudWind.png not read: nothing shown)");
				return Out;
			}();
			return A;
		}

		// ------------------------------------------------------------------ the game's ground surface (M_LandSurface)

		/**
		 * One of the ground's detail pictures (Tools/land_look/T_LandGrain.png, T_LandBump.png, T_LandPatch.png,
		 * make_ground_textures.py) as the game reads it: plain numbers (not sRGB), wrapping, with its smaller pictures made
		 * at load by 2 x 2 averages (as the engine's simple-average mips), read between pixels and between sizes.
		 */
		struct Mipped
		{
			std::vector<std::vector<float>> MipPx; // RGBA, 0..1, the full picture first
			std::vector<int> MipSide;
			bool Ok() const { return !MipPx.empty(); }
			int MipWidth() const { return MipSide.empty() ? 0 : MipSide[0]; }

			void ReadAt(int L, double U, double V, float Out[4]) const
			{
				const int S = MipSide[L];
				const std::vector<float>& Px = MipPx[L];
				const double X = (U - std::floor(U)) * S - 0.5, Y = (V - std::floor(V)) * S - 0.5;
				const double FX0 = std::floor(X), FY0 = std::floor(Y);
				const float FX = static_cast<float>(X - FX0), FY = static_cast<float>(Y - FY0);
				const int X0 = (static_cast<int>(FX0) % S + S) % S, Y0 = (static_cast<int>(FY0) % S + S) % S;
				const int X1 = (X0 + 1) % S, Y1 = (Y0 + 1) % S;
				const float* A = &Px[(static_cast<size_t>(Y0) * S + X0) * 4];
				const float* B = &Px[(static_cast<size_t>(Y0) * S + X1) * 4];
				const float* C = &Px[(static_cast<size_t>(Y1) * S + X0) * 4];
				const float* D = &Px[(static_cast<size_t>(Y1) * S + X1) * 4];
				for (int Q = 0; Q < 4; ++Q) Out[Q] = (A[Q] + (B[Q] - A[Q]) * FX) * (1.0f - FY) + (C[Q] + (D[Q] - C[Q]) * FX) * FY;
			}
			/** At a size (Lod 0 the full picture, 1 the half...), between the two nearest sizes. */
			void ReadBetween(double U, double V, double Lod, float Out[4]) const
			{
				const int Top = static_cast<int>(MipPx.size()) - 1;
				Lod = std::clamp(Lod, 0.0, static_cast<double>(Top));
				const int L0 = static_cast<int>(std::floor(Lod)), L1 = std::min(L0 + 1, Top);
				const float F = static_cast<float>(Lod - L0);
				ReadAt(L0, U, V, Out);
				if (F <= 0.0f || L1 == L0) return;
				float Two[4];
				ReadAt(L1, U, V, Two);
				for (int Q = 0; Q < 4; ++Q) Out[Q] += (Two[Q] - Out[Q]) * F;
			}
		};

		Mipped LoadMipped(const std::string& Path)
		{
			Mipped Out;
			const std::string Cmd = "convert '" + Path + "' -depth 8 RGBA:- 2>/dev/null";
			FILE* P = popen(Cmd.c_str(), "r");
			if (!P) return Out;
			std::vector<uint8_t> Buf;
			uint8_t Chunk[65536];
			size_t Got;
			while ((Got = std::fread(Chunk, 1, sizeof(Chunk), P)) > 0) Buf.insert(Buf.end(), Chunk, Chunk + Got);
			pclose(P);
			const size_t Side = static_cast<size_t>(std::lround(std::sqrt(static_cast<double>(Buf.size() / 4))));
			if (Side == 0 || Side * Side * 4 != Buf.size() || (Side & (Side - 1)) != 0) return Out;
			std::vector<float> Full(Buf.size());
			for (size_t K = 0; K < Buf.size(); ++K) Full[K] = Buf[K] / 255.0f;
			Out.MipPx.push_back(std::move(Full));
			Out.MipSide.push_back(static_cast<int>(Side));
			while (Out.MipSide.back() > 1)
			{
				const int S = Out.MipSide.back(), H = S / 2;
				const std::vector<float>& Up = Out.MipPx.back();
				std::vector<float> Down(static_cast<size_t>(H) * H * 4);
				for (int Y = 0; Y < H; ++Y)
					for (int X = 0; X < H; ++X)
						for (int Q = 0; Q < 4; ++Q)
							Down[(static_cast<size_t>(Y) * H + X) * 4 + Q] =
								0.25f * (Up[(static_cast<size_t>(2 * Y) * S + 2 * X) * 4 + Q] + Up[(static_cast<size_t>(2 * Y) * S + 2 * X + 1) * 4 + Q] +
										 Up[(static_cast<size_t>(2 * Y + 1) * S + 2 * X) * 4 + Q] + Up[(static_cast<size_t>(2 * Y + 1) * S + 2 * X + 1) * 4 + Q]);
				Out.MipPx.push_back(std::move(Down));
				Out.MipSide.push_back(H);
			}
			return Out;
		}

		/**
		 * LAND_FIELD_SURFACE=1: the ground tiles (near, far, the ring; not the tiles' built pieces, the houses, the plants
		 * or the men, which keep M_MapSurface in the game) painted as M_LandSurface paints them (build_content.py,
		 * build_land_surface) over their vertex paint: the grain, the broad lie, the tussocks, the trodden places, the
		 * lean, with the material's numbers and the one the game changes when it starts (TopLight 0.08, LandBattle.cpp;
		 * LAND_FIELD_SURFACE_TOPLIGHT=t as the game's -TheaterLandTopLight=t). The pictures from Tools/land_look, or LAND_SURFACE_DIR. Unset (or the pictures not read): as before, to the byte.
		 */
		struct SurfaceSet
		{
			bool bSurfaceOn = false;
			Mipped GrainPic, BumpPic, PatchPic;
			double TopLight = 0.08;
			int MostAniso = 8;
			/** The furrows on ploughland (M_LandPlough: the tiles' Mesh::Uvs, LandField::Furrows): on unless
			 * LAND_FIELD_FURROWS=0 (the game without M_LandPlough); LAND_FIELD_FURROW_DIAL="period trough depth lean
			 * fade-start fade-end" sets the material's numbers as the art director's Mac pass would. */
			bool bFurrows = true;
			LandField::FurrowDial Dial;
		};
		const SurfaceSet& SurfaceAsked()
		{
			static const SurfaceSet S = [] {
				SurfaceSet Out;
				const char* E = std::getenv("LAND_FIELD_SURFACE");
				if (!E || std::atoi(E) <= 0) return Out;
				std::string Dir;
				if (const char* D = std::getenv("LAND_SURFACE_DIR")) Dir = D;
				else
				{
					Dir = __FILE__;
					const size_t Cut = Dir.rfind("land_field/");
					Dir = Cut == std::string::npos ? std::string("Tools/land_look") : Dir.substr(0, Cut) + "land_look";
				}
				Out.GrainPic = LoadMipped(Dir + "/T_LandGrain.png");
				Out.BumpPic = LoadMipped(Dir + "/T_LandBump.png");
				Out.PatchPic = LoadMipped(Dir + "/T_LandPatch.png");
				Out.bSurfaceOn = Out.GrainPic.Ok() && Out.BumpPic.Ok() && Out.PatchPic.Ok();
				if (const char* T = std::getenv("LAND_FIELD_SURFACE_TOPLIGHT")) Out.TopLight = std::clamp(std::atof(T), 0.0, 1.0);
				if (const char* Fw = std::getenv("LAND_FIELD_FURROWS")) Out.bFurrows = std::atoi(Fw) > 0;
				if (const char* Dl = std::getenv("LAND_FIELD_FURROW_DIAL"))
				{
					LandField::FurrowDial& D = Out.Dial;
					std::sscanf(Dl, "%lf %lf %lf %lf %lf %lf", &D.Period, &D.Trough, &D.Depth, &D.Lean, &D.FadeStart, &D.FadeEnd);
				}
				if (Out.bSurfaceOn)
					std::printf("  the furrows on ploughland (M_LandPlough over the tiles' Uvs): %s, period %.2f m, trough %.2f, depth %.2f, lean %.3f m, fade %.0f to %.0f m\n",
					            Out.bFurrows ? "on" : "off (LAND_FIELD_FURROWS=0)", Out.Dial.Period, Out.Dial.Trough, Out.Dial.Depth, Out.Dial.Lean, Out.Dial.FadeStart, Out.Dial.FadeEnd);
				std::printf("  the ground's surface (M_LandSurface over the tiles' paint, LAND_FIELD_SURFACE): %s from %s, TopLight %.2f\n",
				            Out.bSurfaceOn ? "ON" : "NOT read, off", Dir.c_str(), Out.TopLight);
				return Out;
			}();
			return S;
		}

		/**
		 * M_LandSurface at one sample of a ground tile. Raw: the vertex paint as the mesh holds it (sRGB, 0..1,
		 * between the corners); Nm: the mesh's normal (unit, the field's frame); P: the place (the field's metres);
		 * Depth: along the view (metres); Major, Minor: the picture's pixel on the ground (the longer way as a vector, the
		 * shorter as a length, metres), which pick the pictures' sizes and the taps along the longer way (as a graphics
		 * chip's anisotropic filter, at most MostAniso). Out: the base colour (linear) and the normal the light sees.
		 * The engine's world is the field's x and y swapped, in centimetres (LandParts::FromField, 100 a metre): the
		 * material's metres (R, G) are the field's (y, x).
		 * Way: the tile's two numbers there (Mesh::Uvs between the corners: ploughland's furrows, M_LandPlough; zero for
		 * none); StepR, StepD how far the place moves one picture pixel right and one down (the material's ddx, ddy).
		 */
		void SurfaceShade(const SurfaceSet& S, const double Raw[3], const V3& Nm, const V3& P, double Depth, const V3& Major, double Minor, const float Way[2], const V3& StepR,
						  const V3& StepD, float Out[3], V3& Lit)
		{
			const double WX = P.Y, WY = P.X; // the engine's X and Y, metres
			const double MajorLen = std::hypot(Major.X, Major.Y);
			const int Reads = std::clamp(static_cast<int>(std::ceil(MajorLen / std::max(Minor, 1e-6))), 1, S.MostAniso);
			const double Step = MajorLen / Reads;
			const double GrainSide = S.GrainPic.MipWidth(), BumpSide = S.BumpPic.MipWidth(), PatchSide = S.PatchPic.MipWidth();
			auto LodOf = [Step](double TexelsPerMetre) { return std::max(0.0, std::log2(std::max(Step * TexelsPerMetre, 1e-9))); };
			const double LodGA = LodOf(0.5 * GrainSide), LodGB = LodOf(0.365 * GrainSide);
			const double LodBA = LodOf(0.5 * BumpSide), LodBB = LodOf(0.365 * BumpSide);
			const double LodPA = LodOf(0.025 * PatchSide), LodPB = LodOf(0.0103 * PatchSide);
			float GA[4] = {}, GB[4] = {}, BA[4] = {}, BB[4] = {}, PA[4] = {}, PB[4] = {};
			for (int I = 0; I < Reads; ++I)
			{
				const double T = (I + 0.5) / Reads - 0.5;
				const double X = WX + Major.Y * T, Y = WY + Major.X * T; // (the major way in the engine's frame)
				float Got[4];
				auto Add = [&Got](float Sum[4]) { for (int Q = 0; Q < 4; ++Q) Sum[Q] += Got[Q]; };
				S.GrainPic.ReadBetween(X * 0.5, Y * 0.5, LodGA, Got);
				Add(GA);
				S.GrainPic.ReadBetween(Y * 0.365 + 0.31, X * 0.365 + 0.57, LodGB, Got);
				Add(GB);
				S.BumpPic.ReadBetween(X * 0.5, Y * 0.5, LodBA, Got);
				Add(BA);
				S.BumpPic.ReadBetween(Y * 0.365 + 0.31, X * 0.365 + 0.57, LodBB, Got);
				Add(BB);
				S.PatchPic.ReadBetween(X * 0.025, Y * 0.025, LodPA, Got);
				Add(PA);
				S.PatchPic.ReadBetween(Y * 0.0103 + 0.43, X * 0.0103 + 0.11, LodPB, Got);
				Add(PB);
			}
			for (float* Sum : { GA, GB, BA, BB, PA, PB })
				for (int Q = 0; Q < 4; ++Q) Sum[Q] /= static_cast<float>(Reads);
			auto Sat = [](double V) { return std::clamp(V, 0.0, 1.0); };
			auto Mixed = [](double A, double B, double T) { return A + (B - A) * T; };
			// The two readings of the grain, mixed by the lie's warmth, pushed back out from 0.5 (GrainRestore).
			const double Lead = PA[1] * 0.4 + 0.3, Restore = 1.3;
			double GrainNow[3];
			for (int Q = 0; Q < 3; ++Q) GrainNow[Q] = (Mixed(GA[Q], GB[Q], Lead) - 0.5) * Restore + 0.5;
			const double GrassLum = GrainNow[0], SoilLum = GrainNow[1], GrassHue = GrainNow[2];
			// Only where the surface faces up; fading with the depth (DetailFadeStart 12 m, DetailFadeEnd 140 m, DetailFar 0.35).
			const double Skyward = Nm.Z;
			const double Up = Sat((Skyward - 0.55) * 4.0);
			const double Fade = Sat((Depth - 12.0) / (140.0 - 12.0));
			const double Strength = 1.0 * Mixed(1.0, 0.35, Fade) * Up;
			// Grass or soil from the paint itself; the trodden places on grass (Wear 1).
			const double Green = Sat((Raw[1] - Raw[0]) * 12.0 + 0.6);
			const double Wear = Sat(PA[2] * 1.0) * Green * Up;
			const double Grassy = Green * (1.0 - Wear);
			// The broad lie (PatchValue 0.8, PatchWarmth 1, PatchStrength 1), snow's exception (PatchSnow 0.85), tussocks
			// (TussockBroad 0.5, TussockDark -0.4, TussockCool -0.3), PatchHue (0.35, 0.1, -0.45).
			const double PatchR = 0.5 * (PA[0] + PB[0]), PatchG = 0.5 * (PA[1] + PB[1]);
			const double PatchValue = (PatchR - 0.5) * 0.8, PatchWarm = (PatchG - 0.5) * 1.0;
			const double Snowy = Sat((Raw[2] - 0.7) * 8.0);
			const double Unsnowed = Snowy * -0.85 + 1.0;
			const double PatchOn = 1.0 * Up * Unsnowed;
			const double Lush = Sat(PB[1] * -4.0 + 3.0);
			const double Clumps = PA[3] + PB[3] * 0.5;
			const double Tussock = Sat(Clumps * Lush) * Grassy * PatchOn;
			const double ValueShift = PatchValue * PatchOn + Tussock * -0.4;
			const double WarmShift = PatchWarm * PatchOn + Tussock * -0.3;
			const double PatchHue[3] = { 0.35, 0.1, -0.45 }, GrainHue[3] = { 0.3, 0.1, -0.35 }, WornColour[3] = { 0.17, 0.14, 0.085 };
			// The grain's brightness and hue (SoilWarmth 1.6; a sward's own hue twice over).
			const double Lum = Mixed(SoilLum, GrassLum, Grassy);
			const double Factor = Mixed(1.0, Lum * 2.0, Strength);
			const double Hue = Mixed((SoilLum - 0.5) * 1.6, (GrassHue - 0.5) * 2.0, Grassy);
			const double Worn = Wear * 0.6; // WornMix
			const double TopLit = 1.0 + Sat(Skyward) * S.TopLight;
			// The furrows on ploughland (M_LandPlough's furrow_term: LandField::Furrows, its specification), where the tile
			// carries them; nothing (times 1, no lean) where it does not.
			const LandField::FurrowLook Furrow = S.bFurrows ? LandField::Furrows(P.X, P.Y, Way[0], Way[1], Depth, StepR.X, StepR.Y, StepD.X, StepD.Y, Up, 1.0 - Grassy, SoilLum, PatchValue, S.Dial)
														  : LandField::FurrowLook();
			for (int Q = 0; Q < 3; ++Q)
			{
				const double Broad = (ValueShift + 1.0) * (PatchHue[Q] * WarmShift + 1.0);
				const double Tinted = GrainHue[Q] * (Hue * Strength) + 1.0;
				const double Base = Mixed(std::pow(std::max(Raw[Q], 0.0), 2.2), WornColour[Q], Worn);
				// (the engine keeps a base colour within 0..1)
				Out[Q] = static_cast<float>(std::clamp(Base * Broad * Tinted * Factor * Furrow.Times * TopLit, 0.0, 1.0));
			}
			// The lean: the soil's (blue, alpha) or the sward's (red, green), the turned reading's turned back, restored,
			// DetailBump 0.8, flattened to DetailBumpFar 0.15 by the fade, only facing up; added to the mesh's normal.
			const double LeanAX = Mixed(BA[2] * 2.0 - 1.0, BA[0] * 2.0 - 1.0, Grassy), LeanAY = Mixed(BA[3] * 2.0 - 1.0, BA[1] * 2.0 - 1.0, Grassy);
			const double LeanBX = Mixed(BB[3] * 2.0 - 1.0, BB[1] * 2.0 - 1.0, Grassy), LeanBY = Mixed(BB[2] * 2.0 - 1.0, BB[0] * 2.0 - 1.0, Grassy);
			const double Scale = Restore * 0.8 * Mixed(1.0, 0.15, Fade) * Up;
			const double LeanX = Mixed(LeanAX, LeanBX, Lead) * Scale, LeanY = Mixed(LeanAY, LeanBY, Lead) * Scale; // the engine's X, Y
			Lit = Unit({ Nm.X + LeanY + Furrow.LeanX, Nm.Y + LeanX + Furrow.LeanY, Nm.Z });
		}

		/** The ground a picture's pixel covers at a sample (the plane of the sample's face, one picture pixel, Down
		 * samples, each way): the longer way as a vector and the shorter's length, along the ground (x and y only, as the
		 * material reads the pictures by the world's x and y). */
		void Footprint(const View& V, double SX, double SY, double Down, const V3& P, const V3& N, V3& Major, double& Minor, V3* OutR = nullptr, V3* OutD = nullptr)
		{
			auto Hit = [&](double X, double Y) {
				V3 From = V.Eye, Way = V.Fwd;
				if (V.bOrtho) From = V.FromSample(X, Y, 0.0);
				else Way = V.RayOf(X, Y);
				const double Facing = Dot(Way, N);
				const double T = std::fabs(Facing) < 1e-6 ? 1e6 : Dot(P - From, N) / Facing;
				return From + Way * std::clamp(T, 0.0, 1e6);
			};
			const V3 A = Hit(SX + Down, SY) - P, B = Hit(SX, SY + Down) - P;
			if (OutR) *OutR = A;
			if (OutD) *OutD = B;
			const double LA = std::hypot(A.X, A.Y), LB = std::hypot(B.X, B.Y);
			Major = LA >= LB ? V3{ A.X, A.Y, 0.0 } : V3{ B.X, B.Y, 0.0 };
			const double Most = 1e4;
			const double LM = std::max(LA, LB);
			if (LM > Most) Major = Major * (Most / LM);
			Minor = std::min(std::min(LA, LB), Most);
		}

		/** LAND_FIELD_PICTURES="eye,oblique,map" (any of them): only those painted. Unset: all three. */
		bool WantPicture(const char* Which)
		{
			const char* E = std::getenv("LAND_FIELD_PICTURES");
			return !E || std::string(E).find(Which) != std::string::npos;
		}

		struct Draw
		{
			const LandField::Mesh* M = nullptr;
			const Leaves* Cards = nullptr; // a leaf card mesh: cut out by the picture's alpha, both faces drawn
			bool bPlaced = false;
			double X = 0.0, Y = 0.0, Z = 0.0, C = 1.0, S = 0.0, Scale = 1.0, ScaleZ = 1.0;
			float Tint[3] = { 1.0f, 1.0f, 1.0f };
			uint8_t Mat = MatGround;
		};

		struct Target
		{
			int W = 0, H = 0;
			bool bDepthOnly = false;
			std::vector<float> Depth, Albedo, Normal;
			std::vector<uint8_t> Mat;
			/** Under the game's surface: the ground's two numbers a sample (Mesh::Uvs, ploughland's furrows), between its corners. */
			std::vector<float> Plough;
			void Init(int InW, int InH, bool bOnlyDepth)
			{
				W = InW;
				H = InH;
				bDepthOnly = bOnlyDepth;
				const size_t N = static_cast<size_t>(W) * H;
				Depth.assign(N, Empty);
				if (!bDepthOnly)
				{
					Albedo.assign(N * 3, 0.0f);
					Normal.assign(N * 3, 0.0f);
					Mat.assign(N, 0);
					if (SurfaceAsked().bSurfaceOn) Plough.assign(N * 2, 0.0f);
				}
			}
		};

		struct Vtx
		{
			double X, Y, Z; // view space
			float C[3], N[3];
			float U = 0.0f, V = 0.0f;
		};

		int Threads() { return std::max(1, static_cast<int>(std::thread::hardware_concurrency())); }

		constexpr int Strip = 8;

		/** One triangle into the rows this thread owns (strips of Strip rows dealt round the threads). */
		void Fill(Target& T, const View& V, const Vtx& A, const Vtx& B, const Vtx& C, uint8_t Mat, bool bCull, int Me, int Of, const Leaves* Cards = nullptr)
		{
			if (Cards) bCull = false; // (a card is drawn from both sides)
			double AX, AY, BX, BY, CX, CY;
			V.ToScreen(A.X, A.Y, A.Z, AX, AY);
			V.ToScreen(B.X, B.Y, B.Z, BX, BY);
			V.ToScreen(C.X, C.Y, C.Z, CX, CY);
			const double Area = (BX - AX) * (CY - AY) - (BY - AY) * (CX - AX);
			if (std::fabs(Area) < 1e-12 || (bCull && Area >= 0.0)) return;
			const int X0 = std::max(0, static_cast<int>(std::floor(std::min({ AX, BX, CX }) - 0.5)));
			const int X1 = std::min(T.W - 1, static_cast<int>(std::ceil(std::max({ AX, BX, CX }) - 0.5)));
			const int Y0 = std::max(0, static_cast<int>(std::floor(std::min({ AY, BY, CY }) - 0.5)));
			const int Y1 = std::min(T.H - 1, static_cast<int>(std::ceil(std::max({ AY, BY, CY }) - 0.5)));
			if (X0 > X1 || Y0 > Y1) return;
			const double Inv = 1.0 / Area;
			const bool bPersp = !V.bOrtho;
			const double IA = bPersp ? 1.0 / A.Z : 1.0, IB = bPersp ? 1.0 / B.Z : 1.0, IC = bPersp ? 1.0 / C.Z : 1.0;
			for (int Y = Y0; Y <= Y1; ++Y)
			{
				if ((Y / Strip) % Of != Me)
				{
					Y = (Y / Strip) * Strip + Strip - 1;
					continue;
				}
				const double PY = Y + 0.5;
				for (int X = X0; X <= X1; ++X)
				{
					const double PX = X + 0.5;
					const double L0 = ((BX - PX) * (CY - PY) - (BY - PY) * (CX - PX)) * Inv;
					const double L1 = ((CX - PX) * (AY - PY) - (CY - PY) * (AX - PX)) * Inv;
					const double L2 = 1.0 - L0 - L1;
					if (L0 < 0.0 || L1 < 0.0 || L2 < 0.0) continue;
					double Z, W0 = L0, W1 = L1, W2 = L2;
					if (bPersp)
					{
						const double IZ = L0 * IA + L1 * IB + L2 * IC;
						Z = 1.0 / IZ;
						W0 = L0 * IA * Z;
						W1 = L1 * IB * Z;
						W2 = L2 * IC * Z;
					}
					else Z = L0 * A.Z + L1 * B.Z + L2 * C.Z;
					const size_t K = static_cast<size_t>(Y) * T.W + X;
					if (Z >= T.Depth[K]) continue;
					const uint8_t* Leaf = nullptr;
					if (Cards)
					{
						Leaf = Cards->At(W0 * A.U + W1 * B.U + W2 * C.U, W0 * A.V + W1 * B.V + W2 * C.V);
						if (Leaf[3] < 128) continue; // (between the leaves)
					}
					T.Depth[K] = static_cast<float>(Z);
					if (T.bDepthOnly) continue;
					for (int Q = 0; Q < 3; ++Q)
					{
						const double Paint = W0 * A.C[Q] + W1 * B.C[Q] + W2 * C.C[Q];
						T.Albedo[K * 3 + Q] = static_cast<float>(Leaf ? Paint * (2.0 / 255.0) * Leaf[Q] : Paint);
						T.Normal[K * 3 + Q] = static_cast<float>(W0 * A.N[Q] + W1 * B.N[Q] + W2 * C.N[Q]);
					}
					T.Mat[K] = Mat;
					if (Mat == MatSoil && !T.Plough.empty())
					{
						T.Plough[K * 2] = static_cast<float>(W0 * A.U + W1 * B.U + W2 * C.U);
						T.Plough[K * 2 + 1] = static_cast<float>(W0 * A.V + W1 * B.V + W2 * C.V);
					}
				}
			}
		}

		Vtx Between(const Vtx& A, const Vtx& B, double Near)
		{
			const double T = (Near - A.Z) / (B.Z - A.Z);
			Vtx O;
			O.X = A.X + (B.X - A.X) * T;
			O.Y = A.Y + (B.Y - A.Y) * T;
			O.Z = Near;
			for (int Q = 0; Q < 3; ++Q)
			{
				O.C[Q] = static_cast<float>(A.C[Q] + (B.C[Q] - A.C[Q]) * T);
				O.N[Q] = static_cast<float>(A.N[Q] + (B.N[Q] - A.N[Q]) * T);
			}
			O.U = static_cast<float>(A.U + (B.U - A.U) * T);
			O.V = static_cast<float>(A.V + (B.V - A.V) * T);
			return O;
		}

		/** Every draw into the target, the rows shared among the threads; triangles cut at the near plane. */
		void Rasterize(Target& T, const View& V, const std::vector<Draw>& Draws, bool bCull)
		{
			const int Of = Threads();
			auto Work = [&](int Me) {
				std::vector<Vtx> P;
				for (const Draw& D : Draws)
				{
					const LandField::Mesh& M = *D.M;
					const size_t N = M.Points();
					P.resize(N);
					for (size_t K = 0; K < N; ++K)
					{
						V3 W{ M.Positions[K * 3], M.Positions[K * 3 + 1], M.Positions[K * 3 + 2] };
						V3 Nm{ M.Normals[K * 3], M.Normals[K * 3 + 1], M.Normals[K * 3 + 2] };
						if (D.bPlaced)
						{
							const double LX = W.X * D.Scale, LY = W.Y * D.Scale;
							W = { D.X + LX * D.C - LY * D.S, D.Y + LX * D.S + LY * D.C, D.Z + W.Z * D.ScaleZ };
							const double NX = Nm.X / D.Scale, NY = Nm.Y / D.Scale;
							Nm = Unit({ NX * D.C - NY * D.S, NX * D.S + NY * D.C, Nm.Z / D.ScaleZ });
						}
						const V3 Q = V.ToView(W);
						Vtx& O = P[K];
						O.X = Q.X;
						O.Y = Q.Y;
						O.Z = Q.Z;
						if (D.Cards && M.Uvs.size() >= 2 * N)
						{
							O.U = M.Uvs[K * 2];
							O.V = M.Uvs[K * 2 + 1];
						}
						else if (D.Mat == MatSoil) // (a ground tile under the game's surface: its furrows' two numbers, or none)
						{
							const bool bWay = M.Uvs.size() == 2 * N;
							O.U = bWay ? M.Uvs[K * 2] : 0.0f;
							O.V = bWay ? M.Uvs[K * 2 + 1] : 0.0f;
						}
						if (!T.bDepthOnly)
						{
							const LandField::Rgb8 Cl = M.Colours[K];
							O.C[0] = LinearOf[Cl.R] * D.Tint[0];
							O.C[1] = LinearOf[Cl.G] * D.Tint[1];
							O.C[2] = LinearOf[Cl.B] * D.Tint[2];
							if (D.Mat == MatSoil) // (M_LandSurface reads the paint as the mesh holds it, and makes it linear itself)
								for (int Ch = 0; Ch < 3; ++Ch) O.C[Ch] = (Ch == 0 ? Cl.R : Ch == 1 ? Cl.G : Cl.B) / 255.0f;
							O.N[0] = static_cast<float>(Nm.X);
							O.N[1] = static_cast<float>(Nm.Y);
							O.N[2] = static_cast<float>(Nm.Z);
						}
					}
					for (size_t Tr = 0; Tr + 2 < M.Triangles.size(); Tr += 3)
					{
						const Vtx& A = P[M.Triangles[Tr]];
						const Vtx& B = P[M.Triangles[Tr + 1]];
						const Vtx& C = P[M.Triangles[Tr + 2]];
						if (V.bOrtho)
						{
							Fill(T, V, A, B, C, D.Mat, bCull, Me, Of, D.Cards);
							continue;
						}
						const bool bA = A.Z >= V.Near, bB = B.Z >= V.Near, bC = C.Z >= V.Near;
						if (bA && bB && bC)
						{
							Fill(T, V, A, B, C, D.Mat, bCull, Me, Of, D.Cards);
							continue;
						}
						if (!bA && !bB && !bC) continue;
						// Cut at the near plane: what is left is a triangle or a quad.
						const Vtx* In[3] = { &A, &B, &C };
						Vtx Poly[4];
						int NP = 0;
						for (int K = 0; K < 3; ++K)
						{
							const Vtx& Cur = *In[K];
							const Vtx& Nxt = *In[(K + 1) % 3];
							const bool bCur = Cur.Z >= V.Near, bNxt = Nxt.Z >= V.Near;
							if (bCur) Poly[NP++] = Cur;
							if (bCur != bNxt) Poly[NP++] = Between(Cur, Nxt, V.Near);
						}
						for (int K = 1; K + 1 < NP; ++K) Fill(T, V, Poly[0], Poly[K], Poly[K + 1], D.Mat, bCull, Me, Of, D.Cards);
					}
				}
			};
			std::vector<std::thread> Pool;
			for (int K = 0; K < Of; ++K) Pool.emplace_back(Work, K);
			for (std::thread& Th : Pool) Th.join();
		}

		/** The draws a view needs: tiles (near or far level by distance from the eye), the ring, buildings, instances, clutter. */
		struct Scene
		{
			std::vector<Draw> Draws;
			size_t Tris = 0;
		};

		Material MatOf(LandField::Base B, const LandField::BaseMesh& M)
		{
			using LandField::Base;
			switch (B)
			{
			case Base::ShrubA:
			case Base::ShrubB:
			case Base::ShrubC:
			case Base::Bramble:
			case Base::Oak:
			case Base::Elm:
			case Base::Ash:
			case Base::Beech:
			case Base::Pollard:
			case Base::Apple: return MatLeaves;
			case Base::Tuft:
			case Base::TallGrass:
			case Base::Wheat:
			case Base::Poppies:
			case Base::Flowers:
			case Base::Fern:
			case Base::Rosette:
			case Base::Stubble:
			case Base::Sward: return MatBlades;
			default: return M.Paint == 1 ? MatLeaves : M.Paint == 2 ? MatBlades : MatGround; // (a region's kit and clutter: its own)
			}
		}

		/** The words on trial (Field::Trial), for the trees' crowns: set by Paint. */
		std::string TrialWords;

		/** A distance in metres from the environment, or -1 where it is not set. */
		double Metres(const char* Name)
		{
			const char* E = std::getenv(Name);
			return E ? std::atof(E) : -1.0;
		}

		/** A tree, as the game's LayThings tells one (the bocage's and the steppe's trees, and a region's kit trees). */
		bool IsTree(LandField::Base B, const LandField::BaseMesh& Bm)
		{
			using LandField::Base;
			return B == Base::Oak || B == Base::Elm || B == Base::Ash || B == Base::Beech || B == Base::Pollard || B == Base::Apple || B == Base::Birch || Bm.bTree
				|| Bm.Class == LandField::BaseMesh::Draw::Tree;
		}

		/** A tree's crown as the game draws it far off (LandBattle's LayThings): CrownOf its last level, or on trial
		 * "farcrowns" CrownLobed; made once a kind. */
		const LandField::Mesh& CrownMesh(LandField::Base B, const LandField::BaseMesh& Bm)
		{
			static std::map<int, LandField::Mesh> Made;
			const auto Found = Made.find(static_cast<int>(B));
			if (Found != Made.end()) return Found->second;
			LandField::Field F;
			F.Trial = TrialWords;
			const LandField::Mesh& Last = Bm.Far.Empty() ? Bm.Near : Bm.Far;
			LandField::Mesh& M = Made[static_cast<int>(B)];
			M = !F.Trial.empty() && F.Tried("farcrowns") ? LandField::CrownLobed(Last) : LandField::CrownOf(Last);
			std::printf("  crown of %s: %zu triangles (%s)\n", Bm.Name.c_str(), M.Tris(), !F.Trial.empty() && F.Tried("farcrowns") ? "CrownLobed" : "CrownOf");
			return M;
		}

		/**
		 * LAND_FIELD_TREE_FAR_FROM (metres): the trees plain (their far level) from there, as the game's -TheaterLandTreeFarFrom=
		 * (unset: each its own FarFrom, as before). LAND_FIELD_CROWN_FROM: past there a tree is drawn as its crown, as the
		 * game draws it, casting no shadow (unset: never, as before).
		 */
		void Gather(Scene& S, const LandField::Ground& G, const View& Cull, const V3& Eye, double NearTiles, bool bAllNear, const std::vector<LandField::Placed>& Clutter, bool bShadows = false)
		{
			static const double TreeFarFrom = Metres("LAND_FIELD_TREE_FAR_FROM"), CrownFrom = Metres("LAND_FIELD_CROWN_FROM");
			auto Add = [&S](const LandField::Mesh& M, uint8_t Mat) {
				if (M.Empty()) return;
				Draw D;
				D.M = &M;
				D.Mat = Mat;
				S.Draws.push_back(D);
				S.Tris += M.Tris();
			};
			auto Boxed = [&Cull](const LandField::Tile& T) {
				const V3 C{ 0.5 * (T.X0 + T.X1), 0.5 * (T.Y0 + T.Y1), 0.5 * (T.ZMin + T.ZMax) };
				const double R = 0.5 * std::sqrt((T.X1 - T.X0) * (T.X1 - T.X0) + (T.Y1 - T.Y0) * (T.Y1 - T.Y0) + (T.ZMax - T.ZMin) * (T.ZMax - T.ZMin));
				return Cull.Sees(C, R + 30.0);
			};
			// (the ground tiles themselves on the game's ground surface when it is asked for; their built pieces never)
			const uint8_t TileMat = SurfaceAsked().bSurfaceOn ? MatSoil : MatGround;
			for (const LandField::Tile& T : G.Tiles())
			{
				if (!Boxed(T)) continue;
				const double DX = std::max({ 0.0, T.X0 - Eye.X, Eye.X - T.X1 }), DY = std::max({ 0.0, T.Y0 - Eye.Y, Eye.Y - T.Y1 });
				Add(bAllNear || std::hypot(DX, DY) < NearTiles ? T.Near : T.Far, TileMat);
				Add(T.Built, MatGround);
			}
			for (const LandField::Tile& T : G.Ring())
				if (Boxed(T)) Add(T.Near, TileMat);
			auto Place = [&](const LandField::Placed& P) {
				const LandField::BaseMesh& Bm = G.BaseOf(P.Kind);
				const double Dist = bAllNear ? 1e9 : std::hypot(P.X - Eye.X, P.Y - Eye.Y, P.Z - Eye.Z);
				const bool bTree = (TreeFarFrom > 0.0 || CrownFrom > 0.0) && IsTree(P.Kind, Bm);
				const bool bNear = bTree && TreeFarFrom > 0.0 ? Dist < TreeFarFrom : Dist < Bm.FarFrom;
				const bool bCrown = bTree && !bNear && CrownFrom > 0.0 && Dist >= CrownFrom;
				if (bCrown && bShadows) return;
				const LandField::Mesh& M = bCrown ? CrownMesh(P.Kind, Bm) : bNear ? Bm.Near : Bm.Far;
				if (M.Empty()) return;
				const double R = std::max(static_cast<double>(Bm.Radius) * P.Scale, 0.5 * Bm.Height * P.ScaleZ) + 0.5;
				if (!Cull.Sees({ P.X, P.Y, P.Z + 0.5 * Bm.Height * P.ScaleZ }, R)) return;
				// (near, its leaves: on cards where the picture was read, else the sprigs, as the game draws them)
				if (bNear && (!Bm.Cards.Empty() || !Bm.Sprigs.Empty()))
				{
					const bool bCards = LeafPicture().Ok() && !Bm.Cards.Empty();
					Draw L;
					L.M = bCards ? &Bm.Cards : &Bm.Sprigs;
					L.Cards = bCards ? &LeafPicture() : nullptr;
					L.bPlaced = true;
					L.X = P.X;
					L.Y = P.Y;
					L.Z = P.Z;
					L.C = std::cos(P.Yaw);
					L.S = std::sin(P.Yaw);
					L.Scale = P.Scale;
					L.ScaleZ = P.ScaleZ;
					L.Tint[0] = P.Tint.R / 128.0f;
					L.Tint[1] = P.Tint.G / 128.0f;
					L.Tint[2] = P.Tint.B / 128.0f;
					L.Mat = MatLeaves;
					if (!L.M->Empty())
					{
						S.Draws.push_back(L);
						S.Tris += L.M->Tris();
					}
				}
				Draw D;
				D.M = &M;
				D.bPlaced = true;
				D.X = P.X;
				D.Y = P.Y;
				D.Z = P.Z;
				D.C = std::cos(P.Yaw);
				D.S = std::sin(P.Yaw);
				D.Scale = P.Scale;
				D.ScaleZ = P.ScaleZ;
				D.Tint[0] = P.Tint.R / 128.0f;
				D.Tint[1] = P.Tint.G / 128.0f;
				D.Tint[2] = P.Tint.B / 128.0f;
				D.Mat = MatOf(P.Kind, G.BaseOf(P.Kind));
				S.Draws.push_back(D);
				S.Tris += M.Tris();
			};
			for (int B = 0; B < static_cast<int>(LandField::Base::Count); ++B)
				for (const LandField::Placed& P : G.Instances(static_cast<LandField::Base>(B))) Place(P);
			for (const LandField::Placed& P : Clutter) Place(P);
		}

		// ------------------------------------------------------------------ light

		struct Light
		{
			V3 Sun;          // toward the sun
			double SunCol[3];
			double Sky[3];   // the sky's light on ground facing up
			double Bounce[3];
			double HazeCol[3], HazeSun[3];
			double Beta = 3.912 / 3500.0;
			double Exposure = 0.85;
			double Cover = 0.45, Base = 900.0;
			/** A grey day's softness (Sky::Overcast): shade lifted toward light by this share, the sky a deck. */
			double Soft = 0.0;
			/** The deck's brightness under rain or snow (Sky::SkyTimes). */
			double DeckTimes = 1.0;
			// (the clouds' shadows' picture moved so its clear disc lies over the field's middle at the start, as the
			// game's LandBattle lays M_LandCloudShade's "Anchor": U along the field's y, V along its x, in repeats)
			double AnchorU = 0.0, AnchorV = 0.0;
		};

		Light Morning(const Sky& Sk)
		{
			Light L;
			const double Decl = 23.44 * Pi / 180.0 * std::sin(2.0 * Pi * (284.0 + Sk.Day) / 365.0);
			const double Solar = Sk.Hour - Sk.Zone + Sk.Lon / 15.0;
			const double HourAngle = (Solar - 12.0) * 15.0 * Pi / 180.0, Lat = Sk.Lat * Pi / 180.0;
			const double SinAlt = std::sin(Lat) * std::sin(Decl) + std::cos(Lat) * std::cos(Decl) * std::cos(HourAngle);
			const double Alt = std::asin(std::clamp(SinAlt, -1.0, 1.0));
			double CosAz = (std::sin(Decl) - SinAlt * std::sin(Lat)) / std::max(1e-9, std::cos(Alt) * std::cos(Lat));
			double Az = std::acos(std::clamp(CosAz, -1.0, 1.0));
			if (HourAngle > 0.0) Az = 2.0 * Pi - Az;
			double Up = std::max(Alt, 4.0 * Pi / 180.0);
			// LAND_FIELD_SUN="AZ ALT": the sun put where asked (degrees from north, clockwise; degrees up), as to light the
			// furrows across or along; unset, the battle's own morning sun as before
			if (const char* E = std::getenv("LAND_FIELD_SUN"))
			{
				double AzDeg = 0.0, UpDeg = 0.0;
				if (std::sscanf(E, "%lf %lf", &AzDeg, &UpDeg) == 2)
				{
					Az = AzDeg * Pi / 180.0;
					Up = std::clamp(UpDeg, 2.0, 89.0) * Pi / 180.0;
					std::printf("  the sun put at %.0f degrees from north, %.0f degrees up (LAND_FIELD_SUN)\n", AzDeg, UpDeg);
				}
			}
			L.Sun = { std::sin(Az) * std::cos(Up), std::cos(Az) * std::cos(Up), std::sin(Up) };
			std::printf("  sun %.1f degrees up, %.0f degrees from north (solar time %.2f h)\n", Alt * 180.0 / Pi, Az * 180.0 / Pi, Solar);
			// A clear July morning with some cumulus: a warm sun, a blue sky, a pale blue-grey haze turning warm toward the sun.
			const double Sun[3] = { 1.0, 0.9, 0.76 }, Sky[3] = { 0.36, 0.46, 0.66 }, Bounce[3] = { 0.24, 0.24, 0.16 };
			for (int C = 0; C < 3; ++C)
			{
				L.SunCol[C] = 2.1 * Sun[C];
				L.Sky[C] = 0.62 * Sky[C];
				L.Bounce[C] = 0.3 * Bounce[C];
			}
			const double Haze[3] = { 0.55, 0.62, 0.72 }, Glow[3] = { 0.86, 0.80, 0.68 };
			for (int C = 0; C < 3; ++C)
			{
				L.HazeCol[C] = Haze[C] * Sk.HazeTint[C];
				L.HazeSun[C] = Glow[C] * (0.5 + 0.5 * Sk.HazeTint[C]);
			}
			L.Beta *= Sk.HazeThick;
			L.Cover = Sk.Cover;
			L.Base = Sk.Base;
			// (rain or snow, release 2's G10: a grey day under a deck, as the game lays it for the weather. The sun's
			// light mostly gone into the cloud and its shadows soft; the sky's light greyer and stronger; the haze
			// thicker and drawn toward the weather's grey; the lamps a little duller)
			if (Sk.Overcast > 0.0)
			{
				const double O = std::clamp(Sk.Overcast, 0.0, 1.0);
				const double Grey[3] = { 0.52, 0.54, 0.57 };
				for (int C = 0; C < 3; ++C)
				{
					L.SunCol[C] *= 1.0 - 0.82 * O;
					L.Sky[C] = (L.Sky[C] + (0.62 * Grey[C] - L.Sky[C]) * O) * (1.0 + 0.9 * O);
					L.HazeSun[C] = L.HazeSun[C] + (L.HazeCol[C] - L.HazeSun[C]) * O;
				}
				L.Cover = 0.0;
				L.Soft = O;
			}
			for (int C = 0; C < 3; ++C)
			{
				L.HazeCol[C] += (Sk.FallHaze[C] - L.HazeCol[C]) * std::clamp(Sk.HazeToward, 0.0, 1.0);
				L.HazeSun[C] += (Sk.FallHaze[C] - L.HazeSun[C]) * std::clamp(Sk.HazeToward, 0.0, 1.0);
				L.SunCol[C] *= Sk.LightTimes;
				L.Sky[C] *= Sk.LightTimes;
				L.Bounce[C] *= Sk.LightTimes;
			}
			L.Beta *= std::max(1.0, Sk.HazeTimes);
			L.DeckTimes = std::clamp(Sk.SkyTimes, 0.0, 1.0);
			return L;
		}

		/** The haze's colour along a ray: bluish away from the sun, warmer and brighter toward it. */
		void HazeOf(const Light& L, const V3& Ray, double Out[3])
		{
			const double Toward = std::pow(std::max(0.0, Dot(Ray, L.Sun)), 3.0);
			for (int C = 0; C < 3; ++C) Out[C] = L.HazeCol[C] + (L.HazeSun[C] - L.HazeCol[C]) * Toward;
		}

		void SkyOf(const Light& L, const V3& Eye, const V3& Ray, double Out[3])
		{
			double Haze[3];
			HazeOf(L, Ray, Haze);
			const double Up = std::max(0.0, Ray.Z);
			const double Zenith[3] = { 0.14, 0.26, 0.52 };
			const double T = std::pow(Up, 0.45);
			for (int C = 0; C < 3; ++C) Out[C] = Haze[C] + (Zenith[C] - Haze[C]) * T;
			// The sun's glow and disc.
			const double Cos = Dot(Ray, L.Sun);
			const double Glow = 0.5 * std::pow(std::max(0.0, Cos), 40.0) + (Cos > 0.99996 ? 30.0 : 0.0);
			for (int C = 0; C < 3; ++C) Out[C] += Glow * L.SunCol[C] / 2.1;
			// (a grey day: the deck over everything, lighter overhead, into the haze at the horizon)
			if (L.Soft > 0.0)
			{
				const double Deck[3] = { 0.60, 0.62, 0.66 };
				const double Over = std::pow(std::clamp(Ray.Z / 0.25, 0.0, 1.0), 0.7);
				for (int C = 0; C < 3; ++C)
				{
					const double Grey = Haze[C] + (Deck[C] * L.DeckTimes * (0.85 + 0.25 * Up) - Haze[C]) * Over;
					Out[C] += (Grey - Out[C]) * L.Soft;
				}
				return;
			}
			// Cumulus on a deck at the cloud base: soft, lit on top, greyer underneath, lost in the haze far off.
			if (Ray.Z > 0.01 && L.Cover > 0.0)
			{
				const double Dist = (L.Base - Eye.Z) / Ray.Z;
				const V3 P = Eye + Ray * Dist;
				double N = 0.0, W = 1.0, Sum = 0.0;
				for (int O = 0; O < 4; ++O)
				{
					const double F = std::pow(2.0, O) / 1400.0;
					N += W * Noise(P.X * F + O * 13.1, P.Y * F - O * 7.7, 0xC10D + O);
					Sum += W;
					W *= 0.5;
				}
				N /= Sum;
				const double Edge = 1.0 - L.Cover;
				const double Cloud = std::clamp((N - Edge * 0.85) / 0.12, 0.0, 1.0);
				const double Shade = std::clamp((N - Edge * 0.85) / 0.35, 0.0, 1.0);
				const double Far = 1.0 - std::exp(-Dist / 14000.0);
				const double Lit[3] = { 1.05, 1.02, 0.96 }, Base[3] = { 0.58, 0.6, 0.66 };
				for (int C = 0; C < 3; ++C)
				{
					const double Cl = Lit[C] + (Base[C] - Lit[C]) * Shade * 0.8;
					const double Seen = Cl + (Haze[C] - Cl) * Far;
					Out[C] = Out[C] + (Seen - Out[C]) * Cloud * (1.0 - Far * 0.6);
				}
			}
		}

		/** A shadow map: the sun's view of a square and its depths. */
		struct Shadow
		{
			View V;
			Target T;
			double Texel = 0.1, Soft = 1.5;
		};

		const double Taps[12][2] = { { -0.326, -0.406 }, { -0.840, -0.074 }, { -0.696, 0.457 }, { -0.203, 0.621 }, { 0.962, -0.195 }, { 0.473, -0.480 },
									 { 0.519, 0.767 },   { 0.185, -0.893 }, { 0.507, 0.064 },  { 0.896, 0.412 },  { -0.322, -0.933 }, { -0.792, -0.598 } };

		/** How lit a place is (0 to 1): the finest map that holds it, a dozen taps round it for a soft edge. -1: no map holds it.
		 * bPlane (a solid face, not a leaf or a blade): each tap is tried at the depth the face's own plane has there, so a
		 * wall the sun grazes is not striped by its own depths a tap or two along (the desert's limewashed walls,
		 * 24 November); the run of depth held to eight texels a texel, so the sun does not leak far under what stands on
		 * a face it only skims. */
		double LitBy(const Shadow& S, const V3& P, const V3& N, double Lean, bool bPlane)
		{
			const V3 Q = S.V.ToView(P + N * (S.Texel * (1.5 + Lean)) + S.V.Fwd * -(S.Texel * 0.6));
			double SX, SY;
			S.V.ToScreen(Q.X, Q.Y, Q.Z, SX, SY);
			const double Margin = S.Soft + 2.0;
			if (SX < Margin || SY < Margin || SX > S.T.W - Margin || SY > S.T.H - Margin) return -1.0;
			double GX = 0.0, GY = 0.0;
			const double Away = Dot(N, S.V.Fwd);
			if (bPlane && Away < -1e-3)
			{
				GX = -Dot(N, S.V.Right) / Away * S.Texel;
				GY = Dot(N, S.V.Up) / Away * S.Texel;
				const double Run = std::hypot(GX, GY), Most = 8.0 * S.Texel;
				if (Run > Most)
				{
					GX *= Most / Run;
					GY *= Most / Run;
				}
			}
			int Lit = 0;
			for (const auto& Tp : Taps)
			{
				const int X = static_cast<int>(SX + Tp[0] * S.Soft), Y = static_cast<int>(SY + Tp[1] * S.Soft);
				const double Z = Q.Z + GX * (X + 0.5 - SX) + GY * (Y + 0.5 - SY);
				if (Z <= S.T.Depth[static_cast<size_t>(Y) * S.T.W + X] + S.Texel * 0.5) ++Lit;
			}
			return Lit / 12.0;
		}

		/** Lights, hazes and tones the samples, then averages them down two to one into the picture's bytes. */
		std::vector<uint8_t> Shade(const Target& T, const View& V, const Light& L, const std::vector<const Shadow*>& Shadows, bool bHaze, int Down,
			const std::vector<float>* OverLight = nullptr, const std::vector<float>* OverCover = nullptr)
		{
			const int W = T.W, H = T.H;
			std::vector<float> Lin(static_cast<size_t>(W) * H * 3);
			auto Work = [&](int Me, int Of) {
				for (int Y = Me; Y < H; Y += Of)
					for (int X = 0; X < W; ++X)
					{
						const size_t K = static_cast<size_t>(Y) * W + X;
						double Out[3];
						if (T.Depth[K] == Empty)
						{
							SkyOf(L, V.Eye, V.RayOf(X + 0.5, Y + 0.5), Out);
						}
						else
						{
							const V3 P = V.FromSample(X + 0.5, Y + 0.5, T.Depth[K]);
							V3 N = Unit({ T.Normal[K * 3], T.Normal[K * 3 + 1], T.Normal[K * 3 + 2] });
							uint8_t Mat = T.Mat[K];
							const V3 Ray = Unit(P - V.Eye);
							// (the paint, and the normal the light sees: a ground tile under the game's surface has both made here)
							float Alb[3] = { T.Albedo[K * 3], T.Albedo[K * 3 + 1], T.Albedo[K * 3 + 2] };
							V3 LitNormal = N;
							double Raw[3] = { -1.0, -1.0, -1.0 };
							if (Mat == MatSoil)
							{
								for (int C = 0; C < 3; ++C) Raw[C] = Alb[C];
								V3 Major, StepR, StepD;
								double Minor = 0.0;
								Footprint(V, X + 0.5, Y + 0.5, Down, P, N, Major, Minor, &StepR, &StepD);
								const float Way[2] = { T.Plough.empty() ? 0.0f : T.Plough[K * 2], T.Plough.empty() ? 0.0f : T.Plough[K * 2 + 1] };
								SurfaceShade(SurfaceAsked(), Raw, N, P, T.Depth[K], Major, Minor, Way, StepR, StepD, Alb, LitNormal);
								Mat = MatGround;
							}
							// A face seen from behind (a blade's back, a leaf turned away) is lit as its front.
							// (Not a blade or a crop's leaf lit from above, its normal leaning up on both faces: the game draws
							// whichever face looks at the eye with that normal, so a tall crop's leaf over the eye is lit, not
							// black; the art director's black squares on the near gaoliang, 23 November.)
							if (Mat != MatGround && Dot(N, Ray) > 0.0 && !V.bOrtho && !(Mat == MatBlades && N.Z > 0.5)) N = N * -1.0;
							if (Raw[0] < 0.0) LitNormal = N; // (not under the game's surface: the light sees the face's own normal, turned as above)
							double Lit = 1.0;
							for (const Shadow* S : Shadows)
							{
								const double Got = LitBy(*S, P, N, Mat == MatLeaves ? 2.0 : 0.0, Mat == MatGround);
								if (Got >= 0.0)
								{
									Lit = Got;
									break;
								}
							}
							Lit += (1.0 - Lit) * 0.8 * L.Soft; // (a grey day's shadows soft)
							// (the game's clouds' shadows: where this point's sun ray meets the clouds' base, as M_LandCloudShade lays
							// them: how far inside a cloud (red, metres either side of 0.5 over 400), the edge blurred over 30 m,
							// the clouds the day's cover keeps (blue under four fifths of the cover, at most 0.4, over the 0.41 the
							// picture's clouds shade in all); the field's x is the world's Y)
							const Air& Ai = AirAsked();
							if (Ai.Dark >= 0.0 && L.Cover >= 0.05)
							{
								const double Climb = (L.Base - P.Z) / std::max(L.Sun.Z, 0.15);
								const double QX = P.X + L.Sun.X * Climb, QY = P.Y + L.Sun.Y * Climb;
								const double CU = QY / 4000.0 + L.AnchorU, CV = QX / 4000.0 + L.AnchorV;
								const double Inside = (AirAt(CU, CV, 0) - 0.5) * 800.0;
								const double Kept = std::clamp((std::min(0.8 * L.Cover, 0.4) / 0.41 - AirAt(CU, CV, 2)) * 255.0, 0.0, 1.0);
								const double Ramp = std::clamp(Inside / 30.0 + 0.5, 0.0, 1.0);
								Lit *= 1.0 - Ramp * Ramp * (3.0 - 2.0 * Ramp) * Kept * (1.0 - Ai.Dark);
							}
							double Swept = 1.0;
							if (Ai.Sheen > 0.0 && (Mat == MatGround || Mat == MatBlades))
							{
								// (a plant's paint only, never bare earth, sand or snow: its green well over its blue; the grass at
								// LAND_FIELD_SHEEN, the game's 0.06, ripe grain, its red well over its green, at 11 of its 6)
								// (under the game's surface, the paint itself, as the material reads it)
								const double SR = Raw[0] >= 0.0 ? Raw[0] : std::pow(std::max(0.0, static_cast<double>(Alb[0])), 1.0 / 2.2);
								const double SG = Raw[1] >= 0.0 ? Raw[1] : std::pow(std::max(0.0, static_cast<double>(Alb[1])), 1.0 / 2.2), SB = Raw[2] >= 0.0 ? Raw[2] : std::pow(std::max(0.0, static_cast<double>(Alb[2])), 1.0 / 2.2);
								const double Plant = std::clamp((SG - SB) / (SG + 0.02) * 5.0 - 1.5, 0.0, 1.0) * (Mat == MatGround ? std::clamp((N.Z - 0.55) * 4.0, 0.0, 1.0) : 1.0);
								const double Ripe = std::clamp((SR / (SG + 0.01) - 0.9) * 3.3, 0.0, 1.0);
								Swept = 1.0 + Plant * Ai.Sheen * (1.0 + Ripe * 5.0 / 6.0) * 2.0 * (AirAt(P.Y / 150.0, P.X / 150.0, 1) - 0.5);
							}
							const double NL = Dot(LitNormal, L.Sun);
							// Leaves and blades let light through and scatter it: their lit side wraps round, and their shade is lighter.
							double Diffuse = Mat == MatGround ? std::max(0.0, NL) : std::clamp((NL + 0.45) / 1.45, 0.0, 1.0) * (Mat == MatLeaves ? 0.9 : 0.95);
							const double SkyK = Mat == MatGround ? 0.55 + 0.45 * LitNormal.Z : 0.7 + 0.3 * LitNormal.Z;
							const double BounceK = 0.5 - 0.5 * LitNormal.Z;
							for (int C = 0; C < 3; ++C)
							{
								const double A = Alb[C] * Swept;
								Out[C] = A * (L.SunCol[C] * Diffuse * Lit + L.Sky[C] * SkyK + L.Bounce[C] * BounceK);
							}
							if (bHaze)
							{
								const double D = std::sqrt(Dot(P - V.Eye, P - V.Eye));
								const double Keep = std::exp(-L.Beta * D);
								double Haze[3];
								HazeOf(L, Ray, Haze);
								for (int C = 0; C < 3; ++C) Out[C] = Out[C] * Keep + Haze[C] / L.Exposure * 0.82 * (1.0 - Keep);
							}
						}
						// (what falls over the picture: the drops in the haze's colour along the ray, over what lies behind)
						if (OverCover && (*OverCover)[K] > 0.0f)
						{
							double Haze[3];
							HazeOf(L, V.RayOf(X + 0.5, Y + 0.5), Haze);
							const double Hide = std::clamp(static_cast<double>((*OverCover)[K]), 0.0, 1.0), Add = (*OverLight)[K];
							for (int C = 0; C < 3; ++C) Out[C] = Out[C] * (1.0 - Hide) + Haze[C] / L.Exposure * 0.82 * Add;
						}
						for (int C = 0; C < 3; ++C) Lin[K * 3 + C] = static_cast<float>(Filmic(Out[C] * L.Exposure));
					}
			};
			const int Of = Threads();
			std::vector<std::thread> Pool;
			for (int K = 0; K < Of; ++K) Pool.emplace_back(Work, K, Of);
			for (std::thread& Th : Pool) Th.join();
			const int PW = W / Down, PH = H / Down;
			std::vector<uint8_t> Pic(static_cast<size_t>(PW) * PH * 3);
			for (int Y = 0; Y < PH; ++Y)
				for (int X = 0; X < PW; ++X)
					for (int C = 0; C < 3; ++C)
					{
						double Sum = 0.0;
						for (int B = 0; B < Down; ++B)
							for (int A = 0; A < Down; ++A) Sum += Lin[(static_cast<size_t>(Y * Down + B) * W + X * Down + A) * 3 + C];
						Pic[(static_cast<size_t>(Y) * PW + X) * 3 + C] = static_cast<uint8_t>(std::lround(ToPicture(Sum / (Down * Down))));
					}
			return Pic;
		}

		bool Save(const std::string& Path, int W, int H, const std::vector<uint8_t>& Pic)
		{
			const std::string Ppm = Path + ".ppm";
			FILE* F = std::fopen(Ppm.c_str(), "wb");
			if (!F) return false;
			std::fprintf(F, "P6\n%d %d\n255\n", W, H);
			std::fwrite(Pic.data(), 1, Pic.size(), F);
			std::fclose(F);
			const std::string Cmd = "convert '" + Ppm + "' '" + Path + "' && rm -f '" + Ppm + "'";
			const bool bOk = std::system(Cmd.c_str()) == 0;
			std::printf("  wrote %s (%d x %d)%s\n", Path.c_str(), W, H, bOk ? "" : " FAILED");
			return bOk;
		}

		double Seconds(std::chrono::steady_clock::time_point Since) { return std::chrono::duration<double>(std::chrono::steady_clock::now() - Since).count(); }

		/** One picture: its shadows (a fine map round FineAt, a wide one round WideAt), its draws, painted and saved. */
		bool Picture(const LandField::Ground& G, const Light& L, const View& V, const V3& FineAt, double FineSize, const V3& WideAt, double WideSize, double ClutterReach, bool bHaze, const std::string& Path,
			const Sky* Falling = nullptr)
		{
			const auto T0 = std::chrono::steady_clock::now();
			std::vector<LandField::Placed> Clutter;
			if (ClutterReach > 0.0) G.Clutter(V.Eye.X - ClutterReach, V.Eye.Y - ClutterReach, V.Eye.X + ClutterReach, V.Eye.Y + ClutterReach, Clutter);
			std::vector<Shadow> Maps(FineSize > 0.0 ? 2 : 1);
			size_t ShadowTris = 0;
			for (size_t K = 0; K < Maps.size(); ++K)
			{
				Shadow& S = Maps[K];
				const bool bFine = FineSize > 0.0 && K == 0;
				const double Size = bFine ? FineSize : WideSize;
				const int N = 4096;
				S.V = FromSun(L.Sun, bFine ? FineAt : WideAt, Size, N);
				S.Texel = Size / N;
				S.Soft = bFine ? 2.5 : 1.3;
				S.T.Init(N, N, true);
				Scene Sc;
				Gather(Sc, G, S.V, V.Eye, V.bOrtho ? 1e9 : 700.0, V.bOrtho, Clutter, true);
				Rasterize(S.T, S.V, Sc.Draws, false);
				ShadowTris += Sc.Tris;
			}
			Target T;
			T.Init(V.W, V.H, false);
			Scene Sc;
			Gather(Sc, G, V, V.Eye, V.bOrtho ? 1e9 : 700.0, V.bOrtho, Clutter);
			Rasterize(T, V, Sc.Draws, true);
			std::vector<const Shadow*> Order;
			for (const Shadow& S : Maps) Order.push_back(&S);
			std::vector<float> OverLight, OverCover;
			if (Falling && Falling->Over)
			{
				Lens Ln;
				const V3 Axes[4] = { V.Eye, V.Fwd, V.Right, V.Up };
				double* Into[4] = { Ln.Eye, Ln.Fwd, Ln.Right, Ln.Up };
				for (int A = 0; A < 4; ++A)
				{
					Into[A][0] = Axes[A].X;
					Into[A][1] = Axes[A].Y;
					Into[A][2] = Axes[A].Z;
				}
				Ln.Tan = V.Tan;
				Ln.W = V.W;
				Ln.H = V.H;
				Ln.PixelsAcross = V.W / 2;
				OverLight.assign(static_cast<size_t>(V.W) * V.H, 0.0f);
				OverCover.assign(static_cast<size_t>(V.W) * V.H, 0.0f);
				Falling->Over(Ln, T.Depth, OverLight, OverCover);
			}
			const std::vector<uint8_t> Pic = Shade(T, V, L, Order, bHaze, 2, OverCover.empty() ? nullptr : &OverLight, OverCover.empty() ? nullptr : &OverCover);
			std::printf("  %zu draws, %.1f million triangles (%.1f million in the shadows), %zu clutter things, painted in %.1f s\n", Sc.Draws.size(), Sc.Tris / 1e6, ShadowTris / 1e6, Clutter.size(), Seconds(T0));
			return Save(Path, V.W / 2, V.H / 2, Pic);
		}

		/**
		 * The soldier's place: in a field south of the village, a couple of metres off a hedgerow's bank that runs toward
		 * the village, with the field open ahead some 70 to 150 m to the next hedge. He looks along the bank, turned a
		 * little out into the field, so the bank runs away from him down one side of the picture. Found from the ground.
		 */
		bool FindEye(const LandField::Ground& G, const LandField::Field& F, V3& Eye, double& Yaw)
		{
			const double TX = F.bTown ? F.TownX : 0.5 * F.Width(), TY = F.bTown ? F.TownY : F.Depth();
			double Best = -1e9;
			for (const LandField::HedgeLine& H : G.Hedges())
			{
				if (H.Kind > 1) continue;
				for (size_t K = 2; K + 2 < H.Points.size(); K += 2)
				{
					const double MX = H.Points[K].first, MY = H.Points[K].second;
					const double DX = H.Points[K + 2].first - H.Points[K - 2].first, DY = H.Points[K + 2].second - H.Points[K - 2].second, DL = std::hypot(DX, DY);
					const double ToX = TX - MX, ToY = TY - MY, ToL = std::hypot(ToX, ToY);
					if (DL < 1e-6 || ToL < 350.0 || ToL > 700.0 || MY > TY - 250.0) continue;
					const double Along = (DX * ToX + DY * ToY) / (DL * ToL);
					if (std::fabs(Along) < 0.9) continue;
					for (int Side = -1; Side <= 1; Side += 2)
					{
						const double EX = MX - DY / DL * Side * 2.6, EY = MY + DX / DL * Side * 2.6;
						const LandField::Surface At = G.At(EX, EY);
						if (At.What != LandField::Drawn::Crop && At.What != LandField::Drawn::Pasture) continue;
						// Along the bank toward the village, turned 11 degrees away from it into the field.
						const double Way = Along > 0.0 ? 1.0 : -1.0;
						const double Look = std::atan2(DY * Way, DX * Way) + Side * Way * 11.0 * Pi / 180.0;
						const double LX = std::cos(Look), LY = std::sin(Look);
						// How far the field runs on ahead.
						double Open = 0.0;
						for (double S = 2.0; S < 220.0; S += 2.0)
						{
							const LandField::Surface Q = G.At(EX + LX * S, EY + LY * S);
							if (Q.What != At.What || Q.Z > At.Z + 0.6) break;
							Open = S;
						}
						const double Score = -std::fabs(Open - 110.0) + (At.What == LandField::Drawn::Crop ? 15.0 : 0.0) - 0.02 * std::fabs(ToL - 500.0);
						if (Score > Best)
						{
							Best = Score;
							Eye = { EX, EY, At.Z + 1.7 };
							Yaw = Look;
						}
					}
				}
			}
			return Best > -1e8;
		}
	}

	bool Paint(const LandField::Ground& G, const LandField::Field& F, const Sky& Sk, const std::string& Key, const std::string& Dir)
	{
		for (int K = 0; K < 256; ++K) LinearOf[K] = static_cast<float>(Linear(K));
		TrialWords = F.Trial;
		Light Lamp = Morning(Sk);
		{
			const double MidX = 0.5 * F.Width(), MidY = 0.5 * F.Depth(), MidZ = G.At(MidX, MidY).Z;
			const double Climb = (Lamp.Base - MidZ) / std::max(Lamp.Sun.Z, 0.15);
			const double UX = MidX + Lamp.Sun.X * Climb, UY = MidY + Lamp.Sun.Y * Climb;
			Lamp.AnchorU = 0.5 - (UY / 4000.0 - std::floor(UY / 4000.0));
			Lamp.AnchorV = 0.5 - (UX / 4000.0 - std::floor(UX / 4000.0));
		}
		const Light L = Lamp;
		bool bOk = true;
		const double TX = F.bTown ? F.TownX : 0.5 * F.Width(), TY = F.bTown ? F.TownY : F.Depth();
		const int W = 3200, H = 1800;
		// (a) A soldier's eye in a hedged field, the bank close at one side, looking toward the village.
		{
			V3 Eye;
			double Yaw = 0.5 * Pi;
			if (!FindEye(G, F, Eye, Yaw)) Eye = { TX, TY - 500.0, G.At(TX, TY - 500.0).Z + 1.7 };
			// LAND_FIELD_EYE="X Y A [UP PITCH]": where, the heading (math degrees), and optionally the height over the ground (m)
			// and the pitch (degrees, down negative). With three numbers the eye is the soldier's: 1.7 m up, 2 degrees down.
			bool bHigh = false;
			double Pitch = -2.0;
			if (const char* E = std::getenv("LAND_FIELD_EYE"))
			{
				double X = 0.0, Y = 0.0, A = 0.0, Up = 1.7, PitchDeg = -2.0;
				const int N = std::sscanf(E, "%lf %lf %lf %lf %lf", &X, &Y, &A, &Up, &PitchDeg);
				if (N >= 3)
				{
					if (N < 4) Up = 1.7;
					// How far the ground runs on ahead of a place, before a building or a bank stops it.
					const double LX = std::cos(A * Pi / 180.0), LY = std::sin(A * Pi / 180.0);
					auto Ahead = [&G, LX, LY](double QX, double QY) {
						const LandField::Surface Here = G.At(QX, QY);
						double Open = 0.0;
						for (double S = 2.0; S < 300.0; S += 2.0)
						{
							const LandField::Surface Q = G.At(QX + LX * S, QY + LY * S);
							if (Q.What == LandField::Drawn::Floor || Q.Z > Here.Z + 1.2) break;
							Open = S;
						}
						return Open;
					};
					// (a place asked for inside a building, or hard up against one, as a city's streets make easy to hit:
					// the nearest place within 60 m with a street's run ahead of it, so the picture is not a wall)
					if (G.At(X, Y).What == LandField::Drawn::Floor || Ahead(X, Y) < 25.0)
					{
						double Best = Ahead(X, Y) - 0.0, BX = X, BY = Y;
						for (double R = 3.0; R <= 60.0; R += 3.0)
							for (int K = 0; K < 48; ++K)
							{
								const double Th = 2.0 * Pi * K / 48.0;
								const double QX = X + R * std::cos(Th), QY = Y + R * std::sin(Th);
								const LandField::Surface Q = G.At(QX, QY);
								if (Q.What == LandField::Drawn::Floor || Q.What == LandField::Drawn::Water) continue;
								const double Score = Ahead(QX, QY) - 0.35 * R;
								if (Score > Best) { Best = Score; BX = QX; BY = QY; }
							}
						if (BX != X || BY != Y)
						{
							std::printf("  the eye asked for at %.0f, %.0f had no view: moved %.0f m to %.0f, %.0f, where the ground runs %.0f m ahead\n",
								X, Y, std::hypot(BX - X, BY - Y), BX, BY, Ahead(BX, BY));
							X = BX;
							Y = BY;
						}
					}
					Eye = { X, Y, G.At(X, Y).Z + Up };
					Yaw = A * Pi / 180.0;
				}
				if (N >= 4) // (a height alone: the pitch stays the soldier's 2 degrees down)
				{
					Eye.Z = G.At(X, Y).Z + Up;
					Pitch = PitchDeg;
					bHigh = true;
				}
			}
			// (LAND_FIELD_EYE_UP: the eye that many metres over the ground, not 1.7; LAND_FIELD_EYE_PITCH: looking that many
			// degrees down, not 2: the same as LAND_FIELD_EYE's fourth and fifth numbers, for a found eye)
			if (const double EyeUp = Metres("LAND_FIELD_EYE_UP"); EyeUp > 0.0) { Eye.Z = G.At(Eye.X, Eye.Y).Z + EyeUp; bHigh = true; }
			if (const char* P = std::getenv("LAND_FIELD_EYE_PITCH")) { Pitch = -std::atof(P); bHigh = true; }
			std::printf("  eye at %.0f, %.0f, %.1f m up, looking %.0f degrees from north\n", Eye.X, Eye.Y, Eye.Z - G.At(Eye.X, Eye.Y).Z, std::fmod(450.0 - Yaw * 180.0 / Pi, 360.0));
			if (bHigh) std::printf("  eye pitch %.1f degrees\n", Pitch);
			// (under the game's surface: the furrows' way and strength at the ground point nearest the eye, Mesh::Uvs)
			if (SurfaceAsked().bSurfaceOn)
				for (const LandField::Tile& Tl : G.Tiles())
				{
					if (Eye.X < Tl.X0 || Eye.X >= Tl.X1 || Eye.Y < Tl.Y0 || Eye.Y >= Tl.Y1 || Tl.Near.Uvs.size() != 2 * Tl.Near.Points()) continue;
					size_t Best = 0;
					double BestD = 1e18;
					for (size_t K = 0; K < Tl.Near.Points(); ++K)
					{
						const double Dd = std::hypot(Tl.Near.Positions[K * 3] - Eye.X, Tl.Near.Positions[K * 3 + 1] - Eye.Y);
						if (Dd < BestD)
						{
							BestD = Dd;
							Best = K;
						}
					}
					const double WU = Tl.Near.Uvs[Best * 2], WV = Tl.Near.Uvs[Best * 2 + 1];
					std::printf("  the furrows at the eye's nearest ground point (%.1f m off): way %.3f, %.3f, strength %.2f, %.0f degrees from north\n", BestD, WU, WV, std::hypot(WU, WV),
					            std::hypot(WU, WV) > 1e-6 ? std::fmod(450.0 - std::atan2(WV, WU) * 180.0 / Pi, 360.0) : 0.0);
				}
			const View V = Looking(Eye, Yaw, Pitch * Pi / 180.0, 62.0, W, H);
			const V3 Ahead{ std::cos(Yaw), std::sin(Yaw), 0.0 };
			if (!WantPicture("eye")) {}
			else if (!bHigh) bOk &= Picture(G, L, V, Eye + Ahead * 45.0, 120.0, Eye + Ahead * 520.0, 1400.0, 70.0, true, Dir + "/" + Key + "-field-eye.png", &Sk);
			else
			{
				// A raised eye (as the oblique picture): the fine shadow round where the centre of the view meets the ground,
				// the wide one well past it; the clutter round the eye as the oblique has it.
				const double Over = std::max(Eye.Z - G.At(Eye.X, Eye.Y).Z, 0.5);
				const double Hit = std::clamp(Over / std::tan(std::max(-Pitch, 1.0) * Pi / 180.0), 45.0, 450.0);
				const V3 Ground{ Eye.X, Eye.Y, G.At(Eye.X, Eye.Y).Z };
				const double Fine = std::clamp(1.5 * Hit, 120.0, 640.0);
				bOk &= Picture(G, L, V, Ground + Ahead * Hit, Fine, Ground + Ahead * (Hit + 650.0), 2000.0, 70.0, true, Dir + "/" + Key + "-field-eye.png", &Sk);
			}
		}
		// (LAND_FIELD_EYE_ONLY: the eye's picture alone)
		if (std::getenv("LAND_FIELD_EYE_ONLY")) return bOk;
		// (b) 30 m up over the bocage, south of the village, looking toward it.
		{
			const double EX = TX + 140.0, EY = TY - 560.0;
			const V3 Eye{ EX, EY, G.At(EX, EY).Z + 30.0 };
			const double Yaw = std::atan2(TY - EY, TX - EX);
			const View V = Looking(Eye, Yaw, -9.0 * Pi / 180.0, 60.0, W, H);
			const V3 Ahead{ std::cos(Yaw), std::sin(Yaw), 0.0 };
			if (WantPicture("oblique")) bOk &= Picture(G, L, V, Eye + Ahead * 150.0 - V3{ 0, 0, 30.0 }, 320.0, Eye + Ahead * 800.0, 2000.0, 70.0, true, Dir + "/" + Key + "-field-oblique.png", &Sk);
		}
		// (c) The whole field and its band from straight above, north up.
		{
			const double X0 = -250.0, Y0 = -250.0, X1 = F.Width() + 250.0, Y1 = F.Depth() + 250.0;
			View V;
			V.bOrtho = true;
			V.Fwd = { 0, 0, -1 };
			V.Right = { 1, 0, 0 };
			V.Up = { 0, 1, 0 };
			V.Eye = { 0.5 * (X0 + X1), 0.5 * (Y0 + Y1), 3000.0 };
			V.Half = 0.5 * (X1 - X0);
			V.W = 3200;
			V.H = static_cast<int>(std::lround(3200.0 * (Y1 - Y0) / (X1 - X0) / 2.0)) * 2;
			if (WantPicture("map")) bOk &= Picture(G, L, V, V3{}, 0.0, { V.Eye.X, V.Eye.Y, 0.0 }, std::max(X1 - X0, Y1 - Y0) + 200.0, 0.0, false, Dir + "/" + Key + "-field-map.png");
		}
		return bOk;
	}

	bool PaintView(const LandField::Ground& G, const LandField::Field& F, const Sky& Sk, const double Eye[3], const double At[3], double FovDeg, int W, int H, bool bHaze, double ClutterReach, const std::string& Path)
	{
		for (int K = 0; K < 256; ++K) LinearOf[K] = static_cast<float>(Linear(K));
		Light Lamp = Morning(Sk);
		{
			const double MidX = 0.5 * F.Width(), MidY = 0.5 * F.Depth(), MidZ = G.At(MidX, MidY).Z;
			const double Climb = (Lamp.Base - MidZ) / std::max(Lamp.Sun.Z, 0.15);
			const double UX = MidX + Lamp.Sun.X * Climb, UY = MidY + Lamp.Sun.Y * Climb;
			Lamp.AnchorU = 0.5 - (UY / 4000.0 - std::floor(UY / 4000.0));
			Lamp.AnchorV = 0.5 - (UX / 4000.0 - std::floor(UX / 4000.0));
		}
		const V3 E{ Eye[0], Eye[1], Eye[2] }, T{ At[0], At[1], At[2] };
		const V3 D = T - E;
		const double Far = std::sqrt(Dot(D, D));
		const View V = Looking(E, std::atan2(D.Y, D.X), std::asin(std::clamp(D.Z / std::max(Far, 1e-6), -1.0, 1.0)), FovDeg, 2 * W, 2 * H);
		const double Wide = std::max(60.0, 2.5 * Far * std::tan(0.5 * FovDeg * Pi / 180.0));
		return Picture(G, Lamp, V, T, Wide, T, std::max(1400.0, 6.0 * Wide), ClutterReach, bHaze, Path);
	}
}
