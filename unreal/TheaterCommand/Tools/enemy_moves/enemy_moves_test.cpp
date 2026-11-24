// The enemy's month near you, its cloud test (Tools/enemy_moves/run.sh): the game's own Campaign/EnemyMoves, no engine.
// Given a file of arrows (one a line: kind fromX fromY toX toY war away taken siege corps hasFrom), it also writes what
// the table would lay for them, whole, as JSON for Tools/enemy_moves/picture.py.

#include "Campaign/EnemyMoves.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace TheaterEnemyMoves;

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

	FArrow Make(EKind Kind, double FromX, double FromY, double ToX, double ToY)
	{
		FArrow Arrow;
		Arrow.Kind = Kind;
		Arrow.From = FPoint{ FromX, FromY };
		Arrow.To = FPoint{ ToX, ToY };
		return Arrow;
	}

	size_t Pieces(const FLayer& Layer)
	{
		return Layer.Strokes.size() + Layer.Triangles.size();
	}

	/** The farthest any laid point lies from P. */
	double Farthest(const FDrawing& Drawing, const FPoint& P)
	{
		double Far = 0.0;
		for (const FLayer* Layer : { &Drawing.Under, &Drawing.Over })
		{
			for (const FStroke& Stroke : Layer->Strokes)
			{
				for (const FPoint& Q : Stroke.Points)
				{
					Far = std::max(Far, std::hypot(Q.X - P.X, Q.Y - P.Y));
				}
			}
			for (const FTriangle& T : Layer->Triangles)
			{
				for (const FPoint& Q : { T.A, T.B, T.C })
				{
					Far = std::max(Far, std::hypot(Q.X - P.X, Q.Y - P.Y));
				}
			}
		}
		return Far;
	}

	/** The point of the arrow's head: the head triangle's first corner in the Over layer. */
	bool HeadTip(const FDrawing& Drawing, FPoint& Out)
	{
		for (const FTriangle& T : Drawing.Over.Triangles)
		{
			Out = T.A;
			return true;
		}
		return false;
	}

	void Write(const FDrawing& Drawing, const std::string& File)
	{
		std::ofstream Out(File);
		auto Ink = [&Out](const FInk& I) { Out << "[" << int(I.R) << "," << int(I.G) << "," << int(I.B) << "," << int(I.A) << "]"; };
		auto Layer = [&](const FLayer& L)
		{
			Out << "{\"strokes\":[";
			for (size_t K = 0; K < L.Strokes.size(); ++K)
			{
				const FStroke& S = L.Strokes[K];
				Out << (K ? "," : "") << "{\"half\":" << S.Half << ",\"closed\":" << (S.bClosed ? "true" : "false") << ",\"ink\":";
				Ink(S.Ink);
				Out << ",\"points\":[";
				for (size_t J = 0; J < S.Points.size(); ++J)
				{
					Out << (J ? "," : "") << "[" << S.Points[J].X << "," << S.Points[J].Y << "]";
				}
				Out << "]}";
			}
			Out << "],\"triangles\":[";
			for (size_t K = 0; K < L.Triangles.size(); ++K)
			{
				const FTriangle& T = L.Triangles[K];
				Out << (K ? "," : "") << "{\"ink\":";
				Ink(T.Ink);
				Out << ",\"points\":[[" << T.A.X << "," << T.A.Y << "],[" << T.B.X << "," << T.B.Y << "],[" << T.C.X << "," << T.C.Y << "]]}";
			}
			Out << "]}";
		};
		Out << "{\"under\":";
		Layer(Drawing.Under);
		Out << ",\"over\":";
		Layer(Drawing.Over);
		Out << "}\n";
	}
}

