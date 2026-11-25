#include "Effects/WeatherFallCore.h"

#include <algorithm>
#include <cmath>

namespace WeatherFall
{
	namespace
	{
		uint64_t Mix(uint64_t X)
		{
			X += 0x9E3779B97F4A7C15ull;
			X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
			X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
			return X ^ (X >> 31);
		}

		/** A share 0 to 1 (never 1), the Index'th of a seed's stream. */
		double Share(uint64_t Seed, uint64_t Index)
		{
			return static_cast<double>(Mix(Seed ^ Mix(Index)) >> 11) * (1.0 / 9007199254740992.0);
		}

		Layer Rained(const char* Name, double Box, double Density, double Width, double Length, double Alpha, double Tone, double NearFrom, double NearFull, int Order)
		{
			Layer L;
			L.Name = Name;
			L.Box = Box;
			L.Density = Density;
			L.Fall = 8.5;
			L.Spread = 0.18;
			L.WindShare = 1.0;
			L.Width = Width;
			L.Length = Length;
			L.Alpha = Alpha;
			L.Tone = Tone;
			L.NearFrom = NearFrom;
			L.NearFull = NearFull;
			L.FarFade = 0.55;
			L.Order = Order;
			return L;
		}

		Layer Snowed(const char* Name, double Box, double Density, double Size, double Alpha, double NearFrom, double NearFull, double Flutter, int Order)
		{
			Layer L;
			L.Name = Name;
			L.Box = Box;
			L.Density = Density;
			L.Fall = 1.0;
			L.Spread = 0.35;
			L.WindShare = 0.6;
			L.Width = Size;
			L.Length = 0.0;
			L.Alpha = Alpha;
			L.Tone = 1.0;
			L.NearFrom = NearFrom;
			L.NearFull = NearFull;
			L.FarFade = 0.55;
			L.Flutter = Flutter;
			L.Order = Order;
			return L;
		}

		void ReadLayer(const std::function<bool(const char*, double&)>& Find, const std::string& Kind, Layer& L)
		{
			const std::string Stem = Kind + "." + L.Name + ".";
			auto Num = [&](const char* Key, double& Out)
			{
				const std::string Path = Stem + Key;
				double V = Out;
				if (Find(Path.c_str(), V) && std::isfinite(V)) Out = V;
			};
			Num("box_m", L.Box);
			Num("density", L.Density);
			Num("fall", L.Fall);
			Num("spread", L.Spread);
			Num("wind_share", L.WindShare);
			Num("width_m", L.Width);
			Num("length_m", L.Length);
			Num("alpha", L.Alpha);
			Num("tone", L.Tone);
			Num("near_from_m", L.NearFrom);
			Num("near_full_m", L.NearFull);
			Num("far_fade", L.FarFade);
			Num("flutter_m", L.Flutter);
			double Order = L.Order;
			Num("order", Order);
			L.Order = static_cast<int>(std::lround(Order));
			L.Box = std::clamp(L.Box, 1.0, 400.0);
			L.Density = std::clamp(L.Density, 0.0, 50.0);
			L.Fall = std::clamp(L.Fall, 0.05, 30.0);
			L.Spread = std::clamp(L.Spread, 0.0, 0.9);
			L.Width = std::clamp(L.Width, 0.0001, 1.0);
			L.Length = std::clamp(L.Length, 0.0, 10.0);
			L.Alpha = std::clamp(L.Alpha, 0.0, 1.0);
			L.NearFull = std::max(L.NearFull, L.NearFrom + 0.01);
			L.FarFade = std::clamp(L.FarFade, 0.0, 0.99);
			L.Flutter = std::clamp(L.Flutter, 0.0, 2.0);
		}
	}

	const char* NameOf(Kind K)
	{
		return K == Kind::Rain ? "rain" : K == Kind::Snow ? "snow" : "none";
	}

