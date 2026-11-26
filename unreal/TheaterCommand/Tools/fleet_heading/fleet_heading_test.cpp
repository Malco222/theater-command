// Fleets that keep their heading on the campaign map, their cloud test (Tools/fleet_heading/run.sh): the game's own
// Campaign/FleetHeading.h, no engine.

#include "Campaign/FleetHeading.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace TheaterFleetHeading;

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

	bool Close(double A, double B, double Within = 1.0e-9) { return std::fabs(A - B) <= Within; }

	/** Two angles the same but for whole turns. */
	bool SameAngle(double A, double B, double Within = 1.0e-9) { return std::fabs(Wrap(A - B)) <= Within; }

	struct FPoint
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};

	/** A point turned about the upright, as the engine's FRotator(0, Yaw, 0) turns one. */
	FPoint Yawed(double Yaw, const FPoint& P)
	{
		FPoint Out;
		Turn(Yaw, P.X, P.Y, Out.X, Out.Y);
		Out.Z = P.Z;
		return Out;
	}

	/** A point leant about the piece's own Y axis: what the standard's tilt does to it (the sign does not matter here). */
	FPoint Leant(double Tilt, const FPoint& P)
	{
		const double C = std::cos(ToRadians(Tilt)), S = std::sin(ToRadians(Tilt));
		return FPoint{ P.X * C + P.Z * S, P.Y, -P.X * S + P.Z * C };
	}

	FPoint Add(const FPoint& A, const FPoint& B) { return FPoint{ A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
	double Apart(const FPoint& A, const FPoint& B) { return std::sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y) + (A.Z - B.Z) * (A.Z - B.Z)); }

	/**
	 * The old sail's helm, as AArmyPiece::Tick has it, copied here: corner to corner on one ease, her head brought round
	 * to her leg's heading by a share of what is left each frame, and (where she does not lie headed) round to the
	 * table over the last of her time. Where she comes to rest, and the fastest her head swung, in degrees a second.
	 */
	struct FSailed
	{
		double RestYaw = 0.0, FastestTurn = 0.0;
	};
	FSailed Sail(const std::vector<FPoint>& WorldRoute, double StartYaw, double TableYaw, bool bLiesHeaded, double Frame)
	{
		std::vector<double> Marks{ 0.0 };
		for (size_t K = 1; K < WorldRoute.size(); ++K)
		{
			Marks.push_back(Marks.back() + Apart(WorldRoute[K - 1], WorldRoute[K]));
		}
		const double MarchTime = std::clamp(1.1 + Marks.back() / 3800.0, 1.4, 3.6);
		FSailed Out;
		double Now = StartYaw;
		size_t Segment = 1;
		for (double Clock = Frame;; Clock += Frame)
		{
			const double Progress = std::clamp(Clock / MarchTime, 0.0, 1.0);
			const double Eased = Progress * Progress * (3.0 - 2.0 * Progress);
			const double Target = Eased * Marks.back();
			while (Segment < Marks.size() - 1 && Marks[Segment] < Target)
			{
				++Segment;
			}
			const double HeadX = WorldRoute[Segment].X - WorldRoute[Segment - 1].X, HeadY = WorldRoute[Segment].Y - WorldRoute[Segment - 1].Y;
			const bool bHerWay = HeadsHerWay(bLiesHeaded, Progress);
			double Want = bLiesHeaded ? Now : TableYaw;
			if (bHerWay && HeadX * HeadX + HeadY * HeadY > 1.0e-4)
			{
				Want = YawOfWorld(HeadX, HeadY);
			}
			const double Rate = bHerWay ? 5.0 : std::max(5.0, 3.0 / std::max(0.05, MarchTime * (1.0 - Progress)));
			const double Swing = Wrap(Want - Now) * std::min(1.0, Frame * Rate);
			Out.FastestTurn = std::max(Out.FastestTurn, std::fabs(Swing) / Frame);
			Now += Swing;
			if (Progress >= 1.0)
			{
				break;
			}
		}
		Out.RestYaw = bLiesHeaded ? Now : TableYaw; // (one that faces the table is set to it as she comes in)
		return Out;
	}

	/** A way over the map, east and south, as a route in the world (map units to the world's, ten to one). */
	std::vector<FPoint> WorldOf(const std::vector<FWay>& MapPoints)
	{
		std::vector<FPoint> Out;
		for (const FWay& P : MapPoints)
		{
			Out.push_back(FPoint{ -P.South * 10.0, P.East * 10.0, 0.0 });
		}
		return Out;
	}
}

