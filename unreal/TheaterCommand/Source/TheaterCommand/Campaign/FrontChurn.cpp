#include "Campaign/FrontChurn.h"

#include "Campaign/SeasonTurn.h"

#include <algorithm>
#include <cmath>

namespace TheaterChurn
{
	namespace
	{
		double Clamp01(double V) { return std::clamp(V, 0.0, 1.0); }
		double Lerp(double A, double B, double T) { return A + (B - A) * T; }
		double Smooth(double Edge0, double Edge1, double V)
		{
			const double T = Clamp01((V - Edge0) / (Edge1 - Edge0));
			return T * T * (3.0 - 2.0 * T);
		}
		double Noise(double X, double Y, uint32_t Seed) { return TheaterSeasonTurn::Noise(X, Y, Seed); }

		/** Evenly between the points of a table (X rising), held at its ends. */
		double Table(const double (*Points)[2], int Count, double X)
		{
			if (X <= Points[0][0])
			{
				return Points[0][1];
			}
			for (int K = 1; K < Count; ++K)
			{
				if (X <= Points[K][0])
				{
					const double T = (X - Points[K - 1][0]) / (Points[K][0] - Points[K - 1][0]);
					return Lerp(Points[K - 1][1], Points[K][1], T);
				}
			}
			return Points[Count - 1][1];
		}

		// Colour as light, and back (the sRGB curve).
		double ToLight(double Eye)
		{
			const double C = Eye / 255.0;
			return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
		}
		double ToEye(double Light)
		{
			const double C = std::max(0.0, Light);
			return 255.0 * (C <= 0.0031308 ? C * 12.92 : 1.055 * std::pow(C, 1.0 / 2.4) - 0.055);
		}
		double Luminance(const double Light[3]) { return 0.2126 * Light[0] + 0.7152 * Light[1] + 0.0722 * Light[2]; }
		/** CIE lightness from luminance, 0 to 100. */
		double Lightness(double Y) { return Y > 0.008856 ? 116.0 * std::cbrt(Y) - 16.0 : 903.3 * Y; }
		double FromLightness(double L) { return L > 8.0 ? std::pow((L + 16.0) / 116.0, 3.0) : L / 903.3; }

		uint8_t Byte(double V) { return static_cast<uint8_t>(std::lround(std::clamp(V, 0.0, 255.0))); }

		uint32_t Hash(int64_t A, int64_t B, uint32_t C)
		{
			uint64_t H = static_cast<uint64_t>(A) * 0x9e3779b97f4a7c15ull ^ static_cast<uint64_t>(B) * 0xc2b2ae3d27d4eb4full ^ static_cast<uint64_t>(C) * 0x165667b19e3779f9ull;
			H ^= H >> 31;
			H *= 0xbf58476d1ce4e5b9ull;
			H ^= H >> 29;
			return static_cast<uint32_t>(H >> 16);
		}
		double Unit(uint32_t H) { return static_cast<double>(H & 0xffffffu) / 16777215.0; }

		/** The band's own earth on this ground in this season, as light. */
		void BodyColour(EGround Ground, ESeason Season, double CoreShare, double Light[3])
		{
			for (int C = 0; C < 3; ++C)
			{
				double Eye = Churned[C];
				if (Ground == EGround::Desert)
				{
					Eye = DesertChurned[C];
				}
				else if (Season == ESeason::Mud)
				{
					Eye = Lerp(MudChurned[C], MudWettest[C], CoreShare);
				}
				else if (Ground == EGround::BlackEarth)
				{
					Eye = BlackEarthChurned[C];
				}
				Light[C] = ToLight(Eye);
			}
		}
	}

	// ---- the months ----------------------------------------------------------------------------------------------

	double Grown(double Months)
	{
		static const double Points[][2] = { { 1.0, 0.0 }, { 2.0, 0.3 }, { 3.0, 0.5 }, { 4.0, 0.7 }, { 5.0, 0.85 }, { 6.0, 1.0 } };
		return Table(Points, 6, Months);
	}

	double Left(double MonthsSince)
	{
		static const double Points[][2] = { { 0.0, 1.0 }, { 4.0, 0.5 }, { 8.0, 0.25 }, { 12.0, 0.0 } };
		return Table(Points, 4, MonthsSince);
	}

	double CratersLeft(double MonthsSince) { return std::pow(Clamp01(1.0 - MonthsSince / 6.0), 1.5); }

	FState Growing(double Months)
	{
		const float S = static_cast<float>(Grown(Months));
		FState State;
		State.Size = S;
		State.Dark = S;
		State.Pits = S;
		return State;
	}

