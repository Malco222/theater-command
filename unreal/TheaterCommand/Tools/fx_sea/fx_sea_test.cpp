// The effects kit's sea items of release 2's ten dramatic moments (items 9, 10 and 11: TheaterFxSea.inl), tested in the
// cloud with no engine: DeckDrench, DeckWash and the ship's frame (ShipRide), the oil lane, the deck's brass, and the AA
// guns' hanging haze (the overlay battle-fx-sea-haze.json). Every check prints ok or FAIL with what it measured; the
// time lines (cost) are kept out of the compilers' comparison.
//
//     fx_sea_test <battle-fx.json> <battle-fx-sea-haze.json> <out folder>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define private public
#include "Effects/TheaterFxCore.h"
#undef private
#include "../fx_lamps/fx_lamps_recipe.h"

namespace
{
	using FxLamps::JsonRecipe;
	using TheaterFx::Battle;
	using TheaterFx::Bit;
	using TheaterFx::Vec;

	int Checks = 0, Failed = 0;
	bool bTimeFailed = false;
	const double Pi = 3.14159265358979323846;

	void Check(bool bOk, const char* What, const std::string& Got)
	{
		++Checks;
		Failed += bOk ? 0 : 1;
		std::printf("  %s %-66s %s\n", bOk ? "ok  " : "FAIL", What, Got.c_str());
	}

	std::string Say(const char* Format, double A = 0.0, double B = 0.0, double C = 0.0, double D = 0.0, double E = 0.0)
	{
		char Text[512];
		std::snprintf(Text, sizeof(Text), Format, A, B, C, D, E);
		return Text;
	}

