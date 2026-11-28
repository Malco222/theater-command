#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

/**
 * (Release 2, the campaign map.) Fleets that lie apart, each on open water: the berths of a sea.
 *
 * Before this the first fleet in a sea lay on the sea's middle and every other one 16 units from it, on a ring. A
 * fleet's ships reach 13 to 25 units from her own middle along her length (the more ships, the larger she is drawn),
 * so two fleets in one sea stood on each other, ships through ships and plate over plate; and a sea's middle may lie
 * close under a coast, as the Channel's does 13 units off Cotentin's tip, where the fleet's lowest ship lay over the
 * land. (The art director's finds on the Mac's pictures of 26 November 2026, the Home Fleet and the Dover Fleet in the
 * Channel. Brian, 25 November 2026, of the campaign map: "I want this to visually wow people on how smooth and
 * realistic and detailed grand it looks".)
 *
 * With tc.Map.FleetBerths each sea has berths, found once: places on its own water, each with open water all round
 * it and each well clear of the last. The first is the place nearest the sea's middle that has room for a fleet
 * whichever way she lies; the rest are the nearest places that are clear of every berth before them. A narrow sea
 * gives what room it has: less water round a berth first, then less between two berths, and the old ring where even
 * that fails. The fleets of a sea take its berths in the order they are told of, as they took the ring's places.
 *
 * Plain arithmetic with no engine in it, so that the cloud can check it (Tools/fleet_berths/run.sh) against the
 * chart's own coasts. The map (ACampaignMapActor::ShowFleets) uses it. Lengths are in map units.
 */
namespace TheaterFleetBerths
{
	inline constexpr double Pi = 3.14159265358979323846;

	/** How far from a sea's middle a berth may lie, and how closely the water round the middle is searched. */
	inline constexpr double Reach = 72.0, RingStep = 3.0, AlongRing = 6.0;

	/** How far apart the places on a line of water are asked whether they are water. */
	inline constexpr double WaterStep = 2.0;

	struct FSpot
	{
		double X = 0.0, Y = 0.0;
	};

	inline double Apart(const FSpot& A, const FSpot& B)
	{
		return std::hypot(A.X - B.X, A.Y - B.Y);
	}

	/** A berth: where it is, how much open water it was found to have all round it, and how clear of the others it was asked to be. */
	struct FBerth
	{
		FSpot At;
		double Room = 0.0, Gap = 0.0;
		bool bFound = false; // (false: no berth was to be had, and this is the sea's middle as before)
	};

	/**
	 * What the first berth asks, most wanted first: open water all round, and no farther from the sea's middle than
	 * this. A fleet of 45 ships reaches 20 units from her middle: that much is asked for first, but only near the
	 * middle; then less, anywhere in reach.
	 */
	struct FFirstWant
	{
		double Room, Within;
	};
	inline constexpr FFirstWant FirstWants[] = { { 20.0, 30.0 }, { 16.0, Reach }, { 12.0, Reach }, { 9.0, Reach }, { 6.0, Reach } };

	/**
	 * What every later berth asks, most wanted first: open water all round, and this far from every berth before it.
	 * Two large fleets lying end to end want 40 between their middles; two lying side by side, 20.
	 */
	struct FNextWant
	{
		double Room, Gap;
	};
	inline constexpr FNextWant NextWants[] = {
		{ 20.0, 40.0 }, { 16.0, 40.0 }, { 20.0, 34.0 }, { 16.0, 34.0 }, { 12.0, 34.0 }, { 16.0, 30.0 }, { 12.0, 30.0 },
		{ 16.0, 26.0 }, { 12.0, 26.0 }, { 9.0, 30.0 }, { 9.0, 26.0 }, { 12.0, 22.0 }, { 9.0, 22.0 }, { 6.0, 26.0 },
		{ 6.0, 22.0 }, { 9.0, 18.0 }, { 6.0, 18.0 } };

