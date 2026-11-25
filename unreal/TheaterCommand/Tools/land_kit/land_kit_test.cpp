// The ground battle's tanks, guns and lorries played through the game's own code (Battle/LandKit, no engine): every
// vehicle track of a recorded battle at 60 frames a second, with the checks of Brian's brief (21 November 2026), and a
// short report. A failed check prints FAIL and the run ends with a non-zero status.
// Usage: land_kit_test <Content folder> [recording.json]

#include "Battle/LandKit.h"
#include "Battle/Fates.h"
#include "Battle/LandCrater.h"
#include "Battle/LandTeamGun.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
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
	// ---- The burnt coat (release 2, Dress::bBurntCoat, the trial word "burntcoat").

	/**
	 * Every battle model, worn by her own nation (a "_" file by the Germans), loaded with the burnt coat off and on.
	 * Off must be as before: no point with a burnt colour of its own. On: every point of every mesh has one; her paint
	 * is as it was (the far level whole; the near level's own points, the cut's new ones after them); the near level is
	 * cut by no more than an eighth more triangles, none of them a sliver of no area, and the far level not at all; her
	 * far pieces carry the colours; and the skin of whatever has three places or more the fire comes out of is soot,
	 * rust and ash together. That is counted by area over what does not look down (her underside is all soot, and no
	 * one sees it): soot a tenth to six tenths of it, rust a fifth or more, ash a twentieth or more. (The art
	 * director's own line is on a picture's sunlit faces, soot no more than half and rust and ash a tenth each: the
	 * sheet is measured for that, LAND_KIT_BURNT=1; this counts her inner faces too.) Gives the failures.
	 */
	int CheckBurntCoat(const std::string& Content)
	{
		std::printf("THE BURNT COAT (release 2, -TheaterLandTrial=burntcoat: a wreck in soot, rust and ash, each point to its own)\n");
		std::vector<std::string> Files;
		for (const auto& Entry : std::filesystem::directory_iterator(Content + "/Models/Battle"))
		{
			if (Entry.path().extension() == ".json")
			{
				Files.push_back(Entry.path().stem().string());
			}
		}
		std::sort(Files.begin(), Files.end());
		int Failures = 0;
		size_t Models = 0, TanksSeen = 0;
		double MostMore = 0.0, MostSoot = 0.0, LeastSoot = 1.0, LeastRust = 1.0, LeastAsh = 1.0, MsOff = 0.0, MsOn = 0.0;
		std::string MostMoreOf;
		for (const std::string& Name : Files)
		{
			std::string Text, Error;
			Json Doc;
			if (!ReadFile(Content + "/Models/Battle/" + Name + ".json", Text) || !Doc.Parse(Text, Error))
			{
				std::printf("  FAIL cannot read %s.json\n", Name.c_str());
				++Failures;
				continue;
			}
			std::string Own = Doc.Root()["nation"].Str();
			if (Own == "any" || Own.empty())
			{
				Own = "GER";
			}
			Dress D;
			D.Nation = Own;
			Model Off, On;
			const auto T0 = Clock::now();
			const bool bOff = Off.Load(Text, D, Error);
			const auto T1 = Clock::now();
			D.bBurntCoat = true;
			const bool bOn = bOff && On.Load(Text, D, Error);
			const auto T2 = Clock::now();
			if (!bOn || Off.Bodies.size() != On.Bodies.size())
			{
				std::printf("  FAIL %s (%s): %s\n", Name.c_str(), Own.c_str(), Error.c_str());
				++Failures;
				continue;
			}
			MsOff += std::chrono::duration<double, std::milli>(T1 - T0).count();
			MsOn += std::chrono::duration<double, std::milli>(T2 - T1).count();
			++Models;
			std::string Why;
			size_t NearWas = 0, NearNow = 0;
			double Area[3] = { 0.0, 0.0, 0.0 }, Seen[3] = { 0.0, 0.0, 0.0 };
			if (Off.bBurntCoat || !On.bBurntCoat)
			{
				Why = "the model does not say whether its coat is on";
			}
			for (size_t B = 0; B < On.Bodies.size() && Why.empty(); ++B)
			{
				for (int Level = 0; Level < 2 && Why.empty(); ++Level)
				{
					const Mesh& Was = Level == 0 ? Off.Bodies[B].Near : Off.Bodies[B].Far;
					const Mesh& Now = Level == 0 ? On.Bodies[B].Near : On.Bodies[B].Far;
					const std::string Which = On.Bodies[B].Name + (Level == 0 ? " near" : " far");
					if (!Was.Burnt.empty())
					{
						Why = Which + ": a burnt colour of its own with the coat off";
					}
					else if (Now.Burnt.size() != Now.Count() * 3)
					{
						Why = Which + ": not every point has its burnt colour";
					}
					else if (Now.Count() < Was.Count() || (Level == 1 && (Now.Count() != Was.Count() || Now.Triangles != Was.Triangles)))
					{
						Why = Which + (Level == 1 ? ": the far level was cut" : ": points lost");
					}
					else if (!std::equal(Was.Points.begin(), Was.Points.end(), Now.Points.begin()) || !std::equal(Was.Colours.begin(), Was.Colours.end(), Now.Colours.begin())
						|| !std::equal(Was.Normals.begin(), Was.Normals.end(), Now.Normals.begin()) || Was.LinkFirst != Now.LinkFirst || Was.LinkCount != Now.LinkCount)
					{
						Why = Which + ": her own points or paint changed under the coat";
					}
					if (!Why.empty())
					{
						break;
					}
					if (Level == 0)
					{
						NearWas += Was.Triangles.size() / 3;
						NearNow += Now.Triangles.size() / 3;
					}
					for (size_t T = 0; T + 2 < Now.Triangles.size(); T += 3)
					{
						const uint32_t Ix[3] = { Now.Triangles[T], Now.Triangles[T + 1], Now.Triangles[T + 2] };
						if (Ix[0] >= Now.Count() || Ix[1] >= Now.Count() || Ix[2] >= Now.Count())
						{
							Why = Which + ": a triangle past the points";
							break;
						}
						if (Level != 0)
						{
							continue;
						}
						auto PointOf = [&Now](uint32_t I) { return V3(Now.Points[I * 3], Now.Points[I * 3 + 1], Now.Points[I * 3 + 2]); };
						const double Twice = (PointOf(Ix[1]) - PointOf(Ix[0])).Cross(PointOf(Ix[2]) - PointOf(Ix[0])).Length();
						// (a cut's own triangle, one with a new point: never of no area)
						if (std::max({ Ix[0], Ix[1], Ix[2] }) >= Was.Count() && Twice < 1e-10)
						{
							Why = Which + ": the cut left a triangle of no area";
							break;
						}
						double Rgb[3] = { 0.0, 0.0, 0.0 };
						for (uint32_t I : Ix)
						{
							for (size_t Ch = 0; Ch < 3; ++Ch)
							{
								Rgb[Ch] += Now.Burnt[I * 3 + Ch] / 3.0;
							}
						}
						// (soot: dark; ash: pale and grey; the rest rust)
						const int Kind = Rgb[0] < 62.0 ? 0 : (Rgb[0] - Rgb[2] < 30.0 && Rgb[0] >= 85.0 ? 2 : 1);
						Area[Kind] += 0.5 * Twice;
						const double UpZ = (Now.Normals[Ix[0] * 3 + 2] + Now.Normals[Ix[1] * 3 + 2] + Now.Normals[Ix[2] * 3 + 2]) / 3.0;
						if (UpZ > -0.15)
						{
							Seen[Kind] += 0.5 * Twice;
						}
					}
				}
			}
			FarSplit Split;
			SplitFar(On, Split);
			for (const FarPiece& Piece : Split.Pieces)
			{
				if (Why.empty() && Piece.Merged.Burnt.size() != Piece.Merged.Count() * 3)
				{
					Why = "a far piece without its points' burnt colours";
				}
			}
			const double More = NearWas > 0 ? static_cast<double>(NearNow) / static_cast<double>(NearWas) - 1.0 : 0.0;
			if (Why.empty() && More > 0.125)
			{
				Why = "the cut added more than an eighth to her near triangles";
			}
			if (More > MostMore)
			{
				MostMore = More;
				MostMoreOf = Name;
			}
			int FirePlaces = 0;
			for (const Socket& S : On.Sockets)
			{
				FirePlaces += S.Name == "engine" || S.Name == "ring" || S.Name.compare(0, 5, "fire.") == 0;
			}
			const double All = Area[0] + Area[1] + Area[2], SeenAll = Seen[0] + Seen[1] + Seen[2];
			// (only a model whose every mesh was read: a fault above leaves the count short)
			if (FirePlaces >= 3 && SeenAll > 0.0 && Why.empty())
			{
				++TanksSeen;
				const double Soot = Seen[0] / SeenAll, Rust = Seen[1] / SeenAll, Ash = Seen[2] / SeenAll;
				MostSoot = std::max(MostSoot, Soot);
				LeastSoot = std::min(LeastSoot, Soot);
				LeastRust = std::min(LeastRust, Rust);
				LeastAsh = std::min(LeastAsh, Ash);
				if (Soot > 0.6 || Soot < 0.1 || Rust < 0.2 || Ash < 0.05)
				{
					Why = "her skin is not soot, rust and ash together";
				}
				std::printf("  %-13s soot %2.0f%%  rust %2.0f%%  ash %2.0f%% of what does not look down (soot %2.0f%% of all her skin); near triangles %zu to %zu (+%.1f%%)\n",
					Name.c_str(), 100.0 * Soot, 100.0 * Rust, 100.0 * Ash, 100.0 * Area[0] / All, NearWas, NearNow, 100.0 * More);
			}
			if (!Why.empty())
			{
				std::printf("  FAIL %s (%s): %s\n", Name.c_str(), Own.c_str(), Why.c_str());
				++Failures;
			}
		}
		std::printf("  %zu models, %zu of them with three places or more to burn from: soot %.0f%% to %.0f%%, the least rust %.0f%%, the least ash %.0f%%; the most the cut adds %.1f%% (%s)\n",
			Models, TanksSeen, 100.0 * LeastSoot, 100.0 * MostSoot, 100.0 * LeastRust, 100.0 * LeastAsh, 100.0 * MostMore, MostMoreOf.c_str());
		std::printf("  loading them all: %.0f ms with the coat off, %.0f ms with it on\n", MsOff, MsOn);
		std::printf("  off as before; on, every point its own burnt colour, her paint untouched %s\n\n", Failures == 0 ? "ok" : "FAILED");
		return Failures;
	}

	// ---- National marks (release 2, Dress::bMarks, "-TheaterLandMarks").

	/** The nearest point of triangle A, B, C to P (Ericson's). */
	V3 NearestOnTriangle(const V3& P, const V3& A, const V3& B, const V3& C)
	{
		const V3 AB = B - A, AC = C - A, AP = P - A;
		const double D1 = AB.Dot(AP), D2 = AC.Dot(AP);
		if (D1 <= 0.0 && D2 <= 0.0) return A;
		const V3 BP = P - B;
		const double D3 = AB.Dot(BP), D4 = AC.Dot(BP);
		if (D3 >= 0.0 && D4 <= D3) return B;
		const double VC = D1 * D4 - D3 * D2;
		if (VC <= 0.0 && D1 >= 0.0 && D3 <= 0.0) return A + AB * (D1 / (D1 - D3));
		const V3 CP = P - C;
		const double D5 = AB.Dot(CP), D6 = AC.Dot(CP);
		if (D6 >= 0.0 && D5 <= D6) return C;
		const double VB = D5 * D2 - D1 * D6;
		if (VB <= 0.0 && D2 >= 0.0 && D6 <= 0.0) return A + AC * (D2 / (D2 - D6));
		const double VA = D3 * D6 - D5 * D4;
		if (VA <= 0.0 && (D4 - D3) >= 0.0 && (D5 - D6) >= 0.0) return B + (C - B) * ((D4 - D3) / ((D4 - D3) + (D5 - D6)));
		const double Denom = 1.0 / (VA + VB + VC);
		return A + AB * (VB * Denom) + AC * (VC * Denom);
	}

	/**
	 * Every battle model with mark places, worn by her own nation (a "_" file by the Americans and the Germans), and
	 * the owners whose marks change with the month (the British flash, then star; none before the flash) or who wear
	 * another's kit: loaded with the marks off and on. Off must be as before; on, each body with a place gains marks
	 * (none where the owner paints none) at both levels, every point within its lift of her own skin (the near level
	 * within 12 mm, the far within 35 mm), facing out, its triangles wound to face out, in the owner's paint (by area).
	 * Gives the failures.
	 */
	int CheckMarks(const std::string& Content)
	{
		std::printf("NATIONAL MARKS (release 2, -TheaterLandMarks: each owner's period marking painted onto her skin)\n");
		struct Case
		{
			std::string Model, Nation;
			int Year = 1944, Month = 7;
			std::string Expect; // white (the star), cross, red (the star), flash, rectangle, yellow (the army star), none
		};
		auto Expected = [](const std::string& Nation, int Year, int Month)
		{
			const int Key = Year * 12 + Month;
			if (Nation == "GER") return std::string("cross");
			if (Nation == "USA") return std::string("white");
			if (Nation == "SOV") return std::string("red");
			if (Nation == "UK") return std::string(Key >= 1943 * 12 + 7 ? "white" : Key >= 1941 * 12 + 11 ? "flash" : "none");
			if (Nation == "ITA") return std::string("rectangle");
			if (Nation == "JAP") return std::string("yellow");
			return std::string("none");
		};
		std::vector<Case> Cases;
		std::vector<std::string> Files;
		for (const auto& Entry : std::filesystem::directory_iterator(Content + "/Models/Battle"))
		{
			if (Entry.path().extension() == ".json")
			{
				Files.push_back(Entry.path().stem().string());
			}
		}
		std::sort(Files.begin(), Files.end());
		for (const std::string& Name : Files)
		{
			std::string Text, Error;
			Json Doc;
			if (!ReadFile(Content + "/Models/Battle/" + Name + ".json", Text) || !Doc.Parse(Text, Error) || Doc.Root()["marks"].Count() == 0)
			{
				continue;
			}
			std::string Own = Doc.Root()["nation"].Str();
			if (Own == "any" || Own.empty())
			{
				Cases.push_back({ Name, "USA", 1944, 7, "white" });
				Own = "GER";
			}
			Cases.push_back({ Name, Own, 1944, 7, Expected(Own, 1944, 7) });
		}
		for (const Case& C : std::vector<Case>{ { "LightTank", "UK", 1942, 5, "flash" }, { "LightTank", "UK", 1941, 6, "none" }, { "Sexton", "UK", 1943, 2, "flash" },
				 { "Sherman", "UK", 1944, 7, "white" }, { "Sherman", "SOV", 1944, 7, "red" }, { "LightTank", "JAP", 1942, 1, "yellow" },
				 { "LightTank", "ITA", 1942, 10, "rectangle" }, { "Truck", "SOV", 1943, 7, "red" } })
		{
			Cases.push_back(C);
		}
		int Failures = 0;
		size_t AllAdded = 0, AllCases = 0;
		for (const Case& C : Cases)
		{
			std::string Text, Error;
			if (!ReadFile(Content + "/Models/Battle/" + C.Model + ".json", Text))
			{
				std::printf("  FAIL cannot read %s.json\n", C.Model.c_str());
				++Failures;
				continue;
			}
			Dress D;
			D.Nation = C.Nation;
			D.Year = C.Year;
			D.Month = C.Month;
			Model Off, On;
			const auto T0 = Clock::now();
			const bool bOff = Off.Load(Text, D, Error);
			const auto T1 = Clock::now();
			D.bMarks = true;
			const bool bOn = bOff && On.Load(Text, D, Error);
			const auto T2 = Clock::now();
			if (!bOn || Off.Bodies.size() != On.Bodies.size())
			{
				std::printf("  FAIL %s (%s): %s\n", C.Model.c_str(), C.Nation.c_str(), Error.c_str());
				++Failures;
				continue;
			}
			++AllCases;
			std::string Why;
			size_t Added[2] = { 0, 0 }, AddedTris[2] = { 0, 0 }, OnSkin[2] = { 0, 0 }, Bad = 0, Wound = 0, Tris = 0;
			double Worst[2] = { 0.0, 0.0 };
			double White = 0.0, Black = 0.0, Red = 0.0, Yellow = 0.0, AreaAll = 0.0;
			for (size_t B = 0; B < On.Bodies.size(); ++B)
			{
				// (her places on this body, in its frame)
				std::vector<std::pair<V3, V3>> Places;
				for (const Mark& Mk : On.Painted)
				{
					if (On.Parts[static_cast<size_t>(Mk.Part)].Body == static_cast<int>(B))
					{
						Places.push_back({ Mk.At - On.Bodies[B].Pivot, Mk.Facing.Unit() });
					}
				}
				for (int Level = 0; Level < 2; ++Level)
				{
					const Mesh& Was = Level == 0 ? Off.Bodies[B].Near : Off.Bodies[B].Far;
					const Mesh& Now = Level == 0 ? On.Bodies[B].Near : On.Bodies[B].Far;
					const size_t N0 = Was.Count(), T0n = Was.Triangles.size();
					if (Now.Count() < N0 || Now.Triangles.size() < T0n || !std::equal(Was.Points.begin(), Was.Points.end(), Now.Points.begin())
						|| !std::equal(Was.Triangles.begin(), Was.Triangles.end(), Now.Triangles.begin())
						|| !std::equal(Was.Colours.begin(), Was.Colours.end(), Now.Colours.begin()))
					{
						Why = On.Bodies[B].Name + ": her own mesh changed under the marks";
						continue;
					}
					if (Now.Count() == N0)
					{
						if (!Places.empty() && C.Expect != "none" && !Was.Triangles.empty())
						{
							Why = On.Bodies[B].Name + (Level == 0 ? " near" : " far") + ": a mark place but no mark";
						}
						continue;
					}
					if (Places.empty() || C.Expect == "none")
					{
						Why = On.Bodies[B].Name + ": a mark where none belongs";
						continue;
					}
					Added[Level] += Now.Count() - N0;
					AddedTris[Level] += (Now.Triangles.size() - T0n) / 3;
					auto PointOf = [](const Mesh& M, size_t I) { return V3(M.Points[I * 3], M.Points[I * 3 + 1], M.Points[I * 3 + 2]); };
					auto FacingAt = [&Places](const V3& P)
					{
						double Best = 1e18;
						V3 Out(1.0, 0.0, 0.0);
						for (const auto& Pl : Places)
						{
							const V3 Gap = P - Pl.first;
							if (Gap.Dot(Gap) < Best)
							{
								Best = Gap.Dot(Gap);
								Out = Pl.second;
							}
						}
						return Out;
					};
					// (her own triangles about the marks: those whose box comes within 5 cm of the marks' points)
					V3 Lo(1e9, 1e9, 1e9), Hi(-1e9, -1e9, -1e9);
					for (size_t I = N0; I < Now.Count(); ++I)
					{
						const V3 P = PointOf(Now, I);
						Lo = V3(std::min(Lo.X, P.X), std::min(Lo.Y, P.Y), std::min(Lo.Z, P.Z));
						Hi = V3(std::max(Hi.X, P.X), std::max(Hi.Y, P.Y), std::max(Hi.Z, P.Z));
					}
					std::vector<size_t> Skin;
					for (size_t T = 0; T + 2 < T0n; T += 3)
					{
						const V3 A = PointOf(Was, Was.Triangles[T]), Bv = PointOf(Was, Was.Triangles[T + 1]), Cv = PointOf(Was, Was.Triangles[T + 2]);
						if (std::max({ A.X, Bv.X, Cv.X }) < Lo.X - 0.05 || std::min({ A.X, Bv.X, Cv.X }) > Hi.X + 0.05 || std::max({ A.Y, Bv.Y, Cv.Y }) < Lo.Y - 0.05
							|| std::min({ A.Y, Bv.Y, Cv.Y }) > Hi.Y + 0.05 || std::max({ A.Z, Bv.Z, Cv.Z }) < Lo.Z - 0.05 || std::min({ A.Z, Bv.Z, Cv.Z }) > Hi.Z + 0.05)
						{
							continue;
						}
						Skin.push_back(T);
					}
					const double Within = Level == 0 ? 0.012 : 0.035;
					for (size_t I = N0; I < Now.Count(); ++I)
					{
						const V3 P = PointOf(Now, I);
						double Near2 = 1e18;
						for (size_t T : Skin)
						{
							const V3 Q = NearestOnTriangle(P, PointOf(Was, Was.Triangles[T]), PointOf(Was, Was.Triangles[T + 1]), PointOf(Was, Was.Triangles[T + 2]));
							Near2 = std::min(Near2, (P - Q).Dot(P - Q));
						}
						const double Off2 = std::sqrt(Near2);
						Worst[Level] = std::max(Worst[Level], Off2);
						OnSkin[Level] += Off2 <= Within ? 1 : 0;
						const V3 Nm(Now.Normals[I * 3], Now.Normals[I * 3 + 1], Now.Normals[I * 3 + 2]);
						if (!(Nm.Dot(FacingAt(P)) > 0.2) || !std::isfinite(P.X + P.Y + P.Z))
						{
							++Bad;
						}
					}
					for (size_t T = T0n; T + 2 < Now.Triangles.size(); T += 3)
					{
						const V3 A = PointOf(Now, Now.Triangles[T]), Bv = PointOf(Now, Now.Triangles[T + 1]), Cv = PointOf(Now, Now.Triangles[T + 2]);
						const V3 Face = (Bv - A).Cross(Cv - A);
						if (Face.Length() < 1e-9)
						{
							continue;
						}
						++Tris;
						Wound += Face.Unit().Dot(FacingAt((A + Bv + Cv) * (1.0 / 3.0))) > 0.0 ? 1 : 0;
						if (Level == 0)
						{
							// (the paint by area: each triangle's mean colour)
							double Rgb[3] = { 0.0, 0.0, 0.0 };
							for (int K = 0; K < 3; ++K)
							{
								for (int Ch = 0; Ch < 3; ++Ch)
								{
									Rgb[Ch] += Now.Colours[Now.Triangles[T + static_cast<size_t>(K)] * 4 + static_cast<size_t>(Ch)] / 3.0;
								}
							}
							const double R = Rgb[0], G = Rgb[1], Bc = Rgb[2], Area = 0.5 * Face.Length();
							// (her own paint under it, the nearest of her skin's triangles: a mark under her dust reads
							// against her paint, a white lighter and a black darker than it, however dusty both are)
							const V3 Mid = (A + Bv + Cv) * (1.0 / 3.0);
							double Under = 1e18, SkinSum = 0.0;
							for (size_t Ts : Skin)
							{
								const V3 Q = NearestOnTriangle(Mid, PointOf(Was, Was.Triangles[Ts]), PointOf(Was, Was.Triangles[Ts + 1]), PointOf(Was, Was.Triangles[Ts + 2]));
								if ((Mid - Q).Dot(Mid - Q) < Under)
								{
									Under = (Mid - Q).Dot(Mid - Q);
									SkinSum = 0.0;
									for (int K = 0; K < 3; ++K)
									{
										for (int Ch = 0; Ch < 3; ++Ch)
										{
											SkinSum += Was.Colours[Was.Triangles[Ts + static_cast<size_t>(K)] * 4 + static_cast<size_t>(Ch)] / 3.0;
										}
									}
								}
							}
							AreaAll += Area;
							White += (std::min({ R, G, Bc }) >= 120 && (R + G + Bc) >= 420) || (std::min({ R, G, Bc }) >= 90 && R + G + Bc >= SkinSum + 60.0) ? Area : 0.0;
							Black += std::max({ R, G, Bc }) <= 80 || R + G + Bc <= SkinSum - 60.0 ? Area : 0.0;
							Red += R >= 90 && R >= 1.6 * G ? Area : 0.0;
							Yellow += R >= 140 && G >= 110 && Bc <= 0.75 * G ? Area : 0.0;
						}
					}
				}
			}
			const double Share = Added[0] ? static_cast<double>(OnSkin[0]) / Added[0] : 1.0, FarShare = Added[1] ? static_cast<double>(OnSkin[1]) / Added[1] : 1.0;
			const double Near = std::max(AreaAll, 1e-9);
			if (Why.empty() && C.Expect != "none" && (Added[0] == 0 || Added[1] == 0))
			{
				Why = "no marks added";
			}
			if (Why.empty() && (Share < 0.97 || FarShare < 0.9))
			{
				Why = "only " + std::to_string(Share) + " of the near points (" + std::to_string(FarShare) + " far) on her skin";
			}
			if (Why.empty() && Bad > 0)
			{
				Why = std::to_string(Bad) + " points not facing out";
			}
			if (Why.empty() && Tris > 0 && Wound < Tris * 99 / 100)
			{
				Why = std::to_string(Tris - Wound) + " of " + std::to_string(Tris) + " triangles wound inward";
			}
			// (where the army painted it, the art director's mends of 24 November: no cross on a Tiger's or a Panther's
			// turret, and the cross on both her hull's sides; the Japanese star on her front, not her sides)
			const int TurretIx = On.PartIndex("turret"), HullIx = On.PartIndex("hull");
			const bool bBig = C.Expect == "cross" && (On.Name.compare(0, 5, "Tiger") == 0 || On.Name.compare(0, 7, "Panther") == 0);
			int Sides[2] = { 0, 0 };
			for (const Mark& Mk : On.Painted)
			{
				if (bBig && Mk.Part == TurretIx && Why.empty())
				{
					Why = "a cross on her turret";
				}
				if (Mk.Part == HullIx && std::fabs(Mk.Facing.Y) > 0.5)
				{
					++Sides[Mk.Facing.Y < 0.0 ? 0 : 1];
				}
				if (C.Expect == "yellow" && Mk.Facing.X < 0.7 && Why.empty())
				{
					Why = "the Japanese star not on her front";
				}
			}
			if (bBig && (Sides[0] == 0 || Sides[1] == 0) && Why.empty())
			{
				Why = "no cross on one of her hull's sides";
			}
			const double Wh = White / Near, Bl = Black / Near, Rd = Red / Near, Ye = Yellow / Near;
			// (the cross: black over a white one a little wider, of the same length: about a fifth of it white along its
			// arms' sides; the marks under her own dust, chipped and scuffed)
			const bool bPaint = C.Expect == "none" || (C.Expect == "white" && Wh > 0.8) || (C.Expect == "cross" && Wh > 0.1 && Bl > 0.35)
				|| (C.Expect == "red" && Rd > 0.8) || (C.Expect == "flash" && Wh > 0.45 && Rd > 0.2) || (C.Expect == "rectangle" && Rd > 0.6 && Wh > 0.05)
				|| (C.Expect == "yellow" && Ye > 0.8);
			if (Why.empty() && !bPaint)
			{
				Why = "not her owner's paint";
			}
			AllAdded += AddedTris[0];
			std::printf("  %-11s %-3s %04d-%02d %-9s near %4zu triangles (%5.1f%% of points on her skin, worst %4.1f mm)  far %3zu (%5.1f%%, worst %4.1f mm)  white %3.0f%% black %3.0f%% red %3.0f%% yellow %3.0f%%  +%.0f ms  %s\n",
				C.Model.c_str(), C.Nation.c_str(), C.Year, C.Month, C.Expect.c_str(), AddedTris[0], Share * 100.0, Worst[0] * 1000.0, AddedTris[1], FarShare * 100.0,
				Worst[1] * 1000.0, Wh * 100.0, Bl * 100.0, Rd * 100.0, Ye * 100.0, Ms(T1, T2) - Ms(T0, T1), Why.empty() ? "ok" : ("FAIL " + Why).c_str());
			Failures += Why.empty() ? 0 : 1;
		}
		std::printf("  %zu looks checked, %zu mark triangles near in all: %s\n", AllCases, AllAdded, Failures ? "FAIL" : "ok");
		return Failures;
	}
	/**
	 * The support teams' weapons (Battle/LandTeamGun; release 2, on trial "teamguns"): every army's machine gun and
	 * mortar has a file that loads and reads as a support weapon; and each weapon, stepped as the scene steps it, stands
	 * with its seat on its gunner's own place and on the drawn ground, swings and lays on what he fires at no faster
	 * than its rates and no further than its limits, shakes back along its bore in bursts while he fires, rides on his
	 * shoulder by its middle when the team moves (the wheeled one rolled behind him by her trail), stays where it stood
	 * when he falls, and is not drawn while he is hidden or riding. Gives the failures.
	 */
	int CheckTeamGuns(const std::string& Content)
	{
		std::printf("THE SUPPORT TEAMS' WEAPONS (release 2, -TheaterLandTrial=teamguns: a machine gun or a mortar at each team's key man)\n");
		using LandTeamGun::Crew;
		using LandTeamGun::Gun;
		using LandTeamGun::Kind;
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Dt = 1.0 / 60.0;
		int Failures = 0;
		auto Fail = [&Failures](const std::string& Why)
		{
			std::printf("  FAIL %s\n", Why.c_str());
			++Failures;
		};
		auto Apply = [](const LandMen::Xform& F, const V3& P)
		{
			return V3(F.R[0] * P.X + F.R[1] * P.Y + F.R[2] * P.Z + F.T[0], F.R[3] * P.X + F.R[4] * P.Y + F.R[5] * P.Z + F.T[1],
				F.R[6] * P.X + F.R[7] * P.Y + F.R[8] * P.Z + F.T[2]);
		};
		auto Turn = [](const LandMen::Xform& F, const V3& P)
		{
			return V3(F.R[0] * P.X + F.R[1] * P.Y + F.R[2] * P.Z, F.R[3] * P.X + F.R[4] * P.Y + F.R[5] * P.Z, F.R[6] * P.X + F.R[7] * P.Y + F.R[8] * P.Z);
		};
		// How far a frame is from a soldier's kind of frame: its rows unit and square to each other, and its hand a
		// man's own (x forward, y right, z up on a field of x east, y north: determinant -1). Not a number: huge.
		auto FrameFault = [](const LandMen::Xform& F)
		{
			double Worst = 0.0;
			for (int A = 0; A < 3; ++A)
			{
				for (int B = 0; B < 3; ++B)
				{
					const double Dot = static_cast<double>(F.R[A * 3]) * F.R[B * 3] + static_cast<double>(F.R[A * 3 + 1]) * F.R[B * 3 + 1]
						+ static_cast<double>(F.R[A * 3 + 2]) * F.R[B * 3 + 2];
					Worst = std::max(Worst, std::fabs(Dot - (A == B ? 1.0 : 0.0)));
				}
			}
			const double Det = static_cast<double>(F.R[0]) * (static_cast<double>(F.R[4]) * F.R[8] - static_cast<double>(F.R[5]) * F.R[7])
				- static_cast<double>(F.R[1]) * (static_cast<double>(F.R[3]) * F.R[8] - static_cast<double>(F.R[5]) * F.R[6])
				+ static_cast<double>(F.R[2]) * (static_cast<double>(F.R[3]) * F.R[7] - static_cast<double>(F.R[4]) * F.R[6]);
			Worst = std::max(Worst, std::fabs(Det + 1.0));
			for (double T : { static_cast<double>(F.T[0]), static_cast<double>(F.T[1]), static_cast<double>(F.T[2]) })
			{
				if (!std::isfinite(T))
				{
					Worst = 1e9;
				}
			}
			return std::isfinite(Worst) ? Worst : 1e9;
		};
		// A man's own frame standing at a place and going a way (the crowd's WorldFrame for a man upright).
		auto ManFrame = [](double X, double Y, double Z, double F)
		{
			LandMen::Xform Out;
			const float S = static_cast<float>(std::sin(F)), C = static_cast<float>(std::cos(F));
			const float R[9] = { S, C, 0.f, C, -S, 0.f, 0.f, 0.f, 1.f };
			std::copy(R, R + 9, Out.R);
			Out.T[0] = static_cast<float>(X);
			Out.T[1] = static_cast<float>(Y);
			Out.T[2] = static_cast<float>(Z);
			return Out;
		};
		auto Bearing = [](double X, double Y) { return std::atan2(X, Y) * 180.0 / 3.14159265358979323846; };
		auto Flat = [](double, double) { return 0.0; };
		auto Slope = [](double X, double Y) { return 3.0 + 0.02 * X + 0.01 * Y; };

		// ---- Every army's file.
		const char* Armies[] = { "GER", "GER@desert", "GER_winter", "USA", "UK", "UK@desert", "SOV", "SOV_winter", "FIN", "JAP", "ITA", "ROM", "FRA",
			"CHN", "POL", "HUN", "NED", "" };
		std::vector<std::string> Files;
		size_t Asked = 0;
		for (const char* Army : Armies)
		{
			for (const char* Role : { "hmg", "mortar" })
			{
				for (int Stamp : { 193909, 194205, 194206, 194506 })
				{
					const std::string File = LandTeamGun::FileFor(Army, Role, Stamp / 100, Stamp % 100);
					++Asked;
					if (File.empty())
					{
						Fail(std::string("no weapon for ") + Army + " " + Role);
					}
					else if (std::find(Files.begin(), Files.end(), File) == Files.end())
					{
						Files.push_back(File);
					}
				}
			}
			for (const char* Role : { "rifle", "lmg", "loader", "nco", "at", "" })
			{
				if (!LandTeamGun::FileFor(Army, Role, 1944, 6).empty())
				{
					Fail(std::string("a weapon for the role '") + Role + "'");
				}
			}
		}
		if (LandTeamGun::FileFor("GER", "hmg", 1942, 5) != "HmgMG34" || LandTeamGun::FileFor("GER@desert", "hmg", 1942, 6) != "HmgMG42"
			|| LandTeamGun::FileFor("USA", "hmg", 1944, 6) != "HmgM1919" || LandTeamGun::FileFor("UK@desert", "hmg", 1941, 11) != "HmgVickers"
			|| LandTeamGun::FileFor("SOV_winter", "hmg", 1942, 1) != "HmgMaxim" || LandTeamGun::FileFor("JAP", "mortar", 1942, 1) != "MortarJAP")
		{
			Fail("an army's own weapon is not the one it served (the MG 34 until June 1942 and the MG 42 after, the Browning, the Vickers, the Maxim)");
		}
		std::sort(Files.begin(), Files.end());
		std::printf("  %zu asks of %zu armies, each answered; %zu weapons' files\n", Asked, sizeof(Armies) / sizeof(Armies[0]), Files.size());

		// ---- Each weapon.
		const std::map<std::string, int> FeedOf = { { "HmgMG42", -1 }, { "HmgMG34", -1 }, { "HmgM1919", -1 }, { "HmgM1917", -1 }, { "HmgType92", -1 },
			{ "HmgHotchkiss", -1 }, { "HmgBreda37", -1 }, { "HmgVickers", 1 }, { "HmgMaxim", 1 } };
		Tally Frames, Seats, Rates, Lays, Limits, Shakes, Carries, Rolls, Falls;
		size_t Guns = 0, Mortars = 0;
		for (const std::string& Name : Files)
		{
			std::string Text, Error, Why;
			Dress D;
			Model M;
			Kind K;
			if (!ReadFile(Content + "/Models/Battle/" + Name + ".json", Text))
			{
				Fail("cannot read " + Name + ".json");
				continue;
			}
			D.Nation = "GER";
			if (!M.Load(Text, D, Error))
			{
				Fail(Name + ": " + Error);
				continue;
			}
			if (!K.Read(M, Why))
			{
				Fail(Name + ": " + Why);
				continue;
			}
			(K.bMortar ? Mortars : Guns) += 1;
			// What the file tells the game.
			size_t MountTris = 0, TurnedTris = 0;
			double Lowest = 1e9;
			std::vector<int> All = K.Mount;
			All.insert(All.end(), K.Turned.begin(), K.Turned.end());
			std::sort(All.begin(), All.end());
			bool bEach = All.size() == M.Bodies.size();
			for (size_t B = 0; B < All.size() && bEach; ++B)
			{
				bEach = All[B] == static_cast<int>(B);
			}
			for (int B : K.Mount)
			{
				const Body& Of = M.Bodies[static_cast<size_t>(B)];
				MountTris += Of.Near.Triangles.size() / 3;
				for (size_t P = 2; P < Of.Near.Points.size(); P += 3)
				{
					Lowest = std::min(Lowest, Of.Near.Points[P] + Of.Pivot.Z);
				}
			}
			for (int B : K.Turned)
			{
				TurnedTris += M.Bodies[static_cast<size_t>(B)].Near.Triangles.size() / 3;
			}
			K.File = Name;
			if (K.Title.empty())
			{
				Why = "her model has no name of its own";
			}
			else if (!bEach)
			{
				Why = "a body neither stands nor turns, or does both";
			}
			// (a gun's feet on the ground to 2 cm; a mortar's base plate bedded in, its ribs up to 7 cm under)
			else if (Lowest > 0.02 || Lowest < (K.bMortar ? -0.07 : -0.02))
			{
				Why = "her mount does not stand on the ground (lowest point " + std::to_string(Lowest) + " m)";
			}
			else if (std::fabs(K.Seat.Z) > 1e-6 || K.Seat.X >= K.Muzzle.X)
			{
				Why = "her gunner's place is not on the ground behind her muzzle";
			}
			else if (std::fabs(K.Bore.Length() - 1.0) > 1e-9 || K.Bore.X <= 0.0)
			{
				Why = "her bore is not a unit way forward";
			}
			else if (K.bMortar ? (K.FeedSide != 0 || !K.bKneels || K.bWheeled || K.Bore.Z < 0.7)
				: (FeedOf.count(Name) == 0 || FeedOf.at(Name) != K.FeedSide || K.Left < 15.0 || K.Right < 15.0 || K.Low > -5.0 || K.High < 8.0 || K.bWheeled != (Name == "HmgMaxim")
					|| (K.bWheeled && !K.bKneels)))
			{
				Why = "what her file tells the game is not her (the feed's side, her swing and lay, a mortar's steep tube, the Maxim's wheels)";
			}
			else if (!K.bMortar)
			{
				// (the game swings what turns about the gun's own pivot: the cradle's pin must stand under it)
				const V3 Pin = M.Parts[static_cast<size_t>(M.PartIndex("cradle"))].Pivot;
				if (std::hypot(Pin.X - K.Pivot.X, Pin.Y - K.Pivot.Y) > 0.001)
				{
					Why = "her cradle's pin is not under her gun's pivot";
				}
			}
			if (!Why.empty())
			{
				Fail(Name + ": " + Why);
				continue;
			}
			std::printf("  %-13s %-6s %s%s%s  swings %2.0f left %2.0f right, lays %3.0f to %2.0f  seat %5.2f back %5.2f %s  mount %4zu triangles, what turns %4zu  %s\n",
				Name.c_str(), K.bMortar ? "mortar" : "gun", K.FeedSide < 0 ? "fed from the left " : K.FeedSide > 0 ? "fed from the right" : "dropped down her  ",
				K.bKneels ? ", gunner kneels" : ", gunner lies  ", K.bWheeled ? ", on wheels" : "          ", K.Left, K.Right, K.Low, K.High, -K.Seat.X,
				std::fabs(K.Seat.Y), K.Seat.Y < 0.0 ? "left " : "right", MountTris, TurnedTris, K.Title.c_str());

			// Set up at her gunner, on a slope, nothing to fire at.
			Crew C;
			C.X = 137.25;
			C.Y = -58.5;
			C.Facing = 0.7;
			C.Way = 0.7;
			C.Pose = "deployed";
			Gun G;
			G.Step(K, C, 10.0, Dt, Slope);
			{
				const V3 Seat = Apply(G.MountFrame(), K.Seat);
				LandMen::Vec3 At, Way;
				const bool bMuzzle = G.MuzzleAt(K, At, Way);
				const V3 Rest = Apply(G.MountFrame(), K.Muzzle);
				Frames.Note(G.Is == Gun::State::SetUp && G.MountShown() && G.TurnedShown(), 0.0, Name + ": not set up at a deployed gunner");
				Frames.Note(FrameFault(G.MountFrame()) < 1e-5 && FrameFault(G.TurnedFrame(K)) < 1e-5, std::max(FrameFault(G.MountFrame()), FrameFault(G.TurnedFrame(K))),
					Name + ": her frame set up is not a soldier's kind of frame");
				Seats.Note(std::hypot(Seat.X - C.X, Seat.Y - C.Y) < 0.001, std::hypot(Seat.X - C.X, Seat.Y - C.Y), Name + ": her seat is not on her gunner's place");
				Seats.Note(std::fabs(G.Z - Slope(G.X, G.Y)) < 1e-9, std::fabs(G.Z - Slope(G.X, G.Y)), Name + ": she does not stand on the drawn ground");
				Lays.Note(bMuzzle && std::hypot(At.X - Rest.X, At.Y - Rest.Y) < 0.001 && std::fabs(At.Z - Rest.Z) < 0.001
					&& std::fabs(WrapDeg(Bearing(Way.X, Way.Y) - C.Facing * 180.0 / Pi)) < 0.01 && std::fabs(Way.Z - K.Bore.Z) < 1e-5,
					bMuzzle ? std::fabs(WrapDeg(Bearing(Way.X, Way.Y) - C.Facing * 180.0 / Pi)) : 1e9, Name + ": at rest her muzzle is not her model's, facing as her gunner faces");
			}

			// Firing at a man 300 m off, 10 degrees to the right, on the level: 3 seconds.
			auto Fire = [&](Gun& Of, Crew& By, double Off, double Far, const std::function<double(double, double)>& Ground, double Seconds, const char* What)
			{
				By.Pose = "fire";
				By.bAim = true;
				// (from her own pin, as she stands with nothing to fire at; a mortar's gunner is the place she turns about)
				Gun At;
				Crew Idle = By;
				Idle.bAim = false;
				At.Step(K, Idle, 0.0, Dt, Ground);
				const double FromX = K.bMortar ? By.X : At.X, FromY = K.bMortar ? By.Y : At.Y;
				By.AimX = FromX + Far * std::sin(By.Facing + Off * Pi / 180.0);
				By.AimY = FromY + Far * std::cos(By.Facing + Off * Pi / 180.0);
				double Now = 20.0;
				for (int Frame = 0; Frame < static_cast<int>(Seconds * 60.0); ++Frame, Now += Dt)
				{
					const double WasT = Of.Traverse, WasE = Of.Elevation, WasF = Of.Facing;
					const bool bWasUp = Of.Is == Gun::State::SetUp;
					Of.Step(K, By, Now, Dt, Ground);
					if (bWasUp)
					{
						const double Swung = std::fabs(Of.Traverse - WasT) / Dt, Laid = std::fabs(Of.Elevation - WasE) / Dt;
						const double Faced = std::fabs(WrapDeg((Of.Facing - WasF) * 180.0 / Pi)) / Dt;
						Rates.Note(Swung <= 45.0 + 1e-6 && Laid <= 20.0 + 1e-6 && Faced <= 40.0 + 1e-6, std::max(Swung, std::max(Laid, Faced)),
							Name + ": " + What + ": she swings, lays or is turned faster than her rate");
					}
					Limits.Note(Of.Traverse >= -K.Left - 1e-9 && Of.Traverse <= K.Right + 1e-9 && (K.bMortar || (Of.Elevation >= K.Low - 1e-9 && Of.Elevation <= K.High + 1e-9)),
						std::fabs(Of.Traverse), Name + ": " + What + ": past her limits");
					Frames.Note(Of.Is == Gun::State::SetUp && Of.bFiring && FrameFault(Of.MountFrame()) < 1e-5 && FrameFault(Of.TurnedFrame(K)) < 1e-5,
						FrameFault(Of.TurnedFrame(K)), Name + ": " + What + ": her frame while she fires");
					const V3 Seat = Apply(Of.MountFrame(), K.Seat);
					Seats.Note(std::hypot(Seat.X - By.X, Seat.Y - By.Y) < 0.001, std::hypot(Seat.X - By.X, Seat.Y - By.Y), Name + ": " + What + ": her seat left her gunner");
				}
			};
			{
				Crew By = C;
				Gun Of;
				Fire(Of, By, 10.0, 300.0, Flat, 3.0, "10 degrees right");
				LandMen::Vec3 At, Way;
				Gun Still = Of;
				Still.Recoil = 0.0;
				const bool bMuzzle = Still.MuzzleAt(K, At, Way);
				const double To = Bearing(By.AimX - (K.bMortar ? By.X : At.X), By.AimY - (K.bMortar ? By.Y : At.Y));
				const double Off = bMuzzle ? std::fabs(WrapDeg(Bearing(Way.X, Way.Y) - To)) : 1e9;
				// (a gun's bore is laid on the man's chest and a little over; a mortar's is turned at him, her tube steep)
				Lays.Note(Off < (K.bMortar ? 0.2 : 0.35), Off, Name + ": after 3 s her bore is not on what she fires at");
				if (!K.bMortar && bMuzzle)
				{
					const double Far = std::hypot(By.AimX - At.X, By.AimY - At.Y);
					const double Over = At.Z + Far * Way.Z / std::max(1e-6, std::hypot(static_cast<double>(Way.X), static_cast<double>(Way.Y)));
					Lays.Note(Over >= 0.9 && Over <= 2.5 && std::fabs(Of.Traverse - 10.0) < 0.05, Over, Name + ": her bore does not pass a little over a man's chest out there");
				}
			}
			// Far to each side: a gun stops at her limit, a mortar is turned whole.
			for (double Side : { -80.0, 80.0 })
			{
				Crew By = C;
				Gun Of;
				// (already set up when the order comes, so she is swung or turned to it at her rate)
				Of.Step(K, By, 0.0, Dt, Flat);
				Fire(Of, By, Side, 300.0, Flat, 3.0, "80 degrees off");
				if (K.bMortar)
				{
					const double Off = std::fabs(WrapDeg((Of.Facing - By.Facing) * 180.0 / Pi - Side));
					Limits.Note(Off < 0.2 && Of.Traverse == 0.0, Off, Name + ": a mortar is not turned whole at what she fires at");
				}
				else
				{
					const double Stop = Side < 0.0 ? -K.Left : K.Right;
					LandMen::Vec3 At, Way;
					Of.MuzzleAt(K, At, Way);
					const double Off = std::fabs(WrapDeg(Bearing(Way.X, Way.Y) - By.Facing * 180.0 / Pi - Stop));
					Limits.Note(std::fabs(Of.Traverse - Stop) < 1e-9 && Off < 0.01 && std::fabs(WrapDeg((Of.Facing - By.Facing) * 180.0 / Pi)) < 1e-9, Off,
						Name + ": a gun does not stop at her swing's limit, her mount as her gunner faces");
				}
			}
			// Up a hill and down into a hollow: laid no further than she can be.
			if (!K.bMortar)
			{
				for (double Rise : { 30.0, 150.0, -150.0 })
				{
					Crew By = C;
					Gun Of;
					double AimX = 0.0, AimY = 0.0;
					auto Hill = [&AimX, &AimY, Rise](double X, double Y) { return std::hypot(X - AimX, Y - AimY) < 5.0 ? Rise : 0.0; };
					// (where she will aim: her pin with nothing to fire at, 200 m out dead ahead)
					Gun At;
					At.Step(K, By, 0.0, Dt, Flat);
					AimX = At.X + 200.0 * std::sin(By.Facing);
					AimY = At.Y + 200.0 * std::cos(By.Facing);
					Fire(Of, By, 0.0, 200.0, Hill, 3.0, "a hill");
					LandMen::Vec3 Mz, Way;
					Of.MuzzleAt(K, Mz, Way);
					const double Up = std::asin(std::clamp(static_cast<double>(Way.Z), -1.0, 1.0)) * 180.0 / Pi;
					const double Want = Rise > 100.0 ? K.High : Rise < -100.0 ? K.Low : Up;
					Lays.Note(std::fabs(Up - Of.Elevation) < 0.01 && std::fabs(Up - Want) < 0.01 && (Rise < 0.0 || Up > 5.0), std::fabs(Up - Want),
						Name + ": her bore's rise is not her lay, or past what she can be laid to");
				}
			}
			// Bursts: 1.3 s of fire in every 2.8 s, the gun shaking back along her bore; two of a team not in step.
			if (!K.bMortar)
			{
				Crew By = C;
				Gun Of, Other;
				Other.Phase = 0.5;
				Fire(Of, By, 0.0, 300.0, Flat, 1.0, "a burst");
				Fire(Other, By, 0.0, 300.0, Flat, 1.0, "a burst");
				long Shaking = 0, Apart = 0;
				const long Count = 28 * 60;
				double Now = 50.0;
				for (long Frame = 0; Frame < Count; ++Frame, Now += Dt)
				{
					Of.Step(K, By, Now, Dt, Flat);
					Other.Step(K, By, Now, Dt, Flat);
					Shaking += Of.Recoil > 0.0 ? 1 : 0;
					Apart += (Of.Recoil > 0.0) != (Other.Recoil > 0.0) ? 1 : 0;
					Gun Still = Of;
					Still.Recoil = 0.0;
					LandMen::Vec3 A, B, WayA, WayB;
					Of.MuzzleAt(K, A, WayA);
					Still.MuzzleAt(K, B, WayB);
					const double Back = (static_cast<double>(B.X) - A.X) * WayB.X + (static_cast<double>(B.Y) - A.Y) * WayB.Y + (static_cast<double>(B.Z) - A.Z) * WayB.Z;
					Shakes.Note(Of.Recoil >= 0.0 && Of.Recoil <= 0.006 + 1e-12 && std::fabs(Back - Of.Recoil) < 2e-4, Of.Recoil, Name + ": her shake is not back along her bore, 6 mm at most");
				}
				const double Share = static_cast<double>(Shaking) / static_cast<double>(Count);
				Shakes.Note(Share > 0.40 && Share < 0.50 && Apart > Count / 2, Share, Name + ": she does not fire in bursts (1.3 s in 2.8), or two of a team shake in step");
				// (and not at all once he stops firing)
				By.Pose = "deployed";
				Of.Step(K, By, Now, Dt, Flat);
				Shakes.Note(!Of.bFiring && Of.Recoil == 0.0, Of.Recoil, Name + ": she shakes with no one firing");
			}
			// Told of her bursts by the scene (the rounds as they are fired): she shakes through each and no longer, for
			// her own burst's length (nine tenths of a second at most, the rules firing her again within the second; a
			// mortar has none).
			{
				int Rounds = 0;
				double PerSecond = 0.0;
				const bool bBurst = LandTeamGun::BurstOf(Name, Rounds, PerSecond);
				Shakes.Note(bBurst != K.bMortar && (K.bMortar || (Rounds >= 6 && Rounds <= 40 && PerSecond >= 6.0 && PerSecond <= 30.0 && Rounds / PerSecond >= 0.75 && Rounds / PerSecond <= 0.9 + 1e-9)),
					bBurst ? Rounds / PerSecond : 0.0, Name + ": her burst is not a tripod gun's (three quarters to nine tenths of a second), or a mortar has one");
				Crew By = C;
				Gun Of;
				Of.bByRounds = true;
				Fire(Of, By, 0.0, 300.0, Flat, 0.0, "");
				const double Seconds = bBurst ? Rounds / PerSecond : 0.5, Rate = bBurst ? PerSecond : 1.0;
				bool bQuiet = true, bShook = true, bStopped = true;
				for (int Frame = 0; Frame < 60; ++Frame)
				{
					Of.Step(K, By, 100.0 + Frame * Dt, Dt, Flat);
					bQuiet = bQuiet && !Of.bFiring && Of.Recoil == 0.0;
				}
				Of.Fired(101.0, Seconds, Rate);
				Of.Fired(90.0, 30.0, Rate); // (an earlier one told late: nothing)
				Of.Step(K, By, 100.99, Dt, Flat);
				bQuiet = bQuiet && !Of.bFiring && Of.Recoil == 0.0;
				for (double Into = 0.001; Into < Seconds; Into += Dt)
				{
					Of.Step(K, By, 101.0 + Into, Dt, Flat);
					bShook = bShook && Of.bFiring && (K.bMortar ? Of.Recoil == 0.0 : Of.Recoil > 0.0 && Of.Recoil <= 0.006);
				}
				Of.Step(K, By, 101.0 + Seconds + 0.001, Dt, Flat);
				bStopped = !Of.bFiring && Of.Recoil == 0.0;
				Shakes.Note(bQuiet && bShook && bStopped, Seconds, Name + ": told of her bursts, she does not shake through each and no longer");
			}

			// The team moves: on his right shoulder by her middle, her muzzle up a little; a wheeled one rolled behind.
			{
				Crew By = C;
				Gun Of;
				Of.Step(K, By, 0.0, Dt, Slope);
				By.Pose = "run";
				By.Way = 2.1;
				By.Facing = 2.1;
				const LandMen::Xform Man = ManFrame(By.X, By.Y, Slope(By.X, By.Y), By.Way);
				By.Frame = &Man;
				Of.Step(K, By, 1.0, Dt, Slope);
				LandMen::Vec3 At, Way;
				if (K.bWheeled)
				{
					const V3 Handle = Apply(Of.MountFrame(), K.Pull), Ahead = Turn(Of.MountFrame(), V3(1.0, 0.0, 0.0));
					const double Off = std::hypot(Handle.X - (By.X - 0.3 * std::sin(By.Way)), Handle.Y - (By.Y - 0.3 * std::cos(By.Way)));
					Rolls.Note(Of.Is == Gun::State::Rolled && Of.MountShown() && Of.TurnedShown() && !Of.MuzzleAt(K, At, Way) && Off < 0.001
						&& std::fabs(WrapDeg(Bearing(Ahead.X, Ahead.Y) - By.Way * 180.0 / Pi - 180.0)) < 0.01 && std::fabs(Of.Z - Slope(Of.X, Of.Y)) < 1e-9
						&& Of.Traverse == 0.0 && Of.Elevation == 0.0 && FrameFault(Of.MountFrame()) < 1e-5 && FrameFault(Of.TurnedFrame(K)) < 1e-5,
						Off, Name + ": a wheeled gun is not rolled behind her gunner, her trail's handle a pace back and her muzzle the way he came");
				}
				else
				{
					const V3 Middle = Apply(Of.TurnedFrame(K), K.Balance), Shoulder = Apply(Man, V3(0.02, 0.19, 1.47)), Ahead = Turn(Of.TurnedFrame(K), K.Bore);
					const double Off = (Middle - Shoulder).Length();
					const double Up = std::asin(std::clamp(Ahead.Z, -1.0, 1.0)) * 180.0 / Pi;
					Carries.Note(Of.Is == Gun::State::Carried && !Of.MountShown() && Of.TurnedShown() && !Of.MuzzleAt(K, At, Way) && Off < 0.001
						&& std::fabs(Up - 12.0) < 0.01 && std::fabs(WrapDeg(Bearing(Ahead.X, Ahead.Y) - By.Way * 180.0 / Pi)) < 0.01 && FrameFault(Of.TurnedFrame(K)) < 1e-5,
						Off, Name + ": carried, her middle is not on his shoulder, her muzzle ahead and 12 degrees up");
					// (no frame of his this frame: nothing to ride on, so not drawn)
					Gun Lost;
					Crew None = By;
					None.Frame = nullptr;
					Lost.Step(K, None, 1.0, Dt, Slope);
					Carries.Note(Lost.Is == Gun::State::Away && !Lost.TurnedShown(), 0.0, Name + ": drawn carried by a man with no frame");
				}
				// He falls on the move: what he carried goes with him; a wheeled one stands where she stopped.
				const Gun Before = Of;
				By.bDown = true;
				Of.Step(K, By, 2.0, Dt, Slope);
				Falls.Note(K.bWheeled ? (Of.Is == Gun::State::Left && Of.X == Before.X && Of.Y == Before.Y && Of.Facing == Before.Facing) : Of.Is == Gun::State::Away, 0.0,
					Name + ": her gunner fallen on the move");
			}
			// He falls at her: she stays as she stood, whatever his track says after.
			{
				Crew By = C;
				Gun Of;
				Fire(Of, By, 10.0, 300.0, Slope, 2.0, "before he falls");
				const Gun Before = Of;
				By.bDown = true;
				bool bStill = true;
				for (const char* Pose : { "fire", "run", "cower", "deployed" })
				{
					By.Pose = Pose;
					Of.Step(K, By, 30.0, Dt, Slope);
					bStill = bStill && Of.Is == Gun::State::Left && Of.MountShown() && Of.TurnedShown() && Of.X == Before.X && Of.Y == Before.Y && Of.Z == Before.Z
						&& Of.Facing == Before.Facing && Of.Traverse == Before.Traverse && Of.Elevation == Before.Elevation && Of.Recoil == 0.0 && !Of.bFiring;
				}
				Falls.Note(bStill && FrameFault(Of.TurnedFrame(K)) < 1e-5, 0.0, Name + ": she does not stay as she stood when her gunner falls");
			}
			// Not drawn: hidden in ambush or not yet come, riding, mounted; a routed crew that never set her up. A routed
			// crew at a weapon set up stays by it, and so does one that waits.
			{
				bool bAway = true, bStays = true;
				for (int Case = 0; Case < 4; ++Case)
				{
					Crew By = C;
					Gun Of;
					Of.Step(K, By, 0.0, Dt, Flat);
					By.bShown = Case != 0;
					By.Pose = Case == 1 ? "mounted" : Case == 2 ? "ride" : "deployed";
					if (Case == 3)
					{
						Of = Gun();
						By.Pose = "cower";
					}
					Of.Step(K, By, 1.0, Dt, Flat);
					bAway = bAway && Of.Is == Gun::State::Away && !Of.MountShown() && !Of.TurnedShown();
				}
				for (const char* Pose : { "cower", "wait", "deployed", "fire" })
				{
					Crew By = C;
					Gun Of;
					Of.Step(K, By, 0.0, Dt, Flat);
					By.Pose = Pose;
					Of.Step(K, By, 1.0, Dt, Flat);
					bStays = bStays && Of.Is == Gun::State::SetUp && !Of.bFiring;
				}
				Frames.Note(bAway, 0.0, Name + ": drawn while her gunner is hidden, riding, or routed before she was set up");
				Frames.Note(bStays, 0.0, Name + ": not kept set up through a wait or a rout, or firing with nothing to fire at");
			}
		}
		auto Line = [&Failures](const char* What, const Tally& T, const char* Unit)
		{
			std::printf("  %-52s %s  (%ld checked, worst %.4g %s)\n", What, T.Failed ? "FAIL" : "ok", T.Checked, T.Worst, Unit);
			if (T.Failed)
			{
				std::printf("    %ld failed, the first: %s\n", T.Failed, T.First.c_str());
				++Failures;
			}
		};
		Line("a soldier's kind of frame, drawn only when served", Frames, "off square");
		Line("her seat on her gunner's place, on the ground", Seats, "m");
		Line("swung, laid and turned no faster than her rates", Rates, "deg/s");
		Line("her bore on what she fires at", Lays, "");
		Line("stopped at her limits; a mortar turned whole", Limits, "deg");
		Line("bursts, shaking back along her bore", Shakes, "");
		Line("carried by her middle on his right shoulder", Carries, "m");
		Line("the wheeled one rolled behind by her trail", Rolls, "m");
		Line("left as she stood when her gunner falls", Falls, "");
		if (Guns < 9 || Mortars < 7)
		{
			Fail("fewer weapons than the armies serve: " + std::to_string(Guns) + " guns, " + std::to_string(Mortars) + " mortars");
		}
		std::printf("  %zu machine guns and %zu mortars: %s\n", Guns, Mortars, Failures ? "FAIL" : "ok");
		return Failures;
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
	// (LAND_KIT_ONLY_BURNT: the burnt coat's section alone, a short run while its paint is being mended)
	if (std::getenv("LAND_KIT_ONLY_BURNT"))
	{
		return CheckBurntCoat(Content) ? 1 : 0;
	}
	// (LAND_KIT_ONLY_TEAMGUNS: the support teams' weapons alone)
	if (std::getenv("LAND_KIT_ONLY_TEAMGUNS"))
	{
		return CheckTeamGuns(Content) ? 1 : 0;
	}
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

	Failures += CheckMarks(Content);
	Failures += CheckBurntCoat(Content);
	Failures += CheckTeamGuns(Content);
	if (std::getenv("LAND_KIT_ONLY_MARKS"))
	{
		return Failures ? 1 : 0;
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
	// (LAND_SMOOTH_TANKS=seconds: release 2's tanks rolling smoothly, -TheaterLandSmoothTanks)
	if (const char* Smooth = std::getenv("LAND_SMOOTH_TANKS"))
	{
		B.SmoothTanks = std::atof(Smooth);
		std::printf("  tanks rolling smoothly: %.2f s\n", B.SmoothTanks);
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