	FState Healing(const FState& AtLeaving, double MonthsSince)
	{
		const double Share = Left(MonthsSince);
		FState State;
		State.Size = static_cast<float>(AtLeaving.Size * Share);
		State.Dark = static_cast<float>(AtLeaving.Dark * Share);
		State.Pits = static_cast<float>(AtLeaving.Pits * CratersLeft(MonthsSince));
		State.Pale = static_cast<float>(std::max<double>(AtLeaving.Pale, 1.0 - Share));
		return State;
	}

	FState Mix(const FState& From, const FState& To, double T)
	{
		const double U = Clamp01(T);
		FState State;
		State.Size = static_cast<float>(Lerp(From.Size, To.Size, U));
		State.Dark = static_cast<float>(Lerp(From.Dark, To.Dark, U));
		State.Pits = static_cast<float>(Lerp(From.Pits, To.Pits, U));
		State.Pale = static_cast<float>(Lerp(From.Pale, To.Pale, U));
		return State;
	}

	// ---- the band ---------------------------------------------------------------------------------------------------

	FBand Band(const FBandIn& In)
	{
		FBand Out;
		// Strongest where the armies face each other, tapering to nothing along the rest of the frontier.
		// (along a dug-in pair's own border it thins to two fifths toward its far ends, as the night's fires thin to
		// embers there; beyond the pairs that are dug in it comes to nothing, by their states and the lines' ends)
		const double Taper = 0.4 + 0.6 * (1.0 - Smooth(Reach, Gone, In.Facing));
		const double Strength = static_cast<double>(In.State.Size) * Taper;
		if (Strength < 0.004 || In.D > Touch)
		{
			return Out;
		}
		Out.Strength = static_cast<float>(Strength);
		// The core: a metre wide at first show (a strength of 0.3), 2.4 when whole; narrowing from its edges as it heals.
		const double Grow = Clamp01((Strength - 0.3) / 0.7);
		double Wide = Strength < 0.3 ? CoreFirst * Strength / 0.3 : Lerp(CoreFirst, CoreWhole, Grow);
		const double Edge = Strength < 0.3 ? EdgeFirst * (0.5 + 0.5 * Strength / 0.3) : Lerp(EdgeFirst, EdgeWhole, Grow);
		// It wanders by about two fifths over stretches of four to eight units, and bulges a little where the armies stand.
		const double Wander = 0.62 * Noise(In.X / 6.0, In.Y / 6.0, 71) + 0.38 * Noise(In.X / 3.4, In.Y / 3.4, 72);
		Wide *= 1.0 + 0.55 * (Wander - 0.5);
		Wide *= 0.9 + 0.2 * (1.0 - Smooth(0.0, Reach, In.Facing));
		double Half = 0.5 * Wide;
		Half = std::min(Half, 0.5 * Widest - Edge);
		Out.HalfCore = static_cast<float>(Half);
		const double D = std::fabs(In.D);
		Out.Body = static_cast<float>(1.0 - Smooth(Half - 0.1 * Edge, Half + Edge, D));
		Out.Core = static_cast<float>(1.0 - Smooth(0.7 * Half, Half, D));
		// The ragged edge, for the fine veil: tongues a unit or two long, so that no stretch of edge runs straight.
		const double Tongue = 0.65 * Noise(In.X / 1.4, In.Y / 1.4, 73) + 0.35 * Noise(In.X / 0.6, In.Y / 0.6, 79);
		const double Ragged = D + (0.3 + 0.5 * Edge) * (2.0 * Tongue - 1.0) * 1.1;
		Out.Ragged = static_cast<float>(1.0 - Smooth(Half - 0.15, Half + 0.25 + 0.4 * Edge, Ragged));
		return Out;
	}

	// ---- the ground picture -----------------------------------------------------------------------------------------

	namespace
	{
		double DarkOf(const FBand& Band, const FState& State)
		{
			// (the darkness tapers with the strength: State.Dark against State.Size, where the armies are)
			const double Taper = State.Size > 0.001f ? Band.Strength / State.Size : 0.0;
			return Clamp01(static_cast<double>(State.Dark) * Taper);
		}
	}

