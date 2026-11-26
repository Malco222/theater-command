#include "Battle/ShipPaintCore.h"

#include <algorithm>
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
		// above it (her rule 3, 25 November 20:28Z): anywhere else the ramp starts at its body.
		const float Toned = Clamp01(Along + 0.10f * (Tone - 0.5f));
		const float Hued = P.bBodyAbove ? Toned : std::max(Toned, 0.45f);
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
		// Her aim: a little darker and greyer at a wear of 1, the most at 19.
		const double Worn = std::min(1.0, std::max(0.0, (static_cast<double>(P.Wear) - 1.0) / static_cast<double>(RunBodyWear - 2)));
		const double Dark = LightDarkFrom + (LightDarkTo - LightDarkFrom) * Worn;
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
		// the unworn point has none), red less blue up by 3 at most, no channel above the unworn point, and never above the
		// paint in one channel while below it in another (her rule 1, never pink). The nearest to the aim, looked for in a
		// cube of one level about it, then two, then LightSearch.
		const auto Keeps = [&](const int32_t* L)
		{
			if ((L[0] - L[2]) - (Was[0] - Was[2]) > static_cast<int32_t>(LightWarmMost) || Y <= 0.0)
			{
				return false;
			}
			const double Now = Bright(L[0], L[1], L[2]);
			const double Darker = 1.0 - Now / Y;
			if (Darker < LightDarkLeast || Darker > LightDarkMost)
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
			bool bAbove = false, bBelow = false;
			for (int C = 0; C < 3; ++C)
			{
				bAbove = bAbove || LevelLinear(L[C]) > PaintLin[C];
				bBelow = bBelow || LevelLinear(L[C]) < PaintLin[C];
			}
			return !(bAbove && bBelow);
		};
		const int32_t Near[3] = { static_cast<int32_t>(std::lround(Aim[0])), static_cast<int32_t>(std::lround(Aim[1])), static_cast<int32_t>(std::lround(Aim[2])) };
		int32_t Best[3] = { std::min(Near[0], Top[0]), std::min(Near[1], Top[1]), std::min(Near[2], Top[2]) };
		bool bFound = false;
		for (const int32_t Reach : { 1, 2, LightSearch })
		{
			double Least = 1e30;
			for (int32_t DR = -Reach; DR <= Reach; ++DR)
			{
				for (int32_t DG = -Reach; DG <= Reach; ++DG)
				{
					for (int32_t DB = -Reach; DB <= Reach; ++DB)
					{
						const int32_t L[3] = { Near[0] + DR, Near[1] + DG, Near[2] + DB };
						if (L[0] < 0 || L[1] < 0 || L[2] < 0 || L[0] > Top[0] || L[1] > Top[1] || L[2] > Top[2] || !Keeps(L))
						{
							continue;
						}
						const double Off = (L[0] - Aim[0]) * (L[0] - Aim[0]) + (L[1] - Aim[1]) * (L[1] - Aim[1]) + (L[2] - Aim[2]) * (L[2] - Aim[2]);
						if (Off < Least)
						{
							Least = Off;
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

	Linear Held(Linear C, const Linear& Paint, const Linear& Salted, bool bNoWarmer)
	{
		// (no channel above the paint's own, or where her salt lies, above that salt over the clean paint: a deposit, paler)
		C.R = std::min(C.R, std::max(Paint.R, Salted.R));
		C.G = std::min(C.G, std::max(Paint.G, Salted.G));
		C.B = std::min(C.B, std::max(Paint.B, Salted.B));
		if (bNoWarmer && C.R >= Paint.R && C.G >= Paint.G && C.B >= Paint.B)
		{
			// (lifted by the salt in every channel, it reads as salt: red less blue, in levels, no higher than her salt makes it
			// on the clean paint, so the rust under the salt never warms it)
			const float Room = (ToSrgb(Salted.R) - ToSrgb(Salted.B)) * 255.0f;
			const float Red = ToSrgb(C.B) * 255.0f + Room;
			if (ToSrgb(C.R) * 255.0f > Red)
			{
				C.R = std::min(C.R, ToLinear(Red / 255.0f));
			}
		}
		// (and below the paint in any channel, above it in none: the salt pales a stain, it never warms it, so red never rises
		// while blue falls, the pink she ruled out)
		if (C.R < Paint.R || C.G < Paint.G || C.B < Paint.B)
		{
			C = NoneAbove(C, Paint);
		}
		return C;
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
					Marks[K] = 1;
					break;
				}
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
			if (O.bRustRamp && bStained && P.Wear < RunBodyWear && !P.bBodyAbove)
			{
				// (light wear away from a run is not rust, her rule 2: the point as it would be unworn, her salt and the soot
				// laid, a little darker and greyer, in whole levels inside her bounds; held as every stain is, which it keeps)
				Point Bare = P;
				Bare.Wear = 0;
				const Linear Worn = LightWear(P, Coated(Bare, Pal, O));
				return Held(Worn, P.Paint, P.Salt > 0 ? SaltedClean(P) : P.Paint, false);
			}
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
			if (P.Salt > 0 && P.bPainted)
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
				// pale the stain, in every channel at once, no further than the salt takes the clean paint and no warmer than it
				// makes it, never red up with blue down (Held). A point the skin leaves unworn: its paint, or its salt over the
				// clean paint, at most; the deck's hold only lowers.)
				const Linear Salted = P.Salt > 0 && P.bPainted ? SaltedClean(P) : P.Paint;
				if (bStained)
				{
					Coat = Held(Coat, P.Paint, Salted, P.Salt > 0 && P.bPainted);
				}
				else
				{
					Coat.R = std::min(Coat.R, std::max(P.Paint.R, Salted.R));
					Coat.G = std::min(Coat.G, std::max(P.Paint.G, Salted.G));
					Coat.B = std::min(Coat.B, std::max(P.Paint.B, Salted.B));
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
