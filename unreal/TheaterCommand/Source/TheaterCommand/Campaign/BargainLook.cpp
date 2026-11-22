#include "Campaign/BargainLook.h"
#include "Campaign/EdgeSeals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <utility>

// (the sealed-letter sign is the edge seals' own, at 0.6 of their box)
static_assert(Bargains::SealBoxW == EdgeSeals::Look::BoxW * Bargains::SealScale, "the seal's box is the edge seals' at 0.6");
static_assert(Bargains::SealBoxH == EdgeSeals::Look::BoxH * Bargains::SealScale, "the seal's box is the edge seals' at 0.6");
static_assert(Bargains::SealMid == EdgeSeals::Look::MidX * Bargains::SealScale && EdgeSeals::Look::MidX == EdgeSeals::Look::MidY, "the disc's middle");

namespace Bargains
{
	namespace
	{
		const char* const MiddleDot = "\xC2\xB7"; // U+00B7
		const char* const Minus = "\xE2\x88\x92"; // U+2212
		const char* const EnDash = "\xE2\x80\x93"; // U+2013: an own row with no bid yet

		FPt Add(FPt A, FPt B) { return FPt{ A.X + B.X, A.Y + B.Y }; }
		FPt Sub(FPt A, FPt B) { return FPt{ A.X - B.X, A.Y - B.Y }; }
		FPt Mul(FPt A, double K) { return FPt{ A.X * K, A.Y * K }; }
		double Dot(FPt A, FPt B) { return A.X * B.X + A.Y * B.Y; }
		double Len(FPt A) { return std::sqrt(A.X * A.X + A.Y * A.Y); }
		FPt Unit(FPt A)
		{
			const double L = Len(A);
			return L > 1e-12 ? FPt{ A.X / L, A.Y / L } : FPt{ 0.0, 0.0 };
		}
		FPt Left(FPt D) { return FPt{ -D.Y, D.X }; }

		const FRow* RowById(const FBook& Book, std::string_view Id)
		{
			for (const FRow& Row : Book.Rows)
			{
				if (Row.Id == Id)
				{
					return &Row;
				}
			}
			return nullptr;
		}

		/** A part of the crate's words that shows: it rounds to more than nothing. */
		bool Shows(double Value) { return Value >= 0.05; }

		/**
		 * A strip's two sides at a point P of its path, between the way in and the way out (unit vectors), HalfW each
		 * side: Lefts on the left of the way, Rights on the right. Where the turn is gentle enough for a miter no longer
		 * than MiterMost half widths, one pair at the miter (returns 1). Where it is sharper (more than 120 degrees at
		 * MiterMost 2), the outside of the turn is bevelled with two points, square to the way in and then to the way
		 * out, and the inside is held at MiterMost half widths along the miter, so that the strip neither spikes nor folds
		 * over itself (returns 2, the inside's point twice).
		 */
		int SidesAt(FPt P, FPt DirIn, FPt DirOut, double HalfW, FPt (&Lefts)[2], FPt (&Rights)[2])
		{
			const FPt NIn = Left(DirIn), NOut = Left(DirOut);
			const FPt Sum = Add(NIn, NOut);
			const double SumLong = Len(Sum);
			// (the miter's way, and the cosine of half the turn; a turn right back has no miter: back along the way in)
			const FPt M = SumLong > 1e-9 ? Mul(Sum, 1.0 / SumLong) : Mul(DirIn, -1.0);
			const double Cos = SumLong > 1e-9 ? Dot(M, NOut) : 0.0;
			if (Cos >= 1.0 / Lane::MiterMost)
			{
				const FPt Off = Mul(M, HalfW / Cos);
				Lefts[0] = Lefts[1] = Add(P, Off);
				Rights[0] = Rights[1] = Sub(P, Off);
				return 1;
			}
			const FPt Inside = Mul(M, HalfW * Lane::MiterMost);
			// (the path turns towards its left where the way out leans to the left of the way in: the left is the inside)
			if (Dot(DirOut, NIn) >= 0.0)
			{
				Lefts[0] = Lefts[1] = Add(P, Inside);
				Rights[0] = Sub(P, Mul(NIn, HalfW));
				Rights[1] = Sub(P, Mul(NOut, HalfW));
			}
			else
			{
				Rights[0] = Rights[1] = Sub(P, Inside);
				Lefts[0] = Add(P, Mul(NIn, HalfW));
				Lefts[1] = Add(P, Mul(NOut, HalfW));
			}
			return 2;
		}

		/**
		 * Puts the next point of a strip's side, keeping the side from running backwards where the inside of a turn
		 * would fold it over itself: a point that would step against the way the path steps there stays where the last
		 * one was. Step is the path's step to this point (nothing for the second point of a bevel).
		 */
		void PutSide(std::vector<FPt>& Side, FPt At, FPt Step)
		{
			if (!Side.empty() && Dot(Sub(At, Side.back()), Step) < 0.0)
			{
				At = Side.back();
			}
			Side.push_back(At);
		}