	void PaintCell(const uint8_t In[3], const FBand& Band, const FState& State, EGround Ground, ESeason Season, double X, double Y, uint8_t Out[3])
	{
		const double Dark = DarkOf(Band, State);
		const double Lay = 0.78 * Band.Body * Dark;
		if (Lay < 0.002)
		{
			Out[0] = In[0];
			Out[1] = In[1];
			Out[2] = In[2];
			return;
		}
		double Ground3[3], Body[3], Spoil3[3];
		for (int C = 0; C < 3; ++C)
		{
			Ground3[C] = ToLight(In[C]);
			Spoil3[C] = ToLight(Spoil[C]);
		}
		BodyColour(Ground, Season, Band.Core, Body);
		// Healing: the last of it is a paler, rougher strip.
		const double Pale = 0.55 * State.Pale;
		// A few flecks of dry spoil along its edges.
		const double EdgeZone = Clamp01(4.0 * Band.Body * (1.0 - Band.Core));
		const double Fleck = Season == ESeason::Snow ? 0.0 : Smooth(0.72, 0.82, Noise(X / 0.9, Y / 0.9, 67)) * EdgeZone;
		double Mixed[3];
		for (int C = 0; C < 3; ++C)
		{
			const double B = Lerp(Lerp(Body[C], Spoil3[C], Pale), Spoil3[C], 0.6 * Fleck);
			Mixed[C] = Lerp(Ground3[C], B, Lay);
		}
		// Darker by a quarter of the ground's lightness at most, the plots still showing through as darker versions of
		// themselves; and mottled, never one smooth brown (the mottling kept inside the quarter).
		const double LIn = Lightness(Luminance(Ground3));
		auto SetLightness = [&](double Want)
		{
			const double Lum = Luminance(Mixed);
			if (Lum > 1.0e-6)
			{
				const double Scale = FromLightness(Want) / Lum;
				for (double& V : Mixed)
				{
					V *= Scale;
				}
			}
		};
		const double Mottle = 0.7 * Noise(X / 1.2, Y / 1.2, 61) + 0.3 * Noise(X / 3.0, Y / 3.0, 62);
		const double Room = 0.045 * Band.Body * Dark;
		double L = std::max(Lightness(Luminance(Mixed)), (1.0 - PictureDarker + Room) * LIn);
		L *= 1.0 + Room * (2.0 * Mottle - 1.0) * 2.0;
		SetLightness(std::max(L, (1.0 - PictureDarker) * LIn));
		for (int C = 0; C < 3; ++C)
		{
			Out[C] = Byte(ToEye(Mixed[C]));
		}
	}

	uint8_t Wooded(uint8_t In, const FBand& Band, const FState& State)
	{
		return Byte(static_cast<double>(In) * (1.0 - 0.75 * Band.Core * DarkOf(Band, State)));
	}

	uint8_t Snow(uint8_t In, const FBand& Band, const FState& State)
	{
		return Byte(static_cast<double>(In) * (1.0 - 0.62 * Band.Core * DarkOf(Band, State)));
	}

	// ---- the fine veil ----------------------------------------------------------------------------------------------

	float Veil(const FBand& Band, const FState& State, double X, double Y)
	{
		const double Dark = DarkOf(Band, State);
		if (Band.Ragged * Dark < 0.001)
		{
			return 0.0f;
		}
		const double Mottle = 0.65 * Noise(X / 0.8, Y / 0.8, 89) + 0.35 * Noise(X / 0.3, Y / 0.3, 97);
		return static_cast<float>(Band.Ragged * Dark * (VeilMost * 0.45 + VeilMost * 0.55 * Mottle));
	}

	void PitColours(EGround Kind, ESeason Season, const FPit& Hole, uint8_t Rim[3], uint8_t Bottom[3])
	{
		// The rim: the band's earth lifted a little toward the spoil, the colour of its lit side; the pit: the scars'
		// own pit, never black. In the mud months a few pits hold a dull sky pool. A crater's own few levels of light
		// and dark come from its place.
		const double Lift = 0.32 + 0.12 * Unit(Hash(static_cast<int64_t>(std::llround(Hole.X * 97.0)), static_cast<int64_t>(std::llround(Hole.Y * 97.0)), 5));
		const bool bPool = Season == ESeason::Mud && Kind != EGround::Desert && Unit(Hash(static_cast<int64_t>(std::llround(Hole.X * 61.0)), static_cast<int64_t>(std::llround(Hole.Y * 61.0)), 9)) < 0.18;
		for (int C = 0; C < 3; ++C)
		{
			const double Earth = Kind == EGround::Desert ? DesertChurned[C] : Season == ESeason::Mud ? MudChurned[C] : Kind == EGround::BlackEarth ? BlackEarthChurned[C] : Churned[C];
			Rim[C] = Byte(Lerp(Earth, Spoil[C], Lift));
			Bottom[C] = Byte(bPool ? SkyPool[C] : Kind == EGround::Desert ? DesertPit[C] : Season == ESeason::Mud ? Pit[C] * 1.06 : Pit[C]);
		}
	}

