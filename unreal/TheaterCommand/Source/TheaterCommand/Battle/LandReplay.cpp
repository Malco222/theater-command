#include "Battle/LandReplay.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace LandReplay
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		/** The short way round from A to B (radians, -pi to pi). */
		double Turned(double A, double B)
		{
			double D = std::fmod(B - A, 2.0 * Pi);
			if (D > Pi) D -= 2.0 * Pi;
			if (D < -Pi) D += 2.0 * Pi;
			return D;
		}

		/**
		 * The field's woods, town and wet ground laid smooth, as few rectangles. Each look's cells are read as a value at
		 * their middles (1 that look, 0 not) and joined between them (bilinear), and the field is sampled Fine times finer
		 * than its cells: a sample takes the look whose value there is greatest, where that is at least a half. So a cell's
		 * middle is always its own look, the edge between two cells runs half way between their middles, and a corner is
		 * cut across instead of stepped (marching squares' edge, drawn in small squares). Then each row's runs, and a run
		 * carried up the rows above while they hold the same run.
		 */
		void Lay(const LandFeed::Field& F, std::vector<Patch>& Out)
		{
			const int Cols = F.Cols, Rows = F.Rows;
			if (Cols <= 0 || Rows <= 0 || F.Cell <= 0.0 || F.Ground.size() < static_cast<size_t>(Cols) * static_cast<size_t>(Rows))
			{
				return;
			}
			// (each ground word's look once, -1 for none)
			std::vector<int> Of(F.Grounds.size(), -1);
			for (size_t K = 0; K < F.Grounds.size(); ++K)
			{
				Look L = Look::Woods;
				if (LookOf(F.Grounds[K], L))
				{
					Of[K] = static_cast<int>(L);
				}
			}
			auto At = [&](int C, int R) -> int
			{
				C = std::clamp(C, 0, Cols - 1);
				R = std::clamp(R, 0, Rows - 1);
				const size_t G = F.Ground[static_cast<size_t>(R) * static_cast<size_t>(Cols) + static_cast<size_t>(C)];
				return G < Of.size() ? Of[G] : -1;
			};
			const int SubCols = Cols * Fine, SubRows = Rows * Fine;
			const double Sub = F.Cell / Fine;
			// The sampled looks, row by row (-1 none).
			auto Sampled = [&](int I, int J) -> int
			{
				const double U = (I + 0.5) / Fine - 0.5, V = (J + 0.5) / Fine - 0.5;
				const int C0 = static_cast<int>(std::floor(U)), R0 = static_cast<int>(std::floor(V));
				const double Fu = U - C0, Fv = V - R0;
				double Weight[3] = { 0.0, 0.0, 0.0 };
				const int Corner[4] = { At(C0, R0), At(C0 + 1, R0), At(C0, R0 + 1), At(C0 + 1, R0 + 1) };
				const double Share[4] = { (1.0 - Fu) * (1.0 - Fv), Fu * (1.0 - Fv), (1.0 - Fu) * Fv, Fu * Fv };
				for (int K = 0; K < 4; ++K)
				{
					if (Corner[K] >= 0)
					{
						Weight[Corner[K]] += Share[K];
					}
				}
				int Best = -1;
				for (int L = 0; L < 3; ++L)
				{
					if (Weight[L] >= 0.5 && (Best < 0 || Weight[L] > Weight[Best]))
					{
						Best = L;
					}
				}
				return Best;
			};
			struct Run
			{
				int C0, C1, Look, R0;
			};
			std::vector<Run> Open;
			auto Shut = [&](const Run& R, int RowEnd)
			{
				Patch P;
				P.X0 = R.C0 * Sub;
				P.X1 = std::min(F.W, R.C1 * Sub);
				P.Y0 = R.R0 * Sub;
				P.Y1 = std::min(F.H, RowEnd * Sub);
				P.Kind = static_cast<Look>(R.Look);
				if (P.X1 > P.X0 && P.Y1 > P.Y0)
				{
					Out.push_back(P);
				}
			};
			std::vector<int> Row(static_cast<size_t>(SubCols), -1);
			for (int J = 0; J < SubRows; ++J)
			{
				for (int I = 0; I < SubCols; ++I)
				{
					Row[static_cast<size_t>(I)] = Sampled(I, J);
				}
				std::vector<Run> Here;
				for (int I = 0; I < SubCols;)
				{
					const int L = Row[static_cast<size_t>(I)];
					int E = I + 1;
					while (E < SubCols && Row[static_cast<size_t>(E)] == L)
					{
						++E;
					}
					if (L >= 0)
					{
						Here.push_back({ I, E, L, J });
					}
					I = E;
				}
				std::vector<Run> Next;
				for (Run& H : Here)
				{
					auto Same = std::find_if(Open.begin(), Open.end(), [&](const Run& O) { return O.C0 == H.C0 && O.C1 == H.C1 && O.Look == H.Look; });
					if (Same != Open.end())
					{
						H.R0 = Same->R0;
						Open.erase(Same);
					}
					Next.push_back(H);
				}
				for (const Run& O : Open)
				{
					Shut(O, J);
				}
				Open = std::move(Next);
			}
			for (const Run& O : Open)
			{
				Shut(O, SubRows);
			}
		}
	}

	bool LookOf(const std::string& Ground, Look& Out)
	{
		if (Ground == "woods" || Ground == "park")
		{
			Out = Look::Woods;
			return true;
		}
		if (Ground == "buildings" || Ground == "suburb" || Ground == "industry" || Ground == "works" || Ground == "bunker")
		{
			Out = Look::Town;
			return true;
		}
		if (Ground == "water" || Ground == "marsh")
		{
			Out = Look::Wet;
			return true;
		}
		return false;
	}

	bool Build(const LandFeed::Recording& Rec, Map& Out, std::string& OutWhy)
	{
		if (Rec.Ground.W <= 0.0 || Rec.Ground.H <= 0.0)
		{
			OutWhy = "the recording has no field";
			return false;
		}
		if (Rec.Views.size() < 2)
		{
			OutWhy = "the recording has fewer than two views";
			return false;
		}
		Map M;
		M.W = Rec.Ground.W;
		M.H = Rec.Ground.H;
		M.T0 = Rec.Views.front().T;
		M.T1 = Rec.Views.back().T;
		if (M.T1 - M.T0 < 1.0)
		{
			OutWhy = "the battle lasted under a second";
			return false;
		}
		M.Me = Rec.bLive ? Rec.Me : -1;
		M.Title = Rec.Title;
		M.TownName = Rec.Ground.TownName;
		M.TownX = Rec.Ground.TownX;
		M.TownY = Rec.Ground.TownY;
		M.TownR = Rec.Ground.TownR;
		M.Adj[0] = Rec.Sides[0].Adj;
		M.Adj[1] = Rec.Sides[1].Adj;
		Lay(Rec.Ground, M.Patches);

		std::map<int, size_t> Index;
		for (const LandFeed::View& V : Rec.Views)
		{
			for (const LandFeed::Unit& U : V.Units)
			{
				if (U.Side < 0 || U.Side > 1)
				{
					continue;
				}
				auto Found = Index.find(U.Id);
				if (Found == Index.end())
				{
					Piece P;
					P.Id = U.Id;
					P.Side = U.Side;
					for (const LandFeed::Block& B : Rec.Blocks)
					{
						if (B.Id == U.Id)
						{
							if (B.W > 0.0 && B.D > 0.0)
							{
								P.W = B.W;
								P.D = B.D;
							}
							break;
						}
					}
					Found = Index.emplace(U.Id, M.Pieces.size()).first;
					M.Pieces.push_back(P);
				}
				Piece& P = M.Pieces[Found->second];
				// (a view twice at one time: the later stands)
				if (!P.Marks.empty() && P.Marks.back().T >= V.T)
				{
					P.Marks.pop_back();
				}
				Mark K;
				K.T = V.T;
				K.X = U.X;
				K.Y = U.Y;
				K.Face = U.Face;
				K.State = U.State;
				K.bSeen = M.Me < 0 || U.Side == M.Me || U.bKnown;
				P.Marks.push_back(K);
				if (K.bSeen && P.Kind.empty())
				{
					for (const LandFeed::Block& B : Rec.Blocks)
					{
						if (B.Id == U.Id)
						{
							P.Kind = B.Kind;
							break;
						}
					}
				}
			}
		}
		for (Piece& P : M.Pieces)
		{
			for (const Mark& K : P.Marks)
			{
				M.bUnfound = M.bUnfound || !K.bSeen;
				M.bFled = M.bFled || K.State == "gone";
				M.bFellBack = M.bFellBack || K.State == "withdrawn";
			}
			for (size_t K = 0; K < P.Marks.size(); ++K)
			{
				const Mark& A = P.Marks[K];
				if (P.Way.empty() || K + 1 == P.Marks.size() || std::hypot(A.X - P.Way.back().X, A.Y - P.Way.back().Y) >= WayStep)
				{
					// (the last mark replaces a kept point too near it, so the way ends where the block does)
					if (K + 1 == P.Marks.size() && P.Way.size() > 1 && std::hypot(A.X - P.Way.back().X, A.Y - P.Way.back().Y) < WayStep)
					{
						P.Way.pop_back();
					}
					P.Way.push_back({ A.T, A.X, A.Y });
				}
			}
		}
		if (M.Pieces.empty())
		{
			OutWhy = "the views hold no units";
			return false;
		}
		Out = std::move(M);
		return true;
	}

	void AddLine(Map& M, const std::vector<std::pair<double, double>>& Points, Edge Kind)
	{
		if (M.W <= 0.0 || M.H <= 0.0)
		{
			return;
		}
		Line Now;
		Now.Kind = Kind;
		auto Keep = [&](double X, double Y, bool bLast)
		{
			if (Now.Points.empty() || bLast || std::hypot(X - Now.Points.back().first, Y - Now.Points.back().second) >= LineStep)
			{
				if (bLast && Now.Points.size() > 1 && std::hypot(X - Now.Points.back().first, Y - Now.Points.back().second) < LineStep)
				{
					Now.Points.pop_back();
				}
				Now.Points.emplace_back(X, Y);
			}
		};
		auto Close = [&]()
		{
			if (Now.Points.size() >= 2)
			{
				M.Lines.push_back(Now);
			}
			Now.Points.clear();
		};
		for (size_t K = 1; K < Points.size(); ++K)
		{
			// (the segment clipped to the field: Liang and Barsky's way, its part from T0 to T1)
			const double X0 = Points[K - 1].first, Y0 = Points[K - 1].second, Dx = Points[K].first - X0, Dy = Points[K].second - Y0;
			double T0 = 0.0, T1 = 1.0;
			bool bIn = true;
			const double Ps[4] = { -Dx, Dx, -Dy, Dy }, Qs[4] = { X0, M.W - X0, Y0, M.H - Y0 };
			for (int E = 0; E < 4 && bIn; ++E)
			{
				if (Ps[E] == 0.0)
				{
					bIn = Qs[E] >= 0.0;
				}
				else
				{
					const double R = Qs[E] / Ps[E];
					if (Ps[E] < 0.0)
					{
						T0 = std::max(T0, R);
					}
					else
					{
						T1 = std::min(T1, R);
					}
					bIn = T0 <= T1;
				}
			}
			if (!bIn)
			{
				Close();
				continue;
			}
			const double Ax = X0 + Dx * T0, Ay = Y0 + Dy * T0, Bx = X0 + Dx * T1, By = Y0 + Dy * T1;
			if (T0 > 0.0 || Now.Points.empty())
			{
				Close();
				Now.Points.emplace_back(Ax, Ay);
			}
			const bool bLeaves = T1 < 1.0;
			Keep(Bx, By, bLeaves || K + 1 == Points.size());
			if (bLeaves)
			{
				Close();
			}
		}
		Close();
	}

	Pose PoseAt(const Piece& P, double T)
	{
		Pose Out;
		if (P.Marks.empty() || T < P.Marks.front().T - 1.0e-6)
		{
			return Out;
		}
		auto After = std::upper_bound(P.Marks.begin(), P.Marks.end(), T, [](double Time, const Mark& K) { return Time < K.T; });
		if (After == P.Marks.begin())
		{
			// (within a millionth of a second before its first view: at it)
			++After;
		}
		const Mark& A = *(After - 1);
		Out.bThere = true;
		Out.X = A.X;
		Out.Y = A.Y;
		Out.Face = A.Face;
		if (After != P.Marks.end())
		{
			const Mark& B = *After;
			const double Share = B.T > A.T ? std::clamp((T - A.T) / (B.T - A.T), 0.0, 1.0) : 0.0;
			Out.X = A.X + (B.X - A.X) * Share;
			Out.Y = A.Y + (B.Y - A.Y) * Share;
			Out.Face = A.Face + Turned(A.Face, B.Face) * Share;
		}
		Out.bSeen = A.bSeen;
		Out.bGone = A.State == "gone";
		Out.bOff = A.State == "withdrawn";
		Out.bRouting = A.State == "routing";
		return Out;
	}

	std::vector<std::pair<double, double>> WayTo(const Piece& P, double T)
	{
		std::vector<std::pair<double, double>> Out;
		const Pose Now = PoseAt(P, T);
		if (!Now.bThere)
		{
			return Out;
		}
		for (const Step& S : P.Way)
		{
			if (S.T >= T)
			{
				break;
			}
			Out.emplace_back(S.X, S.Y);
		}
		if (Out.empty() || std::hypot(Out.back().first - Now.X, Out.back().second - Now.Y) > 1.0e-6)
		{
			Out.emplace_back(Now.X, Now.Y);
		}
		return Out;
	}

	double Length(const std::vector<std::pair<double, double>>& Way)
	{
		double Sum = 0.0;
		for (size_t K = 1; K < Way.size(); ++K)
		{
			Sum += std::hypot(Way[K].first - Way[K - 1].first, Way[K].second - Way[K - 1].second);
		}
		return Sum;
	}

	std::vector<std::pair<double, double>> Corners(const Piece& P, const Pose& At)
	{
		// (forward along the facing, 0 north and clockwise; across it to the right)
		const double Fx = std::sin(At.Face), Fy = std::cos(At.Face);
		const double Rx = Fy, Ry = -Fx;
		const double Hw = P.W * 0.5, Hd = P.D * 0.5;
		return {
			{ At.X - Rx * Hw + Fx * Hd, At.Y - Ry * Hw + Fy * Hd },
			{ At.X + Rx * Hw + Fx * Hd, At.Y + Ry * Hw + Fy * Hd },
			{ At.X + Rx * Hw - Fx * Hd, At.Y + Ry * Hw - Fy * Hd },
			{ At.X - Rx * Hw - Fx * Hd, At.Y - Ry * Hw - Fy * Hd },
		};
	}

	void Fit::Into(double FieldW, double FieldH, double BoxX, double BoxY, double BoxW, double BoxH)
	{
		W = FieldW;
		H = FieldH;
		Scale = FieldW > 0.0 && FieldH > 0.0 ? std::max(0.0, std::min(BoxW / FieldW, BoxH / FieldH)) : 0.0;
		X0 = BoxX + (BoxW - FieldW * Scale) * 0.5;
		Y0 = BoxY + (BoxH - FieldH * Scale) * 0.5;
	}

	std::pair<double, double> Fit::On(double X, double Y) const
	{
		return { X0 + X * Scale, Y0 + (H - Y) * Scale };
	}

	void Player::Open()
	{
		bOpen = true;
		Age = 0.0;
	}

	void Player::Again()
	{
		Age = 0.0;
	}

	void Player::Close()
	{
		bOpen = false;
	}

	void Player::Tick(double Real)
	{
		if (bOpen)
		{
			Age = std::min(Length, Age + std::max(0.0, Real));
		}
	}

	double Player::BattleAt(const Map& M) const
	{
		const double Share = std::clamp((Age - Lead) / (Length - Lead), 0.0, 1.0);
		return M.T0 + (M.T1 - M.T0) * Share;
	}

	std::string Clock(double Seconds)
	{
		const long Whole = std::max(0L, std::lround(std::floor(Seconds)));
		char Text[32];
		std::snprintf(Text, sizeof(Text), "%ld:%02ld", Whole / 60, Whole % 60);
		return Text;
	}
}