	/** Is it water all the way from one place to another? (Room is asked every two units of the way.) */
	template <typename TRoom>
	bool WaterBetween(const FSpot& From, const FSpot& To, const TRoom& Room)
	{
		const double Long = Apart(From, To);
		const int Steps = static_cast<int>(std::ceil(Long / WaterStep));
		for (int Step = 1; Step < Steps; ++Step)
		{
			const double Share = static_cast<double>(Step) / static_cast<double>(Steps);
			if (!(Room(From.X + (To.X - From.X) * Share, From.Y + (To.Y - From.Y) * Share) > 0.0))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * The berths of one sea, the first of them first: as many as are asked for, or as many as the sea has.
	 *
	 * Middle: the sea's middle. Turn: an angle in radians that the search starts from, so that two seas do not lay
	 * their second fleets the same way (the sea's number serves). Room(X, Y): how far the nearest shore is from a
	 * place at sea, and nought or less on land or off the table. Mine(X, Y): is the place in this sea, and not in
	 * a neighbour's?
	 *
	 * There is always a first berth: the sea's middle itself, marked not found, where the search finds none.
	 */
	template <typename TRoom, typename TMine>
	std::vector<FBerth> Berths(const FSpot& Middle, int Many, double Turn, const TRoom& Room, const TMine& Mine)
	{
		// The places asked, nearest the middle first: the middle, then rings round it.
		struct FPlace
		{
			FSpot At;
			double Room = 0.0, Far = 0.0;
		};
		std::vector<FPlace> Places;
		auto Ask = [&Places, &Room, &Mine](const FSpot& At, double Far)
		{
			const double Has = Room(At.X, At.Y);
			if (Has > 0.0 && Mine(At.X, At.Y))
			{
				Places.push_back(FPlace{ At, Has, Far });
			}
		};
		Ask(Middle, 0.0);
		for (double Ring = RingStep; Ring <= Reach + 1.0e-9; Ring += RingStep)
		{
			const int Round = std::max(8, static_cast<int>(std::lround(2.0 * Pi * Ring / AlongRing)));
			for (int Each = 0; Each < Round; ++Each)
			{
				const double Angle = Turn + 2.0 * Pi * static_cast<double>(Each) / static_cast<double>(Round);
				Ask(FSpot{ Middle.X + std::cos(Angle) * Ring, Middle.Y + std::sin(Angle) * Ring }, Ring);
			}
		}

		std::vector<FBerth> Out;
		// The first berth. It is reached from the middle by water, where the middle is water: never a place in the
		// next bay over a neck of land.
		const bool bMiddleAfloat = Room(Middle.X, Middle.Y) > 0.0;
		for (const FFirstWant& Want : FirstWants)
		{
			for (const FPlace& Place : Places)
			{
				if (Place.Room >= Want.Room && Place.Far <= Want.Within && (!bMiddleAfloat || WaterBetween(Middle, Place.At, Room)))
				{
					Out.push_back(FBerth{ Place.At, Want.Room, 0.0, true });
					break;
				}
			}
			if (!Out.empty())
			{
				break;
			}
		}
		if (Out.empty())
		{
			Out.push_back(FBerth{ Middle, 0.0, 0.0, false });
			return Out;
		}
		// The rest: each the nearest place to the middle that is clear of every berth before it, and reached by water
		// from the first.
		while (static_cast<int>(Out.size()) < Many)
		{
			bool bGot = false;
			for (const FNextWant& Want : NextWants)
			{
				for (const FPlace& Place : Places)
				{
					if (Place.Room < Want.Room)
					{
						continue;
					}
					bool bClear = true;
					for (const FBerth& Taken : Out)
					{
						if (Apart(Taken.At, Place.At) < Want.Gap)
						{
							bClear = false;
							break;
						}
					}
					if (bClear && WaterBetween(Out[0].At, Place.At, Room))
					{
						Out.push_back(FBerth{ Place.At, Want.Room, Want.Gap, true });
						bGot = true;
						break;
					}
				}
				if (bGot)
				{
					break;
				}
			}
			if (!bGot)
			{
				break;
			}
		}
		return Out;
	}
}
