#include "SeasonTurn.h"

#include <algorithm>
#include <array>
#include <cmath>

// (Plain C++ with no engine in it: see SeasonTurn.h. Built alone by Tools/season_turn/run.sh with the strictest
// warnings, and by the game with the rest of its code.)

namespace TheaterSeasonTurn
{
	namespace
	{
		double Clamp01(double V) { return V < 0.0 ? 0.0 : V > 1.0 ? 1.0 : V; }

		double Smooth(double From, double To, double V)
		{
			const double X = Clamp01((V - From) / (To - From));
			return X * X * (3.0 - 2.0 * X);
		}

		double Mix(double A, double B, double T) { return A + (B - A) * T; }

		/** A whole number's own scramble: the same in, the same out, and neighbours far apart. */
		uint32_t Hash(int32_t I, int32_t J, uint32_t Seed)
		{
			uint32_t H = static_cast<uint32_t>(I) * 0x8da6b343u ^ static_cast<uint32_t>(J) * 0xd8163841u ^ Seed * 0xcb1ab31fu;
			H ^= H >> 13;
			H *= 0x5bd1e995u;
			H ^= H >> 15;
			return H;
		}

		double Lattice(int32_t I, int32_t J, uint32_t Seed) { return static_cast<double>(Hash(I, J, Seed) & 0xffffu) / 65535.0; }

		/** From the eye's 0 to 255 to light's own measure (0 to 1), and back: colours are mixed in light, as paint on a lit table mixes. */
		double ToLight(double Eye) { return std::pow(Clamp01(Eye / 255.0), 2.2); }
		double ToEye(double Light) { return 255.0 * std::pow(Clamp01(Light), 1.0 / 2.2); }

		/** Light for each of the eye's 256 steps, worked out once. */
		const std::array<double, 256>& LightOf()
		{
			static const std::array<double, 256> Table = []
			{
				std::array<double, 256> Each{};
				for (size_t K = 0; K < Each.size(); ++K)
				{
					Each[K] = ToLight(static_cast<double>(K));
				}
				return Each;
			}();
			return Table;
		}

		/** The eye's step for light, looked up by the square root of the light (so that the dark has as many steps as the bright). */
		constexpr size_t EyeSteps = 2048;
		const std::array<uint8_t, EyeSteps>& EyeOf()
		{
			static const std::array<uint8_t, EyeSteps> Table = []
			{
				std::array<uint8_t, EyeSteps> Each{};
				for (size_t K = 0; K < Each.size(); ++K)
				{
					const double Root = static_cast<double>(K) / static_cast<double>(EyeSteps - 1);
					Each[K] = static_cast<uint8_t>(std::lround(std::min(255.0, ToEye(Root * Root))));
				}
				return Each;
			}();
			return Table;
		}
	}

	double Front(double Seconds)
	{
		if (!(Seconds > 0.0))
		{
			return 0.0;
		}
		if (Seconds < Spreads)
		{
			// (half even and half eased, so that it neither jerks off the mark nor crawls at the start)
			const double X = Seconds / Spreads;
			return (1.0 + FrontWidth) * 0.5 * (X + X * X * (3.0 - 2.0 * X));
		}
		return 1.0 + FrontWidth + BandWidth * Clamp01((Seconds - Spreads) / Settles);
	}

	FShown At(float Arrive, double FrontNow)
	{
		FShown Shown;
		const double Past = FrontNow - static_cast<double>(Arrive);
		Shown.Share = static_cast<float>(Smooth(0.0, FrontWidth, Past));
		const double X = Past / BandWidth;
		Shown.Band = X > 0.0 && X < 1.0 ? static_cast<float>(4.0 * X * (1.0 - X)) : 0.0f;
		return Shown;
	}

	double Noise(double X, double Y, uint32_t Seed)
	{
		const double FX = std::floor(X), FY = std::floor(Y);
		const int32_t I = static_cast<int32_t>(FX), J = static_cast<int32_t>(FY);
		double U = X - FX, V = Y - FY;
		U = U * U * U * (U * (U * 6.0 - 15.0) + 10.0);
		V = V * V * V * (V * (V * 6.0 - 15.0) + 10.0);
		const double A = Mix(Lattice(I, J, Seed), Lattice(I + 1, J, Seed), U);
		const double B = Mix(Lattice(I, J + 1, Seed), Lattice(I + 1, J + 1, Seed), U);
		return Mix(A, B, V);
	}

