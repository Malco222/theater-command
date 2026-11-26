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
		// (the pull a fighter's floor guard plans his pull-out at, low down: four g, wings level)
		constexpr double PullOutG = 4.0;
		constexpr double Deg = Pi / 180.0;

		double Wrap(double A)
		{
			// A huge or endless angle never comes down a turn at a time (the pre-check's guard from the sea show, 805a95c0).
			if (!(std::fabs(A) <= 1.0e3)) A = std::isfinite(A) ? std::remainder(A, 2.0 * Pi) : 0.0;
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
			S.Body = High;
		};
		if (Model == "B17") { S.Span = 31.6; S.Long = 22.7; Engines({ -9.2, -4.9, 4.9, 9.2 }, 1.6, 0.2); Bomber(22.7, 2.2); }
		else if (Model == "Lancaster") { S.Span = 31.1; S.Long = 21.2; Engines({ -8.9, -4.2, 4.2, 8.9 }, 1.8, 0.3); Bomber(21.2, 2.1); }
		else if (Model == "He111") { S.Span = 22.6; S.Long = 16.4; Engines({ -3.4, 3.4 }, 1.6, 0.0); Bomber(16.4, 1.7); }
		// (the land bombers the modeller built, their engines where her files' sockets say fire and smoke start, about the
		// middle of her box as the scene draws her: the Betty, the Nell, and the SM.79's three, the nose her second)
		else if (Model == "G4M" || Model == "Betty") { S.Span = 25.0; S.Long = 20.0; Engines({ -3.3, 3.3 }, 5.2, -1.2); Bomber(20.0, 1.8); }
		else if (Model == "G3M" || Model == "Nell") { S.Span = 25.0; S.Long = 16.5; Engines({ -3.0, 3.0 }, 4.4, -0.3); Bomber(16.5, 1.6); }
		// (the modeller's twins of 23 November, the same way: the Wellington, the Pe-2 and the LeO 451)
		else if (Model == "Wellington") { S.Span = 26.3; S.Long = 19.7; Engines({ -2.9, 2.9 }, 5.9, -0.7); Bomber(19.7, 1.9); }
		else if (Model == "Pe2" || Model == "Pe-2") { S.Span = 17.2; S.Long = 12.7; Engines({ -2.3, 2.3 }, 4.3, -0.3); Bomber(12.7, 1.4); }
		else if (Model == "LeO451" || Model == "LeO 451") { S.Span = 22.5; S.Long = 17.2; Engines({ -2.75, 2.75 }, 6.0, -0.7); Bomber(17.2, 1.6); }
		else if (Model == "SM79" || Model == "SM.79") { S.Span = 21.2; S.Long = 16.2; S.Engines = { Vec(5.5, -2.6, -0.7), Vec(7.1, 0.0, -0.15), Vec(5.5, 2.6, -0.7) }; Bomber(16.2, 1.8); }
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
			const bool bD520 = Model == "D520" || Model == "D.520";
			S.Span = Model == "Zero" ? 12.0 : Model == "Warhawk" ? 11.4 : Model == "Spitfire" ? 11.2 : Model == "Hurricane" ? 12.2 : Model == "Mustang" ? 11.3 : Model == "Wildcat" ? 11.6 : Model == "Bf109" ? 9.9 : bD520 ? 10.2 : 10.6;
			S.Long = Model == "Bf109" ? 8.6 : Model == "Zero" ? 9.1 : bD520 ? 8.8 : 9.4;
			Engines({ 0.0 }, S.Long * 0.42, 0.0);
		}
		if (!S.Stations.count("cockpit")) S.Stations["cockpit"] = Vec(bFighter ? -0.5 : S.Long * 0.35, 0.0, 1.0);
		return S;
	}

	Show::Show(uint64_t Seed) : Roll(Seed), Shots(Seed * 2654435761ull + 2111ull)
	{
		// (the effects kit's air_guns rows, 21 November)
		Rounds["rifle"] = Flight{ 0.5, 9.81, 1.2 };
		Rounds["hmg"] = Flight{ 0.36, 9.81, 1.2 };
		Rounds["cannon20"] = Flight{ 0.38, 9.81, 1.02 };
	}

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
		MountsKept.clear();
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
		// (the group's own, by key as Crafts has them; sorted by slot on the slots read once, the same comparisons)
		std::vector<std::pair<int, int>> Slots;
		const auto M = Members.find(GroupId);
		if (M != Members.end())
		{
			Slots.reserve(M->second.size());
			for (int Key : M->second)
			{
				const Craft& C = Crafts.find(Key)->second;
				if (Live(C)) Slots.emplace_back(C.Slot, Key);
			}
		}
		std::sort(Slots.begin(), Slots.end(), [](const std::pair<int, int>& A, const std::pair<int, int>& B) { return A.first < B.first; });
		std::vector<int> Keys;
		Keys.reserve(Slots.size());
		for (const auto& S : Slots) Keys.push_back(S.second);
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

	double Show::Lifted(const Craft& C) const
	{
		// (her load gone, she lifts a little and is trimmed down again: so she is drawn, and so her guns and her strikes
		// are placed; LiftAt has the one reckoning of it)
		return LiftAt(C, AirT);
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

	void Show::Take(double T, const std::vector<Group>& InGroups, const std::vector<Event>& InEvents, const std::vector<Ship>& InShips,
		const AirMarks::View* InMarks)
	{
		View V;
		V.T = T;
		for (const Group& G : InGroups) V.Groups[G.Id] = G;
		if (!Views.empty() && T <= Views.back().T) Views.back() = V; // (the same moment again: the newer word stands)
		else Views.push_back(V);
		// (smooth flying: the views of the last two PathSpan are kept as well, the formations' velocities being read
		// from them, GroupVelocity)
		const double Keep = bSmooth ? 2.0 * PathSpan : 0.0;
		while (Views.size() > 12 && Views[1].T <= ShownT - Keep) Views.pop_front();
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
		// (a raid on troops: the view's word on the pieces, kept for its time; AirShowTroops.cpp)
		if (InMarks && InMarks->bGiven && Column.Has()) TakeMarks(T, *InMarks);
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
					G.CountAlt = Lerp(G.CountAlt, Next->second.CountAlt, F);
					G.Face = Wrap(G.Face + Wrap(Next->second.Face - G.Face) * F);
					// (smooth flying: a straggler's place behind her box as well, or it steps at every view; the bombers'
					// part, SetBoxKeep)
					if (bSmooth && bBoxKeep)
					{
						for (Strag& S : G.Strags)
						{
							for (const Strag& N : Next->second.Strags)
							{
								if (N.Id == S.Id)
								{
									S.Back = Lerp(S.Back, N.Back, F);
									S.Down = Lerp(S.Down, N.Down, F);
								}
							}
						}
					}
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
		Heights();
	}

	void Show::TrueBand(const Group& G, double& In, double& From)
	{
		In = From = 0.0;
		if (G.bFighter) return;
		if (G.Strike == "level" && G.Model == "B5N") In = 3000.0;
		else if (G.Strike == "dive" && G.Model == "D3A") In = From = 4000.0;
		else if (G.Strike == "dive" && G.Model == "SBD") { In = 5600.0; From = 4500.0; }
	}

	double Show::TorpedoSpeed(const std::string& Model)
	{
		if (Model == "TBD") return 51.0;
		if (Model == "Swordfish") return 41.0;
		if (Model == "TBF" || Model == "Barracuda") return 62.0;
		return 72.0;
	}

	const Ship* Show::NearestShip(double X, double Y) const
	{
		const Ship* Best = nullptr;
		double Least = 1e18;
		for (const Ship& S : ShipsNow)
		{
			const double D = std::hypot(S.At.X - X, S.At.Y - Y);
			if (D < Least) { Least = D; Best = &S; }
		}
		return Best;
	}

	void Show::Heights()
	{
		// The torpedo planes come down well before the target (the audit's 4.5; the record: Midway's torpedo squadron at
		// 1,500 feet, down to 150 when the Zeros came, the drop from about a house's height). Each torpedo group's
		// height drawn is brought down by its distance from the target: from its band at 45 km to about 450 m at 25 km
		// (and to 450 m at once, as fast as it lets down, when its fight begins), 150 m at 10 km and wave-top from 3 km;
		// over the ground (Oahu) the same over the highest ground of the next two kilometres. After its drop it flies
		// home low: TorpedoAwayM for a minute, then no higher than 150 m. Never a climb back to the band. The battle's
		// own place and time are kept: only the height drawn is the show's.
		const double Dt = DrawnAt < 0.0 ? 0.0 : std::max(0.0, AirT - DrawnAt);
		DrawnAt = AirT;
		// The other bombers at their kind's true height (TrueBand), let down at up to 10 m a second; a dive group low after
		// its dive, as the torpedo planes after their drop; an escort kept as high over its bombers as the battle has it.
		std::map<int, double> Lowered;
		for (auto& Pair : Now)
		{
			Group& G = Pair.second;
			if (G.bFighter || G.Strike == "torpedo") continue;
			double In = 0.0, From = 0.0;
			TrueBand(G, In, From);
			const auto B = Bands.find(G.Id);
			const bool bDropped = B != Bands.end() && B->second.bDropped;
			const double Floor = std::max(0.0, GroundAt(G.X, G.Y));
			double Want = -1.0;
			if (G.Strike == "dive" && bDropped)
			{
				if (!DroppedAt.count(G.Id)) DroppedAt[G.Id] = AirT;
				Want = Floor + TorpedoAwayM + (150.0 - TorpedoAwayM) * Clamp((AirT - DroppedAt[G.Id] - 60.0) / 120.0, 0.0, 1.0);
			}
			else if (In > 0.0)
			{
				const double D = std::hypot(G.X - TargetX, G.Y - TargetY);
				Want = From > 0.0 ? From + (In - From) * Clamp((D - 6000.0) / 9000.0, 0.0, 1.0) : In;
			}
			if (Want < 0.0) continue;
			auto It = DrawnOver.find(G.Id);
			if (It == DrawnOver.end()) It = DrawnOver.emplace(G.Id, G.Alt).first;
			// (after its dive, at once: its bombers are already low over the water, and come back to it there)
			if (G.Strike == "dive" && bDropped) It->second = Want;
			else It->second += Clamp(Want - It->second, -10.0 * Dt, 3.0 * Dt);
			if (!bDropped) Lowered[G.Id] = It->second - G.Alt;
			G.Alt = It->second;
		}
		for (auto& Pair : Now)
		{
			Group& G = Pair.second;
			if (!G.bFighter || G.With < 0) continue;
			const auto L = Lowered.find(G.With);
			if (L != Lowered.end()) G.Alt = std::max(GroundAt(G.X, G.Y) + 150.0, G.Alt + L->second);
		}
		for (auto& Pair : Now)
		{
			Group& G = Pair.second;
			if (G.bFighter || G.Strike != "torpedo") continue;
			const auto B = Bands.find(G.Id);
			const bool bDropped = B != Bands.end() && B->second.bDropped;
			if (G.Doing == Act::Fight || G.Engaged >= 0) Fought.insert(G.Id);
			const double D = std::hypot(G.X - TargetX, G.Y - TargetY);
			const Vec F = Ahead(G.Face);
			double Floor = std::max(0.0, GroundAt(G.X, G.Y));
			for (double Far : { 700.0, 1400.0, 2100.0 }) Floor = std::max(Floor, GroundAt(G.X + F.X * Far, G.Y + F.Y * Far));
			const double Height = std::max(TorpedoLowM, G.Alt - Floor);
			double Want = Height;
			if (!bDropped)
			{
				if (D <= 3000.0) Want = TorpedoLowM;
				else if (D <= 10000.0) Want = TorpedoLowM + (150.0 - TorpedoLowM) * (D - 3000.0) / 7000.0;
				else if (D <= 25000.0) Want = 150.0 + 300.0 * (D - 10000.0) / 15000.0;
				else if (D <= 45000.0) Want = 450.0 + (Height - 450.0) * (D - 25000.0) / 20000.0;
				if (Fought.count(G.Id)) Want = std::min(Want, 450.0);
				Want = std::min(Want, Height);
			}
			else
			{
				if (!DroppedAt.count(G.Id)) DroppedAt[G.Id] = AirT;
				Want = TorpedoAwayM + (150.0 - TorpedoAwayM) * Clamp((AirT - DroppedAt[G.Id] - 60.0) / 120.0, 0.0, 1.0);
			}
			G.bAbreast = !bDropped && D < 25000.0;
			auto It = DrawnOver.find(G.Id);
			if (It == DrawnOver.end()) It = DrawnOver.emplace(G.Id, Height).first;
			// (let down at up to 20 m a second, a shallow glide at its speed; up again, after the drop, at 3)
			It->second += Clamp(Want - It->second, -20.0 * Dt, 3.0 * Dt);
			G.Alt = Floor + It->second;
		}
	}

	bool Show::PathAt(int GroupId, double T, Vec& OutAt, double& OutT) const
	{
		// A group's middle at a battle time, along the straight lines between the views kept that have it; a time
		// before the first of them or after the last is brought to that view (OutT: the time it is given for).
		const Group* Ga = nullptr;
		const Group* Gb = nullptr;
		double Ta = 0.0, Tb = 0.0;
		for (const View& V : Views)
		{
			const auto It = V.Groups.find(GroupId);
			if (It == V.Groups.end()) continue;
			if (V.T <= T || !Ga)
			{
				Ga = &It->second;
				Ta = V.T;
				if (V.T <= T) continue;
			}
			Gb = &It->second;
			Tb = V.T;
			break;
		}
		if (!Ga) return false;
		if (!Gb || Tb <= Ta || T <= Ta)
		{
			OutAt = Vec(Ga->X, Ga->Y, Ga->Alt);
			OutT = Ta;
			return true;
		}
		const double F = Clamp((T - Ta) / (Tb - Ta), 0.0, 1.0);
		OutAt = Vec(Lerp(Ga->X, Gb->X, F), Lerp(Ga->Y, Gb->Y, F), Lerp(Ga->Alt, Gb->Alt, F));
		OutT = Ta + (Tb - Ta) * F;
		return true;
	}

	Vec Show::GroupVelocity(int GroupId) const
	{
		if (bSmooth)
		{
			// (SetSmooth) Read off the one hop between two views, as it was, a live raid's formation changed direction by a
			// third of a degree at every rules step (10 cm of rounding in the 30 m it had come), and every pilot in it
			// banked after that. Now: its place at three times PathSpan apart, the last of them PathLead ahead of now as
			// far as the views in hand reach (Advance), and the velocity now of the parabola through the three. A steady
			// turn or a steady change of speed is read as it is now, however far ahead the views reach; what is left of a
			// view's rounding is spread over a battle second.
			const double To = ShownT + PathLead;
			Vec P0, P1, P2;
			double T0 = 0.0, T1 = 0.0, T2 = 0.0;
			if (PathAt(GroupId, To - 2.0 * PathSpan, P0, T0) && PathAt(GroupId, To - PathSpan, P1, T1) && PathAt(GroupId, To, P2, T2) && T2 - T0 > 0.5 * PathSpan)
			{
				const double Per = 1.0 / Scale; // (battle seconds to air seconds)
				if (T1 - T0 < 0.25 * PathSpan || T2 - T1 < 0.25 * PathSpan) return (P2 - P0) * (Per / (T2 - T0));
				const double T = Clamp(ShownT, T0, T2);
				const double W0 = ((T - T1) + (T - T2)) / ((T0 - T1) * (T0 - T2));
				const double W1 = ((T - T0) + (T - T2)) / ((T1 - T0) * (T1 - T2));
				const double W2 = ((T - T0) + (T - T1)) / ((T2 - T0) * (T2 - T1));
				return (P0 * W0 + P1 * W1 + P2 * W2) * Per;
			}
		}
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

	namespace
	{
		/** One way of a bomber's place in her box (ahead, right or up; PlaceWant): on toward Want no faster than Most
		 * metres an air second, its speed changing by no more than Pull a second, and slowing in time to come to rest on
		 * it. */
		void PlaceAxis(double& At, double& Vel, double Want, double Most, double Pull, double Dt)
		{
			const double Off = Want - At;
			const double Ask = std::copysign(std::min(Most, std::sqrt(2.0 * Pull * std::fabs(Off))), Off);
			Vel += Clamp(Ask - Vel, -Pull * Dt, Pull * Dt);
			At += Vel * Dt;
			if (std::fabs(Want - At) < 0.01 && std::fabs(Vel) <= Pull * Dt)
			{
				At = Want;
				Vel = 0.0;
			}
		}
	}

	Vec Show::PlaceWant(Craft& C, const Vec& Mid, double Face, const Vec& Want, bool bStrag, double AheadMost, double AheadPull, double AcrossMost, double AcrossPull, double Dt, Vec& ToVel)
	{
		// Her place in the box's own frame: ahead, right and up of its middle along its heading.
		const Vec F = Ahead(Face), R = RightOf(Face);
		const Vec Off = Want - Mid;
		const Vec W(Off.Dot(F), Off.Dot(R), Off.Z);
		// (a place she did not fly the step before: her slot's, as she comes back to it or closes up from behind, is
		// flown at once, as before; a straggler's, from where she is and at her own speed about her box, so that falling
		// out is never a step of where she is asked to be. Her speed about the box is taken in the box's own frame,
		// which turns with it (BoxTurnRate, no faster than BoxTurnMost): less the turn's own speed at where she is, and
		// no faster than her place goes. With the turn left in, a B-17 falling out 400 m behind her box's middle as it
		// turned 1.4 degrees a second had her place set going 10 m/s across, and it ran 800 m out to the wrong side for
		// two minutes and more)
		if (C.PlaceStep != PilotSteps - 1 || C.bPlaceStrag != bStrag)
		{
			if (bStrag)
			{
				const Vec Was = C.P - Mid;
				C.PlaceAt = Vec(Was.Dot(F), Was.Dot(R), Was.Z);
				const double Turn = Clamp(BoxTurnRate(C.Group), -BoxTurnMost * Deg, BoxTurnMost * Deg);
				const Vec Rel = Velocity(C) - ToVel - (R * C.PlaceAt.X - F * C.PlaceAt.Y) * Turn;
				C.PlaceVel = Vec(Clamp(Rel.Dot(F), -AheadMost, AheadMost), Clamp(Rel.Dot(R), -AcrossMost, AcrossMost), Clamp(Rel.Z, -SlotCloseMost, SlotCloseMost));
			}
			else
			{
				C.PlaceAt = W;
				C.PlaceVel = Vec();
			}
		}
		C.PlaceStep = PilotSteps;
		C.bPlaceStrag = bStrag;
		if (Dt > 0.0)
		{
			PlaceAxis(C.PlaceAt.X, C.PlaceVel.X, W.X, AheadMost, AheadPull, Dt);
			PlaceAxis(C.PlaceAt.Y, C.PlaceVel.Y, W.Y, AcrossMost, AcrossPull, Dt);
			PlaceAxis(C.PlaceAt.Z, C.PlaceVel.Z, W.Z, SlotCloseMost, SlotPull, Dt);
		}
		ToVel += F * C.PlaceVel.X + R * C.PlaceVel.Y + Up * C.PlaceVel.Z;
		return Mid + F * C.PlaceAt.X + R * C.PlaceAt.Y + Up * C.PlaceAt.Z;
	}

	Vec SlotOf(const Group& G, int Slot)
	{
		const Vec F = Ahead(G.Face), R = RightOf(G.Face);
		double Rt = 0.0, Bk = 0.0, Hi = 0.0, Lat = 100.0, Back = 100.0, Step = 30.0;
		// (the model's shape, made once a model: every aircraft asks for its place every step)
		thread_local std::map<std::pair<std::string, bool>, Shape> Made;
		auto Mi = Made.find(std::make_pair(G.Model, G.bFighter));
		if (Mi == Made.end()) Mi = Made.emplace(std::make_pair(G.Model, G.bFighter), ShapeOf(G.Model, G.bFighter)).first;
		const Shape& S = Mi->second;
		if (!G.bFighter)
		{
			// Bombers in vics of three, the vics stepped up and down: the lead, the high element to the right, the low
			// to the left, and the low-low behind (the combat box, the art director's "vics of three stepped up").
			static const double Vic[8][3] = { { 0, 0, 0 }, { 1, 0.9, 0.5 }, { -1, 0.9, -0.5 }, { 0, 1.8, -1.0 }, { 2, 1.8, 1.0 }, { -2, 1.8, -1.5 }, { 1, 2.7, -0.5 }, { -1, 2.7, -2.0 } };
			static const double Wing[3][3] = { { 0, 0, 0 }, { -0.42, 0.35, -0.12 }, { 0.42, 0.35, 0.12 } };
			const int V = (Slot / 3) % 8, J = Slot % 3;
			// (the box laid in its w and d however many vics it fills: four vics, 12 figures, take 1.42 lats either side
			// of the lead and 2.15 backs behind him, as the game's first recordings lay them, a little over w and d; a
			// full box of eight, 24 figures, the release 2 recordings' one figure for two aircraft, 2.42 and 3.05. Laid
			// at the four vics' lat and back, a box of 24 stood twice its w across, 1,790 m for 880, and could not turn
			// home together: the air thread, 22 November, on Bremen fought again)
			const int Fill = std::clamp(std::max(G.Figures0, G.Figures), 1, 24);
			double Wide = 0.0, Deep = 0.0;
			for (int K = 0; K < Fill; ++K)
			{
				Wide = std::max(Wide, std::fabs(Vic[K / 3][0] + Wing[K % 3][0]));
				Deep = std::max(Deep, Vic[K / 3][1] + Wing[K % 3][1]);
			}
			Lat = std::max(G.W * 0.42 * std::min(1.0, 1.42 / std::max(Wide, 1e-6)), S.Span * 3.6);
			Back = std::max(G.D * 0.5 * std::min(1.0, 2.15 / std::max(Deep, 1e-6)), S.Long * 3.0);
			Step = std::max(25.0, S.Span * 1.6);
			Rt = Vic[V][0] + Wing[J][0];
			Bk = Vic[V][1] + Wing[J][1] + (Slot / 24) * 3.0;
			Hi = Vic[V][2] + Wing[J][2];
			// A torpedo box goes in low abreast, every aircraft on its own line to the ships.
			// (from its let-down on its approach, 25 km out, until its torpedoes have gone: Show::Heights)
			if (G.Strike == "torpedo" && G.bAbreast)
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
			// Boxes of FighterBox (20: five fours, a squadron and its spares), in trail, each about three box-depths
			// behind the one before, as the bombers' boxes fly (AR2, the view's extra.boxes): a big group is a stream,
			// never one line abreast growing with it. Each box is laid out alike.
			constexpr int FighterBox = 20;
			const int Trail = Slot / FighterBox;
			Slot %= FighterBox;
			Bk = Trail * 4.5;
			if (G.Form == "line")
			{
				// Fours in line astern, the fours abreast.
				const int Four = Slot / 4, K = Slot % 4;
				Rt = (Four % 2 == 0 ? -1.0 : 1.0) * ((Four + 1) / 2) * 1.6;
				Bk += K * 0.45;
				Hi = -K * 0.1 + Four * 0.2;
			}
			else if (G.Form == "fours")
			{
				// Finger fours: the leader, his wingman back on one side, the element on the other; fours abreast.
				static const double Finger[4][3] = { { 0, 0, 0 }, { -0.45, 0.3, -0.05 }, { 0.65, 0.22, 0.08 }, { 1.1, 0.5, 0.12 } };
				const int Four = Slot / 4, K = Slot % 4;
				Rt = Finger[K][0] + (Four % 2 == 0 ? -1.0 : 1.0) * ((Four + 1) / 2) * 2.6;
				Bk += Finger[K][1] + Four * 0.25;
				Hi = Finger[K][2] + Four * 0.6;
			}
			else
			{
				// Vics of three (tight to 1940 for the RAF; the turning fighters' threes).
				static const double Three[3][3] = { { 0, 0, 0 }, { -0.4, 0.3, -0.05 }, { 0.4, 0.3, 0.05 } };
				const int V = Slot / 3, K = Slot % 3;
				const double Tight = G.Form == "vic" ? 0.6 : 1.0;
				Rt = (Three[K][0] + (V % 2 == 0 ? -1.0 : 1.0) * ((V + 1) / 2) * 1.5) * Tight;
				Bk += (Three[K][1] + V * 0.3) * Tight;
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
		Members[C.Group].push_back(C.Key);
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
			C.RollTime = 0.12;
		}
		else
		{
			C.VMin = 65.0; C.VMax = 110.0; C.VTop = 150.0; C.RMin = 500.0; C.GMax = 2.0; C.RollMax = 40.0 * Deg;
			if (C.Form.bDive) { C.GMax = 5.0; C.RMin = 300.0; C.RollMax = 90.0 * Deg; C.VTop = 165.0; }
			// (her roll's own time, by her size: longer for a twin and longer again for a four-engined heavy; kept short
			// enough that her pilot's turn after his box, Fly's, stays as steady as it was)
			C.RollTime = C.Form.Engines.size() >= 4 || C.Form.Span >= 28.0 ? 0.22 : C.Form.Engines.size() >= 2 || C.Form.Span >= 20.0 ? 0.18 : 0.15;
		}
	}

	namespace
	{
		/** The places for N bombers costing least in all (Cost[i][j]: bomber i to place j), one each: Kuhn and Munkres'
		 * assignment, with potentials (a few thousand steps for a box of 29). Each bomber's place. */
		std::vector<int> LeastCost(const std::vector<std::vector<double>>& Cost)
		{
			const int N = static_cast<int>(Cost.size());
			const double Inf = 1e300;
			std::vector<double> U(static_cast<size_t>(N) + 1, 0.0), V(static_cast<size_t>(N) + 1, 0.0);
			std::vector<int> Holds(static_cast<size_t>(N) + 1, 0), Way(static_cast<size_t>(N) + 1, 0);
			for (int I = 1; I <= N; ++I)
			{
				Holds[0] = I;
				int J0 = 0;
				std::vector<double> Least(static_cast<size_t>(N) + 1, Inf);
				std::vector<char> Used(static_cast<size_t>(N) + 1, 0);
				do
				{
					Used[static_cast<size_t>(J0)] = 1;
					const int I0 = Holds[static_cast<size_t>(J0)];
					double Delta = Inf;
					int J1 = 0;
					for (int J = 1; J <= N; ++J)
					{
						if (Used[static_cast<size_t>(J)]) continue;
						const double Cur = Cost[static_cast<size_t>(I0 - 1)][static_cast<size_t>(J - 1)] - U[static_cast<size_t>(I0)] - V[static_cast<size_t>(J)];
						if (Cur < Least[static_cast<size_t>(J)]) { Least[static_cast<size_t>(J)] = Cur; Way[static_cast<size_t>(J)] = J0; }
						if (Least[static_cast<size_t>(J)] < Delta) { Delta = Least[static_cast<size_t>(J)]; J1 = J; }
					}
					for (int J = 0; J <= N; ++J)
					{
						if (Used[static_cast<size_t>(J)])
						{
							U[static_cast<size_t>(Holds[static_cast<size_t>(J)])] += Delta;
							V[static_cast<size_t>(J)] -= Delta;
						}
						else Least[static_cast<size_t>(J)] -= Delta;
					}
					J0 = J1;
				} while (Holds[static_cast<size_t>(J0)] != 0);
				do
				{
					const int J1 = Way[static_cast<size_t>(J0)];
					Holds[static_cast<size_t>(J0)] = Holds[static_cast<size_t>(J1)];
					J0 = J1;
				} while (J0 != 0);
			}
			std::vector<int> Of(static_cast<size_t>(N), 0);
			for (int J = 1; J <= N; ++J) Of[static_cast<size_t>(Holds[static_cast<size_t>(J)] - 1)] = J - 1;
			return Of;
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
					Vec At = SlotPlace(G, K);
					At.Z = std::max(At.Z, GroundAt(At.X, At.Y) + TorpedoLowM);
					Craft& C = Make(G, At, G.Face, Spd, Role::Slot);
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
			// (a level box that has bombed flies on its lead until it has rallied: her place is laid in his frame, his
			// heading and way, never in the battle's own formation already turned for home kilometres off, RallyFrame)
			Vec LeadAt, LeadVel;
			double LeadFace = G.Face;
			const Craft* RallyLead = bAirborne && !G.bFighter && RallyFrame(G.Id, LeadAt, LeadFace, LeadVel) ? BoxLead(G.Id) : nullptr;
			for (int K = Have + Waiting; K < Want; ++K)
			{
				Group Frame = G;
				if (RallyLead) Frame.Face = LeadFace;
				const Vec F = Ahead(Frame.Face);
				Vec At = SlotPlace(Frame, K) - F * 250.0 - Up * 40.0;
				if (RallyLead) At += LeadAt - SlotPlace(Frame, RallyLead->Slot);
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
				// (never on top of another: two gaps that close within a moment of each other would have the second made
				// where the first still is, one airframe inside the other, and a gunner's rounds drawn through both. It
				// comes in a length or two farther back, behind the one before it)
				// (nor in a stream: a gunner's rounds on their way where it came in would be drawn through it. It comes in
				// farther back, clear of them; still in one, it waits a moment)
				const double Clear = G.bFighter ? 60.0 : 90.0;
				bool bStreamed = false;
				for (int Try = 0; bAirborne && Try < 8; ++Try)
				{
					bool bTaken = false;
					for (const auto& Pair : Crafts)
					{
						const Craft& O = Pair.second;
						if (O.Doing != Role::Gone && Len(O.P - At) < Clear) { bTaken = true; break; }
					}
					bStreamed = InStream(At, RallyLead ? LeadVel : GroupVelocity(G.Id), 35.0);
					if (!bTaken && !bStreamed) break;
					At = At - F * Clear;
				}
				if (bStreamed) break;
				// (and never made under the sea or in the ground: a box drawn low, its lower vics stepped down under it)
				if (bAirborne) At.Z = std::max(At.Z, GroundAt(At.X, At.Y) + TorpedoLowM);
				if (RallyLead) Check.RallyMadeFar = std::max(Check.RallyMadeFar, Len(At - LeadAt));
				const Vec Vel = RallyLead ? LeadVel : GroupVelocity(G.Id);
				Craft& C = Make(G, At, Frame.Face, bIntoFight ? 140.0 : bAirborne ? std::max(Len(Vel) * 1.05, G.bFighter ? 95.0 : 75.0) : 0.0, Doing);
				C.Slot = K;
				C.bJoining = bAirborne && !bIntoFight;
				// (or on a box whose gunners the battle has counted one of the group lost to)
				if (bIntoFight) ComeIn(C, G);
			}
		}
		else if (Have - B.Pending - B.Extra > Want)
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
			// (never one a burst the battle counted is on now, its rounds on their way: another goes, or it waits)
			int Go = Have - B.Pending - B.Extra - Want;
			for (size_t K = 0; K < Order.size() && Go > 0; ++K)
			{
				Craft& Off = *Find(Order[K]);
				if (Off.ShotOn != 0 || AirT < Off.HeldTo) continue;
				Leave(Off);
				--Go;
			}
		}
		// Places in the formation, in the order they stand.
		Keys = LiveOf(G.Id);
		if (bSmooth && bBoxKeep && !G.bFighter)
		{
			// (smooth flying: each bomber keeps her slot as the box loses others, and only the gaps are closed up: the
			// box's places given again so that their moves, each from her own place to her new one, cost the least
			// in all by their squares (LeastCost): the box closes up a place or two at a time, each bomber moving to
			// one near her own, never one sent across it. Renumbered, every bomber behind a loss was given another's
			// place 155 to 964 m off and flew a weave to it; filled from the tail, one was sent 960 m across the box
			// from one side to the other; SetSmooth)
			const int N = static_cast<int>(Keys.size());
			std::vector<Craft*> Holder(static_cast<size_t>(N), nullptr);
			std::vector<Craft*> Over; // (a slot past the box's count now, or one another holds: in the order they stand)
			for (int Key : Keys)
			{
				Craft& C = *Find(Key);
				if (C.Slot >= 0 && C.Slot < N && !Holder[static_cast<size_t>(C.Slot)]) Holder[static_cast<size_t>(C.Slot)] = &C;
				else Over.push_back(&C);
			}
			// (a torpedo box on its approach goes in abreast, each on her own line to the ships: a gap there is closed up
			// across the line, its places given again in the order its aircraft fly from left to right, so that none
			// crosses it. Filled from the tail, the line's far end, a Kate was 227 m from her place in the 20 s before
			// her run)
			bool bGap = !Over.empty();
			for (const Craft* H : Holder) bGap = bGap || !H;
			if (G.bAbreast && G.Strike == "torpedo" && bGap)
			{
				const Vec Right = RightOf(G.Face);
				std::vector<Craft*> Across;
				for (int Key : Keys) Across.push_back(Find(Key));
				std::vector<int> Places(static_cast<size_t>(N));
				for (int S = 0; S < N; ++S) Places[static_cast<size_t>(S)] = S;
				std::stable_sort(Across.begin(), Across.end(), [&Right](const Craft* Ca, const Craft* Cb) { return Ca->P.Dot(Right) < Cb->P.Dot(Right); });
				std::stable_sort(Places.begin(), Places.end(), [this, &G, &Right](int Sa, int Sb) { return SlotPlace(G, Sa).Dot(Right) < SlotPlace(G, Sb).Dot(Right); });
				for (int S = 0; S < N; ++S) Across[static_cast<size_t>(S)]->Slot = Places[static_cast<size_t>(S)];
				return;
			}
			if (!bGap) return;
			std::vector<Craft*> Each;
			for (int Key : Keys) Each.push_back(Find(Key));
			std::vector<Vec> Places(static_cast<size_t>(N));
			for (int S = 0; S < N; ++S) Places[static_cast<size_t>(S)] = SlotPlace(G, S);
			// (a move across the box, or up or down it, costs four times its square: she banks to make it, where one
			// ahead or back is a change of her throttle)
			const Vec Fw = Ahead(G.Face);
			std::vector<std::vector<double>> Cost(static_cast<size_t>(N), std::vector<double>(static_cast<size_t>(N)));
			for (int I = 0; I < N; ++I)
			{
				const Vec Own = SlotPlace(G, Each[static_cast<size_t>(I)]->Slot);
				for (int S = 0; S < N; ++S)
				{
					const Vec Way = Places[static_cast<size_t>(S)] - Own;
					const double Along = Way.Dot(Fw);
					Cost[static_cast<size_t>(I)][static_cast<size_t>(S)] = Along * Along + 4.0 * (Way.Dot(Way) - Along * Along);
				}
			}
			const std::vector<int> Of = LeastCost(Cost);
			for (int I = 0; I < N; ++I) Each[static_cast<size_t>(I)]->Slot = Of[static_cast<size_t>(I)];
			return;
		}
		for (int K = 0; K < static_cast<int>(Keys.size()); ++K) Find(Keys[K])->Slot = K;
	}

	void Show::Leave(Craft& C)
	{
		if (C.Doing == Role::Fall || C.Doing == Role::Gone || C.Doing == Role::Home) return;
		// (never while a burst the battle counted is on it, its rounds on their way: it goes once they have struck, or
		// the burst has come to nothing. Whoever sends it home asks again.)
		if (C.ShotOn != 0 || AirT < C.HeldTo) return;
		if (C.Doing == Role::Ground) { C.Doing = Role::Gone; return; }
		C.Doing = Role::Home;
		C.Step = Phase::None;
		C.StepT = 0.0;
		C.bFiring = false;
	}

	Vec Show::GunWant(const Craft& C, const Vec& From, const Craft& Foe, double Near, const Gun& Row, int Mount) const
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
		const Vec Sight = Lead - From;
		const double Range = std::max(1.0, Len(Sight));
		const Vec A = Norm(Sight.Cross(Up), RightOf(C.Hdg));
		const Vec B = Norm(A.Cross(Sight), Up);
		return Norm(Sight + (A * Across + B * Over) * Range, Ahead(C.Hdg, C.Gam));
	}

	bool Show::LayGun(int Mount, const Vec& Want, const Gun& Row, double Lag, Vec& Way)
	{
		// His gun as he has it laid: it follows the point a little behind (about 0.2 s; a gunner laying true on the
		// fighter the battle gives him holds it, Lag 0) and never swings faster than about 40 degrees a second for a
		// power turret or 50 for a hand-held gun. One newly in his arc he has already been following.
		const bool bHand = Row.Mount == "hand";
		Lay& L = Lays[Mount];
		const double Dt = AirT - L.T;
		if (L.T < 0.0 || Dt > 0.5 || Dt < 0.0) L.Way = Want;
		else if (Dt > 0.0)
		{
			Vec Next = Lag > 0.0 ? Norm(L.Way + (Want - L.Way) * (1.0 - std::exp(-Dt / Lag)), Want) : Want;
			const double Step = std::acos(std::max(-1.0, std::min(1.0, Next.Dot(L.Way))));
			const double Most = (bHand ? 50.0 : 40.0) * Deg * Dt;
			if (Step > Most) Next = Norm(L.Way + (Next - L.Way) * (Most / Step), Want);
			L.Way = Next;
		}
		L.T = AirT;
		Way = L.Way;
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

	void Show::Fly(Craft& C, const Cmd& Asked, double Dt, bool bCheck)
	{
		// (rounds the battle counted are on their way to it: until they land it flies on by the command it had as they
		// left, so that they find it where they were sent; see CourseOf)
		const bool bHeld = AirT < C.HeldTo;
		Cmd K = bHeld ? C.Flown : Asked;
		// (but never on into the ground or the sea: under 25 m and coming down, held level)
		if (bHeld && C.Doing != Role::Fall && K.Gam < 0.0 && C.P.Z - GroundAt(C.P.X, C.P.Y) < 25.0) K.Gam = 0.0;
		// (a fighter in a fight, down low after torpedo planes or bombers drawn low: room to pull out of his dive at
		// four g before the ground, and never under 100 m, 30 after a torpedo plane over the sea)
		// (the ground ahead too, high ground coming up; and his wings rolled level as he pulls, so that all his g goes
		// into the pull: in a hard turn it has none to spare, and he would go on down at the angle he dived)
		if (C.bFighter && C.Doing == Role::Fight && !C.bStrafing)
		{
			const double Over = C.P.Z - FloorAhead(C);
			const double Least = Chasing(C) > 0.0 ? 30.0 : 100.0;
			const double SinkNow = std::max(0.0, -std::sin(C.Gam)) * C.Spd;
			// (and, flying smooth, the time her roll takes to pick up and to ease off as her wings come level: about four
			// of her roll times, SetSmooth)
			const double Rolling = std::fabs(C.Bank) / std::max(C.RollMax, 0.5) + (bSmooth ? 4.0 * C.RollTime : 0.0);
			if (Over < Least + SinkNow * SinkNow / (2.0 * Gee * (PullOutG - 1.0)) + SinkNow * (0.5 + Rolling))
			{
				K.Gam = std::max(K.Gam, Over < Least ? 10.0 * Deg : 0.0);
				if (SinkNow > 5.0 || Over < Least)
				{
					K.bBank = true;
					K.Bank = 0.0;
				}
			}
		}
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
		// (a bomber of two engines or more, flying, banks no more than her pilot would with a load and a box about her:
		// wings level on her run, a gentle rally turn in her box, rolled into slowly; by her engines, not only her span:
		// the Pe-2, 17.2 m, and the plain bomber, 19.8, banked 21 degrees with her doors open, and her box spread on its
		// rally turn)
		double RollMost = C.RollMax;
		bool bBoxed = false;
		// (flying smooth, every bomber in her place in a box, a single-engined one too: flown as a fighter rolls, the
		// Kate, the Val, the Dauntless and the Devastator banked to 61 to 75 degrees there, swinging as much as 131 from
		// one side to the other. A single-engined one keeps her own roll law, eased (a step of her roll rate at a frame
		// made her wings chatter), with the box's bank, and her roll rate the box's from the moment her wings are within
		// RallyBankMost and two degrees of level and she rolls no faster than it: out of a steeper bank, a dive's
		// pull-out, she rolls out as she rolls, never cut from 87 degrees a second to 5 in a step. Steeper than that
		// she rolls no faster than SlotRollMost, her roll rate brought down to it in her own roll time: back in her
		// place from a dive banked 55 degrees, rolled level at her full 87 degrees a second in half a second, the
		// Dauntless rocked a third more on her way back to her box than flown as a fighter (bank BAND 0.68 degrees
		// rms at x1 over Midway, against 0.52), and the Val 1.19 over Oahu, against 0.93; SetSmooth)
		// (the bombers' part, SetBoxKeep; off, the roll as 30899121 had it, below, to the byte)
		const bool bTwin = C.bHeavy || C.Form.Span >= 20.0 || C.Form.Engines.size() >= 2;
		if (bSmooth && bBoxKeep)
		{
			if (!C.bFighter && (bTwin || C.Doing == Role::Slot) && C.Doing != Role::Fall)
			{
				const bool bOnRun = C.Bay > 0.0 && (!C.bDropped || C.StickLeft > 0 || (C.StickDone >= 0.0 && AirT < C.StickDone + StickSteady));
				const double Most = bOnRun ? RunBankMost : C.Doing == Role::Slot ? RallyBankMost : HeavyBankMost;
				BankWant = Clamp(BankWant, -Most * Deg, Most * Deg);
				if (C.Doing == Role::Slot && !bOnRun && bTwin)
				{
					RollMost = std::min(RollMost, HeavyRollMost * Deg);
					bBoxed = true;
				}
				else if (C.Doing == Role::Slot && !bOnRun && std::fabs(C.Bank) <= (RallyBankMost + 2.0) * Deg && std::fabs(C.RollNow) <= HeavyRollMost * Deg + 1e-9)
				{
					RollMost = std::min(RollMost, HeavyRollMost * Deg);
				}
				else if (C.Doing == Role::Slot && !bOnRun)
				{
					RollMost = std::min(RollMost, std::max(SlotRollMost * Deg, std::fabs(C.RollNow) * std::exp(-Dt / std::max(C.RollTime, 0.02))));
				}
			}
			// (flying smooth, a bomber of two engines or more out of her box's own roll, on her run, on her way home or
			// falling out of her box, and a single-engined one falling out of hers, rolls no faster than SlotRollMost as
			// well, her roll rate brought down to it in her own roll time; falling, she rolls as she can. A He 111 leaving
			// her box for home rolled from level to 30 degrees in half a second, 60 degrees a second; SetSmooth)
			if (!C.bFighter && C.Doing != Role::Fall && (bTwin ? !bBoxed : C.Doing == Role::Straggle))
			{
				RollMost = std::min(RollMost, std::max(SlotRollMost * Deg, std::fabs(C.RollNow) * std::exp(-Dt / std::max(C.RollTime, 0.02))));
			}
		}
		else if (!C.bFighter && (C.bHeavy || C.Form.Span >= 20.0 || C.Form.Engines.size() >= 2) && C.Doing != Role::Fall)
		{
			const bool bOnRun = C.Bay > 0.0 && (!C.bDropped || C.StickLeft > 0 || (C.StickDone >= 0.0 && AirT < C.StickDone + StickSteady));
			const double Most = bOnRun ? RunBankMost : C.Doing == Role::Slot ? RallyBankMost : HeavyBankMost;
			BankWant = Clamp(BankWant, -Most * Deg, Most * Deg);
			if (C.Doing == Role::Slot && !bOnRun)
			{
				RollMost = std::min(RollMost, HeavyRollMost * Deg);
				bBoxed = true;
			}
		}
		// Roll toward it, no faster than the airframe rolls.
		double Roll_ = Clamp(Wrap(BankWant - C.Bank), -RollMost * Dt, RollMost * Dt);
		// (a bomber in her box rolls at HeavyRollMost at most, a twelfth of a degree a frame: no snap to ease, and her
		// box's rally turn is flown to that roll as it was)
		if (bSmooth && !bBoxed && C.Doing != Role::Fall && Dt > 0.0)
		{
			// (smooth flying, SetSmooth) Her pilot asks a roll rate by how far her wings are from where he wants them,
			// all his aileron for a large change and a little for a small one, and her roll rate eases toward what he
			// asks in her own roll time: she rolls in and rolls out, and a frame's change of mind is never a frame's
			// snap. The gain brings her wings to rest a few percent past at most (RollDamping, of a critical 1).
			const double Gain = 1.0 / (4.0 * RollDamping * RollDamping * std::max(C.RollTime, 0.02));
			const double Ask = Clamp(Wrap(BankWant - C.Bank) * Gain, -RollMost, RollMost);
			const double Rate = C.RollNow + (Ask - C.RollNow) * (1.0 - std::exp(-Dt / std::max(C.RollTime, 0.02)));
			Roll_ = Clamp(Rate, -RollMost, RollMost) * Dt;
		}
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
		const double PushMost = bHeld ? C.FlownPush : C.Step != Phase::Wing ? 3.0 : C.bRejoin ? 5.6 : 4.2;
		const double Push = Clamp((K.Spd - S) / 4.0, -4.0, PushMost);
		const double Load2 = TurnG * TurnG + Vert * Vert;
		const double Bleed = C.Doing == Role::Fall ? 0.0 : 0.3 * std::max(0.0, Load2 - 1.0);
		C.Spd = Clamp(S + (Push - Gee * std::sin(C.Gam) * 0.85 - Bleed) * Dt, C.Doing == Role::Fall ? 20.0 : C.VMin * 0.75, C.VTop);
		C.P += Ahead(C.Hdg, C.Gam) * (C.Spd * Dt);
		C.TurnNow = Omega;
		C.PullNow = Q;
		C.RollNow = Dt > 0.0 ? Roll_ / Dt : 0.0;
		C.Flown = K;
		C.FlownPush = PushMost;
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
		// One going home: away from the target, a little lower, until it is out of the fight; then gone.
		auto FlyHome = [this, &C, Dt]()
		{
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
		};
		// (a dive bomber pulled out of her dive, a torpedo plane whose torpedo has gone: how high she flies after, low)
		if (G && !C.bFighter && C.bDropped && (G->Strike == "torpedo" || G->Strike == "dive") && C.Doing != Role::Ground
		    && !(C.Doing == Role::Dive && (C.Step == Phase::Peel || C.Step == Phase::Plunge || C.Step == Phase::Pull || C.Step == Phase::Run)))
			Check.LeftLowMost = std::max(Check.LeftLowMost, C.P.Z - GroundAt(C.P.X, C.P.Y));
		// A torpedo plane turning in on her run (her group's run begun, or 6 km out), and after her drop breaking away:
		// PilotTorpedo.
		// (not one made after her group's drop: she has no run to fly)
		const auto TB = G ? Bands.find(G->Id) : Bands.end();
		const bool bGroupDropped = TB != Bands.end() && TB->second.bDropped;
		if (G && !C.bFighter && G->Strike == "torpedo" && C.Doing == Role::Slot && !C.bRunDone
		    && (C.bDropped || (!bGroupDropped && (G->Run > 0.0 || std::hypot(G->X - TargetX, G->Y - TargetY) < 6000.0))))
		{
			C.Doing = Role::Dive;
			C.Step = C.bDropped ? Phase::Away : Phase::Run;
			C.StepT = 0.0;
		}
		switch (C.Doing)
		{
		case Role::Ground:
			if (G) PilotGround(C, *G, Dt);
			return;
		case Role::Fight:
			if (G && G->Doing == Act::Fight && G->Engaged >= 0 && PilotFight(C, *G, Dt))
			{
				if (!C.bStrafing && C.Doing == Role::Fight)
				{
					// (one who followed a torpedo plane down to wave-top is counted with those chases until he has climbed
					// back over 60 m: her loss leaves him low over the sea, as it found him, not diving at it)
					const double Over = C.P.Z - GroundAt(C.P.X, C.P.Y);
					if (Chasing(C) > 0.0)
					{
						// (where the chase leaves him, and the sink he carries out of it: read from his flight as she goes,
						// stopped by his floor guard's own pull-out, PullOutG)
						const double Sink = std::max(0.0, -std::sin(C.Gam)) * C.Spd;
						C.bLowChase = true;
						C.bLowChaseUp = false;
						C.LowChaseFrom = Over;
						C.LowChaseGive = Sink * Sink / (2.0 * Gee * (PullOutG - 1.0));
					}
					else if (C.bLowChase)
					{
						// The climb back covers only the climb back (the design director, 25 November: by cause, never a
						// timer): it ends as he passes 60 m; and deeper than the chase left him, past the sink he carried
						// out of it, or any descent once he has begun to climb, is a dive like any other, counted against
						// the 60 m at once, a new fight or not.
						if (Over >= 60.0 || Over < C.LowChaseFrom - C.LowChaseGive || (C.bLowChaseUp && C.Gam < 0.0)) C.bLowChase = false;
						else if (C.Gam > 0.0) C.bLowChaseUp = true;
					}
					double& Lowest = C.bLowChase ? Check.LowestChase : Check.LowestFight;
					Lowest = std::min(Lowest, Over);
				}
				// (one who turned for home in this step flies it as one going home: a step not flown is a stop in the
				// air, and the rounds laid wide of him as he flew are drawn through him)
				if (C.Doing == Role::Home) FlyHome();
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
			FlyHome();
			return;
		}
		if (!G)
		{
			// (its group gone from the battle with a burst still on it: it flies on as it was until that is over)
			if (C.ShotOn != 0 || AirT < C.HeldTo) Fly(C, C.Flown, Dt, true);
			return;
		}
		if (C.bFighter && G->Doing == Act::Fight && G->Engaged >= 0 && C.Doing == Role::Slot)
		{
			// (one still closing up on the formation goes straight into the fight with the rest)
			C.bJoining = false;
			C.Doing = Role::Fight;
			C.Step = Phase::None;
			if (PilotFight(C, *G, Dt) || C.Doing != Role::Fight)
			{
				if (C.Doing == Role::Home) FlyHome();
				return;
			}
			C.Doing = Role::Slot;
			StandDown(C);
		}
		const Vec GV = GroupVelocity(C.Group);
		// A level bomber letting her stick go, in her place or falling back hurt, holds her heading wings level until it
		// has all gone and a moment after; then she turns after her box (and closes on her place in it, or limps on).
		// (and from her doors full open on her run, in her place, before it goes)
		const bool bRunHeld = C.bRunHeld && !C.bDropped && C.Doing == Role::Slot;
		if (!C.bFighter && (bRunHeld || (C.bStickBay && (C.Doing == Role::Slot || C.Doing == Role::Straggle)
			&& (C.StickLeft > 0 || (C.StickDone >= 0.0 && AirT < C.StickDone + StickSteady)))))
		{
			Cmd K;
			K.Hdg = C.RunHdg;
			K.Gam = 0.0;
			K.Spd = C.Spd;
			Fly(C, K, Dt, true);
			return;
		}
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
				// (behind her box as it flies: on its lead while it turns for home, Rally)
				Vec Box(G->X, G->Y, G->Alt), BoxVel = GV;
				double Face = G->Face;
				RallyFrame(C.Group, Box, Face, BoxVel);
				const Vec F = Ahead(Face);
				Vec To = Box - F * (Mine ? Mine->Back : 180.0) - Up * (Mine ? Mine->Down : 18.0) + RightOf(Face) * (C.Hand * G->W * 0.3);
				if (bSmooth && bBoxKeep)
				{
					// (smooth flying: she drops back and down from where she fell out of her box, by as much as the
					// battle has her behind and below it, and out to her side; and her place goes there from where she
					// is, no faster across than one closing up in her box. Asked at once for a place up to 790 m
					// across and the length of her box ahead of her, a He 111 rolled to 30 degrees in half a second as
					// she fell out; PlaceWant)
					if (C.PlaceStep != PilotSteps - 1 || !C.bPlaceStrag)
					{
						const Vec Was = C.P - Box;
						C.StragFrom = Vec(Clamp(Was.Dot(F), -1500.0, 0.0), 0.0, Clamp(Was.Z, -150.0, 150.0));
						C.StragBack = 0.0;
						C.StragDown = 0.0;
					}
					if (Mine)
					{
						C.StragBack = Mine->Back;
						C.StragDown = Mine->Down;
					}
					To = Box + F * (C.StragFrom.X - C.StragBack) + RightOf(Face) * (C.Hand * G->W * 0.3) + Up * (C.StragFrom.Z - C.StragDown);
					// (her place no lower than the floor below, before it is eased there as well as after; held up
					// there, it is the floor's, level, and goes on from there: a place eased down past the floor
					// carried its sinking on, and a Kate falling out of her box as it came down to wave-top let her
					// torpedo go 7 m under the sea; one only held up kept its way down below the floor and climbed back
					// from it as her box came level, and she let it go 37 m over the sea)
					To.Z = std::max(To.Z, std::max(GroundAt(C.P.X, C.P.Y), GroundAt(To.X, To.Y)) + TorpedoLowM);
					const double BoxClimb = BoxVel.Z;
					To = PlaceWant(C, Box, Face, To, true, StragAheadMost, StragAheadPull, SlotCloseMost, SlotPull, Dt, BoxVel);
					const double Floor = std::max(GroundAt(C.P.X, C.P.Y), GroundAt(To.X, To.Y)) + TorpedoLowM;
					if (To.Z < Floor)
					{
						To.Z = Floor;
						C.PlaceAt.Z = Floor - Box.Z;
						C.PlaceVel.Z = -BoxClimb;
						BoxVel.Z = 0.0;
					}
				}
				// (below a box drawn low, a torpedo group's at wave-top: no lower than it, never into the sea)
				To.Z = std::max(To.Z, std::max(GroundAt(C.P.X, C.P.Y), GroundAt(To.X, To.Y)) + TorpedoLowM);
				Cmd K = Steer(C, To, BoxVel, 0.08, C.VMin, C.VMax);
				// (a torpedo plane on her group's run, hurt: slowing for the drop with the rest)
				if (G->Strike == "torpedo" && !C.bDropped && G->Run > 0.3) K.Spd = std::min(K.Spd, TorpedoSpeed(C.Model));
				Fly(C, K, Dt, true);
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
		// After its bombs, a level box turns for home on its lead (Rally).
		if (!C.bFighter && C.Doing == Role::Slot)
		{
			const auto Box = Bands.find(C.Group);
			if (Box != Bands.end() && Box->second.bDropped && Rally(C, *G, Box->second, Dt)) return;
		}
		// In its place: the formation's speed, a slow escort weaving beside its bombers; an escort with a role at its
		// station over them (EscortStation). (A bomber flying smooth: on her way to it when it has moved, PlaceWant.)
		// (a torpedo box gone abreast for its run, each on her own line to the ships, closes across it quicker,
		// TorpedoLineMost: at the box's own pace across, the Kates over Oahu were up to 97 m from their places in the
		// 20 air s before their runs, flown straight to them 18, and at this 71; pulled across quicker as well, the
		// Devastators' wings swung a degree and more twice as often in their box)
		const bool bLine = G->bAbreast && G->Strike == "torpedo";
		Vec ToVel = GV;
		// (off, SetBoxKeep: To and ToVel as 30899121 had them, SlotPlace(*G, C.Slot) and GV)
		Vec To = bSmooth && bBoxKeep && !C.bFighter ? PlaceWant(C, Vec(G->X, G->Y, G->Alt), G->Face, SlotPlace(*G, C.Slot), false, SlotAheadMost, SlotAheadPull,
			bLine ? TorpedoLineMost : SlotCloseMost, SlotPull, Dt, ToVel)
			: SlotPlace(*G, C.Slot);
		const bool bStation = C.bFighter && G->Doing != Act::Fight && EscortStation(C, *G, To, ToVel);
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
		if (C.bFighter && Len(GV) < 105.0 && !bStation)
		{
			// Wide, slow esses: the escort keeps its speed up (about 85 to 120 m/s) and its turns gentle (400 m and more).
			// With the enemy coming it swings wider and a little higher, to have fighting speed in hand when he arrives.
			const double W = AirT * 2.0 * Pi / 34.0 + C.Weave * 0.15;
			const double Wide = 380.0 + 260.0 * Near;
			To += RightOf(G->Face) * (Wide * std::sin(W)) + Up * (120.0 * Near);
			ToVel += RightOf(G->Face) * (Wide * 2.0 * Pi / 34.0 * std::cos(W));
		}
		// (a bomber's place drawn low, a torpedo group let down to wave-top, over the ground: never into it, nor the
		// rising ground a few hundred metres on)
		if (!C.bFighter)
		{
			const Vec F = Ahead(C.Hdg);
			const double Under = std::max({ GroundAt(C.P.X, C.P.Y), GroundAt(C.P.X + F.X * 400.0, C.P.Y + F.Y * 400.0), GroundAt(C.P.X + F.X * 900.0, C.P.Y + F.Y * 900.0) });
			// (held up there, her place's own way down, flying smooth, is not carried on)
			if (bSmooth && bBoxKeep && To.Z < Under + TorpedoLowM) ToVel.Z = std::max(ToVel.Z, GV.Z);
			To.Z = std::max(To.Z, Under + TorpedoLowM);
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
		// (or one the battle has counted a hit on at this group's guns which the scene has yet to show, the group's
		// fire gone elsewhere since: its pilots still have that to act out)
		return DueAllOn(Id, G.Id) > 0;
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
		C.bGuard = C.bCover = C.bReversed = C.bFleeing = C.bOpened = C.bAnywhere = C.bStrafing = C.bBlind = C.bLow = C.bAnswer = C.bPress = false;
		C.PassAt = -1.0;
		C.YoYo = 0.0;
		C.SightOn = C.SightBack = -1;
	}

	void Show::Brief()
	{
		Hunters.clear();
		Chasers.clear();
		Aloft.clear();
		Alive.clear();
		Due.clear();
		DueStrag.clear();
		DueAll.clear();
		for (const Held& Owed : Later)
		{
			if (!Owed.bForShot) continue;
			// (a fighter lost to a box's gunners was going in on that box: its own group's pilots act it, see Pressing. A
			// gunner's hit that it survived is nobody's to act)
			const int Box = BoxOf(Owed.E);
			if (Box >= 0)
			{
				if (Owed.E.What == "air:lost") Due[{ Owed.E.Unit, Box }]++;
				continue;
			}
			// (all a fighter group's guns did: its pilots are sent where it waits; a loss among it they answer)
			DueAll[{ Owed.E.Unit, Owed.E.By }]++;
			if (Owed.E.What != "air:lost") continue;
			Due[{ Owed.E.Unit, Owed.E.By }]++;
			if (Owed.E.Strag >= 0) DueStrag.insert({ Owed.E.Unit, Owed.E.Strag });
		}
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			C.Marker = -1;
			if (!Live(C)) continue;
			Alive[C.Group]++;
			if (C.Doing != Role::Ground) Aloft[C.Group]++;
			if (!C.bFighter || C.Doing != Role::Fight || C.Step == Phase::Away) continue;
			if (C.TargetGroup >= 0) Hunters[C.TargetGroup].push_back(C.Key);
			if (C.Target >= 0) Chasers[C.Target].push_back(C.Key);
		}
		// The ones who never see it coming. A loss or a hit the battle has counted on a fighter group at an enemy
		// group's guns, which the scene has yet to show: the fighters of that group that enemy leaders are going for
		// now, the nearest first and as many as are counted, have not seen them and will not (LookBehind), as most who
		// were shot down never did. The battle decided it; the scene only chooses whose it is.
		{
			struct Going
			{
				double Far = 0.0;
				int On = 0, By = 0;
			};
			std::vector<Going> Goings;
			for (const auto& Pair : Crafts)
			{
				const Craft& S = Pair.second;
				if (!S.bFighter || !Live(S) || S.Doing != Role::Fight || S.Target < 0 || S.Lead != S.Key) continue;
				if (S.Step != Phase::Run && S.Step != Phase::Chase && S.Step != Phase::Turn) continue;
				const Craft* T = Find(S.Target);
				if (!T || !T->bFighter || T->Side == S.Side || !Live(*T) || T->Doing != Role::Fight || DueAllOn(T->Group, S.Group) <= 0) continue;
				const double Far = Len(T->P - S.P);
				if (Far < 1500.0) Goings.push_back(Going{ Far, T->Key, S.Key });
			}
			std::sort(Goings.begin(), Goings.end(), [](const Going& A, const Going& B) { return A.Far < B.Far; });
			std::map<std::pair<int, int>, int> Chosen;
			for (const Going& Each : Goings)
			{
				Craft& T = *Find(Each.On);
				if (T.Marker >= 0) continue;
				const std::pair<int, int> Key(T.Group, Find(Each.By)->Group);
				if (Chosen[Key] >= DueAllOn(Key.first, Key.second)) continue;
				Chosen[Key]++;
				T.Marker = Each.By;
			}
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
		for (int Key_ : MembersOf(G.Id))
		{
			Craft& C = Crafts.find(Key_)->second;
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
		// (an enemy the group's fire has left, with a hit of its guns there the scene has yet to show: a pair's worth)
		for (const auto& Owed : DueAll)
		{
			if (Owed.first.second != G.Id || Owed.second <= 0 || !Fights(G, Owed.first.first)) continue;
			bool bHas = false;
			for (const Mark& Each : Marks) bHas = bHas || Each.Id == Owed.first.first;
			if (bHas) continue;
			Mark M;
			M.Id = Owed.first.first;
			M.Rate = Marks.empty() ? 1.0 : 0.5 * Sum / static_cast<double>(Marks.size());
			Marks.push_back(M);
			Sum += M.Rate;
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
		for (int Key_ : MembersOf(G.Id))
		{
			Craft& C = Crafts.find(Key_)->second;
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
					if (L->TargetGroup != M_.Id || L->bPress) continue;
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
		for (int Key_ : MembersOf(G.Id))
		{
			Craft& C = Crafts.find(Key_)->second;
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

	bool Show::EscortStation(const Craft& C, const Group& G, Vec& To, Vec& ToVel) const
	{
		if (!C.bFighter || G.With < 0) return false;
		const bool bTop = G.Role == "top";
		if (!bTop && G.Role != "close") return false;
		const auto BIt = Now.find(G.With);
		if (BIt == Now.end() || BIt->second.bFighter || BIt->second.Side != G.Side || BIt->second.Doing == Act::Down) return false;
		const Group& Bx = BIt->second;
		Vec Box(Bx.X, Bx.Y, Bx.Alt), BoxVel = GroupVelocity(Bx.Id);
		double Face = Bx.Face;
		RallyFrame(Bx.Id, Box, Face, BoxVel);
		const Vec F = Ahead(Face), R = RightOf(Face);
		const int Four = C.Slot / 4;
		// (close: its fours one to each flank in turn, a flank's fours one behind another; top cover all on one side,
		// behind the lead box; the group's flights weave together, the two flanks out and in as one)
		const double Side = bTop ? 1.0 : Four % 2 == 0 ? 1.0 : -1.0;
		const double Leg = bTop ? TopLegS : CloseLegS, Swing = bTop ? TopSwingM : CloseSwingM;
		const double W = AirT * Pi / Leg + 0.7 * G.Id;
		const Vec Own = SlotOf(G, C.Slot % 4) - SlotOf(G, 0);
		const int Row = bTop ? Four : Four / 2;
		To = Box + R * (Side * ((bTop ? TopOutM : CloseOutM) + Swing * std::sin(W))) + Up * ((bTop ? TopOverM : CloseOverM) + Row * 60.0)
		     - F * ((bTop ? 1000.0 : 0.0) + Row * 450.0) + Own;
		// (over bombers drawn low, torpedo planes at wave-top: never under 600 m over the ground or the sea)
		To.Z = std::max(To.Z, GroundAt(To.X, To.Y) + 600.0);
		ToVel = BoxVel + R * (Side * Swing * Pi / Leg * std::cos(W));
		return true;
	}

	double Show::FloorAhead(const Craft& C) const
	{
		const Vec Way = Ahead(C.Hdg) * std::max(C.Spd * std::cos(C.Gam), 0.0);
		double Floor = GroundAt(C.P.X, C.P.Y);
		for (double T = 1.0; T <= 3.0; T += 1.0) Floor = std::max(Floor, GroundAt(C.P.X + Way.X * T, C.P.Y + Way.Y * T));
		return Floor;
	}

	void Show::MindGround(const Craft& C, Cmd& K, double Least) const
	{
		const double Over = C.P.Z - FloorAhead(C);
		// (after a torpedo plane at wave-top over the sea: down to a little over her)
		const double Low = Chasing(C);
		if (Low > 0.0) Least = std::min(Least, std::max(40.0, Low + 15.0));
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

	void Show::Bear(const Craft& C, Cmd& K) const
	{
		// In range, a pilot pulls his sight on to his aim with all his aircraft has. The easy swing on to a heading
		// that serves him everywhere else leaves his nose trailing ten or twenty degrees behind an aircraft crossing
		// before him, and no burst is laid from there. (Fly still holds him to his g and his turning circle.)
		K.Hdg = C.Hdg + Clamp(Wrap(K.Hdg - C.Hdg) * 4.0, -2.0, 2.0);
		K.Gam = C.Gam + (K.Gam - C.Gam) * 3.0;
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
		for (int Key_ : MembersOf(E.Id))
		{
			const Craft& T = Crafts.find(Key_)->second;
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
		// A hit the battle has counted on this one is his to make, or his burst is making it: he flies his sight on to
		// the place, as a pilot does who means to hit, with what his aircraft has left to turn.
		C.ReadyRange = Range;
		Barrel Sight;
		if (Dist < Range * 1.2 && Dist > 60.0 && Live(T) && Dt > 0.0 && NoseGun(C, Sight))
		{
			Vec Way;
			double Tof = 0.0;
			int At = -1;
			if (const Shot* Mine = Carrying(C.Key, -1))
			{
				const Craft* V = Find(Mine->Victim);
				if (V && Live(*V) && Solve(Sight, *V, WalkOf(*Mine), Way, Tof)) FineAim(C, Way, Dt);
			}
			else if (!C.bScored && Mark(C, Sight, T.Group, Range * 1.2, 12.0 * Deg, false, At, Way, Tof)) FineAim(C, Way, Dt);
		}
		const Vec Aim = T.P + Velocity(T) * (Dist / 800.0);
		const double Off = std::acos(Clamp(Ahead(C.Hdg, C.Gam).Dot(Norm(Aim - C.P)), -1.0, 1.0));
		if (bMay && C.Burst <= 0.0 && C.Rest <= 0.0 && Dist < Range && Off < Most && T.Doing != Role::Fall)
		{
			C.Burst = Roll.U(Lo, Hi);
			C.Rest = C.Burst + Roll.U(0.8, 1.8);
			C.BurstAt = AirT;
			Check.Bursts++;
		}
		// (a burst that carries a hit the battle counted goes on, whatever the group's fire is doing)
		const bool bCarries = Carrying(C.Key, -1) != nullptr;
		C.bFiring = C.Burst > 0.0 && (bMay || bCarries) && Dist < Range * 1.2 && Dist > 60.0 && T.Doing != Role::Fall && T.Doing != Role::Gone;
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
		if (!C.bFiring)
		{
			// In range and his thumb off: should a hit the battle counted on this one be waiting to be shown, he fires
			// as soon as his guns can be laid on it (FireGuns).
			if (C.Burst <= 0.0 && C.Rest <= 0.5 && Dist < Range * 1.2 && Dist > 60.0 && Live(T)) C.Ready = T.Key;
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
		C.Ready = -1;
		C.bStrafing = false;
		if (C.Burst > 0.0) C.Burst -= Dt;
		if (C.Rest > 0.0) C.Rest -= Dt;
		if (C.Burst <= 0.0) C.bScored = false;
		LookBehind(C);
		// A burst he opened on an aircraft crossing his sights goes on before anything.
		if (KeepsSight(C, Dt)) return true;
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
		// (one going in on a box for its gunners goes on in, whatever else is owed)
		if (!Pressing(C, G) && Answering(C, G, Dt)) return true;
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

	bool Show::KeepsSight(Craft& C, double Dt)
	{
		// An aircraft his burst is carrying a hit on which is not the one his pass or his chase is flown on (it crossed
		// his sights as his guns were free: FireGuns), or which will fly on hit with a loss waiting beside the hit: he
		// flies his sight on it, as a pilot does who has a target before his guns, while the burst lasts and its rounds
		// are on their way; and, his strikes seen on it and a loss still to show, for the second it takes him to fire
		// again. Then he goes back to the one he was after. He lets it go sooner than run into it.
		if (C.SightOn < 0) return false;
		Craft* V = Find(C.SightOn);
		const Shot* Mine = Carrying(C.Key, -1);
		Barrel Sight;
		Vec Way;
		double Tof = 0.0;
		bool bOn = V && Live(*V) && V->bShown && AirT < C.SightUntil && (!Mine || Mine->Victim == V->Key) && NoseGun(C, Sight);
		if (bOn && !Mine)
		{
			// (his burst over: only for one his guns have just struck, with a loss to show on it)
			const int At = Wanted(C.Group, -1, *V);
			bOn = At >= 0 && Later[static_cast<size_t>(At)].E.What == "air:lost" && V->StruckBy == C.Group && AirT - V->StruckAt < 1.0 && V->ShotOn == 0;
		}
		if (bOn) bOn = Solve(Sight, *V, Mine ? WalkOf(*Mine) : Vec(), Way, Tof);
		if (bOn)
		{
			const Vec Rel = V->P - C.P;
			const double Dist = Len(Rel);
			const double Closing = (Velocity(C) - Velocity(*V)).Dot(Norm(Rel));
			bOn = Dist > 110.0 + 0.9 * std::max(0.0, Closing) && Way.Dot(Ahead(C.Hdg, C.Gam)) > std::cos(25.0 * Deg);
		}
		if (!bOn)
		{
			// (over: the one he was after again, if it still flies)
			const Craft* Was = C.SightBack >= 0 ? Find(C.SightBack) : nullptr;
			C.Target = Was && Live(*Was) ? C.SightBack : -1;
			// (his burst on it, still going, is over too; one he has since opened on another is not)
			if (Mine && V && Mine->Victim == V->Key) C.Burst = 0.0;
			C.SightOn = C.SightBack = -1;
			return false;
		}
		Cmd K;
		K.Hdg = std::atan2(Way.X, Way.Y);
		K.Gam = std::asin(Clamp(Way.Z, -1.0, 1.0));
		K.Spd = C.Spd;
		MindGround(C, K);
		Fly(C, K, Dt, true);
		FightSpeed(C);
		FineAim(C, Way, Dt);
		C.bFiring = Mine != nullptr && C.Burst > 0.0;
		C.Target = V->Key;
		if (C.bFiring)
		{
			V->ShotAt = AirT;
			V->ShotBy = C.Group;
		}
		C.Task = "attacking";
		return true;
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
		if (Owed) for (int Key_ : MembersOf(C.Group)) { const Craft& O = Crafts.find(Key_)->second; if (O.Group == C.Group && O.Key != C.Key && O.bBlind && Live(O)) Blind++; }
		if (Owed > Blind || C.Marker >= 0)
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
			if (Mine && !Mine->bFighter && (C.bPress || Len(Mine->P - C.P) < 900.0 || Len(F->P - C.P) > 420.0)) return false;
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

	int Show::GoingIn(int GroupId, int Box, double Within) const
	{
		// The fighters of that group going in on that box now (a wingman goes in with his leader): all of them, or
		// those who as they are closing will be at its gunners' reach within so many seconds.
		const auto EIt = Now.find(Box);
		const Vec Middle = EIt != Now.end() ? Vec(EIt->second.X, EIt->second.Y, EIt->second.Alt) : Vec();
		const Vec Bv = GroupVelocity(Box);
		int Going = 0;
		for (int Key_ : MembersOf(GroupId))
		{
			const Craft& O = Crafts.find(Key_)->second;
			if (O.Group != GroupId || !O.bFighter || O.Lead != O.Key || O.Step != Phase::Run || O.TargetGroup != Box || !Live(O) || O.Doing != Role::Fight) continue;
			const Vec Rel = Middle - O.P;
			const double Dist = Len(Rel);
			if (Within > 0.0 && Dist - 600.0 > Within * std::max(1.0, (Velocity(O) - Bv).Dot(Rel) / std::max(1.0, Dist))) continue;
			const Craft* W = O.Wing ? Find(O.Wing) : nullptr;
			Going += W && Live(*W) && W->Step == Phase::Wing ? 2 : 1;
		}
		return Going;
	}

	void Show::PressOn(Craft& C, const Group& G, const Group& E)
	{
		C.bPress = true;
		C.bCover = C.bReversed = C.bAnswer = false;
		C.TargetGroup = E.Id;
		C.bLow = false;
		StartRun(C, G, E, "opening");
		// (at the bomber the battle names, if she is still in her box)
		int Seat = -1;
		for (const Held& Owed : Later) if (Seat < 0 && Owed.bForShot && Owed.E.What == "air:lost" && Owed.E.Unit == G.Id && BoxOf(Owed.E) == E.Id) Seat = Owed.E.GunSeat;
		if (Seat >= 0) for (int Key_ : MembersOf(E.Id))
		{
			const Craft& T = Crafts.find(Key_)->second;
			if (Seat >= 0 && T.Group == E.Id && Live(T) && T.Doing == Role::Slot && T.Slot == Seat / std::max(1, E.Per)) C.Target = T.Key;
		}
		Hunters[E.Id].push_back(C.Key);
	}

	bool Show::Pressing(Craft& C, const Group& G)
	{
		// A fighter of his group the battle has counted lost to a box's gunners, which the scene has yet to show. The
		// battle has it that he was going in on that box when they got him: so the leader best placed goes in on it
		// now, from where he stands, at the bomber whose gunner the battle names, and presses on whatever is behind
		// him. Her gunners do the rest (FireGuns). The battle decided it; the scene only finds the pilot to act it.
		if (C.bPress)
		{
			// (until his pass is flown, or the gunners have been given the last of them)
			if (C.Step == Phase::Run && C.TargetGroup >= 0 && DueOn(G.Id, C.TargetGroup) > 0) return true;
			C.bPress = false;
			return false;
		}
		if (Due.empty() || C.Lead != C.Key || C.bHurt || C.bDry || C.Burst > 0.0) return false;
		for (const auto& Each : Due)
		{
			const int Box = Each.first.second;
			if (Each.first.first != G.Id || Each.second <= 0) continue;
			const auto EIt = Now.find(Box);
			if (EIt == Now.end() || EIt->second.bFighter || !Fights(G, Box)) continue;
			const Group& E = EIt->second;
			const Vec Middle(E.X, E.Y, E.Alt), Bv = GroupVelocity(Box);
			// (as many going in as its gunners are owed; of the leaders free to go, the one who could be firing on it
			// soonest)
			auto Free = [this, &Middle](const Craft& O)
			{
				if (O.Lead != O.Key || !Live(O) || O.Doing != Role::Fight || O.bPress || O.bHurt || O.bDry || O.Burst > 0.0) return false;
				if (O.Step == Phase::Break || O.Step == Phase::Zoom || O.Step == Phase::Away || Dodging(O)) return false;
				// (not from far below it: he could not hold his nose up on it)
				return O.Step != Phase::Run && O.P.Z > Middle.Z - 500.0;
			};
			if (!Free(C) || GoingIn(G.Id, Box, 0.0) >= Each.second) continue;
			const double Eta = EtaTo(C, Middle, Bv, 500.0);
			if (Eta > 24.0) continue;
			bool bSoonest = true;
			for (int Key_ : MembersOf(G.Id))
			{
				const Craft& O = Crafts.find(Key_)->second;
				if (O.Group == G.Id && O.bFighter && O.Key != C.Key && Free(O) && EtaTo(O, Middle, Bv, 500.0) < Eta) bSoonest = false;
			}
			if (!bSoonest) continue;
			PressOn(C, G, E);
			return true;
		}
		return false;
	}

	void Show::ComeIn(Craft& C, const Group& G)
	{
		// The next of a group coming into its fight, with one of the group counted lost to a box's gunners and too
		// few of them about to be there for its gunners to have (a stern chase from out of the fight takes half a
		// minute): he comes in on that box, from out beyond where its pass is flown, and goes straight in
		// (Pressing). A model stands for several aircraft; which of them is seen next, and where, is the scene's
		// to choose.
		for (const auto& Each : Due)
		{
			const int Box = Each.first.second;
			if (Each.first.first != G.Id || Each.second <= 0) continue;
			const auto EIt = Now.find(Box);
			if (EIt == Now.end() || EIt->second.bFighter || !Fights(G, Box) || GoingIn(G.Id, Box, 12.0) >= Each.second) continue;
			const Group& E = EIt->second;
			const Vec Middle(E.X, E.Y, E.Alt);
			int NextFlight = 0;
			for (int Key_ : MembersOf(G.Id)) NextFlight = std::max(NextFlight, Crafts.find(Key_)->second.Flight + 1);
			C.Lead = C.Key;
			C.Seat = 0;
			C.Flight = NextFlight;
			const Vec Perch = PerchOf(C, PassOn(G, Box), Middle, Ahead(E.Face), RightOf(E.Face));
			C.P = Middle + (Perch - Middle) * 1.5;
			C.P.Z = std::max(C.P.Z, GroundAt(C.P.X, C.P.Y) + 400.0);
			C.LastP = C.P;
			C.Hdg = std::atan2(Middle.X - C.P.X, Middle.Y - C.P.Y);
			C.Spd = C.VMax;
			C.Doing = Role::Fight;
			C.bJoining = false;
			PressOn(C, G, E);
			return;
		}
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
		// (nor off a box his pair was given while his group's guns have work of their own to show there)
		{
			const auto Box = Now.find(C.TargetGroup);
			if (Box != Now.end() && !Box->second.bFighter && DueAllOn(C.TargetGroup, G.Id) > 0) return false;
		}
		// (as many of his group after each enemy group as it has losses to show)
		std::map<int, int> Taken;
		for (int Key_ : MembersOf(G.Id))
		{
			const Craft& O = Crafts.find(Key_)->second;
			if (O.Group != G.Id || !O.bAnswer || O.Target < 0) continue;
			const Craft* V = Find(O.Target);
			if (V) Taken[V->Group]++;
		}
		Craft* Pick = nullptr;
		double Best = 45.0;
		for (auto& Pair : Crafts)
		{
			Craft& F = Pair.second;
			if (F.Side == C.Side || !F.bFighter || !Live(F) || F.Doing != Role::Fight || F.Step == Phase::Away) continue;
			const int Owed = DueOn(F.Group, G.Id);
			if (Owed <= 0) continue;
			const auto Tk = Taken.find(F.Group);
			if (Tk != Taken.end() && Tk->second >= Owed) continue;
			// (the one his guns can soonest bear on: the turn on to it and the closing, as it is flying; not one a
			// comrade already has, nor one already turning hard)
			const Vec Rel = F.P - C.P;
			if (Len(Rel) > 2600.0) continue;
			double Cost = EtaTo(C, F.P, Velocity(F), 420.0);
			if (Dodging(F)) Cost += 6.0;
			const auto Ch = Chasers.find(F.Key);
			if (Ch != Chasers.end()) for (int Other : Ch->second) { const Craft* O = Find(Other); if (O && O->Side == C.Side && O->Lead == O->Key && O->Key != C.Key) Cost += 8.0; }
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
		// (never while a burst the battle counted is on him, its rounds on their way: he goes once they have struck, or
		// it has come to nothing)
		if (bHit && G.Per >= 2 && C.StepT > 5.0 && C.ShotOn == 0 && AirT >= C.HeldTo)
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
				for (int Key_ : MembersOf(T->Group))
				{
					Craft& O = Crafts.find(Key_)->second;
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
		const bool bGoing = T && (L.Step == Phase::Run || L.Step == Phase::Turn || L.Step == Phase::Chase);
		const double Range = !T ? 0.0 : T->bFighter ? 480.0 - 100.0 * C.Skill : L.Pass == "headon" ? 900.0 : 700.0 - 150.0 * C.Skill;
		// His own shot. A hit of his group's guns the battle has counted on that unit and the scene has yet to show,
		// and the aircraft before him inside his reach: he leaves his station for the seconds it takes and flies his
		// own sight on to it, as each of a pair going in took his own shot, then closes up on his leader again. (Held
		// on his station, his nose lies along his leader's and never comes on.)
		bool bOwn = false;
		if (bGoing && DueAllOn(T->Group, G.Id) > 0 && Len(L.P - C.P) < 400.0)
		{
			const Vec Rel = T->P - C.P;
			const double Dist = Len(Rel);
			const double Closing = (Velocity(C) - Velocity(*T)).Dot(Norm(Rel));
			bOwn = Dist < 1.5 * Range && Dist > 110.0 + 0.9 * std::max(0.0, Closing) && Rel.Dot(Ahead(C.Hdg, C.Gam)) > 0.7 * Dist;
			if (bOwn)
			{
				Cmd K = Toward(C, T->P + Velocity(*T) * (Dist / 800.0), std::max(108.0, Len(Velocity(L))));
				Bear(C, K);
				MindGround(C, K, 120.0);
				Fly(C, K, Dt, true);
			}
		}
		if (!bOwn) WingOn(C, L, bIn ? 40.0 : 60.0 + Loose, bIn ? 75.0 : 55.0 + Loose, Dt);
		if (bGoing)
		{
			if (T->bFighter) Squeeze(C, *T, G, Range, 8.0 * Deg, 1.0, 2.0, Dt);
			else Squeeze(C, *T, G, Range, 10.0 * Deg, L.Pass == "headon" ? 0.5 : 1.0, L.Pass == "headon" ? 1.2 : 2.0, Dt);
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
		// By the box: as the record has the close escort (the raids answer's Part 3.4), over it and out on its flank,
		// outside its gunners' reach, in S-turns either side of its track to hold the bombers' speed with fighting speed
		// in hand; top cover higher (EscortStation's numbers).
		const double Side = C.Flight % 2 == 0 ? 1.0 : -1.0;
		const bool bTop = G.Role == "top";
		const double Leg = bTop ? TopLegS : CloseLegS, Swing = bTop ? TopSwingM : CloseSwingM;
		const double Wv = AirT * Pi / Leg + C.Weave * 0.2;
		Vec To = Box + Rb * (Side * ((bTop ? TopOutM : CloseOutM) + Swing * std::sin(Wv))) + Up * (bTop ? TopOverM : CloseOverM) + Fb * (C.Seat >= 2 ? -350.0 : 150.0);
		// (over bombers let down low, torpedo planes at wave-top: high above them, as Midway's Wildcats flew over their
		// Devastators, never weaving down at the sea)
		To.Z = std::max(To.Z, GroundAt(To.X, To.Y) + 1200.0);
		Cmd K = Steer(C, To, Bv + Rb * (Side * Swing * Pi / Leg * std::cos(Wv)), 0.1, 130.0, C.VMax);
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
			const bool bDue = DueAllOn(E.Id, G.Id) > RunsOn(E.Id, G.Id);
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
			if (DueAllOn(E.Id, G.Id) > RunsOn(E.Id, G.Id) && Off < 2400.0 && C.Spd > 114.0 && C.P.Z > Box.Z - 450.0 && ((Box - C.P).Dot(Ahead(C.Hdg)) > -0.3 * Off || Off > 1000.0))
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
		for (int Key_ : MembersOf(G.Id))
		{
			const Craft& O = Crafts.find(Key_)->second;
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
			if (Dist < 1.6 * (520.0 - 120.0 * C.Skill)) Bear(C, K);
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
			if (Dist < 1.6 * (540.0 - 100.0 * C.Skill)) Bear(C, K);
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

	int Show::DueAllOn(int Victim, int By) const
	{
		const auto It = DueAll.find({ Victim, By });
		return It == DueAll.end() ? 0 : It->second;
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

	bool Show::Inbound(const Event& E) const
	{
		// A fighter of the group whose guns made it going in on one of the group it was made on, close and nose on:
		// his guns are a few seconds from bearing. For a box's gunners: one of the fighters it was made on coming in
		// on that box.
		const int Box = BoxOf(E);
		const int Strag = E.What == "air:lost" ? E.Strag : -1;
		for (const auto& Pair : Crafts)
		{
			const Craft& S = Pair.second;
			if (!S.bFighter || S.Target < 0 || !Live(S) || S.Doing != Role::Fight) continue;
			if (S.Step != Phase::Run && S.Step != Phase::Chase && S.Step != Phase::Turn) continue;
			const Craft* T = Find(S.Target);
			if (!T || !Live(*T)) continue;
			const Vec Rel = T->P - S.P;
			const double D = Len(Rel);
			if (Box >= 0)
			{
				if (S.Group == E.Unit && T->Group == Box && D < 1100.0 && Ahead(S.Hdg, S.Gam).Dot(Rel) > 0.8 * D) return true;
				continue;
			}
			if (S.Group != E.By || S.bScored || T->Group != E.Unit || (Strag >= 0 ? T->StragId != Strag : T->StragId >= 0)) continue;
			if (D < 1400.0 && Ahead(S.Hdg, S.Gam).Dot(Rel) > 0.9 * D) return true;
		}
		return false;
	}

	bool Show::AwaitsGunners(const Held& H) const
	{
		// A fighter the battle gave a box's gunners, its wait run out: it waits on while one of its group is coming in
		// on that box and all but in their reach, a fighter for each of those waiting longest, and never more than
		// 30 s in all.
		const int Box = BoxOf(H.E);
		if (Box < 0 || H.E.What != "air:lost" || AirT - H.Since >= 30.0) return false;
		int Before = 0;
		for (const Held& O : Later) if (O.bForShot && O.E.What == "air:lost" && O.E.Unit == H.E.Unit && BoxOf(O.E) == Box && O.Since < H.Since) Before++;
		int Coming = 0;
		for (int Key_ : MembersOf(H.E.Unit))
		{
			const Craft& S = Crafts.find(Key_)->second;
			if (S.Group != H.E.Unit || !S.bFighter || S.ShotOn != 0 || !Live(S) || S.Doing != Role::Fight || S.TargetGroup != Box) continue;
			// (a leader on his run, or the wingman going in with him)
			const Craft* L = S.Lead == S.Key ? &S : S.Step == Phase::Wing ? Find(S.Lead) : nullptr;
			if (!L || L->Step != Phase::Run || L->Target < 0) continue;
			const Craft* T = Find(L->Target);
			if (!T || T->Group != Box) continue;
			const Vec Rel = T->P - S.P;
			const double D = Len(Rel);
			const double Closing = (Velocity(S) - Velocity(*T)).Dot(Rel) / std::max(1.0, D);
			if (Closing > 20.0 && D - 600.0 < 7.0 * Closing) Coming++;
		}
		return Coming > Before;
	}

	void Show::PilotDive(Craft& C, const Group& G, double Dt)
	{
		// (a raid on troops: a fighter's strafing pass down a lane; AirShowTroops.cpp)
		if (C.bLaneRun)
		{
			PilotStrafe(C, Dt);
			return;
		}
		if (G.Strike == "torpedo")
		{
			PilotTorpedo(C, G, Dt);
			return;
		}
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

	void Show::PilotTorpedo(Craft& C, const Group& G, double Dt)
	{
		const double Floor = std::max(0.0, GroundAt(C.P.X, C.P.Y));
		const double Over = C.P.Z - Floor;
		if (C.Doing != Role::Fall) Check.TorpedoLowest = std::min(Check.TorpedoLowest, Over);
		if (C.bDropped && C.Step == Phase::Run)
		{
			C.Step = Phase::Away;
			C.StepT = 0.0;
		}
		// (her group's torpedoes gone without hers, one made late: no run of her own; round for home with the rest)
		const auto TB = Bands.find(G.Id);
		if (!C.bDropped && C.Step == Phase::Run && TB != Bands.end() && TB->second.bDropped)
		{
			C.Step = Phase::Egress;
			C.StepT = 0.0;
		}
		switch (C.Step)
		{
		case Phase::Run:
		{
			// Turned in: her place abreast at the height drawn (wave-top from 3 km out, Heights), slowing over the second
			// half of the run to her type's own speed for the drop; never under three quarters of wave-top.
			Vec To = SlotPlace(G, C.Slot);
			const Vec F = Ahead(C.Hdg);
			const double Ahead1 = std::max(GroundAt(C.P.X + F.X * 400.0, C.P.Y + F.Y * 400.0), GroundAt(C.P.X + F.X * 900.0, C.P.Y + F.Y * 900.0));
			const double Own = TorpedoSpeed(C.Model);
			const double Near = std::hypot(C.P.X - TargetX, C.P.Y - TargetY);
			const double Slow = std::max(Clamp((G.Run - 0.45) / 0.35, 0.0, 1.0), Clamp((4500.0 - Near) / 2000.0, 0.0, 1.0));
			// (from the second half of the run, wave-top whatever her place: the drop is from there)
			To.Z = Slow > 0.0 ? std::max(Floor, Ahead1) + TorpedoLowM : std::max({ To.Z, Floor + TorpedoLowM, Ahead1 + TorpedoLowM });
			Cmd K = Steer(C, To, GroupVelocity(G.Id), 0.12, Own * 0.95, C.VMax);
			K.Spd = Clamp(K.Spd, Own * 0.95, Lerp(C.VMax, Own, Slow));
			if (Over < TorpedoLowM * 0.75) K.Gam = std::max(K.Gam, 2.0 * Deg);
			Fly(C, K, Dt, true);
			break;
		}
		case Phase::Away:
		{
			// Her torpedo gone: straight on, low, past the ships (over a ship's masts if one is in her way), opening up to
			// full speed; never a climb in front of their guns. A minute of it, then round for home.
			double Least = TorpedoAwayM;
			for (const Ship& S : ShipsNow)
				if (std::hypot(S.At.X - C.P.X, S.At.Y - C.P.Y) < 350.0) Least = TorpedoHopM;
			Cmd K;
			K.Hdg = C.RunHdg;
			K.Gam = Clamp((Floor + Least - C.P.Z) / 150.0, -0.08, 0.08);
			K.Spd = C.VMax;
			Fly(C, K, Dt, true);
			if (C.StepT > 45.0)
			{
				C.Step = Phase::Egress;
				C.StepT = 0.0;
			}
			break;
		}
		case Phase::Egress:
		default:
		{
			// Round for home after her group, low, in a gentle turn (no more than 30 degrees of bank); then her place again,
			// the group drawn low (Heights).
			const Vec To = SlotPlace(G, C.Slot);
			Cmd K = Toward(C, Vec(To.X, To.Y, C.P.Z), C.VMax);
			Ease(C, K, 30.0 * Deg);
			K.Gam = Clamp((Floor + std::max(TorpedoAwayM, To.Z - Floor) - C.P.Z) / 200.0, -0.08, 0.05);
			Fly(C, K, Dt, true);
			if (C.StepT > 90.0 || Flat(To - C.P) < 1500.0)
			{
				C.Doing = Role::Slot;
				C.Step = Phase::None;
				C.bJoining = true;
				C.bRunDone = true;
			}
			break;
		}
		}
	}

	double Show::Chasing(const Craft& C) const
	{
		// A fighter after a torpedo plane flying low over the sea: how high that plane is over it (0: not so). He may
		// follow her down to wave-top, as the Zeros did at Midway, and is not held to a fighter's 60 m in a fight.
		if (!C.bFighter || C.Target < 0 || GroundAt(C.P.X, C.P.Y) > 1.0) return 0.0;
		const Craft* Q = Find(C.Target);
		if (!Q || Q->bFighter) return 0.0;
		const auto QG = Now.find(Q->Group);
		if (QG == Now.end() || QG->second.Strike != "torpedo") return 0.0;
		const double QOver = Q->P.Z - GroundAt(Q->P.X, Q->P.Y);
		return QOver < 300.0 ? std::max(1.0, QOver) : 0.0;
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
		// (its bank as it was: no fall rolls faster than the eye can follow, below)
		const double Bank0 = C.Bank;
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
				// A helix: round its middle line as that comes down. Steep (looks 0, 1): the nose 60 to 80 degrees down;
				// flat (2, 3): 20 to 40 degrees down. A fighter winds down one turn every two or three seconds (120 to
				// 160 degrees a second), a bomber slower and heavier (36 to 54), the turn building over its first second
				// (the audit's 4.3).
				const bool bSteep = C.Look < 2;
				const double Turns = (C.bHeavy ? (bSteep ? 0.1 + 0.05 * C.Look : 0.15) : (bSteep ? 0.34 + 0.08 * C.Look : 0.4 + 0.05 * (C.Look - 2)))
				                   * Clamp(C.FallT / 1.0, 0.15, 1.0);
				const double Radius = bSteep ? 6.0 + C.Form.Span * 0.4 : 14.0 + C.Form.Span * 0.8;
				C.Spin += C.Turn * Turns * 2.0 * Pi * Dt;
				C.Axis += C.FallV * Dt;
				C.P = C.Axis + Vec(std::sin(C.Spin), std::cos(C.Spin), 0.0) * Radius;
				C.Hdg = Wrap(C.Spin + C.Turn * Pi * 0.5);
				const double Nose = bSteep ? 60.0 + 10.0 * C.Look : 20.0 + 15.0 * (C.Look - 2);
				C.Gam += (-Nose * Deg - C.Gam) * std::min(1.0, Dt * 1.5);
				C.Bank += Clamp(C.Turn * 40.0 * Deg - C.Bank, -60.0 * Deg * Dt, 60.0 * Deg * Dt);
				C.Spd = Turns * 2.0 * Pi * Radius;
			}
			else if (C.Falling == FallHow::Wing)
			{
				// A wing gone: rolling toward it, hard but readably, at most about one and a half rolls a second a fighter
				// (360 to 500 degrees a second), a third of one a bomber, in a fast spiral, the nose well down; the roll
				// building over its first 0.8 s (the audit's 4.3).
				const double Rolls = (C.bHeavy ? 0.25 + 0.05 * C.Look : 1.0 + 0.13 * C.Look) * Clamp(C.FallT / 0.8, 0.1, 1.0);
				C.Bank = Wrap(C.Bank + C.Turn * Rolls * 2.0 * Pi * Dt);
				C.Hdg = Wrap(C.Hdg + C.Turn * 0.9 * Dt);
				C.Gam += (-65.0 * Deg - C.Gam) * std::min(1.0, Dt * 1.2);
				C.P += C.FallV * Dt;
				C.Spd = Len(C.FallV);
			}
			else
			{
				// A fighter's tail gone: the nose pitches down hard, once, over past the vertical, then it plunges in a
				// steep dive, wavering (the audit's 4.3: one pitch-over, not a tumble end over end).
				const double Dive = (-80.0 + 6.0 * std::sin(C.FallT * 2.3)) * Deg;
				if (C.Spin < 1.0) C.Gam -= 150.0 * Deg * Dt; // (the pitch-over: from its line on down to 110 degrees)
				if (C.Gam < -110.0 * Deg) C.Spin = 1.0;
				if (C.Spin >= 1.0) C.Gam += Clamp(Dive - C.Gam, -60.0 * Deg * Dt, 60.0 * Deg * Dt);
				C.Bank += Clamp(-C.Bank, -90.0 * Deg * Dt, 90.0 * Deg * Dt);
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
		// (no fall rolls faster than the eye can follow: a wing-off roll at most 540 degrees a second, any other 300;
		// eased in, at most 120 a second, over its first 0.75 s; never over 10 degrees in a sixtieth of a second)
		if (Dt > 0.0)
		{
			const double Most = (C.FallT < 0.75 ? 120.0 : C.Falling == FallHow::Wing ? 540.0 : 300.0) * Deg * Dt;
			C.Bank = Wrap(Bank0 + Clamp(Wrap(C.Bank - Bank0), -Most, Most));
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
				// (one begun low, a torpedo plane at wave-top, is in the sea at once: sooner than the battle's time for a
				// fall from its band, which it cannot take; the loss itself is the battle's. So is one begun too low for
				// the battle's time even at the slowest an aeroplane comes down, 8 m/s, a long glide out of a fight down
				// low: it took as long as it could, the design director's (a))
				if (C.FallT < C.Dur && (C.FallHigh < LowFallM || C.FallT >= (C.FallHigh - Floor) / 8.0 - 6.0)) Check.FallsLow++;
				// (drawn higher than the battle's band, the fall takes as long as an aeroplane takes from there, never
				// steeper: VTop, the design director's (a))
				else if (C.FallT > C.Dur && C.FallT <= C.FallHigh / std::max(1.0, C.VTop * 0.95) + 6.0) Check.FallsTrue++;
				else
				{
					(Off <= std::max(6.0, 0.25 * C.Dur) ? Check.FallsOnTime : Check.FallsOffTime) += 1;
					Check.MostOffTime = std::max(Check.MostOffTime, Off);
				}
			}
			if (bStruck)
			{
				// A ditching (a glide onto the sea under control): a skip and a long spray, then it floats a while.
				const bool bDitching = C.Falling == FallHow::Glide && Floor <= 0.5;
				const bool bBelly = C.Falling == FallHow::Glide && Floor > 0.5;
				Fx& End = Emit(Floor > 0.5 ? FxKind::Crash : FxKind::Splash, Vec(C.P.X, C.P.Y, Floor), bDitching ? "ditch" : bBelly ? "belly" : "plane");
				End.Size = C.Form.Span;
				// (the way it was going as it struck, as its last FallMove has it: the scene's fireball keeps a little)
				End.Vel = Moved;
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

	int Show::PickToFall(const Event& E, int Victim)
	{
		// (the one the burst that carried this loss struck)
		if (Victim >= 0)
		{
			const Craft* V = Find(Victim);
			if (V && V->Group == E.Unit && Live(*V)) return Victim;
		}
		std::vector<int> Keys = LiveOf(E.Unit);
		if (Keys.empty()) return -1;
		if (E.Strag >= 0)
		{
			for (int K : Keys) if (Find(K)->StragId == E.Strag) return K;
		}
		// (not one another burst is bringing down now, while any other is left: that one falls to its own burst)
		{
			std::vector<int> Free;
			for (int K : Keys) if (Find(K)->ShotOn == 0) Free.push_back(K);
			if (!Free.empty()) Keys = Free;
		}
		// (nor one going in on a box for its gunners to have, while any other is left)
		{
			std::vector<int> Free;
			for (int K : Keys) if (!Find(K)->bPress) Free.push_back(K);
			if (!Free.empty()) Keys = Free;
		}
		// The one the guns of the group that brought it down are on now; else the one they were last on, within 25 s
		// (hit on the last pass, it goes down now); else one a fighter of that group is closing on.
		if (E.By >= 0)
		{
			// More of that group's guns' work on this unit still waiting to be shown: the aircraft one of its pilots is
			// going in on now is left for him (his burst will carry what waits, and be seen to), and this loss, which no
			// gun came to bear for in its time, goes to one nobody is on, the farthest from that group's fighters.
			bool bMore = false;
			if (Victim < 0) for (const Held& H : Later) bMore = bMore || (H.bForShot && H.E.Unit == E.Unit && H.E.By == E.By && BoxOf(H.E) < 0);
			std::set<int> Kept;
			if (bMore)
			{
				for (const auto& Pair : Crafts)
				{
					const Craft& Shooter = Pair.second;
					if (Shooter.Group != E.By || Shooter.Target < 0 || !Live(Shooter) || Shooter.Doing != Role::Fight || Shooter.bFiring) continue;
					if (Shooter.Step != Phase::Run && Shooter.Step != Phase::Turn && Shooter.Step != Phase::Chase && Shooter.Step != Phase::Wing) continue;
					const Craft* V = Find(Shooter.Target);
					if (V && V->Group == E.Unit && Len(V->P - Shooter.P) < 1500.0) Kept.insert(V->Key);
				}
			}
			int Best = -1;
			double BestScore = 1e8;
			for (int K : Keys)
			{
				const Craft& V = *Find(K);
				if (V.Doing == Role::Ground || V.StragId >= 0 || Kept.count(K)) continue;
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
			if (bMore)
			{
				// (a fighter already hit and out of the fight first; never one whose own burst is carrying a hit now)
				double Far = -1.0;
				for (int K : Keys)
				{
					const Craft& V = *Find(K);
					if (V.Doing == Role::Ground || V.StragId >= 0 || Kept.count(K) || V.SightOn >= 0) continue;
					bool bBusy = false;
					for (const Shot& Each : Booked) bBusy = bBusy || (!Each.bDone && Each.Shooter == K);
					if (bBusy) continue;
					double D = 1e5;
					for (const auto& Pair : Crafts)
					{
						const Craft& F = Pair.second;
						if (F.Group == E.By && Live(F) && F.Doing == Role::Fight) D = std::min(D, Len(F.P - V.P));
					}
					if (V.bHurt || V.Step == Phase::Away) D += 1e6;
					if (D > Far) { Far = D; Best = K; }
				}
				if (Best >= 0) return Best;
			}
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
		// (no longer flying as it was: where it will be is not known to any gun until its way down has told)
		C.GunErr = 9.0;
		C.PastT = -1.0;
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
		// (the bomber by name the battle says this is, for her nose art: read by the scene only)
		C.NamedArt = E.BomberArt;
		C.Look = std::clamp(E.Look, 0, 3);
		C.Turn = E.Roll < 0 ? -1.0 : E.Roll > 0 ? 1.0 : (E.Hand < 0 ? -1.0 : E.Hand > 0 ? 1.0 : C.Hand);
		// (every bomber falls as a bomber, slow and heavy: the Kate, the Val and the Dauntless too, the audit's 4.3)
		C.bHeavy = !C.bFighter;
		C.FireAt = E.FireAt;
		C.BoomAt = E.Boom;
		C.bDitch = E.bDitch;
		C.bLit = false;
		C.Ended = -1.0;
		C.Spin = 0.0;
		C.FallV = Velocity(C);
		C.FallHigh = C.P.Z - GroundAt(C.P.X, C.P.Y);
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
		if (E.bByFlak) FlakKill(C); // (under a ship's guns as she brings it down; else the one burst laid on it: this one falls to it)
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

	void Show::HitSurvived(const Event& E, int Victim, const Vec* Spot)
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
		// (the one the burst that carried this hit struck, before any of those)
		for (size_t K = 0; K < Flying.size(); ++K) if (Flying[K] == Victim) Pick = K;
		Craft& C = *Find(Flying[Pick]);
		const bool bStruck = C.Key == Victim && Spot != nullptr;
		Check.HitsShown++;
		if (!bStruck) C.Struck++;
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
		// A hit laid to guns shows the strikes of the burst that carried it, where its rounds landed, and nothing
		// more (the gunnery: no strike is drawn that no round was seen to make). Flak's shows the shell's burst close
		// by and its splinters going in, told to the kit as splinters, not as rounds (FlakOn). One that nothing was
		// firing to make shows its mark alone.
		if (!bStruck && !ForGuns(E))
		{
			if (E.bByFlak)
			{
				FlakOn(C, E);
				Check.HitsToFlak++;
			}
			else Check.HitsToNobody++;
		}
		if (E.Mark == "pieces")
		{
			// A few panels torn off where the rounds landed, fluttering away and turning flat.
			const int Count = 2 + static_cast<int>(Roll.U() * 2.0);
			for (int K = 0; K < Count; ++K)
			{
				const Vec From = bStruck ? *Spot + Vec(Roll.U(-0.6, 0.6), Roll.U(-0.6, 0.6), 0.3)
				                         : Vec(Roll.U(-0.3, 0.3) * C.Form.Long, Roll.U(-0.4, 0.4) * C.Form.Span, 0.3);
				Piece(C, 5 + K, Local(C, From),
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
				C.DeadEngines.assign(1, Engine); // (not "= { Engine }": GCC 13 under the sanitizers takes that for a read past its end, -Warray-bounds)
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
			// (the modeller's of 23 November: the Wellington's nine 500 lb bombs; the Pe-2's four from her fuselage bay
			// (her nacelles' two are not drawn), not the one a span under 18 m gives, which let one bomb go through her
			// shut doors; the LeO 451 takes her span's six)
			{ "wellington", 9 }, { "pe2", 4 },
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
		if (!C.bRunHeld) C.RunHdg = C.Hdg;
		if (bTorpedo)
		{
			Check.TorpedoDropAt.push_back(C.P.Z - GroundAt(C.P.X, C.P.Y));
			Check.TorpedoDropSpeed.push_back(C.Spd);
			Check.TorpedoDropModel.push_back(C.Model);
		}
		const auto G = Now.find(C.Group);
		C.bStickBay = !C.bFighter && !bTorpedo && C.Doing != Role::Dive && C.StickSize > 1 && (G == Now.end() || G->second.Strike != "dive");
		C.StickNext = C.bStickBay && C.Bay < 1.0 ? AirT + (1.0 - C.Bay) * BayOpenS + 0.1 : AirT;
		Check.Sticks++;
		Bays(0.0);
	}

	void Show::DropOnLead()
	{
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			if (C.DropDue < 0.0 || AirT < C.DropDue) continue;
			C.DropDue = -1.0;
			// (shot down or gone before she saw his bombs: hers go with her, as in Bays)
			if (C.bDropped || C.Doing == Role::Fall || C.Doing == Role::Gone || C.Doing == Role::Ground) continue;
			Drop(C, false);
		}
	}

	const Show::Craft* Show::BoxLead(int GroupId) const
	{
		const Craft* Best = nullptr;
		const auto M = Members.find(GroupId);
		if (M == Members.end()) return nullptr;
		for (int Key : M->second)
		{
			const Craft& C = Crafts.find(Key)->second;
			// (not one still joining: made in the place of one lost, she is not yet with the box)
			if (C.bFighter || C.Doing != Role::Slot || C.bJoining) continue;
			if (!Best || C.Slot < Best->Slot || (C.Slot == Best->Slot && C.Key < Best->Key)) Best = &C;
		}
		return Best;
	}

	double Show::BoxTurnRate(int GroupId) const
	{
		// (on its lead while it turns for home, RallyFrame: his own turn; else its heading's between the views either side
		// of now, as Interpolate turns it)
		Vec At, Vel;
		double Face = 0.0;
		if (RallyFrame(GroupId, At, Face, Vel))
		{
			const Craft* L = BoxLead(GroupId);
			return L ? L->TurnNow : 0.0;
		}
		const View* A = nullptr;
		const View* B = nullptr;
		for (const View& V : Views)
		{
			if (V.T <= ShownT) A = &V;
			else { B = &V; break; }
		}
		if (!A || !B || B->T <= A->T) return 0.0;
		const auto Ga = A->Groups.find(GroupId), Gb = B->Groups.find(GroupId);
		if (Ga == A->Groups.end() || Gb == B->Groups.end()) return 0.0;
		return Wrap(Gb->second.Face - Ga->second.Face) / ((B->T - A->T) * Scale);
	}

	bool Show::RallyFrame(int GroupId, Vec& At, double& Face, Vec& Vel) const
	{
		const auto Box = Bands.find(GroupId);
		// (from the box's bombs, while it flies on wings level on his heading, through its turn, until it has rallied)
		const auto G = Now.find(GroupId);
		if (Box == Bands.end() || !Box->second.bDropped || Box->second.bRallied) return false;
		if (G == Now.end() || G->second.bFighter || G->second.Strike == "dive" || G->second.Strike == "torpedo") return false;
		const Craft* L = BoxLead(GroupId);
		if (!L) return false;
		At = L->P;
		Face = L->Hdg;
		Vel = Velocity(*L);
		return true;
	}

	bool Show::Rally(Craft& C, const Group& G, Band& B, double Dt)
	{
		if (G.bFighter || G.Strike == "dive" || G.Strike == "torpedo" || B.bRallied) return false;
		const Craft* L = BoxLead(C.Group);
		if (!L) return false;
		const double Home = G.Face;
		if (B.RallyAt < 0.0)
		{
			// It begins as the lead rolls into his turn, once the box's bombs have all gone: every stick let go and
			// held StickSteady after its last bomb (at most 15 air seconds after his first). Till then the box flies
			// on, wings level on his heading, each as her own stick lets her.
			if (!L->bDropped) return false;
			bool bAllGone = L->StickLeft == 0 && L->StickDone >= 0.0 && AirT >= L->StickDone + StickSteady;
			for (const auto& Pair : Crafts)
			{
				const Craft& X = Pair.second;
				if (!bAllGone) break;
				if (X.Group != C.Group || !Live(X)) continue;
				if (X.DropDue >= 0.0 || X.StickLeft > 0 || (X.StickDone >= 0.0 && AirT < X.StickDone + StickSteady)) bAllGone = false;
			}
			if (!bAllGone && B.LeadBombAt >= 0.0 && AirT < B.LeadBombAt + 15.0)
			{
				// (wings level: her heading is his run's, held from her doors full open; chasing a degree of it would
				// bank her seven, and she would roll into his turn from the wrong side)
				Cmd K;
				K.Hdg = L->RunHdg;
				K.bBank = true;
				K.Bank = 0.0;
				K.Gam = 0.0;
				K.Spd = C.Spd;
				Fly(C, K, Dt, true);
				return true;
			}
			if (!bAllGone && (L->StickLeft > 0 || L->StickDone < 0.0 || AirT < L->StickDone + StickSteady)) return false;
			B.RallyAt = AirT;
			// The way round, picked once: the short way to his heading for home; the battle's formation reversed
			// (nearly a half circle either way), toward the side its place for him now lies.
			const double Turn = Wrap(Home - L->Hdg);
			const Vec Place = SlotPlace(G, L->Slot);
			const double Side = RightOf(L->Hdg).Dot(Vec(Place.X - L->P.X, Place.Y - L->P.Y, 0.0));
			B.RallyWay = std::fabs(Turn) < 150.0 * Deg ? (Turn >= 0.0 ? 1.0 : -1.0) : (Side >= 0.0 ? 1.0 : -1.0);
			Check.Rallies++;
		}
		const double Left = Wrap(Home - L->Hdg);
		// (one turn: once he has rolled out on his heading for home it is done, and what is left is closing on his place)
		if (std::fabs(Left) <= 3.0 * Deg || AirT - B.RallyAt >= 300.0) B.bTurnDone = true;
		const bool bTurning = !B.bTurnDone;
		if (!bTurning && Len(SlotPlace(G, L->Slot) - L->P) < RallyCloseM)
		{
			B.bRallied = true;
			Check.RallyOver[C.Group] = AirT;
			return false;
		}
		if (C.Key == L->Key)
		{
			// The lead: rolled into a steady turn his box can hold, his own way round, and out of it onto his heading
			// for home; then on to his place in the battle's formation, a little faster than it and banking gently (2
			// degrees for each degree off, at most his rally bank), so that the rest, keeping on him, can hold him.
			Cmd K;
			K.bBank = true;
			if (bTurning)
			{
				K.Bank = std::fabs(Left) > 12.0 * Deg ? B.RallyWay * RallyLeadBank * Deg : Clamp(Left * 2.0, -RallyLeadBank * Deg, RallyLeadBank * Deg);
				K.Gam = Clamp((G.Alt - C.P.Z) * 0.001, -0.05, 0.05);
				K.Spd = C.Spd;
			}
			else
			{
				const Vec Place = SlotPlace(G, L->Slot);
				const double Want = std::atan2(Place.X - C.P.X, Place.Y - C.P.Y);
				const Vec GV = GroupVelocity(C.Group);
				K.Bank = Clamp(Wrap(Want - C.Hdg) * 2.0, -RallyLeadBank * Deg, RallyLeadBank * Deg);
				K.Gam = Clamp((Place.Z - C.P.Z) * 0.001, -0.05, 0.05);
				K.Spd = Clamp(std::hypot(GV.X, GV.Y) + 12.0, C.VMin, C.VMax * 0.9);
			}
			Fly(C, K, Dt, true);
			if (bTurning && AirT > B.RallyAt + 3.0) Check.RallyBankSeen = std::max(Check.RallyBankSeen, std::fabs(C.Bank) / Deg);
			return true;
		}
		// The rest: each her place in the box laid about the lead as he flies (its frame his heading), and his way
		// with it as it swings about him.
		Group Ref = G;
		Ref.Face = L->Hdg;
		const Vec Off = L->P - SlotPlace(Ref, L->Slot);
		const Vec To = SlotPlace(Ref, C.Slot) + Off;
		const double Omega = std::fabs(L->Bank) < 80.0 * Deg ? Gee * std::tan(L->Bank) / std::max(L->Spd, 30.0) : 0.0;
		const Vec R = To - L->P;
		const Vec ToVel = Velocity(*L) + Vec(Omega * R.Y, -Omega * R.X, 0.0);
		// (her bank as Fly would ask it for the heading Steer gives, but never past a box's rally bank: a light bomber
		// too, Kate or Val, keeps her turn in her box gentle)
		// (and within a little of his speed while he turns: chasing her place flat out, she would swing wide of him)
		const double Most = std::min(C.VMax, L->Spd + (bTurning ? 8.0 : 15.0)), Least = std::min(Most, std::max(C.VMin, L->Spd - 10.0));
		Cmd K = Steer(C, To, ToVel, 0.12, Least, Most);
		const double S = std::max(C.Spd, 30.0);
		const double Want = std::atan(S * Clamp(Wrap(K.Hdg - C.Hdg) * 0.8, -S / C.RMin, S / C.RMin) / Gee);
		K.bBank = true;
		K.Bank = Clamp(Want, -RallyBankMost * Deg, RallyBankMost * Deg);
		// (in the turn every wing of the box goes down the lead's way, as a box turned: one ahead of her place or inside
		// it eases her bank toward level and her throttle back, she never rolls against him; closing on his place after
		// it, her wings follow his, within a few degrees of level while his are)
		if (bTurning) K.Bank = B.RallyWay > 0.0 ? std::max(0.0, K.Bank) : std::min(0.0, K.Bank);
		else if (L->Bank > 2.0 * Deg) K.Bank = std::max(0.0, K.Bank);
		else if (L->Bank < -2.0 * Deg) K.Bank = std::min(0.0, K.Bank);
		else K.Bank = Clamp(K.Bank, -4.0 * Deg, 4.0 * Deg);
		Fly(C, K, Dt, true);
		if (bTurning)
		{
			Check.RallySteps++;
			// (one still joining, made where the battle's own formation has her place, is not yet in the box's turn)
			if (C.Bank * B.RallyWay < -5.0 * Deg && !C.bJoining) Check.RallyAgainst++;
			// (from 3 s into it: one that came into it hunting her place at a steeper bank has rolled out of that by then)
			if (AirT > B.RallyAt + 3.0) Check.RallyBankSeen = std::max(Check.RallyBankSeen, std::fabs(C.Bank) / Deg);
		}
		return true;
	}

	Vec Show::BombStation(const std::string& Model, int K, int Size) const
	{
		const double Side = (K % 2) ? 1.0 : -1.0;
		const auto Known = BaysOf.find(Model);
		if (Known == BaysOf.end()) return Vec(0.0, Side * 0.25, -1.2);
		const BayPlace& B = Known->second;
		// (two rows, the bombs of each pair side by side, the pairs from the bay's fore end to its aft one)
		const int Rows = std::max(1, (Size + 1) / 2);
		const double Share = (std::clamp(K / 2, 0, Rows - 1) + 0.5) / Rows;
		return Vec(B.Fore + (B.Aft - B.Fore) * Share, Side * B.Half, B.Up);
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
			// (one made after her box has let its sticks go, closing up a gap in it, has no run to fly: the box is still
			// inside the ring, and she opened her doors on the run already flown, held her heading for it, and turned
			// away after with never a stick let go)
			// (and while she waits to let hers go on her lead's: her doors stay open, her heading held)
			const auto Box = Bands.find(C.Group);
			const bool bBoxDone = Box != Bands.end() && Box->second.bDropped;
			const bool bOnRun = bLevel && !C.bDropped && ((!bBoxDone && Run >= 1.0 - Lead && Run > 0.0) || C.DropDue >= 0.0);
			// (held open through the stick; as its last bomb leaves her bombardier closes them, shut BayShutS later, while
			// she still flies wings level on her heading StickSteady more, and then she rolls into her box's rally turn)
			const bool bWant = (bLevel && bOnRun) || (C.bStickBay && C.StickLeft > 0);
			if (Dt > 0.0) C.Bay = bWant ? std::min(1.0, C.Bay + Dt / std::max(0.1, BayOpenS)) : std::max(0.0, C.Bay - Dt / std::max(0.1, BayShutS));
			// (her doors full open on her run: from now her heading is held until her stick has gone, Pilot)
			if (bOnRun && C.Bay >= 1.0 && !C.bRunHeld) { C.bRunHeld = true; C.RunHdg = C.Hdg; }
			if (!bOnRun && !C.bDropped) C.bRunHeld = false;
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
				B.At = Local(C, BombStation(C.Model, K, C.StickSize)) - V * Ago + Vec(0.0, 0.0, -0.5 * Gee * Ago * Ago);
				B.Vel = V + Vec(0.0, 0.0, -Gee * Ago);
				B.Size = C.bStickTorpedo ? 5.5 : C.Form.Span > 25.0 ? 2.2 : 1.8;
				B.Heading = C.Hdg;
				// (a raid on troops: each bomb its number, and laid clear of every piece; AirShowTroops.cpp)
				if (Column.Has() && !B.bTorpedo)
				{
					B.Group = C.Group;
					LayBomb(B);
				}
				Small.push_back(B);
				Check.StickBombs++;
				if (K == 0 && C.bStickBay) C.StickBegan = AirT;
				// (her first bomb against her box's lead's: after his, by her own moment; his own at his)
				const auto Mine = K == 0 ? Bands.find(C.Group) : Bands.end();
				if (Mine != Bands.end() && Mine->second.LeadBombAt >= 0.0)
				{
					const double After = C.StickNext - Mine->second.LeadBombAt;
					if (After < -1e-6) Check.DropsBeforeLead++;
					else if (After > 1e-6)
					{
						Check.DropsOnLead++;
						Check.OnLeadLeast = std::min(Check.OnLeadLeast, After);
						Check.OnLeadMost = std::max(Check.OnLeadMost, After);
					}
				}
				if (C.bStickBay && C.Bay < 0.9) Check.BombsThroughShutDoors++;
				C.StickLeft--;
				C.StickNext += StickGap;
				if (C.StickLeft == 0) C.StickDone = AirT;
			}
		}
	}

	void Show::Play(const Event& E)
	{
		// (a raid on troops: a hit on a piece of the column, held for the bomb or the rounds that carry it, as a ship's
		// hit is held for its bombs; AirShowTroops.cpp)
		if (Column.Has() && E.What == "air:mark-hit")
		{
			PlayMark(E);
			return;
		}
		const bool bShips = E.What == "air:ship-hit" || E.What == "air:ship-sunk";
		if (bShips)
		{
			// Held until the bombs or the torpedoes could have reached her: the stage shows the hit as the bombs land.
			const double Wait = E.Strike == "level" ? 26.0 : E.Strike == "dive" ? 30.0 : E.Strike == "torpedo" ? 14.0 : 20.0;
			Later.push_back(Held{ E, AirT + Wait, false });
			return;
		}
		if (E.What == "end") bEnded = true;
		// A straggler's later news must find her: her falling out of the box, still waiting on its burst, shows first.
		if (E.Strag >= 0 && E.What != "air:straggle")
		{
			for (size_t K = 0; K < Later.size(); ++K)
			{
				if (!Later[K].bForShot || Later[K].E.What != "air:straggle" || Later[K].E.Unit != E.Unit || Later[K].E.Strag != E.Strag) continue;
				const Held Out_ = Later[K];
				Later.erase(Later.begin() + static_cast<long>(K));
				ReleaseUnstruck(Out_);
				break;
			}
			for (size_t K = 0; K < Booked.size(); ++K)
			{
				if (Booked[K].bDone || Booked[K].E.What != "air:straggle" || Booked[K].E.Unit != E.Unit || Booked[K].E.Strag != E.Strag) continue;
				const Shot S = Booked[K];
				Booked[K].bDone = true;
				if (Craft* V = Find(S.Victim)) if (V->ShotOn == S.Id) V->ShotOn = 0;
				if (Moment* M = MomentOf(S.Id)) { M->Stage = 2; M->FallAt = AirT; M->Ended = AirT; }
				(S.First >= 0.0 ? Check.HitsStruck : Check.HitsUnseen)++;
				Release(S.E, S.Victim, &S.Spot, false);
			}
		}
		// What guns did waits for its burst (the gunnery): an aircraft lost to a fighter group's guns or to a box's
		// gunners, a hit survived, a bomber shot out of her box. It is held until a burst of those guns can be seen to
		// strike it: a loss 20 to 24 s (longer as a fight opens, and for a heavy bomber, which took its time to fall
		// out of its box), a hit 10, a straggler 12. The battle's count is the same; only the moment it shows is the
		// scene's. Nothing is held once the battle's end is told (the wait below lets it go at once).
		// A model falls only for its share of the losses (AR1): a loss that brings no model down, a straggler not shown
		// falling out, is told (the dispatch, the tag) and never shown; no gun is sent to it, nothing is owed the box.
		if ((E.What == "air:lost" || E.What == "air:straggle") && E.Models <= 0)
		{
			(E.What == "air:lost" ? Check.LostUnshown : Check.StragUnshown)++;
			if (E.What == "air:straggle" && E.Strag >= 0) StragsUnshown.insert({ E.Unit, E.Strag });
			Release(E);
			return;
		}
		// (a loss of several, the group's last: the rest of its models fall with it, and are no models too many meanwhile)
		if (E.What == "air:lost" && E.Models > 1) Bands[E.Unit].Extra += E.Models - 1;
		Event Ev = E;
		if (Ev.What == "air:hit" && Ev.By < 0 && !Ev.bByFlak && Ev.GunBox < 0)
		{
			// (a recording from before the battle said whose guns: the enemy fighters firing hardest on its group now;
			// none, and the guns round the target firing on it: flak's, shown as flak's)
			auto Whose = [&Ev](const std::map<int, Group>& Groups)
			{
				double Most = 0.0;
				for (const auto& Pair : Groups)
				{
					if (!Pair.second.bFighter) continue;
					for (const Fire& F : Pair.second.Firing) if (F.On == Ev.Unit && F.Rate > Most) { Most = F.Rate; Ev.By = Pair.first; }
				}
				const auto Its = Groups.find(Ev.Unit);
				if (Ev.By < 0 && Its != Groups.end() && !Its->second.Flak.empty()) Ev.bByFlak = true;
			};
			Whose(Now);
			// (the first hit of a fight or of the flak: the view that tells of it is the first to say who was firing)
			for (const View& V : Views)
			{
				if (Ev.By >= 0 || Ev.bByFlak) break;
				if (V.T >= Ev.T - 1e-9 && V.T > ViewT) { Whose(V.Groups); break; }
			}
		}
		if (ForGuns(Ev))
		{
			const bool bLost = Ev.What == "air:lost";
			double Wait = Ev.What == "air:straggle" ? 12.0 : 10.0;
			if (bLost)
			{
				// (longer as a fight opens, while the first of them are still coming in)
				auto Opening = [this](int Id)
				{
					auto It = Bands.find(Id);
					return It == Bands.end() || It->second.FightAt < 0.0 || AirT - It->second.FightAt < 25.0;
				};
				const auto Lost = Now.find(Ev.Unit);
				const bool bHeavy = Lost != Now.end() && !Lost->second.bFighter;
				Wait = bHeavy ? 24.0 : Opening(Ev.By) || Opening(Ev.Unit) ? 22.0 : 20.0;
				Bands[Ev.Unit].Pending++;
				Check.ToGuns++;
				if (BoxOf(Ev) >= 0) Check.GunnerKills++;
			}
			else Check.HitsToGuns++;
			Held H;
			H.E = Ev;
			H.At = AirT + Wait;
			H.bForShot = true;
			H.Since = AirT;
			Later.push_back(H);
			return;
		}
		Release(Ev);
	}

	void Show::Release(const Event& E, int Victim, const Vec* Spot, bool bMayWait)
	{
		// An aircraft lost with no burst to bring it down (flak's, or one no gun came to bear on in its time), whose own
		// guns fired in the frame just drawn: it goes a frame later, its guns stopped meanwhile (FallDue), so that none
		// of its own rounds is in the air about it as its fall throws it over. The battle's count is the same.
		if (bMayWait && E.What == "air:lost" && !bEnded)
		{
			const int Key = PickToFall(E, Victim);
			const auto Own = Key >= 0 ? Firing.lower_bound(Key * 64) : Firing.end();
			if (Own != Firing.end() && *Own < (Key + 1) * 64)
			{
				Find(Key)->FallDue = AirT;
				// (a model yet to fall, not one too many)
				Bands[E.Unit].Pending++;
				Held H;
				H.E = E;
				H.At = AirT + 1e-9;
				H.Fell = Key;
				Later.push_back(H);
				return;
			}
			if (Key >= 0) Victim = Key;
		}
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
			ShipsDown.insert(E.Ship);
			Fx& S = Emit(FxKind::ShipSunk, At, E.How, E.Ship);
			S.Size = E.bBig ? 1.0 : 0.0;
			S.Text = E.Name;
			return;
		}
		if (E.What == "air:lost")
		{
			if (E.Models <= 0) return;
			int Key = PickToFall(E, Victim);
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
			// (a loss of several models, the group's last: the rest fall with it, the crews' parachutes the first's)
			if (E.Models > 1)
			{
				Event More = E;
				More.Chutes = 0;
				More.ChuteAt.clear();
				More.ChuteCue.clear();
				More.Text.clear();
				// (the bomber the battle named is the first: the rest are others of the group)
				More.BomberArt = -1;
				for (int K = 1; K < E.Models; ++K)
				{
					const int Next = PickToFall(More, -1);
					if (Next < 0) break;
					StartFall(*Find(Next), More);
					Bands[E.Unit].Owed.push_back(AirT);
				}
				Band& Bd = Bands[E.Unit];
				Bd.Extra = std::max(0, Bd.Extra - (E.Models - 1));
			}
			return;
		}
		// (a drop's own cue, the rules' bomb_stick_01, is not played at the drop: the stick is heard where and when it
		// lands, from its first bomb's BombHit)
		if (!E.Cue.empty() && E.What != "air:bombs" && E.What != "air:jettison") Emit(FxKind::Sound, At, E.Cue);
		// (a straggler never shown falling out: her fire put out, her engine caught again, told and not shown)
		if ((E.What == "air:fireout" || E.What == "air:regain") && E.Strag >= 0 && StragsUnshown.count({ E.Unit, E.Strag })) return;
		if (E.What == "air:hit" || E.What == "air:fireout" || E.What == "air:regain")
		{
			HitSurvived(E, Victim, Spot);
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
			if (E.Models <= 0) return;
			// A bomber falls out of the box: one at the box's edge, smoke from the engine it lost.
			// (one in its slot; else, the box being in its run, a bomber diving or in a fight)
			std::vector<int> Keys = LiveOf(E.Unit);
			std::vector<int> Pick;
			// (the one the burst that shot her out struck, before any other)
			if (Victim >= 0 && Find(Victim) && Find(Victim)->Group == E.Unit && Live(*Find(Victim)) && !Find(Victim)->bFighter) Pick.push_back(Victim);
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
				C.GunErr = 9.0;
				C.PastT = -1.0;
				C.StragId = E.Strag;
				C.StragAt = AirT;
				// (the bomber by name the battle says this is, for her nose art: read by the scene only)
				C.NamedArt = E.BomberArt;
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
				// (flak's doing: the shell's burst by her, its splinters into the engine it knocked out)
				if (E.bByFlak)
				{
					FlakOn(C, E);
					Check.HitsToFlak++;
				}
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
		// (a raid on troops: a fighter group's pass down a lane; AirShowTroops.cpp)
		if (Column.Has() && E.What == "air:strafe")
		{
			StrafeBegin(E);
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
			// A level box drops on its lead (Brian, 22 November 04:11Z: "the lead plane always drops the bombs first and
			// the rest follow"): his stick goes now, as the battle says; each of the others' when her bombardier sees
			// his bombs leave, a moment after his first (her own moment, from her key: no draw of the dice). Holding
			// his heading wings level meanwhile, as on her run. (A box jettisoning, a torpedo strike and a fighter
			// group's bombs go at once, as before; so do the bombs of a box with nobody in his place to lead it.)
			const Craft* Lead = !bJettison && Strike != "torpedo" && GroupIt != Now.end() && !GroupIt->second.bFighter ? BoxLead(E.Unit) : nullptr;
			if (Lead)
			{
				Craft& L = *Find(Lead->Key);
				Band& B = Bands[E.Unit];
				const bool bLedAlready = L.bDropped && B.LeadBombAt >= 0.0;
				Drop(L, false);
				// (his first bomb's time: still to come while his doors open, or just gone; each bomb after it StickGap on)
				const double First = bLedAlready ? B.LeadBombAt : L.StickNext - (L.StickSize - L.StickLeft) * StickGap;
				B.LeadBombAt = First;
				for (int Key : Keys)
				{
					Craft& C = *Find(Key);
					if (C.Key == L.Key || C.Doing == Role::Ground || C.bDropped || C.DropDue >= 0.0) continue;
					const double Share = std::fmod(static_cast<double>(static_cast<uint32_t>(C.Key) * 2654435761u) / 4294967296.0, 1.0);
					// (DropOnLeadLateS inside DropOnLeadMost: her stick goes on the first frame at or after her moment, and a
					// frame late must still be inside it)
					C.DropDue = First + DropOnLeadLeast + (DropOnLeadMost - DropOnLeadLeast - DropOnLeadLateS) * Share;
					// (on his heading, as the box flies its run on its lead: none of them drifting off it while she waits)
					C.bRunHeld = true;
					C.RunHdg = L.RunHdg;
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
	// The gunnery.
	//
	// Brian, 21 November: "make sure the gun fire from the planes actually hit the target and do damage when its a
	// hit"; "i can see a gunner on a bomber and its tracers hit a fighter and then it goes down"; "make the right odds
	// if they actually hit so it not always an automatic hit but ... when they do hit a plane or something with gun
	// fire than it shows up on the receiving object".
	//
	// The battle counts every loss and every hit (rulings 217 and 237); nothing here adds one or takes one away. What
	// the scene owes them is that the eye can follow each. So what guns did is held (Play) until a burst of those
	// guns can carry it. That burst is laid true: its rounds fly the effects kit's own flight to the place on the
	// aircraft the battle names, its tracers end there, its strikes wink along it, and only then does the damage
	// begin. Every other burst is a miss and is laid to be seen to miss: clear of every airframe by a margin the eye
	// can read, ended short of an aircraft far beyond its mark, or not fired at all. So no round is drawn through an
	// aircraft that flies on unharmed, and nothing is harmed that no round was seen to strike (but where no gun came
	// to bear in the time a hold allows: the checks count those).

	namespace
	{
		struct Slab
		{
			Vec Mid, Half;
		};
		/** An airframe as a handful of boxes in its own frame: the fuselage, the wing, the tailplane, the fin, and a
		 * nacelle for each engine of a type with more than one. Sway: the room it may yet roll in (the sine of the
		 * angle; RollOf): the wing's and the tailplane's boxes then as deep as their tips sweep, the fin's as wide. */
		int SlabsOf(const Shape& S, Slab Out[10], double Sway = 0.0)
		{
			int N = 0;
			const double L = S.Long, B = S.Body;
			Out[N++] = Slab{ Vec(0.0, 0.0, 0.0), Vec(L * 0.5, B * 0.45, B * 0.5) };
			Out[N++] = Slab{ Vec(L * 0.05, 0.0, 0.0), Vec(L * 0.13, S.Span * 0.5, 0.35 + S.Span * 0.5 * Sway) };
			Out[N++] = Slab{ Vec(-L * 0.44, 0.0, 0.2), Vec(L * 0.06, S.Span * 0.19, 0.2 + S.Span * 0.19 * Sway) };
			const double Fin = B * 0.5 + S.Span * 0.1;
			Out[N++] = Slab{ Vec(-L * 0.43, 0.0, Fin * 0.5), Vec(L * 0.07, 0.25 + Fin * Sway, Fin * 0.5) };
			if (S.Engines.size() > 1)
			{
				const double Back = L * 0.05 - L * 0.13;
				for (size_t K = 0; K < S.Engines.size() && N < 10; ++K)
				{
					const Vec& E = S.Engines[K];
					Out[N++] = Slab{ Vec((E.X + 1.2 + Back) * 0.5, E.Y, E.Z), Vec(std::max(0.5, (E.X + 1.2 - Back) * 0.5), 0.7, 0.7 + std::fabs(E.Y) * Sway) };
				}
			}
			return N;
		}
		/** Whether a point of an airframe's own frame is in one of its boxes, each grown by Grow. */
		bool InSlabs(const Slab* Slabs, int N, const Vec& P, double Grow)
		{
			for (int K = 0; K < N; ++K)
			{
				const Vec D = P - Slabs[K].Mid;
				if (std::fabs(D.X) <= Slabs[K].Half.X + Grow && std::fabs(D.Y) <= Slabs[K].Half.Y + Grow && std::fabs(D.Z) <= Slabs[K].Half.Z + Grow) return true;
			}
			return false;
		}
		/** How far an airframe reaches from its middle along U (its own frame, a metre long). */
		double ReachOf(const Shape& S, const Vec& U, double Sway = 0.0)
		{
			Slab Slabs[10];
			const int N = SlabsOf(S, Slabs, Sway);
			double Most = 0.0;
			for (int K = 0; K < N; ++K)
			{
				const Slab& B = Slabs[K];
				Most = std::max(Most, B.Mid.Dot(U) + B.Half.X * std::fabs(U.X) + B.Half.Y * std::fabs(U.Y) + B.Half.Z * std::fabs(U.Z));
			}
			return Most;
		}
		double Between(const Vec& A, const Vec& B) { return std::acos(Clamp(A.Dot(B), -1.0, 1.0)); }
		/** Whether a straight line from a place in an airframe's own frame, along Way, passes through the airframe
		 * between Near and Far metres along it (each box grown by Grow). */
		bool RayMeets(const Shape& S, const Vec& From, const Vec& Way, double Near, double Far, double Grow)
		{
			Slab Slabs[10];
			const int N = SlabsOf(S, Slabs);
			for (int K = 0; K < N; ++K)
			{
				const Vec O = From - Slabs[K].Mid;
				const double Os[3] = { O.X, O.Y, O.Z }, Ds[3] = { Way.X, Way.Y, Way.Z };
				const double Hs[3] = { Slabs[K].Half.X + Grow, Slabs[K].Half.Y + Grow, Slabs[K].Half.Z + Grow };
				double Lo = Near, Hi = Far;
				for (int A = 0; A < 3 && Lo <= Hi; ++A)
				{
					if (std::fabs(Ds[A]) < 1e-9)
					{
						if (std::fabs(Os[A]) > Hs[A]) Hi = Lo - 1.0;
						continue;
					}
					double Ta = (-Hs[A] - Os[A]) / Ds[A], Tb = (Hs[A] - Os[A]) / Ds[A];
					if (Ta > Tb) std::swap(Ta, Tb);
					Lo = std::max(Lo, Ta);
					Hi = std::min(Hi, Tb);
				}
				if (Lo <= Hi) return true;
			}
			return false;
		}
	}

	namespace
	{
		/** The place on an airframe nearest a point of its own frame (the point itself, inside it). */
		Vec OnAirframe(const Shape& S, const Vec& P)
		{
			Slab Slabs[10];
			const int N = SlabsOf(S, Slabs);
			Vec Best = P;
			double Least = 1e18;
			for (int K = 0; K < N; ++K)
			{
				const Vec D = P - Slabs[K].Mid;
				const Vec& H = Slabs[K].Half;
				const Vec In(Clamp(D.X, -H.X, H.X), Clamp(D.Y, -H.Y, H.Y), Clamp(D.Z, -H.Z, H.Z));
				const double Off = Len(D - In);
				if (Off < Least)
				{
					Least = Off;
					Best = Slabs[K].Mid + In;
				}
			}
			return Best;
		}
	}

	bool InAirframe(const Shape& S, const Vec& P, double Grow)
	{
		// (first the box round all of it)
		const double Fin = S.Body * 0.5 + S.Span * 0.1;
		if (std::fabs(P.X) > S.Long * 0.5 + 1.5 + Grow || std::fabs(P.Y) > S.Span * 0.5 + Grow || P.Z > Fin + Grow || P.Z < -S.Body * 0.5 - 0.7 - Grow) return false;
		Slab Slabs[10];
		const int N = SlabsOf(S, Slabs);
		for (int K = 0; K < N; ++K)
		{
			const Vec D = P - Slabs[K].Mid;
			if (std::fabs(D.X) <= Slabs[K].Half.X + Grow && std::fabs(D.Y) <= Slabs[K].Half.Y + Grow && std::fabs(D.Z) <= Slabs[K].Half.Z + Grow) return true;
		}
		return false;
	}

	Vec RoundAt(const Vec& Muzzle, const Vec& Vel, double Age, double Drag, double Drop)
	{
		// (the effects kit's Where for an air gun's round: it slows by Drag a second, and falls)
		const double K = Drag > 1e-6 ? (1.0 - std::exp(-Drag * Age)) / Drag : Age;
		return Muzzle + Vel * K - Vec(0.0, 0.0, 0.5 * Drop * Age * Age);
	}

	void Show::TrackAll()
	{
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			const double Dt = AirT - C.GunT;
			const bool bAgain = C.GunT >= 0.0 && Dt > 1e-6 && Dt < 0.5;
			// (its nose is its way, but for one tumbling or spinning down: that one by where it was)
			const Vec V = C.Doing == Role::Fall && bAgain ? (C.P - C.GunP) * (1.0 / Dt) : Velocity(C);
			if (bAgain)
			{
				Vec A = (V - C.GunV) * (1.0 / Dt);
				const double L = Len(A), Most = 7.0 * Gee;
				if (L > Most) A = A * (Most / L);
				C.GunA = C.GunA * 0.5 + A * 0.5;
			}
			else C.GunA = Vec();
			// (in a turn held, its acceleration swings round with its path: about the axis of the turn, at the turn's rate)
			const double V2 = V.Dot(V);
			const Vec Axis = V2 > 400.0 ? V.Cross(C.GunA) * (1.0 / V2) : Vec();
			C.GunJ = C.Doing == Role::Fall ? Vec() : Axis.Cross(C.GunA);
			C.GunV = V;
			C.GunP = C.P;
			C.GunT = AirT;
			// How far out its place now is from where its flight a third of a second ago told: an aircraft flying
			// steadily, straight or in a held turn, is found where the rounds are sent; one throwing itself about is not.
			if (!bAgain || C.PastT < 0.0 || AirT - C.PastT > 1.0)
			{
				C.PastT = AirT;
				C.PastP = C.P;
				C.PastV = C.GunV;
				C.PastA = C.GunA;
				C.PastJ = C.GunJ;
				if (!bAgain) C.GunErr = 9.0;
			}
			else if (AirT - C.PastT >= 0.3)
			{
				const double Gone = AirT - C.PastT;
				const double Out_ = Len(C.P - (C.PastP + C.PastV * Gone + C.PastA * (0.5 * Gone * Gone) + C.PastJ * (Gone * Gone * Gone / 6.0))) * (0.09 / (Gone * Gone));
				C.GunErr = C.GunErr >= 9.0 ? Out_ : std::max(Out_, C.GunErr * 0.7);
				C.PastT = AirT;
				C.PastP = C.P;
				C.PastV = C.GunV;
				C.PastA = C.GunA;
				C.PastJ = C.GunJ;
			}
		}
	}

	Vec Show::PlaceIn(const Craft& Q, double T) const
	{
		return Q.P + Q.GunV * T + Q.GunA * (0.5 * T * T) + Q.GunJ * (T * T * T / 6.0);
	}

	Vec Show::ToOwn(const Craft& Q, const Vec& W) const
	{
		const Vec F = Ahead(Q.Hdg, Q.Gam);
		const Vec R0 = RightOf(Q.Hdg);
		const Vec U0 = R0.Cross(F);
		const double Cb = std::cos(Q.Bank), Sb = std::sin(Q.Bank);
		const Vec R = R0 * Cb - U0 * Sb;
		const Vec U = U0 * Cb + R0 * Sb;
		return Vec(W.Dot(F), W.Dot(R), W.Dot(U));
	}

	Vec Show::FromOwn(const Craft& Q, const Vec& L) const
	{
		return Local(Q, L) - Q.P;
	}

	Show::Flight Show::FlightOf(const std::string& Kind) const
	{
		auto It = Rounds.find(Kind);
		if (It == Rounds.end()) It = Rounds.find("hmg");
		return It == Rounds.end() ? Flight() : It->second;
	}

	Show::Course Show::CourseOf(const Craft& Q, double FrameDt, double For)
	{
		// Where it will be, frame by frame, flying on as it is flying: its own flight (Fly) played ahead on a copy of
		// it, by the command it last flew, in the steps the frames to come will fly it by. Fly then holds the aircraft
		// itself to that command until the rounds have landed (HeldTo), so the picture has it where this says, and the
		// rounds end on its skin and nowhere else.
		Course Co;
		Co.Step = FrameDt;
		Craft Copy = Q;
		Copy.HeldTo = 1e18;
		auto Note = [&Co, &Copy, this](double On)
		{
			Pose At;
			At.P = Copy.P;
			// (as she will be drawn then: Advance)
			At.P.Z += LiftAt(Copy, AirT + On);
			At.F = Ahead(Copy.Hdg, Copy.Gam);
			const Vec R0 = RightOf(Copy.Hdg), U0 = R0.Cross(At.F);
			const double Cb = std::cos(Copy.Bank), Sb = std::sin(Copy.Bank);
			At.R = R0 * Cb - U0 * Sb;
			At.U = U0 * Cb + R0 * Sb;
			Co.At.push_back(At);
			Co.Low = std::min(Co.Low, Copy.P.Z - GroundAt(Copy.P.X, Copy.P.Y));
		};
		Note(0.0);
		Co.High = Co.Low;
		if (FrameDt <= 0.0) return Co;
		const int Frames = std::min(400, static_cast<int>(std::ceil(For / FrameDt)) + 1);
		const int Steps = std::max(1, static_cast<int>(std::ceil(FrameDt / 0.05)));
		for (int K = 0; K < Frames; ++K)
		{
			for (int S = 0; S < Steps; ++S)
			{
				if (Copy.Doing == Role::Ground)
				{
					// (one on the ground stands where it is, or rolls on down the field as PilotGround has it: a strafing
					// run's rounds are sent to where it stands)
					if (Copy.Spd <= 0.0) continue;
					Copy.Spd = std::min(Copy.Spd + 5.0 * FrameDt / Steps, 75.0);
					if (Copy.Spd > 55.0) Copy.Gam = std::min(Copy.Gam + 0.1 * FrameDt / Steps, 0.16);
					Copy.P += Ahead(Copy.Hdg, Copy.Gam) * (Copy.Spd * FrameDt / Steps);
					Copy.P.Z = std::max(Copy.P.Z, GroundAt(Copy.P.X, Copy.P.Y));
					continue;
				}
				Fly(Copy, Copy.Flown, FrameDt / Steps, false);
				// (a dive bomber on its brakes, as PilotDive holds it)
				if (Copy.Doing == Role::Dive && Copy.Step == Phase::Plunge) Copy.Spd = std::min(Copy.Spd, 115.0);
			}
			Note((K + 1) * FrameDt);
		}
		return Co;
	}

	Vec Show::OwnOn(const Course& Co, double T, const Vec& World) const
	{
		const int Last = static_cast<int>(Co.At.size()) - 1;
		if (Last < 1 || T <= 0.0 || Co.Step <= 0.0)
		{
			const Pose& A = Co.At.front();
			const Vec D = World - A.P;
			return Vec(D.Dot(A.F), D.Dot(A.R), D.Dot(A.U));
		}
		// (a round's place at a time between two frames is drawn against the later frame's aircraft, come there from
		// the earlier one's place: so it is reckoned here)
		const int K = std::min(Last, std::max(1, static_cast<int>(std::ceil(T / Co.Step - 1e-9))));
		const Pose& A = Co.At[static_cast<size_t>(K)];
		const Vec Mid = A.P - (A.P - Co.At[static_cast<size_t>(K - 1)].P) * ((K * Co.Step - T) / Co.Step);
		const Vec D = World - Mid;
		return Vec(D.Dot(A.F), D.Dot(A.R), D.Dot(A.U));
	}

	bool Show::SolveOn(const Barrel& B, const Course& Co, const Vec& Spot, Vec& Way, double& Tof) const
	{
		// As Solve, the aircraft's place and the way it lies taken from its course. Between two frames the spot is
		// where the picture will have it (OwnOn): the aircraft's middle eased from the one frame's place to the next,
		// lying as it lies in the later one. A round that ends between two frames is drawn against the later frame's
		// aircraft, and on one rolling hard a wing tip eased between the two lies half a metre from where that has it.
		const int Last = static_cast<int>(Co.At.size()) - 1;
		if (Last < 1 || Co.Step <= 0.0) return false;
		auto SpotAt = [&Co, &Spot, Last](double At, Vec& Vel)
		{
			const double X = Clamp(At / Co.Step, 0.0, static_cast<double>(Last));
			const int K = std::min(Last, std::max(1, static_cast<int>(std::ceil(X - 1e-9))));
			const Pose& A = Co.At[static_cast<size_t>(K - 1)];
			const Pose& N = Co.At[static_cast<size_t>(K)];
			Vel = (N.P - A.P) * (1.0 / Co.Step);
			return N.P - (N.P - A.P) * (K - X) + N.F * Spot.X + N.R * Spot.Y + N.U * Spot.Z;
		};
		const double D = std::max(1e-6, B.Fl.Drag);
		Vec Vel;
		double T = Len(SpotAt(0.0, Vel) - B.At) / std::max(200.0, B.Mv);
		for (int K = 0; K < 12; ++K)
		{
			// (no farther on than its course was told)
			if (!(T > 1e-4) || T > Last * Co.Step || T > B.Fl.Life + 0.5) return false;
			const double E = std::exp(-D * T), Kt = (1.0 - E) / D;
			const Vec R = SpotAt(T, Vel) - B.At - B.Carry * Kt + Vec(0.0, 0.0, 0.5 * B.Fl.Drop * T * T);
			const double L = Len(R);
			if (L < 1e-6) return false;
			if (std::fabs(L - B.Mv * Kt) < 0.02)
			{
				// (within its tracer's life, with a little to spare)
				if (T > B.Fl.Life * 0.92) return false;
				Way = R * (1.0 / L);
				Tof = T;
				return true;
			}
			const Vec Rd = Vel - B.Carry * E + Vec(0.0, 0.0, B.Fl.Drop * T);
			const double Slope = R.Dot(Rd) / L - B.Mv * E;
			if (Slope > -1.0) return false; // (it is not being caught)
			T -= (L - B.Mv * Kt) / Slope;
		}
		return false;
	}

	double Show::EntryOf(const Barrel& B, const Vec& Way, const Course& Co, const Shape& S, double About) const
	{
		// Along the round's path past the aircraft, as the frames to come will have the two of them, under half a
		// metre at a time: from where it could first touch the airframe to a little past the place it was sent to.
		const int Last = static_cast<int>(Co.At.size()) - 1;
		if (Last < 1 || Co.Step <= 0.0) return -1.0;
		const Vec V0 = Way * B.Mv + B.Carry;
		const int K = std::min(Last - 1, std::max(0, static_cast<int>(About / Co.Step)));
		const Vec Its = (Co.At[static_cast<size_t>(K + 1)].P - Co.At[static_cast<size_t>(K)].P) * (1.0 / Co.Step);
		const double Speed = std::max(50.0, Len(V0 * std::exp(-B.Fl.Drag * About) - Vec(0.0, 0.0, B.Fl.Drop * About) - Its));
		const double Bound = 0.5 * std::hypot(S.Long, S.Span) + 1.5;
		const double Step = 0.4 / Speed, To = std::min(B.Fl.Life, About + 3.0 / Speed);
		for (double T = std::max(0.0, About - (Bound + 3.0) / Speed); T <= To; T += Step)
		{
			if (InAirframe(S, OwnOn(Co, T, RoundAt(B.At, V0, T, B.Fl.Drag, B.Fl.Drop)), 0.0)) return T;
		}
		return -1.0;
	}

	bool Show::Solve(const Barrel& B, const Craft& Q, const Vec& Spot, Vec& Way, double& Tof) const
	{
		// The round leaves at its gun's speed along the way sought, carried by its own aircraft, slows and falls; the
		// aircraft it is for flies on as it is flying now. Sought: the time T at which the two can be in one place.
		// The round's own share of the way there, R(T), must be as long as the gun's speed takes it in T.
		const Vec At0 = Q.P + FromOwn(Q, Spot) - B.At;
		const double D = std::max(1e-6, B.Fl.Drag);
		double T = Len(At0) / std::max(200.0, B.Mv);
		auto Share = [&](double At, double& E, double& Kt)
		{
			E = std::exp(-D * At);
			Kt = (1.0 - E) / D;
			return At0 + Q.GunV * At + Q.GunA * (0.5 * At * At) + Q.GunJ * (At * At * At / 6.0) - B.Carry * Kt + Vec(0.0, 0.0, 0.5 * B.Fl.Drop * At * At);
		};
		double E = 1.0, Kt = 0.0;
		for (int K = 0; K < 8; ++K)
		{
			const Vec R = Share(T, E, Kt);
			const double L = Len(R);
			if (L < 1e-6) return false;
			const Vec Rd = Q.GunV + Q.GunA * T + Q.GunJ * (0.5 * T * T) - B.Carry * E + Vec(0.0, 0.0, B.Fl.Drop * T);
			const double Slope = R.Dot(Rd) / L - B.Mv * E;
			if (Slope > -1.0) return false; // (it is not being caught)
			const double Step = (L - B.Mv * Kt) / Slope;
			T -= Step;
			if (!(T > 1e-4) || T > B.Fl.Life + 0.5) return false;
			if (std::fabs(Step) < 2e-5) break;
		}
		// (within its tracer's life, with a little to spare)
		if (T > B.Fl.Life * 0.92) return false;
		const Vec R = Share(T, E, Kt);
		if (std::fabs(Len(R) - B.Mv * Kt) > 0.25) return false;
		Way = Norm(R);
		Tof = T;
		return true;
	}

	Show::Nearest Show::PassOf(const Barrel& B, const Vec& Way, const Craft& Q) const
	{
		return PassBy(B, Way, Q.P, Q.GunV, Q.GunA, Q.GunJ);
	}

	Show::Nearest Show::PassBy(const Barrel& B, const Vec& Way, const Vec& At, const Vec& Vel, const Vec& Acc, const Vec& Jerk) const
	{
		const Vec V0 = Way * B.Mv + B.Carry;
		const Vec R0 = B.At - At;
		const double D = std::max(1e-6, B.Fl.Drag);
		const Vec Vr = V0 - Vel;
		double T = Clamp(-R0.Dot(Vr) / std::max(1.0, Vr.Dot(Vr)), 0.0, B.Fl.Life);
		Vec R, Rd;
		double E = 1.0, Kt = 0.0;
		for (int K = 0; K < 3; ++K)
		{
			E = std::exp(-D * T);
			Kt = (1.0 - E) / D;
			R = R0 + V0 * Kt - Vec(0.0, 0.0, 0.5 * B.Fl.Drop * T * T) - Vel * T - Acc * (0.5 * T * T) - Jerk * (T * T * T / 6.0);
			Rd = V0 * E - Vec(0.0, 0.0, B.Fl.Drop * T) - Vel - Acc * T - Jerk * (0.5 * T * T);
			if (K == 2) break;
			T = Clamp(T - R.Dot(Rd) / std::max(1.0, Rd.Dot(Rd)), 0.0, B.Fl.Life);
		}
		Nearest P;
		P.T = T;
		P.Off = R;
		P.Speed = Len(Rd);
		P.Dir = Norm(Rd, Way);
		P.Acc = V0 * (-D * E) - Vec(0.0, 0.0, B.Fl.Drop) - Acc - Jerk * T;
		P.Flown = Len(V0) * Kt;
		return P;
	}

	double Show::DriftOf(const Craft& Q, double T) const
	{
		// (as the aircraft of a play were found to stray from where their flight told, a time of flight on: a bomber,
		// and a fighter out of the fight, all but one time in a thousand inside this; a fighter in a fight all but a
		// few in a thousand, his guns not to be laid much wider of him)
		return (!Q.bFighter ? 2.0 : Q.Doing == Role::Fight ? 24.0 : 8.0) * T * T;
	}

	double Show::RollOf(const Craft& Q, double T) const
	{
		// (a fighter in a fight at all the roll it has, one out of it at half. A bomber at a little: changing places
		// in her box she rolls faster, but room for that was tried and kept no more rounds out of her wings.)
		const double Rate = !Q.bFighter ? std::min(Q.RollMax, 0.15) : Q.Doing == Role::Fight ? Q.RollMax : 0.5 * Q.RollMax;
		return std::sin(std::min(0.5 * Pi, Rate * T));
	}

	void Show::FallOf(const Craft& Q, Vec& At, Vec& Vel, Vec& Acc, double& Room) const
	{
		if (Q.Doing == Role::Fall && Q.Falling == FallHow::Spin)
		{
			// (the helix PilotFall winds it down)
			At = Q.Axis;
			Vel = Q.FallV;
			Acc = Vec();
			Room = 2.0 + (Q.Look < 2 ? 6.0 + Q.Form.Span * 0.4 : 14.0 + Q.Form.Span * 0.8);
			return;
		}
		At = Q.P;
		Vel = Q.GunV;
		Acc = Q.GunA;
		Room = 2.0;
	}

	Show::Met Show::Meets(const Barrel& B, const Vec& Way, double Margin, double Spread, int But, double Unsure) const
	{
		Met First;
		const Vec V0 = Way * B.Mv + B.Carry;
		const double Speed0 = std::max(1.0, Len(V0));
		const Vec Along = V0 * (1.0 / Speed0);
		const double Far = Speed0 * B.Fl.Life;
		for (const auto& Pair : Crafts)
		{
			const Craft& Q = Pair.second;
			if (Q.Key == B.Own || Q.Key == But || Q.Doing == Role::Gone || !Q.bShown) continue;
			// (nowhere near its line: behind the gun, out of its reach, or off to a side of it)
			const double Bound = 0.5 * std::hypot(Q.Form.Long, Q.Form.Span) + 1.5;
			const double Drift = (Len(Q.GunV) + 45.0) * B.Fl.Life;
			const double Wide = Bound + Drift + Margin + Spread * Far + 30.0;
			const Vec Rel = Q.P - B.At;
			const double On = Rel.Dot(Along);
			if (On < -Wide || On > Far + Wide || Rel.Dot(Rel) - On * On > Wide * Wide) continue;
			const bool bDown = Q.Doing == Role::Fall;
			Nearest P;
			double Room = 0.0;
			if (bDown)
			{
				Vec At, Vel, Acc;
				FallOf(Q, At, Vel, Acc, Room);
				P = PassBy(B, Way, At, Vel, Acc, Vec());
			}
			else P = PassOf(B, Way, Q);
			const double Speed = std::max(50.0, P.Speed);
			// One going down, or due to before the rounds are by (the burst that brings it down on its way): there is
			// no telling how it will lie by then, nor quite where. A ball about it is kept clear of.
			const double Gone = bDown ? P.T : AirT + P.T + GunDt - Q.FallDue;
			if (bDown || Gone >= 0.0)
			{
				const double Ball = Bound + Margin + Spread * P.Flown + Room + (bDown ? 16.0 * P.T * P.T : 3.0 + std::min(60.0, 80.0 * Gone));
				const double Off2 = P.Off.Dot(P.Off);
				if (Off2 > Ball * Ball) continue;
				const double Near = std::max(0.0, P.T - std::sqrt(Ball * Ball - Off2) / Speed);
				if (First.Key && Near >= First.Tof) continue;
				First.Key = Q.Key;
				First.Tof = Near;
				First.Short = std::max(0.0, Near - 4.0 / Speed);
				First.bThrough = false;
				First.Strike = 0.0;
				continue;
			}
			// (wider of one throwing itself about: it may be some way from where its flight tells by then)
			const double Keep = Margin + Spread * P.Flown + std::min(Q.GunErr, 1.5) * (P.T * P.T / 0.09) + 0.6 * Unsure * DriftOf(Q, P.T);
			// (and one rolling now is taken to roll on as it is until the rounds are by: a bomber hit and falling out of
			// her box banks away at all the roll she has, and her wing comes up into a burst laid true on the next one)
			const double Rolling = std::sin(std::min(0.5 * Pi, std::fabs(Q.RollNow) * P.T));
			const double Sway = std::max(Unsure > 0.0 ? Unsure * RollOf(Q, P.T) : 0.0, Rolling);
			if (Len(P.Off) > Bound + Keep) continue;
			const double Half = (Bound + Keep + 1.0) / Speed;
			if (First.Key && P.T - Half > First.Tof) continue; // (it comes to an earlier one first)
			// Along its path past the aircraft, in the aircraft's own frame, under half a metre at a time.
			const Vec F = Ahead(Q.Hdg, Q.Gam);
			const Vec R0 = RightOf(Q.Hdg);
			const Vec U0 = R0.Cross(F);
			const double Cb = std::cos(Q.Bank), Sb = std::sin(Q.Bank);
			const Vec R = R0 * Cb - U0 * Sb;
			const Vec U = U0 * Cb + R0 * Sb;
			const Vec Vel = P.Dir * P.Speed;
			const double Step = 0.45 / Speed;
			const double From = std::max(0.0, P.T - Half), To = std::min(B.Fl.Life, P.T + Half);
			Slab Swept[10], Bare[10];
			const int Boxes = SlabsOf(Q.Form, Swept, Sway);
			if (Sway > 0.0) SlabsOf(Q.Form, Bare);
			const Slab* Body = Sway > 0.0 ? Bare : Swept;
			bool bNear = false, bThrough = false;
			double NearT = 0.0, ThroughT = 0.0;
			for (double T = From; T <= To; T += Step)
			{
				const double S = T - P.T;
				const Vec W = P.Off + Vel * S + P.Acc * (0.5 * S * S);
				const Vec L(W.Dot(F), W.Dot(R), W.Dot(U));
				if (!bNear)
				{
					if (!InSlabs(Swept, Boxes, L, Keep)) continue;
					// (later than one already found: that one is the first it comes to)
					if (First.Key && T >= First.Tof) break;
					bNear = true;
					NearT = T;
				}
				if (Keep <= 0.0 || InSlabs(Body, Boxes, L, 0.0))
				{
					bThrough = true;
					ThroughT = T;
					break;
				}
			}
			if (!bNear) continue;
			First.Key = Q.Key;
			First.Tof = NearT;
			First.Short = std::max(0.0, NearT - 4.0 / Speed);
			First.bThrough = bThrough;
			First.Strike = ThroughT;
		}
		return First;
	}

	bool Show::InStream(const Vec& Place, const Vec& Vel, double Room) const
	{
		// (each frame's rounds of a gun on their straight way from its muzzle, the aircraft flying on at Vel from Place:
		// how near they come, from now to the end of their life. Drag only slows them along that way.)
		for (const RoundOut& R : RoundsOut)
		{
			const double Age = AirT - R.T;
			if (Age < 0.0 || Age > R.Life) continue;
			const Vec R0 = R.From - Place + Vel * Age, Dv = R.Vel - Vel;
			const double S = Clamp(-R0.Dot(Dv) / std::max(1.0, Dv.Dot(Dv)), Age, R.Life);
			if (Len(R0 + Dv * S) < Room) return true;
		}
		return false;
	}

	bool Show::Shadowed(const Vec& Place, double T, int But) const
	{
		for (const auto& Pair : Crafts)
		{
			const Craft& Q = Pair.second;
			if (Q.Key == But || Q.Doing == Role::Gone) continue;
			const bool bDown = Q.Doing == Role::Fall;
			const double Gone = bDown ? T : AirT + T + GunDt - Q.FallDue;
			if (Gone < 0.0) continue;
			Vec At = Q.P, Vel, Acc;
			double Room = 0.0;
			// (one blown apart stands where it burst)
			if (!bDown || Q.bShown) FallOf(Q, At, Vel, Acc, Room);
			Room += 0.6 * Q.Form.Span + 2.0 + (bDown ? 16.0 * T * T : 3.0 + std::min(60.0, 80.0 * Gone));
			if (Len(At + Vel * T + Acc * (0.5 * T * T) - Place) < Room) return true;
		}
		return false;
	}

	double Show::LiftAt(const Craft& C, double At) const
	{
		// (her load gone, she lifts a little, over about a second, and is trimmed down again over the next six)
		if (C.StickBegan < 0.0 || StickLift <= 0.0) return 0.0;
		const double Since = At - C.StickBegan;
		const auto Ease = [](double X) { X = std::max(0.0, std::min(1.0, X)); return X * X * (3.0 - 2.0 * X); };
		return StickLift * (Since < 1.2 ? Ease(Since / 1.2) : 1.0 - Ease((Since - 1.2) / 6.0));
	}

	bool Show::WideOf(const Barrel& B, const Vec& Way, const Craft& Q, double Keep, double Rolled, Vec& Spot) const
	{
		const Nearest P = PassOf(B, Way, Q);
		// Across its path: how far out from the aircraft's middle the round passes, and on which side.
		Vec Out_ = P.Off - P.Dir * P.Off.Dot(P.Dir);
		const double Miss = Len(Out_);
		if (Miss < 0.3)
		{
			// (dead on: over it; or by its side, coming straight down on it)
			const Vec Top = FromOwn(Q, Vec(0.0, 0.0, 1.0));
			Out_ = Top - P.Dir * Top.Dot(P.Dir);
			if (Len(Out_) < 0.2)
			{
				const Vec Side = FromOwn(Q, Vec(0.0, 1.0, 0.0));
				Out_ = Side - P.Dir * Side.Dot(P.Dir);
			}
		}
		const Vec Ul = ToOwn(Q, Norm(Out_, Up));
		// (past where its wings may have swept by then, rolling)
		const double Edge = ReachOf(Q.Form, Ul, Rolled * RollOf(Q, P.T)) + Keep + std::min(Q.GunErr, 1.5) * (P.T * P.T / 0.09);
		if (Miss >= Edge) return false;
		Spot = Ul * (Edge + 0.4);
		return true;
	}

	double Show::ToGround(const Barrel& B, const Vec& Way) const
	{
		const Vec V0 = Way * B.Mv + B.Carry;
		const double Life = B.Fl.Life;
		// (too high for any round of its life to come down to it)
		const double Sink = -std::min(0.0, V0.Z) * Life + 0.5 * B.Fl.Drop * Life * Life;
		if (B.At.Z - GroundAt(B.At.X, B.At.Y) > Sink + 300.0) return 0.0;
		double Last = 0.0;
		for (int K = 1; K <= 12; ++K)
		{
			const double T = Life * K / 12.0;
			const Vec P = RoundAt(B.At, V0, T, B.Fl.Drag, B.Fl.Drop);
			if (P.Z <= GroundAt(P.X, P.Y))
			{
				double Lo = Last, Hi = T;
				for (int J = 0; J < 8; ++J)
				{
					const double Mid = 0.5 * (Lo + Hi);
					const Vec M = RoundAt(B.At, V0, Mid, B.Fl.Drag, B.Fl.Drop);
					(M.Z <= GroundAt(M.X, M.Y) ? Hi : Lo) = Mid;
				}
				return std::max(Hi, 0.02);
			}
			Last = T;
		}
		return 0.0;
	}

	int Show::BoxOf(const Event& E) const
	{
		if (E.GunBox >= 0) return E.GunBox;
		auto By = Now.find(E.By);
		return By != Now.end() && !By->second.bFighter ? E.By : -1;
	}

	bool Show::ForGuns(const Event& E) const
	{
		if (E.What != "air:lost" && E.What != "air:hit" && E.What != "air:straggle") return false;
		if (E.bByFlak || !Now.count(E.Unit)) return false;
		// (one struck by another's fall, or run into, fell to no gun)
		if (E.What == "air:lost" && (E.How == "struck" || E.How == "collide")) return false;
		const int Box = BoxOf(E);
		if (Box >= 0) return Now.count(Box) > 0 && E.What != "air:straggle";
		auto By = Now.find(E.By);
		return By != Now.end() && By->second.bFighter;
	}

	void Show::SpotOf(const Craft& V, const Event& E, Vec& Spot, Vec& From, std::string& Part) const
	{
		// Where the battle has it hit, so the strikes are seen where the damage then shows: the engine that smokes or
		// burns, the wing that comes off, the tail shot away, the cockpit of a pilot killed. The run of strikes walks
		// to it from a little way off, as a burst walked on to its mark.
		const Shape& S = V.Form;
		const double L = S.Long, W = S.Span;
		const int N = static_cast<int>(S.Engines.size());
		const double Side = E.Hand < 0 ? -1.0 : E.Hand > 0 ? 1.0 : E.Roll < 0 ? -1.0 : E.Roll > 0 ? 1.0 : V.Hand < 0.0 ? -1.0 : 1.0;
		auto Engine = [&](int Index) // (from 0)
		{
			Spot = N > 0 ? S.Engines[static_cast<size_t>(std::max(0, std::min(N - 1, Index)))] : Vec(L * 0.4, 0.0, 0.0);
			// (along the wing to a nacelle; up the nose to a single engine)
			From = N > 1 ? Spot + Vec(-L * 0.06, (Spot.Y < 0.0 ? -1.0 : 1.0) * W * 0.07, 0.0) : Vec(L * 0.05, 0.0, 0.1);
			Part = V.bLiquid ? "coolant" : "oil";
		};
		auto Wing = [&](double Out_)
		{
			Spot = Vec(L * 0.05, Out_, 0.0);
			From = Vec(L * 0.05, Clamp(Out_ * 1.6, -W * 0.45, W * 0.45), 0.0);
			Part = "fuel";
		};
		auto Tail = [&]()
		{
			Spot = Vec(-L * 0.42, 0.0, std::min(0.5, S.Body * 0.4));
			From = Vec(-L * 0.2, 0.0, 0.1);
			Part = "skin";
		};
		auto Cockpit = [&]()
		{
			auto It = S.Stations.find("cockpit");
			Spot = It != S.Stations.end() ? It->second : Vec(0.0, 0.0, 0.5);
			From = Spot - Vec(L * 0.2, 0.0, 0.3);
			Part = "skin";
		};
		auto Middle = [&]()
		{
			Spot = Vec(L * 0.05, 0.0, 0.0);
			From = Vec(-L * 0.25, 0.0, 0.1);
			Part = "fuel";
		};
		if (E.What == "air:straggle")
		{
			Engine((E.Eng.empty() ? 1 : E.Eng.front()) - 1);
		}
		else if (E.What == "air:hit")
		{
			if (E.Mark == "smoke" || E.Mark == "fireout")
			{
				// (the engine HitSurvived then lights)
				Engine(N > 1 && E.EngineAt >= 0.0 ? static_cast<int>(std::lround(std::min(1.0, E.EngineAt) * (N - 1))) : 0);
			}
			else
			{
				// Holes, or panels torn off: out on the wing of the side the battle names, or aft along the fuselage.
				const int Where = std::abs(E.Slot * 7 + static_cast<int>(E.T * 10.0)) % 3;
				if (Where == 0) Wing(Side * W * 0.18);
				else if (Where == 1) Wing(Side * W * 0.3);
				else Tail();
				Part = "skin";
			}
		}
		else
		{
			const std::string How = E.How.empty() ? (E.Kind == "explode" || E.Kind == "spin" ? E.Kind : std::string("burn")) : E.How;
			if (How == "wing")
			{
				// (the wing StartFall then tears away, at its break)
				const double Turn = E.Roll < 0 ? -1.0 : E.Roll > 0 ? 1.0 : (E.Hand < 0 ? -1.0 : E.Hand > 0 ? 1.0 : V.Hand < 0.0 ? -1.0 : 1.0);
				Wing(Turn * W * (E.Look == 1 ? 0.2 : E.Look == 3 ? 0.35 : 0.28) * 0.55);
			}
			else if (How == "tail") Tail();
			else if (How == "pilot") Cockpit();
			else if (How == "explode" || How == "breakup") Middle();
			else if (How == "flamer")
			{
				Spot = Vec(L * 0.05, Side * W * 0.07, 0.0);
				From = Vec(L * 0.05, Side * W * 0.2, 0.0);
				Part = "fuel";
			}
			else if (E.Part == "cockpit") Cockpit();
			else if (E.Part == "tail") Tail();
			else if (E.Part == "wing") Wing(Side * W * 0.2);
			else if (E.Part == "fuselage") Middle();
			// (the engine StartFall then burns: engine k of N is floor(engineAt N) + 1)
			else Engine(E.EngineAt >= 0.0 ? static_cast<int>(std::floor(E.EngineAt * N)) : 0);
		}
		// (both ends of the run on the airframe)
		for (Vec* P : { &Spot, &From })
		{
			if (InAirframe(S, *P, 0.0)) continue;
			P->Z = Clamp(P->Z, -S.Body * 0.4, S.Body * 0.4);
			P->X = Clamp(P->X, -L * 0.48, L * 0.48);
			if (!InAirframe(S, *P, 0.0)) P->Y = 0.0;
		}
	}

	int Show::Wanted(int By, int Box, const Craft& V) const
	{
		int Best = -1, BestRank = 3;
		for (size_t K = 0; K < Later.size(); ++K)
		{
			const Held& H = Later[K];
			if (!H.bForShot || H.E.Unit != V.Group) continue;
			const Event& E = H.E;
			const int Its = BoxOf(E);
			if (Box >= 0 ? Its != Box : (Its >= 0 || E.By != By)) continue;
			if (E.What == "air:lost")
			{
				// (a straggler's own loss on that straggler; any other on one still in its group)
				if (E.Strag >= 0 ? V.StragId != E.Strag : V.StragId >= 0) continue;
			}
			else if (E.What == "air:straggle")
			{
				if (V.bFighter || V.StragId >= 0 || !(V.Doing == Role::Slot || V.Doing == Role::Dive || V.Doing == Role::Fight)) continue;
			}
			else
			{
				// (a hit survived: on one in the air, and one that lights a trail on one not trailing already)
				if (V.Doing == Role::Ground) continue;
				if ((E.Mark == "smoke" || E.Mark == "fireout") && V.TrailKey) continue;
			}
			// (an aircraft lost before one shot out of its box, that before a hit survived; of a kind, the longest held)
			const int Rank = E.What == "air:lost" ? 0 : E.What == "air:straggle" ? 1 : 2;
			if (Best < 0 || Rank < BestRank || (Rank == BestRank && H.At < Later[static_cast<size_t>(Best)].At))
			{
				Best = static_cast<int>(K);
				BestRank = Rank;
			}
		}
		return Best;
	}

	int Show::HitFirst(int By, const Craft& V, int At) const
	{
		// A fighter's guns, a loss wanted on the one in his sights, and a hit survived waiting beside it: the hit goes
		// first, while the loss can wait the second or two the next burst takes. He is seen hit, and flies on smoking;
		// the burst after it brings him down. The loss first, and the one in his sights is gone with the hit still to
		// show and no gun near to show it.
		if (At < 0 || V.Doing == Role::Ground) return At;
		const Held& Loss = Later[static_cast<size_t>(At)];
		if (Loss.E.What != "air:lost" || Loss.At - AirT < 3.0) return At;
		// (one these guns have struck a moment ago has had his hit: the loss now)
		if (V.StruckBy == By && AirT - V.StruckAt < 4.0) return At;
		int Hit = -1;
		for (size_t K = 0; K < Later.size(); ++K)
		{
			const Held& H = Later[K];
			if (!H.bForShot || H.E.Unit != V.Group || H.E.By != By || BoxOf(H.E) >= 0) continue;
			if (H.E.What == "air:lost" || H.E.What == "air:straggle") continue;
			if ((H.E.Mark == "smoke" || H.E.Mark == "fireout") && V.TrailKey) continue;
			if (Hit < 0 || H.At < Later[static_cast<size_t>(Hit)].At) Hit = static_cast<int>(K);
		}
		return Hit >= 0 ? Hit : At;
	}

	int Show::Book(int LaterAt, Craft& Shooter, Craft& Victim, int Mount, const std::string& Station, const Vec& StationAt)
	{
		const Held H = Later[static_cast<size_t>(LaterAt)];
		Later.erase(Later.begin() + LaterAt);
		Shot S;
		S.Id = NextShot++;
		S.E = H.E;
		S.HeldUntil = H.At;
		S.HeldSince = H.Since;
		S.bMore = H.bMore;
		S.Shooter = Shooter.Key;
		S.Mount = Mount;
		S.By = Shooter.Group;
		S.Victim = Victim.Key;
		const bool bLost = S.E.What == "air:lost";
		// (a fall from an engine the battle did not name: named now, one of the inner pair as StartFall has it, so the
		// strikes find the engine that then burns)
		const double N = static_cast<double>(Victim.Form.Engines.size());
		if (bLost && S.E.EngineAt < 0.0 && N > 1.0) S.E.EngineAt = ((Shots.U() < 0.5 ? 1.0 : N - 2.0) + 0.5) / N;
		SpotOf(Victim, S.E, S.Spot, S.From, S.Part);
		S.Booked = AirT;
		S.OnAt = AirT;
		Victim.ShotOn = S.Id;
		Moment M;
		M.Kind = Mount >= 0 ? (bLost ? "gunner-kill" : "gunner-hit") : bLost ? "kill" : S.E.What == "air:straggle" ? "straggle" : "hit";
		M.Shooter = Shooter.Key;
		M.Gun = Mount;
		M.Station = Station;
		M.StationAt = StationAt;
		M.Seat = Mount >= 0 ? S.E.GunSeat : -1;
		M.Victim = Victim.Key;
		M.OpenAt = AirT;
		M.StrikeAt = AirT + Len(Victim.P - Shooter.P) / 700.0;
		M.Id = S.Id;
		Seen.push_back(M);
		Booked.push_back(S);
		return S.Id;
	}

	Show::Shot* Show::ShotOf(int Id)
	{
		for (Shot& S : Booked) if (S.Id == Id) return &S;
		return nullptr;
	}

	Show::Shot* Show::Carrying(int Shooter, int Mount)
	{
		for (Shot& S : Booked) if (!S.bDone && S.Shooter == Shooter && S.Mount == Mount) return &S;
		return nullptr;
	}

	Show::Moment* Show::MomentOf(int Id)
	{
		for (Moment& M : Seen) if (M.Id == Id) return &M;
		return nullptr;
	}

	Vec Show::WalkOf(const Shot& S) const
	{
		const double K = S.OnSince < 0.0 ? 0.0 : Clamp((AirT - S.OnSince) / 0.3, 0.0, 1.0);
		return S.From + (S.Spot - S.From) * K;
	}

	bool Show::NoseGun(const Craft& C, Barrel& B) const
	{
		// A fighter's fixed guns as one gun on his nose line: his first row's rounds.
		auto L = Loadouts.find(C.Nation + ":ftr");
		if (L == Loadouts.end()) return false;
		for (const Gun& Row : L->second.Guns)
		{
			if (Row.Mount != "fixed") continue;
			B.At = Local(C, Vec(C.Form.Long * 0.1, 0.0, 0.0));
			B.Carry = Velocity(C);
			B.Mv = Row.Mv;
			B.Fl = FlightOf(Row.Kind);
			B.Own = C.Key;
			return true;
		}
		return false;
	}

	Show::Craft* Show::Mark(const Craft& C, const Barrel& B, int Unit, double Range, double Within, bool bAny, int& LaterAt, Vec& Way, double& Tof)
	{
		// Of the aircraft of a unit the battle has counted hit by his group, and the scene has yet to show hit: the one
		// nearest his nose line that his rounds can be laid on, inside his range. (A pilot fires on what comes into his
		// sight: the bomber he set out for, or the one beside it; with bAny, an aircraft of any unit his group's guns
		// have a hit to show on, as it crosses before him.)
		const Vec Nose = Ahead(C.Hdg, C.Gam);
		Craft* Best = nullptr;
		double Least = Within;
		for (auto& Pair : Crafts)
		{
			Craft& Q = Pair.second;
			// (one on the ground only on a strafing run)
			if ((Q.Group != Unit && (!bAny || Q.Side == C.Side)) || Q.ShotOn != 0 || !Q.bShown || !Live(Q) || (Q.Doing == Role::Ground && !C.bStrafing)) continue;
			const Vec Rel = Q.P - B.At;
			const double Dist = Len(Rel);
			// (one of another unit: as near as he would open on a fighter in a chase, or on a bomber from behind)
			const double Reach = Q.Group == Unit ? Range : Q.bFighter ? 500.0 - 110.0 * C.Skill : 680.0 - 150.0 * C.Skill;
			if (Dist >= Reach || Dist < 60.0) continue;
			// (nowhere near his nose: the lead on a crossing aircraft is 10 degrees or so at most)
			if (Rel.Dot(Nose) < Dist * std::cos(std::min(1.5, Within + 14.0 * Deg))) continue;
			const int At = HitFirst(C.Group, Q, Wanted(C.Group, -1, Q));
			if (At < 0) continue;
			Vec Spot, From, Its;
			std::string Part;
			double Flight0 = 0.0;
			SpotOf(Q, Later[static_cast<size_t>(At)].E, Spot, From, Part);
			if (!Solve(B, Q, From, Its, Flight0)) continue;
			const double Off = Between(Its, Nose);
			if (Off > Least) continue;
			Least = Off;
			Best = &Q;
			LaterAt = At;
			Way = Its;
			Tof = Flight0;
		}
		return Best;
	}

	void Show::FineAim(Craft& C, const Vec& Way, double Dt)
	{
		// The last few degrees on to a firing solution: his nose is brought to it a little faster than his turn alone
		// brings it, never past what his aircraft may pull, its tightest turn, or a climb it could not hold.
		const Vec Nose = Ahead(C.Hdg, C.Gam);
		if (Between(Way, Nose) > 12.0 * Deg || Dt <= 0.0) return;
		// (flying on as he was for rounds on their way to him: his nose stays where that puts it)
		if (AirT < C.HeldTo) return;
		const double S = std::max(C.Spd, 30.0);
		const double Most = 14.0 * Deg * Dt, Share = std::min(1.0, 5.0 * Dt);
		const double StepH = Clamp(Wrap(std::atan2(Way.X, Way.Y) - C.Hdg) * Share, -Most, Most);
		double StepG = Clamp((std::asin(Clamp(Way.Z, -1.0, 1.0)) - C.Gam) * Share, -Most, Most);
		// (never steeper down near the ground, nor up past the climb he will hold at his speed)
		if (StepG < 0.0 && C.P.Z - GroundAt(C.P.X, C.P.Y) < 260.0) StepG = 0.0;
		if (StepG > 0.0) StepG = std::max(0.0, std::min(StepG, ClimbMost(C) - C.Gam));
		const double GNow = std::min(C.GMax, 2.0 + (C.GMax - 2.0) * Clamp((S - C.VMin) / 45.0, 0.0, 1.0));
		const double Cg = std::cos(C.Gam);
		auto LoadOf = [&](double F, double& Turn)
		{
			Turn = C.TurnNow + F * StepH / Dt;
			const double Pull = C.PullNow + F * StepG / Dt;
			return std::hypot(S * std::fabs(Turn) / Gee, S * Pull / Gee + Cg);
		};
		// (as much of the step as keeps him inside them: all of it, or the largest share found in a few halvings)
		double F = 1.0, Turn = 0.0;
		for (int K = 0; K < 6; ++K)
		{
			const double Load = LoadOf(F, Turn);
			if (Load <= GNow && std::fabs(Turn) <= S / C.RMin) break;
			F = K == 5 ? 0.0 : F * 0.5;
		}
		if (F <= 0.0) return;
		const double Load = LoadOf(F, Turn);
		C.Hdg = Wrap(C.Hdg + F * StepH);
		C.Gam = Clamp(C.Gam + F * StepG, -88.0 * Deg, 80.0 * Deg);
		C.TurnNow = Turn;
		C.PullNow += F * StepG / Dt;
		Check.MostG = std::max(Check.MostG, Load);
		if (std::fabs(Turn) > 1e-4) Check.LeastRadius = std::min(Check.LeastRadius, C.Spd / std::fabs(Turn));
		Check.AimSteps++;
	}

	bool Show::Sure(const Craft& Q, double Tof) const
	{
		// Where an aircraft will be when the rounds reach it is sure only as far as it flies on as it is flying. A
		// stream is laid true (and its hit shown) only on one whose flight has lately told its place well enough for
		// the rounds' time on the way: within the skin's thickness of where they are sent (TrackAll).
		return (Q.GunErr + 0.05) * (Tof * Tof / 0.09) <= 0.8;
	}

	void Show::SendStrike(Shot& S, double Tof, const std::string& Kind, const Vec& In, const Vec& World)
	{
		if (S.OnSince < 0.0)
		{
			S.OnSince = AirT;
			if (Moment* M = MomentOf(S.Id)) M->StrikeAt = AirT + Tof;
			// The damage is timed now, from when these first rounds will land: at once for one that blows apart, a
			// heavy bomber soaking up most of a second of it. The burst is cut to it (no round is fired that would land
			// later), so none is still in the air, to end on nothing, when the aircraft goes.
			Craft* V = Find(S.Victim);
			const bool bLost = S.E.What == "air:lost";
			const bool bApart = bLost && (S.E.How == "explode" || (S.E.How.empty() && S.E.Kind == "explode"));
			S.Plan = AirT + Tof + (bApart ? Shots.U(0.1, 0.2) : !bLost ? Shots.U(0.2, 0.35) : V && V->bFighter ? Shots.U(0.25, 0.45) : Shots.U(0.5, 0.9));
			if (V && bLost) V->FallDue = S.Plan;
		}
		if (AirT < S.NextStrike) return;
		// (none that would land after the damage has begun)
		if (S.ReleaseAt >= 0.0 && AirT + Tof >= S.ReleaseAt) return;
		Landing K;
		K.At = AirT + Tof;
		K.Shot = S.Id;
		K.Victim = S.Victim;
		K.By = S.By;
		K.Nth = S.Sent;
		K.World = World;
		K.Spot = Vec(Shots.U(-0.2, 0.2), Shots.U(-0.2, 0.2), Shots.U(-0.1, 0.1));
		K.In = In;
		K.Kind = Kind;
		// (the skin as the run walks in; what lies at its end once it is there)
		K.Part = AirT - S.OnSince >= 0.25 ? S.Part : std::string("skin");
		InFlight.push_back(K);
		S.Sent++;
		S.NextStrike = AirT + Shots.U(0.05, 0.1);
	}

	bool Show::Masked(const Vec& At) const
	{
		// (no strike is drawn where an aircraft already going down is passing: the scene's rule)
		for (const auto& Pair : Crafts)
		{
			const Craft& Q = Pair.second;
			if (Q.Doing == Role::Fall && Len(Q.P - At) < 0.6 * Q.Form.Span + 2.0) return true;
		}
		return false;
	}

	void Show::LandStrikes()
	{
		for (size_t K = 0; K < InFlight.size();)
		{
			if (InFlight[K].At > AirT) { ++K; continue; }
			const Landing St = InFlight[K];
			InFlight.erase(InFlight.begin() + static_cast<long>(K));
			Craft* V = Find(St.Victim);
			if (!V || V->Doing == Role::Fall || V->Doing == Role::Gone || !V->bShown) continue;
			// Where the rounds end, on the aircraft as it was when they came: there they strike. Turned out of their
			// way while they flew, it is not struck: they end in the air beside it, a miss after all.
			// (as the picture has it: between its last frame's place and this one's, lying as it lies now)
			const Vec Lift(0.0, 0.0, LiftAt(*V, AirT));
			const Vec Then = (V->bHasLast && GunDt > 1e-9 ? V->P - (V->P - V->LastP) * ((AirT - St.At) / GunDt) : V->P - V->GunV * (AirT - St.At)) + Lift;
			Vec Spot = ToOwn(*V, St.World - Then);
			if (!InAirframe(V->Form, Spot, 0.75))
			{
				// (or come through its skin in their last two metres: there, then)
				const Vec Back = ToOwn(*V, St.In);
				bool bIn = false;
				for (int Step = 1; Step <= 5 && !bIn; ++Step)
				{
					bIn = InAirframe(V->Form, Spot + Back * (0.4 * Step), 0.0);
					if (bIn) Spot += Back * (0.4 * Step);
				}
				if (!bIn) { Check.StrikesWide++; continue; }
			}
			Shot* S = ShotOf(St.Shot);
			if (S && S->bDone) S = nullptr;
			// (a little scatter; and drawn on the skin, not in the air a hand's breadth off it)
			Spot = OnAirframe(V->Form, Spot + St.Spot);
			const Vec At = Local(*V, Spot) + Lift;
			if (Masked(At)) Check.StrikesDropped++;
			else
			{
				Fx& H = Emit(FxKind::Hit, At, St.Kind, V->Key);
				H.Text = "air";
				H.Aim = Spot;
				H.Vel = Velocity(*V);
				H.Normal = Norm(Norm(At - V->P, Up) + St.In, Up);
				H.Id = St.Shot;
				H.Nth = St.Nth;
				H.Part = St.Part;
				Check.Hits++;
				Check.Strikes++;
				V->StruckAt = AirT;
				V->StruckBy = St.By;
			}
			if (!S) continue;
			S->Landed++;
			if (S->First >= 0.0) continue;
			// The first of them home: the damage follows as the burst goes on, when it was timed to as they left
			// (SendStrike).
			S->First = AirT;
			V->Struck++;
			(S->Mount >= 0 ? Check.GunnerBurstsOn : Check.BurstsOn)++;
			S->ReleaseAt = std::max(S->Plan, AirT + 0.02);
			if (S->E.What == "air:lost") V->FallDue = S->ReleaseAt;
			if (Moment* M = MomentOf(S->Id)) { M->Stage = 1; M->StrikeAt = AirT; }
		}
	}

	void Show::BeginDamage()
	{
		for (size_t K = 0; K < Booked.size(); ++K)
		{
			if (Booked[K].bDone) continue;
			Craft* V = Find(Booked[K].Victim);
			// (the one it was on has gone another way, or the battle is over: back to be shown as it can be)
			if (!V || !Live(*V) || V->ShotOn != Booked[K].Id || (bEnded && Booked[K].First < 0.0))
			{
				Unbook(Booked[K], true);
				continue;
			}
			if (Booked[K].ReleaseAt < 0.0 || Booked[K].ReleaseAt > AirT) continue;
			// (the last of its rounds landed a frame ago, and the aircraft's own guns quiet a frame: so none of either
			// is drawn in the air about it as it goes; a moment more at most)
			if (AirT < Booked[K].ReleaseAt + 0.3)
			{
				const auto Own = Firing.lower_bound(V->Key * 64);
				if (Booked[K].LastLand > FiredT || (Booked[K].E.What == "air:lost" && Own != Firing.end() && *Own < (V->Key + 1) * 64)) continue;
			}
			const Shot S = Booked[K];
			Booked[K].bDone = true;
			V->ShotOn = 0;
			if (S.Mount < 0) if (Craft* G = Find(S.Shooter)) G->bScored = true; // (one burst, one aircraft)
			if (Moment* M = MomentOf(S.Id)) { M->Stage = 2; M->FallAt = AirT; M->Ended = AirT; }
			if (S.E.What == "air:lost")
			{
				Check.ToGunsStruck++;
				if (BoxOf(S.E) >= 0) Check.GunnerKillsSeen++;
				Check.FallsHeld++;
				Band& B = Bands[S.E.Unit];
				B.Pending = std::max(0, B.Pending - 1);
				// (its rounds still in the air are not drawn striking it as it falls)
				InFlight.erase(std::remove_if(InFlight.begin(), InFlight.end(), [&S](const Landing& St) { return St.Victim == S.Victim; }), InFlight.end());
			}
			else Check.HitsStruck++;
			Release(S.E, S.Victim, &S.Spot, false);
		}
		Booked.erase(std::remove_if(Booked.begin(), Booked.end(), [](const Shot& S) { return S.bDone; }), Booked.end());
		// (a moment is kept 8 s after its end, for a camera still on it)
		Seen.erase(std::remove_if(Seen.begin(), Seen.end(), [this](const Moment& M) { return M.Ended >= 0.0 && AirT - M.Ended > 8.0; }), Seen.end());
	}

	void Show::Unbook(Shot& S, bool bHoldAgain)
	{
		if (S.bDone) return;
		S.bDone = true;
		if (Craft* V = Find(S.Victim))
		{
			if (V->ShotOn == S.Id) V->ShotOn = 0;
			V->FallDue = 1e18;
		}
		if (Moment* M = MomentOf(S.Id)) { M->Stage = -1; M->Ended = AirT; }
		// (what it has in the air strikes nothing: no strike is drawn that no damage follows)
		const int Id = S.Id;
		InFlight.erase(std::remove_if(InFlight.begin(), InFlight.end(), [Id](const Landing& St) { return St.Shot == Id; }), InFlight.end());
		if (!bHoldAgain) return;
		Held H;
		H.E = S.E;
		H.At = S.HeldUntil;
		// (a gunner lost it, its time up: a breath, in which another of his box who has it in his arc may take it up)
		if (S.Mount >= 0 && !bEnded && AirT - S.HeldSince < 30.0) H.At = std::max(H.At, AirT + 0.2);
		H.bForShot = true;
		H.bMore = S.bMore;
		H.Since = S.HeldSince;
		Later.push_back(H);
	}

	void Show::ReleaseUnstruck(const Held& H)
	{
		// No gun came to bear in the time: it shows on one of its group those guns struck a little while ago (hit
		// then, it goes now), if there is one; else as it can.
		const Event& E = H.E;
		const bool bLost = E.What == "air:lost";
		if (bLost)
		{
			Band& B = Bands[E.Unit];
			B.Pending = std::max(0, B.Pending - 1);
			Check.FallsHeld++;
		}
		const int Box = BoxOf(E);
		const int Guns = Box >= 0 ? Box : E.By;
		const bool bLights = E.Mark == "smoke" || E.Mark == "fireout";
		int Best = -1;
		double Latest = AirT - 20.0;
		for (const auto& Pair : Crafts)
		{
			const Craft& V = Pair.second;
			if (V.Group != E.Unit || !Live(V) || V.ShotOn != 0 || V.StruckBy != Guns || V.StruckAt <= Latest) continue;
			if (bLost && (E.Strag >= 0 ? V.StragId != E.Strag : V.StragId >= 0)) continue;
			if (E.What == "air:straggle" && (V.bFighter || V.StragId >= 0 || V.Doing != Role::Slot)) continue;
			if (E.What == "air:hit" && (V.Doing == Role::Ground || (bLights && V.TrailKey))) continue;
			Best = V.Key;
			Latest = V.StruckAt;
		}
		if (bLost) (Best >= 0 ? Check.ToGunsLate : Check.ToGunsUnseen)++;
		else (Best >= 0 ? Check.HitsLate : Check.HitsUnseen)++;
		Release(E, Best);
	}

	const std::vector<Show::MountAt>& Show::MountsOf(const Craft& C, const Loadout& L) const
	{
		auto [Kept, bNew] = MountsKept.try_emplace(std::make_pair(&L, std::make_pair(C.Model, C.bFighter)));
		if (!bNew) return Kept->second;
		// The window of each mount: by where its arc looks, or for a power turret by its socket; one gunner to an
		// opening.
		std::vector<MountAt>& Mounts = Kept->second;
		std::set<std::string> Used;
		auto ChoicesOf = [](const Gun& Row) -> std::vector<std::string>
		{
			if (!Row.bArc) return { "top", "ball", "chin", "radio" };
			double Mid = Row.Yaw0 + std::fmod(Row.Yaw1 - Row.Yaw0 + 360.0, 360.0) * 0.5;
			Mid = std::fmod(Mid + 360.0, 360.0);
			if (Mid < 45.0 || Mid >= 315.0) return { "nose", "chin" };
			if (Mid < 135.0) return { "waistR" };
			if (Mid >= 225.0) return { "waistL" };
			// (looking aft: the tail guns; one that looks only up is the radio room's, only down the belly's)
			if (Row.Pitch0 >= 0.0 && Row.Pitch1 >= 45.0) return { "radio", "tail" };
			if (Row.Pitch1 <= 0.0) return { "ball", "tail" };
			return { "tail" };
		};
		// (the turrets and the mounts with one place of their own first, so a second gun looking the same way never
		// takes the tail gunner's window or a turret's; then the others, each in its row's order)
		std::map<const Gun*, std::string> Windows;
		for (int Round = 0; Round < 2; ++Round)
		{
			for (const Gun& Row : L.Guns)
			{
				if (Row.Mount == "fixed") continue;
				const std::vector<std::string> Choices = ChoicesOf(Row);
				if ((Round == 0) != (!Row.bArc || Choices.size() == 1)) continue;
				for (const std::string& Ch : Choices)
				{
					if (Used.count(Ch) || !C.Form.Stations.count(Ch)) continue;
					Used.insert(Ch);
					Windows[&Row] = Ch;
					break;
				}
			}
		}
		// Her gunners' keys are 32 to 63 of her own 64, one gun to a key: the sound hears a key as one gun, and the key
		// after 63 is the next aircraft's first gun (four keys to every row of her loadout gave a B-17's ninth row 64:
		// the sound director's review, 21 November). They are numbered as her windows are manned, four keys a window,
		// so a mount's guns are its first key and those after it, the same every frame; a mount of more than four guns
		// takes a key for each. A window whose guns would pass 63 is not manned.
		int Index = 32;
		for (const Gun& Row : L.Guns)
		{
			const auto Its = Windows.find(&Row);
			if (Its == Windows.end()) continue;
			const int Of = std::max(1, static_cast<int>(Row.At.size()));
			if (Index + Of > 64) break;
			MountAt M;
			M.Row = &Row;
			M.Window = Its->second;
			M.Station = C.Form.Stations.at(Its->second);
			M.Index = Index;
			Mounts.push_back(M);
			Index += std::max(4, Of);
		}
		return Mounts;
	}

	bool Show::OwnClear(const Craft& C, const Vec& Station, const Vec& Way, double Dt)
	{
		// Never through his own aircraft: from two metres out of his window, his line clears her fuselage, her wings,
		// her engines, her tailplane and her fin (a waist gunner could put a burst through his own tail).
		// (a twin mount's guns stand a hand's breadth either side of his window's middle)
		if (RayMeets(C.Form, Station, Way, 2.0, 40.0, 0.35)) return false;
		if (Dt <= 0.0) return true;
		// And as she will lie a frame on, rolling and turning as she is: rounds just out of the gun are drawn against
		// her as she lies then, a wing come up or the tail swung into their line.
		const Vec F0 = Ahead(C.Hdg, C.Gam), Ra = RightOf(C.Hdg), Ua = Ra.Cross(F0);
		const double Cb0 = std::cos(C.Bank), Sb0 = std::sin(C.Bank);
		const Vec R0 = Ra * Cb0 - Ua * Sb0, U0 = Ua * Cb0 + Ra * Sb0;
		const double Hdg = C.Hdg + C.TurnNow * Dt, Gam = C.Gam + C.PullNow * Dt, Bank = C.Bank + C.RollNow * Dt;
		const Vec F1 = Ahead(Hdg, Gam), Rb = RightOf(Hdg), Ub = Rb.Cross(F1);
		const double Cb1 = std::cos(Bank), Sb1 = std::sin(Bank);
		const Vec R1 = Rb * Cb1 - Ub * Sb1, U1 = Ub * Cb1 + Rb * Sb1;
		auto Then = [&](const Vec& L)
		{
			const Vec W = F0 * L.X + R0 * L.Y + U0 * L.Z;
			return Vec(W.Dot(F1), W.Dot(R1), W.Dot(U1));
		};
		return !RayMeets(C.Form, Then(Station), Then(Way), 2.0, 40.0, 0.35);
	}

	bool Show::Bears(const Craft& C, const MountAt& M, const Loadout& L, const Craft& Foe) const
	{
		const Gun& Row = *M.Row;
		const Vec Rel = Foe.P - C.P;
		if (Len(Rel) >= (Row.Kind == "rifle" ? L.ReachRifle : L.ReachHeavy)) return false;
		const Vec F0 = Ahead(C.Hdg, C.Gam), R0 = RightOf(C.Hdg), U0 = R0.Cross(F0);
		const double Fx_ = Rel.Dot(F0), Fy = Rel.Dot(R0), Fz = Rel.Dot(U0);
		double Yaw = std::atan2(Fy, Fx_) / Deg;
		if (Yaw < 0.0) Yaw += 360.0;
		const double Pitch = std::atan2(Fz, std::sqrt(Fx_ * Fx_ + Fy * Fy)) / Deg;
		if (Row.bArc)
		{
			const double Span_ = std::fmod(Row.Yaw1 - Row.Yaw0 + 360.0, 360.0);
			const double Into = std::fmod(Yaw - Row.Yaw0 + 360.0, 360.0);
			if (!(Into <= Span_ && Pitch >= Row.Pitch0 && Pitch <= Row.Pitch1)) return false;
		}
		else
		{
			// A power turret: the top one above the wing, the ball below, the chin ahead and below.
			if (!(M.Window == "top" ? Pitch > -5.0 : M.Window == "ball" ? Pitch < 5.0 : M.Window == "chin" ? Fx_ > 0.0 && Pitch < 20.0 : Pitch > 0.0)) return false;
		}
		return OwnClear(C, M.Station, Norm(Vec(Fx_, Fy, Fz)));
	}

	void Show::GiveGunners()
	{
		// What the battle laid to a box's gunners and the scene has yet to show is given to the gunner who is to
		// carry it as the fighter comes in: one whose window it is coming into, or is in already, and will stay in
		// long enough for him to range on it and then to hit it. The bomber the battle names has it before any
		// other of her box while it is coming her way. He is given it a second or more before he can strike, so
		// a camera can be on his window as he fires.
		std::set<int> Boxes;
		for (const Held& H : Later) if (H.bForShot && BoxOf(H.E) >= 0) Boxes.insert(BoxOf(H.E));
		for (int Box : Boxes)
		{
			const auto Its = Now.find(Box);
			if (Its == Now.end()) continue;
			const int Per = std::max(1, Its->second.Per);
			std::vector<Craft*> Ships;
			for (int Key_ : MembersOf(Box))
			{
				Craft& C = Crafts.find(Key_)->second;
				if (C.Group == Box && !C.bFighter && C.bShown && (C.Doing == Role::Slot || C.Doing == Role::Straggle)) Ships.push_back(&C);
			}
			if (Ships.empty()) continue;
			const auto L = Loadouts.find(Ships.front()->Nation + ":bmr");
			if (L == Loadouts.end()) continue;
			const double Reach = std::max(L->second.ReachRifle, L->second.ReachHeavy);
			for (auto& Pair : Crafts)
			{
				Craft& X = Pair.second;
				if (!X.bFighter || X.ShotOn != 0 || !X.bShown || !Live(X) || X.Doing == Role::Ground) continue;
				const int At = Wanted(-1, Box, X);
				if (At < 0) continue;
				// (one a gunner has just lost, throwing itself about: not given to another until it flies steadily
				// enough to be hit)
				bool bLostLately = false;
				for (const Moment& Told : Seen) bLostLately = bLostLately || (Told.Victim == X.Key && Told.Gun >= 0 && Told.Stage == -1 && AirT - Told.Ended < 6.0);
				if (bLostLately && X.GunErr > 0.2) continue;
				const Held& H = Later[static_cast<size_t>(At)];
				const Vec Xv = X.GunV;
				Craft* Best = nullptr;
				MountAt BestM;
				double BestScore = 1e18;
				bool bNamedComing = false;
				// (on its run in, or on the wing of one: where it will be in a few seconds can be told)
				const Craft* Runs = X.Step == Phase::Wing ? Find(X.Lead) : &X;
				const bool bRunsIn = Runs && Runs->Step == Phase::Run;
				for (Craft* Ship : Ships)
				{
					Craft& C = *Ship;
					const Vec Rel = X.P - C.P;
					const double Dist = Len(Rel);
					const bool bNamed = H.E.GunSeat >= 0 && C.Slot == H.E.GunSeat / Per;
					if (Dist > Reach + (bNamed ? 700.0 : 320.0)) continue;
					// (how fast the line to it swings)
					const Vec Vr = Xv - C.GunV;
					const double Swing = Len(Rel.Cross(Vr)) / std::max(1.0, Dist * Dist) / Deg;
					for (const MountAt& M : MountsOf(C, L->second))
					{
						const int Key = C.Key * 64 + M.Index;
						if (Carrying(C.Key, Key)) continue;
						// In his arc and his reach now, or within the second; and still in them a second after that.
						auto Then = [&](double T)
						{
							Craft Q = X;
							Q.P = PlaceIn(X, T) - C.GunV * T;
							return Bears(C, M, L->second, Q);
						};
						const bool bNow = Bears(C, M, L->second, X);
						const bool bSoon = bNow || Then(1.0);
						// (on its run, and a gunner of the bomber the battle names will have it within three seconds: it is
						// his to wait for)
						if (bNamed && bRunsIn && !bSoon && (Then(2.0) || Then(3.0))) bNamedComing = true;
						if (!bSoon || !Then(1.9)) continue;
						// (the steadier the line to it the better; a power turret before a hand-held gun; one in his
						// arc already before one coming into it)
						double Score = Swing + (M.Row->Mount == "hand" ? 3.0 : 0.0) + (bNow ? 0.0 : 4.0) - (bNamed ? 1000.0 : 0.0);
						// (his gun on another just now: the seconds it would take him to swing it round to this one. Too
						// many, and it is not his)
						const auto Laid = Lays.find(Key);
						if (Laid != Lays.end() && Laid->second.T >= 0.0 && AirT - Laid->second.T <= 0.5)
						{
							const double Turn = Between(Laid->second.Way, Norm(Rel, Ahead(C.Hdg))) / ((M.Row->Mount == "hand" ? 50.0 : 40.0) * Deg);
							if (Turn > 2.0) continue;
							Score += 2.0 * Turn;
						}
						// (and a line to it that no other aircraft is in)
						Barrel B;
						B.At = Local(C, M.Station);
						B.Carry = Velocity(C);
						B.Mv = M.Row->Mv;
						B.Fl = FlightOf(M.Row->Kind);
						B.Own = C.Key;
						Vec Way;
						double Tof = 0.0;
						if (bNow && Solve(B, X, Vec(), Way, Tof))
						{
							const Met Mt = Meets(B, Way, 0.4, 0.0);
							if (Mt.Key != X.Key) Score += 60.0;
						}
						if (Score < BestScore) { BestScore = Score; Best = &C; BestM = M; }
					}
				}
				if (!Best) continue;
				// (coming into the arcs of the bomber the battle names: hers to have, until the wait is all but over)
				if (bNamedComing && BestScore > -500.0 && (H.At > AirT + 0.3 || AwaitsGunners(H))) continue;
				Shot* S = ShotOf(Book(At, *Best, X, Best->Key * 64 + BestM.Index, BestM.Window, BestM.Station));
				S->OnAt = AirT + Shots.U(1.0, 1.4);
				if (Moment* Told = MomentOf(S->Id)) Told->StrikeAt = S->OnAt + Len(X.P - Best->P) / 700.0;
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Guns, flak, the small things.

	void Show::FireGuns(double Dt)
	{
		GunDt = Dt;
		TrackAll();
		LandStrikes();
		// (a burst whose rounds have all come and found nothing, the aircraft gone another way after all: its timing
		// is off, and its next true rounds make another)
		for (Shot& Each : Booked)
		{
			if (Each.bDone || Each.First >= 0.0 || Each.Plan < 0.0 || AirT <= Each.LastLand) continue;
			Each.Plan = -1.0;
			Each.OnSince = -1.0;
			if (Craft* V = Find(Each.Victim)) V->FallDue = 1e18;
		}
		// (an aircraft flies on as it is flying for rounds on their way to it only where that keeps it off the ground)
		auto Safe = [](const Course& Co) { return Co.Low >= 90.0 || Co.Low >= Co.High - 3.0; };
		// (its fall a moment off, the burst that brings it down on its way: its own guns stop, so that none of their
		// rounds is in the air about it as it goes)
		auto Going = [this, Dt](const Craft& C) { return AirT + 2.0 * Dt + 0.05 >= C.FallDue; };
		struct Want
		{
			double Far = 0.0;
			Fx F;
			bool bGround = false;
			Vec End, In; // (a stream that comes down: where on the ground or the sea, and its rounds' way as they land)
		};
		std::vector<Want> Asked;
		std::set<int> Touched;
		// How far off his nose a fighter's fixed guns may be laid: to bring a burst the battle counted on to its mark,
		// and to take a miss clear of the aircraft he fires at. Guns were harmonised and aircraft skidded by more.
		const double LayHit = 3.0 * Deg, LayAsk = 2.4 * Deg, LayMiss = 2.5 * Deg;
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
		// Whether rounds laid on an aircraft reach it (Strike seconds on) before they come down on the ground or the
		// sea: one standing on the ground, and one a fight has taken down to it. (A fight flown at a few hundred feet
		// can put a fighter under the sea in a hard turn, which takes all his g and leaves him none to lift his nose:
		// the pilots' matter. No round is laid to follow him there.)
		auto Reaches = [this](const Barrel& B, const Vec& Way, double Strike)
		{
			const double Down = ToGround(B, Way);
			return Down <= 0.0 || Down > Strike - 0.004;
		};
		// (nor does any gun fire from an aircraft down at the ground or the sea, or under it: its rounds would be drawn
		// there)
		auto Sunk = [this](const Craft& C) { return C.P.Z < GroundAt(C.P.X, C.P.Y) + 2.0; };
		// Whether a stream is low enough for any round of it to reach the ground or the sea in its life.
		auto Low = [this](const Barrel& B, const Vec& Way)
		{
			const double Sink = -std::min(0.0, Way.Z * B.Mv + B.Carry.Z) * B.Fl.Life;
			return B.At.Z - GroundAt(B.At.X, B.At.Y) < Sink + 400.0;
		};
		// A stream that comes down on the ground or the sea ends there, each round with its strike: each gun's own
		// rounds, from where it stands, by their own flight. Short: the seconds at which it ends anyway, short of an
		// aircraft (0: it does not). False: it does not come down first.
		auto Grounded = [this](Want& W, const Flight& Fl, double Short)
		{
			Barrel Bg;
			Bg.At = W.F.At;
			Bg.Carry = W.F.Vel;
			Bg.Mv = W.F.Mv;
			Bg.Fl = Fl;
			const Vec Own = Norm(W.F.Aim - W.F.At);
			const double At = ToGround(Bg, Own);
			if (At <= 0.0 || (Short > 0.0 && Short <= At)) return false;
			const Vec V0 = Own * W.F.Mv + W.F.Vel;
			W.bGround = true;
			W.F.Tof = At;
			W.End = RoundAt(W.F.At, V0, At, Fl.Drag, Fl.Drop);
			W.F.Reach = Len(W.End - W.F.At);
			W.End.Z = GroundAt(W.End.X, W.End.Y);
			W.In = Norm(V0 * std::exp(-Fl.Drag * At) - Vec(0.0, 0.0, Fl.Drop * At), Own);
			return true;
		};

		// The fighter groups whose guns the battle has counted a hit by, the scene has yet to show.
		std::set<int> Debtors;
		for (const Held& H : Later) if (H.bForShot && BoxOf(H.E) < 0) Debtors.insert(H.E.By);
		// The fighters' fixed guns: along the nose, as the pilot has it.
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			if (!C.bFighter) continue;
			// (his thumb off the button, an aircraft in his sights and in range: he may yet open, below)
			const bool bAsk = !C.bFiring && C.Doing == Role::Fight && C.Ready >= 0 && C.Ready == C.Target;
			// (or his guns free in a fight, and a hit of his group's guns waiting to be shown: he may open on whichever
			// aircraft it can be shown on that crosses his sights, the one he is after or not)
			const bool bSnap = !C.bFiring && !bAsk && C.Doing == Role::Fight && C.Burst <= 0.0 && C.Rest <= 0.5 && Debtors.count(C.Group)
				&& C.Step != Phase::Away && C.Step != Phase::Break && C.Step != Phase::Zoom && !Dodging(C) && !C.bStrafing;
			if (!C.bFiring && !bAsk && !bSnap) continue;
			if (Going(C) || Sunk(C))
			{
				C.bFiring = false;
				continue;
			}
			auto L = Loadouts.find(C.Nation + ":ftr");
			if (L == Loadouts.end()) continue;
			const Gun* Main = nullptr;
			double Outer = 0.0; // (how far out from his nose line his outermost gun stands)
			for (const Gun& Row : L->second.Guns)
			{
				if (Row.Mount != "fixed") continue;
				if (!Main) Main = &Row;
				const int Of = static_cast<int>(Row.At.size());
				for (int K = 0; K < Of; ++K)
				{
					if (Has(L->second.Silent, Row.At[static_cast<size_t>(K)])) continue;
					Outer = std::max(Outer, std::fabs(Muzzle(C.Form, Row.At[static_cast<size_t>(K)], Row.Kind, K, Of).Y));
				}
			}
			if (!Main) continue;
			const Vec Nose = Ahead(C.Hdg, C.Gam);
			const Vec V = Velocity(C);
			const double Conv = L->second.Conv > 0.0 ? L->second.Conv : 300.0;
			Barrel B;
			B.At = Local(C, Vec(C.Form.Long * 0.1, 0.0, 0.0));
			B.Carry = V;
			B.Mv = Main->Mv;
			B.Fl = FlightOf(Main->Kind);
			B.Own = C.Key;
			Craft* T = C.Target >= 0 ? Find(C.Target) : nullptr;
			if (T && (!Live(*T) || !T->bShown)) T = nullptr;
			// A hit the battle has counted on the one in his sights, which the scene has yet to show: his burst carries
			// it, if his nose is near enough on for his rounds to be laid there, the aircraft is flying steadily enough
			// to be found where they are sent, and nothing is in their way. His thumb off, he opens for it.
			Shot* S = Carrying(C.Key, -1);
			const bool bOpen = bAsk || bSnap;
			if (!S && !C.bScored && (bOpen || AirT - C.BurstAt >= 0.15) && Debtors.count(C.Group))
			{
				Vec Way;
				double Tof = 0.0;
				int At = -1;
				const double Range = (bOpen ? 1.0 : 1.2) * (C.ReadyRange > 0.0 ? C.ReadyRange : 500.0);
				Craft* Mk = Mark(C, B, T ? T->Group : -1, Range, bOpen ? LayAsk : LayHit, true, At, Way, Tof);
				if (Mk && Sure(*Mk, Tof))
				{
					const Met M = Meets(B, Way, 0.4 + 0.5 * Outer, 0.0);
					if (M.Key == Mk->Key && M.bThrough && Reaches(B, Way, M.Strike))
					{
						if (bOpen)
						{
							C.Burst = Shots.U(0.6, 1.2);
							C.Rest = C.Burst + Shots.U(0.8, 1.8);
							C.BurstAt = AirT;
							C.bFiring = true;
							Mk->ShotAt = AirT;
							Mk->ShotBy = C.Group;
							Check.Bursts++;
							Check.BurstsAsked++;
						}
						S = ShotOf(Book(At, C, *Mk, -1, std::string(), Vec()));
						// (one that crossed his sights, not the one his pass or his chase is flown on, or one that will fly on
						// hit: he holds his sight on it, and the one he was after waits: KeepsSight)
						if (bSnap || Mk->Key != C.Target || S->E.What != "air:lost")
						{
							if (C.SightOn < 0) C.SightBack = C.Target;
							C.SightOn = Mk->Key;
							C.SightUntil = AirT + 4.0;
							C.Target = Mk->Key;
							T = Mk;
						}
					}
				}
			}
			if (!C.bFiring) continue;
			Check.StreamSteps++;
			Craft* Vic = nullptr;
			if (S)
			{
				Vic = Find(S->Victim);
				// (it has gone another way: what the burst has in the air may yet land; if nothing does, the hit goes
				// back to wait)
				if (!Vic || !Live(*Vic) || !Vic->bShown)
				{
					S = nullptr;
					Vic = nullptr;
				}
				else Touched.insert(S->Id);
			}
			// The lay. True, on the place the battle names, while the burst carries a hit and his nose is near enough
			// on; else a miss, to be seen to miss: wide of the one he fires at, and ended short of any other beyond it.
			bool bTrue = false;
			Vec Way = Nose, Point;
			double Tof = 0.0, Flight0 = 0.0, End = 0.0, Stop = -1.0;
			double Clear = 1.0; // (a miss: the share of what its mark may yet do that it is laid clear of)
			Course Run;
			if (S)
			{
				// The one it is for flies on as it is flying while the rounds are on their way (Fly holds it to that):
				// its own flight, played ahead, tells where they come to it.
				Vec TrueWay;
				Point = WalkOf(*S);
				Run = CourseOf(*Vic, Dt, B.Fl.Life + 2.0 * Dt);
				const bool bSolved = Safe(Run) && SolveOn(B, Run, Point, TrueWay, Flight0);
				// (the damage timed: a round fired now would come after it had begun, on an aircraft no longer flying as
				// it is; his thumb comes off, the last of his burst still on its way)
				Stop = S->ReleaseAt >= 0.0 ? S->ReleaseAt : S->Plan;
				if (Stop >= 0.0 && (!bSolved || AirT + Flight0 + Dt >= Stop))
				{
					C.bFiring = false;
					C.Burst = 0.0;
					// (strikes seen on one that will fly on: he fires again as soon as they have told, if his guns still bear)
					if (S->E.What != "air:lost") C.Rest = std::min(C.Rest, 0.5 + std::max(0.0, Stop - AirT));
					continue;
				}
				if (bSolved && Between(TrueWay, Nose) <= LayHit + 1.0 * Deg && (AirT < Vic->HeldTo || Sure(*Vic, Flight0)))
				{
					// (where they come into its airframe; and no other aircraft's skin within his guns' spread of the way
					// there)
					// (his guns stand as far apart as Outer at the muzzle; and no strike could be drawn where an aircraft
					// going down will be passing; nor, to one on the ground, may the ground itself come first)
					const double In = EntryOf(B, TrueWay, Run, Vic->Form, Flight0);
					const double Lands = In >= 0.0 ? In : Flight0;
					const Met M = Meets(B, TrueWay, 0.4 + Outer, 0.0, Vic->Key);
					if ((!M.Key || M.Tof > Lands) && !Shadowed(RoundAt(B.At, TrueWay * B.Mv + B.Carry, Lands, B.Fl.Drag, B.Fl.Drop), Lands, Vic->Key) && Reaches(B, TrueWay, Lands))
					{
						bTrue = true;
						Way = TrueWay;
						Tof = Lands;
					}
				}
			}
			if (!bTrue)
			{
				bool bClear = true;
				double Least = 0.3, Margin = 2.0;
				if (T)
				{
					const double Range = Len(T->P - B.At);
					const double Flight1 = Range / std::max(200.0, B.Mv);
					Least = Flight1 + 0.12;
					// (his guns stand apart and cross at their distance; the kit scatters each round)
					Margin = 1.2 + Outer * std::min(1.0, std::fabs(1.0 - Range / Conv));
					// Wide of it by all it may yet do while the rounds are on their way (it may not be quite where its
					// flight tells by then, and its wings may have rolled into their line), if his guns can be laid so
					// far over; if not, by half of that; at the least by what the eye can tell from a hit on one flying
					// steadily.
					const double Bare = Margin + 0.005 * Range + (T->bFighter && T->Doing == Role::Fight ? 6.0 : 1.0) * Flight1 * Flight1;
					const double Full = Margin + 0.005 * Range + DriftOf(*T, Flight1);
					bClear = false;
					for (int Try = 0; Try < 3 && !bClear; ++Try)
					{
						const double Share = 1.0 - 0.5 * Try;
						Clear = Share;
						Vec Spot, Wide;
						double Unused = 0.0;
						if (!WideOf(B, Nose, *T, std::max(Bare, Bare + (Full - Bare) * Share), Share, Spot)) bClear = true;
						else if (Solve(B, *T, Spot, Wide, Unused) && Between(Wide, Nose) <= LayMiss)
						{
							bClear = true;
							Way = Wide;
							Check.WideSteps++;
						}
					}
				}
				if (bClear)
				{
					const Met M = Meets(B, Way, 0.6 * Margin, 0.004, 0, Clear);
					if (M.Key && (!T || M.Key != T->Key) && M.Tof >= Least) End = M.Short;
					else if (M.Key) bClear = false;
				}
				if (!bClear)
				{
					// No clean line: his thumb comes off the button.
					Check.HeldSteps++;
					C.bFiring = false;
					C.Burst = 0.0;
					continue;
				}
				if (End > 0.0) Check.ShortSteps++;
			}
			Check.MostLay = std::max(Check.MostLay, Between(Way, Nose) / Deg);
			// (laid true: where on the aircraft the rounds come to its skin, its own frame)
			const Vec Skin = bTrue ? OwnOn(Run, Tof, RoundAt(B.At, Way * B.Mv + B.Carry, Tof, B.Fl.Drag, B.Fl.Drop)) : Vec();
			// (and each gun's rounds as laid: when they end, where, and what they are)
			struct Laid
			{
				double Tof = 0.0;
				Vec End, In;
				std::string Kind;
			};
			std::vector<Laid> True;
			// (low enough, a stream laid to miss comes down on the ground or the sea before it has flown out)
			const bool bLow = !bTrue && Low(B, Way);
			bool bDown = false;
			int Index = 0;
			for (const Gun& Row : L->second.Guns)
			{
				if (Row.Mount != "fixed") continue;
				const int Of = static_cast<int>(Row.At.size());
				const Flight Fl = FlightOf(Row.Kind);
				// Each kind of gun flies its own rounds: laid true, each kind on the same place; laid to miss, a kind
				// whose slower rounds would not pass clear holds its fire.
				double RowEnd = bTrue ? Tof : End;
				bool bRow = true;
				if (!bTrue && &Row != Main && (Row.Mv != Main->Mv || Row.Kind != Main->Kind))
				{
					Barrel Br = B;
					Br.Mv = Row.Mv;
					Br.Fl = Fl;
					const Met M = Meets(Br, Way, 1.2, 0.004, 0, Clear);
					const double Least = T ? Len(T->P - B.At) / std::max(200.0, Row.Mv) + 0.12 : 0.3;
					if (M.Key && (!T || M.Key != T->Key) && M.Tof >= Least) RowEnd = RowEnd > 0.0 ? std::min(RowEnd, M.Short) : M.Short;
					else if (M.Key) bRow = false;
				}
				for (int K = 0; K < std::max(1, Of); ++K)
				{
					const std::string Socket = Of > 0 ? Row.At[static_cast<size_t>(K)] : std::string("gun.nose");
					if (Has(L->second.Silent, Socket) || !bRow) { Index++; continue; }
					const Vec Here = Local(C, Muzzle(C.Form, Socket, Row.Kind, K, Of));
					Want W;
					W.F.Kind = FxKind::Gun;
					W.F.Key = C.Key * 64 + Index++;
					W.F.At = Here;
					W.F.Vel = V;
					W.F.What = Row.Kind;
					W.F.Text = Row.Mount;
					W.F.Rate = Row.Rate;
					W.F.Mv = Row.Mv;
					W.Far = Len(Here - Eye);
					if (bTrue)
					{
						// Every gun on the one place, where the rounds come to the skin, by its own way there from
						// where it stands and its own rounds' flight. Each gun's rounds end where they themselves come
						// into the airframe: short of the place, should a fin or a tailplane stand in their own way.
						Barrel Bg = B;
						Bg.At = Here;
						Bg.Mv = Row.Mv;
						Bg.Fl = Fl;
						Vec Own;
						double Its = 0.0;
						if (!SolveOn(Bg, Run, Skin, Own, Its) || Between(Own, Nose) > LayHit + 3.0 * Deg) continue;
						const double In = EntryOf(Bg, Own, Run, Vic->Form, Its);
						if (In >= 0.0) Its = In;
						// (slower rounds that would land after the damage has begun are not fired)
						if (Stop >= 0.0 && AirT + Its + Dt >= Stop) continue;
						// (a kind whose rounds fly another way there than his main guns', led farther ahead of it: by a
						// way of their own clear of every other aircraft, or that gun holds its fire)
						if (&Row != Main && (Row.Mv != Main->Mv || Row.Kind != Main->Kind))
						{
							const Met Mg = Meets(Bg, Own, 0.4, 0.0, Vic->Key);
							if (Mg.Key && Mg.Tof <= Its) continue;
						}
						W.F.Aim = Here + Own * std::max(50.0, Row.Mv * Its);
						W.F.Tof = Its;
						Laid Each;
						Each.Tof = Its;
						Each.End = RoundAt(Here, Own * Row.Mv + V, Its, Fl.Drag, Fl.Drop);
						Each.In = Norm(Own * (Row.Mv * std::exp(-Fl.Drag * Its)) + V - Vec(0.0, 0.0, Fl.Drop * Its) - (Run.At[1].P - Run.At[0].P) * (1.0 / Dt)) * -1.0;
						Each.Kind = Row.Kind;
						True.push_back(Each);
						W.F.Reach = Len(Each.End - Here);
						W.Far = -1.0; // (the burst the eye is to follow is never the one left undrawn)
					}
					else
					{
						W.F.Aim = B.At + Way * (Socket == "gun.nose" ? 450.0 : Conv);
						// (a gun that stands ahead of the others on his nose: its rounds are there the sooner)
						const double Ends = RowEnd > 0.0 ? std::max(0.02, RowEnd - (Here - B.At).Dot(Way) / std::max(200.0, Row.Mv)) : 0.0;
						if (bLow && Grounded(W, Fl, Ends)) bDown = true;
						else if (Ends > 0.0)
						{
							const Vec Stops = RoundAt(Here, Norm(W.F.Aim - Here) * Row.Mv + V, Ends, Fl.Drag, Fl.Drop);
							// (ended short of an aircraft only just over the ground or the sea: the eye looks for its strike
							// there, and it has none. That gun holds its fire.)
							if (bLow && Stops.Z < GroundAt(Stops.X, Stops.Y) + 4.0) continue;
							W.F.Tof = Ends;
							W.F.Reach = Len(Stops - Here);
						}
					}
					Asked.push_back(W);
				}
			}
			if (bDown) Check.GroundSteps++;
			if (!True.empty())
			{
				// The aircraft flies on as it is flying until the last of them has landed; the strike seen is one gun's
				// rounds ending, each gun's in turn.
				double Latest = 0.0;
				for (const Laid& Each : True) Latest = std::max(Latest, Each.Tof);
				Vic->HeldTo = std::max(Vic->HeldTo, AirT + Latest + Dt + 1e-6);
				S->LastLand = std::max(S->LastLand, AirT + Latest);
				const Laid& One = True[static_cast<size_t>(S->Sent) % True.size()];
				SendStrike(*S, One.Tof, One.Kind, One.In, One.End);
			}
		}

		// The gunners of a box under attack: each at his own window, at a fighter inside his arc and his reach, never
		// through his own aircraft.
		struct Manned
		{
			Craft* C = nullptr;
			Craft* X = nullptr;
			MountAt M;
			int Shot = 0;
			bool bFire = false;
			double Far = 0.0;
		};
		std::vector<Manned> Crew;
		GiveGunners();
		// (the fighters in a fight, by the group each is after, in the order of their keys: nothing below moves them)
		std::map<int, std::vector<Craft*>> OnBox;
		for (auto& Pair : Crafts)
		{
			Craft& F = Pair.second;
			if (F.bFighter && F.Doing == Role::Fight) OnBox[F.TargetGroup].push_back(&F);
		}
		for (auto& Pair : Crafts)
		{
			Craft& C = Pair.second;
			if (C.bFighter || !(C.Doing == Role::Slot || C.Doing == Role::Straggle) || Going(C) || Sunk(C)) continue;
			auto L = Loadouts.find(C.Nation + ":bmr");
			if (L == Loadouts.end()) continue;
			// The nearest enemy fighter on a pass at this box.
			Craft* Foe = nullptr;
			double Near = 700.0;
			const auto OnIt = OnBox.find(C.Group);
			if (OnIt != OnBox.end()) for (Craft* Fp : OnIt->second)
			{
				Craft& F = *Fp;
				if (!F.bFighter || F.Side == C.Side || F.Doing != Role::Fight || F.TargetGroup != C.Group) continue;
				const double D = Len(F.P - C.P);
				if (D < Near) { Near = D; Foe = &F; }
			}
			bool bCarrying = false;
			for (const Shot& Each : Booked) bCarrying = bCarrying || (!Each.bDone && Each.Shooter == C.Key && Each.Mount >= 0);
			if (!Foe && !bCarrying) continue;
			for (const MountAt& M : MountsOf(C, L->second))
			{
				const int Key = C.Key * 64 + M.Index;
				Manned Man;
				Man.C = &C;
				Man.M = M;
				Man.X = Foe;
				// The fighter the battle gave this gunner (GiveGunners), while it is in his arc: he keeps on it whoever else
				// is nearer. Given it as it came in, he follows it with his gun, and opens when it is inside his reach.
				bool bBears = false;
				if (Shot* S = Carrying(C.Key, Key))
				{
					Craft* V = Find(S->Victim);
					bBears = V && Live(*V) && V->bShown && Bears(C, M, L->second, *V);
					// (not yet in it, and he has not fired on it yet: he waits for it a little)
					const bool bComing = V && Live(*V) && V->bShown && AirT - S->Booked < 2.5 && Lays[Key].Fired < S->Booked;
					// (he lost it, or could not get on it, before a round struck: another may take it up; what he
					// has in the air may yet land)
					if ((bBears || bComing) && (S->First >= 0.0 || AirT - S->Booked < 7.0))
					{
						Man.X = V;
						Man.Shot = S->Id;
						Touched.insert(S->Id);
					}
				}
				if (!Man.X) continue;
				if (!Man.Shot && !Bears(C, M, L->second, *Man.X)) continue;
				Check.GunnerSees[M.Window]++;
				// (he fires in bursts while he is on it; all through the one that carries a hit)
				Man.bFire = Man.Shot != 0 ? bBears : (AirT >= Lays[Key].HoldUntil && std::sin(AirT * 1.7 + C.Key * 0.37 + M.Index) > -0.25);
				Man.Far = Man.Shot ? -1.0 : Len(Local(C, M.Station) - Eye);
				Crew.push_back(Man);
			}
		}
		// (the kit draws so many guns, the nearest the eye: a gunner past that is followed, not fired or reckoned)
		double Cut = 1e18;
		{
			std::vector<double> Fars;
			for (const Want& W : Asked) Fars.push_back(W.Far);
			for (const Manned& Man : Crew) if (Man.bFire) Fars.insert(Fars.end(), std::max<size_t>(1, Man.M.Row->At.size()), Man.Far);
			const size_t Most = static_cast<size_t>(std::max(0, MostGuns));
			if (Fars.size() > Most && Most > 0)
			{
				std::nth_element(Fars.begin(), Fars.begin() + static_cast<long>(Most - 1), Fars.end());
				Cut = Fars[Most - 1];
			}
			else if (Most == 0) Cut = -2.0;
		}
		for (Manned& Man : Crew)
		{
			Craft& C = *Man.C;
			Craft& X = *Man.X;
			const Gun& Row = *Man.M.Row;
			const int Key = C.Key * 64 + Man.M.Index;
			Shot* S = Man.Shot ? ShotOf(Man.Shot) : nullptr;
			if (S && S->bDone) S = nullptr;
			// (her load just gone, she is drawn a little higher: her guns with her)
			const Vec Lift(0.0, 0.0, LiftAt(C, AirT));
			Barrel B;
			B.At = Local(C, Man.M.Station) + Lift;
			B.Carry = Velocity(C);
			B.Mv = Row.Mv;
			B.Fl = FlightOf(Row.Kind);
			B.Own = C.Key;
			const double Near = Len(X.P - C.P);
			const double Range = std::max(1.0, Len(X.P - B.At));
			const double Flight1 = Range / std::max(300.0, Row.Mv);
			// What he lays his gun on: the fighter as he leads it and wanders about it; or, his time come on the one
			// the battle gave him, the true place on it, held.
			Vec TrueWay, Way;
			double Flight0 = 0.0;
			// (the fighter flies on as it is flying while his rounds are on their way to it: its own flight, played
			// ahead, tells where they come to it)
			Course Run;
			bool bTrue = false;
			if (S && AirT >= S->OnAt)
			{
				Run = CourseOf(X, Dt, B.Fl.Life + 2.0 * Dt);
				bTrue = Safe(Run) && SolveOn(B, Run, WalkOf(*S), TrueWay, Flight0) && (AirT < X.HeldTo || Sure(X, Flight0));
			}
			const bool bOn = bTrue ? LayGun(Key, TrueWay, Row, 0.0, Way) : LayGun(Key, GunWant(C, B.At, X, Near, Row, Key), Row, 0.2, Way);
			if (!bOn || !Man.bFire || Man.Far > Cut) continue;
			// (the damage timed: a round fired now would come after it had begun; he holds)
			const double Stop = !S ? -1.0 : S->ReleaseAt >= 0.0 ? S->ReleaseAt : S->Plan;
			if (Stop >= 0.0 && AirT + (bTrue ? Flight0 : Flight1) + Dt >= Stop) continue;
			Check.StreamSteps++;
			Lay& G = Lays[Key];
			double End = 0.0;
			bool bStrikes = false;
			if (bTrue && Between(Way, TrueWay) < 0.35 * Deg && OwnClear(C, Man.M.Station, ToOwn(C, TrueWay), Dt))
			{
				// (where they come into its airframe, and no other aircraft in their way there)
				const double In = EntryOf(B, TrueWay, Run, X.Form, Flight0);
				const double Lands = In >= 0.0 ? In : Flight0;
				const Met M = Meets(B, TrueWay, 0.4, 0.0, X.Key);
				if ((!M.Key || M.Tof > Lands) && !Shadowed(RoundAt(B.At, TrueWay * B.Mv + B.Carry, Lands, B.Fl.Drag, B.Fl.Drop), Lands, X.Key) && Reaches(B, TrueWay, Lands))
				{
					bStrikes = true;
					Way = TrueWay;
					End = Lands;
				}
			}
			if (!bStrikes)
			{
				// A miss, to be seen to miss: his stream as he has it, but never nearer the fighter than the eye can
				// tell from a hit (past its nose, its tail or a wing tip, on the side he already errs to), ended short
				// of any other aircraft beyond it, and never fired through one on the way.
				// (and the fighter may not be quite where its flight tells by the time the rounds are there)
				const double Margin = 1.2;
				Vec Spot;
				bool bClear = true;
				if (WideOf(B, Way, X, Margin + 0.005 * Range + DriftOf(X, Flight1), 1.0, Spot))
				{
					Vec Wide;
					double Unused = 0.0;
					if (Solve(B, X, Spot, Wide, Unused))
					{
						// (the fighter crossing his sights from one side to the other: his finger off as it goes through)
						const Vec Side = Norm(Spot);
						if (AirT - G.PushAt < 0.3 && Side.Dot(G.Push) < 0.0) { bClear = false; G.HoldUntil = AirT + 0.25; }
						G.Push = Side;
						G.PushAt = AirT;
						Way = Wide;
						Check.WideSteps++;
					}
					else bClear = false;
				}
				if (bClear)
				{
					const Met M = Meets(B, Way, 0.6 * Margin, 0.004, 0, 1.0);
					if (M.Key && M.Key != X.Key && M.Tof >= Flight1 + 0.12) { End = M.Short; Check.ShortSteps++; }
					else if (M.Key) bClear = false;
				}
				if (bClear && !OwnClear(C, Man.M.Station, ToOwn(C, Way), Dt)) bClear = false;
				if (!bClear)
				{
					Check.HeldSteps++;
					if (!S) G.HoldUntil = std::max(G.HoldUntil, AirT + 0.25);
					continue;
				}
			}
			const bool bLow = !bStrikes && Low(B, Way);
			bool bDown = false;
			const int Of = std::max(1, static_cast<int>(Row.At.size()));
			double Latest = 0.0, StrikeTof = 0.0;
			Vec StrikeAt;
			for (int K = 0; K < Of; ++K)
			{
				// A twin mount is one opening: its two guns side by side in it, laid together.
				const Vec Here = Local(C, Man.M.Station + Vec(0.0, Of > 1 ? (K == 0 ? -0.15 : 0.15) : 0.0, 0.0)) + Lift;
				Want W;
				W.F.Kind = FxKind::Gun;
				W.F.Key = Key + K;
				W.F.At = Here;
				W.F.Aim = Here + Way * Range;
				W.F.Vel = B.Carry;
				W.F.What = Row.Kind;
				W.F.Text = Row.Mount;
				W.F.Rate = Row.Rate;
				W.F.Mv = Row.Mv;
				W.Far = Man.Far;
				if (bLow && Grounded(W, B.Fl, End)) bDown = true;
				else if (bStrikes)
				{
					// (each gun's rounds end where they themselves come into the airframe)
					Barrel Bg = B;
					Bg.At = Here;
					const double In = EntryOf(Bg, Way, Run, X.Form, End);
					W.F.Tof = In >= 0.0 ? In : End;
					const Vec Ends = RoundAt(Here, Way * Row.Mv + B.Carry, W.F.Tof, B.Fl.Drag, B.Fl.Drop);
					W.F.Reach = Len(Ends - Here);
					Latest = std::max(Latest, W.F.Tof);
					// (the strike seen is one gun's rounds ending, each gun's in turn)
					if (K == S->Sent % Of)
					{
						StrikeTof = W.F.Tof;
						StrikeAt = Ends;
					}
				}
				else if (End > 0.0)
				{
					const Vec Stops = RoundAt(Here, Way * Row.Mv + B.Carry, End, B.Fl.Drag, B.Fl.Drop);
					// (ended short of an aircraft only just over the ground or the sea: he holds, as a fighter's gun does)
					if (bLow && Stops.Z < GroundAt(Stops.X, Stops.Y) + 4.0) continue;
					W.F.Tof = End;
					W.F.Reach = Len(Stops - Here);
				}
				Asked.push_back(W);
			}
			if (bDown) Check.GroundSteps++;
			if (bStrikes)
			{
				// (the fighter flies on as it is flying until the last of them has landed)
				X.HeldTo = std::max(X.HeldTo, AirT + Latest + Dt + 1e-6);
				S->LastLand = std::max(S->LastLand, AirT + Latest);
				const Vec In = Norm(Way * (Row.Mv * std::exp(-B.Fl.Drag * StrikeTof)) + B.Carry - Vec(0.0, 0.0, B.Fl.Drop * StrikeTof) - (Run.At[1].P - Run.At[0].P) * (1.0 / Dt)) * -1.0;
				SendStrike(*S, StrikeTof, Row.Kind, In, StrikeAt);
			}
			if (AirT - G.Fired > 0.3)
			{
				Check.GunnerBursts++;
				Check.GunnerFires[Man.M.Window]++;
			}
			G.Fired = AirT;
		}

		// The nearest guns first, as many as the kit can draw.
		std::sort(Asked.begin(), Asked.end(), [](const Want& A, const Want& B) { return A.Far < B.Far; });
		if (static_cast<int>(Asked.size()) > MostGuns) Asked.resize(static_cast<size_t>(std::max(0, MostGuns)));
		std::set<int> Kept, Landing_;
		while (!RoundsOut.empty() && AirT - RoundsOut.front().T > 3.0) RoundsOut.pop_front();
		for (const Want& W : Asked)
		{
			if (Kept.count(W.F.Key)) { Check.SharedWindows++; continue; }
			Kept.insert(W.F.Key);
			Effects.push_back(W.F);
			RoundsOut.push_back(RoundOut{ AirT, W.F.At, Norm(W.F.Aim - W.F.At) * W.F.Mv + W.F.Vel, W.F.Tof > 0.0 ? W.F.Tof : 2.5 });
			if (!W.bGround) continue;
			// Every round of it strikes, as the walking line of a strafing run does: a spurt of dust, or a splash, where
			// it comes down and when it gets there (LandStray). The first as its stream first comes down, so none ends
			// on the ground or the sea with nothing to show for it.
			Landing_.insert(W.F.Key);
			Trod& Walked = GroundDue[W.F.Key];
			Walked.Owing = std::min(Walked.Owing + (W.F.Rate > 0.0 ? W.F.Rate : 800.0) / 60.0 * Dt, 8.0);
			// (and a stream that walks fast over the ground strikes it all along its walk: a nose swinging round carries
			// the far end of a long stream tens of metres between one round's strike and the next)
			if (Walked.Owing < 1.0 && Walked.bSent && Len(W.End - Walked.Last) > 25.0) Walked.Owing = 1.0;
			while (Walked.Owing >= 1.0)
			{
				Walked.Owing -= 1.0;
				const double Scatter = 0.004 * W.F.Reach;
				Spurt Sp;
				Sp.At = AirT + W.F.Tof;
				Sp.Where = W.End + Vec(Shots.U(-1.0, 1.0), Shots.U(-1.0, 1.0), 0.0) * Scatter;
				Sp.Where.Z = GroundAt(Sp.Where.X, Sp.Where.Y);
				Sp.In = W.In;
				Sp.Kind = W.F.What;
				Spurts.push_back(Sp);
				Walked.Last = W.End;
				Walked.bSent = true;
			}
		}
		for (auto It = GroundDue.begin(); It != GroundDue.end();)
		{
			if (Landing_.count(It->first)) ++It;
			else It = GroundDue.erase(It);
		}
		for (int Key : Firing) if (!Kept.count(Key)) Emit(FxKind::GunStop, Vec(), std::string(), Key);
		Firing = Kept;
		FiredT = AirT;
		if (!Kept.empty()) Check.GunFrames++;
		Check.MostGunsAtOnce = std::max(Check.MostGunsAtOnce, static_cast<int>(Kept.size()));
		// A burst over, or a gunner who lost his fighter, before a round of it struck, and none of it still on its
		// way: the hit waits for another.
		for (Shot& Each : Booked)
		{
			if (Each.bDone || Each.First >= 0.0 || Touched.count(Each.Id)) continue;
			bool bOnWay = false;
			for (const Landing& St : InFlight) bOnWay = bOnWay || St.Shot == Each.Id;
			if (!bOnWay) Unbook(Each, true);
		}
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

	void Show::FlakOn(const Craft& C, const Event& E)
	{
		// A shell bursts close by it, off the side the battle has it hit on, and its splinters go into it there: the
		// engine that then smokes, a wing, the tail (SpotOf, as for a burst of guns). Kept in the aircraft's own frame
		// for LandStray to draw once it has flown the frame.
		Vec Spot, From;
		FlakHit H;
		SpotOf(C, E, Spot, From, H.Part);
		H.Victim = C.Key;
		const auto G = Now.find(C.Group);
		H.Size = G != Now.end() && !G->second.Flak.empty() ? G->second.Flak.back() : std::string("heavy");
		const double Side = Spot.Y < -0.5 ? -1.0 : Spot.Y > 0.5 ? 1.0 : Shots.U() < 0.5 ? -1.0 : 1.0;
		const Vec Out_ = Norm(Vec(Shots.U(-0.7, 0.7), Side * Shots.U(0.15, 1.0), Shots.U(-0.8, 0.8)), Up);
		// (clear of its wing tips, near enough for the eye to take the burst and the strikes as one)
		H.Burst = Out_ * (C.Form.Span * Shots.U(0.55, 0.9) + 4.0);
		const int Count = 2 + static_cast<int>(Shots.U() * 2.99);
		const double Bound = 0.5 * std::hypot(C.Form.Long, C.Form.Span) + 2.0;
		for (int K = 0; K < Count; ++K)
		{
			// About that place, each drawn back on to the airframe, then out to the skin a splinter flying in from the
			// burst comes to first: the strikes show on the side that faces the burst.
			Vec P = Spot + Vec(Shots.U(-0.12, 0.12) * C.Form.Long, Shots.U(-0.08, 0.08) * C.Form.Span, Shots.U(-0.3, 0.3));
			for (int Pull = 0; Pull < 12 && !InAirframe(C.Form, P, 0.0); ++Pull) P += (Spot - P) * 0.25;
			if (!InAirframe(C.Form, P, 0.0)) P = Spot;
			const double Far = Len(P - H.Burst);
			const Vec In = Norm(P - H.Burst);
			for (double D = std::max(0.0, Far - 2.0 * Bound); D < Far; D += 0.1)
			{
				const Vec Q = H.Burst + In * D;
				if (!InAirframe(C.Form, Q, 0.0)) continue;
				P = Q;
				break;
			}
			H.Spots.push_back(P);
		}
		FlakHits.push_back(H);
	}

	void Show::LandStray()
	{
		// The rounds that come down on the ground or the sea this frame: each its own spurt of earth or its splash,
		// where it lands and when (a strafing run's walking line; a low miss into the water).
		size_t Left = 0;
		for (size_t K = 0; K < Spurts.size(); ++K)
		{
			if (Spurts[K].At > AirT)
			{
				if (Left != K) Spurts[Left] = Spurts[K];
				++Left;
				continue;
			}
			const Spurt& Sp = Spurts[K];
			// (not where an aircraft already going down is passing: the scene's rule)
			if (Masked(Sp.Where)) { Check.StrikesDropped++; continue; }
			Fx& H = Emit(FxKind::Hit, Sp.Where, Sp.Kind);
			H.Text = Sp.Where.Z <= 0.5 ? "water" : "ground";
			H.Normal = Up;
			H.Aim = Sp.In;
			Check.GroundStrikes++;
		}
		Spurts.resize(Left);
		// Flak's hits: the shell's burst, and its splinters on the skin, the aircraft having flown the frame, so each
		// strike sits on the airframe in the world and in the aircraft's own frame alike. A splinter is told apart
		// from a round by its kind, "flak", and belongs to no burst of guns.
		for (const FlakHit& H : FlakHits)
		{
			const Craft* V = Find(H.Victim);
			if (!V || V->Doing == Role::Fall || V->Doing == Role::Gone || !V->bShown) continue;
			// (her load just gone, she is drawn a little higher: the burst and its strikes with her)
			const Vec Lift(0.0, 0.0, LiftAt(*V, AirT));
			Vec At = Local(*V, H.Burst) + Lift;
			// (never dead on another model of its box)
			for (const auto& Pair : Crafts)
			{
				const Craft& Q = Pair.second;
				if (Q.Key == V->Key || Q.Group != V->Group || !Live(Q)) continue;
				const Vec Off = At - Q.P;
				if (Len(Off) < Q.Form.Span * 0.7) At = Q.P + Norm(Off, Up) * (Q.Form.Span * 0.8);
			}
			Emit(FxKind::Flak, At, H.Size);
			Check.FlakBursts++;
			const Vec Carried = Velocity(*V);
			int Nth = 0;
			for (const Vec& Spot : H.Spots)
			{
				const Vec On = Local(*V, Spot) + Lift;
				if (Masked(On)) { Check.StrikesDropped++; continue; }
				Fx& S = Emit(FxKind::Hit, On, "flak", V->Key);
				S.Text = "flak";
				S.Aim = Spot;
				S.Vel = Carried;
				S.Normal = Norm(At - On, Up);
				S.Nth = Nth++;
				S.Part = H.Part;
				Check.FlakStrikes++;
			}
		}
		FlakHits.clear();
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
				// (a raid on troops: steered for where it is laid; AirShowTroops.cpp)
				if (T.bWant) SteerBomb(T);
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
					// (a raid on troops: down where its frame crossed the ground, its number on it, and the hit it carries
					// shown on its piece; AirShowTroops.cpp)
					const Vec Struck = T.bWant ? Crossing(T, Dt) : Down;
					Fx& Hit = Emit(FxKind::BombHit, Struck, Struck.Z > 0.5 ? "land" : "water");
					Hit.Size = T.Size > 2.0 ? 500.0 : 250.0;
					Hit.Text = T.Lead > 1 ? "stick" : T.Lead == 1 ? "one" : "";
					Hit.Id = T.Serial;
					Check.Bombs++;
					if (T.bWant) BombDown(T);
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
		// (smooth flying: how far ahead the formations' velocities are read, GroupVelocity. Out to PathSpan as the views
		// in hand allow, no faster than the clock runs; never past the last view.)
		PathLead = std::max(0.0, std::min({ PathLead + DT, PathSpan, Latest() - ShownT }));
		const double AirDt = DT * Scale;
		AirT += AirDt;
		Interpolate();
		while (!Queue.empty() && Queue.front().T <= ShownT + 1e-9)
		{
			const Event E = Queue.front();
			Queue.pop_front();
			Play(E);
		}
		// (the sticks due on their lead's, let go where the battle's own drops are: before the pilots fly this frame)
		DropOnLead();
		for (size_t K = 0; K < Later.size();)
		{
			// (its time up, a fighter the gunners are to have and one coming into their reach: until it is there)
			if (Later[K].bForShot && !bEnded && Later[K].At <= AirT && AwaitsGunners(Later[K])) Later[K].At = AirT + 0.25;
			// (its time up with a fighter going in on one of them: a few seconds more, once, for his guns to bear)
			if (Later[K].bForShot && !bEnded && Later[K].At <= AirT && !Later[K].bMore && Inbound(Later[K].E))
			{
				Later[K].bMore = true;
				Later[K].At = AirT + 7.0;
			}
			if (Later[K].At <= AirT || ((Later[K].bForShot || Later[K].Fell >= 0) && bEnded))
			{
				const Held Done = Later[K];
				Later.erase(Later.begin() + static_cast<long>(K));
				// (what guns did, no burst of theirs having come to carry it: shown as it can be)
				if (Done.bForShot) ReleaseUnstruck(Done);
				else if (Done.Fell >= 0)
				{
					// (its frame's grace over: it goes)
					Band& B = Bands[Done.E.Unit];
					B.Pending = std::max(0, B.Pending - 1);
					Release(Done.E, Done.Fell, nullptr, false);
				}
				else Release(Done.E);
			}
			else ++K;
		}
		// The bursts that have struck: their damage begins (before the pilots fly, so nobody fires on at one going down).
		BeginDamage();
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
				++PilotSteps;
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
			LandStray();
			FlakRound(AirDt);
			Bays(AirDt);
			SmallThings(AirDt);
		}
		else
		{
			for (int Key : Firing) Emit(FxKind::GunStop, Vec(), std::string(), Key);
			Firing.clear();
		}
		// (a raid on troops: the held hits, the views' words on the pieces, the strafers' and the column's guns;
		// AirShowTroops.cpp)
		if (Column.Has()) TroopsFrame(AirDt);
		ShipFire(AirDt);
		StrikeNow();
		for (auto It = Crafts.begin(); It != Crafts.end();)
		{
			if (It->second.Doing == Role::Gone)
			{
				auto M = Members.find(It->second.Group);
				if (M != Members.end())
				{
					const auto K = std::lower_bound(M->second.begin(), M->second.end(), It->first);
					if (K != M->second.end() && *K == It->first) M->second.erase(K);
					if (M->second.empty()) Members.erase(M);
				}
				It = Crafts.erase(It);
			}
			else ++It;
		}
		Out.clear();
		Out.reserve(Crafts.size());
		Check.NotInBox.clear();
		Check.SlotGap.clear();
		for (const auto& Pair : Crafts)
		{
			const Craft& C = Pair.second;
			if (!C.bFighter && (C.Doing == Role::Straggle || (C.Doing == Role::Slot && C.bJoining))) Check.NotInBox.insert(C.Key);
			if (!C.bFighter && C.Doing == Role::Slot && !C.bJoining)
			{
				const auto Gi = Now.find(C.Group);
				if (Gi != Now.end())
				{
					const Vec Own = SlotPlace(Gi->second, C.Slot);
					Check.SlotGap[C.Key] = std::hypot(Own.X - C.P.X, Own.Y - C.P.Y);
				}
			}
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
			P.bFiring = C.bFiring || (C.bLaneGuns && C.bLaneRun && C.Doing == Role::Dive);
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
			// (its vapour trail: from the day's trail height, full 600 m over it; none falling or with every engine dead)
			const bool bEngines = C.Form.Engines.empty() || C.DeadEngines.size() < C.Form.Engines.size();
			P.Vapour = TrailM > 0.0 && C.Doing != Role::Fall && C.Doing != Role::Gone && C.Doing != Role::Ground && bEngines ? Clamp((C.P.Z - TrailM) / 600.0, 0.0, 1.0) : 0.0;
			P.bFeathered = C.bFeathered;
			P.Soot = C.SootAt >= 0.0 ? std::max(0.0, std::min(1.0, (AirT - C.SootAt) / 30.0)) : 0.0;
			P.Bay = C.Bay * C.Bay * (3.0 - 2.0 * C.Bay); // (eased at both ends)
			P.bBombsGone = C.StickDone >= 0.0;
			P.At.Z += Lifted(C);
			P.Struck = C.Struck;
			P.NamedArt = C.NamedArt;
			Out.push_back(P);
		}
	}

	void Show::StrikeNow()
	{
		// What holds the strike on this frame (StrikeOn).
		StrikeBombs = StrikeBays = StrikeDives = StrikeTorpedo = StrikeHits = 0;
		for (const Thing& T : Small) StrikeBombs += T.Kind == ThingKind::Bomb ? 1 : 0;
		for (const auto& Pair : Crafts)
		{
			const Craft& C = Pair.second;
			if (C.bFighter || C.Doing == Role::Fall || C.Doing == Role::Gone || C.Doing == Role::Ground) continue;
			// (a level bomber's doors opening on her run, her stick due on her lead's or still leaving the bay)
			if ((C.Bay > 0.0 && !C.bDropped) || C.DropDue >= 0.0 || C.StickLeft > 0) ++StrikeBays;
			// (a dive bomber over on his wing, down, or pulling out)
			if (C.Doing == Role::Dive && (C.Step == Phase::Peel || C.Step == Phase::Plunge || C.Step == Phase::Pull)) ++StrikeDives;
			// (a torpedo plane turned in on her run, her torpedo not yet gone)
			if (C.Doing == Role::Dive && C.Step == Phase::Run && !C.bDropped) ++StrikeTorpedo;
		}
		// (a hit on a ship the battle has counted, held until the bombs or the torpedoes could have reached her)
		for (const Held& H : Later) StrikeHits += !H.bForShot && (H.E.What == "air:ship-hit" || H.E.What == "air:ship-sunk") ? 1 : 0;
		// (and a hit on a piece of a column while its bomb falls, its rounds fly or its strafing pass goes in; not while
		// it only waits for one: AirShowTroops.cpp)
		if (Column.Has()) StrikeHits += MarkStrikes();
		bStrikeOn = StrikeBombs + StrikeBays + StrikeDives + StrikeTorpedo + StrikeHits > 0;
	}

	std::string Show::Describe() const
	{
		// One line an aircraft: what it is doing (for a debug overlay, and the cloud test's trace).
		static const char* Roles[] = { "ground", "slot", "fight", "straggle", "cover", "fall", "home", "dive", "gone" };
		static const char* Steps[] = { "-", "setup", "run", "break", "climb", "turn", "peel", "plunge", "pull", "egress", "hold", "zoom", "evade", "station", "chase", "away", "wing", "scissor" };
		static const char* Ways[] = { "-", "explode", "burn", "spin", "flamer", "wing", "tail", "pilot", "glide", "breakup", "struck" };
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
			              Roles[static_cast<int>(C.Doing)], C.Doing == Role::Fall ? Ways[static_cast<int>(C.Falling)] : Steps[static_cast<int>(C.Step)], C.StepT, C.TargetGroup, C.Target,
			              T ? Len(T->P - C.P) : -1.0, Off, C.Spd, C.P.Z, C.bFiring ? 1 : 0, C.Lead, L ? Len(L->P - C.P) : 0.0, C.Flight, C.Seat,
			              C.bGuard ? " guard" : "", C.bHurt ? " hurt" : "", C.bDry ? " dry" : "", C.Task);
			Text += Line;
		}
		std::snprintf(Line, sizeof Line, "# strike %s: %d bombs and torpedoes in the air, %d bays on the run, %d dives, %d torpedo runs, %d ship hits to show\n", bStrikeOn ? "on" : "off",
		              StrikeBombs, StrikeBays, StrikeDives, StrikeTorpedo, StrikeHits);
		Text += Line;
		return Text;
	}

	// The ships' anti-aircraft guns (ShipGuns: Brian, 21 November 18:06Z, "for the ships being attacked by planes, they
	// can fire machine gun fire and flak guns at them just like real life"). The gunnery is shared with the sea battle
	// scene; here the show hands it its ships and its aircraft and plays what comes back as its own effects.

	std::vector<ShipGuns::Hull> Show::Hulls() const
	{
		std::vector<ShipGuns::Hull> Hs;
		Hs.reserve(ShipsNow.size());
		for (const Ship& S : ShipsNow)
		{
			ShipGuns::Hull H;
			H.Id = S.Id;
			H.At = S.At;
			H.Face = S.Face;
			H.Nation = S.Nation;
			H.Class = S.Class;
			H.Name = S.Name;
			H.Side = S.Side;
			H.Long = S.Long;
			H.Beam = S.Beam;
			H.Manned = S.Manned;
			// (her guns fire on until her sinking is shown, as at Battleship Row; one sunk before the battle, never)
			H.bSunk = ShipsDown.count(S.Id) > 0 || (S.bSunk && !ShipsAfloat.count(S.Id));
			Hs.push_back(H);
		}
		return Hs;
	}

	ShipGuns::Plane Show::PlaneOf(const Craft& C) const
	{
		ShipGuns::Plane P;
		P.Key = C.Key;
		P.P = C.P;
		P.V = C.Doing == Role::Fall ? C.FallV : Velocity(C);
		P.Hdg = C.Hdg;
		P.Gam = C.Gam;
		P.Bank = C.Bank;
		P.Span = C.Form.Span;
		P.Long = C.Form.Long;
		P.Side = C.Side;
		P.bFighter = C.bFighter;
		P.bFighting = C.bFighter && C.Doing == Role::Fight;
		P.bFalling = C.Doing == Role::Fall;
		return P;
	}

	void Show::ShotsOut(const std::vector<ShipGuns::Shot>& Fired)
	{
		// (Fired, not Shots: that name is the gunnery's own dice)
		for (const ShipGuns::Shot& S : Fired)
		{
			switch (S.Kind)
			{
			case ShipGuns::ShotKind::Stream:
			{
				Fx& F = Emit(FxKind::Gun, S.At, S.Kit, S.Key);
				F.Aim = S.Aim;
				F.Vel = S.Vel;
				F.Text = "ship";
				F.Rate = S.Rate;
				F.Mv = S.Mv;
				break;
			}
			case ShipGuns::ShotKind::StreamStop:
				Emit(FxKind::GunStop, Vec(), std::string(), S.Key);
				break;
			case ShipGuns::ShotKind::Round:
			{
				Fx& F = Emit(FxKind::ShipGun, S.At, S.What, S.Ship);
				F.Aim = S.Aim;
				F.Size = S.Size;
				break;
			}
			case ShipGuns::ShotKind::Burst:
				Emit(FxKind::Flak, S.At, "heavy");
				break;
			case ShipGuns::ShotKind::Splash:
			{
				// (a battleship's main battery throws a bomb's column, a 5-inch a shell's; on land, its dust)
				const double Floor = GroundAt(S.At.X, S.At.Y);
				const bool bLand = Floor > 0.5;
				Emit(bLand ? FxKind::Crash : FxKind::Splash, Vec(S.At.X, S.At.Y, Floor), "shell").Size = bLand ? 2.0 : S.Size >= 200.0 ? 10.0 : 5.0;
				break;
			}
			case ShipGuns::ShotKind::Hit:
			{
				Fx& F = Emit(FxKind::Hit, S.At, S.Kit, S.Key);
				F.Normal = S.Normal;
				F.Vel = S.Vel;
				F.Text = S.What == "kill" ? "kill" : "ship";
				break;
			}
			}
		}
	}

	void Show::ShipFire(double Dt)
	{
		if (ShipsNow.empty()) return;
		for (const Ship& S : ShipsNow) if (!S.bSunk) ShipsAfloat.insert(S.Id);
		Gunners.Year = Year;
		Gunners.Month = Month;
		Gunners.MostStreams = MostShipGuns;
		Gunners.RoundsMost = ShipRoundsMost;
		Gunners.Eye = Eye;
		std::vector<ShipGuns::Plane> Planes;
		Planes.reserve(Crafts.size());
		for (const auto& Pair : Crafts)
		{
			const Craft& C = Pair.second;
			if (C.Doing == Role::Gone || C.Doing == Role::Ground || !C.bShown) continue;
			Planes.push_back(PlaneOf(C));
		}
		std::vector<ShipGuns::Shot> Fired;
		Gunners.Fire(AirT, Dt, Hulls(), Planes, Fired);
		ShotsOut(Fired);
	}

	void Show::FlakKill(const Craft& C)
	{
		// (the burst laid as before, its dice thrown as before, so the rest of the show plays as it did)
		const Vec Burst = C.P + Vec(Roll.U(-4.0, 4.0), Roll.U(-4.0, 4.0), Roll.U(-2.0, 3.0));
		if (!ShipsNow.empty())
		{
			std::vector<ShipGuns::Shot> Fired;
			if (Gunners.Kill(AirT, PlaneOf(C), Hulls(), Fired))
			{
				ShotsOut(Fired);
				return;
			}
		}
		Emit(FxKind::Flak, Burst, "heavy");
	}
}