		/** The unit directions along a path at each point (the way in and the way out), skipping repeated points. */
		void Ways(const FPath& Path, bool bClosed, size_t I, FPt& In, FPt& Out)
		{
			const size_t N = Path.size();
			In = FPt{};
			Out = FPt{};
			for (size_t Back = 1; Back < N; ++Back)
			{
				if (I < Back && !bClosed)
				{
					break;
				}
				const FPt D = Sub(Path[I], Path[(I + N - Back) % N]);
				if (Len(D) > 1e-9)
				{
					In = Unit(D);
					break;
				}
			}
			for (size_t Ahead = 1; Ahead < N; ++Ahead)
			{
				if (I + Ahead >= N && !bClosed)
				{
					break;
				}
				const FPt D = Sub(Path[(I + Ahead) % N], Path[I]);
				if (Len(D) > 1e-9)
				{
					Out = Unit(D);
					break;
				}
			}
			if (Len(In) < 0.5)
			{
				In = Out;
			}
			if (Len(Out) < 0.5)
			{
				Out = In;
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Colours and units

	float LinearOfByte(uint8_t Byte)
	{
		const double C = Byte / 255.0;
		return static_cast<float>(C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4));
	}

	FLinear LinearOf(FInk Ink)
	{
		return FLinear{ LinearOfByte(static_cast<uint8_t>((Ink.Srgb >> 16) & 0xFF)), LinearOfByte(static_cast<uint8_t>((Ink.Srgb >> 8) & 0xFF)),
			LinearOfByte(static_cast<uint8_t>(Ink.Srgb & 0xFF)), Ink.A };
	}

	double PxOf(double ViewportHigh, double FrameScale, double TickScale)
	{
		const double Under = FrameScale * TickScale;
		return ViewportHigh > 0.0 && Under > 1e-6 ? (ViewportHigh / 900.0) / Under : 1.0;
	}

	double PixelOf(double CameraDistance, double UnitScale, double Px, double TickScale)
	{
		return 0.00037 * CameraDistance / std::max(UnitScale, 1e-9) * Px * TickScale;
	}

	bool Relay(double& LaidAt, double CameraDistance)
	{
		if (LaidAt > 0.0 && std::fabs(CameraDistance - LaidAt) < RelayStep * LaidAt)
		{
			return false;
		}
		LaidAt = CameraDistance;
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Plain geometry

	bool FRect::Overlaps(const FRect& Other) const
	{
		return X < Other.X + Other.W && Other.X < X + W && Y < Other.Y + Other.H && Other.Y < Y + H;
	}

	double FRect::Overlap(const FRect& Other) const
	{
		const double Wide = std::min(X + W, Other.X + Other.W) - std::max(X, Other.X);
		const double High = std::min(Y + H, Other.Y + Other.H) - std::max(Y, Other.Y);
		return Wide > 0.0 && High > 0.0 ? Wide * High : 0.0;
	}

	double Distance(FPt A, FPt B) { return Len(Sub(A, B)); }

	FPath& FPathList::Add()
	{
		if (Count == Paths.size())
		{
			// (room for a dash that crosses a few of the curve's points, so that laying again at another zoom seldom grows it)
			Paths.emplace_back();
			Paths.back().reserve(8);
		}
		FPath& Path = Paths[Count++];
		Path.clear();
		return Path;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Faces

	FFace FaceOf(EFamily Family, EStyle Style, double SizePx, double Px, int Spacing)
	{
		FFace Face;
		Face.Family = Family;
		Face.Style = Style;
		Face.Size = SizePx * Px;
		Face.Spacing = Spacing;
		return Face;
	}

	FFace CapsFace(double Px) { return FaceOf(EFamily::Courier, EStyle::Bold, 12.0, Px, 80); }
	FFace SellerFace(double Px) { return FaceOf(EFamily::Serif, EStyle::Regular, 12.0, Px); }
	FFace HeldFace(double Px) { return FaceOf(EFamily::Serif, EStyle::Italic, 12.0, Px); }
	FFace NameFace(double Px) { return FaceOf(EFamily::Serif, EStyle::Regular, 12.0, Px); }
	FFace FigureFace(double Px) { return FaceOf(EFamily::Courier, EStyle::Bold, 18.0, Px); }
	FFace RingFace(double Px) { return FaceOf(EFamily::Courier, EStyle::Bold, 14.0, Px); }
	FFace MoFace(double Px) { return FaceOf(EFamily::Courier, EStyle::Bold, 9.0, Px); }
	FFace StepFace(double Px) { return FaceOf(EFamily::Courier, EStyle::Bold, 14.0, Px); }
	FFace KitButtonFace(double Px) { return FaceOf(EFamily::Courier, EStyle::Bold, 11.0, Px, 100); }
	FFace HintFace(double Px) { return FaceOf(EFamily::Serif, EStyle::Regular, 11.0, Px); }
	FFace PlateLineFace(double Px) { return FaceOf(EFamily::Serif, EStyle::Regular, 12.0, Px); }
	FFace PlateNameFace(double Px) { return FaceOf(EFamily::Serif, EStyle::Regular, 13.0, Px); }
	FFace TrayLineFace(double Px) { return FaceOf(EFamily::Courier, EStyle::Regular, 13.0, Px); }
	FFace TrayLegionFace(double Px) { return FaceOf(EFamily::Serif, EStyle::Regular, 12.0, Px); }
	FFace TagFace(double Px) { return FaceOf(EFamily::Serif, EStyle::Regular, 12.0, Px); }

	int Wrap(std::string_view Text, const FFace& Face, double Width, const FMeasure& Measure, int MostLines, std::vector<std::string>& Out)
	{
		Out.clear();
		std::string Line;
		size_t At = 0;
		while (At < Text.size() && static_cast<int>(Out.size()) < MostLines)
		{
			while (At < Text.size() && Text[At] == ' ')
			{
				++At;
			}
			if (At >= Text.size())
			{
				break;
			}
			size_t End = Text.find(' ', At);
			if (End == std::string_view::npos)
			{
				End = Text.size();
			}
			const std::string_view Word = Text.substr(At, End - At);
			At = End;
			std::string Trial = Line;
			if (!Trial.empty())
			{
				Trial += ' ';
			}
			Trial += Word;
			if (Line.empty() || Measure(Trial, Face).X <= Width + 1e-9)
			{
				Line = std::move(Trial);
				continue;
			}
			Out.push_back(Line);
			Line.assign(Word);
		}
		if (!Line.empty() && static_cast<int>(Out.size()) < MostLines)
		{
			Out.push_back(Line);
		}
		return static_cast<int>(Out.size());
	}

	bool FitLine(std::string_view Text, const FFace& Face, double Width, const FMeasure& Measure, std::string& Out)
	{
		const std::string_view Dots = "\xE2\x80\xA6"; // U+2026
		Out.assign(Text);
		if (Text.empty() || Measure(Text, Face).X <= Width + 1e-9)
		{
			return true;
		}
		auto Cut = [&Dots](std::string_view Head, std::string& Into)
		{
			while (!Head.empty() && (Head.back() == ' ' || Head.back() == ',' || Head.back() == ';' || Head.back() == ':' || Head.back() == '.'))
			{
				Head.remove_suffix(1);
			}
			Into.assign(Head);
			Into.append(Dots);
		};
		// At whole words: the longest head that fits with the ellipsis.
		std::string Trial;
		Out.clear();
		for (size_t End = Text.find(' '); End != std::string_view::npos; End = Text.find(' ', End + 1))
		{
			Cut(Text.substr(0, End), Trial);
			if (Measure(Trial, Face).X > Width + 1e-9)
			{
				break;
			}
			Out = Trial;
		}
		if (!Out.empty() && Out != Dots)
		{
			return false;
		}
		// One word too long for the line: by letters (whole UTF-8 sequences).
		Out.clear();
		for (size_t End = 1; End < Text.size(); ++End)
		{
			if ((static_cast<unsigned char>(Text[End]) & 0xC0u) == 0x80u)
			{
				continue;
			}
			Cut(Text.substr(0, End), Trial);
			if (Measure(Trial, Face).X > Width + 1e-9)
			{
				break;
			}
			Out = Trial;
		}
		if (Out.empty() && Measure(Dots, Face).X <= Width + 1e-9)
		{
			Out.assign(Dots);
		}
		return false;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Draws

	FDraw BoxDraw(const FRect& At, FInk Fill, FInk Edge, double EdgeW, double Radius)
	{
		FDraw Draw;
		Draw.Kind = EDraw::Box;
		Draw.At = At;
		Draw.Fill = Fill;
		Draw.Edge = Edge;
		Draw.EdgeW = EdgeW;
		Draw.Radius = Radius;
		return Draw;
	}

	FDraw RingDraw(FPt Centre, double Diameter, double Width, FInk Ink)
	{
		const FRect At{ Centre.X - Diameter * 0.5, Centre.Y - Diameter * 0.5, Diameter, Diameter };
		return BoxDraw(At, FInk{ Ink.Srgb, Kit::Hollow }, Ink, Width, Diameter * 0.5);
	}

	FDraw TextDraw(FPt TopLeft, std::string_view Text, const FFace& Font, FInk Ink, const FMeasure& Measure)
	{
		FDraw Draw;
		Draw.Kind = EDraw::Text;
		const FPt Size = Measure(Text, Font);
		Draw.At = FRect{ TopLeft.X, TopLeft.Y, Size.X, Size.Y };
		Draw.Text.assign(Text);
		Draw.Font = Font;
		Draw.Fill = Ink;
		return Draw;
	}

	FDraw LineDraw(FPt A, FPt B, double Width, FInk Ink)
	{
		FDraw Draw;
		Draw.Kind = EDraw::Line;
		Draw.A = A;
		Draw.B = B;
		Draw.EdgeW = Width;
		Draw.Edge = Ink;
		Draw.At = FRect{ std::min(A.X, B.X), std::min(A.Y, B.Y), std::fabs(B.X - A.X), std::fabs(B.Y - A.Y) };
		return Draw;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Words

	std::string_view NounOf(std::string_view Sign)
	{
		if (Sign == "ore") return "Ore";
		if (Sign == "oil") return "Oil";
		if (Sign == "chrome") return "Chrome";
		if (Sign == "wolfram") return "Wolfram";
		if (Sign == "beef") return "Food";
		if (Sign == "base") return "Bases";
		return "Goods";
	}

	std::string SignKey(std::string_view Sign)
	{
		static const char* const Drawn[] = { "ore", "oil", "chrome", "wolfram", "beef", "base", "exports" };
		for (const char* Each : Drawn)
		{
			if (Sign == Each)
			{
				return std::string("goods-") + Each;
			}
		}
		return "goods-exports";
	}

	std::string Caps(std::string_view Text)
	{
		std::string Out(Text);
		for (char& Letter : Out)
		{
			if (Letter >= 'a' && Letter <= 'z')
			{
				Letter = static_cast<char>(Letter - 'a' + 'A');
			}
		}
		return Out;
	}

	std::string Money(double Pp, double Scale)
	{
		const double Shown = std::isfinite(Pp) && std::isfinite(Scale) ? Pp * Scale : 0.0;
		return std::to_string(std::llround(std::clamp(Shown, -1e15, 1e15)));
	}

	std::string Figure(double Value)
	{
		char Buffer[64];
		std::snprintf(Buffer, sizeof(Buffer), "%.1f", Value);
		std::string Out(Buffer);
		if (Out.size() >= 2 && Out.compare(Out.size() - 2, 2, ".0") == 0)
		{
			Out.resize(Out.size() - 2);
		}
		if (Out == "-0")
		{
			Out = "0";
		}
		return Out;
	}

	std::string NameOf(const FBook& Book, std::string_view Tag)
	{
		const FNation* Nation = Book.Nation(Tag);
		return Nation && !Nation->Name.empty() ? Nation->Name : std::string(Tag);
	}

	namespace
	{
		/** A nation's name as it follows another word, where the book's nations carry one ("the United States"). */
		template <typename TNation>
		std::string TheOf(const TNation& Nation)
		{
			if constexpr (requires(const TNation& N) { N.The; })
			{
				if (!Nation.The.empty())
				{
					return Nation.The;
				}
			}
			return Nation.Name;
		}
	}

	std::string TheNameOf(const FBook& Book, std::string_view Tag)
	{
		const FNation* Nation = Book.Nation(Tag);
		return Nation && !TheOf(*Nation).empty() ? TheOf(*Nation) : std::string(Tag);
	}

	std::string AdjOf(const FBook& Book, std::string_view Tag)
	{
		const FNation* Nation = Book.Nation(Tag);
		return Nation && !Nation->Adj.empty() ? Nation->Adj : NameOf(Book, Tag);
	}

	std::string HeldLine(const FRow& Row, const FBook& Book)
	{
		if (Row.Buyer.empty())
		{
			return "no buyer yet";
		}
		const int Term = std::max(1, Book.Term);
		const int Now = std::clamp(Term - Row.MonthsLeft + 1, 1, Term);
		return "held by " + TheNameOf(Book, Row.Buyer) + ", " + std::to_string(Now) + " of " + std::to_string(Term) + (Term == 1 ? " month" : " months");
	}

	std::string RivalHint(const FRow& Row)
	{
		const std::string Where = !Row.CapitalName.empty() ? Row.CapitalName : !Row.SellerName.empty() ? Row.SellerName + "'s capital" : std::string("the seller's capital");
		return "Rival bids show with an agent in " + Where + ", or in the bidder's capital.";
	}

	std::string SealTip(const FRow& Row)
	{
		if (!Row.Round)
		{
			return Row.Name + ".";
		}
		std::string Tip = Row.Name + ": bids this month.";
		if (OpensCard(Row))
		{
			Tip += " Click to see the bids.";
		}
		else if (!Row.Round->CanBid.empty())
		{
			Tip += " " + Row.Round->CanBid;
		}
		return Tip;
	}

	std::string CutHover(const FRow& Row, const FBook& Book)
	{
		std::string Out = std::string(NounOf(Row.Sign)) + " for " + TheNameOf(Book, Row.Buyer) + " lost at sea";
		const FSeaMark* Mark = Row.Cut ? &*Row.Cut : Row.Loss ? &*Row.Loss : nullptr;
		if (Mark && !Mark->Name.empty())
		{
			Out += ", " + Mark->Name;
		}
		if (Row.Cut && !Row.Cut->Month.empty())
		{
			Out += ", " + Row.Cut->Month;
		}
		return Out;
	}

	std::string CrateHover(const FRow& Row, const FBook& Book)
	{
		std::string Out = std::string(NounOf(Row.Sign)) + " reached " + TheNameOf(Book, Row.Buyer);
		if (!Book.Month.empty())
		{
			Out += ", " + Book.Month;
		}
		std::string Parts;
		auto Part = [&Parts](const std::string& Text)
		{
			Parts += Parts.empty() ? "" : ", ";
			Parts += Text;
		};
		if (Shows(Row.Cargo.Sup))
		{
			Part(Figure(Row.Cargo.Sup) + " supply");
		}
		if (Shows(Row.Cargo.Oil))
		{
			Part(Figure(Row.Cargo.Oil) + " oil");
		}
		if (!Row.Cargo.Pp)
		{
			Part("production ?");
		}
		else if (Shows(*Row.Cargo.Pp))
		{
			Part(Figure(*Row.Cargo.Pp) + " production");
		}
		if (!Parts.empty())
		{
			Out += ": " + Parts;
		}
		return Out;
	}

	std::string ExitText(const FRow& Row, const FBook& Book) { return "to " + TheNameOf(Book, Row.Buyer); }

	std::string PlateLine(const FBook& Book, std::string_view Home)
	{
		int Serving = 0, Returning = 0;
		std::string ServingAt, HomeFrom;
		bool bServingSame = true, bHomeSame = true;
		for (const FLegion& Legion : Book.Legions)
		{
			if (Legion.Home != Home)
			{
				continue;
			}
			int& Count = Legion.bRecall ? Returning : Serving;
			std::string& Place = Legion.bRecall ? HomeFrom : ServingAt;
			bool& bSame = Legion.bRecall ? bHomeSame : bServingSame;
			if (Count == 0)
			{
				Place = Legion.Place;
			}
			else if (Place != Legion.Place)
			{
				bSame = false;
			}
			++Count;
		}
		if (Serving > 0)
		{
			std::string Line = std::to_string(Serving) + (Serving == 1 ? " division abroad" : " divisions abroad");
			if (bServingSame && !ServingAt.empty())
			{
				Line += ", " + ServingAt;
			}
			return Line;
		}
		if (Returning > 0)
		{
			return "home from " + (bHomeSame && !HomeFrom.empty() ? HomeFrom : std::string("abroad"));
		}
		return std::string();
	}

	std::string TrayLine(const FLegion& Legion)
	{
		const std::string Dot = std::string(" ") + MiddleDot + " ";
		return "1 DIV" + Dot + std::to_string(Legion.Pct) + "%" + Dot + "TOUR " + std::to_string(Legion.TourAt) + " OF " + std::to_string(Legion.Tour) + " MO";
	}

	std::string FlagTip(const FLegion& Legion, const FBook& Book)
	{
		const std::string Home = !Legion.HomeName.empty() ? Legion.HomeName : NameOf(Book, Legion.Home);
		std::string Tip = "Volunteers from " + Home + ", with the " + AdjOf(Book, Legion.Patron) + " army";
		if (!Legion.Place.empty())
		{
			Tip += ", " + Legion.Place + " front";
		}
		return Tip + ".";
	}

	std::string LegionLine(const FLegion& Legion, const FBook& Book)
	{
		const FNation* Home = Book.Nation(Legion.Home);
		std::string Line;
		if (Home && !Home->Adj.empty())
		{
			Line = Home->Adj + " volunteers";
		}
		else
		{
			Line = "Volunteers from " + (!Legion.HomeName.empty() ? Legion.HomeName : NameOf(Book, Legion.Home));
		}
		Line += ", with the " + AdjOf(Book, Legion.Patron) + " army";
		if (!Legion.Place.empty())
		{
			Line += ", " + Legion.Place + " front";
		}
		return Line;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Plain flags

	const FPlainFlag* PlainFlagOf(std::string_view Tag)
	{
		// (the picture's red, yellow, black and white; plain period flags, no arms)
		static const FPlainFlag Spain{ "ESP_PLAIN", false, 3, { { 0xB33A2E, 1 }, { 0xC8A43A, 2 }, { 0xB33A2E, 1 } } };
		static const FPlainFlag Portugal{ "POR_PLAIN", true, 2, { { 0x2F6B3A, 2 }, { 0xB33A2E, 3 }, { 0, 0 } } };
		static const FPlainFlag Germany{ "GER_PLAIN", false, 3, { { 0x151515, 1 }, { 0xEFEAE0, 1 }, { 0xB33A2E, 1 } } };
		if (Tag == "ESP") return &Spain;
		if (Tag == "POR") return &Portugal;
		if (Tag == "GER") return &Germany;
		return nullptr;
	}

	FPennantShape PennantShape(double NearY, double TopZ, double FlagWidth, int Allies, double U)
	{
		const double AllyHigh = FlagWidth * 0.46 / 1.5;
		const double Wide = FlagWidth * PennantShare, High = Wide * PennantHoist;
		const double Top = TopZ - static_cast<double>(std::clamp(Allies, 0, 3)) * (AllyHigh + 0.3 * U);
		FPennantShape Out;
		Out.At[0] = FPt{ NearY, Top };
		Out.At[1] = FPt{ NearY, Top - High };
		Out.At[2] = FPt{ NearY + Wide, Top - High * 0.5 };
		Out.Uv[0] = FPt{ 0.0, 0.0 };
		Out.Uv[1] = FPt{ 0.0, 1.0 };
		Out.Uv[2] = FPt{ 1.0, 0.5 };
		return Out;
	}

	std::string FlagKey(std::string_view Tag)
	{
		const FPlainFlag* Plain = PlainFlagOf(Tag);
		return Plain ? std::string(Plain->Key) : std::string(Tag);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// What the map shows

	bool OpensCard(const FRow& Row)
	{
		return Row.Round && (Row.Round->CanBid.empty() || Row.Round->Mine || Row.Round->bAccept || Row.bAgentSeller || !Row.Round->Bids.empty());
	}

	bool IsOwn(const FRow& Row, const FBook& Book)
	{
		return Row.Rel == ERel::Self || (!Book.Viewer.empty() && (Row.Seller == Book.Viewer || Row.Buyer == Book.Viewer)) || (Row.Round && Row.Round->Mine);
	}

	bool DrawsLane(const FRow& Row, const FBook& Book)
	{
		if (Row.Path.size() < 2 || Row.Base >= 0 || Row.Buyer.empty())
		{
			return false;
		}
		if (Row.State == EState::Cut && !Row.Cut)
		{
			return false; // (a cut lane is drawn only with its mark)
		}
		const bool bSelf = Row.Rel == ERel::Self || (!Book.Viewer.empty() && Row.Buyer == Book.Viewer);
		const bool bSells = !Book.Viewer.empty() && Row.Seller == Book.Viewer;
		return bSelf || bSells || (Row.Rel == ERel::Enemy && !Row.bGeneric) || Row.bAgentSeller || Row.bAgentBuyer;
	}

	bool CarriesCrate(const FRow& Row)
	{
		return Row.State != EState::Cut && Row.Cargo.bMoved && (Shows(Row.Cargo.Sup) || Shows(Row.Cargo.Oil) || !Row.Cargo.Pp || Shows(*Row.Cargo.Pp));
	}

	void LookAt(const FBook& Book, const FLookIn& In, FLook& Out)
	{
		Out.Lanes.clear();
		Out.Tints.clear();
		Out.Seals.clear();
		Out.Plates.clear();
		Out.Trays.clear();
		auto Colour = [&In](std::string_view Tag) { return In.SrgbOf ? In.SrgbOf(Tag) : 0x808080u; };
		const int Count = static_cast<int>(Book.Rows.size());

		// Lanes: own rows first, at most MostLanes.
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (int Index = 0; Index < Count && static_cast<int>(Out.Lanes.size()) < MostLanes; ++Index)
			{
				const FRow& Row = Book.Rows[Index];
				if (!DrawsLane(Row, Book) || IsOwn(Row, Book) != (Pass == 0))
				{
					continue;
				}
				FLaneLook Look;
				Look.Row = Index;
				Look.Ink = Colour(Row.Buyer);
				Look.bCut = Row.State == EState::Cut;
				Look.bCrate = !Look.bCut && CarriesCrate(Row);
				Look.Tag = ExitText(Row, Book);
				Look.Hover = Look.bCut ? CutHover(Row, Book) : Look.bCrate ? CrateHover(Row, Book) : std::string();
				Out.Lanes.push_back(std::move(Look));
			}
		}

		// Tints: one per seller of the viewer's own rows and of the open card's row.
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (int Index = 0; Index < Count; ++Index)
			{
				const FRow& Row = Book.Rows[Index];
				const bool bOpen = !In.OpenRow.empty() && Row.Id == In.OpenRow;
				if ((Pass == 0) != bOpen || (!bOpen && !IsOwn(Row, Book)) || std::fabs(Row.Lean) < TintAt)
				{
					continue;
				}
				if (std::any_of(Out.Tints.begin(), Out.Tints.end(), [&Row](const FTintLook& Each) { return Each.Seller == Row.Seller; }))
				{
					continue;
				}
				const int Side = Row.Lean > 0.0 ? 1 : -1;
				const FNation* Buyer = Row.Buyer.empty() ? nullptr : Book.Nation(Row.Buyer);
				FTintLook Tint;
				Tint.Seller = Row.Seller;
				Tint.Row = Index;
				Tint.Ink = Buyer && Buyer->Bloc == Side ? Colour(Row.Buyer) : Colour(Side > 0 ? "GER" : "UK");
				Tint.Alpha = std::fabs(Row.Lean) >= TintStrongAt ? TintStrongA : TintA;
				Out.Tints.push_back(std::move(Tint));
			}
		}

		// Seals, at most MostSeals: those that open a card first (the open card's own; then the rows the viewer bids on,
		// sells, has an agent at the seller of, or sees bids on; then those it may bid on), then at most MostQuietSeals
		// quiet ones (named rows first), none from far off.
		auto Involved = [](const FRow& Row) { return Row.Round->Mine || Row.Round->bAccept || Row.bAgentSeller || !Row.Round->Bids.empty(); };
		int Quiet = 0;
		for (int Pass = 0; Pass < 5; ++Pass)
		{
			for (int Index = 0; Index < Count && static_cast<int>(Out.Seals.size()) < MostSeals; ++Index)
			{
				const FRow& Row = Book.Rows[Index];
				if (!Row.Round)
				{
					continue;
				}
				const bool bOpens = OpensCard(Row);
				const bool bOpen = !In.OpenRow.empty() && Row.Id == In.OpenRow;
				bool bTake = false;
				switch (Pass)
				{
				case 0: bTake = bOpens && bOpen; break;
				case 1: bTake = bOpens && !bOpen && Involved(Row); break;
				case 2: bTake = bOpens && !bOpen && !Involved(Row); break;
				default: bTake = !bOpens && !In.bFar && Row.bGeneric == (Pass == 4) && Quiet < MostQuietSeals; break;
				}
				if (!bTake)
				{
					continue;
				}
				Quiet += bOpens ? 0 : 1;
				FSealLook Seal;
				Seal.Row = Index;
				Seal.bOpens = bOpens;
				Seal.bOpen = bOpen;
				Seal.Tip = SealTip(Row);
				Out.Seals.push_back(std::move(Seal));
			}
		}

		// The senders' plate lines (not from far off), and the tray's lines.
		for (const FLegion& Legion : Book.Legions)
		{
			if (!In.bFar && std::none_of(Out.Plates.begin(), Out.Plates.end(), [&Legion](const FPlateLook& Each) { return Each.Home == Legion.Home; }))
			{
				FPlateLook Plate;
				Plate.Home = Legion.Home;
				Plate.Capital = Legion.HomeCapital;
				Plate.Line = PlateLine(Book, Legion.Home);
				if (!Plate.Line.empty())
				{
					Out.Plates.push_back(std::move(Plate));
				}
			}
		}
		TraysOf(Book, In, Out.Trays);
	}

	void TraysOf(const FBook& Book, const FLookIn& In, std::vector<FTrayLook>& Out)
	{
		// Only for a corps the viewer's own picture shows as a legion: an enemy corps not identified, or known from a stale
		// report, keeps its flag and its true strength unseen.
		Out.clear();
		for (const FLegion& Legion : Book.Legions)
		{
			if (Legion.Corps >= 0 && In.SeesLegion && In.SeesLegion(Legion.Corps, Legion.Home))
			{
				FTrayLook Tray;
				Tray.Corps = Legion.Corps;
				Tray.Home = Legion.Home;
				Tray.Flag = FlagKey(Legion.Home);
				Tray.Line = TrayLine(Legion);
				Tray.Tip = FlagTip(Legion, Book);
				Tray.Legion = LegionLine(Legion, Book);
				Out.push_back(std::move(Tray));
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// One card open

	bool FOpenCard::Press(const FBook& Book, std::string_view Id)
	{
		if (!Open.empty() && Open == Id)
		{
			Open.clear();
			return true;
		}
		const FRow* Row = RowById(Book, Id);
		if (!Row || !OpensCard(*Row))
		{
			return false;
		}
		Open.assign(Id);
		return true;
	}

	bool FOpenCard::Update(const FBook& Book)
	{
		bool bChanged = false;
		if (Book.Turn != Turn)
		{
			Turn = Book.Turn;
			if (!Open.empty())
			{
				Open.clear();
				bChanged = true;
			}
		}
		if (!Open.empty())
		{
			const FRow* Row = RowById(Book, Open);
			if (!Row || !OpensCard(*Row))
			{
				Open.clear();
				bChanged = true;
			}
		}
		if (Open.empty() && AutoTurn != Turn)
		{
			AutoTurn = Turn; // (once a month: closed by hand, it waits for the next)
			for (const FRow& Row : Book.Rows)
			{
				if (Row.Round && Row.Round->bAccept && OpensCard(Row))
				{
					Open = Row.Id;
					bChanged = true;
					break;
				}
			}
		}
		return bChanged;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The bid card

	bool StepUp(const FRound& Round, double& OutPp)
	{
		if (Round.Steps.empty())
		{
			return false;
		}
		if (!Round.Mine)
		{
			OutPp = Round.Steps.front();
			return true;
		}
		for (double Step : Round.Steps)
		{
			if (Step > *Round.Mine + 1e-9)
			{
				OutPp = Step;
				return true;
			}
		}
		return false;
	}

	bool StepDown(const FRound& Round, double& OutPp)
	{
		if (Round.Steps.empty() || !Round.Mine)
		{
			return false;
		}
		bool bFound = false;
		for (double Step : Round.Steps)
		{
			if (Step < *Round.Mine - 1e-9)
			{
				OutPp = Step;
				bFound = true;
			}
		}
		return bFound;
	}

	void CardLines(const FRow& Row, const FBook& Book, std::vector<FCardLine>& Out)
	{
		Out.clear();
		if (!Row.Round)
		{
			return;
		}
		const FRound& Round = *Row.Round;
		const bool bSeller = Round.bAccept || (!Book.Viewer.empty() && Row.Seller == Book.Viewer);
		if (!bSeller && (Round.CanBid.empty() || Round.Mine))
		{
			FCardLine Own;
			Own.Kind = ELine::Own;
			Own.Nat = Book.Viewer;
			Own.Figure = Round.Mine ? Money(*Round.Mine, Book.Scale) : std::string(EnDash);
			Own.Offer = Round.Mine ? *Round.Mine : 0.0;
			Own.bSteps = Round.CanBid.empty();
			Out.push_back(std::move(Own));
		}
		const size_t First = Out.size();
		for (const FBid& Bid : Round.Bids)
		{
			if (!Book.Viewer.empty() && Bid.Nat == Book.Viewer)
			{
				continue; // (the viewer's own bid is its own row)
			}
			FCardLine Seen;
			Seen.Kind = ELine::Seen;
			Seen.Nat = Bid.Nat;
			Seen.Offer = Bid.Offer;
			Seen.Figure = Money(Bid.Offer, Book.Scale);
			Seen.bAccept = Round.bAccept;
			Out.push_back(std::move(Seen));
		}
		std::stable_sort(Out.begin() + static_cast<std::ptrdiff_t>(First), Out.end(), [](const FCardLine& A, const FCardLine& B) { return A.Offer > B.Offer; });
		if (!bSeller && !Row.bAgentSeller)
		{
			FCardLine Others;
			Others.Kind = ELine::Others;
			Others.Figure = "?";
			Out.push_back(std::move(Others));
		}
	}

	int RingMonths(const FRound& Round, const FBook& Book)
	{
		return std::max(1, Book.RoundLen - Round.Months + 1);
	}

	bool RingShown(const FRound& Round, const FBook& Book)
	{
		return RingMonths(Round, Book) > 1;
	}

	bool LayCard(const FRow& Row, const FBook& Book, const FMeasure& Measure, double Px, FCard& Out)
	{
		Out = FCard{};
		if (!Row.Round)
		{
			return false;
		}
		const FRound& Round = *Row.Round;
		const double OX = Card::Edge + Card::PadX, OY = Card::Edge + Card::PadY;
		auto At = [Px, OX, OY](double X, double Y) { return FPt{ (OX + X) * Px, (OY + Y) * Px }; };
		auto Box = [Px, OX, OY](double X, double Y, double W, double H) { return FRect{ (OX + X) * Px, (OY + Y) * Px, W * Px, H * Px }; };
		const FInk Ink{ Kit::Ink900, 1.0f }, Dim{ Kit::DimInk, 1.0f }, Brass{ Kit::Brass700, 1.0f };
		std::vector<FDraw>& D = Out.Draws;
		D.resize(1); // (the frame, filled in once the height is known)

		// The head: the goods' sign, the caps and the seller, the ring of months left in the round.
		FDraw Sign;
		Sign.Kind = EDraw::Sign;
		Sign.At = Box(0.0, (Card::HeadH - Card::SignSize) * 0.5, Card::SignSize, Card::SignSize);
		Sign.Key = SignKey(Row.Sign);
		Sign.Fill = Brass;
		D.push_back(std::move(Sign));
		D.push_back(TextDraw(At(Card::TextX, 2.0), Caps(NounOf(Row.Sign)), CapsFace(Px), Ink, Measure));
		D.push_back(TextDraw(At(Card::TextX, Card::SellerY), Row.SellerName, SellerFace(Px), Ink, Measure));
		if (RingShown(Round, Book))
		{
			// (only for a round of more than a month: at 1 the ring is not drawn, call 2. Its numeral and "MO" stand as
			// one, centred in the ring by their ink, call 1: the figures' 0.59 em and the capitals' 0.58 over their
			// baselines, which Slate sets the ascender under each box's top; across, each by its advance, whose middle is
			// Courier Prime's figures' and capitals' ink middle to within 0.01 em)
			const double RingX = Card::InnerW - Card::RingD * 0.5, RingY = Card::RingD * 0.5;
			D.push_back(RingDraw(At(RingX, RingY), Card::RingD * Px, Card::RingW * Px, Brass));
			const std::string Months = std::to_string(RingMonths(Round, Book));
			const FFace Figures = RingFace(Px), Mo = MoFace(Px);
			const double FigureH = Ink::CourierFigureH * Figures.Size / Px, MoH = Ink::CourierCapH * Mo.Size / Px;
			const double Top = RingY - (FigureH * 0.5 + Card::RingLineStep + MoH * 0.5) * 0.5;
			const double FigureFoot = Top + FigureH, MoFoot = Top + FigureH * 0.5 + Card::RingLineStep + MoH * 0.5;
			const FPt MonthsSize = Measure(Months, Figures), MoSize = Measure("MO", Mo);
			const FPt Mid = At(RingX, 0.0);
			D.push_back(TextDraw(FPt{ Mid.X - MonthsSize.X * 0.5, (OY + FigureFoot) * Px - Ink::CourierAscent * Figures.Size }, Months, Figures, Ink, Measure));
			D.push_back(TextDraw(FPt{ Mid.X - MoSize.X * 0.5, (OY + MoFoot) * Px - Ink::CourierAscent * Mo.Size }, "MO", Mo, Ink, Measure));
		}

		// Who holds it, and the rule over the rows.
		D.push_back(TextDraw(At(0.0, Card::HeldY), HeldLine(Row, Book), HeldFace(Px), FInk{ Kit::HeldInk, 1.0f }, Measure));
		D.push_back(BoxDraw(Box(0.0, Card::RuleY, Card::InnerW, 1.0), Brass, FInk{ Kit::Brass700, 0.0f }, 0.0, 0.0));

		// The rows: the viewer's own, the bids it sees from the highest, then the others it does not see.
		std::vector<FCardLine> Lines;
		CardLines(Row, Book, Lines);
		const int Total = static_cast<int>(Lines.size());
		Out.Rows = std::min(Total, Card::MostRows);
		Out.More = Total - Out.Rows;
		const bool bAnySteps = std::any_of(Lines.begin(), Lines.end(), [](const FCardLine& Line) { return Line.bSteps; });
		// (the figures stand in one column: left of ACCEPT on the seller's card, left of the steps where the viewer bids)
		const double Column = Round.bAccept ? Card::FigureRightAccept : bAnySteps ? Card::FigureRightSteps : Card::FigureRight;
		const FFace Name = NameFace(Px);
		FFace Bold = Name;
		Bold.Style = EStyle::Bold;
		for (int K = 0; K < Out.Rows; ++K)
		{
			const FCardLine& Line = Lines[K];
			const double Top = Card::RowsY + Card::RowStep * K, Mid = Top + Card::RowH * 0.5;
			if (Line.Kind == ELine::Others)
			{
				// (no flag: whose bids they are is part of the sealed bid. A flag-shaped grey silhouette keeps the column, call
				// 3: filled, with no white and no outline, so that it never reads as a box to tick)
				D.push_back(BoxDraw(Box(0.0, Mid - Card::FlagH * 0.5, Card::FlagW, Card::FlagH), FInk{ Kit::Line, Kit::OthersFlagA }, FInk{ Kit::Line, 0.0f }, 0.0, 0.0));
			}
			else
			{
				FDraw Flag;
				Flag.Kind = EDraw::Flag;
				Flag.At = Box(0.0, Mid - Card::FlagH * 0.5, Card::FlagW, Card::FlagH);
				Flag.Key = FlagKey(Line.Nat);
				D.push_back(std::move(Flag));
			}
			const FPt FigureSize = Measure(Line.Figure, FigureFace(Px));
			const double FigureLeft = (OX + Column) * Px - FigureSize.X;
			const double Room = FigureLeft - (OX + Card::NameX + 4.0) * Px;
			if (Line.Kind == ELine::Own)
			{
				// "Britain, yours": the name in bold; where it does not fit, the bold name alone, then the tag.
				std::string Own = NameOf(Book, Line.Nat);
				std::string Yours = ", yours";
				if (Measure(Own, Bold).X + Measure(Yours, Name).X > Room)
				{
					Yours.clear();
					if (Measure(Own, Bold).X > Room)
					{
						Own = Line.Nat;
					}
				}
				const FPt OwnSize = Measure(Own, Bold);
				D.push_back(TextDraw(At(Card::NameX, Mid - OwnSize.Y / Px * 0.5), Own, Bold, Ink, Measure));
				if (!Yours.empty())
				{
					D.push_back(TextDraw(FPt{ (OX + Card::NameX) * Px + OwnSize.X, (OY + Mid) * Px - OwnSize.Y * 0.5 }, Yours, Name, Ink, Measure));
				}
			}
			else
			{
				std::string Shown = Line.Kind == ELine::Others ? std::string("Others") : NameOf(Book, Line.Nat);
				if (Line.Kind == ELine::Seen && Measure(Shown, Name).X > Room)
				{
					Shown = Line.Nat;
				}
				const FPt NameSize = Measure(Shown, Name);
				D.push_back(TextDraw(At(Card::NameX, Mid - NameSize.Y / Px * 0.5), Shown, Name, Ink, Measure));
			}
			const bool bDim = Line.Kind == ELine::Others || (Line.Kind == ELine::Own && Line.Figure == EnDash);
			D.push_back(TextDraw(FPt{ FigureLeft, (OY + Mid) * Px - FigureSize.Y * 0.5 }, Line.Figure, FigureFace(Px), bDim ? Dim : Ink, Measure));
			// (the steps are the picture's small paper keys; ACCEPT is the kit's own dark button, desk-700 with its label
			// in paper-100 and its corners at 4)
			auto Button = [&](EAct Act, double X, double W, std::string_view Label, const FFace& Face, bool bLive, double Pp, const std::string& Buyer)
			{
				const bool bKit = Act == EAct::Accept;
				const FRect Where = Box(X, Mid - Card::StepSize * 0.5, W, Card::StepSize);
				const float A = bLive ? 1.0f : 0.5f;
				D.push_back(BoxDraw(Where, FInk{ bKit ? Kit::Desk700 : Kit::Paper300, A }, FInk{ Kit::Brass700, A }, 1.0 * Px, bKit ? Card::KitRadius * Px : 0.0));
				const FPt LabelSize = Measure(Label, Face);
				const FInk LabelInk = bKit ? FInk{ Kit::Paper100, A } : bLive ? Ink : Dim;
				D.push_back(TextDraw(FPt{ Where.X + (Where.W - LabelSize.X) * 0.5, Where.Y + (Where.H - LabelSize.Y) * 0.5 }, Label, Face, LabelInk, Measure));
				FButton Pressed;
				Pressed.Act = Act;
				Pressed.At = Where;
				Pressed.bLive = bLive;
				Pressed.Pp = Pp;
				Pressed.Buyer = Buyer;
				Out.Buttons.push_back(std::move(Pressed));
			};
			if (Line.bSteps)
			{
				double Down = 0.0, Up = 0.0;
				const bool bDown = StepDown(Round, Down), bUp = StepUp(Round, Up);
				Button(EAct::Minus, Card::MinusX, Card::StepSize, Minus, StepFace(Px), bDown, bDown ? Down : 0.0, std::string());
				Button(EAct::Plus, Card::PlusX, Card::StepSize, "+", StepFace(Px), bUp, bUp ? Up : 0.0, std::string());
			}
			if (Line.bAccept)
			{
				Button(EAct::Accept, Card::AcceptX, Card::AcceptW, "ACCEPT", KitButtonFace(Px), true, Line.Offer, Line.Nat);
			}
		}
		double Y = Card::RowsY;
		const FFace Hint = HintFace(Px);
		const FInk HintInk{ Kit::HintInk, 1.0f };
		if (Total == 0)
		{
			const FPt Size = Measure("No bids yet.", HeldFace(Px));
			D.push_back(TextDraw(At(0.0, Y + Card::RowH * 0.5 - Size.Y / Px * 0.5), "No bids yet.", HeldFace(Px), HintInk, Measure));
			Y += Card::RowH;
		}
		else
		{
			Y += Card::RowStep * (Out.Rows - 1) + Card::RowH;
		}
		if (Out.More > 0)
		{
			const std::string More = "+" + std::to_string(Out.More) + " more";
			const FPt Size = Measure(More, Hint);
			Y += Card::RowStep - Card::RowH;
			D.push_back(TextDraw(At(Card::NameX, Y + Card::RowH * 0.5 - Size.Y / Px * 0.5), More, Hint, HintInk, Measure));
			Y += Card::RowH;
		}

		// The hint: why the viewer cannot bid (two lines at most) leads; then how the rivals' bids are seen.
		std::vector<std::string> Wrapped;
		const double LineH = Measure("Ag", Hint).Y / Px;
		const bool bOthers = std::any_of(Lines.begin(), Lines.end(), [](const FCardLine& Line) { return Line.Kind == ELine::Others; });
		bool bFirst = true;
		auto Paragraph = [&](const std::string& Text, int Most)
		{
			Wrap(Text, Hint, Card::InnerW * Px, Measure, Most, Wrapped);
			for (const std::string& Each : Wrapped)
			{
				if (bFirst)
				{
					Y += Card::HintGap;
					bFirst = false;
				}
				D.push_back(TextDraw(At(0.0, Y), Each, Hint, HintInk, Measure));
				Y += LineH;
				++Out.HintLines;
			}
		};
		if (!Round.CanBid.empty())
		{
			Paragraph(Round.CanBid, Card::MostCanBidLines);
		}
		if (bOthers)
		{
			Paragraph(RivalHint(Row), 4);
		}

		// The seller's foot, "Sell to nobody": the kit's quiet button on paper (its words in capitals as every kit button
		// sets them, ink-600, a brass-700 line under them), at the card's right.
		if (Round.bAccept)
		{
			const FFace Foot = KitButtonFace(Px);
			const std::string Words = Caps("Sell to nobody");
			const FPt Size = Measure(Words, Foot);
			Y += Card::FootGap;
			const FPt TopLeft{ (OX + Card::InnerW) * Px - Size.X, (OY + Y) * Px };
			D.push_back(TextDraw(TopLeft, Words, Foot, FInk{ Kit::Ink600, 1.0f }, Measure));
			const double Under = Size.Y / Px + Card::FootLine;
			D.push_back(BoxDraw(FRect{ TopLeft.X, (OY + Y + Under) * Px, Size.X, Card::FootLine * Px }, Brass, FInk{ Kit::Brass700, 0.0f }, 0.0, 0.0));
			FButton Nobody;
			Nobody.Act = EAct::Nobody;
			Nobody.At = FRect{ TopLeft.X, TopLeft.Y, Size.X, (Under + Card::FootLine) * Px };
			Nobody.bLive = true;
			Out.Buttons.push_back(std::move(Nobody));
			Y += Under + Card::FootLine;
		}

		const double H = OY + Y + Card::PadY + Card::Edge;
		Out.Size = FPt{ Card::W * Px, H * Px };
		// (no drop shadow, call 4: the paper and the date card's 1 px dark keyline, as the game's other paper cards)
		D[0] = BoxDraw(FRect{ 0.0, 0.0, Card::W * Px, H * Px }, FInk{ Kit::Paper200, 1.0f }, FInk{ Kit::Keyline, Kit::KeylineA }, Card::KeylineW, Card::Radius * Px);
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Where the card stands

	FRect SpotBox(int Spot, FPt Capital, FPt Size, double Px)
	{
		const double D = CardFrom * Px;
		return Spot == 0 ? FRect{ Capital.X - D - Size.X, Capital.Y - Size.Y * 0.5, Size.X, Size.Y } : FRect{ Capital.X + D, Capital.Y - Size.Y * 0.5, Size.X, Size.Y };
	}

	double KeepOffIntrusion(const FRect& Box, const FPlaceIn& In)
	{
		const double M = KeepOff * In.Px;
		const FRect Inside{ In.Screen.X + M, In.Screen.Y + M, std::max(0.0, In.Screen.W - 2.0 * M), std::max(0.0, In.Screen.H - 2.0 * M) };
		double Area = Box.W * Box.H - Box.Overlap(Inside);
		if (In.Frame)
		{
			for (const FRect& Piece : *In.Frame)
			{
				Area += Box.Overlap(FRect{ Piece.X - M, Piece.Y - M, Piece.W + 2.0 * M, Piece.H + 2.0 * M });
			}
		}
		return Area > 1e-6 ? Area : 0.0;
	}

	namespace
	{
		/**
		 * The card's best place on one side: the top, from level with the capital, that lies least within the keep-off
		 * (the nearest to level of those that lie least), with the capital PinInset inside the card's top and bottom.
		 * Where the card can keep off on this side, the nearest such top to level is the level one or the end of a span
		 * that the screen's band or a frame piece closes, so those are the tops tried (no list is made).
		 */
		void PlaceOnSide(int Spot, const FPlaceIn& In, FRect& OutBox, double& OutIntrusion)
		{
			const double M = KeepOff * In.Px, H = In.Size.Y;
			const double Inset = std::min(PinInset * In.Px, H * 0.5);
			const double Lo = In.Capital.Y - H + Inset, Hi = In.Capital.Y - Inset, Level = In.Capital.Y - H * 0.5;
			FRect Box = SpotBox(Spot, In.Capital, In.Size, In.Px);
			double BestTop = Level, Best = -1.0;
			auto Try = [&](double Top)
			{
				Top = std::clamp(Top, Lo, std::max(Lo, Hi));
				Box.Y = Top;
				const double Intrusion = KeepOffIntrusion(Box, In);
				if (Best < 0.0 || Intrusion < Best - 1e-6 || (Intrusion <= Best + 1e-6 && std::fabs(Top - Level) < std::fabs(BestTop - Level) - 1e-9))
				{
					Best = Intrusion;
					BestTop = Top;
				}
			};
			Try(Level);
			Try(Lo);
			Try(Hi);
			Try(In.Screen.Y + M);
			Try(In.Screen.Bottom() - M - H);
			if (In.Frame)
			{
				for (const FRect& Piece : *In.Frame)
				{
					Try(Piece.Y - M - H);
					Try(Piece.Bottom() + M);
				}
			}
			Box.Y = BestTop;
			OutBox = Box;
			OutIntrusion = Best;
		}
	}

	FPlaced PlaceCard(const FPlaceIn& In)
	{
		FPlaced Best;
		FRect Left, Right;
		double LeftIn = 0.0, RightIn = 0.0;
		PlaceOnSide(0, In, Left, LeftIn);
		PlaceOnSide(1, In, Right, RightIn);
		// (the left where it keeps off; else the right where it does; else whichever lies less within the keep-off)
		const bool bLeft = LeftIn <= 0.0 || (RightIn > 0.0 && LeftIn <= RightIn + 1e-6);
		Best.Spot = bLeft ? 0 : 1;
		Best.Card = bLeft ? Left : Right;
		Best.Intrusion = bLeft ? LeftIn : RightIn;
		Best.bFits = Best.Intrusion <= 0.0;
		// (the pin: from the card's near side, level with the capital, to PinShort short of it)
		const FPt Near{ std::clamp(In.Capital.X, Best.Card.X, Best.Card.Right()), std::clamp(In.Capital.Y, Best.Card.Y, Best.Card.Bottom()) };
		const FPt Way = Sub(In.Capital, Near);
		const double Long = Len(Way);
		Best.bPin = Long > PinShort * In.Px + 1e-9;
		Best.PinFrom = Near;
		Best.PinTo = Best.bPin ? Sub(In.Capital, Mul(Way, PinShort * In.Px / Long)) : Near;
		Best.LaidFor = In.Capital;
		Best.ScreenSize = FPt{ In.Screen.W, In.Screen.H };
		return Best;
	}

	bool NeedsPlace(const FPlaced& Was, FPt CapitalNow, const FRect& ScreenNow, double Px)
	{
		return Was.Spot < 0 || Distance(Was.LaidFor, CapitalNow) > ReplaceAfter * Px || std::fabs(Was.ScreenSize.X - ScreenNow.W) > 0.5 || std::fabs(Was.ScreenSize.Y - ScreenNow.H) > 0.5;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The lane

	void Unwrap(const FPath& In, double MinX, double MapW, FPathList& Out)
	{
		Out.Clear();
		if (In.size() < 2)
		{
			return;
		}
		FPath* Piece = &Out.Add();
		Piece->push_back(In[0]);
		for (size_t I = 1; I < In.size(); ++I)
		{
			const FPt A = In[I - 1], B = In[I];
			const double DX = B.X - A.X;
			if (MapW > 0.0 && std::fabs(DX) > MapW * 0.5)
			{
				// (across the wrap: B is nearer the other way round, at BX. The piece runs to the seam between A and BX,
				// and the next runs on from the same seam on the other side, MapW away.)
				const double Shift = DX > 0.0 ? -MapW : MapW;
				const double BX = B.X + Shift;
				const double Seam = MinX + MapW * (DX > 0.0 ? std::floor((A.X - MinX) / MapW) : std::ceil((A.X - MinX) / MapW));
				const double T = std::fabs(BX - A.X) > 1e-12 ? std::clamp((Seam - A.X) / (BX - A.X), 0.0, 1.0) : 0.0;
				const double EY = A.Y + (B.Y - A.Y) * T;
				Piece->push_back(FPt{ A.X + (BX - A.X) * T, EY });
				if (Piece->size() < 2)
				{
					Out.Count--;
				}
				Piece = &Out.Add();
				Piece->push_back(FPt{ A.X + (BX - A.X) * T - Shift, EY });
			}
			Piece->push_back(B);
		}
		if (Piece->size() < 2)
		{
			Out.Count--;
		}
	}

	void Densify(const FPath& In, double Step, FPath& Out)
	{
		Out.clear();
		const size_t Count = In.size();
		if (Count < 2)
		{
			Out.insert(Out.end(), In.begin(), In.end());
			return;
		}
		const double Each = std::max(Step, 1e-9);
		for (size_t K = 0; K + 1 < Count; ++K)
		{
			const FPt P0 = In[K > 0 ? K - 1 : 0], P1 = In[K], P2 = In[K + 1], P3 = In[std::min(K + 2, Count - 1)];
			const int Steps = std::clamp(static_cast<int>(std::ceil(Distance(P1, P2) / Each)), 1, 400);
			for (int S = 0; S < Steps; ++S)
			{
				const double T = static_cast<double>(S) / Steps, T2 = T * T, T3 = T2 * T;
				auto Axis = [T, T2, T3](double A0, double A1, double A2, double A3)
				{
					return 0.5 * ((2.0 * A1) + (A2 - A0) * T + (2.0 * A0 - 5.0 * A1 + 4.0 * A2 - A3) * T2 + (3.0 * A1 - A0 - 3.0 * A2 + A3) * T3);
				};
				Out.push_back(FPt{ Axis(P0.X, P1.X, P2.X, P3.X), Axis(P0.Y, P1.Y, P2.Y, P3.Y) });
			}
		}
		Out.push_back(In.back());
	}

	double LengthOf(const FPath& Path)
	{
		double Long = 0.0;
		for (size_t I = 1; I < Path.size(); ++I)
		{
			Long += Distance(Path[I - 1], Path[I]);
		}
		return Long;
	}

	FPt PointAlong(const FPathList& Pieces, double Along, FPt* Dir)
	{
		FPt Last{};
		FPt LastDir{ 1.0, 0.0 };
		bool bAny = false;
		double Left = std::max(Along, 0.0);
		for (size_t P = 0; P < Pieces.Num(); ++P)
		{
			const FPath& Path = Pieces[P];
			for (size_t I = 1; I < Path.size(); ++I)
			{
				const double Seg = Distance(Path[I - 1], Path[I]);
				if (Seg <= 1e-12)
				{
					continue;
				}
				LastDir = Unit(Sub(Path[I], Path[I - 1]));
				if (!bAny && Left <= 0.0)
				{
					if (Dir)
					{
						*Dir = LastDir;
					}
					return Path[I - 1];
				}
				bAny = true;
				if (Left <= Seg)
				{
					if (Dir)
					{
						*Dir = LastDir;
					}
					return Add(Path[I - 1], Mul(LastDir, Left));
				}
				Left -= Seg;
				Last = Path[I];
			}
			if (!bAny && !Path.empty())
			{
				Last = Path.front();
			}
		}
		if (Dir)
		{
			*Dir = LastDir;
		}
		return Last;
	}

	FPt PointAt(const FPathList& Pieces, double Share, FPt* Dir)
	{
		double Long = 0.0;
		for (size_t P = 0; P < Pieces.Num(); ++P)
		{
			Long += LengthOf(Pieces[P]);
		}
		return PointAlong(Pieces, std::clamp(Share, 0.0, 1.0) * Long, Dir);
	}

	void SubPath(const FPath& Path, double From, double To, FPath& Out)
	{
		Out.clear();
		if (Path.size() < 2 || To <= From + 1e-9)
		{
			return;
		}
		double S = 0.0;
		for (size_t I = 1; I < Path.size(); ++I)
		{
			const FPt A = Path[I - 1], B = Path[I];
			const double Seg = Distance(A, B);
			const double S1 = S + Seg;
			if (Seg > 1e-12 && S1 > From && S < To)
			{
				const FPt Way = Mul(Sub(B, A), 1.0 / Seg);
				if (Out.empty())
				{
					Out.push_back(Add(A, Mul(Way, std::max(From - S, 0.0))));
				}
				if (S1 < To)
				{
					Out.push_back(B);
				}
				else
				{
					Out.push_back(Add(A, Mul(Way, To - S)));
					break;
				}
			}
			S = S1;
		}
		if (Out.size() < 2)
		{
			Out.clear();
		}
	}

	void Casing(const FPath& Path, double HalfW, FStrip& Out)
	{
		Out.Outer.clear();
		Out.Inner.clear();
		for (size_t I = 0; I < Path.size(); ++I)
		{
			FPt In, Way, Lefts[2], Rights[2];
			Ways(Path, false, I, In, Way);
			const int Count = SidesAt(Path[I], In, Way, HalfW, Lefts, Rights);
			const FPt Step = I > 0 ? Sub(Path[I], Path[I - 1]) : FPt{};
			for (int K = 0; K < Count; ++K)
			{
				PutSide(Out.Outer, Lefts[K], K == 0 ? Step : FPt{});
				PutSide(Out.Inner, Rights[K], K == 0 ? Step : FPt{});
			}
		}
	}

	void Dashes(const FPathList& Pieces, double Dash, double Gap, double SkipFrom, double SkipTo, FPathList& Out)
	{
		// One pass along the pieces (a lane near the ground has thousands of dashes on thousands of points). The dashes
		// come in rising order, so a cursor (the piece, its point and the distance there) only moves forward, and each
		// dash is cut from the cursor, not from the piece's start. The sums run in the same order as SubPath's, so each
		// dash is what SubPath would cut, to the bit.
		Out.Clear();
		const double Period = Dash + Gap;
		if (Dash <= 0.0 || Period <= 1e-9)
		{
			return;
		}
		double Total = 0.0;
		for (size_t P = 0; P < Pieces.Num(); ++P)
		{
			Total += LengthOf(Pieces[P]);
		}
		size_t P = 0; // the cursor: the piece,
		double Start = 0.0; // where it starts along the whole,
		double Long = Pieces.Num() > 0 ? LengthOf(Pieces[0]) : 0.0; // its length,
		size_t I = 1; // the point the cursor's segment ends at,
		double S = 0.0; // and the distance along the piece at the point before it
		auto Cut = [&Out](const FPath& Path, size_t From, double At, double U, double V)
		{
			// (SubPath's walk, from the segment ending at Path[From], At along the piece where it starts)
			FPath& Piece = Out.Add();
			for (size_t K = From; K < Path.size(); ++K)
			{
				const FPt A = Path[K - 1], B = Path[K];
				const double Seg = Distance(A, B);
				const double At1 = At + Seg;
				if (Seg > 1e-12 && At1 > U && At < V)
				{
					const FPt Way = Mul(Sub(B, A), 1.0 / Seg);
					if (Piece.empty())
					{
						Piece.push_back(Add(A, Mul(Way, std::max(U - At, 0.0))));
					}
					if (At1 < V)
					{
						Piece.push_back(B);
					}
					else
					{
						Piece.push_back(Add(A, Mul(Way, V - At)));
						break;
					}
				}
				At = At1;
			}
			if (Piece.size() < 2)
			{
				Piece.clear();
				Out.Count--;
			}
		};
		auto Emit = [&](double U, double V)
		{
			// (pieces that end before the dash are passed for good: no later dash starts sooner)
			while (P < Pieces.Num() && Start + Long <= U)
			{
				Start += Long;
				++P;
				Long = P < Pieces.Num() ? LengthOf(Pieces[P]) : 0.0;
				I = 1;
				S = 0.0;
			}
			double QStart = Start;
			for (size_t Q = P; Q < Pieces.Num() && QStart < V; ++Q)
			{
				const FPath& Path = Pieces[Q];
				const double QLong = Q == P ? Long : LengthOf(Path);
				const double A = std::max(U, QStart), B = std::min(V, QStart + QLong);
				if (B > A + 1e-9 && Path.size() >= 2 && B - QStart > A - QStart + 1e-9)
				{
					if (Q == P)
					{
						// (the cursor moves past the segments that end before the dash starts; SubPath would pass them too)
						while (I < Path.size() && S + Distance(Path[I - 1], Path[I]) <= A - QStart)
						{
							S += Distance(Path[I - 1], Path[I]);
							++I;
						}
						Cut(Path, I, S, A - QStart, B - QStart);
					}
					else
					{
						Cut(Path, 1, 0.0, A - QStart, B - QStart);
					}
				}
				QStart += QLong;
			}
		};
		for (int K = 0; K * Period < Total; ++K)
		{
			const double U = K * Period, V = std::min(U + Dash, Total);
			if (SkipTo > SkipFrom && V > SkipFrom && U < SkipTo)
			{
				if (SkipFrom > U)
				{
					Emit(U, SkipFrom);
				}
				if (SkipTo < V)
				{
					Emit(SkipTo, V);
				}
				continue;
			}
			Emit(U, V);
		}
	}

	FGap CutGap(const FPathList& Pieces, FPt Sea, double Long)
	{
		FGap Gap;
		double Total = 0.0, Best = -1.0, BestAt = 0.0;
		for (size_t P = 0; P < Pieces.Num(); ++P)
		{
			const FPath& Path = Pieces[P];
			for (size_t I = 1; I < Path.size(); ++I)
			{
				const FPt A = Path[I - 1], B = Path[I];
				const double Seg = Distance(A, B);
				if (Seg <= 1e-12)
				{
					continue;
				}
				const double T = std::clamp(Dot(Sub(Sea, A), Sub(B, A)) / (Seg * Seg), 0.0, 1.0);
				const double Off = Distance(Sea, Add(A, Mul(Sub(B, A), T)));
				if (Best < 0.0 || Off < Best)
				{
					Best = Off;
					BestAt = Total + T * Seg;
				}
				Total += Seg;
			}
		}
		if (Best < 0.0 || Total <= Long)
		{
			return Gap;
		}
		Gap.From = BestAt - Long * 0.5;
		Gap.To = BestAt + Long * 0.5;
		if (Gap.From < 0.0)
		{
			Gap.To -= Gap.From;
			Gap.From = 0.0;
		}
		if (Gap.To > Total)
		{
			Gap.From -= Gap.To - Total;
			Gap.To = Total;
		}
		Gap.A = PointAlong(Pieces, Gap.From, &Gap.DirA);
		Gap.B = PointAlong(Pieces, Gap.To, &Gap.DirB);
		Gap.Mid = PointAlong(Pieces, (Gap.From + Gap.To) * 0.5);
		Gap.bOk = true;
		return Gap;
	}

	void Ticks(const FGap& Gap, double Long, FTick (&Out)[2])
	{
		const FPt Across[2] = { Mul(Left(Gap.DirA), Long * 0.5), Mul(Left(Gap.DirB), Long * 0.5) };
		const FPt Ends[2] = { Gap.A, Gap.B };
		for (int K = 0; K < 2; ++K)
		{
			Out[K].A = Sub(Ends[K], Across[K]);
			Out[K].B = Add(Ends[K], Across[K]);
		}
	}

	void LayLane(const FRow& Row, double Pixel, double MinX, double MapW, FLaneShape& Out)
	{
		Out.Pixel = Pixel;
		Unwrap(Row.Path, MinX, MapW, Out.Raw);
		Out.Pieces.Clear();
		Out.Length = 0.0;
		for (size_t P = 0; P < Out.Raw.Num(); ++P)
		{
			FPath& Piece = Out.Pieces.Add();
			Densify(Out.Raw[P], Lane::Step * Pixel, Piece);
			Out.Length += LengthOf(Piece);
		}
		Out.Gap = FGap{};
		Out.Ticks[0] = Out.Ticks[1] = FTick{};
		if (Row.State == EState::Cut && Row.Cut)
		{
			Out.Gap = CutGap(Out.Pieces, Row.Cut->At, Lane::CutGap * Pixel);
			if (Out.Gap.bOk)
			{
				Ticks(Out.Gap, Lane::TickLong * Pixel, Out.Ticks);
			}
		}
		// The casing's pieces: the lane without its gap.
		Out.Kept.Clear();
		double Start = 0.0;
		for (size_t P = 0; P < Out.Pieces.Num(); ++P)
		{
			const FPath& Piece = Out.Pieces[P];
			const double Long = LengthOf(Piece);
			const bool bCut = Out.Gap.bOk && Out.Gap.To > Start && Out.Gap.From < Start + Long;
			const double Spans[2][2] = { { 0.0, bCut ? Out.Gap.From - Start : Long }, { bCut ? Out.Gap.To - Start : Long, Long } };
			for (const auto& Span : Spans)
			{
				if (Span[1] > Span[0] + 1e-9)
				{
					FPath& Kept = Out.Kept.Add();
					SubPath(Piece, Span[0], Span[1], Kept);
					if (Kept.size() < 2)
					{
						Out.Kept.Count--;
					}
				}
			}
			Start += Long;
		}
		if (Out.Casing.size() < Out.Kept.Num())
		{
			Out.Casing.resize(Out.Kept.Num());
		}
		Out.CasingCount = Out.Kept.Num();
		for (size_t K = 0; K < Out.Kept.Num(); ++K)
		{
			Casing(Out.Kept[K], Lane::CasingW * 0.5 * Pixel, Out.Casing[K]);
		}
		Dashes(Out.Pieces, Lane::Dash * Pixel, Lane::Gap * Pixel, Out.Gap.bOk ? Out.Gap.From : -1.0, Out.Gap.bOk ? Out.Gap.To : -1.0, Out.Dashes);
	}

	double CrateShare(double T)
	{
		const double Rest = 1.0 - std::clamp(T, 0.0, 1.0);
		return Lane::CrateRest * (1.0 - Rest * Rest * Rest);
	}

	double CrateRunT(double Seconds) { return std::clamp(Seconds / Lane::CrateRun, 0.0, 1.0); }

	void CrateDraws(FPt Screen, uint32_t Holder, double Px, std::vector<FDraw>& Out)
	{
		const double W = Lane::CrateW * Px, H = Lane::CrateH * Px;
		Out.push_back(BoxDraw(FRect{ Screen.X - W * 0.5, Screen.Y - H * 0.5, W, H }, FInk{ Kit::Paper200, 1.0f }, FInk{ Holder, 1.0f }, Lane::CrateEdge * Px, Lane::CrateRadius * Px));
		Out.push_back(LineDraw(FPt{ Screen.X - W * 0.5, Screen.Y - 1.0 * Px }, FPt{ Screen.X + W * 0.5, Screen.Y - 1.0 * Px }, Lane::CrateBand * Px, FInk{ Holder, 1.0f }));
	}

	FDraw CutRing(FPt Screen, double Px) { return RingDraw(Screen, Lane::RingD * Px, Lane::RingLineW * Px, FInk{ Kit::Brass300, 1.0f }); }

	bool ExitTagOfPieces(const FPathList& OnScreen, const FRect& Screen, const std::vector<FRect>& Frame, FPt TextSize, double Px, FRect& Out)
	{
		for (size_t P = OnScreen.Num(); P-- > 0;)
		{
			const FPath& Piece = OnScreen[P];
			if (std::any_of(Piece.begin(), Piece.end(), [&Screen](FPt At) { return Screen.Contains(At); }))
			{
				return ExitTag(Piece, Screen, Frame, TextSize, Px, Out);
			}
		}
		return false;
	}

	bool ExitTag(const FPath& OnScreen, const FRect& Screen, const std::vector<FRect>& Frame, FPt TextSize, double Px, FRect& Out)
	{
		const size_t N = OnScreen.size();
		if (N < 2 || Screen.Contains(OnScreen.back()))
		{
			return false; // (the buyer's end is on the screen: no tag)
		}
		size_t Exit = N;
		for (size_t I = N - 1; I-- > 0;)
		{
			if (Screen.Contains(OnScreen[I]) && !Screen.Contains(OnScreen[I + 1]))
			{
				Exit = I;
				break;
			}
		}
		if (Exit == N)
		{
			return false;
		}
		const FPt A = OnScreen[Exit], B = OnScreen[Exit + 1], D = Sub(B, A);
		// Which edge it leaves by: the first bound the segment reaches.
		double TBest = 2.0;
		int Edge = 0; // 0 top, 1 right, 2 bottom, 3 left
		auto Try = [&](int Which, double Num, double Den)
		{
			if (std::fabs(Den) > 1e-12)
			{
				const double T = Num / Den;
				if (T >= 0.0 && T < TBest)
				{
					TBest = T;
					Edge = Which;
				}
			}
		};
		if (D.Y < 0.0) Try(0, Screen.Y - A.Y, D.Y);
		if (D.X > 0.0) Try(1, Screen.Right() - A.X, D.X);
		if (D.Y > 0.0) Try(2, Screen.Bottom() - A.Y, D.Y);
		if (D.X < 0.0) Try(3, Screen.X - A.X, D.X);
		// In the edge's own terms: U along it, V in from it.
		auto ToLocal = [&Screen, Edge](FPt P)
		{
			switch (Edge)
			{
			case 0: return FPt{ P.X - Screen.X, P.Y - Screen.Y };
			case 1: return FPt{ P.Y - Screen.Y, Screen.Right() - P.X };
			case 2: return FPt{ P.X - Screen.X, Screen.Bottom() - P.Y };
			default: return FPt{ P.Y - Screen.Y, P.X - Screen.X };
			}
		};
		const bool bAcross = Edge == 0 || Edge == 2;
		const double Along = bAcross ? TextSize.X : TextSize.Y, Deep = bAcross ? TextSize.Y : TextSize.X;
		const double EdgeLong = bAcross ? Screen.W : Screen.H;
		const FPt LA = ToLocal(A), LB = ToLocal(B);
		const double DU = LB.X - LA.X, DV = LB.Y - LA.Y;
		const double V0 = Lane::TagInside * Px, V1 = V0 + Deep;
		auto UAt = [&](double V) { return std::fabs(DV) > 1e-12 ? LA.X + (V - LA.Y) * DU / DV : LA.X; };
		const double Lo = std::min(UAt(V0), UAt(V1)), Hi = std::max(UAt(V0), UAt(V1));
		const double Gap = Lane::TagGap * Px;
		double U = DU >= 0.0 ? Hi + Gap : Lo - Gap - Along;
		if (U < 0.0 || U + Along > EdgeLong)
		{
			U = DU >= 0.0 ? Lo - Gap - Along : Hi + Gap; // (no room on the side it heads to: the other)
		}
		U = std::clamp(U, 0.0, std::max(EdgeLong - Along, 0.0));
		switch (Edge)
		{
		case 0: Out = FRect{ Screen.X + U, Screen.Y + V0, TextSize.X, TextSize.Y }; break;
		case 1: Out = FRect{ Screen.Right() - V1, Screen.Y + U, TextSize.X, TextSize.Y }; break;
		case 2: Out = FRect{ Screen.X + U, Screen.Bottom() - V1, TextSize.X, TextSize.Y }; break;
		default: Out = FRect{ Screen.X + V0, Screen.Y + U, TextSize.X, TextSize.Y }; break;
		}
		for (const FRect& Piece : Frame)
		{
			if (Piece.Overlaps(Out))
			{
				return false;
			}
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The tint

	void OuterEdges(const std::vector<FPath>& Rings, std::vector<FChain>& Out)
	{
		Out.clear();
		auto Key = [](FPt P) { return std::pair<long long, long long>(std::llround(P.X * 1024.0), std::llround(P.Y * 1024.0)); };
		struct FHash
		{
			size_t operator()(const std::pair<std::pair<long long, long long>, std::pair<long long, long long>>& E) const
			{
				size_t H = std::hash<long long>()(E.first.first);
				H = H * 1000003u ^ std::hash<long long>()(E.first.second);
				H = H * 1000003u ^ std::hash<long long>()(E.second.first);
				return H * 1000003u ^ std::hash<long long>()(E.second.second);
			}
		};
		using FEdgeKey = std::pair<std::pair<long long, long long>, std::pair<long long, long long>>;
		auto EdgeKey = [&Key](FPt A, FPt B)
		{
			auto KA = Key(A), KB = Key(B);
			return KA < KB ? FEdgeKey(KA, KB) : FEdgeKey(KB, KA);
		};
		auto Points = [&Key](const FPath& Ring)
		{
			size_t N = Ring.size();
			if (N >= 2 && Key(Ring.front()) == Key(Ring.back()))
			{
				--N; // (a ring closed on its first point)
			}
			return N;
		};
		std::unordered_map<FEdgeKey, int, FHash> Uses;
		for (const FPath& Ring : Rings)
		{
			const size_t N = Points(Ring);
			for (size_t I = 0; N >= 3 && I < N; ++I)
			{
				++Uses[EdgeKey(Ring[I], Ring[(I + 1) % N])];
			}
		}
		std::vector<bool> Outer;
		for (const FPath& Ring : Rings)
		{
			const size_t N = Points(Ring);
			if (N < 3)
			{
				continue;
			}
			double Area = 0.0;
			for (size_t I = 0; I < N; ++I)
			{
				const FPt A = Ring[I], B = Ring[(I + 1) % N];
				Area += A.X * B.Y - B.X * A.Y;
			}
			const double Inward = Area >= 0.0 ? 1.0 : -1.0;
			Outer.assign(N, false);
			bool bAll = true, bAny = false;
			for (size_t I = 0; I < N; ++I)
			{
				const FPt A = Ring[I], B = Ring[(I + 1) % N];
				Outer[I] = Key(A) != Key(B) && Uses[EdgeKey(A, B)] == 1;
				bAll = bAll && Outer[I];
				bAny = bAny || Outer[I];
			}
			if (!bAny)
			{
				continue;
			}
			if (bAll)
			{
				FChain Chain;
				Chain.Points.assign(Ring.begin(), Ring.begin() + static_cast<std::ptrdiff_t>(N));
				Chain.bClosed = true;
				Chain.Inward = Inward;
				Out.push_back(std::move(Chain));
				continue;
			}
			size_t Start = 0;
			while (!(Outer[Start] && !Outer[(Start + N - 1) % N]))
			{
				++Start;
			}
			FChain* Chain = nullptr;
			for (size_t J = 0; J < N; ++J)
			{
				const size_t I = (Start + J) % N;
				if (!Outer[I])
				{
					Chain = nullptr;
					continue;
				}
				if (!Chain)
				{
					Out.emplace_back();
					Chain = &Out.back();
					Chain->Inward = Inward;
					Chain->Points.push_back(Ring[I]);
				}
				Chain->Points.push_back(Ring[(I + 1) % N]);
			}
		}
	}

	void InnerStrip(const FChain& Chain, double Width, FStrip& Out)
	{
		Out.Outer.clear();
		Out.Inner.clear();
		const FPath& P = Chain.Points;
		const size_t N = P.size();
		if (N < 2)
		{
			return;
		}
		// (the country lies on the left of the chain's way where Inward is +1: its inner side is the strip's left)
		for (size_t I = 0; I < N; ++I)
		{
			FPt In, Way, Lefts[2], Rights[2];
			Ways(P, Chain.bClosed, I, In, Way);
			const int Count = SidesAt(P[I], In, Way, Width, Lefts, Rights);
			const FPt Step = I > 0 ? Sub(P[I], P[I - 1]) : FPt{};
			for (int K = 0; K < Count; ++K)
			{
				Out.Outer.push_back(P[I]);
				PutSide(Out.Inner, Chain.Inward > 0.0 ? Lefts[K] : Rights[K], K == 0 ? Step : FPt{});
			}
		}
		if (Chain.bClosed)
		{
			const FPt First = Out.Inner.front();
			Out.Outer.push_back(Out.Outer.front());
			PutSide(Out.Inner, First, Sub(P.front(), P.back()));
		}
	}

	void LayTint(const std::vector<FPath>& Rings, double Pixel, FTintShape& Out)
	{
		if (Out.Chains.empty())
		{
			OuterEdges(Rings, Out.Chains);
		}
		Out.Pixel = Pixel;
		if (Out.Strips.size() != Out.Chains.size())
		{
			Out.Strips.resize(Out.Chains.size());
		}
		for (size_t K = 0; K < Out.Chains.size(); ++K)
		{
			InnerStrip(Out.Chains[K], TintWidth * Pixel, Out.Strips[K]);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The seal

	FRect SealBox(FPt Centre, double Px)
	{
		return FRect{ Centre.X - SealMid * Px, Centre.Y - SealMid * Px, SealBoxW * Px, SealBoxH * Px };
	}

	bool SealAt(FPt Capital, double Px, const std::vector<FRect>& Frame, const std::vector<FRect>& Avoid, const std::vector<FPt>& Laid, FPt& Out)
	{
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (const FPt& Offset : SealOffsets)
			{
				const FPt Centre{ Capital.X + Offset.X * Px, Capital.Y + Offset.Y * Px };
				const FRect Box = SealBox(Centre, Px);
				bool bClear = std::all_of(Laid.begin(), Laid.end(), [&](const FPt& Other) { return Distance(Other, Centre) >= SealApart * Px - 1e-9; });
				bClear = bClear && std::none_of(Frame.begin(), Frame.end(), [&Box](const FRect& Piece) { return Piece.Overlaps(Box); });
				if (Pass == 0)
				{
					bClear = bClear && std::none_of(Avoid.begin(), Avoid.end(), [&Box](const FRect& Piece) { return Piece.Overlaps(Box); });
				}
				if (bClear)
				{
					Out = Centre;
					return true;
				}
			}
		}
		return false;
	}

	void AvoidOf(const std::vector<FRect>& Frame, bool bCardShown, const FRect& Card, std::vector<FRect>& Out)
	{
		Out.assign(Frame.begin(), Frame.end());
		if (bCardShown)
		{
			Out.push_back(Card);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The plate line, our own plate

	FPt CardSealAt(const FRect& Card)
	{
		return FPt{ Card.Right(), Card.Y };
	}

	bool SealOnCard(const FSealLook& Seal, bool bCardShown)
	{
		return bCardShown && Seal.bOpen;
	}

	bool OnCardSeal(FPt OnCard, FPt CardSize, double Px)
	{
		return SealBox(CardSealAt(FRect{ 0.0, 0.0, CardSize.X, CardSize.Y }), Px).Contains(OnCard);
	}

	void MenSign(FPt TopLeft, double Size, double Px, std::vector<FDraw>& Out)
	{
		// The picture's helmet in its 24 box: "M4.5 14a7.5 7.5 0 0 1 15 0 M3 14h18 M6.5 14v3.5h11V14".
		const double K = Size * Px / 24.0;
		auto P = [&](double X, double Y) { return FPt{ TopLeft.X + X * K, TopLeft.Y + Y * K }; };
		const FInk Brass{ Kit::Brass300, 1.0f };
		FDraw Dome;
		Dome.Kind = EDraw::Poly;
		Dome.Edge = Brass;
		Dome.EdgeW = MenStroke * Px;
		const int Steps = 12;
		for (int S = 0; S <= Steps; ++S)
		{
			const double Angle = 3.14159265358979323846 * (1.0 + static_cast<double>(S) / Steps);
			Dome.Points.push_back(P(12.0 + 7.5 * std::cos(Angle), 14.0 + 7.5 * std::sin(Angle)));
		}
		Dome.At = FRect{ TopLeft.X, TopLeft.Y, Size * Px, Size * Px };
		FDraw Brim = Dome;
		Brim.Points = { P(3.0, 14.0), P(21.0, 14.0) };
		FDraw Cup = Dome;
		Cup.Points = { P(6.5, 14.0), P(6.5, 17.5), P(17.5, 17.5), P(17.5, 14.0) };
		Out.push_back(std::move(Dome));
		Out.push_back(std::move(Brim));
		Out.push_back(std::move(Cup));
	}

	FPlateLineLay LayPlateLine(std::string_view Text, const FMeasure& Measure, double Px)
	{
		FPlateLineLay Lay;
		Lay.TextSize = Measure(Text, PlateLineFace(Px));
		const double Sign = MenSize * Px;
		const double High = std::max(Sign, Lay.TextSize.Y);
		Lay.Sign = FRect{ 0.0, (High - Sign) * 0.5, Sign, Sign };
		Lay.TextAt = FPt{ Sign + MenGap * Px, (High - Lay.TextSize.Y) * 0.5 };
		Lay.Size = FPt{ Lay.TextAt.X + Lay.TextSize.X, High };
		return Lay;
	}

	void DrawPlateLine(const FPlateLineLay& Lay, FPt At, std::string_view Text, double Px, std::vector<FDraw>& Out)
	{
		MenSign(FPt{ At.X + Lay.Sign.X, At.Y + Lay.Sign.Y }, MenSize, Px, Out);
		FDraw Words;
		Words.Kind = EDraw::Text;
		Words.At = FRect{ At.X + Lay.TextAt.X, At.Y + Lay.TextAt.Y, Lay.TextSize.X, Lay.TextSize.Y };
		Words.Text.assign(Text);
		Words.Font = PlateLineFace(Px);
		Words.Fill = FInk{ Kit::Paper300, 1.0f };
		Out.push_back(std::move(Words));
	}

	void LayOwnPlate(std::string_view Name, std::string_view Line, FPt Capital, const FMeasure& Measure, double Px, FOwnPlate& Out)
	{
		Out.Draws.clear();
		const double PadX = 8.0 * Px, PadY = 4.0 * Px;
		const FPt NameSize = Measure(Name, PlateNameFace(Px));
		const FPlateLineLay Lay = Line.empty() ? FPlateLineLay{} : LayPlateLine(Line, Measure, Px);
		const double W = std::max(NameSize.X, Lay.Size.X) + 2.0 * PadX;
		const double H = NameSize.Y + (Line.empty() ? 0.0 : Lay.Size.Y + 1.0 * Px) + 2.0 * PadY;
		Out.Plate = FRect{ Capital.X + 12.0 * Px, Capital.Y + 12.0 * Px, W, H };
		Out.Draws.push_back(BoxDraw(Out.Plate, FInk{ Kit::Desk900, Kit::PlateA }, FInk{ Kit::Brass700, 1.0f }, 1.0 * Px, 2.0 * Px));
		Out.Draws.push_back(TextDraw(FPt{ Out.Plate.X + PadX, Out.Plate.Y + PadY }, Name, PlateNameFace(Px), FInk{ Kit::Paper200, 1.0f }, Measure));
		if (!Line.empty())
		{
			DrawPlateLine(Lay, FPt{ Out.Plate.X + PadX, Out.Plate.Y + PadY + NameSize.Y + 1.0 * Px }, Line, Px, Out.Draws);
		}
	}

	bool OwnPlateShown(double Eye, const FRect& Plate, const std::vector<FRect>& Plates, const std::vector<FRect>& Frame)
	{
		auto Clear = [&Plate](const std::vector<FRect>& Others) { return std::none_of(Others.begin(), Others.end(), [&Plate](const FRect& Other) { return Other.Overlaps(Plate); }); };
		return Eye < EyeMost && Clear(Plates) && Clear(Frame);
	}
}