int main()
{
	// ---- Angles.
	{
		bool bInRange = true, bSame = true, bOnce = true;
		for (double A = -1500.0; A <= 1500.0; A += 0.25)
		{
			const double W = Wrap(A);
			bInRange = bInRange && W > -180.0 && W <= 180.0;
			bSame = bSame && std::fabs(std::remainder(W - A, 360.0)) < 1.0e-9;
			bOnce = bOnce && Wrap(W) == W;
		}
		Check(bInRange, "a wrapped angle lies in (-180, 180]");
		Check(bSame, "and is the angle it was, but for whole turns");
		Check(bOnce, "and wrapping it again changes nothing");
		Check(Wrap(180.0) == 180.0 && Wrap(-180.0) == 180.0 && Wrap(540.0) == 180.0 && Wrap(0.0) == 0.0, "the half turn is +180, either way round");
	}

	// ---- The yaw of a way, the sail's own reckoning of it, and her bows and her wake at that yaw.
	{
		bool bAgree = true, bBowsAlong = true, bWakeBack = true, bBowsAsEngine = true;
		for (int Step = 0; Step < 720; ++Step)
		{
			const double Bearing = Step * 0.5, Long = 3.0 + (Step % 17);
			const double East = Long * std::cos(ToRadians(Bearing)), South = Long * std::sin(ToRadians(Bearing));
			const double Yaw = YawOfWay(East, South);
			// (the map's east is the world's +Y, its south the world's -X)
			const double WorldX = -South, WorldY = East;
			bAgree = bAgree && SameAngle(Yaw, YawOfWorld(WorldX, WorldY), 1.0e-9);
			double BowsX = 0.0, BowsY = 0.0;
			Bows(Yaw, BowsX, BowsY);
			bBowsAlong = bBowsAlong && Close(BowsX * WorldX + BowsY * WorldY, Long, 1.0e-9) && Close(BowsX * BowsX + BowsY * BowsY, 1.0, 1.0e-12);
			// (her wake lies straight astern: from her, it leads back toward where she came from)
			bWakeBack = bWakeBack && Close(-BowsX * WorldX - BowsY * WorldY, -Long, 1.0e-9);
			// (the engine turns her model's own +Y, its bows, by her yaw)
			const FPoint Turned = Yawed(Yaw, FPoint{ 0.0, 1.0, 0.0 });
			bBowsAsEngine = bBowsAsEngine && Close(Turned.X, BowsX, 1.0e-12) && Close(Turned.Y, BowsY, 1.0e-12);
		}
		Check(bAgree, "the yaw of a way over the map is the yaw the sail reckons for the same heading in the world");
		Check(bBowsAlong, "a fleet laid on the yaw of her way has her bows along it");
		Check(bWakeBack, "and her wake, straight astern, leads back the way she came");
		Check(bBowsAsEngine, "her bows are her model's +Y turned by her yaw");
		Check(Close(YawOfWay(1.0, 0.0), 0.0) && Close(YawOfWay(0.0, 1.0), 90.0) && Close(YawOfWay(0.0, -1.0), -90.0) && Close(std::fabs(YawOfWay(-1.0, 0.0)), 180.0),
			"east is yaw 0, south 90, north -90, west the half turn");
	}

	// ---- Her standard, turned back to the eye.
	{
		const FPoint Foot{ 3.84, -3.6, 1.2 }; // (as a fleet's pole stands: behind her middle and to the left)
		const FPoint OnCloth{ 0.4, 2.2, 7.5 }; // (a point of the colours, from the foot, in the standard's own space)
		bool bFaces = true, bFootStays = true, bClothStays = true, bRange = true;
		double Worst = 0.0;
		for (int EyeStep = -72; EyeStep <= 72; ++EyeStep)
		{
			for (int OwnStep = -144; OwnStep <= 144; ++OwnStep)
			{
				const double EyeYaw = EyeStep * 2.5, OwnYaw = OwnStep * 1.25 + 0.3;
				for (double Tilt : { 0.0, 18.0, 41.0 })
				{
					const double Turned = StandardTurn(EyeYaw, OwnYaw);
					bRange = bRange && Turned > -180.0 && Turned <= 180.0;
					bFaces = bFaces && SameAngle(OwnYaw + Turned, EyeYaw, 1.0e-9);
					// As she lies: the piece turned to her own yaw, the standard's foot and the standard turned back in it.
					const FPoint FootHeaded = Yawed(OwnYaw, Yawed(Turned, Foot));
					const FPoint ClothHeaded = Add(FootHeaded, Yawed(OwnYaw, Yawed(Turned, Leant(Tilt, OnCloth))));
					// As it was: the whole piece turned to the eye.
					const FPoint FootFaced = Yawed(EyeYaw, Foot);
					const FPoint ClothFaced = Add(FootFaced, Yawed(EyeYaw, Leant(Tilt, OnCloth)));
					Worst = std::max({ Worst, Apart(FootHeaded, FootFaced), Apart(ClothHeaded, ClothFaced) });
					bFootStays = bFootStays && Apart(FootHeaded, FootFaced) < 1.0e-9;
					bClothStays = bClothStays && Apart(ClothHeaded, ClothFaced) < 1.0e-9;
				}
			}
		}
		Check(bRange, "the standard's turn lies in (-180, 180]");
		Check(bFaces, "her own yaw and her standard's turn together are the eye's yaw");
		Check(bFootStays, "the foot of her standard stands where it stood when the whole piece faced the eye");
		Check(bClothStays, "and every point of it, leaning as it leans, is where it was");
		// Where she faces the table (in harbour, or the switch off) nothing is turned: to the last bit.
		bool bNone = true;
		for (double Yaw : { 0.0, 37.5, -142.25, 180.0, -179.999 })
		{
			double X = 0.0, Y = 0.0;
			Turn(StandardTurn(Yaw, Yaw), Foot.X, Foot.Y, X, Y);
			bNone = bNone && StandardTurn(Yaw, Yaw) == 0.0 && X == Foot.X && Y == Foot.Y;
		}
		Check(bNone, "a fleet on the table's own yaw has a standard turned by nothing, its foot exactly where it was built");
		std::printf("Her standard: over %d ways of lying and looking, the furthest any point of it stands from where it stood facing the eye is %.2e of the piece's units\n",
			145 * 289 * 3, Worst);
	}

	// ---- The old sail: who has her bows along her way, and to when.
	{
		Check(HeadsHerWay(false, 0.0) && HeadsHerWay(false, 0.5) && HeadsHerWay(false, 0.8399), "under the old sail a fleet that faces the table has her bows along her way for the first 0.84 of it");
		Check(!HeadsHerWay(false, 0.84) && !HeadsHerWay(false, 0.95) && !HeadsHerWay(false, 1.0), "and comes round to the table after that, as before");
		Check(HeadsHerWay(true, 0.0) && HeadsHerWay(true, 0.84) && HeadsHerWay(true, 0.99) && HeadsHerWay(true, 1.0), "one that lies headed has them along her way to the end");
	}

	// ---- The heading of a fleet not seen to sail in.
	{
		const FWay North{ 0.0, -40.0, true }, East{ 25.0, 0.0, true }, SouthWest{ -30.0, 30.0, true };
		const FWay Unknown{ 12.0, 5.0, false }, Short{ 0.6, 0.6, true }, None;
		Check(North.Leads() && East.Leads() && !Unknown.Leads() && !Short.Leads() && !None.Leads(), "a way leads where it is known and a map unit long or more");
		Check(Close(CameYaw(North, East, SouthWest, 7), -90.0), "found at a new place without sailing there, she lies the way she moved");
		Check(Close(CameYaw(None, East, SouthWest, 7), 0.0) && Close(CameYaw(Short, East, SouthWest, 7), 0.0), "not seen to move (or hardly), she lies out from her home port");
		Check(Close(CameYaw(None, Unknown, SouthWest, 7), 135.0) && Close(CameYaw(Unknown, Short, SouthWest, 7), 135.0), "her port off the map (or she lies on it), out of the sea her harbour is on");
		Check(Close(CameYaw(None, None, None, 7), YawOfId(7)) && Close(CameYaw(Unknown, Short, None, 7), YawOfId(7)), "nothing known of her way, a heading of her own");
		bool bOwn = true, bApart = true;
		for (int Id = -400; Id <= 5000; ++Id)
		{
			const double Yaw = YawOfId(Id);
			bOwn = bOwn && Yaw > -180.0 && Yaw <= 180.0 && Yaw == YawOfId(Id);
			bApart = bApart && std::fabs(Wrap(YawOfId(Id + 1) - Yaw)) > 30.0;
		}
		Check(bOwn, "a fleet's own heading is the same every time it is asked, and a wrapped angle");
		Check(bApart, "and the fleets numbered one after another lie more than thirty degrees apart");
		// She came north up the sea: her bows point north in the world (+X), and her wake leads south.
		double BowsX = 0.0, BowsY = 0.0;
		Bows(CameYaw(North, None, None, 1), BowsX, BowsY);
		Check(Close(BowsX, 1.0) && Close(BowsY, 0.0, 1.0e-12), "a fleet that came north lies with her bows to the north and her wake to the south");
	}

	// ---- The old sail's helm (copied): where she comes to rest.
	{
		const double Frame = 1.0 / 60.0;
		// A straight sail, from every heading she might lie on, at every length of sail: she comes to rest on her way.
		double WorstStraight = 0.0, FastestHeaded = 0.0, FastestFaced = 0.0;
		bool bFacedRests = true;
		for (double Long : { 20.0, 60.0, 140.0, 400.0, 1200.0 })
		{
			for (int Step = 0; Step < 36; ++Step)
			{
				const double Bearing = Step * 10.0 + 3.0;
				const std::vector<FPoint> Route = WorldOf({ FWay{ 0.0, 0.0, true }, FWay{ Long * std::cos(ToRadians(Bearing)), Long * std::sin(ToRadians(Bearing)), true } });
				for (double StartYaw : { 0.0, 90.0, -135.0, 180.0 })
				{
					const FSailed Headed = Sail(Route, StartYaw, 30.0, true, Frame);
					WorstStraight = std::max(WorstStraight, std::fabs(Wrap(Headed.RestYaw - Bearing)));
					FastestHeaded = std::max(FastestHeaded, Headed.FastestTurn);
					const FSailed Faced = Sail(Route, 30.0, 30.0, false, Frame);
					bFacedRests = bFacedRests && Faced.RestYaw == 30.0;
					FastestFaced = std::max(FastestFaced, Faced.FastestTurn);
				}
			}
		}
		Check(WorstStraight < 0.5, "on a straight sail she comes to rest within half a degree of her way, whatever heading she left on");
		Check(bFacedRests, "one that faces the table comes to rest on the table's yaw, as before");
		Check(FastestHeaded <= 5.0 * 180.0 + 1.0e-6, "her head never swings faster than the old sail's helm lets it");
		// A way with one corner: how near her last leg she rests, by the share of her way the last leg is and the corner's angle.
		std::printf("The old sail, one corner, the shortest sail (1.4 s) and the longest (3.6 s): degrees off her last leg at rest\n");
		std::printf("  last leg's share   corner 45    corner 90    corner 135\n");
		double WorstFromHalf = 0.0, WorstFromThird = 0.0;
		bool bWithinCorner = true;
		for (double Share : { 0.05, 0.1, 0.2, 0.3333, 0.5, 0.7 })
		{
			std::printf("  %14.2f", Share);
			for (double Corner : { 45.0, 90.0, 135.0 })
			{
				double Off[2] = { 0.0, 0.0 };
				int Which = 0;
				for (double Long : { 60.0, 1200.0 })
				{
					const double First = Long * (1.0 - Share), Last = Long * Share;
					const std::vector<FPoint> Route = WorldOf({ FWay{ 0.0, 0.0, true }, FWay{ First, 0.0, true },
						FWay{ First + Last * std::cos(ToRadians(Corner)), Last * std::sin(ToRadians(Corner)), true } });
					const FSailed Headed = Sail(Route, 0.0, 30.0, true, Frame);
					Off[Which] = Wrap(Corner - Headed.RestYaw);
					// (what is left of her turn lies inside the corner: her wake at rest lies between her last two legs, never outside them)
					bWithinCorner = bWithinCorner && Off[Which] > -1.0e-6 && Off[Which] < Corner + 1.0e-6;
					if (Share >= 0.5)
					{
						WorstFromHalf = std::max(WorstFromHalf, std::fabs(Off[Which]));
					}
					if (Share >= 0.33)
					{
						WorstFromThird = std::max(WorstFromThird, std::fabs(Off[Which]));
					}
					++Which;
				}
				std::printf("   %4.1f / %4.1f", Off[0], Off[1]);
			}
			std::printf("\n");
		}
		Check(bWithinCorner, "after a corner her heading at rest lies between her last two legs: her wake never points outside the way she came");
		// (The helm brings her round by a share of what is left each frame, as it always has: on the shortest sail a
		// corner close to her journey's end leaves some of the turn unmade, and she lies part-way round it. The figures
		// above are what the Mac should see; a ship's course, tc.Map.ShipWay, has no corners to leave unmade.)
		Check(WorstFromHalf < 4.0, "where her last leg is half her way or more she rests within four degrees of it");
		Check(WorstFromThird < 8.0, "and within eight where it is a third");
		std::printf("Straight sails: at most %.3f degrees off her way at rest; her head swings at most %.0f degrees a second headed, %.0f facing the table (the turn round to it as she comes in)\n",
			WorstStraight, FastestHeaded, FastestFaced);
	}

	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
