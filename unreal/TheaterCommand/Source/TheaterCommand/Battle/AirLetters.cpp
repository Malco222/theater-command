#include "Battle/AirLetters.h"

#include "Battle/AirFeed.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace AirLetters
{
	const char* const L24 = "ABCDEFGHJKLMNPQRSTUVWXYZ";

	namespace
	{
		bool IsModel(const std::string& Model)
		{
			return Model == "B17" || Model == "B-17";
		}

		bool ReadLevel(const AirFeed::Json& J, Level& Out, std::string& Why, const std::string& Name)
		{
			Out = Level();
			if (!J.IsObject())
			{
				return true; // (a level the sheet does not carry: the serial's far one)
			}
			Out.Ink = J["ink"].Str("white");
			const std::vector<AirFeed::Json>& P = J["points"].Items;
			const std::vector<AirFeed::Json>& F = J["facings"].Items;
			const std::vector<AirFeed::Json>& T = J["triangles"].Items;
			if (P.empty() || P.size() % 3 != 0 || F.size() != P.size() || T.empty() || T.size() % 3 != 0)
			{
				Why = Name + ": its points, facings or triangles are not whole";
				return false;
			}
			Out.Points.reserve(P.size());
			for (const AirFeed::Json& V : P) Out.Points.push_back(V.Num() / 1000.0);
			Out.Facings.reserve(F.size());
			for (const AirFeed::Json& V : F) Out.Facings.push_back(V.Num() / 100.0);
			const int Count = static_cast<int>(P.size() / 3);
			Out.Triangles.reserve(T.size());
			for (const AirFeed::Json& V : T)
			{
				const int I = V.Int(-1);
				if (I < 0 || I >= Count)
				{
					Why = Name + ": a triangle names a point it does not have";
					return false;
				}
				Out.Triangles.push_back(I);
			}
			return true;
		}
	}

	bool Sheet::Read(const std::string& Text, std::string& Why)
	{
		*this = Sheet();
		AirFeed::Json J;
		if (!AirFeed::Parse(Text, J, Why))
		{
			return false;
		}
		Bundle = J["bundle"].Int(0);
		ModelSha = J["model_sha256"].Str("");
		if (Bundle < 1 || J["model"].Str("") != "B17Fine")
		{
			Why = "not the finer B-17's markings";
			return false;
		}
		const AirFeed::Json& I = J["inks"];
		for (size_t K = 0; K < I.Keys.size(); ++K)
		{
			const std::string Hex = I.Values[K].Str("");
			char* End = nullptr;
			const unsigned long Rgb = std::strtoul(Hex.c_str(), &End, 16);
			if (Hex.size() != 6 || !End || *End != '\0')
			{
				Why = "ink " + I.Keys[K] + " is not a colour";
				return false;
			}
			Inks[I.Keys[K]] = static_cast<uint32_t>(Rgb);
		}
		const AirFeed::Json& Kn = J["known"];
		for (size_t K = 0; K < Kn.Keys.size(); ++K)
		{
			KnownGroup G;
			const std::string Letter = Kn.Values[K]["letter"].Str("");
			G.Letter = Letter.size() == 1 ? Letter[0] : 0;
			for (const AirFeed::Json& C : Kn.Values[K]["codes"].Items) G.Codes.push_back(C.Str(""));
			Known[Kn.Keys[K]] = G;
		}
		const AirFeed::Json& Ps = J["parts"];
		for (size_t K = 0; K < Ps.Keys.size(); ++K)
		{
			Part& P = Parts[Ps.Keys[K]];
			if (!ReadLevel(Ps.Values[K]["near"], P.Near, Why, Ps.Keys[K] + " near") || !ReadLevel(Ps.Values[K]["far"], P.Far, Why, Ps.Keys[K] + " far"))
			{
				return false;
			}
		}
		// Whole: every part a mesh of its bundle names, at both levels.
		for (const char* C = L24; *C; ++C)
		{
			Wear W;
			W.Group = *C;
			W.Letter = *C;
			W.Code = std::string(1, *C) + std::string(1, *C);
			for (const std::string& Mesh : MeshesOf(W, Bundle))
			{
				for (const std::string& Name : PartsOf(Mesh))
				{
					if (!LevelOf(Name, true) || !LevelOf(Name, false))
					{
						Why = "part " + Name + " is missing";
						return false;
					}
				}
			}
		}
		return true;
	}

	const Level* Sheet::LevelOf(const std::string& Name, bool bNear) const
	{
		const auto Found = Parts.find(Name);
		if (Found == Parts.end())
		{
			return nullptr;
		}
		const Level& L = bNear ? Found->second.Near : Found->second.Far;
		return L.Empty() ? nullptr : &L;
	}

	uint32_t Sheet::InkOf(const std::string& Name) const
	{
		const auto Found = Inks.find(Name);
		return Found == Inks.end() ? 0xF2F0E8u : Found->second;
	}

	uint64_t Fnv(const std::string& Text)
	{
		uint64_t H = 14695981039346656037ull;
		for (const char C : Text)
		{
			H ^= static_cast<uint64_t>(static_cast<unsigned char>(C));
			H *= 1099511628211ull;
		}
		return H;
	}

	bool Wears(const AirShow::PlaneOut& P)
	{
		return !P.bFighter && P.Nation == "USA" && IsModel(P.Model);
	}

	bool GroupWears(const AirShow::Group& G)
	{
		return !G.bFighter && G.Nation == "USA" && IsModel(G.Model);
	}

	std::string IdentityOf(const AirShow::Group& G)
	{
		return G.Name.empty() ? "id:" + std::to_string(G.Id) : G.Name;
	}

	std::vector<std::string> MeshesOf(const Wear& W, int Bundle)
	{
		std::vector<std::string> Out;
		if (W.Group != 0)
		{
			Out.push_back(std::string("unit.") + W.Group);
		}
		if (Bundle >= 2 && W.Code.size() == 2)
		{
			Out.push_back("code." + W.Code);
		}
		if (Bundle >= 2 && W.Letter != 0)
		{
			Out.push_back(std::string("letter.") + W.Letter);
		}
		return Out;
	}

	std::vector<std::string> PartsOf(const std::string& Mesh)
	{
		if (Mesh.size() == 6 && Mesh.compare(0, 5, "unit.") == 0)
		{
			const std::string L(1, Mesh[5]);
			return { "fin.T", "fin.T." + L, "wing.T", "wing.T." + L };
		}
		if (Mesh.size() == 7 && Mesh.compare(0, 5, "code.") == 0)
		{
			// (on both sides her squadron's first letter is the aft one seen from the side it is on: .1 the first, .2 the
			// second, as the livery lays them)
			const std::string X(1, Mesh[5]), Y(1, Mesh[6]);
			return { "code.port.1." + X, "code.port.2." + Y, "code.starboard.1." + X, "code.starboard.2." + Y };
		}
		if (Mesh.size() == 8 && Mesh.compare(0, 7, "letter.") == 0)
		{
			const std::string L(1, Mesh[7]);
			return { "letter.fin." + L, "letter.side." + L };
		}
		return {};
	}

	double SizeOf(const std::string& Mesh)
	{
		return Mesh.compare(0, 5, "unit.") == 0 ? 1.74 : 0.8;
	}

	bool Shown(double SizeM, double PixelsPerMetreAt1, double AwayM, double MinPx, bool bWas)
	{
		const double Pixels = SizeM * PixelsPerMetreAt1 / std::max(AwayM, 0.1);
		return Pixels >= MinPx * (bWas ? 0.9 : 1.0);
	}

	void Book::LetterOne(int Id, const std::string& Identity)
	{
		Lettered L;
		const auto Hist = Known.find(Identity);
		const uint64_t H = Fnv(Identity);
		if (Hist != Known.end() && Hist->second.Letter != 0)
		{
			L.Letter = Hist->second.Letter;
		}
		else
		{
			// (its hash's letter, or the next free one after it; all 24 taken, its hash's letter shared)
			const int First = static_cast<int>(H % 24u);
			L.Letter = L24[First];
			for (int K = 0; K < 24; ++K)
			{
				const char C = L24[(First + K) % 24];
				if (LettersUsed.find(C) == LettersUsed.end())
				{
					L.Letter = C;
					break;
				}
			}
		}
		++LettersUsed[L.Letter];
		if (Hist != Known.end() && Hist->second.Codes.size() == 4)
		{
			L.Codes = Hist->second.Codes;
			for (const std::string& Code : L.Codes) ++CodesUsed[Code];
		}
		else
		{
			for (int K = 0; K < 4; ++K)
			{
				const uint64_t C = Fnv(Identity + "#" + std::to_string(K));
				std::string Code;
				for (int Try = 0; Try < 576; ++Try)
				{
					const uint64_t N = (C + static_cast<uint64_t>(Try)) % 576u;
					Code = std::string(1, L24[N % 24u]) + std::string(1, L24[N / 24u]);
					if (CodesUsed.find(Code) == CodesUsed.end())
					{
						break;
					}
				}
				// (held at once, so that the group's next squadron cannot draw it too)
				L.Codes.push_back(Code);
				++CodesUsed[Code];
			}
		}
		Groups[Id] = L;
	}

	void Book::Begin(const Sheet& S, const std::map<int, AirShow::Group>& InGroups)
	{
		*this = Book();
		bBegun = true;
		Known = S.Known;
		// (the history's letters and codes held for their own groups, whether or not they fly here)
		for (const auto& Pair : Known)
		{
			if (Pair.second.Letter != 0) ++LettersUsed[Pair.second.Letter];
			for (const std::string& Code : Pair.second.Codes) ++CodesUsed[Code];
		}
		std::vector<std::pair<int, std::string>> Order;
		for (const auto& [Id, G] : InGroups)
		{
			if (GroupWears(G)) Order.emplace_back(Id, IdentityOf(G));
		}
		auto Rank = [this](const std::pair<int, std::string>& A, const std::pair<int, std::string>& B)
		{
			const bool bKa = Known.count(A.second) > 0, bKb = Known.count(B.second) > 0;
			if (bKa != bKb) return bKa;
			const uint64_t Ha = Fnv(A.second), Hb = Fnv(B.second);
			if (Ha != Hb) return Ha < Hb;
			if (A.second != B.second) return A.second < B.second;
			return A.first < B.first;
		};
		std::sort(Order.begin(), Order.end(), Rank);
		for (const auto& [Id, Identity] : Order)
		{
			// (a known group's own letters were held above: taken back so that it is counted once)
			const auto Hist = Known.find(Identity);
			if (Hist != Known.end())
			{
				if (Hist->second.Letter != 0 && --LettersUsed[Hist->second.Letter] == 0) LettersUsed.erase(Hist->second.Letter);
				for (const std::string& Code : Hist->second.Codes)
				{
					if (--CodesUsed[Code] == 0) CodesUsed.erase(Code);
				}
			}
			LetterOne(Id, Identity);
		}
	}

	void Book::Take(const std::vector<AirShow::PlaneOut>& Planes, const std::map<int, AirShow::Group>* InGroups)
	{
		if (!bBegun)
		{
			return;
		}
		// (new ones in their keys' order, whatever order the frame gives them)
		std::vector<const AirShow::PlaneOut*> Fresh;
		for (const AirShow::PlaneOut& P : Planes)
		{
			if (Wears(P) && Worn.find(P.Key) == Worn.end()) Fresh.push_back(&P);
		}
		std::sort(Fresh.begin(), Fresh.end(), [](const AirShow::PlaneOut* A, const AirShow::PlaneOut* B) { return A->Key < B->Key; });
		for (const AirShow::PlaneOut* P : Fresh)
		{
			if (Groups.find(P->Group) == Groups.end())
			{
				// (a group the show did not have at the start: lettered as it is first seen, by its name where it is given)
				std::string Identity = "id:" + std::to_string(P->Group);
				if (InGroups)
				{
					const auto Named = InGroups->find(P->Group);
					if (Named != InGroups->end()) Identity = IdentityOf(Named->second);
				}
				LetterOne(P->Group, Identity);
			}
			Lettered& G = Groups[P->Group];
			const int N = G.Made++;
			Wear W;
			W.Group = G.Letter;
			W.Code = G.Codes[static_cast<size_t>((N / 6) % 4)];
			W.Letter = L24[(N % 6) + 6 * ((N / 24) % 4)];
			Worn[P->Key] = W;
		}
	}

	const Wear* Book::Of(int Key) const
	{
		const auto Found = Worn.find(Key);
		return Found == Worn.end() ? nullptr : &Found->second;
	}

	char Book::LetterOf(int Group) const
	{
		const auto Found = Groups.find(Group);
		return Found == Groups.end() ? 0 : Found->second.Letter;
	}

	std::vector<std::string> Book::CodesOf(int Group) const
	{
		const auto Found = Groups.find(Group);
		return Found == Groups.end() ? std::vector<std::string>() : Found->second.Codes;
	}

	std::vector<std::string> Book::MeshesWorn(int Bundle) const
	{
		std::vector<std::string> Out;
		for (const auto& Pair : Worn)
		{
			for (const std::string& Mesh : MeshesOf(Pair.second, Bundle))
			{
				if (std::find(Out.begin(), Out.end(), Mesh) == Out.end()) Out.push_back(Mesh);
			}
		}
		std::sort(Out.begin(), Out.end());
		return Out;
	}
}
