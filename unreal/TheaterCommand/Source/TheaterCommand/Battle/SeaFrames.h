#pragma once

// The sea battle's frames split by thread (the release 2 ocean plan, step 0: measure first; nothing on screen changes).
// Each counted frame's game thread, render thread, RHI thread and graphics chip times, the most of the game's, the
// render's and the chip's (the number step 15's frame guard watches: a frame is as slow as its slowest thread), the whole
// frame on the real clock, how many times the effects kit asked for the ground under a point in it, and the battle's
// clock; and the one line ASeaBattle::SayFrameTimes says of them at a battle's end, which Tools/sea_bench.sh reads on the
// Mac. The whole frame here is counted over the same frames as the threads, after the bench's drop: release 1.5's
// frame-time line counts from 1 s of play and is kept only to be read beside 22 November's. Engine-free:
// Tools/sea_waves/measure_test.cpp builds it in the cloud.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace SeaFrames
{
	/** One measure over a battle: how many frames, the mean, the 95th and 99th in the hundred and the worst. */
	struct Spread
	{
		int N = 0;
		double Mean = 0.0, P95 = 0.0, P99 = 0.0, Worst = 0.0;
	};

	/** The Share-th of sorted values by the frame-time line's own rule (SayFrameTimes): the value at Share x (N - 1),
	 * rounded to the nearest. */
	inline double ShareOf(const std::vector<float>& Sorted, double Share)
	{
		if (Sorted.empty())
		{
			return 0.0;
		}
		const long long Last = static_cast<long long>(Sorted.size()) - 1;
		const long long Place = static_cast<long long>(std::floor(Share * static_cast<double>(Last) + 0.5));
		return Sorted[static_cast<std::size_t>(std::clamp(Place, 0LL, Last))];
	}

	inline Spread Of(std::vector<float> Values)
	{
		Spread Out;
		Out.N = static_cast<int>(Values.size());
		if (Values.empty())
		{
			return Out;
		}
		std::sort(Values.begin(), Values.end());
		double Sum = 0.0;
		for (const float V : Values)
		{
			Sum += V;
		}
		Out.Mean = Sum / static_cast<double>(Values.size());
		Out.P95 = ShareOf(Values, 0.95);
		Out.P99 = ShareOf(Values, 0.99);
		Out.Worst = Values.back();
		return Out;
	}

	/** The frames counted: each one's game thread, render thread, RHI thread and graphics chip times (milliseconds), the
	 * most of the game's, the render's and the chip's (the plan's three; the RHI thread is said beside them), the whole
	 * frame on the real clock, and the effects kit's ground asks in it; the battle's clock at the first and the last frame
	 * counted; and how many frames took more than OverMs longer than their slowest thread of the four (a wait no thread's
	 * time says: the window's, the screen's, or a thread these four do not count). At most a million frames, as the
	 * frame-time line keeps. */
	struct Record
	{
		static constexpr std::size_t MostFrames = 1000000;
		static constexpr double OverMs = 2.0;
		std::vector<float> Game, Render, Rhi, Chip, Most, Whole, Ground;
		double FirstT = 0.0, LastT = 0.0;
		int Over = 0;

		void Add(double GameMs, double RenderMs, double RhiMs, double ChipMs, double WholeMs, int GroundAsks, double BattleSeconds)
		{
			if (Game.size() >= MostFrames)
			{
				return;
			}
			FirstT = Game.empty() ? BattleSeconds : FirstT;
			LastT = BattleSeconds;
			Game.push_back(static_cast<float>(GameMs));
			Render.push_back(static_cast<float>(RenderMs));
			Rhi.push_back(static_cast<float>(RhiMs));
			Chip.push_back(static_cast<float>(ChipMs));
			const double Three = std::max(GameMs, std::max(RenderMs, ChipMs));
			Most.push_back(static_cast<float>(Three));
			Whole.push_back(static_cast<float>(WholeMs));
			Ground.push_back(static_cast<float>(GroundAsks));
			Over += WholeMs > std::max(Three, RhiMs) + OverMs ? 1 : 0;
		}

		std::size_t Num() const { return Game.size(); }
	};

	/** The threads' line, said once a battle after the frame-time line, in a form Tools/sea_bench.sh reads (bench_table.py):
	 * "Sea battle: threads over N frames from S s of play (the battle's B to E s): game m/p95/p99 ms, render ..., RHI ...,
	 * graphics chip ..., the most of the three (game, render, chip) ..., the whole frame ... (mean/95th/99th in the
	 * hundred); K frames (P%) more than 2 ms over their slowest thread; the kit's ground asked A times a frame on the mean,
	 * B at the 95th, C at most." FromSeconds: the play before it was left out (the bench's warming). */
	inline std::string Line(const Record& R, double FromSeconds)
	{
		char Text[1024];
		if (R.Num() == 0)
		{
			std::snprintf(Text, sizeof(Text), "Sea battle: threads: no frames counted from %.0f s of play.", FromSeconds);
			return Text;
		}
		const Spread G = Of(R.Game), D = Of(R.Render), H = Of(R.Rhi), C = Of(R.Chip), M = Of(R.Most), W = Of(R.Whole), A = Of(R.Ground);
		std::snprintf(Text, sizeof(Text),
			"Sea battle: threads over %d frames from %.0f s of play (the battle's %.0f to %.0f s): game %.2f/%.2f/%.2f ms, "
			"render %.2f/%.2f/%.2f ms, RHI %.2f/%.2f/%.2f ms%s, graphics chip %.2f/%.2f/%.2f ms, the most of the three (game, "
			"render, chip) %.2f/%.2f/%.2f ms, the whole frame %.2f/%.2f/%.2f ms (mean/95th/99th in the hundred); %d frames "
			"(%.1f%%) more than %.0f ms over their slowest thread; the kit's ground asked %.0f times a frame on the mean, %.0f "
			"at the 95th, %.0f at most%s.",
			G.N, FromSeconds, R.FirstT, R.LastT, G.Mean, G.P95, G.P99, D.Mean, D.P95, D.P99, H.Mean, H.P95, H.P99,
			H.Worst <= 0.0 ? " (the RHI thread read 0 every frame: its work is in the render thread's)" : "",
			C.Mean, C.P95, C.P99, M.Mean, M.P95, M.P99, W.Mean, W.P95, W.P99, R.Over, 100.0 * R.Over / static_cast<double>(G.N),
			Record::OverMs, A.Mean, A.P95, A.Worst, C.Worst <= 0.0 ? " (the graphics chip read 0 every frame: not timed on this machine)" : "");
		return Text;
	}
}
