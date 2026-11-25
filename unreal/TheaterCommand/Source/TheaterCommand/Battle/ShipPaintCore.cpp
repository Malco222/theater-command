#include "Battle/ShipPaintCore.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

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
		// 52,34,28 to 66,42,32); each run toned a little along it.
		const float Toned = Clamp01(Along + 0.10f * (Tone - 0.5f));
		const Linear Spring = Lerp(K.SourceDark, K.SourceLight, Tone);
		const Linear Colour = Toned < 0.45f ? Lerp(K.Foot, K.Body, Toned / 0.45f) : Lerp(K.Body, Spring, std::min(1.0f, (Toned - 0.45f) / 0.45f));
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
		}
		return Coat;
	}

	Linear Paint(const Point& P, const Palette& Pal, const Options& O)
	{
		Linear Coat = P.Paint;
		float Rusted = 0.0f;
		if (P.bPainted && P.Wear > 0)
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
		if (P.Salt > 0 && P.bPainted)
		{
			if (O.bRustRamp)
			{
				// (the ruled salt: toward 196,200,198 by no more than 30%, and never over a rust source)
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
		Out.Wear = std::max(0.0f, FirstLookWear);
		Out.bChipsOff = Out.Wear > 0.0f;
		// (the material lifts what faces the sky only where its Wear is on: once the first look turns it on, the lift is the
		// light switch's if asked and otherwise none, as the sea has always drawn; never the map's 0.26)
		Out.TopLight = O.TopLight >= 0.0f ? O.TopLight : Out.Wear > 0.0f ? SeaTopLightToday : -1.0f;
		return Out;
	}

	Linear DeckWoodMost()
	{
		return TheSteps().DeckWood;
	}
}
