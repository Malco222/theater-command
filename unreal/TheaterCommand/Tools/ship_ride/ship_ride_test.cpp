// A fleet under way, second part: the cloud's checks on Campaign/ShipRide.h (see run.sh).
#include "Campaign/ShipRide.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace TheaterShipRide;

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

	const char* Names[5] = { "battleship", "carrier", "cruiser", "destroyer", "submarine" };

	/** The biggest hull of each class in Campaign/CampaignShips.cpp, in metres: length, beam, and the lowest deck amidships. */
	struct FBuilt
	{
		double Long, Beam, Deck;
	};
	const FBuilt Built[5] = {
		{ 270.0, 38.9, 6.0 },  // battleships: C.H rows, the longest, the widest, the lowest deck
		{ 257.0, 29.0, 9.0 },  // carriers
		{ 205.0, 21.6, 5.5 },  // cruisers
		{ 123.0, 11.6, 3.0 },  // destroyers (the flush-decked American one is 4.0)
		{ 95.0, 8.3, 1.6 },    // submarines
	};

	/** A turn as a course gives it: her head's rate comes on over a third of a second, holds, and goes. */
	double TurnAt(double T, double Full, double Held)
	{
		const double On = 0.35;
		if (T < 0.0 || T > Held + 2.0 * On)
		{
			return 0.0;
		}
		const double In = T < On ? T / On : T > Held + On ? (Held + 2.0 * On - T) / On : 1.0;
		return Full * In * In * (3.0 - 2.0 * In);
	}

	/** Her heel through such a turn at a frame rate: the most, the least, where she ends, and when she is upright again after it. */
	struct FRun
	{
		double Most = 0.0, Least = 0.0, AtEnd = 0.0, Upright = -1.0, Held = 0.0, FastestRate = 0.0;
	};
	FRun Run(const FKind& Kind, double Full, double Pace, double Frames, double HeldFor)
	{
		FRun Out;
		FHeel Heel;
		const double Each = 1.0 / Frames, Over = HeldFor + 0.7;
		for (double T = 0.0; T < Over + 6.0; T += Each)
		{
			Lean(Heel, HeelWanted(Kind, TurnAt(T, Full, HeldFor), Pace), Each, Kind);
			Out.Most = std::max(Out.Most, Heel.Angle);
			Out.Least = std::min(Out.Least, Heel.Angle);
			Out.FastestRate = std::max(Out.FastestRate, std::fabs(Heel.Rate));
			if (std::fabs(T - (0.35 + HeldFor)) < Each * 0.5)
			{
				Out.Held = Heel.Angle;
			}
			if (T > Over && Out.Upright < 0.0 && Upright(Heel))
			{
				Out.Upright = T - Over;
			}
		}
		Out.AtEnd = Heel.Angle;
		return Out;
	}
}

