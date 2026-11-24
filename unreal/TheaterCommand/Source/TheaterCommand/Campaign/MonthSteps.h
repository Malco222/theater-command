#pragma once

// (Map speed, 22 November 2026; the production lead's lane "Map smoothness: month change and frame cap".)
//
// The month's change laid on the table a part at a time, a few parts a frame, where it was all laid in the one frame
// of the press: in map run 2 (the Soviet Union, 1941, warm) that frame took 1,880.7 ms, the rules 252 of them and the
// table the rest (SetSeason 440, ShowWar 291, ShowArmies 243, SetHolders 73, SetTownPlots 52, BuildTowns 50). The same
// calls, in the same order, with the same answers: only the frames they fall in change (tc.Speed.MonthInSteps).
//
// Pure logic, standard C++ only, as EdgeSeals is, so that it is tested with g++ in the cloud
// (Tools/month_steps/test.cpp). The game's side is Campaign/MonthLaying.cpp.

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace MonthSteps
{
	/** The parts of a month's change, after the rules have answered. */
	enum class EPart : uint8_t
	{
		Ground, // the ground's new holders: town plots, the ink allowed, SetHolders (ShowWar's first half)
		Season, // the time of year: the snow on the provinces, the snow on the land, the woods, the rivers, the light (SetSeason, in five parts of its own)
		Armies, // the armies and the agents where they now stand (ShowArmies, ShowAgents)
		Rest, // the rest of the table: works, scars, sieges, the front line, the haze and the rest (ShowWarRest)
		Orders, // the eye back where it was after the tour, the army in hand's orders, the province's card
		Keep, // the campaign kept (the save), every month
		News, // the month's news: the great moments, the newsreel, the headlines, the card and the seals made due
		Done, // the speed log's line and the state check
	};

	/** The name the speed log gives a part. */
	const char* NameOf(EPart Part);

	/**
	 * The parts in the order a mode lays them.
	 * 1: the table first, in the order the one frame always laid it, then the keeping and the news (the card or the
	 *    seals come once the table is laid).
	 * 2: the ground first, then the keeping and the news, so that the seals slide in (0.38 s) while the season, the
	 *    armies and the rest of the table are laid under them.
	 * Any other mode: none (the month is laid in one frame, as before).
	 */
	std::vector<EPart> Order(int Mode);

	/** One step: a name for the log, and the work. */
	struct FStep
	{
		const char* Name = "";
		std::function<void()> Work;
	};

	/** What a laying cost, for the speed log's line. */
	struct FTally
	{
		int Frames = 0; // calls of RunSome that ran anything
		int Steps = 0;
		double LongestStep = 0.0; // seconds
		std::string LongestStepName;
		double LongestFrame = 0.0; // seconds of steps in one call of RunSome
	};

	/**
	 * A queue of steps, run a few a frame within a budget of time: the first always, then the next while the time
	 * since the call began is under the budget. A step is taken off the queue before it runs, so a step may add steps
	 * (AddNext), empty the queue (Reset) or run the rest itself (RunAll).
	 */
	class FQueue
	{
	public:
		void Add(const char* Name, std::function<void()> Work);
		/** Puts these at the head of the queue, in their order: the parts a step finds it has (the season's). */
		void AddNext(std::vector<FStep> Steps);
		bool Empty() const { return Steps.empty(); }
		size_t Size() const { return Steps.size(); }
		/** Runs the first step, then more while Clock() - (its value at the call) < BudgetSeconds. Returns how many ran. */
		int RunSome(double BudgetSeconds, const std::function<double()>& Clock);
		/** Runs every step left, whatever the time. Returns how many ran. */
		int RunAll(const std::function<double()>& Clock);
		/** Drops every step left, and the tally. */
		void Reset();
		const FTally& Tally() const { return Counted; }
		void ResetTally() { Counted = FTally(); }

	private:
		int RunOne(const std::function<double()>& Clock);
		std::deque<FStep> Steps;
		FTally Counted;
	};
}
