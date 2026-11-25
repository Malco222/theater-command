#include "Battle/AirNoseArt.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace AirNoseArt
{
	namespace
	{
		/** A number's own throw of the dice (splitmix64): the same number gives the same throw on every machine. */
		uint64_t Mix(uint64_t X)
		{
			X += 0x9E3779B97F4A7C15ull;
			X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
			X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
			return X ^ (X >> 31);
		}

		/** A throw as a share, 0 to under 1. */
		double Share(uint64_t X)
		{
			return static_cast<double>(Mix(X) >> 11) * (1.0 / 9007199254740992.0);
		}

		/** Each kind of throw its own stream: a group's shuffle, and an aircraft's bare nose. */
		constexpr uint64_t ShuffleStream = 0x4E4F5345415254ull; // ("NOSEART")
		constexpr uint64_t BareStream = 0x424152454E4F5345ull;  // ("BARENOSE")

		bool ReadThree(const AirFeed::Json& J, double Out[3])
		{
			if (!J.IsArray() || J.Count() < 3) return false;
			for (size_t K = 0; K < 3; ++K) Out[K] = J.At(K).Num(0.0);
			return true;
		}

		bool ReadSide(const AirFeed::Json& J, int Cols, int Rows, Side& Out, std::string& Why, const char* Name)
		{
			Out = Side();
			if (!J.IsObject())
			{
				Why = std::string("no ") + Name + " side";
				return false;
			}
			ReadThree(J["middle"], Out.Middle);
			ReadThree(J["normal"], Out.Normal);
			ReadThree(J["up"], Out.Up);
			ReadThree(J["right"], Out.Right);
			const AirFeed::Json& Points = J["points"];
			if (Cols < 2 || Rows < 2 || Points.Count() != static_cast<size_t>(Cols) * static_cast<size_t>(Rows))
			{
				Why = std::string("the ") + Name + " side has " + std::to_string(Points.Count()) + " points, not " + std::to_string(Cols) + " by " + std::to_string(Rows);
				return false;
			}
			Out.Points.reserve(Points.Count());
			for (const AirFeed::Json& P : Points.Items)
			{
				if (!P.IsArray() || P.Count() < 8)
				{
					Why = std::string("a point of the ") + Name + " side is not [x, y, z, nx, ny, nz, u, v]";
					return false;
				}
				Point Pt;
				for (size_t K = 0; K < 3; ++K)
				{
					Pt.At[K] = P.At(K).Num(0.0);
					Pt.Facing[K] = P.At(3 + K).Num(0.0);
				}
				Pt.U = P.At(6).Num(0.0);
				Pt.V = P.At(7).Num(0.0);
				Out.Points.push_back(Pt);
			}
			// Each cell of the grid two triangles, (0, 1, 2) and (0, 2, 3) of its corners from its top left, as the tool
			// that laid it sank them (Tools/pack_nose_art.py); each turned to face out, her skin's own facing there.
			for (int I = 0; I + 1 < Rows; ++I)
			{
				for (int K = 0; K + 1 < Cols; ++K)
				{
					const int Q[4] = { I * Cols + K, I * Cols + K + 1, (I + 1) * Cols + K + 1, (I + 1) * Cols + K };
					for (int Half = 0; Half < 2; ++Half)
					{
						const int A = Q[0], B = Q[1 + Half], C = Q[2 + Half];
						double Facing[3];
						FacingOf(Out.Points[static_cast<size_t>(A)], Out.Points[static_cast<size_t>(B)], Out.Points[static_cast<size_t>(C)], Facing);
						double Skin[3] = { 0.0, 0.0, 0.0 };
						for (const int V : { A, B, C })
							for (size_t D = 0; D < 3; ++D) Skin[D] += Out.Points[static_cast<size_t>(V)].Facing[D];
						const double Dot = Facing[0] * Skin[0] + Facing[1] * Skin[1] + Facing[2] * Skin[2];
						if (Dot >= 0.0)
						{
							Out.Triangles.insert(Out.Triangles.end(), { A, B, C });
						}
						else
						{
							Out.Triangles.insert(Out.Triangles.end(), { A, C, B });
						}
					}
				}
			}
			return true;
		}
	}

	const Entry* Index::Of(int Number) const
	{
		const auto Found = std::lower_bound(Art.begin(), Art.end(), Number, [](const Entry& E, int N) { return E.Number < N; });
		return Found != Art.end() && Found->Number == Number ? &*Found : nullptr;
	}

	std::vector<int> Index::Painted() const
	{
		std::vector<int> Out;
		for (const Entry& E : Art)
			if (E.bPainted && !E.File.empty()) Out.push_back(E.Number);
		return Out;
	}

	bool ReadIndex(const std::string& Text, Index& Out, std::string& Why)
	{
		Out = Index();
		AirFeed::Json J;
		if (!AirFeed::Parse(Text, J, Why)) return false;
		if (!J["art"].IsArray())
		{
			Why = "no art list";
			return false;
		}
		Out.BareChance = std::clamp(J["none"].Num(0.2), 0.0, 1.0);
		Out.WideM = J["size_m"].At(0).Num(2.2);
		Out.HighM = J["size_m"].At(1).Num(1.35);
		Out.W = J["px"].At(0).Int(0);
		Out.H = J["px"].At(1).Int(0);
		for (const AirFeed::Json& A : J["art"].Items)
		{
			Entry E;
			E.Number = A["number"].Int(0);
			E.Name = A["name"].Str("");
			E.bPainted = A["painted"].Truth(false);
			E.File = E.bPainted ? A["file"].Str("") : std::string();
			// (a file name only, never a way out of the folder)
			if (E.File.find('/') != std::string::npos || E.File.find('\\') != std::string::npos || E.File.find("..") != std::string::npos)
			{
				E.File.clear();
				E.bPainted = false;
			}
			if (E.Number > 0) Out.Art.push_back(E);
		}
		std::sort(Out.Art.begin(), Out.Art.end(), [](const Entry& A, const Entry& B) { return A.Number < B.Number; });
		Out.Art.erase(std::unique(Out.Art.begin(), Out.Art.end(), [](const Entry& A, const Entry& B) { return A.Number == B.Number; }), Out.Art.end());
		if (Out.Art.empty())
		{
			Why = "the art list is empty";
			return false;
		}
		return true;
	}

	bool ReadPanel(const std::string& Text, const std::string& Kind, Panel& Out, std::string& Why)
	{
		Out = Panel();
		AirFeed::Json J;
		if (!AirFeed::Parse(Text, J, Why)) return false;
		const AirFeed::Json& M = J["models"][Kind];
		if (!M.IsObject())
		{
			Why = "no panel for " + Kind;
			return false;
		}
		Out.Cols = M["cols"].Int(0);
		Out.Rows = M["rows"].Int(0);
		Out.WideM = M["size_m"].At(0).Num(2.2);
		Out.HighM = M["size_m"].At(1).Num(1.35);
		Out.LiftM = M["lift_m"].Num(0.0);
		const std::string Skin = M["skin"].Str("4F5236");
		char* End = nullptr;
		const unsigned long Rgb = std::strtoul(Skin.c_str(), &End, 16);
		if (Skin.size() == 6 && End && *End == '\0') Out.Skin = static_cast<unsigned>(Rgb);
		return ReadSide(M["port"], Out.Cols, Out.Rows, Out.Port, Why, "port") && ReadSide(M["starboard"], Out.Cols, Out.Rows, Out.Starboard, Why, "starboard");
	}

	void FacingOf(const Point& A, const Point& B, const Point& C, double Out[3])
	{
		const double P[3] = { B.At[0] - C.At[0], B.At[1] - C.At[1], B.At[2] - C.At[2] };
		const double Q[3] = { A.At[0] - C.At[0], A.At[1] - C.At[1], A.At[2] - C.At[2] };
		Out[0] = P[1] * Q[2] - P[2] * Q[1];
		Out[1] = P[2] * Q[0] - P[0] * Q[2];
		Out[2] = P[0] * Q[1] - P[1] * Q[0];
	}

	bool BothSides(int Number)
	{
		return Number > 0 && Number % 8 == 0;
	}

	bool Wears(const std::string& Nation, bool bFighter, const std::string& Model, const std::function<bool(const std::string&)>& HasPanel)
	{
		// (Brian, 25 November 2026 01:30Z: "american only"; a captured bomber in another nation's hands wears none)
		return !bFighter && Nation == "USA" && HasPanel && HasPanel(Model);
	}

	void Book::Begin(const Index& InList, uint64_t InSeed, const std::map<int, AirFeed::Sky::BomberGroup>& InBombers)
	{
		*this = Book();
		List = InList;
		Painted = List.Painted();
		Seed = InSeed;
		Bombers = InBombers;
		bBegun = true;
	}

	void Book::Pair(const std::map<int, AirShow::Group>& Groups, const std::function<bool(const std::string&)>& HasPanel)
	{
		bPaired = true;
		// A group the battle names the corps of (its view's extra.corps) flies with that corps' seats, where place.bombers
		// gives them (a nose group's) and it is that corps' only box: a carrier group's bombers split by kind fly as several
		// boxes of one corps, which the rules do not know one by one either (sim.ts, its seated boxes).
		std::map<int, int> Boxes;
		for (const auto& [Id, G] : Groups)
			if (!G.bFighter && G.Corps >= 0) ++Boxes[G.Corps];
		for (const auto& [Id, G] : Groups)
		{
			if (G.Corps < 0 || Boxes[G.Corps] != 1 || !Wears(G.Nation, G.bFighter, G.Model, HasPanel)) continue;
			const auto Found = Bombers.find(G.Corps);
			if (Found == Bombers.end() || Found->second.Style != "nose" || Found->second.Seats.empty()) continue;
			CorpsOf[Id] = G.Corps;
			How[Id] = Told::ByCorps;
		}
		// Every other group's paintings are dealt from the battle's seed. Never paired by their order, whatever the counts:
		// the battle numbers its groups in its ticket's order, which is the campaign's own order of its air groups, where a
		// newer group can stand ahead of older ones (air.ts growCommands and splitOff splice one in after its parent), while
		// place.bombers is keyed by corps; and full groups, all of a size, leave no count on the board to tell them apart (the
		// review's R1, 25 November: two groups of 80 would wear each other's crews). The rules naming each box's corps, for
		// looks only, pairs them exactly.
	}

	int Book::Usable(int Number) const
	{
		// (a seat with art 0 has a bare nose, the air thread's tallies.md; a number not yet painted, too)
		const Entry* E = Number > 0 ? List.Of(Number) : nullptr;
		return E && E->bPainted && !E->File.empty() ? Number : 0;
	}

	int Book::Deal(int Group, int Ordinal, int Per)
	{
		// From her seat, where the battle names her group's corps: her group's models in turn, as the show first lays them
		// slot by slot, each wear the first of the seats it stands for (Per aircraft a model, extra.per: the model in slot k
		// seats k * Per on, as the show's gunners name a bomber's slot, seat / Per), old hands first. A model the show lays
		// later, closing a gap in the box, falls past its seats and is bare: which of its crews it stands for, nothing says.
		if (const auto Corps = CorpsOf.find(Group); Corps != CorpsOf.end())
		{
			const std::vector<AirFeed::Sky::BomberSeat>& Seats = Bombers[Corps->second].Seats;
			const size_t Seat = static_cast<size_t>(std::max(0, Ordinal)) * static_cast<size_t>(std::max(1, Per));
			const int Number = Usable(Ordinal >= 0 && Seat < Seats.size() ? Seats[Seat].Art : 0);
			if (Number > 0) Used.insert(Number);
			return Number;
		}
		// Else dealt (names.json's assign): about one in five a bare nose, each aircraft's own throw ...
		if (Painted.empty() || Share(Seed ^ Mix(BareStream ^ (static_cast<uint64_t>(static_cast<uint32_t>(Group)) << 24) ^ static_cast<uint64_t>(static_cast<uint32_t>(Ordinal)))) < List.BareChance)
		{
			return 0;
		}
		// ... and the rest in turn from the group's own shuffle of the painted numbers, from the battle's seed and its id.
		Turn& T = Turns[Group];
		if (T.Order.empty())
		{
			T.Order = Painted;
			uint64_t State = Seed ^ Mix(ShuffleStream ^ static_cast<uint64_t>(static_cast<uint32_t>(Group)));
			for (size_t K = T.Order.size() - 1; K > 0; --K)
			{
				State = Mix(State);
				std::swap(T.Order[K], T.Order[static_cast<size_t>(State % (K + 1))]);
			}
		}
		const size_t Count = T.Order.size();
		auto TakeAt = [&T, Count, this](size_t K)
		{
			const int Number = T.Order[(T.Next + K) % Count];
			T.Next = (T.Next + K + 1) % Count;
			T.Own.insert(Number);
			Used.insert(Number);
			return Number;
		};
		// One no aircraft of the battle has had; while every one has been had, one none of the group's own has; once its
		// list runs out, the list again from where it is.
		for (size_t K = 0; K < Count; ++K)
			if (!Used.count(T.Order[(T.Next + K) % Count])) return TakeAt(K);
		for (size_t K = 0; K < Count; ++K)
			if (!T.Own.count(T.Order[(T.Next + K) % Count])) return TakeAt(K);
		T.Own.clear();
		return TakeAt(0);
	}

	void Book::Rename(const AirShow::PlaneOut& P, const std::vector<AirShow::PlaneOut>& Planes)
	{
		const int Want = Usable(P.NamedArt);
		const int Was = Art[P.Key];
		if (Want == Was) return;
		// The one that wore it hands it over and takes hers: one of her own group (another group's may wear the same
		// painting for a crew of its own), or, where both groups' paintings are dealt, of any (none is shared while any is
		// unused); never one falling already (she fell as the bomber she was named for).
		if (Want > 0)
		{
			for (const AirShow::PlaneOut& Q : Planes)
			{
				if (Q.Key == P.Key || Q.bFalling) continue;
				const auto Wore = Art.find(Q.Key);
				if (Wore == Art.end() || Wore->second != Want) continue;
				if (Q.Group != P.Group && (HowTold(Q.Group) != Told::Dealt || HowTold(P.Group) != Told::Dealt)) continue;
				Wore->second = Was;
				Changed.push_back(Q.Key);
				break;
			}
			Used.insert(Want);
		}
		Art[P.Key] = Want;
		Changed.push_back(P.Key);
	}

	void Book::Take(const std::map<int, AirShow::Group>& Groups, const std::vector<AirShow::PlaneOut>& Planes, const std::function<bool(const std::string&)>& HasPanel)
	{
		Changed.clear();
		if (!bBegun || Groups.empty()) return;
		if (!bPaired) Pair(Groups, HasPanel);
		for (const AirShow::PlaneOut& P : Planes)
		{
			if (Art.count(P.Key) || !Wears(P.Nation, P.bFighter, P.Model, HasPanel)) continue;
			const auto Its = Groups.find(P.Group);
			int& Ordinal = Made[P.Group];
			Art[P.Key] = Deal(P.Group, Ordinal, Its != Groups.end() ? Its->second.Per : 1);
			++Ordinal;
		}
		// A bomber the battle names as she falls, or falls out of her box (the loss's extra.bomber; the dispatch "That was
		// <name>" names her): she wears that bomber's painting from then, whichever model the show took for her, so that the
		// painting seen to go down is the crew's the campaign strikes off, and none flies on wearing it (the review's R2, 25
		// November). The show's choice of model is its own, the same with the nose art off as on.
		for (const AirShow::PlaneOut& P : Planes)
		{
			if (P.NamedArt < 0 || !Art.count(P.Key)) continue;
			const auto Before = Named.find(P.Key);
			if (Before != Named.end() && Before->second == P.NamedArt) continue;
			Named[P.Key] = P.NamedArt;
			Rename(P, Planes);
		}
	}

	int Book::ArtOf(int Key) const
	{
		const auto Found = Art.find(Key);
		return Found == Art.end() ? -1 : Found->second;
	}

	Told Book::HowTold(int Group) const
	{
		const auto Found = How.find(Group);
		return Found == How.end() ? Told::Dealt : Found->second;
	}

	int Book::Bare() const
	{
		int N = 0;
		for (const auto& Pair : Art) N += Pair.second == 0 ? 1 : 0;
		return N;
	}
}
