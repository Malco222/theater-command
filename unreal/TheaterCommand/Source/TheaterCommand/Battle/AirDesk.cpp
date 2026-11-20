#include "Battle/AirDesk.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace AirDesk
{
	namespace
	{
		std::string Percent(double Share)
		{
			char Buf[32];
			std::snprintf(Buf, sizeof Buf, "%d%%", static_cast<int>(std::lround(std::clamp(Share, 0.0, 1.0) * 100.0)));
			return Buf;
		}

		std::string Units(const std::vector<int>& Ids)
		{
			std::string Out = "[";
			for (size_t K = 0; K < Ids.size(); ++K)
			{
				if (K) Out += ",";
				Out += std::to_string(Ids[K]);
			}
			return Out + "]";
		}

		std::string Metres(double V)
		{
			char Buf[48];
			std::snprintf(Buf, sizeof Buf, "%.1f", V);
			return Buf;
		}

		std::string Upper(std::string S)
		{
			for (char& C : S) if (C >= 'a' && C <= 'z') C = static_cast<char>(C - 'a' + 'A');
			return S;
		}
	}

	double Clock::Step(double Real, const AirFeed::Top& Head, double Scale, std::string& Tag)
	{
		Tag.clear();
		if (Head.bOver) return 0.0;
		if (bQuestion)
		{
			Tag = "HELD WHILE THE QUESTION STANDS";
			return 0.0;
		}
		if (bFlyover)
		{
			Tag = "SURVEYING THE FIELD \xC2\xB7 CLICK OR PRESS ANY KEY TO TAKE COMMAND";
			return Real / std::max(Scale, 0.1);
		}
		if (Speed <= 0.0)
		{
			Tag = "HELD";
			return 0.0;
		}
		// Fast until the formations meet, and once the fight has wound down (the play version's mark 1).
		if (Head.Contact < 0.0)
		{
			Tag = "\xC3\x97" "10 UNTIL THE FORMATIONS MEET";
			return Real * 10.0;
		}
		if (SinceMet < 0.0) SinceMet = 0.0;
		SinceMet += Real;
		if (Head.bWinding)
		{
			Tag = "\xC3\x97" "10 UNTIL THE LAST ARE HOME";
			return Real * 10.0;
		}
		const double Pace = Real * Speed / std::max(Scale, 0.1);
		if (SinceMet < 3.0) Tag = "THE FORMATIONS MEET \xC2\xB7 RUNNING \xC3\x97" "1";
		else if (Speed == 0.5) Tag = "RUNNING \xC3\x97\xC2\xBD";
		else if (Speed != 1.0) Tag = "RUNNING \xC3\x97" + std::to_string(static_cast<int>(Speed));
		return Pace;
	}

	void Desk::Begin(const AirFeed::Sky& Board, const std::string& InMe)
	{
		Sky = Board;
		Me = InMe == "def" ? "def" : "att";
		MyIndex = Me == "att" ? 0 : 1;
		bStarted = false;
		Called.clear();
		LabelsNow.clear();
		Said.clear();
		EngagedOnce.clear();
		Ahead.clear();
	}

	void Desk::Take(const AirFeed::Frame& View)
	{
		if (!bStarted)
		{
			// (the first view is read at once: the screen opens on it)
			Read(View);
			return;
		}
		Ahead.push_back(View);
	}

	void Desk::Reach(double ShownT)
	{
		size_t K = 0;
		while (K < Ahead.size() && Ahead[K].Head.T <= ShownT + 1e-6) Read(Ahead[K++]);
		if (K) Ahead.erase(Ahead.begin(), Ahead.begin() + static_cast<std::ptrdiff_t>(K));
	}

	void Desk::Read(const AirFeed::Frame& View)
	{
		HeadNow = View.Head;
		if (!bStarted)
		{
			bStarted = true;
			BalanceAtStart = View.Head.BalanceAtt;
		}
		for (const auto& Pair : View.Head.Called) Called[Pair.first] = Pair.second;
		for (const AirFeed::Label& L : View.Labels) LabelsNow[L.Id] = L;
	}

	void Desk::Say(const std::string& What, const std::string& Text, int Side, int Unit)
	{
		if (Text.empty() || What == "air:rejoin" || What == "air:ship-hit") return;
		if (What == "air:engage")
		{
			// (shown the first time each escort group breaks away; the scene shows the rest)
			if (EngagedOnce[Unit]) return;
			EngagedOnce[Unit] = true;
		}
		if (What == "air:near" && Side == MyIndex) return; // (his own side's is the near line that stays, not a dispatch)
		Dispatch D;
		D.Text = Text;
		D.Side = Side;
		D.bMine = Side == MyIndex;
		Said.push_back(D);
		while (Said.size() > 6) Said.erase(Said.begin());
	}

	std::string Desk::Elapsed(double Seconds) const
	{
		const int S = std::max(0, static_cast<int>(Seconds));
		char Buf[32];
		std::snprintf(Buf, sizeof Buf, "%d:%02d", S / 60, S % 60);
		return Buf;
	}

	void Desk::Write(double Real, const std::map<int, AirShow::Group>& Shown, const std::string& ClockTag, Words& Out)
	{
		const AirFeed::Top& H = HeadNow;
		Out = Words();
		Out.Title = Sky.Title;
		Out.MySide = MyIndex == 0 ? Sky.AttAdj : Sky.DefAdj;
		Out.TheirSide = MyIndex == 0 ? Sky.DefAdj : Sky.AttAdj;
		// The top bar (b384, land's form): the card's chance and the staff's now, his side's.
		const double Card = MyIndex == 0 ? Sky.CardChance : 1.0 - Sky.CardChance;
		Out.Chance = (Sky.CardChance > 0.0 ? "Card about " + Percent(Card) + " \xC2\xB7 now about " : std::string("Now about ")) + Percent(H.Chance);
		Out.Balance = MyIndex == 0 ? H.BalanceAtt : 1.0 - H.BalanceAtt;
		Out.BalanceStart = MyIndex == 0 ? BalanceAtStart : 1.0 - BalanceAtStart;
		const AirFeed::Near& Mine = MyIndex == 0 ? H.NearAtt : H.NearDef;
		const AirFeed::Near& Theirs = MyIndex == 0 ? H.NearDef : H.NearAtt;
		Out.MyBar = Mine.At;
		Out.TheirBar = Theirs.At;
		Out.ClockTag = ClockTag;
		Out.Clock = Elapsed(H.T);
		Out.bOver = H.bOver;

		// The aim slip (b397): what the raid is for, how it stands, the aircraft lost.
		const int Lost = MyIndex == 0 ? H.LostAtt : H.LostDef, TheirLost = MyIndex == 0 ? H.LostDef : H.LostAtt;
		const std::string Place = Sky.PlaceName.empty() ? std::string("the target") : Sky.PlaceName;
		if (Sky.PlaceKind == "harbour")
		{
			Out.AimTarget = (Sky.FleetName.empty() ? std::string("The fleet") : "The " + Sky.FleetName) + " at " + Place;
		}
		else if (Sky.PlaceKind == "fleet")
		{
			Out.AimTarget = Sky.FleetName.empty() ? "The fleet off " + Place : "The " + Sky.FleetName;
		}
		else if (Sky.PlaceKind == "sky" || Sky.PlaceKind == "superiority")
		{
			Out.AimTarget = "The sky over " + Place;
		}
		else
		{
			Out.AimTarget = Place;
		}
		if (Sky.PlaceKind == "sky" || Sky.PlaceKind == "superiority")
		{
			const int Up = MyIndex == 0 ? H.UpAtt : H.UpDef, TheirUp = MyIndex == 0 ? H.UpDef : H.UpAtt;
			Out.AimState = "Fighters in the air: yours " + std::to_string(Up) + ", " + Out.TheirSide + " " + std::to_string(TheirUp);
		}
		else if (H.bCut)
		{
			Out.AimState = "The line is cut";
		}
		else if (H.bRuined)
		{
			Out.AimState = "A factory is wrecked";
		}
		else
		{
			Out.AimState = H.Ratio >= 1.0 ? "So far its bombs have done more than the raid has cost" : "So far the raid has cost more than its bombs have done";
		}
		Out.AimLost = "Aircraft lost: yours " + std::to_string(Lost) + ", " + Out.TheirSide + " " + std::to_string(TheirLost);
		if (Mine.At >= 0.75)
		{
			Out.NearLine = MyIndex == 0 ? "Your raid is near breaking off" : "Your fighters are near breaking off";
			if (Mine.Why == "ammo") Out.NearLine += ": short of ammunition";
		}

		// The tags: his own groups by their names, the enemy's with their country word (sim.called).
		for (const auto& Pair : Shown)
		{
			const AirShow::Group& G = Pair.second;
			Tag T;
			T.Id = G.Id;
			T.Side = G.Side;
			T.bMine = G.Side == MyIndex;
			T.bFighter = G.bFighter;
			auto C = Called.find(G.Id);
			T.Name = T.bMine || C == Called.end() || C->second.empty() ? G.Name : C->second;
			auto L = LabelsNow.find(G.Id);
			const int N = L != LabelsNow.end() ? L->second.N : G.N;
			T.Count = std::to_string(N) + " aircraft";
			if (G.State == "wavering") T.Doing = "wavering";
			else if (G.State == "routing" || G.Job == "home") T.Doing = "heading home";
			else if (G.Doing == AirShow::Act::Idle) T.Doing = "on the ground";
			T.bGone = G.Doing == AirShow::Act::Down || G.State == "withdrawn";
			Out.Tags.push_back(T);
		}

		for (Dispatch& D : Said) D.Age += Real;
		Out.Dispatches = Said;
	}

	Card Desk::CardOf(const AirFeed::After& After) const
	{
		Card C;
		const bool bSky = Sky.PlaceKind == "sky" || Sky.PlaceKind == "superiority";
		C.Kicker = std::string(bSky ? "BATTLE RESULT" : "RAID RESULT") + (Sky.Date.empty() ? std::string() : " \xC2\xB7 " + Sky.Date);
		C.Title = Sky.Title;
		C.Level = Upper(After.Level);
		C.Reckoning = After.Reckoning;
		C.Lines = After.Lines;
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (const AirFeed::AfterRow& R : After.Groups)
			{
				const bool bMine = R.Side == Me;
				if ((Pass == 0) != bMine) continue;
				Card::Row Row;
				Row.Name = R.Name;
				Row.Words = R.Words;
				Row.bMine = bMine;
				Row.bFighter = R.Kind != "bmr";
				C.Rows.push_back(Row);
			}
		}
		return C;
	}

	std::string Desk::Attack(const std::vector<int>& Ids, int Target) const
	{
		if (Ids.empty()) return std::string();
		return "{\"do\":\"attack\",\"units\":" + Units(Ids) + ",\"target\":" + std::to_string(Target) + "}";
	}

	std::string Desk::Move(const std::vector<int>& Ids, double X, double Y) const
	{
		if (Ids.empty()) return std::string();
		return "{\"do\":\"move\",\"units\":" + Units(Ids) + ",\"x\":" + Metres(X) + ",\"y\":" + Metres(Y) + "}";
	}

	std::string Desk::Hold(const std::vector<int>& Ids) const
	{
		return Ids.empty() ? std::string() : "{\"do\":\"hold\",\"units\":" + Units(Ids) + "}";
	}

	std::string Desk::BreakOff(const std::vector<int>& Ids) const
	{
		return Ids.empty() ? std::string() : "{\"do\":\"withdraw\",\"units\":" + Units(Ids) + "}";
	}

	std::string Desk::BreakOffAll() const
	{
		return "{\"do\":\"withdrawAll\",\"side\":\"" + Me + "\"}";
	}

	std::string Desk::Escort(const std::vector<int>& Ids, int Box, bool bClose) const
	{
		if (Ids.empty() || Box < 0) return std::string();
		return "{\"do\":\"air:escort\",\"units\":" + Units(Ids) + ",\"target\":" + std::to_string(Box) + ",\"close\":" + (bClose ? "true" : "false") + "}";
	}

	std::string Desk::Hunt(const std::vector<int>& Ids) const
	{
		return Ids.empty() ? std::string() : "{\"do\":\"air:hunt\",\"units\":" + Units(Ids) + "}";
	}

	std::string Desk::Band(const std::vector<int>& Ids, int InBand) const
	{
		if (Ids.empty() || InBand < 0 || InBand > 2) return std::string();
		return "{\"do\":\"air:band\",\"units\":" + Units(Ids) + ",\"band\":" + std::to_string(InBand) + "}";
	}

	std::string Desk::Jettison(const std::vector<int>& Ids) const
	{
		return Ids.empty() ? std::string() : "{\"do\":\"air:jettison\",\"units\":" + Units(Ids) + "}";
	}

	std::string Desk::HandOver() const
	{
		return "{\"do\":\"dice\"}";
	}

	std::string Desk::StaffQuestion() const
	{
		const bool bSky = Sky.PlaceKind == "sky" || Sky.PlaceKind == "superiority";
		std::string Q = bSky ? "Hand the fight to the staff?" : "Hand the raid to the staff?";
		if (!HeadNow.bFired) return Q + " No shot has been fired: they settle the whole raid at once. They give you about " + Percent(HeadNow.Chance) + " to win it.";
		return Q + " They settle the rest at once, from where it stands now. They give you about " + Percent(HeadNow.Chance) + " to win it.";
	}

	Subject Choose(const std::vector<AirShow::PlaneOut>& Planes, const std::vector<int>& Firing, const AirShow::Vec& Near, int Avoid)
	{
		(void)Firing;
		Subject Best;
		double BestScore = 1e18;
		for (const AirShow::PlaneOut& P : Planes)
		{
			if (!P.bShown || P.bOnGround || P.bFalling || P.Key == Avoid) continue;
			// A fighter firing on its quarry first, then one on a pass, then a bomber; nearest the eye's middle.
			const double Kind = P.bFiring && P.Target >= 0 ? 0.0 : P.Target >= 0 ? 3000.0 : P.bFighter ? 9000.0 : 6000.0;
			const double D = std::sqrt((P.At.X - Near.X) * (P.At.X - Near.X) + (P.At.Y - Near.Y) * (P.At.Y - Near.Y) + (P.At.Z - Near.Z) * (P.At.Z - Near.Z));
			const double Score = Kind + D;
			if (Score < BestScore)
			{
				BestScore = Score;
				Best.Plane = P.Key;
				Best.Other = P.Target;
			}
		}
		return Best;
	}

	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		double Ease(double U)
		{
			U = std::clamp(U, 0.0, 1.0);
			return U * U * (3.0 - 2.0 * U);
		}

		AirShow::Vec Level(const AirShow::Vec& V, const AirShow::Vec& Otherwise)
		{
			const AirShow::Vec Flat(V.X, V.Y, 0.0);
			return Flat.Length() > 1e-6 ? Flat.Unit() : Otherwise;
		}

		bool Flying(const AirShow::Group& G)
		{
			return G.Doing != AirShow::Act::Down && G.Doing != AirShow::Act::Idle && G.Alt > 150.0;
		}
	}

	bool Flyover::Begin(const std::map<int, AirShow::Group>& Groups, double InTargetX, double InTargetY, bool bHarbour)
	{
		TargetX = InTargetX;
		TargetY = InTargetY;
		bOverHarbour = bHarbour;
		Raid = Guard = -1;
		Last.clear();
		Leads.clear();
		Fallen.clear();
		Shifts.clear();
		// (the raid: the raider's biggest bomber formation in the air; its biggest of any kind if it has no bombers up)
		int MostRaid = -1, MostAny = -1, Any = -1;
		for (const auto& Pair : Groups)
		{
			const AirShow::Group& G = Pair.second;
			if (G.Side != 0 || !Flying(G)) continue;
			const int Size = std::max(G.N, G.Figures);
			if (!G.bFighter && Size > MostRaid)
			{
				MostRaid = Size;
				Raid = G.Id;
			}
			if (Size > MostAny)
			{
				MostAny = Size;
				Any = G.Id;
			}
		}
		if (Raid < 0) Raid = Any;
		if (Raid < 0) return false;
		const AirShow::Group& R = Groups.at(Raid);
		RaidAt = AirShow::Vec(R.X, R.Y, R.Alt);
		Toward = Level(RaidAt - AirShow::Vec(TargetX, TargetY, 0.0), AirShow::Vec(0.0, 1.0, 0.0));
		// (the guard: the defender's biggest fighter formation up)
		int MostGuard = -1;
		for (const auto& Pair : Groups)
		{
			const AirShow::Group& G = Pair.second;
			if (G.Side != 1 || !G.bFighter || !Flying(G)) continue;
			const int Size = std::max(G.N, G.Figures);
			if (Size > MostGuard)
			{
				MostGuard = Size;
				Guard = G.Id;
			}
		}
		return true;
	}

	const AirShow::Group* Flyover::Find(const std::map<int, AirShow::Group>& Groups, int Id) const
	{
		const auto It = Groups.find(Id);
		if (It != Groups.end() && It->second.Doing != AirShow::Act::Down)
		{
			Last[Id] = It->second;
			return &It->second;
		}
		const auto Kept = Last.find(Id);
		return Kept != Last.end() ? &Kept->second : nullptr;
	}

	Flyover::Shot Flyover::At(double T, const std::map<int, AirShow::Group>& Groups, const std::vector<AirShow::PlaneOut>& Planes) const
	{
		// (once the shots are over, the last pose of the last one, for the scene's glide)
		const bool bOver = T >= Length();
		T = std::clamp(T, 0.0, Length() - 1e-3);
		const AirShow::Vec Up(0.0, 0.0, 1.0);
		const AirShow::Vec Target(TargetX, TargetY, 0.0);
		Shot Out;
		// What it comes for: from beyond the target, the far side from the raid, pushing in from 2.6 km to 1.9 km and
		// down from 650 m to 520 m, the look lifting from the target toward the raid. Over a harbour the look lifts no
		// further than holds the target HarbourLow degrees under the picture's middle, in its lower third (the art
		// director on build 1750's Oahu frame: the hills had pushed Ford Island and the ships off the picture's foot).
		auto OverTarget = [&](double Into)
		{
			const double S = Ease(Into / TargetLong);
			Out.Eye = Target - Toward * (2600.0 - 700.0 * S) + Up * (650.0 - 130.0 * S);
			const AirShow::Vec From = Target + Up * 40.0;
			const AirShow::Vec To = Target + Toward * 3500.0 + Up * std::clamp(RaidAt.Z * 0.35, 300.0, 1500.0);
			Out.Look = From + (To - From) * S;
			if (bOverHarbour)
			{
				const AirShow::Vec Way = Out.Look - Out.Eye, ToTarget = Target - Out.Eye;
				const double Flat = std::hypot(Way.X, Way.Y);
				const double Most = std::atan2(ToTarget.Z, std::hypot(ToTarget.X, ToTarget.Y)) + HarbourLow * Pi / 180.0;
				if (Flat > 1e-6 && std::atan2(Way.Z, Flat) > Most)
				{
					const double Far = Way.Length();
					Out.Look = Out.Eye + AirShow::Vec(Way.X / Flat * std::cos(Most), Way.Y / Flat * std::cos(Most), std::sin(Most)) * Far;
				}
			}
			Out.Index = 1;
		};
		// The raid's and the guard's shots: from above and just behind the formation's lead three, looking down
		// LookDown degrees at them so the sea or the cloud deck is behind them, near enough that each of the three is
		// about LeadShare of the picture's width; pushing in from 1.2 times that to 0.9 as the shot runs, and swinging in
		// behind them from out on one side. The lead three are the group's plane furthest ahead as the shot begins and
		// the two nearest it (they fly some 30 to 150 m behind the slots their group's place gives); with none of them
		// up, the group's first three slots.
		auto Behind = [&](const AirShow::Group& G, double Side, double S, int Index)
		{
			const AirShow::Vec Ahead(std::sin(G.Face), std::cos(G.Face), 0.0), Right(std::cos(G.Face), -std::sin(G.Face), 0.0);
			std::vector<int>& Three = Leads[G.Id];
			if (Three.empty())
			{
				// (the plane furthest ahead, and the two nearest it: its own element, a vic of three, not three planes
				// strung across the formation's front)
				const AirShow::PlaneOut* First = nullptr;
				for (const AirShow::PlaneOut& P : Planes)
				{
					if (P.Group == G.Id && P.bShown && !P.bFalling && (!First || Ahead.Dot(P.At) > Ahead.Dot(First->At))) First = &P;
				}
				if (First)
				{
					std::vector<std::pair<double, int>> Order;
					for (const AirShow::PlaneOut& P : Planes)
					{
						if (P.Group == G.Id && P.bShown && !P.bFalling) Order.push_back({ (P.At - First->At).Length(), P.Key });
					}
					std::sort(Order.begin(), Order.end());
					for (size_t K = 0; K < Order.size() && K < 3; ++K) Three.push_back(Order[K].second);
				}
			}
			AirShow::Vec Lead, Was;
			double Span = 0.0;
			int Found = 0, FoundWas = 0;
			// (a lead plane going down leaves the aim where its fellows fly: it is never followed down; the pre-check on round 8.
			// And the aim does not hop as it drops out: Was is the middle with the planes that began to fall this frame
			// still counted, and the aim keeps that much off the flying leads' middle from then on; the pre-check on round 10)
			std::vector<int>& Fell = Fallen[G.Id];
			AirShow::Vec& Shift = Shifts[G.Id];
			for (const AirShow::PlaneOut& P : Planes)
			{
				if (!P.bShown || std::find(Three.begin(), Three.end(), P.Key) == Three.end())
				{
					continue;
				}
				const bool bNewly = P.bFalling && std::find(Fell.begin(), Fell.end(), P.Key) == Fell.end();
				if (!P.bFalling)
				{
					Lead = Lead + P.At;
					Span += P.Span;
					++Found;
				}
				if (!P.bFalling || bNewly)
				{
					Was = Was + P.At;
					++FoundWas;
				}
				if (bNewly)
				{
					Fell.push_back(P.Key);
				}
			}
			if (Found > 0)
			{
				Lead = Lead * (1.0 / Found);
				Span /= Found;
				if (FoundWas > Found)
				{
					Shift = Shift + Was * (1.0 / FoundWas) - Lead;
				}
				Lead = Lead + Shift;
			}
			else
			{
				Lead = (AirShow::SlotOf(G, 0) + AirShow::SlotOf(G, 1) + AirShow::SlotOf(G, 2)) * (1.0 / 3.0);
				Span = AirShow::ShapeOf(G.Model, G.bFighter).Span;
			}
			const double Near = Span / (2.0 * std::tan(Lens * 0.5 * Pi / 180.0) * LeadShare);
			const double Pull = Near * (1.2 - 0.3 * S), Down = LookDown * Pi / 180.0;
			Out.Eye = Lead - Ahead * (Pull * std::cos(Down)) + Up * (Pull * std::sin(Down)) + Right * (Side * Near * 0.3 * (1.0 - S));
			Out.Look = Lead;
			Out.Index = Index;
		};
		if (T < RaidLong)
		{
			// The raid (gone before its shot is over, its last pose stands); out on the side away from the target as the
			// shot begins.
			const AirShow::Group* R = Find(Groups, Raid);
			if (R)
			{
				const AirShow::Vec Right(std::cos(R->Face), -std::sin(R->Face), 0.0);
				const double Side = Right.Dot(Target - AirShow::Vec(R->X, R->Y, R->Alt)) > 0.0 ? -1.0 : 1.0;
				Behind(*R, Side, Ease(T / RaidLong), 0);
			}
			else
			{
				Out.Eye = RaidAt - Toward * 600.0 + Up * 150.0;
				Out.Look = RaidAt;
				Out.Index = 0;
			}
		}
		else if (T < RaidLong + TargetLong)
		{
			OverTarget(T - RaidLong);
		}
		else
		{
			// The guard (gone before its shot, the target's last pose stands).
			const AirShow::Group* G = Find(Groups, Guard);
			if (G)
			{
				Behind(*G, 1.0, Ease((T - RaidLong - TargetLong) / GuardLong), 2);
			}
			else
			{
				OverTarget(TargetLong);
			}
		}
		if (bOver)
		{
			Out.Index = -1;
		}
		return Out;
	}
}
