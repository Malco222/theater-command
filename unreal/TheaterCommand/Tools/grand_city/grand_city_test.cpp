// Grander great cities on the campaign map, their cloud test (Tools/grand_city/run.sh): the game's own
// Campaign/GrandCity.h, no engine.

#include "Campaign/GrandCity.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace TheaterGrandCity;

namespace
{
	int Checks = 0, Faults = 0;

	void Check(bool bOk, const std::string& What)
	{
		++Checks;
		if (!bOk)
		{
			++Faults;
			std::printf("FAULT: %s\n", What.c_str());
		}
	}

	/** Dice for the towns made by chance: the same every run. */
	struct FDice
	{
		std::uint64_t State = 0x9E3779B97F4A7C15ull;
		double Next()
		{
			State ^= State << 13;
			State ^= State >> 7;
			State ^= State << 17;
			return static_cast<double>(State >> 11) / 9007199254740992.0;
		}
		double Between(double Low, double High) { return Low + (High - Low) * Next(); }
	};

	const double Pi = 3.14159265358979323846;

	/** A town as the builder sees it (FMiniatureMesh::Township's own measures, copied here for the reckoning). */
	struct FTown
	{
		bool bCapital = true;
		double Reach = 17.5; // map units
		double Um = 2.1; // a house-unit, in map units
		double Yaw0 = 0.0; // which way its long streets run
		int Factories = 0;
		int Seed = 0;
		std::vector<double> Arms; // the roads and railways out of it
		double PitchA() const { return 1.95 * Um; }
		double PitchB() const { return 1.5 * Um; }
		int Span() const { return static_cast<int>(std::ceil(Reach * 1.1 / std::fmin(PitchA(), PitchB()))) + 1; }
		double BearingOf(double A, double B) const { return std::fmod(Yaw0 + std::atan2(B, A) * 180.0 / Pi + 720.0, 360.0); }
		/** How far the town runs on a bearing (the builder's ReachAt, for a town that is not packed). */
		double ReachAt(double Bearing) const
		{
			double Star = 0.0;
			for (const double Arm : Arms)
			{
				const double Off = std::remainder(Bearing - Arm, 360.0) / 15.0;
				Star = std::fmax(Star, std::exp(-Off * Off));
			}
			for (int K = 0; K < 4; ++K)
			{
				const double Weight = K == 3 && Factories > 0 ? 1.0 : (K == 1 ? 0.75 : 0.5);
				const double Off = std::remainder(Bearing - (Yaw0 + 90.0 * K), 360.0) / 15.0;
				Star = std::fmax(Star, Weight * std::exp(-Off * Off));
			}
			const double Out = Reach * (bCapital ? 0.78 + 0.44 * Star : 0.64 + 0.5 * Star);
			return std::fabs(std::remainder(Bearing - 15.0, 360.0)) < 50.0 ? std::fmin(Out, Reach * 1.06) : Out;
		}
		bool InSquare(double A, double B) const { return std::fabs(A) < PitchA() && std::fabs(B) < (bCapital ? 2.0 : 1.0) * PitchB(); }
	};
}

