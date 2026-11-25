// The setup step's cloud test (release 2; the Mac builder's crash, 22 November: the general's walk set one unit down in
// the woods and the game fell over about five seconds on). The game's own cores, no engine, played through a live
// battle opened with his side deploying (setup-deltas.ts: quiet pieces, two units placed, each place a reset piece that
// stands every man again, the battle begun, the fight after it) at 60 frames a second as the scene plays them: a piece
// taken each frame; the recording grown (LandFeed), the fire (LandPlay), the men (LandMen, with the pose bank, thrifty,
// the eye gliding about the field as his view does) and every question the scene's DrawMen asks of each man; his
// clipboard (LandCommand) and the walk's choices (LandScript). Built with the address and undefined-behaviour
// sanitizers by setup.sh: any read past an end or of freed memory fails it.
// Usage: land_live_setup_test <folder (header.json, deltas.jsonl)> <Content folder>
#include "Battle/Fates.h"
#include "Battle/LandDesk.h"
#include "Battle/LandEye.h"
#include "Battle/LandField.h"
#include "Battle/LandGeneralEye.h"
#include "Battle/LandSight.h"
#include "Battle/LandCommand.h"
#include "Battle/LandFeed.h"
#include "Battle/LandJson.h"
#include "Battle/LandKit.h"
#include "Battle/LandMen.h"
#include "Battle/LandPlay.h"
#include "Battle/LandScript.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	std::string ReadAll(const std::string& Path)
	{
		std::ifstream F(Path, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	/** The battle's ground (the header's field): heights at the cells' centres, bilinear. */
	struct Heights
	{
		int Cols = 1, Rows = 1;
		double Cell = 25.0;
		std::vector<float> H;
		float At(int C, int R) const
		{
			C = std::max(0, std::min(Cols - 1, C));
			R = std::max(0, std::min(Rows - 1, R));
			return H[static_cast<size_t>(R) * static_cast<size_t>(Cols) + static_cast<size_t>(C)];
		}
		LandMen::Ground operator()(double X, double Y) const
		{
			const double FX = X / Cell - 0.5, FY = Y / Cell - 0.5;
			const int C0 = static_cast<int>(std::floor(FX)), R0 = static_cast<int>(std::floor(FY));
			const double U = FX - C0, V = FY - R0;
			const double H00 = At(C0, R0), H10 = At(C0 + 1, R0), H01 = At(C0, R0 + 1), H11 = At(C0 + 1, R0 + 1);
			LandMen::Ground G;
			G.Height = static_cast<float>(H00 * (1 - U) * (1 - V) + H10 * U * (1 - V) + H01 * (1 - U) * V + H11 * U * V);
			G.Normal = { 0.f, 0.f, 1.f };
			return G;
		}
	};
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: land_live_setup_test <folder> <Content folder>\n");
		return 2;
	}
	const std::string Folder = Argv[1], Content = Argv[2], Soldiers = Content + "/Models/Soldiers";
	int Failures = 0;
	auto Check = [&Failures](bool bOk, const char* What)
	{
		if (bOk) return;
		++Failures;
		if (Failures <= 30) std::printf("FAIL: %s\n", What);
	};

	// ---- The header, the deltas a line each ----
	const std::string HeaderText = ReadAll(Folder + "/header.json");
	LandFeed::Recording Rec;
	std::string Error;
	if (HeaderText.empty() || !LandFeed::ReadRecording(HeaderText, Rec, Error))
	{
		std::printf("%s/header.json: %s\n", Folder.c_str(), Error.c_str());
		return 2;
	}
	std::vector<std::string> Deltas;
	{
		std::ifstream In(Folder + "/deltas.jsonl");
		std::string Line;
		while (std::getline(In, Line))
		{
			if (!Line.empty()) Deltas.push_back(Line);
		}
	}
	Heights Hs;
	Hs.Cols = Rec.Ground.Cols;
	Hs.Rows = Rec.Ground.Rows;
	Hs.Cell = Rec.Ground.Cell;
	Hs.H = Rec.Ground.Height;
	if (Hs.H.size() != static_cast<size_t>(Hs.Cols) * static_cast<size_t>(Hs.Rows) || Hs.Cols <= 0)
	{
		std::printf("the header's field heights do not fit its cells\n");
		return 2;
	}

	// ---- The men, as the scene makes them: every soldier file there is, the armies, the pose bank, thrifty ----
	LandMen::Crowd Men;
	Men.SetLife(2.0);
	int Files = 0;
	for (const char* Army : { "GER", "POL", "SOV", "USA", "USA_bar", "USA_nco", "USA_bazooka", "USA_loader", "USA_radio" })
	{
		const std::string Text = ReadAll(Soldiers + "/soldier_" + Army + ".json");
		if (Text.empty()) continue;
		auto One = std::make_shared<LandMen::Soldier>();
		if (!One->Load(Text, Error))
		{
			std::printf("soldier_%s.json: %s\n", Army, Error.c_str());
			return 2;
		}
		Men.AddSoldier(Army, One);
		++Files;
	}
	if (!Men.ReadArmies(ReadAll(Content + "/Data/LandBattles/armies.json"), Error))
	{
		std::printf("armies.json: %s\n", Error.c_str());
		return 2;
	}
	const std::string Bank = ReadAll(Soldiers + "/pose_bank.json");
	const bool bBank = !Bank.empty() && Men.ReadPoseBank(Bank, Error);
	if (!Men.Build(HeaderText, [&Hs](double X, double Y) { return Hs(X, Y); }, Error))
	{
		std::printf("the men from the header: %s\n", Error.c_str());
		return 2;
	}
	Men.SetThrift(true);
	std::printf("%d soldier files, the pose bank %s (%d poses)\n", Files, bBank ? "on" : "off", bBank ? Men.BankPoses() : 0);

	// ---- The tanks, guns and lorries, as the scene loads them: the header's models read ahead ----
	Fates::Table Table;
	const bool bTable = Table.Load(ReadAll(Content + "/Data/Battle/fates.json"), Error);
	LandKit::Battle Armour;
	Armour.Life = 2.0;
	const LandKit::Battle::ReadFn ReadModel = [&Content](const std::string& File, std::string& Out)
	{
		Out = ReadAll(Content + "/Models/Battle/" + File);
		return !Out.empty();
	};
	const LandKit::GroundFn KitGround = [&Hs](double X, double Y, double& OutZ, LandKit::V3& OutNormal)
	{
		OutZ = Hs(X, Y).Height;
		OutNormal = LandKit::V3(0.0, 0.0, 1.0);
	};
	if (!Armour.Load(HeaderText, ReadModel, KitGround, bTable ? &Table : nullptr, static_cast<uint64_t>(Rec.Seed), Error))
	{
		std::printf("the tanks from the header: %s\n", Error.c_str());
		return 2;
	}

	// ---- The field the game lays and what stands on it, for the cameras (as land_play_test builds them) ----
	using LandPlay::Vec;
	const LandField::Field FieldWords = LandSight::FieldOf(Rec);
	const LandField::Ground Field(FieldWords);
	LandSight::Room Sight;
	Sight.Lay(&Field);
	LandEye::World EW;
	EW.Height = [&Field](double X, double Y) { return Field.HeightAt(X, Y); };
	EW.Free = [&Field](const Vec& P)
	{
		LandField::Drawn HW = LandField::Drawn::Pasture;
		const double HZ = Field.HeightAt(P.X, P.Y, &HW);
		if (HW == LandField::Drawn::Floor || HW == LandField::Drawn::Wall) return false;
		return !(HW == LandField::Drawn::Bank && P.Z < HZ + 3.2);
	};
	EW.Open = [&Field](const Vec& From, const Vec& To)
	{
		const Vec Way = To - From;
		const double Long = Way.Length();
		const int Steps = std::clamp(static_cast<int>(Long / 2.0), 2, 200);
		for (int K = 1; K < Steps; ++K)
		{
			const double F = static_cast<double>(K) / Steps;
			if (F * Long < 1.5 || (1.0 - F) * Long < 2.5) continue;
			const Vec P = From + Way * F;
			LandField::Drawn HW = LandField::Drawn::Pasture;
			const double HZ = Field.HeightAt(P.X, P.Y, &HW);
			double Top = HZ;
			switch (HW)
			{
			case LandField::Drawn::Bank: Top += 3.0; break;
			case LandField::Drawn::Wall: Top += 2.0; break;
			case LandField::Drawn::Floor: Top += 7.0; break;
			case LandField::Drawn::Woods: Top += 14.0; break;
			default: break;
			}
			if (P.Z < Top) return false;
		}
		return true;
	};
	EW.Crop = [&Field](double X, double Y)
	{
		std::vector<LandField::Placed> Near;
		Field.Clutter(X - 0.7, Y - 0.7, X + 0.7, Y + 0.7, Near);
		const double Ground = Field.At(X, Y).Z;
		double Top = 0.0;
		for (const LandField::Placed& One : Near)
		{
			if (One.Kind != LandField::Base::Wheat && One.Kind != LandField::Base::TallGrass) continue;
			Top = std::max(Top, static_cast<double>(One.Z) + static_cast<double>(Field.BaseOf(One.Kind).Height) * static_cast<double>(One.ScaleZ) - Ground);
		}
		return Top;
	};
	EW.Solid = [&Sight](const Vec& P, int Except) { return static_cast<int>(Sight.At(P, Except)); };
	// (the scene's own: where a track's man or vehicle is now, his chest, and the enemy hidden from the film)
	std::vector<int> ManOf, VehicleOf;
	auto ManPoints = [&Men](int Man, Vec& OutHead, Vec& OutChest)
	{
		if (Man < 0 || Man >= Men.Count()) return false;
		const LandMen::Soldier* Sol = Men.SoldierOf(Man);
		const LandMen::Xform* Fr = Men.FramesOf(Man);
		if (!Sol || !Fr) return false;
		const int Hb = Sol->BoneIndex("head"), Cb = Sol->BoneIndex("chest"), N = static_cast<int>(Sol->Bones.size());
		if (Hb < 0 || Hb >= N || Cb < 0 || Cb >= N) return false;
		const LandMen::Vec3 H = Fr[Hb].Point(Sol->Bones[static_cast<size_t>(Hb)].Head);
		const LandMen::Vec3 C = Fr[Cb].Point(Sol->Bones[static_cast<size_t>(Cb)].Head);
		OutHead = Vec(H.X, H.Y, H.Z);
		OutChest = Vec(C.X, C.Y, C.Z);
		return true;
	};
	EW.Where = [&](int Track, double, Vec& OutAt, double& OutFacing)
	{
		if (Track < 0) return false;
		const size_t K = static_cast<size_t>(Track);
		if (K < VehicleOf.size() && VehicleOf[K] >= 0)
		{
			const LandKit::Vehicle& V = Armour.Vehicles()[static_cast<size_t>(VehicleOf[K])];
			OutAt = Vec(V.Root.At.X, V.Root.At.Y, V.Root.At.Z);
			OutFacing = V.Heading;
			return true;
		}
		if (K < ManOf.size() && ManOf[K] >= 0)
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
			float F = 0.0f;
			if (Men.PlaceOf(ManOf[K], X, Y, Z, F))
			{
				OutAt = Vec(X, Y, Z);
				OutFacing = F;
				return true;
			}
		}
		return false;
	};
	EW.Body = [&](int Track, double, Vec& OutAt)
	{
		if (Track < 0) return false;
		const size_t K = static_cast<size_t>(Track);
		if (K < VehicleOf.size() && VehicleOf[K] >= 0)
		{
			const LandKit::Vehicle& V = Armour.Vehicles()[static_cast<size_t>(VehicleOf[K])];
			OutAt = Vec(V.Root.At.X, V.Root.At.Y, V.Root.At.Z + 1.3);
			return true;
		}
		Vec Head, Chest;
		if (K < ManOf.size() && ManOf[K] >= 0 && ManPoints(ManOf[K], Head, Chest))
		{
			OutAt = Chest;
			return true;
		}
		return false;
	};
	LandEye::Settings ES;
	ES.Seed = Rec.Seed;
	LandEye::Director Director;
	Director.Begin(Rec, EW, ES);
	LandDesk::Desk Desk;
	Desk.Begin(Rec, 0);
	LandGeneralEye::Eye Own;
	LandGeneralEye::Input In;
	bool bOwn = false;
	std::vector<int> TrackOfMan, TrackOfVehicle;

	LandPlay::Replay Play;
	Play.Begin(Rec, LandPlay::World());
	LandCommand::General G;
	G.Begin(Rec);

	// ---- Play: a piece a frame (all waiting at once when the screen has caught up with the rules), as PumpLive ----
	const double Dt = 1.0 / 60.0;
	double Battle = 0.0, Real = 0.0;
	size_t Next = 0;
	int Resets = 0, Frames = 0, Banked = 0, Skinned = 0, Shown = 0;
	long Vehicled = 0;
	std::vector<float> At, Facing, BankAt, BankFacing;
	std::vector<LandPlay::Effect> Fx;
	std::vector<LandPlay::Noise> Noises;
	std::vector<LandPlay::Loss> Losses;
	bool bSetupSeen = false, bBegunSeen = false;
	// (his view: from over his ground, gliding about the field every few seconds as the walk's glides do)
	double EyeX = Hs.Cols * Hs.Cell * 0.5, EyeY = Hs.Rows * Hs.Cell * 0.2, EyeZ = 260.0;
	while (Next < Deltas.size() || Battle < Rec.Fought - 2.0)
	{
		const bool bWaitedOn = !Rec.bOver && Battle >= Rec.Fought - 2.0 - 1e-6;
		int Taken = 0;
		// (the worker's pieces come in over real time: ten a second here, the quiet ones of the setup step too)
		const size_t Arrived = std::min(Deltas.size(), static_cast<size_t>(Real * 10.0) + 1);
		while ((Taken == 0 || bWaitedOn) && Next < Arrived)
		{
			++Taken;
			LandFeed::Grown Grew;
			if (!LandFeed::AppendDelta(Deltas[Next], Rec, Grew, Error))
			{
				std::printf("delta %zu: %s\n", Next, Error.c_str());
				return 1;
			}
			if (Grew.bReset) ++Resets;
			Play.Extend(Grew);
			// (the scene keeps her vehicles' bodies through a reset only when they are the same vehicles in the same order)
			std::vector<LandKit::TrackWords> WordsBefore;
			if (Grew.bReset)
			{
				for (const LandKit::Vehicle& V : Armour.Vehicles()) WordsBefore.push_back(V.Words());
			}
			if (!Armour.Extend(Deltas[Next], Error))
			{
				std::printf("the tanks not grown at delta %zu: %s\n", Next, Error.c_str());
				return 1;
			}
			if (Grew.bReset)
			{
				bool bKept = Armour.Vehicles().size() == WordsBefore.size();
				for (size_t V = 0; bKept && V < Armour.Vehicles().size(); ++V)
				{
					const LandKit::TrackWords& W = Armour.Vehicles()[V].Words();
					bKept = W.Block == WordsBefore[V].Block && W.K == WordsBefore[V].K && W.J == WordsBefore[V].J;
				}
				std::printf("reset at delta %zu (real %.1f s): vehicles %zu -> %zu, %s\n", Next, Real, WordsBefore.size(), Armour.Vehicles().size(), bKept ? "kept" : "made again");
			}
			if (!Men.Extend(Deltas[Next], Error))
			{
				std::printf("the men not grown at delta %zu: %s\n", Next, Error.c_str());
				return 1;
			}
			++Next;
			// (MapLive: each man's and vehicle's track, afresh after a reset)
			if (Grew.bReset)
			{
				ManOf.assign(Rec.Tracks.size(), -1);
				VehicleOf.assign(Rec.Tracks.size(), -1);
			}
			else
			{
				ManOf.resize(Rec.Tracks.size(), -1);
				VehicleOf.resize(Rec.Tracks.size(), -1);
			}
			if (Grew.bReset) TrackOfMan.assign(static_cast<size_t>(Men.Count()), -1);
			else TrackOfMan.resize(static_cast<size_t>(Men.Count()), -1);
			for (size_t K = Grew.bReset ? 0 : Grew.NewTracksFrom; K < Rec.Tracks.size(); ++K)
			{
				const LandFeed::Track& T = Rec.Tracks[K];
				if (!T.IsMan()) continue;
				const int Man = Men.Find(T.Block, T.K, T.J);
				ManOf[K] = Man;
				if (Man >= 0 && Man < Men.Count()) TrackOfMan[static_cast<size_t>(Man)] = static_cast<int>(K);
			}
			TrackOfVehicle.assign(Armour.Vehicles().size(), -1);
			for (size_t V = 0; V < Armour.Vehicles().size(); ++V)
			{
				const LandKit::TrackWords& W = Armour.Vehicles()[V].Words();
				TrackOfVehicle[V] = Rec.TrackIndexOf(W.Block, W.K, W.J);
				if (TrackOfVehicle[V] >= 0 && static_cast<size_t>(TrackOfVehicle[V]) < VehicleOf.size()) VehicleOf[static_cast<size_t>(TrackOfVehicle[V])] = static_cast<int>(V);
			}
			if (!bWaitedOn) break;
		}
		const double From = Battle;
		if (!Rec.bSetup)
		{
			Battle = std::min(std::max(0.0, Rec.Fought - 2.0), Battle + Dt);
		}
		bSetupSeen = bSetupSeen || Rec.bSetup;
		bBegunSeen = bBegunSeen || (bSetupSeen && !Rec.bSetup);
		Real += Dt;
		// (the eye glides: a new place every three seconds, eased toward it)
		const double Phase = std::fmod(Real, 3.0) / 3.0;
		const int Leg = static_cast<int>(Real / 3.0);
		const double Tx = Hs.Cols * Hs.Cell * (0.2 + 0.6 * ((Leg * 37) % 11) / 10.0), Ty = Hs.Rows * Hs.Cell * (0.1 + 0.5 * ((Leg * 53) % 7) / 6.0);
		EyeX += (Tx - EyeX) * 0.05 * Phase;
		EyeY += (Ty - EyeY) * 0.05 * Phase;
		EyeZ = 80.0 + 200.0 * (0.5 + 0.5 * std::cos(Real * 0.7));
		Armour.Update(Battle);
		for (const LandKit::Vehicle& V : Armour.Vehicles())
		{
			// (what the scene's PlaceMovers and DrawArmour ask of each)
			if (V.Fade <= 0.0) continue;
			const LandKit::Model& Look = V.Look();
			(void)Look.HullLo;
			LandKit::V3 Muzzle, Way;
			(void)V.SocketNow("muzzle", Muzzle, Way);
			++Vehicled;
		}
		Men.SetEye(EyeX, EyeY, EyeZ, 1.0);
		Men.Update(Battle);
		// (PlaceMovers: the men and vehicles where the cores have them, for the cameras' questions)
		Sight.ClearMovers();
		for (int Man = 0; Man < Men.Count(); ++Man)
		{
			if (!Men.Visible(Man) || static_cast<size_t>(Man) >= TrackOfMan.size()) continue;
			double X = 0.0, Y = 0.0, Z = 0.0;
			float F = 0.0f;
			if (!Men.PlaceOf(Man, X, Y, Z, F)) continue;
			LandSight::Man M;
			M.Track = TrackOfMan[static_cast<size_t>(Man)];
			Vec Head, Chest;
			if (ManPoints(Man, Head, Chest))
			{
				M.Feet = Head.Z - Z > 1.0 ? Vec(X, Y, Z + 0.25) : Chest + (Chest - Head) * 1.6;
				M.Top = Head;
			}
			else
			{
				M.Feet = Vec(X, Y, Z + 0.25);
				M.Top = Vec(X, Y, Z + 1.75);
			}
			Sight.Add(M);
		}
		for (size_t V = 0; V < Armour.Vehicles().size() && V < TrackOfVehicle.size(); ++V)
		{
			const LandKit::Vehicle& Vh = Armour.Vehicles()[V];
			if (Vh.Fade <= 0.0) continue;
			Sight.Add(LandSight::Machine::Plain(TrackOfVehicle[V], Vec(Vh.Root.At.X, Vh.Root.At.Y, Vh.Root.At.Z), Vh.Heading, Vh.Heading, 6.4, 3.4, 2.8, true));
		}
		const LandEye::Pose Film = Director.Update(Battle, Real, Dt);
		(void)Film;
		(void)Desk.Update(Battle, Dt);
		if (!bOwn && !G.Tags().empty())
		{
			const double* Room = G.Room();
			bOwn = Own.Begin(Rec, EW, Vec(0.5 * (Room[0] + Room[2]), 0.5 * (Room[1] + Room[3]), 0.0), 0.0, 250.0);
		}
		if (bOwn)
		{
			// (the walk's glides: to a place every three seconds; and a unit's card now and then)
			if (Frames % 180 == 0)
			{
				In.bGoTo = true;
				In.GoTo = Vec(EyeX, EyeY, 0.0);
			}
			if (Frames % 420 == 200 && !G.Tags().empty()) In.GoBlock = G.Tags()[static_cast<size_t>(Frames) % G.Tags().size()].Block;
			const LandEye::Pose Mine0 = Own.Update(In, Dt, Battle);
			(void)Mine0;
			In.bGoTo = false;
			In.GoBlock = -1;
		}
		Fx.clear();
		Noises.clear();
		Losses.clear();
		Play.Step(From, Battle, Fx, Noises, Losses);
		G.Update(Rec, Battle);
		// (the walk's questions, every frame: they read the clipboard and the ground)
		std::vector<int> None;
		const int Mine = LandScript::Mine(G, "inf", None);
		if (Mine >= 0 && G.Deploying())
		{
			double Wx = 0.0, Wy = 0.0;
			std::vector<LandScript::Point> Taken0;
			LandScript::WoodsFor(G, Rec.Ground, Mine, Taken0, Wx, Wy);
		}
		// (every question DrawMen asks of each man)
		for (int Man = 0; Man < Men.Count(); ++Man)
		{
			if (!Men.Visible(Man) || Men.Fade(Man) < 0.5f) continue;
			++Shown;
			double X = 0.0, Y = 0.0, Z = 0.0;
			float F = 0.0f;
			Men.PlaceOf(Man, X, Y, Z, F);
			(void)Men.SeenDistance(Man);
			const int Pose = Men.BankPose(Man);
			if (Pose >= 0)
			{
				const int Key = Men.BankKey(Man);
				Check(Key >= 0 && Key < Men.BankKeys(), "a banked man's set is one of the bank's");
				if (Key >= 0 && Key < Men.BankKeys() && (Frames % 30) == 0)
				{
					Check(Men.BankMesh(Key, Pose, BankAt, BankFacing), "a banked man's mesh is made");
					(void)Men.BankLook(Key);
				}
				(void)Men.WorldFrame(Man);
				++Banked;
				continue;
			}
			const LandMen::Lod Level = Men.LodOf(Man);
			(void)Men.Stamp(Man);
			const int Points = Men.PointsOf(Man, Level);
			At.assign(static_cast<size_t>(Points) * 3 + 3, 0.f);
			Facing.assign(static_cast<size_t>(Points) * 3 + 3, 0.f);
			(void)Men.SkinReadsGround(Man, Level);
			Men.Skin(Man, Level, At.data(), Facing.data());
			(void)Men.LookOf(Man, Level);
			++Skinned;
		}
		++Frames;
		if (Frames > 60 * 400) break;
	}
	std::printf("setup: %zu pieces, %d resets, %d frames (the setup step %s, begun %s), to %.1f s of battle; men shown %d man-frames, banked %d, skinned %d; vehicles %zu (%ld vehicle-frames)\n",
		Deltas.size(), Resets, Frames, bSetupSeen ? "seen" : "NOT seen", bBegunSeen ? "seen" : "NOT seen", Battle, Shown, Banked, Skinned, Armour.Vehicles().size(), Vehicled);
	Check(Resets >= 2, "the placings each stand the men again");
	Check(bSetupSeen && bBegunSeen, "the setup step opened and the battle began");
	std::printf(Failures ? "FAIL (%d)\n" : "the setup step played through\n", Failures);
	return Failures ? 1 : 0;
}
