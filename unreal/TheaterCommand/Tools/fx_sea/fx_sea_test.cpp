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
#include "Battle/ShipGuns.h"

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

	/** A frame drawn from 300 m off her starboard beam, 20 m up (below). */
	int Look(Battle& X, double Now, const Vec& At);

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
		// (both drawn once from the same eye before the shell: the shell's pieces are laid by the eye's distance, so a
		// Then that draws must not be told from one that does not by that alone)
		Look(A, 29.9, Vec(300.0, 40.0, 0.0));
		Look(B, 29.9, Vec(300.0, 40.0, 0.0));
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

	// (the ship of the ship's frame, a frame drawn from her beam, and a piece's place worked out here: below)
	Battle::ShipPose Pose(double T, double Turn = 0.03, double Roll = 6.0, double List = 0.0, double Mps = 10.0);
	Vec Expected(const Bit& B, const Battle::ShipPose& P, double Now);

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
				Late += B.Born >= Now + Rise + 0.5 * Hang ? 1 : 0;
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
		Check(Late == Falling && FirstBorn >= Now + Rise + 0.5 * Hang, "timed after the column's rise and most of its hang", Say("the first lump leaves %.2f s after the shell (rise %.2f + hang %.2f)", FirstBorn - Now, Rise, Hang));
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
		// (the wind and her far edge: every lump and every sheet comes down on her deck, between her two edges, whatever
		// the wind; an offshore wind (from her toward the column) of 4 and of 8 m/s; a destroyer 11 m across, told by her
		// beam and by her pose)
		auto Between = [&](double WindY, double Beam, bool bByPose, int& Lumps, double& Lo, double& Hi)
		{
			Battle Z;
			Open(Z, Book);
			Z.SetWind(Vec(0.0, WindY, 0.0));
			const double Half = 0.5 * Beam;
			const Vec Col(0.0, -(Half + Off), 0.0);
			if (bByPose)
			{
				Battle::ShipPose Q = Pose(Now, 0.0, 0.0, 0.0, 0.0);
				Q.At = Vec();
				Q.HalfBeam = Half;
				Z.ShipRide(Now, 3, Q);
			}
			const size_t From = Z.Bits.size();
			Z.DeckDrench(Now, Col, 406.0, Toward, DeckZ, 3, bByPose ? 0.0 : Beam);
			Lumps = 0;
			Lo = 1e18;
			Hi = -1e18;
			for (size_t I = From; I < Z.Bits.size(); ++I)
			{
				const Bit& B = Z.Bits[I];
				const bool bLump = B.Form == Bit::Shape::Puff && B.StretchBy > 0.0;
				if (!bLump && B.Form != Bit::Shape::Flat)
				{
					continue;
				}
				Lumps += bLump ? 1 : 0;
				const Vec At = bLump ? Z.Where(B, std::sqrt(2.0 * std::max(B.From.Z - DeckZ, 0.5) / B.Gravity)) : (B.DeckMode == 5 ? B.DeckP : B.From);
				const double Across = At.Y - Col.Y;
				Lo = std::min(Lo, Across);
				Hi = std::max(Hi, Across);
			}
		};
		int L4 = 0, L8 = 0, Ld = 0, Lp = 0;
		double Lo4 = 0, Hi4 = 0, Lo8 = 0, Hi8 = 0, Lod = 0, Hid = 0, Lop = 0, Hip = 0;
		Between(-4.0, 33.0, false, L4, Lo4, Hi4);
		Between(-8.0, 33.0, false, L8, Lo8, Hi8);
		Between(0.5, 11.0, false, Ld, Lod, Hid);
		Between(0.5, 11.0, true, Lp, Lop, Hip);
		Check(L4 > 10 && Lo4 >= Off - 1e-6 && Hi4 <= Off + 33.0 + 1e-6 && L8 > 10 && Lo8 >= Off - 1e-6 && Hi8 <= Off + 33.0 + 1e-6, "an offshore wind (4 and 8 m/s from her toward it): every lump and sheet comes down on her deck", Say("4 m/s: %.1f to %.1f m from the column; 8 m/s: %.1f to %.1f", Lo4, Hi4, Lo8, Hi8) + Say(" (her deck %.0f to %.0f)", Off, Off + 33.0));
		Check(Ld > 3 && Lod >= Off - 1e-6 && Hid <= Off + 11.0 + 1e-6 && Lp > 3 && Lop >= Off - 1e-6 && Hip <= Off + 11.0 + 1e-6 && Ld < Falling, "a destroyer 11 m across: none past her far edge, by her beam or her pose", Say("by her beam %.0f lumps, %.1f to %.1f m; by her pose %.0f, %.1f", Ld, Lod, Hid, Lp, Lop) + Say(" to %.1f m (her deck %.0f to %.0f)", Hip, Off, Off + 11.0));
		{
			// (under way: a battleship at 10 m/s, the column 8 m off her starboard side 62.5 m ahead of her middle; her sheets
			// lie in her frame from where they land, and ride her within a centimetre; a short ship at 15 m/s steams past
			// some of the head, whose lumps fall on to the sea)
			auto Run = [&](double Long, double Mps, double Ahead, int& Sheets_, int& ToSea, double& Err, double& Slid)
			{
				Battle Z;
				Open(Z, Book);
				auto Q = [&](double T) { Battle::ShipPose R = Pose(T, 0.0, 0.0, 0.0, Mps); R.Long = Long; R.Deck = DeckZ; return R; };
				for (int F = 0; F <= 10; ++F)
				{
					Z.ShipRide(Now - 1.0 + F / 10.0, 5, Q(Now - 1.0 + F / 10.0));
				}
				const Vec Col(Q(Now).At.X + Ahead, -(HalfBeam + Off), 0.0);
				const size_t From = Z.Bits.size();
				Z.DeckDrench(Now, Col, 406.0, Toward, DeckZ, 5);
				std::vector<Vec> Landed;
				Sheets_ = 0;
				ToSea = 0;
				for (size_t I = From; I < Z.Bits.size(); ++I)
				{
					const Bit& B = Z.Bits[I];
					Sheets_ += B.Form == Bit::Shape::Flat ? 1 : 0;
					ToSea += B.Form == Bit::Shape::Puff && B.StretchBy > 0.0 && Z.Where(B, B.Life).Z < 1.0 ? 1 : 0;
				}
				Err = 0.0;
				Slid = 0.0;
				std::vector<Vec> Was;
				for (int F = 1; F <= 200; ++F)
				{
					const double T = Now + F / 10.0;
					Z.ShipRide(T, 5, Q(T));
					Look(Z, T, Q(T).At);
					for (size_t I = From; I < Z.Bits.size(); ++I)
					{
						const Bit& B = Z.Bits[I];
						if (B.Form == Bit::Shape::Flat && B.DeckShip != 0 && B.DeckMode == 2 && T >= B.Born && T < B.Born + B.Life)
						{
							Err = std::max(Err, (Z.Where(B, T - B.Born) - Expected(B, Q(T), T)).Length());
							Slid = std::max(Slid, (B.DeckV * (1.0 - std::exp(-(T - B.Born)))).Length()); // (its slide on her deck, in her frame)
						}
					}
				}
			};
			int S1 = 0, Sea1 = 0, S2 = 0, Sea2 = 0;
			double E1 = 0, Sl1 = 0, E2 = 0, Sl2 = 0;
			Run(250.0, 10.0, 62.5, S1, Sea1, E1, Sl1);
			const double Gc = 2.0 * H / (Fall * Fall), Tm = std::sqrt(2.0 * (0.85 * H - DeckZ) / Gc); // (a lump's fall from its crown to her deck)
			Run(60.0, 15.0, 30.0 + 15.0 * (Rise + 0.72 * Hang + Tm), S2, Sea2, E2, Sl2); // (her bow passing the column as the head comes down)
			Check(S1 > 10 && Sea1 == 0 && E1 < 0.01 && Sl1 < 3.0, "under way at 10 m/s: her sheets lie in her frame and go with her", Say("%.0f sheets, the worst %.2g m off her pose, sliding at most %.1f m on her deck (in the world they would slip 50 m aft)", S1, E1, Sl1));
			Check(Sea2 > 0 && S2 > 0 && E2 < 0.01, "  ...a 60 m ship at 15 m/s steams past part of the head: those lumps fall on to the sea", Say("%.0f lumps to the sea, %.0f sheets on her", Sea2, S2));
		}
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

	Battle::ShipPose Pose(double T, double Turn, double Roll, double List, double Mps)
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
		// (her port side as the scene names it: Unreal's world turns the other way, so the wrapper passes port as Fore x Up;
		// an edge at plus Y in her frame lies on the side Left names, either way)
		{
			double Side[2] = { 0.0, 0.0 };
			for (int K = 0; K < 2; ++K)
			{
				Battle Y;
				Open(Y, Book);
				Battle::ShipPose P = Pose(0.0, 0.0, 0.0, 0.0, 0.0);
				P.Left = K == 0 ? P.Left : P.Left * -1.0;
				Y.ShipRide(0.0, 4, P);
				const Vec Port[2] = { Vec(-20.0, 16.5, 8.0), Vec(20.0, 16.5, 8.0) };
				Y.DeckWash(0.0, 4, Port, 2, 1.0, 2.0, "runoff");
				for (int F = 1; F <= 15; ++F)
				{
					Y.ShipRide(F / Fps, 4, P);
				}
				Look(Y, 0.6, P.At);
				int N = 0;
				for (const Bit& B : Y.Bits)
				{
					if (B.DeckShip == 5)
					{
						Side[K] += B.From.Dot(P.Left);
						++N;
					}
				}
				Side[K] = N > 0 ? Side[K] / N : 0.0;
			}
			Check(Side[0] > 10.0 && Side[1] > 10.0, "her port side is the side the scene names (a sum's world, and Unreal's turned the other way)", Say("the wash at plus Y lies %.1f m and %.1f m out on the named side", Side[0], Side[1]));
		}
		std::string Got;
		const bool bSame = DiceUntouched(Book, [&](Battle& Z)
			{
				// (10 s of it, drawn as a scene draws, and gone by the shell: so the room the shell finds is as without it,
				// and what is compared is the dice alone)
				Z.ShipRide(0.0, 1, Pose(0.0));
				Z.DeckWash(0.0, 1, Edge, 3, 1.0, 10.0, "runoff");
				for (int F = 1; F <= 600; ++F)
				{
					Z.ShipRide(F / Fps, 1, Pose(F / Fps));
					Look(Z, F / Fps, Pose(F / Fps).At);
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
		{
			// (the scene's paint, OilLook. The look review of 27 November: from another ship's bridge 800 m off, 20 m up, the
			// lane was gone: its darkness divided away under the 1.5 px floor, and a slick seen low reads by its calm)
			Battle Y;
			Open(Y, Book);
			const double Dark = Y.Num("sea_leak.dark", 0.55);
			const Battle::LaneLook Wide = Y.OilLook(Dark, 6.0, 30.0);
			const Battle::LaneLook Thin = Y.OilLook(Dark, 0.15, 30.0);
			const Battle::LaneLook Half = Y.OilLook(Dark, 0.75, 30.0);
			Check(Wide.Widen == 1.0 && Wide.Mid == Dark && std::fabs(Wide.Shoulder - 0.85 * Dark) < 1e-12 && Wide.Calm == 0.0, "the lane's paint from above and wide: as it was (its dark, 0.85 at the shoulder)", Say("mid %.3f, shoulder %.3f", Wide.Mid, Wide.Shoulder));
			Check(std::fabs(Thin.Widen - 10.0) < 1e-9 && std::fabs(Thin.Mid - 0.3) < 1e-9 && std::fabs(Half.Mid - Dark / std::sqrt(2.0)) < 1e-9, "  ...at the floor: widened to 1.5 px, fainter by the root, its middle at least 0.3", Say("0.15 px widened %.1f times, mid %.3f; 0.75 px mid %.3f (the dark / root of 2)", Thin.Widen, Thin.Mid, Half.Mid));
			// (the calm band by grazing angle; its lift in sRGB bytes over the sea (53,76,99 landed: its G byte 76 under the
			// scene's calm paint byte 140) at the middle)
			const double Graze[4] = { 1.0, 3.0, 5.0, 10.0 };
			double Calm[4], Lift[4];
			for (int G = 0; G < 4; ++G)
			{
				const Battle::LaneLook L = Y.OilLook(Dark, 0.15, Graze[G]);
				Calm[G] = L.Calm;
				Lift[G] = L.Calm * L.CalmAlpha * (140.0 - 76.0);
			}
			Check(Calm[0] == 1.0 && std::fabs(Calm[1] - 0.5) < 1e-9 && Calm[2] == 0.0 && Calm[3] == 0.0, "  ...seen low, the calm band: full by 1 degree, half at 3, none from 5", Say("calm at 1, 3, 5, 10 degrees: %.2f %.2f %.2f %.2f", Calm[0], Calm[1], Calm[2], Calm[3]));
			std::printf("    the calm band's lift at its middle (bytes of G over the sea, before the game's curve): %.1f %.1f %.1f %.1f at 1, 3, 5, 10 degrees\n", Lift[0], Lift[1], Lift[2], Lift[3]);
			const Battle::LaneLook Old = Y.OilLook(0.02, 0.15, 1.0);
			Check(Old.CalmAlpha < Thin.CalmAlpha && Old.Mid <= Old.CalmAlpha + 1e-12, "  ...thinner as the oil thins, never more than the calm paint's own", Say("an old lane's calm alpha %.3f against a fresh one's %.3f", Old.CalmAlpha, Thin.CalmAlpha));
		}
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
		{
			// (her deck from her pose: every case lies on it, whatever the gun's elevation; drop_m is only the throw's fall)
			Battle X;
			Open(X, Book);
			Battle::ShipPose Q = Pose(0.0, 0.0, 0.0);
			Q.Deck = 8.0;
			X.ShipRide(0.0, 1, Q);
			X.DeckBrass(0.0, 1, ToWorld(Q, Vec(-20.0, -8.0, 12.5)), Vec(-1.0, 0.0, 0.0), "aa40", 8);
			X.DeckBrass(0.0, 1, ToWorld(Q, Vec(20.0, -8.0, 10.0)), Vec(-1.0, 0.0, 0.0), "aa40", 8);
			for (int F = 1; F <= 150; ++F)
			{
				X.ShipRide(F / 30.0, 1, Q);
				LookAt(X, F / 30.0, ToWorld(Q, Vec(0.0, -8.0, 8.0)), 25.0);
			}
			int N = 0;
			double Worst = 0.0;
			for (const Bit& B : X.Bits)
			{
				if (B.DeckShip != 0 && B.DeckMode == 3)
				{
					++N;
					Worst = std::max(Worst, std::fabs(B.DeckP.Z - (8.0 + B.R0)));
					Worst = std::max(Worst, std::fabs(B.From.Z - (Q.At.Z + 8.0 + B.R0)));
				}
			}
			Check(N == 16 && Worst < 1e-9, "her deck from her pose (8 m): every case lies on it, the breech 12.5 m up or 10 m", Say("%.0f lying, the worst %.2g m off her deck", N, Worst));
		}
		{
			// (a ship's .50, tied to her: no aircraft-style case falls through her deck, and its own case lies on it)
			Battle X;
			Open(X, Book);
			const Battle::ShipPose Q = Pose(0.0, 0.0, 0.0);
			X.ShipRide(0.0, 1, Q);
			X.AirGunShip(31, 1);
			int Spent = 0;
			for (int F = 0; F <= 150; ++F)
			{
				const double T = F / 30.0;
				X.ShipRide(T, 1, Q);
				const Vec Muzzle = ToWorld(Q, Vec(-20.0, -8.0, 10.5));
				X.AirGun(T, 31, Muzzle, Muzzle + Vec(0.2, -0.5, 0.8) * 2000.0, Vec(), "hmg", 600.0, 890.0, "us");
				LookAt(X, T, Muzzle, 15.0);
				for (const Bit& B : X.Bits)
				{
					Spent += IsSpent(B) && B.Born > T - 1.0 / 30.0 && B.Born <= T ? 1 : 0;
				}
			}
			const CaseCount C = CountCases(X, 5.0);
			Check(Spent == 0 && C.Lying > 10, "a ship's .50 tied to her: no Spent through her deck, its 12.7 mm cases on it", Say("%.0f Spent in 5 s; %.0f cases on her deck", Spent, C.Lying));
		}
		{
			// (a far ship's rounds never take the cases in view: 20 lying 15 m from the eye on one ship; another ship 800 m
			// off firing 8 light guns for 40 s, and a third 70 to 80 m off (laid, drawn no more) firing 8 more, past the cap of all)
			Battle X;
			Open(X, Book);
			auto At = [](const Vec& Off) { Battle::ShipPose R = Pose(0.0, 0.0, 0.0, 0.0, 0.0); R.At = Off; return R; };
			const Battle::ShipPose Q1 = At(Vec()), Q2 = At(Vec(0.0, -800.0, 0.0)), Q3 = At(Vec(0.0, 40.0, 0.0));
			const Vec Mount = ToWorld(Q1, Vec(0.0, -8.0, 12.0));
			X.ShipRide(0.0, 1, Q1);
			X.ShipRide(0.0, 2, Q2);
			X.ShipRide(0.0, 3, Q3);
			LookAt(X, 0.0, Mount, 15.0);
			X.DeckBrass(0.0, 1, Mount, Vec(-1.0, 0.0, 0.0), "aa40", 10);
			X.DeckBrass(0.05, 1, Mount, Vec(-1.0, 0.0, 0.0), "aa40", 10);
			for (int F = 1; F <= 400; ++F)
			{
				const double T = F / 10.0;
				X.ShipRide(T, 1, Q1);
				X.ShipRide(T, 2, Q2);
				X.ShipRide(T, 3, Q3);
				for (int M = 0; M < 8; ++M)
				{
					X.DeckBrass(T, 2, ToWorld(Q2, Vec(-60.0 + 15.0 * M, 8.0, 12.0)), Vec(-1.0, 0.0, 0.0), "aa25", 1);
					X.DeckBrass(T, 3, ToWorld(Q3, Vec(-40.0 + 10.0 * M, 8.0, 12.0)), Vec(-1.0, 0.0, 0.0), "aa25", 1);
				}
				LookAt(X, T, Mount, 15.0);
			}
			int Mine = 0, Far = 0, Third = 0;
			for (const Bit& B : X.Bits)
			{
				if (B.DeckMode == 3 && 40.0 >= B.Born && 40.0 < B.Born + B.Life)
				{
					Mine += B.DeckShip == 2 ? 1 : 0;
					Far += B.DeckShip == 3 ? 1 : 0;
					Third += B.DeckShip == 4 ? 1 : 0;
				}
			}
			Check(Mine == 20 && Far == 0 && Third > 100 && X.DeckCases.size() <= 200, "a far ship's rounds never take the cases in view", Say("in view %.0f of 20 after 40 s; the ship 800 m off %.0f laid; the one 70 to 80 m off %.0f pieces of hers (the fading among them); %.0f counted lying in all (the cap 200)", Mine, Far, Third, static_cast<double>(X.DeckCases.size())));
		}
		{
			// (a ship let go: her cases fade out over a second where they lie, a fade and never a pop)
			Battle X;
			Open(X, Book);
			const Battle::ShipPose Q = Pose(0.0, 0.0, 0.0);
			const Vec Mount = ToWorld(Q, Vec(0.0, -8.0, 12.0));
			X.ShipRide(0.0, 1, Q);
			X.DeckBrass(0.0, 1, Mount, Vec(-1.0, 0.0, 0.0), "aa40", 10);
			for (int F = 1; F <= 60; ++F)
			{
				X.ShipRide(F / 30.0, 1, Q);
				LookAt(X, F / 30.0, Mount, 15.0);
			}
			X.ShipRideStop(1);
			LookAt(X, 2.0 + 1.0 / 30.0, Mount, 15.0);
			int Next = 0, Late = 0, Still = 0;
			for (const Bit& B : X.Bits)
			{
				if (B.Form == Bit::Shape::Plate && B.bThrown)
				{
					Next += 2.0 + 1.0 / 30.0 < B.Born + B.Life ? 1 : 0;
					Still += 2.5 < B.Born + B.Life ? 1 : 0;
					Late += 3.1 < B.Born + B.Life ? 1 : 0;
				}
			}
			Check(Next == 10 && Still == 10 && Late == 0, "a ship let go: her cases fade out over a second where they lie", Say("%.0f of 10 still drawn the frame after, %.0f half a second on, %.0f after 1.1 s", Next, Still, Late));
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

	// ---------------------------------------------------------------------------------------------------------------
	// E. The AA haze (item 11 part 3): the overlay battle-fx-sea-haze.json

	/** The fleet of two-lines.json (Content/Data/SeaBattles): Britain's 3 battleships, 4 cruisers and 8 destroyers in a
	 * line 400 m apart, Germany's 2, 5 and 15 in another 1,500 m off, their guns as ShipGuns fits them in June 1944, every
	 * AA gun firing up at a raid overhead for Seconds; drawn at 10 frames a second from 400 m off Britain's line, 30 m
	 * up, looking along it. */
	struct Barrage
	{
		int Ships = 0, LightMounts = 0, Streams = 0, HeavyBarrels = 0;
		int MostHanging = 0, MostHazeEnds = 0, ShedFrames = 0, ShedLeft = 0, Hung = 0, LateHangs = 0, MostKept = 0, SeenFrames = 0, ShortFrames = 0, FullSeenFrames = 0, FullShortFrames = 0, MostOnScreen = 0;
		double MostRoom = 0.0, HangLeast = 1e9, HangMost = 0.0, LingerMost = 0.0, SmokeMost = 0.0, Ms = 0.0;
		int Tails = 0;
		uint64_t Seeds = 0;
		std::vector<std::string> After;
	};
	Barrage Fleet(const JsonRecipe& Book, double Seconds, int Only = 0)
	{
		Barrage R;
		Battle X;
		Open(X, Book);
		struct Gun
		{
			int Key = 0;
			Vec At;
			const ShipGuns::Piece* P = nullptr;
			double Next = 0.0;
		};
		std::vector<Gun> Light, Heavy;
		const char* Sides[2] = { "UK", "GER" };
		const int Fit[2][3] = { { 3, 4, 8 }, { 2, 5, 15 } };
		const char* Classes[3] = { "bb", "ca", "dd" };
		int Ship = 0;
		for (int S = 0; S < 2; ++S)
		{
			int InLine = 0;
			for (int C = 0; C < 3; ++C)
			{
				for (int N = 0; N < Fit[S][C]; ++N, ++Ship, ++InLine)
				{
					if (Only > 0 && Ship >= Only)
					{
						continue;
					}
					++R.Ships;
					const Vec Mid(400.0 * InLine, 1500.0 * S, 0.0);
					const ShipGuns::Battery B = ShipGuns::BatteryOf(Sides[S], Classes[C], 1944, 6);
					const double Deck = ShipGuns::DeckOf(Classes[C]);
					int Stream = 0, Spot = 0;
					for (const ShipGuns::Mount& M : B.Mounts)
					{
						for (int K = 0; K < M.Count; ++K, ++Spot)
						{
							const Vec At = Mid + Vec(-60.0 + 7.0 * (Spot % 18), (Spot % 2 == 0 ? 1.0 : -1.0) * 6.0, Deck + 2.0);
							if (M.Gun->bHeavy)
							{
								if (M.Gun->bLow)
								{
									continue;
								}
								for (int Br = 0; Br < M.Gun->Barrels; ++Br)
								{
									Heavy.push_back({ 0, At + Vec(0.0, 0.5 * Br, 0.0), M.Gun, 0.37 * Br + 0.11 * Spot });
								}
								R.HeavyBarrels += M.Gun->Barrels;
							}
							else
							{
								++R.LightMounts;
								for (int Br = 0; Br < M.Gun->Barrels; ++Br)
								{
									Light.push_back({ ShipGuns::StreamKey(Ship, Stream++ % 60), At + Vec(0.0, 0.3 * Br, 0.0), M.Gun, 0.0 });
								}
							}
						}
					}
				}
			}
		}
		R.Streams = static_cast<int>(Light.size());
		const Vec Eye(2800.0, -400.0, 30.0);
		const Vec Look(2800.0, 400.0, 150.0);
		Battle::View View = FxLamps::Looking(Eye, Look);
		View.HalfAcross = std::tan(0.5 * 55.0 * Pi / 180.0); // (the screen's width known, as the game tells it: the room that can be drawn is the screen's)
		View.HalfUp = View.HalfAcross * 9.0 / 16.0;
		const size_t Keep = static_cast<size_t>(Book.Get("sea_haze.keep", 0.0));
		double Spent = 0.0;
		for (int F = 0; F <= static_cast<int>(Seconds * 10.0); ++F)
		{
			const double T = F / 10.0;
			const auto T0 = std::chrono::steady_clock::now();
			for (const Gun& G : Light)
			{
				// (each laid on a raid overhead, swinging slowly)
				const double Swing = 0.4 * std::sin(0.2 * T + 0.1 * G.Key);
				X.AirGun(T, G.Key, G.At, G.At + Vec(std::cos(Swing), std::sin(Swing), 1.6) * 2000.0, Vec(), G.P->Kind, G.P->Rate, G.P->Mv, "");
			}
			for (Gun& G : Heavy)
			{
				while (G.Next <= T)
				{
					X.Muzzle(G.Next, G.At, Vec(0.4, 0.2, 0.9), "ship_aa", "water");
					G.Next += 60.0 / std::max(G.P->Rate, 1.0);
				}
			}
			const double Room = static_cast<double>(X.RoomSeen()) / std::max(1, X.MaxPuffs);
			R.MostRoom = std::max(R.MostRoom, Room);
			X.Draw(T, View);
			Spent += Since(T0);
			int Hanging = 0, Shown = 0, KeptSeen = 0, OnScreen = 0;
			for (const auto& O : X.Sorted)
			{
				// (what the frame lays: those on the screen (Sorted), the kept counted first. A hanging puff is kept, so it
				// is laid whenever every kept piece fits the room: each takes, at the most, 3 places a spent case, 2 one
				// that may twin, else 1 (Draw's Takes))
				KeptSeen += !O.bKeep ? 0 : O.B->bThrown && O.B->Form == Bit::Shape::Plate ? 3 : O.B->TwinPx > 0.0 ? 2 : 1;
				Shown += O.B->bHaze && T < O.B->CutAt ? 1 : 0;
			}
			R.MostKept = std::max(R.MostKept, KeptSeen);
			Shown = KeptSeen <= X.MaxPuffs ? Shown : 0;
			for (const Bit& B : X.Bits)
			{
				if (B.bHaze && T >= B.Born && T < B.Born + B.Life && T < B.CutAt)
				{
					++Hanging;
					OnScreen += X.OffScreen(B, X.Where(B, T - B.Born), View) ? 0 : 1;
					if (B.bTail)
					{
						R.LingerMost = std::max(R.LingerMost, B.Life);
					}
					else
					{
						R.HangLeast = std::min(R.HangLeast, B.Life);
						R.HangMost = std::max(R.HangMost, B.Life);
					}
					R.LateHangs += Room >= 0.8 && B.Born > T - 0.1 && static_cast<size_t>(Hanging) > Keep ? 1 : 0; // (a new one hung past 4/5 over the haze's share)
				}
				R.Tails += B.bTail && B.Born > T - 0.1 && B.Born <= T ? 1 : 0;
				if (!B.bTail && !B.bHaze && B.Form == Bit::Shape::Puff && B.Drag == 4.0 && B.Windage == 1.0)
				{
					R.SmokeMost = std::max(R.SmokeMost, B.Life); // (a light gun's smoke puff, as AirRound lays it)
				}
			}
			R.MostHanging = std::max(R.MostHanging, Hanging);
			// (the frames with hanging puffs on the screen, and those in which fewer were laid than were on it: the wind
			// takes the haze along the line and out of this view, so the frames are counted, not every frame)
			R.MostOnScreen = std::max(R.MostOnScreen, OnScreen);
			R.SeenFrames += OnScreen > 0 ? 1 : 0;
			R.ShortFrames += OnScreen > 0 && Shown < OnScreen ? 1 : 0;
			R.MostHazeEnds = std::max(R.MostHazeEnds, static_cast<int>(std::count_if(X.HazeEnds.begin(), X.HazeEnds.end(), [&](double E) { return E > T; })));
			if (Room >= 0.8)
			{
				++R.ShedFrames;
				R.ShedLeft = std::max(R.ShedLeft, Hanging);
				R.FullSeenFrames += OnScreen > 0 ? 1 : 0; // (the haze's share drawn, through the barrage's full room)
				R.FullShortFrames += OnScreen > 0 && Shown < OnScreen ? 1 : 0;
			}
		}
		R.Ms = Spent / (Seconds * 10.0 + 1.0);
		R.Hung = static_cast<int>(X.HazeHangs);
		// (40 s after the guns stop, every piece of the barrage gone: an unrelated shell and muzzle, printed)
		X.Draw(Seconds + 40.0, View);
		X.Draw(Seconds + 40.1, View);
		const size_t From = X.Bits.size();
		X.Shell(Seconds + 41.0, Vec(300.0, 40.0, 0.0), "how105", "water", false);
		X.Muzzle(Seconds + 41.5, Vec(280.0, 60.0, 9.0), Vec(0.3, 0.2, 0.9), "naval_medium", "water");
		for (size_t I = From; I < X.Bits.size(); ++I)
		{
			R.After.push_back(Print(X.Bits[I]));
		}
		R.Seeds = X.Seeds;
		return R;
	}

	void Haze(const JsonRecipe& Book, const JsonRecipe& On)
	{
		std::printf("\nE. The AA haze: the overlay battle-fx-sea-haze.json laid over the recipe, the 37 ships of two-lines.json firing every AA gun for 60 s, seen from 400 m\n");
		const Barrage Off = Fleet(Book, 60.0);
		const Barrage Hz = Fleet(On, 60.0);
		std::printf("    the fleet: %d ships, %d light AA mounts (%d barrels, each a stream), %d heavy AA barrels (Muzzle ship_aa)\n", Off.Ships, Off.LightMounts, Off.Streams, Off.HeavyBarrels);
		std::printf("    (the room as RoomSeen counts it, what the screen can draw, of game.max_puffs)\n    off: the room at most %.2f full; a light gun's smoke at most %.2f s; %d lingering pieces of the 5-inch\n", Off.MostRoom, Off.SmokeMost, Off.Tails);
		std::printf("    on:  the room at most %.2f full; %d hanging at most at once (%d counted); %d frames past 4/5 of the room, %d hanging at most then, %d frames with hanging puffs on the screen (%d at most), %d of them with fewer laid (the kept on the screen taking %d places at most)\n", Hz.MostRoom, Hz.MostHanging, Hz.MostHazeEnds, Hz.ShedFrames, Hz.ShedLeft, Hz.SeenFrames, Hz.MostOnScreen, Hz.ShortFrames, Hz.MostKept);
		std::printf("    time: the barrage's frame (every gun and the draw) %.2f ms off, %.2f ms on, on this box\n", Off.Ms, Hz.Ms);
		const double SmokeS = Book.Get("air_guns.aa40.smoke_s", 0.0) * 1.2;
		Check(Off.MostHanging == 0 && Off.Tails == 0 && Off.SmokeMost <= SmokeS + 1e-9 && Off.Hung == 0, "off: no puff hangs, the 5-inch's smoke does not linger, the light guns' smoke as its rows", Say("%.0f hanging; %.0f lingering; the longest light-gun puff %.2f s (the rows' most %.2f)", Off.MostHanging, Off.Tails, Off.SmokeMost, SmokeS));
		Check(Hz.HangLeast >= 6.0 - 1e-9 && Hz.HangMost <= 10.0 + 1e-9 && Hz.HangMost > 6.0, "on: a light gun's hanging puff lives 6 to 10 s", Say("%.2f to %.2f s", Hz.HangLeast, Hz.HangMost));
		Check(Hz.LingerMost > 6.0 && Hz.LingerMost <= On.Get("linger.gun.by.ship_aa.gone_s", 0.0) + 1e-9, "on: the 5-inch's smoke lingers, gone within linger.gun.by.ship_aa's 10 s", Say("the longest %.2f s", Hz.LingerMost));
		Check(Hz.MostHanging <= 250 && Hz.MostHazeEnds <= 250 && Hz.MostHanging >= 200, "on: at most 250 hanging at once through the barrage (sea_haze.most)", Say("%.0f at most (%.0f counted)", Hz.MostHanging, Hz.MostHazeEnds));
		Check(Hz.SeenFrames >= 50 && Hz.MostOnScreen >= 30 && Hz.ShortFrames == 0, "on: the barrage's haze is drawn: every hanging puff on the screen is laid", Say("%.0f frames with hanging puffs on the screen (%.0f at most), %.0f with fewer laid; the screen's room at most %.2f full", Hz.SeenFrames, Hz.MostOnScreen, Hz.ShortFrames, Hz.MostRoom));
		// (the screen's room full: the same barrage with the room cut to 1,000 (game.max_puffs), so the screen asks for
		// over all of it)
		const double KeepN = On.Get("sea_haze.keep", 0.0);
		JsonRecipe Full = On;
		Full.Numbers["game.max_puffs"] = 1000.0;
		const Barrage Fu = Fleet(Full, 60.0);
		std::printf("    the room cut to 1,000: the screen's room at most %.2f full, %d frames past 4/5, %d hanging at most then (keep %.0f), %d of them with hanging puffs on the screen, %d with fewer laid (the kept on the screen taking %d places at most)\n", Fu.MostRoom, Fu.ShedFrames, Fu.ShedLeft, KeepN, Fu.FullSeenFrames, Fu.FullShortFrames, Fu.MostKept);
		Check(Fu.ShedFrames > 100 && Fu.LateHangs == 0 && Fu.ShedLeft <= KeepN, "the screen's room full: those over the haze's share (keep) shed first, none hangs anew over it", Say("%.0f frames past 4/5; %.0f new over the share, %.0f hanging at most (keep %.0f)", Fu.ShedFrames, Fu.LateHangs, Fu.ShedLeft, KeepN));
		Check(Fu.FullSeenFrames >= 20 && Fu.FullShortFrames == 0, "  ...and the haze's share is drawn through it: every hanging puff on the screen laid (a full barrage still shows its haze)", Say("%.0f full frames with hanging puffs on the screen, %.0f with fewer laid", Fu.FullSeenFrames, Fu.FullShortFrames));
		Check(Off.Seeds == Hz.Seeds && Off.After == Hz.After && !Off.After.empty(), "on: no draw on the shared seed; a shell and a muzzle after it the same to the bit", Say("%.0f pieces compared", static_cast<double>(Off.After.size())) + (Off.Seeds == Hz.Seeds ? "; the shared seed the same" : "; the shared seed MOVED"));
		{
			// (each of her clouds keeps puffs of its own: the 5-inch fired eight times in the same place, which of each
			// cloud's puffs stay, as a pattern, not all the same; and a ship's medium gun (its smoke lingers by its own row) after them as without them)
			auto Clouds = [&](const JsonRecipe& Rb, int Shots, std::vector<std::string>& Kept, std::string& After)
			{
				Battle Z;
				Open(Z, Rb);
				for (int K = 0; K < Shots; ++K)
				{
					const size_t From = Z.Bits.size();
					Z.Muzzle(1.0 + 3.0 * K, Vec(280.0, 60.0, 9.0), Vec(0.3, 0.2, 0.9), "ship_aa", "water");
					std::string Pattern;
					for (size_t I = From; I < Z.Bits.size(); ++I)
					{
						Pattern += Z.Bits[I].bTail ? 'x' : '.';
					}
					Kept.push_back(Pattern);
				}
				const size_t From = Z.Bits.size();
				Z.Muzzle(1.0 + 3.0 * Shots, Vec(250.0, 30.0, 9.0), Vec(0.3, 0.2, 0.3), "naval_medium", "water");
				for (size_t I = From; I < Z.Bits.size(); ++I)
				{
					After += Print(Z.Bits[I]);
				}
			};
			std::vector<std::string> Kept, KeptOff;
			std::string After, AfterOff;
			Clouds(On, 8, Kept, After);
			Clouds(Book, 8, KeptOff, AfterOff);
			int Differ = 0;
			for (size_t K = 1; K < Kept.size(); ++K)
			{
				Differ += Kept[K] != Kept[0] && Kept[K].find('x') != std::string::npos ? 1 : 0;
			}
			Check(Differ >= 5 && !After.empty() && After == AfterOff, "on: each of her clouds keeps puffs of its own; another gun's smoke after them as without the overlay", Say("%.0f of 7 later clouds keep other puffs than the first", Differ) + (After == AfterOff ? "; the medium gun's muzzle the same" : "; the medium gun's muzzle MOVED"));
		}
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
	JsonRecipe On = Book;
	if (!FxLamps::LoadRecipe(Argv[2], On))
	{
		return 2;
	}
	Haze(Book, On);
	std::printf("\n%d checks, %d failed\n%s\n", Checks, Failed, Failed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
	std::printf("time: the cost checks %s\n", bTimeFailed ? "FAILED" : "passed");
	return Failed == 0 && !bTimeFailed ? 0 : 1;
}
