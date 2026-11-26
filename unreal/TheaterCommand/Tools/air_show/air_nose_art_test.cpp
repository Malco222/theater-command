// The nose art's test (Battle/AirNoseArt; release 2, Brian 25 November 2026 01:30Z: "american only"). The pack
// (Content/Data/NoseArt, Tools/pack_nose_art.py): every painting the list says is painted is there, 352 by 216 RGBA,
// the folder under 20 MB. The panel on the game's own B-17 (panels.json against her near shape, Content/Models/B17.json):
// on both sides every point of its triangles lies just off her skin (0.5 to 15 mm, never inside her, never standing off),
// all of it on her olive, each triangle facing out as the engine draws a face, the starboard panel the port one mirrored,
// and the picture reading from left to right seen from outside either side, its top up, unstretched: in the model
// file's frame and in the game's (her nose toward -X, her right hand at -Y). Then which aircraft wears which painting:
// the Bremen film played through the game's own show, where the battle does not say who flies them (each mission a
// different set from its seed, about one in five bare, none shared, Americans' bombers alone, an aircraft's for the whole
// battle); and two raids the rules made (nose_art_raid.json, tools/air-record/nose-art-raid.ts), the rules' names
// matching the list's: as the rules send them now, naming no box's corps, every group's paintings dealt and never paired
// by its order, also where the ticket's order is not by corps; each box's corps named (extra.corps, as the rules would
// send it), each group's models wearing its own corps' seats, the model in slot k seat k times per, old hands first; and
// played through its first named losses, each model that falls for one wearing the painting the loss names
// (extra.bomber), a painting changed only at such a naming, none worn twice in a group at once.
// Usage: air_nose_art_test <Content/Data/NoseArt> <Content/Models> <nose_art_raid.json> [recording.json ...]
#include "Battle/AirFeed.h"
#include "Battle/AirNoseArt.h"
#include "Battle/AirShow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
	int Bad = 0;

	void Check(bool bOk, const char* What, const std::string& Detail = std::string())
	{
		std::printf("  %s  %s%s%s\n", bOk ? "ok  " : "FAIL", What, Detail.empty() ? "" : ": ", Detail.c_str());
		if (!bOk) ++Bad;
	}

	bool ReadText(const std::string& Path, std::string& Out)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In) return false;
		Out.assign(std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>());
		return true;
	}

	/** The models the panels name, as the battle names them (AAirBattle::ModelFor's "B-17" and the rules' "B17"). */
	bool HasPanel(const std::string& Model)
	{
		return Model == "B17" || Model == "B-17";
	}

	struct V3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};
	V3 Sub(const V3& A, const V3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
	V3 Add(const V3& A, const V3& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
	V3 Mul(const V3& A, double K) { return { A.X * K, A.Y * K, A.Z * K }; }
	double Dot(const V3& A, const V3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	V3 Cross(const V3& A, const V3& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
	double Len(const V3& A) { return std::sqrt(Dot(A, A)); }
	V3 AtOf(const AirNoseArt::Point& P) { return { P.At[0], P.At[1], P.At[2] }; }
	V3 FacingOf(const AirNoseArt::Point& P) { return { P.Facing[0], P.Facing[1], P.Facing[2] }; }
	/** The model file's frame to the game's (AirParts::TakeTally's TakeSlot, less its size and middle): her nose toward
	 * -X, her right hand at -Y. */
	V3 Game(const V3& A) { return { -A.X, -A.Y, A.Z }; }

	/** Her near shape's skin about her nose (her machine's own: not her bay's doors or load). */
	struct Skin
	{
		std::vector<V3> A, B, C;
		std::vector<int> P0, P1, P2; // the paint of each triangle's corners
	};

	bool ReadSkin(const std::string& Path, double X0, double X1, Skin& Out, std::string& Why)
	{
		std::string Text;
		if (!ReadText(Path, Text))
		{
			Why = "cannot read " + Path;
			return false;
		}
		AirFeed::Json J;
		if (!AirFeed::Parse(Text, J, Why)) return false;
		const AirFeed::Json& Near = J["near"];
		const std::vector<AirFeed::Json>& Points = Near["points"].Items;
		const std::vector<AirFeed::Json>& Tris = Near["triangles"].Items;
		const std::vector<AirFeed::Json>& Paints = Near["paints"].Items;
		const std::vector<AirFeed::Json>& Part = Near["part"].Items;
		const size_t N = Points.size() / 3;
		if (N == 0 || Paints.size() != N || Tris.size() % 3 != 0)
		{
			Why = "her near shape is not whole";
			return false;
		}
		auto At = [&Points](size_t K) { return V3{ Points[3 * K].Num(), Points[3 * K + 1].Num(), Points[3 * K + 2].Num() }; };
		for (size_t T = 0; T + 2 < Tris.size(); T += 3)
		{
			const size_t I = static_cast<size_t>(Tris[T].Int()), K = static_cast<size_t>(Tris[T + 1].Int()), L = static_cast<size_t>(Tris[T + 2].Int());
			if (I >= N || K >= N || L >= N) continue;
			if (Part.size() == N && (Part[I].Int() != 0 || Part[K].Int() != 0 || Part[L].Int() != 0)) continue;
			const V3 A = At(I), B = At(K), C = At(L);
			if (std::max({ A.X, B.X, C.X }) < X0 || std::min({ A.X, B.X, C.X }) > X1) continue;
			Out.A.push_back(A);
			Out.B.push_back(B);
			Out.C.push_back(C);
			Out.P0.push_back(Paints[I].Int());
			Out.P1.push_back(Paints[K].Int());
			Out.P2.push_back(Paints[L].Int());
		}
		return !Out.A.empty();
	}

	/** Where a ray first meets her skin (Moller and Trumbore): its distance and the paint of the corner nearest it, or
	 * an infinite distance. */
	double Meet(const Skin& S, const V3& O, const V3& D, int& Paint, bool bOuter = false)
	{
		// (bOuter: the last of her the ray leaves within OuterM of her axis, as the tool laid the finer B-17's panel: she has
		// her crew and their rooms inside her, which a ray from her axis meets first)
		constexpr double OuterM = 1.8;
		double Best = bOuter ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
		Paint = -1;
		for (size_t K = 0; K < S.A.size(); ++K)
		{
			const V3 E1 = Sub(S.B[K], S.A[K]), E2 = Sub(S.C[K], S.A[K]);
			const V3 P = Cross(D, E2);
			const double Det = Dot(E1, P);
			if (std::fabs(Det) < 1e-12) continue;
			const double Inv = 1.0 / Det;
			const V3 T0 = Sub(O, S.A[K]);
			const double U = Dot(T0, P) * Inv;
			if (U < -1e-9 || U > 1.0 + 1e-9) continue;
			const V3 Q = Cross(T0, E1);
			const double V = Dot(D, Q) * Inv;
			if (V < -1e-9 || U + V > 1.0 + 1e-9) continue;
			const double T = Dot(E2, Q) * Inv;
			if (T > 1e-9 && (bOuter ? T < OuterM && T > Best : T < Best))
			{
				Best = T;
				const double W0 = 1.0 - U - V;
				Paint = W0 >= U && W0 >= V ? S.P0[K] : U >= V ? S.P1[K] : S.P2[K];
			}
		}
		return bOuter && Paint < 0 ? std::numeric_limits<double>::infinity() : Best;
	}

	/** How far a point is outside her skin (negative: inside), along the ray from her axis (her x, on her middle line)
	 * through it, as the tool that laid the panel measured it; and the paint there. */
	double Gap(const Skin& S, const V3& P, int& Paint, bool bOuter = false)
	{
		const V3 O{ P.X, 0.0, 0.0 };
		const V3 Out = Sub(P, O);
		const double R = Len(Out);
		const double T = Meet(S, O, Mul(Out, 1.0 / R), Paint, bOuter);
		return R - T;
	}

	/** What a panel may lie on, and how her skin is found: the older B-17's on her olive (paint 0) alone, her skin where a
	 * ray from her axis first meets her; the finer B-17's on the paints panels-fine.json names (her coat and its rivet
	 * heads and seam lines), her skin where the ray last leaves her. */
	struct SkinRule
	{
		std::set<int> Paints = { 0 };
		bool bOuter = false;
		std::string Words = "all on her olive (paint 0)";
	};

	void CheckSide(const char* Name, const AirNoseArt::Side& Side, const AirNoseArt::Panel& Panel, const Skin& S, double Hand, const SkinRule& Rule = SkinRule())
	{
		std::printf("%s side: %zu points, %zu triangles\n", Name, Side.Points.size(), Side.Triangles.size() / 3);
		const int Cols = Panel.Cols, Rows = Panel.Rows;
		// On her skin: each triangle's corners, the middles of its sides and its middle, 0.5 to 15 mm outside her.
		double Least = 1e9, Most = -1e9;
		std::set<int> Paints;
		int Samples = 0, Outward = 0, OutwardGame = 0, Triangles = 0;
		for (size_t T = 0; T + 2 < Side.Triangles.size(); T += 3)
		{
			const AirNoseArt::Point& A = Side.Points[static_cast<size_t>(Side.Triangles[T])];
			const AirNoseArt::Point& B = Side.Points[static_cast<size_t>(Side.Triangles[T + 1])];
			const AirNoseArt::Point& C = Side.Points[static_cast<size_t>(Side.Triangles[T + 2])];
			const V3 Pa = AtOf(A), Pb = AtOf(B), Pc = AtOf(C);
			const V3 Mid = Mul(Add(Add(Pa, Pb), Pc), 1.0 / 3.0);
			for (const V3& P : { Pa, Pb, Pc, Mul(Add(Pa, Pb), 0.5), Mul(Add(Pb, Pc), 0.5), Mul(Add(Pc, Pa), 0.5), Mid })
			{
				int Paint = -1;
				const double G = Gap(S, P, Paint, Rule.bOuter);
				Least = std::min(Least, G);
				Most = std::max(Most, G);
				Paints.insert(Paint);
				++Samples;
			}
			// (facing out as the engine draws a face: AirBattleTally.cpp's Cross(B - C, A - C) toward her skin's facing,
			// in the file's frame and in the game's)
			++Triangles;
			double F[3];
			AirNoseArt::FacingOf(A, B, C, F);
			const V3 Skinward = Add(Add(FacingOf(A), FacingOf(B)), FacingOf(C));
			Outward += Dot(V3{ F[0], F[1], F[2] }, Skinward) > 0.0 && Dot(V3{ F[0], F[1], F[2] }, { Side.Normal[0], Side.Normal[1], Side.Normal[2] }) > 0.0 ? 1 : 0;
			const V3 Ga = Game(Pa), Gb = Game(Pb), Gc = Game(Pc);
			OutwardGame += Dot(Cross(Sub(Gb, Gc), Sub(Ga, Gc)), Game(Skinward)) > 0.0 ? 1 : 0;
		}
		char Line[256];
		std::snprintf(Line, sizeof Line, "%d samples %.2f to %.2f mm off her skin (lifted %.1f mm)", Samples, Least * 1000.0, Most * 1000.0, Panel.LiftM * 1000.0);
		Check(Least >= 0.0005 && Most <= 0.015, "on her skin, never inside her nor standing off", Line);
		std::string Under;
		for (const int P : Paints) Under += (Under.empty() ? "" : ", ") + std::to_string(P);
		bool bOnAllowed = !Paints.empty();
		for (const int P : Paints) bOnAllowed = bOnAllowed && Rule.Paints.count(P) > 0;
		Check(Rule.Paints == std::set<int>{ 0 } ? Paints.size() == 1 && *Paints.begin() == 0 : bOnAllowed, Rule.Words.c_str(), "paints " + Under);
		std::snprintf(Line, sizeof Line, "%d and %d of %d", Outward, OutwardGame, Triangles);
		Check(Outward == Triangles && OutwardGame == Triangles, "every triangle facing out, in the file's frame and the game's", Line);
		// Her side: its facing out of her port side (-y in the file) or her starboard (+y).
		Check(Side.Normal[1] * Hand > 0.8, "facing out of her own side", std::to_string(Side.Normal[1]));
		// Reading: along each row, u grows the reader's right seen from outside, Cross(out, up); down each column, v
		// grows down. In the game's frame too (where the eye is).
		int RightWay = 0, Steps = 0, Down = 0, DownSteps = 0;
		double ShortRow = 1e9, LongRow = 0.0, ShortCol = 1e9, LongCol = 0.0;
		for (int I = 0; I < Rows; ++I)
		{
			double Along = 0.0;
			for (int K = 0; K + 1 < Cols; ++K)
			{
				const AirNoseArt::Point& P = Side.Points[static_cast<size_t>(I * Cols + K)];
				const AirNoseArt::Point& Q = Side.Points[static_cast<size_t>(I * Cols + K + 1)];
				const V3 Way = Sub(AtOf(Q), AtOf(P));
				const V3 Out = FacingOf(P);
				const V3 Right = Cross(Out, { 0.0, 0.0, 1.0 });
				const V3 RightGame = Cross(Game(Out), { 0.0, 0.0, 1.0 });
				const bool bGrows = Q.U > P.U;
				RightWay += (Dot(Way, Right) > 0.0) == bGrows && (Dot(Game(Way), RightGame) > 0.0) == bGrows ? 1 : 0;
				++Steps;
				Along += Len(Way);
			}
			const double Span = std::fabs(Side.Points[static_cast<size_t>(I * Cols + Cols - 1)].U - Side.Points[static_cast<size_t>(I * Cols)].U) * Panel.WideM;
			ShortRow = std::min(ShortRow, Along / Span);
			LongRow = std::max(LongRow, Along / Span);
		}
		for (int K = 0; K < Cols; ++K)
		{
			double Along = 0.0;
			for (int I = 0; I + 1 < Rows; ++I)
			{
				const AirNoseArt::Point& P = Side.Points[static_cast<size_t>(I * Cols + K)];
				const AirNoseArt::Point& Q = Side.Points[static_cast<size_t>((I + 1) * Cols + K)];
				const V3 Way = Sub(AtOf(Q), AtOf(P));
				Down += Q.V > P.V && Way.Z < 0.0 && Game(Way).Z < 0.0 ? 1 : 0;
				++DownSteps;
				Along += Len(Way);
			}
			const double Span = std::fabs(Side.Points[static_cast<size_t>((Rows - 1) * Cols + K)].V - Side.Points[static_cast<size_t>(K)].V) * Panel.HighM;
			ShortCol = std::min(ShortCol, Along / Span);
			LongCol = std::max(LongCol, Along / Span);
		}
		std::snprintf(Line, sizeof Line, "%d of %d steps along, %d of %d down", RightWay, Steps, Down, DownSteps);
		Check(RightWay == Steps && Down == DownSteps, "reads from left to right seen from outside, its top up", Line);
		std::snprintf(Line, sizeof Line, "rows %.3f to %.3f, columns %.3f to %.3f of the picture's own length", ShortRow, LongRow, ShortCol, LongCol);
		Check(ShortRow > 0.97 && LongRow < 1.05 && ShortCol > 0.98 && LongCol < 1.03, "the picture unstretched on her skin", Line);
		// Its left edge toward her nose seen from her port side, toward her tail from her starboard.
		const AirNoseArt::Point& First = Side.Points[static_cast<size_t>((Rows / 2) * Cols)];
		const AirNoseArt::Point& Last = Side.Points[static_cast<size_t>((Rows / 2) * Cols + Cols - 1)];
		const bool bLeftFore = (First.U < Last.U) == (First.At[0] > Last.At[0]);
		Check(Hand < 0.0 ? bLeftFore : !bLeftFore, Hand < 0.0 ? "its left edge toward her nose (port)" : "its left edge toward her tail (starboard)");
	}

	/** The PNG's own size and kind (its IHDR): 8 bits a channel, RGBA. */
	bool PngIs(const std::string& Path, int W, int H, size_t& Bytes)
	{
		std::string Data;
		if (!ReadText(Path, Data) || Data.size() < 33 || Data.compare(1, 3, "PNG") != 0 || Data.compare(12, 4, "IHDR") != 0) return false;
		Bytes = Data.size();
		auto Be = [&Data](size_t At)
		{
			return (static_cast<uint32_t>(static_cast<unsigned char>(Data[At])) << 24) | (static_cast<uint32_t>(static_cast<unsigned char>(Data[At + 1])) << 16)
				| (static_cast<uint32_t>(static_cast<unsigned char>(Data[At + 2])) << 8) | static_cast<uint32_t>(static_cast<unsigned char>(Data[At + 3]));
		};
		return static_cast<int>(Be(16)) == W && static_cast<int>(Be(20)) == H && Data[24] == 8 && Data[25] == 6;
	}

	/** An aircraft that wears nose art, as she began to fall: the bomber by name the battle said she was (PlaneOut's
	 * NamedArt, -1 none) and the painting she wore then. */
	struct Fell
	{
		int Key = 0, Group = 0;
		int Named = -1;
		int Art = -1;
	};

	/** A battle played through the game's own show at four frames a battle second, each frame's aircraft handed to the
	 * book as the scene hands them: whether any aircraft's painting changed but where the battle named a bomber, the
	 * groups' aircraft in turn as each was first given hers, each that fell, and whether two flying in one group ever
	 * wore the same painting at once. */
	struct Played
	{
		bool bSteady = true;                     // no painting changed but at a bomber's naming (Book::Repainted)
		std::map<int, std::vector<int>> ByGroup; // each group's paintings, its aircraft in turn, as first given
		int Others = 0;                          // aircraft that wear none
		int OthersGiven = 0;                     // ... given one all the same (none should be)
		std::vector<Fell> Falls;                 // each that wears nose art, as she began to fall
		int Repainted = 0;                       // paintings changed at a bomber's naming: hers, and the one she took it from
		int Shared = 0;                          // frames in which two flying in one group wore the same painting
	};

	Played Play(const AirFeed::Sky& Board, const std::vector<AirFeed::Frame>& Views, AirNoseArt::Book& Book, uint64_t ShowSeed, double MostT)
	{
		Played Out;
		AirShow::Show Show(ShowSeed);
		AirFeed::Apply(Board, Show);
		size_t Taken = 0;
		std::map<int, int> Seen, NamedWas;
		std::set<int> Fallen;
		auto TakeNext = [&]()
		{
			const AirFeed::Frame& F = Views[Taken++];
			Show.Take(F.Head.T, F.Groups, F.Events, F.Ships);
		};
		TakeNext();
		double T = Views.front().Head.T;
		const double EndT = std::min(Views.back().Head.T + 30.0, Views.front().Head.T + MostT);
		std::set<int> Others;
		while (T < EndT - 1e-9)
		{
			T = std::min(EndT, T + 0.25);
			while (Taken < Views.size() && Views[Taken].Head.T <= T + 1e-9) TakeNext();
			Show.Advance(T);
			Show.Effects.clear();
			Book.Take(Show.Groups(), Show.Planes(), HasPanel);
			const std::vector<int>& Changed = Book.Repainted();
			int NamedNow = 0;
			std::map<int, std::set<int>> Worn;
			bool bShared = false;
			for (const AirShow::PlaneOut& P : Show.Planes())
			{
				const int Art = Book.ArtOf(P.Key);
				if (!AirNoseArt::Wears(P.Nation, P.bFighter, P.Model, HasPanel))
				{
					Others.insert(P.Key);
					Out.OthersGiven += Art != -1 ? 1 : 0;
					continue;
				}
				const auto Before = NamedWas.find(P.Key);
				if (P.NamedArt >= 0 && (Before == NamedWas.end() || Before->second != P.NamedArt)) ++NamedNow;
				NamedWas[P.Key] = P.NamedArt;
				const auto Was = Seen.find(P.Key);
				if (Was == Seen.end())
				{
					Seen[P.Key] = Art;
					Out.ByGroup[P.Group].push_back(Art);
				}
				else if (Was->second != Art)
				{
					const bool bAsked = std::find(Changed.begin(), Changed.end(), P.Key) != Changed.end();
					Out.Repainted += bAsked ? 1 : 0;
					Out.bSteady = Out.bSteady && bAsked;
					Was->second = Art;
				}
				if (P.bFalling && Fallen.insert(P.Key).second) Out.Falls.push_back({ P.Key, P.Group, P.NamedArt, Art });
				if (!P.bFalling && Art > 0 && !Worn[P.Group].insert(Art).second) bShared = true;
			}
			// (a painting is changed only in a frame in which the battle named a bomber)
			if (!Changed.empty() && NamedNow == 0) Out.bSteady = false;
			Out.Shared += bShared ? 1 : 0;
		}
		Out.Others = static_cast<int>(Others.size());
		return Out;
	}

	/** A number as an aircraft wears it: itself where the list has it painted, else 0 (a bare nose). */
	int Usable(const AirNoseArt::Index& List, int Number)
	{
		const AirNoseArt::Entry* E = Number > 0 ? List.Of(Number) : nullptr;
		return E && E->bPainted && !E->File.empty() ? Number : 0;
	}

	/** A raid the rules made (nose_art_raid.json's "seats" or "spliced", tools/air-record/nose-art-raid.ts): its board,
	 * its views, and the rules' own corps of each bomber box (its "forces", in the order the rules number them). */
	struct Raided
	{
		AirFeed::Sky Board;
		std::vector<AirFeed::Frame> Views;
		std::vector<std::pair<int, int>> Forces; // (the box's id, its corps)
	};

	/** An object's field, to be written. */
	AirFeed::Json* FieldOf(AirFeed::Json& Object, const char* Name)
	{
		for (size_t K = 0; K < Object.Keys.size() && K < Object.Values.size(); ++K)
			if (Object.Keys[K] == Name) return &Object.Values[K];
		return nullptr;
	}

	/** Reads a raid; with Corps, each bomber box's view names its corps (extra.corps) from its forces, as the rules would
	 * send it for looks only, or as Corps gives it (an id missing from it named none). */
	bool ReadRaid(const AirFeed::Json& R, const std::map<int, int>* Corps, Raided& Out, std::string& Why)
	{
		Out = Raided();
		if (!R.IsObject() || !AirFeed::ReadSky(R["sky"], Out.Board))
		{
			Why = "no board";
			return false;
		}
		for (const AirFeed::Json& F : R["forces"].Items) Out.Forces.emplace_back(F["id"].Int(-1), F["corps"].Int(-1));
		for (const AirFeed::Json& V : R["views"].Items)
		{
			AirFeed::Json Named = V;
			if (AirFeed::Json* Units = Corps ? FieldOf(Named, "units") : nullptr)
			{
				for (AirFeed::Json& U : Units->Items)
				{
					const auto Found = Corps->find(U["id"].Int(-1));
					AirFeed::Json* X = FieldOf(U, "extra");
					if (Found == Corps->end() || !X || !X->IsObject()) continue;
					AirFeed::Json Number;
					Number.Kind = AirFeed::Json::Type::Number;
					Number.Value = Found->second;
					X->Keys.push_back("corps");
					X->Values.push_back(Number);
				}
			}
			AirFeed::Frame F;
			if (!AirFeed::ReadView(Named, Out.Board, F))
			{
				Why = "a view not read";
				return false;
			}
			Out.Views.push_back(std::move(F));
		}
		if (Out.Views.empty() || Out.Forces.empty())
		{
			Why = "no views or no forces";
			return false;
		}
		return true;
	}

	std::string ListOf(const std::vector<int>& Art, const AirNoseArt::Index& List, size_t Most)
	{
		std::string Out;
		for (size_t K = 0; K < Art.size() && K < Most; ++K)
		{
			const AirNoseArt::Entry* E = Art[K] > 0 ? List.Of(Art[K]) : nullptr;
			Out += (K ? ", " : "") + (Art[K] == 0 ? std::string("bare") : std::to_string(Art[K]) + (E ? " " + E->Name : std::string()) + (AirNoseArt::BothSides(Art[K]) ? " (both sides)" : ""));
		}
		return Out;
	}

	/** A group of twelve dealt on its own, mission after mission: the share of bare noses, and on average how much of a
	 * mission's set flew in the one before. */
	double BareShare(const AirNoseArt::Index& List, int Battles, double& Overlap)
	{
		int Bare = 0, All = 0, Same = 0, Painted = 0;
		Overlap = 0.0;
		std::vector<int> Before;
		for (int Battle = 0; Battle < Battles; ++Battle)
		{
			AirNoseArt::Book Book;
			Book.Begin(List, 1000003ull * static_cast<uint64_t>(Battle + 1), {});
			std::map<int, AirShow::Group> Groups;
			AirShow::Group G;
			G.Id = 2;
			G.Side = 0;
			G.bFighter = false;
			G.Nation = "USA";
			G.Model = "B17";
			G.N = 48;
			Groups[G.Id] = G;
			std::vector<AirShow::PlaneOut> Planes;
			for (int K = 0; K < 12; ++K)
			{
				AirShow::PlaneOut P;
				P.Key = K + 1;
				P.Group = 2;
				P.bFighter = false;
				P.Nation = "USA";
				P.Model = "B17";
				Planes.push_back(P);
			}
			Book.Take(Groups, Planes, HasPanel);
			std::vector<int> Now;
			for (const AirShow::PlaneOut& P : Planes)
			{
				const int Art = Book.ArtOf(P.Key);
				Bare += Art == 0 ? 1 : 0;
				++All;
				if (Art > 0) Now.push_back(Art);
			}
			if (!Before.empty())
			{
				for (const int N : Now) Same += std::find(Before.begin(), Before.end(), N) != Before.end() ? 1 : 0;
				Painted += static_cast<int>(Now.size());
			}
			Before = Now;
		}
		Overlap = static_cast<double>(Same) / std::max(1, Painted);
		return static_cast<double>(Bare) / std::max(1, All);
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: air_nose_art_test <Content/Data/NoseArt> <Content/Models> <nose_art_raid.json> [recording.json ...]\n");
		return 2;
	}
	const std::string Folder = Argv[1], Models = Argv[2], Raid = Argv[3];

	// The pack.
	std::printf("The pack (%s)\n", Folder.c_str());
	std::string Text, Why;
	AirNoseArt::Index List;
	const bool bIndex = ReadText(Folder + "/index.json", Text) && AirNoseArt::ReadIndex(Text, List, Why);
	Check(bIndex, "its index read", Why);
	if (!bIndex) return 1;
	size_t Total = Text.size(), Files = 0, Good = 0;
	for (const AirNoseArt::Entry& E : List.Art)
	{
		if (!E.bPainted) continue;
		++Files;
		size_t Bytes = 0;
		if (PngIs(Folder + "/" + E.File, List.W, List.H, Bytes)) ++Good;
		Total += Bytes;
	}
	std::string Panels;
	ReadText(Folder + "/panels.json", Panels);
	Total += Panels.size();
	char Line[512];
	std::snprintf(Line, sizeof Line, "%zu of %zu numbers painted, %zu of them %dx%d RGBA; the folder %.2f MB", Files, List.Art.size(), Good, List.W, List.H, Total / 1048576.0);
	Check(List.Art.size() == 200 && Files == 200 && Good == Files && List.W == 352 && List.H == 216, "every painting packed", Line);
	Check(Total < 20u * 1024u * 1024u, "the folder under 20 MB", Line);

	// The panel on her skin.
	std::printf("The panel on the B-17 (panels.json against Content/Models/B17.json's near shape)\n");
	AirNoseArt::Panel Panel;
	const bool bPanel = AirNoseArt::ReadPanel(Panels, "B17", Panel, Why);
	Check(bPanel, "the B-17's panel read", Why);
	Skin S;
	const bool bSkin = ReadSkin(Models + "/B17.json", 4.0, 8.2, S, Why);
	Check(bSkin, "her near shape read", Why);
	AirNoseArt::Panel None;
	Check(!AirNoseArt::ReadPanel(Panels, "Bomber", None, Why), "no panel for a model that has none (the plain bomber)", Why);
	if (bPanel && bSkin)
	{
		CheckSide("Port", Panel.Port, Panel, S, -1.0);
		CheckSide("Starboard", Panel.Starboard, Panel, S, 1.0);
		// The starboard panel the port one mirrored across her middle, its picture read the other way along.
		bool bMirror = Panel.Port.Points.size() == Panel.Starboard.Points.size();
		for (size_t K = 0; bMirror && K < Panel.Port.Points.size(); ++K)
		{
			const AirNoseArt::Point& P = Panel.Port.Points[K];
			const AirNoseArt::Point& Q = Panel.Starboard.Points[K];
			bMirror = std::fabs(P.At[0] - Q.At[0]) < 1e-6 && std::fabs(P.At[1] + Q.At[1]) < 1e-6 && std::fabs(P.At[2] - Q.At[2]) < 1e-6
				&& std::fabs(P.U + Q.U - 1.0) < 1e-6 && std::fabs(P.V - Q.V) < 1e-6;
		}
		Check(bMirror, "the starboard panel the port one mirrored, its picture read the other way along");
	}

	// The finer B-17's panel (the Battle rules card's "Finer B-17", row b17Fine): in a file of its own, panels-fine.json,
	// laid on Content/Models/B17Fine.json's near shape by Tools/pack_nose_art.py --fine; panels.json keeps none for her.
	std::string FinePanels;
	const bool bFineModel = static_cast<bool>(std::ifstream(Models + "/B17Fine.json"));
	const bool bFineFile = ReadText(Folder + "/panels-fine.json", FinePanels);
	if (bFineModel || bFineFile)
	{
		std::printf("The panel on the finer B-17 (panels-fine.json against Content/Models/B17Fine.json's near shape)\n");
		Check(bFineModel && bFineFile, "the finer B-17's file and her panels' file both there");
		AirNoseArt::Panel Fine;
		const bool bFine = bFineFile && AirNoseArt::ReadPanel(FinePanels, "B17Fine", Fine, Why);
		Check(bFine, "her panel read", Why);
		AirNoseArt::Panel Other;
		Check(!AirNoseArt::ReadPanel(Panels, "B17Fine", Other, Why) && !AirNoseArt::ReadPanel(FinePanels, "B17", Other, Why),
			"each B-17's panel in its own file: none for her in panels.json, none for the older one in panels-fine.json");
		// (the paints she may lie on, as the file names them: her coat, its rivet heads and its seam lines, by her own names)
		SkinRule Rule;
		Rule.bOuter = true;
		Rule.Paints.clear();
		AirFeed::Json J;
		std::string Listed;
		if (bFineFile && AirFeed::Parse(FinePanels, J, Why))
		{
			for (const AirFeed::Json& P : J["models"]["B17Fine"]["skin_paints"].Items)
			{
				Rule.Paints.insert(P.Int(-1));
				Listed += (Listed.empty() ? "" : ", ") + std::to_string(P.Int(-1));
			}
		}
		std::string ModelText;
		AirFeed::Json M;
		std::vector<std::string> Names;
		if (ReadText(Models + "/B17Fine.json", ModelText) && AirFeed::Parse(ModelText, M, Why))
		{
			for (const AirFeed::Json& N : M["paint_names"].Items) Names.push_back(N.Str(""));
		}
		std::string Named;
		bool bCoat = !Rule.Paints.empty();
		for (const int P : Rule.Paints)
		{
			const std::string Name = P >= 0 && static_cast<size_t>(P) < Names.size() ? Names[static_cast<size_t>(P)] : std::string("?");
			Named += (Named.empty() ? "" : ", ") + Name;
			bCoat = bCoat && (Name == "upper" || Name == "rivet" || Name == "seam");
		}
		Check(bCoat && Rule.Paints.count(0) > 0, "she may lie on her upper coat and its rivet and seam lines only (never glass, her belly or its lines)", "paints " + Listed + " (" + Named + ")");
		Rule.Words = "all on her coat and its rivet and seam lines (paints " + Listed + ")";
		Skin F;
		const bool bFineSkin = bFineModel && ReadSkin(Models + "/B17Fine.json", 4.0, 8.2, F, Why);
		Check(bFineSkin, "her near shape read", Why);
		if (bFine && bFineSkin && !Rule.Paints.empty())
		{
			CheckSide("Port", Fine.Port, Fine, F, -1.0, Rule);
			CheckSide("Starboard", Fine.Starboard, Fine, F, 1.0, Rule);
			bool bMirror = Fine.Port.Points.size() == Fine.Starboard.Points.size();
			for (size_t K = 0; bMirror && K < Fine.Port.Points.size(); ++K)
			{
				const AirNoseArt::Point& P = Fine.Port.Points[K];
				const AirNoseArt::Point& Q = Fine.Starboard.Points[K];
				bMirror = std::fabs(P.At[0] - Q.At[0]) < 1e-6 && std::fabs(P.At[1] + Q.At[1]) < 1e-6 && std::fabs(P.At[2] - Q.At[2]) < 1e-6
					&& std::fabs(P.U + Q.U - 1.0) < 1e-6 && std::fabs(P.V - Q.V) < 1e-6;
			}
			Check(bMirror, "the starboard panel the port one mirrored, its picture read the other way along");
			// Clear of her kill tally (the file's tally: its bombs and kill marks under her window, port side): the panel's
			// aftmost point ahead of every slot's foremost edge.
			double PanelAft = 1e9, TallyFore = -1e9;
			for (const AirNoseArt::Point& P : Fine.Port.Points) PanelAft = std::min(PanelAft, P.At[0]);
			int Slots = 0;
			for (const char* Block : { "missions", "kills" })
			{
				const AirFeed::Json& B = M["tally"][Block];
				const double R = B["radius"].Num(0.0);
				for (const AirFeed::Json& Slot : B["slots"].Items)
				{
					TallyFore = std::max(TallyFore, Slot.At(0).Num(-1e9) + R);
					++Slots;
				}
			}
			std::snprintf(Line, sizeof Line, "her panel from %.3f m forward, the tally's %d slots to %.3f m", PanelAft, Slots, TallyFore);
			Check(Slots > 0 && PanelAft > TallyFore + 0.05, "her panel clear of her kill tally", Line);
			std::snprintf(Line, sizeof Line, "%.2f m by %.2f m, the older B-17's %.2f m by %.2f m", Fine.WideM, Fine.HighM, Panel.WideM, Panel.HighM);
			// (0.45 each way, the largest that lies on her coat within 15 mm of her skin, facing out of her side, clear of her
			// windows, her windscreen's step and her tally: Tools/pack_nose_art.py's notes; a smaller one would be a step back)
			Check(Fine.WideM >= 0.449 * Panel.WideM && Fine.HighM >= 0.449 * Panel.HighM, "her painting 0.45 of the older one's size each way or more", Line);
		}
	}

	// Which aircraft wears which: the battles where nobody is named.
	int Recordings = 0;
	for (int A = 4; A < Argc; ++A)
	{
		AirFeed::Recording Rec;
		if (!ReadText(Argv[A], Text) || !AirFeed::ReadRecording(Text, Rec, Why) || Rec.Views.empty())
		{
			Check(false, "a recording read", std::string(Argv[A]) + ": " + Why);
			continue;
		}
		++Recordings;
		const AirSky::Placing Place = AirFeed::PlacingOf(Rec.Board, Rec.Views.front().Groups, Rec.Seed);
		std::printf("%s (seed %llu; %zu bombers' seats on its board)\n", Rec.Key.c_str(), static_cast<unsigned long long>(Place.Seed), Rec.Board.Bombers.size());
		AirNoseArt::Book Book;
		Book.Begin(List, Place.Seed, Rec.Board.Bombers);
		const Played Run = Play(Rec.Board, Rec.Views, Book, 238, 600.0);
		int Given = 0, Bare = 0;
		std::set<int> Numbers;
		bool bOwnRepeat = false, bShared = false;
		for (const auto& [Group, Art] : Run.ByGroup)
		{
			std::set<int> Own;
			for (const int N : Art)
			{
				++Given;
				Bare += N == 0 ? 1 : 0;
				if (N <= 0) continue;
				bOwnRepeat = bOwnRepeat || Own.count(N) > 0;
				Own.insert(N);
			}
			for (const int N : Own)
			{
				bShared = bShared || Numbers.count(N) > 0;
			}
			Numbers.insert(Own.begin(), Own.end());
			std::printf("    group %d (%s): %s\n", Group, Book.HowTold(Group) == AirNoseArt::Told::Dealt ? "dealt" : "from its seats", ListOf(Art, List, 30).c_str());
		}
		std::snprintf(Line, sizeof Line, "%d American bombers, %d bare; %d other aircraft, %d of them given one", Given, Bare, Run.Others, Run.OthersGiven);
		Check(Run.OthersGiven == 0, "American bombers alone wear nose art", Line);
		const bool bAmericanBombers = std::any_of(Rec.Views.front().Groups.begin(), Rec.Views.front().Groups.end(),
			[](const AirShow::Group& G) { return AirNoseArt::Wears(G.Nation, G.bFighter, G.Model, HasPanel); });
		if (bAmericanBombers)
		{
			Check(Given > 0 && Bare < Given, "her bombers given their paintings", Line);
		}
		Check(Run.bSteady, "an aircraft keeps her painting the whole battle");
		Check(!bOwnRepeat && !bShared, "no painting twice in the battle while the list holds unused ones");
		// The same battle again: the same paintings, aircraft for aircraft.
		AirNoseArt::Book Again;
		Again.Begin(List, Place.Seed, Rec.Board.Bombers);
		const Played Twice = Play(Rec.Board, Rec.Views, Again, 238, 120.0);
		bool bSame = true;
		for (const auto& [Group, Art] : Twice.ByGroup)
		{
			const auto Was = Run.ByGroup.find(Group);
			for (size_t K = 0; K < Art.size(); ++K) bSame = bSame && Was != Run.ByGroup.end() && K < Was->second.size() && Was->second[K] == Art[K];
		}
		Check(bSame, "the same battle played again wears the same paintings");
		// Another mission (another seed): another set.
		AirNoseArt::Book Other;
		Other.Begin(List, Place.Seed + 1, Rec.Board.Bombers);
		const Played Next = Play(Rec.Board, Rec.Views, Other, 238, 120.0);
		int Same = 0, Count = 0;
		for (const auto& [Group, Art] : Next.ByGroup)
		{
			const auto Was = Run.ByGroup.find(Group);
			for (const int N : Art)
			{
				if (N <= 0) continue;
				++Count;
				Same += Was != Run.ByGroup.end() && std::find(Was->second.begin(), Was->second.end(), N) != Was->second.end() ? 1 : 0;
			}
		}
		std::snprintf(Line, sizeof Line, "%d of %d of the next mission's paintings flew in this one", Same, Count);
		Check(!bAmericanBombers || (Count > 0 && Same * 4 <= Count), "another mission, another set", Line);
	}
	Check(Recordings + 4 == Argc || Argc == 4, "every recording given read");

	// Dealt many times over: about one in five bare, each mission's set its own.
	{
		double Overlap = 0.0;
		const double Share = BareShare(List, 500, Overlap);
		std::snprintf(Line, sizeof Line, "%.3f of 6000 aircraft bare (the list's none: %.2f); %.1f%% of a mission's paintings flew in the one before", Share, List.BareChance, Overlap * 100.0);
		Check(std::fabs(Share - List.BareChance) < 0.03 && Overlap < 0.15, "about one in five bare, each mission's own set", Line);
	}

	// Raids the rules made (tools/air-record/nose-art-raid.ts): who wears what where the board gives the seats.
	std::printf("Raids the rules made (%s)\n", Raid.c_str());
	AirFeed::Json J;
	Raided Seated, Spliced;
	Why.clear();
	const bool bRaid = ReadText(Raid, Text) && AirFeed::Parse(Text, J, Why) && ReadRaid(J["seats"], nullptr, Seated, Why) && ReadRaid(J["spliced"], nullptr, Spliced, Why);
	Check(bRaid, "its two raids read: their boards, views and the rules' own corps of each box", Why);
	if (bRaid)
	{
		const AirFeed::Sky& Board = Seated.Board;
		size_t Seats = 0;
		int Named = 0, NamedRight = 0, Bare = 0;
		for (const auto& [Corps, B] : Board.Bombers)
		{
			Seats += B.Seats.size();
			for (const AirFeed::Sky::BomberSeat& Seat : B.Seats)
			{
				if (Seat.Art <= 0)
				{
					++Bare;
					continue;
				}
				++Named;
				const AirNoseArt::Entry* E = List.Of(Seat.Art);
				NamedRight += E && E->Name == Seat.Name ? 1 : 0;
			}
		}
		std::snprintf(Line, sizeof Line, "%zu groups, %zu seats, %d bare; %d of %d named as the list names their paintings", Board.Bombers.size(), Seats, Bare, NamedRight, Named);
		Check(Board.Bombers.size() == 3 && Seats == 240 && Named > 0 && NamedRight == Named, "the seats read from place.bombers, the rules' names the list's", Line);

		// The rules' own corps of each box (their forces, in the order the rules number them), and each group's aircraft a
		// model (extra.per).
		auto CorpsOf = [](const Raided& R)
		{
			std::map<int, int> Out;
			for (const auto& [Id, Corps] : R.Forces) Out[Id] = Corps;
			return Out;
		};
		auto GroupOf = [](const Raided& R, int Id)
		{
			for (const AirShow::Group& G : R.Views.front().Groups)
				if (G.Id == Id) return G;
			return AirShow::Group();
		};
		// The named losses that bring a model down (air:lost with extra.bomber and models 1 or more), as the fixture's own
		// text has them: each group's paintings named.
		std::map<int, std::multiset<int>> Asked;
		for (const AirFeed::Json& V : J["seats"]["views"].Items)
		{
			for (const AirFeed::Json& E : V["events"].Items)
			{
				const AirFeed::Json& X = E["extra"];
				if (E["what"].Str("") == "air:lost" && X["bomber"].IsObject() && X["models"].Int(1) >= 1) Asked[E["unit"].Int(-1)].insert(Usable(List, X["bomber"]["art"].Int(0)));
			}
		}
		// Each bomber that falls in the picture for a named loss wears that bomber's painting, whichever model the show
		// took; every such loss is seen; and no painting changes but at a naming, none worn twice in a group at once.
		auto NamedFalls = [&](const Played& Run)
		{
			int Right = 0, All = 0;
			std::map<int, std::multiset<int>> Down;
			for (const Fell& F : Run.Falls)
			{
				if (F.Named < 0) continue;
				++All;
				Right += F.Art == Usable(List, F.Named) ? 1 : 0;
				Down[F.Group].insert(Usable(List, F.Named));
			}
			size_t Losses = 0;
			for (const auto& [Group, Arts] : Asked) Losses += Arts.size();
			// (the show's clock stops at the last view, so the losses of its last few seconds are still held for the guns;
			// and one blown apart at once is never drawn falling)
			bool bOwn = true;
			for (const auto& [Group, Arts] : Down)
			{
				const auto Mine = Asked.find(Group);
				bOwn = bOwn && Mine != Asked.end() && std::includes(Mine->second.begin(), Mine->second.end(), Arts.begin(), Arts.end());
			}
			std::snprintf(Line, sizeof Line, "%d of %d falling as the bombers their losses name; %zu such losses in the raid's views", Right, All, Losses);
			Check(All >= 5 && Right == All, "each bomber that falls for a named loss wears the painting the loss names (extra.bomber)", Line);
			Check(bOwn, "each named one seen falling one of her own group's named losses that bring a model down", Line);
			std::snprintf(Line, sizeof Line, "%d paintings changed at a naming; %d frames with one worn twice in a group", Run.Repainted, Run.Shared);
			Check(Run.bSteady && Run.Repainted > 0 && Run.Shared == 0, "a painting changes only at a naming (hers, and the one she took it from), none worn twice in a group", Line);
		};

		// As the rules send it now (no view names its box's corps): every group's paintings dealt, never paired by order.
		{
			AirNoseArt::Book Book;
			Book.Begin(List, 77, Board.Bombers);
			const Played Run = Play(Board, Seated.Views, Book, 238, 1000.0);
			bool bDealt = !Run.ByGroup.empty();
			for (const auto& [Group, Art] : Run.ByGroup)
			{
				bDealt = bDealt && Book.HowTold(Group) == AirNoseArt::Told::Dealt;
				std::printf("    group %d (%s): %s\n", Group, Book.HowTold(Group) == AirNoseArt::Told::Dealt ? "dealt" : "its seats", ListOf(Art, List, 8).c_str());
			}
			Check(bDealt, "no corps named (as the rules send it now): every group's paintings dealt, none paired by its order");
			NamedFalls(Run);
		}
		// Each view naming its box's corps (extra.corps), as the rules would send it for looks only: each group wears its
		// own corps' seats, the model in slot k the first of those it stands for (seat k times per), old hands first.
		auto OwnSeats = [&](const Raided& R, const AirNoseArt::Book& Book, const Played& Run, const char* What)
		{
			int Right = 0, All = 0;
			bool bByCorps = !Run.ByGroup.empty();
			const std::map<int, int> Of = CorpsOf(R);
			for (const auto& [Group, Art] : Run.ByGroup)
			{
				const AirShow::Group G = GroupOf(R, Group);
				const auto Corps = Of.find(Group);
				const auto Own = Corps != Of.end() ? R.Board.Bombers.find(Corps->second) : R.Board.Bombers.end();
				bByCorps = bByCorps && Book.HowTold(Group) == AirNoseArt::Told::ByCorps && Own != R.Board.Bombers.end();
				if (Own == R.Board.Bombers.end()) continue;
				const size_t Per = static_cast<size_t>(std::max(1, G.Per));
				for (size_t K = 0; K < Art.size() && K < static_cast<size_t>(G.Figures0); ++K)
				{
					const int Want = K * Per < Own->second.Seats.size() ? Usable(List, Own->second.Seats[K * Per].Art) : 0;
					Right += Art[K] == Want ? 1 : 0;
					++All;
				}
				std::printf("    group %d (corps %d, %s): %s\n", Group, Corps->second, Book.HowTold(Group) == AirNoseArt::Told::ByCorps ? "its seats" : "dealt", ListOf(Art, List, 8).c_str());
			}
			Check(bByCorps, What);
			std::snprintf(Line, sizeof Line, "%d of %d models as the battle began", Right, All);
			Check(All > 0 && Right == All, "each model wears the first seat it stands for (seat slot times per), old hands first", Line);
		};
		{
			Raided Corpsed;
			const std::map<int, int> Corps = CorpsOf(Seated);
			Check(ReadRaid(J["seats"], &Corps, Corpsed, Why), "the raid read with each box's corps named", Why);
			AirNoseArt::Book Book;
			Book.Begin(List, 77, Corpsed.Board.Bombers);
			const Played Run = Play(Corpsed.Board, Corpsed.Views, Book, 238, 1000.0);
			OwnSeats(Corpsed, Book, Run, "each box's corps named (extra.corps): each group flies with its own corps' seats");
			NamedFalls(Run);
		}
		// A ticket whose order is not by corps (the campaign's growCommands and splitOff leave a newer group ahead of older
		// ones), its groups all of 80, as the board's three nose groups: with no corps named every group is dealt (paired by
		// order, the first group would wear the lowest corps' crews); named, each wears its own, by the rules' forces.
		{
			bool bAscending = true;
			for (size_t K = 1; K < Spliced.Forces.size(); ++K) bAscending = bAscending && Spliced.Forces[K - 1].second < Spliced.Forces[K].second;
			std::string Order;
			for (const auto& [Id, Corps] : Spliced.Forces) Order += (Order.empty() ? "" : ", ") + std::to_string(Id) + " corps " + std::to_string(Corps);
			Check(Spliced.Forces.size() == 3 && !bAscending, "the spliced raid's ticket not in corps order", Order);
			AirNoseArt::Book Book;
			Book.Begin(List, 77, Spliced.Board.Bombers);
			const Played Run = Play(Spliced.Board, Spliced.Views, Book, 238, 5.0);
			bool bDealt = !Run.ByGroup.empty();
			for (const auto& [Group, Art] : Run.ByGroup) bDealt = bDealt && Book.HowTold(Group) == AirNoseArt::Told::Dealt && !Art.empty();
			Check(bDealt, "no corps named: every group dealt, none given another group's crews");
			Raided Corpsed;
			const std::map<int, int> Corps = CorpsOf(Spliced);
			Check(ReadRaid(J["spliced"], &Corps, Corpsed, Why), "the spliced raid read with each box's corps named", Why);
			AirNoseArt::Book Exact;
			Exact.Begin(List, 77, Corpsed.Board.Bombers);
			const Played Named2 = Play(Corpsed.Board, Corpsed.Views, Exact, 238, 5.0);
			OwnSeats(Corpsed, Exact, Named2, "each box's corps named: each group flies with its own corps' seats, whatever the ticket's order");
		}
		// A corps named on two boxes (a carrier group's bombers split by kind, not known one by one), or one the board has
		// no seats for: dealt.
		{
			std::map<int, AirShow::Group> Groups;
			for (const AirShow::Group& G : Spliced.Views.front().Groups) Groups[G.Id] = G;
			Groups[1].Corps = 22;
			Groups[2].Corps = 22;
			Groups[3].Corps = 999;
			AirNoseArt::Book Doubt;
			Doubt.Begin(List, 77, Spliced.Board.Bombers);
			std::vector<AirShow::PlaneOut> Planes;
			for (const auto& [Id, G] : Groups)
			{
				AirShow::PlaneOut P;
				P.Key = Id;
				P.Group = Id;
				P.bFighter = G.bFighter;
				P.Nation = G.Nation;
				P.Model = G.Model;
				Planes.push_back(P);
			}
			Doubt.Take(Groups, Planes, HasPanel);
			Check(Doubt.HowTold(1) == AirNoseArt::Told::Dealt && Doubt.HowTold(2) == AirNoseArt::Told::Dealt && Doubt.HowTold(3) == AirNoseArt::Told::Dealt && Doubt.ArtOf(1) >= 0,
				"a corps named on two boxes, or one the board has no seats for: dealt");
		}
		// Americans only: the same group flown by Germany wears none, nor does a fighter.
		{
			std::map<int, AirShow::Group> Groups;
			AirShow::Group G;
			G.Id = 1;
			G.bFighter = false;
			G.Nation = "GER";
			G.Model = "B17";
			Groups[1] = G;
			G.Id = 2;
			G.Nation = "USA";
			G.bFighter = true;
			Groups[2] = G;
			AirNoseArt::Book Them;
			Them.Begin(List, 77, {});
			std::vector<AirShow::PlaneOut> Planes(2);
			Planes[0].Key = 1;
			Planes[0].Group = 1;
			Planes[0].bFighter = false;
			Planes[0].Nation = "GER";
			Planes[0].Model = "B17";
			Planes[1].Key = 2;
			Planes[1].Group = 2;
			Planes[1].bFighter = true;
			Planes[1].Nation = "USA";
			Planes[1].Model = "B17";
			Them.Take(Groups, Planes, HasPanel);
			Check(Them.ArtOf(1) == -1 && Them.ArtOf(2) == -1, "a captured B-17 in German hands and an American fighter wear none");
		}
	}
	int Both = 0;
	for (const int N : List.Painted()) Both += AirNoseArt::BothSides(N) ? 1 : 0;
	std::snprintf(Line, sizeof Line, "%d of %zu paintings", Both, List.Painted().size());
	Check(Both > 0 && Both * 5 < static_cast<int>(List.Painted().size()), "a few worn on both sides", Line);
	std::printf("%s: %d failed\n", Bad ? "FAIL" : "ok", Bad);
	return Bad ? 1 : 0;
}
