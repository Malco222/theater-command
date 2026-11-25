// Theater Command - a burnt cottage's ground (release 2, effects: Brian's steppe screenshot of 25 November, 17:11Z,
// "make the scorch marks brown, varied and soft-edged, so overlaps don't pile up into solid black"; the art director's
// gate note of 14:01Z).
//
// The east kit's burnt-out house (Content/Models/East/BurntChimney.json) stood on a flat skirt of its 'melt' paint,
// 143 m2 of one dark grey, the ring a fire thaws in snow: on summer grass every ruin drew the same black, jagged mat,
// a crisp grey oval of 'ash' on it and a black pillar for a chimney. Here, on trial behind the word "burntground"
// (Field::Tried; without it the field's old look, untouched):
// - Ruin: the model's flat 'melt' skirt and 'ash' oval are not drawn (found by their paint names, every level); its
//   beams char brown-black, its stove's stack dull burnt brick, its top third and mouth black with soot and streaked
//   down, its stove warmer.
// - Lay: each burnt cottage's own burnt ground, laid in the near ground's own tiles (drawn on the ground's own
//   surface, so its faded edge is the ground itself): a draped grid 0.6 m apart, 8 mm over the drawn ground (under the
//   ruts and shell holes' skins), its colour the ground's own as drawn there turned to the burn's. Each from a hash of
//   its own place: one of three sizes (showing about 1.5, 3 or 5.5 m past its walls), out of round by its lobes, a
//   tongue down the field's wind and often a second its own way, a ragged edge falling to the ground over 2.5 to 3.5 m
//   (a ring no darker than 0.7 of the ground), islands of grass left in it and flecks past it; inside, 1 to 3 m drifts
//   of ash-grey by the hearth, burnt brown and bare black earth, and 2 to 4 broken pieces of char off its middle, its
//   darkest. Where two overlap the darker of the two is drawn, never both. Its hues umber and ochre (G - B no less
//   than R - G), so a blue haze does not turn them mauve; under snow wet earth and dirty snow, the walls' floor char.
// - The clutter standing on it (grass, flowers, corn) is thinned and dried by the ground's own Ground::AddBurn, fed the
//   marks' rounds (RoundsOf, FeedBurn, in the block for the neighbours at the end of this file).
// - ColourOver (Ground::BurntColourAt) for what is laid over it; ShadeOver for a footprint shade over it, the darker of
//   the two and never their product; ShareAt the share of light it leaves, for the vehicles' footprint shade.
//
// No engine in here: Tools/land_field's test builds it with the ground and checks it.
#pragma once

