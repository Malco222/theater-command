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
		Sides = std::max(6, std::min(24, static_cast<int>(Num("game.puff_sides", 16.0))));
		PointsPerPuff = 2 * Sides + 1;
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
		const std::string C = "burst.debris.clods.";
		B.Rough = Pick(D, C + "rough", 0.25, 0.45);
		B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
		B.Turn = D.U(0.0, 2.0 * Pi);
		B.TurnRate = Pick(D, C + "tumble_turns_a_second", 0.5, 2.0) * 2.0 * Pi * (D.U() < 0.5 ? -1.0 : 1.0);
		B.ShadeK = Num(C + "shade", 2.5);
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
			Crown(Now + 0.005, At, CrownH, K, Slow, P, ClodK, D);
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
		for (int I = 0; I < NOut + NIn; ++I)
		{
			const bool bFront = I < NOut;
			const int N = bFront ? NOut : NIn;
			const int Place = bFront ? I : I - NOut;
			const double Speed = bFront ? 1.0 : std::sqrt(D.U(0.02, 0.85)); // (the inner ones fill back toward the hole, evenly over its round: one skirt, not a ring)
			const double A = 2.0 * Pi * (Place + D.U(-0.4, 0.4)) / std::max(N, 1);
			const Vec Outward(std::cos(A), std::sin(A), 0.0);
			const double Drag = Num(Sk + "drag", 8.0) * D.U(0.85, 1.15);
			Bit B;
			B.Form = Bit::Shape::Upright;
			B.bHug = true;
			B.Born = T0 + D.U(0.0, 0.02);
			B.From = At + Outward * (D.U(0.4, 1.0) * K);
			B.Vel = Outward * (Reach * Speed * Drag * D.U(0.85, 1.0));
			B.Drag = Drag;
			B.Rise = Num(Sk + "rise_mps", 0.5) * D.U(0.6, 1.4);
			B.Windage = 0.6;
			B.Life = Pick(D, Sk + "life_s", 2.5, 3.5) * std::min(2.0, K);
			B.R0 = Num(Sk + "start_radius_m", 0.5) * K;
			B.R1 = Num(Sk + "end_radius_m", 1.5) * K * D.U(0.7, 1.3) * (bFront ? 1.0 : Num(Sk + "inner_radius_k", 1.0)); // (fewer, broader inner ones fill the round)
			B.Grow = Num(Sk + "grow", 0.25);
			B.Stretch = Num(Sk + "stretch_along_ring", 1.7);
			B.Along = Vec(-std::sin(A), std::cos(A), 0.0);
			B.Flatten = Num(Sk + "flatten", 0.75) * D.U(0.8, 1.2) * (bFront ? Num(Sk + "front_billow", 1.35) : 1.0); // (the front edge stands tallest)
			B.Col = Col * D.U(0.9, 1.08);
			B.Col2 = B.Col;
			B.Alpha = std::min(0.9, Pick(D, Sk + "alpha", 0.45, 0.6) * std::pow(DustK, 0.4));
			B.FadeIn = 0.02;
			B.Hold = 0.1;
			B.HazeBy = Num(Sk + "haze_by_s", 1.0);
			B.HazeAlpha = std::min(B.Alpha, HazeAlpha);
			B.ShadeK = Num(Sk + "shade", 1.8); // (a lit top and a shadowed foot, so it shows its height)
			B.Heart = Num(Sk + "heart", 0.55);
			B.Rim = 0.0;
			Bits.push_back(B);
		}
	}

	void Battle::Crown(double T0, const Vec& At, double CrownH, double K, double Slow, const Paint& P, double ClodK, Dice& D)
	{
		// The crown: a wide torn fan. A few heavy dark jets, very uneven in length and width, bunched and overlapping;
		// lighter dust filling the gaps so that the outline is one torn fan; each heavy jet thinnest and torn at its
		// tip, drawn to a point, with clods flying on beyond it. Then a dense dark body at the foot.
		const std::string C = "burst.crown.";
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
		const Vec Tilt = Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), 0.0).Unit(Vec(1.0, 0.0, 0.0)) * std::tan(Radians(Pick(D, C + "tilt_deg", 4.0, 14.0)));
		const int NHeavy = D.Whole(static_cast<int>(Num(C + "heavy_jets.0", 5.0)), static_cast<int>(Num(C + "heavy_jets.1", 8.0)));
		const int NFill = D.Whole(static_cast<int>(Num(C + "fill_jets.0", 10.0)), static_cast<int>(Num(C + "fill_jets.1", 16.0)));
		std::vector<Vec> Launch;
		std::vector<double> LaunchR;
		for (int J = 0; J < NHeavy + NFill; ++J)
		{
			const bool bHeavy = J < NHeavy;
			const double Az = D.U(0.0, 2.0 * Pi); // (at random round the compass: bunched and overlapping, never spaced)
			const double Lean = bHeavy ? (J == 0 ? D.U(2.0, 12.0) : Pick(D, C + "heavy_lean", 0.35, 1.0) * Half) : D.U(0.0, 1.0) * Half;
			const double ReachF = bHeavy ? (J == 0 ? 1.0 : Pick(D, C + "heavy_reach", 0.3, 1.0)) : Pick(D, C + "fill_reach", 0.3, 0.75);
			const Vec Dir = (Vec(std::cos(Az) * std::sin(Radians(Lean)), std::sin(Az) * std::sin(Radians(Lean)), std::cos(Radians(Lean))) + Tilt).Unit();
			const double ReachJ = CrownH * ReachF;
			const double V = ReachJ * KD * 1.12 / std::sqrt(Slow);
			const double Thick = bHeavy ? Pick(D, C + "heavy_thick", 0.7, 2.6) : Pick(D, C + "fill_thick", 1.6, 3.0);
			const double Rf = FootR * Thick, Rt = TipR * Thick;
			const double Shade = D.U(0.85, 1.15);
			const double FillAlpha = Pick(D, C + "fill_alpha", 0.3, 0.5);
			const double DustShare = D.U(0.35, 0.7);
			// (puffs stepped along the jet by their own length, overlapping, so that a jet of any size is one streak)
			double F = D.U(0.0, 0.05);
			while (F < 1.0)
			{
				const double Rwhole = Rt + (Rf - Rt) * std::pow(1.0 - F, 0.8);
				const double Rl = Rwhole * P.Grain; // (granular grounds: smaller pieces, more of them)
				const bool bTip = bHeavy && F >= TipAt;
				const double OnTip = bTip ? (F - TipAt) / std::max(1.0 - TipAt, 1e-6) : 0.0;
				const double Sp = V * (0.05 + 0.95 * std::pow(F, 0.9));
				const double Rr = Rl * (D.U() < Lumps ? D.U(1.4, 2.0) : D.U(0.8, 1.1));
				Rgb Col = bHeavy ? P.Heavy * Shade : P.Soil.Mix(P.Dust, DustShare) * Shade;
				const bool bGrained = P.Grain < 0.999;
				if (P.bFirst && D.U() < P.FirstAtFoot + (P.FirstAtTip - P.FirstAtFoot) * F)
				{
					Col = P.First * D.U(0.84, 1.04); // (snow: thrown up first, never two grains the same shade, earth showing through)
				}
				const Vec Side = Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), D.U(-1.0, 1.0)) * (Rwhole * (bTip ? 1.1 : Jitter) * KD * (bGrained ? 1.5 : 1.0)); // (ragged, clumpy sides; grains scattered wider, never a string of beads)
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
				B.Stretch = 1.0 + ((bTip ? Along0 + (TipStretch - Along0) * OnTip : Along0) - 1.0) * P.Grain; // (grains are not drawn out)
				B.Point = bTip ? TipPoint : 0.3; // (torn and pointed, never rounded)
				B.Tail = 0.15;
				B.Col = Col;
				B.Col2 = P.bFirst ? Col : P.Dust;
				B.Cool = 0.7;
				B.Alpha = (bHeavy ? (bTip ? 1.0 - (1.0 - TipAlpha) * OnTip : 1.0) : FillAlpha * (1.0 - 0.3 * F)) * (bGrained ? D.U(0.7, 1.0) : 1.0);
				B.FadeIn = 0.01;
				B.Hold = 0.4;
				B.Heart = bHeavy ? (bGrained ? 0.65 : 0.8) : 0.5;
				B.Rim = bHeavy && !bGrained ? 0.15 : 0.0; // (grains have soft edges, so no jet has a clean blade's side)
				B.Rough = bHeavy ? EdgeRough : EdgeRough * 0.6; // (lumpy outlines: clumpy sides with no more puffs)
				B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
				Bits.push_back(B);
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
		const std::string Bd = C + "body.";
		const int NBody = static_cast<int>(Num(Bd + "puffs", 50.0) * std::max(0.7, std::sqrt(K)));
		for (int I = 0; I < NBody; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			const double Out_ = CrownH * Num(Bd + "width", 0.3) * std::pow(D.U(), 0.6);
			const double Up_ = CrownH * Num(Bd + "height", 0.22) * std::pow(D.U(), 1.3);
			const Vec To = Vec(std::cos(A) * Out_, std::sin(A) * Out_, Up_) + Tilt * Up_;
			const double Rr = CrownH * Pick(D, Bd + "puff_r", 0.06, 0.11);
			const double KB = 5.0;
			Bit B;
			B.Born = T0 + D.U(0.03, Num(Bd + "from_frame", 3.0) / 30.0);
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
			B.Alpha = Pick(D, Bd + "alpha", 0.85, 0.95);
			B.FadeIn = 0.03;
			B.Hold = 0.45;
			B.Heart = 0.75;
			B.Rim = 0.1;
			Bits.push_back(B);
		}
		// the clods: those at the jets' tips, and more thrown clear low and wide
		const int NMore = static_cast<int>(ClodK * 50.0 * std::max(0.5, K * K));
		for (int I = 0; I < NMore; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			const double Out_ = D.U(1.0, 6.0) * K;
			const double Up_ = (2.0 + 9.0 * std::pow(D.U(), 2.0)) * K;
			Launch.push_back(Vec(std::cos(A) * Out_, std::sin(A) * Out_, Up_));
			const double Lo = Num("burst.debris.clod_r_m.0", 0.015), Hi = Num("burst.debris.clod_r_m.1", 0.09);
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
			B.Life = Linger * D.U(0.45, 1.0);
			B.R0 = Pick(D, Sm + "puff_r.0", 0.5, 0.9) * K * Var;
			B.R1 = Pick(D, Sm + "puff_r.1", 1.2, 2.6) * K * (1.0 + 0.5 * F) * Var;
			B.Col = (F < 0.25 && Born - T0 < 0.22) ? Base.Mix(Warm, 0.35) : Base; // (warm only at its foot, at first)
			B.Col2 = Base;
			B.Cool = B.Life / 0.15;
			B.Alpha = Pick(D, Sm + "alpha", 0.35, 0.6);
			B.Drag = D.U(1.4, 2.2);
			B.Rise = D.U(0.25, 0.9) * std::sqrt(K);
			B.FadeIn = 0.08;
			B.Grow = Num(Sm + "grow", 0.3);
			B.Windage = D.U(0.5, 1.0);
			B.Hold = 0.15;
			B.Heart = D.U(0.4, 0.7); // (some firmer than others: a torn cloud, not one soft egg)
			B.Rim = 0.0;
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

	void Battle::Shots(double Now, int Gun, const Vec& From, const Vec& To, const char* Weapon, int Rounds, double PerSecond)
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
		const double Half = Num("tracer.streak_m", 16.0) * 0.5;
		const double Life = Num("tracer.life_s", 0.6);
		const double Least = Num("tracer.least_px", 3.0);
		const Rgb Col = Picture("tracer.colour", Rgb::FromPicture(213, 199, 104));
		const Rgb Pale = Col.Mix(Rgb(1.0f, 1.0f, 1.0f), 0.55);
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Vec Line = To - From;
		const double Dist = Line.Length();
		const Vec Dir = Line.Unit(Vec(1.0, 0.0, 0.0));
		const bool bFlicker = Weapon && std::string(Weapon) == "mg";
		const double Gap = 1.0 / std::max(PerSecond, 0.1);
		auto Streak = [&](double Born, const Vec& Start, const Vec& Way, double Fast, double For)
		{
			for (int Layer = 0; Layer < 2; ++Layer)
			{
				Bit B;
				B.Form = Bit::Shape::Streak;
				B.Born = Born;
				B.From = Start + Way * Half;
				B.Vel = Way * Fast;
				B.Life = For;
				B.HalfLength = Half;
				B.Along = Way;
				B.R0 = B.R1 = Layer == 0 ? 0.045 : 0.018;
				B.LeastPx = Layer == 0 ? Least : Least * 0.4;
				B.Col = Layer == 0 ? Col : Pale; // (paler along its middle)
				B.Col2 = B.Col;
				B.Alpha = Layer == 0 ? 0.8 : 1.0;
				B.FadeIn = 0.001;
				B.Hold = 0.9;
				B.Heart = 0.9;
				B.Rim = Layer == 0 ? 0.2 : 0.5;
				B.Point = 0.3;
				B.Tail = 0.7; // (thinning toward the gun)
				B.bLight = true;
				Bits.push_back(B);
			}
		};
		for (int R = 0; R < Rounds; ++R)
		{
			const double T = Now + R * Gap;
			const int Which = Count++;
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
			const double Reach = Dist / std::max(Speed, 1.0);
			Streak(T, From, Aim, Speed, std::min(Life, Reach));
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
		auto Gone = [Now](const Bit& B) { return Now > B.Born + B.Life; };
		Bits.erase(std::remove_if(Bits.begin(), Bits.end(), Gone), Bits.end());
		Flashes.erase(std::remove_if(Flashes.begin(), Flashes.end(), [Now](const Flash& F) { return Now > F.Born + F.Life; }), Flashes.end());
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

	void Battle::Lay(int Slot, const Vec& Middle, const Vec& AxisA, const Vec& AxisB, const Rgb& Col, double Alpha, double HeartAlpha, double RimAlpha, double Point, double Tail, double Shade, const View& Eye, double Rough, uint32_t LumpSeed, double Turn)
	{
		const size_t First = static_cast<size_t>(Slot) * PointsPerPuff;
		// (a clod's lumpy outline: a few waves round it, its own, turning as it tumbles)
		const double P2 = (LumpSeed & 1023u) * (2.0 * Pi / 1024.0);
		const double P3 = ((LumpSeed >> 10) & 1023u) * (2.0 * Pi / 1024.0);
		const double P5 = ((LumpSeed >> 20) & 1023u) * (2.0 * Pi / 1024.0);
		auto Put = [this](size_t I, const Vec& At, const Rgb& C, double A)
		{
			Out[I] = At;
			OutRgba[I * 4 + 0] = Byte(C.R);
			OutRgba[I * 4 + 1] = Byte(C.G);
			OutRgba[I * 4 + 2] = Byte(C.B);
			OutRgba[I * 4 + 3] = Byte(A);
		};
		Put(First, Middle, Col, Alpha);
		for (int K = 0; K < Sides; ++K)
		{
			const double Angle = K * 2.0 * Pi / Sides;
			const Vec Dir = AxisA * std::cos(Angle) + AxisB * std::sin(Angle);
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
				const double W = Angle - Turn;
				Reach *= std::max(0.35, 1.0 + Rough * (0.5 * std::cos(2.0 * W + P2) + 0.35 * std::cos(3.0 * W + P3) + 0.25 * std::cos(5.0 * W + P5)));
			}
			Rgb Shaded = Col;
			Rgb HeartShaded = Col;
			if (Shade > 0.0)
			{
				// (unlit puffs: lighter on the side toward the sun, darker away from it, so a cloud has a lit side)
				const double Toward = Dir.Unit(ZUp).Dot(Eye.Sun);
				const double F = std::max(0.2, 1.0 + Shade * (ShadeLit * std::max(0.0, Toward) - ShadeDark * std::max(0.0, -Toward)));
				Shaded = Col * F;
				HeartShaded = Col * (1.0 + 0.5 * (F - 1.0));
			}
			Put(First + 1 + static_cast<size_t>(K), Middle + Dir * (0.5 * Reach), HeartShaded, HeartAlpha);
			Put(First + 1 + static_cast<size_t>(Sides) + static_cast<size_t>(K), Middle + Dir * Reach, Shaded, RimAlpha);
		}
	}

	bool Battle::LayBit(int Slot, const Bit& B, double Now, const View& Eye)
	{
		const double Age = Now - B.Born;
		if (Age < 0.0 || Age > B.Life)
		{
			return false;
		}
		const double F = Age / B.Life;
		const double Rad = B.R0 + (B.R1 - B.R0) * std::pow(F, B.Grow);
		const double Fade = std::min(1.0, Age / std::max(B.FadeIn, 1e-3)) * (F < B.Hold ? 1.0 : std::pow(std::max(0.0, 1.0 - (F - B.Hold) / std::max(1.0 - B.Hold, 1e-6)), 1.4));
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
		const double MetresAPixel = Dist / std::max(Eye.PixelsPerRadian, 1.0);
		Vec AxA = Eye.Right * Rad, AxB = Eye.Up * Rad;
		Vec Middle = P;
		switch (B.Form)
		{
		case Bit::Shape::Puff:
			if (B.Stretch > 1.0001 && B.Along.Length() > 0.5)
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
			const Vec Way = B.HalfLength > 0.0 ? B.Along : Going(B, Age);
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
		Lay(Slot, Middle, AxA, AxB, Col, A, A * B.Heart, A * B.Rim, B.Point, B.Tail, B.bLight ? 0.0 : B.ShadeK, Eye, B.Rough, B.LumpSeed, B.Turn + B.TurnRate * Age);
		return true;
	}

	int Battle::Draw(double Now, const View& Eye)
	{
		LastEye = Eye.Eye;
		bEyeKnown = true;
		Run(Now);
		Prune(Now);
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
		// then the rest, far to near, so that each is laid over what is behind it
		Sorted.clear();
		for (const Bit& B : Bits)
		{
			if (Now < B.Born || Now > B.Born + B.Life)
			{
				continue;
			}
			Sorted.push_back(Order{ &B, (Where(B, Now - B.Born) - Eye.Eye).Dot(Eye.Forward) });
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

	int Battle::Lamps(double Now, Lamp* Into, int Room) const
	{
		std::vector<Lamp> All;
		for (const Flash& F : Flashes)
		{
			const double Age = Now - F.Born;
			if (Age < 0.0 || Age > F.Life)
			{
				continue;
			}
			Lamp L;
			L.At = F.At;
			L.Col = F.Col;
			L.Strength = F.Strength * (1.0 - Age / std::max(F.Life, 1e-6));
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
			L.Strength = 0.25 * std::sqrt(S.Across) * (0.75 + 0.25 * std::sin(Now * 23.0 + Seed) * std::sin(Now * 7.3 + Seed * 0.5));
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
