// The nose art's test (Battle/AirNoseArt; release 2, Brian 25 November 2026 01:30Z: "american only"). The pack
// (Content/Data/NoseArt, Tools/pack_nose_art.py): every painting the list says is painted is there, 352 by 216 RGBA,
// the folder under 20 MB. The panel on the game's own B-17 (panels.json against her near shape, Content/Models/B17.json):
// on both sides every point of its triangles lies just off her skin (0.5 to 15 mm, never inside her, never standing off),
// all of it on her olive, each triangle facing out as the engine draws a face, the starboard panel the port one mirrored,
// and the picture reading from left to right seen from outside either side, its top up, unstretched: in the model
// file's frame and in the game's (her nose toward -X, her right hand at -Y). Then which aircraft wears which painting:
// the Bremen film played through the game's own show, where the battle does not say who flies them (each mission a
// different set from its seed, about one in five bare, none shared, Americans' bombers alone, an aircraft's for the whole
// battle); and a raid the rules made, whose bombers wear their seats' paintings (the board's place.bombers), old hands
// first, the rules' names matching the list's.
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
	double Meet(const Skin& S, const V3& O, const V3& D, int& Paint)
	{
		double Best = std::numeric_limits<double>::infinity();
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
			if (T > 1e-9 && T < Best)
			{
				Best = T;
				const double W0 = 1.0 - U - V;
				Paint = W0 >= U && W0 >= V ? S.P0[K] : U >= V ? S.P1[K] : S.P2[K];
			}
		}
		return Best;
	}

	/** How far a point is outside her skin (negative: inside), along the ray from her axis (her x, on her middle line)
	 * through it, as the tool that laid the panel measured it; and the paint there. */
	double Gap(const Skin& S, const V3& P, int& Paint)
	{
		const V3 O{ P.X, 0.0, 0.0 };
		const V3 Out = Sub(P, O);
		const double R = Len(Out);
		const double T = Meet(S, O, Mul(Out, 1.0 / R), Paint);
		return R - T;
	}

	void CheckSide(const char* Name, const AirNoseArt::Side& Side, const AirNoseArt::Panel& Panel, const Skin& S, double Hand)
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
				const double G = Gap(S, P, Paint);
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
		Check(Paints.size() == 1 && *Paints.begin() == 0, "all on her olive (paint 0)", "paints " + Under);
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

	/** A battle played through the game's own show at four frames a battle second, each frame's aircraft handed to the
	 * book as the scene hands them; whether any aircraft's painting ever changed, and the groups' aircraft in turn. */
	struct Played
	{
		bool bSteady = true;
		std::map<int, std::vector<int>> ByGroup; // each group's paintings, its aircraft in turn
		int Others = 0;                          // aircraft that wear none
		int OthersGiven = 0;                     // ... given one all the same (none should be)
	};

	Played Play(const AirFeed::Sky& Board, const std::vector<AirFeed::Frame>& Views, AirNoseArt::Book& Book, uint64_t ShowSeed, double MostT)
	{
		Played Out;
		AirShow::Show Show(ShowSeed);
		AirFeed::Apply(Board, Show);
		size_t Taken = 0;
		std::map<int, int> Seen;
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
			for (const AirShow::PlaneOut& P : Show.Planes())
			{
				const int Art = Book.ArtOf(P.Key);
				if (!AirNoseArt::Wears(P.Nation, P.bFighter, P.Model, HasPanel))
				{
					Others.insert(P.Key);
					Out.OthersGiven += Art != -1 ? 1 : 0;
					continue;
				}
				const auto Was = Seen.find(P.Key);
				if (Was == Seen.end())
				{
					Seen[P.Key] = Art;
					Out.ByGroup[P.Group].push_back(Art);
				}
				else if (Was->second != Art)
				{
					Out.bSteady = false;
				}
			}
		}
		Out.Others = static_cast<int>(Others.size());
		return Out;
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
			G.N = G.N0 = 48;
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

	// A raid the rules made: each bomber wears her seat's painting.
	std::printf("A raid the rules made (%s)\n", Raid.c_str());
	AirFeed::Json J;
	AirFeed::Sky Board;
	AirFeed::Frame First;
	Why.clear();
	const bool bRaid = ReadText(Raid, Text) && AirFeed::Parse(Text, J, Why) && AirFeed::ReadSky(J["sky"], Board) && AirFeed::ReadView(J["view"], Board, First);
	Check(bRaid, "its board and first view read", Why);
	if (bRaid)
	{
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
		AirNoseArt::Book Book;
		Book.Begin(List, 77, Board.Bombers);
		AirFeed::Recording Rec;
		Rec.Board = Board;
		Rec.Views.push_back(First);
		const Played Run = Play(Board, Rec.Views, Book, 238, 20.0);
		// Paired in order (the views name no corps): each group's aircraft in turn wear its seats, old hands first.
		std::vector<int> Corps;
		for (const auto& [Id, B] : Board.Bombers) Corps.push_back(Id);
		int Right = 0, All = 0;
		size_t Index = 0;
		for (const auto& [Group, Art] : Run.ByGroup)
		{
			const std::vector<AirFeed::Sky::BomberSeat>& Own = Board.Bombers[Corps[std::min(Index, Corps.size() - 1)]].Seats;
			for (size_t K = 0; K < Art.size(); ++K)
			{
				const int Want = K < Own.size() && Own[K].Art > 0 && List.Of(Own[K].Art) && List.Of(Own[K].Art)->bPainted ? Own[K].Art : 0;
				Right += Art[K] == Want ? 1 : 0;
				++All;
			}
			std::printf("    group %d (%s): %s\n", Group, Book.HowTold(Group) == AirNoseArt::Told::InOrder ? "its seats, paired in order" : "dealt", ListOf(Art, List, 8).c_str());
			Check(Book.HowTold(Group) == AirNoseArt::Told::InOrder, "a group paired in order with its corps' seats");
			++Index;
		}
		std::snprintf(Line, sizeof Line, "%d of %d aircraft", Right, All);
		Check(All > 0 && Right == All, "each aircraft wears her seat's painting, old hands first", Line);

		// Named by the battle (extra.corps): paired exactly, whatever the order.
		{
			std::map<int, AirShow::Group> Groups;
			for (const AirShow::Group& G : First.Groups) Groups[G.Id] = G;
			size_t K = 0;
			for (auto& [Id, G] : Groups)
			{
				if (!AirNoseArt::Wears(G.Nation, G.bFighter, G.Model, HasPanel)) continue;
				G.Corps = Corps[Corps.size() - 1 - std::min(K, Corps.size() - 1)];
				++K;
			}
			AirNoseArt::Book Named2;
			Named2.Begin(List, 77, Board.Bombers);
			std::vector<AirShow::PlaneOut> Planes;
			int Key = 1;
			for (const auto& [Id, G] : Groups)
			{
				AirShow::PlaneOut P;
				P.Key = Key++;
				P.Group = Id;
				P.bFighter = G.bFighter;
				P.Nation = G.Nation;
				P.Model = G.Model;
				Planes.push_back(P);
			}
			Named2.Take(Groups, Planes, HasPanel);
			bool bExact = true;
			for (const AirShow::PlaneOut& P : Planes)
			{
				const AirShow::Group& G = Groups[P.Group];
				if (!AirNoseArt::Wears(G.Nation, G.bFighter, G.Model, HasPanel)) continue;
				const int Want = Board.Bombers[G.Corps].Seats.front().Art;
				bExact = bExact && Named2.HowTold(P.Group) == AirNoseArt::Told::ByCorps && Named2.ArtOf(P.Key) == Want;
			}
			Check(bExact, "a group the battle names the corps of wears that corps' seats");
		}
		// The counts in doubt (one nose group short): every group dealt, none paired wrongly.
		{
			std::map<int, AirFeed::Sky::BomberGroup> Short = Board.Bombers;
			Short.erase(Short.begin());
			AirNoseArt::Book Doubt;
			Doubt.Begin(List, 77, Short);
			const Played Dealt = Play(Board, Rec.Views, Doubt, 238, 5.0);
			bool bDealt = !Dealt.ByGroup.empty();
			for (const auto& [Group, Art] : Dealt.ByGroup) bDealt = bDealt && Doubt.HowTold(Group) == AirNoseArt::Told::Dealt && !Art.empty();
			Check(bDealt, "with the counts in doubt every group is dealt, none paired wrongly");
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