#include "Battle/LandField.h"
#include "Battle/LandJson.h"
#include "Battle/LandScape.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace LandScorch
{
	namespace Detail
	{
		constexpr double Pi = 3.14159265358979323846;

		inline uint64_t Mix(uint64_t X)
		{
			X += 0x9E3779B97F4A7C15ull;
			X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
			X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
			return X ^ (X >> 31);
		}
		inline uint64_t Hash(uint64_t A, uint64_t B) { return Mix(A ^ Mix(B + 0x632BE59BD9B4E019ull)); }
		inline double Unit(uint64_t H) { return static_cast<double>(H >> 11) / 9007199254740992.0; }
		inline double Range(uint64_t H, double Lo, double Hi) { return Lo + (Hi - Lo) * Unit(H); }
		inline double Smooth(double E0, double E1, double X)
		{
			const double T = std::clamp((X - E0) / (E1 - E0), 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}
		inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }
		/** Value noise, -1 to 1, smooth, about one feature a unit. */
		inline double Noise(double X, double Y, uint64_t Seed)
		{
			const double FX = std::floor(X), FY = std::floor(Y);
			const int64_t I = static_cast<int64_t>(FX), J = static_cast<int64_t>(FY);
			double U = X - FX, V = Y - FY;
			U = U * U * (3.0 - 2.0 * U);
			V = V * V * (3.0 - 2.0 * V);
			auto L = [Seed](int64_t A, int64_t B) { return 2.0 * Unit(Hash(Hash(Seed, static_cast<uint64_t>(A)), static_cast<uint64_t>(B))) - 1.0; };
			const double A0 = Lerp(L(I, J), L(I + 1, J), U), A1 = Lerp(L(I, J + 1), L(I + 1, J + 1), U);
			return Lerp(A0, A1, V);
		}
		/** The eye's lightness of an sRGB colour (0 to 255). */
		inline double Light(double R, double G, double B) { return 0.299 * R + 0.587 * G + 0.114 * B; }
		inline double Light(const LandField::Rgb8& C) { return Light(C.R, C.G, C.B); }
		inline uint8_t Byte(double V) { return static_cast<uint8_t>(std::clamp(std::lround(V), 0L, 255L)); }
	}

	// ====================================================================== the kit model's look

	/**
	 * The burnt-out house's model as read (LandSteppe::ReadKit from the file's Text, every level 1:1 with the file's
	 * points), its look by its paints' names: the flat 'melt' skirt and 'ash' oval not drawn (their triangles dropped,
	 * their points kept, so the model's size, and the men's sight of it, stay as they were); 'char' brown-black, each
	 * point a little its own; the stack ('brick_burnt', and the 'soot' shell over its upper part) burnt brick sooted
	 * toward its top and in streaks; the stove a warmer burnt clay. False (and M left alone) where the file is not the one
	 * M was read from (its points do not agree): see Plain.
	 */
	inline bool Ruin(const std::string& Text, LandField::BaseMesh& M)
	{
		using namespace Detail;
		LandJson::Json Root;
		std::string Error;
		if (Text.empty() || !LandJson::Parse(Text, Root, Error)) return false;
		std::vector<std::string> Names;
		for (size_t K = 0; K < Root["paint_names"].Count(); ++K) Names.push_back(Root["paint_names"].At(K).Str("?"));
		if (Names.empty()) return false;
		struct Level
		{
			const char* Key;
			LandField::Mesh* Into;
		};
		const Level Levels[] = { { "near", &M.Near }, { "mid", &M.Mid }, { "far", &M.Far } };
		// (every level first checked, so nothing is half done)
		for (const Level& L : Levels)
		{
			if (!Root[L.Key].IsObject()) continue;
			if (LandJson::Numbers(Root[L.Key]["paints"]).size() != L.Into->Points()) return false;
		}
		// (the brick dulled by the fire and the weather since, not new terracotta; the soot near black)
		const LandScape::Tone Brick{ 106.0, 70.0, 56.0 }, Soot{ 34.0, 29.0, 26.0 }, Char{ 40.0, 31.0, 25.0 }, Dust{ 98.0, 88.0, 76.0 }, Stove{ 108.0, 96.0, 84.0 };
		for (const Level& L : Levels)
		{
			if (!Root[L.Key].IsObject()) continue;
			LandField::Mesh& Me = *L.Into;
			const std::vector<double> Paint = LandJson::Numbers(Root[L.Key]["paints"]);
			std::vector<std::string> Of(Paint.size());
			float Top = -1e9f;
			for (size_t K = 0; K < Paint.size(); ++K)
			{
				const size_t P = static_cast<size_t>(std::max(0.0, Paint[K]));
				Of[K] = P < Names.size() ? Names[P] : std::string();
				if (Of[K] == "soot" || Of[K] == "brick_burnt") Top = std::max(Top, Me.Positions[3 * K + 2]);
			}
			for (size_t K = 0; K < Paint.size(); ++K)
			{
				const double X = Me.Positions[3 * K], Y = Me.Positions[3 * K + 1], Z = Me.Positions[3 * K + 2], NZ = Me.Normals[3 * K + 2];
				const uint64_t H = Hash(0x5C0C4ull, Hash(static_cast<uint64_t>(std::llround(X * 1000.0)), Hash(static_cast<uint64_t>(std::llround(Y * 1000.0)), static_cast<uint64_t>(std::llround(Z * 1000.0)))));
				LandScape::Tone C;
				bool bSet = true;
				if (Of[K] == "char")
				{
					// (each point its own: some beams nearer black, some brown, a few dusted with ash where they face up)
					const double Shade = Range(H, 0.82, 1.4);
					C = LandScape::Tone{ Char.R * Shade, Char.G * Shade, Char.B * Shade };
					if (NZ > 0.7 && Unit(Hash(H, 1)) < 0.3) C = LandScape::Mixed(C, Dust, 0.4);
				}
				else if (Of[K] == "soot" || Of[K] == "brick_burnt")
				{
					// The stack: burnt brick below, its top third and mouth black with soot (full from some 1.0 m under its
					// top, coming on over 0.8 m below that), each corner by its own amount so the soot runs down its faces in
					// heavy streaks; the stove's sooted top plate (under 2 m) dark.
					const double Up = Smooth(static_cast<double>(Top) - 1.8, static_cast<double>(Top) - 1.0, Z);
					const double Streak = Range(Hash(H, 2), -0.1, 0.4);
					const double S = Z < 2.0 && Of[K] == "soot" ? 0.62 : std::clamp(0.12 + 0.84 * Up + Streak * (0.45 + 0.5 * Up), 0.0, 0.96);
					C = LandScape::Mixed(Brick, Soot, S);
					const double Shade = Range(Hash(H, 3), 0.9, 1.08);
					C = LandScape::Tone{ C.R * Shade, C.G * Shade, C.B * Shade };
				}
				else if (Of[K] == "stove_burnt")
				{
					const double Shade = Range(H, 0.92, 1.06);
					C = LandScape::Mixed(LandScape::Tone{ Stove.R * Shade, Stove.G * Shade, Stove.B * Shade }, Soot, Z > 1.0 ? 0.25 : 0.0);
				}
				else bSet = false;
				if (bSet) Me.Colours[K] = LandField::Rgb8{ Byte(C.R), Byte(C.G), Byte(C.B) };
			}
			std::vector<uint32_t> Kept;
			Kept.reserve(Me.Triangles.size());
			for (size_t T = 0; T + 2 < Me.Triangles.size(); T += 3)
			{
				bool bFlat = false;
				for (size_t V = 0; V < 3; ++V)
				{
					const std::string& N = Of[Me.Triangles[T + V]];
					if (N == "melt" || N == "ash") bFlat = true;
				}
				if (bFlat) continue;
				Kept.insert(Kept.end(), { Me.Triangles[T], Me.Triangles[T + 1], Me.Triangles[T + 2] });
			}
			Me.Triangles = std::move(Kept);
		}
		return true;
	}

	/** The plain stand-in where the model is not read (LandSteppe::PlainKit's, LandFarm's KitPlain): a stack of burnt brick
	 * sooted at its top on a small low bed of brown ash, not the 8 by 6 m dark slab. Its near level only (FarFrom kept). */
	inline void Plain(LandField::BaseMesh& M)
	{
		M.Near = LandField::Mesh();
		M.Mid = LandField::Mesh();
		M.Far = LandField::Mesh();
		LandScape::Writer W(M.Near);
		W.Box(0.4, -0.5, 0.8, 0.32, 0.32, 0.8, 0.0, LandScape::Tone{ 92.0, 62.0, 50.0 });
		W.Box(0.4, -0.5, 1.9, 0.32, 0.32, 0.3, 0.0, LandScape::Tone{ 40.0, 34.0, 30.0 });
		W.Box(0.2, -0.3, 0.45, 0.8, 0.7, 0.45, 0.0, LandScape::Tone{ 106.0, 94.0, 82.0 });
		W.Box(0.0, 0.0, 0.03, 2.6, 2.0, 0.03, 0.0, LandScape::Tone{ 104.0, 92.0, 80.0 });
		M.Far = M.Near;
	}

	/** The burnt-out house's look of release 2: the model (Kit's "BurntChimney") as Ruin makes it over, else the plain
	 * stand-in. */
	inline void Look(const std::map<std::string, std::string>& Kit, LandField::BaseMesh& M)
	{
		const auto Found = Kit.find("BurntChimney");
		if (Found == Kit.end() || !Ruin(Found->second, M)) Plain(M);
	}

	// ====================================================================== the burnt ground

	/** The grid the burnt ground is laid on (metres; the same everywhere, so marks that meet share their points), and
	 * how high over the drawn ground where it is burnt. */
	constexpr double Step = 0.6;
	constexpr double Lift = 0.008;
	/** Where nothing of it is burnt (its own edge, all round, and the odd point among its flecks), the sheet is let down
	 * under the drawn ground, so its edge is no lip a low sun could draw a line of shadow under. */
	constexpr double Sink = 0.03;

	/**
	 * Where a group of marks (those whose boxes meet) is held back from what is not the ground's own (a lane, a bank, a
	 * house's floor, a wall, water, marsh, or off the near tiles), on the grid it is laid on: each point's state (bit 0
	 * near enough something burnt to be asked, bit 1 held, bit 2 something burnt there) and how much of the burn may be
	 * drawn there (Keep: 0 on and next to what is held, 1 from 5.2 m off it, an even fall of 4.5 m between). Made once
	 * by MarksOf and shared by the group's marks, so Lay, ColourOver and ShareAt all hold the burn back alike.
	 */
	struct Hold
	{
		int64_t I0 = 0, J0 = 0;
		int NI = 0, NJ = 0;
		std::vector<uint8_t> State;
		std::vector<double> Keep;
		/** Keep at a field point, mixed across its grid square from the four corners (1 off the grid). */
		double KeepAt(double X, double Y) const
		{
			const double FX = X / Step - static_cast<double>(I0), FY = Y / Step - static_cast<double>(J0);
			if (!(FX >= 0.0 && FY >= 0.0 && FX <= NI - 1 && FY <= NJ - 1)) return 1.0;
			const int I = std::min(static_cast<int>(FX), NI - 2), J = std::min(static_cast<int>(FY), NJ - 2);
			if (I < 0 || J < 0) return 1.0;
			const double U = FX - I, V = FY - J;
			const size_t P = static_cast<size_t>(J) * static_cast<size_t>(NI) + static_cast<size_t>(I);
			const double A = Keep[P] + (Keep[P + 1] - Keep[P]) * U, B = Keep[P + static_cast<size_t>(NI)] + (Keep[P + static_cast<size_t>(NI) + 1] - Keep[P + static_cast<size_t>(NI)]) * U;
			return A + (B - A) * V;
		}
	};

	/** One burnt cottage's burnt ground: all of it from a hash of its own place (and the field's wind, one for all). */
	struct Mark
	{
		double X = 0.0, Y = 0.0, Yaw = 0.0, Scale = 1.0;
		uint64_t Key = 0;
		/** 0 small (showing some 1.5 m past its walls), 1 middle (3 m), 2 large (5.5 m). */
		int Size = 0;
		/** Past the walls (metres, before its lobes and tongues) where it is still wholly burnt, the width of its fall to
		 * the ground (A straight from 1 to 0 across it: it shows some three quarters of the way out), the share of the
		 * ground's lightness at the fall's start. */
		double Plateau = 1.0, Fall = 2.4, Ring = 0.7;
		/** Its tongues: down the wind, and (some) a second its own way: length past the walls (metres), heading (radians
		 * from east), half width at the walls (metres). The walls' half sides and corner (metres, its scale taken). */
		double Tongue = 0.0, TongueYaw = 0.0, TongueWide = 0.4, Tongue2 = 0.0, Tongue2Yaw = 0.0, Tongue2Wide = 0.3;
		double WallA = 4.3, WallB = 3.1, Corner = 1.0;
		double Lobe[4] = { 0.0, 0.0, 0.0, 0.0 }, Phase[4] = { 0.0, 0.0, 0.0, 0.0 };
		/** Its darkest: 2 to 4 broken pieces of char and black earth, each its own place in the walls' frame and size
		 * (U, V, radius; metres), off its middle. */
		int Pieces = 0;
		double Piece[4][3] = {};
		bool bSnow = false;
		/** How far from its middle anything of it can reach (metres): its box. */
		double Out = 20.0;
		/** Where it is held back toward a lane, a bank, a floor, a wall or water (MarksOf; null: nowhere). */
		std::shared_ptr<const Hold> Held;
	};

	inline Mark MarkOf(double X, double Y, double Yaw, double Scale, double WindYaw, bool bSnow)
	{
		using namespace Detail;
		Mark M;
		M.X = X;
		M.Y = Y;
		M.Yaw = Yaw;
		M.Scale = Scale;
		M.bSnow = bSnow;
		M.Key = Hash(0xB0A7ull, Hash(static_cast<uint64_t>(std::llround(X * 100.0)), static_cast<uint64_t>(std::llround(Y * 100.0))));
		const uint64_t K = M.Key;
		M.Size = std::min(2, static_cast<int>(3.0 * Unit(Hash(K, 1))));
		// How far it shows past its walls (A a quarter), its fall's width, and its ring's share: the fall no steeper than
		// (1 - Ring) / Fall times its ragged edge's own slope (at most some 1.45), under 0.14 of the ground's lightness a
		// metre, 8.5% between points 0.6 m apart. Under snow a deeper ring (the thawed wet earth), its fall as gentle and so
		// wider (10.4 times what the ring takes off), the small and middle showing further for it; the walls' floor never
		// in its fall (half a metre past them at least), its char always there.
		static const double Shows[3] = { 1.5, 3.2, 5.6 }, Falls[3] = { 2.5, 2.9, 3.2 }, Rings[3] = { 0.77, 0.73, 0.7 };
		static const double SnowShows[3] = { 2.4, 3.7, 6.2 }, SnowRings[3] = { 0.7, 0.6, 0.58 };
		const double Sh = (bSnow ? SnowShows[M.Size] : Shows[M.Size]) * Range(Hash(K, 2), 0.88, 1.12);
		M.Ring = bSnow ? SnowRings[M.Size] + Range(Hash(K, 8), -0.03, 0.03) : Rings[M.Size] + Range(Hash(K, 8), -0.015, 0.015);
		M.Fall = std::max(Falls[M.Size], bSnow ? 10.4 * (1.0 - M.Ring) : 0.0) * Range(Hash(K, 3), 1.0, 1.1);
		M.Plateau = bSnow ? std::max(Sh - 0.75 * M.Fall, 0.5) : Sh - 0.75 * M.Fall;
		M.Tongue = M.Size == 0 ? Range(Hash(K, 4), 0.3, 1.7) : Sh * Range(Hash(K, 4), 0.45, 0.85) + Range(Hash(K, 5), 0.8, 2.0);
		M.TongueYaw = WindYaw + Range(Hash(K, 6), -0.4, 0.4);
		M.TongueWide = M.Size == 0 ? Range(Hash(K, 7), 0.9, 1.5) : Range(Hash(K, 7), 1.3, 2.4);
		if (M.Size > 0 && Unit(Hash(K, 40)) < 0.65)
		{
			M.Tongue2 = Sh * Range(Hash(K, 41), 0.3, 0.65);
			M.Tongue2Yaw = Range(Hash(K, 42), 0.0, 2.0 * Pi);
			M.Tongue2Wide = Range(Hash(K, 43), 1.0, 1.9);
		}
		M.WallA = 4.3 * Scale;
		M.WallB = 3.1 * Scale;
		M.Corner = 1.0 * Scale;
		double Sum = 0.0;
		for (int L = 0; L < 4; ++L)
		{
			// (the first, twice round, long: no two the same way out of round)
			M.Lobe[L] = L == 0 ? Range(Hash(K, 10), 0.15, 0.4) : Range(Hash(K, 10 + static_cast<uint64_t>(L)), 0.05, 0.22) / std::sqrt(1.0 + L);
			M.Phase[L] = Range(Hash(K, 20 + static_cast<uint64_t>(L)), 0.0, 2.0 * Pi);
			Sum += M.Lobe[L];
		}
		M.Pieces = 2 + std::min(2, static_cast<int>(3.0 * Unit(Hash(K, 50))));
		for (int P = 0; P < M.Pieces; ++P)
		{
			const uint64_t H = Hash(K, 60 + static_cast<uint64_t>(P));
			M.Piece[P][0] = Range(Hash(H, 1), -0.8, 0.8) * (M.WallA + 0.6);
			M.Piece[P][1] = Range(Hash(H, 2), -0.8, 0.8) * (M.WallB + 0.6);
			M.Piece[P][2] = Range(Hash(H, 3), 0.65, 1.25) * std::sqrt(Scale);
		}
		// (the walls' corner, the most it reaches with its lobes, tongues and noise, its fall and its flecks, and a margin)
		M.Out = std::hypot(M.WallA, M.WallB) + std::max(M.Plateau, 0.0) + std::max(1.2, M.Plateau) * Sum + std::max(M.Tongue, M.Tongue2) + 0.7 + M.Fall + 3.6 + 4.0;
		return M;
	}

	/** What a mark lays at a place: how burnt (0 the ground as it is, 1 the burn's own colour), and the burnt ground's
	 * own colour there as a share of the ground's lightness (Rel) in a warm hue (R, G, B, its lightness 1). (BurnAt's
	 * bColour false: only whether anything of it may be there, A 1 or 0, cheaply.) */
	struct Burn
	{
		double A = 0.0;
		double Rel = 1.0;
		double Hue[3] = { 1.0, 1.0, 1.0 };
		/** How far past the walls (negative inside them), metres; how far through its fall (0 still wholly burnt, 1 past it). */
		double D = 0.0, Rim = 0.0;
	};

	inline Burn BurnAt(const Mark& M, double X, double Y, bool bColour = true)
	{
		using namespace Detail;
		Burn B;
		const double DX = X - M.X, DY = Y - M.Y;
		if (DX * DX + DY * DY > M.Out * M.Out) return B;
		const double C = std::cos(M.Yaw), S = std::sin(M.Yaw);
		const uint64_t K = M.Key;
		// (past the walls: a rounded box's signed distance)
		auto Past = [&M, C, S](double PX, double PY) {
			const double QU = (PX - M.X) * C + (PY - M.Y) * S, QV = -(PX - M.X) * S + (PY - M.Y) * C;
			const double QX = std::fabs(QU) - (M.WallA - M.Corner), QY = std::fabs(QV) - (M.WallB - M.Corner);
			return std::hypot(std::max(QX, 0.0), std::max(QY, 0.0)) + std::min(std::max(QX, QY), 0.0) - M.Corner;
		};
		// How far past where it is still wholly burnt (negative inside; near enough metres): its body out past the walls,
		// out of round by its lobes (in metres, so a small one is too), joined smoothly with its tongues (each a capsule
		// from its middle, broad at the walls and narrowing to its tip), the edge ragged (the noise broad and fine).
		auto Field = [&M, C, S](double PX, double PY, double PastHere) {
			const double QU = (PX - M.X) * C + (PY - M.Y) * S, QV = -(PX - M.X) * S + (PY - M.Y) * C;
			const double Th = std::atan2(QV, QU);
			double Lobes = 0.0;
			for (int L = 0; L < 4; ++L) Lobes += M.Lobe[L] * std::cos((L + 2) * Th + M.Phase[L]);
			double F = PastHere - (M.Plateau + std::max(1.2, M.Plateau) * Lobes);
			const double Tl[2] = { M.Tongue, M.Tongue2 }, Ty[2] = { M.TongueYaw, M.Tongue2Yaw }, Tw[2] = { M.TongueWide, M.Tongue2Wide };
			for (int T = 0; T < 2; ++T)
			{
				if (Tl[T] <= 0.0) continue;
				const double WX = std::cos(Ty[T]), WY = std::sin(Ty[T]);
				// (the walls' reach that way, in their own frame)
				const double WU = WX * C + WY * S, WV = -WX * S + WY * C;
				const double Ext = std::min(M.WallA / std::max(1e-3, std::fabs(WU)), M.WallB / std::max(1e-3, std::fabs(WV)));
				const double Len = Ext + std::max(M.Plateau, 0.0) + Tl[T];
				const double Along = std::clamp((PX - M.X) * WX + (PY - M.Y) * WY, 0.0, Len);
				// (narrowing no faster than half a metre a metre toward its tip, so its sides fall no steeper than its end)
				const double Width = std::clamp(0.35 + 0.5 * (Len - Along), 0.35, Tw[T]);
				const double Dt = std::hypot(PX - M.X - WX * Along, PY - M.Y - WY * Along) - Width;
				// (a smooth union, so no crease where it leaves the body)
				constexpr double Kn = 1.5;
				const double Hn = std::max(Kn - std::fabs(F - Dt), 0.0) / Kn;
				F = std::min(F, Dt) - Hn * Hn * Kn * 0.25;
			}
			return F;
		};
		const double D = Past(X, Y);
		B.D = D;
		const double U = DX * C + DY * S, V = -DX * S + DY * C;
		const double F0 = Field(X, Y, D);
		if (!bColour)
		{
			// (only whether anything of it may be here: the slope's steepest taken)
			B.A = F0 < 1.6 * (M.Fall + 3.6) + 0.5 ? 1.0 : 0.0;
			return B;
		}
		// (the field near enough a distance already, its tongues narrowing no faster than half a metre a metre: not divided
		// by its own slope, which varies and would steepen the fall where it does; then the edge made ragged, the noise
		// broad and fine)
		const double Sd = F0 - 0.22 * Noise(X / 3.2, Y / 3.2, Hash(K, 31)) - 0.05 * Noise(X / 1.3, Y / 1.3, Hash(K, 32));
		// Its fall: straight from wholly burnt to the ground over its width (the eye's soft edge, ragged with the reach).
		double A = std::clamp(1.0 - Sd / M.Fall, 0.0, 1.0);
		// (how far in from its fall: what is inside it, its drifts and pieces, comes on over 2.4 m, so the fall itself is
		// one even ring)
		const double Inward = Smooth(0.0, 2.4, -Sd);
		// Islands of grass the fire left, out past the walls (the middle and large), inside its fall.
		if (M.Size > 0)
		{
			const double Band = Smooth(0.3, 1.3, D) * Inward;
			const double Isle = Smooth(0.2, 0.75, Noise(X / 2.6, Y / 2.6, Hash(K, 33)));
			A *= 1.0 - 0.8 * Isle * Band;
		}
		// Flecks of it past the edge, faint.
		const double Fleck = 0.12 * Smooth(0.45, 0.85, Noise(X / 2.0, Y / 2.0, Hash(K, 34))) * Smooth(0.4 * M.Fall, M.Fall, Sd) * (1.0 - Smooth(M.Fall + 1.0, M.Fall + 3.6, Sd));
		B.A = std::clamp(std::max(A, Fleck), 0.0, 1.0);
		B.Rim = std::clamp(Sd / M.Fall, 0.0, 1.0);
		if (B.A <= 0.0) return B;
		// Its own colour (Rel: the share of the ground's own lightness). Inside: burnt brown mottled, drifts of bare
		// black earth, ash-grey drifts round the hearth, broken pieces of char off its middle, all 1 to 3 m across; its
		// fall one even singed ring. Under snow: thawed wet earth, the walls' floor black char, sharp against the snow.
		const double Mottle = Noise(X / 2.5, Y / 2.5, Hash(K, 35));
		const double Bare = Smooth(0.1, 0.6, Noise(X / 3.2, Y / 3.2, Hash(K, 38)));
		const double E = std::hypot(U / (M.WallA + 0.4), V / (M.WallB + 0.4));
		const double Ash = Smooth(1.3, 0.7, E) * Smooth(-0.35, 0.45, Noise(X / 2.4, Y / 2.4, Hash(K, 36)));
		double Dark = 0.0;
		for (int P = 0; P < M.Pieces; ++P)
		{
			const double R = std::hypot(U - M.Piece[P][0], V - M.Piece[P][1]) / M.Piece[P][2];
			Dark = std::max(Dark, Smooth(1.0, 0.25, R) * Smooth(-0.45, 0.35, Noise(X / 1.4, Y / 1.4, Hash(K, 70 + static_cast<uint64_t>(P)))));
		}
		Dark *= Smooth(0.6, 2.4, -Sd); // (the darkest well inside it, none on its fall)
		double Inside = 0.0, Ring = M.Ring + 0.01 * Mottle;
		const double ToBlack = Bare * (1.0 - Ash);
		if (!M.bSnow)
		{
			Inside = 0.7 + 0.06 * Mottle;
			Inside = Lerp(Inside, 0.6, ToBlack);
			Inside = Lerp(Inside, 0.98, 0.9 * Ash);
			Inside = Lerp(Inside, 0.46, Dark);
		}
		else
		{
			// (the walls' floor and up to a metre past them char in broken pieces, their edges sharp against the snow's white; round
			// it the thawed wet earth, ash-grey drifts in it)
			const double Cut = Range(Hash(K, 44), 0.1, 0.35); // (each its own share of char)
			const double Floor = Smooth(1.0, 0.0, D) * Smooth(Cut, Cut + 0.2, Noise(X / 1.3, Y / 1.3, Hash(K, 39))) * Smooth(0.6, 2.4, -Sd);
			Inside = Range(Hash(K, 45), 0.5, 0.6) + 0.08 * Mottle;
			Inside = Lerp(Inside, 0.8, 0.85 * Ash);
			Inside = Lerp(Inside, Range(Hash(K, 46), 0.33, 0.39) + 0.025 * Mottle, std::max(Floor, Dark));
			Dark = std::max(Floor, Dark);
		}
		B.Rel = Lerp(Ring, Inside, Inward);
		// The hues (G - B no less than R - G: umber and ochre, not the reddish grey a blue haze turns mauve): scorched
		// earth, black earth, char, ash, singed grass (wet earth and dirty snow under snow).
		const double Scorched[3] = { 1.0, 0.8, 0.58 }, Black[3] = { 1.0, 0.84, 0.66 }, Char[3] = { 1.0, 0.86, 0.71 }, AshHue[3] = { 1.0, 0.95, 0.87 }, Singed[3] = { 1.0, 0.9, 0.6 };
		const double Wet[3] = { 1.0, 0.83, 0.62 }, Dirty[3] = { 1.0, 0.94, 0.84 };
		const double* Earth = M.bSnow ? Wet : Scorched;
		const double* Edge = M.bSnow ? Dirty : Singed;
		for (int Ch = 0; Ch < 3; ++Ch)
		{
			double H = Lerp(Earth[Ch], Black[Ch], ToBlack);
			H = Lerp(H, AshHue[Ch], 0.9 * Ash);
			H = Lerp(H, Char[Ch], Dark);
			B.Hue[Ch] = Lerp(Edge[Ch], H, Inward);
		}
		const double L = Light(B.Hue[0], B.Hue[1], B.Hue[2]);
		for (double& H : B.Hue) H /= L;
		return B;
	}

	/** The colour a mark draws over the ground's own (Ground, 0 to 255): the ground's hue turned to the burn's first (so
	 * what is a little burnt is singed straw, not olive), its lightness then the ground's times its share, by how burnt. */
	inline void Paint(const Burn& B, const double Ground[3], double Out[3])
	{
		const double Lg = Detail::Light(Ground[0], Ground[1], Ground[2]);
		const double Turn = Detail::Smooth(0.0, 0.5, B.A);
		const double Lo = Lg * (1.0 + (B.Rel - 1.0) * B.A);
		for (int C = 0; C < 3; ++C) Out[C] = Detail::Lerp(Ground[C] / std::max(1.0, Lg), B.Hue[C], Turn) * Lo;
	}

	/** The near ground's own colour at a place as drawn: its tile's square cut along its flatter diagonal (as
	 * Ground::DrawnZ), the colours of the triangle's corners (Ground::ColourAt there) mixed across it. */
	class Drawn
	{
	public:
		explicit Drawn(const LandField::Ground& InG) : G(InG)
		{
			const std::vector<LandField::Tile>& T = G.Tiles();
			if (T.empty()) return;
			X0 = T[0].X0;
			Y0 = T[0].Y0;
			Size = T[0].X1 - T[0].X0;
			while (Across < static_cast<int>(T.size()) && std::fabs(T[static_cast<size_t>(Across)].Y0 - Y0) < 1e-6) ++Across;
			Down = Across > 0 ? static_cast<int>(T.size()) / Across : 0;
		}
		/** The tile a place is on (-1 off them). */
		int TileOf(double X, double Y) const
		{
			if (Across <= 0) return -1;
			const int I = static_cast<int>(std::floor((X - X0) / Size)), J = static_cast<int>(std::floor((Y - Y0) / Size));
			if (I < 0 || J < 0 || I >= Across || J >= Down) return -1;
			return J * Across + I;
		}
		bool Colour(double X, double Y, double Out[3])
		{
			const int Ti = TileOf(X, Y);
			if (Ti < 0) return false;
			const LandField::Tile& T = G.Tiles()[static_cast<size_t>(Ti)];
			const double Sp = T.Spacing;
			const int Q = static_cast<int>(std::lround((T.X1 - T.X0) / Sp));
			const int I = std::clamp(static_cast<int>(std::floor((X - T.X0) / Sp)), 0, Q - 1), J = std::clamp(static_cast<int>(std::floor((Y - T.Y0) / Sp)), 0, Q - 1);
			const Corner& A = At(Ti, T, I, J), &Bc = At(Ti, T, I + 1, J), &C = At(Ti, T, I + 1, J + 1), &D = At(Ti, T, I, J + 1);
			const double U = std::clamp((X - T.X0) / Sp - I, 0.0, 1.0), V = std::clamp((Y - T.Y0) / Sp - J, 0.0, 1.0);
			for (int K = 0; K < 3; ++K)
			{
				if (std::fabs(A.Z - C.Z) <= std::fabs(Bc.Z - D.Z))
					Out[K] = U >= V ? A.C[K] * (1.0 - U) + Bc.C[K] * (U - V) + C.C[K] * V : A.C[K] * (1.0 - V) + C.C[K] * U + D.C[K] * (V - U);
				else
					Out[K] = U + V <= 1.0 ? A.C[K] * (1.0 - U - V) + Bc.C[K] * U + D.C[K] * V : Bc.C[K] * (1.0 - V) + C.C[K] * (U + V - 1.0) + D.C[K] * (1.0 - U);
			}
			return true;
		}

		/** The near ground's height as drawn (Ground::DrawnZ) and its normal as lit (Ground::DrawnUp), from the same
		 * corners: each corner's normal from the slope across its neighbours. */
		double Z(double X, double Y, double OutUp[3])
		{
			const int Ti = TileOf(X, Y);
			if (Ti < 0)
			{
				G.DrawnUp(X, Y, OutUp[0], OutUp[1], OutUp[2]);
				return G.DrawnZ(X, Y);
			}
			const LandField::Tile& T = G.Tiles()[static_cast<size_t>(Ti)];
			const double Sp = T.Spacing;
			const int Q = static_cast<int>(std::lround((T.X1 - T.X0) / Sp));
			const int I = std::clamp(static_cast<int>(std::floor((X - T.X0) / Sp)), 0, Q - 1), J = std::clamp(static_cast<int>(std::floor((Y - T.Y0) / Sp)), 0, Q - 1);
			const Corner& A = At(Ti, T, I, J), &Bc = At(Ti, T, I + 1, J), &C = At(Ti, T, I + 1, J + 1), &D = At(Ti, T, I, J + 1);
			double Na[3], Nb[3], Nc[3], Nd[3];
			NormalAt(Ti, T, I, J, Na);
			NormalAt(Ti, T, I + 1, J, Nb);
			NormalAt(Ti, T, I + 1, J + 1, Nc);
			NormalAt(Ti, T, I, J + 1, Nd);
			const double U = std::clamp((X - T.X0) / Sp - I, 0.0, 1.0), V = std::clamp((Y - T.Y0) / Sp - J, 0.0, 1.0);
			double Wa = 0.0, Wb = 0.0, Wc = 0.0, Wd = 0.0;
			if (std::fabs(A.Z - C.Z) <= std::fabs(Bc.Z - D.Z))
			{
				if (U >= V) { Wa = 1.0 - U; Wb = U - V; Wc = V; }
				else { Wa = 1.0 - V; Wc = U; Wd = V - U; }
			}
			else
			{
				if (U + V <= 1.0) { Wa = 1.0 - U - V; Wb = U; Wd = V; }
				else { Wb = 1.0 - V; Wc = U + V - 1.0; Wd = 1.0 - U; }
			}
			double Len = 0.0;
			for (int K = 0; K < 3; ++K)
			{
				OutUp[K] = Na[K] * Wa + Nb[K] * Wb + Nc[K] * Wc + Nd[K] * Wd;
				Len += OutUp[K] * OutUp[K];
			}
			Len = std::sqrt(std::max(Len, 1e-12));
			for (int K = 0; K < 3; ++K) OutUp[K] /= Len;
			return A.Z * Wa + Bc.Z * Wb + C.Z * Wc + D.Z * Wd;
		}

	private:
		void NormalAt(int Ti, const LandField::Tile& T, int I, int J, double Out[3])
		{
			const double S = T.Spacing;
			const double DX = (At(Ti, T, I + 1, J).Z - At(Ti, T, I - 1, J).Z) / (2.0 * S), DY = (At(Ti, T, I, J + 1).Z - At(Ti, T, I, J - 1).Z) / (2.0 * S);
			const double L = std::sqrt(DX * DX + DY * DY + 1.0);
			Out[0] = -DX / L;
			Out[1] = -DY / L;
			Out[2] = 1.0 / L;
		}
		struct Corner
		{
			double Z = 0.0;
			double C[3] = { 0.0, 0.0, 0.0 };
		};
		const Corner& At(int Ti, const LandField::Tile& T, int I, int J)
		{
			const uint64_t Key = (static_cast<uint64_t>(Ti) << 32) ^ (static_cast<uint64_t>(J + 4) << 16) ^ static_cast<uint64_t>(I + 4);
			const auto Found = Cache.find(Key);
			if (Found != Cache.end()) return Found->second;
			Corner C;
			const double X = T.X0 + I * T.Spacing, Y = T.Y0 + J * T.Spacing;
			C.Z = G.DrawnZ(X, Y);
			const LandField::Rgb8 Col = G.ColourAt(X, Y);
			C.C[0] = Col.R;
			C.C[1] = Col.G;
			C.C[2] = Col.B;
			return Cache.emplace(Key, C).first->second;
		}
		const LandField::Ground& G;
		double X0 = 0.0, Y0 = 0.0, Size = 250.0;
		int Across = 0, Down = 0;
		std::unordered_map<uint64_t, Corner> Cache;
	};

	/** The marks whose boxes meet, together: each mark's group, the lowest index of the group's own root (as Lay lays
	 * them, one grid a group). */
	inline std::vector<size_t> GroupsOf(const std::vector<Mark>& Ms)
	{
		const size_t N = Ms.size();
		std::vector<size_t> Root(N);
		for (size_t K = 0; K < N; ++K) Root[K] = K;
		auto Find = [&Root](size_t K) {
			while (Root[K] != K) K = Root[K] = Root[Root[K]];
			return K;
		};
		for (size_t A = 0; A < N; ++A)
			for (size_t B = A + 1; B < N; ++B)
			{
				const Mark& Ma = Ms[A];
				const Mark& Mb = Ms[B];
				if (std::fabs(Ma.X - Mb.X) < Ma.Out + Mb.Out && std::fabs(Ma.Y - Mb.Y) < Ma.Out + Mb.Out) Root[Find(A)] = Find(B);
			}
		std::vector<size_t> Out(N);
		for (size_t K = 0; K < N; ++K) Out[K] = Find(K);
		return Out;
	}

	/**
	 * Each group's Hold (see Hold): on the group's grid, which points are near something burnt, which are held (a lane,
	 * a bank, a house's floor, a wall, water or marsh there, or off the near tiles), the steps from each to the nearest
	 * held (a chamfer, eight ways), and from that how much of the burn may be drawn: none within 0.7 m, all from 5.2 m.
	 */
	inline void HoldBack(const LandField::Ground& G, std::vector<Mark>& Ms)
	{
		using namespace Detail;
		const size_t N = Ms.size();
		const std::vector<size_t> Group = GroupsOf(Ms);
		Drawn Under(G);
		for (size_t Gr = 0; Gr < N; ++Gr)
		{
			std::vector<size_t> In;
			double BX0 = 1e30, BY0 = 1e30, BX1 = -1e30, BY1 = -1e30;
			for (size_t K = 0; K < N; ++K)
			{
				if (Group[K] != Gr) continue;
				const Mark& M = Ms[K];
				In.push_back(K);
				BX0 = std::min(BX0, M.X - M.Out);
				BY0 = std::min(BY0, M.Y - M.Out);
				BX1 = std::max(BX1, M.X + M.Out);
				BY1 = std::max(BY1, M.Y + M.Out);
			}
			if (In.empty()) continue;
			auto H = std::make_shared<Hold>();
			H->I0 = static_cast<int64_t>(std::floor(BX0 / Step));
			H->J0 = static_cast<int64_t>(std::floor(BY0 / Step));
			const int NI = static_cast<int>(std::ceil(BX1 / Step)) - static_cast<int>(H->I0) + 1, NJ = static_cast<int>(std::ceil(BY1 / Step)) - static_cast<int>(H->J0) + 1;
			H->NI = NI;
			H->NJ = NJ;
			const size_t NP = static_cast<size_t>(NI) * static_cast<size_t>(NJ);
			const int64_t I0 = H->I0, J0 = H->J0;
			auto XOf = [I0](int I) { return static_cast<double>(I0 + I) * Step; };
			auto YOf = [J0](int J) { return static_cast<double>(J0 + J) * Step; };
			std::vector<uint8_t>& State = H->State;
			State.assign(NP, 0);
			std::vector<int> Gap(NP, 1 << 20);
			// (first the burn alone, to ask the ground only where something is laid)
			for (int J = 0; J < NJ; ++J)
				for (int I = 0; I < NI; ++I)
				{
					double A = 0.0;
					for (const size_t K : In) A = std::max(A, BurnAt(Ms[K], XOf(I), YOf(J), false).A);
					if (A > 0.0) State[static_cast<size_t>(J) * NI + I] |= 4;
				}
			const int Reach = static_cast<int>(std::ceil(5.2 / Step)) + 1;
			for (int J = 0; J < NJ; ++J)
				for (int I = 0; I < NI; ++I)
				{
					if (!(State[static_cast<size_t>(J) * NI + I] & 4)) continue;
					for (int B = std::max(0, J - Reach - 1); B <= std::min(NJ - 1, J + Reach + 1); ++B)
						for (int A = std::max(0, I - Reach - 1); A <= std::min(NI - 1, I + Reach + 1); ++A) State[static_cast<size_t>(B) * NI + A] |= 1;
				}
			for (int J = 0; J < NJ; ++J)
				for (int I = 0; I < NI; ++I)
				{
					const size_t P = static_cast<size_t>(J) * NI + I;
					if (!(State[P] & 1)) continue;
					LandField::Drawn What = LandField::Drawn::Pasture;
					G.HeightAt(XOf(I), YOf(J), &What);
					using LandField::Drawn;
					if (What == Drawn::Lane || What == Drawn::Bank || What == Drawn::Floor || What == Drawn::Wall || What == Drawn::Water || What == Drawn::Marsh || Under.TileOf(XOf(I), YOf(J)) < 0)
					{
						State[P] |= 2;
						Gap[P] = 0;
					}
				}
			// (two passes of a chamfer: the steps to the nearest held point)
			for (int J = 0; J < NJ; ++J)
				for (int I = 0; I < NI; ++I)
				{
					int& Gp = Gap[static_cast<size_t>(J) * NI + I];
					if (I > 0) Gp = std::min(Gp, Gap[static_cast<size_t>(J) * NI + I - 1] + 1);
					if (J > 0) Gp = std::min(Gp, Gap[static_cast<size_t>(J - 1) * NI + I] + 1);
					if (I > 0 && J > 0) Gp = std::min(Gp, Gap[static_cast<size_t>(J - 1) * NI + I - 1] + 1);
					if (I + 1 < NI && J > 0) Gp = std::min(Gp, Gap[static_cast<size_t>(J - 1) * NI + I + 1] + 1);
				}
			for (int J = NJ - 1; J >= 0; --J)
				for (int I = NI - 1; I >= 0; --I)
				{
					int& Gp = Gap[static_cast<size_t>(J) * NI + I];
					if (I + 1 < NI) Gp = std::min(Gp, Gap[static_cast<size_t>(J) * NI + I + 1] + 1);
					if (J + 1 < NJ) Gp = std::min(Gp, Gap[static_cast<size_t>(J + 1) * NI + I] + 1);
					if (I + 1 < NI && J + 1 < NJ) Gp = std::min(Gp, Gap[static_cast<size_t>(J + 1) * NI + I + 1] + 1);
					if (I > 0 && J + 1 < NJ) Gp = std::min(Gp, Gap[static_cast<size_t>(J + 1) * NI + I - 1] + 1);
				}
			// (toward what is held, no more burnt than 4.5 m of even fall allows: the lesser of that and its own, not their
			// product, which would steepen its own fall)
			H->Keep.assign(NP, 0.0);
			for (size_t P = 0; P < NP; ++P) H->Keep[P] = (State[P] & 2) ? 0.0 : std::clamp((Gap[P] * Step - 0.7) / 4.5, 0.0, 1.0);
			const std::shared_ptr<const Hold> Shared = std::move(H);
			for (const size_t K : In) Ms[K].Held = Shared;
		}
	}

	/** The burnt cottages of a laid ground (each with its group's Hold): the steppe's (Base::Chimney), and a region's kit slot that holds the
	 * burnt-out house (LandFarm's east village); the field's wind, one for all, from its seed (the field has no wind of
	 * its own). */
	inline std::vector<Mark> MarksOf(const LandField::Ground& G, const LandField::Field& F)
	{
		using namespace Detail;
		if (!F.Tried("burntground")) return {}; // (on trial: none without the word)
		const double Wind = 2.0 * Pi * Unit(Hash(F.Seed, 0x3171Dull));
		const bool bSnow = G.SnowLying() > 0.15;
		std::vector<Mark> Out;
		for (int B = 0; B < static_cast<int>(LandField::Base::Count); ++B)
		{
			const LandField::Base Bs = static_cast<LandField::Base>(B);
			const bool bSlot = B >= static_cast<int>(LandField::Base::Kit0) && B < static_cast<int>(LandField::Base::Kit0) + LandField::KitSlots && G.BaseOf(Bs).Name == "BurntChimney";
			if (!(Bs == LandField::Base::Chimney && F.Steppe.bOn) && !bSlot) continue;
			for (const LandField::Placed& P : G.Instances(Bs)) Out.push_back(MarkOf(P.X, P.Y, P.Yaw, P.Scale, Wind, bSnow));
		}
		HoldBack(G, Out);
		return Out;
	}

	/** The burnt ground laid: a mesh for each of the field's tiles (G.Tiles()' order; empty where none), to go into its
	 * near level; the marks; what it cost. */
	struct Laid
	{
		std::vector<LandField::Mesh> PerTile;
		std::vector<Mark> Marks;
		size_t Points = 0, Tris = 0;
		/** Grid points left as the ground (a lane, a bank, a floor, a wall or water there). */
		size_t Held = 0;
		double Seconds = 0.0;
	};

	/**
	 * Every burnt cottage's ground. The marks whose boxes meet are laid together on one grid, each point the darkest of
	 * their colours there (the darker of two, never the two piled up); each point's height the drawn ground's plus Lift,
	 * its normal the drawn ground's (Ground::DrawnUp, lit as the ground is); a square laid only where a corner of it is
	 * burnt, so the mesh's own edge is the ground's own colour all round. Nothing is laid on a lane, a bank, a house's
	 * floor, a wall or water, and the burn fades out evenly over 4.5 m toward them (gone a step short of them).
	 */
	inline Laid Lay(const LandField::Ground& G, const LandField::Field& F)
	{
		using namespace Detail;
		const auto T0 = std::chrono::steady_clock::now();
		Laid Out;
		Out.Marks = MarksOf(G, F);
		Out.PerTile.assign(G.Tiles().size(), LandField::Mesh());
		const size_t N = Out.Marks.size();
		if (N == 0) return Out;
		// (the marks whose boxes meet, together, each group on its own grid, held back as MarksOf found: Hold)
		const std::vector<size_t> Group = GroupsOf(Out.Marks);
		Drawn Under(G);
		for (size_t Gr = 0; Gr < N; ++Gr)
		{
			std::vector<const Mark*> In;
			for (size_t K = 0; K < N; ++K)
				if (Group[K] == Gr) In.push_back(&Out.Marks[K]);
			if (In.empty() || !In[0]->Held) continue;
			const Hold& H = *In[0]->Held;
			const int64_t I0 = H.I0, J0 = H.J0;
			const int NI = H.NI, NJ = H.NJ;
			const size_t NP = static_cast<size_t>(NI) * static_cast<size_t>(NJ);
			auto XOf = [I0](int I) { return static_cast<double>(I0 + I) * Step; };
			auto YOf = [J0](int J) { return static_cast<double>(J0 + J) * Step; };
			std::vector<uint8_t> Held(NP, 0), Near(NP, 0);
			for (size_t P = 0; P < NP; ++P)
			{
				Held[P] = (H.State[P] & 2) ? 1 : 0;
				Near[P] = (H.State[P] & 1) ? 1 : 0;
			}
			// Each point's colour: the darkest of the marks' there (each faded toward what is held), over the ground's own.
			std::vector<LandField::Rgb8> Col(NP);
			std::vector<double> Amount(NP, 0.0);
			for (int J = 0; J < NJ; ++J)
				for (int I = 0; I < NI; ++I)
				{
					const size_t P = static_cast<size_t>(J) * NI + I;
					if (!Near[P] || Held[P]) continue;
					const double X = XOf(I), Y = YOf(J);
					double Ground[3];
					if (!Under.Colour(X, Y, Ground)) continue;
					// (toward what is held, no more burnt than 4.5 m of even fall allows: the lesser of that and its own, not their
					// product, which would steepen its own fall)
					const double Keep = H.Keep[P];
					double Best[3] = { Ground[0], Ground[1], Ground[2] }, BestL = Light(Ground[0], Ground[1], Ground[2]), BestA = 0.0;
					for (const Mark* M : In)
					{
						if (BurnAt(*M, X, Y, false).A <= 0.0) continue;
						Burn B = BurnAt(*M, X, Y);
						B.A = std::min(B.A, Keep);
						if (B.A <= 0.0) continue;
						double C[3];
						Paint(B, Ground, C);
						const double L = Light(C[0], C[1], C[2]);
						if (L < BestL)
						{
							BestL = L;
							BestA = B.A;
							for (int K = 0; K < 3; ++K) Best[K] = C[K];
						}
					}
					Amount[P] = BestA;
					Col[P] = LandField::Rgb8{ Byte(Best[0]), Byte(Best[1]), Byte(Best[2]) };
				}
			// The squares laid: where a corner is burnt and none held; each into the tile its middle is on.
			std::unordered_map<uint64_t, uint32_t> Index;
			for (int J = 0; J + 1 < NJ; ++J)
				for (int I = 0; I + 1 < NI; ++I)
				{
					const size_t Pa = static_cast<size_t>(J) * NI + I, Pb = Pa + 1, Pc = Pa + 1 + static_cast<size_t>(NI), Pd = Pa + static_cast<size_t>(NI);
					if (Held[Pa] || Held[Pb] || Held[Pc] || Held[Pd]) continue;
					if (Amount[Pa] <= 0.0 && Amount[Pb] <= 0.0 && Amount[Pc] <= 0.0 && Amount[Pd] <= 0.0) continue;
					const int Ti = Under.TileOf(XOf(I) + 0.5 * Step, YOf(J) + 0.5 * Step);
					if (Ti < 0) continue;
					LandField::Mesh& M = Out.PerTile[static_cast<size_t>(Ti)];
					uint32_t Id[4];
					const size_t Ps[4] = { Pa, Pb, Pc, Pd };
					const int Is[4] = { I, I + 1, I + 1, I }, Js[4] = { J, J, J + 1, J + 1 };
					double Zs[4];
					for (int V = 0; V < 4; ++V)
					{
						const uint64_t Key = (static_cast<uint64_t>(Ti) << 40) ^ static_cast<uint64_t>(Ps[V]);
						const auto Found = Index.find(Key);
						const double X = XOf(Is[V]), Y = YOf(Js[V]);
						double Up[3];
						Zs[V] = Under.Z(X, Y, Up);
						if (Found != Index.end())
						{
							Id[V] = Found->second;
							continue;
						}
						Id[V] = static_cast<uint32_t>(M.Points());
						M.Positions.insert(M.Positions.end(), { static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Zs[V] + (Amount[Ps[V]] > 0.0 ? Lift : -Sink)) });
						M.Normals.insert(M.Normals.end(), { static_cast<float>(Up[0]), static_cast<float>(Up[1]), static_cast<float>(Up[2]) });
						M.Colours.push_back(Col[Ps[V]]);
						Index.emplace(Key, Id[V]);
						++Out.Points;
					}
					// (counter-clockwise seen from above, cut along the flatter diagonal as the ground is)
					if (std::fabs(Zs[0] - Zs[2]) <= std::fabs(Zs[1] - Zs[3]))
						M.Triangles.insert(M.Triangles.end(), { Id[0], Id[1], Id[2], Id[0], Id[2], Id[3] });
					else
						M.Triangles.insert(M.Triangles.end(), { Id[0], Id[1], Id[3], Id[1], Id[2], Id[3] });
					Out.Tris += 2;
				}
			for (size_t P = 0; P < NP; ++P)
				if (Held[P] && (H.State[P] & 4)) ++Out.Held;
		}
		Out.Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count();
		return Out;
	}

	/** Lays it into the tiles' near levels (after the ground's own triangles, which are left as they are), their heights'
	 * bounds widened to hold it; gives back its marks (none: null) for the ground to keep (Ground::BurntColourAt). */
	inline std::shared_ptr<const std::vector<Mark>> Into(std::vector<LandField::Tile>& Tiles, Laid&& L)
	{
		for (size_t T = 0; T < Tiles.size() && T < L.PerTile.size(); ++T)
		{
			const LandField::Mesh& From = L.PerTile[T];
			if (From.Empty()) continue;
			LandField::Mesh& To = Tiles[T].Near;
			const uint32_t Base = static_cast<uint32_t>(To.Points());
			To.Positions.insert(To.Positions.end(), From.Positions.begin(), From.Positions.end());
			To.Normals.insert(To.Normals.end(), From.Normals.begin(), From.Normals.end());
			To.Colours.insert(To.Colours.end(), From.Colours.begin(), From.Colours.end());
			for (const uint32_t V : From.Triangles) To.Triangles.push_back(Base + V);
			for (size_t K = 2; K < From.Positions.size(); K += 3)
			{
				Tiles[T].ZMin = std::min(Tiles[T].ZMin, From.Positions[K]);
				Tiles[T].ZMax = std::max(Tiles[T].ZMax, From.Positions[K]);
			}
		}
		if (L.Marks.empty()) return nullptr;
		return std::make_shared<const std::vector<Mark>>(std::move(L.Marks));
	}

	/** How burnt a place is, the darkest of the marks there (0 where none). */
	inline Burn Darkest(const std::vector<Mark>& Ms, double X, double Y, const double Ground[3], double Out[3])
	{
		Burn Best;
		for (int K = 0; K < 3; ++K) Out[K] = Ground[K];
		double BestL = Detail::Light(Ground[0], Ground[1], Ground[2]);
		for (const Mark& M : Ms)
		{
			const double DX = X - M.X, DY = Y - M.Y;
			if (DX * DX + DY * DY > M.Out * M.Out) continue;
			Burn B = BurnAt(M, X, Y);
			if (M.Held) B.A = std::min(B.A, M.Held->KeepAt(X, Y)); // (held back toward a lane as Lay lays it)
			if (B.A <= 0.0) continue;
			double C[3];
			Paint(B, Ground, C);
			const double L = Detail::Light(C[0], C[1], C[2]);
			if (L < BestL)
			{
				BestL = L;
				Best = B;
				for (int K = 0; K < 3; ++K) Out[K] = C[K];
			}
		}
		return Best;
	}

	/** The ground's colour with the burnt ground over it (Ground::BurntColourAt): for what is laid on it, a rut, a shell
	 * hole's skin, to fade to at its edge. */
	inline LandField::Rgb8 ColourOver(const std::vector<Mark>& Ms, double X, double Y, LandField::Rgb8 Ground)
	{
		const double Gc[3] = { static_cast<double>(Ground.R), static_cast<double>(Ground.G), static_cast<double>(Ground.B) };
		double C[3];
		Darkest(Ms, X, Y, Gc, C);
		return LandField::Rgb8{ Detail::Byte(C[0]), Detail::Byte(C[1]), Detail::Byte(C[2]) };
	}

	/**
	 * A footprint shade's light (LandShade: it multiplies what is behind it) over burnt ground whose lightness is Share
	 * of the unburnt ground's there: lifted so the two never pile up, the darker of the two and never their product.
	 * The ground drawn under it is then Burnt x ShadeOver = min(Burnt, Unburnt x Shade). (For the vehicles' shade: its
	 * light at each point, or the whole shade by the share under its root, from Ground::BurntColourAt / ColourAt.)
	 */
	inline double ShadeOver(double Shade, double Share) { return std::min(1.0, Shade / std::clamp(Share, 0.05, 1.0)); }
}