	Recipe Recipe::Standing()
	{
		// (the first round's numbers, for the art director: heavy rain at full strength shows some 1,800 streaks in a
		// 70 degree picture, near ones thin and bright against a hedge, far ones a faint grain over the field, covering
		// about a sixth of it; heavy snow some 1,800 flakes, soft, slow, slanting a little downwind. Each layer hands
		// over to the next as it fades: the near layer is gone by its box's edge, 4 m, the middle one whole from 4.5 m,
		// and so on. Tools/land_weather counts them)
		Recipe R;
		R.Rain = {
			Rained("near", 8.0, 2.0, 0.003, 0.55, 0.28, 1.1, 0.6, 1.6, 2),
			Rained("middle", 22.0, 1.2, 0.006, 0.75, 0.22, 1.0, 3.0, 4.5, 0),
			Rained("far", 64.0, 0.2, 0.02, 1.3, 0.14, 0.95, 8.0, 12.0, -1),
		};
		// (the layers lean about 5 degrees apart in the same wind, the art director's R4: near gusts, far steadier)
		R.Rain[0].WindShare = 1.25;
		R.Rain[2].WindShare = 0.75;
		R.MostDrops = 60000;
		R.StrengthAlpha = 0.35;
		R.StrengthLength = 0.3;
		R.Snow = {
			Snowed("near", 8.0, 2.0, 0.012, 0.9, 0.4, 1.0, 0.12, 2),
			Snowed("middle", 22.0, 1.0, 0.02, 0.8, 3.0, 4.5, 0.15, 0),
			Snowed("far", 64.0, 0.2, 0.05, 0.55, 8.0, 12.0, 0.2, -1),
		};
		R.Snow[0].WindShare = 0.75;
		R.Snow[2].WindShare = 0.45;
		return R;
	}

	void Recipe::ReadOver(const std::function<bool(const char*, double&)>& Find)
	{
		if (!Find) return;
		auto Num = [&](const char* Path, double& Out)
		{
			double V = Out;
			if (Find(Path, V) && std::isfinite(V)) Out = V;
		};
		Num("rain_step", RainStep);
		Num("snow_step", SnowStep);
		Num("least_px", LeastPx);
		Num("strength_alpha", StrengthAlpha);
		Num("strength_length", StrengthLength);
		double Most = MostDrops;
		Num("most_drops", Most);
		MostDrops = static_cast<int>(std::clamp(Most, 0.0, 60000.0));
		RainStep = std::clamp(RainStep, 0.01, 2.0);
		SnowStep = std::clamp(SnowStep, 0.005, 2.0);
		LeastPx = std::clamp(LeastPx, 0.0, 8.0);
		StrengthAlpha = std::clamp(StrengthAlpha, 0.0, 1.0);
		StrengthLength = std::clamp(StrengthLength, 0.0, 1.0);
		for (Layer& L : Rain) ReadLayer(Find, "rain", L);
		for (Layer& L : Snow) ReadLayer(Find, "snow", L);
		// (a layer left out: "rain.far.off" 1)
		auto Kept = [&](const std::string& Kind, std::vector<Layer>& Ls)
		{
			Ls.erase(std::remove_if(Ls.begin(), Ls.end(), [&](const Layer& L)
			{
				double Off = 0.0;
				const std::string Path = Kind + "." + L.Name + ".off";
				return Find(Path.c_str(), Off) && Off >= 0.5;
			}), Ls.end());
		};
		Kept("rain", Rain);
		Kept("snow", Snow);
	}

	int DropsIn(const Layer& L, int MostDrops)
	{
		const double Many = L.Density * L.Box * L.Box * L.Box;
		return static_cast<int>(std::clamp(std::lround(Many), 0L, static_cast<long>(std::max(0, MostDrops))));
	}

	double PeriodOf(const Layer& L, double Step)
	{
		return L.Box / std::max(Step, 1e-6);
	}

	int StepsOf(const Layer& L, double Step, double At)
	{
		const double Fall = L.Fall * (1.0 + L.Spread * (2.0 * At - 1.0));
		return std::max(1, static_cast<int>(std::lround(Fall / std::max(Step, 1e-6))));
	}

	double Stepped(double Wind, double Step)
	{
		if (!std::isfinite(Wind) || Step <= 0.0) return 0.0;
		return std::round(Wind / Step) * Step;
	}

