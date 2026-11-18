#include "Effects/TheaterFxCore.h"

#include <algorithm>
#include <cmath>

namespace TheaterFx
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		const Vec ZUp(0.0, 0.0, 1.0);

		double Clamp01(double V)
		{
			return V < 0.0 ? 0.0 : (V > 1.0 ? 1.0 : V);
		}

		float Linear(double Picture255)
		{
			const double C = Clamp01(Picture255 / 255.0);
			return static_cast<float>(C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4));
		}

		uint8_t Byte(double V)
		{
			return static_cast<uint8_t>(std::lround(Clamp01(V) * 255.0));
		}

		uint64_t Mix64(uint64_t V)
		{
			V += 0x9E3779B97F4A7C15ull;
			V = (V ^ (V >> 30)) * 0xBF58476D1CE4E5B9ull;
			V = (V ^ (V >> 27)) * 0x94D049BB133111EBull;
			return V ^ (V >> 31);
		}

		/** A light shorter than a frame (a flash, a flicker, a close tracer) that no frame has laid yet: it is kept, and
		 * laid at full strength on the next frame, so a battle at 30 frames a second or less still sees it. */
		bool Unseen(const TheaterFx::Bit& B)
		{
			return B.bLight && B.Life < 0.06 && !B.bShown;
		}

		uint64_t PlaceKey(const Vec& At)
		{
			const uint64_t A = static_cast<uint64_t>(static_cast<int64_t>(std::floor(At.X * 10.0)));
			const uint64_t B = static_cast<uint64_t>(static_cast<int64_t>(std::floor(At.Y * 10.0)));
			return Mix64(A * 73856093ull ^ B * 19349663ull);
		}

		double Radians(double Degrees)
		{
			return Degrees * Pi / 180.0;
		}
	}

	double Vec::Length() const
	{
		return std::sqrt(X * X + Y * Y + Z * Z);
	}

	Vec Vec::Unit(const Vec& Otherwise) const
	{
		const double L = Length();
		return L > 1e-9 ? Vec(X / L, Y / L, Z / L) : Otherwise;
	}

	Rgb Rgb::FromPicture(double R255, double G255, double B255)
	{
		return Rgb(Linear(R255), Linear(G255), Linear(B255));
	}

	Rgb Rgb::operator*(double S) const
	{
		return Rgb(static_cast<float>(R * S), static_cast<float>(G * S), static_cast<float>(B * S));
	}

	Rgb Rgb::Mix(const Rgb& O, double T) const
	{
		const float W = static_cast<float>(Clamp01(T));
		return Rgb(R + (O.R - R) * W, G + (O.G - G) * W, B + (O.B - B) * W);
	}

	Dice::Dice(uint64_t Seed)
	{
		State = Mix64(Seed);
		if (State == 0)
		{
			State = 1;
		}
	}

	uint64_t Dice::Next()
	{
		uint64_t V = State;
		V ^= V >> 12;
		V ^= V << 25;
		V ^= V >> 27;
		State = V;
		return V * 0x2545F4914F6CDD1Dull;
	}

	double Dice::U()
	{
		return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0);
	}

	double Dice::U(double A, double B)
	{
		return A + (B - A) * U();
	}

	int Dice::Whole(int A, int B)
	{
		if (B <= A)
		{
			return A;
		}
		const int Span = B - A + 1;
		return std::min(B, A + static_cast<int>(U() * Span));
	}

	double Dice::Bell()
	{
		const double A = std::max(U(), 1e-12), B = U();
		return std::sqrt(-2.0 * std::log(A)) * std::cos(2.0 * Pi * B);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Reading the recipe

	void Battle::Setup(const Recipe* InRecipe, GroundFn InGround, void* InContext, uint32_t Seed)
	{
		Book = InRecipe;
		GroundAt = InGround;
		GroundContext = InContext;
		Seeds = static_cast<uint64_t>(Seed) * 2654435761ull + 1ull;
		MaxPuffs = std::max(100, static_cast<int>(Num("game.max_puffs", 3000.0)));
		ShadeLit = Num("game.shade_lit", 0.18);
		ShadeDark = Num("game.shade_dark", 0.28);
		ClodLeast = Num("game.clod_least_px", 1.2);
		Beyond = Num("game.draw_beyond_m", 1500.0);
		ClodRough = Range("burst.debris.clods.rough", 0.25, 0.45);
		ClodTurns = Range("burst.debris.clods.tumble_turns_a_second", 0.5, 2.0);
		ClodShade = Num("burst.debris.clods.shade", 2.5);
		TracerHalf = Num("tracer.streak_m", 16.0) * 0.5;
		TracerLeast = Num("tracer.least_px", 3.0);
		TracerCol = Picture("tracer.colour", Rgb::FromPicture(213, 199, 104));
		TracerPale = TracerCol.Mix(Rgb(1.0f, 1.0f, 1.0f), 0.55);
		TracerDayWide = Num("tracer.day_width", 1.5);
		WholePathBelow = Num("tracer.whole_path_below_m", 100.0);
		DayFrom = Num("game.day_light_from", 0.38);
		DayFull = std::max(DayFrom + 0.01, Num("game.day_light_full", 0.9));
		DayLampShare = Num("game.day_lamp_share", 0.25);
		DayLampCol = Picture("game.day_lamp_colour", Rgb::FromPicture(255, 240, 215));
		TracerDayCore = Picture("tracer.day_core", Rgb::FromPicture(245, 240, 210));
		CannonHeartCol = Picture("air_guns.flash_heart", Rgb::FromPicture(240, 220, 150));
		FlameCol = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		FlashCoreCol = Picture("burst.flash.core_colour", Rgb::FromPicture(236, 168, 72));
		GunSmokeCol = Picture("air_guns.smoke_colour", Rgb::FromPicture(168, 166, 160));
		AirRows.clear();
		TrailRows.clear();
		Sides = std::max(6, std::min(24, static_cast<int>(Num("game.puff_sides", 16.0))));
		PointsPerPuff = 2 * Sides + 1;
		std::vector<double>* Tables[8] = { &RingCos, &RingSin, &Ring2Cos, &Ring2Sin, &Ring3Cos, &Ring3Sin, &Ring5Cos, &Ring5Sin };
		for (std::vector<double>* T : Tables)
		{
			T->assign(static_cast<size_t>(Sides), 0.0);
		}
		for (int K = 0; K < Sides; ++K)
		{
			const double Angle = K * 2.0 * Pi / Sides;
			const size_t I = static_cast<size_t>(K);
			RingCos[I] = std::cos(Angle);
			RingSin[I] = std::sin(Angle);
			Ring2Cos[I] = std::cos(2.0 * Angle);
			Ring2Sin[I] = std::sin(2.0 * Angle);
			Ring3Cos[I] = std::cos(3.0 * Angle);
			Ring3Sin[I] = std::sin(3.0 * Angle);
			Ring5Cos[I] = std::cos(5.0 * Angle);
			Ring5Sin[I] = std::sin(5.0 * Angle);
		}
		Out.assign(static_cast<size_t>(MaxPuffs) * PointsPerPuff, Vec());
		OutRgba.assign(static_cast<size_t>(MaxPuffs) * PointsPerPuff * 4, 0);
		Clear();
	}

	void Battle::Clear()
	{
		Bits.clear();
		Marks.clear();
		Flashes.clear();
		Knocks.clear();
		Sources.clear();
		GunRounds.clear();
		Firings.clear();
		Trails.clear();
		for (size_t I = 0; I < OutRgba.size(); ++I)
		{
			OutRgba[I] = 0;
		}
		LastDrawn = 0;
	}

	double Battle::Num(const std::string& Path, double Fallback) const
	{
		double V = 0.0;
		return Book && Book->Find(Path.c_str(), V) ? V : Fallback;
	}

	bool Battle::Has(const std::string& Path) const
	{
		double V = 0.0;
		return Book && Book->Find(Path.c_str(), V);
	}

	double Battle::Pick(Dice& D, const std::string& Path, double Low, double High) const
	{
		const double A = Num(Path + ".0", Num(Path, Low));
		const double B = Num(Path + ".1", Has(Path + ".0") ? A : High);
		return D.U(A, B);
	}

	Battle::Span Battle::Range(const std::string& Path, double Low, double High) const
	{
		Span S;
		S.A = Num(Path + ".0", Num(Path, Low));
		S.B = Num(Path + ".1", Has(Path + ".0") ? S.A : High);
		return S;
	}

	Rgb Battle::Picture(const std::string& Path, const Rgb& Fallback) const
	{
		if (!Has(Path + ".0"))
		{
			return Fallback;
		}
		return Rgb::FromPicture(Num(Path + ".0", 0.0), Num(Path + ".1", 0.0), Num(Path + ".2", 0.0));
	}

	double Battle::Height(double X, double Y) const
	{
		return GroundAt ? GroundAt(GroundContext, X, Y) : 0.0;
	}

	Battle::Paint Battle::PaintOf(const std::string& Ground) const
	{
		const std::string P = "grounds." + Ground + ".paint.";
		Paint Out_;
		Out_.Soil = Picture(P + "soil", Rgb::FromPicture(66, 55, 44));
		Out_.Heavy = Picture(P + "heavy", Rgb::FromPicture(50, 41, 33));
		Out_.Dust = Picture(P + "dust", Rgb::FromPicture(150, 134, 108));
		Out_.Clod = Picture(P + "clod", Rgb::FromPicture(40, 33, 26));
		Out_.Smoke = Picture(P + "smoke", Rgb::FromPicture(72, 62, 52));
		Out_.bFirst = Has(P + "first.0");
		Out_.First = Picture(P + "first", Out_.Dust);
		Out_.Grain = std::max(0.2, std::min(1.0, Num(P + "grain", 1.0)));
		Out_.FirstAtFoot = Num(P + "first_share.0", 0.0);
		Out_.FirstAtTip = Num(P + "first_share.1", 1.0);
		Out_.bSplinter = Has(P + "splinter.0");
		Out_.Splinter = Picture(P + "splinter", Out_.Clod);
		return Out_;
	}

	std::string Battle::SizeOfCalibre(const char* Calibre) const
	{
		const std::string Cal = Calibre ? Calibre : "";
		std::string Size;
		if (Book && Book->FindWord(("from_shell_cal." + Cal).c_str(), Size) && Has("sizes." + Size + ".scale"))
		{
			return Size;
		}
		// (as the recipe has it, for a reader that cannot give words)
		if (Cal == "field")
		{
			return "gun75";
		}
		if (Cal == "howitzer")
		{
			return "how105";
		}
		if (Cal == "heavy")
		{
			return "how150";
		}
		return Has("sizes." + Cal + ".scale") ? Cal : std::string("how105");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Where a thing is

	Vec Battle::Where(const Bit& B, double Age) const
	{
		if (B.bThrown)
		{
			if (Age < B.Land)
			{
				return B.From + B.Vel * Age + Vec(0.0, 0.0, -0.5 * B.Gravity * Age * Age);
			}
			if (Age < B.Land2)
			{
				const double A = Age - B.Land;
				return B.At1 + B.Vel2 * A + Vec(0.0, 0.0, -0.5 * B.Gravity * A * A);
			}
			return B.At2;
		}
		const double Kd = B.Drag;
		const double Go = Kd > 1e-6 ? (1.0 - std::exp(-Kd * Age)) / Kd : Age;
		Vec P = B.From + B.Vel * Go + Wind * (B.Windage * Age) + Vec(0.0, 0.0, B.Rise * Age - 0.5 * B.Gravity * Age * Age);
		if (B.bHug)
		{
			P.Z = Height(P.X, P.Y) + B.Rise * Age;
		}
		return P;
	}

	Vec Battle::Going(const Bit& B, double Age) const
	{
		if (B.bThrown)
		{
			if (Age < B.Land)
			{
				return B.Vel + Vec(0.0, 0.0, -B.Gravity * Age);
			}
			if (Age < B.Land2)
			{
				return B.Vel2 + Vec(0.0, 0.0, -B.Gravity * (Age - B.Land));
			}
			return Vec();
		}
		return B.Vel * std::exp(-B.Drag * Age) + Wind * B.Windage + Vec(0.0, 0.0, B.Rise - B.Gravity * Age);
	}

	void Battle::Throw(Bit& B) const
	{
		// (true flight under gravity: a clod thrown 10 m up comes down about 2.8 s later; the big ones bounce once)
		auto Fall = [](const Vec& Start, const Vec& Speed, double G, double T)
		{
			return Start + Speed * T + Vec(0.0, 0.0, -0.5 * G * T * T);
		};
		double T = 0.02;
		bool bHit = false;
		for (; T < 8.0; T += 0.01)
		{
			const Vec P = Fall(B.From, B.Vel, B.Gravity, T);
			if (B.Vel.Z - B.Gravity * T < 0.0 && P.Z <= Height(P.X, P.Y))
			{
				bHit = true;
				break;
			}
		}
		B.Land = T;
		B.At1 = Fall(B.From, B.Vel, B.Gravity, T);
		B.At1.Z = Height(B.At1.X, B.At1.Y) + B.R0 * 0.5;
		if (bHit && B.R0 > 0.05)
		{
			B.Vel2 = Vec(B.Vel.X * 0.35, B.Vel.Y * 0.35, std::fabs(B.Vel.Z - B.Gravity * T) * 0.22);
			double T2 = 0.02;
			for (; T2 < 4.0; T2 += 0.01)
			{
				const Vec P = Fall(B.At1, B.Vel2, B.Gravity, T2);
				if (B.Vel2.Z - B.Gravity * T2 < 0.0 && P.Z <= Height(P.X, P.Y) + B.R0 * 0.5)
				{
					break;
				}
			}
			B.Land2 = T + T2;
			B.At2 = Fall(B.At1, B.Vel2, B.Gravity, T2);
			B.At2.Z = Height(B.At2.X, B.At2.Y) + B.R0 * 0.5;
		}
		else
		{
			B.Vel2 = Vec();
			B.Land2 = B.Land;
			B.At2 = B.At1;
		}
	}

	void Battle::Chunk(Bit& B, Dice& D) const
	{
		// (a clod is an irregular chunk, never a smooth bean: a lumpy outline of its own, tumbling as it flies, with a
		// lit face toward the sun and a dark one away from it)
		B.Rough = ClodRough.Pick(D);
		B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
		B.Turn = D.U(0.0, 2.0 * Pi);
		const double Turns = ClodTurns.Pick(D); // (then which way: two throws, in this order)
		B.TurnRate = Turns * 2.0 * Pi * (D.U() < 0.5 ? -1.0 : 1.0);
		B.ShadeK = ClodShade;
		B.Point = D.U(0.0, 0.3);
		B.Tail = D.U(0.0, 0.25);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// A shell landing

	void Battle::Shell(double Now, const Vec& AtIn, const char* SizeIn, const char* GroundIn, bool bOnArmour)
	{
		std::string Size = SizeIn ? SizeIn : "how105";
		if (!Has("sizes." + Size + ".scale"))
		{
			Size = "how105";
		}
		std::string Ground = GroundIn ? GroundIn : "earth";
		if (!Has("grounds." + Ground + ".paint.soil.0"))
		{
			Ground = "earth";
		}
		Dice D(++Seeds * 7919ull ^ PlaceKey(AtIn));
		const std::string S = "sizes." + Size + ".";
		const double Scale = Num(S + "scale", 1.0);
		const double K = std::sqrt(Scale);
		const double Within = Num(S + "shake_within_m", 40.0);
		if (bOnArmour)
		{
			Armour(Now, AtIn, Scale, D);
		}
		else
		{
			const Vec At(AtIn.X, AtIn.Y, Height(AtIn.X, AtIn.Y));
			const std::string G = "grounds." + Ground + ".";
			const Paint P = PaintOf(Ground);
			const double Slow = Num(G + "crown_slow", 1.0);
			const double DustK = Num(G + "dust", 1.0);
			const double ClodK = Ground == "water" ? 0.0 : Num(G + "clods", 1.0);
			const double CrownH = Pick(D, S + "crown_m", 8.0, 12.0);
			Wink(Now, At, Scale, D);
			Skirt(Now + 0.01, At, K, P, DustK, D);
			Crown(Now + 0.005, At, CrownH, K, Slow, P, ClodK, Size, D);
			Smoke(Now, At, CrownH, K, Scale, Size, Ground, P, DustK, D);
			const double Across = Num(S + "crater_m", 1.5);
			if (Across > 0.0 && Ground != "water")
			{
				Scar(Now + 0.25, At, Across, P, Ground == "snow", D);
			}
		}
		Knock J;
		J.At = AtIn;
		J.Born = Now;
		J.Strength = std::min(1.0, Scale) * (bOnArmour ? 0.5 : 1.0);
		J.Within = Within;
		J.Push = Num("camera.shake.push_m." + Size, 0.0);
		Knocks.push_back(J);
	}

	void Battle::Wink(double T0, const Vec& At, double Scale, Dice& D)
	{
		// The flash: a small low star of ragged flame under one frame, a glow left in the crown's foot for 3 to 5
		// frames, and a real lamp for about one frame (it lifts what is within 30 m, never a white-out).
		const double K = std::sqrt(Scale);
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Rgb Core = Picture("burst.flash.core_colour", Rgb::FromPicture(236, 168, 72));
		const double Small = std::min(1.0, 1.2 / std::pow(std::max(K, 1e-3), 0.3));
		for (int I = 0; I < 7; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			const double H = D.U(0.15, 0.8);
			const double W = 0.55 + 0.9 * H;
			const Vec Dir = Vec(std::cos(A) * W, std::sin(A) * W, 0.8).Unit();
			const double Rr = D.U(0.38, 0.7) * K * (I ? 0.72 : 0.45) * Small;
			Bit B;
			B.Born = T0;
			B.From = At + ZUp * 0.04 + Vec(D.U(-0.3, 0.3), D.U(-0.3, 0.3), 0.0) * (K * 1.3) + Dir * (Rr * 0.85);
			B.Vel = Dir * 4.0;
			B.Drag = 6.0;
			B.Life = D.U(0.028, 0.045);
			B.R0 = Rr * 0.8;
			B.R1 = Rr * 1.2;
			B.Grow = 0.4;
			B.Along = Dir;
			B.Stretch = 2.6;
			B.Point = 0.9;
			B.Col = I == 0 ? Core : Flame;
			B.Col2 = Flame * 0.5;
			B.Alpha = 1.0;
			B.FadeIn = 0.004;
			B.Hold = 0.3;
			B.Heart = 0.8;
			B.Rim = 0.1;
			B.bLight = true;
			Bits.push_back(B);
		}
		const Rgb Glow = Picture("burst.crown.foot_glow.colour", Flame);
		for (int I = 0; I < 5; ++I)
		{
			const Vec Dir = (ZUp + Vec(D.U(-0.5, 0.5), D.U(-0.5, 0.5), 0.0)).Unit();
			const double Rr = D.U(0.25, 0.45) * K;
			Bit B;
			B.Born = T0 + 0.03;
			B.From = At + ZUp * (0.25 * K) + Dir * (Rr * 0.5);
			B.Vel = Dir * 1.5;
			B.Drag = 4.0;
			B.Life = D.U(0.1, 0.16);
			B.R0 = Rr * 0.8;
			B.R1 = Rr * 1.1;
			B.Along = Dir;
			B.Stretch = 1.8;
			B.Point = 0.6;
			B.Col = Glow * 0.8;
			B.Col2 = Rgb(0.05f, 0.01f, 0.0f);
			B.Alpha = 0.85;
			B.FadeIn = 0.01;
			B.Hold = 0.2;
			B.Heart = 0.7;
			B.bLight = true;
			Bits.push_back(B);
		}
		Flash F;
		F.At = At + ZUp * (Num("burst.flash.light.height_m", 16.0) * K); // (high, so the lift falls off gently over 30 m)
		F.Col = Picture("burst.flash.light.colour", Rgb::FromPicture(255, 196, 140));
		F.Born = T0;
		F.Life = 0.085;
		F.Strength = Scale;
		F.Radius = Num("burst.flash.light.radius_m", 40.0) * std::max(1.0, K);
		Flashes.push_back(F);
	}

	void Battle::Skirt(double T0, const Vec& At, double K, const Paint& P, double DustK, Dice& D)
	{
		// The shock: one low skirt of the ground's own dust, racing out and thinning, with a soft billowing top that
		// settles to a low haze by about frame 30. Upright cards, so that its foot never cuts into the ground.
		const std::string R = "burst.shock_ring.";
		const std::string Sk = R + "skirt.";
		const double Reach = Num(R + "reach_m", 15.0) * K;
		const Rgb Col = P.Dust * Num(R + "colour_from_ground", 0.85);
		const double Many = std::max(0.7, std::sqrt(K));
		const int NOut = static_cast<int>(Num(Sk + "puffs_outer", 110.0) * Many);
		const int NIn = static_cast<int>(Num(Sk + "puffs_inner", 60.0) * Many);
		const double HazeAlpha = Num(Sk + "haze_alpha", 0.15) * std::pow(DustK, 0.4);
		// (every number the loop needs, read once)
		const double DragOf = Num(Sk + "drag", 8.0), RiseOf = Num(Sk + "rise_mps", 0.5);
		const Span LifeOf = Range(Sk + "life_s", 2.5, 3.5), AlphaOf = Range(Sk + "alpha", 0.45, 0.6);
		const double StartR = Num(Sk + "start_radius_m", 0.5), EndR = Num(Sk + "end_radius_m", 1.5), InnerK = Num(Sk + "inner_radius_k", 1.0);
		const double GrowOf = Num(Sk + "grow", 0.25), StretchOf = Num(Sk + "stretch_along_ring", 1.7);
		const double FlattenOf = Num(Sk + "flatten", 0.75), Billow = Num(Sk + "front_billow", 1.35);
		const double HazeBy = Num(Sk + "haze_by_s", 1.0), ShadeOf = Num(Sk + "shade", 1.8), HeartOf = Num(Sk + "heart", 0.55);
		const double DustAlpha = std::pow(DustK, 0.4);
		for (int I = 0; I < NOut + NIn; ++I)
		{
			const bool bFront = I < NOut;
			const int N = bFront ? NOut : NIn;
			const int Place = bFront ? I : I - NOut;
			const double Speed = bFront ? 1.0 : std::sqrt(D.U(0.02, 0.85)); // (the inner ones fill back toward the hole, evenly over its round: one skirt, not a ring)
			const double A = 2.0 * Pi * (Place + D.U(-0.4, 0.4)) / std::max(N, 1);
			const Vec Outward(std::cos(A), std::sin(A), 0.0);
			const double Drag = DragOf * D.U(0.85, 1.15);
			Bit B;
			B.Form = Bit::Shape::Upright;
			B.bHug = true;
			B.Born = T0 + D.U(0.0, 0.02);
			B.From = At + Outward * (D.U(0.4, 1.0) * K);
			B.Vel = Outward * (Reach * Speed * Drag * D.U(0.85, 1.0));
			B.Drag = Drag;
			B.Rise = RiseOf * D.U(0.6, 1.4);
			B.Windage = 0.6;
			B.Life = LifeOf.Pick(D) * std::min(2.0, K);
			B.R0 = StartR * K;
			B.R1 = EndR * K * D.U(0.7, 1.3) * (bFront ? 1.0 : InnerK); // (fewer, broader inner ones fill the round)
			B.Grow = GrowOf;
			B.Stretch = StretchOf;
			B.Along = Vec(-std::sin(A), std::cos(A), 0.0);
			B.Flatten = FlattenOf * D.U(0.8, 1.2) * (bFront ? Billow : 1.0); // (the front edge stands tallest)
			B.Col = Col * D.U(0.9, 1.08);
			B.Col2 = B.Col;
			B.Alpha = std::min(0.9, AlphaOf.Pick(D) * DustAlpha);
			B.FadeIn = 0.02;
			B.Hold = 0.1;
			B.HazeBy = HazeBy;
			B.HazeAlpha = std::min(B.Alpha, HazeAlpha);
			B.ShadeK = ShadeOf; // (a lit top and a shadowed foot, so it shows its height)
			B.Heart = HeartOf;
			B.Rim = 0.0;
			Bits.push_back(B);
		}
	}

	void Battle::Crown(double T0, const Vec& At, double CrownH, double K, double Slow, const Paint& P, double ClodK, const std::string& Size, Dice& D)
	{
		// The crown: a wide torn fan. A few heavy dark jets, very uneven in length and width, bunched and overlapping;
		// lighter dust filling the gaps so that the outline is one torn fan; each heavy jet thinnest and torn at its
		// tip, drawn to a point, with clods flying on beyond it. Then a dense dark body at the foot.
		const std::string C = "burst.crown.";
		// (a size may have its own crown numbers under sizes.<size>.crown.: the bombs' fan of many jets)
		const std::string Own = "sizes." + Size + ".crown.";
		auto Path = [&](const std::string& Name) -> std::string
		{
			return Has(Own + Name) || Has(Own + Name + ".0") ? Own + Name : C + Name;
		};
		const double Half = Num(C + "fan_deg", 100.0) * 0.5;
		const double KD = Num(C + "drag", 6.5);
		const double Over = Num(C + "overlap", 0.9);
		const double TipAt = Num(C + "tip_breakup", 0.6);
		const double TipStretch = Num(C + "tip_stretch", 2.6);
		const double TipPoint = Num(C + "tip_point", 0.9);
		const double TipAlpha = Num(C + "tip_alpha", 0.55);
		const double Along0 = Num(C + "stretch_along_jet", 1.6);
		const double FootR = Num(C + "jet_foot_r", 0.05) * CrownH;
		const double TipR = Num(C + "jet_tip_r", 0.012) * CrownH;
		const double Lumps = Num(C + "lump_chance", 0.2);
		const double Jitter = Num(C + "edge_jitter", 0.4);
		const double EdgeRough = Num(C + "edge_rough", 0.3);
		const double RootSolid = Num(C + "root_solid", 0.6); // (how far up a heavy jet stays dense)
		const double ClumpChance = Num(C + "side_clumps", 0.7); // (the chance of a clump on the side at each step of a jet's lower part)
		const double TipGrain = Num(C + "tip_grain", 0.55); // (a heavy jet's tip torn into smaller pieces, scattered: never one long translucent blade)
		const double TipRough = Num(C + "tip_rough", 0.6);
		const double HeavyHeart = Num(C + "heavy_heart", 0.92); // (how solid a heavy piece is at its heart ring)
		const double HeavyInner = Num(C + "heavy_inner", 0.72); // (and where that ring sits: the edge band is the rest)
		const double TipInner = Num(C + "tip_inner", 0.6);
		const double PieceMax = Num(Path("piece_max_m"), 0.0); // (0: no limit, as for the shells)
		const double FillRough = Num(C + "fill_rough", 0.55); // (the dust between the jets: soft, billowing, scattered; never a long clear prism)
		const double FillScatter = Num(C + "fill_scatter", 1.6);
		const double FillStretch = Num(C + "fill_stretch", 1.15);
		const Vec Tilt = Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), 0.0).Unit(Vec(1.0, 0.0, 0.0)) * std::tan(Radians(Pick(D, C + "tilt_deg", 4.0, 14.0)));
		const int NHeavy = D.Whole(static_cast<int>(Num(Path("heavy_jets") + ".0", 5.0)), static_cast<int>(Num(Path("heavy_jets") + ".1", 8.0)));
		// (ranked heights, where a size asks for them: the second tallest jet clearly shorter than the tallest, then a
		// few more at half its height or over, so the fan never reads as one or two stalks)
		const bool bRanked = Has(Path("second_reach") + ".0");
		const int NMid = std::clamp(static_cast<int>(Num(Path("mid_jets"), 0.0)), 0, std::max(NHeavy, 0));
		// (the heavy jets spaced round the compass, each nudged by up to this share of a gap: every side shows a fan)
		const double SpreadEven = Clamp01(Num(Path("spread_even"), 0.0));
		const double AzFrom = SpreadEven > 0.0 ? D.U(0.0, 2.0 * Pi) : 0.0;
		const int NFill = D.Whole(static_cast<int>(Num(C + "fill_jets.0", 10.0)), static_cast<int>(Num(C + "fill_jets.1", 16.0)));
		std::vector<Vec> Launch;
		std::vector<double> LaunchR;
		for (int J = 0; J < NHeavy + NFill; ++J)
		{
			const bool bHeavy = J < NHeavy;
			const double Az = bHeavy && SpreadEven > 0.0 ? AzFrom + 2.0 * Pi * (J + SpreadEven * D.U(-0.5, 0.5)) / std::max(NHeavy, 1)
			                                             : D.U(0.0, 2.0 * Pi); // (otherwise at random round the compass: bunched and overlapping, never spaced)
			const bool bShort = bRanked && J > 1 + NMid; // (a ranked crown's short jets, which may lean further out to widen its fan)
			const double Lean = bHeavy ? (J == 0 ? D.U(2.0, 12.0) : Pick(D, Path(bShort && Has(Path("short_lean") + ".0") ? "short_lean" : "heavy_lean"), 0.35, 1.0) * Half) : D.U(0.0, 1.0) * Half;
			double ReachF = 1.0;
			if (!bHeavy)
			{
				ReachF = Pick(D, C + "fill_reach", 0.3, 0.75);
			}
			else if (J > 0)
			{
				ReachF = bRanked && J == 1 ? Pick(D, Path("second_reach"), 0.6, 0.75)
				       : bRanked && !bShort ? Pick(D, Path("mid_reach"), 0.5, 0.7)
				       : Pick(D, Path("heavy_reach"), 0.3, 1.0);
			}
			const Vec Dir = (Vec(std::cos(Az) * std::sin(Radians(Lean)), std::sin(Az) * std::sin(Radians(Lean)), std::cos(Radians(Lean))) + Tilt).Unit();
			const double ReachJ = CrownH * ReachF;
			const double V = ReachJ * KD * 1.12 / std::sqrt(Slow);
			const double Thick = bHeavy ? Pick(D, Path("heavy_thick"), 0.7, 2.6) : Pick(D, C + "fill_thick", 1.6, 3.0);
			const double Rf = FootR * Thick, Rt = TipR * Thick;
			const double Shade = D.U(0.85, 1.15);
			const double FillAlpha = Pick(D, C + "fill_alpha", 0.3, 0.5);
			const double DustShare = D.U(0.35, 0.7);
			// (puffs stepped along the jet by their own length, overlapping, so that a jet of any size is one streak)
			double F = D.U(0.0, 0.05);
			while (F < 1.0)
			{
				const double Rwhole = Rt + (Rf - Rt) * std::pow(1.0 - F, 0.8);
				const bool bTip = bHeavy && F >= TipAt;
				const double OnTip = bTip ? (F - TipAt) / std::max(1.0 - TipAt, 1e-6) : 0.0;
				const bool bGrained = P.Grain < 0.999;
				double Rl = Rwhole * (bGrained ? P.Grain : 1.0 + (std::clamp(TipGrain, 0.2, 1.0) - 1.0) * OnTip); // (granular grounds and torn tips: smaller pieces, more of them)
				// (a piece's outline has the ring's few points however big it is, so a bomb's jet is built of more pieces
				// no bigger than piece_max_m, scattered across the jet's width, and never one big flat-faced piece)
				double Split = 1.0;
				if (PieceMax > 0.0 && Rl > PieceMax)
				{
					Split = Rl / PieceMax;
					Rl = PieceMax;
				}
				const double Sp = V * (0.05 + 0.95 * std::pow(F, 0.9));
				const double Rr = Rl * (D.U() < Lumps ? D.U(1.4, 2.0) : D.U(0.8, 1.1));
				Rgb Col = bHeavy ? P.Heavy * Shade : P.Soil.Mix(P.Dust, DustShare) * Shade;
				if (P.bFirst && D.U() < P.FirstAtFoot + (P.FirstAtTip - P.FirstAtFoot) * F)
				{
					Col = P.First * D.U(0.84, 1.04); // (snow: thrown up first, never two grains the same shade, earth showing through)
				}
				const Vec Side = Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), D.U(-1.0, 1.0)) * (Rwhole * (bTip ? 1.1 : Jitter) * KD * (bGrained ? 1.5 : 1.0) * (bHeavy ? 1.0 : FillScatter) * std::min(2.0, std::sqrt(Split))); // (ragged, clumpy sides; grains, dust and split pieces scattered wider, never a string of beads)
				Bit B;
				B.Born = T0 + 0.01 + 0.025 * (1.0 - F);
				B.From = At + ZUp * 0.1 + Dir * (0.25 * K);
				B.Vel = Dir * Sp + Side;
				B.Drag = KD;
				B.Gravity = 9.8 / Slow;
				B.Windage = 0.35;
				B.Life = D.U(1.2, 2.0) * Slow;
				B.R0 = Rr;
				B.R1 = Rr * D.U(1.8, 2.4);
				B.Grow = 0.5;
				B.Along = Dir;
				B.Stretch = 1.0 + ((bTip ? Along0 + (TipStretch - Along0) * OnTip : (bHeavy ? Along0 : FillStretch)) - 1.0) * P.Grain; // (grains are not drawn out)
				B.Point = bTip ? TipPoint : (bHeavy ? 0.3 : 0.0); // (torn and pointed, never rounded; the dust between has no points)
				B.Tail = 0.15;
				B.Col = Col;
				B.Col2 = P.bFirst ? Col : (bHeavy && F < RootSolid ? P.Heavy.Mix(P.Dust, 0.35) : P.Dust); // (the root stays dark earth)
				B.Cool = 0.7;
				B.Alpha = (bHeavy ? (bTip ? 1.0 - (1.0 - TipAlpha) * OnTip : 1.0) : FillAlpha * (1.0 - 0.3 * F)) * (bGrained ? D.U(0.7, 1.0) : 1.0);
				B.FadeIn = 0.01;
				B.Hold = 0.4;
				// (heavy at the root, thinning only near the tip: the lower half of a heavy jet dense, with hard lumpy
				// edges as the clods have, never a translucent blade)
				const double Root = bHeavy && !bGrained ? Clamp01((RootSolid - F) / std::max(RootSolid, 1e-3)) : 0.0;
				// (a heavy piece is solid out to most of its radius and then fades in a thin soft edge; a wide see-through
				// band with a hard rim, on a big piece, is what read as glass)
				B.Heart = bHeavy ? (bGrained ? 0.65 : HeavyHeart + (1.0 - HeavyHeart) * 0.5 * Root) : 0.5;
				B.Inner = bHeavy && !bGrained ? HeavyInner + (TipInner - HeavyInner) * OnTip : 0.5;
				B.Rim = 0.0; // (soft edges everywhere: no jet has a clean blade's side)
				B.Rough = bHeavy ? EdgeRough + (TipRough - EdgeRough) * OnTip : FillRough; // (lumpy outlines, most torn at the tip)
				B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
				Bits.push_back(B);
				if (Root > 0.0 && D.U() < ClumpChance)
				{
					// a dense clump of earth bulging out of the jet's side, so its lower half has a broken, lumpy
					// edge, as the clods do, and never a straight one
					const Vec Across_ = Dir.Cross(Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), D.U(-1.0, 1.0))).Unit(Vec(1.0, 0.0, 0.0));
					Bit Lump = B;
					Lump.From = B.From;
					Lump.Vel = B.Vel + Across_ * (Rl * D.U(0.8, 1.3) * KD);
					Lump.R0 = Rl * D.U(0.35, 0.6);
					Lump.R1 = Lump.R0 * D.U(1.4, 1.9);
					Lump.Stretch = 1.0;
					Lump.Point = D.U(0.0, 0.3);
					Lump.Tail = 0.0;
					Lump.Col = P.Heavy * (Shade * D.U(0.8, 1.05));
					Lump.Heart = 0.95;
					Lump.Rim = 0.55;
					Lump.Rough = 0.4;
					Lump.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
					Lump.ShadeK = 2.0;
					Bits.push_back(Lump);
				}
				// (stepped by the length it is drawn; never less than a hundredth, so no number in the recipe can stall it)
				F += std::max(0.01, Rl * std::max(1.0, 1.0 + (Along0 - 1.0) * P.Grain) * Over / std::max(ReachJ, 1e-3) * D.U(0.7, 1.2));
			}
			if (bHeavy && ClodK > 0.05)
			{
				const int N = D.Whole(static_cast<int>(Num(C + "tip_clods.0", 3.0)), static_cast<int>(Num(C + "tip_clods.1", 5.0)));
				for (int I = 0; I < N; ++I)
				{
					const Vec Way = (Dir + Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), D.U(-1.0, 1.0)) * 0.08).Unit();
					Launch.push_back(Way * (std::sqrt(2.0 * 9.8 * ReachJ) * Pick(D, C + "tip_clod_speed", 1.1, 1.35)));
					LaunchR.push_back(Pick(D, C + "tip_clod_m", 0.1, 0.22) * K);
				}
			}
		}
		// the body: a dense dark mound of earth and dust at the crown's foot from about frame 4, slumping into the smoke
		const int NBody = static_cast<int>(Num(Path("body.puffs"), 50.0) * std::max(0.7, std::sqrt(K)));
		const double BodyWide = Num(Path("body.width"), 0.3), BodyHigh = Num(Path("body.height"), 0.22);
		const Span BodyR = Range(Path("body.puff_r"), 0.06, 0.11), BodyAlpha = Range(Path("body.alpha"), 0.85, 0.95);
		const double BodyFrom = Num(Path("body.from_frame"), 3.0) / 30.0;
		const double BodyRim = Num(Path("body.rim"), 0.1), BodyRough = Num(Path("body.rough"), 0.0);
		for (int I = 0; I < NBody; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			const double Out_ = CrownH * BodyWide * std::pow(D.U(), 0.6);
			const double Up_ = CrownH * BodyHigh * std::pow(D.U(), 1.3);
			const Vec To = Vec(std::cos(A) * Out_, std::sin(A) * Out_, Up_) + Tilt * Up_;
			const double Rr = CrownH * BodyR.Pick(D);
			const double KB = 5.0;
			Bit B;
			B.Born = T0 + D.U(0.03, BodyFrom);
			B.From = At + ZUp * (0.15 * K);
			B.Vel = To * KB;
			B.Drag = KB;
			B.Gravity = 2.5 / Slow;
			B.Windage = 0.4;
			B.Life = D.U(1.4, 2.4) * Slow;
			B.R0 = Rr * 0.5;
			B.R1 = Rr;
			B.Grow = 0.35;
			B.Col = P.Heavy.Mix(P.Dust, D.U(0.0, 0.35)) * D.U(0.85, 1.1);
			B.Col2 = P.Smoke;
			B.Cool = 1.2;
			B.Alpha = BodyAlpha.Pick(D);
			B.FadeIn = 0.03;
			B.Hold = 0.45;
			B.Heart = 0.75;
			B.Rim = BodyRim;
			B.Rough = BodyRough; // (a bomb's big body pieces lumpy, never flat-faced)
			if (B.Rough > 0.0)
			{
				B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
			}
			Bits.push_back(B);
		}
		// the clods: those at the jets' tips, and more thrown clear low and wide
		const int NMore = static_cast<int>(ClodK * 50.0 * std::max(0.5, K * K));
		const double ClodLo = Num("burst.debris.clod_r_m.0", 0.015), ClodHi = Num("burst.debris.clod_r_m.1", 0.09);
		for (int I = 0; I < NMore; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			const double Out_ = D.U(1.0, 6.0) * K;
			const double Up_ = (2.0 + 9.0 * std::pow(D.U(), 2.0)) * K;
			Launch.push_back(Vec(std::cos(A) * Out_, std::sin(A) * Out_, Up_));
			const double Lo = ClodLo, Hi = ClodHi;
			LaunchR.push_back((Lo + (Hi - Lo) * std::pow(D.U(), 2.2)) * K);
		}
		// (a close hit throws a few lumps past the lens: what puts the player in the hole)
		if (bEyeKnown && ClodK > 0.05)
		{
			const double Near = Num("burst.debris.lens.within_m", 40.0) * std::max(1.0, K);
			if ((LastEye - At).Length() < Near)
			{
				const int N = D.Whole(static_cast<int>(Num("burst.debris.lens.pieces.0", 3.0)), static_cast<int>(Num("burst.debris.lens.pieces.1", 7.0)));
				for (int I = 0; I < N; ++I)
				{
					const double T = D.U(0.7, 1.4);
					const Vec Miss = Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), D.U(-0.6, 0.4)).Unit() * D.U(1.2, 3.0);
					const Vec Aim = LastEye + Miss - At;
					Launch.push_back((Aim + Vec(0.0, 0.0, 4.9 * T * T)) * (1.0 / T));
					LaunchR.push_back(D.U(0.05, 0.12));
				}
			}
		}
		for (size_t I = 0; I < Launch.size(); ++I)
		{
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.bThrown = true;
			B.Born = T0 + 0.015;
			B.From = At + ZUp * (0.15 * K);
			B.Vel = Launch[I];
			B.Gravity = 9.8;
			B.R0 = B.R1 = LaunchR[I];
			B.Col = (P.bSplinter && D.U() < 0.35 ? P.Splinter : P.Clod) * D.U(0.8, 1.2);
			B.Col2 = B.Col;
			B.Alpha = 1.0;
			B.FadeIn = 0.005;
			B.Heart = 0.95;
			B.Rim = 0.6;
			B.LeastPx = ClodLeast;
			Chunk(B, D);
			Throw(B);
			B.Life = B.Land2 + D.U(2.0, 4.0);
			B.Hold = std::min(0.95, (B.Land2 + 1.0) / B.Life);
			const Vec Landed = B.At1;
			const double Down = B.Born + B.Land;
			const double R = B.R0;
			Bits.push_back(B);
			if (R > 0.04)
			{
				Bit Puff;
				Puff.Born = Down;
				Puff.From = Landed;
				Puff.Vel = Vec(0.0, 0.0, 0.6);
				Puff.Drag = 2.0;
				Puff.Windage = 0.5;
				Puff.Life = D.U(0.8, 1.4);
				Puff.R0 = R * 2.0;
				Puff.R1 = R * 6.0;
				Puff.Col = P.Dust;
				Puff.Col2 = P.Dust;
				Puff.Alpha = 0.45;
				Puff.FadeIn = 0.03;
				Puff.Hold = 0.1;
				Bits.push_back(Puff);
			}
		}
	}

	void Battle::Smoke(double T0, const Vec& At, double CrownH, double K, double Scale, const std::string& Size, const std::string& Ground, const Paint& P, double DustK, Dice& D)
	{
		// The veil: fine earth that stands a moment over the crown's foot and falls back.
		const double Slow = Num("grounds." + Ground + ".crown_slow", 1.0);
		const int NVeil = static_cast<int>(30.0 * std::max(0.7, K));
		for (int I = 0; I < NVeil; ++I)
		{
			Bit B;
			B.Born = T0 + D.U(0.06, 0.14);
			B.From = At + ZUp * 0.2 + Vec(D.U(-0.4, 0.4), D.U(-0.4, 0.4), 0.0) * K;
			B.Vel = Vec(D.U(-1.0, 1.0) * 1.6, D.U(-1.0, 1.0) * 1.6, D.U(4.0, 8.5)) * (K / std::sqrt(Slow));
			B.Life = D.U(1.4, 2.4) * Slow;
			B.R0 = 0.25 * K;
			B.R1 = D.U(0.7, 1.2) * K;
			B.Col = P.Dust;
			B.Col2 = P.Dust;
			B.Alpha = std::min(0.6, D.U(0.25, 0.4) * std::sqrt(DustK));
			B.Gravity = 6.0 / Slow;
			B.Drag = 1.2;
			B.FadeIn = 0.05;
			B.Hold = 0.3;
			B.Windage = 0.5;
			B.Heart = 0.45;
			Bits.push_back(B);
		}
		// The smoke: the crown collapses into a rolling charcoal-brown cloud that climbs, leans downwind and stays.
		// Many puffs of varied size, overlapping and merging, soft-edged: never separate round balls.
		const double Linger = Pick(D, "sizes." + Size + ".linger_s", 15.0, 25.0) * Num("grounds." + Ground + ".haze_lingers", 1.0);
		const int N = static_cast<int>(130.0 * std::pow(std::max(0.4, Scale), 0.85));
		const Rgb Warm = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Vec Lean = Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), 0.0) * 0.15;
		const std::string Sm = "burst.smoke.";
		const Rgb Top = Picture(Sm + "colour_top", Rgb::FromPicture(66, 64, 62));
		const double TopShare = Num(Sm + "top_share", 0.8);
		// (the top breaks into a few lumps, never one smooth dome)
		const int NLobes = D.Whole(static_cast<int>(Num(Sm + "top_lobes.0", 3.0)), static_cast<int>(Num(Sm + "top_lobes.1", 5.0)));
		std::vector<Vec> Lobes;
		for (int L = 0; L < NLobes; ++L)
		{
			const double La = 2.0 * Pi * (L + D.U(-0.3, 0.3)) / std::max(NLobes, 1);
			Lobes.push_back(Vec(std::cos(La), std::sin(La), D.U(-0.15, 0.2)));
		}
		const double CloudShade = Num(Sm + "cloud_shade", 1.0);
		const double CloudR = Num(Sm + "cloud_r", 0.3);
		const Span SmokeR0 = Range(Sm + "puff_r.0", 0.5, 0.9), SmokeR1 = Range(Sm + "puff_r.1", 1.2, 2.6), SmokeAlpha = Range(Sm + "alpha", 0.35, 0.6);
		const double SmokeGrow = Num(Sm + "grow", 0.3);
		for (int I = 0; I < N; ++I)
		{
			const double F = std::pow(D.U(), 0.9); // (0 at the foot, 1 at the top of the crown)
			const double Born = T0 + 0.1 + 0.5 * D.U() * (0.4 + F);
			const double Hgt = CrownH * (0.05 + 0.8 * F);
			const double Spread = (0.5 + 2.2 * F) * K;
			const double Var = D.U(0.6, 1.6);
			const double C = D.U(0.8, 1.2);
			// (warm brown near the ground, charcoal grey toward the top: brown all the way up reads as dust)
			const Rgb Base = (D.U() > 0.25 ? P.Smoke * C : P.Dust * (0.6 * C)).Mix(Top * C, std::min(1.0, F * 1.1) * TopShare);
			Bit B;
			B.Born = Born;
			B.From = At + Lean * Hgt + Vec(D.Bell() * Spread * 0.6, D.Bell() * Spread * 0.6, Hgt * 0.45);
			B.Vel = Vec(D.Bell(), D.Bell(), 0.0) * (1.2 * K) + ZUp * (Hgt * D.U(0.6, 1.5));
			if (F > 0.55 && !Lobes.empty())
			{
				const Vec& Lobe = Lobes[static_cast<size_t>(D.Whole(0, static_cast<int>(Lobes.size()) - 1))];
				const Vec Off = Vec(Lobe.X, Lobe.Y, 0.0) * (Spread * 0.5) + Vec(D.Bell(), D.Bell(), 0.0) * (Spread * 0.22);
				B.From = At + Lean * Hgt + Off + ZUp * (Hgt * (0.45 + Lobe.Z));
				B.Vel = Vec(Lobe.X, Lobe.Y, 0.0) * (0.8 * K) + Vec(D.Bell(), D.Bell(), 0.0) * (0.4 * K) + ZUp * (Hgt * D.U(0.7, 1.4) * (1.0 + Lobe.Z));
			}
			B.Life = Linger * D.U(0.45, 1.0);
			B.R0 = SmokeR0.Pick(D) * K * Var;
			B.R1 = SmokeR1.Pick(D) * K * (1.0 + 0.5 * F) * Var;
			B.Col = (F < 0.25 && Born - T0 < 0.22) ? Base.Mix(Warm, 0.35) : Base; // (warm only at its foot, at first)
			B.Col2 = Base;
			B.Cool = B.Life / 0.15;
			B.Alpha = SmokeAlpha.Pick(D);
			B.Drag = D.U(1.4, 2.2);
			B.Rise = D.U(0.25, 0.9) * std::sqrt(K);
			B.FadeIn = 0.08;
			B.Grow = SmokeGrow;
			B.Windage = D.U(0.5, 1.0);
			B.Hold = 0.15;
			B.Heart = D.U(0.4, 0.7); // (some firmer than others: a torn cloud, not one soft egg)
			B.Rim = 0.0;
			B.CloudShade = CloudShade;
			B.Centre = At + (Lean + ZUp) * (CrownH * 0.45); // (the cloud's middle leans as the cloud does)
			B.CloudR = CrownH * CloudR; // (within this of the middle a puff is only partly lit or shadowed)
			B.CentreRise = 0.55 * std::sqrt(K);
			Bits.push_back(B);
		}
		// the crater smoulders faintly for a while
		Source S;
		S.At = At;
		S.Kind = 1;
		S.Start = T0 + 1.0;
		S.Next = S.Start;
		S.NextSmoke = S.Start;
		S.End = S.Start + Num("burst.linger.crater_smoulders_s", 8.0);
		S.Across = std::max(0.3, Num("sizes." + Size + ".crater_m", 1.0));
		S.Seed = Seeds * 31ull + 7ull;
		Sources.push_back(S);
	}

	void Battle::Scar(double T0, const Vec& At, double Across, const Paint& P, bool bSnow, Dice& D)
	{
		// The crater: a dark scorched hole, a ring of thrown earth round it out to almost twice its size, and spatter.
		// It stays for the whole battle; craters overlap where hits overlap.
		const double R = Across * 0.5;
		const double Ring_ = Num("burst.scar.ring_of_thrown_earth", 1.8);
		auto Mark = [&](const Vec& Where_, double Radius, double Stretch, double Spin, const Rgb& Col, double Alpha, double Heart, double Point)
		{
			Bit B;
			B.Form = Bit::Shape::Flat;
			B.Born = T0;
			B.From = Vec(Where_.X, Where_.Y, Height(Where_.X, Where_.Y) + 0.04);
			B.Life = 1e9;
			B.R0 = B.R1 = Radius;
			B.Stretch = Stretch;
			B.Spin = Spin;
			B.Col = Col;
			B.Col2 = Col;
			B.Alpha = Alpha;
			B.FadeIn = 0.3;
			B.Hold = 1.0;
			B.Heart = Heart;
			B.Rim = 0.0;
			B.Point = Point;
			Marks.push_back(B);
		};
		const Rgb Hole = bSnow ? Rgb::FromPicture(30, 28, 26) : P.Heavy * 0.6;
		const Rgb Thrown = bSnow ? P.Soil.Mix(P.Dust, 0.35) : P.Heavy;
		const int NRing = D.Whole(6, 9);
		for (int I = 0; I < NRing; ++I)
		{
			const double A = 2.0 * Pi * (I + D.U(-0.3, 0.3)) / NRing;
			const double Dist = D.U(0.8, 0.5 + 0.55 * Ring_) * R;
			Mark(At + Vec(std::cos(A), std::sin(A), 0.0) * Dist, D.U(0.35, 0.6) * R, D.U(1.2, 1.8), A + Pi / 2.0, Thrown * D.U(0.8, 1.1), D.U(0.55, 0.8), 0.6, 0.3);
		}
		Mark(At, R * 1.05, D.U(0.9, 1.15), D.U(0.0, 2.0 * Pi), Hole, 0.92, 0.9, 0.15);
		const int NSpat = D.Whole(5, 8);
		for (int I = 0; I < NSpat; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			Mark(At + Vec(std::cos(A), std::sin(A), 0.0) * (D.U(1.5, 2.2) * R), D.U(0.08, 0.18) * R, D.U(1.0, 2.0), A, Thrown, 0.7, 0.7, 0.4);
		}
		const size_t MostMarks = 2400;
		if (Marks.size() > MostMarks)
		{
			Marks.erase(Marks.begin(), Marks.begin() + static_cast<std::ptrdiff_t>(Marks.size() - MostMarks));
		}
	}

	void Battle::Armour(double T0, const Vec& At, double Scale, Dice& D)
	{
		// A shell that strikes a tank and does not go in: sparks, a small grey puff gone in 2 s, a short flash. No earth.
		const Rgb Spark = Picture("on_armour.spark_colour", Rgb::FromPicture(255, 214, 140));
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const double K = std::sqrt(Scale);
		const int N = D.Whole(static_cast<int>(Num("on_armour.sparks.0", 12.0)), static_cast<int>(Num("on_armour.sparks.1", 24.0)));
		for (int I = 0; I < N; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			const double E = D.U(-0.2, 1.2);
			const Vec Dir(std::cos(A) * std::cos(E), std::sin(A) * std::cos(E), std::sin(E));
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.Born = T0;
			B.From = At;
			B.Vel = Dir * D.U(8.0, 20.0);
			B.Gravity = 9.8;
			B.Drag = 1.5;
			B.Life = Pick(D, "game.spark_life_s", 0.12, 0.3);
			B.R0 = B.R1 = 0.025;
			B.Col = Spark;
			B.Col2 = Flame * 0.6;
			B.Alpha = 1.0;
			B.FadeIn = 0.002;
			B.Hold = 0.4;
			B.Heart = 0.9;
			B.Rim = 0.3;
			B.LeastPx = 1.5;
			B.bLight = true;
			Bits.push_back(B);
		}
		for (int I = 0; I < 4; ++I)
		{
			Bit B;
			B.Born = T0 + 0.02;
			B.From = At + Vec(D.U(-0.3, 0.3), D.U(-0.3, 0.3), D.U(0.0, 0.4)) * K;
			B.Vel = Vec(D.Bell(), D.Bell(), D.U(0.5, 1.5)) * 0.8;
			B.Drag = 2.0;
			B.Rise = 0.6;
			B.Windage = 0.7;
			B.Life = D.U(1.6, 2.0);
			B.R0 = 0.2 * K;
			B.R1 = 0.9 * K;
			B.Col = Rgb::FromPicture(122, 120, 116);
			B.Col2 = B.Col;
			B.Alpha = 0.5;
			B.FadeIn = 0.04;
			B.Hold = 0.1;
			Bits.push_back(B);
		}
		Flash F;
		F.At = At;
		F.Col = Spark;
		F.Born = T0;
		F.Life = 0.06;
		F.Strength = 0.35 * Scale;
		F.Radius = 15.0;
		Flashes.push_back(F);
	}

	void Battle::Skip(double Now, const Vec& AtIn, const Vec& Heading, const char* GroundIn)
	{
		// An armour-piercing round that misses: a single low spurt of earth and a skip; no crown, no crater.
		std::string Ground = GroundIn ? GroundIn : "earth";
		if (!Has("grounds." + Ground + ".paint.soil.0"))
		{
			Ground = "earth";
		}
		const Paint P = PaintOf(Ground);
		Dice D(++Seeds * 104729ull ^ PlaceKey(AtIn));
		const Vec At(AtIn.X, AtIn.Y, Height(AtIn.X, AtIn.Y));
		const Vec Fw = Vec(Heading.X, Heading.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		const Vec Side = ZUp.Cross(Fw);
		const double Reach = Pick(D, "ap_miss.spurt_m", 1.2, 2.0);
		const int N = D.Whole(static_cast<int>(Num("ap_miss.puffs.0", 5.0)), static_cast<int>(Num("ap_miss.puffs.1", 8.0)));
		for (int I = 0; I < N; ++I)
		{
			const Vec Dir = (Fw + ZUp * D.U(0.2, 0.45) + Side * D.U(-0.25, 0.25)).Unit();
			const double KD = 5.0;
			Bit B;
			B.Born = Now + D.U(0.0, 0.02);
			B.From = At + ZUp * 0.05;
			B.Vel = Dir * (Reach * KD * D.U(0.6, 1.1));
			B.Drag = KD;
			B.Gravity = 4.0;
			B.Windage = 0.4;
			B.Life = D.U(1.0, 1.6);
			B.R0 = 0.12;
			B.R1 = D.U(0.35, 0.55);
			B.Along = Dir;
			B.Stretch = 1.8;
			B.Point = 0.6;
			B.Col = (I % 2 ? P.Heavy : P.Soil.Mix(P.Dust, 0.4)) * D.U(0.9, 1.1);
			B.Col2 = P.Dust;
			B.Cool = 0.8;
			B.Alpha = 0.85;
			B.FadeIn = 0.01;
			B.Hold = 0.25;
			B.Heart = 0.7;
			B.Rim = 0.1;
			Bits.push_back(B);
		}
		for (int I = 0; I < 5; ++I)
		{
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.bThrown = true;
			B.Born = Now + 0.01;
			B.From = At + ZUp * 0.05;
			B.Vel = (Fw * D.U(2.0, 5.0) + Side * D.U(-1.0, 1.0) + ZUp * D.U(1.5, 3.5));
			B.Gravity = 9.8;
			B.R0 = B.R1 = D.U(0.02, 0.05);
			B.Col = P.Clod;
			B.Col2 = P.Clod;
			B.Heart = 0.95;
			B.Rim = 0.6;
			B.LeastPx = ClodLeast;
			Chunk(B, D);
			Throw(B);
			B.Life = B.Land2 + 2.0;
			B.Hold = std::min(0.95, (B.Land2 + 0.5) / B.Life);
			Bits.push_back(B);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Guns

	void Battle::Muzzle(double Now, const Vec& At, const Vec& Facing, const char* GunIn)
	{
		std::string M = std::string("muzzle.") + (GunIn ? GunIn : "rifle") + ".";
		if (!Has(M + "flame_m"))
		{
			M = "muzzle.rifle.";
		}
		Dice D(++Seeds * 15485863ull ^ PlaceKey(At));
		const double Len = Num(M + "flame_m", 0.3);
		const Vec Fw = Facing.Unit(Vec(1.0, 0.0, 0.0));
		const Vec Across = Fw.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Rgb Core = Picture("burst.flash.core_colour", Rgb::FromPicture(236, 168, 72));
		// the flame: a ragged tongue along the bore with licks thrown sideways, under one frame
		{
			Bit B;
			B.Born = Now;
			B.From = At + Fw * (Len * 0.5);
			B.Vel = Fw * 2.0;
			B.Drag = 6.0;
			B.Life = D.U(0.03, 0.045);
			B.R0 = Len * 0.22;
			B.R1 = Len * 0.3;
			B.Along = Fw;
			B.Stretch = 2.3;
			B.Point = 0.9;
			B.Tail = 0.1;
			B.Col = Core.Mix(Flame, 0.35);
			B.Col2 = Flame;
			B.Alpha = 1.0;
			B.FadeIn = 0.003;
			B.Hold = 0.5;
			B.Heart = 0.85;
			B.Rim = 0.2;
			B.bLight = true;
			Bits.push_back(B);
		}
		const int NLicks = D.Whole(2, 4);
		for (int I = 0; I < NLicks; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			const Vec Dir = (Fw * 0.5 + (Across * std::cos(A) + ZUp * std::sin(A)) * D.U(0.6, 1.0)).Unit();
			Bit B;
			B.Born = Now;
			B.From = At + Fw * (Len * 0.3) + Dir * (Len * 0.25);
			B.Vel = Dir * 1.5;
			B.Drag = 6.0;
			B.Life = D.U(0.025, 0.04);
			B.R0 = Len * 0.1;
			B.R1 = Len * 0.14;
			B.Along = Dir;
			B.Stretch = 2.0;
			B.Point = 0.8;
			B.Col = Flame;
			B.Col2 = Flame * 0.5;
			B.Alpha = 0.95;
			B.FadeIn = 0.003;
			B.Hold = 0.4;
			B.Heart = 0.8;
			B.Rim = 0.1;
			B.bLight = true;
			Bits.push_back(B);
		}
		if (Has(M + "light_w"))
		{
			Flash F;
			F.At = At + Fw * (Len * 0.5);
			F.Col = Picture("burst.flash.light.colour", Rgb::FromPicture(255, 196, 140));
			F.Born = Now;
			F.Life = 0.03;
			F.Strength = Num(M + "light_w", 0.0) / 8500.0;
			F.Radius = 12.0 * std::max(1.0, Len);
			F.bNightOnly = Num(M + "day_lamp", 0.0) <= 0.0; // (by day only the big guns light the ground)
			Flashes.push_back(F);
		}
		// then grey-brown smoke rolls out, hangs and drifts
		const double SmokeLo = Num(M + "smoke_s.0", Num(M + "smoke_s", 2.0));
		const double SmokeHi = Num(M + "smoke_s.1", SmokeLo);
		const Rgb Grey = Picture("game.muzzle_smoke", Rgb::FromPicture(150, 140, 126));
		const int NSmoke = Len < 0.5 ? 3 : static_cast<int>(10.0 + 6.0 * Len);
		for (int I = 0; I < NSmoke; ++I)
		{
			Bit B;
			B.Born = Now + D.U(0.0, 0.05);
			B.From = At + Fw * (Len * D.U(0.3, 1.2));
			B.Vel = Fw * (D.U(3.0, 9.0) * Len) + Vec(D.Bell(), D.Bell(), D.Bell()) * 0.4;
			B.Drag = 2.5;
			B.Rise = D.U(0.2, 0.6);
			B.Windage = 0.8;
			B.Life = D.U(SmokeLo, SmokeHi) * D.U(0.7, 1.0);
			B.R0 = 0.15 * Len + 0.05;
			B.R1 = D.U(0.6, 1.2) * Len * 2.0 + 0.3;
			B.Col = Grey * D.U(0.9, 1.1);
			B.Col2 = B.Col;
			B.Alpha = Len < 0.5 ? D.U(0.12, 0.22) : D.U(0.25, 0.45);
			B.FadeIn = 0.05;
			B.Hold = 0.1;
			B.Heart = 0.45;
			Bits.push_back(B);
		}
		// big guns throw a ring of dust off the ground in front
		const double Ring_ = Num(M + "dust_ring_m", 0.0);
		if (Ring_ > 0.0)
		{
			const Paint P = PaintOf("earth");
			const Vec FwFlat = Vec(Fw.X, Fw.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
			const Vec Foot0 = At + FwFlat * std::min(3.0, Ring_ * 0.3);
			const Vec Foot(Foot0.X, Foot0.Y, Height(Foot0.X, Foot0.Y));
			const int N = static_cast<int>(18.0 + Ring_ * 2.0);
			for (int I = 0; I < N; ++I)
			{
				const double A = D.U(0.0, 2.0 * Pi);
				const Vec Dir = (Vec(std::cos(A), std::sin(A), 0.0) + FwFlat * 1.2).Unit();
				const double Drag = 8.0;
				Bit B;
				B.Form = Bit::Shape::Upright;
				B.bHug = true;
				B.Born = Now + D.U(0.0, 0.03);
				B.From = Foot;
				B.Vel = Dir * (Ring_ * Drag * D.U(0.5, 1.0) * 0.5);
				B.Drag = Drag;
				B.Rise = 0.3;
				B.Windage = 0.6;
				B.Life = D.U(1.5, 2.5);
				B.R0 = 0.4;
				B.R1 = D.U(0.8, 1.4);
				B.Grow = 0.35;
				B.Along = Vec(-Dir.Y, Dir.X, 0.0);
				B.Stretch = 1.6;
				B.Flatten = 0.7;
				B.Col = P.Dust * 0.85;
				B.Col2 = B.Col;
				B.Alpha = D.U(0.35, 0.5);
				B.FadeIn = 0.02;
				B.Hold = 0.1;
				B.HazeBy = 0.8;
				B.HazeAlpha = 0.12;
				B.Heart = 0.55;
				Bits.push_back(B);
			}
		}
	}

	void Battle::Shots(double Now, int Gun, const Vec& From, const Vec& To, const char* Weapon, int Rounds, double PerSecond, const char* GroundIn)
	{
		// Brian: "1 out of every 5 bullets with yellow". As a belt was loaded: every fifth round of a gun is a tracer
		// and the four between are not seen; guns are out of step with each other.
		Gun = std::max(0, Gun);
		if (static_cast<size_t>(Gun) >= GunRounds.size())
		{
			GunRounds.resize(static_cast<size_t>(Gun) + 1, -1);
		}
		const int OneIn = std::max(1, static_cast<int>(Num("tracer.one_in", 5.0)));
		int& Count = GunRounds[static_cast<size_t>(Gun)];
		if (Count < 0)
		{
			Count = static_cast<int>(Mix64(static_cast<uint64_t>(Gun) + 977ull) % static_cast<uint64_t>(OneIn));
		}
		Dice D(++Seeds * 32452843ull ^ PlaceKey(From));
		const double Speed = Num("tracer.speed_mps", 760.0);
		const double Half = TracerHalf;
		const double Life = Num("tracer.life_s", 0.6);
		const Rgb Flame = FlameCol;
		const Vec Line = To - From;
		const double Dist = Line.Length();
		const Vec Dir = Line.Unit(Vec(1.0, 0.0, 0.0));
		const bool bFlicker = Weapon && std::string(Weapon) == "mg";
		const double Gap = 1.0 / std::max(PerSecond, 0.1);
		auto Streak = [&](double Born, const Vec& Start, const Vec& Way, double Fast, double For)
		{
			Tracer(Born, Start, Way, Way * Fast, For, 0.0, 0.0);
		};
		// (where the rounds strike the ground, small dust spurts kick up: art director 21:16Z)
		std::string Ground = GroundIn && *GroundIn ? GroundIn : "earth";
		if (!Has("grounds." + Ground + ".paint.soil.0"))
		{
			Ground = "earth";
		}
		const bool bOnGround = To.Z - Height(To.X, To.Y) < 1.5;
		const Rgb Dust = PaintOf(Ground).Dust;
		const double SpurtChance = bOnGround ? Num("bullet_strike.spurt_chance", 0.5) : 0.0;
		const double Scatter = 0.4 + Dist * Num("bullet_strike.spread_mil", 12.0) * 0.001;
		const double Reach = Dist / std::max(Speed, 1.0);
		for (int R = 0; R < Rounds; ++R)
		{
			const double T = Now + R * Gap;
			const int Which = Count++;
			if (D.U() < SpurtChance)
			{
				Vec At = To + Vec(D.Bell(), D.Bell(), 0.0) * (Scatter * 0.5);
				At.Z = Height(At.X, At.Y) + 0.05;
				for (int K = 0; K < 2; ++K)
				{
					Bit B;
					B.Born = T + Reach + D.U(0.0, 0.01);
					B.From = At;
					B.Vel = Vec(D.Bell() * 0.6, D.Bell() * 0.6, K == 0 ? D.U(2.5, 4.5) : D.U(0.6, 1.2));
					B.Gravity = 4.0;
					B.Drag = 1.5;
					B.Windage = 0.6;
					B.Life = D.U(0.5, 0.9);
					B.R0 = K == 0 ? 0.05 : 0.08;
					B.R1 = K == 0 ? D.U(0.25, 0.4) : D.U(0.4, 0.6);
					B.Grow = 0.5;
					B.Col = Dust;
					B.Col2 = Dust;
					B.Alpha = K == 0 ? 0.65 : 0.45;
					B.FadeIn = 0.01;
					B.Hold = 0.15;
					B.Heart = 0.7;
					B.Rim = 0.0;
					B.Rough = 0.3;
					B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
					Bits.push_back(B);
				}
			}
			if (bFlicker)
			{
				Bit B;
				B.Born = T;
				B.From = From + Dir * 0.15;
				B.Life = 0.025;
				B.R0 = B.R1 = 0.12;
				B.Along = Dir;
				B.Stretch = 2.0;
				B.Point = 0.8;
				B.Col = Flame;
				B.Col2 = Flame;
				B.Alpha = 0.9;
				B.FadeIn = 0.002;
				B.Hold = 0.5;
				B.Heart = 0.8;
				B.bLight = true;
				Bits.push_back(B);
			}
			if (Which % OneIn != 0)
			{
				continue;
			}
			const Vec Aim = (Dir + Vec(D.Bell(), D.Bell(), D.Bell()) * 0.006).Unit();
			if (Dist < WholePathBelow)
			{
				// (close in, a tracer crosses in under a frame or two: its whole path is drawn at once, muzzle to strike,
				// fading toward the muzzle, for at least one frame; art director 21:16Z)
				const double Third = Dist / 3.0;
				for (int K = 0; K < 3; ++K)
				{
					Tracer(T, From + Aim * (Third * K), Aim, Vec(), Reach, 0.0, 0.0, Third * 0.5, 0.35 + 0.325 * K);
				}
			}
			else
			{
				Streak(T, From, Aim, Speed, std::min(Life, Reach));
			}
			// (a tracer striking the ground or armour sometimes skips up at an angle)
			if (Reach < Life && D.U() < Num("tracer.ricochet.chance", 0.18))
			{
				const double Up_ = Radians(Pick(D, "tracer.ricochet.deflect_deg", 10.0, 50.0));
				const double Yaw = Radians(D.U(-25.0, 25.0));
				const Vec Flat_ = Vec(Aim.X * std::cos(Yaw) - Aim.Y * std::sin(Yaw), Aim.X * std::sin(Yaw) + Aim.Y * std::cos(Yaw), 0.0).Unit(Vec(1.0, 0.0, 0.0));
				const Vec Off = (Flat_ * std::cos(Up_) + ZUp * std::sin(Up_)).Unit();
				Streak(T + Reach, From + Aim * Dist - Off * Half, Off, Speed * Num("tracer.ricochet.slower", 0.5), std::max(0.15, Life - Reach));
			}
		}
	}

	void Battle::Tracer(double Born, const Vec& Start, const Vec& Way, const Vec& Vel, double For, double Drag, double Drop, double Half, double AlphaK)
	{
		if (Half <= 0.0)
		{
			Half = TracerHalf;
		}
		for (int Layer = 0; Layer < 2; ++Layer)
		{
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.Born = Born;
			B.From = Start + Way * Half;
			B.Vel = Vel;
			B.Drag = Drag;
			B.Gravity = Drop;
			B.Life = For;
			B.HalfLength = Half;
			B.Along = Way;
			B.R0 = B.R1 = Layer == 0 ? 0.045 : 0.018;
			B.LeastPx = Layer == 0 ? TracerLeast : TracerLeast * 0.4;
			B.Col = Layer == 0 ? TracerCol : TracerPale; // (paler along its middle)
			B.Col2 = B.Col;
			B.Alpha = (Layer == 0 ? 0.8 : 1.0) * AlphaK;
			B.FadeIn = 0.001;
			B.Hold = 0.9;
			B.Heart = 0.9;
			B.Rim = Layer == 0 ? 0.2 : 0.5;
			B.Point = 0.3;
			B.Tail = 0.7; // (thinning toward the gun)
			B.bLight = true;
			B.Glow = static_cast<uint8_t>(Layer + 1);
			Bits.push_back(B);
		}
	}

	const Battle::AirRow& Battle::AirRowOf(const std::string& Kind)
	{
		for (const AirRow& R : AirRows)
		{
			if (R.Name == Kind)
			{
				return R;
			}
		}
		const std::string P = "air_guns." + Kind + ".";
		AirRow R;
		if (Kind != "hmg" && !Has(P + "muzzle_mps"))
		{
			R = AirRowOf("hmg"); // (a kind the recipe does not know fires as a heavy machine gun)
		}
		else
		{
			R.Rate = Num(P + "rate_per_min", 800.0);
			R.Mps = Num(P + "muzzle_mps", 870.0);
			R.Flash = Num(P + "flash_m", 0.45);
			R.Strike = Num(P + "strike_m", 0.8);
			R.Drag = Num(P + "drag", 0.36);
			R.Drop = Num(P + "drop", 1.0) > 0.0 ? Num("air_guns.gravity", 9.81) : 0.0;
			R.TracerS = Num(P + "tracer_s", 2.4);
			R.Spread = Num(P + "spread_mil", 4.0) * 0.001;
			R.OneIn = std::max(1, static_cast<int>(Num(P + "tracer_every", Num("tracer.one_in", 5.0))));
			R.SmokeEvery = std::max(1, static_cast<int>(Num(P + "smoke_every", 3.0)));
			R.SmokeS = Num(P + "smoke_s", 0.8);
			R.SmokeAlpha = Num(P + "smoke_alpha", 0.1);
			R.Licks = std::max(0, static_cast<int>(Num(P + "licks", 0.0)));
			R.Lamp = Num(P + "lamp", 0.0);
		}
		R.Name = Kind;
		AirRows.push_back(R);
		return AirRows.back();
	}

	void Battle::AirGun(double Now, int Gun, const Vec& Muzzle, const Vec& Aim, const Vec& Carry, const char* Kind, double RatePerMin, double MuzzleMps)
	{
		// Brian 19:35Z: "make the gun fire very realistic and the tracer fire too". Each gun fires at its own rate
		// (no two quite alike, so wing guns drift out of step), from wherever the muzzle was at the moment of each
		// round; a round flies at the gun's speed plus the aircraft's own, slows in the air and falls. The tracer is
		// the ruled streak, one in five.
		Gun = std::max(0, Gun);
		const AirRow& Row = AirRowOf(Kind && *Kind ? Kind : "hmg");
		Firing* F = nullptr;
		for (Firing& Each : Firings)
		{
			if (Each.Gun == Gun)
			{
				F = &Each;
				break;
			}
		}
		if (!F)
		{
			Firing New;
			New.Gun = Gun;
			New.Count = static_cast<int>(Mix64(static_cast<uint64_t>(Gun) + 1931ull) % static_cast<uint64_t>(Row.OneIn));
			New.RateK = 0.97 + 0.06 * static_cast<double>(Mix64(static_cast<uint64_t>(Gun) + 7177ull) % 1000ull) / 999.0;
			Firings.push_back(New);
			F = &Firings.back();
		}
		const Vec Dir = (Aim - Muzzle).Unit(F->bOn ? F->Dir : Carry.Unit(Vec(1.0, 0.0, 0.0)));
		const double Gap = 60.0 / std::max((RatePerMin > 0.0 ? RatePerMin : Row.Rate) * F->RateK, 1.0);
		const double Mps = MuzzleMps > 0.0 ? MuzzleMps : Row.Mps;
		Dice D(++Seeds * 86028121ull ^ PlaceKey(Muzzle));
		if (!F->bOn || Now < F->Last || Now - F->Last > 0.25)
		{
			// (a new burst, or a gun not heard from for a while: the first round comes within half a gap)
			F->bOn = true;
			F->Last = Now;
			F->Muzzle = Muzzle;
			F->Dir = Dir;
			F->Carry = Carry;
			F->Next = Now + D.U(0.0, Gap * 0.5);
		}
		const double Between = Now - F->Last;
		for (int Fired = 0; F->Next <= Now && Fired < 64; ++Fired)
		{
			const double K = Between > 1e-9 ? Clamp01((F->Next - F->Last) / Between) : 1.0;
			const Vec At = F->Muzzle + (Muzzle - F->Muzzle) * K;
			const Vec Way = (F->Dir + (Dir - F->Dir) * K).Unit(Dir);
			const Vec By = F->Carry + (Carry - F->Carry) * K;
			const int Which = F->Count++;
			AirRound(F->Next, At, Way, By, Row, Mps, Which % Row.OneIn == 0, Which % Row.SmokeEvery == 0, D);
			F->Next += Gap;
		}
		F->Next = std::max(F->Next, Now); // (a long stall: the rounds of the gap are not made up)
		F->Last = Now;
		F->Muzzle = Muzzle;
		F->Dir = Dir;
		F->Carry = Carry;
	}

	void Battle::AirGunStop(int Gun)
	{
		for (Firing& F : Firings)
		{
			if (F.Gun == std::max(0, Gun))
			{
				F.bOn = false;
			}
		}
	}

	void Battle::AirRound(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, double Mps, bool bTracer, bool bSmoke, Dice& D)
	{
		// the flicker: a tongue along the bore, under one frame, carried with the aircraft so it stays on the gun
		// (machine guns, passed 21:07Z); cannon have their own flash
		const double Len = Row.Flash * D.U(0.8, 1.15);
		if (Row.Licks > 0)
		{
			CannonFlash(T, At, Dir, Carry, Row, D);
		}
		else
		{
			Bit B;
			B.Born = T;
			B.From = At + Dir * (Len * 0.5);
			B.Vel = Carry;
			B.Life = D.U(0.02, 0.035);
			B.R0 = Len * 0.2;
			B.R1 = Len * 0.28;
			B.Along = Dir;
			B.Stretch = 2.3;
			B.Point = 0.9;
			B.Tail = 0.1;
			B.Col = FlashCoreCol.Mix(FlameCol, 0.35);
			B.Col2 = FlameCol;
			B.Alpha = 1.0;
			B.FadeIn = 0.002;
			B.Hold = 0.5;
			B.Heart = 0.85;
			B.Rim = 0.2;
			B.bLight = true;
			Bits.push_back(B);
		}
		if (Row.Lamp > 0.0)
		{
			Flash L;
			L.At = At + Dir * (Len * 0.5);
			L.Col = FlameCol;
			L.Born = T;
			L.Life = 0.03;
			L.Strength = Row.Lamp;
			L.Radius = 12.0;
			L.bNightOnly = true;
			Flashes.push_back(L);
		}
		// gun smoke: a thin grey wisp left hanging in the air where the gun was, so a firing wing trails a short
		// smoky line behind it
		if (bSmoke && Row.SmokeAlpha > 0.0 && Bits.size() < static_cast<size_t>(MaxPuffs) * 4 / 5) // (the first thing shed when the room fills)
		{
			Bit B;
			B.Born = T + 0.01;
			B.From = At + Dir * (Len * 0.8);
			B.Vel = Dir * 6.0;
			B.Drag = 4.0;
			B.Windage = 1.0;
			B.Life = Row.SmokeS * D.U(0.8, 1.2);
			B.R0 = Len * 0.3;
			B.R1 = Len * D.U(1.4, 2.0);
			B.Grow = 0.4;
			B.Col = GunSmokeCol;
			B.Col2 = GunSmokeCol;
			B.Alpha = Row.SmokeAlpha * D.U(0.7, 1.2);
			B.FadeIn = 0.04;
			B.Hold = 0.15;
			B.Heart = 0.6;
			B.Rim = 0.0;
			Bits.push_back(B);
		}
		if (bTracer)
		{
			const Vec Aim = (Dir + Vec(D.Bell(), D.Bell(), D.Bell()) * Row.Spread).Unit(Dir);
			const Vec Vel = Aim * Mps + Carry;
			Tracer(T, At, Vel.Unit(Aim), Vel, Row.TracerS, Row.Drag, Row.Drop);
		}
	}

	void Battle::CannonFlash(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, Dice& D)
	{
		// Art director 21:07Z: a hot pale heart at the muzzle with orange only toward its edges; a ragged forward
		// tongue two to three times the side licks' length; short side licks bent aft by the airstream; soft, uneven,
		// never the same twice; one frame at full, a second at about half (Life 0.05 with Hold 0.45), then gone.
		const Vec Across = Dir.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
		const Vec Over = Across.Cross(Dir).Unit(ZUp);
		const double Len = Row.Flash * D.U(0.8, 1.2);
		auto Lick = [&](const Vec& From, const Vec& Way, double R, double Stretch, const Rgb& Col, const Rgb& Col2, double Alpha, double Heart, double Rim, double Rough)
		{
			Bit B;
			B.Born = T;
			B.From = From;
			B.Vel = Carry;
			B.Life = 0.05 * D.U(0.9, 1.1);
			B.R0 = R * 0.9;
			B.R1 = R * 1.1;
			B.Along = Way;
			B.Stretch = Stretch;
			B.Point = 0.7;
			B.Tail = 0.15;
			B.Col = Col;
			B.Col2 = Col2;
			B.Alpha = Alpha;
			B.FadeIn = 0.002;
			B.Hold = 0.45;
			B.Heart = Heart;
			B.Rim = Rim;
			B.Rough = Rough;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.bLight = true;
			Bits.push_back(B);
		};
		// the orange edge round the heart: see-through in its middle, strongest at its heart ring, soft at its rim
		Lick(At + Dir * (Len * 0.14), Dir, Len * 0.3, 1.3, FlameCol, FlameCol * 0.7, 0.3, 2.6, 0.3, 0.35);
		// the forward tongue: three ragged pieces, pale at the root and orange beyond, each a little off the line
		const double Reach = Len * D.U(1.1, 1.4);
		for (int K = 0; K < 3; ++K)
		{
			const double Along = Reach * (0.3 + 0.33 * K) * D.U(0.9, 1.1);
			const Vec Off = (Across * D.Bell() + Over * D.Bell()) * (Len * 0.05 * (K + 1));
			const Rgb Col = K == 0 ? CannonHeartCol.Mix(FlameCol, 0.45) : FlameCol;
			Lick(At + Dir * Along + Off, (Dir + Off * (0.6 / std::max(Len, 1e-3))).Unit(Dir), Len * (0.2 - 0.045 * K) * D.U(0.85, 1.15), 2.2, Col, FlameCol * 0.75, 0.9, 0.8, 0.15, 0.4);
		}
		// short side licks, raked aft by the air the aircraft flies into; a third of the tongue or less
		for (int I = 0; I < Row.Licks; ++I)
		{
			const double A = (I * 2.0 * Pi) / std::max(1, Row.Licks) + D.U(-0.6, 0.6);
			const Vec Outward = (Across * std::cos(A) + Over * std::sin(A)).Unit(Across);
			const Vec Way = (Outward - Dir * D.U(0.3, 0.55)).Unit(Outward);
			const double R = Len * D.U(0.06, 0.085);
			const double Stretch = D.U(2.2, 2.8);
			Lick(At + Dir * (Len * 0.08) + Way * (R * Stretch * 0.8), Way, R, Stretch, FlameCol, FlameCol * 0.6, 0.85, 0.8, 0.1, 0.3);
		}
		// the hot heart right at the muzzle, laid last so it is pale over the orange
		Lick(At + Dir * (Len * 0.1), Dir, Len * 0.15, 1.4, CannonHeartCol, CannonHeartCol.Mix(FlameCol, 0.5), 1.0, 0.95, 0.35, 0.2);
	}

	void Battle::AirHit(double Now, const Vec& At, const Vec& Normal, const Vec& Carry, const char* Kind)
	{
		// Rounds striking an aircraft: incendiary and HE rounds spark and flash on the skin, tear off scraps of it that
		// tumble away behind, and leave a small dark puff hanging where the aircraft was. Bigger for cannon.
		const AirRow& Row = AirRowOf(Kind && *Kind ? Kind : "hmg");
		const double S = std::max(0.2, Row.Strike);
		Dice D(++Seeds * 67867967ull ^ PlaceKey(At));
		const Vec Skin = Normal.Unit(Vec(0.0, 0.0, 1.0));
		const Rgb Spark = Picture("air_hit.spark_colour", Rgb::FromPicture(255, 236, 180));
		auto Away = [&](double Spread) { return (Skin + Vec(D.Bell(), D.Bell(), D.Bell()) * Spread).Unit(Skin); };
		// the spit: a hot flash on the skin, one frame and a half
		{
			Bit B;
			B.Born = Now;
			B.From = At + Skin * (0.1 * S);
			B.Vel = Carry;
			B.Life = 0.045;
			B.R0 = 0.22 * S;
			B.R1 = 0.3 * S;
			B.Col = Spark;
			B.Col2 = FlameCol;
			B.Alpha = 1.0;
			B.FadeIn = 0.002;
			B.Hold = 0.4;
			B.Heart = 0.9;
			B.Rim = 0.25;
			B.Rough = 0.35;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			B.bLight = true;
			Bits.push_back(B);
		}
		const int NTongues = D.Whole(2, 3);
		for (int I = 0; I < NTongues; ++I)
		{
			const Vec Way = Away(0.6);
			Bit B;
			B.Born = Now;
			B.From = At + Way * (0.2 * S);
			B.Vel = Carry + Way * 4.0;
			B.Life = D.U(0.04, 0.07);
			B.R0 = 0.08 * S;
			B.R1 = 0.12 * S;
			B.Along = Way;
			B.Stretch = 2.5;
			B.Point = 0.8;
			B.Col = FlashCoreCol;
			B.Col2 = FlameCol * 0.6;
			B.Alpha = 0.95;
			B.FadeIn = 0.002;
			B.Hold = 0.4;
			B.Heart = 0.8;
			B.Rim = 0.1;
			B.Rough = 0.3;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			B.bLight = true;
			Bits.push_back(B);
		}
		// sparks: short bright streaks thrown off the skin, slowing in the air so they fall away behind (their streak is a
		// fixed half a metre or so, not the aircraft's own speed, or each would read as a tracer)
		const int NSparks = std::max(1, static_cast<int>(std::lround(Pick(D, "air_hit.sparks", 3.0, 6.0) * S)));
		for (int I = 0; I < NSparks; ++I)
		{
			const Vec Way = Away(0.9);
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.Born = Now;
			B.From = At;
			B.Vel = Carry + Way * D.U(20.0, 60.0);
			B.Drag = 3.0;
			B.HalfLength = D.U(0.15, 0.35);
			B.Along = Way;
			B.Life = D.U(0.12, 0.3);
			B.R0 = B.R1 = 0.025;
			B.Col = Spark;
			B.Col2 = FlameCol * 0.6;
			B.Alpha = 1.0;
			B.FadeIn = 0.002;
			B.Hold = 0.4;
			B.Heart = 0.9;
			B.Rim = 0.3;
			B.LeastPx = 1.5;
			B.bLight = true;
			Bits.push_back(B);
		}
		// scraps of skin: dark, some catching the light, tumbling and left behind (lumps, not streaks: a streak is drawn as
		// long as its speed, and a scrap leaves at the aircraft's)
		const Rgb Scrap = Picture("air_hit.scrap_colour", Rgb::FromPicture(70, 72, 70));
		const Rgb Bare = Picture("air_hit.bare_colour", Rgb::FromPicture(150, 152, 156));
		const int NScraps = std::max(1, static_cast<int>(std::lround(Pick(D, "air_hit.scraps", 1.0, 2.0) * S)));
		for (int I = 0; I < NScraps; ++I)
		{
			Bit B;
			B.Born = Now;
			B.From = At + Skin * 0.1;
			B.Vel = Carry + Away(0.7) * D.U(4.0, 14.0);
			B.Drag = D.U(0.8, 1.6); // (a sheet of light metal soon loses the aircraft's speed)
			B.Gravity = 9.8;
			B.Life = D.U(1.4, 2.4);
			B.R0 = B.R1 = D.U(0.05, 0.12) * S;
			B.Col = D.U() < 0.3 ? Bare : Scrap;
			B.Col2 = B.Col;
			B.Alpha = 1.0;
			B.FadeIn = 0.002;
			B.Hold = 0.85;
			B.Heart = 1.0;
			B.Rim = 0.9;
			B.LeastPx = ClodLeast;
			Chunk(B, D);
			Bits.push_back(B);
		}
		// a small dark puff hanging where the hit was (the aircraft flies on out of it)
		const Rgb Smoke = Picture("air_hit.smoke_colour", Rgb::FromPicture(70, 68, 64));
		const int NPuffs = S > 1.0 ? 2 : 1;
		for (int I = 0; I < NPuffs; ++I)
		{
			Bit B;
			B.Born = Now + 0.01;
			B.From = At + Skin * (0.2 * S) + Vec(D.Bell(), D.Bell(), D.Bell()) * (0.15 * S);
			B.Vel = Carry * 0.15 + Skin * 2.0;
			B.Drag = 3.0;
			B.Windage = 1.0;
			B.Life = Pick(D, "air_hit.smoke_s", 0.8, 1.5);
			B.R0 = 0.25 * S;
			B.R1 = 1.1 * S * D.U(0.8, 1.2);
			B.Grow = 0.4;
			B.Col = Smoke;
			B.Col2 = Smoke.Mix(Rgb::FromPicture(130, 128, 124), 0.5);
			B.Alpha = Num("air_hit.smoke_alpha", 0.55);
			B.FadeIn = 0.02;
			B.Hold = 0.2;
			B.Heart = 0.7;
			B.Rim = 0.0;
			B.Rough = 0.3;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			Bits.push_back(B);
		}
		Flash L;
		L.At = At + Skin * (0.3 * S);
		L.Col = Spark;
		L.Born = Now;
		L.Life = 0.04;
		L.Strength = 0.08 * S;
		L.Radius = 12.0;
		L.bNightOnly = true;
		Flashes.push_back(L);
	}

	void Battle::AirFlak(double Now, const Vec& At, const char* Kind)
	{
		// The art director's hanging-smoke brief (2026-11-16 03:44Z), the look Brian picked: smoke, never a body. Five
		// to eight lobes of different sizes in a crooked chain, longer along the wind and lopsided, with three to five
		// thin wisps trailing downwind; dense at the heart and fading to nothing at the edge, torn more as it ages,
		// lit only on its sunward side. A soft flash of about 0.2 s with no sparks sits off-centre in it. It hangs
		// where it burst and drifts with the wind; it greys, stretches to about three times its length by 8 s, and
		// fades away by about 9 s. Never cut off.
		std::string Name = Kind && *Kind ? Kind : "heavy";
		if (!Has("air_flak." + Name + ".long_m"))
		{
			Name = "heavy"; // (a kind the recipe does not know bursts as heavy)
		}
		const std::string P = "air_flak." + Name + ".";
		Dice D(++Seeds * 86028121ull ^ PlaceKey(At));
		const double Half = std::max(0.5, 0.5 * Num(P + "long_m", 22.0)); // (floored: a zero length would divide by nothing below)
		const double AcrossK = Num(P + "across_m", 16.0) / std::max(2.0 * Half, 1e-3);
		const Vec Down = Vec(Wind.X, Wind.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0)); // (down the wind, level)
		const Vec Side = ZUp.Cross(Down).Unit(Vec(0.0, 1.0, 0.0));
		const double Stretch = Num(P + "stretch_mps", 2.0);
		const double Downwind = Num(P + "downwind_mps", 0.6);
		const double Grow = Num(P + "grow", 1.6);
		const double Alpha = Num(P + "alpha", 0.75);
		const double HalfBy = Num(P + "half_by_s", 8.0);
		const double Shade = Num(P + "cloud_shade", 4.0);
		const double HeartEnd = Num(P + "heart_end", 0.4);
		const double Spread = Num(P + "spread_mps", 0.8);
		const double LeastPx = Num(P + "least_px", 9.0);
		const Rgb Young = Picture(P + "colour", Rgb::FromPicture(46, 43, 42));
		const Rgb Old = Picture(P + "colour_old", Rgb::FromPicture(128, 122, 116));
		auto Smoke = [&](const Vec& From, double R, double R1, const Vec& Vel, double A, double Life)
		{
			Bit B;
			B.Born = Now;
			B.From = From;
			B.Vel = Vel;
			B.Windage = 1.0;
			B.Life = Life;
			B.R0 = R;
			B.R1 = R1;
			B.Grow = 0.8;
			B.Col = Young;
			B.Col2 = Old;
			B.Alpha = A;
			B.HazeBy = HalfBy; // (its opacity falls to half by half_by_s, then it fades out over its last tenth)
			B.HazeAlpha = 0.5 * A;
			B.FadeIn = 0.04;
			B.Hold = 0.88;
			B.Heart = 0.9;
			B.HeartEnd = HeartEnd; // (art director 21:47Z: an old burst loses its dark core first, pale and ragged)
			B.Rim = 0.0;
			B.Rough = 0.5;
			B.LeastPx = LeastPx; // (and at range it never shrinks into a solid dot)
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.TurnRate = D.Bell() * 0.08;
			B.Centre = At;
			B.CloudShade = Shade;
			B.CloudR = Half;
			return B;
		};
		// the lobes: a crooked chain down the wind, never a clump: spread along the wind, wandering across it and up
		// and down, of different sizes, each drawn out along the wind; one end of the chain longer than the other
		const int Lobes = static_cast<int>(std::lround(Pick(D, P + "lobes", 5.0, 8.0)));
		const double Lopsided = D.U(0.75, 1.3);
		double Ps = D.Bell() * 0.15 * Half, Pz = D.Bell() * 0.1 * Half;
		for (int I = 0; I < Lobes; ++I)
		{
			double Pa = (-0.45 + 0.9 * (I + D.U(-0.3, 0.3)) / std::max(1, Lobes - 1)) * Half;
			Pa *= Pa > 0.0 ? Lopsided : 1.0 / Lopsided;
			const double R = Half * Pick(D, P + "lobe_r", 0.3, 0.55);
			const Vec From = At + Down * Pa + Side * (Ps * AcrossK) + ZUp * (Pz * AcrossK);
			// (each end of the chain draws away along the wind, the downwind end faster: it stretches and leans)
			// (and it spreads across and up and down as it ages, so an old burst is taller as well as longer)
			const Vec Vel = Down * (Stretch * Pa / Half + Downwind) + (Side * (Ps / Half) + ZUp * (Pz / Half) + Side * (0.4 * D.Bell()) + ZUp * (0.3 * D.Bell())) * Spread;
			const double Life = Pick(D, P + "life_s", 8.5, 9.5);
			Bit B = Smoke(From, R, R * Grow * D.U(0.85, 1.15), Vel, Alpha * D.U(0.6, 1.0), Life);
			B.Along = (Down + Side * (0.25 * D.Bell()) + ZUp * (0.2 * D.Bell())).Unit(Down);
			B.Stretch = D.U(1.0, 1.4);
			Bits.push_back(B);
			Ps = std::clamp(Ps + D.Bell() * 0.35 * Half, -0.5 * Half, 0.5 * Half);
			Pz = std::clamp(Pz + D.Bell() * 0.25 * Half, -0.35 * Half, 0.35 * Half);
		}
		// wisps trailing downwind, thin, coming in as the burst smears
		const int Wisps = static_cast<int>(std::lround(Pick(D, P + "wisps", 3.0, 5.0)));
		for (int I = 0; I < Wisps; ++I)
		{
			const double R = Half * D.U(0.12, 0.22);
			const Vec From = At + Down * (Half * D.U(0.2, 1.1)) + Side * (D.Bell() * 0.4 * Half) + ZUp * (D.Bell() * 0.2 * Half);
			Bit B = Smoke(From, R, R * 2.0, Down * (Stretch * D.U(1.0, 1.6) + Downwind), D.U(0.2, 0.4), Pick(D, P + "life_s", 8.5, 9.5) * D.U(0.85, 1.0));
			B.Along = Down;
			B.Stretch = D.U(2.4, 3.2);
			B.FadeIn = 0.6;
			B.Rough = 0.6;
			Bits.push_back(B);
		}
		// the flash (art director 21:47Z: never an orange disc with a white dot): a torn glow, its edge at least two to
		// one, with two or three short bright spikes thrown out; the white-hot heart a third to half the glow's radius
		// off its middle; the heart gone in 0.05 s and the glow in 0.1 s, so the smoke owns the frame. Off-centre in the
		// smoke and laid in front of it from the eye.
		const double GlowS = Num(P + "flash_glow_s", 0.1);
		const double HeartS = Num(P + "flash_heart_s", 0.05);
		const Vec Off = (Down * D.Bell() + Side * D.Bell() + ZUp * D.Bell()) * (0.25 * Half);
		const Vec ToEye = bEyeKnown ? (LastEye - At).Unit(ZUp) : Vec();
		const Vec Hot = At + Off + ToEye * (0.6 * Half);
		// (two ways square to the eye's line, so what is off the middle shows as off the middle)
		const Vec FaceA = (bEyeKnown ? ToEye.Cross(ZUp) : Side).Unit(Side);
		const Vec FaceB = (bEyeKnown ? FaceA.Cross(ToEye) : ZUp).Unit(ZUp);
		const Rgb GlowCol = Picture(P + "flash_glow", Rgb::FromPicture(150, 70, 14));
		const Rgb MidCol = Picture(P + "flash_mid", Rgb::FromPicture(255, 170, 60));
		const Rgb HeartCol = Picture(P + "flash_heart", Rgb::FromPicture(255, 245, 215));
		const double GlowR = Num(P + "flash_glow_m", 8.0) * D.U(0.85, 1.15);
		auto Glow = [&](const Vec& From, const Vec& Vel, const Vec& Along, double R, double Stretch_, double Life, const Rgb& Col, double A, double Heart, double Rough, double Point)
		{
			Bit B;
			B.Born = Now;
			B.From = From;
			B.Vel = Vel;
			B.Windage = 1.0;
			B.Life = Life;
			B.R0 = R;
			B.R1 = R * 1.15;
			B.Along = Along;
			B.Stretch = Stretch_;
			B.Point = Point;
			B.Col = Col;
			B.Col2 = Col * 0.6;
			B.Alpha = A;
			B.FadeIn = 0.005;
			B.Hold = 0.35;
			B.Heart = Heart;
			B.Rim = 0.0;
			B.Rough = Rough;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.bLight = true;
			Bits.push_back(B);
		};
		const double Tilt = D.U(0.0, 2.0 * Pi);
		const Vec GlowWay = FaceA * std::cos(Tilt) + FaceB * std::sin(Tilt);
		Glow(Hot, Vec(), GlowWay, GlowR, D.U(1.2, 1.45), GlowS * D.U(0.9, 1.1), GlowCol, 0.55, 0.7, 0.6, 0.0);
		// the spikes: short bright fragments thrown out past the glow's edge
		const int Spikes = static_cast<int>(std::lround(Pick(D, P + "flash_spikes", 2.0, 3.0)));
		for (int I = 0; I < Spikes; ++I)
		{
			const double A_ = Tilt + (I * 2.0 * Pi) / std::max(1, Spikes) + D.U(-0.7, 0.7);
			const Vec Thrown = FaceA * std::cos(A_) + FaceB * std::sin(A_);
			Glow(Hot + Thrown * (GlowR * D.U(0.75, 0.95)) + ToEye * 0.5, Thrown * D.U(20.0, 35.0), Thrown, GlowR * D.U(0.07, 0.1), D.U(2.0, 2.6), GlowS * D.U(0.5, 0.8), MidCol.Mix(HeartCol, 0.3), 0.9, 0.85, 0.3, 0.6);
		}
		// the heart, a third to half the glow's radius off its middle, inside a hotter orange
		const double Turn_ = D.U(0.0, 2.0 * Pi);
		const Vec HeartAt = Hot + (FaceA * std::cos(Turn_) + FaceB * std::sin(Turn_)) * (GlowR * D.U(0.33, 0.5));
		Glow(HeartAt + (Hot - HeartAt) * 0.25 + ToEye * 0.5, Vec(), GlowWay, Num(P + "flash_mid_m", 3.5) * D.U(0.85, 1.15), D.U(1.2, 1.5), (HeartS + GlowS) * 0.5, MidCol, 0.8, 0.85, 0.45, 0.0);
		Glow(HeartAt + ToEye * 1.0, Vec(), GlowWay, Num(P + "flash_heart_m", 1.6) * D.U(0.85, 1.15), 1.2, HeartS, HeartCol, 1.0, 0.8, 0.25, 0.0);
		Flash L;
		L.At = Hot;
		L.Col = Picture(P + "flash_mid", Rgb::FromPicture(255, 170, 60));
		L.Born = Now;
		L.Life = GlowS;
		L.Strength = Num(P + "lamp", 0.35);
		L.Radius = Num(P + "lamp_reach_m", 60.0);
		Flashes.push_back(L);
	}

	const Battle::TrailRow& Battle::TrailRowOf(const std::string& Kind)
	{
		for (const TrailRow& R : TrailRows)
		{
			if (R.Name == Kind)
			{
				return R;
			}
		}
		const std::string P = "air_trails." + Kind + ".";
		TrailRow R;
		if (Kind != "smoke" && !Has(P + "life_s"))
		{
			R = TrailRowOf("smoke"); // (a kind the recipe does not know trails as smoke)
		}
		else
		{
			R.Life = Num(P + "life_s", 6.0);
			R.Spacing = std::max(1.0, Num(P + "spacing_m", 8.0));
			R.R0 = Num(P + "r0_m", 0.5);
			R.R1 = Num(P + "r1_m", 3.5);
			R.Alpha = Num(P + "alpha", 0.5);
			R.DarkenS = std::max(0.01, Num(P + "darken_s", 4.0));
			R.RampS = std::max(0.01, Num(P + "ramp_s", 1.5));
			R.Smear = Num(P + "smear", 1.5);
			R.Carried = Num(P + "carried", 0.05);
			R.Flame = Num(P + "flame_m", 0.0);
			R.Grow = std::max(0.1, Num(P + "grow", 0.5));
			R.Lumpy = Clamp01(Num(P + "lumpy", 0.0));
			R.SmearS = std::max(0.0, Num(P + "smear_s", 0.0));
			R.Rise = Num(P + "rise_mps", 0.0);
			R.FlameLift = Range(P + "flame_lift_m", 0.3, 0.6);
			R.Col = Picture(P + "colour", Rgb::FromPicture(128, 126, 122));
			R.ColFull = Picture(P + "colour_full", R.Col);
			R.ColOld = Picture(P + "colour_old", R.ColFull);
		}
		R.Name = Kind;
		TrailRows.push_back(R);
		return TrailRows.back();
	}

	void Battle::TrailStart(double Now, int Id, const Vec& At, const Vec& Carry, const char* Kind)
	{
		const std::string Name = Kind && *Kind ? Kind : "smoke";
		for (Trailing& Tr : Trails)
		{
			if (Tr.Id == Id)
			{
				if (!Tr.bOn)
				{
					Tr.bOn = true;
					Tr.Born = Now;
					Tr.Last = Now;
					Tr.Owed = 0.0;
					Tr.LastPuff = Now;
					Tr.At = At;
					Tr.Carry = Carry;
					Tr.LastPuffAt = At;
				}
				if (Tr.Kind != Name)
				{
					Tr.Kind = Name;
					Tr.KindBorn = Now; // (smoke catching fire: the new kind ramps in from now)
				}
				return;
			}
		}
		Trailing Tr;
		Tr.Id = Id;
		Tr.bOn = true;
		Tr.Kind = Name;
		Tr.Born = Tr.KindBorn = Tr.Last = Tr.LastPuff = Tr.NextFlame = Now;
		Tr.At = At;
		Tr.Carry = Carry;
		Tr.LastPuffAt = At;
		Trails.push_back(Tr);
	}

	void Battle::TrailPuff(double T, const Vec& At, const Vec& Carry, const Trailing& Tr, const TrailRow& Row, Dice& D)
	{
		// Art director 21:27Z: thin and dark at the engine; about three times wider by 2 s and still widening; breaking
		// into uneven lumps, paler and thinner with age; each lump hanging where it was laid. A trail still comes to
		// full over ramp_s and darkens over darken_s as the damage takes hold.
		const double Ramp = Clamp01((T - Tr.KindBorn) / Row.RampS);
		const double Dark = Clamp01((T - Tr.KindBorn) / Row.DarkenS);
		const Vec Way = Carry.Unit(Vec(1.0, 0.0, 0.0));
		// (some puffs are lumps, bigger and longer lived; the rest between them are smaller and go sooner, so an old
		// trail is lumps with gaps where once it was a line)
		const bool bLump = D.U() < 0.4;
		const double Size = bLump ? 1.0 + 0.45 * Row.Lumpy * D.U(0.5, 1.0) : 1.0 - 0.5 * Row.Lumpy * D.U(0.5, 1.0);
		const double Lasts = bLump ? 1.0 + 0.1 * Row.Lumpy : 1.0 - 0.4 * Row.Lumpy * D.U(0.6, 1.0);
		Bit B;
		B.Born = T;
		B.From = At + Vec(D.Bell(), D.Bell(), D.Bell()) * (Row.R0 * 0.3);
		B.Vel = Carry * Row.Carried + Vec(D.Bell(), D.Bell(), 0.5 * D.Bell()) * (3.0 * Row.Lumpy);
		B.Drag = 2.0;
		B.Rise = Row.Rise * D.U(0.6, 1.4);
		B.Windage = 1.0;
		B.Life = Row.Life * Lasts * D.U(0.85, 1.15);
		const double Thick = 0.7 + 0.3 * Ramp;
		B.R0 = Row.R0 * Thick * D.U(0.85, 1.15);
		B.R1 = Row.R1 * Thick * Size * D.U(0.85, 1.15);
		B.Grow = Row.Grow;
		B.Along = Way;
		B.Smear = Row.Spacing * Row.Smear;
		B.SmearFor = Row.SmearS;
		B.Col = Row.Col.Mix(Row.ColFull, Dark);
		B.Col2 = Row.ColOld;
		B.Cool = Row.Lumpy > 0.0 ? 1.0 : 0.6; // (paling toward colour_old as it hangs)
		B.Alpha = Row.Alpha * (0.45 + 0.55 * Ramp) * (bLump ? 1.0 + 0.15 * Row.Lumpy : 1.0 - 0.25 * Row.Lumpy) * D.U(0.9, 1.05);
		B.FadeIn = 0.01;
		B.Hold = 0.25;
		B.Heart = 0.7;
		B.Rim = 0.0;
		B.Rough = 0.25 + 0.2 * Row.Lumpy;
		B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
		Bits.push_back(B);
	}

	void Battle::TrailMove(double Now, int Id, const Vec& At, const Vec& Carry)
	{
		Trailing* Tr = nullptr;
		for (Trailing& Each : Trails)
		{
			if (Each.Id == Id && Each.bOn)
			{
				Tr = &Each;
				break;
			}
		}
		if (!Tr)
		{
			return;
		}
		const TrailRow& Row = TrailRowOf(Tr->Kind);
		Dice D(++Seeds * 49979693ull ^ PlaceKey(At));
		if (Now < Tr->Last || Now - Tr->Last > 0.5)
		{
			// (a long silence or time going back: start laying again from here)
			Tr->Last = Now;
			Tr->At = At;
			Tr->Carry = Carry;
			Tr->Owed = 0.0;
			Tr->LastPuffAt = At;
		}
		const double Between = Now - Tr->Last;
		const Vec Path = At - Tr->At;
		const double Long = Path.Length();
		// (fire: its smoke starts at the flame's tips, not at the cowl)
		const double Grown = Clamp01((Now - Tr->KindBorn) / Row.RampS);
		Tr->Tip = Row.Flame * (0.45 + 0.55 * Grown);
		const Vec TipBack = -Carry.Unit(Vec(1.0, 0.0, 0.0)) * Tr->Tip;
		// puffs every spacing_m along the path (and at least ten a second, should the point hang nearly still)
		double Done = Row.Spacing - Tr->Owed;
		int Laid = 0;
		for (; Done <= Long && Laid < 64; Done += Row.Spacing, ++Laid)
		{
			const double K = Long > 1e-9 ? Done / Long : 1.0;
			const double T = Tr->Last + Between * K;
			Tr->LastPuffAt = Tr->At + Path * K + TipBack;
			TrailPuff(T, Tr->LastPuffAt, Tr->Carry + (Carry - Tr->Carry) * K, *Tr, Row, D);
			Tr->LastPuff = T;
		}
		Tr->Owed = std::fmod(Tr->Owed + Long, Row.Spacing);
		if (Laid == 0 && Now - Tr->LastPuff > 0.1)
		{
			Tr->LastPuffAt = At + TipBack;
			TrailPuff(Now, Tr->LastPuffAt, Carry, *Tr, Row, D);
			Tr->LastPuff = Now;
			Tr->Owed = 0.0;
		}
		// fire (art director 21:27Z): ragged tongues that tear free of the cowl and stream aft, a pale yellow heart at
		// the cowl and deep orange along the tongues, a new length and shape every frame; the standing flame rule,
		// never soft balls merged into a smear
		if (Row.Flame > 0.0)
		{
			const Rgb HeartCol = Rgb::FromPicture(252, 228, 160);
			const Rgb DeepCol = Rgb::FromPicture(196, 86, 30);
			Tr->NextFlame = std::max(Tr->NextFlame, Now - 0.1);
			for (int Guard = 0; Tr->NextFlame <= Now && Guard < 8; ++Guard)
			{
				const double T = Tr->NextFlame;
				const double K = Between > 1e-9 ? Clamp01((T - Tr->Last) / Between) : 1.0;
				const Vec Root = Tr->At + Path * K;
				const Vec Vel = Tr->Carry + (Carry - Tr->Carry) * K;
				const Vec Back = -Vel.Unit(Vec(1.0, 0.0, 0.0));
				const Vec Side = Back.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
				const Vec Lift = Side.Cross(Back).Unit(ZUp);
				// this frame's tongue: its own length, its own curl sideways and lift
				const double Len = Row.Flame * (0.45 + 0.55 * Grown) * D.U(0.55, 1.35);
				const Vec Curl = (Side * (0.2 * D.Bell()) + Lift * D.U(-0.05, 0.1)) * Len;
				// (art director 21:37Z: fire in a slipstream stands off the skin and spills past the silhouette, so each
				// tongue lifts above or below as it streams aft; the heart stays on the cowl)
				const Vec Spill = Lift * ((D.U() < 0.5 ? 1.0 : -1.0) * Row.FlameLift.Pick(D));
				const int Pieces = 4;
				for (int I = 0; I < Pieces; ++I)
				{
					const double At_ = (I + 0.5) / Pieces; // (0 at the cowl to 1 at the tip)
					const Vec Ragged = (Side * D.Bell() + Lift * D.Bell()) * (Len * 0.035 * (1.0 + 2.0 * At_));
					const Vec Way = (Back * Len + Curl * (2.0 * At_)).Unit(Back);
					Bit B;
					B.Born = T;
					B.From = Root + Back * (Len * At_) + Curl * (At_ * At_) + Spill * std::min(1.0, 1.4 * At_) + Ragged;
					B.Vel = Vel + Back * (Len * D.U(3.0, 6.0)); // (streaming aft off the cowl)
					B.Life = D.U(0.045, 0.075);
					B.R0 = Len * (0.12 - 0.065 * At_) * D.U(0.85, 1.15);
					B.R1 = B.R0 * 0.6; // (eaten away as it goes)
					B.Grow = 1.0;
					B.Along = Way;
					B.Stretch = D.U(2.0, 2.8) + At_;
					B.Point = 0.6;
					B.Tail = 0.5;
					B.Col = At_ < 0.5 ? HeartCol.Mix(FlameCol, 0.35 + 1.3 * At_) : FlameCol.Mix(DeepCol, (At_ - 0.5) * 2.0);
					B.Col2 = DeepCol * 0.7;
					B.Alpha = 0.92 - 0.2 * At_;
					B.FadeIn = 0.002;
					B.Hold = 0.45;
					B.Heart = 0.85 - 0.2 * At_;
					B.Rim = 0.1;
					B.Rough = 0.45;
					B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
					B.Turn = D.U(0.0, 2.0 * Pi);
					B.bLight = true;
					Bits.push_back(B);
				}
				// now and then a lick tears free of the tip and streams away aft on its own, dimming
				if (D.U() < 0.4)
				{
					Bit B;
					B.Born = T;
					B.From = Root + Back * (Len * D.U(0.85, 1.1)) + Curl * 0.8 + Spill;
					B.Vel = Vel + Back * (Len * D.U(8.0, 12.0));
					B.Life = D.U(0.07, 0.11);
					B.R0 = Len * D.U(0.045, 0.065);
					B.R1 = B.R0 * 0.4;
					B.Grow = 1.0;
					B.Along = Back;
					B.Stretch = D.U(2.2, 3.0);
					B.Point = 0.6;
					B.Tail = 0.6;
					B.Col = FlameCol.Mix(DeepCol, 0.5);
					B.Col2 = DeepCol * 0.7;
					B.Alpha = 0.9;
					B.FadeIn = 0.002;
					B.Hold = 0.3;
					B.Heart = 0.7;
					B.Rim = 0.1;
					B.Rough = 0.4;
					B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
					B.Turn = D.U(0.0, 2.0 * Pi);
					B.bLight = true;
					Bits.push_back(B);
				}
				// the heart: small and pale yellow, hugging the cowl
				{
					Bit B;
					B.Born = T;
					B.From = Root + Back * (Len * 0.06);
					B.Vel = Vel;
					B.Life = 0.05;
					B.R0 = Row.Flame * (0.45 + 0.55 * Grown) * 0.085 * D.U(0.85, 1.15);
					B.R1 = B.R0;
					B.Along = Back;
					B.Stretch = 1.6;
					B.Point = 0.4;
					B.Tail = 0.3;
					B.Col = HeartCol;
					B.Col2 = HeartCol.Mix(FlameCol, 0.5);
					B.Alpha = 1.0;
					B.FadeIn = 0.002;
					B.Hold = 0.5;
					B.Heart = 0.95;
					B.Rim = 0.3;
					B.Rough = 0.2;
					B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
					B.Turn = D.U(0.0, 2.0 * Pi);
					B.bLight = true;
					Bits.push_back(B);
				}
				Tr->NextFlame += 1.0 / 60.0;
			}
		}
		Tr->Last = Now;
		Tr->At = At;
		Tr->Carry = Carry;
	}

	void Battle::TrailStop(int Id)
	{
		for (Trailing& Tr : Trails)
		{
			if (Tr.Id == Id)
			{
				Tr.bOn = false;
			}
		}
	}

	void Battle::Burn(double Now, const Vec& At, double Across, double Seconds)
	{
		Source S;
		S.At = At;
		S.Kind = 0;
		S.Start = Now;
		S.Next = Now;
		S.NextSmoke = Now;
		S.Flames = Now + std::max(0.0, Seconds);
		S.End = S.Flames + Num("fire.column_s", 600.0);
		S.Across = std::max(0.3, Across);
		S.Seed = ++Seeds * 49979687ull ^ PlaceKey(At);
		Sources.push_back(S);
	}

	void Battle::Run(double Now)
	{
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Rgb Heart = Rgb::FromPicture(250, 196, 110);
		const Rgb Ember = Rgb::FromPicture(110, 35, 15);
		for (Source& S : Sources)
		{
			// (after a long pause, such as the game being paused, only the last second is made up)
			S.Next = std::max(S.Next, Now - 1.0);
			S.NextSmoke = std::max(S.NextSmoke, Now - 1.0);
			if (S.Kind == 0)
			{
				// long ragged tongues rooted where it burns, dying by being eaten away; then a thinning grey column
				while (S.Next <= Now && S.Next < S.Flames)
				{
					Dice D(S.Seed ^ static_cast<uint64_t>(S.Next * 1000.0));
					const int N = D.Whole(1, 2);
					for (int I = 0; I < N; ++I)
					{
						const Vec Base = S.At + Vec(D.U(-0.5, 0.5), D.U(-0.5, 0.5), 0.0) * S.Across;
						const bool bShort = D.U() < 0.2;
						Bit B;
						B.Born = S.Next;
						B.From = Base + ZUp * (0.2 * S.Across);
						B.Vel = ZUp * D.U(1.5, 3.5);
						B.Drag = 0.8;
						B.Windage = 0.3;
						B.Life = bShort ? D.U(0.15, 0.25) : D.U(0.3, 0.6);
						B.R0 = S.Across * D.U(0.15, 0.25) * (bShort ? 0.6 : 1.0);
						B.R1 = S.Across * 0.08;
						B.Grow = 1.0;
						B.Along = ZUp;
						B.Stretch = D.U(2.0, 3.0);
						B.Point = 0.9;
						B.Col = bShort ? Heart : Flame;
						B.Col2 = Ember;
						B.Alpha = 0.95;
						B.FadeIn = 0.03;
						B.Hold = 0.3;
						B.Heart = 0.75;
						B.Rim = 0.1;
						B.bLight = true;
						Bits.push_back(B);
					}
					S.Next += 0.05;
				}
				while (S.NextSmoke <= Now && S.NextSmoke < S.End)
				{
					Dice D(S.Seed ^ (static_cast<uint64_t>(S.NextSmoke * 1000.0) + 17ull));
					const bool bFlames = S.NextSmoke < S.Flames;
					const double After = bFlames ? 0.0 : (S.NextSmoke - S.Flames) / std::max(S.End - S.Flames, 1.0);
					Bit B;
					B.Born = S.NextSmoke;
					B.From = S.At + Vec(D.U(-0.3, 0.3), D.U(-0.3, 0.3), 0.5) * S.Across;
					B.Vel = Vec(D.Bell(), D.Bell(), 0.0) * 0.3;
					B.Rise = D.U(1.8, 3.0);
					B.Drag = 0.3;
					B.Windage = 1.0;
					B.Life = D.U(8.0, 12.0);
					B.R0 = 0.3 * S.Across;
					B.R1 = D.U(2.0, 4.0) * std::max(1.0, S.Across);
					B.Col = bFlames ? Rgb::FromPicture(50, 46, 42) : Rgb::FromPicture(110, 108, 104);
					B.Col2 = B.Col.Mix(Rgb::FromPicture(120, 118, 114), 0.5);
					B.Alpha = 0.5 * std::max(0.15, 1.0 - After);
					B.FadeIn = 0.3;
					B.Hold = 0.1;
					B.Heart = 0.45;
					Bits.push_back(B);
					S.NextSmoke += bFlames ? 0.12 : 0.35;
				}
			}
			else
			{
				while (S.NextSmoke <= Now && S.NextSmoke < S.End)
				{
					Dice D(S.Seed ^ static_cast<uint64_t>(S.NextSmoke * 1000.0));
					Bit B;
					B.Born = S.NextSmoke;
					B.From = S.At + Vec(D.U(-0.3, 0.3), D.U(-0.3, 0.3), 0.1) * S.Across;
					B.Rise = 0.8;
					B.Windage = 1.0;
					B.Life = D.U(4.0, 6.0);
					B.R0 = 0.3;
					B.R1 = 1.5 * std::max(0.6, S.Across * 0.5);
					B.Col = Rgb::FromPicture(96, 88, 78);
					B.Col2 = B.Col;
					B.Alpha = 0.2 * (1.0 - (S.NextSmoke - S.Start) / std::max(S.End - S.Start, 0.1));
					B.FadeIn = 0.4;
					B.Hold = 0.1;
					B.Heart = 0.45;
					Bits.push_back(B);
					S.NextSmoke += 0.5;
				}
			}
		}
	}

	void Battle::Prune(double Now)
	{
		auto Gone = [Now](const Bit& B) { return Now > B.Born + B.Life && (!Unseen(B) || Now > B.Born + 0.5); };
		Bits.erase(std::remove_if(Bits.begin(), Bits.end(), Gone), Bits.end());
		// (a flash goes once its lamp has had its two frames, or after half a second unseen)
		Flashes.erase(std::remove_if(Flashes.begin(), Flashes.end(), [this, Now](const Flash& F) { return (F.FirstFrame >= 0 && FrameNo - F.FirstFrame >= 2) || Now > F.Born + 0.5; }), Flashes.end());
		Firings.erase(std::remove_if(Firings.begin(), Firings.end(), [Now](const Firing& F) { return Now - F.Last > 30.0; }), Firings.end());
		Trails.erase(std::remove_if(Trails.begin(), Trails.end(), [Now](const Trailing& T) { return Now - T.Last > 30.0; }), Trails.end()); // (stopped, or left on and never moved again)
		Knocks.erase(std::remove_if(Knocks.begin(), Knocks.end(), [Now](const Knock& J) { return Now > J.Born + 1.0; }), Knocks.end());
		Sources.erase(std::remove_if(Sources.begin(), Sources.end(), [Now](const Source& S) { return Now > S.End; }), Sources.end());
	}

	bool Battle::Busy(double Now) const
	{
		(void)Now;
		return !Bits.empty() || !Marks.empty() || !Sources.empty() || !Flashes.empty();
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Drawing

	void Battle::Lay(int Slot, const Vec& Middle, const Vec& AxisA, const Vec& AxisB, const Rgb& Col, double Alpha, double HeartAlpha, double RimAlpha, double Inner, double Point, double Tail, double Shade, const View& Eye, double Rough, uint32_t LumpSeed, double Turn)
	{
		const size_t First = static_cast<size_t>(Slot) * PointsPerPuff;
		auto Put = [this](size_t I, const Vec& At, const Rgb& C, double A)
		{
			Out[I] = At;
			OutRgba[I * 4 + 0] = Byte(C.R);
			OutRgba[I * 4 + 1] = Byte(C.G);
			OutRgba[I * 4 + 2] = Byte(C.B);
			OutRgba[I * 4 + 3] = Byte(A);
		};
		Put(First, Middle, Col, Alpha);
		// (a clod's lumpy outline: a few waves round it, its own, turning as it tumbles. Each wave is
		// cos(n * (Angle - Turn) + Pn), worked out from the ring's own table, so a puff costs six cosines, not three a side)
		double C2 = 1.0, S2 = 0.0, C3 = 1.0, S3 = 0.0, C5 = 1.0, S5 = 0.0;
		if (Rough > 0.0)
		{
			const double P2 = (LumpSeed & 1023u) * (2.0 * Pi / 1024.0);
			const double P3 = ((LumpSeed >> 10) & 1023u) * (2.0 * Pi / 1024.0);
			const double P5 = ((LumpSeed >> 20) & 1023u) * (2.0 * Pi / 1024.0);
			C2 = std::cos(P2 - 2.0 * Turn);
			S2 = std::sin(P2 - 2.0 * Turn);
			C3 = std::cos(P3 - 3.0 * Turn);
			S3 = std::sin(P3 - 3.0 * Turn);
			C5 = std::cos(P5 - 5.0 * Turn);
			S5 = std::sin(P5 - 5.0 * Turn);
		}
		// (unlit puffs: lighter on the side toward the sun, darker away from it, so a cloud has a lit side)
		const double SunA = AxisA.Dot(Eye.Sun), SunB = AxisB.Dot(Eye.Sun);
		const double AA = AxisA.Dot(AxisA), BB = AxisB.Dot(AxisB), AB = AxisA.Dot(AxisB);
		const double SunUp = ZUp.Dot(Eye.Sun);
		const double In = std::clamp(Inner, 0.1, 0.95);
		for (int K = 0; K < Sides; ++K)
		{
			const size_t I = static_cast<size_t>(K);
			const double Cs = RingCos[I], Sn = RingSin[I];
			const Vec Dir = AxisA * Cs + AxisB * Sn;
			double Reach = 1.0;
			if (K == 0)
			{
				Reach += Point;
			}
			else if (K == Sides / 2)
			{
				Reach += Tail;
			}
			if (Rough > 0.0)
			{
				const double Wave2 = Ring2Cos[I] * C2 - Ring2Sin[I] * S2;
				const double Wave3 = Ring3Cos[I] * C3 - Ring3Sin[I] * S3;
				const double Wave5 = Ring5Cos[I] * C5 - Ring5Sin[I] * S5;
				Reach *= std::max(0.35, 1.0 + Rough * (0.5 * Wave2 + 0.35 * Wave3 + 0.25 * Wave5));
			}
			Rgb Shaded = Col;
			Rgb HeartShaded = Col;
			if (Shade > 0.0)
			{
				const double Len2 = AA * Cs * Cs + BB * Sn * Sn + 2.0 * AB * Cs * Sn;
				const double Toward = Len2 > 1e-18 ? (SunA * Cs + SunB * Sn) / std::sqrt(Len2) : SunUp;
				const double F = std::max(0.2, 1.0 + Shade * (ShadeLit * std::max(0.0, Toward) - ShadeDark * std::max(0.0, -Toward)));
				Shaded = Col * F;
				HeartShaded = Col * (1.0 + 0.5 * (F - 1.0));
			}
			Put(First + 1 + I, Middle + Dir * (In * Reach), HeartShaded, HeartAlpha);
			Put(First + 1 + static_cast<size_t>(Sides) + I, Middle + Dir * Reach, Shaded, RimAlpha);
		}
	}

	bool Battle::LayBit(int Slot, const Bit& B, double Now, const View& Eye)
	{
		const double Age = Now - B.Born;
		if (Age < 0.0 || (Age > B.Life && !Unseen(B)))
		{
			return false;
		}
		// (a light shorter than a frame that no frame has caught yet is laid now, where it is, at its full strength)
		const double Shown = Unseen(B) && Age > B.Life * std::min(B.Hold, 0.5) ? B.Life * std::min(B.Hold, 0.5) : Age;
		const double F = Shown / B.Life;
		double Rad = B.R0 + (B.R1 - B.R0) * std::pow(F, B.Grow);
		const double Fade = std::min(1.0, Shown / std::max(B.FadeIn, 1e-3)) * (F < B.Hold ? 1.0 : std::pow(std::max(0.0, 1.0 - (F - B.Hold) / std::max(1.0 - B.Hold, 1e-6)), 1.4));
		double A = B.Alpha * Fade;
		if (B.HazeBy > 0.0)
		{
			const double T = Clamp01(Age / B.HazeBy);
			A *= 1.0 + (B.HazeAlpha / std::max(B.Alpha, 1e-6) - 1.0) * T;
		}
		if (A < 0.004)
		{
			return false;
		}
		Rgb Col = B.Col.Mix(B.Col2, F * B.Cool);
		if (!B.bLight)
		{
			Col = Rgb(Col.R * Eye.SmokeLight.R, Col.G * Eye.SmokeLight.G, Col.B * Eye.SmokeLight.B);
		}
		const Vec P = Where(B, Age);
		const double Dist = (P - Eye.Eye).Length();
		if (Dist > Beyond)
		{
			return false;
		}
		if (B.CloudShade > 0.0 && !B.bLight)
		{
			// (the cloud's sunward side lit, its far side in its own shadow: shading each puff alone barely shows
			// once many overlap)
			const Vec FromMid = P - (B.Centre + Wind * (0.75 * Age) + ZUp * (B.CentreRise * Age));
			const double Len = FromMid.Length();
			if (Len > 1e-3)
			{
				const double Toward = FromMid.Dot(Eye.Sun) / Len * Clamp01(Len / std::max(B.CloudR, 1e-3));
				Col = Col * std::max(0.3, 1.0 + B.CloudShade * (ShadeLit * std::max(0.0, Toward) - ShadeDark * std::max(0.0, -Toward)));
			}
		}
		const double MetresAPixel = Dist / std::max(Eye.PixelsPerRadian, 1.0);
		if (B.Form == Bit::Shape::Puff && B.LeastPx > 0.0)
		{
			// (a puff never shrinks on the screen below LeastPx across: far off it is held at that size and made thinner
			// to match, so a far burst stays see-through smoke and never a solid dot)
			const double Least = 0.5 * B.LeastPx * MetresAPixel;
			if (Rad < Least)
			{
				A *= std::sqrt(Rad / Least);
				Rad = Least;
			}
		}
		Vec AxA = Eye.Right * Rad, AxB = Eye.Up * Rad;
		Vec Middle = P;
		switch (B.Form)
		{
		case Bit::Shape::Puff:
			if (B.Smear > 0.0 && B.Along.Length() > 0.5)
			{
				// (a trail's puff: a length added behind it along the path on top of its round, so young thin puffs join
				// up; the length shrinks as it ages, so an old trail breaks into lumps)
				const Vec OnScreen = B.Along - Eye.Forward * B.Along.Dot(Eye.Forward);
				const double L = OnScreen.Length();
				if (L > 1e-3)
				{
					const double Smear = B.SmearFor > 0.0 ? B.Smear * std::max(0.3, 1.0 - Age / B.SmearFor) : B.Smear;
					const Vec Ua = OnScreen * (1.0 / L);
					AxA = Ua * (Rad + Smear * 0.5 * L);
					AxB = Eye.Forward.Cross(Ua).Unit(Eye.Up) * Rad;
					Middle = P - Ua * (Smear * 0.5 * L);
				}
			}
			else if (B.Stretch > 1.0001 && B.Along.Length() > 0.5)
			{
				const Vec OnScreen = B.Along - Eye.Forward * B.Along.Dot(Eye.Forward);
				const double L = OnScreen.Length();
				if (L > 1e-3)
				{
					const Vec Ua = OnScreen * (1.0 / L);
					AxA = Ua * (Rad * (1.0 + (B.Stretch - 1.0) * L)); // (shorter when it points at the eye)
					AxB = Eye.Forward.Cross(Ua).Unit(Eye.Up) * Rad;
				}
			}
			break;
		case Bit::Shape::Upright:
		{
			// A low flattened ball on the ground, drawn as its outline from where the eye is: from the side a low wall
			// (as high as Flatten), from above the whole round it covers, so a skirt is one haze from any height,
			// never a ring of separate dabs.
			const Vec Flat_ = Vec(Eye.Right.X, Eye.Right.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
			const Vec Ahead = Vec(-Flat_.Y, Flat_.X, 0.0); // (level, square to Flat_)
			double Wide = 1.0, Deep = 1.0;
			if (B.Stretch > 1.0001)
			{
				const Vec Al = Vec(B.Along.X, B.Along.Y, 0.0).Unit(Flat_);
				Wide += (B.Stretch - 1.0) * std::fabs(Al.Dot(Flat_));
				Deep += (B.Stretch - 1.0) * std::fabs(Al.Dot(Ahead));
			}
			const double Down = Clamp01(-Eye.Forward.Z); // (the sine of how steeply the eye looks down)
			const double Across = Rad * Deep; // (its reach on the ground away from the eye: from above, its height on the screen)
			const double High = Rad * B.Flatten;
			const double Tall = std::sqrt(Across * Across * Down * Down + High * High * (1.0 - Down * Down));
			const Vec Upward = Flat_.Cross(Eye.Forward).Unit(ZUp);
			AxA = Flat_ * (Rad * Wide);
			AxB = (Upward.Z < 0.0 ? -Upward : Upward) * Tall;
			Middle = P + ZUp * (High * 0.8);
			break;
		}
		case Bit::Shape::Flat:
			AxA = Vec(std::cos(B.Spin), std::sin(B.Spin), 0.0) * (Rad * B.Stretch);
			AxB = Vec(-std::sin(B.Spin), std::cos(B.Spin), 0.0) * Rad;
			break;
		case Bit::Shape::Streak:
		{
			// (a tracer keeps its own heading, but an air gun's, slowing and falling over a long flight, bends with it)
			const Vec Way = B.HalfLength > 0.0 ? (B.Gravity > 0.0 ? Going(B, Age).Unit(B.Along) : B.Along) : Going(B, Age);
			const double Speed = Way.Length();
			const Vec OnScreen = Way - Eye.Forward * Way.Dot(Eye.Forward);
			const double L = OnScreen.Length();
			const Vec Ua = L > 1e-6 ? OnScreen * (1.0 / L) : Eye.Right;
			const Vec Ub = Eye.Forward.Cross(Ua).Unit(Eye.Up);
			const double Share = Speed > 1e-9 ? L / Speed : 0.0;
			double Half = B.HalfLength > 0.0 ? B.HalfLength * Share : Rad + L / 60.0; // (a clod is seen as a streak along its flight)
			double Width = Rad;
			const double Least = B.LeastPx * MetresAPixel * 0.5;
			if (Width < Least)
			{
				if (!B.bLight)
				{
					A *= Width / Least;
				}
				Width = Least;
			}
			if (B.Glow > 0)
			{
				// (by day a tracer widens and its core whitens, so a stream holds on a pale sky; by dusk it is as ruled.
				// Day is read from the hour's smoke light: white by day, dim at dusk)
				const double Day = DayOf(Eye.SmokeLight);
				Width *= 1.0 + (TracerDayWide - 1.0) * Day;
				if (B.Glow == 2)
				{
					Col = Col.Mix(TracerDayCore, Day);
				}
			}
			Half = std::max(Half, Width);
			AxA = Ua * Half;
			AxB = Ub * Width;
			break;
		}
		}
		if (A < 0.004)
		{
			return false;
		}
		const double Heart = B.HeartEnd >= 0.0 ? B.Heart + (B.HeartEnd - B.Heart) * F : B.Heart;
		Lay(Slot, Middle, AxA, AxB, Col, A, A * Heart, A * B.Rim, B.Inner, B.Point, B.Tail, B.bLight ? 0.0 : B.ShadeK, Eye, B.Rough, B.LumpSeed, B.Turn + B.TurnRate * Age);
		B.bShown = true;
		return true;
	}

	int Battle::Draw(double Now, const View& Eye)
	{
		LastEye = Eye.Eye;
		bEyeKnown = true;
		Run(Now);
		Prune(Now);
		++FrameNo;
		for (Flash& F : Flashes)
		{
			if (F.FirstFrame < 0 && Now > F.Born)
			{
				F.FirstFrame = FrameNo; // (strictly after its birth: a flash born at this very time shows only next frame)
			}
		}
		int Slot = 0;
		// the craters first: they lie on the ground, under everything else
		for (const Bit& M : Marks)
		{
			if (Slot >= MaxPuffs)
			{
				break;
			}
			if (LayBit(Slot, M, Now, Eye))
			{
				++Slot;
			}
		}
		// the young end of each trail this frame: from its point (or its flame's tips) back to its newest hanging puff,
		// thin and dark, riding with the aircraft, so a trail starts at the engine and not a puff's spacing behind it
		Roots.clear();
		for (const Trailing& Tr : Trails)
		{
			if (!Tr.bOn || Now < Tr.Last || Now - Tr.Last > 0.25)
			{
				continue;
			}
			const TrailRow& Row = TrailRowOf(Tr.Kind);
			const Vec Way = Tr.Carry.Unit(Vec(1.0, 0.0, 0.0));
			const Vec Head = Tr.At + Tr.Carry * (Now - Tr.Last) - Way * Tr.Tip;
			const double Gap = std::min((Head - Tr.LastPuffAt).Dot(Way), 2.0 * Row.Spacing);
			if (Gap <= 0.05)
			{
				continue;
			}
			const double Ramp = Clamp01((Now - Tr.KindBorn) / Row.RampS);
			Bit B;
			B.Born = Now - 0.001;
			B.Life = 1.0;
			B.From = Head;
			B.Along = Way;
			B.Smear = Gap;
			B.R0 = B.R1 = Row.R0 * (0.7 + 0.3 * Ramp);
			B.Col = Row.Col.Mix(Row.ColFull, Clamp01((Now - Tr.KindBorn) / Row.DarkenS));
			B.Col2 = B.Col;
			B.Alpha = Row.Alpha * (0.45 + 0.55 * Ramp);
			B.FadeIn = 0.0005;
			B.Hold = 1.0;
			B.Heart = 0.7;
			B.Rough = 0.25 + 0.2 * Row.Lumpy;
			B.LumpSeed = static_cast<uint32_t>(Tr.Id) * 2654435761u;
			Roots.push_back(B);
		}
		// then the rest, far to near, so that each is laid over what is behind it
		Sorted.clear();
		for (const Bit& B : Bits)
		{
			if (Now < B.Born || (Now > B.Born + B.Life && !Unseen(B)))
			{
				continue;
			}
			Sorted.push_back(Order{ &B, (Where(B, Now - B.Born) - Eye.Eye).Dot(Eye.Forward) });
		}
		for (const Bit& B : Roots)
		{
			Sorted.push_back(Order{ &B, (B.From - Eye.Eye).Dot(Eye.Forward) });
		}
		std::sort(Sorted.begin(), Sorted.end(), [](const Order& L, const Order& R) { return L.Deep > R.Deep; });
		const size_t Left = static_cast<size_t>(MaxPuffs - Slot);
		const size_t Start = Sorted.size() > Left ? Sorted.size() - Left : 0; // (out of room: the farthest go)
		for (size_t I = Start; I < Sorted.size() && Slot < MaxPuffs; ++I)
		{
			if (LayBit(Slot, *Sorted[I].B, Now, Eye))
			{
				++Slot;
			}
		}
		// fold away what was drawn last frame and is not now
		for (int S = Slot; S < LastDrawn; ++S)
		{
			const size_t First = static_cast<size_t>(S) * PointsPerPuff;
			for (size_t I = 0; I < static_cast<size_t>(PointsPerPuff); ++I)
			{
				Out[First + I] = Out[First];
				OutRgba[(First + I) * 4 + 3] = 0;
			}
		}
		LastDrawn = Slot;
		return Slot;
	}

	double Battle::DayOf(const Rgb& HourLight) const
	{
		const double Lum = 0.2126 * HourLight.R + 0.7152 * HourLight.G + 0.0722 * HourLight.B;
		return Clamp01((Lum - DayFrom) / (DayFull - DayFrom));
	}

	int Battle::Lamps(double Now, Lamp* Into, int Room, double Day) const
	{
		// Art director 21:16Z: never before its flash; full on the flash's frame, about half the next, then out. By day
		// a lamp is a quarter as strong and warm white, and small guns light no ground at all.
		Day = Clamp01(Day);
		const double ByDay = 1.0 + (DayLampShare - 1.0) * Day;
		std::vector<Lamp> All;
		for (const Flash& F : Flashes)
		{
			if (F.FirstFrame < 0 || Now < F.Born)
			{
				continue;
			}
			const int Frames = FrameNo - F.FirstFrame;
			if (Frames < 0 || Frames > 1)
			{
				continue;
			}
			const double Strength = F.Strength * (Frames == 0 ? 1.0 : 0.5) * (F.bNightOnly ? 1.0 - Day : ByDay);
			if (Strength <= 1e-4)
			{
				continue;
			}
			Lamp L;
			L.At = F.At;
			L.Col = F.Col.Mix(DayLampCol, Day);
			L.Strength = Strength;
			L.Radius = F.Radius;
			All.push_back(L);
		}
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		for (const Source& S : Sources)
		{
			if (S.Kind != 0 || Now < S.Start || Now > S.Flames)
			{
				continue;
			}
			const double Seed = static_cast<double>(S.Seed % 1000ull);
			Lamp L;
			L.At = S.At + ZUp * (S.Across * 0.6);
			L.Col = Flame;
			L.Strength = ByDay * 0.25 * std::sqrt(S.Across) * (0.75 + 0.25 * std::sin(Now * 23.0 + Seed) * std::sin(Now * 7.3 + Seed * 0.5));
			L.Col = Flame.Mix(DayLampCol, Day);
			L.Radius = 8.0 * S.Across;
			All.push_back(L);
		}
		std::sort(All.begin(), All.end(), [](const Lamp& L, const Lamp& R) { return L.Strength > R.Strength; });
		const int N = std::min(Room, static_cast<int>(All.size()));
		for (int I = 0; I < N; ++I)
		{
			Into[I] = All[static_cast<size_t>(I)];
		}
		return N;
	}

	Battle::Jolt Battle::Shake(double Now, const Vec& Eye) const
	{
		// A short jolt, mostly up and down, half gone in 0.08 s and over in half a second; the roll is under half a
		// degree; hits beyond their size's reach never shake it; big bombs add a slight push away from the blast.
		Jolt J;
		const double Side = Num("camera.shake.side_m_at_full", 0.12);
		const double UpDown = Num("camera.shake.up_down_m_at_full", 0.35);
		const double MostRoll = Num("camera.shake.roll_deg_max", 0.5);
		int Index = 0;
		for (const Knock& Kn : Knocks)
		{
			++Index;
			const double A = Now - Kn.Born;
			if (A < 0.0 || A > 0.6)
			{
				continue;
			}
			const double Dist = (Eye - Kn.At).Length();
			if (Dist >= Kn.Within)
			{
				continue;
			}
			const double S = Kn.Strength * std::pow(1.0 - Dist / Kn.Within, 2.0);
			const double Env = std::exp(-A / 0.11) * std::min(1.0, A / 0.012);
			const double W = 2.0 * Pi * 11.0;
			J.Offset += Vec(std::sin(W * A * 0.83 + Index) * Side, std::sin(W * A * 0.71 + 2 * Index) * Side * 0.6, std::sin(W * A + 0.6 + Index) * UpDown) * (S * Env);
			J.Roll += std::sin(W * A * 0.9 + Index) * MostRoll * S * Env;
			if (Kn.Push > 0.0)
			{
				J.Offset += (Eye - Kn.At).Unit() * (Kn.Push * (1.0 - Dist / Kn.Within) * (1.0 - std::exp(-A / 0.05)) * std::exp(-A / 0.4));
			}
		}
		J.Roll = std::max(-MostRoll, std::min(MostRoll, J.Roll));
		return J;
	}
}