	void Spread(std::vector<float>& Field, int NX, int NY, float Far)
	{
		if (NX <= 0 || NY <= 0 || static_cast<long long>(Field.size()) != static_cast<long long>(NX) * NY)
		{
			return;
		}
		const float Side = 1.0f, Corner = 1.41421356f;
		auto At = [&](int I, int J) -> float& { return Field[static_cast<size_t>(J) * static_cast<size_t>(NX) + static_cast<size_t>(I)]; };
		for (float& Each : Field)
		{
			Each = std::min(Each, Far);
		}
		for (int J = 0; J < NY; ++J)
		{
			for (int I = 0; I < NX; ++I)
			{
				float D = At(I, J);
				if (I > 0) { D = std::min(D, At(I - 1, J) + Side); }
				if (J > 0)
				{
					D = std::min(D, At(I, J - 1) + Side);
					if (I > 0) { D = std::min(D, At(I - 1, J - 1) + Corner); }
					if (I + 1 < NX) { D = std::min(D, At(I + 1, J - 1) + Corner); }
				}
				At(I, J) = D;
			}
		}
		for (int J = NY - 1; J >= 0; --J)
		{
			for (int I = NX - 1; I >= 0; --I)
			{
				float D = At(I, J);
				if (I + 1 < NX) { D = std::min(D, At(I + 1, J) + Side); }
				if (J + 1 < NY)
				{
					D = std::min(D, At(I, J + 1) + Side);
					if (I + 1 < NX) { D = std::min(D, At(I + 1, J + 1) + Corner); }
					if (I > 0) { D = std::min(D, At(I - 1, J + 1) + Corner); }
				}
				At(I, J) = D;
			}
		}
	}

	void Blur(std::vector<float>& Field, int NX, int NY, int Radius)
	{
		if (Radius <= 0 || NX <= 0 || NY <= 0 || static_cast<long long>(Field.size()) != static_cast<long long>(NX) * NY)
		{
			return;
		}
		std::vector<float> Line;
		// One pass along a line of Count points Step apart from First: each point the mean of those within Radius (the ends held).
		auto Pass = [&](size_t First, size_t Step, int Count)
		{
			Line.resize(static_cast<size_t>(Count));
			for (int K = 0; K < Count; ++K)
			{
				Line[static_cast<size_t>(K)] = Field[First + static_cast<size_t>(K) * Step];
			}
			auto Get = [&](int K) { return static_cast<double>(Line[static_cast<size_t>(std::clamp(K, 0, Count - 1))]); };
			double Sum = 0.0;
			for (int K = -Radius; K <= Radius; ++K)
			{
				Sum += Get(K);
			}
			const double Width = 2.0 * Radius + 1.0;
			for (int K = 0; K < Count; ++K)
			{
				Field[First + static_cast<size_t>(K) * Step] = static_cast<float>(Sum / Width);
				Sum += Get(K + Radius + 1) - Get(K - Radius);
			}
		};
		for (int Twice = 0; Twice < 2; ++Twice)
		{
			for (int J = 0; J < NY; ++J)
			{
				Pass(static_cast<size_t>(J) * static_cast<size_t>(NX), 1, NX);
			}
			for (int I = 0; I < NX; ++I)
			{
				Pass(static_cast<size_t>(I), static_cast<size_t>(NX), NY);
			}
		}
	}

	void Even(std::vector<float>& Arrive)
	{
		if (Arrive.size() < 2)
		{
			return;
		}
		// (a count of them in 4096 steps of the turn, and each placed by those before it, and its share of its own step)
		constexpr size_t Steps = 4096;
		std::array<double, Steps + 1> Before{};
		std::array<double, Steps> Count{};
		auto StepOf = [](float A) { return std::min(Steps - 1, static_cast<size_t>(Clamp01(A) * static_cast<double>(Steps))); };
		for (const float A : Arrive)
		{
			Count[StepOf(A)] += 1.0;
		}
		for (size_t K = 0; K < Steps; ++K)
		{
			Before[K + 1] = Before[K] + Count[K];
		}
		const double All = static_cast<double>(Arrive.size());
		for (float& A : Arrive)
		{
			const size_t K = StepOf(A);
			const double Within = Clamp01(Clamp01(A) * static_cast<double>(Steps) - static_cast<double>(K));
			A = static_cast<float>(Clamp01((Before[K] + Within * Count[K]) / All));
		}
	}

