#include "Battle/AirSheen.h"

#include "Battle/LandJson.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace AirSheen
{
	namespace
	{
		/** A key that would make paint depend on the date, or carry marks: refused anywhere in the file (AirSkin's rule). */
		bool SheenKeyRefused(const std::string& Key)
		{
			std::string Low = Key;
			for (char& C : Low)
			{
				C = static_cast<char>(C >= 'A' && C <= 'Z' ? C - 'A' + 'a' : C);
			}
			return Low == "from" || Low.find("month") != std::string::npos || Low.find("year") != std::string::npos
				|| Low.find("scheme") != std::string::npos || Low.find("mark") != std::string::npos;
		}

		bool SheenKeysClean(const LandJson::Json& Node, std::string& Why)
		{
			if (Node.IsObject())
			{
				for (size_t K = 0; K < Node.Keys.size(); ++K)
				{
					if (SheenKeyRefused(Node.Keys[K]))
					{
						Why = "the key \"" + Node.Keys[K] + "\" (paint never by date, and no marks)";
						return false;
					}
					if (!SheenKeysClean(Node.Values[K], Why))
					{
						return false;
					}
				}
			}
			else if (Node.IsArray())
			{
				for (const LandJson::Json& Item : Node.Items)
				{
					if ((Item.IsObject() || Item.IsArray()) && !SheenKeysClean(Item, Why))
					{
						return false;
					}
				}
			}
			return true;
		}

		bool SheenHex(const std::string& Text, size_t Digits, uint64_t& Out)
		{
			if (Text.size() != Digits)
			{
				return false;
			}
			uint64_t V = 0;
			for (const char C : Text)
			{
				const int D = C >= '0' && C <= '9' ? C - '0' : C >= 'a' && C <= 'f' ? C - 'a' + 10 : C >= 'A' && C <= 'F' ? C - 'A' + 10 : -1;
				if (D < 0)
				{
					return false;
				}
				V = (V << 4) | static_cast<uint64_t>(D);
			}
			Out = V;
			return true;
		}

		bool SheenSha(const std::string& Sha)
		{
			uint64_t Part = 0;
			if (Sha.size() != 64)
			{
				return false;
			}
			for (size_t At = 0; At < 64; At += 16)
			{
				if (!SheenHex(Sha.substr(At, 16), 16, Part))
				{
					return false;
				}
			}
			return true;
		}

		/** A whole number 0 to 100, checked before any cast. */
		bool SheenPercent(const LandJson::Json& N, int& Out)
		{
			if (!N.IsNumber() || !(N.Value >= 0.0) || !(N.Value <= 100.0) || N.Value != std::floor(N.Value))
			{
				return false;
			}
			Out = static_cast<int>(N.Value);
			return true;
		}

		bool SheenPercents(const LandJson::Json& List, size_t Count, std::vector<uint8_t>& Out)
		{
			if (!List.IsArray() || List.Items.size() != Count)
			{
				return false;
			}
			Out.resize(Count);
			for (size_t K = 0; K < Count; ++K)
			{
				int V = 0;
				if (!SheenPercent(List.Items[K], V))
				{
					return false;
				}
				Out[K] = static_cast<uint8_t>(V);
			}
			return true;
		}

		bool SheenLevel(const LandJson::Json& Node, size_t Count, Level& Out, std::string& Why, const char* Name)
		{
			if (!Node.IsObject())
			{
				Why = std::string("no ") + Name + " mesh";
				return false;
			}
			if (!SheenPercents(Node["gloss"], Count, Out.Gloss) || !SheenPercents(Node["metal"], Count, Out.Metal))
			{
				Why = std::string(Name) + ": gloss and metal must each be one whole number 0 to 100 a point (" + std::to_string(Count) + " points)";
				return false;
			}
			return true;
		}

		double SheenSat(double V)
		{
			return std::min(std::max(V, 0.0), 1.0);
		}

		/** The material's Lerp: A + (B - A) x T (A at T = 0 and wherever A = B, exactly). */
		double SheenLerp(double A, double B, double T)
		{
			return A + (B - A) * T;
		}

		double SheenDot(const Vec3& A, const Vec3& B)
		{
			return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
		}
	}

	double GuardOf(const Vec3& N, const Vec3& E, const Vec3& S, const Tune& T)
	{
		// (the nodes' order: the sun's mirror about N, then how near the eye is to it; how far the eye looks along her)
		const double NS = SheenDot(N, S);
		const Vec3 Mirror{ N.X * (NS * 2.0) + S.X * -1.0, N.Y * (NS * 2.0) + S.Y * -1.0, N.Z * (NS * 2.0) + S.Z * -1.0 };
		const double Toward = SheenDot(E, Mirror);
		const double NE = SheenDot(N, E);
		const double Low = SheenSat((S.Z * -1.0 + T.LowSun) * 4.1667);
		const double Glare = SheenSat((Toward + -0.9) * 10.0) * T.Glare;
		const double Graze = (1.0 - SheenSat((NE + -0.15) * 4.0)) * T.Graze;
		const double Guard = SheenSat(std::max(Glare, Graze) * Low);
		return std::isfinite(Guard) ? Guard : 0.0;
	}

	Carried Carry(int Gloss, int Metal, double ShadeFactor)
	{
		const double Below = std::min(std::max(Untouched - Gloss / 100.0, 0.0), Untouched);
		const double Shade = std::min(std::max(ShadeFactor, 0.0), 1.0);
		Carried C;
		// (her glass (gloss under 12) keeps its whole depth, so that it glints in a shaded nook too: the shade would
		// otherwise carry it under the material's glass test, SheenGlassBelow, and floor it to her paint's satin)
		C.U = static_cast<float>(Below * (Gloss < 12 ? 1.0 : std::sqrt(Shade)));
		C.V = static_cast<float>(std::min(std::max(Metal / 100.0, 0.0), 1.0));
		return C;
	}

	Finish Law(double U, double V, double InkRough, double InkSpec, const Tune& T, double Guard)
	{
		const double On = SheenSat(U * T.Edge);
		const double Own = U * -1.0 + Untouched;
		const double Mixed = SheenLerp(InkRough, Own, On * T.Gain);
		const double Glass = SheenSat(((U + -0.9) + T.GlassBelow) * 20.0);
		const double Floor = (T.Floor * On) * (1.0 - Glass);
		double Rough = std::max(Mixed, Floor);
		double Spec = SheenLerp(InkSpec, T.Spec, On);
		const double Fitting = SheenSat((V + T.MetalFrom * -1.0) * 20.0);
		double Metal = (SheenSat(V * T.Metal) * Fitting) * On;
		Rough = SheenLerp(Rough, InkRough, Guard);
		Spec = SheenLerp(Spec, InkSpec, Guard);
		Metal = SheenLerp(Metal, 0.0, Guard);
		// (then the chips, as M_MapSurface's, were "Wear" ever set: the air battle never sets it, so chipped is 0 and
		// lerp(Rough, 0.34, 0) = Rough, max(Metal, 0) = Metal)
		Finish F;
		F.Rough = Rough;
		F.Spec = Spec;
		F.Metal = std::max(Metal, 0.0);
		return F;
	}

	bool ParseTune(const std::string& Words, Tune& T, std::string& Why)
	{
		struct Slot
		{
			double* Into;
			double Least, Most;
			const char* Name;
		};
		const Slot Slots[] = {
			{ &T.Gain, 0.0, 1.5, "gain" }, { &T.Floor, 0.0, 0.9, "floor" }, { &T.Spec, 0.0, 0.5, "spec" }, { &T.Metal, 0.0, 1.5, "metal" },
			{ &T.Glare, 0.0, 1.0, "glare" }, { &T.Graze, 0.0, 1.0, "graze" }, { &T.MetalFrom, 0.0, 1.0, "metalfrom" },
		};
		constexpr size_t SlotCount = sizeof(Slots) / sizeof(Slots[0]);
		Why.clear();
		size_t Index = 0, Start = 0;
		while (Start <= Words.size())
		{
			size_t End = Words.find(',', Start);
			if (End == std::string::npos)
			{
				End = Words.size();
			}
			std::string Word = Words.substr(Start, End - Start);
			const size_t First = Word.find_first_not_of(" \t");
			const size_t Last = Word.find_last_not_of(" \t");
			Word = First == std::string::npos ? std::string() : Word.substr(First, Last - First + 1);
			if (!Word.empty())
			{
				if (Index >= SlotCount)
				{
					Why += (Why.empty() ? "" : "; ") + std::string("more than seven numbers: the rest ignored");
					break;
				}
				char* Stop = nullptr;
				const double V = std::strtod(Word.c_str(), &Stop);
				const Slot& S = Slots[Index];
				if (Stop == Word.c_str() || *Stop != '\0' || !std::isfinite(V))
				{
					Why += (Why.empty() ? "" : "; ") + std::string(S.Name) + " \"" + Word + "\" is not a number: kept";
				}
				else
				{
					const double Kept = std::min(std::max(V, S.Least), S.Most);
					if (Kept != V)
					{
						Why += (Why.empty() ? "" : "; ") + std::string(S.Name) + " " + Word + " clamped to " + std::to_string(Kept);
					}
					*S.Into = Kept;
				}
			}
			++Index;
			if (End == Words.size())
			{
				break;
			}
			Start = End + 1;
		}
		return Why.empty();
	}

	bool Read(const std::string& Text, size_t NearCount, size_t FarCount, Sheen& Out, std::string& Why)
	{
		LandJson::Json Root;
		if (!LandJson::Parse(Text, Root, Why))
		{
			return false;
		}
		if (!Root.IsObject())
		{
			Why = "not an object";
			return false;
		}
		if (!SheenKeysClean(Root, Why))
		{
			return false;
		}
		Sheen S;
		S.Of = Root["sheen_of"].Str("");
		if (S.Of != "B17Fine")
		{
			Why = "sheen_of is not B17Fine";
			return false;
		}
		S.BaseSha256 = Root["base_sha256"].Str("");
		S.SourceSha256 = Root["source_sha256"].Str("");
		if (!SheenSha(S.BaseSha256) || !SheenSha(S.SourceSha256))
		{
			Why = "no base_sha256 or source_sha256";
			return false;
		}
		if (!SheenHex(Root["base_fnv64"].Str(""), 16, S.BaseFnv64))
		{
			Why = "no base_fnv64";
			return false;
		}
		const LandJson::Json& Coat = Root["coat"];
		if (!Coat.IsArray() || Coat.Items.size() != 2 || !SheenPercent(Coat.Items[0], S.CoatGloss) || !SheenPercent(Coat.Items[1], S.CoatMetal))
		{
			Why = "no coat [gloss, metal], each 0 to 100";
			return false;
		}
		if (!SheenLevel(Root["near"], NearCount, S.Near, Why, "near") || !SheenLevel(Root["far"], FarCount, S.Far, Why, "far"))
		{
			return false;
		}
		Out = std::move(S);
		return true;
	}

	Tune BareTune()
	{
		Tune T;
		T.Floor = 0.25;
		T.Glare = 0.0;
		return T;
	}

	bool ReadOwn(const std::string& Text, OwnFinish& Out, std::string& Why)
	{
		LandJson::Json Root;
		if (!LandJson::Parse(Text, Root, Why))
		{
			return false;
		}
		if (!Root.IsObject())
		{
			Why = "not an object";
			return false;
		}
		OwnFinish F;
		F.Look = BareTune();
		const char* Names[2] = { "near", "far" };
		for (int L = 0; L < 2; ++L)
		{
			const LandJson::Json& Node = Root[Names[L]];
			const LandJson::Json& Paints = Node["paints"];
			if (!Node.IsObject() || !Paints.IsArray() || Paints.Items.empty())
			{
				Why = std::string("no ") + Names[L] + " paints";
				return false;
			}
			const size_t Count = Paints.Items.size();
			if (!SheenLevel(Node, Count, L == 0 ? F.Near : F.Far, Why, Names[L]))
			{
				return false;
			}
			const LandJson::Json& Shade = Node["shade"];
			if (!Shade.IsArray() || Shade.Items.size() != Count)
			{
				Why = std::string(Names[L]) + ": shade must be one number 0 to 1 a point (" + std::to_string(Count) + " points)";
				return false;
			}
			std::vector<float>& Into = L == 0 ? F.NearShade : F.FarShade;
			Into.resize(Count);
			for (size_t K = 0; K < Count; ++K)
			{
				const LandJson::Json& N = Shade.Items[K];
				if (!N.IsNumber() || !(N.Value >= 0.0) || !(N.Value <= 1.0))
				{
					Why = std::string(Names[L]) + ": shade must be one number 0 to 1 a point";
					return false;
				}
				Into[K] = static_cast<float>(N.Value);
			}
		}
		Out = std::move(F);
		return true;
	}
}