	[[maybe_unused]] double Since(const std::chrono::steady_clock::time_point& T0)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	}

	/** A battle on a flat sea at height 0, the kit's own seed 3, a light wind across. */
	void Open(Battle& X, const JsonRecipe& Book)
	{
		X.Setup(&Book, &FxLamps::Flat, nullptr, 3);
		X.SetWind(Vec(1.5, 0.5, 0.0));
		X.UseSea();
	}

	/** Every number of a piece the draw reads, written exactly (so two pieces' prints match only if they are the same). */
	std::string Print(const Bit& B)
	{
		char T[640];
		std::snprintf(T, sizeof(T), "%d %a %a %a %a %a %a %a %a %a %a %a %a %a %a %a %a %a %a %a %u",
			static_cast<int>(B.Form), B.From.X, B.From.Y, B.From.Z, B.Vel.X, B.Vel.Y, B.Vel.Z, B.Born, B.Life, B.R0, B.R1, B.Alpha,
			static_cast<double>(B.Col.R), static_cast<double>(B.Col.G), static_cast<double>(B.Col.B), B.Gravity, B.Drag, B.Windage, B.Stretch, B.Turn, B.LumpSeed);
		return T;
	}

	/** An unrelated effect laid after Then (a howitzer shell on the sea 300 m off and a 5-inch gun's muzzle), its pieces
	 * printed: the same with and without Then, or Then moved a draw on the battle's dice. */
	template <typename F>
	bool DiceUntouched(const JsonRecipe& Book, F&& Then, std::string& Got)
	{
		Battle A, B;
		Open(A, Book);
		Open(B, Book);
		const uint64_t Was = A.Seeds;
		Then(A);
		const uint64_t Is = A.Seeds;
		const size_t FromA = A.Bits.size(), FromB = B.Bits.size();
		for (Battle* X : { &A, &B })
		{
			X->Shell(30.0, Vec(300.0, 40.0, 0.0), "how105", "water", false);
			X->Muzzle(30.5, Vec(280.0, 60.0, 9.0), Vec(0.3, 0.2, 0.9), "ship_aa", "water");
		}
		const size_t NA = A.Bits.size() - FromA, NB = B.Bits.size() - FromB;
		bool bSame = NA == NB && NA > 0;
		for (size_t I = 0; bSame && I < NA; ++I)
		{
			bSame = Print(A.Bits[FromA + I]) == Print(B.Bits[FromB + I]);
		}
		Got = Say("the shared seed %.0f before, %.0f after; the shell and muzzle after it %.0f pieces, %.0f without it", static_cast<double>(Was), static_cast<double>(Is), static_cast<double>(NA), static_cast<double>(NB));
		Got += bSame ? ", every one the same to the bit" : ", NOT the same";
		return bSame && Was == Is;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// A. DeckDrench (item 9 part 4)

	void Drench(const JsonRecipe& Book)
	{
		std::printf("\nA. DeckDrench: a 16-inch column 8 m off a battleship's side (a hull 33 m across, her deck 8 m up, heading +X)\n");
		const double HalfBeam = 16.5, DeckZ = 8.0, Off = 8.0;
		const Vec Column(0.0, -(HalfBeam + Off), 0.0);
		const Vec Toward(0.0, Off, 0.0);
		Battle X;
		Open(X, Book);
		const double Now = 10.0;
		X.SeaSplash(Now, Column, 406.0, "");
		const size_t First = X.Bits.size();
		const int Laid = X.DeckDrench(Now, Column, 406.0, Toward, DeckZ, 7);
		const double H = X.SeaTable("height_m", 406.0, 0.0), Rise = X.SeaTable("rise_s", 406.0, 0.0), Hang = X.SeaTable("hang_s", 406.0, 0.0), Fall = X.SeaTable("fall_s", 406.0, 0.0);
		const double Reach = X.Num("sea_splash.drench.reach_k", 0.0) * H, Life = X.Num("sea_splash.drench.life", 0.0);
		std::printf("    the column: %.1f m, rising %.2f s, standing %.2f s, falling %.2f s; the drench reaches %.1f m from it, %.1f m across her deck\n", H, Rise, Hang, Fall, Reach, Reach - Off);
		int Falling = 0, Sheets = 0, OnDeckSide = 0, InReach = 0, AtDeck = 0, Late = 0;
		double FirstBorn = 1e18, LastGone = 0.0, Nearest = 1e18, Farthest = -1e18;
		for (size_t I = First; I < X.Bits.size(); ++I)
		{
			const Bit& B = X.Bits[I];
			FirstBorn = std::min(FirstBorn, B.Born);
			LastGone = std::max(LastGone, B.Born + B.Life);
			if (B.Form == Bit::Shape::Puff && B.StretchBy > 0.0)
			{
				// (a lump of the head: where it is when it comes down)
				++Falling;
				const Vec P = X.Where(B, B.Life);
				const double Across = P.Y - Column.Y; // (from the column toward her)
				Nearest = std::min(Nearest, Across);
				Farthest = std::max(Farthest, Across);
				OnDeckSide += Across > Off - 1.0 ? 1 : 0; // (on her side of her near edge, a metre of wind allowed)
				InReach += Across < Reach + 1.0 ? 1 : 0;
				AtDeck += std::fabs(P.Z - DeckZ) < 0.05 ? 1 : 0;
				Late += B.Born >= Now + Rise + 0.3 * Hang ? 1 : 0;
			}
			else if (B.Form == Bit::Shape::Flat)
			{
				++Sheets;
				AtDeck += 0;
			}
		}
		Check(Laid > 0 && Falling > 10 && Sheets == Falling, "the head falls as lumps, each leaving a sheet on her deck", Say("%.0f pieces: %.0f lumps, %.0f sheets", Laid, Falling, Sheets));
		Check(OnDeckSide == Falling && InReach == Falling, "every lump lands on her side of her near edge, within its reach", Say("%.1f to %.1f m from the column (her edge %.0f m, the reach %.1f m)", Nearest, Farthest, Off, Reach));
		Check(AtDeck == Falling, "  ...at her deck's height", Say("%.0f of %.0f within 5 cm of %.1f m", AtDeck, Falling, DeckZ));
		Check(Late == Falling && FirstBorn >= Now + Rise + 0.3 * Hang, "timed after the column's rise and most of its hang", Say("the first lump leaves %.2f s after the shell (rise %.2f + hang %.2f)", FirstBorn - Now, Rise, Hang));
		Check(LastGone <= Now + Rise + Hang + Fall + Life + 0.01, "all of it gone within rise + hang + fall + its life", Say("the last piece gone %.2f s after the shell (%.2f)", LastGone - Now, Rise + Hang + Fall + Life));
		const int Second = X.DeckDrench(Now + 0.5, Column + Vec(30.0, 0.0, 0.0), 406.0, Toward, DeckZ, 7);
		const int Third = X.DeckDrench(Now + 0.7, Column + Vec(-30.0, 0.0, 0.0), 406.0, Toward, DeckZ, 7);
		const int Other = X.DeckDrench(Now + 0.7, Column + Vec(400.0, 0.0, 0.0), 406.0, Toward, DeckZ, 8);
		const int AfterAll = X.DeckDrench(Now + 30.0, Column, 406.0, Toward, DeckZ, 7);
		Check(Second > 0 && Third == 0 && Other > 0 && AfterAll > 0, "at most 2 at once on a ship; another ship has her own", Say("the second %.0f pieces, the third %.0f, another ship's %.0f; once the first are down, %.0f", Second, Third, Other, AfterAll));
		Battle Y;
		Open(Y, Book);
		const int Beyond = Y.DeckDrench(Now, Vec(0.0, -(HalfBeam + Reach + 1.0), 0.0), 406.0, Vec(0.0, Reach + 1.0, 0.0), DeckZ, 7);
		const int Small = Y.DeckDrench(Now, Column, 76.0, Toward, DeckZ, 9); // (a 3-inch's 9 m column reaches 3 m: not 8)
		const int Close = Y.DeckDrench(Now, Vec(0.0, -(HalfBeam + 3.0), 0.0), 76.0, Vec(0.0, 3.0, 0.0), DeckZ, 10);
		Check(Beyond == 0 && Small == 0, "nothing when her edge is beyond the column's reach", Say("a 16-inch's column %.1f m off: %.0f pieces; a 3-inch's 8 m off: %.0f", Reach + 1.0, Beyond, Small));
		Check(Close == 0, "  ...a 3-inch's 9 m column 3 m off (reach 3.2 m) lays nothing either", Say("%.0f pieces", Close));
		std::string Got;
		const bool bSame = DiceUntouched(Book, [&](Battle& Z) { Z.DeckDrench(Now, Column, 406.0, Toward, DeckZ, 7); }, Got);
		Check(bSame, "no draw on the shared seed", Got);
		JsonRecipe Without = Book;
		for (auto It = Without.Numbers.begin(); It != Without.Numbers.end();)
		{
			It = It->first.rfind("sea_splash.drench.", 0) == 0 ? Without.Numbers.erase(It) : std::next(It);
		}
		Battle W;
		Open(W, Without);
		Check(W.DeckDrench(Now, Column, 406.0, Toward, DeckZ, 7) == 0, "without the row: nothing", "");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// A ship for the ship's frame: a battleship 250 m long, 33 m across, steaming at 10 m/s, turning 1.7 degrees a second
	// and rolling 6 degrees either way every 10 s (heel about her bow's axis), from the origin heading +X

	Battle::ShipPose Pose(double T, double Turn = 0.03, double Roll = 6.0, double List = 0.0, double Mps = 10.0)
	{
		const double Psi = Turn * T;
		const double Phi = (List + Roll * std::sin(2.0 * Pi * T / 10.0)) * Pi / 180.0;
		Battle::ShipPose P;
		// (her track: a circle of radius Mps / Turn, or a straight line when she does not turn)
		P.At = Turn != 0.0 ? Vec(Mps / Turn * std::sin(Psi), Mps / Turn * (1.0 - std::cos(Psi)), 0.0) : Vec(Mps * T, 0.0, 0.0);
		P.Fore = Vec(std::cos(Psi), std::sin(Psi), 0.0);
		const Vec L0(0.0, std::cos(Phi), std::sin(Phi)), U0(0.0, -std::sin(Phi), std::cos(Phi));
		auto Yaw = [&](const Vec& V) { return Vec(V.X * std::cos(Psi) - V.Y * std::sin(Psi), V.X * std::sin(Psi) + V.Y * std::cos(Psi), V.Z); };
		P.Left = Yaw(L0);
		P.Up = Yaw(U0);
		P.Long = 250.0;
		P.HalfBeam = 16.5;
		P.Coaming = 0.0;
		return P;
	}

	/** A frame drawn from 300 m off her starboard beam, 20 m up. */
	int Look(Battle& X, double Now, const Vec& At)
	{
		Battle::View V;
		V.Eye = At + Vec(0.0, -300.0, 20.0);
		V.Forward = (At - V.Eye).Unit();
		V.Right = V.Forward.Cross(Vec(0.0, 0.0, 1.0)).Unit(Vec(1.0, 0.0, 0.0));
		V.Up = V.Right.Cross(V.Forward).Unit();
		V.PixelsPerRadian = 1600.0 / (2.0 * std::tan(0.5 * 55.0 * Pi / 180.0));
		V.HalfAcross = std::tan(0.5 * 55.0 * Pi / 180.0);
		return X.Draw(Now, V);
	}

	/** Where a piece in her frame should be now, worked out here from its own numbers and her pose (not the kit's). */
	Vec Expected(const Bit& B, const Battle::ShipPose& P, double Now)
	{
		const double A = std::max(Now - B.Born, 0.0);
		Vec L = B.DeckP;
		if (B.DeckMode == 1)
		{
			L = B.DeckP + B.DeckV * A + Vec(0.0, 0.0, -0.5 * B.DeckG * A * A);
		}
		else if (B.DeckMode == 2)
		{
			L = B.DeckP + B.DeckV * (1.0 - std::exp(-A));
		}
		else
		{
			L = B.DeckP;
		}
		// (the pose made square as the kit makes it)
		const Vec Up = P.Up.Unit(), Fore = (P.Fore - Up * P.Fore.Dot(Up)).Unit(), Left = Up.Cross(Fore).Unit();
		return P.At + Fore * L.X + Left * L.Y + Up * L.Z;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// B. DeckWash and the ship's frame (item 9 part 5, item 10 part 6)

	void Wash(const JsonRecipe& Book)
	{
		std::printf("\nB. DeckWash: runoff along her starboard edge (80 m of it, 8 m freeboard) of a battleship turning 1.7 deg/s at 10 m/s, rolling 6 deg\n");
		const Vec Edge[3] = { Vec(-40.0, -16.5, 8.0), Vec(0.0, -16.5, 8.0), Vec(40.0, -16.5, 8.0) };
		Battle X;
		Open(X, Book);
		const double Fps = 30.0;
		X.ShipRide(0.0, 1, Pose(0.0));
		const int Laid = X.DeckWash(0.0, 1, Edge, 3, 1.0, 70.0, "runoff");
		double Worst = 0.0, Moved = 0.0;
		int Seen = 0, Over = 0;
		size_t Most = 0;
		for (int F = 1; F <= static_cast<int>(60.0 * Fps); ++F)
		{
			const double T = F / Fps;
			X.ShipRide(T, 1, Pose(T));
			Look(X, T, Pose(T).At);
			size_t Alive = 0;
			for (const Bit& B : X.Bits)
			{
				if (B.DeckShip == 0 || B.Born > T)
				{
					continue;
				}
				++Alive;
				if (F % 30 == 0 || F == static_cast<int>(60.0 * Fps))
				{
					const double Err = (X.Where(B, T - B.Born) - Expected(B, Pose(T), T)).Length();
					Worst = std::max(Worst, Err);
					++Seen;
					// (and how far it would be from her had it been left where it was laid: what riding saves)
					Moved = std::max(Moved, (Expected(B, Pose(B.Born), B.Born) - Expected(B, Pose(T), T)).Length());
					const double Age = T - B.Born;
					Over += B.DeckMode == 1 && B.DeckP.Z + B.DeckV.Z * Age - 0.5 * B.DeckG * Age * Age < -0.3 ? 1 : 0; // (in her frame)
				}
			}
			Most = std::max(Most, Alive);
		}
		Check(Laid == 1 && Seen > 100, "the wash laid, sheets and foam alive through the minute", Say("%.0f pieces checked at once a second, at most %.0f alive", Seen, static_cast<double>(Most)));
		Check(Worst < 0.01, "every piece rides her frame: error under 1 cm after 60 s", Say("the worst %.2g m (a piece left in the world would be up to %.1f m off)", Worst, Moved));
		Check(Over == 0, "  ...no sheet falls past her waterline (in her frame)", Say("%.0f more than 0.3 m under it", Over));
		// (strength scales the pieces: 10 s at 1, at 0.5, at 0)
		double Count[3] = { 0.0, 0.0, 0.0 };
		const double Strengths[3] = { 1.0, 0.5, 0.0 };
		for (int K = 0; K < 3; ++K)
		{
			Battle Y;
			Open(Y, Book);
			Y.ShipRide(0.0, 1, Pose(0.0));
			Y.DeckWash(0.0, 1, Edge, 3, Strengths[K], 10.0, "runoff");
			const size_t Before = Y.Bits.size();
			for (int F = 1; F <= 300; ++F)
			{
				Y.ShipRide(F / Fps, 1, Pose(F / Fps));
			}
			Count[K] = static_cast<double>(Y.Bits.size() - Before);
		}
		Check(Count[0] > 50.0 && Count[1] > 0.4 * Count[0] && Count[1] < 0.6 * Count[0] && Count[2] == 0.0, "strength scales the pieces", Say("10 s at 1: %.0f pieces; at 0.5: %.0f; at 0: %.0f", Count[0], Count[1], Count[2]));
		// (ends when told: no piece born after the stop, none left 4 s on)
		{
			Battle Y;
			Open(Y, Book);
			Y.ShipRide(0.0, 1, Pose(0.0));
			Y.DeckWash(0.0, 1, Edge, 3, 1.0, 60.0, "runoff");
			int After = 0, Left = 0;
			for (int F = 1; F <= 300; ++F)
			{
				const double T = F / Fps;
				if (F == 150)
				{
					Y.DeckWashStop(T, 1);
				}
				Y.ShipRide(T, 1, Pose(T));
				Look(Y, T, Pose(T).At);
			}
			for (const Bit& B : Y.Bits)
			{
				After += B.DeckShip != 0 && B.Born > 5.0 + 1e-9 && B.DeckMode == 1 ? 1 : 0;
				Left += B.DeckShip != 0 && B.Born + B.Life > 10.0 ? 1 : 0;
			}
			Check(After == 0 && Left == 0, "it ends when told: nothing new after DeckWashStop, all gone 5 s on", Say("%.0f sheets born after the stop; %.0f alive at 10 s", After, Left));
			Y.ShipRideStop(1);
			Look(Y, 10.1, Pose(10.1).At);
			int Carried = 0;
			for (const Bit& B : Y.Bits)
			{
				Carried += B.DeckShip != 0 ? 1 : 0;
			}
			Check(Carried == 0 && Y.DeckWash(10.2, 1, Edge, 3, 1.0, 5.0, "runoff") == 0, "  ...and a ship let go carries nothing, and takes no wash until told of again", Say("%.0f pieces still hers", Carried));
		}
		// (awash: a ship listing 10 degrees to starboard, the sea along her low edge, 2 m freeboard there)
		{
			Battle Y;
			Open(Y, Book);
			const Vec Low[2] = { Vec(-50.0, -16.5, 2.0), Vec(50.0, -16.5, 2.0) };
			Y.ShipRide(0.0, 2, Pose(0.0, 0.0, 1.0, -10.0, 3.0));
			const int Awash = Y.DeckWash(0.0, 2, Low, 2, 1.0, 20.0, "awash");
			int Inboard = 0, Pieces = 0;
			for (int F = 1; F <= 150; ++F)
			{
				const double T = F / Fps;
				Y.ShipRide(T, 2, Pose(T, 0.0, 1.0, -10.0, 3.0));
				Look(Y, T, Pose(T, 0.0, 1.0, -10.0, 3.0).At);
			}
			for (const Bit& B : Y.Bits)
			{
				if (B.DeckShip == 3)
				{
					++Pieces;
					Inboard += B.DeckV.Y > 0.0 ? 1 : 0;
				}
			}
			Check(Awash == 1 && Pieces > 20 && Inboard == Pieces, "awash: the sea washes in over her low edge", Say("%.0f pieces alive at 5 s, %.0f running inboard", Pieces, Inboard));
		}
		std::string Got;
		const bool bSame = DiceUntouched(Book, [&](Battle& Z)
			{
				Z.ShipRide(0.0, 1, Pose(0.0));
				Z.DeckWash(0.0, 1, Edge, 3, 1.0, 30.0, "runoff");
				for (int F = 1; F <= 600; ++F)
				{
					Z.ShipRide(F / Fps, 1, Pose(F / Fps));
				}
			}, Got);
		Check(bSame, "no draw on the shared seed", Got);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// C. The oil lane (item 10 part 3)

	void OilLane(const JsonRecipe& Book)
	{
		std::printf("\nC. The oil lane: a holed ship at 10 m/s, the wind (1.5, 0.5) m/s\n");
		const double W0 = Book.Get("sea_leak.w0_m", 0.0), K = Book.Get("sea_leak.k_m", 0.0), DriftK = Book.Get("sea_leak.drift_k", 0.0);
		const double Dark0 = Book.Get("sea_leak.dark", 0.0), DarkS = Book.Get("sea_leak.dark_s", 0.0), DarkPow = Book.Get("sea_leak.dark_pow", 0.0), Ragged = Book.Get("sea_leak.ragged", 0.0);
		std::printf("    the row: w0 %.1f m + %.2f m sqrt(age s); drift %.0f%% of the wind; dark %.2f fading over %.0f s (power %.1f); ragged %.2f\n", W0, K, DriftK * 100.0, Dark0, DarkS, DarkPow, Ragged);
		std::vector<Battle::LanePoint> Strip;
		{
			Battle X;
			Open(X, Book);
			const uint64_t Was = X.Seeds;
			for (int F = 0; F <= 300 * 10; ++F)
			{
				const double T = F / 10.0;
				X.OilLeak(T, 4, Vec(10.0 * T, 0.0, 0.0), 1.0);
			}
			const double Now = 300.0;
			X.OilLane(Now, 4, Strip);
			double WorstW = 0.0, WorstAt = 0.0, WorstDark = 0.0, RagLo = 1e9, RagHi = -1e9;
			for (size_t I = 0; I + 1 < Strip.size(); ++I)
			{
				const Battle::LanePoint& L = Strip[I];
				const double Born = Now - L.Age;
				const double Half = 0.5 * (W0 + K * std::sqrt(L.Age));
				const double Mean = 0.5 * (L.Left + L.Right);
				RagLo = std::min(RagLo, std::min(L.Left, L.Right) / Half - 1.0);
				RagHi = std::max(RagHi, std::max(L.Left, L.Right) / Half - 1.0);
				WorstW = std::max(WorstW, std::fabs(Mean - Half) / Half - Ragged);
				const Vec Want = Vec(10.0 * Born, 0.0, 0.0) + Vec(1.5, 0.5, 0.0) * (DriftK * L.Age);
				WorstAt = std::max(WorstAt, (L.At - Want).Length());
				WorstDark = std::max(WorstDark, std::fabs(L.Dark - Dark0 * std::pow(1.0 - L.Age / DarkS, DarkPow)));
			}
			const Battle::LanePoint& Old = Strip.front();
			std::printf("    at 300 s: %d points; the oldest %.0f s old, %.1f m wide, %.2f dark, drifted %.1f m; the newest at her hole\n", static_cast<int>(Strip.size()), Old.Age, Old.Left + Old.Right, Old.Dark, Old.Age * DriftK * std::sqrt(1.5 * 1.5 + 0.5 * 0.5));
			Check(Strip.size() == 151 && std::fabs(Strip.back().At.X - 3000.0) < 1e-9, "a point every 2 s, and the last where the oil leaves her", Say("%.0f points over 300 s; the last at x %.1f (her hole 3000)", static_cast<double>(Strip.size()), Strip.back().At.X));
			Check(WorstW <= 1e-9 && RagLo >= -Ragged - 1e-9 && RagHi <= Ragged + 1e-9 && RagHi - RagLo > Ragged, "width w0 + k sqrt(age), each edge ragged within its share", Say("edges %+.2f to %+.2f of the half width (ragged %.2f)", RagLo, RagHi, Ragged));
			Check(WorstAt < 1e-6, "drifting at drift_k of the wind from where it was laid", Say("worst %.2g m off the formula", WorstAt));
			Check(WorstDark < 1e-9, "darkness by age against the formula", Say("worst %.2g off", WorstDark));
			Check(X.Seeds == Was, "no dice: the shared seed untouched", Say("%.0f before, %.0f after", static_cast<double>(Was), static_cast<double>(X.Seeds)));
		}
		// (a 20-minute track with a turn: the lane's whole length kept within its 200 points)
		{
			Battle X;
			Open(X, Book);
			Vec At;
			double Heading = 0.0;
			size_t Most = 0;
			for (int F = 0; F <= 1200 * 10; ++F)
			{
				const double T = F / 10.0;
				if (T > 600.0 && T < 690.0)
				{
					Heading += 0.1 * (Pi / 2.0) / 90.0; // (a quarter turn over a minute and a half)
				}
				At = At + Vec(std::cos(Heading), std::sin(Heading), 0.0) * 1.0;
				X.OilLeak(T, 4, At, 1.0);
				Most = std::max(Most, X.Lanes.empty() ? 0 : X.Lanes[0].Dots.size());
			}
			X.OilLane(1200.0, 4, Strip);
			double Widest = 0.0, Gap = 0.0;
			for (size_t I = 0; I < Strip.size(); ++I)
			{
				Widest = std::max(Widest, Strip[I].Left + Strip[I].Right);
				Gap = I > 0 ? std::max(Gap, (Strip[I].At - Strip[I - 1].At).Length()) : 0.0;
			}
			std::printf("    20 minutes with a quarter turn: %d points; the oldest %.0f s, %.1f m wide; the widest gap between points %.0f m\n", static_cast<int>(Strip.size()), Strip.front().Age, Strip.front().Left + Strip.front().Right, Gap);
			Check(Most <= 200 && Strip.front().Age > 1190.0 && (Strip.back().At - At).Length() < 1e-6 && Gap < 150.0, "at most 200 points, the whole 20 minutes kept, no gap over 150 m", Say("at most %.0f kept; the oldest %.0f s old; the last at her hole", static_cast<double>(Most), Strip.front().Age));
			// (the cost of the strip as the scene asks for it each frame: 6 ships of 200 points)
			Battle Y;
			Open(Y, Book);
			for (int F = 0; F <= 1200 * 2; ++F)
			{
				for (int S = 0; S < 6; ++S)
				{
					Y.OilLeak(F / 2.0, S, Vec(5.0 * F, 300.0 * S, 0.0), 1.0);
				}
			}
			const auto T0 = std::chrono::steady_clock::now();
			int Points = 0;
			for (int Rep = 0; Rep < 200; ++Rep)
			{
				for (int S = 0; S < 6; ++S)
				{
					Points += Y.OilLane(1200.0, S, Strip);
				}
			}
			const double Ms = Since(T0) / 200.0;
			std::printf("    time: the strips of 6 lanes (%d points) %.3f ms a frame on this box\n", Points / 200, Ms);
			bTimeFailed = bTimeFailed || Ms > 1.0;
		}
		// (six ships at most)
		{
			Battle X;
			Open(X, Book);
			for (int S = 0; S < 7; ++S)
			{
				X.OilLeak(0.0, 10 + S, Vec(0.0, 100.0 * S, 0.0), 1.0);
			}
			std::vector<int> Ships;
			const int N = X.OilLanes(Ships);
			Check(N == 6 && X.OilLane(1.0, 16, Strip) == 0, "at most 6 ships: a seventh is refused", Say("%.0f lanes; the seventh's strip %.0f points", N, static_cast<double>(X.OilLane(1.0, 16, Strip))));
			X.OilLeakStop(1.0, 10);
			X.OilLeak(3700.0, 16, Vec(0.0, 600.0, 0.0), 1.0);
			Check(X.OilLane(3700.0, 16, Strip) > 0, "  ...and taken once a stopped lane has faded (an hour on)", "");
		}
		// (a ship that stops: no new points, the lane stays and widens)
		{
			Battle X;
			Open(X, Book);
			size_t AtStop = 0, Later = 0;
			for (int F = 0; F <= 400 * 10; ++F)
			{
				const double T = F / 10.0;
				X.OilLeak(T, 4, Vec(10.0 * std::min(T, 100.0), 0.0, 0.0), 1.0);
				if (F == 1010)
				{
					AtStop = X.Lanes[0].Dots.size();
				}
			}
			Later = X.Lanes[0].Dots.size();
			X.OilLane(400.0, 4, Strip);
			const Battle::LanePoint& Pool = Strip[Strip.size() - 2]; // (her last kept point: the live one at her hole is fresh oil)
			Check(AtStop == Later && Pool.Left + Pool.Right > (1.0 - Ragged) * (W0 + K * std::sqrt(300.0)), "a ship that stops lays no new point; the oil spreads where she lies", Say("%.0f points when she stopped, %.0f 300 s later; the last %.1f m wide", static_cast<double>(AtStop), static_cast<double>(Later), Pool.Left + Pool.Right));
		}
		// (a ship that sinks: OilLeakStop; the lane stays, stops growing, fades within the hour)
		{
			Battle X;
			Open(X, Book);
			for (int F = 0; F <= 200 * 10; ++F)
			{
				X.OilLeak(F / 10.0, 4, Vec(F * 1.0, 0.0, 0.0), 1.0);
			}
			X.OilLeakStop(200.0, 4);
			const int AtSink = X.OilLane(200.5, 4, Strip);
			const double End = Strip.back().At.X, EndAge = Strip.back().Age;
			const int Later = X.OilLane(1000.0, 4, Strip);
			const double EndLater = Strip.back().At.X, EndAgeLater = Strip.back().Age;
			const int Gone = X.OilLane(200.0 + 3600.0 + 1.0, 4, Strip);
			Check(AtSink > 0 && Later == AtSink && std::fabs(EndLater - End - (EndAgeLater - EndAge) * DriftK * 1.5) < 1e-6 && Gone == 0, "a ship that sinks: her lane stays, stops growing, gone in the hour", Say("%.0f points as she sank, %.0f 800 s on (its end only drifted), %.0f an hour on", AtSink, Later, Gone));
		}
		Battle W;
		JsonRecipe Without = Book;
		for (auto It = Without.Numbers.begin(); It != Without.Numbers.end();)
		{
			It = It->first.rfind("sea_leak.", 0) == 0 ? Without.Numbers.erase(It) : std::next(It);
		}
		Open(W, Without);
		W.OilLeak(0.0, 4, Vec(), 1.0);
		W.OilLeak(5.0, 4, Vec(50.0, 0.0, 0.0), 1.0);
		Check(W.OilLane(5.0, 4, Strip) == 0, "without the row: no lane", "");
		std::string Got;
		Check(DiceUntouched(Book, [&](Battle& Z) { for (int F = 0; F < 600; ++F) { Z.OilLeak(F / 10.0, 4, Vec(F * 1.0, 0.0, 0.0), 1.0); } Z.OilLane(60.0, 4, Strip); }, Got), "no draw on the shared seed", Got);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// D. The deck's brass (item 11 part 4)

	/** Her pieces of the deck's brass: lying (or in the air) on her deck, and gone over the side. */
	struct CaseCount
	{
		int Lying = 0, Over = 0, Air = 0;
	};
	CaseCount CountCases(const Battle& X, double Now)
	{
		CaseCount C;
		for (const Bit& B : X.Bits)
		{
			if (B.DeckShip == 0 || B.DeckMode < 3 || !(Now < B.Born + B.Life) || Now < B.Born)
			{
				continue;
			}
			C.Over += B.DeckMode == 4 ? 1 : 0;
			C.Lying += B.DeckMode == 3 && Now - B.Born >= B.Land ? 1 : 0;
			C.Air += B.DeckMode == 3 && Now - B.Born < B.Land ? 1 : 0;
		}
		return C;
	}

	/** A frame drawn from Dist m off her starboard side at the mount, Up m up, looking at it. */
	int LookAt(Battle& X, double Now, const Vec& At, double Dist, double Up = 3.0)
	{
		Battle::View V;
		V.Eye = At + Vec(0.0, -Dist, Up);
		V.Forward = (At - V.Eye).Unit();
		V.Right = V.Forward.Cross(Vec(0.0, 0.0, 1.0)).Unit(Vec(1.0, 0.0, 0.0));
		V.Up = V.Right.Cross(V.Forward).Unit();
		V.PixelsPerRadian = 1600.0 / (2.0 * std::tan(0.5 * 55.0 * Pi / 180.0));
		V.HalfAcross = std::tan(0.5 * 55.0 * Pi / 180.0);
		return X.Draw(Now, V);
	}

	Vec ToWorld(const Battle::ShipPose& P, const Vec& L)
	{
		const Vec Up = P.Up.Unit(), Fore = (P.Fore - Up * P.Fore.Dot(Up)).Unit(), Left = Up.Cross(Fore).Unit();
		return P.At + Fore * L.X + Left * L.Y + Up * L.Z;
	}

	bool IsSpent(const Bit& B)
	{
		return B.Form == Bit::Shape::Plate && !B.bThrown && B.Rise < 0.0;
	}

	/** A twin 40 mm on her starboard side (its breech 20 m aft of her middle, 8 m to starboard, 12.5 m up: 1.6 m over her
	 * deck at 10.9 m), firing up and out to starboard at 120 rounds a minute a barrel for Seconds, through AirGun with
	 * AirGunShip, drawn at 30 frames a second from 15 m. List and Roll in degrees; she steams straight at 10 m/s. */
	struct TwinRun
	{
		CaseCount End;
		int Spent = 0, MostLying = 0, Draws = 0, Counted = 0;
		double RideErr = 0.0, LowSide = 1e9, HighMoved = 0.0, StillMoved = 0.0, AtCoaming = 0.0;
		std::vector<std::string> Others;
		uint64_t Seeds = 0;
	};
	TwinRun Twin(const JsonRecipe& Book, double Seconds, double List, double Roll, double Coaming, bool bTie, bool bOtherGun = false)
	{
		TwinRun R;
		Battle X;
		Open(X, Book);
		auto P = [&](double T)
		{
			Battle::ShipPose Q = Pose(T, 0.0, Roll, List, 10.0);
			Q.Coaming = Coaming;
			return Q;
		};
		const Vec Breech(-20.0, -8.0, 12.5);
		const double Fps = 30.0;
		X.ShipRide(0.0, 1, P(0.0));
		if (bTie)
		{
			X.AirGunShip(11, 1);
			X.AirGunShip(12, 1);
		}
		std::map<uint32_t, Vec> Local; // (each case's place on her once it has lain 5 s, kept from second Seconds - 30)
		std::map<uint32_t, Vec> Laid;
		for (int F = 0; F <= static_cast<int>(Seconds * Fps); ++F)
		{
			const double T = F / Fps;
			const Battle::ShipPose Q = P(T);
			X.ShipRide(T, 1, Q);
			for (int G = 0; G < 2; ++G)
			{
				const Vec Muzzle = ToWorld(Q, Breech + Vec(2.0, 0.3 * G, 0.6));
				const Vec Aim = Muzzle + (Q.Fore * 0.3 - Q.Left * 0.6 + Vec(0.0, 0.0, 0.75)) * 2000.0;
				X.AirGun(T, 11 + G, Muzzle, Aim, Vec(10.0, 0.0, 0.0), "aa40", 120.0, 881.0, "us");
			}
			if (bOtherGun)
			{
				// (another ship's gun, 30 m off on the sea, not tied to any ship)
				const Vec Muzzle = Q.At + Vec(-20.0, -40.0, 6.0);
				X.AirGun(T, 21, Muzzle, Muzzle + Vec(0.2, -0.5, 0.8) * 2000.0, Vec(10.0, 0.0, 0.0), "aa40", 120.0, 881.0, "us");
			}
			R.Draws += LookAt(X, T, ToWorld(Q, Breech), 15.0);
			R.MostLying = std::max(R.MostLying, static_cast<int>(X.DeckCases.size()));
			for (const Bit& B : X.Bits)
			{
				R.Spent += IsSpent(B) && B.Born > T - 1.0 / Fps && B.Born <= T ? 1 : 0;
				if (B.DeckShip == 0 || B.DeckMode != 3 || T - B.Born < B.Land)
				{
					continue;
				}
				if (!Laid.count(B.DeckId))
				{
					Laid[B.DeckId] = B.DeckP; // (where it landed)
				}
				if (T >= Seconds - 30.0 && T - B.Born >= B.Land + 5.0 && !Local.count(B.DeckId))
				{
					Local[B.DeckId] = B.DeckP;
				}
			}
		}
		const double End = Seconds;
		const Battle::ShipPose Q = P(End);
		R.End = CountCases(X, End);
		R.Counted = static_cast<int>(X.DeckCases.size());
		for (const Bit& B : X.Bits)
		{
			if (B.DeckShip == 0 || B.DeckMode != 3 || End - B.Born < B.Land || !(End < B.Born + B.Life))
			{
				R.Others.push_back(B.DeckShip == 0 && !IsSpent(B) ? Print(B) : std::string());
				continue;
			}
			// (where it is in the world, against its place on her worked out here)
			R.RideErr = std::max(R.RideErr, (B.From - ToWorld(Q, B.DeckP)).Length());
			const double Edge = Q.HalfBeam * std::min(1.0, 1.25 * std::sqrt(std::max(0.0, 1.0 - std::pow(2.0 * B.DeckP.X / Q.Long, 2.0))));
			if (End - B.Born >= B.Land + 20.0)
			{
				R.AtCoaming = std::max(R.AtCoaming, std::fabs(std::fabs(B.DeckP.Y) - (Edge - B.R0))); // (20 s is time to roll 8.5 m at 6 deg)
			}
			auto It = Local.find(B.DeckId);
			if (It != Local.end())
			{
				R.StillMoved = std::max(R.StillMoved, (B.DeckP - It->second).Length());
				// (and the world: where she carries it now against where its place 30 s ago puts it on her now)
				R.RideErr = std::max(R.RideErr, (B.From - ToWorld(Q, It->second)).Length() - (B.DeckP - It->second).Length());
			}
			auto Was = Laid.find(B.DeckId);
			if (Was != Laid.end())
			{
				R.HighMoved = std::max(R.HighMoved, B.DeckP.Y - Was->second.Y);
				if (End - B.Born >= B.Land + 5.0)
				{
					R.LowSide = std::min(R.LowSide, Was->second.Y - B.DeckP.Y);
				}
			}
		}
		R.Others.erase(std::remove(R.Others.begin(), R.Others.end(), std::string()), R.Others.end());
		std::sort(R.Others.begin(), R.Others.end()); // (as a set: Prune moves the last piece into a gone one's place, so the order is the room's)
		R.Seeds = X.Seeds;
		return R;
	}

	void Brass(const JsonRecipe& Book)
	{
		std::printf("\nD. The deck's brass: a twin 40 mm (AirGun, tied to her with AirGunShip) on a battleship's starboard side, 60 s at 120 rounds a minute a barrel\n");
		const double CaseR = 0.5 * Book.Numbers.at("deck_cases.aa40.case_d_m");
		{
			const TwinRun L = Twin(Book, 60.0, 0.0, 0.0, 0.0, true);
			std::printf("    level: %.0f cases lying at the end (%.0f at most at once), %.0f over the side, %.0f in the air\n", static_cast<double>(L.End.Lying), static_cast<double>(L.MostLying), static_cast<double>(L.End.Over), static_cast<double>(L.End.Air));
			Check(L.Counted == 40 && L.MostLying <= 40 && L.End.Lying <= 42 && L.End.Over == 0, "level: the mount's pile holds 40 (its cap), none over the side", Say("%.0f counted lying (%.0f pieces, the oldest fading out), %.0f at most", L.Counted, L.End.Lying, L.MostLying));
			Check(L.StillMoved < 0.001, "level: a case lying stays where it lies on her (30 s)", Say("moved %.4f m on her deck", L.StillMoved));
			Check(L.RideErr < 0.01, "level: it rides her within 1 cm after 60 s", Say("%.5f m off where her pose puts it", L.RideErr));
			Check(L.Spent == 0, "level: no aircraft-style cases from her guns (Spent off for her)", Say("%.0f Spent pieces", L.Spent));
		}
		{
			const TwinRun H = Twin(Book, 60.0, 6.0, 0.0, 0.0, true);
			std::printf("    heeled 6 deg to starboard, no coaming: %.0f lying, %.0f over the side now\n", static_cast<double>(H.End.Lying), static_cast<double>(H.End.Over));
			Check(H.HighMoved < 0.3 && H.LowSide > 0.5 && H.RideErr < 0.01, "heeled 6 deg: every case lying 5 s has rolled to her low side", Say("the least roll from where it landed %.2f m to starboard; up the deck at most %.2f m (the throw's slide as it lands); ride %.5f m", H.LowSide, H.HighMoved, H.RideErr));
			Check(H.End.Lying < 40, "heeled 6 deg, no coaming: cases go over the side", Say("%.0f lying of 240 fired", H.End.Lying));
			const TwinRun C = Twin(Book, 60.0, 6.0, 0.0, 0.3, true);
			Check(C.End.Over == 0 && C.Counted == 40 && C.AtCoaming < 0.001, "heeled 6 deg, a 0.3 m coaming: they stop against it, none over", Say("%.0f lying, %.0f over; the farthest %.4f m off the coaming", C.End.Lying, C.End.Over, C.AtCoaming));
			const TwinRun S = Twin(Book, 40.0, 1.5, 0.0, 0.0, true);
			const TwinRun M = Twin(Book, 40.0, 2.5, 0.0, 0.0, true);
			Check(S.StillMoved < 0.001 && S.End.Over == 0, "heeled 1.5 deg (under stick_deg 2): a case lying stays put", Say("moved %.4f m in 30 s", S.StillMoved));
			Check(M.StillMoved > 0.5 || M.End.Over > 0, "heeled 2.5 deg (over stick_deg 2): it rolls", Say("moved %.2f m in 30 s, %.0f over the side", M.StillMoved, M.End.Over));
			const TwinRun Rl = Twin(Book, 60.0, 0.0, 6.0, 0.3, true);
			Check(Rl.RideErr < 0.01 && Rl.End.Over == 0, "rolling 6 deg either way, a 0.3 m coaming: they ride her within 1 cm", Say("%.5f m; %.0f lying", Rl.RideErr, Rl.End.Lying));
		}
		{
			// (Spent kept off for her guns alone: her twin tied, another gun near the eye not; and every other piece the same)
			const TwinRun A = Twin(Book, 8.0, 0.0, 0.0, 0.0, true, true);
			const TwinRun B = Twin(Book, 8.0, 0.0, 0.0, 0.0, false, true);
			const TwinRun B1 = Twin(Book, 8.0, 0.0, 0.0, 0.0, false, false);
			Check(A.Spent > 0 && A.Spent < B.Spent && B1.Spent > 0 && A.Spent == B.Spent - B1.Spent, "Spent off for her guns only: the untied gun still throws its cases", Say("%.0f Spent with her twin tied, %.0f untied (her twin's own %.0f)", A.Spent, B.Spent, B1.Spent));
			for (size_t I = 0; I < std::min(A.Others.size(), B.Others.size()); ++I)
			{
				if (A.Others[I] != B.Others[I])
				{
					std::printf("    the first piece that differs, %d:\n      %s\n      %s\n", static_cast<int>(I), A.Others[I].c_str(), B.Others[I].c_str());
					break;
				}
			}
			Check(A.Others == B.Others && A.Seeds == B.Seeds && !A.Others.empty(), "  ...and every other piece of the guns' the same to the bit, the shared seed as it was", Say("%.0f pieces compared; the shared seed ", static_cast<double>(A.Others.size())) + (A.Seeds == B.Seeds ? "the same" : "MOVED"));
		}
		{
			// (the caps: eight mounts, 60 s; and drawn within 40 to 60 m)
			Battle X;
			Open(X, Book);
			X.ShipRide(0.0, 1, Pose(0.0, 0.0, 0.0));
			int MostMount = 0;
			for (int F = 0; F <= 60 * 4; ++F)
			{
				const double T = F / 4.0;
				X.ShipRide(T, 1, Pose(T, 0.0, 0.0));
				for (int M = 0; M < 8; ++M)
				{
					X.DeckBrass(T, 1, ToWorld(Pose(T, 0.0, 0.0), Vec(-60.0 + 15.0 * M, 10.0, 12.0)), Vec(-1.0, 0.0, 0.0), "aa40", 1);
				}
				LookAt(X, T, Pose(T, 0.0, 0.0).At, 30.0);
			}
			std::map<uint64_t, int> ByMount;
			for (const Battle::CaseRec& R : X.DeckCases)
			{
				++ByMount[R.Mount];
			}
			for (const auto& M : ByMount)
			{
				MostMount = std::max(MostMount, M.second);
			}
			const CaseCount C = CountCases(X, 60.0);
			Check(X.DeckCases.size() <= 200 && MostMount <= 40, "eight mounts for 60 s: at most 200 lying in all, 40 by a mount", Say("%.0f counted lying, %.0f at most by a mount (%.0f pieces still fading)", static_cast<double>(X.DeckCases.size()), MostMount, C.Lying - static_cast<double>(X.DeckCases.size())));
		}
		{
			Battle X;
			Open(X, Book);
			const Battle::ShipPose Q = Pose(0.0, 0.0, 0.0);
			X.ShipRide(0.0, 1, Q);
			const Vec Mount = ToWorld(Q, Vec(0.0, -8.0, 12.0));
			X.DeckBrass(0.0, 1, Mount, Vec(-1.0, 0.0, 0.0), "aa40", 10);
			Battle E;
			Open(E, Book);
			E.ShipRide(0.0, 1, Q);
			double Alpha[3] = {};
			int Laid[3] = {}, Bare[3] = {};
			const double Dist[3] = { 30.0, 50.0, 70.0 };
			for (int K = 0; K < 3; ++K)
			{
				for (int F = 0; F <= 30; ++F)
				{
					X.ShipRide(5.0 + F / 30.0, 1, Q);
					E.ShipRide(5.0 + F / 30.0, 1, Q);
					Laid[K] = LookAt(X, 5.0 + F / 30.0, Mount, Dist[K], 0.0);
					Bare[K] = LookAt(E, 5.0 + F / 30.0, Mount, Dist[K], 0.0);
				}
				for (const Bit& B : X.Bits)
				{
					Alpha[K] = std::max(Alpha[K], B.DeckMode == 3 ? B.Alpha : 0.0);
				}
			}
			Check(std::fabs(Alpha[0] - 1.0) < 1e-9 && std::fabs(Alpha[1] - 0.5) < 0.05 && Alpha[2] == 0.0 && Laid[2] == Bare[2] && Laid[0] > Bare[0], "drawn whole within 40 m, half at 50, nothing at 70 (and nothing counted)", Say("opacity %.2f, %.2f, %.2f; pieces laid at 70 m %.0f, %.0f without them", Alpha[0], Alpha[1], Alpha[2], Laid[2], Bare[2]));
			const CaseCount C = CountCases(X, 6.0);
			double Long = 0.0, Wide = 0.0;
			for (const Bit& B : X.Bits)
			{
				if (B.DeckMode == 3)
				{
					Long = std::max(Long, 2.0 * B.R0 * B.Stretch);
					Wide = std::max(Wide, 2.0 * B.R0);
				}
			}
			Check(C.Lying == 10 && std::fabs(Long - 0.311) < 1e-9 && std::fabs(Wide - 0.059) < 1e-9, "a 40 mm case is its true size: 0.311 m by 0.059 m", Say("%.3f m by %.3f m; %.0f lying", Long, Wide, C.Lying));
			(void)CaseR;
		}
		{
			// (the cost: 200 cases lying on eight mounts, drawn from 15 m)
			Battle X;
			Open(X, Book);
			const Battle::ShipPose Q = Pose(0.0, 0.0, 0.0);
			X.ShipRide(0.0, 1, Q);
			for (int M = 0; M < 8; ++M)
			{
				X.DeckBrass(0.0, 1, ToWorld(Q, Vec(-8.0 + 2.0 * M, -8.0, 12.0)), Vec(-1.0, 0.0, 0.0), "aa40", 16);
				X.DeckBrass(0.1, 1, ToWorld(Q, Vec(-8.0 + 2.0 * M, -8.0, 12.0)), Vec(-1.0, 0.0, 0.0), "aa40", 16);
			}
			for (int F = 0; F < 60; ++F)
			{
				X.ShipRide(2.0 + F / 30.0, 1, Pose(2.0 + F / 30.0, 0.0, 6.0, 0.0, 10.0));
				LookAt(X, 2.0 + F / 30.0, ToWorld(Q, Vec(0.0, -8.0, 12.0)), 15.0);
			}
			const auto T0 = std::chrono::steady_clock::now();
			int Shown = 0;
			for (int F = 0; F < 100; ++F)
			{
				const double T = 4.0 + F / 30.0;
				X.ShipRide(T, 1, Pose(T, 0.0, 6.0, 0.0, 10.0));
				Shown = LookAt(X, T, Pose(T, 0.0, 6.0, 0.0, 10.0).At + Vec(0.0, -8.0, 12.0), 15.0);
			}
			const double Ms = Since(T0) / 100.0;
			// (the same frames with her deck bare)
			Battle E;
			Open(E, Book);
			E.ShipRide(0.0, 1, Q);
			const auto T1 = std::chrono::steady_clock::now();
			for (int F = 0; F < 100; ++F)
			{
				const double T = 4.0 + F / 30.0;
				E.ShipRide(T, 1, Pose(T, 0.0, 6.0, 0.0, 10.0));
				LookAt(E, T, Pose(T, 0.0, 6.0, 0.0, 10.0).At + Vec(0.0, -8.0, 12.0), 15.0);
			}
			const double Bare = Since(T1) / 100.0;
			std::printf("    time: %d cases lying, the frame (ShipRide and Draw, %d fans) %.3f ms on this box, %.3f ms with her deck bare\n", static_cast<int>(X.DeckCases.size()), Shown, Ms, Bare);
			bTimeFailed = bTimeFailed || Ms - Bare > 3.0;
		}
		JsonRecipe Without = Book;
		for (auto It = Without.Numbers.begin(); It != Without.Numbers.end();)
		{
			It = It->first.rfind("deck_cases.", 0) == 0 ? Without.Numbers.erase(It) : std::next(It);
		}
		{
			const TwinRun W = Twin(Without, 8.0, 0.0, 0.0, 0.0, true, true);
			const TwinRun B = Twin(Book, 8.0, 0.0, 0.0, 0.0, false, true);
			Check(W.End.Lying == 0 && W.Spent == B.Spent && W.Others == B.Others, "without the row: no deck cases, every gun's pieces as today", Say("%.0f lying; %.0f Spent as %.0f", W.End.Lying, W.Spent, B.Spent));
		}
		std::string Got;
		Check(DiceUntouched(Book, [&](Battle& Z) { Z.ShipRide(0.0, 1, Pose(0.0)); Z.AirGunShip(11, 1); for (int F = 0; F < 40; ++F) { Z.ShipRide(F / 4.0, 1, Pose(F / 4.0)); Z.DeckBrass(F / 4.0, 1, ToWorld(Pose(F / 4.0), Vec(-20.0, -8.0, 12.5)), Vec(-1.0, 0.0, 0.0), "aa40", 2); } }, Got), "no draw on the shared seed", Got);
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "fx_sea_test <battle-fx.json> <battle-fx-sea-haze.json> <out folder>\n");
		return 2;
	}
	JsonRecipe Book;
	if (!FxLamps::LoadRecipe(Argv[1], Book))
	{
		return 2;
	}
	std::printf("The sea's dramatic moments (items 9, 10, 11) in the effects kit (%s)\n", Argv[1]);
	Drench(Book);
	Wash(Book);
	OilLane(Book);
	Brass(Book);
	std::printf("\n%d checks, %d failed\n%s\n", Checks, Failed, Failed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
	std::printf("time: the cost checks %s\n", bTimeFailed ? "FAILED" : "passed");
	return Failed == 0 && !bTimeFailed ? 0 : 1;
}
