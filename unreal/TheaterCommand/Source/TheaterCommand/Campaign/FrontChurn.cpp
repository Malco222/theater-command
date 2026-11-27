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

	namespace
	{
		/** The taper away from where the armies face: whole within Reach, half at Gone and beyond (where a weaker band shows a few gaps, never a thread). */
		double Taper(double Facing) { return 0.5 + 0.5 * (1.0 - Smooth(Reach, Gone, Facing)); }

		/** Where the craters cluster (and where a weak band keeps its patches, and a healing strip its islands). */
		double ClusterField(double X, double Y) { return 0.6 * Noise(X / 2.2, Y / 2.2, 83) + 0.4 * Noise(X / 0.9, Y / 0.9, 84); }
	}

	FBand Band(const FBandIn& In)
	{
		FBand Out;
		const double Strength = std::min(1.0, In.Strength >= 0.0 ? In.Strength : static_cast<double>(In.State.Size) * Taper(In.Facing));
		if (Strength < 0.004 || In.D > Touch)
		{
			return Out;
		}
		Out.Strength = static_cast<float>(Strength);
		// The core: 1.0 wide at first show, 2.4 when whole; weaker than at first show it keeps its width and breaks up
		// instead (below).
		const double Grow = Clamp01((Strength - 0.3) / 0.7);
		double Wide = Lerp(CoreFirst, CoreWhole, Grow);
		const double Edge = Lerp(EdgeFirst, EdgeWhole, Grow);
		// It wanders by about two fifths over stretches of four to eight units, and bulges a little where the armies stand.
		const double FX = In.bFoot ? In.FootX : In.X, FY = In.bFoot ? In.FootY : In.Y;
		const double Wander = 0.62 * Noise(FX / 6.0, FY / 6.0, 71) + 0.38 * Noise(FX / 3.4, FY / 3.4, 72);
		Wide *= 1.0 + 0.55 * (Wander - 0.5);
		Wide *= 0.9 + 0.2 * (1.0 - Smooth(0.0, Reach, In.Facing));
		// (never over Widest in all, never under Narrowest where its body is half laid)
		const double Half = std::clamp(0.5 * Wide, 0.5 * Narrowest - 0.45 * Edge, 0.5 * Widest - Edge);
		Out.HalfCore = static_cast<float>(Half);
		// Its middle strays off the frontier by up to half a unit, over stretches of four to eight units: never a ruler
		// laid on the border.
		const double Middle = Stray * std::clamp(2.6 * (Noise(FX / 5.5, FY / 5.5, 74) - 0.5), -1.0, 1.0);
		Out.Middle = static_cast<float>(Middle);
		const double Off = (In.bSided ? In.Side : In.D) - Middle;
		const double D = std::fabs(Off);
		// Weak is broken, never thin: patches two to five units long with gaps between, more of them the stronger it is,
		// where the craters cluster (so that a healing strip keeps islands where its craters were).
		const double Field = ClusterField(In.X, In.Y);
		const double Cover = Smooth(0.03, 0.62, Strength);
		const double Bar = 0.80 - 0.62 * Cover;
		const double Patch = std::max(Smooth(Bar - 0.05, Bar + 0.05, Field), Smooth(0.85, 1.0, Cover));
		Out.Whole = static_cast<float>(Patch);
		Out.Cluster = static_cast<float>(Smooth(0.40, 0.56, Field));
		Out.Body = static_cast<float>(Patch * (1.0 - Smooth(Half - 0.1 * Edge, Half + Edge, D)));
		Out.Core = static_cast<float>(Patch * (1.0 - Smooth(0.7 * Half, Half, D)));
		// The ragged edge, for the fine veil: crisp, with uneven tongues reaching out 0.3 to 0.8 of a unit and a few
		// bites in, a different run of them on each side.
		const uint32_t SideSeed = Off >= 0.0 ? 73u : 77u;
		const double Tongue = 0.85 * Noise(In.Along / 0.6, 0.37, SideSeed) + 0.15 * Noise(In.Along / 0.3, 0.71, SideSeed + 6u);
		const double Reach2 = 0.9 * TongueLongest * Smooth(0.46, 0.84, Tongue) - 0.1 * (1.0 - Smooth(0.14, 0.42, Tongue));
		const double Wobble = 0.09 * (2.0 * Noise(In.Along / 1.3, 0.53, SideSeed + 11u) - 1.0);
		const double Rim = Half + 0.35 * Edge + Reach2 * (0.55 + 0.45 * Grow) + Wobble;
		Out.Ragged = static_cast<float>(Patch * (1.0 - Smooth(Rim - 0.07, Rim + 0.07, D)));
		return Out;
	}

	double Shown(double BandPixels) { return Smooth(3.0, 5.0, BandPixels); }

	double CraterShown(double Pixels) { return Smooth(3.0, 6.0, Pixels); }

	// ---- the ground picture -----------------------------------------------------------------------------------------

	namespace
	{
		double DarkOf(const FBand& Band, const FState& State)
		{
			// (the darkness tapers with the strength, State.Dark against State.Size where the armies are; but gently, so
			// that a weak band's patches still read as broken earth, not as a faint pencil line)
			const double Share = State.Size > 0.001f ? Clamp01(Band.Strength / State.Size) : 0.0;
			return std::sqrt(Clamp01(static_cast<double>(State.Dark) * Share));
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
		// A grain of 0.2 to 0.6 of a unit, and the clusters of craters a little darker (from high up, where the
		// craters themselves are too small to draw, they show as this mottle and never as dots).
		const double Grain = 0.55 * Noise(X / 0.55, Y / 0.55, 89) + 0.45 * Noise(X / 0.22, Y / 0.22, 97);
		const double Pitted = State.Size > 0.001f ? Clamp01(static_cast<double>(State.Pits) / State.Size) : 0.0;
		const double Share = std::clamp(0.52 + 1.7 * (Grain - 0.5) + 0.25 * Band.Cluster * Pitted - 0.1, 0.0, 1.0);
		return static_cast<float>(Band.Ragged * Dark * VeilMost * Share);
	}

	void PitColours(EGround Kind, ESeason Season, const FPit& Hole, uint8_t Rim[3], uint8_t Bottom[3], uint8_t Shade[3])
	{
		const int64_t QX = static_cast<int64_t>(std::llround(Hole.X * 97.0)), QY = static_cast<int64_t>(std::llround(Hole.Y * 97.0));
		// (each its own few levels of light and dark)
		const double Lift = 0.94 + 0.1 * Unit(Hash(QX, QY, 5));
		if (Hole.bSpoil)
		{
			for (int C = 0; C < 3; ++C)
			{
				const double Fleck = Season == ESeason::Snow ? SnowDirty[C] : Kind == EGround::Desert ? Lerp(Spoil[C], 230.0, 0.3) : Spoil[C];
				Rim[C] = Bottom[C] = Shade[C] = Byte(Fleck * Lift);
			}
			return;
		}
		const bool bLarge = Hole.Radius >= 0.15;
		const bool bPool = bLarge && Season == ESeason::Mud && Kind != EGround::Desert && Unit(Hash(QX, QY, 9)) < 1.0 / 6.0;
		for (int C = 0; C < 3; ++C)
		{
			double R = RimLit[C], B = PitSmall[C], S = Pit[C];
			if (Kind == EGround::Desert)
			{
				R = Lerp(DesertChurned[C], 240.0, 0.25);
				B = DesertPit[C];
				S = DesertPit[C] * 0.72;
			}
			else if (Season == ESeason::Snow)
			{
				R = SnowDirty[C];
			}
			else if (Season == ESeason::Mud)
			{
				R *= 0.82;
				B *= 0.88;
			}
			else if (Kind == EGround::BlackEarth)
			{
				R = Lerp(R, BlackEarthChurned[C], 0.35);
				B *= 0.8;
				S *= 0.85;
			}
			if (bPool)
			{
				B = SkyPool[C];
				S = SkyPool[C] * 0.86;
			}
			Rim[C] = Byte(R * Lift);
			Bottom[C] = Byte(B * Lift);
			Shade[C] = Byte(bLarge ? S * Lift : B * Lift);
		}
	}

	// ---- the lines ----------------------------------------------------------------------------------------------------

	namespace
	{
		/** The running length of a line at each point (a closed line's last stretch back to its first is not counted in). */
		void RunOf(const FLine& Line, std::vector<double>& Run)
		{
			const size_t N = Line.X.size();
			Run.assign(N, 0.0);
			for (size_t K = 1; K < N; ++K)
			{
				Run[K] = Run[K - 1] + std::hypot(Line.X[K] - Line.X[K - 1], Line.Y[K] - Line.Y[K - 1]);
			}
		}

		/** A line's strength at a point (its own, once strengthened; else from its state and facing). */
		double StrengthAt(const FLine& Line, size_t K)
		{
			if (Line.Strength.size() == Line.X.size())
			{
				return Line.Strength[K];
			}
			return static_cast<double>(Line.State[K].Size) * Taper(Line.Facing[K]);
		}
	}

	void Strengthen(FLine& Line, double Over)
	{
		const size_t N = Line.X.size();
		if (N == 0 || Line.Y.size() != N || Line.Facing.size() != N || Line.State.size() != N)
		{
			return;
		}
		RunOf(Line, Line.Run);
		const double Whole = Line.Run[N - 1] + (Line.bClosed ? std::hypot(Line.X[0] - Line.X[N - 1], Line.Y[0] - Line.Y[N - 1]) : 0.0);
		std::vector<double> Own(N);
		for (size_t K = 0; K < N; ++K)
		{
			Own[K] = static_cast<double>(Line.State[K].Size) * Taper(Line.Facing[K]);
		}
		// (the points within half of Over either way along the line, round a closed one's join)
		auto Gap = [&](size_t A, size_t B)
		{
			const double Apart = std::fabs(Line.Run[A] - Line.Run[B]);
			return Line.bClosed ? std::min(Apart, Whole - Apart) : Apart;
		};
		auto Near = [&](size_t K, auto&& Each)
		{
			Each(K);
			const size_t Most = Line.bClosed ? N / 2 : N;
			for (size_t Step = 1; Step <= Most; ++Step)
			{
				bool bAny = false;
				const bool bUp = Line.bClosed || K + Step < N, bDown = Line.bClosed || Step <= K;
				const size_t Up = (K + Step) % N, Down = (K + N - Step % N) % N;
				if (bUp && Gap(K, Up) <= 0.5 * Over + 1.0e-9)
				{
					Each(Up);
					bAny = true;
				}
				if (bDown && Down != Up && Gap(K, Down) <= 0.5 * Over + 1.0e-9)
				{
					Each(Down);
					bAny = true;
				}
				if (!bAny)
				{
					break;
				}
			}
		};
		// Where two stretches meet they join at the stronger (no notch), and the join is blended over about Over.
		std::vector<double> Most(N, 0.0);
		for (size_t K = 0; K < N; ++K)
		{
			Near(K, [&](size_t J) { Most[K] = std::max(Most[K], Own[J]); });
		}
		std::vector<float> Strength(N);
		std::vector<FState> State(N);
		for (size_t K = 0; K < N; ++K)
		{
			double Sum = 0.0, Size = 0.0, Dark = 0.0, Pits = 0.0, Pale = 0.0;
			int Count = 0;
			Near(K, [&](size_t J)
			{
				Sum += Most[J];
				Size += Line.State[J].Size;
				Dark += Line.State[J].Dark;
				Pits += Line.State[J].Pits;
				Pale += Line.State[J].Pale;
				++Count;
			});
			Strength[K] = static_cast<float>(Sum / Count);
			State[K].Size = static_cast<float>(std::max<double>(Size / Count, Line.State[K].Size));
			State[K].Dark = static_cast<float>(std::max<double>(Dark / Count, Line.State[K].Dark));
			State[K].Pits = static_cast<float>(Pits / Count);
			State[K].Pale = static_cast<float>(Pale / Count);
		}
		Line.Strength = std::move(Strength);
		Line.State = std::move(State);
	}

	void Pits(const FLine& Line, std::vector<FPit>& Out)
	{
		const int N = static_cast<int>(Line.X.size());
		if (N < 2 || Line.Y.size() != Line.X.size() || Line.Facing.size() != Line.X.size() || Line.State.size() != Line.X.size())
		{
			return;
		}
		std::vector<double> Run = Line.Run;
		if (Run.size() != Line.X.size())
		{
			RunOf(Line, Run);
		}
		const double Step = 0.35;
		// (where whole and where the armies stand, the pit floors come to about 7% of the band: the art director's
		// ceiling, from the close drawing of 27 November; all of them in clusters that cover a little under half the line)
		const double PerStep = 6.0;
		const int Stretches = Line.bClosed ? N : N - 1;
		double Carry = 0.0;
		for (int K = 0; K < Stretches; ++K)
		{
			const int J = (K + 1) % N;
			const size_t UK = static_cast<size_t>(K), UJ = static_cast<size_t>(J);
			const double AX = Line.X[UK], AY = Line.Y[UK];
			const double BX = Line.X[UJ], BY = Line.Y[UJ];
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
				In.bSided = true;
				In.Side = 0.0;
				In.bFoot = true;
				In.FootX = In.X;
				In.FootY = In.Y;
				In.Along = Run[UK] + S;
				In.Facing = Lerp(Line.Facing[UK], Line.Facing[UJ], T);
				In.Strength = Lerp(StrengthAt(Line, UK), StrengthAt(Line, UJ), T);
				In.State = Mix(Line.State[UK], Line.State[UJ], T);
				const FBand Here = Band(In);
				const int64_t QX = static_cast<int64_t>(std::llround(In.X * 64.0)), QY = static_cast<int64_t>(std::llround(In.Y * 64.0));
				// A few flecks of dry spoil in the soft edge, either side, where the band is laid.
				const double Edge = Lerp(EdgeFirst, EdgeWhole, Clamp01((Here.Strength - 0.3) / 0.7));
				if (Here.Whole > 0.5f && Unit(Hash(QX, QY, 41)) < 0.16 * Here.Whole * std::min(1.0, Here.Strength / 0.5))
				{
					FPit Fleck;
					Fleck.bSpoil = true;
					Fleck.Radius = 0.04 + 0.05 * Unit(Hash(QX, QY, 42));
					const double Across = Here.Middle + (Unit(Hash(QX, QY, 43)) < 0.5 ? -1.0 : 1.0) * (Here.HalfCore + Edge * (0.1 + 0.6 * Unit(Hash(QX, QY, 44))));
					Fleck.X = In.X + NXd * Across + TX * (Unit(Hash(QX, QY, 45)) - 0.5) * Step;
					Fleck.Y = In.Y + NYd * Across + TY * (Unit(Hash(QX, QY, 45)) - 0.5) * Step;
					Fleck.Dark = static_cast<float>(0.7 + 0.3 * Here.Whole);
					Out.push_back(Fleck);
				}
				// The craters: fewer where it is weaker, none in its gaps.
				const double Pitted = Here.Strength > 0.0f && In.State.Size > 0.001f ? std::min(1.0, static_cast<double>(In.State.Pits) * Here.Strength / In.State.Size) * Here.Whole : 0.0;
				if (Pitted < 0.02 || Here.HalfCore < 0.12f || Here.Cluster <= 0.0f)
				{
					continue;
				}
				const double Many = PerStep * Pitted * Here.Cluster * (2.0 * Here.HalfCore / CoreWhole);
				const int Count = static_cast<int>(Many) + (Unit(Hash(QX, QY, 1)) < Many - std::floor(Many) ? 1 : 0);
				for (int P = 0; P < Count; ++P)
				{
					const uint32_t Seed = static_cast<uint32_t>(P) * 7u + 11u;
					const double U = Unit(Hash(QX, QY, Seed));
					FPit Hole;
					Hole.Radius = 0.075 + 0.175 * U * U; // (0.15 to 0.5 across, most of them small)
					const double Room = std::max(0.0, static_cast<double>(Here.HalfCore) - Hole.Radius);
					const double Across = Here.Middle + (2.0 * Unit(Hash(QX, QY, Seed + 1u)) - 1.0) * std::min(Room, 0.85 * Here.HalfCore);
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
		std::vector<double> Run;
		RunOf(Line, Run);
		const double Whole = Run[N - 1];
		for (size_t K = 0; K < N; ++K)
		{
			const double Ease = Smooth(0.0, Over, std::min(Run[K], Whole - Run[K]));
			FState& State = Line.State[K];
			State.Size = static_cast<float>(State.Size * Ease);
			State.Dark = static_cast<float>(State.Dark * Ease);
			State.Pits = static_cast<float>(State.Pits * Ease);
			if (Line.Strength.size() == N)
			{
				Line.Strength[K] = static_cast<float>(Line.Strength[K] * Ease);
			}
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
		const size_t UK = static_cast<size_t>(K), UJ = static_cast<size_t>(J);
		const double T = Nearest.At[static_cast<size_t>(Index)];
		In.X = X;
		In.Y = Y;
		In.D = Nearest.Dist[static_cast<size_t>(Index)];
		// (which side: to the left of the stretch's way is plus)
		const double DX = Line.X[UJ] - Line.X[UK], DY = Line.Y[UJ] - Line.Y[UK];
		In.bSided = true;
		In.Side = DX * (Y - Line.Y[UK]) - DY * (X - Line.X[UK]) >= 0.0 ? In.D : -In.D;
		In.bFoot = true;
		In.FootX = Line.X[UK] + DX * T;
		In.FootY = Line.Y[UK] + DY * T;
		In.Along = (Line.Run.size() == Line.X.size() ? Line.Run[UK] : 0.0) + T * std::hypot(DX, DY);
		In.Facing = Lerp(Line.Facing[UK], Line.Facing[UJ], T);
		In.Strength = Line.Strength.size() == Line.X.size() ? Lerp(Line.Strength[UK], Line.Strength[UJ], T) : -1.0;
		In.State = Mix(Line.State[UK], Line.State[UJ], T);
		return true;
	}
}
