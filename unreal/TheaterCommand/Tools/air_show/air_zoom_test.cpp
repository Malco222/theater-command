// air_zoom_test: the player's zoom over a raid with the raid camera's row on (the Battle rules card's flySteadyEye;
// AAirBattle::Zoom, AimEye). A notch of his wheel or trackpad asks for 15% nearer or farther, and the zoom goes there
// eased (AirDesk::Overview::ZoomEase, in the distance's logarithm over its own ZoomEaseS): no frame may move the
// picture's distance by 2% or more for each sixtieth of a second it lasts (the strategy camera's "eye steps over 2%" in
// air_smooth_probe; a long frame of 100 ms carries six sixtieths of the zoom's own glide), and it must be there within
// 1% in 4 real seconds. Played at 60 frames a second, and at a Mac's uneven frames (one of 100 ms in every six). And it
// must not feel floaty (the first review measured the framing's own ease, AwayEase, on the zoom: a notch 95% there only
// after 2.4 s, and a swipe in kept zooming in 0.7 s after the hand turned out): a notch at 60 frames a second 95% of
// its way within 1 s, the picture going the hand's new way within 0.3 s of a swipe turning, and never more than 0.1%
// past where it was last asked to go. With the row off, the
// notch lands whole in one frame, as before: 15%.
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
		double NinetyFiveS = -1.0; // real seconds after the first ask until it is 95% of its way from where it was
		double TurnS = 0.0; // real seconds after a scroll the other way until the distance last moved the old way
		double PastShare = 0.0; // the most it went past where it was last asked to go, as a share of that
	};

	// Scrolls: (real s, notches) in order; Frame(i): the i-th frame's real seconds.
	template <typename F>
	Result Play(const std::vector<std::pair<double, double>>& Scrolls, F Frame)
	{
		double Distance = 800.0, Want = -1.0, Vel = 0.0, T = 0.0, LastAsk = 0.0, Final = 800.0, Sign = 0.0, TurnAt = -1.0;
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
				const double Way = Scrolls[Next].second > 0.0 ? -1.0 : 1.0; // (in: the distance falls)
				if (Sign != 0.0 && Way != Sign) TurnAt = LastAsk;
				Sign = Way;
				++Next;
			}
			const double Was = Distance;
			// (AimEye with the row on)
			if (Want > 0.0)
			{
				if (AirDesk::Overview::ZoomEase(Distance, Vel, Want, Real))
				{
					Want = -1.0;
				}
			}
			R.MostStep = std::max(R.MostStep, std::fabs(Distance - Was) / Was / std::max(Real * 60.0, 1.0));
			if (TurnAt >= 0.0 && (Distance - Was) * Sign < -1e-9) R.TurnS = std::max(R.TurnS, T - TurnAt);
			if (Next == Scrolls.size() && Sign != 0.0) R.PastShare = std::max(R.PastShare, (Distance - Final) * Sign / Final);
			if (R.NinetyFiveS < 0.0 && Next > 0 && std::fabs(Distance - 800.0) >= 0.95 * std::fabs(Final - 800.0)) R.NinetyFiveS = T - Scrolls[0].first;
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
		{ "six notches out at once", { { 0.5, -6.0 } } },
		{ "a trackpad's swipe in for a second, then out", { { 0.5, 0.1 }, { 0.6, 0.1 }, { 0.7, 0.1 }, { 0.8, 0.1 }, { 0.9, 0.1 }, { 1.0, 0.1 }, { 1.1, 0.1 }, { 1.2, 0.1 }, { 1.3, 0.1 }, { 1.4, 0.1 }, { 1.5, 0.1 },
			{ 1.6, -0.1 }, { 1.7, -0.1 }, { 1.8, -0.1 }, { 1.9, -0.1 }, { 2.0, -0.1 } } },
	};
	for (const Case& C : Cases)
	{
		for (int K = 0; K < 2; ++K)
		{
			const Result R = K == 0 ? Play(C.Scrolls, Sixty) : Play(C.Scrolls, Mac);
			const bool bOne = C.Scrolls.size() == 1 && std::fabs(C.Scrolls[0].second) == 1.0 && K == 0;
			const bool bOk = R.MostStep < 0.02 && R.SettleS >= 0.0 && R.SettleS <= 4.0 && R.TurnS <= 0.3 && R.PastShare <= 0.001 && (!bOne || (R.NinetyFiveS >= 0.0 && R.NinetyFiveS <= 1.0));
			std::printf("%s: the zoom, %s at %s: most move in a frame %.2f%% a sixtieth of a second, 95%% of its way in %.2f real s, there within 1%% in %.2f real s, still going the old way %.2f s after a turn, past where asked by %.3f%% (at most 2%%, %s4 s, 0.3 s and 0.1%%)\n", bOk ? "pass" : "FAIL", C.Name,
				K == 0 ? "60 frames a second" : "a Mac's frames", R.MostStep * 100.0, R.NinetyFiveS, R.SettleS, R.TurnS, R.PastShare * 100.0, bOne ? "1 s, " : "");
			if (!bOk) ++Fails;
		}
	}
	std::printf("the zoom with the raid camera's row off, as before: a notch steps it %.0f%% in one frame\n", (1.0 - 0.85) * 100.0);
	return Fails ? 1 : 0;
}