	Mesh Make(const Layer& L, double Step, int MostDrops, uint64_t Seed)
	{
		Mesh M;
		const int N = DropsIn(L, MostDrops);
		M.DropCount = N;
		const size_t Points = static_cast<size_t>(N) * 4 + 2;
		M.Points.reserve(Points * 3);
		M.Corners.reserve(Points * 2);
		M.Drops.reserve(Points * 4);
		M.Triangles.reserve(static_cast<size_t>(N) * 6);
		static const float CornerOf[4][2] = { { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
		for (int I = 0; I < N; ++I)
		{
			const uint64_t K = static_cast<uint64_t>(I) * 8;
			const float X = static_cast<float>((Share(Seed, K) - 0.5) * L.Box);
			const float Y = static_cast<float>((Share(Seed, K + 1) - 0.5) * L.Box);
			const float Z = static_cast<float>((Share(Seed, K + 2) - 0.5) * L.Box);
			const float Steps = static_cast<float>(StepsOf(L, Step, Share(Seed, K + 3)));
			const float Phase = static_cast<float>(Share(Seed, K + 4));
			const float Size = static_cast<float>(Share(Seed, K + 5));
			// (its turn to show: the drops a strength shows are the same wherever the eye is, spread evenly through the box)
			const float Turn = static_cast<float>((static_cast<double>(I) + Share(Seed, K + 6)) / std::max(1, N));
			const int First = static_cast<int>(M.Points.size() / 3);
			for (int C = 0; C < 4; ++C)
			{
				M.Points.insert(M.Points.end(), { X, Y, Z });
				M.Corners.insert(M.Corners.end(), { CornerOf[C][0], CornerOf[C][1] });
				M.Drops.insert(M.Drops.end(), { Steps, Phase, Size, Turn });
			}
			M.Triangles.insert(M.Triangles.end(), { First, First + 1, First + 2, First, First + 2, First + 3 });
		}
		// (the bounds: a box's side either way of the middle, so the mesh, kept at the eye, is never left out of the picture)
		const float B = static_cast<float>(L.Box);
		for (const float S : { -B, B })
		{
			M.Points.insert(M.Points.end(), { S, S, S });
			M.Corners.insert(M.Corners.end(), { 0.0f, 0.0f });
			M.Drops.insert(M.Drops.end(), { 1.0f, 0.0f, 0.0f, 2.0f });
		}
		return M;
	}

	Params Now(const Recipe& R, const Layer& L, Kind K, double Strength, double WindX, double WindY, double Clock, double PixelAngle)
	{
		Params P;
		const double Step = R.StepOf(K);
		const double Period = PeriodOf(L, Step);
		const double S = K == Kind::None ? 0.0 : std::clamp(Strength, 0.0, 1.0);
		const double Within = Period > 0.0 ? std::fmod(std::max(0.0, Clock), Period) : 0.0;
		P.Box[0] = static_cast<float>(L.Box);
		P.Box[1] = static_cast<float>(Within);
		P.Box[2] = static_cast<float>(Period);
		P.Box[3] = static_cast<float>(S);
		P.Move[0] = static_cast<float>(Stepped(WindX * L.WindShare, Step));
		P.Move[1] = static_cast<float>(Stepped(WindY * L.WindShare, Step));
		P.Move[2] = static_cast<float>(Step);
		P.Move[3] = static_cast<float>(K == Kind::Snow ? L.Flutter : 0.0);
		P.Look[0] = static_cast<float>(L.Width);
		P.Look[1] = static_cast<float>(L.Length * (R.StrengthLength + (1.0 - R.StrengthLength) * S));
		P.Look[2] = static_cast<float>(L.NearFrom);
		P.Look[3] = static_cast<float>(L.NearFull);
		P.Lens[0] = static_cast<float>(std::max(0.0, PixelAngle));
		P.Lens[1] = static_cast<float>(R.LeastPx);
		P.Lens[2] = K == Kind::Snow ? 1.0f : 0.0f;
		P.Lens[3] = static_cast<float>(L.FarFade);
		P.Opacity = static_cast<float>(L.Alpha * (R.StrengthAlpha + (1.0 - R.StrengthAlpha) * S));
		P.Tone = static_cast<float>(L.Tone);
		return P;
	}
}