	// ---- the lines ----------------------------------------------------------------------------------------------------

	void Pits(const FLine& Line, std::vector<FPit>& Out)
	{
		const int N = static_cast<int>(Line.X.size());
		if (N < 2 || Line.Y.size() != Line.X.size() || Line.Facing.size() != Line.X.size() || Line.State.size() != Line.X.size())
		{
			return;
		}
		const double Step = 0.35;
		// (at full strength about a third of the core is pitted, all of it in clusters that cover a little under half the line)
		const double PerStep = 6.0;
		const int Stretches = Line.bClosed ? N : N - 1;
		double Carry = 0.0;
		for (int K = 0; K < Stretches; ++K)
		{
			const int J = (K + 1) % N;
			const double AX = Line.X[static_cast<size_t>(K)], AY = Line.Y[static_cast<size_t>(K)];
			const double BX = Line.X[static_cast<size_t>(J)], BY = Line.Y[static_cast<size_t>(J)];
			const double Long = std::hypot(BX - AX, BY - AY);
			if (Long < 1.0e-6)
			{
				continue;
			}
			const double TX = (BX - AX) / Long, TY = (BY - AY) / Long, NXd = -TY, NYd = TX;
			for (double S = Carry; S < Long; S += Step)
			{
				const double T = S / Long;
				FBandIn In;
				In.X = AX + TX * S;
				In.Y = AY + TY * S;
				In.D = 0.0;
				In.Facing = Lerp(Line.Facing[static_cast<size_t>(K)], Line.Facing[static_cast<size_t>(J)], T);
				In.State = Mix(Line.State[static_cast<size_t>(K)], Line.State[static_cast<size_t>(J)], T);
				const FBand Here = Band(In);
				const double Pitted = Here.Strength > 0.0f && In.State.Size > 0.001f ? static_cast<double>(In.State.Pits) * Here.Strength / In.State.Size : 0.0;
				if (Pitted < 0.02 || Here.HalfCore < 0.12f)
				{
					continue;
				}
				// In clusters, with bare stretches between.
				const double Cluster = Smooth(0.40, 0.56, 0.6 * Noise(In.X / 2.2, In.Y / 2.2, 83) + 0.4 * Noise(In.X / 0.9, In.Y / 0.9, 84));
				if (Cluster <= 0.0)
				{
					continue;
				}
				const int64_t QX = static_cast<int64_t>(std::llround(In.X * 64.0)), QY = static_cast<int64_t>(std::llround(In.Y * 64.0));
				const double Many = PerStep * Pitted * Cluster * (2.0 * Here.HalfCore / CoreWhole);
				const int Count = static_cast<int>(Many) + (Unit(Hash(QX, QY, 1)) < Many - std::floor(Many) ? 1 : 0);
				for (int P = 0; P < Count; ++P)
				{
					const uint32_t Seed = static_cast<uint32_t>(P) * 7u + 11u;
					const double U = Unit(Hash(QX, QY, Seed));
					FPit Hole;
					Hole.Radius = 0.075 + 0.175 * U * U; // (0.15 to 0.5 across, most of them small)
					const double Room = std::max(0.0, static_cast<double>(Here.HalfCore) - Hole.Radius);
					const double Across = (2.0 * Unit(Hash(QX, QY, Seed + 1u)) - 1.0) * std::min(Room, 0.85 * Here.HalfCore);
					const double Along = (Unit(Hash(QX, QY, Seed + 2u)) - 0.5) * Step;
					Hole.X = In.X + TX * Along + NXd * Across;
					Hole.Y = In.Y + TY * Along + NYd * Across;
					Hole.Dark = static_cast<float>(Clamp01(0.55 + 0.45 * Pitted) * (0.8 + 0.2 * Unit(Hash(QX, QY, Seed + 3u))));
					Out.push_back(Hole);
				}
			}
			// (the next stretch carries on the same steps)
			if (Long > Carry)
			{
				const double Taken = std::ceil((Long - Carry) / Step);
				Carry = Carry + Taken * Step - Long;
			}
			else
			{
				Carry -= Long;
			}
		}
	}

