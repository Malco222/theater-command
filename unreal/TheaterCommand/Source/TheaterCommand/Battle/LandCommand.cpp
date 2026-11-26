#include "Battle/LandCommand.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace LandCommand
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		/** The setup step's clearance between two blocks, metres: the rules' own LAND.placeClear (src/battle/land/rules.ts;
		 * tests/land-drawup.test.ts holds the two together), and the draw-up's gap between two side by side (units.ts GAP). */
		constexpr double PlaceClear = 12.0;
		constexpr double DrawGap = 40.0;

		/** The smallest turn from A to B, -pi..pi. */
		double TurnOf(double A, double B)
		{
			double D = std::fmod(B - A, 2.0 * Pi);
			if (D > Pi) D -= 2.0 * Pi;
			if (D < -Pi) D += 2.0 * Pi;
			return D;
		}

		bool Off(const std::string& State)
		{
			return State == "gone" || State == "withdrawn";
		}

		/** How far a point is from a block's ground (its frontage W across its facing, D deep): 0 inside it. */
		double FromBlock(const Tag& B, double X, double Y)
		{
			const double Dx = X - B.X, Dy = Y - B.Y;
			// (the facing's own axes: across it to the right, and along it forward; 0 north, clockwise)
			const double S = std::sin(B.Face), C = std::cos(B.Face);
			const double Across = Dx * C - Dy * S, Along = Dx * S + Dy * C;
			const double Ox = std::max(0.0, std::fabs(Across) - B.W * 0.5), Oy = std::max(0.0, std::fabs(Along) - B.D * 0.5);
			return std::sqrt(Ox * Ox + Oy * Oy);
		}
	}

	std::string Number(double V)
	{
		if (!std::isfinite(V))
		{
			V = 0.0;
		}
		char Text[48];
		std::snprintf(Text, sizeof(Text), "%.2f", V);
		std::string Out(Text);
		while (!Out.empty() && Out.back() == '0') Out.pop_back();
		if (!Out.empty() && Out.back() == '.') Out.pop_back();
		return Out == "-0" ? "0" : Out;
	}

	void General::Begin(const LandFeed::Recording& Rec)
	{
		Blocks.clear();
		Shown.clear();
		Coming.clear();
		Hand.clear();
		Groups.clear();
		Me = Rec.bLive ? Rec.Me : -1;
		bSetup = Rec.bSetup;
		std::copy(Rec.Room, Rec.Room + 4, RoomAt);
		for (const LandFeed::Block& B : Rec.Blocks)
		{
			Words W;
			W.Id = B.Id;
			W.Side = B.Side;
			W.Name = B.Name;
			W.Short = B.Short;
			W.Parent = B.Parent;
			W.Kind = B.Kind;
			W.Team = B.Team;
			W.W = B.W;
			W.D = B.D;
			W.Figures0 = B.Figures0;
			W.Men0 = B.Men0;
			W.bVehicles = !B.Model.empty();
			Blocks.push_back(W);
		}
		std::sort(Blocks.begin(), Blocks.end(), [](const Words& A, const Words& B) { return A.Id < B.Id; });
		DrawnOf.assign(Blocks.size(), 0);
		Counted = 0;
		RatioOf[0] = RatioOf[1] = 0;
	}

	void General::CountDrawn(const LandFeed::Recording& Rec)
	{
		if (Rec.Tracks.size() == Counted && DrawnOf.size() == Blocks.size())
		{
			return;
		}
		// (a live battle's tracks only grow; placed again before the clock ran, they begin again: counted afresh)
		if (Rec.Tracks.size() < Counted || DrawnOf.size() != Blocks.size())
		{
			DrawnOf.assign(Blocks.size(), 0);
			Counted = 0;
		}
		for (size_t I = Counted; I < Rec.Tracks.size(); ++I)
		{
			const LandFeed::Track& Tr = Rec.Tracks[I];
			if (Tr.Role == "tank" || Tr.Role == "gun" || Tr.Role == "lorry")
			{
				continue;
			}
			auto It = std::lower_bound(Blocks.begin(), Blocks.end(), Tr.Block, [](const Words& W, int Key) { return W.Id < Key; });
			if (It != Blocks.end() && It->Id == Tr.Block)
			{
				++DrawnOf[static_cast<size_t>(It - Blocks.begin())];
			}
		}
		Counted = Rec.Tracks.size();
		// One figure for how many men, a side: its men on foot over the men drawn for them (the scale plan's one N a battle
		// makes the two the same).
		double Men[2] = { 0.0, 0.0 }, Drawn[2] = { 0.0, 0.0 };
		for (size_t I = 0; I < Blocks.size(); ++I)
		{
			const Words& W = Blocks[I];
			if (W.bVehicles || DrawnOf[I] <= 0 || W.Men0 <= 0 || W.Side < 0 || W.Side > 1) continue;
			Men[W.Side] += W.Men0;
			Drawn[W.Side] += DrawnOf[I];
		}
		for (int S = 0; S < 2; ++S)
		{
			RatioOf[S] = Drawn[S] > 0.0 ? std::max(1, static_cast<int>(std::lround(Men[S] / Drawn[S]))) : 0;
		}
	}

	const General::Words* General::WordsOf(int Id) const
	{
		auto It = std::lower_bound(Blocks.begin(), Blocks.end(), Id, [](const Words& W, int Key) { return W.Id < Key; });
		return It != Blocks.end() && It->Id == Id ? &*It : nullptr;
	}

	const Tag* General::TagOf(int Block) const
	{
		for (const Tag& T : Shown)
		{
			if (T.Block == Block) return &T;
		}
		return nullptr;
	}

	void General::Update(const LandFeed::Recording& Rec, double T)
	{
		Me = Rec.bLive ? Rec.Me : -1;
		bSetup = Rec.bSetup;
		std::copy(Rec.Room, Rec.Room + 4, RoomAt);
		CountDrawn(Rec);
		Shown.clear();
		Coming.clear();
		RidersNow.clear();
		bHelp = false;
		ReadAt = T;
		const LandFeed::View* A = Rec.ViewAt(T);
		if (!A)
		{
			return;
		}
		// (the corps still coming up from next door, a row a corps: its blocks walk on together)
		for (const LandFeed::Arrival& C : A->Coming)
		{
			auto It = std::find_if(Coming.begin(), Coming.end(), [&C](const Neighbour& N) { return N.Corps == C.Corps && N.Side == C.Side; });
			if (It == Coming.end())
			{
				Neighbour N;
				N.Corps = C.Corps;
				N.Side = C.Side;
				N.bMine = Me >= 0 && C.Side == Me;
				N.Kind = C.Kind;
				N.Men = N.bMine ? 0 : -1;
				N.In = std::max(0.0, C.At - T);
				N.Edge = C.Edge;
				N.Arrive = C.Arrive;
				N.X = C.X;
				N.Y = C.Y;
				if (const Words* W = WordsOf(C.Id)) N.Name = W->Parent.empty() ? W->Name : W->Parent;
				Coming.push_back(std::move(N));
				It = Coming.end() - 1;
			}
			++It->Units;
			if (C.Kind == "arm") It->Kind = "arm";
			It->In = std::min(It->In, std::max(0.0, C.At - T));
			if (It->bMine)
			{
				if (const Words* W = WordsOf(C.Id)) It->Men += W->Men0;
			}
		}
		std::sort(Coming.begin(), Coming.end(), [](const Neighbour& L, const Neighbour& R) { return L.In < R.In || (L.In == R.In && L.Corps < R.Corps); });
		// (the next view, a battle second on, where the screen has not reached it yet: each block eased toward it)
		const size_t At = static_cast<size_t>(A - Rec.Views.data());
		const LandFeed::View* B = At + 1 < Rec.Views.size() ? &Rec.Views[At + 1] : nullptr;
		const double K = B && B->T > A->T ? std::clamp((T - A->T) / (B->T - A->T), 0.0, 1.0) : 0.0;
		// (his own despatch riders, eased toward the next view as the blocks are; one gone by then stands where he was)
		for (const LandFeed::Rider& R : A->Riders)
		{
			if (Me < 0 || R.Side != Me) continue;
			LandFeed::Rider Now = R;
			if (B && K > 0.0)
			{
				for (const LandFeed::Rider& N : B->Riders)
				{
					if (N.Corps != R.Corps || N.Side != R.Side) continue;
					Now.X += (N.X - R.X) * K;
					Now.Y += (N.Y - R.Y) * K;
					break;
				}
			}
			RidersNow.push_back(std::move(Now));
		}
		for (const Neighbour& N : Coming) bHelp = bHelp || N.bMine;
		for (const LandFeed::Unit& U : A->Units)
		{
			Tag G;
			G.Block = U.Id;
			G.Side = U.Side;
			G.X = U.X;
			G.Y = U.Y;
			G.Face = U.Face;
			if (B && K > 0.0)
			{
				for (const LandFeed::Unit& N : B->Units)
				{
					if (N.Id != U.Id) continue;
					if (N.bKnown == U.bKnown && !Off(N.State))
					{
						G.X += (N.X - U.X) * K;
						G.Y += (N.Y - U.Y) * K;
						G.Face += TurnOf(U.Face, N.Face) * K;
					}
					break;
				}
			}
			if (const Words* W = WordsOf(U.Id))
			{
				G.Name = W->Name;
				G.Short = W->Short;
				G.Parent = W->Parent;
				G.Kind = W->Kind;
				G.Team = W->Team;
				G.W = W->W;
				G.D = W->D;
				G.Figures0 = W->Figures0;
				G.Men0 = W->Men0;
				const size_t Wi = static_cast<size_t>(W - Blocks.data());
				G.Drawn = Wi < DrawnOf.size() ? DrawnOf[Wi] : 0;
				G.Ratio = !W->bVehicles && G.Drawn > 0 && U.Side >= 0 && U.Side <= 1 ? RatioOf[U.Side] : 0;
			}
			G.Men = U.bKnown ? U.Men : -1;
			G.Str = U.Str;
			G.Mor = U.Mor;
			G.Figures = U.Figures;
			G.State = U.State;
			G.Act = U.Act;
			G.bMine = Me >= 0 && U.Side == Me;
			G.bKnown = U.bKnown;
			G.bTaken = U.bTaken;
			G.bHidden = U.bHidden;
			G.bVague = U.bVague;
			G.bSearch = U.bSearch;
			G.SearchX = U.SearchX;
			G.SearchY = U.SearchY;
			G.bCutter = G.bMine && U.bCutter;
			G.Smoke = G.bMine ? U.Smoke : -1;
			G.bWaits = G.bMine && U.bWaits;
			G.bWaitLate = G.bWaits && U.bWaitLate;
			G.bWaitFree = G.bWaits && U.bWaitFree;
			G.WaitDue = G.bWaits ? U.WaitDue : -1.0;
			G.OrderIn = U.OrderIn;
			G.OrderKind = U.OrderKind;
			G.OrderX = U.OrderX;
			G.OrderY = U.OrderY;
			G.OrderTarget = U.OrderTarget;
			G.Path = U.OrderPath;
			G.bOrderable = G.bMine && !Off(U.State) && U.State != "routing" && G.Kind != "fort";
			if (Off(U.State)) G.Says = Chip::Gone;
			else if (Me >= 0 && U.Side != Me) G.Says = U.bKnown ? Chip::Enemy : Chip::Unseen;
			else if (G.bMine && U.OrderIn >= 0.0) G.Says = Chip::Radio;
			else if (G.bMine && U.bTaken) G.Says = Chip::Yours;
			else G.Says = Chip::Staff;
			Shown.push_back(std::move(G));
		}
		std::sort(Shown.begin(), Shown.end(), [](const Tag& L, const Tag& R) { return L.Block < R.Block; });
		// (his orders the views have not caught up with yet: on the air, each to its point)
		Given.erase(std::remove_if(Given.begin(), Given.end(), [T](const Pending& P) { return P.Until <= T; }), Given.end());
		for (const Pending& P : Given)
		{
			Tag* G = nullptr;
			for (Tag& Each : Shown) if (Each.Block == P.Block) G = &Each;
			if (!G || !G->bMine || Off(G->State)) continue;
			if (P.Kind == "staff")
			{
				G->bTaken = false;
				G->Says = G->OrderIn >= 0.0 ? Chip::Radio : Chip::Staff;
				continue;
			}
			G->bTaken = true;
			G->Says = Chip::Radio;
			G->OrderKind = P.Kind == "line" ? "move" : P.Kind;
			G->OrderX = P.X;
			G->OrderY = P.Y;
			G->OrderTarget = P.Target;
			G->Path.clear();
		}
		// (what can no longer take his orders leaves his hand: fallen back off the field, gone, or running)
		Hand.erase(std::remove_if(Hand.begin(), Hand.end(), [this](int Id) { const Tag* G = TagOf(Id); return !G || !G->bOrderable; }), Hand.end());
		for (Tag& G : Shown)
		{
			G.bChosen = std::find(Hand.begin(), Hand.end(), G.Block) != Hand.end();
			G.Group = G.bMine ? GroupOf(G.Block) : 0;
		}
	}

	int General::MineAt(double X, double Y, double Reach) const
	{
		int Best = -1;
		double Near = Reach;
		for (const Tag& G : Shown)
		{
			if (!G.bOrderable) continue;
			const double D = FromBlock(G, X, Y);
			if (D <= Near)
			{
				Near = D;
				Best = G.Block;
			}
		}
		return Best;
	}

	int General::EnemyAt(double X, double Y, double Reach) const
	{
		int Best = -1;
		double Near = Reach;
		for (const Tag& G : Shown)
		{
			// (one hidden in woods, not yet found, is not there for him: he cannot point at what he cannot see)
			if (Me < 0 || G.Side == Me || Off(G.State) || G.bHidden) continue;
			const double D = FromBlock(G, X, Y);
			if (D <= Near)
			{
				Near = D;
				Best = G.Block;
			}
		}
		return Best;
	}

	void General::Pick(int Block, bool bAdd)
	{
		const Tag* G = TagOf(Block);
		if (!G || !G->bOrderable)
		{
			if (!bAdd) Hand.clear();
		}
		else if (bAdd)
		{
			auto It = std::find(Hand.begin(), Hand.end(), Block);
			if (It != Hand.end()) Hand.erase(It);
			else Hand.push_back(Block);
		}
		else
		{
			Hand.assign(1, Block);
		}
		for (Tag& T : Shown) T.bChosen = std::find(Hand.begin(), Hand.end(), T.Block) != Hand.end();
	}

	void General::Choose(const std::vector<int>& Picked, bool bAdd)
	{
		if (!bAdd) Hand.clear();
		for (int Id : Picked)
		{
			const Tag* G = TagOf(Id);
			if (G && G->bOrderable && std::find(Hand.begin(), Hand.end(), Id) == Hand.end()) Hand.push_back(Id);
		}
		for (Tag& T : Shown) T.bChosen = std::find(Hand.begin(), Hand.end(), T.Block) != Hand.end();
	}

	void General::Clear()
	{
		Hand.clear();
		for (Tag& T : Shown) T.bChosen = false;
	}

	std::vector<int> General::Chosen() const
	{
		std::vector<int> Out;
		for (int Id : Hand)
		{
			const Tag* G = TagOf(Id);
			if (G && G->bOrderable) Out.push_back(Id);
		}
		return Out;
	}

	int General::Next(bool bBack)
	{
		std::vector<int> Mine;
		for (const Tag& G : Shown)
		{
			if (G.bOrderable) Mine.push_back(G.Block);
		}
		if (Mine.empty()) return -1;
		const std::vector<int> Now = Chosen();
		const auto It = Now.empty() ? Mine.end() : std::find(Mine.begin(), Mine.end(), Now.front());
		size_t I = 0;
		if (It == Mine.end()) I = bBack ? Mine.size() - 1 : 0;
		else I = (static_cast<size_t>(It - Mine.begin()) + (bBack ? Mine.size() - 1 : 1)) % Mine.size();
		Pick(Mine[I], false);
		return Mine[I];
	}

	std::string General::Units(const std::vector<int>& Ids) const
	{
		std::string Out = "[";
		for (size_t K = 0; K < Ids.size(); ++K)
		{
			if (K) Out += ",";
			Out += std::to_string(Ids[K]);
		}
		return Out + "]";
	}

	std::string General::Click(double X, double Y, double Reach, bool bRun) const
	{
		const std::vector<int> Ids = Chosen();
		if (Ids.empty()) return std::string();
		const int Foe = EnemyAt(X, Y, Reach);
		if (Foe >= 0)
		{
			// (guns alone fire on it from where they stand; anything else closes with it)
			const bool bGuns = std::all_of(Ids.begin(), Ids.end(), [this](int Id) { const Tag* G = TagOf(Id); return G && G->Kind == "art"; });
			return bGuns ? FireOn(Foe) : Attack(Foe);
		}
		return Move(X, Y, bRun);
	}

	std::string General::Move(double X, double Y, bool bRun) const
	{
		const std::vector<int> Ids = Chosen();
		if (Ids.empty()) return std::string();
		return "{\"do\":\"move\",\"units\":" + Units(Ids) + ",\"x\":" + Number(X) + ",\"y\":" + Number(Y) + (bRun ? ",\"run\":true" : "") + "}";
	}

	std::string General::Attack(int Enemy) const
	{
		const std::vector<int> Ids = Chosen();
		if (Ids.empty() || Enemy < 0) return std::string();
		return "{\"do\":\"attack\",\"units\":" + Units(Ids) + ",\"target\":" + std::to_string(Enemy) + "}";
	}

	std::string General::FireOn(int Enemy) const
	{
		std::vector<int> Guns;
		for (int Id : Chosen())
		{
			const Tag* G = TagOf(Id);
			if (G && G->Kind == "art") Guns.push_back(Id);
		}
		if (Guns.empty() || Enemy < 0) return std::string();
		return "{\"do\":\"land:fire\",\"units\":" + Units(Guns) + ",\"target\":" + std::to_string(Enemy) + "}";
	}

	std::string General::FireInto(double X, double Y) const
	{
		const std::vector<int> Ids = Chosen();
		if (Ids.empty()) return std::string();
		return "{\"do\":\"land:fire\",\"units\":" + Units(Ids) + ",\"x\":" + Number(X) + ",\"y\":" + Number(Y) + "}";
	}

	std::string General::Line(double X0, double Y0, double X1, double Y1) const
	{
		const std::vector<int> Ids = Chosen();
		if (Ids.empty()) return std::string();
		return "{\"do\":\"line\",\"units\":" + Units(Ids) + ",\"x0\":" + Number(X0) + ",\"y0\":" + Number(Y0) + ",\"x1\":" + Number(X1) + ",\"y1\":" + Number(Y1) + "}";
	}

	std::string General::Hold() const
	{
		const std::vector<int> Ids = Chosen();
		return Ids.empty() ? std::string() : "{\"do\":\"hold\",\"units\":" + Units(Ids) + "}";
	}

	std::string General::FallBack() const
	{
		const std::vector<int> Ids = Chosen();
		return Ids.empty() ? std::string() : "{\"do\":\"withdraw\",\"units\":" + Units(Ids) + "}";
	}

	std::string General::ToStaff() const
	{
		std::vector<int> Taken;
		for (int Id : Chosen())
		{
			const Tag* G = TagOf(Id);
			if (G && G->bTaken) Taken.push_back(Id);
		}
		return Taken.empty() ? std::string() : "{\"do\":\"land:staff\",\"units\":" + Units(Taken) + "}";
	}

	std::vector<Ghost> General::LineUp(double X0, double Y0, double X1, double Y1) const
	{
		// As the rules line them up (src/battle/core/select.ts lineUp, 150 m apart for a click): across the line, on the
		// enemy's side of it, each keeping its place from left to right as they stand now, so none crosses his neighbour.
		std::vector<const Tag*> Us;
		for (int Id : Chosen())
		{
			if (const Tag* G = TagOf(Id)) Us.push_back(G);
		}
		std::vector<Ghost> Out;
		if (Us.empty()) return Out;
		double Long = std::hypot(X1 - X0, Y1 - Y0);
		if (Long < 30.0)
		{
			const double Wide = 150.0 * static_cast<double>(Us.size() - 1);
			X0 -= Wide / 2.0;
			X1 = X0 + Wide;
			Y1 = Y0;
			Long = Wide;
		}
		double Face = std::atan2(X1 - X0, Y1 - Y0) - Pi / 2.0;
		const double Toward = Me == 1 ? Pi : 0.0;
		if (std::cos(Face - Toward) < 0.0) Face += Pi;
		const double Ux = Long > 0.0 ? (X1 - X0) / Long : 1.0, Uy = Long > 0.0 ? (Y1 - Y0) / Long : 0.0;
		std::stable_sort(Us.begin(), Us.end(), [Ux, Uy](const Tag* A, const Tag* B) { return A->X * Ux + A->Y * Uy < B->X * Ux + B->Y * Uy; });
		const size_t N = Us.size();
		for (size_t I = 0; I < N; ++I)
		{
			const double F = N == 1 ? 0.5 : static_cast<double>(I) / static_cast<double>(N - 1);
			Out.push_back({ Us[I]->Block, X0 + (X1 - X0) * F, Y0 + (Y1 - Y0) * F, Face });
		}
		return Out;
	}

	int General::Cutter() const
	{
		for (int Id : Chosen())
		{
			const Tag* G = TagOf(Id);
			if (G && G->bCutter) return Id;
		}
		return -1;
	}

	std::vector<int> General::Smokers() const
	{
		std::vector<int> Out;
		for (int Id : Chosen())
		{
			const Tag* G = TagOf(Id);
			if (G && G->Smoke > 0) Out.push_back(Id);
		}
		return Out;
	}

	std::vector<int> General::Waiters() const
	{
		std::vector<int> Out;
		for (int Id : Chosen())
		{
			const Tag* G = TagOf(Id);
			const bool bFoot = G && (G->Kind == "inf" || G->Kind == "mtn" || G->Kind == "cav" || G->Kind == "eng");
			if (bFoot && !(G->bWaits && !G->bWaitFree)) Out.push_back(Id);
		}
		return Out;
	}

	std::string General::Wait() const
	{
		const std::vector<int> Ids = Waiters();
		return Ids.empty() ? std::string() : "{\"do\":\"land:wait\",\"units\":" + Units(Ids) + "}";
	}

	int General::RiderAt(double X, double Y, double Reach) const
	{
		int Best = -1;
		double Near = Reach;
		for (size_t K = 0; K < RidersNow.size(); ++K)
		{
			const double D = std::hypot(RidersNow[K].X - X, RidersNow[K].Y - Y);
			if (D <= Near)
			{
				Near = D;
				Best = static_cast<int>(K);
			}
		}
		return Best;
	}

	std::string General::Cut(double X, double Y) const
	{
		const int Id = Cutter();
		if (Id < 0) return std::string();
		return "{\"do\":\"land:cut\",\"units\":[" + std::to_string(Id) + "],\"x\":" + Number(X) + ",\"y\":" + Number(Y) + "}";
	}

	std::string General::SmokeLine(double X0, double Y0, double X1, double Y1) const
	{
		const std::vector<int> Ids = Smokers();
		if (Ids.empty()) return std::string();
		return "{\"do\":\"land:smoke\",\"units\":" + Units(Ids) + ",\"x0\":" + Number(X0) + ",\"y0\":" + Number(Y0) + ",\"x1\":" + Number(X1) + ",\"y1\":" + Number(Y1) + "}";
	}

	bool NearestOnLines(const std::vector<const std::vector<std::pair<double, double>>*>& Lines, double X, double Y, double Reach, double& OutX, double& OutY)
	{
		double Best = Reach * Reach;
		bool bFound = false;
		for (const std::vector<std::pair<double, double>>* Line : Lines)
		{
			if (!Line) continue;
			for (size_t K = 0; K < Line->size(); ++K)
			{
				const std::pair<double, double>& A = (*Line)[K];
				const std::pair<double, double>& B = (*Line)[K + 1 < Line->size() ? K + 1 : K];
				const double Dx = B.first - A.first, Dy = B.second - A.second, Long2 = Dx * Dx + Dy * Dy;
				double F = Long2 > 0.0 ? ((X - A.first) * Dx + (Y - A.second) * Dy) / Long2 : 0.0;
				F = std::max(0.0, std::min(1.0, F));
				const double Px = A.first + Dx * F, Py = A.second + Dy * F;
				const double D2 = (Px - X) * (Px - X) + (Py - Y) * (Py - Y);
				if (D2 <= Best)
				{
					Best = D2;
					OutX = Px;
					OutY = Py;
					bFound = true;
				}
			}
		}
		return bFound;
	}

	void General::Sent(const std::vector<int>& Ids, const std::string& Kind, const std::vector<std::pair<double, double>>& Points, int Target, double Until)
	{
		for (size_t K = 0; K < Ids.size(); ++K)
		{
			Given.erase(std::remove_if(Given.begin(), Given.end(), [&](const Pending& P) { return P.Block == Ids[K]; }), Given.end());
			Pending P;
			P.Block = Ids[K];
			P.Kind = Kind;
			if (!Points.empty())
			{
				const std::pair<double, double>& At = Points[std::min(K, Points.size() - 1)];
				P.X = At.first;
				P.Y = At.second;
			}
			else if (const Tag* G = TagOf(Ids[K]))
			{
				P.X = G->X;
				P.Y = G->Y;
			}
			// (an attack's route ends on the block it goes for)
			if (Target >= 0)
			{
				if (const Tag* Foe = TagOf(Target))
				{
					P.X = Foe->X;
					P.Y = Foe->Y;
				}
			}
			P.Target = Target;
			P.Until = Until;
			Given.push_back(P);
		}
	}

	bool General::MayStand(int Block, double X, double Y) const
	{
		const Tag* G = TagOf(Block);
		if (!bSetup || !G || !G->bOrderable) return false;
		const double Hw = G->W * 0.5, Hd = G->D * 0.5;
		return X - Hw >= RoomAt[0] && X + Hw <= RoomAt[2] && Y - Hd >= RoomAt[1] && Y + Hd <= RoomAt[3];
	}

	// ---- Drawing up while he places his army ----

	std::vector<Ghost> General::CarryAll(int Held, double X, double Y) const
	{
		std::vector<Ghost> Out;
		const Tag* H = TagOf(Held);
		if (!H) return Out;
		const std::vector<int> Ids = Chosen();
		const bool bAll = Ids.size() > 1 && std::find(Ids.begin(), Ids.end(), Held) != Ids.end();
		if (!bAll)
		{
			Out.push_back({ Held, X, Y, H->Face });
			return Out;
		}
		for (int Id : Ids)
		{
			if (const Tag* G = TagOf(Id)) Out.push_back({ Id, X + (G->X - H->X), Y + (G->Y - H->Y), G->Face });
		}
		return Out;
	}

	bool General::MayStandAll(const std::vector<Ghost>& Places) const
	{
		for (const Ghost& One : Places)
		{
			if (!MayStand(One.Block, One.X, One.Y)) return false;
		}
		return !Places.empty();
	}

	std::vector<Ghost> General::DrawnLine(double X0, double Y0, double X1, double Y1) const
	{
		std::vector<const Tag*> Us;
		for (int Id : Chosen())
		{
			if (const Tag* G = TagOf(Id)) Us.push_back(G);
		}
		std::vector<Ghost> Out;
		if (Us.empty()) return Out;
		// (a click, or a drag too short to say which way: a level line through the point)
		double Long = std::hypot(X1 - X0, Y1 - Y0);
		double Ux = 1.0, Uy = 0.0;
		if (Long < 30.0)
		{
			X0 = X1 = (X0 + X1) * 0.5;
			Y0 = Y1 = (Y0 + Y1) * 0.5;
			Long = 0.0;
		}
		else
		{
			Ux = (X1 - X0) / Long;
			Uy = (Y1 - Y0) / Long;
		}
		// Facing across the line, toward the enemy's side of it (as LineUp).
		double Face = std::atan2(Ux, Uy) - Pi / 2.0;
		const double Toward = Me == 1 ? Pi : 0.0;
		if (std::cos(Face - Toward) < 0.0) Face += Pi;
		std::stable_sort(Us.begin(), Us.end(), [Ux, Uy](const Tag* A, const Tag* B) { return A->X * Ux + A->Y * Uy < B->X * Ux + B->Y * Uy; });
		// How near two neighbours may stand along it: the rules keep every two blocks apart by half their frontages
		// across the field or half their depths up it, and the setup step's clearance more (sim.ts standWhy); the
		// draw-up's own gap between two side by side.
		const size_t N = Us.size();
		std::vector<double> At(N, 0.0);
		for (size_t I = 1; I < N; ++I)
		{
			const double Across = ((Us[I - 1]->W + Us[I]->W) * 0.5 + DrawGap) / std::max(std::fabs(Ux), 1e-6);
			const double Up = ((Us[I - 1]->D + Us[I]->D) * 0.5 + PlaceClear + 8.0) / std::max(std::fabs(Uy), 1e-6);
			At[I] = At[I - 1] + std::min(Across, Up);
		}
		const double Needs = At[N - 1];
		// From end to end of his line where it is long enough for them; else about its middle.
		const double Scale = Needs > 0.0 && Long > Needs ? Long / Needs : 1.0;
		const double Start = (Long - Needs * Scale) * 0.5;
		for (size_t I = 0; I < N; ++I)
		{
			const double Along = Start + At[I] * Scale;
			Out.push_back({ Us[I]->Block, X0 + Ux * Along, Y0 + Uy * Along, Face });
		}
		return Out;
	}

	std::string General::PlaceMany(const std::vector<Ghost>& Places)
	{
		if (Places.empty()) return std::string();
		std::string Out = "[";
		for (size_t K = 0; K < Places.size(); ++K)
		{
			char Face[32];
			std::snprintf(Face, sizeof(Face), "%.4f", std::isfinite(Places[K].Face) ? Places[K].Face : 0.0);
			if (K) Out += ",";
			Out += "[" + std::to_string(Places[K].Block) + "," + Number(Places[K].X) + "," + Number(Places[K].Y) + "," + Face + "]";
		}
		return Out + "]";
	}

	std::string General::DrawUp(const std::string& Kind) const
	{
		return "[\"" + Kind + "\"," + Units(Chosen()) + "]";
	}

	// ---- Groups on the number keys ----

	General::Tied General::Tie(int Num)
	{
		Tied Out;
		const std::vector<int> Ids = Chosen();
		if (Num < 1 || Num > 9 || Ids.empty()) return Out;
		for (int Id : Ids)
		{
			const auto It = std::find_if(Groups.begin(), Groups.end(), [Id](const std::pair<int, int>& G) { return G.first == Id; });
			if (It == Groups.end())
			{
				Groups.push_back({ Id, Num });
			}
			else
			{
				if (It->second != Num)
				{
					++Out.Moved;
					if (std::find(Out.From.begin(), Out.From.end(), It->second) == Out.From.end()) Out.From.push_back(It->second);
				}
				It->second = Num;
			}
			++Out.Count;
		}
		std::sort(Out.From.begin(), Out.From.end());
		for (Tag& T : Shown) T.Group = T.bMine ? GroupOf(T.Block) : 0;
		return Out;
	}

	bool General::Take(int Num, bool bAdd)
	{
		std::vector<int> Ids;
		for (const std::pair<int, int>& G : Groups)
		{
			const Tag* T = TagOf(G.first);
			if (G.second == Num && T && T->bOrderable) Ids.push_back(G.first);
		}
		if (Ids.empty()) return false;
		Choose(Ids, bAdd);
		return true;
	}

	int General::GroupOf(int Block) const
	{
		for (const std::pair<int, int>& G : Groups)
		{
			if (G.first == Block) return G.second;
		}
		return 0;
	}

	bool General::GroupAt(int Num, double& OutX, double& OutY) const
	{
		double Sx = 0.0, Sy = 0.0;
		int Count = 0;
		for (const std::pair<int, int>& G : Groups)
		{
			const Tag* T = TagOf(G.first);
			if (G.second != Num || !T || !T->bOrderable) continue;
			Sx += T->X;
			Sy += T->Y;
			++Count;
		}
		if (Count == 0) return false;
		OutX = Sx / Count;
		OutY = Sy / Count;
		return true;
	}

	std::vector<const Tag*> General::OnMap() const
	{
		std::vector<const Tag*> Out;
		for (const Tag& G : Shown)
		{
			// (as EnemyAt: an enemy hidden in woods, not yet found, is not there for him)
			if (Off(G.State) || (Me >= 0 && G.Side != Me && G.bHidden)) continue;
			Out.push_back(&G);
		}
		return Out;
	}

	std::string General::TiedWords(const Tied& What, int Num)
	{
		if (What.Count <= 0) return std::string();
		// (counts in words to ten, the group's number a figure, as its key)
		auto Said = [](int Many, bool bHead)
		{
			static const char* const Words[] = { "none", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten" };
			std::string Word = Many >= 0 && Many <= 10 ? Words[Many] : std::to_string(Many);
			if (bHead && !Word.empty() && Word[0] >= 'a' && Word[0] <= 'z') Word[0] = static_cast<char>(Word[0] - 'a' + 'A');
			return Word;
		};
		std::string Out = Said(What.Count, true) + (What.Count == 1 ? " unit" : " units") + " tied to " + std::to_string(Num);
		if (What.Moved > 0 && !What.From.empty())
		{
			Out += "; " + Said(What.Moved, false) + " of them " + (What.Moved == 1 ? "leaves " : "leave ");
			for (size_t K = 0; K < What.From.size(); ++K)
			{
				if (K) Out += K + 1 == What.From.size() ? " and " : ", ";
				Out += std::to_string(What.From[K]);
			}
		}
		return Out + ".";
	}
}
