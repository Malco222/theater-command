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

	std::string PhraseOfSalvoLine(const std::string& Line)
	{
		// (the log's own words for the piece, up to its verb: "Four destroyers of Force H fire torpedoes at ...", "A cruiser of
		// the Home Fleet fires torpedoes at ..."; its first letter as the rules write a name, lower case, where it opens with an
		// article or a count)
		size_t At = std::string::npos;
		for (const char* Verb : { " fire torpedoes at ", " fires torpedoes at " })
		{
			const size_t Found = Line.find(Verb);
			if (Found != std::string::npos && Found > 0 && (At == std::string::npos || Found < At)) At = Found;
		}
		if (At == std::string::npos) return std::string();
		std::string Phrase = Line.substr(0, At);
		std::string First = Phrase.substr(0, Phrase.find(' '));
		for (char& C : First) C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		if (First == "the" || ShipsByName(First + " ") >= 1) Phrase[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(Phrase[0])));
		return Phrase;
	}

	Picture PictureOf(const SeaShow::Show& Show)
	{
		Picture Pic;
		Show.TubesNow(Pic.Tubes);
		// Each piece's phrase: the later of the rules' name and its latest salvo's line, either only where its count is its
		// ships afloat now (a name that counts ships no longer there is not said).
		const std::map<int, SeaShow::Piece>& Pieces = Show.Pieces();
		auto Fits = [&Pieces](int Piece, const std::string& Phrase)
		{
			if (Phrase.empty()) return false;
			const int Counted = ShipsByName(Phrase);
			const auto It = Pieces.find(Piece);
			return Counted < 1 || (It != Pieces.end() && It->second.Ships == Counted);
		};
		const std::map<int, SeaShow::Phrase>& Named = Show.NamesGiven();
		const std::map<int, SeaShow::Phrase>& Lines = Show.SalvoLines();
		for (const auto& Pair : Named)
		{
			if (Fits(Pair.first, Pair.second.Text)) Pic.Phrases[Pair.first] = Pair.second.Text;
		}
		for (const auto& Pair : Lines)
		{
			const std::string Phrase = PhraseOfSalvoLine(Pair.second.Text);
			if (!Fits(Pair.first, Phrase)) continue;
			const auto Rules = Named.find(Pair.first);
			const bool bRulesFit = Rules != Named.end() && Pic.Phrases.count(Pair.first);
			if (!bRulesFit || Pair.second.T > Rules->second.T) Pic.Phrases[Pair.first] = Phrase;
		}
		return Pic;
	}

	Fields FieldsOf(const SeaShow::Piece& P, const Picture& Pic)
	{
		Fields F = FieldsOf(P);
		const auto Own = Pic.Tubes.find(P.Id);
		if (Own != Pic.Tubes.end())
		{
			F.SceneHeld = Own->second.Held;
			F.bHeldAway = Own->second.HeldFired > 0;
			F.Across = Own->second.Held > 0 ? Own->second.Across : -1;
			const auto Name = F.Across >= 0 ? Pic.Phrases.find(F.Across) : Pic.Phrases.end();
			if (Name != Pic.Phrases.end()) F.AcrossName = Name->second;
			// (her own piece in the way: her own ships so found, and their class, never her piece's phrase)
			if (F.Across >= 0 && F.Across == P.Id)
			{
				F.bAcrossOwn = true;
				F.OwnAcross = Own->second.OwnAcross;
				F.OwnClass = Own->second.OwnClass;
			}
		}
		if (F.ClearFrom >= 0)
		{
			const auto From = Pic.Tubes.find(F.ClearFrom);
			F.bLauncherAway = From != Pic.Tubes.end() && From->second.Fired > 0;
		}
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

	std::string CountWords(int Count)
	{
		static const char* const Words[] = { "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten" };
		if (Count < 1) return std::string();
		return Count <= 10 ? std::string(Words[Count - 1]) : std::to_string(Count);
	}

	std::string KindWord(const std::string& Class)
	{
		// (as the log names them: "a battleship of the 1st Fleet", "a cruiser of the Home Fleet", "four destroyers of Force H")
		if (Class == "bb") return "battleships";
		if (Class == "cv") return "carriers";
		if (Class == "ca") return "cruisers";
		if (Class == "dd") return "destroyers";
		if (Class == "ss") return "submarines";
		if (Class == "mv") return "merchant ships";
		return "ships";
	}

	std::string OwnPhrase(int Ships, const std::string& Class, const std::string& Her)
	{
		if (Ships < 1) return std::string();
		return CountWords(Ships) + " of " + Her + " own " + KindWord(Class);
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
		if (F.ShowsHeld())
		{
			// Held: by the rules, naming the piece they hold her for; else by the scene, naming the piece whose ship stands
			// across her held tubes' way, "the rest of" her torpedoes once some of that salvo is in the water; her own piece
			// in the way, never by its phrase: the count and kind of her own ships there, "of their own".
			Out.What = Kind::Held;
			Out.Tag = HeldTag;
			const bool bRules = F.RulesHeld();
			const bool bOwn = !bRules && F.bAcrossOwn;
			const std::string Name = bRules ? F.HeldForName : bOwn ? OwnPhrase(F.OwnAcross, F.OwnClass, Her) : F.AcrossName;
			if (!Name.empty())
			{
				// (the verb by the ships of the piece in the way: as its name counts them where it does, so the phrase and its
				// verb agree, else as the views have it afloat, else more; her own ships by their count)
				const int Counted = bOwn ? F.OwnAcross : ShipsByName(Name);
				const int Named = Counted >= 1 ? Counted : NamedShips >= 1 ? NamedShips : 2;
				const std::string Fires = F.bOrdered ? std::string("Your order stands: ") + Agree(Ships, "she fires", "they fire") : std::string(Agree(Ships, "She fires", "They fire"));
				const std::string Rest = !bRules && F.bHeldAway ? "the rest of " : "";
				Out.Line = "Holding " + Rest + Her + " torpedoes while " + Name + " " + Agree(Named, "is", "are") + " in the line of fire. " + Fires +
				           " the moment the water is clear.";
			}
			return Out;
		}
		Out.What = Kind::Clearing;
		Out.Tag = ClearingTag;
		// (fired by her launcher once some torpedo of its latest salvo is in the water; until then, about to be)
		const std::string By = F.bLauncherAway ? "fired by " : "about to be fired by ";
		if (!F.ClearFromName.empty() && F.bRouting)
		{
			// (breaking off, she has no orders to take up again: no count, no orders)
			Out.Line = "Breaking off, and turning clear of torpedoes " + By + F.ClearFromName + ".";
		}
		else if (!F.ClearFromName.empty())
		{
			// (the count to clearUntil, when she turns back on screen; never past it)
			const std::string Orders = F.bOrdered ? std::string("your") : Her;
			Out.Line = "Turning clear of torpedoes " + By + F.ClearFromName + ". " + (bMany ? "They take" : "She takes") + " up " + Orders + " orders again in " +
			           SecondsWords(SecondsLeft(F.ClearUntil, Now)) + ".";
		}
		return Out;
	}

	Said Of(const SeaShow::Piece& P, const std::map<int, SeaShow::Piece>& Pieces, const Picture& Pic, int Mine, double Now)
	{
		const Fields F = FieldsOf(P, Pic);
		int Named = -1;
		const int Meant = !F.ShowsHeld() ? -1 : F.RulesHeld() ? F.HeldFor : F.Across;
		if (Meant >= 0)
		{
			const auto It = Pieces.find(Meant);
			Named = It != Pieces.end() ? It->second.Ships : -1;
		}
		return Of(F, P.Side, Mine, P.Ships, Named, Now);
	}

	Words Write(const std::map<int, SeaShow::Piece>& Pieces, const Picture& Pic, int Mine, int Subject, double Now)
	{
		Words Out;
		if (Mine < 0) return Out;
		for (const auto& Pair : Pieces)
		{
			const Said One = Of(Pair.second, Pieces, Pic, Mine, Now);
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

	Words Write(const SeaShow::Show& Show, int Mine, int Subject, bool bOn)
	{
		if (!bOn || Mine < 0) return Words();
		return Write(Show.Pieces(), PictureOf(Show), Mine, Subject, Show.Shown());
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
