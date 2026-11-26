// The ground battle's tanks, guns and lorries played through the game's own code (Battle/LandKit, no engine): every
// vehicle track of a recorded battle at 60 frames a second, with the checks of Brian's brief (21 November 2026), and a
// short report. A failed check prints FAIL and the run ends with a non-zero status.
// Usage: land_kit_test <Content folder> [recording.json]

#include "Battle/LandKit.h"
#include "Battle/Fates.h"
#include "Battle/LandCrater.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace LandKit;

namespace
{
	using Clock = std::chrono::steady_clock;

	double Ms(Clock::time_point A, Clock::time_point B) { return std::chrono::duration<double, std::milli>(B - A).count(); }

	bool ReadFile(const std::string& Path, std::string& Out)
	{
		std::ifstream F(Path, std::ios::binary);
		if (!F)
		{
			return false;
		}
		std::stringstream S;
		S << F.rdbuf();
		Out = S.str();
		return true;
	}

	bool Finite(const V3& P) { return std::isfinite(P.X) && std::isfinite(P.Y) && std::isfinite(P.Z); }
	bool Finite(const Quat& Q) { return std::isfinite(Q.W) && std::isfinite(Q.X) && std::isfinite(Q.Y) && std::isfinite(Q.Z); }

	double WrapDeg(double A)
	{
		A = std::fmod(A + 180.0, 360.0);
		if (A < 0.0)
		{
			A += 360.0;
		}
		return A - 180.0;
	}

	/** How far each part may stand off its parent's joint: the farthest any clip shifts it, and its slide. */
	std::vector<double> Reach(const Model& M)
	{
		std::vector<double> Out(M.Parts.size(), 0.0);
		for (const auto& Named : M.Clips)
		{
			for (const ClipTrack& K : Named.second.Tracks)
			{
				for (size_t I = 0; I + 2 < K.Move.size(); I += 3)
				{
					const double L = std::sqrt(static_cast<double>(K.Move[I]) * K.Move[I] + static_cast<double>(K.Move[I + 1]) * K.Move[I + 1]
						+ static_cast<double>(K.Move[I + 2]) * K.Move[I + 2]);
					Out[static_cast<size_t>(K.Part)] = std::max(Out[static_cast<size_t>(K.Part)], L);
				}
			}
		}
		for (size_t P = 0; P < M.Parts.size(); ++P)
		{
			if (M.Parts[P].Kind == PartKind::Slide)
			{
				Out[P] += std::max(std::fabs(M.Parts[P].SlideLo), std::fabs(M.Parts[P].SlideHi)) * M.Parts[P].SlideWay.Length();
			}
		}
		// a body under a slide that has no points of its own (no body: the Tiger's hatch_commander_lift)
		// is carried by that slide too: its reach from the body above grows by it.
		for (size_t P = 0; P < M.Parts.size(); ++P)
		{
			for (int Up = M.Parts[P].Parent; Up >= 0 && M.Parts[static_cast<size_t>(Up)].Body < 0; Up = M.Parts[static_cast<size_t>(Up)].Parent)
			{
				const Part& S = M.Parts[static_cast<size_t>(Up)];
				if (S.Kind == PartKind::Slide)
				{
					Out[P] += std::max(std::fabs(S.SlideLo), std::fabs(S.SlideHi)) * S.SlideWay.Length();
				}
			}
		}
		return Out;
	}

	struct Tally
	{
		long Checked = 0, Failed = 0;
		double Worst = 0.0;
		std::string First;
		void Note(bool bOk, double Size, const std::string& Why)
		{
			++Checked;
			Worst = std::max(Worst, Size);
			if (!bOk)
			{
				if (Failed == 0)
				{
					First = Why;
				}
				++Failed;
			}
		}
	};

	std::string Who(const Vehicle& V)
	{
		char Buf[96];
		std::snprintf(Buf, sizeof(Buf), "%s %d/%d (%s)", V.Look().Name.c_str(), V.Words().Block, V.Words().K, V.Words().Role.c_str());
		return Buf;
	}

	// TK1: the far pieces (LandKit::SplitFar). Each triangle as drawn on the field: its three corners, normals (to a
	// millimetre and a thousandth), colours and soot weights, so the far bodies' and the pieces' can be compared as
	// lists: every triangle there exactly once, at the same place, painted the same.
	using TriangleKey = std::vector<long long>;

	void PlacedTriangles(const Mesh& M, const Place& P, std::vector<TriangleKey>& Out)
	{
		for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
		{
			TriangleKey Key;
			Key.reserve(30);
			for (int C = 0; C < 3; ++C)
			{
				const size_t K = M.Triangles[T + static_cast<size_t>(C)];
				const V3 At = P.Point(V3(M.Points[K * 3], M.Points[K * 3 + 1], M.Points[K * 3 + 2]));
				const V3 Way = P.Way(V3(M.Normals[K * 3], M.Normals[K * 3 + 1], M.Normals[K * 3 + 2]));
				for (double X : { At.X, At.Y, At.Z })
				{
					Key.push_back(std::llround(X * 1000.0));
				}
				for (double X : { Way.X, Way.Y, Way.Z })
				{
					Key.push_back(std::llround(X * 1000.0));
				}
				for (int Ch = 0; Ch < 4; ++Ch)
				{
					Key.push_back(M.Colours[K * 4 + static_cast<size_t>(Ch)]);
				}
				Key.push_back(std::llround(static_cast<double>(M.Soot[K]) * 10000.0));
			}
			Out.push_back(std::move(Key));
		}
	}

	/** The pieces of M checked against her bodies at a pinned pose: the same triangles exactly once each, FarOff nothing
	 * there; and every far triangle of hers in a piece, or in a body kept out. bLidUp: the lid-up piece (FarSplit::Open)
	 * drawn in its piece's stead, as the scene draws it while her commander's lid is open. Why: the first fault. */
	bool CheckPieces(const Model& M, const FarSplit& S, const Vehicle::Pin& Pin, const GroundFn& Ground, std::string& Why, bool bLidUp = false)
	{
		Vehicle V;
		V.PinPose(M, Pin, Ground);
		const std::vector<Place>& P = V.Bodies();
		std::vector<TriangleKey> Bodies, Pieces;
		size_t FarAll = 0, FarIn = 0, FarKept = 0;
		for (size_t B = 0; B < M.Bodies.size(); ++B)
		{
			FarAll += M.Bodies[B].Far.Triangles.size() / 3;
			FarKept += S.Kept[B] ? M.Bodies[B].Far.Triangles.size() / 3 : 0;
			if (S.PieceOf[B] < 0)
			{
				continue;
			}
			if (S.Kept[B])
			{
				Why = M.Bodies[B].Name + " is both kept out and in a piece";
				return false;
			}
			FarIn += S.StandIn[B] ? 0 : M.Bodies[B].Far.Triangles.size() / 3;
			PlacedTriangles(S.Level(M, static_cast<int>(B)), P[B], Bodies);
		}
		if (FarIn + FarKept != FarAll)
		{
			Why = std::to_string(FarAll) + " far triangles, " + std::to_string(FarIn) + " in the pieces and " + std::to_string(FarKept) + " kept out";
			return false;
		}
		for (int Which = 0; Which < 2; ++Which)
		{
			const FarPiece& Piece = bLidUp && Which == S.OpenOf ? S.Open : S.Pieces[Which];
			if (Piece.Empty())
			{
				continue;
			}
			if (Piece.Anchor < 0 || static_cast<size_t>(Piece.Anchor) >= P.size() || Piece.FirstTriangle.size() != Piece.Bodies.size() + 1
				|| Piece.FirstTriangle.back() * 3 != Piece.Merged.Triangles.size())
			{
				Why = "a piece's anchor or its bodies' bookkeeping";
				return false;
			}
			PlacedTriangles(Piece.Merged, P[static_cast<size_t>(Piece.Anchor)], Pieces);
		}
		std::sort(Bodies.begin(), Bodies.end());
		std::sort(Pieces.begin(), Pieces.end());
		if (Bodies != Pieces)
		{
			size_t Same = 0;
			for (size_t K = 0; K < std::min(Bodies.size(), Pieces.size()) && Bodies[K] == Pieces[K]; ++K)
			{
				++Same;
			}
			Why = std::to_string(Bodies.size()) + " body triangles against " + std::to_string(Pieces.size()) + " in the pieces, the first " + std::to_string(Same)
				+ " (sorted) the same";
			return false;
		}
		const double Off = FarOff(M, S, P);
		if (Off > 1e-6)
		{
			Why = "FarOff " + std::to_string(Off) + " m at rest";
			return false;
		}
		return true;
	}

	double Percentile(std::vector<double> L, double P)
	{
		if (L.empty())
		{
			return 0.0;
		}
		std::sort(L.begin(), L.end());
		return L[std::min(L.size() - 1, static_cast<size_t>(P * static_cast<double>(L.size() - 1) + 0.5))];
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::printf("usage: land_kit_test <Content folder> [recording.json]\n");
		return 2;
	}
	const std::string Content = argv[1];
	const std::string RecordingPath = argc > 2 ? argv[2] : Content + "/Data/LandBattles/normandy-1944.json";
	int Failures = 0;
	auto Report = [&Failures](const char* What, const Tally& T, const char* Unit)
	{
		std::printf("  %-52s %s  (%ld checked, worst %.4g %s)\n", What, T.Failed ? "FAIL" : "ok", T.Checked, T.Worst, Unit);
		if (T.Failed)
		{
			std::printf("      %ld failed; first: %s\n", T.Failed, T.First.c_str());
			++Failures;
		}
	};

