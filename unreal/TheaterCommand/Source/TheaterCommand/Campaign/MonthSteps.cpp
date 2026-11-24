#include "Campaign/MonthSteps.h"

namespace MonthSteps
{
	const char* NameOf(EPart Part)
	{
		switch (Part)
		{
		case EPart::Ground: return "MonthGround";
		case EPart::Season: return "MonthSeason";
		case EPart::Armies: return "MonthArmies";
		case EPart::Rest: return "MonthRest";
		case EPart::Orders: return "MonthOrders";
		case EPart::Keep: return "MonthKeep";
		case EPart::News: return "MonthNews";
		case EPart::Done: return "MonthDone";
		}
		return "Month";
	}

	std::vector<EPart> Order(int Mode)
	{
		// (1 is the one frame's own order: ShowWar's ground, SetSeason and ShowArmies, then ShowWarRest; FinishMonth's
		// eye, orders and card; its save; its news. Only the frames change.)
		if (Mode == 1)
		{
			return { EPart::Ground, EPart::Season, EPart::Armies, EPart::Rest, EPart::Orders, EPart::Keep, EPart::News, EPart::Done };
		}
		// (2: the news is made due straight after the ground has changed hands, so that the seals come in over the
		// table while it is laid. The save goes before the news, as it always has.)
		if (Mode == 2)
		{
			return { EPart::Ground, EPart::Keep, EPart::News, EPart::Season, EPart::Armies, EPart::Rest, EPart::Orders, EPart::Done };
		}
		return {};
	}

	void FQueue::Add(const char* Name, std::function<void()> Work)
	{
		Steps.push_back(FStep{ Name, std::move(Work) });
	}

	void FQueue::AddNext(std::vector<FStep> Next)
	{
		for (auto It = Next.rbegin(); It != Next.rend(); ++It)
		{
			Steps.push_front(std::move(*It));
		}
	}

	int FQueue::RunOne(const std::function<double()>& Clock)
	{
		if (Steps.empty())
		{
			return 0;
		}
		// (taken off before it runs: it may add, drop or run the others)
		FStep Step = std::move(Steps.front());
		Steps.pop_front();
		const double Began = Clock();
		if (Step.Work)
		{
			Step.Work();
		}
		const double Took = Clock() - Began;
		++Counted.Steps;
		if (Counted.Steps == 1 || Took > Counted.LongestStep)
		{
			Counted.LongestStep = Took;
			Counted.LongestStepName = Step.Name ? Step.Name : "";
		}
		return 1;
	}

	int FQueue::RunSome(double BudgetSeconds, const std::function<double()>& Clock)
	{
		const double Began = Clock();
		int Ran = RunOne(Clock);
		while (!Steps.empty() && Clock() - Began < BudgetSeconds)
		{
			Ran += RunOne(Clock);
		}
		if (Ran > 0)
		{
			++Counted.Frames;
			const double Took = Clock() - Began;
			Counted.LongestFrame = Took > Counted.LongestFrame ? Took : Counted.LongestFrame;
		}
		return Ran;
	}

	int FQueue::RunAll(const std::function<double()>& Clock)
	{
		const double Began = Clock();
		int Ran = 0;
		while (!Steps.empty())
		{
			Ran += RunOne(Clock);
		}
		if (Ran > 0)
		{
			++Counted.Frames;
			const double Took = Clock() - Began;
			Counted.LongestFrame = Took > Counted.LongestFrame ? Took : Counted.LongestFrame;
		}
		return Ran;
	}

	void FQueue::Reset()
	{
		Steps.clear();
		Counted = FTally();
	}
}