	FMud Mud(const FMudIn& In)
	{
		FMud Out;
		if (!(In.Edge > EdgeStop))
		{
			return Out;
		}
		// How far it is in from the edge of its ground: near the edge the mud breaks into patches, the fewer the nearer,
		// and it stops a little inside, softly, so that no province's line and no cell of the ground is ever drawn.
		const double In01 = Clamp01((In.Edge - EdgeStop) / EdgeReach);
		const double Grain = Noise(In.X / 2.2, In.Y / 2.2, 11u);
		const double Need = 0.95 * (1.0 - In01) - 0.12;
		const double Patch = Smooth(Need - 0.07, Need + 0.07, Grain) * Smooth(0.0, 2.0, In.Edge - EdgeStop);
		// Wettest by the water and on the low, flat ground; drier on the hills and the broken country.
		// (flat at the water's edge and soft away from it, so that the first steps of the grid from a river show no ring)
		const double Near = std::max(In.Water, 0.0) / SoakNear;
		const double River = 1.0 / (1.0 + Near * Near);
		const double Low = (1.0 - Smooth(0.95, 1.75, In.Height)) * (1.0 - 0.6 * Smooth(0.4, 2.0, In.Rough));
		const double Broad = Noise(In.X / 14.0, In.Y / 14.0, 23u);
		// (never one smooth brown: the drier ground on the rises and in broad patches lies paler and thinner, the
		// ground's own colour showing through it)
		double Wet = Clamp01(0.42 + 0.4 * River + 0.2 * Low + 0.3 * (Broad - 0.5));
		Wet *= Patch * (1.0 - 0.7 * Clamp01(In.Wooded));
		Out.Wet = static_cast<float>(Wet);
		// It soaks outward from the water: half way across the land, the front stands SoakReach from it. The low
		// ground soaks before the high, and the front's edge is ragged, never ruled.
		const double Far = std::max(In.Water, 0.0);
		const double Soak = Far / (Far + SoakReach);
		// (broad fingers, with a finer fray on them: seen from high up the front is ragged, never grainy)
		const double Ragged = 0.65 * Noise(In.X / 16.0, In.Y / 16.0, 37u) + 0.35 * Noise(In.X / 5.0, In.Y / 5.0, 41u);
		Out.Arrive = static_cast<float>(Clamp01(0.72 * Soak + 0.2 * (1.0 - Low) + 0.22 * (Ragged - 0.5)));
		// A few small pools on the wettest low ground near the water, catching a dull sky: few, and never white.
		const double Speck = Noise(In.X / 1.6, In.Y / 1.6, 53u);
		Out.Pool = static_cast<float>(Smooth(0.84, 0.92, Speck) * Smooth(0.25, 0.5, River) * Smooth(0.55, 0.8, Low) * Smooth(0.5, 0.6, Wet));
		// The mud's own light and dark, in broad soft patches.
		Out.Tone = static_cast<float>(0.9 + 0.2 * Noise(In.X / 3.1, In.Y / 3.1, 71u));
		return Out;
	}

	void PaintMud(const uint8_t In[3], const FMud& Mud, const FShown& Shown, uint8_t Out[3])
	{
		const double Wet = Mud.Wet, Share = Shown.Share, Band = Shown.Band * Share;
		if (!(Wet > 0.0) || !(Share > 0.0))
		{
			Out[0] = In[0];
			Out[1] = In[1];
			Out[2] = In[2];
			return;
		}
		// Its colour by how wet it lies: the drying edge toward the road's dust, then wet earth, then the wettest by the water.
		const double ToWettest = Smooth(0.72, 1.0, Wet), ToWet = Smooth(0.2, 0.72, Wet);
		const double Cover01 = std::min(1.0, Cover * Smooth(0.05, 0.5, Wet) * Share + 0.15 * Band);
		for (int K = 0; K < 3; ++K)
		{
			double Eye = Mix(Mix(Drying[K], WetEarth[K], ToWet), Wettest[K], ToWettest);
			// (the wet band at the soak's front: darker and wetter, and gone when the turn has settled)
			Eye = Mix(Eye, Wettest[K] * 0.9, 0.6 * Band);
			double Light = ToLight(Eye) * Mud.Tone;
			Light = Mix(ToLight(In[K]), Light, Cover01);
			Light = Mix(Light, ToLight(SkyPool[K]), 0.55 * static_cast<double>(Mud.Pool) * Share);
			Out[K] = static_cast<uint8_t>(std::lround(std::min(255.0, ToEye(Light))));
		}
	}

	void MixMud(const uint8_t Dry[3], const uint8_t Whole[3], const uint8_t Banded[3], const FShown& Shown, uint8_t Out[3])
	{
		const std::array<double, 256>& Light = LightOf();
		const std::array<uint8_t, EyeSteps>& Eye = EyeOf();
		const double Share = Clamp01(Shown.Share), Band = Clamp01(Shown.Band) * Share;
		for (int K = 0; K < 3; ++K)
		{
			// (the band's own weight grows with the share as the paint's does: the cover under it is the share's)
			const double Mixed = Mix(Light[Dry[K]], Light[Whole[K]], Share) + Band * Share * (Light[Banded[K]] - Light[Whole[K]]);
			Out[K] = Eye[static_cast<size_t>(std::sqrt(Clamp01(Mixed)) * static_cast<double>(EyeSteps - 1) + 0.5)];
		}
	}

	float SnowArrive(double South, double High, double Grain, bool bComing)
	{
		const double A = bComing
			? 0.68 * Clamp01(South) + 0.32 * (1.0 - Clamp01(High)) + 0.2 * (Grain - 0.5)
			: 0.62 * (1.0 - Clamp01(South)) + 0.38 * Clamp01(High) + 0.2 * (Grain - 0.5);
		return static_cast<float>(Clamp01(A));
	}

	uint8_t SnowNow(uint8_t Was, uint8_t Will, const FShown& Shown)
	{
		return static_cast<uint8_t>(std::lround(Mix(Was, Will, Clamp01(Shown.Share))));
	}
}
