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
//       alike and none one flat colour; edge wear darker, never whiter; the salt as ruled; a wooden deck under her
//       150,135,110 as drawn; and the sea's steel ink, worked through M_MapSurface's own sums as Tools/build_content.py
//       makes them: the top light never above today's (none) unless the light switch asks with the skin on, no chips;
//   (d) a flipped default is caught: the identity check run with an option on fails, both legs of it.
// Usage: ship_paint_test <Source/TheaterCommand> <Content/Models/Ships>   (built by Tools/ship_detail/run.sh)

#include "paint_shim.h"

#include "Battle/LandJson.h"
#include "Battle/SeaLookSwitches.h"
#include "Battle/ShipPaintCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
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
		for (int Far = 0; Far < 2; ++Far)
		{
			const LandJson::Json& Part = File[Far ? "far" : "near"];
			FRawShip& Raw = Out.Mesh[Far];
			const std::vector<double> Points = LandJson::Numbers(Part["points"]), Paints = LandJson::Numbers(Part["paints"]);
			if (Points.size() != Paints.size() * 3 || Paints.empty())
			{
				Why = std::string(Far ? "far" : "near") + ": points and paints do not agree";
				return false;
			}
			for (size_t K = 0; K + 2 < Points.size(); K += 3)
			{
				// (the file has x forward and y to the right; the game's vehicles are built nose toward -X with their right hand at -Y)
				Raw.Points.Add(FVector{ -Points[K], -Points[K + 1], Points[K + 2] });
				Raw.Facings.Add(FVector{});
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

	/** A point as the tree's branch hands it to the core. */
	ShipPaintCore::Point PointOf(const FRawShip& Raw, int32 K)
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
		return At;
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
		Check(Call != std::string::npos && Gate != std::string::npos && Call - Gate < 600, "and only behind its switches (-TheaterSeaShipLight=1 or -TheaterSeaShipSkin=1)", "SeaBattle.cpp");
		// (the three ship switches are for review pictures only, as the reel's two: the month's sea scene inside a campaign
		// opens the same way, with bMonthScene set first, and never takes them)
		const size_t Month = Scene.find("\nbool ASeaBattle::BeginMonthScene(");
		const size_t MonthSet = Scene.find("bMonthScene = true;", Month), MonthOpen = Scene.find("OpenOn(", Month);
		bool bGated = MonthSet != std::string::npos && MonthOpen != std::string::npos && MonthSet < MonthOpen && !Has(Scene, "TakeOnOff(SeaLook, \"TheaterSeaShip");
		for (const char* Line : { "const bool bShipLight = SeaLookSwitches::TakeReview(SeaLook, \"TheaterSeaShipLight=\", !bMonthScene);",
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
		Check(Reads == 2 && Has(Minis, "(Raw->bShip ? FString(UTF8_TO_TCHAR(ShipPaintCore::KeyOf(ShipPaint).c_str())) : FString())") &&
				  Has(Minis, "ShipPaintCore::Paint(At, ShipColours, ShipPaint)"),
			"the miniatures read them in two places only: a ship's cache key and the ship branch", std::to_string(Reads) + " reads");
	}

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
		size_t Feet = 0, Bodies = 0, Heads = 0;
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
					const ShipPaintCore::Point P = PointOf(Raw, K);
					if (!P.bPainted || P.Wear <= 0)
					{
						continue;
					}
					++Rusted;
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
			std::to_string(Rusted) + " rusted points: " + std::to_string(Feet) + " film, " + std::to_string(Bodies) + " body, " + std::to_string(Heads) + " source");
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
				ShipPaintCore::Point P = PointOf(Raw, K);
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

		// The salt as ruled (the U-boats): toward 196,200,198 by no more than 30%, less over a rust source.
		size_t Salts = 0, SaltOver = 0;
		double MostShare = 0.0;
		const ShipPaintCore::Linear Pale = { ShipPaintCore::ToLinear(ShipPaintCore::SaltR / 255.0f), ShipPaintCore::ToLinear(ShipPaintCore::SaltG / 255.0f),
			ShipPaintCore::ToLinear(ShipPaintCore::SaltB / 255.0f), 1.0f };
		for (const ShipFile& Ship : Files)
		{
			const FRawShip& Raw = Ship.Mesh[0];
			const ShipPaintCore::Palette Pal = PaletteOf(Raw);
			for (int32 K = 0; K < Raw.Points.Num(); ++K)
			{
				ShipPaintCore::Point P = PointOf(Raw, K);
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
						SaltOver += Share > ShipPaintCore::SaltMost + 1e-4 ? 1 : 0;
					}
				}
			}
		}
		Check(Salts > 1000 && SaltOver == 0, "the salt toward 196,200,198 by no more than 30%", std::to_string(Salts) + " salted points, the most " + Num(100.0 * MostShare) + "%");

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
		// the sums below are the graph's): the lift on what faces the sky, 1 + up x TopLight x (metal x Wear); the blot
		// mottle, the colour times lerp(1, 0.8 + 0.42 blot, Wear x (metal/2 + 1/2)); a chip where (speck x (blot + 0.5) +
		// ChipAt + gather x ChipGather) x ChipSharp passes 0, times metal and Wear, drawn as metal (0.85) in ChipColour.
		{
			const std::string Build = ReadAll(Src + "/../../Tools/build_content.py");
			const size_t From = Build.find("def build_map_surface("), To = Build.find("\ndef build_solid(");
			const std::string Graph = From != std::string::npos && To != std::string::npos && To > From ? Build.substr(From, To - From) : std::string();
			bool bSums = !Graph.empty();
			for (const char* Piece : { "wear = scalar(m, \"Wear\", 0.0, ", "scalar(m, \"TopLight\", 0.26, ",
					 "lit_top = plus(m, 1.0, times(m, times(m, one_in(m, unreal.MaterialExpressionSaturate, skyward, ", "times(m, metal, wear, -600, -2150), -450, -2100), -300, -2100)",
					 "lerp(m, 1.0, plus(m, times(m, (blot, \"R\"), 0.42, -350, -1500), 0.8, -200, -1500),",
					 "times(m, wear, plus(m, times(m, metal, 0.5, 100, -1450), 0.5, 200, -1450), 300, -1450), 450, -1500), 600, -900), lit_top, 700, -1000)",
					 "gather = one_in(m, unreal.MaterialExpressionSaturate, ", "times(m, (speck, \"R\"), plus(m, (blot, \"G\"), 0.5, -350, -1650), -200, -1700), scalar(m, \"ChipAt\", -0.78, ",
					 "times(m, gather, scalar(m, \"ChipGather\", 0.3, ", "scalar(m, \"ChipSharp\", 9.0, ", "metal, 450, -1750), wear, 600, -1750)",
					 "to_property(times(m, chipped, 0.85, 750, -700), \"\", unreal.MaterialProperty.MP_METALLIC)", "scalar(m, \"BlotTiling\", 0.009, " })
			{
				bSums = bSums && Has(Graph, Piece);
			}
			Check(bSums && ShipPaintCore::MapTopLight == 0.26f && ShipPaintCore::MottleMost == 0.8f + 0.42f,
				"M_MapSurface's lift, mottle and chip as this test works them (Tools/build_content.py, its defaults: TopLight 0.26, Wear 0, ChipAt -0.78)", "build_map_surface");
			const auto Lift = [](float Up, float Metal, float TopLight, float Wear) { return 1.0f + std::min(1.0f, std::max(0.0f, Up)) * TopLight * (Metal * Wear); };
			const auto Mottle = [](float Metal, float Wear, float Blot)
			{
				const float Share = Wear * (Metal * 0.5f + 0.5f);
				return 1.0f + (0.8f + 0.42f * Blot - 1.0f) * Share;
			};
			const auto Chip = [](float Speck, float Blot, float Gather, float ChipAt, float Metal, float Wear)
			{
				return std::min(1.0f, std::max(0.0f, (Speck * (Blot + 0.5f) + ChipAt + Gather * 0.3f) * 9.0f)) * Metal * Wear;
			};
			// (each switch as the scene sets it: SeaBattle's options and wear, through the stage's SetShipPaint, which sets
			// what SteelOf gives and nothing else; with neither switch, nothing is set and the ink is as made)
			const std::string Stage = ReadAll(Src + "/Battle/AirStage.cpp"), Scene = ReadAll(Src + "/Battle/SeaBattle.cpp");
			const size_t SetAt = Stage.find("void AAirStage::SetShipPaint(");
			const std::string SetBody = SetAt == std::string::npos ? std::string() : Stage.substr(SetAt, Stage.find("\n}\n", SetAt) - SetAt);
			size_t Params = 0;
			for (size_t At = SetBody.find("SetScalarParameterValue"); At != std::string::npos; At = SetBody.find("SetScalarParameterValue", At + 1))
			{
				++Params;
			}
			Check(Has(Scene, "Paint.TopLight = bShipLight ? ShipPaintCore::SeaTopLight : -1.0f;") && Has(Scene, "Stage->SetShipPaint(Paint, bShipSkin ? ShipPaintCore::SeaFirstLookWear : 0.0f);") &&
					  Has(SetBody, "const ShipPaintCore::Steel SteelSet = ShipPaintCore::SteelOf(Given, FirstLookWear);") &&
					  Has(SetBody, "SteelInk->SetScalarParameterValue(TEXT(\"TopLight\"), SteelSet.TopLight);") && Has(SetBody, "SteelInk->SetScalarParameterValue(TEXT(\"Wear\"), SteelSet.Wear);") &&
					  Has(SetBody, "SteelInk->SetScalarParameterValue(TEXT(\"ChipAt\"), ShipPaintCore::SeaChipAt);") && Params == 4 && !Has(SetBody, "SetVectorParameterValue"),
				"the scene's switches reach the steel ink only through SteelOf: TopLight, Wear, BlotTiling and ChipAt, nothing else", std::to_string(Params) + " set");
			std::string Lifts;
			bool bLift = true, bNoChips = true;
			float SkinMottle = 0.0f;
			for (const int Case : { 0, 1, 2, 3 })
			{
				const bool bLight = (Case & 1) != 0, bSkin = (Case & 2) != 0;
				float TopLight = ShipPaintCore::MapTopLight, Wear = 0.0f, ChipAt = -0.78f;
				if (bLight || bSkin)
				{
					ShipPaintCore::Options O;
					O.bShade = bLight;
					O.TopLight = bLight ? ShipPaintCore::SeaTopLight : -1.0f;
					O.bRustRamp = bSkin;
					const ShipPaintCore::Steel Set = ShipPaintCore::SteelOf(O, bSkin ? ShipPaintCore::SeaFirstLookWear : 0.0f);
					TopLight = Set.TopLight >= 0.0f ? Set.TopLight : TopLight;
					Wear = Set.Wear > 0.0f ? Set.Wear : Wear;
					ChipAt = Set.bChipsOff ? ShipPaintCore::SeaChipAt : ChipAt;
				}
				// (a steel deck facing the sky, metal as the reader marks painted steel: its alpha 128 less half its wear)
				const float Lifted = Lift(1.0f, 1.0f, TopLight, Wear);
				Lifts += std::string(Lifts.empty() ? "" : ", ") + (Case == 0 ? "off " : Case == 1 ? "light alone " : Case == 2 ? "skin alone " : "both ") + Num(Lifted, 2);
				bLift = bLift && std::abs(Lifted - (Case == 3 ? 1.0f + ShipPaintCore::SeaTopLight : 1.0f)) < 1e-6f;
				for (const float Speck : { 0.0f, 1.0f })
				{
					for (const float Blot : { 0.0f, 1.0f })
					{
						for (const float Gather : { 0.0f, 1.0f })
						{
							bNoChips = bNoChips && Chip(Speck, Blot, Gather, ChipAt, 1.0f, Wear) == 0.0f;
						}
					}
				}
				SkinMottle = bSkin ? std::max(SkinMottle, Mottle(0.0f, Wear, 1.0f)) : SkinMottle;
			}
			Check(bLift, "a steel deck facing the sky is lifted no more than today (none) unless the light switch asks with the skin on, and then by the art director's 0.10",
				Lifts);
			Check(bNoChips && Chip(1.0f, 1.0f, 1.0f, -0.78f, 1.0f, 1.0f) > 0.0f, "no chip anywhere at sea under any switch (the material's own chips, as made, would show)",
				"ChipAt " + Num(ShipPaintCore::SeaChipAt, 1));
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

	std::printf("%d checks, %d failed\n%s\n", Checks, Failures, Failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failures ? 1 : 0;
}
