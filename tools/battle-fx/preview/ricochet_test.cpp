// Effects (R2), small arms off armour (Brian, 25 November: "bullets should be bouncing off it"): the kit's own check of
// ShotsOnArmour, the game's code (unreal/.../Effects/TheaterFxCore.cpp) with no painter. A machine gun 120 m off fires a
// burst of 25 rounds at 15 a second at a stand-in tank, the plate handed over as the battle does (LandPlay): where it is
// aimed, the plate's outward way, how far the plate runs flat about it (its room), her deck or track guard under it (her
// floor), and how she goes (her way and her turn). The cases:
//   - her side, 45 degrees off square, the eye 30 m, 150 m and 600 m off; her turret's roof from a rise;
//   - her front plate aimed 3 cm under its top edge and 0.8 of the way to its side (both sides); her side aimed 0.1 m
//     under its top edge: the burst kept inside the plate, never in the air past her edge;
//   - her turret's side aimed 5 cm above her deck (the turret's foot), and 0.42 m above it: no spark through her deck,
//     and the same burst with no floor handed over does go through it (so the check would see it);
//   - her side low over her tracks, aimed 0.9 m up: no dust spurt (the kit's own count of spurts, whatever the height),
//     where the same burst as Shots into the same place kicks some (so the count would see one);
//   - her driving at the gun at 5 m/s, backing away from it at 5 m/s, and pivoting at 0.5 radians a second: every strike
//     on her plate where it is when it strikes, and nothing inside her as she goes (her gun, a part standing out over her
//     front that the kit is not told of, is counted apart: a spark may cross it).
// For each it checks:
//   - every round strikes the plate: one flash each, its middle on the face within 3 cm of her plate at its moment, inside
//     her face less edge_m (none off her face), and sparks whose streaks start on her face;
//   - the sparks are short and gone in under 0.3 s, thrown out of the plate, and far off fewer, bigger and drawn out to
//     a few pixels (never beads), never none (never pops);
//   - a few tracers go off again as ricochets, 10 to 50 degrees off the plate; every tracer coming in ends on it;
//   - no dust spurt: the kit's count of spurts none, and no piece of the ground's dust within 5 m of her;
//   - nothing inside the hull or the ground: every piece, all its life (a streak's both ends, a tracer to where it is
//     cut), outside every box of her as she is at that moment and above the ground;
//   - its cost: pieces and microseconds a burst, and the frames' drawing, against the same burst into the ground;
//   - a digest of every piece laid, so a g++ build and a clang++ build are shown to lay the same, number for number.
//
//   python3 flatten.py ../fx.json > /tmp/recipe.txt
//   SRC=../../../unreal/TheaterCommand/Source/TheaterCommand
//   g++ -O2 -std=c++20 -I $SRC ricochet_test.cpp $SRC/Effects/TheaterFxCore.cpp -o ricochet_test
//   ./ricochet_test /tmp/recipe.txt     (exit 0 when every check passes; the lines starting "cost" are timings)
#include "Effects/TheaterFxCore.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using TheaterFx::Battle;
using TheaterFx::Bit;
using TheaterFx::Rgb;
using TheaterFx::Vec;

namespace
{
	struct Flat : TheaterFx::Recipe
	{
		std::map<std::string, double> N;
		std::map<std::string, std::string> W;
		bool Find(const char* P, double& O) const override
		{
			auto I = N.find(P);
			if (I == N.end()) return false;
			O = I->second;
			return true;
		}
		bool FindWord(const char* P, std::string& O) const override
		{
			auto I = W.find(P);
			if (I == W.end()) return false;
			O = I->second;
			return true;
		}
	};

	double FlatGround(void*, double, double) { return 0.0; }

	int Faults = 0;
	void Fault(const char* What, const char* Case)
	{
		++Faults;
		std::printf("  FAULT (%s): %s\n", Case, What);
	}

