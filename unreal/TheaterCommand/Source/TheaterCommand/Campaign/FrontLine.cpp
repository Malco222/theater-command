#include "Campaign/FrontLine.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace TheaterFront
{
	namespace
	{
		// (File-local helpers, each named for this file: the module is built one file at a time, and a helper of the
		// same name in another file must never be the one a reader finds.)
		double FrontDistance(const FPoint& One, const FPoint& Other)
		{
			return std::hypot(Other.X - One.X, Other.Y - One.Y);
		}

		FPoint FrontLerp(const FPoint& One, const FPoint& Other, double Share)
		{
			return FPoint{ One.X + (Other.X - One.X) * Share, One.Y + (Other.Y - One.Y) * Share };
		}

		/** A unit vector along From to To, or nothing (0, 0) where the two are one point. */
		FPoint FrontDirection(const FPoint& From, const FPoint& To)
		{
			const double Length = FrontDistance(From, To);
			if (Length < 1e-12)
			{
				return FPoint{};
			}
			return FPoint{ (To.X - From.X) / Length, (To.Y - From.Y) / Length };
		}

		/** How far a point stands from the stretch From to To. */
		double FrontOffLine(const FPoint& Point, const FPoint& From, const FPoint& To)
		{
			const double DX = To.X - From.X, DY = To.Y - From.Y;
			const double Square = DX * DX + DY * DY;
			if (Square < 1e-18)
			{
				return FrontDistance(Point, From);
			}
			const double Share = std::clamp(((Point.X - From.X) * DX + (Point.Y - From.Y) * DY) / Square, 0.0, 1.0);
			return FrontDistance(Point, FPoint{ From.X + DX * Share, From.Y + DY * Share });
		}

		/** The nearest point to Point on the stretch From to To. */
		FPoint FrontNearestOn(const FPoint& Point, const FPoint& From, const FPoint& To)
		{
			const double DX = To.X - From.X, DY = To.Y - From.Y;
			const double Square = DX * DX + DY * DY;
			if (Square < 1e-18)
			{
				return From;
			}
			const double Share = std::clamp(((Point.X - From.X) * DX + (Point.Y - From.Y) * DY) / Square, 0.0, 1.0);
			return FPoint{ From.X + DX * Share, From.Y + DY * Share };
		}

		long long FrontCellKey(long long CellX, long long CellY)
		{
			return (CellX << 32) ^ (CellY & 0xffffffffLL);
		}

		float FrontSmoothStep(float From, float To, float Value)
		{
			if (Value < From)
			{
				return 0.0f;
			}
			if (Value >= To)
			{
				return 1.0f;
			}
			const float Share = (Value - From) / (To - From);
			return Share * Share * (3.0f - 2.0f * Share);
		}

		// A piece's end, as a number: the piece times two, and 0 for its A end or 1 for its B.
		int FrontPieceOf(int End)
		{
			return End / 2;
		}

		bool FrontIsB(int End)
		{
			return (End % 2) == 1;
		}

		/** Which side the player's enemy is on, going along the piece away from this end: +1 left, -1 right. */
		int FrontEnemyLeaving(const std::vector<FFrontPiece>& Front, int End)
		{
			const FFrontPiece& Piece = Front[static_cast<size_t>(FrontPieceOf(End))];
			return FrontIsB(End) ? -Piece.Enemy : Piece.Enemy;
		}

		/** Two pieces meeting at a point can be one line through it: the same war, and the player's enemy on the same side all the way. */
		bool FrontGoesOn(const std::vector<FFrontPiece>& Front, int Arriving, int Leaving)
		{
			const FFrontPiece& In = Front[static_cast<size_t>(FrontPieceOf(Arriving))];
			const FFrontPiece& Out = Front[static_cast<size_t>(FrontPieceOf(Leaving))];
			if (In.Kind != Out.Kind)
			{
				return false;
			}
			// (arriving at a point by one end is leaving it the other way round: the side flips)
			return In.Kind == EKind::Theirs || -FrontEnemyLeaving(Front, Arriving) == FrontEnemyLeaving(Front, Leaving);
		}

		/** Takes out the point at Index from a line, with what is kept of it. */
		void FrontErase(FChain& Chain, size_t Index)
		{
			Chain.Points.erase(Chain.Points.begin() + static_cast<std::ptrdiff_t>(Index));
			Chain.Along.erase(Chain.Along.begin() + static_cast<std::ptrdiff_t>(Index));
			Chain.Keep.erase(Chain.Keep.begin() + static_cast<std::ptrdiff_t>(Index));
		}

		/** The Along of the point after Index, going on round a closed line past its start. */
		double FrontAlongAfter(const FChain& Chain, size_t Index)
		{
			return Index + 1 < Chain.Along.size() ? Chain.Along[Index + 1] : Chain.Along[0] + Chain.Span;
		}

		/** The Along of the point before Index, going back round a closed line past its start. */
		double FrontAlongBefore(const FChain& Chain, size_t Index)
		{
			return Index > 0 ? Chain.Along[Index - 1] : Chain.Along.back() - Chain.Span;
		}
	}

	void FSides::Reset(int ProvinceCount, int HolderCount)
	{
		Holders = std::max(HolderCount, 0);
		HolderOf.assign(static_cast<size_t>(std::max(ProvinceCount, 0)), -1);
		Foe.assign(static_cast<size_t>(Holders), 0);
		War.assign(static_cast<size_t>(Holders) * static_cast<size_t>(Holders), 0);
	}

	void FSides::SetWar(int One, int Other)
	{
		if (One < 0 || Other < 0 || One >= Holders || Other >= Holders || One == Other)
		{
			return;
		}
		War[static_cast<size_t>(One) * static_cast<size_t>(Holders) + static_cast<size_t>(Other)] = 1;
		War[static_cast<size_t>(Other) * static_cast<size_t>(Holders) + static_cast<size_t>(One)] = 1;
	}

	bool FSides::AtWar(int One, int Other) const
	{
		if (One < 0 || Other < 0 || One >= Holders || Other >= Holders)
		{
			return false;
		}
		return War[static_cast<size_t>(One) * static_cast<size_t>(Holders) + static_cast<size_t>(Other)] != 0;
	}

	void PickFront(const std::vector<FBorderPiece>& Pieces, const FSides& Sides, std::vector<FFrontPiece>& Out)
	{
		Out.clear();
		const int Provinces = static_cast<int>(Sides.HolderOf.size());
		auto IsFoe = [&Sides](int Holder)
		{
			return Holder >= 0 && Holder < static_cast<int>(Sides.Foe.size()) && Sides.Foe[static_cast<size_t>(Holder)] != 0;
		};
		for (const FBorderPiece& Piece : Pieces)
		{
			if (Piece.Here < 0 || Piece.There < 0 || Piece.Here >= Provinces || Piece.There >= Provinces)
			{
				continue;
			}
			const int HereHolder = Sides.HolderOf[static_cast<size_t>(Piece.Here)], ThereHolder = Sides.HolderOf[static_cast<size_t>(Piece.There)];
			if (HereHolder < 0 || ThereHolder < 0 || HereHolder == ThereHolder || !Sides.AtWar(HereHolder, ThereHolder))
			{
				continue;
			}
			FFrontPiece& Into = Out.emplace_back();
			Into.A = Piece.A;
			Into.B = Piece.B;
			Into.Low = std::min(Piece.Here, Piece.There);
			Into.High = std::max(Piece.Here, Piece.There);
			const bool bHereFoe = IsFoe(HereHolder), bThereFoe = IsFoe(ThereHolder);
			if (bHereFoe != bThereFoe)
			{
				// (the player's war: one side is his enemy and the other his own or a friend's)
				Into.Kind = EKind::Mine;
				const double LeftX = -(Piece.B.Y - Piece.A.Y), LeftY = Piece.B.X - Piece.A.X;
				const bool bThereLeft = Piece.Side.X * LeftX + Piece.Side.Y * LeftY > 0.0;
				Into.Enemy = (bThereFoe ? bThereLeft : !bThereLeft) ? 1 : -1;
			}
		}
	}

	void ChainFront(const std::vector<FFrontPiece>& Front, const FRules& Rules, FScratch& Work, std::vector<FChain>& Chains)
	{
		Chains.clear();
		const int PieceCount = static_cast<int>(Front.size());
		const size_t Ends = static_cast<size_t>(PieceCount) * 2;

		// The ends welded into points: closer than Rules.Weld, one point.
		Work.Nodes.clear();
		Work.NodeA.assign(static_cast<size_t>(PieceCount), -1);
		Work.NodeB.assign(static_cast<size_t>(PieceCount), -1);
		Work.Remap.clear(); // (here: the next point in the same cell of the welding grid)
		const double Weld = std::max(Rules.Weld, 1e-6);
		std::unordered_map<long long, int> FirstInCell;
		FirstInCell.reserve(Ends);
		auto PointFor = [&Work, &FirstInCell, Weld](const FPoint& At)
		{
			const long long CellX = static_cast<long long>(std::floor(At.X / Weld)), CellY = static_cast<long long>(std::floor(At.Y / Weld));
			for (long long DX = -1; DX <= 1; ++DX)
			{
				for (long long DY = -1; DY <= 1; ++DY)
				{
					const auto Found = FirstInCell.find(FrontCellKey(CellX + DX, CellY + DY));
					for (int Node = Found == FirstInCell.end() ? -1 : Found->second; Node >= 0; Node = Work.Remap[static_cast<size_t>(Node)])
					{
						if (FrontDistance(Work.Nodes[static_cast<size_t>(Node)], At) <= Weld)
						{
							return Node;
						}
					}
				}
			}
			const int Made = static_cast<int>(Work.Nodes.size());
			Work.Nodes.push_back(At);
			int& Head = FirstInCell.try_emplace(FrontCellKey(CellX, CellY), -1).first->second;
			Work.Remap.push_back(Head);
			Head = Made;
			return Made;
		};
		for (int Piece = 0; Piece < PieceCount; ++Piece)
		{
			Work.NodeA[static_cast<size_t>(Piece)] = PointFor(Front[static_cast<size_t>(Piece)].A);
			Work.NodeB[static_cast<size_t>(Piece)] = PointFor(Front[static_cast<size_t>(Piece)].B);
		}
		const int NodeCount = static_cast<int>(Work.Nodes.size());

		// Loose ends closer than Rules.Gap are joined: the stretches the table left out for being under 0.05 long, and
		// borders of the wider chart that do not quite meet. Nearest first, and only where the line can go on through.
		Work.Degree.assign(static_cast<size_t>(NodeCount), 0);
		for (int Piece = 0; Piece < PieceCount; ++Piece)
		{
			if (Work.NodeA[static_cast<size_t>(Piece)] != Work.NodeB[static_cast<size_t>(Piece)])
			{
				++Work.Degree[static_cast<size_t>(Work.NodeA[static_cast<size_t>(Piece)])];
				++Work.Degree[static_cast<size_t>(Work.NodeB[static_cast<size_t>(Piece)])];
			}
		}
		Work.Loose.assign(static_cast<size_t>(NodeCount), -1); // (by point: the end of the one piece at a loose end)
		for (int Piece = 0; Piece < PieceCount; ++Piece)
		{
			const int AtA = Work.NodeA[static_cast<size_t>(Piece)], AtB = Work.NodeB[static_cast<size_t>(Piece)];
			if (AtA == AtB)
			{
				continue;
			}
			if (Work.Degree[static_cast<size_t>(AtA)] == 1)
			{
				Work.Loose[static_cast<size_t>(AtA)] = Piece * 2;
			}
			if (Work.Degree[static_cast<size_t>(AtB)] == 1)
			{
				Work.Loose[static_cast<size_t>(AtB)] = Piece * 2 + 1;
			}
		}
		Work.Remap.resize(static_cast<size_t>(NodeCount));
		for (int Node = 0; Node < NodeCount; ++Node)
		{
			Work.Remap[static_cast<size_t>(Node)] = Node;
		}
		if (Rules.Gap > Weld)
		{
			Work.Walk.clear(); // (here: the loose points)
			for (int Node = 0; Node < NodeCount; ++Node)
			{
				if (Work.Loose[static_cast<size_t>(Node)] >= 0)
				{
					Work.Walk.push_back(Node);
				}
			}
			std::vector<std::pair<double, std::pair<int, int>>> Near;
			for (size_t One = 0; One < Work.Walk.size(); ++One)
			{
				for (size_t Other = One + 1; Other < Work.Walk.size(); ++Other)
				{
					const int NodeOne = Work.Walk[One], NodeOther = Work.Walk[Other];
					const int EndOne = Work.Loose[static_cast<size_t>(NodeOne)], EndOther = Work.Loose[static_cast<size_t>(NodeOther)];
					const double Apart = FrontDistance(Work.Nodes[static_cast<size_t>(NodeOne)], Work.Nodes[static_cast<size_t>(NodeOther)]);
					if (Apart <= Rules.Gap && FrontPieceOf(EndOne) != FrontPieceOf(EndOther) && FrontGoesOn(Front, EndOne, EndOther))
					{
						Near.push_back({ Apart, { NodeOne, NodeOther } });
					}
				}
			}
			std::sort(Near.begin(), Near.end());
			for (const auto& Pair : Near)
			{
				const int NodeOne = Pair.second.first, NodeOther = Pair.second.second;
				if (Work.Remap[static_cast<size_t>(NodeOne)] == NodeOne && Work.Remap[static_cast<size_t>(NodeOther)] == NodeOther && Work.Loose[static_cast<size_t>(NodeOne)] >= 0 && Work.Loose[static_cast<size_t>(NodeOther)] >= 0)
				{
					Work.Remap[static_cast<size_t>(NodeOther)] = NodeOne;
					Work.Loose[static_cast<size_t>(NodeOne)] = -1;
					Work.Loose[static_cast<size_t>(NodeOther)] = -1;
				}
			}
			for (int Piece = 0; Piece < PieceCount; ++Piece)
			{
				Work.NodeA[static_cast<size_t>(Piece)] = Work.Remap[static_cast<size_t>(Work.NodeA[static_cast<size_t>(Piece)])];
				Work.NodeB[static_cast<size_t>(Piece)] = Work.Remap[static_cast<size_t>(Work.NodeB[static_cast<size_t>(Piece)])];
			}
		}

		// Which pieces meet at each point (a piece with both ends on one point is left out: it has no length).
		Work.Degree.assign(static_cast<size_t>(NodeCount), 0);
		for (int Piece = 0; Piece < PieceCount; ++Piece)
		{
			if (Work.NodeA[static_cast<size_t>(Piece)] != Work.NodeB[static_cast<size_t>(Piece)])
			{
				++Work.Degree[static_cast<size_t>(Work.NodeA[static_cast<size_t>(Piece)])];
				++Work.Degree[static_cast<size_t>(Work.NodeB[static_cast<size_t>(Piece)])];
			}
		}
		Work.Offsets.assign(static_cast<size_t>(NodeCount) + 1, 0);
		for (int Node = 0; Node < NodeCount; ++Node)
		{
			Work.Offsets[static_cast<size_t>(Node) + 1] = Work.Offsets[static_cast<size_t>(Node)] + Work.Degree[static_cast<size_t>(Node)];
		}
		Work.Incident.assign(static_cast<size_t>(Work.Offsets[static_cast<size_t>(NodeCount)]), -1);
		Work.Through.assign(static_cast<size_t>(NodeCount), 0); // (here: how many have been put at each point so far)
		for (int Piece = 0; Piece < PieceCount; ++Piece)
		{
			const int AtA = Work.NodeA[static_cast<size_t>(Piece)], AtB = Work.NodeB[static_cast<size_t>(Piece)];
			if (AtA == AtB)
			{
				continue;
			}
			Work.Incident[static_cast<size_t>(Work.Offsets[static_cast<size_t>(AtA)] + Work.Through[static_cast<size_t>(AtA)]++)] = Piece * 2;
			Work.Incident[static_cast<size_t>(Work.Offsets[static_cast<size_t>(AtB)] + Work.Through[static_cast<size_t>(AtB)]++)] = Piece * 2 + 1;
		}

		// Where the line goes on through each point: by the other piece where there are two that can; where three or
		// more meet, by the two that run on straightest, if they turn by less than about a right angle.
		auto Leaving = [&Front, &Work](int End)
		{
			const int Piece = FrontPieceOf(End);
			const FPoint& Here = Work.Nodes[static_cast<size_t>(FrontIsB(End) ? Work.NodeB[static_cast<size_t>(Piece)] : Work.NodeA[static_cast<size_t>(Piece)])];
			const FPoint& There = FrontIsB(End) ? Front[static_cast<size_t>(Piece)].A : Front[static_cast<size_t>(Piece)].B;
			return FrontDirection(Here, There);
		};
		Work.Partner.assign(Ends, -1);
		std::vector<std::pair<double, std::pair<int, int>>> Turns;
		for (int Node = 0; Node < NodeCount; ++Node)
		{
			const int First = Work.Offsets[static_cast<size_t>(Node)], Count = Work.Degree[static_cast<size_t>(Node)];
			if (Count == 2)
			{
				const int One = Work.Incident[static_cast<size_t>(First)], Other = Work.Incident[static_cast<size_t>(First) + 1];
				if (FrontGoesOn(Front, One, Other))
				{
					Work.Partner[static_cast<size_t>(One)] = Other;
					Work.Partner[static_cast<size_t>(Other)] = One;
				}
				continue;
			}
			if (Count < 3)
			{
				continue;
			}
			Turns.clear();
			for (int One = 0; One < Count; ++One)
			{
				for (int Other = One + 1; Other < Count; ++Other)
				{
					const int EndOne = Work.Incident[static_cast<size_t>(First + One)], EndOther = Work.Incident[static_cast<size_t>(First + Other)];
					if (!FrontGoesOn(Front, EndOne, EndOther))
					{
						continue;
					}
					const FPoint DirOne = Leaving(EndOne), DirOther = Leaving(EndOther);
					const double Straightness = -(DirOne.X * DirOther.X + DirOne.Y * DirOther.Y);
					if (Straightness > Rules.Straight)
					{
						Turns.push_back({ -Straightness, { EndOne, EndOther } });
					}
				}
			}
			std::sort(Turns.begin(), Turns.end());
			for (const auto& Turn : Turns)
			{
				const int EndOne = Turn.second.first, EndOther = Turn.second.second;
				if (Work.Partner[static_cast<size_t>(EndOne)] < 0 && Work.Partner[static_cast<size_t>(EndOther)] < 0)
				{
					Work.Partner[static_cast<size_t>(EndOne)] = EndOther;
					Work.Partner[static_cast<size_t>(EndOther)] = EndOne;
				}
			}
		}

		// The lines walked out, end to end; then whatever is left is closed on itself (an enclave, or an island's front
		// all round).
		Work.Used.assign(static_cast<size_t>(PieceCount), 0);
		Work.Through.assign(static_cast<size_t>(NodeCount), -1); // (by point where three or more meet: the line that runs on through it)
		auto NodeAt = [&Work](int End)
		{
			const int Piece = FrontPieceOf(End);
			return FrontIsB(End) ? Work.NodeB[static_cast<size_t>(Piece)] : Work.NodeA[static_cast<size_t>(Piece)];
		};
		auto Make = [&](bool bClosed)
		{
			// Work.Walk holds the ends the line set out from, piece by piece.
			const int Index = static_cast<int>(Chains.size());
			FChain& Chain = Chains.emplace_back();
			Chain.bClosed = bClosed;
			Chain.Kind = Front[static_cast<size_t>(FrontPieceOf(Work.Walk[0]))].Kind;
			if (Chain.Kind == EKind::Mine && FrontEnemyLeaving(Front, Work.Walk[0]) < 0)
			{
				// (the player's lines are turned so that his enemy is to the left: their teeth are on that side)
				std::reverse(Work.Walk.begin(), Work.Walk.end());
				for (int& End : Work.Walk)
				{
					End ^= 1;
				}
			}
			if (bClosed && Work.Walk.size() > 1)
			{
				// (round a closed line, start where one pair's border meets the next, so that no run is cut in two)
				for (size_t Step = 0; Step < Work.Walk.size(); ++Step)
				{
					const FFrontPiece& Now = Front[static_cast<size_t>(FrontPieceOf(Work.Walk[Step]))];
					const FFrontPiece& Before = Front[static_cast<size_t>(FrontPieceOf(Work.Walk[(Step + Work.Walk.size() - 1) % Work.Walk.size()]))];
					if (Now.Low != Before.Low || Now.High != Before.High)
					{
						std::rotate(Work.Walk.begin(), Work.Walk.begin() + static_cast<std::ptrdiff_t>(Step), Work.Walk.end());
						break;
					}
				}
			}
			const size_t Steps = Work.Walk.size();
			Chain.Points.reserve(Steps + 1);
			Chain.Along.reserve(Steps + 1);
			Chain.Keep.reserve(Steps + 1);
			Chain.Points.push_back(Work.Nodes[static_cast<size_t>(NodeAt(Work.Walk[0]))]);
			Chain.Keep.push_back(bClosed ? 1 : 2);
			Chain.Along.push_back(0.0);
			// (first how long each run is, then where each point stands on its run)
			std::vector<double>& Lengths = Chain.RunLength;
			std::vector<int>& RunOf = Work.RunOf;
			RunOf.assign(Steps, 0);
			for (size_t Step = 0; Step < Steps; ++Step)
			{
				const FFrontPiece& Now = Front[static_cast<size_t>(FrontPieceOf(Work.Walk[Step]))];
				if (Step == 0 || Now.Low != Front[static_cast<size_t>(FrontPieceOf(Work.Walk[Step - 1]))].Low || Now.High != Front[static_cast<size_t>(FrontPieceOf(Work.Walk[Step - 1]))].High)
				{
					Lengths.push_back(0.0);
				}
				RunOf[Step] = static_cast<int>(Lengths.size()) - 1;
				const FPoint& From = Work.Nodes[static_cast<size_t>(NodeAt(Work.Walk[Step]))];
				const FPoint& To = Work.Nodes[static_cast<size_t>(NodeAt(Work.Walk[Step] ^ 1))];
				Lengths.back() += FrontDistance(From, To);
			}
			double Gone = 0.0;
			for (size_t Step = 0; Step < Steps; ++Step)
			{
				const int Run = RunOf[Step];
				const bool bRunEnds = Step + 1 == Steps || RunOf[Step + 1] != Run;
				const FPoint& From = Work.Nodes[static_cast<size_t>(NodeAt(Work.Walk[Step]))];
				const FPoint& To = Work.Nodes[static_cast<size_t>(NodeAt(Work.Walk[Step] ^ 1))];
				Gone += FrontDistance(From, To);
				const double RunLength = Lengths[static_cast<size_t>(Run)];
				const double Share = bRunEnds || RunLength <= 0.0 ? 1.0 : std::min(Gone / RunLength, 1.0);
				if (bRunEnds)
				{
					Gone = 0.0;
				}
				if (Step + 1 == Steps && bClosed)
				{
					break; // (round and back to the first point, which is there already)
				}
				Chain.Points.push_back(To);
				Chain.Along.push_back(static_cast<double>(Run) + Share);
				Chain.Keep.push_back(Step + 1 == Steps ? 2 : (bRunEnds ? 1 : 0));
			}
			Chain.Span = static_cast<double>(Lengths.size());
			if (!bClosed)
			{
				Chain.StartNode = NodeAt(Work.Walk.front());
				Chain.EndNode = NodeAt(Work.Walk.back() ^ 1);
			}
			// (the points it ran on through where three or more lines meet: an end stopping there is brought onto it)
			for (size_t Step = 0; Step + 1 < Steps || (bClosed && Step < Steps); ++Step)
			{
				const int Node = NodeAt(Work.Walk[Step] ^ 1);
				if (Work.Offsets[static_cast<size_t>(Node) + 1] - Work.Offsets[static_cast<size_t>(Node)] >= 3 && Work.Through[static_cast<size_t>(Node)] < 0)
				{
					Work.Through[static_cast<size_t>(Node)] = Index;
				}
			}
		};
		auto WalkFrom = [&](int Start)
		{
			Work.Walk.clear();
			int End = Start;
			while (true)
			{
				const int Piece = FrontPieceOf(End);
				Work.Used[static_cast<size_t>(Piece)] = 1;
				Work.Walk.push_back(End);
				const int Next = Work.Partner[static_cast<size_t>(End ^ 1)];
				if (Next < 0 || Work.Used[static_cast<size_t>(FrontPieceOf(Next))])
				{
					return Next == Start;
				}
				End = Next;
			}
		};
		for (int Node = 0; Node < NodeCount; ++Node)
		{
			for (int Slot = Work.Offsets[static_cast<size_t>(Node)]; Slot < Work.Offsets[static_cast<size_t>(Node) + 1]; ++Slot)
			{
				const int End = Work.Incident[static_cast<size_t>(Slot)];
				if (Work.Partner[static_cast<size_t>(End)] < 0 && !Work.Used[static_cast<size_t>(FrontPieceOf(End))])
				{
					WalkFrom(End);
					Make(false);
				}
			}
		}
		for (int Piece = 0; Piece < PieceCount; ++Piece)
		{
			if (!Work.Used[static_cast<size_t>(Piece)] && Work.NodeA[static_cast<size_t>(Piece)] != Work.NodeB[static_cast<size_t>(Piece)])
			{
				const bool bRound = WalkFrom(Piece * 2);
				Make(bRound);
			}
		}

		// An end that stops at a point another line runs on through is brought onto that line (see MeetLines).
		for (size_t Index = 0; Index < Chains.size(); ++Index)
		{
			FChain& Chain = Chains[Index];
			if (Chain.bClosed)
			{
				continue;
			}
			const int OntoStart = Work.Through[static_cast<size_t>(Chain.StartNode)], OntoEnd = Work.Through[static_cast<size_t>(Chain.EndNode)];
			Chain.StartOnto = OntoStart != static_cast<int>(Index) ? OntoStart : -1;
			Chain.EndOnto = OntoEnd != static_cast<int>(Index) ? OntoEnd : -1;
		}
	}

	void Despike(FChain& Chain)
	{
		for (int Pass = 0; Pass < 4; ++Pass)
		{
			bool bCut = false;
			for (size_t Index = 0; Index < Chain.Points.size(); ++Index)
			{
				const size_t Count = Chain.Points.size();
				if (Count <= (Chain.bClosed ? 4u : 2u))
				{
					return;
				}
				if (Chain.Keep[Index] >= 2 || (!Chain.bClosed && (Index == 0 || Index + 1 == Count)))
				{
					continue;
				}
				const FPoint& Before = Chain.Points[(Index + Count - 1) % Count];
				const FPoint& Here = Chain.Points[Index];
				const FPoint& After = Chain.Points[(Index + 1) % Count];
				const double InX = Here.X - Before.X, InY = Here.Y - Before.Y, OnX = After.X - Here.X, OnY = After.Y - Here.Y;
				const double LongIn = std::hypot(InX, InY), LongOn = std::hypot(OnX, OnY);
				if (LongIn > 1e-6 && LongOn > 1e-6 && LongIn < 2.6 && LongOn < 2.6 && (InX * OnX + InY * OnY) / (LongIn * LongOn) < -0.4)
				{
					FrontErase(Chain, Index);
					--Index;
					bCut = true;
				}
			}
			if (!bCut)
			{
				return;
			}
		}
	}

	void Thin(FChain& Chain, double Tolerance, FScratch& Work)
	{
		const size_t Count = Chain.Points.size();
		if (Count < 3)
		{
			return;
		}
		Work.Keep.assign(Count, 0);
		size_t Anchors = 0;
		for (size_t Index = 0; Index < Count; ++Index)
		{
			if (Chain.Keep[Index] >= 1 || (!Chain.bClosed && (Index == 0 || Index + 1 == Count)))
			{
				Work.Keep[Index] = 1;
				++Anchors;
			}
		}
		if (Chain.bClosed && Anchors < 2)
		{
			// (a ring with nothing to hold it: held at its first point and the point furthest from it)
			size_t Furthest = 1;
			for (size_t Index = 1; Index < Count; ++Index)
			{
				if (FrontDistance(Chain.Points[Index], Chain.Points[0]) > FrontDistance(Chain.Points[Furthest], Chain.Points[0]))
				{
					Furthest = Index;
				}
			}
			Work.Keep[0] = 1;
			Work.Keep[Furthest] = 1;
		}
		// Between each two held points, the point furthest off the straight line between them is held too, while any
		// stands further off than Tolerance. (Indexes past the last go on round a closed line.)
		auto At = [&Chain, Count](size_t Index) -> const FPoint&
		{
			return Chain.Points[Index % Count];
		};
		std::vector<int>& Stack = Work.Walk;
		size_t First = 0;
		while (First < Count && !Work.Keep[First])
		{
			++First;
		}
		const size_t Last = Chain.bClosed ? First + Count : Count - 1;
		size_t From = First;
		while (From < Last)
		{
			size_t To = From + 1;
			while (To < Last && !Work.Keep[To % Count])
			{
				++To;
			}
			Stack.clear();
			Stack.push_back(static_cast<int>(From));
			Stack.push_back(static_cast<int>(To));
			while (!Stack.empty())
			{
				const size_t Hi = static_cast<size_t>(Stack.back());
				Stack.pop_back();
				const size_t Lo = static_cast<size_t>(Stack.back());
				Stack.pop_back();
				double Worst = -1.0;
				size_t WorstAt = Lo;
				for (size_t Index = Lo + 1; Index < Hi; ++Index)
				{
					const double Off = FrontOffLine(At(Index), At(Lo), At(Hi));
					if (Off > Worst)
					{
						Worst = Off;
						WorstAt = Index;
					}
				}
				if (Worst > Tolerance)
				{
					Work.Keep[WorstAt % Count] = 1;
					Stack.push_back(static_cast<int>(Lo));
					Stack.push_back(static_cast<int>(WorstAt));
					Stack.push_back(static_cast<int>(WorstAt));
					Stack.push_back(static_cast<int>(Hi));
				}
			}
			From = To;
		}
		size_t Into = 0;
		for (size_t Index = 0; Index < Count; ++Index)
		{
			if (Work.Keep[Index])
			{
				Chain.Points[Into] = Chain.Points[Index];
				Chain.Along[Into] = Chain.Along[Index];
				Chain.Keep[Into] = Chain.Keep[Index];
				++Into;
			}
		}
		Chain.Points.resize(Into);
		Chain.Along.resize(Into);
		Chain.Keep.resize(Into);
	}

	void Round(FChain& Chain, int Passes, double Cut, FScratch& Work)
	{
		for (int Pass = 0; Pass < Passes; ++Pass)
		{
			const size_t Count = Chain.Points.size();
			if (Count < 3)
			{
				return;
			}
			Work.Points.clear();
			Work.Along.clear();
			Work.Keep.clear();
			for (size_t Index = 0; Index < Count; ++Index)
			{
				const bool bEnd = !Chain.bClosed && (Index == 0 || Index + 1 == Count);
				if (bEnd || Chain.Keep[Index] >= 2)
				{
					Work.Points.push_back(Chain.Points[Index]);
					Work.Along.push_back(Chain.Along[Index]);
					Work.Keep.push_back(Chain.Keep[Index]);
					continue;
				}
				// (a corner cut: a point a little way back along the stretch in, and one a little way on along the
				// stretch out; each no further than a quarter of its stretch, nor than Cut)
				const FPoint& Here = Chain.Points[Index];
				const FPoint& Before = Chain.Points[(Index + Count - 1) % Count];
				const FPoint& After = Chain.Points[(Index + 1) % Count];
				const double AlongHere = Chain.Along[Index], AlongBefore = FrontAlongBefore(Chain, Index), AlongAfter = FrontAlongAfter(Chain, Index);
				const double LongIn = FrontDistance(Before, Here), LongOn = FrontDistance(Here, After);
				if (LongIn > 1e-9)
				{
					const double Share = std::min(0.25 * LongIn, Cut) / LongIn;
					Work.Points.push_back(FrontLerp(Here, Before, Share));
					Work.Along.push_back(AlongHere + (AlongBefore - AlongHere) * Share);
					Work.Keep.push_back(0);
				}
				if (LongOn > 1e-9)
				{
					const double Share = std::min(0.25 * LongOn, Cut) / LongOn;
					Work.Points.push_back(FrontLerp(Here, After, Share));
					Work.Along.push_back(AlongHere + (AlongAfter - AlongHere) * Share);
					Work.Keep.push_back(0);
				}
			}
			std::swap(Chain.Points, Work.Points);
			std::swap(Chain.Along, Work.Along);
			std::swap(Chain.Keep, Work.Keep);
		}
	}

	void Divide(FChain& Chain, double Step, FScratch& Work)
	{
		const size_t Count = Chain.Points.size();
		if (Count < 2 || Step <= 0.0)
		{
			return;
		}
		Work.Points.clear();
		Work.Along.clear();
		Work.Keep.clear();
		const size_t Stretches = Chain.bClosed ? Count : Count - 1;
		for (size_t Index = 0; Index < Count; ++Index)
		{
			Work.Points.push_back(Chain.Points[Index]);
			Work.Along.push_back(Chain.Along[Index]);
			Work.Keep.push_back(Chain.Keep[Index]);
			if (Index >= Stretches)
			{
				continue;
			}
			const FPoint& From = Chain.Points[Index];
			const FPoint& To = Chain.Points[(Index + 1) % Count];
			const double AlongFrom = Chain.Along[Index], AlongTo = FrontAlongAfter(Chain, Index);
			const int Parts = static_cast<int>(std::ceil(FrontDistance(From, To) / Step));
			for (int Part = 1; Part < Parts; ++Part)
			{
				const double Share = static_cast<double>(Part) / Parts;
				Work.Points.push_back(FrontLerp(From, To, Share));
				Work.Along.push_back(AlongFrom + (AlongTo - AlongFrom) * Share);
				Work.Keep.push_back(0);
			}
		}
		std::swap(Chain.Points, Work.Points);
		std::swap(Chain.Along, Work.Along);
		std::swap(Chain.Keep, Work.Keep);
	}

	void MeetLines(std::vector<FChain>& Chains)
	{
		auto Onto = [&Chains](const FPoint& Point, int Line)
		{
			const FChain& Target = Chains[static_cast<size_t>(Line)];
			const size_t Count = Target.Points.size();
			FPoint Best = Point;
			double Nearest = -1.0;
			const size_t Stretches = Target.bClosed ? Count : (Count > 0 ? Count - 1 : 0);
			for (size_t Index = 0; Index < Stretches; ++Index)
			{
				const FPoint Candidate = FrontNearestOn(Point, Target.Points[Index], Target.Points[(Index + 1) % Count]);
				const double Apart = FrontDistance(Candidate, Point);
				if (Nearest < 0.0 || Apart < Nearest)
				{
					Nearest = Apart;
					Best = Candidate;
				}
			}
			return Best;
		};
		const int Lines = static_cast<int>(Chains.size());
		for (FChain& Chain : Chains)
		{
			if (Chain.bClosed || Chain.Points.size() < 2)
			{
				continue;
			}
			if (Chain.StartOnto >= 0 && Chain.StartOnto < Lines && Chains[static_cast<size_t>(Chain.StartOnto)].Points.size() >= 2)
			{
				Chain.Points.front() = Onto(Chain.Points.front(), Chain.StartOnto);
			}
			if (Chain.EndOnto >= 0 && Chain.EndOnto < Lines && Chains[static_cast<size_t>(Chain.EndOnto)].Points.size() >= 2)
			{
				Chain.Points.back() = Onto(Chain.Points.back(), Chain.EndOnto);
			}
		}
	}

	void DrawFront(const std::vector<FBorderPiece>& Pieces, const FSides& Sides, const FRules& Rules, FScratch& Work, std::vector<FFrontPiece>& Front, std::vector<FChain>& Chains)
	{
		PickFront(Pieces, Sides, Front);
		ChainFront(Front, Rules, Work, Chains);
		for (FChain& Chain : Chains)
		{
			Despike(Chain);
			Thin(Chain, Rules.Thin, Work);
			Round(Chain, Rules.Passes, Rules.Cut, Work);
		}
		MeetLines(Chains); // (onto the lines as rounded: dividing them puts points on them and moves none)
		for (FChain& Chain : Chains)
		{
			Divide(Chain, Rules.Step, Work);
		}
	}

	double Waver(double X, double Y)
	{
		// (two slow waves across the map at different slants: a pen's pressure, never a pattern. At most 1 either way.)
		return 0.55 * std::sin(0.83 * X + 0.31 * Y + 1.3) + 0.45 * std::sin(-0.27 * X + 0.61 * Y + 4.1);
	}

	void FInkMesh::Clear()
	{
		Vertices.clear();
		Triangles.clear();
	}

	int FInkMesh::Add(double X, double Y, double Lift, const unsigned char* Rgb, unsigned char Alpha)
	{
		FInkVertex& Made = Vertices.emplace_back();
		Made.X = X;
		Made.Y = Y;
		Made.Lift = Lift;
		Made.R = Rgb[0];
		Made.G = Rgb[1];
		Made.B = Rgb[2];
		Made.A = Alpha;
		return static_cast<int>(Vertices.size()) - 1;
	}

	void FInkMesh::Triangle(int One, int Two, int Three)
	{
		Triangles.push_back(One);
		Triangles.push_back(Two);
		Triangles.push_back(Three);
	}

	void LayPen(const std::vector<FChain>& Chains, const FPenLook& Look, int Weight, FInkMesh& Ink, FInkMesh& Halo)
	{
		Ink.Clear();
		Halo.Clear();
		const double Scale = Weight == 1 ? Look.Heavier : 1.0;
		const double ShadowX = Look.ShadowX * Scale, ShadowY = Look.ShadowY * Scale;
		std::vector<double> Teeth;
		for (const FChain& Chain : Chains)
		{
			const size_t Count = Chain.Points.size();
			if (Count < 2)
			{
				continue;
			}
			const int Kind = Chain.Kind == EKind::Mine ? 1 : 0;
			const unsigned char* Rgb = Look.Ink[Kind];
			const unsigned char HaloAlpha = Look.HaloAlpha[Kind];
			const double Half = Look.Half[Kind] * Scale;
			auto WidthAt = [&Look, Half](const FPoint& At)
			{
				return Half * (1.0 + Look.WaverShare * Waver(At.X, At.Y));
			};
			auto TangentAt = [&Chain, Count](size_t Index)
			{
				const bool bOpenStart = !Chain.bClosed && Index == 0, bOpenEnd = !Chain.bClosed && Index + 1 == Count;
				const FPoint& Before = bOpenStart ? Chain.Points[Index] : Chain.Points[(Index + Count - 1) % Count];
				const FPoint& After = bOpenEnd ? Chain.Points[Index] : Chain.Points[(Index + 1) % Count];
				FPoint Way = FrontDirection(Before, After);
				if (Way.X == 0.0 && Way.Y == 0.0)
				{
					Way = FPoint{ 1.0, 0.0 };
				}
				return Way;
			};

			// The ink, and the shadow under it: two points across the line for each of its points, four for the shadow's
			// (the outer two clear, the inner two at its strength), and at a loose end a short narrowing run past it.
			const int InkFirst = static_cast<int>(Ink.Vertices.size()), HaloFirst = static_cast<int>(Halo.Vertices.size());
			auto Section = [&](const FPoint& At, const FPoint& Tangent, double Width, double Fade)
			{
				const double NX = -Tangent.Y, NY = Tangent.X;
				Ink.Add(At.X + NX * Width, At.Y + NY * Width, Look.InkLift, Rgb, 255);
				Ink.Add(At.X - NX * Width, At.Y - NY * Width, Look.InkLift, Rgb, 255);
				const double SX = At.X + ShadowX, SY = At.Y + ShadowY;
				const double Inner = Width * Look.HaloInner, Outer = Width * Look.HaloOuter;
				const unsigned char Strength = static_cast<unsigned char>(HaloAlpha * Fade);
				Halo.Add(SX + NX * Outer, SY + NY * Outer, Look.HaloLift, Look.HaloInk, 0);
				Halo.Add(SX + NX * Inner, SY + NY * Inner, Look.HaloLift, Look.HaloInk, Strength);
				Halo.Add(SX - NX * Inner, SY - NY * Inner, Look.HaloLift, Look.HaloInk, Strength);
				Halo.Add(SX - NX * Outer, SY - NY * Outer, Look.HaloLift, Look.HaloInk, 0);
			};
			auto Join = [&Ink, &Halo](int InkOne, int InkOther, int HaloOne, int HaloOther)
			{
				Ink.Triangle(InkOne, InkOne + 1, InkOther);
				Ink.Triangle(InkOne + 1, InkOther + 1, InkOther);
				for (int Band = 0; Band < 3; ++Band)
				{
					Halo.Triangle(HaloOne + Band, HaloOne + Band + 1, HaloOther + Band);
					Halo.Triangle(HaloOne + Band + 1, HaloOther + Band + 1, HaloOther + Band);
				}
			};
			int Sections = 0;
			if (!Chain.bClosed)
			{
				const FPoint Tangent = TangentAt(0);
				const double Width = WidthAt(Chain.Points[0]);
				const FPoint Cap{ Chain.Points[0].X - Tangent.X * Width * Look.CapReach, Chain.Points[0].Y - Tangent.Y * Width * Look.CapReach };
				Section(Cap, Tangent, Width * Look.CapNarrow, 0.0);
				++Sections;
			}
			for (size_t Index = 0; Index < Count; ++Index)
			{
				Section(Chain.Points[Index], TangentAt(Index), WidthAt(Chain.Points[Index]), 1.0);
				++Sections;
			}
			if (!Chain.bClosed)
			{
				const FPoint Tangent = TangentAt(Count - 1);
				const double Width = WidthAt(Chain.Points[Count - 1]);
				const FPoint Cap{ Chain.Points[Count - 1].X + Tangent.X * Width * Look.CapReach, Chain.Points[Count - 1].Y + Tangent.Y * Width * Look.CapReach };
				Section(Cap, Tangent, Width * Look.CapNarrow, 0.0);
				++Sections;
			}
			for (int Index = 0; Index + 1 < Sections; ++Index)
			{
				Join(InkFirst + Index * 2, InkFirst + (Index + 1) * 2, HaloFirst + Index * 4, HaloFirst + (Index + 1) * 4);
			}
			if (Chain.bClosed)
			{
				Join(InkFirst + (Sections - 1) * 2, InkFirst, HaloFirst + (Sections - 1) * 4, HaloFirst);
			}

			// The teeth, on the player's own fronts only: shared out evenly along each run of border, so that a tooth
			// stands in the same place each time the line is drawn again, however the front has moved elsewhere.
			if (Chain.Kind != EKind::Mine || Chain.RunLength.empty() || Chain.Along.size() != Count)
			{
				continue;
			}
			const double Every = Look.TickEvery * (Weight == 1 ? Look.TickEveryFar : 1.0);
			Teeth.clear();
			for (size_t Run = 0; Run < Chain.RunLength.size(); ++Run)
			{
				const int Many = static_cast<int>(std::floor(Chain.RunLength[Run] / Every + 0.5));
				for (int Tooth = 0; Tooth < Many; ++Tooth)
				{
					double At = static_cast<double>(Run) + (Tooth + 0.5) / Many;
					if (Chain.bClosed)
					{
						At += At < Chain.Along[0] ? Chain.Span : (At >= Chain.Along[0] + Chain.Span ? -Chain.Span : 0.0);
					}
					Teeth.push_back(At);
				}
			}
			std::sort(Teeth.begin(), Teeth.end());
			double Total = 0.0;
			const size_t Stretches = Chain.bClosed ? Count : Count - 1;
			for (size_t Index = 0; Index < Stretches; ++Index)
			{
				Total += FrontDistance(Chain.Points[Index], Chain.Points[(Index + 1) % Count]);
			}
			const double Clear = Look.TickClear * Scale;
			double Gone = 0.0;
			size_t Next = 0;
			for (size_t Index = 0; Index < Stretches && Next < Teeth.size(); ++Index)
			{
				const FPoint& From = Chain.Points[Index];
				const FPoint& To = Chain.Points[(Index + 1) % Count];
				const double AlongFrom = Chain.Along[Index], AlongTo = FrontAlongAfter(Chain, Index);
				const double Length = FrontDistance(From, To);
				while (Next < Teeth.size() && Teeth[Next] <= AlongTo)
				{
					const double At = Teeth[Next++];
					if (At < AlongFrom || AlongTo <= AlongFrom || Length < 1e-9)
					{
						continue;
					}
					const double Share = (At - AlongFrom) / (AlongTo - AlongFrom);
					const double Reached = Gone + Length * Share;
					if (!Chain.bClosed && (Reached < Clear || Reached > Total - Clear))
					{
						continue;
					}
					const FPoint Foot = FrontLerp(From, To, Share);
					const FPoint Way = FrontDirection(From, To);
					const double NX = -Way.Y, NY = Way.X; // (to the left, where the enemy is)
					const double Width = WidthAt(Foot), Base = Look.TickBase * Scale;
					const int One = Ink.Add(Foot.X - Way.X * Base + NX * Width * 0.5, Foot.Y - Way.Y * Base + NY * Width * 0.5, Look.InkLift, Rgb, 255);
					const int Other = Ink.Add(Foot.X + Way.X * Base + NX * Width * 0.5, Foot.Y + Way.Y * Base + NY * Width * 0.5, Look.InkLift, Rgb, 255);
					const int Tip = Ink.Add(Foot.X + NX * (Width + Look.TickReach * Scale), Foot.Y + NY * (Width + Look.TickReach * Scale), Look.InkLift, Rgb, 255);
					Ink.Triangle(One, Other, Tip);
				}
				Gone += Length;
			}
		}
	}

	FInkPhase InkPhase(double Second, double Length)
	{
		FInkPhase Phase;
		const double Run = std::max(Length, 0.05);
		const float Time = static_cast<float>(std::clamp(Second / Run, 0.0, 1.0));
		Phase.Share = 1.0f - (1.0f - Time) * (1.0f - Time);
		Phase.Late = FrontSmoothStep(0.7f, 1.0f, Time);
		Phase.Dry = static_cast<float>(std::clamp((Second - Run) / 0.3, 0.0, 1.0));
		Phase.bBorders = Time >= 0.7f;
		Phase.bOver = Second >= Run + 0.3;
		return Phase;
	}

	double InkFitted(double Length, double Shown)
	{
		const double Room = Shown - 0.125; // (the ink is laid every eighth of a second: its last laying falls inside the step)
		return std::clamp(Room, std::min(Length, 0.4), Length);
	}
}
