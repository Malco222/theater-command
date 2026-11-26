// The ships' paint (the ship detail plan, round 1: R1.3's scene half, R1.4's rust), held to every ship file the game has.
// Battle/ShipPaintCore is what the ship branch of FMiniatureMesh::Sculpted (Campaign/CampaignMiniatures.cpp) now calls; the
// branch itself, the reference's (verbatim at 55419eb6) and the tree's own, is cut out of the source by extract_paint.py
// and built here against the engine's stand-ins (paint_shim.h), so what is tested is the game's own code.
//   (a) off: on every ship file in Content/Models/Ships (26: the battleships, carriers, cruisers, destroyers, the three
//       merchants and the three U-boats), near and far, every point's colour is bit for bit the reference's: the linear
//       floats ToFColor is handed, and the bytes and the finish it gives; each mesh twice, as the file is and with a
//       made-up shade (today's files carry none, and the reference never reads one), so a shade that leaks with the
//       switches off fails here on every mesh;
//   (b) the map's and the air stage's callers never pass the options: who sets them, and where, read from the source;
//       the month's sea scene never takes the ship switches; the reader marks a wooden deck by its paint's name;
//   (c) on: the shade only darkens; the rust ramp keeps to the art director's numbers step by step, never lighter than
//       the paint, never red over 135 nor 80 over blue, the same every time; on Submarine_GER no two neighbouring weeps
//       alike and none one flat colour; edge wear darker, never whiter (light wear, no longer the ramp's film, among it);
//       the salt as ruled; a wooden deck under her 150,135,110 as drawn; and the sea's steel ink, worked through
//       M_MapSurface's own sums as Tools/build_content.py
//       makes them: a steel deck facing the sky lifted by the light asked (0.08, 0.10, 0.12, 0.26), with the skin or
//       without it, and by nothing with the light off; with the light alone no mottle to see; no chips anywhere, the
//       metal and the roughness as made; with both switches off, nothing set on the ink;
//   (d) a flipped default is caught: the identity check run with an option on fails, both legs of it;
//   (e) the art director's correction of 25 November 20:28Z, "light wear is not rust", on every mesh: her own up axis is
//       the file's +Z; the reader marks the foot of a run with body above it as this test's own finder does (worked apart
//       from the core); (1) no channel of a stained point above the paint's own, but where her salt (pinned here at
//       196,200,198, 30%) pales it in every channel at once, and never red above the paint with blue below it; (2) wear
//       under 20 with no run, in the mesh's own bytes against the same point unworn (salt and soot laid), 3 to 6% darker,
//       0 to 15% greyer, red less blue up by 3 at most; (3) no foot colour without body above it, every film point with a
//       body point at 45 degrees or steeper above it, and the film still at runs' feet; and her three spots printed as
//       painted colours on the points (the U-boat's band between her rails, the battleship's side, a destroyer's blotch),
//       today and now.
// Usage: ship_paint_test <Source/TheaterCommand> <Content/Models/Ships>   (built by Tools/ship_detail/run.sh)

#include "paint_shim.h"

