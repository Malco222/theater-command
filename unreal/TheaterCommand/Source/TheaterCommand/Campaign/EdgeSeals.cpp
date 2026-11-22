#include "Campaign/EdgeSeals.h"

#include <algorithm>
#include <cmath>

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
		const int Count = std::max(Total, 0);
		const int Room = std::max(Most, 1);
		// (the most items that may stand: all of them where they and no "+N" fit in Most seals, else Most - 1)
		const int Start = std::min(static_cast<int>(Items.size()), Count <= Room ? Count : Room - 1);
		for (int Standing = Start; Standing >= 0; --Standing)
		{
			const int More = Count - Standing;
			Trial.assign(Items.begin(), Items.begin() + Standing);
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

		int MarkOf(EGlyph Glyph, double PxPerUnit, FStroke (&Out)[4])
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
				// (two strokes crossed flat, as sabres are: about 17 across, 13 high)
				Out[0] = Bar(0.0, 0.0, 20.0, 2.4, 36.0);
				Out[1] = Bar(0.0, 0.0, 20.0, 2.4, -36.0);
				return 2;
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
	}
}
