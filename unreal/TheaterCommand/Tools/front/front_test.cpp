// The drawn front's cloud test (Tools/front/run.sh): the game's own Campaign/FrontLine, no engine, on small made-up maps
// of square provinces, and (given a file of pieces, Tools/front/pieces_from_map.py) on the game's own map.

#include "Campaign/FrontLine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace TheaterFront;

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

	const double Cell = 10.0; // (each made-up province is a square this many map units across)

	/**
	 * A made-up map: one letter a square province, its holder; '.' is sea. Rows run down the map (y grows), columns
	 * across (x grows). The pieces are found as the table finds its shared edges: each line two provinces share, once,
	 * from the lower-numbered province (Here) into the higher (There).
	 */
	struct FMadeUp
	{
		std::vector<std::string> Rows;
		std::vector<FBorderPiece> Pieces;
		std::map<char, int> Holder;
		FSides Sides;
		int Wide = 0, High = 0;

		FMadeUp(const std::vector<std::string>& Map, const std::vector<std::string>& Wars, char Player)
			: Rows(Map)
		{
			High = static_cast<int>(Map.size());
			Wide = static_cast<int>(Map[0].size());
			for (const std::string& Row : Map)
			{
				for (char Letter : Row)
				{
					if (Letter != '.' && !Holder.count(Letter))
					{
						const int Number = static_cast<int>(Holder.size());
						Holder[Letter] = Number;
					}
				}
			}
			Sides.Reset(Wide * High, static_cast<int>(Holder.size()));
			for (int Y = 0; Y < High; ++Y)
			{
				for (int X = 0; X < Wide; ++X)
				{
					Sides.HolderOf[static_cast<size_t>(Y * Wide + X)] = Map[static_cast<size_t>(Y)][static_cast<size_t>(X)] == '.' ? -1 : Holder[Map[static_cast<size_t>(Y)][static_cast<size_t>(X)]];
				}
			}
			for (const std::string& War : Wars)
			{
				Sides.SetWar(Holder[War[0]], Holder[War[1]]);
				if (War[0] == Player)
				{
					Sides.Foe[static_cast<size_t>(Holder[War[1]])] = 1;
				}
				if (War[1] == Player)
				{
					Sides.Foe[static_cast<size_t>(Holder[War[0]])] = 1;
				}
			}
			for (int Y = 0; Y < High; ++Y)
			{
				for (int X = 0; X < Wide; ++X)
				{
					if (Map[static_cast<size_t>(Y)][static_cast<size_t>(X)] == '.')
					{
						continue;
					}
					if (X + 1 < Wide && Map[static_cast<size_t>(Y)][static_cast<size_t>(X) + 1] != '.')
					{
						FBorderPiece& Piece = Pieces.emplace_back();
						Piece.A = FPoint{ (X + 1) * Cell, Y * Cell };
						Piece.B = FPoint{ (X + 1) * Cell, (Y + 1) * Cell };
						Piece.Side = FPoint{ 1.0, 0.0 };
						Piece.Here = Y * Wide + X;
						Piece.There = Y * Wide + X + 1;
					}
					if (Y + 1 < High && Map[static_cast<size_t>(Y) + 1][static_cast<size_t>(X)] != '.')
					{
						FBorderPiece& Piece = Pieces.emplace_back();
						// (the other way round, so that the pieces do not all run the same way)
						Piece.A = FPoint{ (X + 1) * Cell, (Y + 1) * Cell };
						Piece.B = FPoint{ X * Cell, (Y + 1) * Cell };
						Piece.Side = FPoint{ 0.0, 1.0 };
						Piece.Here = Y * Wide + X;
						Piece.There = (Y + 1) * Wide + X;
					}
				}
			}
		}

		/** Which made-up province a point of the map lies in, or -1 for sea or beyond. */
		int ProvinceAt(double X, double Y) const
		{
			const int CX = static_cast<int>(std::floor(X / Cell)), CY = static_cast<int>(std::floor(Y / Cell));
			if (CX < 0 || CY < 0 || CX >= Wide || CY >= High || Rows[static_cast<size_t>(CY)][static_cast<size_t>(CX)] == '.')
			{
				return -1;
			}
			return CY * Wide + CX;
		}
	};

	double Apart(const FPoint& One, const FPoint& Other)
	{
		return std::hypot(Other.X - One.X, Other.Y - One.Y);
	}

	/** How far a point is from a line (every stretch of it). */
	double OffChain(const FPoint& Point, const FChain& Chain)
	{
		double Best = 1e300;
		const size_t Count = Chain.Points.size();
		const size_t Stretches = Chain.bClosed ? Count : Count - 1;
		for (size_t Index = 0; Index < Stretches; ++Index)
		{
			const FPoint& From = Chain.Points[Index];
			const FPoint& To = Chain.Points[(Index + 1) % Count];
			const double DX = To.X - From.X, DY = To.Y - From.Y, Square = DX * DX + DY * DY;
			const double Share = Square < 1e-18 ? 0.0 : std::clamp(((Point.X - From.X) * DX + (Point.Y - From.Y) * DY) / Square, 0.0, 1.0);
			Best = std::min(Best, Apart(Point, FPoint{ From.X + DX * Share, From.Y + DY * Share }));
		}
		return Best;
	}

	double LengthOf(const FChain& Chain)
	{
		double Total = 0.0;
		const size_t Count = Chain.Points.size();
		const size_t Stretches = Chain.bClosed ? Count : Count - 1;
		for (size_t Index = 0; Index < Stretches; ++Index)
		{
			Total += Apart(Chain.Points[Index], Chain.Points[(Index + 1) % Count]);
		}
		return Total;
	}

	/** The checks every finished line must pass, whatever the map. */
	void CheckFinished(const std::vector<FChain>& Chains, const FRules& Rules, const std::string& Name)
	{
		for (size_t Index = 0; Index < Chains.size(); ++Index)
		{
			const FChain& Chain = Chains[Index];
			const std::string Which = Name + ": line " + std::to_string(Index);
			Check(Chain.Points.size() >= 2, Which + " has at least two points");
			Check(Chain.Along.size() == Chain.Points.size() && Chain.Keep.size() == Chain.Points.size(), Which + " keeps a measure and a mark for every point");
			bool bRising = true, bFinite = true;
			for (size_t Point = 0; Point < Chain.Points.size(); ++Point)
			{
				bFinite &= std::isfinite(Chain.Points[Point].X) && std::isfinite(Chain.Points[Point].Y) && std::isfinite(Chain.Along[Point]);
				if (Point > 0)
				{
					bRising &= Chain.Along[Point] >= Chain.Along[Point - 1] - 1e-9;
				}
			}
			Check(bFinite, Which + " is all finite numbers");
			Check(bRising, Which + ": its measure along the runs never goes back");
			const size_t Count = Chain.Points.size();
			const size_t Stretches = Chain.bClosed ? Count : Count - 1;
			double Longest = 0.0;
			for (size_t Point = 0; Point < Stretches; ++Point)
			{
				Longest = std::max(Longest, Apart(Chain.Points[Point], Chain.Points[(Point + 1) % Count]));
			}
			Check(Longest <= Rules.Step + 1e-9, Which + ": no stretch longer than the step (" + std::to_string(Longest) + ")");
		}
	}

	struct FDrawn
	{
		FScratch Work;
		std::vector<FFrontPiece> Front;
		std::vector<FChain> Chains;
	};

	FDrawn Draw(const FMadeUp& Map, const FRules& Rules)
	{
		FDrawn Out;
		DrawFront(Map.Pieces, Map.Sides, Rules, Out.Work, Out.Front, Out.Chains);
		return Out;
	}

	std::vector<FChain> ChainOnly(const FMadeUp& Map, const FRules& Rules)
	{
		FScratch Work;
		std::vector<FFrontPiece> Front;
		std::vector<FChain> Chains;
		PickFront(Map.Pieces, Map.Sides, Front);
		ChainFront(Front, Rules, Work, Chains);
		return Chains;
	}

	/** Every tooth of a laid line: its tip, and the foot it stands on (the middle of its two base points). */
	struct FTooth
	{
		FPoint Tip, Foot;
	};
	std::vector<FTooth> TeethOf(const FInkMesh& Ink)
	{
		// (a tooth is a triangle of three points of its own, laid after the line's ribbon: the ribbon's triangles all
		// share points with the next, so a triangle whose three points are used by nothing else is a tooth)
		std::vector<int> Uses(Ink.Vertices.size(), 0);
		for (int Corner : Ink.Triangles)
		{
			++Uses[static_cast<size_t>(Corner)];
		}
		std::vector<FTooth> Out;
		for (size_t T = 0; T + 2 < Ink.Triangles.size(); T += 3)
		{
			const int One = Ink.Triangles[T], Other = Ink.Triangles[T + 1], Tip = Ink.Triangles[T + 2];
			if (Uses[static_cast<size_t>(One)] == 1 && Uses[static_cast<size_t>(Other)] == 1 && Uses[static_cast<size_t>(Tip)] == 1)
			{
				const FInkVertex& A = Ink.Vertices[static_cast<size_t>(One)];
				const FInkVertex& B = Ink.Vertices[static_cast<size_t>(Other)];
				const FInkVertex& C = Ink.Vertices[static_cast<size_t>(Tip)];
				Out.push_back(FTooth{ FPoint{ C.X, C.Y }, FPoint{ (A.X + B.X) * 0.5, (A.Y + B.Y) * 0.5 } });
			}
		}
		return Out;
	}

	void CheckMesh(const FInkMesh& Mesh, const std::string& Name)
	{
		bool bInside = Mesh.Triangles.size() % 3 == 0, bFinite = true;
		for (int Corner : Mesh.Triangles)
		{
			bInside &= Corner >= 0 && static_cast<size_t>(Corner) < Mesh.Vertices.size();
		}
		for (const FInkVertex& Vertex : Mesh.Vertices)
		{
			bFinite &= std::isfinite(Vertex.X) && std::isfinite(Vertex.Y) && std::isfinite(Vertex.Lift);
		}
		Check(bInside, Name + ": every triangle's corners are points of the mesh");
		Check(bFinite, Name + ": every point is finite");
	}

	// ---------------------------------------------------------------------------------------------------------------

	void StraightFront()
	{
		const FMadeUp Map({ "GGPP", "GGPP", "GGPP", "GGPP" }, { "GP" }, 'G');
		FRules Rules;
		const std::vector<FChain> Raw = ChainOnly(Map, Rules);
		Check(Raw.size() == 1, "straight front: one line (" + std::to_string(Raw.size()) + ")");
		if (Raw.size() != 1)
		{
			return;
		}
		Check(!Raw[0].bClosed && Raw[0].Kind == EKind::Mine, "straight front: an open line of the player's war");
		Check(Raw[0].Points.size() == 5, "straight front: four pieces end to end make five points");
		Check(Raw[0].RunLength.size() == 4, "straight front: four runs (each row's pair of provinces is one)");
		// (his enemy P lies east, +x: going up the map, -y, the left hand is east, so the line runs up)
		Check(Raw[0].Points.front().Y > Raw[0].Points.back().Y, "straight front: turned so that the enemy is on its left (it runs up the map)");
		Check(Raw[0].Keep.front() == 2 && Raw[0].Keep.back() == 2 && Raw[0].Keep[1] == 1, "straight front: its ends held through everything, its runs' meetings through thinning");

		FDrawn Drawn = Draw(Map, Rules);
		CheckFinished(Drawn.Chains, Rules, "straight front");
		Check(Drawn.Chains.size() == 1 && std::fabs(Drawn.Chains[0].Points.front().X - 20.0) < 1e-9 && std::fabs(Drawn.Chains[0].Points.front().Y - 40.0) < 1e-9, "straight front: its end stays where the border meets the map's edge");
		bool bOnBorder = true;
		for (const FPoint& Point : Drawn.Chains[0].Points)
		{
			bOnBorder &= std::fabs(Point.X - 20.0) < 1e-9;
		}
		Check(bOnBorder, "straight front: a straight border stays a straight line");
		Check(std::fabs(LengthOf(Drawn.Chains[0]) - 40.0) < 1e-9, "straight front: as long as the border");

		FPenLook Look;
		FInkMesh Ink, Halo;
		LayPen(Drawn.Chains, Look, 0, Ink, Halo);
		CheckMesh(Ink, "straight front ink");
		CheckMesh(Halo, "straight front shadow");
		const std::vector<FTooth> Teeth = TeethOf(Ink);
		// (four runs of 10 at a tooth every 2.4: four teeth each; none within 0.8 of an end)
		Check(Teeth.size() == 16, "straight front: four teeth to each run of ten (" + std::to_string(Teeth.size()) + ")");
		bool bEast = true;
		for (const FTooth& Tooth : Teeth)
		{
			bEast &= Tooth.Tip.X > Tooth.Foot.X + 0.3;
		}
		Check(bEast, "straight front: every tooth points east, at the enemy");
		const size_t Sections = Drawn.Chains[0].Points.size() + 2;
		Check(Ink.Triangles.size() == (Sections - 1) * 6 + Teeth.size() * 3, "straight front: the ink is one ribbon with its two narrowing ends, and the teeth");
		Check(Halo.Vertices.size() == Sections * 4 && Halo.Triangles.size() == (Sections - 1) * 18, "straight front: the shadow is one ribbon four points across");
		bool bClearEdges = true, bSoftMiddle = true;
		for (size_t Index = 0; Index < Halo.Vertices.size(); ++Index)
		{
			const size_t Across = Index % 4;
			if (Across == 0 || Across == 3)
			{
				bClearEdges &= Halo.Vertices[Index].A == 0;
			}
			else if (Index >= 4 && Index + 4 < Halo.Vertices.size())
			{
				bSoftMiddle &= Halo.Vertices[Index].A == Look.HaloAlpha[1];
			}
		}
		Check(bClearEdges && bSoftMiddle, "straight front: the shadow is clear at its edges and at its strength under the line");
		double Narrowest = 1e300, Widest = 0.0;
		for (size_t Index = 2; Index + 2 < Ink.Vertices.size() && Index < Sections * 2 - 2; Index += 2)
		{
			const double Width = Apart(FPoint{ Ink.Vertices[Index].X, Ink.Vertices[Index].Y }, FPoint{ Ink.Vertices[Index + 1].X, Ink.Vertices[Index + 1].Y });
			Narrowest = std::min(Narrowest, Width);
			Widest = std::max(Widest, Width);
		}
		Check(Narrowest >= 2.0 * Look.Half[1] * (1.0 - Look.WaverShare) - 1e-9 && Widest <= 2.0 * Look.Half[1] * (1.0 + Look.WaverShare) + 1e-9 && Widest > Narrowest * 1.02,
			"straight front: the nib swells and thins, but only by its share (" + std::to_string(Narrowest) + " to " + std::to_string(Widest) + ")");

		// The same front seen by the other side: the line runs the other way and its teeth point west.
		const FMadeUp Other({ "GGPP", "GGPP", "GGPP", "GGPP" }, { "GP" }, 'P');
		FDrawn Theirs = Draw(Other, Rules);
		LayPen(Theirs.Chains, Look, 0, Ink, Halo);
		bool bWest = true;
		for (const FTooth& Tooth : TeethOf(Ink))
		{
			bWest &= Tooth.Tip.X < Tooth.Foot.X - 0.3;
		}
		Check(Theirs.Chains.size() == 1 && Theirs.Chains[0].Points.front().Y < Theirs.Chains[0].Points.back().Y && bWest, "straight front, played by the other side: the line runs down the map and its teeth point west");

		// Watched by nobody at war: the line is the quieter one, with no teeth.
		const FMadeUp Watched({ "GGPP", "GGPP", "GGPP", "GGPP" }, { "GP" }, 'X');
		FDrawn Quiet = Draw(Watched, Rules);
		LayPen(Quiet.Chains, Look, 0, Ink, Halo);
		Check(Quiet.Chains.size() == 1 && Quiet.Chains[0].Kind == EKind::Theirs && TeethOf(Ink).empty(), "straight front, others' war: one quiet line, no teeth");

		// The staff map's weight: everything heavier, and the teeth further apart.
		LayPen(Drawn.Chains, Look, 1, Ink, Halo);
		const size_t FarTeeth = TeethOf(Ink).size();
		Check(FarTeeth == 8, "straight front, staff map's weight: half as many teeth (" + std::to_string(FarTeeth) + ")");
	}

	void Staircase()
	{
		// (the border turns at every corner where four squares meet: the line goes on through each)
		const FMadeUp Map({ "GGPPP", "GGGPP", "GGGGP", "GPPPP" }, { "GP" }, 'G');
		FRules Rules;
		const std::vector<FChain> Raw = ChainOnly(Map, Rules);
		int Pieces = 0;
		{
			std::vector<FFrontPiece> Front;
			PickFront(Map.Pieces, Map.Sides, Front);
			Pieces = static_cast<int>(Front.size());
		}
		Check(Raw.size() == 1 && static_cast<int>(Raw[0].Points.size()) == Pieces + 1, "staircase: every piece of the front in one line (" + std::to_string(Raw.size()) + " lines, " + std::to_string(Pieces) + " pieces)");
		FDrawn Drawn = Draw(Map, Rules);
		CheckFinished(Drawn.Chains, Rules, "staircase");
		// (rounded, but never more than a cut's length off the border it follows)
		double Furthest = 0.0;
		for (const FPoint& Point : Drawn.Chains[0].Points)
		{
			Furthest = std::max(Furthest, OffChain(Point, Raw[0]));
		}
		Check(Furthest > 0.05 && Furthest <= Rules.Cut, "staircase: its corners rounded, within a cut of the border (" + std::to_string(Furthest) + ")");
		Check(LengthOf(Drawn.Chains[0]) < LengthOf(Raw[0]) - 0.5, "staircase: rounding takes the corners in, so the line is shorter than the border");
	}

	void Junctions()
	{
		// Three countries all at war, meeting at a point: the player's line goes on through it, the others' war stops on it.
		{
			const FMadeUp Map({ "GGRR", "GGRR", "PPPP", "PPPP" }, { "GR", "GP", "RP" }, 'G');
			FRules Rules;
			FDrawn Drawn = Draw(Map, Rules);
			CheckFinished(Drawn.Chains, Rules, "three at war");
			int Mine = 0, Theirs = 0, Through = -1, Branch = -1;
			for (size_t Index = 0; Index < Drawn.Chains.size(); ++Index)
			{
				if (Drawn.Chains[Index].Kind == EKind::Mine)
				{
					++Mine;
					Through = static_cast<int>(Index);
				}
				else
				{
					++Theirs;
					Branch = static_cast<int>(Index);
				}
			}
			Check(Mine == 1 && Theirs == 1, "three at war: one line of the player's (round the corner) and one of the others'");
			if (Mine == 1 && Theirs == 1)
			{
				const FChain& Line = Drawn.Chains[static_cast<size_t>(Through)];
				const FChain& Stub = Drawn.Chains[static_cast<size_t>(Branch)];
				Check(std::fabs(LengthOf(Line) - 40.0) < 1.5, "three at war: the player's line runs round the corner, the whole of both borders");
				Check(Stub.StartOnto == Through || Stub.EndOnto == Through, "three at war: the others' line knows it stops on the player's");
				const FPoint& Meets = Stub.StartOnto == Through ? Stub.Points.front() : Stub.Points.back();
				Check(OffChain(Meets, Line) < 1e-9, "three at war: the others' line ends exactly on the player's, as rounded");
				Check(Apart(Meets, FPoint{ 20.0, 20.0 }) < Rules.Cut, "three at war: and near where the three borders meet");
			}
		}
		// Two wars that touch at a point and are not one line: two lines, both ending on that very point.
		{
			const FMadeUp Map({ "GGRR", "GGRR", "PPSS", "PPSS" }, { "GP", "RS" }, 'G');
			FRules Rules;
			FDrawn Drawn = Draw(Map, Rules);
			CheckFinished(Drawn.Chains, Rules, "two wars touching");
			Check(Drawn.Chains.size() == 2, "two wars touching: two lines (" + std::to_string(Drawn.Chains.size()) + ")");
			int AtPoint = 0;
			for (const FChain& Chain : Drawn.Chains)
			{
				AtPoint += Apart(Chain.Points.front(), FPoint{ 20.0, 20.0 }) < 1e-9 ? 1 : 0;
				AtPoint += Apart(Chain.Points.back(), FPoint{ 20.0, 20.0 }) < 1e-9 ? 1 : 0;
			}
			Check(AtPoint == 2, "two wars touching: each ends exactly where they meet");
		}
		// Four at war round one point: two lines cross it, each going straight on.
		{
			const FMadeUp Map({ "GGRR", "GGRR", "PPSS", "PPSS" }, { "GR", "GP", "RS", "PS" }, 'X');
			FRules Rules;
			const std::vector<FChain> Raw = ChainOnly(Map, Rules);
			bool bStraight = Raw.size() == 2;
			for (const FChain& Chain : Raw)
			{
				const FPoint& From = Chain.Points.front();
				const FPoint& To = Chain.Points.back();
				bStraight &= std::fabs(From.X - To.X) < 1e-9 || std::fabs(From.Y - To.Y) < 1e-9;
			}
			Check(bStraight, "four at war round a point: two straight lines crossing it");
		}
	}

	void Split()
	{
		// A neutral between them: the front is two lines, one each side of its ground.
		const FMadeUp Map({ "GGPP", "GGPP", "GNNP", "GGPP", "GGPP" }, { "GP" }, 'G');
		FRules Rules;
		FDrawn Drawn = Draw(Map, Rules);
		CheckFinished(Drawn.Chains, Rules, "a neutral between");
		Check(Drawn.Chains.size() == 2, "a neutral between: the front splits into two lines (" + std::to_string(Drawn.Chains.size()) + ")");
		bool bApart = true;
		for (const FChain& Chain : Drawn.Chains)
		{
			bApart &= Chain.Kind == EKind::Mine;
			const bool bNorth = Chain.Points.front().Y <= 20.0 + 1e-9 && Chain.Points.back().Y <= 20.0 + 1e-9;
			const bool bSouth = Chain.Points.front().Y >= 30.0 - 1e-9 && Chain.Points.back().Y >= 30.0 - 1e-9;
			bApart &= bNorth || bSouth;
		}
		Check(bApart, "a neutral between: one line north of its ground and one south");
	}

	void Islands()
	{
		// Two islands at war across the water, a third all one country's: a line on each of the first two, none at sea.
		const FMadeUp Map({ "GGP..GGG..GGG", "GGP..GPG..GGG" }, { "GP" }, 'G');
		FRules Rules;
		FDrawn Drawn = Draw(Map, Rules);
		CheckFinished(Drawn.Chains, Rules, "islands");
		Check(Drawn.Chains.size() == 2, "islands: a line on each island at war (" + std::to_string(Drawn.Chains.size()) + ")");
		bool bAshore = true;
		for (const FChain& Chain : Drawn.Chains)
		{
			double Least = 1e300, Most = -1e300;
			for (const FPoint& Point : Chain.Points)
			{
				Least = std::min(Least, Point.X);
				Most = std::max(Most, Point.X);
			}
			bAshore &= (Least >= 0.0 && Most <= 30.0) || (Least >= 50.0 && Most <= 80.0);
			bAshore &= !Chain.bClosed;
		}
		Check(bAshore, "islands: no line crosses the water, and each ends at its coast");
		FPenLook Look;
		FInkMesh Ink, Halo;
		LayPen(Drawn.Chains, Look, 0, Ink, Halo);
		bool bAtThem = true;
		for (const FTooth& Tooth : TeethOf(Ink))
		{
			const int Province = Map.ProvinceAt(Tooth.Tip.X, Tooth.Tip.Y);
			bAtThem &= Province >= 0 && Map.Sides.HolderOf[static_cast<size_t>(Province)] == Map.Holder.at('P');
		}
		Check(bAtThem, "islands: every tooth stands on the enemy's ground");
	}

	void Enclave()
	{
		for (const char Player : { 'G', 'P' })
		{
			const std::string Seen = std::string("enclave, played by ") + Player;
			const FMadeUp Map({ "GGGGG", "GGPGG", "GGGGG" }, { "GP" }, Player);
			FRules Rules;
			const std::vector<FChain> Raw = ChainOnly(Map, Rules);
			Check(Raw.size() == 1 && Raw[0].bClosed, Seen + ": one line, closed round it");
			if (Raw.size() != 1)
			{
				continue;
			}
			Check(Raw[0].Points.size() == 4 && Raw[0].RunLength.size() == 4 && std::fabs(Raw[0].Span - 4.0) < 1e-12, Seen + ": four sides, four runs");
			FDrawn Drawn = Draw(Map, Rules);
			CheckFinished(Drawn.Chains, Rules, Seen);
			const FPoint Middle{ 25.0, 15.0 };
			bool bRound = true;
			for (const FPoint& Point : Drawn.Chains[0].Points)
			{
				const double Off = std::max(std::fabs(Point.X - Middle.X), std::fabs(Point.Y - Middle.Y));
				bRound &= Off <= 5.0 + 1e-9 && Off >= 5.0 - Rules.Cut;
			}
			Check(bRound, Seen + ": the line keeps to the enclave's edge, its corners rounded");
			FPenLook Look;
			FInkMesh Ink, Halo;
			LayPen(Drawn.Chains, Look, 0, Ink, Halo);
			CheckMesh(Ink, Seen + " ink");
			CheckMesh(Halo, Seen + " shadow");
			const std::vector<FTooth> Teeth = TeethOf(Ink);
			bool bFacing = !Teeth.empty();
			for (const FTooth& Tooth : Teeth)
			{
				const bool bInward = Apart(Tooth.Tip, Middle) < Apart(Tooth.Foot, Middle);
				bFacing &= Player == 'G' ? bInward : !bInward;
			}
			Check(bFacing, Seen + (Player == 'G' ? ": every tooth points in, at the enclave" : ": every tooth points out, at the country round it"));
			Check(Teeth.size() == 16, Seen + ": four teeth to each side, none lost where the ring closes (" + std::to_string(Teeth.size()) + ")");
			const size_t Sections = Drawn.Chains[0].Points.size();
			Check(Halo.Triangles.size() == Sections * 18, Seen + ": the shadow closes round on itself");
		}
	}

	void Gaps()
	{
		// A piece the table left out (under 0.05 long): the line still runs on.
		FMadeUp Map({ "GGPP", "GGPP", "GGPP" }, { "GP" }, 'G');
		for (FBorderPiece& Piece : Map.Pieces)
		{
			if (std::fabs(Piece.A.X - 20.0) < 1e-9 && std::fabs(Piece.A.Y - 10.0) < 1e-9 && std::fabs(Piece.B.X - 20.0) < 1e-9)
			{
				Piece.A.Y += 0.03;
			}
		}
		FRules Rules;
		Check(ChainOnly(Map, Rules).size() == 1, "a short gap: the line runs on over it");
		Rules.Gap = 0.0;
		Check(ChainOnly(Map, Rules).size() == 2, "a short gap, with joining turned off: two lines (the test is fair)");
	}

	void Thinning()
	{
		// A border drawn with many points in a row: thinned to its two ends; a bend more than the tolerance is kept.
		FChain Chain;
		for (int Index = 0; Index <= 10; ++Index)
		{
			Chain.Points.push_back(FPoint{ Index * 1.0, Index == 5 ? 0.1 : 0.0 });
			Chain.Along.push_back(Index / 10.0);
			Chain.Keep.push_back(Index == 0 || Index == 10 ? 2 : 0);
		}
		FChain Bent = Chain;
		Bent.Points[5].Y = 0.5;
		const FChain Unbent = Bent;
		FScratch Work;
		Thin(Chain, 0.2, Work);
		Check(Chain.Points.size() == 2, "thinning: a straight border's points come down to its ends");
		Thin(Bent, 0.2, Work);
		bool bKept = false, bNear = true;
		for (const FPoint& Point : Bent.Points)
		{
			bKept |= std::fabs(Point.X - 5.0) < 1e-12 && std::fabs(Point.Y - 0.5) < 1e-12;
		}
		for (const FPoint& Point : Unbent.Points)
		{
			bNear &= OffChain(Point, Bent) <= 0.2 + 1e-12;
		}
		Check(bKept && Bent.Points.size() < Unbent.Points.size(), "thinning: a real bend is kept, and fewer points with it");
		Check(bNear, "thinning: no point of the border stands further than the tolerance off the thinned line");
		// (a point where two pairs' borders meet is never thinned away, though it lies on the straight)
		FChain Held;
		for (int Index = 0; Index <= 4; ++Index)
		{
			Held.Points.push_back(FPoint{ Index * 1.0, 0.0 });
			Held.Along.push_back(Index / 4.0);
			Held.Keep.push_back(Index == 0 || Index == 4 ? 2 : (Index == 2 ? 1 : 0));
		}
		Thin(Held, 0.2, Work);
		Check(Held.Points.size() == 3 && std::fabs(Held.Points[1].X - 2.0) < 1e-12, "thinning: where one run meets the next is held");
		// (the art director's round 34 rule against saws)
		FChain Saw;
		const double Xs[] = { 0.0, 2.0, 3.0, 2.2, 4.0, 6.0 };
		for (int Index = 0; Index < 6; ++Index)
		{
			Saw.Points.push_back(FPoint{ Xs[Index], Index == 3 ? 0.2 : 0.0 });
			Saw.Along.push_back(Index / 5.0);
			Saw.Keep.push_back(Index == 0 || Index == 5 ? 2 : 0);
		}
		Despike(Saw);
		Check(Saw.Points.size() < 6, "despiking: a point where the line doubles back on itself is taken out");
	}

	void InkClock()
	{
		// Release 1's second, which the new length must still give when asked for one.
		const FInkPhase Half = InkPhase(0.5, 1.0);
		Check(std::fabs(Half.Share - 0.75f) < 1e-6f, "ink, one second: three quarters across at half a second");
		Check(!InkPhase(0.69, 1.0).bBorders && InkPhase(0.7, 1.0).bBorders, "ink, one second: the frontier moves at seven tenths");
		Check(!InkPhase(1.29, 1.0).bOver && InkPhase(1.3, 1.0).bOver, "ink, one second: over at 1.3 (the rim dried)");
		// The run of this release: a second and a half.
		const double Length = 1.5;
		Check(InkPhase(0.0, Length).Share == 0.0f && InkPhase(Length, Length).Share == 1.0f && InkPhase(9.0, Length).Share == 1.0f, "ink, 1.5 s: nothing at the start, all of it at the end and after");
		bool bRising = true;
		float Last = -1.0f;
		for (int Tenth = 0; Tenth <= 20; ++Tenth)
		{
			const float Share = InkPhase(Tenth * 0.1, Length).Share;
			bRising &= Share >= Last;
			Last = Share;
		}
		Check(bRising, "ink, 1.5 s: the colour only ever goes on");
		Check(InkPhase(0.375, Length).Share > 0.4f, "ink, 1.5 s: quick at first (over two fifths across in its first quarter)");
		Check(InkPhase(1.0, Length).Late == 0.0f && InkPhase(1.05, Length).bBorders && !InkPhase(1.04, Length).bBorders, "ink, 1.5 s: the ground beside and the frontier come over in the last three tenths of the run");
		Check(std::fabs(InkPhase(1.65, Length).Dry - 0.5f) < 1e-5f && InkPhase(1.8, Length).bOver && !InkPhase(1.79, Length).bOver, "ink, 1.5 s: the rim dries in three tenths more, and then it is over");
		// The month's replay: steps shown for 0.55 s (marches), 0.8 s (others' battles) and 1.3 s (the player's own), each
		// ended by the next step that moves ground. Unfitted, the 0.8 s step would cut the 1.5 s run before the frontier
		// had moved, at a little over three quarters across.
		Check(!InkPhase(0.8, Length).bBorders && InkPhase(0.8, Length).Share < 0.8f, "ink, replay: unfitted, a 0.8 s step cuts the 1.5 s run before the frontier moves");
		bool bFits = true, bWhole = true;
		for (const double Shown : { 0.55, 0.8, 1.3 })
		{
			const double Run = InkFitted(Length, Shown);
			bFits &= Run >= 0.4 && Run <= Shown && Run < Length;
			// (the last laying before the next step: an eighth of a second before it, at the latest)
			const FInkPhase AtLastLaying = InkPhase(Shown - 0.125, Run);
			bWhole &= AtLastLaying.Share == 1.0f && AtLastLaying.Late == 1.0f && AtLastLaying.bBorders;
		}
		Check(bFits, "ink, replay: each step's run is fitted inside the step, never under 0.4 s");
		Check(bWhole, "ink, replay: by the step's last laying the colour has crossed and the frontier band and line have come over");
		Check(InkFitted(Length, 2.2) == Length && InkFitted(Length, 6.4) == Length, "ink, replay: a raid's or a sinking's long step keeps the whole second and a half");
		Check(std::fabs(InkFitted(1.0, 0.8) - 0.675) < 1e-9 && InkFitted(0.3, 0.55) == 0.3 && InkFitted(Length, 0.3) == 0.4, "ink, replay: a shorter asked run is never lengthened; nothing under 0.4 s");
	}

	void Wavering()
	{
		bool bBounded = true, bSame = true;
		double Least = 1.0, Most = -1.0;
		for (int Step = 0; Step < 4000; ++Step)
		{
			const double X = Step * 0.37 - 600.0, Y = Step * 0.21 - 300.0;
			const double Value = Waver(X, Y);
			bBounded &= Value >= -1.0 && Value <= 1.0;
			bSame &= Value == Waver(X, Y);
			Least = std::min(Least, Value);
			Most = std::max(Most, Value);
		}
		Check(bBounded && bSame, "the nib's waver: never more than its share either way, and the same at the same place");
		Check(Most - Least > 1.0, "the nib's waver: it does waver");
	}

	void Steadiness()
	{
		// Ground changes hands at one end of a long front: the far end of the line, and its teeth, do not move.
		const FMadeUp Before({ "GGGPPP", "GGGPPP", "GGGPPP", "GGGPPP", "GGGPPP", "GGGPPP", "GGGPPP", "GGGPPP" }, { "GP" }, 'G');
		const FMadeUp After({ "GGGGPP", "GGGPPP", "GGGPPP", "GGGPPP", "GGGPPP", "GGGPPP", "GGGPPP", "GGGPPP" }, { "GP" }, 'G');
		FRules Rules;
		FDrawn One = Draw(Before, Rules), Other = Draw(After, Rules);
		FPenLook Look;
		FInkMesh InkOne, HaloOne, InkOther, HaloOther;
		LayPen(One.Chains, Look, 0, InkOne, HaloOne);
		LayPen(Other.Chains, Look, 0, InkOther, HaloOther);
		int Kept = 0, South = 0;
		for (const FTooth& Tooth : TeethOf(InkOne))
		{
			if (Tooth.Foot.Y < 30.0)
			{
				continue;
			}
			++South;
			for (const FTooth& Again : TeethOf(InkOther))
			{
				if (Apart(Tooth.Tip, Again.Tip) < 1e-9)
				{
					++Kept;
					break;
				}
			}
		}
		Check(South > 0 && Kept == South, "a front that moves at its north end: every tooth further south stands where it stood (" + std::to_string(Kept) + " of " + std::to_string(South) + ")");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The game's own map, from a file of its pieces (Tools/front/pieces_from_map.py): every piece of the front is in
	// exactly one line, lines end only at the coast, at another line or where the war stops, and a picture is drawn.

	bool RealMap(const std::string& File, const std::string& Picture)
	{
		std::ifstream In(File);
		if (!In)
		{
			std::printf("no such file: %s\n", File.c_str());
			return false;
		}
		std::vector<FBorderPiece> Pieces;
		std::map<std::string, int> Numbers;
		std::vector<int> HolderOf;
		std::vector<std::pair<std::string, std::string>> Wars;
		std::string Player, Line;
		while (std::getline(In, Line))
		{
			std::istringstream Words(Line);
			std::string Kind;
			Words >> Kind;
			if (Kind == "piece")
			{
				FBorderPiece& Piece = Pieces.emplace_back();
				Words >> Piece.A.X >> Piece.A.Y >> Piece.B.X >> Piece.B.Y >> Piece.Side.X >> Piece.Side.Y >> Piece.Here >> Piece.There;
			}
			else if (Kind == "holds")
			{
				int Province = 0;
				std::string Tag;
				Words >> Province >> Tag;
				if (!Numbers.count(Tag))
				{
					const int Number = static_cast<int>(Numbers.size());
					Numbers[Tag] = Number;
				}
				if (Province >= static_cast<int>(HolderOf.size()))
				{
					HolderOf.resize(static_cast<size_t>(Province) + 1, -1);
				}
				HolderOf[static_cast<size_t>(Province)] = Numbers[Tag];
			}
			else if (Kind == "war")
			{
				std::string One, Other;
				Words >> One >> Other;
				Wars.push_back({ One, Other });
			}
			else if (Kind == "player")
			{
				Words >> Player;
			}
		}
		FSides Sides;
		Sides.Reset(static_cast<int>(HolderOf.size()), static_cast<int>(Numbers.size()));
		Sides.HolderOf = HolderOf;
		for (const auto& War : Wars)
		{
			if (Numbers.count(War.first) && Numbers.count(War.second))
			{
				Sides.SetWar(Numbers[War.first], Numbers[War.second]);
				if (War.first == Player)
				{
					Sides.Foe[static_cast<size_t>(Numbers[War.second])] = 1;
				}
				if (War.second == Player)
				{
					Sides.Foe[static_cast<size_t>(Numbers[War.first])] = 1;
				}
			}
		}
		FRules Rules;
		FScratch Work;
		std::vector<FFrontPiece> Front;
		std::vector<FChain> Raw;
		PickFront(Pieces, Sides, Front);
		ChainFront(Front, Rules, Work, Raw);
		size_t InLines = 0, Closed = 0, Mine = 0;
		for (const FChain& Chain : Raw)
		{
			InLines += Chain.bClosed ? Chain.Points.size() : Chain.Points.size() - 1;
			Closed += Chain.bClosed ? 1 : 0;
			Mine += Chain.Kind == EKind::Mine ? 1 : 0;
		}
		std::printf("%s: %zu pieces of border, %zu of them front, in %zu lines (%zu the player's, %zu closed)\n", File.c_str(), Pieces.size(), Front.size(), Raw.size(), Mine, Closed);
		Check(InLines == Front.size(), File + ": every piece of the front is in one line, and only one (" + std::to_string(InLines) + " of " + std::to_string(Front.size()) + ")");
		std::vector<FChain> Chains;
		DrawFront(Pieces, Sides, Rules, Work, Front, Chains);
		CheckFinished(Chains, Rules, File);
		FPenLook Look;
		Look.ShadowX = 0.12;
		Look.ShadowY = -0.05;
		FInkMesh Ink, Halo;
		LayPen(Chains, Look, 0, Ink, Halo);
		CheckMesh(Ink, File + " ink");
		CheckMesh(Halo, File + " shadow");
		std::printf("  laid: %zu points and %zu triangles of ink, %zu points and %zu triangles of shadow\n", Ink.Vertices.size(), Ink.Triangles.size() / 3, Halo.Vertices.size(), Halo.Triangles.size() / 3);
		if (Picture.empty())
		{
			return true;
		}
		// A plain picture, for the eye: the ink's triangles and the shadow over a pale ground, in a box round the front
		// (or in FRONT_BOX="x0 y0 x1 y1 points-to-a-unit": about 3.6 points to a unit is the table at working height).
		double X0 = 1e300, Y0 = 1e300, X1 = -1e300, Y1 = -1e300;
		for (const FInkVertex& Vertex : Ink.Vertices)
		{
			X0 = std::min(X0, Vertex.X);
			Y0 = std::min(Y0, Vertex.Y);
			X1 = std::max(X1, Vertex.X);
			Y1 = std::max(Y1, Vertex.Y);
		}
		if (X1 < X0)
		{
			return true;
		}
		int Wide = 1400;
		double Scale = Wide / std::max(X1 - X0 + 20.0, 1.0);
		if (const char* Box = std::getenv("FRONT_BOX"))
		{
			double PerUnit = 0.0;
			std::istringstream Asked(Box);
			if (Asked >> X0 >> Y0 >> X1 >> Y1 >> PerUnit && PerUnit > 0.0 && X1 > X0 && Y1 > Y0)
			{
				X0 -= 10.0;
				Y0 -= 10.0;
				X1 -= 10.0;
				Y1 -= 10.0;
				Scale = PerUnit;
				Wide = static_cast<int>((X1 - X0 + 20.0) * Scale);
			}
		}
		const int High = static_cast<int>((Y1 - Y0 + 20.0) * Scale);
		std::vector<float> Shade(static_cast<size_t>(Wide) * static_cast<size_t>(High), 0.0f);
		std::vector<unsigned char> Inked(static_cast<size_t>(Wide) * static_cast<size_t>(High), 0);
		auto Fill = [&](const FInkMesh& Mesh, bool bInk)
		{
			for (size_t T = 0; T + 2 < Mesh.Triangles.size(); T += 3)
			{
				const FInkVertex* V[3] = { &Mesh.Vertices[static_cast<size_t>(Mesh.Triangles[T])], &Mesh.Vertices[static_cast<size_t>(Mesh.Triangles[T + 1])], &Mesh.Vertices[static_cast<size_t>(Mesh.Triangles[T + 2])] };
				double PX[3], PY[3];
				for (int K = 0; K < 3; ++K)
				{
					PX[K] = (V[K]->X - X0 + 10.0) * Scale;
					PY[K] = (V[K]->Y - Y0 + 10.0) * Scale;
				}
				const int MinX = std::max(0, static_cast<int>(std::floor(std::min({ PX[0], PX[1], PX[2] })))), MaxX = std::min(Wide - 1, static_cast<int>(std::ceil(std::max({ PX[0], PX[1], PX[2] }))));
				const int MinY = std::max(0, static_cast<int>(std::floor(std::min({ PY[0], PY[1], PY[2] })))), MaxY = std::min(High - 1, static_cast<int>(std::ceil(std::max({ PY[0], PY[1], PY[2] }))));
				const double Area = (PX[1] - PX[0]) * (PY[2] - PY[0]) - (PX[2] - PX[0]) * (PY[1] - PY[0]);
				if (std::fabs(Area) < 1e-12)
				{
					continue;
				}
				for (int Y = MinY; Y <= MaxY; ++Y)
				{
					for (int X = MinX; X <= MaxX; ++X)
					{
						const double SX = X + 0.5, SY = Y + 0.5;
						const double W0 = ((PX[1] - SX) * (PY[2] - SY) - (PX[2] - SX) * (PY[1] - SY)) / Area;
						const double W1 = ((PX[2] - SX) * (PY[0] - SY) - (PX[0] - SX) * (PY[2] - SY)) / Area;
						const double W2 = 1.0 - W0 - W1;
						if (W0 < 0.0 || W1 < 0.0 || W2 < 0.0)
						{
							continue;
						}
						const size_t At = static_cast<size_t>(Y) * static_cast<size_t>(Wide) + static_cast<size_t>(X);
						if (bInk)
						{
							Inked[At] = 1;
						}
						else
						{
							const float Alpha = static_cast<float>((W0 * V[0]->A + W1 * V[1]->A + W2 * V[2]->A) / 255.0);
							Shade[At] = std::max(Shade[At], Alpha);
						}
					}
				}
			}
		};
		Fill(Halo, false);
		Fill(Ink, true);
		std::ofstream Out(Picture, std::ios::binary);
		Out << "P6\n" << Wide << " " << High << "\n255\n";
		for (size_t At = 0; At < Shade.size(); ++At)
		{
			const double Paper[3] = { 214.0, 200.0, 164.0 };
			unsigned char Rgb[3];
			for (int K = 0; K < 3; ++K)
			{
				const double Shaded = Paper[K] * (1.0 - Shade[At]) + 18.0 * Shade[At];
				Rgb[K] = static_cast<unsigned char>(Inked[At] ? Look.Ink[1][K] : static_cast<unsigned char>(Shaded));
			}
			Out.write(reinterpret_cast<const char*>(Rgb), 3);
		}
		std::printf("  picture: %s (%d by %d)\n", Picture.c_str(), Wide, High);
		return true;
	}
}

int main(int ArgCount, char** Args)
{
	StraightFront();
	Staircase();
	Junctions();
	Split();
	Islands();
	Enclave();
	Gaps();
	Thinning();
	InkClock();
	Wavering();
	Steadiness();
	for (int Arg = 1; Arg < ArgCount; ++Arg)
	{
		const std::string File = Args[Arg];
		std::string Picture;
		if (Arg + 1 < ArgCount && std::string(Args[Arg + 1]).size() > 4 && std::string(Args[Arg + 1]).substr(std::string(Args[Arg + 1]).size() - 4) == ".ppm")
		{
			Picture = Args[++Arg];
		}
		RealMap(File, Picture);
	}
	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