// ====================================================================== for the neighbours (release 2 pass 2)
//
// What the rest of the ground reads of the burnt cottages, kept apart from the look above so the look can change
// without these moving: the rounds the clutter thins in (Ground::AddBurn, the environments' clutter), and the share of
// light the burn leaves at a place (the vehicles' footprint shade, so a shade on a burn does not darken it twice).
// All of it empty or 1 without the trial word "burntground", because MarksOf gives no marks then.
namespace LandScorch
{
	/** A round of a mark's burnt ground for the clutter: its middle (field metres), Core (within it the burn is at or
	 * near its full strength) and Outer (by there it has faded out), metres from the middle. */
	struct Round
	{
		double X = 0.0, Y = 0.0, Core = 0.0, Outer = 0.0;
	};

	/**
	 * The share of the ground's light a burn leaves where it lies: the burnt ground's lightness over the unburnt
	 * ground's there, as Paint draws it (the hue turned first, then the lightness taken down), never above 1, 1 where
	 * nothing of it lies. Paint keeps the ground's lightness times 1 - A (1 - Rel) whatever the ground's own colour (its
	 * hue is turned at an even lightness), so this is the same at every ground and is read off a plain grey.
	 */
	inline double ShareOf(const Burn& B)
	{
		if (B.A <= 0.0) return 1.0;
		const double Grey[3] = { 128.0, 128.0, 128.0 };
		double Out[3];
		Paint(B, Grey, Out);
		return std::clamp(Detail::Light(Out[0], Out[1], Out[2]) / 128.0, 0.0, 1.0);
	}