int main()
{
	// ---- What each kind of ship is.
	for (int Class = 0; Class < 5; ++Class)
	{
		const FKind Kind = KindOf(Class);
		const std::string Name = Names[Class];
		Check(Kind.Heel > 2.0 * Degree && Kind.Heel < 10.0 * Degree, Name + ": her heel is a ship's, between two and ten degrees");
		Check(Kind.Damp > 0.4 && Kind.Damp < 0.8, Name + ": her roll dies, but she rolls back once");
		Check(Kind.Pitch < 1.0 * Degree && Kind.Roll < 2.0 * Degree, Name + ": the swell moves her a little, not a storm");
		Check(Kind.PitchTime > 3.0 && Kind.RollTime > 4.0, Name + ": she is slow in the swell");
	}
	Check(KindOf(3).Heel > KindOf(2).Heel && KindOf(2).Heel > KindOf(0).Heel && KindOf(0).Heel > KindOf(1).Heel, "a destroyer heels more than a cruiser, a cruiser more than a battleship, a carrier least of the big ships");
	Check(KindOf(3).Quick > KindOf(2).Quick && KindOf(2).Quick > KindOf(0).Quick, "the small ships are the quick ones");
	Check(KindOf(7).Heel == KindOf(2).Heel, "a kind nobody named rides as a cruiser");

	// ---- The heel her turn asks.
	for (int Class = 0; Class < 5; ++Class)
	{
		const FKind Kind = KindOf(Class);
		const std::string Name = Names[Class];
		Check(HeelWanted(Kind, 0.0, 1.0) == 0.0, Name + ": no turn, no heel");
		Check(HeelWanted(Kind, 2.0, 0.0) == 0.0, Name + ": swung round where she lies, she does not heel");
		Check(HeelWanted(Kind, 1.5, 1.0) > 0.0 && HeelWanted(Kind, -1.5, 1.0) < 0.0, Name + ": to starboard she leans to port, and to port to starboard (outward)");
		Check(std::fabs(HeelWanted(Kind, 1.5, 1.0) + HeelWanted(Kind, -1.5, 1.0)) < 1.0e-12, Name + ": the same either way");
		bool bGrows = true, bWithin = true;
		for (double Turn = 0.0; Turn < 6.0; Turn += 0.05)
		{
			bGrows = bGrows && HeelWanted(Kind, Turn + 0.05, 1.0) >= HeelWanted(Kind, Turn, 1.0);
			bWithin = bWithin && HeelWanted(Kind, Turn, 1.0) <= Kind.Heel && HeelWanted(Kind, Turn, 3.0) <= Kind.Heel;
		}
		Check(bGrows, Name + ": the harder the turn, the more she heels");
		Check(bWithin, Name + ": never past her most, however hard the turn or whatever speed is given");
		Check(HeelWanted(Kind, 1.5, 1.0) > 0.9 * Kind.Heel, Name + ": near her most by a radian and a half a second");
		Check(HeelWanted(Kind, 0.3, 1.0) < 0.4 * Kind.Heel, Name + ": a gentle turn is a gentle heel");
		Check(std::fabs(HeelWanted(Kind, 1.5, 0.5) - 0.5 * HeelWanted(Kind, 1.5, 1.0)) < 1.0e-12, Name + ": at half her speed, half the heel");
	}

	// ---- Her roll has weight.
	for (int Class = 0; Class < 5; ++Class)
	{
		const FKind Kind = KindOf(Class);
		const std::string Name = Names[Class];
		const FRun At60 = Run(Kind, 2.0, 1.0, 60.0, 3.0), At30 = Run(Kind, 2.0, 1.0, 30.0, 3.0), At120 = Run(Kind, 2.0, 1.0, 120.0, 3.0), At20 = Run(Kind, 2.0, 1.0, 20.0, 3.0);
		const double Asked = HeelWanted(Kind, 2.0, 1.0);
		Check(std::fabs(At60.Held - Asked) < 0.03 * Asked, Name + ": held in a turn, she comes to the heel asked");
		Check(At60.Most > Asked * 1.02 && At60.Most < Asked * 1.25, Name + ": she rolls a little past it first, and no more than a quarter");
		Check(At60.Least < -0.02 * Asked && At60.Least > -0.3 * Asked, Name + ": coming out of the turn she rolls back past upright once, a little");
		Check(At60.Upright > 0.3 && At60.Upright < 4.0, Name + ": and is upright again within four seconds");
		Check(std::fabs(At60.AtEnd) < 0.01 * Degree, Name + ": nothing is left of it");
		Check(std::fabs(At30.Most - At60.Most) < 0.02 * Asked && std::fabs(At120.Most - At60.Most) < 0.02 * Asked && std::fabs(At20.Most - At60.Most) < 0.03 * Asked, Name + ": the same roll at 20, 30, 60 and 120 frames a second");
		Check(At60.FastestRate < 40.0 * Degree, Name + ": she never rolls faster than forty degrees a second");
		// (a short turn: she does not reach her heel, and that is right)
		const FRun Short = Run(Kind, 2.0, 1.0, 60.0, 0.05);
		Check(Short.Most <= At60.Most + 1.0e-9, Name + ": a flick of the helm throws her no further than a turn held");
		// (a frame of a whole second, as when the game hitches: taken as a quarter, and steady)
		FHeel Hitch;
		for (int K = 0; K < 40; ++K)
		{
			Lean(Hitch, Asked, 1.0, Kind);
		}
		Check(std::isfinite(Hitch.Angle) && std::fabs(Hitch.Angle - Asked) < 0.02 * Asked, Name + ": a hitch in the game does not throw her");
		FHeel Still;
		Still.Angle = 0.03;
		Lean(Still, 0.0, 0.0, Kind);
		Check(Still.Angle == 0.03 && Still.Rate == 0.0, Name + ": no time, no change");
		Lean(Still, 0.0, -1.0, Kind);
		Check(Still.Angle == 0.03, Name + ": time run backward is no time");
	}
	{
		FHeel None;
		Check(Upright(None), "a ship that has not turned is upright");
		None.Angle = 1.0 * Degree;
		Check(!Upright(None), "a degree of heel is not upright");
		None.Angle = 0.0;
		None.Rate = 1.0 * Degree;
		Check(!Upright(None), "nor is a ship still rolling through it");
	}

	// ---- The swell.
	for (int Class = 0; Class < 5; ++Class)
	{
		const FKind Kind = KindOf(Class);
		const std::string Name = Names[Class];
		double MostPitch = 0.0, MostRoll = 0.0, MostHeave = 0.0, Step = 0.0, Both = 0.0, One = 0.0, Other = 0.0, Again = 0.0, Self = 0.0;
		const double Each = 1.0 / 60.0;
		FSea Last = Swell(Kind, 0.0, 0.4, 1.0);
		for (double T = 0.0; T < 240.0; T += Each)
		{
			const FSea A = Swell(Kind, T, 0.4, 1.0), B = Swell(Kind, T, 2.9, 1.0), Later = Swell(Kind, T + 30.0, 0.4, 1.0);
			MostPitch = std::max(MostPitch, std::fabs(A.Pitch));
			MostRoll = std::max(MostRoll, std::fabs(A.Roll));
			MostHeave = std::max(MostHeave, std::fabs(A.Heave));
			Step = std::max(Step, std::max(std::fabs(A.Pitch - Last.Pitch), std::fabs(A.Roll - Last.Roll)));
			Both += A.Pitch * B.Pitch;
			One += A.Pitch * A.Pitch;
			Other += B.Pitch * B.Pitch;
			Again += A.Roll * Later.Roll;
			Self += A.Roll * A.Roll;
			Last = A;
		}
		Check(MostPitch <= Kind.Pitch * 1.0001 && MostPitch > 0.8 * Kind.Pitch, Name + ": her bows rise and fall by what her kind says, and no more");
		Check(MostRoll <= Kind.Roll * 1.0001 && MostRoll > 0.8 * Kind.Roll, Name + ": she rolls by what her kind says, and no more");
		Check(MostHeave <= Kind.Heave * 1.0001, Name + ": and heaves no more");
		Check(Step < 0.05 * Degree, Name + ": no jump from one frame to the next");
		Check(std::fabs(Both) / std::sqrt(One * Other) < 0.9, Name + ": two ships of a fleet do not pitch as one");
		Check(std::fabs(Again) / Self < 0.9, Name + ": half a minute on she is not doing the same again");
		const FSea Gone = Swell(Kind, 7.3, 0.4, 0.0), Half = Swell(Kind, 7.3, 0.4, 0.5), Whole = Swell(Kind, 7.3, 0.4, 1.0), Over = Swell(Kind, 7.3, 0.4, 5.0);
		Check(Gone.Pitch == 0.0 && Gone.Roll == 0.0 && Gone.Heave == 0.0, Name + ": with the eye far off there is no swell");
		Check(std::fabs(Half.Pitch - 0.5 * Whole.Pitch) < 1.0e-12 && std::fabs(Half.Roll - 0.5 * Whole.Roll) < 1.0e-12, Name + ": it comes on by degrees");
		Check(Over.Pitch == Whole.Pitch, Name + ": and never more than the whole of it");
	}

	// ---- How she is set on the water.
	for (int Class = 0; Class < 5; ++Class)
	{
		const FKind Kind = KindOf(Class);
		const FBuilt Hull = Built[Class];
		const std::string Name = Names[Class];
		const FSea Calm;
		const FSet Level = Set(Kind, Hull.Long, Hull.Beam, 0.0, 1.0, 0.0, Calm, 0.0);
		Check(Level.UpX == 0.0 && Level.UpY == 0.0 && Level.UpZ == 1.0 && Level.Rise == 0.0, Name + ": with no heel and no swell she lies exactly as she was built");
		// (her bows toward +Y: her port side is +X. A turn to starboard leans her to port.)
		const FSet Over = Set(Kind, Hull.Long, Hull.Beam, 0.0, 1.0, Kind.Heel, Calm, 0.0);
		Check(Over.UpX > 0.0 && std::fabs(Over.UpY) < 1.0e-12, Name + ": heeled to port, her masts lean to port and neither forward nor aft");
		Check(std::fabs(std::atan2(Over.UpX, Over.UpZ) - Kind.Heel) < 1.0e-9, Name + ": by the angle of her heel");
		FSea Down;
		Down.Pitch = Kind.Pitch;
		const FSet Nose = Set(Kind, Hull.Long, Hull.Beam, 0.0, 1.0, 0.0, Down, 1.0);
		Check(Nose.UpY > 0.0 && std::fabs(Nose.UpX) < 1.0e-12, Name + ": bows down, her masts lean forward");
		// (the same ship lying the other way on the table leans the other way on the table, and the same way by her own sides)
		const FSet Turned = Set(Kind, Hull.Long, Hull.Beam, 1.0, 0.0, Kind.Heel, Calm, 0.0);
		Check(Turned.UpY < 0.0 && std::fabs(Turned.UpX) < 1.0e-12, Name + ": bows toward +X, her port side is -Y, and she leans there");

		// No part of her bottom shows, and no lee rail goes under: every heel she can reach (a sixth past her most,
		// for the roll past it), every moment of the swell, each way she can lie.
		double Highest = -1.0e9, LowestRail = 1.0e9, LowestStern = 1.0e9, MostDown = 0.0;
		bool bUnit = true;
		for (double Around = 0.0; Around < TwoPi; Around += 0.7)
		{
			const double AX = std::cos(Around), AY = std::sin(Around), PX = AY, PY = -AX;
			for (double Heel = -1.17; Heel <= 1.17; Heel += 0.13)
			{
				for (double T = 0.0; T < 60.0; T += 0.21)
				{
					for (double Much = 0.0; Much <= 1.0; Much += 0.5)
					{
						const FSea Sea = Swell(Kind, T, 1.3, Much);
						const FSet Now = Set(Kind, Hull.Long, Hull.Beam, AX, AY, Heel * Kind.Heel, Sea, Much);
						bUnit = bUnit && std::fabs(Now.UpX * Now.UpX + Now.UpY * Now.UpY + Now.UpZ * Now.UpZ - 1.0) < 1.0e-9;
						MostDown = std::min(MostDown, Now.Rise);
						// (a point of her in her own frame, so far ahead, so far to port, so high: where it is once she
						// is set. Turned the short way from up to her masts: for these small angles, its height is
						// what it was along her masts, less what her lean takes from its place on the water.)
						auto High = [&](double Ahead, double Port, double Up)
						{
							const double X = AX * Ahead + PX * Port, Y = AY * Ahead + PY * Port;
							// Rodrigues, about the axis that takes up to her masts.
							const double KX = -Now.UpY, KY = Now.UpX; // (up cross masts, not yet a unit)
							const double Sine = std::sqrt(KX * KX + KY * KY), Cosine = Now.UpZ;
							if (Sine < 1.0e-12)
							{
								return Up + Now.Rise;
							}
							const double UX = KX / Sine, UY = KY / Sine;
							// v' = v cos + (k x v) sin + k (k.v)(1 - cos); its height:
							const double CrossZ = UX * Y - UY * X, DotK = UX * X + UY * Y;
							(void)DotK; // (k lies flat: it adds nothing to height)
							return Up * Cosine + CrossZ * Sine + Now.Rise;
						};
						for (const double End : { -0.5, 0.0, 0.5 })
						{
							for (const double Side : { -0.5, 0.5 })
							{
								// (her bottom's edge: the full beam amidships, six tenths of it at the stern, a point at the bow)
								const double Wide = End > 0.25 ? 0.0 : End < -0.25 ? 0.6 : 1.0;
								Highest = std::max(Highest, High(End * Hull.Long, Side * Wide * Hull.Beam, 0.0));
							}
						}
						for (const double Side : { -0.5, 0.5 })
						{
							LowestRail = std::min(LowestRail, High(0.0, Side * Hull.Beam, Hull.Deck));
							LowestStern = std::min(LowestStern, High(-0.5 * Hull.Long, Side * 0.6 * Hull.Beam, Hull.Deck));
						}
					}
				}
			}
		}
		Check(bUnit, Name + ": the way her masts point is always a unit");
		const double Lifted = 0.5 * Hull.Long * std::sin(Kind.Pitch) + 0.5 * Hull.Beam * std::sin(Kind.Roll) + Kind.Heave * Hull.Beam + 0.5 * Hull.Beam * std::sin(1.17 * Kind.Heel);
		Check(Highest < (1.0 - Let) * Lifted + 1.0e-6 && Highest < 0.45 * Hull.Deck, Name + ": her bottom never lifts off the sea by more than the bilge a ship shows, under half her freeboard");
		Check(LowestRail > 0.3, Name + ": her lee rail amidships never comes within 0.3 m of the sea");
		Check(LowestStern > 0.3, Name + ": nor does her quarter");
		Check(MostDown > -0.34 * Hull.Deck, Name + ": she is never let down by a third of her freeboard");
		std::printf("%s: lee rail never under %.2f m (deck %.1f m), quarter %.2f m, let down at most %.2f m, bilge shown at most %.2f m\n", Names[Class], LowestRail, Hull.Deck, LowestStern, -MostDown, Highest);
	}
	// (the swell alone shows very little of her bottom)
	for (int Class = 0; Class < 5; ++Class)
	{
		const FKind Kind = KindOf(Class);
		const FBuilt Hull = Built[Class];
		double Highest = -1.0e9;
		for (double T = 0.0; T < 120.0; T += 0.05)
		{
			const FSea Sea = Swell(Kind, T, 0.8, 1.0);
			const FSet Now = Set(Kind, Hull.Long, Hull.Beam, 0.0, 1.0, 0.0, Sea, 1.0);
			// (small angles: a point's height is what her lean gives it, and her rise)
			for (const double End : { -0.5, 0.5 })
			{
				for (const double Side : { -0.5, 0.5 })
				{
					const double Wide = End > 0.0 ? 0.0 : 0.6;
					const double Height = -(End * Hull.Long) * Now.UpY / Now.UpZ - (Side * Wide * Hull.Beam) * Now.UpX / Now.UpZ;
					Highest = std::max(Highest, Height * Now.UpZ + Now.Rise);
				}
			}
			for (const double Side : { -0.5, 0.5 })
			{
				Highest = std::max(Highest, -(Side * Hull.Beam) * Now.UpX + Now.Rise);
			}
		}
		const double Lifted = 0.5 * Hull.Long * std::sin(Kind.Pitch) + 0.5 * Hull.Beam * std::sin(Kind.Roll) + Kind.Heave * Hull.Beam;
		Check(Highest <= (1.0 - Let) * Lifted + 1.0e-9 && Highest < 0.2 * Hull.Deck, std::string(Names[Class]) + ": lying in the swell with no turn on, her forefoot shows by under a fifth of her freeboard");
	}

	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
