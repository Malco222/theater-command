#include "Battle/FieldNotes.h"

#include "Battle/LandJson.h"

#include <algorithm>
#include <cmath>

namespace FieldNotes
{
	namespace
	{
		constexpr double Turn = 6.283185307179586;
		/** The closing note has no deed: it folds itself away after this long if he never clicks it. */
		constexpr double ClosingStays = 25.0;

		bool Has(const std::vector<std::string>& List, const std::string& Id)
		{
			return std::find(List.begin(), List.end(), Id) != List.end();
		}

		void Add(std::vector<std::string>& List, const std::string& Id)
		{
			if (!Id.empty() && !Has(List, Id))
			{
				List.push_back(Id);
			}
		}

		std::string Quoted(const std::string& Text)
		{
			std::string Out = "\"";
			for (const char C : Text)
			{
				if (C == '"' || C == '\\')
				{
					Out += '\\';
					Out += C;
				}
				else if (static_cast<unsigned char>(C) < 0x20)
				{
					Out += ' ';
				}
				else
				{
					Out += C;
				}
			}
			return Out + "\"";
		}

		std::string Listed(const std::vector<std::string>& List)
		{
			std::string Out = "[";
			for (size_t K = 0; K < List.size(); ++K)
			{
				Out += (K ? "," : "") + Quoted(List[K]);
			}
			return Out + "]";
		}

		std::vector<std::string> Strings(const LandJson::Json& Array)
		{
			std::vector<std::string> Out;
			for (size_t K = 0; K < Array.Count(); ++K)
			{
				if (Array.At(K).IsString())
				{
					Add(Out, Array.At(K).Str());
				}
			}
			return Out;
		}

		/** Radians between two headings, the short way round. */
		double Apart(double A, double B)
		{
			double D = std::fmod(std::fabs(A - B), Turn);
			return std::min(D, Turn - D);
		}
	}

	int CountWords(const std::string& Text)
	{
		int Count = 0;
		bool bIn = false;
		for (const char C : Text)
		{
			const bool bSpace = C == ' ' || C == '\t' || C == '\n';
			if (!bSpace && !bIn)
			{
				++Count;
			}
			bIn = !bSpace;
		}
		return Count;
	}

	bool ReadWords(const std::string& Json, Words& Out, std::string& OutWhy)
	{
		LandJson::Json Root;
		if (!LandJson::Parse(Json, Root, OutWhy))
		{
			return false;
		}
		Words Read;
		Read.Label = Root["label"].Str("STAFF NOTE");
		const LandJson::Json& List = Root["notes"];
		for (size_t K = 0; K < List.Count(); ++K)
		{
			const LandJson::Json& J = List.At(K);
			Note N;
			N.Id = J["id"].Str("");
			N.When = J["when"].Str("any");
			N.Aim = J["aim"].Str("none");
			N.Heading = J["heading"].Str("");
			N.Text = J["text"].Str("");
			N.Deed = J["deed"].Str("");
			if (N.Id.empty() || N.Heading.empty() || N.Text.empty() || N.Deed.empty())
			{
				OutWhy = "note " + std::to_string(K + 1) + " lacks its id, heading, text or deed";
				return false;
			}
			for (const Note& Before : Read.Notes)
			{
				if (Before.Id == N.Id)
				{
					OutWhy = "two notes named " + N.Id;
					return false;
				}
			}
			Read.Notes.push_back(N);
		}
		if (Read.Notes.empty())
		{
			OutWhy = "no notes";
			return false;
		}
		const LandJson::Json& Closing = Root["closing"];
		Read.Closing.Id = Closing["id"].Str("yours");
		Read.Closing.Heading = Closing["heading"].Str("");
		Read.Closing.Text = Closing["text"].Str("");
		const LandJson::Json& Tips = Root["tips"];
		for (size_t K = 0; K < Tips.Count(); ++K)
		{
			if (Tips.At(K)["id"].Str("") == "fight")
			{
				Read.FightTip = Tips.At(K)["text"].Str("");
			}
		}
		Read.OffLine = Root["off"].Str("");
		Out = Read;
		return true;
	}

	bool Book::IsDone(const std::string& Id) const { return Has(Done, Id); }
	bool Book::HasSeen(const std::string& Id) const { return Has(Seen, Id); }
	void Book::MarkDone(const std::string& Id) { Add(Done, Id); }
	void Book::MarkSeen(const std::string& Id) { Add(Seen, Id); }

	bool Book::AllDone(const Words& W) const
	{
		for (const Note& N : W.Notes)
		{
			if (!IsDone(N.Id))
			{
				return false;
			}
		}
		return true;
	}

	void Book::ArmAgain(const Words& W)
	{
		bOn = true;
		if (AllDone(W))
		{
			Done.clear();
			bClosed = false;
		}
	}

	Book ReadBook(const std::string& Json)
	{
		Book B;
		LandJson::Json Root;
		std::string Why;
		if (Json.empty() || !LandJson::Parse(Json, Root, Why) || !Root.IsObject())
		{
			return B;
		}
		B.bOn = Root["on"].Truth(true);
		B.Done = Strings(Root["done"]);
		B.Seen = Strings(Root["seen"]);
		B.bClosed = Root["closed"].Truth(false);
		return B;
	}

	std::string WriteBook(const Book& B)
	{
		return std::string("{\"on\":") + (B.bOn ? "true" : "false") + ",\"done\":" + Listed(B.Done) + ",\"seen\":" + Listed(B.Seen)
			+ ",\"closed\":" + (B.bClosed ? "true" : "false") + "}\n";
	}