	/**
	 * The share of light the burnt ground leaves at a field point: the burnt ground's lightness over the unburnt
	 * ground's; 1 off every mark; where marks meet the smallest of theirs (the darker of two drawn, as Lay draws it,
	 * never the two multiplied). A footprint shade S over it is drawn right as unburnt x min(ShareAt, S), which is what
	 * ShadeOver(S, ShareAt) times the burnt ground gives. Held back toward a lane, a bank, a floor, a wall or water as Lay
	 * lays it (each mark's Hold, mixed across its grid squares).
	 */
	inline double ShareAt(const std::vector<Mark>& Marks, double X, double Y)
	{
		double Share = 1.0;
		for (const Mark& M : Marks)
		{
			const double DX = X - M.X, DY = Y - M.Y;
			if (DX * DX + DY * DY > M.Out * M.Out) continue;
			Burn B = BurnAt(M, X, Y);
			if (M.Held) B.A = std::min(B.A, M.Held->KeepAt(X, Y)); // (held back toward a lane as Lay lays it)
			Share = std::min(Share, ShareOf(B));
		}
		return Share;
	}

	/**
	 * The rounds a mark's burnt ground gives the clutter (Ground::AddBurn), so the thinning follows its shape. First one
	 * for the whole mark, round its middle, measured from the mark itself along 24 ways out every 0.25 m: Core is how far
	 * out, on the mean, the burn is still at or near its full strength (at least 0.85 burnt), Outer how far, on the mean,
	 * before its fading edge is through (its faint flecks past that left out), at least half a metre past Core. Then one
	 * small round for each of its broken char pieces, round the piece: Core half its radius, Outer its radius and a
	 * metre, so the ground round the char is the barest. Core is always under Outer, Outer never over 60 m.
	 */
	inline std::vector<Round> RoundsOf(const Mark& M)
	{
		constexpr int Ways = 24;
		constexpr double By = 0.25;
		const int Steps = static_cast<int>(std::floor(M.Out / By));
		double SumCore = 0.0, SumOuter = 0.0;
		for (int K = 0; K < Ways; ++K)
		{
			const double Th = 2.0 * Detail::Pi * K / Ways;
			const double CX = std::cos(Th), CY = std::sin(Th);
			double Core = 0.0, Outer = 0.0;
			for (int I = 0; I <= Steps; ++I)
			{
				const double R = I * By;
				const Burn B = BurnAt(M, M.X + CX * R, M.Y + CY * R);
				if (B.A >= 0.85) Core = R;
				if (B.A > 0.0 && B.Rim < 1.0) Outer = R;
				if (B.D > 0.0 && B.Rim >= 1.0) break; // (out past its fading edge: nothing more of it this way but flecks)
			}
			SumCore += Core;
			SumOuter += Outer;
		}
		std::vector<Round> Out;
		Round Rd;
		Rd.X = M.X;
		Rd.Y = M.Y;
		Rd.Core = std::min(SumCore / Ways, 59.5);
		Rd.Outer = std::min(std::max(Rd.Core + 0.5, SumOuter / Ways), 60.0);
		Out.push_back(Rd);
		const double C = std::cos(M.Yaw), S = std::sin(M.Yaw);
		for (int P = 0; P < M.Pieces; ++P)
		{
			Round Pc;
			Pc.X = M.X + M.Piece[P][0] * C - M.Piece[P][1] * S;
			Pc.Y = M.Y + M.Piece[P][0] * S + M.Piece[P][1] * C;
			Pc.Core = 0.5 * M.Piece[P][2];
			Pc.Outer = M.Piece[P][2] + 1.0;
			Out.push_back(Pc);
		}
		return Out;
	}

	/** Gives the ground's clutter every round of every mark (Ground::AddBurn): the near clutter thins and dries on the
	 * burnt ground. Call it once the ground is built and before its clutter is first asked for, from one thread. */
	inline void FeedBurn(LandField::Ground& G, const std::vector<Mark>& Marks)
	{
		for (const Mark& M : Marks)
			for (const Round& Rd : RoundsOf(M)) G.AddBurn(Rd.X, Rd.Y, Rd.Core, Rd.Outer);
	}
}