	// The tank, as fxpreview's ricochet shot draws her (facing +X, her middle at the origin at the burst's Now, on flat
	// ground): tracks, lower hull, upper hull, turret, gun. Her side one flat face over her tracks, as a Tiger's (and as
	// LandPlay's plain box): the kit knows only the plate it is told of and the floor under it, so a spark thrown off a
	// plate may cross a part of her standing out beside it that is neither (her gun), never the plate's own hull, her
	// deck under her turret nor her track guard under her front.
	struct Box3 { Vec Lo, Hi; };
	const Box3 Hull[] = {
		{ Vec(-3.2, -1.6, 0.0), Vec(3.2, -1.15, 1.2) },
		{ Vec(-3.2, 1.15, 0.0), Vec(3.2, 1.6, 1.2) },
		{ Vec(-3.0, -1.15, 0.4), Vec(2.6, 1.15, 1.2) },
		{ Vec(-3.2, -1.6, 1.2), Vec(2.2, 1.6, 1.83) },
		{ Vec(-1.6, -1.2, 1.83), Vec(1.2, 1.2, 2.85) },
		{ Vec(1.2, -0.12, 2.1), Vec(5.2, 0.12, 2.34) },
	};
	const int GunBox = 5;
	// (which of her boxes a place is in, -1 none)
	int InHull(const Vec& P)
	{
		const double In = 0.01; // (a centimetre inside a face: on it is not in it)
		for (int K = 0; K < static_cast<int>(sizeof(Hull) / sizeof(Hull[0])); ++K)
		{
			const Box3& B = Hull[K];
			if (P.X > B.Lo.X + In && P.X < B.Hi.X - In && P.Y > B.Lo.Y + In && P.Y < B.Hi.Y - In && P.Z > B.Lo.Z + In && P.Z < B.Hi.Z - In) return K;
		}
		return -1;
	}

	// (FNV-1a over a number printed to nine figures: the same on any compiler that lays the same numbers)
	uint64_t Digest = 1469598103934665603ull;
	void Mix(double V)
	{
		char S[40];
		std::snprintf(S, sizeof(S), "%.9g;", V);
		for (const char* C = S; *C; ++C)
		{
			Digest ^= static_cast<unsigned char>(*C);
			Digest *= 1099511628211ull;
		}
	}
	void MixVec(const Vec& V) { Mix(V.X); Mix(V.Y); Mix(V.Z); }
	void MixRgb(const Rgb& C) { Mix(C.R); Mix(C.G); Mix(C.B); }

	struct Case
	{
		const char* Name;
		double EyeAway, EyeUp;
		Vec EyeWay;
		Vec To, Normal, Gun;
		// (the face struck, as she is at Now: its middle, and how far it runs from it Across and Up it; handed to the kit
		// as the plate's room)
		Vec FaceMid, Across, Up;
		double HalfAcross, HalfUp;
		// (her way (m/s), her turn about her middle (the origin at Now; radians a second, + turning x toward y), her own
		// floor under the plate (-1e30: none, the ground))
		Vec Carry;
		double Spin, Floor;
		bool bSide, bLow, bFloorControl;
	};

	// (a place at moment T, as she was at Now: back along her way and turned back about her middle)
	Vec Back(const Case& C, const Vec& P, double T)
	{
		const Vec Q = P - C.Carry * T;
		const double A = -C.Spin * T;
		return Vec(Q.X * std::cos(A) - Q.Y * std::sin(A), Q.X * std::sin(A) + Q.Y * std::cos(A), Q.Z);
	}
	Vec BackWay(const Case& C, const Vec& W, double T)
	{
		const double A = -C.Spin * T;
		return Vec(W.X * std::cos(A) - W.Y * std::sin(A), W.X * std::sin(A) + W.Y * std::cos(A), W.Z);
	}

	Battle::ArmourPlate PlateOf(const Case& C, bool bFloor)
	{
		Battle::ArmourPlate P;
		P.At = C.To;
		P.Normal = C.Normal;
		P.RoomMid = C.FaceMid;
		P.RoomA = C.Across * C.HalfAcross;
		P.RoomB = C.Up * C.HalfUp;
		P.Carry = C.Carry;
		P.Pivot = Vec(0.0, 0.0, 0.0);
		P.Spin = C.Spin;
		P.Floor = bFloor ? C.Floor : -1.0e30;
		return P;
	}

