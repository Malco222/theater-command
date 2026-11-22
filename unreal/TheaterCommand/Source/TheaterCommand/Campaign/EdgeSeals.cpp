#include "Campaign/EdgeSeals.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace EdgeSeals
{
	namespace
	{
		bool IsStop(char Letter)
		{
			return Letter == ',' || Letter == ';' || Letter == ':';
		}

		bool IsSpace(char Letter)
		{
			return Letter == ' ' || Letter == '\t' || Letter == '\n' || Letter == '\r';
		}

		/** A word that is only a dash (-, the en dash or the em dash in UTF-8): no word to end a cut line on. */
		bool IsDashWord(const std::string& Text, size_t Start, size_t End)
		{
			const std::string Word = Text.substr(Start, End - Start);
			return Word == "-" || Word == "--" || Word == "\xE2\x80\x93" || Word == "\xE2\x80\x94";
		}

		double ClampTo(double Value, double Low, double High)
		{
			return Value < Low ? Low : Value > High ? High : Value;
		}
	}

	bool FBox::Overlaps(const FBox& Other) const
	{
		return MinX < Other.MaxX && Other.MinX < MaxX && MinY < Other.MaxY && Other.MinY < MaxY;
	}

	FBox FBox::Grown(double By) const
	{
		return FBox{ MinX - By, MinY - By, MaxX + By, MaxY + By };
	}

	void ListItems(const FCardCounts& Counts, std::vector<FItem>& Out)
	{
		Out.clear();
		auto Add = [&Out](ESection Section, int Count)
		{
			for (int Index = 0; Index < Count; ++Index)
			{
				Out.push_back(FItem{ Section, Index });
			}
		};
		Add(ESection::Band, Counts.Bands);
		Add(ESection::Lead, Counts.bLead ? 1 : 0);
		Add(ESection::Row, Counts.Rows);
		Add(ESection::Home, Counts.Home);
		Add(ESection::Staff, Counts.Staff);
		Add(ESection::Elsewhere, Counts.Elsewhere);
	}

	FFold Fold(int Count, int Most)
	{
		const int Room = std::max(Most, 1);
		if (Count <= Room)
		{
			return FFold{ std::max(Count, 0), 0 };
		}
		return FFold{ Room - 1, Count - (Room - 1) };
	}

	int ClassOf(ESection Section, const std::string& Word)
	{
		if (Section == ESection::Band)
		{
			return Word == "WARNING" ? 1 : 2;
		}
		if (Section == ESection::Lead)
		{
			// (the month a stake falls due, its warning is the lead story: ruling 118(f))
			return Word == "LAST MONTH" ? 2 : 3;
		}
		if (Section == ESection::Row)
		{
			// (a band's item that found no band keeps its band's word as its tag, and sits above the first battle row)
			if (Word == "WARNING")
			{
				return 1;
			}
			return Word == "WAR" || Word == "FALLEN" || Word == "FREED" || Word == "NEWS" ? 2 : 3;
		}
		if (Section == ESection::Home || Section == ESection::Staff)
		{
			return 6;
		}
		return Section == ESection::Elsewhere ? 7 : 8;
	}

	FLook LookOf(ESection Section, const std::string& Word, int Tone)
	{
		const int Class = ClassOf(Section, Word);
		// (a dated warning: act by its date or lose something. The lead of the month a stake falls due is that warning,
		// and the card frames it in the warning's red: ruling 118(f))
		if (Class == 1 || (Section == ESection::Lead && Word == "LAST MONTH"))
		{
			return FLook{ EWax::Red, true };
		}
		// (news of war and peace arrives: the brass band's colours, or the card's green for a freeing his own side did)
		if (Class == 2)
		{
			return FLook{ Tone == 1 ? EWax::Green : EWax::Band, true };
		}
		if (Section == ESection::Elsewhere)
		{
			return FLook{ EWax::Grey, false };
		}
		if (Section == ESection::More)
		{
			return FLook{ EWax::Brass, false };
		}
		// (a front's row of plain news: the card edges it in its muted grey, MonthRowOf's A89F8D, and so it is grey)
		if (Section == ESection::Row && Tone == 0)
		{
			return FLook{ EWax::Grey, false };
		}
		// (bad news is no warning: it wears plain brass, never red)
		return FLook{ Tone == 1 ? EWax::Green : Tone == 2 ? EWax::Amber : Tone == 3 ? EWax::Grey : EWax::Brass, false };
	}

	bool MayPress(const std::string& Sign)
	{
		return !Sign.empty() && Sign != "star" && Sign != "night" && Sign != "day";
	}

	EGlyph GlyphOf(ESection Section, const std::string& Word, bool bSign)
	{
		if (Section == ESection::More)
		{
			return EGlyph::More;
		}
		if (bSign)
		{
			return EGlyph::Sign;
		}
		if (Word == "WARNING" || Word == "LAST MONTH")
		{
			return EGlyph::Warning;
		}
		if (Word == "WAR")
		{
			return EGlyph::War;
		}
		if (Word == "FALLEN")
		{
			return EGlyph::Fallen;
		}
		if (Word == "FREED")
		{
			return EGlyph::Freed;
		}
		return Section == ESection::Elsewhere ? EGlyph::Globe : EGlyph::News;
	}

	std::string LeadSign(const std::string& Picture)
	{
		if (Picture == "troops" || Picture == "fieldworks" || Picture == "town" || Picture == "rail-yard")
		{
			return "inf";
		}
		if (Picture == "fleet" || Picture == "harbour")
		{
			return "hull";
		}
		if (Picture == "airfield")
		{
			return "ftr";
		}
		return std::string();
	}

	std::string RowLine(const std::string& Place, const std::string& Tag, const std::string& Join, const std::string& JoinName, const std::string& Text)
	{
		if (Tag.empty())
		{
			return Text.empty() ? Place : Text;
		}
		std::string Line = Place;
		Line += Line.empty() ? "" : " ";
		Line += Tag;
		if (!Join.empty() && !JoinName.empty())
		{
			Line += " " + Join + " " + JoinName;
		}
		else if (!Text.empty())
		{
			// (with no other side named the head alone says nothing of what happened: the card's sentence under it does)
			Line += ": " + Text;
		}
		return Line;
	}

	std::string EightWords(const std::string& Text, int Most)
	{
		// The words, as where each starts and ends.
		std::vector<std::pair<size_t, size_t>> Words;
		for (size_t At = 0; At < Text.size();)
		{
			while (At < Text.size() && IsSpace(Text[At]))
			{
				++At;
			}
			const size_t Start = At;
			while (At < Text.size() && !IsSpace(Text[At]))
			{
				++At;
			}
			if (At > Start)
			{
				Words.emplace_back(Start, At);
			}
		}
		if (Words.empty())
		{
			return std::string();
		}
		const int Room = std::max(Most, 1);
		size_t Kept = Words.size();
		if (static_cast<int>(Words.size()) > Room)
		{
			Kept = static_cast<size_t>(Room);
			// (at a stop, where one falls late enough to leave at least half of the words)
			for (size_t Count = static_cast<size_t>(Room); Count >= static_cast<size_t>((Room + 1) / 2) && Count >= 1; --Count)
			{
				if (IsStop(Text[Words[Count - 1].second - 1]))
				{
					Kept = Count;
					break;
				}
			}
			// (a lone dash is no word to end on)
			while (Kept > 1 && IsDashWord(Text, Words[Kept - 1].first, Words[Kept - 1].second))
			{
				--Kept;
			}
		}
		std::string Out = Text.substr(Words[0].first, Words[Kept - 1].second - Words[0].first);
		if (Kept < Words.size())
		{
			while (!Out.empty() && IsStop(Out.back()))
			{
				Out.pop_back();
			}
		}
		return Out;
	}

	FAlike AlikeOf(ESection Section, const std::string& Word, int Tone, const std::string& Sign)
	{
		const FLook Worn = LookOf(Section, Word, Tone);
		FAlike Alike;
		Alike.Wax = Worn.Wax;
		Alike.bPulse = Worn.bPulse;
		Alike.Glyph = GlyphOf(Section, Word, MayPress(Sign));
		if (Alike.Glyph == EGlyph::Sign)
		{
			Alike.Sign = Sign;
		}
		return Alike;
	}

	void FoldAlike(const std::vector<FAlike>& Looks, std::vector<FAlikeRun>& Out, int Least)
	{
		Out.clear();
		const int Count = static_cast<int>(Looks.size());
		const int Fewest = std::max(Least, 2);
		for (int First = 0; First < Count;)
		{
			int End = First + 1;
			// (a dated warning stands alone: it never folds, and as it looks like nothing else, no run goes past it)
			if (!Looks[static_cast<size_t>(First)].IsWarning())
			{
				while (End < Count && Looks[static_cast<size_t>(End)] == Looks[static_cast<size_t>(First)])
				{
					++End;
				}
			}
			if (End - First >= Fewest)
			{
				Out.push_back(FAlikeRun{ First, End - First });
			}
			else
			{
				for (int Each = First; Each < End; ++Each)
				{
					Out.push_back(FAlikeRun{ Each, 1 });
				}
			}
			First = End;
		}
	}

	namespace
	{
		void SplitWords(const std::string& Text, std::vector<std::string>& Out)
		{
			Out.clear();
			for (size_t At = 0; At < Text.size();)
			{
				while (At < Text.size() && IsSpace(Text[At]))
				{
					++At;
				}
				const size_t Start = At;
				while (At < Text.size() && !IsSpace(Text[At]))
				{
					++At;
				}
				if (At > Start)
				{
					Out.push_back(Text.substr(Start, At - Start));
				}
			}
			// (the sentence's own full stop is no part of what it says of each)
			while (!Out.empty())
			{
				std::string& Last = Out.back();
				while (!Last.empty() && (Last.back() == '.' || Last.back() == '!' || Last.back() == '?'))
				{
					Last.pop_back();
				}
				if (!Last.empty())
				{
					break;
				}
				Out.pop_back();
			}
		}

		std::string JoinWords(const std::vector<std::string>& Words, size_t From, size_t To)
		{
			std::string Out;
			for (size_t Index = From; Index < To; ++Index)
			{
				Out += Index > From ? " " : "";
				Out += Words[Index];
			}
			return Out;
		}

		/** The rules' listNames: two as "A and B", three or four whole, more as the first three "and N more". */
		std::string ListNames(const std::vector<std::string>& Names)
		{
			const size_t Count = Names.size();
			if (Count <= 2)
			{
				return Count == 0 ? std::string() : Count == 1 ? Names[0] : Names[0] + " and " + Names[1];
			}
			std::string Out;
			const size_t Whole = Count > 4 ? 3 : Count - 1;
			for (size_t Index = 0; Index < Whole; ++Index)
			{
				Out += Index > 0 ? ", " : "";
				Out += Names[Index];
			}
			return Count > 4 ? Out + " and " + std::to_string(Count - 3) + " more" : Out + " and " + Names[Count - 1];
		}

		bool StartsLower(const std::string& Word)
		{
			return !Word.empty() && Word[0] >= 'a' && Word[0] <= 'z';
		}

		bool EndsStopped(const std::string& Word)
		{
			return !Word.empty() && (IsStop(Word.back()) || Word.back() == '.');
		}

		bool EndsWith(const std::string& Word, const char* Tail)
		{
			const std::string Wanted(Tail);
			return Word.size() >= Wanted.size() && Word.compare(Word.size() - Wanted.size(), Wanted.size(), Wanted) == 0;
		}

		/**
		 * The sentences of a run as a name each and the same words after it (the predicate): where each sentence is one
		 * to four words with no stop in them, none named twice, and then the same words, the first of them in small
		 * letters (a verb, never another name). Out: the names, "The" made "the" for the list.
		 */
		bool SplitNames(const std::vector<FSaid>& Said, const FAlikeRun& Run, std::vector<std::string>& OutNames, std::string& OutSays)
		{
			std::vector<std::string> First, Each;
			SplitWords(Said[static_cast<size_t>(Run.First)].Sentence, First);
			for (size_t Lead = 1; Lead <= 4 && Lead < First.size(); ++Lead)
			{
				if (!StartsLower(First[Lead]) || EndsStopped(First[Lead - 1]))
				{
					continue;
				}
				const size_t Says = First.size() - Lead;
				bool bAll = true;
				OutNames.clear();
				for (int Index = Run.First; bAll && Index < Run.First + Run.Count; ++Index)
				{
					SplitWords(Said[static_cast<size_t>(Index)].Sentence, Each);
					const size_t Named = Each.size() > Says ? Each.size() - Says : 0;
					bAll = Named >= 1 && Named <= 4;
					for (size_t Word = 0; bAll && Word < Each.size(); ++Word)
					{
						bAll = Word < Named ? !EndsStopped(Each[Word]) : Each[Word] == First[Lead + Word - Named];
					}
					if (bAll)
					{
						std::string Name = JoinWords(Each, 0, Named);
						if (Name.compare(0, 4, "The ") == 0)
						{
							Name[0] = 't';
						}
						bAll = std::find(OutNames.begin(), OutNames.end(), Name) == OutNames.end();
						OutNames.push_back(Name);
					}
				}
				if (bAll)
				{
					// (the verb made plural, and an "is", "has" or "was" after an "and" with it: "report for duty and are waiting")
					std::vector<std::string> Words(First.begin() + static_cast<std::ptrdiff_t>(Lead), First.end());
					Words[0] = PluralVerb(Words[0]);
					for (size_t Word = 1; Word < Words.size(); ++Word)
					{
						if (Words[Word - 1] == "and" && (Words[Word] == "is" || Words[Word] == "has" || Words[Word] == "was"))
						{
							Words[Word] = PluralVerb(Words[Word]);
						}
					}
					OutSays = JoinWords(Words, 0, Words.size());
					return true;
				}
			}
			return false;
		}
	}

	FSaid RowSaid(const std::string& Place, const std::string& Tag, const std::string& Join, const std::string& JoinName, const std::string& Text)
	{
		FSaid Said;
		Said.Section = ESection::Row;
		Said.Line = RowLine(Place, Tag, Join, JoinName, Text);
		Said.Sentence = Text.empty() ? Said.Line : Text;
		Said.Name = Place;
		if (!Tag.empty())
		{
			Said.Head = !Join.empty() && !JoinName.empty() ? Tag + " " + Join + " " + JoinName : Tag;
		}
		return Said;
	}

	FSaid PlainSaid(ESection Section, const std::string& Text)
	{
		FSaid Said;
		Said.Section = Section;
		Said.Line = Text;
		Said.Sentence = Text;
		return Said;
	}

	std::string PluralVerb(const std::string& Word)
	{
		if (Word == "is")
		{
			return "are";
		}
		if (Word == "has")
		{
			return "have";
		}
		if (Word == "was")
		{
			return "were";
		}
		if (Word == "does" || Word == "goes")
		{
			return Word.substr(0, Word.size() - 2);
		}
		if (!StartsLower(Word) || Word.size() < 3 || Word.back() != 's' || EndsWith(Word, "ss") || EndsWith(Word, "us") || EndsWith(Word, "is"))
		{
			return Word;
		}
		// (carries carry, but dies die and lies lie)
		if (EndsWith(Word, "ies"))
		{
			return Word.size() > 4 ? Word.substr(0, Word.size() - 3) + "y" : Word.substr(0, Word.size() - 1);
		}
		// (presses press, pushes push, reaches reach, taxes tax, buzzes buzz; but closes close, seizes seize)
		if (EndsWith(Word, "sses") || EndsWith(Word, "shes") || EndsWith(Word, "ches") || EndsWith(Word, "xes") || EndsWith(Word, "zzes"))
		{
			return Word.substr(0, Word.size() - 2);
		}
		return Word.substr(0, Word.size() - 1);
	}

	std::string FoldWords(const std::vector<FSaid>& Said, const FAlikeRun& Run)
	{
		if (Run.First < 0 || Run.Count < 1 || static_cast<size_t>(Run.First + Run.Count) > Said.size())
		{
			return std::string();
		}
		const FSaid& Head = Said[static_cast<size_t>(Run.First)];
		if (Run.Count == 1)
		{
			return EightWords(Head.Line);
		}
		const std::string Count = std::to_string(Run.Count);
		std::vector<std::string> Names;
		std::string Says;
		if (SplitNames(Said, Run, Names, Says))
		{
			return Count + " " + Says + ": " + ListNames(Names);
		}
		// (a front's rows that share their head: its place on each, and the head as the card prints it)
		bool bRows = !Head.Head.empty();
		Names.clear();
		for (int Index = Run.First; bRows && Index < Run.First + Run.Count; ++Index)
		{
			const FSaid& Row = Said[static_cast<size_t>(Index)];
			bRows = Row.Section == ESection::Row && Row.Head == Head.Head && !Row.Name.empty() && std::find(Names.begin(), Names.end(), Row.Name) == Names.end();
			Names.push_back(Row.Name);
		}
		if (bRows)
		{
			return Count + " " + Head.Head + ": " + ListNames(Names);
		}
		// (else the first item's own words, how many more, and the part of the card they are in, where they share one)
		bool bOnePart = true;
		for (int Index = Run.First; Index < Run.First + Run.Count; ++Index)
		{
			bOnePart = bOnePart && Said[static_cast<size_t>(Index)].Section == Head.Section;
		}
		const char* Where = "";
		if (bOnePart)
		{
			Where = Head.Section == ESection::Row ? " on your fronts" : Head.Section == ESection::Home ? " at home" : Head.Section == ESection::Staff ? " from your staff" : Head.Section == ESection::Elsewhere ? " elsewhere in the war" : "";
		}
		std::string First = EightWords(Head.Line);
		while (!First.empty() && (IsStop(First.back()) || First.back() == '.' || First.back() == '!' || First.back() == '?'))
		{
			First.pop_back(); // (its full stop: the words go on)
		}
		return First + ", and " + std::to_string(Run.Count - 1) + " more" + Where;
	}

	FLane LaneOf(const FParams& Params, EEdge Edge)
	{
		FLane Lane;
		Lane.bAlongX = Edge == EEdge::Top || Edge == EEdge::Bottom;
		const double HalfW = Params.SealW * 0.5, HalfH = Params.SealH * 0.5;
		if (Lane.bAlongX)
		{
			Lane.Fixed = Edge == EEdge::Top ? Params.Margin + HalfH : Params.ScreenH - Params.Margin - HalfH;
			Lane.Low = Params.Margin + HalfW;
			Lane.High = Params.ScreenW - Params.Margin - HalfW;
		}
		else
		{
			Lane.Fixed = Edge == EEdge::Left ? Params.Margin + HalfW : Params.ScreenW - Params.Margin - HalfW;
			Lane.Low = Params.Margin + HalfH;
			Lane.High = Params.ScreenH - Params.Margin - HalfH;
		}
		return Lane;
	}

	double DistanceToLane(const FLane& Lane, const FPoint& At, double& OutAlong)
	{
		const double Along = Lane.bAlongX ? At.X : At.Y, Across = Lane.bAlongX ? At.Y : At.X;
		OutAlong = Lane.High >= Lane.Low ? ClampTo(Along, Lane.Low, Lane.High) : (Lane.Low + Lane.High) * 0.5;
		return std::hypot(Along - OutAlong, Across - Lane.Fixed);
	}

	FPoint FromOffScreen(const FParams& Params, EEdge Edge, const FPoint& Centre)
	{
		const double OutX = Params.SealW * 0.5 + Params.Margin, OutY = Params.SealH * 0.5 + Params.Margin;
		if (Edge == EEdge::Top)
		{
			return FPoint{ Centre.X, -OutY };
		}
		if (Edge == EEdge::Bottom)
		{
			return FPoint{ Centre.X, Params.ScreenH + OutY };
		}
		if (Edge == EEdge::Left)
		{
			return FPoint{ -OutX, Centre.Y };
		}
		return FPoint{ Params.ScreenW + OutX, Centre.Y };
	}

	bool FLayout::FreeNearest(const FParams& Params, EEdge Edge, double Wanted, double HalfAlong, double& OutAt)
	{
		const FLane Lane = LaneOf(Params, Edge);
		const double HalfAcross = Lane.bAlongX ? Params.SealH * 0.5 : Params.SealW * 0.5;
		const double Length = Lane.bAlongX ? Params.ScreenW : Params.ScreenH;
		const double Low = Params.Margin + HalfAlong, High = Length - Params.Margin - HalfAlong;
		if (High < Low)
		{
			return false;
		}
		// Where along the lane the box's middle may not stand: every obstacle that reaches across the lane, widened by
		// half the box. (Open stretches: a box may touch one, never go into it.)
		Blocked.clear();
		for (const FBox& Each : Obstacles)
		{
			const double AcrossMin = Lane.bAlongX ? Each.MinY : Each.MinX, AcrossMax = Lane.bAlongX ? Each.MaxY : Each.MaxX;
			if (!(Lane.Fixed - HalfAcross < AcrossMax && AcrossMin < Lane.Fixed + HalfAcross))
			{
				continue;
			}
			const double AlongMin = Lane.bAlongX ? Each.MinX : Each.MinY, AlongMax = Lane.bAlongX ? Each.MaxX : Each.MaxY;
			Blocked.emplace_back(AlongMin - HalfAlong, AlongMax + HalfAlong);
		}
		std::sort(Blocked.begin(), Blocked.end());
		size_t Merged = 0;
		for (size_t Index = 0; Index < Blocked.size(); ++Index)
		{
			if (Merged > 0 && Blocked[Index].first < Blocked[Merged - 1].second)
			{
				Blocked[Merged - 1].second = std::max(Blocked[Merged - 1].second, Blocked[Index].second);
			}
			else
			{
				Blocked[Merged++] = Blocked[Index];
			}
		}
		Blocked.resize(Merged);
		const double Want = ClampTo(Wanted, Low, High);
		auto IsFree = [this, Low, High](double At)
		{
			if (At < Low || At > High)
			{
				return false;
			}
			for (const std::pair<double, double>& Stretch : Blocked)
			{
				if (Stretch.first < At && At < Stretch.second)
				{
					return false;
				}
			}
			return true;
		};
		if (IsFree(Want))
		{
			OutAt = Want;
			return true;
		}
		bool bFound = false;
		double Best = 0.0, BestOff = 0.0;
		auto Try = [&IsFree, &bFound, &Best, &BestOff, Want](double At)
		{
			const double Off = std::fabs(At - Want);
			if (IsFree(At) && (!bFound || Off < BestOff))
			{
				bFound = true;
				Best = At;
				BestOff = Off;
			}
		};
		Try(Low);
		Try(High);
		for (size_t Index = 0; Index < Blocked.size(); ++Index)
		{
			Try(Blocked[Index].first);
			Try(Blocked[Index].second);
		}
		if (bFound)
		{
			OutAt = Best;
		}
		return bFound;
	}

	void FLayout::Claim(const FParams& Params, const FSpot& Spot)
	{
		const double HalfW = Params.SealW * 0.5, HalfH = Params.SealH * 0.5;
		Obstacles.push_back(FBox{ Spot.Centre.X - HalfW, Spot.Centre.Y - HalfH, Spot.Centre.X + HalfW, Spot.Centre.Y + HalfH }.Grown(Params.Gap));
	}

	bool FLayout::PlaceOne(const FParams& Params, const EEdge* Edges, int EdgeCount, double Along[4], FSpot& Out)
	{
		for (int Index = 0; Index < EdgeCount; ++Index)
		{
			const EEdge Edge = Edges[Index];
			const FLane Lane = LaneOf(Params, Edge);
			double At = 0.0;
			if (FreeNearest(Params, Edge, Along[static_cast<int>(Edge)], Lane.bAlongX ? Params.SealW * 0.5 : Params.SealH * 0.5, At))
			{
				Out.bShown = true;
				Out.Edge = Edge;
				Out.Centre = Lane.bAlongX ? FPoint{ At, Lane.Fixed } : FPoint{ Lane.Fixed, At };
				Out.From = FromOffScreen(Params, Edge, Out.Centre);
				Claim(Params, Out);
				return true;
			}
		}
		Out = FSpot();
		return false;
	}

	void FLayout::Lay(const FParams& Params, const std::vector<FWant>& Wants, const std::vector<FBox>& Reserved, std::vector<FSpot>& Out)
	{
		Out.assign(Wants.size(), FSpot());
		Obstacles.clear();
		for (const FBox& Piece : Reserved)
		{
			if (!Piece.IsEmpty())
			{
				Obstacles.push_back(Piece.Grown(Params.Keep));
			}
		}

		// 1. The seals with no place: a neat stack in the middle of the top edge, side by side in card order.
		int Stacked = 0;
		for (const FWant& Want : Wants)
		{
			Stacked += Want.bPlace ? 0 : 1;
		}
		if (Stacked > 0)
		{
			const double Block = Stacked * Params.SealW + (Stacked - 1) * Params.Gap;
			double Middle = 0.0;
			if (FreeNearest(Params, EEdge::Top, Params.ScreenW * 0.5, Block * 0.5, Middle))
			{
				const FLane Top = LaneOf(Params, EEdge::Top);
				double X = Middle - Block * 0.5 + Params.SealW * 0.5;
				for (size_t Index = 0; Index < Wants.size(); ++Index)
				{
					if (Wants[Index].bPlace)
					{
						continue;
					}
					FSpot& Spot = Out[Index];
					Spot.bShown = true;
					Spot.Edge = EEdge::Top;
					Spot.Centre = FPoint{ X, Top.Fixed };
					Spot.From = FromOffScreen(Params, EEdge::Top, Spot.Centre);
					Claim(Params, Spot);
					X += Params.SealW + Params.Gap;
				}
			}
			else
			{
				// (no stretch of the top takes them all: one by one, as near its middle as each can, then the other edges)
				static const EEdge Round[4] = { EEdge::Top, EEdge::Bottom, EEdge::Left, EEdge::Right };
				double Along[4] = { Params.ScreenW * 0.5, Params.ScreenH * 0.5, Params.ScreenW * 0.5, Params.ScreenH * 0.5 };
				for (size_t Index = 0; Index < Wants.size(); ++Index)
				{
					if (!Wants[Index].bPlace)
					{
						PlaceOne(Params, Round, 4, Along, Out[Index]);
					}
				}
			}
		}

		// 2. Each seal with a place, in card order: on the edge nearest the place, as near it as there is room.
		for (size_t Index = 0; Index < Wants.size(); ++Index)
		{
			if (!Wants[Index].bPlace)
			{
				continue;
			}
			EEdge Edges[4] = { EEdge::Top, EEdge::Right, EEdge::Bottom, EEdge::Left };
			double Far[4] = { 0.0, 0.0, 0.0, 0.0 }, Along[4] = { 0.0, 0.0, 0.0, 0.0 }, Past[4] = { 0.0, 0.0, 0.0, 0.0 };
			for (int Side = 0; Side < 4; ++Side)
			{
				const FLane Lane = LaneOf(Params, static_cast<EEdge>(Side));
				Far[Side] = DistanceToLane(Lane, Wants[Index].At, Along[Side]);
				Past[Side] = std::fabs((Lane.bAlongX ? Wants[Index].At.X : Wants[Index].At.Y) - Along[Side]);
			}
			if (Wants[Index].Kept >= 0 && Wants[Index].Kept < 4)
			{
				Far[Wants[Index].Kept] = std::max(0.0, Far[Wants[Index].Kept] - Params.Stick);
			}
			// Nearest first. Beyond a corner both edges are as far (each is measured to the corner): there the edge the
			// place lies more squarely in front of comes first, the one it runs less far past the end of; and at an equal
			// distance otherwise, the order top, right, bottom, left. (A sort of four, in place.)
			auto Before = [&Far, &Past](EEdge A, EEdge B)
			{
				const double FarA = Far[static_cast<int>(A)], FarB = Far[static_cast<int>(B)];
				if (std::fabs(FarA - FarB) > 0.5)
				{
					return FarA < FarB;
				}
				return Past[static_cast<int>(A)] < Past[static_cast<int>(B)] - 0.5;
			};
			for (int Next = 1; Next < 4; ++Next)
			{
				const EEdge Moving = Edges[Next];
				int Into = Next;
				while (Into > 0 && Before(Moving, Edges[Into - 1]))
				{
					Edges[Into] = Edges[Into - 1];
					--Into;
				}
				Edges[Into] = Moving;
			}
			PlaceOne(Params, Edges, 4, Along, Out[Index]);
		}
	}

	FFold FLayout::LayRanked(const FParams& Params, const std::vector<FWant>& Items, int Total, int Most, const std::vector<FBox>& Reserved, std::vector<FSpot>& Out)
	{
		// (each item a seal of its own: an empty list of carries, which allocates nothing)
		return LayRanked(Params, Items, std::vector<int>(), Total, Most, Reserved, Out);
	}

	FFold FLayout::LayRanked(const FParams& Params, const std::vector<FWant>& Seals, const std::vector<int>& Carries, int Total, int Most, const std::vector<FBox>& Reserved, std::vector<FSpot>& Out)
	{
		const int Count = std::max(Total, 0);
		const int Room = std::max(Most, 1);
		// (the items the first Standing seals stand for)
		auto CarriedBy = [&Carries](int Standing)
		{
			int Carried = 0;
			for (int Index = 0; Index < Standing; ++Index)
			{
				Carried += static_cast<size_t>(Index) < Carries.size() ? std::max(Carries[static_cast<size_t>(Index)], 1) : 1;
			}
			return Carried;
		};
		// (the most seals that may stand: all of them where they are no more than Most and carry every item, so that no
		// "+N" is needed; else Most - 1, and the "+N")
		int Start = std::min(static_cast<int>(Seals.size()), Room);
		while (Start > 0 && CarriedBy(Start - 1) >= Count)
		{
			--Start; // (and no more seals than it takes to carry every item)
		}
		if (CarriedBy(Start) < Count)
		{
			Start = std::min(Start, Room - 1);
		}
		for (int Standing = Start; Standing >= 0; --Standing)
		{
			const int More = std::max(Count - CarriedBy(Standing), 0);
			Trial.assign(Seals.begin(), Seals.begin() + Standing);
			if (More > 0)
			{
				Trial.push_back(FWant()); // (the "+N" seal: it has no place, and waits in the stack at the top)
			}
			Lay(Params, Trial, Reserved, Out);
			bool bWhole = true;
			for (const FSpot& Spot : Out)
			{
				bWhole = bWhole && Spot.bShown;
			}
			if (bWhole || Standing == 0)
			{
				return FFold{ bWhole ? Standing : 0, More };
			}
		}
		Out.clear();
		return FFold{ 0, 0 };
	}

	bool Meet(const FBox& A, const FBox& B)
	{
		// (FSlateRect::DoRectanglesIntersect: apart only where one lies wholly beyond the other; touching is meeting)
		return !(B.MaxX < A.MinX || A.MaxX < B.MinX || B.MaxY < A.MinY || A.MaxY < B.MinY);
	}

	int UnderPiece(const FBox& Seal, const std::vector<FBox>& Pieces)
	{
		for (size_t Index = 0; Index < Pieces.size(); ++Index)
		{
			if (Meet(Seal, Pieces[Index]))
			{
				return static_cast<int>(Index);
			}
		}
		return -1;
	}

	bool FUnderWatch::Due(int Slot, int Revision, bool bLive, bool bArrived)
	{
		if (Slot < 0 || Slot >= Slots || !bLive || !bArrived || (bSeen[Slot] && SeenRevision[Slot] == Revision))
		{
			return false;
		}
		bSeen[Slot] = true;
		SeenRevision[Slot] = Revision;
		return true;
	}

	FSealAsk ReadSealAsk(const std::string& Asked, int Most)
	{
		FSealAsk Ask;
		size_t At = 0;
		while (At <= Asked.size())
		{
			const size_t End = std::min(Asked.find(',', At), Asked.size());
			std::string Part;
			for (size_t Index = At; Index < End; ++Index)
			{
				const char Letter = Asked[Index];
				if (!IsSpace(Letter))
				{
					Part += Letter >= 'A' && Letter <= 'Z' ? static_cast<char>(Letter - 'A' + 'a') : Letter;
				}
			}
			At = End + 1;
			if (Part.empty())
			{
				continue;
			}
			if (Part == "slide")
			{
				Ask.bSlide = true;
				continue;
			}
			// ("open:N", "hover:N": N a whole number from 1 to Most, at most two figures. The figures are read only once
			// they are known to be two digits or fewer, so a long number can never overflow the count)
			const size_t Colon = Part.find(':');
			const std::string Word = Part.substr(0, Colon == std::string::npos ? Part.size() : Colon);
			const std::string Number = Colon == std::string::npos ? std::string() : Part.substr(Colon + 1);
			bool bNumber = !Number.empty() && Number.size() <= 2;
			for (const char Figure : Number)
			{
				bNumber = bNumber && Figure >= '0' && Figure <= '9';
			}
			int Value = 0;
			for (size_t Index = 0; bNumber && Index < Number.size(); ++Index)
			{
				Value = Value * 10 + (Number[Index] - '0');
			}
			const bool bSeal = bNumber && Value >= 1 && Value <= std::max(Most, 1);
			if (bSeal && Word == "open")
			{
				Ask.Open = Value;
			}
			else if (bSeal && Word == "hover")
			{
				Ask.Hover = Value;
			}
			else
			{
				++Ask.Unknown;
			}
		}
		return Ask;
	}

	double SlideShare(double Since, int Order, double Stagger, double Slide)
	{
		const double Share = (Since - Order * Stagger) / std::max(Slide, 0.001);
		if (Share <= 0.0)
		{
			return 0.0;
		}
		if (Share >= 1.0)
		{
			return 1.0;
		}
		const double Left = 1.0 - Share;
		return 1.0 - Left * Left * Left;
	}

	double Pulse(double Now, double Period)
	{
		const double Turn = 6.283185307179586 * Now / std::max(Period, 0.001);
		return 0.5 - 0.5 * std::cos(Turn);
	}

	namespace Look
	{
		double Stroke(double Units, double PxPerUnit)
		{
			// (a screen of no measure is taken at a pixel a unit; a tiny one is not let swell the strokes past 6 units)
			const double Scale = PxPerUnit > 0.0 ? std::max(PxPerUnit, 0.25) : 1.0;
			return std::max(Units, MinStrokePx / Scale);
		}

		FPoint FitSign(double DrawnW, double DrawnH)
		{
			const double W = DrawnW > 0.0 && DrawnH > 0.0 ? DrawnW : 22.0;
			const double H = DrawnW > 0.0 && DrawnH > 0.0 ? DrawnH : 15.0;
			const double Fit = std::min(SignW / W, SignH / H);
			return FPoint{ W * Fit, H * Fit };
		}

		namespace
		{
			/** A seal sign's two measures, in pixels of its picture as drawn SignPictureW wide. */
			struct FSealMeasure
			{
				const char* Sign;
				double Stroke; // the thinnest stroke
				double Gap; // the narrowest gap or counter
			};

			// (the seal set of 22 November, 06:45Z, Content/CityOrders/Seal: each picture's ink measured at four samples
			// a pixel, as the widest disc that opens the ink, or the clear ground, taking nothing deeper than a pixel. A
			// stroke narrower goes whole, so none here is under the floor; but at a corner a sliver up to a pixel deep
			// passes, so a round cap or a fillet 16 across reads wider by the corner's angle (a square corner about 21).
			// What the screen holds to 1.5 px is SealFloor, the same for every sign: SignFinest. The star and the night
			// and day raids' signs have no seal picture and are never pressed: MayPress)
			// BEGIN seal_signs.py (written by Tools/edge_seals/seal_signs.py --write-table; edge_seals_test.cpp measures
			// the pictures again and holds these to them)
			constexpr FSealMeasure SealMeasures[] = {
				{ "aa", 19.625, 18.25 }, // cradle, breech, recoil and square dropped
				{ "arm", 21.0, 17.5 }, // gun lifted, deck cut back; turret, hull and tracks merged
				{ "art", 18.875, 19.0 }, // hub dropped
				{ "bmr", 16.375, 20.25 }, // offset only
				{ "cav", 16.875, 17.0 }, // offset only
				{ "cv", 17.625, 21.0 }, // offset only
				{ "eng", 21.0, 21.0 }, // river dropped
				{ "ftr", 16.375, 17.75 }, // offset only
				{ "hull", 17.5, 20.0 }, // offset only
				{ "inf", 17.0, 18.75 }, // rifle and bayonet dropped
				{ "mot", 21.0, 21.75 }, // box and cab merged
				{ "mtn", 17.5, 18.5 }, // flag dropped
				{ "para", 23.5, 16.5 }, // middle cord and hem scallops dropped
				{ "ss", 21.5, 20.0 }, // offset only
				{ "tp", 17.5, 20.0 }, // both funnels kept, set apart
			};
			// END seal_signs.py

			const FSealMeasure* SealMeasureOf(const std::string& Sign)
			{
				for (const FSealMeasure& Each : SealMeasures)
				{
					if (Sign == Each.Sign)
					{
						return &Each;
					}
				}
				return nullptr;
			}
		}

		double SignThinnest(const std::string& Sign)
		{
			const FSealMeasure* Measure = SealMeasureOf(Sign);
			return Measure ? Measure->Stroke : 0.0;
		}

		double SignNarrowestGap(const std::string& Sign)
		{
			const FSealMeasure* Measure = SealMeasureOf(Sign);
			return Measure ? Measure->Gap : 0.0;
		}

		double SignFinest(const std::string& Sign)
		{
			// (every cap, gap and fillet of the set is drawn 16 across: one width for the whole set, so that every seal
			// takes its sign, or every seal its mark, on any one screen, never a mix set by the measure's corner allowance)
			return SealMeasureOf(Sign) ? SealFloor : 0.0;
		}

		bool SignHolds(double Finest, double DrawnW, double DrawnH, double PxPerUnit)
		{
			// (as Stroke takes a screen of no measure: a pixel a unit)
			const double Scale = PxPerUnit > 0.0 ? PxPerUnit : 1.0;
			const double Units = Finest * FitSign(DrawnW, DrawnH).X / SignPictureW;
			return Finest > 0.0 && Units * Scale >= MinStrokePx;
		}

		FSword SwordOf(double PxPerUnit)
		{
			// (a screen of no measure is taken at a pixel a unit, as Stroke takes it)
			const double Scale = PxPerUnit > 0.0 ? PxPerUnit : 1.0;
			FSword Sword;
			Sword.Turn = 36.0; // (as flat as the X's strokes were)
			Sword.Lift = -0.32; // (the two swords' box, point to pommel, centred on the disc's middle)
			Sword.Tip = 9.8;
			Sword.BladeThick = Stroke(2.4, PxPerUnit); // (the X's own weight)
			Sword.Guard = -5.0;
			Sword.GuardThick = Stroke(1.8, PxPerUnit);
			// (the guard stands out 2 units past the blade on each side, more on a small screen, so that it holds 1.5 px)
			Sword.Overhang = std::max(2.0, MinStrokePx / Scale);
			Sword.GuardLong = Sword.BladeThick + 2.0 * Sword.Overhang;
			Sword.GripThick = Stroke(1.6, PxPerUnit);
			Sword.Pommel = -9.0;
			Sword.PommelD = 3.2;
			Sword.Neck = (Sword.Guard - Sword.GuardThick * 0.5) - (Sword.Pommel + Sword.PommelD * 0.5);
			// (the pommel where its neck and its width past the grip both hold 1.5 px: from a pixel a unit up)
			Sword.bPommel = Sword.Neck * Scale >= MinStrokePx - 1e-9 && (Sword.PommelD - Sword.GripThick) * Scale >= MinStrokePx - 1e-9;
			return Sword;
		}

		/** The strokes of a drawn mark, all of them (MarkOf hands on as many as its caller has room for). */
		static int MarkStrokes(EGlyph Glyph, double PxPerUnit, FStroke (&Out)[MarkMost])
		{
			auto Bar = [PxPerUnit](double X, double Y, double Long, double Thick, double Degrees)
			{
				return FStroke{ EStroke::Bar, X, Y, Long, Stroke(Thick, PxPerUnit), Degrees };
			};
			// (a chevron's arms run from (+-7.5, -+3.25) to (0, +-3.25): 17 across with their round ends, and the ends
			// stay at the arms' tips however thick the stroke is made)
			const double ArmThick = Stroke(2.4, PxPerUnit);
			const double ArmLong = std::sqrt(7.5 * 7.5 + 6.5 * 6.5) + ArmThick;
			const double ArmTurn = std::atan2(6.5, 7.5) * 57.29577951308232;
			switch (Glyph)
			{
			case EGlyph::Warning:
				// (an upright stroke and a dot under it, 16 high)
				Out[0] = Bar(0.0, -2.4, 11.5, 2.6, 90.0);
				Out[1] = FStroke{ EStroke::Dot, 0.0, 6.0, std::max(3.6, Stroke(2.6, PxPerUnit)), 0.0, 0.0 };
				return 2;
			case EGlyph::War:
			{
				// (two swords crossed flat, as sabres are, the points up and out: each a blade, a guard, a grip and, where it
				// holds, a pommel; the second the first in a mirror. About 18 across and 13 high, as the X was: SwordOf)
				const FSword Sword = SwordOf(PxPerUnit);
				const double Turn = Sword.Turn * 0.017453292519943295;
				const double AlongX = std::cos(Turn), AlongY = -std::sin(Turn); // (the first sword, from its foot toward its point)
				int Count = 0;
				for (const double Side : { 1.0, -1.0 })
				{
					// (a point at S along the sword, from where the two cross; the second sword mirrored left for right)
					auto At = [&](double S) { return FPoint{ Side * S * AlongX, Sword.Lift + S * AlongY }; };
					const double Blade = -Side * Sword.Turn, Across = Side * (90.0 - Sword.Turn);
					const FPoint BladeAt = At((Sword.Tip + Sword.Guard) * 0.5), GuardAt = At(Sword.Guard), GripAt = At((Sword.Guard + Sword.Pommel) * 0.5), PommelAt = At(Sword.Pommel);
					Out[Count++] = FStroke{ EStroke::Bar, BladeAt.X, BladeAt.Y, Sword.Tip - Sword.Guard, Sword.BladeThick, Blade };
					Out[Count++] = FStroke{ EStroke::Bar, GuardAt.X, GuardAt.Y, Sword.GuardLong, Sword.GuardThick, Across };
					Out[Count++] = FStroke{ EStroke::Bar, GripAt.X, GripAt.Y, Sword.Guard - Sword.Pommel + Sword.GripThick, Sword.GripThick, Blade };
					if (Sword.bPommel)
					{
						Out[Count++] = FStroke{ EStroke::Dot, PommelAt.X, PommelAt.Y, Sword.PommelD, 0.0, 0.0 };
					}
				}
				return Count;
			}
			case EGlyph::Fallen:
				Out[0] = FStroke{ EStroke::Bar, -3.75, 0.0, ArmLong, ArmThick, ArmTurn };
				Out[1] = FStroke{ EStroke::Bar, 3.75, 0.0, ArmLong, ArmThick, -ArmTurn };
				return 2;
			case EGlyph::Freed:
				Out[0] = FStroke{ EStroke::Bar, -3.75, 0.0, ArmLong, ArmThick, -ArmTurn };
				Out[1] = FStroke{ EStroke::Bar, 3.75, 0.0, ArmLong, ArmThick, ArmTurn };
				return 2;
			case EGlyph::News:
				// (three short lines, the last shorter: 17 across)
				Out[0] = Bar(0.0, -4.6, 17.0, 2.2, 0.0);
				Out[1] = Bar(0.0, 0.0, 17.0, 2.2, 0.0);
				Out[2] = Bar(-3.0, 4.6, 11.0, 2.2, 0.0);
				return 3;
			case EGlyph::Globe:
				// (a ring 16.4 across, its equator and its meridian)
				Out[0] = FStroke{ EStroke::Ring, 0.0, 0.0, 16.4, Stroke(1.7, PxPerUnit), 0.0 };
				Out[1] = Bar(0.0, 0.0, 16.4, 1.6, 0.0);
				Out[2] = Bar(0.0, 0.0, 16.4, 1.6, 90.0);
				return 3;
			case EGlyph::Sign:
			case EGlyph::More:
				break;
			}
			return 0;
		}

		int MarkOf(EGlyph Glyph, double PxPerUnit, FStroke* Out, int Room)
		{
			FStroke Made[MarkMost];
			const int Count = MarkStrokes(Glyph, PxPerUnit, Made);
			const int Given = std::max(std::min(Count, Room), 0);
			for (int Index = 0; Index < Given; ++Index)
			{
				Out[Index] = Made[Index];
			}
			return Given;
		}

		double WallOf(double PxPerUnit)
		{
			return PxPerUnit > 0.0 ? std::min(std::max(1.0 / PxPerUnit, 0.75), 1.0) : 1.0;
		}

		FLips HollowLips()
		{
			return FLips{ LightX, LightY, -LightX * 0.7, -LightY * 0.7, HollowR + 0.1 };
		}

		FCall CallOf(double Since, double Period)
		{
			const double Span = std::max(Period, 0.001);
			const double Beat = Pulse(Since, Span);
			const double Spread = std::fmod(std::max(Since, 0.0), Span) / Span;
			FCall Call;
			Call.Grow = 1.0 + Breath * Beat;
			Call.GlowR = DiscR + 2.5;
			Call.Glow = 0.2 * Beat;
			// (from just off the rim out to 24.5: it rises over the first tenth of its spread and fades as it goes, so that
			// it starts again unseen at the rim and is gone at its widest)
			Call.RingR = DiscR + 1.5 + 8.0 * Spread;
			Call.RingW = 1.6;
			Call.Ring = 0.55 * std::min(Spread / 0.1, 1.0) * (1.0 - Spread);
			return Call;
		}

		double FigureLift()
		{
			// (the line's middle, and the middle of a figure standing on its baseline, both from the line's top)
			return ((Ascent + Descent) * 0.5 - (Ascent - FigureH * 0.5)) * CountEm();
		}

		FBox TabOf(int Figures)
		{
			const int Shown = std::min(std::max(Figures, 1), CountMostFigures);
			const double HalfW = ((FigureInk + FigureAdvance * (Shown - 1)) * CountEm()) * 0.5 + TabPad;
			return FBox{ MidX - HalfW, TabTop, MidX + HalfW, TabBottom };
		}

		FBox FiguresOf(int Figures)
		{
			const FBox Tab = TabOf(Figures);
			const double Middle = (Tab.MinY + Tab.MaxY) * 0.5, HalfH = FigureH * CountEm() * 0.5;
			return FBox{ Tab.MinX + TabPad, Middle - HalfH, Tab.MaxX - TabPad, Middle + HalfH };
		}

		double FigurePx(double PxPerUnit)
		{
			return FigureH * CountEm() * PxPerUnit;
		}

		int FiguresIn(int Count)
		{
			int Figures = 1;
			for (int Left = std::max(Count, 0); Left >= 10; Left /= 10)
			{
				++Figures;
			}
			return Figures;
		}
	}
}
