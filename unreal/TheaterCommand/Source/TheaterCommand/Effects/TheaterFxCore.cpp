#include "Effects/TheaterFxCore.h"

#include <algorithm>
#include <cmath>
#include <utility>

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
		 * laid at full strength on the next frame, so a battle at 30 frames a second or less still sees it. A light
		 * marked whole (each piece of one flash) is laid the same way however long it lives, so in a slow frame the
		 * pieces of one flash keep their weights to each other. */
		bool Unseen(const TheaterFx::Bit& B)
		{
			return B.bLight && (B.Life < 0.06 || B.bWhole) && !B.bShown;
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
		SkyFill = Clamp01(Num("game.sky_fill", 0.25));
		SunThrough = std::max(0.0, Num("game.sun_through", 1.5));
		SunThroughPow = std::max(0.5, Num("game.sun_through_power", 3.0));
		SunThroughCol = Picture("game.sun_through_colour", Rgb::FromPicture(255, 205, 160));
		ClodLeast = Num("game.clod_least_px", 1.2);
		// (art director 17:52Z: smoke and dust one can see through fades out as it comes up to the eye, so it never fills
		// the whole frame evenly. Without the key, as before)
		NearGone = NearWhole = 0.0;
		if (Has("game.near_eye_fade_m.0"))
		{
			NearGone = std::min(20.0, std::max(0.0, Num("game.near_eye_fade_m.0", 2.0)));
			NearWhole = std::min(30.0, std::max(NearGone + 0.1, Num("game.near_eye_fade_m.1", 8.0)));
		}
		Beyond = Num("game.draw_beyond_m", 1500.0);
		KeepBeyond = Num("oil_fires.draw_beyond_m", Beyond); // (a column on the horizon is drawn kilometres off)
		RoundUp = Num("oil_fires.round_up", 0.35); // (the sun high: how much of an oil column it lights all round, not only on its sunward side)
		ClodRough = Range("burst.debris.clods.rough", 0.25, 0.45);
		ClodTurns = Range("burst.debris.clods.tumble_turns_a_second", 0.5, 2.0);
		ClodShade = Num("burst.debris.clods.shade", 2.5);
		TracerHalf = Num("tracer.streak_m", 16.0) * 0.5;
		TracerLeast = Num("tracer.least_px", 3.0);
		GlintPow = std::max(1.0, Num("air_debris.glint_power", 30.0));
		TracerMost = Num("tracer.most_px", 5.0);
		TracerMost = TracerMost > 0.0 ? std::max(TracerLeast, TracerMost) : 0.0; // (0: no limit)
		TracerCol = Picture("tracer.colour", Rgb::FromPicture(213, 199, 104));
		TracerPale = TracerCol.Mix(Rgb(1.0f, 1.0f, 1.0f), 0.55);
		TracerDayWide = Num("tracer.day_width", 1.5);
		WholePathBelow = Num("tracer.whole_path_below_m", 100.0);
		WholeFor = std::max(0.001, Num("tracer.whole_path_s", 0.015));
		DayFrom = Num("game.day_light_from", 0.38);
		DayFull = std::max(DayFrom + 0.01, Num("game.day_light_full", 0.9));
		DayLampShare = Num("game.day_lamp_share", 0.25);
		DayLampCol = Picture("game.day_lamp_colour", Rgb::FromPicture(255, 240, 215));
		TracerDayCore = Picture("tracer.day_core", Rgb::FromPicture(245, 240, 210));
		LitGain = std::clamp(Num("picture.lit_gain", 1.0), 1.0, 4.0);
		PutK = 1.0;
		TracerDayPx = std::max(0.0, Num("tracer.day_px", 0.0));
		TracerDayCorePx = std::clamp(Num("tracer.day_core_px", TracerDayPx * 0.6), 0.0, TracerDayPx);
		TracerDayEdge = Picture("tracer.day_edge", Rgb::FromPicture(235, 180, 70));
		TracerDayOver = std::max(1.0, Num("tracer.day_over", 1.0));
		CannonHeartCol = Picture("air_guns.flash_heart", Rgb::FromPicture(240, 220, 150));
		FlameCol = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		FlashCoreCol = Picture("burst.flash.core_colour", Rgb::FromPicture(236, 168, 72));
		GunSmokeCol = Picture("air_guns.smoke_colour", Rgb::FromPicture(168, 166, 160));
		AirRows.clear();
		TrailRows.clear();
		DustRows.clear();
		ThinAbove = Range("dust.vehicles.from_above.above_m", 15.0, 25.0);
		ThinSteep = Range("dust.vehicles.from_above.steep", 0.3, 0.5);
		ThinNear = Range("dust.vehicles.from_above.near_m", 6.0, 12.0);
		ThinKeep = Clamp01(Num("dust.vehicles.from_above.keep", 0.25));
		DustRoom = std::max(0.05, std::min(1.0, Num("dust.vehicles.most_of_room", 0.4)));
		DustFullWithin = std::max(0.0, Num("dust.vehicles.full_within_m", 120.0));
		CaseMouth = Picture("ground_cases.mouth", Rgb::FromPicture(46, 34, 18));
		CaseGlint = Picture("ground_cases.glint_colour", Rgb::FromPicture(255, 220, 140));
		CaseShadow = Clamp01(Num("ground_cases.contact_shadow", 0.5));
		CaseShine = std::clamp(Num("ground_cases.shine", 8.0), 1.0, 64.0);
		MostLying = static_cast<size_t>(std::clamp(Num("ground_cases.most_lying", 200.0), 0.0, 5000.0));
		CaseFar = std::max(1.0, Num("ground_cases.draw_within_m", 150.0));
		CaseDetailPx = std::max(0.0, Num("ground_cases.detail_px", 3.0));
		CaseHoldPx = std::max(0.0, Num("ground_cases.hold_px", 0.0));
		CaseFullPx = std::max(0.0, Num("ground_cases.full_px", 0.6));
		CaseGonePx = std::clamp(Num("ground_cases.gone_px", 0.2), 0.0, CaseFullPx);
		CaseFarGlint = std::max(0.0, Num("ground_cases.far_glint", 1.0));
		CaseFarShade = Clamp01(Num("ground_cases.far_shade", 1.0));
		CaseFarShine = std::max(0.0, Num("ground_cases.far_shine", 0.0));
		CaseNearer = std::max(0.0, Num("ground_cases.nearer_m", 0.0));
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
		Falls.clear();
		Dusts.clear();
		FlamesDue.clear();
		InSight = HeldAtDraw = TwinBits = 0;
		SeenBits = SeenTwin = 0;
		CaseOff = 0;
		bEyeWas = false;
		EyeMoved = EyeTurned = 0.0;
		DustInSight = DustSince = 0;
		CaseUntil.clear();
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
		Out_.Ring = Picture(P + "ring", Out_.Dust);
		Out_.Foot = Picture(P + "foot", Out_.Heavy);
		Out_.Stone = Picture(P + "stone", Out_.Clod);
		Out_.FootDust = Has(P + "foot_dust") ? Clamp01(Num(P + "foot_dust", 0.0)) : -1.0;
		Out_.bScar = Has(P + "scar.0");
		Out_.Scar = Picture(P + "scar", Out_.Heavy);
		Out_.ScarRays = std::max(1.0, Num(P + "scar_rays", 1.0));
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
			const double ShowerK = Ground == "water" ? 0.0 : Num(G + "shower", ClodK);
			const double CrownH = Pick(D, S + "crown_m", 8.0, 12.0);
			Wink(Now, At, Scale, D);
			Skirt(Now + 0.01, At, K, P, DustK, D);
			Crown(Now + 0.005, At, CrownH, K, Slow, P, ClodK, ShowerK, Size, D);
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
			B.Col2 = Rgb::FromPicture(66, 36, 22); // (art director 22:24Z: a flash's edge is never fully saturated: it dies to a warm grey)
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
		const Rgb Col = P.Ring * Num(R + "colour_from_ground", 0.85); // (the ground's own ring colour where it has one: earth's darker than its dust)
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
		// (art director 03:13Z: from a low eye the skirt read as a flat disc with an edge. Its rim is broken into a few
		// lobes of uneven reach, its outer part feathers away, and it stays low)
		const int NLobes = Has(Sk + "lobes.0") ? D.Whole(static_cast<int>(Num(Sk + "lobes.0", 5.0)), static_cast<int>(Num(Sk + "lobes.1", 7.0))) : 0;
		const double LobeLeast = Num(Sk + "lobe_least", 0.45);
		std::vector<double> LobeAt, LobeReach, LobeHalf;
		for (int L = 0; L < NLobes; ++L)
		{
			LobeAt.push_back(2.0 * Pi * (L + D.U(-0.3, 0.3)) / NLobes);
			LobeReach.push_back(D.U(0.6, 1.0));
			LobeHalf.push_back(2.0 * Pi / NLobes * D.U(0.6, 0.95));
		}
		auto ReachAt = [&](double A) // (the share of the full reach the skirt has that way round)
		{
			if (NLobes == 0)
			{
				return 1.0;
			}
			double Most = 0.0;
			for (int L = 0; L < NLobes; ++L)
			{
				const double Off = std::fabs(std::remainder(A - LobeAt[static_cast<size_t>(L)], 2.0 * Pi)) / LobeHalf[static_cast<size_t>(L)];
				Most = std::max(Most, LobeReach[static_cast<size_t>(L)] * (Off < 1.0 ? 0.5 + 0.5 * std::cos(Pi * Off) : 0.0));
			}
			return LobeLeast + (1.0 - LobeLeast) * Most;
		};
		const double FeatherFrom = Clamp01(1.0 - Num(Sk + "feather", 0.0)); // (the share of the reach out to which it is full strength)
		const double MostHigh = Num(Sk + "most_high_m", 0.0) * K;
		for (int I = 0; I < NOut + NIn; ++I)
		{
			const bool bFront = I < NOut;
			const int N = bFront ? NOut : NIn;
			const int Place = bFront ? I : I - NOut;
			const double Speed = bFront ? (FeatherFrom < 1.0 ? D.U(FeatherFrom, 1.0) : 1.0) : std::sqrt(D.U(0.02, 0.85)); // (the inner ones fill back toward the hole, evenly over its round: one skirt, not a ring)
			const double A = 2.0 * Pi * (Place + D.U(-0.4, 0.4)) / std::max(N, 1);
			const Vec Outward(std::cos(A), std::sin(A), 0.0);
			const double Drag = DragOf * D.U(0.85, 1.15);
			Bit B;
			B.Form = Bit::Shape::Upright;
			B.bHug = true;
			B.Born = T0 + D.U(0.0, 0.02);
			B.From = At + Outward * (D.U(0.4, 1.0) * K);
			const double Out_ = Speed * D.U(0.85, 1.0); // (how far out it goes, as a share of the reach that way)
			B.Vel = Outward * (Reach * ReachAt(A) * Out_ * Drag);
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
			if (MostHigh > 0.0)
			{
				B.Flatten = std::min(B.Flatten, MostHigh / (1.8 * std::max(B.R1, 1e-3))); // (its top, at 1.8 times its half height, under most_high_m)
			}
			B.Col = Col * D.U(0.9, 1.08);
			B.Col2 = B.Col;
			// (its outer part feathers away: full strength out to FeatherFrom of the reach, a sixth of it at the rim)
			const double Feather = Out_ > FeatherFrom ? 1.0 - 0.85 * Clamp01((Out_ - FeatherFrom) / std::max(1.0 - FeatherFrom, 1e-3)) : 1.0;
			B.Alpha = std::min(0.9, AlphaOf.Pick(D) * DustAlpha) * Feather;
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

	void Battle::Crown(double T0, const Vec& At, double CrownH, double K, double Slow, const Paint& P, double ClodK, double ShowerK, const std::string& Size, Dice& D)
	{
		// The crown: a wide torn fan. A few heavy dark jets, very uneven in length and width, bunched and overlapping;
		// lighter dust filling the gaps so that the outline is one torn fan; each heavy jet thinnest and torn at its
		// tip, drawn to a point, with clods flying on beyond it. Then a dense dark body at the foot.
		const std::string C = "burst.crown.";
		// (art director 03:13Z, from Brian's battle clips: the burst itself carries the black. With burst.foot on, a dark
		// mass of lumps stands at the foot, and the body and the dust between the jets start near-black too, settling to
		// brown-grey; never on water)
		const std::string Ft = "burst.foot.";
		const bool bDarkFoot = (ClodK > 0.02 || ShowerK > 0.02) && Has(Ft + "count.0");
		const Span FootShade = Range(Ft + "shade", 0.35, 0.55);
		const Rgb Settled = P.Smoke.Mix(P.Dust, Num(Ft + "settle_dust", 0.3));
		const double FillDust = P.FootDust >= 0.0 ? P.FootDust : Num(Ft + "fill_dust", 0.35);
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
		std::vector<Vec> JetThrow; // (each jet's way and the speed that carries a lump to its tip: the shower streams along them)
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
			JetThrow.push_back(Dir * std::sqrt(2.0 * 9.8 * ReachJ));
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
				Rgb Col = bHeavy ? P.Heavy * Shade : (bDarkFoot ? P.Foot.Mix(P.Dust, DustShare * FillDust) : P.Soil.Mix(P.Dust, DustShare)) * Shade;
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
				B.Col2 = P.bFirst ? Col : (bHeavy && F < RootSolid ? P.Heavy.Mix(P.Dust, 0.35) : (bDarkFoot ? Settled : P.Dust)); // (the root stays dark earth)
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
		// the spikes: many thin dark rays of earth among the heavy jets, each drawn to a point with a lump riding its tip
		// (art director 03:13Z, from Brian's battle clips). Few long pieces each, so thirty of them cost little. On land
		// only: a water spout keeps its passed column.
		const std::string Sp_ = "burst.crown.spikes.";
		const double HalvePast = Num("burst.debris.shower.halve_past", 0.0);
		const double Busy = HalvePast > 0.0 && static_cast<double>(RoomAsked()) > HalvePast ? 0.5 : 1.0; // (a busy screen: half the many small things)
		const int NSpike = (ClodK > 0.02 || ShowerK > 0.02) && Has(Sp_ + "count.0") ? static_cast<int>(D.Whole(static_cast<int>(Num(Sp_ + "count.0", 24.0)), static_cast<int>(Num(Sp_ + "count.1", 40.0))) * Busy) : 0;
		const int PiecesLo = std::max(2, static_cast<int>(Num(Sp_ + "pieces.0", 3.0))), PiecesHi = std::max(PiecesLo, static_cast<int>(Num(Sp_ + "pieces.1", 5.0)));
		const Span SpikeReach = Range(Sp_ + "reach", 0.4, 1.15), SpikeLean = Range(Sp_ + "lean", 0.15, 1.1), SpikeThick = Range(Sp_ + "thick", 0.25, 0.6);
		const Span SpikeShade = Range(Sp_ + "shade", 0.55, 0.85), SpikeTipClod = Range(Sp_ + "tip_clod_m", 0.05, 0.12), SpikeFill = Range(Sp_ + "lump_share", 0.6, 0.9);
		const double SpikeTipAlpha = Num(Sp_ + "tip_alpha", 0.6), SpikeRagged = Num(Sp_ + "ragged", 0.8), SpikeDroop = Clamp01(Num(Sp_ + "droop", 0.0));
		const double SpikeMost = Num(Sp_ + "longest_of_median", 0.0), SpikeStretchMost = Num(Sp_ + "stretch_most", 14.0);
		const Span SpikeRough = Range(Sp_ + "rough", 0.35, 0.65);
		// (unequal lengths, the longest no more than longest_of_median times the middle one: a long lone blade reads as a stick)
		std::vector<double> Reaches;
		for (int S = 0; S < NSpike; ++S)
		{
			Reaches.push_back(CrownH * std::max(0.0, SpikeReach.Pick(D))); // (never below nothing: a negative reach had no root)
		}
		if (SpikeMost > 0.0 && !Reaches.empty())
		{
			std::vector<double> ByLength = Reaches;
			std::nth_element(ByLength.begin(), ByLength.begin() + static_cast<std::ptrdiff_t>(ByLength.size() / 2), ByLength.end());
			const double Cap = ByLength[ByLength.size() / 2] * SpikeMost;
			for (double& Rch : Reaches)
			{
				Rch = std::min(Rch, Cap);
			}
		}
		for (int S = 0; S < NSpike; ++S)
		{
			const double Az = D.U(0.0, 2.0 * Pi);
			const double Lean = SpikeLean.Pick(D) * Half;
			const Vec Dir = (Vec(std::cos(Az) * std::sin(Radians(Lean)), std::sin(Az) * std::sin(Radians(Lean)), std::cos(Radians(Lean))) + Tilt).Unit();
			const double ReachS = Reaches[static_cast<size_t>(S)];
			JetThrow.push_back(Dir * std::sqrt(2.0 * 9.8 * ReachS));
			const double Thick = SpikeThick.Pick(D);
			const double Rf = FootR * Thick * (P.Grain < 0.999 ? P.Grain : 1.0), Rt = TipR * Thick;
			const Rgb Col = P.Heavy * SpikeShade.Pick(D);
			// (a few lumps along it with small gaps between, never one clean cone: art director 03:28Z)
			const int SpikePieces = D.Whole(PiecesLo, PiecesHi);
			for (int Q = 0; Q < SpikePieces; ++Q)
			{
				const double F = (Q + D.U(0.35, 0.65)) / SpikePieces; // (the middle of this lump, along the spike)
				const double Rl = (Rt + (Rf - Rt) * std::pow(1.0 - F, 0.8)) * D.U(0.75, 1.3); // (unequal lumps)
				const double V = ReachS * KD * 1.12 / std::sqrt(Slow);
				Bit B;
				B.Born = T0 + 0.01 + 0.025 * (1.0 - F);
				B.From = At + ZUp * 0.1 + Dir * (0.25 * K);
				B.Vel = Dir * (V * (0.05 + 0.95 * std::pow(F, 0.9))) + Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), D.U(-1.0, 1.0)) * (Rl * SpikeRagged * KD); // (ragged: never a smooth blade)
				B.Drag = KD;
				B.Gravity = 9.8 / Slow * (1.0 + SpikeDroop * F); // (the outer lumps fall faster, so the tip bends down after about 0.3 s)
				B.Windage = 0.35;
				B.Life = D.U(0.9, 1.5) * Slow;
				B.R0 = Rl;
				B.R1 = Rl * D.U(1.1, 1.3); // (they barely swell, so the gaps stay open)
				B.Grow = 0.5;
				B.Along = Dir;
				B.Stretch = std::clamp(ReachS * SpikeFill.Pick(D) / (2.0 * SpikePieces * Rl * 1.25), 1.0, std::max(1.0, SpikeStretchMost)); // (each lump short of the next: a small gap)
				B.Point = Q == SpikePieces - 1 ? 0.6 : 0.2; // (only the last drawn to a point)
				B.Tail = 0.1;
				B.Col = Col * D.U(0.9, 1.1);
				B.Col2 = P.Heavy.Mix(P.Dust, 0.4);
				B.Cool = 0.7;
				B.Alpha = 1.0 - (1.0 - SpikeTipAlpha) * F * F;
				B.FadeIn = 0.01;
				B.Hold = 0.35;
				B.Heart = 0.95;
				B.Inner = 0.8;
				B.Rim = 0.0;
				B.Rough = SpikeRough.A + (SpikeRough.B - SpikeRough.A) * F; // (broken edges, most torn at the tip)
				B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
				Bits.push_back(B);
			}
			if (ClodK > 0.05)
			{
				const Vec Way = (Dir + Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), D.U(-1.0, 1.0)) * 0.06).Unit();
				Launch.push_back(Way * (std::sqrt(2.0 * 9.8 * ReachS) * Pick(D, C + "tip_clod_speed", 1.1, 1.35)));
				LaunchR.push_back(SpikeTipClod.Pick(D) * K);
			}
		}
		// the body: a dense dark mound of earth and dust at the crown's foot from about frame 4, slumping into the smoke
		const int NBody = static_cast<int>(Num(Path("body.puffs"), 50.0) * std::max(0.7, std::sqrt(K)));
		const double BodyWide = Num(Path("body.width"), 0.3), BodyHigh = Num(Path("body.height"), 0.22);
		const Span BodyR = Range(Path("body.puff_r"), 0.06, 0.11), BodyAlpha = Range(Path("body.alpha"), 0.85, 0.95);
		const double BodyFrom = Num(Path("body.from_frame"), 3.0) / 30.0;
		const double BodyRim = Num(Path("body.rim"), 0.1), BodyRough = Num(Path("body.rough"), 0.0);
		// (with the dark foot on, the body is near-black too for its first second, settling to brown-grey)
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
			if (bDarkFoot)
			{
				B.Col = P.Foot * FootShade.Pick(D);
				B.Col2 = Settled;
				B.Cool = Num(Ft + "settle", 0.75);
			}
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
		// the foot: a dense mass of near-black lumps at the crown's base for the first second, settling to brown-grey
		// (art director 03:13Z, from Brian's battle clips: the burst itself has to carry the black, no grade can)
		if (bDarkFoot)
		{
			const int NFoot = static_cast<int>(Pick(D, Ft + "count", 70.0, 100.0) * std::max(0.7, std::sqrt(K)));
			const double FootWide = Num(Ft + "width", 0.25), FootHigh = Num(Ft + "height", 0.22);
			const Span FootRs = Range(Ft + "lump_r", 0.025, 0.07), FootLife = Range(Ft + "life_s", 1.2, 1.8);
			const double FootCool = Num(Ft + "settle", 0.75), FootRough = Num(Ft + "rough", 0.45), FootShadeK = Num(Ft + "light", 1.2);
			for (int I = 0; I < NFoot; ++I)
			{
				const double A = D.U(0.0, 2.0 * Pi);
				const double Out_ = CrownH * FootWide * std::pow(D.U(), 0.7);
				const double Up_ = CrownH * FootHigh * std::pow(D.U(), 1.5);
				const Vec To = Vec(std::cos(A) * Out_, std::sin(A) * Out_, Up_) + Tilt * Up_;
				const double KB = 6.0;
				Bit B;
				B.Born = T0 + D.U(0.02, 0.12);
				B.From = At + ZUp * (0.1 * K);
				B.Vel = To * KB;
				B.Drag = KB;
				B.Gravity = 1.5 / Slow;
				B.Windage = 0.3;
				B.Life = FootLife.Pick(D) * Slow;
				const double Rr = CrownH * FootRs.Pick(D);
				B.R0 = Rr * 0.7;
				B.R1 = Rr * D.U(1.1, 1.4);
				B.Grow = 0.4;
				B.Col = P.Foot * FootShade.Pick(D);
				B.Col2 = Settled * D.U(0.85, 1.05);
				B.Cool = FootCool;
				B.Alpha = D.U(0.9, 1.0);
				B.FadeIn = 0.02;
				B.Hold = 0.55;
				B.Heart = 0.95;
				B.Inner = 0.7;
				B.Rim = 0.3;
				B.Rough = FootRough;
				B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
				B.ShadeK = FootShadeK; // (a lit top and a dark underside, so the mass has form)
				Bits.push_back(B);
			}
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
		// the shower: a dense spray of dark lumps flung higher than the crown and wide, raining back for a second or two,
		// so a hit reads as violent from any distance (burst.debris.shower)
		const size_t ShowerFrom = Launch.size();
		const std::string Sh = "burst.debris.shower.";
		if (ShowerK > 0.02 && Has(Sh + "count.0"))
		{
			const int NShower = static_cast<int>(Pick(D, Sh + "count", 60.0, 90.0) * ShowerK * std::max(0.5, std::min(K * K, Num(Sh + "most_k2", 3.0))) * Busy);
			const double SpeedK = std::sqrt(K); // (a bigger shell throws higher: the top of the shower grows as K)
			const Span UpS = Range(Sh + "up_mps", 7.0, 16.0), OutS = Range(Sh + "out_mps", 2.0, 8.0), Rs = Range(Sh + "r_m", 0.03, 0.14);
			const double Skew = Num(Sh + "small_skew", 2.0);
			// (most stream along the crown's jets, some falling short inside them and some flying on past their tips;
			// the rest are flung every way)
			const double AlongJets = JetThrow.empty() ? 0.0 : Clamp01(Num(Sh + "along_jets", 0.6));
			const double JetSpread = Num(Sh + "jet_spread", 0.2);
			const Span JetSpeed = Range(Sh + "jet_speed", 0.6, 1.3);
			for (int I = 0; I < NShower; ++I)
			{
				if (D.U() < AlongJets)
				{
					const Vec& Jet = JetThrow[static_cast<size_t>(D.Whole(0, static_cast<int>(JetThrow.size()) - 1))];
					const double Speed = Jet.Length();
					const Vec Way = (Jet * (1.0 / std::max(Speed, 1e-6)) + Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), D.U(-1.0, 1.0)) * JetSpread).Unit(ZUp);
					Launch.push_back(Way * (Speed * JetSpeed.Pick(D)));
				}
				else
				{
					const double A = D.U(0.0, 2.0 * Pi);
					const double Out_ = OutS.Pick(D) * SpeedK;
					const double Up_ = UpS.Pick(D) * SpeedK;
					Launch.push_back(Vec(std::cos(A) * Out_, std::sin(A) * Out_, Up_));
				}
				LaunchR.push_back((Rs.A + (Rs.B - Rs.A) * std::pow(D.U(), Skew)) * K);
			}
		}
		const size_t ShowerTo = Launch.size();
		const Span ShowerShade = Range(Sh + "shade", 0.55, 0.9);
		const Span ShowerRest = Range(Sh + "rest_s", 1.0, 2.5);
		const double ShowerPuffR = Num(Sh + "landing_puff_r_m", 0.08);
		// (a lump thrown past the lens lands beside the eye, where a dust puff fills a sixth of the screen as a flat slab:
		// art director 03:40Z. The recipe can leave that puff out)
		const bool bLensPuff = Num("burst.debris.lens.landing_puff", 1.0) > 0.5;
		// (and it is a dark torn lump, never a flat pale slab: broken edges and earth's shade)
		const bool bLensRough = Has("burst.debris.lens.rough.0"), bLensShade = Has("burst.debris.lens.shade.0");
		const Span LensRough = Range("burst.debris.lens.rough", 0.6, 0.9), LensShade = Range("burst.debris.lens.shade", 0.5, 0.8);
		// (a close hit throws a few lumps past the lens: what puts the player in the hole)
		if (bEyeKnown && ClodK > 0.05)
		{
			const double Near = Num("burst.debris.lens.within_m", 40.0) * std::max(1.0, K);
			if ((LastEye - At).Length() < Near)
			{
				const Span LensMiss = Range("burst.debris.lens.miss_m", 1.2, 3.0);
				const int N = D.Whole(static_cast<int>(Num("burst.debris.lens.pieces.0", 3.0)), static_cast<int>(Num("burst.debris.lens.pieces.1", 7.0)));
				for (int I = 0; I < N; ++I)
				{
					const double T = D.U(0.7, 1.4);
					const Vec Miss = Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), D.U(-0.6, 0.4)).Unit() * LensMiss.Pick(D); // (no nearer than this: one at a metre fills a sixth of the screen)
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
			const bool bShower = I >= ShowerFrom && I < ShowerTo;
			B.Col = (P.bSplinter && D.U() < 0.35 ? P.Splinter : (bShower ? P.Stone : P.Clod)) * (bShower ? ShowerShade.Pick(D) : D.U(0.8, 1.2));
			B.Col2 = B.Col;
			B.Alpha = 1.0;
			B.FadeIn = 0.005;
			B.Heart = 0.95;
			B.Rim = 0.6;
			B.LeastPx = ClodLeast;
			Chunk(B, D);
			if (I >= ShowerTo)
			{
				if (bLensRough)
				{
					B.Rough = LensRough.Pick(D);
				}
				if (bLensShade)
				{
					B.Col = P.Foot * LensShade.Pick(D);
					B.Col2 = B.Col;
				}
			}
			Throw(B);
			B.Life = B.Land2 + (bShower ? ShowerRest.Pick(D) : D.U(2.0, 4.0));
			B.Hold = std::min(0.95, (B.Land2 + 1.0) / B.Life);
			const Vec Landed = B.At1;
			const double Down = B.Born + B.Land;
			const double R = B.R0;
			Bits.push_back(B);
			if (R > (bShower ? ShowerPuffR * K : 0.04) && (bLensPuff || I < ShowerTo))
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
		// (art director 03:13Z: the foot carries the black for its first second, so the earth that stands over it and
		// the smoke at its foot start dark and settle to brown-grey; never on water)
		const bool bDarkFoot = Ground != "water" && Has("burst.foot.count.0");
		const Rgb VeilCol = bDarkFoot ? P.Dust.Mix(P.Foot, P.FootDust >= 0.0 ? 1.0 - P.FootDust : Num("burst.foot.veil_dark", 0.5)) : P.Dust;
		const double FootBand = Num("burst.foot.smoke_band", 0.45), FootSmokeDark = Num("burst.foot.smoke_dark", 0.45), FootSettle = std::max(0.1, Num("burst.foot.smoke_settle_s", 1.0));
		for (int I = 0; I < NVeil; ++I)
		{
			Bit B;
			B.Born = T0 + D.U(0.06, 0.14);
			B.From = At + ZUp * 0.2 + Vec(D.U(-0.4, 0.4), D.U(-0.4, 0.4), 0.0) * K;
			B.Vel = Vec(D.U(-1.0, 1.0) * 1.6, D.U(-1.0, 1.0) * 1.6, D.U(4.0, 8.5)) * (K / std::sqrt(Slow));
			B.Life = D.U(1.4, 2.4) * Slow;
			B.R0 = 0.25 * K;
			B.R1 = D.U(0.7, 1.2) * K;
			B.Col = VeilCol;
			B.Col2 = VeilCol;
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
		const Rgb Top = Picture("grounds." + Ground + ".smoke_top", Picture(Sm + "colour_top", Rgb::FromPicture(66, 64, 62))); // (a water spout's mist goes pale, never charcoal)
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
		const double AlphaK = Num("grounds." + Ground + ".smoke_alpha", 1.0); // (a water spout leaves a thin mist, not a cloud)
		// (and the wind draws that mist out and thins it to half by smoke_half_by_s, so no round ball is left: art director 03:28Z)
		const double HalfBy = Num("grounds." + Ground + ".smoke_half_by_s", 0.0);
		const double WindStretch = Num("grounds." + Ground + ".smoke_stretch", 1.0);
		const Vec Downwind = Vec(Wind.X, Wind.Y, 0.0).Unit(Vec(Lean.X, Lean.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0)));
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
			if (bDarkFoot && F < FootBand)
			{
				B.Col = B.Col * FootSmokeDark; // (dark at the foot for its first second, settling to the cloud's own colour)
				B.Cool = B.Life / FootSettle;
			}
			B.Alpha = SmokeAlpha.Pick(D) * AlphaK;
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
			if (HalfBy > 0.0)
			{
				B.HazeBy = HalfBy;
				B.HazeAlpha = B.Alpha * 0.5;
			}
			if (WindStretch > 1.0)
			{
				B.Along = Downwind;
				B.Stretch = WindStretch;
				B.Windage = D.U(0.3, 1.7); // (some puffs carried further than others: the cloud draws out downwind)
				// (and the cloud itself is laid out and spreads that much longer down the wind than across it)
				const Vec Off = B.From - At;
				B.From = B.From + Downwind * (Off.Dot(Downwind) * (WindStretch - 1.0));
				B.Vel = B.Vel + Downwind * (B.Vel.Dot(Downwind) * (WindStretch - 1.0));
			}
			Bits.push_back(B);
		}
		// the crater smoulders faintly for a while (water has none)
		if (Ground == "water")
		{
			return;
		}
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
		const Rgb Thrown = P.bScar ? P.Scar : (bSnow ? P.Soil.Mix(P.Dust, 0.35) : P.Heavy); // (art director 03:40Z: on snow a brown-black star)
		const int NRing = D.Whole(6, 9);
		for (int I = 0; I < NRing; ++I)
		{
			const double A = 2.0 * Pi * (I + D.U(-0.3, 0.3)) / NRing;
			const double Dist = D.U(0.8, 0.5 + 0.55 * Ring_) * R;
			// (longer rays sit further out, so they reach out from the hole rather than across it)
			Mark(At + Vec(std::cos(A), std::sin(A), 0.0) * (Dist + 0.7 * R * (P.ScarRays - 1.0)), D.U(0.35, 0.6) * R, D.U(1.2, 1.8) * P.ScarRays, P.bScar ? A : A + Pi / 2.0, Thrown * D.U(0.8, 1.1), D.U(0.55, 0.8), 0.6, 0.3);
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

	void Battle::Muzzle(double Now, const Vec& At, const Vec& Facing, const char* GunIn, const char* GroundIn)
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
		// (art director 07:08Z: close to the eye a gun's flash is lit from within, a yellow-white root and an orange body
		// with dense rims, shrinking rather than going see-through; far off as before)
		HotFlash H;
		const bool bHot = HotOf(At, H);
		const bool bRagged = bHot && H.bRagged;
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
			if (bHot)
			{
				B.From = At + Fw * (Len * 0.4); // (its back end at the muzzle as it shrinks)
				B.R0 = Len * 0.24;
				B.R1 = Len * 0.18;
				B.Col = H.Orange.Mix(H.Hot, 0.3);
				B.Col2 = H.Orange;
				B.Heart = 0.95;
				B.Rim = 0.8;
				B.Inner = 0.7;
				B.Edge = H.Rim;
				B.EdgeK = 0.4;
				B.Hold = 0.8;
			}
			if (bRagged)
			{
				// (art director 13:54Z: graded from its root out and ragged, new every shot; the same dice go on as before)
				Dice DF(PlaceKey(At) * 0xD6E8FEB86659FD93ull ^ (static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) + 0x52ull));
				HotFlame(Now, At, Fw, Len * 1.4, B.Life, H, DF);
			}
			else
			{
				Bits.push_back(B);
			}
			if (bHot && !bRagged)
			{
				// (the root: white-yellow at the muzzle, laid over the tongue)
				Bit R = B;
				R.From = At + Fw * (Len * 0.24);
				R.Vel = Fw * 1.0;
				R.R0 = Len * 0.1;
				R.R1 = Len * 0.08;
				R.Stretch = 2.1;
				R.Point = 0.5;
				R.Tail = 0.0;
				R.Col = H.Root;
				R.Col2 = H.Root2;
				R.Heart = 1.0;
				R.Rim = 0.85;
				R.Inner = 0.6;
				R.Edge = H.Hot;
				R.EdgeK = 0.35;
				R.Nearer = 0.4;
				Bits.push_back(R);
			}
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
			if (bHot)
			{
				B.R1 = B.R0 * 0.7;
				B.Col = H.Orange;
				B.Col2 = H.Orange.Mix(H.Rim, 0.5);
				B.Alpha = 1.0;
				B.Heart = 1.0;
				B.Rim = 0.6;
				B.Inner = 0.7;
				B.Edge = H.Rim;
				B.EdgeK = 0.4;
				B.Hold = 0.8;
			}
			if (!bRagged)
			{
				Bits.push_back(B);
			}
		}
		if (Has(M + "light_w"))
		{
			// (the lamp hangs above the muzzle, as a shell's does, so the ground at the gun's foot is lifted, never washed
			// white: at the muzzle's own height a lamp of a shell's strength burns a white pool there. Its strength is cut
			// by the square of how much lower it hangs than a shell's lamp of the same strength, so its brightest spot on
			// the ground is no brighter than that shell's: 1700, a tank gun at night lit 1 m round its foot to 249 of 255)
			const double Watts = std::max(0.0, Num(M + "light_w", 0.0) / 8500.0);
			const double Hang = Num("game.muzzle_lamp_height_m", 6.0) * std::sqrt(Watts);
			const double ShellHang = std::max(1.0, Num("burst.flash.light.height_m", 16.0) * std::sqrt(Watts));
			Flash F;
			F.At = At + Fw * (Len * 0.5) + ZUp * Hang;
			F.Col = Picture("burst.flash.light.colour", Rgb::FromPicture(255, 196, 140));
			F.Born = Now;
			F.Life = 0.03;
			F.Strength = Watts * std::min(1.0, (Hang * Hang) / (ShellHang * ShellHang));
			F.Radius = 12.0 * std::max(1.0, Len) + Hang;
			F.bNightOnly = Num(M + "day_lamp", 0.0) <= 0.0; // (by day only the big guns light the ground)
			Flashes.push_back(F);
		}
		// (a towed or field gun: after each shot the breech opens and throws its big brass case out behind, to lie there,
		// so the cases pile up behind the gun through a fight. Its own dice)
		{
			CaseRow Cases;
			if (CaseRowOf(GunIn ? GunIn : "", At, Cases) && RoomForCases() < static_cast<size_t>(MaxPuffs) * 3 / 5)
			{
				Dice DC(PlaceKey(At) * 0xD1B54A32D192ED03ull ^ static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)));
				const Vec Flat_ = Vec(Fw.X, Fw.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
				const Vec Out_ = (-Flat_ + Across * (Cases.Side * 0.25 + DC.Bell() * 0.15)).Unit(-Flat_);
				GroundCase(Now + Cases.After, At - Fw * Cases.Back, Out_, Cases, DC);
			}
		}
		// then grey-brown smoke rolls out, hangs and drifts
		const double SmokeLo = Num(M + "smoke_s.0", Num(M + "smoke_s", 2.0));
		const double SmokeHi = Num(M + "smoke_s.1", SmokeLo);
		const Rgb Grey = Picture("game.muzzle_smoke", Rgb::FromPicture(150, 140, 126));
		// (art director 17:52Z, build 1746's test, a tank gun firing 4 m from two riflemen: its smoke lay over them as one
		// even gauze, fog and not dust. With muzzle.billows a field gun's shot raises a cloud in three layers instead,
		// each a scatter of lumps of its own size and thickness with gaps between, never one sheet. Low: dense mounds of
		// the ground's own dust on the ground under and ahead of the muzzle, as far as its dust ring reaches. Mid: lumps
		// of dust and smoke over them, see-through. Top: the gun's own smoke at the muzzle's height, a few thin wisps.
		// So it is heaviest low and near the muzzle and thins upward, and what stands behind it comes clear from the top
		// down. The mid and top are gone in clear_s and top_s (the thick part in 1.5 to 2 s); of the low mounds,
		// tuft_share thin to haze_alpha and drift on as tufts for tuft_s. Counts are a tank gun's: a wider ring and a
		// longer flame throw more. Dust goes by the ground's own (grounds.<g>.dust against earth's). Its own dice:
		// without the key a gun smokes as it did)
		const bool bBillows = Len >= 0.5 && Has("muzzle.billows.low");
		const std::string Bl = "muzzle.billows.";
		Dice DB(PlaceKey(At) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) ^ 0xB111ull);
		auto BillowCount = [&](const char* Key, double Usual, double Share) { return static_cast<int>(std::lround(std::min(40.0, std::max(0.0, Num(Bl + Key, Usual))) * std::min(4.0, std::max(0.0, Share)))); };
		auto BillowSpan = [&](const char* Key, double Low, double High, double Least, double Most)
		{
			Span S = Range(Bl + Key, Low, High);
			S.A = std::min(Most, std::max(Least, S.A));
			S.B = std::min(Most, std::max(S.A, S.B));
			return S;
		};
		const double BillowRough = std::min(0.8, std::max(0.0, Num(Bl + "rough", 0.5)));
		const double BillowShade = std::min(2.0, std::max(0.0, Num(Bl + "shade", 0.4)));
		const double BillowHeart = std::min(1.0, std::max(0.0, Num(Bl + "heart", 0.9))), BillowInner = std::min(0.9, std::max(0.1, Num(Bl + "inner", 0.7)));
		const double BillowRest = 1.0 + 0.6 * BillowRough; // (clear of the ground by its radius and its outline's lumps)
		if (bBillows)
		{
			const double Flame_ = std::min(Len, 4.0);
			const int NTop = BillowCount("top", 12.0, (10.0 + 6.0 * Flame_) / 17.8);
			const Span TopA = BillowSpan("top_alpha", 0.35, 0.7, 0.0, 0.95), TopS = BillowSpan("top_s", 1.6, 2.4, 0.2, 20.0), TopR = BillowSpan("top_r", 0.5, 1.3, 0.05, 3.0);
			const double Scatter = std::min(8.0, std::max(0.0, Num(Bl + "scatter_mps", 2.0)));
			const double PaleUp = std::min(1.0, std::max(0.0, Num(Bl + "pale_up", 0.6)));
			for (int I = 0; I < NTop; ++I)
			{
				// (thrown along the bore and scattered off it: the further it flies and the higher it is thrown, the
				// thinner it is and the sooner gone; the dense ones stay by the muzzle and sink)
				const double Far = DB.U(), Up = DB.Bell() * 0.7, High = Clamp01(Up), Thin = (1.0 - 0.5 * Far) * (1.0 - PaleUp * High);
				Bit B;
				B.Born = Now + DB.U(0.0, 0.05);
				B.From = At + Fw * (Flame_ * DB.U(0.3, 1.2));
				B.Vel = Fw * ((3.0 + 7.0 * Far) * Flame_) + Across * (DB.Bell() * Scatter) + ZUp * (Up * Scatter);
				B.Drag = 2.5;
				B.Rise = -0.3 + 0.6 * High;
				B.Windage = 0.8;
				B.Life = TopS.Pick(DB) * (1.0 - 0.3 * High);
				B.R0 = 0.15 * Flame_ + 0.05;
				B.R1 = TopR.Pick(DB) * Flame_ + 0.2;
				B.Col = Grey * DB.U(0.9, 1.1);
				B.Col2 = B.Col;
				B.Alpha = TopA.Pick(DB) * Thin;
				B.FadeIn = 0.05;
				B.Hold = 0.45;
				B.Heart = BillowHeart;
				B.Inner = BillowInner;
				B.Rough = BillowRough;
				B.LumpSeed = static_cast<uint32_t>(DB.U() * 1073741823.0);
				B.Turn = DB.U(0.0, 2.0 * Pi);
				B.Centre = At + Fw * (Flame_ * 2.5);
				B.CloudShade = BillowShade;
				B.CloudR = Flame_ * 1.5;
				B.Rest = BillowRest;
				Bits.push_back(B);
			}
		}
		const int NSmoke = bBillows ? 0 : Len < 0.5 ? 3 : static_cast<int>(10.0 + 6.0 * std::min(Len, 4.0)); // (a wild flame is still a few puffs)
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
		const double RingIs = Num(M + "dust_ring_m", 0.0);
		const double Ring_ = RingIs > 0.0 ? std::min(RingIs, 30.0) : 0.0; // (never wider than 30 m: a wild number stays a big gun's ring)
		const std::string Ground = GroundIn && *GroundIn ? GroundIn : "earth";
		if (Ring_ > 0.0)
		{
			const Paint P = PaintOf(Ground);
			// (how much dust this ground gives against earth's, grounds.<g>.dust: sand and town more, mud little)
			const double Dusty = Ground == "earth" ? 1.0 : std::clamp(Num("grounds." + Ground + ".dust", 0.6) / std::max(0.05, Num("grounds.earth.dust", 0.6)), 0.0, 1.6);
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
				B.Alpha = std::min(0.85, D.U(0.35, 0.5) * Dusty);
				B.FadeIn = 0.02;
				B.Hold = 0.1;
				B.HazeBy = 0.8;
				B.HazeAlpha = 0.12 * Dusty;
				B.Heart = 0.55;
				Bits.push_back(B);
			}
			if (bBillows)
			{
				const double Reach_ = Ring_, DustK = std::min(1.0, Dusty), By = std::sqrt(Reach_ / 6.0);
				const int NLow = BillowCount("low", 9.0, By), NMid = BillowCount("mid", 10.0, By);
				const Span LowA = BillowSpan("low_alpha", 0.55, 0.8, 0.0, 0.95), MidA = BillowSpan("mid_alpha", 0.35, 0.6, 0.0, 0.95);
				const Span LowR = BillowSpan("low_r", 0.14, 0.26, 0.01, 1.0), MidR = BillowSpan("mid_r", 0.15, 0.28, 0.01, 1.0);
				const Span ClearS = BillowSpan("clear_s", 1.8, 2.4, 0.2, 20.0), TuftS = BillowSpan("tuft_s", 3.0, 5.0, 0.2, 30.0);
				const Span Forth = BillowSpan("reach", -0.15, 1.1, -1.0, 2.0);
				const double Wide = std::min(1.0, std::max(0.0, Num(Bl + "wide", 0.3)));
				const double TuftShare = std::min(1.0, std::max(0.0, Num(Bl + "tuft_share", 0.4)));
				const double HazeA = std::min(0.95, std::max(0.0, Num(Bl + "haze_alpha", 0.2))), HazeBy = std::min(20.0, std::max(0.1, Num(Bl + "haze_by_s", 1.8)));
				const double SmokeMix = std::min(1.0, std::max(0.0, Num(Bl + "smoke_mix", 0.4)));
				const Span MidUp = BillowSpan("mid_up_m", 0.8, 2.6, 0.0, 10.0);
				const double PaleUp_ = std::min(1.0, std::max(0.0, Num(Bl + "pale_up", 0.6)));
				const Vec Middle = Foot + FwFlat * (Reach_ * 0.45) + ZUp * 1.0;
				for (int I = 0; I < NLow + NMid; ++I)
				{
					const bool bLow = I < NLow;
					const double Side = DB.Bell();
					Bit B;
					B.Born = Now + DB.U(0.0, 0.06);
					B.From = Foot + FwFlat * (Reach_ * Forth.Pick(DB)) + Across * (Side * Wide * Reach_);
					B.Vel = FwFlat * DB.U(1.0, bLow ? 4.0 : 5.0) + Across * (Side * DB.U(0.0, 2.0));
					B.Windage = 0.6;
					B.FadeIn = 0.05;
					B.Rough = BillowRough;
					B.LumpSeed = static_cast<uint32_t>(DB.U() * 1073741823.0);
					B.Turn = DB.U(0.0, 2.0 * Pi);
					B.Centre = Middle;
					B.CloudShade = BillowShade;
					B.CloudR = Reach_ * 0.35;
					B.CentreRise = 0.2;
					B.Rest = BillowRest;
					if (bLow)
					{
						// (a mound of the ground's dust lying on the ground, wider than it is high: thick until the thick part
						// clears; some thin to a haze and drift on as tufts. It rests on the ground as it swells: see Bit::Rest)
						const bool bTuft = DB.U() < TuftShare;
						const double Heading = DB.U(0.0, 2.0 * Pi);
						B.Drag = 2.0;
						B.Life = bTuft ? TuftS.Pick(DB) : ClearS.Pick(DB) * 1.15;
						B.R0 = 0.06 * Reach_;
						B.R1 = LowR.Pick(DB) * Reach_;
						B.Grow = 0.4;
						B.Rise = 0.0;
						B.Along = Vec(std::cos(Heading), std::sin(Heading), 0.0);
						B.Stretch = DB.U(1.3, 1.8);
						B.Col = P.Dust * (0.85 * DB.U(0.9, 1.05));
						B.Col2 = B.Col;
						B.Alpha = LowA.Pick(DB) * DustK;
						B.Hold = 0.5;
						if (bTuft)
						{
							B.HazeBy = HazeBy;
							B.HazeAlpha = std::min(B.Alpha, HazeA * DustK);
						}
						B.Heart = BillowHeart;
						B.Inner = BillowInner;
					}
					else
					{
						// (a lump of dust and smoke thrown up over them: see-through, and gone with the thick part)
						const double High = DB.U();
						B.From = B.From + ZUp * (MidUp.A + (MidUp.B - MidUp.A) * High);
						B.Vel = B.Vel + ZUp * DB.U(0.5, 2.0);
						B.Drag = 2.5;
						B.Rise = DB.U(0.2, 0.5);
						B.Life = ClearS.Pick(DB);
						B.R0 = 0.08 * Reach_;
						B.R1 = MidR.Pick(DB) * Reach_;
						B.Col = (P.Dust * 0.85).Mix(Grey, static_cast<float>(SmokeMix)) * DB.U(0.9, 1.08);
						B.Col2 = B.Col;
						B.Alpha = MidA.Pick(DB) * (0.4 + 0.6 * DustK) * (1.0 - PaleUp_ * High);
						B.Life *= 1.0 - 0.3 * High; // (the higher, the thinner and the sooner gone)
						B.Hold = 0.45;
						B.Heart = BillowHeart;
						B.Inner = BillowInner;
					}
					Bits.push_back(B);
				}
			}
			const double Veil = Num("muzzle.veil.by_ground." + Ground, 0.0);
			if (Veil > 0.0)
			{
				// (art director 06:19Z, the desert: a tank or field gun's blast raises the ground round its muzzle in a veil,
				// spread most along the line of fire, rising a metre or two, that hides the gun and its line of sight for a
				// second or two, then thins to a haze that lingers and drifts on the one wind)
				const DustRow& Sand = DustRowOf(Ground);
				const std::string V = "muzzle.veil.";
				const Span Life = Range(V + "life_s", 5.0, 8.0), R1 = Range(V + "r1_of_ring", 0.45, 0.65), Rise = Range(V + "rise_mps", 0.3, 0.7), Alpha = Range(V + "alpha", 0.7, 0.9);
				const double Reach = Num(V + "reach_of_ring", 0.9), HideS = Num(V + "hide_s", 1.8), Haze = Num(V + "haze_alpha", 0.18), Flatten = Num(V + "flatten", 0.6);
				const int NVeil = std::clamp(static_cast<int>(std::lround(std::clamp(Num(V + "puffs", 10.0), 0.0, 64.0) * std::sqrt(std::clamp(Ring_ / 6.0, 0.0, 16.0)))), 1, 160);
				// (art director 18:28Z: the veil as the tank gun's cloud, in lumps and never in upright sheets. muzzle.veil.lumps
				// times as many pieces, each a lump of its own size and thickness with a firm heart and a rough outline, lit as
				// one cloud: every other one a mound lying on the ground, the rest thrown up to lump_up_m over it, thinner the
				// higher, each lump_thick of the veil's own thickness. lump_tuft_share of them thin to the haze by hide_s and
				// linger as the veil did; the others are gone soon after it. Its own dice. 0, or no such key: the sheets, piece
				// for piece as before)
				const double LumpsIs = Num(V + "lumps", 0.0);
				const double LumpsBy = LumpsIs == LumpsIs ? std::min(4.0, std::max(0.0, LumpsIs)) : 0.0;
				const int NLumps = LumpsBy > 0.0 ? std::clamp(static_cast<int>(std::lround(NVeil * LumpsBy)), 1, 200) : 0;
				if (NLumps > 0)
				{
					auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
					Dice DV(PlaceKey(At) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) ^ 0x5A2Dull);
					const double SizeA = Safe(Num(V + "lump_r.0", 0.25), 0.05, 2.0, 0.25), SizeB = std::max(SizeA, Safe(Num(V + "lump_r.1", 0.5), 0.05, 2.0, 0.5));
					const double UpA = Safe(Num(V + "lump_up_m.0", 0.2), 0.0, 10.0, 0.2), UpB = std::max(UpA, Safe(Num(V + "lump_up_m.1", 1.2), 0.0, 10.0, 1.2));
					const double PaleUpV = Safe(Num(V + "lump_pale_up", 0.5), 0.0, 1.0, 0.5), TuftV = Safe(Num(V + "lump_tuft_share", 0.45), 0.0, 1.0, 0.45);
					const double ThickA = Safe(Num(V + "lump_thick.0", 0.3), 0.0, 1.0, 0.3), ThickB = std::max(ThickA, Safe(Num(V + "lump_thick.1", 0.9), 0.0, 1.0, 0.9));
					const double HideV = Safe(HideS, 0.2, 20.0, 1.8), LifeA = Safe(Life.A, 0.2, 60.0, 5.0), LifeB = std::max(LifeA, Safe(Life.B, 0.2, 60.0, 8.0));
					const double R1A = Safe(R1.A, 0.01, 2.0, 0.45), R1B = std::max(R1A, Safe(R1.B, 0.01, 2.0, 0.65));
					const double ReachV = Safe(Reach, -0.3, 3.0, 0.9), HazeV = Safe(Haze, 0.0, 0.95, 0.18), VeilK = Safe(Veil, 0.0, 4.0, 1.0);
					const Vec MiddleV = Foot + FwFlat * (Ring_ * 0.3) + ZUp * 1.0;
					for (int I = 0; I < NLumps; ++I)
					{
						const bool bLow = I % 2 == 0;
						const double Forth = Ring_ * DV.U(-0.3, ReachV);
						const double Wide = DV.Bell() * Ring_ * 0.4;
						const double High = bLow ? 0.0 : DV.U();
						const bool bTuft = DV.U() < TuftV;
						const double Thick = ThickA + (ThickB - ThickA) * DV.U();
						Bit B;
						B.Born = Now + DV.U(0.0, 0.06);
						B.From = Foot + FwFlat * Forth + Across * Wide + ZUp * (bLow ? 0.0 : UpA + (UpB - UpA) * High);
						B.Vel = (FwFlat * 1.0 + Across * (Wide / std::max(Ring_, 1.0))).Unit(FwFlat) * DV.U(2.0, 5.0) + ZUp * (bLow ? 0.0 : DV.U(0.3, 1.5));
						B.Drag = bLow ? 1.5 : 2.0;
						B.Rise = bLow ? 0.0 : Safe(Rise.Pick(DV), 0.0, 5.0, 0.3);
						B.Windage = Sand.Windage;
						B.Life = bTuft ? LifeA + (LifeB - LifeA) * DV.U() : HideV * DV.U(1.0, 1.4) * (1.0 - 0.3 * High);
						B.R0 = 0.1 * Ring_;
						B.R1 = Ring_ * (R1A + (R1B - R1A) * DV.U()) * (SizeA + (SizeB - SizeA) * DV.U());
						B.Grow = 0.3;
						if (bLow)
						{
							const double Heading = DV.U(0.0, 2.0 * Pi);
							B.Along = Vec(std::cos(Heading), std::sin(Heading), 0.0);
							B.Stretch = DV.U(1.3, 1.8);
						}
						B.Col = Sand.Low * DV.U(0.92, 1.06);
						B.Col2 = Sand.High * DV.U(0.95, 1.05);
						B.Alpha = std::min(0.95, Safe(Alpha.Pick(DV), 0.0, 0.95, 0.8) * VeilK * Thick * (1.0 - PaleUpV * High));
						B.FadeIn = 0.04;
						B.Hold = bTuft ? 0.25 : 0.5;
						if (bTuft)
						{
							B.HazeBy = HideV;
							B.HazeAlpha = std::min(B.Alpha, HazeV * VeilK);
						}
						B.Heart = BillowHeart;
						B.Inner = BillowInner;
						B.Rough = BillowRough;
						B.LumpSeed = static_cast<uint32_t>(DV.U() * 1073741823.0);
						B.Turn = DV.U(0.0, 2.0 * Pi);
						B.Centre = MiddleV;
						B.CloudShade = BillowShade;
						B.CloudR = Ring_ * 0.4;
						B.CentreRise = 0.2;
						B.Rest = BillowRest;
						B.Thin = 2;
						B.Origin = At - FwFlat * 1.5; // (the gun's body, behind its muzzle)
						Bits.push_back(B);
					}
				}
				for (int I = 0; I < NVeil && NLumps == 0; ++I)
				{
					const double Forth = Ring_ * D.U(-0.3, Reach);
					const double Wide = D.Bell() * Ring_ * 0.4;
					Bit B;
					B.Form = Bit::Shape::Upright;
					B.bHug = true;
					B.Born = Now + D.U(0.0, 0.06);
					B.From = Foot + FwFlat * Forth + Across * Wide;
					B.Vel = (FwFlat * 1.0 + Across * (Wide / std::max(Ring_, 1.0))).Unit(FwFlat) * D.U(2.0, 5.0);
					B.Drag = 1.5;
					B.Rise = Rise.Pick(D);
					B.Windage = Sand.Windage;
					B.Life = Life.Pick(D);
					B.R0 = 0.15 * Ring_;
					B.R1 = Ring_ * R1.Pick(D);
					B.Grow = 0.25;
					B.Along = FwFlat;
					B.Stretch = 1.3;
					B.Flatten = Flatten;
					B.Col = Sand.Low * D.U(0.95, 1.05);
					B.Col2 = Sand.High * D.U(0.95, 1.05);
					B.Alpha = std::min(0.95, Alpha.Pick(D) * Veil);
					B.FadeIn = 0.04;
					B.Hold = 0.25;
					B.HazeBy = HideS;
					B.HazeAlpha = Haze * Veil;
					B.Heart = 0.7;
					B.Inner = 0.6;
					B.Rough = 0.3;
					B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
					B.Turn = D.U(0.0, 2.0 * Pi);
					B.Thin = 2;
					B.Origin = At - FwFlat * 1.5; // (the gun's body, behind its muzzle)
					if (I % 2 == 0)
					{
						// (every other one a low dense skirt that stays on the ground, so the gun's foot is hidden too while
						// the billows above climb)
						B.Rise = 0.0;
						B.Flatten = Sand.LowFlatten;
						B.R1 *= Sand.LowWide;
						B.Heart = 0.85;
						B.Inner = 0.7;
						B.Rim = 0.2;
					}
					Bits.push_back(B);
				}
			}
		}
	}

	bool Battle::HotOf(const Vec& At, HotFlash& H) const
	{
		if (!Has("muzzle.hot.within_m") || !bEyeKnown || (LastEye - At).Length() > Num("muzzle.hot.within_m", 50.0))
		{
			return false;
		}
		H.Root = Picture("muzzle.hot.root", Rgb::FromPicture(255, 250, 232));
		H.Root2 = Picture("muzzle.hot.root_edge", Rgb::FromPicture(255, 240, 190));
		H.Hot = Picture("muzzle.hot.hot", Rgb::FromPicture(255, 232, 150));
		H.Orange = Picture("muzzle.hot.orange", Rgb::FromPicture(250, 140, 30));
		H.Rim = Picture("muzzle.hot.rim", Rgb::FromPicture(230, 96, 16));
		H.bRagged = Has("muzzle.hot.tongues.0");
		if (H.bRagged)
		{
			H.Yellow = Picture("muzzle.hot.yellow", Rgb::FromPicture(240, 200, 90));
			H.Red = Picture("muzzle.hot.red", Rgb::FromPicture(200, 80, 30));
			H.Tongues = Range("muzzle.hot.tongues", 3.0, 5.0);
			H.Licks = Range("muzzle.hot.licks", 2.0, 4.0);
			H.Long = Range("muzzle.hot.long", 0.6, 1.3);
			H.Wide = std::clamp(Num("muzzle.hot.wide", 0.2), 0.02, 1.0);
			H.Glow = Clamp01(Num("muzzle.hot.glow", 0.0));
			H.GlowDay = Clamp01(Num("muzzle.hot.glow_day_share", 0.5));
			H.GlowM = std::max(0.0, Num("muzzle.hot.glow_m", 0.035));
			H.Over = std::max(1.0, Num("muzzle.hot.over", 1.0));
			H.YellowOver = std::max(0.0, Num("muzzle.hot.yellow_over", 0.0));
			H.Halo = std::min(0.25, std::max(0.0, Num("muzzle.hot.halo", 0.0)));
			H.HaloM = std::min(0.5, std::max(0.0, Num("muzzle.hot.halo_m", 0.0))); // (never a card wider than a flash: a wild number stays a small glow)
		}
		return true;
	}

	void Battle::HotFlame(double Born, const Vec& At, const Vec& Fw, double Len, double Life, const HotFlash& H, Dice& D)
	{
		// Art director 13:54Z on the game's own frames (build 1720), where the close flash read as a sticker: one flat
		// orange fill with a cut edge and a white oval laid on it. Now, back to front: a thin glow; three to five ragged
		// tongues the body breaks into, orange with red-orange ends; the orange body, widest a third of the way out,
		// its outline lumpy; a yellow inside it and a small white-yellow root right at the muzzle, each melting into
		// the one under it; and two to four short licks thrown sideways. Dense inside with an edge a pixel or two soft.
		// Its length (muzzle.hot.long) and every piece are rolled anew, so no two flashes match.
		const double L = Len * std::max(0.1, H.Long.Pick(D));
		const double W = Len * H.Wide * D.U(0.85, 1.15) * (0.55 + 0.45 * L / std::max(Len, 1e-6)); // (half its width where it is widest: a short flame is a thinner one)
		const Vec Side = Fw.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
		const Vec Over_ = Side.Cross(Fw).Unit(ZUp);
		auto Leaning = [&](double Degrees, double Round_)
		{
			const double A = Radians(Degrees);
			return (Fw * std::cos(A) + (Side * std::cos(Round_) + Over_ * std::sin(Round_)) * std::sin(A)).Unit(Fw);
		};
		// (one piece: an oval from From_ along Way, HalfLong and HalfWide, shrinking a little as it dies)
		auto Piece = [&](const Vec& Mid, const Vec& Way, double HalfLong, double HalfWide, const Rgb& Col, const Rgb& Edge, double EdgeK_, double EdgeIn_, double Inner, double Rim, double Nearer)
		{
			Bit B;
			B.Born = Born;
			B.Life = Life;
			B.From = Mid;
			B.Along = Way;
			B.R0 = std::max(HalfWide, 1e-3);
			B.R1 = B.R0 * 0.8;
			B.Grow = 1.0;
			B.Stretch = std::max(1.0, HalfLong / B.R0);
			B.Col = Col;
			B.Col2 = Col;
			B.Alpha = 1.0;
			B.FadeIn = 0.002;
			B.Hold = 0.8;
			B.Heart = 1.0;
			B.Rim = Rim;
			B.Inner = Inner;
			B.Edge = Edge;
			B.EdgeK = EdgeK_;
			B.EdgeIn = EdgeIn_;
			B.Nearer = Nearer;
			B.bLight = true;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			return B;
		};
		if (H.Glow > 0.0 && H.GlowM > 0.0)
		{
			// (the glow: thin, a few centimetres past the flame all round, and by day a share of what it is at dusk)
			// (round the body alone: round the tongues too, it was a see-through wash between them)
			Bit G = Piece(At + Fw * (L * 0.33), Fw, L * 0.36 + H.GlowM, W + H.GlowM, H.Orange, H.Orange, 0.0, 0.0, std::clamp(W / (W + H.GlowM), 0.1, 0.95), 0.0, -0.5 * W);
			G.Alpha = H.Glow;
			G.DayAlpha = H.GlowDay;
			Bits.push_back(G);
		}
		const int NTongues = std::max(1, static_cast<int>(std::lround(H.Tongues.Pick(D))));
		const double Round0 = D.U(0.0, 2.0 * Pi);
		for (int I = 0; I < NTongues; ++I)
		{
			// (the tongues: fanned a little off the bore all round it, the longest reaching the flame's whole length)
			const Vec Way = Leaning(I == 0 ? D.U(0.0, 4.0) : D.U(5.0, 16.0), Round0 + (I + D.U(-0.3, 0.3)) * 2.0 * Pi / NTongues);
			const double S0 = L * D.U(0.25, 0.36), S1 = L * (I == 0 ? 1.0 : D.U(0.7, 0.98));
			const double HalfWide = W * D.U(0.2, 0.34);
			// (art director 14:26Z: ragged and blunt at its end, as gas is; a needle point read as a drawn comic flame)
			const double Point = 0.6, HalfLong = (S1 - S0) / (2.0 + Point);
			Bit T = Piece(At + Way * (S0 + HalfLong), Way, HalfLong, HalfWide, H.Orange, H.Red, 0.9, 0.45, 0.85, 0.3, 0.0);
			T.Point = Point;
			T.Rough = 0.3;
			Bits.push_back(T);
		}
		{
			Bit Body = Piece(At + Fw * (L * 0.29), Fw, L * 0.3, W, H.Orange, H.Orange.Mix(H.Red, 0.5), 0.7, 0.2, 0.85, 0.15, 0.6 * W);
			Body.Point = 0.45;
			Body.Rough = 0.22;
			Bits.push_back(Body);
			Bit Yellow = Piece(At + Fw * (L * 0.23), Fw, L * 0.23, W * 0.62, H.Yellow, H.Orange, 1.0, 0.3, 0.55, 0.0, 1.2 * W);
			Yellow.Rough = 0.15;
			Yellow.Over = H.YellowOver > 0.0 ? std::max(1.0, H.YellowOver) : 1.0 + (H.Over - 1.0) * 0.35;
			Bits.push_back(Yellow);
			Bit Root = Piece(At + Fw * (L * 0.1), Fw, L * 0.11, W * 0.34, H.Root, H.Yellow, 1.0, 0.3, 0.6, 0.0, 1.8 * W);
			Root.Over = H.Over;
			if (H.Halo > 0.0 && H.HaloM > 0.0)
			{
				// (the root's glow: a soft halo of the root's own colour under it, that opaque at the root's edge and fading
				// to nothing halo_m out. A copy of the root, so the dice fall as they did and every flash keeps its shape)
				Bit Halo = Root;
				const double RootWide = Root.R0, RootLong = Root.R0 * Root.Stretch;
				Halo.R0 = RootWide + H.HaloM;
				Halo.R1 = Halo.R0 * 0.8;
				Halo.Stretch = std::max(1.0, (RootLong + H.HaloM) / Halo.R0);
				Halo.Edge = H.Root;
				Halo.EdgeK = 0.0;
				Halo.EdgeIn = 0.0;
				Halo.Inner = std::clamp(RootWide / Halo.R0, 0.1, 0.95);
				Halo.Alpha = H.Halo;
				Halo.Over = std::min(H.Over, 2.0);
				Halo.Nearer = 1.5 * W;
				Bits.push_back(Halo);
			}
			Bits.push_back(Root);
		}
		const int NLicks = std::max(0, static_cast<int>(std::lround(H.Licks.Pick(D))));
		for (int I = 0; I < NLicks; ++I)
		{
			// (the licks: short, thrown out of the body's flanks well off the bore)
			const Vec Way = Leaning(D.U(35.0, 70.0), D.U(0.0, 2.0 * Pi));
			const double Long = Len * D.U(0.14, 0.24), HalfWide = W * D.U(0.16, 0.28);
			Bit K = Piece(At + Fw * (L * D.U(0.14, 0.42)) + Way * (W * 0.55 + Long * 0.3), Way, Long * 0.42, HalfWide, H.Orange, H.Red, 0.9, 0.35, 0.82, 0.3, 0.3 * W);
			K.Point = 0.5;
			K.Rough = 0.3;
			Bits.push_back(K);
		}
	}

	bool Battle::CaseRowOf(const std::string& Gun, const Vec& At, CaseRow& C) const
	{
		const std::string P = "ground_cases." + Gun + ".";
		if (Gun.empty() || !Has(P + "case_m") || !bEyeKnown || (LastEye - At).Length() > Num("ground_cases.within_m", 40.0))
		{
			return false;
		}
		C.M = std::max(0.005, Num(P + "case_m", 0.08));
		C.D = std::max(0.003, Num(P + "case_d_m", 0.012));
		C.After = std::max(0.0, Num(P + "after_s", 0.0));
		C.Back = Num(P + "breech_back_m", 0.0);
		C.Below = Num(P + "below_m", 0.0);
		C.Side = Num(P + "side", 1.0);
		C.Scatter = std::clamp(Num(P + "scatter_mps", 0.25), 0.0, 5.0);
		C.Throw = Range(P + "throw_mps", 2.0, 4.0);
		C.Up = Range(P + "up_mps", 1.0, 2.5);
		C.Lie = Range(P + "lie_s", 2.0, 4.0);
		C.Tumble = Range(P + "tumble", 15.0, 30.0);
		C.Glint = Num("ground_cases.glint", 4.0);
		C.LeastPx = Num("ground_cases.least_px", 0.8);
		C.Col = Picture("ground_cases.colour", Rgb::FromPicture(196, 156, 82));
		C.Dark = Picture("ground_cases.shade", Picture("ground_cases.dark", Rgb::FromPicture(112, 82, 37)));
		return true;
	}

	void Battle::GroundCase(double T, const Vec& Breech, const Vec& Away, const CaseRow& C, Dice& D)
	{
		// One spent case: a brass cylinder thrown out, tumbling end over end under gravity, landing and lying in the dirt
		// (its axis level), then gone; drawn as a cylinder's outline from wherever the eye is, glinting as it turns
		Bit B;
		B.Form = Bit::Shape::Plate;
		B.bThrown = true;
		B.Born = T;
		B.From = Breech;
		B.From = Breech - ZUp * C.Below;
		B.Vel = Away * C.Throw.Pick(D) + ZUp * C.Up.Pick(D) + Vec(D.Bell(), D.Bell(), 0.0) * C.Scatter;
		B.Gravity = 9.8;
		B.R0 = B.R1 = 0.5 * C.D;
		B.Stretch = std::max(1.0, C.M / C.D);
		const double Yaw = D.U(0.0, 2.0 * Pi);
		B.Along = Vec(std::cos(Yaw), std::sin(Yaw), 0.0);
		B.Face = ZUp;
		Throw(B);
		// (lying on its side, its middle a case's radius off the ground)
		B.At1.Z += 0.5 * B.R0;
		B.At2.Z += 0.5 * B.R0;
		// (so many half turns that it comes to rest lying level)
		const double Stop = std::max(B.Land2, 1e-3);
		const double Spin = C.Tumble.Pick(D);
		B.Tumble = (D.U() < 0.5 ? -1.0 : 1.0) * std::max(1.0, std::round(Spin * Stop / Pi)) * Pi / Stop;
		double Lie = std::max(0.1, C.Lie.Pick(D));
		if (Lie > 10.0)
		{
			// (a gun's pile or a machine gun's heap grows to most_lying cases; past that a new one lies only a moment)
			CaseUntil.erase(std::remove_if(CaseUntil.begin(), CaseUntil.end(), [T](double Until) { return Until < T; }), CaseUntil.end());
			if (CaseUntil.size() >= MostLying)
			{
				Lie = D.U(2.0, 3.0);
			}
			else
			{
				CaseUntil.push_back(T + Stop + Lie);
			}
		}
		B.Life = Stop + Lie;
		B.Hold = std::clamp((B.Life - 1.0) / B.Life, 0.0, 0.98); // (fading over its last second)
		B.Col = C.Col * D.U(0.9, 1.1);
		B.Col2 = C.Dark;
		B.Alpha = 1.0;
		B.FadeIn = 0.01;
		B.Heart = 1.0;
		B.Rim = 0.9;
		B.Inner = 0.5;
		B.Glint = C.Glint;
		B.LeastPx = C.LeastPx;
		B.LeastKeep = 0.6;
		Bits.push_back(B);
	}

	const Battle::DustRow& Battle::DustRowOf(const std::string& Ground)
	{
		for (const DustRow& R : DustRows)
		{
			if (R.Ground == Ground)
			{
				return R;
			}
		}
		const std::string P = "dust.vehicles.";
		DustRow R;
		R.Ground = Ground;
		R.Share = std::max(0.0, Num(P + "by_ground." + Ground, 0.0));
		R.LeastMps = Num(P + "least_mps", 1.0);
		R.FullMps = std::max(R.LeastMps + 0.1, Num(P + "full_mps", 12.0));
		R.Spacing = std::max(0.3, Num(P + "spacing_m", 2.5));
		R.MostPerS = std::max(0.5, Num(P + "most_a_second", 4.0));
		R.Kick = Num(P + "kick", 0.1);
		R.Drag = Num(P + "drag", 1.0);
		R.Windage = Num(P + "windage", 1.0);
		R.Spread = Num(P + "spread_mps", 0.6);
		R.FlatSlow = Num(P + "flatten.0", 0.5);
		R.FlatFast = Num(P + "flatten.1", 0.75);
		R.HazeBy = Num(P + "haze_by_s", 2.0);
		R.HazeAlpha = Num(P + "haze_alpha", 0.2);
		R.Tracked = Num(P + "tracked", 1.3);
		R.LowShare = Clamp01(Num(P + "low_skirt.share", 0.5));
		R.LowFlatten = Num(P + "low_skirt.flatten", 0.35);
		R.LowWide = Num(P + "low_skirt.wide", 1.25);
		R.LowThick = Num(P + "low_skirt.thick", 1.2);
		R.Life = Range(P + "life_s", 4.0, 6.0);
		R.LifeFast = Range(P + "life_fast_s", 7.0, 10.0);
		R.R0 = Range(P + "r0_m", 0.5, 0.9);
		R.R1 = Range(P + "r1_m", 1.5, 2.5);
		R.R1Fast = Range(P + "r1_fast_m", 3.5, 5.0);
		R.RiseM = Range(P + "rise_m", 1.0, 3.0);
		R.Alpha = Range(P + "alpha", 0.35, 0.5);
		const Rgb GroundDust = PaintOf(Ground).Dust;
		R.Low = Picture(P + "low." + Ground, GroundDust);
		R.High = Picture(P + "high." + Ground, GroundDust * Num(P + "colour_from_ground", 1.25));
		DustRows.push_back(R);
		return DustRows.back();
	}

	void Battle::VehicleDust(double Now, int Id, const Vec& At, const Vec& Velocity, const char* GroundIn, bool bTracked)
	{
		// The Mediterranean set (03:35Z) and the art director (06:19Z): every vehicle moving on a desert field trails pale
		// fine dust from its dust sockets, the stony gravel's colour where it is thrown up and paler as it climbs; low
		// and long, taller and longer at speed, drifting on the one wind, lingering, never popping. Seen from a man's
		// eye a column's dust hides the vehicles behind its lead.
		const DustRow& Row = DustRowOf(GroundIn && *GroundIn ? GroundIn : "earth");
		Dusting* Du = nullptr;
		for (Dusting& Each : Dusts)
		{
			if (Each.Id == Id)
			{
				Du = &Each;
				break;
			}
		}
		if (!Du)
		{
			Dusts.push_back(Dusting());
			Du = &Dusts.back();
			Du->Id = Id;
		}
		if (Now < Du->Last || Now - Du->Last > 0.5)
		{
			// (new, or a long silence or time going back: start laying again from here)
			Du->Last = Now;
			Du->At = At;
			Du->Owed = 0.0;
			return;
		}
		const Vec From = Du->At;
		const double Then = Du->Last, Between = Now - Du->Last;
		const Vec Path = Vec(At.X - From.X, At.Y - From.Y, 0.0);
		Du->At = At;
		Du->Last = Now;
		const Vec Flat(Velocity.X, Velocity.Y, 0.0);
		const double Speed = Flat.Length();
		// (the fuller of the room and the dust's own share of it: dust alone never climbs to its stop and cuts a salvo
		// behind the columns. The pre-check, 07:10Z. A vehicle within full_within_m of the eye keeps its whole trail, so
		// the column in front of a man still hides what drives behind its lead; the far columns thin)
		const bool bNearEye = bEyeKnown && (LastEye - At).Length() < DustFullWithin;
		const double Busy = std::max(static_cast<double>(RoomAsked()), bNearEye ? 0.0 : static_cast<double>(DustInSight + DustSince) / DustRoom) / std::max(1, MaxPuffs);
		if (Row.Share <= 0.0 || Speed < Row.LeastMps || Speed < 1e-6 || Busy > 0.85)
		{
			Du->Owed = 0.0;
			return;
		}
		// (a puff every spacing_m along its path, no more than most_a_second; when the screen is filling, half as many)
		const double Fast = Clamp01((Speed - Row.LeastMps) / (Row.FullMps - Row.LeastMps));
		double Spacing = std::max(Row.Spacing, Speed / Row.MostPerS);
		if (Busy > 0.6)
		{
			Spacing *= 2.0;
		}
		const double Long = Path.Length();
		const Vec Ahead = Flat * (1.0 / Speed);
		const Vec Side(-Ahead.Y, Ahead.X, 0.0);
		const double Big = (bTracked ? Row.Tracked : 1.0) * (0.7 + 0.3 * std::min(1.0, Row.Share));
		Dice D(++Seeds * 67867967ull ^ PlaceKey(At));
		double Done = Spacing - std::min(Du->Owed, Spacing);
		for (int Laid = 0; Done <= Long && Laid < 16; Done += Spacing, ++Laid)
		{
			const double K = Long > 1e-9 ? Done / Long : 1.0;
			Bit B;
			B.Form = Bit::Shape::Upright;
			B.bHug = true;
			B.Born = Then + Between * K;
			B.From = From + Path * K;
			B.Vel = Flat * Row.Kick + Side * (D.Bell() * Row.Spread);
			B.Drag = Row.Drag;
			B.Windage = Row.Windage;
			B.Life = Row.Life.Pick(D) * (1.0 - Fast) + Row.LifeFast.Pick(D) * Fast; // (at full speed within life_fast_s, never past it)
			B.Rise = Row.RiseM.Pick(D) * (0.5 + 0.5 * Fast) / B.Life;
			B.R0 = Row.R0.Pick(D) * Big;
			B.R1 = (Row.R1.Pick(D) * (1.0 - Fast) + Row.R1Fast.Pick(D) * Fast) * Big;
			B.Grow = 0.4;
			B.Along = Ahead;
			B.Stretch = 1.3 + 0.5 * Fast;
			B.Flatten = Row.FlatSlow + (Row.FlatFast - Row.FlatSlow) * Fast;
			B.Col = Row.Low * D.U(0.95, 1.05);
			B.Col2 = Row.High * D.U(0.95, 1.05);
			B.Alpha = std::min(0.9, Row.Alpha.Pick(D) * std::min(1.0, Row.Share) * (bTracked ? 1.0 : 0.8));
			B.FadeIn = 0.15;
			B.Hold = 0.3;
			B.HazeBy = Row.HazeBy;
			B.HazeAlpha = Row.HazeAlpha * std::min(1.0, Row.Share);
			B.Heart = 0.6;
			B.Inner = 0.55;
			B.Rough = 0.25;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.Thin = 1;
			if (D.U() < Row.LowShare)
			{
				// (a low dense skirt along the ground, wider than the billows: the trail's body, thick from the ground to
				// a vehicle's height, so what drives in it is hidden; the billows above it are the soft tall part)
				B.Flatten = Row.LowFlatten;
				B.R1 *= Row.LowWide;
				B.Alpha = std::min(0.95, B.Alpha * Row.LowThick);
				B.HazeAlpha = std::min(B.Alpha, B.HazeAlpha * Row.LowThick);
				B.Heart = 0.85;
				B.Inner = 0.7;
				B.Rim = 0.2;
			}
			Bits.push_back(B);
			++DustSince;
		}
		Du->Owed = std::fmod(Du->Owed + Long, Spacing);
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
		const double Life = Num("tracer.life_s", 0.6);
		const Rgb Flame = FlameCol;
		const Vec Line = To - From;
		const double Dist = Line.Length();
		const Vec Dir = Line.Unit(Vec(1.0, 0.0, 0.0));
		const bool bFlicker = Weapon && std::string(Weapon) == "mg";
		HotFlash HotMg;
		const bool bHotMg = bFlicker && HotOf(From, HotMg);
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
		// (art director: the grit up close. Within ground_cases.within_m of the eye a machine gun throws its spent brass out
		// of its right side every round, flicking, tumbling and glinting, to lie a moment in the dirt beside it. Its own
		// dice, so the rounds and strikes fall as they always did)
		CaseRow Cases;
		const bool bCases = CaseRowOf(Weapon ? Weapon : "", From, Cases);
		Dice DC(PlaceKey(From) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)));
		const Vec CaseOut = (Dir.Cross(ZUp).Unit(Vec(0.0, -1.0, 0.0)) * Cases.Side - Dir * 0.3).Unit(Vec(0.0, -1.0, 0.0));
		// (the close flame and the burst's smoke have dice of their own too)
		Dice DF(PlaceKey(From) * 0xD6E8FEB86659FD93ull ^ (static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) + 0x51ull));
		const double MgFlame = Num("muzzle.hot.mg_flame_m", 0.5);
		// (art director 13:54Z: close to the eye, a burst of three rounds or more leaves a thin pale grey-brown wisp at
		// the muzzle, drifting off on the wind for a second or two, and on dry ground a low puff of the ground's own
		// dust kicked up just in front of the muzzle. Never more than a quarter opaque, however long the burst)
		const bool bWisp = bHotMg && Rounds >= 3 && Has("mg_smoke.alpha");
		const double WispAlpha = std::clamp(Num("mg_smoke.alpha", 0.25), 0.0, 0.25);
		const int WispEvery = std::max(1, static_cast<int>(Num("mg_smoke.every_rounds", 4.0)));
		const Span WispLife = Range("mg_smoke.life_s", 1.0, 2.0);
		const Rgb WispCol = Picture("mg_smoke.colour", Rgb::FromPicture(168, 158, 142));
		// (puff_share: one puff's share of alpha. Art director 16:30Z: where its puffs overlap the wisp reaches its cap)
		const double WispShareIs = Num("mg_smoke.puff_share", 0.55);
		const double WispShare = WispShareIs == WispShareIs ? std::min(1.0, std::max(0.1, WispShareIs)) : 0.55; // (not a number: the usual share)
		const double DustShare = bWisp && From.Z - Height(From.X, From.Y) < Num("mg_smoke.dust_under_m", 1.2) ? Clamp01(DustRowOf(Ground).Share) : 0.0;
		for (int R = 0; R < Rounds; ++R)
		{
			const double T = Now + R * Gap;
			const int Which = Count++;
			if (bWisp && R % WispEvery == WispEvery - 1)
			{
				// (overlapping two or three deep as they drift, so each is under half the most the whole may be)
				Bit B;
				B.Born = T;
				B.From = From + Dir * DF.U(0.25, 0.45);
				B.Vel = Dir * DF.U(1.5, 3.0) + Vec(DF.Bell() * 0.3, DF.Bell() * 0.3, DF.U(0.2, 0.6));
				B.Drag = 3.0;
				B.Rise = 0.25;
				B.Windage = 1.0;
				B.Life = WispLife.Pick(DF);
				B.R0 = 0.08;
				B.R1 = DF.U(0.35, 0.6);
				B.Grow = 0.6;
				B.Col = WispCol;
				B.Col2 = WispCol;
				B.Alpha = WispAlpha * WispShare;
				B.FadeIn = 0.06;
				B.Hold = 0.25;
				B.Heart = 0.8;
				B.Rim = 0.0;
				B.Rough = 0.3;
				B.LumpSeed = static_cast<uint32_t>(DF.U() * 1073741823.0);
				Bits.push_back(B);
				if (DustShare > 0.0)
				{
					Bit Du = B;
					Du.From = From + Dir * DF.U(0.6, 1.1);
					Du.From.Z = Height(Du.From.X, Du.From.Y) + 0.08;
					Du.Vel = Dir * DF.U(0.6, 1.4) + Vec(DF.Bell() * 0.4, DF.Bell() * 0.4, DF.U(0.1, 0.3));
					Du.Rise = 0.0;
					Du.Windage = 0.8;
					Du.R0 = 0.1;
					Du.R1 = DF.U(0.4, 0.7);
					Du.Col = Dust;
					Du.Col2 = Dust;
					Du.Alpha = WispAlpha * 0.55 * DustShare;
					Du.LumpSeed = static_cast<uint32_t>(DF.U() * 1073741823.0);
					Bits.push_back(Du);
				}
			}
			if (bCases && RoomForCases() < static_cast<size_t>(MaxPuffs) * 3 / 5)
			{
				GroundCase(T + Cases.After, From - Dir * Cases.Back, CaseOut, Cases, DC);
			}
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
			if (bHotMg && HotMg.bRagged)
			{
				FlameDue Due;
				Due.Born = T;
				Due.At = From;
				Due.Fw = Dir;
				Due.Len = MgFlame;
				Due.Life = 0.025;
				Due.H = HotMg;
				Due.Seed = static_cast<uint64_t>(DF.U() * 9.0e15) + 1ull;
				FlamesDue.push_back(Due);
			}
			else if (bFlicker)
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
				if (bHotMg)
				{
					// (art director 07:08Z: lit from within close to the eye, never a soft orange ball; the body out ahead of
					// the muzzle, its hot root inside it at the muzzle)
					B.From = From + Dir * 0.24;
					B.R0 = 0.12;
					B.R1 = 0.085;
					B.Tail = 0.0;
					B.Col = HotMg.Orange.Mix(HotMg.Hot, 0.25);
					B.Col2 = HotMg.Orange;
					B.Alpha = 1.0;
					B.Heart = 0.95;
					B.Rim = 0.85;
					B.Inner = 0.7;
					B.Edge = HotMg.Rim;
					B.EdgeK = 0.4;
					B.Hold = 0.8;
				}
				Bits.push_back(B);
				if (bHotMg)
				{
					Bit Root_ = B;
					Root_.From = From + Dir * 0.14;
					Root_.R0 = Root_.R1 = 0.045;
					Root_.Stretch = 2.4;
					Root_.Point = 0.6;
					Root_.Col = HotMg.Root;
					Root_.Col2 = HotMg.Root2;
					Root_.Heart = 1.0;
					Root_.Rim = 0.85;
					Root_.Inner = 0.6;
					Root_.Edge = HotMg.Hot;
					Root_.EdgeK = 0.35;
					Root_.Nearer = 0.2;
					Bits.push_back(Root_);
				}
			}
			if (Which % OneIn != 0)
			{
				continue;
			}
			const Vec Aim = (Dir + Vec(D.Bell(), D.Bell(), D.Bell()) * 0.006).Unit();
			if (Dist < WholePathBelow)
			{
				// (close in, a tracer crosses in under a frame or two: its whole path is drawn at once, muzzle to strike,
				// fading toward the muzzle, for one frame (whole_path_s, under a frame at 60 a second; a frame is never
				// missed), so it is a whip and never a standing line; art director 21:16Z and 02:13Z)
				const double Third = Dist / 3.0;
				for (int K = 0; K < 3; ++K)
				{
					Tracer(T, From + Aim * (Third * K), Aim, Vec(), std::min(Reach, WholeFor), 0.0, 0.0, Third * 0.5, 0.35 + 0.325 * K, Third * K, Dist - Third * K);
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
				Streak(T + Reach, From + Aim * Dist, Off, Speed * Num("tracer.ricochet.slower", 0.5), std::max(0.15, Life - Reach)); // (from the strike point, never behind it)
			}
		}
	}

	void Battle::Tracer(double Born, const Vec& Start, const Vec& Way, const Vec& Vel, double For, double Drag, double Drop, double Half, double AlphaK, double Behind, double Until)
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
			B.Origin = Start - Way * Behind; // (a piece of a whole path: the muzzle is Behind metres back from its start)
			B.bFired = true;
			B.Until = Until > 0.0 ? Behind + Until : 0.0;
			B.Along = Way;
			B.R0 = B.R1 = Layer == 0 ? 0.045 : 0.018;
			B.LeastPx = Layer == 0 ? TracerLeast : TracerLeast * 0.4;
			B.MostPx = Layer == 0 ? TracerMost : TracerMost * 0.4;
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
			B.Nearer = Layer == 1 && TracerDayPx > 0.0 ? 0.01 : 0.0; // (solid by day, the core must lie over its edge)
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
			R.FlashLeastPx = std::max(0.0, Num(P + "flash_least_px", Num("air_guns.flash_least_px", 0.0)));
			// (the close view: what only shows within a few tens of metres of the eye)
			const std::string C = "air_guns.close.";
			R.CloseLicks = Range(P + "close_licks", 0.0, 0.0);
			R.CloseLamp = std::max(0.0, Num(P + "close_lamp", 0.0));
			R.LicksWithin = std::max(0.0, Num(C + "licks_within_m", 30.0));
			R.CasesWithin = std::max(0.0, Num(C + "cases_within_m", 60.0));
			R.CaseM = std::max(0.0, Num(P + "case_m", 0.0));
			R.CaseD = std::max(0.002, Num(P + "case_d_m", 0.02));
			R.LinkM = std::max(0.0, Num(P + "link_m", 0.0));
			R.ChuteBack = Num(C + "chute_back_m", 1.0);
			R.ChuteDown = Num(C + "chute_down_m", 0.25);
			R.CaseDrag = std::max(0.1, Num(C + "drag", 5.0));
			R.CaseGlint = std::max(0.0, Num(C + "glint", 4.0));
			R.CaseLeastPx = std::max(0.0, Num(C + "least_px", 0.8));
			R.Throw = Range(C + "throw_mps", 3.0, 8.0);
			R.CaseSink = Range(C + "case_sink_mps", 18.0, 28.0);
			R.LinkSink = Range(C + "link_sink_mps", 12.0, 20.0);
			R.CaseLife = Range(C + "life_s", 1.2, 2.0);
			R.CaseTumble = Range(C + "tumble", 15.0, 40.0);
			R.CaseCol = Picture(C + "case_colour", Rgb::FromPicture(196, 156, 82));
			R.CaseDark = Picture(C + "case_dark", Rgb::FromPicture(140, 104, 52));
			R.LinkCol = Picture(C + "link_colour", Rgb::FromPicture(70, 70, 72));
			R.bHotFlash = Num(P + "hot_flash", 0.0) > 0.0;
			R.FlashHot = Picture("air_guns.flash_hot", Rgb::FromPicture(255, 232, 150));
			R.FlashOrange = Picture("air_guns.flash_orange", Rgb::FromPicture(250, 140, 30));
			R.FlashRim = Picture("air_guns.flash_rim", Rgb::FromPicture(230, 96, 16));
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
			B.LeastPx = Row.FlashLeastPx; // (art director 04:40Z: from the strategy camera a muzzle flash is a twinkle of 2 px, never lost)
			B.LeastKeep = 0.8;
			B.bLight = true;
			const bool bClose = bEyeKnown && (Row.CloseLicks.B > 0.0 || Row.CloseLamp > 0.0) && (LastEye - At).Length() < Row.LicksWithin;
			if (bClose)
			{
				// (art director 06:19Z: seen close, the flicker's body the same saturated orange as the licks, dense to its
				// edge so the sky behind never turns it peach)
				B.Col = Row.FlashOrange.Mix(Row.FlashHot, 0.2);
				B.Col2 = Row.FlashOrange;
				B.Heart = 0.95;
				B.Rim = 0.85;
				B.Inner = 0.7;
				B.Edge = Row.FlashRim;
				B.EdgeK = 0.4;
				B.R1 = B.R0 * 0.7; // (fading by shrinking, still dense)
				B.Hold = 0.8;
			}
			Bits.push_back(B);
			if (bClose)
			{
				// (art director 04:40Z, the close view: seen within 30 m a heavy machine gun's flicker throws none to two
				// short side licks, raked aft by the airstream, and a small lamp. 06:19Z: lit from within, a white-yellow root
				// at the muzzle over the flicker's first quarter and the licks' roots, an orange body, a fading tip)
				{
					Bit H;
					H.Born = T;
					H.From = At + Dir * (Len * 0.12);
					H.Vel = Carry;
					H.Life = B.Life;
					H.R0 = Len * 0.1;
					H.R1 = Len * 0.1;
					H.Along = Dir;
					H.Stretch = 1.6;
					H.Point = 0.3;
					H.Col = Rgb::FromPicture(255, 250, 230);
					H.Col2 = Row.FlashHot;
					H.Edge = Row.FlashHot;
					H.EdgeK = 0.5;
					H.Alpha = 1.0;
					H.FadeIn = 0.002;
					H.Hold = 0.8;
					H.Heart = 1.0;
					H.Rim = 0.7;
					H.Nearer = 0.5; // (over the flicker and the licks it roots)
					H.bLight = true;
					Bits.push_back(H);
				}
				const Vec Across = Dir.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
				const Vec Over = Across.Cross(Dir).Unit(ZUp);
				const int Licks = static_cast<int>(std::lround(std::clamp(Row.CloseLicks.Pick(D), 0.0, 8.0)));
				for (int I = 0; I < Licks; ++I)
				{
					const double A = D.U(0.0, 2.0 * Pi);
					const Vec Outward = (Across * std::cos(A) + Over * std::sin(A)).Unit(Across);
					const Vec Way = (Outward - Dir * D.U(0.3, 0.55)).Unit(Outward);
					const double R = Len * D.U(0.06, 0.085);
					Bit L;
					L.Born = T;
					L.Stretch = D.U(2.2, 2.8);
					L.From = At + Dir * (Len * 0.3) + Way * (R * L.Stretch * 0.8);
					L.Vel = Carry;
					L.Life = B.Life;
					L.R0 = R * 1.05;
					L.R1 = R * 0.6;
					L.Along = Way;
					L.Point = 0.7;
					L.Tail = 0.15;
					L.Col = Row.FlashOrange.Mix(Row.FlashHot, 0.3);
					L.Col2 = Row.FlashOrange; // (orange to the end, never darkening into a crease)
					L.Alpha = 0.95;
					L.FadeIn = 0.002;
					L.Hold = 0.8;
					L.Heart = 0.95;
					L.Rim = 0.75;
					L.Inner = 0.7;
					L.Edge = Row.FlashRim;
					L.EdgeK = 0.4;
					L.Rough = 0.12;
					L.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
					L.Turn = D.U(0.0, 2.0 * Pi);
					L.bLight = true;
					Bits.push_back(L);
				}
				if (Row.CloseLamp > 0.0)
				{
					Flash L;
					L.At = At + Dir * (Len * 0.5);
					L.Col = FlameCol;
					L.Born = T;
					L.Life = 0.03;
					L.Strength = Row.CloseLamp;
					L.Radius = 8.0;
					L.bNightOnly = true; // (as every small gun's: it lights the wing by night, nothing by day)
					Flashes.push_back(L);
				}
			}
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
		if (Row.CaseM > 0.0 && bEyeKnown && (LastEye - At).Length() < Row.CasesWithin && RoomForCases() < static_cast<size_t>(MaxPuffs) * 3 / 5)
		{
			Spent(T, At, Dir, Carry, Row, D); // (the close view only, and the first thing left out when the room fills)
		}
		// gun smoke: a thin grey wisp left hanging in the air where the gun was, so a firing wing trails a short
		// smoky line behind it
		if (bSmoke && Row.SmokeAlpha > 0.0 && RoomAsked() < static_cast<size_t>(MaxPuffs) * 4 / 5) // (the first thing shed when the room fills; a twin counts its two slots)
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

	void Battle::Spent(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, Dice& D)
	{
		// Art director 04:40Z, the close view: the spent case and its belt link from each round, out of the chute under
		// a wing gun (or thrown overboard from a turret), drawn only within 60 m. They leave at the aircraft's own
		// speed, kicked down and out, and the airstream tears them back at once, so a firing wing trails a glinting
		// spray of brass that streams aft and falls away; the dark steel links tumble with them.
		const Vec Across = Dir.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
		const Vec Chute = At - Dir * Row.ChuteBack - ZUp * Row.ChuteDown;
		for (int K = 0; K < (Row.LinkM > 0.0 ? 2 : 1); ++K)
		{
			Bit B;
			B.Form = Bit::Shape::Plate;
			B.Born = T + D.U(0.0, 0.008);
			B.From = Chute + Across * (D.Bell() * 0.04);
			const double Sink = std::max(1.0, (K == 0 ? Row.CaseSink : Row.LinkSink).Pick(D));
			const Vec Throw = Across * (D.Bell() * 1.5) - ZUp * Row.Throw.Pick(D) - Dir * D.U(0.0, 2.0);
			// (the air's drag: from the aircraft's speed toward falling at its own speed in a fraction of a second)
			B.Drag = Row.CaseDrag;
			B.Vel = Carry + Throw + ZUp * Sink;
			B.Rise = -Sink;
			B.Gravity = 0.0;
			B.Windage = 0.3;
			if (K == 0)
			{
				B.R0 = B.R1 = 0.5 * Row.CaseD;
				B.Stretch = std::max(1.0, Row.CaseM / Row.CaseD);
				B.Col = Row.CaseCol;
				B.Col2 = Row.CaseDark;
				B.Glint = Row.CaseGlint;
			}
			else
			{
				B.R0 = B.R1 = 0.35 * Row.LinkM;
				B.Stretch = 1.4;
				B.Rough = 0.2;
				B.Col = B.Col2 = Row.LinkCol;
				B.Glint = 0.3 * Row.CaseGlint;
			}
			B.Tumble = Row.CaseTumble.Pick(D) * (D.U() < 0.5 ? 1.0 : -1.0);
			B.Along = Vec(D.Bell(), D.Bell(), D.Bell()).Unit(Dir);
			B.Face = B.Along.Cross(Vec(D.Bell(), D.Bell(), D.Bell())).Unit(B.Along.Cross(ZUp).Unit(Across));
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.Life = Row.CaseLife.Pick(D);
			B.Alpha = 1.0;
			B.FadeIn = 0.005;
			B.Hold = 0.6;
			B.Heart = 1.0;
			B.Rim = 1.0;
			B.Inner = 0.6;
			B.ShadeK = 0.0;
			B.LeastPx = Row.CaseLeastPx;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			Bits.push_back(B);
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
		const bool bHot = Row.bHotFlash;
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
			if (bHot)
			{
				// (a dense body out to its edge: a see-through rim over the sky goes peach. Its second frame is about half by
				// shrinking, still dense, never by going see-through)
				B.Inner = 0.7;
				B.R0 = R * 1.05;
				B.R1 = R * 0.55;
				B.Hold = 0.8;
				B.Edge = Row.FlashRim;
				B.EdgeK = 0.45;
			}
			B.bLight = true;
			Bits.push_back(B);
		};
		// (art director 06:19Z, bHotFlash: the cannon's the bigger, hotter flash, a white-yellow core over a saturated orange
		// body that stays orange to its tip, near opaque, never pale peach smoke)
		const Rgb Orange = bHot ? Row.FlashOrange : FlameCol;
		// the orange edge round the heart: see-through in its middle, strongest at its heart ring, soft at its rim
		Lick(At + Dir * (Len * 0.14), Dir, Len * 0.3, 1.3, Orange, bHot ? Orange : FlameCol * 0.7, bHot ? 0.95 : 0.3, bHot ? 1.0 : 2.6, bHot ? 0.8 : 0.3, bHot ? 0.15 : 0.35);
		// the forward tongue: three ragged pieces, pale at the root and orange beyond, each a little off the line
		const double Reach = Len * D.U(1.1, 1.4);
		for (int K = 0; K < 3; ++K)
		{
			const double Along = Reach * (0.3 + 0.33 * K) * D.U(0.9, 1.1);
			const Vec Off = (Across * D.Bell() + Over * D.Bell()) * (Len * 0.05 * (K + 1));
			const Rgb Col = K == 0 ? (bHot ? Orange.Mix(Row.FlashHot, 0.4) : CannonHeartCol.Mix(FlameCol, 0.45)) : Orange;
			Lick(At + Dir * Along + Off, (Dir + Off * (0.6 / std::max(Len, 1e-3))).Unit(Dir), Len * (0.2 - 0.045 * K) * D.U(0.85, 1.15), 2.2, Col, bHot ? Orange : FlameCol * 0.75, bHot ? 1.0 : 0.9, bHot ? 1.0 : 0.8, bHot ? 0.75 : 0.15, bHot ? 0.2 : 0.4);
		}
		// short side licks, raked aft by the air the aircraft flies into; a third of the tongue or less
		for (int I = 0; I < Row.Licks; ++I)
		{
			const double A = (I * 2.0 * Pi) / std::max(1, Row.Licks) + D.U(-0.6, 0.6);
			const Vec Outward = (Across * std::cos(A) + Over * std::sin(A)).Unit(Across);
			const Vec Way = (Outward - Dir * D.U(0.3, 0.55)).Unit(Outward);
			const double R = Len * D.U(0.06, 0.085);
			const double Stretch = D.U(2.2, 2.8);
			Lick(At + Dir * (Len * 0.08) + Way * (R * Stretch * 0.8), Way, R, Stretch, Orange, bHot ? Orange : FlameCol * 0.6, bHot ? 1.0 : 0.85, bHot ? 1.0 : 0.8, bHot ? 0.6 : 0.1, bHot ? 0.12 : 0.3);
		}
		// the hot heart right at the muzzle, laid last so it is pale over the orange (and from the strategy camera never
		// under flash_least_px: a twinkle)
		Lick(At + Dir * (Len * 0.1), Dir, Len * (bHot ? 0.19 : 0.15), 1.4, bHot ? Rgb::FromPicture(255, 252, 238) : CannonHeartCol, bHot ? Rgb::FromPicture(255, 246, 205) : CannonHeartCol.Mix(FlameCol, 0.5), 1.0, bHot ? 1.0 : 0.95, bHot ? 0.7 : 0.35, 0.2);
		if (bHot)
		{
			Bits.back().Edge = Row.FlashHot;
			Bits.back().EdgeK = 0.35; // (art director 06:34Z: the cannon's core at least as white as the heavy machine gun's root)
			Bits.back().Nearer = 0.5; // (the white core over its own orange)
		}
		Bits.back().LeastPx = Row.FlashLeastPx;
		Bits.back().LeastKeep = 0.8;
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
		const double HitLeastPx = std::max(0.0, Num("air_hit.least_px", 0.0));
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
			B.LeastPx = HitLeastPx; // (art director 04:40Z: from the strategy camera a hit is a twinkle of 2 px, never lost)
			B.LeastKeep = 0.8;
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
			B.R1 = Num("air_hit.smoke_r1", 1.1) * S * D.U(0.8, 1.2);
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
		const double FaceLit = std::max(0.0, Num(P + "face_lit", 0.0));
		const Rgb LitCol = Picture(P + "lit_colour", Rgb::FromPicture(182, 176, 166));
		const double TwinPx = std::max(0.0, Num(P + "twin_px", 40.0));
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
			// (art director 00:03Z: each lobe its own lit weight, 0.6 to 1.0, so neighbours differ and the lit side breaks
			// into lumps; taken from its lump seed, so the throw of the burst's dice is as before)
			B.FaceLit = FaceLit * (0.6 + 0.4 * static_cast<double>((B.LumpSeed * 2654435761u) >> 22) / 1023.0);
			B.LitCol = LitCol;
			B.TwinPx = TwinPx;
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
		const Rgb GlowCol = Picture(P + "flash_glow", Rgb::FromPicture(160, 70, 34));
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
			B.bWhole = true; // (a slow frame shows the glow, spikes and heart together, never the white heart alone)
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
			R.ThinAfter = std::max(0.0, Num(P + "thin_after_s", 0.0));
			R.ThinKeep = static_cast<int>(std::clamp(Num(P + "thin_keep_one_in", 1.0), 1.0, 1000.0)); // (held: twice it must fit an int)
			R.ThinGrow = std::max(1.0, Num(P + "thin_grow", 1.0));
			R.ThinAlpha = std::max(0.0, Num(P + "thin_alpha", 1.0));
			R.ThinHold = Num(P + "thin_hold", -1.0);
			R.ThinRagged = Clamp01(Num(P + "thin_ragged", 0.0));
			R.LeastPx = std::max(0.0, Num(P + "least_px", 0.0));
			R.LeastKeep = Clamp01(Num(P + "least_keep", 0.0));
			R.Core = Has(P + "ribbon") && Num(P + "ribbon", 0.0) > 0.0 ? std::clamp(Num(P + "core", 0.15), 0.1, 0.9) : 0.0;
			R.JoinFrom = Clamp01(Num(P + "join_from", 0.15));
			R.JoinBy = std::max(R.JoinFrom + 0.01, Num(P + "join_by", 0.45));
			R.TurnCos = std::cos(std::clamp(Num(P + "keep_each_turn_deg", 26.0), 2.0, 90.0) * Pi / 180.0);
			R.LumpsFrom = Clamp01(Num(P + "lumps_from", 0.75));
			R.Drift = Clamp01(Num(P + "drift", 0.35));
			R.PaleTo = Clamp01(Num(P + "pale_to", 0.0));
			R.PaleFrom = std::max(0.0, Num(P + "pale_from_s", 0.0));
			R.PaleBy = std::max(R.PaleFrom + 0.1, Num(P + "pale_by_s", 2.5));
			R.Wander = std::clamp(Num(P + "wide_wander", 0.0), 0.0, 2.0);
			R.WidenBy = std::max(0.0, Num(P + "widen_by_s", 0.0));
			R.SmokeStart = std::max(0.0, Num(P + "smoke_start", 1.0));
			R.FlameLiftOfLen = Range(P + "flame_lift_of_length", 0.0, 0.0);
			R.LicksPerFrame = std::max(0.0, Num(P + "licks_a_frame", 0.4));
			if (Has(P + "flame_alpha") || Has(P + "flame_alpha.0"))
			{
				const Span FlameAlpha = Range(P + "flame_alpha", 0.92, 0.72);
				R.FlameAlphaRoot = Clamp01(FlameAlpha.A);
				R.FlameAlphaFall = R.FlameAlphaRoot - Clamp01(FlameAlpha.B);
			}
			R.FlameHold = Clamp01(Num(P + "flame_hold", 0.45));
			R.FlameWide = std::max(0.1, Num(P + "flame_wide", 1.0));
			R.LickAlpha = Clamp01(Num(P + "lick_alpha", 0.9));
			R.FlameLiftCurve = std::max(0.0, Num(P + "flame_lift_curve", 0.0));
			R.bFlameTip = Has(P + "flame_tip_colour.0");
			R.FlameTip = Picture(P + "flame_tip_colour", Rgb::FromPicture(120, 34, 18));
			R.TipSmoke = Clamp01(Num(P + "tip_smoke", 0.0));
			R.TipSmokeR = std::max(0.0, Num(P + "tip_smoke_r_of_flame", 0.3));
			R.TipSmokeLife = Range(P + "tip_smoke_s", 0.3, 0.6);
			R.FlameOfSpan = std::max(0.0, Num(P + "flame_of_span", 0.0));
			R.FlameMost = Num(P + "flame_most_m", R.Flame);
			R.FlameLeastPx = std::max(0.0, Num(P + "flame_least_px", 0.0));
			R.Col = Picture(P + "colour", Rgb::FromPicture(128, 126, 122));
			R.ColFull = Picture(P + "colour_full", R.Col);
			R.ColOld = Picture(P + "colour_old", R.ColFull);
		}
		R.Name = Kind;
		TrailRows.push_back(R);
		return TrailRows.back();
	}

	void Battle::TrailStart(double Now, int Id, const Vec& At, const Vec& Carry, const char* Kind, double SpanM)
	{
		const std::string Name = Kind && *Kind ? Kind : "smoke";
		for (Trailing& Tr : Trails)
		{
			if (Tr.Id == Id)
			{
				if (SpanM > 0.0)
				{
					Tr.Span = SpanM;
				}
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
					if (TrailRowOf(Tr.Kind).Core > 0.0)
					{
						// (a ribbon starts again from here: no piece reaches back over the break)
						Tr.bPuffAt = Tr.bKeptAt = false;
						Tr.Prev.bOn = Tr.Kept.bOn = Tr.bWander = false;
						Tr.Wide = 0.0;
						Tr.NextKept = Tr.Puffs;
					}
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
		Tr.Span = std::max(0.0, SpanM);
		Trails.push_back(Tr);
	}

	double Battle::FlameOf(const Trailing& Tr, const TrailRow& Row) const
	{
		// (art director 04:40Z: a burning plane's flame is half its span, at most 10 m; with no span given, the row's own)
		if (Row.FlameOfSpan > 0.0 && Tr.Span > 0.0 && Row.Flame > 0.0)
		{
			return std::max(0.5, std::min(Row.FlameMost, Row.FlameOfSpan * Tr.Span));
		}
		return Row.Flame;
	}

	void Battle::TrailPuff(double T, const Vec& At, const Vec& Carry, Trailing& Tr, const TrailRow& Row, Dice& D)
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
		const bool bRibbon = Row.Core > 0.0;
		B.Vel = Carry * Row.Carried + Vec(D.Bell(), D.Bell(), 0.5 * D.Bell()) * (3.0 * Row.Lumpy * (bRibbon ? Row.Drift : 1.0));
		B.Drag = 2.0;
		B.Rise = Row.Rise * D.U(0.6, 1.4);
		B.Windage = 1.0;
		B.Life = Row.Life * Lasts * D.U(0.85, 1.15);
		if (bRibbon)
		{
			// (a ribbon is one strip: each piece takes most of the drift and the rise of the piece before it, so the
			// strip meanders as it hangs and never zig-zags; and its pieces all last as long, so it thins and goes
			// from its old end, never a piece out of its middle)
			const Vec Kick = B.Vel - Carry * Row.Carried;
			const double RiseK = Row.Rise != 0.0 ? B.Rise / Row.Rise : 1.0;
			Tr.Wander = Tr.bWander ? Tr.Wander * 0.9 + Kick * 0.44 : Kick;
			Tr.RiseK = Tr.bWander ? 1.0 + (Tr.RiseK - 1.0) * 0.9 + (RiseK - 1.0) * 0.44 : RiseK;
			Tr.bWander = true;
			B.Vel = Carry * Row.Carried + Tr.Wander;
			B.Rise = Row.Rise * Tr.RiseK;
			B.Life = Row.Life * (1.0 - 0.15 * Row.Lumpy);
		}
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
		B.LeastPx = Row.LeastPx; // (art director 04:59Z: far off a trail stays a dark line, never fading to nothing)
		B.LeastKeep = Row.LeastKeep;
		if (bRibbon)
		{
			// (art director 14:13Z, build 1722: the trails read as beads on a string. Each puff was a lens drawn out behind
			// it, so only the thin faint tips of two met. A ribbon's piece is a length of one strip, as wide and as dark
			// at each end as the trail is there, from where this puff is laid back to the last one, wherever that has
			// drifted to; the two share that edge, however fast the aircraft flies or falls, and it stays a link of
			// the strip as it widens and pales, so no gap ever opens)
			B.Ribbon = Row.Core;
			B.SmearFor = 0.0;
			if (Row.Wander > 0.0)
			{
				// (art director 15:22Z, the falls read as pen strokes: its width wanders along its length, each piece
				// taking most of the last one's, so its edges swell and pinch in soft lumps over several pieces and it
				// is never a ruled line)
				Tr.Wide = Tr.Wide * 0.8 + 0.6 * Row.Wander * (Size - (1.0 - 0.09 * Row.Lumpy));
				B.EdgeSoft = 4.0;
				B.R1 = B.R0 + (B.R1 / Size - B.R0) * std::clamp(1.0 + Tr.Wide, 0.5, 1.7);
			}
			B.SmearHold = Row.LumpsFrom; // (only toward its old end does it shorten to a lump)
			if (Row.PaleTo > 0.0)
			{
				// (art director 15:22Z: it pales with age; the length that hangs on under it takes the same)
				B.PaleTo = Row.PaleTo;
				B.PaleFrom = Row.PaleFrom;
				B.PaleBy = Row.PaleBy;
			}
			B.Smear = 3.0 * Row.Spacing; // (the most it reaches back: for what is off the screen, and the sun through its edge)
			if (Tr.bPuffAt && (At - Tr.PuffAt).Length() < 3.0 * Row.Spacing)
			{
				B.Back = Tr.Prev;
			}
		}
		if (Row.ThinAfter > 0.0 && Row.ThinKeep > 1 && Row.Life > Row.ThinAfter)
		{
			// (art director 04:40Z: smoke hangs. A trail's smoke lives life_s, but to fit the room only one puff in
			// thin_keep_one_in hangs on that long, wider by thin_grow to fill the gaps; the rest go as they did, by
			// thin_after_s, and the old trail is fewer, bigger lumps. Each puff keeps its look over those first seconds)
			const double Early = Row.ThinAfter / Row.Life; // (the share of its life the old trail lived)
			// (a ribbon keeps one wherever the path has turned too, so the old trail of a spin or a tight turn follows
			// its curve, never a straight cut across it)
			const bool bKept = Tr.Puffs == Tr.NextKept || (bRibbon && Tr.bKeptAt && Way.Dot(Tr.KeptWay) < Row.TurnCos);
			++Tr.Puffs;
			if (bKept)
			{
				// (art director 05:25Z: kept evenly the old trail read as a string of beads; thin_ragged tears it, the gaps
				// and sizes uneven, so it reads as a torn ribbon)
				const unsigned Behind = Tr.KeptGap > 0 ? Tr.KeptGap : static_cast<unsigned>(Row.ThinKeep);
				unsigned Gap = static_cast<unsigned>(Row.ThinKeep);
				double Torn = 1.0;
				if (Row.ThinRagged > 0.0)
				{
					Gap = static_cast<unsigned>(std::clamp(static_cast<int>(std::lround(Row.ThinKeep * (1.0 + Row.ThinRagged * D.U(-1.0, 1.0)))), 1, 2 * Row.ThinKeep));
					Torn = 1.0 + Row.ThinRagged * D.U(-0.5, 0.6);
				}
				Tr.NextKept = Tr.Puffs - 1 + Gap;
				Tr.KeptGap = Gap;
				if (bRibbon)
				{
					// (a ribbon: the bar that hangs on is a piece of its own, laid where this puff is and drifting with it,
					// reaching back to the last bar. Unseen while the trail is young and thin, it comes into sight as the
					// short pieces over it go, so the trail is as dark all along at every age and its old length is bars
					// that overlap; it grows on as the old smoke did, thin_grow times wider, and shortens to a lump only
					// toward its old end)
					Bit C = B;
					if (Row.WidenBy > 0.0)
					{
						// (widen_by_s: it widens as the short pieces over it do, so the two are one width, and then on)
						C.GrowBy = Row.WidenBy;
						C.RMid = B.R1;
						C.GrowEarly = B.Grow;
					}
					C.R1 = C.R0 + (C.R1 - C.R0) * Row.ThinGrow * Torn;
					if (Row.ThinGrow > 1.0)
					{
						C.Grow = std::clamp(std::log(Row.ThinGrow) / -std::log(std::max(Early, 1e-3)), 0.2, 3.0);
					}
					C.Alpha = std::min(0.95, B.Alpha * Row.ThinAlpha);
					C.Hold = Row.ThinHold >= 0.0 ? Row.ThinHold : B.Hold * Early;
					C.Life = Row.Life; // (every bar hangs the whole of life_s: one going early left a gap)
					C.ShowFrom = Row.JoinFrom * Row.ThinAfter;
					C.ShowBy = Row.JoinBy * Row.ThinAfter;
					C.FadeIn = 0.0;
					C.LumpSeed = B.LumpSeed * 2654435761u + 1u;
					// (each bar lets go a little sooner or later than the next, so the old end tears into lumps of uneven
					// length, never a row of even dashes)
					C.SmearHold = std::min(0.95, Row.LumpsFrom + 0.5 * Row.ThinRagged * (1.0 - Row.LumpsFrom) * ((C.LumpSeed >> 8) % 1024u / 511.5 - 1.0));
					C.Smear = (2.0 * Row.ThinKeep + 1.0) * Row.Spacing;
					C.Back = Bit::Reach();
					if (Tr.bKeptAt && (At - Tr.KeptAt).Length() < C.Smear)
					{
						C.Back = Tr.Kept;
					}
					Tr.Kept.From = C.From;
					Tr.Kept.Vel = C.Vel;
					Tr.Kept.Way = Way;
					Tr.Kept.Rise = C.Rise;
					Tr.Kept.Born = C.Born;
					Tr.Kept.R0 = C.R0;
					Tr.Kept.RMid = C.RMid;
					Tr.Kept.R1 = C.R1;
					Tr.Kept.Grow = C.Grow;
					Tr.Kept.GrowBy = C.GrowBy;
					Tr.Kept.GrowEarly = C.GrowEarly;
					Tr.Kept.Life = C.Life;
					Tr.Kept.Alpha = C.Alpha; // (its own opacity: the next bar's far end takes it)
					Tr.Kept.LumpsFrom = C.SmearHold;
					Tr.Kept.bOn = true;
					Tr.KeptWay = Way;
					Tr.KeptAt = At;
					Tr.bKeptAt = true;
					Bits.push_back(C);
					B.Life *= Early; // (and the short piece here goes as the rest do)
					B.SmearHold = 1.0;
				}
				else
				{
					// (a lump that hangs on: it grows as it did over the first seconds and on after, thin_grow times more,
					// and starts thinning when it did, but slowly, over the whole of life_s)
					// (art director 04:59Z: it grows but holds its density. It ends thin_grow times as wide as the old smoke
					// did, on a curve that reaches the old size at thin_after_s, so the young trail looks as it did)
					B.R1 = B.R0 + (B.R1 - B.R0) * Row.ThinGrow * Torn;
					if (Row.ThinGrow > 1.0)
					{
						B.Grow = std::clamp(std::log(Row.ThinGrow) / -std::log(std::max(Early, 1e-3)), 0.2, 3.0);
					}
					// (it thickens by thin_alpha as the three beside it go, by thin_after_s, standing in for them; the young
					// trail by the engine looks as it did)
					B.HazeAlpha = std::min(0.95, B.Alpha * Row.ThinAlpha);
					B.HazeBy = Row.ThinAfter;
					B.Hold = Row.ThinHold >= 0.0 ? Row.ThinHold : B.Hold * Early;
					// (and it is drawn out along the path over the gap to the next one kept, shrinking as slowly, so the old
					// trail stays one dark line of lumps that touch, never a dotted line)
					B.Smear *= Behind;
					if (Tr.bKeptAt)
					{
						// (drawn back to where the last one kept was laid, the straight way there, so a trail through a
						// tight turn or a spin joins up along its curve and never shoots out straight past it)
						const Vec Chord = At - Tr.KeptAt;
						const double Long_ = Chord.Length();
						if (Long_ > 1e-3 && Chord.Dot(Way) > 0.94 * Long_)
						{
							B.Along = Chord * (1.0 / Long_);
							B.Smear = std::min(B.Smear, Long_ * Row.Smear);
						}
						else
						{
							B.Smear = Row.Spacing * Row.Smear; // (turning hard: a straight length would cut across the curve)
						}
					}
					Tr.KeptAt = At;
					Tr.bKeptAt = true;
					B.SmearFor /= Early;
					B.Life /= Lasts; // (every one kept hangs the whole of life_s: a small one going early left a gap)
				}
			}
			else
			{
				B.Life *= Early; // (the rest live and grow as the trail's smoke did before)
				B.SmearHold = bRibbon ? 1.0 : B.SmearHold; // (a ribbon: and keep their length to the end, as the bar over them takes their place)
			}
		}
		if (bRibbon)
		{
			if (Row.WidenBy > 0.0)
			{
				// (widen_by_s: it has its width by then, by the power grow; see Bit::GrowBy. And its colour pales by its age
				// in the trail's whole life, as the length under it does, so the two are one colour)
				B.GrowBy = Row.WidenBy;
				B.RMid = B.R1;
				B.GrowEarly = B.Grow;
				if (Row.Life > 0.0)
				{
					B.Cool *= B.Life / Row.Life;
				}
			}
			Tr.Prev.From = B.From;
			Tr.Prev.Vel = B.Vel;
			Tr.Prev.Way = Way;
			Tr.Prev.Rise = B.Rise;
			Tr.Prev.Born = B.Born;
			Tr.Prev.R0 = B.R0;
			Tr.Prev.RMid = B.RMid;
			Tr.Prev.R1 = B.R1;
			Tr.Prev.Grow = B.Grow;
			Tr.Prev.GrowBy = B.GrowBy;
			Tr.Prev.GrowEarly = B.GrowEarly;
			Tr.Prev.Life = B.Life;
			Tr.Prev.Alpha = B.Alpha; // (its own opacity: the next piece's far end takes it)
			Tr.Prev.LumpsFrom = B.SmearHold;
			Tr.Prev.bOn = true;
		}
		Tr.PuffAt = At;
		Tr.bPuffAt = true;
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
			if (Row.Core > 0.0)
			{
				Tr->bPuffAt = Tr->bKeptAt = false;
				Tr->Prev.bOn = Tr->Kept.bOn = Tr->bWander = false;
				Tr->Wide = 0.0;
				Tr->NextKept = Tr->Puffs;
			}
		}
		const double Between = Now - Tr->Last;
		const Vec Path = At - Tr->At;
		const double Long = Path.Length();
		// (fire: its smoke starts at the flame's tips, not at the cowl)
		const double Grown = Clamp01((Now - Tr->KindBorn) / Row.RampS);
		const double FlameM = FlameOf(*Tr, Row);
		Tr->Tip = FlameM * Row.SmokeStart * (0.45 + 0.55 * Grown); // (art director 04:59Z: the smoke starts inside the flame's tips, no gap)
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
		if (FlameM > 0.0)
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
				const double Len = FlameM * (0.45 + 0.55 * Grown) * D.U(0.55, 1.35);
				const Vec Curl = (Side * (0.2 * D.Bell()) + Lift * D.U(-0.05, 0.1)) * Len;
				// (art director 21:37Z: fire in a slipstream stands off the skin and spills past the silhouette, so each
				// tongue lifts above or below as it streams aft; the heart stays on the cowl)
				// (art director 04:59Z: a fire at speed is outside the plane: a long flame's tongues stand off the skin by a share
				// of their length, licking out above and below the fuselage)
				const double Above = D.U() < 0.5 ? 1.0 : -1.0; // (above or below: drawn before how far, as it always was)
				const double StandOff = Row.FlameLiftOfLen.B > 0.0 ? Len * Row.FlameLiftOfLen.Pick(D) : Row.FlameLift.Pick(D);
				const Vec Spill = Lift * (Above * StandOff);
				const int Pieces = 4;
				for (int I = 0; I < Pieces; ++I)
				{
					const double At_ = (I + 0.5) / Pieces; // (0 at the cowl to 1 at the tip)
					const Vec Ragged = (Side * D.Bell() + Lift * D.Bell()) * (Len * 0.035 * (1.0 + 2.0 * At_));
					const Vec Way = (Back * Len + Curl * (2.0 * At_)).Unit(Back);
					Bit B;
					B.Born = T;
					// (art director 05:25Z: a tongue is fixed at its root and free at its tip: rooted on the skin at the fire,
					// only its tip licks out above or below. flame_lift_curve: how late along it the stand-off comes)
					const double Outward_ = Row.FlameLiftCurve > 0.0 ? std::pow(At_, Row.FlameLiftCurve) : std::min(1.0, 1.4 * At_);
					B.From = Root + Back * (Len * At_) + Curl * (At_ * At_) + Spill * Outward_ + Ragged;
					B.Vel = Vel + Back * (Len * D.U(3.0, 6.0)); // (streaming aft off the cowl)
					B.Life = D.U(0.045, 0.075);
					B.R0 = Len * (0.12 - 0.065 * At_) * D.U(0.85, 1.15) * Row.FlameWide;
					B.R1 = B.R0 * 0.6; // (eaten away as it goes)
					B.Grow = 1.0;
					B.Along = Way;
					B.Stretch = D.U(2.0, 2.8) + At_;
					B.Point = 0.6;
					B.Tail = 0.5;
					B.Col = At_ < 0.5 ? HeartCol.Mix(FlameCol, 0.35 + 1.3 * At_) : FlameCol.Mix(DeepCol, (At_ - 0.5) * 2.0);
					B.Col2 = DeepCol * 0.7;
					if (Row.bFlameTip && At_ > 0.6)
					{
						// (yellow-white at the root, orange along it, dark red at the tip going into the black smoke)
						B.Col = B.Col.Mix(Row.FlameTip, (At_ - 0.6) / 0.4);
						B.Col2 = Row.FlameTip.Mix(Row.ColFull, 0.4);
					}
					// (art director 04:59Z: by day a tongue off the skin must read as fire against the sky, not a pale smear)
					B.Alpha = Row.FlameAlphaRoot - Row.FlameAlphaFall * At_;
					B.FadeIn = 0.002;
					B.Hold = Row.FlameHold;
					B.Heart = 0.85 - 0.2 * At_;
					B.Rim = 0.1;
					B.Rough = 0.45;
					B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
					B.Turn = D.U(0.0, 2.0 * Pi);
					B.bLight = true;
					Bits.push_back(B);
				}
				if (Row.TipSmoke > 0.0)
				{
					// (art director 04:59Z: the black smoke starts right at the flame's tips, above or below the fuselage as the
					// tongue is, so there is no gap between flame and smoke. It hangs in the air where the tip was, the aircraft
					// flying on from it, and joins the trail behind)
					Bit B;
					B.Born = T;
					B.From = Root + Back * (Len * D.U(0.8, 1.0)) + Curl + Spill;
					B.Vel = Vel * Row.Carried + Vec(D.Bell(), D.Bell(), D.Bell()) * 1.5;
					B.Drag = 2.0;
					B.Rise = Row.Rise;
					B.Windage = 1.0;
					B.Life = Row.TipSmokeLife.Pick(D);
					B.R0 = Len * 0.07 * Row.FlameWide;
					B.R1 = std::max(B.R0, FlameM * Row.TipSmokeR * D.U(0.8, 1.2));
					B.Grow = 0.6;
					B.Along = -Back;
					B.Smear = Vel.Length() / 60.0 * Row.Smear; // (one a frame: each drawn back to the last)
					B.Col = Row.ColFull;
					B.Col2 = Row.Col.Mix(Row.ColOld, 0.3);
					B.Cool = 1.0;
					B.Alpha = Row.TipSmoke * (0.45 + 0.55 * Grown) * D.U(0.85, 1.05);
					B.FadeIn = 0.015;
					B.Hold = 0.4;
					B.Heart = 0.7;
					B.Rim = 0.0;
					B.Rough = 0.4;
					B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
					Bits.push_back(B);
				}
				// now and then a lick tears free of the tip and streams away aft on its own, dimming
				if (D.U() < Row.LicksPerFrame)
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
					B.Alpha = Row.LickAlpha;
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
					B.R0 = FlameM * (0.45 + 0.55 * Grown) * 0.085 * D.U(0.85, 1.15);
					B.R1 = B.R0;
					B.Along = Back;
					B.Stretch = 1.6;
					B.LeastPx = Row.FlameLeastPx; // (art director 04:40Z: from the strategy camera a fire is never under 4 px)
					B.LeastKeep = 0.8;
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

	void Battle::AirFall(double Now, int Id, const Vec& At, const Vec& Velocity, const char* Kind, double SpanM, double DeckZ)
	{
		// The art director, 04:40Z: a fall throws a debris row. 30 to 50 pieces for a fighter, 60 to 100 for a bomber;
		// explode sheds them all at once with a short torn fireball, never a ball; burn sheds 5 to 10 over its fall;
		// spin a few, sometimes a wing. Nothing pops out of view: pieces end in the cloud deck, or the haze or the ground.
		const std::string K = Kind && *Kind ? Kind : "burn";
		const std::string P = "air_debris.";
		AirFallStop(Id);
		Falling Fa;
		Fa.Id = Id;
		Fa.bOn = true;
		Fa.Span = std::max(2.0, SpanM > 0.0 ? SpanM : 11.0);
		Fa.DeckZ = DeckZ;
		Fa.At = At;
		Fa.Vel = Velocity;
		Fa.Last = Now;
		Fa.Seed = ++Seeds * 73856093ull ^ PlaceKey(At);
		Dice D(Fa.Seed);
		const bool bBomber = Fa.Span >= Num(P + "bomber_span_m", 20.0);
		if (K == "explode")
		{
			const Span Row = Range(P + (bBomber ? "bomber" : "fighter"), bBomber ? 60.0 : 30.0, bBomber ? 100.0 : 50.0);
			const int Count = static_cast<int>(std::lround(std::clamp(Row.Pick(D), 0.0, 400.0))); // (held: a slip of the recipe never floods the frame)
			const double Heavy = Clamp01(Num(P + "heavy_share", 0.12));
			const Span Throw = Range(P + "throw_mps", 8.0, 35.0);
			for (int I = 0; I < Count; ++I)
			{
				const Vec Toward = Vec(D.Bell(), D.Bell(), D.Bell() + 0.2).Unit(ZUp);
				Shed(Now, At + Toward * (0.25 * Fa.Span * D.U()), Velocity, Toward * Throw.Pick(D), Fa.Span, DeckZ, D.U() < Heavy ? 1 : 0, D);
			}
			TornFireball(Now, At, Velocity, Fa.Span, D);
			return; // (nothing more falls: the model is gone)
		}
		if (K == "spin")
		{
			Fa.Left = static_cast<int>(std::lround(std::clamp(Range(P + "spin_sheds", 2.0, 5.0).Pick(D), 0.0, 64.0)));
			Fa.bWing = D.U() < Clamp01(Num(P + "wing_odds", 0.35));
		}
		else
		{
			Fa.Left = static_cast<int>(std::lround(std::clamp(Range(P + "burn_sheds", 5.0, 10.0).Pick(D), 0.0, 64.0)));
		}
		Fa.Next = Now + Range(P + "first_s", 0.3, 1.5).Pick(D);
		Fa.Seed = static_cast<uint64_t>(D.U() * 9007199254740991.0) ^ Fa.Seed;
		Falls.push_back(Fa);
	}

	void Battle::AirFallMove(double Now, int Id, const Vec& At, const Vec& Velocity)
	{
		for (Falling& Fa : Falls)
		{
			if (Fa.Id != Id || !Fa.bOn)
			{
				continue;
			}
			const std::string P = "air_debris.";
			Dice D(Fa.Seed ^ static_cast<uint64_t>(static_cast<int64_t>(Now * 1000.0)) * 0x9E3779B97F4A7C15ull);
			const Span Every = Range(P + "every_s", 1.2, 4.0);
			const double Heavy = Clamp01(Num(P + "heavy_share", 0.12));
			const Span Loose = Range(P + "loose_mps", 3.0, 12.0);
			for (int Guard = 0; Fa.Left > 0 && Fa.Next <= Now && Guard < 8; ++Guard)
			{
				// (shed where the model was when it came loose, between the last call and this one)
				const double K = Now - Fa.Last > 1e-6 ? Clamp01((Fa.Next - Fa.Last) / (Now - Fa.Last)) : 1.0;
				const Vec Where_ = Fa.At + (At - Fa.At) * K;
				const Vec Vel = Fa.Vel + (Velocity - Fa.Vel) * K;
				const Vec Toward = Vec(D.Bell(), D.Bell(), D.Bell()).Unit(ZUp);
				const int Sort = Fa.bWing ? 2 : (D.U() < Heavy ? 1 : 0);
				Shed(Fa.Next, Where_ + Toward * (0.15 * Fa.Span * D.U()), Vel, Toward * Loose.Pick(D), Fa.Span, Fa.DeckZ, Sort, D);
				Fa.bWing = false;
				--Fa.Left;
				Fa.Next += Every.Pick(D);
			}
			Fa.At = At;
			Fa.Vel = Velocity;
			Fa.Last = Now;
			return;
		}
	}

	void Battle::AirFallStop(int Id)
	{
		Falls.erase(std::remove_if(Falls.begin(), Falls.end(), [Id](const Falling& Fa) { return Fa.Id == Id; }), Falls.end());
	}

	void Battle::Shed(double Now, const Vec& At, const Vec& Vel, const Vec& Throw, double SpanM, double DeckZ, int Sort, Dice& D)
	{
		// (one piece torn from a falling aircraft: carried on by the aircraft's speed, thrown out, then falling at its own
		// speed through the air: a light skin plate soon flutters down slowly and drifts with the wind, a heavy piece
		// drops nearly straight and fast, a wing tumbles slowly between the two)
		const std::string P = "air_debris.";
		const double Scale = std::sqrt(SpanM / 11.0); // (a bomber's pieces bigger, not three times bigger)
		Bit B;
		B.Form = Bit::Shape::Plate;
		B.Born = Now;
		B.From = At;
		double Sink = 10.0;
		if (Sort == 2)
		{
			const double Chord = std::max(0.05, Num(P + "wing_chord_of_span", 0.17) * SpanM); // (held over a least, as case_d_m is: a chord of 0 divided by nought)
			B.R0 = B.R1 = 0.5 * Chord;
			B.Stretch = std::max(1.0, 0.45 * SpanM / Chord);
			B.Rough = 0.15;
			B.Tumble = Range(P + "wing_tumble", 0.6, 1.6).Pick(D) * (D.U() < 0.5 ? 1.0 : -1.0);
			Sink = Range(P + "wing_sink_mps", 22.0, 32.0).Pick(D);
			B.Windage = 0.5;
			B.Col = Picture(P + "paint", Rgb::FromPicture(92, 98, 74));
			B.Col2 = Picture(P + "underside", B.Col);
			B.Glint = 0.5 * Num(P + "glint", 1.6);
		}
		else if (Sort == 1)
		{
			B.R0 = B.R1 = 0.5 * Range(P + "heavy_m", 0.3, 0.8).Pick(D) * Scale;
			B.Stretch = D.U(1.0, 1.7);
			B.Rough = 0.4;
			B.Tumble = Range(P + "heavy_tumble", 0.5, 2.5).Pick(D) * (D.U() < 0.5 ? 1.0 : -1.0);
			Sink = Range(P + "heavy_sink_mps", 45.0, 80.0).Pick(D);
			B.Windage = 0.2;
			B.Col = B.Col2 = Picture(P + "heavy_colour", Rgb::FromPicture(40, 38, 36));
			B.Glint = 0.15 * Num(P + "glint", 1.6);
		}
		else
		{
			B.R0 = B.R1 = 0.5 * Range(P + "plate_m", 0.25, 1.1).Pick(D) * Scale;
			B.Stretch = D.U(1.0, 2.2);
			B.Rough = 0.5; // (torn edges)
			B.Tumble = Range(P + "plate_tumble", 2.0, 9.0).Pick(D) * (D.U() < 0.5 ? 1.0 : -1.0);
			Sink = Range(P + "plate_sink_mps", 7.0, 16.0).Pick(D);
			B.Windage = 1.0;
			B.Col = Picture(P + "paint", Rgb::FromPicture(92, 98, 74)); // (its painted face)
			B.Col2 = Picture(P + "bare", Rgb::FromPicture(168, 170, 172)); // (and the bare metal inside)
			B.Glint = Num(P + "glint", 1.6);
		}
		// (true flight with the air's drag: it slows from Vel + Throw toward sinking at Sink and drifting with the wind)
		Sink = std::max(1.0, Sink);
		const double Kd = 9.8 / Sink;
		const Vec Start = Vel * Range(P + "carry", 0.55, 0.9).Pick(D) + Throw;
		B.Drag = Kd;
		B.Vel = Start + ZUp * Sink;
		B.Rise = -Sink;
		B.Gravity = 0.0;
		B.Along = Vec(D.Bell(), D.Bell(), D.Bell()).Unit(Vec(1.0, 0.0, 0.0));
		B.Face = B.Along.Cross(Vec(D.Bell(), D.Bell(), D.Bell())).Unit(B.Along.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0)));
		B.Turn = D.U(0.0, 2.0 * Pi);
		// (it lives until it sinks into the deck: about how long its fall takes once it has slowed to its own speed)
		const double Down = At.Z - DeckZ + (Start.Z + Sink) / Kd;
		const double Most = std::max(1.0, Num(P + "most_s", 240.0));
		B.Life = std::clamp(Down / Sink, 0.5, Most);
		const double Into = std::max(0.3, Num(P + "enter_deck_m", 40.0) / Sink); // (fading as it sinks into the deck or the haze)
		B.Hold = Clamp01(1.0 - Into / B.Life);
		B.Alpha = 1.0;
		B.FadeIn = 0.01;
		B.Heart = 1.0;
		B.Rim = 1.0;
		B.Inner = 0.6;
		B.ShadeK = 0.0;
		B.LeastPx = Num(P + "least_px", 1.0); // (far off it shrinks to a speck that fades, but a glint can still catch)
		B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
		Bits.push_back(B);
	}

	void Battle::TornFireball(double Now, const Vec& At, const Vec& Vel, double SpanM, Dice& D)
	{
		// (art director 04:40Z: a short torn fireball, never a ball: the airframe tears along a few ways at once, so the
		// fire is thrown out in ragged clumps of different sizes along those ways, deep orange, gone in half a second,
		// each clump leaving black smoke that hangs. A white-hot heart for a frame or two)
		// (art director 06:00Z: fire, never pale peach. It is lit from within: the heart near white-yellow through about
		// 0.1 s, the body a saturated orange, cooling to deep orange-red by 0.2 to 0.3 s and then into the black smoke,
		// never through clay or salmon. So every colour on the way keeps its blue low, the ramp runs hot, orange, deep
		// red with no pale mix, and a clump is near opaque to its rim, as a see-through rim over the sky goes salmon)
		const std::string P = "air_debris.fireball.";
		const double S = SpanM / 11.0;
		const Rgb HotCol = Picture(P + "hot_colour", Rgb::FromPicture(255, 222, 90));
		const Rgb DeepCol = Picture(P + "deep_colour", Rgb::FromPicture(140, 40, 16));
		const Rgb SmokeCol = Picture(P + "smoke_colour", Rgb::FromPicture(38, 35, 33));
		const Rgb FireCol = Picture(P + "colour", Rgb::FromPicture(250, 136, 22));
		const Span HeartMix = Range(P + "heart_mix", 0.2, 0.6);
		const double CoolBy = std::max(0.02, Num(P + "cool_by_s", 0.25));
		const double LobeRim = Clamp01(Num(P + "rim", 0.85)), LobeRough = Clamp01(Num(P + "rough", 0.35));
		const double EdgeShare = Clamp01(Num(P + "edge", 0.5)), TwinPx = std::max(0.0, Num(P + "twin_px", 40.0));
		const int Lobes = static_cast<int>(std::lround(std::clamp(Range(P + "lobes", 18.0, 28.0).Pick(D), 0.0, 200.0)));
		const int Ways = static_cast<int>(std::lround(std::clamp(Range(P + "tears", 2.0, 4.0).Pick(D), 1.0, 16.0)));
		const Span Life = Range(P + "life_s", 0.25, 0.6), Reach = Range(P + "reach_of_span", 0.3, 0.9), R = Range(P + "r_of_span", 0.05, 0.12);
		const Span SmokeS = Range(P + "smoke_s", 5.0, 9.0), SmokeGrow = Range(P + "smoke_grow", 5.0, 7.0);
		std::vector<Vec> Tears;
		for (int I = 0; I < Ways; ++I)
		{
			Tears.push_back(Vec(D.Bell(), D.Bell(), D.Bell()).Unit(ZUp));
		}
		// (the white heart, broken along the tears so it follows the clumps, never one flat disc: art director 06:09Z)
		const int Hearts = static_cast<int>(std::lround(std::clamp(Num(P + "hearts", 3.0), 1.0, 32.0)));
		const double HeartR = Num(P + "heart_r_of_span", 0.11);
		for (int I = 0; I < Hearts; ++I)
		{
			const Vec Way = Tears[static_cast<size_t>(I % Ways)];
			Bit H;
			H.Born = Now;
			H.From = At + Way * (D.U(0.03, 0.12) * SpanM);
			H.Vel = Vel * 0.8 + Way * (D.U(4.0, 10.0) * S);
			H.Drag = 2.0;
			H.Life = Num(P + "heart_s", 0.12) * D.U(0.9, 1.1);
			H.R0 = HeartR * SpanM * D.U(0.7, 1.1);
			H.R1 = H.R0 * 1.5;
			H.Along = Way;
			H.Stretch = D.U(1.2, 1.7);
			H.Turn = D.U(0.0, 2.0 * Pi);
			H.Col = Rgb::FromPicture(255, 252, 236);
			H.Col2 = Picture(P + "heart_colour", Rgb::FromPicture(255, 240, 170));
			H.Nearer = 0.5 * SpanM; // (drawn over its own clumps: the white heart shows through the fire round it)
			H.Edge = HotCol; // (white going to yellow at its edge, never a pale fringe over the sky)
			H.EdgeK = 0.6;
			H.Alpha = 1.0;
			H.FadeIn = 0.002;
			H.Hold = 0.85; // (full through about 0.1 s, then gone in a frame: never a pale ghost over the sky)
			H.Heart = 1.0;
			H.Rim = LobeRim;
			H.Rough = 0.45;
			H.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			H.bLight = true;
			H.bWhole = true;
			Bits.push_back(H);
		}
		for (int I = 0; I < Lobes; ++I)
		{
			const Vec Toward = (Tears[static_cast<size_t>(I % Ways)] + Vec(D.Bell(), D.Bell(), D.Bell()) * 0.35).Unit(ZUp);
			const double Far = Reach.Pick(D) * SpanM;
			Bit B;
			B.Born = Now + D.U(0.0, 0.06);
			B.From = At + Toward * (Far * D.U(0.1, 0.6));
			B.Vel = Vel * 0.6 + Toward * (Far * D.U(1.2, 3.0));
			B.Drag = 3.0;
			B.Life = Life.Pick(D);
			B.R0 = R.Pick(D) * SpanM * D.U(0.4, 1.0);
			B.R1 = B.R0 * D.U(1.6, 2.6);
			B.Grow = 0.5;
			B.Along = Toward;
			B.Stretch = D.U(1.1, 2.0);
			B.Col = FireCol.Mix(HotCol, HeartMix.Pick(D));
			B.Col2 = DeepCol; // (cooling to deep orange-red by CoolBy, still near opaque, then gone into its smoke)
			B.Cool = B.Life / CoolBy;
			B.Alpha = 1.0;
			B.FadeIn = 0.005;
			B.Hold = 0.6;
			B.Heart = 1.0;
			B.Rim = LobeRim;
			B.Rough = LobeRough;
			B.Edge = DeepCol; // (a hot middle darkening out to a deep red rim: a lump of fire)
			B.EdgeK = EdgeShare;
			B.TwinPx = TwinPx;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.bLight = true;
			B.bWhole = true;
			Bits.push_back(B);
			{
				// (and the black smoke a clump leaves, from where the clump is as it starts to die, riding with it and a
				// little wider, so the dying fire goes into the smoke and never fades out over the bare sky, then hanging)
				const double Late = B.Life * std::max(0.0, B.Hold - 0.15);
				Bit M;
				M.Born = B.Born + Late;
				M.From = Where(B, Late);
				M.Vel = Going(B, Late);
				M.Drag = B.Drag;
				M.Windage = 1.0;
				M.Life = SmokeS.Pick(D);
				M.R0 = 1.15 * (B.R0 + (B.R1 - B.R0) * std::pow(B.Hold, B.Grow));
				M.R1 = M.R0 * SmokeGrow.Pick(D);
				M.Grow = 0.5;
				M.Col = SmokeCol;
				M.Col2 = SmokeCol.Mix(Rgb::FromPicture(120, 116, 112), 0.6);
				M.Alpha = Num(P + "smoke_alpha", 0.9) * D.U(0.8, 1.05);
				M.FadeIn = 0.04;
				M.Hold = 0.3;
				M.Heart = 0.7;
				M.Rim = 0.0;
				M.Rough = 0.5;
				M.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
				M.LeastPx = 2.0;
				M.LeastKeep = 0.4;
				Bits.push_back(M);
			}
		}
		Flash L;
		L.At = At;
		L.Col = FlameCol;
		L.Born = Now;
		L.Life = 0.3;
		L.Strength = Num(P + "lamp", 0.6) * S;
		L.Radius = 40.0 * S;
		Flashes.push_back(L);
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

	Battle::OilRow Battle::OilRowOf(const std::string& Kind) const
	{
		const std::string P = "oil_fires." + (Has("oil_fires." + Kind + ".column_life_s.0") ? Kind : std::string("sunk")) + ".";
		OilRow R;
		R.Heads = Range(P + "heads", 3.0, 5.0);
		R.HeadSpread = Num(P + "head_spread", 0.6);
		R.HeadR = Range(P + "head_r", 0.15, 0.3);
		R.FlameUp = Num(P + "flame_up_m", 0.0);
		R.FlameEvery = std::max(0.02, Num(P + "flame_every_s", 0.1));
		R.FlameR = Range(P + "flame_r_m", 2.0, 4.5);
		R.FlameTall = Range(P + "flame_tall", 2.0, 3.5);
		R.FlameLife = Range(P + "flame_life_s", 0.35, 0.7);
		R.ColumnEvery = std::max(0.1, Num(P + "column_every_s", 1.0));
		R.ColumnLife = Range(P + "column_life_s", 50.0, 70.0);
		R.Rise = Range(P + "rise_mps", 6.0, 9.0);
		R.Burst = Range(P + "burst_mps", 0.0, 0.0);
		R.BaseEvery = Num(P + "base_every_s", 0.0);
		R.BaseLife = Range(P + "base_life_s", 10.0, 15.0);
		R.BaseRise = Range(P + "base_rise_mps", 1.5, 3.0);
		R.BaseR = Range(P + "base_r", 0.15, 0.3);
		R.BaseAlpha = Range(P + "base_alpha", 0.8, 0.95);
		R.BaseFlat = Num(P + "base_flat", 1.0);
		R.PallEvery = Num(P + "pall_every_s", 0.0);
		R.PallLife = Range(P + "pall_life_s", 150.0, 240.0);
		R.PallR1 = Range(P + "pall_r1", 1.2, 2.0);
		R.PallWindage = Range(P + "pall_windage", 1.3, 1.8);
		R.PallAlpha = Range(P + "pall_alpha", 0.55, 0.75);
		R.PallStretch = std::max(1.0, Num(P + "pall_stretch", 1.0));
		R.PallStretchBy = Num(P + "pall_stretch_by_s", 0.0);
		R.PallHold = Num(P + "pall_hold", 0.15);
		R.PallLevel = Range(P + "pall_level", 1.0, 1.0);
		R.PallDrift = std::max(0.0, Num(P + "pall_drift_mps", 0.0));
		R.PallRough = std::max(0.0, Num(P + "pall_rough", 0.0));
		R.PallRest = std::min(2.0, std::max(0.0, Num(P + "pall_rest", 0.0)));
		R.SlickLong = Num(P + "slick_long", 0.0);
		R.SlickWide = Num(P + "slick_wide", 0.0);
		R.FlameOdds = Clamp01(Num(P + "flame_odds", 1.0));
		R.FlamePoint = Clamp01(Num(P + "flame_point", 0.9));
		R.SunSide = Num(P + "sun_side", 0.0);
		R.Lit = Picture(P + "lit_colour", Rgb::FromPicture(118, 106, 94));
		R.BurstDrag = std::max(0.01, Num(P + "burst_drag", 0.1));
		R.Slow = Num(P + "slow_mps2", 0.06);
		R.Windage = Range(P + "windage", 1.0, 1.6);
		R.R0 = Range(P + "r0", 0.2, 0.35);
		R.R1 = Range(P + "r1", 0.8, 1.4);
		R.Grow = Num(P + "grow", 0.6);
		R.Alpha = Range(P + "alpha", 0.85, 0.95);
		R.Hold = Num(P + "hold", 0.5);
		R.Heart = Num(P + "heart", 0.7);
		R.Rough = Num(P + "rough", 0.3);
		R.Cool = Num(P + "cool", 1.0);
		R.Lamp = Num(P + "lamp", 0.25);
		R.LampReach = Num(P + "lamp_reach", 2.0);
		R.TwinPx = Num(P + "twin_px", 0.0);
		R.bKeep = Num(P + "keep", 1.0) > 0.5;
		R.Col = Picture(P + "colour", Rgb::FromPicture(24, 22, 21));
		R.Col2 = Picture(P + "colour_top", Rgb::FromPicture(64, 60, 57));
		R.Flame = Picture(P + "flame_colour", Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56)));
		return R;
	}

	void Battle::OilFire(double Now, int Id, const Vec& AtIn, double Reach, const char* Kind, double Until, double Since)
	{
		OilStop(Now, Id); // (one fire to an Id: a second call starts it afresh)
		Source S;
		S.Kind = 2;
		S.Id = Id;
		S.At = Vec(AtIn.X, AtIn.Y, std::max(AtIn.Z, Height(AtIn.X, AtIn.Y))); // (on the sea, or on a burning ship's deck if given)
		S.Across = std::max(5.0, Reach);
		S.Oil = OilRowOf(Kind && *Kind ? Kind : "sunk");
		S.Seed = static_cast<uint64_t>(static_cast<uint32_t>(Id)) * 2654435761ull ^ PlaceKey(S.At) ^ 0x9e3779b97f4a7c15ull;
		S.Start = Now - std::max(0.0, Since);
		S.Flames = S.End = Until > Now ? Until : 1e12;
		// (where on the slick it burns hardest: a few heads, fixed by the fire, never by the frame)
		Dice D(S.Seed);
		const int NHeads = std::max(1, static_cast<int>(std::lround(S.Oil.Heads.Pick(D))));
		if (S.Oil.SlickLong > 0.0)
		{
			// (the slick an oval along her last heading, the fire's heads scattered all over it: a burning sea, not a chimney)
			const double Heading = D.U(0.0, Pi);
			S.SlickA = Vec(std::cos(Heading), std::sin(Heading), 0.0) * (0.5 * S.Oil.SlickLong * S.Across);
			S.SlickB = Vec(-std::sin(Heading), std::cos(Heading), 0.0) * (0.5 * std::max(S.Oil.SlickWide, 0.05) * S.Across);
			for (int I = 0; I < NHeads; ++I)
			{
				S.Heads.push_back(OnSlick(S, D));
			}
		}
		else
		{
			for (int I = 0; I < NHeads; ++I)
			{
				const double A = D.U(0.0, 2.0 * Pi), Out_ = I == 0 ? 0.0 : std::sqrt(D.U()) * S.Oil.HeadSpread * S.Across;
				const Vec H = S.At + Vec(std::cos(A), std::sin(A), 0.0) * Out_;
				S.Heads.push_back(Vec(H.X, H.Y, S.At.Z));
			}
		}
		// (a fire that has burned a while already stands as a grown column)
		const double Back = std::min(std::max(0.0, Since), S.Oil.ColumnLife.B);
		double T = Now - Back;
		for (; T < Now; T += S.Oil.ColumnEvery)
		{
			OilColumn(S, T);
		}
		S.NextSmoke = T;
		if (S.Oil.BaseEvery > 0.0)
		{
			for (T = Now - std::min(std::max(0.0, Since), S.Oil.BaseLife.B); T < Now; T += S.Oil.BaseEvery)
			{
				OilBase(S, T);
			}
		}
		S.NextBase = T;
		if (S.Oil.PallEvery > 0.0)
		{
			for (T = Now - std::min(std::max(0.0, Since), S.Oil.PallLife.B); T < Now; T += S.Oil.PallEvery)
			{
				OilColumn(S, T, true);
			}
		}
		S.NextPall = T;
		S.Next = Now;
		Sources.push_back(S);
	}

	void Battle::OilStop(double Now, int Id)
	{
		for (Source& S : Sources)
		{
			if (S.Kind == 2 && S.Id == Id && S.End > Now)
			{
				S.Flames = S.End = Now;
			}
		}
	}

	Vec Battle::OnSlick(const Source& S, Dice& D) const
	{
		const double A = D.U(0.0, 2.0 * Pi), Out_ = std::sqrt(D.U());
		const Vec P = S.At + S.SlickA * (std::cos(A) * Out_) + S.SlickB * (std::sin(A) * Out_);
		return Vec(P.X, P.Y, S.At.Z);
	}

	void Battle::OilBase(Source& S, double T)
	{
		// The slick's own smoke: low, black and broad, rolling off the burning oil anywhere across it and drawn into the
		// foot of the column, so the column stands on the whole fire and not on a stalk
		const OilRow& R = S.Oil;
		// (every time seed goes through a signed number first: a fire lit with time already behind it is grown from
		// before the clock's zero, and a negative number cast straight to an unsigned one is undefined; the Mac's chip
		// gave 0, one puff copied up the whole column. The pre-check, 05:07Z)
		Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(T * 1000.0)) * 0xD6E8FEB86659FD93ull + 41ull));
		const double A = D.U(0.0, 2.0 * Pi), Out_ = std::sqrt(D.U()) * R.HeadSpread * S.Across;
		const bool bFlat = R.BaseFlat < 0.999;
		Bit B;
		B.Born = T;
		B.Life = R.BaseLife.Pick(D);
		B.R0 = R.BaseR.Pick(D) * S.Across;
		B.R1 = B.R0 * D.U(1.6, 2.2);
		B.Grow = 0.5;
		if (bFlat)
		{
			// (art director 03:59Z: a low dark roll as wide as the slick, hugging the water: it stands on the sea and
			// turns about the upright only, so from a low eye it is a long low bank, never a chimney puff)
			const Vec On = S.SlickA.Length() > 0.0 ? OnSlick(S, D) : S.At + Vec(std::cos(A), std::sin(A), 0.0) * Out_;
			B.Form = Bit::Shape::Upright;
			B.bHug = true;
			B.Flatten = R.BaseFlat;
			B.From = On;
			B.Vel = (S.At - On) * 0.03;
		}
		else
		{
			B.From = S.At + Vec(std::cos(A), std::sin(A), 0.0) * Out_ + ZUp * (R.FlameUp + 0.4 * B.R0);
			B.Vel = (S.At - B.From) * 0.08 + ZUp * D.U(4.0, 8.0); // (drawn in toward the column's foot as it rises)
		}
		B.Drag = 0.25;
		B.Rise = R.BaseRise.Pick(D);
		B.Windage = R.Windage.A;
		B.Col = R.Col * D.U(0.85, 1.05);
		B.Col2 = R.Col * D.U(1.0, 1.3);
		B.Cool = 1.0;
		B.Alpha = R.BaseAlpha.Pick(D);
		B.FadeIn = 0.5;
		B.Hold = 0.4;
		B.Heart = R.Heart;
		B.Inner = 0.6;
		B.Rough = R.Rough;
		B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
		B.Turn = D.U(0.0, 2.0 * Pi);
		B.TwinPx = R.TwinPx;
		B.bKeep = R.bKeep && !bFlat; // (a low roll on the far horizon is under a pixel high: it may go when the screen is full)
		Bits.push_back(B);
	}

	void Battle::OilColumn(Source& S, double T, bool bPall)
	{
		// One puff of the column: born over a head of the fire, black and thick, climbing at first fast and then slower
		// as it cools, so with the wind carrying it all the while the column stands up from the sea and bends over
		const OilRow& R = S.Oil;
		Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(T * 1000.0)) * 0x2545F4914F6CDD1Dull + (bPall ? 83ull : 29ull)));
		const Vec& Head = S.Heads[static_cast<size_t>(D.Whole(0, static_cast<int>(S.Heads.size()) - 1))];
		const double Big = std::pow(S.Across / 100.0, 0.4); // (a bigger fire's smoke climbs higher)
		Bit B;
		B.Born = T;
		B.Life = R.ColumnLife.Pick(D);
		B.R0 = R.R0.Pick(D) * S.Across;
		B.R1 = R.R1.Pick(D) * S.Across;
		B.Grow = R.Grow;
		B.From = Head + Vec(D.Bell(), D.Bell(), 0.0) * (R.HeadR.Pick(D) * S.Across * 0.5) + ZUp * (R.FlameUp + 0.5 * B.R0);
		// (hot at first: it shoots up off the fire and the push dies away over burst_drag's seconds, then it climbs slowly
		// as it cools; so the column stands nearly upright off the water and bends over down the wind higher up)
		B.Vel = Vec(D.Bell(), D.Bell(), 0.0) * 0.6 + ZUp * (R.Burst.Pick(D) * Big);
		B.Drag = R.BurstDrag;
		B.Rise = R.Rise.Pick(D) * Big;
		B.Gravity = std::min(R.Slow, 0.8 * B.Rise / std::max(B.Life, 1.0)); // (slowing, never sinking)
		B.Windage = R.Windage.Pick(D);
		B.Col = R.Col * D.U(0.85, 1.1);
		B.Col2 = R.Col2 * D.U(0.9, 1.1);
		B.Cool = R.Cool;
		B.Alpha = R.Alpha.Pick(D);
		B.FadeIn = 0.15; // (thick from the first: the column's foot is never a thin stalk)
		B.Hold = R.Hold;
		B.Heart = R.Heart;
		B.HeartEnd = R.Heart * 0.5; // (the old smoke aloft loses its dark core and thins)
		B.Inner = 0.6;
		B.Rim = 0.0;
		B.Rough = R.Rough;
		B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
		B.Turn = D.U(0.0, 2.0 * Pi);
		B.TwinPx = R.TwinPx;
		B.bKeep = R.bKeep;
		B.SunSide = R.SunSide;
		B.LitCol = R.Lit;
		B.Centre = S.At;
		B.AxisWindage = 0.5 * ((bPall ? R.PallWindage.A : R.Windage.A) + (bPall ? R.PallWindage.B : R.Windage.B));
		B.OilId = bPall ? -1 : S.Id;
		if (bPall)
		{
			// (art director 03:59Z: never close the top. The pall: fewer, longer-lived puffs that climb with the column to
			// where its push gives out, then run nearly level down the wind as a widening, flattening sheet two or three
			// times the column's height long, thinning to nothing)
			B.Life = R.PallLife.Pick(D);
			B.R1 = R.PallR1.Pick(D) * S.Across;
			B.Grow = 0.8;
			B.Windage = R.PallWindage.Pick(D);
			B.Alpha = R.PallAlpha.Pick(D);
			B.Hold = R.PallHold;
			B.Rise = 0.0;
			B.Gravity = 0.0;
			const Vec Down = Vec(Wind.X, Wind.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
			B.Along = Down;
			B.Stretch = R.PallStretch * D.U(0.85, 1.15);
			B.StretchBy = R.PallStretchBy;
			B.Col2 = R.Col2 * D.U(0.95, 1.15);
			B.SunSide = R.SunSide * 0.6; // (a thin sheet aloft: lit, but less than the column's thick sunward side)
			B.Centre = S.At + ZUp * (0.5 * (R.Burst.A + R.Burst.B) * Big / std::max(R.BurstDrag, 1e-3)); // (the sheet's own height: where the push gives out)
			// (art director 04:48Z: never a bent pipe. Each pall puff levels at its own share of the push, so the elbow is
			// round and the sheet thick; and wanders slowly up or down, so it fans out down the wind and breaks into loose
			// lumps as it thins)
			if (R.PallLevel.A != 1.0 || R.PallLevel.B != 1.0)
			{
				B.Vel.Z *= R.PallLevel.Pick(D);
			}
			if (R.PallDrift > 0.0)
			{
				B.Rise = D.Bell() * R.PallDrift;
			}
			if (R.PallRough > 0.0)
			{
				B.Rough = R.PallRough;
			}
			// (seen from the air, a wide old puff that has wandered down would be cut by the sea along a straight line)
			B.Rest = R.PallRest;
		}
		Bits.push_back(B);
	}

	void Battle::Run(double Now)
	{
		// (a burst's close flames whose rounds have fired by now: a flame shorter than a frame is still laid once)
		if (!FlamesDue.empty())
		{
			size_t Left = 0;
			for (size_t I = 0; I < FlamesDue.size(); ++I)
			{
				const FlameDue& Due = FlamesDue[I];
				if (Due.Born > Now)
				{
					FlamesDue[Left++] = Due;
				}
				else if (Now - Due.Born < 0.5)
				{
					Dice D(Due.Seed);
					HotFlame(Due.Born, Due.At, Due.Fw, Due.Len, Due.Life, Due.H, D);
				}
			}
			FlamesDue.resize(Left);
		}
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Rgb Heart = Rgb::FromPicture(250, 196, 110);
		const Rgb Ember = Rgb::FromPicture(110, 35, 15);
		// (fire.smoke_rest: a fire's smoke rests on the ground or the sea under it, see Bit::Rest. A big fire's puffs grow
		// far wider than they stand high (a 40 m fire's to 160 m, 40 m up): from the air the sea cut each along a straight
		// level line, build 1746's Pearl Harbor. 0, or no such key: as before)
		const double SmokeRest = std::min(2.0, std::max(0.0, Num("fire.smoke_rest", 0.0)));
		for (Source& S : Sources)
		{
			// (after a long pause, such as the game being paused, only the last second is made up)
			S.Next = std::max(S.Next, Now - 1.0);
			S.NextSmoke = std::max(S.NextSmoke, Now - 1.0);
			if (S.Kind == 2)
			{
				// burning oil on the sea: low ragged flames on each head of the slick, and the column over them
				const OilRow& R = S.Oil;
				const Rgb OilHeart = R.Flame.Mix(Heart, 0.6);
				while (S.Next <= Now && S.Next < S.Flames)
				{
					Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(S.Next * 1000.0)) * 0x9E3779B97F4A7C15ull + 7ull));
					for (const Vec& Head : S.Heads)
					{
						if (R.FlameOdds < 1.0 && D.U() > R.FlameOdds)
						{
							continue;
						}
						const double Rf = R.FlameR.Pick(D);
						const bool bShort = D.U() < 0.25;
						Bit B;
						B.Born = S.Next;
						B.From = Head + Vec(D.Bell(), D.Bell(), 0.0) * (R.HeadR.Pick(D) * S.Across * 0.35) + ZUp * (R.FlameUp + 0.3 * Rf);
						B.Vel = ZUp * D.U(1.5, 3.5);
						B.Drag = 0.8;
						B.Windage = 0.4;
						B.Life = R.FlameLife.Pick(D) * (bShort ? 0.5 : 1.0);
						B.R0 = Rf * (bShort ? 0.6 : 1.0);
						B.R1 = Rf * 0.35;
						B.Grow = 1.0;
						B.Along = ZUp;
						B.Stretch = R.FlameTall.Pick(D);
						B.Point = R.FlamePoint;
						B.Col = bShort ? OilHeart : R.Flame;
						B.Col2 = Ember;
						B.Alpha = 0.95;
						B.FadeIn = 0.03;
						B.Hold = 0.3;
						B.Heart = 0.75;
						B.Rim = 0.1;
						B.Rough = 0.25;
						B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
						B.bLight = true;
						Bits.push_back(B);
					}
					S.Next += R.FlameEvery;
				}
				while (S.NextSmoke <= Now && S.NextSmoke < S.End)
				{
					OilColumn(S, S.NextSmoke);
					S.NextSmoke += R.ColumnEvery;
				}
				S.NextBase = std::max(S.NextBase, Now - 1.0);
				while (R.BaseEvery > 0.0 && S.NextBase <= Now && S.NextBase < S.End)
				{
					OilBase(S, S.NextBase);
					S.NextBase += R.BaseEvery;
				}
				S.NextPall = std::max(S.NextPall, Now - 1.0);
				while (R.PallEvery > 0.0 && S.NextPall <= Now && S.NextPall < S.End)
				{
					OilColumn(S, S.NextPall, true);
					S.NextPall += R.PallEvery;
				}
				continue;
			}
			if (S.Kind == 0)
			{
				// long ragged tongues rooted where it burns, dying by being eaten away; then a thinning grey column
				while (S.Next <= Now && S.Next < S.Flames)
				{
					Dice D(S.Seed ^ static_cast<uint64_t>(static_cast<int64_t>(S.Next * 1000.0)));
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
					Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(S.NextSmoke * 1000.0)) + 17ull));
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
					B.Rest = SmokeRest;
					Bits.push_back(B);
					S.NextSmoke += bFlames ? 0.12 : 0.35;
				}
			}
			else
			{
				while (S.NextSmoke <= Now && S.NextSmoke < S.End)
				{
					Dice D(S.Seed ^ static_cast<uint64_t>(static_cast<int64_t>(S.NextSmoke * 1000.0)));
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
					B.Rest = SmokeRest;
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
		Falls.erase(std::remove_if(Falls.begin(), Falls.end(), [Now](const Falling& Fa) { return Fa.Left <= 0 || Now - Fa.Last > 30.0; }), Falls.end()); // (all its pieces shed, or never moved again)
		Dusts.erase(std::remove_if(Dusts.begin(), Dusts.end(), [Now](const Dusting& Du) { return Now - Du.Last > 10.0; }), Dusts.end()); // (a vehicle gone, or stopped calling)
		Knocks.erase(std::remove_if(Knocks.begin(), Knocks.end(), [Now](const Knock& J) { return Now > J.Born + 1.0; }), Knocks.end());
		Sources.erase(std::remove_if(Sources.begin(), Sources.end(), [Now](const Source& S) { return Now > S.End; }), Sources.end());
	}

	bool Battle::Busy(double Now) const
	{
		(void)Now;
		return !Bits.empty() || !Marks.empty() || !Sources.empty() || !Flashes.empty() || !FlamesDue.empty();
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Drawing

	void Battle::Lay(int Slot, const Vec& Middle, const Vec& AxisA, const Vec& AxisB, const Rgb& Col, double Alpha, double HeartAlpha, double RimAlpha, double Inner, double Point, double Tail, double Shade, const View& Eye, double Rough, uint32_t LumpSeed, double Turn, const Rgb& Floor, const Rgb& Through, const Vec& Outward, const Rgb& Lift, const Vec& TaperB)
	{
		const size_t First = static_cast<size_t>(Slot) * PointsPerPuff;
		auto Put = [this](size_t I, const Vec& At, const Rgb& C, double A)
		{
			Out[I] = At;
			OutRgba[I * 4 + 0] = Byte(C.R * PutK);
			OutRgba[I * 4 + 1] = Byte(C.G * PutK);
			OutRgba[I * 4 + 2] = Byte(C.B * PutK);
			OutRgba[I * 4 + 3] = Byte(A);
		};
		// (Lift: the sunlit face of smoke seen with the sun behind the eye. Art director 23:59Z: each lobe a dome, its
		// crown on the side toward the sun lit and its underside and edge darker, so the lobes count as lumps. The
		// middle takes the ring's mean, so the crown is a smooth slope across the lobe and never a star of spokes)
		const bool bLift = std::fabs(Lift.R) + std::fabs(Lift.G) + std::fabs(Lift.B) > 1e-6f;
		// (Round: an oil column's puff is lit as a part of a round column, each point by the column's own surface where it
		// sits across it on the screen, so the sunward side is lit and the far side stays dark however big the puff)
		auto RoundK = [this](const Vec& Q)
		{
			const double S = std::clamp((Q.X * Round.Right.X + Q.Y * Round.Right.Y - Round.Mid) / Round.R, -1.0, 1.0);
			const Vec N = Round.Right * S + Round.Back * std::sqrt(1.0 - S * S);
			return Clamp01(Round.Gain * std::max(0.0, Round.Level * N.Dot(Round.Sun) + Round.Up));
		};
		const float KMid = Round.bOn ? static_cast<float>(0.9 * RoundK(Middle)) : 0.75f;
		// (a spent case's body: lit as a tube, each point by the tube's own surface where it sits across it)
		auto TubeK = [this](const Vec& Q)
		{
			const double S = std::clamp((Q - Tube.Mid).Dot(Tube.Across) / Tube.R, -1.0, 1.0);
			const Vec N = Tube.Across * S + Tube.Back * std::sqrt(1.0 - S * S);
			const double Lit_ = std::max(0.0, N.Dot(Tube.Sun));
			const double Spot = std::pow(std::max(0.0, N.Dot(Tube.Half)), Tube.Shine);
			return Rgb(static_cast<float>(Tube.Shade.R + (Tube.Lit.R - Tube.Shade.R) * Lit_ + Tube.Spark.R * Spot),
				static_cast<float>(Tube.Shade.G + (Tube.Lit.G - Tube.Shade.G) * Lit_ + Tube.Spark.G * Spot),
				static_cast<float>(Tube.Shade.B + (Tube.Lit.B - Tube.Shade.B) * Lit_ + Tube.Spark.B * Spot));
		};
		const Rgb Mid = Tube.bOn ? TubeK(Middle) : bLift ? Rgb(Col.R + KMid * Lift.R, Col.G + KMid * Lift.G, Col.B + KMid * Lift.B) : Col;
		Put(First, Middle, Rgb(std::max(Mid.R, Floor.R), std::max(Mid.G, Floor.G), std::max(Mid.B, Floor.B)), bLayLong ? std::min(1.0, Alpha * (1.0 + 0.5 * (LayTailAlpha - 1.0) * (1.0 - Clamp01(2.0 * LayLump)))) : Alpha);
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
		const bool bThrough = Through.R + Through.G + Through.B > 1e-6f;
		const double SunAcross = std::sqrt(std::max(0.0, 1.0 - Eye.Forward.Dot(Eye.Sun) * Eye.Forward.Dot(Eye.Sun))); // (how far the sun is off the eye's line, on the screen)
		const bool bTaper = TaperB.Dot(TaperB) > 0.0;
		for (int K = 0; K < Sides; ++K)
		{
			const size_t I = static_cast<size_t>(K);
			double Cs = RingCos[I], Sn = RingSin[I];
			if (LaySquare > 0.0)
			{
				// (a squared ring: each point pushed out toward the corners of its box, so the outline has straight sides)
				const double Pw = 1.0 - std::min(LaySquare, 0.9);
				Cs = std::copysign(std::pow(std::fabs(Cs), Pw), Cs);
				Sn = std::copysign(std::pow(std::fabs(Sn), Pw), Sn);
			}
			// (TaperB: a tracer's width running along it, so it is as wide on the screen from end to end)
			const Vec Dir = bTaper ? AxisA * Cs + AxisB * Sn + TaperB * (Sn * Cs) : AxisA * Cs + AxisB * Sn;
			// (a ribbon's piece: the ring laid round a length of strip. Its first point is the middle of the near edge and
			// its half-way point the middle of the far edge; the points next to those are the four corners; the rest
			// run down the two long sides. Along: 1 at the near edge, -1 at the far; OutSide: 0 at an edge's middle)
			double Along = 0.0, OutSide = 0.0;
			if (bLayLong)
			{
				const int Half_ = Sides / 2;
				const int Kk = K <= Half_ ? K : Sides - K;
				OutSide = Kk == 0 || Kk == Half_ ? 0.0 : (K < Half_ ? 1.0 : -1.0);
				Along = Kk <= 1 ? 1.0 : Kk >= Half_ - 1 ? -1.0 : 1.0 - 2.0 * (Kk - 1) / static_cast<double>(std::max(Half_ - 2, 1));
			}
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
			if (bLift)
			{
				const double Len2 = AA * Cs * Cs + BB * Sn * Sn + 2.0 * AB * Cs * Sn;
				const double Toward = Len2 > 1e-18 ? (SunA * Cs + SunB * Sn) / std::sqrt(Len2) : 0.0;
				const double Lean = Clamp01(0.5 + 0.5 * Toward / std::max(SunAcross, 0.05)); // (1 on the side toward the sun on the screen)
				// (art director 00:03Z: the pale fades with the puff's own edge opacity, so the rim goes see-through grey)
				const double HeartK = Clamp01(HeartAlpha / std::max(Alpha, 1e-6)), RimK = Clamp01(RimAlpha / std::max(Alpha, 1e-6));
				float KHeart = static_cast<float>((0.1 + 0.9 * Lean) * HeartK), KRim = static_cast<float>(0.1 * Lean * RimK);
				if (Round.bOn)
				{
					// (each lobe still a little dome: a quarter of its light from its own sunward side)
					const double Lobe = 0.75 + 0.25 * Lean;
					KHeart = static_cast<float>(RoundK(Middle + Dir * (In * Reach)) * Lobe * HeartK);
					KRim = static_cast<float>(0.5 * RoundK(Middle + Dir * Reach) * Lobe * RimK);
				}
				HeartShaded = Rgb(HeartShaded.R + KHeart * Lift.R, HeartShaded.G + KHeart * Lift.G, HeartShaded.B + KHeart * Lift.B);
				Shaded = Rgb(Shaded.R + KRim * Lift.R, Shaded.G + KRim * Lift.G, Shaded.B + KRim * Lift.B);
			}
			if (Tube.bOn)
			{
				HeartShaded = TubeK(Middle + Dir * (In * Reach));
				Shaded = TubeK(Middle + Dir * Reach);
			}
			Shaded = Rgb(std::max(Shaded.R, Floor.R), std::max(Shaded.G, Floor.G), std::max(Shaded.B, Floor.B));
			HeartShaded = Rgb(std::max(HeartShaded.R, Floor.R), std::max(HeartShaded.G, Floor.G), std::max(HeartShaded.B, Floor.B));
			if (EdgeK > 0.0)
			{
				Shaded = Shaded.Mix(EdgeTo, EdgeK);
				HeartShaded = HeartShaded.Mix(EdgeTo, EdgeIn * EdgeK);
			}
			if (bThrough)
			{
				// (the sun through the thin edge: all round, most on the side toward the sun on the screen)
				const double Len2 = AA * Cs * Cs + BB * Sn * Sn + 2.0 * AB * Cs * Sn;
				const double Toward = Len2 > 1e-18 ? (SunA * Cs + SunB * Sn) / std::sqrt(Len2) : 0.0;
				double Side = 0.6 + 0.4 * Clamp01(Toward / std::max(SunAcross, 0.05));
				if (Outward.Length() > 0.5)
				{
					Side *= Clamp01(Dir.Dot(Outward) / std::max(std::sqrt(Len2), 1e-9)); // (a cloud's puff: only its edge facing out of the cloud)
				}
				const Rgb Lit = Through * Side;
				Shaded = Rgb(Shaded.R + Lit.R, Shaded.G + Lit.G, Shaded.B + Lit.B);
				HeartShaded = Rgb(HeartShaded.R + 0.1f * Lit.R, HeartShaded.G + 0.1f * Lit.G, HeartShaded.B + 0.1f * Lit.B);
			}
			if (bLayLong)
			{
				// (as wide and as dark at each end as the trail is there; the lumpy outline only along its two sides, its
				// corners where the next piece's are. Its line bows between its edges, leaving each along the path there.
				// The middle of each edge is dense, as the core is, while it is a link of the strip. Toward the trail's
				// old end each end first lets go of the piece next to it: the piece pales from its middle to nothing at
				// that edge, as the next piece's end does, so the strip thins between its lumps before it parts; then
				// the piece draws in, each point to its place on a round puff, a share of its length long. The far end,
				// the older, goes first)
				const double U = 0.5 * (1.0 + Along), T = 1.0 - U; // (T: 0 at the near edge, 1 at the far)
				const double Wob = std::fabs(Along) < 0.999 ? 1.0 + (Reach - 1.0) * LayWob : 1.0;
				const Vec OnStrip = Middle + LayLong * Along + LayBendFar * (((U - 2.0) * U + 1.0) * U - 0.125) + LayBendNear * ((U - 1.0) * U * U + 0.125);
				const double LumpAt = LayLump + (LayLumpFar - LayLump) * T;
				const double Let = Clamp01(4.0 * LumpAt), Soft = Let * Let * (3.0 - 2.0 * Let);
				const double In_ = Clamp01((LumpAt - 0.1) / 0.5), Draw = In_ * In_ * (3.0 - 2.0 * In_);
				// (and it narrows toward that edge as it pales, so its shoulders are round, never a straight cut across)
				const Vec Edge = (AxisB + (LayFar - AxisB) * T) * (OutSide * Wob * (1.0 - 0.7 * Soft * Along * Along));
				const double Dark = (1.0 + (LayTailAlpha - 1.0) * T * (1.0 - Draw)) * (1.0 - Soft * std::fabs(Along) * (1.0 - Draw));
				const double RimA = OutSide == 0.0 ? HeartAlpha + (RimAlpha - HeartAlpha) * Draw : RimAlpha;
				const double InAt = In + std::max(0.0, 0.5 - In) * Soft; // (a lump is dense half way out, where a link is only at its core)
				const Vec HeartAt = OnStrip + Edge * InAt, RimAt = OnStrip + Edge;
				if (Draw > 0.0)
				{
					Put(First + 1 + I, HeartAt + (Middle + Dir * (InAt * Reach) - HeartAt) * Draw, HeartShaded, std::min(1.0, HeartAlpha * Dark));
					Put(First + 1 + static_cast<size_t>(Sides) + I, RimAt + (Middle + Dir * Reach - RimAt) * Draw, Shaded, std::min(1.0, RimA * Dark));
				}
				else
				{
					Put(First + 1 + I, HeartAt, HeartShaded, std::min(1.0, HeartAlpha * Dark));
					Put(First + 1 + static_cast<size_t>(Sides) + I, RimAt, Shaded, std::min(1.0, RimA * Dark));
				}
				continue;
			}
			if (LayInnerAlong > 0.0)
			{
				const Vec Within = bTaper ? AxisA * (Cs * LayInnerAlong) + (AxisB * Sn + TaperB * (Sn * Cs)) * In : AxisA * (Cs * LayInnerAlong) + AxisB * (Sn * In);
				Put(First + 1 + I, Middle + Within * Reach, HeartShaded, HeartAlpha);
			}
			else
			{
				Put(First + 1 + I, Middle + Dir * (In * Reach), HeartShaded, HeartAlpha);
			}
			Put(First + 1 + static_cast<size_t>(Sides) + I, Middle + Dir * Reach, Shaded, RimAlpha);
		}
	}

	int Battle::LayCase(int Slot, const Bit& B, double Age, const Vec& Seen, double A, const View& Eye, int Room)
	{
		// A ground gun's spent case (art director 07:08Z): a brass tube tumbling end over end until it lands, then lying
		// level. Laid as up to three fans, back to front: a soft shadow on the ground under it once it lies there; the
		// tube, its outline square-ended and straight-sided from wherever the eye is, lit across it as metal is (a bright
		// band toward the sun with a glint, a shade side and underside); and the dark open mouth at one end when that end
		// is turned toward the eye, its brass lip round it
		double Rad = B.R0;
		const double Th = B.Tumble * std::min(Age, B.Land2);
		const Vec Axis = (B.Along * std::cos(Th) + B.Face * std::sin(Th)).Unit(B.Along);
		const Vec ToEye = (Eye.Eye - Seen).Unit(-Eye.Forward);
		const Vec Lv = Axis.Cross(ToEye).Unit(Eye.Up);
		const Vec Back = (ToEye - Axis * ToEye.Dot(Axis)).Unit(ToEye);
		const Vec Sun = Eye.Sun.Unit(ZUp);
		double Long = Rad * B.Stretch;
		auto Times = [](const Rgb& X, const Rgb& Y) { return Rgb(X.R * Y.R, X.G * Y.G, X.B * Y.B); };
		const double Off = (Seen - Eye.Eye).Length();
		A *= Clamp01((CaseFar - Off) / (0.25 * CaseFar)); // (fading out over the last quarter of draw_within_m)
		// (held: 0 close to, where it is as the art director passed it at 07:25Z; 1 at its least width)
		double Held = 0.0;
		if (CaseHoldPx > 0.0)
		{
			const double Px = 2.0 * Rad * std::max(Eye.PixelsPerRadian, 1.0) / std::max(Off, 1e-3); // (its true width on the screen)
			Held = CaseDetailPx > CaseHoldPx ? Clamp01((CaseDetailPx - Px) / (CaseDetailPx - CaseHoldPx)) : (Px < CaseHoldPx ? 1.0 : 0.0);
			if (Px < CaseHoldPx)
			{
				A *= Clamp01((Px - CaseGonePx) / std::max(CaseFullPx - CaseGonePx, 1e-6));
				Rad = 0.5 * CaseHoldPx * Off / std::max(Eye.PixelsPerRadian, 1.0);
				Long = std::max(Long, 2.5 * Rad); // (still longer than it is wide: a case, never a dot)
			}
		}
		if (A < 0.004)
		{
			return 0;
		}
		Vec P = Seen;
		if (CaseNearer > 0.0 && Off > 4.0 * CaseNearer)
		{
			// (nearer the eye along its own line of sight and smaller to match: the same picture, in front of the ground)
			const double K = 1.0 - CaseNearer / Off;
			P = Eye.Eye + (Seen - Eye.Eye) * K;
			Rad *= K;
			Long *= K;
		}
		int Used = 0;
		LaySquare = 0.0;
		Tube.bOn = false;
		EdgeK = 0.0;
		const bool bDetail = CaseDetailed(B.R0, (Seen - Eye.Eye).Dot(Eye.Forward), Eye); // (under detail_px across, the tube alone)
		if (bDetail && Age >= B.Land2 && CaseShadow > 0.0 && Room - Used >= 3)
		{
			// (the contact shadow: on the ground under the case, a little away from the sun, soft-edged)
			const Vec Level = Vec(Axis.X, Axis.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
			const Vec SunAway = Vec(-Sun.X, -Sun.Y, 0.0) * (Rad * 0.8);
			const Vec Under = Vec(P.X, P.Y, P.Z - Rad + 0.004) + SunAway;
			LaySquare = 0.45;
			Lay(Slot + Used, Under, Level * (Long * 1.08), Level.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0)) * (Rad * 1.5), Rgb::FromPicture(22, 19, 13), A * CaseShadow, A * CaseShadow * 0.85, 0.0, 0.6, 0.0, 0.0, 0.0, Eye, 0.0, 0u, 0.0, Rgb(), Rgb(), Vec(), Rgb());
			++Used;
		}
		if (Room - Used < 1)
		{
			LaySquare = 0.0;
			return Used;
		}
		// (the tube)
		Tube.bOn = true;
		Tube.Mid = P;
		Tube.Across = Lv;
		Tube.Back = Back;
		Tube.Sun = Sun;
		Tube.Half = (Sun + ToEye).Unit(ZUp);
		Tube.R = Rad;
		Tube.Shine = CaseFarShine > 0.0 ? CaseShine + (CaseFarShine - CaseShine) * Held : CaseShine;
		Tube.Shade = Times(B.Col2, Eye.SmokeLight) * (1.0 + (CaseFarShade - 1.0) * Held);
		Tube.Lit = Times(B.Col, Eye.SmokeLight);
		Tube.Spark = Times(Eye.SunLight, CaseGlint) * (B.Glint * 0.12 * (1.0 + (CaseFarGlint - 1.0) * Held)); // (brass is a metal: its glint takes its own gold)
		LaySquare = 0.75; // (square ends and straight sides: a case, never a grain of rice)
		Lay(Slot + Used, P, Axis * Long, Lv * Rad, B.Col, A, A * B.Heart, A * B.Rim, B.Inner, 0.0, 0.0, 0.0, Eye, 0.0, 0u, 0.0, Rgb(), Rgb(), Vec(), Rgb());
		++Used;
		Tube.bOn = false;
		const double Facing = Axis.Dot(ToEye);
		if (bDetail && Facing > 0.08 && Room - Used >= 1)
		{
			// (the open mouth, turned toward the eye: the dark inside, its rim the brass lip lit as the tube's top)
			const Vec Cross_ = Axis.Cross(Lv).Unit(Eye.Up);
			LaySquare = 0.0;
			EdgeTo = Times(B.Col, Eye.SmokeLight);
			EdgeK = 0.85;
			Lay(Slot + Used, P + Axis * Long, Lv * (Rad * 0.92), Cross_ * (Rad * 0.92), Times(CaseMouth, Eye.SmokeLight), A, A, A * 0.95, 0.72, 0.0, 0.0, 0.0, Eye, 0.0, 0u, 0.0, Rgb(), Rgb(), Vec(), Rgb());
			EdgeK = 0.0;
			++Used;
		}
		LaySquare = 0.0;
		return Used;
	}

	int Battle::LayBit(int Slot, const Bit& B, double Now, const View& Eye, int Room)
	{
		const double Age = Now - B.Born;
		if (Age < 0.0 || (Age > B.Life && !Unseen(B)))
		{
			return 0;
		}
		// (a light shorter than a frame that no frame has caught yet is laid now, where it is, at its full strength)
		const double Shown = Unseen(B) && Age > B.Life * std::min(B.Hold, 0.5) ? B.Life * std::min(B.Hold, 0.5) : Age;
		const double F = Shown / B.Life;
		double Rad = B.R0 + (B.R1 - B.R0) * std::pow(F, B.Grow);
		// (a ribbon's piece, in two stages: see Bit::GrowBy)
		auto RadTwo = [](double R0_, double RMid_, double R1_, double Age_, double Life_, double By_, double Early_, double Late_)
		{
			return R0_ + (RMid_ - R0_) * std::pow(Clamp01(Age_ / By_), Early_) + (R1_ - RMid_) * std::pow(Clamp01((Age_ - By_) / std::max(Life_ - By_, 1e-6)), Late_);
		};
		if (B.GrowBy > 0.0)
		{
			Rad = RadTwo(B.R0, B.RMid, B.R1, Shown, B.Life, B.GrowBy, B.GrowEarly, B.Grow);
		}
		const double Fade = std::min(1.0, Shown / std::max(B.FadeIn, 1e-3)) * (F < B.Hold ? 1.0 : std::pow(std::max(0.0, 1.0 - (F - B.Hold) / std::max(1.0 - B.Hold, 1e-6)), 1.4));
		double A = B.Alpha * Fade;
		// (a ribbon's piece pales as it widens: see Bit::PaleBy. How dark it is at this age, as a share of how it was laid)
		auto Paled = [&B](double Age_)
		{
			const double G = Clamp01((Age_ - B.PaleFrom) / std::max(B.PaleBy - B.PaleFrom, 1e-6));
			return 1.0 - (1.0 - B.PaleTo) * G * G * (3.0 - 2.0 * G);
		};
		if (B.PaleBy > 0.0)
		{
			A *= Paled(Age);
		}
		if (B.DayAlpha != 1.0)
		{
			A *= 1.0 + (B.DayAlpha - 1.0) * DayOf(Eye.SmokeLight);
		}
		if (B.HazeBy > 0.0)
		{
			const double T = Clamp01(Age / B.HazeBy);
			A *= 1.0 + (B.HazeAlpha / std::max(B.Alpha, 1e-6) - 1.0) * T;
		}
		if (B.ShowBy > 0.0)
		{
			// (a ribbon's bar that hangs on: coming into sight as the short pieces it stands in for go)
			const double T = Clamp01((Age - B.ShowFrom) / std::max(B.ShowBy - B.ShowFrom, 1e-6));
			A *= T * T * (3.0 - 2.0 * T);
		}
		// (a ribbon's piece reaches back to the piece behind: how dark that one is now, where the two meet, and how far it
		// has drawn in to a lump. Its far edge takes both, so the two pieces are one strip at the edge they share)
		const bool bReach = B.Ribbon > 0.0 && B.Back.bOn && B.Form == Bit::Shape::Puff;
		double TailA = 0.0, LumpFar = 0.0;
		if (bReach)
		{
			const double AgeT = Now - B.Back.Born;
			const double FT = Clamp01(AgeT / std::max(B.Back.Life, 1e-6));
			if (AgeT <= B.Back.Life)
			{
				TailA = B.Back.Alpha * std::min(1.0, AgeT / std::max(B.FadeIn, 1e-3)) * (FT < B.Hold ? 1.0 : std::pow(std::max(0.0, 1.0 - (FT - B.Hold) / std::max(1.0 - B.Hold, 1e-6)), 1.4));
				if (B.DayAlpha != 1.0)
				{
					TailA *= 1.0 + (B.DayAlpha - 1.0) * DayOf(Eye.SmokeLight);
				}
				if (B.ShowBy > 0.0)
				{
					const double T = Clamp01((AgeT - B.ShowFrom) / std::max(B.ShowBy - B.ShowFrom, 1e-6));
					TailA *= T * T * (3.0 - 2.0 * T);
				}
				if (B.PaleBy > 0.0)
				{
					TailA *= Paled(AgeT);
				}
			}
			if (B.Back.LumpsFrom >= 0.0 && FT > B.Back.LumpsFrom)
			{
				LumpFar = Clamp01((FT - B.Back.LumpsFrom) / std::max(1.0 - B.Back.LumpsFrom, 1e-6));
			}
		}
		if (A < 0.004 && TailA < 0.004)
		{
			return 0;
		}
		if (bReach)
		{
			A = std::max(A, 1e-3); // (unseen at its own end yet, it is still the strip up to the piece behind)
		}
		Rgb Col = B.Col.Mix(B.Col2, F * B.Cool);
		Rgb Floor, Through, Lift;
		if (!B.bLight)
		{
			Col = Rgb(Col.R * Eye.SmokeLight.R, Col.G * Eye.SmokeLight.G, Col.B * Eye.SmokeLight.B);
			Floor = Col * SkyFill; // (art director 22:24Z: the sky lights smoke's shadow side too, so it is never a hole)
		}
		Vec P = Where(B, Age);
		if (B.Rest > 0.0)
		{
			P.Z = std::max(P.Z, Height(P.X, P.Y) + B.Rest * Rad);
		}
		const double Dist = (P - Eye.Eye).Length();
		if (Dist > (B.bKeep ? KeepBeyond : Beyond))
		{
			return 0;
		}
		if (B.Thin != 0 && SteepThin > 0.0)
		{
			// (from a high steep eye, dust near a unit thins: how near, its edge counted, to the nearest dust socket or its gun)
			double Near2 = 1e18;
			if (B.Thin == 2)
			{
				Near2 = (P.X - B.Origin.X) * (P.X - B.Origin.X) + (P.Y - B.Origin.Y) * (P.Y - B.Origin.Y);
			}
			else
			{
				for (const Vec& Q : NearSockets)
				{
					Near2 = std::min(Near2, (P.X - Q.X) * (P.X - Q.X) + (P.Y - Q.Y) * (P.Y - Q.Y));
				}
			}
			const double Near = std::sqrt(Near2) - 0.5 * Rad;
			const double T = Clamp01((Near - ThinNear.A) / std::max(ThinNear.B - ThinNear.A, 1e-6));
			const double Close = 1.0 - T * T * (3.0 - 2.0 * T);
			A *= 1.0 - SteepThin * Close * (1.0 - ThinKeep);
			if (A < 0.004)
			{
				return 0;
			}
		}
		if (NearWhole > 0.0 && !B.bLight && !B.bThrown && !B.bKeep && B.Ribbon <= 0.0 && B.Smear <= 0.0 && (B.Form == Bit::Shape::Puff || B.Form == Bit::Shape::Upright))
		{
			// (smoke and dust thins out as it comes up to the eye, by its near edge, its long way counted: see NearGone.
			// Never a light, a thrown clod, an aircraft's trail or an oil fire's column)
			const double Edge = Dist - Rad * std::max(1.0, B.Stretch);
			if (Edge < NearWhole)
			{
				const double T = Clamp01((Edge - NearGone) / (NearWhole - NearGone));
				A *= T * T * (3.0 - 2.0 * T);
				if (A < 0.004)
				{
					return 0;
				}
			}
		}
		double Outer = 1.0; // (how near the puff is to its cloud's edge: only a cloud's own fringe lets the sun through)
		Vec Outward; // (and which way on the screen is out of its cloud)
		if (B.CloudShade > 0.0 && !B.bLight)
		{
			// (the cloud's sunward side lit, its far side in its own shadow: shading each puff alone barely shows
			// once many overlap)
			const Vec FromMid = P - (B.Centre + Wind * (0.75 * Age) + ZUp * (B.CentreRise * Age));
			const double Len = FromMid.Length();
			Outer = Clamp01(Len / std::max(B.CloudR, 1e-3));
			Outward = (FromMid - Eye.Forward * FromMid.Dot(Eye.Forward)).Unit(Vec());
			if (Len > 1e-3)
			{
				const double Toward = FromMid.Dot(Eye.Sun) / Len * Clamp01(Len / std::max(B.CloudR, 1e-3));
				Col = Col * std::max(0.3, 1.0 + B.CloudShade * (ShadeLit * std::max(0.0, Toward) - ShadeDark * std::max(0.0, -Toward)));
				if (B.FaceLit > 0.0 && Dist > 1e-6)
				{
					// (art director 23:44Z: flak seen from above with the sun behind the eye shows its sunlit face in a
					// light grey, dark only in its hollows, its far side and its young heart; seen side-on or against
					// the sun it keeps the look she passed. Most on the cloud's thin outer lobes and wisps and on its
					// sunward side; next to none in its dense young heart, where the lobes are still close in, nor on
					// its far side. Laid by Lay: all of it at a lobe's middle, less toward its edge)
					const double Face = (Eye.Eye - P).Dot(Eye.Sun) / Dist;
					const double Day = (Eye.SunLight.R + Eye.SunLight.G + Eye.SunLight.B) / 3.0;
					const double W = Clamp01(1.2 * Outer + 1.5 * Toward - 0.15);
					const float L = static_cast<float>(Clamp01(B.FaceLit * std::max(0.0, Face) * W * std::sqrt(W) * Day));
					Lift = Rgb((B.LitCol.R * Eye.SmokeLight.R - Col.R) * L, (B.LitCol.G * Eye.SmokeLight.G - Col.G) * L, (B.LitCol.B * Eye.SmokeLight.B - Col.B) * L);
				}
				Col = Rgb(std::max(Col.R, Floor.R), std::max(Col.G, Floor.G), std::max(Col.B, Floor.B));
			}
		}
		Round.bOn = false;
		LaySquare = 0.0;
		LayInnerAlong = 0.0;
		bLayLong = false;
		LayTailAlpha = 1.0;
		LayLump = LayLumpFar = 0.0;
		LayWob = 1.0;
		EdgeK = B.EdgeK;
		EdgeTo = B.Edge;
		EdgeIn = B.EdgeIn;
		PutK = (B.bLight ? std::clamp(B.Over, 1.0, LitGain) : 1.0) / LitGain;
		if (B.SunSide > 0.0 && !B.bLight && Dist > 1e-6)
		{
			// (art director 03:59Z: an oil column reads round, never a flat ink cut-out: each lobe's sunward side lit up
			// toward a brown-grey, its shadow side kept near black. The more the sun is behind the eye, the more of the
			// face is lit; against the sun next to none, and the warm fringe below shows instead. None at night)
			const double Face = (Eye.Eye - P).Dot(Eye.Sun) / Dist;
			const double Day = (Eye.SunLight.R + Eye.SunLight.G + Eye.SunLight.B) / 3.0;
			// (which side of the column it is on: out from the column's axis, level, against the sun's way, in its radii)
			Vec Off = P - (B.Centre + Wind * (B.AxisWindage * Age));
			const double Rise = Off.Z;
			Off.Z = 0.0;
			if (B.StretchBy > 0.0)
			{
				Off = Off - B.Along * Off.Dot(B.Along); // (the pall runs along the wind: only how far it is off that line counts)
			}
			const Vec SunLevel = Vec(Eye.Sun.X, Eye.Sun.Y, 0.0).Unit(Vec());
			// (only the column's own fringe lets the sun through, never every lobe's edge inside it: the column's by how
			// near the puff is to its edge across the screen, the pall's by how near it is to its top or underside)
			Outer = 0.0;
			Outward = Vec();
			if (B.StretchBy > 0.0)
			{
				if (Rise > -Rad)
				{
					Outer = Clamp01(1.5 * std::fabs(Rise) / std::max(Rad, 1e-3) - 0.6);
					Outward = Rise >= 0.0 ? Eye.Up : Eye.Up * -1.0;
				}
			}
			else
			{
				double SpanMid = 0.0, SpanHalf = 0.0;
				if (SpanAt(B.OilId, P.Z, SpanMid, SpanHalf))
				{
					const double Across = (P.X * Eye.Right.X + P.Y * Eye.Right.Y) / std::max(std::hypot(Eye.Right.X, Eye.Right.Y), 1e-6) - SpanMid;
					Outer = Clamp01(2.0 * std::fabs(Across) / std::max(SpanHalf, 1e-3) - 0.8);
					Outward = Across >= 0.0 ? Eye.Right : Eye.Right * -1.0;
				}
			}
			if (B.StretchBy > 0.0)
			{
				// (the pall: a thin sheet aloft, lit lobe by lobe on the side of it toward the sun)
				const double Side = std::clamp(1.5 * Off.Dot(SunLevel) / std::max(Rad, 1e-3), -1.0, 1.0);
				const double W = Clamp01(0.1 + Side);
				const float L = static_cast<float>(Clamp01(B.SunSide * Day * W * (0.15 + 0.85 * Clamp01(0.65 + 0.6 * Face))));
				Lift = Rgb((B.LitCol.R * Eye.SmokeLight.R - Col.R) * L, (B.LitCol.G * Eye.SmokeLight.G - Col.G) * L, (B.LitCol.B * Eye.SmokeLight.B - Col.B) * L);
			}
			else if (Day > 1e-6)
			{
				// (the column: lit as a round column, Lay lighting each point by the column's surface where it sits across it.
				// Its sunward side lit, its far side dark, the sun high lighting a little all round; against the sun none)
				Round.bOn = true;
				Round.Right = Vec(Eye.Right.X, Eye.Right.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
				Round.Back = Vec(-Eye.Forward.X, -Eye.Forward.Y, 0.0).Unit(Vec(0.0, -1.0, 0.0));
				Round.Sun = SunLevel;
				// (the column as the eye sees it: its middle and half its width at this height this frame, from all its
				// puffs; failing that, its axis carried by the wind and this puff's own size)
				const Vec Axis = B.Centre + Wind * (B.AxisWindage * Age);
				Round.Mid = Axis.X * Round.Right.X + Axis.Y * Round.Right.Y;
				Round.R = std::max(Rad, 1e-3);
				double SpanMid = 0.0, SpanHalf = 0.0;
				if (SpanAt(B.OilId, P.Z, SpanMid, SpanHalf))
				{
					Round.Mid = SpanMid;
					Round.R = std::max(SpanHalf, 1e-3);
				}
				Round.Gain = B.SunSide * Day;
				Round.Level = std::sqrt(std::max(0.0, 1.0 - Eye.Sun.Z * Eye.Sun.Z));
				Round.Up = RoundUp * std::max(0.0, Eye.Sun.Z);
				Lift = Rgb(B.LitCol.R * Eye.SmokeLight.R - Col.R, B.LitCol.G * Eye.SmokeLight.G - Col.G, B.LitCol.B * Eye.SmokeLight.B - Col.B);
			}
		}
		if (!B.bLight && (B.CloudShade > 0.0 || B.Smear > 0.0 || B.SunSide > 0.0) && SunThrough > 0.0 && Dist > 1e-6)
		{
			// (smoke clouds and trails only: a low sheet of dust along the ground would glow as a fog over the field)
			// (art director 22:24Z: with the sun behind it, smoke's thin edges glow warm where the sun shows through,
			// most when the eye looks almost into the sun; a cloud's fringe, never its thick body. Most at middle
			// thickness (art director 22:36Z): near see-through dust stays near unseen, never a glowing fog sheet)
			const double Back = (P - Eye.Eye).Dot(Eye.Sun) / Dist;
			if (Back > 0.0)
			{
				const double Thick = Clamp01(A);
				const double K = SunThrough * std::pow(Back, SunThroughPow) * 4.0 * Thick * (1.0 - Thick) * Outer * Outer;
				Through = Rgb(SunThroughCol.R * Eye.SunLight.R, SunThroughCol.G * Eye.SunLight.G, SunThroughCol.B * Eye.SunLight.B) * K;
			}
		}
		const double MetresAPixel = Dist / std::max(1.0, Eye.PixelsPerRadian); // (1 first: a scale that is not a number counts as 1)
		const double StretchNow = B.StretchBy > 0.0 ? 1.0 + (B.Stretch - 1.0) * Clamp01(Age / B.StretchBy) : B.Stretch; // (a pall drawn out as it runs)
		if ((B.Form == Bit::Shape::Puff || B.Form == Bit::Shape::Plate) && B.LeastPx > 0.0)
		{
			// (a puff never shrinks on the screen below LeastPx across its long way: far off it is held at that size and
			// made thinner to match, so a far burst stays see-through smoke and never a solid dot. A drawn-out puff is
			// held by its length, so a thin wisp stays thin and never outgrows the lobes it trails from)
			double Long = 1.0;
			if (B.Smear <= 0.0 && StretchNow > 1.0001 && B.Along.Length() > 0.5)
			{
				Long = 1.0 + (StretchNow - 1.0) * (B.Along - Eye.Forward * B.Along.Dot(Eye.Forward)).Length();
			}
			const double Least = 0.5 * B.LeastPx * MetresAPixel;
			if (Rad * Long < Least)
			{
				// (held up, it keeps at least LeastKeep of its opacity: a light's twinkle, a far trail's dark line)
				A *= std::max(B.bLight ? std::max(0.8, B.LeastKeep) : B.LeastKeep, std::sqrt(std::max(0.0, Rad * Long) / Least));
				Rad = Least / Long;
			}
		}
		Vec AxA = Eye.Right * Rad, AxB = Eye.Up * Rad;
		Vec Middle = P;
		Vec TaperB; // (a tracer: how its width runs along it, so it is as wide on the screen at both ends)
		double PointNow = B.Point, TailNow = B.Tail; // (a tracer cut short at an end is blunter there)
		double RimNow = B.Rim, InnerNow = B.Inner, HeartUp = 0.0; // (a tracer by day is solid across: its edge and heart brought up)
		switch (B.Form)
		{
		case Bit::Shape::Puff:
			if (B.Ribbon > 0.0)
			{
				// (a ribbon trail's piece: a length of one strip from its own point back to the piece laid before it, where
				// that one is now and as wide and as dark there as it is, its two ends where they truly are in the air.
				// Its near edge lies square across its own run; its far edge is the piece behind's near edge, square
				// across that one's run, so the two pieces share the edge exactly: one strip, with no overlap to darken
				// a join and no gap, round any bend and from wherever the eye is. Dense to its core's edge all along,
				// thinning from there to nothing at its two sides)
				InnerNow = std::clamp(B.Ribbon, 0.1, 0.9);
				HeartUp = 1.0;
				if (bReach)
				{
					const Bit::Reach& T = B.Back;
					const double AgeT = Now - T.Born;
					const double GoT = B.Drag > 1e-6 ? (1.0 - std::exp(-B.Drag * AgeT)) / B.Drag : AgeT;
					const Vec Far = T.From + T.Vel * GoT + Wind * (B.Windage * AgeT) + Vec(0.0, 0.0, T.Rise * AgeT);
					// (the far edge is the piece before's own near edge: by that piece's own clock, which is another row's where the
					// trail changed kind at this join, glycol to fire. Pre-check 16:22Z)
					double RadT = T.GrowBy > 0.0 ? RadTwo(T.R0, T.RMid, T.R1, AgeT, T.Life, T.GrowBy, T.GrowEarly, T.Grow) : T.R0 + (T.R1 - T.R0) * std::pow(Clamp01(AgeT / std::max(T.Life, 1e-6)), T.Grow);
					if (B.LeastPx > 0.0)
					{
						// (held up far off and thinned to match, as that piece holds itself)
						const double LeastT = 0.5 * B.LeastPx * (Far - Eye.Eye).Length() / std::max(1.0, Eye.PixelsPerRadian);
						if (RadT < LeastT)
						{
							TailA *= std::max(B.LeastKeep, std::sqrt(std::max(0.0, RadT) / LeastT));
							RadT = LeastT;
						}
					}
					const Vec Run = P - Far;
					const double RunLong = Run.Length();
					if (RunLong > 1e-4)
					{
						auto Across = [&](const Vec& Way_, const Vec& Else)
						{
							// (square across the path on the screen; a path straight at the eye takes the other's)
							const Vec OnScreen = Way_ - Eye.Forward * Way_.Dot(Eye.Forward);
							return OnScreen.Length() > 1e-3 ? Eye.Forward.Cross(OnScreen).Unit(Else) : Else;
						};
						// (each edge lies square across the way the aircraft flew there, and the strip's line leaves each
						// edge along that way: the next piece does the same at the edge they share, so the strip bends
						// through its pieces as the path did, round a spin too, and never turns a corner at a join)
						const Vec RunWay = Run * (1.0 / RunLong);
						const Vec WayNear = B.Along.Length() > 0.5 ? B.Along : RunWay, WayFar = T.Way.Length() > 0.5 ? T.Way : RunWay;
						const Vec Side = Across(WayNear, Eye.Up);
						const Vec SideT = Across(WayFar, Side);
						LayBendFar = LayBendNear = Vec();
						if (WayNear.Dot(RunWay) > 0.5 && WayFar.Dot(RunWay) > 0.5)
						{
							LayBendFar = WayFar * RunLong - Run;
							LayBendNear = WayNear * RunLong - Run;
						}
						// (the lump it draws in to at its old end: as wide as it is, and a share of its length long)
						AxA = Eye.Forward.Cross(Side).Unit(Eye.Right) * -std::max(Rad, 0.4 * RunLong);
						AxB = Side * Rad;
						Middle = (P + Far) * 0.5 + (LayBendFar - LayBendNear) * 0.125;
						LayLong = Run * 0.5;
						LayFar = SideT * RadT;
						bLayLong = true;
						LayTailAlpha = TailA / A;
						LayLumpFar = LumpFar;
						if (B.EdgeSoft > 0.0)
						{
							LayWob = std::min(1.0, RunLong / (B.EdgeSoft * std::max({ Rad, RadT, 1e-3 })));
						}
						if (B.SmearHold >= 0.0 && F > B.SmearHold)
						{
							LayLump = Clamp01((F - B.SmearHold) / std::max(1.0 - B.SmearHold, 1e-6)); // (its old end: drawing in to a round lump)
						}
					}
				}
			}
			else if (B.Smear > 0.0 && B.Along.Length() > 0.5)
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
			else if (StretchNow > 1.0001 && B.Along.Length() > 0.5)
			{
				const Vec OnScreen = B.Along - Eye.Forward * B.Along.Dot(Eye.Forward);
				const double L = OnScreen.Length();
				if (L > 1e-3)
				{
					const Vec Ua = OnScreen * (1.0 / L);
					AxA = Ua * (Rad * (1.0 + (StretchNow - 1.0) * L)); // (shorter when it points at the eye)
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
		case Bit::Shape::Plate:
		{
			// (a torn piece of a falling aircraft, tumbling about its long way: seen edge-on it thins to a sliver, so it
			// flutters as it falls. Its painted face or the bare metal behind, lit by the sun, and a glint when the sun
			// catches it as a mirror would toward the eye)
			if (B.bThrown)
			{
				// (a ground gun's spent case: laid at the end, below, as a lit tube with its mouth and its shadow)
				Floor = Rgb();
				Through = Rgb();
				Lift = Rgb();
				break;
			}
			const double Th = B.Tumble * Age;
			const Vec Lv = B.Face * std::cos(Th) + B.Along.Cross(B.Face) * std::sin(Th);
			const Vec N = B.Along.Cross(Lv);
			AxA = B.Along * (Rad * B.Stretch);
			AxB = Lv * Rad;
			const Vec ToEye = (Eye.Eye - P).Unit(-Eye.Forward);
			const bool bFace = N.Dot(ToEye) >= 0.0;
			const Rgb Skin = bFace ? B.Col : B.Col2;
			const Vec Sun = Eye.Sun.Unit(ZUp);
			const double Diffuse = 0.4 + 0.6 * std::fabs(N.Dot(Sun));
			const double Spot = B.Glint * (bFace ? 0.3 : 1.0) * std::pow(std::fabs(N.Dot((Sun + ToEye).Unit(N))), GlintPow);
			Col = Rgb(static_cast<float>(Skin.R * Diffuse * Eye.SmokeLight.R + Spot * Eye.SunLight.R),
				static_cast<float>(Skin.G * Diffuse * Eye.SmokeLight.G + Spot * Eye.SunLight.G),
				static_cast<float>(Skin.B * Diffuse * Eye.SmokeLight.B + Spot * Eye.SunLight.B));
			Floor = Rgb();
			Through = Rgb();
			Lift = Rgb();
			break;
		}
		case Bit::Shape::Streak:
		{
			// (a tracer keeps its own heading, but an air gun's, slowing and falling over a long flight, bends with it)
			const Vec Way = B.HalfLength > 0.0 ? (B.Gravity > 0.0 ? Going(B, Age).Unit(B.Along) : B.Along) : Going(B, Age);
			const double Speed = Way.Length();
			// (how far the middle is in front of the eye. A tracer's heading on the screen is taken by the picture's own
			// perspective from there: taken square to the eye's forward line, one aimed close by the eye off the middle
			// of the view drew 60 px long and then 11 as it turned. The pre-check, 01:57Z)
			const double Deep = (P - Eye.Eye).Dot(Eye.Forward);
			const Vec OnScreen = B.HalfLength > 0.0 && Deep > 1e-3 ? Way - (P - Eye.Eye) * (Way.Dot(Eye.Forward) / Deep) : Way - Eye.Forward * Way.Dot(Eye.Forward);
			const double L = OnScreen.Length();
			const Vec Ua = L > 1e-6 ? OnScreen * (1.0 / L) : Eye.Right;
			const Vec Ub = Eye.Forward.Cross(Ua).Unit(Eye.Up);
			// (never more than its whole length: as the middle crosses the eye's plane far off to the side, the heading on
			// the screen grows without end, and a tracer hundreds of metres away drew a line across the screen for a frame.
			// The pre-check, 02:15Z)
			const double Share = Speed > 1e-9 ? std::min(L / Speed, 1.0) : 0.0;
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
			const double Ppr = std::max(1.0, Eye.PixelsPerRadian);
			if (B.MostPx > 0.0)
			{
				// (art director 01:31Z: a near streak stays thin, no wider or brighter than a far one. A tracer's glow is a
				// few centimetres across, so passing a few metres off it drew 20 px wide or more, a beam; it is held here.
				// Metres a pixel by how far in front of the eye, not how far off: off the middle of the screen the picture
				// draws things wider)
				const double PerPixel = Deep > 1e-3 ? std::min(MetresAPixel, Deep / Ppr) : MetresAPixel;
				Width = std::min(Width, std::max(B.MostPx, B.LeastPx) * PerPixel * 0.5);
			}
			double Wide = 1.0, DayRamp = 0.0;
			if (B.Glow > 0)
			{
				// (by day a tracer widens and its core whitens, so a stream holds on a pale sky; by dusk it is as ruled.
				// Day is read from the hour's smoke light: white by day, dim at dusk)
				const double Day = DayOf(Eye.SmokeLight);
				Wide = 1.0 + (TracerDayWide - 1.0) * Day;
				if (TracerDayPx > 0.0 && Width > 1e-9)
				{
					// (by day its width is its own, in pixels where it is: the warm edge's whole width, or the core's)
					// (laid half a pixel wider each side, solid to half a pixel inside its width and fading out over that
					// one pixel: each pixel takes the share of it the line covers, so it is one straight line at any slope)
					const double PerPixel = Deep > 1e-3 ? std::min(MetresAPixel, Deep / Ppr) : MetresAPixel;
					const double Px = std::max(1.0, B.Glow == 2 ? TracerDayCorePx : TracerDayPx);
					const double ByDay = 0.5 * (Px + 1.0) * PerPixel;
					Wide = 1.0 + (ByDay / Width - 1.0) * Day;
					// (and solid: its whole opacity all along, its heart full)
					A += (std::min(1.0, A / std::max(B.Alpha, 1e-6)) - A) * Day;
					HeartUp = Day;
					LaySquare = 0.8 * Day; // (and as wide from end to end: straight-sided, never a long thin lens)
					RimNow = B.Rim * (1.0 - Day);
					InnerNow = B.Inner + ((Px - 1.0) / (Px + 1.0) - B.Inner) * Day;
					DayRamp = PerPixel * Day; // (the same pixel's fade at its ends)
					if (B.Glow == 1)
					{
						Col = Col.Mix(TracerDayEdge, Day);
					}
					else
					{
						PutK = (1.0 + (std::min(TracerDayOver, LitGain) - 1.0) * Day) / LitGain; // (the core white-hot by day)
					}
				}
				Width *= Wide;
				if (B.Glow == 2)
				{
					Col = Col.Mix(TracerDayCore, Day);
				}
			}
			Half = std::max(Half, Width);
			AxA = Ua * Half;
			AxB = Ub * Width;
			if (DayRamp > 0.0)
			{
				LayInnerAlong = std::clamp(1.0 - DayRamp / std::max(B.HalfLength > 0.0 ? B.HalfLength : Half, 1e-6), InnerNow, 0.999);
			}
			if (B.HalfLength > 0.0 && Speed > 1e-9)
			{
				// (a tracer is laid along its true heading in the world, turned about it to face the eye, so the picture's
				// own perspective puts its ends where they truly are. Laid flat across the eye's view at its middle, a long
				// near tracer off the middle of the screen and running toward or away from the eye drew off its own line,
				// by up to 150 px: the 3D builder's machine gun on hills, build 1708. Pointing almost at the eye it is laid
				// as before)
				const Vec Heading = Way * (1.0 / Speed);
				const Vec ToEye = Eye.Eye - P;
				const Vec Across = Heading.Cross(ToEye);
				const double Far = ToEye.Length();
				if (Far > 1e-6 && Across.Length() > 0.05 * Far)
				{
					const Vec Bu = Across.Unit(Ub);
					// (nothing is drawn behind where it was fired from: art director 02:13Z, build 1710, the machine gun's line
					// ran on past the muzzle, its thinning tail reaching 7 m behind the gun. A close shot's whole path is never
					// drawn past its strike either; its pieces still overlap where they meet, so the line is unbroken. Cut
					// short, the streak keeps its shape: its point ahead, its thinning tail behind, fitted between)
					double Long = std::max(B.HalfLength, Width);
					const double Behind = Long * (1.0 + B.Tail), Ahead = Long * (1.0 + B.Point);
					const double Gone = (P - B.Origin).Dot(Heading); // (how far its middle is along from where it was fired)
					const double Back = std::max(0.0, Gone);
					const double Front = B.Until > 0.0 ? std::min(Ahead, std::max(0.0, B.Until - Gone)) : Ahead;
					if (B.bFired && (Behind > Back || Ahead > Front))
					{
						// (an end cut short is blunt, so the line runs at its width right up to the muzzle flash or the strike,
						// with no faint gap before it)
						if (Behind > Back)
						{
							TailNow = std::min(TailNow, 0.1);
						}
						if (Ahead > Front)
						{
							PointNow = std::min(PointNow, 0.1);
						}
						const double From_ = -std::min(Behind, Back);
						Long = (Front - From_) / (2.0 + PointNow + TailNow);
						Middle = P + Heading * (Front - Long * (1.0 + PointNow));
					}
					AxA = Heading * Long;
					AxB = Bu * Width;
					// (and as wide on the screen from end to end as at its middle: laid as wide in metres all along, the end
					// nearer the eye drew wider, up to four times, a wedge. Its width in metres follows how far each point is
					// in front of the eye, which runs straight along it. Held, the width stays in step with the depth, so the
					// taper is never cut short: a round passing under half a metre off drew a wedge 50 px across for a frame.
					// A point behind the eye takes a mirrored width, and the picture cuts it away)
					const double Run = AxA.Dot(Eye.Forward);
					const double DeepM = (Middle - Eye.Eye).Dot(Eye.Forward);
					if (B.MostPx > 0.0 && Deep > 1e-3)
					{
						// (as wide in pixels all along as the width was set for at P)
						AxB = Bu * (Width * DeepM / Deep);
						TaperB = Bu * (Width * Run / Deep);
					}
					else if (B.MostPx > 0.0)
					{
						// (its middle behind the eye with an end still in view: as wide in pixels as the limit, all along)
						const double K = std::max(B.MostPx, B.LeastPx) * 0.5 / Ppr * Wide;
						AxB = Bu * (K * DeepM);
						TaperB = Bu * (K * Run);
					}
					else if (DeepM > 1e-3)
					{
						TaperB = Bu * (Width * std::clamp(Run / DeepM, -0.9, 0.9));
					}
				}
			}
			break;
		}
		}
		if (A < 0.004)
		{
			return 0;
		}
		const double HeartOwn = B.HeartEnd >= 0.0 ? B.Heart + (B.HeartEnd - B.Heart) * F : B.Heart;
		const double Heart = HeartOwn + (1.0 - HeartOwn) * HeartUp;
		const double Turned = B.Turn + B.TurnRate * Age;
		if (B.Form == Bit::Shape::Plate && B.bThrown)
		{
			return LayCase(Slot, B, Age, P, A, Eye, Room);
		}
		if (B.TwinPx > 0.0 && Room >= 2 && 2.0 * Rad >= B.TwinPx * MetresAPixel)
		{
			// (art director 00:03Z: a near flak puff is two fans, the second turned half a step round the same outline, so the
			// two together have twice the sides and no straight facet shows. Each takes the opacity that makes the two
			// together what one would be)
			const double Step = Pi / Sides;
			const Vec AxA2 = AxA * std::cos(Step) + AxB * std::sin(Step);
			const Vec AxB2 = AxB * std::cos(Step) - AxA * std::sin(Step);
			auto Half = [](double X) { return 1.0 - std::sqrt(1.0 - Clamp01(X)); };
			Lay(Slot, Middle, AxA, AxB, Col, Half(A), Half(A * Heart), Half(A * B.Rim), B.Inner, B.Point, B.Tail, B.bLight ? 0.0 : B.ShadeK, Eye, B.Rough, B.LumpSeed, Turned, Floor, Through, Outward, Lift);
			Lay(Slot + 1, Middle, AxA2, AxB2, Col, Half(A), Half(A * Heart), Half(A * B.Rim), B.Inner, B.Point, B.Tail, B.bLight ? 0.0 : B.ShadeK, Eye, B.Rough, B.LumpSeed, Turned - Step, Floor, Through, Outward, Lift);
			B.bShown = true;
			return 2;
		}
		Lay(Slot, Middle, AxA, AxB, Col, A, A * Heart, A * RimNow, InnerNow, PointNow, TailNow, B.bLight ? 0.0 : B.ShadeK, Eye, B.Rough, B.LumpSeed, Turned, Floor, Through, Outward, Lift, TaperB);
		B.bShown = true;
		return 1;
	}

	bool Battle::SpanAt(int Id, double Z, double& Mid, double& Half) const
	{
		// (between the two nearest bins' middles, each bin's span drawn from the bins either side too, so the column's
		// edge runs smooth up its height and a lone puff in a bin never narrows it)
		if (Id < 0)
		{
			return false;
		}
		const double U = Z / ColumnBin - 0.5;
		const int B0 = static_cast<int>(std::floor(U));
		const double T = U - B0;
		double SumMid = 0.0, SumHalf = 0.0, SumW = 0.0;
		auto It = std::lower_bound(Spans.begin(), Spans.end(), std::make_pair(Id, B0 - 1), [](const ColumnSpan& C, const std::pair<int, int>& K) { return C.Id != K.first ? C.Id < K.first : C.Bin < K.second; });
		for (; It != Spans.end() && It->Id == Id && It->Bin <= B0 + 2; ++It)
		{
			const ColumnSpan& C = *It;
			const double W = C.Bin == B0 ? 1.0 - T : C.Bin == B0 + 1 ? T : (C.Bin == B0 - 1 ? 0.5 * (1.0 - T) : 0.5 * T);
			if (W <= 0.0)
			{
				continue;
			}
			SumMid += W * 0.5 * (C.Lo + C.Hi);
			SumHalf += W * 0.5 * (C.Hi - C.Lo);
			SumW += W;
		}
		if (SumW <= 1e-9)
		{
			return false;
		}
		Mid = SumMid / SumW;
		Half = SumHalf / SumW;
		return true;
	}

	int Battle::Draw(double Now, const View& Eye)
	{
		LastEye = Eye.Eye;
		bEyeKnown = true;
		// (how far the eye came and turned since the last frame: see EyeMoved)
		EyeMoved = bEyeWas ? (Eye.Eye - EyeWas).Length() : 0.0;
		EyeTurned = bEyeWas ? std::max(std::acos(std::clamp(Eye.Forward.Dot(AheadWas), -1.0, 1.0)), std::acos(std::clamp(Eye.Up.Dot(UpWas), -1.0, 1.0))) : 0.0;
		EyeWas = Eye.Eye;
		AheadWas = Eye.Forward;
		UpWas = Eye.Up;
		bEyeWas = true;
		Run(Now);
		Prune(Now);
		++FrameNo;
		// (art director 06:48Z: on the overview, or any eye above about 25 m looking steeply down, the dust within about
		// 10 m of each vehicle or gun thins so each shows at least half; the long trail behind stays, a column's tell. A
		// ground or follow eye keeps the full veil)
		{
			auto Smooth = [](double X, const Span& R) { const double T = Clamp01((X - R.A) / std::max(R.B - R.A, 1e-6)); return T * T * (3.0 - 2.0 * T); };
			SteepThin = Smooth(Eye.Eye.Z - Height(Eye.Eye.X, Eye.Eye.Y), ThinAbove) * Smooth(-Eye.Forward.Z, ThinSteep);
			NearSockets.clear();
			if (SteepThin > 0.0)
			{
				for (const Dusting& Du : Dusts)
				{
					if (Now - Du.Last < 1.0)
					{
						NearSockets.push_back(Du.At);
					}
				}
			}
		}
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
			Slot += LayBit(Slot, M, Now, Eye, MaxPuffs - Slot);
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
			if (Row.Core > 0.0 && Tr.Prev.bOn)
			{
				// (a ribbon: a bar too, back to the newest piece where it now is)
				B.Ribbon = Row.Core;
				B.Back = Tr.Prev;
				B.Drag = 2.0;
				B.Windage = 1.0;
			}
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
		Spans.clear();
		TwinBits = 0;
		DustInSight = 0;
		SeenTwin = 0;
		CaseOff = 0;
		size_t OffSight = 0;
		const Vec RightLevel = Vec(Eye.Right.X, Eye.Right.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		for (const Bit& B : Bits)
		{
			if (Now < B.Born || (Now > B.Born + B.Life && !Unseen(B)))
			{
				continue;
			}
			const Vec At = Where(B, Now - B.Born);
			if ((At - Eye.Eye).Length() > (B.bKeep ? KeepBeyond : Beyond))
			{
				// (what LayBit will not lay for distance never takes a place: counted first, a far light held a slot it
				// never filled, and the barrage in front of the eye was cut for it. The pre-check, 05:33Z)
				continue;
			}
			const bool bCase = B.bThrown && B.Form == Bit::Shape::Plate;
			if (bCase && (At - Eye.Eye).Length() > CaseFar)
			{
				// (a spent case past its own distance is neither counted nor laid: with the eye gone back to the overview,
				// minutes of lying cases held places as specks. The pre-check, 07:20Z)
				continue;
			}
			if (bCase && CaseHoldPx > 0.0)
			{
				// (nor one LayCase lays nothing of for its size: under hold_px across and no wider than gone_px)
				const double Px = 2.0 * B.R0 * std::max(Eye.PixelsPerRadian, 1.0) / std::max((At - Eye.Eye).Length(), 1e-3);
				if (Px < CaseHoldPx && Px <= CaseGonePx)
				{
					continue;
				}
			}
			// (wholly off the screen: counted in the room asked, as ever, so nothing is made that the whole field has no
			// room for; but neither laid nor given a place, so what is in front of the eye is never cut for it)
			const bool bOff = OffScreen(B, At, Eye);
			if (bOff && Now > B.Born + B.Life)
			{
				// (a light off the screen when its life is over is done. Never laid, it stayed unseen and held its place in
				// the room asked for its half second: every round's flame, with guns firing just off the screen, and the
				// gun smoke's share shut sooner. The pre-check, 15:02Z)
				B.bShown = true;
				continue;
			}
			const size_t Twins = (B.TwinPx > 0.0 ? 1u : 0u) + (bCase ? 2u : 0u); // (a far twin is not counted either; a spent case's shadow and mouth)
			TwinBits += Twins;
			DustInSight += B.Thin == 1 ? 1u : 0u;
			OffSight += bOff ? 1u : 0u;
			SeenTwin += bOff ? 0u : Twins;
			CaseOff += bOff && bCase ? 1u + Twins : 0u;
			// (counted first when the room is short: an oil fire's kept column, and every light: flames, flashes and
			// tracers are few and are what an air fight is read by from far off, so its smoke is what goes. The
			// pre-check, 05:14Z: by depth alone the far side of a fight went bare, flames and all)
			if (!bOff)
			{
				Sorted.push_back(Order{ &B, (At - Eye.Eye).Dot(Eye.Forward) - B.Nearer, B.bKeep || B.bLight });
			}
			if (B.OilId >= 0)
			{
				// (an oil column's puff, not yet faded far: how far across the screen the column reaches at its height)
				const double F = Clamp01((Now - B.Born) / std::max(B.Life, 1e-6));
				if (F < B.Hold + 0.5 * (1.0 - B.Hold))
				{
					const double Rad = B.R0 + (B.R1 - B.R0) * std::pow(F, B.Grow);
					const double X = At.X * RightLevel.X + At.Y * RightLevel.Y;
					Spans.push_back(ColumnSpan{ B.OilId, static_cast<int>(std::floor(At.Z / ColumnBin)), X - Rad, X + Rad });
				}
			}
		}
		if (!Spans.empty())
		{
			// (one span per fire and bin: sorted, then each run of the same fire and bin folded into its first)
			std::sort(Spans.begin(), Spans.end(), [](const ColumnSpan& L, const ColumnSpan& R) { return L.Id != R.Id ? L.Id < R.Id : L.Bin < R.Bin; });
			size_t Put = 0;
			for (size_t I = 1; I < Spans.size(); ++I)
			{
				if (Spans[I].Id == Spans[Put].Id && Spans[I].Bin == Spans[Put].Bin)
				{
					Spans[Put].Lo = std::min(Spans[Put].Lo, Spans[I].Lo);
					Spans[Put].Hi = std::max(Spans[Put].Hi, Spans[I].Hi);
				}
				else
				{
					Spans[++Put] = Spans[I];
				}
			}
			Spans.resize(Put + 1);
		}
		SeenBits = Sorted.size();
		InSight = SeenBits + OffSight;
		HeldAtDraw = Bits.size();
		DustSince = 0;
		for (const Bit& B : Roots)
		{
			Sorted.push_back(Order{ &B, (B.From - Eye.Eye).Dot(Eye.Forward), false });
		}
		std::sort(Sorted.begin(), Sorted.end(), [](const Order& L, const Order& R) { return L.Deep > R.Deep; });
		// (out of room: the farthest go. Counted from the nearest back, a twin taking two slots, so the nearest are never
		// the ones left out)
		const size_t Left = static_cast<size_t>(std::max(0, MaxPuffs - Slot));
		// (a twin counts two only if it may be big enough on the screen to be laid as two: its size now, at the most,
		// against its depth, which is never more than its distance, so the count never comes short and seldom wastes
		// a slot. One held up to its least size is never a twin, unless twin_px is under least_px)
		auto Takes = [&](const Order& O) -> size_t
		{
			const Bit& Piece = *O.B;
			if (Piece.bThrown && Piece.Form == Bit::Shape::Plate)
			{
				// (a spent case lays up to three fans: its shadow once it lies there, its tube, its mouth. Under detail_px
				// across, the tube alone. Counted as one, the room ran out before the nearest: the heap under the gun
				// blinked out. The pre-check, 07:35Z)
				if (!CaseDetailed(Piece.R0, O.Deep + Piece.Nearer, Eye))
				{
					return 1u;
				}
				return Now - Piece.Born >= Piece.Land2 && CaseShadow > 0.0 ? 3u : 2u;
			}
			bool bMayTwin = false;
			if (Piece.TwinPx > 0.0)
			{
				const double Grown = Clamp01((Now - Piece.Born) / std::max(Piece.Life, 1e-6));
				const double Most = std::max(Piece.R0, Piece.R0 + (Piece.R1 - Piece.R0) * std::pow(Grown, Piece.Grow));
				bMayTwin = Piece.TwinPx <= Piece.LeastPx || 2.0 * Most * std::max(Eye.PixelsPerRadian, 1.0) >= Piece.TwinPx * O.Deep;
			}
			return bMayTwin ? 2u : 1u;
		};
		// (what is kept, an oil fire's column or a light, is counted first, so a far column on the horizon never blinks
		// out and a far fight keeps its fires and flashes)
		size_t Kept = 0;
		for (const Order& O : Sorted)
		{
			Kept += O.bKeep ? Takes(O) : 0u;
		}
		Kept = std::min(Kept, Left);
		size_t Start = Sorted.size();
		for (size_t Need = Kept; Start > 0; --Start)
		{
			if (Sorted[Start - 1].bKeep)
			{
				continue;
			}
			const size_t Take = Takes(Sorted[Start - 1]);
			if (Need + Take > Left)
			{
				break;
			}
			Need += Take;
		}
		for (size_t I = 0; I < Sorted.size() && Slot < MaxPuffs; ++I)
		{
			if (I >= Start || Sorted[I].bKeep)
			{
				Slot += LayBit(Slot, *Sorted[I].B, Now, Eye, MaxPuffs - Slot);
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

	bool Battle::OffScreen(const Bit& B, const Vec& At, const View& Eye) const
	{
		if (Eye.HalfAcross <= 0.0)
		{
			return false;
		}
		// (a ball round it that holds all it can be laid as: its long way, its point and tail, its lumps, a trail's
		// smear, a tracer's length, a low skirt's height, a size held up in pixels; then a quarter more and half a
		// metre, and the view a tenth wider, for the eye's own shake)
		const Vec D = At - Eye.Eye;
		const double Z = D.Dot(Eye.Forward), X = std::fabs(D.Dot(Eye.Right)), Y = std::fabs(D.Dot(Eye.Up));
		const double Ends = 1.0 + std::max({ B.Point, B.Tail, 0.0 });
		const double Big = std::max(B.R0, B.R1);
		double R = Big * std::max(1.0, B.Stretch) * Ends * (1.0 + 1.2 * std::max(0.0, B.Rough));
		R += std::max(0.0, B.Smear) + std::max(0.0, B.HalfLength) * Ends + Big * std::max(0.0, B.Flatten);
		R += (std::max(B.LeastPx, B.MostPx) + 2.0) * D.Length() / std::max(Eye.PixelsPerRadian, 1.0);
		R = R * 1.25 + 0.5 + 2.0 * EyeMoved;
		double Ta = Eye.HalfAcross * 1.1, Tu = (Eye.HalfUp > 0.0 ? Eye.HalfUp : Eye.HalfAcross * 1.25) * 1.1;
		if (EyeTurned > 0.0 || !(EyeTurned == EyeTurned) || !(EyeMoved == EyeMoved))
		{
			// (the view that much wider each way; turned a long way, or not a number, and nothing is called off the screen)
			const double Turn = std::tan(std::min(2.0 * EyeTurned, 1.2));
			if (!(2.0 * EyeTurned < 1.2) || !(EyeMoved == EyeMoved) || Ta * Turn >= 0.9 || Tu * Turn >= 0.9)
			{
				return false;
			}
			Ta = (Ta + Turn) / (1.0 - Ta * Turn);
			Tu = (Tu + Turn) / (1.0 - Tu * Turn);
		}
		return Z < -R || X - Ta * Z > R * (1.0 + Ta) || Y - Tu * Z > R * (1.0 + Tu);
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
			if (S.Kind == 2 && Now >= S.Start && Now <= S.Flames && S.Oil.Lamp > 0.0)
			{
				// (burning oil lights the sea round it: weak by day, and the night's strongest lamp after a big flash)
				const double Seed = static_cast<double>(S.Seed % 1000ull);
				Lamp L;
				L.At = S.At + ZUp * (S.Oil.FlameUp + 4.0);
				L.Strength = ByDay * S.Oil.Lamp * std::sqrt(S.Across) * (0.8 + 0.2 * std::sin(Now * 11.0 + Seed) * std::sin(Now * 4.1 + Seed * 0.5));
				L.Col = S.Oil.Flame.Mix(DayLampCol, Day);
				L.Radius = S.Oil.LampReach * S.Across;
				All.push_back(L);
				continue;
			}
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
