// Funnel smoke for a fleet on the campaign map: the cloud's checks on Campaign/ShipSmoke.h (see run.sh).
#include "Campaign/ShipSmoke.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace TheaterShipSmoke;

namespace
{
	int Checks = 0, Faults = 0;

	void Check(bool bHolds, const std::string& What)
	{
		++Checks;
		if (!bHolds)
		{
			++Faults;
			std::printf("FAULT: %s\n", What.c_str());
		}
	}

	std::string Num(double V)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.4f", V);
		return Text;
	}

	const char* Names[5] = { "battleship", "carrier", "cruiser", "destroyer", "submarine" };

	/** A voyage as the map makes one (CampaignMapActor.cpp, SailShipWay): planned on open water, timed, and put in the world's measure. */
	struct FVoyage
	{
		std::vector<FSpot> At, Head;
		std::vector<double> Ran;
		double Each = 0.02, Full = 0.0, Seconds = 0.0;
	};

	FVoyage Sail(const std::vector<FSpot>& Way)
	{
		TheaterShipWay::FAsk Ask;
		Ask.Way = Way;
		Ask.Bows = FSpot{ 1.0, 0.0 };
		Ask.Rest = Ask.Bows;
		Ask.Turn = 13.0;
		Ask.Sweep = 45.0;
		Ask.Berth = 4.0;
		Ask.Step = 0.75;
		const TheaterShipWay::FCourse Course = TheaterShipWay::Plan(Ask, [](FSpot) { return true; });
		const TheaterShipWay::FTimes Times = TheaterShipWay::Time(Course, TheaterShipWay::Seconds(Course.Length), 2.4);
		FVoyage Out;
		Out.Each = Times.Tick;
		Out.Seconds = Times.Seconds;
		for (size_t K = 0; K < Times.At.size(); ++K)
		{
			Out.At.push_back(Times.At[K] * 10.0); // (a map unit is ten of the world's)
			Out.Head.push_back(Times.Head[K]);
			Out.Ran.push_back(Times.Ran[K] * 10.0);
			if (K > 0)
			{
				Out.Full = std::max(Out.Full, TheaterShipWay::Size(Out.At[K] - Out.At[K - 1]));
			}
		}
		return Out;
	}

	double Luminance(const FInk& C)
	{
		return 0.3 * C.R + 0.59 * C.G + 0.11 * C.B;
	}
}

