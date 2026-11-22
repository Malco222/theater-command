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
}
