#include "Battle/AirDesk.h"

#include <algorithm>
#include <cctype>
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

		/** Every From in Text made To (the after card's lines of a raid on troops speak of the column, not "the target"). */
		std::string Swapped(std::string Text, const std::string& From, const std::string& To)
		{
			if (From.empty()) return Text;
			for (size_t At = Text.find(From); At != std::string::npos; At = Text.find(From, At + To.size()))
			{
				Text.replace(At, From.size(), To);
			}
			return Text;
		}

		/** A share as a percentage with one place, "1.2%" (the organisation a raid on troops cuts: 4% at most). */
		std::string Tenths(double Share)
		{
			char Buf[32];
			std::snprintf(Buf, sizeof Buf, "%.1f%%", std::clamp(Share, 0.0, 1.0) * 100.0);
			return Buf;
		}

		/** How a column stands (place.marks.posture), in words after its name. */
		std::string PostureWords(const std::string& Posture)
		{
			if (Posture == "march") return " on the march";
			if (Posture == "camp") return " in camp";
			if (Posture == "dug") return " dug in";
			if (Posture == "works") return " in its works";
			if (Posture == "town") return " in the town";
			return std::string();
		}
	}

	double Clock::Step(double Real, const AirFeed::Top& Head, double Scale, std::string& Tag)
	{
		Tag.clear();
		bFast = false;
		SinceStrike = bStrike ? 0.0 : SinceStrike + std::max(0.0, Real);
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
		const double Pace = Real * Speed / std::max(Scale, 0.1);
		// Fast until the formations meet, and once the fight has wound down (the play version's mark 1); but never while
		// the strike is on, nor StrikeHoldS after it: the bombs and the dives are seen at the player's pace, then the
		// clock eases into the fast run over StrikeEaseS.
		const auto RunFast = [&](const char* Words) {
			const double Share = std::min(1.0, std::max(0.0, (SinceStrike - StrikeHoldS) / std::max(StrikeEaseS, 0.01)));
			bFast = Share > 0.5;
			Tag = Share <= 0.0 ? std::string("THE STRIKE \xC2\xB7 RUNNING \xC3\x97") + (Speed == 0.5 ? "\xC2\xBD" : std::to_string(static_cast<int>(Speed))) : std::string(Words);
			return Pace + (Real * 10.0 - Pace) * Share;
		};
		if (Head.Contact < 0.0) return RunFast("\xC3\x97" "10 UNTIL THE FORMATIONS MEET");
		if (SinceMet < 0.0) SinceMet = 0.0;
		SinceMet += Real;
		if (Head.bWinding) return RunFast("\xC3\x97" "10 UNTIL THE LAST ARE HOME");
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
		KnotShown = 0.5;
		Settled.clear();
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

	void Desk::Struck(const AirMarks::Column& Shown, double Cut)
	{
		if (!OnTroops()) return;
		// The vehicles and guns wrecked, burning and damaged, and the men lost from the blocks, as the show has them now:
		// each piece as the hits shown on it and the views' words after them leave it (AirShow::Show::Marks), so that the
		// aim slip never counts a hit whose bomb or rounds are still on their way (the reviewer, 25 November: it had
		// counted each as its view came). The cut so far, likewise (Show::MarksCut).
		MarksWrecked = MarksBurning = MarksDamaged = MarksMenLost = 0;
		for (const AirMarks::Piece& P : Shown.Pieces)
		{
			if (P.What == "men")
			{
				MarksMenLost += std::max(0, P.N - P.Men);
				continue;
			}
			MarksWrecked += P.Now == AirMarks::State::Out ? 1 : 0;
			MarksBurning += P.Now == AirMarks::State::Burn ? 1 : 0;
			MarksDamaged += P.Now == AirMarks::State::Hit ? 1 : 0;
		}
		MarksCutNow = std::max(MarksCutNow, Cut);
	}

	bool Desk::ColumnTheirs() const
	{
		// (the column is the enemy's when its nation is theirs; his own when it is his; any other (an ally's, or none
		// said) stands with the side whose ground it is, the defender's)
		const std::string& Mine = MyIndex == 0 ? Sky.AttNat : Sky.DefNat;
		const std::string& Theirs = MyIndex == 0 ? Sky.DefNat : Sky.AttNat;
		if (!Sky.Marks.Nat.empty() && Sky.Marks.Nat == Mine) return false;
		if (!Sky.Marks.Nat.empty() && Sky.Marks.Nat == Theirs) return true;
		return MyIndex == 0;
	}

	void Desk::Read(const AirFeed::Frame& View)
	{
		HeadNow = View.Head;
		bStarted = true;
		// (release 2, a raid on troops: the view's marks are not read here, but from the show, as its pieces are drawn
		// struck: Struck)
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
		// (a line still on the screen is not said again: Oahu 02 showed one twice, word for word, on the Mac's release 2
		// check; the screen lets a dispatch go 14 s after it is said)
		for (const Dispatch& Was : Said)
		{
			if (Was.Text == Text && Was.Age < 14.0) return;
		}
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
		Out.Title = OnTroops() ? TroopsTitle() : Sky.Title;
		Out.MySide = MyIndex == 0 ? Sky.AttAdj : Sky.DefAdj;
		Out.TheirSide = MyIndex == 0 ? Sky.DefAdj : Sky.AttAdj;
		// The top bar (b384, land's form): the card's chance and the staff's now, his side's.
		const double Card = MyIndex == 0 ? Sky.CardChance : 1.0 - Sky.CardChance;
		// (release 2, the scramble offer: the view's chance is the raider's; read for the defender, it is turned to his)
		const double StaffNow = DefendRead() ? 1.0 - H.Chance : H.Chance;
		Out.Chance = (Sky.CardChance > 0.0 ? "Card about " + Percent(Card) + " \xC2\xB7 now about " : std::string("Now about ")) + Percent(StaffNow);
		// (release 2, a raid on troops: what the raid has done to the army so far, as the rules count it, view.marks.cut;
		// a raid cuts 4% of an army's organisation at most)
		if (OnTroops())
		{
			Out.Chance += std::string(" \xC2\xB7 ") + (ColumnTheirs() ? "their" : "your") + " column's organisation cut by " + Tenths(MarksCutNow) + " so far";
		}
		// The tug-of-war bar (it took the place of the strength fill and its start mark: the UI director, 14:47Z).
		KnotShown = KnotEase(KnotShown, KnotOf(H, MyIndex, Winner()), Real);
		Out.Knot = KnotShown;
		Out.MyNat = MyIndex == 0 ? Sky.AttNat : Sky.DefNat;
		Out.TheirNat = MyIndex == 0 ? Sky.DefNat : Sky.AttNat;
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
		else if (OnTroops())
		{
			// (release 2: "The British I Corps on the march near Hampshire")
			std::string Named = ColumnName();
			if (!Named.empty()) Named[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(Named[0])));
			Out.AimTarget = Named + PostureWords(Sky.Marks.Posture) + (Sky.PlaceName.empty() ? std::string() : " near " + Sky.PlaceName);
		}
		else
		{
			Out.AimTarget = Place;
		}
		if (OnTroops())
		{
			// (release 2, a raid on troops: the vehicles and guns wrecked, burning and damaged, and the men lost from the
			// blocks, as the rules count them and as the show has drawn them struck (Struck); never the scene's own)
			const int Wrecked = MarksWrecked, Burning = MarksBurning, Damaged = MarksDamaged, MenLost = MarksMenLost;
			std::string Done;
			auto Add = [&Done](int N, const char* One, const char* Many)
			{
				if (N <= 0) return;
				Done += (Done.empty() ? "" : ", ") + std::to_string(N) + " " + (N == 1 ? One : Many);
			};
			Add(Wrecked, "vehicle or gun wrecked", "vehicles and guns wrecked");
			Add(Burning, "burning", "burning");
			Add(Damaged, "damaged", "damaged");
			Add(MenLost, "man lost", "men lost");
			const bool bTheirs = ColumnTheirs();
			Out.AimState = Done.empty() ? std::string(bTheirs ? "No hits on the column yet" : "No hits on your column yet")
				: std::string(bTheirs ? "On the column so far: " : "On your column so far: ") + Done;
		}
		else if (Sky.PlaceKind == "sky" || Sky.PlaceKind == "superiority")
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
		else if (DefendRead())
		{
			// (release 2, the scramble offer: the raid is the enemy's, and he is the one it costs)
			Out.AimState = H.Ratio >= 1.0 ? "So far the raid's bombs have done more than it has cost the enemy" : "So far the raid has cost the enemy more than its bombs have done";
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

	void Desk::SettleFrom(const AirFeed::After& After)
	{
		// (the card's level is the verdict as the player reads it: "Close victory", "Decisive defeat", "Crushing defeat")
		std::string Level = After.Level;
		for (char& Ch : Level) Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
		const std::string Other = Me == "att" ? "def" : "att";
		if (Level.find("victory") != std::string::npos) Settled = Me;
		else if (Level.find("defeat") != std::string::npos) Settled = Other;
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
		if (OnTroops())
		{
			// (release 2, a raid on troops: the card's title names the column, and a line that speaks of "the target"
			// speaks of the column)
			C.Title = TroopsTitle();
			auto ColumnWords = [](const std::string& Line)
			{
				return Swapped(Swapped(Swapped(Line, "before the target", "before they reached the column"), "the target", "the column"), "The target", "The column");
			};
			C.Reckoning = ColumnWords(C.Reckoning);
			for (std::string& Line : C.Lines)
			{
				Line = ColumnWords(Line);
			}
		}
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

	std::string Desk::Strafe(const std::vector<int>& Ids, const std::vector<Tag>& Tags) const
	{
		if (!CanStrafe()) return std::string();
		std::vector<int> Fighters;
		for (const int Id : Ids)
		{
			for (const Tag& T : Tags)
			{
				if (T.Id == Id && T.bMine && T.bFighter && !T.bGone)
				{
					Fighters.push_back(Id);
					break;
				}
			}
		}
		return Fighters.empty() ? std::string() : "{\"do\":\"air:strafe\",\"units\":" + Units(Fighters) + "}";
	}

	std::string Desk::ColumnName() const
	{
		const std::string Adj = Sky.Marks.Nat == Sky.DefNat ? Sky.DefAdj : Sky.Marks.Nat == Sky.AttNat ? Sky.AttAdj : std::string();
		const std::string Whose = Adj.empty() ? std::string("the ") : "the " + Adj + " ";
		if (!Sky.Marks.Under.empty() && !Sky.Marks.Under.front().Name.empty())
		{
			return Whose + Sky.Marks.Under.front().Name;
		}
		return Whose + "column";
	}

	std::string Desk::TroopsTitle() const
	{
		return "The raid on " + ColumnName() + (Sky.PlaceName.empty() ? std::string() : " near " + Sky.PlaceName);
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
		// (release 2, the scramble offer: read for the defender, the raid is the enemy's to hand over, the fight his, and
		// the chance the staff give him his own, not the raider's)
		const bool bDefending = DefendRead();
		std::string Q = bSky || bDefending ? "Hand the fight to the staff?" : "Hand the raid to the staff?";
		const double Yours = bDefending ? 1.0 - HeadNow.Chance : HeadNow.Chance;
		if (!HeadNow.bFired) return Q + " No shot has been fired: they settle the whole raid at once. They give you about " + Percent(Yours) + " to win it.";
		return Q + " They settle the rest at once, from where it stands now. They give you about " + Percent(Yours) + " to win it.";
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

		/** The way an aircraft flies (board axes: east, north, up). */
		AirShow::Vec WayOf(const AirShow::PlaneOut& P)
		{
			const double H = P.Heading * Pi / 180.0, Up = P.Pitch * Pi / 180.0;
			return AirShow::Vec(std::sin(H) * std::cos(Up), std::cos(H) * std::cos(Up), std::sin(Up));
		}

		const AirShow::PlaneOut* QuarryOf(const AirShow::PlaneOut& P, const std::vector<AirShow::PlaneOut>& All)
		{
			if (!P.bFighter || P.Target < 0) return nullptr;
			for (const AirShow::PlaneOut& Q : All)
			{
				if (Q.Key == P.Target) return Q.bShown && !Q.bFalling && !Q.bOnGround ? &Q : nullptr;
			}
			return nullptr;
		}

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

	double HeadOn(const AirShow::PlaneOut& Fighter, const std::vector<AirShow::PlaneOut>& All)
	{
		const AirShow::PlaneOut* Quarry = QuarryOf(Fighter, All);
		if (!Quarry || Quarry->bFighter) return 0.0;
		const AirShow::Vec Mine = WayOf(Fighter), Its = WayOf(*Quarry);
		const AirShow::Vec Gap = Quarry->At - Fighter.At;
		const double Apart = Gap.Length();
		if (Apart < 1.0) return 0.0;
		// (flying the other way to it, and out ahead of it)
		const double Against = std::max(0.0, -Mine.Dot(Its));
		const double Ahead = std::max(0.0, -Gap.Dot(Its) / Apart);
		return Against * Ahead;
	}

	Subject ChooseRide(const std::vector<AirShow::PlaneOut>& Planes, const std::vector<AirShow::PlaneOut>& All, const AirShow::Vec& Near, int Avoid)
	{
		Subject Best;
		double BestScore = 1e18;
		for (const AirShow::PlaneOut& P : Planes)
		{
			if (!P.bShown || P.bOnGround || P.bFalling || P.Key == Avoid) continue;
			const AirShow::PlaneOut* Quarry = QuarryOf(P, All);
			if (!Quarry) continue;
			const AirShow::Vec Gap = Quarry->At - P.At;
			const double Apart = Gap.Length();
			// (within 2.5 km of it, and the nearer its nose is pointed at it the better: the quarry ahead of the eye that
			// rides behind it, or soon to be as it turns in)
			if (Apart > 2500.0 || Apart < 1.0) continue;
			const double Pointed = Gap.Dot(WayOf(P)) / Apart;
			const AirShow::Vec Off = P.At - Near;
			// (a bomber before any fighter, however near: the raid's attackers are the picture; head-on most of all)
			const double Score = Apart + (P.bFiring ? 0.0 : 400.0) + 900.0 * (1.0 - Pointed) + (Quarry->bFighter ? 5000.0 : 0.0) - 1200.0 * HeadOn(P, All) + 0.1 * Off.Length();
			if (Score < BestScore)
			{
				BestScore = Score;
				Best.Plane = P.Key;
				Best.Other = P.Target;
			}
		}
		return Best.Plane >= 0 ? Best : Choose(Planes, std::vector<int>(), Near, Avoid);
	}

	std::vector<int> RideLine(const std::vector<AirShow::PlaneOut>& Planes, int Group)
	{
		std::vector<int> Line;
		for (const AirShow::PlaneOut& P : Planes)
		{
			if (P.Group == Group && P.bShown && !P.bOnGround && !P.bFalling) Line.push_back(P.Key);
		}
		std::sort(Line.begin(), Line.end());
		return Line;
	}

	int RideStep(const std::vector<AirShow::PlaneOut>& Planes, int Group, int Current, int Step)
	{
		const std::vector<int> Line = RideLine(Planes, Group);
		if (Line.empty()) return -1;
		if (Step == 0)
		{
			return std::binary_search(Line.begin(), Line.end(), Current) ? Current : Line.front();
		}
		if (Step > 0)
		{
			const auto After = std::upper_bound(Line.begin(), Line.end(), Current);
			return After == Line.end() ? Line.front() : *After;
		}
		const auto Before = std::lower_bound(Line.begin(), Line.end(), Current);
		return Before == Line.begin() ? Line.back() : *(Before - 1);
	}

	std::vector<int> RideSquadrons(const std::vector<Tag>& Tags, const std::vector<AirShow::PlaneOut>& Planes)
	{
		std::vector<int> Out;
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (const Tag& T : Tags)
			{
				if (T.bMine != (Pass == 0) || T.bGone) continue;
				if (!RideLine(Planes, T.Id).empty()) Out.push_back(T.Id);
			}
		}
		return Out;
	}

	int RideSquadronStep(const std::vector<Tag>& Tags, const std::vector<AirShow::PlaneOut>& Planes, int Current, int Step)
	{
		const std::vector<int> Ones = RideSquadrons(Tags, Planes);
		if (Ones.empty()) return -1;
		const auto At = std::find(Ones.begin(), Ones.end(), Current);
		if (At == Ones.end()) return Step < 0 ? Ones.back() : Ones.front();
		const int N = static_cast<int>(Ones.size());
		const int Here = static_cast<int>(At - Ones.begin());
		return Ones[static_cast<size_t>(((Here + (Step < 0 ? -1 : 1)) % N + N) % N)];
	}

	std::string RideCount(const std::vector<AirShow::PlaneOut>& Planes, int Group, int Current)
	{
		const std::vector<int> Line = RideLine(Planes, Group);
		const auto At = std::find(Line.begin(), Line.end(), Current);
		if (At == Line.end()) return std::string();
		return "Aircraft " + std::to_string(At - Line.begin() + 1) + " of " + std::to_string(Line.size());
	}

	bool CrowdOf(const std::vector<AirShow::PlaneOut>& Planes, const std::vector<int>& Groups, double Reach, int& Anchor, AirShow::Vec& Middle)
	{
		std::vector<const AirShow::PlaneOut*> In;
		for (const AirShow::PlaneOut& P : Planes)
		{
			if (P.bShown && !P.bFalling && !P.bOnGround && std::find(Groups.begin(), Groups.end(), P.Group) != Groups.end())
			{
				In.push_back(&P);
			}
		}
		if (In.empty())
		{
			Anchor = -1;
			return false;
		}
		const double Reach2 = Reach * Reach;
		auto Near = [Reach2](const AirShow::PlaneOut& A, const AirShow::PlaneOut& B)
		{
			const AirShow::Vec Off = B.At - A.At;
			return Off.Dot(Off) <= Reach2;
		};
		const AirShow::PlaneOut* Best = nullptr;
		const AirShow::PlaneOut* Held = nullptr;
		int Most = -1, HeldCount = 0;
		for (const AirShow::PlaneOut* A : In)
		{
			int Around = 0;
			for (const AirShow::PlaneOut* B : In)
			{
				Around += Near(*A, *B) ? 1 : 0;
			}
			if (Around > Most)
			{
				Most = Around;
				Best = A;
			}
			if (A->Key == Anchor)
			{
				Held = A;
				HeldCount = Around;
			}
		}
		if (Held && HeldCount * 4 >= Most * 3)
		{
			Best = Held;
		}
		Anchor = Best->Key;
		AirShow::Vec Sum;
		int Count = 0;
		for (const AirShow::PlaneOut* B : In)
		{
			if (Near(*Best, *B))
			{
				Sum += B->At;
				++Count;
			}
		}
		Middle = Sum * (1.0 / std::max(Count, 1));
		return true;
	}

	namespace
	{
		/** A critically damped ease over Real seconds of what is still to go (Off, from where it is wanted) and its
		 * speed (Vel): about three fifths of the way in Ease seconds and nine tenths in twice it, never past, and its
		 * speed never stepping. */
		template <typename T>
		void Settle(T& Off, T& Vel, double Real, double Ease)
		{
			const double W = 2.0 / std::max(Ease, 1e-3);
			const double Fall = std::exp(-W * std::max(Real, 0.0));
			const T Lead = Vel + Off * W;
			Off = (Off + Lead * Real) * Fall;
			Vel = (Vel - Lead * (W * Real)) * Fall;
		}

		/** An aircraft's weight in a ring of Reach metres about a point it is Far from: 1 well inside, 0 well outside,
		 * eased between over Overview::Soft metres either side of the ring. */
		double InRing(double Far, double Reach)
		{
			const double X = std::clamp((Reach + Overview::Soft - Far) / (2.0 * Overview::Soft), 0.0, 1.0);
			return X * X * (3.0 - 2.0 * X);
		}
	}

	double Overview::TypeAwayOf(double Span)
	{
		const double Heavy = std::clamp((Span - 24.0) / 2.0, 0.0, 1.0);
		const double Pixels = 45.0 + 25.0 * Heavy * Heavy * (3.0 - 2.0 * Heavy);
		return std::clamp(Span * 1920.0 / (Pixels * 2.0 * std::tan(27.5 * 3.14159265358979323846 / 180.0)), 250.0, 1500.0);
	}

	Overview::Aim Overview::Step(const std::vector<AirShow::PlaneOut>& Planes, const std::vector<int>& Picked, double Real, int& Anchor, bool bCut)
	{
		if (bCut)
		{
			Fade.clear();
			Prev.clear();
			bStarted = bGiven = false;
			Away = -1.0;
			AwayVel = 0.0;
			LookVel = AirShow::Vec();
		}
		Carry = AirShow::Vec();
		// (each aircraft's share of its weight: eased in while it flies, is drawn and is of the groups looked at, eased
		// out once it is not; all in at once on a cut)
		const double Ease = std::clamp(Real / FadeS, 0.0, 1.0);
		std::map<int, double> Shares;
		std::map<int, bool> In;
		for (const AirShow::PlaneOut& P : Planes)
		{
			const bool bIn = P.bShown && !P.bFalling && !P.bOnGround && std::find(Picked.begin(), Picked.end(), P.Group) != Picked.end();
			const auto It = Fade.find(P.Key);
			const double Was = It != Fade.end() ? It->second : (!bStarted && bIn ? 1.0 : 0.0);
			const double Share = bIn ? std::min(1.0, Was + Ease) : std::max(0.0, Was - Ease);
			if (Share > 0.0)
			{
				Shares[P.Key] = Share;
				In[P.Key] = bIn;
			}
		}
		Fade.swap(Shares);
		bStarted = true;
		Aim A;
		auto Frame = [this, Real, &A](double Wanted)
		{
			if (Away < 0.0) Away = Wanted;
			double Off = Away - Wanted;
			Settle(Off, AwayVel, Real, AwayEase);
			Away = Wanted + Off;
			A.TypeAway = Away;
		};
		AirShow::Vec Crowd;
		const AirShow::PlaneOut* Held = nullptr;
		if (CrowdOf(Planes, Picked, Reach, Anchor, Crowd))
		{
			for (const AirShow::PlaneOut& P : Planes)
			{
				if (P.Key == Anchor) Held = &P;
			}
		}
		if (!Held)
		{
			Prev.clear();
			Frame(800.0);
			return A;
		}
		// The crowd's middle: about its anchor, as CrowdOf's, each aircraft by its weight in the ring.
		AirShow::Vec Mid;
		double MidWeight = 0.0;
		for (const AirShow::PlaneOut& P : Planes)
		{
			const auto Share = Fade.find(P.Key);
			if (Share == Fade.end()) continue;
			const double W = Share->second * InRing((P.At - Held->At).Length(), Reach);
			Mid += P.At * W;
			MidWeight += W;
		}
		Mid = MidWeight > 1e-9 ? Mid * (1.0 / MidWeight) : Held->At;
		// The look: the middle of those within Tight of it, and with fewer than two there the one nearest it (eased from
		// one to the other between one and two); its travel since the last frame, the same aircraft by the same weights.
		// The framing: the crowd's mean span.
		AirShow::Vec Close, CloseStep, Nearest, NearestStep;
		double CloseWeight = 0.0, StepWeight = 0.0, SpanSum = 0.0, SpanWeight = 0.0, NearestFar = 1e18;
		for (const AirShow::PlaneOut& P : Planes)
		{
			const auto Share = Fade.find(P.Key);
			if (Share == Fade.end()) continue;
			const double Far = (P.At - Mid).Length();
			const double Wc = Share->second * InRing(Far, Tight), Ws = Share->second * InRing(Far, Reach);
			Close += P.At * Wc;
			CloseWeight += Wc;
			SpanSum += Ws * (P.Span > 1.0 ? P.Span : 12.0);
			SpanWeight += Ws;
			const auto Was = Prev.find(P.Key);
			if (Was != Prev.end())
			{
				CloseStep += (P.At - Was->second) * Wc;
				StepWeight += Wc;
			}
			if (In[P.Key] && Far < NearestFar)
			{
				NearestFar = Far;
				Nearest = P.At;
				NearestStep = Was != Prev.end() ? P.At - Was->second : AirShow::Vec();
			}
		}
		Prev.clear();
		for (const AirShow::PlaneOut& P : Planes)
		{
			if (Fade.count(P.Key)) Prev[P.Key] = P.At;
		}
		if (NearestFar > 1e17)
		{
			Nearest = Held->At;
			NearestStep = AirShow::Vec();
		}
		const double Many = std::clamp(CloseWeight - 1.0, 0.0, 1.0);
		const AirShow::Vec CloseMid = CloseWeight > 1e-9 ? Close * (1.0 / CloseWeight) : Nearest;
		const AirShow::Vec CloseTravel = StepWeight > 1e-9 ? CloseStep * (1.0 / StepWeight) : AirShow::Vec();
		A.bFound = true;
		A.Focus = Nearest + (CloseMid - Nearest) * Many;
		Carry = NearestStep + (CloseTravel - NearestStep) * Many;
		A.Span = SpanWeight > 1e-9 ? SpanSum / SpanWeight : 12.0;
		Frame(TypeAwayOf(A.Span));
		return A;
	}

	AirShow::Vec Overview::Look(const AirShow::Vec& Was, const AirShow::Vec& Wanted, double Real)
	{
		// (the aircraft's speed it goes with, eased; a look set otherwise since this one's last, a picture's or a cut's,
		// starts from rest and at their speed as it is)
		const AirShow::Vec Rate = Real > 1e-6 ? Carry * (1.0 / Real) : CarryVel;
		if (!bGiven || (Was - Given).Length() > 0.01)
		{
			LookVel = AirShow::Vec();
			CarryVel = Rate;
		}
		else CarryVel += (Rate - CarryVel) * (1.0 - std::exp(-std::max(Real, 0.0) / CarryEase));
		AirShow::Vec Off = Was + CarryVel * Real - Wanted;
		Carry = AirShow::Vec();
		Settle(Off, LookVel, Real, LookEase);
		Given = Wanted + Off;
		bGiven = true;
		return Given;
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
		Middles.clear();
		Trios.clear();
		RaidSide = 0.0;
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
		// The raid's and the guard's shots: from above and just behind three of the formation's planes, looking down
		// Down degrees at them (RaidLookDown, LookDown) so the sea or the cloud deck is behind them, near enough that each
		// of the three is about Share of the picture's width; pushing in from 1.2 times that to 0.9 as the shot runs, and
		// swinging in behind them from out on one side. The guard's three are its lead: the plane furthest ahead as the
		// shot begins and the two nearest it (they fly some 30 to 150 m behind the slots their group's place gives). The
		// raid's are its trailing element (bTrail: the plane furthest back and the two nearest it), so the whole raid
		// stretches ahead of them toward the horizon in its vics (the art director on build 1760: three planes alone do
		// not read as a raid). With none of them up, the group's first three slots.
		auto Behind = [&](const AirShow::Group& G, double Side, double S, int Index, double DownDegrees, bool bTrail, double Share)
		{
			const AirShow::Vec Ahead(std::sin(G.Face), std::cos(G.Face), 0.0), Right(std::cos(G.Face), -std::sin(G.Face), 0.0);
			std::vector<int>& Three = Leads[G.Id];
			if (Three.empty())
			{
				// (the plane furthest ahead, or furthest back, and the two nearest it: its own element, a vic of three,
				// not three planes strung across the formation's front. One whose element is not with it, its second
				// nearest more than twice as far as the group's own spacing (the middle of every plane's second nearest),
				// is passed over for the next: a fighter group's last flight may be a pair strung out a kilometre behind
				// the rest, and three planes taken across two flights put the eye among them, looking at empty sky; the
				// shakeout of the campaign's fight for the sky, 23 November. The set pieces' first plane always passes.)
				const double Sense = bTrail ? -1.0 : 1.0;
				std::vector<const AirShow::PlaneOut*> Mine;
				for (const AirShow::PlaneOut& P : Planes)
				{
					if (P.Group == G.Id && P.bShown && !P.bFalling) Mine.push_back(&P);
				}
				// (furthest ahead first, or furthest back; the first seen first among equals, as before)
				std::stable_sort(Mine.begin(), Mine.end(), [&](const AirShow::PlaneOut* A, const AirShow::PlaneOut* B) { return Sense * Ahead.Dot(A->At) > Sense * Ahead.Dot(B->At); });
				auto Nearest = [&Mine](const AirShow::PlaneOut* Of)
				{
					std::vector<std::pair<double, int>> Order;
					for (const AirShow::PlaneOut* P : Mine) Order.push_back({ (P->At - Of->At).Length(), P->Key });
					std::sort(Order.begin(), Order.end());
					return Order;
				};
				size_t Pick = 0;
				if (Mine.size() >= 3)
				{
					std::vector<double> Seconds;
					for (const AirShow::PlaneOut* P : Mine) Seconds.push_back(Nearest(P)[2].first);
					std::vector<double> Sorted = Seconds;
					std::nth_element(Sorted.begin(), Sorted.begin() + Sorted.size() / 2, Sorted.end());
					const double Spacing = Sorted[Sorted.size() / 2];
					while (Pick < Mine.size() && Seconds[Pick] > 2.0 * Spacing) ++Pick;
					if (Pick == Mine.size()) Pick = 0;
				}
				if (!Mine.empty())
				{
					const std::vector<std::pair<double, int>> Order = Nearest(Mine[Pick]);
					for (size_t K = 0; K < Order.size() && K < 3; ++K) Three.push_back(Order[K].second);
				}
				if (bTrail)
				{
					// (the raid's middle as the shot begins, kept in the group's own frame, ahead, right and up of its
					// place: the look rides it with the group, and never hops as a plane drops out)
					AirShow::Vec Sum;
					int Count = 0;
					for (const AirShow::PlaneOut& P : Planes)
					{
						if (P.Group == G.Id && P.bShown && !P.bFalling)
						{
							Sum = Sum + P.At;
							++Count;
						}
					}
					const AirShow::Vec Off = Count > 0 ? Sum * (1.0 / Count) - AirShow::Vec(G.X, G.Y, G.Alt) : AirShow::Vec();
					Middles[G.Id] = AirShow::Vec(Ahead.Dot(Off), Right.Dot(Off), Off.Z);
					// (and where each of the three flies off their own middle, kept the same way: the look is held to
					// these, so it does not hop as one of them drops out)
					std::vector<AirShow::Vec>& Trio = Trios[G.Id];
					Trio.clear();
					AirShow::Vec Mid;
					for (const AirShow::PlaneOut& P : Planes)
					{
						if (P.bShown && std::find(Three.begin(), Three.end(), P.Key) != Three.end())
						{
							Trio.push_back(P.At);
							Mid = Mid + P.At;
						}
					}
					for (AirShow::Vec& Each : Trio)
					{
						const AirShow::Vec From = Each - Mid * (1.0 / Trio.size());
						Each = AirShow::Vec(Ahead.Dot(From), Right.Dot(From), From.Z);
					}
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
			double Near = Span / (2.0 * std::tan(Lens * 0.5 * Pi / 180.0) * Share);
			// (the raid's three wider than the picture holds at that size, a fighter sweep's loose flights some 400 m
			// across against a bomber vic's 100: the eye stands back until each is within RaidAside of the look, so the
			// raid is framed smaller rather than lost; the shakeout of the campaign's fight for the sky, 23 November.
			// A bomber raid's three always fit, and its shot is as it was)
			if (bTrail)
			{
				const auto Spread = Trios.find(G.Id);
				if (Spread != Trios.end())
				{
					double Aside = 0.0, Deep = 0.0;
					for (const AirShow::Vec& Each : Spread->second)
					{
						Aside = std::max(Aside, std::fabs(Each.Y));
						Deep = std::max(Deep, std::fabs(Each.X));
					}
					Near = std::max(Near, Aside * 1.15 / std::tan(RaidAside * Pi / 180.0) + Deep);
				}
			}
			const double Pull = Near * (1.2 - 0.3 * S), Down = DownDegrees * Pi / 180.0;
			Out.Eye = Lead - Ahead * (Pull * std::cos(Down)) + Up * (Pull * std::sin(Down)) + Right * (Side * Near * 0.3 * (1.0 - S));
			Out.Look = Lead;
			// (the raid's shot looks on along the raid, square across at its middle and RaidLookOn of the way ahead to it,
			// the eye still Down degrees over its trailing three: they stand low in the picture and the rest recede ahead
			// of them toward the horizon)
			const auto Middle = Middles.find(G.Id);
			if (bTrail && Middle != Middles.end())
			{
				const AirShow::Vec& M = Middle->second;
				const AirShow::Vec OnTo = AirShow::Vec(G.X, G.Y, G.Alt) + Ahead * M.X + Right * M.Y + Up * M.Z - Lead;
				const AirShow::Vec Across = Right * Right.Dot(OnTo);
				Out.Look = Lead + Across + (OnTo - Across) * RaidLookOn;
				// (but never so far on that one of the three leaves the picture: the look is drawn back toward them
				// until each stands no more than RaidLow degrees under the picture's middle and RaidAside to its side)
				const auto Trio = Trios.find(G.Id);
				if (Trio != Trios.end() && !Trio->second.empty())
				{
					const AirShow::Vec Full = Out.Look - Lead;
					const double TanLow = std::tan(RaidLow * Pi / 180.0), TanAside = std::tan(RaidAside * Pi / 180.0);
					auto Holds = [&](double K)
					{
						const AirShow::Vec Fw = (Lead + Full * K - Out.Eye).Unit(Ahead);
						const AirShow::Vec Rt = AirShow::Vec(Fw.Y, -Fw.X, 0.0).Unit(Right);
						const AirShow::Vec Hi = Rt.Cross(Fw);
						for (const AirShow::Vec& Each : Trio->second)
						{
							const AirShow::Vec To = Lead + Ahead * Each.X + Right * Each.Y + Up * Each.Z - Out.Eye;
							const double Deep = To.Dot(Fw);
							if (Deep <= 1.0 || -To.Dot(Hi) > Deep * TanLow || std::fabs(To.Dot(Rt)) > Deep * TanAside)
							{
								return false;
							}
						}
						return true;
					};
					if (!Holds(1.0))
					{
						double Kept = 0.0, Lost = 1.0;
						for (int Halving = 0; Halving < 16; ++Halving)
						{
							const double Half = 0.5 * (Kept + Lost);
							(Holds(Half) ? Kept : Lost) = Half;
						}
						Out.Look = Lead + Full * Kept;
					}
				}
			}
			Out.Index = Index;
		};
		if (T < RaidLong)
		{
			// The raid (gone before its shot is over, its last pose stands); out on the side away from the target as the
			// shot begins.
			const AirShow::Group* R = Find(Groups, Raid);
			if (R)
			{
				// (the side taken once, as the shot begins: a raid flying straight at its target would otherwise swing the
				// eye across the formation as its heading wavers by a hundredth of a degree; the flight test on Bremen)
				if (RaidSide == 0.0)
				{
					const AirShow::Vec Right(std::cos(R->Face), -std::sin(R->Face), 0.0);
					RaidSide = Right.Dot(Target - AirShow::Vec(R->X, R->Y, R->Alt)) > 0.0 ? -1.0 : 1.0;
				}
				Behind(*R, RaidSide, Ease(T / RaidLong), 0, RaidLookDown, true, TrailShare);
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
				Behind(*G, 1.0, Ease((T - RaidLong - TargetLong) / GuardLong), 2, LookDown, false, LeadShare);
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

	double KnotOf(const AirFeed::Top& Head, int MyIndex)
	{
		return KnotOf(Head, MyIndex, Head.Winner);
	}

	double KnotOf(const AirFeed::Top& Head, int MyIndex, const std::string& Winner)
	{
		double K = 0.5;
		if (Head.LostAtt + Head.LostDef > 0 || Head.Worth > 0.0)
		{
			// (the ratio has the rules' floors of 10 aircraft each way, so the first loss moves it a little, never flings it)
			const double X = std::isfinite(Head.Ratio) && Head.Ratio > 0.0 ? Head.Ratio : 1.0;
			K = X / (1.0 + X);
			if (MyIndex == 1) K = 1.0 - K; // (1 / x) / (1 + 1 / x)
		}
		if (Head.bOver)
		{
			// The stamp decides the side: a side that held the field won whatever the exchange said, and the knot settles past
			// the middle on its side (the ruling: "settles on the side the stamp names"). Since release 1.5 the stamp is the
			// after card's once it is read (Desk::Winner), so the bar never ends on the side the card then calls beaten.
			const int Won = AirFeed::SideOf(Winner);
			if (Won == MyIndex) K = std::max(K, 0.5 + KnotSettle);
			else if (Won == 1 - MyIndex) K = std::min(K, 0.5 - KnotSettle);
		}
		return std::min(0.98, std::max(0.02, K));
	}

	double KnotEase(double Shown, double Want, double Real)
	{
		if (!(Real > 0.0) || !std::isfinite(Want)) return Shown;
		return Shown + (Want - Shown) * (1.0 - std::exp(-std::min(Real, 0.1) / KnotLag));
	}
}
