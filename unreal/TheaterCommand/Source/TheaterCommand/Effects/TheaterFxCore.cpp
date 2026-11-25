#include "Effects/TheaterFxCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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

		// (SetSrgbBytes: the byte that a material raising it to 2.2 reads as V, from a table made once of 65,536 steps)
		uint8_t ByteGamma(double V)
		{
			static const std::vector<uint8_t> Table = []()
			{
				std::vector<uint8_t> Made(65536);
				for (size_t I = 0; I < Made.size(); ++I)
				{
					Made[I] = static_cast<uint8_t>(std::lround(std::pow(static_cast<double>(I) / 65535.0, 1.0 / 2.2) * 255.0));
				}
				return Made;
			}();
			if (!(V > 0.0))
			{
				return 0; // (and a colour that is no number is black, as Byte gives: the pre-check on 8b88db15)
			}
			return V >= 1.0 ? 255 : Table[static_cast<size_t>(std::lround(V * 65535.0))];
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

		/** Bit::CutAt: a trail's piece cut to end with one laid after it fades over the last Over seconds before. */
		double CutFade(double CutAt, double Now, double Over)
		{
			if (!(CutAt < 1.0e17))
			{
				return 1.0;
			}
			const double T = std::clamp((CutAt - Now) / std::max(Over, 1e-3), 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
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

		/** A place or a speed a caller gave: every part a number, and within a million kilometres. */
		// (Bit::WakeBy, WakeOn: a piece's half width as the aircraft's wake spreads it. To WakeR by WakeBy seconds, fast at
	// first; then, WakeOn over 0, on as that power of its age, as a smoke trail flares behind the tail)
	double WakeWide(double R0, double WakeR, double By, double On, double Age)
	{
		if (On > 0.0 && Age > By)
		{
			return WakeR * std::pow(std::min(Age / By, 1.0e6), std::min(On, 1.0));
		}
		return R0 + (WakeR - R0) * std::pow(std::clamp(Age / By, 0.0, 1.0), 0.7);
	}

	bool RealPlace(const Vec& P)
		{
			return std::fabs(P.X) <= 1e9 && std::fabs(P.Y) <= 1e9 && std::fabs(P.Z) <= 1e9;
		}

		/** A number from the recipe or a caller, held between Least and Most; not a number at all, the Usual one. */
		double SafeNum(double Is, double Least, double Most, double Usual)
		{
			return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual;
		}

		/** A lamp every number of which is a number, its strength and reach none under 0 (SetLampRule's lamps). */
		bool RealLamp(const Battle::Lamp& L)
		{
			return std::isfinite(L.At.X) && std::isfinite(L.At.Y) && std::isfinite(L.At.Z) && std::isfinite(L.Strength) && L.Strength >= 0.0 && std::isfinite(L.Radius) && L.Radius >= 0.0
				&& std::isfinite(L.Col.R) && std::isfinite(L.Col.G) && std::isfinite(L.Col.B);
		}

		/** Effects (R2), -TheaterLandGunFlash: a gun named "mg:star" is the row mg drawn with the look star; the row's
		 * name is kept in Base. A name with no colon, as ever (Look left empty). */
		const char* LookSplit(const char* Gun, std::string& Base, std::string& Look)
		{
			const char* Colon = Gun ? std::strchr(Gun, ':') : nullptr;
			if (!Colon)
			{
				return Gun;
			}
			Base.assign(Gun, static_cast<size_t>(Colon - Gun));
			Look.assign(Colon + 1);
			return Base.c_str();
		}

		/** A fire's flicker by the clock (Bit::FlickerSeed, fire.lamp): three waves between Lo and Hi a second, their rates
		 * and phases from the seed and Salt, weighted a half, three tenths and a fifth; from -1 to 1, smooth every frame. */
		double Flickering(uint32_t Seed, uint32_t Salt, double Now, double LoIn, double HiIn)
		{
			const uint64_t H = Mix64((static_cast<uint64_t>(Seed) << 20) ^ (static_cast<uint64_t>(Salt) * 0x9E3779B97F4A7C15ull));
			const double Lo = SafeNum(LoIn, 0.1, 30.0, 3.0), Hi = SafeNum(HiIn, Lo, 30.0, std::max(6.0, Lo));
			const double T = Now == Now ? std::fmod(Now, 3600.0) : 0.0;
			double Sum = 0.0;
			for (int K = 0; K < 3; ++K)
			{
				const double U1 = static_cast<double>((H >> (K * 20)) & 0x3FFull) / 1023.0;
				const double U2 = static_cast<double>((H >> (K * 20 + 10)) & 0x3FFull) / 1023.0;
				Sum += (K == 0 ? 0.5 : K == 1 ? 0.3 : 0.2) * std::sin(2.0 * Pi * ((Lo + (Hi - Lo) * U1) * T + U2));
			}
			return std::clamp(Sum, -1.0, 1.0);
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
		LingerKept.clear();
		LingerSpareFrom = SafeNum(Num("linger.spare_from", 0.5), 0.0, 4.0, 0.5);
		LingerSpareBy = SafeNum(Num("linger.spare_by", 0.9), LingerSpareFrom + 0.01, 8.0, std::max(0.9, LingerSpareFrom + 0.01));
		LingerSpareLeast = SafeNum(Num("linger.spare_least", 0.3), 0.05, 1.0, 0.3);
		SpareAtBurst = 1.0;
		LingerNo = 0;
		bSkyKnown = false;
		// (set up again for another battle, it is no sea battle's until that one calls UseSea)
		bSeaFar = false;
		SeaGonePx = SeaHitFullPx = 0.0;
		// (and its lamps are handed out as ever until that one asks: SetLampRule)
		Rules = LampRule();
		bLampRuled = false;
		// (nor a land battle's until it asks: SetLensFade, SetScars, SetLandGrade, SetSoftContact)
		LensGone = LensWhole = LensPlane = 0.0;
		bScars = true;
		bLandGrade = false;
		LieNearer = 0.0;
		bFromAboveAsked = false;
		LayNear = 1.0;
		// (the far form, far: how far its pieces are drawn, over what share of the draw range a bomb's near burst and its far
		// form cross, and whether the room keeps them first. Its own dice, from the kit's seed. No row: nothing far is laid)
		FarRows.clear();
		FarSeed = Mix64(Seeds ^ 0xFA4F0A11ull);
		FarBeyond = SafeNum(Num("far.draw_beyond_m", 20000.0), 1.0, 1.0e5, 20000.0);
		FarBand = SafeNum(Num("far.hand_over", 0.0), 0.0, 0.5, 0.0);
		bFarKept = Num("far.keep", 0.0) > 0.5;
		// (the pre-check, 12:57Z: turned into a whole number unchecked, a broken recipe's nan or inf here was a fault
		// and its 1e9 asked for hundreds of gigabytes. Held to 100 to 20,000; not a number: the 3,000 it ships with)
		const double PuffsAsked = Num("game.max_puffs", 3000.0);
		MaxPuffs = PuffsAsked == PuffsAsked ? static_cast<int>(std::clamp(PuffsAsked, 100.0, 20000.0)) : 3000;
		// (the lumps' cost held: see LumpsMostPuffs. No such keys: every lump laid, as before)
		const double LumpsMostAsked = Num("game.lumps_most", 0.0);
		LumpsMostPuffs = LumpsMostAsked == LumpsMostAsked ? static_cast<int>(std::clamp(LumpsMostAsked, 0.0, 4000.0)) : 0;
		LumpsBigA = LumpsBigB = 0.0;
		if (Has("game.lumps_px_most.1"))
		{
			const double BigA = Num("game.lumps_px_most.0", 0.0), BigB = Num("game.lumps_px_most.1", 0.0);
			LumpsBigB = BigB == BigB ? std::clamp(BigB, 0.0, 1.0e6) : 0.0;
			LumpsBigA = LumpsBigB > 0.0 ? (BigA == BigA ? std::clamp(BigA, 0.0, LumpsBigB * 0.999) : 0.5 * LumpsBigB) : 0.0;
		}
		const double LeastInkAsked = Num("game.lumps_least_ink", 0.0);
		LumpsLeastInk = LeastInkAsked == LeastInkAsked ? std::clamp(LeastInkAsked, 0.0, 0.5) : 0.0;
		// (the pre-check on 7d83869, 16:14Z: see LumpsSoft and LumpsBand. No such keys: as 7d83869)
		const double SoftAsked = Num("game.lumps_soft_s", 0.0), BandAsked = Num("game.lumps_band", 0.25);
		LumpsSoft = SoftAsked == SoftAsked ? std::clamp(SoftAsked, 0.0, 5.0) : 0.0;
		LumpsBand = BandAsked == BandAsked ? std::clamp(BandAsked, 0.02, 1.0) : 0.25;
		LumpsTight = 1.0;
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
		// (art director 04:17Z 22 November: an aircraft's trail the same, from farther off. Without the key, as before)
		TrailNearGone = TrailNearWhole = 0.0;
		if (Has("game.trail_near_eye_fade_m.0"))
		{
			TrailNearGone = SafeNum(Num("game.trail_near_eye_fade_m.0", 30.0), 0.0, 500.0, 30.0);
			TrailNearWhole = SafeNum(Num("game.trail_near_eye_fade_m.1", 60.0), TrailNearGone + 0.1, 1000.0, TrailNearGone + 30.0);
		}
		TrailLoomsWhole = TrailLoomsGone = 0.0;
		if (Has("game.trail_near_eye_looms.0"))
		{
			TrailLoomsWhole = SafeNum(Num("game.trail_near_eye_looms.0", 0.06), 0.001, 2.0, 0.06);
			TrailLoomsGone = SafeNum(Num("game.trail_near_eye_looms.1", 0.2), TrailLoomsWhole + 0.001, 4.0, TrailLoomsWhole + 0.14);
		}
		TrailLeastGone = SafeNum(Num("game.trail_near_eye_least_m.0", 8.0), 0.5, 500.0, 8.0);
		TrailLeastWhole = SafeNum(Num("game.trail_near_eye_least_m.1", 16.0), TrailLeastGone + 0.1, 1000.0, TrailLeastGone + 8.0);
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
		// (game.lens_cap, the art director 22:46Z 21 November, on clods and cards right in front of the lens: "clods
		// faded 4 to 8 m from the lens, no card over 15% of the frame, no flash or flame over 20%, never a blackout")
		CapThrownGone = SafeNum(Num("game.lens_cap.thrown_gone_m", 0.0), 0.0, 100.0, 0.0);
		CapThrownWhole = std::max(CapThrownGone, SafeNum(Num("game.lens_cap.thrown_whole_m", 0.0), 0.0, 200.0, 0.0));
		CapCard = SafeNum(Num("game.lens_cap.card_most", 0.0), 0.0, 10.0, 0.0);
		CapLight = SafeNum(Num("game.lens_cap.light_most", 0.0), 0.0, 10.0, 0.0);
		CapSoft = SafeNum(Num("game.lens_cap.soft", 0.3), 0.01, 1.0, 0.3);
		CapNear = SafeNum(Num("game.lens_cap.near_m", 10.0), 0.1, 1000.0, 10.0);
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
		// (the air guns' tracer look, art director 15:02Z and 15:38Z: only with air_guns.tracers.life_s in the recipe)
		bAirTracers = Has("air_guns.tracers.life_s");
		AirTracerLife = SafeNum(Num("air_guns.tracers.life_s", 1.2), 0.1, 10.0, 1.2);
		AirStreakS = SafeNum(Num("air_guns.tracers.streak_s", 1.0 / 30.0), 0.002, 0.2, 1.0 / 30.0);
		AirDimFrom = SafeNum(Num("air_guns.tracers.dim_from", 0.667), 0.0, 1.0, 0.667);
		AirDimTo = SafeNum(Num("air_guns.tracers.dim_to", 0.5), 0.0, 1.0, 0.5);
		AirOutS = SafeNum(Num("air_guns.tracers.out_s", 0.045), 0.0, 1.0, 0.045);
		AirTracerNear = SafeNum(Num("air_guns.tracers.always_within_m", 300.0), 0.0, 1e5, 300.0);
		AirBodyPx = SafeNum(Num("air_guns.tracers.body_px", 0.0), 0.0, 64.0, 0.0);
		AirCorePx = SafeNum(Num("air_guns.tracers.core_px", 0.0), 0.0, 64.0, 0.0);
		AirCoreInner = SafeNum(Num("air_guns.tracers.core_inner", 0.0), 0.0, 0.95, 0.0);
		AirWhiteWithin = SafeNum(Num("air_guns.tracers.white_within_m", 0.0), 0.0, 1e5, 0.0);
		AirWhiteBeyond = SafeNum(Num("air_guns.tracers.white_beyond_m", 0.0), 0.0, 1e5, 0.0);
		AirTracerMost = static_cast<int>(SafeNum(Num("air_guns.tracers.most", 1500.0), 0.0, 6000.0, 1500.0));
		AirOneFanBeyond = SafeNum(Num("air_guns.tracers.one_fan_beyond_m", 0.0), 0.0, 1.0e6, 0.0);
		// (the air scene's far forms, 11-22, for its trail room going from 24 to 60 at 150 aircraft: off by default, and
		// then nothing moves. game.air_far.trails_on: a trail past trails_from_m laid as about trails_puffs puffs, over
		// the last trails_band of that range by degrees. guns_on: a gun past guns_from_m fires no rounds, one dash in its
		// tracer's colour along the aim every guns_dash_s, guns_dash_flight_s of a round's flight long)
		FarTrailFromM = Num("game.air_far.trails_on", 0.0) >= 0.5 ? SafeNum(Num("game.air_far.trails_from_m", 1200.0), 1.0, 1.0e6, 1200.0) : 0.0;
		FarTrailBand = SafeNum(Num("game.air_far.trails_band", 0.15), 0.0, 0.9, 0.15);
		FarTrailPuffs = SafeNum(Num("game.air_far.trails_puffs", 40.0), 4.0, 1000.0, 40.0);
		FarTrailKeep = static_cast<int>(SafeNum(Num("game.air_far.trails_keep_one_in", 3.0), 2.0, 64.0, 3.0));
		FarTrailBillow = SafeNum(Num("game.air_far.trails_billow", 0.25), 0.0, 0.6, 0.25); // (the art director's mend of 18:44Z: see Bit::Billow)
		FarGunFromM = Num("game.air_far.guns_on", 0.0) >= 0.5 ? SafeNum(Num("game.air_far.guns_from_m", 1500.0), 1.0, 1.0e6, 1500.0) : 0.0;
		FarGunDashS = SafeNum(Num("game.air_far.guns_dash_s", 0.2), 0.03, 2.0, 0.2);
		FarGunFlightS = SafeNum(Num("game.air_far.guns_dash_flight_s", 0.12), 0.01, 2.0, 0.12);
		AirGlowSoft = SafeNum(Num("air_guns.tracers.glow_soft", 0.0), 0.0, 1.0, 0.0);
		AirFarPx = SafeNum(Num("air_guns.tracers.one_fan_px", 0.0), 0.0, 64.0, 0.0);
		TracerHues.clear();
		AirTracerEnds.clear();
		HoseMost = static_cast<int>(SafeNum(Num("air_guns.tracers.hose_most", 600.0), 0.0, 6000.0, 600.0)); // (fx-air r2: a ship gun's hose lines at once)
		HoseEnds.clear();
		ThinAbove = Range("dust.vehicles.from_above.above_m", 15.0, 25.0);
		ThinSteep = Range("dust.vehicles.from_above.steep", 0.3, 0.5);
		ThinNear = Range("dust.vehicles.from_above.near_m", 6.0, 12.0);
		ThinKeep = Clamp01(Num("dust.vehicles.from_above.keep", 0.25));
		// (game.from_above, for a scene that asks, SetFromAbove: the art director's read of the general's stills, item 5)
		bAboveOn = Num("game.from_above.on", 0.0) >= 0.5;
		AboveEye = Range("game.from_above.eye_m", 100.0, 220.0);
		AboveSteep = Range("game.from_above.steep", 0.4, 0.7);
		AboveLit = SafeNum(Num("game.from_above.lit", 0.45), 0.0, 1.0, 0.45);
		AboveShadow = SafeNum(Num("game.from_above.shadow", 0.2), 0.0, 1.0, 0.2);
		AboveEvery = static_cast<int>(SafeNum(Num("game.from_above.shadow_every", 2.0), 1.0, 64.0, 2.0));
		AboveWide = SafeNum(Num("game.from_above.shadow_wide", 1.5), 0.2, 4.0, 1.5);
		AboveLeastM = SafeNum(Num("game.from_above.shadow_least_m", 3.0), 0.0, 1000.0, 3.0);
		AboveLeastR = SafeNum(Num("game.from_above.shadow_least_r_m", 1.5), 0.0, 1000.0, 1.5);
		AboveStretch = SafeNum(Num("game.from_above.shadow_stretch_most", 2.5), 1.0, 8.0, 2.5);
		AboveMost = static_cast<int>(SafeNum(Num("game.from_above.shadow_most", 300.0), 0.0, 6000.0, 300.0));
		AboveDarkLit = SafeNum(Num("game.from_above.dark_lit", 0.45), 0.0, 1.0, 0.45);
		AboveDarkLum = Range("game.from_above.dark_lum", 0.015, 0.04);
		AboveDarkest = SafeNum(Num("game.from_above.shadow_darkest", 0.55), 0.0, 1.0, 0.55);
		AboveSoftM = SafeNum(Num("game.from_above.shadow_soft_m", 60.0), 1.0, 10000.0, 60.0);
		DustRoom = std::max(0.05, std::min(1.0, Num("dust.vehicles.most_of_room", 0.4)));
		DustFullWithin = std::max(0.0, Num("dust.vehicles.full_within_m", 120.0));
		// (FX5, room shares by kind: off by default, and then nothing moves. On, every gate but the dust's reads RoomGate,
		// and the dust thins its far trails to fit what the rest leaves: ShareThin)
		ShareDustFloor = Num("game.room_shares.on", 0.0) >= 0.5 ? SafeNum(Num("game.room_shares.dust_floor", 0.08), 0.0, 0.4, 0.08) : -1.0;
		ShareDustTop = SafeNum(Num("game.room_shares.dust_top", 0.92), 0.3, 1.0, 0.92);
		ShareDustMemory = SafeNum(Num("game.room_shares.dust_memory_s", 4.0), 0.1, 30.0, 4.0);
		ShareDustFullM = SafeNum(Num("game.room_shares.dust_full_m", 300.0), 0.0, 1.0e5, 300.0);
		ShareDustMostThin = SafeNum(Num("game.room_shares.dust_most_thin", 8.0), 1.0, 32.0, 8.0);
		ShareDustThinS = SafeNum(Num("game.room_shares.dust_thin_s", 2.0), 0.1, 30.0, 2.0);
		ShareDustLeastAlive = SafeNum(Num("game.room_shares.dust_least_alive", 2.0), 1.0, 20.0, 2.0);
		ShareDropHoldS = SafeNum(Num("game.room_shares.drop_hold_s", 2.0), 0.0, 30.0, 2.0);
		ShareDropFrom = SafeNum(Num("game.room_shares.drop_from", 1.0), 0.3, 2.0, 1.0);
		ShareDropBack = SafeNum(Num("game.room_shares.drop_back", 0.9), 0.1, ShareDropFrom, 0.9);
		CaseMouth = Picture("ground_cases.mouth", Rgb::FromPicture(46, 34, 18));
		CaseGlint = Picture("ground_cases.glint_colour", Rgb::FromPicture(255, 220, 140));
		CaseShadow = Clamp01(Num("ground_cases.contact_shadow", 0.5));
		CaseShine = std::clamp(Num("ground_cases.shine", 8.0), 1.0, 64.0);
		MostLying = static_cast<size_t>(std::clamp(Num("ground_cases.most_lying", 200.0), 0.0, 5000.0));
		CaseFar = std::max(1.0, Num("ground_cases.draw_within_m", 150.0));
		TrailCutFade = SafeNum(Num("game.trail_cut_fade_s", 3.0), 0.05, 60.0, 3.0); // (an aircraft trail's piece cut to end with a later one: Bit::CutAt)
		// (burst.far, FX3: see Shell. The row is on with its full_px; every number held to what it can mean)
		bLandFar = Has("burst.far.full_px");
		LandFullPx = bLandFar ? SafeNum(Num("burst.far.full_px", 0.0), 0.0, 1.0e4, 0.0) : 0.0;
		LandLeast = SafeNum(Num("burst.far.least", 0.1), 0.02, 1.0, 0.1);
		LandHalvePast = SafeNum(Num("burst.far.halve_past", 1800.0), 0.0, 1.0e6, 1800.0);
		LandNearBusy = SafeNum(Num("burst.far.near_busy", 0.5), 0.05, 1.0, 0.5);
		LandOverBusy = SafeNum(Num("burst.far.over_busy", 0.125), 0.02, 1.0, 0.125);
		LandGonePx = bLandFar ? SafeNum(Num("burst.far.gone_px", 0.0), 0.0, 8.0, 0.0) : 0.0;
		LandGoneBeyond = SafeNum(Num("burst.far.gone_beyond_m", 300.0), 0.0, 1.0e5, 300.0);
		bLandGoneDirt = Num("burst.far.gone_dirt", 0.0) > 0.5;
		LandSmokePow = SafeNum(Num("burst.far.smoke_pow", 0.5), 0.0, 1.0, 0.5);
		LandSkirtPow = SafeNum(Num("burst.far.skirt_pow", 0.5), 0.0, 1.0, 0.5);
		LandAlphaPow = bLandFar ? SafeNum(Num("burst.far.alpha_pow", 0.0), 0.0, 2.0, 0.0) : 0.0;
		LandAlphaMost = SafeNum(Num("burst.far.alpha_most", 0.9), 0.0, 1.0, 0.9);
		FewAlphaPow = 0.0;
		LandHits = LandHitsFew = 0;
		LandFewSum = 0.0;
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
		// (FX7, game.six_sided: see SixUnderPx. The six points: the first, the one half way round, and four between, each
		// the ring's nearest to a sixth of the way round; each point of the ring goes with the nearest of them)
		SixUnderPx = Num("game.six_sided.on", 0.0) >= 0.5 ? SafeNum(Num("game.six_sided.under_px", 8.0), 0.0, 64.0, 8.0) : 0.0;
		for (int J = 0; J < 6; ++J)
		{
			SixSlot[J] = J == 3 ? Sides / 2 : static_cast<int>(std::lround(J * Sides / 6.0)) % Sides;
		}
		SixOf.assign(static_cast<size_t>(Sides), 0);
		for (int K = 0; K < Sides; ++K)
		{
			int Best = 0, BestGap = Sides;
			for (int J = 0; J < 6; ++J)
			{
				const int Gap = std::min((K - SixSlot[J] + Sides) % Sides, (SixSlot[J] - K + Sides) % Sides);
				if (Gap < BestGap)
				{
					Best = J;
					BestGap = Gap;
				}
			}
			SixOf[static_cast<size_t>(K)] = Best;
		}
		Out.assign(static_cast<size_t>(MaxPuffs) * PointsPerPuff, Vec());
		OutRgba.assign(static_cast<size_t>(MaxPuffs) * PointsPerPuff * 4, 0);
		ThrownLong = SafeNum(Num("burst.debris.clods.most_long", 0.0), 0.0, 100.0, 0.0);
		ThrownLong = ThrownLong > 0.0 ? std::max(1.0, ThrownLong) : 0.0; // (never shorter than it is wide: the pre-check on d487b717)
		LieNearerK = SafeNum(Num("game.soft_contact.lie_nearer", 1.5), 1.0, 10.0, 1.5);
		// (FX9, game.smoke_budget: see SkipPxLo. on 0, or no row: none of it, as before; on 1 and thin 0: the skip alone)
		SkipPxLo = SkipPxHi = ThinScreens = 0.0;
		if (Num("game.smoke_budget.on", 0.0) >= 0.5)
		{
			if (Has("game.smoke_budget.skip_px.0"))
			{
				SkipPxLo = SafeNum(Num("game.smoke_budget.skip_px.0", 2.0), 0.0, 64.0, 2.0);
				SkipPxHi = SafeNum(Num("game.smoke_budget.skip_px.1", 1.5 * SkipPxLo), SkipPxLo + 0.01, 128.0, 1.5 * SkipPxLo + 0.01);
			}
			// (the thinning has its own switch, thin: the art director refused it at 17:43Z, holes and lumps in the near veil
			// where one smooth body is the film look; the skip alone is invisible)
			ThinScreens = Num("game.smoke_budget.thin", 0.0) >= 0.5 && Has("game.smoke_budget.screens") ? SafeNum(Num("game.smoke_budget.screens", 2.0), 0.0, 100.0, 0.0) : 0.0;
			ThinFromPx = SafeNum(Num("game.smoke_budget.thin_from_px", 150.0), 8.0, 1.0e5, 150.0);
			ThinMost = SafeNum(Num("game.smoke_budget.thin_most", 0.5), 0.0, 0.9, 0.5);
			ThinS = SafeNum(Num("game.smoke_budget.thin_s", 1.0), 0.0, 30.0, 1.0);
			ThinBand = SafeNum(Num("game.smoke_budget.thin_band", 0.15), 0.02, 0.5, 0.15);
		}
		ReadGradeLooks();
		Clear();
	}

	void Battle::SetLandGrade(bool bOn)
	{
		if (bLandGrade != bOn)
		{
			bLandGrade = bOn;
			ReadGradeLooks();
		}
	}

	void Battle::SetSoftContact(double Metres)
	{
		LieNearer = Metres > 0.0 && Metres < 100.0 ? Metres * LieNearerK : 0.0;
	}

	void Battle::SetFromAbove(bool bOn)
	{
		bFromAboveAsked = bOn;
	}

	double Battle::GradeNum(const std::string& Path, double Fallback) const
	{
		if (bLandGrade && Has("land_grade." + Path))
		{
			return Num("land_grade." + Path, Num(Path, Fallback));
		}
		return Num(Path, Fallback);
	}

	Battle::Span Battle::GradeRange(const std::string& Path, double Low, double High) const
	{
		return Range(bLandGrade && Has("land_grade." + Path + ".0") ? "land_grade." + Path : Path, Low, High);
	}

	void Battle::ReadGradeLooks()
	{
		// (Picture reads land_grade.<path> itself while bLandGrade is set; the paints are read at each burst, so only
		// what Setup keeps is read again here: all of it, so a land_grade entry for any of them works when the grade is
		// set after Begin. The pre-check on d487b717)
		ClodShade = GradeNum("burst.debris.clods.shade", 2.5);
		ClodTint = GradeRange("burst.debris.clods.tint", 0.8, 1.2);
		TracerCol = Picture("tracer.colour", Rgb::FromPicture(213, 199, 104));
		TracerPale = TracerCol.Mix(Rgb(1.0f, 1.0f, 1.0f), 0.55);
		TracerDayCore = Picture("tracer.day_core", Rgb::FromPicture(245, 240, 210));
		TracerDayEdge = Picture("tracer.day_edge", Rgb::FromPicture(235, 180, 70));
		SunThroughCol = Picture("game.sun_through_colour", Rgb::FromPicture(255, 205, 160));
		DayLampCol = Picture("game.day_lamp_colour", Rgb::FromPicture(255, 240, 215));
		CannonHeartCol = Picture("air_guns.flash_heart", Rgb::FromPicture(240, 220, 150));
		FlameCol = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		FlashCoreCol = Picture("burst.flash.core_colour", Rgb::FromPicture(236, 168, 72));
		GunSmokeCol = Picture("air_guns.smoke_colour", Rgb::FromPicture(168, 166, 160));
		CaseMouth = Picture("ground_cases.mouth", Rgb::FromPicture(46, 34, 18));
		CaseGlint = Picture("ground_cases.glint_colour", Rgb::FromPicture(255, 220, 140));
		AboveLitCol = Picture("game.from_above.lit_colour", Rgb::FromPicture(176, 170, 160));
		AboveDarkCol = Picture("game.from_above.dark_lit_colour", Rgb::FromPicture(120, 108, 95));
		AboveShadowCol = Picture("game.from_above.shadow_colour", Rgb::FromPicture(30, 34, 42));
	}

	void Battle::Clear()
	{
		Bits.clear();
		Marks.clear();
		Flashes.clear();
		Knocks.clear();
		Sources.clear();
		ShotEnds.clear();
		GlanceAt = -1.0e18;
		GlanceSame = 0;
		AirTracerEnds.clear();
		HoseEnds.clear();
		GunRounds.clear();
		Firings.clear();
		Trails.clear();
		Falls.clear();
		Dusts.clear();
		FlamesDue.clear();
		SeaScreens.clear();
		Sticks.clear();
		InSight = HeldAtDraw = TwinBits = 0;
		SeenBits = SeenTwin = 0;
		Serials = 0;
		Numbered = 0;
		TrailCuts.clear();
		CaseOff = 0;
		bEyeWas = false;
		EyeMoved = EyeTurned = 0.0;
		DustInSight = DustSince = 0;
		CaseUntil.clear();
		BallLamps.clear();
		LampsKept.clear(); // (the pre-check, 22 November: kept lamps outlived a Clear, so a fire lit again with the clock set back
		LampsAt = -1.0e18; // had no lamp until the clock passed where it was)
		ThinShare = FillSmall = FillBig = FillLaid = SmokeLaidScreens = 0.0; // (FX9: no memory of another battle's smoke)
		SharePeak = 0.0; // (FX5: nor of its barrage's peak, nor its trails' thinning)
		ShareThin = 1.0;
		ShareThinAt = -1.0e9;
		ShareDrop = 0.0;
		std::fill(SharePeaks, SharePeaks + 8, 0.0);
		SharePeakSlot = std::numeric_limits<int64_t>::min();
		ThinAt = -1.0e9;
		for (size_t I = 0; I < OutRgba.size(); ++I)
		{
			OutRgba[I] = 0;
		}
		LastDrawn = 0;
		ShadowRoom = ShadowsWere = 0;
	}

	void Battle::SetLensFade(double Gone, double Whole, double Plane)
	{
		// (numbers that are no number, or wild, are none: the fade off, as made)
		const auto Sound = [](double V) { return V == V && V >= 0.0 && V <= 100.0; };
		if (!Sound(Gone) || !Sound(Whole) || !Sound(Plane) || Whole <= 0.0)
		{
			LensGone = LensWhole = LensPlane = 0.0;
			return;
		}
		LensGone = std::min(Gone, Whole);
		LensWhole = std::max(Whole, LensGone + 0.1);
		LensPlane = Plane;
	}

	void Battle::UseSea()
	{
		// A sea battle's gun actions are fought with the enemy line 5 to 15 km from the eye, where a metre is a tenth of
		// a pixel: drawn only as far as an air scene's 4 km the far line's flashes and columns were never seen, and
		// drawn to 16 km at their own size a gun's flash is under a pixel and a 40 m column one pixel wide. So the
		// sea's distance, and floors on the screen for the two things a far line is read by. Every floor is a number
		// of the sea row's; without that row nothing is held and the distance is the air's.
		SetDrawBeyond(SafeNum(SeaBeyond(), 1.0, 1.0e5, Beyond));
		bSeaFar = Has("sea.draw_beyond_m");
		SeaFlashPx = SafeNum(Num("sea.flash.least_px", 0.0), 0.0, 32.0, 0.0);
		SeaFlashKeep = SafeNum(Num("sea.flash.keep", 0.9), 0.0, 1.0, 0.9);
		SeaGlowPx = SafeNum(Num("sea.flash.glow_px", 0.0), 0.0, 64.0, 0.0);
		SeaGlowAlpha = SafeNum(Num("sea.flash.glow_alpha", 0.3), 0.0, 1.0, 0.3);
		SeaGlowS = SafeNum(Num("sea.flash.glow_s", 0.1), 0.0, 2.0, 0.1);
		SeaFlashHot = SafeNum(Num("sea.flash.hot", 0.0), 0.0, 1.0, 0.0);
		SeaFlashOver = SafeNum(Num("sea.flash.over", 1.0), 1.0, 4.0, 1.0);
		SeaGonePx = bSeaFar ? SafeNum(Num("sea.gone_px", 0.0), 0.0, 8.0, 0.0) : 0.0;
		SeaGoneBeyond = SafeNum(AirBeyond(), 1.0, 1.0e5, 4000.0);
		SeaHitFullPx = bSeaFar ? SafeNum(Num("sea.hit.full_px", 0.0), 0.0, 1.0e4, 0.0) : 0.0;
		SeaHitLeast = SafeNum(Num("sea.hit.least", 0.1), 0.02, 1.0, 0.1);
		SeaHitOver = SafeNum(Num("sea.hit.over_m", 2000.0), 1.0, 1.0e5, 2000.0);
		SeaFlashCol = Picture("sea.flash.colour", Rgb::FromPicture(255, 232, 150));
		SeaColumnPx = SafeNum(Num("sea.column.least_px", 0.0), 0.0, 32.0, 0.0);
		SeaColumnKeep = SafeNum(Num("sea.column.keep", 0.6), 0.0, 1.0, 0.6);
	}

	void Battle::SeaFlash(double Now, size_t FromBit)
	{
		// (UseSea: where the flame a gun or a hit has just laid, lit from within and over in a moment, would stand under
		// sea.flash.glow_px across as a whole, it is far. Then its pieces are never under least_px on the screen and
		// keep sea.flash.keep of their strength held there; they are turned toward sea.flash.colour, hot of the way at
		// the most, and burn over times white; and one soft piece of the flame's own colour is laid over them, held
		// at glow_px and glow_alpha thick, that outlives them by glow_s. So a far flash is a bright point with a
		// little glow, long enough on the screen to be caught. It comes in from glow_px down to half of it. A flash
		// that stands glow_px across or more is as it always was, to the byte)
		if (!bSeaFar || !(SeaFlashPx > 0.0) || FromBit >= Bits.size())
		{
			return;
		}
		const size_t Laid = Bits.size();
		const auto IsFlame = [Now](const Bit& B)
		{
			return B.Form == Bit::Shape::Puff && B.bLight && B.Born <= Now + 0.06 && B.Life <= 0.3;
		};
		Vec Mid;
		double Weigh = 0.0, Reach = 0.0, Ends = Now, Big = -1.0;
		Rgb Hue;
		for (size_t I = FromBit; I < Laid; ++I)
		{
			const Bit& B = Bits[I];
			const double R = std::max(B.R0, B.R1);
			if (!IsFlame(B) || !(R > 0.0) || !RealPlace(B.From))
			{
				continue;
			}
			Mid = Mid + B.From * R;
			Weigh += R;
			Reach = std::max(Reach, R * std::max(1.0, B.Stretch));
			Ends = std::max(Ends, B.Born + B.Life);
			if (R > Big)
			{
				Big = R;
				Hue = B.Col;
			}
		}
		if (!(Weigh > 0.0) || !(SeaGlowPx > 0.0) || !bEyeKnown)
		{
			return;
		}
		Mid = Mid * (1.0 / Weigh);
		const double SeenPx = 2.0 * Reach * SafeNum(LastPpr, 1.0, 1e7, 1000.0) / std::max(1.0, (LastEye - Mid).Length());
		const double Far = Clamp01(2.0 * (1.0 - SeenPx / SeaGlowPx)); // (0 where it stands glow_px across or more, 1 from half that down)
		if (!(Far > 0.0))
		{
			return;
		}
		for (size_t I = FromBit; I < Laid; ++I)
		{
			Bit& B = Bits[I];
			if (IsFlame(B))
			{
				B.LeastPx = std::max(B.LeastPx, SeaFlashPx);
				B.LeastKeep = std::max(B.LeastKeep, SeaFlashKeep);
				B.Col = B.Col.Mix(SeaFlashCol, SeaFlashHot * Far);
				B.Col2 = B.Col2.Mix(SeaFlashCol, 0.5 * SeaFlashHot * Far);
				B.Over = std::max(B.Over, 1.0 + (SeaFlashOver - 1.0) * Far);
			}
		}
		if (!(SeaGlowAlpha > 0.0))
		{
			return;
		}
		Bit G;
		G.Born = Now;
		G.From = Mid;
		G.Life = std::min(Ends - Now, 0.3) + SeaGlowS;
		G.R0 = G.R1 = 0.5 * Reach;
		G.Col = Hue;
		G.Col2 = Hue;
		G.Alpha = SeaGlowAlpha * Far;
		G.FadeIn = 0.003;
		G.Hold = 0.35;
		G.Heart = 0.35;
		G.Rim = 0.0;
		G.Inner = 0.45;
		G.bLight = true;
		G.LeastPx = SeaGlowPx;
		G.LeastKeep = 1.0;
		Bits.push_back(G);
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
		if (bLandGrade && Has("land_grade." + Path + ".0"))
		{
			// (SetLandGrade: the colour written for the land battle's grade, where the recipe has one)
			const std::string Own = "land_grade." + Path;
			return Rgb::FromPicture(Num(Own + ".0", 0.0), Num(Own + ".1", 0.0), Num(Own + ".2", 0.0));
		}
		if (!Has(Path + ".0"))
		{
			return Fallback;
		}
		return Rgb::FromPicture(Num(Path + ".0", 0.0), Num(Path + ".1", 0.0), Num(Path + ".2", 0.0));
	}

	bool Battle::GroundColour(const Vec& At, double& R255, double& G255, double& B255) const
	{
		double R = 0.0, G = 0.0, B = 0.0;
		if (!ColourAt || !ColourAt(ColourContext, At.X, At.Y, R, G, B) || !(R == R && G == G && B == B))
		{
			return false;
		}
		R255 = std::min(255.0, std::max(0.0, R));
		G255 = std::min(255.0, std::max(0.0, G));
		B255 = std::min(255.0, std::max(0.0, B));
		return true;
	}

	Battle::LandTable Battle::LandTableOf() const
	{
		// (picture.to_land: what to write, 0 to 255, for the game's screen to show 0, then each even step up to 255. The
		// game's film curve and grade turned round, from tools/battle-fx/game_curve.py table. Read once by whoever
		// needs several colours off it. No such list, or a row that is not a number: every colour as it is)
		LandTable T;
		while (T.Count < LandTable::Most && Has("picture.to_land." + std::to_string(T.Count)))
		{
			const double Is = Num("picture.to_land." + std::to_string(T.Count), 0.0);
			if (!(Is == Is))
			{
				T.Count = 0;
				break;
			}
			T.Write[T.Count++] = std::min(255.0, std::max(0.0, Is));
		}
		return T;
	}

	double Battle::LandTable::ToLand(double Screen255) const
	{
		if (!(Screen255 == Screen255))
		{
			return 0.0;
		}
		const double S = std::min(255.0, std::max(0.0, Screen255));
		if (Count < 2)
		{
			return S;
		}
		const double At = S / 255.0 * (Count - 1);
		const int Lo = std::min(Count - 2, static_cast<int>(At));
		return Write[Lo] + (Write[Lo + 1] - Write[Lo]) * (At - Lo);
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
		Out_.bBruise = Has(P + "bruise.0");
		if (Out_.bBruise)
		{
			Out_.Bruise = Picture(P + "bruise", Out_.Dust);
			Out_.BruiseCore = SafeNum(Num(P + "bruise_core", 0.4), 0.1, 1.0, 0.4);
			Out_.BruiseLobesLo = SafeNum(Num(P + "bruise_lobes.0", 4.0), 1.0, 12.0, 4.0);
			Out_.BruiseLobesHi = SafeNum(Num(P + "bruise_lobes.1", 6.0), Out_.BruiseLobesLo, 12.0, 6.0);
			Out_.BruiseChipsLo = SafeNum(Num(P + "bruise_chips_m.0", 3.0), 0.0, 30.0, 3.0);
			Out_.BruiseChipsHi = SafeNum(Num(P + "bruise_chips_m.1", 5.0), Out_.BruiseChipsLo, 30.0, 5.0);
		}
		return Out_;
	}

	void Battle::Squash(Vec& Dir, double& Reach) const
	{
		// (grounds.<ground>.crown_low and crown_wide: the crown's fan squashed, a jet's height by CrownLow and its spread by
		// CrownWide, its way turned and its reach changed to match, so a jet still ends where the squashed fan has it)
		const Vec S(Dir.X * CrownWide, Dir.Y * CrownWide, Dir.Z * CrownLow);
		const double L = S.Length();
		if (L > 1e-9)
		{
			Dir = S * (1.0 / L);
			Reach *= L;
		}
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
		// (a moving ship's flame or fragment, BurnMove: its own flight, and the ship's way it was born with)
		const Vec Own = WhereOwn(B, Age);
		if (B.InTail(Age))
		{
			// (smoke going into the air, Lingering: it climbs on, and the cloud grows as it thins, each puff standing
			// further from the cloud's middle, level with the ground. Here, so whoever asks where it is is told the same)
			Vec P = B.bCarried ? Own + B.Carry * std::min(Age, B.CarryFor) : Own;
			const double TailFrom = B.Hold * B.Life;
			const double TailOwn = Clamp01((Age - TailFrom) / std::max(B.Life - TailFrom, 1e-6));
			const double TailInto = B.TailGoBy > 0.0 ? TailOwn * std::min(B.TailGoBy, 1.0) : TailOwn;
			P.Z += std::clamp(B.TailLift, 0.0, 5.0) * std::min(Age - TailFrom, B.Life - TailFrom);
			const double TailSpreads = std::clamp(B.TailSpread, 0.0, 4.0) * TailInto;
			if (TailSpreads > 0.0)
			{
				const Vec TailAxis = B.TailMid + Wind * (B.TailMidWindage * Age);
				P.X += (P.X - TailAxis.X) * TailSpreads;
				P.Y += (P.Y - TailAxis.Y) * TailSpreads;
			}
			return P;
		}
		return B.bCarried ? Own + B.Carry * std::min(Age, B.CarryFor) : Own;
	}

	Vec Battle::WhereOwn(const Bit& B, double Age) const
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
		// (a moving ship's flame or fragment: its own going, and the ship's way while it still has it)
		const Vec Own = GoingOwn(B, Age);
		return B.bCarried && Age < B.CarryFor ? Own + B.Carry : Own;
	}

	Vec Battle::GoingOwn(const Bit& B, double Age) const
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

	const char* Battle::GroundOn(const char* Ground) const
	{
		// (a grounds row with its own switch, grounds.<ground>.on, is drawn only when it is on; off, the ground is drawn
		// as earth, the look the battle had before the row came: rock, until the art director's stills and the Mac's
		// frame rate pass it. A row with no switch is always drawn)
		if (!Ground || !*Ground)
		{
			return Ground;
		}
		const std::string Switch = std::string("grounds.") + Ground + ".on";
		return Has(Switch) && !(Num(Switch, 1.0) >= 0.5) ? "earth" : Ground;
	}

	void Battle::Shell(double Now, const Vec& AtIn, const char* SizeIn, const char* GroundIn, bool bOnArmour)
	{
		GroundIn = GroundOn(GroundIn); // (a ground whose own switch is off is drawn as earth)
		// (a hit whose place or moment is no number, or lies beyond any field or battle, is no hit: every point of it
		// would be no number in the picture. The pre-check on 3e3c4ef)
		const auto Sound = [](double V, double Most) { return V == V && V >= -Most && V <= Most; };
		if (!Sound(Now, 1.0e9) || !Sound(AtIn.X, 1.0e7) || !Sound(AtIn.Y, 1.0e7) || !Sound(AtIn.Z, 1.0e7))
		{
			return;
		}
		SpareAtBurst = LingerSpare(); // (how much room the cloud's slow going may take: before this burst's own pieces count)
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
		const size_t HitFrom = Bits.size(); // (a sea battle's far floor on the hit's flash: SeaFlash)
		const std::string S = "sizes." + Size + ".";
		// (the size's dials held to what a shell can be: a row with a wild number or none draws a shell all the same)
		const double Scale = SafeNum(Num(S + "scale", 1.0), 0.01, 25.0, 1.0);
		const double K = std::sqrt(Scale);
		const double Within = SafeNum(Num(S + "shake_within_m", 40.0), 0.0, 1e5, 40.0);
		// (a size with quiet 1 is water moved by no explosion, the white water round a sinking ship: no flash and no jolt)
		const bool bQuiet = Num(S + "quiet", 0.0) > 0.0;
		if (bOnArmour)
		{
			Armour(Now, AtIn, Scale, D);
		}
		else
		{
			const Vec At(AtIn.X, AtIn.Y, Height(AtIn.X, AtIn.Y));
			const std::string G = "grounds." + Ground + ".";
			Paint P = PaintOf(Ground);
			const double Slow = Num(G + "crown_slow", 1.0);
			// (grounds.<ground>.smoke_of_ground, the art director's 20:25Z read of rock: the cloud the colour of the ground
			// it rose from, as the game's screen shows it there, pulled grey of the way toward its own grey and shade
			// times as light, its top top_shade times; written to land on that (picture.to_land). So one rule gives
			// Italy's limestone a warm pale cloud, the desert's escarpment a tan one and the steppe's stone a neutral one.
			// Where the game cannot say: the paint's smoke and the recipe's top, as before)
			double GrR = 0.0, GrG = 0.0, GrB = 0.0;
			if (Has(G + "smoke_of_ground.grey") && GroundColour(At, GrR, GrG, GrB))
			{
				const double Pull = SafeNum(Num(G + "smoke_of_ground.grey", 0.5), 0.0, 1.0, 0.5);
				const double Shade = SafeNum(Num(G + "smoke_of_ground.shade", 0.9), 0.2, 1.5, 0.9);
				const double TopShade = SafeNum(Num(G + "smoke_of_ground.top_shade", 0.75), 0.2, 1.5, 0.75);
				const double Grey = 0.2126 * GrR + 0.7152 * GrG + 0.0722 * GrB;
				const LandTable Land = LandTableOf();
				auto Of = [&](double Is, double By) { return Land.ToLand(std::min(255.0, (Is + (Grey - Is) * Pull) * By)); };
				P.Smoke = Rgb::FromPicture(Of(GrR, Shade), Of(GrG, Shade), Of(GrB, Shade));
				P.bSmokeTop = true;
				P.SmokeTop = Rgb::FromPicture(Of(GrR, TopShade), Of(GrG, TopShade), Of(GrB, TopShade));
			}
			const double DustK = Num(G + "dust", 1.0);
			const double ClodK = Ground == "water" ? 0.0 : Num(G + "clods", 1.0);
			const double ShowerK = Ground == "water" ? 0.0 : Num(G + "shower", ClodK);
			// (on the sea, the size's spout_m where it has one: the white column's height, apart from the land burst's
			// earth, so the bigger land bursts of 22 November left the sea as it was. One throw of the dice either way)
			const bool bSpoutOwn = Ground == "water" && Has(S + "spout_m.0");
			const double CrownH = SafeNum(Pick(D, S + (bSpoutOwn ? "spout_m" : "crown_m"), 8.0, 12.0), 0.1, 500.0, 10.0);
			// (art director 03:27Z 22 November, on the bigger bursts: "the throw at the flash is near black... a black
			// comic-book splat with an orange dot. Make it dark earth brown with the faces toward the flash lit warm,
			// floor about 15". burst.throw: lighter, the jets, spikes, body and foot up to that many times as light as the
			// ground's heavy earth has them, so they have a lit side and a shaded one above the floor; mottle, each piece
			// that share lighter or darker than the next, so a jet is streaked earth and never one flat fill; floor, the darkest
			// any point of the thrown earth is drawn; flash_lit, how far the earth nearest the flash goes toward
			// flash_colour on its side toward it, dying over flash_lit_s; half as much flash_lit_reach of the flash's own
			// width away. Never on water, nor for a quiet size. No dice)
			ThrowIs = ThrowRow();
			if (!bQuiet && Ground != "water")
			{
				const std::string Th = "burst.throw.";
				// (lighter is the most; with body, only as much as brings the ground's heavy earth up to that written value
				// at its brightest: sand and rubble, light already, are left as they are)
				const double LighterMost = SafeNum(Num(Th + "lighter", 1.0), 1.0, 8.0, 1.0);
				ThrowIs.Lighter = LighterMost;
				if (LighterMost > 1.0 && Has(Th + "body"))
				{
					const double BodyWritten = SafeNum(Num(Th + "body", 0.0), 0.0, 255.0, 0.0);
					const double HeavyIs = std::max(P.Heavy.R, std::max(P.Heavy.G, P.Heavy.B));
					ThrowIs.Lighter = SafeNum(Rgb::FromPicture(BodyWritten, BodyWritten, BodyWritten).R / std::max(HeavyIs, 1e-6), 1.0, LighterMost, 1.0);
				}
				ThrowIs.Mottle = SafeNum(Num(Th + "mottle", 0.0), 0.0, 0.9, 0.0);
				if (Has(Th + "floor.0"))
				{
					ThrowIs.bFloor = true;
					ThrowIs.Floor = Picture(Th + "floor", Rgb::FromPicture(46, 38, 30));
				}
				const double GlareK = SafeNum(Num(Th + "flash_lit", 0.0), 0.0, 4.0, 0.0);
				if (GlareK > 0.0)
				{
					const double FlashWide = 0.5 * (SafeNum(Num("burst.flash.reach_m.0", 1.6), 0.05, 20.0, 1.6) + SafeNum(Num("burst.flash.reach_m.1", 2.5), 0.05, 20.0, 2.5))
						* K * std::min(1.0, 1.2 / std::pow(std::max(K, 1e-3), 0.3)) * SafeNum(Num(S + "flash_k", 1.0), 0.1, 10.0, 1.0);
					ThrowIs.bGlare = true;
					ThrowIs.GlareK = GlareK;
					ThrowIs.GlareS = SafeNum(Num(Th + "flash_lit_s", 0.2), 0.02, 2.0, 0.2);
					ThrowIs.GlareReach = std::max(0.05, SafeNum(Num(Th + "flash_lit_reach", 1.0), 0.05, 20.0, 1.0) * FlashWide);
					ThrowIs.GlareCol = Picture(Th + "flash_colour", Rgb::FromPicture(200, 128, 60));
					ThrowIs.T0 = Now;
					ThrowIs.At = At + ZUp * (0.3 * K);
				}
			}
			if (!bQuiet)
			{
				Wink(Now, At, Scale, D, Ground == "water" ? std::string() : Size); // (the sea's flash as it was: the sizes' flash and ember were ruled for land)
			}
			if (Ground == "water" && Has("grounds.water.spout.rise_s"))
			{
				// (the sea's own column, where the recipe has its row; without it, the crown of pale dust below, as it was)
				Spout(Now + 0.005, At, CrownH, Size, D);
			}
			else
			{
				// (a sea battle, UseSea: a hit beyond the air's distance, where none used to be drawn, has fewer and bigger
				// puffs of smoke the smaller it stands in the picture: its crown sea.hit.full_px tall or more, all of them;
				// under that, that share, never under sea.hit.least. It comes in over the first sea.hit.over_m beyond
				// the air's distance, so two hits either side of that line are alike. A bomb's cloud is some 370 puffs,
				// each big enough to be drawn from 15 km: three such hits on the far line were the room)
				double Many = 1.0;
				if (bSeaFar && SeaHitFullPx > 0.0 && bEyeKnown)
				{
					const double Away = (LastEye - At).Length();
					if (Away > SeaGoneBeyond)
					{
						const double Share = std::clamp(CrownH * SafeNum(LastPpr, 1.0, 1e7, 1000.0) / Away / SeaHitFullPx, SeaHitLeast, 1.0);
						Many = 1.0 + (Share - 1.0) * Clamp01((Away - SeaGoneBeyond) / SeaHitOver);
					}
				}
				// (FX3, burst.far, the production lead's battle-scale plan of 22 November: a hit on land outside a sea battle
				// keeps Few of its pieces, each as much bigger. Few is FarK, its crown's height in the picture against
				// full_px (all of them at that or over, never under least), times BusyK: past halve_past pieces asked for on
				// the screen half, with the room four fifths full a quarter, and with more asked than the room over_busy (the
				// room leaves out the farthest first, so a wide shot's far line went bare); a hit whole in the picture only
				// near_busy, and only with the room four fifths full; between, by how small it stands. Its skirt keeps Few to skirt_pow, its smoke to smoke_pow: the
				// cloud stays on after a cut to a nearer eye. The GS1 study: at the Prokhorovka opening shells alone filled
				// the room, 743 pieces each, three quarters of them specks under 8 px)
				double Few = 1.0;
				if (bLandFar && !bSeaFar && LandFullPx > 0.0 && bEyeKnown && Ground != "water")
				{
					const double Away = std::max(1.0, (LastEye - At).Length());
					const double FarK = std::clamp(CrownH * SafeNum(LastPpr, 1.0, 1e7, 1000.0) / Away / LandFullPx, LandLeast, 1.0);
					const double Asked = static_cast<double>(RoomGate());
					const bool bFull = Asked > 0.8 * static_cast<double>(MaxPuffs), bOver = Asked > static_cast<double>(MaxPuffs);
					const double BusyFar = LandHalvePast > 0.0 && Asked > LandHalvePast ? (bOver ? LandOverBusy : bFull ? 0.25 : 0.5) : 1.0, BusyNear = bFull ? LandNearBusy : 1.0;
					const double BusyK = BusyNear + (BusyFar - BusyNear) * Clamp01((1.0 - FarK) / std::max(1.0 - LandLeast, 1e-3)); // (by how small it stands: a hit nearly whole in the picture is thinned nearly as a near one)
					Few = FarK * BusyK;
					++LandHits;
					LandHitsFew += Few < 1.0 ? 1u : 0u;
					LandFewSum += Few;
				}
				FewAlphaPow = Few < 1.0 ? LandAlphaPow : 0.0; // (its see-through pieces, laid bigger, as much thicker: Thicker)
				CrownGround = Ground; // (the skirt reads it too: grounds.<ground>.wet)
				if (!(Many < 0.5)) // (and under half, no skirt of dust racing out over the ground: 230 wide sheets, a few pixels long out there)
				{
					Skirt(Now + 0.01, At, K, P, DustK, D, Size, Few < 1.0 ? std::pow(Few, LandSkirtPow) : 1.0);
				}
				CrownLow = SafeNum(Num(G + "crown_low", 1.0), 0.2, 2.0, 1.0);
				CrownWide = SafeNum(Num(G + "crown_wide", 1.0), 0.5, 3.0, 1.0);
				Crown(Now + 0.005, At, CrownH, K, Slow, P, ClodK, ShowerK, Size, D, Few);
				CrownLow = CrownWide = 1.0;
				CrownGround.clear();
				Smoke(Now, At, CrownH, K, Scale, Size, Ground, P, DustK, D, Few < 1.0 ? Many * std::pow(Few, LandSmokePow) : Many);
				FewAlphaPow = 0.0;
				if (bLandFar && !bSeaFar && LandGonePx > 0.0 && Ground != "water")
				{
					for (size_t I = HitFrom; I < Bits.size(); ++I)
					{
						Bits[I].bFarCull = true; // (its flash, skirt, crown and smoke: Draw leaves out those too small to see far off)
					}
				}
			}
			// (grounds.<ground>.crater_k: a hard ground's scar smaller, rock's a shallow chipped star; no such key, 1)
			const double Across = SafeNum(Num(S + "crater_m", 1.5), 0.0, 200.0, 1.5) * SafeNum(Num(G + "crater_k", 1.0), 0.1, 4.0, 1.0);
			if (bScars && Across > 0.0 && Ground != "water")
			{
				Scar(Now + 0.25, At, Across, P, Ground == "snow", D);
			}
		}
		if (bSeaFar && !bQuiet && Ground != "water")
		{
			SeaFlash(Now, HitFrom); // (a hit on a ship or the shore: a shell falling in the sea is read by its column)
		}
		// (sizes.<size>.lens_near_m, the art director 07:22Z 22 November: the camera keeps out of a burst's cloud, and as
		// a backstop the lens cap's near_m grows with the burst, 10 m for a 75, 12 for a 105, 30 for a 500 lb bomb, so a
		// lens caught inside a big cloud dims and never goes black. No such key: the cap's own near_m)
		const double LensNear = SafeNum(Num(S + "lens_near_m", 0.0), 0.0, 1000.0, 0.0);
		if (LensNear > 0.0)
		{
			for (size_t I = HitFrom; I < Bits.size(); ++I)
			{
				Bits[I].LensNear = LensNear;
			}
		}
		if (bQuiet)
		{
			return;
		}
		Knock J;
		J.At = AtIn;
		J.Born = Now;
		J.Strength = std::min(1.0, Scale) * (bOnArmour ? 0.5 : 1.0);
		J.Within = Within;
		J.Push = SafeNum(Num("camera.shake.push_m." + Size, 0.0), 0.0, 20.0, 0.0);
		Knocks.push_back(J);
	}

	void Battle::InFlash(Bit& B, double Share, double OfLighter) const
	{
		// (a shell's thrown piece: as light as the throw's earth is, never darker than its floor, and lit by its burst's
		// own flash: Shell, ThrowIs)
		if (ThrowIs.Lighter > 1.0 && OfLighter > 0.0)
		{
			B.Col = B.Col * (1.0 + (ThrowIs.Lighter - 1.0) * std::min(OfLighter, 1.0));
		}
		if (ThrowIs.Mottle > 0.0 && OfLighter > 0.0 && B.LumpSeed != 0)
		{
			// (no two pieces of a jet the same shade, by the piece's own lump seed: no dice)
			B.Col = B.Col * (1.0 + ThrowIs.Mottle * (static_cast<double>((B.LumpSeed >> 7) & 0xFFFFu) * (2.0 / 65535.0) - 1.0));
		}
		if (ThrowIs.bFloor)
		{
			B.bNeverUnder = true;
			B.NeverUnder = ThrowIs.Floor;
		}
		if (ThrowIs.bGlare && Share > 0.0)
		{
			B.GlareK = ThrowIs.GlareK * Share;
			B.GlareFor = ThrowIs.GlareS;
			B.GlareEnd = ThrowIs.T0 + ThrowIs.GlareS;
			B.GlareReach = ThrowIs.GlareReach;
			B.GlareAt = ThrowIs.At;
			B.GlareCol = ThrowIs.GlareCol;
		}
	}

	void Battle::Wink(double T0, const Vec& At, double Scale, Dice& D, const std::string& Size)
	{
		// The flash: a small low star of ragged flame under one frame, a glow left in the crown's foot for 3 to 5
		// frames, and a real lamp for about one frame (it lifts what is within 30 m, never a white-out).
		const double K = std::sqrt(Scale);
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Rgb Core = Picture("burst.flash.core_colour", Rgb::FromPicture(236, 168, 72));
		const double Small = std::min(1.0, 1.2 / std::pow(std::max(K, 1e-3), 0.3));
		// (the game's own frames, build 1748: written as plain paint this star landed a flat orange, 202,124,6, darker
		// than pale dry ground, and even all round as a drawn star is. With burst.flash.tongues in the recipe the flash
		// is RaggedFlash's instead. The star's dice are thrown all the same, so the dirt and smoke after it fall as
		// they did)
		const bool bRagged = Has("burst.flash.tongues.0");
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
			if (!bRagged)
			{
				Bits.push_back(B);
			}
		}
		// (art director 19:58Z 21 November, for Brian's "bigger": a shell's flash is as wide and as long as its size's own
		// row has it, sizes.<size>.flash_k times the flash of burst.flash and flash_s seconds; and what it leaves low in
		// the earth is a dull orange glow no wider than burst.flash.ember.share of the flash, dying to dark over
		// ember.life_s. No such keys: as before)
		const double FlashGrows = Size.empty() ? 1.0 : SafeNum(Num("sizes." + Size + ".flash_k", 1.0), 0.1, 10.0, 1.0);
		const double FlashLasts = Size.empty() ? 0.0 : SafeNum(Num("sizes." + Size + ".flash_s", 0.0), 0.0, 0.5, 0.0);
		if (bRagged)
		{
			RaggedFlash(T0, At, K, Small, FlashGrows, FlashLasts);
		}
		const Rgb Glow = Picture("burst.crown.foot_glow.colour", Flame);
		const bool bEmber = !Size.empty() && Has("burst.flash.ember.life_s.0"); // (no size, as on the sea: no ember)
		const Span EmberLife = Range("burst.flash.ember.life_s", 0.15, 0.3);
		const double EmberR = 0.46 * SafeNum(Num("burst.flash.ember.share", 0.3), 0.02, 1.0, 0.3) * 1.3 * 0.5 * (SafeNum(Num("burst.flash.reach_m.0", 1.6), 0.05, 20.0, 1.6) + SafeNum(Num("burst.flash.reach_m.1", 2.5), 0.05, 20.0, 2.5)) * K * Small * FlashGrows;
		const Rgb EmberCol = Picture("burst.flash.ember.colour", Glow * 0.8), EmberTo = Picture("burst.flash.ember.to", Rgb::FromPicture(40, 24, 16));
		const double EmberNear = SafeNum(Num("burst.flash.ember.nearer", 0.0), 0.0, 20.0, 0.0);
		const double EmberStretch = SafeNum(Num("burst.flash.ember.stretch", 1.8), 1.0, 4.0, 1.8), EmberPoint = SafeNum(Num("burst.flash.ember.point", 0.6), 0.0, 1.0, 0.6);
		const double EmberHeart = SafeNum(Num("burst.flash.ember.heart", 0.7), 0.0, 1.0, 0.7), EmberAlpha = SafeNum(Num("burst.flash.ember.alpha", 0.85), 0.0, 1.0, 0.85);
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
			if (bEmber)
			{
				// (the same five pieces by the same dice: bigger, lower, duller and longer)
				const double Big = EmberR * Rr / (0.45 * K);
				B.From = At + ZUp * (0.6 * Big) + Dir * (Big * 0.4);
				B.Life = SafeNum(EmberLife.A + (EmberLife.B - EmberLife.A) * (B.Life - 0.1) / 0.06, 0.05, 1.0, 0.2);
				B.R0 = Big * 0.8;
				B.R1 = Big * 1.1;
				B.Col = EmberCol;
				B.Col2 = EmberTo;
				B.Hold = 0.35;
				B.Nearer = EmberNear * EmberR; // (glimpsed through the dirt nearest it, hidden by the dirt further out)
				B.Stretch = EmberStretch; // (a glow, not a tongue of flame)
				B.Point = EmberPoint;
				B.Heart = EmberHeart;
				B.Alpha = EmberAlpha;
			}
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

	void Battle::RaggedFlash(double T0, const Vec& At, double K, double Small, double GrowK, double LastsS)
	{
		// Art director 19:57Z on the game's own frames (build 1748): a shell's flash follows the gun flashes' rule, and is
		// no star. Back to front: ragged tongues of unequal length thrown mostly up and out, as the dirt is, orange
		// inside and red-orange toward their ends; short licks flung low; a lumpy orange body at the hit; a yellow
		// inside it, at least as bright as pale ground; and a small heart, heart_share of the flash's width, that burns
		// near white (over times white: picture.lit_gain). Dense, shrinking as it dies, for about two frames. The
		// tongues stand among the first of the dirt; the body, the yellow and the heart shine through it.
		// Its own dice, by place and moment: no two match, and the dirt's dice are left alone.
		// Art director 20:12Z on its first still, where hard-edged flat fills read as cut paper: every edge fades to
		// nothing over feather_m (2 to 3 pixels from the battle eye), and no piece is one flat colour: a tongue is
		// brighter at its root and darker red-orange at its end, the body orange within and red-orange at its edge.
		auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		const std::string F = "burst.flash.";
		Dice DF(PlaceKey(At) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(static_cast<int64_t>(std::min(std::max(0.0, T0), 9.0e15) * 1000.0)) ^ 0xF1A5ull); // (held: a wild moment is no cast past the type)
		const Rgb HeartCol = Picture(F + "heart", Rgb::FromPicture(255, 233, 158));
		const Rgb Yellow = Picture(F + "yellow", Rgb::FromPicture(255, 186, 81));
		const Rgb Orange = Picture(F + "orange", Rgb::FromPicture(236, 131, 71));
		const Rgb Red = Picture(F + "red", Rgb::FromPicture(200, 80, 30));
		const Rgb Tip = Orange.Mix(Red, 0.6);
		const double HeartOver = Safe(Num(F + "over", 4.0), 1.0, 4.0, 1.0);
		const double YellowOver = Safe(Num(F + "yellow_over", 1.8), 1.0, 4.0, 1.0);
		const double HeartShare = Safe(Num(F + "heart_share", 0.2), 0.0, 1.0, 0.2);
		const double Size = Safe(K * Small * GrowK, 0.0, 30.0, 1.0); // (GrowK: a shell's own flash_k; 1 leaves it as it was)
		const double Feather = Safe(Num(F + "feather_m", 0.035), 0.0, 1.0, 0.035) * Size;
		// (art director 06:08Z 20 November, on build 1758's frames. From 18 m feather_m is about one pixel and the tongues
		// read as cut paper: with feather_px every feathered edge fades over at least that many pixels on the screen,
		// whatever the distance. And the tongues read as one flat orange: with root_reach the yellow-orange root
		// (root_yellow of the way from orange to yellow) holds that share of the way out along each tongue before it
		// reddens, so the gradient shows at the game's size. Neither key: as before, and the dice fall as they did)
		const double FeatherPx = Safe(Num(F + "feather_px", 0.0), 0.0, 32.0, 0.0);
		const double RootReach = Safe(Num(F + "root_reach", 0.0), 0.0, 0.6, 0.0);
		const Rgb RootFar = Orange.Mix(Yellow, Safe(Num(F + "root_yellow", 0.75), 0.0, 1.0, 0.75));
		// (art director 08:03Z 20 November, on build 1762's frames from 18 m: there the root showed as a flat yellow petal
		// with a firm line round it inside each orange tongue, and the flash read as a flower. With root_melt (and
		// root_reach) a tongue's fan is drawn from where it leaves the body (Bit::Hub): orange there, running evenly
		// out to the red-orange of its edge. Its root is the tongue's own shape laid over it, outline for outline,
		// yellow and whole at that same point and thinning evenly from there to nothing at the tongue's rim, half
		// gone root_melt of the way out, turning orange as it thins. So the yellow melts into the orange along the
		// tongue and across it with no line anywhere, the hottest yellow runs down its centre line, and its edges are
		// orange to red-orange. root_from: where the point sits, as a share of what the body hides of the tongue.
		// The dice fall as they did. No key: as before)
		const double RootMelt = RootReach > 0.0 ? Safe(Num(F + "root_melt", 0.0), 0.0, 0.9, 0.0) : 0.0;
		const double RootFrom = Safe(Num(F + "root_from", 0.85), 0.0, 1.5, 0.85);
		const Rgb RootCol = Orange.Mix(Yellow, 0.4);
		const double RootOver = 1.0 + (YellowOver - 1.0) * 0.3;
		const double L = Safe(Range(F + "reach_m", 1.2, 1.9).Pick(DF), 0.05, 20.0, 1.5) * Size; // (its longest tongue)
		const double LifeOfRow = Safe(Range(F + "life_s", 0.05, 0.06).Pick(DF), 0.02, 0.5, 0.055);
		const double Life = LastsS > 0.0 ? Safe(LastsS, 0.02, 0.5, LifeOfRow) : LifeOfRow; // (a shell's own flash_s; the dice fall as they did)
		const int NTongues = static_cast<int>(std::lround(Safe(Range(F + "tongues", 6.0, 10.0).Pick(DF), 0.0, 16.0, 8.0)));
		const int NLicks = static_cast<int>(std::lround(Safe(Range(F + "licks", 2.0, 4.0).Pick(DF), 0.0, 8.0, 3.0)));
		if (L <= 0.0)
		{
			return;
		}
		// (one piece: an oval about Mid along Way, HalfLong and HalfWide, shrinking a little as it dies. Nearer: sorted
		// that much nearer the eye than it stands)
		// (Inner: where its colour begins to turn and its edge to fade; under 0, as near its rim as feather_m is wide)
		auto Piece = [&](const Vec& Mid, const Vec& Way, double HalfLong, double HalfWide, const Rgb& Col, const Rgb& Edge, double EdgeK_, double EdgeIn_, double Inner, double Nearer, double LifeK)
		{
			Bit B;
			B.Born = T0;
			B.Life = Life * LifeK;
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
			B.Hold = 0.7;
			B.Heart = 1.0;
			B.Rim = 0.0;
			B.Inner = Inner >= 0.0 ? Inner : std::clamp(1.0 - Feather / B.R0, 0.35, 0.9);
			B.FeatherPx = Inner >= 0.0 ? 0.0 : FeatherPx;
			B.Edge = Edge;
			B.EdgeK = EdgeK_;
			B.EdgeIn = EdgeIn_;
			B.Nearer = Nearer;
			B.bLight = true;
			B.bWhole = true; // (however slow the frame, its first one shows the flash whole)
			B.TwinPx = 6.0; // (two fans, the second half a step round: no straight facet along a tongue or round a lump)
			B.LumpSeed = static_cast<uint32_t>(DF.U() * 1073741823.0);
			return B;
		};
		auto Thrown = [&](double LeanA, double LeanB, const Vec& Tilt_)
		{
			const double Lean = Radians(DF.U(LeanA, LeanB)), Az = DF.U(0.0, 2.0 * Pi);
			return (Vec(std::cos(Az) * std::sin(Lean), std::sin(Az) * std::sin(Lean), std::cos(Lean)) + Tilt_).Unit(ZUp);
		};
		// (the whole flash leans one way, as the dirt's crown does)
		const double TiltAz = DF.U(0.0, 2.0 * Pi);
		const Vec Tilt = Vec(std::cos(TiltAz), std::sin(TiltAz), 0.0) * std::tan(Radians(DF.U(0.0, 22.0)));
		const Vec Up = (ZUp + Tilt).Unit(ZUp);
		const Vec Foot = At + ZUp * 0.04;
		const double W = L * DF.U(0.22, 0.3); // (half the body's width: a squat lump, so the tongues stand well clear of it)
		for (int I = 0; I < NTongues; ++I)
		{
			// (the first the tallest and near upright; the rest shorter, flung out at every lean, bunched as the dice fall)
			const Vec Way = I == 0 ? Thrown(0.0, 25.0, Tilt) : Thrown(15.0, 78.0, Tilt);
			const double Long = L * (I == 0 ? 1.0 : DF.U(0.35, 1.0)), S0 = L * DF.U(0.02, 0.1);
			// (blunt and ragged at its end, as gas is: a needle point reads as a drawn flame. Art director 14:26Z, of the guns)
			const double Point = DF.U(0.45, 0.7), HalfLong = std::max(1e-3, (Long - S0) / (2.0 + Point));
			const double HalfWide = Long * DF.U(0.08, 0.15), LifeK = DF.U(0.7, 1.0);
			Bit T = Piece(Foot + Way * (S0 + HalfLong), Way, HalfLong, HalfWide, Tip, Red, 0.9, 0.45, -1.0, 0.0, LifeK);
			T.Point = Point;
			T.Rough = DF.U(0.4, 0.6);
			T.Vel = Way * (DF.U(2.0, 6.0) * Size);
			T.Drag = 6.0;
			// (the body hides a tongue's first W sideways and twice that upward)
			const double Hidden = W * (1.0 + std::max(0.0, Way.Dot(Up)));
			if (RootMelt > 0.0)
			{
				// (orange at its root; its edge band and rim the colours they were)
				T.Hub = std::clamp((S0 + HalfLong - RootFrom * Hidden) / HalfLong, -0.5, 0.6);
				T.Col = T.Col2 = Orange;
				T.EdgeK = 0.96;
				T.EdgeIn = 0.79;
			}
			Bits.push_back(T);
			// (its inside, three fifths as long, bright at the root and melting out into the tongue: it reddens toward its end)
			// (with root_reach: seven tenths of the tongue's width, and its root's colour held from the body out to root_reach
			// of what shows of the tongue beyond the body, which hides its first W sideways and twice that upward; from
			// there it turns orange and melts out over a third of what is left of the tongue)
			const double SolidTo = std::min(Hidden + RootReach * std::max(0.0, Long - Hidden), 0.8 * Long);
			const double MeltTo = SolidTo + 0.35 * (Long - SolidTo);
			Bit In = RootReach > 0.0 ? Piece(Foot + Way * (0.5 * MeltTo), Way, 0.5 * MeltTo, HalfWide * 0.7, RootFar, Orange, 1.0, 0.2, std::clamp(2.0 * SolidTo / std::max(MeltTo, 1e-6) - 1.0, 0.3, 0.8), 0.05 * L, LifeK)
				: Piece(Foot + Way * (S0 + HalfLong * 0.6), Way, HalfLong * 0.6, HalfWide * 0.75, RootCol, Orange, 1.0, 0.5, 0.3, 0.05 * L, LifeK);
			In.Over = RootOver;
			In.Point = RootReach > 0.0 ? 0.0 : Point;
			In.Rough = RootReach > 0.0 ? 0.5 * T.Rough : T.Rough;
			In.Vel = T.Vel;
			In.Drag = 6.0;
			if (RootMelt > 0.0)
			{
				// (the tongue's own shape and outline, so its rim is the tongue's rim and nothing of it shows outside the
				// tongue. Written RootOver times darker at its rim, since the piece burns that far past white for its root)
				In = T;
				In.Col = In.Col2 = RootFar;
				In.Over = RootOver;
				In.Edge = Orange * (1.0 / RootOver);
				In.EdgeK = 1.0;
				In.EdgeIn = RootMelt;
				In.Inner = std::max(0.1, RootMelt);
				In.FeatherPx = 0.0;
				In.Heart = 0.5;
				In.Rim = 0.0;
				In.Nearer = 0.05 * L;
			}
			Bits.push_back(In);
		}
		for (int I = 0; I < NLicks; ++I)
		{
			// (the licks: short, flung low out of the body's flanks)
			const Vec Way = Thrown(60.0, 86.0, Vec(0.0, 0.0, 0.0));
			const double Long = L * DF.U(0.2, 0.45);
			Bit Lick = Piece(Foot + ZUp * (W * 0.25) + Way * (W * 0.6 + Long * 0.3), Way, Long * 0.42, Long * DF.U(0.1, 0.18), Orange, Red, 0.9, 0.35, 0.4, 0.0, DF.U(0.6, 0.9));
			Lick.Point = 0.5;
			Lick.Rough = 0.4;
			Bits.push_back(Lick);
		}
		// (the body, the yellow and the heart: over every tongue whichever way it is thrown, and through the first dirt;
		// the yellow and the heart over the second lump too, wherever it is thrown)
		const double Front = 1.5 * Size + L;
		// (a lump's inside: a copy of it four fifths the size, brighter and melting out into it, so the dice fall as they did)
		auto Within = [&](const Bit& Outer)
		{
			Bit B = Outer;
			B.R0 = Outer.R0 * 0.8;
			B.R1 = Outer.R1 * 0.8;
			B.Col = B.Col2 = RootCol;
			B.Edge = Orange;
			B.EdgeK = 1.0;
			B.EdgeIn = 0.5;
			B.Inner = 0.3;
			B.Over = RootOver;
			B.Nearer = Outer.Nearer + 0.02 * W;
			return B;
		};
		const double BodyLong = W * DF.U(0.9, 1.25); // (it stands on the ground: the ground never cuts it along a straight line)
		Bit Body = Piece(Foot + Up * (BodyLong * 0.9), Up, BodyLong, W, Orange.Mix(Tip, 0.5), Tip, 0.7, 0.3, -1.0, Front, 1.0);
		Body.Point = DF.U(0.1, 0.35);
		Body.Rough = DF.U(0.35, 0.5);
		Bits.push_back(Body);
		Bits.push_back(Within(Body));
		{
			// (and a second, smaller lump thrown to one side of it, so the body is never the same shape twice)
			const Vec Aside = Thrown(55.0, 85.0, Vec(0.0, 0.0, 0.0));
			const double Lump = W * DF.U(0.55, 0.8);
			Bit Second = Piece(Foot + ZUp * (Lump * 0.6) + Aside * (W * DF.U(0.5, 0.9)), Aside, Lump * DF.U(1.0, 1.3), Lump, Orange.Mix(Tip, 0.5), Tip, 0.7, 0.3, -1.0, Front - 0.1 * W, DF.U(0.8, 1.0));
			Second.Point = 0.3;
			Second.Rough = 0.45;
			Bits.push_back(Second);
			Bits.push_back(Within(Second));
		}
		// (yellow well out from the heart, turning orange only at its rim, so a band of it always shows round the heart)
		const double YellowWide = L * DF.U(0.17, 0.22), YellowLong = L * DF.U(0.2, 0.27);
		Bit Inside = Piece(Foot + Up * (YellowLong * 0.95), Up, YellowLong, YellowWide, Yellow, Orange, 0.7, 0.1, 0.6, Front + 1.2 * W, 1.0);
		Inside.Point = 0.15;
		Inside.Rough = 0.3;
		Inside.Over = YellowOver;
		Bits.push_back(Inside);
		// (the heart: its width heart_share of the flash's, which is a little over its longest tongue)
		const double HeartR = std::max(1e-3, HeartShare * L * 0.55);
		Bit Core = Piece(Foot + Up * (HeartR * 1.5), Up, HeartR, HeartR, HeartCol, Yellow, 1.0, 0.3, 0.6, Front + 1.5 * W, 1.0);
		Core.Over = HeartOver;
		Bits.push_back(Core);
	}

	void Battle::Skirt(double T0, const Vec& At, double K, const Paint& P, double DustK, Dice& D, const std::string& Size, double Few)
	{
		// The shock: one low skirt of the ground's own dust, racing out and thinning, with a soft billowing top that
		// settles to a low haze by about frame 30. Upright cards, so that its foot never cuts into the ground.
		const std::string R = "burst.shock_ring.";
		const std::string Sk = R + "skirt.";
		// (art director 19:58Z 21 November, the ground skirt of a burst with bulk: with burst.shock_ring.of_cloud and the
		// size's own cloud_m, the skirt goes out to that many times the cloud's width across, whatever the size's scale,
		// and its pieces are sized by that reach (the row's metres being a reach of roll_ref_m's); with skirt.hangs it
		// stays as long as the cloud does, going as the cloud goes (Lingering, the size's own times) but never
		// climbing, keeping hang_keep times the cloud's share of its pieces. Neither key: as before)
		const double CloudM = Size.empty() ? 0.0 : SafeNum(Num("sizes." + Size + ".cloud_m", 0.0), 0.0, 400.0, 0.0);
		const double OfCloud = SafeNum(Num(R + "of_cloud", 0.0), 0.0, 4.0, 0.0);
		const bool bRolls = CloudM > 0.0 && OfCloud > 0.0;
		const double Reach = bRolls ? 0.5 * OfCloud * CloudM : Num(R + "reach_m", 15.0) * K;
		const double KS = bRolls ? Reach / SafeNum(Num(R + "roll_ref_m", 22.5), 1.0, 400.0, 22.5) : K; // (what its pieces are sized by)
		const bool bHangs = bRolls && Num(Sk + "hangs", 0.0) > 0.0;
		const LingerRow* HangsOn = bHangs ? &LingerOf("shell", Size) : nullptr;
		LingerRow HangsAs; // (the skirt goes as its cloud does: the bulk's own smoke-left numbers)
		if (HangsOn != nullptr)
		{
			HangsAs = *HangsOn;
			HangsAs.Third = HangsAs.BulkThird;
			HangsAs.TwoThirds = HangsAs.BulkTwoThirds;
			HangsOn = &HangsAs;
		}
		// (the skirt of a burst with bulk is a rolling front: inner_alpha, how thick the dust inside the front is, as a
		// share of the front's; high_vary, how much lower than most_high_m some of its pieces stand, so its top is uneven;
		// rough, its pieces' lumpy outline. By dice of their own. No keys: as before)
		const double InnerAlpha = SafeNum(Num(Sk + "inner_alpha", 1.0), 0.0, 2.0, 1.0);
		// (inner_high: how high the dust inside the front may stand, as a share of most_high_m, a low even haze; front_stretch:
		// how long along the ring a piece of the front is for its width, 1 a round billow)
		const double InnerHigh = SafeNum(Num(Sk + "inner_high", 1.0), 0.05, 1.0, 1.0);
		const double FrontStretch = Has(Sk + "front_stretch") ? SafeNum(Num(Sk + "front_stretch", 1.0), 1.0, 6.0, 1.0) : Num(Sk + "stretch_along_ring", 1.7);
		const double HighVary = SafeNum(Num(Sk + "high_vary", 0.0), 0.0, 1.0, 0.0), SkirtRough = SafeNum(Num(Sk + "rough", 0.0), 0.0, 1.0, 0.0);
		Dice DS(PlaceKey(At) * 0xA24BAED4963EE407ull ^ 0x5C1A7ull);
		const double HangKeep = SafeNum(Num(Sk + "hang_keep", 0.7), 0.05, 2.0, 0.7), HangWide = SafeNum(Num(Sk + "hang_wide", 0.3), 0.0, 1.0, 0.3), HangSpread = SafeNum(Num(Sk + "hang_spread", 0.3), 0.0, 1.0, 0.3);
		const Rgb Col = P.Ring * Num(R + "colour_from_ground", 0.85); // (the ground's own ring colour where it has one: earth's darker than its dust)
		// (art director 01:19Z 23 November, a thaw: wet ground sprays, it does not dust. With grounds.<ground>.wet on, the
		// skirt is ring_colour, a dark grey-brown, ring_high_k as high, gone in ring_life_s, and never hangs with the
		// cloud, as the cloud's low ring is (Bulk). By dice of its own, so its pieces go where they did. Off: as before)
		const std::string Wt = "grounds." + CrownGround + ".wet.";
		const bool bWetSkirt = !CrownGround.empty() && Num(Wt + "on", 0.0) >= 0.5;
		Dice DWS(PlaceKey(At) * 0x9FB21C651E98DF25ull ^ 0x5E7ull);
		const Rgb WetSkirtCol = bWetSkirt ? Picture(Wt + "ring_colour", Col) : Col;
		const double WetHighK = bWetSkirt ? SafeNum(Num(Wt + "ring_high_k", 1.0), 0.05, 1.0, 1.0) : 1.0;
		const Span WetLife = Range(Wt + "ring_life_s", 0.8, 1.2);
		const double Many = std::max(0.7, std::sqrt(K));
		// (burst.far, Shell: Few of its pieces, never under 8 out and 4 in, each as much wider as keeps the ring as full)
		const bool bFew = Few < 1.0;
		const int NOutAll = static_cast<int>(Num(Sk + "puffs_outer", 110.0) * Many), NInAll = static_cast<int>(Num(Sk + "puffs_inner", 60.0) * Many);
		const int NOut = bFew ? std::min(NOutAll, std::max(8, static_cast<int>(NOutAll * std::max(Few, 0.0) + 0.5))) : NOutAll;
		const int NIn = bFew ? std::min(NInAll, std::max(4, static_cast<int>(NInAll * std::max(Few, 0.0) + 0.5))) : NInAll;
		const double FewR = bFew ? std::min(3.0, std::sqrt(static_cast<double>(NOutAll + NInAll) / std::max(1, NOut + NIn))) : 1.0;
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
		const double MostHigh = Num(Sk + "most_high_m", 0.0) * KS;
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
			B.From = At + Outward * (D.U(0.4, 1.0) * KS);
			const double Out_ = Speed * D.U(0.85, 1.0); // (how far out it goes, as a share of the reach that way)
			B.Vel = Outward * (Reach * ReachAt(A) * Out_ * Drag);
			B.Drag = Drag;
			B.Rise = RiseOf * D.U(0.6, 1.4);
			B.Windage = 0.6;
			B.Life = LifeOf.Pick(D) * std::min(2.0, K);
			if (bWetSkirt)
			{
				B.Life = SafeNum(WetLife.Pick(DWS), 0.1, 60.0, 1.0);
			}
			B.R0 = StartR * KS * FewR;
			B.R1 = EndR * KS * D.U(0.7, 1.3) * (bFront ? 1.0 : InnerK) * FewR; // (fewer, broader inner ones fill the round)
			B.Grow = GrowOf;
			B.Stretch = bFront ? FrontStretch : StretchOf;
			B.Along = Vec(-std::sin(A), std::cos(A), 0.0);
			B.Flatten = FlattenOf * D.U(0.8, 1.2) * (bFront ? Billow : 1.0); // (the front edge stands tallest)
			if (MostHigh > 0.0)
			{
				B.Flatten = std::min(B.Flatten, (HighVary > 0.0 ? MostHigh * (1.0 - HighVary * DS.U()) : MostHigh) * (bFront ? 1.0 : InnerHigh) / (1.8 * std::max(B.R1, 1e-3))); // (its top, at 1.8 times its half height, under most_high_m)
			}
			B.Flatten *= WetHighK;
			if (SkirtRough > 0.0)
			{
				B.Rough = SkirtRough;
				B.LumpSeed = static_cast<uint32_t>(DS.U() * 4294967295.0);
			}
			B.Col = WetSkirtCol * D.U(0.9, 1.08);
			B.Col2 = B.Col;
			// (its outer part feathers away: full strength out to FeatherFrom of the reach, a sixth of it at the rim)
			const double Feather = Out_ > FeatherFrom ? 1.0 - 0.85 * Clamp01((Out_ - FeatherFrom) / std::max(1.0 - FeatherFrom, 1e-3)) : 1.0;
			B.Alpha = std::min(0.9, AlphaOf.Pick(D) * DustAlpha) * Feather * (bFront ? 1.0 : InnerAlpha);
			B.Alpha = Thicker(B.Alpha, FewR);
			B.FadeIn = 0.02;
			B.Hold = 0.1;
			B.HazeBy = HazeBy;
			B.HazeAlpha = std::min(B.Alpha, HazeAlpha);
			B.ShadeK = ShadeOf; // (a lit top and a shadowed foot, so it shows its height)
			B.Heart = HeartOf;
			B.Rim = 0.0;
			if (HangsOn)
			{
				// (hang_keep of its pieces hang on with the cloud, each to the cloud's end; the rest settle as a skirt's
				// dust ever did, so a barrage's skirts do not fill the room. A wet skirt never hangs)
				if (DS.U() < HangKeep && !bWetSkirt)
				{
					Lingering(B, *HangsOn, T0, At, 0.6, 0.0, SpareAtBurst, 1.0 / std::max(HangsOn->Keep, 0.05));
				}
				if (B.bTail)
				{
					B.TailLift = 0.0; // (it stays on the ground)
					B.TailWide *= HangWide;
					B.TailSpread *= HangSpread;
				}
			}
			Bits.push_back(B);
		}
	}

	void Battle::Crown(double T0, const Vec& At, double CrownH, double K, double Slow, const Paint& P, double ClodK, double ShowerK, const std::string& Size, Dice& D, double Few)
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
		const double ClumpRim = SafeNum(Num(C + "side_clump_rim", 0.55), 0.0, 1.0, 0.55); // (how much of a clump is left at its very edge)
		const double TipGrain = Num(C + "tip_grain", 0.55); // (a heavy jet's tip torn into smaller pieces, scattered: never one long translucent blade)
		const double TipRough = Num(C + "tip_rough", 0.6);
		const double HeavyHeart = Num(C + "heavy_heart", 0.92); // (how solid a heavy piece is at its heart ring)
		const double HeavyInner = Num(C + "heavy_inner", 0.72); // (and where that ring sits: the edge band is the rest)
		const double TipInner = Num(C + "tip_inner", 0.6);
		const double PieceMax = Num(Path("piece_max_m"), 0.0); // (0: no limit, as for the shells)
		// (art director 19:58Z 21 November, bigger bursts: a jet twice as tall stands against the sky, and a dark piece
		// thinning there reads as grey glass. With burst.crown.to_dust a heavy jet above its root goes that far toward
		// the ground's own dust as it thins, and holds solid for jet_hold of its life, so it ends as a streak of dust)
		const bool bJetDust = Has(C + "to_dust");
		const double JetDust = SafeNum(Num(C + "to_dust", 0.0), 0.0, 1.0, 0.0), JetHold = SafeNum(Num(C + "jet_hold", 0.4), 0.05, 0.95, 0.4);
		// (art director 03:27Z 22 November: at half a second the crown was "a few broad flat wedges with straight edges...
		// cardboard slabs. Keep the mass, but make it more, narrower streaks with soft tips that fall back by about
		// 1.5 s". More and thinner heavy jets are heavy_jets and heavy_thick. heavy_stretch: a heavy jet's pieces drawn
		// that long for their width, and stepped as far apart, so a thin jet is a streak of few pieces; heavy_swell:
		// they widen that much over their lives and no more, so a streak stays one; heavy_droop: a piece falls faster the
		// further up its jet it is, so the tips come back down; tip_life_s: how long the piece at a jet's very tip
		// lasts, the root's as ever; tip_heart: how thin the tip is at its heart ring, a soft end; tip_swell: at the
		// very tip a piece ends that many times its width, under 1 drawing in, so the streak narrows away as it falls
		// and is never a pale sheet standing in the air; heart_end: above its root a piece's heart thins to this by
		// the end of its life. A size may have its own of each. None of them: as before, by the same dice)
		const bool bHeavyAlong = Has(Path("heavy_stretch")), bHeavySwell = Has(Path("heavy_swell") + ".0"), bTipLife = Has(Path("tip_life_s") + ".0"), bTipHeart = Has(Path("tip_heart"));
		const bool bTipSwell = Has(Path("tip_swell") + ".0"), bHeartEnd = Has(Path("heart_end"));
		const double HeavyAlong = bHeavyAlong ? SafeNum(Num(Path("heavy_stretch"), Along0), 1.0, 8.0, Along0) : Along0;
		const Span HeavySwell = Range(Path("heavy_swell"), 1.8, 2.4), TipLifeS = Range(Path("tip_life_s"), 1.2, 2.0), TipSwell = Range(Path("tip_swell"), 1.8, 2.4);
		const double HeartEndIs = SafeNum(Num(Path("heart_end"), 0.5), 0.0, 1.0, 0.5);
		const double HeavyDroop = SafeNum(Num(Path("heavy_droop"), 0.0), 0.0, 20.0, 0.0);
		const double TipHeart = SafeNum(Num(Path("tip_heart"), HeavyHeart), 0.0, 1.0, 0.5);
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
		const int NFillAll = D.Whole(static_cast<int>(Num(C + "fill_jets.0", 10.0)), static_cast<int>(Num(C + "fill_jets.1", 16.0)));
		// (burst.far, Shell: a hit that keeps Few of its pieces keeps every heavy jet, the square root of Few of the dust
		// between them, never under 2, each jet of pieces up to three times as long and as far apart (JetCoarse), so it
		// is the same streak in fewer pieces; Few of the spikes, the clumps and the clods; the square root of Few of the
		// body and the foot, each as much bigger. Few 1: as before)
		const bool bFew = Few < 1.0;
		const double FewRoot = std::sqrt(std::clamp(Few, 0.0, 1.0));
		const int NFill = bFew ? std::min(NFillAll, std::max(2, static_cast<int>(NFillAll * FewRoot + 0.5))) : NFillAll;
		const double JetCoarse = bFew ? std::min(3.0, 1.0 / std::max(FewRoot, 1e-3)) : 1.0;
		const double LumpCoarse = bFew ? std::min(2.0, 1.0 / std::sqrt(std::max(FewRoot, 1e-3))) : 1.0;
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
			Vec Dir = (Vec(std::cos(Az) * std::sin(Radians(Lean)), std::sin(Az) * std::sin(Radians(Lean)), std::cos(Radians(Lean))) + Tilt).Unit();
			double ReachJ = CrownH * ReachF;
			if (CrownLow != 1.0 || CrownWide != 1.0)
			{
				Squash(Dir, ReachJ); // (a hard ground's fan: lower and wider)
			}
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
				if (bHeavy && HeavyDroop > 0.0)
				{
					B.Gravity = 9.8 / Slow * (1.0 + HeavyDroop * F); // (the tips come back down)
				}
				B.Windage = 0.35;
				const double LifeOwn = D.U(1.2, 2.0);
				B.Life = LifeOwn * Slow;
				if (bHeavy && bTipLife)
				{
					// (the root lasts as ever; the tip tip_life_s, by the same throw of the dice)
					const double LifeTip = SafeNum(TipLifeS.A + (TipLifeS.B - TipLifeS.A) * (LifeOwn - 1.2) / 0.8, 0.2, 10.0, 1.2);
					B.Life = (LifeOwn + (LifeTip - LifeOwn) * F) * Slow;
				}
				B.R0 = Rr;
				const double SwellOwn = D.U(1.8, 2.4);
				B.R1 = Rr * SwellOwn;
				if (bHeavy && (bHeavySwell || bTipSwell))
				{
					const double SwellRoot = bHeavySwell ? HeavySwell.A + (HeavySwell.B - HeavySwell.A) * (SwellOwn - 1.8) / 0.6 : SwellOwn;
					const double SwellTip = bTipSwell ? TipSwell.A + (TipSwell.B - TipSwell.A) * (SwellOwn - 1.8) / 0.6 : SwellRoot;
					B.R1 = Rr * SafeNum(SwellRoot + (SwellTip - SwellRoot) * F, 0.2, 4.0, 1.5);
				}
				B.Grow = 0.5;
				B.Along = Dir;
				B.Stretch = 1.0 + ((bTip ? HeavyAlong + (TipStretch - HeavyAlong) * OnTip : (bHeavy ? HeavyAlong : FillStretch)) - 1.0) * P.Grain; // (grains are not drawn out)
				B.Stretch *= JetCoarse; // (burst.far: as much longer as its pieces are fewer)
				B.Point = bTip ? TipPoint : (bHeavy ? 0.3 : 0.0); // (torn and pointed, never rounded; the dust between has no points)
				B.Tail = 0.15;
				B.Col = Col;
				B.Col2 = P.bFirst ? Col : (bHeavy && F < RootSolid ? P.Heavy.Mix(P.Dust, 0.35) : (bDarkFoot ? Settled : P.Dust)); // (the root stays dark earth)
				B.Cool = 0.7;
				B.Alpha = (bHeavy ? (bTip ? 1.0 - (1.0 - TipAlpha) * OnTip : 1.0) : FillAlpha * (1.0 - 0.3 * F)) * (bGrained ? D.U(0.7, 1.0) : 1.0);
				B.FadeIn = 0.01;
				B.Hold = 0.4;
				if (bJetDust && bHeavy && !P.bFirst && F >= RootSolid)
				{
					B.Col2 = B.Col2.Mix(P.Dust, JetDust);
					B.Hold = JetHold;
				}
				// (heavy at the root, thinning only near the tip: the lower half of a heavy jet dense, with hard lumpy
				// edges as the clods have, never a translucent blade)
				const double Root = bHeavy && !bGrained ? Clamp01((RootSolid - F) / std::max(RootSolid, 1e-3)) : 0.0;
				// (a heavy piece is solid out to most of its radius and then fades in a thin soft edge; a wide see-through
				// band with a hard rim, on a big piece, is what read as glass)
				B.Heart = bHeavy ? (bGrained ? 0.65 : HeavyHeart + (1.0 - HeavyHeart) * 0.5 * Root) : 0.5;
				if (bHeavy && !bGrained && bTip && bTipHeart)
				{
					B.Heart = HeavyHeart + (TipHeart - HeavyHeart) * OnTip; // (a soft end)
				}
				if (bHeavy && !bGrained && bHeartEnd && F >= RootSolid)
				{
					B.HeartEnd = std::min(B.Heart, HeartEndIs);
				}
				B.Inner = bHeavy && !bGrained ? HeavyInner + (TipInner - HeavyInner) * OnTip : 0.5;
				B.Rim = 0.0; // (soft edges everywhere: no jet has a clean blade's side)
				B.Rough = bHeavy ? EdgeRough + (TipRough - EdgeRough) * OnTip : FillRough; // (lumpy outlines, most torn at the tip)
				B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
				InFlash(B, 1.0, bHeavy ? 1.0 : 0.0);
				Bits.push_back(B);
				if (Root > 0.0 && D.U() < ClumpChance * Few)
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
					if (ThrowIs.Lighter > 1.0)
					{
						Lump.Col = Lump.Col * ThrowIs.Lighter; // (as light as the jet it bulges from: burst.throw.lighter)
					}
					Lump.Heart = 0.95;
					Lump.Rim = ClumpRim;
					Lump.Rough = 0.4;
					Lump.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
					Lump.ShadeK = 2.0;
					Bits.push_back(Lump);
				}
				// (stepped by the length it is drawn; never less than a hundredth, so no number in the recipe can stall it)
				F += std::max(0.01, Rl * std::max(1.0, 1.0 + ((bHeavy ? HeavyAlong : Along0) - 1.0) * P.Grain) * Over / std::max(ReachJ, 1e-3) * D.U(0.7, 1.2)) * JetCoarse;
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
		const double Busy = HalvePast > 0.0 && static_cast<double>(RoomGate()) > HalvePast ? 0.5 : 1.0; // (a busy screen: half the many small things)
		const int NSpikeAll = (ClodK > 0.02 || ShowerK > 0.02) && Has(Sp_ + "count.0") ? static_cast<int>(D.Whole(static_cast<int>(Num(Sp_ + "count.0", 24.0)), static_cast<int>(Num(Sp_ + "count.1", 40.0))) * Busy) : 0;
		const int NSpike = bFew ? std::min(NSpikeAll, std::max(std::min(NSpikeAll, 2), static_cast<int>(NSpikeAll * Few + 0.5))) : NSpikeAll; // (burst.far: Few of them)
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
			Vec Dir = (Vec(std::cos(Az) * std::sin(Radians(Lean)), std::sin(Az) * std::sin(Radians(Lean)), std::cos(Radians(Lean))) + Tilt).Unit();
			double ReachS = Reaches[static_cast<size_t>(S)];
			if (CrownLow != 1.0 || CrownWide != 1.0)
			{
				Squash(Dir, ReachS);
			}
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
				InFlash(B, 1.0);
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
		const int NBodyAll = static_cast<int>(Num(Path("body.puffs"), 50.0) * std::max(0.7, std::sqrt(K)));
		const int NBody = bFew ? std::min(NBodyAll, std::max(6, static_cast<int>(NBodyAll * FewRoot + 0.5))) : NBodyAll; // (burst.far: the square root of Few, each LumpCoarse bigger)
		const double BodyWide = Num(Path("body.width"), 0.3), BodyHigh = Num(Path("body.height"), 0.22);
		const Span BodyR = Range(Path("body.puff_r"), 0.06, 0.11), BodyAlpha = Range(Path("body.alpha"), 0.85, 0.95);
		const double BodyFrom = Num(Path("body.from_frame"), 3.0) / 30.0;
		const double BodyRim = Num(Path("body.rim"), 0.1), BodyRough = Num(Path("body.rough"), 0.0);
		// (with the dark foot on, the body is near-black too for its first second, settling to brown-grey)
		for (int I = 0; I < NBody; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			const double Out_ = CrownH * BodyWide * std::pow(D.U(), 0.6) * CrownWide;
			const double Up_ = CrownH * BodyHigh * std::pow(D.U(), 1.3) * CrownLow;
			const Vec To = Vec(std::cos(A) * Out_, std::sin(A) * Out_, Up_) + Tilt * Up_;
			const double Rr = CrownH * BodyR.Pick(D) * LumpCoarse;
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
			InFlash(B, 1.0);
			Bits.push_back(B);
		}
		// the foot: a dense mass of near-black lumps at the crown's base for the first second, settling to brown-grey
		// (art director 03:13Z, from Brian's battle clips: the burst itself has to carry the black, no grade can)
		if (bDarkFoot)
		{
			const int NFootAll = static_cast<int>(Pick(D, Ft + "count", 70.0, 100.0) * std::max(0.7, std::sqrt(K)));
			const int NFoot = bFew ? std::min(NFootAll, std::max(6, static_cast<int>(NFootAll * FewRoot + 0.5))) : NFootAll; // (burst.far: as the body)
			const double FootWide = Num(Ft + "width", 0.25), FootHigh = Num(Ft + "height", 0.22);
			const Span FootRs = Range(Ft + "lump_r", 0.025, 0.07), FootLife = Range(Ft + "life_s", 1.2, 1.8);
			const double FootCool = Num(Ft + "settle", 0.75), FootRough = Num(Ft + "rough", 0.45), FootShadeK = Num(Ft + "light", 1.2);
			const double FootRim = SafeNum(Num(Ft + "rim", 0.3), 0.0, 1.0, 0.3); // (how much of a foot piece is left at its very edge)
			for (int I = 0; I < NFoot; ++I)
			{
				const double A = D.U(0.0, 2.0 * Pi);
				const double Out_ = CrownH * FootWide * std::pow(D.U(), 0.7) * CrownWide;
				const double Up_ = CrownH * FootHigh * std::pow(D.U(), 1.5) * CrownLow;
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
				const double Rr = CrownH * FootRs.Pick(D) * LumpCoarse;
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
				B.Rim = FootRim;
				B.Rough = FootRough;
				B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
				B.ShadeK = FootShadeK; // (a lit top and a dark underside, so the mass has form)
				InFlash(B, 1.0);
				Bits.push_back(B);
			}
		}
		// the clods: those at the jets' tips, and more thrown clear low and wide
		const int NMore = static_cast<int>(ClodK * 50.0 * std::max(0.5, K * K));
		const double ClodLo = Num("burst.debris.clod_r_m.0", 0.015), ClodHi = Num("burst.debris.clod_r_m.1", 0.09);
		for (int I = 0; I < NMore; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			// (speeds: a squashed fan's lumps rise CrownLow as high and land CrownWide as far)
			const double Out_ = D.U(1.0, 6.0) * K * CrownWide / std::sqrt(CrownLow);
			const double Up_ = (2.0 + 9.0 * std::pow(D.U(), 2.0)) * K * std::sqrt(CrownLow);
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
					const double Out_ = OutS.Pick(D) * SpeedK * CrownWide / std::sqrt(CrownLow);
					const double Up_ = UpS.Pick(D) * SpeedK * std::sqrt(CrownLow);
					Launch.push_back(Vec(std::cos(A) * Out_, std::sin(A) * Out_, Up_));
				}
				LaunchR.push_back((Rs.A + (Rs.B - Rs.A) * std::pow(D.U(), Skew)) * K);
			}
		}
		const size_t ShowerTo = Launch.size();
		const Span ShowerShade = GradeRange(Sh + "shade", 0.55, 0.9);
		const Span ShowerRest = Range(Sh + "rest_s", 1.0, 2.5);
		const double ShowerPuffR = Num(Sh + "landing_puff_r_m", 0.08);
		// (a lump thrown past the lens lands beside the eye, where a dust puff fills a sixth of the screen as a flat slab:
		// art director 03:40Z. The recipe can leave that puff out)
		const bool bLensPuff = Num("burst.debris.lens.landing_puff", 1.0) > 0.5;
		// (and it is a dark torn lump, never a flat pale slab: broken edges and earth's shade)
		const bool bLensRough = Has("burst.debris.lens.rough.0"), bLensShade = Has("burst.debris.lens.shade.0");
		const Span LensRough = Range("burst.debris.lens.rough", 0.6, 0.9), LensShade = Range("burst.debris.lens.shade", 0.5, 0.8);
		// (art director 14:09Z: close to the eye a lens piece's hard rim and the straight sides between its ring's points
		// read as a cut card. lens.rim: how much of it is left at its very edge, so its dark heart fades out softly)
		const bool bLensRim = Has("burst.debris.lens.rim");
		const double LensRim = SafeNum(Num("burst.debris.lens.rim", 0.6), 0.0, 1.0, 0.6), LensInner = SafeNum(Num("burst.debris.lens.inner", 0.5), 0.0, 0.95, 0.5); // (without the key, a piece's own 0.5: the pre-check on 5dc2e510)
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
		// (art director 19:58Z 21 November: the clods fall back within 2 to 3 s, a few trailing thin dust. With
		// sizes.<size>.clods_back_s every clod's throw is as much slower as brings the highest of them down by then, so
		// taller jets throw no clod for longer; with burst.debris.dust_trails that many of the bigger ones leave a thin
		// line of dust behind them, by dice of their own. Neither key: as before)
		const double ClodsBack = SafeNum(Num("sizes." + Size + ".clods_back_s", 0.0), 0.0, 20.0, 0.0);
		double ClodSlower = 1.0;
		if (ClodsBack > 0.0)
		{
			double Highest = 0.0;
			for (size_t I = 0; I < ShowerTo && I < Launch.size(); ++I)
			{
				Highest = std::max(Highest, Launch[I].Z);
			}
			ClodSlower = Highest > 4.9 * ClodsBack ? 4.9 * ClodsBack / Highest : 1.0;
		}
		const bool bDustTrails = Has("burst.debris.dust_trails.0");
		Dice DT(PlaceKey(At) * 0xD6E8FEB86659FD93ull ^ 0x7A11ull);
		int DustTrails = bDustTrails ? DT.Whole(static_cast<int>(SafeNum(Num("burst.debris.dust_trails.0", 3.0), 0.0, 12.0, 3.0)), static_cast<int>(SafeNum(Num("burst.debris.dust_trails.1", 5.0), 0.0, 12.0, 5.0))) : 0;
		const double TrailFrom = SafeNum(Num("burst.debris.dust_trail_from_m", 0.08), 0.0, 5.0, 0.08) * K;
		Dice Thin(PlaceKey(At) * 0x9FB21C651E98DF25ull ^ 0xFE3ull); // (burst.far: which clods are thrown, by dice of their own)
		// (grounds.<ground>.wet, its own switch on: the art director's read of the thaw (23 November): thrown mud is dark
		// wet clods that land and stay, never a dust puff. The clods and the shower take the wet colour (clod, by
		// tint; the shower by shower_shade) and the clods r_k as big; none bounces, none sheds dust in flight, none
		// puffs dust where it lands; and of a burst within stay_within_m of the eye, up to stay_most clods of
		// stay_from_m or more lie stay_s where they land, fading over the last fade_s. Dice of their own; off, as before)
		const std::string Wt = "grounds." + CrownGround + ".wet.";
		const bool bWet = !CrownGround.empty() && Num(Wt + "on", 0.0) >= 0.5;
		Dice DW(PlaceKey(At) * 0xE7037ED1A0B428DBull ^ 0x3E7ull);
		const Rgb WetClod = bWet ? Picture(Wt + "clod", P.Clod) : P.Clod;
		const Span WetTint = GradeRange(Wt + "tint", 0.9, 1.05), WetShower = GradeRange(Wt + "shower_shade", 0.7, 0.95), WetStay = Range(Wt + "stay_s", 20.0, 35.0);
		const double WetBig = SafeNum(Num(Wt + "r_k", 1.0), 0.2, 4.0, 1.0), WetFade = SafeNum(Num(Wt + "fade_s", 5.0), 0.1, 120.0, 5.0);
		const double WetFrom = SafeNum(Num(Wt + "stay_from_m", 0.04), 0.0, 5.0, 0.04);
		const int WetMost = static_cast<int>(SafeNum(Num(Wt + "stay_most", 24.0), 0.0, 500.0, 24.0));
		const bool bWetNear = bWet && bEyeKnown && (LastEye - At).Length() < SafeNum(Num(Wt + "stay_within_m", 150.0), 0.0, 1.0e6, 150.0);
		int WetStays = 0;
		// (art director 01:19Z: wet mud flies as lumps, few slivers, and has no shine. round_share of the wet clods are drawn
		// no longer than round_long times their width, their ends blunt; every one is lit shade_k as strongly, its lit side
		// darker. Dice of their own, so the colours and the stays fall as they did)
		Dice DR(PlaceKey(At) * 0xC2B2AE3D27D4EB4Full ^ 0x40Dull);
		const double WetShadeK = SafeNum(Num(Wt + "shade_k", 1.0), 0.0, 2.0, 1.0), WetRound = SafeNum(Num(Wt + "round_share", 0.0), 0.0, 1.0, 0.0);
		const double WetRoundLong = std::max(1.0, SafeNum(Num(Wt + "round_long", 1.3), 1.0, 10.0, 1.3));
		for (size_t I = 0; I < Launch.size(); ++I)
		{
			if (bFew && Thin.U() >= Few)
			{
				continue; // (burst.far: Few of the clods, the shower and the lens's pieces, as they were)
			}
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.bThrown = true;
			B.Born = T0 + 0.015;
			B.From = At + ZUp * (0.15 * K);
			B.Vel = I < ShowerTo && ClodSlower < 1.0 ? Launch[I] * ClodSlower : Launch[I];
			B.Gravity = 9.8;
			B.R0 = B.R1 = LaunchR[I];
			const bool bShower = I >= ShowerFrom && I < ShowerTo;
			B.Col = (P.bSplinter && D.U() < 0.35 ? P.Splinter : (bShower ? P.Stone : P.Clod)) * (bShower ? ShowerShade.Pick(D) : ClodTint.Pick(D));
			if (bWet && I < ShowerTo)
			{
				B.Col = WetClod * (bShower ? WetShower.Pick(DW) : WetTint.Pick(DW));
				if (!bShower)
				{
					B.R0 = B.R1 = LaunchR[I] * WetBig;
				}
			}
			B.Col2 = B.Col;
			B.Alpha = 1.0;
			B.FadeIn = 0.005;
			B.Heart = 0.95;
			B.Rim = 0.6;
			B.LeastPx = ClodLeast;
			Chunk(B, D);
			if (bWet && I < ShowerTo)
			{
				B.ShadeK *= WetShadeK;
				if (DR.U() < WetRound)
				{
					B.MostLong = WetRoundLong;
					B.Point = 0.0;
					B.Tail = 0.0;
				}
			}
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
				if (bLensRim)
				{
					B.Rim = LensRim;
					B.Inner = LensInner;
				}
			}
			Throw(B);
			if (bWet)
			{
				B.Vel2 = Vec(); // (wet mud sticks where it lands: no bounce)
				B.Land2 = B.Land;
				B.At2 = B.At1;
			}
			B.Life = B.Land2 + (bShower ? ShowerRest.Pick(D) : D.U(2.0, 4.0));
			B.Hold = std::min(0.95, (B.Land2 + 1.0) / B.Life);
			if (bWetNear && I < ShowerFrom && B.R0 >= WetFrom && WetStays < WetMost)
			{
				++WetStays;
				B.Life = B.Land2 + WetStay.Pick(DW);
				B.Hold = std::min(0.99, std::max(B.Hold, (B.Life - WetFade) / B.Life));
			}
			const Vec Landed = B.At1;
			const double Down = B.Born + B.Land;
			const double R = B.R0;
			if (DustTrails > 0 && !bWet && I < ShowerTo && R > TrailFrom && B.Land > 0.6)
			{
				// (thin dust shed along its flight, each puff where the clod was at that moment)
				--DustTrails;
				const int Shed = 6;
				for (int Q = 0; Q < Shed; ++Q)
				{
					const double At_ = std::min(B.Land, 2.5) * (Q + DT.U(0.2, 0.8)) / Shed;
					Bit Wisp;
					Wisp.Born = B.Born + At_;
					Wisp.From = B.From + B.Vel * At_ + Vec(0.0, 0.0, -0.5 * B.Gravity * At_ * At_);
					Wisp.Vel = (B.Vel + Vec(0.0, 0.0, -B.Gravity * At_)) * 0.12;
					Wisp.Drag = 3.0;
					Wisp.Windage = 0.5;
					Wisp.Life = DT.U(0.7, 1.2);
					Wisp.R0 = R * 1.6;
					Wisp.R1 = R * DT.U(4.0, 6.5);
					Wisp.Col = P.Dust.Mix(P.Soil, 0.4);
					Wisp.Col2 = P.Dust;
					Wisp.Alpha = DT.U(0.22, 0.34) * (1.0 - 0.5 * Q / Shed);
					Wisp.FadeIn = 0.03;
					Wisp.Hold = 0.15;
					Wisp.Heart = 0.5;
					Bits.push_back(Wisp);
				}
			}
			Bits.push_back(B);
			if (!bWet && R > (bShower ? ShowerPuffR * K : 0.04) && (bLensPuff || I < ShowerTo))
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
		// the chips (grounds.<ground>.chips, the art director's 20:25Z read of rock): stone splinters flung low, flat and
		// fast out to out_m, bigger than the shower's lumps, each one's fresh face catching the sun as it turns (Glint).
		// skitter of them hop on along the ground when they land, keeping skitter_keep of their speed, a little dust kicked
		// up at each landing: the rock's own mark, seen from the general's height too. Dice of their own; burst.far, Few
		const std::string Ch = "grounds." + CrownGround + ".chips.";
		if (!CrownGround.empty() && Has(Ch + "count.0"))
		{
			Dice DC(PlaceKey(At) * 0xA24BAED4963EE407ull ^ 0xC41Bull);
			const int NAll = static_cast<int>(SafeNum(Range(Ch + "count", 24.0, 36.0).Pick(DC), 0.0, 200.0, 30.0) * std::max(0.5, std::min(K * K, 3.0)) * Busy);
			const int NChips = bFew ? std::min(NAll, std::max(std::min(NAll, 2), static_cast<int>(NAll * Few + 0.5))) : NAll;
			const Span ChipR = Range(Ch + "r_m", 0.05, 0.125), ChipOut = Range(Ch + "out_m", 30.0, 60.0), ChipUp = Range(Ch + "up_deg", 3.0, 12.0);
			const Span Keep = Range(Ch + "skitter_keep", 0.4, 0.6), ChipPuff = Range(Ch + "puff_r_m", 0.25, 0.6), ChipRest = Range(Ch + "rest_s", 1.5, 3.0);
			const double ChipGlint = SafeNum(Num(Ch + "glint", 1.5), 0.0, 8.0, 1.5), Skitter = Clamp01(Num(Ch + "skitter", 0.3));
			const double OutK = std::sqrt(K);
			for (int I = 0; I < NChips; ++I)
			{
				const double A = DC.U(0.0, 2.0 * Pi);
				const double E = Radians(std::clamp(ChipUp.Pick(DC), 0.5, 60.0));
				const double Far = std::clamp(ChipOut.Pick(DC), 1.0, 300.0) * OutK;
				const double V = std::sqrt(9.8 * Far / std::max(std::sin(2.0 * E), 1e-3)); // (to land that far on level ground)
				Bit B;
				B.Form = Bit::Shape::Streak;
				B.bThrown = true;
				B.bChip = true;
				B.Born = T0 + 0.01 + DC.U(0.0, 0.03);
				B.From = At + ZUp * (0.2 * K);
				B.Vel = Vec(std::cos(A) * std::cos(E), std::sin(A) * std::cos(E), std::sin(E)) * V;
				B.Gravity = 9.8;
				B.R0 = B.R1 = std::clamp(ChipR.Pick(DC), 0.01, 1.0);
				B.Col = P.Splinter * DC.U(0.9, 1.08);
				B.Col2 = B.Col;
				B.Alpha = 1.0;
				B.FadeIn = 0.005;
				B.Heart = 0.95;
				B.Rim = 0.6;
				B.LeastPx = ClodLeast;
				Chunk(B, DC);
				B.Glint = ChipGlint;
				Throw(B);
				const bool bSkit = DC.U() < Skitter;
				if (bSkit)
				{
					// (its own hop, low and long: Throw's bounce is a clod's, which stops short)
					const Vec Down = B.Vel + Vec(0.0, 0.0, -B.Gravity * B.Land);
					const double KeepIs = Keep.Pick(DC);
					B.Vel2 = Vec(Down.X * KeepIs, Down.Y * KeepIs, std::fabs(Down.Z) * 0.3);
					double T2 = 0.02;
					for (; T2 < 4.0; T2 += 0.01)
					{
						const Vec Q = B.At1 + B.Vel2 * T2 + Vec(0.0, 0.0, -0.5 * B.Gravity * T2 * T2);
						if (B.Vel2.Z - B.Gravity * T2 < 0.0 && Q.Z <= Height(Q.X, Q.Y) + B.R0 * 0.5)
						{
							break;
						}
					}
					B.Land2 = B.Land + T2;
					B.At2 = B.At1 + B.Vel2 * T2 + Vec(0.0, 0.0, -0.5 * B.Gravity * T2 * T2);
					B.At2.Z = Height(B.At2.X, B.At2.Y) + B.R0 * 0.5;
				}
				else
				{
					B.Vel2 = Vec();
					B.Land2 = B.Land;
					B.At2 = B.At1;
				}
				B.Life = B.Land2 + ChipRest.Pick(DC);
				B.Hold = std::min(0.95, (B.Land2 + 1.0) / B.Life);
				Bits.push_back(B);
				if (bSkit)
				{
					// (a little dust where it strikes and where it comes down again)
					for (int L = 0; L < 2; ++L)
					{
						Bit Puff;
						Puff.Born = B.Born + (L == 0 ? B.Land : B.Land2);
						Puff.From = L == 0 ? B.At1 : B.At2;
						Puff.Vel = Vec(B.Vel2.X, B.Vel2.Y, 0.0) * 0.08 + Vec(0.0, 0.0, 0.5);
						Puff.Drag = 2.0;
						Puff.Windage = 0.5;
						Puff.Life = DC.U(0.8, 1.4);
						const double Pr = ChipPuff.Pick(DC) * (L == 0 ? 1.0 : 0.7);
						Puff.R0 = Pr * 0.35;
						Puff.R1 = Pr;
						Puff.Col = P.Dust;
						Puff.Col2 = P.Dust;
						Puff.Alpha = L == 0 ? 0.5 : 0.35;
						Puff.FadeIn = 0.03;
						Puff.Hold = 0.1;
						Bits.push_back(Puff);
					}
				}
			}
		}
	}

	double Battle::Thicker(double Alpha, double Bigger) const
	{
		// (burst.far.alpha_pow: a see-through piece Bigger times as wide, where fewer of them overlap, is as thick as
		// Bigger to alpha_pow of them laid one over another, never thinner than it was and never over alpha_most)
		if (!(FewAlphaPow > 0.0) || !(Bigger > 1.0) || !(Alpha > 0.0) || Alpha >= LandAlphaMost)
		{
			return Alpha;
		}
		const double Layers = std::pow(std::min(Bigger, 4.0), FewAlphaPow);
		return std::max(Alpha, std::min(LandAlphaMost, 1.0 - std::pow(1.0 - Clamp01(Alpha), Layers)));
	}

	void Battle::Smoke(double T0, const Vec& At, double CrownH, double K, double Scale, const std::string& Size, const std::string& Ground, const Paint& P, double DustK, Dice& D, double Many)
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
		const int NNear = static_cast<int>(130.0 * std::pow(std::max(0.4, Scale), 0.85));
		// (Many under 1, a sea battle's hit far beyond the air's distance: that share of the puffs, never under 8, each
		// as much bigger as keeps the cloud as full. Shell says how far)
		const bool bFew = Many < 1.0;
		// (a burst with bulk, where the recipe has burst.bulk and the size its cloud_m: Bulk lays the cloud, and none
		// of the puffs below are laid. Never on water)
		const double CloudM = Ground == "water" ? 0.0 : SafeNum(Num("sizes." + Size + ".cloud_m", 0.0), 0.0, 400.0, 0.0);
		const bool bBulk = CloudM > 0.0 && Has("burst.bulk.column.puffs");
		const int N = bBulk ? 0 : bFew ? std::min(NNear, std::max(8, static_cast<int>(NNear * std::max(Many, 0.0)))) : NNear;
		const double Bigger = bFew && N > 0 ? std::min(4.0, std::sqrt(static_cast<double>(NNear) / N)) : 1.0;
		const Rgb Warm = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Vec Lean = Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), 0.0) * 0.15;
		const std::string Sm = "burst.smoke.";
		const Rgb Top = P.bSmokeTop ? P.SmokeTop : Picture("grounds." + Ground + ".smoke_top", Picture(Sm + "colour_top", Rgb::FromPicture(66, 64, 62))); // (a water spout's mist goes pale, never charcoal; smoke_of_ground: the ground's own)
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
		// (the cloud goes slowly, by its size's own times: linger.shell; spray falls back as it did)
		// (the pre-check on 86d71f2: a bomb's smoke lived longer than its cloud's gone_s, 112 s against 60 for a 500 lb on
		// earth, and the rule cut it short. Brian asked for smoke that does not go right away: a cloud is gone no sooner
		// than its longest puff was before the rule, and goes by the same curve over that longer time)
		LingerRow StaysOn = LingerOf("shell", Ground == "water" ? std::string("water") : Size);
		StaysOn.GoneS = std::max(StaysOn.GoneS, SafeNum(Linger, 0.0, 3600.0, 0.0));
		const double StaysSpare = SpareAtBurst; // (one for the whole cloud)
		if (bBulk)
		{
			Bulk(T0, At, CrownH, K, Size, Ground, P, CloudM, Linger, D, Many);
		}
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
			if (bFew)
			{
				B.R0 *= Bigger;
				B.R1 *= Bigger;
			}
			B.Col = (F < 0.25 && Born - T0 < 0.22) ? Base.Mix(Warm, 0.35) : Base; // (warm only at its foot, at first)
			B.Col2 = Base;
			B.Cool = B.Life / 0.15;
			if (bDarkFoot && F < FootBand)
			{
				B.Col = B.Col * FootSmokeDark; // (dark at the foot for its first second, settling to the cloud's own colour)
				B.Cool = B.Life / FootSettle;
			}
			B.Alpha = Thicker(SmokeAlpha.Pick(D) * AlphaK, Bigger);
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
			Lingering(B, StaysOn, T0, At, 0.75, CrownH, StaysSpare);
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

	void Battle::Bulk(double T0, const Vec& At, double CrownH, double K, const std::string& Size, const std::string& Ground, const Paint& P, double CloudM, double LooksS, Dice& D, double Many)
	{
		// A burst with bulk. Art director 19:58Z 21 November, for Brian's "the explosions should be bigger and more
		// realistic": more bulk, a denser core and a longer hang, never a ball of soft smoke lifting off the ground.
		// The cloud is a column of the ground's own dark earth standing on the hit, many puffs at every height from
		// its foot, dense enough to hide what is behind it through its first two seconds; and round it the lighter
		// grey-brown smoke and dust, widest at the ground, sizes.<size>.cloud_m across by about two seconds. It climbs
		// from its foot: every puff starts at the hit and goes out to its place, the higher the later; and it stays
		// on the ground: a puff rises on by how high in the cloud it stands, the foot not at all. Going slowly
		// (Lingering, by the size's own times) the puffs at the foot are the likelier to stay and take the smoke of
		// the rest (linger.shell.stay_low), and a puff climbs by its height in the cloud, so the base holds on the
		// ground while the smoke above lifts, and the base thins last.
		const std::string Q = "burst.bulk.", C = Q + "column.", Fr = Q + "fringe.";
		const double Slow = SafeNum(Num("grounds." + Ground + ".crown_slow", 1.0), 0.2, 5.0, 1.0);
		// (a cloud this dense has its own smoke-left numbers, bulk_left_third and bulk_left_two_thirds, set in pictures
		// as the row's are: many thick puffs one behind another hold their look far longer than a thin cloud's do)
		LingerRow StaysOn = LingerOf("shell", Size);
		StaysOn.Third = StaysOn.BulkThird;
		StaysOn.TwoThirds = StaysOn.BulkTwoThirds;
		StaysOn.GoneS = std::max(StaysOn.GoneS, SafeNum(LooksS, 0.0, 3600.0, 0.0)); // (gone no sooner than its smoke was before the rule: see Smoke)
		const double StaysSpare = SpareAtBurst; // (one for the whole cloud)
		const double StayLow = Clamp01(StaysOn.StayLow);
		// (a bigger burst has more puffs, each a smaller share of its cloud, so its cloud is as full and no puff a great flat disc)
		const double KSafe = SafeNum(K, 0.05, 6.0, 1.0);
		// (sizes.<size>.cloud_many: that many times the puffs for this size alone, a bomb's tall cloud)
		// (a smaller burst has fewer still: a mortar's or a grenade's cloud needs no more than it had, and a barrage of them fills the room)
		const double KFull = std::clamp(std::pow(KSafe, KSafe < 1.0 ? 0.9 : 0.6), 0.4, 2.2);
		const double KMany = KFull * SafeNum(Num("sizes." + Size + ".cloud_many", 1.0), 0.25, 3.0, 1.0), KSmall = 1.0 / std::sqrt(KFull);
		// (Many under 1, a sea battle's hit far beyond the air's distance: that share of the puffs, never under 6, each as much bigger)
		const bool bFew = Many < 1.0;
		auto Counted = [&](double Asked, double& Bigger)
		{
			const int Near = std::max(1, static_cast<int>(SafeNum(Asked, 0.0, 400.0, 30.0) * KMany));
			const int Laid = bFew ? std::min(Near, std::max(6, static_cast<int>(Near * std::max(Many, 0.0)))) : Near;
			Bigger = bFew ? std::min(4.0, std::sqrt(static_cast<double>(Near) / Laid)) : 1.0;
			return Laid;
		};
		const Vec Tilt = Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), 0.0) * 0.1; // (the whole cloud leans a little one way)
		const double SqrtK = std::sqrt(KSafe);
		// (a puff's size is its share of the cloud as it stands, as wide as cloud_m and as tall as its crown: a bomb's
		// cloud is taller for its width than a shell's, and puffs sized by its width alone left it a stack of spots.
		// puff_of: the crown's height over cloud_m that the recipe's puff_r were set by, a 105's)
		const double PuffOf = SafeNum(Num(Q + "puff_of", 0.75), 0.1, 4.0, 0.75);
		const double PuffM = std::clamp(std::sqrt(CloudM * std::max(CrownH, 1e-3) / PuffOf), 0.7 * CloudM, 1.6 * CloudM);
		const Rgb Settled = P.Smoke.Mix(P.Dust, SafeNum(Num("burst.foot.settle_dust", 0.3), 0.0, 1.0, 0.3));
		const double CloudShade = SafeNum(Num("burst.smoke.cloud_shade", 1.0), 0.0, 8.0, 1.0);
		// (every number of the column and the fringe is held as it is read or picked: one that is no number draws the usual)
		// the column
		double StackBigger = 1.0;
		const int NStack = Counted(Num(C + "puffs", 34.0), StackBigger);
		const double CoreWide = SafeNum(Num(C + "wide", 0.36), 0.05, 1.0, 0.36) * CloudM; // (across, at the ground)
		const double CoreTop = SafeNum(Num(C + "top_wide", 0.7), 0.2, 2.0, 0.7); // (and at its top, as a share of that)
		const double CoreTall = SafeNum(Num(C + "tall", 0.7), 0.1, 2.0, 0.7) * CrownH;
		const Span CoreR = Range(C + "puff_r", 0.085, 0.13), CoreAlpha = Range(C + "alpha", 0.92, 1.0), CoreLife = Range(C + "life_s", 3.0, 4.5), CoreShade = Range(C + "shade", 0.85, 1.15);
		const double ClimbS = SafeNum(Num(C + "climb_s", 0.8), 0.1, 5.0, 0.8); // (how long its top takes to stand)
		const double CoreRise = SafeNum(Num(C + "rise_mps", 0.8), 0.0, 10.0, 0.8);
		const double CoreSoil = SafeNum(Num(C + "soil", 0.5), 0.0, 1.0, 0.5), CoreSettle = SafeNum(Num(C + "settle", 0.5), 0.0, 1.0, 0.5);
		const double CoreRough = SafeNum(Num(C + "rough", 0.3), 0.0, 1.0, 0.3), CoreHeart = SafeNum(Num(C + "heart", 0.9), 0.0, 1.0, 0.9), CoreInner = SafeNum(Num(C + "inner", 0.62), 0.1, 0.95, 0.62);
		const double CoreLit = SafeNum(Num(C + "light", 1.0), 0.0, 4.0, 1.0);
		const double CoreHang = SafeNum(Num(C + "hang", 0.35), 0.0, 1.0, 0.35);
		// (life_thick: a column puff that does not hang lasts that many of its size's thick times, never under life_least_s:
		// a grenade's earth is down sooner than a bomb's. Without it, or for a size without times, life_s)
		const bool bLifeByThick = Has(C + "life_thick.0") && StaysOn.bOn && StaysOn.ThickS > 0.0;
		const Span CoreLifeOf = Range(C + "life_thick", 1.125, 1.625);
		const double CoreLifeLeast = SafeNum(Num(C + "life_least_s", 2.0), 0.2, 60.0, 2.0);
		for (int I = 0; I < NStack; ++I)
		{
			const double F = std::pow(D.U(), 1.25); // (0 at the foot, 1 at the top: more of them low)
			const double Rr = PuffM * SafeNum(CoreR.Pick(D), 0.005, 0.6, 0.08) * KSmall * StackBigger;
			const double Half = 0.5 * CoreWide * (1.0 + (CoreTop - 1.0) * F);
			const double Az = D.U(0.0, 2.0 * Pi), Out_ = std::max(0.0, Half - 0.7 * Rr) * std::sqrt(D.U());
			const double Up_ = std::max(CoreTall * F, 0.55 * Rr);
			const Vec To = Vec(std::cos(Az) * Out_, std::sin(Az) * Out_, Up_) + Tilt * Up_;
			const double KB = 3.0 / std::max(ClimbS * (0.35 + 0.65 * F), 0.05); // (the foot stands at once, the top by climb_s)
			Bit B;
			B.Born = T0 + 0.03 + 0.25 * ClimbS * F * D.U();
			B.From = At + ZUp * (0.2 * K);
			B.Vel = To * KB;
			B.Drag = KB;
			B.Rise = CoreRise * F * D.U(0.6, 1.2) * SqrtK;
			B.Windage = 0.5 + 0.4 * F;
			B.Life = (bLifeByThick ? SafeNum(CoreLifeOf.Pick(D) * StaysOn.ThickS, CoreLifeLeast, 120.0, CoreLifeLeast) : SafeNum(CoreLife.Pick(D), 0.2, 120.0, 4.0)) * Slow;
			B.R0 = Rr * 0.4;
			B.R1 = Rr;
			B.Grow = 0.4;
			B.Col = P.Heavy.Mix(P.Soil, CoreSoil * D.U()) * SafeNum(CoreShade.Pick(D), 0.0, 4.0, 0.85);
			B.Col2 = B.Col.Mix(Settled, CoreSettle);
			B.Cool = 1.0;
			B.Alpha = SafeNum(CoreAlpha.Pick(D), 0.0, 1.0, 0.95);
			B.FadeIn = 0.04;
			B.Hold = 0.6;
			B.Heart = CoreHeart;
			B.Inner = CoreInner;
			B.Rim = 0.0;
			B.Rough = CoreRough;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.ShadeK = CoreLit;
			B.CloudShade = CloudShade;
			B.Centre = At + (Tilt + ZUp) * (0.45 * CoreTall);
			B.CloudR = std::max(0.5 * CoreWide, 1e-3);
			B.CentreRise = 0.3 * CoreRise * SqrtK;
			// (column.hang of the column's puffs hang on with the smoke, the likelier the lower, each to the cloud's end; the
			// rest are earth and fall out of the air by their own life_s)
			if (D.U() < CoreHang * (1.0 + StayLow * (0.44 - F) / 0.44))
			{
				Lingering(B, StaysOn, T0, At, 0.7, 0.0, StaysSpare, 1.0 / std::max(StaysOn.Keep, 0.05));
			}
			if (B.bTail)
			{
				B.TailLift *= F; // (the foot stays on the ground)
			}
			InFlash(B, 1.0, 0.0);
			Bits.push_back(B);
		}
		// the lighter smoke and dust round it
		double FringeBigger = 1.0;
		const int NFringe = Counted(Num(Fr + "puffs", 80.0), FringeBigger);
		const double FrTall = SafeNum(Num(Fr + "tall", 0.95), 0.1, 3.0, 0.95) * CrownH;
		const double FrTaper = SafeNum(Num(Fr + "taper", 0.55), 0.0, 0.95, 0.55); // (how much narrower at its top than at the ground)
		const Span FrR = Range(Fr + "puff_r", 0.07, 0.12), FrAlpha = Range(Fr + "alpha", 0.32, 0.5);
		// (sizes.<size>.cloud_puff: this size's smoke puffs that many times as big. A bomb's cloud has so many, each so small a
		// share of it, that its edge was a field of spots)
		const double FrPuffK = SafeNum(Num("sizes." + Size + ".cloud_puff", 1.0), 0.3, 3.0, 1.0);
		const double RollS = SafeNum(Num(Fr + "roll_s", 1.2), 0.1, 6.0, 1.2); // (how long it takes to roll out to its width)
		const double FrGrow = SafeNum(Num(Fr + "grow", Num("burst.smoke.grow", 0.3)), 0.05, 2.0, 0.3);
		const double FrTopBig = SafeNum(Num(Fr + "top_bigger", 0.3), 0.0, 2.0, 0.3);
		const double FrSmall = SafeNum(Num(Fr + "born_r", 0.35), 0.05, 1.0, 0.35);
		const Span FrRise = Range(Fr + "rise_mps", 0.25, 0.9);
		const double FrOutward = SafeNum(Num(Fr + "outward", 1.0), 0.2, 4.0, 1.0); // (over 1: more of it near the column, thinning out to its width)
		const Rgb Warm = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Rgb Top = P.bSmokeTop ? P.SmokeTop : Picture("grounds." + Ground + ".smoke_top", Picture("burst.smoke.colour_top", Rgb::FromPicture(66, 64, 62)));
		const double TopShare = SafeNum(Num(Fr + "top_share", Num("burst.smoke.top_share", 0.8)), 0.0, 1.0, 0.8);
		const Span FrDust = Range(Fr + "dust", 0.0, 0.0); // (how much of the ground's dust is in each puff's smoke: lighter grey-brown)
		const double FrLow = SafeNum(Num(Fr + "low", 1.3), 0.3, 4.0, 1.3); // (over 1: more of it low)
		const Span FrHeart = Range(Fr + "heart", 0.4, 0.7);
		const double FrCloudShade = SafeNum(Num(Fr + "cloud_shade", CloudShade), 0.0, 8.0, CloudShade); // (spread wide, its far side is not in the column's shadow as the column's own is)
		const double FrFrom = SafeNum(Num(Fr + "from_column", 0.0), 0.0, 1.5, 0.0); // (it begins that share of the column's own half width out: round the column, not inside it)
		const double FrRough = SafeNum(Num(Fr + "rough", 0.0), 0.0, 1.0, 0.0);
		// (art director 03:27Z 22 November: the lit top of the cloud at most 90. shade: its smoke that many times as
		// light as the ground's paints have it; light: how strongly each puff is lit on its sunward side, 1 as any puff)
		const double FrShade = SafeNum(Num(Fr + "shade", 1.0), 0.2, 2.0, 1.0), FrLight = SafeNum(Num(Fr + "light", 1.0), 0.0, 4.0, 1.0);
		// (keep_k: of the row's keep, the share of the fringe that stays to the end; go_by_k: how much sooner than the
		// row's go_by its others retire. Fewer pieces in a barrage's old smoke)
		const double FrKeep = SafeNum(Num(Fr + "keep_k", 1.0), 0.1, 2.0, 1.0);
		LingerRow FringeOn = StaysOn;
		FringeOn.GoBy = std::clamp(FringeOn.GoBy * SafeNum(Num(Fr + "go_by_k", 1.0), 0.1, 1.0, 1.0), 0.05, 1.0);
		const double FrReach = SafeNum(Num(Fr + "reach", 1.0), 0.3, 3.0, 1.0) * SafeNum(Num("sizes." + Size + ".cloud_reach", 1.0), 0.3, 3.0, 1.0); // (its outermost puffs' middles as a share of half cloud_m: set so the cloud MEASURES cloud_m across at two seconds)
		// (at the column's flank a puff is near_alpha thick, near_dark of the way to the column's own earth and its heart
		// near_heart; out at the cloud's edge it is the fringe's own: so the cloud goes from its core to its edge by degrees)
		const double FrNearAlpha = SafeNum(Num(Fr + "near_alpha", 0.0), 0.0, 1.0, 0.0), FrNearDark = SafeNum(Num(Fr + "near_dark", 0.0), 0.0, 1.0, 0.0), FrNearHeart = SafeNum(Num(Fr + "near_heart", 0.0), 0.0, 1.0, 0.0);
		const Rgb ColumnIs = P.Heavy.Mix(P.Soil, 0.5 * CoreSoil) * SafeNum(0.5 * (CoreShade.A + CoreShade.B), 0.0, 4.0, 0.85);
		const double AlphaK = Num("grounds." + Ground + ".smoke_alpha", 1.0);
		const bool bDarkFoot = Has("burst.foot.count.0");
		const double FootBand = Num("burst.foot.smoke_band", 0.45), FootSmokeDark = Num("burst.foot.smoke_dark", 0.45), FootSettle = std::max(0.1, Num("burst.foot.smoke_settle_s", 1.0));
		const double LooksFor = SafeNum(LooksS, 1.0, 600.0, 20.0);
		// (art director 01:19Z 23 November, a thaw: the cloud's low ring, warm at its foot at first, read at half a second
		// as dry earth. With grounds.<ground>.wet on, no puff of the cloud is warm, and its lowest, ring_below of the
		// cloud's height (a share of FrTall), are ring_colour, a dark grey-brown; a low wall on the ground as the skirt
		// is, ring_high_k as high as they stood; gone in ring_life_s, never lingering with the cloud. The rest of the
		// cloud as it was; dice of its own. Off: as before)
		const std::string Wt = "grounds." + Ground + ".wet.";
		const bool bWetRing = !Ground.empty() && Num(Wt + "on", 0.0) >= 0.5;
		Dice DWR(PlaceKey(At) * 0xD6E8FEB86659FD93ull ^ 0x41Bull);
		const double WetBelow = bWetRing ? SafeNum(Num(Wt + "ring_below", 0.1), 0.0, 1.0, 0.1) : 0.0;
		const Rgb WetRingCol = Picture(Wt + "ring_colour", P.Smoke);
		const double WetRingHigh = SafeNum(Num(Wt + "ring_high_k", 0.5), 0.05, 1.0, 0.5);
		const Span WetRingLife = Range(Wt + "ring_life_s", 0.8, 1.2);
		for (int I = 0; I < NFringe; ++I)
		{
			const double F = std::pow(D.U(), FrLow);
			const double Rr = PuffM * SafeNum(FrR.Pick(D), 0.005, 0.6, 0.1) * (1.0 + FrTopBig * F) * KSmall * FringeBigger * FrPuffK;
			const double Half = 0.5 * CloudM * FrReach * (1.0 - FrTaper * F);
			const double Up_ = std::max(FrTall * F, 0.55 * Rr);
			const double Within = Up_ < CoreTall ? FrFrom * 0.5 * CoreWide * (1.0 + (CoreTop - 1.0) * Up_ / std::max(CoreTall, 1e-3)) : 0.0;
			const double Far = std::pow(D.U(), 0.5 * FrOutward); // (0 at the column's flank, 1 at the cloud's edge)
			const double Az = D.U(0.0, 2.0 * Pi), Out_ = Within + std::max(0.0, Half - Within) * Far;
			const Vec To = Vec(std::cos(Az) * Out_, std::sin(Az) * Out_, Up_) + Tilt * Up_;
			const double KB = 3.0 / std::max(RollS * D.U(0.5, 1.0), 0.05);
			const double Shade = D.U(0.8, 1.2);
			// (warm brown near the ground, charcoal grey toward the top, as a shell's smoke ever was)
			const Rgb Own = (D.U() > 0.25 ? P.Smoke.Mix(P.Dust, SafeNum(FrDust.Pick(D), 0.0, 1.0, 0.0)) * Shade : P.Dust * (0.6 * Shade)).Mix(Top * Shade, std::min(1.0, F * 1.1) * TopShare);
			const Rgb Base = (Own * FrShade).Mix(ColumnIs * Shade, FrNearDark * (1.0 - Far));
			Bit B;
			B.Born = T0 + 0.08 + 0.3 * D.U() * (0.4 + F);
			B.From = At + ZUp * (0.2 * K);
			B.Vel = To * KB;
			B.Drag = KB;
			B.Rise = SafeNum(FrRise.Pick(D), 0.0, 10.0, 0.5) * SqrtK * F;
			B.Windage = D.U(0.5, 0.95);
			B.Life = LooksFor * D.U(0.45, 1.0);
			B.R0 = Rr * FrSmall;
			B.R1 = Rr;
			B.Grow = FrGrow;
			B.Col = (F < 0.25 && B.Born - T0 < 0.22 && !bWetRing) ? Base.Mix(Warm, 0.35) : Base; // (wet ground: never warm)
			B.Col2 = Base;
			B.Cool = B.Life / 0.15;
			if (bDarkFoot && F < FootBand)
			{
				B.Col = B.Col * FootSmokeDark; // (dark at the foot for its first second, settling to the cloud's own colour)
				B.Cool = B.Life / FootSettle;
			}
			const double AlphaOwn = SafeNum(FrAlpha.Pick(D), 0.0, 1.0, 0.5), HeartOwn = SafeNum(FrHeart.Pick(D), 0.0, 1.0, 0.5);
			B.Alpha = std::min(0.98, (FrNearAlpha > 0.0 ? AlphaOwn + (FrNearAlpha - AlphaOwn) * (1.0 - Far) : AlphaOwn) * AlphaK);
			B.Alpha = Thicker(B.Alpha, FringeBigger);
			B.FadeIn = 0.08;
			B.Hold = 0.15;
			B.Heart = Clamp01(FrNearHeart > 0.0 ? HeartOwn + (FrNearHeart - HeartOwn) * (1.0 - Far) : HeartOwn);
			B.Rim = 0.0;
			if (FrRough > 0.0)
			{
				B.Rough = FrRough;
				B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
			}
			B.ShadeK = FrLight;
			B.CloudShade = FrCloudShade;
			B.Centre = At + (Tilt + ZUp) * (0.45 * FrTall);
			B.CloudR = std::max(0.3 * CloudM, 1e-3);
			B.CentreRise = 0.3 * SqrtK;
			if (F < WetBelow)
			{
				// (its top stood about Up_ and its own half width over the ground; as a low wall its top is 1.8 times its
				// half height: Upright)
				B.Form = Bit::Shape::Upright;
				B.bHug = true;
				B.Vel = Vec(To.X, To.Y, 0.0) * KB;
				B.Rise = 0.0;
				B.Flatten = std::clamp(WetRingHigh * (Up_ + Rr) / (1.8 * std::max(Rr, 1e-3)), 0.05, 2.0);
				B.Col = WetRingCol * Shade;
				B.Col2 = B.Col;
				B.Life = SafeNum(WetRingLife.Pick(DWR), 0.1, 60.0, 1.0);
				Bits.push_back(B);
				continue;
			}
			Lingering(B, FringeOn, T0, At, 0.7, 0.0, StaysSpare, FrKeep, 1.0 + StayLow * (1.0 / (1.0 + FrLow) - F) * (1.0 + FrLow));
			if (B.bTail)
			{
				B.TailLift *= F;
			}
			Bits.push_back(B);
		}
	}

	void Battle::Spout(double T0, const Vec& At, double CrownH, const std::string& Size, Dice& D, const SplashLook* Look)
	{
		// A hit in the sea. (Art director 17:54Z 20 November, from build 1778: the columns were "soft white billows that
		// hang like steam or cotton". Wanted: white water, a fast spout to about mast height with a hard ragged crown and
		// streamers, falling back as a curtain within 3 to 5 s, a dark wet base, then a foam ring and a thin drifting
		// mist.) Every piece is water thrown from the hit: it goes up fast against the air, stands a moment and comes
		// down again, drawn out as it falls. Nothing climbs, and nothing hangs but a little mist. Its numbers are
		// grounds.water.spout's; a size may have its own under sizes.<size>.spout.
		const std::string Wp = "grounds.water.spout.";
		const std::string OwnP = "sizes." + Size + ".spout.";
		auto PathOf = [&](const std::string& Name) -> std::string
		{
			return Has(OwnP + Name) || Has(OwnP + Name + ".0") ? OwnP + Name : Wp + Name;
		};
		auto NumOf = [&](const std::string& Name, double Usual, double Least, double Most)
		{
			return SafeNum(Num(PathOf(Name), Usual), Least, Most, Usual);
		};
		auto SpanOf = [&](const std::string& Name, double Low, double High, double Least, double Most)
		{
			Span S = Range(PathOf(Name), Low, High);
			S.A = SafeNum(S.A, Least, Most, Low);
			S.B = std::max(S.A, SafeNum(S.B, Least, Most, High));
			return S;
		};
		const double H = SafeNum(CrownH, 0.5, 400.0, 10.0);
		const double Tall = std::clamp(std::sqrt(H / 50.0), 0.3, 1.5); // (a small splash is quicker up and down than a bomb's column)
		const double RiseS = Look ? Look->RiseS : NumOf("rise_s", 0.9, 0.1, 5.0) * Tall; // (SeaSplash: the calibre's own, see Stand)
		const double Kd = 3.0 / RiseS;
		const Span FallS = SpanOf("fall_s", 3.2, 4.6, 0.5, 20.0);
		const double StalkR = NumOf("wide", 0.1, 0.02, 3.0) * H, HeadR = std::max(StalkR, (Look && Look->CrownWide > 0.0 ? Look->CrownWide : NumOf("crown_wide", 0.17, 0.02, 4.0)) * H);
		// (how long for its width the water is drawn as it goes up: a column's is jets; the low boil round a sinking ship, lumps)
		const Span Jet = SpanOf("jet_stretch", 1.9, 2.8, 1.0, 6.0);
		const double JetK = 0.5 * (Jet.A + Jet.B) / 2.35;
		// (a hit far off in the picture is fewer and coarser pieces: so far that a 50 m column would stand under full_px
		// tall on the screen, it has that share of them, never under far_least, each as much bigger as keeps it as full.
		// Before the first frame: all of them)
		const double FullPx = NumOf("full_px", 0.0, 0.0, 1e5);
		const double SeenPx = bEyeKnown ? 50.0 * SafeNum(LastPpr, 1.0, 1e7, 1000.0) / std::max(1.0, (LastEye - At).Length()) : FullPx;
		const double FarK = FullPx > 0.0 ? std::clamp(SeenPx / FullPx, NumOf("far_least", 0.12, 0.05, 1.0), 1.0) : 1.0;
		const double PieceMax = NumOf("piece_max_m", 2.4, 0.2, 20.0) / std::sqrt(FarK);
		const Rgb WaterCol = Picture(PathOf("colour"), Rgb::FromPicture(240, 244, 246));
		const Rgb ShadowCol = Picture(PathOf("shadow"), Rgb::FromPicture(150, 166, 180));
		const Rgb WetCol = Picture(PathOf("base_colour"), Rgb::FromPicture(84, 98, 104)).Mix(ShadowCol, Look ? Look->FarFoot : 0.0); // (SeaSplash far off: its foot never a dark puff)
		const Rgb FoamCol = Picture(PathOf("foam_colour"), Rgb::FromPicture(206, 222, 220));
		const double ShadowK = NumOf("shadow_k", 0.7, 0.0, 1.0), BaseTo = NumOf("base_to", 0.3, 0.01, 1.0), Solid = NumOf("alpha", 0.95, 0.0, 1.0);
		const double SideShade = NumOf("side_shade", 0.6, 0.0, 1.0);
		const double HalvePast = NumOf("halve_past", 1800.0, 0.0, 1e6);
		// (a small splash is far fewer pieces, each bigger for its size; on a busy screen a new column is half as many,
		// and with the room four fifths full a quarter: a salvo's last columns are coarse, never missing)
		// (water already thrown that is not yet born counts as asked for: a column's spray and foam come a moment after
		// it, and the later columns of a salvo must know of them, or the room is full before the first is at its height)
		size_t Unborn = 0;
		for (size_t I = 0, Seen = std::min(HeldAtDraw, Bits.size()); I < Seen; ++I)
		{
			Unborn += Bits[I].Born > T0 ? 1 : 0;
		}
		const double Asked = static_cast<double>(RoomGate() + Unborn);
		const double BusyK = HalvePast > 0.0 && Asked > HalvePast ? (Asked > 0.8 * MaxPuffs ? 0.25 : 0.5) : 1.0;
		const double ManyNear = std::clamp(std::pow(H / 50.0, 0.6), 0.12, 1.3) * BusyK;
		const double Many = std::max(0.03, ManyNear * FarK);
		const double Coarse = 1.0 / std::clamp(std::sqrt(ManyNear), 0.5, 1.0) / std::sqrt(FarK);
		// (the column has a shadow side of its own, as a steam plume has: water thrown to the side away from the sun is
		// born that much greyer, the more the lower the sun; shaded piece by piece alone, the column read white all over)
		const Vec SunLevel = bEyeKnown && LastSun.X == LastSun.X && LastSun.Y == LastSun.Y ? Vec(LastSun.X, LastSun.Y, 0.0) : Vec();
		auto Sided = [&](const Rgb& Col, const Vec& Outward, double Off)
		{
			return Col.Mix(ShadowCol, SideShade * Clamp01(0.25 - 0.75 * Outward.Dot(SunLevel) * Off));
		};
		const Vec Downwind = Vec(Wind.X, Wind.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		// (when water thrown Top metres up is back on the sea: up against the air, then down under G)
		auto BackAt = [&](double Top, double G)
		{
			double T = std::sqrt(2.0 * std::max(Top, 0.01) / std::max(G, 0.1));
			for (int I = 0; I < 4; ++I)
			{
				const double Ex = std::exp(-Kd * T);
				const double Slope = Top * Kd * Ex - G * T;
				if (!(Slope < -1e-3))
				{
					break;
				}
				T -= (Top * (1.0 - Ex) - 0.5 * G * T * T) / Slope;
			}
			return T == T ? std::clamp(T, 0.15, 30.0) : 1.0;
		};
		// (how hard it comes down: the top of the column is back on the sea in fall_s, a small one sooner)
		auto FallG = [&]()
		{
			const double Tf = FallS.Pick(D) * Tall;
			return 2.0 * H / (Tf * Tf);
		};
		// (one piece of water, thrown from From so as to stand at To when its rise is spent)
		auto Water = [&](const Vec& From, const Vec& To, double G, double Rad, const Rgb& Col)
		{
			Bit B;
			B.From = From;
			B.Vel = (To - From) * Kd;
			B.Drag = Kd;
			B.Gravity = G;
			B.Windage = 0.3;
			B.Life = BackAt(std::max(To.Z - At.Z, 0.05), G);
			B.R1 = Rad;
			B.R0 = Rad * 0.55;
			B.Grow = 0.35;
			B.Col = Col;
			B.Col2 = Col;
			B.Alpha = Solid;
			B.FadeIn = 0.01;
			B.Hold = 0.45;
			B.Heart = 0.9;
			B.Inner = 0.68;
			B.Rim = 0.0;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.ShadeK = 1.0;
			B.ShadowCol = ShadowCol;
			B.ShadowK = ShadowK;
			return B;
		};
		// (SeaSplash, Look given. Stand: water that stands, as a big shell's column does. Thrown so as to be about its
		// height by RiseS, it climbs on a little, stands near it through HangS and is back on the sea at Ends: the drag
		// takes it to under To by as much as a slow throw against a gentle fall then adds, whose top is To in the middle
		// of the hang. Dyed: the dye of the shell's ship through its water, k of the way, the shade side toward the
		// dye's own shade; as it falls it pales DyePale of the way back to white)
		const double HangS = Look ? Look->HangS : 0.0;
		auto Stand = [&](Bit& B, const Vec& From, const Vec& To, double Ends)
		{
			const double Top = std::max(To.Z - At.Z, 0.05);
			const double Peak = RiseS + 0.5 * HangS, After = std::max(Ends - Peak, 0.2);
			const double G = 2.0 * Top / (After * After);
			const double Bump = std::min(0.5 * G * Peak * Peak, 0.9 * Top);
			B.Vel = (To - ZUp * Bump - From) * Kd;
			B.Gravity = G;
			B.Rise = G * Peak;
			B.Life = Ends;
		};
		auto Tinted = [](const Rgb& C, const Rgb& T, double K)
		{
			const Rgb By = Rgb(1.0f, 1.0f, 1.0f).Mix(T, K);
			return Rgb(C.R * By.R, C.G * By.G, C.B * By.B);
		};
		auto Dye = [&](Bit& B, double K)
		{
			if (!Look || !(Look->DyeK > 0.0))
			{
				return;
			}
			const double Own = Clamp01(K * Look->DyeK);
			const Rgb Plain = B.Col;
			B.Col = Tinted(Plain, Look->Tint, Own);
			B.Col2 = Tinted(B.Col2, Look->Tint, Own * (1.0 - Look->DyePale));
			B.ShadowCol = Tinted(B.ShadowCol, Look->ShadeTint, Own);
			B.Cool = 1.0;
		};
		// (how much of the dye water at this share of the column's height holds: all of it up its body, less toward its
		// crown (DyeTop at the top), and DyeFoot of it in the dark water at its foot (White: how far it has broken white))
		auto DyeAt = [&](double Up, double White)
		{
			const double Crown = Clamp01((Up - 0.55) / 0.45);
			const double Low = Look ? Look->DyeFoot + (1.0 - Look->DyeFoot) * White : 1.0;
			return (1.0 - (1.0 - (Look ? Look->DyeTop : 1.0)) * Crown) * Low;
		};
		// (domes: no column at all, but that many low domes of white water heaving up and sinking over the column's
		// width, with darker water between them: the boil round a ship that is going down. The art director, 22:03Z)
		const Span Domes = SpanOf("domes", 0.0, 0.0, 0.0, 24.0);
		const bool bBoil = Domes.B >= 1.0;
		// the foam: a pale patch where the column stood, and a broken ring of it spreading as the water comes down.
		// Laid first: it lies on the sea, under everything
		const double FoamReach = Look ? 0.5 * Look->FoamM : NumOf("foam_reach", 0.42, 0.05, 2.0) * H;
		const Span FoamS = Look ? Span{ 0.85 * Look->FoamS / Tall, 1.15 * Look->FoamS / Tall } : SpanOf("foam_s", 10.0, 16.0, 1.0, 120.0); // (SeaSplash: its calibre's patch, so long whatever its height)
		const double FoamAlpha = NumOf("foam_alpha", 0.6, 0.0, 1.0);
		const int NFoam = FoamAlpha > 0.0 ? std::max(6, static_cast<int>(NumOf("foam", 22.0, 0.0, 60.0) * Many)) : 0;
		for (int I = -1; I < NFoam; ++I)
		{
			// (the patch first, then the pieces: every other one in the patch, mottling it, the rest a broken band round
			// it that spreads; all soft-edged, so no piece shows an outline of its own)
			const bool bPatch = I < 0, bInner = !bPatch && I % 2 == 0;
			const double A = 2.0 * Pi * (I + D.U(-0.5, 0.5)) / std::max(NFoam, 1);
			const Vec Outward(std::cos(A), std::sin(A), 0.0);
			const double Start = bPatch ? 0.0 : (bInner ? D.U(0.1, 0.9) * StalkR : D.U(0.5, 1.1) * HeadR);
			const double Slow = 0.7 / Tall;
			Bit B;
			B.Form = Bit::Shape::Flat;
			B.Born = T0 + (bPatch ? 0.05 : RiseS * (bInner ? D.U(0.2, 1.5) : D.U(0.9, 2.4)));
			B.From = At + Outward * Start;
			B.From.Z = Height(B.From.X, B.From.Y) + 0.06;
			B.Vel = bPatch ? Vec() : Outward * (std::max(0.0, FoamReach * (bInner ? 0.5 : 1.0) - Start) * D.U(0.35, 1.0) * Slow);
			B.Drag = Slow;
			B.Life = FoamS.Pick(D) * Tall * (bPatch ? 1.0 : D.U(0.6, 1.0));
			B.R0 = bPatch ? StalkR : 0.04 * H;
			B.R1 = FoamReach * (bPatch ? 0.75 : std::min(0.8, (bInner ? D.U(0.14, 0.26) : D.U(0.2, 0.38)) * Coarse)); // (a far column's few big pieces no wider than the patch)
			B.Grow = 0.5;
			B.Stretch = bPatch ? D.U(1.0, 1.25) : D.U(1.2, 1.7);
			B.Spin = bPatch ? D.U(0.0, 2.0 * Pi) : A + Pi / 2.0 + D.U(-0.5, 0.5);
			B.Col = bPatch ? (bBoil ? WetCol.Mix(FoamCol, 0.3) : FoamCol) : WaterCol.Mix(FoamCol, D.U(0.2, 0.8)); // (a boil's patch is dark troubled water, its foam lying on that)
			B.Col2 = FoamCol;
			B.Alpha = FoamAlpha * (bPatch ? 0.55 : D.U(0.35, 0.75));
			B.FadeIn = 0.6 * Tall;
			B.Hold = 0.3;
			B.Heart = 0.6; // (soft from its middle to nothing at its edge: no bright spot, no outline)
			B.Inner = 0.3;
			B.Rim = 0.0;
			B.Rough = 0.3;
			B.ShadeK = 0.0; // (flat on the sea: evenly lit, no bright side and dark side to show its outline)
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			if (Look && Look->StainK > 0.0)
			{
				// (SeaSplash: the patch stained with the dye, its pieces a little less)
				B.Col = B.Col.Mix(Look->Stain, Look->StainK * (bPatch ? 1.0 : 0.6));
				B.Col2 = B.Col2.Mix(Look->Stain, Look->StainK * (bPatch ? 1.0 : 0.6));
			}
			Bits.push_back(B);
		}
		// the surge: a low ring of spray racing out over the sea from the column's foot and settling
		const int NSurge = static_cast<int>(NumOf("surge", 40.0, 0.0, 200.0) * Many);
		for (int I = 0; I < NSurge; ++I)
		{
			const double A = 2.0 * Pi * (I + D.U(-0.4, 0.4)) / std::max(NSurge, 1);
			const Vec Outward(std::cos(A), std::sin(A), 0.0);
			const double Drag = 5.0 / Tall * D.U(0.85, 1.15);
			const double Out_ = D.U(0.45, 1.0);
			Bit B;
			B.Form = Bit::Shape::Upright;
			B.bHug = true;
			B.Born = T0 + D.U(0.0, 0.03);
			B.From = At + Outward * (D.U(0.4, 1.0) * StalkR);
			B.Vel = Outward * (FoamReach * Out_ * Drag);
			B.Drag = Drag;
			B.Windage = 0.5;
			B.Life = D.U(1.6, 2.6) * Tall;
			B.R0 = 0.03 * H;
			B.R1 = std::min(0.6, D.U(0.17, 0.29) * Coarse) * FoamReach; // (broad for their spacing, and soft: one ragged ring, never a ring of discs)
			B.Grow = 0.3;
			B.Stretch = 1.9;
			B.Along = Vec(-std::sin(A), std::cos(A), 0.0);
			B.Flatten = D.U(0.3, 0.6);
			B.Col = WaterCol * D.U(0.9, 1.0);
			B.Col2 = B.Col;
			B.Alpha = Solid * D.U(0.3, 0.5) * (Out_ > 0.7 ? 1.0 - 0.8 * (Out_ - 0.7) / 0.3 : 1.0); // (its outer part feathers away)
			B.FadeIn = 0.03;
			B.Hold = 0.15;
			B.HazeBy = 1.0 * Tall;
			B.HazeAlpha = std::min(B.Alpha, 0.12);
			B.ShadeK = 1.6;
			B.ShadowCol = ShadowCol;
			B.ShadowK = ShadowK;
			B.Heart = 0.4;
			B.Inner = 0.4;
			B.Rim = 0.0;
			B.Rough = 0.35;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			if (Look)
			{
				Dye(B, 0.5);
				B.LitOver = Look->FarOver;
			}
			Bits.push_back(B);
		}
		// (spray: a streamer of water leaving the column at one place, thrown out and a little up over a moment, so
		// that at any instant it is an arc from its root that bends over under its own weight: dashes at its root,
		// thicker there, breaking into beads over its last third. It falls as things fall, faster than the curtain,
		// drawn in shorter and shorter dashes, and is gone before the column's top is down. The art director, 22:03Z:
		// single streaks were "long, dead-straight hairlines that fan out like a firework", and later hung "alone
		// above the column like rain or scratches")
		const int SprayPieces = static_cast<int>(NumOf("streamer_pieces", 5.0, 1.0, 8.0));
		auto Spray = [&](const Vec& Leaves, const Vec& Away, double Throw, double Up, double Born0, int Pieces)
		{
			const double G = std::max(FallG() * D.U(1.3, 1.8), 9.8 * D.U(1.4, 2.0));
			const double Slows = D.U(1.0, 1.6) / Tall;
			const Vec V0 = Away * (std::max(Throw, 0.0) * Slows) + ZUp * std::sqrt(2.0 * G * std::max(Up, 0.0));
			const double Every = Tall * D.U(0.07, 0.1);
			const double Thick = std::max(H, 10.0) * D.U(0.0045, 0.008) * Coarse;
			const double Gone = std::min(V0.Z / G + std::sqrt(2.0 * (std::max(Leaves.Z - At.Z, 0.0) + std::max(Up, 0.0)) / G), D.U(0.8, 1.15) * std::max(Tall, 0.75));
			const double Sheen = D.U(0.6, 0.9);
			for (int K = 0; K < Pieces; ++K)
			{
				// (the first thrown is its tip, the last its root)
				const double Rootward = Pieces > 1 ? static_cast<double>(K) / (Pieces - 1) : 1.0;
				const bool bBead = Pieces > 2 && Rootward < 0.34;
				Bit B;
				B.Form = Bit::Shape::Streak;
				B.Born = Born0 + K * Every;
				B.From = Leaves;
				B.Vel = V0 * (bBead ? D.U(0.86, 1.1) : 1.0); // (its beads no longer quite on its line)
				B.Along = V0.Unit(ZUp);
				B.Drag = Slows;
				B.Gravity = G;
				B.Windage = 0.5;
				B.Life = std::max(0.1, Gone * D.U(0.85, 1.0));
				B.R0 = B.R1 = Thick * (0.4 + 0.6 * Rootward);
				B.HalfLength = H * 0.03; // (falling fast, a dash no longer reaches the next: beads, never a long line)
				B.BySpeed = Every * (bBead ? D.U(0.35, 0.5) : 1.7); // (a dash runs into the next, one line of water; a bead stands clear)
				B.Col = WaterCol;
				B.Col2 = WaterCol;
				B.Alpha = Solid * Sheen * (bBead ? 0.8 : 1.0);
				B.FadeIn = 0.02;
				B.Hold = 0.55;
				B.Heart = 0.9;
				B.Rim = 0.2;
				B.Point = bBead ? 0.4 : 0.6;
				B.Tail = 0.4;
				B.LeastPx = 1.0;
				if (Look)
				{
					Dye(B, Look->DyeTop + 0.3);
					B.LitOver = Look->FarOver;
				}
				Bits.push_back(B);
			}
		};
		if (bBoil)
		{
			// the boil: each dome a cap of white water over a few lower lumps, thrown up slowly from one place and
			// sinking back, no two at the same moment; here and there a burst of spray as air vents
			const int NDomes = std::max(1, static_cast<int>(D.Whole(static_cast<int>(Domes.A), static_cast<int>(Domes.B)) * BusyK * std::sqrt(FarK) + 0.5));
			const int NBursts = D.Whole(1, 3);
			// (dome_in_s: a dome's water comes up out of the sea over this long, never all at once: the pre-check
			// measured the patch's ink rising a sixth in one frame as a dome was born, and the art director asked 0.3 s.
			// Without the key, as it was)
			const double DomeIn = NumOf("dome_in_s", 0.01, 0.01, 2.0);
			const Vec Across = bEyeKnown ? (LastEye - At).Cross(ZUp).Unit(Vec(1.0, 0.0, 0.0)) : Vec(1.0, 0.0, 0.0);
			for (int J = 0; J < NDomes; ++J)
			{
				const double A = 2.0 * Pi * (J + D.U(-0.4, 0.4)) / NDomes, Off = std::sqrt(D.U(0.05, 1.0));
				const Vec Outward(std::cos(A), std::sin(A), 0.0);
				Vec Mid = At + Outward * (HeadR * Off);
				Mid.Z = Height(Mid.X, Mid.Y);
				const double DomeH = H * D.U(0.45, 1.0), DomeR = DomeH * D.U(0.9, 1.4);
				const double Born0 = T0 + RiseS * 2.6 * std::pow(D.U(), 1.7); // (most in its first second or two, the rest straggling after)
				const double G = FallG();
				const int NLumps = D.Whole(3, 4);
				for (int I = -1; I < NLumps; ++I)
				{
					// (the cap: smooth heaving water, pale with the air in it; then froth breaking round its foot, whiter
					// and rougher, a moment after it)
					const bool bCap = I < 0;
					const double B_ = 2.0 * Pi * (I + D.U(-0.4, 0.4)) / NLumps;
					const Vec Aside(std::cos(B_), std::sin(B_), 0.0);
					const Vec To = Mid + (bCap ? ZUp * (0.62 * DomeH) : Aside * (DomeR * D.U(0.65, 1.0)) + ZUp * (DomeH * D.U(0.08, 0.26)));
					const double Rad = std::min(PieceMax * 2.0, DomeR * (bCap ? D.U(0.55, 0.7) : D.U(0.24, 0.38)));
					const Rgb Col = Sided(bCap ? WaterCol.Mix(FoamCol, D.U(0.2, 0.5)).Mix(WetCol, D.U(0.08, 0.22)) : WaterCol, Outward, Off);
					Bit B = Water(Mid + Aside * (0.3 * DomeR * D.U(0.0, 1.0)) + ZUp * 0.05, To, G, Rad, Col * D.U(0.94, 1.0));
					B.Born = Born0 + (bCap ? 0.0 : D.U(0.1, 0.5) * RiseS);
					B.FadeIn = DomeIn;
					B.Along = bCap ? Across : (Across + ZUp * D.U(-0.3, 0.3)).Unit(Across);
					B.Stretch = bCap ? D.U(1.25, 1.6) : D.U(1.4, 2.0); // (a dome: wider than it is high)
					B.Heart = 0.9;
					B.Inner = bCap ? 0.4 : 0.3; // (soft-edged: water, never a block of ice)
					B.Hold = 0.5;
					B.Rough = bCap ? D.U(0.25, 0.4) : D.U(0.45, 0.6);
					if (!bCap)
					{
						B.Life *= D.U(0.6, 0.9);
					}
					Bits.push_back(B);
				}
				if (J < NBursts)
				{
					// (air venting: a short jet of white water thrown up out of the dome, and its spray)
					const double Lean0 = Radians(D.U(0.0, 20.0)), Az0 = D.U(0.0, 2.0 * Pi);
					const Vec Up0(std::cos(Az0) * std::sin(Lean0), std::sin(Az0) * std::sin(Lean0), std::cos(Lean0));
					const double BurstAt = Born0 + RiseS * D.U(0.1, 0.4), Reach = DomeH * D.U(1.5, 2.4);
					for (int I = 0; I < 3; ++I)
					{
						// (soft lumps of spray, the highest the smallest, gone while the dome still stands under them)
						const double F = (I + 1) / 3.0;
						Bit B = Water(Mid + ZUp * 0.1, Mid + Up0 * (Reach * F), FallG() * 2.0, DomeR * 0.2 * (1.3 - 0.6 * F), WaterCol);
						B.Born = BurstAt;
						B.Life = std::min(B.Life, RiseS * D.U(0.6, 0.9));
						B.Along = Up0;
						B.Stretch = 1.6;
						B.Alpha = Solid * 0.8;
						B.Hold = 0.4;
						B.Inner = 0.5;
						B.Rough = 0.4;
						Bits.push_back(B);
					}
					const int NSpray = 2;
					for (int I = 0; I < NSpray; ++I)
					{
						const double B_ = D.U(0.0, 2.0 * Pi), Lean = Radians(D.U(5.0, 25.0));
						Spray(Mid + ZUp * (0.4 * DomeH), Vec(std::cos(B_), std::sin(B_), 0.0), Reach * std::tan(Lean) * D.U(1.0, 1.6), Reach * D.U(0.6, 1.0), BurstAt, std::min(SprayPieces, 4));
					}
				}
			}
		}
		// the dark wet base: heavy unbroken water heaved up and out round the foot, back down first
		const int NFoot = bBoil ? 0 : static_cast<int>(NumOf("foot", 20.0, 0.0, 100.0) * Many);
		for (int I = 0; I < NFoot; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			const bool bMound = I % 3 == 0; // (every third stands at the foot itself, hiding the join with the sea)
			const double Lean = bMound ? D.U(0.0, 0.5) : D.U(0.5, 1.15);
			const Vec Dir(std::cos(A) * std::sin(Lean), std::sin(A) * std::sin(Lean), std::cos(Lean));
			const double Reach = (bMound ? D.U(0.08, 0.18) : D.U(0.1, 0.22)) * H;
			const double Rad = std::min(PieceMax, StalkR * D.U(0.5, 0.85) * Coarse);
			Bit B = Water(At + Vec(std::cos(A), std::sin(A), 0.0) * (StalkR * D.U(0.0, 0.5)) + ZUp * 0.1, At + Dir * Reach, FallG() * 1.3, Rad, WetCol * D.U(0.9, 1.15));
			B.Born = T0 + D.U(0.0, 0.03);
			B.Col2 = WetCol.Mix(WaterCol, 0.6); // (breaking white as it falls back)
			B.ShadowK = 0.0; // (dark all round: the froth's pale shadow colour would grey it)
			B.Cool = 1.0;
			B.Along = Dir;
			B.Stretch = 1.4;
			B.Alpha = Solid * 0.9;
			B.Heart = 0.85;
			B.Inner = 0.5;
			B.Rough = D.U(0.25, 0.4); // (heavy water: a rounder, softer outline than the froth above it)
			if (Look)
			{
				Dye(B, Look->DyeFoot);
			}
			Bits.push_back(B);
		}
		// the column: white water all the way up, dark and wet at its base, wider toward its head
		// (in a sea battle, UseSea, its jets and fingers are never under sea.column.least_px long on the screen: 15 km off
		// a column one pixel wide stands a small white pillar. 0 otherwise: as it was)
		const double FarPx = bSeaFar ? SeaColumnPx * std::min(1.0, Tall) : 0.0, FarKeep = bSeaFar ? SeaColumnKeep : 0.0; // (a small splash held smaller: the heavy shell's column stays the tallest)
		const int NBody = bBoil ? 0 : static_cast<int>(NumOf("body", 110.0, 0.0, 400.0) * Many);
		for (int I = 0; I < NBody; ++I)
		{
			const double F = 0.04 + 0.7 * std::pow(D.U(), 1.15); // (how far up the column it stands: a little denser low down; the fingers are its top)
			const double Flare = Clamp01((F - 0.35) / 0.5);
			const double Rc = StalkR + (HeadR - StalkR) * Flare * Flare * (3.0 - 2.0 * Flare);
			const double A = D.U(0.0, 2.0 * Pi), Off = std::sqrt(D.U());
			const Vec Outward(std::cos(A), std::sin(A), 0.0);
			// (big pieces fill its middle; its sides are many small ones, so its outline is broken froth, never facets)
			const double Rad = std::min(PieceMax, Rc * (Off < 0.6 ? D.U(0.34, 0.52) : D.U(0.18, 0.32)) * (1.0 - 0.3 * Flare) * Coarse);
			const double Whiter = Clamp01((F - 0.25 * BaseTo) / (0.75 * BaseTo));
			// (and one in five is water in the shade of the rest, so the column has depth between its jets and is never one white card)
			const double White = Whiter * Whiter * (3.0 - 2.0 * Whiter);
			// (the froth is shaded first and the dark of the base laid over that: dark water is dark on every side, and
			// never greyed by the froth's pale shadow)
			const double Tint = D.U(0.94, 1.0);
			const Rgb Froth = Sided(D.U() < 0.2 ? WaterCol.Mix(ShadowCol, D.U(0.3, 0.6)) : WaterCol, Outward, Off);
			const Rgb Col = WetCol.Mix(Froth, White) * Tint;
			Bit B = Water(At + Outward * (0.6 * StalkR * Off) + ZUp * 0.1, At + Outward * (0.9 * Rc * Off) + ZUp * (F * H), FallG(), Rad * D.U(0.8, 1.25), Col);
			B.Born = T0 + 0.03 * (1.0 - F);
			B.Along = (ZUp + Outward * D.U(-0.12, 0.22) + Vec(-Outward.Y, Outward.X, 0.0) * D.U(-0.15, 0.15)).Unit(ZUp); // (no two jets quite upright)
			B.ShadowK = ShadowK * White; // (dark water's shadow side is its own dark, never the froth's pale grey)
			B.StretchFrom = Jet.Pick(D); // (a jet going up)
			B.Stretch = B.StretchFrom * D.U(1.2, 1.5); // (and a curtain coming down)
			B.StretchAfter = RiseS * 1.6;
			B.StretchBy = 1.4 * Tall;
			B.Point = 0.1;
			B.Tail = 0.1;
			B.Inner = 0.6;
			B.Hold = 0.6;
			B.Rough = D.U(0.45, 0.6);
			B.LeastPx = FarPx;
			B.LeastKeep = FarKeep;
			if (Look)
			{
				// (SeaSplash: it stands, the column's top the last down; it falls as a curtain once its hang is over)
				const double Ends = (RiseS + HangS + Look->FallS) * D.U(0.9, 1.05) * (0.8 + 0.2 * F / 0.74);
				Stand(B, B.From, At + Outward * (0.9 * Rc * Off) + ZUp * (F * H), Ends);
				// (its foot broken white as it stands: the dark wet water is the first moment's, never a dark lump the
				// column stands on for seconds)
				const double Broken = std::max(White, Look->FootWhite);
				B.Col = WetCol.Mix(Froth, Broken) * Tint;
				B.Col2 = WetCol.Mix(Froth, std::max(Broken, Look->FootWhite + 0.5 * (1.0 - Look->FootWhite))) * Tint;
				B.Cool = 1.0;
				B.ShadowK = ShadowK * Broken;
				B.StretchAfter = RiseS + HangS;
				B.StretchBy = 0.5 * Look->FallS;
				B.Hold = Clamp01((RiseS + HangS) / Ends);
				Dye(B, DyeAt(F, Broken));
				B.LitOver = Look->FarOver;
			}
			Bits.push_back(B);
		}
		// the crown: fingers of water thrown on past the column's head, uneven and leaning out, each drawn to a torn
		// point; and streamers of spray leaving their heads, which bend over and are down before the column is
		const Span Fingers = SpanOf("fingers", 10.0, 16.0, 0.0, 30.0), FingerLean = SpanOf("finger_lean_deg", 5.0, 24.0, 0.0, 80.0), FingerReach = SpanOf("finger_reach", 0.62, 0.98, 0.1, 1.5);
		const Span Streamers = SpanOf("streamers", 0.0, 2.0, 0.0, 30.0), StreamOut = SpanOf("streamer_out", 0.16, 0.32, 0.0, 1.0);
		const int NFingers = bBoil ? 0 : std::max(3, static_cast<int>(D.Whole(static_cast<int>(Fingers.A), static_cast<int>(Fingers.B)) * std::pow(ManyNear, 0.75) * std::pow(FarK, 0.4) + 0.5)); // (a far column keeps more of its fingers than of its body: they are its outline)
		const double AzFrom = D.U(0.0, 2.0 * Pi);
		for (int J = 0; J < NFingers; ++J)
		{
			const double Az = AzFrom + 2.0 * Pi * (J + D.U(-0.35, 0.35)) / std::max(NFingers, 1);
			// (a finger goes up inside the column and bends out of its head: its lean is its tip's, so the column stands
			// straight under a spreading crown and is never a fan from the foot)
			const double Spread = std::tan(Radians(J == 0 ? D.U(0.0, 4.0) : FingerLean.Pick(D)));
			const double ReachJ = H * (J == 0 ? D.U(1.0, 1.06) : (J == 1 ? D.U(0.84, 0.92) : FingerReach.Pick(D))); // (one tallest, the next clearly shorter, the rest uneven)
			const Vec Out2(std::cos(Az), std::sin(Az), 0.0);
			const double RootOff = 0.6 * StalkR * D.U(0.2, 1.0);
			const double Rf = std::min(PieceMax, StalkR * D.U(0.3, 0.5) * Coarse), Rt = Rf * 0.22;
			const Rgb FingerCol = Sided(WaterCol * D.U(0.92, 1.0), Out2, std::min(1.0, 0.3 + Spread * 2.5));
			const double G = FallG();
			const double Fades = Look ? (RiseS + HangS + 0.6 * Look->FallS) * D.U(0.85, 1.1) : D.U(1.8, 2.8) * Tall; // (a finger's water breaks to spray in the air and is gone before it is down)
			double F = 0.4 + D.U(0.0, 0.04);
			while (F < 1.0)
			{
				const double Fn = Clamp01((F - 0.4) / 0.6); // (0 where the finger leaves the column, 1 at its tip)
				const double Rl = Rt + (Rf - Rt) * std::pow(1.0 - Fn, 0.8);
				const Vec Side = Vec(D.U(-1.0, 1.0), D.U(-1.0, 1.0), D.U(-1.0, 1.0)) * (Rl * 0.4);
				const Vec Stands = At + Out2 * (RootOff + ReachJ * Spread * std::pow(Fn, 1.6)) + ZUp * (F * ReachJ);
				// (where a finger stands inside the column's dark base it is that dark water too, as the body round it)
				const double WhiterJ = Clamp01((F * ReachJ / H - 0.25 * BaseTo) / (0.75 * BaseTo)), WhiteJ = WhiterJ * WhiterJ * (3.0 - 2.0 * WhiterJ);
				Bit B = Water(At + Out2 * (0.6 * RootOff) + ZUp * 0.1, Stands + Side, G, Rl * D.U(0.85, 1.2), WetCol.Mix(FingerCol, WhiteJ));
				B.ShadowK = ShadowK * WhiteJ;
				B.Born = T0 + 0.03 * (1.0 - F);
				B.Life = std::min(B.Life, Fades * D.U(0.8, 1.2) * (1.0 - 0.3 * Fn)); // (its tip first: nothing is left standing alone over the column as it comes down)
				B.Along = (ZUp + Out2 * (Spread * 1.6 * std::pow(Fn, 0.6) / 0.6)).Unit(ZUp); // (along the finger where it stands)
				B.StretchFrom = std::max(1.0, (2.0 + 1.4 * Fn) * JetK);
				B.Stretch = B.StretchFrom * 1.5;
				B.StretchAfter = RiseS * 1.1;
				B.StretchBy = 1.4 * Tall;
				B.Point = 0.2 + 0.6 * Fn; // (torn and pointed at the tip)
				B.Tail = 0.15;
				B.Alpha = Solid * (1.0 - 0.3 * Fn);
				B.Heart = 0.9 - 0.2 * Fn;
				B.Inner = 0.6;
				B.Rough = 0.45 + 0.15 * Fn;
				B.LeastPx = FarPx;
				B.LeastKeep = FarKeep;
				if (Look)
				{
					Stand(B, B.From, Stands + Side, (RiseS + HangS + Look->FallS) * D.U(0.95, 1.08));
					B.Life = std::min(B.Life, Fades * D.U(0.8, 1.2) * (1.0 - 0.3 * Fn));
					B.StretchAfter = RiseS + HangS;
					B.StretchBy = 0.5 * Look->FallS;
					Dye(B, DyeAt(F * ReachJ / H, WhiteJ));
					B.LitOver = Look->FarOver;
				}
				Bits.push_back(B);
				F += std::max(0.02, Rl * std::max(1.0, (2.0 + 1.4 * Fn) * JetK) * 0.85 / std::max(ReachJ, 1e-3) * D.U(0.7, 1.1)) * Coarse;
			}
			const int NSpray = D.Whole(static_cast<int>(Streamers.A * Many), static_cast<int>(Streamers.B * Many));
			const Vec Tip = Out2 * (RootOff + ReachJ * Spread) + ZUp * ReachJ; // (the finger's tip, from the hit)
			for (int I = 0; I < NSpray; ++I)
			{
				// (it leaves the finger's head where that is at the moment, on its way up, and swings out to one side of it)
				const double When = RiseS * D.U(0.3, 0.6), Swing = D.U(-0.9, 0.9);
				const Vec Leaves = At + Tip * ((1.0 - std::exp(-Kd * When)) * D.U(0.9, 1.0));
				Spray(Leaves, Out2 * std::cos(Swing) + Vec(-Out2.Y, Out2.X, 0.0) * std::sin(Swing), H * StreamOut.Pick(D), H * D.U(0.03, 0.09), T0 + When, SprayPieces);
			}
		}
		// (and streamers out of the column's sides, lower down)
		const int NSide = bBoil ? 0 : static_cast<int>(NumOf("side_spray", 24.0, 0.0, 200.0) * Many);
		for (int I = 0; I < NSide; ++I)
		{
			// (out of the column's side where the water has just come up to that height)
			const double A = D.U(0.0, 2.0 * Pi), F = D.U(0.3, 0.72);
			const Vec Outward(std::cos(A), std::sin(A), 0.0);
			const double Flare = Clamp01((F - 0.35) / 0.5);
			const double Rc = StalkR + (HeadR - StalkR) * Flare * Flare * (3.0 - 2.0 * Flare);
			Spray(At + Outward * (0.8 * Rc) + ZUp * (F * H), Outward, H * D.U(0.08, 0.2), H * D.U(0.0, 0.04), T0 + RiseS * (-std::log(1.0 - F) / 3.0 + D.U(0.05, 0.35)), SprayPieces);
		}
		// the mist: a little fine spray left in the air as the water comes down, thin, going off down the wind
		const Span MistAlpha = SpanOf("mist_alpha", 0.08, 0.16, 0.0, 1.0), MistS = SpanOf("mist_s", 4.0, 7.0, 0.5, 60.0);
		const int NMist = static_cast<int>(NumOf("mist", 10.0, 0.0, 100.0) * Many);
		for (int I = 0; I < NMist; ++I)
		{
			const double A = D.U(0.0, 2.0 * Pi);
			Bit B;
			B.Born = Look ? T0 + RiseS + HangS * D.U(0.2, 1.0) : T0 + RiseS * D.U(1.0, 2.6); // (SeaSplash: as its hang ends)
			B.From = At + Vec(std::cos(A), std::sin(A), 0.0) * (HeadR * D.U(0.0, 0.8)) + ZUp * (H * D.U(0.12, 0.7));
			B.Vel = Vec(D.Bell(), D.Bell(), 0.0) * (0.02 * H) + ZUp * (-0.03 * H * D.U(0.5, 1.5));
			B.Drag = 0.4;
			B.Windage = D.U(0.7, 1.3);
			B.Life = MistS.Pick(D) * Tall;
			B.R0 = 0.07 * H;
			B.R1 = D.U(0.14, 0.24) * H;
			B.Grow = 0.6;
			B.Along = Downwind;
			B.Stretch = D.U(1.5, 2.2);
			B.Col = WaterCol;
			B.Col2 = WaterCol;
			B.Alpha = MistAlpha.Pick(D);
			B.FadeIn = 0.6 * Tall;
			B.Hold = 0.2;
			B.Heart = 0.4;
			B.Rim = 0.0;
			B.Rough = 0.3;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.ShadowCol = ShadowCol;
			B.ShadowK = ShadowK;
			if (Look)
			{
				if (Look->MistS > 0.0)
				{
					B.Life = Look->MistS * D.U(0.85, 1.15); // (the ruling's 4 to 6 s at the foot of a tall column)
				}
				Dye(B, Look->MistDye);
			}
			Bits.push_back(B);
		}
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
			B.bFarCull = bLandFar && LandGonePx > 0.0 && !bSeaFar; // (burst.far: a mark too small to see far off is not laid)
			Marks.push_back(B);
		};
		if (P.bBruise)
		{
			// (paint.bruise, the art director's 20:25Z read of rock: high explosive on rock leaves a pale bruise, never a
			// dark star. A pale powder splash with a few broad, short, soft lobes, a small dark middle bruise_core of its
			// radius across (paint.scar), and pale chips of stone scattered bruise_chips_m out)
			Mark(At, R * 1.1, D.U(0.9, 1.15), D.U(0.0, 2.0 * Pi), P.Bruise, 0.7, 0.5, 0.0);
			const int NLobe = D.Whole(static_cast<int>(P.BruiseLobesLo), static_cast<int>(P.BruiseLobesHi));
			const double LobeFrom = D.U(0.0, 2.0 * Pi);
			for (int I = 0; I < NLobe; ++I)
			{
				// (each lobe its own length, 0.6 to 1.4, and unevenly spaced, so no two scars match and none is a regular
				// star: the art director's 20:35Z carry)
				const double A = LobeFrom + 2.0 * Pi * (I + D.U(-0.4, 0.4)) / std::max(NLobe, 1);
				const double Long = D.U(0.6, 1.4);
				Mark(At + Vec(std::cos(A), std::sin(A), 0.0) * (D.U(0.75, 1.05) * R * (0.5 + 0.5 * Long)), D.U(0.45, 0.65) * R * (0.7 + 0.3 * Long), D.U(1.2, 1.6) * Long, A, P.Bruise * D.U(0.95, 1.05), D.U(0.55, 0.7), 0.4, 0.0);
			}
			Mark(At, P.BruiseCore * R, D.U(0.9, 1.2), D.U(0.0, 2.0 * Pi), P.bScar ? P.Scar : P.Heavy * 0.6, 0.85, 0.7, 0.0);
			const int NChip = D.Whole(10, 16);
			for (int I = 0; I < NChip; ++I)
			{
				const double A = D.U(0.0, 2.0 * Pi);
				Mark(At + Vec(std::cos(A), std::sin(A), 0.0) * (R + D.U(P.BruiseChipsLo, P.BruiseChipsHi) * std::sqrt(D.U())), D.U(0.05, 0.12), D.U(1.0, 1.6), D.U(0.0, 2.0 * Pi), P.bSplinter ? P.Splinter : P.Bruise, 0.8, 0.8, 0.3);
			}
		}
		else
		{
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
		GroundIn = GroundOn(GroundIn); // (a ground whose own switch is off is drawn as earth)
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
		GroundIn = GroundOn(GroundIn); // (a ground whose own switch is off is drawn as earth)
		// (Effects (R2): "mg:star", the row mg with its own look, muzzle.look.star; a name with no colon as ever)
		std::string GunBase, LookName;
		GunIn = LookSplit(GunIn, GunBase, LookName);
		GunLook GL;
		const bool bLook = GunLookOf(LookName, GL);
		if (bLook && GL.bFlicker)
		{
			GL.Star = 0; // (a machine gun's burst: its rounds (Shots, the first at this same moment) lay its star, one a round)
		}
		// (a tank's or a field gun's smoke and the dust it kicks up go slowly into the air: linger.gun, by the gun's row)
		const size_t First = Bits.size();
		MuzzleLaid(Now, At, Facing, GunIn, GroundIn, bLook ? &GL : nullptr);
		if (Bits.size() > First)
		{
			const double Foot = Height(At.X, At.Y);
			LingerFrom(First, LingerOf("gun", GunIn ? GunIn : "rifle"), Now, Vec(At.X, At.Y, Foot), 0.65, std::max(At.Z - Foot, 1.0));
		}
		if (bLook)
		{
			LookLaid(Now, At, Facing.Unit(Vec(1.0, 0.0, 0.0)), GL, GroundIn);
		}
	}

	void Battle::MuzzleLaid(double Now, const Vec& At, const Vec& Facing, const char* GunIn, const char* GroundIn, const GunLook* Look)
	{
		std::string M = std::string("muzzle.") + (GunIn ? GunIn : "rifle") + ".";
		if (!Has(M + "flame_m"))
		{
			M = "muzzle.rifle.";
		}
		Dice D(++Seeds * 15485863ull ^ PlaceKey(At));
		const size_t FlashFrom = Bits.size(); // (a sea battle's far floor is laid on the flash alone: SeaFlash)
		// (a flame length that is not a number, or without end, would lay points that are not numbers: the pre-check's find)
		const double LenIs = Look && Look->FlameM > 0.0 ? Look->FlameM : Num(M + "flame_m", 0.3); // (Effects (R2): the look's own length)
		const double Len = LenIs == LenIs ? std::min(20.0, std::max(0.0, LenIs)) : 0.3;
		const Vec Fw = Facing.Unit(Vec(1.0, 0.0, 0.0));
		const Vec Across = Fw.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Rgb Core = Picture("burst.flash.core_colour", Rgb::FromPicture(236, 168, 72));
		// (art director 07:08Z: close to the eye a gun's flash is lit from within, a yellow-white root and an orange body
		// with dense rims, shrinking rather than going see-through; far off as before)
		HotFlash H;
		const bool bHot = HotOf(At, H);
		const bool bRagged = bHot && H.bRagged;
		if (Look && bHot)
		{
			// (Effects (R2): the look's flame, a flash hider's narrower cone, its licks, and its star or brake jets laid last)
			H.Wide = std::clamp(H.Wide * Look->Narrow, 0.02, 1.0);
			H.Licks.A *= Look->LicksK;
			H.Licks.B *= Look->LicksK;
			H.Look = *Look;
			PaleHot(H, Look->DayPale * LookDay); // (by day paler, and smaller below)
		}
		const double LookK = Look ? LookDayK(*Look) : 1.0;
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
			if (Look)
			{
				B.R0 *= Look->Narrow * LookK;
				B.R1 *= Look->Narrow * LookK;
				B.From = At + (B.From - At) * LookK;
				const double Pale_ = Look->DayPale * LookDay;
				if (Pale_ > 0.0)
				{
					B.Col = B.Col.Mix(Rgb::FromPicture(250, 238, 200), Pale_);
					B.Col2 = B.Col2.Mix(Rgb::FromPicture(250, 238, 200), Pale_);
				}
			}
			if (bRagged)
			{
				// (art director 13:54Z: graded from its root out and ragged, new every shot; the same dice go on as before)
				Dice DF(PlaceKey(At) * 0xD6E8FEB86659FD93ull ^ (static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) + 0x52ull));
				HotFlame(Now, At, Fw, Len * 1.4 * LookK, B.Life, H, DF); // (LookK: a look by day, smaller; 1 otherwise)
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
			if (!bRagged && (!Look || I < std::lround(NLicks * Look->LicksK)))
			{
				Bits.push_back(B);
			}
		}
		if (Look && !bRagged && (Look->Star > 0 || Look->Jets > 0))
		{
			// (Effects (R2): the look's star and brake jets past the close flash's reach, lit as the far flash, with dice
			// of their own; the star only within star_far_m of the eye, where its spikes are more than a pixel)
			GunLook Far_ = *Look;
			if (!bHot && !(bEyeKnown && (LastEye - At).Length() <= Look->StarFarM))
			{
				Far_.Star = 0;
			}
			Dice DL(PlaceKey(At) * 0x8EBC6AF09C88C6E3ull ^ (static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) + 0x6A2ull));
			LookFlame(Now, At, Fw, Len, DL.U(0.03, 0.045), Far_, bHot ? &H : nullptr, DL);
		}
		if (Has(M + "fireball_m"))
		{
			GunFireball(Now, At, Fw, M); // (a naval gun's fireball and smoke ring, laid before the sea's far floor so a far one is held as one flash)
		}
		if (bSeaFar)
		{
			SeaFlash(Now, FlashFrom);
		}
		if (Has(M + "lamp_cd"))
		{
			// (a naval gun's lamp, the sea's night fight of 25 November (look bible S7: "gun flashes light the hulls"):
			// lamp_cd candelas at night at full, whatever game.lamp_candela_at_full is, reaching lamp_reach_m (about her
			// hull's length, so the flash lights its own ship and the water by her, not a 30 m patch by the turret), hung
			// lamp_out_m out along the bore's level way and lamp_up_m up from the middle of its fireball (its flame's,
			// without one) and going down over lamp_s; by day only a row with day_lamp lights anything, at
			// game.day_lamp_share, as every gun's. Out and up: the look reviews of 25 November. A lamp by the fireball, a
			// few metres off her side, burnt a white spot on the plating by the gun and left her ends 7 stops darker
			// (inverse square, and the side plating taking it edge on); hung out and up it lights her whole side within
			// about 1.5 stops. Hung high (two fifths of her length up) it lit her from overhead, as a star shell does, her
			// decks and roofs as bright as her side; hung about half her length out and a quarter of it up, the light
			// comes to her side some 30 degrees up, over the water, and her deck takes under half what her side does)
			const double CdAtFull = SafeNum(Num("game.lamp_candela_at_full", 2000.0), 1.0, 1.0e9, 2000.0); // (the wrapper's own default)
			const double BallLong = std::max(SafeNum(Num(M + "fireball_m", 0.0), 0.0, 150.0, 0.0), Len);
			Flash F;
			F.At = At + Fw * (BallLong * 0.5) + ZUp * SafeNum(Num(M + "lamp_up_m", 0.0), -20.0, 150.0, 0.0);
			const double OutBy = SafeNum(Num(M + "lamp_out_m", 0.0), 0.0, 300.0, 0.0);
			if (OutBy > 0.0)
			{
				F.At = F.At + Vec(Fw.X, Fw.Y, 0.0).Unit(Vec()) * OutBy; // (a bore straight up: no level way, hung over it)
			}
			// (the lamp rule's merge measures from the muzzle, not from the lamp hung out along the bore: two ships 500 m
			// apart firing toward each other hang their lamps toward each other, never their guns. The review of 25 Nov)
			F.Gun = At;
			F.bGun = true;
			F.Col = Picture(M + "lamp_colour", Picture("burst.flash.light.colour", Rgb::FromPicture(255, 196, 140)));
			F.Born = Now;
			F.Life = 0.03;
			F.Strength = SafeNum(Num(M + "lamp_cd", 0.0), 0.0, 1.0e7, 0.0) / CdAtFull;
			F.Radius = SafeNum(Num(M + "lamp_reach_m", 30.0), 1.0, 2000.0, 30.0);
			F.LampS = SafeNum(Num(M + "lamp_s", 0.0), 0.0, 0.5, 0.0); // (0.5: a flash is pruned half a second after its birth)
			F.bNightOnly = Num(M + "day_lamp", 0.0) <= 0.0;
			Flashes.push_back(F);
		}
		else if (Has(M + "light_w"))
		{
			// (the lamp hangs above the muzzle, as a shell's does, so the ground at the gun's foot is lifted, never washed
			// white: at the muzzle's own height a lamp of a shell's strength burns a white pool there. Its strength is cut
			// by the square of how much lower it hangs than a shell's lamp of the same strength, so its brightest spot on
			// the ground is no brighter than that shell's: 1700, a tank gun at night lit 1 m round its foot to 249 of 255)
			const double Watts = SafeNum(Num(M + "light_w", 0.0), 0.0, 1.0e7, 0.0) / 8500.0; // (held, the pre-check of 22 November: an infinite light_w was an infinite lamp)
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
		const Rgb Grey = Picture(M + "smoke_colour", Picture("game.muzzle_smoke", Rgb::FromPicture(150, 140, 126))); // (smoke_colour: a row's own, a naval gun's browner cordite smoke; none: the game's)
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
		const bool bBillows = Len >= 0.5 && Has("muzzle.billows.low") && Num(M + "billows", 1.0) > 0.0; // (billows 0: a row that never takes them, a naval gun's smoke over the sea)
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
		// big guns throw a ring of dust off the ground in front
		const double RingIs = Num(M + "dust_ring_m", 0.0);
		const double Ring_ = RingIs > 0.0 ? std::min(RingIs, 30.0) : 0.0; // (never wider than 30 m: a wild number stays a big gun's ring)
		const std::string Ground = GroundIn && *GroundIn ? GroundIn : "earth";
		// (dry ground, muzzle.dry: a few dense lumps stand in for the ring's sheets, the cloud's mounds and the veil, and the
		// gun's own smoke is fewer wisps that take on the dust's colour. See below. Every old piece's dice are thrown all
		// the same)
		const std::string Dr = "muzzle.dry.";
		auto DryNum = [&](const char* Key, double Least, double Most, double Usual) { const double Is = Num(Dr + Key, Usual); return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		const bool bDryTall = Has(Dr + "tall.0"); // (the dense lit lumps of 06:08Z 20 November: see below)
		const double DryK = Ring_ > 0.0 && (bDryTall || Has(Dr + "lumps.0")) ? DryNum(("by_ground." + Ground).c_str(), 0.0, 2.0, 0.0) : 0.0;
		const bool bDry = DryK > 0.0;
		// (the dry ground's dust, Tone times as light as its sunlit top: where the game can say what its screen shows of the
		// ground under the muzzle (SetGroundColour), that colour lighter times as light, written to land on it
		// (picture.to_land); else the ground row's dust paint)
		double UnderR = 0.0, UnderG = 0.0, UnderB = 0.0;
		const bool bUnder = bDry && GroundColour(At, UnderR, UnderG, UnderB);
		const double DryLighter = bDry ? DryNum("lighter", 0.5, 1.5, 1.0) : 1.0;
		const LandTable Land = bUnder ? LandTableOf() : LandTable();
		const Rgb DryPaint = bDry ? PaintOf(Ground).Dust * 0.85 : Rgb();
		auto DryDust = [&](double Tone)
		{
			const double K = std::max(0.0, Tone) * DryLighter;
			return bUnder ? Rgb::FromPicture(Land.ToLand(UnderR * K), Land.ToLand(UnderG * K), Land.ToLand(UnderB * K)) : DryPaint * std::pow(std::max(0.0, Tone), 2.2);
		};
		if (bBillows)
		{
			const double Flame_ = std::min(Len, 4.0);
			const double DrySmoke = bDry ? DryNum("smoke_as_dust", 0.0, 1.0, 0.6) : 0.0, DryShade = bDry ? DryNum("cloud_shade", 0.0, 2.0, BillowShade) : BillowShade;
			const int NTop = BillowCount("top", 12.0, (10.0 + 6.0 * Flame_) / 17.8 * (bDry ? DryNum("smoke", 0.0, 1.0, 0.6) : 1.0));
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
				B.Col = (bDry ? Grey.Mix(DryDust(0.94), DrySmoke) : Grey) * DB.U(0.9, 1.1); // (0.94: its dice and the sun's side still leave it under the dust's lightest)
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
				B.CloudShade = bDry ? DryShade : BillowShade;
				B.CloudR = Flame_ * 1.5;
				B.Rest = BillowRest;
				if (!(bDry && bDryTall)) // (with muzzle.dry.tall no wisp is laid: the gun's smoke is the dense puffs below)
				{
					Bits.push_back(B);
				}
			}
		}
		const int NSmoke = bBillows || Has(M + "cloud_m") ? 0 : Len < 0.5 ? 3 : static_cast<int>(10.0 + 6.0 * std::min(Len, 4.0)); // (a wild flame is still a few puffs; a naval gun's cloud (cloud_m) is its own, GunFireball)
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
				if (!bDry)
				{
					Bits.push_back(B);
				}
			}
			if (bBillows && !bDry)
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
			if (bDry)
			{
				// Art director 19:57Z on the game's own west view on sand (build 1748). There the ring's sheets, the cloud's
				// mounds and the veil's lumps, some sixty soft pieces of one pale colour, read from the battle eye as one
				// sheet of fog over a third of the frame, and the men 4 m from the muzzle were grey shapes. On dry ground
				// (muzzle.dry.by_ground) a gun's blast raises instead a few dense lumps with clear ground between them: one
				// to a stretch of the line of fire, the heaviest low and by the muzzle, smaller and thinner farther out,
				// every third standing a little higher as the cloud's heads, each of its own shade, none thinning to a
				// haze: all gone by clear_s. Their colour is the ground's own under the muzzle where the game can say
				// (SetGroundColour), lighter times as light at the most; else the ground row's dust paint. Its own dice.
				auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
				if (bDryTall)
				{
					// Art director 06:08Z 20 November on build 1758's west still on sand. Half a second after the shot the lumps
					// above and the gun's smoke wisps along the bore still read as one soft cloud, and two riflemen standing
					// 4 m off the line of fire, seen across it, almost vanished. With muzzle.dry.tall the blast raises four to
					// six bigger, denser pieces and nothing thin. Tall lumps (tall) stand where the blast strikes the ground
					// hardest: at the muzzle's two flanks and just ahead of it, never behind it. Low mounds (low) lie farther
					// down the line of fire, one to a stretch with clear ground between, wider than they are high, smaller
					// the farther out. Each is lit as a thing with form (light): its top a little lighter than the ground,
					// its underside in shadow. The gun's own smoke is one or two dense puffs at the muzzle (puffs) in place
					// of the wisps. The tall ones hold to near_s, thinning, the cloud that gives the gun away; the mounds
					// are gone by clear_s. Their colour is the ground's own, as above. Its own dice.
					Dice DL(PlaceKey(At) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) ^ 0xD27Eull);
					auto CountOf = [&](const char* Key, double Low, double High) { return std::clamp(static_cast<int>(std::lround(Safe(Range(Dr + Key, Low, High).Pick(DL), 0.0, 8.0, Low) * DryK)), 0, 12); };
					const int NTall = CountOf("tall", 2.0, 3.0), NLow = CountOf("low", 2.0, 3.0), NPuff = bBillows ? CountOf("puffs", 1.0, 2.0) : 0;
					const Span TallR = Range(Dr + "tall_r", 0.15, 0.19), LowR = Range(Dr + "low_r", 0.085, 0.11), PuffR = Range(Dr + "puff_r", 0.45, 0.6);
					const Span AlphaIs = Range(Dr + "alpha", 0.88, 0.95), ClearIs = Range(Dr + "clear_s", 2.0, 2.4), NearIs = Range(Dr + "near_s", 2.8, 3.2), ShadeIs = Range(Dr + "shade", 0.86, 0.93);
					const double Light = DryNum("light", 0.0, 6.0, 3.0), DryTwin = Safe(Num(Dr + "twin_px", 6.0), 0.0, 4096.0, 6.0);
					const double DryInner = DryNum("inner", 0.1, 0.9, 0.78), DryRough = DryNum("rough", 0.0, 0.8, BillowRough), DryRest = 1.0 + 0.6 * DryRough;
					const double SmokeDust = DryNum("smoke_as_dust", 0.0, 1.0, 0.6);
					// (art director 08:03Z 20 November, on build 1762's still from 60 m a fifth of a second after the shot: the
					// young lumps read as small pale balls. With low_s they are born low (Bit::LowBy): until low_s.0 seconds
					// old each lies along the ground low_flat times as high as it is wide, the way blast dust first skirts
					// out, in its plain tone with no lit top and with a soft edge (low_soft); by low_s.1 it has risen round,
					// firmed and earned its light. The gun's smoke puffs, off the ground, keep their shape and only firm and
					// come into their light as late. No key: as before.
					// Build 1764's stills from 60 m (09:28Z): a soft-edged mound in one flat tone a little under the ground's
					// is lost on the game's rippled sand, so nothing marked the shot for a third of a second. low_lit: the
					// share of its light and shade a mound is born with, so it has a top near the ground's own tone and a
					// darker foot and reads as a low heap of dust; low_soft 1 gives it its firm edge from birth)
					const double LowFlat = DryNum("low_flat", 0.1, 1.0, 0.5), LowFrom = DryNum("low_s.0", 0.0, 10.0, 0.0), LowBy = DryNum("low_s.1", 0.0, 10.0, 0.0), LowSoft = DryNum("low_soft", 0.1, 1.0, 0.55), LowLit = DryNum("low_lit", 0.0, 1.0, 0.0);
					const Vec Under(At.X, At.Y, Height(At.X, At.Y)); // (the ground under the muzzle)
					const double FlankFirst = DL.U() < 0.5 ? -1.0 : 1.0;
					// (a lobe of a lump: the same dust beside it, Size times as big, with an outline of its own)
					auto Lobe = [&](const Bit& Main, const Vec& Off, double Size)
					{
						Bit C = Main;
						C.From = Main.From + Off;
						C.R0 = Main.R0 * Size;
						C.R1 = Main.R1 * Size;
						C.LumpSeed = static_cast<uint32_t>(DL.U() * 1073741823.0);
						C.Turn = DL.U(0.0, 2.0 * Pi);
						Bits.push_back(C);
					};
					for (int I = 0; I < NTall + NLow + NPuff; ++I)
					{
						const bool bTall = I < NTall, bPuff = I >= NTall + NLow;
						Bit B;
						B.Born = Now + DL.U(0.0, 0.05);
						B.Windage = 0.6;
						B.FadeIn = 0.05;
						B.Heart = 1.0;
						B.Inner = DryInner;
						B.Rough = DryRough;
						B.LumpSeed = static_cast<uint32_t>(DL.U() * 1073741823.0);
						B.Turn = DL.U(0.0, 2.0 * Pi);
						B.Rest = DryRest;
						B.TwinPx = DryTwin;
						B.ShadeK = Light;
						B.LowFlat = bPuff ? 1.0 : LowFlat;
						B.LowFrom = LowFrom;
						B.LowBy = LowBy;
						B.LowSoft = LowSoft;
						B.LowLit = LowLit;
						B.Thin = 2;
						B.Origin = At - FwFlat * 1.5; // (the gun's body, behind its muzzle)
						const double Tone = Safe(ShadeIs.Pick(DL), 0.3, 1.5, 0.9);
						B.Alpha = Safe(AlphaIs.Pick(DL), 0.0, 0.95, 0.9);
						B.Grow = 0.4;
						B.Col = DryDust(Tone);
						B.Col2 = B.Col;
						if (bTall)
						{
							// (the first two at the flanks, one to a side; any more just ahead of the muzzle. Its middle at least
							// 0.9 of its radius ahead of the muzzle and its two shoulders to its sides, leaning up to half a
							// radian: a shoulder may reach a few tenths of a metre back past the muzzle's line (0.43 m at the
							// most, the pre-check, 07:31Z), a metre short of the gun's body)
							const bool bFlank = I < 2;
							const double Side = bFlank ? (I == 0 ? FlankFirst : -FlankFirst) * DL.U(0.07, 0.16) : DL.Bell() * 0.04;
							B.R1 = Ring_ * Safe(TallR.Pick(DL), 0.01, 1.0, 0.17);
							B.R0 = B.R1 * 0.3;
							const double Ahead = std::max(0.9 * B.R1, Ring_ * (bFlank ? DL.U(0.08, 0.2) : DL.U(0.2, 0.3)));
							B.From = Under + FwFlat * Ahead + Across * (Side * Ring_);
							B.Vel = FwFlat * DL.U(0.5, 1.5) + Across * (Side * Ring_ * DL.U(0.5, 1.2));
							B.Drag = 2.5;
							B.Rise = DL.U(0.1, 0.3);
							B.Life = std::max(Safe(NearIs.Pick(DL), 0.0, 30.0, 0.0), Safe(ClearIs.Pick(DL), 0.2, 20.0, 2.0));
							B.Hold = 0.45; // (it holds on by thinning)
							Bits.push_back(B);
							const double Lean = DL.U(-0.5, 0.5);
							const Vec Aside = Across * std::cos(Lean) + FwFlat * std::sin(Lean);
							B.Rise = 0.0; // (its shoulders lie on the ground)
							Lobe(B, Aside * (B.R1 * DL.U(0.6, 0.8)), DL.U(0.6, 0.75));
							Lobe(B, Aside * (-B.R1 * DL.U(0.6, 0.8)), DL.U(0.6, 0.75));
						}
						else if (!bPuff)
						{
							// (a heap of two lobes laid along the line of fire, give or take: wider than it is high)
							const int J = I - NTall;
							const double Along_ = (J + DL.U(0.2, 0.8)) / NLow, Lean = DL.U(-0.45, 0.45);
							B.R1 = Ring_ * Safe(LowR.Pick(DL), 0.01, 1.0, 0.1) * (1.0 - 0.25 * Along_);
							B.R0 = B.R1 * 0.3;
							B.From = Under + FwFlat * (Ring_ * (0.38 + 0.8 * Along_)) + Across * (DL.Bell() * 0.05 * Ring_);
							B.Vel = FwFlat * DL.U(1.0, 2.5) + Across * DL.U(-0.5, 0.5);
							B.Drag = 2.0;
							B.Life = Safe(ClearIs.Pick(DL), 0.2, 20.0, 2.0) * (1.0 - 0.2 * Along_);
							B.Hold = 0.6;
							Bits.push_back(B);
							Lobe(B, (FwFlat * std::cos(Lean) + Across * std::sin(Lean)) * (B.R1 * DL.U(0.8, 1.0) * (DL.U() < 0.5 ? -1.0 : 1.0)), DL.U(0.65, 0.8));
						}
						else
						{
							// (the gun's own smoke: a dense puff rolling out of the muzzle, the dust's colour mixed into its grey)
							const double Flame_ = std::min(Len, 4.0);
							B.R1 = Flame_ * Safe(PuffR.Pick(DL), 0.05, 3.0, 0.5) + 0.1;
							B.R0 = B.R1 * 0.3;
							B.From = At + Fw * (Flame_ * DL.U(0.3, 0.7));
							B.Vel = Fw * DL.U(1.5, 3.0) + Across * DL.U(-0.4, 0.4);
							B.Drag = 3.0;
							B.Rise = DL.U(0.2, 0.5);
							B.Windage = 0.8;
							B.Life = DL.U(1.2, 1.6);
							B.Hold = 0.5;
							B.Rest = 0.0;
							B.Thin = 0;
							B.Col = Grey.Mix(DryDust(Tone), SmokeDust);
							B.Col2 = B.Col;
							Bits.push_back(B);
						}
					}
				}
				if (!bDryTall) // (with muzzle.dry.tall none of these is laid, so none of their numbers is read and none of their dice thrown: the pre-check, 07:31Z)
				{
					Dice DD(PlaceKey(At) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) ^ 0xD27Dull);
					const double DryBy = std::sqrt(std::clamp(Ring_ / 6.0, 0.0, 16.0));
					const int NDry = std::clamp(static_cast<int>(std::lround(Safe(Range(Dr + "lumps", 6.0, 9.0).Pick(DD), 0.0, 40.0, 7.0) * DryBy * DryK)), 0, 80);
					const Span SizeIs = Range(Dr + "lump_r", 0.1, 0.19), AlphaIs = Range(Dr + "alpha", 0.75, 0.92), ClearIs = Range(Dr + "clear_s", 1.7, 2.3), ShadeIs = Range(Dr + "shade", 0.86, 1.0);
					// (art director 03:30Z: the two or three nearest the muzzle hold to near_s, thinning, the cloud that gives the
					// gun away; the far ones are gone by clear_s as before. No key: none holds)
					const Span NearIs = Range(Dr + "near_s", 0.0, 0.0);
					const double DryShade = DryNum("cloud_shade", 0.0, 2.0, BillowShade), DryTwin = Safe(Num(Dr + "twin_px", 6.0), 0.0, 4096.0, 6.0);
					const double BellyIs = Safe(Num(Dr + "belly", 0.8), 0.2, 1.0, 0.8), CrownSize = Safe(Num(Dr + "crown_r", 0.7), 0.0, 1.0, 0.7), CrownUp = Safe(Num(Dr + "crown_up", 0.55), 0.0, 2.0, 0.55);
					const double WideIs = Safe(Num(Dr + "wide", 0.16), 0.0, 1.0, 0.16), ReachIs = Safe(Num(Dr + "reach", 1.05), 0.1, 3.0, 1.05);
					const Vec MiddleD = Foot + FwFlat * (Ring_ * 0.35) + ZUp * 0.8;
					for (int I = 0; I < NDry; ++I)
					{
						const double Along_ = (I + DD.U(0.15, 0.85)) / NDry, Near = 1.0 - Along_; // (one to a stretch of the line: they never pile)
						const double Heading = DD.U(0.0, 2.0 * Pi);
						const bool bHead = I % 3 == 1;
						Bit B;
						B.Born = Now + DD.U(0.0, 0.06);
						B.From = Foot + FwFlat * (Ring_ * ReachIs * (std::pow(Along_, 1.3) - 0.08)) + Across * (DD.Bell() * Ring_ * WideIs * (0.5 + Along_)) + ZUp * (bHead ? DD.U(0.3, 0.8) : 0.0);
						B.Vel = FwFlat * (DD.U(1.5, 4.0) * (0.5 + 0.5 * Near)) + Across * DD.U(-0.8, 0.8) + ZUp * (bHead ? DD.U(0.3, 1.0) : 0.0);
						B.Drag = 2.0;
						B.Rise = bHead ? DD.U(0.1, 0.3) : 0.0;
						B.Windage = 0.6;
						B.Life = Safe(ClearIs.Pick(DD), 0.2, 20.0, 2.0) * (0.75 + 0.25 * Near);
						const double Stay = Clamp01((Near - 0.55) / 0.3), NearLife = Safe(NearIs.Pick(DD), 0.0, 30.0, 0.0);
						const bool bStays = NearLife > B.Life && Stay > 0.0;
						if (bStays)
						{
							B.Life += (NearLife - B.Life) * Stay;
						}
						B.R1 = Ring_ * Safe(SizeIs.Pick(DD), 0.01, 1.0, 0.15) * (0.65 + 0.5 * Near);
						B.R0 = B.R1 * 0.3;
						B.Grow = 0.4;
						if (!bHead)
						{
							B.Along = Vec(std::cos(Heading), std::sin(Heading), 0.0);
							B.Stretch = DD.U(1.2, 1.6);
						}
						const double ShadeK = Safe(ShadeIs.Pick(DD), 0.3, 1.5, 0.95);
						B.Col = DryDust(ShadeK * (bHead ? 1.0 : BellyIs));
						B.Col2 = B.Col;
						B.Alpha = std::min(0.95, Safe(AlphaIs.Pick(DD), 0.0, 0.95, 0.85) * (0.75 + 0.25 * Near) * (bHead ? 0.8 : 1.0));
						B.FadeIn = 0.05;
						B.Hold = bStays ? 0.55 - 0.15 * Stay : 0.55; // (thick no longer than the others: it holds on by thinning)
						B.Heart = BillowHeart;
						B.Inner = BillowInner;
						B.Rough = BillowRough;
						B.LumpSeed = static_cast<uint32_t>(DD.U() * 1073741823.0);
						B.Turn = DD.U(0.0, 2.0 * Pi);
						B.Centre = MiddleD;
						B.CloudShade = DryShade;
						B.CloudR = Ring_ * 0.35;
						B.CentreRise = 0.2;
						B.Rest = BillowRest;
						B.TwinPx = DryTwin;
						B.Thin = 2;
						B.Origin = At - FwFlat * 1.5; // (the gun's body, behind its muzzle)
						Bits.push_back(B);
						if (!bHead && CrownSize > 0.0)
						{
							// (its crown: the same lump's top in the light, riding on the mound as both swell)
							Bit C = B;
							C.R0 = B.R0 * CrownSize;
							C.R1 = B.R1 * CrownSize;
							C.Rest = (BillowRest + CrownUp) / std::max(CrownSize, 0.05);
							C.Col = DryDust(ShadeK);
							C.Col2 = C.Col;
							C.Alpha = B.Alpha * 0.9;
							C.LumpSeed = static_cast<uint32_t>(DD.U() * 1073741823.0);
							C.Turn = DD.U(0.0, 2.0 * Pi);
							Bits.push_back(C);
						}
					}
				}
			}
			const double Veil = Num("muzzle.veil.by_ground." + Ground, 0.0);
			if (Veil > 0.0 && !bDry)
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

	void Battle::GunFireball(double Now, const Vec& At, const Vec& Fw, const std::string& Row)
	{
		// (the sea's night fight, 25 November. A big naval gun's flash is no tongue but a fireball: the propellant's gas,
		// rich in fuel not yet burnt, bursts into flame again where it mixes with the air a little ahead of the muzzle
		// (the secondary flash: Klingenberg and Heimerl, "Gun Muzzle Blast and Flash", AIAA Progress in Astronautics and
		// Aeronautics 139, 1992), a ball tens of metres long on a heavy gun, yellow-white at its root, orange through, its
		// edge a deep orange, gone in a fifth of a second; the blast can blow a ring of smoke that rolls on down the line
		// of fire for seconds (the 16-inch rings of the period's photographs); then the propellant's brown-grey cloud.
		// Every number is the row's: fireball_m its length along the bore, fireball_wide its width as a share of that,
		// fireball_s how long it burns, fireball_lobes its lobes, fireball_core, fireball_colour and fireball_edge its
		// colours; ring_m the smoke ring's radius grown, ring_ahead_m how far it rolls, ring_s how long it lasts,
		// ring_puffs, ring_alpha; cloud_m the smoke cloud's radius, cloud_puffs, cloud_alpha, its life smoke_s and colour
		// smoke_colour (laid in place of the row's usual smoke puffs). Its own dice, one throw a statement)
		Dice D(PlaceKey(At) * 0xA0761D6478BD642Full ^ (static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) + 0xF1BEull));
		const Vec Across = Fw.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
		const Vec Upward = Across.Cross(Fw).Unit(ZUp);
		const Rgb Smoke = Picture(Row + "smoke_colour", Picture("game.muzzle_smoke", Rgb::FromPicture(150, 140, 126)));
		const double Long = SafeNum(Num(Row + "fireball_m", 0.0), 0.0, 150.0, 0.0);
		if (Long > 0.0)
		{
			const double Wide = SafeNum(Num(Row + "fireball_wide", 0.55), 0.1, 2.0, 0.55);
			const double Life = SafeNum(Num(Row + "fireball_s", 0.15), 0.02, 0.25, 0.15); // (0.25: under the sea's far floor's 0.3 s, so a far one is still held as a flash)
			const int Lobes = static_cast<int>(SafeNum(Num(Row + "fireball_lobes", 6.0), 1.0, 24.0, 6.0));
			const Rgb Root = Picture(Row + "fireball_core", Rgb::FromPicture(255, 228, 172));
			const Rgb Body = Picture(Row + "fireball_colour", Rgb::FromPicture(244, 142, 48));
			const Rgb Rim_ = Picture(Row + "fireball_edge", Rgb::FromPicture(180, 72, 24));
			// (fireball_hold_s: the lobes nearest the muzzle (the first third of its length) burn at the root's colour,
			// white-hot, for this long before they cool, so the flash reads as the brightest thing on the screen; the look
			// review of 25 November. 0: every lobe cools from its birth, as built)
			const double HoldHot = SafeNum(Num(Row + "fireball_hold_s", 0.0), 0.0, 0.25, 0.0);
			const double Half = 0.5 * Long * Wide;
			for (int I = 0; I < Lobes; ++I)
			{
				// (lobes along the bore from an eighth of its length out, fattest in the middle; the far ones catch a moment
				// later, so it swells out from the muzzle; each drifts on a little and swells as it burns)
				const double T = (I + 0.5) / Lobes;
				const double Side = D.Bell();
				const double Lift = D.Bell();
				const double Size = D.U(0.8, 1.15);
				const double Lives = D.U(0.8, 1.2);
				const double Late = D.U();
				const double Drift = D.U(0.4, 1.0);
				Bit B;
				B.Born = Now + 0.15 * Life * T * Late;
				B.From = At + Fw * (Long * (0.12 + 0.76 * T)) + Across * (Side * 0.18 * Half) + Upward * (Lift * 0.18 * Half);
				B.Drag = 8.0;
				B.Vel = Fw * (0.25 * Long * Drift * B.Drag);
				B.Life = Life * Lives;
				B.R0 = Half * Size * (0.45 + 0.4 * std::sin(Pi * T));
				B.R1 = B.R0 * 1.3;
				B.Grow = 0.3;
				B.Col = Root.Mix(Body, 0.25 + 0.6 * T);
				B.Col2 = Body.Mix(Rim_, 0.5);
				if (HoldHot > 0.0 && T < 1.0 / 3.0)
				{
					B.Col = Root;
					B.CoolFrom = std::min(0.9, HoldHot / B.Life);
				}
				B.Alpha = 1.0;
				B.FadeIn = 0.004;
				B.Hold = 0.45;
				B.Heart = 0.95;
				B.Rim = 0.45;
				B.Inner = 0.65;
				B.Edge = Rim_;
				B.EdgeK = 0.4;
				B.Rough = 0.3;
				B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
				B.bLight = true;
				Bits.push_back(B);
			}
		}
		const double RingR = SafeNum(Num(Row + "ring_m", 0.0), 0.0, 100.0, 0.0);
		if (RingR > 0.0)
		{
			// (the smoke ring: puffs round a ring across the bore, rolling down the line of fire and widening as they slow)
			const double Ahead = SafeNum(Num(Row + "ring_ahead_m", 4.0 * RingR), 0.0, 1000.0, 4.0 * RingR);
			const double RingLife = SafeNum(Num(Row + "ring_s", 4.0), 0.2, 60.0, 4.0);
			const int NRing = static_cast<int>(SafeNum(Num(Row + "ring_puffs", 14.0), 3.0, 48.0, 14.0));
			const double RingAlpha = SafeNum(Num(Row + "ring_alpha", 0.5), 0.0, 1.0, 0.5);
			const double RingGrow = SafeNum(Num(Row + "ring_grow", 0.32), 0.05, 2.0, 0.32); // (each puff's radius grown, a share of the ring's: fat puffs read as one soft ring, thin ones as beads)
			const Rgb RingCol = Picture(Row + "ring_colour", Smoke);
			const double Start = D.U(0.0, 2.0 * Pi);
			for (int I = 0; I < NRing; ++I)
			{
				const double A = Start + 2.0 * Pi * I / NRing;
				const double Wobble = D.U(0.85, 1.15);
				const double Shade = D.U(0.9, 1.08);
				const double Thick = D.U(0.8, 1.0);
				const Vec Out_ = Across * std::cos(A) + Upward * std::sin(A);
				Bit B;
				B.Born = Now + 0.03;
				B.From = At + Fw * (0.1 * Ahead) + Out_ * (0.3 * RingR);
				B.Drag = 1.2;
				B.Vel = Fw * (0.9 * Ahead * B.Drag) + Out_ * (0.7 * RingR * B.Drag * Wobble);
				B.Rise = 0.3;
				B.Windage = 0.7;
				B.Life = RingLife * Wobble;
				B.R0 = 0.12 * RingR;
				B.R1 = RingGrow * RingR;
				B.Grow = 0.5;
				B.Col = RingCol * Shade;
				B.Col2 = B.Col;
				B.Alpha = RingAlpha * Thick;
				B.FadeIn = 0.08;
				B.Hold = 0.35;
				B.Heart = 0.6;
				B.Inner = 0.55;
				Bits.push_back(B);
			}
		}
		const double CloudR = SafeNum(Num(Row + "cloud_m", 0.0), 0.0, 200.0, 0.0);
		if (CloudR > 0.0)
		{
			// (the cloud: thrown down the line of fire, fewer and thinner the farther, swelling, climbing and drifting)
			const int NCloud = static_cast<int>(SafeNum(Num(Row + "cloud_puffs", 14.0), 1.0, 60.0, 14.0));
			const double CloudAlpha = SafeNum(Num(Row + "cloud_alpha", 0.5), 0.0, 1.0, 0.5);
			const Span Lives = Range(Row + "smoke_s", 6.0, 10.0);
			const double Reach = std::max(Long, CloudR);
			for (int I = 0; I < NCloud; ++I)
			{
				const double Far = D.U();
				const double Side = D.Bell();
				const double Lift = D.Bell();
				const double Wide = D.U(0.35, 0.6);
				const double Life = SafeNum(Lives.Pick(D), 0.2, 120.0, 8.0);
				const double Shade = D.U(0.88, 1.1);
				const double Thick = D.U(0.7, 1.0);
				const double Late = D.U(0.0, 0.08);
				Bit B;
				B.Born = Now + Late;
				B.From = At + Fw * (0.2 * Reach);
				B.Drag = 2.0;
				B.Vel = (Fw * (Reach * (0.2 + 1.1 * Far)) + Across * (Side * 0.35 * CloudR) + Upward * (Lift * 0.25 * CloudR)) * B.Drag;
				B.Rise = 0.4;
				B.Windage = 0.8;
				B.Life = Life;
				B.R0 = 0.15 * CloudR;
				B.R1 = Wide * CloudR;
				B.Grow = 0.4;
				B.Col = Smoke * Shade;
				B.Col2 = B.Col;
				B.Alpha = CloudAlpha * Thick * (1.0 - 0.4 * Far);
				B.FadeIn = 0.06;
				B.Hold = 0.15;
				B.Heart = 0.5;
				B.Inner = 0.5;
				Bits.push_back(B);
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
		if (H.Look.bOn)
		{
			LookFlame(Born, At, Fw, Len, Life, H.Look, &H, D); // (Effects (R2): its look's star or brake jets, after the rest)
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Effects (R2), -TheaterLandGunFlash: each gun's own look (Brian, 23 November: "how does the gun fire look when they
	// shoot? can we make the mussel flash realistic?"). The scene names a gun's row and its look, "mg:star"; the look's
	// numbers are muzzle.look.<look>, laid over the recipe from battle-fx-guns.json only when the switch is on. Its
	// pieces are thrown with dice of their own, after the row's, so the row's own pieces fall as they did.

	bool Battle::GunLookOf(const std::string& Look, GunLook& L) const
	{
		if (Look.empty())
		{
			return false;
		}
		const std::string P = "muzzle.look." + Look + ".";
		if (!Has(P + "on") || Num(P + "on", 0.0) <= 0.0)
		{
			return false;
		}
		L = GunLook();
		L.bOn = true;
		L.FlameM = SafeNum(Num(P + "flame_m", 0.0), 0.0, 6.0, 0.0);
		L.Narrow = SafeNum(Num(P + "narrow", 1.0), 0.2, 1.5, 1.0);
		L.LicksK = SafeNum(Num(P + "licks", 1.0), 0.0, 1.5, 1.0);
		L.bFlicker = Num(P + "flicker", 0.0) > 0.0;
		L.HotM = SafeNum(Num(P + "hot_m", 0.0), 0.0, 2.0, 0.0);
		L.FarK = SafeNum(Num(P + "far_k", 1.0), 0.2, 3.0, 1.0);
		L.bTracer = Num(P + "tracer", 1.0) > 0.0;
		L.Star = static_cast<int>(SafeNum(Num(P + "star", 0.0), 0.0, 8.0, 0.0));
		L.StarM = SafeNum(Num(P + "star_m", 0.0), 0.0, 1.5, 0.0);
		L.StarWide = SafeNum(Num(P + "star_wide", 0.12), 0.02, 0.5, 0.12);
		L.StarLean = SafeNum(Num(P + "star_lean", 75.0), 20.0, 100.0, 75.0);
		L.StarFarM = SafeNum(Num(P + "star_far_m", 0.0), 0.0, 1000.0, 0.0);
		L.Jets = Num(P + "jets", 0.0) > 0.0 ? 2 : 0;
		L.JetM = SafeNum(Num(P + "jet_m", 0.0), 0.0, 4.0, 0.0);
		L.JetBackM = SafeNum(Num(P + "jet_back_m", 0.15), 0.0, 1.0, 0.15);
		L.JetWide = SafeNum(Num(P + "jet_wide", 0.2), 0.05, 0.6, 0.2);
		L.JetDust = SafeNum(Num(P + "jet_dust", 0.0), 0.0, 1.0, 0.0);
		L.Haze = SafeNum(Num(P + "haze", 0.0), 0.0, 0.3, 0.0);
		L.HazeR = SafeNum(Num(P + "haze_r", 0.6), 0.1, 3.0, 0.6);
		L.HazeWithinM = SafeNum(Num(P + "haze_within_m", 200.0), 0.0, 1000.0, 200.0);
		L.HazeS = Range(P + "haze_s", 2.5, 4.0);
		L.HazeS.A = SafeNum(L.HazeS.A, 0.2, 20.0, 2.5);
		L.HazeS.B = SafeNum(L.HazeS.B, L.HazeS.A, 20.0, L.HazeS.A);
		L.ProneUnderM = SafeNum(Num(P + "prone_under_m", 0.0), 0.0, 1.5, 0.0);
		L.ProneDust = SafeNum(Num(P + "prone_dust", 0.0), 0.0, 0.6, 0.0);
		L.DustWithinM = SafeNum(Num(P + "dust_within_m", 150.0), 0.0, 1000.0, 150.0);
		L.LampW = SafeNum(Num(P + "lamp_w", 0.0), 0.0, 3000.0, 0.0);
		L.LampBackM = SafeNum(Num(P + "lamp_back_m", 0.4), 0.0, 1.5, 0.4);
		L.LampR = SafeNum(Num(P + "lamp_r", 3.0), 0.5, 10.0, 3.0);
		L.LampWithinM = SafeNum(Num(P + "lamp_within_m", 150.0), 0.0, 1000.0, 150.0);
		L.DayK = SafeNum(Num(P + "day_k", 1.0), 0.1, 1.0, 1.0);
		L.DayPale = SafeNum(Num(P + "day_pale", 0.0), 0.0, 1.0, 0.0);
		L.bStreak = Num(P + "streak", 0.0) > 0.0;
		return true;
	}

	void Battle::LookFlame(double Born, const Vec& At, const Vec& Fw, double Len, double Life, const GunLook& L, const HotFlash* H, Dice& D)
	{
		if (!RealPlace(At) || !(Born == Born) || !(Len == Len))
		{
			return;
		}
		const Vec Side = Fw.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
		const Vec Over_ = Side.Cross(Fw).Unit(ZUp);
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		// (one spike or jet: a thin tongue from its root at From_ out along Way, hot near its root and red-orange at its end)
		const double Pale = L.DayPale * LookDay; // (by day: paler, toward yellow-white)
		auto Tongue = [&](const Vec& From_, const Vec& Way, double Long, double HalfWide, bool bSpike)
		{
			Bit B;
			B.Born = Born;
			B.Life = Life;
			B.From = From_ + Way * (Long * 0.5);
			B.Along = Way;
			B.R0 = std::max(HalfWide, 1e-3);
			B.R1 = B.R0 * 0.7;
			B.Grow = 1.0;
			B.Stretch = std::max(1.0, Long * 0.5 / B.R0);
			B.Point = 0.35; // (blunt and ragged at its end, as gas is: a needle reads as a drawn flame)
			B.Alpha = 1.0;
			B.FadeIn = 0.002;
			B.Hold = H ? 0.8 : 0.5;
			B.bLight = true;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			if (bSpike)
			{
				// (the art director's round 1, 17:06Z: a short straight spike tapering to its tip, yellow-white, never a
				// curved petal: no lumps, no hub, drawn straight out from the muzzle)
				// (and its round 2, 17:19Z: at dusk not stark white needles in front of the flash but white at the root running
				// out to yellow tips, so they sit with it: the root's colour drawn from the muzzle end, the tip's at its point)
				B.Point = 0.85;
				B.Tail = 0.0;
				B.Col = H ? H->Hot.Mix(H->Yellow, 0.25) : Rgb::FromPicture(252, 242, 214);
				B.Col2 = B.Col;
				B.Hub = 0.9;
				B.Heart = 1.0;
				B.Rim = 0.5;
				B.Inner = 0.7;
				B.Edge = H ? H->Yellow.Mix(H->Orange, 0.3) : Flame.Mix(Rgb::FromPicture(250, 214, 120), 0.6);
				B.EdgeK = 1.0;
				B.EdgeIn = 0.6;
				if (H)
				{
					B.Over = 1.0 + (H->Over - 1.0) * 0.35;
				}
			}
			else if (H)
			{
				// (yellow at its root, running out to orange and red-orange at its end and edge)
				B.Col = H->Yellow.Mix(H->Orange, 0.25);
				B.Col2 = H->Orange;
				B.Heart = 1.0;
				B.Rim = 0.4;
				B.Inner = 0.75;
				B.Edge = H->Orange.Mix(H->Red, 0.5);
				B.EdgeK = 0.85;
				B.EdgeIn = 0.4;
				B.Rough = 0.3;
				B.Hub = 0.7; // (its root's colour at its root)
				B.Over = 1.0 + (H->Over - 1.0) * 0.2;
			}
			else
			{
				B.Col = Flame;
				B.Col2 = Flame;
				B.Heart = 0.8;
				B.Rim = 0.1;
			}
			if (Pale > 0.0)
			{
				const Rgb Day_ = Rgb::FromPicture(250, 238, 200);
				B.Col = B.Col.Mix(Day_, Pale);
				B.Col2 = B.Col2.Mix(Day_, Pale);
				B.Edge = B.Edge.Mix(Day_, Pale * 0.7);
			}
			Bits.push_back(B);
		};
		if (L.Star > 0 && L.StarM > 0.0)
		{
			// (the MG 42's star: its muzzle booster's cone throws the gas out round the bore in a few bright spikes, near
			// square to it, so a burst at night is a flickering star and not a tongue; each round's spikes are new)
			// (the art director's round 1: 4 to 6 short straight spikes evenly round the muzzle, each shot rolled anew so
			// no two show the same pair; by day shorter, with the rest of the flash)
			const int N = D.Whole(std::max(1, L.Star - 2), L.Star);
			const double Round0 = D.U(0.0, 2.0 * Pi);
			const double DayK = LookDayK(L);
			for (int I = 0; I < N; ++I)
			{
				const double Round_ = Round0 + (I + D.U(-0.1, 0.1)) * 2.0 * Pi / N;
				const double Lean = Radians(L.StarLean + D.U(-5.0, 5.0));
				const Vec Way = (Fw * std::cos(Lean) + (Side * std::cos(Round_) + Over_ * std::sin(Round_)) * std::sin(Lean)).Unit(Side);
				const double Long = L.StarM * D.U(0.75, 1.25) * DayK;
				Tongue(At + Fw * (Len * 0.04), Way, Long, Long * L.StarWide * D.U(0.85, 1.15), true);
			}
		}
		if (L.Jets > 0 && L.JetM > 0.0)
		{
			// (a muzzle brake: the gas turned out of its baffles to either side, a little back, in two flat jets level with
			// the ground, longer than the shortened flame ahead)
			for (int I = 0; I < L.Jets; ++I)
			{
				const double Sign = I == 0 ? 1.0 : -1.0;
				const double Back = Radians(D.U(5.0, 20.0));
				const Vec Way = (Side * (Sign * std::cos(Back)) - Fw * std::sin(Back) + Over_ * D.U(-0.08, 0.12)).Unit(Side * Sign);
				const double Long = L.JetM * D.U(0.8, 1.15);
				const Vec Root = At - Fw * L.JetBackM;
				Tongue(Root, Way, Long, Long * L.JetWide * D.U(0.85, 1.15), false);
				// (and a shorter second tongue off each, the brake's other baffle)
				const double Long2 = Long * D.U(0.45, 0.7);
				Tongue(Root - Fw * (L.JetBackM * 0.6), (Way - Fw * 0.25).Unit(Way), Long2, Long2 * L.JetWide * 0.9, false);
			}
		}
	}

	void Battle::PaleHot(HotFlash& H, double Pale)
	{
		if (!(Pale > 0.0))
		{
			return;
		}
		const Rgb Day_ = Rgb::FromPicture(250, 238, 200);
		const double P = std::min(1.0, Pale);
		H.Orange = H.Orange.Mix(Day_, P);
		H.Rim = H.Rim.Mix(Day_, P * 0.8);
		H.Red = H.Red.Mix(Day_, P * 0.8);
		H.Yellow = H.Yellow.Mix(Day_, P * 0.6);
		H.Hot = H.Hot.Mix(Day_, P * 0.5);
	}

	void Battle::LookLamp(double Now, const Vec& At, const Vec& Fw, const GunLook& L)
	{
		// (the flash's glow on the firer's face and hands and the ground at his elbows: a small lamp a little back from the
		// muzzle, at dusk and night only, as a small gun's lamp; never further than lamp_within_m from the eye)
		if (L.LampW <= 0.0 || !bEyeKnown || !RealPlace(At) || !(Now == Now) || !((LastEye - At).Length() <= L.LampWithinM))
		{
			return;
		}
		Flash F;
		F.At = At - Fw * L.LampBackM + ZUp * 0.05;
		F.Col = Picture("burst.flash.light.colour", Rgb::FromPicture(255, 196, 140));
		F.Born = Now;
		F.Life = 0.03;
		F.Strength = L.LampW / 8500.0;
		F.Radius = L.LampR;
		F.bNightOnly = true;
		F.bOneFrame = true; // (the art director's round 1: lit on the flash's frame alone)
		Flashes.push_back(F);
	}

	void Battle::LookLaid(double Now, const Vec& At, const Vec& Fw, const GunLook& L, const char* GroundIn)
	{
		if (!bEyeKnown || !RealPlace(At) || !(Now == Now))
		{
			return;
		}
		const double Far = (LastEye - At).Length();
		Dice D(PlaceKey(At) * 0xA0761D6478BD642Full ^ (static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) + 0x6A1ull));
		const std::string Ground = GroundIn && *GroundIn && Has(std::string("grounds.") + GroundIn + ".paint.soil.0") ? GroundIn : "earth";
		const Vec FwFlat = Vec(Fw.X, Fw.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		const Vec Across = FwFlat.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
		const double Foot = Height(At.X, At.Y);
		if (L.Haze > 0.0 && Far <= L.HazeWithinM)
		{
			// (the rifle's haze: a pale grey-blue puff left at the muzzle that hangs, widens and drifts off on the wind, so
			// a firing line is soon under a thin veil of its own smoke)
			const Rgb Pale = Picture("muzzle.look.haze_colour", Rgb::FromPicture(196, 198, 200));
			for (int I = 0; I < 2; ++I)
			{
				Bit B;
				B.Born = Now + D.U(0.0, 0.04);
				B.From = At + Fw * D.U(0.2, 0.6) + ZUp * D.U(0.0, 0.1);
				B.Vel = Fw * D.U(0.6, 1.6) + Vec(D.Bell() * 0.2, D.Bell() * 0.2, D.U(0.05, 0.25));
				B.Drag = 2.0;
				B.Rise = 0.08;
				B.Windage = 1.0;
				B.Life = L.HazeS.Pick(D);
				B.R0 = 0.08;
				B.R1 = L.HazeR * D.U(0.75, 1.2);
				B.Grow = 0.5;
				B.Col = Pale * D.U(0.95, 1.05);
				B.Col2 = B.Col;
				B.Alpha = L.Haze * (I == 0 ? 0.65 : 0.4);
				B.FadeIn = 0.08;
				B.Hold = 0.35;
				B.Heart = 0.6;
				B.Rim = 0.0;
				B.Rough = 0.3;
				B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
				Bits.push_back(B);
			}
		}
		const double Up = At.Z - Foot;
		const double Share = Clamp01(DustRowOf(Ground).Share);
		const Rgb Dust = PaintOf(Ground).Dust;
		// (the art director's round 1, 17:06Z: the kick is the real giveaway: a tan puff 0.5 to 1 m at the muzzle on dry
		// ground, white powder on snow, none on wet ground; the ground's dust share only between half and full)
		const std::string GroundIs = GroundIn ? GroundIn : "";
		const bool bWet = GroundIs == "mud" || GroundIs == "water" || GroundIs == "marsh";
		const bool bSnowKick = GroundIs == "snow";
		const double KickK = bWet ? 0.0 : bSnowKick ? 0.85 : 0.5 + 0.5 * Share;
		const Rgb KickCol = bSnowKick ? Picture("muzzle.look.snow_kick", Rgb::FromPicture(232, 235, 240)) : Dust;
		if (L.ProneDust > 0.0 && Up < L.ProneUnderM && Far <= L.DustWithinM && KickK > 0.0)
		{
			// (a man firing prone, or a gun on its bipod: the blast blows the ground's dust up from under and in front of
			// the muzzle, a low brown puff that gives him away; more on dry ground, little on mud, none on water)
			const int N = 3;
			for (int I = 0; I < N; ++I)
			{
				Bit B;
				B.Form = Bit::Shape::Upright;
				B.bHug = true;
				B.Born = Now + D.U(0.0, 0.03);
				const Vec P = At + FwFlat * D.U(0.2, 0.7) + Across * (D.Bell() * 0.15);
				B.From = Vec(P.X, P.Y, Height(P.X, P.Y) + 0.05);
				B.Vel = (FwFlat * D.U(1.0, 2.2) + Across * (D.Bell() * 0.6)) + ZUp * D.U(0.3, 0.7);
				B.Drag = 3.5;
				B.Windage = 0.8;
				B.Life = D.U(1.2, 2.0);
				B.R0 = 0.12;
				B.R1 = D.U(0.28, 0.5);
				B.Grow = 0.3;
				B.Flatten = 0.75;
				B.Col = KickCol * D.U(0.94, 1.04);
				B.Col2 = B.Col;
				B.Alpha = std::min(0.9, L.ProneDust * KickK * (I == 0 ? 1.0 : 0.75));
				B.FadeIn = 0.02;
				B.Hold = 0.35;
				B.Heart = 0.7;
				B.Rim = 0.0;
				B.Rough = 0.35;
				B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
				Bits.push_back(B);
			}
		}
		if (L.Jets > 0 && L.JetDust > 0.0 && Up < 3.5 && Far <= L.DustWithinM * 2.0 && Share > 0.0)
		{
			// (a muzzle brake low over the ground: its side jets blow the dust out to either side of the gun in two low
			// fans, as well as the row's ring ahead)
			for (int Sd = 0; Sd < 2; ++Sd)
			{
				const double Sign = Sd == 0 ? 1.0 : -1.0;
				for (int I = 0; I < 5; ++I)
				{
					Bit B;
					B.Form = Bit::Shape::Upright;
					B.bHug = true;
					B.Born = Now + D.U(0.0, 0.05);
					const Vec P = At - FwFlat * L.JetBackM + Across * (Sign * D.U(0.3, 1.2));
					B.From = Vec(P.X, P.Y, Height(P.X, P.Y) + 0.1);
					B.Vel = Across * (Sign * D.U(4.0, 9.0)) - FwFlat * D.U(0.0, 1.5) + ZUp * D.U(0.3, 1.2);
					B.Drag = 2.5;
					B.Windage = 0.8;
					B.Rise = 0.1;
					B.Life = D.U(2.5, 4.5);
					B.R0 = 0.3;
					B.R1 = D.U(1.2, 2.2);
					B.Grow = 0.35;
					B.Flatten = 0.55;
					B.Col = Dust * D.U(0.92, 1.05);
					B.Col2 = B.Col;
					B.Alpha = std::min(0.8, L.JetDust * Share * D.U(0.7, 1.0));
					B.FadeIn = 0.03;
					B.Hold = 0.3;
					B.Heart = 0.75;
					B.Inner = 0.6;
					B.Rim = 0.0;
					B.Rough = 0.35;
					B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
					Bits.push_back(B);
				}
			}
		}
		LookLamp(Now, At, Fw, L);
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
		GroundIn = GroundOn(GroundIn); // (a ground whose own switch is off is drawn as earth)
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
		// (FX5, room shares by kind, the art director's ruling (a) of 18:12Z: the bursts, their lingering smoke and the
		// fires take the room first, RoomGate, and the dust trails give way. Never by stopping: a tank on sand with no
		// dust looks parked. Each far trail lays one puff in ShareThin of its own, each that much longer along the
		// track and a little softer, so it is the same line in fewer, bigger pieces, never beads; the farthest thin
		// first, and a vehicle within dust_full_m of the eye keeps its whole trail)
		double ShareK = 1.0;
		if (ShareDustFloor >= 0.0)
		{
			const double Away = bEyeKnown ? (LastEye - At).Length() : 0.0;
			const double Over = Clamp01((Away - ShareDustFullM) / std::max(1.5 * ShareDustFullM, 1.0));
			ShareK = 1.0 + (ShareThin - 1.0) * Over * Over * (3.0 - 2.0 * Over);
			// (the art director's ruling of 18:31Z, the heaviest barrage: when thinning is not enough, far trails past
			// dust_full_m are let go before any far burst loses a piece, the farthest first. A trail let go lays nothing
			// for at least drop_hold_s, so it never flickers; what it laid fades over its own life, never cut; then it
			// comes back from its next puff, which fades in)
			if (Now < Du->DropUntil && Now > Du->DropUntil - 60.0)
			{
				Du->Owed = 0.0;
				return;
			}
			if (ShareDrop > 0.0 && Away > ShareDustFullM)
			{
				const double Lot = static_cast<double>(Mix64(static_cast<uint64_t>(Id) * 0xD1B54A32D192ED03ull + 0x3C6EF372FE94F82Bull) >> 11) * (1.0 / 9007199254740992.0);
				if (0.5 * (Lot + 1.0 - Over) < ShareDrop) // (the farthest first: past 2.5 dust_full_m, half of them by 0.25)
				{
					Du->DropUntil = Now + ShareDropHoldS;
					Du->Owed = 0.0;
					return;
				}
			}
		}
		if (Row.Share <= 0.0 || Speed < Row.LeastMps || Speed < 1e-6 || (ShareDustFloor < 0.0 && Busy > 0.85))
		{
			Du->Owed = 0.0;
			return;
		}
		// (a puff every spacing_m along its path, no more than most_a_second; when the screen is filling, half as many)
		const double Fast = Clamp01((Speed - Row.LeastMps) / (Row.FullMps - Row.LeastMps));
		double Spacing = std::max(Row.Spacing, Speed / Row.MostPerS);
		double Wider = 1.0, ShareBase = Spacing;
		if (ShareDustFloor >= 0.0)
		{
			// (never so thin that a trail has fewer than dust_least_alive puffs alive along it, or it would come and
			// go; and far off, a vehicle's two dust sockets, Id 2K and 2K+1 as the land battle calls them, become one
			// trail a little wider: the right one's puffs go as the vehicle thins, the left one's widen)
			const double LivesMid = 0.25 * ((Row.Life.A + Row.Life.B) * (1.0 - Fast) + (Row.LifeFast.A + Row.LifeFast.B) * Fast);
			ShareK = std::clamp(std::min(ShareK, LivesMid * Speed / (ShareDustLeastAlive * Spacing)), 1.0, ShareDustMostThin);
			const double Merged = Clamp01(ShareK - 1.0);
			if (Merged > 0.0 && (Id & 1) != 0)
			{
				const double Tick = Now * 30.0;
				const int64_t TickN = Tick == Tick && std::fabs(Tick) < 1.0e15 ? std::llround(Tick) : 0;
				Dice Coin(Mix64(static_cast<uint64_t>(Id) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(TickN)));
				if (Coin.U() < Merged)
				{
					Du->Owed = 0.0;
					return;
				}
			}
			Wider = (Id & 1) == 0 ? 1.0 + 0.4 * Merged : 1.0;
			ShareBase = Spacing;
			Spacing *= ShareK;
		}
		else if (Busy > 0.6)
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
			B.R1 = (Row.R1.Pick(D) * (1.0 - Fast) + Row.R1Fast.Pick(D) * Fast) * Big * Wider;
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
			if (ShareK > 1.0)
			{
				// (FX5: a thinned trail's puff reaches back over the gap to the last one kept, as much longer along the
				// track, and as much denser as the puffs it stands for overlapped, so the line reads as dense as before;
				// a little softer and wider, as the far bursts' fewer pieces are)
				const double Soft = std::min(1.0, (ShareK - 1.0) / 3.0);
				const double Was = B.Stretch;
				B.Stretch = Was + (ShareK - 1.0) * ShareBase / std::max(2.0 * B.R1, 0.1);
				const double Stood = ShareK * Was / B.Stretch;
				B.Alpha = std::min(B.Alpha >= 0.9 ? B.Alpha : 0.9, 1.0 - std::pow(1.0 - Clamp01(B.Alpha), std::max(Stood, 1.0)));
				B.HazeAlpha = std::min(B.Alpha, 1.0 - std::pow(1.0 - Clamp01(B.HazeAlpha), std::max(Stood, 1.0)));
				B.Heart *= 1.0 - 0.25 * Soft;
				B.Inner *= 1.0 - 0.2 * Soft;
				B.R1 *= 1.0 + 0.15 * Soft;
			}
			Bits.push_back(B);
			++DustSince;
		}
		Du->Owed = std::fmod(Du->Owed + Long, Spacing);
	}

	void Battle::Shots(double Now, int Gun, const Vec& From, const Vec& To, const char* Weapon, int Rounds, double PerSecond, const char* GroundIn)
	{
		GroundIn = GroundOn(GroundIn); // (a ground whose own switch is off is drawn as earth)
		// (Effects (R2): "mg:star", the row mg with its own look; a name with no colon as ever)
		std::string WeaponBase, LookName;
		Weapon = LookSplit(Weapon, WeaponBase, LookName);
		GunLook GL;
		const bool bLook = GunLookOf(LookName, GL);
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
		const bool bFlicker = Weapon && (std::string(Weapon) == "mg" || (bLook && GL.bFlicker));
		HotFlash HotMg;
		const bool bHotMg = bFlicker && HotOf(From, HotMg);
		if (bLook && bHotMg)
		{
			// (Effects (R2): each round's close flame by the look: its cone, its licks, its star; no brake on a small arm)
			HotMg.Wide = std::clamp(HotMg.Wide * GL.Narrow, 0.02, 1.0);
			HotMg.Licks.A *= GL.LicksK;
			HotMg.Licks.B *= GL.LicksK;
			HotMg.Look = GL;
			HotMg.Look.Jets = 0;
			PaleHot(HotMg, GL.DayPale * LookDay); // (by day paler, and smaller below)
		}
		const double LookK = bLook ? LookDayK(GL) : 1.0;
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
		const double MgFlame = (bLook && GL.HotM > 0.0 ? GL.HotM : Num("muzzle.hot.mg_flame_m", 0.5)) * LookK;
		// (Effects (R2): the look's far star and its kicks of dust through a long burst have dice of their own)
		Dice DL(PlaceKey(From) * 0x8EBC6AF09C88C6E3ull ^ (static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) + 0x6A3ull));
		GunLook Kick = GL;
		Kick.Haze = 0.0;
		Kick.LampW = 0.0;
		Kick.Jets = 0;
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
			if (bLook && R > 0)
			{
				// (Effects (R2): each round's glow on the gunner at dusk, and through a long burst the low muzzle's dust
				// kicked up again every fourth round; the first round's are the muzzle's own, LookLaid)
				LookLamp(T, From, Dir, GL);
				if (R % 4 == 0)
				{
					LookLaid(T, From, Dir, Kick, GroundIn);
				}
			}
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
				B.R0 = B.R1 = 0.12 * (bLook && !bHotMg ? GL.FarK * LookK : 1.0); // (Effects (R2): the look's far flicker, by day smaller)
				if (bLook && !bHotMg && GL.Star > 0 && (LastEye - From).Length() <= GL.StarFarM)
				{
					GunLook Far_ = GL;
					Far_.Jets = 0;
					LookFlame(T, From, Dir, 0.3, 0.025, Far_, nullptr, DL);
				}
				B.Along = Dir;
				B.Stretch = 2.0;
				B.Point = 0.8;
				B.Col = Flame;
				B.Col2 = Flame;
				if (bLook && !bHotMg && GL.DayPale * LookDay > 0.0)
				{
					// (Effects (R2): by day the far flicker paler too)
					B.Col = B.Col.Mix(Rgb::FromPicture(250, 238, 200), GL.DayPale * LookDay);
					B.Col2 = B.Col;
				}
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
			if (Which % OneIn != 0 || (bLook && !GL.bTracer)) // (Effects (R2): a look with no tracer, a sub-machine gun's)
			{
				continue;
			}
			const Vec Aim = (Dir + Vec(D.Bell(), D.Bell(), D.Bell()) * 0.006).Unit();
			if (Dist < WholePathBelow && !(bLook && GL.bStreak)) // (Effects (R2): a look's tracer a travelling streak even close in)
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

	void Battle::AirTracer(double Born, const Vec& Start, const Vec& Way, const Vec& Vel, const AirRow& Row, const TracerHue& Hue, bool bOneFan)
	{
		// Art director 15:02Z (air-fates-look, D) for Brian's "more real istic chaser fire": a hot core in a modest glow,
		// alight 1.2 s, a streak a thirtieth of a second of its flight long (about 30 m for a .50) that shortens as the
		// round slows, dimming over its last third and out in two or three frames; a cannon's half as wide again. It
		// flies in the world, at the gun's speed and the aircraft's own, slowing and falling, so a moving bomber's
		// streams bend back like water from a hose. 15:38Z (G2): each nation's own colours; the yellow retires.
		const double StreakS = Row.StreakS > 0.0 ? Row.StreakS : AirStreakS;
		const double Half = SafeNum(0.5 * StreakS * Vel.Length(), 0.5, 60.0, TracerHalf);
		const double Life = BurnLife(Row, Vel.Length()); // (fx-air r2: to its row's burn_out_m where it has one; else as before)
		// (bOneFan, a gun far from the eye: the glow alone, a little toward the core's colour. Its core was a pixel there,
		// and a tracer costs half as much)
		for (int Layer = 0; Layer < (bOneFan ? 1 : 2); ++Layer)
		{
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.Born = Born;
			B.From = Start + Way * Half;
			B.Vel = Vel;
			B.Drag = Row.Drag;
			B.Gravity = Row.Drop;
			B.Life = Life;
			B.HalfLength = Half;
			B.BySpeed = StreakS;
			B.Origin = Start;
			B.bFired = true;
			B.Along = Way;
			B.R0 = B.R1 = (Layer == 0 ? 0.045 : 0.018) * Row.TracerWide;
			// (its width is its own, in the pixels the scene tells the kit: the air scene draws 1072 across and tells the
			// kit its window's 2881, so the ruled streak's 3.4 px came out at 1.3 drawn, a hairline that the film lost)
			const double OwnPx = (Layer == 0 ? AirBodyPx : AirCorePx) * Row.TracerWide;
			B.LeastPx = OwnPx > 0.0 ? 0.75 * OwnPx : (Layer == 0 ? TracerLeast : TracerLeast * 0.4) * Row.TracerWide;
			B.MostPx = OwnPx > 0.0 ? OwnPx : (Layer == 0 ? TracerMost : TracerMost * 0.4) * Row.TracerWide;
			B.DayPx = OwnPx > 0.0 ? OwnPx : (Layer == 0 ? TracerDayPx : TracerDayCorePx) * Row.TracerWide;
			B.Col = Layer == 0 ? Hue.Body : Hue.Core;
			B.Col2 = B.Col;
			B.Over = Layer == 0 ? Hue.BodyOver : Hue.CoreOver;
			const bool bDash = bOneFan && AirFarPx > 0.0;
			if (bDash)
			{
				// (art director 19:52Z: a short far tracer is a solid dash, never an outlined oval. The core's own colour and
				// heat, so it stands off a pale sky, one_fan_px wide)
				const double FarPx = AirFarPx * Row.TracerWide;
				B.LeastPx = 0.75 * FarPx;
				B.MostPx = B.DayPx = FarPx;
				B.Col = Hue.Core;
				B.Col2 = B.Col;
				B.Over = Hue.CoreOver;
			}
			else if (bOneFan)
			{
				B.Col = Hue.Body.Mix(Hue.Core, 0.4);
				B.Col2 = B.Col;
				B.Over = Hue.BodyOver + 0.4 * (Hue.CoreOver - Hue.BodyOver);
			}
			B.bOwnDay = true;
			B.Alpha = Layer == 0 && !bOneFan ? (AirGlowSoft > 0.0 ? AirGlowSoft : 0.8) : 1.0;
			B.FadeIn = 0.001;
			B.Hold = Life > AirOutS ? 1.0 - AirOutS / Life : 0.9; // (out over out_s, two or three frames)
			B.DimFrom = std::min(AirDimFrom, B.Hold);
			B.DimTo = AirDimTo;
			B.Heart = 0.9;
			B.Rim = Layer == 0 ? 0.2 : 0.5;
			if (Layer == 1 && AirCoreInner > 0.0)
			{
				B.Inner = AirCoreInner; // (the core melts into its glow: hottest along its middle)
			}
			B.Point = 0.3;
			B.Tail = 0.7;
			B.bLight = true;
			B.Glow = static_cast<uint8_t>(bDash ? 2 : Layer + 1);
			B.Nearer = Layer == 1 && B.DayPx > 0.0 ? 0.01 : 0.0;
			if (Row.BurnOut > 0.0 || Row.TailFade > 0.0 || Row.CoreTailFade > 0.0)
			{
				// (fx-air r2, a ship's tracer: see Bit::BurnOut. It dwindles by how far it has flown, not by its age)
				B.BurnOut = Row.BurnOut;
				B.DwindleFrom = Row.DwindleFrom;
				B.DwindleTo = Row.DwindleTo;
				B.DwindleWide = Row.DwindleWide;
				B.TailFade = Layer == 0 ? Row.TailFade : Row.CoreTailFade;
				if (Row.BurnOut > 0.0)
				{
					B.DimTo = 1.0;
				}
			}
			Bits.push_back(B);
		}
	}

	const Battle::TracerHue* Battle::HueOf(const char* Nation, bool bCannon, bool bShip)
	{
		if (!bAirTracers)
		{
			return nullptr;
		}
		// (a nation the recipe does not know, or none: the look's default. A cannon takes its nation's cannon colours
		// where it has them: the Soviet 20 mm burns red where their machine guns burn green)
		const std::string N = "air_guns.tracers.nations.";
		std::string Name;
		for (const char* C = Nation; C && *C && Name.size() < 16; ++C)
		{
			Name += *C;
		}
		if (Name.empty() || !Has(N + Name + ".body.0"))
		{
			Name = "default";
		}
		if (bShip && Has(N + Name + ".ship.body.0"))
		{
			Name += ".ship"; // (fx-air r2: a ship gun's, in its nation's ship colours)
		}
		else if (bCannon && Has(N + Name + ".cannon.body.0"))
		{
			Name += ".cannon";
		}
		for (const TracerHue& H : TracerHues)
		{
			if (H.Name == Name)
			{
				return &H;
			}
		}
		const std::string P = N + Name + ".";
		TracerHue H;
		H.Name = Name;
		H.Body = Picture(P + "body", Rgb::FromPicture(255, 220, 127));
		H.Core = Picture(P + "core", Rgb::FromPicture(255, 225, 131));
		H.BodyOver = SafeNum(Num(P + "body_over", 1.0), 1.0, 4.0, 1.0);
		H.CoreOver = SafeNum(Num(P + "core_over", 1.0), 1.0, 4.0, 1.0);
		TracerHues.push_back(H);
		return &TracerHues.back();
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
			// (each held to a sane span, the pre-check of 22 November: a value that is no number, infinite or huge drew
			// no-number points and lamps through the ships' AA rows; a sane value is as it was)
			R.Rate = SafeNum(Num(P + "rate_per_min", 800.0), 1.0, 60000.0, 800.0);
			R.Mps = SafeNum(Num(P + "muzzle_mps", 870.0), 1.0, 5000.0, 870.0);
			R.Flash = SafeNum(Num(P + "flash_m", 0.45), 0.0, 50.0, 0.45);
			R.Strike = SafeNum(Num(P + "strike_m", 0.8), 0.0, 50.0, 0.8);
			R.Drag = SafeNum(Num(P + "drag", 0.36), 0.0, 100.0, 0.36);
			R.Drop = Num(P + "drop", 1.0) > 0.0 ? SafeNum(Num("air_guns.gravity", 9.81), 0.0, 100.0, 9.81) : 0.0;
			R.TracerS = SafeNum(Num(P + "tracer_s", 2.4), 0.0, 60.0, 2.4);
			R.Spread = SafeNum(Num(P + "spread_mil", 4.0), 0.0, 1000.0, 4.0) * 0.001;
			R.OneIn = static_cast<int>(SafeNum(Num(P + "tracer_every", Num("tracer.one_in", 5.0)), 1.0, 1.0e6, 5.0));
			R.SmokeEvery = static_cast<int>(SafeNum(Num(P + "smoke_every", 3.0), 1.0, 1.0e6, 3.0));
			R.SmokeS = SafeNum(Num(P + "smoke_s", 0.8), 0.0, 60.0, 0.8);
			R.SmokeAlpha = SafeNum(Num(P + "smoke_alpha", 0.1), 0.0, 1.0, 0.1);
			R.Licks = static_cast<int>(SafeNum(Num(P + "licks", 0.0), 0.0, 16.0, 0.0)); // (held to 16: a wild recipe value asked a billion licks a shot, the fuzz of 22 November)
			R.Lamp = SafeNum(Num(P + "lamp", 0.0), 0.0, 100.0, 0.0);
			R.FlashLeastPx = SafeNum(Num(P + "flash_least_px", Num("air_guns.flash_least_px", 0.0)), 0.0, 100.0, 0.0);
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
			R.bCannon = Num(P + "cannon", 0.0) > 0.0;
			R.TracerWide = SafeNum(Num(P + "tracer_wide", 1.0), 0.25, 4.0, 1.0);
			R.TracerLifeK = SafeNum(Num(P + "tracer_life_k", 1.0), 0.1, 4.0, 1.0);
			R.StreakS = SafeNum(Num(P + "streak_s", 0.0), 0.0, 0.2, 0.0);
			// (fx-air r2, a ship's light guns: see AirRow::Hose)
			R.Hose = SafeNum(Num(P + "hose", 0.0), 0.0, 1.0, 0.0);
			R.HosePx = SafeNum(Num(P + "hose_px", 0.0), 0.0, 64.0, 0.0);
			R.HoseMix = SafeNum(Num(P + "hose_mix", 0.0), 0.0, 1.0, 0.0);
			R.HoseS = SafeNum(Num(P + "hose_s", 0.0), 0.0, 2.0, 0.0);
			R.bShipHue = SafeNum(Num(P + "ship_hue", 0.0), 0.0, 1.0, 0.0) > 0.5;
			R.BurnOut = SafeNum(Num(P + "burn_out_m", 0.0), 0.0, 20000.0, 0.0);
			R.DwindleFrom = SafeNum(Num(P + "dwindle_from_m", 0.0), 0.0, 20000.0, 0.0);
			R.DwindleTo = SafeNum(Num(P + "dwindle_to", 1.0), 0.0, 1.0, 1.0);
			R.DwindleWide = SafeNum(Num(P + "dwindle_wide", 1.0), 0.1, 1.0, 1.0);
			R.TailFade = SafeNum(Num(P + "tail_fade", 0.0), 0.0, 1.0, 0.0);
			R.CoreTailFade = SafeNum(Num(P + "core_tail_fade", 0.0), 0.0, 1.0, 0.0);
		}
		R.Name = Kind;
		AirRows.push_back(R);
		return AirRows.back();
	}

	void Battle::AirGun(double Now, int Gun, const Vec& Muzzle, const Vec& Aim, const Vec& Carry, const char* Kind, double RatePerMin, double MuzzleMps, const char* Nation)
	{
		// Brian 19:35Z: "make the gun fire very realistic and the tracer fire too". Each gun fires at its own rate
		// (no two quite alike, so wing guns drift out of step), from wherever the muzzle was at the moment of each
		// round; a round flies at the gun's speed plus the aircraft's own, slows in the air and falls. The tracer is
		// the ruled streak, one in five.
		Gun = std::max(0, Gun);
		const AirRow& Row = AirRowOf(Kind && *Kind ? Kind : "hmg");
		const TracerHue* Hue = HueOf(Nation, Row.bCannon, Row.bShipHue);
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
			F->bHose = false; // (fx-air r2: a new burst starts its own hose)
		}
		if (FarGunFromM > 0.0 && Hue && bEyeKnown && !((LastEye - Muzzle).Length() <= FarGunFromM))
		{
			// (the far form, game.air_far: no rounds flown past guns_from_m, one dash along the aim every guns_dash_s in
			// the tracer's colour; the gun takes up its rounds again where the dashes leave off)
			for (int Dashed = 0; F->Next <= Now && Dashed < 8; ++Dashed)
			{
				const double K = Now - F->Last > 1e-9 ? Clamp01((F->Next - F->Last) / (Now - F->Last)) : 1.0;
				FarGunDash(F->Next, F->Muzzle + (Muzzle - F->Muzzle) * K, (F->Dir + (Dir - F->Dir) * K).Unit(Dir), F->Carry + (Carry - F->Carry) * K, Row, Mps, *Hue);
				F->Next += FarGunDashS;
			}
			F->Next = std::max(F->Next, Now);
			F->Last = Now;
			F->Muzzle = Muzzle;
			F->Dir = Dir;
			F->Carry = Carry;
			return;
		}
		const double Between = Now - F->Last;
		for (int Fired = 0; F->Next <= Now && Fired < 64; ++Fired)
		{
			const double K = Between > 1e-9 ? Clamp01((F->Next - F->Last) / Between) : 1.0;
			const Vec At = F->Muzzle + (Muzzle - F->Muzzle) * K;
			const Vec Way = (F->Dir + (Dir - F->Dir) * K).Unit(Dir);
			const Vec By = F->Carry + (Carry - F->Carry) * K;
			const int Which = F->Count++;
			AirRound(F->Next, At, Way, By, Row, Mps, Which % Row.OneIn == 0, Which % Row.SmokeEvery == 0, D, Hue);
			if (Row.Hose > 0.0)
			{
				HoseLink(F->Next, *F, At, Way * Mps + By, Row, Hue, Gap); // (fx-air r2: the hose back to the last round)
			}
			F->Next += Gap;
		}
		F->Next = std::max(F->Next, Now); // (a long stall: the rounds of the gap are not made up)
		F->Last = Now;
		F->Muzzle = Muzzle;
		F->Dir = Dir;
		F->Carry = Carry;
	}

	void Battle::FarGunDash(double T, const Vec& At, const Vec& Way, const Vec& Carry, const AirRow& Row, double Mps, const TracerHue& Hue)
	{
		// (a far gun's burst, game.air_far: a solid dash in the tracer core's colour, guns_dash_flight_s of a round's
		// flight long, from just off the muzzle along the aim, carried with the aircraft; as wide as the far tracer's
		// dash, one_fan_px, or the glow's own width)
		const double Half = SafeNum(0.5 * FarGunFlightS * std::max(Mps, 1.0), 1.0, 2000.0, 50.0);
		const double OwnPx = (AirFarPx > 0.0 ? AirFarPx : (AirBodyPx > 0.0 ? AirBodyPx : TracerLeast)) * Row.TracerWide;
		Bit B;
		B.Form = Bit::Shape::Streak;
		B.Born = T;
		B.From = At + Way * (Half * 1.2);
		B.Vel = Carry;
		B.Life = FarGunDashS * 1.25;
		B.HalfLength = Half;
		B.Along = Way;
		B.R0 = B.R1 = 0.045 * Row.TracerWide;
		B.LeastPx = 0.75 * OwnPx;
		B.MostPx = B.DayPx = OwnPx;
		B.Col = B.Col2 = Hue.Core;
		B.Over = Hue.CoreOver;
		B.bOwnDay = true;
		B.Alpha = 1.0;
		B.FadeIn = 0.01;
		B.Hold = 0.8;
		B.Heart = 0.9;
		B.Rim = 0.5;
		B.Point = 0.3;
		B.Tail = 0.7;
		B.bLight = true;
		B.Glow = 2;
		Bits.push_back(B);
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

	// ---------------------------------------------------------------------------------------------------------------
	// fx-air r2: a ship's light guns as hoses of tracer that burn out at their reach, and a burning ship's column that
	// pales as it climbs

	void Battle::HoseLink(double T, Firing& F, const Vec& At, const Vec& Vel, const AirRow& Row, const TracerHue* Hue, double Gap)
	{
		// (the art director on the air scene's 06b: a ship's light AA read as a ladder of dashes, where the film shows
		// each mount's fire as a hose of tracer curving across the sky. Each round of a row with hose over 0 lays a thin
		// line back to the round fired before it, drawn where both are now, so a burst is one line through its rounds,
		// bending as the gun swings and the rounds slow and drop, its tracers bright along it. A new burst, or a round
		// more than two and a half gaps after the last, starts a new hose: hoses with gaps between bursts, as the
		// historian's table has them (ship-air-defence.md B2))
		// (hose_s: the hose's points no closer than that in time, so a fast gun's hose is laid through every second or
		// third round, at half or a third the cost)
		if (F.bHose && T > F.HoseBorn && T - F.HoseBorn < Row.HoseS)
		{
			return;
		}
		const bool bJoin = Hue != nullptr && F.bHose && T > F.HoseBorn && T - F.HoseBorn <= 2.5 * std::max(Gap, Row.HoseS) && RealPlace(At) && RealPlace(F.HoseAt);
		const Vec WasAt = F.HoseAt, WasVel = F.HoseVel;
		const double WasBorn = F.HoseBorn;
		F.HoseAt = At;
		F.HoseVel = Vel;
		F.HoseBorn = T;
		F.bHose = true;
		if (!bJoin || !(Vel.Length() < 1.0e5) || !(WasVel.Length() < 1.0e5))
		{
			return;
		}
		// (it goes when the older round burns out. No more than hose_most at once; and as the tracers' room fills, the
		// guns farthest from the eye lose their hoses first, as they lose their tracers)
		const double Life = BurnLife(Row, WasVel.Length()) - (T - WasBorn);
		if (!(Life > 0.01))
		{
			return;
		}
		HoseEnds.erase(std::remove_if(HoseEnds.begin(), HoseEnds.end(), [T](double End) { return !(End > T) || End > T + 40.0; }), HoseEnds.end());
		if (HoseEnds.size() >= static_cast<size_t>(std::max(HoseMost, 0)))
		{
			return;
		}
		const double Fill = AirTracerMost > 0 ? static_cast<double>(AirTracerEnds.size()) / AirTracerMost : 1.0;
		if (Fill > 0.7 && !(bEyeKnown && (LastEye - At).Length() <= AirTracerNear))
		{
			return;
		}
		HoseEnds.push_back(T + Life);
		Bit B;
		B.Form = Bit::Shape::Streak;
		B.Hose = Row.Hose;
		B.Born = T;
		B.Life = Life;
		B.From = At;
		B.Vel = Vel;
		B.Drag = Row.Drag;
		B.Gravity = Row.Drop;
		B.Origin = At;
		B.Back.From = WasAt;
		B.Back.Vel = WasVel;
		B.Back.Born = WasBorn;
		// (how long it may stand on the screen, for OffScreen: the two rounds' gap now, and room for the gun's swing)
		const double Go = B.Drag > 1e-6 ? (1.0 - std::exp(-B.Drag * (T - WasBorn))) / B.Drag : T - WasBorn;
		const double Gone = (WasAt + WasVel * Go + Vec(0.0, 0.0, -0.5 * B.Gravity * (T - WasBorn) * (T - WasBorn)) - At).Length();
		B.HalfLength = (Gone == Gone ? std::min(Gone, 5000.0) : 0.0) + 50.0;
		B.R0 = B.R1 = 0.02;
		B.LeastPx = B.MostPx = B.DayPx = std::max(Row.HosePx, 0.5);
		B.Col = Hue->Body.Mix(Hue->Core, Row.HoseMix);
		B.Col2 = B.Col;
		B.Over = Hue->BodyOver + Row.HoseMix * (Hue->CoreOver - Hue->BodyOver);
		B.bOwnDay = true;
		B.Glow = 1;
		B.bLight = true;
		B.Alpha = 1.0;
		B.FadeIn = 0.001;
		B.Hold = Life > AirOutS ? 1.0 - AirOutS / Life : 0.9;
		B.Heart = 1.0;
		B.Rim = 0.2;
		B.Inner = 0.3;
		B.BurnOut = Row.BurnOut;
		B.DwindleFrom = Row.DwindleFrom;
		B.DwindleTo = Row.DwindleTo;
		B.DwindleWide = Row.DwindleWide;
		Bits.push_back(B);
	}

	bool Battle::HoseNow(const Bit& B, double Now, Bit& Out_) const
	{
		// (a hose's line as it is now: from its round to the round before, each where its own flight has it, laid as one
		// plain streak between them. Nothing where either end is no place, or the two have come together)
		const double Age = Now - B.Born, AgeWas = Now - B.Back.Born;
		if (!(Age >= 0.0) || !(Age <= B.Life) || !(AgeWas >= Age))
		{
			return false;
		}
		auto Flight = [&B](const Vec& From_, const Vec& Vel_, double Age_)
		{
			const double Go = B.Drag > 1e-6 ? (1.0 - std::exp(-B.Drag * Age_)) / B.Drag : Age_;
			return From_ + Vel_ * Go + Vec(0.0, 0.0, -0.5 * B.Gravity * Age_ * Age_);
		};
		const Vec Near = Flight(B.From, B.Vel, Age), Far = Flight(B.Back.From, B.Back.Vel, AgeWas);
		const Vec Run = Far - Near;
		const double Long = Run.Length();
		if (!(Long > 0.05) || !(Long < 1.0e4) || !RealPlace(Near) || !RealPlace(Far))
		{
			return false;
		}
		Out_ = B;
		Out_.Hose = 0.0;
		Out_.AlongK = B.Hose;
		Out_.From = (Near + Far) * 0.5;
		Out_.Vel = Vec();
		Out_.Drag = 0.0;
		Out_.Gravity = 0.0;
		Out_.Along = Run * (1.0 / Long); // (its head the older round, the farther out)
		Out_.HalfLength = 0.5 * Long;
		Out_.BySpeed = 0.0;
		Out_.bFired = false;
		Out_.Point = 0.0;
		Out_.Tail = 0.0;
		Out_.Back = Bit::Reach();
		return true;
	}

	double Battle::BurnLife(const AirRow& Row, double Mps) const
	{
		// (how long a tracer burns: as the look has it, or, with burn_out_m (the art director's 2 km for a 20 mm and 3.5 km
		// for a 40 mm), until it has flown that far at its drag; one that never gets so far burns until it is down to a
		// twentieth of its speed)
		const double Own = AirTracerLife * Row.TracerLifeK;
		if (!(Row.BurnOut > 0.0) || !(Mps > 1.0) || !(Mps < 1.0e5))
		{
			return Own;
		}
		const double K = Row.Drag;
		const double T = K > 1e-6 && K < 1e3 ? -std::log(std::max(1.0 - Row.BurnOut * K / Mps, 0.05)) / K : Row.BurnOut / Mps;
		return std::clamp(T, 0.05, 20.0);
	}

	void Battle::PaleUp(const Bit& B, const Vec& P, const View& Eye, Rgb& Col, Rgb& Floor, Rgb& Lit, double& A) const
	{
		// (the art director on Midway 06: a burning carrier's smoke stood a column hundreds of metres to kilometres high,
		// bent over by the wind, black and dense low and browner grey and thinner high, lit on its sun side. By its height
		// over the fire a puff goes toward pale_up_colour, its sunlit side toward pale_up_lit, and it thins)
		const double Up = (P.Z - B.PaleUpZ) / B.PaleUpM;
		const double Share = std::pow(Up == Up ? Clamp01(Up) : 0.0, std::clamp(B.PaleUpPow, 0.1, 8.0));
		const double To = std::clamp(B.PaleUpMost, 0.0, 1.0) * Share;
		Col = Col.Mix(Rgb(B.PaleUpCol.R * Eye.SmokeLight.R, B.PaleUpCol.G * Eye.SmokeLight.G, B.PaleUpCol.B * Eye.SmokeLight.B), To);
		Floor = Col * SkyFill;
		Lit = Lit.Mix(B.PaleUpLit, To);
		A *= 1.0 - std::clamp(B.ThinUp, 0.0, 1.0) * Share;
	}

	void Battle::AirRound(double T, const Vec& At, const Vec& Dir, const Vec& Carry, const AirRow& Row, double Mps, bool bTracer, bool bSmoke, Dice& D, const TracerHue* Hue)
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
		if (bSmoke && Row.SmokeAlpha > 0.0 && RoomGate() < static_cast<size_t>(MaxPuffs) * 4 / 5) // (the first thing shed when the room fills; a twin counts its two slots)
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
			if (!Hue)
			{
				Tracer(T, At, Vel.Unit(Aim), Vel, Row.TracerS, Row.Drag, Row.Drop);
				return;
			}
			// (the air look. No more than air_guns.tracers.most alight at once: as the count nears it, the guns farthest
			// from the eye go without first, those within always_within_m never)
			AirTracerEnds.erase(std::remove_if(AirTracerEnds.begin(), AirTracerEnds.end(), [T](double End) { return !(End > T) || End > T + 40.0; }), AirTracerEnds.end());
			const double Fill = AirTracerMost > 0 ? static_cast<double>(AirTracerEnds.size()) / AirTracerMost : 1.0;
			if (Fill > 0.7)
			{
				const double Far = bEyeKnown ? (LastEye - At).Length() : 0.0;
				const double Reach = AirTracerNear + (std::max(AirBeyond(), AirTracerNear) - AirTracerNear) * Clamp01((1.0 - Fill) / 0.3);
				if (!(Far <= Reach) || AirTracerEnds.size() >= static_cast<size_t>(2 * AirTracerMost))
				{
					return;
				}
			}
			AirTracerEnds.push_back(T + BurnLife(Row, Vel.Length()));
			AirTracer(T, At, Vel.Unit(Aim), Vel, Row, *Hue, AirOneFanBeyond > 0.0 && bEyeKnown && !((LastEye - At).Length() <= AirOneFanBeyond));
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
		// (the puff a hit leaves in the air goes slowly: linger.hit)
		const size_t First = Bits.size();
		AirHitLaid(Now, At, Normal, Carry, Kind);
		LingerFrom(First, LingerOf("hit"), Now, At, 1.0, 0.0);
	}

	void Battle::AirHitLaid(double Now, const Vec& At, const Vec& Normal, const Vec& Carry, const char* Kind)
	{
		// Rounds striking an aircraft: incendiary and HE rounds spark and flash on the skin, tear off scraps of it that
		// tumble away behind, and leave a small dark puff hanging where the aircraft was. Bigger for cannon.
		const AirRow& Row = AirRowOf(Kind && *Kind ? Kind : "hmg");
		const double S = std::max(0.2, Row.Strike);
		Dice D(++Seeds * 67867967ull ^ PlaceKey(At));
		const Vec Skin = Normal.Unit(Vec(0.0, 0.0, 1.0));
		const Rgb Spark = Picture("air_hit.spark_colour", Rgb::FromPicture(255, 236, 180));
		// (art director 15:02Z, air-fates-look D: a rifle-calibre or .50 hit is a bright sparkle of one or two frames and
		// nothing more; a 20 mm hit a small flash, a black-grey puff a metre or two across and a fragment or two. Only
		// with air_hit.sparkle and air_hit.burst in the recipe; without them every hit is as it was)
		const bool bBurst = Row.bCannon && Has("air_hit.burst.puff_m.0");
		if (!Row.bCannon && Has("air_hit.sparkle.points.0"))
		{
			if (!std::isfinite(At.X + At.Y + At.Z) || !std::isfinite(Carry.X + Carry.Y + Carry.Z))
			{
				return;
			}
			const std::string P = "air_hit.sparkle.";
			const int Points = static_cast<int>(SafeNum(std::round(Pick(D, P + "points", 1.0, 3.0)), 1.0, 8.0, 2.0));
			const double Scatter = SafeNum(Num(P + "scatter_m", 0.5), 0.0, 5.0, 0.5), Within = SafeNum(Num(P + "within_s", 0.05), 0.0, 0.5, 0.05);
			const double Size = SafeNum(Num(P + "size_m", 0.12), 0.01, 2.0, 0.12), Over = SafeNum(Num(P + "over", 4.0), 1.0, 4.0, 1.0);
			const double SparkLeastPx = SafeNum(Num(P + "least_px", 5.0), 0.0, 32.0, 5.0);
			const Span Life = Range(P + "life_s", 0.02, 0.035);
			const Vec Side = Skin.Cross(ZUp).Unit(Skin.Cross(Vec(1.0, 0.0, 0.0)).Unit(Vec(0.0, 1.0, 0.0)));
			const Vec Fore = Side.Cross(Skin).Unit(Vec(1.0, 0.0, 0.0));
			for (int I = 0; I < Points; ++I)
			{
				Bit B;
				B.Born = Now + (I == 0 ? 0.0 : D.U(0.0, Within));
				B.From = At + Skin * 0.05 + (Side * D.Bell() + Fore * D.Bell()) * (I == 0 ? 0.0 : Scatter);
				B.Vel = Carry;
				B.Life = SafeNum(Life.Pick(D), 0.008, 0.055, 0.03); // (always a light shorter than a frame: laid once, whole)
				B.R0 = B.R1 = Size * S * D.U(0.7, 1.3);
				B.Col = B.Col2 = Spark;
				B.Over = Over;
				B.Alpha = 1.0;
				B.FadeIn = 0.001;
				B.Hold = 0.6;
				B.Heart = 1.0;
				B.Rim = 0.3;
				B.Rough = 0.5;
				B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
				B.LeastPx = SparkLeastPx;
				B.LeastKeep = 0.9;
				B.bLight = true;
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
			return;
		}
		const double HitLeastPx = bBurst ? SafeNum(Num("air_hit.burst.least_px", 5.0), 0.0, 32.0, 5.0) : std::max(0.0, Num("air_hit.least_px", 0.0));
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
		const int NSparks = bBurst ? static_cast<int>(SafeNum(std::round(Pick(D, "air_hit.burst.sparks", 2.0, 4.0)), 0.0, 32.0, 3.0)) : std::max(1, static_cast<int>(std::lround(Pick(D, "air_hit.sparks", 3.0, 6.0) * S)));
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
		const int NScraps = bBurst ? static_cast<int>(SafeNum(std::round(Pick(D, "air_hit.burst.fragments", 1.0, 2.0)), 0.0, 16.0, 1.0)) : std::max(1, static_cast<int>(std::lround(Pick(D, "air_hit.scraps", 1.0, 2.0) * S)));
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
		const Rgb Smoke = bBurst ? Picture("air_hit.burst.puff_colour", Rgb::FromPicture(58, 57, 58)) : Picture("air_hit.smoke_colour", Rgb::FromPicture(70, 68, 64));
		// (art director 19:52Z: one rough puff read as a bird or a moth. A cannon shell's is a ragged roundish ball: a
		// main puff and puff_lobes smaller ones crowding it, so its outline is lumps and never wings)
		const int BurstLobes = bBurst ? static_cast<int>(SafeNum(std::round(Num("air_hit.burst.puff_lobes", 0.0)), 0.0, 6.0, 0.0)) : 0;
		double BurstR = 0.0, BurstS = 0.0; // (picked with the main puff, where and in the order they always were)
		const int NPuffs = bBurst ? 1 + BurstLobes : (S > 1.0 ? 2 : 1);
		Vec BurstMid;
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
			if (bBurst)
			{
				// (a black-grey puff a metre or two across, hanging where the hit was as the aircraft flies on)
				if (I == 0)
				{
					BurstS = SafeNum(Pick(D, "air_hit.burst.puff_s", 1.5, 3.0), 0.1, 30.0, 2.0);
					BurstR = SafeNum(0.5 * Pick(D, "air_hit.burst.puff_m", 1.0, 2.0), 0.1, 10.0, 0.75);
				}
				B.Life = BurstS;
				B.R0 = 0.2;
				B.R1 = BurstR;
				B.Alpha = SafeNum(Num("air_hit.burst.puff_alpha", 0.75), 0.0, 1.0, 0.75);
				B.Rough = 0.4;
				B.LeastPx = SafeNum(Num("air_hit.burst.puff_least_px", 0.0), 0.0, 32.0, 0.0);
				B.LeastKeep = 0.5;
				if (BurstLobes > 0)
				{
					B.Rough = 0.22;
					B.Heart = 0.85;
					if (I == 0)
					{
						BurstMid = B.From;
					}
					else
					{
						// (a lobe: smaller, on the main puff's shoulder and swelling out of it, a little shorter-lived)
						const double Share = D.U(0.55, 0.8);
						const Vec Off = Vec(D.Bell(), D.Bell(), D.Bell()).Unit(Skin);
						B.From = BurstMid + Off * 0.1;
						B.Vel = B.Vel + Off * (BurstR * D.U(0.9, 1.5)); // (slowed by its drag to a third of a radius or so out)
						B.R1 = BurstR * Share;
						B.Life = BurstS * D.U(0.75, 0.95);
						B.LeastPx *= Share;
					}
				}
			}
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
		// (a burst's smoke keeps its dark until its thick time, then greys, widens and thins into the air: linger.flak)
		const size_t First = Bits.size();
		AirFlakLaid(Now, At, Kind);
		LingerFrom(First, LingerOf("flak"), Now, At, 1.0, 0.0);
	}

	void Battle::AirFlakLaid(double Now, const Vec& At, const char* Kind)
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
			R.ThinKeep = static_cast<int>(SafeNum(Num(P + "thin_keep_one_in", 1.0), 1.0, 1000.0, 1.0)); // (held: twice it must fit an int; and a number, or the cast is not defined)
			R.ThinGrow = std::max(1.0, Num(P + "thin_grow", 1.0));
			R.ThinAlpha = std::max(0.0, Num(P + "thin_alpha", 1.0));
			R.ThinHold = Num(P + "thin_hold", -1.0);
			R.ThinRagged = Clamp01(Num(P + "thin_ragged", 0.0));
			R.LeastPx = std::max(0.0, Num(P + "least_px", 0.0));
			R.LeastKeep = Clamp01(Num(P + "least_keep", 0.0));
			R.Core = Has(P + "ribbon") && Num(P + "ribbon", 0.0) > 0.0 ? SafeNum(Num(P + "core", 0.15), 0.1, 0.9, 0.15) : 0.0;
			R.JoinFrom = Clamp01(Num(P + "join_from", 0.15));
			R.JoinBy = std::max(R.JoinFrom + 0.01, Num(P + "join_by", 0.45));
			R.TurnCos = std::cos(SafeNum(Num(P + "keep_each_turn_deg", 26.0), 2.0, 90.0, 26.0) * Pi / 180.0);
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
			R.FlameTongues = static_cast<int>(SafeNum(Num(P + "flame_tongues", 0.0), 0.0, 4.0, 0.0));
			R.FlameOver = SafeNum(Num(P + "flame_over", 1.0), 1.0, 8.0, 1.0);
			if (Has(P + "tail_wide_m.0"))
			{
				R.TailWide = Range(P + "tail_wide_m", 0.0, 0.0);
				R.TailWide.A = SafeNum(R.TailWide.A, 0.0, 60.0, 0.0);
				R.TailWide.B = SafeNum(R.TailWide.B, R.TailWide.A, 60.0, R.TailWide.A);
				R.TailM = SafeNum(Num(P + "tail_m", 16.0), 1.0, 200.0, 16.0);
			}
			if (Has(P + "gout_every_s.0"))
			{
				R.GoutEvery = Range(P + "gout_every_s", 0.0, 0.0);
				R.GoutEvery.A = SafeNum(R.GoutEvery.A, 0.2, 600.0, 2.0);
				R.GoutEvery.B = SafeNum(R.GoutEvery.B, R.GoutEvery.A, 600.0, R.GoutEvery.A);
				R.GoutFor = Range(P + "gout_s", 0.5, 1.0);
				R.GoutFor.A = SafeNum(R.GoutFor.A, 0.05, 30.0, 0.5);
				R.GoutFor.B = SafeNum(R.GoutFor.B, R.GoutFor.A, 30.0, R.GoutFor.A);
				R.GoutWide = SafeNum(Num(P + "gout_wide", 1.5), 1.0, 4.0, 1.5);
				R.GoutAlpha = SafeNum(Num(P + "gout_alpha", 1.3), 1.0, 4.0, 1.3);
			}
			if (Has(P + "fade_over_s.0"))
			{
				R.FadeOver = Range(P + "fade_over_s", 0.0, 0.0);
				R.FadeOver.A = SafeNum(R.FadeOver.A, 0.1, 3600.0, 30.0);
				R.FadeOver.B = SafeNum(R.FadeOver.B, R.FadeOver.A, 3600.0, R.FadeOver.A);
				R.FadeTo = SafeNum(Num(P + "fade_to", 0.3), 0.0, 1.0, 0.3);
			}
			if (Has(P + "for_s.0"))
			{
				R.ForS = Range(P + "for_s", 0.0, 0.0);
				R.ForS.A = SafeNum(R.ForS.A, 0.05, 3600.0, 1.0);
				R.ForS.B = SafeNum(R.ForS.B, R.ForS.A, 3600.0, R.ForS.A);
			}
			R.bThenStreak = Num(P + "then_streak", 0.0) > 0.0;
			R.Wisps = SafeNum(Num(P + "wisps", 0.0), 0.0, 1.0, 0.0);
			R.TailWiden = SafeNum(Num(P + "tail_widen", 0.0), 0.0, 1.0, 0.0);
			R.Knots = SafeNum(Num(P + "knots", 0.0), 0.0, 1.0, 0.0);
			R.KnotAlpha = SafeNum(Num(P + "knot_alpha", 0.7), 0.0, 2.0, 0.7);
			R.KnotDark = SafeNum(Num(P + "knot_dark", 0.8), 0.0, 1.0, 0.8);
			R.KnotR = Range(P + "knot_r", 0.3, 0.55);
			R.KnotOff = Range(P + "knot_off", 0.15, 0.9);
			R.KnotS = Range(P + "knot_s", 2.5, 5.0);
			R.Fold = SafeNum(Num(P + "fold", 0.0), 0.0, 1.0, 0.0);
			R.EdgeTorn = SafeNum(Num(P + "edge_torn", 0.0), 0.0, 4.0, 0.0);
			R.Longest = SafeNum(Num(P + "longest_m", 0.0), 0.0, 1.0e7, 0.0);
			R.SpurtM = Range(P + "spurt_m", 0.0, 0.0);
			R.SpurtWide = SafeNum(Num(P + "spurt_wide", 2.0), 0.1, 8.0, 2.0);
			R.SpurtAlpha = SafeNum(Num(P + "spurt_alpha", 1.6), 0.0, 8.0, 1.6);
			for (Span* Each : { &R.KnotR, &R.KnotOff, &R.KnotS, &R.SpurtM })
			{
				Each->A = SafeNum(Each->A, 0.0, 60.0, 0.0);
				Each->B = SafeNum(Each->B, 0.0, 60.0, 0.0);
			}
			R.bSoft = Num(P + "soft", 0.0) > 0.0;
			R.Blend = SafeNum(Num(P + "blend_s", 0.0), 0.0, 30.0, 0.0);
			R.ShadowK = Has(P + "shadow.0") ? SafeNum(Num(P + "shadow_k", 0.8), 0.0, 1.0, 0.8) : 0.0;
			R.Shadow = Picture(P + "shadow", Rgb::FromPicture(186, 194, 208));
			R.FlameShrink = SafeNum(Num(P + "flame_shrink_s", 0.0), 0.0, 30.0, 0.0);
			R.bStream = Has(P + "stream_lengths.0");
			if (R.bStream)
			{
				for (int I = 0; I < 4; ++I)
				{
					const std::string N = "." + std::to_string(I);
					R.StreamLen[I] = SafeNum(Num(P + "stream_lengths" + N, R.StreamLen[I]), 0.0, 20.0, R.StreamLen[I]);
					R.StreamSoot[I] = SafeNum(Num(P + "stream_sooty" + N, R.StreamSoot[I]), 0.0, 1.0, R.StreamSoot[I]);
				}
				R.StreamLenOfSpan = SafeNum(Num(P + "length_of_span", 0.8), 0.1, 4.0, 0.8);
				R.StreamMost = SafeNum(Num(P + "stream_most_m", 90.0), 1.0, 400.0, 90.0);
				R.StreamCarried = SafeNum(Num(P + "stream_carried", 0.25), 0.0, 0.9, 0.25);
				R.StreamWide = SafeNum(Num(P + "stream_wide_m", 0.77), 0.05, 20.0, 0.77);
				R.StreamSmokeFrom = SafeNum(Num(P + "stream_smoke_from", 0.45), 0.0, 1.0, 0.45);
				R.StreamOver = SafeNum(Num(P + "stream_over", 1.6), 1.0, 8.0, 1.6);
				R.StreamRagged = SafeNum(Num(P + "stream_ragged", 1.7), 0.0, 20.0, 1.7);
				R.StreamStrands = static_cast<int>(SafeNum(Num(P + "stream_strands", 2.0), 1.0, 4.0, 2.0));
				R.StreamHot = Picture(P + "stream_hot", Rgb::FromPicture(255, 206, 90));
				R.StreamBody = Picture(P + "stream_body", Rgb::FromPicture(250, 146, 36));
				R.StreamDeep = Picture(P + "stream_deep", Rgb::FromPicture(176, 62, 30));
				R.StreamSootCol = Picture(P + "stream_soot", Rgb::FromPicture(70, 34, 24));
			}
			R.Col = Picture(P + "colour", Rgb::FromPicture(128, 126, 122));
			R.ColFull = Picture(P + "colour_full", R.Col);
			R.ColOld = Picture(P + "colour_old", R.ColFull);
			// (this head's wild numbers, 19:20Z 20 November: a row's every number held to what a trail can be. One that was
			// not a number, or without end, went through to the points laid: r0_m, r1_m, carried, rise_mps, flame_m,
			// life_s, least_px, thin_grow, lumpy and drift each did. The bounds are far outside any trail's, so a
			// recipe's own numbers are read as they were)
			auto Hold = [](double& V, double Least, double Most, double Usual) { V = SafeNum(V, Least, Most, Usual); };
			Hold(R.Life, 0.05, 600.0, 6.0);
			Hold(R.Spacing, 1.0, 1000.0, 8.0);
			Hold(R.R0, 0.0, 200.0, 0.5);
			Hold(R.R1, 0.0, 1000.0, 3.5);
			Hold(R.Alpha, 0.0, 4.0, 0.5);
			Hold(R.DarkenS, 0.01, 3600.0, 4.0);
			Hold(R.RampS, 0.01, 3600.0, 1.5);
			Hold(R.Smear, 0.0, 100.0, 1.5);
			Hold(R.Carried, -2.0, 2.0, 0.05);
			Hold(R.Flame, 0.0, 500.0, 0.0);
			Hold(R.Grow, 0.1, 10.0, 0.5);
			Hold(R.Lumpy, 0.0, 1.0, 0.0);
			Hold(R.SmearS, 0.0, 3600.0, 0.0);
			Hold(R.Rise, -100.0, 100.0, 0.0);
			Hold(R.ThinAfter, 0.0, 3600.0, 0.0);
			Hold(R.ThinGrow, 1.0, 100.0, 1.0);
			Hold(R.ThinAlpha, 0.0, 100.0, 1.0);
			Hold(R.ThinHold, -1.0, 1.0, -1.0);
			Hold(R.ThinRagged, 0.0, 1.0, 0.0);
			Hold(R.LeastPx, 0.0, 4096.0, 0.0);
			Hold(R.LeastKeep, 0.0, 1.0, 0.0);
			Hold(R.JoinFrom, 0.0, 1.0, 0.15);
			Hold(R.JoinBy, R.JoinFrom + 0.01, 10.0, 0.45);
			Hold(R.LumpsFrom, 0.0, 1.0, 0.75);
			Hold(R.Drift, 0.0, 1.0, 0.35);
			Hold(R.PaleTo, 0.0, 1.0, 0.0);
			Hold(R.PaleFrom, 0.0, 3600.0, 0.0);
			Hold(R.PaleBy, R.PaleFrom + 0.1, 3700.0, 2.5);
			Hold(R.Wander, 0.0, 2.0, 0.0);
			Hold(R.WidenBy, 0.0, 3600.0, 0.0);
			Hold(R.SmokeStart, 0.0, 10.0, 1.0);
			Hold(R.FlameLiftOfLen.A, 0.0, 10.0, 0.0);
			Hold(R.FlameLiftOfLen.B, 0.0, 10.0, 0.0);
			Hold(R.LicksPerFrame, 0.0, 100.0, 0.4);
			Hold(R.FlameAlphaRoot, 0.0, 1.0, 0.92);
			Hold(R.FlameAlphaFall, -1.0, 1.0, 0.2);
			Hold(R.FlameHold, 0.0, 1.0, 0.45);
			Hold(R.FlameWide, 0.1, 100.0, 1.0);
			Hold(R.LickAlpha, 0.0, 1.0, 0.9);
			Hold(R.FlameLiftCurve, 0.0, 10.0, 0.0);
			Hold(R.TipSmoke, 0.0, 1.0, 0.0);
			Hold(R.TipSmokeR, 0.0, 10.0, 0.3);
			Hold(R.TipSmokeLife.A, 0.01, 60.0, 0.3);
			Hold(R.TipSmokeLife.B, 0.01, 60.0, 0.6);
			Hold(R.FlameOfSpan, 0.0, 10.0, 0.0);
			Hold(R.FlameMost, 0.0, 500.0, R.Flame);
			Hold(R.FlameLeastPx, 0.0, 4096.0, 0.0);
			Hold(R.FlameLift.A, 0.0, 100.0, 0.3);
			Hold(R.FlameLift.B, 0.0, 100.0, 0.6);
		}
		if (Has(P + "plume.lip_of_funnel"))
		{
			// (a funnel's plume, the art director's ruling of 05:30Z 21 November: see PlumeMove. Only a row with these keys)
			const std::string Q = P + "plume.";
			TrailRow::PlumeRow& Pl = R.Plume;
			Pl.bOn = true;
			Pl.Lip = SafeNum(Num(Q + "lip_of_funnel", 1.1), 0.2, 5.0, 1.1);
			Pl.Overlap = SafeNum(Num(Q + "overlap", 3.0), 1.0, 6.0, 3.0);
			Pl.WideK = SafeNum(Num(Q + "wide_k", 0.498), 0.0, 5.0, 0.498);
			Pl.WideByFunnel = SafeNum(Num(Q + "wide_by_funnel", 0.45), 0.0, 1.0, 0.45);
			Pl.RiseK = SafeNum(Num(Q + "rise_k", 4.3), 0.0, 30.0, 4.3);
			Pl.FeltLeast = SafeNum(Num(Q + "felt_least_mps", 2.0), 0.5, 20.0, 2.0);
			Pl.TopSpeed = SafeNum(Num(Q + "top_speed_mps", 15.4), 1.0, 60.0, 15.4);
			Pl.Funnel = SafeNum(Num(Q + "funnel_m", 3.0), 0.5, 20.0, 3.0);
			Pl.FullFrom = SafeNum(Num(Q + "full_from", 0.75), 0.01, 1.0, 0.75);
			Pl.CruiseTo = SafeNum(Num(Q + "cruise_to", 0.4), 0.0, Pl.FullFrom - 0.005, std::min(0.4, Pl.FullFrom - 0.005));
			Pl.Size = SafeNum(Num(Q + "size", 0.3), 0.0, 0.8, 0.3);
			Pl.Shade = SafeNum(Num(Q + "shade", 0.15), 0.0, 0.8, 0.15);
			Pl.Rough = SafeNum(Num(Q + "rough", 0.35), 0.0, 1.0, 0.35);
			Pl.LipLeastPx = SafeNum(Num(Q + "lip_least_px", 0.0), 0.0, 64.0, 0.0);
			Pl.LipMost = SafeNum(Num(Q + "lip_most", 4.0), 1.0, 20.0, 4.0);
			Pl.ShadowOfLit = SafeNum(Num(Q + "shadow_of_lit", 0.55), 0.0, 1.0, 0.55);
			Pl.ShadowK = SafeNum(Num(Q + "shadow_k", 0.0), 0.0, 1.0, 0.0);
			Pl.OverlapFar = SafeNum(Num(Q + "overlap_far", Pl.Overlap), 1.0, Pl.Overlap, Pl.Overlap);
			Pl.FarPx = Range(Q + "far_px", 3.0, 6.0);
			Pl.FarPx.A = SafeNum(Pl.FarPx.A, 0.0, 4096.0, 3.0);
			Pl.FarPx.B = SafeNum(Pl.FarPx.B, Pl.FarPx.A + 0.01, 4097.0, Pl.FarPx.A + 3.0);
			// (laid evenly, the art director 14:13Z 21 November: see PlumeMove. Without the key: thinned by an even spread, as before)
			Pl.Even = SafeNum(Num(Q + "even", 0.0), 0.0, 0.9, 0.0);
			Pl.EvenNear = static_cast<int>(SafeNum(Num(Q + "even_near", 30.0), 0.0, 30.0, 30.0));
			Pl.EvenFar = static_cast<int>(SafeNum(Num(Q + "even_far", 0.0), 0.0, 8.0, 0.0));
			Pl.EvenStep = SafeNum(Num(Q + "even_step", 2.0), 1.2, 2.0, 2.0);
			Pl.HeartRound = SafeNum(Num(Q + "heart_round", 0.0), 0.0, 1.0, 0.0);
			// (a plume that rolls, and a wide funnel's puffs laid closer: the art director 16:03Z 21 November. Without the keys: no
			// roll, and one overlap for every funnel)
			Pl.RollLift = SafeNum(Num(Q + "roll_lift", 0.0), 0.0, 1.0, 0.0);
			Pl.RollShade = SafeNum(Num(Q + "roll_shade", 0.0), 0.0, 0.5, 0.0);
			Pl.RollRun = SafeNum(Num(Q + "roll_run", 4.0), 1.0, 64.0, 4.0);
			Pl.OverlapWide = SafeNum(Num(Q + "overlap_wide", Pl.Overlap), Pl.Overlap, 6.0, Pl.Overlap);
			Pl.WideFunnel = SafeNum(Num(Q + "wide_funnel_m", 8.0), Pl.Funnel + 0.1, 30.0, std::max(8.0, Pl.Funnel + 0.1));
			Pl.SurgeRise = SafeNum(Num(Q + "surge.rise", 0.08), 0.005, 1.0, 0.08);
			Pl.SurgeWithin = SafeNum(Num(Q + "surge.within_s", 5.0), 0.2, 60.0, 5.0);
			Pl.SurgeFor = Range(Q + "surge.for_s", 15.0, 25.0);
			Pl.SurgeFor.A = SafeNum(Pl.SurgeFor.A, 0.0, 120.0, 15.0);
			Pl.SurgeFor.B = SafeNum(Pl.SurgeFor.B, Pl.SurgeFor.A, 120.0, std::max(Pl.SurgeFor.A, 25.0));
			auto LookOf = [&](const char* Name, double Alpha, double FadeS, double FadePow, double AgeS, double OldBy, const Rgb& Col, const Rgb& Old)
			{
				const std::string L = Q + Name + ".";
				TrailRow::PlumeLook Is;
				Is.Alpha = SafeNum(Num(L + "alpha", Alpha), 0.0, 0.97, Alpha);
				Is.FadeS = SafeNum(Num(L + "fade_s", FadeS), 0.2, 600.0, FadeS);
				Is.FadePow = SafeNum(Num(L + "fade_pow", FadePow), 0.25, 4.0, FadePow);
				Is.AgeS = SafeNum(Num(L + "age_s", AgeS), 1.0, 120.0, AgeS);
				Is.OldBy = SafeNum(Num(L + "old_by_s", OldBy), 0.5, 600.0, OldBy);
				Is.Col = Picture(L + "colour", Col);
				Is.Old = Picture(L + "colour_old", Old);
				return Is;
			};
			Pl.Full = LookOf("full", 0.64, 10.6, 0.92, 40.0, 25.0, Rgb::FromPicture(80, 77, 75), Rgb::FromPicture(123, 122, 121));
			Pl.Cruise = LookOf("cruise", 0.27, 6.5, 1.0, 19.0, 10.0, Rgb::FromPicture(138, 136, 133), Rgb::FromPicture(143, 141, 139));
			Pl.Surge = LookOf("surge", 0.88, Pl.Full.FadeS, Pl.Full.FadePow, Pl.Full.AgeS, Pl.Full.OldBy, Rgb::FromPicture(64, 61, 59), Pl.Full.Old);
		}
		R.Name = Kind;
		TrailRows.push_back(R);
		return TrailRows.back();
	}

	void Battle::TrailStart(double Now, int Id, const Vec& At, const Vec& Carry, const char* Kind, double SpanM, int Look)
	{
		if (!RealPlace(At) || !RealPlace(Carry) || !(std::fabs(Now) <= 1.0e12))
		{
			return; // (a place, a speed or a time that is not a number, or without end: nothing is laid for it. This head's wild numbers)
		}
		const std::string Name = Kind && *Kind ? Kind : "smoke";
		for (Trailing& Tr : Trails)
		{
			if (Tr.Id == Id)
			{
				if (SpanM > 0.0)
				{
					Tr.Span = SpanM;
				}
				if (Look >= 0)
				{
					Tr.LookNo = std::min(Look, 3);
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
					// (a funnel's plume starts again from here too, a puff at once: the first power it is given is where
					// she stands, never a rise from where she stood before it was stopped, and no surge is left over from
					// then. The sea scene, 14:20Z 21 November)
					Tr.PowerLate = -1.0;
					Tr.PlumeOwed = 1.0e9;
					Tr.SurgeUntil = -1.0e18;
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
					TrailKind(Tr, Name, Now, true); // (smoke catching fire: the new kind ramps in from now)
				}
				return;
			}
		}
		Trailing Tr;
		Tr.Id = Id;
		Tr.bOn = true;
		Tr.Born = Tr.KindBorn = Tr.Last = Tr.LastPuff = Tr.NextFlame = Now;
		Tr.At = At;
		Tr.Carry = Carry;
		Tr.LastPuffAt = At;
		Tr.Span = std::max(0.0, SpanM);
		Tr.LookNo = Look >= 0 ? std::min(Look, 3) : -1;
		Tr.NextStream = Now;
		TrailKind(Tr, Name, Now, false);
		Trails.push_back(Tr);
	}

	void Battle::TrailKind(Trailing& Tr, const std::string& Name, double Now, bool bChange)
	{
		// (the clocks of an engine that is out, art director 15:38Z: only rows with those keys start any, and they
		// throw their own dice, so every other trail is laid as it was)
		double FlameWas = 0.0;
		Trailing::Bars OldBars;
		bool bOldBars = false;
		if (bChange)
		{
			const TrailRow& Old = TrailRowOf(Tr.Kind);
			bOldBars = Old.Core > 0.0 && Old.ThinAfter > 0.0 && Old.ThinKeep > 1 && Old.Life > Old.ThinAfter;
			if (bOldBars)
			{
				// (squeezed as the old kind's last pieces were, longest_m: on a fast aircraft the closing bar goes with its
				// neighbours, not after them. The pre-check, 22 November)
				const double K = SafeNum(Tr.Squeeze, 0.02, 1.0, 1.0);
				OldBars.Life = Old.Life * K;
				OldBars.Hold = Old.ThinHold >= 0.0 ? Old.ThinHold : 0.25 * Old.ThinAfter / Old.Life;
				OldBars.ShowFrom = Old.JoinFrom * Old.ThinAfter * K;
				OldBars.ShowBy = Old.JoinBy * Old.ThinAfter * K;
				OldBars.Smear = (2.0 * Old.ThinKeep + 1.0) * Old.Spacing;
				OldBars.PaleTo = Old.PaleTo;
				OldBars.PaleFrom = Old.PaleFrom * K;
				OldBars.PaleBy = Old.PaleBy * K;
			}
			FlameWas = FlameOf(Tr, Old) * (0.45 + 0.55 * Clamp01((Now - Tr.KindBorn) / Old.RampS));
			if (Now < Tr.FlameOutBy && Tr.FlameShrinkS > 0.0)
			{
				FlameWas = Tr.FlameWas * Clamp01((Tr.FlameOutBy - Now) / Tr.FlameShrinkS); // (already shrinking: on from where it is; never more than it was, should time go back)
			}
			else
			{
				Tr.FlameStartWas = Old.SmokeStart;
			}
		}
		Tr.Kind = Name;
		Tr.KindBorn = Now;
		Tr.Run = ++TrailRuns; // (a kind of its own: a kind change is ruled by its own rows, never a cut. Bit::CutAt)
		Tr.RunEnd = -1.0e18;
		const TrailRow& Row = TrailRowOf(Name);
		Tr.NextGout = Tr.GoutFrom = Tr.GoutUntil = Tr.FadeFor = Tr.KindFor = Tr.FlameWas = Tr.FlameOutBy = Tr.FlameShrinkS = 0.0;
		// (blend_s: the new kind's pieces start as the last piece laid was, and go over to their own; no dice)
		Tr.BlendFor = bChange && Row.Blend > 0.0 && Tr.Laid.bOn ? Row.Blend : 0.0;
		Tr.Was = Tr.Laid;
		Tr.bClose = Tr.BlendFor > 0.0 && bOldBars && Tr.Kept.bOn && Tr.bKeptAt;
		Tr.Close = OldBars;
		if (Row.GoutEvery.B <= 0.0 && Row.FadeOver.B <= 0.0 && Row.ForS.B <= 0.0 && Row.FlameShrink <= 0.0)
		{
			return;
		}
		Dice D(Mix64(static_cast<uint64_t>(static_cast<int64_t>(Tr.Id)) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(static_cast<int64_t>(SafeNum(Now, -1e9, 1e9, 0.0) * 1000.0))) | 1ull);
		if (Row.GoutEvery.B > 0.0)
		{
			Tr.NextGout = Now + Row.GoutEvery.Pick(D) * D.U(0.2, 1.0);
		}
		if (Row.FadeOver.B > 0.0)
		{
			Tr.FadeFor = Row.FadeOver.Pick(D);
		}
		if (Row.ForS.B > 0.0)
		{
			Tr.KindFor = Row.ForS.Pick(D);
		}
		else if (Row.bThenStreak && Tr.FadeFor > 0.0)
		{
			Tr.KindFor = Tr.FadeFor;
		}
		if (!Row.bThenStreak)
		{
			Tr.KindFor = 0.0;
		}
		if (Row.FlameShrink > 0.0 && Row.Flame <= 0.0 && FlameWas > 0.0)
		{
			Tr.FlameWas = FlameWas;
			Tr.FlameShrinkS = Row.FlameShrink;
			Tr.FlameOutBy = Now + Row.FlameShrink;
		}
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
		// (every piece laid here is of the trail's run: Bit::CutAt)
		struct Tag
		{
			std::vector<Bit>& Into;
			size_t From;
			uint32_t Run;
			~Tag()
			{
				for (size_t I = From; I < Into.size(); ++I)
				{
					Into[I].TrailRun = Run;
				}
			}
		} Tagged{ Bits, Bits.size(), Tr.Run };
		// Art director 21:27Z: thin and dark at the engine; about three times wider by 2 s and still widening; breaking
		// into uneven lumps, paler and thinner with age; each lump hanging where it was laid. A trail still comes to
		// full over ramp_s and darkens over darken_s as the damage takes hold.
		const double Ramp = Clamp01((T - Tr.KindBorn) / Row.RampS);
		const double Dark = Clamp01((T - Tr.KindBorn) / Row.DarkenS);
		const Vec Way = Carry.Unit(Vec(1.0, 0.0, 0.0));
		// (blend_s: how far a trail that changed kind has gone over to this one, 1 all the way)
		const double Blent = Tr.BlendFor > 0.0 && Tr.Was.bOn ? Clamp01((T - Tr.KindBorn) / Tr.BlendFor) : 1.0;
		const double BlendK = Blent * Blent * (3.0 - 2.0 * Blent);
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
			if (Tr.FarK > 1.0)
			{
				// (the far form, the art director's mend of 18:44Z: a puff for FarK of them, so the strip meanders over as
				// many metres and as widely as ever, never straightened into a contrail)
				const double Keep = std::pow(0.9, Tr.FarK), Gain = 0.44 * std::sqrt((1.0 - Keep * Keep) / 0.19);
				Tr.Wander = Tr.bWander ? Tr.Wander * Keep + Kick * Gain : Kick;
				Tr.RiseK = Tr.bWander ? 1.0 + (Tr.RiseK - 1.0) * Keep + (RiseK - 1.0) * Gain : RiseK;
			}
			else
			{
				Tr.Wander = Tr.bWander ? Tr.Wander * 0.9 + Kick * 0.44 : Kick;
				Tr.RiseK = Tr.bWander ? 1.0 + (Tr.RiseK - 1.0) * 0.9 + (RiseK - 1.0) * 0.44 : RiseK;
			}
			Tr.bWander = true;
			B.Vel = Carry * Row.Carried + Tr.Wander;
			B.Rise = Row.Rise * Tr.RiseK;
			B.Life = Row.Life * (1.0 - 0.15 * Row.Lumpy);
		}
		const double Thick = 0.7 + 0.3 * Ramp;
		B.R0 = Row.R0 * Thick * D.U(0.85, 1.15);
		B.R1 = Row.R1 * Thick * Size * D.U(0.85, 1.15);
		// (an engine that is out: a thicker gout as the oil spits, every few seconds; glycol thinning as it runs out)
		double Gout = 0.0;
		if (Row.GoutEvery.B > 0.0)
		{
			if (T >= Tr.NextGout)
			{
				Tr.GoutFrom = T;
				Tr.GoutUntil = T + Row.GoutFor.Pick(D);
				Tr.NextGout = Tr.GoutUntil + Row.GoutEvery.Pick(D);
			}
			if (T < Tr.GoutUntil && Tr.GoutUntil > Tr.GoutFrom)
			{
				Gout = std::sin(Pi * Clamp01((T - Tr.GoutFrom) / (Tr.GoutUntil - Tr.GoutFrom)));
			}
		}
		const double Thinned = Tr.FadeFor > 0.0 ? 1.0 - (1.0 - Row.FadeTo) * Clamp01((T - Tr.KindBorn) / Tr.FadeFor) : 1.0;
		B.Grow = Row.Grow;
		B.Along = Way;
		B.Smear = Row.Spacing * Row.Smear;
		B.SmearFor = Row.SmearS;
		B.Col = Row.Col.Mix(Row.ColFull, Dark);
		B.Col2 = Row.ColOld;
		B.Cool = Row.Lumpy > 0.0 ? 1.0 : 0.6; // (paling toward colour_old as it hangs)
		B.Alpha = Row.Alpha * (0.45 + 0.55 * Ramp) * (bLump ? 1.0 + 0.15 * Row.Lumpy : 1.0 - 0.25 * Row.Lumpy) * D.U(0.9, 1.05);
		if (Row.bSoft && bRibbon)
		{
			// (soft: no piece darker or paler than the next by its own dice; its density follows its width's slow wander, below)
			B.Alpha = Row.Alpha * (0.45 + 0.55 * Ramp);
			B.bSoft = true;
			if (Row.Fold > 0.0)
			{
				// (art director 19:52Z: lighter folds inside it. Denser on one side and thinner on the other, the lean
				// taking most of the last piece's, so a fold runs along several pieces and swaps sides)
				Tr.Lean = std::clamp(Tr.Lean * 0.8 + 0.35 * D.Bell(), -1.0, 1.0);
				B.Lean = Row.Fold * Tr.Lean;
			}
			B.Torn = Row.EdgeTorn;
		}
		if (Row.ShadowK > 0.0)
		{
			B.ShadowCol = Row.Shadow;
			B.ShadowK = Row.ShadowK;
		}
		if (Gout > 0.0 || Thinned < 1.0)
		{
			B.Alpha = std::min(0.97, B.Alpha * (1.0 + (Row.GoutAlpha - 1.0) * Gout)) * Thinned;
			B.R1 *= 1.0 + (Row.GoutWide - 1.0) * Gout;
		}
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
				if (Row.bSoft)
				{
					B.Alpha = std::min(0.97, B.Alpha * std::clamp(1.0 + 0.3 * Tr.Wide, 0.8, 1.2)); // (denser where it swells)
				}
			}
			B.SmearHold = Row.LumpsFrom; // (only toward its old end does it shorten to a lump)
			if (Row.PaleTo > 0.0)
			{
				// (art director 15:22Z: it pales with age; the length that hangs on under it takes the same)
				B.PaleTo = Row.PaleTo;
				B.PaleFrom = Row.PaleFrom;
				B.PaleBy = Row.PaleBy;
				if (BlendK < 1.0)
				{
					B.PaleTo = 1.0 - (1.0 - B.PaleTo) * BlendK; // (going over from a kind that does not pale)
				}
			}
			B.Smear = 3.0 * Row.Spacing; // (the most it reaches back: for what is off the screen, and the sun through its edge)
			if (Tr.bPuffAt && (At - Tr.PuffAt).Length() < 3.0 * Row.Spacing)
			{
				B.Back = Tr.Prev;
			}
		}
		if (Row.TailWide.B > 0.0)
		{
			// (art director 15:38Z: as wide as its nacelle at the engine, and three or four times that as it passes the
			// tail: the aircraft's own wake spreads it, long before the row's slow widening does)
			const double Mid = Row.TailWide.A + (Row.TailWide.B - Row.TailWide.A) * (0.5 + 0.5 * std::clamp(Tr.Wide * 2.0, -1.0, 1.0));
			B.WakeBy = Row.TailM / std::max(Carry.Length(), 20.0);
			B.WakeR = std::max(B.R0, 0.5 * Mid * Thick * (1.0 + (Row.GoutWide - 1.0) * Gout));
			B.WakeOn = Row.TailWiden;
		}
		if (BlendK < 1.0)
		{
			// (blend_s: a trail going over to this kind. This piece is still part of the way from the last piece the old
			// kind laid, in colour, density and width, so the trail never changes at a cut across it)
			B.Col = Tr.Was.Col.Mix(B.Col, BlendK);
			B.Col2 = Tr.Was.Col2.Mix(B.Col2, BlendK);
			B.Alpha = Tr.Was.Alpha + (B.Alpha - Tr.Was.Alpha) * BlendK;
			B.R0 = Tr.Was.R0 + (B.R0 - Tr.Was.R0) * BlendK;
			B.R1 = Tr.Was.R1 + (B.R1 - Tr.Was.R1) * BlendK;
			if (B.WakeBy > 0.0 && Tr.Was.WakeR > 0.0)
			{
				B.WakeR = Tr.Was.WakeR + (B.WakeR - Tr.Was.WakeR) * BlendK;
			}
			if (B.Ribbon > 0.0 && Tr.Was.Core > 0.0)
			{
				B.Ribbon = Tr.Was.Core + (B.Ribbon - Tr.Was.Core) * BlendK; // (and how much of its width is dense)
			}
			// (its shadow side too: the colour of whichever kind has one, or between the two)
			B.ShadowCol = B.ShadowK <= 0.0 ? Tr.Was.Shadow : (Tr.Was.ShadowK <= 0.0 ? B.ShadowCol : Tr.Was.Shadow.Mix(B.ShadowCol, BlendK));
			B.ShadowK = Tr.Was.ShadowK + (B.ShadowK - Tr.Was.ShadowK) * BlendK;
		}
		Tr.Laid.Col = B.Col;
		Tr.Laid.Col2 = B.Col2;
		Tr.Laid.Alpha = B.Alpha;
		Tr.Laid.R0 = B.R0;
		Tr.Laid.R1 = B.R1;
		Tr.Laid.WakeR = B.WakeBy > 0.0 ? B.WakeR : 0.0;
		Tr.Laid.ShadowK = B.ShadowK;
		Tr.Laid.Shadow = B.ShadowCol;
		Tr.Laid.Core = B.Ribbon;
		if (Tr.bClose)
		{
			Tr.bClose = false;
			if (!(bRibbon && Row.ThinAfter > 0.0 && Row.ThinKeep > 1 && Row.Life > Row.ThinAfter))
			{
				// (the kind before left lengths that hang on, and this kind lays none: one more of them from here back to
				// the last, as wide and as dark there as that one is and nothing at this end, so the old trail thins away
				// to where it ended. Left as it was, its last length ended square across the sky. A kind that lays its
				// own carries the line on instead)
				if (bRibbon && Tr.Kept.bOn && Tr.bKeptAt && (At - Tr.KeptAt).Length() < Tr.Close.Smear)
				{
					Bit C = B;
					C.R0 = Tr.Kept.R0;
					C.RMid = Tr.Kept.RMid;
					C.R1 = Tr.Kept.R1;
					C.Grow = Tr.Kept.Grow;
					C.GrowBy = Tr.Kept.GrowBy;
					C.GrowEarly = Tr.Kept.GrowEarly;
					C.WakeBy = Tr.Kept.WakeBy;
					C.WakeR = Tr.Kept.WakeR;
					C.WakeOn = Tr.Kept.WakeOn;
					C.Lean = 0.0;
					C.Col = Tr.Kept.Col;
					C.Col2 = Tr.Kept.Col2;
					C.Cool = Tr.Kept.Cool;
					C.Alpha = 0.0015;
					C.FadeIn = 0.0;
					C.Hold = Tr.Close.Hold;
					C.Life = Tr.Close.Life;
					C.ShowFrom = Tr.Close.ShowFrom;
					C.ShowBy = Tr.Close.ShowBy;
					C.PaleTo = Tr.Close.PaleTo;
					C.PaleFrom = Tr.Close.PaleFrom;
					C.PaleBy = Tr.Close.PaleTo > 0.0 ? Tr.Close.PaleBy : 0.0;
					C.SmearHold = Tr.Kept.LumpsFrom;
					C.Smear = Tr.Close.Smear;
					C.Back = Tr.Kept;
					C.bSoft = false;
					C.ShadowK = 0.0;
					C.LumpSeed = B.LumpSeed * 2654435761u + 1u;
					Bits.push_back(C);
				}
				Tr.Kept.bOn = false;
				Tr.bKeptAt = false;
			}
		}
		Tr.Laid.bOn = true;
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
				const int Every = Row.KeepFar > 1 ? Row.KeepFar : Row.ThinKeep; // (the far form: its bars nearer together in its puffs, so a long bar still follows the path's slow meander)
				unsigned Gap = static_cast<unsigned>(Every);
				double Torn = 1.0;
				if (Row.ThinRagged > 0.0)
				{
					Gap = static_cast<unsigned>(std::clamp(static_cast<int>(std::lround(Every * (1.0 + Row.ThinRagged * D.U(-1.0, 1.0)))), 1, 2 * Every));
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
					if (Tr.FarK > 1.0)
					{
						C.Billow = FarTrailBillow * Clamp01(Tr.FarK - 1.0); // (the art director's mend of 18:44Z: never smooth as a contrail)
						C.bShowEach = true;
						C.bFarLong = true;
					}
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
					Tr.Kept.WakeBy = C.WakeBy;
					Tr.Kept.WakeR = C.WakeR;
					Tr.Kept.WakeOn = C.WakeOn;
					Tr.Kept.Lean = C.Lean;
					Tr.Kept.Life = C.Life;
					Tr.Kept.Alpha = C.Alpha; // (its own opacity: the next bar's far end takes it)
					Tr.Kept.Col = C.Col;
					Tr.Kept.Col2 = C.Col2;
					Tr.Kept.Cool = C.Cool;
					Tr.Kept.PaleTo = C.bSoft ? (C.PaleBy > 0.0 ? C.PaleTo : 1.0) : -1.0;
					Tr.Kept.LumpsFrom = C.SmearHold;
					Tr.Kept.bOn = true;
					Tr.Kept.CutAt = C.CutAt; // (the pre-check on d2356292: re-pointed field by field, it kept an old cut, and every later piece took it as its far end's)
					Tr.KeptWay = Way;
					Tr.KeptAt = At;
					Tr.bKeptAt = true;
					Bits.push_back(C);
					++Tr.BarsLaid;
					if (Tr.FarK > 1.0)
					{
						B.Billow = FarTrailBillow * Clamp01(Tr.FarK - 1.0); // (the short piece here as lumpy as the rest: Bit::Billow)
						B.bFarLong = true;
					}
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
				if (bRibbon && Tr.FarK > 1.0)
				{
					// (the far form: standing in for the bar to be laid over it, until it is. Bit::StandA)
					B.StandA = std::min(0.95, B.Alpha * Row.ThinAlpha);
					B.StandFrom = Row.JoinFrom * Row.ThinAfter;
					B.StandBy = Row.JoinBy * Row.ThinAfter;
					B.StandBars = Tr.BarsLaid;
					B.Billow = FarTrailBillow * Clamp01(Tr.FarK - 1.0); // (and as lumpy along its length as the short ones near: Bit::Billow)
					B.bFarLong = true;
				}
			}
		}
		if (BlendK < 1.0 && Tr.Was.Life > 0.0)
		{
			B.Life = Tr.Was.Life + (B.Life - Tr.Was.Life) * BlendK; // (and it hangs as long as the smoke it grew out of, then as its own)
		}
		Tr.Laid.Life = B.Life;
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
			Tr.Prev.WakeBy = B.WakeBy;
			Tr.Prev.WakeR = B.WakeR;
			Tr.Prev.WakeOn = B.WakeOn;
			Tr.Prev.Lean = B.Lean;
			Tr.Prev.Life = B.Life;
			Tr.Prev.Alpha = B.Alpha; // (its own opacity: the next piece's far end takes it)
			Tr.Prev.Col = B.Col;
			Tr.Prev.Col2 = B.Col2;
			Tr.Prev.Cool = B.Cool;
			Tr.Prev.PaleTo = B.bSoft ? (B.PaleBy > 0.0 ? B.PaleTo : 1.0) : -1.0;
			Tr.Prev.LumpsFrom = B.SmearHold;
			Tr.Prev.bOn = true;
			Tr.Prev.CutAt = B.CutAt; // (as Kept: a new piece is uncut)
		}
		Tr.PuffAt = At;
		Tr.bPuffAt = true;
		Bits.push_back(B);
		if (Row.Wisps > 0.0 && D.U() < Row.Wisps && RoomGate() < static_cast<size_t>(MaxPuffs) * 4 / 5)
		{
			// (the art director's standing rule: one torn, lumpy column. A wisp thrown out of the young column to hang at
			// its edge, so its outline is ragged where the eye is nearest it; the first thing left out when the room fills)
			const Bit& Laid = Bits.back();
			const Vec Side = Way.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
			const Vec Over = Side.Cross(Way).Unit(ZUp);
			const double Bearing = D.U(0.0, 2.0 * Pi);
			const Vec Out_ = Side * std::cos(Bearing) + Over * std::sin(Bearing);
			const double Half = Laid.WakeBy > 0.0 ? Laid.WakeR : 3.0 * Laid.R0;
			Bit W;
			W.Born = T;
			W.From = At + Out_ * (0.5 * Laid.R0);
			// (thrown out to the column's edge and a little past it, slowing there; soft, long along the path, and thin,
			// so it is a shred hanging off the edge and never a dark blot in the middle)
			W.Vel = Carry * Row.Carried + Out_ * (Half * D.U(1.6, 2.8));
			W.Drag = 2.0;
			W.Rise = Row.Rise;
			W.Windage = 1.0;
			W.Life = D.U(1.6, 3.4);
			W.R0 = std::max(Laid.R0, 0.35 * Half);
			W.R1 = Half * D.U(0.7, 1.2);
			W.Grow = 0.45;
			W.Along = Way;
			W.Stretch = D.U(1.5, 2.6);
			W.Col = Laid.Col;
			W.Col2 = Laid.Col2;
			W.Cool = 0.5;
			W.Alpha = Laid.Alpha * D.U(0.3, 0.5);
			W.FadeIn = 0.25;
			W.Hold = 0.35;
			W.Heart = 0.55;
			W.Rim = 0.0;
			W.Rough = 0.35;
			W.ShadowCol = Laid.ShadowCol;
			W.ShadowK = Laid.ShadowK;
			W.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			W.Turn = D.U(0.0, 2.0 * Pi);
			Bits.push_back(W);
		}
		if (Row.Knots > 0.0 && D.U() < Row.Knots * (1.0 + 2.0 * Gout) && RoomGate() < static_cast<size_t>(MaxPuffs) * 4 / 5)
		{
			// (art director 19:52Z 20 November: denser knots inside it. A soft lump of the trail's own smoke, darker,
			// riding with the piece just laid and off its middle, drawn out along the path; it swells with the strip and
			// goes in a few seconds, so the trail's inside is never one even grey. Left out when the room fills)
			const Bit& Laid = B; // (the piece itself: Bits.back() is the wisp when one was just thrown. The pre-check, 21:24Z)
			const Vec Side = Way.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
			const Vec Over = Side.Cross(Way).Unit(ZUp);
			const double Bearing = D.U(0.0, 2.0 * Pi);
			const Vec Out_ = Side * std::cos(Bearing) + Over * std::sin(Bearing);
			const double Half = Laid.WakeBy > 0.0 ? Laid.WakeR : 3.0 * Laid.R0;
			const double Share = Row.KnotR.Pick(D);
			const double Off = Row.KnotOff.Pick(D);
			const bool bEdge = Off > 0.6; // (out at its edge: a lump of the column's own smoke, crisper, so its outline is lumps and never a ruled side)
			Bit K;
			K.Born = T;
			K.From = Laid.From + Out_ * (0.3 * Laid.R0);
			K.Vel = Laid.Vel + Out_ * (Half * Off * Laid.Drag); // (it comes to rest that share of the strip's half width off its middle)
			K.Drag = Laid.Drag;
			K.Rise = Laid.Rise;
			K.Windage = Laid.Windage;
			K.Life = Row.KnotS.Pick(D);
			K.R0 = Laid.R0 * std::max(0.6, 1.6 * Share);
			K.R1 = Half * Share * (1.0 + 0.5 * Row.TailWiden * K.Life);
			K.WakeBy = Laid.WakeBy;
			K.WakeR = Half * Share;
			K.Grow = 0.6;
			K.Along = Way;
			K.Stretch = D.U(1.6, 3.0);
			K.Col = bEdge ? Laid.Col : Laid.Col * Row.KnotDark;
			K.Col2 = Laid.Col2;
			K.Cool = 0.5;
			K.Alpha = std::min(0.95, Laid.Alpha * (bEdge ? 0.9 : Row.KnotAlpha) * D.U(0.7, 1.0));
			K.FadeIn = 0.15;
			K.Hold = 0.4;
			K.Heart = bEdge ? 0.55 : 0.3;
			K.Rim = 0.0;
			K.Rough = 0.2;
			K.ShadowCol = Laid.ShadowCol;
			K.ShadowK = Laid.ShadowK;
			K.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			K.Turn = D.U(0.0, 2.0 * Pi);
			Bits.push_back(K);
		}
	}

	void Battle::TrailMove(double Now, int Id, const Vec& At, const Vec& Carry, double Power)
	{
		if (!RealPlace(At) || !RealPlace(Carry) || !(std::fabs(Now) <= 1.0e12))
		{
			return; // (as TrailStart: the trail waits where it was for a real place)
		}
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
		if (Tr->KindFor > 0.0 && Now - Tr->KindBorn >= Tr->KindFor)
		{
			TrailKind(*Tr, "streak", Now, true); // (the glycol run out, the extinguisher's puff blown away: the thin dark streak that stays)
		}
		const TrailRow& AsRow = TrailRowOf(Tr->Kind);
		if (AsRow.Plume.bOn)
		{
			PlumeMove(Now, *Tr, AsRow, At, Carry, Power); // (a funnel: laid by time into the air over the ship, with dice of its own, so nothing else is thrown differently for it)
			return;
		}
		// (longest_m, the art director's mend of 22 November on the Bremen frames: no trail longer than about 2 km. A
		// fast aircraft lays its smoke over more sky in life_s than a slow one; over longest_m, its puffs live that much
		// shorter, and the whole of its age curve (its widening, paling, thinning and lumps) is squeezed by the same
		// share, so it is the same trail, only shorter. Without the key, or slower: as the row says)
		TrailRow Squeezed;
		const TrailRow* Use = &AsRow;
		const double Flies = Carry.Length();
		Tr->Squeeze = 1.0;
		if (AsRow.Longest > 0.0 && Flies * AsRow.Life > AsRow.Longest)
		{
			const double K = std::max(AsRow.Longest / (Flies * AsRow.Life), 0.02);
			Tr->Squeeze = K;
			Squeezed = AsRow;
			Squeezed.Life *= K;
			Squeezed.ThinAfter *= K;
			Squeezed.PaleFrom *= K;
			Squeezed.PaleBy *= K;
			Squeezed.WidenBy *= K;
			Squeezed.SmearS *= K;
			Squeezed.KnotS.A *= K;
			Squeezed.KnotS.B *= K;
			Use = &Squeezed;
		}
		double FarK = 1.0;
		Tr->FarK = 1.0;
		if (FarTrailFromM > 0.0 && bEyeKnown)
		{
			// (the far form, game.air_far: past trails_from_m a trail is about trails_puffs puffs over its length, each
			// laid that much farther apart and drawn as much longer back to the last; its bars that hang on are still one
			// in thin_keep_one_in, so each reaches back to the last as ever, over as many more metres. Over the last
			// trails_band of the range it goes over by degrees, so no length of a trail is laid in one form and the next
			// in the other; nothing laid changes form as the eye closes in)
			const double Band = FarTrailBand * FarTrailFromM;
			const double Over = Clamp01(((LastEye - At).Length() - (FarTrailFromM - Band)) / std::max(Band, 1.0));
			const double Far = Over * Over * (3.0 - 2.0 * Over);
			const double Each = Use->Life * std::max(Flies, 1.0) / FarTrailPuffs;
			const double K = 1.0 + (std::max(Each / std::max(Use->Spacing, 0.1), 1.0) - 1.0) * Far;
			if (K > 1.0 + 1.0e-6)
			{
				if (Use != &Squeezed)
				{
					Squeezed = AsRow;
					Use = &Squeezed;
				}
				Squeezed.Spacing *= K;
				if (Far >= 0.5 && FarTrailKeep > 1 && FarTrailKeep < Squeezed.ThinKeep)
				{
					Squeezed.KeepFar = FarTrailKeep;
				}
				FarK = K;
				Tr->FarK = K;
			}
		}
		const TrailRow& Row = *Use;
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
		// (the pre-check on 13388a4f, Bit::CutAt: speeding up, the row's life is squeezed shorter, and the first piece laid
		// now would end before the last one laid before it. Every piece of this run born by the last call is cut to end
		// with it, so the trail goes from its old end and never has a hole. Slowing down, the new pieces end later: no
		// cut, and the old end goes first as ever)
		if (Tr->RunEnd > Tr->Last + Row.Life + 1.0e-3)
		{
			TrailCuts.push_back(TrailCut{ Tr->Run, Tr->Last, Tr->Last + Row.Life });
		}
		Tr->RunEnd = Now + Row.Life;
		const double Between = Now - Tr->Last;
		const Vec Path = At - Tr->At;
		const double Long = Path.Length();
		// (fire: its smoke starts at the flame's tips, not at the cowl)
		double Grown = Clamp01((Now - Tr->KindBorn) / Row.RampS);
		double FlameM = FlameOf(*Tr, Row);
		bool bShrinking = false;
		if (FlameM <= 0.0 && Now < Tr->FlameOutBy && Tr->FlameShrinkS > 0.0)
		{
			// (a fire being put out: its tongues shrink back to the nacelle, never cut off and never thinner)
			FlameM = Tr->FlameWas * Clamp01((Tr->FlameOutBy - Now) / Tr->FlameShrinkS);
			Grown = 1.0;
			bShrinking = true;
		}
		Tr->Tip = FlameM * (bShrinking ? Tr->FlameStartWas : Row.SmokeStart) * (0.45 + 0.55 * Grown); // (art director 04:59Z: the smoke starts inside the flame's tips, no gap)
		// (a flamer's stream, the art director's ruling B: "long torn tongues 1 to 3 aircraft lengths behind, bright at
		// the source and orange to sooty further back, turning to thick black smoke". Its smoke starts part of the way
		// down the stream, among the tongues)
		double StreamM = 0.0, StreamSooty = 0.0;
		if (Row.bStream && !bShrinking)
		{
			const int LookIs = Tr->LookNo >= 0 ? Tr->LookNo : 1;
			const double SpanIs = Tr->Span > 0.0 ? Tr->Span : 11.0;
			StreamM = std::min(Row.StreamMost, Row.StreamLen[LookIs] * Row.StreamLenOfSpan * SpanIs) * (0.35 + 0.65 * Grown);
			StreamSooty = Row.StreamSoot[LookIs];
			Tr->Tip = std::max(Tr->Tip, StreamM * Row.StreamSmokeFrom);
		}
		const Vec TipBack = -Carry.Unit(Vec(1.0, 0.0, 0.0)) * Tr->Tip;
		// puffs every spacing_m along the path (and at least ten a second, should the point hang nearly still)
		double Done = Row.Spacing - std::min(Tr->Owed, Row.Spacing); // (the far form's spacing changes with the eye: never a puff laid behind the last)
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
		if (Laid == 0 && Now - Tr->LastPuff > 0.1 * FarK) // (the far form: as many times longer between)
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
				// (flame_tongues, art director 19:52Z 20 November: the one wide tongue a step read as flat orange shards lying on
				// the skin like cut paper. With the key, that many thin tongues a step, each rooted on the nacelle's skin at its
				// own place round it, yellow-white and past white at its root, orange along it and dark red at its tip, soft at
				// its edge, licking back and a little off the skin; the first is the long one, and the smoke, the torn-off lick
				// and the heart follow it. Without the key the one tongue, as it was)
				for (int N = 0; N < Row.FlameTongues; ++N)
				{
					const double Bearing = N == 0 ? 0.5 * Pi * Above : D.U(0.0, 2.0 * Pi);
					const Vec Out_ = Side * std::cos(Bearing) + Lift * std::sin(Bearing);
					const double TLen = N == 0 ? Len : Len * D.U(0.3, 0.8);
					const double Off = N == 0 ? StandOff : TLen * (Row.FlameLiftOfLen.B > 0.0 ? Row.FlameLiftOfLen.Pick(D) : 0.15);
					const Vec RootAt = Root + Out_ * (0.8 * Row.R0);
					const Vec Bend = N == 0 ? Curl : (Side * D.Bell() + Lift * D.Bell()) * (0.1 * TLen);
					const int Links = 4;
					for (int I = 0; I < Links; ++I)
					{
						const double At_ = (I + 0.5) / Links; // (0 at the skin to 1 at the tip)
						const Vec Ragged = (Side * D.Bell() + Lift * D.Bell()) * (TLen * 0.025 * (1.0 + 2.0 * At_));
						Bit B;
						B.Born = T;
						B.From = RootAt + Back * (TLen * At_) + Bend * (At_ * At_) + Out_ * (Off * std::pow(At_, std::max(Row.FlameLiftCurve, 1.0))) + Ragged;
						B.Vel = Vel + Back * (TLen * D.U(3.0, 6.0)); // (streaming aft off the nacelle)
						B.Life = D.U(0.05, 0.085);
						B.R0 = TLen * (0.095 - 0.055 * At_) * D.U(0.85, 1.15) * Row.FlameWide;
						B.R1 = B.R0 * 0.55; // (eaten away as it goes)
						B.Grow = 1.0;
						B.Along = (Back * TLen + Bend * (2.0 * At_)).Unit(Back);
						B.Stretch = D.U(2.8, 3.6) + At_;
						B.Point = 0.3 + 0.5 * At_; // (blunt on the skin, drawn to a point at its tip)
						B.Tail = 0.3;
						B.Col = At_ < 0.4 ? HeartCol.Mix(FlameCol, 2.0 * At_) : FlameCol.Mix(DeepCol, (At_ - 0.4) / 0.6);
						B.Col2 = DeepCol * 0.7;
						if (Row.bFlameTip && At_ > 0.6)
						{
							B.Col = B.Col.Mix(Row.FlameTip, (At_ - 0.6) / 0.4);
							B.Col2 = Row.FlameTip.Mix(Row.ColFull, 0.4);
						}
						B.Over = 1.0 + (Row.FlameOver - 1.0) * std::max(0.0, 1.0 - 2.5 * At_); // (its root burns past white)
						B.Alpha = Row.FlameAlphaRoot - Row.FlameAlphaFall * At_;
						B.FadeIn = 0.002;
						B.Hold = Row.FlameHold;
						B.Heart = 0.42 - 0.12 * At_;
						B.Rim = 0.0;
						B.Rough = 0.25;
						B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
						B.Turn = D.U(0.0, 2.0 * Pi);
						B.bLight = true;
						Bits.push_back(B);
					}
				}
				const int Pieces = Row.FlameTongues > 0 ? 0 : 4;
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
					if (Row.FlameTongues > 0)
					{
						B.R0 = std::max(B.R0, 0.9 * Row.R0); // (the tongues' roots are one yellow-white heart round the nacelle)
						B.Over = Row.FlameOver;
					}
					B.R1 = B.R0;
					B.Along = Back;
					B.Stretch = 1.6;
					B.LeastPx = Row.FlameLeastPx; // (art director 04:40Z: from the strategy camera a fire is never under 4 px)
					B.LeastKeep = 0.8;
					B.Point = 0.4;
					B.Tail = 0.3;
					if (Row.FlameTongues > 0)
					{
						B.Tail = 0.0;
						B.From = Root + Back * (B.Stretch * B.R0); // (its front at the fire's own point, never ahead of the nacelle)
					}
					B.Col = HeartCol;
					B.Col2 = HeartCol.Mix(FlameCol, 0.5);
					B.Alpha = 1.0;
					B.FadeIn = 0.002;
					B.Hold = 0.5;
					B.Heart = 0.95;
					B.Rim = 0.3;
					if (Row.FlameTongues > 0)
					{
						B.Heart = 0.55; // (a glow round the tongues' roots, soft at its edge)
						B.Rim = 0.0;
					}
					B.Rough = 0.2;
					B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
					B.Turn = D.U(0.0, 2.0 * Pi);
					B.bLight = true;
					Bits.push_back(B);
				}
				Tr->NextFlame += 1.0 / 60.0;
			}
		}
		if (StreamM > 0.5)
		{
			// The stream is laid in the air and left behind, as the smoke is: each tongue is born at the source going on
			// at a share of the aircraft's speed (stream_carried), so the aircraft flies out from under it, and it lives
			// as long as that takes to put it the stream's length behind. So the flame is as long as ruled at any speed,
			// its tongues part as they go back, each on its own way, and its far end is separate torn flames among the
			// smoke, never a sheet. Two sorts: short pale ones, burning past white, which are the bright source; and the
			// long ones, orange at their forward end and deep red (sootier by the look) at their rear, eaten away as they
			// go so they end thin, never fading out as a pale smear. Its own dice: the smoke's are thrown as they were.
			const double Speed = Carry.Length();
			const Vec Back = -Carry.Unit(Vec(1.0, 0.0, 0.0));
			const Vec Side = Back.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
			const Vec Lift = Side.Cross(Back).Unit(ZUp);
			const double SpanIs = Tr->Span > 0.0 ? Tr->Span : 11.0;
			const double Wide = Row.StreamWide * std::sqrt(std::max(SpanIs, 1.0) / 11.0); // (a bomber's tongues wider, not three times wider)
			const double Away = std::max(5.0, Speed * (1.0 - Row.StreamCarried)); // (how fast the aircraft leaves a tongue behind)
			const double Lives = std::clamp(StreamM / Away, 0.06, 1.5);
			const double Every = std::clamp(0.3 * (2.0 * Wide * 2.6) / Away, 1.0 / 120.0, 0.25);
			const bool bRoom = static_cast<double>(RoomGate()) < 0.85 * static_cast<double>(MaxPuffs); // (a full room: one strand, and no pale ones)
			const int Strands = bRoom ? Row.StreamStrands : 1;
			const Rgb Rear = Row.StreamDeep.Mix(Row.StreamSootCol, StreamSooty);
			Tr->NextStream = std::max(Tr->NextStream, Now - 0.1);
			for (int Guard = 0; Tr->NextStream <= Now && Guard < 12; ++Guard)
			{
				const double T = Tr->NextStream;
				const double K = Between > 1e-9 ? Clamp01((T - Tr->Last) / Between) : 1.0;
				const Vec Root = Tr->At + Path * K;
				const Vec Vel = Tr->Carry + (Carry - Tr->Carry) * K;
				Dice Sd((static_cast<uint64_t>(static_cast<int64_t>(Tr->Id)) * 0x9E3779B97F4A7C15ull) ^ (static_cast<uint64_t>(static_cast<int64_t>(T * 1000.0)) * 0xD1B54A32D192ED03ull) ^ 0x57EAull);
				for (int N = 0; N < Strands + (bRoom ? 2 : 0); ++N)
				{
					const bool bPale = N >= Strands; // (the last two: short pale ones at the source)
					const Vec Out_ = (Side * Sd.Bell() + Lift * Sd.Bell()) * 0.5;
					Bit B;
					B.Born = T;
					// (a tongue dies anywhere along the stream, the longer-lived the bigger: so the fire is thick at its
					// source and thins to a few long separate tongues at its end)
					const double Far = bPale ? Sd.U(0.28, 0.5) : Sd.U(0.3, 1.1);
					B.Life = Lives * Far;
					B.R0 = Wide * (bPale ? Sd.U(0.6, 0.95) : (0.6 + 0.6 * Far) * Sd.U(0.8, 1.2));
					B.R1 = B.R0 * (bPale ? 1.1 : Sd.U(0.4, 0.6));
					B.Grow = 1.0;
					B.Stretch = bPale ? Sd.U(1.6, 2.2) : Sd.U(2.2, 3.0) + Far;
					B.Along = (Back + Out_ * 0.3).Unit(Back);
					B.From = Root + Out_ * Wide + B.Along * (B.R0 * B.Stretch); // (its forward end at the source)
					B.Vel = Vel * (Row.StreamCarried * Sd.U(0.7, 1.3)) + (Side * Sd.Bell() + Lift * (Sd.Bell() + 0.3)) * (Row.StreamRagged * Wide / Lives);
					B.Drag = 1.5;
					B.Windage = 0.5;
					B.Point = Sd.U(0.3, 0.55);
					B.Tail = 0.3;
					B.Over = bPale ? Row.StreamOver : 1.0 + 0.4 * (Row.StreamOver - 1.0);
					B.Col = bPale ? Row.StreamHot : Row.StreamBody;
					B.Col2 = bPale ? Row.StreamBody : Rear;
					B.Cool = bPale ? 1.0 : Far / 0.9; // (by how far back it is, not by how much of its own life is gone: orange up the stream, the rear's colour only at its end)
					B.Hub = bPale ? 0.4 : 0.6;
					B.Edge = (bPale ? Row.StreamBody : Rear.Mix(Row.StreamDeep, 0.5)) * (1.0 / B.Over);
					B.EdgeK = 1.0;
					B.EdgeIn = bPale ? 0.3 : 0.45;
					B.Inner = bPale ? 0.4 : 0.45; // (soft-edged: a flame, never cut paper)
					B.Alpha = 1.0;
					B.FadeIn = 0.004;
					B.Hold = bPale ? 0.5 : 0.8;
					B.Heart = 0.9;
					B.Rim = 0.0;
					B.Rough = Sd.U(0.3, 0.45);
					B.LumpSeed = static_cast<uint32_t>(Sd.U() * 1073741823.0);
					B.Turn = Sd.U(0.0, 2.0 * Pi);
					B.Nearer = 0.5 * SpanIs; // (sorted nearer the eye than its own smoke, as a ship's fire is: the fire is this fall's read)
					B.bLight = true;
					Bits.push_back(B);
				}
				Tr->NextStream += Every;
			}
		}
		Tr->Last = Now;
		Tr->At = At;
		Tr->Carry = Carry;
	}

	void Battle::PlumeMove(double Now, Trailing& Tr, const TrailRow& Row, const Vec& At, const Vec& Carry, double PowerIn)
	{
		// A funnel's plume, the art director's ruling of 05:30Z 21 November. The borrowed air trail laid a piece every
		// few metres of the ship's way and kept one in six, each a tall strip: a picket fence astern at a ship's speed.
		// A funnel's smoke is laid by TIME into the air the ship steams through. Each puff keeps none of her speed: it
		// hangs where it was let go, drifts with the world's wind and climbs, fast at first and ever slower (its height
		// as the two thirds power of its age, and less the stronger the wind she feels), so astern of a fast ship the
		// plume lies low and long, and over a stopped one it stands up. Round puffs turned to the eye, never strips:
		// one every third of a puff's width of felt wind (the row's overlap), and as they widen with age only as many hang on as still
		// overlap three deep (puff n lives by the n-th number of an even spread: every other one goes first, then
		// every other of the rest), so the plume is unbroken at every age and its old end costs few pieces. With the
		// row's even they go by halves instead, so the ones left always stand evenly: see where a puff's life is set.
		const TrailRow::PlumeRow& P = Row.Plume;
		const Vec Blown = Wind.X == Wind.X && Wind.Y == Wind.Y && Wind.Z == Wind.Z ? Wind : Vec();
		if (Now < Tr.Last || Now - Tr.Last > 0.5)
		{
			// (a long silence or time going back: it starts again from here, with a puff at once and no surge)
			Tr.Last = Now;
			Tr.At = At;
			Tr.Carry = Carry;
			Tr.PlumeOwed = 1.0e9;
			Tr.PowerLate = -1.0;
		}
		const double Between = Now - Tr.Last;
		const Vec Path = At - Tr.At;
		// (how hard she is steaming: what the scene says, or her speed over the row's top speed)
		const double Power = PowerIn >= 0.0 ? std::min(PowerIn, 1.0) : Clamp01(std::min(Carry.Length(), 1.0e6) / P.TopSpeed);
		if (Tr.PowerLate < 0.0)
		{
			Tr.PowerLate = Power;
		}
		else
		{
			Tr.PowerLate += (Power - Tr.PowerLate) * (1.0 - std::exp(-Between / P.SurgeWithin));
		}
		if (Power - Tr.PowerLate >= P.SurgeRise && Now >= Tr.SurgeUntil && P.SurgeFor.B > 0.0)
		{
			// (she rings up: unburnt oil goes up the funnel black until the boilers catch up. How long by its own dice)
			Dice Ds(Mix64(static_cast<uint64_t>(static_cast<int64_t>(Tr.Id)) * 0xD6E8FEB86659FD93ull ^ static_cast<uint64_t>(Tr.PlumeN)) | 1ull);
			Tr.SurgeFrom = Now;
			Tr.SurgeUntil = Now + P.SurgeFor.Pick(Ds);
		}
		// (the wind she feels over her funnel, the funnel's width, and a puff's width at its lip: never under the
		// row's pixels in the last frame's picture, so a far ship's plume is fewer, bigger puffs)
		const double Felt = std::clamp((Blown - Carry).Length(), P.FeltLeast, 60.0);
		const double Funnel = Tr.Span > 0.0 ? SafeNum(Tr.Span, 1.0, 14.0, P.Funnel) : P.Funnel;
		// (a wide funnel's puffs are laid closer, the row's overlap_wide deep at wide_funnel_m and wider. The art director, 16:03Z
		// 21 November, of a capital ship's: "beads read at any range", and a big puff is more of the picture to see them in)
		const double DeepOwn = P.OverlapWide > P.Overlap ? P.Overlap + (P.OverlapWide - P.Overlap) * Clamp01((Funnel - P.Funnel) / std::max(P.WideFunnel - P.Funnel, 1e-3)) : P.Overlap;
		double Lip = P.Lip * Funnel, Deep = DeepOwn;
		if (bEyeKnown)
		{
			const double Away = (At - LastEye).Length(), Ppr = SafeNum(LastPpr, 1.0, 1.0e7, 1000.0); // (held: a lens that is no number lays no puff that is none)
			if (Away == Away && Away > 1.0)
			{
				// (and where its lip is small in the picture the puffs overlap less deep, each as much denser)
				Deep = P.OverlapFar + (DeepOwn - P.OverlapFar) * Clamp01((Lip * Ppr / Away - P.FarPx.A) / (P.FarPx.B - P.FarPx.A));
				if (P.LipLeastPx > 0.0)
				{
					Lip = std::max(Lip, std::min(P.LipLeastPx * Away / Ppr, P.LipMost * Lip));
				}
			}
		}
		Deep = SafeNum(Deep, 1.0, DeepOwn, DeepOwn);
		const double Every = SafeNum(Lip / (Deep * Felt), 0.02, 60.0, 0.1);
		const double WideK = P.WideK * std::pow(Funnel, P.WideByFunnel) * std::pow(Felt, 2.0 / 3.0); // (its width grows by this times its age to the two thirds)
		const double Climb = P.RiseK * std::sqrt(Funnel) / std::cbrt(Felt); // (and its height by this)
		const double FullK = Clamp01((Power - P.CruiseTo) / std::max(P.FullFrom - P.CruiseTo, 1e-3));
		const double Full = FullK * FullK * (3.0 - 2.0 * FullK);
		int Laid = 0;
		double T = Tr.Last + std::max(0.0, Every - Tr.PlumeOwed);
		for (; T <= Now && Laid < 32; T += Every, ++Laid)
		{
			const double K = Between > 1e-9 ? Clamp01((T - Tr.Last) / Between) : 1.0;
			const unsigned N = Tr.PlumeN++;
			Dice D(Mix64(static_cast<uint64_t>(static_cast<int64_t>(Tr.Id)) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(N) * 0xD1B54A32D192ED03ull ^ 0xF0E1ull) | 1ull);
			// (the look: cruising haze to full power's dark smoke, and the black surge over both, in over 2 s and out over 5)
			const double Surge = T < Tr.SurgeUntil ? Clamp01((T - Tr.SurgeFrom) / 2.0) * Clamp01((Tr.SurgeUntil - T) / 5.0) : 0.0;
			auto Blend = [&](double Cruise, double FullIs, double SurgeIs) { const double Is = Cruise + (FullIs - Cruise) * Full; return Is + (SurgeIs - Is) * Surge; };
			const double AlphaIs = Blend(P.Cruise.Alpha, P.Full.Alpha, P.Surge.Alpha), FadeS = Blend(P.Cruise.FadeS, P.Full.FadeS, P.Surge.FadeS), FadePow = Blend(P.Cruise.FadePow, P.Full.FadePow, P.Surge.FadePow);
			const double AgeS = Blend(P.Cruise.AgeS, P.Full.AgeS, P.Surge.AgeS), OldBy = Blend(P.Cruise.OldBy, P.Full.OldBy, P.Surge.OldBy);
			const Rgb ColIs = P.Cruise.Col.Mix(P.Full.Col, Full).Mix(P.Surge.Col, Surge), OldIs = P.Cruise.Old.Mix(P.Full.Old, Full).Mix(P.Surge.Old, Surge);
			// (how long this one hangs: until the plume is so wide that the ones that outlive it overlap three deep
			// without it, and a quarter longer, thinning away over its last fifth)
			uint32_t Bits_ = N;
			Bits_ = ((Bits_ >> 1) & 0x55555555u) | ((Bits_ & 0x55555555u) << 1);
			Bits_ = ((Bits_ >> 2) & 0x33333333u) | ((Bits_ & 0x33333333u) << 2);
			Bits_ = ((Bits_ >> 4) & 0x0F0F0F0Fu) | ((Bits_ & 0x0F0F0F0Fu) << 4);
			Bits_ = ((Bits_ >> 8) & 0x00FF00FFu) | ((Bits_ & 0x00FF00FFu) << 8);
			Bits_ = (Bits_ >> 16) | (Bits_ << 16);
			const double Share = (static_cast<double>(Bits_) + 0.5) / 4294967296.0;
			const double Needed = WideK > 1e-6 ? std::pow(Lip * (1.0 / Share - 1.0) / WideK, 1.5) : AgeS;
			// (laid evenly, the row's even. The art director on the first sheets, 14:13Z 21 November: "sky between" the puffs
			// in the first 100 m astern, a dark knot at the lip, a gap at 10 m, knots and gaps after it. Thinned by the
			// even spread above the puffs that hang on stand 2, 2 and 4 places apart, and a puff is thick only about its
			// middle: the plume was half as deep in every third gap. So here every other puff goes at once, each time the
			// plume has doubled its width: the ones left always stand evenly, one place, then two, then four apart. A
			// puff's opacity falls as the plume widens (the same smoke, laid over more of them), and over the last share
			// of each doubling the ones that go thin away to nothing while the ones between take their smoke up, so the
			// smoke along the plume is the same before, through and after it. Bit::Even, worked out in LayBit)
			int Halvings = N == 0u ? 31 : 0; // (how many halvings it lives through: puff n goes at the first that n is not a multiple of)
			for (unsigned Left = N; Left != 0u && (Left & 1u) == 0u; Left >>= 1)
			{
				++Halvings;
			}
			const double Widens = Lip > 1e-6 ? std::min(WideK / Lip, 1.0e6) : 0.0;
			// (how many lips wide the plume is at its next halving after those: the row's first even_near come at each
			// doubling of its width, the next even_far sooner, each when it is even_step times wider, and the rest at each
			// doubling again. Far astern the plume is faint and soft and needs fewer puffs deep than at the funnel)
			const int Next = std::min(Halvings + 1, 30), NextNear = std::min(Next, P.EvenNear), NextMid = std::clamp(Next - P.EvenNear, 0, P.EvenFar);
			const double HalvedAt = std::ldexp(1.0, NextNear + std::max(Next - P.EvenNear - P.EvenFar, 0)) * std::pow(P.EvenStep, NextMid);
			const double Goes = P.Even > 0.0 && Widens > 1e-9 ? std::pow((HalvedAt - 1.0) / Widens, 1.5) : AgeS;
			const double Size = D.U(1.0 - P.Size, 1.0 + P.Size);
			// (its climb and its drift across the wind wander slowly along the plume, so it meanders and is no ruled bar)
			const double Keeps = std::exp(-Every / 3.0), Fresh = std::sqrt(1.0 - Keeps * Keeps); // (over about three seconds, whatever her speed)
			Tr.PlumeRise = 1.0 + (Tr.PlumeRise - 1.0) * Keeps + 0.13 * Fresh * D.Bell();
			Tr.PlumeSide = Tr.PlumeSide * Keeps + 0.5 * Fresh * D.Bell();
			const double RiseK = std::clamp(Tr.PlumeRise, 0.6, 1.4) * D.U(0.9, 1.1);
			const Vec Across = (Blown - Carry).Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
			Bit B;
			B.Born = T;
			B.Life = std::min(AgeS, std::max(0.6, 1.25 * Needed)) * D.U(0.9, 1.0);
			if (P.Even > 0.0)
			{
				B.Life = std::clamp(Goes, 0.05, AgeS);
				B.Even = Halvings;
				B.EvenK = Widens;
				B.EvenWin = P.Even;
				B.EvenNear = P.EvenNear;
				B.EvenFar = P.EvenFar;
				B.EvenStep = std::log2(P.EvenStep);
				B.EvenN = N;
				B.EvenSeed = static_cast<uint32_t>(Mix64(static_cast<uint64_t>(static_cast<int64_t>(Tr.Id)) * 0xA24BAED4963EE407ull ^ 0x1F3Dull) >> 32);
				B.EvenRollLift = P.RollLift;
				B.EvenRollShade = P.RollShade;
				B.EvenRollRun = P.RollRun;
			}
			B.From = Tr.At + Path * K + Vec(D.Bell(), D.Bell(), 0.5 * D.Bell()) * (0.08 * Lip);
			B.Vel = ZUp * (0.544 * Climb * RiseK) + Across * (0.5 * std::clamp(Tr.PlumeSide, -1.0, 1.0));
			B.Drag = 0.16;
			B.Rise = 0.21 * Climb * RiseK;
			B.Windage = 1.0;
			B.R0 = 0.5 * Lip * Size;
			B.R1 = B.R0 + 0.5 * WideK * Size * std::pow(B.Life, 2.0 / 3.0);
			B.Grow = 2.0 / 3.0;
			const double Shade = D.U(1.0 - P.Shade, 1.0 + P.Shade);
			B.Col = ColIs * Shade;
			B.Col2 = B.Col.Mix(OldIs * (0.5 + 0.5 * Shade), std::min(1.0, B.Life / OldBy));
			B.Cool = 1.0;
			const double AlphaDie = D.U(0.88, 1.0);
			B.Alpha = std::min(0.97, AlphaIs * (P.Overlap / Deep) * AlphaDie);
			if (DeepOwn > P.Overlap)
			{
				// (laid closer, each holds as much less smoke: by the light it lets through, so the plume is as dense as the row says)
				B.Alpha = std::min(0.97, (1.0 - std::pow(1.0 - std::min(AlphaIs, 0.999), P.Overlap / DeepOwn)) * (DeepOwn / Deep) * AlphaDie);
			}
			B.AgeFade = FadeS;
			B.AgeFadePow = FadePow;
			B.FadeIn = 0.08;
			B.Hold = P.Even > 0.0 && Goes < AgeS ? 1.0 : 0.8; // (laid evenly, one that goes at a halving thins by Bit::Even alone)
			B.Heart = 0.7;
			B.HeartRound = P.HeartRound;
			B.Rim = 0.0;
			B.Rough = P.Rough;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.TurnRate = D.U(-0.3, 0.3); // (its lumps roll slowly)
			if (P.ShadowK > 0.0)
			{
				// (its side away from the sun: darker and a little cool)
				B.ShadowCol = Rgb(static_cast<float>(B.Col.R * P.ShadowOfLit * 0.96), static_cast<float>(B.Col.G * P.ShadowOfLit), static_cast<float>(B.Col.B * P.ShadowOfLit * 1.05));
				B.ShadowK = P.ShadowK;
			}
			Bits.push_back(B);
			Tr.PlumeOwed = 0.0;
			Tr.LastPuff = T;
		}
		Tr.PlumeOwed = Laid > 0 ? Now - Tr.LastPuff : std::min(Tr.PlumeOwed + Between, 1.0e9);
		Tr.Last = Now;
		Tr.At = At;
		Tr.Carry = Carry;
		Tr.LastPuffAt = At;
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

	void Battle::AirFall(double Now, int Id, const Vec& At, const Vec& Velocity, const char* Kind, double SpanM, double DeckZ, int Look, bool bBombs)
	{
		// The art director, 04:40Z: a fall throws a debris row. 30 to 50 pieces for a fighter, 60 to 100 for a bomber;
		// explode sheds them all at once with a short torn fireball, never a ball; burn sheds 5 to 10 over its fall;
		// spin a few, sometimes a wing. Nothing pops out of view: pieces end in the cloud deck, or the haze or the ground.
		if (!RealPlace(At) || !RealPlace(Velocity) || !(std::fabs(Now) <= 1.0e12))
		{
			return; // (as TrailStart: a place, a speed or a time that is no number, or without end, is no fall)
		}
		const std::string K = Kind && *Kind ? Kind : "burn";
		const std::string P = "air_debris.";
		const double DeckIs = SafeNum(DeckZ, -1.0e9, 1.0e9, At.Z - 3000.0); // (a deck that is no number: one three kilometres down)
		AirFallStop(Id);
		Falling Fa;
		Fa.Id = Id;
		Fa.bOn = true;
		Fa.Span = std::clamp(SpanM > 0.0 ? SpanM : 11.0, 2.0, 200.0); // (held: no span without end makes a cloud without end)
		Fa.DeckZ = DeckIs;
		Fa.At = At;
		Fa.Vel = Velocity;
		Fa.Last = Now;
		Fa.Seed = ++Seeds * 73856093ull ^ PlaceKey(At);
		Dice D(Fa.Seed);
		const bool bBomber = Fa.Span >= Num(P + "bomber_span_m", 20.0);
		// (the ruling of 15:02Z 20 November, air-fates-look A, B and E. A fall's look, 0 to 3, is the event's: the kit
		// throws no dice for it. Far off, its span under far_px on the screen, or with the room four fifths full, a fall
		// throws nothing: an explosion keeps its flash and its black cloud. Only with the rows in the recipe: without
		// them every fall is as it was, by the same dice)
		const std::string LookNo = "." + std::to_string(Look < 0 ? 1 : std::min(Look, 3));
		const double FarPx = SafeNum(Num(P + "far_px", 0.0), 0.0, 1.0e4, 0.0);
		const bool bNear = !(FarPx > 0.0 && bEyeKnown && Fa.Span * SafeNum(LastPpr, 1.0, 1e7, 1000.0) / std::max(1.0, (LastEye - At).Length()) < FarPx)
			&& static_cast<double>(RoomGate()) < 0.8 * static_cast<double>(MaxPuffs);
		if (K == "explode")
		{
			const std::string X = P + "explode.";
			const bool bRuled = Has(X + "across_fighter.0");
			const Span Row = Range(P + (bBomber ? "bomber" : "fighter"), bBomber ? 60.0 : 30.0, bBomber ? 100.0 : 50.0);
			int Count = static_cast<int>(std::lround(std::clamp(Row.Pick(D), 0.0, 400.0))); // (held: a slip of the recipe never floods the frame)
			if (bRuled)
			{
				Count = bNear ? std::min(400, static_cast<int>(std::lround(Count * SafeNum(Num(X + "pieces" + LookNo, 1.0), 0.0, 2.0, 1.0)))) : 0; // (held again after the look's share: the pre-check on 1fc2904)
			}
			const double Heavy = Clamp01(Num(P + "heavy_share", 0.12));
			const Span Throw = Range(P + "throw_mps", 8.0, 35.0);
			for (int I = 0; I < Count; ++I)
			{
				const Vec Toward = Vec(D.Bell(), D.Bell(), D.Bell() + 0.2).Unit(ZUp);
				Shed(Now, At + Toward * (0.25 * Fa.Span * D.U()), Velocity, Toward * Throw.Pick(D), Fa.Span, DeckIs, D.U() < Heavy ? 1 : 0, D);
			}
			if (!bRuled)
			{
				const size_t First = Bits.size();
				TornFireball(Now, At, Velocity, Fa.Span, D);
				LingerFrom(First, LingerOf("burst"), Now, At, 1.0, 0.0);
				return; // (nothing more falls: the model is gone)
			}
			const double Bombs = bBombs ? SafeNum(Num(X + "bombs_across", 1.3), 1.0, 3.0, 1.3) : 1.0;
			const double Across = Bombs * SafeNum(Num(X + (bBomber ? "across_bomber" : "across_fighter") + LookNo, bBomber ? 1.75 : 1.25), 0.2, 6.0, bBomber ? 1.75 : 1.25);
			const double Hang = SafeNum(Num(X + "hang_s" + LookNo, 10.0), 1.0, 120.0, 10.0);
			{
				// (the cloud an aircraft leaves where it blew up goes slowly: linger.burst)
				const size_t First = Bits.size();
				TornFireball(Now, At, Velocity, Fa.Span, D, Across, Hang, bBombs ? SafeNum(Num(X + "bombs_heart", 1.6), 1.0, 4.0, 1.6) : 1.0);
				LingerFrom(First, LingerOf("burst"), Now, At, 1.0, 0.0);
			}
			if (bNear)
			{
				int Streaks = static_cast<int>(SafeNum(Num(X + "streaks" + LookNo, 3.0), 0.0, 8.0, 3.0));
				if (bBombs)
				{
					Streaks = std::max(Streaks, static_cast<int>(SafeNum(Num(X + "bombs_streaks", 3.0), 0.0, 8.0, 3.0))); // (the ruling: always with bombs aboard)
				}
				FuelStreaks(Now, At, Velocity, Fa.Span, Streaks, Fa.Seed ^ 0x57EA4Bull, 0.5 * Across * Fa.Span);
				MetalSparkle(Now, At, Velocity, Fa.Span, static_cast<int>(SafeNum(Num(X + "sparkle", 40.0), 0.0, 200.0, 40.0)), 1.0, Fa.Seed ^ 0x5BA4C1Eull);
			}
			return;
		}
		const std::string W = P + "ways." + K + ".";
		if (Has(W + "burst.0"))
		{
			// (a way that starts with a break or a blow: small torn panels at once, a sparkle of metal, sparks where two
			// aircraft meet; then a few more pieces over the fall. The big pieces are the model's own. By its own dice)
			Dice Wd(Fa.Seed ^ 0xB0A57ull);
			const double LookK = 0.7 + 0.2 * (Look < 0 ? 1.5 : std::min(Look, 3));
			const Span Burst = Range(W + "burst", 0.0, 0.0), Sheds = Range(W + "sheds", 0.0, 0.0);
			const int Count = bNear ? static_cast<int>(std::lround(std::clamp(Burst.Pick(Wd) * LookK * (bBomber ? 1.5 : 1.0), 0.0, 120.0))) : 0;
			const double Heavy = Clamp01(Num(P + "heavy_share", 0.12));
			const Span Loose = Range(P + "loose_mps", 3.0, 12.0);
			for (int I = 0; I < Count; ++I)
			{
				const Vec Toward = Vec(Wd.Bell(), Wd.Bell(), Wd.Bell()).Unit(ZUp);
				Shed(Now, At + Toward * (0.1 * Fa.Span * Wd.U()), Velocity, Toward * (Loose.Pick(Wd) * 1.5), Fa.Span, DeckIs, Wd.U() < Heavy ? 1 : 0, Wd);
			}
			if (bNear)
			{
				MetalSparkle(Now, At, Velocity, Fa.Span, static_cast<int>(SafeNum(Num(W + "sparkle", 0.0), 0.0, 200.0, 0.0) * LookK), 1.0, Fa.Seed ^ 0x5BA4C1Eull);
				const int Sparks = static_cast<int>(SafeNum(Num(W + "sparks", 0.0), 0.0, 64.0, 0.0));
				const Rgb SparkCol = Picture("air_hit.spark_colour", Rgb::FromPicture(255, 236, 180));
				for (int I = 0; I < Sparks; ++I)
				{
					// (sparks where metal meets metal: short bright streaks thrown out and carried on, gone in a third of a second)
					const Vec Toward = Vec(Wd.Bell(), Wd.Bell(), Wd.Bell()).Unit(ZUp);
					Bit B;
					B.Born = Now + Wd.U(0.0, 0.08);
					B.From = At + Toward * (0.1 * Fa.Span * Wd.U());
					B.Vel = Velocity * 0.9 + Toward * Wd.U(15.0, 45.0);
					B.Drag = 1.5;
					B.Gravity = 9.8;
					B.Life = Wd.U(0.15, 0.35);
					B.R0 = Wd.U(0.05, 0.1);
					B.R1 = 0.5 * B.R0;
					B.Along = Toward;
					B.Stretch = Wd.U(4.0, 8.0);
					B.Col = SparkCol;
					B.Col2 = FlameCol;
					B.Over = 2.0;
					B.Alpha = 1.0;
					B.FadeIn = 0.002;
					B.Hold = 0.5;
					B.Heart = 1.0;
					B.Rim = 0.3;
					B.LeastPx = 1.5;
					B.LeastKeep = 0.8;
					B.bLight = true;
					Bits.push_back(B);
				}
			}
			Fa.Left = static_cast<int>(std::lround(std::clamp(Sheds.Pick(Wd), 0.0, 64.0)));
			if (Fa.Left <= 0 || !bNear)
			{
				return; // (nothing more comes off it)
			}
			Fa.Next = Now + Range(P + "first_s", 0.3, 1.5).Pick(Wd);
			Fa.Seed = static_cast<uint64_t>(Wd.U() * 9007199254740991.0) ^ Fa.Seed;
			Falls.push_back(Fa);
			return;
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

	void Battle::FuelStreaks(double Now, const Vec& At, const Vec& Vel, double SpanM, int Count, uint64_t Seed, double CloudR)
	{
		// (the art director's ruling B: "2 to 5 burning fuel streaks curve down, orange heads with black tails". Each
		// is a clot of burning fuel thrown out of the fire cloud and carried on, slowing and curving over to fall: a
		// ragged orange head, shrinking as it burns out; flame torn off it down the first of its path; and its black
		// smoke left along the whole path, each length drawn along the way the head was going there, hanging and
		// greying after it. All laid at the call, each piece born when the head gets to it)
		const std::string P = "air_debris.explode.streak.";
		Dice D(Seed);
		const double S = std::sqrt(std::max(SpanM, 1.0) / 11.0);
		const Span Burns = Range(P + "burns_s", 1.5, 3.0), Thrown = Range(P + "throw_mps", 10.0, 28.0), HeadM = Range(P + "head_m", 0.5, 0.9), SmokeS = Range(P + "smoke_s", 3.0, 6.0);
		const Span Sinks = Range(P + "sink_mps", 16.0, 28.0);
		const double Carry = SafeNum(Num(P + "carry", 0.6), 0.0, 1.0, 0.6), Slows = SafeNum(Num(P + "drag", 2.5), 0.1, 20.0, 2.5);
		const int Steps = static_cast<int>(SafeNum(Num(P + "steps", 10.0), 2.0, 24.0, 10.0));
		const double SmokeAlpha = SafeNum(Num(P + "smoke_alpha", 0.85), 0.0, 1.0, 0.85), Over = SafeNum(Num(P + "over", 1.6), 1.0, 8.0, 1.6);
		const Rgb Hot = Picture(P + "hot", Rgb::FromPicture(255, 196, 70)), Deep = Picture(P + "deep", Rgb::FromPicture(196, 72, 24)), Smoke = Picture(P + "smoke", Rgb::FromPicture(34, 32, 31));
		for (int I = 0; I < Count; ++I)
		{
			const Vec Toward = Vec(D.Bell(), D.Bell(), D.Bell() + 0.4).Unit(ZUp);
			// (a big aircraft's clots are bigger: they burn longer, fall faster and are thrown harder, as they must to come
			// out of its bigger cloud)
			const double Burn = SafeNum(Burns.Pick(D), 0.2, 20.0, 2.0) * S;
			const double Sink = SafeNum(Sinks.Pick(D), 1.0, 200.0, 22.0) * std::sqrt(S);
			// (a clot is heavier than the fire cloud's gas: thrown at the blast and carried on, it is slowed less than the
			// cloud is, so it comes out of the cloud's front or side a moment after the flash and curves over the whole of
			// its burn, from the way the aircraft was going toward falling at its own speed)
			const double Late = D.U(0.4, 0.65);
			Bit H;
			H.From = At + Toward * (std::max(0.2 * SpanM, CloudR) * D.U(0.3, 0.8));
			H.Vel = Vel * (Carry * D.U(0.9, 1.3)) + Toward * (SafeNum(Thrown.Pick(D), 0.0, 200.0, 18.0) * std::clamp(CloudR / 7.0, 1.0, 6.0)) + ZUp * Sink;
			H.Drag = Slows;
			H.Rise = -Sink;
			H.Windage = 0.3;
			H.From = Where(H, Late); // (seen from Late on, as it leaves the cloud)
			H.Vel = H.Vel * std::exp(-Slows * Late);
			H.Born = Now + Late;
			H.Life = Burn;
			H.R0 = 0.5 * SafeNum(HeadM.Pick(D), 0.05, 10.0, 0.7) * S;
			H.R1 = 0.35 * H.R0;
			H.Grow = 1.5;
			H.Col = Hot;
			H.Col2 = Deep;
			H.Cool = 1.0;
			H.Over = Over;
			H.Edge = Deep * (1.0 / Over);
			H.EdgeK = 1.0;
			H.EdgeIn = 0.4;
			H.Inner = 0.5;
			H.Alpha = 1.0;
			H.FadeIn = 0.02;
			H.Hold = 0.85;
			H.Heart = 0.95;
			H.Rim = 0.0;
			H.Rough = 0.45;
			H.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			H.LeastPx = 3.0;
			H.LeastKeep = 0.9;
			H.Nearer = 0.5 * SpanM; // (over its own smoke)
			H.bLight = true;
			const double Step = Burn / Steps;
			for (int N = 0; N < Steps; ++N)
			{
				const double A0 = N * Step, A1 = A0 + Step;
				const Vec P0 = Where(H, A0), P1 = Where(H, A1);
				const double Seg = (P1 - P0).Length();
				const Vec Way = (P0 - P1).Unit(ZUp); // (back up the path)
				const double Left = 1.0 - (N + 0.5) / Steps; // (the head burns down: less fire and less smoke off it toward its end)
				const double HeadR = H.R0 + (H.R1 - H.R0) * std::pow((N + 0.5) / Steps, H.Grow);
				// its smoke along this length of the path
				Bit M;
				M.Born = H.Born + A1;
				M.From = (P0 + P1) * 0.5 + Vec(D.Bell(), D.Bell(), D.Bell()) * (0.3 * HeadR);
				M.Vel = Going(H, A1) * 0.06;
				M.Drag = 2.0;
				M.Windage = 1.0;
				M.Rise = 0.3;
				M.Life = SafeNum(SmokeS.Pick(D), 0.3, 120.0, 4.0) * (0.6 + 0.4 * Left);
				M.R0 = HeadR * D.U(0.9, 1.25);
				M.R1 = M.R0 * D.U(1.7, 2.4);
				M.Grow = 0.6;
				M.Along = Way;
				M.Stretch = std::clamp(0.75 * Seg / std::max(M.R0, 0.05), 1.0, 7.0); // (each length reaches the next from its birth: one line, never dashes)
				M.Col = Smoke;
				M.Col2 = Smoke.Mix(Rgb::FromPicture(120, 116, 112), 0.6);
				M.Alpha = SmokeAlpha * (0.55 + 0.45 * Left) * D.U(0.85, 1.0);
				M.FadeIn = 0.05;
				M.Hold = 0.3;
				M.Heart = 0.9;
				M.Rim = 0.0;
				M.Rough = 0.5;
				M.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
				M.LeastPx = 1.5;
				M.LeastKeep = 0.5;
				Bits.push_back(M);
				// and the flame torn off the head here, a short lick left on the path
				Bit F;
				F.Born = H.Born + A0 + D.U(0.0, Step);
				F.From = Where(H, F.Born - H.Born) + Vec(D.Bell(), D.Bell(), D.Bell()) * (0.3 * HeadR);
				F.Vel = Going(H, F.Born - H.Born) * 0.35;
				F.Drag = 3.0;
				F.Life = D.U(0.18, 0.32);
				F.R0 = HeadR * D.U(0.8, 1.1);
				F.R1 = 0.4 * F.R0;
				F.Along = Way;
				F.Stretch = D.U(2.0, 3.2);
				F.Point = 0.5;
				F.Col = Hot.Mix(Deep, 0.4);
				F.Col2 = Deep;
				F.Over = 1.0 + 0.4 * (Over - 1.0);
				F.Alpha = 1.0;
				F.FadeIn = 0.005;
				F.Hold = 0.7;
				F.Heart = 0.9;
				F.Rim = 0.0;
				F.Inner = 0.45;
				F.Rough = 0.4;
				F.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
				F.LeastPx = 1.5;
				F.LeastKeep = 0.8;
				F.Nearer = 0.5 * SpanM;
				F.bLight = true;
				Bits.push_back(F);
			}
			Bits.push_back(H);
		}
	}

	void Battle::MetalSparkle(double Now, const Vec& At, const Vec& Vel, double SpanM, int Count, double Over, uint64_t Seed)
	{
		// (the art director's ruling B: "a sparkle of small metal bits catches the sun for 1 to 3 s". Each wink is one
		// bright point for a frame or two, somewhere in the cloud of small metal as it spreads, is carried on, slows
		// and sinks: laid at the call, each born at its own moment)
		const std::string P = "air_debris.sparkle.";
		Dice D(Seed);
		const Span For = Range(P + "for_s", 1.0, 3.0), Life = Range(P + "life_s", 0.02, 0.05);
		const double Size = SafeNum(Num(P + "size_m", 0.15), 0.01, 2.0, 0.15), LeastPx = SafeNum(Num(P + "least_px", 3.0), 0.0, 32.0, 3.0);
		const double Burns = SafeNum(Num(P + "over", 4.0), 1.0, 8.0, 4.0) * Over;
		const double Spread = SafeNum(Num(P + "spread_mps", 14.0), 0.0, 200.0, 14.0), Sink = SafeNum(Num(P + "sink_mps", 10.0), 0.0, 200.0, 10.0);
		const Rgb Col = Picture(P + "colour", Rgb::FromPicture(255, 250, 232));
		const double Until = SafeNum(For.Pick(D), 0.1, 20.0, 2.0);
		for (int I = 0; I < Count; ++I)
		{
			const double A = D.U(0.08, Until);
			const Vec Toward = Vec(D.Bell(), D.Bell(), D.Bell()).Unit(ZUp);
			Bit B;
			B.Born = Now + A;
			// (where the cloud of metal is by then: carried on and slowing, spreading, sinking)
			B.From = At + Vel * (0.7 * (1.0 - std::exp(-0.8 * A)) / 0.8) + Toward * (0.2 * SpanM + Spread * A * D.U(0.2, 1.0)) - ZUp * (Sink * A);
			B.Vel = Vel * (0.7 * std::exp(-0.8 * A)) - ZUp * Sink;
			B.Windage = 1.0;
			B.Life = SafeNum(Life.Pick(D), 0.008, 0.2, 0.03);
			B.R0 = B.R1 = Size * D.U(0.6, 1.4);
			B.Col = B.Col2 = Col;
			B.Over = Burns;
			B.Alpha = 1.0;
			B.FadeIn = 0.001;
			B.Hold = 0.6;
			B.Heart = 1.0;
			B.Rim = 0.3;
			B.Rough = 0.5;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.LeastPx = LeastPx;
			B.LeastKeep = 0.9;
			B.bLight = true;
			B.bWhole = true;
			Bits.push_back(B);
		}
	}

	void Battle::AirFallMove(double Now, int Id, const Vec& At, const Vec& Velocity)
	{
		for (Falling& Fa : Falls)
		{
			if (Fa.Id != Id || !Fa.bOn)
			{
				continue;
			}
			if (!RealPlace(At) || !RealPlace(Velocity) || !(std::fabs(Now) <= 1.0e12))
			{
				return; // (the fall waits where it was for a real place)
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

	void Battle::TornFireball(double Now, const Vec& At, const Vec& Vel, double SpanM, Dice& D, double Across, double Hang, double Whiter)
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
		int Lobes = static_cast<int>(std::lround(std::clamp(Range(P + "lobes", 18.0, 28.0).Pick(D), 0.0, 200.0)));
		// (Across over 0, the ruling of 15:02Z 20 November: "a ragged orange fire cloud of burning fuel, never a clean
		// sphere. About 1 to 1.5 wingspans across for a fighter and 1.5 to 2 for a bomber, bigger and whiter-hearted with
		// bombs aboard. Black smoke boils out of it within 0.3 s; the fire is gone in 0.5 to 1 s and the black cloud
		// hangs 5 to 20 s, ageing like flak." The clumps are thrown along the tears as before, each to its own place
		// within half of Across spans of the middle, so the cloud is as wide as ruled along its tears and ragged
		// between them; each burns up to fire_s, the shortest out within 0.3 s; its smoke is born round it as it dies,
		// as it always was, and hangs Hang seconds)
		const bool bRuled = Across > 0.0;
		const std::string X = "air_debris.explode.";
		const Span FireS = Range(X + "fire_s", 0.5, 1.0), LobeR = Range(X + "lobe_r", 0.13, 0.27), SmokeOut = Range(X + "smoke_grow", 1.5, 2.3);
		const double Rc = 0.5 * Across * SpanM, SmokeRough = SafeNum(Num(X + "smoke_rough", 0.5), 0.0, 1.0, 0.5);
		const Rgb EmberCol = Picture(X + "ember", Rgb::FromPicture(140, 40, 16));
		// (the art director on 3355503, 04:00Z 21 November: the big cloud's dying clumps were "a red flower", and the
		// bombs' heart "a flat pale plate". clump_edge: a ruled clump's rim colour, deep red without the key.
		// heart_edge: with it a ruled heart is brightest in its middle and falls off to that colour at a ragged edge,
		// a hot core in the fire; without it the white heart with a yellow edge it was)
		const Rgb ClumpEdge = Picture(X + "clump_edge", DeepCol);
		const bool bHeartCore = bRuled && Has(X + "heart_edge.0");
		const Rgb HeartEdge = Picture(X + "heart_edge", FireCol);
		const double HeartEdgeIn = SafeNum(Num(X + "heart_edge_in", 0.45), 0.0, 1.0, 0.45), HeartRough = SafeNum(Num(X + "heart_rough", 0.6), 0.0, 1.0, 0.6);
		const double HeartFall = SafeNum(Num(X + "heart_fall", 0.6), 0.0, 1.0, 0.6);
		if (bRuled)
		{
			Lobes = std::min(72, static_cast<int>(std::lround(Lobes * std::clamp(Across / 1.25, 0.8, 2.6))));
		}
		const int Ways = static_cast<int>(std::lround(std::clamp(Range(P + "tears", 2.0, 4.0).Pick(D), 1.0, 16.0)));
		const Span Life = Range(P + "life_s", 0.25, 0.6), Reach = Range(P + "reach_of_span", 0.3, 0.9), R = Range(P + "r_of_span", 0.05, 0.12);
		const Span SmokeS = Range(P + "smoke_s", 5.0, 9.0), SmokeGrow = Range(P + "smoke_grow", 5.0, 7.0);
		std::vector<Vec> Tears;
		for (int I = 0; I < Ways; ++I)
		{
			Tears.push_back(Vec(D.Bell(), D.Bell(), D.Bell()).Unit(ZUp));
		}
		// (the white heart, broken along the tears so it follows the clumps, never one flat disc: art director 06:09Z)
		// (a ruled cloud wider than a fighter's usual one has more hearts, further out along its tears, each a little
		// bigger: white-hot knots along the tears, never one flat white shape)
		const double Wider = bRuled ? std::clamp(Across / 1.25, 1.0, 2.6) : 1.0;
		const int Hearts = std::min(12, static_cast<int>(std::lround(std::clamp(Num(P + "hearts", 3.0), 1.0, 32.0) * Wider)));
		const double HeartR = Num(P + "heart_r_of_span", 0.11);
		for (int I = 0; I < Hearts; ++I)
		{
			const Vec Way = Tears[static_cast<size_t>(I % Ways)];
			Bit H;
			H.Born = Now;
			const double HeartU = D.U(0.03, 0.12), HeartK = (I < Ways ? 1.0 : Wider); // (spans out from the middle along its tear, and the later hearts' share more)
			H.From = At + Way * (HeartU * SpanM * HeartK);
			H.Vel = Vel * 0.8 + Way * (D.U(4.0, 10.0) * S);
			H.Drag = 2.0;
			if (bRuled)
			{
				H.Vel = Vel * 0.6 + Way * (D.U(4.0, 10.0) * S); // (carried as the clumps are: the heart stays inside its fire)
				H.Drag = 3.0;
			}
			H.Life = Num(P + "heart_s", 0.12) * D.U(0.9, 1.1) * Whiter;
			H.R0 = HeartR * SpanM * D.U(0.7, 1.1) * (bRuled ? std::sqrt(Whiter * Wider) : 1.0);
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
			if (bHeartCore)
			{
				// (one hot core, not a paving of pale tiles: a heart is whiter, bigger and drawn nearer the closer it lies
				// to the cloud's middle, so the middle one lies over its neighbours' rims and the whole falls off outward)
				const double OutK = Clamp01(HeartU * HeartK / (0.12 * Wider));
				H.Col = H.Col.Mix(HeartEdge, HeartFall * OutK);
				H.Col2 = H.Col2.Mix(HeartEdge, HeartFall * OutK);
				H.Nearer = 0.5 * SpanM * (2.0 - OutK);
				H.R0 *= 1.3 - 0.5 * OutK;
				H.R1 = H.R0 * 1.5;
				H.Edge = HeartEdge;
				H.EdgeK = 1.0;
				H.EdgeIn = HeartEdgeIn;
				H.Inner = 0.4;
				H.Rough = HeartRough;
			}
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
			if (bRuled)
			{
				const double REnd = Rc * SafeNum(LobeR.Pick(D), 0.02, 1.0, 0.28);
				const double Reached = std::max(0.0, Rc - 0.6 * REnd) * D.U(0.2, 1.0), Starts = Reached * D.U(0.1, 0.4);
				B.Life = SafeNum(FireS.Pick(D), 0.1, 5.0, 0.75) * D.U(0.3, 1.0); // (the short ones are out, and their smoke boiling, within 0.3 s; the longest burn the ruled time)
				B.R1 = REnd;
				B.R0 = REnd / D.U(1.6, 2.6);
				B.From = At + Toward * Starts;
				B.Vel = Vel * 0.6 + Toward * ((Reached - Starts) * B.Drag / (1.0 - std::exp(-B.Drag * B.Life)));
				B.Col = B.Col.Mix(HotCol, Clamp01(0.3 * (Whiter - 1.0)));
			}
			B.Col2 = bRuled ? EmberCol : DeepCol; // (cooling to deep orange-red by CoolBy, still near opaque, then gone into its smoke; a ruled clump, which burns longer, on to a dark ember by the time it goes, so what little of it fades over the bare sky is dark, never pink)
			B.Cool = bRuled ? SafeNum(Num(X + "cool", 1.4), 0.2, 20.0, 1.4) : B.Life / CoolBy; // (the ruled cloud burns longer: orange through most of its life, deep red at its end)
			B.Alpha = 1.0;
			B.FadeIn = 0.005;
			B.Hold = bRuled ? 0.94 : 0.6; // (a ruled clump stays whole until its smoke has it, then is gone in a frame or two: a see-through fade over the bare sky is pink)
			B.Heart = 1.0;
			B.Rim = LobeRim;
			B.Rough = LobeRough;
			B.Edge = bRuled ? ClumpEdge : DeepCol; // (a hot middle darkening out to a deep red rim: a lump of fire)
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
				const double HeldTo = 0.6; // (the smoke is born as it always was, whatever the clump's own fade)
				const double Late = B.Life * std::max(0.0, HeldTo - 0.15);
				Bit M;
				M.Born = B.Born + Late;
				M.From = Where(B, Late);
				M.Vel = Going(B, Late);
				M.Drag = B.Drag;
				M.Windage = 1.0;
				M.Life = SmokeS.Pick(D);
				M.R0 = 1.15 * (B.R0 + (B.R1 - B.R0) * std::pow(HeldTo, B.Grow));
				M.R1 = M.R0 * SmokeGrow.Pick(D);
				if (bRuled)
				{
					M.Life = Hang * D.U(0.7, 1.2); // (it thins through most of that: the cloud is there to see about Hang seconds)
					M.Vel = M.Vel + Vec(D.Bell(), D.Bell(), 0.6 * D.Bell()) * (0.12 * Rc * M.Drag); // (its lumps drift apart a little, so the cloud is ragged and never one ball)
					M.Windage = D.U(0.8, 1.2); // (and the wind draws it out, as it does a flak burst)
					M.R1 = B.R1 * SafeNum(SmokeOut.Pick(D), 1.0, 12.0, 1.9);
				}
				M.Grow = 0.5;
				M.Col = SmokeCol;
				M.Col2 = SmokeCol.Mix(Rgb::FromPicture(120, 116, 112), 0.6);
				M.Alpha = Num(P + "smoke_alpha", 0.9) * D.U(0.8, 1.05);
				M.FadeIn = bRuled ? std::min(0.04, std::max(0.03, 0.2 * B.Life) / std::max(M.Life, 0.5)) : 0.04; // (thick as its clump starts to fade, however long it hangs: "black smoke boils out of it within 0.3 s", and no clump fades out pink over the bare sky)
				M.Hold = bRuled ? 0.55 : 0.3; // (the ruled cloud hangs thick through half its time, then thins)
				M.Heart = 0.7;
				M.Rim = 0.0;
				M.Rough = bRuled ? SmokeRough : 0.5;
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

	// (Bit::Lumps: how whole a puff's own lumps are, this far through its life. Whole for the first three quarters of
	// the time it is thick, Hold of its life, and melted into it by the end of that: young smoke low in a column
	// billows, old smoke aloft is one soft mass, and a puff that has begun to thin takes no places for lumps)
	// (game.lumps_px_most, .0 BigA and .1 BigB: what a puff Px pixels wide counts as when the lumps' places are dealt.
	// Its own width up to BigA; wider, less and less, down to nothing at BigB: a puff that fills the screen is the last
	// to be given lumps, and one as wide as BigB is given none. BigB of 0: its own width, whatever it is)
	static double LumpsWorth(double Px, double BigA, double BigB)
	{
		return BigB > 0.0 && Px > BigA ? BigA * std::max(0.0, BigB - Px) / std::max(BigB - BigA, 1e-6) : Px;
	}

	static double LumpsLeft(double Hold, double F)
	{
		const double Held = Hold == Hold ? std::clamp(Hold, 0.0, 1.0) : 0.5;
		return F == F ? std::clamp((Held - F) / std::max(0.25 * Held, 1e-6), 0.0, 1.0) : 0.0;
	}

	// (a place that is not a number, or endlessly far off, is no place: a fire asked for there is not lit, so no point of
	// it is ever laid where the screen cannot hold it)
	static bool IsAPlace(const Vec& P)
	{
		return std::fabs(P.X) <= 1e9 && std::fabs(P.Y) <= 1e9 && std::fabs(P.Z) <= 1e9;
	}

	// (a caller's direction made a unit one. Not a number, or without end, in any part: the usual way instead. Checked
	// for a number alone, a way of (inf, 0, 0) came out as not a number in every part and so did every point laid
	// along it (the wild-number run on the outcome looks))
	static Vec WayOf(const Vec& In, const Vec& Usual)
	{
		return (IsAPlace(In) ? In : Usual).Unit(Usual);
	}

	void Battle::Vent(double Now, const Vec& At, double Across, double Seconds, int Id)
	{
		Burning(Now, At, Across, Seconds, Id, false, true);
	}

	void Battle::Burn(double Now, const Vec& At, double Across, double Seconds, int Id, bool bOnShip)
	{
		Burning(Now, At, Across, Seconds, Id, bOnShip, false);
	}

	void Battle::Burning(double Now, const Vec& At, double Across, double Seconds, int Id, bool bOnShip, bool bVent)
	{
		if (!IsAPlace(At))
		{
			return;
		}
		if (Id >= 0)
		{
			// (one fire to a number: an earlier fire under it burns on as it was, and answers to no number)
			for (Source& Was : Sources)
			{
				if (Was.Kind == 0 && Was.Id == Id)
				{
					Was.Id = -1;
				}
				// (and a fire that joined another under it, fire.join, is no longer found by it)
				for (size_t K = 0; Was.Kind == 0 && K < Was.AlsoIds.size(); ++K)
				{
					if (Was.AlsoIds[K] == Id)
					{
						Was.AlsoIds.erase(Was.AlsoIds.begin() + static_cast<std::ptrdiff_t>(K));
						Was.AlsoOffsets.erase(Was.AlsoOffsets.begin() + static_cast<std::ptrdiff_t>(K));
						break;
					}
				}
			}
		}
		if (!bOnShip && !bVent && JoinBurn(Now, At, Across, Seconds, Id))
		{
			return;
		}
		Source S;
		S.At = At;
		S.Kind = 0;
		S.bOnShip = bOnShip;
		S.bVent = bVent;
		S.Id = Id >= 0 ? Id : -1;
		S.Start = Now;
		S.Next = Now;
		S.NextSmoke = Now;
		S.Flames = Now + std::max(0.0, Seconds);
		S.End = S.Flames + Num("fire.column_s", 600.0);
		S.Across = std::min(5000.0, std::max(0.3, Across)); // (a width that is not a number counts as the least, and an endless one is held: no point of it is ever laid at infinity)
		S.Seed = ++Seeds * 49979687ull ^ PlaceKey(At);
		S.Bank = BankRowOf(S.Across, S.Seed);
		S.Column = bVent ? -1 : OwnColumn(Now, S);
		if (S.Column < 0 && !bOnShip && !bVent)
		{
			S.Column = OwnColumn(Now, S, true); // (a burning vehicle on land: its own black column, fire.land_column)
		}
		S.NextGlow = Now;
		Sources.push_back(S);
	}

	bool Battle::JoinBurn(double Now, const Vec& At, double Across, double Seconds, int Id)
	{
		// fire.join, the frame study of 22 November: the ground scene lit a knocked-out tank as six to eight fires of
		// its own along the whole hull (each of the tank's fire and smoke beats a Burn), and the art director's ruling
		// is one body of flame as wide as the engine deck. So a fire lit within within_m of one already burning, no more
		// than up_m above or below it, and neither of them over most_m across, joins it: that fire burns on as one,
		// as long as the longest of them, growing over grow_s toward wide_m across at the most (add of each newcomer's
		// width more for each that joins, never narrower than it was), one lamp, one glow and one column; and the
		// newcomer's number finds it, for BurnMove and BurnOut. No such key: every fire burns alone, as before
		const std::string J = "fire.join.";
		if (!Has(J + "within_m") || !(Across == Across))
		{
			return false;
		}
		const double Within = SafeNum(Num(J + "within_m", 4.0), 0.0, 100.0, 4.0);
		const double Most = SafeNum(Num(J + "most_m", 6.0), 0.3, 1000.0, 6.0);
		const double UpTo = SafeNum(Num(J + "up_m", 3.0), 0.0, 100.0, 3.0);
		const double NewAcross = std::min(5000.0, std::max(0.3, Across));
		if (NewAcross > Most || Within <= 0.0)
		{
			return false;
		}
		Source* Host = nullptr;
		size_t HostAt = 0;
		double Best = Within * Within;
		for (Source& H : Sources)
		{
			if (H.Kind != 0 || H.bOnShip || H.bVent || H.OutAt >= 0.0 || H.Flames <= Now || Now < H.Start || std::max(H.Across, H.AcrossTo) > Most)
			{
				continue;
			}
			const double Dx = H.At.X - At.X, Dy = H.At.Y - At.Y, Dz = H.At.Z - At.Z;
			if (std::fabs(Dz) > UpTo)
			{
				continue;
			}
			const double D2 = Dx * Dx + Dy * Dy;
			if (D2 <= Best)
			{
				Best = D2;
				Host = &H;
				HostAt = static_cast<size_t>(&H - Sources.data());
			}
		}
		if (!Host)
		{
			return false;
		}
		Source& H = *Host;
		const double Grow = SafeNum(Num(J + "grow_s", 1.5), 0.05, 600.0, 1.5);
		const double Wide = SafeNum(Num(J + "wide_m", 3.0), 0.3, Most, 3.0);
		const double Add = SafeNum(Num(J + "add", 0.1), 0.0, 1.0, 0.1);
		const double Was = H.AcrossBy > 0.0 ? H.AcrossTo : H.Across;
		const double To = std::max(Was, std::min(Wide, std::max(Was, NewAcross) + Add * NewAcross));
		if (To > H.Across + 1e-9)
		{
			H.AcrossFrom = H.Across;
			H.AcrossTo = To;
			H.AcrossAt = Now;
			H.AcrossBy = Grow;
		}
		H.Flames = std::max(H.Flames, Now + SafeNum(Seconds, 0.0, 1.0e7, 0.0));
		H.End = std::max(H.End, H.Flames + SafeNum(Num("fire.column_s", 600.0), 0.0, 1.0e7, 600.0));
		if (Id >= 0 && H.AlsoIds.size() < 64)
		{
			H.AlsoIds.push_back(Id);
			H.AlsoOffsets.push_back(H.At - At);
		}
		if (H.Column < 0)
		{
			// (it burns long enough now for a column of its own. OwnColumn adds to Sources, which may move them: it is
			// handed a copy, and the host is found again by its place in the list)
			const Source Was_ = H;
			const int Column = OwnColumn(Now, Was_, true);
			Sources[HostAt].Column = Column;
		}
		else
		{
			for (Source& C : Sources)
			{
				if (C.Kind == 3 && C.Id == H.Column)
				{
					C.Flames = std::max(C.Flames, H.Flames);
					C.End = std::max(C.End, H.End);
					break;
				}
			}
		}
		return true;
	}

	bool Battle::MoveSource(Source& S, double Now, const Vec& At, const Vec& Velocity)
	{
		if (!RealPlace(At))
		{
			return false;
		}
		// (what lies on the ship goes with the seat; the pieces in the air are left to it)
		if (S.Ride != 0)
		{
			const Vec Delta = At - S.At;
			if (Delta.X != 0.0 || Delta.Y != 0.0 || Delta.Z != 0.0)
			{
				for (Bit& B : Bits)
				{
					if (B.Ride == S.Ride)
					{
						B.From = B.From + Delta;
					}
				}
			}
		}
		else
		{
			RideNo = RideNo == 0xFFFFFFFFu ? 1u : RideNo + 1u;
			S.Ride = RideNo;
		}
		S.At = At;
		// (a velocity that is no number, or faster than anything afloat or on wheels, counts as none)
		S.Vel = RealPlace(Velocity) && Velocity.Length() <= 150.0 ? Velocity : Vec();
		S.MovedAt = Now;
		S.bMoves = true;
		return true;
	}

	void Battle::Rides(const Source& S, size_t FromBit)
	{
		// The pieces a moving fire, vent or cook-off has just laid (BurnMove, SteamMove, CookOffMove; the sea battle,
		// 21 November: a fire stayed on the water where the ship was struck while she steamed on at 25 knots).
		// A piece with no motion of its own lies on the ship, an ember or a glow on her deck: it is moved with the
		// seat from now on. The others are born where the seat is at their own moment. Of those, smoke and steam are
		// left to the air at once, so they stream astern; a thing that burns keeps the share of the ship's way that
		// the air has not got of it (a foot or a root nearly all, a tongue's tip half, a torn tip none), so the fire
		// stays on her and its flames lean aft; and a thrown piece flies with all of her way on it until it lands.
		for (size_t I = FromBit; I < Bits.size(); ++I)
		{
			Bit& B = Bits[I];
			if (B.bCarried || B.Ride != 0)
			{
				continue;
			}
			if (!B.bThrown && B.Windage == 0.0 && B.Rise == 0.0 && B.Gravity == 0.0 && B.Vel.X == 0.0 && B.Vel.Y == 0.0 && B.Vel.Z == 0.0)
			{
				B.Ride = S.Ride;
				continue;
			}
			const Vec Lead = S.Vel * std::clamp(B.Born - S.MovedAt, -1.0, 3.0);
			B.From = B.From + Lead;
			if (B.bFired)
			{
				B.Origin = B.Origin + Lead;
			}
			if (B.bThrown)
			{
				B.At1 = B.At1 + Lead;
				B.At2 = B.At2 + Lead;
				B.Carry = S.Vel;
				B.CarryFor = std::min(B.Land, 30.0);
				B.bCarried = true;
			}
			else if (B.bLight)
			{
				B.Carry = S.Vel * (1.0 - Clamp01(B.Windage));
				B.CarryFor = 2.0;
				B.bCarried = true;
			}
			// (a vent's rag of steam or a hatch's jet of flame is drawn out along its flight: on a moving ship the eye
			// sees that flight from her, running aft, so the jet bends astern as one stream and never stands as a row
			// of upright slivers left along her track. A fire's own tongues lean by the wind over her deck already)
			if (!B.bThrown && B.Stretch > 1.05 && (S.Kind == 4 || S.Kind == 5))
			{
				const Vec Seen = GoingOwn(B, 0.0) + (B.bCarried ? B.Carry : Vec()) - S.Vel;
				if (Seen.Length() > 0.5 && RealPlace(Seen))
				{
					B.Along = Seen.Unit(B.Along);
				}
			}
		}
	}

	void Battle::BurnMove(double Now, int Id, const Vec& At, const Vec& Velocity)
	{
		if (Id < 0 || !(std::fabs(Now) <= 1.0e9))
		{
			return;
		}
		for (Source& S : Sources)
		{
			if (S.Kind != 0)
			{
				continue;
			}
			Vec Target = At;
			if (S.Id != Id)
			{
				// (a fire that joined this one, fire.join: it is moved as that fire's seat was, by the offset it had)
				const auto Found = std::find(S.AlsoIds.begin(), S.AlsoIds.end(), Id);
				if (Found == S.AlsoIds.end())
				{
					continue;
				}
				Target = At + S.AlsoOffsets[static_cast<size_t>(Found - S.AlsoIds.begin())];
			}
			if (!S.AlsoIds.empty())
			{
				// (a fire of several numbers is moved by the first of them that moves it, and the others are let be: each
				// would set it a little elsewhere, as their ship turns, and it would shake)
				if (!S.bMoverSet)
				{
					S.bMoverSet = true;
					S.Mover = Id;
				}
				else if (S.Mover != Id)
				{
					return;
				}
			}
			const Vec Was = S.At;
			if (!MoveSource(S, Now, Target, Velocity))
			{
				return;
			}
			if (S.Column >= 0)
			{
				for (Source& C : Sources)
				{
					if (C.Kind != 3 || C.Id != S.Column)
					{
						continue;
					}
					// (one column to a ship, and each of her fires is moved in a frame: the first of them moves it,
					// and every place it rises from)
					if (!C.bMoves || C.MovedAt != Now)
					{
						const Vec Delta = S.At - Was;
						for (Vec& Head : C.Heads)
						{
							Head = Head + Delta;
						}
						MoveSource(C, Now, C.At + Delta, S.Vel);
					}
					break;
				}
			}
			return;
		}
	}

	void Battle::BurnOut(double Now, int Id, double OverIn, const char* How)
	{
		// (fire.put_out, art director 14:39Z 20 November: "the smoke turns white, then the fire is gone". The flames
		// are eaten back from the edges toward the seat over some seconds, lower and sputtering, never thinned. On a
		// ship, as the water hits, a short surge of white steam at the seat; on a tank the crew's extinguisher gives a
		// brief pale puff. The smoke goes from black through grey to grey-white over 10 to 30 s, a thin wisp hangs on
		// to about a minute, and red embers glow for 10 to 20 s after the last flame. Its own dice, by the fire's seed)
		const std::string P = "fire.put_out.";
		if (Id < 0 || !Has(P + "gone_s"))
		{
			return;
		}
		auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		for (Source& S : Sources)
		{
			if (S.Kind != 0 || (S.Id != Id && std::find(S.AlsoIds.begin(), S.AlsoIds.end(), Id) == S.AlsoIds.end()) || S.OutAt >= 0.0 || S.Flames <= Now)
			{
				continue;
			}
			Dice D(S.Seed * 0x9E3779B97F4A7C15ull ^ 0x0D7ull);
			const size_t LaidFrom = Bits.size(); // (on a moving ship, BurnMove: its puff is born at the seat as it moves, its embers lie on her)
			const bool bWater = !(How && std::string(How) == "extinguisher");
			const double Over = OverIn > 0.0 ? Safe(OverIn, 0.5, 300.0, 8.0) : Safe(Range(P + "over_s", 6.0, 12.0).Pick(D), 0.5, 300.0, 8.0);
			S.OutAt = Now;
			S.OutOver = Over;
			S.Flames = std::min(S.Flames, Now + Over);
			S.OutPale = Safe(Range(P + "pale_s", 10.0, 30.0).Pick(D), 1.0, 600.0, 20.0);
			S.End = std::max(S.Flames + 1.0, Now + std::max(S.OutPale + 1.0, Safe(Num(P + "gone_s", 60.0), 2.0, 3600.0, 60.0)));
			S.OutSurge = bWater ? Safe(Range(P + "surge_s", 2.0, 5.0).Pick(D), 0.0, 60.0, 3.0) : 0.0;
			S.NextSteam = Now;
			S.Vent = SteamRowOf("surge");
			const double Across = std::min(S.Across, 2000.0);
			if (!bWater)
			{
				// (the extinguisher: a brief pale puff over the seat, gone in two or three seconds)
				const Rgb Pale = Picture(P + "puff", Rgb::FromPicture(214, 214, 210));
				const int N = D.Whole(3, 5);
				for (int I = 0; I < N; ++I)
				{
					Bit B;
					B.Born = Now + D.U(0.1, 0.9);
					B.From = S.At + Vec(D.U(-0.3, 0.3), D.U(-0.3, 0.3), D.U(0.15, 0.4)) * Across;
					B.Vel = Vec(D.Bell(), D.Bell(), D.U(0.5, 1.5)) * (0.6 * std::sqrt(Across));
					B.Drag = 1.5;
					B.Rise = 0.5;
					B.Windage = 0.8;
					B.Life = D.U(1.6, 2.8);
					B.R0 = 0.25 * Across;
					B.R1 = D.U(0.6, 1.0) * Across;
					B.Col = Pale;
					B.Col2 = Pale;
					B.Alpha = D.U(0.5, 0.7);
					B.FadeIn = 0.08;
					B.Hold = 0.25;
					B.Heart = 0.6;
					B.HeartEnd = 0.2;
					B.Rough = 0.3;
					B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
					Bits.push_back(B);
				}
			}
			// (the embers: where the flames stood, showing as the flames are driven off them, the outermost first, and
			// glowing on for some seconds after the last flame. The art director on the mend round, 16:41Z: as soft
			// round red dots of 10 to 20 pixels at 90 m they "read as red lamps. Make them a dull red-orange glow patch
			// on the charred seat with 5 to 15 irregular specks of 1 to 3 px that flicker and fade." So: a dull patch
			// lying on the seat, two ragged pieces of it, coming up as the flames leave and dying with the last ember;
			// and over it small specks, each alight for a second or so and then another somewhere else, so many at
			// once at first and fewer and fewer to the end)
			const double EmbersFor = Safe(Range(P + "embers_s", 10.0, 20.0).Pick(D), 0.0, 600.0, 15.0);
			const double Alight = Safe(Range(P + "embers", 5.0, 15.0).Pick(D), 0.0, 40.0, 10.0);
			const Rgb EmberCol = Picture(P + "ember", Rgb::FromPicture(255, 150, 62)), EmberOld = Picture(P + "ember_old", Rgb::FromPicture(176, 52, 20));
			const Rgb GlowCol = Picture(P + "glow", Rgb::FromPicture(168, 62, 24));
			const double GlowAlpha = Safe(Num(P + "glow_alpha", 0.4), 0.0, 1.0, 0.4);
			const double EmbersFrom = Now + 0.3 * Over, EmbersTo = S.Flames + EmbersFor;
			if (EmbersTo > EmbersFrom + 0.5 && GlowAlpha > 0.0)
			{
				for (int I = 0; I < 2; ++I)
				{
					// (turned to the eye and low for its width, so it is a patch from the masthead and from the sea alike:
					// lying flat it was a thin line from a low eye)
					Bit G;
					G.Born = Now + Over * (0.35 + 0.25 * I);
					G.Life = std::max(1.0, EmbersTo - G.Born);
					G.R0 = G.R1 = Across * D.U(0.2, 0.3);
					G.Flatten = D.U(0.3, 0.42);
					G.From = S.At + Vec(D.U(-0.12, 0.12), D.U(-0.12, 0.12), 0.0) * Across + ZUp * (0.6 * G.Flatten * G.R0);
					G.Col = GlowCol * D.U(0.85, 1.1);
					G.Col2 = GlowCol * 0.45;
					G.Cool = 1.0;
					G.Alpha = GlowAlpha;
					G.FadeIn = std::min(3.0, 0.3 * Over);
					G.Hold = 0.45;
					G.Heart = 0.3;
					G.Rim = 0.0;
					G.Inner = 0.15;
					G.Rough = 0.45;
					G.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
					G.bLight = true;
					Bits.push_back(G);
				}
			}
			// (each speck lives about 1.1 s, so this many are born for that many to be alight at once, thinning out)
			const int NEmbers = EmbersTo > EmbersFrom + 0.5 ? static_cast<int>(std::min(400.0, std::round(Alight * (EmbersTo - EmbersFrom) / 1.1 * 0.9))) : 0;
			for (int I = 0; I < NEmbers; ++I)
			{
				const double When = EmbersFrom + (EmbersTo - 0.4 - EmbersFrom) * std::pow(D.U(), 1.6); // (most of them early)
				// (where the flames have already left: the outside of the seat first)
				const double Left = Clamp01(1.0 - (When - Now) / std::max(Over, 1e-3));
				const double Out_ = Left + (1.0 - Left) * std::sqrt(D.U());
				const double A = D.U(0.0, 2.0 * Pi);
				Bit B;
				B.Born = When;
				B.Life = D.U(0.5, 1.7);
				B.From = S.At + Vec(std::cos(A) * Out_ * 0.45, std::sin(A) * Out_ * 0.45, 0.07) * Across;
				B.R0 = B.R1 = std::max(0.015, Across * D.U(0.005, 0.013));
				B.Stretch = D.U(1.0, 2.4); // (irregular: none of them a round dot)
				B.Along = Vec(D.Bell(), D.Bell(), 0.3 * D.Bell()).Unit(Vec(1.0, 0.0, 0.0));
				B.Col = EmberCol * D.U(0.8, 1.15);
				B.Col2 = EmberOld;
				B.Cool = 1.0;
				B.Alpha = D.U(0.6, 0.95);
				B.FadeIn = 0.25 * B.Life;
				B.Hold = 0.35;
				B.Heart = 0.9;
				B.Rim = 0.0;
				B.Rough = 0.5;
				B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				B.LeastPx = 1.2;
				B.LeastKeep = 0.6;
				B.bLight = true;
				B.bShown = true; // (never laid late and whole by Unseen: a speck not yet lit stays dark)
				Bits.push_back(B);
			}
			if (S.bMoves)
			{
				Rides(S, LaidFrom);
			}
			if (S.Column >= 0)
			{
				// (her own column burns on only while another of its fires does: with the last of them put out it greys
				// and thins as this fire's smoke does)
				double Flames = S.Flames, End = S.End;
				for (const Source& F : Sources)
				{
					if (F.Kind == 0 && F.Column == S.Column && &F != &S)
					{
						Flames = std::max(Flames, F.Flames);
						End = std::max(End, F.End);
					}
				}
				for (Source& C : Sources)
				{
					if (C.Kind == 3 && C.Id == S.Column)
					{
						C.Flames = std::min(C.Flames, Flames);
						C.End = std::min(C.End, std::max(End, C.Flames + 1.0));
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Steam, ammunition cooking off, a shot glancing off armour (with BurnOut above: the looks of a hit's outcome)

	Battle::SteamRow Battle::SteamRowOf(const std::string& Kind) const
	{
		auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		const std::string P = "steam." + (Has("steam." + Kind + ".life_s.0") ? Kind : std::string("vent")) + ".";
		SteamRow R;
		R.Cone = Safe(Num(P + "cone_deg", 12.0), 1.0, 120.0, 12.0);
		R.Gap = Safe(Num(P + "gap_m", 1.0), 0.0, 10.0, 1.0);
		R.Every = Safe(Num(P + "every_s", 0.06), 0.02, 2.0, 0.06);
		R.Roar = Safe(Num(P + "roar", 0.3), 0.0, 1.0, 0.3);
		R.Least = Safe(Num(P + "least", 0.12), 0.02, 1.0, 0.12);
		R.Mouth = Safe(Num(P + "mouth", 0.5), 0.05, 2.0, 0.5);
		R.Rag = Safe(Num(P + "rag", 0.2), 0.02, 1.0, 0.2);
		R.Rise = Safe(Num(P + "rise_mps", 1.5), 0.0, 20.0, 1.5);
		R.Windage = Safe(Num(P + "windage", 0.5), 0.0, 2.0, 0.5);
		R.Alpha = Safe(Num(P + "alpha", 0.9), 0.0, 1.0, 0.9);
		R.Waver = Safe(Num(P + "waver", 0.25), 0.0, 1.0, 0.25);
		R.Rough = Safe(Num(P + "rough", 0.35), 0.0, 0.6, 0.35);
		R.Lance = Safe(Num(P + "lance", 0.0), 0.0, 1.0, 0.0);
		R.LanceLife = Safe(Num(P + "lance_life_s", 0.3), 0.05, 5.0, 0.3);
		R.LanceLong = Safe(Num(P + "lance_long", 3.5), 1.0, 12.0, 3.5);
		const Span Life = Range(P + "life_s", 1.6, 2.8);
		R.Life.A = Safe(Life.A, 0.2, 60.0, 1.6);
		R.Life.B = std::max(R.Life.A, Safe(Life.B, 0.2, 60.0, 2.8));
		R.Reach = Safe(Num(P + "reach", 1.2), 0.05, 20.0, 1.2); // (a surge's reach, as a share of its fire's width; a vent's own is thrown when it opens)
		// (a row's own colour where it has one: release 2's smoulder, a burnt-out wreck's grey wisps, is not white)
		R.Col = Picture(Has(P + "colour.0") ? P + "colour" : std::string("steam.colour"), Rgb::FromPicture(238, 240, 244));
		R.Old = Picture(Has(P + "colour_old.0") ? P + "colour_old" : std::string("steam.colour_old"), Rgb::FromPicture(226, 230, 236));
		R.Shadow = Picture("steam.shadow", Rgb::FromPicture(186, 194, 208));
		R.ShadowK = Safe(Num("steam.shadow_k", 0.0), 0.0, 1.0, 0.0);
		R.SideShade = Safe(Num("steam.side_shade", 0.0), 0.0, 1.0, 0.0);
		R.Cap = Safe(Num(P + "cap", 0.0), 0.0, 1.0, 0.0);
		R.CapWide = Safe(Num(P + "cap_wide", 2.0), 1.0, 6.0, 2.0);
		R.CapFrom = Safe(Num(P + "cap_from", 0.62), 0.1, 0.95, 0.62);
		const Span CapLife = Range(P + "cap_life_s", 3.0, 6.0);
		R.CapLife.A = Safe(CapLife.A, 0.2, 60.0, 3.0);
		R.CapLife.B = std::max(R.CapLife.A, Safe(CapLife.B, 0.2, 60.0, 6.0));
		return R;
	}

	void Battle::SteamRag(double T, const Vec& At, const Vec& WayIn, double Reach, double Across, double Push, const SteamRow& R, Dice& D)
	{
		// One rag of steam. Thrown along its way inside the row's narrow cone, it covers most of its reach in the first
		// third of its life, widening as it goes, then hangs, lifts a little and leans with the wind while it loses its
		// heart and evaporates: white that rises fast and vanishes, never a bank. Clear at the mouth, where dry steam is
		// not yet seen. Few live long, so its top is torn into rags
		const Vec Way = WayIn.Unit(ZUp);
		const Vec SideA = Way.Cross(std::fabs(Way.Z) < 0.9 ? ZUp : Vec(1.0, 0.0, 0.0)).Unit(Vec(1.0, 0.0, 0.0));
		const Vec SideB = Way.Cross(SideA).Unit(Vec(0.0, 1.0, 0.0));
		const double Off = Radians(0.5 * R.Cone) * std::sqrt(D.U());
		const double Round_ = D.U(0.0, 2.0 * Pi);
		const Vec Out_ = SideA * std::cos(Round_) + SideB * std::sin(Round_);
		const Vec Dir = (Way * std::cos(Off) + Out_ * std::sin(Off)).Unit(Way);
		const double Long = D.U();
		const double Life = std::max(0.2, (R.Life.A + (R.Life.B - R.Life.A) * Long * Long) * (0.6 + 0.4 * Push));
		const double Far = std::max(0.05, Reach * Push * D.U(0.7, 1.0));
		const double Kd = 3.0 / Life;
		Bit B;
		B.Born = T;
		B.Life = Life;
		B.From = At + Out_ * (0.3 * Across * std::sqrt(D.U()));
		B.Vel = Dir * (Far * Kd);
		B.Drag = Kd;
		B.Rise = R.Rise * D.U(0.6, 1.2);
		B.Windage = std::min(1.2, R.Windage * (1.0 + 0.8 * (1.0 - Push)));
		B.R0 = std::max(0.02, 0.5 * R.Mouth * Across);
		B.R1 = std::max(B.R0, Far * R.Rag * D.U(0.7, 1.2) * (0.75 + 0.75 * Long)); // (the few that live long are its biggest rags)
		// (and never smaller than the way to the next rag, so a weak leak is one wavering thread, not a string of beads)
		const double Blow = Wind.Length();
		B.R1 = std::max(B.R1, 0.8 * (Far * Kd + R.Rise + B.Windage * (Blow == Blow ? std::min(Blow, 30.0) : 0.0)) * R.Every * (2.0 - Push));
		B.Grow = 0.45;
		B.Col = R.Col;
		B.Col2 = R.Old;
		B.Alpha = R.Alpha * D.U(0.85, 1.0);
		B.FadeIn = std::clamp(R.Gap / std::max(Far * Kd, 0.1), 0.01, 0.3);
		B.Hold = 0.3;
		B.Heart = 0.65;
		B.HeartEnd = 0.1;
		B.Inner = 0.5;
		B.Rough = R.Rough;
		B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
		B.Turn = D.U(0.0, 2.0 * Pi);
		// (the art director on the first cut, 15:40Z: "give the rags a visible cool shadow side, about 0.75 to 0.8 of
		// the lit side")
		B.ShadowCol = R.Shadow;
		B.ShadowK = R.ShadowK;
		if (R.SideShade > 0.0 && bEyeKnown)
		{
			// (and the plume has a shadow side of its own: each rag shaded alone, the shadow of one fell under the
			// light of the next and the plume read white all over. A rag thrown to the side away from the sun is in
			// the others' shade, the more the further off the middle it is thrown, and is born that much greyer)
			const Vec Across_ = LastSun - Way * LastSun.Dot(Way);
			const double Len = Across_.Length();
			if (Len > 0.05 && Len == Len)
			{
				const double Away = Clamp01(-Out_.Dot(Across_) / Len * Off / std::max(Radians(0.5 * R.Cone), 1e-6) * 0.75 + 0.25);
				B.Col = B.Col.Mix(R.Shadow, R.SideShade * Away);
				B.Col2 = B.Col2.Mix(R.Shadow, R.SideShade * Away);
			}
		}
		Bits.push_back(B);
		if (R.Cap > 0.0)
		{
			// (the same, 15:40Z: "let the funnel plume's top third broaden and roll over downwind, so it isn't a white
			// rope". Where the rags' climb is spent the steam is still there, slower and wider: now and then a bigger
			// rag is born in the plume's top, where a rag of this step will be when it has climbed that far, going on up
			// a little and off down the wind as it thins, so the head spreads and leans over. Its own dice: the rags
			// are thrown as they were)
			Dice Dc(static_cast<uint64_t>(B.LumpSeed) * 0x9E3779B97F4A7C15ull ^ 0xCA9ull);
			if (Dc.U() < R.Cap)
			{
				const double H = Dc.U(R.CapFrom, 0.97);
				const double Climbed = -std::log(1.0 - H) / Kd; // (how old a rag is when it has come that share of its way)
				const Vec Blown = Wind.X == Wind.X && Wind.Y == Wind.Y && Wind.Z == Wind.Z ? Wind : Vec();
				Bit C = B;
				C.Born = T + Climbed;
				C.Life = std::max(0.4, R.CapLife.Pick(Dc) * (0.6 + 0.4 * Push));
				C.From = B.From + Dir * (Far * H) + ZUp * (B.Rise * Climbed) + Blown * (B.Windage * std::min(Climbed, 60.0));
				C.Vel = Dir * (Far * Kd * (1.0 - H));
				C.Drag = 1.5 / C.Life;
				C.Rise = 0.5 * B.Rise;
				C.Windage = std::min(1.3, 2.2 * B.Windage);
				C.R0 = B.R0 + (B.R1 - B.R0) * std::pow(Clamp01(Climbed / Life), B.Grow);
				C.R1 = std::max(C.R0, B.R1 * R.CapWide * Dc.U(0.8, 1.25));
				C.Grow = 0.7;
				C.Alpha = B.Alpha * 0.85;
				C.FadeIn = 0.35;
				C.Hold = 0.3;
				C.Heart = 0.6;
				C.Rough = std::min(0.6, R.Rough * 1.3);
				C.LumpSeed = static_cast<uint32_t>(Dc.U() * 4294967295.0);
				C.Turn = Dc.U(0.0, 2.0 * Pi);
				Bits.push_back(C);
			}
		}
		if (R.Lance > 0.0 && D.U() < R.Lance)
		{
			// (and the jet itself at the mouth: a short dense length drawn out along its way and flying with the rag,
			// so that where the steam flies fastest no gap shows between rags and its root is one white lance)
			Bit J = B;
			J.Life = std::min(Life, R.LanceLife);
			J.From = At + Out_ * (0.15 * Across * std::sqrt(D.U()));
			J.Along = Dir;
			J.Stretch = 1.0 + (R.LanceLong - 1.0) * Push; // (a weak jet's is short: no long pale leaf at a wisp's foot)
			J.R1 = B.R0 * (1.0 + 1.2 * Push);
			J.Grow = 1.0;
			J.Rise = 0.0;
			J.Windage = 0.3 * B.Windage;
			J.Hold = 0.45;
			J.Heart = 0.85;
			J.HeartEnd = -1.0;
			J.Inner = 0.5;
			J.Rough = 0.15;
			J.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			Bits.push_back(J);
		}
	}

	void Battle::Steam(double Now, int Id, const Vec& At, const Vec& WayIn, double Across, double Seconds, const char* Kind)
	{
		// (steam, art director 14:39Z 20 November: "white that rises fast and vanishes, beside black that stays". From
		// the escape pipes on the funnel's face a tall plume straight up, 30 to 60 m; from deck vents, skylights and
		// holes in her side low jets, 5 to 15 m. A narrow cone, clear for a metre or so at the mouth, then pure white
		// breaking into rags that evaporate; never a bank, never brown, never a flash. Over its time the length and
		// speed shrink together, ending in a wavering wisp. Its own dice, by its number and place)
		SteamStop(Now, Id);
		const std::string Asked = Kind && *Kind ? Kind : "vent";
		const std::string Row = Has("steam." + Asked + ".reach_m.0") ? Asked : std::string("vent");
		const std::string P = "steam." + Row + ".";
		if (!IsAPlace(At) || !Has(P + "reach_m.0") || !(Seconds > 0.0))
		{
			return;
		}
		auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		Source S;
		S.Kind = 4;
		S.Id = Id;
		S.At = At;
		S.Way = WayOf(WayIn, ZUp);
		S.Across = Safe(Across, 0.05, 50.0, 1.0);
		S.Start = S.Next = Now;
		S.End = S.Flames = Now + Safe(Seconds, 0.5, 600.0, 20.0);
		S.Seed = static_cast<uint64_t>(static_cast<uint32_t>(Id)) * 2654435761ull ^ PlaceKey(At) ^ 0x57EA3A11ull;
		S.Vent = SteamRowOf(Row);
		// (its reach: the row's, for a vent as wide as the row's own; a narrower one's is shorter, a wider one's a
		// little longer, by the row's power)
		Dice D(S.Seed);
		const double Reach = Safe(Range(P + "reach_m", 5.0, 15.0).Pick(D), 0.1, 400.0, 10.0);
		const double Wide = Safe(Num(P + "across_m", 2.0), 0.05, 50.0, 2.0);
		S.JetTall = Reach * std::clamp(std::pow(S.Across / Wide, Safe(Num(P + "by_width", 0.7), 0.0, 2.0, 0.7)), 0.1, 2.0);
		Sources.push_back(S);
	}

	void Battle::SteamStop(double Now, int Id)
	{
		for (Source& S : Sources)
		{
			if (S.Kind == 4 && S.Id == Id && S.End > Now)
			{
				S.Flames = S.End = Now;
			}
		}
	}

	void Battle::SteamMove(double Now, int Id, const Vec& At, const Vec& WayIn, const Vec& Velocity)
	{
		if (!(std::fabs(Now) <= 1.0e9))
		{
			return;
		}
		for (Source& S : Sources)
		{
			if (S.Kind == 4 && S.Id == Id && S.End > Now && MoveSource(S, Now, At, Velocity))
			{
				S.Way = WayOf(WayIn, S.Way); // (no direction: the way it blew)
			}
		}
	}

	Battle::CookRow Battle::CookRowOf() const
	{
		auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		auto Held = [&](const std::string& Path, double A, double B, double Least, double Most)
		{
			Span R = Range(Path, A, B);
			R.A = Safe(R.A, Least, Most, A);
			R.B = std::max(R.A, Safe(R.B, Least, Most, B));
			return R;
		};
		const std::string P = "cook_off.";
		CookRow R;
		R.JetWide = Safe(Num(P + "jet_wide", 0.22), 0.02, 2.0, 0.22);
		R.JetEvery = Safe(Num(P + "jet_every_s", 0.04), 0.02, 1.0, 0.04);
		R.JetSmokeEvery = Safe(Num(P + "jet_smoke_every_s", 0.12), 0.03, 5.0, 0.12);
		R.Dip = Safe(Num(P + "pulse_dip", 0.35), 0.0, 1.0, 0.35);
		R.Two = Safe(Num(P + "two_pulses", 0.5), 0.0, 1.0, 0.5);
		R.PuffAlpha = Safe(Num(P + "puff_alpha", 0.6), 0.0, 1.0, 0.6);
		R.FlashK = Safe(Num(P + "flash", 0.75), 0.0, 4.0, 0.75);
		R.Lamp = Safe(Num(P + "lamp", 0.3), 0.0, 2.0, 0.3);
		R.FragmentOdds = Safe(Num(P + "fragment_odds", 0.35), 0.0, 1.0, 0.35);
		R.Embers = Held(P + "embers", 6.0, 14.0, 0.0, 60.0);
		R.EmberUp = Held(P + "ember_up_m", 5.0, 20.0, 0.1, 80.0);
		R.EmberLife = Held(P + "ember_life_s", 0.8, 2.2, 0.05, 10.0);
		R.Puffs = Held(P + "puffs", 2.0, 3.0, 0.0, 12.0);
		R.Ember = Picture(P + "ember", Rgb::FromPicture(255, 150, 60));
		R.EmberOld = Picture(P + "ember_old", Rgb::FromPicture(170, 56, 20));
		R.Puff = Picture(P + "puff", Rgb::FromPicture(62, 59, 56));
		R.Smoke = Picture(P + "jet_smoke", Rgb::FromPicture(38, 35, 32));
		R.Fragment = Picture(P + "fragment", Rgb::FromPicture(44, 41, 38));
		R.JetPulse = Safe(Num(P + "jet_pulse", 0.0), 0.0, 0.6, 0.0);
		R.JetLean = Safe(Num(P + "jet_lean", 0.0), 0.0, 0.5, 0.0);
		R.JetLeanMost = Safe(Num(P + "jet_lean_most", 0.4), 0.0, 1.0, 0.4);
		R.JetSparks = Safe(Num(P + "jet_sparks", 0.0), 0.0, 4.0, 0.0);
		R.JetLamp = Safe(Num(P + "jet_lamp", 0.0), 0.0, 4.0, 0.0);
		R.JetLampDay = Safe(Num(P + "jet_lamp_day", 0.6), 0.0, 1.0, 0.6);
		R.JetTorn = Safe(Num(P + "jet_torn", 0.0), 0.0, 2.0, 0.0);
		R.EmberUneven = Safe(Num(P + "ember_uneven", 1.0), 1.0, 6.0, 1.0);
		R.Root = Picture(P + "jet_root", Rgb::FromPicture(255, 244, 206));
		return R;
	}

	void Battle::CookOffMove(double Now, int Id, const Vec& At, const Vec& UpIn, const Vec& Velocity)
	{
		if (Id < 0 || !(std::fabs(Now) <= 1.0e9))
		{
			return;
		}
		for (Source& S : Sources)
		{
			if (S.Kind == 5 && S.Id == Id && MoveSource(S, Now, At, Velocity))
			{
				S.Way = WayOf(UpIn, S.Way);
			}
		}
	}

	void Battle::CookOff(double Now, const Vec& At, const Vec& UpIn, double Across, int PopsIn, double OverIn, double JetIn, double JetFrom, int Id)
	{
		// (cook_off, art director 14:39Z 20 November: "repeated small pops, and flame roaring from the hatches". Each
		// pop: a small flash by the flash rule, a grey-black puff out of the hatch, orange embers and a fragment in
		// short arcs, 5 to 20 m up; never a glowing round streaking skyward, which reads as fireworks. The jet: 3 to
		// 8 s in one or two pulses, 3 to 6 m tall from a tank's hatch, ragged and tapering, flickering, a yellow-orange
		// heart, black smoke rolling off its top; first, between the pops or last, as the caller's outcome has it.
		// Its own dice, by its place and moment)
		const std::string P = "cook_off.";
		if (!IsAPlace(At) || !Has(P + "jet_tall_m.0"))
		{
			return;
		}
		auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		if (Id >= 0)
		{
			// (one cook-off to a number: an earlier one under it goes on as it was, and answers to no number)
			for (Source& Was : Sources)
			{
				if (Was.Kind == 5 && Was.Id == Id)
				{
					Was.Id = -1;
				}
			}
		}
		Source S;
		S.Kind = 5;
		S.Id = Id >= 0 ? Id : -1;
		S.At = At;
		S.Way = WayOf(UpIn, ZUp);
		S.Across = Safe(Across, 0.1, 50.0, 0.6);
		S.Start = Now;
		S.Seed = PlaceKey(At) * 0xC2B2AE3D27D4EB4Full ^ static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) ^ 0xC00C0FFull;
		S.Cook = CookRowOf();
		Dice D(S.Seed);
		const int NPops = std::clamp(PopsIn, 0, 40);
		const double Over = Safe(OverIn, 0.0, 600.0, 0.0);
		for (int I = 0; I < NPops; ++I)
		{
			// (ragged, and quickening: more of them late than early)
			S.Pops.push_back(Now + Over * std::pow(D.U(), 0.625));
		}
		std::sort(S.Pops.begin(), S.Pops.end());
		const double LastPop = S.Pops.empty() ? Now : S.Pops.back();
		const double Jet = Safe(JetIn, 0.0, 120.0, 0.0);
		S.JetAt = JetFrom >= 0.0 ? Now + std::min(JetFrom, 600.0) : LastPop + (S.Pops.empty() ? 0.0 : D.U(0.3, 1.0));
		S.JetEnd = S.JetAt + Jet;
		S.JetTall = Safe(Range(P + "jet_tall_m", 3.0, 6.0).Pick(D), 0.1, 60.0, 4.5) * std::clamp(std::sqrt(S.Across / Safe(Num(P + "hatch_m", 0.6), 0.05, 20.0, 0.6)), 0.4, 3.0);
		S.JetDip = Jet >= 2.5 && D.U() < S.Cook.Two ? D.U(0.35, 0.6) : 0.0;
		S.Next = S.NextSmoke = S.JetAt;
		S.Flames = S.JetEnd;
		S.End = std::max(LastPop, S.JetEnd) + 0.5;
		Sources.push_back(S);
	}

	void Battle::CookPop(const Source& S, double T)
	{
		const CookRow& R = S.Cook;
		Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(T * 1000.0)) * 0x9E3779B97F4A7C15ull + 0x9090ull));
		const Vec Way = S.Way;
		const Vec SideA = Way.Cross(std::fabs(Way.Z) < 0.9 ? ZUp : Vec(1.0, 0.0, 0.0)).Unit(Vec(1.0, 0.0, 0.0));
		const Vec SideB = Way.Cross(SideA).Unit(Vec(0.0, 1.0, 0.0));
		const double K = std::clamp(std::sqrt(S.Across), 0.45, 3.0);
		auto Thrown = [&](double HalfDegrees)
		{
			const double Off = Radians(HalfDegrees) * std::sqrt(D.U()), Round_ = D.U(0.0, 2.0 * Pi);
			return (Way * std::cos(Off) + (SideA * std::cos(Round_) + SideB * std::sin(Round_)) * std::sin(Off)).Unit(Way);
		};
		// (the flash: by the flash rule, small, two frames; and its lamp)
		if (R.FlashK > 0.0)
		{
			RaggedFlash(T, S.At + Way * (0.1 * S.Across), R.FlashK * K, 1.0);
		}
		if (R.Lamp > 0.0)
		{
			Flash F;
			F.At = S.At + Way * 0.3;
			F.Col = R.Ember;
			F.Born = T;
			F.Life = 0.06;
			F.Strength = R.Lamp * K;
			F.Radius = 12.0;
			Flashes.push_back(F);
		}
		// (the puff: grey-black, out of the hatch along its way, rolling up and off on the wind)
		const int NPuffs = static_cast<int>(std::lround(R.Puffs.Pick(D)));
		for (int I = 0; I < NPuffs; ++I)
		{
			Bit B;
			B.Born = T + 0.02 + 0.05 * I;
			B.From = S.At + Way * (0.2 * S.Across) + Vec(D.U(-0.2, 0.2), D.U(-0.2, 0.2), 0.0) * S.Across;
			B.Vel = Thrown(25.0) * (D.U(3.0, 6.0) * K);
			B.Drag = 2.5;
			B.Rise = D.U(0.8, 1.6);
			B.Windage = 0.9;
			B.Life = D.U(2.0, 3.4);
			B.R0 = 0.35 * S.Across;
			B.R1 = D.U(1.1, 1.8) * K;
			B.Grow = 0.6;
			B.Col = R.Puff * D.U(0.85, 1.15);
			B.Col2 = B.Col.Mix(Rgb::FromPicture(110, 108, 104), 0.5);
			B.Alpha = R.PuffAlpha * D.U(0.85, 1.0);
			B.FadeIn = 0.04;
			B.Hold = 0.25;
			B.Heart = 0.65;
			B.HeartEnd = 0.3;
			B.Rough = 0.3;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			Bits.push_back(B);
		}
		// (the embers: orange, thrown up and out in short arcs, cooling to dull red as they fall)
		// (the art director on the first cut, 15:40Z: "the first pop is an even orange fan, the hand again". Thrown
		// together from one point at one speed, the embers' first frame was a hand of equal fingers. ember_uneven
		// over 1: most go low and slow and a few high (the power their heights are skewed by), one in four leaves a
		// few frames late, they start from across the hatch and some are thrown wide, so the flash is the pop's
		// first frame and no frame shows an even fan)
		const bool bUneven = R.EmberUneven > 1.0;
		const int NEmbers = static_cast<int>(std::lround(R.Embers.Pick(D)));
		for (int I = 0; I < NEmbers; ++I)
		{
			const double Up_ = bUneven ? R.EmberUp.A + (R.EmberUp.B - R.EmberUp.A) * std::pow(D.U(), R.EmberUneven) : R.EmberUp.Pick(D);
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.Born = T + (bUneven ? (D.U() < 0.25 ? D.U(0.1, 0.24) : D.U(0.01, 0.1)) : D.U(0.0, 0.03));
			B.From = S.At + Way * (0.15 * S.Across) + (bUneven ? (SideA * D.U(-0.4, 0.4) + SideB * D.U(-0.4, 0.4)) * S.Across : Vec());
			B.Vel = Thrown(bUneven && D.U() < 0.3 ? 62.0 : 38.0) * std::sqrt(2.0 * 9.8 * Up_);
			B.Gravity = 9.8;
			B.Drag = bUneven ? D.U(0.15, 0.9) : 0.25;
			B.Life = R.EmberLife.Pick(D);
			B.R0 = B.R1 = D.U(0.025, 0.05);
			B.Col = R.Ember * D.U(0.9, 1.15);
			B.Col2 = R.EmberOld;
			B.Alpha = 1.0;
			B.FadeIn = 0.002;
			B.Hold = 0.45;
			B.Heart = 0.9;
			B.Rim = 0.3;
			B.LeastPx = 1.5;
			B.bLight = true;
			Bits.push_back(B);
		}
		// (now and then a dark fragment, tumbling: a hatch cover's catch, a shell case; from a tank it lands and lies
		// a moment, from a ship's deck it is gone when it comes down)
		if (D.U() < R.FragmentOdds)
		{
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.bThrown = true;
			B.Born = T + 0.01;
			B.From = S.At + Way * (0.2 * S.Across);
			B.Vel = Thrown(30.0) * std::sqrt(2.0 * 9.8 * D.U(4.0, 14.0));
			B.Gravity = 9.8;
			B.R0 = B.R1 = D.U(0.06, 0.12) * std::clamp(K, 1.0, 2.0);
			B.Col = R.Fragment;
			B.Col2 = R.Fragment;
			B.Heart = 0.95;
			B.Rim = 0.6;
			B.LeastPx = ClodLeast;
			Chunk(B, D);
			Throw(B);
			const bool bHigh = S.At.Z - Height(S.At.X, S.At.Y) > 4.0;
			B.Life = bHigh ? B.Land : B.Land2 + 2.0;
			B.Hold = bHigh ? 0.98 : std::min(0.95, (B.Land2 + 0.5) / B.Life);
			Bits.push_back(B);
		}
	}

	bool Battle::LandFire(const Source& Fire) const
	{
		const double Wide = std::max(Fire.Across, Fire.AcrossTo);
		const double Below = SafeNum(Num("fire.smoke_column.from_m", 8.0), 0.0, 1e6, 8.0), Above = SafeNum(Num("fire.land_column.above_m", 6.0), 0.0, 1e6, 6.0);
		return Wide < Below && !(Fire.At.Z - Height(Fire.At.X, Fire.At.Y) > Above) && !Fire.bMoves && !Fire.bOnShip;
	}

	Battle::TongueRow Battle::TongueRowOf(bool bLand, bool bVent) const
	{
		// (fire.tongues, every dial held to a sane span. Without the row's every_s: off, and a burning thing's flames
		// are the upright cones they were. bLand, the pre-check of 22 November: head C's numbers and its mends are the
		// art director's for a burning tank, and in fire.tongues they reached every ship's fire too, twice the pieces
		// and redder. A recipe with fire.land_tongues gives a fire on land (LandFire) that whole row instead, and leaves
		// fire.tongues to the ships as they were)
		TongueRow R;
		// (bVent, a burning opening: fire.vent_tongues, narrow and tall out of a hatch, where the recipe has it)
		const bool bVentRow = bVent && Has("fire.vent_tongues.every_s");
		const std::string P = bVentRow ? "fire.vent_tongues." : bLand && Has("fire.land_tongues.every_s") ? "fire.land_tongues." : "fire.tongues.";
		R.bOn = Has(P + "every_s");
		if (!R.bOn)
		{
			return R;
		}
		auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		auto Held = [&](const std::string& Path, double A, double B, double Least, double Most)
		{
			Span Is = Range(Path, A, B);
			Is.A = Safe(Is.A, Least, Most, A);
			Is.B = std::max(Is.A, Safe(Is.B, Least, Most, B));
			return Is;
		};
		R.Every = Safe(Num(P + "every_s", 0.05), 0.02, 1.0, 0.05);
		R.Seats = Held(P + "seats", 3.0, 5.0, 1.0, 12.0);
		R.SeatsLeast = static_cast<int>(Safe(Num(P + "seats_least", 3.0), 1.0, 12.0, 3.0));
		R.Tall = Held(P + "tall", 0.6, 1.5, 0.05, bVentRow ? 12.0 : 6.0); // (a hatch's flame stands many times its opening)
		R.TallWide = Safe(Num(P + "tall_wide_m", 12.0), 0.5, 5000.0, 12.0);
		R.Wide = Held(P + "wide", 0.14, 0.24, 0.02, 1.0);
		R.Life = Held(P + "life_s", 0.22, 0.4, 0.05, 1.5);
		R.Pulse = Safe(Num(P + "pulse", 0.45), 0.0, 0.95, 0.45);
		R.Lean = Safe(Num(P + "lean", 0.06), 0.0, 1.0, 0.06);
		R.LeanMost = Safe(Num(P + "lean_most", 0.6), 0.0, 1.5, 0.6);
		R.Torn = Safe(Num(P + "torn", 0.5), 0.0, 1.0, 0.5);
		R.Tips = Safe(Num(P + "tips", 0.5), 0.0, 1.0, 0.5);
		R.Forks = Safe(Num(P + "forks", 0.6), 0.0, 1.0, 0.6);
		R.FullPx = Safe(Num(P + "full_px", 80.0), 0.0, 1e5, 80.0);
		R.Over = Safe(Num(P + "over_smoke", 0.0), 0.0, 2.0, 0.0);
		R.Root = Picture(P + "root", Rgb::FromPicture(255, 236, 170));
		R.Body = Picture(P + "body", Rgb::FromPicture(255, 196, 84));
		R.Tip = Picture(P + "tip", Rgb::FromPicture(176, 62, 30));
		R.RootOver = Safe(Num(P + "root_over", 1.0), 1.0, 8.0, 1.0);
		R.FarTall = Safe(Num(P + "far_tall", 0.0), 0.0, 2.0, 0.0);
		R.FarPx = Safe(Num(P + "far_px", 400.0), 0.0, 1e5, 400.0);
		R.FootSoft = Safe(Num(P + "foot_soft", 0.4), 0.1, 0.9, 0.4);
		// (smooth flames and the fire's body: see TongueRow::BornK and TongueRow::Mass. Every key at its old value
		// without the recipe's word: as before)
		R.BornK = Safe(Num(P + "born_k", 1.0), 0.05, 1.0, 1.0);
		R.FullBy = Safe(Num(P + "full_by", 0.3), 0.02, 0.9, 0.3);
		R.FadeShare = Safe(Num(P + "fade_in", 0.0), 0.0, 0.6, 0.0);
		R.Jitter = Safe(Num(P + "jitter", 0.0), 0.0, 1.0, 0.0);
		R.Anchor = Safe(Num(P + "anchor", 0.0), 0.0, 1.0, 0.0);
		R.Climb = Safe(Num(P + "climb", 1.0), 0.0, 2.0, 1.0);
		R.BedOverlap = Safe(Num(P + "bed_overlap", 1.0), 1.0, 6.0, 1.0);
		R.Flicker = Safe(Num(P + "flicker", 0.0), 0.0, 0.9, 0.0);
		R.FlickerLong = Safe(Num(P + "flicker_long", 0.0), 0.0, 0.9, 0.0);
		R.FlickerWide = Safe(Num(P + "flicker_wide", 0.0), 0.0, 0.9, 0.0);
		R.FlickerHz = Held(P + "flicker_hz", 3.0, 6.0, 0.1, 30.0);
		R.Point = Safe(Num(P + "point", 0.9), 0.0, 1.0, 0.9);
		R.LongMost = Safe(Num(P + "long_most", 5.0), 1.3, 8.0, 5.0);
		R.bSmooth = R.BornK < 1.0 || R.FadeShare > 0.0 || R.Jitter > 0.0 || R.Anchor > 0.0 || R.Climb != 1.0 || R.BedOverlap > 1.0 || R.Flicker > 0.0 || R.FlickerLong > 0.0 || R.FlickerWide > 0.0;
		R.Mass = Safe(Num(P + "mass", 0.0), 0.0, 8.0, 0.0);
		R.MassUp = Safe(Num(P + "mass_up", 0.7), 0.0, 3.0, 0.7);
		R.MassR = Held(P + "mass_r", 0.22, 0.34, 0.02, 2.0);
		R.MassLife = Held(P + "mass_life_s", 0.5, 0.8, 0.05, 5.0);
		R.MassCol = Picture(P + "mass_colour", R.Body);
		R.MassTop = Picture(P + "mass_top", R.Tip);
		R.Soot = Safe(Num(P + "soot", 0.0), 0.0, 8.0, 0.0);
		R.SootR = Held(P + "soot_r", 0.25, 0.4, 0.02, 2.0);
		R.SootLife = Held(P + "soot_life_s", 1.2, 2.0, 0.1, 20.0);
		R.SootCol = Picture(P + "soot_colour", Rgb::FromPicture(30, 26, 24));
		R.bBed = Has(P + "bed.0");
		R.Bed = Picture(P + "bed", Rgb());
		R.BedOver = Safe(Num(P + "bed_over", 0.0), 0.0, 4.0, 0.0);
		R.BedInner = Safe(Num(P + "bed_inner", 0.4), 0.0, 0.95, 0.4);
		R.BedTall = Safe(Num(P + "bed_tall", 1.0), 0.3, 3.0, 1.0);
		R.Inner = Safe(Num(P + "inner", 0.0), 0.0, 0.97, 0.0);
		R.bBedEdge = Has(P + "bed_edge.0");
		R.BedEdge = Picture(P + "bed_edge", Rgb());
		R.MassOver = Safe(Num(P + "mass_over", 1.0), 0.25, 4.0, 1.0);
		R.MassInner = Safe(Num(P + "mass_inner", 0.5), 0.0, 0.95, 0.5);
		R.bFlare = Has(P + "flare_every_s.0");
		R.FlareEvery = Held(P + "flare_every_s", 2.0, 3.0, 0.3, 60.0);
		R.FlareTall = Safe(Num(P + "flare_tall", 1.5), 1.0, 3.0, 1.5);
		R.FlareS = std::min(Safe(Num(P + "flare_s", 0.5), 0.1, 3.0, 0.5), 0.9 * R.FlareEvery.A);
		return R;
	}

	Battle::GlowRow Battle::GlowRowOf() const
	{
		// (fire.glow: the art director's ruling of 21 November, a burning tank, "the flame lights ... the ground within 5
		// to 8 m with a flicker"; 04:54Z 22 November, "a warm ground glow with a soft fall-off and no hard rim". No
		// every_s: none, as before)
		GlowRow R;
		const std::string P = "fire.glow.";
		R.bOn = Has(P + "every_s");
		if (!R.bOn)
		{
			return R;
		}
		R.Every = SafeNum(Num(P + "every_s", 1.0), 0.1, 60.0, 1.0);
		R.Life = SafeNum(Num(P + "life_s", 3.0), R.Every, 600.0, std::max(3.0, R.Every));
		R.R = SafeNum(Num(P + "r", 2.2), 0.0, 50.0, 2.2);
		R.RLeast = SafeNum(Num(P + "r_least_m", 3.0), 0.1, 500.0, 3.0);
		R.RMost = SafeNum(Num(P + "r_most_m", 9.0), R.RLeast, 2000.0, std::max(9.0, R.RLeast));
		R.Up = SafeNum(Num(P + "up_m", 0.2), 0.0, 10.0, 0.2);
		R.Above = SafeNum(Num(P + "above_m", 6.0), 0.0, 1.0e6, 6.0);
		R.Alpha = SafeNum(Num(P + "alpha", 0.35), 0.0, 1.0, 0.35);
		R.DayAlpha = SafeNum(Num(P + "day_alpha", 0.4), 0.0, 1.0, 0.4);
		R.Over = SafeNum(Num(P + "over", 1.0), 1.0, 8.0, 1.0);
		R.Flicker = SafeNum(Num(P + "flicker", 0.2), 0.0, 0.9, 0.2);
		Span Hz = Range(P + "flicker_hz", 1.5, 4.0);
		R.FlickerHz.A = SafeNum(Hz.A, 0.1, 30.0, 1.5);
		R.FlickerHz.B = SafeNum(Hz.B, R.FlickerHz.A, 30.0, std::max(4.0, R.FlickerHz.A));
		R.Col = Picture(P + "colour", Rgb::FromPicture(255, 150, 60));
		return R;
	}

	Battle::LateRow Battle::LateRowOf() const
	{
		// (fire.late: the ruling, section 3, a long fire burning down. No by_s: never, as before)
		LateRow R;
		const std::string P = "fire.late.";
		R.bOn = Has(P + "by_s");
		if (!R.bOn)
		{
			return R;
		}
		const Span From = Range(P + "from_s", 180.0, 300.0);
		R.From.A = SafeNum(From.A, 0.0, 1.0e6, 180.0);
		R.From.B = SafeNum(From.B, R.From.A, 1.0e6, std::max(300.0, R.From.A));
		R.By = SafeNum(Num(P + "by_s", 60.0), 0.1, 1.0e5, 60.0);
		R.Flames = SafeNum(Num(P + "flames", 0.4), 0.05, 1.0, 0.4);
		R.Light = SafeNum(Num(P + "light", 0.4), 0.0, 1.0, 0.4);
		R.Alpha = SafeNum(Num(P + "column_alpha", 0.6), 0.0, 1.0, 0.6);
		R.Col = Picture(P + "column_colour", Rgb::FromPicture(70, 60, 50));
		R.Top = Picture(P + "column_top", Rgb::FromPicture(110, 100, 88));
		return R;
	}

	double Battle::LateShare(const LateRow& R, uint64_t Seed, double Start, double T)
	{
		// (its own moment, by its seed, so a field of wrecks lit together never burn down together; smoothly over by_s)
		if (!R.bOn)
		{
			return 0.0;
		}
		Dice D(Seed * 0x9E3779B97F4A7C15ull ^ 0x1A7E5EEDull);
		const double From = R.From.A + (R.From.B - R.From.A) * D.U();
		const double G = Clamp01((T - Start - From) / std::max(R.By, 1e-3));
		return G == G ? G * G * (3.0 - 2.0 * G) : 0.0;
	}

	void Battle::FireGlow(Source& S, double Now, const GlowRow& R, double LateLight)
	{
		// The glow: soft warm pieces lying on the ground under the fire, a new one every Every seconds fading in over a
		// third of its life, so three overlap and their sum barely moves; all of one fire flicker together by its seed.
		// Only while it flames and only near the ground (a ship's deck fire high over the sea lights no sea), and less as
		// it is put out
		S.NextGlow = std::max(S.NextGlow, Now - 1.0);
		const double Ground = Height(S.At.X, S.At.Y);
		if (!(S.At.Z - Ground <= R.Above))
		{
			S.NextGlow = std::max(S.NextGlow, Now);
			return;
		}
		while (S.NextGlow <= Now && S.NextGlow < S.Flames)
		{
			const double Left = S.OutAt >= 0.0 && S.NextGlow > S.OutAt ? Clamp01(1.0 - (S.NextGlow - S.OutAt) / std::max(S.OutOver, 1e-3)) : 1.0;
			Bit B;
			B.Form = Bit::Shape::Flat;
			B.Born = S.NextGlow;
			B.Life = std::min(R.Life, std::max(R.Every, S.Flames - S.NextGlow + R.Life / 3.0));
			B.From = Vec(S.At.X, S.At.Y, Height(S.At.X, S.At.Y) + R.Up);
			B.R0 = B.R1 = std::clamp(R.R * std::min(S.Across, 2000.0), R.RLeast, R.RMost);
			B.Grow = 1.0;
			B.Col = R.Col;
			B.Col2 = R.Col;
			B.Alpha = R.Alpha * Left * Clamp01(LateLight);
			B.DayAlpha = R.DayAlpha;
			B.Over = R.Over;
			B.FadeIn = R.Life / 3.0;
			B.Hold = 1.0 / 3.0;
			B.Heart = 0.55;
			B.Inner = 0.3;
			B.Rim = 0.0; // (soft from its middle to nothing at its edge: no rim)
			B.ShadeK = 0.0;
			B.bLight = true;
			B.Nearer = -30.0; // (drawn first, under the fire and its smoke)
			B.FlickerSeed = static_cast<uint32_t>((S.Seed * 0x9E3779B97F4A7C15ull) >> 32) | 1u;
			B.Flicker = R.Flicker;
			B.FlickerLo = R.FlickerHz.A;
			B.FlickerHi = R.FlickerHz.B;
			if (B.Alpha > 0.0)
			{
				Bits.push_back(B);
			}
			S.NextGlow += R.Every;
		}
	}

	bool Battle::LandColumnStands(int Column, double T) const
	{
		for (const Source& C : Sources)
		{
			if (C.Kind == 3 && C.Id == Column)
			{
				return C.bLandColumn && !C.bStoodDown && T < C.End;
			}
		}
		return false;
	}

	void Battle::FireTongues(const Source& S, double T, const TongueRow& R, const Rgb& Flame, const Rgb& HeartIn, const Rgb& Ember, const LateRow* Late, double LateS)
	{
		const Rgb& Heart = R.bBed ? R.Bed : HeartIn; // (fire.tongues.bed: the foot's own colour, the art director's "never white")
		// One step of a burning thing's flames (fire.tongues). The art director, 17:54Z 20 November, on build 1778's
		// ships: the fires were "rows of pointed orange cones like birthday candles". Wanted: ragged tongues of uneven
		// height, roots on the skin with a yellow-white heart, tips tearing off and sweeping downwind. So a fire burns
		// at a few seats of its own, fixed on the skin where it was lit. Each seat throws tongues of its own height,
		// which surges and sinks several times a second, no two seats together; a tongue leans down the wind and bends
		// further over toward its tip, and dies by being eaten away from below; at each seat a low yellow-white root
		// is laid over the rest; and tips are torn off and carried away down the wind. Its own dice, by the fire's
		// seed and the step: an older recipe's fire draws as it did.
		const double Wide = std::min(S.Across, 2000.0);
		const int64_t Step = static_cast<int64_t>(std::floor(T / R.Every + 0.5));
		Dice D(S.Seed ^ (static_cast<uint64_t>(Step) * 0xC2B2AE3D27D4EB4Full + 0x70E5ull));
		Dice Lit(S.Seed * 0x9E3779B97F4A7C15ull ^ 0x5EA75ull);
		const int NSeats = Lit.Whole(static_cast<int>(R.Seats.A), static_cast<int>(R.Seats.B));
		const double Turn0 = Lit.U(0.0, 2.0 * Pi);
		// (a fire small in the picture burns at fewer seats, each as much wider: never under seats_least)
		const double SeenPx = bEyeKnown && R.FullPx > 0.0 ? Wide * SafeNum(LastPpr, 1.0, 1e7, 1000.0) / std::max(1.0, (LastEye - S.At).Length()) : R.FullPx;
		const double FarK = R.FullPx > 0.0 ? Clamp01(SeenPx / R.FullPx) : 1.0;
		const int Seen = std::clamp(static_cast<int>(NSeats * FarK + 0.5), std::min(R.SeatsLeast, NSeats), NSeats);
		const double WideK = std::sqrt(static_cast<double>(NSeats) / Seen);
		// (and its tongues stand taller, far_tall more at the most, the smaller it is under far_px: the art director,
		// 23:19Z, "taller far off ... so at 830 m it reads as fire before smoke")
		const double TallK = 1.0 + (bEyeKnown && R.FarPx > 0.0 ? R.FarTall * Clamp01(1.0 - SeenPx / R.FarPx) : 0.0);
		// (put out, fire.put_out: eaten back from its edges toward its seat, steadily lower, at fewer seats, each
		// tongue as dense as ever; a fit only lowers them a little for its third of a second)
		double Left = 1.0, Low = 1.0;
		if (S.OutAt >= 0.0 && T > S.OutAt)
		{
			Left = Clamp01(1.0 - (T - S.OutAt) / std::max(S.OutOver, 1e-3));
			Dice Fit(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(T / 0.3)) * 0xD1B54A32D192ED03ull + 0x70E6ull));
			Low = std::max(0.12, std::pow(Left, 1.4)) * (Fit.U() > 0.45 + 0.55 * Left ? 0.8 : 1.0);
		}
		// (burning down, fire.late: its flames lower and narrower, at as many seats, and its soot greyer)
		const double LateK = Late && Late->bOn ? Clamp01(LateS) : 0.0;
		Low *= Late ? std::clamp(1.0 - (1.0 - Late->Flames) * LateK, 0.05, 1.0) : 1.0;
		const int Shown = std::clamp(static_cast<int>(std::ceil(Seen * (0.25 + 0.75 * std::pow(Left, 1.2)))), 1, Seen);
		// (the lean: down the wind, and further over the higher up a tongue)
		// (on a moving ship, BurnMove: the wind over her deck, her own way against the true wind, so they lean aft)
		const Vec TrueWind = Wind.X == Wind.X && Wind.Y == Wind.Y ? Vec(Wind.X, Wind.Y, 0.0) : Vec();
		const Vec Blown = S.bMoves ? TrueWind - Vec(S.Vel.X, S.Vel.Y, 0.0) : TrueWind;
		const double Blow = Blown.Length();
		const Vec Down = Blow > 1e-6 && Blow < 1e6 ? Blown * (1.0 / Blow) : Vec();
		const double Lean = std::min(R.LeanMost, R.Lean * std::min(Blow, 40.0));
		auto Bent = [&](double H) { return (ZUp + Down * (Lean * (0.2 + 1.6 * H))).Unit(ZUp); };
		auto Spine = [&](const Vec& Seat, double H, double Tall) { return Seat + (ZUp + Down * (Lean * (0.2 + 0.8 * H))) * (H * Tall); };
		auto Around = [&]()
		{
			const double A = D.U(0.0, 2.0 * Pi);
			return Vec(std::cos(A), std::sin(A), 0.0);
		};
		auto Leaning = [&](const Vec& About, double Degrees)
		{
			return (About * std::cos(Radians(Degrees)) + Around() * std::sin(Radians(Degrees))).Unit(About);
		};
		// (a wide fire's tongues are not as tall for its width as a small one's)
		const double SizeK = std::min(1.0, std::pow(R.TallWide / std::max(Wide, 0.01), 0.25));
		// (a seat: where on the skin, how tall its tongues for the fire's width, how wide, and its own surge)
		struct SeatIs
		{
			Vec At;
			double Tall = 1.0, Half = 0.2, High = 1.0; // (High: 1 the fire's tallest seat, 0 its lowest)
			int I = 0;
		};
		auto SeatOf = [&](int I)
		{
			Dice Sd((S.Seed ^ (static_cast<uint64_t>(I + 1) * 0xA0761D6478BD642Full)) * 0x9E3779B97F4A7C15ull + 0x5EA7ull);
			// (round the fire's middle by turns, so the fewest seats still stand apart across its width)
			const double A = Turn0 + 2.0 * Pi * (I + Sd.U(-0.3, 0.3)) / NSeats, Off = std::sqrt(Sd.U(0.12, 1.0));
			const double High = I == 0 ? 1.0 : std::pow(Sd.U(), 1.5); // (one tall, the rest uneven, more of them short)
			const double Share = R.Tall.A + (R.Tall.B - R.Tall.A) * High;
			const double Half = R.Wide.Pick(Sd) * Wide * WideK * (1.25 - 0.35 * High); // (a low seat burns broad, a tall one narrower)
			const double Phase = Sd.U(0.0, 2.0 * Pi), F1 = Sd.U(1.2, 2.4), F2 = Sd.U(3.5, 6.5);
			const double Surge = 1.0 - R.Pulse * (0.5 - 0.5 * (0.6 * std::sin(2.0 * Pi * F1 * T + Phase) + 0.4 * std::sin(2.0 * Pi * F2 * T + 2.1 * Phase)));
			SeatIs Seat;
			Seat.At = S.At + Vec(std::cos(A), std::sin(A), 0.0) * (0.5 * Wide * Off * Left);
			Seat.Tall = std::max(0.05, Share * Wide * SizeK * TallK * Surge * Low);
			Seat.Half = std::max(0.01, Half * (0.5 + 0.5 * Low));
			Seat.High = High;
			Seat.I = I;
			return Seat;
		};
		// (smooth flames, TongueRow::BornK: their own dice, so the shapes thrown are the same as without them)
		Dice Jit(S.Seed ^ (static_cast<uint64_t>(Step) * 0x94D049BB133111EBull + 0x5100ull));
		// (the fire's own smoke rolls off its middle: flames at its far seats are sorted this much nearer the eye, so
		// they show through it as they do through real smoke, where the eye finds the brightest thing)
		const double Over = R.Over * Wide;
		auto Tongue = [&](const Vec& From, const Vec& Dir, double Speed, double Drag_, double HalfA, double HalfB, double Long, const Rgb& Col, const Rgb& Col2, double Life, double Nearer, double Windage)
		{
			Bit B;
			B.Born = T;
			B.Life = Life;
			B.From = From + Dir * (HalfA * Long);
			B.Vel = Dir * Speed;
			B.Drag = Drag_;
			B.Windage = Windage;
			B.R0 = HalfA;
			B.R1 = HalfB;
			B.Grow = 0.8;
			B.Along = Dir;
			B.Stretch = Long;
			B.Point = R.Point;
			B.Col = Col;
			B.Col2 = Col2;
			B.Alpha = 0.95;
			B.FadeIn = 0.02;
			B.Hold = 0.4;
			B.Heart = 0.8;
			B.Inner = 0.45; // (soft-edged: a flame, never cut paper)
			B.Rim = 0.0;
			B.Rough = 0.3;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Nearer = Over + Nearer;
			B.bLight = true;
			if (R.bSmooth)
			{
				// (the frame study of 22 November: each tongue was born whole on a step of a twentieth of a second and
				// only shrank, so at 30 frames a second the fire popped ten times a second. So it is born late by up to a
				// step, small, grows to its size by full_by of its life and fades in; its root stays where it rose from,
				// and it climbs climb as fast)
				B.Born = T + R.Jitter * R.Every * Jit.U();
				if (R.BornK < 1.0)
				{
					B.GrowBy = R.FullBy * Life;
					B.RMid = HalfA;
					B.R0 = HalfA * R.BornK;
					B.GrowEarly = 0.7;
				}
				B.FadeIn = std::max(B.FadeIn, R.FadeShare * Life);
				if (R.Anchor > 0.0)
				{
					B.Anchor = R.Anchor;
					B.From = From + Dir * (HalfA * Long * (1.0 - R.Anchor));
				}
				B.Vel = Dir * (Speed * R.Climb);
			}
			Bits.push_back(B);
		};
		// (and every piece of a seat flickers together, by the clock: Bit::Flicker)
		const bool bFlick = R.bSmooth && (R.Flicker > 0.0 || R.FlickerLong > 0.0 || R.FlickerWide > 0.0);
		auto Flick = [&](const SeatIs& Seat_)
		{
			if (!bFlick)
			{
				return;
			}
			Bit& B = Bits.back();
			B.FlickerSeed = static_cast<uint32_t>(((S.Seed ^ (static_cast<uint64_t>(Seat_.I + 1) * 0xA0761D6478BD642Full)) * 0x9E3779B97F4A7C15ull) >> 32) | 1u;
			B.Flicker = R.Flicker;
			B.FlickerLong = R.FlickerLong;
			B.FlickerWide = R.FlickerWide;
			B.FlickerLo = R.FlickerHz.A;
			B.FlickerHi = R.FlickerHz.B;
		};
		const int Turn = static_cast<int>(((Step % Shown) + Shown) % Shown);
		// (the art director on the first sheet, 23:19Z: "each flame is one flat orange from root to tip. Want a
		// white-yellow heart at the foot, an orange body, and tips going a dull deep red as they fade into the smoke."
		// So a piece is lit along its length: its own colour sits at its root, Hub of its half length back from its
		// middle, and runs out to Edge at its rim and so at its tip; its heart ring lies Inner of the way from the
		// root to the rim and is EdgeIn of the way to that colour)
		auto Rooted = [&](double Hub, const Rgb& Edge, double EdgeIn_, double Inner)
		{
			Bit& B = Bits.back();
			B.Hub = Hub;
			B.Edge = Edge;
			B.EdgeK = 1.0;
			B.EdgeIn = EdgeIn_;
			B.Inner = Inner;
		};
		{
			// the bed: at each seat in its turn, so that it never goes out while the fire burns. A foot, the flame
			// widest where it leaves the skin and yellow there, where it burns hottest, going orange toward its top
			const SeatIs Seat = SeatOf(Turn);
			const double Stays = Shown * R.Every * D.U(1.0, 1.2) * R.BedOverlap;
			const double HalfH = Seat.Half * D.U(0.85, 1.05);
			Tongue(Seat.At - ZUp * (0.25 * HalfH), Leaning(Bent(0.1), D.U(0.0, 8.0)), Seat.Tall * D.U(0.15, 0.3) * R.BedTall, 3.0, HalfH, HalfH * D.U(0.5, 0.7), std::clamp(0.5 * Seat.Tall * R.BedTall * D.U(0.3, 0.45) / HalfH, 1.0, 2.2 * R.BedTall), Heart, Flame.Mix(Heart, 0.5), Stays, 0.05, 0.2);
			Bits.back().Point = 0.6;
			if (R.BedOver > 0.0)
			{
				Bits.back().Over = R.BedOver; // (bed_over: the base metre yellow-orange, the art director 06:43Z 22 November)
			}
			Rooted(0.6, R.bBedEdge ? R.BedEdge : Flame, 0.45, R.BedInner);
			Flick(Seat);
			// and its heart (the art director: the round root "reads as a lamp. Spread the heart along the foot as a
			// ragged white-yellow band under the tongues, brightest under the tallest, never a disc"): two short licks
			// side by side along the foot, across the eye's line, each blunt and of its own height, white-yellow at
			// its root and melting into the foot's yellow, so the band's top is broken and it is no one shape
			const Vec Flat = bEyeKnown ? (LastEye - S.At).Cross(ZUp).Unit(Vec(1.0, 0.0, 0.0)) : Vec(1.0, 0.0, 0.0);
			for (int Q = 0; Q < 2; ++Q)
			{
				const double Side = (Q == 0 ? -1.0 : 1.0) * D.U(0.3, 0.6);
				const double HalfR = Seat.Half * D.U(0.4, 0.55) * (0.85 + 0.25 * Seat.High);
				Tongue(Seat.At + Flat * (Side * Seat.Half) + ZUp * (0.1 * HalfR), Leaning(Bent(0.1), D.U(0.0, 14.0)), Seat.Tall * D.U(0.05, 0.12) * R.BedTall, 4.0, HalfR, HalfR * D.U(0.6, 0.8), D.U(1.4, 2.1) * R.BedTall, R.Root, Heart, Stays, 0.1 + 0.02 * Q, 0.1);
				Bit& B = Bits.back();
				B.Over = 1.0 + (R.RootOver - 1.0) * (0.5 + 0.5 * Seat.High);
				B.Alpha = 1.0;
				B.Heart = 1.0;
				B.Hold = 0.7;
				B.Point = D.U(0.4, 0.7);
				B.Rough = D.U(0.25, 0.38);
				// (its edge written that much darker, since the piece burns that far past white. foot_soft: how much of
				// it fades away toward its rim. The art director on the mended sheet, 23:51Z: "soften the foot licks a
				// little so they don't read as cut shapes up close")
				Rooted(0.55, Heart * (1.0 / B.Over), 0.15, 1.0 - R.FootSoft);
				Flick(Seat);
			}
		}
		// the tongues: out of a seat or two each step, by the dice, so no seat burns to a beat and the fire's top is
		// never the same twice. As the room fills a fire keeps its bed and its tongues' bodies, and loses their
		// forks, thin tips and torn tips: all of them with the room seven tenths full, none at seventeen twentieths,
		// by degrees between (every fire reads the same room in the same step: at one line they would all drop them
		// and take them back together, the pre-check's note of 23:25Z)
		const int Serve = (Shown + 2) / 3;
		const double BodyOver = 1.0 + 0.75 * (R.RootOver - 1.0); // (a tongue's own root burns most of the way to the foot's white)
		const double Fine = FarK * Clamp01((0.85 - static_cast<double>(RoomGate()) / std::max(1.0, static_cast<double>(MaxPuffs))) / 0.15);
		// (the body's puffs and soot, Mass and Soot: fewer as the fire is small in the picture, down to a quarter, and half
		// as many with the room near full, each as much bigger, so its heap still covers the same and the room is spared)
		const double BodyK = std::max(0.25, FarK) * (0.5 + 0.5 * Clamp01((0.85 - static_cast<double>(RoomGate()) / std::max(1.0, static_cast<double>(MaxPuffs))) / 0.15));
		for (int K = 0; K < Serve; ++K)
		{
			const SeatIs Seat = SeatOf(D.Whole(0, Shown - 1));
			const double Life = R.Life.Pick(D);
			// (the body: rooted at the seat, as long as most of the tongue's height; it goes up and narrows, eaten away
			// from below. Yellow at its root, orange up its middle, the tip's dull red at its top)
			const double HalfA = Seat.Half * D.U(0.75, 1.1), HalfB = HalfA * D.U(0.3, 0.5);
			const double Long = std::clamp(0.5 * Seat.Tall * D.U(0.4, 0.78) / HalfA, 1.3, R.LongMost);
			const double Reach = Seat.Tall * D.U(0.18, 0.32);
			Tongue(Seat.At + Around() * (0.6 * Seat.Half * D.U()), Leaning(Bent(0.3), D.U(0.0, 14.0)), Reach * (2.0 / Life) / 0.8647, 2.0 / Life, HalfA, HalfB, Long, R.Body, Flame.Mix(R.Tip, 0.4), Life, 0.0, 0.2);
			Bits.back().Alpha = 1.0;
			Bits.back().Heart = 0.9;
			Bits.back().Hold = 0.6;
			Bits.back().Over = BodyOver;
			Rooted(0.75, Flame.Mix(R.Tip, 0.6) * (1.0 / BodyOver), 0.45, R.Inner > 0.0 ? R.Inner : 0.8); // (dense to near its tip, and reddening there and along its edge; inner: crisper)
			Flick(Seat);
			if (D.U() < R.Forks * Fine)
			{
				// (a fork: a thinner tongue out of the same root, leaning to one side of the body, so the two part
				// toward their tips and the flame's top is torn, never one point)
				const double HalfF = HalfA * D.U(0.55, 0.75), LifeF = Life * D.U(0.7, 0.95);
				const double LongF = std::clamp(0.5 * Seat.Tall * D.U(0.4, 0.62) / HalfF, 1.3, std::min(3.5, R.LongMost));
				Tongue(Seat.At + Around() * (0.5 * Seat.Half * D.U()), Leaning(Bent(0.5), D.U(10.0, 24.0)), Seat.Tall * D.U(0.15, 0.3) * (2.0 / LifeF) / 0.8647, 2.0 / LifeF, HalfF, HalfF * D.U(0.25, 0.4), LongF, R.Body.Mix(Flame, 0.4), Flame.Mix(R.Tip, 0.6), LifeF, 0.0, 0.3);
				Bits.back().Heart = 0.9;
				Bits.back().Hold = 0.55;
				Rooted(0.7, R.Tip, 0.6, R.Inner > 0.0 ? R.Inner : 0.78);
				Flick(Seat);
			}
			if (D.U() < R.Tips * Fine)
			{
				// (its tip: a thin tongue above the body, leaning further over, narrowing to a point: orange where it
				// leaves the body, dull red above and darkening as it goes into the smoke)
				const double At_ = D.U(0.4, 0.65), HalfT = Seat.Half * D.U(0.35, 0.6);
				const double LongT = std::clamp(0.5 * Seat.Tall * D.U(0.3, 0.5) / HalfT, 1.5, std::max(1.5, std::min(3.5, R.LongMost)));
				Tongue(Spine(Seat.At, At_, Seat.Tall), Leaning(Bent(At_), D.U(5.0, 22.0)), Seat.Tall * D.U(0.5, 1.1), 3.0, HalfT, HalfT * 0.3, LongT, Flame.Mix(R.Tip, 0.3), R.Tip.Mix(Ember, 0.5), Life * D.U(0.6, 0.85), 0.0, 0.5);
				Bits.back().Heart = 0.9;
				Bits.back().Hold = 0.65; // (it goes by shrinking, dense and red to the last, never a pale ghost on the sky)
				Rooted(0.6, R.Tip.Mix(Ember, 0.3), 0.7, 0.75);
				Flick(Seat);
			}
		}
		auto Tear = [&](const SeatIs& Seat, double AtLo, double AtHi)
		{
			// (a tip torn off: out of a tall tongue's top, the first thing left out when the room fills. The art
			// director, 23:19Z: the first were "pointed darts flying across the smoke, which read as sparks. Each
			// should be gone in 0.2 to 0.3 s within one tongue's height of the top, ragged and darkening into smoke as
			// it goes." So a ragged scrap, blunt, thrown up a little faster than the tongue climbs and slowed at once,
			// dull red going to the smoke's own dark, gone in a quarter of a second)
			const double At_ = D.U(AtLo, AtHi), HalfA = Seat.Half * D.U(0.3, 0.5);
			const Vec Aside = Around();
			const Vec Dir = (Bent(At_) + Down * D.U(0.2, 0.6) + Aside * std::tan(Radians(D.U(6.0, 22.0)))).Unit(ZUp);
			Tongue(Spine(Seat.At, At_, Seat.Tall) + Aside * (Seat.Half * D.U(0.2, 0.7)), Dir, Seat.Tall * D.U(1.5, 2.5), 4.0, HalfA, HalfA * 0.35, D.U(1.3, 1.9), Flame.Mix(R.Tip, 0.5), Rgb::FromPicture(62, 46, 40), D.U(0.2, 0.3), 0.0, 1.0);
			Bits.back().Point = 0.5;
			Bits.back().Rough = 0.6;
			Bits.back().Hold = 0.7;
			Rooted(0.4, R.Tip.Mix(Ember, 0.4), 0.5, 0.6);
			Flick(Seat);
		};
		if (D.U() < R.Torn * Left * Fine)
		{
			// (off the taller of two seats: the low ones have no tips to lose)
			const SeatIs SeatA = SeatOf(D.Whole(0, Shown - 1)), SeatB = SeatOf(D.Whole(0, Shown - 1));
			const SeatIs& Seat = SeatA.Tall > SeatB.Tall ? SeatA : SeatB;
			Tear(Seat, 0.6, 0.9);
		}
		// (the fire's body, TongueRow::Mass, the art director's ruling: "one merged body, not tongues side by side:
		// overlapping, camera-facing flame puffs". Soft round puffs out of the seats, born small, swelling and rising
		// through the body, orange-yellow at the heart going dark red at the rim and as they climb; at its top black
		// soot rolls off, Soot, so the flames go into the smoke within a metre or two. Their own dice)
		auto HowMany = [&](double Mean) { const int Whole = static_cast<int>(Mean); return Whole + (Jit.U() < Mean - Whole ? 1 : 0); };
		const int NMass = R.Mass > 0.0 ? HowMany(R.Mass * (0.4 + 0.6 * Left) * BodyK) : 0;
		for (int K = 0; K < NMass; ++K)
		{
			const SeatIs Seat = SeatOf(Jit.Whole(0, Shown - 1));
			const double Life = R.MassLife.Pick(Jit);
			const double Rm = R.MassR.Pick(Jit) * Wide * (0.5 + 0.5 * Low) * std::min(1.6, std::sqrt(1.0 / BodyK));
			const double Ang = Jit.U(0.0, 2.0 * Pi), Off = std::sqrt(Jit.U()) * 0.35 * Wide * Left;
			const double Drag_ = 1.5 / Life;
			const double Up = R.MassUp * Seat.Tall * Jit.U(0.6, 1.0);
			Bit B;
			B.Born = T + R.Jitter * R.Every * Jit.U();
			B.Life = Life;
			B.From = (Seat.At * 0.5 + S.At * 0.5) + Vec(std::cos(Ang), std::sin(Ang), 0.0) * Off + ZUp * (0.35 * Rm);
			B.Vel = Bent(0.5) * (Up * Drag_ / (1.0 - std::exp(-Drag_ * Life)));
			B.Drag = Drag_;
			B.Windage = 0.3;
			B.R0 = Rm * R.BornK;
			B.RMid = Rm;
			B.GrowBy = R.FullBy * Life;
			B.GrowEarly = 0.7;
			B.R1 = Rm * 0.7;
			B.Grow = 1.0;
			B.Along = Bent(0.6);
			B.Stretch = Jit.U(1.15, 1.5);
			B.Point = 0.35;
			B.Col = R.MassCol;
			B.Col2 = R.MassTop;
			B.Cool = 0.8;
			B.Alpha = 0.9;
			B.FadeIn = std::max(0.03, R.FadeShare * Life);
			B.Hold = 0.45;
			B.Heart = 0.85;
			B.Inner = R.MassInner; // (mass_inner: crisper toward its rim, the art director 06:43Z 22 November)
			B.Rim = 0.0;
			B.Rough = 0.25;
			B.LumpSeed = static_cast<uint32_t>(Jit.U() * 4294967295.0);
			B.Nearer = Over - 0.05 * Wide;
			B.bLight = true;
			B.Over = R.MassOver;
			B.Edge = R.MassTop * (1.0 / R.MassOver);
			B.EdgeK = 0.85;
			B.EdgeIn = 0.35;
			Bits.push_back(B);
			Flick(Seat);
		}
		const int NSoot = R.Soot > 0.0 ? HowMany(R.Soot * Left * BodyK) : 0;
		for (int K = 0; K < NSoot; ++K)
		{
			const SeatIs Seat = SeatOf(Jit.Whole(0, Shown - 1));
			const double Life = R.SootLife.Pick(Jit);
			const double Rs = R.SootR.Pick(Jit) * Wide * std::min(1.6, std::sqrt(1.0 / BodyK));
			Bit B;
			B.Born = T + R.Every * Jit.U();
			B.Life = Life;
			B.From = Spine(Seat.At, Jit.U(0.7, 0.95), Seat.Tall) + Vec(Jit.Bell(), Jit.Bell(), 0.0) * (0.2 * Wide);
			B.Vel = ZUp * Jit.U(1.5, 3.0) + Down * (0.3 * Blow);
			B.Drag = 0.6;
			B.Rise = Jit.U(0.8, 1.5);
			B.Windage = 0.8;
			B.R0 = Rs * 0.6;
			B.R1 = Rs * 1.8;
			B.Grow = 0.7;
			B.Col = R.SootCol;
			B.Col2 = R.SootCol * 1.25;
			if (LateK > 0.0)
			{
				B.Col = B.Col.Mix(Late->Col, LateK);
				B.Col2 = B.Col2.Mix(Late->Top, LateK);
			}
			B.Alpha = 0.85;
			B.FadeIn = 0.3 * Life;
			B.Hold = 0.4;
			B.Heart = 0.75;
			B.Inner = 0.55;
			B.Rim = 0.05;
			B.Rough = 0.3;
			B.LumpSeed = static_cast<uint32_t>(Jit.U() * 4294967295.0);
			B.Nearer = Over;
			Bits.push_back(B);
		}
		// (a flare, TongueRow::bFlare, the art director 06:43Z 22 November: "about one lick every 2 to 3 s reaching 5 to
		// 6 m over the roof". Out of the fire's tallest seat by its own clock and seed: time cut into slots of the mean
		// gap, one flare in each at a point of its own within the span, swelling to flare_tall of the seat's height and
		// back over flare_s, throwing a tall lick and its tip a step, its top torn off as it falls. Laid after every
		// other piece and with dice of its own, so the rest of the fire is drawn as without it)
		if (R.bFlare && Left > 0.5)
		{
			const double Gap = 0.5 * (R.FlareEvery.A + R.FlareEvery.B), Since = T - S.Start;
			const double SlotAt = std::floor(Since / Gap);
			double Swell = 0.0, Past = 0.0;
			for (double N = SlotAt - 1.0; N <= SlotAt && SlotAt == SlotAt && std::abs(SlotAt) < 1e12; N += 1.0)
			{
				Dice Fd(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(N) + 7) * 0xF1A3E5D7C9B2A4E1ull + 0xF1A2Eull));
				const double U = (Since - (N * Gap + Fd.U(0.0, R.FlareEvery.B - R.FlareEvery.A))) / R.FlareS;
				if (U >= 0.0 && U < 1.0)
				{
					Swell = std::sin(Pi * U) * std::sin(Pi * U);
					Past = U;
				}
			}
			if (Swell > 0.02)
			{
				Dice Fl(S.Seed ^ (static_cast<uint64_t>(Step) * 0xE7037ED1A0B428DBull + 0xF1A3ull));
				SeatIs Seat = SeatOf(0);
				Seat.Tall *= 1.0 + (R.FlareTall - 1.0) * Swell;
				const double Life = R.Life.Pick(Fl);
				const double HalfA = Seat.Half * Fl.U(0.8, 1.0), HalfB = HalfA * Fl.U(0.4, 0.55);
				const double Long = std::clamp(0.5 * Seat.Tall * Fl.U(0.9, 1.1) / HalfA, 1.3, 10.0); // (thin and long, the seat's whole height: a lick, not a heap)
				const double Reach = Seat.Tall * Fl.U(0.25, 0.35);
				Tongue(Seat.At + Vec(Fl.Bell(), Fl.Bell(), 0.0) * (0.2 * Seat.Half), Bent(0.3), Reach * (2.0 / Life) / 0.8647, 2.0 / Life, HalfA, HalfB, Long, R.Body, Flame.Mix(R.Tip, 0.4), Life, 0.0, 0.2);
				Bits.back().Point = std::max(R.Point, 0.55);
				Bits.back().Alpha = 1.0;
				Bits.back().Heart = 0.9;
				Bits.back().Hold = 0.6;
				Bits.back().Over = BodyOver;
				Rooted(0.75, Flame.Mix(R.Tip, 0.6) * (1.0 / BodyOver), 0.45, R.Inner > 0.0 ? R.Inner : 0.8);
				Flick(Seat);
				const double At_ = Fl.U(0.55, 0.7), HalfT = HalfA * Fl.U(0.5, 0.7);
				const double LongT = std::clamp(0.5 * Seat.Tall * Fl.U(0.3, 0.45) / HalfT, 1.5, 5.0);
				Tongue(Spine(Seat.At, At_, Seat.Tall), Bent(At_), Seat.Tall * Fl.U(0.6, 1.0), 3.0, HalfT, HalfT * 0.3, LongT, Flame.Mix(R.Tip, 0.3), R.Tip.Mix(Ember, 0.5), Life * Fl.U(0.6, 0.85), 0.0, 0.5);
				Bits.back().Point = 0.8;
				Bits.back().Heart = 0.9;
				Bits.back().Hold = 0.65;
				Rooted(0.6, R.Tip.Mix(Ember, 0.3), 0.7, 0.75);
				Flick(Seat);
				if (Past > 0.45 && Fl.U() < 0.6)
				{
					Tear(Seat, 0.8, 1.0); // (and as it falls its top breaks off and goes)
				}
			}
		}
	}

	void Battle::CookJet(const Source& S, double T, double Full)
	{
		// One step of the hatch jet, by the fire rules: long ragged tapering tongues rooted at the hatch and streaming
		// along its way, as dense as ever however low it burns, and black smoke rolling off it.
		// The art director on the first cut, 15:40Z: "the jet does read as a tall candle. Give it a hot near-white
		// yellow root at the hatch as its brightest point. In the upper half, the sides tear into 2 to 4 tongues that
		// break off and vanish. Width and height pulse about 30%, several times a second. It leans with the wind,
		// sparks stream up in it, and black smoke rolls off its sides as well as its top. ... The smoke above is a
		// chain of separate blobs. Overlap them into one torn column." So: its tongues live a fifth of a second, short
		// enough for the surge (jet_pulse) to show; the root is white-hot and laid over the rest; tongues are torn
		// off its upper sides (jet_torn a step), leaning out, going up faster than the body and shrinking away; it
		// leans down the wind and bends further over toward its top (jet_lean); sparks go up through it
		// (jet_sparks); and its smoke leaves by the clock, never by the dice, from its sides and its top, each puff
		// born wider than the gap to the last
		const CookRow& R = S.Cook;
		Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(T * 1000.0)) * 0xD6E8FEB86659FD93ull + 0x7E7ull));
		const double Phase = static_cast<double>(S.Seed % 6283ull) * 0.001;
		const double Beat = T - S.JetAt;
		// (the surge takes from its height and never adds: at its fullest it is the height it was given, the art
		// director's 3 to 6 m for a tank's hatch, and it sinks jet_pulse of that and comes back several times a second)
		const double PulseTall = 1.0 - R.JetPulse * (0.5 - 0.5 * (0.6 * std::sin(2.0 * Pi * 3.3 * Beat + Phase) + 0.4 * std::sin(2.0 * Pi * 7.9 * Beat + 2.1 * Phase)));
		const double PulseWide = 1.0 - R.JetPulse * (0.5 - 0.5 * (0.6 * std::sin(2.0 * Pi * 4.1 * Beat + 1.7 * Phase) + 0.4 * std::sin(2.0 * Pi * 9.7 * Beat + 0.6 * Phase)));
		const double Tall = S.JetTall * Full * PulseTall * D.U(0.9, 1.0);
		if (!(Tall > 0.1))
		{
			return;
		}
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		const Rgb Heart = Rgb::FromPicture(255, 214, 130);
		const Rgb Ember = Rgb::FromPicture(110, 35, 15);
		const Vec Way = S.Way;
		const Vec SideA = Way.Cross(std::fabs(Way.Z) < 0.9 ? ZUp : Vec(1.0, 0.0, 0.0)).Unit(Vec(1.0, 0.0, 0.0));
		const Vec SideB = Way.Cross(SideA).Unit(Vec(0.0, 1.0, 0.0));
		// (its widest: jet_wide of its height, and never narrower than the hatch it comes from)
		const double Root = 0.5 * S.Across;
		const double Wide = std::max(Root, 0.5 * R.JetWide * S.JetTall * PulseWide) * (0.6 + 0.4 * Full);
		const double K = std::clamp(std::sqrt(S.Across), 0.45, 3.0);
		// (the lean: across its way, down the wind; Bent(H) is its heading this share of the way up it)
		const Vec Blown = Wind.X == Wind.X && Wind.Y == Wind.Y && Wind.Z == Wind.Z ? Wind - Way * Wind.Dot(Way) : Vec();
		const double Blow = Blown.Length();
		const Vec Down = Blow > 1e-6 ? Blown * (1.0 / Blow) : Vec();
		const double Lean = std::min(R.JetLeanMost, R.JetLean * std::min(Blow, 40.0));
		auto Bent = [&](double H) { return (Way + Down * (Lean * (0.4 + 1.2 * H))).Unit(Way); };
		// (where its middle is, that share of the way up: the heading run up from the hatch)
		auto Spine = [&](double H) { return S.At + (Way + Down * (Lean * (0.4 + 0.6 * H))) * (H * Tall); };
		auto Around = [&]()
		{
			const double A = D.U(0.0, 2.0 * Pi);
			return SideA * std::cos(A) + SideB * std::sin(A);
		};
		auto Leaning = [&](const Vec& About, double Degrees)
		{
			return (About * std::cos(Radians(Degrees)) + Around() * std::sin(Radians(Degrees))).Unit(About);
		};
		auto Tongue = [&](const Vec& From, const Vec& Dir, double Speed, double Drag_, double HalfA, double HalfB, double Long, const Rgb& Col, const Rgb& Col2, double Life, double Nearer)
		{
			Bit B;
			B.Born = T;
			B.Life = Life;
			B.From = From + Dir * (HalfA * Long);
			B.Vel = Dir * Speed;
			B.Drag = Drag_;
			B.Windage = 0.2;
			B.R0 = HalfA;
			B.R1 = HalfB;
			B.Grow = 0.8;
			B.Along = Dir;
			B.Stretch = Long;
			B.Point = 0.9;
			B.Col = Col;
			B.Col2 = Col2;
			B.Alpha = 0.95;
			B.FadeIn = 0.02;
			B.Hold = 0.4;
			B.Heart = 0.75;
			B.Rim = 0.1;
			B.Rough = 0.3;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Nearer = Nearer;
			B.bLight = true;
			Bits.push_back(B);
		};
		const bool bRoar = R.JetPulse > 0.0 || R.JetTorn > 0.0;
		// (the body: tongues driven up out of the hatch, fast and slowing, widening from the hatch's width to the jet's:
		// each reaches the top, so those alive together overlap into one roaring column with a ragged edge)
		const int NBody = bRoar ? D.Whole(2, 3) : D.Whole(1, 2);
		for (int I = 0; I < NBody; ++I)
		{
			const double Life = bRoar ? D.U(0.16, 0.26) : D.U(0.3, 0.45), Long = D.U(2.0, 2.8);
			const double HalfB = Wide * D.U(0.7, 1.0);
			const double Reach = std::max(0.0, Tall * D.U(0.7, 1.0) - HalfB * Long);
			const Vec Off = (SideA * D.U(-1.0, 1.0) + SideB * D.U(-1.0, 1.0)) * (0.25 * Root);
			Tongue(S.At + Off, Leaning(Bent(0.35), D.U(0.0, 5.0)), Reach * (2.0 / Life) / 0.8647, 2.0 / Life, Root * D.U(0.85, 1.1), HalfB, Long, Flame.Mix(Heart, 0.35), Flame.Mix(Ember, 0.6), Life, 0.0);
		}
		// (the tips: thin tongues at its top, leaning out, narrowing to a point and darkening)
		const int NTips = bRoar ? D.Whole(1, 2) : D.Whole(2, 3);
		for (int I = 0; I < NTips; ++I)
		{
			const double At_ = D.U(0.3, 0.8), Life = D.U(0.14, 0.26), HalfA = Wide * D.U(0.3, 0.6);
			const Vec Dir = Leaning(Bent(At_), D.U(3.0, 16.0));
			Tongue(Spine(At_), Dir, Tall * D.U(0.8, 1.6), 3.0, HalfA, HalfA * 0.3, D.U(3.0, 4.5), Flame.Mix(Ember, 0.2), Ember, Life, 0.0);
		}
		// (the torn tongues: off its side in its upper half, leaning out from it, going up faster than the body so a
		// gap opens under each, and shrinking to nothing: two to four in sight at a time)
		if (D.U() < R.JetTorn)
		{
			const double At_ = D.U(0.45, 0.8), Life = D.U(0.22, 0.36), HalfA = Wide * D.U(0.4, 0.65);
			const Vec Out_ = Around();
			const Vec Dir = (Bent(At_) + Out_ * std::tan(Radians(D.U(12.0, 30.0)))).Unit(Way);
			Tongue(Spine(At_) + Out_ * (Wide * D.U(0.5, 0.9)), Dir, Tall * D.U(1.2, 2.2), 1.5, HalfA, HalfA * 0.25, D.U(2.4, 3.4), Flame.Mix(Heart, 0.15), Ember, Life, 0.0);
		}
		// (the heart: yellow-orange over its lower part, where it burns hottest)
		Tongue(S.At, Leaning(Bent(0.1), D.U(0.0, 2.0)), Tall * D.U(1.0, 2.0), 3.0, Root * D.U(0.8, 1.0), Wide * D.U(0.45, 0.6), D.U(1.8, 2.4), Heart, Flame, D.U(0.14, 0.22), 0.05);
		if (bRoar)
		{
			// (the root: white-hot at the hatch, its brightest point, laid over the rest)
			Tongue(S.At, Leaning(Way, D.U(0.0, 2.0)), Tall * D.U(0.4, 0.8), 4.0, Root * D.U(0.9, 1.15), Root * D.U(0.7, 0.9), D.U(1.3, 1.8), R.Root, Heart, D.U(0.07, 0.11), 0.1);
			Bits.back().Heart = 1.0;
			Bits.back().Rim = 0.35;
		}
		// (the sparks: up through it and out of its top, falling back cooled)
		for (double Owed = R.JetSparks; Owed > 0.0; Owed -= 1.0)
		{
			if (Owed < 1.0 && !(D.U() < Owed))
			{
				break;
			}
			const double At_ = D.U(0.1, 0.6);
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.Born = T + D.U(0.0, R.JetEvery);
			B.From = Spine(At_) + Around() * (Wide * D.U(0.0, 0.6));
			B.Vel = Leaning(Bent(At_), D.U(0.0, 14.0)) * (D.U(9.0, 20.0) * std::sqrt(K) * (0.5 + 0.5 * Full));
			B.Gravity = 9.8;
			B.Drag = D.U(0.5, 1.4);
			B.Windage = 0.3;
			B.Life = D.U(0.4, 1.0);
			B.R0 = B.R1 = D.U(0.018, 0.034);
			B.Col = Heart;
			B.Col2 = R.EmberOld;
			B.Cool = 1.4;
			B.Alpha = 1.0;
			B.FadeIn = 0.002;
			B.Hold = 0.5;
			B.Heart = 0.9;
			B.Rim = 0.3;
			B.LeastPx = 1.5;
			B.bLight = true;
			Bits.push_back(B);
		}
		// (its smoke: black, rolling up and off down the wind. First cut: one puff from its top by the dice, a chain
		// of separate blobs. Now by the clock, a puff each jet_smoke_every_s, two in five from its sides in its upper
		// half and the rest from its top, each born wider than the way to the one before it)
		const double SmokeEvery = std::max(R.JetSmokeEvery, R.JetEvery);
		const bool bSmokeNow = bRoar ? std::floor(Beat / SmokeEvery) != std::floor((Beat - R.JetEvery) / SmokeEvery) || Beat < 0.5 * R.JetEvery : D.U() < R.JetEvery / SmokeEvery;
		if (bSmokeNow)
		{
			const bool bSide = bRoar && D.U() < 0.4;
			const double At_ = bSide ? D.U(0.4, 0.8) : (bRoar ? D.U(0.8, 1.0) : 0.85);
			Bit B;
			B.Born = T;
			B.From = (bRoar ? Spine(At_) : S.At + Way * (0.85 * Tall)) + (bSide ? Around() * (Wide * D.U(0.9, 1.3)) : (SideA * D.U(-1.0, 1.0) + SideB * D.U(-1.0, 1.0)) * Wide);
			B.Vel = (bRoar ? Bent(At_) : Way) * (D.U(2.0, 4.0) * K) + Vec(D.Bell(), D.Bell(), 0.0) * 0.4;
			B.Drag = 1.2;
			B.Rise = D.U(2.2, 3.6);
			B.Windage = 1.0;
			B.Life = bRoar ? D.U(3.5, 5.5) : D.U(3.0, 5.0);
			B.R0 = Wide * (bRoar ? D.U(1.3, 1.7) : 1.2);
			B.R1 = (bRoar ? D.U(2.0, 3.2) : D.U(1.6, 2.6)) * K;
			if (bRoar)
			{
				// (never smaller at birth than the way the last puff has risen since: one column from its first metre)
				B.R0 = std::max(B.R0, 0.75 * (3.0 * K + 2.9) * SmokeEvery);
			}
			B.Grow = 0.6;
			B.Col = R.Smoke * D.U(0.85, 1.2);
			B.Col2 = B.Col.Mix(Rgb::FromPicture(96, 92, 88), 0.5);
			B.Alpha = (bRoar ? 0.8 : 0.72) * (0.5 + 0.5 * Full);
			B.FadeIn = 0.12;
			B.Hold = bRoar ? 0.45 : 0.3;
			B.Heart = 0.7;
			B.HeartEnd = 0.35;
			B.Rough = bRoar ? 0.42 : 0.3;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			Bits.push_back(B);
		}
	}

	void Battle::Glance(double Now, const Vec& At, const Vec& Incoming, const Vec& NormalIn, double CalibreMm, const char* GroundIn)
	{
		GroundIn = GroundOn(GroundIn); // (a ground whose own switch is off is drawn as earth)
		// (glance, art director 14:39Z 20 November: "a spark spray thrown the way the shot glances, and a glowing shot
		// streaking off at a new angle". A small two-frame near-white flash by the flash rule; sparks in a 20 to 40
		// degree cone along the glance, white-yellow cooling to orange, falling, gone in 0.3 to 0.8 s; a very small
		// grey puff of paint and metal dust for half a second; the shot red-orange, tumbling, so its streak wobbles,
		// going off at a shallow angle, and kicking up a little dust where it comes down in sight. No burst, no smoke.
		// Its own dice, by its place and moment)
		const std::string P = "glance.";
		if (!IsAPlace(At) || !Has(P + "sparks.0"))
		{
			return;
		}
		auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		const double K = std::sqrt(Safe(CalibreMm, 20.0, 460.0, 75.0) / 75.0);
		// (the art director on the mend round, 16:41Z: "make sure each ricochet takes its own glance angle, kink time and
		// kink direction from its seed (never the same twice)". Its dice were thrown by the place and the millisecond
		// alone, so two shots off one spot in one frame, or one hit seen under two of the kit's seeds, flew alike. Now
		// by the kit's seed and by how many have glanced before it too)
		const uint64_t Own = (Seeds + ++Glances) * 0xD6E8FEB86659FD93ull;
		Dice D(PlaceKey(At) * 0xA0761D6478BD642Full ^ static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) ^ 0x61A9CEull ^ Own);
		const Vec Normal = WayOf(NormalIn, ZUp);
		const Vec In = WayOf(Incoming, Vec(1.0, 0.0, 0.0));
		// (it leaves along the plate the way it came, lifted off it by a shallow angle)
		const Vec Flat = In - Normal * In.Dot(Normal);
		const Vec Along = Flat.Unit(Normal.Cross(std::fabs(Normal.Z) < 0.9 ? ZUp : Vec(1.0, 0.0, 0.0)).Unit(Vec(1.0, 0.0, 0.0)));
		const double Lift = Radians(Safe(Range(P + "lift_deg", 8.0, 25.0).Pick(D), 1.0, 80.0, 15.0));
		const Vec Off = (Along * std::cos(Lift) + Normal * std::sin(Lift)).Unit(Along);
		const Vec SideA = Off.Cross(Normal).Unit(Vec(0.0, 1.0, 0.0));
		const Vec SideB = SideA.Cross(Off).Unit(Normal);
		// (glance.shot_from_mm, the production lead's FX4: a rifle's or a machine gun's round glancing off armour, a
		// calibre under this, is its flash and its sparks and nothing more: no lamp, no puff and no glowing shot followed
		// off. The ground battle sends them off a tank's plates at 8 mm, 1,541 of them in Normandy, and each with its
		// shot was about 500 pieces. A calibre that is no number is a shell's; without the key, every glance as
		// before)
		const double ShotFrom = Safe(Num(P + "shot_from_mm", 0.0), 0.0, 1000.0, 0.0);
		const bool bSmall = ShotFrom > 0.0 && CalibreMm == CalibreMm && CalibreMm < ShotFrom;
		const double FlashK = Safe(Num(P + "flash", 0.4), 0.0, 4.0, 0.4);
		if (FlashK > 0.0)
		{
			RaggedFlash(Now, At + Normal * 0.03, FlashK * K, 1.0);
		}
		const Rgb Hot = Picture(P + "spark", Rgb::FromPicture(255, 236, 170));
		const Rgb Cooled = Picture(P + "spark_old", Rgb::FromPicture(255, 140, 50));
		const double LampK = Safe(Num(P + "lamp", 0.3), 0.0, 2.0, 0.3);
		if (LampK > 0.0 && !bSmall)
		{
			Flash F;
			F.At = At + Normal * 0.2;
			F.Col = Hot;
			F.Born = Now;
			F.Life = 0.06;
			F.Strength = LampK * K;
			F.Radius = 6.0;
			Flashes.push_back(F);
		}
		// (the sparks)
		const double Cone = Radians(0.5 * Safe(Range(P + "cone_deg", 20.0, 40.0).Pick(D), 2.0, 170.0, 30.0));
		const Span SparkLife = Range(P + "spark_life_s", 0.3, 0.8);
		const Span SparkFast = Range(P + "spark_mps", 12.0, 40.0);
		const int NSparks = static_cast<int>(std::lround(Safe(Range(P + "sparks", 14.0, 26.0).Pick(D) * K, 0.0, 120.0, 20.0)));
		// (the art director, 15:40Z: "make the sparks uneven in length, so the first frame isn't an even fan". Most
		// are slow and short and a few are fast and long (uneven: the power their speeds are skewed by), a spark in
		// five leaves a frame or two late, and each slows at its own rate, so no two frames show the same fan)
		const double Uneven = Safe(Num(P + "spark_uneven", 1.0), 1.0, 6.0, 1.0);
		const bool bUneven = Uneven > 1.0;
		Dice Du(PlaceKey(At) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(static_cast<int64_t>(std::max(0.0, Now) * 1000.0)) ^ 0x5BA2C5ull ^ (Own >> 7));
		for (int I = 0; I < NSparks; ++I)
		{
			const double Out_ = Cone * std::sqrt(D.U()), Round_ = D.U(0.0, 2.0 * Pi);
			const Vec Dir = (Off * std::cos(Out_) + (SideA * std::cos(Round_) + SideB * std::sin(Round_)) * std::sin(Out_)).Unit(Off);
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.Born = Now + D.U(0.0, 0.015);
			B.From = At + Normal * 0.02;
			B.Vel = Dir * (Safe(SparkFast.Pick(D), 0.5, 200.0, 25.0) * K);
			B.Gravity = 9.8;
			B.Drag = 2.0;
			B.Life = Safe(SparkLife.Pick(D), 0.05, 5.0, 0.5);
			B.R0 = B.R1 = 0.022 * K;
			if (bUneven)
			{
				const double Quick = std::pow(Du.U(), Uneven);
				B.Vel = Dir * (Safe(SparkFast.A + (SparkFast.B - SparkFast.A) * Quick * 1.6, 0.5, 300.0, 25.0) * K);
				B.Born = Now + (Du.U() < 0.2 ? Du.U(0.03, 0.09) : Du.U(0.0, 0.02));
				B.Drag = Du.U(1.2, 4.5);
				B.Life *= 0.6 + 0.8 * Quick;
				B.R0 = B.R1 = Du.U(0.014, 0.03) * K;
			}
			B.Col = Hot;
			B.Col2 = Cooled;
			B.Cool = 1.6;
			B.Alpha = 1.0;
			B.FadeIn = 0.002;
			B.Hold = 0.45;
			B.Heart = 0.9;
			B.Rim = 0.3;
			B.LeastPx = 1.5;
			B.bLight = true;
			Bits.push_back(B);
		}
		if (bSmall)
		{
			return; // (a small arm's: its flash and its sparks, and its shot takes no place among those followed)
		}
		// (the puff: paint and metal dust, very small, gone in about half a second)
		for (int I = 0; I < 2; ++I)
		{
			Bit B;
			B.Born = Now + 0.01;
			B.From = At + Normal * (0.05 + 0.1 * I) + Off * (0.1 * I);
			B.Vel = (Off * D.U(1.0, 2.5) + Normal * D.U(0.5, 1.5)) * K;
			B.Drag = 4.0;
			B.Windage = 0.5;
			B.Life = D.U(0.3, 0.6);
			B.R0 = 0.1 * K;
			B.R1 = D.U(0.3, 0.45) * K;
			B.Col = Picture(P + "puff", Rgb::FromPicture(128, 126, 122));
			B.Col2 = B.Col;
			B.Alpha = 0.4;
			B.FadeIn = 0.02;
			B.Hold = 0.2;
			B.Heart = 0.5;
			Bits.push_back(B);
		}
		// (the shot: going off glowing, slowing hard and dropping, veering and wobbling about its way as it tumbles.
		// The art director on the first cut, 15:40Z: "a long straight laser line. Hold it to the 1/30 s streak rule,
		// about 15 m at its speed after the glance: a glowing head with a short tail, slowing and dropping. Show the
		// tumble at 90 m as a wobble or kinked path of about 1 to 2 m." So the streak is the path the shot covered
		// in the last streak_s and nothing more: a short length is left lying on the path at each step, widest and
		// brightest as it is laid, thinned and gone streak_s later, so what shows is a tail that tapers and dims
		// behind the shot, bends where the path bends and shortens as the shot slows; and at its front the shot's
		// own white-hot glow, a blunt head)
		const double Fast = Safe(Range(P + "shot_mps", 320.0, 480.0).Pick(D), 20.0, 1200.0, 400.0);
		const double For = Safe(Range(P + "shot_s", 0.8, 1.4).Pick(D), 0.0, 4.0, 1.0);
		const double Wobble = Safe(Num(P + "wobble_m", 0.7), 0.0, 3.0, 0.7);
		const double Turns = Safe(Range(P + "tumble_hz", 11.0, 16.0).Pick(D), 0.5, 40.0, 13.0) * 2.0 * Pi;
		const double PhaseA = D.U(0.0, 2.0 * Pi), PhaseB = D.U(0.0, 2.0 * Pi);
		const Rgb ShotHead = Picture(P + "shot_head", Rgb::FromPicture(255, 214, 150));
		const Rgb ShotCol = Picture(P + "shot", Rgb::FromPicture(255, 122, 44));
		const Rgb ShotOld = Picture(P + "shot_old", Rgb::FromPicture(210, 64, 22));
		const double Kd = Safe(Num(P + "shot_drag", 1.2), 0.01, 8.0, 1.2), Step = 1.0 / 120.0;
		const double StreakS = Safe(Num(P + "streak_s", 1.0 / 30.0), Step, 0.2, 1.0 / 30.0);
		const double HeadWide = Safe(Num(P + "shot_wide_m", 0.06), 0.005, 0.5, 0.06); // (half the glow's width at the head, for a 75 mm shot)
		const double Late = 0.37 * Step; // (the shot's steps begin a part of a step after the flash, so a frame that falls on the flash's own beat never falls between two of them)
		const double Veer = std::tan(Radians(Safe(Range(P + "veer_deg", 3.0, 10.0).Pick(D), 0.0, 45.0, 6.0))), VeerRound = D.U(0.0, 2.0 * Pi);
		const Vec VeerWay = SideA * std::cos(VeerRound) + SideB * std::sin(VeerRound);
		auto PathAt = [&](double T)
		{
			const double Go = (1.0 - std::exp(-Kd * T)) / Kd, Sway = Wobble * std::min(1.0, T / 0.15);
			return At + Off * (Fast * Go) + VeerWay * (Veer * Fast * Go * Clamp01(T / std::max(For, 1e-3))) + Vec(0.0, 0.0, -0.5 * 9.8 * T * T) + SideA * (Sway * std::sin(Turns * T + PhaseA)) + SideB * (Sway * std::sin(1.31 * Turns * T + PhaseB));
		};
		std::string Ground = GroundIn && *GroundIn ? GroundIn : "earth";
		if (!Has("grounds." + Ground + ".paint.soil.0"))
		{
			Ground = "earth";
		}
		// (so many shots are followed at once and no more, glance.most_shots: each is some hundreds of pieces, short as
		// their lives are. One past that has its flash, its sparks and its puff, and its shot is not drawn)
		const int MostShots = static_cast<int>(Safe(Num(P + "most_shots", 12.0), 0.0, 64.0, 12.0));
		// (the pre-check on de54166, 17:51Z: a ricochet's cost is its call, some hundreds of pieces pushed at once off a
		// deck, and a dozen in one frame took 7 ms. glance.most_a_frame: no more than that many shots begin to be
		// followed at one moment; one past it keeps its flash, its sparks and its puff. And should time go back, the
		// shots counted as in flight are forgotten: they stood up to six seconds ahead, and new ones went unfollowed)
		const int MostAFrame = static_cast<int>(Safe(Num(P + "most_a_frame", 0.0), 0.0, 64.0, 0.0));
		if (Now < GlanceAt - 0.5)
		{
			ShotEnds.clear();
		}
		if (!(std::fabs(Now - GlanceAt) <= 0.004))
		{
			GlanceAt = Now;
			GlanceSame = 0;
		}
		ShotEnds.erase(std::remove_if(ShotEnds.begin(), ShotEnds.end(), [Now](double End) { return !(End > Now) || End > Now + 10.0; }), ShotEnds.end());
		const bool bFollowed = static_cast<int>(ShotEnds.size()) < MostShots && (MostAFrame <= 0 || GlanceSame < MostAFrame);
		if (bFollowed)
		{
			ShotEnds.push_back(Now + For);
			++GlanceSame;
		}
		const int Steps = bFollowed ? static_cast<int>(std::min(480.0, std::ceil(For / Step))) : 0;
		for (int N = 0; N < Steps; ++N)
		{
			const double T = N * Step;
			const Vec Here = PathAt(T), Next = PathAt(T + Step);
			if (T > 0.05 && Here.Z <= Height(Here.X, Here.Y))
			{
				// (it comes down in sight: a little of the ground's own dust kicked up, and no more)
				const Rgb Dust = PaintOf(Ground).Dust;
				for (int I = 0; I < 3 && Ground != "water"; ++I)
				{
					Bit B;
					B.Born = Now + T;
					B.From = Vec(Here.X, Here.Y, Height(Here.X, Here.Y) + 0.1);
					B.Vel = (Off * D.U(2.0, 5.0) + ZUp * D.U(1.0, 2.5)) * K;
					B.Drag = 3.0;
					B.Windage = 0.6;
					B.Life = D.U(0.9, 1.5);
					B.R0 = 0.12 * K;
					B.R1 = D.U(0.4, 0.7) * K;
					B.Col = Dust * D.U(0.9, 1.1);
					B.Col2 = Dust;
					B.Alpha = 0.6;
					B.FadeIn = 0.02;
					B.Hold = 0.25;
					B.Heart = 0.6;
					Bits.push_back(B);
				}
				break;
			}
			const Vec Run = Next - Here;
			const double Far = Run.Length();
			if (!(Far > 1e-6))
			{
				continue;
			}
			const double Share = T / std::max(For, 1e-3);
			const double Dim = 1.0 - 0.6 * Share * Share; // (it cools as it goes: dimmer and redder to the end)
			// (the length left on the path once the shot has passed it, a little longer than the step so each overlaps
			// the next and the tail is one line; orange, thinning to a thread and gone streak_s later)
			const double Born = Now + Late + T;
			Bit B;
			B.Form = Bit::Shape::Streak;
			B.Born = Born + Step;
			B.Life = StreakS;
			B.From = Here + Run * 0.5;
			B.Along = Run * (1.0 / Far);
			B.HalfLength = (N == 0 ? 0.5 : 0.6) * Far;
			B.R0 = 0.7 * HeadWide * K;
			B.R1 = 0.15 * HeadWide * K;
			B.Grow = 0.6;
			B.Col = ShotCol.Mix(ShotOld, Share);
			B.Col2 = ShotOld;
			B.Cool = 1.0;
			B.Alpha = 0.95 * Dim;
			B.FadeIn = 0.001;
			B.Hold = 0.1;
			B.Heart = 0.95;
			B.Rim = 0.5;
			B.Inner = 0.6;
			B.LeastPx = 1.4;
			B.MostPx = 6.0;
			B.bLight = true;
			// (one of a chain: a light shorter than a frame is kept until a frame has laid it, Unseen, and here that
			// drew every step since the last frame at once, four shots abreast at 30 frames a second, and it was the
			// first cut's laser line. A step no frame saw is simply gone; the next stands for it)
			B.bShown = true;
			Bits.push_back(B);
			// (the shot itself, for this step: a white-hot length from the tail's end up to it, and its glow at the
			// front, a blunt bright head, both going with it)
			Bit Neck = B;
			Neck.Born = Born;
			Neck.Life = Step;
			Neck.From = Here - Run * 0.5;
			Neck.Vel = Run * (1.0 / Step);
			Neck.HalfLength = 0.5 * Far;
			Neck.R0 = Neck.R1 = 0.85 * HeadWide * K;
			Neck.Col = Neck.Col2 = ShotHead.Mix(ShotCol, 0.35 + 0.65 * Share);
			Neck.Alpha = Dim;
			Neck.FadeIn = 0.0;
			Neck.Hold = 1.0;
			if (N > 0)
			{
				Bits.push_back(Neck);
			}
			Bit H;
			H.Born = Born;
			H.Life = Step;
			H.From = Here;
			H.Vel = Run * (1.0 / Step);
			H.R0 = H.R1 = 1.3 * HeadWide * K;
			H.Col = H.Col2 = ShotHead.Mix(ShotCol, Share);
			H.Alpha = Dim;
			H.FadeIn = 0.0;
			H.Hold = 1.0;
			H.Heart = 1.0;
			H.Rim = 0.2;
			H.Inner = 0.5;
			H.LeastPx = 3.0;
			H.LeastKeep = 0.8;
			H.bLight = true;
			H.bShown = true;
			Bits.push_back(H);
		}
	}

	int Battle::OwnColumn(double Now, const Source& Fire, bool bLand)
	{
		// (fire.smoke_column, art director 04:06Z 20 November: a burning ship gets a column of her own, of the oil
		// column's kind but about half its width: black at the root, in lumps, leaning down the wind, rising a few
		// hundred metres before it bends flat, the bank lying on the sea beneath it. A fire from_m across or more.
		// One column to a ship: a fire lit within join_m of a fire that already has one joins it as another place the
		// column rises from, so a carrier ablaze from end to end sends up one broad column and never three. And none
		// where burning oil already stands over the fire (UnderOil): there the oil's column is hers. No such key: none)
		// (bLand, fire.land_column, the art director's ruling of 21 November, a burning tank: a black column of the same
		// kind over a fire on land under the ship's from_m, 3 m wide at the hull, 10 to 12 m at 30 m up, 25 to 35 m at
		// 100 m, rising 150 to 250 m and bending down the wind. Its own row, its own most and joins; none on the sea)
		const std::string P = bLand ? "fire.land_column." : "fire.smoke_column.";
		if (!Has(P + "column_life_s.0"))
		{
			return -1;
		}
		auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		if (bLand)
		{
			const double Wide = std::max(Fire.Across, Fire.AcrossTo);
			if (!(Wide < Safe(Num("fire.smoke_column.from_m", 8.0), 0.0, 1e6, 8.0)) || Wide < Safe(Num(P + "from_m", 1.5), 0.0, 1e6, 1.5))
			{
				return -1;
			}
			if (Fire.At.Z - Height(Fire.At.X, Fire.At.Y) > Safe(Num(P + "above_m", 6.0), 0.0, 1e6, 6.0) || Fire.bMoves)
			{
				return -1; // (high over the ground, or under way: a ship's deck)
			}
		}
		else if (Fire.Across < Safe(Num(P + "from_m", 8.0), 0.0, 1e6, 8.0))
		{
			return -1;
		}
		// (from_s: a fire that burns less than this long sends up none: a crashed aeroplane's minute and a half of fire
		// lays its bank and never a column a few hundred metres tall)
		if (Fire.Flames - Now < Safe(Num(P + "from_s", 120.0), 0.0, 1e6, 120.0))
		{
			return -1;
		}
		const double Join = Safe(Num(P + "join_m", 120.0), 0.0, 2000.0, 120.0);
		Source* Hers = nullptr;
		size_t HerHead = 0;
		double LeastSq = Join * Join;
		for (Source& S : Sources)
		{
			// (a column that has stood down under burning oil stands no more: a later fire does not join it)
			if (S.Kind != 3 || S.End <= Now || S.bStoodDown || S.bLandColumn != bLand)
			{
				continue;
			}
			for (size_t I = 0; I < S.Heads.size(); ++I)
			{
				const double Dx = S.Heads[I].X - Fire.At.X, Dy = S.Heads[I].Y - Fire.At.Y;
				if (Dx * Dx + Dy * Dy <= LeastSq)
				{
					LeastSq = Dx * Dx + Dy * Dy;
					Hers = &S;
					HerHead = I;
				}
			}
		}
		if (Hers)
		{
			// (the column rises from between the two fires as well: one broad column off the whole length of her,
			// never a chimney to a fire)
			// (from eight places at most: a ship struck again after that keeps her one column, which burns the longer
			// for it and rises from where it already does)
			Source& S = *Hers;
			if (S.Heads.size() < 7)
			{
				S.Heads.push_back((S.Heads[HerHead] + Fire.At) * 0.5);
			}
			if (S.Heads.size() < 8)
			{
				S.Heads.push_back(Fire.At);
			}
			// (and it is as broad as the fires lie apart: join_wide of her burning length, so its lumps close over the
			// gaps between her fires; never broader than a fire join_m across would send up. It stands on their middle)
			double Length = 0.0;
			Vec Middle;
			for (size_t I = 0; I < S.Heads.size(); ++I)
			{
				Middle = Middle + S.Heads[I];
				for (size_t K = I + 1; K < S.Heads.size(); ++K)
				{
					Length = std::max(Length, std::hypot(S.Heads[I].X - S.Heads[K].X, S.Heads[I].Y - S.Heads[K].Y));
				}
			}
			const double FireWide = bLand ? std::max(Fire.Across, Fire.AcrossTo) : Fire.Across;
			const double Broad = Safe(Num(P + "join_wide", 0.5), 0.0, 4.0, 0.5) * (Length + FireWide);
			S.Across = std::max(std::max(S.Across, FireWide), std::min(Broad, std::max(Join, FireWide)));
			S.At = Middle * (1.0 / static_cast<double>(S.Heads.size()));
			S.Flames = std::max(S.Flames, Fire.Flames);
			S.End = std::max(S.End, Fire.End);
			return S.Id;
		}
		// (most: so many columns of their own stand at once and never more, each some hundred puffs that are never
		// dropped when the screen is full; a fire past that lays its bank only, chosen as it is lit and never by the
		// eye. A column that burning oil stands over has stood down for good, bStoodDown, and is not counted: were it
		// to stand again when that oil went out, more than most would stand (the 3D pre-check, 05:22Z 20 November))
		const int Most = static_cast<int>(Safe(Num(P + "most", 6.0), 0.0, 64.0, 6.0));
		int Standing = 0;
		for (Source& S : Sources)
		{
			if (S.Kind != 3 || S.End <= Now || S.bStoodDown || S.bLandColumn != bLand)
			{
				continue;
			}
			if (UnderOil(S, Now))
			{
				S.bStoodDown = true;
			}
			else
			{
				++Standing;
			}
		}
		if (Standing >= Most)
		{
			return -1;
		}
		Source C;
		C.Kind = 3;
		C.bLandColumn = bLand;
		C.At = Fire.At;
		C.Across = bLand ? std::max(Fire.Across, Fire.AcrossTo) : Fire.Across;
		C.Start = Now;
		C.Flames = Fire.Flames;
		C.End = Fire.End;
		C.Next = C.NextSmoke = C.NextPall = Now;
		C.Seed = Fire.Seed ^ 0x5C01A3D5EEDull;
		C.Oil = OilRowAt(P);
		C.Id = OwnColumnId + OwnColumns;
		OwnColumns = (OwnColumns + 1) % 100000;
		C.UnderOil = Safe(Num(P + "under_oil", 0.75), 0.0, 10.0, 0.75);
		C.Heads.push_back(Fire.At);
		// (lit under burning oil: the oil's column is hers from the first, and she takes no place among the most)
		if (UnderOil(C, Now))
		{
			return -1;
		}
		Sources.push_back(C);
		return C.Id;
	}

	bool Battle::UnderOil(const Source& Column, double T) const
	{
		if (Column.UnderOil <= 0.0)
		{
			return false;
		}
		for (const Source& S : Sources)
		{
			if (S.Kind != 2 || T < S.Start || T >= S.End)
			{
				continue;
			}
			const double Reach = Column.UnderOil * S.Across;
			for (const Vec& H : Column.Heads)
			{
				const double Dx = H.X - S.At.X, Dy = H.Y - S.At.Y;
				if (Dx * Dx + Dy * Dy <= Reach * Reach)
				{
					return true;
				}
			}
		}
		return false;
	}

	Battle::BankRow Battle::BankRowOf(double Across, uint64_t Seed) const
	{
		// (fire.smoke_bank, art director 03:30Z 20 November on build 1750's Pearl Harbor: a big fire's smoke lying on the
		// sea was a smooth soft dark oval with no lumps and no edge, a stain or a hole in the water from the air. A fire
		// from_m across or more lays its smoke as a bank of the oil column's own kind of lumps, run out down the wind.
		// No such key, or a smaller fire: as before. Every dial is held to a sane span, and one that is not a number
		// takes its usual value)
		BankRow R;
		const std::string P = "fire.smoke_bank.";
		auto Safe = [](double Is, double Least, double Most, double Usual) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Usual; };
		auto One = [&](const char* Key, double Least, double Most, double Usual) { return Safe(Num(P + Key, Usual), Least, Most, Usual); };
		auto Two = [&](const char* Key, double Least, double Most, double Low, double High)
		{
			Span S = Range(P + Key, Low, High);
			S.A = Safe(S.A, Least, Most, Low);
			S.B = Safe(S.B, Least, Most, High);
			return S;
		};
		R.bOn = Has(P + "long.0") && Across >= One("from_m", 0.0, 1e6, 8.0);
		if (!R.bOn)
		{
			return R;
		}
		Dice D(Seed ^ 0xBA2C5EEDull);
		R.Wide = One("wide", 0.5, 6.0, 1.6);
		R.Long = Two("long", 1.0, 8.0, 2.2, 3.0).Pick(D);
		// (a small fire's lumps are small and go sooner: a 12 m fire's bank is laid in about half the time of a 40 m one's)
		R.Life = One("life_s", 2.0, 300.0, 40.0) * std::clamp(std::sqrt(std::min(Across, 2000.0) / 40.0), 0.5, 1.5);
		R.Short = One("short", 0.05, 1.0, 0.3);
		R.Thin = One("thin", 0.2, 8.0, 2.0);
		const Span Every = Two("every_s", 0.05, 30.0, 0.35, 0.8);
		// (never more than some three hundred lumps to a fire, whatever the recipe says)
		R.EveryA = std::max(Every.A, R.Life / 300.0);
		R.EveryB = std::max(Every.B, R.Life / 300.0);
		R.LumpR = Two("lump_r", 0.02, 3.0, 0.3, 0.5);
		R.Alpha = Two("alpha", 0.0, 1.0, 0.75, 0.95);
		R.Shade = Two("shade", 0.1, 4.0, 0.8, 1.25);
		R.Rise = Two("rise_mps", 0.0, 20.0, 0.0, 0.4);
		R.HeadRise = Two("head_rise_mps", 0.0, 20.0, 0.6, 1.4);
		R.Heads = One("heads", 0.0, 1.0, 0.33);
		R.Rough = One("rough", 0.0, 0.8, 0.35);
		R.Heart = One("heart", 0.0, 1.0, 0.85);
		R.Hold = One("hold", 0.0, 0.95, 0.5);
		R.SunSide = One("sun_side", 0.0, 10.0, 0.0);
		R.TwinPx = One("twin_px", 0.0, 4000.0, 0.0);
		R.Rest = One("rest", 0.0, 2.0, 1.0);
		R.Spread = One("spread", 0.0, 2.0, 0.35);
		R.SunYoung = One("sun_young", 0.0, 1.0, 1.0);
		R.SunFrom = One("sun_from", 0.0, 1.0, 0.0);
		R.SunBy = One("sun_by", 0.0, 1.0, 0.0);
		R.Lit = Picture(P + "lit_colour", Rgb::FromPicture(118, 106, 94));
		return R;
	}

	void Battle::SmokeBank(const Source& S, double T, bool bFlames, double After, const Rgb& Foot, double FootClear, double FootK, double Pale, const Rgb& PaleCol)
	{
		// One lump of a big fire's smoke bank. Born on the fire, it runs down the wind at the pace that takes the longest
		// lived to the bank's far end, wandering a little to either side. All of them live through the first part of
		// the bank, where they pile into one dark mass; fewer and fewer live on, so down the wind it thins to lumps
		// apart with clear sea between them. Most lie on the sea; some lift and pale as they ride clear
		const BankRow& R = S.Bank;
		Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(T * 1000.0)) * 0xA24BAED4963EE407ull + 131ull));
		const double Across = std::min(S.Across, 2000.0);
		const Vec Down = Vec(Wind.X, Wind.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		const Vec Aside(-Down.Y, Down.X, 0.0);
		const double Width = R.Wide * Across, Length = R.Long * Width;
		const double LifeMost = R.Life;
		const double Pace = Length / LifeMost;
		const bool bHead = D.U() < R.Heads;
		const double Tone = R.Shade.Pick(D);
		Bit B;
		B.Born = T;
		B.Life = std::max(1.0, LifeMost * (R.Short + (1.0 - R.Short) * std::pow(D.U(), R.Thin)));
		B.R1 = R.LumpR.Pick(D) * Across;
		B.R0 = 0.5 * B.R1;
		B.Grow = 0.5;
		B.From = S.At + Vec(D.U(-0.3, 0.3), D.U(-0.3, 0.3), bHead ? 0.5 : 0.3) * Across;
		B.Vel = Down * (Pace * D.U(0.75, 1.1)) + Aside * (D.Bell() * R.Spread * Width / LifeMost);
		B.Rise = bHead ? R.HeadRise.Pick(D) : R.Rise.Pick(D);
		B.Col = (bFlames ? Rgb::FromPicture(50, 46, 42) : Rgb::FromPicture(110, 108, 104)) * Tone;
		B.Col2 = B.Col.Mix(Rgb::FromPicture(120, 118, 114), 0.5);
		if (FootClear > 0.0 && FootK > 0.0)
		{
			B.FootK = FootK;
			B.FootClear = FootClear;
			B.FootCol = Foot * Tone; // (each lump its own shade, so the mass is lumps and never one flat dark)
		}
		B.Alpha = R.Alpha.Pick(D) * std::max(0.15, 1.0 - After);
		if (Pale > 0.0)
		{
			// (a fire being put out, fire.put_out: its lumps pale from black through grey to grey-white, lose their
			// dark foot, and thin to nothing by the fire's end)
			const Rgb Grey = Rgb::FromPicture(110, 108, 104), Dark = Rgb::FromPicture(50, 46, 42);
			B.Col = (Pale < 0.5 ? Dark.Mix(Grey, Pale * 2.0) : Grey.Mix(PaleCol, Pale * 2.0 - 1.0)) * Tone;
			B.Col2 = B.Col.Mix(PaleCol, 0.5);
			B.FootK *= 1.0 - Pale;
			B.Alpha *= (1.0 - 0.6 * Pale) * (After > 0.0 ? std::pow(std::max(0.0, 1.0 - After), 1.5) / std::max(0.15, 1.0 - After) : 1.0);
		}
		B.FadeIn = 0.3;
		B.Hold = R.Hold;
		B.Heart = R.Heart;
		B.HeartEnd = R.Heart * 0.5;
		B.Inner = 0.6;
		B.Rough = R.Rough;
		B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
		B.Turn = D.U(0.0, 2.0 * Pi);
		B.TwinPx = R.TwinPx;
		B.Rest = R.Rest;
		if (R.SunSide > 0.0)
		{
			// (lit as the oil fire's pall is: the lumps on the bank's sunward side lifted toward the lit colour, those
			// on its far side left dark, by which side of the bank's own line down the wind each lies)
			B.SunSide = R.SunSide;
			B.LitCol = R.Lit;
			// (from the fire's own height, a deck fire's too: so a deck fire's bank and a waterline fire's take the sun's
			// fringe alike, the look the art director passed in build 1758. Counted from the sea under a deck fire, its
			// bank glowed about a seventh more against a low sun (the 3D pre-check, 06:32Z 20 November))
			B.Centre = S.At;
			B.Along = Down;
			B.StretchBy = 1.0;
			B.SunYoung = R.SunYoung;
			B.SunFrom = R.SunFrom * LifeMost;
			B.SunBy = R.SunBy * LifeMost;
		}
		Bits.push_back(B);
	}

	Battle::OilRow Battle::OilRowOf(const std::string& Kind) const
	{
		return OilRowAt("oil_fires." + (Has("oil_fires." + Kind + ".column_life_s.0") ? Kind : std::string("sunk")) + ".");
	}

	Battle::OilRow Battle::OilRowAt(const std::string& P) const
	{
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
		const double LumpsAsked = Num(P + "lumps", 0.0);
		R.Lumps = LumpsAsked == LumpsAsked ? static_cast<int>(std::min(8.0, std::max(0.0, LumpsAsked))) : 0;
		R.LumpsPx = Num(P + "lumps_px", 0.0);
		R.LumpsAlpha = Num(P + "lumps_alpha", 1.0);
		R.LumpsLight = Num(P + "lumps_light", 0.6);
		R.LumpsRim = Num(P + "lumps_rim", 0.8);
		R.LumpsInner = Num(P + "lumps_inner", 0.0);
		R.LumpsR = Range(P + "lumps_r", 0.3, 0.45);
		R.LumpsOut = Range(P + "lumps_out", 0.3, 1.0);
		R.bKeep = Num(P + "keep", 1.0) > 0.5;
		R.Col = Picture(P + "colour", Rgb::FromPicture(24, 22, 21));
		R.Col2 = Picture(P + "colour_top", Rgb::FromPicture(64, 60, 57));
		R.Flame = Picture(P + "flame_colour", Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56)));
		R.SunYoung = Num(P + "sun_young", 1.0);
		R.SunFrom = Num(P + "sun_from_s", 0.0);
		R.SunBy = Num(P + "sun_by_s", 0.0);
		R.TightM = Num(P + "tight_m", 0.0);
		R.Overlap = Num(P + "overlap", 0.0);
		R.ColumnLeastPx = Num(P + "least_px", 0.0);
		R.ColumnLeastKeep = Num(P + "least_keep", 0.0);
		// (every dial held to a sane span, and one that is not a number given its usual value: no wild recipe number
		// lays a point at infinity or asks for puffs without end. A sane row is not changed by any of it)
		auto Held = [](double& V, double Least, double Most, double Usual) { V = V == V ? std::min(Most, std::max(Least, V)) : Usual; };
		Held(R.TightM, 0.0, 2000.0, 0.0);
		Held(R.Overlap, 0.0, 4.0, 0.0);
		Held(R.ColumnLeastPx, 0.0, 64.0, 0.0);
		Held(R.ColumnLeastKeep, 0.0, 1.0, 0.0);
		auto Held2 = [&Held](Span& S, double Least, double Most, double Low, double High) { Held(S.A, Least, Most, Low); Held(S.B, Least, Most, High); };
		Held2(R.Heads, 1.0, 64.0, 3.0, 5.0);
		Held(R.HeadSpread, 0.0, 5.0, 0.6);
		Held2(R.HeadR, 0.0, 5.0, 0.15, 0.3);
		Held(R.FlameUp, -1000.0, 1000.0, 0.0);
		Held(R.FlameEvery, 0.02, 60.0, 0.1);
		Held2(R.FlameR, 0.0, 200.0, 2.0, 4.5);
		Held2(R.FlameTall, 0.1, 20.0, 2.0, 3.5);
		Held2(R.FlameLife, 0.02, 30.0, 0.35, 0.7);
		Held(R.ColumnEvery, 0.1, 600.0, 1.0);
		Held2(R.ColumnLife, 1.0, 900.0, 50.0, 70.0);
		Held2(R.Rise, -50.0, 100.0, 6.0, 9.0);
		Held2(R.Burst, 0.0, 300.0, 0.0, 0.0);
		Held(R.BaseEvery, 0.0, 600.0, 0.0);
		Held2(R.BaseLife, 0.5, 600.0, 10.0, 15.0);
		Held2(R.BaseRise, -20.0, 50.0, 1.5, 3.0);
		Held2(R.BaseR, 0.0, 5.0, 0.15, 0.3);
		Held2(R.BaseAlpha, 0.0, 1.0, 0.8, 0.95);
		Held(R.BaseFlat, 0.02, 1.0, 1.0);
		Held(R.PallEvery, 0.0, 600.0, 0.0);
		Held2(R.PallLife, 1.0, 1800.0, 150.0, 240.0);
		Held2(R.PallR1, 0.0, 10.0, 1.2, 2.0);
		Held2(R.PallWindage, 0.0, 10.0, 1.3, 1.8);
		Held2(R.PallAlpha, 0.0, 1.0, 0.55, 0.75);
		Held(R.PallStretch, 1.0, 20.0, 1.0);
		Held(R.PallStretchBy, 0.0, 3600.0, 0.0);
		Held(R.PallHold, 0.0, 0.99, 0.15);
		Held2(R.PallLevel, 0.0, 5.0, 1.0, 1.0);
		Held(R.PallDrift, 0.0, 20.0, 0.0);
		Held(R.PallRough, 0.0, 0.9, 0.0);
		Held(R.PallRest, 0.0, 2.0, 0.0);
		Held(R.SlickLong, 0.0, 20.0, 0.0);
		Held(R.SlickWide, 0.0, 20.0, 0.0);
		Held(R.FlameOdds, 0.0, 1.0, 1.0);
		Held(R.FlamePoint, 0.0, 1.0, 0.9);
		Held(R.SunSide, 0.0, 20.0, 0.0);
		Held(R.SunYoung, 0.0, 1.0, 1.0);
		Held(R.SunFrom, 0.0, 3600.0, 0.0);
		Held(R.SunBy, 0.0, 3600.0, 0.0);
		Held(R.BurstDrag, 0.01, 100.0, 0.1);
		Held(R.Slow, -10.0, 10.0, 0.06);
		Held2(R.Windage, 0.0, 10.0, 1.0, 1.6);
		Held2(R.R0, 0.001, 10.0, 0.2, 0.35);
		Held2(R.R1, 0.001, 20.0, 0.8, 1.4);
		Held(R.Grow, 0.05, 8.0, 0.6);
		Held2(R.Alpha, 0.0, 1.0, 0.85, 0.95);
		Held(R.Hold, 0.0, 0.99, 0.5);
		Held(R.Heart, 0.0, 4.0, 0.7);
		Held(R.Rough, 0.0, 0.9, 0.3);
		Held(R.Cool, 0.0, 4.0, 1.0);
		Held(R.Lamp, 0.0, 100.0, 0.25);
		Held(R.LampReach, 0.0, 100.0, 2.0);
		Held(R.TwinPx, 0.0, 100000.0, 0.0);
		Held(R.LumpsPx, 0.0, 100000.0, 0.0);
		Held(R.LumpsAlpha, 0.0, 1.0, 1.0);
		Held(R.LumpsLight, 0.0, 1.0, 0.6);
		Held(R.LumpsRim, 0.0, 1.0, 0.8);
		Held(R.LumpsInner, 0.0, 0.95, 0.0);
		Held2(R.LumpsR, 0.05, 1.0, 0.3, 0.45);
		Held2(R.LumpsOut, 0.0, 1.0, 0.3, 1.0);
		// (fx-air r2, a ship burning afloat: the column pales and thins as it climbs; see Bit::PaleUpM. None asked: as before)
		R.PaleUpM = SafeNum(Num(P + "pale_up_m", 0.0), 0.0, 20000.0, 0.0);
		R.PaleUpMost = SafeNum(Num(P + "pale_up_most", 0.0), 0.0, 1.0, 0.0);
		R.PaleUpPow = SafeNum(Num(P + "pale_up_pow", 1.0), 0.1, 8.0, 1.0);
		R.ThinUp = SafeNum(Num(P + "thin_up", 0.0), 0.0, 1.0, 0.0);
		auto Sane = [](const Rgb& C, const Rgb& Usual) { return C.R == C.R && C.G == C.G && C.B == C.B ? C : Usual; };
		R.PaleUpCol = Sane(Picture(P + "pale_up_colour", R.Col2), R.Col2);
		R.PaleUpLit = Sane(Picture(P + "pale_up_lit", R.Lit), R.Lit);
		// (and never more than some four hundred puffs of a kind alive to a fire, however often the recipe asks)
		R.ColumnEvery = std::max(R.ColumnEvery, std::max(R.ColumnLife.A, R.ColumnLife.B) / 400.0);
		if (R.BaseEvery > 0.0)
		{
			R.BaseEvery = std::max(R.BaseEvery, std::max(R.BaseLife.A, R.BaseLife.B) / 400.0);
		}
		if (R.PallEvery > 0.0)
		{
			R.PallEvery = std::max(R.PallEvery, std::max(R.PallLife.A, R.PallLife.B) / 400.0);
		}
		R.FlameEvery = std::max(R.FlameEvery, std::max(R.FlameLife.A, R.FlameLife.B) * std::max(R.Heads.A, R.Heads.B) / 600.0);
		return R;
	}

	void Battle::OilFire(double Now, int Id, const Vec& AtIn, double Reach, const char* Kind, double Until, double Since)
	{
		OilStop(Now, Id); // (one fire to an Id: a second call starts it afresh)
		if (!IsAPlace(AtIn))
		{
			return;
		}
		Source S;
		S.Kind = 2;
		S.Id = Id;
		S.At = Vec(AtIn.X, AtIn.Y, std::max(AtIn.Z, Height(AtIn.X, AtIn.Y))); // (on the sea, or on a burning ship's deck if given)
		S.Across = std::min(5000.0, std::max(5.0, Reach)); // (a reach that is not a number counts as the least, and an endless one is held)
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

	void Battle::Screen(double Now, const Vec& At, double Across, double Seconds, const char* Kind, int Id)
	{
		// A smoke round (the drill book's screens, 22 November). The 2-inch and 60 mm mortars' bombs gave a screen for
		// about a minute, the 81 mm's about a minute and a half, a field gun's base-ejection shell two to three minutes;
		// the caller says how long and how wide. A phosphorus round bursts hard and white and throws burning streamers;
		// a burning bomb or canister lies on the ground and gushes. Either way the screen is white, low and drifting,
		// never a crater and never black.
		if (!IsAPlace(At))
		{
			return;
		}
		const std::string P = "smoke_screen.";
		Source S;
		S.At = At;
		S.Kind = 6;
		S.Id = Id >= 0 ? Id : -1;
		S.Start = Now;
		S.Next = Now;
		S.NextSmoke = Now + (Kind && std::string(Kind) == "wp" ? 0.4 : 0.0);
		S.Across = Across == Across ? std::clamp(Across, 2.0, 200.0) : 20.0;
		S.Flames = Now + (Seconds == Seconds ? std::clamp(Seconds, 1.0, 1200.0) : 60.0);
		S.End = S.Flames + std::max(1.0, Range(P + "life_s", 25.0, 45.0).B);
		S.Seed = ++Seeds * 0x2545F4914F6CDD1Dull ^ PlaceKey(At);
		S.bPhos = Kind && std::string(Kind) == "wp";
		Dice D(S.Seed ^ 0x5C2EE7ull);
		if (Kind && std::string(Kind) == "grenade")
		{
			// (a tank commander's smoke grenade: one canister, its own dice; its lumps live no longer than its plume needs)
			const std::string G = P + "grenade.";
			Dice Dg(S.Seed ^ 0x6E7ADEull);
			S.bGrenade = true;
			S.GrenadeStart = std::min(S.Across, std::max(0.5, Range(G + "start_m", 3.0, 5.0).Pick(Dg)));
			S.GrenadePlume = std::max(5.0, Range(G + "plume_m", 85.0, 100.0).Pick(Dg));
			S.End = S.Flames + std::max(1.0, Num(G + "most_life_s", 45.0) * 1.15);
			// (its own clocks: the jet at once, the billow, the plume and its shadow a moment later; GrenadeLumps)
			S.NextJet = Now;
			S.NextSeat = Now + std::max(0.0, Num(G + "billow_from_s", 0.8));
			S.NextSmoke = Now + std::max(0.0, Num(G + "plume_from_s", 1.0));
			S.NextShade = Now + std::max(0.0, Num(G + "shadow.from_s", 1.5));
		}
		const Rgb White = Picture(P + "colour", Rgb::FromPicture(232, 233, 230));
		const Rgb Shadow = Picture(P + "shadow_colour", Rgb::FromPicture(170, 173, 176));
		const double Ground = Height(At.X, At.Y);
		const Vec Foot(At.X, At.Y, (Ground == Ground ? Ground : At.Z) + 0.2);
		if (!S.bPhos)
		{
			// (a burning bomb or a canister: it lies where it fell, and a base-ejection shell's canisters a few metres
			// apart along its flight; the wall is laid from them)
			S.Heads.push_back(Foot);
			const int More = S.bGrenade ? 0 : D.Whole(0, 2); // (a grenade: the one canister)
			for (int I = 0; I < More; ++I)
			{
				const double A = D.U(0.0, 2.0 * Pi), R = D.U(2.0, 5.0);
				const Vec Q(At.X + std::cos(A) * R, At.Y + std::sin(A) * R, 0.0);
				const double Z = Height(Q.X, Q.Y);
				S.Heads.push_back(Vec(Q.X, Q.Y, (Z == Z ? Z : Foot.Z) + 0.2));
			}
			Sources.push_back(S);
			return;
		}
		// The phosphorus burst: burning streamers thrown out and up in a dome, each a white-hot point trailing its own
		// white smoke, that curls over as it falls; the trails hang, swell and drift. And at the burst's heart a ball of
		// dense white, rising a little as it swells.
		const Span Streamers = Range(P + "streamers", 10.0, 16.0);
		const double Reach = S.Across * Num(P + "streamer_reach", 0.8);
		const double TrailEvery = std::max(0.02, Num(P + "trail_every_s", 0.07));
		const Span TrailLife = Range(P + "trail_life_s", 8.0, 14.0);
		const Span TrailR = Range(P + "trail_r_m", 1.0, 1.8);
		const Rgb Hot = Picture(P + "streamer_colour", Rgb::FromPicture(255, 250, 236));
		const int N = std::clamp(static_cast<int>(std::lround(Streamers.Pick(D))), 3, 40);
		for (int I = 0; I < N; ++I)
		{
			const double A = (2.0 * Pi * (I + D.U(0.0, 0.7))) / N; // (round the dome, a little uneven)
			const double E = D.U(0.55, 1.25); // (thrown 30 to 70 degrees up)
			const double Far = Reach * D.U(0.45, 1.1);
			const double V = std::sqrt(Far * 9.8 / std::max(0.25, std::sin(2.0 * E)));
			const Vec Vel(std::cos(A) * std::cos(E) * V, std::sin(A) * std::cos(E) * V, std::sin(E) * V);
			const double Flight = std::min(3.0, 2.0 * V * std::sin(E) / 9.8);
			Bit H;
			H.Form = Bit::Shape::Streak;
			H.Born = Now;
			H.From = Foot + Vec(0.0, 0.0, 0.2);
			H.Vel = Vel;
			H.Gravity = 9.8;
			H.Life = Flight;
			H.R0 = H.R1 = 0.06;
			H.Col = Hot;
			H.Col2 = Hot * 0.85;
			H.Alpha = 1.0;
			H.FadeIn = 0.002;
			H.Hold = 0.85;
			H.Heart = 0.9;
			H.Rim = 0.4;
			H.LeastPx = 1.5;
			H.bLight = true;
			Bits.push_back(H);
			for (double T = 0.03; T < Flight; T += TrailEvery * D.U(0.8, 1.2))
			{
				Bit B;
				B.Born = Now + T;
				B.From = H.From + Vel * T + Vec(0.0, 0.0, -4.9 * T * T);
				B.Life = TrailLife.Pick(D) * (0.6 + 0.4 * T / Flight);
				B.R1 = TrailR.Pick(D) * (0.7 + 0.6 * T / Flight);
				B.R0 = 0.15 * B.R1;
				B.Grow = 0.3;
				B.Windage = D.U(0.8, 1.0);
				B.Rise = D.U(0.1, 0.4);
				const double Tone = D.U(0.94, 1.03);
				B.Col = White * Tone;
				B.Col2 = White.Mix(Shadow, 0.3) * Tone;
				B.Alpha = D.U(0.7, 0.9);
				B.FadeIn = 0.04;
				B.Hold = 0.35;
				B.Heart = 0.65;
				B.HeartEnd = 0.3;
				B.Inner = 0.55;
				B.Rough = 0.3;
				B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				B.Turn = D.U(0.0, 2.0 * Pi);
				Bits.push_back(B);
			}
			const Vec Down(Foot.X + Vel.X * Flight, Foot.Y + Vel.Y * Flight, 0.0);
			const double Z = Height(Down.X, Down.Y);
			S.Heads.push_back(Vec(Down.X, Down.Y, (Z == Z ? Z : Foot.Z) + 0.2));
		}
		const double Ball = Num(P + "burst_r_m", 0.22) * S.Across;
		for (int I = 0; I < 9; ++I)
		{
			Bit B;
			B.Born = Now + D.U(0.0, 0.06);
			const Vec Way = Vec(D.Bell(), D.Bell(), std::fabs(D.Bell()) + 0.4).Unit();
			B.From = Foot + Vec(0.0, 0.0, 0.6) + Way * D.U(0.2, 1.0);
			B.Vel = Way * D.U(4.0, 9.0);
			B.Drag = D.U(2.0, 3.0);
			B.Rise = D.U(0.4, 1.0);
			B.Windage = D.U(0.8, 1.0);
			B.Life = D.U(16.0, 26.0);
			B.R1 = Ball * D.U(0.8, 1.2);
			B.R0 = 0.25 * B.R1;
			B.Grow = 0.2;
			const double Tone = D.U(0.96, 1.04);
			B.Col = White * Tone;
			B.Col2 = White.Mix(Shadow, 0.4) * Tone;
			B.Alpha = D.U(0.8, 0.95);
			B.FadeIn = 0.03;
			B.Hold = 0.4;
			B.Heart = 0.75;
			B.HeartEnd = 0.35;
			B.Inner = 0.6;
			B.Rough = 0.3;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			Bits.push_back(B);
		}
		Sources.push_back(S);
	}

	void Battle::ScreenLumps(Source& S, double Now)
	{
		if (S.bGrenade)
		{
			GrenadeLumps(S, Now); // (a tank commander's smoke grenade: its own jet, billow, plume and shadow)
			return;
		}
		// The wall: a lump every every_s from one of its seats, laid low, swelling as it goes down the wind, and some
		// spread across it, so the screen stands as one through its middle with rags at its edges. It thickens over its
		// first build_s (a phosphorus round's at once) and thins over the last of its time from thin_from.
		const std::string P = "smoke_screen.";
		const double Every = std::max(0.05, Num(P + "every_s", 0.3));
		const Span Life = Range(P + "life_s", 25.0, 45.0);
		const Span LumpR = Range(P + "lump_r", 0.24, 0.36);
		const Span Alpha = Range(P + "alpha", 0.55, 0.8);
		const Span Rise = Range(P + "rise_mps", 0.05, 0.3);
		const double Build = std::max(0.1, Num(P + (S.bPhos ? "build_wp_s" : "build_s"), S.bPhos ? 1.5 : 3.0));
		const double Fade = std::clamp(Num(P + "thin_from", 0.7), 0.0, 0.99);
		const double Spread = Num(P + "spread", 0.25);
		const Rgb White = Picture(P + "colour", Rgb::FromPicture(232, 233, 230));
		const Rgb Shadow = Picture(P + "shadow_colour", Rgb::FromPicture(170, 173, 176));
		const int Lumps = static_cast<int>(std::clamp(Num(P + "lumps", 3.0), 0.0, 8.0));
		const double LumpsPx = Num(P + "lumps_px", 120.0);
		const Vec Down = Vec(Wind.X, Wind.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		const Vec Aside(-Down.Y, Down.X, 0.0);
		const double Breeze = std::hypot(Wind.X, Wind.Y);
		const double Creep = Num(P + "creep_mps", 0.6); // (on a still day a screen still creeps)
		while (S.NextSmoke <= Now && S.NextSmoke < S.Flames)
		{
			const double T = S.NextSmoke;
			Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(T * 1000.0)) * 0x9E3779B97F4A7C15ull + 0x5C2Eull));
			const double Since = T - S.Start;
			const double X = Since / std::max(S.Flames - S.Start, 0.1);
			const double Full = Clamp01((Since + 0.3) / Build) * (X < Fade ? 1.0 : std::max(0.25, 1.0 - 0.75 * (X - Fade) / (1.0 - Fade)));
			const double Width = S.Across;
			const Vec Seat = S.Heads.empty() ? S.At : S.Heads[static_cast<size_t>(D.Whole(0, static_cast<int>(S.Heads.size()) - 1))];
			Bit B;
			B.Born = T;
			B.Life = Life.Pick(D);
			B.R1 = LumpR.Pick(D) * S.Across * (0.7 + 0.3 * Full);
			B.R0 = std::max(0.6, 0.35 * B.R1);
			B.Grow = 0.35;
			const Vec Off = Vec(D.Bell(), D.Bell(), 0.0) * (0.06 * Width);
			const double Z = Height(Seat.X + Off.X, Seat.Y + Off.Y);
			B.From = Vec(Seat.X + Off.X, Seat.Y + Off.Y, (Z == Z ? Z : Seat.Z) + 0.6 * B.R0 + D.U(0.0, 0.6));
			B.Windage = D.U(0.8, 1.0);
			B.Vel = Aside * (D.Bell() * Spread * Width / B.Life) + (Breeze < Creep ? Down * (Creep - Breeze) : Vec());
			B.Rise = Rise.Pick(D);
			const double Tone = D.U(0.93, 1.03);
			B.Col = White.Mix(Shadow, 0.0) * Tone;
			B.Col2 = White.Mix(Shadow, 0.35) * Tone;
			B.Alpha = Alpha.Pick(D) * (0.5 + 0.5 * Full);
			B.FadeIn = 0.6;
			B.Hold = 0.4;
			B.Heart = 0.7;
			B.HeartEnd = 0.3;
			B.Inner = 0.55;
			B.Rough = 0.3;
			B.Lumps = Lumps;
			B.LumpsPx = LumpsPx;
			B.LumpsAlpha = 0.85;
			B.LumpsLight = 0.6;
			B.LumpsRim = 0.8;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			Bits.push_back(B);
			S.NextSmoke += Every / std::max(0.35, Full);
		}
	}

	void Battle::GrenadeLumps(Source& S, double Now)
	{
		// A tank commander's smoke grenade, smoke_screen.grenade (the art director's notes of 24 November 02:18Z). Four
		// kinds of piece, each by its own clock and its own dice:
		// - its jet, over its first jet_s: small lumps hissing out of the canister down the wind, slowed by the air within
		//   jet_reach_m, so the first thing seen is one short jet off the canister, never a puff and a dot;
		// - its billow, from billow_from_s: big lumps rolling about the canister and carried off slowly (billow_windage),
		//   so a dense rolling mass stands where it gushes;
		// - its plume, from plume_from_s: lumps carried down the wind, climbing rise_per_m metres for each metre they go
		//   and widening tail_grow times as they go, each as thick as laid for its first solid_m metres and then thinner
		//   by e for every fade_m metres more (as the power fade_pow), so the plume is solid near the canister and thins
		//   steadily down the wind, about a fifth as thick 40 to 60 m down, and gone by plume_m; wisp_share of them torn
		//   wisps off its top edge (wisp_up) and sides (wisp_out), thinner, rougher, quicker, climbing wisp_climb faster;
		// - its shadow, from shadow.from_s: soft dark marks on the ground under its dense part, laid away from the sun,
		//   carried with it for shadow.reach_m.
		// Grey-white: lit toward the sun shade_k, its underside and lee going lee_k of the way to lee_colour. It gushes
		// at once (build_s), start_m across by 2 s and the caller's width at the canister by swell_s, and thins over its
		// last thin_last_s. The pieces of a wall (ScreenLumps) are not touched by any of this.
		const std::string P = "smoke_screen.";
		const std::string G = P + "grenade.";
		const Vec Down = Vec(Wind.X, Wind.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		const Vec Aside(-Down.Y, Down.X, 0.0);
		const double Breeze = std::hypot(Wind.X, Wind.Y);
		const double Creep = Num(P + "creep_mps", 0.6);
		const Vec Crept = Breeze < Creep ? Down * (Creep - Breeze) : Vec(); // (on a still day it still creeps)
		const Vec Seat = S.Heads.empty() ? S.At : S.Heads.front();
		const double Laid = std::max(S.Flames - S.Start, 0.1);
		const double Build = std::max(0.1, Num(G + "build_s", 1.0));
		const double Fade = std::clamp(1.0 - Num(G + "thin_last_s", 15.0) / Laid, 0.3, 0.97);
		const double Swell = std::max(2.5, Num(G + "swell_s", 10.0));
		const Rgb White = Picture(P + "colour", Rgb::FromPicture(232, 233, 230));
		const Rgb Shadow = Picture(P + "shadow_colour", Rgb::FromPicture(170, 173, 176));
		const double Grey = Clamp01(Num(G + "grey", 0.4));
		const Rgb Lee = Picture(G + "lee_colour", Rgb::FromPicture(140, 146, 156));
		const double LeeK = Clamp01(Num(G + "lee_k", 0.95));
		const double ShadeK = std::clamp(Num(G + "shade_k", 1.7), 0.0, 3.0);
		const int Lumps = static_cast<int>(std::clamp(Num(P + "lumps", 3.0), 0.0, 8.0));
		const double LumpsPx = Num(P + "lumps_px", 120.0);
		const double Rough = std::clamp(Num(G + "rough", 0.35), 0.0, 0.9);
		const double Rest = std::clamp(Num(G + "rest", 0.85), 0.0, 1.5);
		// (how fast its plume runs down the wind, metres a second, a lump's windage taken as 0.92: by this a length down the
		// wind is a time, whatever the wind)
		const double Go = std::max(0.92 * Breeze, Creep);
		// (its share of its fullness at T: gushing at once, thinning over its last seconds; and its width at the canister)
		const auto FullAt = [&](double Since)
		{
			const double X = Since / Laid;
			return Clamp01((Since + 0.3) / Build) * (X < Fade ? 1.0 : std::max(0.25, 1.0 - 0.75 * (X - Fade) / (1.0 - Fade)));
		};
		const auto WidthAt = [&](double Since)
		{
			const double Sw = Clamp01((Since - 2.0) / (Swell - 2.0));
			return Since < 2.0 ? S.GrenadeStart * (0.3 + 0.7 * Since / 2.0) : S.GrenadeStart + (S.Across - S.GrenadeStart) * Sw * Sw * (3.0 - 2.0 * Sw);
		};
		const auto DiceAt = [&](double T, uint64_t Salt)
		{
			return Dice(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(T * 1000.0)) * 0x9E3779B97F4A7C15ull + Salt));
		};
		const auto Ground = [&](double X, double Y)
		{
			const double Z = Height(X, Y);
			return Z == Z ? Z : Seat.Z - 0.2;
		};
		// (what every lump of smoke shares: its colours, light and shade, its lumpy face)
		const auto Smoke = [&](Bit& B, Dice& D, double Full)
		{
			const double Tone = D.U(0.95, 1.03);
			B.Col = White.Mix(Shadow, Grey) * Tone;
			B.Col2 = White.Mix(Shadow, 0.3 + Grey) * Tone;
			B.ShadeK = ShadeK;
			B.ShadowCol = Lee;
			B.ShadowK = LeeK;
			B.Heart = 0.75;
			B.HeartEnd = 0.45;
			B.Inner = 0.55;
			B.Rough = Rough;
			B.Rest = Rest;
			B.Lumps = Lumps;
			B.LumpsPx = LumpsPx;
			B.LumpsAlpha = 0.85;
			B.LumpsLight = 0.6;
			B.LumpsRim = 0.8;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.Alpha *= 0.5 + 0.5 * Full;
		};

		// The jet: one short jet off the canister, down the wind
		{
			const double JetFor = std::max(0.0, Num(G + "jet_s", 2.0));
			const double Every = std::max(0.02, Num(G + "jet_every_s", 0.05));
			const Span Speed = Range(G + "jet_mps", 1.6, 2.4), Reach = Range(G + "jet_reach_m", 0.5, 1.0);
			const Span R0 = Range(G + "jet_r_m", 0.12, 0.2), R1 = Range(G + "jet_grown_m", 0.6, 0.9);
			const Span Life = Range(G + "jet_life_s", 1.5, 2.0), Alpha = Range(G + "jet_alpha", 0.6, 0.8);
			const double Windage = Clamp01(Num(G + "jet_windage", 0.35));
			while (S.NextJet <= Now && S.NextJet < std::min(S.Flames, S.Start + JetFor))
			{
				const double T = S.NextJet;
				Dice D = DiceAt(T, 0x6A37ull);
				Bit B;
				B.Born = T;
				B.Life = Life.Pick(D);
				B.R0 = R0.Pick(D);
				B.R1 = R1.Pick(D);
				B.Grow = 0.5;
				const double V = Speed.Pick(D);
				B.Vel = Down * V + Aside * (0.15 * V * D.U(-1.0, 1.0)) + Crept;
				B.Drag = V / std::max(0.1, Reach.Pick(D)); // (it goes Vel over Drag metres before the air has stopped it)
				B.Windage = Windage;
				B.From = Vec(Seat.X, Seat.Y, Ground(Seat.X, Seat.Y) + 0.15 + B.R0);
				B.Rise = 0.25;
				B.Alpha = Alpha.Pick(D);
				B.FadeIn = 0.05;
				B.Hold = 0.5;
				Smoke(B, D, 1.0);
				B.Lumps = 0; // (too small for lumps of its own)
				B.Rest = 0.0;
				Bits.push_back(B);
				S.NextJet += Every;
			}
		}

		// The billow: big lumps rolling about the canister, carried off slowly
		{
			const double Every = std::max(0.05, Num(G + "billow_every_s", 0.3));
			const Span R = Range(G + "billow_r", 0.18, 0.24), Life = Range(G + "billow_life_s", 3.0, 5.0);
			const Span Windage = Range(G + "billow_windage", 0.25, 0.45), Alpha = Range(G + "billow_alpha", 0.7, 0.9);
			const Span Roll = Range(G + "billow_roll", 0.15, 0.35);
			const double Grow = std::max(1.0, Num(G + "billow_grow", 1.3));
			while (S.NextSeat <= Now && S.NextSeat < S.Flames)
			{
				const double T = S.NextSeat;
				Dice D = DiceAt(T, 0xB111ull);
				const double Since = T - S.Start, Full = FullAt(Since), Width = WidthAt(Since);
				Bit B;
				B.Born = T;
				B.Life = Life.Pick(D);
				B.R0 = std::max(0.3, R.Pick(D) * Width * (0.6 + 0.4 * Full));
				B.R1 = Grow * B.R0;
				B.Grow = 0.5;
				const double Ba = D.Bell(), Bb = D.Bell();
				const Vec Off = Aside * (Ba * 0.15 * Width) + Down * (Bb * 0.08 * Width);
				B.From = Vec(Seat.X + Off.X, Seat.Y + Off.Y, Ground(Seat.X + Off.X, Seat.Y + Off.Y) + 0.5 * B.R0 + D.U(0.0, 0.3));
				B.Windage = Windage.Pick(D);
				B.Vel = Crept;
				B.Rise = 0.15;
				B.Alpha = Alpha.Pick(D);
				B.FadeIn = 0.3;
				B.Hold = 0.45;
				B.TurnRate = Roll.Pick(D) * (D.U() < 0.5 ? -1.0 : 1.0); // (its lumps roll over)
				Smoke(B, D, Full);
				Bits.push_back(B);
				S.NextSeat += Every / std::max(0.35, Full);
			}
		}

		// The plume: down the wind, climbing and widening, thinning steadily as it goes; some of it torn wisps
		{
			const double Every = std::max(0.05, Num(G + "every_s", 0.26));
			const Span LumpR = Range(P + "lump_r", 0.24, 0.36);
			const double LumpK = Num(G + "lump_k", 0.8), Across = Num(G + "across_k", 0.2), Spread = Num(G + "spread", 0.3);
			const double TailGrow = std::max(1.0, Num(G + "tail_grow", 2.0));
			const Span Climb = Range(G + "rise_per_m", 0.12, 0.2), Alpha = Range(G + "alpha", 0.6, 0.8);
			const double FadeM = std::max(1.0, Num(G + "fade_m", 12.0)), FadePow = std::clamp(Num(G + "fade_pow", 1.0), 0.5, 4.0);
			const double SolidM = std::max(0.0, Num(G + "solid_m", 9.0));
			const double Hold = Clamp01(Num(G + "hold", 0.85));
			const double Least = std::max(1.0, Num(G + "least_life_s", 8.0)), Most = std::max(Least, Num(G + "most_life_s", 45.0));
			const double WispShare = Clamp01(Num(G + "wisp_share", 0.3));
			const Span WispOut = Range(G + "wisp_out", 0.2, 0.5), WispWindage = Range(G + "wisp_windage", 1.05, 1.2);
			const double WispR = Num(G + "wisp_r", 0.4), WispAlpha = Num(G + "wisp_alpha", 0.6), WispLife = Num(G + "wisp_life", 0.6);
			const double WispRough = std::clamp(Num(G + "wisp_rough", 0.5), 0.0, 0.9);
			const Span WispUp = Range(G + "wisp_up", 0.8, 1.5); // (how high it is laid, in the plume's lump's radius)
			const double WispClimb = std::max(0.0, Num(G + "wisp_climb", 1.4)); // (and how much faster it climbs)
			const double Life = std::clamp(S.GrenadePlume / Go, Least, Most);
			while (S.NextSmoke <= Now && S.NextSmoke < S.Flames)
			{
				const double T = S.NextSmoke;
				Dice D = DiceAt(T, 0x5C2Eull);
				const double Since = T - S.Start, Full = FullAt(Since), Width = WidthAt(Since);
				// (each step lays one lump of the plume's body, and wisp_share of the steps a torn wisp as well, so the body
				// never has a gap where wisps happened to fall together)
				const auto Lump = [&](bool bWisp)
				{
					Bit B;
					B.Born = T;
					B.Life = Life * D.U(0.85, 1.15) * (bWisp ? WispLife : 1.0);
					B.R0 = std::max(0.3, LumpR.Pick(D) * LumpK * Width * (0.6 + 0.4 * Full)) * (bWisp ? WispR : 1.0);
					B.R1 = TailGrow * B.R0;
					B.Grow = 0.6;
					const double Ba = D.Bell(), Bb = D.Bell();
					const double Side = D.U() < 0.5 ? -1.0 : 1.0;
					const Vec Off = bWisp ? Aside * (Side * WispOut.Pick(D) * Width) + Down * (Bb * 0.06 * Width)
						: Aside * (Ba * Across * Width) + Down * (Bb * 0.06 * Width);
					// (a wisp off its top edge, where the eye sees it torn off against the sky)
					const double Up = bWisp ? D.U(WispUp.A, WispUp.B) * B.R0 / std::max(WispR, 0.1) : D.U(0.0, 0.4);
					B.From = Vec(Seat.X + Off.X, Seat.Y + Off.Y, Ground(Seat.X + Off.X, Seat.Y + Off.Y) + 0.5 * B.R0 + Up);
					B.Windage = bWisp ? WispWindage.Pick(D) : D.U(0.85, 1.0);
					B.Vel = Aside * (D.Bell() * Spread * Width / B.Life) + Crept;
					const double Runs = B.Windage * Breeze + std::max(0.0, Creep - Breeze); // (its own pace down the wind)
					B.Rise = std::clamp(Runs * Climb.Pick(D) * (bWisp ? WispClimb : 1.0), 0.03, 1.0);
					B.Alpha = Alpha.Pick(D) * (bWisp ? WispAlpha : 1.0);
					B.AgeFade = FadeM / std::max(Runs, 0.1);
					B.AgeFadePow = FadePow;
					B.AgeFadeFrom = SolidM / std::max(Runs, 0.1);
					B.FadeIn = 0.3;
					B.Hold = Hold;
					B.TurnRate = D.U(-0.08, 0.08);
					Smoke(B, D, Full);
					if (bWisp)
					{
						B.Rough = WispRough;
						B.Lumps = 0;
						B.Rest = 0.0;
						B.Stretch = 1.6; // (torn out along the wind)
						B.Along = Down;
					}
					Bits.push_back(B);
				};
				Lump(false);
				if (D.U() < WispShare)
				{
					Lump(true);
				}
				S.NextSmoke += Every / std::max(0.35, Full);
			}
		}

		// Its shadow: soft dark marks on the ground under its dense part, away from the sun
		{
			const std::string Sh = G + "shadow.";
			const double Every = std::max(0.1, Num(Sh + "every_s", 0.5));
			const double Reach = std::max(1.0, Num(Sh + "reach_m", 18.0)), Over = std::max(0.0, Num(Sh + "over_m", 2.5));
			const Span R = Range(Sh + "r", 0.3, 0.4), Alpha = Range(Sh + "alpha", 0.12, 0.16);
			const Rgb Dark = Picture(Sh + "colour", Rgb::FromPicture(30, 32, 34));
			// (the sun's last way: its shadow lies Over metres' height of smoke away from it, no further than three times that)
			const Vec Sun = LastSun.Unit(ZUp);
			const Vec Away = Sun.Z > 0.1 ? Vec(-Sun.X, -Sun.Y, 0.0) * (Over / Sun.Z) : Vec();
			const Vec Cast = Away.Length() > 3.0 * Over ? Away.Unit() * (3.0 * Over) : Away;
			while (S.NextShade <= Now && S.NextShade < S.Flames)
			{
				const double T = S.NextShade;
				Dice D = DiceAt(T, 0x54ADull);
				const double Since = T - S.Start, Full = FullAt(Since), Width = WidthAt(Since);
				Bit B;
				B.Form = Bit::Shape::Flat;
				B.Born = T;
				B.Life = std::clamp(Reach / Go, 3.0, 15.0);
				B.R0 = R.Pick(D) * Width;
				B.R1 = 1.6 * B.R0;
				B.Grow = 0.6;
				B.Stretch = 1.3; // (drawn out down the wind)
				B.Spin = std::atan2(Down.Y, Down.X);
				const double Ba = D.Bell();
				const Vec At = Seat + Cast + Aside * (Ba * 0.1 * Width) + Down * (0.15 * Width);
				B.From = Vec(At.X, At.Y, Ground(At.X, At.Y));
				B.bHug = true;
				B.Windage = 0.92;
				B.Vel = Crept;
				B.Col = B.Col2 = Dark;
				B.Alpha = Alpha.Pick(D) * Full;
				B.FadeIn = 1.0;
				B.Hold = 0.4;
				B.Heart = 0.85;
				B.Inner = 0.5;
				B.Rim = 0.0; // (soft from its middle out: a shadow has no rim)
				B.ShadeK = 0.0;
				B.Nearer = -30.0; // (drawn first, under the smoke)
				Bits.push_back(B);
				S.NextShade += Every;
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The sea battle's own: a shell's column by its calibre and its ship's dye, and a smoke screen laid by a ship
	// under way (release 2, the sea scene's asks of 22 November)

	void Battle::SeaSplash(double Now, const Vec& AtIn, double CalibreMm, const char* DyeIn)
	{
		// A shell falling in the sea, by its calibre: the art director's columns by calibre (sea-guns-ruling.md, 06:12Z
		// 21 November: a heavy shell's 50 to 70 m, full height in 2 to 2.5 s, standing 3 to 4 s then falling over 4 to 6
		// s, a foam patch 30 to 40 m that lasts 20 to 30 s; a medium's and a light's smaller and quicker), and the dye
		// its ship loaded so she could tell her own fall of shot (the historian, 22 November: Nagato brilliant pink,
		// Kongo blood red reading red to purple by the light, Haruna greenish yellow, Yamato none). The passed column's
		// build is kept (Spout): only how long it stands, how wide its foam, its colour and its floor far off are the
		// calibre's and the dye's. A hit whose place or moment is no number is no hit.
		const auto Sound = [](double V, double Most) { return V == V && V >= -Most && V <= Most; };
		if (!Sound(Now, 1.0e9) || !Sound(AtIn.X, 1.0e7) || !Sound(AtIn.Y, 1.0e7) || !Sound(AtIn.Z, 1.0e7))
		{
			return;
		}
		const double Mm = SafeNum(CalibreMm, 20.0, 600.0, 150.0);
		const std::string P = "sea_splash.";
		if (!Has(P + "calibre_mm.0") || !Has("grounds.water.spout.rise_s"))
		{
			// (a recipe without the row: the size row the scene passed before for that calibre, as it was drawn)
			Shell(Now, AtIn, Mm >= 300.0 ? "bomb500" : (Mm >= 180.0 ? "bomb250" : "how150"), "water", false);
			return;
		}
		// (the row's table by calibre: each number between the two calibres either side, held at the table's ends. Up to
		// eight calibres, rising; a calibre that does not rise ends the table)
		double Cal[8] = {};
		int NCal = 0;
		for (int I = 0; I < 8; ++I)
		{
			const std::string Key = P + "calibre_mm." + std::to_string(I);
			const double C = Num(Key, -1.0);
			if (!Has(Key) || !(C == C) || C <= (NCal > 0 ? Cal[NCal - 1] : 0.0) || C > 2000.0)
			{
				break;
			}
			Cal[NCal++] = C;
		}
		if (NCal == 0)
		{
			Shell(Now, AtIn, Mm >= 300.0 ? "bomb500" : (Mm >= 180.0 ? "bomb250" : "how150"), "water", false);
			return;
		}
		int Lo = 0;
		while (Lo + 1 < NCal && Cal[Lo + 1] < Mm)
		{
			++Lo;
		}
		const int Hi = std::min(Lo + 1, NCal - 1);
		const double Between = Hi > Lo ? Clamp01((Mm - Cal[Lo]) / (Cal[Hi] - Cal[Lo])) : 0.0;
		const double Below = Mm < Cal[0] ? 1.0 : 0.0; // (under the table's least calibre: as its first row)
		auto Table = [&](const char* Name, double Usual, double Least, double Most)
		{
			const double A = SafeNum(Num(P + Name + "." + std::to_string(Lo), Usual), Least, Most, Usual);
			const double B = SafeNum(Num(P + Name + "." + std::to_string(Hi), A), Least, Most, A);
			return Below > 0.0 ? A : A + (B - A) * Between;
		};
		const double H = Table("height_m", 30.0, 1.0, 150.0);
		Dice D(++Seeds * 7919ull ^ PlaceKey(AtIn));
		SplashLook Look;
		Look.RiseS = Table("rise_s", 1.5, 0.1, 10.0);
		Look.HangS = Table("hang_s", 2.0, 0.0, 20.0);
		Look.FallS = Table("fall_s", 3.0, 0.2, 30.0);
		Look.FoamM = Table("foam_m", 0.5 * H, 0.5, 200.0);
		Look.FoamS = Table("foam_s", 15.0, 0.5, 300.0);
		Look.CrownWide = Has(P + "crown_wide.0") ? Table("crown_wide", 0.17, 0.02, 1.0) : 0.0;
		Look.MistS = SafeNum(Num(P + "mist_s", 0.0), 0.0, 60.0, 0.0) * std::clamp(std::sqrt(H / 60.0), 0.5, 1.2);
		Look.FootWhite = SafeNum(Num(P + "foot_white", 0.0), 0.0, 1.0, 0.0);
		// (its dye: a name of the row's dyes, its colour in sunlight on the water, its shade side, and the colour it
		// shifts toward against the sun or with the sun low; the patch it leaves. No such dye, or none: white water)
		const std::string DyeName = DyeIn ? DyeIn : "";
		const std::string Dp = P + "dyes." + DyeName + ".";
		if (!DyeName.empty() && Has(Dp + "colour.0"))
		{
			const Rgb Water = Picture("grounds.water.spout.colour", Rgb::FromPicture(240, 244, 246));
			const Rgb Shadow = Picture("grounds.water.spout.shadow", Rgb::FromPicture(150, 166, 180));
			const auto Over = [](const Rgb& C, const Rgb& Of)
			{
				const auto One = [](float A, float B) { return static_cast<float>(SafeNum(A / std::max(B, 1e-4f), 0.0, 1.2, 1.0)); };
				return Rgb(One(C.R, Of.R), One(C.G, Of.G), One(C.B, Of.B));
			};
			const Rgb Lit = Picture(Dp + "colour", Water);
			const Rgb Shade = Picture(Dp + "shade", Lit * 0.7);
			double Shift = 0.0;
			if (bEyeKnown && Has(Dp + "shift.0"))
			{
				// (against the sun, or with the sun low, it shifts: Kongo's red toward purple)
				const Vec Seen = (AtIn - LastEye).Unit(Vec(0.0, 1.0, 0.0));
				const Vec Sun = LastSun.Unit(ZUp);
				Shift = SafeNum(Num(Dp + "shift_k", 1.0), 0.0, 1.0, 1.0) * std::max(Clamp01(2.0 * Seen.Dot(Sun)), Clamp01(1.0 - Sun.Z / 0.35));
			}
			const Rgb LitIs = Lit.Mix(Picture(Dp + "shift", Lit), Shift);
			Look.Tint = Over(LitIs, Water);
			Look.ShadeTint = Over(Shade.Mix(Picture(Dp + "shift", Shade) * 0.7, Shift), Shadow);
			Look.DyeK = SafeNum(Num(P + "dye_k", 0.85), 0.0, 1.0, 0.85) * SafeNum(Num(Dp + "strength", 1.0), 0.0, 1.0, 1.0);
			Look.DyeTop = SafeNum(Num(P + "dye_top", 0.35), 0.0, 1.0, 0.35);
			Look.DyeFoot = SafeNum(Num(P + "dye_foot", 0.5), 0.0, 1.0, 0.5);
			Look.DyePale = SafeNum(Num(P + "dye_pale", 0.45), 0.0, 1.0, 0.45);
			Look.MistDye = SafeNum(Num(P + "mist_dye", 0.4), 0.0, 1.0, 0.4);
			Look.StainK = SafeNum(Num(P + "stain", 0.35), 0.0, 1.0, 0.35) * SafeNum(Num(Dp + "strength", 1.0), 0.0, 1.0, 1.0);
			Look.Stain = Picture(Dp + "stain", Picture("grounds.water.spout.foam_colour", Rgb::FromPicture(206, 222, 220)).Mix(LitIs, 0.6));
		}
		// (far off its white is held over the sky and its dark foot is no dark puff: where the whole column stands under
		// far_px tall in the last frame's picture, in full from half that down)
		if (bEyeKnown)
		{
			const double Away = std::max(1.0, (AtIn - LastEye).Length());
			const double SeenPx = H * SafeNum(LastPpr, 1.0, 1.0e7, 1000.0) / Away;
			const double FarPxIs = SafeNum(Num(P + "far_px", 0.0), 0.0, 1.0e4, 0.0);
			const double FarIs = FarPxIs > 0.0 ? Clamp01(2.0 * (1.0 - SeenPx / FarPxIs)) : 0.0;
			Look.FarOver = 1.0 + (SafeNum(Num(P + "far_over", 1.0), 1.0, 4.0, 1.0) - 1.0) * FarIs;
			Look.FarFoot = SafeNum(Num(P + "far_foot", 0.0), 0.0, 1.0, 0.0) * FarIs;
		}
		const Vec At(AtIn.X, AtIn.Y, Height(AtIn.X, AtIn.Y));
		Spout(Now + 0.005, At, H, std::string(), D, &Look);
		// (a near miss shakes the eye close by: shake_within_k times its height. 0: none)
		const double Within = SafeNum(Num(P + "shake_within_k", 0.0), 0.0, 20.0, 0.0) * H;
		if (Within > 0.0)
		{
			Knock J;
			J.At = AtIn;
			J.Born = Now;
			J.Strength = Clamp01(H / 60.0);
			J.Within = Within;
			Knocks.push_back(J);
		}
	}

	void Battle::ScreenLay(double Now, int Id, const Vec& At, const Vec& Velocity, bool bFunnel)
	{
		// A smoke screen laid at sea (the sea scene's ask, 22 November: Samar's escorts laid eight, up to five at once,
		// each reported every step at her stern; the art director: each escort lays her own plume, dense at the source,
		// white-grey from the stern generators and black from the funnels, lumpy, thinning downwind, with gaps). It
		// hangs low on the water, lasts minutes, drifts and spreads down the wind. No row, or a call that is no place
		// or no moment: nothing.
		if (!Has("sea_screen.white.lip_m") || !(Now == Now) || std::fabs(Now) > 1.0e9)
		{
			return;
		}
		const auto Place = [](double V) { return V == V && std::fabs(V) <= 1.0e7; };
		if (!Place(At.X) || !Place(At.Y) || !Place(At.Z))
		{
			return;
		}
		const auto Kept = [](double V) { return V == V && std::fabs(V) <= 150.0 ? V : 0.0; };
		Vec Vel(Kept(Velocity.X), Kept(Velocity.Y), Kept(Velocity.Z));
		if (Vel.Length() > 150.0)
		{
			Vel = Vec();
		}
		SeaScreen* Own = nullptr;
		for (SeaScreen& S : SeaScreens)
		{
			if (S.Id == Id)
			{
				Own = &S;
			}
		}
		if (!Own || !Own->bOn)
		{
			if (!Own)
			{
				// (held to 64 screens at once: the stopped ones make room first; with 64 still laying, a new one lays nothing)
				if (SeaScreens.size() >= 64)
				{
					SeaScreens.erase(std::remove_if(SeaScreens.begin(), SeaScreens.end(), [](const SeaScreen& S) { return !S.bOn; }), SeaScreens.end());
				}
				if (SeaScreens.size() >= 64)
				{
					return;
				}
				SeaScreens.push_back(SeaScreen());
				Own = &SeaScreens.back();
			}
			// (a new screen, or one laid again after it stopped: afresh from here, with a puff at once)
			*Own = SeaScreen();
			Own->Id = Id;
			Own->bOn = true;
			Own->Last = Now;
			Own->At = At;
			Own->Vel = Vel;
			Own->Owed = 1.0e9;
			Own->Seed = Mix64(static_cast<uint64_t>(static_cast<int64_t>(Id)) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(static_cast<int64_t>(Now * 1000.0)) ^ 0x5EA5C4ull) | 1ull;
		}
		Own->bFunnel = bFunnel;
		SeaScreenPuffs(*Own, Now, At, Vel);
	}

	void Battle::ScreenLayStop(double Now, int Id)
	{
		(void)Now;
		for (SeaScreen& S : SeaScreens)
		{
			if (S.Id == Id)
			{
				S.bOn = false;
			}
		}
	}

	void Battle::SeaScreenPuffs(SeaScreen& S, double Now, const Vec& At, const Vec& Vel)
	{
		// The screen's puffs laid since her last call, by time, along her way between the two calls (as a funnel's
		// plume, PlumeMove): one every overlap-th of a puff's width of the wind she feels, each let go into the air she
		// steams through with none of her speed. It widens as the two thirds power of its age (wide_k), and as it does
		// its puffs go by halves (even, Bit::Even), so those left always stand evenly and as deep: the screen is
		// unbroken and its old end costs few pieces. Each puff holds its smoke over a face as much wider, so it thins
		// downwind; it also thins by its own age (fade_s), and along the screen some stretches thin sooner than the
		// rest (gaps, over gap_s of laying; gap_thin of their smoke is thin from the start), so gaps open in its old parts
		// while it stays dense at her stern. It rests on the water (rest) and climbs only a little (rise_mps), drawn out
		// along its line (stretch), lumpy (rough, lumps, the roll; and billow of its puffs billow_size as big and climbing
		// billow_rise as fast, so its top is heaped and uneven). Far off, a puff is never under least_px across in the last frame's picture: fewer, bigger
		// puffs, overlapping overlap_far deep, each as much denser. Past busy_from of the room, half as deep.
		const std::string P = "sea_screen.";
		const std::string K = P + (S.bFunnel ? "black." : "white.");
		auto One = [&](const char* Name, double Usual, double Least, double Most)
		{
			return SafeNum(Num(K + Name, Num(P + Name, Usual)), Least, Most, Usual);
		};
		auto Two = [&](const char* Name, double Low, double High, double Least, double Most)
		{
			const std::string Path = Has(K + Name + ".0") || Has(K + Name) ? K + Name : P + Name;
			Span R = Range(Path, Low, High);
			R.A = SafeNum(R.A, Least, Most, Low);
			R.B = std::max(R.A, SafeNum(R.B, Least, Most, High));
			return R;
		};
		auto Colour = [&](const char* Name, const Rgb& Usual)
		{
			return Picture(Has(K + Name + ".0") ? K + Name : P + Name, Usual);
		};
		const double Restart = One("restart_after_s", 6.0, 0.1, 120.0);
		if (Now < S.Last || Now - S.Last > Restart)
		{
			// (time going back, or a long silence: afresh from here, with a puff at once)
			S.Last = Now;
			S.At = At;
			S.Vel = Vel;
			S.Owed = 1.0e9;
		}
		const double Between = Now - S.Last;
		const Vec Path = At - S.At;
		const Vec Blown = Wind.X == Wind.X && Wind.Y == Wind.Y && Wind.Z == Wind.Z ? Vec(Wind.X, Wind.Y, 0.0) : Vec();
		const Vec Through = Vec(Vel.X - Blown.X, Vel.Y - Blown.Y, 0.0); // (her way through the air: the line the screen is laid along)
		const double Felt = std::clamp(Through.Length(), One("felt_least_mps", 3.0, 0.5, 30.0), 60.0);
		const double LipOwn = One("lip_m", 14.0, 1.0, 200.0);
		double Lip = LipOwn;
		const double DeepOwn = One("overlap", 3.0, 1.0, 6.0);
		double Deep = DeepOwn;
		if (bEyeKnown)
		{
			const double Away = (At - LastEye).Length(), Ppr = SafeNum(LastPpr, 1.0, 1.0e7, 1000.0);
			const double LeastPx = One("least_px", 0.0, 0.0, 400.0);
			if (Away == Away && Away > 1.0 && LeastPx > 0.0)
			{
				const double SeenPx = LipOwn * Ppr / Away;
				if (SeenPx < LeastPx)
				{
					const double DeepFar = std::min(One("overlap_far", DeepOwn, 1.0, 6.0), DeepOwn);
					Deep = DeepFar + (DeepOwn - DeepFar) * Clamp01(SeenPx / LeastPx);
					Lip = std::min(LeastPx * Away / Ppr, One("lip_most", 8.0, 1.0, 50.0) * LipOwn);
				}
			}
		}
		if (static_cast<double>(RoomGate()) > One("busy_from", 0.6, 0.05, 4.0) * MaxPuffs)
		{
			Deep = std::max(1.0, 0.5 * Deep);
		}
		const double Every = SafeNum(Lip / (Deep * Felt), 0.05, 60.0, 0.5);
		const double WideK = One("wide_k", 1.7, 0.0, 50.0);
		const double Widens = std::min(WideK / std::max(Lip, 1e-3), 1.0e6);
		const double AgeS = One("age_s", 300.0, 5.0, 1800.0);
		const double Even = One("even", 0.0, 0.0, 0.9);
		const int EvenNear = static_cast<int>(One("even_near", 2.0, 0.0, 20.0)), EvenFar = static_cast<int>(One("even_far", 0.0, 0.0, 20.0));
		const double EvenStep = One("even_step", 1.6, 1.15, 2.0);
		const Span Rises = Two("rise_mps", 0.02, 0.1, 0.0, 5.0);
		const double Spread = One("spread_mps", 0.2, 0.0, 10.0), FromUp = One("from_up_m", 2.0, 0.0, 100.0), Ahead = One("from_ahead_m", 0.0, -200.0, 200.0);
		const double Size = One("size", 0.25, 0.0, 0.8), Shading = One("shade", 0.05, 0.0, 0.5), Rough = One("rough", 0.3, 0.0, 1.0);
		const double Alpha = One("alpha", 0.85, 0.0, 0.99), FadeS = One("fade_s", 150.0, 1.0, 3600.0), FadePow = One("fade_pow", 1.5, 0.3, 6.0);
		const double OldBy = One("old_by_s", 120.0, 0.5, 3600.0), Stretch = One("stretch", 1.0, 1.0, 4.0), Rest = One("rest", 0.5, 0.0, 2.0);
		const double Gaps = One("gaps", 0.0, 0.0, 0.95), GapS = One("gap_s", 15.0, 0.5, 600.0), GapFrom = One("gap_from", 0.5, 0.0, 0.95);
		const double GapThin = One("gap_thin", 0.0, 0.0, 0.95), Billow = One("billow", 0.0, 0.0, 1.0);
		const double BillowSize = One("billow_size", 1.5, 1.0, 3.0), BillowRise = One("billow_rise", 3.0, 1.0, 20.0);
		const double ShadowOf = One("shadow_of_lit", 0.75, 0.0, 1.0), ShadowK = One("shadow_k", 0.0, 0.0, 1.0);
		const double RollLift = One("roll_lift", 0.0, 0.0, 1.0), RollShade = One("roll_shade", 0.0, 0.0, 0.5), RollRun = One("roll_run", 4.0, 1.0, 64.0);
		const int Lumps = static_cast<int>(One("lumps", 0.0, 0.0, 8.0));
		const double LumpsPx = One("lumps_px", 120.0, 0.0, 1.0e5);
		const Rgb Col = Colour("colour", Rgb::FromPicture(232, 233, 230)), Old = Colour("colour_old", Col);
		const Vec Line = Through.Unit(Vel.Length() > 0.1 ? Vec(Vel.X, Vel.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0)) : Vec(1.0, 0.0, 0.0));
		const Vec Across(-Line.Y, Line.X, 0.0);
		const Vec Forward = Vec(Vel.X, Vel.Y, 0.0).Unit(Line);
		const int MostLaid = static_cast<int>(One("most_a_call", 64.0, 1.0, 400.0));
		// (the screen's gaps: a slow value along its laying, by its own seed, cell by cell of gap_s seconds)
		auto GapAt = [&](double T)
		{
			const double X = (T - std::floor(T / 1.0e6) * 1.0e6) / GapS;
			const double Cell = std::floor(X), In = X - Cell;
			auto Thrown = [&](double C)
			{
				const uint64_t Hh = Mix64(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(C)) * 0xD1B54A32D192ED03ull));
				return static_cast<double>(Hh >> 11) * (1.0 / 9007199254740992.0);
			};
			const double V = Thrown(Cell) + (Thrown(Cell + 1.0) - Thrown(Cell)) * (In * In * (3.0 - 2.0 * In));
			return Clamp01((V - GapFrom) / std::max(1.0 - GapFrom, 1e-3));
		};
		int Laid = 0;
		double LastT = S.Last;
		double T = S.Last + std::max(0.0, Every - S.Owed);
		for (; T <= Now && Laid < MostLaid; T += Every, ++Laid)
		{
			const double Kt = Between > 1e-9 ? Clamp01((T - S.Last) / Between) : 1.0;
			const uint32_t N = S.N++;
			Dice D(Mix64(S.Seed ^ (static_cast<uint64_t>(N) * 0xD1B54A32D192ED03ull) ^ 0x5C3Eull) | 1ull);
			// (how many halvings it lives through, and when the next comes: as a funnel's, Bit::Even)
			int Halvings = N == 0u ? 31 : 0;
			for (uint32_t Left = N; Left != 0u && (Left & 1u) == 0u; Left >>= 1)
			{
				++Halvings;
			}
			const int Next = std::min(Halvings + 1, 30), NextNear = std::min(Next, EvenNear), NextMid = std::clamp(Next - EvenNear, 0, EvenFar);
			const double HalvedAt = std::ldexp(1.0, NextNear + std::max(Next - EvenNear - EvenFar, 0)) * std::pow(EvenStep, NextMid);
			const double Goes = Even > 0.0 && Widens > 1e-9 ? std::pow((HalvedAt - 1.0) / Widens, 1.5) : AgeS;
			const bool bBillow = D.U() < Billow;
			const double SizeK = D.U(1.0 - Size, 1.0 + Size) * (bBillow ? BillowSize : 1.0);
			S.Wander = S.Wander * std::exp(-Every / 20.0) + 0.6 * std::sqrt(1.0 - std::exp(-2.0 * Every / 20.0)) * D.Bell();
			S.Lift = 1.0 + (S.Lift - 1.0) * std::exp(-Every / 10.0) + 0.3 * std::sqrt(1.0 - std::exp(-2.0 * Every / 10.0)) * D.Bell();
			Bit B;
			B.Born = T;
			B.Life = AgeS * D.U(0.9, 1.0);
			if (Even > 0.0)
			{
				B.Life = std::clamp(Goes, 0.05, AgeS);
				B.Even = Halvings;
				B.EvenK = Widens;
				B.EvenWin = Even;
				B.EvenNear = EvenNear;
				B.EvenFar = EvenFar;
				B.EvenStep = std::log2(EvenStep);
				B.EvenN = N;
				B.EvenSeed = static_cast<uint32_t>(S.Seed >> 32);
				B.EvenRollLift = RollLift;
				B.EvenRollShade = RollShade;
				B.EvenRollRun = RollRun;
			}
			B.From = S.At + Path * Kt + Forward * Ahead + Across * (D.Bell() * 0.1 * Lip) + ZUp * FromUp;
			B.Vel = Across * (Spread * std::clamp(S.Wander, -2.0, 2.0));
			B.Drag = 0.0;
			B.Windage = D.U(0.85, 1.0);
			B.Rise = Rises.Pick(D) * std::clamp(S.Lift, 0.3, 1.7) * (bBillow ? BillowRise : 1.0);
			B.R0 = 0.5 * Lip * SizeK;
			B.R1 = B.R0 + 0.5 * WideK * SizeK * std::pow(B.Life, 2.0 / 3.0);
			B.Grow = 2.0 / 3.0;
			B.Along = Line;
			B.Stretch = Stretch;
			B.Rest = Rest;
			const double Shade = D.U(1.0 - Shading, 1.0 + Shading);
			B.Col = Col * Shade;
			B.Col2 = B.Col.Mix(Old * Shade, std::min(1.0, B.Life / OldBy));
			B.Cool = 1.0;
			const double Thick = std::min(0.97, Alpha * D.U(0.9, 1.0));
			B.Alpha = (Deep < DeepOwn ? std::min(0.97, 1.0 - std::pow(1.0 - Thick, DeepOwn / Deep)) : Thick) * (1.0 - GapThin * GapAt(T)); // (laid less deep, each holds as much more smoke)
			B.AgeFade = FadeS * (1.0 - Gaps * GapAt(T));
			B.AgeFadePow = FadePow;
			B.FadeIn = 0.3;
			B.Hold = Even > 0.0 && Goes < AgeS ? 1.0 : 0.8;
			B.Heart = 0.7;
			B.HeartRound = 1.0;
			B.Inner = 0.55;
			B.Rim = 0.0;
			B.Rough = Rough;
			B.Lumps = Lumps;
			B.LumpsPx = LumpsPx;
			B.LumpsAlpha = 0.85;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.TurnRate = D.U(-0.05, 0.05);
			if (ShadowK > 0.0)
			{
				B.ShadowCol = Rgb(static_cast<float>(B.Col.R * ShadowOf * 0.97), static_cast<float>(B.Col.G * ShadowOf), static_cast<float>(B.Col.B * ShadowOf * 1.04));
				B.ShadowK = ShadowK;
			}
			Bits.push_back(B);
			LastT = T;
		}
		S.Owed = Laid > 0 ? Now - LastT : std::min(S.Owed + Between, 1.0e9);
		S.Last = Now;
		S.At = At;
		S.Vel = Vel;
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
		B.SunYoung = R.SunYoung; // (sun_by_s: the column's young smoke, low over the fire, takes little of the light)
		B.SunFrom = R.SunFrom;
		B.SunBy = R.SunBy;
		B.Centre = S.At;
		B.AxisWindage = 0.5 * ((bPall ? R.PallWindage.A : R.Windage.A) + (bPall ? R.PallWindage.B : R.Windage.B));
		B.OilId = bPall ? -1 : S.Id;
		if (!bPall)
		{
			// (Bit::Lumps: the column's own puffs only; the pall is a thin sheet aloft and stays as it was)
			B.Lumps = R.Lumps;
			B.LumpsPx = R.LumpsPx;
			B.LumpsAlpha = R.LumpsAlpha;
			B.LumpsLight = R.LumpsLight;
			B.LumpsRim = R.LumpsRim;
			B.LumpsInner = R.LumpsInner;
			B.LumpsR0 = R.LumpsR.A;
			B.LumpsR1 = R.LumpsR.B;
			B.LumpsOut0 = R.LumpsOut.A;
			B.LumpsOut1 = R.LumpsOut.B;
		}
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
		if (R.PaleUpM > 0.0)
		{
			// (fx-air r2: it pales and thins as it climbs over the fire; see Bit::PaleUpM)
			B.PaleUpM = R.PaleUpM;
			B.PaleUpMost = R.PaleUpMost;
			B.PaleUpPow = R.PaleUpPow;
			B.ThinUp = R.ThinUp;
			B.PaleUpZ = S.At.Z;
			B.PaleUpCol = R.PaleUpCol;
			B.PaleUpLit = R.PaleUpLit;
		}
		Bits.push_back(B);
	}

	void Battle::Run(double Now)
	{
		// (a moment that is no number, or so far off that a step of a twentieth of a second no longer moves it, lays
		// nothing: a fire's loop below would never end. The pre-check on 3f7a8e9: an endless fire and a Draw at 1e300
		// laid pieces until memory was gone)
		if (!(Now == Now) || Now < -1.0e9 || Now > 1.0e9)
		{
			return;
		}
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
		// (fire.smoke_foot, art director 19:57Z on build 1748's Pearl Harbor: a big fire's smoke, resting on the sea, paled
		// with age to a sunlit grey bank at the column's foot and read as steam. Smoke is youngest and darkest at the fire:
		// while a big fire's puff lies on the ground or sea it keeps this dark, after the flames too, and takes its own
		// colour only as it rides smoke_foot_clear of its radius clear of where it rests (Bit::FootClear). Big: from
		// smoke_foot_from_m across, a little, to all of it. A small fire's smoke is as before. No such key: as before)
		const bool bSmokeFoot = SmokeRest > 0.0 && Has("fire.smoke_foot.0");
		const Rgb SmokeFoot = bSmokeFoot ? Picture("fire.smoke_foot", Rgb::FromPicture(50, 46, 42)) : Rgb();
		const double FootClearIs = bSmokeFoot ? Num("fire.smoke_foot_clear", 1.0) : 0.0, FootFromA = Num("fire.smoke_foot_from_m.0", 8.0), FootFromB = Num("fire.smoke_foot_from_m.1", 20.0);
		const double FootClear = FootClearIs == FootClearIs ? std::min(10.0, std::max(0.0, FootClearIs)) : 0.0;
		const TongueRow Tongues = TongueRowOf();
		const bool bLandTongues = Has("fire.land_tongues.every_s");
		const TongueRow LandTongues = bLandTongues ? TongueRowOf(true) : TongueRow();
		const bool bVentTongues = Has("fire.vent_tongues.every_s");
		const TongueRow VentTongues = bVentTongues ? TongueRowOf(true, true) : TongueRow();
		const GlowRow Glow = GlowRowOf();
		const LateRow Late = LateRowOf();
		// (fire.land_column: a burning vehicle's plain smoke under its own column is only its foot, base_r1 as wide and
		// base_life as long as it was; the column above carries the rest)
		const bool bLandBase = Has("fire.land_column.base_r1");
		const double BaseR1 = bLandBase ? SafeNum(Num("fire.land_column.base_r1", 1.0), 0.05, 4.0, 1.0) : 1.0;
		const double BaseLife = bLandBase ? SafeNum(Num("fire.land_column.base_life", 1.0), 0.05, 4.0, 1.0) : 1.0;
		// (a moving ship's, BurnMove: whatever a source lays in its turn below is born at its seat as it moves)
		struct LaidBy
		{
			Battle& Maker;
			const Source& Whose;
			size_t Since;
			LaidBy(Battle& MakerIn, const Source& WhoseIn) : Maker(MakerIn), Whose(WhoseIn), Since(MakerIn.Bits.size()) {}
			~LaidBy()
			{
				if (Whose.bMoves)
				{
					Maker.Rides(Whose, Since);
				}
			}
		};
		for (Source& S : Sources)
		{
			const LaidBy InItsTurn(*this, S);
			// (after a long pause, such as the game being paused, only the last second is made up)
			S.Next = std::max(S.Next, Now - 1.0);
			S.NextSmoke = std::max(S.NextSmoke, Now - 1.0);
			if (S.Kind == 3)
			{
				// a big fire's own column (fire.smoke_column): the oil column's kind of puff, over the fire or fires it
				// stands on, and its pall; thinner, greyer and slower once the flames are out. None of it while burning
				// oil stands over her
				const OilRow& R = S.Oil;
				const double Step = R.ColumnEvery;
				auto Lay = [&](double T, bool bPall)
				{
					const bool bFlames = T < S.Flames;
					if (!S.bStoodDown && UnderOil(S, T))
					{
						S.bStoodDown = true;
					}
					if (!S.Heads.empty() && !S.bStoodDown)
					{
						OilColumn(S, T, bPall);
						if (Late.bOn && S.bLandColumn)
						{
							// (a land column over a fire burning down, fire.late: grey-brown and thinner; its fire's seed is its own
							// turned back, as OwnColumn turned it)
							const double K = LateShare(Late, S.Seed ^ 0x5C01A3D5EEDull, S.Start, T);
							if (K > 0.0)
							{
								Bit& Old = Bits.back();
								Old.Col = Old.Col.Mix(Late.Col, K);
								Old.Col2 = Old.Col2.Mix(Late.Top, K);
								Old.Alpha *= 1.0 - (1.0 - Late.Alpha) * K;
							}
						}
						// (and a moving ship's column, BurnMove, of any width: her puffs are left astern at her own speed, far
						// apart for their size, a row of beads. Each is as wide as the way to the next, as a small fire's)
						if (R.TightM > 0.0 && (S.Across < R.TightM || S.bMoves))
						{
							// (the art director on the looks' first cuts, 15:40Z: "at 300 m the ship fire's smoke breaks into a
							// scatter of separate flakes. Each puff should overlap the next at every distance, at least 1.5
							// times the spacing with a floor in pixels." A puff's size goes by the fire's width, but how far
							// apart the row's spread of windage and push throws two puffs does not: over a 60 m fire the
							// puffs are wider than their scatter and the column is one piece, over a 12 m fire they are a
							// fifth as wide and thrown as far apart. So a fire narrower than tight_m keeps only its share
							// of that spread, about the middle of each range: its puffs go nearly one way. And each is at
							// least overlap times as wide as the way to the next, young and old, with what scatter is left.
							// No dice are thrown; at tight_m or wider, or without the key, nothing is changed)
							Bit& Held = Bits.back();
							const double Keep = Clamp01(S.Across / R.TightM);
							const double Big = std::pow(S.Across / 100.0, 0.4);
							const Span& Blown = bPall ? R.PallWindage : R.Windage;
							const double Level = bPall ? 0.5 * (R.PallLevel.A + R.PallLevel.B) : 1.0;
							const double BlownMid = 0.5 * (Blown.A + Blown.B), PushMid = 0.5 * (R.Burst.A + R.Burst.B) * Big * Level;
							Held.Windage = BlownMid + (Held.Windage - BlownMid) * Keep;
							Held.Vel = Vec(Held.Vel.X * Keep, Held.Vel.Y * Keep, PushMid + (Held.Vel.Z - PushMid) * Keep);
							if (bPall)
							{
								// (the pall: its puffs are born every few seconds, far apart for their size over a small
								// fire, and run out ahead of the column as stray flakes. A small fire's smoke thins away
								// before it makes a sheet: its pall keeps its share of its ink, and of its slow wander)
								Held.Rise *= Keep;
								Held.Alpha *= Keep;
							}
							else if (R.Overlap > 0.0)
							{
								const Vec TrueAir = Wind.X == Wind.X && Wind.Y == Wind.Y && Wind.Z == Wind.Z ? Wind : Vec();
								const Vec Air = S.bMoves ? TrueAir - S.Vel : TrueAir; // (a moving ship's, BurnMove: her puffs are left astern at her own speed)
								const double Blow = std::min(Air.Length(), 60.0);
								const double Spent = std::exp(-std::max(Held.Drag, 0.0) * Held.Life);
								const double Pushed = Held.Drag > 1e-6 ? (1.0 - Spent) / Held.Drag : Held.Life;
								const double StepNow = Step * (bFlames ? 1.0 : 2.0);
								const double Young = (Held.Vel + Air * Held.Windage).Length();
								const double Old = (Held.Vel * Spent + Air * Held.Windage + ZUp * std::max(0.0, Held.Rise - Held.Gravity * Held.Life)).Length();
								// (two throws of a range lie a third of it apart, taken one with another)
								const double Scatter = Keep * ((Blown.B - Blown.A) * Blow * Held.Life + (R.Burst.B - R.Burst.A) * Big * Pushed) / 3.0;
								const double Widest = std::min(400.0, 2.0 * S.Across);
								const double Wide0 = std::min(Widest, 0.5 * R.Overlap * Young * StepNow);
								const double Wide1 = std::min(Widest, 0.5 * R.Overlap * (Old * StepNow + (1.0 - Keep) * Scatter));
								if (Wide0 == Wide0 && Wide1 == Wide1)
								{
									Held.R0 = std::max(Held.R0, Wide0);
									Held.R1 = std::max(Held.R1, std::max(Wide1, Held.R0));
								}
							}
							if (R.ColumnLeastPx > 0.0)
							{
								Held.LeastPx = R.ColumnLeastPx;
								Held.LeastKeep = R.ColumnLeastKeep;
							}
						}
						if (!bFlames)
						{
							const double After = (T - S.Flames) / std::max(S.End - S.Flames, 1.0);
							Bit& B = Bits.back();
							B.Alpha *= std::max(0.15, 1.0 - After);
							B.Col = B.Col.Mix(Rgb::FromPicture(110, 108, 104), 0.5);
							B.Col2 = B.Col2.Mix(Rgb::FromPicture(120, 118, 114), 0.5);
						}
					}
					return bFlames ? 1.0 : 2.0;
				};
				while (S.NextSmoke <= Now && S.NextSmoke < S.End)
				{
					S.NextSmoke += Step * Lay(S.NextSmoke, false);
				}
				S.NextPall = std::max(S.NextPall, Now - 1.0);
				while (R.PallEvery > 0.0 && S.NextPall <= Now && S.NextPall < S.End)
				{
					S.NextPall += R.PallEvery * Lay(S.NextPall, true);
				}
				continue;
			}
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
			if (S.Kind == 4)
			{
				// a steam vent (steam): rags blown along its way, the pressure full while it roars and then falling, so
				// its length and speed sink together to a wisp that wavers about its way and leans with the wind
				const SteamRow& R = S.Vent;
				const Vec SideA = S.Way.Cross(std::fabs(S.Way.Z) < 0.9 ? ZUp : Vec(1.0, 0.0, 0.0)).Unit(Vec(1.0, 0.0, 0.0));
				const Vec SideB = S.Way.Cross(SideA).Unit(Vec(0.0, 1.0, 0.0));
				const double Phase = static_cast<double>(S.Seed % 6283ull) * 0.001;
				while (S.Next <= Now && S.Next < S.End)
				{
					Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(S.Next * 1000.0)) * 0x9E3779B97F4A7C15ull + 41ull));
					const double X = (S.Next - S.Start) / std::max(S.End - S.Start, 0.1);
					const double Fall = X <= R.Roar ? 0.0 : Clamp01((X - R.Roar) / std::max(1.0 - R.Roar, 1e-3));
					const double Push = 1.0 - (1.0 - R.Least) * std::pow(Fall, 0.8);
					const double Wave = R.Waver * Fall * Fall;
					const Vec Way = (S.Way + SideA * (Wave * std::sin(S.Next * 2.3 + Phase)) + SideB * (Wave * std::sin(S.Next * 3.9 + 1.3 * Phase))).Unit(S.Way);
					SteamRag(S.Next, S.At, Way, S.JetTall, S.Across, Push, R, D);
					S.Next += R.Every * (1.0 + Fall);
				}
				continue;
			}
			if (S.Kind == 5)
			{
				// ammunition cooking off (cook_off): its pops at their moments, and the hatch jet while it burns
				while (S.PopNext < S.Pops.size() && S.Pops[S.PopNext] <= Now)
				{
					if (Now - S.Pops[S.PopNext] < 1.0)
					{
						const size_t First = Bits.size();
						CookPop(S, S.Pops[S.PopNext]);
						LingerFrom(First, LingerOf("cook"), S.Pops[S.PopNext], S.At, 0.9, 6.0);
					}
					++S.PopNext;
				}
				const double JetFor = S.JetEnd - S.JetAt;
				while (JetFor > 0.0 && S.Next <= Now && S.Next < S.JetEnd)
				{
					// (up in a quarter of a second, sinking over its last quarter, eaten back and never thinned; in two
					// pulses it sinks to the row's dip between them)
					const double X = (S.Next - S.JetAt) / JetFor;
					double Full = std::min(1.0, (S.Next - S.JetAt) / 0.25) * Clamp01((1.0 - X) / 0.25);
					if (S.JetDip > 0.0)
					{
						const double Dx = (X - S.JetDip) / 0.1;
						Full *= 1.0 - (1.0 - S.Cook.Dip) * std::exp(-Dx * Dx);
					}
					if (S.Next >= S.JetAt)
					{
						const size_t First = Bits.size();
						CookJet(S, S.Next, Full);
						LingerFrom(First, LingerOf("cook"), S.Next, S.At, 0.9, 6.0);
					}
					S.Next += S.Cook.JetEvery;
				}
				continue;
			}
			if (S.Kind == 6)
			{
				// a smoke screen (smoke_screen): its low white wall, laid from its seats while it lasts
				ScreenLumps(S, Now);
				continue;
			}
			if (S.Kind == 0)
			{
				if (S.AcrossBy > 0.0)
				{
					// (joined by another, fire.join: it widens smoothly to its new width, never in a step)
					const double G = Clamp01((Now - S.AcrossAt) / S.AcrossBy);
					S.Across = S.AcrossFrom + (S.AcrossTo - S.AcrossFrom) * G * G * (3.0 - 2.0 * G);
					if (G >= 1.0)
					{
						S.Across = S.AcrossTo;
						S.AcrossBy = 0.0;
					}
				}
				if (Glow.bOn)
				{
					FireGlow(S, Now, Glow, 1.0 - (1.0 - Late.Light) * LateShare(Late, S.Seed, S.Start, Now));
				}
				// long ragged tongues rooted where it burns, dying by being eaten away; then a thinning grey column
				const TongueRow& Row = S.bVent && bVentTongues ? VentTongues : bLandTongues && S.Next <= Now && S.Next < S.Flames && LandFire(S) ? LandTongues : Tongues;
				while (S.Next <= Now && S.Next < S.Flames)
				{
					if (Row.bOn)
					{
						// (fire.tongues: ragged tongues at the fire's own seats, in place of the upright cones below)
						FireTongues(S, S.Next, Row, Flame, Heart, Ember, &Late, LateShare(Late, S.Seed, S.Start, S.Next));
						S.Next += Row.Every;
						continue;
					}
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
						if (S.OutAt >= 0.0 && S.Next > S.OutAt)
						{
							// (put out, fire.put_out: eaten back from the edges toward its seat, lower and in fits, each
							// tongue as dense as ever. Its own dice: the fire's own fall as they did)
							// (the art director on the mend round, 16:41Z: with whole fits of no flame at all, at 90 m "the
							// flames vanish behind the steam at 2.5 s and stand full height again at 3.7 s, so they read as
							// blinking off and on, not being eaten back. By 3.7 s they should be about half height with
							// fewer tongues, shrinking steadily from the edges." So no fit takes every tongue: a fit only
							// lowers them a little for its third of a second; their height falls steadily, half by two
							// fifths of the time; and fewer are kept as it goes)
							const double Left = Clamp01(1.0 - (S.Next - S.OutAt) / std::max(S.OutOver, 1e-3));
							Dice Fit(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(S.Next / 0.3)) * 0xD1B54A32D192ED03ull + 0x0D7ull));
							Dice One(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(S.Next * 1000.0)) * 0xA0761D6478BD642Full + static_cast<uint64_t>(I) + 0x0D8ull));
							if (One.U() > 0.25 + 0.75 * std::pow(Left, 1.2))
							{
								continue;
							}
							const double Low = std::max(0.12, std::pow(Left, 1.4)) * (Fit.U() > 0.45 + 0.55 * Left ? 0.8 : 1.0);
							B.From = S.At + (Base - S.At) * Left + ZUp * (0.2 * S.Across * Low);
							B.R0 *= Low;
							B.R1 *= Low;
							B.Stretch = 1.0 + (B.Stretch - 1.0) * (0.5 + 0.5 * Left);
						}
						Bits.push_back(B);
					}
					S.Next += 0.05;
				}
				if (S.OutAt >= 0.0 && S.OutSurge > 0.0)
				{
					// (put out with water: a surge of white steam at the seat for the first seconds, strongest a third
					// of the way through them; it rises fast and is gone, beside the smoke that stays)
					S.NextSteam = std::max(S.NextSteam, Now - 1.0);
					const double Wide = std::min(S.Across, 2000.0);
					while (S.NextSteam <= Now && S.NextSteam < S.OutAt + S.OutSurge)
					{
						Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(S.NextSteam * 1000.0)) * 0x9E3779B97F4A7C15ull + 0x57EAull));
						const double X = (S.NextSteam - S.OutAt) / S.OutSurge;
						const double Push = std::max(S.Vent.Least, std::sin(Pi * std::pow(Clamp01(X), 0.6)));
						const Vec From = S.At + Vec(D.U(-0.3, 0.3), D.U(-0.3, 0.3), 0.1) * Wide;
						SteamRag(S.NextSteam, From, ZUp, S.Vent.Reach * Wide, 0.5 * Wide, Push, S.Vent, D);
						S.NextSteam += S.Vent.Every;
					}
				}
				// (put out: its smoke pales from black through grey to grey-white by OutPale seconds, thinning to a wisp
				// that is gone at its end)
				const Rgb PaleCol = S.OutAt >= 0.0 ? Picture("fire.put_out.smoke", Rgb::FromPicture(196, 195, 191)) : Rgb();
				while (S.NextSmoke <= Now && S.NextSmoke < S.End)
				{
					Dice D(S.Seed ^ (static_cast<uint64_t>(static_cast<int64_t>(S.NextSmoke * 1000.0)) + 17ull));
					const bool bFlames = S.NextSmoke < S.Flames;
					const double After = bFlames ? 0.0 : (S.NextSmoke - S.Flames) / std::max(S.End - S.Flames, 1.0);
					const double Pale = S.OutAt >= 0.0 ? Clamp01((S.NextSmoke - S.OutAt) / std::max(S.OutPale, 1e-3)) : 0.0;
					if (S.Bank.bOn)
					{
						// (a big fire's smoke: a bank of lumps down the wind, see fire.smoke_bank)
						const double BigIs = (S.Across - FootFromA) / std::max(FootFromB - FootFromA, 1e-3);
						SmokeBank(S, S.NextSmoke, bFlames, After, SmokeFoot, FootClear, BigIs == BigIs ? Clamp01(BigIs) : 0.0, Pale, PaleCol);
						S.NextSmoke += bFlames ? S.Bank.EveryA : S.Bank.EveryB;
						continue;
					}
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
					if (bLandBase && S.Column >= 0 && bFlames && LandColumnStands(S.Column, S.NextSmoke))
					{
						B.Life *= BaseLife;
						B.R1 = std::max(B.R0, B.R1 * BaseR1);
					}
					B.Col = bFlames ? Rgb::FromPicture(50, 46, 42) : Rgb::FromPicture(110, 108, 104);
					B.Col2 = B.Col.Mix(Rgb::FromPicture(120, 118, 114), 0.5);
					if (Late.bOn && bFlames)
					{
						// (burning down, fire.late: its own smoke greyer, as its column's)
						const double LateK = LateShare(Late, S.Seed, S.Start, S.NextSmoke);
						B.Col = B.Col.Mix(Late.Col, LateK);
						B.Col2 = B.Col2.Mix(Late.Top, LateK);
					}
					if (FootClear > 0.0)
					{
						const double BigIs = (S.Across - FootFromA) / std::max(FootFromB - FootFromA, 1e-3);
						B.FootK = BigIs == BigIs ? Clamp01(BigIs) : 0.0;
						B.FootClear = B.FootK > 0.0 ? FootClear : 0.0;
						B.FootCol = SmokeFoot;
					}
					B.Alpha = 0.5 * std::max(0.15, 1.0 - After);
					if (S.OutAt >= 0.0 && S.NextSmoke > S.OutAt)
					{
						// (grey by half of that, grey-white by its end; no dark foot once it pales; and thinning to nothing)
						const Rgb Grey = Rgb::FromPicture(110, 108, 104);
						const Rgb Dark = Rgb::FromPicture(50, 46, 42);
						B.Col = Pale < 0.5 ? Dark.Mix(Grey, Pale * 2.0) : Grey.Mix(PaleCol, Pale * 2.0 - 1.0);
						B.Col2 = B.Col.Mix(PaleCol, 0.5);
						B.FootK *= 1.0 - Pale;
						B.Alpha = 0.5 * (1.0 - 0.6 * Pale) * std::pow(std::max(0.0, 1.0 - After), 1.5);
					}
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

	void Battle::SetSky(double Red, double Green, double Blue)
	{
		const double Share = SafeNum(Num("linger.sky_share", 0.9), 0.3, 1.0, 0.9);
		const double SkyR = SafeNum(Red, 0.0, 255.0, 0.0), SkyG = SafeNum(Green, 0.0, 255.0, 0.0), SkyB = SafeNum(Blue, 0.0, 255.0, 0.0);
		bSkyKnown = SkyR + SkyG + SkyB > 1.0;
		const LandTable Land = LandTableOf();
		SkyTail = Rgb::FromPicture(Land.ToLand(SkyR * Share), Land.ToLand(SkyG * Share), Land.ToLand(SkyB * Share));
	}

	const Battle::LingerRow& Battle::LingerOf(const std::string& Kind, const std::string& Name)
	{
		const std::string Key = Kind + "/" + Name;
		const auto Found = LingerKept.find(Key);
		if (Found != LingerKept.end())
		{
			return Found->second;
		}
		LingerRow R;
		const std::string Q = "linger." + Kind + ".";
		const std::string Times = Name.empty() ? Q : Q + "by." + Name + ".";
		R.ThickS = SafeNum(Num(Times + "thick_s", 0.0), 0.0, 120.0, 0.0);
		R.GoneS = SafeNum(Num(Times + "gone_s", 0.0), 0.0, 240.0, 0.0);
		R.bOn = Has(Times + "gone_s") && R.GoneS > R.ThickS + 0.1;
		if (R.bOn)
		{
			// (each number its own under by.<name> where that is given, else the kind's)
			auto Asked = [&](const char* Of, double Usual) { return Num(Times + Of, Num(Q + Of, Usual)); };
			R.Keep = SafeNum(Asked("keep", 0.5), 0.05, 1.0, 0.5);
			R.GoBy = SafeNum(Asked("go_by", 0.4), 0.05, 1.0, 0.4);
			R.Third = SafeNum(Asked("left_third", 0.1), 1.0e-4, 1.0, 0.1);
			R.TwoThirds = SafeNum(Asked("left_two_thirds", 0.02), 1.0e-6, R.Third, std::min(0.02, R.Third));
			R.Wide = SafeNum(Asked("wide", 0.0), 0.0, 4.0, 0.0);
			R.Spread = SafeNum(Asked("spread", 0.0), 0.0, 4.0, 0.0);
			R.Lift = SafeNum(Asked("lift_mps", 0.0), 0.0, 5.0, 0.0);
			R.Pale = SafeNum(Asked("pale", 0.0), 0.0, 1.0, 0.0);
			R.PaleFrom = SafeNum(Asked("pale_from", 0.33), 0.0, 0.95, 0.33);
			R.PaleBy = SafeNum(Asked("pale_by", 0.8), R.PaleFrom + 0.01, 1.0, std::max(0.8, R.PaleFrom + 0.01));
			R.To = Picture(Q + "to", Picture("linger.to", Rgb::FromPicture(150, 156, 164)));
			R.StayLow = SafeNum(Asked("stay_low", 0.0), 0.0, 1.0, 0.0);
			R.BulkThird = SafeNum(Asked("bulk_left_third", R.Third), 1.0e-4, 1.0, R.Third);
			R.BulkTwoThirds = SafeNum(Asked("bulk_left_two_thirds", std::min(R.TwoThirds, R.BulkThird)), 1.0e-6, R.BulkThird, std::min(R.TwoThirds, R.BulkThird));
		}
		if (LingerKept.size() > 256)
		{
			LingerKept.clear(); // (a recipe asked for under endless names: never more rows than a few hundred)
		}
		return LingerKept.emplace(Key, R).first->second;
	}

	double Battle::LingerSpare(size_t Own) const
	{
		const size_t Asked = RoomGate();
		const double Full = static_cast<double>(Asked - std::min(Asked, Own)) / std::max(1, MaxPuffs);
		return std::clamp(1.0 - (1.0 - LingerSpareLeast) * (Full - LingerSpareFrom) / std::max(LingerSpareBy - LingerSpareFrom, 0.01), LingerSpareLeast, 1.0);
	}

	void Battle::Lingering(Bit& B, const LingerRow& R, double T0, const Vec& Mid, double MidWindage, double Tall, double Spare, double KeepK, double StayBias)
	{
		if (!R.bOn || !(B.Life > 0.0) || !(B.Life < 1.0e6) || B.bLight)
		{
			return;
		}
		// (which puffs stay: by the count of the puffs this rule has seen, so no dice are thrown and nothing else in the
		// burst moves)
		const uint64_t Thrown = Mix64((++LingerNo) * 0x9E3779B97F4A7C15ull ^ 0x51ED270B0F2Dull);
		const double One = static_cast<double>(Thrown >> 11) * (1.0 / 9007199254740992.0);
		// (one keep and one go_by for the whole cloud, less of both when the room is short: LingerSpare)
		const double Room = SafeNum(Spare, 0.05, 1.0, 1.0);
		// (KeepK: a part of a cloud that keeps more or fewer of its puffs than the row says, a skirt of dust. StayBias: a
		// puff that much the likelier to stay than its cloud's share, one at a burst's foot. Both 1: as the row says)
		const double Kept = std::clamp(R.Keep * Room * SafeNum(KeepK, 0.05, 4.0, 1.0), 0.05, 1.0), GoBy = std::clamp(R.GoBy * Room, 0.05, 1.0);
		const bool bStays = One < Kept * SafeNum(StayBias, 0.0, 4.0, 1.0);
		const double Late = std::clamp(B.Born - T0, 0.0, R.GoneS); // (born a moment after the burst, it has that much less of each time)
		const double Tail = R.GoneS - R.ThickS;
		const double Was = B.Life;
		const double Lives = std::max((bStays ? R.GoneS : R.ThickS + Tail * GoBy) - Late, 0.25);
		const double Thick = std::clamp(R.ThickS - Late, 0.0, 0.95 * Lives);
		// (as it was laid until the cloud's thick time: its looks keep the clock they had, see Bit::bTail)
		B.LookLife = Was;
		B.LookHold = B.Hold;
		B.Life = Lives;
		B.Hold = std::clamp(Thick / Lives, 0.0, 0.95);
		B.bTail = true;
		B.TailThird = R.Third;
		B.TailTwoThirds = R.TwoThirds;
		B.TailKeep = Kept;
		B.TailGoes = GoBy;
		B.TailGoBy = bStays ? 0.0 : GoBy;
		B.TailWide = R.Wide;
		B.TailSpread = R.Spread;
		B.TailMid = Mid;
		B.TailMidWindage = SafeNum(MidWindage, 0.0, 2.0, 0.75);
		B.TailLift = R.Lift * (0.75 + 0.5 * static_cast<double>(Mix64(Thrown ^ 0xD1B54A32D192ED03ull) >> 11) * (1.0 / 9007199254740992.0)) * (Tall > 0.0 ? std::clamp((B.From.Z - Mid.Z) / Tall, 0.0, 1.5) : 1.0);
		B.TailPale = R.Pale;
		B.TailPaleFrom = R.PaleFrom;
		B.TailPaleBy = R.PaleBy;
		B.TailCol = R.To;
	}

	void Battle::LingerFrom(size_t First, const LingerRow& RowIn, double T0, const Vec& Mid, double MidWindage, double Tall)
	{
		if (!RowIn.bOn)
		{
			return;
		}
		auto Lingers = [](const Bit& B) { return !(B.bLight || B.bTail || B.bThrown || B.Gravity > 0.0 || (B.Form != Bit::Shape::Puff && B.Form != Bit::Shape::Upright) || B.Ribbon > 0.0) && B.Life > 0.0 && B.Life < 1.0e6; };
		// (where each will be when the cloud's tail begins: a muzzle's smoke is thrown down the line of fire and an
		// aircraft's is carried on with it, so the cloud's middle is found from the puffs themselves, and the cloud
		// then grows about that, carried by the wind)
		// (a cloud is gone no sooner than its longest puff was before the rule: see Smoke)
		LingerRow RowOwn = RowIn;
		for (size_t I = First; I < Bits.size(); ++I)
		{
			const Bit& B = Bits[I];
			if (Lingers(B))
			{
				RowOwn.GoneS = std::max(RowOwn.GoneS, SafeNum(std::max(B.Born - T0, 0.0) + B.Life, 0.0, 3600.0, 0.0));
			}
		}
		const LingerRow& R = RowOwn;
		auto TailStarts = [&](const Bit& B) { return std::max(R.ThickS - std::clamp(B.Born - T0, 0.0, R.GoneS), 0.0); };
		Vec Middle;
		int Among = 0;
		for (size_t I = First; I < Bits.size(); ++I)
		{
			const Bit& B = Bits[I];
			if (Lingers(B))
			{
				Middle = Middle + Where(B, TailStarts(B));
				++Among;
			}
		}
		// (the pre-check on 86d71f2: a middle beyond any field, an aircraft's speed given as infinite, is no middle either:
		// each puff's place in its tail was then infinity less infinity, no number in the picture)
		if (Among < 1 || !(std::fabs(Middle.X) <= 1.0e7 * Among) || !(std::fabs(Middle.Y) <= 1.0e7 * Among))
		{
			return;
		}
		Middle = Middle * (1.0 / Among);
		const double Carried = SafeNum(MidWindage, 0.0, 2.0, 0.75);
		const double Spare = LingerSpare(Bits.size() - std::min(Bits.size(), First)); // (one for the whole cloud)
		for (size_t I = First; I < Bits.size(); ++I)
		{
			Bit& B = Bits[I];
			if (!Lingers(B))
			{
				continue;
			}
			const double Starts = TailStarts(B);
			Lingering(B, R, T0, Mid, MidWindage, Tall, Spare);
			if (B.bTail)
			{
				B.TailMid.X = Middle.X - Wind.X * (Carried * Starts);
				B.TailMid.Y = Middle.Y - Wind.Y * (Carried * Starts);
			}
			if (B.bCarried)
			{
				B.TailSpread = 0.0; // (born with a ship's way on it: its cloud's middle is not where the wind alone would have it)
			}
		}
	}

	void Battle::FuelBall(double Now, const Vec& AtIn, double AcrossIn, const Vec& VelIn)
	{
		// fuel_ball, the art director's ruling of 21 November (section 2): bursts are earth and smoke, never a petrol
		// fireball; "rolling fireballs only from fuel: a vehicle 6 to 10 m for 1 to 1.5 s, an aircraft crash or a dump
		// 15 to 25 m for 1.5 to 2 s", the black smoke boiling from its top after half a second. Fire as the air
		// fireball's was ruled (06:00Z 20 November): lit from within, a near white-yellow heart for its first moment,
		// a saturated orange body cooling to deep red at its rims and into black smoke, never pale peach or salmon,
		// each lump near opaque to its edge. Its lumps are thrown out and up from the seat, slowed by the air, and the
		// whole climbs faster as it goes (the hot gas rising), each lump rolling as it climbs
		const std::string P = "fuel_ball.";
		if (!Has(P + "lobes.0") || !(Now == Now) || std::fabs(Now) > 1.0e9 || !IsAPlace(AtIn))
		{
			return;
		}
		auto Held = [&](const std::string& Path, double A, double B, double Least, double Most)
		{
			Span S = Range(Path, A, B);
			S.A = SafeNum(S.A, Least, Most, A);
			S.B = SafeNum(S.B, Least, Most, B);
			if (S.B < S.A)
			{
				std::swap(S.A, S.B);
			}
			return S;
		};
		const double Across = SafeNum(AcrossIn, 1.0, 60.0, 8.0);
		const Vec Vel = IsAPlace(VelIn) && VelIn.Length() < 400.0 ? VelIn : Vec();
		Dice D(++Seeds * 6151ull ^ PlaceKey(AtIn));
		const size_t First = Bits.size();
		// (its time by its width: small_s at small_m across, big_s at big_m, between them in proportion)
		const double SmallM = SafeNum(Num(P + "small_m", 8.0), 0.5, 100.0, 8.0), BigM = std::max(SmallM + 0.5, SafeNum(Num(P + "big_m", 20.0), 0.5, 200.0, 20.0));
		const Span SmallS = Held(P + "small_s", 1.0, 1.5, 0.1, 10.0), BigS = Held(P + "big_s", 1.5, 2.0, 0.1, 10.0);
		const double Kw = Clamp01((Across - SmallM) / (BigM - SmallM));
		const double FireS = (SmallS.A + (BigS.A - SmallS.A) * Kw) + D.U() * ((SmallS.B - SmallS.A) + ((BigS.B - SmallS.B) - (BigS.A - SmallS.A)) * Kw);
		const double Rc = 0.5 * Across;
		const double Ground = Height(AtIn.X, AtIn.Y);
		const Vec At(AtIn.X, AtIn.Y, std::max(AtIn.Z, Ground == Ground ? Ground : AtIn.Z));
		// (how far the whole climbs while it burns, rise_of_across of its width: from rest, faster and faster)
		const double Lift = SafeNum(Num(P + "rise_of_across", 0.7), 0.0, 5.0, 0.7) * Across;
		const double Buoy = 2.0 * Lift / std::max(FireS * FireS, 1e-3);
		const Rgb FireCol = Picture(P + "colour", Rgb::FromPicture(250, 136, 22));
		const Rgb HotCol = Picture(P + "hot_colour", Rgb::FromPicture(255, 222, 90));
		const Rgb EmberCol = Picture(P + "ember", Rgb::FromPicture(140, 40, 16));
		const Rgb EdgeCol = Picture(P + "edge_colour", EmberCol);
		const Rgb SmokeCol = Picture(P + "smoke_colour", Rgb::FromPicture(34, 31, 29));
		const Rgb SmokeOld = Picture(P + "smoke_old", Rgb::FromPicture(96, 92, 88));
		const Span HeartMix = Held(P + "heart_mix", 0.1, 0.5, 0.0, 1.0);
		const Span Reach = Held(P + "reach", 0.3, 0.9, 0.0, 3.0), LobeR = Held(P + "lobe_r", 0.2, 0.32, 0.01, 2.0), LobeGrow = Held(P + "lobe_grow", 1.6, 2.2, 1.0, 10.0);
		const Span Roll = Held(P + "roll_rad_s", 1.5, 3.0, 0.0, 30.0);
		const double Edge = SafeNum(Num(P + "edge", 0.5), 0.0, 1.0, 0.5), Rim = SafeNum(Num(P + "rim", 0.85), 0.0, 1.0, 0.85);
		const double Rough = SafeNum(Num(P + "rough", 0.4), 0.0, 1.0, 0.4), Cool = SafeNum(Num(P + "cool", 1.3), 0.0, 20.0, 1.3);
		const double Over = SafeNum(Num(P + "over", 1.3), 1.0, 4.0, 1.3), TwinPx = SafeNum(Num(P + "twin_px", 40.0), 0.0, 1000.0, 40.0);
		const Span Lobes = Held(P + "lobes", 16.0, 64.0, 1.0, 200.0);
		const int N = static_cast<int>(std::lround(std::clamp(SafeNum(Num(P + "lobes_per_m", 3.0), 0.0, 50.0, 3.0) * Across, Lobes.A, Lobes.B)));
		const double SmokeFrom = SafeNum(Num(P + "smoke_from_s", 0.5), 0.0, 10.0, 0.5);
		const Span SmokeGrow = Held(P + "smoke_grow", 2.5, 3.5, 1.0, 20.0);
		const double SmokeAlpha = SafeNum(Num(P + "smoke_alpha", 0.9), 0.0, 1.0, 0.9), SmokeRise = SafeNum(Num(P + "smoke_rise_mps", 1.2), 0.0, 50.0, 1.2);
		const double SmokeRough = SafeNum(Num(P + "smoke_rough", 0.5), 0.0, 1.0, 0.5);
		const Span SmokeS = Held(P + "smoke_s", 12.0, 20.0, 0.5, 600.0);
		const Span LifeShare = Held(P + "life_share", 0.7, 1.0, 0.05, 1.0); // (each lump burns this share of the ball's time)
		const double DeepMix = SafeNum(Num(P + "deep_mix", 0.0), 0.0, 1.0, 0.0);
		// (art director 08:08Z 22 November: lit from within, the brightest thing in the frame. cool_from: the share of
		// its life a lump keeps its colour before it cools toward ember, so the middle holds yellow-orange through its
		// first half second; soot_life: the share of its life a lump goes to smoke, the first at the ball's top and the
		// second at its foot, so the dark comes in on top and the middle burns on)
		const double CoolFrom = SafeNum(Num(P + "cool_from", 0.0), 0.0, 0.95, 0.0);
		const Span SootLife = Held(P + "soot_life", 0.7, 0.7, 0.05, 0.95);
		// (the big one's cloud by cap_s: each puff's way out from the ball's upright cap_spread further again, level
		// with the ground, what of it stood above the ball's middle cap_flat lower, so the top flattens and spreads; and
		// leaning lean_deg down the wind, by its height over the ground, in a wind of lean_full_mps or more (less in
		// less, upright in none). smoke_climb: the share of its lump's climb a puff keeps, the first from the ball's
		// foot and the second from its top, so the cloud trails a stem)
		const double CapSpread = SafeNum(Num(P + "cap_spread", 0.0), 0.0, 5.0, 0.0), CapFlat = SafeNum(Num(P + "cap_flat", 0.0), 0.0, 1.0, 0.0);
		const double CapS = SafeNum(Num(P + "cap_s", 2.5), 0.1, 30.0, 2.5);
		const double LeanTan = std::tan(SafeNum(Num(P + "lean_deg", 0.0), 0.0, 60.0, 0.0) * Pi / 180.0);
		const double WindNow = std::hypot(Wind.X, Wind.Y);
		const double LeanBy = LeanTan * Clamp01(WindNow / SafeNum(Num(P + "lean_full_mps", 1.5), 0.01, 100.0, 1.5));
		const Vec Downwind = WindNow > 1e-6 ? Vec(Wind.X / WindNow, Wind.Y / WindNow, 0.0) : Vec();
		const Span Climb = Held(P + "smoke_climb", 0.15, 0.7, 0.0, 2.0);
		const bool bClimbByTop = Has(P + "smoke_climb.0");
		// (and a lens the ball's own: within lens_near_of_across of its width it fades as game.lens_cap says, never black)
		const double LensNear = SafeNum(Num(P + "lens_near_of_across", 0.0), 0.0, 20.0, 0.0) * Across;
		// (soot_in_s over 0: a lump's smoke is whole that many seconds after it is born, and the lump goes then)
		const double SootIn = SafeNum(Num(P + "soot_in_s", 0.0), 0.0, 1.0, 0.0);
		const bool bQuickSoot = SootIn > 0.0;
		// (heart_in: how much more of hot_colour a lump at the middle takes than one at the rim; core_nearer: the middle's
		// lumps sorted this share of the half width nearer the eye, so from any side the hot middle is seen through
		// the cooler lumps round it, lit from within)
		const double HeartIn = SafeNum(Num(P + "heart_in", 0.4), 0.0, 1.0, 0.4), CoreNearer = SafeNum(Num(P + "core_nearer", 0.0), 0.0, 2.0, 0.0);
		const Vec Seat = At + ZUp * (0.3 * Rc);
		auto AxisAt = [&](double T) { return Seat + Vel * (0.5 * (1.0 - std::exp(-3.5 * T)) / 3.5) + Wind * (0.3 * T); };
		const double CapRaise = SafeNum(Num(P + "cap_raise", 0.0), 0.0, 1.0, 0.0);
		const double Ceiling = Seat.Z + 0.4 * Rc + 0.5 * Buoy * FireS * FireS + SafeNum(Num(P + "cap_top", 0.5), -5.0, 20.0, 0.5) * Rc;
		auto Shaped = [&](Bit& M, double T)
		{
			// (T: seconds after Now it is born. Its way, slowed by the air, is spent by about cap_s)
			const double K = M.Drag > 1e-6 ? M.Drag / std::max(1.0 - std::exp(-M.Drag * CapS), 1e-6) : 1.0 / CapS;
			const Vec Off = M.From - AxisAt(T);
			// (the top: where its way would carry its top over the ceiling, the ball's middle as it went to smoke and cap_top
			// of its half width over that, it is held cap_flat of that much lower, so the cloud's top flattens under it)
			const double Reaches = M.From.Z + M.Vel.Z / K + M.R0 + (M.R1 - M.R0) * std::pow(Clamp01(CapS / std::max(M.Life, 1e-3)), M.Grow); // (its top, as wide as it will be by then)
			const double OverTop = Reaches - Ceiling;
			// (and cap_raise: what of it stood above the ball's middle as it went to smoke is carried that share of the
			// way up under the ceiling, the more the further out it is, so the cloud's shoulders come up level with its
			// crown: a cap, never a ball with its top cut off)
			const double Above = M.From.Z - (Seat.Z + 0.4 * Rc + 0.5 * Buoy * T * T);
			const double Lifts = OverTop < 0.0 && Above > 0.0 ? -OverTop * CapRaise * Clamp01(Above / std::max(0.5 * Rc, 1e-3)) * Clamp01(std::hypot(Off.X, Off.Y) / std::max(0.5 * Rc, 1e-3)) : 0.0;
			M.Vel = M.Vel + Vec(Off.X, Off.Y, 0.0) * (CapSpread * K) + ZUp * ((Lifts - std::max(OverTop, 0.0) * CapFlat) * K);
			M.Vel = M.Vel + Downwind * (LeanBy * std::max(M.From.Z - At.Z, 0.0) * K);
			M.LensNear = LensNear;
		};
		for (int I = 0; I < N; ++I)
		{
			// (thrown out and up from the seat, most of them upward: the ball stands on the ground and climbs off it)
			const Vec Toward = Vec(D.Bell(), D.Bell(), 0.35 + 0.8 * std::fabs(D.Bell())).Unit(ZUp);
			const double Reached = Rc * Reach.Pick(D), Starts = Reached * D.U(0.05, 0.25);
			Bit B;
			B.Born = Now + D.U(0.0, 0.12) * FireS; // (it swells over its first tenth)
			// (no lump goes before its smoke can take it: each burns past smoke_from_s, so nothing of the ball fades out
			// over the bare sky, where thin fire reads pink)
			B.Life = std::max(FireS * LifeShare.Pick(D) - (B.Born - Now), (SmokeFrom + 0.15 - (B.Born - Now)) / 0.85);
			B.Drag = 3.5;
			B.From = Seat + Toward * Starts;
			B.Vel = Vel * 0.5 + Toward * ((Reached - Starts) * B.Drag / (1.0 - std::exp(-B.Drag * B.Life)));
			B.Gravity = -Buoy * D.U(0.8, 1.2); // (it climbs faster and faster: the hot gas rising)
			B.Windage = 0.3;
			const double In = 1.0 - Clamp01(Reached / std::max(Rc * Reach.B, 1e-3)); // (1 at the middle, 0 at the far rim)
			const double Top = Clamp01(Toward.Z * Reached / std::max(Rc * Reach.B, 1e-3)); // (1 thrown straight up to the full reach, 0 at the seat)
			B.R0 = Rc * LobeR.Pick(D);
			B.R1 = B.R0 * LobeGrow.Pick(D);
			B.Grow = 0.5;
			B.Col = FireCol.Mix(HotCol, Clamp01(HeartMix.Pick(D) + HeartIn * In));
			if (DeepMix > 0.0)
			{
				B.Col = B.Col.Mix(EdgeCol, D.U(0.0, DeepMix) * (1.0 - In)); // (some lumps toward the outside a deeper orange-red: billows, never one flat orange)
			}
			B.Col2 = EmberCol;
			B.Cool = Cool;
			B.CoolFrom = CoolFrom;
			B.Alpha = 1.0;
			B.FadeIn = 0.01;
			B.Hold = 0.92; // (whole until its smoke has it, then gone in a frame or two: a see-through fade over the sky is pink)
			B.LensNear = LensNear;
			B.Heart = 1.0;
			B.Rim = Rim;
			B.Rough = Rough;
			B.Edge = EdgeCol;
			B.EdgeK = Edge;
			B.TwinPx = TwinPx;
			B.Over = 1.0 + (Over - 1.0) * In; // (its middle burns brighter)
			B.Nearer = CoreNearer * In * Rc;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.TurnRate = Roll.Pick(D) * (Toward.X >= 0.0 ? -1.0 : 1.0); // (its lumps roll out and over as it climbs)
			B.bLight = true;
			B.bWhole = true;
			// (the black smoke each lump goes into as it dies, never before smoke_from_s: born round it a little higher,
			// so the smoke boils off the ball's top and the fire is never seen fading over the bare sky)
			const double Dies = std::max((SootLife.A + (SootLife.B - SootLife.A) * (1.0 - Top)) * B.Life, Now + SmokeFrom - B.Born);
			if (Dies >= 0.96 * B.Life)
			{
				Bits.push_back(B);
			}
			else
			{
				Bit M;
				M.Born = B.Born + Dies;
				M.From = Where(B, Dies) + ZUp * (0.25 * B.R1);
				const Vec Was = Going(B, Dies);
				// (each keeps its own share of the ball's climb, and its lumps drift apart: a ragged cloud trailing a dark
				// stem back toward the ground, never one black ball lifting off)
				const double Keeps = bClimbByTop ? (Climb.A + (Climb.B - Climb.A) * Top) * D.U(0.85, 1.15) : Climb.Pick(D);
				M.Vel = (IsAPlace(Was) ? Was * Keeps : Vec()) + Vec(D.Bell(), D.Bell(), 0.4 * D.Bell()) * (0.18 * Rc);
				M.Drag = 1.2;
				M.Rise = SmokeRise * D.U(0.7, 1.3);
				M.Windage = D.U(0.8, 1.1);
				M.Life = SmokeS.Pick(D);
				M.R0 = 1.1 * (B.R0 + (B.R1 - B.R0) * std::sqrt(Dies / B.Life));
				M.R1 = M.R0 * SmokeGrow.Pick(D);
				M.Grow = 0.45;
				M.Col = SmokeCol;
				M.Col2 = SmokeCol.Mix(SmokeOld, 0.7);
				M.Alpha = SmokeAlpha * D.U(0.85, 1.05);
				M.FadeIn = std::min(0.05, 0.15 / std::max(M.Life, 0.5));
				M.Hold = 0.3;
				M.Heart = 0.75;
				M.Rim = 0.0;
				M.Rough = SmokeRough;
				M.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
				M.Turn = D.U(0.0, 2.0 * Pi);
				M.TurnRate = 0.3 * B.TurnRate;
				M.LeastPx = 2.0;
				M.LeastKeep = 0.4;
				if (bQuickSoot)
				{
					// (art director 08:08Z: the lump goes out the moment its smoke has it, whole until then and gone in a
					// frame, so a lump is never seen fading pink over the sky as its smoke moves off. Its looks keep the
					// clock they had: its widening and its cooling as over its whole life)
					M.FadeIn = std::min(0.5, SootIn / std::max(M.Life, 0.5));
					const double Was0 = B.Life, Now0 = std::min(Dies + SootIn + 0.03, B.Life);
					const double Ratio = Now0 / Was0;
					B.R1 = B.R0 + (B.R1 - B.R0) * std::pow(Ratio, B.Grow);
					if (B.CoolFrom > 0.0 && Ratio <= B.CoolFrom)
					{
						B.Cool = 0.0; // (it goes before it would have begun to cool)
					}
					else if (B.CoolFrom > 0.0)
					{
						const double From = B.CoolFrom / Ratio;
						B.Cool *= (Ratio - B.CoolFrom) / (1.0 - B.CoolFrom);
						B.CoolFrom = std::min(From, 0.95);
					}
					else
					{
						B.Cool *= Ratio;
					}
					B.Life = Now0;
					B.Hold = std::min(0.999, (Dies + SootIn) / Now0);
				}
				Bits.push_back(B);
				Shaped(M, M.Born - Now);
				Bits.push_back(M);
			}
		}
		// (top_puffs_per_m: black smoke boiling off the ball's top from smoke_from_s while it still burns below, born at
		// the top as the ball has climbed by then and climbing on with it, so the fire wears a black cap that grows)
		const int Tops = static_cast<int>(std::lround(std::clamp(SafeNum(Num(P + "top_puffs_per_m", 0.0), 0.0, 20.0, 0.0) * Across, 0.0, 200.0)));
		const double TopClimb = SafeNum(Num(P + "top_climb", 0.6), 0.0, 2.0, 0.6); // (the share of the ball's climb then that each keeps)
		for (int I = 0; I < Tops && SmokeFrom < FireS; ++I)
		{
			const double T = SmokeFrom + (FireS - SmokeFrom) * (I + D.U()) / Tops;
			const Vec Side = Vec(D.Bell(), D.Bell(), 0.0) * (0.35 * Rc);
			Bit M;
			M.Born = Now + T;
			M.From = Seat + Vel * (0.5 * (1.0 - std::exp(-3.5 * T)) / 3.5) + ZUp * (0.5 * Buoy * T * T + Rc * D.U(0.6, 0.95)) + Side;
			M.Vel = ZUp * (Buoy * T * TopClimb) + Side * 0.5;
			M.Drag = 1.0;
			M.Rise = SmokeRise * D.U(0.8, 1.3);
			M.Windage = D.U(0.8, 1.1);
			M.Life = SmokeS.Pick(D);
			M.R0 = Rc * LobeR.Pick(D) * 1.2;
			M.R1 = M.R0 * SmokeGrow.Pick(D) * 1.3;
			M.Grow = 0.45;
			M.Col = SmokeCol;
			M.Col2 = SmokeCol.Mix(SmokeOld, 0.7);
			M.Alpha = SmokeAlpha * D.U(0.85, 1.05);
			M.FadeIn = std::min(0.05, 0.2 / std::max(M.Life, 0.5));
			M.Hold = 0.3;
			M.Heart = 0.75;
			M.Rim = 0.0;
			M.Rough = SmokeRough;
			M.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			M.Turn = D.U(0.0, 2.0 * Pi);
			M.TurnRate = D.U(-0.6, 0.6);
			M.LeastPx = 2.0;
			M.LeastKeep = 0.4;
			Shaped(M, T);
			Bits.push_back(M);
		}
		// (stem_puffs_per_m: the dark stem from the fire on the ground up into the cloud, art director 08:08Z: puffs born
		// at the seat from smoke_from_s to stem_s, each climbing to its own height between the ground and where the
		// ball's middle was as it went to smoke, stem_r of the ball's half width and widening stem_grow, leaning with
		// the cloud)
		const int Stems = static_cast<int>(std::lround(std::clamp(SafeNum(Num(P + "stem_puffs_per_m", 0.0), 0.0, 20.0, 0.0) * Across, 0.0, 200.0)));
		const double StemS = std::max(SafeNum(Num(P + "stem_s", 3.0), 0.0, 60.0, 3.0), SmokeFrom + 0.1);
		const Span StemR = Held(P + "stem_r", 0.25, 0.4, 0.01, 5.0), StemGrow = Held(P + "stem_grow", 1.8, 2.6, 1.0, 20.0);
		const double CapZ = Seat.Z + 0.4 * Rc + 0.5 * Buoy * FireS * FireS - At.Z; // (how high the ball's middle was as it went to smoke)
		for (int I = 0; I < Stems; ++I)
		{
			const double T = SmokeFrom + (StemS - SmokeFrom) * (I + D.U()) / Stems;
			const double Reaches = CapZ * std::sqrt(D.U(0.0, 1.0)) * D.U(0.8, 1.05); // (more of them high: the stem is thin at its foot)
			Bit M;
			M.Born = Now + T;
			M.From = AxisAt(std::min(T, FireS)) + Vec(D.Bell(), D.Bell(), 0.0) * (0.12 * Rc);
			M.From.Z = At.Z + 0.2 * Rc;
			M.Drag = 1.2;
			M.Vel = ZUp * (Reaches * M.Drag / std::max(1.0 - std::exp(-M.Drag * CapS), 1e-6)) + Vec(D.Bell(), D.Bell(), 0.0) * (0.1 * Rc);
			M.Rise = SmokeRise * D.U(0.7, 1.1);
			M.Windage = D.U(0.8, 1.1);
			M.Life = SmokeS.Pick(D);
			M.R0 = Rc * StemR.Pick(D);
			M.R1 = M.R0 * StemGrow.Pick(D);
			M.Grow = 0.45;
			M.Col = SmokeCol;
			M.Col2 = SmokeCol.Mix(SmokeOld, 0.7);
			M.Alpha = SmokeAlpha * D.U(0.8, 1.0);
			M.FadeIn = std::min(0.08, 0.3 / std::max(M.Life, 0.5));
			M.Hold = 0.3;
			M.Heart = 0.75;
			M.Rim = 0.0;
			M.Rough = SmokeRough;
			M.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			M.Turn = D.U(0.0, 2.0 * Pi);
			M.TurnRate = D.U(-0.5, 0.5);
			M.LeastPx = 2.0;
			M.LeastKeep = 0.4;
			// (it leans as the cloud does, by the height it climbs to)
			const double K = M.Drag / std::max(1.0 - std::exp(-M.Drag * CapS), 1e-6);
			M.Vel = M.Vel + Downwind * (LeanBy * Reaches * K);
			M.LensNear = LensNear;
			Bits.push_back(M);
		}
		// (the white-hot heart, a few knots low in the middle for its first moment, drawn over the lumps round it)
		const Span HeartsIs = Held(P + "hearts", 3.0, 6.0, 0.0, 32.0);
		const int Hearts = static_cast<int>(std::lround(HeartsIs.Pick(D)));
		const double HeartS = SafeNum(Num(P + "heart_s", 0.18), 0.01, 2.0, 0.18), HeartR = SafeNum(Num(P + "heart_r", 0.3), 0.01, 2.0, 0.3);
		const Rgb HeartCol = Picture(P + "heart_colour", Rgb::FromPicture(255, 240, 170));
		for (int I = 0; I < Hearts; ++I)
		{
			const Vec Toward = Vec(D.Bell(), D.Bell(), 0.5 + std::fabs(D.Bell())).Unit(ZUp);
			Bit H;
			H.Born = Now;
			H.From = Seat + Toward * (Rc * D.U(0.0, 0.15));
			H.Vel = Vel * 0.5 + Toward * (Rc * D.U(0.4, 1.0)); // (it stays low inside the ball: never a pale shape over its top)
			H.Drag = 4.0;
			H.Gravity = -Buoy;
			H.Life = HeartS * D.U(0.8, 1.2);
			H.R0 = Rc * HeartR * D.U(0.6, 1.0);
			H.R1 = H.R0 * 1.6;
			H.Col = Rgb::FromPicture(255, 252, 236);
			H.Col2 = HeartCol;
			H.Edge = HotCol;
			H.EdgeK = 0.6;
			H.Alpha = 1.0;
			H.FadeIn = 0.002;
			H.Hold = 0.85;
			H.Heart = 1.0;
			H.Rim = Rim;
			H.Rough = 0.35;
			H.TwinPx = TwinPx; // (never a pale plate with straight sides seen close)
			H.Over = Over;
			H.Nearer = 0.5 * Rc;
			H.LensNear = LensNear;
			H.LumpSeed = static_cast<uint32_t>(D.U() * 1073741823.0);
			H.Turn = D.U(0.0, 2.0 * Pi);
			H.bLight = true;
			H.bWhole = true;
			Bits.push_back(H);
		}
		// (its smoke hangs and goes as the linger row's fuel says: thick a few seconds, then into the air)
		LingerFrom(First, LingerOf("fuel"), Now, At, 0.9, Across);
		// (and its lamp, while it burns)
		const double LampIs = SafeNum(Num(P + "lamp", 0.9), 0.0, 100.0, 0.9);
		if (LampIs > 0.0)
		{
			BallLamp L;
			L.At = Seat + ZUp * (0.3 * Rc);
			L.Vel = Vel * 0.5;
			L.Born = Now;
			L.Life = FireS;
			L.Strength = LampIs * std::sqrt(Across);
			L.Radius = SafeNum(Num(P + "lamp_reach", 4.0), 0.1, 100.0, 4.0) * Across;
			L.Lift = Buoy;
			L.Seed = static_cast<uint64_t>(D.U() * 4294967295.0) * 2654435761ull + 0xBA11ull;
			L.Col = FireCol.Mix(HotCol, 0.3);
			BallLamps.push_back(L);
		}
	}

	bool Battle::BallLampAt(const BallLamp& L, double Now, double Day, Lamp& Into) const
	{
		const double Age = Now - L.Born;
		if (!(Age >= 0.0) || Age > L.Life)
		{
			return false;
		}
		// (up over its first tenth as the ball swells, full, then down over its last two fifths as it goes to smoke)
		const double X = Age / std::max(L.Life, 1e-3);
		const double On = Clamp01(X / 0.1) * Clamp01((1.0 - X) / 0.4);
		const double ByDay = 1.0 + (DayLampShare - 1.0) * Clamp01(Day);
		Into.At = L.At + L.Vel * Age + ZUp * (0.5 * L.Lift * Age * Age);
		Into.Col = L.Col.Mix(DayLampCol, Clamp01(Day));
		Into.Strength = L.Strength * On * ByDay;
		Into.Radius = L.Radius;
		return Into.Strength > 1e-4;
	}

	double Battle::CutOf(uint32_t Run_, double Born, double Was) const
	{
		// (Bit::CutAt: of the cuts TrailMove asked since the last Draw, sorted by CutTrails, the soonest that reaches a
		// piece of that run laid no later than it)
		auto Low = std::lower_bound(TrailCuts.begin(), TrailCuts.end(), Run_, [](const TrailCut& C, uint32_t R) { return C.Run < R; });
		for (; Low != TrailCuts.end() && Low->Run == Run_; ++Low)
		{
			if (Born <= Low->LaidBy)
			{
				Was = std::min(Was, Low->End);
			}
		}
		return Was;
	}

	void Battle::CutTrails()
	{
		// (the cuts TrailMove asked since the last Draw: sorted here and laid on what the trails keep to reach back to;
		// the pieces take theirs in Prune's own pass over them, the pre-check's cost note on 5833912a: a pass of its own
		// over every piece cost about 2 ms a frame with 300 trails changing speed)
		if (TrailCuts.empty())
		{
			return;
		}
		std::sort(TrailCuts.begin(), TrailCuts.end(), [](const TrailCut& L, const TrailCut& R) { return L.Run != R.Run ? L.Run < R.Run : L.LaidBy < R.LaidBy; });
		for (Trailing& Tr : Trails)
		{
			Tr.Kept.CutAt = CutOf(Tr.Run, Tr.Kept.Born, Tr.Kept.CutAt);
			Tr.Prev.CutAt = CutOf(Tr.Run, Tr.Prev.Born, Tr.Prev.CutAt);
		}
	}

	void Battle::Prune(double Now)
	{
		auto Gone = [Now](const Bit& B) { return (Now > B.Born + B.Life || Now > B.CutAt) && (!Unseen(B) || Now > B.Born + 0.5); };
		// (the production lead's FX2: a piece is 1.8 kB, and erase(remove_if) moved every piece after the first one gone,
		// most of the battle's pieces each frame. Now each gone piece takes the last one in its place, a piece moved
		// for each gone. Nothing holds a place in Bits from one Draw to the next, and the order they are drawn in is
		// their depth and then Bit::Serial, so the picture does not depend on where they lie)
		const bool bCuts = !TrailCuts.empty(); // (CutTrails sorted them)
		for (size_t I = 0; I < Bits.size();)
		{
			if (bCuts && Bits[I].TrailRun != 0)
			{
				Bit& B = Bits[I];
				B.CutAt = CutOf(B.TrailRun, B.Born, B.CutAt);
				if (B.Back.bOn)
				{
					B.Back.CutAt = CutOf(B.TrailRun, B.Back.Born, B.Back.CutAt);
				}
			}
			if (!Gone(Bits[I]))
			{
				++I;
				continue;
			}
			if (I + 1 < Bits.size())
			{
				Bits[I] = Bits.back();
			}
			Bits.pop_back();
		}
		Numbered = Bits.size();
		TrailCuts.clear();
		BallLamps.erase(std::remove_if(BallLamps.begin(), BallLamps.end(), [Now](const BallLamp& L) { return !(Now <= L.Born + L.Life + 1.0); }), BallLamps.end());
		// (a flash goes once its lamp has had its two frames, or after half a second unseen)
		Flashes.erase(std::remove_if(Flashes.begin(), Flashes.end(), [this, Now](const Flash& F) { return (F.FirstFrame >= 0 && FrameNo - F.FirstFrame >= 2 && !(F.LampS > 0.0 && Now < F.Born + F.LampS)) || Now > F.Born + 0.5; }), Flashes.end()); // (a naval gun's, LampS: kept while its lamp goes down)
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
		return !Bits.empty() || !Marks.empty() || !Sources.empty() || !Flashes.empty() || !FlamesDue.empty() || !Sticks.empty();
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The far form: a stick of bombs (BombStick), and the far look of anything seen beyond the draw range (FarLay)

	void Battle::BombStick(double Now, int Id, const Vec& First, const Vec& Last, int Count, const char* SizeIn, const char* GroundIn, double Over)
	{
		GroundIn = GroundOn(GroundIn); // (a ground whose own switch is off is drawn as earth)
		// (a stick whose moment or either end is no number, or lies beyond any field, is no stick, as a shell's hit is not)
		const auto Sound = [](double V, double Most) { return V == V && V >= -Most && V <= Most; };
		if (!Sound(Now, 1.0e9) || !Sound(First.X, 1.0e7) || !Sound(First.Y, 1.0e7) || !Sound(First.Z, 1.0e7) || !Sound(Last.X, 1.0e7) || !Sound(Last.Y, 1.0e7) || !Sound(Last.Z, 1.0e7) || Count < 1)
		{
			return;
		}
		// (one stick under one number while its bombs land; and never more than a few hundred sticks waiting)
		for (const Stick& Was : Sticks)
		{
			if (Was.Id == Id)
			{
				return;
			}
		}
		if (Sticks.size() >= 256)
		{
			return;
		}
		Stick S;
		S.Id = Id;
		S.Count = std::min(Count, 48);
		S.Size = SizeIn ? SizeIn : "how105";
		if (!Has("sizes." + S.Size + ".scale"))
		{
			S.Size = "how105";
		}
		S.Ground = GroundIn ? GroundIn : "earth";
		if (!Has("grounds." + S.Ground + ".paint.soil.0"))
		{
			S.Ground = "earth";
		}
		// (along the ground, never more than 5 km of it; a stick with no length lands its bombs on one spot)
		const Vec Run(Last.X - First.X, Last.Y - First.Y, 0.0);
		const double Length = std::min(Run.Length(), 5000.0);
		S.First = First;
		S.Way = Run.Unit(Vec(1.0, 0.0, 0.0));
		S.Gap = S.Count > 1 ? Length / (S.Count - 1) : 0.0;
		S.Mid = First + S.Way * (0.5 * Length);
		const double Walk = SafeNum(Num("far.bombs.walk_mps", 85.0), 10.0, 400.0, 85.0);
		const double Takes = Over >= 0.0 && Over <= 1.0e9 ? std::min(Over, 30.0) : std::min(Length / Walk, 4.0);
		S.T0 = Now;
		S.Dt = S.Count > 1 ? Takes / (S.Count - 1) : 0.0;
		S.Seed = Mix64(FarSeed ^ (static_cast<uint64_t>(static_cast<uint32_t>(Id)) * 0x9E3779B97F4A7C15ull) ^ PlaceKey(First));
		Sticks.push_back(S);
	}

	void Battle::LayStrikes(double Now)
	{
		if (Sticks.empty() || !(Now == Now))
		{
			return;
		}
		// (every bomb due by now, the earliest first; of two at one moment, the stick called first)
		for (int Guard = 0; Guard < 256 * 48; ++Guard)
		{
			Stick* Next = nullptr;
			double Due = 0.0;
			for (Stick& S : Sticks)
			{
				const double T = S.T0 + S.Dt * S.Laid;
				if (S.Laid < S.Count && T <= Now && (Next == nullptr || T < Due))
				{
					Next = &S;
					Due = T;
				}
			}
			if (Next == nullptr)
			{
				break;
			}
			LayStrike(*Next, Due);
		}
		Sticks.erase(std::remove_if(Sticks.begin(), Sticks.end(), [](const Stick& S) { return S.Laid >= S.Count; }), Sticks.end());
	}

	void Battle::LayStrike(Stick& S, double T)
	{
		const int K = S.Laid++;
		const Vec Hit = S.First + S.Way * (S.Gap * K);
		const FarRow& R = FarRowOf("bombs");
		if (!R.bOn)
		{
			Shell(T, Hit, S.Size.c_str(), S.Ground.c_str()); // (no far row: the bomb as any shell, as before)
			return;
		}
		const Vec At(Hit.X, Hit.Y, Height(Hit.X, Hit.Y));
		// (near or far by the eye of the last frame: within the draw range, the near burst, which hands over to its far
		// form beyond the range; beyond it, the far form alone. A bomb laid before any frame is near)
		const bool bFar = bEyeKnown && (LastEye - At).Length() > Beyond;
		if (!bFar)
		{
			const size_t Near = Bits.size();
			Shell(T, Hit, S.Size.c_str(), S.Ground.c_str());
			for (size_t I = Near; I < Bits.size(); ++I)
			{
				Bits[I].FarRep = 1;
				Bits[I].FarAt = At;
			}
		}
		// (how many bombs one merging piece stands for: as many as stand within far.bombs.merge_px of each other on the
		// screen, by the stick's middle, set at its first bomb so the stick keeps one pattern)
		if (S.Step < 1)
		{
			S.Step = 1;
			const double Away = (LastEye - S.Mid).Length();
			if (R.MergePx > 0.0 && S.Gap > 0.0 && bEyeKnown && Away > 1.0)
			{
				const double Px = S.Gap * SafeNum(LastPpr, 1.0, 1.0e7, 1000.0) / Away;
				const double Asked = R.MergePx / std::max(Px, 1.0e-9);
				S.Step = Asked >= S.Count ? S.Count : std::max(1, static_cast<int>(std::ceil(Asked)));
			}
		}
		Dice D(S.Seed ^ (static_cast<uint64_t>(K) + 1ull) * 0xD1B54A32D192ED03ull);
		const std::string Sz = "sizes." + S.Size + ".";
		const double Scale = SafeNum(Num(Sz + "scale", 1.0), 0.01, 25.0, 1.0);
		FarSpot F;
		F.At = At;
		F.Along = S.Way;
		F.Born = T;
		F.Across = SafeNum(Num(Sz + "cloud_m", 30.0 * std::sqrt(Scale)), 1.0, 400.0, 30.0);
		F.FlashM = R.FlashM * SafeNum(Num(Sz + "flash_k", 1.0), 0.1, 10.0, 1.0);
		F.FlashS = SafeNum(Num(Sz + "flash_s", 0.1), 0.02, 0.5, 0.1) * R.FlashLong;
		F.LingerS = SafeNum(Pick(D, Sz + "linger_s", 15.0, 25.0) * Num("grounds." + S.Ground + ".haze_lingers", 1.0), 0.0, 3600.0, 20.0);
		F.Linger = S.Size;
		F.bWater = S.Ground == "water";
		F.bCarrier = K % S.Step == 0;
		const int Carries = std::min(S.Step, S.Count - K);
		F.Long = F.bCarrier ? (Carries - 1) * S.Gap : 0.0;
		F.LongS = F.bCarrier ? (Carries - 1) * S.Dt : 0.0;
		if (F.bWater)
		{
			const std::string W = "grounds.water.spout.";
			F.Dark = Picture(W + "base_colour", Rgb::FromPicture(58, 72, 80));
			F.Light = Picture(W + "colour", Rgb::FromPicture(240, 244, 246));
		}
		else
		{
			const Paint P = PaintOf(S.Ground);
			F.Dark = P.Heavy.Mix(P.Soil, 0.5);
			F.Light = P.Smoke.Mix(P.Dust, 0.35);
		}
		F.Seed = Mix64(S.Seed ^ (static_cast<uint64_t>(K) + 1ull) * 0x94D049BB133111EBull);
		FarLay(F, R, bFar ? 3 : 2);
	}

	Battle::FarPart Battle::FarPartOf(const std::string& Path) const
	{
		FarPart P;
		P.bOn = Has(Path + "alpha");
		if (!P.bOn)
		{
			return P;
		}
		auto Got = [&](const char* Key, double Least, double Most, double Usual) { return SafeNum(Num(Path + Key, Usual), Least, Most, Usual); };
		P.bUpright = Num(Path + "upright", 0.0) > 0.5;
		P.bMerges = Num(Path + "merges", 0.0) > 0.5;
		P.bLingers = Num(Path + "lingers", 0.0) > 0.5;
		P.R0 = Got("r.0", 0.0, 4.0, 0.2);
		P.R1 = Got("r.1", 0.01, 4.0, 0.5);
		P.Late = Got("late", 1.0, 6.0, 1.0);
		P.GrowS = Got("grow_s", 0.05, 120.0, 2.0);
		P.Grow = Got("grow", 0.1, 4.0, 0.5);
		P.Tall = Got("tall", 1.0, 8.0, 1.0);
		P.TallEnd = Got("tall_end", 1.0, 8.0, P.Tall);
		P.TallS = Got("tall_s", 0.0, 60.0, 0.0);
		P.SettleS = Got("settle_s", 0.05, 600.0, 1.0);
		P.Flat = Got("flat", 0.05, 1.0, 0.4);
		P.Up = Got("up", 0.0, 4.0, 0.5);
		P.Rise = Got("rise", -5.0, 20.0, 0.0);
		P.Windage = Got("windage", 0.0, 2.0, 0.75);
		P.Lift = Got("lift", 0.0, 4.0, 1.0);
		P.Alpha = Got("alpha", 0.0, 1.0, 0.5);
		P.Heart = Got("heart", 0.0, 1.0, 0.7);
		P.Rim = Got("rim", 0.0, 1.0, 0.1);
		P.Inner = Got("inner", 0.05, 0.95, 0.5);
		P.Rough = Got("rough", 0.0, 1.0, 0.2);
		P.Shade = Got("shade", 0.0, 2.0, 1.0);
		P.Light = Got("light", 0.0, 3.0, 1.0);
		P.FadeIn = Got("fade_in", 0.001, 30.0, 0.05);
		P.Hold = Got("hold", 0.0, 0.99, 0.4);
		P.PaintFrom = Got("paint.0", 0.0, 1.0, 0.5);
		P.PaintTo = Got("paint.1", 0.0, 1.0, P.PaintFrom);
		P.LeastPx = Got("least_px", 0.0, 64.0, 0.0);
		P.Keep = Got("keep", 0.0, 1.0, 0.5);
		P.LifeS = Got("life_s", 0.0, 600.0, 0.0);
		P.LifeThick = Got("life_thick", 0.0, 20.0, 0.0);
		P.Third = Got("left_third", 0.0, 1.0, 0.0);
		P.TwoThirds = Got("left_two_thirds", 0.0, P.Third > 0.0 ? P.Third : 1.0, 0.0);
		return P;
	}

	const Battle::FarRow& Battle::FarRowOf(const std::string& Kind)
	{
		const auto Found = FarRows.find(Kind);
		if (Found != FarRows.end())
		{
			return Found->second;
		}
		FarRow R;
		const std::string Q = "far." + Kind + ".", Fl = Q + "flash.";
		R.WalkMps = SafeNum(Num(Q + "walk_mps", 85.0), 10.0, 400.0, 85.0);
		R.MergePx = SafeNum(Num(Q + "merge_px", 0.0), 0.0, 64.0, 0.0);
		R.FlashPx = SafeNum(Num(Fl + "least_px", 0.0), 0.0, 64.0, 0.0);
		R.FlashKeep = SafeNum(Num(Fl + "keep", 0.9), 0.0, 1.0, 0.9);
		R.FlashOver = SafeNum(Num(Fl + "over", 1.0), 1.0, 4.0, 1.0);
		R.FlashM = SafeNum(Num(Fl + "width_m", 6.0), 0.1, 100.0, 6.0);
		R.FlashLong = SafeNum(Num(Fl + "long", 1.0), 0.2, 10.0, 1.0);
		R.FlashCol = Picture(Fl + "colour", Rgb::FromPicture(255, 232, 150));
		R.GlowPx = SafeNum(Num(Fl + "glow_px", 0.0), 0.0, 128.0, 0.0);
		R.GlowAlpha = SafeNum(Num(Fl + "glow_alpha", 0.0), 0.0, 1.0, 0.0);
		R.GlowS = SafeNum(Num(Fl + "glow_s", 0.1), 0.0, 2.0, 0.1);
		R.GlowWide = SafeNum(Num(Fl + "glow_wide", 2.0), 0.5, 10.0, 2.0);
		R.Crown = FarPartOf(Q + "crown.");
		R.Body = FarPartOf(Q + "body.");
		R.Veil = FarPartOf(Q + "veil.");
		R.Skirt = FarPartOf(Q + "skirt.");
		R.Column = FarPartOf(Q + "column.");
		R.Foam = FarPartOf(Q + "foam.");
		R.Mist = FarPartOf(Q + "mist.");
		// (on only where the recipe has the far row and this kind draws something)
		R.bOn = Has("far.draw_beyond_m") && (R.FlashPx > 0.0 || R.Crown.bOn || R.Body.bOn || R.Veil.bOn || R.Skirt.bOn || R.Column.bOn || R.Foam.bOn || R.Mist.bOn);
		if (FarRows.size() > 64)
		{
			FarRows.clear(); // (never more than a few dozen kinds)
		}
		return FarRows.emplace(Kind, R).first->second;
	}

	void Battle::FarLay(const FarSpot& S, const FarRow& R, uint8_t Rep)
	{
		if (!R.bOn || !RealPlace(S.At) || !(S.Born == S.Born) || std::fabs(S.Born) > 1.0e9)
		{
			return;
		}
		const size_t From = Bits.size();
		Dice D(S.Seed);
		const double Wide = SafeNum(S.Across, 1.0, 2000.0, 30.0);
		const double RunLong = SafeNum(S.Long, 0.0, 1.0e4, 0.0), RunS = SafeNum(S.LongS, 0.0, 600.0, 0.0);
		const Vec Way = Vec(S.Along.X, S.Along.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		// The flash: a light held to flash.least_px on the screen, keeping flash.keep of its strength there and burning
		// flash.over past white, its size's own width and time; and a soft glow of its colour over it, held to glow_px.
		// Both drawn over the bomb's own crown and body, born with them (sorted the cloud's width nearer the eye)
		const double FlashM = SafeNum(S.FlashM, 0.0, 400.0, 0.0), FlashS = SafeNum(S.FlashS, 0.0, 5.0, 0.0);
		if (FlashM > 0.0 && FlashS > 0.0)
		{
			Bit B;
			B.Born = S.Born;
			B.From = S.At + ZUp * (0.3 * FlashM);
			B.Life = FlashS;
			B.R0 = 0.35 * FlashM;
			B.R1 = 0.5 * FlashM;
			B.Grow = 0.4;
			B.Col = R.FlashCol;
			B.Col2 = R.FlashCol * 0.55;
			B.Alpha = 1.0;
			B.FadeIn = 0.004;
			B.Hold = 0.35;
			B.Heart = 0.8;
			B.Rim = 0.1;
			B.bLight = true;
			B.bWhole = true;
			B.LeastPx = R.FlashPx;
			B.LeastKeep = R.FlashKeep;
			B.Over = R.FlashOver;
			B.Nearer = Wide;
			Bits.push_back(B);
			if (R.GlowAlpha > 0.0)
			{
				Bit G;
				G.Born = S.Born;
				G.From = B.From;
				G.Life = FlashS + R.GlowS;
				G.R0 = G.R1 = 0.5 * R.GlowWide * FlashM;
				G.Col = R.FlashCol;
				G.Col2 = R.FlashCol;
				G.Alpha = R.GlowAlpha;
				G.FadeIn = 0.003;
				G.Hold = 0.35;
				G.Heart = 0.35;
				G.Rim = 0.0;
				G.Inner = 0.45;
				G.bLight = true;
				G.bWhole = true;
				G.LeastPx = R.GlowPx;
				G.LeastKeep = 1.0;
				G.Nearer = Wide;
				Bits.push_back(G);
			}
		}
		// The parts: a few large soft pieces of the thing's own colours, in its near look's sizes and times
		const FarPart* const Parts[] = { &R.Crown, &R.Body, &R.Veil, &R.Skirt, &R.Column, &R.Foam, &R.Mist };
		const LingerRow& Row = LingerOf("shell", S.Linger);
		LingerRow Lasts = Row;
		Lasts.GoneS = std::max(Row.GoneS, SafeNum(S.LingerS, 0.0, 3600.0, 0.0)); // (gone no sooner than its near cloud: see Smoke)
		for (int I = 0; I < 7; ++I)
		{
			const FarPart& P = *Parts[I];
			const bool bSea = I >= 4;
			if (!P.bOn || bSea != S.bWater || (P.bMerges && !S.bCarrier))
			{
				continue;
			}
			// (a part that merges stands for the whole row it carries: in its middle, drawn out along it)
			const double Long = P.bMerges ? RunLong : 0.0;
			const Vec Mid = S.At + Way * (0.5 * Long);
			const double Var = D.U(0.9, 1.1);
			Bit B;
			B.Born = S.Born + 0.01 * I;
			B.R0 = P.R0 * Wide * Var;
			B.RMid = P.R1 * Wide * Var;
			B.R1 = B.RMid * P.Late;
			B.GrowBy = P.GrowS;
			B.GrowEarly = P.Grow;
			B.Grow = 0.8;
			const bool bLong = Long > 0.5;
			if (P.bUpright)
			{
				B.Form = Bit::Shape::Upright;
				B.Flatten = P.Flat;
				B.From = Mid;
				B.bHug = true;
				if (bLong)
				{
					B.Along = Way;
					B.Stretch = 1.0 + Long / (2.0 * std::max(B.RMid, 1.0e-3));
				}
			}
			else if (bLong)
			{
				// (drawn out along the row as its bombs land)
				B.Along = Way;
				B.Stretch = 1.0 + Long / (2.0 * std::max(B.RMid, 1.0e-3));
				B.StretchFrom = 1.0;
				B.StretchAfter = 0.0;
				B.StretchBy = std::max(RunS, 0.05);
				B.Rest = P.Up;
				B.From = Mid + ZUp * (P.Up * B.R0);
			}
			else if (P.Tall > 1.0 || P.TallEnd > 1.0)
			{
				// (standing up from the ground, its foot on it: Tall times as high as wide, held tall_s, then TallEnd by settle_s)
				B.Along = ZUp;
				B.Stretch = P.TallEnd;
				B.StretchFrom = P.Tall;
				B.StretchAfter = P.TallS;
				B.StretchBy = P.SettleS;
				B.Rest = P.Up * std::max(P.Tall, P.TallEnd);
				B.From = Mid + ZUp * (B.Rest * B.R0);
			}
			else
			{
				B.Rest = P.Up;
				B.From = Mid + ZUp * (P.Up * B.R0);
			}
			B.Rise = P.Rise;
			B.Windage = P.Windage;
			B.Alpha = P.Alpha;
			B.Heart = P.Heart;
			B.Rim = P.Rim;
			B.Inner = P.Inner;
			B.Rough = P.Rough;
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.ShadeK = P.Shade;
			B.FadeIn = P.FadeIn;
			B.Hold = P.Hold;
			B.Col = S.Dark.Mix(S.Light, P.PaintFrom) * P.Light;
			B.Col2 = S.Dark.Mix(S.Light, P.PaintTo) * P.Light;
			B.LeastPx = P.LeastPx;
			B.LeastKeep = P.Keep;
			// (its life: its own seconds; or a share of its cloud's thick time; or its cloud's whole slow going)
			const bool bLingers = P.bLingers && Lasts.bOn && P.LifeS <= 0.0 && P.LifeThick <= 0.0;
			B.Life = P.LifeS > 0.0 ? P.LifeS * Var : P.LifeThick > 0.0 && Row.ThickS > 0.0 ? P.LifeThick * Row.ThickS * Var : std::max(Lasts.GoneS, 1.0);
			B.Cool = std::min(B.Life / P.SettleS, 1.0e6);
			if (bLingers)
			{
				// (as its near cloud goes, by the row's own clock: it stays, and takes the smoke of the cloud. Its own dice
				// for the row's lift, so nothing near moves for it)
				const uint64_t LingerWas = LingerNo;
				LingerNo = Mix64(S.Seed ^ (static_cast<uint64_t>(I) + 1ull) * 0x2545F4914F6CDD1Dull);
				Lingering(B, Lasts, S.Born, Vec(Mid.X, Mid.Y, S.At.Z), P.Windage, 0.0, 1.0, 8.0, 1.0);
				LingerNo = LingerWas;
				B.TailLift *= P.bUpright ? 0.0 : P.Lift;
				// (how thick it looks through its tail, Third and TwoThirds of how thick it looked: the smoke it must have
				// left for that, one layer as thick as Alpha, as much wider as the tail has widened it)
				const double Thick = std::clamp(B.Alpha, 0.01, 0.99), Thin = std::log(1.0 - Thick);
				auto Left = [&](double Looks, double Into)
				{
					const double Wider = 1.0 + std::clamp(B.TailWide, 0.0, 4.0) * Into;
					return std::log(1.0 - Thick * std::clamp(Looks, 1.0e-4, 1.0)) / Thin * Wider * Wider;
				};
				if (P.Third > 0.0)
				{
					B.TailThird = std::clamp(Left(P.Third, 1.0 / 3.0), 1.0e-4, 1.0);
					B.TailTwoThirds = std::clamp(P.TwoThirds > 0.0 ? Left(P.TwoThirds, 2.0 / 3.0) : B.TailThird * 0.2, 1.0e-6, B.TailThird);
				}
			}
			Bits.push_back(B);
		}
		for (size_t J = From; J < Bits.size(); ++J)
		{
			Bits[J].FarRep = Rep;
			Bits[J].FarAt = S.At;
		}
	}

	double Battle::FarShare(const Bit& B, const Vec& EyeAt) const
	{
		if (B.FarRep != 1 && B.FarRep != 2)
		{
			return 1.0;
		}
		// (by where the thing struck, so a burst's pieces cross over as one: 0 to 1 over the last far.hand_over of the
		// draw range; none asked, at the range itself)
		const double Away = (B.FarAt - EyeAt).Length();
		double Far = Away > Beyond ? 1.0 : 0.0;
		const double Band = FarBand * Beyond;
		if (Band > 1.0e-6)
		{
			const double T = Clamp01((Away - (Beyond - Band)) / Band);
			Far = T * T * (3.0 - 2.0 * T);
		}
		if (!(Far == Far))
		{
			Far = 0.0;
		}
		return B.FarRep == 1 ? 1.0 - Far : Far;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Drawing

	void Battle::Lay(int Slot, const Vec& Middle, const Vec& AxisA, const Vec& AxisB, const Rgb& Col, double Alpha, double HeartAlpha, double RimAlpha, double Inner, double Point, double Tail, double Shade, const View& Eye, double Rough, uint32_t LumpSeed, double Turn, const Rgb& Floor, const Rgb& Through, const Vec& Outward, const Rgb& Lift, const Vec& TaperB)
	{
		const size_t First = static_cast<size_t>(Slot) * PointsPerPuff;
		const double FadeLen2 = bLayFade ? AxisA.Dot(AxisA) : 0.0; // (fx-air r2: see LayFadeHead)
		auto Put = [this, &Middle, &AxisA, FadeLen2](size_t I, const Vec& At, const Rgb& C, double A)
		{
			Out[I] = LayNear < 1.0 ? LayEye + (At - LayEye) * LayNear : At; // (a lying piece, nearer and smaller: the same picture; SetSoftContact)
			if (bSrgbBytes)
			{
				OutRgba[I * 4 + 0] = ByteGamma(C.R * PutK);
				OutRgba[I * 4 + 1] = ByteGamma(C.G * PutK);
				OutRgba[I * 4 + 2] = ByteGamma(C.B * PutK);
			}
			else
			{
				OutRgba[I * 4 + 0] = Byte(C.R * PutK);
				OutRgba[I * 4 + 1] = Byte(C.G * PutK);
				OutRgba[I * 4 + 2] = Byte(C.B * PutK);
			}
			OutRgba[I * 4 + 3] = Byte(FadeLen2 > 1e-18 ? A * (LayFadeTail + (LayFadeHead - LayFadeTail) * Clamp01(0.5 + 0.5 * (At - Middle).Dot(AxisA) / FadeLen2)) : A);
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
		// (game.from_above, Round.Top: seen from a high steep eye the column's top takes the light toward its own lit
		// colour, Round.TopLift, all of it on its sunward side and a little over half on the far side)
		auto RoundTopK = [this](const Vec& Q)
		{
			const double S = std::clamp((Q.X * Round.Right.X + Q.Y * Round.Right.Y - Round.Mid) / Round.R, -1.0, 1.0);
			const Vec N = Round.Right * S + Round.Back * std::sqrt(1.0 - S * S);
			return Round.Top * (0.55 + 0.45 * Clamp01(N.Dot(Round.Sun)));
		};
		// (bLayGlare: the Lift is a burst's flash, Bit::GlareK. A point is lit by how far round it is toward the flash on
		// the screen, GlareSide: all of the light on that side, 0.15 of it on the side away. A piece the eye sees from the
		// flash's side is lit all over; one standing before the flash keeps three tenths, on its edge toward it)
		const double GlareA = bLayGlare ? AxisA.Dot(LayGlareWay) : 0.0, GlareB = bLayGlare ? AxisB.Dot(LayGlareWay) : 0.0;
		const double GlareAcross = bLayGlare ? std::sqrt(std::max(0.0, 1.0 - Eye.Forward.Dot(LayGlareWay) * Eye.Forward.Dot(LayGlareWay))) : 1.0;
		auto GlareSide = [this](double Lean_)
		{
			const double Side = 0.15 + 0.85 * Lean_;
			return (Side + (1.0 - Side) * Clamp01(2.0 * LayGlareAll - 1.0)) * (0.3 + 0.7 * std::min(1.0, 2.0 * LayGlareAll));
		};
		const float KMid = Round.bOn ? static_cast<float>(0.9 * RoundK(Middle)) : bLayGlare ? static_cast<float>(GlareSide(0.5)) : 0.75f;
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
		const bool bHub = LayHub.Dot(LayHub) > 0.0; // (Bit::Hub: the fan's middle point off its middle, its heart ring between that point and the rim)
		// (Bit::bSoft: the middle of a ribbon's piece is half way to the colour of the piece behind)
		const Rgb MidBy = bLayLong && bLayFarCol ? Rgb(0.5f * LayFarBy.R, 0.5f * LayFarBy.G, 0.5f * LayFarBy.B) : Rgb();
		const Rgb MidFloor = bLayLong && bLayFarCol ? Rgb(0.5f * LayFarFloor.R, 0.5f * LayFarFloor.G, 0.5f * LayFarFloor.B) : Rgb();
		// (Bit::bShowEach: how far in sight the strip is at a share T of the way from its near end to its far, by the age there)
		auto ShowAt = [this](double T_)
		{
			if (!bLayShowEach)
			{
				return 1.0;
			}
			const double S = Clamp01((LayAgeNear + (LayAgeFar - LayAgeNear) * T_ - LayShowFrom) / std::max(LayShowBy - LayShowFrom, 1e-6));
			return S * S * (3.0 - 2.0 * S);
		};
		Put(First, bHub ? Middle + LayHub : Middle, Rgb(std::max(0.0f, std::max(Mid.R + MidBy.R, Floor.R + MidFloor.R)), std::max(0.0f, std::max(Mid.G + MidBy.G, Floor.G + MidFloor.G)), std::max(0.0f, std::max(Mid.B + MidBy.B, Floor.B + MidFloor.B))), bLayLong ? std::min(1.0, Alpha * (1.0 + 0.5 * (LayTailAlpha - 1.0) * (1.0 - Clamp01(2.0 * LayLump))) * ShowAt(0.5)) : Alpha);
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
		// (FX7, LaySix: a small piece's ring worked out at six of its points alone, the rest copied after: see SixUnderPx.
		// Not a strip's piece, a tapered or squared one, a hub's, a tube's or a round column's, whose points each have
		// their own place)
		const bool bSix = LaySix && !bLayLong && !bTaper && LaySquare <= 0.0 && LayInnerAlong <= 0.0 && !bHub && !Tube.bOn && !Round.bOn;
		const int RingLaid = bSix ? 6 : Sides;
		for (int Kn = 0; Kn < RingLaid; ++Kn)
		{
			const int K = bSix ? SixSlot[Kn] : Kn;
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
				if (bLayFarCol)
				{
					// (Bit::bSoft: lit across the strip only, by which side of it the point is on, and so the same all along:
					// lit by its place round the ring, each piece of pale smoke was bright at one end and dark at the other, a
					// step at every join. The points are placed already; from here Cs and Sn only light them)
					Cs = 0.0;
					Sn = OutSide;
				}
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
			const double ReachRound = Reach;
			if (Rough > 0.0)
			{
				const double Wave2 = Ring2Cos[I] * C2 - Ring2Sin[I] * S2;
				const double Wave3 = Ring3Cos[I] * C3 - Ring3Sin[I] * S3;
				const double Wave5 = Ring5Cos[I] * C5 - Ring5Sin[I] * S5;
				Reach *= std::max(0.35, 1.0 + Rough * (0.5 * Wave2 + 0.35 * Wave3 + 0.25 * Wave5));
			}
			// (Bit::bSoft: this point's own colour, on the way along the piece to the colour of the piece behind; lit and
			// shaded from there as any point is, so the two pieces are one colour at the edge they share)
			const float FarBy = bLayLong && bLayFarCol ? static_cast<float>(0.5 * (1.0 - Along)) : 0.0f;
			const Rgb ColK = FarBy > 0.0f ? Rgb(std::max(0.0f, Col.R + FarBy * LayFarBy.R), std::max(0.0f, Col.G + FarBy * LayFarBy.G), std::max(0.0f, Col.B + FarBy * LayFarBy.B)) : Col;
			const Rgb FloorK = FarBy > 0.0f ? Rgb(std::max(0.0f, Floor.R + FarBy * LayFarFloor.R), std::max(0.0f, Floor.G + FarBy * LayFarFloor.G), std::max(0.0f, Floor.B + FarBy * LayFarFloor.B)) : Floor;
			Rgb Shaded = ColK;
			Rgb HeartShaded = ColK;
			if (Shade > 0.0)
			{
				const double Len2 = AA * Cs * Cs + BB * Sn * Sn + 2.0 * AB * Cs * Sn;
				const double Toward = Len2 > 1e-18 ? (SunA * Cs + SunB * Sn) / std::sqrt(Len2) : (bLayLong && bLayFarCol ? 0.0 : SunUp);
				const double F = std::max(0.2, 1.0 + Shade * (ShadeLit * std::max(0.0, Toward) - ShadeDark * std::max(0.0, -Toward)));
				Shaded = ColK * F;
				HeartShaded = ColK * (1.0 + 0.5 * (F - 1.0));
			}
			if (LayShadowK > 0.0)
			{
				// (Bit::ShadowCol: the side away from the sun goes to its own shadow colour)
				const double Len2 = AA * Cs * Cs + BB * Sn * Sn + 2.0 * AB * Cs * Sn;
				const double Away = LayShadowK * std::max(0.0, Len2 > 1e-18 ? -(SunA * Cs + SunB * Sn) / std::sqrt(Len2) : (bLayLong && bLayFarCol ? 0.0 : -SunUp));
				Shaded = Shaded.Mix(LayShadow, std::min(1.0, Away));
				HeartShaded = HeartShaded.Mix(LayShadow, std::min(1.0, 0.6 * Away));
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
					const double Lobe = (1.0 - Round.Lobe) + Round.Lobe * Lean;
					KHeart = static_cast<float>(RoundK(Middle + Dir * (In * Reach)) * Lobe * HeartK);
					KRim = static_cast<float>(0.5 * RoundK(Middle + Dir * Reach) * Lobe * RimK);
					if (Round.Rim > 0.0)
					{
						// (Bit::LumpsRim: a lump on a lump is lit to its edge on its sunward side, so the dark between two
						// lumps lies on their shadow sides and never rings each one)
						KRim = std::max(KRim, static_cast<float>(Round.Rim * RoundK(Middle + Dir * Reach) * Lobe));
					}
				}
				else if (bLayGlare)
				{
					const double TowardFlash = Len2 > 1e-18 ? (GlareA * Cs + GlareB * Sn) / std::sqrt(Len2) : 0.0;
					KHeart = KRim = static_cast<float>(GlareSide(Clamp01(0.5 + 0.5 * TowardFlash / std::max(GlareAcross, 0.05))));
				}
				HeartShaded = Rgb(HeartShaded.R + KHeart * Lift.R, HeartShaded.G + KHeart * Lift.G, HeartShaded.B + KHeart * Lift.B);
				Shaded = Rgb(Shaded.R + KRim * Lift.R, Shaded.G + KRim * Lift.G, Shaded.B + KRim * Lift.B);
			}
			if (Round.bOn && Round.Top > 0.0)
			{
				// (game.from_above: the column's top, fading with the puff's own edge as the sun's side does)
				const double HeartK = Clamp01(HeartAlpha / std::max(Alpha, 1e-6)), RimK = Clamp01(RimAlpha / std::max(Alpha, 1e-6));
				const float TH = static_cast<float>(RoundTopK(Middle + Dir * (In * Reach)) * HeartK), TR = static_cast<float>(0.5 * RoundTopK(Middle + Dir * Reach) * RimK);
				HeartShaded = Rgb(HeartShaded.R + TH * Round.TopLift.R, HeartShaded.G + TH * Round.TopLift.G, HeartShaded.B + TH * Round.TopLift.B);
				Shaded = Rgb(Shaded.R + TR * Round.TopLift.R, Shaded.G + TR * Round.TopLift.G, Shaded.B + TR * Round.TopLift.B);
			}
			if (Tube.bOn)
			{
				HeartShaded = TubeK(Middle + Dir * (In * Reach));
				Shaded = TubeK(Middle + Dir * Reach);
			}
			Shaded = Rgb(std::max(Shaded.R, FloorK.R), std::max(Shaded.G, FloorK.G), std::max(Shaded.B, FloorK.B));
			HeartShaded = Rgb(std::max(HeartShaded.R, FloorK.R), std::max(HeartShaded.G, FloorK.G), std::max(HeartShaded.B, FloorK.B));
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
				const double Wob = std::fabs(Along) < 0.999 ? (LayWob > 1.0 ? std::max(0.4, 1.0 + (Reach - 1.0) * LayWob) : 1.0 + (Reach - 1.0) * LayWob) : 1.0;
				const Vec OnStrip = Middle + LayLong * Along + LayBendFar * (((U - 2.0) * U + 1.0) * U - 0.125) + LayBendNear * ((U - 1.0) * U * U + 0.125);
				const double LumpAt = LayLump + (LayLumpFar - LayLump) * T;
				const double Let = Clamp01(4.0 * LumpAt), Soft = Let * Let * (3.0 - 2.0 * Let);
				const double In_ = Clamp01((LumpAt - 0.1) / 0.5), Draw = In_ * In_ * (3.0 - 2.0 * In_);
				// (and it narrows toward that edge as it pales, so its shoulders are round, never a straight cut across)
				double Swell = 1.0;
				if (LayBillow > 0.0 && std::fabs(Along) < 0.999)
				{
					// (Bit::Billow: each point along its sides, the same on both, wider or narrower by its own share; the
					// corners as laid, where the next piece's are)
					// (and each side by a little of its own, so its two edges are broken, never one ruled tube that swells)
					const uint32_t H = (LayBillowSeed ^ (0x9E3779B9u * static_cast<uint32_t>((K <= Sides / 2 ? K : Sides - K) + 1))) * 2654435761u;
					const uint32_t Hs = (LayBillowSeed ^ (0x85EBCA6Bu * static_cast<uint32_t>(K + 1))) * 2246822519u;
					Swell = std::max(0.3, 1.0 + LayBillow * ((static_cast<double>((H >> 12) & 1023u) / 511.5 - 1.0) + 0.6 * (static_cast<double>((Hs >> 12) & 1023u) / 511.5 - 1.0)));
				}
				const Vec Edge = (AxisB + (LayFar - AxisB) * T) * (OutSide * Wob * Swell * (1.0 - 0.7 * Soft * Along * Along));
				const double Dark = (1.0 + (LayTailAlpha - 1.0) * T * (1.0 - Draw)) * (1.0 - Soft * std::fabs(Along) * (1.0 - Draw)) * ShowAt(T);
				const double RimA = OutSide == 0.0 ? HeartAlpha + (RimAlpha - HeartAlpha) * Draw : RimAlpha;
				const double InAt = In + std::max(0.0, 0.5 - In) * Soft; // (a lump is dense half way out, where a link is only at its core)
				const Vec HeartAt = OnStrip + Edge * InAt, RimAt = OnStrip + Edge;
				// (Bit::Lean: thinner on the one side of the strip it leans from, by the lean at each end, so a fold of
				// lighter smoke runs along inside it; never denser than the strip is, and nothing at an edge's middle,
				// where the next piece joins)
				const double HeartDark = bLayLean && OutSide != 0.0 ? Dark * std::clamp(1.0 + std::min(0.0, (LayLean + (LayLeanFar - LayLean) * T) * OutSide), 0.3, 1.0) : Dark;
				if (Draw > 0.0)
				{
					Put(First + 1 + I, HeartAt + (Middle + Dir * (InAt * Reach) - HeartAt) * Draw, HeartShaded, std::min(1.0, HeartAlpha * HeartDark));
					Put(First + 1 + static_cast<size_t>(Sides) + I, RimAt + (Middle + Dir * Reach - RimAt) * Draw, Shaded, std::min(1.0, RimA * Dark));
				}
				else
				{
					Put(First + 1 + I, HeartAt, HeartShaded, std::min(1.0, HeartAlpha * HeartDark));
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
				// (Bit::HeartRound: the heart ring keeps a true round, or part of the way to one, under a lumpy rim)
				const double HeartReach = LayHeartRound > 0.0 ? Reach + (ReachRound - Reach) * LayHeartRound : Reach;
				Put(First + 1 + I, bHub ? Middle + LayHub * (1.0 - In) + Dir * (In * HeartReach) : Middle + Dir * (In * HeartReach), HeartShaded, HeartAlpha);
			}
			Put(First + 1 + static_cast<size_t>(Sides) + I, Middle + Dir * Reach, Shaded, RimAlpha);
		}
		if (bSix)
		{
			// (every other point of both rings a copy of the nearest of the six)
			for (int K = 0; K < Sides; ++K)
			{
				const int From = SixSlot[SixOf[static_cast<size_t>(K)]];
				if (From == K)
				{
					continue;
				}
				for (size_t Ring_ = 0; Ring_ < 2; ++Ring_)
				{
					const size_t To = First + 1 + Ring_ * static_cast<size_t>(Sides) + static_cast<size_t>(K), Of = First + 1 + Ring_ * static_cast<size_t>(Sides) + static_cast<size_t>(From);
					Out[To] = Out[Of];
					for (size_t C = 0; C < 4; ++C)
					{
						OutRgba[To * 4 + C] = OutRgba[Of * 4 + C];
					}
				}
			}
		}
		if (bHub && !bLayLong && LayInnerAlong <= 0.0)
		{
			// (a fan drawn from a point off its middle folds over itself wherever its rough outline turns its back on
			// that point: the sector there is wound the other way and drawn over its neighbour, a thin wedge inked
			// twice (the pre-check's reading, 09:08Z 20 November; seen as a bright sharp wedge in a tongue of flame in
			// build 1764's still). Going round, each rim point that would lie behind the last one in sight, as the
			// hub sees them, is brought onto that one's line from the hub, as far out as it was, and its heart point
			// follows it: that sector is then a line and covers nothing. An outline wholly in sight of the hub is
			// left as it was, to the bit)
			const Vec Hub = Middle + LayHub;
			const Vec Ua = AxisA.Unit(Vec());
			const Vec Vb = (AxisB - Ua * AxisB.Dot(Ua)).Unit(Vec());
			if (Ua.Dot(Ua) > 0.5 && Vb.Dot(Vb) > 0.5 && Sides <= 24)
			{
				const size_t Heart0 = First + 1, Rim0 = First + 1 + static_cast<size_t>(Sides);
				// (the pre-check on cf7ed3f: a hub that lies outside its outline, and in no notch of it, sees every point
				// behind the last one in sight; the whole ring would be brought onto one line and the fan would vanish
				// (hub 0.75: 3 fans in a hundred, hub 0.9: 31; the recipe's own hubs, to 0.6: none in 600,000). A fan
				// more than half of whose rim would be moved is left as it was laid, folded)
				Vec WasHeart[24], WasRim[24];
				bool bMovedAt[24] = {};
				int MovedCount = 0;
				for (int K = 0; K < Sides; ++K)
				{
					WasHeart[K] = Out[Heart0 + static_cast<size_t>(K)];
					WasRim[K] = Out[Rim0 + static_cast<size_t>(K)];
				}
				auto Onto = [&](size_t I, double Px, double Py, double X, double Y)
				{
					const double L0 = std::sqrt(Px * Px + Py * Py), L1 = std::sqrt(X * X + Y * Y);
					if (L0 > 1e-12 && L1 == L1)
					{
						if (!bMovedAt[I])
						{
							bMovedAt[I] = true;
							++MovedCount;
						}
						const Vec Q = (Ua * Px + Vb * Py) * (L1 / L0);
						Out[Rim0 + I] = Hub + Q;
						Out[Heart0 + I] = Hub + Q * In;
					}
				};
				const Vec Q0 = Out[Rim0] - Hub;
				const double X0 = Q0.Dot(Ua), Y0 = Q0.Dot(Vb);
				double Px = X0, Py = Y0;
				for (int K = 1; K < Sides; ++K)
				{
					const size_t I = static_cast<size_t>(K);
					const Vec Q = Out[Rim0 + I] - Hub;
					const double X = Q.Dot(Ua), Y = Q.Dot(Vb);
					if (Px * Y - Py * X < 0.0)
					{
						Onto(I, Px, Py, X, Y);
					}
					else
					{
						Px = X;
						Py = Y;
					}
				}
				// (and the last points round, should they have passed the first)
				for (int K = Sides - 1; K >= 1; --K)
				{
					const size_t I = static_cast<size_t>(K);
					const Vec Q = Out[Rim0 + I] - Hub;
					const double X = Q.Dot(Ua), Y = Q.Dot(Vb);
					if (X * Y0 - Y * X0 >= 0.0)
					{
						break;
					}
					Onto(I, X0, Y0, X, Y);
				}
				if (2 * MovedCount > Sides)
				{
					for (int K = 0; K < Sides; ++K)
					{
						Out[Heart0 + static_cast<size_t>(K)] = WasHeart[K];
						Out[Rim0 + static_cast<size_t>(K)] = WasRim[K];
					}
				}
			}
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
		const double Nearer = std::max(CaseNearer, LieNearer); // (and with SetSoftContact, at least as near as a lying piece)
		if (Nearer > 0.0 && Off > 4.0 * Nearer)
		{
			// (nearer the eye along its own line of sight and smaller to match: the same picture, in front of the ground)
			const double K = 1.0 - Nearer / Off;
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

	double Battle::TrailNearK(double OffEye, double HalfWide) const
	{
		// (how much of an aircraft's trail shows that far from the eye: all of it from TrailNearWhole on; nearer, as
		// much as the distance leaves or as its size in the picture does, whichever leaves more)
		if (!(OffEye >= 0.0))
		{
			return 0.0; // (a distance that is not a number: none of it)
		}
		double Shows = Clamp01((OffEye - TrailNearGone) / std::max(TrailNearWhole - TrailNearGone, 1e-3));
		if (TrailLoomsGone > 0.0 && Shows < 1.0 && HalfWide >= 0.0)
		{
			const double Looms = HalfWide / std::max(OffEye, 1e-3);
			const double Small = Clamp01((TrailLoomsGone - Looms) / std::max(TrailLoomsGone - TrailLoomsWhole, 1e-6));
			Shows = std::max(Shows, Small * Clamp01((OffEye - TrailLeastGone) / std::max(TrailLeastWhole - TrailLeastGone, 1e-3)));
		}
		return Shows * Shows * (3.0 - 2.0 * Shows);
	}

	int Battle::LayBit(int Slot, const Bit& B, double Now, const View& Eye, int Room)
	{
		if (B.Hose > 0.0)
		{
			// (fx-air r2: a ship gun's hose, laid as one streak where its two rounds are now; see HoseLink)
			Bit Laid;
			if (!HoseNow(B, Now, Laid))
			{
				return 0;
			}
			const int Took = LayBit(Slot, Laid, Now, Eye, Room);
			B.bShown = B.bShown || Laid.bShown;
			return Took;
		}
		const double Age = Now - B.Born;
		// (a piece of no life is never laid: its share of its life is 0 over 0. A trail's knot with knot_s 0 and 0 was
		// one: the pre-check on 1fc2904, its cure)
		if (Age < 0.0 || !(B.Life > 0.0) || ((Age > B.Life || Now > B.CutAt) && !Unseen(B)))
		{
			return 0;
		}
		// (a light shorter than a frame that no frame has caught yet is laid now, where it is, at its full strength)
		const double Shown = Unseen(B) && Age > B.Life * std::min(B.Hold, 0.5) ? B.Life * std::min(B.Hold, 0.5) : Age;
		// (one that lingers: its looks by the life it was laid with, Bit::bTail)
		const double F = B.bTail ? std::min(Shown / std::max(B.LookLife, 1e-6), 1.0) : Shown / B.Life;
		double Rad = B.R0 + (B.R1 - B.R0) * std::pow(F, B.Grow);
		// (a ribbon's piece, in two stages: see Bit::GrowBy)
		auto RadTwo = [](double R0_, double RMid_, double R1_, double Age_, double Life_, double By_, double Early_, double Late_)
		{
			return R0_ + (RMid_ - R0_) * std::pow(Clamp01(Age_ / By_), Early_) + (R1_ - RMid_) * std::pow(Clamp01((Age_ - By_) / std::max(Life_ - By_, 1e-6)), Late_);
		};
		if (B.GrowBy > 0.0)
		{
			Rad = B.bTail ? RadTwo(B.R0, B.RMid, B.R1, std::min(Shown, B.LookLife), B.LookLife, B.GrowBy, B.GrowEarly, B.Grow) : RadTwo(B.R0, B.RMid, B.R1, Shown, B.Life, B.GrowBy, B.GrowEarly, B.Grow);
		}
		if (B.WakeBy > 0.0)
		{
			Rad = std::max(Rad, WakeWide(B.R0, B.WakeR, B.WakeBy, B.WakeOn, Shown)); // (spread in the aircraft's wake: see Bit::WakeBy)
		}
		// (Bit::bSoft, a soft strip's piece: as dark as it will be from the moment it is laid. The trail's young end has drawn
		// the smoke up to that place already, so nothing appears; fading in, each new piece was pale at its near end for
		// a frame under the dark end of the piece ahead of it, a notch behind the engine ten times a second, and a piece
		// laid at the very moment of a frame was left out whole, the trail starting a piece's length behind its engine)
		const bool bBornFull = B.bSoft && B.Ribbon > 0.0;
		double Fade = (bBornFull ? 1.0 : std::min(1.0, Shown / std::max(B.FadeIn, 1e-3))) * (F < B.Hold ? 1.0 : std::pow(std::max(0.0, 1.0 - (F - B.Hold) / std::max(1.0 - B.Hold, 1e-6)), 1.4)) * CutFade(B.CutAt, Now, TrailCutFade);
		// (smoke that lingers, Lingering: through its cloud's tail it goes by the cloud's law, widens on, climbs and pales
		// toward the air)
		double TailPaled = 0.0;
		if (B.bTail)
		{
			Fade = bBornFull ? 1.0 : std::min(1.0, Shown / std::max(B.FadeIn, 1e-3)); // (as thick as it was laid until its tail begins)
		}
		if (B.InTail(Shown))
		{
			const double TailFrom = B.Hold * B.Life;
			const double TailOwn = Clamp01((Shown - TailFrom) / std::max(B.Life - TailFrom, 1e-6)); // (how far through its own fade)
			const bool bTailGoes = B.TailGoBy > 0.0;
			const double TailInto = bTailGoes ? TailOwn * std::min(B.TailGoBy, 1.0) : TailOwn; // (and through the cloud's tail)
			// (what the cloud has left of its smoke: see Bit::bTail)
			const double TailA = std::log(std::clamp(B.TailThird, 1.0e-4, 1.0)), TailB = std::log(std::clamp(B.TailTwoThirds, 1.0e-6, 1.0));
			const double TailLate = std::min(TailB, TailA);
			const double TailLog = TailInto < 1.0 / 3.0 ? TailA * 3.0 * TailInto : TailInto < 2.0 / 3.0 ? TailA + (TailLate - TailA) * 3.0 * (TailInto - 1.0 / 3.0) : TailLate + std::min((TailLate - TailA) * 0.4, -0.8) * 7.5 * (std::min(TailInto, 0.8) - 2.0 / 3.0);
			const double TailEnd = Clamp01((1.0 - TailInto) / 0.2);
			const double TailCloud = std::exp(TailLog) * TailEnd * TailEnd * (3.0 - 2.0 * TailEnd);
			// (its own share of that: one that goes gives its smoke up, one that stays takes it over)
			const double TailKept = std::clamp(B.TailKeep, 0.05, 1.0);
			const double TailGoing = bTailGoes ? std::pow(1.0 - TailOwn, 1.4) : (TailInto < B.TailGoes ? std::pow(1.0 - TailInto / std::max(B.TailGoes, 1e-6), 1.4) : 0.0);
			const double TailShare = bTailGoes ? TailGoing : (1.0 - (1.0 - TailKept) * TailGoing) / TailKept;
			const double TailWider = 1.0 + std::clamp(B.TailWide, 0.0, 4.0) * TailInto;
			const double TailWas = std::clamp(B.Alpha, 1e-4, 0.999);
			Fade = (bBornFull ? 1.0 : std::min(1.0, Shown / std::max(B.FadeIn, 1e-3))) * (1.0 - std::pow(1.0 - TailWas, TailCloud * TailShare / (TailWider * TailWider))) / TailWas;
			Rad *= TailWider;
			const double TailFor = Clamp01((TailInto - B.TailPaleFrom) / std::max(B.TailPaleBy - B.TailPaleFrom, 1e-6));
			TailPaled = Clamp01(B.TailPale) * TailFor * TailFor * (3.0 - 2.0 * TailFor);
		}
		if (B.DimTo < 1.0 && F > B.DimFrom)
		{
			// (an air tracer: dimming over its last third, then out; see Bit::BySpeed)
			Fade *= 1.0 - (1.0 - std::max(0.0, B.DimTo)) * Clamp01((F - B.DimFrom) / std::max(B.Hold - B.DimFrom, 1e-6));
		}
		double A = B.Alpha * Fade;
		// (Bit::LowBy, dust born low. Squat: how high it is for its width now; LitNow: how much of its light and shade it
		// takes yet; InnerOwn: where its heart ring sits now, further in while it is low, so a young skirt has a soft edge)
		double Squat = 1.0, LitNow = 1.0, InnerOwn = B.Inner;
		if (B.LowBy > 0.0)
		{
			const double LowFrom = B.LowFrom == B.LowFrom ? std::max(0.0, B.LowFrom) : 0.0, LowFlat = B.LowFlat == B.LowFlat ? std::clamp(B.LowFlat, 0.1, 1.0) : 1.0;
			const double LowSoft = B.LowSoft == B.LowSoft ? std::clamp(B.LowSoft, 0.1, 1.0) : 1.0;
			const double G = Clamp01((Age - LowFrom) / std::max(B.LowBy - LowFrom, 1e-3));
			LitNow = G * G * (3.0 - 2.0 * G);
			Squat = LowFlat + (1.0 - LowFlat) * LitNow;
			InnerOwn = B.Inner * (LowSoft + (1.0 - LowSoft) * LitNow);
			const double LowLit = B.LowLit == B.LowLit ? std::clamp(B.LowLit, 0.0, 1.0) : 0.0;
			LitNow = LowLit + (1.0 - LowLit) * LitNow; // (the share of its light and shade it is born with)
		}
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
		// (Bit::StandA: a far trail's short piece standing in for the bar to be laid over it, until it is)
		bool bStand = false;
		if (B.StandA > 0.0)
		{
			const auto Of = std::lower_bound(RunBars.begin(), RunBars.end(), std::make_pair(B.TrailRun, 0u));
			bStand = !(Of != RunBars.end() && Of->first == B.TrailRun && Of->second > B.StandBars);
		}
		auto StandIn = [&B, &Paled](double Alpha_, double Age_)
		{
			const double S = Clamp01((Age_ - B.StandFrom) / std::max(B.StandBy - B.StandFrom, 1e-6));
			const double Bar = B.StandA * S * S * (3.0 - 2.0 * S) * (B.PaleBy > 0.0 ? Paled(Age_) : 1.0);
			return 1.0 - (1.0 - Clamp01(Alpha_)) * (1.0 - Clamp01(Bar));
		};
		if (bStand)
		{
			A = StandIn(A, Age);
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
		if (B.AgeFade > 0.0)
		{
			A *= std::exp(-std::pow(std::max(0.0, Age - B.AgeFadeFrom) / B.AgeFade, B.AgeFadePow)); // (a funnel's puff thins by its age, see Bit::AgeFade)
		}
		double RollShadeNow = 0.0, RollLiftNow = 0.0; // (a funnel's puff on its plume's roll: this much darker and this many metres higher, see Bit::EvenRollLift)
		if (B.Even >= 0)
		{
			// (a funnel's plume laid evenly, see PlumeMove: Wider is the plume's width over its width at the lip, Doubled
			// how many times that has doubled. Until the last share of a doubling every puff is as much thinner as the
			// plume is deeper in them; over that share the ones that go at its end thin to nothing and the others take
			// their smoke up, and stand whole again as the next doubling starts)
			const double Widened = 1.0 + B.EvenK * std::pow(std::max(Age, 0.0), 2.0 / 3.0);
			const double Wider = Widened >= 1.0 ? std::min(Widened, 1.0e12) : 1.0; // (held: an age or a widening that is no number thins nothing)
			const double Log = std::log2(Wider);
			// (which halving it is past, where in the width's doublings that stretch began and how long it is: Bit::EvenNear)
			const double StepLog = std::clamp(B.EvenStep, 0.2, 1.0), NearEnd = std::max(B.EvenNear, 0), FarStart = NearEnd + std::max(B.EvenFar, 0) * StepLog;
			int Doubled = 0;
			double From = 0.0, Long = 1.0;
			if (Log < NearEnd)
			{
				Doubled = static_cast<int>(Log);
				From = Doubled;
			}
			else if (Log < FarStart)
			{
				const int Past = static_cast<int>((Log - NearEnd) / StepLog);
				Doubled = std::max(B.EvenNear, 0) + Past;
				From = NearEnd + Past * StepLog;
				Long = StepLog;
			}
			else
			{
				const int Past = static_cast<int>(std::min(Log - FarStart, 40.0));
				Doubled = std::max(B.EvenNear, 0) + std::max(B.EvenFar, 0) + Past;
				From = FarStart + Past;
			}
			// (the plume's roll, Bit::EvenRollLift: the wave at this puff's number, one value every EvenRollRun puffs times two for
			// each doubling of the plume's width so far, going over from each such spacing to the next as it widens)
			if (B.EvenRollLift > 0.0 || B.EvenRollShade > 0.0)
			{
				const int Octave = static_cast<int>(std::min(Log, 30.0));
				const double Part = Clamp01(Log - Octave), PartEased = Part * Part * (3.0 - 2.0 * Part);
				const double Run = B.EvenRollRun >= 1.0 ? std::min(B.EvenRollRun, 64.0) : 1.0;
				auto WaveAt = [&B, Run](int Oct_)
				{
					const double X = static_cast<double>(B.EvenN) / std::ldexp(Run, Oct_);
					const double Cell = std::floor(X), In_ = X - Cell;
					auto Thrown = [&B, Oct_](double C_)
					{
						const uint64_t H = Mix64((static_cast<uint64_t>(B.EvenSeed) << 32) ^ (static_cast<uint64_t>(C_) * 0x9E3779B97F4A7C15ull) ^ (static_cast<uint64_t>(Oct_) * 0xD1B54A32D192ED03ull));
						const double V = static_cast<double>(H >> 11) * (2.0 / 9007199254740992.0) - 1.0;
						return V * (1.5 - 0.5 * V * V); // (from -1 to 1, oftener far from 0 than near it: crests and troughs, few flats)
					};
					const double A0 = Thrown(Cell), A1 = Thrown(Cell + 1.0);
					return A0 + (A1 - A0) * (In_ * In_ * (3.0 - 2.0 * In_));
				};
				const double Rolled = std::clamp(WaveAt(Octave) * (1.0 - PartEased) + WaveAt(Octave + 1) * PartEased, -1.0, 1.0);
				RollShadeNow = std::min(Clamp01(B.EvenRollShade), 0.5) * Rolled;
				// (no lift at the funnel's lip, eased in until the plume is near three times as wide: the roll grows as the smoke
				// does, and there is no hook at her funnel)
				const double Grown = Clamp01(Log / 1.5);
				RollLiftNow = Clamp01(B.EvenRollLift) * Rolled * Rad * (Grown * Grown * (3.0 - 2.0 * Grown));
			}
			if (Doubled > B.Even)
			{
				A = 0.0;
			}
			else
			{
				const double Gone = B.EvenWin > 0.0 ? Clamp01((Log - From - (1.0 - B.EvenWin) * Long) / (B.EvenWin * Long)) : 0.0;
				const double Eased = Gone * Gone * (3.0 - 2.0 * Gone);
				// (as much thinner as its share of the smoke: the light it lets through, to that power. Halving a dense
				// puff's opacity would more than halve its smoke)
				const double Share = std::ldexp(1.0, std::min(Doubled, 60)) / Wider * (Doubled < B.Even ? 1.0 + Eased : 1.0 - Eased);
				A = 1.0 - std::pow(1.0 - std::clamp(A, 0.0, 0.999), Share);
			}
		}
		const bool bShowEach = B.bShowEach && B.ShowBy > 0.0 && B.Ribbon > 0.0 && B.Back.bOn && B.Form == Bit::Shape::Puff;
		double ShowNear = 1.0, ShowFar = 1.0; // (Bit::bShowEach: how far in sight it is at its two ends, laid on along it by Lay)
		if (B.ShowBy > 0.0)
		{
			// (a ribbon's bar that hangs on: coming into sight as the short pieces it stands in for go)
			const double T = Clamp01((Age - B.ShowFrom) / std::max(B.ShowBy - B.ShowFrom, 1e-6));
			ShowNear = T * T * (3.0 - 2.0 * T);
			if (!bShowEach)
			{
				A *= ShowNear;
			}
		}
		// (a ribbon's piece reaches back to the piece behind: how dark that one is now, where the two meet, and how far it
		// has drawn in to a lump. Its far edge takes both, so the two pieces are one strip at the edge they share)
		const bool bReach = B.Ribbon > 0.0 && B.Back.bOn && B.Form == Bit::Shape::Puff;
		double TailA = 0.0, LumpFar = 0.0;
		if (bReach)
		{
			const double AgeT = Now - B.Back.Born;
			const double FT = Clamp01(AgeT / std::max(B.Back.Life, 1e-6));
			if (AgeT <= B.Back.Life && Now <= B.Back.CutAt)
			{
				TailA = B.Back.Alpha * (bBornFull ? 1.0 : std::min(1.0, AgeT / std::max(B.FadeIn, 1e-3))) * (FT < B.Hold ? 1.0 : std::pow(std::max(0.0, 1.0 - (FT - B.Hold) / std::max(1.0 - B.Hold, 1e-6)), 1.4)) * CutFade(B.Back.CutAt, Now, TrailCutFade);
				if (B.DayAlpha != 1.0)
				{
					TailA *= 1.0 + (B.DayAlpha - 1.0) * DayOf(Eye.SmokeLight);
				}
				if (B.ShowBy > 0.0)
				{
					const double T = Clamp01((AgeT - B.ShowFrom) / std::max(B.ShowBy - B.ShowFrom, 1e-6));
					if (bShowEach)
					{
						ShowFar = T * T * (3.0 - 2.0 * T);
					}
					else
					{
						TailA *= T * T * (3.0 - 2.0 * T);
					}
				}
				if (B.PaleBy > 0.0)
				{
					if (B.Back.PaleTo >= 0.0)
					{
						// (that piece pales as far as its own kind does: a trail going over from one kind to another)
						const double G = Clamp01((AgeT - B.PaleFrom) / std::max(B.PaleBy - B.PaleFrom, 1e-6));
						TailA *= 1.0 - (1.0 - std::min(1.0, B.Back.PaleTo)) * G * G * (3.0 - 2.0 * G);
					}
					else
					{
						TailA *= Paled(AgeT);
					}
				}
			}
			if (bStand && AgeT <= B.Back.Life && Now <= B.Back.CutAt)
			{
				TailA = StandIn(TailA, AgeT); // (its far end too: the piece behind, up to the bar over both)
			}
			if (B.Back.LumpsFrom >= 0.0 && FT > B.Back.LumpsFrom)
			{
				LumpFar = Clamp01((FT - B.Back.LumpsFrom) / std::max(1.0 - B.Back.LumpsFrom, 1e-6));
			}
		}
		if ((bShowEach ? A * ShowNear : A) < 0.004 && (bShowEach ? TailA * ShowFar : TailA) < 0.004) // (any other piece has its show in A already: the pre-check's trails-and-hits probe on 3730f9e4)
		{
			return 0;
		}
		if (bReach)
		{
			A = std::max(A, 1e-3); // (unseen at its own end yet, it is still the strip up to the piece behind)
		}
		const double CoolF = B.CoolFrom > 0.0 ? Clamp01((F - B.CoolFrom) / std::max(1.0 - B.CoolFrom, 1e-6)) : F; // (Bit::CoolFrom)
		Rgb Col = B.Col.Mix(B.Col2, CoolF * B.Cool);
		if (RollShadeNow != 0.0)
		{
			Col = Col * (1.0 - RollShadeNow);
		}
		Rgb Floor, Through, Lift;
		if (!B.bLight)
		{
			Col = Rgb(Col.R * Eye.SmokeLight.R, Col.G * Eye.SmokeLight.G, Col.B * Eye.SmokeLight.B);
			if (TailPaled > 0.0)
			{
				Col = Col.Mix(bSkyKnown ? SkyTail : Rgb(B.TailCol.R * Eye.SmokeLight.R, B.TailCol.G * Eye.SmokeLight.G, B.TailCol.B * Eye.SmokeLight.B), TailPaled);
			}
			Floor = Col * SkyFill; // (art director 22:24Z: the sky lights smoke's shadow side too, so it is never a hole)
			if (B.bNeverUnder)
			{
				// (a burst's thrown earth: no point of it darker than its floor, Bit::bNeverUnder)
				Floor = Rgb(std::max(Floor.R, B.NeverUnder.R * Eye.SmokeLight.R), std::max(Floor.G, B.NeverUnder.G * Eye.SmokeLight.G), std::max(Floor.B, B.NeverUnder.B * Eye.SmokeLight.B));
			}
		}
		Vec P = Where(B, Age);
		if (RollLiftNow != 0.0)
		{
			P.Z += RollLiftNow;
		}
		if (B.Rest > 0.0)
		{
			const double Surface = Height(P.X, P.Y);
			P.Z = std::max(P.Z, Surface + (Squat < 1.0 ? B.Rest * Rad * std::sqrt(Squat) : B.Rest * Rad)); // (a low mound's middle lies as much lower)
			if (B.FootClear > 0.0 && !B.bLight)
			{
				// (a big fire's smoke lying on the ground or sea keeps the fire's own dark: see fire.smoke_foot)
				const double Lies = B.FootK * (1.0 - Clamp01(((P.Z - Surface) / std::max(Rad, 1e-3) - B.Rest) / B.FootClear));
				if (Lies > 0.0)
				{
					Col = Col.Mix(Rgb(B.FootCol.R * Eye.SmokeLight.R, B.FootCol.G * Eye.SmokeLight.G, B.FootCol.B * Eye.SmokeLight.B), Lies);
					Floor = Col * SkyFill;
				}
			}
		}
		if (B.bShadeTwin)
		{
			// (game.from_above, LayShadows: a puff's shadow, where the sun's line through it meets the ground, the ground
			// found again there so a slope takes it right)
			const Vec Sun = Eye.Sun.Unit(ZUp);
			if (!(Sun.Z >= 0.15))
			{
				return 0;
			}
			Vec On = P - Sun * ((P.Z - Height(P.X, P.Y)) / Sun.Z);
			On = P - Sun * (std::max(0.0, P.Z - Height(On.X, On.Y)) / Sun.Z);
			On.Z = Height(On.X, On.Y) + 0.05;
			P = On;
			A *= ShadeTwinA;
			// (shadow_darkest: where shadows lie over each other, as under a wide plume, the ground keeps at least 1 - shadow_darkest
			// of its light, so a column's shadow never stacks into a dark puddle on the field (the art director's
			// 19:59Z mend). Kept by patches of the ground a little smaller than the shadow, one set for each size of
			// shadow, the darkest of the patch and its eight neighbours ruling)
			if (AboveDarkest < 1.0)
			{
				const int Level = std::clamp(static_cast<int>(std::floor(std::log2(std::max(Rad, 1.0)))), 0, 15);
				const double Cell = 0.8 * std::ldexp(1.0, Level);
				const int64_t Cx = static_cast<int64_t>(std::floor(std::clamp(P.X / Cell, -1.0e8, 1.0e8))), Cy = static_cast<int64_t>(std::floor(std::clamp(P.Y / Cell, -1.0e8, 1.0e8)));
				auto Key = [Level](int64_t X_, int64_t Y_) { return (static_cast<uint64_t>(Level) << 58) ^ ((static_cast<uint64_t>(X_) & 0x1FFFFFFFull) << 29) ^ (static_cast<uint64_t>(Y_) & 0x1FFFFFFFull); };
				float Left = 1.0f;
				for (int64_t Dx = -1; Dx <= 1; ++Dx)
				{
					for (int64_t Dy = -1; Dy <= 1; ++Dy)
					{
						const auto It = ShadowCells.find(Key(Cx + Dx, Cy + Dy));
						if (It != ShadowCells.end())
						{
							Left = std::min(Left, It->second);
						}
					}
				}
				A = std::min(A, std::max(0.0, 1.0 - (1.0 - AboveDarkest) / std::max(static_cast<double>(Left), 1e-3)));
				float& Here = ShadowCells.emplace(Key(Cx, Cy), 1.0f).first->second;
				Here = static_cast<float>(Here * (1.0 - A));
			}
			if (!(A >= 0.004))
			{
				return 0;
			}
		}
		Rgb LitOwn = B.LitCol; // (fx-air r2: its sunlit side's colour, paled as it climbs; see Bit::PaleUpM)
		if (B.PaleUpM > 0.0 && !B.bLight)
		{
			PaleUp(B, P, Eye, Col, Floor, LitOwn, A);
		}
		const double Dist = (P - Eye.Eye).Length();
		if (Dist > (B.FarRep >= 2 ? FarBeyond : (B.bKeep ? KeepBeyond : Beyond)))
		{
			return 0;
		}
		if (B.FarRep != 0)
		{
			// (a bomb's near burst and its far form, Bit::FarRep: each as much as shows to this eye)
			A *= FarShare(B, Eye.Eye);
			if (!(A >= 0.004))
			{
				return 0;
			}
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
		// (a strip kept for its far end, the near one gone: drawn, and not dropped by the test for a faint piece below. The
		// pre-check, 06:06Z 22 November: without it the strip was cut there in a step, 20 to 38 m from the eye)
		bool bTailShows = false;
		if (TrailNearWhole > 0.0 && !B.bLight && B.Form == Bit::Shape::Puff && (B.Ribbon > 0.0 || B.Smear > 0.0))
		{
			// (art director 04:17Z 22 November, riding with an aircraft: a trail passing the lens was a soft band across
			// the frame, and a strip through the eye's own plane a hard-edged sheet. An aircraft's trail thins out as it
			// comes up to the eye and is gone before it reaches it: see TrailNearGone. Its flames are lights and stay)
			if (bReach)
			{
				// (a strip's far end by where the piece behind is now and how wide, as that piece fades itself)
				const Bit::Reach& Behind = B.Back;
				const double AgeBack = Now - Behind.Born;
				const double GoBack = B.Drag > 1e-6 ? (1.0 - std::exp(-B.Drag * AgeBack)) / B.Drag : AgeBack;
				const Vec FarAt = Behind.From + Behind.Vel * GoBack + Wind * (B.Windage * AgeBack) + Vec(0.0, 0.0, Behind.Rise * AgeBack);
				double RadBack = Behind.GrowBy > 0.0 ? RadTwo(Behind.R0, Behind.RMid, Behind.R1, AgeBack, Behind.Life, Behind.GrowBy, Behind.GrowEarly, Behind.Grow) : Behind.R0 + (Behind.R1 - Behind.R0) * std::pow(Clamp01(AgeBack / std::max(Behind.Life, 1e-6)), Behind.Grow);
				if (Behind.WakeBy > 0.0)
				{
					RadBack = std::max(RadBack, WakeWide(Behind.R0, Behind.WakeR, Behind.WakeBy, Behind.WakeOn, AgeBack));
				}
				const double OwnA = A * TrailNearK(Dist, Rad);
				TailA *= TrailNearK((FarAt - Eye.Eye).Length(), RadBack);
				if (OwnA < 0.004 && TailA < 0.004)
				{
					return 0;
				}
				A = std::max(OwnA, 1e-3);
				bTailShows = TailA >= 0.004;
			}
			else if (Dist < TrailNearWhole)
			{
				A *= TrailNearK(Dist, Rad);
				if (A < 0.004)
				{
					return 0;
				}
			}
		}
		if (LensWhole > 0.0 && (B.Form == Bit::Shape::Puff || B.Form == Bit::Shape::Upright))
		{
			// (SetLensFade, the land battle's: a puff is laid flat to the lens, so one level with it a few metres aside
			// was laid across the whole picture. Its middle behind the lens or at it: never laid, a light neither, and
			// a light so left out is done with, as one off the screen is. Smoke and dust thin out by how far in front
			// of the lens their near side is)
			const double Depth = (P - Eye.Eye).Dot(Eye.Forward);
			if (!(Depth > LensPlane))
			{
				B.bShown = true;
				return 0;
			}
			if (!B.bLight && !B.bThrown && !B.bKeep && B.Ribbon <= 0.0 && B.Smear <= 0.0)
			{
				const double Front = Depth - 0.5 * Rad * std::max(1.0, B.Stretch);
				if (Front < LensWhole)
				{
					const double T = Clamp01((Front - LensGone) / (LensWhole - LensGone));
					A *= T * T * (3.0 - 2.0 * T);
					if (A < 0.004)
					{
						return 0;
					}
				}
			}
		}
		if ((CapThrownWhole > 0.0 || CapCard > 0.0 || CapLight > 0.0) && (B.Form == Bit::Shape::Puff || B.Form == Bit::Shape::Upright))
		{
			// (game.lens_cap, the art director 22:46Z 21 November; cut 4's still 06, 22 November: a clod thrown a few
			// metres in front of the lens filled a twentieth of the picture as a flat black slab. A thrown clod fades out
			// as it comes within thrown_whole_m of the lens, gone by thrown_gone_m; a card of smoke or dust wider than
			// card_most of the picture, or a light wider than light_most, within near_m of the lens, fades away over the
			// last soft of that, so nothing near the lens ever blacks the picture out or blinds it)
			const double Depth = (P - Eye.Eye).Dot(Eye.Forward);
			if (Depth > 1e-3)
			{
				if (B.bThrown && CapThrownWhole > 0.0)
				{
					const double T = CapThrownWhole > CapThrownGone ? Clamp01((Depth - CapThrownGone) / (CapThrownWhole - CapThrownGone)) : (Depth > CapThrownGone ? 1.0 : 0.0);
					A *= T * T * (3.0 - 2.0 * T);
				}
				const double Most = B.bLight ? CapLight : (B.bThrown ? 0.0 : CapCard);
				const double NearM = std::max(CapNear, B.LensNear); // (a big burst's own, further: sizes.<size>.lens_near_m)
				if (Most > 0.0 && Eye.HalfAcross > 0.0 && Depth < NearM)
				{
					const double Share = Rad / (Depth * Eye.HalfAcross); // (its width over the picture's)
					if (Share > Most * (1.0 - CapSoft))
					{
						const double T = Clamp01((Most - Share) / (Most * CapSoft));
						const double Near = Clamp01((NearM - Depth) / (0.3 * NearM)); // (only near the lens: a column filling the picture from further off is untouched)
						A *= 1.0 - Near * (1.0 - T * T * (3.0 - 2.0 * T));
					}
				}
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
		Round.Top = 0.0;
		bLayGlare = false;
		LaySquare = 0.0;
		LayInnerAlong = 0.0;
		bLayFade = false;
		LayHub = Vec();
		bLayLong = false;
		bLayFarCol = false;
		bLayLean = false;
		LayTailAlpha = 1.0;
		LayLump = LayLumpFar = 0.0;
		LayWob = 1.0;
		LayBillow = 0.0;
		bLayShowEach = false;
		EdgeK = B.EdgeK;
		EdgeTo = B.Edge;
		EdgeIn = B.EdgeIn;
		PutK = (B.bLight ? std::clamp(B.Over, 1.0, LitGain) : std::clamp(B.LitOver, 1.0, LitGain)) / LitGain; // (Bit::LitOver: 1 but for a far sea splash's white water)
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
				// (SunBy: young smoke is lit less, so a bank is darkest where it leaves the fire. Art director 04:06Z: the
				// dark pile at the root takes next to none of the light and reads by its edge against the sea; the light
				// comes in through the middle of the bank, so the far lumps keep their lit tops)
				const double Grown = B.SunBy > 0.0 ? B.SunYoung + (1.0 - B.SunYoung) * Clamp01((Age - B.SunFrom) / std::max(B.SunBy - B.SunFrom, 1e-3)) : 1.0;
				const float L = static_cast<float>(Clamp01(B.SunSide * Grown * Day * W * (0.15 + 0.85 * Clamp01(0.65 + 0.6 * Face))));
				Lift = Rgb((LitOwn.R * Eye.SmokeLight.R - Col.R) * L, (LitOwn.G * Eye.SmokeLight.G - Col.G) * L, (LitOwn.B * Eye.SmokeLight.B - Col.B) * L);
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
				// (SunBy, as the bank's: the column's young smoke, low over the fire, takes little of the light, so the
				// column stands black at its root and its lit side comes in higher up. None asked: all of it, as before)
				Round.Gain = B.SunSide * Day * (B.SunBy > 0.0 ? B.SunYoung + (1.0 - B.SunYoung) * Clamp01((Age - B.SunFrom) / std::max(B.SunBy - B.SunFrom, 1e-3)) : 1.0);
				Round.Level = std::sqrt(std::max(0.0, 1.0 - Eye.Sun.Z * Eye.Sun.Z));
				Round.Up = RoundUp * std::max(0.0, Eye.Sun.Z);
				Round.Lobe = 0.25;
				Round.Rim = 0.0;
				Lift = Rgb(LitOwn.R * Eye.SmokeLight.R - Col.R, LitOwn.G * Eye.SmokeLight.G - Col.G, LitOwn.B * Eye.SmokeLight.B - Col.B);
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
		if (B.GlareK > 0.0 && Now < B.GlareEnd && !B.bLight && !Round.bOn && Dist > 1e-6)
		{
			// (thrown earth in its own burst's flash, Bit::GlareK: strongest as the flash goes off and gone by GlareEnd,
			// the most on what is nearest the flash. A number that is no number lights nothing)
			const Vec ToFlash = B.GlareAt - P;
			const double FlashFar = ToFlash.Length();
			const double GlareLeft = Clamp01((B.GlareEnd - Now) / std::max(B.GlareFor, 1e-3));
			const double GlareOff = FlashFar / std::max(B.GlareReach, 1e-3);
			const float L = static_cast<float>(Clamp01(B.GlareK * GlareLeft * GlareLeft / (1.0 + GlareOff * GlareOff)));
			if (L > 0.004f)
			{
				Lift = Rgb((B.GlareCol.R - Col.R) * L, (B.GlareCol.G - Col.G) * L, (B.GlareCol.B - Col.B) * L);
				bLayGlare = true;
				LayGlareWay = FlashFar > 1e-6 ? ToFlash * (1.0 / FlashFar) : Eye.Forward * -1.0;
				LayGlareAll = FlashFar > 1e-6 ? Clamp01(0.5 + 0.5 * ToFlash.Dot(Eye.Eye - P) / (FlashFar * Dist)) : 1.0;
			}
		}
		if (FromAbove > 0.0 && !B.bLight && !B.bShadeTwin && !bLayGlare && Dist > 1e-6 && (B.Form == Bit::Shape::Puff || B.Form == Bit::Shape::Upright)
			&& B.Ribbon <= 0.0 && B.Smear <= 0.0)
		{
			// (game.from_above, the art director's read of the general's stills, item 5: from 300 m up the smoke was a flat
			// dark smudge. From a high steep eye, smoke's face toward the sun is lit: pale smoke and dust up to lit of the
			// way to lit_colour, black smoke (by its own colour's light, dark_lum) only dark_lit of the way to a warm
			// brown-grey, as black smoke never goes pale in the sun (her 19:59Z mend). Most on each puff's sunward side
			// (Lay) and on its cloud's; none against the sun, nor at night; never darker than it was)
			const double Face = std::max(0.0, (Eye.Eye - P).Dot(Eye.Sun) / Dist);
			const double Day = Clamp01((Eye.SunLight.R + Eye.SunLight.G + Eye.SunLight.B) / 3.0);
			const double Light = std::max(1e-3, 0.2126 * Eye.SmokeLight.R + 0.7152 * Eye.SmokeLight.G + 0.0722 * Eye.SmokeLight.B);
			const double Own = (0.2126 * Col.R + 0.7152 * Col.G + 0.0722 * Col.B) / Light;
			const double T = Clamp01((Own - AboveDarkLum.A) / std::max(AboveDarkLum.B - AboveDarkLum.A, 1e-6));
			const double Pale = Round.bOn ? 0.0 : T * T * (3.0 - 2.0 * T); // (1 for pale smoke and dust, 0 for black smoke and every column)
			const Rgb To = AboveDarkCol.Mix(AboveLitCol, static_cast<float>(Pale));
			const Rgb ToLift(std::max(0.0f, To.R * Eye.SmokeLight.R - Col.R), std::max(0.0f, To.G * Eye.SmokeLight.G - Col.G), std::max(0.0f, To.B * Eye.SmokeLight.B - Col.B));
			const double Most = (AboveDarkLit + (AboveLit - AboveDarkLit) * Pale) * FromAbove * Day * LitNow;
			if (Round.bOn)
			{
				// (a burning wreck's or an oil fire's column, lit as a round column: its top takes the light too, on its own
				// sunward side the most (Lay, RoundTopK), and its young smoke low over the fire as little of it as of the
				// sun's side)
				// (a column's top faces up, so it takes the light by the sun's height wherever the eye stands round it: all of
				// it from a sun 45 degrees up or higher)
				const double Young = B.SunBy > 0.0 ? B.SunYoung + (1.0 - B.SunYoung) * Clamp01((Age - B.SunFrom) / std::max(B.SunBy - B.SunFrom, 1e-3)) : 1.0;
				const double Up = Clamp01(1.4 * Eye.Sun.Unit(ZUp).Z);
				Round.Top = Clamp01(Most * Up * Young);
				Round.TopLift = ToLift;
			}
			else if (std::fabs(Lift.R) + std::fabs(Lift.G) + std::fabs(Lift.B) < 1e-6f)
			{
				double Side = 1.0;
				if (B.CloudShade > 0.0)
				{
					const Vec FromMid = P - (B.Centre + Wind * (0.75 * Age) + ZUp * (B.CentreRise * Age));
					const double Len = FromMid.Length();
					if (Len > 1e-3)
					{
						Side = Clamp01(0.6 + 0.6 * FromMid.Dot(Eye.Sun) / Len * Clamp01(Len / std::max(B.CloudR, 1e-3)));
					}
				}
				const float L = static_cast<float>(Clamp01(Most * Face * Side));
				if (L > 0.002f)
				{
					Lift = ToLift * L;
				}
			}
		}
		const double MetresAPixel = Dist / std::max(1.0, Eye.PixelsPerRadian); // (1 first: a scale that is not a number counts as 1)
		double StretchNow = B.StretchBy > 0.0 ? B.StretchFrom + (B.Stretch - B.StretchFrom) * Clamp01((Age - B.StretchAfter) / B.StretchBy) : B.Stretch; // (a pall drawn out as it runs; a spout's water as it falls)
		if (B.FlickerSeed != 0)
		{
			// (a flame's flicker, Bit::FlickerSeed: three waves by the clock, their rates and phases from the seed, so all
			// of one seat's pieces swell and sink together, smoothly, every frame. Each wave is from -1 to 1; the sum is
			// held to that too)
			auto Wave = [&B, Now](uint32_t Salt) { return Flickering(B.FlickerSeed, Salt, Now, B.FlickerLo, B.FlickerHi); };
			if (B.Flicker > 0.0)
			{
				A *= std::max(0.0, 1.0 + Clamp01(B.Flicker) * Wave(1));
			}
			if (B.FlickerLong > 0.0)
			{
				StretchNow = std::max(1.0, StretchNow) * (1.0 + Clamp01(B.FlickerLong) * Wave(2));
			}
			if (B.FlickerWide > 0.0)
			{
				Rad *= 1.0 + Clamp01(B.FlickerWide) * Wave(3);
			}
		}
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
		if (B.Anchor > 0.0 && B.Along.Length() > 0.5)
		{
			// (a tongue on its root, Bit::Anchor: drawn ahead of where it is by its own long half-way, so its root stays put)
			P = P + B.Along * (Rad * std::max(1.0, StretchNow) * std::min(B.Anchor, 1.0));
		}
		Vec AxA = Eye.Right * Rad, AxB = Eye.Up * Rad;
		Vec Middle = P;
		// (SetSoftContact: a piece lying on the ground, a thrown clod or fragment come to rest or a crater's mark, is
		// laid LieNearer nearer the eye and smaller to match, the same picture, so the scene's material, fading every
		// piece where it meets the ground, leaves it whole. A flat piece already that high over the ground (a fire's
		// glow, 0.2 m up) is laid where it is, and one lower only the rest of the way: pulled the whole way, a glow by
		// a wreck showed as much more of itself under the hull's foot. A spent case does as much by itself: LayCase)
		LayNear = 1.0;
		if (LieNearer > 0.0 && (B.Form == Bit::Shape::Flat || (B.bThrown && Age >= B.Land2 && B.Form != Bit::Shape::Plate)))
		{
			const double Off = (Middle - Eye.Eye).Length();
			const double Pull = B.Form == Bit::Shape::Flat ? LieNearer - std::max(0.0, Middle.Z - Height(Middle.X, Middle.Y)) : LieNearer;
			if (Pull > 0.0 && Off > 4.0 * Pull)
			{
				LayNear = 1.0 - Pull / Off;
				LayEye = Eye.Eye;
			}
		}
		Vec TaperB; // (a tracer: how its width runs along it, so it is as wide on the screen at both ends)
		double PointNow = B.Point, TailNow = B.Tail; // (a tracer cut short at an end is blunter there)
		double RimNow = B.Rim, InnerNow = InnerOwn, HeartUp = 0.0; // (a tracer by day is solid across: its edge and heart brought up)
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
					if (T.WakeBy > 0.0)
					{
						RadT = std::max(RadT, WakeWide(T.R0, T.WakeR, T.WakeBy, T.WakeOn, AgeT));
					}
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
							const double Bowed = std::max(LayBendFar.Length(), LayBendNear.Length()), Thin = std::min(Rad, RadT);
							if (B.bFarLong && Bowed > Thin && Bowed > 1e-9)
							{
								// (Bit::bFarLong: a long piece bowed more than its width folds over itself, a dark sliver drawn twice)
								LayBendFar = LayBendFar * (Thin / Bowed);
								LayBendNear = LayBendNear * (Thin / Bowed);
							}
						}
						// (the lump it draws in to at its old end: as wide as it is, and a share of its length long)
						AxA = Eye.Forward.Cross(Side).Unit(Eye.Right) * -std::max(Rad, 0.4 * RunLong);
						AxB = Side * Rad;
						Middle = (P + Far) * 0.5 + (LayBendFar - LayBendNear) * 0.125;
						LayLong = Run * 0.5;
						LayFar = SideT * RadT;
						bLayLong = true;
						LayTailAlpha = TailA / A;
						if (B.bSoft && !B.bLight)
						{
							// (soft: its far edge is the colour the piece behind is now, so the strip's colour runs through its joins)
							const Rgb FarCol = T.Col.Mix(T.Col2, Clamp01(AgeT / std::max(T.Life, 1e-6)) * T.Cool);
							const Rgb Own = B.Col.Mix(B.Col2, CoolF * B.Cool);
							LayFarBy = Rgb((FarCol.R - Own.R) * Eye.SmokeLight.R, (FarCol.G - Own.G) * Eye.SmokeLight.G, (FarCol.B - Own.B) * Eye.SmokeLight.B);
							LayFarFloor = LayFarBy * SkyFill;
							bLayFarCol = true;
						}
						LayLumpFar = LumpFar;
						if (B.Lean != 0.0 || T.Lean != 0.0)
						{
							bLayLean = true;
							LayLean = B.Lean;
							LayLeanFar = T.Lean;
						}
						if (B.EdgeSoft > 0.0)
						{
							LayWob = std::min(1.0, RunLong / (B.EdgeSoft * std::max({ Rad, RadT, 1e-3 })));
							if (B.Torn > 0.0)
							{
								LayWob *= 1.0 + B.Torn; // (edge_torn: its two sides in and out by more, a torn outline)
							}
						}
						if (B.SmearHold >= 0.0 && F > B.SmearHold)
						{
							LayLump = Clamp01((F - B.SmearHold) / std::max(1.0 - B.SmearHold, 1e-6)); // (its old end: drawing in to a round lump)
						}
						LayBillow = B.Billow * Clamp01(Age); // (growing over its first second, never there at once)
						LayBillowSeed = B.LumpSeed;
						if (bShowEach)
						{
							bLayShowEach = true;
							LayShowFrom = B.ShowFrom;
							LayShowBy = B.ShowBy;
							LayAgeNear = Age;
							LayAgeFar = AgeT;
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
			else if (Squat < 1.0)
			{
				// (Bit::LowBy: a low mound on the ground, drawn as its outline from where the eye is, as a skirt is: from the
				// side its low height, from above the round it covers)
				const Vec Flat_ = Vec(Eye.Right.X, Eye.Right.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
				const double Wide = Rad / std::sqrt(Squat), High = Rad * std::sqrt(Squat);
				const double Down = std::min(1.0, std::fabs(Eye.Forward.Z)); // (the sine of how steeply the eye looks down, or up)
				const Vec Upward = Flat_.Cross(Eye.Forward).Unit(ZUp);
				AxA = Flat_ * Wide;
				AxB = (Upward.Z < 0.0 ? -Upward : Upward) * std::sqrt(Wide * Wide * Down * Down + High * High * (1.0 - Down * Down));
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
			// (an air tracer's streak is so many seconds of its flight long, so it shortens as the round slows)
			const double HalfOwn = B.BySpeed > 0.0 ? std::min(B.HalfLength, std::max(0.25, 0.5 * B.BySpeed * Going(B, Age).Length())) : B.HalfLength;
			double Half = B.HalfLength > 0.0 ? HalfOwn * Share : Rad + L / 60.0; // (a clod is seen as a streak along its flight)
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
			double Wide = 1.0, DayRamp = 0.0, SoftGlowDay = 0.0;
			if (B.Glow > 0)
			{
				// (by day a tracer widens and its core whitens, so a stream holds on a pale sky; by dusk it is as ruled.
				// Day is read from the hour's smoke light: white by day, dim at dusk)
				const double Day = DayOf(Eye.SmokeLight);
				Wide = 1.0 + (TracerDayWide - 1.0) * Day;
				if (B.bOwnDay && B.Glow == 2 && AirWhiteBeyond > 0.0 && !(B.BurnOut > 0.0)) // (fx-air r2: a ship's tracer, with its own reach, keeps its colour near the eye)
				{
					// (art director 15:02Z: the core near white at close range; far off, its nation's own colour)
					const double Own = std::clamp(B.Over, 1.0, LitGain);
					PutK = (Own + (LitGain - Own) * Clamp01((AirWhiteBeyond - Deep) / std::max(AirWhiteBeyond - AirWhiteWithin, 1.0))) / LitGain;
				}
				if ((TracerDayPx > 0.0 || B.DayPx > 0.0) && Width > 1e-9)
				{
					// (by day its width is its own, in pixels where it is: the warm edge's whole width, or the core's)
					// (laid half a pixel wider each side, solid to half a pixel inside its width and fading out over that
					// one pixel: each pixel takes the share of it the line covers, so it is one straight line at any slope)
					const double PerPixel = Deep > 1e-3 ? std::min(MetresAPixel, Deep / Ppr) : MetresAPixel;
					const double Px = std::max(1.0, B.DayPx > 0.0 ? B.DayPx : (B.Glow == 2 ? TracerDayCorePx : TracerDayPx));
					const double ByDay = 0.5 * (Px + 1.0) * PerPixel;
					Wide = 1.0 + (ByDay / Width - 1.0) * Day;
					const bool bSoftGlow = B.bOwnDay && B.Glow == 1 && AirGlowSoft > 0.0 && AirBodyPx > 0.0;
					if (!bSoftGlow)
					{
						// (and solid: its whole opacity all along, its heart full)
						A += (std::min(1.0, A / std::max(B.Alpha, 1e-6)) - A) * Day;
					}
					HeartUp = Day;
					LaySquare = 0.8 * Day; // (and as wide from end to end: straight-sided, never a long thin lens)
					RimNow = B.Rim * (1.0 - Day);
					if (bSoftGlow)
					{
						// (art director 19:52Z: solid across, an air tracer's glow was a bar darker than a pale sky round a
						// sky-pale core, a hollow tube, and seen end-on a ring. It is strongest beside the core and fades
						// smoothly from the core's edge to nothing at its own, with no rim)
						InnerNow = B.Inner + (std::clamp(AirCorePx / AirBodyPx, 0.1, 0.9) - B.Inner) * Day;
						SoftGlowDay = Day;
					}
					else if (!(B.bOwnDay && B.Glow == 2 && AirCoreInner > 0.0))
					{
						InnerNow = B.Inner + ((Px - 1.0) / (Px + 1.0) - B.Inner) * Day;
					}
					DayRamp = PerPixel * Day; // (the same pixel's fade at its ends)
					if (B.bOwnDay)
					{
						// (a nation's own colours by day too, each burning its own Over past white)
					}
					else if (B.Glow == 1)
					{
						Col = Col.Mix(TracerDayEdge, Day);
					}
					else
					{
						PutK = (1.0 + (std::min(TracerDayOver, LitGain) - 1.0) * Day) / LitGain; // (the core white-hot by day)
					}
				}
				Width *= Wide;
				if (B.Glow == 2 && !B.bOwnDay)
				{
					Col = Col.Mix(TracerDayCore, Day);
				}
			}
			Half = std::max(Half, Width);
			const double LongCap = B.MostLong > 0.0 ? B.MostLong : ThrownLong; // (a wet clod's own, Bit::MostLong)
			if (LongCap > 0.0 && B.bThrown && !B.bLight && !(B.HalfLength > 0.0))
			{
				// (art director on cut 5: a fast clod drawn along its flight was a long dark slab across the men. A thrown
				// lump is a lump: never longer than ThrownLong times its width)
				Half = std::min(Half, LongCap * Width);
			}
			if (B.bChip && B.Glint > 0.0 && Age < B.Land2)
			{
				// (grounds.<ground>.chips: a stone splinter's fresh face catches the sun as it turns, a flash a few times a
				// second while it flies and skitters; none at night)
				const double Th = B.Turn + B.TurnRate * Age;
				const float Spot = static_cast<float>(B.Glint * DayOf(Eye.SmokeLight) * std::pow(std::fabs(std::cos(Th)), 12.0));
				Col = Rgb(Col.R + Spot * Eye.SunLight.R, Col.G + Spot * Eye.SunLight.G, Col.B + Spot * Eye.SunLight.B);
			}
			if (SoftGlowDay > 0.0)
			{
				// (and seen from behind or far off, short on the screen, the glow closed round its core as a ring: there it
				// thins to a faint halo, and the tracer is its core, a solid dot or dash)
				const double Long = Clamp01((Half / std::max(Width, 1e-9) - 1.5) / 4.5);
				A *= 1.0 - SoftGlowDay * 0.8 * (1.0 - Long * Long * (3.0 - 2.0 * Long));
			}
			AxA = Ua * Half;
			AxB = Ub * Width;
			if (DayRamp > 0.0)
			{
				LayInnerAlong = std::clamp(1.0 - DayRamp / std::max(B.HalfLength > 0.0 ? HalfOwn : Half, 1e-6), InnerNow, 0.999);
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
					double Long = std::max(HalfOwn, Width);
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
			if (B.BurnOut > 0.0 || B.TailFade > 0.0 || B.AlongK > 0.0)
			{
				// (fx-air r2, a ship's tracer or hose: each end as strong as how far it has flown leaves it (Bit::BurnOut),
				// the tail TailFade fainter than the head, evenly along it; and narrower as it dwindles)
				auto Spent_ = [&B](double Flown)
				{
					return B.BurnOut > B.DwindleFrom && Flown == Flown ? Clamp01((Flown - B.DwindleFrom) / (B.BurnOut - B.DwindleFrom)) : 0.0;
				};
				const double Own = B.AlongK > 0.0 ? std::min(B.AlongK, 1.0) : 1.0;
				const double Gone = 1.0 - std::clamp(B.DwindleTo, 0.0, 1.0);
				LayFadeHead = Own * (1.0 - Gone * Spent_((Middle + AxA - B.Origin).Length()));
				LayFadeTail = Own * (1.0 - Gone * Spent_((Middle - AxA - B.Origin).Length())) * (1.0 - std::clamp(B.TailFade, 0.0, 1.0));
				bLayFade = true;
				const double Narrow = 1.0 - (1.0 - std::clamp(B.DwindleWide, 0.1, 1.0)) * Spent_((Middle - B.Origin).Length());
				AxB = AxB * Narrow;
				TaperB = TaperB * Narrow;
			}
			break;
		}
		}
		if ((SkipPxLo > 0.0 || ThinScreens > 0.0) && Smoky(B))
		{
			// (FX9, game.smoke_budget: see SkipPxLo. Its size on the screen as it is laid: its two half-ways in pixels)
			const double Ha = AxA.Length() / std::max(MetresAPixel, 1e-9), Hb = AxB.Length() / std::max(MetresAPixel, 1e-9);
			const double Px = 2.0 * std::max(Ha, Hb);
			if (SkipPxLo > 0.0)
			{
				const double T = Clamp01((Px - SkipPxLo) / (SkipPxHi - SkipPxLo));
				A *= T * T * (3.0 - 2.0 * T);
			}
			if (A >= 0.004 && ScreenPx > 0.0)
			{
				// (its fill: each fan's area on the screen, as much of it as its box has on the screen and no more than the
				// screen. A fan lies square to the eye's forward, so one whose middle is within 0.1 m of the eye's plane, or
				// behind it, is all clipped away and fills nothing. The big share over thin_from_px, whole at a quarter as
				// wide again)
				const double T = Clamp01((Px - ThinFromPx) / (0.25 * ThinFromPx));
				const double Big = ThinScreens > 0.0 ? T * T * (3.0 - 2.0 * T) : 0.0;
				const double Fans = B.TwinPx > 0.0 && Room >= 2 && 2.0 * Rad >= B.TwinPx * MetresAPixel ? 2.0 : 1.0;
				const Vec Seen = Middle - Eye.Eye;
				const double Deep = Seen.Dot(Eye.Forward), Half = 0.5 * Px;
				double OnScreen = 0.0;
				if (Deep > 0.1 && Half > 0.0)
				{
					const double Sx = Seen.Dot(Eye.Right) / Deep * Eye.PixelsPerRadian, Sy = Seen.Dot(Eye.Up) / Deep * Eye.PixelsPerRadian;
					const double InX = std::max(0.0, std::min(Sx + Half, ScreenHalfX) - std::max(Sx - Half, -ScreenHalfX));
					const double InY = std::max(0.0, std::min(Sy + Half, ScreenHalfY) - std::max(Sy - Half, -ScreenHalfY));
					OnScreen = InX * InY / (4.0 * Half * Half);
					OnScreen = OnScreen == OnScreen ? Clamp01(OnScreen) : 1.0;
				}
				const double Area = std::min(Pi * Ha * Hb * OnScreen, ScreenPx) * Fans;
				FillSmall += Area * (1.0 - Big);
				FillBig += Area * Big;
				const double Share = ThinShare * Big;
				if (Share > 0.0)
				{
					// (its own lot, the same every frame, against the share left out: one whose lot is under that goes, by
					// degrees over ThinBand of lots so it fades and never goes at a stroke; the rest are as much darker as
					// the share of them still laid (Kept): the light they let through to that power, so a cloud of many
					// keeps its weight)
					const double Lot = static_cast<double>(Mix64(B.Serial * 0x9E3779B97F4A7C15ull + 0x5F9ull) >> 11) * (1.0 / 9007199254740992.0);
					const double L = Share * (1.0 + ThinBand) - ThinBand, W = Clamp01((Lot - L) / ThinBand);
					if (!(W > 0.0))
					{
						return 0;
					}
					const double Lo = Clamp01(L), Hi = Clamp01(L + ThinBand);
					const double Kept = std::max(0.05, ((Hi - L) * (Hi - L) - (Lo - L) * (Lo - L)) / (2.0 * ThinBand) + 1.0 - Hi);
					A = (1.0 - std::pow(1.0 - std::clamp(A, 0.0, 0.999), 1.0 / Kept)) * W;
				}
				FillLaid += Area;
			}
		}
		if (A < 0.004 && !bTailShows)
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
		// (Bit::FeatherPx: the heart ring no nearer the rim than leaves that many pixels of fade across its narrow way;
		// a piece too small for that fades from a fifth of the way out)
		const double InnerSoft = B.FeatherPx > 0.0 ? std::min(InnerOwn, std::max(0.2, 1.0 - B.FeatherPx * MetresAPixel / std::max(Rad, 1e-6))) : InnerOwn;
		const double ShadeNow = B.bLight ? 0.0 : (B.LowBy > 0.0 ? B.ShadeK * LitNow : B.ShadeK);
		LayHub = B.Hub != 0.0 && B.Hub == B.Hub ? AxA * -std::clamp(B.Hub, -0.9, 0.9) : Vec();
		LayShadowK = !B.bLight && B.ShadowK > 0.0 ? std::min(B.ShadowK, 1.0) : 0.0;
		LayHeartRound = B.HeartRound > 0.0 ? std::min(B.HeartRound, 1.0) : 0.0;
		LayShadow = Rgb(B.ShadowCol.R * Eye.SmokeLight.R, B.ShadowCol.G * Eye.SmokeLight.G, B.ShadowCol.B * Eye.SmokeLight.B);
		// (FX7, game.six_sided: under under_px across its long way on the screen, its own fans six-sided; see SixUnderPx)
		LaySix = SixUnderPx > 0.0 && 2.0 * std::max(AxA.Length(), AxB.Length()) < SixUnderPx * MetresAPixel;
		if (bShowEach && !bLayLong)
		{
			A *= ShowNear; // (too short to lay along: as in sight as its near end)
		}
		int Used = 1;
		if (B.TwinPx > 0.0 && Room >= 2 && 2.0 * Rad >= B.TwinPx * MetresAPixel)
		{
			// (art director 00:03Z: a near flak puff is two fans, the second turned half a step round the same outline, so the
			// two together have twice the sides and no straight facet shows. Each takes the opacity that makes the two
			// together what one would be)
			const double Step = Pi / Sides;
			const Vec AxA2 = AxA * std::cos(Step) + AxB * std::sin(Step);
			const Vec AxB2 = AxB * std::cos(Step) - AxA * std::sin(Step);
			auto Half = [](double X) { return 1.0 - std::sqrt(1.0 - Clamp01(X)); };
			Lay(Slot, Middle, AxA, AxB, Col, Half(A), Half(A * Heart), Half(A * B.Rim), InnerSoft, B.Point, B.Tail, ShadeNow, Eye, B.Rough, B.LumpSeed, Turned, Floor, Through, Outward, Lift);
			Lay(Slot + 1, Middle, AxA2, AxB2, Col, Half(A), Half(A * Heart), Half(A * B.Rim), InnerSoft, B.Point, B.Tail, ShadeNow, Eye, B.Rough, B.LumpSeed, Turned - Step, Floor, Through, Outward, Lift);
			Used = 2;
		}
		else
		{
			Lay(Slot, Middle, AxA, AxB, Col, A, A * Heart, A * RimNow, B.FeatherPx > 0.0 ? std::min(InnerNow, InnerSoft) : InnerNow, PointNow, TailNow, ShadeNow, Eye, B.Rough, B.LumpSeed, Turned, Floor, Through, Outward, Lift, TaperB);
		}
		LayShadowK = 0.0; // (its own only: what is laid next has none)
		LayHeartRound = 0.0;
		LaySix = false;
		B.bShown = true;
		const int LumpsNow = std::min(B.Lumps, 8);
		const double LumpsStay = LumpsNow > 0 && !B.InTail(Shown) ? LumpsLeft(B.HoldOfLooks(), F) : 0.0;
		// (the lumps' cost held, see LumpsMostPuffs: with too many puffs wide enough, only the widest carry lumps
		// (LumpsTight); one as wide as the screen carries none; and lumps too thin to be seen are not laid)
		// LumpsBig: how much of its lumps a puff wider than lumps_px_most.0 keeps: all at that width, none by the width
		// at which its worth is down to its own lumps_px, a little short of lumps_px_most.1)
		const double LumpsFromPx = B.LumpsPx * LumpsTight;
		const double LumpsPxNow = 2.0 * Rad / std::max(MetresAPixel, 1e-9);
		const bool bLumpsWide = LumpsBigB > 0.0 && LumpsPxNow > LumpsBigA;
		const double LumpsWorthNow = bLumpsWide ? LumpsWorth(LumpsPxNow, LumpsBigA, LumpsBigB) : LumpsPxNow;
		const double LumpsBig = bLumpsWide ? Clamp01((LumpsWorthNow - B.LumpsPx) / std::max(LumpsBigA - B.LumpsPx, 1e-6)) : 1.0;
		const double LumpsCame = bLumpsWide ? Clamp01((LumpsWorthNow - LumpsFromPx) / std::max(LumpsBand * LumpsFromPx, 1e-6)) : Clamp01((2.0 * Rad / std::max(MetresAPixel, 1e-9) - LumpsFromPx) / std::max(LumpsBand * LumpsFromPx, 1e-6));
		const double LumpsInk = LumpsNow > 0 ? A * (B.LumpsAlpha == B.LumpsAlpha ? Clamp01(B.LumpsAlpha) : 1.0) * LumpsCame * LumpsStay * LumpsRoom * LumpsBig : 0.0;
		if (LumpsNow > 0 && B.LumpsPx > 0.0 && LumpsStay > 0.0 && LumpsRoom > 0.0 && Room >= Used + LumpsNow && (bLumpsWide ? LumpsWorthNow >= LumpsFromPx && LumpsBig > 0.0 : 2.0 * Rad >= LumpsFromPx * MetresAPixel) && (!(LumpsLeastInk > 0.0) || LumpsInk >= LumpsLeastInk))
		{
			// (Bit::Lumps, the art director on build 1766's Midway ships picture: Akagi's column was the right shape and
			// colour and read as a soft brown smear. Its puffs are 60 to 90 m across, 300 to 400 pixels of the window
			// from a kilometre, each one fan whose edge fades over two fifths of its radius: at that size no lump has
			// an outline. Each lump's place, size and shade are its own, from the puff's own seed: no dice are thrown,
			// so nothing else is laid differently for them. They come in as the puff passes LumpsPx across and are
			// whole at a quarter as wide again, and melt into the puff before it begins to thin (LumpsLeft), so none
			// appears or goes at a stroke; and thin as the room fills (LumpsRoom, set by Draw))
			const double AOf = LumpsInk;
			const double R0Of = B.LumpsR0 == B.LumpsR0 ? std::clamp(B.LumpsR0, 0.05, 1.0) : 0.3, R1Of = B.LumpsR1 == B.LumpsR1 ? std::clamp(B.LumpsR1, 0.05, 1.0) : 0.45;
			const double Out0Of = B.LumpsOut0 == B.LumpsOut0 ? std::clamp(B.LumpsOut0, 0.0, 1.0) : 0.3, Out1Of = B.LumpsOut1 == B.LumpsOut1 ? std::clamp(B.LumpsOut1, 0.0, 1.0) : 1.0;
			const double InnerOf = B.LumpsInner > 0.0 ? std::min(B.LumpsInner, 0.95) : InnerSoft; // (over 0: where a lump's own heart ring sits; else as the puff's)
			if (Round.bOn)
			{
				Round.Lobe = B.LumpsLight == B.LumpsLight ? std::clamp(B.LumpsLight, 0.0, 1.0) : 0.6;
				Round.Rim = B.LumpsRim == B.LumpsRim ? std::clamp(B.LumpsRim, 0.0, 1.0) : 0.8;
			}
			LayHub = Vec();
			// (the art director, 11:29Z: no two puffs the same pattern. The sizes are dealt: lumps_r is cut in as many
			// steps as there are lumps and each lump takes one, by the puff's own seed, so every puff carries a big
			// lump, small ones and the sizes between, and which stands where differs from puff to puff)
			int Deal[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
			uint32_t Dealt = (B.LumpSeed * 2654435761u + 0x51ED270Bu) | 1u;
			for (int K = LumpsNow - 1; K > 0; --K)
			{
				Dealt ^= Dealt << 13;
				Dealt ^= Dealt >> 17;
				Dealt ^= Dealt << 5;
				std::swap(Deal[K], Deal[(Dealt >> 8) % static_cast<uint32_t>(K + 1)]);
			}
			for (int K = 0; K < LumpsNow; ++K)
			{
				uint32_t Mixed = (B.LumpSeed ^ (0x9E3779B9u * static_cast<uint32_t>(K + 1))) | 1u;
				auto Next = [&Mixed]()
				{
					Mixed ^= Mixed << 13;
					Mixed ^= Mixed >> 17;
					Mixed ^= Mixed << 5;
					return (Mixed & 0xFFFFFFu) / 16777216.0;
				};
				Next();
				const double Angle = Turned + 2.0 * Pi * (K + 0.8 * (Next() - 0.5)) / LumpsNow;
				const double OutShare = Out0Of + (Out1Of - Out0Of) * Next();
				const double Size = R0Of + (R1Of - R0Of) * (Deal[K] + Next()) / LumpsNow;
				const double OutBy = (1.0 - Size) * OutShare; // (never past the puff's own radius: no lump stands free of it)
				const float Tone = static_cast<float>(0.88 + 0.24 * Next());
				const Vec LumpAt = Middle + (AxA * std::cos(Angle) + AxB * std::sin(Angle)) * OutBy;
				Lay(Slot + Used + K, LumpAt, AxA * Size, AxB * Size, Col * Tone, AOf, AOf * Heart, 0.0, InnerOf, 0.0, 0.0, ShadeNow, Eye, B.Rough, Mixed, Angle, Floor, Through, Outward, Lift);
			}
			Used += LumpsNow;
		}
		return Used;
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

	int Battle::LayShadows(int Slot, double Now, const View& Eye)
	{
		// (game.from_above: seen from high above, the big smoke lays its shadow on the ground away from the sun, one puff
		// in shadow_every of it, each shadow its own puff's as it drifts, grows and thins (LayBit, Bit::bShadeTwin). The
		// art director's read of the general's stills, item 5: from 300 m up the smoke was a flat smudge. None at night,
		// nor with the sun so low its shadows would run off over the field)
		const double Day = Clamp01((Eye.SunLight.R + Eye.SunLight.G + Eye.SunLight.B) / 3.0);
		const Vec Sun = Eye.Sun.Unit(ZUp);
		const double Strength = AboveShadow * FromAbove * Day;
		// (the room: never smoke's own. What the rest of the picture left last Draw, less a twentieth to spare, at most an
		// eighth of the room and shadow_most, eased by 25 a frame so the shadows come and go softly as the room fills)
		const int Cap = std::min(AboveMost, std::max(0, MaxPuffs / 8));
		const int Want = std::clamp(MaxPuffs - (LastDrawn - ShadowsWere) - MaxPuffs / 20, 0, Cap);
		ShadowRoom = std::clamp(Want, ShadowRoom - 25, ShadowRoom + 25);
		ShadowsWere = 0;
		if (!(Strength >= 0.004) || !(Sun.Z >= 0.15) || ShadowRoom <= 0)
		{
			return 0;
		}
		ShadowCands.clear();
		ShadowCells.clear();
		for (size_t I = 0; I < Bits.size(); ++I)
		{
			const Bit& B = Bits[I];
			// (smoke and dust of some size only, a burning wreck's own column too: never a light, a clod, an aircraft's trail
			// or a funnel's plume)
			if (B.bLight || B.Form != Bit::Shape::Puff || B.Ribbon > 0.0 || B.Smear > 0.0 || B.bThrown || B.Hose > 0.0 || B.Even >= 0
				|| B.Serial % static_cast<uint64_t>(AboveEvery) != 0 || !(std::max(B.R0, B.R1) >= AboveLeastR) || !(B.Alpha > 0.05))
			{
				continue;
			}
			const double Age = Now - B.Born;
			if (Age < 0.0 || !(Age <= B.Life) || Now > B.CutAt)
			{
				continue;
			}
			const Vec At = Where(B, Age);
			if (!(At.Z - Height(At.X, At.Y) >= AboveLeastM)) // (a puff low on the ground hides its own shadow)
			{
				continue;
			}
			ShadowCands.emplace_back(B.Serial, static_cast<uint32_t>(I));
		}
		// (the oldest first: when the room is short the newest go, and the last few laid fade, so none blinks)
		const int Room = std::min(ShadowRoom, MaxPuffs - Slot);
		if (static_cast<int>(ShadowCands.size()) > Room)
		{
			std::nth_element(ShadowCands.begin(), ShadowCands.begin() + Room, ShadowCands.end());
			ShadowCands.resize(static_cast<size_t>(std::max(Room, 0)));
		}
		std::sort(ShadowCands.begin(), ShadowCands.end());
		const double Spin = std::atan2(Sun.Y, Sun.X), Long = std::min(AboveStretch, 1.0 / Sun.Z); // (a round puff's shadow, drawn out along the sun's way)
		const bool bShort = static_cast<int>(ShadowCands.size()) >= ShadowRoom;
		int Took = 0;
		for (size_t K = 0; K < ShadowCands.size() && Slot + Took < MaxPuffs; ++K)
		{
			const Bit& B = Bits[ShadowCands[K].second];
			Bit S = B;
			S.bShadeTwin = true;
			S.Form = Bit::Shape::Flat;
			S.Spin = Spin;
			S.Stretch = Long;
			S.R0 *= AboveWide;
			S.R1 *= AboveWide;
			S.RMid *= AboveWide;
			S.WakeR *= AboveWide;
			S.Col = S.Col2 = AboveShadowCol;
			S.Cool = 0.0;
			S.TailPale = 0.0;
			S.ShadeK = S.ShadowK = S.CloudShade = S.FaceLit = S.SunSide = S.GlareK = S.PaleUpM = 0.0;
			S.Lumps = 0;
			S.TwinPx = S.EdgeK = S.FootK = S.Rest = S.Nearer = 0.0;
			S.bHug = S.bNeverUnder = false;
			S.FlickerSeed = 0;
			S.DayAlpha = S.LitOver = 1.0;
			// (soft from its middle out: a shadow has no rim of its own, and the higher its puff the softer and wider its edge,
			// as the sun's own width blurs it: the art director's 19:59Z mend)
			const Vec Over = Where(B, Now - B.Born);
			const double Soft = Clamp01((Over.Z - Height(Over.X, Over.Y)) / AboveSoftM);
			S.Heart = 0.8 - 0.15 * Soft;
			S.Inner = 0.45 - 0.2 * Soft;
			S.R0 *= 1.0 + 0.2 * Soft;
			S.R1 *= 1.0 + 0.2 * Soft;
			S.RMid *= 1.0 + 0.2 * Soft;
			S.WakeR *= 1.0 + 0.2 * Soft;
			S.Rim = 0.0;
			S.HeartEnd = -1.0;
			S.HeartRound = 1.0;
			S.Rough = std::min(S.Rough, 0.25);
			const int FromEnd = ShadowRoom - static_cast<int>(K);
			ShadeTwinA = Strength * (bShort && FromEnd < 20 ? Clamp01(FromEnd / 20.0) : 1.0);
			Took += LayBit(Slot + Took, S, Now, Eye, MaxPuffs - Slot - Took);
		}
		ShadowsWere = Took;
		return Took;
	}

	int Battle::Draw(double Now, const View& Eye)
	{
		LayStrikes(Now); // (the bombs of a stick due by now, by the eye of the last frame: BombStick)
		LastEye = Eye.Eye;
		LookDay = DayOf(Eye.SmokeLight); // (Effects (R2): a gun's own look by day)
		LastSun = Eye.Sun;
		LastPpr = Eye.PixelsPerRadian;
		bEyeKnown = true;
		{
			// (the lamp rule's view, SetLampRule's bByEye: a lamp whose light cannot reach the screen ranks nothing. Kept
			// here and read only by the rule)
			const double Up_ = Eye.HalfUp > 0.0 ? Eye.HalfUp : 0.5625 * Eye.HalfAcross;
			const double Diag = std::sqrt(Eye.HalfAcross * Eye.HalfAcross + Up_ * Up_);
			const double Ahead = Eye.Forward.Length();
			SightAhead = Ahead > 1e-9 && std::isfinite(Ahead) ? Eye.Forward * (1.0 / Ahead) : Vec();
			SightHalf = Eye.HalfAcross > 0.0 && std::isfinite(Diag) && Ahead > 1e-9 && std::isfinite(Ahead) ? std::atan(Diag) : 0.0;
		}
		// (FX9, game.smoke_budget: the screen's pixels, for the smoke's fill. The view's width not given: 0, and none of
		// the big smoke is thinned)
		{
			const double Across = 2.0 * Eye.HalfAcross * Eye.PixelsPerRadian, Up = 2.0 * (Eye.HalfUp > 0.0 ? Eye.HalfUp : 0.5625 * Eye.HalfAcross) * Eye.PixelsPerRadian;
			ScreenPx = Across > 0.0 && Up > 0.0 && Across * Up < 1.0e12 ? Across * Up : 0.0;
			ScreenHalfX = ScreenPx > 0.0 ? 0.5 * Across : 0.0;
			ScreenHalfY = ScreenPx > 0.0 ? 0.5 * Up : 0.0;
			FillSmall = FillBig = FillLaid = 0.0;
		}
		// (how far the eye came and turned since the last frame: see EyeMoved)
		EyeMoved = bEyeWas ? (Eye.Eye - EyeWas).Length() : 0.0;
		EyeTurned = bEyeWas ? std::max(std::acos(std::clamp(Eye.Forward.Dot(AheadWas), -1.0, 1.0)), std::acos(std::clamp(Eye.Up.Dot(UpWas), -1.0, 1.0))) : 0.0;
		EyeWas = Eye.Eye;
		AheadWas = Eye.Forward;
		UpWas = Eye.Up;
		bEyeWas = true;
		Run(Now);
		CutTrails();
		// (Bit::Serial: what was made since the last Draw lies at the end of Bits in the order it was made; numbered so
		// before Prune moves anything)
		for (size_t I = std::min(Numbered, Bits.size()); I < Bits.size(); ++I)
		{
			Bits[I].Serial = ++Serials;
		}
		Prune(Now);
		RunBars.clear();
		for (const Trailing& Each : Trails)
		{
			if (Each.BarsLaid > 0 || Each.FarK > 1.0)
			{
				RunBars.emplace_back(Each.Run, Each.BarsLaid);
			}
		}
		std::sort(RunBars.begin(), RunBars.end());
		++FrameNo;
		// (art director 06:48Z: on the overview, or any eye above about 25 m looking steeply down, the dust within about
		// 10 m of each vehicle or gun thins so each shows at least half; the long trail behind stays, a column's tell. A
		// ground or follow eye keeps the full veil)
		{
			auto Smooth = [](double X, const Span& R) { const double T = Clamp01((X - R.A) / std::max(R.B - R.A, 1e-6)); return T * T * (3.0 - 2.0 * T); };
			SteepThin = Smooth(Eye.Eye.Z - Height(Eye.Eye.X, Eye.Eye.Y), ThinAbove) * Smooth(-Eye.Forward.Z, ThinSteep);
			// (game.from_above, SetFromAbove: the same high steep eye, by its own numbers, for smoke's lit face and shadows)
			FromAbove = bFromAboveAsked && bAboveOn ? Smooth(Eye.Eye.Z - Height(Eye.Eye.X, Eye.Eye.Y), AboveEye) * Smooth(-Eye.Forward.Z, AboveSteep) : 0.0;
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
			if (M.bFarCull)
			{
				// (FX3, burst.far: a crater's mark past gone_beyond_m and under gone_px across takes no place. The marks
				// are laid first and stay all battle, up to 2,400 of them: at the wide shot of a long barrage they held
				// a third of the room, and the far line's bursts were left out for them)
				const double Away = (M.From - Eye.Eye).Length();
				if (Away > LandGoneBeyond && 2.0 * M.R0 * std::max(1.0, M.Stretch) * std::max(Eye.PixelsPerRadian, 1.0) < LandGonePx * Away)
				{
					continue;
				}
			}
			Slot += LayBit(Slot, M, Now, Eye, MaxPuffs - Slot);
		}
		if (FromAbove > 0.0)
		{
			Slot += LayShadows(Slot, Now, Eye); // (the big smoke's shadows lie on the ground too, under everything but the craters)
		}
		else
		{
			ShadowRoom = ShadowsWere = 0;
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
			if (Row.Plume.bOn)
			{
				continue; // (a funnel's plume starts at its lip: its newest puff is never more than a third of its width from it)
			}
			const Vec Way = Tr.Carry.Unit(Vec(1.0, 0.0, 0.0));
			const Vec Head = Tr.At + Tr.Carry * (Now - Tr.Last) - Way * Tr.Tip;
			const double Gap = std::min((Head - Tr.LastPuffAt).Dot(Way), 2.0 * Row.Spacing * std::max(Tr.FarK, 1.0)); // (the far form: its root reaches back as much farther to the newest puff, so the smoke behind the fire is never pale)
			const double Ramp = Clamp01((Now - Tr.KindBorn) / Row.RampS);
			if (Row.SpurtM.B > 0.0)
			{
				// (art director 19:52Z 20 November, glycol: a denser, wider spurt for the first two or three metres behind the
				// radiator, where the coolant leaves under pressure before the airflow thins it. One soft piece riding with the
				// aircraft, its length pulsing a little, thinning as the row's trail does while the coolant runs out)
				const double Thinned = Tr.FadeFor > 0.0 ? 1.0 - (1.0 - Row.FadeTo) * Clamp01((Now - Tr.KindBorn) / Tr.FadeFor) : 1.0;
				const double Beat = 0.5 + 0.5 * std::sin(Now * 9.0 + Tr.Id * 2.4) * std::sin(Now * 3.7 + Tr.Id);
				Bit S;
				S.Born = Now - 0.001;
				S.Life = 1.0;
				S.R0 = S.R1 = Row.R0 * Row.SpurtWide * (0.7 + 0.3 * Ramp);
				S.From = Tr.At + Tr.Carry * (Now - Tr.Last) - Way * S.R0;
				S.Along = Way;
				S.Smear = Row.SpurtM.A + (Row.SpurtM.B - Row.SpurtM.A) * Beat;
				S.Col = Row.Col.Mix(Row.ColFull, Clamp01((Now - Tr.KindBorn) / Row.DarkenS));
				S.Col2 = S.Col;
				S.Alpha = std::min(0.9, Row.Alpha * Row.SpurtAlpha * Ramp) * Thinned;
				if (Row.ShadowK > 0.0)
				{
					S.ShadowCol = Row.Shadow;
					S.ShadowK = Row.ShadowK;
				}
				S.FadeIn = 0.0005;
				S.Hold = 1.0;
				S.Heart = 0.55;
				S.Rim = 0.0;
				S.Rough = 0.3;
				S.LumpSeed = static_cast<uint32_t>(Tr.Id) * 2654435761u + 7u;
				Roots.push_back(S);
			}
			if (Gap <= 0.05)
			{
				continue;
			}
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
			if (Tr.BlendFor > 0.0 && Tr.Was.bOn && Now - Tr.KindBorn < Tr.BlendFor)
			{
				// (blend_s: going over to this kind, as its pieces are)
				const double Blent = Clamp01((Now - Tr.KindBorn) / Tr.BlendFor), BlendK = Blent * Blent * (3.0 - 2.0 * Blent);
				B.Col = Tr.Was.Col.Mix(B.Col, BlendK);
				B.Col2 = B.Col;
				B.Alpha = Tr.Was.Alpha + (B.Alpha - Tr.Was.Alpha) * BlendK;
				B.R0 = B.R1 = Tr.Was.R0 + (B.R0 - Tr.Was.R0) * BlendK;
				B.ShadowCol = Row.ShadowK <= 0.0 ? Tr.Was.Shadow : (Tr.Was.ShadowK <= 0.0 ? Row.Shadow : Tr.Was.Shadow.Mix(Row.Shadow, BlendK));
				B.ShadowK = Tr.Was.ShadowK + (Row.ShadowK - Tr.Was.ShadowK) * BlendK;
			}
			else if (Row.ShadowK > 0.0)
			{
				B.ShadowCol = Row.Shadow;
				B.ShadowK = Row.ShadowK;
			}
			B.bSoft = Row.bSoft && B.Ribbon > 0.0;
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
			if (Now < B.Born || ((Now > B.Born + B.Life || Now > B.CutAt) && !Unseen(B)))
			{
				continue;
			}
			const Vec At = Where(B, Now - B.Born);
			if ((At - Eye.Eye).Length() > (B.FarRep >= 2 ? FarBeyond : (B.bKeep ? KeepBeyond : Beyond)) || (B.FarRep != 0 && !(FarShare(B, Eye.Eye) > 0.0)))
			{
				// (what LayBit will not lay for distance never takes a place: counted first, a far light held a slot it
				// never filled, and the barrage in front of the eye was cut for it. The pre-check, 05:33Z)
				continue;
			}
			// (thrown dirt is a streak held at a floor of its own, clod_least_px, made for a field seen from 20 m: far off
			// at sea it is left out by its true size all the same)
			const bool bDirt = B.Form == Bit::Shape::Streak && B.bThrown && !B.bFired;
			if (SeaGonePx > 0.0 && !B.bKeep && !B.bLight && (bDirt || (!(B.LeastPx > 0.0) && B.Form != Bit::Shape::Streak && B.Form != Bit::Shape::Plate)))
			{
				// (a sea battle, UseSea: beyond the air's distance, where nothing used to be drawn at all, a piece of smoke,
				// dust, dirt or water that would stand under sea.gone_px across at its biggest takes no place and is not
				// laid. A hit on the far line throws over a thousand pieces a metre across that no one can see from 10 km,
				// and two such hits were the room. Lights, kept columns and whatever else has a floor of its own are left
				// alone)
				const double Away = (At - Eye.Eye).Length();
				if (Away > SeaGoneBeyond)
				{
					const double Grown = Clamp01((Now - B.Born) / std::max(B.Life, 1e-6));
					const double Most = B.bTail ? std::max(B.R0, B.R1) * (1.0 + std::clamp(B.TailWide, 0.0, 4.0)) * std::max(1.0, B.Stretch) : std::max(std::max(B.R0, B.R0 + (B.R1 - B.R0) * std::pow(Grown, B.Grow)) * std::max(1.0, B.Stretch), B.HalfLength);
					if (2.0 * Most * std::max(Eye.PixelsPerRadian, 1.0) < SeaGonePx * Away)
					{
						continue;
					}
				}
			}
			if (B.bFarCull && !B.bKeep && !B.bLight && ((bDirt && bLandGoneDirt) || (!(B.LeastPx > 0.0) && B.Form != Bit::Shape::Streak && B.Form != Bit::Shape::Plate)))
			{
				// (FX3, burst.far: a land hit's piece past gone_beyond_m that would stand under gone_px across at its
				// biggest takes no place and is not laid. Its flash, and what has a floor of its own on the screen, are
				// left alone; its thrown dirt too, unless gone_dirt)
				const double Away = (At - Eye.Eye).Length();
				if (Away > LandGoneBeyond)
				{
					const double Grown = Clamp01((Now - B.Born) / std::max(B.Life, 1e-6));
					const double Most = B.bTail ? std::max(B.R0, B.R1) * (1.0 + std::clamp(B.TailWide, 0.0, 4.0)) * std::max(1.0, B.Stretch) : std::max(std::max(B.R0, B.R0 + (B.R1 - B.R0) * std::pow(Grown, B.Grow)) * std::max(1.0, B.Stretch), B.HalfLength);
					if (2.0 * Most * std::max(Eye.PixelsPerRadian, 1.0) < LandGonePx * Away)
					{
						continue;
					}
				}
			}
			if (SkipPxLo > 0.0 && Smoky(B) && 2.0 * MostRad(B, Now) * std::max(Eye.PixelsPerRadian, 1.0) < SkipPxLo * (At - Eye.Eye).Length())
			{
				// (FX9, game.smoke_budget.skip_px: a piece of smoke under skip_px.0 across at its biggest takes no place and
				// is not laid, near or far. At the wide shot of a barrage 518 of the 3,000 places went to smoke under 2 px
				// across, specks no one reads, and the battle in front of the eye was cut for them. It comes in over
				// skip_px.1, in LayBit, so none appears at a stroke)
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
				Sorted.push_back(Order{ &B, (At - Eye.Eye).Dot(Eye.Forward) - B.Nearer, B.bKeep || B.bLight || (B.FarRep >= 2 && bFarKept), B.Lumps > 0 ? (At - Eye.Eye).Length() : 0.0, B.Serial });
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
		if (ShareDustFloor >= 0.0)
		{
			// (FX5: the room the rest asked of late, its highest over the last dust_memory_s, in eight steps; dust lives
			// long, so it is laid for the barrage's peaks, not for a lull between salvos)
			const int64_t PeakSlot = static_cast<int64_t>(std::floor(Now / (ShareDustMemory / 8.0)));
			if (PeakSlot != SharePeakSlot)
			{
				const bool bFresh = SharePeakSlot == std::numeric_limits<int64_t>::min() || PeakSlot < SharePeakSlot || PeakSlot - SharePeakSlot >= 8;
				for (int64_t S = bFresh ? PeakSlot - 7 : SharePeakSlot + 1; S <= PeakSlot; ++S)
				{
					SharePeaks[static_cast<size_t>(S & 7)] = 0.0;
				}
				SharePeakSlot = PeakSlot;
			}
			double& Here = SharePeaks[static_cast<size_t>(PeakSlot & 7)];
			Here = std::max(Here, static_cast<double>(RoomGate()));
			SharePeak = *std::max_element(SharePeaks, SharePeaks + 8);
			// (how thin the far trails are laid: up while the dust holds more than the rest leaves it, between dust_top of
			// the room less that peak and dust_floor of it, down while it holds less; over dust_thin_s, and never past
			// dust_most_thin, so every moving vehicle keeps a line)
			const double Most = static_cast<double>(std::max(1, MaxPuffs));
			const double Allowed = std::clamp(ShareDustTop * Most - SharePeak, ShareDustFloor * Most, DustRoom * Most);
			const double Since = Now - ShareThinAt;
			if (Since > 0.0 && Since < 1.0)
			{
				const double Over = std::clamp(static_cast<double>(DustInSight) / std::max(Allowed, 1.0), 0.25, 4.0);
				// (the trails are taken back in the order they were let go: none thickened while any is let go)
				const double Thinner = ShareThin * std::pow(Over, Since / ShareDustThinS);
				ShareThin = std::clamp(ShareDrop > 0.0 ? std::max(Thinner, ShareThin) : Thinner, 1.0, ShareDustMostThin);
				// (thinned all it may, and the rest's peak of late with the dust past drop_from of the room, so the farthest
				// bursts would be left out at the next salvo: let more far trails go, over dust_thin_s; the dust lives
				// seconds, so it is let go ahead of the peak, not at it. Under drop_back, take them back half as fast)
				const double Full = (SharePeak + static_cast<double>(DustInSight)) / Most;
				const bool bShort = ShareThin >= ShareDustMostThin - 1.0e-6 && Full > ShareDropFrom && DustInSight > 0;
				ShareDrop = Clamp01(ShareDrop + (bShort ? 1.0 : Full < ShareDropBack ? -0.5 : 0.0) * Since / ShareDustThinS);
			}
			ShareThinAt = Now;
		}
		for (size_t I = 0; I < Roots.size(); ++I)
		{
			// (a trail's root, laid for this frame alone: after every piece of Bits, in the order laid)
			const Bit& B = Roots[I];
			Sorted.push_back(Order{ &B, (B.From - Eye.Eye).Dot(Eye.Forward), false, 0.0, (uint64_t{1} << 62) + I });
		}
		// (far to near; two as deep in the order they were made: Bit::Serial)
		std::sort(Sorted.begin(), Sorted.end(), [](const Order& L, const Order& R) { return L.Deep != R.Deep ? L.Deep > R.Deep : L.Serial < R.Serial; });
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
				// (the pre-check on 86d71f2: one that lingers grows by the life it was laid with and is TailWide wider again by
				// its tail's end. Counted by its long life, a lingering flak puff was taken for one fan where two were laid,
				// the count came short, and the room ran out before the nearest)
				const double Grown = Clamp01((Now - Piece.Born) / std::max(Piece.LifeOfLooks(), 1e-6));
				const double Most = std::max(Piece.R0, Piece.R0 + (Piece.R1 - Piece.R0) * std::pow(Grown, Piece.Grow)) * (Piece.bTail ? 1.0 + std::clamp(Piece.TailWide, 0.0, 4.0) : 1.0);
				bMayTwin = Piece.TwinPx <= Piece.LeastPx || 2.0 * Most * std::max(Eye.PixelsPerRadian, 1.0) >= Piece.TwinPx * O.Deep;
			}
			return bMayTwin ? 2u : 1u;
		};
		// (Bit::Lumps: the lumps a big puff may lay on its face, each a slot, counted as a twin is: by its size now, at
		// the most, against its depth, so the count never comes short)
		auto MayLump = [&](const Order& O) -> size_t
		{
			const Bit& Piece = *O.B;
			if (Piece.Lumps <= 0 || !(Piece.LumpsPx > 0.0))
			{
				return 0u;
			}
			const double Grown = Clamp01((Now - Piece.Born) / std::max(Piece.LifeOfLooks(), 1e-6));
			const double RadNow = Piece.R0 + (Piece.R1 - Piece.R0) * std::pow(Grown, Piece.Grow);
			const double Most = std::max(Piece.R0, RadNow);
			if (LumpsBigB > 0.0 && 1.1 * LumpsWorth(2.0 * RadNow * std::max(Eye.PixelsPerRadian, 1.0) / std::max(O.Far, 1e-3), LumpsBigA, LumpsBigB) < Piece.LumpsPx * LumpsTight)
			{
				// (too wide on the screen to be worth its lumps now, with a tenth to spare: see LumpsWorth)
				return 0u;
			}
			return LumpsLeft(Piece.HoldOfLooks(), Grown) > 0.0 && !Piece.InTail(Now - Piece.Born) && 2.0 * Most * std::max(Eye.PixelsPerRadian, 1.0) >= Piece.LumpsPx * LumpsTight * O.Deep ? static_cast<size_t>(std::min(Piece.Lumps, 8)) : 0u;
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
		size_t Need = Kept;
		for (; Start > 0; --Start)
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
		// (the lumps take only places nothing else wants: they are not in the count above, nor in the room asked, so
		// no smoke is shed and no far piece dropped for them. They are whole while a tenth of the room would still
		// be free with them all laid, thinner as that goes, and not laid once the room is full; so a crowded sky
		// costs a near column its billows, smoothly, and nothing else. With something already left out for want of
		// room: none)
		// (the pre-check on f6cb25c: judged by this frame's count alone, the lumps' ink stepped with every piece born
		// or gone once the room was in its last tenth. A fire's flames come and go by ten places a frame or more, so
		// every lump on the screen flickered with them, and blinked out for a frame where the count touched the
		// room. So the room is judged by the most asked of late, LumpsMost. A new most is taken at once; a count
		// within a hundredth of the room of it lets it stand; left untouched for two seconds it comes down by a
		// hundredth of the room a second, so the lumps come back slowly. And what is shown falls by no more than
		// a half a second, so a rise in the count is a fade and not a step; held under three times what this
		// frame's count alone would give, so it is still at nothing when the room is full. The lumps are
		// laid, as before, only while this frame's count has a place for every one of them. A second or more
		// since the last Draw, time gone back, or a cut: no memory, this frame's count alone)
		LumpsRoom = 0.0;
		const double LumpsTenth = std::max(1.0, 0.1 * MaxPuffs);
		const double LumpsSince = Now - LumpsAt;
		// (a cut: since the last frame the eye came over 50 m, turned by over 0.6, or its view grew or shrank by over
		// a quarter. What was asked of the last view says nothing of this one, so there is no memory on this frame
		// or the next, whose count is the first of the new view laid with its own margins)
		const bool bLumpsCut = !(EyeMoved <= 50.0) || !(EyeTurned <= 0.6) || !(std::abs(Eye.PixelsPerRadian - LumpsZoom) <= 0.25 * LumpsZoom);
		const bool bLumpsWas = !bLumpsCut && LumpsSince >= 0.0 && LumpsSince < 1.0;
		// (the lumps' cost held, game.lumps_most: the builder's rested reading of build 1770, 15:09Z 20 November, had
		// Pearl Harbor at 55.8 frames a second against 72.7 and Midway at 59.0 against 67.8 once the ships burned,
		// every puff over lumps_px carrying its five. So no more than lumps_most puffs carry lumps at once, the
		// widest on the screen first: with more than that wide enough, the width a puff must have rises to that of
		// the first one left out (LumpsTight times its own lumps_px), and each comes in over the next quarter of
		// that as before, so the last few in are faint and none appears at a stroke. It follows the count by half
		// its size a second at most, both ways; after a cut it is this frame's own. A puff wider than
		// lumps_px_most.0 counts for less the wider it is (LumpsWorth), so the lumps go to puffs they can be read on)
		double TightAsked = 1.0;
		if (LumpsMostPuffs > 0 && Start == 0 && Need < Left)
		{
			LumpSizes.clear();
			for (const Order& O : Sorted)
			{
				const Bit& Piece = *O.B;
				if (Piece.Lumps <= 0 || !(Piece.LumpsPx > 0.0) || !(O.Far > 0.0))
				{
					continue;
				}
				const double Grown = Clamp01((Now - Piece.Born) / std::max(Piece.LifeOfLooks(), 1e-6));
				if (!(LumpsLeft(Piece.HoldOfLooks(), Grown) > 0.0) || Piece.InTail(Now - Piece.Born))
				{
					continue;
				}
				const double Worth = LumpsWorth(2.0 * (Piece.R0 + (Piece.R1 - Piece.R0) * std::pow(Grown, Piece.Grow)) * std::max(Eye.PixelsPerRadian, 1.0) / std::max(O.Far, 1e-3), LumpsBigA, LumpsBigB);
				if (Worth >= Piece.LumpsPx)
				{
					LumpSizes.push_back(Worth / Piece.LumpsPx);
				}
			}
			if (LumpSizes.size() > static_cast<size_t>(LumpsMostPuffs))
			{
				std::nth_element(LumpSizes.begin(), LumpSizes.begin() + LumpsMostPuffs, LumpSizes.end(), [](double L, double R) { return L > R; });
				TightAsked = std::min(1.0e6, std::max(1.0, LumpSizes[static_cast<size_t>(LumpsMostPuffs)]));
			}
		}
		if (LumpsMostPuffs <= 0)
		{
			LumpsTight = 1.0;
		}
		else if (!bLumpsWas || !(LumpsTight >= 1.0))
		{
			LumpsTight = TightAsked;
		}
		else
		{
			// (the pre-check on 7d83869, 16:14Z: no single lump popped, but at the cap all the lumps in the fade band
			// stepped TOGETHER about once a second, 6 to 13% of all their ink in one frame. What is asked drops a little
			// each time a wide puff ends its lumps' time; a drop under the clamp's reach passed whole in one frame, and
			// every puff in the band moved by four times it. So with game.lumps_soft_s it goes toward what is asked by
			// a share of the way left each frame, most of it closed in that many seconds and the same at any frame
			// rate: a small move of the count is a fade over many frames. Still by half its size a second at most;
			// a cut sets it at once, as before. The pre-check's readings with 0.35: the biggest frame change 2% from
			// 12%, no blinks on an approach from two. 0 or no key: the clamp alone, as 7d83869)
			const double By = std::exp(0.5 * LumpsSince);
			const double Toward = LumpsSoft > 0.0 ? LumpsTight * std::pow(TightAsked / LumpsTight, 1.0 - std::exp(-LumpsSince / LumpsSoft)) : TightAsked;
			LumpsTight = std::clamp(Toward, LumpsTight / By, LumpsTight * By);
		}
		size_t Lumped = 0;
		double Asked = static_cast<double>(Left); // (something left out for want of room: the room is full)
		if (Start == 0 && Need < Left)
		{
			for (const Order& O : Sorted)
			{
				Lumped += MayLump(O);
			}
			Asked = static_cast<double>(std::min(Left, Need + Lumped));
		}
		if (!bLumpsWas || Asked >= LumpsMost)
		{
			LumpsMost = Asked;
			LumpsHeld = 0.0;
		}
		else if (Asked >= LumpsMost - 0.1 * LumpsTenth)
		{
			LumpsHeld = 0.0;
		}
		else if ((LumpsHeld += LumpsSince) > 2.0)
		{
			LumpsMost = std::max(Asked, LumpsMost - LumpsSince * 0.1 * LumpsTenth);
		}
		LumpsAt = bLumpsCut ? -1.0e9 : Now;
		LumpsZoom = Eye.PixelsPerRadian;
		if (Lumped > 0 && Need + Lumped < Left)
		{
			LumpsRoom = Clamp01((static_cast<double>(Left) - LumpsMost) / LumpsTenth);
			if (bLumpsWas && LumpsShown > LumpsRoom)
			{
				LumpsRoom = std::min(std::max(LumpsRoom, LumpsShown - 0.5 * LumpsSince), Clamp01(3.0 * (static_cast<double>(Left) - Asked) / LumpsTenth));
			}
		}
		LumpsShown = LumpsRoom;
		for (size_t I = 0; I < Sorted.size() && Slot < MaxPuffs; ++I)
		{
			if (I >= Start || Sorted[I].bKeep)
			{
				Slot += LayBit(Slot, *Sorted[I].B, Now, Eye, MaxPuffs - Slot);
			}
		}
		// (FX9, game.smoke_budget: the smoke laid, in screens, and the share of the big smoke left out from the next frame
		// on: enough that what this frame asked would come within screens, never more than thin_most, followed over
		// thin_s seconds so the big smoke thins and comes back by degrees. A second or more since the last Draw, or
		// time gone back: this frame's own)
		SmokeLaidScreens = ScreenPx > 0.0 ? FillLaid / ScreenPx : 0.0;
		if (ThinScreens > 0.0)
		{
			const double Target = ScreenPx > 0.0 && FillBig > 0.0 ? std::clamp(1.0 - (ThinScreens * ScreenPx - FillSmall) / FillBig, 0.0, ThinMost) : 0.0;
			const double Since = Now - ThinAt;
			ThinShare = Since >= 0.0 && Since < 1.0 && ThinS > 0.0 ? ThinShare + (Target - ThinShare) * (1.0 - std::exp(-Since / ThinS)) : Target;
			ThinShare = ThinShare == ThinShare ? std::clamp(ThinShare, 0.0, ThinMost) : 0.0;
			ThinAt = Now;
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

	bool Battle::Smoky(const Bit& B) const
	{
		// (FX9: what the smoke's budget may leave out or thin. A light, a kept column or an oil fire's, a trail, a thrown
		// piece, a piece with a floor of its own on the screen, a far form and a funnel's even plume are each held to
		// their own law, and are left alone)
		return (B.Form == Bit::Shape::Puff || B.Form == Bit::Shape::Upright) && !B.bKeep && !B.bLight && !B.bThrown && !(B.LeastPx > 0.0) && !(B.Ribbon > 0.0) && !(B.Smear > 0.0) && B.FarRep == 0 && !(B.Hose > 0.0) && B.Even < 0 && B.OilId < 0 && !(B.WakeBy > 0.0);
	}

	double Battle::MostRad(const Bit& B, double Now) const
	{
		// (as burst.far's: its radius by its age, at the most; one that lingers its tail's end wider again; its long way,
		// a flame's swell, a two-stage grower's middle size. Never under what LayBit lays, so a piece skip_px would lay
		// is never left out for it)
		const double Grown = Clamp01((Now - B.Born) / std::max(B.Life, 1e-6));
		double Most = B.bTail ? std::max(B.R0, B.R1) * (1.0 + std::clamp(B.TailWide, 0.0, 4.0)) : std::max(B.R0, B.R0 + (B.R1 - B.R0) * std::pow(Grown, B.Grow));
		if (B.GrowBy > 0.0)
		{
			Most = std::max({ Most, B.RMid, B.R1 });
		}
		Most *= std::max({ 1.0, B.Stretch, B.StretchFrom, B.Flatten }) * (1.0 + Clamp01(B.FlickerWide)) * (1.0 + Clamp01(B.FlickerLong));
		if (B.LowBy > 0.0)
		{
			Most /= std::sqrt(B.LowFlat == B.LowFlat ? std::clamp(B.LowFlat, 0.1, 1.0) : 1.0); // (a low mound is wider than its radius: Bit::LowBy)
		}
		return Most == Most ? std::max(Most, B.HalfLength) : 1.0e9;
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
		const double Big = std::max(std::max(B.R0, B.R1), B.WakeBy > 0.0 ? (B.WakeOn > 0.0 ? WakeWide(B.R0, B.WakeR, B.WakeBy, B.WakeOn, std::max(B.Life, B.WakeBy)) : B.WakeR) : 0.0);
		// (one that lingers is by its tail's end TailWide wider again)
		double R = (B.bTail ? Big * (1.0 + std::clamp(B.TailWide, 0.0, 4.0)) : Big) * std::max(1.0, B.Stretch) * Ends * (1.0 + 1.2 * std::max(0.0, B.Rough));
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

	void Battle::SetLampRule(const LampRule& Rule)
	{
		// (the sea's night fight, 25 November: see LampRule in the header. Every knob held, one that is not a number its
		// default; a rule of all defaults hands the lamps out as ever, to the byte)
		const LampRule Usual;
		const auto Held = [](double Is, double Least, double Most, double Otherwise) { return Is == Is ? std::min(Most, std::max(Least, Is)) : Otherwise; };
		Rules.Most = std::clamp(Rule.Most, 0, MostLamps);
		Rules.bByEye = Rule.bByEye;
		Rules.MergeM = Held(Rule.MergeM, 0.0, 2000.0, Usual.MergeM);
		Rules.MergeS = Held(Rule.MergeS, 0.0, 2.0, Usual.MergeS);
		Rules.MergeMost = Held(Rule.MergeMost, 1.0, 16.0, Usual.MergeMost);
		Rules.Yield = Held(Rule.Yield, 1.0, 1.0e6, Usual.Yield);
		bLampRuled = Rules.Asks();
	}

	int Battle::LampsWanted() const
	{
		if (Rules.Most > 0)
		{
			return Rules.Most;
		}
		return static_cast<int>(SafeNum(Num("game.max_lamps", 2.0), 0.0, static_cast<double>(MostLamps), 2.0));
	}

	double Battle::FlashShare(const Flash& F, double Now) const
	{
		// (art director 21:16Z: never before its flash; full on the flash's frame, about half the next, then out. A naval
		// gun's (LampS, the sea's night fight): full on its frame, then down to nothing by LampS after its birth, never
		// under that half on the next frame, so a big gun's lamp burns as long as its fireball does)
		if (F.FirstFrame < 0 || Now < F.Born)
		{
			return -1.0;
		}
		const int Frames = FrameNo - F.FirstFrame;
		if (Frames < 0)
		{
			return -1.0;
		}
		if (F.LampS > 0.0 && Frames > 0)
		{
			const double Down = Clamp01(1.0 - (Now - F.Born) / F.LampS);
			const double Next = Frames == 1 && !F.bOneFrame ? 0.5 : 0.0;
			const double Share = std::max(Down, Next);
			return Share > 0.0 ? Share : -1.0; // (not a number: out)
		}
		if (Frames > 1)
		{
			return -1.0;
		}
		return Frames == 0 ? 1.0 : F.bOneFrame ? 0.0 : 0.5;
	}

	bool Battle::LampInSight(const Vec& At, double Reach) const
	{
		if (!bEyeKnown || !(SightHalf > 0.0))
		{
			return true;
		}
		const Vec To = At - LastEye;
		const double Far = To.Length();
		if (!(Far > Reach))
		{
			return true; // (the eye within its sphere, or not a number: in sight)
		}
		const double Cos = std::clamp(To.Dot(SightAhead) / Far, -1.0, 1.0);
		const double Wide = std::asin(std::clamp(Reach / Far, 0.0, 1.0));
		return std::acos(Cos) <= SightHalf + Wide;
	}

	bool Battle::LampFirst(const LampLit& A, const LampLit& B)
	{
		// (a strict order on every lamp the rule hands out: all of them are numbers by then, RealLamp)
		if (A.Rank != B.Rank)
		{
			return A.Rank > B.Rank;
		}
		if (A.L.Strength != B.L.Strength)
		{
			return A.L.Strength > B.L.Strength;
		}
		if (A.Born != B.Born)
		{
			return A.Born < B.Born;
		}
		if (A.L.At.X != B.L.At.X)
		{
			return A.L.At.X < B.L.At.X;
		}
		if (A.L.At.Y != B.L.At.Y)
		{
			return A.L.At.Y < B.L.At.Y;
		}
		if (A.L.At.Z != B.L.At.Z)
		{
			return A.L.At.Z < B.L.At.Z;
		}
		if (A.L.Radius != B.L.Radius)
		{
			return A.L.Radius < B.L.Radius;
		}
		if (A.L.Col.R != B.L.Col.R)
		{
			return A.L.Col.R < B.L.Col.R;
		}
		if (A.L.Col.G != B.L.Col.G)
		{
			return A.L.Col.G < B.L.Col.G;
		}
		if (A.L.Col.B != B.L.Col.B)
		{
			return A.L.Col.B < B.L.Col.B;
		}
		return A.Seed < B.Seed;
	}

	double Battle::EyeRank(const Vec& At, double Strength, double Reach) const
	{
		if (!Rules.bByEye)
		{
			return Strength;
		}
		if (!LampInSight(At, Reach))
		{
			return 0.0; // (its light cannot reach the screen: a broadside behind the eye lights nothing it sees)
		}
		// (the light a lamp sends the eye: what it lands round it over the square of its distance from the eye. Its reach
		// cancels: from afar its lit patch is as much dimmer a square metre as it is wider on the screen. 5 m at the least,
		// as fire.lamp's steady lights (SteadyLamps' RankOf), so the two rank on one scale; the eye not yet known, 1 m)
		const double Far = bEyeKnown ? (At - LastEye).Length() : 1.0;
		const double Far2 = Far * Far;
		return Strength / std::max(25.0, Far2 == Far2 ? Far2 : 1.0e18);
	}

	void Battle::MergeLamps(std::vector<LampLit>& Lits) const
	{
		// (a lamp that is not a number lights nothing, nor one whose gun is not a number: dropped, so no lamp the rule
		// hands out is ever one, and no distance between guns is)
		Lits.erase(std::remove_if(Lits.begin(), Lits.end(), [](const LampLit& Is) { return !RealLamp(Is.L) || !std::isfinite(Is.Born) || !std::isfinite(Is.Gun.X) || !std::isfinite(Is.Gun.Y) || !std::isfinite(Is.Gun.Z); }), Lits.end());
		if (!(Rules.MergeM > 0.0) || Lits.size() < 2)
		{
			return;
		}
		// (strongest first, each the seed of a lamp; a flash joins it when it was born within MergeS and its gun lies
		// within MergeM of every gun already in it, so no lamp's guns are more than MergeM apart nor MergeS apart in time,
		// and a lamp never chains from ship to ship. The guns', never the lamps': a naval lamp hangs far out along its bore
		// (lamp_out_m), so two ships 500 m apart firing toward each other bring their lamps within MergeM while their guns
		// stay 300 m apart and more (Flash::Gun; a flash without a gun is measured from its lamp). Its strength is their
		// sum held to MergeMost times its seed's (the strongest), at their lamps' strength-weighted middle, coloured as
		// their strength-weighted mix, reaching as far as the farthest of their own reaches from there)
		const size_t N = Lits.size();
		std::vector<size_t> Seeds_(N);
		for (size_t I = 0; I < N; ++I)
		{
			Seeds_[I] = I;
		}
		// (strongest first, a tie by the flash itself and never by the order it was laid in: LampFirst with every Rank
		// alike, so the groups and every sum over them are the same whatever the order of the Muzzle calls)
		std::sort(Seeds_.begin(), Seeds_.end(), [&Lits](size_t A, size_t B)
		{
			LampLit X = Lits[A], Y = Lits[B];
			X.Rank = Y.Rank = 0.0;
			return LampFirst(X, Y);
		});
		std::vector<uint8_t> Taken(N, 0);
		std::vector<size_t> In;
		std::vector<LampLit> Made;
		Made.reserve(N);
		const double Near2 = Rules.MergeM * Rules.MergeM;
		for (const size_t A : Seeds_)
		{
			if (Taken[A] != 0)
			{
				continue;
			}
			Taken[A] = 1;
			In.clear();
			In.push_back(A);
			for (const size_t B : Seeds_)
			{
				if (Taken[B] != 0)
				{
					continue;
				}
				bool bJoins = true;
				for (const size_t K : In)
				{
					const Vec D = Lits[B].Gun - Lits[K].Gun;
					if (D.Dot(D) > Near2 || std::fabs(Lits[B].Born - Lits[K].Born) > Rules.MergeS)
					{
						bJoins = false;
						break;
					}
				}
				if (bJoins)
				{
					Taken[B] = 1;
					In.push_back(B);
				}
			}
			LampLit Is = Lits[A];
			double Sum = 0.0;
			for (const size_t K : In)
			{
				Sum += Lits[K].L.Strength;
			}
			if (In.size() > 1 && Sum > 0.0)
			{
				Vec Mid;
				double R = 0.0, G = 0.0, Bl = 0.0, Born = Lits[A].Born;
				for (const size_t K : In)
				{
					const Lamp& L = Lits[K].L;
					const double W = L.Strength / Sum;
					Mid += L.At * W;
					R += L.Col.R * W;
					G += L.Col.G * W;
					Bl += L.Col.B * W;
					Born = std::min(Born, Lits[K].Born);
				}
				double Reach = 0.0;
				for (const size_t K : In)
				{
					Reach = std::max(Reach, (Lits[K].L.At - Mid).Length() + Lits[K].L.Radius);
				}
				Is.L.At = Mid;
				Is.L.Col = Rgb(static_cast<float>(R), static_cast<float>(G), static_cast<float>(Bl));
				Is.L.Strength = std::min(Sum, Rules.MergeMost * Lits[A].L.Strength);
				Is.L.Radius = Reach;
				Is.Born = Born;
			}
			Made.push_back(Is);
		}
		Lits.swap(Made);
	}

	int Battle::Lamps(double Now, Lamp* Into, int Room, double Day) const
	{
		// Art director 21:16Z: never before its flash; full on the flash's frame, about half the next, then out. By day
		// a lamp is a quarter as strong and warm white, and small guns light no ground at all.
		if (Has("fire.lamp.ramp_s"))
		{
			return SteadyLamps(Now, Into, Room, Day);
		}
		Day = Clamp01(Day);
		const double ByDay = 1.0 + (DayLampShare - 1.0) * Day;
		std::vector<Lamp> All;
		std::vector<LampLit> Fired; // (SetLampRule: the flashes kept apart, to be merged and ranked by the rule)
		for (const Flash& F : Flashes)
		{
			const double Share = FlashShare(F, Now);
			if (Share < 0.0)
			{
				continue;
			}
			const double Strength = F.Strength * Share * (F.bNightOnly ? 1.0 - Day : ByDay);
			if (Strength <= 1e-4)
			{
				continue;
			}
			Lamp L;
			L.At = F.At;
			L.Col = F.Col.Mix(DayLampCol, Day);
			L.Strength = Strength;
			L.Radius = F.Radius;
			if (bLampRuled)
			{
				LampLit Is;
				Is.L = L;
				Is.Born = F.Born;
				Is.Gun = F.bGun ? F.Gun : F.At;
				Fired.push_back(Is);
				continue;
			}
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
			if (S.Kind == 5 && S.Cook.JetLamp > 0.0 && S.JetEnd > S.JetAt && Now >= S.JetAt && Now <= S.JetEnd)
			{
				// (the hatch jet's glow on the hull and the ground by it, the art director 15:40Z: orange, by day too,
				// jet_lamp_day of it; up and down with the jet, flickering as a fire's does)
				const double JetFor = S.JetEnd - S.JetAt, X = (Now - S.JetAt) / JetFor;
				double Full = std::min(1.0, (Now - S.JetAt) / 0.25) * Clamp01((1.0 - X) / 0.25);
				if (S.JetDip > 0.0)
				{
					const double Dx = (X - S.JetDip) / 0.1;
					Full *= 1.0 - (1.0 - S.Cook.Dip) * std::exp(-Dx * Dx);
				}
				const double Seed = static_cast<double>(S.Seed % 1000ull);
				Lamp L;
				L.At = S.At + S.Way * (0.3 * S.JetTall);
				L.Col = Flame;
				L.Strength = (1.0 + (S.Cook.JetLampDay - 1.0) * Day) * S.Cook.JetLamp * std::sqrt(S.Across) * Full * (0.75 + 0.25 * std::sin(Now * 23.0 + Seed) * std::sin(Now * 7.3 + Seed * 0.5));
				L.Radius = std::max(6.0, 2.5 * S.JetTall);
				if (L.Strength > 1e-4)
				{
					All.push_back(L);
				}
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
			if (S.OutAt >= 0.0 && Now > S.OutAt)
			{
				// (a fire being put out lights the ground less as its flames go, and not at all with the last of them)
				L.Strength *= Clamp01(1.0 - (Now - S.OutAt) / std::max(S.OutOver, 1e-3));
			}
			L.Col = Flame.Mix(DayLampCol, Day);
			L.Radius = 8.0 * S.Across;
			All.push_back(L);
		}
		for (const BallLamp& Ball : BallLamps)
		{
			Lamp L;
			if (BallLampAt(Ball, Now, Day, L))
			{
				All.push_back(L);
			}
		}
		if (bLampRuled)
		{
			// (SetLampRule without fire.lamp's steady lights: a salvo's flashes one lamp, then every lamp by the rule's
			// rank, the first Room of them, never more than MostLamps)
			MergeLamps(Fired);
			for (const Lamp& L : All)
			{
				if (!RealLamp(L))
				{
					continue;
				}
				LampLit Is;
				Is.L = L;
				Is.bSteady = true;
				Fired.push_back(Is);
			}
			for (LampLit& Is : Fired)
			{
				Is.Rank = EyeRank(Is.L.At, Is.L.Strength, Is.L.Radius);
			}
			std::sort(Fired.begin(), Fired.end(), LampFirst);
			const int Most = std::min({ std::max(0, Room), MostLamps, static_cast<int>(Fired.size()) });
			for (int I = 0; I < Most; ++I)
			{
				Into[I] = Fired[static_cast<size_t>(I)].L;
			}
			return Most;
		}
		std::sort(All.begin(), All.end(), [](const Lamp& L, const Lamp& R) { return L.Strength > R.Strength; });
		const int N = std::min(Room, static_cast<int>(All.size()));
		for (int I = 0; I < N; ++I)
		{
			Into[I] = All[static_cast<size_t>(I)];
		}
		return N;
	}

	int Battle::SteadyLamps(double Now, Lamp* Into, int Room, double Day) const
	{
		// fire.lamp, the frame study of 22 November: a fire's light came on whole in one frame, and the battle's two
		// lamps went to whichever lights were strongest that frame, so they hopped between a wreck's fires sixteen times
		// a second and dropped out for frames at a time. Now: game.steady_lamps of them are kept for the lights that
		// burn (fires, burning oil, a hatch's jet), given to the ones that matter most to the eye (strength over the
		// distance squared) and kept by the ones lit last frame unless another is keep times as much; a flash takes
		// what is left, and a steady light fills it when no flash wants it. A fire's lamp comes up over ramp_s as it
		// catches and goes down over ramp_s after its last flame, and a lamp new to its place comes up over ramp_s
		// too; it flickers smoothly, flicker more or less, never in a beat
		Day = Clamp01(Day);
		Room = std::max(0, Room);
		if (bLampRuled)
		{
			Room = std::min(Room, MostLamps); // (SetLampRule: never more than MostLamps)
		}
		const double ByDay = 1.0 + (DayLampShare - 1.0) * Day;
		const std::string P = "fire.lamp.";
		const double Ramp = SafeNum(Num(P + "ramp_s", 0.6), 0.02, 30.0, 0.6);
		const double Power = SafeNum(Num(P + "strength", 0.25), 0.0, 100.0, 0.25);
		const double Reach = SafeNum(Num(P + "reach", 8.0), 0.1, 100.0, 8.0);
		const double ReachLeast = SafeNum(Num(P + "reach_least_m", 0.0), 0.0, 1000.0, 0.0);
		const double UpBy = SafeNum(Num(P + "up", 0.6), -10.0, 20.0, 0.6);
		const double Flick = SafeNum(Num(P + "flicker", 0.25), 0.0, 0.9, 0.25);
		const Span Hz = Range(P + "flicker_hz", 3.0, 6.0);
		const double Keep = SafeNum(Num(P + "keep", 1.5), 1.0, 100.0, 1.5);
		// (land_strength, land_reach, land_up: a fire on land's (LandFire), the tank's lamp of head C and the art director's
		// dusk deck mend of 22 November; strength, reach and up stay the ships'. No such key: the same for all)
		const bool bLandPower = Has(P + "land_strength"), bLandReach = Has(P + "land_reach"), bLandUp = Has(P + "land_up");
		const double PowerLand = bLandPower ? SafeNum(Num(P + "land_strength", 0.25), 0.0, 100.0, 0.25) : Power;
		const double ReachLand = bLandReach ? SafeNum(Num(P + "land_reach", 8.0), 0.1, 100.0, 8.0) : Reach;
		const double UpLand = bLandUp ? SafeNum(Num(P + "land_up", 0.6), -10.0, 20.0, 0.6) : UpBy;
		const int Steady = std::clamp(static_cast<int>(SafeNum(Num("game.steady_lamps", 1.0), 0.0, 64.0, 1.0)), 0, Room);
		const LateRow LateNow = LateRowOf();
		const double Since = Now == Now && Now >= LampsAt ? std::min(Now - LampsAt, 1.0) : 0.0;
		using Lit = LampLit;
		std::vector<Lit> Steadies, Flashed;
		for (const Flash& F : Flashes)
		{
			const double Share = FlashShare(F, Now);
			if (Share < 0.0)
			{
				continue;
			}
			const double Strength = F.Strength * Share * (F.bNightOnly ? 1.0 - Day : ByDay);
			if (Strength <= 1e-4)
			{
				continue;
			}
			Lit Is;
			Is.L.At = F.At;
			Is.L.Col = F.Col.Mix(DayLampCol, Day);
			Is.L.Strength = Strength;
			Is.L.Radius = F.Radius;
			Is.Rank = Strength;
			Is.Born = F.Born;
			Is.Gun = F.bGun ? F.Gun : F.At;
			Flashed.push_back(Is);
		}
		if (bLampRuled)
		{
			// (SetLampRule: a salvo's flashes one lamp, each ranked by the light it sends the eye, or its strength)
			MergeLamps(Flashed);
			for (Lit& Is : Flashed)
			{
				Is.Rank = EyeRank(Is.L.At, Is.L.Strength, Is.L.Radius);
			}
		}
		const bool bRealOnly = bLampRuled; // (SetLampRule: a steady light that is not a number is never handed out either)
		const Rgb Flame = Picture("fire.flame_colour", Rgb::FromPicture(207, 134, 56));
		auto RankOf = [&](const Lit& Is, double Steadily)
		{
			const double Far2 = bEyeKnown ? (Is.L.At - LastEye).Length() * (Is.L.At - LastEye).Length() : 1.0;
			const double Rank = Steadily / std::max(25.0, Far2 == Far2 ? Far2 : 1.0e18);
			for (const LampKept& K : LampsKept)
			{
				if (K.Seed == Is.Seed)
				{
					return Rank * Keep;
				}
			}
			return Rank;
		};
		for (const Source& S : Sources)
		{
			const uint32_t Seed32 = static_cast<uint32_t>((S.Seed * 0xD1B54A32D192ED03ull) >> 32) | 1u;
			Lit Is;
			Is.Seed = S.Seed;
			Is.bSteady = true;
			if (S.Kind == 2 && Now >= S.Start && Now <= S.Flames && S.Oil.Lamp > 0.0)
			{
				const double Steadily = ByDay * S.Oil.Lamp * std::sqrt(S.Across);
				Is.L.At = S.At + ZUp * (S.Oil.FlameUp + 4.0);
				Is.L.Strength = Steadily * (1.0 + 0.2 * Flickering(Seed32, 7u, Now, 1.5, 4.0));
				Is.L.Col = S.Oil.Flame.Mix(DayLampCol, Day);
				Is.L.Radius = S.Oil.LampReach * S.Across;
				Is.Rank = RankOf(Is, Steadily);
				Steadies.push_back(Is);
				continue;
			}
			if (S.Kind == 5 && S.Cook.JetLamp > 0.0 && S.JetEnd > S.JetAt && Now >= S.JetAt && Now <= S.JetEnd)
			{
				const double JetFor = S.JetEnd - S.JetAt, X = (Now - S.JetAt) / JetFor;
				double Full = std::min(1.0, (Now - S.JetAt) / 0.25) * Clamp01((1.0 - X) / 0.25);
				if (S.JetDip > 0.0)
				{
					const double Dx = (X - S.JetDip) / 0.1;
					Full *= 1.0 - (1.0 - S.Cook.Dip) * std::exp(-Dx * Dx);
				}
				const double Steadily = (1.0 + (S.Cook.JetLampDay - 1.0) * Day) * S.Cook.JetLamp * std::sqrt(S.Across) * Full;
				Is.L.At = S.At + S.Way * (0.3 * S.JetTall);
				Is.L.Col = Flame;
				Is.L.Strength = Steadily * (1.0 + Flick * Flickering(Seed32, 5u, Now, Hz.A, Hz.B));
				Is.L.Radius = std::max(6.0, 2.5 * S.JetTall);
				if (Is.L.Strength > 1e-4)
				{
					Is.Rank = RankOf(Is, Steadily);
					Steadies.push_back(Is);
				}
				continue;
			}
			if (S.Kind != 0 || Now < S.Start || Now > S.Flames + Ramp)
			{
				continue;
			}
			// (up over ramp_s as it catches, down over ramp_s after its last flame; less as it is put out)
			double On = Clamp01((Now - S.Start) / Ramp) * Clamp01((S.Flames + Ramp - Now) / Ramp);
			if (S.OutAt >= 0.0 && Now > S.OutAt)
			{
				On *= Clamp01(1.0 - (Now - S.OutAt) / std::max(S.OutOver, 1e-3));
			}
			const bool bOnLand = (bLandPower || bLandReach || bLandUp) && LandFire(S);
			const double Steadily = ByDay * (bOnLand ? PowerLand : Power) * std::sqrt(std::min(S.Across, 5000.0)) * On * (1.0 - (1.0 - LateNow.Light) * LateShare(LateNow, S.Seed, S.Start, Now));
			if (Steadily <= 1e-4)
			{
				continue;
			}
			Is.L.At = S.At + ZUp * (std::min(S.Across, 5000.0) * (bOnLand ? UpLand : UpBy));
			Is.L.Col = Flame.Mix(DayLampCol, Day);
			Is.L.Strength = Steadily * (1.0 + Flick * Flickering(Seed32, 3u, Now, Hz.A, Hz.B));
			Is.L.Radius = std::max(ReachLeast, (bOnLand ? ReachLand : Reach) * std::min(S.Across, 5000.0));
			Is.Rank = RankOf(Is, Steadily);
			Steadies.push_back(Is);
		}
		for (const BallLamp& Ball : BallLamps)
		{
			// (a fuel ball's, FuelBall: steady while it burns, as a fire's is)
			Lit Is;
			Is.Seed = Ball.Seed;
			Is.bSteady = true;
			if (BallLampAt(Ball, Now, Day, Is.L))
			{
				Is.Rank = RankOf(Is, Is.L.Strength);
				Steadies.push_back(Is);
			}
		}
		if (bRealOnly)
		{
			Steadies.erase(std::remove_if(Steadies.begin(), Steadies.end(), [](const Lit& Is) { return !RealLamp(Is.L) || !(Is.Rank == Is.Rank); }), Steadies.end());
			if (Rules.bByEye)
			{
				for (Lit& Is : Steadies)
				{
					Is.Rank = LampInSight(Is.L.At, Is.L.Radius) ? Is.Rank : 0.0; // (as a flash's, EyeRank: a fire whose light cannot reach the screen ranks nothing)
				}
			}
			// (SetLampRule: ties broken by the lamps themselves, never by the order they were laid in)
			std::sort(Steadies.begin(), Steadies.end(), LampFirst);
			std::sort(Flashed.begin(), Flashed.end(), LampFirst);
		}
		else
		{
			auto ByRank = [](const Lit& L, const Lit& R) { return L.Rank > R.Rank; };
			std::stable_sort(Steadies.begin(), Steadies.end(), ByRank);
			std::stable_sort(Flashed.begin(), Flashed.end(), ByRank);
		}
		std::vector<Lit> Chosen;
		size_t NextSteady = 0, NextFlash = 0;
		if (bLampRuled && Rules.bByEye)
		{
			// (SetLampRule's bByEye: the steady lights keep their own share; the rest go by rank, flash or steady alike, so
			// a far flash takes no lamp from a fire by the eye; then a steady light's own lamp goes, for the frames it is
			// outshone, to a flash left unlit that sends the eye Yield times its light: a slick burning 6 km off lights
			// nothing the eye can see while a broadside 2 km off goes dark. It comes back up over ramp_s, as a new one)
			const int Most = Room;
			const int Own = std::min(Steady, Most);
			while (static_cast<int>(Chosen.size()) < Own && NextSteady < Steadies.size())
			{
				Chosen.push_back(Steadies[NextSteady++]);
			}
			const size_t Kept = Chosen.size();
			while (static_cast<int>(Chosen.size()) < Most && (NextFlash < Flashed.size() || NextSteady < Steadies.size()))
			{
				const bool bFlash = NextSteady >= Steadies.size() || (NextFlash < Flashed.size() && Flashed[NextFlash].Rank >= Steadies[NextSteady].Rank);
				if (bFlash)
				{
					Chosen.push_back(Flashed[NextFlash++]);
				}
				else
				{
					Chosen.push_back(Steadies[NextSteady++]);
				}
			}
			for (size_t I = Kept; I-- > 0 && NextFlash < Flashed.size();)
			{
				if (Chosen[I].bSteady && Flashed[NextFlash].Rank > Rules.Yield * Chosen[I].Rank)
				{
					Chosen[I] = Flashed[NextFlash++];
				}
			}
		}
		else
		{
			while (static_cast<int>(Chosen.size()) < Steady && NextSteady < Steadies.size())
			{
				Chosen.push_back(Steadies[NextSteady++]);
			}
			while (static_cast<int>(Chosen.size()) < Room && NextFlash < Flashed.size())
			{
				Chosen.push_back(Flashed[NextFlash++]);
			}
			while (static_cast<int>(Chosen.size()) < Room && NextSteady < Steadies.size())
			{
				Chosen.push_back(Steadies[NextSteady++]);
			}
		}
		// (a steady lamp new to its place comes up over ramp_s; one that held its place keeps coming up)
		std::vector<LampKept> Now_;
		for (Lit& Is : Chosen)
		{
			if (Is.Seed == 0)
			{
				continue;
			}
			double Level = Since > 0.0 ? Clamp01(Since / Ramp) : 0.0;
			for (const LampKept& K : LampsKept)
			{
				if (K.Seed == Is.Seed)
				{
					Level = Clamp01(K.Level + Since / Ramp);
					break;
				}
			}
			Is.L.Strength *= Level;
			LampKept K;
			K.Seed = Is.Seed;
			K.Level = Level;
			Now_.push_back(K);
		}
		if (Since > 0.0 || LampsKept.empty())
		{
			LampsKept = Now_;
			LampsAt = Now;
		}
		int N = 0;
		for (const Lit& Is : Chosen)
		{
			if (Is.L.Strength > 1e-6 && N < Room)
			{
				Into[N++] = Is.L;
			}
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