	void EaseEnds(FLine& Line, double Over)
	{
		const size_t N = Line.X.size();
		if (Line.bClosed || N < 2 || Line.State.size() != N || Over <= 0.0)
		{
			return;
		}
		std::vector<double> Run(N, 0.0);
		for (size_t K = 1; K < N; ++K)
		{
			Run[K] = Run[K - 1] + std::hypot(Line.X[K] - Line.X[K - 1], Line.Y[K] - Line.Y[K - 1]);
		}
		const double Whole = Run[N - 1];
		for (size_t K = 0; K < N; ++K)
		{
			const double Ease = Smooth(0.0, Over, std::min(Run[K], Whole - Run[K]));
			FState& State = Line.State[K];
			State.Size = static_cast<float>(State.Size * Ease);
			State.Dark = static_cast<float>(State.Dark * Ease);
			State.Pits = static_cast<float>(State.Pits * Ease);
		}
	}

	void Measure(const std::vector<FLine>& Lines, double OriginX, double OriginY, double Cell, int NX, int NY, FNearest& Out)
	{
		const size_t Count = static_cast<size_t>(std::max(0, NX)) * static_cast<size_t>(std::max(0, NY));
		if (Out.Dist.size() != Count)
		{
			Out.Dist.assign(Count, 1.0e9f);
			Out.Line.assign(Count, -1);
			Out.Point.assign(Count, -1);
			Out.At.assign(Count, 0.0f);
		}
		for (size_t L = 0; L < Lines.size(); ++L)
		{
			const FLine& Line = Lines[L];
			const int N = static_cast<int>(Line.X.size());
			const int Stretches = N < 2 ? 0 : Line.bClosed ? N : N - 1;
			for (int K = 0; K < Stretches; ++K)
			{
				const int J = (K + 1) % N;
				const double AX = Line.X[static_cast<size_t>(K)], AY = Line.Y[static_cast<size_t>(K)];
				const double BX = Line.X[static_cast<size_t>(J)], BY = Line.Y[static_cast<size_t>(J)];
				const double DX = BX - AX, DY = BY - AY, Long2 = DX * DX + DY * DY;
				const int I0 = std::max(0, static_cast<int>(std::floor((std::min(AX, BX) - Touch - OriginX) / Cell)));
				const int I1 = std::min(NX - 1, static_cast<int>(std::ceil((std::max(AX, BX) + Touch - OriginX) / Cell)));
				const int J0 = std::max(0, static_cast<int>(std::floor((std::min(AY, BY) - Touch - OriginY) / Cell)));
				const int J1 = std::min(NY - 1, static_cast<int>(std::ceil((std::max(AY, BY) + Touch - OriginY) / Cell)));
				for (int GJ = J0; GJ <= J1; ++GJ)
				{
					for (int GI = I0; GI <= I1; ++GI)
					{
						const double PX = OriginX + GI * Cell, PY = OriginY + GJ * Cell;
						const double T = Long2 > 1.0e-12 ? Clamp01(((PX - AX) * DX + (PY - AY) * DY) / Long2) : 0.0;
						const double Dist = std::hypot(PX - (AX + DX * T), PY - (AY + DY * T));
						const size_t Idx = static_cast<size_t>(GJ) * static_cast<size_t>(NX) + static_cast<size_t>(GI);
						if (Dist <= Touch && Dist < static_cast<double>(Out.Dist[Idx]))
						{
							Out.Dist[Idx] = static_cast<float>(Dist);
							Out.Line[Idx] = static_cast<int>(L);
							Out.Point[Idx] = K;
							Out.At[Idx] = static_cast<float>(T);
						}
					}
				}
			}
		}
	}

	bool At(const std::vector<FLine>& Lines, const FNearest& Nearest, int Index, double X, double Y, FBandIn& In)
	{
		if (Index < 0 || static_cast<size_t>(Index) >= Nearest.Line.size())
		{
			return false;
		}
		const int L = Nearest.Line[static_cast<size_t>(Index)];
		if (L < 0 || static_cast<size_t>(L) >= Lines.size())
		{
			return false;
		}
		const FLine& Line = Lines[static_cast<size_t>(L)];
		const int N = static_cast<int>(Line.X.size());
		const int K = Nearest.Point[static_cast<size_t>(Index)];
		if (K < 0 || K >= N)
		{
			return false;
		}
		const int J = (K + 1) % N;
		const double T = Nearest.At[static_cast<size_t>(Index)];
		In.X = X;
		In.Y = Y;
		In.D = Nearest.Dist[static_cast<size_t>(Index)];
		In.Facing = Lerp(Line.Facing[static_cast<size_t>(K)], Line.Facing[static_cast<size_t>(J)], T);
		In.State = Mix(Line.State[static_cast<size_t>(K)], Line.State[static_cast<size_t>(J)], T);
		return true;
	}
}
