#include "Battle/ShipPaintCore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace ShipPaintCore
{
	namespace
	{
		// ---- Today's sum, as the engine works it

		/** One channel of FMath::Lerp over two FLinearColors, as the engine works it: B less A, times the share, added to A
		 * (FLinearColor's own operator-, operator* by a float and operator+). Each is its own statement, so that no compiler
		 * fuses the multiply and the add into one rounding where the engine's three calls round three times. */
		float LerpOne(float A, float B, float Share)
		{
			const float Apart = B - A;
			const float Part = Apart * Share;
			return A + Part;
		}

		Linear Lerp(const Linear& A, const Linear& B, float Share)
		{
			Linear Out;
			Out.R = LerpOne(A.R, B.R, Share);
			Out.G = LerpOne(A.G, B.G, Share);
			Out.B = LerpOne(A.B, B.B, Share);
			Out.A = LerpOne(A.A, B.A, Share);
			return Out;
		}

		// ---- The ramp's steps, in the engine's linear light

		Linear FromSrgb(float R, float G, float B)
		{
			Linear Out;
			Out.R = ToLinear(R / 255.0f);
			Out.G = ToLinear(G / 255.0f);
			Out.B = ToLinear(B / 255.0f);
			return Out;
		}

		struct Steps
		{
			Linear Source, SourceDark, SourceLight, Body, Foot, Salt;
			float RedMost = 0.0f; // (135 of 255, linear)
			Linear DeckWood; // (a wooden deck's most: DeckWoodMost)
		};

		/** The art director's cap on one channel of a wooden deck, less the mottle's most, down to a whole sRGB level. */
		float DeckLevel(float Cap)
		{
			return ToLinear(std::floor(ToSrgb(ToLinear(Cap / 255.0f) / WoodMottleMost) * 255.0f) / 255.0f);
		}

		const Steps& TheSteps()
		{
			static const Steps Made = []()
			{
				Steps S;
				S.Source = FromSrgb(ShipPaintCore::Source.R, ShipPaintCore::Source.G, ShipPaintCore::Source.B);
				S.SourceDark = FromSrgb(ShipPaintCore::SourceDark.R, ShipPaintCore::SourceDark.G, ShipPaintCore::SourceDark.B);
				S.SourceLight = FromSrgb(ShipPaintCore::SourceLight.R, ShipPaintCore::SourceLight.G, ShipPaintCore::SourceLight.B);
				S.Body = FromSrgb(ShipPaintCore::Body.R, ShipPaintCore::Body.G, ShipPaintCore::Body.B);
				S.Foot = FromSrgb(ShipPaintCore::Foot.R, ShipPaintCore::Foot.G, ShipPaintCore::Foot.B);
				S.Salt = FromSrgb(SaltR, SaltG, SaltB);
				S.RedMost = ToLinear(135.0f / 255.0f);
				S.DeckWood.R = DeckLevel(DeckR);
				S.DeckWood.G = DeckLevel(DeckG);
				S.DeckWood.B = DeckLevel(DeckB);
				return S;
			}();
			return Made;
		}

		// ---- The look's own hash (never a draw of the battle's: the point's place on her, to the millimetre)

		uint32_t Mix(uint32_t H)
		{
			H ^= H >> 16;
			H *= 0x7FEB352Du;
			H ^= H >> 15;
			H *= 0x846CA68Bu;
			H ^= H >> 16;
			return H;
		}

		uint32_t CellHash(int64_t I, int64_t J, int64_t K, uint32_t Seed)
		{
			uint32_t H = Mix(static_cast<uint32_t>(I) * 0x8DA6B343u ^ Seed);
			H = Mix(H ^ (static_cast<uint32_t>(J) * 0xD8163841u));
			return Mix(H ^ (static_cast<uint32_t>(K) * 0xCB1AB31Fu));
		}

		float UnitOf(uint32_t H)
		{
			return static_cast<float>(H >> 8) * (1.0f / 16777216.0f);
		}

		float Smooth(float T)
		{
			return T * T * (3.0f - 2.0f * T);
		}

		int64_t Millimetres(double Metres)
		{
			return static_cast<int64_t>(std::llround(Metres * 1000.0));
		}

		int64_t FloorDiv(int64_t A, int64_t B)
		{
			const int64_t Q = A / B;
			return (A % B != 0 && ((A < 0) != (B < 0))) ? Q - 1 : Q;
		}

		/** Smooth value noise, 0 to 1, over cells of Size millimetres (a point in millimetres). */
		float Noise(int64_t X, int64_t Y, int64_t Z, int64_t Size, uint32_t Seed)
		{
			const int64_t CX = FloorDiv(X, Size), CY = FloorDiv(Y, Size), CZ = FloorDiv(Z, Size);
			const float FX = Smooth(static_cast<float>(X - CX * Size) / static_cast<float>(Size));
			const float FY = Smooth(static_cast<float>(Y - CY * Size) / static_cast<float>(Size));
			const float FZ = Smooth(static_cast<float>(Z - CZ * Size) / static_cast<float>(Size));
			float Corner[2][2][2];
			for (int I = 0; I < 2; ++I)
			{
				for (int J = 0; J < 2; ++J)
				{
					for (int K = 0; K < 2; ++K)
					{
						Corner[I][J][K] = UnitOf(CellHash(CX + I, CY + J, CZ + K, Seed));
					}
				}
			}
			auto Along = [](float A, float B, float T) { return A + (B - A) * T; };
			const float Y0 = Along(Along(Corner[0][0][0], Corner[1][0][0], FX), Along(Corner[0][1][0], Corner[1][1][0], FX), FY);
			const float Y1 = Along(Along(Corner[0][0][1], Corner[1][0][1], FX), Along(Corner[0][1][1], Corner[1][1][1], FX), FY);
			return Along(Y0, Y1, FZ);
		}

		float Clamp01(float V)
		{
			return std::min(1.0f, std::max(0.0f, V));
		}

		Linear Scaled(const Linear& C, float By)
		{
			Linear Out = C;
			Out.R *= By;
			Out.G *= By;
			Out.B *= By;
			return Out;
		}
	}

	float ToLinear(float Srgb)
	{
		const double C = std::min(1.0, std::max(0.0, static_cast<double>(Srgb)));
		return static_cast<float>(C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4));
	}

	float ToSrgb(float Lin)
	{
		const double C = std::min(1.0, std::max(0.0, static_cast<double>(Lin)));
		return static_cast<float>(C <= 0.0031308 ? C * 12.92 : 1.055 * std::pow(C, 1.0 / 2.4) - 0.055);
	}

	float Brightness(const Linear& C)
	{
		return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B;
	}

	Linear Ramp(const Point& P, Laid* Out)
	{
		const Steps& K = TheSteps();
		const float Worn = static_cast<float>(std::min(100, std::max(0, P.Wear))) / 100.0f;
		const int64_t QX = Millimetres(P.X), QY = Millimetres(P.Y), QZ = Millimetres(P.Z);
		// (each run its own strength and tone: a slow noise along her length, its own on each side, in cells of 30 and 26 cm
		// so that the runs beside one another, 0.25 to 1.8 m apart on a U-boat, are seldom alike; and a fine grain, a few
		// centimetres, for the edge)
		const uint32_t Side = P.Y >= 0.0 ? 0x51DEu : 0x0B0Bu;
		const float Strength = Noise(QX, 0, 0, 300, 0x5EA1u ^ Side);
		const float Tone = Noise(QX + 211, 0, 0, 260, 0x70E5u ^ Side);
		const float Grain = 2.0f * Noise(QX, QY, QZ, 55, 0x7357u) - 1.0f;
		// Where on the ramp: much wear its source, a little its film; the film's own place roughened by the grain.
		const float Base = Worn * (0.60f + 0.70f * Strength);
		const float Along = Clamp01(Base * (1.0f + 0.6f * Grain * (1.0f - Worn)));
		// Its colour: the foot's at 0, the body's at 0.45, the source's from 0.9 (each run's own, within the art director's
		// 52,34,28 to 66,42,32); each run toned a little along it. The foot's film only at the foot of a run with its body
		// above it (her rule 3, 25 November 20:28Z), and never on her salt nor beside it (26 November): anywhere else the
		// ramp starts at its body.
		const float Toned = Clamp01(Along + 0.10f * (Tone - 0.5f));
		const float Hued = P.bBodyAbove && !P.bBesideSalt ? Toned : std::max(Toned, 0.45f);
		const Linear Spring = Lerp(K.SourceDark, K.SourceLight, Tone);
		const Linear Colour = Hued < 0.45f ? Lerp(K.Foot, K.Body, Hued / 0.45f) : Lerp(K.Body, Spring, std::min(1.0f, (Hued - 0.45f) / 0.45f));
		// How much of it is laid over the coat: the film no more than 0.35, the body about 0.45 to 0.70, the source 0.8 to 1.
		float Over = Along < 0.15f ? 0.33f * Along / 0.15f
			: Along < 0.45f ? 0.33f + 0.25f * (Along - 0.15f) / 0.30f
			: Along < 0.90f ? 0.58f + 0.32f * (Along - 0.45f) / 0.45f
			: 0.90f + 0.10f * (Along - 0.90f) / 0.10f;
		// (where the stain thins out to nothing, the fringe of a run and an edge's lightest chips, the grain decides whether
		// it is there at all: a ragged edge, never a line; what the plan counts as worn, wear above 4 of 100, keeps its film)
		Over *= Smooth(Clamp01(Base / 0.02f + Grain));
		Linear Coat = Lerp(P.Paint, Colour, Over);
		Coat.A = P.Paint.A;
		// Never lighter than the paint under it: at the source no more than half its brightness, at the foot still a tenth
		// darker, the ceiling easing up to the paint's own as the film thins to nothing (no edge drawn where it ends). On a
		// dark paint, where the rust's own colour is the brighter, it shows by its warmth at the paint's own brightness.
		const float Under = Brightness(P.Paint);
		float Ceiling = 0.9f + (0.5f - 0.9f) * Along;
		if (Under > 0.0f)
		{
			Ceiling = std::max(Ceiling, std::min(1.0f, Brightness(Colour) / Under));
		}
		Ceiling = 1.0f - (1.0f - Ceiling) * std::min(1.0f, Over / 0.35f);
		const float Now = Brightness(Coat);
		if (Under > 0.0f && Now > Ceiling * Under)
		{
			Coat = Scaled(Coat, Ceiling * Under / Now);
			Coat.A = P.Paint.A;
		}
		// Never red above 135, nor red more than 80 over blue (orange), unless the paint itself was.
		const float PaintRed = ToSrgb(P.Paint.R) * 255.0f, PaintBlue = ToSrgb(P.Paint.B) * 255.0f;
		const float RedMost = std::max(135.0f, PaintRed);
		if (ToSrgb(Coat.R) * 255.0f > RedMost)
		{
			Coat.R = ToLinear(RedMost / 255.0f);
		}
		const float OverBlue = std::max(80.0f, PaintRed - PaintBlue);
		const float Red = ToSrgb(Coat.R) * 255.0f, Blue = ToSrgb(Coat.B) * 255.0f;
		if (Red - Blue > OverBlue)
		{
			Coat.R = ToLinear((Blue + OverBlue) / 255.0f);
		}
		if (Out)
		{
			Out->Along = Along;
			Out->Colour = Colour;
			Out->Over = Over;
			Out->Ceiling = Ceiling;
			Out->Foot = Hued < 0.45f ? 1.0f - Hued / 0.45f : 0.0f;
		}
		return Coat;
	}

	int32_t StoredLevel(float Lin)
	{
		// (as ToFColor(true) stores it: the standard's curve, worked in double, rounded half up)
		const double C = std::min(1.0, std::max(0.0, static_cast<double>(Lin)));
		const double S = C <= 0.0031308 ? C * 12.92 : 1.055 * std::pow(C, 1.0 / 2.4) - 0.055;
		return static_cast<int32_t>(std::floor(S * 255.0 + 0.5));
	}

	float LevelLinear(int32_t Level)
	{
		// (as FLinearColor reads a byte back: the standard's curve from Level / 255, worked in double; one table, made once)
		static const std::vector<float> Table = []()
		{
			std::vector<float> Made(256);
			for (int K = 0; K < 256; ++K)
			{
				const double C = K / 255.0;
				Made[static_cast<size_t>(K)] = static_cast<float>(C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4));
			}
			return Made;
		}();
		return Table[static_cast<size_t>(std::min(255, std::max(0, Level)))];
	}

	Linear LightWear(const Point& P, const Linear& Unworn)
	{
		// (her rule 2, worked in sRGB levels as she measures, against the point unworn as the mesh stores it: Was its bytes,
		// Top the most each channel may take, never above the unworn point's own linear colour)
		const float UnwornLin[3] = { Unworn.R, Unworn.G, Unworn.B };
		const float PaintLin[3] = { P.Paint.R, P.Paint.G, P.Paint.B };
		int32_t Was[3], Top[3];
		for (int C = 0; C < 3; ++C)
		{
			Was[C] = StoredLevel(UnwornLin[C]);
			int32_t Most = Was[C];
			while (Most > 0 && LevelLinear(Most) > UnwornLin[C])
			{
				--Most;
			}
			Top[C] = Most;
		}
		const auto Bright = [](double R, double G, double B) { return 0.2126 * R + 0.7152 * G + 0.0722 * B; };
		const auto Spread = [](double R, double G, double B) { return std::max({ R, G, B }) - std::min({ R, G, B }); };
		const double R = Was[0], G = Was[1], B = Was[2];
		const double Y = Bright(R, G, B), Colour = Spread(R, G, B);
		// Her line (26 November): 3% darker at a wear of 5, rising evenly to 6% at 19; greyer by 1% to 14% as the wear grows
		// from 1 to 19, as round 1c's.
		const double Dark = static_cast<double>(LightDarkAt(P.Wear));
		const double Worn = std::min(1.0, std::max(0.0, (static_cast<double>(P.Wear) - 1.0) / static_cast<double>(RunBodyWear - 2)));
		double Grey = LightGreyFrom + (LightGreyTo - LightGreyFrom) * Worn;
		// (greyer only so far as red less blue rises by no more than 2 levels: on a grey-blue paint, taking colour away takes
		// blue away from over red; and no channel taken up toward the grey past the unworn point's own)
		if (B > R)
		{
			Grey = std::min(Grey, 1.0 - (1.0 - (LightWarmMost - 1.0) / (B - R)) / (1.0 - Dark));
		}
		for (const double C : { R, G, B })
		{
			if (C < Y)
			{
				Grey = std::min(Grey, Dark / (1.0 - Dark) * C / (Y - C));
			}
		}
		Grey = std::max(0.0, Grey);
		const auto Moved = [Y, Grey, Dark](double C) { return (1.0 - Dark) * (C + Grey * (Y - C)); };
		const double Aim[3] = { Moved(R), Moved(G), Moved(B) };
		// Whole levels that keep all of her bounds in the bytes themselves: 3 to 6% darker, 0 to 15% greyer (no colour where
		// the unworn point has none), red less blue up by 3 at most (Warmest), no channel above the unworn point nor above the
		// paint; and, while whole levels can, within LightDarkBand of her line (bBand). How far it is from her line.
		const auto Off = [&](const int32_t* L) { return std::abs((1.0 - Bright(L[0], L[1], L[2]) / Y) - Dark); };
		const auto Keeps = [&](const int32_t* L, bool bBand, int32_t Warmest)
		{
			if ((L[0] - L[2]) - (Was[0] - Was[2]) > Warmest || Y <= 0.0)
			{
				return false;
			}
			const double Now = Bright(L[0], L[1], L[2]);
			const double Darker = 1.0 - Now / Y;
			if (Darker < LightDarkLeast || Darker > LightDarkMost || (bBand && std::abs(Darker - Dark) > static_cast<double>(LightDarkBand)))
			{
				return false;
			}
			const double Has = Spread(L[0], L[1], L[2]);
			if (Colour > 0.0)
			{
				const double Greyer = 1.0 - (Has / Now) / (Colour / Y);
				if (Greyer < 0.0 || Greyer > LightGreyMost)
				{
					return false;
				}
			}
			else if (Has > 0.0)
			{
				return false;
			}
			for (int C = 0; C < 3; ++C)
			{
				if (LevelLinear(L[C]) > PaintLin[C])
				{
					return false;
				}
			}
			return true;
		};
		// The nearest to the aim, looked for in a cube of one level about it, then two, then LightSearch: first within the
		// band and red less blue up by no more than the aim's 2 (as neutral as round 1c kept it), then up by her 3; where whole
		// levels cannot keep the band, the one whose darkening is nearest her line (then nearest the aim).
		const int32_t Near[3] = { static_cast<int32_t>(std::lround(Aim[0])), static_cast<int32_t>(std::lround(Aim[1])), static_cast<int32_t>(std::lround(Aim[2])) };
		int32_t Best[3] = { std::min(Near[0], Top[0]), std::min(Near[1], Top[1]), std::min(Near[2], Top[2]) };
		bool bFound = false;
		struct Stage
		{
			bool bBand;
			int32_t Warmest;
		};
		const int32_t Warm = static_cast<int32_t>(LightWarmMost);
		for (const Stage Now : { Stage{ true, Warm - 1 }, Stage{ true, Warm }, Stage{ false, Warm } })
		{
			const bool bBand = Now.bBand;
			for (const int32_t Reach : { 1, 2, LightSearch })
			{
				double Least = 1e30, LeastOff = 1e30;
				for (int32_t DR = -Reach; DR <= Reach; ++DR)
				{
					for (int32_t DG = -Reach; DG <= Reach; ++DG)
					{
						for (int32_t DB = -Reach; DB <= Reach; ++DB)
						{
							const int32_t L[3] = { Near[0] + DR, Near[1] + DG, Near[2] + DB };
							if (L[0] < 0 || L[1] < 0 || L[2] < 0 || L[0] > Top[0] || L[1] > Top[1] || L[2] > Top[2] || !Keeps(L, bBand, Now.Warmest))
							{
								continue;
							}
							const double Apart = (L[0] - Aim[0]) * (L[0] - Aim[0]) + (L[1] - Aim[1]) * (L[1] - Aim[1]) + (L[2] - Aim[2]) * (L[2] - Aim[2]);
							const double FromLine = bBand ? 0.0 : Off(L);
							if (FromLine < LeastOff || (FromLine == LeastOff && Apart < Least))
							{
								LeastOff = FromLine;
								Least = Apart;
								Best[0] = L[0];
								Best[1] = L[1];
								Best[2] = L[2];
								bFound = true;
							}
						}
					}
				}
				if (bFound)
				{
					break;
				}
			}
			if (bFound)
			{
				break;
			}
		}
		Linear Out;
		Out.R = LevelLinear(std::max(0, Best[0]));
		Out.G = LevelLinear(std::max(0, Best[1]));
		Out.B = LevelLinear(std::max(0, Best[2]));
		Out.A = Unworn.A;
		return Out;
	}

	Linear NoneAbove(Linear C, const Linear& Most)
	{
		C.R = std::min(C.R, Most.R);
		C.G = std::min(C.G, Most.G);
		C.B = std::min(C.B, Most.B);
		return C;
	}

	namespace
	{
		/** Where her salt lies: red less blue no higher than her salt makes it on the clean paint, both in levels as she
		 * measures them and in the whole levels the mesh stores (Salted's own bytes; held there to a whole level, so the
		 * engine stores that level), only red moving. */
		Linear NoWarmerThan(Linear C, const Linear& Salted)
		{
			const float Room = ToSrgb(Salted.R) - ToSrgb(Salted.B);
			if (ToSrgb(C.R) - ToSrgb(C.B) > Room)
			{
				C.R = std::min(C.R, ToLinear(ToSrgb(C.B) + Room));
			}
			const int32_t StoredRoom = StoredLevel(Salted.R) - StoredLevel(Salted.B);
			const int32_t StoredRed = StoredLevel(C.B) + StoredRoom;
			if (StoredLevel(C.R) > StoredRed)
			{
				C.R = std::min(C.R, LevelLinear(StoredRed));
			}
			return C;
		}
	}

	Linear Held(Linear C, const Linear& Paint, const Linear& Salted, bool bNoWarmer, bool* bRedHeld)
	{
		if (bRedHeld)
		{
			*bRedHeld = false;
		}
		// (no channel above the paint's own, or where her salt lies, above that salt over the clean paint: a deposit, paler)
		C.R = std::min(C.R, std::max(Paint.R, Salted.R));
		C.G = std::min(C.G, std::max(Paint.G, Salted.G));
		C.B = std::min(C.B, std::max(Paint.B, Salted.B));
		if (bNoWarmer && C.R >= Paint.R && C.G >= Paint.G && C.B >= Paint.B)
		{
			// (lifted by the salt in every channel, it reads as salt: red less blue, in the levels stored, no higher than her
			// salt makes it on the clean paint, so the rust under the salt never warms it; held before the hold to the paint
			// below, so a red it takes under the paint takes green and blue to the paint with it)
			C = NoWarmerThan(C, Salted);
		}
		// (and below the paint in any channel, above it in none: the salt pales a stain, it never warms it, so red never rises
		// while blue falls, the pink she ruled out)
		if (C.R < Paint.R || C.G < Paint.G || C.B < Paint.B)
		{
			C = NoneAbove(C, Paint);
		}
		if (bNoWarmer)
		{
			// (her call 3 of 26 November, hold the rust beside salt: where her salt lies but cannot lift the stain in every
			// channel, rule 1 keeps red at the paint while green and blue stay down, warmer than the salt beside it, and across
			// a triangle from a pale salted corner red rises while blue falls, salmon. Never warmer than her salt on the clean
			// paint, lifted or not: only red comes down, so nothing rises above the paint)
			const float Red = C.R;
			C = NoWarmerThan(C, Salted);
			if (bRedHeld)
			{
				*bRedHeld = C.R != Red;
			}
		}
		return C;
	}

	Linear HerPaintDarker(const Linear& Paint, const Linear& Kept, const Linear& Salted)
	{
		// (her call A of 26 November 06:31Z, worked in sRGB levels as she measures: the paint's own levels, what round 1d's
		// hold left, and the most each channel may take, never above the paint's own linear colour)
		const float PaintLin[3] = { Paint.R, Paint.G, Paint.B }, KeptLin[3] = { Kept.R, Kept.G, Kept.B };
		double Own[3], Was[3];
		int32_t Top[3];
		for (int C = 0; C < 3; ++C)
		{
			Own[C] = static_cast<double>(ToSrgb(PaintLin[C])) * 255.0;
			Was[C] = static_cast<double>(ToSrgb(KeptLin[C])) * 255.0;
			if (!(Own[C] > 0.0))
			{
				return Kept;
			}
			int32_t Most = StoredLevel(PaintLin[C]);
			while (Most > 0 && LevelLinear(Most) > PaintLin[C])
			{
				--Most;
			}
			Top[C] = Most;
		}
		// (her salt's room over the clean paint, red less blue: in levels as the test measures them, and in the bytes stored)
		const float Room = ToSrgb(Salted.R) * 255.0f - ToSrgb(Salted.B) * 255.0f;
		const int32_t StoredRoom = StoredLevel(Salted.R) - StoredLevel(Salted.B);
		// (the darkness round 1d gave it: the least of its channels over the paint's; tried first, then each 1/1024 above it)
		const double Least = std::min({ Was[0] / Own[0], Was[1] / Own[1], Was[2] / Own[2] });
		const int32_t From = static_cast<int32_t>(std::floor(Least * 1024.0));
		for (int32_t Step = std::max(0, From); Step <= 1024; ++Step)
		{
			const double By = Step == From ? Least : static_cast<double>(Step) / 1024.0;
			const double Aim[3] = { Own[0] * By, Own[1] * By, Own[2] * By };
			int32_t Best[3] = { 0, 0, 0 };
			double Least2 = 1e30;
			for (int32_t R = static_cast<int32_t>(std::ceil(Aim[0] - 1.0)); R <= static_cast<int32_t>(std::floor(Aim[0] + 1.0)); ++R)
			{
				for (int32_t G = static_cast<int32_t>(std::ceil(Aim[1] - 1.0)); G <= static_cast<int32_t>(std::floor(Aim[1] + 1.0)); ++G)
				{
					for (int32_t B = static_cast<int32_t>(std::ceil(Aim[2] - 1.0)); B <= static_cast<int32_t>(std::floor(Aim[2] + 1.0)); ++B)
					{
						if (R < 0 || G < 0 || B < 0 || R > Top[0] || G > Top[1] || B > Top[2])
						{
							continue;
						}
						// (the paint's own balance, to a level: green less red and blue less green as the paint's so scaled)
						if (std::abs(static_cast<double>(G - R) - (Aim[1] - Aim[0])) > 1.0 || std::abs(static_cast<double>(B - G) - (Aim[2] - Aim[1])) > 1.0)
						{
							continue;
						}
						// (never warmer than her salt on the clean paint, in levels and in the bytes stored)
						if (R - B > StoredRoom || ToSrgb(LevelLinear(R)) * 255.0f - ToSrgb(LevelLinear(B)) * 255.0f > Room)
						{
							continue;
						}
						const double Apart = (R - Aim[0]) * (R - Aim[0]) + (G - Aim[1]) * (G - Aim[1]) + (B - Aim[2]) * (B - Aim[2]);
						if (Apart < Least2)
						{
							Least2 = Apart;
							Best[0] = R;
							Best[1] = G;
							Best[2] = B;
						}
					}
				}
			}
			if (Least2 < 1e30)
			{
				Linear Out;
				Out.R = LevelLinear(Best[0]);
				Out.G = LevelLinear(Best[1]);
				Out.B = LevelLinear(Best[2]);
				Out.A = Kept.A;
				return Out;
			}
		}
		return Kept;
	}

	bool PaintsAsUnworn(const Point& P, const Options& O)
	{
		// (light wear away from a run, her rule 2: below a wear of 5 at the paint, and under her salt the salt as on the point
		// unworn, her ruling of 26 November; and a run's body under her salt on a foot's salt line, her ruling of 06:31Z: there
		// salt beats a run's body. Coated paints exactly these as the point unworn)
		if (!O.bRustRamp || !P.bPainted || P.Wear <= 0)
		{
			return false;
		}
		if (P.Wear >= BodyOverSaltWear)
		{
			return P.bFootLine && P.Salt > 0;
		}
		return P.Wear < RunBodyWear && !P.bBodyAbove && (P.Wear < LightWearFrom || P.Salt > 0);
	}

	uint8_t SteelFinish(const Point& P, const Options& O)
	{
		// (the reader's 128 less half the wear, today's; a point the skin paints as unworn keeps the unworn 128, the material's
		// metal as on the point unworn, so its whole vertex is the unworn point's)
		const int32_t Worn = PaintsAsUnworn(P, O) ? 0 : std::min(100, std::max(0, P.Wear));
		return static_cast<uint8_t>(128 - Worn / 2);
	}

	std::vector<uint8_t> BodyAbove(const std::vector<RunPoint>& Points, const int32_t* Triangles, size_t Indices)
	{
		const size_t Count = Points.size();
		std::vector<uint8_t> Marks(Count, 0);
		// (a stained point: painted steel with wear above 0; each to the millimetre, one node for each place, so that the
		// copies a mesh makes of a point at a hard edge count as one)
		struct Place
		{
			int64_t X = 0, Y = 0, Z = 0;
			bool operator==(const Place& Other) const { return X == Other.X && Y == Other.Y && Z == Other.Z; }
		};
		struct PlaceHash
		{
			size_t operator()(const Place& At) const
			{
				return static_cast<size_t>(CellHash(At.X, At.Y, At.Z, 0x2A11u));
			}
		};
		const auto Stained = [](const RunPoint& One) { return One.bPainted && One.Wear > 0; };
		std::vector<Place> At(Count);
		std::vector<int32_t> Node(Count, -1);
		std::unordered_map<Place, int32_t, PlaceHash> NodeOf;
		for (size_t K = 0; K < Count; ++K)
		{
			if (Stained(Points[K]))
			{
				At[K] = { Millimetres(Points[K].X), Millimetres(Points[K].Y), Millimetres(Points[K].Z) };
				const int32_t Next = static_cast<int32_t>(NodeOf.size());
				Node[K] = NodeOf.emplace(At[K], Next).first->second;
			}
		}
		// (the patches: nodes joined by a triangle's side with both its ends stained; each patch known by its least node)
		std::vector<int32_t> Parent(NodeOf.size());
		for (size_t K = 0; K < Parent.size(); ++K)
		{
			Parent[K] = static_cast<int32_t>(K);
		}
		const auto Find = [&Parent](int32_t A)
		{
			while (Parent[static_cast<size_t>(A)] != A)
			{
				Parent[static_cast<size_t>(A)] = Parent[static_cast<size_t>(Parent[static_cast<size_t>(A)])];
				A = Parent[static_cast<size_t>(A)];
			}
			return A;
		};
		const auto NodeAt = [&Node, Count](int32_t K) { return K >= 0 && static_cast<size_t>(K) < Count ? Node[static_cast<size_t>(K)] : -1; };
		for (size_t T = 0; T + 2 < Indices; T += 3)
		{
			const int32_t Ends[3] = { NodeAt(Triangles[T]), NodeAt(Triangles[T + 1]), NodeAt(Triangles[T + 2]) };
			for (int Side = 0; Side < 3; ++Side)
			{
				const int32_t A = Ends[Side], B = Ends[(Side + 1) % 3];
				if (A >= 0 && B >= 0)
				{
					const int32_t RootA = Find(A), RootB = Find(B);
					if (RootA != RootB)
					{
						Parent[static_cast<size_t>(std::max(RootA, RootB))] = std::min(RootA, RootB);
					}
				}
			}
		}
		// (each patch's body, wear of RunBodyWear and more, in order of its patch and then of its height)
		struct BodyAt
		{
			int32_t Patch = 0;
			int64_t Z = 0, X = 0, Y = 0;
			size_t Point = 0;
		};
		std::vector<BodyAt> Bodies;
		for (size_t K = 0; K < Count; ++K)
		{
			if (Node[K] >= 0 && Points[K].Wear >= RunBodyWear)
			{
				Bodies.push_back({ Find(Node[K]), At[K].Z, At[K].X, At[K].Y, K });
			}
		}
		const auto Before = [](const BodyAt& A, const BodyAt& B) { return A.Patch != B.Patch ? A.Patch < B.Patch : A.Z < B.Z; };
		std::sort(Bodies.begin(), Bodies.end(), Before);
		// A point is marked when a body point of its own patch lies above it: 1 mm to RunLongestMm higher in her own Z, more
		// above than beside it (45 degrees or steeper: a run goes down), no further aside than half a run's foot is wide,
		// RunFootWideMm / 2, plus the rake, RunRakePer10000 of the drop, and on the same face of the plate (their facings
		// not more than a right angle apart, so the underside of a plate never takes the film of its top).
		for (size_t K = 0; K < Count; ++K)
		{
			if (Node[K] < 0)
			{
				continue;
			}
			BodyAt Key;
			Key.Patch = Find(Node[K]);
			Key.Z = At[K].Z;
			const auto First = std::upper_bound(Bodies.begin(), Bodies.end(), Key, Before);
			for (auto It = First; It != Bodies.end() && It->Patch == Key.Patch && It->Z - Key.Z <= RunLongestMm; ++It)
			{
				const int64_t Drop = It->Z - Key.Z;
				const int64_t Reach = RunFootWideMm / 2 + Drop * RunRakePer10000 / 10000;
				const int64_t DX = It->X - At[K].X, DY = It->Y - At[K].Y;
				const int64_t Aside = DX * DX + DY * DY;
				const RunPoint &Low = Points[K], &High = Points[It->Point];
				const bool bSameFace = Low.FX * High.FX + Low.FY * High.FY + Low.FZ * High.FZ >= 0.0;
				if (Aside <= Reach * Reach && Aside <= Drop * Drop && bSameFace)
				{
					Marks[K] = FootMark;
					break;
				}
			}
		}
		// (her ruling of 26 November: the film never on her salt nor beside it. A point at a run's foot is marked SaltMark as
		// well where her salt lies on it, painted steel with salt above 0, or on any corner of a triangle it is a corner of)
		const auto Salted = [&Points, Count](int32_t K)
		{ return K >= 0 && static_cast<size_t>(K) < Count && Points[static_cast<size_t>(K)].bPainted && Points[static_cast<size_t>(K)].Salt > 0; };
		std::vector<uint8_t> Beside(Count, 0);
		for (size_t T = 0; T + 2 < Indices; T += 3)
		{
			const int32_t Corners[3] = { Triangles[T], Triangles[T + 1], Triangles[T + 2] };
			bool bInRange = true, bSalt = false;
			for (const int32_t Corner : Corners)
			{
				bInRange = bInRange && Corner >= 0 && static_cast<size_t>(Corner) < Count;
				bSalt = bSalt || Salted(Corner);
			}
			if (bInRange && bSalt)
			{
				for (const int32_t Corner : Corners)
				{
					Beside[static_cast<size_t>(Corner)] = 1;
				}
			}
		}
		for (size_t K = 0; K < Count; ++K)
		{
			if (Marks[K] != 0 && (Beside[K] != 0 || Salted(static_cast<int32_t>(K))))
			{
				Marks[K] |= SaltMark;
			}
		}
		// (her ruling of 26 November 06:31Z, the foot's salt line: a painted point facing out with a deck at its foot. The decks:
		// triangles whose three corners face up, kept in cells of half a metre by the ground they cover, each with its
		// lowest and highest corner; every point at whole millimetres)
		const auto FacesUp = [](const RunPoint& One)
		{ return One.FZ > 0.0 && One.FZ >= FootLineUp * std::sqrt(One.FX * One.FX + One.FY * One.FY + One.FZ * One.FZ); };
		std::vector<std::array<int64_t, 3>> Mm(Count);
		for (size_t K = 0; K < Count; ++K)
		{
			Mm[K] = { Millimetres(Points[K].X), Millimetres(Points[K].Y), Millimetres(Points[K].Z) };
		}
		constexpr int64_t Cell = 500;
		struct DeckAt
		{
			int32_t A = 0, B = 0, C = 0;
			int64_t Low = 0, High = 0;
		};
		std::vector<DeckAt> Decks;
		std::unordered_map<Place, std::vector<int32_t>, PlaceHash> DecksIn;
		for (size_t T = 0; T + 2 < Indices; T += 3)
		{
			const int32_t Corners[3] = { Triangles[T], Triangles[T + 1], Triangles[T + 2] };
			bool bDeck = true;
			for (const int32_t Corner : Corners)
			{
				bDeck = bDeck && Corner >= 0 && static_cast<size_t>(Corner) < Count && FacesUp(Points[static_cast<size_t>(Corner)]);
			}
			if (!bDeck)
			{
				continue;
			}
			const auto &A = Mm[static_cast<size_t>(Corners[0])], &B = Mm[static_cast<size_t>(Corners[1])], &C = Mm[static_cast<size_t>(Corners[2])];
			DeckAt One;
			One.A = Corners[0];
			One.B = Corners[1];
			One.C = Corners[2];
			One.Low = std::min({ A[2], B[2], C[2] });
			One.High = std::max({ A[2], B[2], C[2] });
			const int32_t Id = static_cast<int32_t>(Decks.size());
			Decks.push_back(One);
			for (int64_t CX = FloorDiv(std::min({ A[0], B[0], C[0] }), Cell); CX <= FloorDiv(std::max({ A[0], B[0], C[0] }), Cell); ++CX)
			{
				for (int64_t CY = FloorDiv(std::min({ A[1], B[1], C[1] }), Cell); CY <= FloorDiv(std::max({ A[1], B[1], C[1] }), Cell); ++CY)
				{
					DecksIn[{ CX, CY, 0 }].push_back(Id);
				}
			}
		}
		for (size_t K = 0; K < Count && !Decks.empty(); ++K)
		{
			const RunPoint& One = Points[K];
			const double Level = std::sqrt(One.FX * One.FX + One.FY * One.FY);
			if (!One.bPainted || FacesUp(One) || !(Level > 0.0))
			{
				continue;
			}
			bool bFoot = false;
			for (int64_t Out = FootLineStepMm; Out <= FootLineReachMm && !bFoot; Out += FootLineStepMm)
			{
				// (the step's place on the ground, to the millimetre, and the decks over it: inside or on a side of the triangle,
				// its height there within the line's reach of the point's, all in whole numbers)
				const int64_t QX = Mm[K][0] + std::llround(static_cast<double>(Out) * One.FX / Level), QY = Mm[K][1] + std::llround(static_cast<double>(Out) * One.FY / Level);
				const auto Found = DecksIn.find({ FloorDiv(QX, Cell), FloorDiv(QY, Cell), 0 });
				if (Found == DecksIn.end())
				{
					continue;
				}
				for (const int32_t Id : Found->second)
				{
					const DeckAt& D = Decks[static_cast<size_t>(Id)];
					if (D.High < Mm[K][2] - FootLineHighMm || D.Low > Mm[K][2] + FootLineSunkMm)
					{
						continue;
					}
					const auto &A = Mm[static_cast<size_t>(D.A)], &B = Mm[static_cast<size_t>(D.B)], &C = Mm[static_cast<size_t>(D.C)];
					int64_t Whole = (B[0] - A[0]) * (C[1] - A[1]) - (B[1] - A[1]) * (C[0] - A[0]);
					int64_t WA = (B[0] - QX) * (C[1] - QY) - (B[1] - QY) * (C[0] - QX);
					int64_t WB = (C[0] - QX) * (A[1] - QY) - (C[1] - QY) * (A[0] - QX);
					int64_t WC = (A[0] - QX) * (B[1] - QY) - (A[1] - QY) * (B[0] - QX);
					if (Whole == 0)
					{
						continue;
					}
					if (Whole < 0)
					{
						Whole = -Whole;
						WA = -WA;
						WB = -WB;
						WC = -WC;
					}
					if (WA < 0 || WB < 0 || WC < 0)
					{
						continue;
					}
					// (the deck's height there over the point's, times Whole: no more than FootLineHighMm below, FootLineSunkMm above)
					const int64_t Over = WA * (A[2] - Mm[K][2]) + WB * (B[2] - Mm[K][2]) + WC * (C[2] - Mm[K][2]);
					if (Over >= -FootLineHighMm * Whole && Over <= FootLineSunkMm * Whole)
					{
						bFoot = true;
						break;
					}
				}
			}
			if (bFoot)
			{
				Marks[K] |= FootLineMark;
			}
		}
		return Marks;
	}

	namespace
	{
		/** Her salt over the clean paint at a point (where it lies). */
		Linear SaltedClean(const Point& P)
		{
			return Lerp(P.Paint, TheSteps().Salt, SaltMost * static_cast<float>(std::min(P.Salt, 100)) / 100.0f);
		}

		/** All Paint lays but the shade. */
		Linear Coated(const Point& P, const Palette& Pal, const Options& O)
		{
			const bool bStained = P.bPainted && P.Wear > 0;
			const bool bSalted = P.bPainted && P.Salt > 0;
			if (PaintsAsUnworn(P, O))
			{
				// (light wear away from a run is not rust, her rule 2: below a wear of 5 it stays at the paint, and under her salt
				// the salt lies as it lies on the point unworn (her ruling of 26 November); and on a foot's salt line her salt
				// beats a run's body (06:31Z): each the point as it would be unworn, her salt and the soot laid, bit for bit)
				Point Bare = P;
				Bare.Wear = 0;
				return Coated(Bare, Pal, O);
			}
			if (O.bRustRamp && bStained && P.Wear < RunBodyWear && !P.bBodyAbove)
			{
				// (from 5, with no salt on it: that point a little darker and greyer, 3% at 5 rising to 6% at 19, in whole levels
				// inside her bounds; held as every stain is, which it keeps)
				Point Bare = P;
				Bare.Wear = 0;
				const Linear Unworn = Coated(Bare, Pal, O);
				return Held(LightWear(P, Unworn), P.Paint, P.Paint, false);
			}
			// (her order for the salt, 26 November: off a foot's salt line a run's body, a wear of BodyOverSaltWear and more,
			// beats her salt, which is not laid there under the skin; the body shows as ramped, held to rule 1. On the foot's
			// line her salt beats it, above)
			const bool bSaltLies = bSalted && !(O.bRustRamp && bStained && P.Wear >= BodyOverSaltWear);
			Linear Coat = P.Paint;
			float Rusted = 0.0f;
			if (bStained)
			{
				if (O.bRustRamp)
				{
					Laid How;
					Coat = Ramp(P, &How);
					Rusted = How.Over;
				}
				else
				{
					Coat = Lerp(Coat, Pal.Rust, 0.55f * P.Wear / 100.0f);
				}
			}
			if (bSaltLies)
			{
				if (O.bRustRamp)
				{
					// (the ruled salt: toward 196,200,198 by no more than 30%, and never over a rust source; the stain under it
					// held first to the paint's own, her rule 1)
					Coat = NoneAbove(Coat, P.Paint);
					Coat = Lerp(Coat, TheSteps().Salt, SaltMost * static_cast<float>(std::min(P.Salt, 100)) / 100.0f * (1.0f - Rusted));
				}
				else
				{
					Coat = Lerp(Coat, Pal.Salt, 0.45f * std::min(P.Salt, 100) / 100.0f);
				}
			}
			if (P.Soot > 4 && Pal.bSoot)
			{
				Coat = Lerp(Coat, Pal.Soot, 0.6f * std::min(P.Soot, 100) / 100.0f);
			}
			if (O.bRustRamp && P.bDeckWood)
			{
				// (the art director's wooden deck under the skin: each channel held under her 150,135,110 with room for the blot
				// mottle the skin's wear lays over what is not metal, so the planks as drawn never pass it; a darker plank keeps
				// its own)
				const Linear& Most = TheSteps().DeckWood;
				Coat.R = std::min(Coat.R, Most.R);
				Coat.G = std::min(Coat.G, Most.G);
				Coat.B = std::min(Coat.B, Most.B);
			}
			if (O.bRustRamp)
			{
				// (her rule 1, on what the point shows once the skin has laid all it lays. A stain: no channel above the paint's
				// own; on a grey-blue paint the ramp's red would rise, a pink wash; held, the stain shows by the green and blue
				// it takes away, darker and warmer; where soot darkens it first, as much as is left. Where her salt lies it may
				// pale the point, in every channel at once, no further than the salt takes the clean paint and no warmer than it
				// makes it, never red up with blue down (Held): on a stain, and on the point unworn, where soot laid over the
				// salt would otherwise leave red a fraction over the paint with blue under it. A point the skin leaves unworn
				// and unsalted: its paint at most; the deck's hold only lowers.)
				const Linear Salted = bSaltLies ? SaltedClean(P) : P.Paint;
				if (bStained || bSaltLies)
				{
					// (a stain, and her salt on the point unworn, and so on light wear, which shows it as it lies there: held, and
					// where her salt lies, paler or where soot takes it back under the paint, never warmer than her salt makes the
					// clean paint, red less blue in the levels stored; her salt alone on the clean paint is that salt, bit for bit)
					bool bRedHeld = false;
					Coat = Held(Coat, P.Paint, Salted, bSaltLies, &bRedHeld);
					if (bStained && bSaltLies && bRedHeld)
					{
						// (her call A of 26 November 06:31Z: her salted rust that the salt cannot lift, which that hold made a cold
						// grey with red alone brought down, is her paint, darker: green and blue come down with red)
						Coat = HerPaintDarker(P.Paint, Coat, Salted);
					}
				}
				else
				{
					Coat = NoneAbove(Coat, P.Paint);
				}
			}
			return Coat;
		}
	}

	Linear Paint(const Point& P, const Palette& Pal, const Options& O)
	{
		Linear Coat = Coated(P, Pal, O);
		if (O.bShade && P.Shade >= 0.0f && P.Shade < 1.0f)
		{
			// (as a figure's worn cloth: the colour times the point's shade, its alpha kept)
			Coat.R = Coat.R * P.Shade;
			Coat.G = Coat.G * P.Shade;
			Coat.B = Coat.B * P.Shade;
		}
		return Coat;
	}

	Steel SteelOf(const Options& O, float FirstLookWear)
	{
		Steel Out;
		const bool bSkin = FirstLookWear > 0.0f, bLight = O.TopLight > 0.0f;
		if (!bSkin && !bLight)
		{
			return Out; // (every switch off: the ink as made, nothing set)
		}
		// (the material lifts what faces the sky by up x TopLight x metal x Wear: so the Wear is the skin's first look if it
		// is on, and else the light alone's, too little to lay a mottle anyone could see; and the TopLight set is the light
		// asked over that Wear, so the lift is the same either way, as a Wear of 1 gives with the light itself (1 + the
		// light x the metal: 254/255 of the light on unworn steel). With the skin and no light, none, as the sea has always
		// drawn: never the map's 0.26)
		Out.Wear = bSkin ? FirstLookWear : SeaLightAloneWear;
		Out.TopLight = (bLight ? O.TopLight : SeaTopLightToday) / Out.Wear;
		Out.bChipsOff = true;
		Out.bBlots = bSkin;
		return Out;
	}

	Linear DeckWoodMost()
	{
		return TheSteps().DeckWood;
	}
}
