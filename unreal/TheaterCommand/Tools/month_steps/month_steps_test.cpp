// The month laid in steps: the cloud's test of Campaign/MonthSteps (the game's own code, no engine).
// Tools/month_steps/run.sh builds it with g++ and runs it.
#include "Campaign/MonthSteps.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace MonthSteps;

static int Failures = 0;
#define EXPECT(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

// A clock the test moves: each step says how long it "takes".
static double Now = 0.0;
static double ClockNow() { return Now; }

static std::string Join(const std::vector<std::string>& Seen)
{
	std::string Out;
	for (const std::string& One : Seen)
	{
		Out += (Out.empty() ? "" : ",") + One;
	}
	return Out;
}

int main()
{
	const std::function<double()> Clock = ClockNow;

	// 1. The orders. Mode 1 is the one frame's own order; mode 2 the same parts with the keeping and the news moved up
	// after the ground; every part once in each; nothing for any other mode.
	{
		const std::vector<EPart> One = Order(1), Two = Order(2);
		EXPECT(One.size() == 8 && Two.size() == 8);
		EXPECT(Order(0).empty() && Order(3).empty() && Order(-1).empty());
		const EPart Expected[] = { EPart::Ground, EPart::Season, EPart::Armies, EPart::Rest, EPart::Orders, EPart::Keep, EPart::News, EPart::Done };
		for (int i = 0; i < 8 && i < (int)One.size(); ++i)
		{
			EXPECT(One[i] == Expected[i]);
		}
		// every part once in mode 2
		for (EPart Part : Expected)
		{
			int Count = 0;
			for (EPart Each : Two) { Count += Each == Part ? 1 : 0; }
			EXPECT(Count == 1);
		}
		// mode 2: ground first, keep before news, news before the season, done last; the table's own parts in the one
		// frame's order among themselves (ground, season, armies, rest, orders)
		auto At = [&Two](EPart P) { for (int i = 0; i < (int)Two.size(); ++i) { if (Two[i] == P) return i; } return -1; };
		EXPECT(At(EPart::Ground) == 0);
		EXPECT(At(EPart::Keep) < At(EPart::News));
		EXPECT(At(EPart::News) < At(EPart::Season));
		EXPECT(At(EPart::Season) < At(EPart::Armies) && At(EPart::Armies) < At(EPart::Rest) && At(EPart::Rest) < At(EPart::Orders));
		EXPECT(At(EPart::Done) == 7);
		for (EPart Part : Expected)
		{
			EXPECT(std::string(NameOf(Part)).rfind("Month", 0) == 0);
		}
	}

	// 2. The budget: the first step always runs, then more while the time since the call began is under the budget;
	// cheap steps share a frame, a dear one stands alone; the order never changes.
	{
		FQueue Queue;
		std::vector<std::string> Seen;
		auto Takes = [&Seen](const char* Name, double Seconds) { return [&Seen, Name, Seconds]() { Seen.push_back(Name); Now += Seconds; }; };
		Queue.Add("a", Takes("a", 0.073)); // dear: alone
		Queue.Add("b", Takes("b", 0.001));
		Queue.Add("c", Takes("c", 0.002));
		Queue.Add("d", Takes("d", 0.003)); // b+c+d = 6 ms: c runs (1 ms < 6), d runs (3 ms < 6), then 6 ms is not under 6
		Queue.Add("e", Takes("e", 0.240));
		Queue.Add("f", Takes("f", 0.0));
		Now = 0.0;
		std::vector<int> Frames;
		while (!Queue.Empty())
		{
			Frames.push_back(Queue.RunSome(0.006, Clock));
		}
		EXPECT(Join(Seen) == "a,b,c,d,e,f");
		EXPECT(Frames.size() == 4);
		EXPECT(Frames.size() == 4 && Frames[0] == 1 && Frames[1] == 3 && Frames[2] == 1 && Frames[3] == 1);
		EXPECT(Queue.Tally().Steps == 6 && Queue.Tally().Frames == 4);
		EXPECT(Queue.Tally().LongestStepName == "e");
		EXPECT(Queue.Tally().LongestStep > 0.239 && Queue.Tally().LongestStep < 0.241);
		EXPECT(Queue.Tally().LongestFrame > 0.239 && Queue.Tally().LongestFrame < 0.241);
		// a budget of nothing still runs one a frame
		FQueue One;
		int Count = 0;
		for (int i = 0; i < 5; ++i) { One.Add("x", [&Count]() { ++Count; }); }
		int Calls = 0;
		while (!One.Empty()) { EXPECT(One.RunSome(0.0, Clock) == 1); ++Calls; }
		EXPECT(Calls == 5 && Count == 5);
		// an empty queue runs nothing and counts no frame
		EXPECT(One.RunSome(1.0, Clock) == 0 && One.Tally().Frames == 5);
	}

	// 3. A step that finds parts of its own (the season's five) puts them at the head, in their order, before what was
	// queued after it.
	{
		FQueue Queue;
		std::vector<std::string> Seen;
		Queue.Add("ground", [&]() { Seen.push_back("ground"); });
		Queue.Add("season", [&]() {
			Seen.push_back("season");
			std::vector<FStep> Parts;
			for (const char* Name : { "snow-paint", "masks", "woods", "rivers", "light" })
			{
				Parts.push_back(FStep{ Name, [&Seen, Name]() { Seen.push_back(Name); } });
			}
			Queue.AddNext(std::move(Parts));
		});
		Queue.Add("armies", [&]() { Seen.push_back("armies"); });
		Now = 0.0;
		while (!Queue.Empty()) { Queue.RunSome(0.006, Clock); }
		EXPECT(Join(Seen) == "ground,season,snow-paint,masks,woods,rivers,light,armies");
	}

	// 4. Drained at once (an order's answer finished, a saved game taken up): everything left runs, in order, and a
	// step that runs the rest itself does not run any twice.
	{
		FQueue Queue;
		std::vector<std::string> Seen;
		Queue.Add("1", [&]() { Seen.push_back("1"); });
		Queue.Add("2", [&]() { Seen.push_back("2"); Queue.RunAll(Clock); });
		Queue.Add("3", [&]() { Seen.push_back("3"); });
		Queue.Add("4", [&]() { Seen.push_back("4"); });
		Queue.RunSome(0.0, Clock);
		EXPECT(Join(Seen) == "1");
		Queue.RunSome(0.0, Clock); // "2" drains the rest from inside itself
		EXPECT(Join(Seen) == "1,2,3,4");
		EXPECT(Queue.Empty());
		Queue.Add("5", [&]() { Seen.push_back("5"); });
		EXPECT(Queue.RunAll(Clock) == 1 && Join(Seen) == "1,2,3,4,5");
	}

	// 5. Dropped (a new war laid over the month): nothing left runs, the tally starts again, and a step that drops the
	// queue stops the frame there.
	{
		FQueue Queue;
		std::vector<std::string> Seen;
		Queue.Add("1", [&]() { Seen.push_back("1"); Queue.Reset(); });
		Queue.Add("2", [&]() { Seen.push_back("2"); });
		EXPECT(Queue.RunSome(1.0, Clock) == 1);
		EXPECT(Join(Seen) == "1" && Queue.Empty());
		Queue.Add("3", [&]() { Seen.push_back("3"); });
		Queue.Reset();
		EXPECT(Queue.Empty() && Queue.Tally().Steps == 0 && Queue.RunAll(Clock) == 0 && Join(Seen) == "1");
	}

	// 6. A step with no work, and a step with no name, are run and counted without harm.
	{
		FQueue Queue;
		Queue.Add(nullptr, nullptr);
		Queue.Add("x", std::function<void()>());
		EXPECT(Queue.RunAll(Clock) == 2 && Queue.Tally().Steps == 2);
	}

	// 7. The run 2 split, laid by mode 1 at a budget of 6 ms: no frame holds more than its one dear step, and the
	// count of frames is small (the press frame, the rules', is not here).
	{
		FQueue Queue;
		auto Takes = [](double Ms) { return [Ms]() { Now += Ms / 1000.0; }; };
		// (cloud stand-ins, not the Mac's: ground 125 = SetTownPlots 52 + SetHolders 73; the season's five parts 440 in all;
		// armies 243; rest 60; orders 20; keep 40; news 5; done 0)
		Queue.Add("ground", Takes(125));
		Queue.Add("snow-paint", Takes(220));
		Queue.Add("masks", Takes(150));
		Queue.Add("woods", Takes(60));
		Queue.Add("rivers", Takes(5));
		Queue.Add("light", Takes(5));
		Queue.Add("armies", Takes(243));
		Queue.Add("rest", Takes(60));
		Queue.Add("orders", Takes(20));
		Queue.Add("keep", Takes(40));
		Queue.Add("news", Takes(1));
		Queue.Add("done", Takes(0));
		Now = 0.0;
		int Frames = 0;
		while (!Queue.Empty()) { Queue.RunSome(0.006, Clock); ++Frames; }
		EXPECT(Queue.Tally().LongestFrame < 0.2431 && Queue.Tally().LongestFrame > 0.2429);
		EXPECT(Queue.Tally().LongestStepName == "armies");
		EXPECT(Frames <= 12);
		std::printf("run 2 stand-in: %d frames, longest %.0f ms (%s), total %.0f ms\n", Frames, Queue.Tally().LongestFrame * 1000.0, Queue.Tally().LongestStepName.c_str(), Now * 1000.0);
	}

	std::printf(Failures == 0 ? "month steps: all checks passed\n" : "month steps: %d checks FAILED\n", Failures);
	return Failures == 0 ? 0 : 1;
}
