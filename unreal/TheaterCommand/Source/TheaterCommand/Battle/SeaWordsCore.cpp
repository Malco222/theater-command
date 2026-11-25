#include "Battle/SeaWordsCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>

namespace SeaWords
{
	Fields FieldsOf(const SeaShow::Piece& P)
	{
		Fields F;
		F.HeldFor = P.HeldFor;
		F.HeldForName = P.HeldForName;
		F.ClearFrom = P.ClearFrom;
		F.ClearUntil = P.ClearUntil;
		F.ClearFromName = P.ClearFromName;
		F.bOrdered = P.bOrdered;
		F.bRouting = P.State == "routing";
		return F;
	}

	const char* Agree(int Ships, const char* One, const char* Many)
	{
		return Ships > 1 ? Many : One;
	}

	int SecondsLeft(double Until, double Now)
	{
		const double Left = Until - Now;
		if (!std::isfinite(Left) || Left <= 0.0) return 0;
		// (a hair under a whole second is that second: 12.0000001 left by the clock's sums is 12, not 13)
		return static_cast<int>(std::clamp(std::ceil(Left - 1e-6), 0.0, 1e6));
	}

	std::string SecondsWords(int Seconds)
	{
		return std::to_string(Seconds) + (Seconds == 1 ? " second" : " seconds");
	}

	int ShipsByName(const std::string& Name)
	{
		// (its first word, as the rules or the log write a count: "a", "an" or "one", a count in words, or in figures)
		const size_t Space = Name.find(' ');
		if (Space == std::string::npos || Space == 0) return 0;
		std::string First = Name.substr(0, Space);
		for (char& C : First) C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		if (std::all_of(First.begin(), First.end(), [](char C) { return std::isdigit(static_cast<unsigned char>(C)) != 0; }))
		{
			return First.size() <= 4 ? std::stoi(First) : 0;
		}
		static const char* const Counted[] = { "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
		                                       "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
		                                       "eighteen", "nineteen", "twenty" };
		if (First == "a" || First == "an") return 1;
		for (int K = 0; K < static_cast<int>(sizeof(Counted) / sizeof(Counted[0])); ++K)
		{
			if (First == Counted[K]) return K + 1;
		}
		return 0;
	}

	Said Of(const Fields& F, int Side, int Mine, int Ships, int NamedShips, double Now)
	{
		Said Out;
		if (Mine < 0 || Side != Mine || Ships < 1 || !F.Any()) return Out;
		// (the piece showing the words, by its own ships: she and her for one ship, they and their for a division)
		const bool bMany = Ships > 1;
		const std::string Her = Agree(Ships, "her", "their");
		if (F.Held())
		{
			Out.What = Kind::Held;
			Out.Tag = HeldTag;
			if (!F.HeldForName.empty())
			{
				// (the verb by the ships of the piece in the way: as its name counts them where it does, so the phrase and its
				// verb agree, else as the views have it afloat, else more)
				const int Counted = ShipsByName(F.HeldForName);
				const int Named = Counted >= 1 ? Counted : NamedShips >= 1 ? NamedShips : 2;
				const std::string Fires = F.bOrdered ? std::string("Your order stands: ") + Agree(Ships, "she fires", "they fire") : std::string(Agree(Ships, "She fires", "They fire"));
				Out.Line = "Holding " + Her + " torpedoes while " + F.HeldForName + " " + Agree(Named, "is", "are") + " in the line of fire. " + Fires +
				           " the moment the water is clear.";
			}
			return Out;
		}
		Out.What = Kind::Clearing;
		Out.Tag = ClearingTag;
		if (!F.ClearFromName.empty() && F.bRouting)
		{
			// (breaking off, she has no orders to take up again: no count, no orders)
			Out.Line = "Breaking off, and turning clear of torpedoes fired by " + F.ClearFromName + ".";
		}
		else if (!F.ClearFromName.empty())
		{
			const std::string Orders = F.bOrdered ? std::string("your") : Her;
			Out.Line = "Turning clear of torpedoes fired by " + F.ClearFromName + ". " + (bMany ? "They take" : "She takes") + " up " + Orders + " orders again in " +
			           SecondsWords(SecondsLeft(F.ClearUntil, Now)) + ", once the torpedoes have passed.";
		}
		return Out;
	}

	Said Of(const SeaShow::Piece& P, const std::map<int, SeaShow::Piece>& Pieces, int Mine, double Now)
	{
		const Fields F = FieldsOf(P);
		int Named = -1;
		if (F.Held())
		{
			const auto It = Pieces.find(F.HeldFor);
			Named = It != Pieces.end() ? It->second.Ships : -1;
		}
		return Of(F, P.Side, Mine, P.Ships, Named, Now);
	}

