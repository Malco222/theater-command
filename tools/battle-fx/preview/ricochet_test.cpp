// Effects (R2), small arms off armour (Brian, 25 November: "bullets should be bouncing off it"): the kit's own check of
// ShotsOnArmour, the game's code (unreal/.../Effects/TheaterFxCore.cpp) with no painter. A machine gun 120 m off fires a
// burst of 25 rounds at 15 a second into a tank's side, 45 degrees off square, the eye 30 m, 150 m and 600 m off; and
// once from a rise into her turret's roof. For each it checks:
//   - every round strikes the plate, on it: its flash, and sparks whose streaks start on the plate's face;
//   - the sparks are short and gone in under 0.3 s, thrown out of the plate, and far off fewer, bigger and drawn out to
//     a few pixels (never beads), never none (never pops);
//   - a few tracers go off again as ricochets, 10 to 50 degrees off the plate; every tracer coming in ends on it;
//   - no dust spurt at the tank (no piece of the ground's dust within 5 m of her);
//   - nothing inside the hull or the ground: every piece, all its life (a streak's both ends, a tracer to where it is
//     cut), outside every box of her and above the ground;
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

	// The tank, as fxpreview's ricochet shot draws her (facing +X, her middle at the origin, on flat ground): tracks,
	// lower hull, upper hull, turret, gun. Her side one flat face over the tracks, as a Tiger's (and as LandPlay's plain
	// box): the kit knows only the plate it is told of, so a spark thrown down off a plate may cross a track guard
	// standing out below it (the wreck shot's tank has one 15 cm wide), never the plate's own hull behind it.
	struct Box3 { Vec Lo, Hi; };
	const Box3 Hull[] = {
		{ Vec(-3.2, -1.6, 0.0), Vec(3.2, -1.15, 1.2) },
		{ Vec(-3.2, 1.15, 0.0), Vec(3.2, 1.6, 1.2) },
		{ Vec(-3.0, -1.15, 0.4), Vec(2.6, 1.15, 1.2) },
		{ Vec(-3.2, -1.6, 1.2), Vec(2.2, 1.6, 1.83) },
		{ Vec(-1.6, -1.2, 1.83), Vec(1.2, 1.2, 2.85) },
		{ Vec(1.2, -0.12, 2.1), Vec(5.2, 0.12, 2.34) },
	};
	bool InHull(const Vec& P)
	{
		const double In = 0.01; // (a centimetre inside a face: on it is not in it)
		for (const Box3& B : Hull)
		{
			if (P.X > B.Lo.X + In && P.X < B.Hi.X - In && P.Y > B.Lo.Y + In && P.Y < B.Hi.Y - In && P.Z > B.Lo.Z + In && P.Z < B.Hi.Z - In) return true;
		}
		return false;
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
		Vec To, Normal, Gun;
		// (the plate struck: its face, Across along it and Up it, each a span)
		double AcrossLo, AcrossHi, UpLo, UpHi;
		Vec Across, Up;
	};

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
	// (the game's picture: 1072 by 670, a 42 degree lens across)
	const int W = 1072, H = 670;
	const double Ppr = W / (2.0 * std::tan(42.0 * 3.14159265358979 / 360.0));
	const int Rounds = 25;
	const double Rate = 15.0;
	const Vec Side(-0.6, -1.6, 1.5), Out(0.0, -1.0, 0.0);
	const Vec SideGun = Side + Vec(-1.0, -1.0, 0.0).Unit() * 120.0 + Vec(0.0, 0.0, -0.9);
	const Vec Roof(-0.2, 0.0, 2.85);
	const Vec RoofGun = Roof + Vec(-1.0, -0.4, 0.0).Unit() * 120.0 + Vec(0.0, 0.0, 30.0);
	const Case Cases[] = {
		{ "side, eye 30 m", 30.0, 1.7, Side, Out, SideGun, -3.2, 2.2, 1.2, 1.83, Vec(1, 0, 0), Vec(0, 0, 1) },
		{ "side, eye 150 m", 150.0, 6.0, Side, Out, SideGun, -3.2, 2.2, 1.2, 1.83, Vec(1, 0, 0), Vec(0, 0, 1) },
		{ "side, eye 600 m", 600.0, 20.0, Side, Out, SideGun, -3.2, 2.2, 1.2, 1.83, Vec(1, 0, 0), Vec(0, 0, 1) },
		{ "roof from a rise, eye 30 m", 30.0, 4.0, Roof, Vec(0, 0, 1), RoofGun, -1.6, 1.2, -1.2, 1.2, Vec(1, 0, 0), Vec(0, 1, 0) },
	};
	double NearSparksPer = 0.0, NearWidth = 0.0;
	for (const Case& C : Cases)
	{
		std::printf("%s:\n", C.Name);
		const Vec EyeWay = Vec(0.35, -1.0, 0.0).Unit();
		const Vec Eye = C.To + EyeWay * C.EyeAway + Vec(0.0, 0.0, C.EyeUp - C.To.Z + 0.0);
		const Battle::View View = ViewFrom(Eye, C.To, Ppr, W, H);
		const double Away = (Eye - C.To).Length();
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
			if (Pass == 0) Fx.ShotsOnArmour(0.0, 13, C.Gun, C.To, C.Normal, "mg", Rounds, Rate, "earth");
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
			// What was laid on the plate.
			int Flashes = 0, Sparks = 0, Chips = 0, Incoming = 0, IncomingOnPlate = 0, Ricochets = 0, RicochetsInAngles = 0, InsideHull = 0, SparksOffPlate = 0, SparksLong = 0, SparksIntoPlate = 0, Beads = 0, DustNear = 0;
			double WidestSpark = 0.0, MostSparkLife = 0.0, LeastLift = 90.0, MostLift = 0.0, ShortestPx = 1e9;
			const Vec Gun = C.Gun;
			Rgb Dust;
			for (const Bit& B : Pieces)
			{
				Mix(B.Born); MixVec(B.From); MixVec(B.Vel); MixVec(B.Along); Mix(B.Life); Mix(B.R0); Mix(B.R1); Mix(B.HalfLength); Mix(B.Until); Mix(B.Alpha); MixRgb(B.Col); MixRgb(B.Col2);
				const bool bNear = (B.From - C.To).Length() < 5.0;
				const double Px = Ppr / std::max(1.0, (B.From - Eye).Length()); // (a metre's pixels there)
				if (B.Form == Bit::Shape::Streak && B.bFired)
				{
					// A tracer: one coming in from the gun (its first piece's Origin the muzzle) ends on the plate; one going off
					// from the plate leaves it 10 to 50 degrees off it (its first piece: Origin on the plate, not behind it).
					const bool bFirst = B.Glow == 1 && (B.From - B.Origin).Length() < B.HalfLength + 1e-6; // (a tracer's glow, its first piece: one a tracer)
					if ((B.Origin - Gun).Length() < 1.0)
					{
						if (B.Until > 0.0 && bFirst)
						{
							++Incoming;
							const Vec End = B.Origin + (B.From - B.Origin).Unit() * B.Until;
							const double OffFace = (End - C.To).Dot(C.Normal);
							const double A = (End - C.To).Dot(C.Across) + C.To.Dot(C.Across), U = (End - C.To).Dot(C.Up) + C.To.Dot(C.Up);
							if (std::fabs(OffFace) < 0.05 && A >= C.AcrossLo && A <= C.AcrossHi && U >= C.UpLo && U <= C.UpHi) ++IncomingOnPlate;
						}
						continue; // (the tracer's own flight to the plate: cut at Until, nothing of it drawn past)
					}
					if ((B.Origin - C.To).Length() < 1.0 && B.Vel.Length() > 1.0)
					{
						const double Lift = std::asin(std::clamp(B.Vel.Unit().Dot(C.Normal), -1.0, 1.0)) * 180.0 / 3.14159265358979;
						if ((B.Origin - C.To).Dot(C.Normal) < 0.03 && bFirst)
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
					// A spark: its tail where it was born on the plate's face, short-lived, thrown out of the plate.
					++Sparks;
					const double Speed = B.Vel.Length();
					const double Half = B.HalfLength > 0.0 ? B.HalfLength : B.R0 + Speed / 60.0;
					const Vec Way = B.HalfLength > 0.0 ? B.Along : B.Vel.Unit();
					const Vec Tail = B.From - Way * Half;
					const double OffFace = (Tail - C.To).Dot(C.Normal);
					const double A = Tail.Dot(C.Across), U = Tail.Dot(C.Up);
					if (std::fabs(OffFace) > 0.03 || A < C.AcrossLo || A > C.AcrossHi || U < C.UpLo || U > C.UpHi) ++SparksOffPlate;
					if (B.Vel.Dot(C.Normal) <= 0.0) ++SparksIntoPlate;
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
					if (B.Alpha >= 0.99) ++Flashes;
				}
				else if (B.Form == Bit::Shape::Puff && !B.bLight && bNear)
				{
					++Chips;
				}
				// Nothing inside the hull: the piece's middle every 1/120 s of its life, and a streak's two ends (a tracer only
				// as far as it is drawn, to Until from its Origin).
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
					bool bIn = false;
					for (const Vec& E : Ends)
					{
						if (B.bFired && B.Until > 0.0 && (E - B.Origin).Length() > B.Until + 0.01) continue; // (not drawn past its end)
						if (B.bFired && (E - B.Origin).Dot(B.From - B.Origin) < 0.0) continue; // (nor behind its start)
						bIn = bIn || InHull(E) || E.Z < -0.01; // (or under the ground)
					}
					if (bIn)
					{
						++InsideHull;
						if (std::getenv("RICOCHET_SHOW")) std::printf("    inside at age %.3f: form %d fired %d glow %d born %.3f life %.3f from (%.2f %.2f %.2f) vel (%.1f %.1f %.1f) half %.2f until %.2f origin (%.2f %.2f %.2f) at (%.2f %.2f %.2f)\n", Age, static_cast<int>(B.Form), B.bFired ? 1 : 0, B.Glow, B.Born, B.Life, B.From.X, B.From.Y, B.From.Z, B.Vel.X, B.Vel.Y, B.Vel.Z, B.HalfLength, B.Until, B.Origin.X, B.Origin.Y, B.Origin.Z, P.X, P.Y, P.Z);
						break;
					}
				}
			}
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
					if (!B.bLight && B.Col.R == Dust.R && B.Col.G == Dust.G && B.Col.B == Dust.B && (B.From - C.To).Length() < 5.0) ++DustNear;
				}
			}
			const int Strikes = Fx.RicochetStrikes();
			const double SparksPer = Strikes ? static_cast<double>(Sparks) / Strikes : 0.0;
			std::printf("  %d rounds struck the plate (%d flashes), %d sparks (%.1f a round, the widest %.3f m, the longest-lived %.3f s, the shortest drawn %.1f px), %d chips of paint; %d tracers came in (%d ended on the plate), %d went off again, %.1f to %.1f degrees off the plate\n",
				Strikes, Flashes, Sparks, SparksPer, WidestSpark, MostSparkLife, ShortestPx, Chips, Incoming, IncomingOnPlate, Ricochets, Ricochets ? LeastLift : 0.0, Ricochets ? MostLift : 0.0);
			std::printf("  off the plate %d, into it %d, alive 0.3 s or more %d, beads %d; dust by her %d; inside her hull or under the ground %d\n", SparksOffPlate, SparksIntoPlate, SparksLong, Beads, DustNear, InsideHull);
			if (Strikes != Rounds) Fault("not every round struck the plate", C.Name);
			if (Flashes != Strikes) Fault("not one flash a round struck", C.Name);
			if (Fx.RicochetSparks() != Sparks) Fault("the kit's count of sparks is not what was laid by the plate", C.Name);
			if (SparksOffPlate) Fault("a spark began off the plate's face", C.Name);
			if (SparksIntoPlate) Fault("a spark was thrown into the plate", C.Name);
			if (SparksLong) Fault("a spark lived 0.3 s or more", C.Name);
			if (Beads) Fault("a spark is drawn no longer than it is wide (a bead)", C.Name);
			if (Sparks < Strikes) Fault("a round struck and threw no spark (a far burst pops)", C.Name);
			if (Incoming != IncomingOnPlate || Incoming == 0) Fault("a tracer coming in did not end on the plate", C.Name);
			if (Ricochets == 0 || Ricochets > Incoming) Fault("no tracer went off as a ricochet, or more than came in", C.Name);
			if (RicochetsInAngles != Ricochets) Fault("a ricochet left the plate under 10 or over 50 degrees off it", C.Name);
			if (DustNear) Fault("a dust spurt by the tank", C.Name);
			if (InsideHull) Fault("a piece inside the hull or under the ground", C.Name);
			if (C.EyeAway < 31.0 && C.Normal.Z == 0.0)
			{
				NearSparksPer = SparksPer;
				NearWidth = WidestSpark;
				if (SparksPer < 3.0 || SparksPer > 8.0) Fault("close in, not 3 to 8 sparks a round", C.Name);
			}
			if (C.EyeAway > 500.0 && !(SparksPer < NearSparksPer && WidestSpark > NearWidth)) Fault("far off, the sparks are not fewer and bigger than close in", C.Name);
			CaseDigest = Digest;
			const auto D0 = std::chrono::steady_clock::now();
			for (int F = 0; F <= 150; ++F) Got.MostPuffs = std::max(Got.MostPuffs, Fx.Draw(F / 60.0, View));
			Got.DrawMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - D0).count();
			(void)Away;
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