int main(int Count, char** Args)
{
	// Growing: nothing at 0, something early, all of it at 1; the head's point moves out along the way and ends short of the place.
	{
		const std::vector<FArrow> One{ Make(EKind::Move, 0.0, 0.0, 65.0, 0.0) };
		FDrawing D;
		Lay(One, 0.0, D);
		Check(D.IsEmpty(), "nothing is laid before the arrow begins to grow");
		Lay(One, 0.2, D);
		FPoint Early;
		Check(!D.IsEmpty() && HeadTip(D, Early), "a fifth grown, a shaft and a head are laid");
		Lay(One, 1.0, D);
		FPoint Whole;
		Check(HeadTip(D, Whole), "whole, the head is laid");
		Check(Whole.X > Early.X + 20.0, "the head's point moves out as the arrow grows");
		Check(std::fabs(Whole.X - (65.0 - 4.5)) < 0.5 && std::fabs(Whole.Y) < 0.5, "whole, the point stops short of the place it came to, on its line");
		Check(D.Under.Strokes.size() == D.Over.Strokes.size() && D.Under.Triangles.size() == D.Over.Triangles.size(), "every ink has its dark edge under it");
		Check(D.Under.Strokes[0].Half > D.Over.Strokes[0].Half, "the edge shows round the ink");
		Check(D.Under.Strokes[0].Lift < D.Over.Strokes[0].Lift, "the edge lies under the ink");
		Check(Grown(0.0) == 0.0 && Grown(GrowSeconds) == 1.0 && Grown(GrowSeconds * 0.5) > 0.5, "the growth is eased: fast at first, whole at its time");
	}
	// Two arrows on one road bow apart.
	{
		const std::vector<FArrow> Two{ Make(EKind::Move, 0.0, 0.0, 60.0, 0.0), Make(EKind::Move, 0.0, 0.0, 60.0, 0.0) };
		FDrawing D;
		Lay(Two, 1.0, D);
		const FStroke& A = D.Over.Strokes[0];
		const FStroke& B = D.Over.Strokes[1];
		Check(A.Points[A.Points.size() / 2].Y * B.Points[B.Points.size() / 2].Y < 0.0, "two arrows between the same places bow to either side");
	}
	// An attack: its mark lands only as the arrow arrives; lost red and filled, held brass; a landing is the mark alone.
	{
		FArrow Hit = Make(EKind::Attack, 0.0, 0.0, 70.0, 10.0);
		Hit.bTaken = true;
		FDrawing D;
		Lay({ Hit }, 0.5, D);
		const size_t Half = Pieces(D.Under) + Pieces(D.Over);
		Lay({ Hit }, 1.0, D);
		Check(Pieces(D.Under) + Pieces(D.Over) > Half + 4, "the battle mark is laid as the arrow arrives, not before");
		bool bFilled = false, bRed = false;
		for (const FTriangle& T : D.Under.Triangles)
		{
			bFilled = bFilled || (T.Ink.R > 200 && T.Ink.A < 100);
		}
		for (const FStroke& S : D.Over.Strokes)
		{
			bRed = bRed || (S.bClosed && S.Ink.R > 200 && S.Ink.G < 60);
		}
		Check(bFilled && bRed, "a place lost: a red ring, faintly filled");
		Hit.bTaken = false;
		Lay({ Hit }, 1.0, D);
		bool bBrass = false;
		for (const FStroke& S : D.Over.Strokes)
		{
			bBrass = bBrass || (S.bClosed && S.Ink.R == 224 && S.Ink.G == 184);
		}
		Check(bBrass, "a place that held: a brass ring");
		FArrow Landing = Hit;
		Landing.bFrom = false;
		Lay({ Landing }, 1.0, D);
		Check(D.Over.Triangles.empty() && Farthest(D, Landing.To) < 10.0, "where it came from is not known: the mark alone, round the place");
	}
	// A move away from him is fainter; his own retreat is dashed; a peace-time army is amber.
	{
		FArrow Toward = Make(EKind::Move, 0.0, 0.0, 65.0, 0.0), Away = Toward, Peace = Toward, Fell = Toward;
		Away.bAway = true;
		Peace.bWar = false;
		Fell.Kind = EKind::Fell;
		FDrawing D;
		Lay({ Toward }, 1.0, D);
		const FInk WarInk = D.Over.Strokes[0].Ink;
		const double WarHalf = D.Over.Strokes[0].Half;
		Lay({ Away }, 1.0, D);
		Check(D.Over.Strokes[0].Ink.A < WarInk.A && D.Over.Strokes[0].Half < WarHalf, "an army going away is drawn fainter and thinner");
		Lay({ Peace }, 1.0, D);
		Check(D.Over.Strokes[0].Ink.G > WarInk.G + 60, "a country at peace with him is amber, not red");
		Lay({ Fell }, 1.0, D);
		Check(D.Over.Strokes.size() > 4, "his own corps falling back: dashes");
	}
	// The frame holds every point; the zoom stays in the watch's range and grows with the reach.
	{
		const FFrame F = Frame({ FPoint{ 0.0, 0.0 }, FPoint{ 100.0, 0.0 }, FPoint{ 50.0, 80.0 } });
		Check(F.bValid && std::fabs(F.Middle.X - 50.0) < 1e-9 && std::fabs(F.Middle.Y - 40.0) < 1e-9, "the frame's middle is the middle of the points' box");
		Check(F.Reach >= std::hypot(50.0, 40.0) - 1e-9, "the frame reaches the farthest point");
		Check(!Frame({}).bValid, "no points, no frame");
		const double Near = ZoomFor(20.0, 10.0, 750.0, 30000.0), Wide = ZoomFor(250.0, 10.0, 750.0, 30000.0), Huge = ZoomFor(5000.0, 10.0, 750.0, 30000.0);
		Check(Near >= 0.40 && Huge <= 0.66 && Wide > Near, "the zoom grows with the reach, within 0.40 to 0.66");
		std::printf("zoom for a reach of 20, 65, 120, 250 map units: %.3f %.3f %.3f %.3f\n", Near, ZoomFor(65.0, 10.0, 750.0, 30000.0), ZoomFor(120.0, 10.0, 750.0, 30000.0), Wide);
	}
	// The pauses: an attack longer than moves alone, a lost place longer still, raids keep their own time.
	{
		Check(Pause(1, 0, 0, 0.0, 0.0) > Pause(0, 0, 3, 0.0, 0.0), "an attack is looked at longer than moves");
		Check(Pause(1, 1, 0, 0.0, 0.0) > Pause(1, 0, 0, 0.0, 0.0), "a place lost longer than one held");
		Check(Pause(3, 1, 2, 0.0, 0.0) <= 6.0, "even a heavy month moves on within six seconds");
		Check(Pause(0, 0, 1, 6.0, 0.0) == 6.0, "a raid keeps the time it takes to fly");
		std::printf("pauses: moves 1 %.1f s, moves 5 %.1f s, attack held %.1f s, attack lost %.1f s, three attacks %.1f s\n", Pause(0, 0, 1, 0, 0), Pause(0, 0, 5, 0, 0), Pause(1, 0, 0, 0, 0), Pause(1, 1, 0, 0, 0), Pause(3, 1, 0, 0, 0));
	}
	// Arrows from a file, laid whole, for the picture.
	if (Count >= 3)
	{
		std::ifstream In(Args[1]);
		std::vector<FArrow> Arrows;
		std::string Line;
		while (std::getline(In, Line))
		{
			std::istringstream Words(Line);
			int Kind = 0, War = 1, Away = 0, Taken = 0, Siege = 0, Corps = 1, HasFrom = 1;
			FArrow Arrow;
			if (Words >> Kind >> Arrow.From.X >> Arrow.From.Y >> Arrow.To.X >> Arrow.To.Y >> War >> Away >> Taken >> Siege >> Corps >> HasFrom)
			{
				Arrow.Kind = static_cast<EKind>(Kind);
				Arrow.bWar = War != 0;
				Arrow.bAway = Away != 0;
				Arrow.bTaken = Taken != 0;
				Arrow.bSiege = Siege != 0;
				Arrow.Corps = Corps;
				Arrow.bFrom = HasFrom != 0;
				Arrows.push_back(Arrow);
			}
		}
		FDrawing D;
		Lay(Arrows, Count >= 4 ? std::atof(Args[3]) : 1.0, D);
		Write(D, Args[2]);
		std::printf("laid %zu arrows: %zu strokes, %zu triangles\n", Arrows.size(), D.Under.Strokes.size() + D.Over.Strokes.size(), D.Under.Triangles.size() + D.Over.Triangles.size());
	}
	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
