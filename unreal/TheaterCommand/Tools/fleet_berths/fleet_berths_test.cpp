// Fleets that lie apart on the campaign map, their cloud test (Tools/fleet_berths/run.sh): the game's own
// Campaign/FleetBerths.h, no engine. Made-up seas first, then the chart's own coasts.

#include "Campaign/FleetBerths.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace TheaterFleetBerths;

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

	std::string Text(double Value)
	{
		char Buffer[48];
		std::snprintf(Buffer, sizeof(Buffer), "%.1f", Value);
		return Buffer;
	}

	/** The least distance between any two of a sea's berths. */
	double LeastApart(const std::vector<FBerth>& Got)
	{
		double Least = 1.0e9;
		for (size_t A = 0; A < Got.size(); ++A)
		{
			for (size_t B = A + 1; B < Got.size(); ++B)
			{
				Least = std::min(Least, Apart(Got[A].At, Got[B].At));
			}
		}
		return Least;
	}

	/** What every sea's berths must be, whatever the sea: said of a made-up sea or a real one. */
	template <typename TRoom, typename TMine>
	void CheckBerths(const std::string& Sea, const FSpot& Middle, const std::vector<FBerth>& Got, const TRoom& Room, const TMine& Mine)
	{
		Check(!Got.empty(), Sea + ": there is always a first berth");
		for (size_t K = 0; K < Got.size(); ++K)
		{
			const FBerth& Berth = Got[K];
			const std::string Which = Sea + " berth " + std::to_string(K);
			if (!Berth.bFound)
			{
				Check(K == 0 && Got.size() == 1 && Apart(Berth.At, Middle) < 1.0e-9, Which + ": a berth not found is the middle, and the only one");
				continue;
			}
			Check(Room(Berth.At.X, Berth.At.Y) >= Berth.Room - 1.0e-9, Which + ": has the open water it says, " + Text(Berth.Room));
			Check(Berth.Room >= 6.0, Which + ": never less than 6 of open water");
			Check(Mine(Berth.At.X, Berth.At.Y), Which + ": is in its own sea");
			Check(Apart(Berth.At, Middle) <= Reach + 1.0e-6, Which + ": is within reach of the middle");
			Check(WaterBetween(Got[0].At, Berth.At, Room), Which + ": is reached by water from the first");
			for (size_t Before = 0; Before < K; ++Before)
			{
				Check(Apart(Got[Before].At, Berth.At) >= Berth.Gap - 1.0e-9, Which + ": is " + Text(Berth.Gap) + " clear of berth " + std::to_string(Before));
			}
			Check(K == 0 || Berth.Gap >= 18.0, Which + ": never less than 18 from another");
		}
	}

	bool Same(const std::vector<FBerth>& A, const std::vector<FBerth>& B)
	{
		if (A.size() != B.size())
		{
			return false;
		}
		for (size_t K = 0; K < A.size(); ++K)
		{
			if (A[K].At.X != B[K].At.X || A[K].At.Y != B[K].At.Y || A[K].Room != B[K].Room || A[K].Gap != B[K].Gap || A[K].bFound != B[K].bFound)
			{
				return false;
			}
		}
		return true;
	}

	void MadeUpSeas()
	{
		const auto Anywhere = [](double, double) { return true; };

		// Open water: the first fleet on the middle, the rest 40 clear, as many as are asked.
		{
			const auto Room = [](double, double) { return 1000.0; };
			const FSpot Middle{ 100.0, 200.0 };
			const std::vector<FBerth> Got = Berths(Middle, 6, 0.0, Room, Anywhere);
			CheckBerths("open water", Middle, Got, Room, Anywhere);
			Check(Got.size() == 6, "open water: six berths asked, six had");
			Check(Apart(Got[0].At, Middle) < 1.0e-9 && Got[0].Room == 20.0, "open water: the first berth is the middle, with 20 round it");
			Check(LeastApart(Got) >= 40.0, "open water: every two berths 40 apart, got " + Text(LeastApart(Got)));
			Check(Got.size() > 1 && Apart(Got[1].At, Middle) <= 42.0 + 1.0e-9, "open water: the second berth is the nearest ring that is clear");
			Check(Same(Got, Berths(Middle, 6, 0.0, Room, Anywhere)), "open water: asked twice, the same berths");
			const std::vector<FBerth> Turned = Berths(Middle, 6, 1.3, Room, Anywhere);
			Check(Apart(Turned[0].At, Got[0].At) < 1.0e-9 && Apart(Turned[1].At, Got[1].At) > 1.0, "open water: a turn moves the second berth and not the first");
			Check(Berths(Middle, 1, 0.0, Room, Anywhere).size() == 1, "open water: one asked, one had");
			Check(Berths(Middle, 0, 0.0, Room, Anywhere).size() == 1, "open water: none asked, the first is still had");
		}
		// A channel 40 wide whose middle is 13 off its south shore, as the Channel's is off Cotentin: the first berth
		// moves out to mid-channel, and the second lies along the channel, not across it.
		{
			const auto Room = [](double, double Y) { return 20.0 - std::fabs(Y); };
			const FSpot Middle{ 0.0, 7.0 };
			const std::vector<FBerth> Got = Berths(Middle, 3, 0.0, Room, Anywhere);
			CheckBerths("channel", Middle, Got, Room, Anywhere);
			Check(Got.size() == 3, "channel: three berths");
			Check(Got[0].Room >= 16.0 && std::fabs(Got[0].At.Y) < 7.0, "channel: the first berth has 16 round it and lies nearer mid-channel than the middle did");
			Check(Apart(Got[0].At, Middle) <= 9.0, "channel: and no farther from the middle than it need be, " + Text(Apart(Got[0].At, Middle)));
			Check(Got.size() > 1 && std::fabs(Got[1].At.X - Got[0].At.X) >= 30.0, "channel: the second lies along the channel");
			Check(LeastApart(Got) >= 34.0, "channel: berths 34 apart or more, got " + Text(LeastApart(Got)));
		}
		// An islet close by the middle: the first berth stands off it.
		{
			const auto Room = [](double X, double Y) { return std::hypot(X - 5.0, Y) - 2.0; };
			const FSpot Middle{ 0.0, 0.0 };
			const std::vector<FBerth> Got = Berths(Middle, 2, 0.0, Room, Anywhere);
			CheckBerths("islet", Middle, Got, Room, Anywhere);
			Check(Got[0].bFound && Room(Got[0].At.X, Got[0].At.Y) >= 20.0, "islet: the first berth has 20 clear of the islet");
			Check(Apart(Got[0].At, Middle) <= 21.0 + 1.0e-9, "islet: and is the nearest such place, " + Text(Apart(Got[0].At, Middle)));
		}
		// The middle itself on land (a round island 10 across its half): a berth on the water beside it.
		{
			const auto Room = [](double X, double Y) { return std::hypot(X, Y) - 10.0; };
			const FSpot Middle{ 0.0, 0.0 };
			const std::vector<FBerth> Got = Berths(Middle, 2, 0.0, Room, Anywhere);
			CheckBerths("middle on land", Middle, Got, Room, Anywhere);
			Check(Got[0].bFound && Got[0].Room >= 16.0, "middle on land: a berth is found on the water");
		}
		// No water at all: the middle, marked not found, and nothing else.
		{
			const auto Room = [](double, double) { return 0.0; };
			const FSpot Middle{ 3.0, 4.0 };
			const std::vector<FBerth> Got = Berths(Middle, 4, 0.0, Room, Anywhere);
			CheckBerths("no water", Middle, Got, Room, Anywhere);
			Check(Got.size() == 1 && !Got[0].bFound, "no water: the middle as before, not found");
		}
		// A neighbour's sea east of x = 10: no berth of ours lies in it.
		{
			const auto Room = [](double, double) { return 1000.0; };
			const auto Mine = [](double X, double) { return X <= 10.0; };
			const FSpot Middle{ 0.0, 0.0 };
			const std::vector<FBerth> Got = Berths(Middle, 6, 0.0, Room, Mine);
			CheckBerths("neighbour", Middle, Got, Room, Mine);
			Check(Got.size() == 6, "neighbour: six berths on our own side");
		}
		// A neck of land 3 wide running north and south, 25 east of the middle: the water beyond it is not ours to
		// reach, though it is nearer than much that is.
		{
			const auto Room = [](double X, double) { return X < 25.0 ? 25.0 - X : X > 28.0 ? X - 28.0 : 0.0; };
			const FSpot Middle{ 0.0, 0.0 };
			const std::vector<FBerth> Got = Berths(Middle, 6, 0.0, Room, Anywhere);
			CheckBerths("neck of land", Middle, Got, Room, Anywhere);
			bool bAllWest = true;
			for (const FBerth& Berth : Got)
			{
				bAllWest = bAllWest && Berth.At.X < 25.0;
			}
			Check(bAllWest && Got.size() >= 3, "neck of land: every berth on the middle's own side of it");
		}
		// A strait 10 wide: no room for anyone, and the middle stands as before. One 14 wide: berths with 6 round them.
		{
			const auto Narrow = [](double, double Y) { return 5.0 - std::fabs(Y); };
			const auto Wider = [](double, double Y) { return 7.0 - std::fabs(Y); };
			const FSpot Middle{ 0.0, 2.0 };
			const std::vector<FBerth> None = Berths(Middle, 3, 0.0, Narrow, Anywhere);
			CheckBerths("strait of 10", Middle, None, Narrow, Anywhere);
			Check(None.size() == 1 && !None[0].bFound, "strait of 10: the middle as before");
			const std::vector<FBerth> Some = Berths(Middle, 3, 0.0, Wider, Anywhere);
			CheckBerths("strait of 14", Middle, Some, Wider, Anywhere);
			Check(Some.size() == 3 && Some[0].bFound && Some[0].Room == 6.0, "strait of 14: three berths with 6 round them");
			Check(LeastApart(Some) >= 26.0, "strait of 14: and 26 apart along it, got " + Text(LeastApart(Some)));
		}
	}

	// ---- The chart's own coasts.

	struct FRing
	{
		std::vector<FSpot> Points;
		double MinX = 1.0e9, MinY = 1.0e9, MaxX = -1.0e9, MaxY = -1.0e9;
	};

	struct FChart
	{
		double Wide = 0.0, High = 0.0;
		std::vector<FSpot> Seas;
		std::vector<std::string> Names;
		std::vector<FRing> Coasts;
	};

	bool ReadChart(const char* Path, FChart& Chart)
	{
		std::ifstream File(Path);
		std::string Word;
		size_t Count = 0;
		if (!(File >> Word >> Chart.Wide >> Chart.High) || Word != "table" || !(File >> Word >> Count) || Word != "seas")
		{
			return false;
		}
		for (size_t K = 0; K < Count; ++K)
		{
			FSpot Sea;
			std::string Name;
			File >> Sea.X >> Sea.Y >> Name;
			Chart.Seas.push_back(Sea);
			Chart.Names.push_back(Name);
		}
		if (!(File >> Word >> Count) || Word != "rings")
		{
			return false;
		}
		for (size_t K = 0; K < Count; ++K)
		{
			size_t Points = 0;
			File >> Points;
			FRing Ring;
			for (size_t P = 0; P < Points; ++P)
			{
				FSpot Point;
				File >> Point.X >> Point.Y;
				Ring.Points.push_back(Point);
				Ring.MinX = std::min(Ring.MinX, Point.X);
				Ring.MinY = std::min(Ring.MinY, Point.Y);
				Ring.MaxX = std::max(Ring.MaxX, Point.X);
				Ring.MaxY = std::max(Ring.MaxY, Point.Y);
			}
			Chart.Coasts.push_back(Ring);
		}
		return static_cast<bool>(File) && !Chart.Seas.empty() && !Chart.Coasts.empty();
	}

	bool Inside(const FRing& Ring, double X, double Y)
	{
		if (X < Ring.MinX || X > Ring.MaxX || Y < Ring.MinY || Y > Ring.MaxY)
		{
			return false;
		}
		bool bIn = false;
		for (size_t K = 0, Last = Ring.Points.size() - 1; K < Ring.Points.size(); Last = K++)
		{
			const FSpot& A = Ring.Points[K];
			const FSpot& B = Ring.Points[Last];
			if ((A.Y > Y) != (B.Y > Y) && X < (B.X - A.X) * (Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bIn = !bIn;
			}
		}
		return bIn;
	}

	double ToSegment(const FSpot& A, const FSpot& B, double X, double Y)
	{
		const double Dx = B.X - A.X, Dy = B.Y - A.Y;
		const double Long = Dx * Dx + Dy * Dy;
		const double Share = Long > 0.0 ? std::clamp(((X - A.X) * Dx + (Y - A.Y) * Dy) / Long, 0.0, 1.0) : 0.0;
		return std::hypot(X - (A.X + Dx * Share), Y - (A.Y + Dy * Share));
	}

	/** How far the nearest coast, or the table's edge, is from a place at sea; nought on land or off the table. Looks no farther than 100. */
	double ChartRoom(const FChart& Chart, double X, double Y)
	{
		double Least = std::min({ X, Y, Chart.Wide - X, Chart.High - Y, 100.0 });
		if (Least <= 0.0)
		{
			return 0.0;
		}
		for (const FRing& Ring : Chart.Coasts)
		{
			if (X < Ring.MinX - Least || X > Ring.MaxX + Least || Y < Ring.MinY - Least || Y > Ring.MaxY + Least)
			{
				continue;
			}
			if (Inside(Ring, X, Y))
			{
				return 0.0;
			}
			for (size_t K = 0, Last = Ring.Points.size() - 1; K < Ring.Points.size(); Last = K++)
			{
				Least = std::min(Least, ToSegment(Ring.Points[K], Ring.Points[Last], X, Y));
			}
		}
		return Least;
	}

	/** A count of its own: is every place within a round of a berth water? (Not by the measure the berth was found by.) */
	bool AllWater(const FChart& Chart, const FSpot& At, double Round)
	{
		for (double Out = 0.0; Out <= Round + 1.0e-9; Out += 1.0)
		{
			const int Spokes = std::max(1, static_cast<int>(std::ceil(2.0 * Pi * Out)));
			for (int Spoke = 0; Spoke < Spokes; ++Spoke)
			{
				const double Angle = 2.0 * Pi * static_cast<double>(Spoke) / static_cast<double>(Spokes);
				const double X = At.X + std::cos(Angle) * Out, Y = At.Y + std::sin(Angle) * Out;
				if (X < 0.0 || Y < 0.0 || X > Chart.Wide || Y > Chart.High)
				{
					return false;
				}
				for (const FRing& Ring : Chart.Coasts)
				{
					if (Inside(Ring, X, Y))
					{
						return false;
					}
				}
			}
		}
		return true;
	}

	void TheChart(const char* CoastsPath, const char* BerthsPath)
	{
		FChart Chart;
		if (!ReadChart(CoastsPath, Chart))
		{
			Check(false, std::string("the chart's coasts could not be read from ") + CoastsPath);
			return;
		}
		std::ofstream Written(BerthsPath);
		std::printf("\nThe chart: %zu seas, %zu coasts, table %.0f by %.0f. Six berths asked of each sea.\n", Chart.Seas.size(), Chart.Coasts.size(), Chart.Wide, Chart.High);
		std::printf("  %-24s %-9s %-22s %s\n", "sea", "berths", "first: room, from mid", "the rest: room/gap");
		double Slowest = 0.0;
		int Moved = 0, Fewer = 0;
		for (size_t Sea = 0; Sea < Chart.Seas.size(); ++Sea)
		{
			const FSpot Middle = Chart.Seas[Sea];
			const auto Room = [&Chart](double X, double Y) { return ChartRoom(Chart, X, Y); };
			const auto Mine = [&Chart, Sea, &Middle](double X, double Y)
			{
				const double Own = std::hypot(X - Middle.X, Y - Middle.Y);
				for (size_t Other = 0; Other < Chart.Seas.size(); ++Other)
				{
					if (Other != Sea && std::hypot(X - Chart.Seas[Other].X, Y - Chart.Seas[Other].Y) < Own)
					{
						return false;
					}
				}
				return true;
			};
			const auto Began = std::chrono::steady_clock::now();
			const std::vector<FBerth> Got = Berths(Middle, 6, static_cast<double>(Sea) * 2.4, Room, Mine);
			Slowest = std::max(Slowest, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Began).count());
			CheckBerths(Chart.Names[Sea], Middle, Got, Room, Mine);
			Check(Got[0].bFound, Chart.Names[Sea] + ": the chart's sea has a first berth");
			Check(Got.size() >= 2, Chart.Names[Sea] + ": and room for a second fleet");
			Check(Same(Got, Berths(Middle, 6, static_cast<double>(Sea) * 2.4, Room, Mine)), Chart.Names[Sea] + ": asked twice, the same berths");
			std::string Rest;
			for (size_t K = 0; K < Got.size(); ++K)
			{
				if (Got[K].bFound)
				{
					// (a little inside the room it says: the count steps by whole units)
					Check(AllWater(Chart, Got[K].At, std::floor(Got[K].Room) - 1.0), Chart.Names[Sea] + " berth " + std::to_string(K) + ": every place within its room is water, by a count of its own");
				}
				Written << Sea << ' ' << K << ' ' << Got[K].At.X << ' ' << Got[K].At.Y << ' ' << Got[K].Room << ' ' << Got[K].Gap << ' ' << (Got[K].bFound ? 1 : 0) << '\n';
				if (K > 0)
				{
					Rest += " " + std::to_string(static_cast<int>(Got[K].Room)) + "/" + std::to_string(static_cast<int>(Got[K].Gap));
				}
			}
			Moved += Apart(Got[0].At, Middle) > 1.0e-9 ? 1 : 0;
			Fewer += Got.size() < 6 ? 1 : 0;
			std::printf("  %-24s %-9zu %4.0f, %5.1f            %s\n", Chart.Names[Sea].c_str(), Got.size(), Got[0].Room, Apart(Got[0].At, Middle), Rest.c_str());
			// The Channel, where the fault was seen: the first berth stands 16 or more off every coast, Cotentin's with them.
			if (Chart.Names[Sea] == "English_Channel")
			{
				Check(ChartRoom(Chart, Middle.X, Middle.Y) < 16.0, "the Channel: its middle is under 16 off a coast (the fault: " + Text(ChartRoom(Chart, Middle.X, Middle.Y)) + ")");
				Check(Got[0].Room >= 16.0, "the Channel: its first berth has 16 or more of open water");
				Check(Got.size() >= 2 && Apart(Got[0].At, Got[1].At) >= 34.0, "the Channel: its second berth is 34 or more from the first");
			}
		}
		std::printf("  %d of %zu first berths moved off the sea's middle; %d seas gave fewer than six; the slowest sea took %.1f ms here (the game asks its terrain, which is quicker).\n",
			Moved, Chart.Seas.size(), Fewer, Slowest);
	}
}

int main(int Count, char** Words)
{
	MadeUpSeas();
	const int MadeUpChecks = Checks, MadeUpFaults = Faults;
	std::printf("Made-up seas: %d checks, %d faults\n", MadeUpChecks, MadeUpFaults);
	if (Count >= 3)
	{
		TheChart(Words[1], Words[2]);
	}
	else
	{
		Check(false, "usage: fleet_berths_test <coasts.txt> <berths.txt>");
	}
	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
