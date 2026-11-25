// Theater Command - a burnt cottage's ground (release 2, effects: Brian's steppe screenshot of 25 November, 17:11Z,
// "make the scorch marks brown, varied and soft-edged, so overlaps don't pile up into solid black"; the art director's
// gate note of 14:01Z).
//
// The east kit's burnt-out house (Content/Models/East/BurntChimney.json) stood on a flat skirt of its 'melt' paint,
// 143 m2 of one dark grey, the ring a fire thaws in snow: on summer grass every ruin drew the same black, jagged mat,
// a crisp grey oval of 'ash' on it and a black pillar for a chimney. Here, with the field's old look kept behind
// Field::bOldBurntGround:
// - Ruin: the model's flat 'melt' skirt and 'ash' oval are not drawn (found by their paint names, every level); its
//   beams char brown-black, its stove's stack burnt brick sooted toward its top and in streaks, its stove warmer.
// - Lay: each burnt cottage's own burnt ground, laid in the near ground's own tiles (drawn on the ground's own
//   surface, so its faded edge is the ground itself): a draped grid 0.6 m apart, 4 cm over the drawn ground, its colour
//   the ground's own as drawn there mixed toward the burn's. Each from a hash of its own place: one of three sizes
//   (reaching about 1.5, 3 or 5 m past its walls), its own lobes and a longer tongue down the field's wind, a ragged
//   edge fading to the ground over 1.6 to 2.5 m, islands of grass left in it and flecks past it; inside, ash-grey
//   drifts by the hearth, burnt brown, bare black earth and a few broken pieces of char. Where two overlap the darker of
//   the two is drawn, never both. Its colours are the ground's lightness times a share (0.4 to 1) in warm hues, so it
//   reads as scorched earth on grass, on stubble or (wet earth, dirty snow) on snow alike.
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
#include <string>
#include <unordered_map>
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
		const LandScape::Tone Brick{ 120.0, 74.0, 58.0 }, Soot{ 50.0, 42.0, 37.0 }, Char{ 40.0, 31.0, 25.0 }, Dust{ 98.0, 88.0, 76.0 }, Stove{ 116.0, 102.0, 88.0 };
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
					// The stack: burnt brick below, sooted toward its top (from some 1.8 m under it), each corner by its own
					// amount so the soot runs down its faces in streaks; the stove's sooted top plate (under 2 m) dark.
					const double Up = Smooth(static_cast<double>(Top) - 1.9, static_cast<double>(Top), Z);
					const double Streak = Range(Hash(H, 2), -0.12, 0.22);
					const double S = Z < 2.0 && Of[K] == "soot" ? 0.62 : std::clamp(0.05 + 0.6 * Up + Streak * (0.3 + Up), 0.0, 0.85);
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
		W.Box(0.4, -0.5, 1.1, 0.32, 0.32, 1.1, 0.0, LandScape::Tone{ 116.0, 72.0, 57.0 });
		W.Box(0.4, -0.5, 2.35, 0.34, 0.34, 0.15, 0.0, LandScape::Tone{ 56.0, 46.0, 40.0 });
		W.Box(0.2, -0.3, 0.45, 0.8, 0.7, 0.45, 0.0, LandScape::Tone{ 112.0, 100.0, 88.0 });
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

	/** One burnt cottage's burnt ground: all of it from a hash of its own place (and the field's wind, one for all). */
	struct Mark
	{
		double X = 0.0, Y = 0.0, Yaw = 0.0, Scale = 1.0;
		uint64_t Key = 0;
		/** 0 small (reaching some 1.5 m past its walls), 1 middle (3 m), 2 large (5 m). */
		int Size = 0;
		/** Past the walls (metres, before its lobes and tongue), the fade's width, the tongue's length, heading (radians
		 * from east) and width (radians), the walls' half sides and corner (metres, its scale taken). */
		double Reach = 3.0, Feather = 2.0, Tongue = 0.0, TongueYaw = 0.0, TongueWide = 0.4, WallA = 4.3, WallB = 3.1, Corner = 1.0;
		double Lobe[4] = { 0.0, 0.0, 0.0, 0.0 }, Phase[4] = { 0.0, 0.0, 0.0, 0.0 };
		bool bSnow = false;
		/** How far from its middle anything of it can reach (metres): its box. */
		double Out = 20.0;
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
		static const double Reaches[3] = { 1.5, 3.6, 5.8 };
		M.Reach = Reaches[M.Size] * Range(Hash(K, 2), 0.88, 1.12);
		M.Feather = Range(Hash(K, 3), M.Size == 0 ? 2.0 : 1.6, 2.5);
		M.Tongue = M.Reach * Range(Hash(K, 4), 0.5, 1.0) + Range(Hash(K, 5), 0.8, 2.0);
		M.TongueYaw = WindYaw + Range(Hash(K, 6), -0.4, 0.4);
		M.TongueWide = Range(Hash(K, 7), 0.28, 0.5);
		M.WallA = 4.3 * Scale;
		M.WallB = 3.1 * Scale;
		M.Corner = 1.0 * Scale;
		double Sum = 0.0;
		for (int L = 0; L < 4; ++L)
		{
			M.Lobe[L] = Range(Hash(K, 10 + static_cast<uint64_t>(L)), 0.06, 0.32) / std::sqrt(1.0 + L);
			M.Phase[L] = Range(Hash(K, 20 + static_cast<uint64_t>(L)), 0.0, 2.0 * Pi);
			Sum += M.Lobe[L];
		}
		// (the walls' corner, the most it reaches with its lobes, tongue and noise, its feather and its flecks, and a margin)
		M.Out = std::hypot(M.WallA, M.WallB) + std::max(M.Reach * (1.0 + Sum), 2.8) + M.Tongue + 0.62 + M.Feather + 3.0 + 4.0;
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
		/** How far past the walls (negative inside them), metres; how far through its fading edge (0 inside it, 1 past it). */
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
		// Its reach by the way out: lobes, the tongue down the wind, the edge ragged (the noise broad); and how burnt
		// falls off past the walls, straight to Knee at the reach (no steeper than 0.22 a metre at the deepest ring's
		// 0.38 under the ground), then to nothing over
		// the feather: the eye's soft edge.
		const double Knee = std::min(0.55, 0.2 * M.Feather);
		// (the ring past the walls a little lighter the smaller the mark: a small one is shallow burnt grass)
		const double Depth = 1.0 - (M.Size == 0 ? 0.7 : M.Size == 1 ? 0.65 : 0.61);
		const double Floor = (1.0 - Knee) * Depth / (0.38 * 0.22);
		auto ReachAt = [&M, C, S, K, Floor](double PX, double PY) {
			const double QU = (PX - M.X) * C + (PY - M.Y) * S, QV = -(PX - M.X) * S + (PY - M.Y) * C;
			const double Th = std::atan2(QV, QU);
			double Lobes = 1.0;
			for (int L = 0; L < 4; ++L) Lobes += M.Lobe[L] * std::cos((L + 2) * Th + M.Phase[L]);
			const double Off = std::remainder(std::atan2(PY - M.Y, PX - M.X) - M.TongueYaw, 2.0 * Pi);
			const double Tongue = M.Tongue * std::exp(-(Off / M.TongueWide) * (Off / M.TongueWide));
			// (no shorter than the gentlest fall its depth allows, Floor, that too in its lobes)
			return std::max(0.75, Lobes) * std::max(M.Reach, Floor) + Tongue + 0.5 * Noise(PX / 2.8, PY / 2.8, Hash(K, 31)) + 0.12 * Noise(PX / 1.3, PY / 1.3, Hash(K, 32));
		};
		const double D = Past(X, Y);
		B.D = D;
		const double U = DX * C + DY * S, V = -DX * S + DY * C;
		const double Inner = ReachAt(X, Y);
		if (!bColour)
		{
			// (only whether anything of it may be here: the slope's steepest taken)
			B.A = D - Inner < 1.6 * (M.Feather + 3.0) ? 1.0 : 0.0;
			return B;
		}
		// (how far out past its reach, along the way out: the reach's own lobes and tongue taken into the slope, so
		// the fall is as soft across a tongue's side as across its end)
		constexpr double Hd = 0.25;
		const double GX = (Past(X + Hd, Y) - ReachAt(X + Hd, Y) - Past(X - Hd, Y) + ReachAt(X - Hd, Y)) / (2.0 * Hd);
		const double GY = (Past(X, Y + Hd) - ReachAt(X, Y + Hd) - Past(X, Y - Hd) + ReachAt(X, Y - Hd)) / (2.0 * Hd);
		const double Sd = (D - Inner) / std::max(0.4, std::hypot(GX, GY));
		double A = Sd < 0.0 ? 1.0 - (1.0 - Knee) * std::clamp(1.0 + Sd / Inner, 0.0, 1.0) : Knee * std::max(0.0, 1.0 - Sd / M.Feather);
		// Islands of grass the fire left, out past the walls (some 2 to 4 m), short of its fading edge.
		const double Band = Smooth(0.3, 2.6, D) * (1.0 - Smooth(-1.5, 0.0, Sd));
		const double Isle = Smooth(0.15, 0.8, Noise(X / 3.0, Y / 3.0, Hash(K, 33)));
		A *= 1.0 - 0.4 * Isle * Band;
		// Flecks of it past the edge, faint.
		const double Fleck = 0.18 * Smooth(0.4, 0.85, Noise(X / 1.5, Y / 1.5, Hash(K, 34))) * Smooth(0.0, M.Feather, Sd) * (1.0 - Smooth(M.Feather, M.Feather + 3.0, Sd));
		B.A = std::clamp(std::max(A, Fleck), 0.0, 1.0);
		B.Rim = std::clamp(Sd / M.Feather, 0.0, 1.0);
		if (B.A <= 0.0 || !bColour) return B;
		// Its own colour. Inside the walls: burnt brown and bare black earth mottled, ash-grey drifts round the hearth,
		// broken pieces of char. Out past them: scorched earth going to singed grass toward the edge, patches of bare
		// black earth. (Rel: the share of the ground's own lightness.)
		const double Mottle = Noise(X / 2.2, Y / 2.2, Hash(K, 35));
		double Core = 0.58 + 0.07 * Mottle;
		const double E = std::hypot(U / (3.9 * M.Scale), V / (3.2 * M.Scale));
		const double Ash = Smooth(1.3, 0.7, E) * Smooth(-0.15, 0.45, Noise(X / 1.6, Y / 1.6, Hash(K, 36)));
		const double Char = Smooth(0.45, 0.7, Noise(X / 1.2, Y / 1.2, Hash(K, 37))) * Smooth(0.0, -0.8, D) * (1.0 - Ash);
		Core = Lerp(Core, 0.96 + 0.05 * Mottle, 0.85 * Ash);
		Core = Lerp(Core, 0.42, Char);
		const double Far = Smooth(0.0, Inner, D);
		const double Bare = Smooth(0.1, 0.7, Noise(X / 3.0, Y / 3.0, Hash(K, 38))) * (1.0 - Far);
		const double Ring = 1.0 - Depth + 0.03 * Mottle - 0.08 * Bare;
		const double In = Smooth(0.6, -0.6, D);
		B.Rel = Lerp(Ring, Core, In);
		// The hues: scorched earth, black earth, singed grass, ash (wet earth and dirty snow under snow).
		const double Scorched[3] = { 1.0, 0.8, 0.61 }, Black[3] = { 1.0, 0.83, 0.7 }, Singed[3] = { 1.0, 0.88, 0.63 }, AshHue[3] = { 1.0, 0.95, 0.88 };
		const double Wet[3] = { 1.0, 0.84, 0.72 }, Dirty[3] = { 1.0, 0.96, 0.92 };
		const double* Earth = M.bSnow ? Wet : Scorched;
		const double* Edge = M.bSnow ? Dirty : Singed;
		const double ToEdge = Smooth(0.3 * Inner, Inner, D) * (1.0 - In);
		const double ToBlack = std::max(Bare * (1.0 - In), Char * In);
		const double ToAsh = 0.85 * Ash * In;
		for (int Ch = 0; Ch < 3; ++Ch)
		{
			double H = Lerp(Earth[Ch], Edge[Ch], ToEdge);
			H = Lerp(H, Black[Ch], ToBlack);
			B.Hue[Ch] = Lerp(H, AshHue[Ch], ToAsh);
		}
		const double L = Light(B.Hue[0], B.Hue[1], B.Hue[2]);
		for (double& H : B.Hue) H /= L;
		return B;
	}

	/** The colour a mark draws over the ground's own (Ground, 0 to 255): the ground's lightness times its share, in its
	 * hue, mixed in by how burnt. */
	inline void Paint(const Burn& B, const double Ground[3], double Out[3])
	{
		const double Lg = Detail::Light(Ground[0], Ground[1], Ground[2]);
		for (int C = 0; C < 3; ++C) Out[C] = Ground[C] + (B.Hue[C] * B.Rel * Lg - Ground[C]) * B.A;
	}

	/** The burnt cottages of a laid ground: the steppe's (Base::Chimney), and a region's kit slot that holds the
	 * burnt-out house (LandFarm's east village); the field's wind, one for all, from its seed (the field has no wind of
	 * its own). */
	inline std::vector<Mark> MarksOf(const LandField::Ground& G, const LandField::Field& F)
	{
		using namespace Detail;
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
		return Out;
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
			const double Step = T.Spacing;
			const int Q = static_cast<int>(std::lround((T.X1 - T.X0) / Step));
			const int I = std::clamp(static_cast<int>(std::floor((X - T.X0) / Step)), 0, Q - 1), J = std::clamp(static_cast<int>(std::floor((Y - T.Y0) / Step)), 0, Q - 1);
			const Corner& A = At(Ti, T, I, J), &Bc = At(Ti, T, I + 1, J), &C = At(Ti, T, I + 1, J + 1), &D = At(Ti, T, I, J + 1);
			const double U = std::clamp((X - T.X0) / Step - I, 0.0, 1.0), V = std::clamp((Y - T.Y0) / Step - J, 0.0, 1.0);
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
			const double Step = T.Spacing;
			const int Q = static_cast<int>(std::lround((T.X1 - T.X0) / Step));
			const int I = std::clamp(static_cast<int>(std::floor((X - T.X0) / Step)), 0, Q - 1), J = std::clamp(static_cast<int>(std::floor((Y - T.Y0) / Step)), 0, Q - 1);
			const Corner& A = At(Ti, T, I, J), &Bc = At(Ti, T, I + 1, J), &C = At(Ti, T, I + 1, J + 1), &D = At(Ti, T, I, J + 1);
			double Na[3], Nb[3], Nc[3], Nd[3];
			NormalAt(Ti, T, I, J, Na);
			NormalAt(Ti, T, I + 1, J, Nb);
			NormalAt(Ti, T, I + 1, J + 1, Nc);
			NormalAt(Ti, T, I, J + 1, Nd);
			const double U = std::clamp((X - T.X0) / Step - I, 0.0, 1.0), V = std::clamp((Y - T.Y0) / Step - J, 0.0, 1.0);
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

	/** The grid the burnt ground is laid on (metres; the same everywhere, so marks that meet share their points), and
	 * how high over the drawn ground where it is burnt. */
	constexpr double Step = 0.6;
	constexpr double Lift = 0.04;
	/** Where nothing of it is burnt (its own edge, all round, and the odd point among its flecks), the sheet is let down
	 * under the drawn ground, so its edge is no lip a low sun could draw a line of shadow under. */
	constexpr double Sink = 0.03;

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
	 * floor, a wall or water, and the burn fades out evenly over 3 m toward them (gone a step short of them).
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
		// (the marks whose boxes meet, together)
		std::vector<size_t> Root(N);
		for (size_t K = 0; K < N; ++K) Root[K] = K;
		auto Find = [&Root](size_t K) {
			while (Root[K] != K) K = Root[K] = Root[Root[K]];
			return K;
		};
		for (size_t A = 0; A < N; ++A)
			for (size_t B = A + 1; B < N; ++B)
			{
				const Mark& Ma = Out.Marks[A];
				const Mark& Mb = Out.Marks[B];
				if (std::fabs(Ma.X - Mb.X) < Ma.Out + Mb.Out && std::fabs(Ma.Y - Mb.Y) < Ma.Out + Mb.Out) Root[Find(A)] = Find(B);
			}
		Drawn Under(G);
		for (size_t Gr = 0; Gr < N; ++Gr)
		{
			std::vector<const Mark*> In;
			double BX0 = 1e30, BY0 = 1e30, BX1 = -1e30, BY1 = -1e30;
			for (size_t K = 0; K < N; ++K)
			{
				if (Find(K) != Gr) continue;
				const Mark& M = Out.Marks[K];
				In.push_back(&M);
				BX0 = std::min(BX0, M.X - M.Out);
				BY0 = std::min(BY0, M.Y - M.Out);
				BX1 = std::max(BX1, M.X + M.Out);
				BY1 = std::max(BY1, M.Y + M.Out);
			}
			if (In.empty()) continue;
			const int64_t I0 = static_cast<int64_t>(std::floor(BX0 / Step)), J0 = static_cast<int64_t>(std::floor(BY0 / Step));
			const int NI = static_cast<int>(std::ceil(BX1 / Step)) - static_cast<int>(I0) + 1, NJ = static_cast<int>(std::ceil(BY1 / Step)) - static_cast<int>(J0) + 1;
			const size_t NP = static_cast<size_t>(NI) * static_cast<size_t>(NJ);
			auto XOf = [I0](int I) { return static_cast<double>(I0 + I) * Step; };
			auto YOf = [J0](int J) { return static_cast<double>(J0 + J) * Step; };
			// What is drawn at each point: off the ground's own (a lane, a bank, a floor, a wall, water), held; how far
			// from the nearest held point (in steps, eight ways).
			std::vector<uint8_t> Held(NP, 0);
			std::vector<int> Gap(NP, 1 << 20);
			std::vector<double> Burnt(NP, 0.0);
			// (first the burn alone, to ask the ground only where something is laid)
			for (int J = 0; J < NJ; ++J)
				for (int I = 0; I < NI; ++I)
				{
					double A = 0.0;
					for (const Mark* M : In) A = std::max(A, BurnAt(*M, XOf(I), YOf(J), false).A);
					Burnt[static_cast<size_t>(J) * NI + I] = A;
				}
			std::vector<uint8_t> Near(NP, 0);
			const int Reach = static_cast<int>(std::ceil(3.7 / Step)) + 1;
			for (int J = 0; J < NJ; ++J)
				for (int I = 0; I < NI; ++I)
				{
					if (Burnt[static_cast<size_t>(J) * NI + I] <= 0.0) continue;
					for (int B = std::max(0, J - Reach - 1); B <= std::min(NJ - 1, J + Reach + 1); ++B)
						for (int A = std::max(0, I - Reach - 1); A <= std::min(NI - 1, I + Reach + 1); ++A) Near[static_cast<size_t>(B) * NI + A] = 1;
				}
			for (int J = 0; J < NJ; ++J)
				for (int I = 0; I < NI; ++I)
				{
					const size_t P = static_cast<size_t>(J) * NI + I;
					if (!Near[P]) continue;
					LandField::Drawn What = LandField::Drawn::Pasture;
					G.HeightAt(XOf(I), YOf(J), &What);
					using LandField::Drawn;
					if (What == Drawn::Lane || What == Drawn::Bank || What == Drawn::Floor || What == Drawn::Wall || What == Drawn::Water || What == Drawn::Marsh || Under.TileOf(XOf(I), YOf(J)) < 0)
					{
						Held[P] = 1;
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
					const double Keep = std::clamp((Gap[P] * Step - 0.7) / 3.0, 0.0, 1.0);
					double Best[3] = { Ground[0], Ground[1], Ground[2] }, BestL = Light(Ground[0], Ground[1], Ground[2]), BestA = 0.0;
					for (const Mark* M : In)
					{
						if (BurnAt(*M, X, Y, false).A <= 0.0) continue;
						Burn B = BurnAt(*M, X, Y);
						B.A *= Keep;
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
				if (Held[P] && Burnt[P] > 0.0) ++Out.Held;
		}
		Out.Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count();
		return Out;
	}

	/** Lays it into the tiles' near levels (after the ground's own triangles, which are left as they are), their heights'
	 * bounds widened to hold it. */
	inline void Into(std::vector<LandField::Tile>& Tiles, const Laid& L)
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
	}
}
