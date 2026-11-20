#include "Battle/AirShow.h"

#include <cstdio>

#include <algorithm>
#include <cmath>

namespace AirShow
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Gee = 9.81;
		constexpr double Deg = Pi / 180.0;

		double Wrap(double A)
		{
			while (A > Pi) A -= 2.0 * Pi;
			while (A <= -Pi) A += 2.0 * Pi;
			return A;
		}
		double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : V > Hi ? Hi : V; }
		double Lerp(double A, double B, double T) { return A + (B - A) * T; }
		double Len(const Vec& V) { return std::sqrt(V.Dot(V)); }
		double Flat(const Vec& V) { return std::sqrt(V.X * V.X + V.Y * V.Y); }
		Vec Norm(const Vec& V, const Vec& Otherwise = Vec(0.0, 1.0, 0.0))
		{
			const double L = Len(V);
			return L > 1e-9 ? V * (1.0 / L) : Otherwise;
		}
		/** Along a heading (clockwise from north) and a climb. */
		Vec Ahead(double Hdg, double Gam = 0.0) { return Vec(std::sin(Hdg) * std::cos(Gam), std::cos(Hdg) * std::cos(Gam), std::sin(Gam)); }
		/** To the right of a heading, level. */
		Vec RightOf(double Hdg) { return Vec(std::cos(Hdg), -std::sin(Hdg), 0.0); }
		const Vec Up(0.0, 0.0, 1.0);

		bool Has(const std::vector<std::string>& List, const std::string& Word) { return std::find(List.begin(), List.end(), Word) != List.end(); }
	}

	Shape ShapeOf(const std::string& Model, bool bFighter)
	{
		// The record's measurements; engines numbered from the port wing tip, as the B-17's crews counted them; the gun
		// stations where each gunner stood. Approximate until the model files carry their own sockets.
		Shape S;
		auto Engines = [&S](std::initializer_list<double> Out, double X, double Z)
		{
			for (const double Y : Out) S.Engines.push_back(Vec(X, Y, Z));
		};
		auto Bomber = [&S](double Long, double High)
		{
			const double L = Long * 0.5;
			S.Stations["nose"] = Vec(L * 0.95, 0.0, 0.1 * High);
			S.Stations["chin"] = Vec(L * 0.85, 0.0, -0.35 * High);
			S.Stations["top"] = Vec(L * 0.45, 0.0, 0.75 * High);
			S.Stations["radio"] = Vec(-L * 0.05, 0.0, 0.55 * High);
			S.Stations["ball"] = Vec(-L * 0.15, 0.0, -0.55 * High);
			S.Stations["waistL"] = Vec(-L * 0.4, -0.45 * High, 0.15 * High);
			S.Stations["waistR"] = Vec(-L * 0.4, 0.45 * High, 0.15 * High);
			S.Stations["tail"] = Vec(-L * 0.97, 0.0, 0.15 * High);
		};
		if (Model == "B17") { S.Span = 31.6; S.Long = 22.7; Engines({ -9.2, -4.9, 4.9, 9.2 }, 1.6, 0.2); Bomber(22.7, 2.2); }
		else if (Model == "Lancaster") { S.Span = 31.1; S.Long = 21.2; Engines({ -8.9, -4.2, 4.2, 8.9 }, 1.8, 0.3); Bomber(21.2, 2.1); }
		else if (Model == "He111") { S.Span = 22.6; S.Long = 16.4; Engines({ -3.4, 3.4 }, 1.6, 0.0); Bomber(16.4, 1.7); }
		else if (Model == "G4M") { S.Span = 25.0; S.Long = 20.0; Engines({ -3.9, 3.9 }, 1.8, 0.1); Bomber(20.0, 1.8); }
		else if (Model == "B5N" || Model == "B6N") { S.Span = 15.5; S.Long = 10.3; Engines({ 0.0 }, 4.4, 0.0); Bomber(10.3, 1.2); }
		else if (Model == "D3A" || Model == "D4Y") { S.Span = 14.4; S.Long = 10.2; Engines({ 0.0 }, 4.4, 0.0); Bomber(10.2, 1.2); S.bDive = true; }
		else if (Model == "SBD" || Model == "SB2C" || Model == "Skua") { S.Span = 12.7; S.Long = 10.1; Engines({ 0.0 }, 4.3, 0.0); Bomber(10.1, 1.2); S.bDive = true; }
		else if (Model == "TBD" || Model == "TBF" || Model == "Swordfish" || Model == "Barracuda") { S.Span = 15.2; S.Long = 10.7; Engines({ 0.0 }, 4.5, 0.0); Bomber(10.7, 1.2); }
		else if (Model == "Stuka") { S.Span = 13.8; S.Long = 11.1; Engines({ 0.0 }, 4.6, 0.0); Bomber(11.1, 1.2); S.bDive = true; }
		// (the plain carrier models, "Dive:ITA" or "Torpedo:FRA": single-engined, as every carrier aircraft of the war was)
		else if (Model.rfind("Dive", 0) == 0) { S.Span = 13.0; S.Long = 10.2; Engines({ 0.0 }, 4.3, 0.0); Bomber(10.2, 1.2); S.bDive = true; }
		else if (Model.rfind("Torpedo", 0) == 0) { S.Span = 15.0; S.Long = 10.6; Engines({ 0.0 }, 4.5, 0.0); Bomber(10.6, 1.2); }
		else if (!bFighter) { S.Span = 19.8; S.Long = 16.0; Engines({ -3.0, 3.0 }, 1.5, 0.0); Bomber(16.0, 1.6); }
		else
		{
			// Fighters: one engine in the nose.
			S.Span = Model == "Zero" ? 12.0 : Model == "Warhawk" ? 11.4 : Model == "Spitfire" ? 11.2 : Model == "Hurricane" ? 12.2 : Model == "Mustang" ? 11.3 : Model == "Wildcat" ? 11.6 : Model == "Bf109" ? 9.9 : 10.6;
			S.Long = Model == "Bf109" ? 8.6 : Model == "Zero" ? 9.1 : 9.4;
			Engines({ 0.0 }, S.Long * 0.42, 0.0);
		}
		if (!S.Stations.count("cockpit")) S.Stations["cockpit"] = Vec(bFighter ? -0.5 : S.Long * 0.35, 0.0, 1.0);
		return S;
	}

	Show::Show(uint64_t Seed) : Roll(Seed) {}

	void Show::SetBoard(double InTargetX, double InTargetY, double InFlakR, double InScale)
	{
		TargetX = InTargetX;
		TargetY = InTargetY;
		FlakR = InFlakR;
		Scale = InScale > 0.0 ? InScale : 2.0;
	}

	void Show::SetLoadout(const std::string& Nation, bool bFighter, const Loadout& Guns)
	{
		Loadouts[Nation + (bFighter ? ":ftr" : ":bmr")] = Guns;
	}

	double Show::GroundAt(double X, double Y) const
	{
		return Ground ? std::max(0.0, Ground(X, Y)) : 0.0;
	}

	DeckSpan Show::DeckHere(double X, double Y) const
	{
		if (DeckReach > 0.0 && std::hypot(X - Eye.X, Y - Eye.Y) > DeckReach)
		{
			return DeckSpan();
		}
		if (DeckAt)
		{
			return DeckAt(X, Y);
		}
		DeckSpan Everywhere;
		if (CloudTop > 0.0)
		{
			Everywhere.Foot = std::max(0.0, CloudTop - 500.0);
			Everywhere.Hide = CloudTop;
		}
		return Everywhere;
	}

	double Show::DeckFor(const Vec& At) const
	{
		if (CloudTop <= 0.0 && !DeckAt)
		{
			return 0.0;
		}
		return DeckBase > 0.0 && At.Z <= DeckBase ? 0.0 : 1.0;
	}

	bool Show::IntoDeck(double& Deck, const Vec& At) const
	{
		if (Deck <= 0.0)
		{
			return false;
		}
		if (DeckBase > 0.0 && At.Z <= DeckBase)
		{
			// (under the deck's laid base: no tile reaches down here, on down to the ground or the sea for good)
			Deck = 0.0;
			return false;
		}
		const DeckSpan Here = DeckHere(At.X, At.Y);
		if (Here.Hide <= 0.0 || At.Z <= Here.Foot)
		{
			// (a hole, or under the tile over it: on down; it may yet fall into a lower tile beside it, one lifted less
			// over high ground)
			return false;
		}
		return At.Z < Here.Hide - std::min(150.0, 0.5 * (Here.Hide - Here.Foot));
	}

	Fx& Show::Emit(FxKind Kind, const Vec& At, const std::string& What, int Key)
	{
		Fx F;
		F.Kind = Kind;
		F.At = At;
		F.What = What;
		F.Key = Key;
		Effects.push_back(F);
		return Effects.back();
	}

	Show::Craft* Show::Find(int Key)
	{
		auto It = Crafts.find(Key);
		return It == Crafts.end() ? nullptr : &It->second;
	}
	const Show::Craft* Show::Find(int Key) const
	{
		auto It = Crafts.find(Key);
		return It == Crafts.end() ? nullptr : &It->second;
	}

	bool Show::Live(const Craft& C) const
	{
		return C.Doing == Role::Ground || C.Doing == Role::Slot || C.Doing == Role::Fight || C.Doing == Role::Straggle || C.Doing == Role::Cover || C.Doing == Role::Dive;
	}

	std::vector<int> Show::LiveOf(int GroupId) const
	{
		std::vector<int> Keys;
		for (const auto& Pair : Crafts)
		{
			if (Pair.second.Group == GroupId && Live(Pair.second)) Keys.push_back(Pair.first);
		}
		std::sort(Keys.begin(), Keys.end(), [this](int A, int B) { return Find(A)->Slot < Find(B)->Slot; });
		return Keys;
	}

	Vec Show::Velocity(const Craft& C) const
	{
		return Ahead(C.Hdg, C.Gam) * C.Spd;
	}

	Vec Show::Local(const Craft& C, const Vec& L) const
	{
		const Vec F = Ahead(C.Hdg, C.Gam);
		const Vec R0 = RightOf(C.Hdg);
		const Vec U0 = R0.Cross(F);
		const double Cb = std::cos(C.Bank), Sb = std::sin(C.Bank);
		const Vec R = R0 * Cb - U0 * Sb;
		const Vec U = U0 * Cb + R0 * Sb;
		return C.P + F * L.X + R * L.Y + U * L.Z;
	}

	bool Show::Middle(int GroupId, Vec& OutAt, double& OutFace) const
	{
		auto It = Now.find(GroupId);
		if (It == Now.end()) return false;
		OutAt = Vec(It->second.X, It->second.Y, It->second.Alt);
		OutFace = It->second.Face;
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The views and the picture between them.

	void Show::Take(double T, const std::vector<Group>& InGroups, const std::vector<Event>& InEvents, const std::vector<Ship>& InShips)
	{
		View V;
		V.T = T;
		for (const Group& G : InGroups) V.Groups[G.Id] = G;
		if (!Views.empty() && T <= Views.back().T) Views.back() = V; // (the same moment again: the newer word stands)
		else Views.push_back(V);
		while (Views.size() > 12 && Views[1].T <= ShownT) Views.pop_front();
		// A group made from a view that named no model (the place's main one stood in) takes its own when a view names
		// it: every model of the group still flying is made over, its shape and limits with it, where it is.
		// The views kept and the groups as now shown take it too, so that what reads a group's model (its engines'
		// sound, its formation's spacing) has its own from then on.
		for (const Group& G : InGroups)
		{
			if (G.bModelGuessed || G.Model.empty()) continue;
			for (auto& Pair : Crafts)
			{
				Craft& C = Pair.second;
				if (C.Group == G.Id && C.Model != G.Model && C.Doing != Role::Fall && C.Doing != Role::Gone) Fit(C, G);
			}
			auto Named = [&G](Group& Kept)
			{
				if (Kept.bModelGuessed)
				{
					Kept.Model = G.Model;
					Kept.bModelGuessed = false;
				}
			};
			for (View& Kept : Views)
			{
				auto It = Kept.Groups.find(G.Id);
				if (It != Kept.Groups.end()) Named(It->second);
			}
			auto Drawn = Now.find(G.Id);
			if (Drawn != Now.end()) Named(Drawn->second);
		}
		for (const Event& E : InEvents) Queue.push_back(E);
		std::stable_sort(Queue.begin(), Queue.end(), [](const Event& A, const Event& B) { return A.T < B.T; });
		if (!InShips.empty()) ShipsNow = InShips;
		if (!bStarted)
		{
			bStarted = true;
			ShownT = T;
			Interpolate();
			for (auto& Pair : Now) Reconcile(Pair.second, Bands[Pair.first]);
			Out.clear();
		}
	}

	void Show::Interpolate()
	{
		Now.clear();
		if (Views.empty()) return;
		const View* A = &Views.front();
		const View* B = nullptr;
		for (const View& V : Views)
		{
			if (V.T <= ShownT) A = &V;
			else { B = &V; break; }
		}
		const double F = B ? Clamp((ShownT - A->T) / std::max(1e-6, B->T - A->T), 0.0, 1.0) : 0.0;
		for (const auto& Pair : A->Groups)
		{
			Group G = Pair.second;
			if (B)
			{
				auto Next = B->Groups.find(Pair.first);
				if (Next != B->Groups.end())
				{
					G.X = Lerp(G.X, Next->second.X, F);
					G.Y = Lerp(G.Y, Next->second.Y, F);
					G.Alt = Lerp(G.Alt, Next->second.Alt, F);
					G.Face = Wrap(G.Face + Wrap(Next->second.Face - G.Face) * F);
				}
			}
			Now[Pair.first] = G;
		}
		if (B)
		{
			for (const auto& Pair : B->Groups)
			{
				if (!Now.count(Pair.first) && F > 0.5) Now[Pair.first] = Pair.second;
			}
		}
	}

	Vec Show::GroupVelocity(int GroupId) const
	{
		const View* A = nullptr;
		const View* B = nullptr;
		for (const View& V : Views)
		{
			if (V.T <= ShownT) A = &V;
			else { B = &V; break; }
		}
		if (A && !B && Views.size() >= 2) { B = A; A = &Views[Views.size() - 2]; }
		if (A && B && B->T > A->T)
		{
			auto Ga = A->Groups.find(GroupId), Gb = B->Groups.find(GroupId);
			if (Ga != A->Groups.end() && Gb != B->Groups.end())
			{
				const double Dt = (B->T - A->T) * Scale;
				return Vec(Gb->second.X - Ga->second.X, Gb->second.Y - Ga->second.Y, Gb->second.Alt - Ga->second.Alt) * (1.0 / Dt);
			}
		}
		auto It = Now.find(GroupId);
		if (It == Now.end()) return Vec();
		return Ahead(It->second.Face) * (It->second.Speed / Scale);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Formations.

	Vec Show::SlotPlace(const Group& G, int Slot) const
	{
		return SlotOf(G, Slot);
	}

	Vec SlotOf(const Group& G, int Slot)
	{
		const Vec F = Ahead(G.Face), R = RightOf(G.Face);
		double Rt = 0.0, Bk = 0.0, Hi = 0.0, Lat = 100.0, Back = 100.0, Step = 30.0;
		const Shape S = ShapeOf(G.Model, G.bFighter);
		if (!G.bFighter)
		{
			// Bombers in vics of three, the vics stepped up and down: the lead, the high element to the right, the low
			// to the left, and the low-low behind (the combat box, the art director's "vics of three stepped up").
			static const double Vic[8][3] = { { 0, 0, 0 }, { 1, 0.9, 0.5 }, { -1, 0.9, -0.5 }, { 0, 1.8, -1.0 }, { 2, 1.8, 1.0 }, { -2, 1.8, -1.5 }, { 1, 2.7, -0.5 }, { -1, 2.7, -2.0 } };
			static const double Wing[3][3] = { { 0, 0, 0 }, { -0.42, 0.35, -0.12 }, { 0.42, 0.35, 0.12 } };
			const int V = (Slot / 3) % 8, J = Slot % 3;
			Lat = std::max(G.W * 0.42, S.Span * 3.6);
			Back = std::max(G.D * 0.5, S.Long * 3.0);
			Step = std::max(25.0, S.Span * 1.6);
			Rt = Vic[V][0] + Wing[J][0];
			Bk = Vic[V][1] + Wing[J][1] + (Slot / 24) * 3.0;
			Hi = Vic[V][2] + Wing[J][2];
			// A torpedo box goes in low abreast, every aircraft on its own line to the ships.
			if (G.Strike == "torpedo" && G.Alt < 400.0)
			{
				Rt = (Slot % 2 == 0 ? 1.0 : -1.0) * ((Slot + 1) / 2) * 0.55;
				Bk = (Slot % 3) * 0.25;
				Hi = 0.0;
			}
		}
		else
		{
			Lat = std::max(G.W * 0.25, S.Span * 6.0);
			Back = Lat;
			Step = 12.0;
			if (G.Form == "line")
			{
				// Fours in line astern, the fours abreast.
				const int Four = Slot / 4, K = Slot % 4;
				Rt = (Four % 2 == 0 ? -1.0 : 1.0) * ((Four + 1) / 2) * 1.6;
				Bk = K * 0.45;
				Hi = -K * 0.1 + Four * 0.2;
			}
			else if (G.Form == "fours")
			{
				// Finger fours: the leader, his wingman back on one side, the element on the other; fours abreast.
				static const double Finger[4][3] = { { 0, 0, 0 }, { -0.45, 0.3, -0.05 }, { 0.65, 0.22, 0.08 }, { 1.1, 0.5, 0.12 } };
				const int Four = Slot / 4, K = Slot % 4;
				Rt = Finger[K][0] + (Four % 2 == 0 ? -1.0 : 1.0) * ((Four + 1) / 2) * 2.6;
				Bk = Finger[K][1] + Four * 0.25;
				Hi = Finger[K][2] + Four * 0.6;
			}
			else
			{
				// Vics of three (tight to 1940 for the RAF; the turning fighters' threes).
				static const double Three[3][3] = { { 0, 0, 0 }, { -0.4, 0.3, -0.05 }, { 0.4, 0.3, 0.05 } };
				const int V = Slot / 3, K = Slot % 3;
				const double Tight = G.Form == "vic" ? 0.6 : 1.0;
				Rt = (Three[K][0] + (V % 2 == 0 ? -1.0 : 1.0) * ((V + 1) / 2) * 1.5) * Tight;
				Bk = (Three[K][1] + V * 0.3) * Tight;
				Hi = Three[K][2] + V * 0.5;
			}
		}
		double Loose = G.Box == "loose" ? 1.5 : G.Box == "broken" ? 2.2 : 1.0;
		if (G.Doing == Act::Rout) Loose = std::max(Loose, 1.8);
		Vec P = Vec(G.X, G.Y, G.Alt) + R * (Rt * Lat * Loose) - F * (Bk * Back * Loose) + Up * (Hi * Step * Loose);
		if (Loose > 1.4)
		{
			// A box come apart: each aircraft a little off its place, as it was when the box was hit.
			P += R * (std::sin(Slot * 12.9898) * Lat * 0.35) - F * (std::sin(Slot * 78.233) * Back * 0.3);
		}
		return P;
	}

	Show::Craft& Show::Make(const Group& G, const Vec& At, double Hdg, double Spd, Role Doing)
	{
		Craft C;
		C.Key = NextKey++;
		C.Group = G.Id;
		C.Side = G.Side;
		C.bFighter = G.bFighter;
		C.Nation = G.Nation;
		Fit(C, G);
		C.P = At;
		C.Hdg = Hdg;
		C.Spd = Spd;
		C.Doing = Doing;
		C.Hand = Roll.U() < 0.5 ? -1.0 : 1.0;
		C.Weave = Roll.U(0.0, 2.0 * Pi);
		C.LastP = At;
		return Crafts[C.Key] = C;
	}

	void Show::Fit(Craft& C, const Group& G)
	{
		C.Model = G.Model;
		C.Form = ShapeOf(G.Model, C.bFighter);
		// A liquid-cooled engine hit in its cooling trails white glycol; a radial, blue-grey oil smoke (the art director).
		// The rules name it (extra.cool); an older recording does not, and the model's name tells.
		static const char* Cooled[] = { "Spitfire", "Bf109", "Mustang", "Warhawk", "Hurricane", "Yak", "Sturmovik", "Stuka", "He111", "Lancaster" };
		C.bLiquid = G.Cool == "liquid";
		if (G.Cool.empty()) for (const char* M : Cooled) C.bLiquid = C.bLiquid || C.Model.find(M) != std::string::npos;
		if (!C.TrailKey) C.TrailAt = EngineOf(C, 1);
		if (C.bFighter)
		{
			C.VMin = 80.0; C.VMax = 153.0; C.VTop = 200.0; C.RMin = 210.0; C.GMax = 6.0; C.RollMax = 120.0 * Deg;
		}
		else
		{
			C.VMin = 65.0; C.VMax = 110.0; C.VTop = 150.0; C.RMin = 500.0; C.GMax = 2.0; C.RollMax = 40.0 * Deg;
			if (C.Form.bDive) { C.GMax = 5.0; C.RMin = 300.0; C.RollMax = 90.0 * Deg; C.VTop = 165.0; }
		}
	}

	void Show::Reconcile(const Group& G, Band& B)
	{
		const bool bAirborne = !(G.Doing == Act::Idle || (G.Alt < 30.0 && G.Job == "wait"));
		std::vector<int> Keys = LiveOf(G.Id);
		int Want = G.Doing == Act::Down ? 0 : std::max(0, G.Figures);
		// Models already fallen for losses the group's figures have not yet shown stand for those losses.
		if (B.LastFigures >= 0 && G.Figures < B.LastFigures)
		{
			for (int K = 0; K < B.LastFigures - G.Figures && !B.Owed.empty(); ++K) B.Owed.pop_front();
		}
		B.LastFigures = G.Figures;
		while (!B.Owed.empty() && AirT - B.Owed.front() > 8.0) B.Owed.pop_front(); // (a model is one of several aircraft: after a while the gap closes up)
		const int Waiting = static_cast<int>(B.Owed.size());

		if (!B.bSeen)
		{
			B.bSeen = true;
			const Vec Vel = GroupVelocity(G.Id);
			const double Spd = std::max(Flat(Vel) > 1.0 ? Len(Vel) : G.Speed / Scale, G.bFighter ? 90.0 : 70.0);
			for (int K = 0; K < Want; ++K)
			{
				if (!bAirborne)
				{
					// On the ground at its field: in a line along the field, facing the way it will take off.
					const Vec F = Ahead(G.Face), R = RightOf(G.Face);
					Vec At = Vec(G.X, G.Y, 0.0) + R * ((K - Want * 0.5) * 28.0) - F * ((K % 2) * 25.0);
					At.Z = GroundAt(At.X, At.Y);
					Craft& C = Make(G, At, G.Face, 0.0, Role::Ground);
					C.Slot = K;
				}
				else
				{
					Craft& C = Make(G, SlotPlace(G, K), G.Face, Spd, Role::Slot);
					C.Slot = K;
				}
			}
			return;
		}
		if (G.Doing == Act::Down)
		{
			for (int Key : Keys) Leave(*Find(Key));
			return;
		}
		const int Have = static_cast<int>(Keys.size());
		if (Have + Waiting < Want)
		{
			// A replacement: the next model of the group closing up from behind, as a formation closed up its gaps.
			for (int K = Have + Waiting; K < Want; ++K)
			{
				const Vec F = Ahead(G.Face);
				Vec At = SlotPlace(G, K) - F * 250.0 - Up * 40.0;
				Role Doing = Role::Slot;
				if (!bAirborne)
				{
					At = Vec(G.X, G.Y, 0.0) + RightOf(G.Face) * ((K - Want * 0.5) * 28.0);
					At.Z = GroundAt(At.X, At.Y);
					Doing = Role::Ground;
				}
				const Vec Vel = GroupVelocity(G.Id);
				Craft& C = Make(G, At, G.Face, bAirborne ? std::max(Len(Vel) * 1.05, G.bFighter ? 95.0 : 75.0) : 0.0, Doing);
				C.Slot = K;
				C.bJoining = bAirborne;
			}
		}
		else if (Have > Want)
		{
			// One model too many: the youngest joining, else the last in the formation, peels off for home.
			std::vector<int> Order = Keys;
			std::stable_sort(Order.begin(), Order.end(), [this](int A, int Bk)
			{
				const Craft& Ca = *Find(A);
				const Craft& Cb = *Find(Bk);
				const int Ra = Ca.bJoining ? 0 : Ca.Doing == Role::Slot ? 1 : 2;
				const int Rb = Cb.bJoining ? 0 : Cb.Doing == Role::Slot ? 1 : 2;
				return Ra != Rb ? Ra < Rb : Ca.Slot > Cb.Slot;
			});
			for (int K = 0; K < Have - Want; ++K) Leave(*Find(Order[K]));
		}
		// Places in the formation, in the order they stand.
		Keys = LiveOf(G.Id);
		for (int K = 0; K < static_cast<int>(Keys.size()); ++K) Find(Keys[K])->Slot = K;
	}

	void Show::Leave(Craft& C)
	{
		if (C.Doing == Role::Fall || C.Doing == Role::Gone || C.Doing == Role::Home) return;
		if (C.Doing == Role::Ground) { C.Doing = Role::Gone; return; }
		C.Doing = Role::Home;
		C.Step = Phase::None;
		C.StepT = 0.0;
		C.bFiring = false;
	}

	Vec Show::EngineOf(const Craft& C, int N)
	{
		if (C.Form.Engines.empty()) return Vec();
		return C.Form.Engines[static_cast<size_t>(std::max(0, std::min(N - 1, static_cast<int>(C.Form.Engines.size()) - 1)))];
	}

	void Show::LayTrail(Craft& C, const std::string& Kind)
	{
		if (!C.TrailKey) C.TrailKey = C.Key * 8 + 1;
		const Vec V = Velocity(C);
		{
			Fx& T = Emit(FxKind::TrailStart, Local(C, C.TrailAt), Kind, C.TrailKey);
			T.Vel = V;
			T.Size = C.Form.Span;
		}
		if (C.TrailKey2)
		{
			Fx& T = Emit(FxKind::TrailStart, Local(C, C.TrailAt2), Kind, C.TrailKey2);
			T.Vel = V;
			T.Size = C.Form.Span;
		}
		C.TrailKind = Kind;
	}

	void Show::DeadEngineTrail(Craft& C, bool bFire)
	{
		// (the art director's three states, 15:38Z: burning, the densest black; out and windmilling, heavy dark oil
		// smoke that pulses; the fire put out and feathered, about half that, a thin dark streak that never clears)
		C.TrailPlan.clear();
		if (bFire) { LayTrail(C, "fire"); return; }
		if (C.bFireOut) { LayTrail(C, "streak"); return; }
		// (a liquid-cooled engine: white glycol first, thinning as the coolant runs out, then the same dark oil)
		LayTrail(C, C.bLiquid ? "glycol" : "oil");
		if (C.bLiquid) C.TrailPlan.push_back({ AirT + 45.0, "oil" });
		// (feathered with no fire, its oil spent: it thins to the streak and keeps it)
		if (C.bFeathered) C.TrailPlan.push_back({ AirT + (C.bLiquid ? 65.0 : 20.0), "streak" });
	}

	void Show::StopTrails(Craft& C)
	{
		if (C.TrailKey) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey); C.TrailKey = 0; }
		if (C.TrailKey2) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey2); C.TrailKey2 = 0; }
		C.TrailKind.clear();
		C.TrailPlan.clear();
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Flight.

	Show::Cmd Show::Steer(const Craft& C, const Vec& To, const Vec& ToVel, double Gain, double SpeedLow, double SpeedHigh) const
	{
		const Vec Rel = To - C.P;
		Vec Want = ToVel + Rel * Gain;
		if (Len(Want) < 1.0) Want = Ahead(C.Hdg) * C.Spd;
		Cmd K;
		K.Hdg = std::atan2(Want.X, Want.Y);
		const double Climb = C.bFighter ? 0.6 : 0.25;
		K.Gam = Clamp(std::atan2(Want.Z, std::max(1.0, Flat(Want))), -Climb, std::min(Climb, ClimbMost(C)));
		K.Spd = Clamp(Len(Want), SpeedLow, SpeedHigh);
		return K;
	}

	Show::Cmd Show::Toward(const Craft& C, const Vec& To, double Speed) const
	{
		const Vec Rel = To - C.P;
		Cmd K;
		K.Hdg = Flat(Rel) > 1.0 ? std::atan2(Rel.X, Rel.Y) : C.Hdg;
		K.Gam = std::min(std::atan2(Rel.Z, std::max(1.0, Flat(Rel))), ClimbMost(C));
		K.Spd = Speed;
		return K;
	}

	double Show::ClimbMost(const Craft& C) const
	{
		// A fighter pilot trades height for speed: below about 112 m/s he will not climb, and slower still he lets
		// the nose down to gather speed again (the turning fights stay fights, never a slow climb toward a stall).
		if (!C.bFighter || C.Doing != Role::Fight) return 80.0 * Deg;
		return Clamp((C.Spd - 112.0) / 35.0, -0.25, 1.0) * 0.6;
	}

	void Show::Fly(Craft& C, const Cmd& K, double Dt, bool bCheck)
	{
		const double S = std::max(C.Spd, 30.0);
		// The pull a pilot dares: his full g only at speed, easing off as he slows toward the stall.
		const double GNow = C.Doing == Role::Fall ? C.GMax : std::min(C.GMax, 2.0 + (C.GMax - 2.0) * Clamp((S - C.VMin) / 45.0, 0.0, 1.0));
		// The tightest turn this speed allows: no tighter than its radius, no more than that g.
		const double OmegaTurn = S / C.RMin;
		const double OmegaG = Gee * std::sqrt(std::max(0.0, GNow * GNow - 1.0)) / S;
		const double OmegaMost = std::min(OmegaTurn, OmegaG);
		double BankWant;
		if (K.bBank) BankWant = K.Bank;
		else
		{
			const double Turn = Wrap(K.Hdg - C.Hdg);
			const double Omega = Clamp(Turn * 0.8, -OmegaMost, OmegaMost);
			BankWant = std::atan(S * Omega / Gee); // bank = atan(v squared / (g r))
		}
		// Roll toward it, no faster than the airframe rolls.
		const double Roll_ = Clamp(Wrap(BankWant - C.Bank), -C.RollMax * Dt, C.RollMax * Dt);
		C.Bank = Wrap(C.Bank + Roll_);
		// The turn the bank gives (none on its back or on its side: a half-roll turns the nose down, not round).
		double Omega = 0.0;
		if (std::fabs(C.Bank) < 88.0 * Deg) Omega = Clamp(Gee * std::tan(C.Bank) / S, -OmegaMost, OmegaMost);
		C.Hdg = Wrap(C.Hdg + Omega * Dt);
		// The climb: the pull it takes, within what is left of its g after the turn.
		const double TurnG = S * std::fabs(Omega) / Gee;
		const double Left = std::sqrt(std::max(0.0, GNow * GNow - TurnG * TurnG));
		const double Cg = std::cos(C.Gam);
		const bool bInverted = std::fabs(C.Bank) > 100.0 * Deg;
		// Up: what is left of its g. Down: the same on its back; right side up, pushing the nose over, no more than a g
		// and a half the wrong way.
		const double QUp = (Left - Cg) * Gee / S;
		const double QDown = ((bInverted ? Left : std::min(Left, 1.5)) + Cg) * Gee / S;
		const double Q = Clamp((K.Gam - C.Gam) * 1.1, -std::max(0.0, QDown), std::max(0.0, QUp));
		const double Vert = S * Q / Gee + Cg; // the load the pull puts on it, in g (with the turn's, below)
		C.Gam = Clamp(C.Gam + Q * Dt, -88.0 * Deg, 80.0 * Deg);
		// Speed: what the pilot asks of his engine, and what the climb or the dive does to it.
		// A hard turn costs speed (the drag of the lift it takes): a fighter cannot hold six g for long, and a sustained
		// turn settles at three or four.
		const double Push = Clamp((K.Spd - S) / 4.0, -4.0, 3.0);
		const double Load2 = TurnG * TurnG + Vert * Vert;
		const double Bleed = C.Doing == Role::Fall ? 0.0 : 0.3 * std::max(0.0, Load2 - 1.0);
		C.Spd = Clamp(S + (Push - Gee * std::sin(C.Gam) * 0.85 - Bleed) * Dt, C.Doing == Role::Fall ? 20.0 : C.VMin * 0.75, C.VTop);
		C.P += Ahead(C.Hdg, C.Gam) * (C.Spd * Dt);
		if (bCheck && Dt > 0.0)
		{
			Check.MostRoll = std::max(Check.MostRoll, std::fabs(Roll_) / Dt / Deg);
			if (std::fabs(Omega) > 1e-4) Check.LeastRadius = std::min(Check.LeastRadius, C.Spd / std::fabs(Omega));
			Check.MostG = std::max(Check.MostG, std::sqrt(Load2));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The pilots.

	void Show::Pilot(Craft& C, double Dt)
	{
		C.StepT += Dt;
		auto GroupIt = Now.find(C.Group);
		if (C.Doing == Role::Fall) { PilotFall(C, Dt); return; }
		if (C.Doing == Role::Gone) return;
		if (GroupIt == Now.end() && C.Doing != Role::Home) Leave(C);
		const Group* G = GroupIt == Now.end() ? nullptr : &GroupIt->second;
		switch (C.Doing)
		{
		case Role::Ground:
			if (G) PilotGround(C, *G, Dt);
			return;
		case Role::Fight:
			if (G && G->Doing == Act::Fight && G->Engaged >= 0) { PilotFight(C, *G, Dt); return; }
			C.Doing = Role::Slot;
			C.Step = Phase::None;
			C.bFiring = false;
			break;
		case Role::Dive:
			if (G) { PilotDive(C, *G, Dt); return; }
			Leave(C);
			break;
		default:
			break;
		}
		if (C.Doing == Role::Home)
		{
			// Away from the target, a little lower, until it is out of the fight; then gone.
			const Vec Away = Norm(Vec(C.P.X - TargetX, C.P.Y - TargetY, 0.0));
			Cmd K;
			K.Hdg = std::atan2(Away.X, Away.Y);
			K.Gam = C.P.Z > 600.0 ? -0.06 : 0.0;
			K.Spd = C.bFighter ? 120.0 : 85.0;
			Fly(C, K, Dt, true);
			if (C.StepT > 120.0)
			{
				C.Doing = Role::Gone;
				StopTrails(C);
			}
			return;
		}
		if (!G) return;
		if (C.bFighter && G->Doing == Act::Fight && G->Engaged >= 0 && C.Doing == Role::Slot && !C.bJoining)
		{
			C.Doing = Role::Fight;
			C.Step = Phase::None;
			PilotFight(C, *G, Dt);
			return;
		}
		const Vec GV = GroupVelocity(C.Group);
		if (C.Doing == Role::Straggle)
		{
			const Strag* Mine = nullptr;
			for (const Strag& S : G->Strags) if (S.Id == C.StragId) Mine = &S;
			// (its dead engine catching fire, as the box's view tells it; a fire put out is the battle's own event)
			if (Mine && Mine->bFire && C.TrailKind != "fire" && !C.bFireOut) DeadEngineTrail(C, true);
			// (the battle tells of a bomber falling out a moment before its box's view lists it: it drops back meanwhile)
			if (Mine || AirT - C.StragAt < 8.0)
			{
				const Vec F = Ahead(G->Face);
				const Vec To = Vec(G->X, G->Y, G->Alt) - F * (Mine ? Mine->Back : 180.0) - Up * (Mine ? Mine->Down : 18.0) + RightOf(G->Face) * (C.Hand * G->W * 0.3);
				Fly(C, Steer(C, To, GV, 0.08, C.VMin, C.VMax), Dt, true);
				return;
			}
			// (taken back into the box's count: it flies its place again, closing slowly)
			C.Doing = Role::Slot;
			C.StragId = -1;
			C.bJoining = true;
		}
		if (C.Doing == Role::Cover)
		{
			const Craft* Lame = C.Target >= 0 ? Find(C.Target) : nullptr;
			if (Lame && Lame->Doing == Role::Straggle)
			{
				const double W = AirT * 2.0 * Pi / 26.0 + C.Weave;
				const Vec To = Lame->P + Up * 180.0 + RightOf(Lame->Hdg) * (220.0 * std::sin(W));
				const Vec ToVel = Velocity(*Lame) + RightOf(Lame->Hdg) * (220.0 * 2.0 * Pi / 26.0 * std::cos(W));
				Fly(C, Steer(C, To, ToVel, 0.1, C.VMin, C.VMax), Dt, true);
				return;
			}
			C.Doing = Role::Slot;
		}
		// In its place: the formation's speed, a slow escort weaving beside its bombers.
		Vec To = SlotPlace(*G, C.Slot), ToVel = GV;
		if (C.bFighter && Len(GV) < 105.0)
		{
			// Wide, slow esses: the escort keeps its speed up (about 85 to 120 m/s) and its turns gentle (400 m and more).
			const double W = AirT * 2.0 * Pi / 34.0 + C.Weave * 0.15;
			To += RightOf(G->Face) * (380.0 * std::sin(W));
			ToVel += RightOf(G->Face) * (380.0 * 2.0 * Pi / 34.0 * std::cos(W));
		}
		const double Gap = Len(To - C.P);
		if (C.bJoining && Gap < 40.0) C.bJoining = false;
		Fly(C, Steer(C, To, ToVel, Gap > 2000.0 ? 0.05 : 0.12, C.VMin, Gap > 300.0 ? C.VMax : C.VMax * 0.9), Dt, true);
	}

	void Show::PilotGround(Craft& C, const Group& G, double Dt)
	{
		if (G.Doing == Act::Idle || (G.Job == "wait" && G.Alt < 30.0))
		{
			C.Spd = 0.0;
			C.Gam = 0.0;
			C.Bank = 0.0;
			C.P.Z = GroundAt(C.P.X, C.P.Y);
			C.StepT = 0.0;
			return;
		}
		// Scrambled: each in turn, a few seconds apart, rolling down the field and climbing away.
		if (C.StepT < C.Slot * 4.0) return;
		C.Spd = std::min(C.Spd + 5.0 * Dt, 75.0);
		C.Hdg = G.Face;
		const double Floor = GroundAt(C.P.X, C.P.Y);
		if (C.Spd > 55.0) C.Gam = std::min(C.Gam + 0.1 * Dt, 0.16);
		C.P += Ahead(C.Hdg, C.Gam) * (C.Spd * Dt);
		C.P.Z = std::max(C.P.Z, Floor);
		if (C.P.Z > Floor + 150.0)
		{
			C.Doing = Role::Slot;
			C.Step = Phase::None;
			C.bJoining = true;
		}
	}

	bool Show::Fights(const Group& G, int Id) const
	{
		if (Now.find(Id) == Now.end() || LiveOf(Id).empty()) return false;
		if (Id == G.Engaged) return true;
		for (const Fire& F : G.Firing) if (F.On == Id) return true;
		return false;
	}

	int Show::PickQuarry(const Group& G)
	{
		// A group engaged with one enemy fires on several at once (the defenders on the escort and on each box,
		// extra.fire): each of its aircraft picks among them by how hard the group fires on each.
		double Sum = 0.0;
		for (const Fire& F : G.Firing) if (F.Rate > 0.0 && Fights(G, F.On)) Sum += F.Rate;
		if (Sum <= 0.0) return G.Engaged;
		double U = Roll.U() * Sum;
		for (const Fire& F : G.Firing)
		{
			if (F.Rate <= 0.0 || !Fights(G, F.On)) continue;
			U -= F.Rate;
			if (U < 0.0) return F.On;
		}
		return G.Engaged;
	}

	void Show::PilotFight(Craft& C, const Group& G, double Dt)
	{
		// Its quarry: kept through a pass and for half a minute or so; then picked again, by how hard the group fires on
		// each enemy group, so the group's aircraft go where its fire goes.
		int Quarry = C.TargetGroup;
		const bool bMidPass = C.Step == Phase::Run || C.Step == Phase::Break;
		if (!(Quarry >= 0 && Fights(G, Quarry) && (bMidPass || AirT < C.Repick)))
		{
			Quarry = PickQuarry(G);
			C.Repick = AirT + Roll.U(20.0, 35.0);
		}
		if (Quarry != C.TargetGroup)
		{
			C.TargetGroup = Quarry;
			C.Target = -1;
			C.Step = Phase::None;
			C.bFiring = false;
		}
		auto EIt = Now.find(Quarry);
		if (EIt == Now.end()) { C.Doing = Role::Slot; return; }
		const Group& E = EIt->second;
		// Its quarry: one aircraft of the enemy group, the group's aircraft shared out among his.
		const Craft* T = C.Target >= 0 ? Find(C.Target) : nullptr;
		if (!T || T->Group != E.Id || !Live(*T))
		{
			const std::vector<int> Theirs = LiveOf(E.Id);
			if (Theirs.empty()) { C.Doing = Role::Slot; return; }
			C.Target = Theirs[(C.Slot + static_cast<int>(Roll.U() * 3.0)) % Theirs.size()];
			C.TargetGroup = E.Id;
			T = Find(C.Target);
			if (C.Step == Phase::None) { C.Step = E.bFighter ? (G.Style == "dive" ? Phase::Setup : Phase::Turn) : Phase::Setup; C.StepT = 0.0; }
		}
		// The pass the contact named, or what the group is firing with now.
		std::string Pass = "astern";
		{
			auto PIt = Bands[G.Id].Pass.find(E.Id);
			if (PIt != Bands[G.Id].Pass.end() && !PIt->second.empty()) Pass = PIt->second;
			for (const Fire& F : G.Firing) if (F.On == E.Id && !F.Pass.empty()) Pass = F.Pass;
		}
		bool bMayFire = false;
		for (const Fire& F : G.Firing) if (F.On == E.Id && F.Rate > 0.0) bMayFire = true;
		const Vec Tp = T->P, Tv = Velocity(*T);
		const Vec Fb = Ahead(T->Hdg), Rb = RightOf(T->Hdg);
		const double Dist = Len(Tp - C.P);
		const double Fast = C.VMax, Slow = 111.0;
		if (C.Burst > 0.0) C.Burst -= Dt;
		if (C.Rest > 0.0) C.Rest -= Dt;
		auto Trigger = [&C, this, bMayFire, Dist, Tp](double Range, double Most, double Lo, double Hi)
		{
			// Fire only with the nose on the lead, inside the range, in bursts of a second or two.
			const Vec Nose = Ahead(C.Hdg, C.Gam);
			const double Off = std::acos(Clamp(Nose.Dot(Norm(Tp - C.P)), -1.0, 1.0));
			if (bMayFire && C.Burst <= 0.0 && C.Rest <= 0.0 && Dist < Range && Off < Most)
			{
				C.Burst = Roll.U(Lo, Hi);
				C.Rest = C.Burst + Roll.U(1.0, 2.5);
			}
			C.bFiring = C.Burst > 0.0 && bMayFire && Dist < Range * 1.2 && Dist > 60.0;
		};
		const double Lead = Dist / 800.0;
		const Vec Aim = Tp + Tv * Lead;

		if (!E.bFighter)
		{
			// On a bomber box: the pass the contact names, the break, the climb back ahead for another.
			switch (C.Step)
			{
			case Phase::Setup:
			default:
			{
				if (C.Step != Phase::Setup) { C.Step = Phase::Setup; C.StepT = 0.0; }
				Vec S;
				if (Pass == "headon") S = Tp + Fb * 2600.0 + Up * 120.0;
				else if (Pass == "quarter") S = Tp + Fb * 1400.0 + Rb * (C.Hand * 1400.0) + Up * 350.0;
				else if (Pass == "beam") S = Tp + Rb * (C.Hand * 2000.0) + Up * 150.0;
				else if (Pass == "below") S = Tp - Fb * 1400.0 - Up * 350.0;
				else S = Tp - Fb * 1800.0 + Up * 250.0; // astern, and a close escort's fight
				Fly(C, Steer(C, S, Tv, 0.09, Slow, Fast), Dt, true);
				C.bFiring = false;
				// Near enough to the start of its pass: a fighter at speed circles a point more often than it hits it.
				if (Len(S - C.P) < 900.0 || C.StepT > 45.0)
				{
					C.Step = Phase::Run;
					C.StepT = 0.0;
					C.Pass = Pass;
					Check.Passes++;
				}
				break;
			}
			case Phase::Run:
			{
				Cmd K = Toward(C, Aim, Fast);
				Fly(C, K, Dt, true);
				Check.LeastFightSpeed = std::min(Check.LeastFightSpeed, C.Spd);
				Check.MostFightSpeed = std::max(Check.MostFightSpeed, C.Spd);
				const bool bHead = C.Pass == "headon";
				const bool bDeflection = C.Pass == "beam" || C.Pass == "quarter";
				Trigger(bHead ? 900.0 : 650.0, 10.0 * Deg, bHead ? 0.5 : 1.0, bHead ? 1.2 : bDeflection ? 2.2 : 2.0);
				if (Dist < (bHead ? 260.0 : 160.0) || C.StepT > 28.0)
				{
					// Three breaks in four a half-roll and a dive beneath the box; the rest over it, most after a pass from a little high.
					const double Over = (C.Pass == "astern" || C.Pass == "quarter") ? 0.35 : C.Pass == "below" ? 0.9 : 0.2;
					C.bUnder = Roll.U() >= Over;
					(C.bUnder ? Check.UnderBreaks : Check.OverBreaks)++;
					C.Step = Phase::Break;
					C.StepT = 0.0;
					C.bFiring = false;
					C.Burst = 0.0;
				}
				break;
			}
			case Phase::Break:
			{
				Cmd K;
				K.Hdg = C.Hdg;
				K.Spd = C.VTop;
				if (C.bUnder)
				{
					// Over on its back, the nose pulled down through the dive, then right side up and out underneath.
					K.bBank = C.StepT < 4.2;
					K.Bank = C.Hand * 170.0 * Deg;
					K.Gam = C.StepT < 1.1 ? C.Gam : C.StepT < 4.2 ? -60.0 * Deg : -15.0 * Deg;
				}
				else
				{
					K.bBank = true;
					K.Bank = C.Hand * 60.0 * Deg;
					K.Gam = 30.0 * Deg;
					K.Spd = Fast;
				}
				Fly(C, K, Dt, true);
				C.bFiring = false;
				if (C.StepT > (C.bUnder ? 7.0 : 4.0)) { C.Step = Phase::Climb; C.StepT = 0.0; }
				break;
			}
			case Phase::Climb:
			{
				// Back ahead of the box and up, for the next pass.
				const Vec S = Tp + Fb * 3200.0 + Rb * (C.Hand * 1300.0) + Up * 450.0;
				Fly(C, Steer(C, S, Tv, 0.06, Slow, Fast), Dt, true);
				C.bFiring = false;
				if (Len(S - C.P) < 900.0 || C.StepT > 45.0)
				{
					C.Step = Phase::Setup;
					C.StepT = 0.0;
					if (Roll.U() < 0.4) C.Hand = -C.Hand;
				}
				break;
			}
			}
			return;
		}

		// Fighter against fighter.
		if (G.Style == "dive")
		{
			// Dive from height, fire, zoom away and come again.
			switch (C.Step)
			{
			case Phase::Run:
			{
				Fly(C, Toward(C, Aim, C.VTop), Dt, true);
				Trigger(500.0, 9.0 * Deg, 0.8, 1.6);
				if (Dist < 140.0 || C.StepT > 18.0 || C.P.Z < Tp.Z - 300.0) { C.Step = Phase::Break; C.StepT = 0.0; C.bFiring = false; }
				break;
			}
			case Phase::Break:
			{
				Cmd K;
				K.Hdg = C.Hdg + C.Hand * 0.3;
				K.Gam = 45.0 * Deg;
				K.Spd = Fast;
				Fly(C, K, Dt, true);
				C.bFiring = false;
				if (C.StepT > 9.0 || C.Spd < 115.0) { C.Step = Phase::Setup; C.StepT = 0.0; }
				break;
			}
			default:
			{
				if (C.Step != Phase::Setup) { C.Step = Phase::Setup; C.StepT = 0.0; }
				const Vec S = Tp - Ahead(T->Hdg) * 900.0 + Up * 900.0 + RightOf(T->Hdg) * (C.Hand * 500.0);
				Fly(C, Steer(C, S, Tv, 0.08, Slow, Fast), Dt, true);
				C.bFiring = false;
				if (Len(S - C.P) < 700.0 || C.StepT > 40.0) { C.Step = Phase::Run; C.StepT = 0.0; Check.Passes++; }
				break;
			}
			}
			return;
		}
		// The turning fight: after his tail, the circle tightening, never tighter than the aircraft turns; a burst when
		// the nose comes on; back toward the fight when it has drawn away.
		C.Step = Phase::Turn;
		Vec Middle(E.X, E.Y, E.Alt);
		Middle = (Middle + Vec(G.X, G.Y, G.Alt)) * 0.5;
		Cmd K;
		if (Len(C.P - Middle) > 3500.0) K = Steer(C, Middle, GroupVelocity(G.Id), 0.05, Slow, Fast);
		else K = Toward(C, Aim, Clamp(Len(Tv) + 8.0, Slow, Fast));
		K.Gam = Clamp(K.Gam, -0.35, 0.35);
		Fly(C, K, Dt, true);
		Check.LeastFightSpeed = std::min(Check.LeastFightSpeed, C.Spd);
		Check.MostFightSpeed = std::max(Check.MostFightSpeed, C.Spd);
		Trigger(450.0, 7.0 * Deg, 1.0, 2.0);
	}

	void Show::PilotDive(Craft& C, const Group& G, double Dt)
	{
		(void)G;
		// The dive bombers' attack: over on a wing, down at seventy degrees on the brakes, the bomb at six hundred
		// metres, the pull-out low over the water and away, then back up to the group.
		const Ship* Mark = nullptr;
		for (const Ship& S : ShipsNow) if (S.Id == C.Target) Mark = &S;
		const Vec To = Mark ? Mark->At : Vec(TargetX, TargetY, 0.0);
		switch (C.Step)
		{
		case Phase::Peel:
		{
			if (C.StepT < 0.0) { Fly(C, Toward(C, To + Up * C.P.Z, C.Spd), Dt, true); return; }
			Cmd K = Toward(C, To, 110.0);
			K.bBank = C.StepT < 2.0;
			K.Bank = C.Hand * 100.0 * Deg;
			K.Gam = -70.0 * Deg;
			Fly(C, K, Dt, false);
			if (C.StepT > 3.5) { C.Step = Phase::Plunge; C.StepT = 0.0; }
			break;
		}
		case Phase::Plunge:
		{
			Cmd K = Toward(C, To, 110.0);
			K.Gam = std::min(K.Gam, -55.0 * Deg);
			Fly(C, K, Dt, false);
			C.Spd = std::min(C.Spd, 115.0); // (the dive brakes)
			if (C.P.Z < GroundAt(C.P.X, C.P.Y) + 600.0 || C.StepT > 60.0)
			{
				Drop(C, false);
				C.Step = Phase::Pull;
				C.StepT = 0.0;
			}
			break;
		}
		case Phase::Pull:
		{
			Cmd K;
			K.Hdg = C.Hdg;
			K.Gam = 2.0 * Deg;
			K.Spd = 140.0;
			Fly(C, K, Dt, false);
			if (C.Gam > -3.0 * Deg || C.StepT > 8.0) { C.Step = Phase::Egress; C.StepT = 0.0; }
			break;
		}
		case Phase::Egress:
		default:
		{
			Cmd K;
			K.Hdg = C.Hdg + C.Hand * 0.15;
			K.Gam = C.P.Z < 200.0 ? 3.0 * Deg : 0.0;
			K.Spd = 120.0;
			Fly(C, K, Dt, true);
			if (C.StepT > 25.0)
			{
				C.Doing = Role::Slot;
				C.Step = Phase::None;
				C.bJoining = true;
			}
			break;
		}
		}
	}

	void Show::PilotFall(Craft& C, double Dt)
	{
		C.FallT += Dt;
		if (C.Falling == FallHow::Explode || C.Falling == FallHow::Breakup)
		{
			C.bShown = false;
			if (C.FallT > 1.0) C.Doing = Role::Gone;
			return;
		}
		// Struck the sea or the ground already: a ditched aircraft floats nose-low a while, then sinks; a belly-landed
		// one lies in its furrow.
		if (C.Ended >= 0.0)
		{
			C.Spd = 0.0;
			C.Gam = -4.0 * Deg;
			C.Bank = 0.0;
			if (C.FallT - C.Ended > 5.0 + 3.3 * C.Look)
			{
				if (C.TrailKey) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey); C.TrailKey = 0; }
				Emit(FxKind::FallStop, C.P, std::string(), C.Key);
				C.Doing = Role::Gone;
				C.bShown = false;
			}
			return;
		}
		const double Floor0 = GroundAt(C.P.X, C.P.Y);
		// How fast it must come down to strike when the battle says (the event's dur is the truth: the art director);
		// never under 8 m/s, never past what the airframe may move.
		const double Left = std::max(C.Dur - C.FallT, 1.5);
		const double Sink = std::clamp((C.P.Z - Floor0) / Left, 8.0, C.VTop * 0.95);
		const bool bTumbling = C.Falling == FallHow::Wing || C.Falling == FallHow::Spin || (C.Falling == FallHow::Tail && !C.bHeavy);
		// Its fire shows (a burning engine's smoke catches; a flamer's wing root), and later it may blow apart.
		if (!C.bLit && C.FireAt >= 0.0 && C.FallT >= C.FireAt && (C.Falling == FallHow::Burn || C.Falling == FallHow::Flamer || C.Falling == FallHow::Struck))
		{
			C.bLit = true;
			if (!C.TrailKey) C.TrailKey = C.Key * 8 + 1;
			Fx& T = Emit(FxKind::TrailStart, C.Falling == FallHow::Flamer ? C.P : Local(C, C.Form.Engines.empty() ? Vec() : C.Form.Engines[0]), "fire", C.TrailKey);
			T.Vel = Velocity(C);
			T.Size = C.Form.Span * (C.Falling == FallHow::Flamer ? 1.0 + 0.5 * C.Look : 1.0);
		}
		if (C.BoomAt >= 0.0 && C.FallT >= C.BoomAt)
		{
			// (its debris row ends here, in the air: "boom")
			Emit(FxKind::FallStop, C.P, "boom", C.Key);
			BlowApart(C, true);
			return;
		}
		if (bTumbling)
		{
			// Its own way through the air, no longer its nose's: what it was flying at dies away, and it comes down.
			const double Ease = std::min(1.0, Dt * 0.35);
			C.FallV.X += (0.0 - C.FallV.X) * Ease;
			C.FallV.Y += (0.0 - C.FallV.Y) * Ease;
			C.FallV.Z += (-Sink - C.FallV.Z) * std::min(1.0, Dt * 0.6);
			if (C.Falling == FallHow::Spin)
			{
				// A helix: round its middle line as that comes down. Steep (looks 0, 1): the nose 60 to 80 degrees down,
				// 1 to 2 turns a second a fighter, 0.3 to 0.6 a heavy; flat (2, 3): the nose 20 to 40 degrees down, lazy.
				const bool bSteep = C.Look < 2;
				const double Turns = C.bHeavy ? (bSteep ? 0.3 + 0.3 * C.Look : 0.2) : (bSteep ? 1.0 + 1.0 * C.Look : 0.5 + 0.2 * (C.Look - 2));
				const double Radius = bSteep ? 6.0 + C.Form.Span * 0.4 : 14.0 + C.Form.Span * 0.8;
				C.Spin += C.Turn * Turns * 2.0 * Pi * Dt;
				C.Axis += C.FallV * Dt;
				C.P = C.Axis + Vec(std::sin(C.Spin), std::cos(C.Spin), 0.0) * Radius;
				C.Hdg = Wrap(C.Spin + C.Turn * Pi * 0.5);
				const double Nose = bSteep ? 60.0 + 10.0 * C.Look : 20.0 + 15.0 * (C.Look - 2);
				C.Gam += (-Nose * Deg - C.Gam) * std::min(1.0, Dt * 1.5);
				C.Bank = C.Turn * 40.0 * Deg;
				C.Spd = Turns * 2.0 * Pi * Radius;
			}
			else if (C.Falling == FallHow::Wing)
			{
				// A wing gone: snap-rolling toward it, 2 to 4 rolls a second a fighter, about 1 a heavy, in a fast
				// spiral, the nose well down.
				const double Rolls = C.bHeavy ? 0.8 + 0.15 * C.Look : 2.0 + 0.66 * C.Look;
				C.Bank = Wrap(C.Bank + C.Turn * Rolls * 2.0 * Pi * Dt);
				C.Hdg = Wrap(C.Hdg + C.Turn * 0.9 * Dt);
				C.Gam += (-65.0 * Deg - C.Gam) * std::min(1.0, Dt * 1.2);
				C.P += C.FallV * Dt;
				C.Spd = Len(C.FallV);
			}
			else
			{
				// A fighter's tail gone: the nose down hard and over and over, end over end.
				C.Spin += (1.0 + 0.4 * C.Look) * 2.0 * Pi * Dt;
				C.Gam = Wrap(-C.Spin);
				C.Bank = 0.0;
				C.P += C.FallV * Dt;
				C.Spd = Len(C.FallV);
			}
		}
		else
		{
			// Still flying, after a fashion: the nose where the way down puts it, its speed what the drop asks.
			double Nose = -30.0, Bank = 0.0, Turn = 0.0;
			switch (C.Falling)
			{
			case FallHow::Pilot:
			{
				// Holds its line 0.5 to 2 s, then rolls gently off and dives straight in, 60 to 85 degrees; a heavy whose
				// pilots are hit (look 3) rears up first, stalls and falls off on a wing.
				const double Hold = 0.5 + 0.5 * C.Look;
				if (C.bHeavy && C.Look == 3 && C.FallT < Hold + 2.5) { Nose = C.FallT < Hold ? 0.0 : 18.0; Bank = 0.0; }
				else if (C.FallT < Hold) { Nose = C.Gam / Deg; Bank = C.Bank / Deg; }
				else { Nose = -(60.0 + 8.0 * C.Look); Bank = C.Turn * 25.0; }
				break;
			}
			case FallHow::Glide:
				// A long, shallow, steady glide on dead engines.
				Nose = -std::clamp(std::atan2(Sink, C.VMin * 1.1) / Deg, 3.0, 25.0);
				Bank = C.Turn * 8.0 * (C.Look % 2);
				Turn = C.Turn * 0.03 * (C.Look % 2);
				break;
			case FallHow::Tail:
				// A heavy's tail gone: a steep nose-down plunge, turning.
				Nose = -75.0;
				Bank = C.Turn * 30.0;
				Turn = C.Turn * 0.25;
				break;
			case FallHow::Flamer:
				// A shallow dive that steepens.
				Nose = -(10.0 + (25.0 + 10.0 * C.Look) * std::min(1.0, C.FallT / 15.0));
				Bank = C.Turn * 15.0;
				Turn = C.Turn * 0.1;
				break;
			default:
				// Burning (and struck, going down on fire): banking away and diving, steeper by its look.
				Nose = -(12.0 + (18.0 + 12.0 * C.Look) * std::min(1.0, C.FallT / 10.0));
				Bank = C.Turn * (25.0 + 10.0 * C.Look);
				Turn = C.Turn * 0.2;
				break;
			}
			// (no steeper than the drop allows at its speed, no shallower than the drop asks)
			const double Want = Nose * Deg;
			C.Gam += (Want - C.Gam) * std::min(1.0, Dt * (C.Falling == FallHow::Pilot ? 1.2 : 0.6));
			C.Bank += (Bank * Deg - C.Bank) * std::min(1.0, Dt * 1.5);
			C.Hdg = Wrap(C.Hdg + Turn * Dt);
			const double Down = std::max(std::sin(-C.Gam), 0.05);
			const double Pace = std::clamp(Sink / Down, 35.0, C.VTop + 20.0);
			C.Spd += (Pace - C.Spd) * std::min(1.0, Dt * 0.8);
			C.P += Ahead(C.Hdg, C.Gam) * (C.Spd * Dt);
			C.FallV = Ahead(C.Hdg, C.Gam) * C.Spd;
		}
		const Vec Moved = bTumbling ? (C.Falling == FallHow::Spin ? C.FallV + Ahead(C.Hdg) * C.Spd : C.FallV) : Velocity(C);
		if (C.TrailKey) Emit(FxKind::TrailMove, C.Falling == FallHow::Flamer ? C.P : Local(C, C.Form.Engines.empty() ? Vec() : C.Form.Engines[0]), std::string(), C.TrailKey).Vel = Moved;
		// (the ground where it is now, after this frame's move: on a steep slope, where it was is not where it lands)
		const double Floor = GroundAt(C.P.X, C.P.Y);
		// (into the deck, or under it: down a hole and under a tile, its debris row, told the deck, stops shedding)
		const bool bCloud = IntoDeck(C.FallDeck, C.P);
		Fx& Moving = Emit(FxKind::FallMove, C.P, std::string(), C.Key);
		Moving.Vel = Moved;
		if (C.bDebrisToDeck && !bCloud)
		{
			// (its pieces leave sight at the height the row was told: once it is no longer over a drawn tile above that
			// height (a hole, past the deck's reach, under a tile's foot, or under that height), the row stops shedding,
			// so no piece is shed where it would fade in clear air)
			const DeckSpan Here = DeckHere(C.P.X, C.P.Y);
			if (Here.Hide <= 0.0 || C.P.Z <= Here.Foot || C.P.Z < C.DebrisDeck)
			{
				Moving.Deck = -1.0;
				C.bDebrisToDeck = false;
			}
		}
		// Parachutes, one after another, as the crew get out: when the battle says, else every second or two.
		if (C.ChutesLeft > 0 && C.FallT > C.ChuteAt)
		{
			Thing Chute;
			Chute.Kind = ThingKind::Chute;
			Chute.At = Local(C, Vec(-C.Form.Long * 0.3, 0.0, -1.5));
			Chute.Vel = Moved * 0.25;
			Chute.Size = 7.0;
			Chute.Deck = DeckFor(Chute.At);
			Small.push_back(Chute);
			if (!C.ChuteCue.empty()) Emit(FxKind::Sound, Chute.At, C.ChuteCue);
			C.ChutesLeft--;
			if (C.ChuteNext < static_cast<int>(C.ChuteTimes.size()) && C.FallT - C.ChuteTimes[static_cast<size_t>(C.ChuteNext)] > Dt + 1e-6) Check.ChutesOffTime++;
			C.ChuteNext++;
			C.ChuteAt = C.ChuteNext < static_cast<int>(C.ChuteTimes.size()) ? C.ChuteTimes[static_cast<size_t>(C.ChuteNext)] : C.ChuteAt + Roll.U(1.0, 2.5);
			Check.Chutes++;
		}
		// (it strikes when it reaches the ground; a long glide may take its whole dur, and a little over)
		if (C.P.Z <= Floor + 2.0 || bCloud || C.FallT > std::max(400.0, C.Dur + 60.0))
		{
			const bool bStruck = !bCloud && C.P.Z <= Floor + 2.0;
			if (bStruck)
			{
				const double Off = std::fabs(C.FallT - C.Dur);
				(Off <= std::max(6.0, 0.25 * C.Dur) ? Check.FallsOnTime : Check.FallsOffTime) += 1;
				Check.MostOffTime = std::max(Check.MostOffTime, Off);
			}
			if (bStruck)
			{
				// A ditching (a glide onto the sea under control): a skip and a long spray, then it floats a while.
				const bool bDitching = C.Falling == FallHow::Glide && Floor <= 0.5;
				const bool bBelly = C.Falling == FallHow::Glide && Floor > 0.5;
				Fx& End = Emit(Floor > 0.5 ? FxKind::Crash : FxKind::Splash, Vec(C.P.X, C.P.Y, Floor), bDitching ? "ditch" : bBelly ? "belly" : "plane");
				End.Size = C.Form.Span;
				if (bDitching || bBelly)
				{
					C.P.Z = Floor + (bDitching ? 0.3 : 1.0);
					C.Ended = C.FallT;
					Check.Ditched += bDitching ? 1 : 0;
					if (C.TrailKey) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey); C.TrailKey = 0; }
					return;
				}
			}
			if (C.TrailKey) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey); C.TrailKey = 0; }
			Emit(FxKind::FallStop, C.P, std::string(), C.Key);
			C.Doing = Role::Gone;
			C.bShown = false;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The scene's room for trails.

	bool TrailRoom::On(int Key) const
	{
		for (const One& L : Lit) if (L.Key == Key) return true;
		return false;
	}

	bool TrailRoom::Start(int Key, bool bPiece, int& OutPutOut)
	{
		OutPutOut = 0;
		// (lit again with a new kind: it yields from now as that kind does, a dead engine's thin streak as a piece)
		for (One& L : Lit) if (L.Key == Key) { L.bPiece = bPiece; return true; }
		if (static_cast<int>(Lit.size()) >= Most && !bPiece)
		{
			for (size_t K = 0; K < Lit.size(); ++K)
			{
				if (!Lit[K].bPiece) continue;
				OutPutOut = Lit[K].Key;
				Lit.erase(Lit.begin() + static_cast<long>(K));
				++PutOut;
				break;
			}
		}
		if (static_cast<int>(Lit.size()) >= Most)
		{
			++Refused;
			RefusedOwn += bPiece ? 0 : 1;
			return false;
		}
		One L;
		L.Key = Key;
		L.bPiece = bPiece;
		Lit.push_back(L);
		Peak = std::max(Peak, static_cast<int>(Lit.size()));
		return true;
	}

	bool TrailRoom::Stop(int Key)
	{
		for (size_t K = 0; K < Lit.size(); ++K)
		{
			if (Lit[K].Key != Key) continue;
			Lit.erase(Lit.begin() + static_cast<long>(K));
			return true;
		}
		return false;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The effects kit's room for debris.

	int DebrisRoom::Holding(double T)
	{
		Lots.erase(std::remove_if(Lots.begin(), Lots.end(), [T](const Lot& L) { return L.Until <= T; }), Lots.end());
		int Sum = 0;
		for (const Lot& L : Lots) Sum += L.Puffs;
		return Sum;
	}

	bool DebrisRoom::Take(double T, const Fx& Fall)
	{
		// (the most each sheds: air_debris fighter 30 to 50, bomber 60 to 100 at a span of 20 m, burn_sheds 5 to 10,
		// spin_sheds 2 to 5; a light plate sinks at 7 m/s at the slowest, and none is kept past most_s, 240 s)
		const int Puffs = Fall.What == "explode" ? (Fall.Size >= 20.0 ? 100 : 50) : Fall.What == "spin" ? 5 : 10;
		const int Held = Holding(T);
		const bool bBlast = Fall.What == "explode";
		if (Held + Puffs > (bBlast ? Most : Most - Keep))
		{
			++Refused;
			RefusedBlasts += bBlast ? 1 : 0;
			return false;
		}
		Lot L;
		L.Until = T + std::min(240.0, std::max(0.0, Fall.At.Z - Fall.Deck) / 7.0 + 10.0);
		L.Puffs = Puffs;
		Lots.push_back(L);
		Peak = std::max(Peak, Held + Puffs);
		++Given;
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// What happens.

	int Show::PickToFall(const Event& E)
	{
		std::vector<int> Keys = LiveOf(E.Unit);
		if (Keys.empty()) return -1;
		if (E.Strag >= 0)
		{
			for (int K : Keys) if (Find(K)->StragId == E.Strag) return K;
		}
		// The one an enemy fighter of the group that brought it down is firing at, or has just fired at.
		if (E.By >= 0)
		{
			for (const auto& Pair : Crafts)
			{
				const Craft& Shooter = Pair.second;
				if (Shooter.Group != E.By || Shooter.Target < 0 || !Live(Shooter)) continue;
				const Craft* T = Find(Shooter.Target);
				if (T && T->Group == E.Unit && Live(*T) && T->Doing != Role::Ground && T->StragId < 0 && (Shooter.bFiring || Shooter.Step == Phase::Break || Shooter.Step == Phase::Turn)) return Shooter.Target;
			}
		}
		// Any in a fight; then any. A straggler the battle still counts is kept for the end it gives it (its fire put
		// out, its limp home, its own loss), unless no other is left.
		for (int K : Keys) if (Find(K)->Doing == Role::Fight && Find(K)->StragId < 0) return K;
		std::vector<int> Flying, Lame;
		for (int K : Keys)
		{
			if (Find(K)->Doing == Role::Ground) continue;
			(Find(K)->StragId < 0 ? Flying : Lame).push_back(K);
		}
		if (Flying.empty()) Flying = Lame;
		if (Flying.empty()) return Keys.back();
		return Flying[static_cast<size_t>(Roll.U() * Flying.size()) % Flying.size()];
	}

	void Show::StartFall(Craft& C, const Event& E)
	{
		C.Doing = Role::Fall;
		C.Step = Phase::None;
		C.FallT = 0.0;
		C.bFiring = false;
		C.TrailPlan.clear();
		// (a second dead engine's ribbon ends here; the way down lays its own from the engine the battle names)
		if (C.TrailKey2) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey2); C.TrailKey2 = 0; }
		C.ChutesLeft = E.Chutes;
		C.ChuteTimes = E.ChuteAt;
		C.ChuteNext = 0;
		C.ChuteAt = !C.ChuteTimes.empty() ? C.ChuteTimes.front() : Roll.U(2.0, 5.0);
		C.ChuteCue = E.ChuteCue.empty() && E.Chutes > 0 ? "parachute_open" : E.ChuteCue;
		// The way down the battle names (the air rules pick it from the situation; the scene never does). A recording
		// from before the ways down names only the old three (explode, burn, spin).
		std::string How = E.How.empty() ? (E.Kind == "explode" || E.Kind == "spin" ? E.Kind : std::string("burn")) : E.How;
		C.FallName = How;
		C.Look = std::clamp(E.Look, 0, 3);
		C.Turn = E.Roll < 0 ? -1.0 : E.Roll > 0 ? 1.0 : (E.Hand < 0 ? -1.0 : E.Hand > 0 ? 1.0 : C.Hand);
		C.bHeavy = !C.bFighter && C.Form.Engines.size() >= 2;
		C.FireAt = E.FireAt;
		C.BoomAt = E.Boom;
		C.bDitch = E.bDitch;
		C.bLit = false;
		C.Ended = -1.0;
		C.Spin = 0.0;
		C.FallV = Velocity(C);
		// (an old recording says nothing of how long: a fighter is down in under a minute from its height, a heavy longer)
		C.Dur = E.Dur > 0.0 ? E.Dur : (C.bFighter ? 45.0 : 75.0);
		// A collision: the fighter comes apart; the bomber loses what it hit, a wing (looks 0, 1) or its tail (2, 3).
		// Struck by another's fall: on fire (even looks) or cut in two (odd).
		bool bCut = false;
		if (How == "collide") How = C.bFighter ? "breakup" : C.Look < 2 ? "wing" : "tail";
		if (How == "struck" && C.Look % 2 == 1) { How = "breakup"; bCut = true; }
		C.Falling = How == "explode" ? FallHow::Explode : How == "spin" ? FallHow::Spin : How == "flamer" ? FallHow::Flamer : How == "wing" ? FallHow::Wing
		          : How == "tail" ? FallHow::Tail : How == "pilot" ? FallHow::Pilot : How == "glide" ? FallHow::Glide : How == "breakup" ? FallHow::Breakup
		          : How == "struck" ? FallHow::Struck : FallHow::Burn;
		if (C.Falling == FallHow::Struck && C.FireAt < 0.0) C.FireAt = 0.0;
		C.FallDeck = DeckFor(C.P);
		Check.Falls++;
		Check.FallsBy[C.FallName]++;
		if (E.bByFlak) Emit(FxKind::Flak, C.P + Vec(Roll.U(-4.0, 4.0), Roll.U(-4.0, 4.0), Roll.U(-2.0, 3.0)), "heavy"); // (the one burst laid on a model: this one falls to it)
		if (!E.Cue.empty()) Emit(FxKind::Sound, C.P, E.Cue);
		const Vec V = Velocity(C);
		{
			// The debris row, for the effects kit (its three kinds: blown apart, burning, spinning; Text the way down, for
			// the kit that knows more): where the pieces leave sight is where the deck hides them, for a fall begun over a
			// drawn tile's foot, else the ground (and the row stops shedding once the fall is no longer over a drawn tile
			// above that height).
			const bool bApart = C.Falling == FallHow::Explode || C.Falling == FallHow::Breakup;
			Fx& F = Emit(FxKind::Fall, C.P, bApart ? "explode" : C.Falling == FallHow::Spin ? "spin" : "burn", C.Key);
			F.Text = C.FallName;
			F.Vel = V;
			F.Size = C.Form.Span;
			const DeckSpan Here = DeckHere(C.P.X, C.P.Y);
			C.bDebrisToDeck = Here.Hide > 0.0 && C.P.Z > Here.Foot;
			F.Deck = C.bDebrisToDeck ? std::min(Here.Hide, C.P.Z) : GroundAt(C.P.X, C.P.Y);
			C.DebrisDeck = F.Deck;
		}
		// Which engine: the one the battle names (engine k of N is floor(engineAt N) + 1), else one of the inner pair.
		size_t Engine = 0;
		if (!C.Form.Engines.empty())
		{
			const size_t N = C.Form.Engines.size();
			Engine = E.EngineAt >= 0.0 ? std::min(N - 1, static_cast<size_t>(std::floor(E.EngineAt * static_cast<double>(N))))
			                           : (N > 1 ? (Roll.U() < 0.5 ? 1 : N - 2) : 0);
			// (its trail follows from engine 0's place: the burning one put there, the list otherwise as it was)
			std::swap(C.Form.Engines[0], C.Form.Engines[Engine]);
		}
		const Vec EngineAt = Local(C, C.Form.Engines.empty() ? Vec() : C.Form.Engines[0]);
		// A liquid-cooled engine hit in its cooling trails white glycol; a radial, blue-grey oil smoke (the art director).
		const bool bCooled = C.bLiquid;
		switch (C.Falling)
		{
		case FallHow::Explode:
			BlowApart(C, true);
			break;
		case FallHow::Breakup:
		{
			// Into three or four big pieces at the real joints: the wings flutter, the fuselage falls fastest, the tail
			// tumbles; one or two of them smoke. Cut in two (struck): the fore and the aft.
			if (C.TrailKey) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey); C.TrailKey = 0; }
			const Vec R = RightOf(C.Hdg);
			const int Smoking = 1 + C.Look % 2;
			int Made = 0;
			auto Part = [&](const Vec& At, const Vec& Push, double Size, double Flutter)
			{
				Piece(C, 2 + Made, At, V * 0.85 + Push, Size, Flutter, Made < Smoking ? (Made == 0 ? "fire" : "smoke") : nullptr, 120.0);
				Made++;
			};
			if (bCut)
			{
				Part(Local(C, Vec(C.Form.Long * 0.2, 0.0, 0.0)), Vec(0.0, 0.0, -4.0), C.Form.Long * 0.55, 0.0);
				Part(Local(C, Vec(-C.Form.Long * 0.3, 0.0, 0.0)), Vec(0.0, 0.0, 2.0), C.Form.Long * 0.4, 0.0);
			}
			else
			{
				Part(C.P, Vec(0.0, 0.0, -6.0), C.Form.Long * 0.6, 0.0);                                        // the fuselage
				Part(C.P + R * (C.Form.Span * 0.3), R * 9.0 + Vec(0.0, 0.0, 4.0), C.Form.Span * 0.42, 14.0);   // the starboard wing
				if (C.Look < 2) Part(C.P - R * (C.Form.Span * 0.3), R * -9.0 + Vec(0.0, 0.0, 4.0), C.Form.Span * 0.42, 14.0); // the port wing
				Part(Local(C, Vec(-C.Form.Long * 0.45, 0.0, 0.0)), Vec(0.0, 0.0, 6.0), C.Form.Span * 0.3, 0.0); // the tail
			}
			C.bShown = false;
			break;
		}
		case FallHow::Wing:
		{
			// The wing on its side tears away and flutters, slowly, flat like a leaf, far above the wreck; fuel mist
			// from the torn root, on fire in looks 2 and 3 (the art director's break point and root fire).
			const double Out_ = (C.Turn < 0.0 ? -1.0 : 1.0) * C.Form.Span * (C.Look == 1 ? 0.2 : C.Look == 3 ? 0.35 : 0.28);
			Piece(C, 2, Local(C, Vec(0.0, Out_, 0.0)), V * 0.6 + RightOf(C.Hdg) * (Out_ > 0.0 ? 6.0 : -6.0), C.Form.Span * 0.45, 9.0 + 2.0 * C.Look, nullptr, 90.0);
			if (!C.TrailKey) C.TrailKey = C.Key * 8 + 1;
			Fx& T = Emit(FxKind::TrailStart, Local(C, Vec(0.0, Out_ * 0.3, 0.0)), C.Look >= 2 ? "fire" : "glycol", C.TrailKey);
			T.Vel = V;
			T.Size = C.Form.Span;
			break;
		}
		case FallHow::Tail:
			// The tail unit breaks away, fluttering (looks 0, 2) or tumbling, a little smoke (1, 3) or none.
			Piece(C, 2, Local(C, Vec(-C.Form.Long * 0.42, 0.0, 0.5)), V * 0.7, C.Form.Span * 0.3, C.Look % 2 == 0 ? 11.0 : 0.0, nullptr, 90.0);
			if (C.Look % 2 == 1)
			{
				if (!C.TrailKey) C.TrailKey = C.Key * 8 + 1;
				Fx& T = Emit(FxKind::TrailStart, EngineAt, "smoke", C.TrailKey);
				T.Vel = V;
				T.Size = C.Form.Span * 0.6;
			}
			break;
		case FallHow::Pilot:
			// Clean and silent: no fire, no smoke, no pieces (a straggler's smoke stops).
			if (C.TrailKey) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey); C.TrailKey = 0; }
			break;
		case FallHow::Spin:
		{
			// A helix round a middle line, the aircraft on it where it is now; a smoke (looks 0, 2), a thin vapour (1)
			// or nothing (3).
			C.Spin = Wrap(C.Hdg - C.Turn * Pi * 0.5);
			const double Radius = C.Look < 2 ? 6.0 + C.Form.Span * 0.4 : 14.0 + C.Form.Span * 0.8;
			C.Axis = C.P - Vec(std::sin(C.Spin), std::cos(C.Spin), 0.0) * Radius;
			C.FallV = Vec(V.X * 0.5, V.Y * 0.5, std::min(V.Z, 0.0));
			if (C.Look == 3) { if (C.TrailKey) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey); C.TrailKey = 0; } break; }
			if (!C.TrailKey) C.TrailKey = C.Key * 8 + 1;
			Fx& T = Emit(FxKind::TrailStart, EngineAt, C.Look == 1 ? "glycol" : "smoke", C.TrailKey);
			T.Vel = V;
			T.Size = C.Form.Span * (C.Look == 1 ? 0.5 : 0.8);
			break;
		}
		case FallHow::Glide:
			// Dead engines, a thin smoke (looks 0, 2) or none.
			if (C.TrailKey) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey); C.TrailKey = 0; }
			if (C.Look % 2 == 0)
			{
				C.TrailKey = C.Key * 8 + 1;
				Fx& T = Emit(FxKind::TrailStart, EngineAt, "smoke", C.TrailKey);
				T.Vel = V;
				T.Size = C.Form.Span * 0.4;
			}
			break;
		case FallHow::Struck:
			// Struck by another's fall: a burst of debris where it met, then down burning.
			for (int K = 0; K < 3; ++K)
			{
				Piece(C, 2 + K, C.P + Vec(Roll.U(-1.0, 1.0), Roll.U(-1.0, 1.0), Roll.U(-0.5, 0.5)) * (C.Form.Span * 0.2),
				      V * 0.7 + Vec(Roll.U(-1.0, 1.0), Roll.U(-1.0, 1.0), Roll.U(-0.3, 1.0)) * 12.0, C.Form.Span * Roll.U(0.08, 0.16), 0.0, nullptr, 20.0);
			}
			[[fallthrough]];
		default:
		{
			// Burning (a flamer from its wing root, a burn from one engine): first the cause's smoke, white glycol or
			// blue-grey oil, then fire when the battle says; a straggler's smoke already running goes on.
			if (C.Falling == FallHow::Flamer && C.FireAt < 0.0) C.FireAt = 0.4;
			const bool bAtOnce = C.FireAt <= 0.0;
			if (!C.TrailKey) C.TrailKey = C.Key * 8 + 1;
			Fx& T = Emit(FxKind::TrailStart, C.Falling == FallHow::Flamer ? C.P : EngineAt, bAtOnce ? "fire" : bCooled ? "glycol" : "smoke", C.TrailKey);
			T.Vel = V;
			T.Size = C.Form.Span;
			C.bLit = bAtOnce;
			break;
		}
		}
	}

	void Show::BlowApart(Craft& C, bool bFireball)
	{
		// Blown apart: a short torn flash and the pieces thrown out (4 to 8 by its look, one more for a heavy), the
		// biggest trailing fire and smoke, two or three burning fuel streaks among them.
		const Vec V = C.Doing == Role::Fall ? C.FallV : Velocity(C);
		if (bFireball)
		{
			Fx& B = Emit(FxKind::Blast, C.P, "plane", C.Key);
			B.Vel = V;
			B.Size = C.Form.Span;
		}
		if (C.TrailKey) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey); C.TrailKey = 0; }
		const int Pieces = std::min(8, 4 + C.Look + (C.bHeavy ? 1 : 0));
		const int Burning = C.Look >= 2 ? 3 : 2;
		for (int K = 0; K < Pieces && K < 6; ++K)
		{
			const Vec At = C.P + Vec(Roll.U(-1.0, 1.0), Roll.U(-1.0, 1.0), Roll.U(-0.5, 0.5)) * (C.Form.Span * 0.25);
			const Vec Thrown = V * 0.8 + Vec(Roll.U(-1.0, 1.0), Roll.U(-1.0, 1.0), Roll.U(-0.4, 1.0)) * 18.0;
			// (a wing panel flutters; an engine and the rest fall fast)
			const double Size = C.Form.Span * (K == 0 ? 0.45 : Roll.U(0.12, 0.3));
			Piece(C, 2 + K, At, Thrown, Size, K == 1 ? 12.0 : 0.0, K < Burning ? (K == 0 ? "fire" : K == 1 ? "smoke" : "fire") : nullptr);
		}
		C.Falling = FallHow::Explode;
		C.FallT = 0.0;
		C.bShown = false;
	}

	void Show::Piece(const Craft& C, int Slot, const Vec& At, const Vec& Vel, double Size, double Flutter, const char* Trail, double Life)
	{
		Thing P;
		P.Kind = ThingKind::Piece;
		P.Key = C.Key * 8 + std::clamp(Slot, 2, 7);
		P.At = At;
		P.Vel = Vel;
		P.Size = Size;
		P.Spin = Roll.U(0.0, 6.0);
		P.Deck = C.FallDeck;
		P.Flutter = Flutter;
		P.Life = Life;
		// (a trail holds about 240 of the effects kit's 3,000 puffs, and the kit's debris row is the small pieces)
		P.bTrail = Trail != nullptr;
		Small.push_back(P);
		if (Trail) { Fx& T = Emit(FxKind::TrailStart, P.At, Trail, P.Key); T.Vel = P.Vel; T.Size = P.Size; T.Text = "piece"; }
	}

	void Show::HitSurvived(const Event& E)
	{
		const std::vector<int> Keys = LiveOf(E.Unit);
		if (E.What == "air:fireout" || E.What == "air:regain")
		{
			for (int Key : Keys)
			{
				Craft& C = *Find(Key);
				if (C.StragId != E.Strag || C.Doing == Role::Fall || C.Doing == Role::Gone || C.Doing == Role::Ground) continue;
				if (E.What == "air:fireout")
				{
					// The crew put the fire out: the tongues shrink back, the extinguisher's white-grey puff, then the
					// thin dark streak that stays (the art director, 15:38Z, on Brian's 15:29Z ask), its airscrew feathered.
					C.bFireOut = true;
					C.bFeathered = C.bFeathered || E.bFeather;
					LayTrail(C, "doused");
					C.TrailPlan = { { AirT + 2.0, "streak" } };
					Check.FiresOut++;
				}
				else
				{
					// On three engines it has caught its box up: back into its slot, closing slowly.
					C.Doing = Role::Slot;
					C.StragId = -1;
					C.bJoining = true;
					Check.Regained++;
				}
				break;
			}
			return;
		}
		// A hit survived, on the model in the formation's slot the battle names (any one still flying in it).
		std::vector<int> Flying;
		for (int Key : Keys)
		{
			const Craft& C = *Find(Key);
			if (C.Doing != Role::Ground && C.Doing != Role::Fall && C.Doing != Role::Gone) Flying.push_back(Key);
		}
		if (Flying.empty()) return;
		// (a hit that lights a trail goes to the first in the slot's turn not already trailing one, so none is lost)
		const bool bLights = E.Mark == "smoke" || E.Mark == "fireout";
		size_t Pick = static_cast<size_t>(std::max(0, E.Slot)) % Flying.size();
		for (size_t K = 0; bLights && K < Flying.size(); ++K)
		{
			const size_t Try = (Pick + K) % Flying.size();
			if (!Find(Flying[Try])->TrailKey) { Pick = Try; break; }
		}
		Craft& C = *Find(Flying[Pick]);
		Check.HitsShown++;
		const Vec V = Velocity(C);
		// The sparkle where the stream met it: a few strikes winking over it.
		for (int K = 0; K < 3; ++K)
		{
			const Vec Spot = Local(C, Vec(Roll.U(-0.4, 0.4) * C.Form.Long, (E.Hand != 0 ? E.Hand * Roll.U(0.05, 0.45) : Roll.U(-0.45, 0.45)) * C.Form.Span, Roll.U(-0.2, 0.5)));
			Fx& H = Emit(FxKind::Hit, Spot, "hmg");
			H.Normal = Norm(Spot - C.P, Vec(0.0, 0.0, 1.0));
			H.Vel = V;
		}
		if (E.Mark == "pieces")
		{
			// A few panels torn off, fluttering away and turning flat.
			const int Count = 2 + static_cast<int>(Roll.U() * 2.0);
			for (int K = 0; K < Count; ++K)
			{
				Piece(C, 5 + K, Local(C, Vec(Roll.U(-0.3, 0.3) * C.Form.Long, Roll.U(-0.4, 0.4) * C.Form.Span, 0.3)),
				      V * 0.9 + Vec(Roll.U(-1.0, 1.0), Roll.U(-1.0, 1.0), Roll.U(0.0, 1.0)) * 6.0, C.Form.Span * Roll.U(0.04, 0.08), Roll.U(5.0, 9.0), nullptr, 25.0);
			}
		}
		if ((E.Mark == "smoke" || E.Mark == "fireout") && !C.TrailKey)
		{
			// From the engine the battle names (0 port to 1 starboard): a puff, then a thin trail for its time, then
			// clear; or an engine fire in the box that its crew put out: it burns for its time, the extinguisher's puff,
			// then the feathered engine's thin dark streak, which stays (the air thread's engine-out note).
			const int N = static_cast<int>(C.Form.Engines.size());
			const int Engine = N > 1 && E.EngineAt >= 0.0 ? 1 + static_cast<int>(std::lround(std::min(1.0, E.EngineAt) * (N - 1))) : 1;
			C.TrailAt = EngineOf(C, Engine);
			const double For = std::max(E.For, 3.0);
			if (E.Mark == "fireout")
			{
				C.DeadEngines = { Engine };
				C.bFeathered = E.bFeather || N > 1;
				C.bFireOut = true;
				if (C.SootAt < 0.0) C.SootAt = AirT;
				LayTrail(C, "fire");
				C.TrailPlan = { { AirT + For, "doused" }, { AirT + For + 2.0, "streak" } };
				Check.FiresOut++;
			}
			else
			{
				LayTrail(C, "smoke");
				C.TrailPlan = { { AirT + For, "" } };
			}
		}
	}

	void Show::Drop(Craft& C, bool bTorpedo)
	{
		if (C.bDropped) return;
		C.bDropped = true;
		const Vec V = Velocity(C);
		const int Count = bTorpedo ? 1 : C.bFighter ? 1 : C.Form.Span > 25.0 ? 4 : C.Form.Span > 18.0 ? 3 : 1;
		for (int K = 0; K < Count; ++K)
		{
			Thing B;
			B.Kind = ThingKind::Bomb;
			B.bTorpedo = bTorpedo;
			// A stick: each a little behind the last as they leave the bay.
			B.At = Local(C, Vec(-K * 1.5, 0.0, -1.2)) - V * (K * 0.25);
			B.Vel = V;
			B.Size = bTorpedo ? 5.5 : C.Form.Span > 25.0 ? 2.2 : 1.8;
			B.Heading = C.Hdg;
			Small.push_back(B);
		}
	}

	void Show::Play(const Event& E)
	{
		const bool bShips = E.What == "air:ship-hit" || E.What == "air:ship-sunk";
		if (bShips)
		{
			// Held until the bombs or the torpedoes could have reached her: the stage shows the hit as the bombs land.
			const double Wait = E.Strike == "level" ? 26.0 : E.Strike == "dive" ? 30.0 : E.Strike == "torpedo" ? 14.0 : 20.0;
			Later.push_back(Held{ E, AirT + Wait });
			return;
		}
		Release(E);
	}

	void Show::Release(const Event& E)
	{
		auto GroupIt = Now.find(E.Unit);
		const double Alt = E.Alt >= 0.0 ? E.Alt : GroupIt != Now.end() ? GroupIt->second.Alt : 0.0;
		const Vec At = E.bAt ? Vec(E.X, E.Y, Alt) : GroupIt != Now.end() ? Vec(GroupIt->second.X, GroupIt->second.Y, Alt) : Vec(TargetX, TargetY, Alt);
		if (!E.Text.empty())
		{
			Fx& D = Emit(FxKind::Dispatch, At, E.What, E.Unit);
			D.Text = E.Text;
			D.Side = E.Side;
		}
		if (E.What == "air:ship-hit")
		{
			Emit(FxKind::ShipHit, At, E.Strike, E.Ship);
			return;
		}
		if (E.What == "air:ship-sunk")
		{
			Fx& S = Emit(FxKind::ShipSunk, At, E.How, E.Ship);
			S.Size = E.bBig ? 1.0 : 0.0;
			S.Text = E.Name;
			return;
		}
		if (E.What == "air:lost")
		{
			int Key = PickToFall(E);
			if (Key < 0 && GroupIt != Now.end())
			{
				// Nothing left to fall in the picture (a group already thinned): one falls from where the battle says.
				Craft& C = Make(GroupIt->second, At, GroupIt->second.Face, GroupIt->second.bFighter ? 120.0 : 85.0, Role::Slot);
				Key = C.Key;
			}
			if (Key >= 0)
			{
				StartFall(*Find(Key), E);
				Bands[E.Unit].Owed.push_back(AirT);
			}
			return;
		}
		if (!E.Cue.empty()) Emit(FxKind::Sound, At, E.Cue);
		if (E.What == "air:hit" || E.What == "air:fireout" || E.What == "air:regain")
		{
			HitSurvived(E);
			return;
		}
		if (E.What == "contact")
		{
			// The pass the contact names: kept for the fighters of the pair, against the other group.
			auto Other = Now.find(E.With);
			if (GroupIt != Now.end() && Other != Now.end())
			{
				const int Fighters = GroupIt->second.bFighter ? E.Unit : E.With;
				const int Box = Fighters == E.Unit ? E.With : E.Unit;
				if (!E.Pass.empty()) Bands[Fighters].Pass[Box] = E.Pass;
			}
			return;
		}
		if (E.What == "air:straggle")
		{
			// A bomber falls out of the box: one at the box's edge, smoke from the engine it lost.
			// (one in its slot; else, the box being in its run, a bomber diving or in a fight)
			std::vector<int> Keys = LiveOf(E.Unit);
			std::vector<int> Pick;
			for (auto It = Keys.rbegin(); It != Keys.rend(); ++It) if (Find(*It)->Doing == Role::Slot) Pick.push_back(*It);
			for (auto It = Keys.rbegin(); It != Keys.rend(); ++It)
				if (!Find(*It)->bFighter && (Find(*It)->Doing == Role::Dive || Find(*It)->Doing == Role::Fight)) Pick.push_back(*It);
			for (int Each : Pick)
			{
				Craft& C = *Find(Each);
				if (C.StragId >= 0) continue;
				C.Doing = Role::Straggle;
				C.Step = Phase::None;
				C.bFiring = false;
				C.StragId = E.Strag;
				C.StragAt = AirT;
				// (each dead engine trails from its own nacelle for as long as it is out, a second one its own ribbon)
				C.DeadEngines = E.Eng.empty() ? std::vector<int>{ 1 } : E.Eng;
				C.bFeathered = E.bFeather;
				C.bFireOut = false;
				if (C.SootAt < 0.0) C.SootAt = AirT;
				const Vec Socket = EngineOf(C, C.DeadEngines.front());
				C.Hand = Socket.Y < 0.0 ? -1.0 : 1.0;
				if (C.TrailKey2) { Emit(FxKind::TrailStop, C.P, std::string(), C.TrailKey2); C.TrailKey2 = 0; }
				C.TrailAt = Socket;
				if (C.DeadEngines.size() > 1 && C.Form.Engines.size() > 1 && C.DeadEngines[1] != C.DeadEngines[0])
				{
					C.TrailKey2 = C.Key * 8 + 7;
					C.TrailAt2 = EngineOf(C, C.DeadEngines[1]);
				}
				// (falling out burning, the battle says, or its dead engine smoking)
				DeadEngineTrail(C, E.bFire);
				// An escort's cover: two of its aircraft weave over it.
				if (GroupIt != Now.end())
				{
					for (const Strag& S : GroupIt->second.Strags)
					{
						if (S.Id != E.Strag || S.Cover < 0) continue;
						int Sent = 0;
						for (int Key : LiveOf(S.Cover))
						{
							Craft& Escort = *Find(Key);
							if (Sent >= 2 || Escort.Doing != Role::Slot) continue;
							Escort.Doing = Role::Cover;
							Escort.Target = C.Key;
							Sent++;
						}
					}
				}
				break;
			}
			return;
		}
		if (E.What == "air:limped")
		{
			for (auto& Pair : Crafts)
			{
				Craft& C = Pair.second;
				if (C.Group == E.Unit && C.StragId == E.Strag && C.Doing == Role::Straggle) Leave(C);
			}
			return;
		}
		if (E.What == "air:bombs" || E.What == "air:jettison")
		{
			const bool bJettison = E.What == "air:jettison";
			const std::string Strike = GroupIt != Now.end() ? GroupIt->second.Strike : std::string();
			std::vector<int> Keys = LiveOf(E.Unit);
			Bands[E.Unit].bDropped = true;
			if (!bJettison && Strike == "dive")
			{
				// One after another over on a wing and down, each on a ship of her own among those nearest the aim.
				std::vector<const Ship*> Marks;
				for (const Ship& S : ShipsNow) if (!S.bSunk) Marks.push_back(&S);
				std::sort(Marks.begin(), Marks.end(), [&At](const Ship* A, const Ship* B) { return Flat(A->At - At) < Flat(B->At - At); });
				if (Marks.size() > 4) Marks.resize(4);
				int K = 0;
				for (int Key : Keys)
				{
					Craft& C = *Find(Key);
					if (C.Doing != Role::Slot) continue;
					C.Doing = Role::Dive;
					C.Step = Phase::Peel;
					C.StepT = -K * 0.9;
					C.Target = Marks.empty() ? -1 : Marks[static_cast<size_t>(K) % Marks.size()]->Id;
					K++;
				}
				return;
			}
			for (int Key : Keys)
			{
				Craft& C = *Find(Key);
				if (C.Doing == Role::Ground) continue;
				Drop(C, !bJettison && Strike == "torpedo");
			}
			return;
		}
		if (E.What == "air:rail" || E.What == "air:ruin")
		{
			Emit(FxKind::Smoke, Vec(E.bAt ? E.X : TargetX, E.bAt ? E.Y : TargetY, GroundAt(E.bAt ? E.X : TargetX, E.bAt ? E.Y : TargetY)), E.What);
			return;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Guns, flak, the small things.

	void Show::FireGuns(double Dt)
	{
		struct Want
		{
			double Far = 0.0;
			Fx F;
		};
		std::vector<Want> Wanted;
		auto Muzzle = [](const Shape& S, const std::string& Socket, const std::string& Kind, int Index, int Of) -> Vec
		{
			// The fighters' guns: the hub cannon at the spinner, the cowl pair above it either side; the wing guns
			// numbered from the fuselage outward, odd to port.
			if (Socket == "gun.nose")
			{
				if (Kind == "cannon20") return Vec(S.Long * 0.5, 0.0, 0.0);
				return Vec(S.Long * 0.42, Of > 1 ? (Index % 2 == 0 ? -0.15 : 0.15) : 0.0, 0.3);
			}
			int N = 1;
			if (Socket.size() > 4) N = std::max(1, std::atoi(Socket.c_str() + 4));
			const double Wide = (S.Span / 11.0) * (1.2 + 0.55 * ((N + 1) / 2 - 1));
			return Vec(S.Long * 0.08, (N % 2 == 1 ? -1.0 : 1.0) * Wide, -0.1);
		};
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			if (!C.bFiring || !C.bFighter) continue;
			auto L = Loadouts.find(C.Nation + ":ftr");
			if (L == Loadouts.end()) continue;
			const Vec Nose = Ahead(C.Hdg, C.Gam);
			const Vec V = Velocity(C);
			const double Conv = L->second.Conv > 0.0 ? L->second.Conv : 300.0;
			int Index = 0;
			for (const Gun& Row : L->second.Guns)
			{
				if (Row.Mount != "fixed") continue;
				const int Of = static_cast<int>(Row.At.size());
				for (int K = 0; K < std::max(1, Of); ++K)
				{
					const std::string Socket = Of > 0 ? Row.At[static_cast<size_t>(K)] : std::string("gun.nose");
					if (Has(L->second.Silent, Socket)) { Index++; continue; }
					const Vec Here = Local(C, Muzzle(C.Form, Socket, Row.Kind, K, Of));
					Want W;
					W.F.Kind = FxKind::Gun;
					W.F.Key = C.Key * 64 + Index++;
					W.F.At = Here;
					W.F.Aim = C.P + Nose * (Socket == "gun.nose" ? 450.0 : Conv);
					W.F.Vel = V;
					W.F.What = Row.Kind;
					W.F.Text = Row.Mount;
					W.F.Rate = Row.Rate;
					W.F.Mv = Row.Mv;
					W.Far = Len(Here - Eye);
					Wanted.push_back(W);
				}
			}
			// Rounds striking the one it is firing at, as close as a pass comes.
			const Craft* T = C.Target >= 0 ? Find(C.Target) : nullptr;
			if (T && T->Doing != Role::Gone && Len(T->P - C.P) < 600.0)
			{
				bool bCannon = false;
				for (const Gun& Row : L->second.Guns) bCannon = bCannon || Row.Kind == "cannon20";
				if (Roll.U() < Dt * 6.0)
				{
					const Vec Spot = Local(*T, Vec(Roll.U(-0.4, 0.4) * T->Form.Long, Roll.U(-0.45, 0.45) * T->Form.Span, Roll.U(-0.2, 0.6)));
					Fx& H = Emit(FxKind::Hit, Spot, bCannon && Roll.U() < 0.4 ? "cannon20" : L->second.Guns.empty() ? "rifle" : L->second.Guns.front().Kind);
					H.Normal = Norm(Spot - T->P, Up);
					H.Vel = Velocity(*T);
					Check.Hits++;
				}
			}
		}
		// The gunners of a box under attack: each at his own window, at a fighter inside his arc and his reach, never
		// through his own aircraft.
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			if (C.bFighter || !(C.Doing == Role::Slot || C.Doing == Role::Straggle)) continue;
			auto L = Loadouts.find(C.Nation + ":bmr");
			if (L == Loadouts.end()) continue;
			// The nearest enemy fighter on a pass at this box.
			const Craft* Foe = nullptr;
			double Near = 700.0;
			for (const auto& Other : Crafts)
			{
				const Craft& F = Other.second;
				if (!F.bFighter || F.Side == C.Side || F.Doing != Role::Fight || F.TargetGroup != C.Group) continue;
				const double D = Len(F.P - C.P);
				if (D < Near) { Near = D; Foe = &F; }
			}
			if (!Foe) continue;
			const Vec F0 = Ahead(C.Hdg, C.Gam), R0 = RightOf(C.Hdg), U0 = R0.Cross(F0);
			const Vec Rel = Foe->P - C.P;
			const double Fx_ = Rel.Dot(F0), Fy = Rel.Dot(R0), Fz = Rel.Dot(U0);
			double Yaw = std::atan2(Fy, Fx_) / Deg;
			if (Yaw < 0.0) Yaw += 360.0;
			const double Pitch = std::atan2(Fz, std::sqrt(Fx_ * Fx_ + Fy * Fy)) / Deg;
			std::set<std::string> Used;
			int Index = 32;
			for (const Gun& Row : L->second.Guns)
			{
				if (Row.Mount == "fixed") { Index += 4; continue; }
				// The window: by where the arc looks, or for a power turret by its socket; one gunner to an opening.
				std::vector<std::string> Choices;
				if (Row.bArc)
				{
					double Mid = Row.Yaw0 + std::fmod(Row.Yaw1 - Row.Yaw0 + 360.0, 360.0) * 0.5;
					Mid = std::fmod(Mid + 360.0, 360.0);
					if (Mid < 45.0 || Mid >= 315.0) Choices = { "nose", "chin" };
					else if (Mid < 135.0) Choices = { "waistR" };
					else if (Mid < 225.0) Choices = { "tail" };
					else Choices = { "waistL" };
				}
				else Choices = { "top", "ball", "radio" };
				std::string Window;
				for (const std::string& Ch : Choices) if (!Used.count(Ch) && C.Form.Stations.count(Ch)) { Window = Ch; break; }
				if (Window.empty()) { Index += 4; continue; }
				Used.insert(Window);
				bool bInArc = Near < (Row.Kind == "rifle" ? L->second.ReachRifle : L->second.ReachHeavy);
				if (Row.bArc)
				{
					const double Span_ = std::fmod(Row.Yaw1 - Row.Yaw0 + 360.0, 360.0);
					const double Into = std::fmod(Yaw - Row.Yaw0 + 360.0, 360.0);
					bInArc = bInArc && Into <= Span_ && Pitch >= Row.Pitch0 && Pitch <= Row.Pitch1;
				}
				else
				{
					// A power turret: the top one above the wing, the ball below.
					bInArc = bInArc && (Window == "top" ? Pitch > -5.0 : Window == "ball" ? Pitch < 5.0 : Pitch > 0.0);
				}
				const Vec Station = C.Form.Stations[Window];
				// Never through his own aircraft: the line out of his window must clear the fuselage, the fin and the wings.
				if (bInArc)
				{
					const Vec Way = Norm(Vec(Fx_, Fy, Fz));
					const double Body = std::max(0.9, C.Form.Span * 0.045), Fin = C.Form.Span * 0.18;
					for (double Step = 1.5; Step < 30.0 && bInArc; Step += 1.0)
					{
						const Vec Q = Station + Way * Step;
						const bool bFuselage = std::fabs(Q.X) < C.Form.Long * 0.5 && std::fabs(Q.Y) < Body && std::fabs(Q.Z) < Body * 1.2;
						const bool bTail = Q.X < -C.Form.Long * 0.38 && std::fabs(Q.Y) < 0.5 && Q.Z > 0.0 && Q.Z < Fin;
						const bool bWing = std::fabs(Q.X - C.Form.Long * 0.05) < C.Form.Long * 0.12 && std::fabs(Q.Z) < 0.5 && std::fabs(Q.Y) < C.Form.Span * 0.5;
						if (bFuselage || bTail || bWing) bInArc = false;
					}
				}
				// He fires in bursts while it is there to be fired at.
				if (bInArc && std::sin(AirT * 1.7 + C.Key * 0.37 + Index) > -0.25)
				{
					const int Of = std::max(1, static_cast<int>(Row.At.size()));
					for (int K = 0; K < Of; ++K)
					{
						// A twin mount is one opening: its two guns side by side in it.
						const Vec Here = Local(C, Station + Vec(0.0, Of > 1 ? (K == 0 ? -0.15 : 0.15) : 0.0, 0.0));
						Want W;
						W.F.Kind = FxKind::Gun;
						W.F.Key = C.Key * 64 + Index + K;
						W.F.At = Here;
						W.F.Aim = Foe->P + Velocity(*Foe) * (Near / std::max(300.0, Row.Mv));
						W.F.Vel = Velocity(C);
						W.F.What = Row.Kind;
						W.F.Text = Row.Mount;
						W.F.Rate = Row.Rate;
						W.F.Mv = Row.Mv;
						W.Far = Len(Here - Eye);
						Wanted.push_back(W);
					}
				}
				Index += 4;
			}
		}
		// The nearest guns first, as many as the kit can draw.
		std::sort(Wanted.begin(), Wanted.end(), [](const Want& A, const Want& B) { return A.Far < B.Far; });
		if (static_cast<int>(Wanted.size()) > MostGuns) Wanted.resize(static_cast<size_t>(MostGuns));
		std::set<int> Kept;
		for (const Want& W : Wanted)
		{
			if (Kept.count(W.F.Key)) { Check.SharedWindows++; continue; }
			Kept.insert(W.F.Key);
			Effects.push_back(W.F);
		}
		for (int Key : Firing) if (!Kept.count(Key)) Emit(FxKind::GunStop, Vec(), std::string(), Key);
		Firing = Kept;
		if (!Kept.empty()) Check.GunFrames++;
		Check.MostGunsAtOnce = std::max(Check.MostGunsAtOnce, static_cast<int>(Kept.size()));
	}

	void Show::FlakRound(double Dt)
	{
		for (auto& Pair : Now)
		{
			const Group& G = Pair.second;
			if (G.bFighter || G.Flak.empty()) continue;
			std::vector<int> Keys = LiveOf(G.Id);
			if (Keys.empty()) continue;
			Band& B = Bands[G.Id];
			for (const std::string& Kind : G.Flak)
			{
				// The art director's cadence: about every 2.5 s for the light guns, 1.4 medium, 0.9 heavy, ragged.
				const double Every = Kind == "light" ? 2.5 : Kind == "medium" ? 1.4 : 0.9;
				double& Next = B.FlakNext.emplace(Kind, Roll.U(0.0, Every)).first->second;
				Next -= Dt;
				if (Next > 0.0) continue;
				Next = Every * Roll.U(0.6, 1.4);
				const Craft& C = *Find(Keys[static_cast<size_t>(Roll.U() * Keys.size()) % Keys.size()]);
				const double Span = C.Form.Span;
				// Within about two wingspans, more behind than ahead, and never dead on a model.
				Vec At = C.P - Ahead(C.Hdg) * (Roll.U(-1.0, 2.6) * Span) + RightOf(C.Hdg) * (Roll.U(-2.0, 2.0) * Span) + Up * (Roll.U(-1.0, 1.0) * Span * 0.6);
				for (int Key : Keys)
				{
					const Vec Off = At - Find(Key)->P;
					if (Len(Off) < Span * 0.7) At = Find(Key)->P + Norm(Off, Up) * (Span * 0.8);
				}
				Emit(FxKind::Flak, At, Kind);
				Check.FlakBursts++;
			}
		}
	}

	void Show::SmallThings(double Dt)
	{
		for (Thing& T : Small)
		{
			T.Age += Dt;
			if (T.Kind == ThingKind::Chute)
			{
				// Out and falling, then the canopy open and drifting down with the wind.
				if (!T.bOpen && T.Age > 1.6) T.bOpen = true;
				const Vec Drift = T.bOpen ? Vec(2.5, 1.0, -5.5) : Vec(T.Vel.X * 0.98, T.Vel.Y * 0.98, std::max(T.Vel.Z - Gee * Dt, -45.0));
				T.Vel = T.bOpen ? T.Vel + (Drift - T.Vel) * std::min(1.0, Dt * 1.5) : Drift;
			}
			else if (T.Kind == ThingKind::Piece && T.Flutter > 0.0)
			{
				// A torn wing or panel: it flutters down like a leaf, slowly turning flat, swaying as it goes.
				const double Sway = std::sin(T.Age * 1.7 + T.Spin) * 3.0;
				T.Vel.X += (Sway - T.Vel.X) * std::min(1.0, Dt * 0.5);
				T.Vel.Y += (Sway * 0.6 - T.Vel.Y) * std::min(1.0, Dt * 0.5);
				T.Vel.Z += (-T.Flutter - T.Vel.Z) * std::min(1.0, Dt * 0.9);
				T.Spin += Dt * 1.3;
				if (T.bTrail) Emit(FxKind::TrailMove, T.At, std::string(), T.Key).Vel = T.Vel;
			}
			else if (T.Kind == ThingKind::Piece)
			{
				T.Vel = T.Vel * (1.0 - 0.12 * Dt) + Vec(0.0, 0.0, -Gee * Dt);
				T.Spin += Dt * 4.0;
				if (T.bTrail) Emit(FxKind::TrailMove, T.At, std::string(), T.Key).Vel = T.Vel;
			}
			else
			{
				T.Vel = T.Vel * (1.0 - 0.02 * Dt) + Vec(0.0, 0.0, -Gee * Dt);
			}
			T.At += T.Vel * Dt;
		}
		for (size_t K = 0; K < Small.size();)
		{
			Thing& T = Small[K];
			const double Floor = GroundAt(T.At.X, T.At.Y);
			// (a piece fluttering down drifts as it goes: one that drifts into a drawn tile's side is gone into it there)
			bool bCloud = T.Kind != ThingKind::Bomb && IntoDeck(T.Deck, T.At);
			if (!bCloud && T.Kind == ThingKind::Piece && T.Flutter > 0.0 && (CloudTop > 0.0 || DeckAt) && !(DeckBase > 0.0 && T.At.Z <= DeckBase))
			{
				const DeckSpan Here = DeckHere(T.At.X, T.At.Y);
				bCloud = Here.Hide > 0.0 && T.At.Z > Here.Foot && T.At.Z < Here.Hide;
			}
			// (at most 400 s, a parachute 1,500 s: one from 8 km is down in about 1,450)
			if (T.At.Z > Floor && !bCloud && T.Age < (T.Kind == ThingKind::Chute ? 1500.0 : T.Kind == ThingKind::Piece ? T.Life : 400.0)) { ++K; continue; }
			const Vec Down(T.At.X, T.At.Y, Floor);
			if (T.Kind == ThingKind::Bomb)
			{
				if (T.bTorpedo) Emit(FxKind::Splash, Down, "torpedo").Size = 4.0;
				else
				{
					Emit(FxKind::BombHit, Down, Floor > 0.5 ? "land" : "water").Size = T.Size > 2.0 ? 500.0 : 250.0;
					Check.Bombs++;
				}
			}
			else if (T.Kind == ThingKind::Piece)
			{
				// (one that has lived out its time aloft leaves the picture there: no splash in the air)
				if (!bCloud && T.At.Z <= Floor) Emit(Floor > 0.5 ? FxKind::Crash : FxKind::Splash, Down, "piece").Size = T.Size;
				if (T.bTrail) Emit(FxKind::TrailStop, T.At.Z <= Floor ? Down : T.At, std::string(), T.Key);
			}
			Small.erase(Small.begin() + static_cast<long>(K));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------

	void Show::Advance(double T)
	{
		if (Views.empty()) return;
		T = std::min(T, Latest());
		const double DT = std::max(0.0, T - ShownT);
		ShownT = std::max(ShownT, T);
		const double AirDt = DT * Scale;
		AirT += AirDt;
		Interpolate();
		while (!Queue.empty() && Queue.front().T <= ShownT + 1e-9)
		{
			const Event E = Queue.front();
			Queue.pop_front();
			Play(E);
		}
		for (size_t K = 0; K < Later.size();)
		{
			if (Later[K].At <= AirT) { const Event E = Later[K].E; Later.erase(Later.begin() + static_cast<long>(K)); Release(E); }
			else ++K;
		}
		for (auto& Pair : Now) Reconcile(Pair.second, Bands[Pair.first]);
		// Groups the battle no longer shows: whatever is left of them goes home.
		for (auto& Pair : Crafts) if (!Now.count(Pair.second.Group)) Leave(Pair.second);

		if (AirDt > 0.0)
		{
			const int Steps = std::max(1, static_cast<int>(std::ceil(AirDt / 0.05)));
			const double Dt = AirDt / Steps;
			for (auto& Pair : Crafts) { Pair.second.LastP = Pair.second.P; Pair.second.bHasLast = Pair.second.Doing != Role::Gone; }
			for (int S = 0; S < Steps; ++S)
			{
				for (auto& Pair : Crafts) Pilot(Pair.second, Dt);
			}
			for (auto& Pair : Crafts)
			{
				Craft& C = Pair.second;
				if (!C.bHasLast || C.Doing == Role::Gone) continue;
				Check.MostJump = std::max(Check.MostJump, Len(C.P - C.LastP) - (C.VTop + 30.0) * AirDt);
				// (a hit's trail, or a fire put out: its kind changes when it was planned to, and it clears)
				while (!C.TrailPlan.empty() && AirT >= C.TrailPlan.front().first && C.Doing != Role::Fall)
				{
					const std::string Kind = C.TrailPlan.front().second;
					C.TrailPlan.erase(C.TrailPlan.begin());
					if (!C.TrailKey) continue;
					if (Kind.empty()) { StopTrails(C); break; }
					LayTrail(C, Kind);
				}
				if (C.TrailKey && C.Doing != Role::Fall) Emit(FxKind::TrailMove, Local(C, C.TrailAt), std::string(), C.TrailKey).Vel = Velocity(C);
				if (C.TrailKey2 && C.Doing != Role::Fall) Emit(FxKind::TrailMove, Local(C, C.TrailAt2), std::string(), C.TrailKey2).Vel = Velocity(C);
			}
			FireGuns(AirDt);
			FlakRound(AirDt);
			SmallThings(AirDt);
		}
		else
		{
			for (int Key : Firing) Emit(FxKind::GunStop, Vec(), std::string(), Key);
			Firing.clear();
		}
		for (auto It = Crafts.begin(); It != Crafts.end();)
		{
			if (It->second.Doing == Role::Gone) It = Crafts.erase(It);
			else ++It;
		}
		Out.clear();
		Out.reserve(Crafts.size());
		for (const auto& Pair : Crafts)
		{
			const Craft& C = Pair.second;
			PlaneOut P;
			P.Key = C.Key;
			P.Group = C.Group;
			P.Side = C.Side;
			P.bFighter = C.bFighter;
			P.Model = C.Model;
			P.Nation = C.Nation;
			P.At = C.P;
			P.Heading = std::fmod(C.Hdg / Deg + 360.0, 360.0);
			P.Pitch = C.Gam / Deg;
			P.Bank = C.Bank / Deg;
			P.Span = C.Form.Span;
			P.bShown = C.bShown;
			P.bOnGround = C.Doing == Role::Ground;
			P.bFalling = C.Doing == Role::Fall;
			P.bFiring = C.bFiring;
			P.Target = C.bFighter && C.Doing == Role::Fight ? C.Target : -1;
			P.Speed = C.Spd;
			P.DeadEngines = C.DeadEngines;
			P.bFeathered = C.bFeathered;
			P.Soot = C.SootAt >= 0.0 ? std::max(0.0, std::min(1.0, (AirT - C.SootAt) / 30.0)) : 0.0;
			Out.push_back(P);
		}
	}

	std::string Show::Describe() const
	{
		// One line an aircraft: what it is doing (for a debug overlay, and the cloud test's trace).
		static const char* Roles[] = { "ground", "slot", "fight", "straggle", "cover", "fall", "home", "dive", "gone" };
		static const char* Steps[] = { "-", "setup", "run", "break", "climb", "turn", "peel", "plunge", "pull", "egress" };
		std::string Text;
		char Line[256];
		for (const auto& Pair : Crafts)
		{
			const Craft& C = Pair.second;
			if (C.Doing == Role::Gone) continue;
			const Craft* T = C.Target >= 0 ? Find(C.Target) : nullptr;
			std::snprintf(Line, sizeof Line, "%d g%d %s %s/%s t%.0f q%d tgt%d d%.0f v%.0f z%.0f fire%d\n", C.Key, C.Group, C.bFighter ? "F" : "B",
			              Roles[static_cast<int>(C.Doing)], Steps[static_cast<int>(C.Step)], C.StepT, C.TargetGroup, C.Target,
			              T ? Len(T->P - C.P) : -1.0, C.Spd, C.P.Z, C.bFiring ? 1 : 0);
			Text += Line;
		}
		return Text;
	}
}