	// ---- The models, read and cut.
	std::printf("THE MODELS (Content/Models/Battle)\n");
	// (and Prokhorovka's: the StuG, the T-34, the light tank and the ZiS-3, for the far pieces)
	const char* Names[10] = { "Sherman", "Tiger", "PanzerIV", "Howitzer105", "leFH18", "Truck", "StuG3", "T34", "LightTank", "ZiS3" };
	const char* Owners[10] = { "USA", "GER", "GER", "USA", "GER", "USA", "GER", "SOV", "SOV", "SOV" };
	for (int I = 0; I < 10; ++I)
	{
		std::string Text;
		if (!ReadFile(Content + "/Models/Battle/" + Names[I] + ".json", Text))
		{
			std::printf("  FAIL cannot read %s.json\n", Names[I]);
			++Failures;
			continue;
		}
		for (int Pass = 0; Pass < (I == 5 ? 2 : 1); ++Pass)
		{
			Dress D;
			D.Nation = Pass == 1 ? "GER" : Owners[I];
			const auto T0 = Clock::now();
			Json Doc;
			std::string Error;
			Doc.Parse(Text, Error);
			const auto T1 = Clock::now();
			Model M;
			const bool bOk = M.Load(Text, D, Error);
			const auto T2 = Clock::now();
			if (!bOk)
			{
				std::printf("  FAIL %s: %s\n", Names[I], Error.c_str());
				++Failures;
				continue;
			}
			size_t NearPoints = 0, NearTris = 0, FarPoints = 0, FarTris = 0, Shared = 0;
			for (const Body& B : M.Bodies)
			{
				NearPoints += B.Near.Count();
				NearTris += B.Near.Triangles.size() / 3;
				FarPoints += B.Far.Count();
				FarTris += B.Far.Triangles.size() / 3;
				Shared += B.SameAs >= 0 ? 1 : 0;
			}
			std::printf("  %-11s %-3s %5.2f MB  coat %-10s %3zu bodies (%zu drawn from another's mesh)  near %6zu points %6zu triangles  far %5zu/%5zu"
				"  links %zu+%zu  read %.0f ms (of it JSON %.0f ms)\n",
				Names[I], D.Nation.c_str(), static_cast<double>(Text.size()) / 1e6, M.Coat.c_str(), M.Bodies.size(), Shared, NearPoints, NearTris,
				FarPoints, FarTris, M.Links(-1).size(), M.Links(1).size(), Ms(T0, T2), Ms(T0, T1));
			// TK1: her far pieces, checked against her bodies at rest, on a slope with her turret trained, and with a roof
			// gun kept out (as the scene keeps a body it may hide).
			FarSplit Split;
			const auto S0 = Clock::now();
			SplitFar(M, Split);
			const auto S1 = Clock::now();
			const GroundFn Slope = [](double X, double Y, double& OutZ, V3& OutNormal)
			{
				OutZ = 0.06 * X - 0.04 * Y + 3.0;
				OutNormal = V3(-0.06, 0.04, 1.0).Unit();
			};
			const GroundFn Level = [](double, double, double& OutZ, V3& OutNormal)
			{
				OutZ = 0.0;
				OutNormal = V3(0.0, 0.0, 1.0);
			};
			Vehicle::Pin Rest, Trained;
			Trained.X = 120.0;
			Trained.Y = -45.0;
			Trained.Heading = 2.2;
			Trained.Turret = M.Turret >= 0 ? 57.0 : 0.0;
			std::string Why;
			bool bPieces = CheckPieces(M, Split, Rest, Level, Why) && CheckPieces(M, Split, Trained, Slope, Why);
			std::vector<uint8_t> KeepOut(M.Bodies.size(), 0);
			int Kept = -1;
			for (size_t B = 0; B < M.Bodies.size() && Kept < 0; ++B)
			{
				if (M.Bodies[B].Name.find("roof_mg") == 0 || M.Bodies[B].Name == "aerial")
				{
					KeepOut[B] = 1;
					Kept = static_cast<int>(B);
				}
			}
			if (bPieces && Kept >= 0)
			{
				FarSplit Without;
				SplitFar(M, Without, &KeepOut);
				bPieces = CheckPieces(M, Without, Trained, Slope, Why);
				for (size_t B = 0; bPieces && B < M.Bodies.size(); ++B)
				{
					bool bUnder = false;
					for (int Up = static_cast<int>(B); Up >= 0; Up = M.Bodies[static_cast<size_t>(Up)].Parent)
					{
						bUnder = bUnder || Up == Kept;
					}
					if (bUnder && Without.PieceOf[B] >= 0)
					{
						bPieces = false;
						Why = M.Bodies[B].Name + " kept out with " + M.Bodies[static_cast<size_t>(Kept)].Name + ", but in a piece";
					}
				}
			}
			// TK1 (the commander's lid): the lid-up piece against her bodies with her open_up clip played to its end, on
			// the slope with her turret trained; and her lid carried well clear of where the shut piece has it.
			double LidMoved = 0.0;
			if (bPieces && Split.OpenOf >= 0)
			{
				Vehicle::Pin Unbuttoned = Trained;
				Unbuttoned.bOpenUp = true;
				bPieces = CheckPieces(M, Split, Unbuttoned, Slope, Why, true);
				const Mesh& ShutMesh = Split.Pieces[Split.OpenOf].Merged;
				const Mesh& OpenMesh = Split.Open.Merged;
				if (bPieces && (OpenMesh.Points.size() != ShutMesh.Points.size() || OpenMesh.Triangles != ShutMesh.Triangles || OpenMesh.Colours != ShutMesh.Colours))
				{
					bPieces = false;
					Why = "the lid-up piece is not its piece's points and triangles";
				}
				for (size_t PK = 0; bPieces && PK + 2 < OpenMesh.Points.size(); PK += 3)
				{
					const V3 Gap(OpenMesh.Points[PK] - ShutMesh.Points[PK], OpenMesh.Points[PK + 1] - ShutMesh.Points[PK + 1], OpenMesh.Points[PK + 2] - ShutMesh.Points[PK + 2]);
					LidMoved = std::max(LidMoved, std::sqrt(Gap.Dot(Gap)));
				}
				if (bPieces && LidMoved < 0.1)
				{
					bPieces = false;
					Why = "the lid-up piece's lid moved only " + std::to_string(LidMoved) + " m";
				}
			}
			size_t Stand = 0, Out = 0;
			for (size_t B = 0; B < M.Bodies.size(); ++B)
			{
				Stand += Split.StandIn[B];
				Out += Split.Kept[B];
			}
			const std::string Checked = Kept >= 0 ? "every far triangle once, as the bodies at rest, also with " + M.Bodies[static_cast<size_t>(Kept)].Name + " kept out"
				: std::string("every far triangle once, as the bodies at rest");
			std::printf("    far pieces (TK1): hull %5zu triangles from %2zu bodies, turret %4zu from %2zu; %zu standing in at their near level, %zu kept out; cut in %.1f ms: %s\n",
				Split.Pieces[0].Merged.Triangles.size() / 3, Split.Pieces[0].Bodies.size(), Split.Pieces[1].Merged.Triangles.size() / 3,
				Split.Pieces[1].Bodies.size(), Stand, Out, Ms(S0, S1), bPieces ? Checked.c_str() : ("FAIL " + Why).c_str());
			if (bPieces && Split.OpenOf >= 0)
			{
				std::printf("    lid up (TK1): the %s piece again with open_up played to its end, its lid carried up to %.2f m: as her bodies unbuttoned\n",
					Split.OpenOf == FarSplit::TurretPiece ? "turret" : "hull", LidMoved);
			}
			if (!bPieces)
			{
				++Failures;
			}
		}
	}