	// ---- The desk ----

	void Desk::Begin(const Words& InWords, const Book& InBook, bool bInShow)
	{
		W = InWords;
		B = InBook;
		bShow = bInShow;
		Shown = Up();
		At = Last = Facts();
		ShownAt = StampAt = QuietTill = HiddenFor = 0.0;
		bHidden = false;
		LaterNow.clear();
	}

	const Note* Desk::Current() const
	{
		if (!Shown.bShown)
		{
			return nullptr;
		}
		if (Shown.Index < 0)
		{
			return &W.Closing;
		}
		return Shown.Index < static_cast<int>(W.Notes.size()) ? &W.Notes[static_cast<size_t>(Shown.Index)] : nullptr;
	}

	bool Desk::Eligible(const Note& N, const Facts& F) const
	{
		if (N.When == "setup")
		{
			return F.bDeploying;
		}
		if (N.When == "fight")
		{
			return !F.bDeploying;
		}
		return true;
	}

	bool Desk::DeedDone(const Note& N, const Facts& F) const
	{
		if (N.Id == "ground")
		{
			return F.Placed > At.Placed || (At.bDeploying && !F.bDeploying);
		}
		if (N.Id == "look")
		{
			const bool bMoved = std::hypot(F.LookX - At.LookX, F.LookY - At.LookY) >= LookMetres;
			const bool bTurned = Apart(F.Heading, At.Heading) >= LookTurn;
			const bool bZoomed = At.Zoom > 0.0 && F.Zoom > 0.0 && std::fabs(std::log(F.Zoom / At.Zoom)) >= LookZoom;
			return bMoved || bTurned || bZoomed || F.bTopView != At.bTopView;
		}
		if (N.Id == "pick")
		{
			return F.InHand > 0;
		}
		if (N.Id == "order")
		{
			return F.Orders > At.Orders;
		}
		if (N.Id == "clock")
		{
			return F.SpeedNow <= 0.0 || (!F.bOwnSpeed && (At.bOwnSpeed || F.Speed != At.Speed));
		}
		if (N.Id == "decides")
		{
			return F.Hover >= 0;
		}
		if (N.Id == "break")
		{
			return F.Battle - At.Battle >= 60.0;
		}
		// (a note the words add with no deed known here waits for LATER)
		return false;
	}

	void Desk::Raise(int Index, const Facts& F)
	{
		Shown = Up();
		Shown.bShown = true;
		Shown.Index = Index;
		Shown.Number = Index >= 0 ? Index + 1 : 0;
		Shown.Of = Index >= 0 ? static_cast<int>(W.Notes.size()) : 0;
		At = F;
		ShownAt = F.Real;
		HiddenFor = 0.0;
		bHidden = false;
	}

	void Desk::Lower(double Real)
	{
		Shown = Up();
		QuietTill = Real + Between;
		bHidden = false;
		HiddenFor = 0.0;
	}

	bool Desk::Tick(const Facts& F)
	{
		const double Was = Last.Real;
		Last = F;
		if (!Showing())
		{
			Shown = Up();
			return false;
		}
		const bool bHide = !F.bGeneral || F.bBusy;
		if (Shown.bShown)
		{
			if (bHide)
			{
				// (held out of sight: the after card, a question, the film; the time it is hidden is not reading time)
				if (bHidden)
				{
					HiddenFor += std::max(0.0, F.Real - Was);
				}
				bHidden = true;
				return false;
			}
			bHidden = false;
			Shown.Age = F.Real - ShownAt - HiddenFor;
			if (Shown.bStamped)
			{
				Shown.StampAge = F.Real - StampAt;
				if (Shown.StampAge >= StampStays)
				{
					Lower(F.Real);
				}
				return false;
			}
			if (Shown.Index < 0)
			{
				if (Shown.Age >= ClosingStays)
				{
					Lower(F.Real);
				}
				return false;
			}
			const Note& N = W.Notes[static_cast<size_t>(Shown.Index)];
			if (Shown.Age >= LeastUp && DeedDone(N, F))
			{
				Shown.bStamped = true;
				Shown.StampAge = 0.0;
				StampAt = F.Real;
				B.MarkDone(N.Id);
				return true;
			}
			return false;
		}
		if (bHide || F.Real < QuietTill)
		{
			return false;
		}
		for (size_t K = 0; K < W.Notes.size(); ++K)
		{
			const Note& N = W.Notes[K];
			if (!B.IsDone(N.Id) && !Has(LaterNow, N.Id) && Eligible(N, F))
			{
				Raise(static_cast<int>(K), F);
				return false;
			}
		}
		if (B.AllDone(W) && !B.bClosed && !W.Closing.Heading.empty())
		{
			Raise(-1, F);
			B.bClosed = true;
			return true;
		}
		return false;
	}

	void Desk::Later()
	{
		if (!Shown.bShown)
		{
			return;
		}
		if (Shown.Index >= 0 && !Shown.bStamped)
		{
			Add(LaterNow, W.Notes[static_cast<size_t>(Shown.Index)].Id);
		}
		Lower(Last.Real);
	}

	bool Desk::Off()
	{
		Shown = Up();
		const bool bWas = B.bOn;
		B.bOn = false;
		return bWas;
	}

	void Desk::Close()
	{
		if (Shown.bShown && (Shown.bStamped || Shown.Index < 0))
		{
			Lower(Last.Real);
		}
	}
}
