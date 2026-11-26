// air_zoom_test: the player's zoom over a raid with the raid camera's row on (the Battle rules card's flySteadyEye;
// AAirBattle::Zoom, AimEye). A notch of his wheel or trackpad asks for 15% nearer or farther, and the zoom goes there
// eased (AirDesk::Overview::ZoomEase, the framing's own ease, AwayEase): no frame may move the picture's distance by 2%
// or more for each sixtieth of a second it lasts (the strategy camera's "eye steps over 2%" in air_smooth_probe; a long
// frame of 100 ms carries six sixtieths of the zoom's own glide), and it must be there within 1% in 4 real seconds.
// Played at 60 frames a second, and at a Mac's uneven frames (one of 100 ms in every six). With the row off, the notch
// lands whole in one frame, as before: 15%.
#include "Battle/AirDesk.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace
{
	struct Result
	{
		double MostStep = 0.0; // the largest move of the distance in one frame, as a share of it, for each sixtieth of a second of the frame
		double SettleS = -1.0; // real seconds until it stays within 1% of where it was asked to go
	};

	// Scrolls: (real s, notches) in order; Frame(i): the i-th frame's real seconds.
	template <typename F>
	Result Play(const std::vector<std::pair<double, double>>& Scrolls, F Frame)
	{
		double Distance = 800.0, Want = -1.0, Vel = 0.0, T = 0.0, LastAsk = 0.0, Final = 800.0;
		Result R;
		size_t Next = 0;
		for (int I = 0; T < 12.0; ++I)
		{
			const double Real = Frame(I);
			T += Real;
			while (Next < Scrolls.size() && Scrolls[Next].first <= T)
			{
				// (AAirBattle::Zoom with the row on)
				const double From = Want > 0.0 ? Want : Distance;
				Want = std::clamp(From * std::pow(0.85, Scrolls[Next].second), 120.0, 20000.0);
				Final = Want;
				LastAsk = Scrolls[Next].first;
				++Next;
			}
			const double Was = Distance;
			// (AimEye with the row on)
			if (Want > 0.0)
			{
				AirDesk::Overview::ZoomEase(Distance, Vel, Want, Real);
				if (std::fabs(Distance - Want) < 0.05 && std::fabs(Vel) < 0.05)
				{
					Distance = Want;
					Want = -1.0;
					Vel = 0.0;
				}
			}
			R.MostStep = std::max(R.MostStep, std::fabs(Distance - Was) / Was / std::max(Real * 60.0, 1.0));
			const bool bThere = std::fabs(Distance - Final) <= 0.01 * Final;
			if (!bThere) R.SettleS = -1.0;
			else if (R.SettleS < 0.0 && Next == Scrolls.size()) R.SettleS = T - LastAsk;
		}
		return R;
	}
}

int main()
{
	int Fails = 0;
	const auto Sixty = [](int) { return 1.0 / 60.0; };
	const auto Mac = [](int I) { return I % 6 == 5 ? 0.1 : 1.0 / 60.0; };
	struct Case { const char* Name; std::vector<std::pair<double, double>> Scrolls; };
	const Case Cases[] = {
		{ "a notch in", { { 0.5, 1.0 } } },
		{ "a notch out", { { 0.5, -1.0 } } },
		{ "three notches in a tenth of a second", { { 0.5, 1.0 }, { 0.55, 1.0 }, { 0.6, 1.0 } } },
		{ "a trackpad's flick, twenty small steps", { { 0.5, 0.25 }, { 0.52, 0.25 }, { 0.54, 0.25 }, { 0.56, 0.25 }, { 0.58, 0.25 }, { 0.6, 0.25 }, { 0.62, 0.25 }, { 0.64, 0.25 }, { 0.66, 0.25 }, { 0.68, 0.25 },
			{ 0.7, 0.25 }, { 0.72, 0.25 }, { 0.74, 0.25 }, { 0.76, 0.25 }, { 0.78, 0.25 }, { 0.8, 0.25 }, { 0.82, 0.25 }, { 0.84, 0.25 }, { 0.86, 0.25 }, { 0.88, 0.25 } } },
		{ "in and straight back out", { { 0.5, 2.0 }, { 0.8, -2.0 } } },
	};
	for (const Case& C : Cases)
	{
		for (int K = 0; K < 2; ++K)
		{
			const Result R = K == 0 ? Play(C.Scrolls, Sixty) : Play(C.Scrolls, Mac);
			const bool bOk = R.MostStep < 0.02 && R.SettleS >= 0.0 && R.SettleS <= 4.0;
			std::printf("%s: the zoom, %s at %s: most move in a frame %.2f%% a sixtieth of a second, there within 1%% in %.2f real s (at most 2%% and 4 s)\n", bOk ? "pass" : "FAIL", C.Name,
				K == 0 ? "60 frames a second" : "a Mac's frames", R.MostStep * 100.0, R.SettleS);
			if (!bOk) ++Fails;
		}
	}
	std::printf("the zoom with the raid camera's row off, as before: a notch steps it %.0f%% in one frame\n", (1.0 - 0.85) * 100.0);
	return Fails ? 1 : 0;
}