	Words Write(const std::map<int, SeaShow::Piece>& Pieces, int Mine, int Subject, double Now)
	{
		Words Out;
		if (Mine < 0) return Out;
		for (const auto& Pair : Pieces)
		{
			const Said One = Of(Pair.second, Pieces, Mine, Now);
			if (One.What == Kind::None) continue;
			Tag T;
			T.Piece = Pair.first;
			T.What = One.What;
			T.Text = One.Tag;
			Out.Tags.push_back(T);
			if (Pair.first == Subject && !One.Line.empty())
			{
				Out.Line = One.Line;
				Out.LinePiece = Subject;
				Out.LineWhat = One.What;
			}
		}
		return Out;
	}

	bool Box::Touches(const Box& Other, double Room) const
	{
		return L < Other.R + Room && Other.L < R + Room && T < Other.B + Room && Other.T < B + Room;
	}

	std::vector<Laid> Lay(const std::vector<Spot>& Spots, const std::vector<Box>& Kept, double High, double Stalk)
	{
		// The camera's subject's first, then the nearest to the eye (then by piece, so the same tags stand the same way
		// whatever order they come in).
		std::vector<size_t> Order(Spots.size());
		std::iota(Order.begin(), Order.end(), size_t(0));
		std::sort(Order.begin(), Order.end(), [&Spots](size_t First, size_t Second)
		{
			const Spot& A = Spots[First];
			const Spot& Z = Spots[Second];
			if (A.bSubject != Z.bSubject) return A.bSubject;
			const double DeepA = std::isfinite(A.Deep) ? A.Deep : 1e300, DeepZ = std::isfinite(Z.Deep) ? Z.Deep : 1e300;
			if (DeepA != DeepZ) return DeepA < DeepZ;
			if (A.Piece != Z.Piece) return A.Piece < Z.Piece;
			return First < Second;
		});
		std::vector<Laid> Out;
		// (each tag standing: its plate with the first Stalk under it, and a lifted one's stalk on down to its ship)
		std::vector<Box> Taken;
		for (const size_t Which : Order)
		{
			const Spot& S = Spots[Which];
			if (!(S.Wide > 0.0) || !std::isfinite(S.X) || !std::isfinite(S.Y) || !std::isfinite(S.Wide)) continue;
			Box Plate{ S.X - S.Wide * 0.5, S.Y - Stalk - High, S.X + S.Wide * 0.5, S.Y - Stalk };
			int Lifts = 0;
			bool bClear = false;
			for (;;)
			{
				const Box Zone{ Plate.L, Plate.T, Plate.R, Plate.B + Stalk };
				const Box* Hit = nullptr;
				for (const Box& One : Taken)
				{
					if (Zone.Touches(One, TagRoom))
					{
						Hit = &One;
						break;
					}
				}
				if (!Hit)
				{
					bClear = true;
					break;
				}
				if (Lifts >= MostLifts) break;
				// (just over the one it would touch: its stalk's first Stalk and TagGap between; always up)
				const double Rise = Plate.T - (Hit->T - TagGap - Stalk - High);
				Plate.T -= Rise;
				Plate.B -= Rise;
				++Lifts;
			}
			if (!bClear || (Lifts > 0 && Plate.T < 0.0)) continue;
			// (plate and stalk down to its ship clear of every place kept, as on its own ship)
			const Box Whole{ Plate.L, Plate.T, Plate.R, S.Y };
			if (std::any_of(Kept.begin(), Kept.end(), [&Whole](const Box& Keep) { return Whole.Touches(Keep, 0.0); })) continue;
			Taken.push_back(Box{ Plate.L, Plate.T, Plate.R, Plate.B + Stalk });
			if (Lifts > 0) Taken.push_back(Box{ S.X - 1.0, Plate.B, S.X + 1.0, S.Y });
			Laid Put;
			Put.Of = Which;
			Put.Plate = Plate;
			Put.FootX = S.X;
			Put.FootY = S.Y;
			Put.Lifts = Lifts;
			Out.push_back(Put);
		}
		// Each stalk from its plate down to its foot, in pieces round the other tags' plates it passes behind.
		for (Laid& Each : Out)
		{
			Each.Stalk.clear();
			Each.Stalk.emplace_back(Each.Plate.B, Each.FootY);
			for (const Laid& Other : Out)
			{
				if (&Other == &Each || Each.FootX < Other.Plate.L - 1.0 || Each.FootX > Other.Plate.R + 1.0) continue;
				const double CutT = Other.Plate.T - 1.0, CutB = Other.Plate.B + 1.0;
				std::vector<std::pair<double, double>> Left;
				for (const std::pair<double, double>& Bit : Each.Stalk)
				{
					if (Bit.second <= CutT || Bit.first >= CutB)
					{
						Left.push_back(Bit);
						continue;
					}
					if (Bit.first < CutT) Left.emplace_back(Bit.first, CutT);
					if (CutB < Bit.second) Left.emplace_back(CutB, Bit.second);
				}
				Each.Stalk.swap(Left);
			}
			Each.Stalk.erase(std::remove_if(Each.Stalk.begin(), Each.Stalk.end(), [](const std::pair<double, double>& Bit) { return Bit.second - Bit.first < 0.5; }),
			                 Each.Stalk.end());
		}
		return Out;
	}
}