#include "Battle/LandJson.h"
#include "Battle/SeaLookSwitches.h"
#include "Battle/ShipPaintCore.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Checks = 0, Failures = 0;

	void Check(bool bOk, const std::string& What, const std::string& Got)
	{
		++Checks;
		Failures += bOk ? 0 : 1;
		std::printf("  %s  %s: %s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.c_str());
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		std::stringstream Out;
		Out << In.rdbuf();
		return Out.str();
	}

	bool Has(const std::string& In, const std::string& What)
	{
		return In.find(What) != std::string::npos;
	}

	std::string Num(double V, int Places = 1)
	{
		char Buf[64];
		std::snprintf(Buf, sizeof(Buf), "%.*f", Places, V);
		return Buf;
	}

	struct Painted
	{
		std::vector<FColor> Colours;
		std::vector<float> Linear;
	};

	// ---- The branch, the reference's and the tree's own, built inside a loop over a file's points

	Painted PaintOld(const FRawShip& Model)
	{
		const FRawShip* Raw = &Model;
		FFreshShip Fresh;
		for (int32 K = 0; K < Raw->Points.Num(); ++K)
		{
#include "old_ship_branch.inc"
		}
		return { Fresh.Colours, Fresh.Linear };
	}

	Painted PaintNew(const FRawShip& Model, const ShipPaintCore::Options& ShipPaint)
	{
		const FRawShip* Raw = &Model;
		FFreshShip Fresh;
#include "new_ship_palette.inc"
#include "new_ship_runs.inc"
		for (int32 K = 0; K < Raw->Points.Num(); ++K)
		{
#include "new_ship_branch.inc"
		}
		return { Fresh.Colours, Fresh.Linear };
	}

	// ---- A ship file read as the reader reads it

	struct ShipFile
	{
		std::string Name;
		FRawShip Mesh[2]; // near, far
		std::vector<std::string> PaintNames;
		double Draught = 0.0; // (her file's own, metres below z 0)
		bool bLoads = false; // (a file with loads has triangles the reader may leave out; no ship's has)
	};

	bool Load(const std::string& Path, ShipFile& Out, std::string& Why)
	{
		std::string Text;
		LandJson::Json File;
		if (!LandJson::ReadFile(Path, Text) || !LandJson::Parse(Text, File, Why))
		{
			Why = Why.empty() ? "unreadable" : Why;
			return false;
		}
		if (File["kind"].Str("") != "ship")
		{
			Why = "not a ship";
			return false;
		}
		// (the reader's own sets, Campaign/CampaignMiniatures.cpp's ship branch of the file's reading)
		static const std::set<std::string> Metal = { "hull", "upper", "deck", "flight", "boot", "antifoul", "steel", "black", "boat", "roof", "torn", "brass", "line",
			"red", "white", "door", "seam", "upper.on", "hull.on", "helmet", "plane" };
		static const std::set<std::string> Coat = { "hull", "upper", "deck", "roof", "door", "seam", "upper.on", "hull.on" };
		for (size_t K = 0; K < File["paint_names"].Count(); ++K)
		{
			Out.PaintNames.push_back(File["paint_names"].At(K).Str(""));
		}
		Out.Draught = File["draught"].Num(-1.0);
		Out.bLoads = File.Has("loads");
		for (int Far = 0; Far < 2; ++Far)
		{
			const LandJson::Json& Part = File[Far ? "far" : "near"];
			FRawShip& Raw = Out.Mesh[Far];
			const std::vector<double> Points = LandJson::Numbers(Part["points"]), Paints = LandJson::Numbers(Part["paints"]);
			const std::vector<double> Facings = LandJson::Numbers(Part["facings"]), Triangles = LandJson::Numbers(Part["triangles"]);
			if (Points.size() != Paints.size() * 3 || Paints.empty() || Facings.size() != Points.size() || Triangles.size() % 3 != 0)
			{
				Why = std::string(Far ? "far" : "near") + ": points, facings, paints and triangles do not agree";
				return false;
			}
			for (size_t K = 0; K + 2 < Points.size(); K += 3)
			{
				// (the file has x forward and y to the right; the game's vehicles are built nose toward -X with their right hand at
				// -Y: turned about z, which stays her own up)
				Raw.Points.Add(FVector{ -Points[K], -Points[K + 1], Points[K + 2] });
				Raw.Facings.Add(FVector{ -Facings[K], -Facings[K + 1], Facings[K + 2] });
			}
			for (const double T : Triangles)
			{
				// (ships have no loads, so the reader keeps every triangle the file gives)
				Raw.Triangles.Add(static_cast<int32>(T));
			}
			for (const double P : Paints)
			{
				Raw.Paints.Add(static_cast<int32>(P));
			}
			if (Part.Has("shade") && Part["shade"].Count() == Paints.size())
			{
				for (const double V : LandJson::Numbers(Part["shade"]))
				{
					Raw.Worn.Add(std::min(1.0f, std::max(0.0f, static_cast<float>(V))));
				}
			}
			for (size_t K = 0; K < File["colours"].Count(); ++K)
			{
				Raw.PaintColours.Add(FLinearColor(FColor::FromHex(File["colours"].At(K).Str("000000").c_str())));
			}
			if (File["rust"].IsString())
			{
				Raw.Rust = FLinearColor(FColor::FromHex(File["rust"].Str().c_str()));
			}
			for (const char* Field : { "wear", "soot", "salt" })
			{
				TArray<int32>& Into = std::string(Field) == "wear" ? Raw.Wear : std::string(Field) == "soot" ? Raw.Soot : Raw.Salt;
				for (const double V : LandJson::Numbers(Part[Field]))
				{
					Into.Add(static_cast<int32>(V));
				}
			}
			for (const std::string& Paint : Out.PaintNames)
			{
				Raw.ShipMetal.Add(Metal.count(Paint) == 1);
				Raw.ShipPainted.Add(Coat.count(Paint) == 1);
				Raw.ShipPlanks.Add(Paint == "planks");
			}
		}
		return true;
	}

	/** A point as the tree's branch hands it to the core (under the skin, with its run mark: Marks, one a point, from this
	 * test's own finder, RunsOf below; none, as with the skin off). */
	ShipPaintCore::Point PointOf(const FRawShip& Raw, int32 K, const std::vector<uint8_t>* Marks = nullptr)
	{
		const int32 Kind = FMath::Clamp(Raw.Paints[K], 0, Raw.PaintColours.Num() - 1);
		const FLinearColor& Own = Raw.PaintColours[Kind];
		ShipPaintCore::Point At;
		At.Paint = { Own.R, Own.G, Own.B, Own.A };
		At.bPainted = Raw.ShipPainted.IsValidIndex(Kind) && Raw.ShipPainted[Kind];
		At.Wear = Raw.Wear.IsValidIndex(K) ? FMath::Clamp(Raw.Wear[K], 0, 100) : 0;
		At.Salt = Raw.Salt.IsValidIndex(K) ? Raw.Salt[K] : -1;
		At.Soot = Raw.Soot.IsValidIndex(K) ? Raw.Soot[K] : -1;
		At.Shade = Raw.Worn.IsValidIndex(K) ? Raw.Worn[K] : -1.0f;
		At.X = Raw.Points[K].X;
		At.Y = Raw.Points[K].Y;
		At.Z = Raw.Points[K].Z;
		At.bDeckWood = Raw.ShipPlanks.IsValidIndex(Kind) && Raw.ShipPlanks[Kind];
		At.bBodyAbove = Marks && static_cast<size_t>(K) < Marks->size() && (*Marks)[static_cast<size_t>(K)] != 0;
		return At;
	}

	/** The reader's own run marks, as its block before the loop leaves them (cut out of the tree by extract_paint.py). */
	std::vector<uint8_t> ReaderRuns(const FRawShip& Model, const ShipPaintCore::Options& ShipPaint)
	{
		const FRawShip* Raw = &Model;
#include "new_ship_runs.inc"
		return ShipRuns;
	}

	/** Her points' places to the millimetre, one number for each place (points the mesh splits at a hard edge share it). */
	std::vector<int32> PlacesOf(const FRawShip& Raw, std::vector<std::array<long long, 3>>& Mm)
	{
		std::map<std::array<long long, 3>, int32> Seen;
		std::vector<int32> Place(Raw.Points.size());
		Mm.resize(Raw.Points.size());
		for (size_t K = 0; K < Raw.Points.size(); ++K)
		{
			const FVector& At = Raw.Points[K];
			Mm[K] = { std::llround(At.X * 1000.0), std::llround(At.Y * 1000.0), std::llround(At.Z * 1000.0) };
			Place[K] = Seen.emplace(Mm[K], static_cast<int32>(Seen.size())).first->second;
		}
		return Place;
	}

	/** Patches of the points Keep says, joined by the mesh's triangle sides with both ends kept, a place counted once:
	 * each point's patch (its least place), -1 where not kept. Walked here breadth first, apart from the core's own. */
	std::vector<int32> PatchesOf(const FRawShip& Raw, const std::vector<bool>& Keep, const std::vector<int32>& Place)
	{
		const size_t Places = Place.empty() ? 0 : static_cast<size_t>(*std::max_element(Place.begin(), Place.end())) + 1;
		std::vector<std::vector<int32>> Next(Places);
		std::vector<bool> Kept(Places, false);
		for (size_t K = 0; K < Raw.Points.size(); ++K)
		{
			Kept[static_cast<size_t>(Place[K])] = Kept[static_cast<size_t>(Place[K])] || Keep[K];
		}
		for (size_t T = 0; T + 2 < Raw.Triangles.size(); T += 3)
		{
			for (int Side = 0; Side < 3; ++Side)
			{
				const int32 A = Raw.Triangles[T + static_cast<size_t>(Side)], B = Raw.Triangles[T + static_cast<size_t>((Side + 1) % 3)];
				if (A >= 0 && B >= 0 && A < Raw.Points.Num() && B < Raw.Points.Num() && Keep[static_cast<size_t>(A)] && Keep[static_cast<size_t>(B)])
				{
					Next[static_cast<size_t>(Place[static_cast<size_t>(A)])].push_back(Place[static_cast<size_t>(B)]);
					Next[static_cast<size_t>(Place[static_cast<size_t>(B)])].push_back(Place[static_cast<size_t>(A)]);
				}
			}
		}
		std::vector<int32> PatchOfPlace(Places, -1);
		for (size_t Start = 0; Start < Places; ++Start)
		{
			if (!Kept[Start] || PatchOfPlace[Start] >= 0)
			{
				continue;
			}
			std::vector<int32> Queue = { static_cast<int32>(Start) };
			PatchOfPlace[Start] = static_cast<int32>(Start);
			for (size_t Head = 0; Head < Queue.size(); ++Head)
			{
				for (const int32 To : Next[static_cast<size_t>(Queue[Head])])
				{
					if (PatchOfPlace[static_cast<size_t>(To)] < 0)
					{
						PatchOfPlace[static_cast<size_t>(To)] = static_cast<int32>(Start);
						Queue.push_back(To);
					}
				}
			}
		}
		std::vector<int32> Out(Raw.Points.size(), -1);
		for (size_t K = 0; K < Raw.Points.size(); ++K)
		{
			Out[K] = Keep[K] ? PatchOfPlace[static_cast<size_t>(Place[K])] : -1;
		}
		return Out;
	}

	/** Whether body point B lies above point K as rule 3 has it (ShipPaintCore.h, "A run, and what lies above", in this
	 * test's own words): higher in her own Z by 1 mm to 3 m (millimetres, Mm), no more aside than above (45 degrees or
	 * steeper), no further aside than half the foot's 45 cm plus 1763 in 10000 of the drop (tan 10 degrees), and on the
	 * same face (the facings not more than a right angle apart). */
	bool LiesAbove(const FRawShip& Raw, const std::vector<std::array<long long, 3>>& Mm, size_t B, size_t K)
	{
		const long long Drop = Mm[B][2] - Mm[K][2];
		if (Drop < 1 || Drop > 3000)
		{
			return false;
		}
		const long long Reach = 225 + Drop * 1763 / 10000;
		const long long DX = Mm[B][0] - Mm[K][0], DY = Mm[B][1] - Mm[K][1];
		const long long Aside = DX * DX + DY * DY;
		const FVector &Foot = Raw.Facings[K], &Body = Raw.Facings[B];
		return Aside <= Reach * Reach && Aside <= Drop * Drop && Foot.X * Body.X + Foot.Y * Body.Y + Foot.Z * Body.Z >= 0.0;
	}

	/** This test's own finder of the art director's rule 3, as ShipPaintCore.h says it in words, worked apart from the core
	 * (breadth first and every pair, not the core's union and sorted search): a stained point (painted, wear above 0) lies
	 * at the foot of a run with body above it when a point of its own stained patch with wear of 20 or more lies above it
	 * (LiesAbove). */
	std::vector<uint8_t> RunsOf(const FRawShip& Raw)
	{
		std::vector<std::array<long long, 3>> Mm;
		const std::vector<int32> Place = PlacesOf(Raw, Mm);
		std::vector<bool> Stained(Raw.Points.size(), false);
		for (int32 K = 0; K < Raw.Points.Num(); ++K)
		{
			const ShipPaintCore::Point P = PointOf(Raw, K);
			Stained[static_cast<size_t>(K)] = P.bPainted && P.Wear > 0;
		}
		const std::vector<int32> Patch = PatchesOf(Raw, Stained, Place);
		std::map<int32, std::vector<size_t>> BodiesOf;
		for (int32 K = 0; K < Raw.Points.Num(); ++K)
		{
			if (Stained[static_cast<size_t>(K)] && PointOf(Raw, K).Wear >= 20)
			{
				BodiesOf[Patch[static_cast<size_t>(K)]].push_back(static_cast<size_t>(K));
			}
		}
		std::vector<uint8_t> Marks(Raw.Points.size(), 0);
		for (size_t K = 0; K < Raw.Points.size(); ++K)
		{
			if (!Stained[K])
			{
				continue;
			}
			const auto Found = BodiesOf.find(Patch[K]);
			if (Found == BodiesOf.end())
			{
				continue;
			}
			for (const size_t B : Found->second)
			{
				if (LiesAbove(Raw, Mm, B, K))
				{
					Marks[K] = 1;
					break;
				}
			}
		}
		return Marks;
	}

	/** A colour in sRGB levels (0-255, as floats) and what the art director measures of it. */
	struct Levels
	{
		double R = 0.0, G = 0.0, B = 0.0;
		double Bright() const { return 0.2126 * R + 0.7152 * G + 0.0722 * B; }
		double Chroma() const { return std::max({ R, G, B }) - std::min({ R, G, B }); }
	};

	Levels LevelsOf(float LinR, float LinG, float LinB)
	{
		return { ShipPaintCore::ToSrgb(LinR) * 255.0, ShipPaintCore::ToSrgb(LinG) * 255.0, ShipPaintCore::ToSrgb(LinB) * 255.0 };
	}

	Levels LevelsOf(const ShipPaintCore::Linear& C)
	{
		return LevelsOf(C.R, C.G, C.B);
	}

	std::string Said(const Levels& C)
	{
		return Num(C.R) + "," + Num(C.G) + "," + Num(C.B);
	}

	ShipPaintCore::Palette PaletteOf(const FRawShip& Raw)
	{
		ShipPaintCore::Palette Pal;
		Pal.Rust = { Raw.Rust.R, Raw.Rust.G, Raw.Rust.B, Raw.Rust.A };
		const FLinearColor Dried(FColor(214, 210, 198));
		Pal.Salt = { Dried.R, Dried.G, Dried.B, Dried.A };
		Pal.bSoot = Raw.PaintColours.Num() > 8;
		if (Pal.bSoot)
		{
			Pal.Soot = { Raw.PaintColours[8].R, Raw.PaintColours[8].G, Raw.PaintColours[8].B, Raw.PaintColours[8].A };
		}
		return Pal;
	}

	/** How many points of two paintings differ: in their bytes (and finish), or in their floats bit for bit. */
	size_t Differ(const Painted& A, const Painted& B)
	{
		if (A.Colours.size() != B.Colours.size() || A.Linear.size() != B.Linear.size())
		{
			return std::max(A.Colours.size(), B.Colours.size()) + 1;
		}
		size_t Out = 0;
		for (size_t K = 0; K < A.Colours.size(); ++K)
		{
			const FColor &X = A.Colours[K], &Y = B.Colours[K];
			const bool bBytes = X.R == Y.R && X.G == Y.G && X.B == Y.B && X.A == Y.A;
			const bool bFloats = std::memcmp(&A.Linear[4 * K], &B.Linear[4 * K], 4 * sizeof(float)) == 0;
			Out += bBytes && bFloats ? 0 : 1;
		}
		return Out;
	}

	float Level(float Lin)
	{
		return ShipPaintCore::ToSrgb(Lin) * 255.0f;
	}

	uint32_t ShadeHash(int32 K, uint32_t Salt)
	{
		uint32_t H = static_cast<uint32_t>(K) * 2654435761u ^ Salt * 0x9E3779B9u;
		H ^= H >> 15;
		H *= 0x85EBCA6Bu;
		H ^= H >> 13;
		return H;
	}

	/** Today's files carry no shade: a made-up one for the test (0 to 1, some exactly 1, some 0). */
	void GiveShade(FRawShip& Raw)
	{
		Raw.Worn.clear();
		for (int32 K = 0; K < Raw.Points.Num(); ++K)
		{
			const uint32_t H = ShadeHash(K, 77u);
			Raw.Worn.Add(H % 7u == 0u ? 1.0f : H % 11u == 0u ? 0.0f : 0.3f + 0.7f * static_cast<float>(H & 0xFFFFu) / 65535.0f);
		}
	}

	/** A U-boat's weeps: the ribbons on her skin (paint hull.on or upper.on, by the file's names) that carry rust. */
	struct Run
	{
		std::vector<int32> Points;
		double X = 0.0, Top = -1e9, Foot = 1e9;
		int Side = 0;
		int32 Head = -1; // (its most worn point)
	};

	std::vector<Run> WeepsOf(const std::string& Path, const ShipFile& Ship)
	{
		std::string Text, Why;
		LandJson::Json File;
		std::vector<Run> Out;
		if (!LandJson::ReadFile(Path, Text) || !LandJson::Parse(Text, File, Why))
		{
			return Out;
		}
		const FRawShip& Raw = Ship.Mesh[0];
		std::set<int32> On;
		for (size_t K = 0; K < Ship.PaintNames.size(); ++K)
		{
			if (Ship.PaintNames[K] == "hull.on" || Ship.PaintNames[K] == "upper.on")
			{
				On.insert(static_cast<int32>(K));
			}
		}
		const std::vector<double> Triangles = LandJson::Numbers(File["near"]["triangles"]);
		std::vector<int32> Parent(Raw.Points.size());
		for (size_t K = 0; K < Parent.size(); ++K)
		{
			Parent[K] = static_cast<int32>(K);
		}
		auto Find = [&Parent](int32 A)
		{
			while (Parent[A] != A)
			{
				Parent[A] = Parent[Parent[A]];
				A = Parent[A];
			}
			return A;
		};
		auto Lies = [&](int32 P) { return On.count(Raw.Paints[P]) == 1; };
		for (size_t T = 0; T + 2 < Triangles.size(); T += 3)
		{
			const int32 A = static_cast<int32>(Triangles[T]), B = static_cast<int32>(Triangles[T + 1]), C = static_cast<int32>(Triangles[T + 2]);
			if (Lies(A) && Lies(B) && Lies(C))
			{
				Parent[Find(A)] = Find(B);
				Parent[Find(B)] = Find(C);
			}
		}
		std::map<int32, Run> Runs;
		for (int32 P = 0; P < Raw.Points.Num(); ++P)
		{
			if (Lies(P))
			{
				Run& R = Runs[Find(P)];
				R.Points.push_back(P);
			}
		}
		for (auto& [Root, R] : Runs)
		{
			int Most = 0;
			double SumX = 0.0, SumY = 0.0;
			for (const int32 P : R.Points)
			{
				const int W = Raw.Wear.IsValidIndex(P) ? Raw.Wear[P] : 0;
				if (W > Most)
				{
					Most = W;
					R.Head = P;
				}
				SumX += Raw.Points[P].X;
				SumY += Raw.Points[P].Y;
				R.Top = std::max(R.Top, Raw.Points[P].Z);
				R.Foot = std::min(R.Foot, Raw.Points[P].Z);
			}
			R.X = SumX / R.Points.size();
			R.Side = SumY >= 0.0 ? 1 : -1;
			if (Most >= 30 && R.Top - R.Foot > 0.05)
			{
				Out.push_back(R);
			}
		}
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: ship_paint_test <Source/TheaterCommand> <Content/Models/Ships>\n");
		return 2;
	}
	const std::string Src = Argv[1], Ships = Argv[2];
	std::vector<std::string> Paths;
	for (const auto& Entry : std::filesystem::directory_iterator(Ships))
	{
		const std::string Name = Entry.path().filename().string();
		if (Entry.path().extension() == ".json" && Name.find(".sockets.") == std::string::npos)
		{
			Paths.push_back(Entry.path().string());
		}
	}
	std::sort(Paths.begin(), Paths.end());
	std::vector<ShipFile> Files;
	for (const std::string& Path : Paths)
	{
		ShipFile One;
		One.Name = std::filesystem::path(Path).stem().string();
		std::string Why;
		const bool bRead = Load(Path, One, Why);
		Check(bRead, "read " + One.Name, bRead ? std::to_string(One.Mesh[0].Points.Num()) + " points near, " + std::to_string(One.Mesh[1].Points.Num()) + " far" : Why);
		if (bRead)
		{
			Files.push_back(std::move(One));
		}
	}
	Check(Files.size() == 26, "every ship file the game has (7 battleships, 3 carriers, 6 cruisers, 4 destroyers, 3 merchants, 3 U-boats)", std::to_string(Files.size()));

	std::printf("(a) Off: every point as at 55419eb6, bit for bit\n");
	{
		size_t Points = 0, Worn = 0, Salted = 0, Sooted = 0;
		for (const ShipFile& Ship : Files)
		{
			for (int Far = 0; Far < 2; ++Far)
			{
				const FRawShip& Raw = Ship.Mesh[Far];
				const Painted Old = PaintOld(Raw), New = PaintNew(Raw, ShipPaintCore::Options());
				const size_t Bad = Differ(Old, New);
				// (and again with a made-up shade on every point: today's files carry none, so only this leg can see a shade
				// painted with every switch off, a flipped default or the switch's gate lost)
				FRawShip Shaded = Raw;
				GiveShade(Shaded);
				const Painted OldShaded = PaintOld(Shaded), NewShaded = PaintNew(Shaded, ShipPaintCore::Options());
				const size_t BadShaded = Differ(OldShaded, NewShaded);
				const size_t Total = static_cast<size_t>(Raw.Points.Num());
				Check(Bad == 0 && BadShaded == 0 && Old.Colours.size() == Total && OldShaded.Colours.size() == Total,
					Ship.Name + (Far ? " far" : " near") + ": every point's floats, bytes and finish as the reference's, as the file is and with a made-up shade",
					std::to_string(Old.Colours.size() - std::min(Old.Colours.size(), Bad)) + " of " + std::to_string(Total) + " alike, " +
						std::to_string(OldShaded.Colours.size() - std::min(OldShaded.Colours.size(), BadShaded)) + " shaded");
				Points += Raw.Points.size();
				for (int32 K = 0; K < Raw.Points.Num(); ++K)
				{
					const ShipPaintCore::Point P = PointOf(Raw, K);
					Worn += P.bPainted && P.Wear > 0 ? 1 : 0;
					Salted += P.bPainted && P.Salt > 0 ? 1 : 0;
					Sooted += P.Soot > 4 ? 1 : 0;
				}
			}
		}
		Check(Worn > 100000 && Salted > 1000 && Sooted > 1000, "the files exercise every step of today's sum (rust, salt, soot)",
			std::to_string(Points) + " points: " + std::to_string(Worn) + " rusted, " + std::to_string(Salted) + " salted, " + std::to_string(Sooted) + " sooted");
		Check(ShipPaintCore::KeyOf(ShipPaintCore::Options()).empty() && ShipPaintCore::PaintsAsToday(ShipPaintCore::Options()),
			"today's options leave the painted model's cache key as it was (the map's, Pearl's and Midway's keys unchanged)", "\"" + ShipPaintCore::KeyOf(ShipPaintCore::Options()) + "\"");
		const ShipPaintCore::Options Made;
		Check(!Made.bShade && !Made.bRustRamp && Made.TopLight < 0.0f, "every option as made is off (no top light set)", "shade, rust ramp, top light " + Num(Made.TopLight, 2));
		ShipPaintCore::Options Lit;
		Lit.TopLight = ShipPaintCore::SeaTopLight;
		Check(ShipPaintCore::KeyOf(Lit).empty(), "the top light is the ink's, not a point's: it leaves the points and the key alone", "\"" + ShipPaintCore::KeyOf(Lit) + "\"");
		Check(ShipPaintCore::SeaTopLight == 0.10f && ShipPaintCore::MapTopLight == 0.26f, "the ships' top light at sea, the art director's 0.10 (the map's 0.26), one named number",
			Num(ShipPaintCore::SeaTopLight, 2));
	}

	std::printf("(b) The map's and the air stage's callers never pass the options\n");
	{
		std::map<std::string, std::string> Text;
		for (const auto& Entry : std::filesystem::recursive_directory_iterator(Src))
		{
			const std::string Ext = Entry.path().extension().string();
			if (Ext == ".cpp" || Ext == ".h" || Ext == ".inl")
			{
				Text[std::filesystem::relative(Entry.path(), Src).string()] = ReadAll(Entry.path().string());
			}
		}
		auto Count = [](const std::string& In, const std::string& What)
		{
			size_t N = 0;
			for (size_t At = In.find(What); At != std::string::npos; At = In.find(What, At + 1))
			{
				++N;
			}
			return N;
		};
		std::string Setters, Callers;
		for (const auto& [File, Body] : Text)
		{
			if (Count(Body, "FMiniatureMesh::ShipPaint") > 0)
			{
				Setters += (Setters.empty() ? "" : ", ") + File + " x" + std::to_string(Count(Body, "FMiniatureMesh::ShipPaint"));
			}
			if (File.find("AirStage.") == std::string::npos && Count(Body, "SetShipPaint(") > 0)
			{
				Callers += (Callers.empty() ? "" : ", ") + File + " x" + std::to_string(Count(Body, "SetShipPaint("));
			}
		}
		const std::string& Minis = Text["Campaign/CampaignMiniatures.cpp"];
		const std::string& Stage = Text["Battle/AirStage.cpp"];
		const std::string& Scene = Text["Battle/SeaBattle.cpp"];
		Check(Setters == "Battle/AirStage.cpp x1, Campaign/CampaignMiniatures.cpp x1" && Has(Minis, "ShipPaintCore::Options FMiniatureMesh::ShipPaint;\n") &&
				  Has(Stage, "TGuardValue<ShipPaintCore::Options> Painting(FMiniatureMesh::ShipPaint, Paint);"),
			"the miniatures' options are set in one place only, the stage's HullModel, for the one call and given back after", Setters);
		Check(Has(Stage, "const ShipPaintCore::Options& Paint = ShipPaintCore::Options())") && Count(Stage, "HullModel(") == 4 && Count(Stage, ", ShipPaint)") == 3,
			"HullModel paints today's unless handed the stage's own options: LayShip's near and far, and the break's halves", std::to_string(Count(Stage, ", ShipPaint)")) + " calls");
		Check(Count(Stage, "\tShipPaint = ") == 1 && Has(Stage, "void AAirStage::SetShipPaint(const ShipPaintCore::Options& Given, float FirstLookWear)\n{\n\tShipPaint = Given;"),
			"the stage's own options set only by SetShipPaint", "AirStage.cpp");
		const size_t Call = Scene.find("Stage->SetShipPaint(");
		const size_t Open = Scene.find("\nbool ASeaBattle::OpenOn("), OpenEnd = Open == std::string::npos ? Open : Scene.find("\n}\n", Open);
		Check(Callers == "Battle/SeaBattle.cpp x1" && Call != std::string::npos && Open != std::string::npos && Call > Open && Call < OpenEnd,
			"SetShipPaint called only from the sea battle's own opening (ASeaBattle::OpenOn): never by the air scene, Pearl Harbor, Midway or the map (the month's sea scene opens the same way, and the next checks keep the switches from it)",
			Callers);
		const size_t Gate = Scene.rfind("if (bShipLight || bShipSkin)", Call);
		Check(Call != std::string::npos && Gate != std::string::npos && Call - Gate < 600, "and only behind its switches (-TheaterSeaShipLight= on, or -TheaterSeaShipSkin=1)", "SeaBattle.cpp");
		// (the three ship switches are for review pictures only, as the reel's two: the month's sea scene inside a campaign
		// opens the same way, with bMonthScene set first, and never takes them)
		const size_t Month = Scene.find("\nbool ASeaBattle::BeginMonthScene(");
		const size_t MonthSet = Scene.find("bMonthScene = true;", Month), MonthOpen = Scene.find("OpenOn(", Month);
		bool bGated = MonthSet != std::string::npos && MonthOpen != std::string::npos && MonthSet < MonthOpen && !Has(Scene, "TakeOnOff(SeaLook, \"TheaterSeaShip") &&
					  !Has(Scene, "TakeReview(SeaLook, \"TheaterSeaShipLight=");
		for (const char* Line : { "const SeaLookSwitches::Number ShipLight = SeaLookSwitches::TakeReviewNumber(SeaLook, \"TheaterSeaShipLight=\", !bMonthScene, ShipPaintCore::SeaTopLight, ShipPaintCore::SeaTopLightMost);",
				 "const bool bShipLight = ShipLight.Value > 0.0f;",
				 "const bool bShipSkin = SeaLookSwitches::TakeReview(SeaLook, \"TheaterSeaShipSkin=\", !bMonthScene);",
				 "bShipShade = SeaLookSwitches::TakeReview(SeaLook, \"TheaterSeaShipShade=\", !bMonthScene);" })
		{
			const size_t At = Scene.find(Line);
			bGated = bGated && At != std::string::npos && At > Open && At < OpenEnd;
		}
		Check(bGated, "the month's sea scene never takes the three ship switches (bMonthScene set before OpenOn, each switch read with !bMonthScene)", "SeaBattle.cpp");
		{
			int SkinAt = -1;
			for (int K = 0; K < SeaLookSwitches::Count; ++K)
			{
				SkinAt = std::string(SeaLookSwitches::All[K].Name) == "TheaterSeaShipSkin=" ? K : SkinAt;
			}
			SeaLookSwitches::Look AskedSkin;
			AskedSkin.Given.push_back({ SkinAt, "1" });
			SeaLookSwitches::Look InMonth = AskedSkin, InBattle = AskedSkin, None;
			const bool bMonth = SeaLookSwitches::TakeReview(InMonth, "TheaterSeaShipSkin=", false);
			const bool bBattle = SeaLookSwitches::TakeReview(InBattle, "TheaterSeaShipSkin=", true);
			const bool bNone = SeaLookSwitches::TakeReview(None, "TheaterSeaShipSkin=", true);
			const std::string MonthLine = SeaLookSwitches::LineOf(SeaLookSwitches::All, SeaLookSwitches::Count, 0, InMonth.Given);
			Check(SkinAt >= 0 && !bMonth && InMonth.Given[0].Value == "0" && bBattle && InBattle.Given[0].Value == "1" && !bNone &&
					  Has(MonthLine, "-TheaterSeaShipSkin=0 (asked)"),
				"a review switch asked for in the month's scene is off and said as 0 on the \"Sea look:\" line; in a battle, as asked", std::string("in the month's scene ") + InMonth.Given[0].Value + ", in a battle " + InBattle.Given[0].Value);
		}
		{
			// (the light switch takes a value: off or 0, none; 1 or on, the art director's 0.10; a top light from 0.0 to 0.30,
			// to at most four places after the point; anything else refused, said in the log and taken as off, a light too fine
			// for the "Sea look:" line to say among it; in the month's scene, off whatever is asked)
			int LightAt = -1;
			for (int K = 0; K < SeaLookSwitches::Count; ++K)
			{
				LightAt = std::string(SeaLookSwitches::All[K].Name) == "TheaterSeaShipLight=" ? K : LightAt;
			}
			struct LightAsked
			{
				const char* Typed;
				bool bReview;
				float Want;
				bool bRefused;
			};
			const LightAsked Asked[] = { { "1", true, ShipPaintCore::SeaTopLight, false }, { "on", true, ShipPaintCore::SeaTopLight, false }, { "0.08", true, 0.08f, false },
				{ "0.10", true, 0.10f, false }, { "0.12", true, 0.12f, false }, { "0.26", true, ShipPaintCore::MapTopLight, false }, { "0.30", true, 0.30f, false },
				{ "0", true, 0.0f, false }, { "off", true, 0.0f, false }, { "0.31", true, 0.0f, true }, { "1.0", true, 0.0f, true }, { "-0.1", true, 0.0f, true },
				{ "bright", true, 0.0f, true }, { "0.0001", true, 0.0001f, false }, { "0.10000", true, 0.10f, false }, { "0.00001", true, 0.0f, true },
				{ "0.0000000001", true, 0.0f, true }, { "0.10", false, 0.0f, false }, { "1", false, 0.0f, false } };
			std::string Took;
			bool bRight = LightAt >= 0;
			for (const LightAsked& One : Asked)
			{
				SeaLookSwitches::Look Asks;
				Asks.Given.push_back({ LightAt, One.Typed });
				const SeaLookSwitches::Number Got = SeaLookSwitches::TakeReviewNumber(Asks, "TheaterSeaShipLight=", One.bReview, ShipPaintCore::SeaTopLight, ShipPaintCore::SeaTopLightMost);
				bRight = bRight && Got.Value == One.Want && Got.bRefused == One.bRefused && Asks.Given[0].Value == SeaLookSwitches::NumberSaid(One.Want);
				Took += std::string(Took.empty() ? "" : ", ") + One.Typed + (One.bReview ? "" : " (month)") + " " + Asks.Given[0].Value + (Got.bRefused ? " refused" : "");
			}
			const size_t Refusal = Scene.find("if (ShipLight.bRefused)");
			Check(bRight && Refusal != std::string::npos && Refusal > Open && Refusal < OpenEnd && Has(Scene, "Paint.TopLight = bShipLight ? ShipLight.Value : -1.0f;") &&
					  ShipPaintCore::SeaTopLightMost == 0.30f,
				"-TheaterSeaShipLight= takes off or 0, 1 or on (0.10), or a top light from 0.0 to 0.30 to four places for the ink; anything else refused (said in the log) and off; the month's scene off",
				Took);
		}
		Check(Has(Minis, "Raw->ShipPlanks.Add(Paint == TEXT(\"planks\"));") && Has(Minis, "At.bDeckWood = Raw->ShipPlanks.IsValidIndex(Kind) && Raw->ShipPlanks[Kind];"),
			"the reader marks a wooden deck by the file's paint name, planks, as this test reads it", "CampaignMiniatures.cpp");
		// (in the miniatures: the options read only by the ship branch and the ship's own cache key)
		size_t Reads = 0;
		for (size_t At = Minis.find("ShipPaint"); At != std::string::npos; At = Minis.find("ShipPaint", At + 1))
		{
			const bool bWord = (At == 0 || !(std::isalnum(static_cast<unsigned char>(Minis[At - 1])) || Minis[At - 1] == '_' || Minis[At - 1] == ':')) &&
							   !(std::isalnum(static_cast<unsigned char>(Minis[At + 9])) || Minis[At + 9] == '_');
			Reads += bWord ? 1 : 0;
		}
		Check(Reads == 3 && Has(Minis, "(Raw->bShip ? FString(UTF8_TO_TCHAR(ShipPaintCore::KeyOf(ShipPaint).c_str())) : FString())") &&
				  Has(Minis, "ShipPaintCore::Paint(At, ShipColours, ShipPaint)") && Has(Minis, "\t\tif (Raw->bShip && ShipPaint.bRustRamp && Raw->PaintColours.Num() > 0)\n"),
			"the miniatures read them in three places only: a ship's cache key, the run marks under the skin, and the ship branch", std::to_string(Reads) + " reads");
		// (the run marks: made only under the skin, by the core's BodyAbove from the same wear and paints the branch hands the
		// core, the file's own triangles and each point's facing, and handed to each point; with the skin off nothing is made,
		// and nothing read)
		Check(Has(Minis, "ShipRuns = ShipPaintCore::BodyAbove(Runs, Raw->Triangles.GetData(), static_cast<size_t>(Raw->Triangles.Num()));") &&
				  Has(Minis, "At.bBodyAbove = static_cast<size_t>(K) < ShipRuns.size() && ShipRuns[static_cast<size_t>(K)] != 0;") &&
				  Has(Minis, "\t\t\t\tRun.FX = Raw->Facings[K].X;\n\t\t\t\tRun.FY = Raw->Facings[K].Y;\n\t\t\t\tRun.FZ = Raw->Facings[K].Z;\n") &&
				  Has(Minis, "Run.Wear = Raw->Wear.IsValidIndex(K) ? FMath::Clamp(Raw->Wear[K], 0, 100) : 0;") &&
				  Has(Minis, "Run.bPainted = Raw->ShipPainted.IsValidIndex(Kind) && Raw->ShipPainted[Kind];") && Count(Minis, "ShipPaintCore::BodyAbove(") == 1 &&
				  Count(Minis, "ShipRuns") == 4,
			"the reader marks a run's foot only under the skin (ShipPaintCore::BodyAbove, from the wear and paints the branch reads, the file's triangles and each point's facing) and hands each point its mark",
			"CampaignMiniatures.cpp");
	}

	// (each mesh's run marks by this test's own finder, rule 3's "body above it", read by (c) and (e))
	std::vector<std::array<std::vector<uint8_t>, 2>> RunMarks(Files.size());
	for (size_t F = 0; F < Files.size(); ++F)
	{
		for (int Far = 0; Far < 2; ++Far)
		{
			RunMarks[F][static_cast<size_t>(Far)] = RunsOf(Files[F].Mesh[Far]);
		}
	}
	const auto MarksOf = [&Files, &RunMarks](const ShipFile& Ship, int Far) -> const std::vector<uint8_t>&
	{ return RunMarks[static_cast<size_t>(&Ship - Files.data())][static_cast<size_t>(Far)]; };

	std::printf("(c) On\n");
	{
		// The shade only darkens (a made-up shade on every file: today's carry none).
		size_t Shaded = 0, Lighter = 0, Moved = 0, Kept = 0;
		ShipPaintCore::Options Shade;
		Shade.bShade = true;
		for (const ShipFile& Ship : Files)
		{
			FRawShip Raw = Ship.Mesh[0];
			const Painted Bare = PaintNew(Raw, Shade);
			Check(Differ(Bare, PaintNew(Raw, ShipPaintCore::Options())) == 0, Ship.Name + ": with no shade in her file, -TheaterSeaShipLight paints her as today", "alike");
			GiveShade(Raw);
			const Painted Dark = PaintNew(Raw, Shade), Plain = PaintNew(Raw, ShipPaintCore::Options());
			for (int32 K = 0; K < Raw.Points.Num(); ++K)
			{
				const float* D = &Dark.Linear[4 * K];
				const float* L = &Plain.Linear[4 * K];
				Lighter += D[0] > L[0] || D[1] > L[1] || D[2] > L[2] || D[3] != L[3] ? 1 : 0;
				const bool bSame = std::memcmp(D, L, 4 * sizeof(float)) == 0;
				Moved += !bSame && Raw.Worn[K] == 1.0f ? 1 : 0;
				Kept += bSame && Raw.Worn[K] < 1.0f && (L[0] > 0.0f || L[1] > 0.0f || L[2] > 0.0f) ? 1 : 0;
				++Shaded;
			}
		}
		Check(Lighter == 0, "the shade never lightens a channel (nor touches the alpha)", std::to_string(Shaded) + " points, " + std::to_string(Lighter) + " lighter");
		Check(Moved == 0 && Kept == 0, "a shade of 1 changes nothing, and less than 1 always darkens", std::to_string(Moved) + " moved at 1, " + std::to_string(Kept) + " kept below 1");

		// The rust ramp, point by point, on every file near and far.
		ShipPaintCore::Options Skin;
		Skin.bRustRamp = true;
		size_t Rusted = 0, OutOfBox = 0, OverWrong = 0, Brighter = 0, SourceBright = 0, Sources = 0, RedHigh = 0, Orange = 0, Unlike = 0;
		size_t Feet = 0, Bodies = 0, Heads = 0, Ramped = 0;
		double MostRed = 0.0, MostOverBlue = -1e9;
		const ShipPaintCore::Step &Src0 = ShipPaintCore::Source, &Dark0 = ShipPaintCore::SourceDark, &Foot0 = ShipPaintCore::Foot;
		for (const ShipFile& Ship : Files)
		{
			for (int Far = 0; Far < 2; ++Far)
			{
				const FRawShip& Raw = Ship.Mesh[Far];
				const Painted Once = PaintNew(Raw, Skin), Again = PaintNew(Raw, Skin);
				Unlike += Differ(Once, Again);
				for (int32 K = 0; K < Raw.Points.Num(); ++K)
				{
					// (every point the file wears, with its run mark: the ramp's own numbers hold wherever it is asked; under the
					// skin it is asked at a wear of 20 and more, and at a run's foot, and light wear elsewhere is (e)'s rule 2)
					const ShipPaintCore::Point P = PointOf(Raw, K, &MarksOf(Ship, Far));
					if (!P.bPainted || P.Wear <= 0)
					{
						continue;
					}
					++Rusted;
					Ramped += P.Wear >= ShipPaintCore::RunBodyWear || P.bBodyAbove ? 1 : 0;
					ShipPaintCore::Laid How;
					const ShipPaintCore::Linear Coat = ShipPaintCore::Ramp(P, &How);
					// (the ramp's own colour between its source and its foot, a level's give)
					const float R = Level(How.Colour.R), G = Level(How.Colour.G), B = Level(How.Colour.B);
					OutOfBox += R < Dark0.R - 0.6f || R > Foot0.R + 0.6f || G < Dark0.G - 0.6f || G > Foot0.G + 0.6f || B < Dark0.B - 0.6f || B > Foot0.B + 0.6f ? 1 : 0;
					// (how much is laid, by step: the film no more than 35%, the body 45-70%, the source 80-100%)
					if (How.Along <= 0.15f)
					{
						++Feet;
						OverWrong += How.Over > Foot0.MostOver + 1e-5f ? 1 : 0;
					}
					else if (How.Along >= 0.35f && How.Along <= 0.60f)
					{
						++Bodies;
						OverWrong += How.Over < ShipPaintCore::Body.LeastOver - 1e-5f || How.Over > ShipPaintCore::Body.MostOver + 1e-5f ? 1 : 0;
					}
					else if (How.Along >= 0.85f)
					{
						++Heads;
						OverWrong += How.Over < Src0.LeastOver - 1e-5f || How.Over > Src0.MostOver + 1e-5f ? 1 : 0;
					}
					// (never lighter than the paint under it; at the source no more than half its brightness but on a dark paint)
					const float Under = ShipPaintCore::Brightness(P.Paint), Now = ShipPaintCore::Brightness(Coat);
					Brighter += Now > Under * 1.0001f ? 1 : 0;
					if (How.Along >= 0.9f)
					{
						++Sources;
						const float Allowed = std::max(0.55f, std::min(1.0f, ShipPaintCore::Brightness(How.Colour) / std::max(Under, 1e-6f)));
						SourceBright += Now > Allowed * Under * 1.0001f ? 1 : 0;
					}
					const float PaintRed = Level(P.Paint.R), PaintBlue = Level(P.Paint.B);
					const float Red = Level(Coat.R), Blue = Level(Coat.B);
					RedHigh += Red > std::max(135.0f, PaintRed) + 0.5f ? 1 : 0;
					Orange += Red - Blue > std::max(80.0f, PaintRed - PaintBlue) + 0.5f ? 1 : 0;
					MostRed = std::max<double>(MostRed, Red);
					MostOverBlue = std::max<double>(MostOverBlue, Red - Blue);
				}
			}
		}
		Check(Rusted > 100000 && Feet > 1000 && Bodies > 1000 && Heads > 100, "the ramp's three steps all met in the files: its film, its body and its source",
			std::to_string(Rusted) + " worn points: " + std::to_string(Feet) + " film, " + std::to_string(Bodies) + " body, " + std::to_string(Heads) + " source (" +
				std::to_string(Ramped) + " of them ramped under the skin, the rest light wear)");
		Check(OutOfBox == 0, "the ramp's colour from the source's (52,34,28 to 66,42,32) to the foot's 124,80,50 and nowhere else", std::to_string(OutOfBox) + " outside");
		Check(OverWrong == 0, "laid over the coat by step: the film at most 35%, the body 45 to 70%, the source 80 to 100%", std::to_string(OverWrong) + " out of step");
		Check(Brighter == 0, "never lighter than the paint under it (edge wear darker, never whiter)", std::to_string(Brighter) + " lighter");
		Check(SourceBright == 0 && Sources > 0, "at the source no more than about half the paint's brightness (on a dark paint, its own brightness)",
			std::to_string(Sources) + " at the source, " + std::to_string(SourceBright) + " too bright");
		Check(RedHigh == 0 && Orange == 0, "never red above 135, never red more than 80 over blue", "most red " + Num(MostRed) + ", most over blue " + Num(MostOverBlue));
		Check(Unlike == 0, "the same file painted the same way every time (the look's hash, no draw)", std::to_string(Unlike) + " unlike");

		// Edge wear on the surface ships: darker under the ramp; today's single rust lightens some of it.
		size_t Edges = 0, Darker = 0, TodayLighter = 0, Chips = 0, ChipsDarker = 0;
		for (const ShipFile& Ship : Files)
		{
			if (Ship.Name.rfind("Submarine", 0) == 0)
			{
				continue;
			}
			const FRawShip& Raw = Ship.Mesh[0];
			const ShipPaintCore::Palette Pal = PaletteOf(Raw);
			for (int32 K = 0; K < Raw.Points.Num(); ++K)
			{
				ShipPaintCore::Point P = PointOf(Raw, K, &MarksOf(Ship, 0));
				if (!P.bPainted || P.Wear <= 0)
				{
					continue;
				}
				P.Salt = -1;
				P.Soot = -1;
				const float Under = ShipPaintCore::Brightness(P.Paint);
				const bool bDarker = ShipPaintCore::Brightness(ShipPaintCore::Paint(P, Pal, Skin)) < Under;
				TodayLighter += ShipPaintCore::Brightness(ShipPaintCore::Paint(P, Pal, ShipPaintCore::Options())) > Under * 1.0001f ? 1 : 0;
				// (the plan's own measure: worn is wear above 4 of 100; below it, an edge's lightest chips, the grain decides)
				Edges += P.Wear > 4 ? 1 : 0;
				Darker += P.Wear > 4 && bDarker ? 1 : 0;
				Chips += P.Wear <= 4 ? 1 : 0;
				ChipsDarker += P.Wear <= 4 && bDarker ? 1 : 0;
			}
		}
		Check(Edges > 5000 && Darker * 100 >= Edges * 95, "the surface ships' worn edges (wear above 4, the plan's measure) go darker under the ramp",
			std::to_string(Darker) + " of " + std::to_string(Edges) + " darker; of the lightest chips (1 to 4) " + std::to_string(ChipsDarker) + " of " + std::to_string(Chips) +
				" (today's one rust lightens " + std::to_string(TodayLighter) + ")");

		// The salt as ruled (the U-boats): toward 196,200,198 by no more than 30%, less over a rust source. Her numbers are
		// written here, not read from the core, and the core's own are pinned to them.
		size_t Salts = 0, SaltOver = 0;
		double MostShare = 0.0;
		const ShipPaintCore::Linear Pale = { ShipPaintCore::ToLinear(196.0f / 255.0f), ShipPaintCore::ToLinear(200.0f / 255.0f), ShipPaintCore::ToLinear(198.0f / 255.0f), 1.0f };
		for (const ShipFile& Ship : Files)
		{
			const FRawShip& Raw = Ship.Mesh[0];
			const ShipPaintCore::Palette Pal = PaletteOf(Raw);
			for (int32 K = 0; K < Raw.Points.Num(); ++K)
			{
				ShipPaintCore::Point P = PointOf(Raw, K, &MarksOf(Ship, 0));
				if (!P.bPainted || P.Salt <= 0)
				{
					continue;
				}
				++Salts;
				P.Soot = -1;
				ShipPaintCore::Point Dry = P;
				Dry.Salt = -1;
				const ShipPaintCore::Linear Before = ShipPaintCore::Paint(Dry, Pal, Skin), After = ShipPaintCore::Paint(P, Pal, Skin);
				for (const int C : { 0, 1, 2 })
				{
					const float A = C == 0 ? Before.R : C == 1 ? Before.G : Before.B, B = C == 0 ? After.R : C == 1 ? After.G : After.B;
					const float T = C == 0 ? Pale.R : C == 1 ? Pale.G : Pale.B;
					if (std::abs(T - A) > 0.05f)
					{
						const double Share = (B - A) / (T - A);
						MostShare = std::max(MostShare, Share);
						SaltOver += Share > 0.30 + 1e-4 ? 1 : 0;
					}
				}
			}
		}
		const bool bSaltRuled = ShipPaintCore::SaltR == 196.0f && ShipPaintCore::SaltG == 200.0f && ShipPaintCore::SaltB == 198.0f && ShipPaintCore::SaltMost == 0.30f;
		Check(Salts > 1000 && SaltOver == 0 && bSaltRuled, "the salt toward 196,200,198 by no more than 30% (and the core's salt pinned to her 196,200,198 at 30%)",
			std::to_string(Salts) + " salted points, the most " + Num(100.0 * MostShare) + "%; the core's " + Num(ShipPaintCore::SaltR, 0) + "," + Num(ShipPaintCore::SaltG, 0) + "," +
				Num(ShipPaintCore::SaltB, 0) + " at " + Num(100.0 * ShipPaintCore::SaltMost, 0) + "%");

		// Submarine_GER: her weeps. Today: salmon, a tenth darker than her paint, every one the same: painted stripes. Under
		// the ramp: dark at the source, thinning down the run, and no two neighbours alike.
		const auto Ger = std::find_if(Files.begin(), Files.end(), [](const ShipFile& One) { return One.Name == "Submarine_GER"; });
		Check(Ger != Files.end(), "Submarine_GER read", Ger != Files.end() ? "yes" : "no");
		if (Ger != Files.end())
		{
			std::vector<Run> Weeps = WeepsOf(Ships + "/Submarine_GER.json", *Ger);
			const FRawShip& Raw = Ger->Mesh[0];
			size_t TodayAlike = 0, TodayFlatAlike = 0;
			for (const bool bOn : { false, true })
			{
				const Painted Look = bOn ? PaintNew(Raw, Skin) : PaintOld(Raw);
				auto Bright = [&Look](int32 P) { return 0.2126f * Look.Linear[4 * P] + 0.7152f * Look.Linear[4 * P + 1] + 0.0722f * Look.Linear[4 * P + 2]; };
				auto LevelAt = [&Look](int32 P, int C) { return Level(Look.Linear[4 * P + C]); };
				std::vector<double> HeadShare;
				std::vector<bool> FlatRun;
				size_t Flat = 0;
				for (const Run& R : Weeps)
				{
					const ShipPaintCore::Point Head = PointOf(Raw, R.Head);
					const float Under = ShipPaintCore::Brightness(Head.Paint);
					float Darkest = 1e9f, Lightest = 0.0f;
					for (const int32 P : R.Points)
					{
						if (Raw.Wear.IsValidIndex(P) && Raw.Wear[P] > 0)
						{
							Darkest = std::min(Darkest, Bright(P));
							Lightest = std::max(Lightest, Bright(P));
						}
					}
					HeadShare.push_back(Darkest / std::max(Under, 1e-6f));
					// (one flat colour: the run's worn points within two levels of one another in brightness)
					FlatRun.push_back(Level(Lightest) - Level(Darkest) < 8.0f);
					Flat += FlatRun.back() ? 1 : 0;
				}
				// (neighbours: the same side, the same row of slots, the next along her)
				std::vector<const Run*> Sorted;
				for (const Run& R : Weeps)
				{
					Sorted.push_back(&R);
				}
				std::sort(Sorted.begin(), Sorted.end(), [](const Run* A, const Run* B) { return A->Side != B->Side ? A->Side < B->Side : A->X < B->X; });
				auto FlatOf = [&Weeps, &FlatRun](const Run* R) { return static_cast<bool>(FlatRun[static_cast<size_t>(R - Weeps.data())]); };
				size_t Pairs = 0, Alike = 0, FlatAlike = 0;
				for (size_t I = 0; I < Sorted.size(); ++I)
				{
					for (size_t J = I + 1; J < Sorted.size() && Sorted[J]->Side == Sorted[I]->Side && Sorted[J]->X - Sorted[I]->X < 2.0; ++J)
					{
						if (std::abs(Sorted[J]->Top - Sorted[I]->Top) > 0.15)
						{
							continue;
						}
						++Pairs;
						const int32 A = Sorted[I]->Head, B = Sorted[J]->Head;
						bool bSame = true;
						for (const int C : { 0, 1, 2 })
						{
							bSame = bSame && std::abs(LevelAt(A, C) - LevelAt(B, C)) < 3.0f;
						}
						Alike += bSame ? 1 : 0;
						FlatAlike += bSame && FlatOf(Sorted[I]) && FlatOf(Sorted[J]) ? 1 : 0;
						break;
					}
				}
				std::vector<double> Shares = HeadShare;
				std::sort(Shares.begin(), Shares.end());
				const double Middle = Shares.empty() ? 1.0 : Shares[Shares.size() / 2], Palest = Shares.empty() ? 1.0 : Shares.back();
				const std::string Said = std::to_string(Weeps.size()) + " weeps, " + std::to_string(Pairs) + " neighbouring pairs, " + std::to_string(Alike) + " alike at their heads, " +
										 std::to_string(FlatAlike) + " both one flat colour and alike, " + std::to_string(Flat) + " flat; the darkest of a run " + Num(100.0 * Middle, 0) +
										 "% of her paint's brightness at the middle, " + Num(100.0 * Palest, 0) + "% at the palest";
				if (!bOn)
				{
					std::printf("  note  today's weeps on Submarine_GER: %s\n", Said.c_str());
					TodayAlike = Alike;
					TodayFlatAlike = FlatAlike;
					continue;
				}
				Check(Weeps.size() >= 40 && Pairs >= 20, "Submarine_GER's weeps found on her skin, with their neighbours", std::to_string(Weeps.size()) + " weeps, " + std::to_string(Pairs) + " pairs");
				Check(FlatAlike == 0 && TodayFlatAlike > 0, "no two neighbouring weeps share one flat colour (today some do)", Said);
				Check(Flat == 0, "no weep one flat colour: each darkens toward its source", Said);
				// (the rest of "no two neighbours alike" is the models' half: one fitting in three weeps; every slot weeps today)
				Check(Alike * 2 <= TodayAlike, "neighbouring weeps alike even at their heads (within 3 levels) at most half as often as today",
					std::to_string(Alike) + " of " + std::to_string(Pairs) + ", today " + std::to_string(TodayAlike));
				Check(Middle <= 0.5 && Palest <= 0.8, "dark, not salmon: the middle run's source under half her paint's brightness, none paler than 0.8 of it", Said);
			}
		}

		// A wooden deck under the skin (the art director: "the scene holds a wooden deck under 150,135,110"). The skin's
		// first look lays M_MapSurface's blot mottle over what is not metal by up to WoodMottleMost (below, from the graph's
		// own sum), so each plank is held under her cap with room for it: as drawn, never over it, whether the material
		// reads the bytes back by the standard's curve or by its own power of 2.2.
		{
			const ShipPaintCore::Linear Most = ShipPaintCore::DeckWoodMost();
			const float Cap[3] = { ShipPaintCore::DeckR, ShipPaintCore::DeckG, ShipPaintCore::DeckB };
			size_t Planks = 0, Over = 0, OverPower = 0, NotHeld = 0, OthersMoved = 0, PlanksMoved = 0, Mismatched = 0, TodayOver = 0;
			std::map<std::string, size_t> PlanksOf;
			int MostLevel[3] = { 0, 0, 0 };
			for (const ShipFile& Ship : Files)
			{
				for (int Far = 0; Far < 2; ++Far)
				{
					const FRawShip& Raw = Ship.Mesh[Far];
					FRawShip Unheld = Raw;
					std::fill(Unheld.ShipPlanks.begin(), Unheld.ShipPlanks.end(), false);
					const Painted Held = PaintNew(Raw, Skin), Free = PaintNew(Unheld, Skin), Today = PaintOld(Raw);
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						const ShipPaintCore::Point P = PointOf(Raw, K);
						const bool bMoved = std::memcmp(&Held.Linear[4 * K], &Free.Linear[4 * K], 4 * sizeof(float)) != 0;
						if (!P.bDeckWood)
						{
							OthersMoved += bMoved ? 1 : 0;
							continue;
						}
						++Planks;
						PlanksOf[Ship.Name + (Far ? " far" : " near")] += 1;
						PlanksMoved += bMoved ? 1 : 0;
						const bool bWasOver = Free.Linear[4 * K] > Most.R || Free.Linear[4 * K + 1] > Most.G || Free.Linear[4 * K + 2] > Most.B;
						Mismatched += bMoved != bWasOver ? 1 : 0;
						const FColor& Byte = Held.Colours[K];
						const uint8 Level[3] = { Byte.R, Byte.G, Byte.B };
						const float Lin[3] = { Most.R, Most.G, Most.B };
						const FColor& Was = Today.Colours[K];
						TodayOver += Was.R > Cap[0] || Was.G > Cap[1] || Was.B > Cap[2] ? 1 : 0;
						for (int C = 0; C < 3; ++C)
						{
							MostLevel[C] = std::max<int>(MostLevel[C], Level[C]);
							NotHeld += Held.Linear[4 * K + C] > Lin[C] ? 1 : 0;
							Over += PaintShim::SrgbToLinear(Level[C]) * ShipPaintCore::WoodMottleMost > ShipPaintCore::ToLinear(Cap[C] / 255.0f) ? 1 : 0;
							OverPower += std::pow(Level[C] / 255.0, 2.2) * ShipPaintCore::WoodMottleMost > std::pow(Cap[C] / 255.0, 2.2) ? 1 : 0;
						}
					}
				}
			}
			const std::string Counted = std::to_string(Planks) + " planks (Battleship_USA " + std::to_string(PlanksOf["Battleship_USA near"]) + ", Carrier_USA " +
										std::to_string(PlanksOf["Carrier_USA near"]) + ", Carrier_JAP " + std::to_string(PlanksOf["Carrier_JAP near"]) + " near), " +
										std::to_string(TodayOver) + " over the cap today";
			Check(Planks > 5000 && PlanksOf["Battleship_USA near"] == 686 && PlanksOf["Carrier_USA near"] == 67 && PlanksOf["Carrier_JAP near"] == 67 && TodayOver * 10 >= Planks * 9,
				"the wooden decks found in the files, nearly every plank over the art director's cap today", Counted);
			Check(NotHeld == 0 && Over == 0 && OverPower == 0,
				"under the skin a plank times the mottle's most stays under 150,135,110, read back either way", "the brightest plank " + std::to_string(MostLevel[0]) + "," +
					std::to_string(MostLevel[1]) + "," + std::to_string(MostLevel[2]) + " (times " + Num(ShipPaintCore::WoodMottleMost, 2) + "), " + std::to_string(Over + OverPower + NotHeld) + " over");
			Check(OthersMoved == 0 && Mismatched == 0 && PlanksMoved > 0, "the hold moves the planks over the cap and nothing else",
				std::to_string(PlanksMoved) + " planks moved, " + std::to_string(Planks - PlanksMoved) + " already under it, " + std::to_string(OthersMoved) + " others");
		}

		// The sea's steel ink, worked through M_MapSurface's own sums (Tools/build_content.py, build_map_surface, read here so
		// the sums below are the graph's). The graph reads the Wear in three places and the TopLight in one: the lift on what
		// faces the sky, 1 + up x TopLight x (metal x Wear); the blot mottle, the colour times lerp(1, 0.8 + 0.42 blot, Wear x
		// (metal/2 + 1/2)); and a chip where (speck x (blot + 0.5) + ChipAt + gather x ChipGather) x ChipSharp passes 0, times
		// metal and Wear, drawn in ChipColour, as metal (0.85 of the chip) and smoother (the roughness taken toward 0.34 by it).
		// Metal is the point's own finish, saturate(2 x (1 - its alpha)), less where the alpha marks water (240), and the chips
		// gather where the alpha falls below a half. The emissive (a figure's fill and rim) is the colour after these sums
		// times FigureFill and FigureSun's alpha, both 0 as made and never set on the steel ink, and nothing on metal.
		{
			const std::string Build = ReadAll(Src + "/../../Tools/build_content.py");
			const size_t From = Build.find("def build_map_surface("), To = Build.find("\ndef build_solid(");
			const std::string Graph = From != std::string::npos && To != std::string::npos && To > From ? Build.substr(From, To - From) : std::string();
			auto Times = [](const std::string& In, const std::string& What)
			{
				size_t N = 0;
				for (size_t At = In.find(What); At != std::string::npos; At = In.find(What, At + 1))
				{
					++N;
				}
				return N;
			};
			auto Words = [](const std::string& In, const std::string& Word)
			{
				size_t N = 0;
				for (size_t At = In.find(Word); At != std::string::npos; At = In.find(Word, At + 1))
				{
					const char Before = At == 0 ? ' ' : In[At - 1], After = At + Word.size() < In.size() ? In[At + Word.size()] : ' ';
					N += std::isalnum(static_cast<unsigned char>(Before)) || Before == '_' || std::isalnum(static_cast<unsigned char>(After)) || After == '_' ? 0 : 1;
				}
				return N;
			};
			bool bSums = !Graph.empty();
			for (const char* Piece : { "wear = scalar(m, \"Wear\", 0.0, ", "scalar(m, \"TopLight\", 0.26, ",
					 "lit_top = plus(m, 1.0, times(m, times(m, one_in(m, unreal.MaterialExpressionSaturate, skyward, ", "times(m, metal, wear, -600, -2150), -450, -2100), -300, -2100)",
					 "lerp(m, 1.0, plus(m, times(m, (blot, \"R\"), 0.42, -350, -1500), 0.8, -200, -1500),",
					 "times(m, wear, plus(m, times(m, metal, 0.5, 100, -1450), 0.5, 200, -1450), 300, -1450), 450, -1500), 600, -900), lit_top, 700, -1000)",
					 "gather = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, times(m, (colour, \"A\"), -1.0, -900, -1950), 0.5, -750, -1950), 2.56, -600, -1950), -450, -1950)",
					 "times(m, (speck, \"R\"), plus(m, (blot, \"G\"), 0.5, -350, -1650), -200, -1700), scalar(m, \"ChipAt\", -0.78, ",
					 "times(m, gather, scalar(m, \"ChipGather\", 0.3, ", "scalar(m, \"ChipSharp\", 9.0, ", "metal, 450, -1750), wear, 600, -1750)",
					 "metal = times(m, one_in(m, unreal.MaterialExpressionSaturate, times(m, one_in(m, unreal.MaterialExpressionOneMinus, (colour, \"A\"), -900, -1500), 2.0, -750, -1500), -600, -1500),",
					 "one_in(m, unreal.MaterialExpressionOneMinus, on_water, -600, -1420), -500, -1480)",
					 "plus(m, times(m, one_in(m, unreal.MaterialExpressionAbs, plus(m, (colour, \"A\"), -0.9412, -1100, -1380), -1000, -1380), -60.0, -900, -1380), 1.0, -800, -1380)",
					 "tinted = lerp(m, weathered, vector(m, \"ChipColour\", (0.13, 0.125, 0.12, 1.0), 450, -1300), chipped, 750, -900)",
					 "to_property(times(m, chipped, 0.85, 750, -700), \"\", unreal.MaterialProperty.MP_METALLIC)",
					 "to_property(lerp(m, scalar(m, \"Roughness\", 0.9, -450, 200), 0.34, chipped, -250, 200), \"\", unreal.MaterialProperty.MP_ROUGHNESS)",
					 "scalar(m, \"FigureFill\", 0.0, ", "figure_sun = vector(m, \"FigureSun\", (0.0, 0.0, 1.0, 0.0), ",
					 "cloth = times(m, one_in(m, unreal.MaterialExpressionOneMinus, metal, ", "scalar(m, \"BlotTiling\", 0.009, " })
			{
				bSums = bSums && Has(Graph, Piece);
			}
			// (and nothing else in the graph reads them: the wear made once and read three times, the TopLight and the lift once)
			const size_t WearWords = Words(Graph, "wear"), TopLights = Times(Graph, "\"TopLight\""), LitTops = Words(Graph, "lit_top");
			Check(bSums && WearWords == 4 && TopLights == 1 && LitTops == 2 && ShipPaintCore::MapTopLight == 0.26f && ShipPaintCore::MottleMost == 0.8f + 0.42f,
				"M_MapSurface's lift, mottle, chip, metal and roughness as this test works them, and nothing else reading the Wear or the TopLight (Tools/build_content.py; as made TopLight 0.26, Wear 0, ChipAt -0.78)",
				"wear read " + std::to_string(WearWords - std::min<size_t>(WearWords, 1)) + " times, TopLight " + std::to_string(TopLights));
			const auto Saturate = [](float V) { return std::min(1.0f, std::max(0.0f, V)); };
			// (a point's finish as the graph reads its alpha: how much metal, and how much the chips gather)
			const auto MetalOf = [&Saturate](int Alpha)
			{
				const float A = static_cast<float>(Alpha) / 255.0f;
				const float OnWater = Saturate(std::abs(A + -0.9412f) * -60.0f + 1.0f);
				return Saturate((1.0f - A) * 2.0f) * (1.0f - OnWater);
			};
			const auto GatherOf = [&Saturate](int Alpha) { return Saturate((static_cast<float>(Alpha) / 255.0f * -1.0f + 0.5f) * 2.56f); };
			const auto Lift = [&Saturate](float Up, float Metal, float TopLight, float Wear) { return 1.0f + Saturate(Up) * TopLight * (Metal * Wear); };
			const auto Mottle = [](float Metal, float Wear, float Blot)
			{
				const float Share = Wear * (Metal * 0.5f + 0.5f);
				return 1.0f + (0.8f + 0.42f * Blot - 1.0f) * Share;
			};
			const auto Chip = [&Saturate](float Speck, float Blot, float Gather, float ChipAt, float Metal, float Wear)
			{
				return Saturate((Speck * (Blot + 0.5f) + ChipAt + Gather * 0.3f) * 9.0f) * Metal * Wear;
			};
			// (the scene's switches reach the ink only through SteelOf, in SetShipPaint, each parameter behind its own mark)
			const std::string Stage = ReadAll(Src + "/Battle/AirStage.cpp"), Scene = ReadAll(Src + "/Battle/SeaBattle.cpp");
			const size_t SetAt = Stage.find("void AAirStage::SetShipPaint(");
			const std::string SetBody = SetAt == std::string::npos ? std::string() : Stage.substr(SetAt, Stage.find("\n}\n", SetAt) - SetAt);
			Check(Has(Scene, "Paint.bShade = bShipLight;") && Has(Scene, "Paint.TopLight = bShipLight ? ShipLight.Value : -1.0f;") &&
					  Has(Scene, "Stage->SetShipPaint(Paint, bShipSkin ? ShipPaintCore::SeaFirstLookWear : 0.0f);") &&
					  Has(SetBody, "const ShipPaintCore::Steel SteelSet = ShipPaintCore::SteelOf(Given, FirstLookWear);") &&
					  Has(SetBody, "if (SteelSet.TopLight >= 0.0f)\n\t{\n\t\tSteelInk->SetScalarParameterValue(TEXT(\"TopLight\"), SteelSet.TopLight);\n\t}") &&
					  Has(SetBody, "if (SteelSet.Wear > 0.0f)\n\t{\n\t\tSteelInk->SetScalarParameterValue(TEXT(\"Wear\"), SteelSet.Wear);\n\t}") &&
					  Has(SetBody, "if (SteelSet.bBlots)\n\t{\n\t\tSteelInk->SetScalarParameterValue(TEXT(\"BlotTiling\"), ") &&
					  Has(SetBody, "if (SteelSet.bChipsOff)\n\t{\n\t\tSteelInk->SetScalarParameterValue(TEXT(\"ChipAt\"), ShipPaintCore::SeaChipAt);\n\t}") &&
					  Times(SetBody, "SetScalarParameterValue") == 4 && !Has(SetBody, "SetVectorParameterValue") && Times(Stage, "SteelInk->Set") == Times(SetBody, "SteelInk->Set"),
				"the scene's switches reach the steel ink only through SteelOf, each of TopLight, Wear, BlotTiling and ChipAt behind its own mark, and nothing else sets it",
				std::to_string(Times(SetBody, "SetScalarParameterValue")) + " set, " + std::to_string(Times(Stage, "SteelInk->Set")) + " in AirStage.cpp");
			// (the ink as SetShipPaint leaves it: what SteelOf marks to set, else as made)
			struct Ink
			{
				float TopLight = ShipPaintCore::MapTopLight, Wear = 0.0f, ChipAt = -0.78f;
				bool bBlots = false;
				int Set = 0;
			};
			const auto InkOf = [](const ShipPaintCore::Steel& Steel)
			{
				Ink Out;
				Out.TopLight = Steel.TopLight >= 0.0f ? Steel.TopLight : Out.TopLight;
				Out.Wear = Steel.Wear > 0.0f ? Steel.Wear : Out.Wear;
				Out.bBlots = Steel.bBlots;
				Out.ChipAt = Steel.bChipsOff ? ShipPaintCore::SeaChipAt : Out.ChipAt;
				Out.Set = (Steel.TopLight >= 0.0f ? 1 : 0) + (Steel.Wear > 0.0f ? 1 : 0) + (Steel.bBlots ? 1 : 0) + (Steel.bChipsOff ? 1 : 0);
				return Out;
			};
			// (a steel point's finish as the reader marks it: 128 less half its wear, the wear held to 0 to 100, so 128 unworn
			// (wear 0 or 1), which the graph reads as 254/255 metal, to 78 at the most wear, all metal; a plank, 255: none)
			const std::string Reader = ReadAll(Src + "/Campaign/CampaignMiniatures.cpp");
			const bool bReaderFinish = Has(Reader, "const int32 Worn = Raw->Wear.IsValidIndex(K) ? FMath::Clamp(Raw->Wear[K], 0, 100) : 0;\n\t\t\t\tFresh.Finish = bShipSteel ? static_cast<uint8>(128 - Worn / 2) : 255;");
			const int SteelUnworn = 128, SteelMostWorn = 128 - 100 / 2;
			const float Plank = MetalOf(255), Roughness = 0.55f;
			std::string Lifts, Params;
			bool bDeckLift = true, bPlank = true, bAlike = true, bNoChips = true, bAsMade = true, bOff = false, bSets = true;
			float AloneMottle = 0.0f, SkinMottle = 0.0f;
			for (const float Light : { 0.0f, 0.08f, ShipPaintCore::SeaTopLight, 0.12f, ShipPaintCore::MapTopLight })
			{
				float DeckLift[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
				Ink Inks[2];
				for (const bool bSkin : { false, true })
				{
					ShipPaintCore::Options O;
					O.bShade = Light > 0.0f;
					O.TopLight = Light > 0.0f ? Light : -1.0f;
					O.bRustRamp = bSkin;
					const ShipPaintCore::Steel Steel = ShipPaintCore::SteelOf(O, bSkin ? ShipPaintCore::SeaFirstLookWear : 0.0f);
					const Ink Made = InkOf(Steel);
					Inks[bSkin ? 1 : 0] = Made;
					if (Light <= 0.0f && !bSkin)
					{
						// (both switches off: nothing set at all, the ink as made)
						bOff = Made.Set == 0 && Steel.TopLight < 0.0f && Steel.Wear == 0.0f && !Steel.bChipsOff && !Steel.bBlots;
					}
					else if (bSkin)
					{
						// (the skin: its own Wear, its blots, no chips, and the top light the light asked, none if not)
						bSets = bSets && Made.Set == 4 && Steel.Wear == ShipPaintCore::SeaFirstLookWear && Steel.TopLight == Light && Steel.bBlots && Steel.bChipsOff;
					}
					else
					{
						// (the light alone: a Wear, the TopLight over it, no chips, and nothing else)
						bSets = bSets && Made.Set == 3 && Steel.Wear > 0.0f && Steel.TopLight == Light / Steel.Wear && !Steel.bBlots && Steel.bChipsOff;
					}
					// (a steel deck facing the sky at every finish the reader gives steel, unworn (128, 254/255 metal) to the most
					// worn (78, all metal): lifted by 1 + T x its metal, as the map's sum gives the light at a Wear of 1, so
					// 254/255 of the light where unworn and the light itself where worn; a plank, alpha 255: never)
					for (int Alpha = SteelMostWorn; Alpha <= SteelUnworn; ++Alpha)
					{
						const float Lifted = Lift(1.0f, MetalOf(Alpha), Made.TopLight, Made.Wear);
						bDeckLift = bDeckLift && std::abs(Lifted - (1.0f + Light * MetalOf(Alpha))) < 1e-6f;
						DeckLift[bSkin ? 1 : 0][0] = Alpha == SteelUnworn ? Lifted : DeckLift[bSkin ? 1 : 0][0];
						DeckLift[bSkin ? 1 : 0][1] = Alpha == SteelUnworn - 1 ? Lifted : DeckLift[bSkin ? 1 : 0][1];
					}
					bPlank = bPlank && Lift(1.0f, Plank, Made.TopLight, Made.Wear) == 1.0f;
					// (no chip anywhere, at every finish and every corner and step of the noise: the metal stays 0, the roughness as made)
					for (int Alpha = 0; Alpha <= 255; ++Alpha)
					{
						const float Metal = MetalOf(Alpha), Gather = GatherOf(Alpha);
						for (int S = 0; S <= 32; ++S)
						{
							for (int B = 0; B <= 32; ++B)
							{
								const float Chipped = Chip(S / 32.0f, B / 32.0f, Gather, Made.ChipAt, Metal, Made.Wear);
								const float Metallic = Chipped * 0.85f, Rough = Roughness + Chipped * (0.34f - Roughness);
								bNoChips = bNoChips && Chipped == 0.0f && Metallic == 0.0f && Rough == Roughness;
							}
						}
						// (the mottle's most, every blot from 0 to 1: with the light alone, no more than a sliver)
						for (int B = 0; B <= 256; ++B)
						{
							const float Change = std::abs(Mottle(Metal, Made.Wear, B / 256.0f) - 1.0f);
							AloneMottle = !bSkin && Light > 0.0f ? std::max(AloneMottle, Change) : AloneMottle;
						}
					}
					SkinMottle = bSkin ? std::max(SkinMottle, Mottle(0.0f, Made.Wear, 1.0f)) : SkinMottle;
					bAsMade = bAsMade && (Light > 0.0f || bSkin || (Made.TopLight == ShipPaintCore::MapTopLight && Made.Wear == 0.0f && Made.ChipAt == -0.78f));
				}
				// (the light alone lifts every finish, every way a face can turn, exactly as the skin with the same light does)
				if (Light > 0.0f)
				{
					for (int Alpha = 0; Alpha <= 255; ++Alpha)
					{
						for (const float Up : { -1.0f, -0.5f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
						{
							const float Alone = Lift(Up, MetalOf(Alpha), Inks[0].TopLight, Inks[0].Wear), Skinned = Lift(Up, MetalOf(Alpha), Inks[1].TopLight, Inks[1].Wear);
							bAlike = bAlike && std::abs(Alone - Skinned) < 1e-6f;
						}
					}
				}
				Lifts += std::string(Lifts.empty() ? "" : "; ") + "light " + (Light > 0.0f ? Num(Light, 2) : std::string("off")) + ": alone " + Num(DeckLift[0][0], 4) + " unworn, " +
						 Num(DeckLift[0][1], 4) + " worn, with the skin " + Num(DeckLift[1][0], 4) + ", " + Num(DeckLift[1][1], 4);
				Params += std::string(Params.empty() ? "" : "; ") + (Light > 0.0f ? Num(Light, 2) : std::string("off")) + " alone " +
						  (Inks[0].Set == 0 ? std::string("nothing set") : "Wear " + Num(Inks[0].Wear, 2) + " TopLight " + Num(Inks[0].TopLight, 2)) + ", skin Wear " + Num(Inks[1].Wear, 2) +
						  " TopLight " + Num(Inks[1].TopLight, 2);
			}
			Check(bOff && bAsMade, "with both switches off nothing is set on the steel ink: as made, TopLight 0.26 with no Wear to lift by, so no lift, no mottle, no chips", "nothing set");
			Check(bSets, "the light alone sets Wear, TopLight (the light over the Wear) and ChipAt, nothing else; the skin its own Wear 1.0, the light itself (0 if off), BlotTiling and ChipAt",
				Params);
			Check(bDeckLift && bReaderFinish && std::abs(MetalOf(SteelUnworn) - 254.0f / 255.0f) < 1e-6f && MetalOf(SteelUnworn - 1) == 1.0f && MetalOf(SteelMostWorn) == 1.0f,
				"a steel deck facing the sky is lifted by 1 + T x its metal within 0.000001 at every finish the reader gives steel (128 unworn, 254/255 metal; 127 to 78 worn, all "
				"metal), for 0.08, 0.10, 0.12 and 0.26, with or without the skin, and by nothing with the light off",
				Lifts);
			Check(bAlike, "the light alone lifts every finish (alpha 0 to 255), facing every way, exactly as the skin with the same light does", "within 0.000001");
			Check(bPlank && Plank == 0.0f, "a plank (not metal, alpha 255) is never lifted, whatever the light", "metal " + Num(Plank, 1));
			Check(AloneMottle > 0.0f && AloneMottle < 0.005f, "with the light alone the blot mottle moves a colour by under 0.5 in 100 anywhere (every finish, every blot)",
				"at most " + Num(AloneMottle * 100.0f, 3) + " in 100");
			Check(bNoChips && Chip(1.0f, 1.0f, 1.0f, -0.78f, 1.0f, 1.0f) > 0.0f,
				"no chip anywhere at sea under any switch, so the metal stays 0 and the roughness as made (the material's own chips, as made, would show)", "ChipAt " + Num(ShipPaintCore::SeaChipAt, 1));
			Check(std::abs(SkinMottle - ShipPaintCore::WoodMottleMost) < 1e-6f && std::abs(ShipPaintCore::WoodMottleMost - 1.11f) < 1e-6f,
				"the mottle's most on what is not metal under the skin, which the deck's hold leaves room for", Num(SkinMottle, 3));
		}
	}

	std::printf("(d) A flipped default is caught\n");
	{
		size_t Meshes = 0, Caught = 0, ShadeCaught = 0;
		ShipPaintCore::Options Flipped;
		Flipped.bRustRamp = true;
		ShipPaintCore::Options Shaded;
		Shaded.bShade = true;
		for (const ShipFile& Ship : Files)
		{
			for (int Far = 0; Far < 2; ++Far)
			{
				FRawShip Raw = Ship.Mesh[Far];
				++Meshes;
				Caught += Differ(PaintOld(Raw), PaintNew(Raw, Flipped)) > 0 ? 1 : 0;
				GiveShade(Raw);
				ShadeCaught += Differ(PaintOld(Raw), PaintNew(Raw, Shaded)) > 0 ? 1 : 0;
			}
		}
		Check(Caught == Meshes, "the rust ramp on by default would fail the identity check on every mesh", std::to_string(Caught) + " of " + std::to_string(Meshes));
		Check(ShadeCaught == Meshes, "the shade painted with the switches off (on by default, or its gate lost) would fail the identity check's shaded leg on every mesh",
			std::to_string(ShadeCaught) + " of " + std::to_string(Meshes));
	}

	std::printf("(e) Her correction of 25 November 20:28Z: light wear is not rust\n");
	{
		ShipPaintCore::Options Skin;
		Skin.bRustRamp = true;
		const double BodyR = ShipPaintCore::Body.R, BodyG = ShipPaintCore::Body.G, BodyB = ShipPaintCore::Body.B;

		// Her own up axis, the "above" of rule 3: the file's +Z, which the reader keeps (it turns x and y about z, never z).
		// Found from the file itself: on every mesh her keel, the lowest point, lies her draught below z 0 (the waterline),
		// her deck paint faces +Z, and no file has loads, so the reader keeps every triangle the patches are joined by.
		{
			size_t Meshes = 0, Up = 0;
			double LeastDeck = 1.0, MostKeel = 0.0;
			for (const ShipFile& Ship : Files)
			{
				const auto Deck = std::find(Ship.PaintNames.begin(), Ship.PaintNames.end(), "deck");
				for (int Far = 0; Far < 2; ++Far)
				{
					const FRawShip& Raw = Ship.Mesh[Far];
					double Lowest = 1e9, Facing = 0.0;
					size_t Decks = 0;
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						Lowest = std::min(Lowest, Raw.Points[K].Z);
						if (Deck != Ship.PaintNames.end() && Raw.Paints[K] == static_cast<int32>(Deck - Ship.PaintNames.begin()))
						{
							const FVector& F = Raw.Facings[K];
							const double Long = std::sqrt(F.X * F.X + F.Y * F.Y + F.Z * F.Z);
							Facing += Long > 0.0 ? F.Z / Long : 0.0;
							++Decks;
						}
					}
					const double DeckUp = Decks ? Facing / static_cast<double>(Decks) : -1.0, Keel = std::abs(Lowest + Ship.Draught);
					LeastDeck = std::min(LeastDeck, DeckUp);
					MostKeel = std::max(MostKeel, Keel);
					++Meshes;
					Up += Ship.Draught > 0.0 && Keel <= 0.1 && DeckUp >= 0.75 && !Ship.bLoads ? 1 : 0;
				}
			}
			Check(Up == Meshes && Meshes == 52,
				"her own up axis is her file's +Z, kept by the reader: on every mesh her keel lies her draught below z 0 and her deck faces +Z (so a run's body \"above\" a point is higher in Z); no file has loads",
				std::to_string(Up) + " of " + std::to_string(Meshes) + " meshes; keel within " + Num(MostKeel, 2) + " m of her draught, deck facing up by " + Num(LeastDeck, 2) + " at the least");
		}

		// The reader's run marks are this test's own (RunsOf, worked apart from the core), on every mesh; none are made with the
		// skin off; and the branch paints each point exactly as the core does with those marks.
		size_t Stained = 0, Bodies = 0, Feet = 0, Light = 0, LightAtFeet = 0;
		{
			size_t Meshes = 0, Agree = 0, Unlike = 0;
			std::string Disagree;
			for (const ShipFile& Ship : Files)
			{
				for (int Far = 0; Far < 2; ++Far)
				{
					const FRawShip& Raw = Ship.Mesh[Far];
					const std::vector<uint8_t>& Mine = MarksOf(Ship, Far);
					const std::vector<uint8_t> Reader = ReaderRuns(Raw, Skin), Off = ReaderRuns(Raw, ShipPaintCore::Options());
					const bool bAgree = Reader == Mine && Off.empty();
					Agree += bAgree ? 1 : 0;
					Disagree += bAgree ? "" : (Disagree.empty() ? "" : ", ") + Ship.Name + (Far ? " far" : " near");
					++Meshes;
					const ShipPaintCore::Palette Pal = PaletteOf(Raw);
					const Painted Now = PaintNew(Raw, Skin);
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						const ShipPaintCore::Point P = PointOf(Raw, K, &Mine);
						const ShipPaintCore::Linear C = ShipPaintCore::Paint(P, Pal, Skin);
						const float Four[4] = { C.R, C.G, C.B, C.A };
						Unlike += std::memcmp(Four, &Now.Linear[4 * static_cast<size_t>(K)], sizeof(Four)) == 0 ? 0 : 1;
						if (!P.bPainted || P.Wear <= 0)
						{
							continue;
						}
						++Stained;
						Bodies += P.Wear >= ShipPaintCore::RunBodyWear ? 1 : 0;
						Feet += P.bBodyAbove ? 1 : 0;
						Light += P.Wear < ShipPaintCore::RunBodyWear && !P.bBodyAbove ? 1 : 0;
						LightAtFeet += P.Wear < ShipPaintCore::RunBodyWear && P.bBodyAbove ? 1 : 0;
					}
				}
			}
			Check(Agree == Meshes && Unlike == 0 && ShipPaintCore::RunBodyWear == 20 && ShipPaintCore::RunLongestMm == 3000 && ShipPaintCore::RunFootWideMm == 450 &&
					  ShipPaintCore::RunRakePer10000 == 1763,
				"the reader marks the foot of a run with body above it as this test's own finder does (20 of wear and more its body; 1 mm to 3 m higher in Z, at 45 degrees or steeper, 22.5 cm aside plus tan 10 degrees of the drop at most, on the same face), on every mesh near and far, none with the skin off; the branch paints each point as the core does with them",
				std::to_string(Agree) + " of " + std::to_string(Meshes) + " meshes agree" + (Disagree.empty() ? "" : " (not " + Disagree + ")") + ", " + std::to_string(Unlike) + " points painted otherwise");
			Check(Light > 50000 && Feet > 1000 && LightAtFeet > 500 && Bodies > 1000,
				"the files carry light wear away from any run, and runs' feet with their body above",
				std::to_string(Stained) + " stained points: " + std::to_string(Light) + " light wear away from a run, " + std::to_string(Feet) + " at a run's foot (" +
					std::to_string(LightAtFeet) + " of them under 20), " + std::to_string(Bodies) + " of 20 and more");
		}

		// Rule 1: no channel of a stained point (painted and worn) above the paint's own. Every point of every mesh as the branch
		// paints it under the skin. Salt is a deposit and may be paler (her 16:05Z sheet): where her salt lies (196,200,198 at
		// 30% at most, written here and pinned in the core) a point may rise as far as that salt takes the clean paint, but a
		// stain only in every channel at once: below the paint in any channel, above it in none, so never red above the paint
		// with blue below it (the pink of round 1, and of round 1c where salt lay on rust); rust the salt lifts above the paint
		// no warmer (red less blue, in levels) than her salt makes the clean paint, light wear no warmer than rule 2 allows
		// (its own check). An unworn point: its paint, or its salt over the clean paint, at most.
		{
			size_t Points = 0, StainedPoints = 0, Salted = 0, Above = 0, Mixed = 0, Pink = 0, Paler = 0, Under = 0, RustPaler = 0, Warmer = 0;
			double MostAbove = 0.0, MostPaler = 0.0, MostWarmer = -1e9;
			const ShipPaintCore::Linear Pale = { ShipPaintCore::ToLinear(196.0f / 255.0f), ShipPaintCore::ToLinear(200.0f / 255.0f), ShipPaintCore::ToLinear(198.0f / 255.0f), 1.0f };
			for (const ShipFile& Ship : Files)
			{
				for (int Far = 0; Far < 2; ++Far)
				{
					const FRawShip& Raw = Ship.Mesh[Far];
					const Painted Now = PaintNew(Raw, Skin);
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						const ShipPaintCore::Point P = PointOf(Raw, K, &MarksOf(Ship, Far));
						const bool bSalted = P.bPainted && P.Salt > 0;
						const bool bStained = P.bPainted && P.Wear > 0;
						const float Own[3] = { P.Paint.R, P.Paint.G, P.Paint.B };
						float Most[3] = { P.Paint.R, P.Paint.G, P.Paint.B };
						if (bSalted)
						{
							// (her salt over the clean paint, as FMath::Lerp works it: 0.30 of the salt's share toward 196,200,198)
							const float Share = 0.30f * static_cast<float>(std::min(P.Salt, 100)) / 100.0f;
							const float Toward[3] = { Pale.R, Pale.G, Pale.B };
							for (int C = 0; C < 3; ++C)
							{
								const float Apart = Toward[C] - Own[C];
								const float Part = Apart * Share;
								Most[C] = std::max(Most[C], Own[C] + Part);
							}
						}
						bool bAbove = false, bUp = false, bDown = false;
						double Least = 1e9;
						for (int C = 0; C < 3; ++C)
						{
							const float Got = Now.Linear[4 * static_cast<size_t>(K) + static_cast<size_t>(C)];
							bAbove = bAbove || Got > Most[C];
							bUp = bUp || Got > Own[C];
							bDown = bDown || Got < Own[C];
							MostAbove = std::max<double>(MostAbove, Level(Got) - Level(Most[C]));
							Least = std::min<double>(Least, Level(Got) - Level(Own[C]));
						}
						++Points;
						Above += bAbove ? 1 : 0;
						if (!bStained)
						{
							continue;
						}
						++StainedPoints;
						Salted += bSalted ? 1 : 0;
						Mixed += bUp && bDown ? 1 : 0;
						Pink += Now.Linear[4 * static_cast<size_t>(K)] > Own[0] && Now.Linear[4 * static_cast<size_t>(K) + 2] < Own[2] ? 1 : 0;
						Paler += bUp && !bDown ? 1 : 0;
						Under += !bUp ? 1 : 0;
						MostPaler = bUp && !bDown ? std::max(MostPaler, Least) : MostPaler;
						if (bUp && !bDown && (P.Wear >= 20 || P.bBodyAbove))
						{
							++RustPaler;
							const size_t At = 4 * static_cast<size_t>(K);
							const double Warm = (Level(Now.Linear[At]) - Level(Now.Linear[At + 2])) - (Level(Most[0]) - Level(Most[2]));
							MostWarmer = std::max(MostWarmer, Warm);
							Warmer += Warm > 0.001 ? 1 : 0;
						}
					}
				}
			}
			const bool bSaltRuled = ShipPaintCore::SaltR == 196.0f && ShipPaintCore::SaltG == 200.0f && ShipPaintCore::SaltB == 198.0f && ShipPaintCore::SaltMost == 0.30f;
			Check(Above == 0 && Mixed == 0 && Pink == 0 && Warmer == 0 && bSaltRuled && StainedPoints > 100000 && Salted > 1000,
				"rule 1: no channel of a stained point above the paint's own, but where her salt (196,200,198 at 30%, pinned) pales it in every channel at once, no further than that salt over the clean paint (and rust no warmer than it); never red above the paint with blue below it; every point of every mesh, near and far",
				std::to_string(StainedPoints) + " stained points (" + std::to_string(Salted) + " salted) of " + std::to_string(Points) + ": " + std::to_string(Under) + " at or under the paint, " +
					std::to_string(Paler) + " paler in every channel under the salt (by " + Num(MostPaler, 1) + " levels at most in the least), " + std::to_string(Mixed) +
					" above in one channel and below in another, " + std::to_string(Pink) + " red above with blue below; of the paler, " + std::to_string(RustPaler) + " rust, " +
					std::to_string(Warmer) + " warmer than the salted clean paint (red less blue " + (RustPaler ? Num(MostWarmer, 2) : std::string("-")) + " against it at most); " +
					std::to_string(Above) + " points above the paint or its salt, by at most " + Num(MostAbove, 3) + " levels");
		}

		// Rule 2: wear under 20 away from any run takes no rust colour: 3 to 6% darker, 0 to 15% greyer, red minus blue up by 3
		// at most, in sRGB levels as she measures (brightness the Rec. 709 sum of the levels; colour the most channel less the
		// least, for the brightness). Held in the mesh's own bytes, what the game draws, against the same point unworn (the
		// branch's bytes with no wear: the paint, with her salt and the soot where the file lays them), and never above it.
		{
			size_t Count = 0, Bad = 0, WithSalt = 0, WithSoot = 0, OverUnworn = 0, Plain = 0;
			double LeastDark = 1.0, MostDark = 0.0, LeastGrey = 1.0, MostGrey = -1.0;
			int MostWarm = -1000, MostWarmPlain = -1000;
			for (const ShipFile& Ship : Files)
			{
				for (int Far = 0; Far < 2; ++Far)
				{
					const FRawShip& Raw = Ship.Mesh[Far];
					const ShipPaintCore::Palette Pal = PaletteOf(Raw);
					const Painted Now = PaintNew(Raw, Skin);
					FRawShip Bare = Raw;
					std::fill(Bare.Wear.begin(), Bare.Wear.end(), 0);
					const Painted Unworn = PaintNew(Bare, Skin);
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						const ShipPaintCore::Point P = PointOf(Raw, K, &MarksOf(Ship, Far));
						if (!P.bPainted || P.Wear <= 0 || P.Wear >= 20 || P.bBodyAbove)
						{
							continue;
						}
						++Count;
						WithSalt += P.Salt > 0 ? 1 : 0;
						WithSoot += P.Soot > 4 && Pal.bSoot ? 1 : 0;
						const bool bPlain = P.Salt <= 0 && !(P.Soot > 4 && Pal.bSoot);
						Plain += bPlain ? 1 : 0;
						const size_t At = static_cast<size_t>(K);
						const FColor &Byte = Now.Colours[At], &Was = Unworn.Colours[At];
						const Levels Got = { static_cast<double>(Byte.R), static_cast<double>(Byte.G), static_cast<double>(Byte.B) };
						const Levels Before = { static_cast<double>(Was.R), static_cast<double>(Was.G), static_cast<double>(Was.B) };
						const double Dark = 1.0 - Got.Bright() / Before.Bright();
						const double Grey = Before.Chroma() > 0.0 ? 1.0 - (Got.Chroma() / Got.Bright()) / (Before.Chroma() / Before.Bright()) : Got.Chroma() > 0.0 ? -1.0 : 0.0;
						const int Warm = (static_cast<int>(Byte.R) - static_cast<int>(Byte.B)) - (static_cast<int>(Was.R) - static_cast<int>(Was.B));
						bool bOver = Byte.R > Was.R || Byte.G > Was.G || Byte.B > Was.B;
						for (int C = 0; C < 3; ++C)
						{
							bOver = bOver || Now.Linear[4 * At + static_cast<size_t>(C)] > Unworn.Linear[4 * At + static_cast<size_t>(C)];
						}
						OverUnworn += bOver ? 1 : 0;
						LeastDark = std::min(LeastDark, Dark);
						MostDark = std::max(MostDark, Dark);
						LeastGrey = std::min(LeastGrey, Grey);
						MostGrey = std::max(MostGrey, Grey);
						MostWarm = std::max(MostWarm, Warm);
						MostWarmPlain = bPlain ? std::max(MostWarmPlain, Warm) : MostWarmPlain;
						Bad += Dark < 0.03 || Dark > 0.06 || Grey < 0.0 || Grey > 0.15 || Warm > 3 || bOver ? 1 : 0;
					}
				}
			}
			Check(Bad == 0 && Count > 50000 && WithSalt > 1000 && WithSoot > 1000 && ShipPaintCore::LightDarkLeast == 0.03f && ShipPaintCore::LightDarkMost == 0.06f &&
					  ShipPaintCore::LightGreyMost == 0.15f && ShipPaintCore::LightWarmMost == 3.0f,
				"rule 2: every point with wear under 20 and no run takes no rust colour: in the mesh's bytes against the same point unworn (salt and soot laid), 3 to 6% darker, 0 to 15% greyer, red minus blue up by 3 at most, never above it",
				std::to_string(Count) + " points (" + std::to_string(Plain) + " plain, " + std::to_string(WithSalt) + " with salt, " + std::to_string(WithSoot) + " with soot), " + std::to_string(Bad) +
					" out: darker " + Num(100.0 * LeastDark, 2) + " to " + Num(100.0 * MostDark, 2) + "%, greyer " + Num(100.0 * LeastGrey, 2) + " to " + Num(100.0 * MostGrey, 2) +
					"%, red less blue up by " + std::to_string(MostWarm) + " at most (" + std::to_string(MostWarmPlain) + " where plain), " + std::to_string(OverUnworn) + " above the point unworn");
		}

		// Rule 3: the film 124,80,50 only at the foot of a run with body above it. Every point the ramp paints under the skin
		// (wear of 20 and more, or a run's foot) that has no body above it, by this test's own finder, takes no share of the
		// foot's colour, and the ramp's colour there is no lighter than the body's 96,58,38 in any channel; every point that
		// takes the film as the game paints it (by the reader's own marks) has a body point of its own run above it at 45
		// degrees or steeper, within half the foot's width plus the rake and on its own face; light wear with no body above
		// never reaches the ramp (rule 2's own check).
		{
			size_t Ramped = 0, NoBody = 0, FootLaid = 0, Lighter = 0, WithFilm = 0, Shallow = 0;
			double MostFoot = 0.0, Shallowest = 90.0;
			for (const ShipFile& Ship : Files)
			{
				for (int Far = 0; Far < 2; ++Far)
				{
					const FRawShip& Raw = Ship.Mesh[Far];
					std::vector<std::array<long long, 3>> Mm;
					const std::vector<int32> Place = PlacesOf(Raw, Mm);
					std::vector<bool> Worn(Raw.Points.size(), false);
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						const ShipPaintCore::Point P = PointOf(Raw, K);
						Worn[static_cast<size_t>(K)] = P.bPainted && P.Wear > 0;
					}
					const std::vector<int32> Patch = PatchesOf(Raw, Worn, Place);
					const std::vector<uint8_t> Reader = ReaderRuns(Raw, Skin);
					std::map<int32, std::vector<size_t>> BodiesOf;
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						if (Worn[static_cast<size_t>(K)] && PointOf(Raw, K).Wear >= 20)
						{
							BodiesOf[Patch[static_cast<size_t>(K)]].push_back(static_cast<size_t>(K));
						}
					}
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						// (the film as the game lays it: the reader's own mark)
						const ShipPaintCore::Point Game = PointOf(Raw, K, &Reader);
						if (Game.bPainted && Game.Wear > 0 && Game.bBodyAbove)
						{
							ShipPaintCore::Laid Filmed;
							ShipPaintCore::Ramp(Game, &Filmed);
							if (Filmed.Foot > 0.0f && Filmed.Over > 0.0f)
							{
								// (the steepest body point of its run above it, within half the foot's width plus the rake, on its face)
								++WithFilm;
								double Steepest = -90.0;
								bool bSteep = false;
								for (const size_t B : BodiesOf[Patch[static_cast<size_t>(K)]])
								{
									const long long Drop = Mm[B][2] - Mm[static_cast<size_t>(K)][2];
									const long long DX = Mm[B][0] - Mm[static_cast<size_t>(K)][0], DY = Mm[B][1] - Mm[static_cast<size_t>(K)][1];
									const long long Aside = DX * DX + DY * DY, Reach = 225 + Drop * 1763 / 10000;
									const FVector &Foot = Raw.Facings[K], &Body = Raw.Facings[B];
									if (Drop >= 1 && Drop <= 3000 && Aside <= Reach * Reach && Foot.X * Body.X + Foot.Y * Body.Y + Foot.Z * Body.Z >= 0.0)
									{
										bSteep = bSteep || Aside <= Drop * Drop;
										Steepest = std::max(Steepest, std::atan2(static_cast<double>(Drop), std::sqrt(static_cast<double>(Aside))) * 180.0 / 3.14159265358979);
									}
								}
								Shallow += bSteep ? 0 : 1;
								Shallowest = std::min(Shallowest, Steepest);
							}
						}
						const ShipPaintCore::Point P = PointOf(Raw, K, &MarksOf(Ship, Far));
						if (!P.bPainted || P.Wear <= 0 || (P.Wear < 20 && !P.bBodyAbove))
						{
							continue;
						}
						++Ramped;
						if (P.bBodyAbove)
						{
							continue;
						}
						ShipPaintCore::Laid How;
						ShipPaintCore::Ramp(P, &How);
						++NoBody;
						const Levels C = LevelsOf(How.Colour);
						FootLaid += How.Foot > 0.0f ? 1 : 0;
						MostFoot = std::max<double>(MostFoot, How.Foot);
						Lighter += C.R > BodyR + 0.01 || C.G > BodyG + 0.01 || C.B > BodyB + 0.01 ? 1 : 0;
					}
				}
			}
			Check(FootLaid == 0 && Lighter == 0 && NoBody > 1000,
				"rule 3: no foot colour without body above it: where the ramp paints a point with no run's body above, no share of 124,80,50, and its colour no lighter than the body's 96,58,38",
				std::to_string(NoBody) + " of " + std::to_string(Ramped) + " ramped points with no body above, " + std::to_string(FootLaid) + " with film (at most " + Num(100.0 * MostFoot, 1) +
					"%), " + std::to_string(Lighter) + " lighter than the body");
			Check(WithFilm > 1000 && Shallow == 0,
				"the film is still laid at the foot of runs with body above them, and every point that takes it has a body point of its run above it at 45 degrees or steeper (within 22.5 cm plus the rake, on its own face)",
				std::to_string(WithFilm) + " points with film, " + std::to_string(Shallow) + " without; the shallowest steepest body above at " + Num(Shallowest, 1) + " degrees");
		}

		// Her three spots, as painted colours on the points (sRGB levels, the mean over the spot): today's (55419eb6, every
		// switch off) and under the skin now. Measured in the round 1 AFTER renders (Blender's light): the U-boat's band between
		// her rails 106,107,111 to 108,99,99; the battleship's side 72,87,104 to 74,82,95; a destroyer's blotches. The render
		// will confirm these.
		{
			struct Spot
			{
				std::string Name;
				Levels Paint, Today, Now, LightToday, LightNow;
				size_t Points = 0, Light = 0;
			};
			const auto Mean = [](const Painted& Look, const std::vector<int32>& Which)
			{
				Levels Out;
				for (const int32 K : Which)
				{
					const Levels One = LevelsOf(Look.Linear[4 * static_cast<size_t>(K)], Look.Linear[4 * static_cast<size_t>(K) + 1], Look.Linear[4 * static_cast<size_t>(K) + 2]);
					Out.R += One.R / static_cast<double>(Which.size());
					Out.G += One.G / static_cast<double>(Which.size());
					Out.B += One.B / static_cast<double>(Which.size());
				}
				return Out;
			};
			const auto Measure = [&](const std::string& File, const std::string& Name, const std::function<std::vector<int32>(const FRawShip&, int32)>& Which)
			{
				Spot Out;
				Out.Name = Name;
				const auto Found = std::find_if(Files.begin(), Files.end(), [&File](const ShipFile& One) { return One.Name == File; });
				if (Found == Files.end())
				{
					return Out;
				}
				const FRawShip& Raw = Found->Mesh[0];
				const auto Hull = std::find(Found->PaintNames.begin(), Found->PaintNames.end(), "hull");
				const int32 HullAt = Hull == Found->PaintNames.end() ? -1 : static_cast<int32>(Hull - Found->PaintNames.begin());
				const std::vector<int32> Points = Which(Raw, HullAt);
				Out.Points = Points.size();
				if (Points.empty())
				{
					return Out;
				}
				Out.Paint = LevelsOf(PointOf(Raw, Points[0]).Paint);
				const Painted Today = PaintOld(Raw), Now = PaintNew(Raw, Skin);
				Out.Today = Mean(Today, Points);
				Out.Now = Mean(Now, Points);
				// (and its light wear away from any run alone, rule 2's points, as against its runs' rust and feet)
				std::vector<int32> Lights;
				for (const int32 K : Points)
				{
					const ShipPaintCore::Point P = PointOf(Raw, K, &MarksOf(*Found, 0));
					if (P.bPainted && P.Wear > 0 && P.Wear < 20 && !P.bBodyAbove)
					{
						Lights.push_back(K);
					}
				}
				Out.Light = Lights.size();
				if (!Lights.empty())
				{
					Out.LightToday = Mean(Today, Lights);
					Out.LightNow = Mean(Now, Lights);
				}
				return Out;
			};
			// (the U-boat: her casing side between her rails, every point of her hull paint, the weeps' own ribbons, hull.on, left out)
			const Spot Band = Measure("Submarine_GER", "the U-boat's band between her rails (Submarine_GER, every point of her hull paint)",
				[](const FRawShip& Raw, int32 HullAt)
				{
					std::vector<int32> Out;
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						if (Raw.Paints[K] == HullAt)
						{
							Out.push_back(K);
						}
					}
					return Out;
				});
			// (the battleship: her side's lightly worn patches, her hull paint with wear 5 to 19, worn by the plan's measure)
			const Spot Side = Measure("Battleship_USA", "the battleship's side (Battleship_USA, her hull paint with wear 5 to 19)",
				[](const FRawShip& Raw, int32 HullAt)
				{
					std::vector<int32> Out;
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						if (Raw.Paints[K] == HullAt && Raw.Wear[K] >= 5 && Raw.Wear[K] < 20)
						{
							Out.push_back(K);
						}
					}
					return Out;
				});
			// (a destroyer's blotch: the German destroyer of board 02, her largest patch of hull paint with wear 5 to 19, joined
			// by the mesh's triangles, low on her side aft of the bridge)
			const Spot Blotch = Measure("Destroyer_GER", "a destroyer's blotch (Destroyer_GER, her largest patch of hull paint with wear 5 to 19)",
				[](const FRawShip& Raw, int32 HullAt)
				{
					std::vector<bool> Keep(Raw.Points.size(), false);
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						Keep[static_cast<size_t>(K)] = Raw.Paints[K] == HullAt && Raw.Wear[K] >= 5 && Raw.Wear[K] < 20;
					}
					std::vector<std::array<long long, 3>> Mm;
					const std::vector<int32> Patch = PatchesOf(Raw, Keep, PlacesOf(Raw, Mm));
					std::map<int32, std::vector<int32>> Of;
					for (int32 K = 0; K < Raw.Points.Num(); ++K)
					{
						if (Patch[static_cast<size_t>(K)] >= 0)
						{
							Of[Patch[static_cast<size_t>(K)]].push_back(K);
						}
					}
					std::vector<int32> Most;
					for (const auto& [Root, Members] : Of)
					{
						Most = Members.size() > Most.size() ? Members : Most;
					}
					return Most;
				});
			bool bHeld = true;
			for (const Spot* One : { &Band, &Side, &Blotch })
			{
				std::printf("  spot  %s, %zu points: paint %s; today %s; now %s (red %+.1f, red less blue %+.1f, %.1f%% darker against the paint)\n", One->Name.c_str(), One->Points,
					Said(One->Paint).c_str(), Said(One->Today).c_str(), Said(One->Now).c_str(), One->Now.R - One->Paint.R, (One->Now.R - One->Now.B) - (One->Paint.R - One->Paint.B),
					100.0 * (1.0 - One->Now.Bright() / std::max(One->Paint.Bright(), 1e-9)));
				std::printf("        of them %zu light wear away from a run: today %s; now %s (red less blue %+.1f, %.1f%% darker against the paint)\n", One->Light, Said(One->LightToday).c_str(),
					Said(One->LightNow).c_str(), (One->LightNow.R - One->LightNow.B) - (One->Paint.R - One->Paint.B), 100.0 * (1.0 - One->LightNow.Bright() / std::max(One->Paint.Bright(), 1e-9)));
				bHeld = bHeld && One->Points > 0 && One->Now.R <= One->Paint.R + 1e-6 && One->Now.G <= One->Paint.G + 1e-6 && One->Now.B <= One->Paint.B + 1e-6;
			}
			Check(bHeld, "her three spots found, and under the skin no channel of any above its paint (the pink wash gone)",
				"the band " + Said(Band.Now) + ", the side " + Said(Side.Now) + ", the blotch " + Said(Blotch.Now));
		}
	}

	std::printf("%d checks, %d failed\n%s\n", Checks, Failures, Failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failures ? 1 : 0;
}
