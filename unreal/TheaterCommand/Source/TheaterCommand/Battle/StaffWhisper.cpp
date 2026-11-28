#include "Battle/StaffWhisper.h"

#include "Battle/LandJson.h"

#include <cmath>

namespace StaffWhisper
{
	namespace
	{
		const char* const Moments[] = { "radio", "wavering", "hidden", "cutters", "smoke", "guns" };

		bool Known(const std::string& When)
		{
			for (const char* M : Moments)
			{
				if (When == M)
				{
					return true;
				}
			}
			return false;
		}
	}

	bool ReadWords(const std::string& Json, Words& Out, std::string& OutWhy)
	{
		LandJson::Json Root;
		if (!LandJson::Parse(Json, Root, OutWhy))
		{
			return false;
		}
		Words Read;
		Read.Label = Root["label"].Str("CHIEF OF STAFF");
		const LandJson::Json& List = Root["lines"];
		for (size_t K = 0; K < List.Count(); ++K)
		{
			const LandJson::Json& J = List.At(K);
			Line L;
			L.Id = J["id"].Str("");
			L.When = J["when"].Str("");
			L.Scope = J["scope"].Str("once");
			L.Text = J["text"].Str("");
			if (L.Id.empty() || L.Text.empty() || !Known(L.When) || (L.Scope != "once" && L.Scope != "each"))
			{
				OutWhy = "line " + std::to_string(K + 1) + " lacks its id or words, or names a moment or scope this desk does not know";
				return false;
			}
			for (const Line& Before : Read.Lines)
			{
				if (Before.Id == L.Id)
				{
					OutWhy = "two lines named " + L.Id;
					return false;
				}
			}
			Read.Lines.push_back(L);
		}
		if (Read.Lines.empty())
		{
			OutWhy = "no lines";
			return false;
		}
		Out = Read;
		return true;
	}

	bool Holds(const Line& L, const Facts& F)
	{
		if (F.Block < 0)
		{
			return false;
		}
		if (L.When == "radio")
		{
			// (an order on the air: the rules' orderAt less the battle's clock, LandFeed's orderIn)
			return F.OrderIn > 0.5;
		}
		if (L.When == "wavering")
		{
			return F.State == "wavering";
		}
		if (L.When == "hidden")
		{
			// (a sniper team fires from hiding and is found only by odds: not this line's truth)
			return F.bHidden && F.Team != "snp";
		}
		if (L.When == "cutters")
		{
			// (the orders bar's keys wait for the clock)
			return F.bCutter && !F.bDeploying;
		}
		if (L.When == "smoke")
		{
			return F.Smoke > 0 && !F.bDeploying;
		}
		if (L.When == "guns")
		{
			return F.bAllGuns && !F.bDeploying;
		}
		return false;
	}

	std::string Said(const Line& L, const Facts& F)
	{
		std::string Text = L.Text;
		const size_t At = Text.find("{s}");
		if (At != std::string::npos)
		{
			const long Seconds = std::lround(std::max(1.0, F.OrderIn));
			Text.replace(At, 3, std::to_string(Seconds));
		}
		return Text;
	}

	void Desk::Begin(const Words& InWords, bool bInOn)
	{
		W = InWords;
		bOn = bInOn && !W.Lines.empty();
		bQuiet = false;
		Shown = Up();
		HandBlock = -1;
		HandSince = 0.0;
		GoneAt = -1.0e9;
		Last = 0.0;
		Done.clear();
	}

	void Desk::Quiet()
	{
		bQuiet = true;
		Shown = Up();
	}

	const Line* Desk::Current() const
	{
		return Shown.bShown && Shown.Index >= 0 && Shown.Index < static_cast<int>(W.Lines.size()) ? &W.Lines[static_cast<size_t>(Shown.Index)] : nullptr;
	}

	bool Desk::Said(int Index, int Block) const
	{
		const bool bEach = W.Lines[static_cast<size_t>(Index)].Scope == "each";
		for (const std::pair<int, int>& D : Done)
		{
			if (D.first == Index && (!bEach || D.second == Block))
			{
				return true;
			}
		}
		return false;
	}

	bool Desk::Tick(const Facts& F)
	{
		const double Dt = std::max(0.0, F.Real - Last);
		Last = F.Real;
		if (!Showing())
		{
			return false;
		}
		if (F.Block != HandBlock)
		{
			HandBlock = F.Block;
			HandSince = F.Real;
		}
		const bool bQuietNow = !F.bGeneral || F.bBusy || F.bNoteUp;
		if (Shown.bShown)
		{
			// (up: it stays its time, hidden time not counted; it goes at once where its unit has left his hand)
			if (!bQuietNow)
			{
				Shown.Age += Dt;
			}
			if (Shown.Age >= Stays || F.Block != Shown.Block)
			{
				Shown = Up();
				GoneAt = F.Real;
				return true;
			}
			return false;
		}
		if (bQuietNow || F.Block < 0 || F.Real - HandSince < Settle || F.Real - GoneAt < Between)
		{
			return false;
		}
		for (size_t K = 0; K < W.Lines.size(); ++K)
		{
			const int Index = static_cast<int>(K);
			if (Holds(W.Lines[K], F) && !Said(Index, F.Block))
			{
				Done.emplace_back(Index, W.Lines[K].Scope == "each" ? F.Block : -1);
				Shown.bShown = true;
				Shown.Index = Index;
				Shown.Block = F.Block;
				Shown.Age = 0.0;
				Shown.Text = StaffWhisper::Said(W.Lines[K], F);
				return true;
			}
		}
		return false;
	}
}