int main()
{
	// ---- The numbers that stand in for dice.
	{
		bool bInRange = true, bSame = true;
		double Sum = 0.0;
		int Low = 0, Count = 0, Differs = 0;
		for (int Seed = 0; Seed < 40; ++Seed)
		{
			for (int J = -12; J < 12; ++J)
			{
				for (int I = -12; I < 12; ++I)
				{
					const double C = Chance(I, J, Seed, 7);
					bInRange = bInRange && C >= 0.0 && C < 1.0;
					bSame = bSame && C == Chance(I, J, Seed, 7);
					Sum += C;
					Low += C < 0.25 ? 1 : 0;
					Differs += std::fabs(C - Chance(I, J, Seed, 8)) > 1.0e-6 ? 1 : 0;
					++Count;
				}
			}
		}
		Check(bInRange, "a chance is from nothing up to, never reaching, one");
		Check(bSame, "and the same each time it is asked");
		Check(std::fabs(Sum / Count - 0.5) < 0.01, "its mean over many blocks is a half");
		Check(std::fabs(static_cast<double>(Low) / Count - 0.25) < 0.01, "and a quarter of them fall in its lowest quarter");
		Check(Differs > Count * 99 / 100, "another question of the same block has another answer");
		Check(std::fabs(Between(3, -2, 17, 5, 2.0, 6.0) - (2.0 + 4.0 * Chance(3, -2, 17, 5))) < 1.0e-12, "a number laid between two bounds is that chance of the way from one to the other");
		Check(NearBearing(359.0, 2.0, 5.0) && NearBearing(2.0, 359.0, 5.0) && !NearBearing(350.0, 2.0, 5.0) && NearBearing(-178.0, 181.0, 2.0), "bearings are compared round the compass");
	}

	// ---- The heart.
	{
		Check(std::fabs(HeartTall(0.0) - 1.25) < 1.0e-12, "a closed block at the square is as tall as it is today");
		Check(std::fabs(HeartTall(1.0) - CorniceAtEdge) < 1.0e-12 && HeartTall(1.0) > 0.88, "at the heart's edge it keeps its cornice, above today's 0.88");
		Check(HeartTall(3.0) == HeartTall(1.0) && HeartTall(-1.0) == HeartTall(0.0), "and never more or less than those two");
		bool bFalls = true, bStepsDown = true;
		for (double R = 0.0; R < 1.0; R += 0.01)
		{
			bFalls = bFalls && HeartTall(R + 0.01) <= HeartTall(R) + 1.0e-12;
		}
		Check(bFalls, "the heart is never taller further out");
		const double Heart = 0.5, Middle = 0.78;
		Check(std::fabs(Storeys(Heart, Heart, Middle) - TerraceMost) < 1.0e-12 && std::fabs(Storeys(Middle, Heart, Middle) - 1.0) < 1.0e-12, "the terraces are tallest beside the heart and today's height by the middle ring");
		Check(Storeys(0.95, Heart, Middle) == 1.0 && Storeys(0.1, Heart, Middle) == TerraceMost, "and beyond those, no more and no less");
		for (double R = Heart; R < Middle; R += 0.005)
		{
			bStepsDown = bStepsDown && Storeys(R + 0.005, Heart, Middle) <= Storeys(R, Heart, Middle) + 1.0e-12;
		}
		Check(bStepsDown, "the terraces step down all the way out");
		// (a wing is built to its block's height times 0.86 to 1.1; a terrace house to 0.4 to 0.62 of its storeys)
		Check(TerraceMost * 0.62 < CorniceAtEdge * 0.86, "the tallest terrace is lower than the lowest wing of the heart's last block");
		Check(HeartRoof > 0.14 && HeartRoof < 0.32, "the heart's roofs are steeper than today's and no steeper than a house's");
	}

	// ---- What the heart's blocks carry.
	{
		int Turrets = 0, Domes = 0, Count = 0;
		bool bSame = true;
		for (int Seed = 0; Seed < 300; ++Seed)
		{
			for (int J = -3; J < 3; ++J)
			{
				for (int I = -2; I < 2; ++I)
				{
					const EAccent Kind = Accent(I, J, Seed);
					bSame = bSame && Kind == Accent(I, J, Seed);
					Turrets += Kind == EAccent::Turret ? 1 : 0;
					Domes += Kind == EAccent::Dome ? 1 : 0;
					++Count;
				}
			}
		}
		Check(bSame, "a block carries the same thing each time it is asked");
		Check(std::fabs(static_cast<double>(Turrets) / Count - TurretShare) < 0.02, "four blocks in ten carry a turret");
		Check(std::fabs(static_cast<double>(Domes) / Count - DomeShare) < 0.02, "and three in twenty a dome");
		// (a capital's own dome: its top 1.7 house-units up and a full unit across; its spike to 2.15)
		Check(AccentTop(EAccent::Dome, HeartTall(0.0)) < 1.8 && DomeWide < 0.7, "a block's dome is smaller than a capital's own, and its spike ends below the capital's");
		Check(DrumTall(HeartTall(0.0)) + DomeRise < 1.7, "and the dome itself stands lower than a capital's own");
		Check(AccentTop(EAccent::Turret, HeartTall(0.0)) < 1.8, "a turret's point stands under the tip of a capital's dome");
		Check(TurretTall(HeartTall(1.0)) > HeartTall(1.0) * WingMost && DrumTall(HeartTall(1.0)) > HeartTall(1.0) * WingMost, "both stand over the tallest wing their block can have");
		Check(DrumTall(HeartTall(0.5)) + DomeRise > HeartTall(0.5) * 0.98 + HeartRoof, "a dome shows over the roof ridge of a wing of the usual height");
		Check(TurretWide > 0.42 && TurretWide < 0.5, "a turret is a little wider than the wing it stands on, so that no two faces lie in one plane");
		Check(AccentTop(EAccent::None, 1.25) == 0.0, "a block that carries nothing has nothing over it");
	}

	// ---- Where the suburb lies.
	{
		const double Um = 2.1, Reach = 17.5, Limit = 0.78 * Reach;
		const double From = FringeFrom(Limit, Um), To = FringeOut * Reach;
		bool bNoneInside = true, bNoneBeyond = true, bThins = true, bInBounds = true;
		// (the builder lays a block where Far <= Limit * (0.96 + 0.08 * throw) - 0.35 Um, the throw under one)
		for (double Throw = 0.0; Throw < 1.0; Throw += 0.001)
		{
			const double Laid = Limit * (0.96 + 0.08 * Throw) - 0.35 * Um;
			bNoneInside = bNoneInside && FringeShare(Laid, Limit, Reach, Um) == 0.0;
		}
		Check(bNoneInside, "the suburb never takes a block the builder may lay, whatever it throws");
		for (double Far = To + 1.0e-9; Far < 2.0 * Reach; Far += 0.01)
		{
			bNoneBeyond = bNoneBeyond && FringeShare(Far, Limit, Reach, Um) == 0.0;
		}
		Check(bNoneBeyond, "nor a block beyond the town's own measure");
		for (double Far = From; Far + 0.01 <= To; Far += 0.01)
		{
			const double Here = FringeShare(Far, Limit, Reach, Um);
			bThins = bThins && FringeShare(Far + 0.01, Limit, Reach, Um) <= Here + 1.0e-12;
			bInBounds = bInBounds && Here >= 0.4 - 1.0e-9 && Here <= 0.9 + 1.0e-9;
		}
		Check(bThins, "the suburb thins toward its rim");
		Check(bInBounds, "from nine blocks in ten at the last houses to four in ten at the rim");
		Check(FringeShare(0.5 * (From + To), 1.2 * Reach, Reach, Um) == 0.0, "along a road out of the town, where the town itself runs past its measure, there is no suburb");
		Check(FringeShare(0.955 * Reach, 0.94 * Reach, Reach, Um) == 0.0, "nor a sliver of one where the ring would be thinner than a villa's plot");
		Check(!InFringe(0, 0, 1, 0.0) && FringeOut < 1.0 && VillaOut < 1.0, "a block with no chance is not the suburb's; the suburb and its villas stay inside the town's measure");
	}

	// ---- A plot of the suburb, and a garden at the edge.
	{
		bool bSame = true, bOnLawn = true, bApart = true, bTreesClear = true, bSizes = true, bPaints = true, bLawns = true, bEdge = true;
		int Plots = 0, Villas = 0, Trees = 0, Lit = 0, Gardens = 0, Asked = 0;
		for (int Seed = 0; Seed < 250; ++Seed)
		{
			for (int J = -8; J < 8; ++J)
			{
				for (int I = -6; I < 6; ++I)
				{
					const FPlot Made = Plot(I, J, Seed, 0.3);
					const FPlot Again = Plot(I, J, Seed, 0.3);
					++Plots;
					Villas += Made.Villas;
					Trees += Made.Garden.Trees;
					bSame = bSame && Made.Villas == Again.Villas && Made.Villa[0].A == Again.Villa[0].A && Made.Villa[0].Turn == Again.Villa[0].Turn && Made.Garden.Trees == Again.Garden.Trees && Made.Garden.Long == Again.Garden.Long;
					const FGarden& G = Made.Garden;
					bLawns = bLawns && G.bLawn && G.Long >= 1.3 && G.Long <= LawnLongMost && G.Wide >= 0.95 && G.Wide <= LawnWideMost
						&& std::fabs(G.A) + 0.5 * G.Long < 0.5 * 1.65 + 0.03 && std::fabs(G.B) + 0.5 * G.Wide < 0.5 * 1.22 + 0.03;
					bSizes = bSizes && Made.Villas >= 1 && Made.Villas <= 2 && G.Trees >= 0 && G.Trees <= 4;
					for (int K = 0; K < Made.Villas; ++K)
					{
						const FVilla& V = Made.Villa[K];
						Lit += V.bLit ? 1 : 0;
						bSizes = bSizes && V.Long >= 0.5 && V.Long <= 0.8 && V.Wide >= 0.36 && V.Wide <= 0.54 && V.High >= 0.3 && V.High <= 0.46 && V.Roof >= 0.22 && V.Roof <= 0.3
							&& (Made.Villas == 1 ? V.Long >= 0.62 && std::fabs(V.Turn) <= 10.0 : V.Long <= 0.62 && std::fabs(V.Turn - 90.0) <= 8.0);
						bPaints = bPaints && V.Wall >= 0 && V.Wall <= 4 && V.Tile >= 0 && V.Tile <= 4;
						// (each corner of her walls is on the lawn)
						const double Turned = V.Turn * Pi / 180.0;
						for (const double EndA : { -0.5, 0.5 })
						{
							for (const double EndB : { -0.5, 0.5 })
							{
								const double CA = V.A + EndA * V.Long * std::cos(Turned) - EndB * V.Wide * std::sin(Turned);
								const double CB = V.B + EndA * V.Long * std::sin(Turned) + EndB * V.Wide * std::cos(Turned);
								bOnLawn = bOnLawn && std::fabs(CA - G.A) <= 0.5 * G.Long + 1.0e-9 && std::fabs(CB - G.B) <= 0.5 * G.Wide + 1.0e-9;
								if (Made.Villas == 2)
								{
									bApart = bApart && !Beside(Made.Villa[1 - K], CA, CB, 0.04);
								}
							}
						}
						for (int T = 0; T < G.Trees; ++T)
						{
							bTreesClear = bTreesClear && !Beside(V, G.Tree[T].A, G.Tree[T].B, TrunkClear - 1.0e-9);
						}
					}
					for (int T = 0; T < G.Trees; ++T)
					{
						const FTreeSpot& Spot = G.Tree[T];
						bSizes = bSizes && Spot.High >= 0.45 && Spot.High <= 0.7 && Spot.Round >= 0.16 && Spot.Round <= 0.24
							&& std::fabs(Spot.A - G.A) < 0.5 * G.Long && std::fabs(Spot.B - G.B) < 0.5 * G.Wide;
					}
					const FGarden Edge = EdgeGarden(I, J, Seed);
					++Asked;
					Gardens += Edge.bLawn ? 1 : 0;
					bEdge = bEdge && (Edge.bLawn ? (Edge.Trees == 2 && Edge.Long <= LawnLongMost && Edge.Wide <= LawnWideMost) : Edge.Trees == 0);
				}
			}
		}
		Check(bSame, "a plot is the same each time it is asked");
		Check(bSizes, "one villa the size of a house at the town's edge along the street, or two smaller ones across it, and up to four trees of the town's own sizes on the lawn");
		Check(bPaints, "its paints are among the place's five walls and five roofs");
		Check(bLawns, "its lawn is smaller than a block, about the block's middle");
		Check(bOnLawn, "every corner of every villa stands on its own lawn");
		Check(bApart, "two villas of one plot stand clear of each other");
		Check(bTreesClear, "no tree's trunk stands against a villa's wall");
		Check(bEdge, "an edge block's garden is a lawn and two trees, or nothing");
		Check(std::fabs(static_cast<double>(Gardens) / Asked - EdgeGardens) < 0.02, "about two edge blocks in three have a garden");
		Check(std::fabs(static_cast<double>(Lit) / Villas - 0.3) < 0.02, "the villas keep lamps in their windows as often as the town's houses do");
		Check(static_cast<double>(Trees) / Plots > 1.5, "a plot keeps most of its trees");
		// (two gardens never meet: a lawn reaches no further from its block's middle than half a block, and the blocks are a street apart)
		Check(0.08 + 0.5 * LawnLongMost < 0.5 * 1.95 - 0.1 && 0.05 + 0.5 * LawnWideMost < 0.5 * 1.5 - 0.1, "the lawns of two blocks side by side never meet: a walk of a fifth of a house-unit at the least lies between them");
		std::printf("Plots: %d reckoned; %.2f villas and %.2f trees to a plot\n", Plots, static_cast<double>(Villas) / Plots, static_cast<double>(Trees) / Plots);
	}

	// ---- Whole towns made by chance: where their suburbs fall.
	{
		FDice Dice;
		int Towns = 0, WithSuburb = 0, PlotsAll = 0, VillasAll = 0, MostPlots = 0, NearRoad = 0, Inside = 0, Beyond = 0, InSquares = 0, VillaBeyond = 0;
		int CapitalPlots = 0, Capitals = 0, CityPlots = 0, Cities = 0;
		for (int N = 0; N < 600; ++N)
		{
			FTown Town;
			Town.bCapital = N % 2 == 0;
			Town.Reach = Town.bCapital ? Dice.Between(14.0, 19.0) : Dice.Between(9.5, 13.5);
			Town.Yaw0 = Dice.Between(-1.0, 1.0) * (Town.bCapital ? 10.0 : 24.0);
			Town.Factories = static_cast<int>(Dice.Between(0.0, 4.0));
			Town.Seed = static_cast<int>(Dice.Between(0.0, 4000.0));
			const int Roads = static_cast<int>(Dice.Between(2.0, 7.0));
			for (int R = 0; R < Roads; ++R)
			{
				Town.Arms.push_back(Dice.Between(0.0, 360.0));
			}
			int Plots = 0;
			for (int J = -Town.Span(); J < Town.Span(); ++J)
			{
				for (int I = -Town.Span(); I < Town.Span(); ++I)
				{
					const double A = (I + 0.5) * Town.PitchA(), B = (J + 0.5) * Town.PitchB();
					const double Far = std::hypot(A, B), Bearing = Town.BearingOf(A, B), Limit = Town.ReachAt(Bearing);
					const double Share = FringeShare(Far, Limit, Town.Reach, Town.Um);
					if (!InFringe(I, J, Town.Seed, Share))
					{
						continue;
					}
					bool bRoad = false;
					for (const double Arm : Town.Arms)
					{
						bRoad = bRoad || NearBearing(Bearing, Arm, 12.0);
					}
					if (bRoad)
					{
						++NearRoad; // (the builder leaves these out; here they are only counted)
						continue;
					}
					++Plots;
					InSquares += Town.InSquare(A, B) ? 1 : 0;
					Inside += Far <= Limit * 1.04 - 0.35 * Town.Um - 1.0e-9 ? 1 : 0;
					Beyond += Far > FringeOut * Town.Reach + 1.0e-9 ? 1 : 0;
					const FPlot Made = Plot(I, J, Town.Seed, 0.3);
					for (int K = 0; K < Made.Villas; ++K)
					{
						const double VA = A + Made.Villa[K].A * Town.Um, VB = B + Made.Villa[K].B * Town.Um;
						if (std::hypot(VA, VB) > VillaOut * Town.Reach)
						{
							++VillaBeyond; // (the builder leaves such a villa out)
							continue;
						}
						++VillasAll;
					}
				}
			}
			++Towns;
			WithSuburb += Plots > 0 ? 1 : 0;
			PlotsAll += Plots;
			MostPlots = std::max(MostPlots, Plots);
			(Town.bCapital ? CapitalPlots : CityPlots) += Plots;
			(Town.bCapital ? Capitals : Cities) += 1;
		}
		Check(Inside == 0, "over 600 towns made by chance, no plot of a suburb is a block the builder may lay");
		Check(Beyond == 0, "none lies beyond the town's own measure");
		Check(InSquares == 0, "none lies in the square");
		Check(WithSuburb > Towns * 9 / 10, "nearly every great city has a suburb");
		Check(MostPlots <= 40, "and none has more than forty plots of it");
		Check(static_cast<double>(CapitalPlots) / Capitals > 4.0 && static_cast<double>(CityPlots) / Cities > 4.0, "a capital and a city each have more than four plots of suburb as a rule");
		std::printf("Towns: %d made by chance; %d with a suburb; %d plots in all, %.1f to a capital, %.1f to a city, %d at the most; %d villas in all, %d left out as too far out; %d plots left out as too near a road\n",
			Towns, WithSuburb, PlotsAll, static_cast<double>(CapitalPlots) / Capitals, static_cast<double>(CityPlots) / Cities, MostPlots, VillasAll, VillaBeyond, NearRoad);
	}

	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
