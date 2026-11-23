#include "Battle/LandDesk.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace LandDesk
{
	void Desk::Begin(const LandFeed::Recording& InRecording, int InOwn)
	{
		Rec = &InRecording;
		Own = InOwn;
		Now = Words();
		bEased = false;
		Now.Title = Rec->Title;
		Now.Date = Rec->Date;
		Now.Place = Rec->Ground.PlaceName;
		Now.OwnNat = Rec->Sides[Own].Nat;
		Now.EnemyNat = Rec->Sides[1 - Own].Nat;
		bBalance0 = false;
		if (!Rec->Views.empty())
		{
			Balance0[0] = std::max(0.01, Rec->Views.front().BalanceAtt);
			Balance0[1] = std::max(0.01, Rec->Views.front().BalanceDef);
			bBalance0 = true;
		}
	}

	double Desk::KnotAt(double Battle) const
	{
		const LandFeed::View* V = Rec ? Rec->ViewAt(Battle) : nullptr;
		if (!V) return 0.5;
		// The exchange, as the attacker reads it: the share each side has lost, with the rules' floor of 0.05.
		const double LostAtt = std::max(0.0, 1.0 - V->BalanceAtt / Balance0[0]);
		const double LostDef = std::max(0.0, 1.0 - V->BalanceDef / Balance0[1]);
		double X = (LostDef + 0.05) / (LostAtt + 0.05);
		if (Own == 1) X = 1.0 / X;
		const double Exchange = X / (1.0 + X);
		// The bars: the enemy's nearer breaking than his own pushes the knot his way.
		const double OwnBar = Own == 0 ? V->BreakingAtt : V->BreakingDef;
		const double EnemyBar = Own == 0 ? V->BreakingDef : V->BreakingAtt;
		const double BarKnot = std::clamp(0.5 + 0.5 * (EnemyBar - OwnBar), 0.0, 1.0);
		const double Weight = std::min(1.0, (V->BreakingAtt + V->BreakingDef) / 0.3);
		double Knot = Exchange + (BarKnot - Exchange) * Weight;
		// Over the last quarter of the limit the clock pulls it toward the defender, who wins by holding.
		if (Rec->Limit > 0.0 && Battle > Rec->Limit * 0.75)
		{
			const double Pull = std::clamp((Battle - Rec->Limit * 0.75) / (Rec->Limit * 0.25), 0.0, 1.0) * 0.15;
			Knot += Own == 1 ? Pull : -Pull;
		}
		// Settled at the end on the winner's side of the middle.
		if (Battle >= Rec->End && !Rec->Result.Winner.empty())
		{
			const bool bWon = (Rec->Result.Winner == "att") == (Own == 0);
			Knot = bWon ? std::max(Knot, 0.58) : std::min(Knot, 0.42);
		}
		return std::clamp(Knot, 0.02, 0.98);
	}

	const Words& Desk::Update(double Battle, double RealDt)
	{
		if (!Rec) return Now;
		// (a live battle's first view comes with its first delta: the balances the knot is read against are taken then)
		if (Rec->bLive && !bBalance0 && !Rec->Views.empty())
		{
			Balance0[0] = std::max(0.01, Rec->Views.front().BalanceAtt);
			Balance0[1] = std::max(0.01, Rec->Views.front().BalanceDef);
			bBalance0 = true;
		}
		const double Shown = std::clamp(Battle, 0.0, Rec->End);
		const int Seconds = static_cast<int>(std::floor(Shown));
		char Buffer[16];
		std::snprintf(Buffer, sizeof(Buffer), "%d:%02d", Seconds / 60, Seconds % 60);
		Now.Clock = Buffer;
		const double Want = KnotAt(Battle);
		if (!bEased) { Now.Knot = Want; bEased = true; }
		else Now.Knot += (Want - Now.Knot) * (1.0 - std::exp(-RealDt / 1.5));
		if (const LandFeed::View* V = Rec->ViewAt(Battle))
		{
			Now.BreakOwn = Own == 0 ? V->BreakingAtt : V->BreakingDef;
			Now.BreakEnemy = Own == 0 ? V->BreakingDef : V->BreakingAtt;
		}
		// The battle's dispatches as it speaks them: its own words (contact, wavering, broken), newest last; each
		// shown 9 battle seconds, at most four.
		Now.Lines.clear();
		for (const LandFeed::Event& E : Rec->Events)
		{
			if (E.T > Battle) break;
			if (E.Text.empty() || E.What == "shell" || E.What == "end") continue;
			if (Battle - E.T > 9.0) continue;
			Line L;
			L.T = E.T;
			L.Text = E.Text;
			L.Side = LandFeed::SideOf(E.Side);
			L.Age = std::clamp((Battle - E.T) / 9.0, 0.0, 1.0);
			Now.Lines.push_back(L);
		}
		if (Now.Lines.size() > 4) Now.Lines.erase(Now.Lines.begin(), Now.Lines.end() - 4);
		// (a live battle is over only once the rules say so: until then its End is only how far they have fought)
		Now.bOver = Battle >= Rec->End && (!Rec->bLive || Rec->bOver);
		if (Now.bOver)
		{
			Now.Level = Rec->Result.Level;
			Now.After = Rec->Result.Lines;
		}
		return Now;
	}
}