	// ---- Effects (R2), small arms off armour: the plates her skin hands the effects kit (Vehicle::PlateAbout, what
	// PlateNow gives the battle for a round's strike), cast on the Tiger and the Panzer IV as rounds strike her, face by
	// face, at rest on level ground and trained on a slope. Every place in a plate's room less edge_m (3 cm), all the kit
	// may strike, is on her drawn surface within 5 cm (straight in from it, or at an edge her nearest point), with nothing
	// of her in front of it along the round's way (3 m out); a strike cast behind her running gear (from inside her) is
	// moved onto what stands in front of it; her side aimed 0.1 m under its top edge is struck on it, never above it;
	// and what a plate costs.
	std::printf("\nSMALL ARMS OFF ARMOUR (Effects R2): HER PLATES AS THE KIT IS HANDED THEM (Vehicle::PlateAbout)\n");
	{
		const double EdgeM = 0.03, Near = 0.05;
		const char* PlateModels[2] = { "Tiger", "PanzerIV" };
		for (const char* Name : PlateModels)
		{
			std::string Text, LoadError;
			Model M;
			Dress D;
			D.Nation = "GER";
			if (!ReadFile(Content + "/Models/Battle/" + Name + ".json", Text) || !M.Load(Text, D, LoadError))
			{
				std::printf("  FAIL cannot read %s.json %s\n", Name, LoadError.c_str());
				++Failures;
				continue;
			}
			Tally OnHer, InFront, UnderEdge, Gear;
			long Plates = 0, Roomless = 0, WithFloor = 0, Moved = 0, EdgeAims = 0;
			double SumMs = 0.0, MostMs = 0.0, SumA = 0.0, SumB = 0.0, MovedMost = 0.0;
			for (int Posed = 0; Posed < 2; ++Posed)
			{
				const GroundFn Ground = Posed == 0 ? GroundFn([](double, double, double& OutZ, V3& OutNormal) { OutZ = 0.0; OutNormal = V3(0.0, 0.0, 1.0); })
					: GroundFn([](double X, double Y, double& OutZ, V3& OutNormal) { OutZ = 0.06 * X - 0.04 * Y + 3.0; OutNormal = V3(-0.06, 0.04, 1.0).Unit(); });
				Vehicle::Pin Pin;
				if (Posed == 1)
				{
					Pin.X = 120.0;
					Pin.Y = -45.0;
					Pin.Heading = 2.2;
					Pin.Turret = M.Turret >= 0 ? 57.0 : 0.0;
				}
				Vehicle V;
				V.PinPose(M, Pin, Ground);
				const std::vector<Place>& Pl = V.Bodies();
				size_t RootB = 0;
				for (size_t B = 0; B < M.Bodies.size(); ++B)
				{
					if (M.Bodies[B].Parent < 0)
					{
						RootB = B;
						break;
					}
				}
				// (her hull's ways, and how far she reaches along them: every drawn point of her)
				const V3 Fwd = Pl[RootB].Way(V3(1.0, 0.0, 0.0)).Unit(V3(1.0, 0.0, 0.0)), Side = Pl[RootB].Way(V3(0.0, 1.0, 0.0)).Unit(V3(0.0, 1.0, 0.0));
				const V3 Up = Pl[RootB].Way(V3(0.0, 0.0, 1.0)).Unit(V3(0.0, 0.0, 1.0)), Mid = Pl[RootB].At;
				double Lo[3] = { 1e9, 1e9, 1e9 }, Hi[3] = { -1e9, -1e9, -1e9 };
				for (size_t B = 0; B < M.Bodies.size() && B < Pl.size(); ++B)
				{
					const Body& Bo = M.Bodies[B];
					if (Bo.Name == "aerial" || Bo.Part < 0 || !M.Parts[static_cast<size_t>(Bo.Part)].bShown)
					{
						continue;
					}
					for (size_t K = 0; K + 2 < Bo.Near.Points.size(); K += 3)
					{
						const V3 P = Pl[B].Point(V3(Bo.Near.Points[K], Bo.Near.Points[K + 1], Bo.Near.Points[K + 2])) - Mid;
						const double Along[3] = { P.Dot(Fwd), P.Dot(Side), P.Dot(Up) };
						for (int A = 0; A < 3; ++A)
						{
							Lo[A] = std::min(Lo[A], Along[A]);
							Hi[A] = std::max(Hi[A], Along[A]);
						}
					}
				}
				auto At3 = [&](double F, double S, double U) { return Mid + Fwd * F + Side * S + Up * U; };
				auto Frac = [](double L, double H, double K) { return L + (H - L) * K; };
				// How far a place is from her drawn surface: the nearest point of any triangle of her shown bodies (the
				// slow way, every triangle; only where the ray straight in does not find her within 5 cm, as at an edge).
				auto Nearest = [&](const V3& Q)
				{
					double Best = 1e9;
					for (size_t B = 0; B < M.Bodies.size() && B < Pl.size(); ++B)
					{
						const Body& Bo = M.Bodies[B];
						if (Bo.Name == "aerial" || Bo.Part < 0 || !M.Parts[static_cast<size_t>(Bo.Part)].bShown)
						{
							continue;
						}
						const Mesh& Me = Bo.Near;
						auto Corner = [&](uint32_t I) { return Pl[B].Point(V3(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2])); };
						for (size_t K = 0; K + 2 < Me.Triangles.size(); K += 3)
						{
							// (the nearest point of a triangle: Ericson's regions, corners, edges, face)
							const V3 A = Corner(Me.Triangles[K]), Bv = Corner(Me.Triangles[K + 1]), C = Corner(Me.Triangles[K + 2]);
							const V3 Ab = Bv - A, Ac = C - A, Ap = Q - A;
							const double D1 = Ab.Dot(Ap), D2 = Ac.Dot(Ap);
							V3 Closest;
							if (D1 <= 0.0 && D2 <= 0.0)
							{
								Closest = A;
							}
							else
							{
								const V3 Bp = Q - Bv;
								const double D3 = Ab.Dot(Bp), D4 = Ac.Dot(Bp);
								const V3 Cp = Q - C;
								const double D5 = Ab.Dot(Cp), D6 = Ac.Dot(Cp);
								const double Vc = D1 * D4 - D3 * D2, Vb = D5 * D2 - D1 * D6, Va = D3 * D6 - D5 * D4;
								if (D3 >= 0.0 && D4 <= D3)
								{
									Closest = Bv;
								}
								else if (D6 >= 0.0 && D5 <= D6)
								{
									Closest = C;
								}
								else if (Vc <= 0.0 && D1 >= 0.0 && D3 <= 0.0)
								{
									Closest = A + Ab * (D1 / (D1 - D3));
								}
								else if (Vb <= 0.0 && D2 >= 0.0 && D6 <= 0.0)
								{
									Closest = A + Ac * (D2 / (D2 - D6));
								}
								else if (Va <= 0.0 && D4 - D3 >= 0.0 && D5 - D6 >= 0.0)
								{
									Closest = Bv + (C - Bv) * ((D4 - D3) / ((D4 - D3) + (D5 - D6)));
								}
								else
								{
									const double Sum = Va + Vb + Vc;
									Closest = Sum != 0.0 ? A + Ab * (Vb / Sum) + Ac * (Vc / Sum) : A;
								}
							}
							Best = std::min(Best, (Q - Closest).Length());
						}
					}
					return Best;
				};
				// Every place the kit may strike in a plate's room: on her within 5 cm, nothing of her in front of it.
				auto CheckRoom = [&](const Vehicle::PlateSpot& P, const V3& In, const std::string& Where, double Highest)
				{
					const double HA = P.A.Length(), HB = P.B.Length();
					const V3 Ua = P.A.Unit(V3(1.0, 0.0, 0.0)), Ub = P.B.Unit(V3(0.0, 0.0, 1.0));
					const double Ha = std::max(0.0, HA - EdgeM), Hb = std::max(0.0, HB - EdgeM);
					const bool bRoom = HA > 0.0 || HB > 0.0;
					// (a 9 by 9 lattice over it, at quarters: off the 3 cm lattice PlateAbout tries it on)
					for (int I = -4; I <= 4; ++I)
					{
						for (int J = -4; J <= 4; ++J)
						{
							if (!bRoom && (I != 0 || J != 0))
							{
								continue;
							}
							const V3 Q = bRoom ? P.Mid + Ua * (Ha * I / 4.0) + Ub * (Hb * J / 4.0) : P.At;
							double T1 = 0.0, T2 = 0.0;
							V3 N1, N2;
							// (her surface straight in from 6 cm out within 5 cm of the place, in front of it or behind; else the
							// nearest point of her, as where the place is on her edge)
							const bool bOn = V.RayNow(Q + P.Normal * 0.06, -P.Normal, 0.12, T1, N1);
							const double Off = bOn && std::fabs(T1 - 0.06) <= Near ? std::fabs(T1 - 0.06) : Nearest(Q);
							OnHer.Note(Off <= Near, Off, Where + ": a place in the room " + std::to_string(Off) + " m off her drawn surface");
							const bool bFirst = V.RayNow(Q - In * 3.0, In, 3.5, T2, N2);
							const double Hid = bFirst ? std::max(0.0, 3.0 - T2) : 1.0;
							InFront.Note(Hid <= Near, Hid, Where + ": something of her " + std::to_string(Hid) + " m in front of a place in the room");
							if (std::getenv("LAND_KIT_PLATE_SHOW") && (Off > Near || Hid > Near))
							{
								std::printf("    %s: at (%.2f %.2f %.2f) way out (%.2f %.2f %.2f), room %.2f by %.2f about (%.2f %.2f %.2f); place %d,%d (%.2f %.2f %.2f): %.3f m off her, %.3f m behind something (its way out %.2f %.2f %.2f)\n",
									Where.c_str(), P.At.X, P.At.Y, P.At.Z, P.Normal.X, P.Normal.Y, P.Normal.Z, HA, HB, P.Mid.X, P.Mid.Y, P.Mid.Z, I, J, Q.X, Q.Y, Q.Z, Off, Hid, N2.X, N2.Y, N2.Z);
							}
							if (Highest > -1e8)
							{
								const double Over = (Q - Mid).Dot(Up) - Highest;
								UnderEdge.Note(Over <= 0.02, std::max(0.0, Over), Where + ": a place in the room " + std::to_string(Over) + " m over her plate's top edge");
							}
						}
					}
				};
				auto Plate = [&](const V3& At, const V3& Normal, const V3& In, Vehicle::PlateSpot& Out)
				{
					const auto T0 = Clock::now();
					V.PlateAbout(At, Normal, In, Out);
					const double Took = Ms(T0, Clock::now());
					SumMs += Took;
					MostMs = std::max(MostMs, Took);
					++Plates;
					const double HA = Out.A.Length(), HB = Out.B.Length();
					Roomless += HA == 0.0 && HB == 0.0 ? 1 : 0;
					SumA += HA;
					SumB += HB;
					WithFloor += Out.Floor > -1e29 ? 1 : 0;
				};
				// Each face, rounds coming in square to it over a 7 by 7 grid of her: front, rear, left, right and top.
				struct Face
				{
					const char* Name;
					V3 In;
					int Across, Upward; // (her axes the grid runs along: 0 ahead, 1 side, 2 up)
					V3 From;            // (a place outside her on that side)
				};
				const Face Faces[5] = {
					{ "front", -Fwd, 1, 2, Fwd * (Hi[0] + 2.0) },
					{ "rear", Fwd, 1, 2, Fwd * (Lo[0] - 2.0) },
					{ "left", Side, 0, 2, Side * (Lo[1] - 2.0) },
					{ "right", -Side, 0, 2, Side * (Hi[1] + 2.0) },
					{ "top", -Up, 0, 1, Up * (Hi[2] + 2.0) },
				};
				const V3 Axis[3] = { Fwd, Side, Up };
				const double Ks[7] = { 0.08, 0.2, 0.35, 0.5, 0.65, 0.8, 0.92 };
				for (const Face& F : Faces)
				{
					for (double Ka : Ks)
					{
						for (double Kb : Ks)
						{
							const V3 S = Mid + F.From + Axis[F.Across] * Frac(Lo[F.Across], Hi[F.Across], Ka) + Axis[F.Upward] * Frac(F.Upward == 2 ? Lo[2] + 0.25 : Lo[F.Upward], Hi[F.Upward], Kb);
							double T = 0.0;
							V3 N;
							if (!V.RayNow(S, F.In, 20.0, T, N))
							{
								continue;
							}
							Vehicle::PlateSpot P;
							Plate(S + F.In * T, N, F.In, P);
							CheckRoom(P, F.In, std::string(Name) + " " + F.Name, -1e9);
						}
					}
				}
				// A strike cast behind her running gear (found from inside her, as the scar's cast may pass through her
				// wheels and tracks): moved onto what stands in front of it, nothing of her before it.
				for (int Sd = 0; Sd < 2; ++Sd)
				{
					const V3 In = Sd == 0 ? Side : -Side;
					for (double Ka : { 0.2, 0.4, 0.6, 0.8 })
					{
						for (double Kz : { 0.35, 0.55, 0.75 })
						{
							const V3 Inside = At3(Frac(Lo[0], Hi[0], Ka), 0.0, Frac(Lo[2], Hi[2], 0.25 * Kz));
							double T = 0.0;
							V3 N;
							if (!V.RayNow(Inside, -In, 5.0, T, N))
							{
								continue;
							}
							const V3 Behind = Inside - In * T;
							Vehicle::PlateSpot P;
							Plate(Behind, -In, In, P);
							const double By = (P.At - Behind).Length();
							Moved += By > 0.01 ? 1 : 0;
							MovedMost = std::max(MovedMost, By);
							double T2 = 0.0;
							V3 N2;
							const bool bFirst = V.RayNow(P.At - In * 3.0, In, 3.5, T2, N2);
							const double Hid = bFirst ? std::max(0.0, 3.0 - T2) : 1.0;
							Gear.Note(Hid <= Near, Hid, std::string(Name) + ": a strike cast behind her running gear left " + std::to_string(Hid) + " m behind what stands in front");
							CheckRoom(P, In, std::string(Name) + " behind her running gear", -1e9);
						}
					}
				}
				// Her side aimed 0.1 m under its top edge (found from the top down, square to her side, toward her ends where
				// no turret stands over it): every place the kit may strike on it, none over the edge.
				for (int Sd = 0; Sd < 2; ++Sd)
				{
					const V3 In = Sd == 0 ? Side : -Side;
					const double Outside = Sd == 0 ? Lo[1] - 2.0 : Hi[1] + 2.0;
					for (double Ka : { 0.12, 0.25, 0.88 })
					{
						const double Fa = Frac(Lo[0], Hi[0], Ka);
						double Top = -1e9;
						for (double Z = Hi[2]; Z > Lo[2] + 0.2 && Top < -1e8; Z -= 0.01)
						{
							double T = 0.0;
							V3 N;
							if (V.RayNow(At3(Fa, Outside, Z), In, 20.0, T, N) && N.Dot(-In) >= 0.9)
							{
								Top = Z;
							}
						}
						double T = 0.0;
						V3 N;
						if (Top < -1e8 || !V.RayNow(At3(Fa, Outside, Top - 0.1), In, 20.0, T, N))
						{
							continue;
						}
						++EdgeAims;
						Vehicle::PlateSpot P;
						Plate(At3(Fa, Outside, Top - 0.1) + In * T, N, In, P);
						CheckRoom(P, In, std::string(Name) + " side 0.1 m under its top edge", Top);
					}
				}
			}
			std::printf("  %-9s %ld plates cast (at rest, and trained on a slope): %ld with no room (every round at the strike), the room on average %.2f by %.2f m about its middle, %ld with her own floor under it; %ld cast behind her running gear moved onto it (as far as %.2f m); %ld aims 0.1 m under her side's top edge; %.3f ms a plate on average, %.2f ms the most\n",
				Name, Plates, Roomless, Plates ? 2.0 * SumA / static_cast<double>(Plates) : 0.0, Plates ? 2.0 * SumB / static_cast<double>(Plates) : 0.0, WithFloor, Moved, MovedMost, EdgeAims,
				Plates ? SumMs / static_cast<double>(Plates) : 0.0, MostMs);
			Report("every place the kit may strike, on her drawn surface (5 cm)", OnHer, "m");
			Report("nothing of her in front of it along the round's way", InFront, "m");
			Report("a strike cast behind her running gear, moved in front", Gear, "m");
			Report("her side aimed 0.1 m under its top edge: none over it", UnderEdge, "m");
		}
	}

	// ---- The battle.
	std::string Recording, FatesText, Error;
	if (!ReadFile(RecordingPath, Recording))
	{
		std::printf("FAIL cannot read %s\n", RecordingPath.c_str());
		return 1;
	}
	Fates::Table Table;
	const bool bTable = ReadFile(Content + "/Data/Battle/fates.json", FatesText) && Table.Load(FatesText, Error);
	if (!bTable)
	{
		std::printf("  (no Fates table: %s; a fixed one in four brews up)\n", Error.c_str());
	}
	const Battle::ReadFn Read = [&Content](const std::string& File, std::string& Out) { return ReadFile(Content + "/Models/Battle/" + File, Out); };
	const GroundFn Flat = [](double, double, double& OutZ, V3& OutNormal)
	{
		OutZ = 0.0;
		OutNormal = V3(0.0, 0.0, 1.0);
	};
	// (life seconds a battle second, as the scene plays the fight: LAND_KIT_LIFE, 2 by default; 1 as before)
	const char* LifeEnv = std::getenv("LAND_KIT_LIFE");
	Battle B;
	B.Life = LifeEnv ? std::atof(LifeEnv) : 2.0;
	// (LAND_BREW_WAIT=least,most: release 2's brew-up wait, life's seconds (LandBail, -TheaterLandBailOut))
	if (const char* Wait = std::getenv("LAND_BREW_WAIT"))
	{
		B.BrewWaitLeast = std::atof(Wait);
		const char* Comma = std::strchr(Wait, ',');
		B.BrewWaitMost = Comma ? std::atof(Comma + 1) : B.BrewWaitLeast;
	}
	// (the vehicles' measured fire odds, by the face struck: Content/Data/Battle/performance-land.json)
	std::map<std::string, FireOdds> Fires;
	{
		std::string PerfText, PerfError;
		if (!ReadFile(Content + "/Data/Battle/performance-land.json", PerfText) || !ReadFireOdds(PerfText, Fires, PerfError))
		{
			std::printf("  (no performance file: %s; kills look by the fates table alone)\n", PerfError.c_str());
		}
	}
	B.Fires = Fires;
	// (captured kit: LAND_KIT_CAPTURED=1 every other attacker tank block drawn as the defender's tank, taken from them)
	B.bCapturedTest = std::getenv("LAND_KIT_CAPTURED") != nullptr;
	const auto L0 = Clock::now();
	if (!B.Load(Recording, Read, Flat, bTable ? &Table : nullptr, 79, Error))
	{
		std::printf("FAIL the battle: %s\n", Error.c_str());
		return 1;
	}
	for (const std::string& Line : B.StandIns()) std::printf("  stand-in: %s\n", Line.c_str()); // (a model not here: the nearest generic)
	if (B.bCapturedTest)
	{
		std::map<std::string, int> Coats;
		for (const Vehicle& V : B.Vehicles())
			++Coats[V.Look().Name + ": " + V.Look().Coat];
		for (const auto& KV : Coats) std::printf("  coat: %s (%d)\n", KV.first.c_str(), KV.second);
	}
	const auto L1 = Clock::now();
	{
		Json Doc;
		const auto P0 = Clock::now();
		Doc.Parse(Recording, Error);
		const auto P1 = Clock::now();
		std::printf("\nTHE BATTLE %s: %.2f MB, %.1f s, %d/%d; %zu vehicles, %zu rounds. Loaded in %.0f ms (recording JSON %.0f ms; models, cutting, "
			"painting and every vehicle worked out ahead)\n",
			RecordingPath.substr(RecordingPath.find_last_of('/') + 1).c_str(), static_cast<double>(Recording.size()) / 1e6, B.End, B.Month, B.Year,
			B.Vehicles().size(), B.Rounds().size(), Ms(L0, L1), Ms(P0, P1));
	}
	// ---- Effects (R2), small arms off armour (-TheaterLandRicochet, Battle::bKeepPlates): the same battle made with every
	// rifle's and machine gun's strike's plate worked out as she is made, so the game's frame only turns it by her pose
	// (PlateNow). Each kept plate, at her pose as she stood when it struck, is the plate PlateAbout searches out there (to a
	// millimetre); PlateNow costs well under 0.3 ms the most (the game's rule step asks it once a burst on armour, three in
	// a frame at worst); and what keeping them adds to the battle's making.
	std::printf("\nSMALL ARMS OFF ARMOUR (Effects R2): PLATES KEPT AS THE BATTLE IS MADE (Battle::bKeepPlates, Vehicle::PlateNow)\n");
	{
		Battle KB;
		KB.Life = B.Life;
		KB.BrewWaitLeast = B.BrewWaitLeast;
		KB.BrewWaitMost = B.BrewWaitMost;
		KB.Fires = Fires;
		KB.bCapturedTest = B.bCapturedTest;
		KB.bKeepPlates = true;
		// (armour as the game has it, LandPlay::ArmouredVehicle, which this test does not build: here every tank and gun
		// is armour and every lorry soft, so a lorry's strikes are never searched)
		KB.Armoured = [](const TrackWords& W, const std::string&) { return W.Role != "lorry"; };
		std::string KeepError;
		const auto K0 = Clock::now();
		if (!KB.Load(Recording, Read, Flat, bTable ? &Table : nullptr, 79, KeepError))
		{
			std::printf("  FAIL the battle with its plates kept: %s\n", KeepError.c_str());
			++Failures;
		}
		const auto K1 = Clock::now();
		// (every small arm's round that struck a vehicle, by the moment it struck)
		struct Struck { double L; size_t V; int N; std::string Face; };
		std::vector<Struck> List;
		for (const Round& R : KB.Rounds())
		{
			if ((R.W != "rifle" && R.W != "mg") || R.R.empty() || R.R == "miss")
			{
				continue;
			}
			for (size_t I = 0; I < KB.Vehicles().size(); ++I)
			{
				const TrackWords& W = KB.Vehicles()[I].Words();
				if (R.On == W.Block && R.Ok == W.K && R.Oj == W.J)
				{
					List.push_back({ R.L, I, R.N, R.Face.empty() ? std::string("front") : R.Face });
				}
			}
		}
		std::stable_sort(List.begin(), List.end(), [](const Struck& A, const Struck& Bs) { return A.L < Bs.L; });
		Tally Same, Cost;
		long Kept = 0, Roomed = 0, Wanted = 0, SoftKept = 0;
		for (const Struck& S : List)
		{
			const bool bSoft = KB.Vehicles()[S.V].Words().Role == "lorry";
			Wanted += bSoft ? 0 : 1;
			SoftKept += bSoft && KB.Vehicles()[S.V].PlateKept(S.N) ? 1 : 0;
		}
		const size_t Waiting = KB.PlatesWaiting(-1e300);
		double SumUs = 0.0, MostUs = 0.0;
		const size_t Every = std::max<size_t>(1, List.size() / 120); // (about 120 of them searched again: each a few ms)
		for (size_t I = 0; I < List.size(); ++I)
		{
			const Struck& S = List[I];
			KB.Update(S.L);
			const Vehicle& V = KB.Vehicles()[S.V];
			Vehicle::PlateSpot Got;
			const auto C0 = Clock::now();
			const bool bGot = V.PlateNow(S.N, Got);
			const double Us = 1000.0 * Ms(C0, Clock::now());
			SumUs += Us;
			MostUs = std::max(MostUs, Us);
			Cost.Note(Us < 300.0, Us, "round " + std::to_string(S.N) + ": PlateNow took " + std::to_string(Us) + " microseconds");
			if (!bGot || !V.PlateKept(S.N))
			{
				continue; // (not kept: PlateNow gives her skin's point, no room)
			}
			++Kept;
			Roomed += Got.A.Length() > 0.0 || Got.B.Length() > 0.0 ? 1 : 0;
			if (I % Every != 0)
			{
				continue;
			}
			// (searched again at her pose now, as it struck)
			V3 At, Normal;
			if (!V.StruckNow(S.N, At, Normal))
			{
				continue;
			}
			const V3 In = V.Root.Way(S.Face == "rear" ? V3(1.0, 0.0, 0.0) : S.Face == "left" ? V3(0.0, 1.0, 0.0) : S.Face == "right" ? V3(0.0, -1.0, 0.0)
				: S.Face == "top" ? V3(0.0, 0.0, -1.0) : V3(-1.0, 0.0, 0.0)).Unit(V3(-1.0, 0.0, 0.0));
			Vehicle::PlateSpot Full;
			V.PlateAbout(At, Normal, In, Full);
			const double Off = std::max({ (Got.At - Full.At).Length(), (Got.Mid - Full.Mid).Length(), (Got.A - Full.A).Length(), (Got.B - Full.B).Length(),
				(Got.Normal - Full.Normal).Length(), (Got.Floor > -1e29) != (Full.Floor > -1e29) ? 1.0 : Got.Floor > -1e29 ? std::fabs(Got.Floor - Full.Floor) : 0.0 });
			Same.Note(Off <= 0.001, Off, Who(V) + " round " + std::to_string(S.N) + ": kept plate " + std::to_string(Off) + " m off the plate searched at her pose then");
		}
		std::printf("  %zu small-arms strikes on %zu vehicles, %ld of them on armour: %ld plates kept (%ld with a room about the strike, the rest a point on her plate alone; a strike whose search finds no plate is drawn at her skin's point), %zu left unsearched, %ld kept on a lorry; the battle made in %.0f ms with them kept (%.0f ms without); PlateNow %.1f microseconds on average, %.1f the most\n",
			List.size(), KB.Vehicles().size(), Wanted, Kept, Roomed, Waiting, SoftKept, Ms(K0, K1), Ms(L0, L1), List.empty() ? 0.0 : SumUs / static_cast<double>(List.size()), MostUs);
		Report("a kept plate, the plate searched at her pose then (1 mm)", Same, "m");
		Report("PlateNow under 0.3 ms", Cost, "us");
		if (Kept == 0 && Wanted > 0)
		{
			std::printf("  FAIL no plate kept\n");
			++Failures;
		}
		if (Waiting > 0)
		{
			std::printf("  FAIL %zu strikes on armour left unsearched once the battle was loaded\n", Waiting);
			++Failures;
		}
		if (SoftKept > 0)
		{
			std::printf("  FAIL %ld plates searched and kept on a lorry (never drawn off a plate)\n", SoftKept);
			++Failures;
		}
	}

	std::vector<Vehicle>& All = B.Vehicles();
	for (const Vehicle& V : All)
	{
		const TrackWords& W = V.Words();
		size_t OnSurface = 0;
		for (const Scar& S : V.Scars())
		{
			OnSurface += S.bOnSurface ? 1 : 0;
		}
		std::printf("  %-28s %-3s %-3s  %4zu kept points  %3zu shots  %2zu scars (%zu on her surface)%s", Who(V).c_str(), V.Side.c_str(),
			V.Nation.c_str(), W.At.size() / 4, W.Shots.size(), V.Scars().size(), OnSurface, W.Died >= 0.0 ? "" : "\n");
		if (W.Died >= 0.0)
		{
			std::printf("  killed at %.1f s: %s (%s), burns %.1f s to the end\n", W.Died, V.FateKey.c_str(), V.bBrewUp ? "brews up, turret thrown" : "knocked out",
				V.BurnUntil - V.BurnFrom);
		}
	}

	// ---- Played at 60 frames a second, every check on every frame.
	std::map<const Model*, std::vector<double>> Reaches;
	for (const auto& Named : B.Models())
	{
		Reaches[&Named.second] = Reach(Named.second);
	}
	const size_t Count = All.size();
	std::vector<double> LastTurret(Count, 0.0), LastTraverse(Count, 0.0);
	std::vector<V3> DeadAt(Count);
	std::vector<Quat> DeadQ(Count);
	std::vector<double> DeadRunL(Count, 0.0), DeadRunR(Count, 0.0);
	std::vector<uint8_t> bHaveDead(Count, 0);
	std::vector<std::vector<float>> LinkBuffer(Count);
	Tally Nan, Joined, Trains, Traverses, Recoils, Still, Sink, Float, EventTimes, Flashes;
	// TK1: how far each vehicle's merged bodies stand off where her far pieces draw them (FarOff), every frame. A tank or
	// a lorry is always drawn by her pieces past the hand-over (the scene's give is 0.25 m at 140 m, more beyond); a gun
	// only while she is that near her rest (a barrel laid high or trails swung for the tow keep her own far meshes).
	std::map<const Model*, FarSplit> Splits;
	for (const auto& Named : B.Models())
	{
		SplitFar(Named.second, Splits[&Named.second]);
	}
	Tally FarFit;
	double GunOff = 0.0, FarOffSum = 0.0;
	long GunFrames = 0, GunFit = 0;
	std::vector<double> FrameMs, LinkMs;
	std::map<std::string, int> FxCount;
	std::map<int, std::vector<double>> MuzzleTimes; // by vehicle
	const double Dt = 1.0 / 60.0;
	const long Frames = static_cast<long>(std::ceil(B.End / Dt)) + 1;
	double Previous = -1.0;
	double LowestTrack = 1e9, HighestTrack = -1e9, LowestWheel = 1e9, HighestWheel = -1e9;
	// Their track marks as the game lays them (LandBattle::MakeRuts), all on grass here: the most the battle can lay.
	LandCrater::Ruts Marks;
	Marks.Begin([](double, double) { return 0.0; }, nullptr, [](double, double) { return LandCrater::Ruts::Under::Grass; });
	double MarksMs = 0.0;
	for (long Frame = 0; Frame < Frames; ++Frame)
	{
		const double T = std::min(B.End, static_cast<double>(Frame) * Dt);
		const auto U0 = Clock::now();
		B.Update(T);
		const auto U1 = Clock::now();
		FrameMs.push_back(Ms(U0, U1));
		for (const Event& E : B.Events)
		{
			const bool bOk = Finite(E.At) && Finite(E.Facing) && std::isfinite(E.T);
			Nan.Note(bOk, 0.0, "an event's place at " + std::to_string(E.T));
			EventTimes.Note(E.T > Previous - 1e-9 && E.T <= T + 1e-9, 0.0, E.Fx + " at " + std::to_string(E.T) + " given at " + std::to_string(T));
			++FxCount[E.Fx];
			if (E.Fx.compare(0, 12, "muzzle_flash") == 0 && (E.Socket == "muzzle"))
			{
				MuzzleTimes[E.Vehicle].push_back(E.T);
			}
		}
		B.Events.clear();
		for (size_t I = 0; I < Count; ++I)
		{
			const Vehicle& V = All[I];
			const Model& M = V.Look();
			const TrackWords& W = V.Words();
			const bool bDead = W.Died >= 0.0 && T >= W.Died;
			const std::vector<Place>& P = V.Bodies();
			// No NaN anywhere.
			bool bFinite = Finite(V.Root.At) && Finite(V.Root.Q);
			for (const Place& Pl : P)
			{
				bFinite = bFinite && Finite(Pl.At) && Finite(Pl.Q);
			}
			Nan.Note(bFinite, 0.0, Who(V) + " at " + std::to_string(T));
			{
				const auto F0 = Clock::now();
				const double Off = FarOff(M, Splits[&M], P);
				FarOffSum += Ms(F0, Clock::now());
				if (W.Role == "gun")
				{
					GunOff = std::max(GunOff, Off);
					++GunFrames;
					GunFit += Off <= 0.25 ? 1 : 0;
				}
				else
				{
					FarFit.Note(Off <= 0.25, Off, Who(V) + " stands " + std::to_string(Off) + " m off her far pieces at " + std::to_string(T));
				}
			}
			// The bodies stay joined: each one's pivot where its parent carries it, give or take what its clips shift it.
			const std::vector<double>& R = Reaches[&M];
			for (size_t Bo = 0; Bo < M.Bodies.size(); ++Bo)
			{
				const Body& Child = M.Bodies[Bo];
				if (Child.Parent < 0)
				{
					continue;
				}
				const Body& Parent = M.Bodies[static_cast<size_t>(Child.Parent)];
				const V3 Want = P[static_cast<size_t>(Child.Parent)].Point(Child.Pivot - Parent.Pivot);
				const double Off = (P[Bo].At - Want).Length();
				const double Allowed = R[static_cast<size_t>(Child.Part)] + 0.002;
				Joined.Note(Off <= Allowed, Off - R[static_cast<size_t>(Child.Part)],
					Who(V) + " " + Child.Name + " " + std::to_string(Off) + " m off at " + std::to_string(T));
			}
			// The turret never trains faster than its control (nor a gun's traverse).
			if (Frame > 0 && !bDead && M.Turret >= 0 && W.Role == "tank")
			{
				const double Step = std::fabs(WrapDeg(V.TurretDeg - LastTurret[I]));
				const double Most = M.TurretDps * B.Life * Dt + 1e-3;
				Trains.Note(Step <= Most, Step / Dt, Who(V) + " " + std::to_string(Step / Dt) + " deg/s at " + std::to_string(T));
			}
			if (Frame > 0 && !bDead && M.Traverse >= 0)
			{
				const double Step = std::fabs(V.TraverseDeg - LastTraverse[I]);
				Traverses.Note(Step <= M.TraverseDps * B.Life * Dt + 1e-3, Step / Dt, Who(V) + " traverse at " + std::to_string(T));
			}
			LastTurret[I] = V.TurretDeg;
			LastTraverse[I] = V.TraverseDeg;
			// The barrel recoils on every shot stamp, within a frame.
			for (double S : W.Shots)
			{
				if (S >= Previous && S < T && (W.Died < 0.0 || S < W.Died))
				{
					Recoils.Note(V.Recoil > 0.005, V.Recoil, Who(V) + " shot at " + std::to_string(S) + ": recoil " + std::to_string(V.Recoil));
				}
			}
			// A knocked-out vehicle never moves again.
			if (bDead)
			{
				if (!bHaveDead[I])
				{
					bHaveDead[I] = 1;
					DeadAt[I] = V.Root.At;
					DeadQ[I] = V.Root.Q;
					DeadRunL[I] = V.RunLeft;
					DeadRunR[I] = V.RunRight;
				}
				const double Moved = (V.Root.At - DeadAt[I]).Length() + std::fabs(V.Root.Q.W - DeadQ[I].W) + std::fabs(V.Root.Q.X - DeadQ[I].X)
					+ std::fabs(V.Root.Q.Y - DeadQ[I].Y) + std::fabs(V.Root.Q.Z - DeadQ[I].Z) + std::fabs(V.RunLeft - DeadRunL[I])
					+ std::fabs(V.RunRight - DeadRunR[I]) + std::fabs(V.Speed);
				Still.Note(Moved < 1e-6, Moved, Who(V) + " moved after her death at " + std::to_string(T));
			}
			// Nothing sinks into the ground or floats over it: a tank's tracks (every tenth of a second, the links posed),
			// a gun's or a lorry's wheels (every frame).
			if (!bDead && M.TrackBody[0] >= 0 && Frame % 6 == 0)
			{
				for (int Side = -1; Side <= 1; Side += 2)
				{
					const int TB = M.TrackBody[Side < 0 ? 0 : 1];
					const Body& Tb = M.Bodies[static_cast<size_t>(TB)];
					std::vector<float>& Buf = LinkBuffer[I];
					Buf.resize(Tb.Near.Points.size());
					const auto K0 = Clock::now();
					V.PoseLinks(Side, Buf.data());
					const auto K1 = Clock::now();
					LinkMs.push_back(Ms(K0, K1));
					double Low = 1e9;
					for (size_t J = 0; J + 2 < Buf.size(); J += 3)
					{
						Low = std::min(Low, P[static_cast<size_t>(TB)].Point(V3(Buf[J], Buf[J + 1], Buf[J + 2])).Z);
					}
					LowestTrack = std::min(LowestTrack, Low);
					HighestTrack = std::max(HighestTrack, Low);
					Sink.Note(Low >= -0.05, -Low, Who(V) + " track sinks " + std::to_string(-Low) + " m at " + std::to_string(T));
					Float.Note(Low <= 0.05, Low, Who(V) + " track floats " + std::to_string(Low) + " m at " + std::to_string(T));
				}
			}
			if (!bDead && M.TrackBody[0] < 0)
			{
				for (int Wh : M.Wheels)
				{
					const Part& Pa = M.Parts[static_cast<size_t>(Wh)];
					if (Pa.Body < 0 || Pa.Pivot.Z - Pa.Radius > 0.05)
					{
						continue;
					}
					const double Low = P[static_cast<size_t>(Pa.Body)].At.Z - Pa.Radius;
					LowestWheel = std::min(LowestWheel, Low);
					HighestWheel = std::max(HighestWheel, Low);
					Sink.Note(Low >= -0.05, -Low, Who(V) + " " + Pa.Name + " sinks " + std::to_string(-Low) + " m at " + std::to_string(T));
					Float.Note(Low <= 0.05, Low, Who(V) + " " + Pa.Name + " floats " + std::to_string(Low) + " m at " + std::to_string(T));
				}
			}
		}
		const auto M0 = Clock::now();
		for (const Vehicle& V : All)
		{
			if (V.bDead || V.Fade < 0.5)
			{
				Marks.Lift(V.Index);
				continue;
			}
			if (V.bMoving)
			{
				Marks.Drive(V.Index, V.DustLeft.X, V.DustLeft.Y, V.DustRight.X, V.DustRight.Y, LandCrater::Ruts::TrackWidthOf(V.Look().Name, V.Look().Kind));
			}
		}
		MarksMs += Ms(M0, Clock::now());
		Previous = T;
	}
	// Every shot stamp has its muzzle flash, at the muzzle, at its time.
	for (size_t I = 0; I < Count; ++I)
	{
		const TrackWords& W = All[I].Words();
		const std::vector<double>& Got = MuzzleTimes[static_cast<int>(I)];
		for (double S : W.Shots)
		{
			if (W.Died >= 0.0 && S >= W.Died)
			{
				continue;
			}
			double Best = 1e9;
			for (double G : Got)
			{
				Best = std::min(Best, std::fabs(G - S));
			}
			Flashes.Note(Best < 0.051, Best, Who(All[I]) + " no muzzle flash for the shot at " + std::to_string(S));
		}
	}

	std::printf("\nTHE CHECKS (%ld frames at 60 a second, every vehicle on every frame)\n", Frames);
	Report("no NaN in any body's place or any event", Nan, "");
	Report("bodies stay joined (off the joint beyond their clips)", Joined, "m");
	Report("the turret never trains faster than its control", Trains, "deg/s");
	Report("a gun's traverse never faster than its handwheel", Traverses, "deg/s");
	Report("the barrel recoils on every shot stamp within a frame", Recoils, "m slid");
	Report("every shot stamp has its muzzle flash (within 0.05 s)", Flashes, "s");
	Report("a knocked-out vehicle never moves again", Still, "");
	Report("nothing sinks into flat ground by more than 5 cm", Sink, "m");
	Report("nothing floats over flat ground by more than 5 cm", Float, "m");
	Report("events come once, in the frame they fall in", EventTimes, "");
	Report("tanks and lorries within 0.25 m of their far pieces (TK1)", FarFit, "m");
	std::printf("      (guns: worst %.3f m off their far pieces, within 0.25 m in %.0f%% of frames; FarOff for every vehicle %.3f ms a frame)\n", GunOff,
		GunFrames ? 100.0 * static_cast<double>(GunFit) / static_cast<double>(GunFrames) : 0.0, FarOffSum / static_cast<double>(Frames));
	{
		// The marks: the whole battle's, uncapped, in a size the scene draws without a thought.
		long Tris = 0;
		int Sections = 0;
		for (int P = 0; P < Marks.Patches(); ++P)
		{
			for (const LandField::Mesh& M : Marks.PatchAt(P).Sections)
			{
				Tris += static_cast<long>(M.Triangles.size() / 3);
				++Sections;
			}
		}
		Tally MarksT;
		MarksT.Note(Marks.Pieces() > 100 && Marks.Pieces() < LandCrater::Ruts::Most * 3 / 4 && Tris < 700000, static_cast<double>(Tris),
			std::to_string(Marks.Pieces()) + " pieces, " + std::to_string(Tris) + " triangles");
		std::printf("      track marks over the battle (all on grass): %d pieces, %ld triangles in %d sections of %d patches; %.3f ms a frame for every vehicle\n",
			Marks.Pieces(), Tris, Sections, Marks.Patches(), MarksMs / static_cast<double>(Frames));
		Report("the battle's track marks, uncapped and under 700,000 triangles", MarksT, "triangles");
	}
	std::printf("  (lowest point of a track: %.3f to %.3f m; of a gun's or lorry's wheel: %.3f to %.3f m)\n", LowestTrack, HighestTrack, LowestWheel, HighestWheel);

	std::printf("\nTHE COST\n");
	double Sum = 0.0;
	for (double F : FrameMs)
	{
		Sum += F;
	}
	std::printf("  every vehicle posed, a frame: mean %.3f ms, 99th %.3f ms, worst %.3f ms (%zu vehicles, events included)\n",
		Sum / static_cast<double>(FrameMs.size()), Percentile(FrameMs, 0.99), Percentile(FrameMs, 1.0), Count);
	double LinkSum = 0.0;
	for (double F : LinkMs)
	{
		LinkSum += F;
	}
	std::printf("  one track's links re-posed on the CPU (PoseLinks): mean %.3f ms, worst %.3f ms; all 32 tracks a frame would be %.2f ms\n",
		LinkMs.empty() ? 0.0 : LinkSum / static_cast<double>(LinkMs.size()), Percentile(LinkMs, 1.0),
		LinkMs.empty() ? 0.0 : 32.0 * LinkSum / static_cast<double>(LinkMs.size()));

	std::printf("\nTHE EVENTS\n  ");
	int Column = 0;
	for (const auto& Named : FxCount)
	{
		std::printf("%s %d   ", Named.first.c_str(), Named.second);
		if (++Column % 6 == 0)
		{
			std::printf("\n  ");
		}
	}
	std::printf("\n");

	// ---- Once more over rolling ground, a frame every twentieth of a second: no NaN, and how well she sits on it.
	const GroundFn Hills = [](double X, double Y, double& OutZ, V3& OutNormal)
	{
		OutZ = 1.5 * std::sin(X / 45.0) * std::cos(Y / 60.0) + 0.6 * std::sin((X + Y) / 23.0);
		const double DX = 1.5 / 45.0 * std::cos(X / 45.0) * std::cos(Y / 60.0) + 0.6 / 23.0 * std::cos((X + Y) / 23.0);
		const double DY = -1.5 / 60.0 * std::sin(X / 45.0) * std::sin(Y / 60.0) + 0.6 / 23.0 * std::cos((X + Y) / 23.0);
		OutNormal = V3(-DX, -DY, 1.0).Unit();
	};
	Battle Rolling;
	Rolling.Fires = Fires;
	if (!Rolling.Load(Recording, Read, Hills, bTable ? &Table : nullptr, 79, Error))
	{
		std::printf("FAIL the battle on rolling ground: %s\n", Error.c_str());
		return 1;
	}
	Tally RollNan;
	double Gap = 0.0, GapSum = 0.0, MostPitch = 0.0, MostRoll = 0.0;
	long Gaps = 0;
	for (long Frame = 0; Frame * 0.05 <= Rolling.End; ++Frame)
	{
		const double T = static_cast<double>(Frame) * 0.05;
		Rolling.Update(T);
		Rolling.Events.clear();
		for (const Vehicle& V : Rolling.Vehicles())
		{
			bool bFinite = Finite(V.Root.At) && Finite(V.Root.Q);
			for (const Place& Pl : V.Bodies())
			{
				bFinite = bFinite && Finite(Pl.At) && Finite(Pl.Q);
			}
			RollNan.Note(bFinite, 0.0, Who(V) + " at " + std::to_string(T));
			MostPitch = std::max(MostPitch, std::fabs(V.Pitch));
			MostRoll = std::max(MostRoll, std::fabs(V.Roll));
			for (int C = 0; C < 4; ++C)
			{
				const V3 At = V.Root.Point(V.Look().Foot[C]);
				double Z = 0.0;
				V3 Up;
				Hills(At.X, At.Y, Z, Up);
				Gap = std::max(Gap, std::fabs(At.Z - Z));
				GapSum += std::fabs(At.Z - Z);
				++Gaps;
			}
		}
	}
	std::printf("\nON ROLLING GROUND (hills 1.5 m high, 280 m apart; ripples 0.6 m, 140 m apart)\n");
	Report("no NaN", RollNan, "");
	std::printf("  her footprint's corners off the ground: mean %.3f m, worst %.3f m (a plane fitted to four points on curved ground); "
		"pitch up to %.1f deg, roll up to %.1f deg\n", GapSum / static_cast<double>(std::max(1L, Gaps)), Gap, MostPitch, MostRoll);

	// How a pierced tank ends by the face struck (Brian, 22 November: a rear hit on a Tiger or a Panther raises the
	// chance she blows up): the draw keeps the measured odds, a rear hit blows a Tiger and a Panther up more often than
	// a front one, and the fates table has a look for every end on every tank family.
	std::printf("\nHOW A KNOCKED-OUT TANK ENDS, BY THE FACE STRUCK (performance-land.json; blow / burn / still in 100,000 draws)\n");
	{
		Tally Odds, RearWorse, EveryEnd;
		const int Draws = 100000;
		for (const auto& Kv : Fires)
		{
			for (const char* Face : { "front", "side", "rear" })
			{
				int Blow = 0, Burn = 0;
				for (int K = 0; K < Draws; ++K)
				{
					const uint64_t H = (static_cast<uint64_t>(K) + 1) * 0x9E3779B97F4A7C15ull;
					const double U1 = static_cast<double>((H ^ (H >> 31)) >> 11) / 9007199254740992.0;
					const uint64_t H2 = (H ^ 0xB10Bull) * 0xBF58476D1CE4E5B9ull;
					const double U2 = static_cast<double>((H2 ^ (H2 >> 29)) >> 11) / 9007199254740992.0;
					const std::string End = FireEnd(Kv.second, Face, U1, U2);
					Blow += End == "blow";
					Burn += End == "burn";
				}
				const double Fire = std::string(Face) == "front" ? Kv.second.Front : std::string(Face) == "rear" ? Kv.second.Rear : Kv.second.Side;
				const double Off = std::max(std::fabs(Blow / static_cast<double>(Draws) - Fire * Kv.second.Explodes),
					std::fabs(Burn / static_cast<double>(Draws) - Fire * (1.0 - Kv.second.Explodes)));
				Odds.Note(Off < 0.01, Off, Kv.first + " " + Face);
				if ((Kv.first == "Tiger" || Kv.first == "Panther") && Kv.second.Explodes > 0.0)
				{
					if (std::string(Face) == "front") std::printf("  %-8s front: blows up %4.1f%%, burns %4.1f%%", Kv.first.c_str(), 100.0 * Blow / Draws, 100.0 * Burn / Draws);
					if (std::string(Face) == "rear") std::printf(";  rear: blows up %4.1f%%, burns %4.1f%%\n", 100.0 * Blow / Draws, 100.0 * Burn / Draws);
				}
			}
			if (Kv.first == "Tiger" || Kv.first == "Panther")
			{
				RearWorse.Note(Kv.second.Rear > Kv.second.Front && Kv.second.Explodes > 0.0, Kv.second.Rear - Kv.second.Front, Kv.first + " no worse from the rear");
			}
		}
		if (bTable)
		{
			for (const char* Family : { "tank_light", "tank_medium", "tank_heavy", "spgun" })
			{
				for (const char* End : { "blow", "burn", "still" })
				{
					bool bAny = false;
					for (const Fates::Way& W : Table.Ways())
					{
						if (W.How == End && std::find(W.Families.begin(), W.Families.end(), Family) != W.Families.end()) bAny = true;
					}
					// (a self-propelled gun has no turret to throw: her blow is shown burning)
					EveryEnd.Note(bAny || (std::string(Family) == "spgun" && std::string(End) == "blow"), 0.0, std::string(Family) + " has no look for " + End);
				}
			}
		}
		Report("each end drawn at its measured odds (within 1 in 100)", Odds, "");
		Report("a Tiger and a Panther blow up more from the rear", RearWorse, "");
		Report("the fates table shows every end on every tank", EveryEnd, "");
		for (const Battle::KillEnd& E : B.Ends)
		{
			std::printf("  this battle: %-9s hit %-6s -> %-6s %s\n", E.Model.c_str(), E.Face.empty() ? "(none)" : E.Face.c_str(), E.End.empty() ? "(any)" : E.End.c_str(), E.Fate.c_str());
		}
		// Where the rules name her end on her killing round (fate: out, burn or blow, as their recordings carry it from
		// 22 November), the scene shows theirs: this battle again with every kill round named, by turns.
		if (bTable)
		{
			Tally Theirs;
			const char* const Named[] = { "out", "burn", "blow" };
			for (const char* Fate : Named)
			{
				std::string Text = Recording;
				size_t At = 0, Put = 0;
				while ((At = Text.find("\"r\":\"kill\"", At)) != std::string::npos)
				{
					const std::string Add = std::string(",\"fate\":\"") + Fate + "\"";
					Text.insert(At + 10, Add);
					At += 10 + Add.size();
					++Put;
				}
				Battle Named1;
				Named1.Fires = Fires;
				std::string NamedError;
				if (Put == 0 || !Named1.Load(Text, Read, Flat, &Table, 79, NamedError))
				{
					Theirs.Note(Put == 0, 0.0, std::string("could not load the battle with fate ") + Fate + ": " + NamedError);
					continue;
				}
				const std::string Want = std::string(Fate) == "out" ? "still" : Fate;
				for (const Battle::KillEnd& E : Named1.Ends)
				{
					// (a tank whose table has no blow for her family shows it burning, as the scene's own draw does)
					const bool bOk = E.End == Want || (Want == "blow" && E.End == "burn" && E.Fate.find("burn") != std::string::npos);
					Theirs.Note(bOk || E.End.empty(), 0.0, E.Model + " told " + Fate + " ended " + E.End);
				}
			}
			Report("the rules' own end on a killing round is the one shown", Theirs, "");
		}
	}

	// ---- The desert coats (release 2): a model's swaps.paint.by_ground rows are read before by_month on a field of that
	// ground; a month none of them holds, and every other ground, is dressed as before.
	{
		std::printf("\nTHE DESERT COATS (swaps.paint.by_ground)\n");
		// (another folder of model files, for those not in the game yet: the Matilda's Light Stone)
		const char* More = std::getenv("LAND_KIT_MORE");
		auto ReadModel = [&Content, More](const std::string& Name, std::string& Out)
		{
			return ReadFile(Content + "/Models/Battle/" + Name + ".json", Out) || (More && ReadFile(std::string(More) + "/" + Name + ".json", Out));
		};
		auto CoatOf = [](const std::string& Text, const std::string& Nation, int Year, int Month, const std::string& Terrain, std::string& Out)
		{
			Dress D;
			D.Nation = Nation;
			D.Year = Year;
			D.Month = Month;
			D.Terrain = Terrain;
			Model M;
			const bool bOk = M.Load(Text, D, Out);
			if (bOk)
			{
				Out = M.Coat;
			}
			return bOk;
		};
		struct Want
		{
			const char* Name;
			const char* Nation;
			int Year, Month;
			const char* Coat;
		};
		const Want Wants[] = {
			{ "PanzerIV", "GER", 1941, 6, "desert41" }, { "PanzerIV", "GER", 1942, 2, "desert41" }, { "PanzerIV", "GER", 1942, 3, "desert42" },
			{ "PanzerIV", "GER", 1943, 5, "desert42" }, { "PanzerIV", "GER", 1943, 6, "" }, { "StuG3", "GER", 1942, 11, "desert42" },
			{ "leFH18", "GER", 1941, 4, "desert41" }, { "Tiger", "GER", 1943, 2, "desert42" }, { "Tiger", "GER", 1943, 7, "" },
			{ "Matilda", "UK", 1941, 6, "stone" }, { "Sherman", "USA", 1943, 3, "" },
			// (the lorry stands in for every army's: her owner's desert coat, from LandKit's NationCoat, not her file)
			{ "Truck", "GER", 1941, 6, "desert41" }, { "Truck", "GER", 1942, 3, "desert42" }, { "Truck", "GER", 1943, 5, "desert42" },
			{ "Truck", "GER", 1943, 6, "" }, { "Truck", "UK", 1942, 10, "stone" }, { "Truck", "ITA", 1941, 1, "sand" }, { "Truck", "USA", 1943, 3, "" } };
		Tally Picked;
		for (const Want& W : Wants)
		{
			std::string Text, Desert, Plains;
			if (!ReadModel(W.Name, Text))
			{
				std::printf("  (%s.json is in neither the game's models nor LAND_KIT_MORE: not checked)\n", W.Name);
				continue;
			}
			const bool bOk = CoatOf(Text, W.Nation, W.Year, W.Month, "desert", Desert) && CoatOf(Text, W.Nation, W.Year, W.Month, "plains", Plains);
			// (an empty want: a month no desert row holds, or a model with none, dressed on the desert as by its by_month)
			const std::string Wanted = *W.Coat ? std::string(W.Coat) : Plains;
			std::printf("  %-9s %-3s %04d-%02d  desert: %-9s plains: %-9s %s\n", W.Name, W.Nation, W.Year, W.Month, Desert.c_str(), Plains.c_str(),
				bOk && Desert == Wanted ? "ok" : "WRONG");
			Picked.Note(bOk && Desert == Wanted, 0.0, std::string(W.Name) + " " + std::to_string(W.Year) + "-" + std::to_string(W.Month) + " on the desert: " + Desert + ", not " + Wanted);
		}
		Report("a desert field picks the desert coat of its month", Picked, "");

		// Every game model, every month of the war, its owners: no ground, plains and steppe dress alike (as before this
		// reader), and the desert differs only in the months its desert rows hold.
		Tally Same;
		const char* Models[10] = { "Sherman", "Tiger", "PanzerIV", "Howitzer105", "leFH18", "Truck", "StuG3", "T34", "LightTank", "ZiS3" };
		const char* Nations[4] = { "USA", "GER", "SOV", "UK" };
		long Desert = 0;
		for (const char* Name : Models)
		{
			std::string Text;
			if (!ReadModel(Name, Text))
			{
				continue;
			}
			Json Doc;
			std::string Why;
			Doc.Parse(Text, Why);
			const Json::Value Rows = Doc.Root()["swaps"]["paint"]["by_ground"]["desert"];
			const bool bOwners = !Doc.Root()["swaps"]["paint"]["choices"].IsObject()
				&& (Doc.Root()["nation"].Str("") == "any" || Doc.Root()["camouflage"].IsObject());
			for (const char* Nation : Nations)
			{
				for (int Key = 1939 * 12 + 8; Key <= 1945 * 12 + 7; ++Key)
				{
					const int Year = Key / 12, Month = Key % 12 + 1;
					char MonthKey[16];
					std::snprintf(MonthKey, sizeof(MonthKey), "%04d-%02d", Year, Month);
					std::string None, Plains, Steppe, Sand;
					const bool bOk = CoatOf(Text, Nation, Year, Month, "", None) && CoatOf(Text, Nation, Year, Month, "plains", Plains)
						&& CoatOf(Text, Nation, Year, Month, "steppe", Steppe) && CoatOf(Text, Nation, Year, Month, "desert", Sand);
					std::string Row;
					for (size_t I = 0; I < Rows.Count(); ++I)
					{
						const std::string From = Rows.At(I)["from"].Str(""), To = Rows.At(I)["to"].Str("");
						if ((From.empty() || From <= MonthKey) && (To.empty() || MonthKey <= To))
						{
							Row = Rows.At(I)["paint"].Str("");
							break;
						}
					}
					if (bOwners && Row.empty())
					{
						// (a model in her owner's coat, the lorry and the shared light tank: her desert coat is her owner's,
						// in LandKit's NationCoat, not rows of her file)
						const std::string N = Nation;
						Row = N == "GER" ? (Key <= 1942 * 12 + 1 ? "desert41" : Key <= 1943 * 12 + 4 ? "desert42" : "") : N == "UK" ? "stone" : "";
					}
					Desert += Row.empty() ? 0 : 1;
					Same.Note(bOk && Plains == None && Steppe == None && Sand == (Row.empty() ? None : Row), 0.0,
						std::string(Name) + " " + Nation + " " + MonthKey + ": none " + None + ", plains " + Plains + ", steppe " + Steppe + ", desert " + Sand);
				}
			}
		}
		std::printf("  (%ld model-nation-months held by a desert row)\n", Desert);
		Report("every other ground and month dressed as before", Same, "");

		// The whole way: this battle's recording with its field made desert and its date November 1942 (Tunisia), read by
		// Battle::Load, dresses the German models in the desert coat and leaves the others as on its own ground.
		Tally Whole;
		std::string Sand = Recording, Home = Recording;
		const size_t TerrainAt = Sand.find("\"terrain\":\"");
		const size_t DateAt = Sand.find("\"date\":\"");
		if (TerrainAt != std::string::npos && DateAt != std::string::npos)
		{
			const size_t TerrainEnd = Sand.find('"', TerrainAt + 11);
			Sand.replace(TerrainAt + 11, TerrainEnd - (TerrainAt + 11), "desert");
			const size_t DateIn = Sand.find("\"date\":\"") + 8;
			Sand.replace(DateIn, Sand.find('"', DateIn) - DateIn, "November 1942");
			const size_t HomeDate = Home.find("\"date\":\"") + 8;
			Home.replace(HomeDate, Home.find('"', HomeDate) - HomeDate, "November 1942");
			Battle OnSand, OnHome;
			std::string SandError, HomeError;
			const bool bSand = OnSand.Load(Sand, Read, Flat, nullptr, 79, SandError);
			const bool bHome = OnHome.Load(Home, Read, Flat, nullptr, 79, HomeError);
			Whole.Note(bSand && bHome && OnSand.Terrain == "desert" && OnHome.Terrain == B.Terrain, 0.0,
				"the battle on the desert would not load (" + SandError + HomeError + "), or its ground read " + OnSand.Terrain);
			std::printf("  this battle's own ground: %s\n", B.Terrain.c_str());
			for (const auto& [Key, M] : OnSand.Models())
			{
				const auto Other = OnHome.Models().find(Key);
				const std::string Was = Other != OnHome.Models().end() ? Other->second.Coat : std::string("?");
				const bool bGerman = M.Dressed.Nation == "GER";      // (the lorry too: her owner's desert coat)
				std::printf("  %-16s November 1942 on its own ground: %-9s on the desert: %s\n", Key.c_str(), Was.c_str(), M.Coat.c_str());
				Whole.Note(bGerman ? M.Coat == "desert42" : M.Coat == Was, 0.0, Key + " on the desert in November 1942: " + M.Coat + " (on its own ground " + Was + ")");
			}
		}
		else
		{
			Whole.Note(false, 0.0, "the recording has no field.place.terrain or date to change");
		}
		Report("a recording's desert field dresses its models (Battle::Load)", Whole, "");
	}

	std::printf("\n%s\n", Failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failures ? 1 : 0;
}