	Battle::View ViewFrom(const Vec& Eye, const Vec& Look, double Ppr, int W, int H)
	{
		Battle::View V;
		V.Eye = Eye;
		V.Forward = (Look - Eye).Unit();
		V.Right = V.Forward.Cross(Vec(0, 0, 1)).Unit();
		V.Up = V.Right.Cross(V.Forward).Unit();
		V.PixelsPerRadian = Ppr;
		V.HalfAcross = W / (2.0 * Ppr);
		V.HalfUp = H / (2.0 * Ppr);
		return V;
	}

	struct Laid
	{
		size_t Pieces = 0;
		double CallUs = 0.0, DrawMs = 0.0;
		int MostPuffs = 0;
	};

	// (how many pieces, at any moment of their lives, are inside her as she is then or under the ground; and how many
	// only cross her gun, a part standing out beside the plate that the kit is not told of)
	struct Into
	{
		int Hull = 0, Gun = 0;
	};
	Into InsideOf(const Battle& Fx, const Case& C, const std::vector<Bit>& Pieces, bool bShow)
	{
		Into Got;
		for (const Bit& B : Pieces)
		{
			for (double Age = 0.0; Age <= B.Life + 1e-9; Age += 1.0 / 120.0)
			{
				const Vec P = Fx.PieceAtForTest(B, Age);
				Vec Ends[3] = { P, P, P };
				if (B.Form == Bit::Shape::Streak)
				{
					const Vec Later = Fx.PieceAtForTest(B, Age + 1e-3);
					const Vec Way = B.HalfLength > 0.0 && B.Gravity <= 0.0 ? B.Along : (Later - P).Unit(B.Along);
					const double Half = B.HalfLength > 0.0 ? B.HalfLength : B.R0 + (Later - P).Length() / 1e-3 / 60.0;
					Ends[1] = P - Way * Half;
					Ends[2] = P + Way * Half;
				}
				bool bIn = false, bGun = false;
				for (const Vec& E : Ends)
				{
					if (B.bFired && B.Until > 0.0 && (E - B.Origin).Length() > B.Until + 0.01) continue; // (not drawn past its end)
					if (B.bFired && (E - B.Origin).Dot(B.From - B.Origin) < 0.0) continue; // (nor behind its start)
					const int Box = InHull(Back(C, E, B.Born + Age)); // (in her where she is then, or under the ground)
					bIn = bIn || (Box >= 0 && Box != GunBox) || E.Z < -0.01;
					bGun = bGun || Box == GunBox;
				}
				if (bGun && !bIn)
				{
					++Got.Gun;
					break;
				}
				if (bIn)
				{
					++Got.Hull;
					if (bShow) std::printf("    inside at age %.3f: form %d fired %d glow %d born %.3f life %.3f from (%.2f %.2f %.2f) vel (%.1f %.1f %.1f) half %.2f until %.2f origin (%.2f %.2f %.2f) at (%.2f %.2f %.2f)\n", Age, static_cast<int>(B.Form), B.bFired ? 1 : 0, B.Glow, B.Born, B.Life, B.From.X, B.From.Y, B.From.Z, B.Vel.X, B.Vel.Y, B.Vel.Z, B.HalfLength, B.Until, B.Origin.X, B.Origin.Y, B.Origin.Z, P.X, P.Y, P.Z);
					break;
				}
			}
		}
		return Got;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "ricochet_test recipe.txt\n");
		return 2;
	}
	Flat Book;
	{
		std::ifstream F(Argv[1]);
		std::string L;
		while (std::getline(F, L))
		{
			const size_t T = L.find('\t');
			if (T == std::string::npos) continue;
			const std::string K = L.substr(0, T), V = L.substr(T + 1);
			if (!V.empty() && V[0] == '=') Book.W[K] = V.substr(1);
			else Book.N[K] = std::stod(V);
		}
	}
	const double EdgeM = Book.N.count("bullet_ricochet.edge_m") ? Book.N["bullet_ricochet.edge_m"] : 0.03;
	// (the game's picture: 1072 by 670, a 42 degree lens across)
	const int W = 1072, H = 670;
	const double Ppr = W / (2.0 * std::tan(42.0 * 3.14159265358979 / 360.0));
	const int Rounds = 25;
	const double Rate = 15.0;
	const double None = -1.0e30;
	// Her faces: her side over her tracks (upper hull), her side low over her tracks, her front plate (upper hull, her
	// track guards and lower hull standing out under it at 1.2 m), her turret's side (her deck under it at 1.83 m), and her
	// turret's roof.
	const Vec SideMid(-0.5, -1.6, 1.515), LowMid(0.0, -1.6, 0.6), FrontMid(2.2, 0.0, 1.515), TurretMid(-0.2, -1.2, 2.34), RoofMid(-0.2, 0.0, 2.85);
	const Vec X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1), Left(0, -1, 0);
	const Vec FromLeft = Vec(-1.0, -1.0, 0.0).Unit() * 120.0 + Vec(0.0, 0.0, -0.9);
	const Vec FromAhead = Vec(1.0, 0.3, 0.0).Unit() * 120.0 + Vec(0.0, 0.0, -0.9);
	const Vec Side(-0.6, -1.6, 1.5);
	const Vec Roof(-0.2, 0.0, 2.85);
	const Vec RoofGun = Roof + Vec(-1.0, -0.4, 0.0).Unit() * 120.0 + Vec(0.0, 0.0, 30.0);
	const Vec SideEye(0.35, -1.0, 0.0), FrontEye(1.0, -0.35, 0.0);
	const Vec EdgeHi(2.2, 1.28, 1.2 + 0.95 * 0.63), EdgeLo(2.2, -1.28, 1.2 + 0.95 * 0.63); // (u 0.8 and -0.8, h 0.95: 3 cm under her top edge)
	const Vec UnderTop(-0.6, -1.6, 1.73), Foot(-0.2, -1.2, 1.88), TurretSide(-0.2, -1.2, 2.25), Low(-0.6, -1.6, 0.9);
	const Vec Driving(2.2, -0.9, 1.5);
	const Case Cases[] = {
		{ "side, eye 30 m", 30.0, 1.7, SideEye, Side, Left, Side + FromLeft, SideMid, X, Z, 2.7, 0.315, Vec(), 0.0, None, true, false, false },
		{ "side, eye 150 m", 150.0, 6.0, SideEye, Side, Left, Side + FromLeft, SideMid, X, Z, 2.7, 0.315, Vec(), 0.0, None, true, false, false },
		{ "side, eye 600 m", 600.0, 20.0, SideEye, Side, Left, Side + FromLeft, SideMid, X, Z, 2.7, 0.315, Vec(), 0.0, None, true, false, false },
		{ "roof from a rise, eye 30 m", 30.0, 4.0, SideEye, Roof, Z, RoofGun, RoofMid, X, Y, 1.4, 1.2, Vec(), 0.0, None, false, false, false },
		{ "front, 3 cm under her top edge, u 0.8", 30.0, 1.7, FrontEye, EdgeHi, X, EdgeHi + FromAhead, FrontMid, Y, Z, 1.6, 0.315, Vec(), 0.0, 1.2, false, false, false },
		{ "front, 3 cm under her top edge, u -0.8", 30.0, 1.7, FrontEye, EdgeLo, X, EdgeLo + FromAhead, FrontMid, Y, Z, 1.6, 0.315, Vec(), 0.0, 1.2, false, false, false },
		{ "side, 0.1 m under her top edge", 30.0, 1.7, SideEye, UnderTop, Left, UnderTop + FromLeft, SideMid, X, Z, 2.7, 0.315, Vec(), 0.0, None, false, false, false },
		{ "turret's foot, 5 cm over her deck", 30.0, 1.7, SideEye, Foot, Left, Foot + FromLeft, TurretMid, X, Z, 1.4, 0.51, Vec(), 0.0, 1.83, false, false, false },
		{ "turret's side, 0.42 m over her deck", 30.0, 1.7, SideEye, TurretSide, Left, TurretSide + FromLeft, TurretMid, X, Z, 1.4, 0.51, Vec(), 0.0, 1.83, false, false, true },
		{ "side low over her tracks, 0.9 m up", 30.0, 1.7, SideEye, Low, Left, Low + FromLeft, LowMid, X, Z, 3.2, 0.6, Vec(), 0.0, None, false, true, false },
		{ "front, driving at the gun at 5 m/s", 30.0, 1.7, FrontEye, Driving, X, Driving + X * 120.0 + Vec(0.0, 0.0, -0.9), FrontMid, Y, Z, 1.6, 0.315, Vec(5.0, 0.0, 0.0), 0.0, 1.2, false, false, false },
		{ "front, backing from the gun at 5 m/s", 30.0, 1.7, FrontEye, Driving, X, Driving + FromAhead, FrontMid, Y, Z, 1.6, 0.315, Vec(-5.0, 0.0, 0.0), 0.0, 1.2, false, false, false },
		{ "front, pivoting at 0.5 rad/s", 30.0, 1.7, FrontEye, Driving, X, Driving + FromAhead, FrontMid, Y, Z, 1.6, 0.315, Vec(), 0.5, 1.2, false, false, false },
	};
	double NearSparksPer = 0.0, NearWidth = 0.0;
	for (const Case& C : Cases)
	{
		std::printf("%s:\n", C.Name);
		const Vec EyeWay = C.EyeWay.Unit();
		const Vec Eye = C.To + EyeWay * C.EyeAway + Vec(0.0, 0.0, C.EyeUp - C.To.Z + 0.0);
		const Battle::View View = ViewFrom(Eye, C.To, Ppr, W, H);
		Laid Armour, Ground;
		uint64_t CaseDigest = 0;
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			// (pass 0 the burst on her plate; pass 1 the same burst into the ground beside her, Shots as ever, to set the
			// cost against and to learn the ground's dust)
			Battle Fx;
			Fx.Setup(&Book, &FlatGround, nullptr, 3u);
			Fx.SetWind(Vec(1.6, 0.4, 0.0));
			Fx.Draw(-0.1, View); // (the eye is known before the burst, as in the game)
			Laid& Got = Pass == 0 ? Armour : Ground;
			const auto T0 = std::chrono::steady_clock::now();
			if (Pass == 0) Fx.ShotsOnArmour(0.0, 13, C.Gun, PlateOf(C, true), "mg", Rounds, Rate, "earth");
			else Fx.Shots(0.0, 13, C.Gun, Vec(C.To.X, C.To.Y - 4.0, 0.0), "mg", Rounds, Rate, "earth");
			Got.CallUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - T0).count();
			const std::vector<Bit> Pieces = Fx.PiecesForTest();
			Got.Pieces = Pieces.size();
			if (Pass == 1)
			{
				// (draw it as the game would, 60 frames a second for 2.5 s)
				const auto D0 = std::chrono::steady_clock::now();
				for (int F = 0; F <= 150; ++F) Got.MostPuffs = std::max(Got.MostPuffs, Fx.Draw(F / 60.0, View));
				Got.DrawMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - D0).count();
				continue;
			}
			// What was laid on the plate, each piece set against her as she was at Now (back along her way and her turn).
			int Flashes = 0, FlashesOffFace = 0, FlashesInEdge = 0, Sparks = 0, Chips = 0, Incoming = 0, IncomingOnPlate = 0, Ricochets = 0, RicochetsInAngles = 0, SparksOffPlate = 0, SparksLong = 0, SparksIntoPlate = 0, Beads = 0, DustNear = 0;
			double WidestSpark = 0.0, MostSparkLife = 0.0, LeastLift = 90.0, MostLift = 0.0, ShortestPx = 1e9, StrikeOffPlate = 0.0, Moved = 0.0;
			const Vec Gun = C.Gun;
			auto Across = [&](const Vec& P) { return (P - C.FaceMid).Dot(C.Across); };
			auto UpIt = [&](const Vec& P) { return (P - C.FaceMid).Dot(C.Up); };
			auto OffFace = [&](const Vec& P) { return (P - C.FaceMid).Dot(C.Normal); };
			auto OnFace = [&](const Vec& P, double Less) { return std::fabs(Across(P)) <= C.HalfAcross - Less + 1e-9 && std::fabs(UpIt(P)) <= C.HalfUp - Less + 1e-9; };
			Rgb Dust;
			for (const Bit& B : Pieces)
			{
				Mix(B.Born); MixVec(B.From); MixVec(B.Vel); MixVec(B.Along); Mix(B.Life); Mix(B.R0); Mix(B.R1); Mix(B.HalfLength); Mix(B.Until); Mix(B.Alpha); MixRgb(B.Col); MixRgb(B.Col2);
				const Vec FromThen = Back(C, B.From, B.Born);
				const bool bNear = (FromThen - C.To).Length() < 5.0;
				const double Px = Ppr / std::max(1.0, (B.From - Eye).Length()); // (a metre's pixels there)
				if (B.Form == Bit::Shape::Streak && B.bFired)
				{
					// A tracer: one coming in from the gun (its first piece's Origin the muzzle) ends on the plate where it is
					// when the tracer gets there; one going off from the plate leaves it 10 to 50 degrees off it (its first
					// piece: Origin on the plate, not behind it).
					const bool bFirst = B.Glow == 1 && (B.From - B.Origin).Length() < B.HalfLength + 1e-6; // (a tracer's glow, its first piece: one a tracer)
					if ((B.Origin - Gun).Length() < 1.0)
					{
						if (B.Until > 0.0 && bFirst)
						{
							++Incoming;
							const Vec End = B.Origin + (B.From - B.Origin).Unit() * B.Until;
							const Vec EndThen = Back(C, End, B.Born + B.Until / std::max(1.0, B.Vel.Length()));
							if (std::fabs(OffFace(EndThen)) < 0.05 && OnFace(EndThen, 0.0)) ++IncomingOnPlate;
						}
						continue; // (the tracer's own flight to the plate: cut at Until, nothing of it drawn past)
					}
					const Vec OriginThen = Back(C, B.Origin, B.Born);
					if ((OriginThen - C.To).Length() < 1.0 && B.Vel.Length() > 1.0)
					{
						const double Lift = std::asin(std::clamp(BackWay(C, B.Vel, B.Born).Unit().Dot(C.Normal), -1.0, 1.0)) * 180.0 / 3.14159265358979;
						if (OffFace(OriginThen) < 0.03 && bFirst)
						{
							++Ricochets; // (its first piece, from the strike)
							LeastLift = std::min(LeastLift, Lift);
							MostLift = std::max(MostLift, Lift);
							if (Lift >= 9.99 && Lift <= 50.01) ++RicochetsInAngles;
						}
					}
				}
				else if (B.Form == Bit::Shape::Streak && bNear)
				{
					// A spark: its streak as drawn when born (along its flight as seen, her way on its own), the end nearer her
					// plate on the plate's face; short-lived, thrown out of the plate.
					++Sparks;
					const Vec Seen = B.bCarried ? B.Vel + B.Carry : B.Vel;
					const double Half = B.HalfLength > 0.0 ? B.HalfLength : B.R0 + Seen.Length() / 60.0;
					const Vec Way = B.HalfLength > 0.0 ? B.Along : Seen.Unit();
					const Vec EndA = Back(C, B.From - Way * Half, B.Born), EndB = Back(C, B.From + Way * Half, B.Born);
					const Vec Tail = OffFace(EndA) <= OffFace(EndB) ? EndA : EndB;
					if (std::fabs(OffFace(Tail)) > 0.03 || !OnFace(Tail, 0.0)) ++SparksOffPlate;
					if (BackWay(C, B.Vel, B.Born).Dot(C.Normal) <= 0.0) ++SparksIntoPlate;
					if (B.Life >= 0.3) ++SparksLong;
					MostSparkLife = std::max(MostSparkLife, B.Life);
					WidestSpark = std::max(WidestSpark, B.R0);
					// (on the screen: as long as it is drawn at birth, both halves, at least a few pixels; a bead is a spark
					// no longer than it is wide)
					const double LongPx = 2.0 * Half * Px, WidePx = std::max(2.0 * B.R0 * Px, B.LeastPx);
					ShortestPx = std::min(ShortestPx, LongPx);
					if (LongPx < 1.5 * WidePx) ++Beads;
				}
				else if (B.Form == Bit::Shape::Puff && B.bLight && bNear)
				{
					if (B.Alpha >= 0.99)
					{
						// (a round's flash: laid its own width and a centimetre out from where it struck; the strike, as she was
						// at Now, on her face, inside it less edge_m, within 3 cm of her plate)
						++Flashes;
						const Vec Struck = FromThen - C.Normal * (0.6 * B.R0 + 0.01);
						StrikeOffPlate = std::max(StrikeOffPlate, std::fabs(OffFace(Struck)));
						if (!OnFace(Struck, 0.0) || std::fabs(OffFace(Struck)) > 0.03) ++FlashesOffFace;
						if (OnFace(Struck, EdgeM)) ++FlashesInEdge;
						Moved = std::max(Moved, (B.From - FromThen).Length());
					}
				}
				else if (B.Form == Bit::Shape::Puff && !B.bLight && bNear)
				{
					++Chips;
				}
			}
			const Into Inside = InsideOf(Fx, C, Pieces, std::getenv("RICOCHET_SHOW") != nullptr);
			const int InsideHull = Inside.Hull;
			// (the ground's dust: the colour of the spurts the same burst kicks up in the ground, where Shots lays them)
			{
				Battle Ref;
				Ref.Setup(&Book, &FlatGround, nullptr, 3u);
				Ref.Draw(-0.1, View);
				Ref.Shots(0.0, 13, C.Gun, Vec(C.To.X, C.To.Y - 4.0, 0.0), "mg", Rounds, Rate, "earth");
				bool bDust = false;
				for (const Bit& B : Ref.PiecesForTest())
				{
					if (B.Form == Bit::Shape::Puff && B.Gravity == 4.0 && B.Drag == 1.5 && !B.bLight) { Dust = B.Col; bDust = true; break; }
				}
				if (!bDust) Fault("the same burst into the ground kicked up no dust (nothing to compare the plate's with)", C.Name);
				for (const Bit& B : Pieces)
				{
					if (!B.bLight && B.Col.R == Dust.R && B.Col.G == Dust.G && B.Col.B == Dust.B && (Back(C, B.From, B.Born) - C.To).Length() < 5.0) ++DustNear;
				}
			}
			const int Strikes = Fx.RicochetStrikes();
			const int Spurts = Fx.SpurtsForTest();
			const double SparksPer = Strikes ? static_cast<double>(Sparks) / Strikes : 0.0;
			std::printf("  %d rounds struck the plate (%d flashes: %d off her face, %d inside it less edge_m; the farthest %.4f m off her plate at its moment, her plate moved on as far as %.2f m), %d sparks (%.1f a round, the widest %.3f m, the longest-lived %.3f s, the shortest drawn %.1f px), %d chips of paint; %d tracers came in (%d ended on the plate), %d went off again, %.1f to %.1f degrees off the plate\n",
				Strikes, Flashes, FlashesOffFace, FlashesInEdge, StrikeOffPlate, Moved, Sparks, SparksPer, WidestSpark, MostSparkLife, ShortestPx, Chips, Incoming, IncomingOnPlate, Ricochets, Ricochets ? LeastLift : 0.0, Ricochets ? MostLift : 0.0);
			std::printf("  off the plate %d, into it %d, alive 0.3 s or more %d, beads %d; dust spurts %d, dust by her %d; inside her hull or under the ground %d (and %d crossing her gun only)\n", SparksOffPlate, SparksIntoPlate, SparksLong, Beads, Spurts, DustNear, InsideHull, Inside.Gun);
			if (Strikes != Rounds) Fault("not every round struck the plate", C.Name);
			if (Flashes != Strikes) Fault("not one flash a round struck", C.Name);
			if (FlashesOffFace) Fault("a round struck off her face, or more than 3 cm off her plate where it was when it struck", C.Name);
			if (FlashesInEdge != Flashes) Fault("a round struck nearer her plate's edge than edge_m", C.Name);
			if (Fx.RicochetSparks() != Sparks) Fault("the kit's count of sparks is not what was laid by the plate", C.Name);
			if (SparksOffPlate) Fault("a spark began off the plate's face", C.Name);
			if (SparksIntoPlate) Fault("a spark was thrown into the plate", C.Name);
			if (SparksLong) Fault("a spark lived 0.3 s or more", C.Name);
			if (Beads) Fault("a spark is drawn no longer than it is wide (a bead)", C.Name);
			if (Sparks < Strikes) Fault("a round struck and threw no spark (a far burst pops)", C.Name);
			if (Incoming != IncomingOnPlate || Incoming == 0) Fault("a tracer coming in did not end on the plate", C.Name);
			if (Ricochets == 0 || Ricochets > Incoming) Fault("no tracer went off as a ricochet, or more than came in", C.Name);
			if (RicochetsInAngles != Ricochets) Fault("a ricochet left the plate under 10 or over 50 degrees off it", C.Name);
			if (Spurts) Fault("a dust spurt off armour (the kit's own count)", C.Name);
			if (DustNear) Fault("a dust spurt by the tank", C.Name);
			if (InsideHull) Fault("a piece inside the hull or under the ground", C.Name);
			if (C.bLow)
			{
				// (the same burst as Shots into the same place, under 1.5 m up: it does kick dust, so a spurt off armour would
				// be counted)
				Battle Ref;
				Ref.Setup(&Book, &FlatGround, nullptr, 3u);
				Ref.Draw(-0.1, View);
				Ref.Shots(0.0, 13, C.Gun, C.To, "mg", Rounds, Rate, "earth");
				std::printf("  the same burst as Shots into the same place: %d dust spurts\n", Ref.SpurtsForTest());
				if (Ref.SpurtsForTest() == 0) Fault("the same burst as Shots kicked no spurt there (the count would not see one)", C.Name);
			}
			if (C.bFloorControl)
			{
				// (the same burst with no floor handed over: some of its sparks go through her deck, so the floor is what
				// keeps them out)
				Battle Ref;
				Ref.Setup(&Book, &FlatGround, nullptr, 3u);
				Ref.SetWind(Vec(1.6, 0.4, 0.0));
				Ref.Draw(-0.1, View);
				Ref.ShotsOnArmour(0.0, 13, C.Gun, PlateOf(C, false), "mg", Rounds, Rate, "earth");
				const int Through = InsideOf(Ref, C, Ref.PiecesForTest(), false).Hull;
				std::printf("  the same burst with no floor handed over: %d pieces through her deck\n", Through);
				if (Through == 0) Fault("with no floor no spark went through her deck (the check would not see one)", C.Name);
			}
			if (C.bSide && C.EyeAway < 31.0)
			{
				NearSparksPer = SparksPer;
				NearWidth = WidestSpark;
				if (SparksPer < 3.0 || SparksPer > 8.0) Fault("close in, not 3 to 8 sparks a round", C.Name);
			}
			if (C.bSide && C.EyeAway > 500.0 && !(SparksPer < NearSparksPer && WidestSpark > NearWidth)) Fault("far off, the sparks are not fewer and bigger than close in", C.Name);
			CaseDigest = Digest;
			const auto D0 = std::chrono::steady_clock::now();
			for (int F = 0; F <= 150; ++F) Got.MostPuffs = std::max(Got.MostPuffs, Fx.Draw(F / 60.0, View));
			Got.DrawMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - D0).count();
		}
		std::printf("  pieces laid for the burst: %zu on the plate, %zu into the ground; the most puffs a frame: %d on the plate, %d into the ground; digest %016llx\n",
			Armour.Pieces, Ground.Pieces, Armour.MostPuffs, Ground.MostPuffs, static_cast<unsigned long long>(CaseDigest));
		std::printf("cost (%s): the call %.0f us on the plate, %.0f us into the ground; drawing its 151 frames %.2f ms on the plate, %.2f ms into the ground\n",
			C.Name, Armour.CallUs, Ground.CallUs, Armour.DrawMs, Ground.DrawMs);
		if (Armour.MostPuffs > 4 * std::max(1, Ground.MostPuffs) + 400) Fault("the burst on the plate draws far more than the same burst into the ground", C.Name);
	}
	std::printf("digest of everything laid: %016llx\n", static_cast<unsigned long long>(Digest));
	std::printf(Faults ? "FAIL: %d faults\n" : "all checks pass\n", Faults);
	return Faults ? 1 : 0;
}