int main()
{
	// ---- How she steams.
	{
		const FFire Still = Fire(0.0, 0.0), Best = Fire(1.0, 0.0), Forced = Fire(0.3, 2.0), Easing = Fire(0.6, -2.0);
		Check(Still.Reach == 0.0 && Still.Thick > 0.15 && Still.Thick < 0.3 && Still.Dark < 0.3, "lying still: a thin pale wisp, and nothing astern");
		Check(Still.High > 0.25 && Still.High < 0.3 && Still.Lee > 0.3 && Still.Lee < 0.4, "lying still: it stands a little over a quarter of her length above her and leans down the wind");
		Check(Best.Reach > 1.2 && Best.Reach < 2.0 && Best.High < 0.5 * Still.High + 0.1, "at her best: a length and a half astern, blown low");
		// (A steady fire is pale: the table's sea at working height is a dark blue, and pale smoke reads on it best.)
		Check(Best.Thick > 2.0 * Still.Thick && Best.Dark > Still.Dark && Best.Dark < 0.3, "at her best: twice as thick as lying still, and a pale haze still");
		Check(Luminance(Ink(Still.Dark, 0.0)) > 0.5 && Luminance(Ink(Best.Dark, 0.0)) > 0.45 && Luminance(Ink(Forced.Dark, 0.0)) > 0.28 && Luminance(Ink(Forced.Dark, 0.0)) < Luminance(Ink(Best.Dark, 0.0)) - 0.15, "a steady fire's smoke is pale, far over the tone of the table's sea (0.10 to 0.17); a forced one's is dark beside it, and over the sea still (a black is as dark as the water and cannot be seen on it)");
		Check(Forced.Dark > 0.8 && Forced.Thick > Best.Thick && Fire(0.3, Hard).Dark == Forced.Dark, "working up to speed: dark, and thicker than at her best");
		Check(Easing.Dark < Fire(0.6, 0.0).Dark && Easing.Thick < Fire(0.6, 0.0).Thick, "easing: paler and thinner than her speed alone would make it");
		bool bGrows = true, bBounded = true, bForcedMore = true;
		for (int Step = 0; Step < 100; ++Step)
		{
			const double P = Step / 100.0, Q = (Step + 1) / 100.0;
			const FFire A = Fire(P, 0.0), B = Fire(Q, 0.0);
			bGrows = bGrows && B.Thick >= A.Thick && B.Dark >= A.Dark && B.Reach > A.Reach && B.High < A.High && B.Lee < A.Lee && B.End > A.End;
			for (const double G : { -6.0, -1.0, 0.0, 0.7, 1.5, 6.0 })
			{
				for (const double Much : { 0.0, 0.5, 1.0, 2.0, 9.0 })
				{
					const FFire F = Fire(P, G, Much, Much);
					bBounded = bBounded && F.Thick >= 0.0 && F.Thick <= 1.0 && F.Dark >= 0.0 && F.Dark <= 1.0 && F.Reach >= 0.0 && F.Reach <= 3.2 && F.High > 0.1 && F.Lee > 0.15;
				}
				bForcedMore = bForcedMore && (G <= 0.0 || (Fire(P, G).Dark >= A.Dark && Fire(P, G).Thick >= A.Thick));
			}
		}
		Check(bGrows, "the faster she goes: thicker, darker, further astern, lower and less down the wind, step by step");
		Check(bBounded, "whatever is asked of it: never thicker than a puff can be, never past black, never more than twice the reach");
		Check(bForcedMore, "gathering way never makes less smoke than holding the same speed");
		Check(Fire(0.7, 0.0, 0.0).Thick == 0.0, "the measure of how thick at nought: no smoke");
		Check(Fire(0.7, 0.0, 1.0, 0.0).Reach == 0.0 && Fire(0.7, 0.0, 1.0, 2.0).Reach == 2.0 * Fire(0.7, 0.0).Reach, "the measure of how far astern: nought stands it over her, two doubles it");
		Check(Fire(-3.0, 0.0).Reach == 0.0 && Fire(7.0, 0.0).Reach == Best.Reach, "a pace out of range is held to lying still and her best");
	}

	// ---- One puff, and the plume they make.
	for (int Class = 0; Class < 4; ++Class)
	{
		const std::string Name = Names[Class];
		const double G = Girth(Class);
		for (const double Pace : { 0.0, 0.35, 1.0 })
		{
			for (const double Gather : { 0.0, 2.0 })
			{
				const FFire F = Fire(Pace, Gather);
				const std::string Case = Name + " at pace " + Num(Pace) + (Gather > 0.0 ? ", working up" : "");
				for (int Level = 0; Level < 3; ++Level)
				{
					const int Count = Puffs(Level, Pace > 0.0);
					if (Count == 0)
					{
						continue;
					}
					const FPuff Born = Puff(F, 0.0, Count, G), Gone = Puff(F, 1.0, Count, G);
					Check(Born.Astern == 0.0 && Born.Rise == 0.0 && Born.Lee == 0.0 && Born.Thick == 0.0, Case + ": a puff is born at the funnel's top, and is nothing yet");
					Check(Gone.Thick == 0.0 && std::fabs(Gone.Astern - F.Reach) < 1.0e-12 && std::fabs(Gone.Rise - F.High) < 1.0e-12 && std::fabs(Gone.Lee - F.Lee) < 1.0e-12, Case + ": and has thinned to nothing at the plume's end");
					bool bOn = true, bSmooth = true, bThin = true;
					double MostStep = 0.0;
					FPuff Before = Born;
					for (int Step = 1; Step <= 400; ++Step)
					{
						const FPuff Now = Puff(F, Step / 400.0, Count, G);
						bOn = bOn && Now.Astern >= Before.Astern && Now.Rise >= Before.Rise && Now.Lee >= Before.Lee && Now.Size >= Before.Size && Now.Stretch >= 1.0 && Now.Stretch <= 4.0;
						const double Moved = std::sqrt(std::pow(Now.Astern - Before.Astern, 2) + std::pow(Now.Rise - Before.Rise, 2) + std::pow(Now.Lee - Before.Lee, 2));
						MostStep = std::max(MostStep, Moved);
						// (a 400th of its life is a third of a frame at 60 a second: nothing may jump in it. The thickest black
						// puff comes out of its funnel over nine frames, and gains under 0.05 in a third of one.)
						bSmooth = bSmooth && Moved < 0.02 && std::fabs(Now.Thick - Before.Thick) < 0.05 && std::fabs(Now.Size * Now.Stretch - Before.Size * Before.Stretch) < 0.01;
						bThin = bThin && Now.Thick >= 0.0 && Now.Thick <= F.Thick + 1.0e-12;
						Before = Now;
					}
					Check(bOn, Case + ": it only ever goes astern, up and down the wind, and only ever grows");
					bool bRuns = true;
					for (int Step = 0; Step <= 40; ++Step)
					{
						const FPuff Now = Puff(F, Step / 40.0, Count, G);
						const double Run = std::sqrt(Now.RunAstern * Now.RunAstern + Now.RunRise * Now.RunRise + Now.RunLee * Now.RunLee);
						bRuns = bRuns && Now.RunAstern >= 0.0 && Now.RunRise >= 0.0 && Now.RunLee >= 0.0 && (Now.Stretch > 1.0 ? std::fabs(Run - 1.0) < 1.0e-9 : Run == 0.0);
					}
					Check(bRuns, Case + ": a puff drawn out is drawn out the way the plume runs, and a round one no way");
					Check(bSmooth, Case + ": nothing of it jumps (most moved in a 400th of its life: " + Num(MostStep) + " lengths)");
					Check(bThin, Case + ": never thicker than what left the funnel");
					// No beads: at every moment each puff reaches its neighbour astern, or nearly (a tenth of the way short at most).
					// And more: the longer of two neighbours reaches the other's MIDDLE, or nearly. A soft puff is thick
					// at its middle and nothing at its rim, evenly between, so two that reach each other's middles are
					// together as thick between them as either is at its own: the plume is one even ribbon, not a row of puffs.
					bool bStreak = true, bRibbon = true;
					double WorstGap = 0.0, WorstReach = 0.0;
					for (int Phase = 0; Phase < 20; ++Phase)
					{
						for (int K = 0; K + 1 < Count; ++K)
						{
							const double AgeA = (K + Phase / 20.0) / Count, AgeB = (K + 1 + Phase / 20.0) / Count;
							if (AgeB > 0.97)
							{
								continue; // (the last of it has thinned to nothing: under a twentieth of what left the funnel)
							}
							const FPuff A = Puff(F, AgeA, Count, G), B = Puff(F, AgeB, Count, G);
							const double Apart = std::sqrt(std::pow(B.Astern - A.Astern, 2) + std::pow(B.Rise - A.Rise, 2) + std::pow(B.Lee - A.Lee, 2));
							const double Covered = A.Size * A.Stretch + B.Size * B.Stretch;
							WorstGap = std::max(WorstGap, Apart / Covered);
							bStreak = bStreak && Apart <= 1.1 * Covered;
							const double Longer = std::max(A.Size * A.Stretch, B.Size * B.Stretch);
							WorstReach = std::max(WorstReach, Apart / Longer);
							bRibbon = bRibbon && Apart <= 1.1 * Longer;
						}
					}
					Check(bStreak, Case + ", " + std::to_string(Count) + " puffs: one streak, never a string of beads (worst: " + Num(WorstGap) + " of what the two cover)");
					Check(bRibbon, Case + ", " + std::to_string(Count) + " puffs: an even ribbon, each puff reaching its neighbour's middle (worst: " + Num(WorstReach) + " of the longer one's reach)");
					if (std::getenv("SHIPSMOKE_SAY"))
					{
						std::printf("   %s, %d puffs: worst %.3f of what the two cover, %.3f of the longer one's reach\n", Case.c_str(), Count, WorstGap, WorstReach);
					}
				}
			}
		}
		Check(Puff(Fire(1.0, 0.0), 0.5, 6, G).Size > 0.05 * G && Puff(Fire(1.0, 0.0), 1.0, 6, G).Size < 0.2 * G, Name + ": a puff ends under a fifth of her length across its half, times her girth");
	}
	Check(Girth(3) > Girth(2) && Girth(2) > Girth(0) && Girth(0) > Girth(1) && Girth(1) > 0.5, "a destroyer smokes most for her length, then a cruiser, a battleship, a carrier");
	Check(Girth(4) == 0.0 && Puff(Fire(1.0, 2.0), 0.4, 6, Girth(4)).Size == 0.0, "a submarine makes none");
	Check(Girth(7) == Girth(2) && Girth(-1) == Girth(2), "a class not known smokes as a cruiser");

	// ---- Its colour.
	{
		bool bNoLight = true, bGrey = true, bDarker = true, bPaler = true;
		for (int D = 0; D <= 50; ++D)
		{
			for (int A = 0; A <= 50; ++A)
			{
				const FInk C = Ink(D / 50.0, A / 50.0);
				// (the map's night rule, DrawPuff: warm is R - 2.2 B - 0.03 over nought, bright is a luminance over 0.75)
				bNoLight = bNoLight && Luminance(C) < 0.7 && C.R - 2.2 * C.B - 0.03 < 0.0;
				bGrey = bGrey && C.R >= C.G && C.G >= C.B && C.R - C.B < 0.11 && C.R - C.B >= 0.018 && C.B > 0.25;
				if (D < 50)
				{
					bDarker = bDarker && Luminance(Ink((D + 1) / 50.0, A / 50.0)) < Luminance(C);
				}
				if (A < 50)
				{
					bPaler = bPaler && Luminance(Ink(D / 50.0, (A + 1) / 50.0)) > Luminance(C);
				}
			}
		}
		Check(bNoLight, "no smoke of hers is bright or warm enough to be taken for a light by night");
		Check(bGrey, "it is grey, to the warm side (the more the darker), and never near black");
		Check(bDarker, "the harder her fires are forced the darker it leaves her");
		Check(bPaler, "and the older a puff the paler");
		Check(Luminance(Ink(1.0, 0.0)) > 0.3 && Luminance(Ink(1.0, 0.0)) < 0.38 && Luminance(Ink(0.0, 0.0)) > 0.55, "from the pale of an easy fire to the dirty brown-grey of a forced one, which is a third of white: twice the sea's tone and more");
		Check(Ink(-4.0, 9.0).R == Ink(0.0, 1.0).R && Ink(4.0, -9.0).R == Ink(1.0, 0.0).R, "out of range is held");
	}

	// ---- What it costs.
	{
		Check(Level(700.0) == 0 && Level(1499.0) == 0 && Level(1500.0) == 1 && Level(2600.0) == 1 && Level(3600.0) == 2 && Level(9000.0) == 2, "close to, from working height, from high up");
		Check(EachFunnel(0) && !EachFunnel(1) && !EachFunnel(2), "every funnel its own plume only close to");
		Check(Puffs(0, true) == 6 && Puffs(1, true) == 5 && Puffs(2, true) == 4 && Puffs(0, false) == 4 && Puffs(1, false) == 3 && Puffs(2, false) == 0, "six, five and four puffs under way; four, three and none lying still");
		Check(Puffs(-3, true) == Puffs(0, true) && Puffs(9, false) == 0, "a level out of range is held");
		// A fleet shows three ships at most, and they have seven funnels between them at most (a battleship's two, a
		// cruiser's three, a destroyer's two: Campaign/CampaignShips.cpp). Close to, every funnel has its plume.
		const int Close = 7 * Puffs(0, true), CloseLying = 7 * Puffs(0, false), Working = 3 * Puffs(1, true), High = 3 * Puffs(2, true), Lying = 3 * Puffs(1, false);
		Check(Close == 42 && CloseLying == 28 && Working == 15 && High == 12 && Lying == 9, "a fleet's worst: 42 puffs close to (28 lying still), 15 from working height (9 lying still), 12 from high up");
		Check(10 * Close <= MostPuffs, "ten fleets under way close to are within what the fleets may take (the eye that close sees two or three)");
		Check(20 * Working <= MostPuffs && 35 * High <= MostPuffs && 10 * Working + 30 * Lying <= MostPuffs, "twenty fleets under way in sight from working height, or thirty-five from high up, or ten with thirty lying still, are within what the fleets may take");
		Check(MostPuffs * 6 <= 2400 + 120, "and that is about a sixth of the room the table's smoke has");
	}

	// ---- The clock.
	{
		bool bIn = true, bApart = true, bRound = true;
		for (int Step = 0; Step < 500; ++Step)
		{
			const double Now = 1000.0 + Step * 0.0137;
			for (int K = 0; K < 6; ++K)
			{
				const double A = AgeOf(Now, K, 6, 0.37), Next = AgeOf(Now, (K + 1) % 6, 6, 0.37);
				double Gap = Next - A;
				Gap -= std::floor(Gap);
				bIn = bIn && A >= 0.0 && A < 1.0;
				bApart = bApart && std::fabs(Gap - 1.0 / 6.0) < 1.0e-9;
				bRound = bRound && std::fabs(AgeOf(Now + Life, K, 6, 0.37) - A) < 1.0e-9;
			}
		}
		Check(bIn && bApart && bRound, "six puffs evenly apart, each from the funnel to the plume's end in its life and round again");
		double Ahead = AgeOf(50.0 + 0.5, 2, 5, 0.1) - AgeOf(50.0, 2, 5, 0.1);
		Ahead -= std::floor(Ahead);
		Check(std::fabs(Ahead - 0.5 / Life) < 1.0e-9, "half a second is half a second of its life, whatever the frames between");
		Check(std::fabs(AgeOf(12.0, 0, 4, 0.0) - AgeOf(12.0, 0, 4, 0.5)) > 0.2, "the seed tells two funnels apart");
	}

	// ---- On a real course: how she steams over it.
	const FVoyage Bent = Sail({ FSpot{ 0.0, 0.0 }, FSpot{ 300.0, 0.0 }, FSpot{ 300.0, 200.0 }, FSpot{ 620.0, 260.0 } });
	const FVoyage Line = Sail({ FSpot{ 0.0, 0.0 }, FSpot{ 700.0, 0.0 } });
	for (const FVoyage* Voyage : { &Bent, &Line })
	{
		const std::string Name = Voyage == &Bent ? "a voyage of two bends" : "a straight voyage";
		const int Count = static_cast<int>(Voyage->At.size());
		Check(Count > 50 && Voyage->Full > 0.0, Name + ": a course of her times (" + std::to_string(Count) + " ticks, " + Num(Voyage->Seconds) + " s)");
		double Pace = 0.0, Gather = 0.0;
		Steams(Voyage->At.data(), Count, Voyage->Each, Voyage->Full, -0.5, Pace, Gather);
		Check(Pace == 0.0 && Gather == 0.0, Name + ": before it begins she lies still");
		Steams(Voyage->At.data(), Count, Voyage->Each, Voyage->Full, Voyage->Seconds + 0.5, Pace, Gather);
		Check(Pace == 0.0 && Gather == 0.0, Name + ": and after it ends");
		bool bHeld = true, bTrue = true;
		double Best = 0.0, MostGather = 0.0, LeastGather = 0.0, MostDark = 0.0, MostThickStep = 0.0, MostDarkStep = 0.0, FirstForced = -1.0, LastEased = -1.0;
		FFire Was60 = Fire(0.0, 0.0);
		double PaceWas = 0.0;
		const double Frame = 1.0 / 60.0;
		for (double Clock = 0.0; Clock <= Voyage->Seconds + 1.0e-9; Clock += Frame)
		{
			Steams(Voyage->At.data(), Count, Voyage->Each, Voyage->Full, Clock, Pace, Gather);
			bHeld = bHeld && Pace >= 0.0 && Pace <= 1.0;
			Best = std::max(Best, Pace);
			MostGather = std::max(MostGather, Gather);
			LeastGather = std::min(LeastGather, Gather);
			// (how fast she gathers way is the rate her pace in fact changes, near enough: within a quarter of its most)
			if (Clock > GatherOver + Frame && Clock < Voyage->Seconds - GatherOver - Frame)
			{
				double Ahead = 0.0, Behind = 0.0, Unused = 0.0;
				Steams(Voyage->At.data(), Count, Voyage->Each, Voyage->Full, Clock + 0.05, Ahead, Unused);
				Steams(Voyage->At.data(), Count, Voyage->Each, Voyage->Full, Clock - 0.05, Behind, Unused);
				bTrue = bTrue && std::fabs((Ahead - Behind) / 0.1 - Gather) < 1.2;
			}
			const FFire Now = Fire(Pace, Gather);
			MostDark = std::max(MostDark, Now.Dark);
			MostThickStep = std::max(MostThickStep, std::fabs(Now.Thick - Was60.Thick));
			MostDarkStep = std::max(MostDarkStep, std::fabs(Now.Dark - Was60.Dark));
			if (FirstForced < 0.0 && Now.Dark > 0.75)
			{
				FirstForced = Clock;
			}
			if (Gather < -0.2)
			{
				LastEased = Clock;
			}
			Was60 = Now;
			PaceWas = Pace;
		}
		(void)PaceWas;
		Check(bHeld && Best > 0.95, Name + ": her pace is a share of her best, and she reaches it (" + Num(Best) + ")");
		Check(MostGather > 0.3 && LeastGather < -0.3 && MostGather < 12.0 && LeastGather > -12.0, Name + ": she gathers way and loses it (" + Num(MostGather) + " and " + Num(LeastGather) + " shares a second at most)");
		Check(bTrue, Name + ": what is read as gathering way is the rate her pace changes");
		Check(MostDark > 0.8 && FirstForced >= 0.0 && FirstForced < 0.4 * Voyage->Seconds, Name + ": her smoke is black as she works up, early in the voyage (first at " + Num(FirstForced) + " s)");
		Check(LastEased > 0.6 * Voyage->Seconds, Name + ": and she is easing as she comes in (to " + Num(LastEased) + " s)");
		// (Her fire of the moment. No puff takes it whole: each keeps the fire that made it, see Carries below.)
		Check(MostThickStep < 0.09 && MostDarkStep < 0.2, Name + ": from one frame to the next at 60 a second her fire changes by so much and no more (thick " + Num(MostThickStep) + ", dark " + Num(MostDarkStep) + ")");
	}
	{
		double Pace = 0.0, Gather = 0.0;
		SteamsEased(0.0, 2.0, Pace, Gather);
		Check(Pace == 0.0 && Gather == 0.0, "the old sail: lying still as it begins");
		SteamsEased(0.5, 2.0, Pace, Gather);
		Check(std::fabs(Pace - 1.0) < 1.0e-12 && std::fabs(Gather) < 1.0e-12, "the old sail: at her best in the middle, neither gathering nor easing");
		SteamsEased(0.2, 2.0, Pace, Gather);
		const double EarlyPace = Pace, EarlyGather = Gather;
		SteamsEased(0.8, 2.0, Pace, Gather);
		Check(EarlyGather > 0.5 && Gather < -0.5 && std::fabs(EarlyPace - Pace) < 1.0e-12, "the old sail: she gathers way before the middle and loses it after, evenly");
		SteamsEased(1.0, 2.0, Pace, Gather);
		Check(Pace == 0.0 && Gather == 0.0, "the old sail: lying still as it ends");
		const FVoyage None;
		Steams(None.At.data(), 0, 0.02, 0.0, 1.0, Pace, Gather);
		Check(Pace == 0.0 && Gather == 0.0, "no course at all: no fault, and she lies still");
	}

	// ---- On a real course: where her smoke lies.
	{
		const int Count = static_cast<int>(Line.At.size());
		bool bStraight = true;
		for (const double Ran : { 900.0, 3500.0, 6400.0 })
		{
			for (const double Back : { 1.0, 80.0, 300.0 })
			{
				for (const FSpot Off : { FSpot{ 0.0, 0.0 }, FSpot{ 40.0, -12.0 }, FSpot{ -70.0, 9.0 } })
				{
					const FSpot Moved = Was(Line.At.data(), Line.Head.data(), Line.Ran.data(), Count, Ran, Back, Off);
					bStraight = bStraight && std::fabs(Moved.X + Back) < 1.0e-6 && std::fabs(Moved.Y) < 1.0e-6;
				}
			}
		}
		Check(bStraight, "on a straight course her smoke lies straight astern, whichever funnel");
		const FSpot Early = Was(Line.At.data(), Line.Head.data(), Line.Ran.data(), Count, 30.0, 200.0, FSpot{});
		Check(std::fabs(Early.X + 200.0) < 1.0e-6 && std::fabs(Early.Y) < 1.0e-6, "further astern than she has yet run: straight astern of where she lay");
	}
	{
		const int Count = static_cast<int>(Bent.At.size());
		const double Whole = Bent.Ran.back();
		bool bNothing = true, bOnCourse = true, bRigid = true, bSmooth = true, bAstern = true;
		double Worst = 0.0, WorstStep = 0.0;
		for (int Step = 1; Step < 200; ++Step)
		{
			const double Ran = Whole * Step / 200.0;
			FSpot Here, HereHead;
			TheaterShipWay::Along(Bent.At.data(), Bent.Head.data(), Bent.Ran.data(), Count, Ran, Here, HereHead);
			for (const FSpot Off : { FSpot{ 0.0, 0.0 }, FSpot{ 45.0, 0.0 }, FSpot{ -60.0, 14.0 } })
			{
				const FSpot None = Was(Bent.At.data(), Bent.Head.data(), Bent.Ran.data(), Count, Ran, 0.0, Off);
				bNothing = bNothing && None.X == 0.0 && None.Y == 0.0;
				FSpot Before{};
				for (int K = 1; K <= 64; ++K)
				{
					const double Back = 330.0 * K / 64.0; // (a length and a half of a battleship, in the world's measure)
					const FSpot Moved = Was(Bent.At.data(), Bent.Head.data(), Bent.Ran.data(), Count, Ran, Back, Off);
					FSpot There, ThereHead;
					TheaterShipWay::Along(Bent.At.data(), Bent.Head.data(), Bent.Ran.data(), Count, Ran - Back, There, ThereHead);
					const FSpot Lies = Here + Off + Moved; // where the smoke lies, in the world
					// (as far from where her middle then was as the funnel is from her middle now: she is carried round whole)
					bRigid = bRigid && std::fabs(TheaterShipWay::Size(Lies - There) - TheaterShipWay::Size(Off)) < 1.0e-6;
					if (Off.X == 0.0 && Off.Y == 0.0 && Ran - Back >= 0.0)
					{
						// (and her middle's smoke is on the water she crossed: no further from the nearest tick than a tick is long)
						double Nearest = 1.0e9;
						for (int Tick = 0; Tick < Count; ++Tick)
						{
							Nearest = std::min(Nearest, TheaterShipWay::Size(Lies - Bent.At[static_cast<size_t>(Tick)]));
						}
						Worst = std::max(Worst, Nearest);
						bOnCourse = bOnCourse && Nearest <= Bent.Full + 1.0e-6;
					}
					const double Stepped = TheaterShipWay::Size(Moved - Before);
					WorstStep = std::max(WorstStep, Stepped);
					bSmooth = bSmooth && Stepped < 3.0 * 330.0 / 64.0;
					// (never thrown ahead of her: at most a little of it lies forward of her beam, on the tightest turn)
					bAstern = bAstern && TheaterShipWay::Dot(Moved, TheaterShipWay::Unit(HereHead, FSpot{ 0.0, 1.0 })) < 0.35 * Back + 1.0;
					Before = Moved;
				}
			}
		}
		Check(bNothing, "round the bends: with nothing gone astern a plume begins exactly at its funnel");
		Check(bOnCourse, "round the bends: her smoke lies on the water she crossed (never further off it than " + Num(Worst) + ")");
		Check(bRigid, "round the bends: a funnel off her middle is carried round with her, and its smoke lies where it went");
		Check(bSmooth, "round the bends: the plume has no break in it (most between two of 64 stations: " + Num(WorstStep) + ")");
		Check(bAstern, "round the bends: it is never thrown ahead of her");
	}
	{
		const FSpot Astern = WasStraight(FSpot{ 0.6, 0.8 }, 50.0);
		Check(std::fabs(Astern.X + 30.0) < 1.0e-9 && std::fabs(Astern.Y + 40.0) < 1.0e-9, "with no course of her own: straight astern of her bows");
		const FSpot None = WasStraight(FSpot{ 0.6, 0.8 }, 0.0), Lost = WasStraight(FSpot{ 0.0, 0.0 }, 10.0);
		Check(None.X == 0.0 && None.Y == 0.0 && std::fabs(Lost.Y + 10.0) < 1.0e-9, "nothing astern is nothing; and bows not known are taken for the table's own");
		const FVoyage Empty;
		const FSpot Blank = Was(Empty.At.data(), Empty.Head.data(), Empty.Ran.data(), 0, 100.0, 40.0, FSpot{ 3.0, 4.0 });
		Check(Blank.X == 0.0 && Blank.Y == 0.0, "no course handed in: no fault, and nothing moved");
	}

	// ---- The fire a puff carries: as thick and as dark as the fire that made it, and never the whole plume at once.
	{
		const FFire Slow = Fire(0.2, -0.5), Forced = Fire(0.5, 0.9);
		const FFire All = Carries(Slow, Forced, 1.0), Nothing = Carries(Slow, Forced, 0.0), Half = Carries(Slow, Forced, 0.5), Over = Carries(Slow, Forced, 7.0), Under = Carries(Slow, Forced, -3.0);
		Check(Forced.Dark > Slow.Dark + 0.5 && Forced.Thick > Slow.Thick + 0.15, "(the two fires of this check are far apart: " + Num(Slow.Dark) + " and " + Num(Forced.Dark) + ")");
		auto Same = [](double A, double B) { return std::fabs(A - B) < 1.0e-12; };
		Check(Same(All.Thick, Forced.Thick) && Same(All.Dark, Forced.Dark), "a puff carries the thickness and the darkness of the fire that made it");
		Check(All.Reach == Slow.Reach && All.High == Slow.High && All.Lee == Slow.Lee && All.End == Slow.End, "and lies as the plume lies now");
		Check(Nothing.Thick == Slow.Thick && Nothing.Dark == Slow.Dark && Nothing.Reach == Slow.Reach, "keeping nothing of it, it is the smoke of the moment");
		Check(Half.Dark > Slow.Dark && Half.Dark < Forced.Dark && Half.Thick > Slow.Thick && Half.Thick < Forced.Thick, "keeping half, it is between the two");
		Check(Same(Over.Dark, All.Dark) && Same(Over.Thick, All.Thick) && Under.Dark == Nothing.Dark && Under.Thick == Nothing.Thick, "what is kept is held between nothing and all");
		Check(Keeps(0.0) == 0.0 && Keeps(-1.0) == 0.0 && Keeps(LetGo) == 1.0 && Keeps(60.0) == 1.0, "a puff keeps its fire out at sea and has let it go as her course ends");
		bool bFalls = true;
		for (double Left = 0.0; Left < LetGo; Left += 0.01)
		{
			bFalls = bFalls && Keeps(Left + 0.01) >= Keeps(Left) && Keeps(Left + 0.01) - Keeps(Left) < 0.02;
		}
		Check(bFalls, "and lets it go smoothly over the last " + Num(LetGo) + " s");
	}
	// Every puff of a plume followed frame by frame down two real courses, as the map lays it (CampaignMapActor.cpp,
	// UpdateSmoke): the fire of the moment for where it lies, the fire that made it for how thick and dark it is.
	{
		const FVoyage* Courses[2] = { &Bent, &Line };
		const char* CourseNames[2] = { "round the bends", "straight" };
		const FFire Lying = Fire(0.0, 0.0);
		for (int Which = 0; Which < 2; ++Which)
		{
			const FVoyage& Voyage = *Courses[Which];
			const int Count = static_cast<int>(Voyage.At.size());
			for (const int Rate : { 30, 60, 144 })
			{
				for (const int Many : { 6, 5, 4 })
				{
					std::vector<double> DarkWas(static_cast<size_t>(Many), 0.0), ThickWas(static_cast<size_t>(Many), 0.0), AgeWas(static_cast<size_t>(Many), 2.0);
					double KeepWas = 1.0, WholeWas = 0.0, MostAtSea = 0.0, MostComingIn = 0.0, MostWhole = 0.0;
					bool bWisps = true, bBlackGoesDown = false, bEndsStill = true;
					for (int Frame = 0;; ++Frame)
					{
						const double Clock = std::min(static_cast<double>(Frame) / Rate, Voyage.Seconds);
						const double Now = 100.0 + Clock;
						double Pace = 0.0, Gather = 0.0;
						Steams(Voyage.At.data(), Count, Voyage.Each, Voyage.Full, Clock, Pace, Gather);
						const FFire Steam = Fire(Pace, Gather);
						const double Keep = Keeps(Voyage.Seconds - Clock);
						double Youngest = 2.0, YoungestDark = 0.0, OldestBlack = -1.0;
						for (int K = 0; K < Many; ++K)
						{
							const size_t At = static_cast<size_t>(K);
							const double Age = AgeOf(Now, K, Many, 0.31);
							double MadePace = 0.0, MadeGather = 0.0;
							Steams(Voyage.At.data(), Count, Voyage.Each, Voyage.Full, Clock - Age * Life, MadePace, MadeGather);
							const FFire Own = Carries(Steam, Fire(MadePace, MadeGather), Keep);
							if (Frame > 0 && Age > AgeWas[At])
							{
								// (the same puff as the frame before, not one that has left the funnel anew)
								const double Changed = std::fabs(Own.Dark - DarkWas[At]) * Rate, Thinned = std::fabs(Own.Thick - ThickWas[At]) * Rate;
								double& Most = Keep >= 1.0 && KeepWas >= 1.0 ? MostAtSea : MostComingIn;
								Most = std::max(Most, std::max(Changed, Thinned));
							}
							// (made before she sailed, and before her fires were stirred for it: Steams reads how fast
							// she gathers way from a little before to a little after)
							if (Keep >= 1.0 && Clock - Age * Life < -GatherOver)
							{
								bWisps = bWisps && std::fabs(Own.Dark - Lying.Dark) < 1.0e-12 && std::fabs(Own.Thick - Lying.Thick) < 1.0e-12;
							}
							if (Age < Youngest)
							{
								Youngest = Age;
								YoungestDark = Own.Dark;
							}
							if (Own.Dark > 0.9)
							{
								OldestBlack = std::max(OldestBlack, Age);
							}
							if (Clock >= Voyage.Seconds)
							{
								bEndsStill = bEndsStill && Own.Dark == Steam.Dark && Own.Thick == Steam.Thick;
							}
							DarkWas[At] = Own.Dark;
							ThickWas[At] = Own.Thick;
							AgeWas[At] = Age;
						}
						// (the black made working up is still in the plume, further down it, when her newest smoke has cleared)
						bBlackGoesDown = bBlackGoesDown || (Keep >= 1.0 && YoungestDark < 0.6 && OldestBlack > Youngest + 0.1);
						if (Frame > 0)
						{
							MostWhole = std::max(MostWhole, std::fabs(Steam.Dark - WholeWas) * Rate);
						}
						WholeWas = Steam.Dark;
						KeepWas = Keep;
						if (Clock >= Voyage.Seconds)
						{
							break;
						}
					}
					const std::string Where = std::string(CourseNames[Which]) + ", " + std::to_string(Rate) + " frames a second, " + std::to_string(Many) + " puffs: ";
					Check(MostAtSea < 1.0e-6, Where + "out at sea no puff changes its darkness or thickness (most " + Num(MostAtSea) + " a second)");
					Check(MostComingIn < 2.6, Where + "coming in, a puff lets its fire go no faster than " + Num(MostComingIn) + " a second (black to pale takes over a third of a second)");
					// (What this mends: round the bends she eases and works up again, and the fire of the moment goes
					// from black to pale in about a fifth of a second. Taken for every puff, the whole plume blinked.)
					Check(Which != 0 || MostWhole > 3.0, Where + "(the fire of the moment changes by up to " + Num(MostWhole) + " a second)");
					if (std::getenv("SHIPSMOKE_SAY") && Many == 6)
					{
						std::printf("   %sthe fire of the moment changes by up to %.2f a second; a puff's own by %.6f at sea and %.2f coming in\n", Where.c_str(), MostWhole, MostAtSea, MostComingIn);
					}
					Check(bWisps, Where + "a puff made before she sailed is the wisp of a ship lying still");
					Check(bBlackGoesDown, Where + "the black of working up goes down the plume when her newest smoke has cleared");
					Check(bEndsStill, Where + "as her course ends every puff is the smoke of the moment");
				}
			}
		}
	}
	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
