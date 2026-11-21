#include "Battle/AirShow.h"

#include <cctype>
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
		ViewT = A->T;
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
		if (C.bFighter)
		{
			// (his group's skill as the battle gives it, a little his own; and how soon his guns run dry. Told off by
			// his number, not by the dice: the scene's other dice fall as they did before there were pilots to grade)
			const double Own = std::fmod(C.Key * 0.6180339887498949, 1.0), Spare = std::fmod(C.Key * 0.7548776662466927, 1.0);
			C.Skill = Clamp((G.Skill >= 0.0 ? G.Skill : 0.5) + (Own - 0.5) * 0.24, 0.05, 1.0);
			C.Thrift = 0.02 + 0.12 * Spare;
		}
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
				// (into a fight: the next of the group coming in from well behind and above it, at fighting speed)
				const bool bIntoFight = bAirborne && G.bFighter && G.Doing == Act::Fight && G.Engaged >= 0;
				if (bIntoFight)
				{
					At = Vec(G.X, G.Y, G.Alt) - F * 3200.0 + RightOf(G.Face) * (((K / 2) % 2 ? 900.0 : -900.0) + (K % 2 ? 70.0 : -70.0)) + Up * 600.0;
					At.Z = std::max(At.Z, GroundAt(At.X, At.Y) + 400.0);
				}
				Role Doing = Role::Slot;
				if (!bAirborne)
				{
					At = Vec(G.X, G.Y, 0.0) + RightOf(G.Face) * ((K - Want * 0.5) * 28.0);
					At.Z = GroundAt(At.X, At.Y);
					Doing = Role::Ground;
				}
				const Vec Vel = GroupVelocity(G.Id);
				Craft& C = Make(G, At, G.Face, bIntoFight ? 140.0 : bAirborne ? std::max(Len(Vel) * 1.05, G.bFighter ? 95.0 : 75.0) : 0.0, Doing);
				C.Slot = K;
				C.bJoining = bAirborne && !bIntoFight;
			}
		}
		else if (Have - B.Pending > Want)
		{
			// One model too many (a loss still held for the guns to bear is a model yet to fall, not one too many): a
			// fighter already out of the fight first, then the youngest joining, else the last in the formation, peels
			// off for home.
			std::vector<int> Order = Keys;
			std::stable_sort(Order.begin(), Order.end(), [this](int A, int Bk)
			{
				const Craft& Ca = *Find(A);
				const Craft& Cb = *Find(Bk);
				const int Ra = Ca.Step == Phase::Away ? -1 : Ca.bJoining ? 0 : Ca.Doing == Role::Slot ? 1 : 2;
				const int Rb = Cb.Step == Phase::Away ? -1 : Cb.bJoining ? 0 : Cb.Doing == Role::Slot ? 1 : 2;
				return Ra != Rb ? Ra < Rb : Ca.Slot > Cb.Slot;
			});
			for (int K = 0; K < Have - B.Pending - Want; ++K) Leave(*Find(Order[K]));
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

	bool Show::LayGun(const Craft& C, const Vec& Station, const Craft& Foe, double Near, const Gun& Row, int Mount, Vec& Aim)
	{
		// (each gunner's own phase, from his mount's key: the golden ratio spreads neighbouring keys apart)
		const double Ph = std::fmod(static_cast<double>(Mount) * 0.6180339887498949, 1.0) * 2.0 * Pi;
		// His lead: 55 to 105 per cent of the full one, slowly changing. Gunners mostly under-led, and their
		// streams passed behind the fighter.
		const double LeadK = 0.8 + 0.25 * std::sin(AirT * 0.37 + Ph * 3.0);
		const Vec Lead = Foe.P + Velocity(Foe) * (LeadK * Near / std::max(300.0, Row.Mv));
		// His wander about it, two slow swings across and up: about 0.6 degrees for a power turret, 1.8 for a
		// hand-held gun (D6: turret streams wander a little and hand-held waist guns more), so a burst bends like a
		// hose but stays one stream, and the box's streams make a web about the fighter, never one crossing point.
		const bool bHand = Row.Mount == "hand";
		const double Wander = (bHand ? 1.8 : 0.6) * Deg;
		const double Across = Wander * (std::sin(AirT * 2.3 + Ph) + 0.5 * std::sin(AirT * 5.1 + Ph * 2.0)) / 1.5;
		const double Over = Wander * (std::cos(AirT * 1.9 + Ph * 1.7) + 0.5 * std::sin(AirT * 4.3 + Ph * 2.6)) / 1.5;
		const Vec From = Local(C, Station);
		const Vec Sight = Lead - From;
		const double Range = std::max(1.0, Len(Sight));
		const Vec A = Norm(Sight.Cross(Up), RightOf(C.Hdg));
		const Vec B = Norm(A.Cross(Sight), Up);
		const Vec Want = Norm(Sight + (A * Across + B * Over) * Range, Ahead(C.Hdg, C.Gam));
		// His gun as he has it laid: it follows the point a little behind (about 0.2 s) and never swings faster
		// than about 40 degrees a second for a power turret or 50 for a hand-held gun. One newly in his arc he
		// has already been following.
		Lay& L = Lays[Mount];
		const double Dt = AirT - L.T;
		if (L.T < 0.0 || Dt > 0.5 || Dt < 0.0) L.Way = Want;
		else if (Dt > 0.0)
		{
			Vec Next = Norm(L.Way + (Want - L.Way) * (1.0 - std::exp(-Dt / 0.2)), Want);
			const double Step = std::acos(std::max(-1.0, std::min(1.0, Next.Dot(L.Way))));
			const double Most = (bHand ? 50.0 : 40.0) * Deg * Dt;
			if (Step > Most) Next = Norm(L.Way + (Next - L.Way) * (Most / Step), Want);
			L.Way = Next;
		}
		L.T = AirT;
		Aim = From + L.Way * Range;
		// (on it, within 15 degrees: he does not fire while he swings onto a new one)
		return L.Way.Dot(Want) > std::cos(15.0 * Deg);
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
		// (a liquid-cooled engine: white glycol first, thinning over 30 to 60 s as the coolant runs out, then the thin
		// dark streak that never clears; the kit from the effects thread's 17:34Z head hands over on its own, an
		// older kit is told)
		LayTrail(C, C.bLiquid ? "glycol" : "oil");
		if (C.bLiquid) { C.TrailPlan.push_back({ AirT + 45.0, "streak" }); return; }
		// (a radial feathered with no fire, its oil spent: it thins to the streak and keeps it)
		if (C.bFeathered) C.TrailPlan.push_back({ AirT + 20.0, "streak" });
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
		// (a leader flies with a little throttle in hand so that his wingman can hold him: the wingman has it)
		// (and one parted from him comes back flat out, the throttle through the gate)
		const double Push = Clamp((K.Spd - S) / 4.0, -4.0, C.Step != Phase::Wing ? 3.0 : C.bRejoin ? 5.6 : 4.2);
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
		C.Task = "";
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
			if (G && G->Doing == Act::Fight && G->Engaged >= 0 && PilotFight(C, *G, Dt))
			{
				if (!C.bStrafing && C.Doing == Role::Fight) Check.LowestFight = std::min(Check.LowestFight, C.P.Z - GroundAt(C.P.X, C.P.Y));
				return;
			}
			// (the fight over, or nobody of his to fight in it: back to his place)
			if (C.Doing != Role::Fight) return;
			C.Doing = Role::Slot;
			StandDown(C);
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
			if (C.bFighter) C.Task = "going home";
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
		if (C.bFighter && G->Doing == Act::Fight && G->Engaged >= 0 && C.Doing == Role::Slot)
		{
			// (one still closing up on the formation goes straight into the fight with the rest)
			C.bJoining = false;
			C.Doing = Role::Fight;
			C.Step = Phase::None;
			if (PilotFight(C, *G, Dt) || C.Doing != Role::Fight) return;
			C.Doing = Role::Slot;
			StandDown(C);
		}
		const Vec GV = GroupVelocity(C.Group);
		if (C.Doing == Role::Straggle)
		{
			const Strag* Mine = nullptr;
			for (const Strag& S : G->Strags) if (S.Id == C.StragId) Mine = &S;
			// (its dead engine catching fire, as the box's view tells it; a fire put out is the battle's own event)
			if (Mine && Mine->bFire && C.TrailKind != "fire" && !C.bFireOut) DeadEngineTrail(C, true);
			// (the battle tells of a bomber falling out a moment before its box's view lists it: it drops back meanwhile)
			// (and its loss told and held for the guns to bear on it, it limps on until they do)
			if (Mine || AirT - C.StragAt < 8.0 || DueStrag.count({ C.Group, C.StragId }))
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
		// How near the enemy the rules have it making for (its quarry; an escort's, the enemy coming for its bombers):
		// nothing at 7 km, all of it inside 3 km.
		double Near = 0.0;
		if (C.bFighter && G->Doing != Act::Fight)
		{
			if (G->With >= 0) C.Task = "escorting";
			auto QIt = G->Quarry >= 0 ? Now.find(G->Quarry) : Now.end();
			// Sent up after an enemy flying higher than the height it was sent to: on up toward his, from the moment it
			// has him, as fast as it climbs (a scramble from the fields below a raid). What height it lacks when the
			// fight opens it attacks from: up at him from below.
			// (his, or a box of the enemy's bombers nearer than 12 km and no more than a height band above it: the
			// battle has a group fight what is a band above it)
			double High = QIt != Now.end() ? QIt->second.Alt : 0.0;
			if (G->With < 0 && G->Quarry >= 0)
			{
				for (const auto& Pair : Now)
				{
					const Group& O = Pair.second;
					if (O.Side == G->Side || O.bFighter || O.Alt < G->Alt || O.Alt > G->Alt + 3000.0 || O.Doing == Act::Down) continue;
					if (std::hypot(O.X - G->X, O.Y - G->Y) < 12000.0) High = std::max(High, O.Alt);
				}
			}
			if (G->With < 0 && High > G->Alt + 200.0 && C.P.Z > GroundAt(C.P.X, C.P.Y) + 140.0)
			{
				C.Task = "closing";
				To.Z = std::min(To.Z + (High - G->Alt), C.P.Z + 300.0);
			}
			if (QIt != Now.end() && G->Alt > 600.0 && !C.bJoining)
			{
				Near = Clamp((7000.0 - std::hypot(QIt->second.X - G->X, QIt->second.Y - G->Y)) / 4000.0, 0.0, 1.0);
				// Making for him: as they close, the fours string out and step up, to come down on him from above and
				// one four after another.
				if (Near > 0.0 && G->With < 0)
				{
					C.Task = "closing";
					const int Four = C.Slot / 4;
					To += Up * (Near * ((QIt->second.bFighter ? 150.0 : 380.0) + Four * 70.0)) - Ahead(G->Face) * (Near * Four * 140.0);
				}
			}
		}
		if (C.bFighter && Len(GV) < 105.0)
		{
			// Wide, slow esses: the escort keeps its speed up (about 85 to 120 m/s) and its turns gentle (400 m and more).
			// With the enemy coming it swings wider and a little higher, to have fighting speed in hand when he arrives.
			const double W = AirT * 2.0 * Pi / 34.0 + C.Weave * 0.15;
			const double Wide = 380.0 + 260.0 * Near;
			To += RightOf(G->Face) * (Wide * std::sin(W)) + Up * (120.0 * Near);
			ToVel += RightOf(G->Face) * (Wide * 2.0 * Pi / 34.0 * std::cos(W));
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
		if (Id < 0 || Now.find(Id) == Now.end()) return false;
		const auto Left = Alive.find(Id);
		if (Left == Alive.end() || Left->second <= 0) return false;
		if (Id == G.Engaged) return true;
		for (const Fire& F : G.Firing) if (F.On == Id) return true;
		return false;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The fighter pilots (the air battles thread, 20 November; Brian on the Bremen film: "lets work on the ai for the
	// fighter pilots. they were kinda just flying around doing nothing"; "lets make this super realistic and
	// meaningful and everyone has high AI iq").
	//
	// The battle counts and the pilots act the count out: nothing here brings an aircraft down or saves one. What is
	// decided here is what each pilot does with the fight the battle gives his group, and it is decided as a pilot of
	// the war was taught to decide it:
	//
	// - Nobody fights alone. A group flies in pairs (its fours are two pairs; a vic is a leader and two wingmen). The
	//   leader attacks; his wingman holds his wing, fires with him and watches his tail. When an enemy gets on one's
	//   tail the other goes after that enemy. A wingman who loses his leader leads; two left alone join up.
	// - The group's pairs are shared out among the enemy groups it fires on, by how hard it fires on each (extra.fire),
	//   a four kept together where it can be, and they stay on that enemy.
	// - On a bomber box the fours go in one after another on the battle's own beat (the fire's go and wait): from
	//   where they stand when the fight opens, then from the perch the named pass is flown from (high astern, ahead for
	//   a head-on pass, the beam, below), reached round the box's side and never through it. Each pilot picks a bomber
	//   at the edge he comes at (a cripple out of its box first), fires in bursts as close as his skill takes him,
	//   breaks under the box three times in four, and climbs back for the next pass.
	// - Against fighters a diving type comes down from above, fires, and climbs away on its speed, never turning with
	//   a turning type; a turning type goes for the tail and pulls up and over rather than slide outside the turn. A
	//   pilot who sees an enemy on his tail breaks hard into him, or (a diving type with the height) half-rolls and
	//   dives away; when the enemy slides past in front, he reverses on to him.
	// - An escort keeps some of its pairs by its box whatever happens. Those go after an attacker coming in on the
	//   bombers, chase him off, and come back; the rest fight the enemy's fighters near the box.
	// - A fighter hit hard leaves the fight with his wingman beside him; a pair with empty guns stands off.
	// - How sharply he sees, how close he presses and how tight he holds a wing follow his group's skill (the rules'
	//   extra.skill), a little different for each pilot.

	void Show::StandDown(Craft& C)
	{
		C.Step = Phase::None;
		C.bFiring = false;
		C.Burst = 0.0;
		C.Target = -1;
		C.TargetGroup = -1;
		C.Threat = -1;
		C.Foe = -1;
		C.bGuard = C.bCover = C.bReversed = C.bFleeing = C.bOpened = C.bAnywhere = C.bStrafing = C.bBlind = C.bLow = C.bAnswer = false;
		C.PassAt = -1.0;
		C.YoYo = 0.0;
	}

	void Show::Brief()
	{
		Hunters.clear();
		Chasers.clear();
		Aloft.clear();
		Alive.clear();
		Due.clear();
		DueStrag.clear();
		for (const Held& Owed : Later)
		{
			if (!Owed.bForShot) continue;
			Due[{ Owed.E.Unit, Owed.E.By }]++;
			if (Owed.E.Strag >= 0) DueStrag.insert({ Owed.E.Unit, Owed.E.Strag });
		}
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			if (!Live(C)) continue;
			Alive[C.Group]++;
			if (C.Doing != Role::Ground) Aloft[C.Group]++;
			if (!C.bFighter || C.Doing != Role::Fight || C.Step == Phase::Away) continue;
			if (C.TargetGroup >= 0) Hunters[C.TargetGroup].push_back(C.Key);
			if (C.Target >= 0) Chasers[C.Target].push_back(C.Key);
		}
		for (auto& Pair : Now)
		{
			const Group& G = Pair.second;
			if (!G.bFighter) continue;
			Band& B = Bands[G.Id];
			PairUp(G);
			if (!(G.Doing == Act::Fight && G.Engaged >= 0))
			{
				B.FightAt = -1.0;
				B.Waves.clear();
				continue;
			}
			if (B.FightAt < 0.0)
			{
				B.FightAt = AirT;
				B.ShareAt = -1e9;
				Check.Fights++;
			}
			// The beat of its passes on each box, as the battle has it now (a view is up to a battle second old).
			const double Stale = std::max(0.0, ShownT - ViewT) * Scale;
			for (const Fire& F : G.Firing)
			{
				if (F.On < 0) continue;
				Wave& W = B.Waves[F.On];
				const bool bWas = W.bKnown && W.bGo;
				W.bKnown = F.Go >= 0.0 || F.Wait >= 0.0;
				if (!W.bKnown) continue;
				W.bGo = F.Go >= 0.0;
				W.Left = std::max(0.0, (W.bGo ? F.Go : F.Wait) - Stale);
				if (W.bGo && !bWas) Check.Waves++;
				if (!W.bGo && bWas) W.Ended = AirT;
			}
			SetGuards(G);
			ShareOut(G, B);
			SetSpent(G);
		}
	}

	void Show::PairUp(const Group& G)
	{
		// (in the order they stand; one still on the ground pairs up when it is off it)
		std::vector<Craft*> Mine;
		for (int Key : LiveOf(G.Id))
		{
			Craft* C = Find(Key);
			if (C->Doing != Role::Ground) Mine.push_back(C);
		}
		if (Mine.empty()) return;
		const bool bThrees = !(G.Form == "fours" || G.Form == "line");
		bool bAny = false;
		for (const Craft* C : Mine) bAny = bAny || C->Lead != 0;
		if (!bAny)
		{
			// As the formation stands: fours of two pairs, or vics of a leader and two wingmen.
			const size_t Unit = bThrees ? 3 : 4;
			for (size_t K = 0; K < Mine.size(); ++K)
			{
				Craft& C = *Mine[K];
				C.Flight = static_cast<int>(K / Unit);
				C.Seat = static_cast<int>(K % Unit);
				const size_t Leader = bThrees ? K - K % Unit : K - (K % Unit) % 2;
				C.Lead = Mine[Leader]->Key;
			}
		}
		else
		{
			// A wingman whose leader is gone is on his own until he joins another.
			for (Craft* C : Mine)
			{
				if (C->Lead == 0 || C->Lead == C->Key) continue;
				const Craft* L = Find(C->Lead);
				if (!L || !Live(*L) || L->Group != G.Id || L->Lead != L->Key || L->Doing == Role::Ground) C->Lead = 0;
			}
			std::set<int> Followed;
			for (const Craft* C : Mine) if (C->Lead != 0 && C->Lead != C->Key) Followed.insert(C->Lead);
			// Those alone join up two and two: a leader goes on leading; of two wingmen the one ahead in the formation.
			std::vector<Craft*> Alone;
			for (Craft* C : Mine) if (C->Lead == 0 || (C->Lead == C->Key && !Followed.count(C->Key))) Alone.push_back(C);
			std::stable_sort(Alone.begin(), Alone.end(), [](const Craft* A, const Craft* Bc) { return (A->Lead != 0) > (Bc->Lead != 0); });
			int NextFlight = 0;
			for (const Craft* C : Mine) NextFlight = std::max(NextFlight, C->Flight + 1);
			auto Leads = [&](Craft& L)
			{
				if (L.Lead == L.Key) return;
				// (a wingman takes his leader's seat; one new come leads the second pair of a four that has only one, else
				// a four of his own)
				if (L.Seat >= 0) L.Seat = bThrees ? 0 : L.Seat - L.Seat % 2;
				else
				{
					int Short = -1;
					for (const Craft* O : Mine)
					{
						if (bThrees || Short >= 0 || O->Lead != O->Key || O->Seat != 0) continue;
						bool bSecond = false;
						for (const Craft* P2 : Mine) bSecond = bSecond || (P2->Flight == O->Flight && P2->Seat >= 2);
						if (!bSecond) Short = O->Flight;
					}
					L.Flight = Short >= 0 ? Short : NextFlight++;
					L.Seat = Short >= 0 ? 2 : 0;
				}
				L.Lead = L.Key;
			};
			// (with the nearest other alone inside 1.5 km; one new come joins whoever is alone, from wherever he is)
			std::vector<bool> Paired(Alone.size(), false);
			for (size_t K = 0; K < Alone.size(); ++K)
			{
				if (Paired[K]) continue;
				Paired[K] = true;
				Craft& L = *Alone[K];
				const bool bNewL = L.Lead == 0 && L.Seat < 0;
				size_t Pick = Alone.size();
				double Near = 1e18;
				for (size_t J = K + 1; J < Alone.size(); ++J)
				{
					if (Paired[J]) continue;
					const double D = Len(Alone[J]->P - L.P);
					const bool bNew = bNewL || (Alone[J]->Lead == 0 && Alone[J]->Seat < 0);
					if ((D < 1500.0 || bNew) && D < Near) { Near = D; Pick = J; }
				}
				Leads(L);
				if (Pick == Alone.size()) continue;
				Paired[Pick] = true;
				Craft& W = *Alone[Pick];
				W.Lead = L.Key;
				W.Flight = L.Flight;
				W.Seat = L.Seat + 1;
			}
		}
		for (Craft* C : Mine) { C->Wing = 0; C->Boss = 0; }
		for (Craft* C : Mine)
		{
			if (C->Lead == C->Key)
			{
				// (the second pair of a four follows the four's leader in)
				if (C->Seat >= 2) for (const Craft* O : Mine) if (O->Flight == C->Flight && O->Seat == 0 && O->Lead == O->Key) C->Boss = O->Key;
				continue;
			}
			Craft* L = Find(C->Lead);
			if (L && !L->Wing) L->Wing = C->Key;
		}
	}

	void Show::SetGuards(const Group& G)
	{
		std::vector<Craft*> Leaders;
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			if (C.Group == G.Id && C.bFighter && Live(C) && C.Doing != Role::Ground && C.Lead == C.Key) Leaders.push_back(&C);
		}
		// An escort keeps pairs by its box: half of a close escort's, a third of a top cover's, one at the least. A
		// sweep ranges ahead and keeps none.
		const auto Box = G.With >= 0 ? Aloft.find(G.With) : Aloft.end();
		const bool bKeeps = Box != Aloft.end() && Box->second > 0 && Now.count(G.With) && G.Role != "sweep";
		const int Pairs = static_cast<int>(Leaders.size());
		const int Want = !bKeeps ? 0 : G.Role == "top" ? std::max(1, Pairs / 3) : std::max(1, (Pairs + 1) / 2);
		// (those already kept stay kept; then the fours at the back of the formation)
		std::stable_sort(Leaders.begin(), Leaders.end(), [](const Craft* A, const Craft* Bc) { return A->bGuard != Bc->bGuard ? A->bGuard : A->Flight > Bc->Flight; });
		int Kept = 0;
		for (Craft* L : Leaders)
		{
			const bool bKeep = Kept < Want && !L->bHurt;
			if (bKeep) Kept++;
			if (bKeep == L->bGuard) continue;
			L->bGuard = bKeep;
			L->Target = -1;
			L->TargetGroup = -1;
			L->bCover = L->bReversed = false;
			if (!Dodging(*L) && L->Step != Phase::Away) { L->Step = Phase::None; L->StepT = 0.0; }
		}
	}

	void Show::ShareOut(const Group& G, Band& B)
	{
		struct Mark
		{
			int Id = -1;
			double Rate = 0.0;
			int Want = 0, Have = 0;
		};
		std::vector<Mark> Marks;
		double Sum = 0.0;
		for (const Fire& F : G.Firing)
		{
			if (F.Rate <= 0.0 || !Fights(G, F.On)) continue;
			Mark M;
			M.Id = F.On;
			M.Rate = F.Rate;
			Marks.push_back(M);
			Sum += F.Rate;
		}
		if (Marks.empty() && Fights(G, G.Engaged))
		{
			Mark M;
			M.Id = G.Engaged;
			M.Rate = 1.0;
			Marks.push_back(M);
			Sum = 1.0;
		}
		std::vector<Craft*> Leaders;
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			if (C.Group == G.Id && C.bFighter && Live(C) && C.Doing != Role::Ground && C.Lead == C.Key && !C.bGuard) Leaders.push_back(&C);
		}
		if (Marks.empty() || Leaders.empty()) return;
		auto IndexOf = [&Marks](int Id)
		{
			for (size_t K = 0; K < Marks.size(); ++K) if (Marks[K].Id == Id) return static_cast<int>(K);
			return -1;
		};
		bool bAdrift = false;
		for (const Craft* L : Leaders) bAdrift = bAdrift || IndexOf(L->TargetGroup) < 0;
		// (shared out again only now and then, or when a pair has no enemy: a pair stays on its enemy)
		if (!bAdrift && AirT < B.ShareAt) return;
		B.ShareAt = AirT + 6.0;
		const int N = static_cast<int>(Leaders.size()), M = static_cast<int>(Marks.size());
		// Each enemy's share of the pairs by how hard the group fires on it, to the nearest pair; a pair on every enemy
		// it fires on while there are pairs enough.
		int Given = 0;
		std::vector<double> Over(Marks.size(), 0.0);
		for (int K = 0; K < M; ++K)
		{
			const double Exact = Marks[static_cast<size_t>(K)].Rate / Sum * N;
			Marks[static_cast<size_t>(K)].Want = static_cast<int>(std::floor(Exact));
			Over[static_cast<size_t>(K)] = Exact - std::floor(Exact);
			if (Marks[static_cast<size_t>(K)].Want == 0 && N >= M) { Marks[static_cast<size_t>(K)].Want = 1; Over[static_cast<size_t>(K)] = 0.0; }
			Given += Marks[static_cast<size_t>(K)].Want;
		}
		while (Given > N)
		{
			size_t Most = 0;
			for (size_t K = 1; K < Marks.size(); ++K) if (Marks[K].Want > Marks[Most].Want) Most = K;
			Marks[Most].Want--;
			Given--;
		}
		while (Given < N)
		{
			size_t Best = 0;
			for (size_t K = 1; K < Marks.size(); ++K) if (Over[K] > Over[Best]) Best = K;
			Marks[Best].Want++;
			Over[Best] -= 1.0;
			Given++;
		}
		std::vector<Craft*> Free;
		for (Craft* L : Leaders)
		{
			const int K = IndexOf(L->TargetGroup);
			if (K < 0) Free.push_back(L);
			else Marks[static_cast<size_t>(K)].Have++;
		}
		for (Mark& M_ : Marks)
		{
			auto EIt = Now.find(M_.Id);
			const Vec At = EIt == Now.end() ? Vec() : Vec(EIt->second.X, EIt->second.Y, EIt->second.Alt);
			while (M_.Have > M_.Want)
			{
				// (one not in the middle of a pass or a chase, the farthest from this enemy; one going in from far off,
				// more than a mile and a half out, can still be called off it)
				Craft* Pick = nullptr;
				double Far = -1.0;
				for (Craft* L : Leaders)
				{
					if (L->TargetGroup != M_.Id) continue;
					if (L->Step == Phase::Break || Dodging(*L) || L->Step == Phase::Chase || L->Step == Phase::Away || L->Burst > 0.0) continue;
					const double D = Len(L->P - At);
					if (L->Step == Phase::Run && D < 2400.0) continue;
					if (D > Far) { Far = D; Pick = L; }
				}
				if (!Pick) break;
				Pick->TargetGroup = -1;
				Free.push_back(Pick);
				M_.Have--;
			}
		}
		for (Craft* L : Free)
		{
			Mark* Best = nullptr;
			double BestCost = 1e18;
			for (Mark& M_ : Marks)
			{
				auto EIt = Now.find(M_.Id);
				if (EIt == Now.end()) continue;
				double Cost = Len(L->P - Vec(EIt->second.X, EIt->second.Y, EIt->second.Alt));
				// (the enemy short of its share first; a four stays together)
				if (M_.Have >= M_.Want) Cost += 1e6;
				for (const Craft* O : Leaders) if (O != L && O->Flight == L->Flight && O->TargetGroup == M_.Id) Cost -= 1500.0;
				if (Cost < BestCost) { BestCost = Cost; Best = &M_; }
			}
			if (!Best) continue;
			Best->Have++;
			L->TargetGroup = Best->Id;
			L->Target = -1;
			L->bFiring = false;
			if (!Dodging(*L) && L->Step != Phase::Away && !L->bCover) { L->Step = Phase::None; L->StepT = 0.0; }
		}
	}

	void Show::SetSpent(const Group& G)
	{
		// The pairs that have fired longest run dry first, as the group's ammunition goes: up to half of them stand off
		// with empty guns before the battle calls the whole group spent.
		std::vector<Craft*> Leaders;
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			if (C.Group == G.Id && C.bFighter && Live(C) && C.Doing != Role::Ground && C.Lead == C.Key) Leaders.push_back(&C);
		}
		std::stable_sort(Leaders.begin(), Leaders.end(), [](const Craft* A, const Craft* Bc) { return A->bDry != Bc->bDry ? A->bDry : A->Thrift > Bc->Thrift; });
		const int Most = static_cast<int>(Leaders.size()) / 2;
		int Dry = 0;
		for (Craft* L : Leaders)
		{
			const bool bNow = L->bDry || (Dry < Most && G.Ammo < L->Thrift);
			if (bNow) Dry++;
			if (bNow && !L->bDry) Check.Dry++;
			L->bDry = bNow;
		}
	}

	void Show::FightSpeed(const Craft& C)
	{
		Check.LeastFightSpeed = std::min(Check.LeastFightSpeed, C.Spd);
		Check.MostFightSpeed = std::max(Check.MostFightSpeed, C.Spd);
	}

	void Show::MindGround(const Craft& C, Cmd& K, double Least) const
	{
		const double Over = C.P.Z - GroundAt(C.P.X, C.P.Y);
		// (the height a dive at this angle still takes to come out of)
		const double Sink = std::max(0.0, -std::sin(C.Gam)) * C.Spd;
		if (Over >= Least + Sink * 2.5) return;
		K.Gam = std::max(K.Gam, Over < Least ? 8.0 * Deg : 0.0);
		if (K.bBank && std::fabs(K.Bank) > 90.0 * Deg) K.bBank = false;
	}

	void Show::Ease(const Craft& C, Cmd& K, double Most) const
	{
		// A turn no steeper than this bank: a hard turn costs the speed a climb needs.
		const double S = std::max(C.Spd, 30.0);
		const double Omega = Clamp(Wrap(K.Hdg - C.Hdg) * 0.8, -1.2, 1.2);
		K.bBank = true;
		K.Bank = Clamp(std::atan(S * Omega / Gee), -Most, Most);
	}

	double Show::EtaTo(const Craft& C, const Vec& To, const Vec& ToVel, double Range) const
	{
		// Air seconds until he could be firing on something from here: the turn on to it, then the closing.
		const Vec Rel = To - C.P;
		const Vec Way = Norm(Rel);
		const double Closing = std::max(40.0, C.VMax - ToVel.Dot(Way));
		const double Turn = std::acos(Clamp(Ahead(C.Hdg, C.Gam).Dot(Way), -1.0, 1.0));
		return std::max(0.0, Len(Rel) - Range) / Closing + Turn / 0.42;
	}

	std::string Show::PassOn(const Group& G, int Box)
	{
		// The pass the contact named, or what the group is firing with now.
		std::string Pass = "astern";
		const Band& B = Bands[G.Id];
		auto PIt = B.Pass.find(Box);
		if (PIt != B.Pass.end() && !PIt->second.empty()) Pass = PIt->second;
		for (const Fire& F : G.Firing) if (F.On == Box && !F.Pass.empty()) Pass = F.Pass;
		return Pass;
	}

	Vec Show::PerchOf(const Craft& C, const std::string& Pass, const Vec& Box, const Vec& Fb, const Vec& Rb) const
	{
		// Where a pass is flown from, about the box: out of its gunners' reach, with the height or the place the pass
		// wants. High astern; well ahead for a head-on pass; the front quarter, high; the beam; below and behind.
		const double H = C.Hand;
		const bool bAhead = Pass == "headon" || Pass == "quarter";
		Vec P;
		if (Pass == "headon") P = Box + Fb * 2300.0 + Rb * (H * 260.0) + Up * 120.0;
		else if (Pass == "quarter") P = Box + Fb * 1500.0 + Rb * (H * 1300.0) + Up * 400.0;
		else if (Pass == "beam") P = Box + Fb * 250.0 + Rb * (H * 1700.0) + Up * 220.0;
		else if (Pass == "below") P = Box - Fb * 950.0 + Rb * (H * 150.0) - Up * 280.0;
		else if (Pass == "close") P = Box - Fb * 1200.0 + Rb * (H * 120.0) + Up * 60.0;
		else P = Box - Fb * 1250.0 + Rb * (H * 220.0) + Up * 380.0;
		// (each four beside the last, the second pair behind its leader as the pass goes)
		const int Row = C.Flight % 3;
		P += Rb * (H * Row * 190.0) + Up * (Row * 60.0);
		if (C.Seat >= 2) P += Rb * (H * 150.0) + Fb * (bAhead ? 170.0 : -170.0) + Up * 30.0;
		P.Z = std::max(P.Z, GroundAt(P.X, P.Y) + 200.0);
		return P;
	}

	bool Show::MayGo(const Craft& C, const Wave& W, const Craft* FourLead, double Eta) const
	{
		if (FourLead)
		{
			// The second pair of a four follows its leader in, a breath behind him; left waiting, it goes by itself.
			if (FourLead->Step == Phase::Run) return FourLead->StepT >= 1.2;
			if (FourLead->Step == Phase::Break) return true;
			return C.StepT > 20.0;
		}
		// One four at a time, a few seconds apart.
		if (AirT - W.Last < 4.5) return false;
		if (!W.bKnown) return true;
		// On the beat: with enough of the pass left to reach the box in it; or, standing off, so as to reach it as the
		// next pass opens.
		return W.bGo ? W.Left + 1.0 >= Eta : W.Left <= Eta;
	}

	int Show::MarkBomber(const Craft& C, const Group& E, const std::string& Pass)
	{
		const Vec Box(E.X, E.Y, E.Alt), Fb = Ahead(E.Face), Rb = RightOf(E.Face);
		// (what his own side's leaders are already going in on)
		std::map<int, int> On;
		auto H = Hunters.find(E.Id);
		if (H != Hunters.end())
		{
			for (int Key : H->second)
			{
				const Craft* F = Find(Key);
				if (F && F->Side == C.Side && F->Key != C.Key && F->Step == Phase::Run && F->Target >= 0 && F->Lead == F->Key) On[F->Target]++;
			}
		}
		const bool bFront = Pass == "headon" || Pass == "quarter";
		const bool bSide = Pass == "beam";
		const auto Up_ = Aloft.find(E.Id);
		const bool bAloft = Up_ != Aloft.end() && Up_->second > 0;
		int Best = -1;
		double BestScore = 1e18;
		for (const auto& Pair : Crafts)
		{
			const Craft& T = Pair.second;
			if (T.Group != E.Id || !Live(T) || (bAloft && T.Doing == Role::Ground)) continue;
			// The edge of the box he comes at: its leaders from ahead, its last ships from behind, the near side from
			// the beam; the nearer to him the better.
			const Vec Rel = T.P - Box;
			double Score = bSide ? -Rel.Dot(Rb) * C.Hand : bFront ? -Rel.Dot(Fb) : Rel.Dot(Fb);
			if (Pass == "below") Score += 2.0 * Rel.Z;
			Score += 0.15 * Len(T.P - C.P);
			auto It = On.find(T.Key);
			if (It != On.end()) Score += (T.Doing == Role::Straggle ? 2500.0 : 450.0) * It->second;
			// (a cripple out of its box's guns, as the fighters went for them: first of all one the battle has counted
			// lost and the scene has yet to show; but with a loss counted in the box itself, the box)
			if (T.Doing == Role::Straggle) Score += DueStrag.count({ E.Id, T.StragId }) ? -2500.0 : DueOn(E.Id, C.Group) > 0 ? 6000.0 : -500.0;
			Score += Roll.U(0.0, 200.0);
			if (Score < BestScore) { BestScore = Score; Best = T.Key; }
		}
		return Best;
	}

	void Show::StartRun(Craft& C, const Group& G, const Group& E, const std::string& Pass)
	{
		C.Step = Phase::Run;
		C.StepT = 0.0;
		C.Pass = Pass;
		C.bOpened = true;
		C.bAnywhere = false;
		Bands[G.Id].Waves[E.Id].Last = AirT;
		if (C.PassAt >= 0.0) Check.LongestWait = std::max(Check.LongestWait, AirT - C.PassAt);
		C.PassAt = AirT;
		Check.Passes++;
		C.Target = MarkBomber(C, E, Pass);
	}

	void Show::Squeeze(Craft& C, Craft& T, const Group& G, double Range, double Most, double Lo, double Hi, double Dt)
	{
		// A burst only with the nose on the lead and inside the range, a second or two long, as the group fires on that
		// enemy at all.
		bool bMay = false;
		for (const Fire& F : G.Firing) if (F.On == T.Group && F.Rate > 0.0) bMay = true;
		const Vec Rel = T.P - C.P;
		const double Dist = Len(Rel);
		const Vec Aim = T.P + Velocity(T) * (Dist / 800.0);
		const double Off = std::acos(Clamp(Ahead(C.Hdg, C.Gam).Dot(Norm(Aim - C.P)), -1.0, 1.0));
		if (bMay && C.Burst <= 0.0 && C.Rest <= 0.0 && Dist < Range && Off < Most && T.Doing != Role::Fall)
		{
			C.Burst = Roll.U(Lo, Hi);
			C.Rest = C.Burst + Roll.U(0.8, 1.8);
		}
		C.bFiring = C.Burst > 0.0 && bMay && Dist < Range * 1.2 && Dist > 60.0 && T.Doing != Role::Fall && T.Doing != Role::Gone;
		if (!C.bFiring) return;
		// (he holds his fire while an aircraft already going down is passing close by the one he is after: no rounds
		// are drawn striking a falling aircraft)
		const double Clear = 0.45 * std::hypot(T.Form.Long, T.Form.Span) + 8.0;
		for (const auto& Pair : Crafts)
		{
			const Craft& Q = Pair.second;
			if (Q.Doing != Role::Fall || Len(Q.P - T.P) > Clear + 0.6 * Q.Form.Span) continue;
			C.bFiring = false;
			return;
		}
		T.ShotAt = AirT;
		T.ShotBy = C.Group;
		if (!T.bFighter)
		{
			const Band& B = Bands[C.Group];
			auto WIt = B.Waves.find(T.Group);
			if (WIt != B.Waves.end() && WIt->second.bKnown)
			{
				(WIt->second.bGo || AirT - WIt->second.Ended < 3.0 ? Check.BeatIn : Check.BeatOut) += Dt;
			}
		}
	}

	bool Show::PilotFight(Craft& C, const Group& G, double Dt)
	{
		C.bFiring = false;
		C.bStrafing = false;
		if (C.Burst > 0.0) C.Burst -= Dt;
		if (C.Rest > 0.0) C.Rest -= Dt;
		if (C.Burst <= 0.0) C.bScored = false;
		LookBehind(C);
		// His pair: the leader he flies on, or the wingman who flies on him.
		Craft* L = C.Lead != C.Key && C.Lead != 0 ? Find(C.Lead) : nullptr;
		if (L && (!Live(*L) || L->Doing != Role::Fight)) L = nullptr;
		Craft* W = !L && C.Wing ? Find(C.Wing) : nullptr;
		if (W && (!Live(*W) || W->Doing != Role::Fight)) W = nullptr;
		// A scissors he is in, and an enemy on his own tail, come before anything.
		if (Scissoring(C, G, Dt)) return true;
		if (Evading(C, G, Dt)) return true;
		// Out of the fight: hit hard, or his pair's guns empty (an escort kept by its box stays by it all the same).
		{
			Craft* Follow = nullptr;
			bool bOut = C.bHurt;
			if (!bOut)
			{
				if (L && (L->bHurt || (L->bDry && !L->bGuard))) { bOut = true; Follow = L; }
				else if (W && W->bHurt) { bOut = true; Follow = W; }
				else if (!L && C.bDry && !C.bGuard) bOut = true;
			}
			if (bOut)
			{
				FlyAway(C, G, Follow, Dt);
				return true;
			}
			if (C.Step == Phase::Away) { C.Step = Phase::None; C.StepT = 0.0; }
		}
		// Then the enemy on his partner's tail.
		if (Covering(C, G, L ? L : W, Dt)) return true;
		if (L)
		{
			FlyWing(C, *L, G, Dt);
			return true;
		}
		if (Answering(C, G, Dt)) return true;
		if (C.bGuard && FlyGuard(C, G, Dt)) return true;
		auto EIt = Now.find(C.TargetGroup);
		if (EIt == Now.end() || !Fights(G, C.TargetGroup)) return false;
		const Group& E = EIt->second;
		// A box of bombers, or aircraft caught on the ground: passes. Fighters in the air: a fight.
		const auto Up_ = Aloft.find(E.Id);
		if (!E.bFighter || Up_ == Aloft.end() || Up_->second <= 0)
		{
			FlyPass(C, G, E, Dt);
			return true;
		}
		return FlyDuel(C, G, E, Dt);
	}

	void Show::LookBehind(Craft& C)
	{
		const Vec Nose = Ahead(C.Hdg, C.Gam);
		if (AirT < C.Scan)
		{
			if (C.Threat >= 0)
			{
				const Craft* F = Find(C.Threat);
				if (!F || !Live(*F) || F->Doing != Role::Fight) { C.Threat = -1; C.bBlind = false; }
			}
			return;
		}
		// He looks behind him three times a second. An enemy fighter inside 750 m, behind his wing line and coming for
		// him, is on his tail.
		C.Scan = AirT + 0.3;
		int Best = -1;
		double Near = 750.0;
		auto See = [&](const std::vector<int>& Keys)
		{
			for (int Key : Keys)
			{
				const Craft* F = Find(Key);
				if (!F || F->Side == C.Side || !F->bFighter || !Live(*F) || F->Doing != Role::Fight || F->Step == Phase::Away || Dodging(*F)) continue;
				// (one that is after him, its nose within 35 degrees of him; or any enemy on a pass or a chase with him
				// square in its sights, inside 450 m)
				const bool bAfterHim = F->Target == C.Key;
				if (!bAfterHim && !(F->Step == Phase::Run || F->Step == Phase::Turn || F->Step == Phase::Chase)) continue;
				const Vec Rel = C.P - F->P;
				const double D = Len(Rel);
				if (D >= (bAfterHim ? Near : std::min(Near, 450.0)) || D < 1.0) continue;
				if (Nose.Dot(Rel) < 0.17 * D) continue;
				if (Ahead(F->Hdg, F->Gam).Dot(Rel) < (bAfterHim ? 0.82 : 0.978) * D) continue;
				Near = D;
				Best = Key;
			}
		};
		auto H = Hunters.find(C.Group);
		if (H != Hunters.end()) See(H->second);
		auto Ch = Chasers.find(C.Key);
		if (Ch != Chasers.end()) See(Ch->second);
		if (Best < 0 && C.Threat >= 0)
		{
			// (one already seen is not lost the moment its nose swings a little wide)
			const Craft* F = Find(C.Threat);
			if (F && Live(*F) && F->Doing == Role::Fight && F->Step != Phase::Away && !Dodging(*F))
			{
				const Vec Rel = C.P - F->P;
				const double D = Len(Rel);
				if (D < 950.0 && Nose.Dot(Rel) > -0.2 * D && Ahead(F->Hdg, F->Gam).Dot(Rel) > 0.5 * D) Best = C.Threat;
			}
		}
		if (Best < 0)
		{
			C.Threat = -1;
			C.bBlind = false;
			return;
		}
		// Whether he sees it. One he has seen he keeps in sight.
		if (C.Threat >= 0 && !C.bBlind)
		{
			C.Threat = Best;
			return;
		}
		const Craft* F = Find(Best);
		// A loss the battle has already counted for his group at that enemy's guns, which the scene has yet to show:
		// he is the one who never saw it coming (as most who were shot down never did). The battle decided the loss;
		// the scene only chooses whose it is.
		const int Owed = DueOn(C.Group, F->Group);
		int Blind = 0;
		if (Owed) for (const auto& Pair : Crafts) if (Pair.second.Group == C.Group && Pair.second.Key != C.Key && Pair.second.bBlind && Live(Pair.second)) Blind++;
		if (Owed > Blind)
		{
			C.bBlind = true;
			C.Threat = Best;
			C.ThreatSeen = 1e18;
			return;
		}
		// Otherwise he may miss it for a look or two (his partner beside him doubles his eyes), and sees it for sure
		// when its tracer goes past him; then it takes him a moment to act, less the better he is.
		const bool bWasBlind = C.bBlind;
		C.bBlind = false;
		const bool bShotAt = F->bFiring && F->Target == C.Key;
		const Craft* Mate = C.Lead != 0 && C.Lead != C.Key ? Find(C.Lead) : C.Wing ? Find(C.Wing) : nullptr;
		if (Mate && (!Live(*Mate) || Mate->Doing != Role::Fight || Len(Mate->P - C.P) > 500.0)) Mate = nullptr;
		if (bWasBlind || bShotAt || Roll.U() < (0.05 + 0.12 * C.Skill) * (Mate ? 2.0 : 1.0))
		{
			C.Threat = Best;
			C.ThreatSeen = AirT + (bShotAt || bWasBlind ? 0.3 : 0.2 + (1.0 - C.Skill) * Roll.U(0.4, 1.0));
			Check.Threats++;
		}
		else C.Threat = -1;
	}

	bool Show::Scissoring(Craft& C, const Group& G, double Dt)
	{
		// The scissors: an attacker who came in too fast on a pilot breaking into him slides out past his tail, and each
		// then turns back into the other, crossing and crossing again, each trying to make the other fly out in front.
		// The one who flies the slower, wider weave comes out behind. Shown, never counted: nobody falls here unless
		// the battle has counted it.
		if (C.Step != Phase::Scissor) return false;
		Craft* O = C.Foe >= 0 ? Find(C.Foe) : nullptr;
		const bool bWith = O && Live(*O) && O->Doing == Role::Fight && O->Step == Phase::Scissor && O->Foe == C.Key;
		const Vec Rel = bWith ? O->P - C.P : Vec();
		const double D = Len(Rel);
		// Won, after a crossing or two: behind him, he before the nose and going away. On to his tail.
		if (bWith && C.bScissWin && C.ScissLegs >= 2 && D < 520.0 && Ahead(C.Hdg, C.Gam).Dot(Rel) > 0.8 * D && Ahead(O->Hdg, O->Gam).Dot(Rel) > 0.3 * D)
		{
			Check.ScissorWins++;
			Check.LongestScissors = std::max(Check.LongestScissors, C.StepT);
			C.Step = Phase::Chase;
			C.StepT = 0.0;
			C.bReversed = true;
			C.bCover = C.bAnswer = false;
			C.Target = O->Key;
			C.Threat = -1;
			C.Foe = -1;
			C.ScissNext = AirT + 25.0;
			return false;
		}
		// Over: the other out of it; another enemy on his own tail; hit; too low; drawn too far apart; or at its time.
		// (his enemy's wingman, holding his leader's wing through it, is not after him)
		const Craft* Third = C.Threat >= 0 && C.Threat != C.Foe ? Find(C.Threat) : nullptr;
		if (Third && Third->Step == Phase::Wing && Third->Lead == C.Foe) { Third = nullptr; C.Threat = -1; }
		const bool bThird = Third && AirT >= C.ThreatSeen;
		if (!bWith || bThird || C.bHurt || C.P.Z - GroundAt(C.P.X, C.P.Y) < 350.0 || D > 1000.0 || AirT > C.ScissUntil)
		{
			Check.LongestScissors = std::max(Check.LongestScissors, C.StepT);
			C.Step = Phase::None;
			C.StepT = 0.0;
			C.Foe = -1;
			C.ScissNext = AirT + 25.0;
			if (!bThird) C.Threat = -1;
			return false;
		}
		// Each turns toward the other, never more than so far off the line the chase was flown on: they cross, and each
		// rolls back the other way. First the overshoot: the attacker comes through fast and nearly straight while the
		// other hangs back in a wide, slow weave. Then the one who is to come out behind hangs back, and the other, out
		// in front, weaves narrow and fast to get clear.
		const Vec Fa = Ahead(C.ScissAxis), Rt = RightOf(C.ScissAxis);
		const double Fore = Rel.Dot(Fa), Lat = Rel.Dot(Rt), Side = Lat >= 0.0 ? 1.0 : -1.0;
		if (C.bScissIn && (Fore < 30.0 || C.StepT > 8.0)) C.bScissIn = false;
		const bool bOpening = C.bScissIn || O->bScissIn;
		const bool bBack = bOpening ? !C.bScissIn : C.bScissWin;
		const double Wide = (bBack ? 58.0 : C.bScissIn ? 16.0 : 44.0) * Deg;
		// (he rolls back toward the other once the other has crossed him, and not before he has swung most of the way out)
		const double Swung = C.ScissTurn * Wrap(C.Hdg - C.ScissAxis);
		if (C.ScissTurn == 0.0) C.ScissTurn = Side;
		else if (Side != C.ScissTurn && (Swung > 0.75 * Wide || std::fabs(Lat) > 170.0))
		{
			C.ScissTurn = Side;
			C.ScissLegs++;
			Check.ScissorTurns++;
		}
		Cmd K;
		K.Hdg = C.ScissAxis + C.ScissTurn * Wide;
		K.Spd = bBack ? 106.0 : C.bScissIn ? C.VTop : 122.0;
		// (the other well out in front of him now: round on to his tail)
		if (!bOpening && C.bScissWin && C.ScissLegs >= 2 && Fore > 110.0) K = Toward(C, O->P + Velocity(*O) * (D / 800.0), 118.0);
		// (a little over or under him at the crossing, never through him)
		K.Gam = std::min(Clamp((O->P.Z + (C.Key < O->Key ? 28.0 : -28.0) - C.P.Z) * 0.004, -0.14, 0.14), ClimbMost(C));
		MindGround(C, K);
		Fly(C, K, Dt, true);
		FightSpeed(C);
		// A snap shot as he crosses before the nose.
		Squeeze(C, *O, G, 340.0, 9.0 * Deg, 0.4, 0.9, Dt);
		C.Target = O->Key;
		C.Task = Fore >= 0.0 ? "attacking" : "evading";
		// (losing it, the enemy behind him: his partner's cue to come for the one on his tail)
		if (C.Threat < 0 || C.Threat == C.Foe)
		{
			C.Threat = !C.bScissWin && C.StepT > 5.0 && Fore < -80.0 ? C.Foe : -1;
			if (C.Threat >= 0) C.ThreatSeen = std::min(C.ThreatSeen, AirT);
		}
		return true;
	}

	bool Show::Evading(Craft& C, const Group& G, double Dt)
	{
		const double Over = C.P.Z - GroundAt(C.P.X, C.P.Y);
		const bool bZooming = C.bFleeing && C.Step == Phase::Zoom;
		if (C.Step != Phase::Evade && !bZooming)
		{
			if (C.Threat < 0 || AirT < C.ThreatSeen) return false;
			// (his own shot first, if his guns are going; and one already breaking away at speed keeps going)
			if (C.Burst > 0.0 && (C.Step == Phase::Run || C.Step == Phase::Turn || C.Step == Phase::Chase)) return false;
			if (C.Step == Phase::Break || C.Step == Phase::Zoom) return false;
			const Craft* F = Find(C.Threat);
			if (!F) return false;
			// (going in on a bomber he presses on, his speed and his break after it his way out: inside 900 m of it
			// whatever is behind him, and farther out unless the enemy is close enough to fire)
			const Craft* Mine = C.Step == Phase::Run && C.Target >= 0 ? Find(C.Target) : nullptr;
			if (Mine && !Mine->bFighter && (Len(Mine->P - C.P) < 900.0 || Len(F->P - C.P) > 420.0)) return false;
			C.Step = Phase::Evade;
			C.StepT = 0.0;
			C.Foe = C.Threat;
			C.bCover = C.bReversed = false;
			C.Hand = (F->P - C.P).Dot(RightOf(C.Hdg)) >= 0.0 ? 1.0 : -1.0;
			// A diving type with the height, the enemy close behind, half-rolls and dives away two times in five, then
			// climbs back on its speed; else he breaks hard into the attack.
			// (not from below the enemy he is there to attack: the dive would take him out of the fight)
			const auto Mark = Now.find(C.TargetGroup);
			const bool bHigh = Mark == Now.end() || C.P.Z > Mark->second.Alt - 150.0;
			C.bFleeing = G.Style == "dive" && bHigh && Over > 1600.0 && Len(F->P - C.P) < 450.0 && Roll.U() < 0.4;
			Check.Evasions++;
			if (C.bFleeing) Check.SplitS++;
			// Or the break makes him overshoot: an enemy close behind comes on too fast to hold the turn, slides out past
			// his tail, and the two fall into a scissors. (Not where the battle has counted a loss on his group at that
			// enemy's guns, yet to be shown: that chase ends as the battle has it.)
			Craft* A = !C.bFleeing && AirT > C.ScissNext && Over > 700.0 ? Find(C.Threat) : nullptr;
			if (A && !(A->Target == C.Key && (A->Step == Phase::Chase || A->Step == Phase::Turn || A->Step == Phase::Run) && AirT > A->ScissNext
			           && A->P.Z - GroundAt(A->P.X, A->P.Y) > 700.0 && DueOn(C.Group, A->Group) == 0)) A = nullptr;
			// (close, and closing; the less often the better the pilot coming in, who would pull up and over instead; and
			// seldom one diving through, whose way is up and away on his speed, never round and round)
			const Vec Gap = A ? C.P - A->P : Vec();
			const double Far = Len(Gap);
			if (A && Far < 340.0 && Far > 1.0 && (Velocity(*A) - Velocity(C)).Dot(Gap) > 10.0 * Far && Roll.U() < (A->Step == Phase::Run ? 0.6 : 0.9) - 0.5 * A->Skill)
			{
				// Who comes out behind: his, where the battle has a loss of the enemy's group to show at his group's guns;
				// else the better pilot's more often than not.
				const bool bMine = DueOn(A->Group, C.Group) > 0 || Roll.U() < Clamp(0.5 + 0.8 * (C.Skill - A->Skill), 0.15, 0.85);
				C.Step = A->Step = Phase::Scissor;
				C.StepT = A->StepT = 0.0;
				C.Foe = A->Key;
				A->Foe = C.Key;
				C.Target = A->Key;
				C.ScissAxis = A->ScissAxis = A->Hdg;
				C.ScissUntil = A->ScissUntil = AirT + Roll.U(17.0, 23.0);
				C.ScissTurn = A->ScissTurn = 0.0;
				C.ScissLegs = A->ScissLegs = 0;
				C.bScissWin = bMine;
				A->bScissWin = !bMine;
				C.bScissIn = false;
				A->bScissIn = true;
				A->bCover = A->bReversed = A->bAnswer = false;
				A->Burst = 0.0;
				A->YoYo = 0.0;
				Check.Scissors++;
				return Scissoring(C, G, Dt);
			}
		}
		Cmd K;
		C.Task = "evading";
		if (C.bFleeing)
		{
			if (C.Step == Phase::Evade)
			{
				// Over on his back, the nose pulled down through the dive, then out from under and away from him.
				K.Hdg = C.StepT < 3.2 ? C.Hdg : C.Hdg - C.Hand * 1.0;
				K.Spd = C.VTop;
				K.bBank = C.StepT < 3.2;
				K.Bank = C.Hand * 172.0 * Deg;
				K.Gam = C.StepT < 0.9 ? C.Gam : C.StepT < 3.2 ? -58.0 * Deg : -8.0 * Deg;
				MindGround(C, K);
				Fly(C, K, Dt, true);
				if (C.StepT > 5.0 || Over < 500.0) { C.Step = Phase::Zoom; C.StepT = 0.0; }
			}
			else
			{
				K.Hdg = C.Hdg;
				K.Gam = 32.0 * Deg;
				K.Spd = C.VMax;
				Fly(C, K, Dt, true);
				if (C.StepT > 7.0 || C.Spd < 122.0)
				{
					C.bFleeing = false;
					C.Step = Phase::None;
					C.StepT = 0.0;
					C.Threat = -1;
				}
			}
			FightSpeed(C);
			return true;
		}
		// The break: a hard turn into him, the nose a little down to keep the speed up.
		Craft* F = C.Foe >= 0 ? Find(C.Foe) : nullptr;
		if (F && !(Live(*F) && F->Doing == Role::Fight)) F = nullptr;
		const bool bStillOn = F && C.Threat == C.Foe;
		if (F && (bStillOn || C.StepT < 1.5) && C.StepT < 9.0)
		{
			const Vec Rel = F->P - C.P;
			const double D = Len(Rel);
			K.Hdg = std::atan2(Rel.X, Rel.Y);
			K.Gam = -8.0 * Deg;
			K.Spd = C.VMax;
			MindGround(C, K);
			Fly(C, K, Dt, true);
			FightSpeed(C);
			// He has slid past in front: the reversal, on to his tail in turn (the scissors, when he reverses too).
			if (C.StepT > 1.2 && D < 650.0 && Ahead(C.Hdg, C.Gam).Dot(Rel) > 0.25 * D)
			{
				Check.Reversals++;
				C.Step = Phase::Chase;
				C.StepT = 0.0;
				C.bReversed = true;
				C.Target = F->Key;
				C.Threat = -1;
			}
			return true;
		}
		C.Step = Phase::None;
		C.StepT = 0.0;
		C.Foe = -1;
		return false;
	}

	bool Show::Covering(Craft& C, const Group& G, Craft* Mate, double Dt)
	{
		if (C.Step == Phase::Chase && (C.bCover || C.bReversed))
		{
			Craft* T = C.Target >= 0 ? Find(C.Target) : nullptr;
			bool bOn = T && Live(*T) && T->Doing == Role::Fight && T->Step != Phase::Away && C.StepT < (C.bCover ? 14.0 : 10.0) && Len(T->P - C.P) < 2000.0;
			// (after the enemy on his partner's tail only while it is on it; after the one that slid past only while it
			// is still before his nose)
			if (bOn && C.bCover) bOn = Mate && (Mate->Threat == T->Key || C.StepT < 2.5);
			if (bOn && C.bReversed) bOn = C.StepT < 2.0 || (T->P - C.P).Dot(Ahead(C.Hdg, C.Gam)) > 0.0;
			if (!bOn)
			{
				C.bCover = C.bReversed = false;
				C.Step = Phase::None;
				C.StepT = 0.0;
				return false;
			}
			TailChase(C, *T, G, Dt);
			C.Task = C.bCover ? "covering" : "attacking";
			return true;
		}
		if (!Mate || Mate->Threat < 0 || AirT < Mate->ThreatSeen || Mate->Step == Phase::Away) return false;
		if (C.Step == Phase::Break || C.Step == Phase::Zoom || (C.Step == Phase::Run && C.Burst > 0.0)) return false;
		// (a leader going in on a bomber goes on in: his wingman breaks for himself and comes back to him)
		if (C.Step == Phase::Run && C.Lead == C.Key)
		{
			const Craft* Mine = C.Target >= 0 ? Find(C.Target) : nullptr;
			if (Mine && !Mine->bFighter) return false;
		}
		Craft* T = Find(Mate->Threat);
		if (!T || !Live(*T) || Len(T->P - C.P) > 1800.0) return false;
		C.bCover = true;
		C.bReversed = C.bAnswer = false;
		C.Step = Phase::Chase;
		C.StepT = 0.0;
		C.Since = AirT;
		C.Target = T->Key;
		Check.Covers++;
		TailChase(C, *T, G, Dt);
		C.Task = "covering";
		return true;
	}

	bool Show::Answering(Craft& C, const Group& G, double Dt)
	{
		// A loss the battle has counted on an enemy fighter group at his group's guns, which the scene has yet to show:
		// the pair best placed goes for one of that group now, and the loss is shown when his guns bear. The battle
		// decided it; the scene only finds the pilot to act it.
		if (C.bAnswer)
		{
			Craft* T = C.Step == Phase::Chase && C.Target >= 0 ? Find(C.Target) : nullptr;
			const bool bOn = T && Live(*T) && T->Doing == Role::Fight && DueOn(T->Group, G.Id) > 0 && C.StepT < 18.0 && Len(T->P - C.P) < 2600.0;
			if (!bOn)
			{
				C.bAnswer = false;
				if (C.Step == Phase::Chase) { C.Step = Phase::None; C.StepT = 0.0; C.Target = -1; }
				return false;
			}
			TailChase(C, *T, G, Dt);
			C.Task = "attacking";
			return true;
		}
		if (Due.empty()) return false;
		// (not out of a break or an evasion, nor off a bomber he is about to fire on)
		if (C.Step == Phase::Break || C.Step == Phase::Zoom || C.Step == Phase::Evade || C.Burst > 0.0) return false;
		if (C.Step == Phase::Run)
		{
			const Craft* Mine = C.Target >= 0 ? Find(C.Target) : nullptr;
			if (Mine && !Mine->bFighter && Len(Mine->P - C.P) < 900.0) return false;
		}
		// (as many of his group after each enemy group as it has losses to show)
		std::map<int, int> Taken;
		for (const auto& Pair : Crafts)
		{
			const Craft& O = Pair.second;
			if (O.Group != G.Id || !O.bAnswer || O.Target < 0) continue;
			const Craft* V = Find(O.Target);
			if (V) Taken[V->Group]++;
		}
		Craft* Pick = nullptr;
		double Best = 2200.0;
		for (auto& Pair : Crafts)
		{
			Craft& F = Pair.second;
			if (F.Side == C.Side || !F.bFighter || !Live(F) || F.Doing != Role::Fight || F.Step == Phase::Away) continue;
			const int Owed = DueOn(F.Group, G.Id);
			if (Owed <= 0) continue;
			const auto Tk = Taken.find(F.Group);
			if (Tk != Taken.end() && Tk->second >= Owed) continue;
			// (the nearest; one before his nose rather than behind him, and not one already turning hard)
			const Vec Rel = F.P - C.P;
			double Cost = Len(Rel);
			if (Ahead(C.Hdg, C.Gam).Dot(Rel) < 0.0) Cost += 500.0;
			if (Dodging(F)) Cost += 400.0;
			if (Cost < Best) { Best = Cost; Pick = &F; }
		}
		if (!Pick) return false;
		C.bAnswer = true;
		C.bCover = C.bReversed = false;
		C.Step = Phase::Chase;
		C.StepT = 0.0;
		C.Since = AirT;
		C.Target = Pick->Key;
		Chasers[Pick->Key].push_back(C.Key);
		TailChase(C, *Pick, G, Dt);
		C.Task = "attacking";
		return true;
	}

	void Show::FlyAway(Craft& C, const Group& G, Craft* Follow, double Dt)
	{
		if (C.Step != Phase::Away)
		{
			C.Step = Phase::Away;
			C.StepT = 0.0;
			C.bCover = C.bReversed = false;
			C.Target = -1;
		}
		C.Task = "going home";
		// A model of a big group stands for several aircraft: the one hit hard, and the wingman seeing him home, are
		// gone for good, and the next of the group closes up in their place. In a small group every model is one
		// aircraft the battle still counts, so he stands off the fight until the group goes home.
		const bool bHit = C.bHurt || (Follow && Follow->bHurt) || (Follow && Follow->Doing == Role::Home);
		if (bHit && G.Per >= 2 && C.StepT > 5.0)
		{
			Leave(C);
			return;
		}
		if (Follow && Live(*Follow) && Follow->Step == Phase::Away)
		{
			WingOn(C, *Follow, 70.0, 60.0, Dt);
			return;
		}
		// Out to the side of the fight toward home (the raider away from the target, the defender toward it), lower,
		// and round and round there.
		Vec Way = C.Side == 0 ? Vec(G.X - TargetX, G.Y - TargetY, 0.0) : Vec(TargetX - G.X, TargetY - G.Y, 0.0);
		Way = Norm(Way, Ahead(G.Face + Pi));
		const Vec Stand = Vec(G.X, G.Y, std::max(G.Alt - 400.0, GroundAt(G.X, G.Y) + 400.0)) + Way * 2600.0;
		const double Wv = AirT * 2.0 * Pi / 40.0 + C.Weave;
		Cmd K = Steer(C, Stand + Vec(std::sin(Wv), std::cos(Wv), 0.0) * 500.0, GroupVelocity(G.Id), 0.06, 95.0, 128.0);
		Ease(C, K, 50.0 * Deg);
		MindGround(C, K);
		Fly(C, K, Dt, true);
	}

	void Show::WingOn(Craft& C, const Craft& L, double Back, double Wide, double Dt)
	{
		// The fighting wing: back and out to his own side of his leader, and through whatever his leader does.
		const double Side = C.Seat % 2 == 1 ? -1.0 : 1.0;
		const Vec To = L.P - Ahead(L.Hdg, L.Gam) * Back + RightOf(L.Hdg) * (Side * Wide) - Up * 6.0;
		const Vec Lv = Velocity(L);
		// (he keeps his speed up, to climb with his leader; and may fly a little faster than him to catch him up)
		const double Apart = Len(L.P - C.P);
		C.bRejoin = Apart > (C.bRejoin ? 160.0 : 280.0);
		Cmd K = Steer(C, To, Lv, 0.7, 108.0, std::max(C.VMax, Len(Lv) + (C.bRejoin ? 40.0 : 25.0)));
		// (and climbs with him: his leader's climb is his, slow or not, while he has flying speed)
		if (C.Spd > 100.0 && To.Z > C.P.Z)
		{
			const Vec Want = Lv + (To - C.P) * 0.7;
			K.Gam = Clamp(std::atan2(Want.Z, std::max(1.0, Flat(Want))), -0.6, std::max(ClimbMost(C), std::min(std::max(L.Gam, 0.0) + 0.08, 0.4)));
		}
		MindGround(C, K, 120.0);
		Fly(C, K, Dt, true);
		if (C.Doing == Role::Fight && C.Step == Phase::Wing && C.WingOf == L.Key && AirT - C.WingAt > 15.0 && !Dodging(L) && L.Step != Phase::Zoom)
		{
			const double Off = Len(L.P - C.P);
			Check.WingSum += Off;
			Check.WingSamples++;
			if (Off > 500.0) Check.WingFar++;
		}
	}

	void Show::FlyWing(Craft& C, Craft& L, const Group& G, double Dt)
	{
		if (C.Step != Phase::Wing || C.WingOf != L.Key)
		{
			C.Step = Phase::Wing;
			C.StepT = 0.0;
			C.bCover = C.bReversed = false;
			C.WingOf = L.Key;
			C.WingAt = AirT;
		}
		C.TargetGroup = L.TargetGroup;
		C.bStrafing = L.bStrafing;
		C.Task = L.Task;
		// His leader's half-roll and dive (a break under a box, or away from an enemy), half a second behind him.
		const double Behind = L.StepT - 0.5;
		const bool bDives = (L.Step == Phase::Break && L.bUnder) || (L.Step == Phase::Evade && L.bFleeing);
		if (bDives && Behind > 0.0 && Behind < 3.2)
		{
			Cmd K;
			K.Hdg = C.Hdg;
			K.Spd = C.VTop;
			K.bBank = true;
			K.Bank = L.Hand * 170.0 * Deg;
			K.Gam = Behind < 1.0 ? C.Gam : L.Step == Phase::Break ? -50.0 * Deg : -58.0 * Deg;
			MindGround(C, K);
			Fly(C, K, Dt, true);
			FightSpeed(C);
			C.Target = -1;
			return;
		}
		// Closer abreast of him as he goes in, to fire with him; looser the greener the pilot.
		const bool bIn = L.Step == Phase::Run;
		const double Loose = 30.0 * (1.0 - C.Skill);
		WingOn(C, L, bIn ? 40.0 : 60.0 + Loose, bIn ? 75.0 : 55.0 + Loose, Dt);
		// His guns with his leader's when his own nose is on it: at the same fighter; in a box, at the bomber beside his
		// leader's on his own side, as each of a pair or four going in on a box took his own.
		Craft* T = L.Target >= 0 ? Find(L.Target) : nullptr;
		if (T && !Live(*T)) T = nullptr;
		if (T && !T->bFighter && T->Doing == Role::Slot && bIn)
		{
			Craft* Own = C.OwnMarkBy == T->Key && C.OwnMark >= 0 ? Find(C.OwnMark) : nullptr;
			if (!Own || !Live(*Own) || Own->Doing != Role::Slot)
			{
				const Vec Side = RightOf(L.Hdg) * ((C.P - L.P).Dot(RightOf(L.Hdg)) >= 0.0 ? 1.0 : -1.0);
				double Near = 260.0;
				Own = nullptr;
				for (auto& Pair : Crafts)
				{
					Craft& O = Pair.second;
					if (O.Group != T->Group || O.Key == T->Key || !Live(O) || O.Doing != Role::Slot) continue;
					const Vec Rel = O.P - T->P;
					const double D = Len(Rel);
					if (D < Near && Rel.Dot(Side) > 0.0) { Near = D; Own = &O; }
				}
				C.OwnMark = Own ? Own->Key : -1;
				C.OwnMarkBy = T->Key;
			}
			if (Own) T = Own;
		}
		C.Target = T ? T->Key : -1;
		if (T && (L.Step == Phase::Run || L.Step == Phase::Turn || L.Step == Phase::Chase))
		{
			if (T->bFighter) Squeeze(C, *T, G, 480.0 - 100.0 * C.Skill, 8.0 * Deg, 1.0, 2.0, Dt);
			else Squeeze(C, *T, G, L.Pass == "headon" ? 900.0 : 700.0 - 150.0 * C.Skill, 10.0 * Deg, L.Pass == "headon" ? 0.5 : 1.0, L.Pass == "headon" ? 1.2 : 2.0, Dt);
			if (bIn) FightSpeed(C);
		}
	}

	bool Show::FlyGuard(Craft& C, const Group& G, double Dt)
	{
		auto BIt = Now.find(G.With);
		if (BIt == Now.end()) return false;
		const Group& Bx = BIt->second;
		const Vec Box(Bx.X, Bx.Y, Bx.Alt), Fb = Ahead(Bx.Face), Rb = RightOf(Bx.Face), Bv = GroupVelocity(Bx.Id);
		Craft* T = C.Step == Phase::Chase && C.Target >= 0 ? Find(C.Target) : nullptr;
		if (C.Step == Phase::Chase)
		{
			// The leash: he chases an attacker off, no farther than 1.9 km from his bombers nor longer than 16 s, and
			// comes back.
			const bool bOn = T && Live(*T) && T->Doing == Role::Fight && T->Step != Phase::Away && AirT - C.Since < 16.0
				&& Flat(C.P - Box) < 1900.0 && Flat(T->P - Box) < 2600.0;
			if (!bOn)
			{
				C.Step = Phase::Station;
				C.StepT = 0.0;
				C.Target = -1;
				C.TargetGroup = -1;
				T = nullptr;
				Check.GuardReturns++;
			}
		}
		if (C.Step != Phase::Chase)
		{
			if (C.Step != Phase::Station)
			{
				C.Step = Phase::Station;
				C.StepT = 0.0;
				C.Target = -1;
				C.TargetGroup = -1;
			}
			// An enemy fighter coming in on the box inside 2 km of it (or after its escort, inside 900 m): the nearest
			// leader whose pair none of his own side is already chasing. One pair to an attacking pair, and a breath by
			// the box between chases.
			if (C.StepT > 8.0 && Flat(C.P - Box) < 1500.0)
			{
				Craft* Pick = nullptr;
				double Near = 2000.0;
				auto See = [&](int GroupId, double Within)
				{
					auto H = Hunters.find(GroupId);
					if (H == Hunters.end()) return;
					for (int Key : H->second)
					{
						Craft* F = Find(Key);
						if (!F || F->Side == C.Side || !Live(*F) || F->Doing != Role::Fight || F->Step == Phase::Away || Dodging(*F) || F->Lead != F->Key) continue;
						const double D = Len(F->P - Box);
						if (D >= std::min(Near, Within)) continue;
						bool bTaken = false;
						for (int Member : { F->Key, F->Wing })
						{
							auto Ch = Chasers.find(Member);
							if (Member == 0 || Ch == Chasers.end()) continue;
							for (int Other : Ch->second) { const Craft* O = Find(Other); bTaken = bTaken || (O && O->Side == C.Side); }
						}
						if (bTaken) continue;
						Near = D;
						Pick = F;
					}
				};
				See(Bx.Id, 2000.0);
				See(G.Id, 900.0);
				if (Pick)
				{
					C.Step = Phase::Chase;
					C.StepT = 0.0;
					C.Since = AirT;
					C.Target = Pick->Key;
					C.TargetGroup = Pick->Group;
					Chasers[Pick->Key].push_back(C.Key);
					T = Pick;
					Check.GuardChases++;
				}
			}
		}
		if (T)
		{
			TailChase(C, *T, G, Dt);
			C.Task = "covering";
			return true;
		}
		// By the box: above it and out to one side, weaving wide to hold the bombers' speed with fighting speed in hand.
		const double Side = C.Flight % 2 == 0 ? 1.0 : -1.0;
		const double Wv = AirT * 2.0 * Pi / 30.0 + C.Weave * 0.2;
		const Vec To = Box + Rb * (Side * 620.0 + 600.0 * std::sin(Wv)) + Up * 400.0 + Fb * (C.Seat >= 2 ? -350.0 : 150.0);
		Cmd K = Steer(C, To, Bv + Rb * (600.0 * 2.0 * Pi / 30.0 * std::cos(Wv)), 0.1, 130.0, C.VMax);
		MindGround(C, K);
		Fly(C, K, Dt, true);
		C.Task = "escorting";
		return true;
	}

	void Show::FlyPass(Craft& C, const Group& G, const Group& E, double Dt)
	{
		Band& B = Bands[G.Id];
		Wave& W = B.Waves[E.Id];
		const Vec Box(E.X, E.Y, E.Alt), Fb = Ahead(E.Face), Rb = RightOf(E.Face), Bv = GroupVelocity(E.Id);
		const double Fast = C.VMax, Slow = 111.0;
		const std::string Named = PassOn(G, E.Id);
		const bool bSternKind = Named == "astern" || Named == "close" || Named == "below";
		// His four's leader, when he leads its second pair and they are on the same box.
		Craft* Boss = C.Boss ? Find(C.Boss) : nullptr;
		if (Boss && (!Live(*Boss) || Boss->Doing != Role::Fight || Boss->TargetGroup != E.Id || Boss->bGuard || Boss->Step == Phase::Away || Dodging(*Boss) || Boss->Step == Phase::Chase)) Boss = nullptr;
		if (Boss && C.Step != Phase::Run && C.Step != Phase::Break) C.Hand = Boss->Hand;
		Craft* T = C.Target >= 0 ? Find(C.Target) : nullptr;
		if (T && (T->Group != E.Id || !Live(*T))) { T = nullptr; C.Target = -1; }
		// (no room under a low box: every break is up and over it)
		const bool bLow = E.Alt - GroundAt(E.X, E.Y) < 900.0;
		if (C.Step != Phase::Run && C.Step != Phase::Break && C.Step != Phase::Hold && C.Step != Phase::Setup && C.Step != Phase::Climb)
		{
			// The fight's opening: in from where he stands. After that, from the perch his pass is flown from.
			C.bAnywhere = !C.bOpened && Len(Box - C.P) < 3500.0;
			C.Step = C.bAnywhere ? Phase::Hold : Phase::Setup;
			C.StepT = 0.0;
			C.bLow = bSternKind && C.P.Z < Box.Z - 350.0;
		}
		// (far below the box after a dive, he comes up at it from low astern rather than climb all the way back
		// above it)
		const std::string Pass = C.bLow ? std::string("below") : Named;
		auto BreakOff = [&](bool bOver)
		{
			C.bUnder = !bOver;
			(C.bUnder ? Check.UnderBreaks : Check.OverBreaks)++;
			C.Step = Phase::Break;
			C.StepT = 0.0;
			C.Burst = 0.0;
		};
		switch (C.Step)
		{
		case Phase::Run:
		{
			// (the one he was going in on has fallen out of its box, with a loss counted in the box itself and none on
			// this cripple: he lets it go for one still in the box)
			if (T && T->Doing == Role::Straggle && C.Burst <= 0.0 && DueOn(E.Id, G.Id) > 0 && !DueStrag.count({ E.Id, T->StragId })) T = nullptr;
			if (!T)
			{
				// (the one he was on has gone down: the next of them before his nose, or away)
				const int Next = MarkBomber(C, E, C.Pass);
				T = Next >= 0 ? Find(Next) : nullptr;
				if (T && (Len(T->P - C.P) > 900.0 || Ahead(C.Hdg, C.Gam).Dot(T->P - C.P) < 0.0)) T = nullptr;
				C.Target = T ? T->Key : -1;
			}
			if (!T)
			{
				BreakOff(true);
				break;
			}
			const bool bStrafe = T->Doing == Role::Ground;
			C.bStrafing = bStrafe;
			const Vec Tp = T->P, Tv = Velocity(*T);
			const double Dist = Len(Tp - C.P);
			const Vec Aim = Tp + Tv * (Dist / 800.0);
			const bool bHead = C.Pass == "headon";
			const bool bStern = C.Pass == "astern" || C.Pass == "close" || C.Pass == "below";
			// (from behind he throttles back as he closes, to hold his fire on it and not run into it)
			// (coming up at it from below he keeps his speed, or he could not hold his nose up on it)
			const double Speed = C.Pass == "below" ? Fast : bStern && Dist < 700.0 ? Clamp(Len(Tv) + 38.0, 118.0, Fast) : Dist > 900.0 ? C.VTop : Fast;
			Cmd K = Toward(C, Aim, Speed);
			MindGround(C, K, bStrafe ? 40.0 : 150.0);
			Fly(C, K, Dt, true);
			FightSpeed(C);
			C.Task = "attacking";
			// A head-on pass closes at 240 m/s: half a second to a second of fire from 900 m. From behind, a second or
			// two from as near as his skill takes him.
			const double Range = bHead ? 900.0 : bStrafe ? 700.0 : 700.0 - 150.0 * C.Skill;
			Squeeze(C, *T, G, Range, 10.0 * Deg, bHead ? 0.5 : 1.0, bHead ? 1.2 : bStern ? 2.0 : 2.2, Dt);
			const double Near = bStrafe ? 330.0 : bHead ? 270.0 - 60.0 * C.Skill : C.Pass == "close" ? 115.0 : 200.0 - 70.0 * C.Skill;
			const bool bPast = C.StepT > 2.0 && Dist < 500.0 && (Tp - C.P).Dot(Ahead(C.Hdg, C.Gam)) < 0.0;
			if (Dist < Near || bPast || C.StepT > 30.0)
			{
				// Three breaks in four a half-roll and a dive beneath the box; the rest over it, most after a pass from a
				// little high.
				const double Over = bStrafe || bLow ? 1.0 : (C.Pass == "astern" || C.Pass == "quarter" || C.Pass == "opening") ? 0.35 : C.Pass == "below" ? 0.1 : 0.2;
				BreakOff(Roll.U() < Over);
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
				// Over on its back, the nose pulled down through the dive, then right side up and out underneath: out to
				// the side from a pass from behind, to let the box go by; straight on through from the beam.
				K.bBank = C.StepT < 3.4;
				K.Bank = C.Hand * 170.0 * Deg;
				K.Gam = C.StepT < 1.1 ? C.Gam : C.StepT < 3.4 ? -50.0 * Deg : -8.0 * Deg;
				if (C.StepT >= 3.4 && C.Pass != "beam") K.Hdg = C.Hdg + C.Hand * 1.2;
			}
			else
			{
				K.bBank = true;
				K.Bank = C.Hand * 60.0 * Deg;
				K.Gam = 30.0 * Deg;
				K.Spd = Fast;
			}
			MindGround(C, K);
			Fly(C, K, Dt, true);
			C.Task = "breaking";
			if (C.StepT > (C.bUnder ? 6.2 : 4.0))
			{
				C.Step = Phase::Climb;
				C.StepT = 0.0;
				C.Target = -1;
				C.bLow = bSternKind && C.P.Z < Box.Z - 350.0;
				if (!Boss && Roll.U() < 0.4) C.Hand = -C.Hand;
			}
			break;
		}
		case Phase::Hold:
		{
			Cmd K;
			double Eta = 0.0;
			if (C.bAnywhere) K = Toward(C, Box + Up * 120.0, Fast);
			else
			{
				// On his perch, along with the box, weaving to hold its speed, until his four's turn on the beat.
				const Vec P = PerchOf(C, Pass, Box, Fb, Rb);
				const double Wv = AirT * 2.0 * Pi / 20.0 + C.Weave;
				K = Steer(C, P + Rb * (320.0 * std::sin(Wv)), Bv + Rb * (320.0 * 2.0 * Pi / 20.0 * std::cos(Wv)), 0.1, 110.0, Fast);
				Eta = EtaTo(C, Box, Bv, Pass == "headon" ? 900.0 : 600.0);
			}
			MindGround(C, K);
			Fly(C, K, Dt, true);
			C.Task = "positioning";
			// (a loss the battle has counted on this box at his group's guns, which the scene has yet to show, and
			// nobody going in: he goes now)
			const bool bDue = DueOn(E.Id, G.Id) > RunsOn(E.Id, G.Id);
			if (MayGo(C, W, Boss, Eta) || bDue || C.StepT > (C.bAnywhere ? 8.0 : 26.0)) StartRun(C, G, E, C.bAnywhere ? std::string("opening") : Pass);
			break;
		}
		default:
		{
			// Back up to his perch for the next pass. One ahead of the box is reached round its side, out of its
			// gunners' reach, never through it.
			const Vec P = PerchOf(C, Pass, Box, Fb, Rb);
			Vec To = P;
			const double Along = (C.P - Box).Dot(Fb), Across = (C.P - Box).Dot(Rb), PerchAlong = (P - Box).Dot(Fb);
			if (PerchAlong > 600.0 && Along < PerchAlong - 500.0 && std::fabs(Across) < 1000.0)
			{
				if (!Boss && std::fabs(Across) > 150.0) C.Hand = Across > 0.0 ? 1.0 : -1.0;
				To = Box + Fb * (Along + 1300.0) + Rb * (C.Hand * 1150.0) + Up * (P.Z - Box.Z);
			}
			Cmd K = Steer(C, To, Bv, 0.09, Slow, Fast);
			// (height costs speed: with height to make he keeps his engine wide open and the speed of his dive, pulls
			// up on it first, and comes round in a climbing turn no harder than the climb can pay for)
			if (To.Z - C.P.Z > 60.0)
			{
				K.Spd = std::max(Fast, C.Spd);
				Ease(C, K, (C.Spd > 138.0 ? 45.0 : 66.0) * Deg);
			}
			MindGround(C, K);
			Fly(C, K, Dt, true);
			C.Task = "positioning";
			// A loss the battle has counted on this box at his group's guns, which the scene has yet to show, and
			// nobody going in: with the speed and the height for it he goes in now, from where he stands.
			const double Off = Len(Box - C.P);
			// (the box before him; or behind him and far enough off to come round on it)
			if (DueOn(E.Id, G.Id) > RunsOn(E.Id, G.Id) && Off < 2400.0 && C.Spd > 114.0 && C.P.Z > Box.Z - 450.0 && ((Box - C.P).Dot(Ahead(C.Hdg)) > -0.3 * Off || Off > 1000.0))
			{
				StartRun(C, G, E, "opening");
				break;
			}
			// (on his perch when he is by it with his nose the way the pass goes)
			if ((Len(P - C.P) < 700.0 && Ahead(C.Hdg).Dot(Box - P) > 0.0) || C.StepT > 110.0)
			{
				C.Step = Phase::Hold;
				C.StepT = 0.0;
			}
			break;
		}
		}
	}

	Show::Craft* Show::PickFoe(Craft& C, const Group& G, const Group& E, const Vec& Middle)
	{
		// What the other leaders of his group are after, and which enemies sit on its pilots' tails.
		std::map<int, int> Claimed;
		std::set<int> Menacing;
		for (const auto& Pair : Crafts)
		{
			const Craft& O = Pair.second;
			if (O.Group != G.Id || O.Key == C.Key || !Live(O)) continue;
			if (O.Lead == O.Key && O.Target >= 0) Claimed[O.Target]++;
			if (O.Threat >= 0) Menacing.insert(O.Threat);
		}
		const bool bEscort = G.With >= 0 && Now.count(G.With);
		Craft* Best = nullptr;
		double BestCost = 1e18;
		for (auto& Pair : Crafts)
		{
			Craft& F = Pair.second;
			if (F.Side == C.Side || !F.bFighter || !Live(F) || F.Doing == Role::Ground) continue;
			// The enemy group his pair was given; an escort also takes whoever is after its bombers or itself; and
			// anyone on a comrade's tail.
			const bool bOnBox = bEscort && F.Doing == Role::Fight && F.TargetGroup == G.With;
			const bool bOnUs = F.Doing == Role::Fight && F.TargetGroup == G.Id;
			if (F.Group != E.Id && !(bEscort && (bOnBox || bOnUs)) && !Menacing.count(F.Key)) continue;
			// The nearest, not one a comrade already has; first of all one on a comrade's tail, and for an escort one
			// going in on the bombers. One far from the fight, or leaving it, is let go.
			double Cost = Len(F.P - C.P);
			auto Cl = Claimed.find(F.Key);
			if (Cl != Claimed.end()) Cost += 700.0 * Cl->second;
			if (Menacing.count(F.Key)) Cost -= 900.0;
			if (F.Step == Phase::Away) Cost += 2500.0;
			const double Off = Len(F.P - Middle);
			if (bOnBox && Off < 2500.0) Cost -= 400.0;
			Cost += 2.0 * std::max(0.0, Off - 3500.0);
			if (Cost < BestCost) { BestCost = Cost; Best = &F; }
		}
		return Best;
	}

	bool Show::FlyDuel(Craft& C, const Group& G, const Group& E, double Dt)
	{
		// The middle of the fight: between the two groups; for an escort, its bombers.
		Vec Middle = (Vec(E.X, E.Y, E.Alt) + Vec(G.X, G.Y, G.Alt)) * 0.5;
		int Anchor = G.Id;
		auto BIt = G.With >= 0 ? Now.find(G.With) : Now.end();
		if (BIt != Now.end())
		{
			Middle = Vec(BIt->second.X, BIt->second.Y, BIt->second.Alt);
			Anchor = G.With;
		}
		Craft* T = C.Target >= 0 ? Find(C.Target) : nullptr;
		if (T && (!Live(*T) || T->Side == C.Side || T->Doing == Role::Ground || !T->bFighter)) T = nullptr;
		const bool bBusy = C.Step == Phase::Run || C.Burst > 0.0 || (T && Len(T->P - C.P) < 500.0);
		if (!T || (AirT > C.Repick && !bBusy))
		{
			Craft* Was = T;
			T = PickFoe(C, G, E, Middle);
			C.Repick = AirT + Roll.U(14.0, 22.0);
			if (T != Was) { C.Step = Phase::None; C.StepT = 0.0; }
			C.Target = T ? T->Key : -1;
		}
		if (!T) return false;
		// Drawn too far from the fight: back toward it, and another enemy.
		if (Len(C.P - Middle) > 4200.0 && C.Burst <= 0.0)
		{
			Cmd K = Steer(C, Middle, GroupVelocity(Anchor), 0.05, 111.0, C.VMax);
			MindGround(C, K);
			Fly(C, K, Dt, true);
			C.Task = "positioning";
			C.Repick = 0.0;
			return true;
		}
		if (G.Style == "dive") BoomZoom(C, *T, G, Dt);
		else
		{
			if (C.Step != Phase::Turn) { C.Step = Phase::Turn; C.StepT = 0.0; }
			TailChase(C, *T, G, Dt);
			C.Task = "attacking";
		}
		return true;
	}

	void Show::TailChase(Craft& C, Craft& T, const Group& G, double Dt)
	{
		// After his tail: cutting the corner while he is far, the nose on the lead as the range closes, the circle
		// never tighter than the aircraft turns; a burst when the nose comes on.
		const Vec Tp = T.P, Tv = Velocity(T);
		const Vec Rel = Tp - C.P;
		const double Dist = Len(Rel);
		const double Off = std::acos(Clamp(Ahead(C.Hdg, C.Gam).Dot(Norm(Rel)), -1.0, 1.0));
		const double Closing = (Velocity(C) - Tv).Dot(Norm(Rel));
		const double Fast = C.VMax, Slow = 111.0;
		const Vec Aim = Tp + Tv * (Dist / 800.0);
		Cmd K;
		if (C.YoYo > 0.0)
		{
			// Up and over (the high yo-yo): the speed traded for height above his turn, then down inside it.
			C.YoYo -= Dt;
			K = Toward(C, Aim, Fast);
			K.Gam = std::min(22.0 * Deg, ClimbMost(C));
		}
		else
		{
			// Closing fast on a turning enemy at a wide angle he would slide outside the turn and past: up and over.
			if (Dist < 380.0 && Closing > 28.0 && Off > 28.0 * Deg && C.Spd > 108.0 && AirT > C.YoYoNext)
			{
				C.YoYo = 2.2;
				C.YoYoNext = AirT + 9.0;
				Check.YoYos++;
			}
			// (flat out until he is in range; then a little faster than his enemy, to stay behind him)
			if (Dist > 1000.0) K = Toward(C, Tp + Tv * (Dist / (Fast + 60.0)), C.VTop);
			else if (Dist > 380.0) K = Toward(C, Aim, C.VTop);
			else K = Toward(C, Aim, Clamp(Len(Tv) + 15.0, Slow, C.VTop));
			// (close behind him: no faster than he is, not to run past)
			if (Dist < 140.0) K.Spd = std::max(Slow, Len(Tv) - 8.0);
			K.Gam = Clamp(K.Gam, -0.45, 0.45);
		}
		MindGround(C, K);
		Fly(C, K, Dt, true);
		FightSpeed(C);
		Squeeze(C, T, G, 520.0 - 120.0 * C.Skill, 8.0 * Deg, 1.0, 2.0, Dt);
	}

	void Show::BoomZoom(Craft& C, Craft& T, const Group& G, double Dt)
	{
		// The diving fighter's fight: down on him from above, a burst, and up and away on the speed of the dive, never
		// round and round with him; then over and down again.
		const Vec Tp = T.P, Tv = Velocity(T);
		const Vec Rel = Tp - C.P;
		const double Dist = Len(Rel);
		const Vec Nose = Ahead(C.Hdg, C.Gam);
		const double Fast = C.VMax, Slow = 111.0;
		switch (C.Step)
		{
		case Phase::Run:
		{
			Cmd K = Toward(C, Tp + Tv * (Dist / 800.0), C.VTop);
			MindGround(C, K);
			Fly(C, K, Dt, true);
			FightSpeed(C);
			Squeeze(C, T, G, 540.0 - 100.0 * C.Skill, 9.0 * Deg, 0.8, 1.6, Dt);
			C.Task = "attacking";
			const bool bPast = C.StepT > 2.0 && Nose.Dot(Rel) < 0.0 && Dist < 600.0;
			// (he has turned inside the attack, close in: no turning with him, up and away)
			const bool bInside = C.StepT > 2.5 && Dist < 380.0 && Nose.Dot(Rel) < 0.77 * Dist && C.Burst <= 0.0;
			if (Dist < 150.0 - 40.0 * C.Skill || bPast || bInside || C.StepT > 16.0 || C.P.Z < Tp.Z - 350.0)
			{
				C.Step = Phase::Zoom;
				C.StepT = 0.0;
				C.Burst = 0.0;
			}
			break;
		}
		case Phase::Zoom:
		{
			// (with the speed for it; without it he runs out level first, the nose a little down)
			Cmd K;
			K.Hdg = C.Hdg + C.Hand * 0.25;
			K.Gam = C.Spd > 124.0 ? 38.0 * Deg : -4.0 * Deg;
			K.Spd = Fast;
			MindGround(C, K);
			Fly(C, K, Dt, true);
			FightSpeed(C);
			C.Task = "breaking";
			if (C.StepT > 8.0 || (C.StepT > 2.5 && C.Spd < 116.0))
			{
				C.Step = Phase::Setup;
				C.StepT = 0.0;
				if (Roll.U() < 0.4) C.Hand = -C.Hand;
			}
			break;
		}
		default:
		{
			if (C.Step != Phase::Setup) { C.Step = Phase::Setup; C.StepT = 0.0; }
			Vec S = Tp - Ahead(T.Hdg) * 700.0 + Up * 450.0 + RightOf(T.Hdg) * (C.Hand * 400.0);
			S.Z = std::max(S.Z, GroundAt(S.X, S.Y) + 300.0);
			Cmd K = Steer(C, S, Tv, 0.08, Slow, Fast);
			if (S.Z - C.P.Z > 60.0)
			{
				K.Spd = std::max(Fast, C.Spd);
				Ease(C, K, (C.Spd > 138.0 ? 45.0 : 66.0) * Deg);
			}
			MindGround(C, K);
			Fly(C, K, Dt, true);
			C.Task = "positioning";
			// Down on him from his perch; or at once, with the height in hand and the enemy before his nose (the
			// bounce); or level and fast, when the enemy is there before his nose for the taking; or straight at one
			// who has not seen him: an enemy going in on the bombers, or with his eyes on somebody else.
			const bool bBounce = C.P.Z - Tp.Z > 150.0 && Dist < 1800.0 && Nose.Dot(Rel) > 0.5 * Dist;
			const bool bGift = C.StepT > 1.5 && Dist < 1300.0 && C.Spd > 118.0 && Nose.Dot(Rel) > 0.6 * Dist;
			const bool bUnseen = C.StepT > 2.0 && Dist < 1700.0 && T.Target != C.Key && (T.Threat != C.Key || T.bBlind) && T.Step != Phase::Zoom && !Dodging(T)
				&& (T.Step == Phase::Run || Ahead(T.Hdg).Dot(Rel) > -0.2 * Dist) && C.P.Z > Tp.Z - 250.0 && (Nose.Dot(Rel) > 0.3 * Dist || Dist > 700.0);
			// (and at once at one of a group the battle has counted a loss on at his group's guns, yet to be shown)
			const bool bDue = C.StepT > 1.0 && DueOn(T.Group, C.Group) > 0 && Dist < 2500.0 && C.P.Z > Tp.Z - 300.0;
			if (bBounce || bGift || bUnseen || bDue || Len(S - C.P) < 600.0 || (C.StepT > 18.0 && C.P.Z > Tp.Z - 300.0))
			{
				C.Step = Phase::Run;
				C.StepT = 0.0;
				Check.Passes++;
			}
			break;
		}
		}
	}

	int Show::DueOn(int Victim, int By) const
	{
		const auto It = Due.find({ Victim, By });
		return It == Due.end() ? 0 : It->second;
	}

	int Show::RunsOn(int Box, int By) const
	{
		// The pairs of that group going in on that box now.
		int Runs = 0;
		const auto H = Hunters.find(Box);
		if (H == Hunters.end()) return 0;
		for (int Key : H->second)
		{
			const Craft* F = Find(Key);
			if (F && F->Group == By && F->Lead == F->Key && F->Step == Phase::Run && Live(*F)) Runs++;
		}
		return Runs;
	}

	int Show::UnderGuns(const Event& E) const
	{
		// The fighter whose guns bring this loss down now (his key), or -1: one of the group that shot it down, firing
		// on one of the lost aircraft's group, his burst not having brought one down already. For a fighter lost to a
		// box's gunners, the one of its group close in on that box.
		auto By = Now.find(E.By);
		const int Box = E.GunBox >= 0 ? E.GunBox : (By != Now.end() && !By->second.bFighter ? E.By : -1);
		for (const auto& Pair : Crafts)
		{
			const Craft& S = Pair.second;
			if (!S.bFighter || S.Target < 0 || !Live(S) || S.Doing != Role::Fight) continue;
			const Craft* T = Find(S.Target);
			if (!T || !Live(*T) || T->Doing == Role::Ground) continue;
			if (Box < 0 && S.Group == E.By && S.bFiring && !S.bScored && T->Group == E.Unit && (E.Strag >= 0 ? T->StragId == E.Strag : T->StragId < 0)) return S.Key;
			if (Box >= 0 && S.Group == E.Unit && T->Group == Box && Len(T->P - S.P) < 450.0) return S.Key;
		}
		// (or one of them that group's guns were on within the last five seconds: hit, it goes down now. No shooter to
		// name: 0)
		if (Box < 0)
		{
			for (const auto& Pair : Crafts)
			{
				const Craft& V = Pair.second;
				if (V.Group == E.Unit && Live(V) && V.Doing != Role::Ground && V.ShotBy == E.By && AirT - V.ShotAt < (V.bFighter ? 5.0 : 10.0) && (E.Strag >= 0 ? V.StragId == E.Strag : V.StragId < 0)) return 0;
			}
		}
		return -1;
	}

	bool Show::Inbound(const Event& E) const
	{
		// A fighter of the group that shot it down going in on one of the lost aircraft's group, close and nose on:
		// his guns are a few seconds from bearing.
		for (const auto& Pair : Crafts)
		{
			const Craft& S = Pair.second;
			if (!S.bFighter || S.Group != E.By || S.Target < 0 || !Live(S) || S.Doing != Role::Fight || S.bScored) continue;
			if (S.Step != Phase::Run && S.Step != Phase::Chase && S.Step != Phase::Turn) continue;
			const Craft* T = Find(S.Target);
			if (!T || !Live(*T) || T->Group != E.Unit || (E.Strag >= 0 ? T->StragId != E.Strag : T->StragId >= 0)) continue;
			const Vec Rel = T->P - S.P;
			const double D = Len(Rel);
			if (D < 1400.0 && Ahead(S.Hdg, S.Gam).Dot(Rel) > 0.9 * D) return true;
		}
		return false;
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
		// The one the guns of the group that brought it down are on now; else the one they were last on, within 25 s
		// (hit on the last pass, it goes down now); else one a fighter of that group is closing on.
		if (E.By >= 0)
		{
			int Best = -1;
			double BestScore = 1e8;
			for (int K : Keys)
			{
				const Craft& V = *Find(K);
				if (V.Doing == Role::Ground || V.StragId >= 0) continue;
				double Score = 1e9;
				if (V.ShotBy == E.By && AirT - V.ShotAt < 25.0) Score = AirT - V.ShotAt;
				for (const auto& Pair : Crafts)
				{
					const Craft& Shooter = Pair.second;
					if (Shooter.Group != E.By || Shooter.Target != K || !Live(Shooter)) continue;
					if (Shooter.bFiring) Score = std::min(Score, -1.0);
					else if (Shooter.Step == Phase::Run || Shooter.Step == Phase::Break || Shooter.Step == Phase::Turn || Shooter.Step == Phase::Chase) Score = std::min(Score, 30.0 + Len(Shooter.P - V.P) / 40.0);
				}
				if (Score < BestScore) { BestScore = Score; Best = K; }
			}
			if (Best >= 0) return Best;
		}
		// A fighter brought down by a box's gunners: the one of its group closest in on that box.
		{
			auto By = Now.find(E.By);
			const int Box = E.GunBox >= 0 ? E.GunBox : (By != Now.end() && !By->second.bFighter ? E.By : -1);
			int Best = -1;
			double Near = 1500.0;
			for (int K : Keys)
			{
				const Craft& V = *Find(K);
				if (Box < 0 || !V.bFighter || V.Doing != Role::Fight || V.TargetGroup != Box || V.Target < 0) continue;
				const Craft* T = Find(V.Target);
				if (!T || T->Group != Box) continue;
				const double D = Len(T->P - V.P);
				if (D < Near) { Near = D; Best = K; }
			}
			if (Best >= 0) return Best;
		}
		// A fighter already hit and out of the fight; any in a fight; then any. A straggler the battle still counts is
		// kept for the end it gives it (its fire put out, its limp home, its own loss), unless no other is left.
		for (int K : Keys) if (Find(K)->bHurt && Find(K)->Doing == Role::Fight) return K;
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
		// (among fighters in a fight, the one an enemy's guns were on within the last 6 s shows the hit)
		for (size_t K = 0; K < Flying.size(); ++K)
		{
			const Craft& O = *Find(Flying[K]);
			if (!O.bFighter || O.Doing != Role::Fight || AirT - O.ShotAt > 6.0 || (bLights && O.TrailKey)) continue;
			if (AirT - Find(Flying[Pick])->ShotAt > 6.0 || O.ShotAt > Find(Flying[Pick])->ShotAt) Pick = K;
		}
		Craft& C = *Find(Flying[Pick]);
		Check.HitsShown++;
		// A fighter hit hard in a fight is out of it, his wingman seeing him home (the pilots' section): no more than a
		// quarter of a group out at once.
		if (C.bFighter && C.Doing == Role::Fight && !C.bHurt && (E.Mark == "smoke" || E.Mark == "pieces" || E.Mark == "fireout"))
		{
			int Gone = 0;
			for (int Key : Flying) Gone += Find(Key)->Step == Phase::Away ? 1 : 0;
			if (Gone + 2 <= std::max(2, static_cast<int>(Flying.size()) / 4))
			{
				C.bHurt = true;
				Check.Hurt++;
			}
		}
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

	int Show::BombsOf(const std::string& Model, double Span, bool bFighter)
	{
		if (bFighter) return 1;
		// (a usual war load, Brian 21 November 05:45Z: "a realistic number of bombs"; the modeller's table)
		std::string M;
		for (const char Ch : Model) if (std::isalnum(static_cast<unsigned char>(Ch))) M.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(Ch))));
		static const std::pair<const char*, int> Loads[] = {
			{ "b17", 12 }, { "lancaster", 14 }, { "he111", 8 }, { "b29", 20 }, { "betty", 4 }, { "g4m", 4 }, { "sm79", 5 },
			{ "avenger", 4 }, { "tbf", 4 }, { "sturmovik", 4 }, { "il2", 4 }, { "nell", 2 }, { "g3m", 2 }, { "kate", 1 },
			{ "b5n", 1 }, { "dauntless", 1 }, { "sbd", 1 }, { "val", 1 }, { "d3a", 1 }, { "stuka", 1 }, { "ju87", 1 },
		};
		for (const auto& Load : Loads) if (M == Load.first) return Load.second;
		return Span > 25.0 ? 10 : Span > 18.0 ? 6 : 1;
	}

	void Show::Drop(Craft& C, bool bTorpedo)
	{
		if (C.bDropped) return;
		C.bDropped = true;
		// A stick: its bombs leave the bay one after another, StickGap apart (Bays lets each go), once the doors are
		// open; a dive bomber's one bomb, a fighter's and a torpedo slung under her go at once.
		C.StickSize = C.StickLeft = bTorpedo ? 1 : BombsOf(C.Model, C.Form.Span, C.bFighter);
		C.bStickTorpedo = bTorpedo;
		const auto G = Now.find(C.Group);
		C.bStickBay = !C.bFighter && !bTorpedo && C.Doing != Role::Dive && C.StickSize > 1 && (G == Now.end() || G->second.Strike != "dive");
		C.StickNext = C.bStickBay && C.Bay < 1.0 ? AirT + (1.0 - C.Bay) * BayOpenS + 0.1 : AirT;
		Check.Sticks++;
		Bays(0.0);
	}

	void Show::Bays(double Dt)
	{
		for (const auto& [Id, G] : Now)
		{
			if (G.bFighter || G.Side != 0) continue;
			double& In = RingT[Id];
			In = std::hypot(G.X - TargetX, G.Y - TargetY) <= TargetR ? In + Dt : 0.0;
		}
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			if (C.bFighter) continue;
			// (a craft shot down with bombs aboard takes them with it)
			const bool bAlive = C.Doing != Role::Fall && C.Doing != Role::Gone && C.Doing != Role::Ground;
			if (!bAlive) { C.StickLeft = 0; continue; }
			// The doors: open from BayLead seconds before the run is done, through the stick and 3 s after it; then shut
			// (each over BayOpenS and BayShutS, eased in the picture). A dive bomber has none.
			const auto G = Now.find(C.Group);
			// (only a bomber with a bay: a stick of two or more, not a torpedo or a dive bomber's bomb slung under her)
			const bool bLevel = G != Now.end() && G->second.Strike != "dive" && G->second.Strike != "torpedo" && C.Doing != Role::Dive
			                    && BombsOf(C.Model, C.Form.Span, false) > 1;
			const double Lead = std::max(0.0, std::min(1.0, BayLead / 45.0));
			const auto Ring = RingT.find(C.Group);
			const double Run = G == Now.end() ? 0.0 : G->second.Run > 0.0 ? G->second.Run : Ring == RingT.end() ? 0.0 : std::min(1.0, Ring->second / 45.0);
			const bool bOnRun = bLevel && !C.bDropped && Run >= 1.0 - Lead && Run > 0.0;
			const bool bWant = (bLevel && bOnRun) || (C.bStickBay && (C.StickLeft > 0 || (C.StickDone >= 0.0 && AirT < C.StickDone + 3.0)));
			if (Dt > 0.0) C.Bay = bWant ? std::min(1.0, C.Bay + Dt / std::max(0.1, BayOpenS)) : std::max(0.0, C.Bay - Dt / std::max(0.1, BayShutS));
			if (!bWant && C.Bay > 0.0 && C.StickDone >= 0.0 && AirT > C.StickDone + 8.0) { Check.BaysLeftOpen++; C.Bay = 0.0; }
			// The bombs due out of the bay: each from its station under her, moving as she moves; one due a while ago (a
			// frame at speed) is where it would be by now.
			const Vec V = Velocity(C);
			while (C.StickLeft > 0 && AirT >= C.StickNext)
			{
				const int K = C.StickSize - C.StickLeft;
				const double Ago = std::max(0.0, AirT - C.StickNext);
				Thing B;
				B.Kind = ThingKind::Bomb;
				B.bTorpedo = C.bStickTorpedo;
				B.Lead = K == 0 ? C.StickSize : 0;
				B.Key = C.Key;
				B.At = Local(C, Vec(0.0, ((K % 2) ? 0.25 : -0.25), -1.2)) - V * Ago + Vec(0.0, 0.0, -0.5 * Gee * Ago * Ago);
				B.Vel = V + Vec(0.0, 0.0, -Gee * Ago);
				B.Size = C.bStickTorpedo ? 5.5 : C.Form.Span > 25.0 ? 2.2 : 1.8;
				B.Heading = C.Hdg;
				Small.push_back(B);
				Check.StickBombs++;
				if (K == 0 && C.bStickBay) C.StickBegan = AirT;
				if (C.bStickBay && C.Bay < 0.9) Check.BombsThroughShutDoors++;
				C.StickLeft--;
				C.StickNext += StickGap;
				if (C.StickLeft == 0) C.StickDone = AirT;
			}
		}
	}

	void Show::Play(const Event& E)
	{
		const bool bShips = E.What == "air:ship-hit" || E.What == "air:ship-sunk";
		if (bShips)
		{
			// Held until the bombs or the torpedoes could have reached her: the stage shows the hit as the bombs land.
			const double Wait = E.Strike == "level" ? 26.0 : E.Strike == "dive" ? 30.0 : E.Strike == "torpedo" ? 14.0 : 20.0;
			Later.push_back(Held{ E, AirT + Wait, false });
			return;
		}
		if (E.What == "end") bEnded = true;
		// An aircraft lost to fighters (or a fighter to a box's gunners) falls when the guns that brought it down bear:
		// held up to 16 s for a fighter of that group to fire on one of its group, so that what falls is what was shot
		// at. The battle's count is the same; only the moment its model falls is the scene's. A straggler's loss waits
		// for the guns to bear on that straggler; and nothing is held once the battle's end is told.
		const int Shooter = E.What == "air:lost" && !bEnded && (E.By >= 0 || E.GunBox >= 0) && Now.count(E.Unit) ? UnderGuns(E) : 0;
		// (0: nothing to hold it for, or one of them hit a moment ago)
		if (Shooter < 0)
		{
			// (longer as a fight opens, while the first of them are still coming in)
			auto Opening = [this](int Id)
			{
				auto It = Bands.find(Id);
				return It == Bands.end() || It->second.FightAt < 0.0 || AirT - It->second.FightAt < 25.0;
			};
			// (and a heavy bomber hit hard took its time to fall out of its box)
			const auto Lost = Now.find(E.Unit);
			const bool bHeavy = Lost != Now.end() && !Lost->second.bFighter;
			Later.push_back(Held{ E, AirT + (bHeavy ? 24.0 : Opening(E.By) || Opening(E.Unit) ? 22.0 : 20.0), true });
			Bands[E.Unit].Pending++;
			return;
		}
		if (Shooter > 0) Scored(Shooter, E);
		Release(E);
	}

	void Show::Scored(int Shooter, const Event& E)
	{
		// (one burst, one aircraft: the next loss the battle has counted waits for the next guns to bear)
		Craft* S = Find(Shooter);
		if (S && S->Group == E.By) S->bScored = true;
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
				auto By = Now.find(E.By);
				if (By != Now.end() && By->second.bFighter)
				{
					Check.FallsByFighters++;
					if (Find(Key)->ShotBy == E.By && AirT - Find(Key)->ShotAt < 25.0) Check.FallsUnderFire++;
				}
				StartFall(*Find(Key), E);
				Bands[E.Unit].Owed.push_back(AirT);
			}
			return;
		}
		// (a drop's own cue, the rules' bomb_stick_01, is not played at the drop: the stick is heard where and when it
		// lands, from its first bomb's BombHit)
		if (!E.Cue.empty() && E.What != "air:bombs" && E.What != "air:jettison") Emit(FxKind::Sound, At, E.Cue);
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
				// He follows it while it is in his arc, his gun swinging only as fast as a man or a turret lays one
				// (the art director's D6, 17:27Z on the Bremen film: one gun, one stream), and fires in bursts while
				// he is on it.
				Vec Aim;
				if (bInArc && LayGun(C, Station, *Foe, Near, Row, C.Key * 64 + Index, Aim) && std::sin(AirT * 1.7 + C.Key * 0.37 + Index) > -0.25)
				{
					const int Of = std::max(1, static_cast<int>(Row.At.size()));
					for (int K = 0; K < Of; ++K)
					{
						// A twin mount is one opening: its two guns side by side in it, laid together.
						const Vec Here = Local(C, Station + Vec(0.0, Of > 1 ? (K == 0 ? -0.15 : 0.15) : 0.0, 0.0));
						Want W;
						W.F.Kind = FxKind::Gun;
						W.F.Key = C.Key * 64 + Index + K;
						W.F.At = Here;
						W.F.Aim = Aim;
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
			// A bomb's last moment: its rush heard near where it will land (the mix's impacts.incoming.bomb: the take
			// bomb_release, 1.76 s, ending at the landing, within 400 m), one at a time.
			if (T.Kind == ThingKind::Bomb && !T.bTorpedo && !T.bRushed && T.At.Z > Floor)
			{
				const double H = T.At.Z - Floor;
				const double Fall = (T.Vel.Z + std::sqrt(T.Vel.Z * T.Vel.Z + 2.0 * Gee * H)) / Gee;
				if (Fall <= 1.76)
				{
					T.bRushed = true;
					const Vec Land(T.At.X + T.Vel.X * Fall, T.At.Y + T.Vel.Y * Fall, GroundAt(T.At.X + T.Vel.X * Fall, T.At.Y + T.Vel.Y * Fall));
					if (Len(Land - Eye) <= 400.0 && AirT - RushAt >= 0.7)
					{
						RushAt = AirT;
						Emit(FxKind::Sound, Land, "bomb_release");
						Check.Rushes++;
					}
				}
			}
			const Vec Down(T.At.X, T.At.Y, Floor);
			if (T.Kind == ThingKind::Bomb)
			{
				// (a torpedo let go over the land, short of the water, goes into it with a puff of earth, not a column of
				// water: the pre-check on round 16 found three splashes on Oahu's land)
				if (T.bTorpedo) Emit(Floor > 0.5 ? FxKind::Crash : FxKind::Splash, Down, Floor > 0.5 ? "piece" : "torpedo").Size = Floor > 0.5 ? 2.0 : 4.0;
				else
				{
					Fx& Hit = Emit(FxKind::BombHit, Down, Floor > 0.5 ? "land" : "water");
					Hit.Size = T.Size > 2.0 ? 500.0 : 250.0;
					Hit.Text = T.Lead > 1 ? "stick" : T.Lead == 1 ? "one" : "";
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
			const int Shooter = Later[K].bForShot && !bEnded ? UnderGuns(Later[K].E) : -1;
			// (its time up with a fighter going in on one of them: a few seconds more, once, for his guns to bear)
			if (Later[K].bForShot && !bEnded && Shooter < 0 && Later[K].At <= AirT && !Later[K].bMore && Inbound(Later[K].E))
			{
				Later[K].bMore = true;
				Later[K].At = AirT + 7.0;
			}
			const bool bNow = Later[K].At <= AirT || (Later[K].bForShot && (bEnded || Shooter >= 0));
			if (bNow)
			{
				if (Shooter >= 0) Scored(Shooter, Later[K].E);
				const Held Done = Later[K];
				Later.erase(Later.begin() + static_cast<long>(K));
				if (Done.bForShot)
				{
					Band& B = Bands[Done.E.Unit];
					B.Pending = std::max(0, B.Pending - 1);
					Check.FallsHeld++;
				}
				Release(Done.E);
			}
			else ++K;
		}
		for (auto& Pair : Now) Reconcile(Pair.second, Bands[Pair.first]);
		// Groups the battle no longer shows: whatever is left of them goes home.
		for (auto& Pair : Crafts) if (!Now.count(Pair.second.Group)) Leave(Pair.second);
		// The fighter pilots' brief for this frame.
		Brief();

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
			Bays(AirDt);
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
			P.Task = C.bFighter ? C.Task : "";
			P.Leader = -1;
			if (C.bFighter && C.Lead != 0 && C.Lead != C.Key)
			{
				// (his leader while that one still flies with the group)
				const Craft* L = Find(C.Lead);
				if (L && L->Group == C.Group && Live(*L)) P.Leader = C.Lead;
			}
			P.Speed = C.Spd;
			P.DeadEngines = C.DeadEngines;
			P.bFeathered = C.bFeathered;
			P.Soot = C.SootAt >= 0.0 ? std::max(0.0, std::min(1.0, (AirT - C.SootAt) / 30.0)) : 0.0;
			P.Bay = C.Bay * C.Bay * (3.0 - 2.0 * C.Bay); // (eased at both ends)
			P.bBombsGone = C.StickDone >= 0.0;
			if (C.StickBegan >= 0.0 && StickLift > 0.0)
			{
				// (her load gone, she lifts a little and is trimmed down again)
				const double Since = AirT - C.StickBegan;
				const auto Ease = [](double X) { X = std::max(0.0, std::min(1.0, X)); return X * X * (3.0 - 2.0 * X); };
				P.At.Z += StickLift * (Since < 1.2 ? Ease(Since / 1.2) : 1.0 - Ease((Since - 1.2) / 6.0));
			}
			Out.push_back(P);
		}
	}

	std::string Show::Describe() const
	{
		// One line an aircraft: what it is doing (for a debug overlay, and the cloud test's trace).
		static const char* Roles[] = { "ground", "slot", "fight", "straggle", "cover", "fall", "home", "dive", "gone" };
		static const char* Steps[] = { "-", "setup", "run", "break", "climb", "turn", "peel", "plunge", "pull", "egress", "hold", "zoom", "evade", "station", "chase", "away", "wing", "scissor" };
		std::string Text;
		char Line[256];
		for (const auto& Pair : Crafts)
		{
			const Craft& C = Pair.second;
			if (C.Doing == Role::Gone) continue;
			const Craft* T = C.Target >= 0 ? Find(C.Target) : nullptr;
			const Craft* L = C.Lead != 0 && C.Lead != C.Key ? Find(C.Lead) : nullptr;
			// (the target's range and how far off his nose it is, in degrees; a wingman's distance from his leader)
			const double Off = T ? std::acos(Clamp(Ahead(C.Hdg, C.Gam).Dot(Norm(T->P - C.P)), -1.0, 1.0)) / Deg : -1.0;
			std::snprintf(Line, sizeof Line, "%d g%d %s %s/%s t%.0f q%d tgt%d d%.0f off%.0f v%.0f z%.0f fire%d lead%d(%.0f) four%d.%d%s%s%s [%s]\n", C.Key, C.Group, C.bFighter ? "F" : "B",
			              Roles[static_cast<int>(C.Doing)], Steps[static_cast<int>(C.Step)], C.StepT, C.TargetGroup, C.Target,
			              T ? Len(T->P - C.P) : -1.0, Off, C.Spd, C.P.Z, C.bFiring ? 1 : 0, C.Lead, L ? Len(L->P - C.P) : 0.0, C.Flight, C.Seat,
			              C.bGuard ? " guard" : "", C.bHurt ? " hurt" : "", C.bDry ? " dry" : "", C.Task);
			Text += Line;
		}
		return Text;
	}
}
